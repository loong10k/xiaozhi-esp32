/*
 * multi_cloud_llm_router.cc — MultiCloudLlmRouter 的非叶子实现
 *
 * ModelRegistry（agentscope/core/model/model_registry.h）会拖入完整
 * 模型栈，违反 Protocol 叶子规则对"可传递头文件依赖"的要求；故唯一
 * 依赖它的 resolve_best() 实现放在本文件 —— .cc 的 include 不向依赖
 * 方传递，叶子规则（check_protocol_leaves.sh）只约束头文件。
 */
#include "multi_cloud_llm_router.h"

#include "agentscope/core/model/model_registry.h"

namespace xiaozhi {

std::shared_ptr<agentscope::core::model::Model> MultiCloudLlmRouter::resolve_best(
    const agentscope::core::model::ModelCreationContext& ctx) {
    const auto& ep = router_.select();
    current_index_ = index_of(ep.model_id);
    auto model =
        agentscope::core::model::ModelRegistry::resolve(ep.model_id, ctx);
    return model;
}

}  // namespace xiaozhi
