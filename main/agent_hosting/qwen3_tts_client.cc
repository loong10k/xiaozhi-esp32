/*
 * qwen3_tts_client.cc — DashScope Qwen3-TTS Realtime API 客户端实现
 *
 * 协议流程（详见 2026-09-10-tts-tech-decision.md §5.2）：
 *   Client → {"type":"session.create","model":...,"voice":...}
 *   Client → {"type":"input_text_delta","delta":"你好"}
 *   Client → {"type":"input_text_done"}
 *   Server → {"type":"session.created"}
 *   Server → {"type":"response.audio.delta"} + binary opus 帧
 *   Server → {"type":"response.done"}
 */
#include "qwen3_tts_client.h"

#include <cstring>

#include <esp_log.h>

#include "board.h"

#define TAG "Qwen3Tts"

namespace xiaozhi {

Qwen3TtsClient::Qwen3TtsClient(Config cfg) : cfg_(std::move(cfg)) {}

Qwen3TtsClient::~Qwen3TtsClient() { Close(); }

bool Qwen3TtsClient::Connect() {
    if (ws_ && ws_->IsConnected()) {
        return true;
    }

    auto* network = Board::GetInstance().GetNetwork();
    if (network == nullptr) {
        ESP_LOGE(TAG, "Network not ready");
        return false;
    }

    ws_ = network->CreateWebSocket(cfg_.connect_id);
    if (ws_ == nullptr) {
        ESP_LOGE(TAG, "CreateWebSocket failed");
        return false;
    }

    if (!cfg_.api_key.empty()) {
        ws_->SetHeader("Authorization",
                       ("Bearer " + cfg_.api_key).c_str());
    }

    ws_->OnData([this](const char* data, size_t len, bool binary) {
        if (binary) {
            HandleBinaryFrame(data, len);
        } else {
            HandleTextFrame(data, len);
        }
    });

    if (!ws_->Connect(cfg_.uri.c_str())) {
        ESP_LOGE(TAG, "WebSocket connect failed: %s", cfg_.uri.c_str());
        ws_.reset();
        return false;
    }

    // session.create：声明模型与音色
    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "type", "session.create");
    cJSON_AddStringToObject(req, "model", cfg_.model.c_str());
    cJSON_AddStringToObject(req, "voice", cfg_.voice.c_str());
    bool ok = SendJson(req);
    cJSON_Delete(req);

    if (!ok) {
        ESP_LOGE(TAG, "session.create send failed");
        Close();
        return false;
    }

    finished_ = true;
    ESP_LOGI(TAG, "Connected, model=%s voice=%s",
             cfg_.model.c_str(), cfg_.voice.c_str());
    return true;
}

bool Qwen3TtsClient::IsConnected() const {
    return ws_ != nullptr && ws_->IsConnected() && session_created_;
}

void Qwen3TtsClient::OnOpusFrame(OpusFrameCallback cb) {
    on_opus_frame_ = std::move(cb);
}

bool Qwen3TtsClient::SendTextDelta(const std::string& text) {
    if (!IsConnected()) {
        return false;
    }
    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "type", "input_text_delta");
    cJSON_AddStringToObject(req, "delta", text.c_str());
    bool ok = SendJson(req);
    cJSON_Delete(req);
    if (ok) {
        finished_ = false;
    }
    return ok;
}

bool Qwen3TtsClient::Finish() {
    if (!IsConnected() || finished_) {
        return true;
    }
    cJSON* req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "type", "input_text_done");
    bool ok = SendJson(req);
    cJSON_Delete(req);
    finished_ = ok;
    return ok;
}

void Qwen3TtsClient::Close() {
    if (ws_) {
        ws_.reset();
    }
    session_created_ = false;
    finished_ = true;
}

void Qwen3TtsClient::Poll() {
    // 当前 esp-ml307 WebSocket 通过回调推送数据，无需显式 poll。
    // 预留：后续若切换到轮询式传输，在此驱动接收。
}

bool Qwen3TtsClient::SendJson(const cJSON* json) {
    if (ws_ == nullptr) {
        return false;
    }
    char* printed = cJSON_PrintUnformatted(json);
    if (printed == nullptr) {
        return false;
    }
    bool ok = ws_->Send(std::string(printed));
    cJSON_free(printed);
    if (!ok) {
        ESP_LOGE(TAG, "Send failed");
    }
    return ok;
}

void Qwen3TtsClient::HandleTextFrame(const char* data, size_t len) {
    cJSON* root = cJSON_ParseWithLength(data, len);
    if (root == nullptr) {
        return;
    }
    const cJSON* type = cJSON_GetObjectItem(root, "type");
    const char* type_str = cJSON_IsString(type) ? type->valuestring : "";

    if (strcmp(type_str, "session.created") == 0) {
        session_created_ = true;
        ESP_LOGI(TAG, "Session created");
    } else if (strcmp(type_str, "response.done") == 0) {
        // 一段音频结束（与 input_text_done 对应）
        ESP_LOGD(TAG, "Response done");
    } else if (strcmp(type_str, "error") == 0) {
        const cJSON* msg = cJSON_GetObjectItem(root, "message");
        ESP_LOGE(TAG, "Server error: %s",
                 cJSON_IsString(msg) ? msg->valuestring : "(unknown)");
    }
    // 其余事件（response.audio.delta 等）走二进制帧，这里忽略

    cJSON_Delete(root);
}

void Qwen3TtsClient::HandleBinaryFrame(const char* data, size_t len) {
    if (on_opus_frame_ && len > 0) {
        on_opus_frame_(reinterpret_cast<const uint8_t*>(data), len);
    }
}

}  // namespace xiaozhi
