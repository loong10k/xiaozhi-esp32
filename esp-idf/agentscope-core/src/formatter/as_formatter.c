/*
 * as_formatter.c -- Provider-specific LLM API formatting.
 *
 * Maps to agentscope-java: AbstractBaseFormatter + OpenAI/DashScope/Anthropic formatters.
 */
#include "as_formatter.h"
#include <string.h>
#include <esp_log.h>

static const char *TAG = "as_fmt";

/* --- OpenAI Format --- */
static cJSON* format_openai(const as_chat_request_t *cfg, const cJSON *messages) {
    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "model", cfg->model_name);
    if (cfg->stream) cJSON_AddTrueToObject(req, "stream");

    if (cfg->temperature > 0) cJSON_AddNumberToObject(req, "temperature", cfg->temperature);
    if (cfg->max_tokens > 0) cJSON_AddNumberToObject(req, "max_tokens", cfg->max_tokens);
    if (cfg->top_p > 0 && cfg->top_p < 1.0) cJSON_AddNumberToObject(req, "top_p", cfg->top_p);

    /* Messages */
    cJSON *msgs = cJSON_AddArrayToObject(req, "messages");
    if (cfg->system_prompt) {
        cJSON *sys = cJSON_CreateObject();
        cJSON_AddStringToObject(sys, "role", "system");
        cJSON_AddStringToObject(sys, "content", cfg->system_prompt);
        cJSON_AddItemToArray(msgs, sys);
    }
    if (messages) {
        cJSON *msg = NULL;
        cJSON_ArrayForEach(msg, messages) {
            cJSON_AddItemToArray(msgs, cJSON_Duplicate(msg, 1));
        }
    }

    /* Tools */
    if (cfg->tools_schema) {
        cJSON_AddItemToObject(req, "tools", cJSON_Duplicate(cfg->tools_schema, 1));
    }

    return req;
}

/* --- DashScope Format --- */
static cJSON* format_dashscope(const as_chat_request_t *cfg, const cJSON *messages) {
    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "model", cfg->model_name);

    cJSON *input = cJSON_AddObjectToObject(req, "input");
    cJSON *msgs = cJSON_AddArrayToObject(input, "messages");
    if (cfg->system_prompt) {
        cJSON *sys = cJSON_CreateObject();
        cJSON_AddStringToObject(sys, "role", "system");
        cJSON_AddStringToObject(sys, "content", cfg->system_prompt);
        cJSON_AddItemToArray(msgs, sys);
    }
    if (messages) {
        cJSON *msg = NULL;
        cJSON_ArrayForEach(msg, messages) {
            cJSON_AddItemToArray(msgs, cJSON_Duplicate(msg, 1));
        }
    }

    cJSON *params = cJSON_AddObjectToObject(req, "parameters");
    cJSON_AddStringToObject(params, "result_format", "message");
    if (cfg->temperature > 0) cJSON_AddNumberToObject(params, "temperature", cfg->temperature);
    if (cfg->max_tokens > 0) cJSON_AddNumberToObject(params, "max_tokens", cfg->max_tokens);

    if (cfg->tools_schema) {
        cJSON_AddItemToObject(req, "tools", cJSON_Duplicate(cfg->tools_schema, 1));
    }

    return req;
}

/* --- Anthropic Format --- */
static cJSON* format_anthropic(const as_chat_request_t *cfg, const cJSON *messages) {
    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "model", cfg->model_name);
    if (cfg->stream) cJSON_AddTrueToObject(req, "stream");
    if (cfg->max_tokens > 0) cJSON_AddNumberToObject(req, "max_tokens", cfg->max_tokens);
    if (cfg->system_prompt) cJSON_AddStringToObject(req, "system", cfg->system_prompt);

    cJSON *msgs = cJSON_AddArrayToObject(req, "messages");
    if (messages) {
        cJSON *msg = NULL;
        cJSON_ArrayForEach(msg, messages) {
            cJSON *m = cJSON_CreateObject();
            cJSON *role = cJSON_GetObjectItem(msg, "role");
            cJSON_AddStringToObject(m, "role", cJSON_IsString(role) ? role->valuestring : "user");
            cJSON *content = cJSON_GetObjectItem(msg, "content");
            if (cJSON_IsString(content)) {
                cJSON_AddStringToObject(m, "content", content->valuestring);
            } else if (content) {
                cJSON_AddItemToObject(m, "content", cJSON_Duplicate(content, 1));
            }
            cJSON_AddItemToArray(msgs, m);
        }
    }

    if (cfg->tools_schema) {
        cJSON_AddItemToObject(req, "tools", cJSON_Duplicate(cfg->tools_schema, 1));
    }

    return req;
}

