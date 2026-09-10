/*
 * as_mcp_bridge.h -- Bridge between agentscope-core tools and xiaozhi McpServer.
 *
 * Maps to agentscope-java: Tool injection into MCP server.
 *
 * This module registers all tools from the core as_tool registry into
 * xiaozhi-esp32's existing McpServer, so the cloud LLM can discover
 * and invoke them through the standard MCP protocol.
 *
 * Design (D-003): Bridge, don't replace. xiaozhi's McpServer is preserved.
 * Design (D-006): C++ file (as_mcp_bridge.cc) for calling xiaozhi C++ APIs.
 *
 * When compiled outside xiaozhi (no mcp_server.h), all functions are no-ops.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the MCP bridge subsystem.
 * Call after as_tool_registry_init().
 */
void as_mcp_bridge_init(void);

/**
 * Deinitialize the MCP bridge.
 */
void as_mcp_bridge_deinit(void);

/**
 * Register all core tools with xiaozhi's McpServer.
 *
 * Iterates the as_tool registry and for each tool:
 *   1. Converts as_tool_param_t → xiaozhi PropertyList
 *   2. Creates a bridge callback (as_tool_fn_t → McpServer ReturnValue)
 *   3. Calls McpServer::AddTool()
 *
 * Pre-hooks and post-hooks are automatically applied:
 *   - AS_HOOK_PRE_EXEC before tool execution
 *   - AS_HOOK_POST_EXEC after tool execution
 *   - AS_HOOK_ON_TELEMETRY on result
 *
 * No-op if xiaozhi mcp_server.h is not available.
 */
void as_mcp_bridge_register_all(void);

/**
 * Check if the bridge is connected to xiaozhi McpServer.
 * @return 1 if connected, 0 if standalone mode
 */
int as_mcp_bridge_is_connected(void);

#ifdef __cplusplus
}
#endif
