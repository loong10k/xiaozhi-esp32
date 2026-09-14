/*
 * multi_cloud_llm_router.h — xiaozhi 端多云 LLM 路由薄封装
 *
 * 目的：把 agentscope-cpp 的 ModelRouter（核心路由 + 熔断 + 健康统计，
 *       已由 tests/test_model_router.cc 8/8 单测覆盖）桥接到 xiaozhi
 *       的模型调用链：select → ModelRegistry::resolve → 记录结果。
 *
 * 分层说明（Protocol 叶子规则）：
 *   - 头文件仅 include 叶子集（model_router.h）。
 *   - ModelRegistry / Model / ModelCreationContext 属非叶子（拖入完整
 *     模型栈），头文件只做前置声明；唯一的调用点 resolve_best() 移入
 *     multi_cloud_llm_router.cc —— .cc 的 include 是实现细节，不向
 *     依赖方传递，不违反叶子规则。
 *
 * 默认 endpoint 集（见 2026-09-10-multi-cloud-llm-integration.md §4.1）：
 *   0. dashscope:qwen-plus            （首选）
 *   1. dashscope:glm-4-flash          （阿里云 GLM 备选）
 *   2. deepseek:deepseek-v4-flash     （DeepSeek 官方）
 *   3. glm:glm-4-flash                （智谱 BigModel）
 */
#ifndef AGENT_HOSTING_MULTI_CLOUD_LLM_ROUTER_H_
#define AGENT_HOSTING_MULTI_CLOUD_LLM_ROUTER_H_

#include <memory>
#include <string>
#include <vector>

#include "agentscope/core/model/model_router.h"

namespace agentscope {
namespace core {
namespace model {
class Model;                  // 完整定义：model_registry.h（非叶子，仅 .cc 可见）
struct ModelCreationContext;  // 完整定义：model_creation_context.h
}  // namespace model
}  // namespace core
}  // namespace agentscope

namespace xiaozhi {

class MultiCloudLlmRouter {
public:
    struct Defaults {
        // 默认候选（按冷启动优先级排序）
        static std::vector<agentscope::core::model::RouteEndpoint>
        make_endpoints() {
            using agentscope::core::model::RouteEndpoint;
            return {
                {"dashscope:qwen-plus", 0, true},
                {"dashscope:glm-4-flash", 1, true},
                {"deepseek:deepseek-v4-flash", 2, true},
                {"glm:glm-4-flash", 3, true},
            };
        }
    };

    explicit MultiCloudLlmRouter(
        std::vector<agentscope::core::model::RouteEndpoint> endpoints =
            Defaults::make_endpoints())
        : router_(std::move(endpoints)) {}

    // 路由到当前最优模型并从 ModelRegistry 解析实例。
    // 失败返回 nullptr（凭证缺失 / 不可解析）。
    // 实现在 multi_cloud_llm_router.cc（需 ModelRegistry 完整定义）。
    std::shared_ptr<agentscope::core::model::Model> resolve_best(
        const agentscope::core::model::ModelCreationContext& ctx);

    // 调用成功/失败回报（透传给核心 router）
    void report_success(double latency_ms) {
        if (current_index_ >= 0) router_.report_success(current_index_, latency_ms);
    }
    void report_failure() {
        if (current_index_ >= 0) router_.report_failure(current_index_);
    }

    // 当前选中的 model_id（监控/日志用；resolve_best 后有效）
    std::string current_model_id() const {
        return router_.select().model_id;
    }

    agentscope::core::model::ModelRouter& core() { return router_; }

private:
    int index_of(const std::string& model_id) const {
        for (size_t i = 0; i < router_.endpoint_count(); ++i) {
            if (router_.endpoint(i).model_id == model_id) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    agentscope::core::model::ModelRouter router_;
    int current_index_ = -1;
};

}  // namespace xiaozhi

#endif  // AGENT_HOSTING_MULTI_CLOUD_LLM_ROUTER_H_
