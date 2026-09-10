/*
 * as_credential.h -- LLM provider credential management.
 *
 * Maps to agentscope-java:
 *   - CredentialBase                → as_credential_t
 *   - OpenAICredential / DashScopeCredential / AnthropicCredential
 *     → as_credential_init() with appropriate as_provider_t
 *
 * Handles API keys, base URLs, and model configuration for each
 * provider.  Supports env-var bootstrapping and ESP32 NVS persistence.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "as_formatter.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Credential for a single LLM provider.
 *
 * Fixed-size buffers suitable for ESP32 SRAM (no heap allocation).
 */
typedef struct {
    as_provider_t provider;
    char api_key[128];
    char base_url[256];
    char model_name[64];
    bool stream;
    float temperature;
    int max_tokens;
} as_credential_t;

/**
 * Initialise credential with defaults, then overlay env vars.
 *
 * Looks up env vars in this order:
 *   OPENAI_API_KEY / OPENAI_BASE_URL / OPENAI_MODEL
 *   DASHSCOPE_API_KEY / DASHSCOPE_BASE_URL / DASHSCOPE_MODEL
 *   ANTHROPIC_API_KEY / ANTHROPIC_BASE_URL / ANTHROPIC_MODEL
 *   GEMINI_API_KEY / GEMINI_BASE_URL / GEMINI_MODEL
 *   OLLAMA_BASE_URL / OLLAMA_MODEL
 *
 * @param cred     Credential to initialise.
 * @param provider Target provider.
 * @return 0 on success, -1 on error (e.g. missing required key).
 */
int as_credential_init(as_credential_t *cred, as_provider_t provider);

/**
 * Load credential from ESP32 NVS flash.
 *
 * @param cred      Credential to populate.
 * @param namespace NVS namespace (e.g. "as_creds").
 * @return 0 on success, -1 on error.
 */
int as_credential_load_from_nvs(as_credential_t *cred, const char *ns);

/**
 * Save credential to ESP32 NVS flash.
 *
 * @param cred      Credential to persist.
 * @param namespace NVS namespace.
 * @return 0 on success, -1 on error.
 */
int as_credential_save_to_nvs(const as_credential_t *cred, const char *ns);

/**
 * Mask API key for safe logging.
 *
 * Shows first 4 and last 4 characters, replaces middle with "***".
 * E.g. "sk-abcdef123456" -> "sk-a***3456"
 *
 * @param key      Raw API key.
 * @param out      Output buffer.
 * @param out_len  Size of output buffer.
 */
void as_credential_mask_key(const char *key, char *out, size_t out_len);

/**
 * Get default base URL for a provider.
 *
 * @param provider Target provider.
 * @return Static string with the default URL. Never returns NULL.
 */
const char* as_credential_default_base_url(as_provider_t provider);

#ifdef __cplusplus
}
#endif
