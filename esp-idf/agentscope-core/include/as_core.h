/*
 * as_core.h -- One-include header for the IoT agent framework.
 *
 * Usage:
 *   #include "as_core.h"
 */
#pragma once

#ifdef CONFIG_AGENTSCOPE_CORE_ENABLE

#include "as_tool.h"
#include "as_tool_param.h"
#include "as_hook.h"
#include "as_rule.h"
#include "as_card.h"
#include "as_telemetry.h"
#include "as_event.h"
#include "as_plan.h"
#include "as_skill.h"

#ifdef CONFIG_AGENTSCOPE_CORE_FORMATTER
#include "as_formatter.h"
#endif

#ifdef CONFIG_AGENTSCOPE_CORE_CREDENTIAL
#include "as_credential.h"
#endif

#ifdef CONFIG_AGENTSCOPE_CORE_RAG
#include "as_rag.h"
#endif

#ifdef CONFIG_AGENTSCOPE_CORE_MCP_BRIDGE
#include "as_mcp_bridge.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize all enabled AgentScope Core subsystems. */
void as_core_init(void);

/** Deinitialize all AgentScope Core subsystems. */
void as_core_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_AGENTSCOPE_CORE_ENABLE */
