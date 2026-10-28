// agentscope_local_turn.h — AgentScope 本地文本轮次的宿主侧实现
// ==============================================================================
// 设计依据：agentscope-cpp docs/migration/xiaozhi-adapter-s2-text-slice-design-20261028.md
//           §2/§3/§4/§5/§6/§8；规格：openspec specs/xiaozhi-local-turn/spec.md
//
// 职责边界（宿主侧）：
//   1) 提供适配器 S2 需要的三个注入回调：轮次执行（runner）、结果投递（delivery）、
//      检查点落盘（checkpoint）；适配器本身不持有模型/凭证/存储介质。
//   2) runner 由 worker 线程调用，内部构造并复用设备 Agent（ReActAgent + Toolkit +
//      NVS 检查点），模型网络等待与可阻塞工具因此只发生在 worker 内。
//   3) delivery 只投递短文本/错误码，并经 Application::Schedule 回到主任务，
//      绝不在回调内等待网络/模型。
//   4) 云端 stt 显示路径不进入本模块（只做显示），因此不会触发并行本地 ReAct。
//
// 凭证来源：NVS 命名空间 "agentscope" 的键 "api_key"（不在固件里烘焙密钥）。
// 缺失时 runner 显式报错并回传错误码，绝不假成功。
// ==============================================================================
#ifndef _AGENTSCOPE_LOCAL_TURN_H_
#define _AGENTSCOPE_LOCAL_TURN_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "sdkconfig.h"

#if CONFIG_XIAOZHI_AGENTSCOPE_ADAPTER

#include "agentscope/core/util/result.h"

class XiaozhiAdapter;

namespace agentscope_local_turn {

/// 把本地轮次三件套注入适配器（主任务调用，Initialize 末尾）。
/// 适配器未创建时本函数不做事，宿主其余路径照常。
void Install(XiaozhiAdapter& adapter);

/// 轮次执行回调（由适配器 worker 线程调用）。
/// 构造/复用设备 Agent 并跑完一轮，返回本轮文本；不可用时返回错误码。
agentscope::Result<std::string, agentscope::AgentError> RunLocalTurn(void* context,
                                                                       std::string_view text,
                                                                       uint64_t token);

/// 结果投递回调（由适配器 worker 线程调用）：经 Application::Schedule 投递短结果。
void DeliverLocalTurn(void* context, uint64_t token, const char* reply, size_t reply_bytes,
                      agentscope::AgentError status);

/// 检查点落盘回调（由适配器 worker 线程调用）：本切片由 Agent 自身检查点负责落盘，
/// 此处只做可观测记录，不直接操作存储介质。
void CheckpointLocalTurn(void* context, uint64_t token);

}  // namespace agentscope_local_turn

#endif  // CONFIG_XIAOZHI_AGENTSCOPE_ADAPTER

#endif  // _AGENTSCOPE_LOCAL_TURN_H_
