/*
 * as_credential.c -- API key management for LLM providers.
 *
 * Maps to agentscope-java: CredentialBase + provider credentials.
 */
#include "as_credential.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <esp_log.h>

static const char *TAG = "as_cred";

static const char* default_base_url(as_provider_t provider) {
    switch (provider) {
        case AS_PROVIDER_OPENAI:     return "https://api.openai.com/v1";
        case AS_PROVIDER_DASHSCOPE:  return "https://dashscope.aliyuncs.com/compatible-mode/v1";
        case AS_PROVIDER_ANTHROPIC:  return "https://api.anthropic.com";
        case AS_PROVIDER_GEMINI:     return "https://generativelanguage.googleapis.com/v1beta";
        case AS_PROVIDER_OLLAMA:     return "http://localhost:11434";
        default:                     return "";
    }
}

static const char* default_model(as_provider_t provider) {
    switch (provider) {
        case AS_PROVIDER_OPENAI:     return "gpt-4o-mini";
        case AS_PROVIDER_DASHSCOPE:  return "qwen-plus";
        case AS_PROVIDER_ANTHROPIC:  return "claude-sonnet-4-20250514";
        case AS_PROVIDER_GEMINI:     return "gemini-2.0-flash";
        case AS_PROVIDER_OLLAMA:     return "llama3";
        default:                     return "unknown";
    }
}

static const char* env_var_name(as_provider_t provider) {
    switch (provider) {
        case AS_PROVIDER_OPENAI:     return "OPENAI_API_KEY";
        case AS_PROVIDER_DASHSCOPE:  return "DASHSCOPE_API_KEY";
        case AS_PROVIDER_ANTHROPIC:  return "ANTHROPIC_API_KEY";
        case AS_PROVIDER_GEMINI:     return "GEMINI_API_KEY";
        case AS_PROVIDER_OLLAMA:     return "";
        default:                     return "";
    }
}

int as_credential_init(as_credential_t *cred, as_provider_t provider) {
    if (!cred) return -1;
    memset(cred, 0, sizeof(as_credential_t));
    cred->provider = provider;
    cred->stream = true;
    cred->temperature = 0.7f;
    cred->max_tokens = 2048;

    /* Load base URL */
    const char *url = default_base_url(provider);
    if (url) strncpy(cred->base_url, url, sizeof(cred->base_url) - 1);

    /* Load model */
    const char *model = default_model(provider);
    if (model) strncpy(cred->model_name, model, sizeof(cred->model_name) - 1);

    /* Load API key from environment */
    const char *env = env_var_name(provider);
    if (env && env[0]) {
        const char *key = getenv(env);
        if (key && key[0]) {
            strncpy(cred->api_key, key, sizeof(cred->api_key) - 1);
            ESP_LOGI(TAG, "Loaded %s for provider %d (key=%s...)", env, provider,
                     strlen(key) > 8 ? "****" : "***");
        } else {
            ESP_LOGW(TAG, "Environment variable %s not set", env);
        }
    }

    return 0;
}

int as_credential_load_from_nvs(as_credential_t *cred, const char *namespace) {
    if (!cred || !namespace) return -1;
    /* TODO: nvs_open + nvs_get_str for api_key, base_url, model_name */
    ESP_LOGI(TAG, "NVS load (namespace=%s) - not yet implemented", namespace);
    return -1;
}

int as_credential_save_to_nvs(const as_credential_t *cred, const char *namespace) {
    if (!cred || !namespace) return -1;
    /* TODO: nvs_open + nvs_set_str + nvs_commit */
    ESP_LOGI(TAG, "NVS save (namespace=%s) - not yet implemented", namespace);
    return -1;
}

void as_credential_mask_key(const char *key, char *out, size_t out_len) {
    if (!key || !out || out_len < 10) {
        if (out && out_len > 0) out[0] = '\0';
        return;
    }
    size_t len = strlen(key);
    if (len < 10) {
        snprintf(out, out_len, "****");
        return;
    }
    snprintf(out, out_len, "%.4s...%.4s", key, key + len - 4);
}

const char* as_credential_default_base_url(as_provider_t provider) {
    return default_base_url(provider);
}
