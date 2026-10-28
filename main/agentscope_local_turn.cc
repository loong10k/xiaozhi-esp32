// agentscope_local_turn.cc — AgentScope 本地文本轮次的宿主侧实现
// ==============================================================================
// 设计依据：agentscope-cpp docs/migration/xiaozhi-adapter-s2-text-slice-design-20261028.md
//           §2/§3/§4/§5/§6/§8；规格：openspec specs/xiaozhi-local-turn/spec.md
//
// 线程与所有权：
//   - Install/RequestLocalTurn 在主任务调用；RunLocalTurn/DeliverLocalTurn/
//     CheckpointLocalTurn 由适配器 worker 调用。
//   - Agent 与其 Toolkit/检查点为进程级固定存储，跨轮次复用（会话连续），
//     运行期不反复构造；首次构造在 worker 内完成（属允许的阻塞点）。
//   - 凭证缺失/构造失败一律显式错误码回传，不假成功、不静默降级。
// ==============================================================================
#include "agentscope_local_turn.h"

#if CONFIG_XIAOZHI_AGENTSCOPE_ADAPTER

#include "agentscope/core/agent/react_agent.h"
#include "agentscope/core/model/dashscope_chat_model.h"
#include "agentscope/core/state/device_checkpoint.h"
#include "agentscope/core/state/nvs_checkpoint_sink.h"
#include "agentscope/core/tool/tool_gate.h"
#include "agentscope/core/tool/toolkit.h"
#include "xiaozhi_adapter.h"

#include "application.h"
#include "board.h"
#include "display.h"
#include "esp_log.h"
#include "nvs_flash.h"

