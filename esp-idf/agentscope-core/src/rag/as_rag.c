/*
 * as_rag.c -- RAG abstractions (Core, lightweight, no PSRAM).
 *
 * Maps to agentscope-java: GenericRAGHook + Knowledge + RAGMode.
 */
#include "as_rag.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <esp_log.h>

static const char *TAG = "as_rag";

#define MAX_KNOWLEDGE_BASES 4

static as_knowledge_t *s_bases[MAX_KNOWLEDGE_BASES];
static size_t s_base_count = 0;
static bool s_initialized = false;

void as_rag_init(void) {
    memset(s_bases, 0, sizeof(s_bases));
    s_base_count = 0;
    s_initialized = true;
    ESP_LOGI(TAG, "RAG subsystem initialized");
}

int as_rag_register_knowledge(as_knowledge_t *knowledge) {
    if (!s_initialized || !knowledge || s_base_count >= MAX_KNOWLEDGE_BASES) return -1;
    s_bases[s_base_count++] = knowledge;
    ESP_LOGI(TAG, "Registered knowledge base '%s'", knowledge->name ? knowledge->name : "?");
    return 0;
}

as_document_t* as_rag_retrieve(const char *query, int top_k, int *count) {
    if (!s_initialized || !query || !count) {
        if (count) *count = 0;
        return NULL;
    }
    /* Delegate to first active knowledge base */
    for (size_t i = 0; i < s_base_count; i++) {
        if (s_bases[i] && s_bases[i]->retrieve) {
            return s_bases[i]->retrieve(query, top_k, count);
        }
    }
    *count = 0;
    return NULL;
}

char* as_rag_build_context(const char *query, int max_chars) {
    if (!s_initialized || !query) return NULL;

    int count = 0;
    as_document_t *docs = as_rag_retrieve(query, 5, &count);
    if (!docs || count == 0) return NULL;

    /* Build context string */
    size_t buf_size = (size_t)max_chars + 1;
    char *buf = calloc(1, buf_size);
    if (!buf) {
        free(docs);
        return NULL;
    }

    int offset = 0;
    for (int i = 0; i < count && offset < max_chars - 1; i++) {
        int written = snprintf(buf + offset, buf_size - offset,
                               "[%s] %s\n\n",
                               docs[i].metadata.source ? docs[i].metadata.source : "unknown",
                               docs[i].content ? docs[i].content : "");
        if (written > 0) offset += written;
        /* Free the content we allocated */
        free((char *)docs[i].content);
        free((char *)docs[i].metadata.source);
        free((char *)docs[i].metadata.title);
        free((char *)docs[i].metadata.author);
    }
    free(docs);

    if (offset == 0) {
        free(buf);
        return NULL;
    }
    return buf;
}
