/*
 * as_formatter.h -- LLM API request/response formatter.
 *
 * Maps to agentscope-java:
 *   - AbstractBaseFormatter        → as_format_request / as_parse_response
 *   - OpenAIFormatter              → AS_PROVIDER_OPENAI
 *   - DashScopeFormatter           → AS_PROVIDER_DASHSCOPE
 *   - AnthropicFormatter            → AS_PROVIDER_ANTHROPIC
 *   - GeminiFormatter              → AS_PROVIDER_GEMINI
 *   - OllamaFormatter              → AS_PROVIDER_OLLAMA
 *
 * Lightweight formatters for direct LLM API calls on ESP32.
 * Each provider has different request/response schemas; this module
 * normalises them behind a single interface.
 */
#pragma once

#include <cJSON.h>
#include <stdbool.h>
#include <stddef.h>
#include "as_tool.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Supported LLM providers. */
typedef enum {
    AS_PROVIDER_OPENAI,
    AS_PROVIDER_DASHSCOPE,
    AS_PROVIDER_ANTHROPIC,
    AS_PROVIDER_GEMINI,
    AS_PROVIDER_OLLAMA,
} as_provider_t;

/**
 * Chat request configuration.
 *
 * All string fields point to compile-time .rodata (no ownership).
 * tools_schema is optional (NULL if function calling is not needed).
 */
typedef struct {
    as_provider_t provider;
    const char *model_name;
    const char *system_prompt;
    cJSON *tools_schema;    /**< Optional: tools for function calling */
    bool stream;
    float temperature;
    int max_tokens;
    float top_p;
} as_chat_request_t;

/**
 * Format a chat request for a specific provider.
 *
 * @param config   Request configuration.
 * @param messages Message array in OpenAI format: [{"role":"...","content":"..."}].
 * @return Provider-specific JSON request body. Caller must cJSON_Delete().
 *         Returns NULL on error.
 */
cJSON* as_format_request(const as_chat_request_t *config, const cJSON *messages);

/**
 * Parse a provider API response into a normalised message object.
 *
 * Returns {"role":"assistant","content":"...","tool_calls":[...]} or NULL on error.
 * Caller must cJSON_Delete().
 */
cJSON* as_parse_response(as_provider_t provider, const char *raw_response);

/**
 * Parse an SSE streaming chunk.
 *
 * @param provider  Provider to parse for.
 * @param chunk     Raw SSE line (e.g. "data: {...}").
 * @param is_done   Set to true when stream is finished ("data: [DONE]").
 * @return Partial message content, or NULL if no content in this chunk.
 *         Caller must cJSON_Delete().
 */
cJSON* as_parse_stream_chunk(as_provider_t provider, const char *chunk, bool *is_done);

/**
 * Convert as_tool_entry_t array to provider-specific tool schema.
 *
 * @param provider Target provider format.
 * @param tools    Tool entries to convert.
 * @param count    Number of tools.
 * @return JSON array of tool definitions. Caller must cJSON_Delete().
 */
cJSON* as_format_tools(as_provider_t provider, const as_tool_entry_t *tools, size_t count);

#ifdef __cplusplus
}
#endif