namespace agentscope_local_turn {
namespace {

constexpr const char* TAG = "agentscope_turn";
constexpr const char* kNvsNamespace = "agentscope";
constexpr const char* kNvsApiKey = "api_key";
constexpr const char* kDefaultModel = "qwen-max";
constexpr const char* kSessionKey = "local";
constexpr size_t kMaxApiKeyBytes = 256;

/// 进程级固定存储：Agent 与其依赖跨轮次复用，避免每轮重建带来的堆抖动。
struct LocalTurnStorage {
    agentscope::NvsCheckpointSink sink{kNvsNamespace};
    agentscope::DeviceCheckpoint checkpoint{&sink};
    agentscope::ToolGate gate;
    agentscope::Toolkit toolkit;
    std::unique_ptr<agentscope::ReActAgent> agent;
    bool agent_ready = false;
};

LocalTurnStorage g_storage;

/// 从 NVS 读取模型凭证；缺失或超长时返回空串（调用方据此显式报错）。
std::string load_api_key() {
    nvs_handle_t handle = 0;
    if (nvs_open(kNvsNamespace, NVS_READONLY, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "nvs namespace '%s' unavailable", kNvsNamespace);
        return {};
    }
    size_t length = 0;
    esp_err_t status = nvs_get_str(handle, kNvsApiKey, nullptr, &length);
    if (status != ESP_OK || length == 0 || length > kMaxApiKeyBytes) {
        ESP_LOGE(TAG, "api key missing or too long in nvs '%s' (len=%u)", kNvsNamespace,
                 static_cast<unsigned>(length));
        nvs_close(handle);
        return {};
    }
    std::string key(length, '\0');
    status = nvs_get_str(handle, kNvsApiKey, key.data(), &length);
    nvs_close(handle);
    if (status != ESP_OK) {
        ESP_LOGE(TAG, "api key read failed: %s", esp_err_to_name(status));
        return {};
    }
    key.resize(length > 0 ? length - 1 : 0);  // 去掉结尾 NUL
    return key;
}

/// 首次本地轮次时构造设备 Agent：模型 + 空工具集 + NVS 检查点。
/// 构造失败（凭证缺失/模型不可用/内存不足）一律显式错误，不假成功。
agentscope::AgentError ensure_agent() {
    if (g_storage.agent_ready) {
        return agentscope::AgentError::None;
    }
    const std::string api_key = load_api_key();
    if (api_key.empty()) {
        ESP_LOGE(TAG, "local turn unavailable: no model credential configured");
        return agentscope::AgentError::InvalidArgument;
    }
    auto model = agentscope::core::model::DashScopeChatModel::Builder()
                     .apiKey(api_key)
                     .modelName(kDefaultModel)
                     .build();
    if (!model) {
        ESP_LOGE(TAG, "local turn unavailable: model construction failed");
        return agentscope::AgentError::Unsupported;
    }
    // 生产缺门拒绝档：无显式放行规则时工具一律拒绝；本切片工具集为空。
    g_storage.toolkit.requireToolGate();
    g_storage.toolkit.setToolGate(&g_storage.gate);
    agentscope::ReActAgent::Builder builder("xiaozhi-local");
    builder.toolkit(&g_storage.toolkit)
        .checkpoint(&g_storage.checkpoint, kSessionKey)
        .maxIterations(3)
        .model(std::move(model));
    auto built = builder.buildChecked();
    if (built.is_err()) {
        ESP_LOGE(TAG, "local turn unavailable: agent build failed: %d",
                 static_cast<int>(built.unwrap_err()));
        return built.unwrap_err();
    }
    g_storage.agent = std::move(built).unwrap();
    auto started = g_storage.agent->startSession();
    if (started.is_err()) {
        ESP_LOGE(TAG, "local turn unavailable: session start failed: %d",
                 static_cast<int>(started.error()));
        g_storage.agent.reset();
        return started.error();
    }
    g_storage.agent_ready = true;
    ESP_LOGI(TAG, "local agent ready (model=%s session=%s)", kDefaultModel, kSessionKey);
    return agentscope::AgentError::None;
}

}  // namespace

void Install(XiaozhiAdapter& adapter) {
    adapter.ConfigureLocalTurn(RunLocalTurn, DeliverLocalTurn, CheckpointLocalTurn, nullptr);
    ESP_LOGI(TAG, "local turn callbacks installed");
}

agentscope::Result<std::string, agentscope::AgentError> RunLocalTurn(void* context,
                                                                       std::string_view text,
                                                                       uint64_t token) {
    using Reply = agentscope::Result<std::string, agentscope::AgentError>;
    (void)context;
    if (text.empty()) {
        return Reply::err(agentscope::AgentError::InvalidArgument);
    }
    const agentscope::AgentError ready = ensure_agent();
    if (ready != agentscope::AgentError::None) {
        return Reply::err(ready);
    }
    std::vector<agentscope::Msg> history{agentscope::Msg::user_text(std::string(text))};
    auto result = g_storage.agent->call(history);
    if (result.is_err()) {
        ESP_LOGE(TAG, "local turn %llu failed: %d", static_cast<unsigned long long>(token),
                 static_cast<int>(result.unwrap_err()));
        return Reply::err(result.unwrap_err());
    }
    std::string reply = result.value().getTextContent();
    ESP_LOGI(TAG, "local turn %llu produced %u bytes", static_cast<unsigned long long>(token),
             static_cast<unsigned>(reply.size()));
    return Reply::ok(std::move(reply));
}

void DeliverLocalTurn(void* context, uint64_t token, const char* reply, size_t reply_bytes,
                      agentscope::AgentError status) {
    (void)context;
    std::string text(reply != nullptr ? reply : "", reply != nullptr ? reply_bytes : 0);
    const int error_code = static_cast<int>(status);
    // 投递只做短串拷贝 + Schedule；不在回调内等待网络/模型/工具。
    Application::GetInstance().Schedule([text, token, error_code]() {
        if (error_code != static_cast<int>(agentscope::AgentError::None)) {
            ESP_LOGE(TAG, "local turn %llu error %d", static_cast<unsigned long long>(token),
                     error_code);
            return;
        }
        ESP_LOGI(TAG, "local turn %llu reply: %s", static_cast<unsigned long long>(token),
                 text.c_str());
        if (!text.empty()) {
            // 显示经宿主既有路径（与云端回复同一显示通道），不在此处迁移设备状态。
            Board::GetInstance().GetDisplay()->SetChatMessage("assistant", text.c_str());
        }
    });
}

void CheckpointLocalTurn(void* context, uint64_t token) {
    (void)context;
    // Agent 自身 checkpoint（DeviceCheckpoint + NVS）已在轮次结束时落盘；
    // 这里只登记可观测计数，不直接操作存储介质。
    ESP_LOGI(TAG, "local turn %llu checkpointed", static_cast<unsigned long long>(token));
}

}  // namespace agentscope_local_turn

#endif  // CONFIG_XIAOZHI_AGENTSCOPE_ADAPTER