/* --- Ollama Format --- */
static cJSON* format_ollama(const as_chat_request_t *cfg, const cJSON *messages) {
    cJSON *req = cJSON_CreateObject();
    cJSON_AddStringToObject(req, "model", cfg->model_name);
    if (cfg->stream) cJSON_AddTrueToObject(req, "stream");

    cJSON *msgs = cJSON_AddArrayToObject(req, "messages");
    if (cfg->system_prompt) {
        cJSON *sys = cJSON_CreateObject();
        cJSON_AddStringToObject(sys, "role", "system");
        cJSON_AddStringToObject(sys, "content", cfg->system_prompt);
        cJSON_AddItemToArray(msgs, sys);
    }
    if (messages) {
        cJSON *msg = NULL;
        cJSON_ArrayForEach(msg, messages) {
            cJSON_AddItemToArray(msgs, cJSON_Duplicate(msg, 1));
        }
    }

    if (cfg->tools_schema) {
        cJSON_AddItemToObject(req, "tools", cJSON_Duplicate(cfg->tools_schema, 1));
    }

    return req;
}

cJSON* as_format_request(const as_chat_request_t *config, const cJSON *messages) {
    if (!config || !config->model_name) return NULL;

    switch (config->provider) {
        case AS_PROVIDER_OPENAI:
        case AS_PROVIDER_GEMINI:
            return format_openai(config, messages);
        case AS_PROVIDER_DASHSCOPE:
            return format_dashscope(config, messages);
        case AS_PROVIDER_ANTHROPIC:
            return format_anthropic(config, messages);
        case AS_PROVIDER_OLLAMA:
            return format_ollama(config, messages);
        default:
            ESP_LOGE(TAG, "Unknown provider %d", config->provider);
            return NULL;
    }
}

cJSON* as_parse_response(as_provider_t provider, const char *raw_response) {
    if (!raw_response) return NULL;

    cJSON *resp = cJSON_Parse(raw_response);
    if (!resp) return NULL;

    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "role", "assistant");

    switch (provider) {
        case AS_PROVIDER_OPENAI:
        case AS_PROVIDER_GEMINI:
        case AS_PROVIDER_DASHSCOPE:
        case AS_PROVIDER_OLLAMA: {
            /* OpenAI/DashScope format: choices[0].message.content */
            cJSON *choices = cJSON_GetObjectItem(resp, "choices");
            if (choices && cJSON_GetArraySize(choices) > 0) {
                cJSON *choice = cJSON_GetArrayItem(choices, 0);
                cJSON *message = cJSON_GetObjectItem(choice, "message");
                if (message) {
                    cJSON *content = cJSON_GetObjectItem(message, "content");
                    if (cJSON_IsString(content)) {
                        cJSON_AddStringToObject(msg, "content", content->valuestring);
                    }
                    cJSON *tool_calls = cJSON_GetObjectItem(message, "tool_calls");
                    if (tool_calls) {
                        cJSON_AddItemToObject(msg, "tool_calls", cJSON_Duplicate(tool_calls, 1));
                    }
                }
            }
            /* DashScope: output.choices[0].message.content */
            cJSON *output = cJSON_GetObjectItem(resp, "output");
            if (output) {
                choices = cJSON_GetObjectItem(output, "choices");
                if (choices && cJSON_GetArraySize(choices) > 0) {
                    cJSON *choice = cJSON_GetArrayItem(choices, 0);
                    cJSON *message = cJSON_GetObjectItem(choice, "message");
                    if (message) {
                        cJSON *content = cJSON_GetObjectItem(message, "content");
                        if (cJSON_IsString(content)) {
                            cJSON_AddStringToObject(msg, "content", content->valuestring);
                        }
                    }
                }
            }
            break;
        }
        case AS_PROVIDER_ANTHROPIC: {
            /* Anthropic: content[0].text */
            cJSON *content_arr = cJSON_GetObjectItem(resp, "content");
            if (content_arr && cJSON_GetArraySize(content_arr) > 0) {
                cJSON *block = cJSON_GetArrayItem(content_arr, 0);
                cJSON *text = cJSON_GetObjectItem(block, "text");
                if (cJSON_IsString(text)) {
                    cJSON_AddStringToObject(msg, "content", text->valuestring);
                }
            }
            break;
        }
    }

    cJSON_Delete(resp);
    return msg;
}

cJSON* as_parse_stream_chunk(as_provider_t provider, const char *chunk, bool *is_done) {
    if (!chunk || !is_done) return NULL;
    *is_done = false;

    /* SSE format: "data: {...}\n\n" or "data: [DONE]\n\n" */
    const char *data = strstr(chunk, "data: ");
    if (!data) return NULL;
    data += 6;

    if (strncmp(data, "[DONE]", 6) == 0) {
        *is_done = true;
        return NULL;
    }

    cJSON *parsed = cJSON_Parse(data);
    if (!parsed) return NULL;

    /* Extract delta content */
    cJSON *delta_text = NULL;

    switch (provider) {
        case AS_PROVIDER_OPENAI:
        case AS_PROVIDER_DASHSCOPE:
        case AS_PROVIDER_OLLAMA:
        case AS_PROVIDER_GEMINI: {
            cJSON *choices = cJSON_GetObjectItem(parsed, "choices");
            if (choices && cJSON_GetArraySize(choices) > 0) {
                cJSON *choice = cJSON_GetArrayItem(choices, 0);
                cJSON *delta = cJSON_GetObjectItem(choice, "delta");
                if (delta) {
                    cJSON *content = cJSON_GetObjectItem(delta, "content");
                    if (cJSON_IsString(content)) {
                        delta_text = cJSON_CreateObject();
                        cJSON_AddStringToObject(delta_text, "content", content->valuestring);
                    }
                }
                cJSON *finish = cJSON_GetObjectItem(choice, "finish_reason");
                if (cJSON_IsString(finish) && strcmp(finish->valuestring, "stop") == 0) {
                    *is_done = true;
                }
            }
            break;
        }
        case AS_PROVIDER_ANTHROPIC: {
            cJSON *type = cJSON_GetObjectItem(parsed, "type");
            if (cJSON_IsString(type)) {
                if (strcmp(type->valuestring, "content_block_delta") == 0) {
                    cJSON *delta = cJSON_GetObjectItem(parsed, "delta");
                    if (delta) {
                        cJSON *text = cJSON_GetObjectItem(delta, "text");
                        if (cJSON_IsString(text)) {
                            delta_text = cJSON_CreateObject();
                            cJSON_AddStringToObject(delta_text, "content", text->valuestring);
                        }
                    }
                } else if (strcmp(type->valuestring, "message_stop") == 0) {
                    *is_done = true;
                }
            }
            break;
        }
    }

    cJSON_Delete(parsed);
    return delta_text;
}

cJSON* as_format_tools(as_provider_t provider, const as_tool_entry_t *tools, size_t count) {
    if (!tools || count == 0) return NULL;

    cJSON *array = cJSON_CreateArray();
    if (!array) return NULL;

    for (size_t i = 0; i < count; i++) {
        if (!(tools[i].visibility & AS_TOOL_VISIBILITY_AI)) continue;

        cJSON *tool = cJSON_CreateObject();

        if (provider == AS_PROVIDER_ANTHROPIC) {
            /* Anthropic format: { "name", "description", "input_schema" } */
            cJSON_AddStringToObject(tool, "name", tools[i].name);
            cJSON_AddStringToObject(tool, "description", tools[i].description);
            cJSON *schema = cJSON_CreateObject();
            cJSON_AddStringToObject(schema, "type", "object");
            if (tools[i].params && tools[i].param_count > 0) {
                cJSON *props = as_tool_params_to_json(tools[i].params, tools[i].param_count);
                if (props) cJSON_AddItemToObject(schema, "properties", props);
                cJSON *req = as_tool_params_required_array(tools[i].params, tools[i].param_count);
                if (req) cJSON_AddItemToObject(schema, "required", req);
            } else {
                cJSON_AddItemToObject(schema, "properties", cJSON_CreateObject());
            }
            cJSON_AddItemToObject(tool, "input_schema", schema);
        } else {
            /* OpenAI/DashScope/Ollama format: { "type": "function", "function": {...} } */
            cJSON_AddStringToObject(tool, "type", "function");
            cJSON *fn = cJSON_AddObjectToObject(tool, "function");
            cJSON_AddStringToObject(fn, "name", tools[i].name);
            cJSON_AddStringToObject(fn, "description", tools[i].description);
            cJSON *params = cJSON_CreateObject();
            cJSON_AddStringToObject(params, "type", "object");
            if (tools[i].params && tools[i].param_count > 0) {
                cJSON *props = as_tool_params_to_json(tools[i].params, tools[i].param_count);
                if (props) cJSON_AddItemToObject(params, "properties", props);
                cJSON *req = as_tool_params_required_array(tools[i].params, tools[i].param_count);
                if (req) cJSON_AddItemToObject(params, "required", req);
            } else {
                cJSON_AddItemToObject(params, "properties", cJSON_CreateObject());
            }
            cJSON_AddItemToObject(fn, "parameters", params);
        }

        cJSON_AddItemToArray(array, tool);
    }

    return array;
}
