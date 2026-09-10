/*
 * as_rag.h -- RAG (Retrieval-Augmented Generation) abstractions.
 *
 * Maps to agentscope-java:
 *   - Document              → as_document_t
 *   - DocumentMetadata      → as_doc_metadata_t
 *   - GenericRAGHook        → as_rag_hook_t
 *   - Knowledge             → as_knowledge_t
 *   - RAGMode               → as_rag_mode_t
 *
 * Core provides lightweight interfaces; Extensions provide vector store
 * implementation (requires PSRAM).
 */
#pragma once

#include <cJSON.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** RAG mode — how retrieval is triggered. */
typedef enum {
    AS_RAG_MODE_DISABLED,  /**< No retrieval */
    AS_RAG_MODE_AUTO,      /**< Automatically retrieve before each reasoning */
    AS_RAG_MODE_MANUAL,    /**< Agent explicitly calls retrieve tool */
} as_rag_mode_t;

/** Document metadata. */
typedef struct {
    const char *source;       /**< e.g., "manual.pdf" */
    const char *title;
    const char *author;
    int64_t created_at;       /**< Unix timestamp (seconds) */
} as_doc_metadata_t;

/** Document — content plus metadata and relevance score. */
typedef struct {
    const char *content;      /**< Document text content */
    as_doc_metadata_t metadata;
    float score;              /**< Relevance score from retrieval (0.0-1.0) */
} as_document_t;

/**
 * Knowledge base interface.
 *
 * Implementations provide retrieve/add_document/clear.
 * Core RAG delegates to registered knowledge instances.
 */
typedef struct {
    const char *name;
    as_rag_mode_t mode;
    /**
     * Retrieve top_k documents matching query.
     * Caller must free() the returned array.
     * @param query   Search query text
     * @param top_k   Maximum results
     * @param count   [out] Actual number of results returned
     * @return Array of as_document_t, or NULL on error/empty
     */
    as_document_t* (*retrieve)(const char *query, int top_k, int *count);
    /**
     * Add a document to the knowledge base.
     * Implementation makes a deep copy.
     */
    void (*add_document)(const as_document_t *doc);
    /** Remove all documents. */
    void (*clear)(void);
} as_knowledge_t;

/** RAG hook — injects retrieved context into the system prompt. */
typedef struct {
    as_knowledge_t *knowledge;  /**< Backing knowledge base */
    int top_k;                  /**< Number of documents to retrieve */
    const char *prefix;         /**< Prefix for injected context */
} as_rag_hook_t;

/* === Lifecycle === */

/** Initialize the RAG subsystem. Clears all registered knowledge bases. */
void as_rag_init(void);

/** Deinitialize the RAG subsystem. */
void as_rag_deinit(void);

/* === Registration === */

/**
 * Register a knowledge base.
 * @return 0 on success, -1 if table is full
 */
int as_rag_register_knowledge(as_knowledge_t *knowledge);

/**
 * Get the number of registered knowledge bases.
 */
int as_rag_knowledge_count(void);

/**
 * Get a registered knowledge base by index.
 * @return Pointer to knowledge, or NULL if out of bounds.
 */
as_knowledge_t* as_rag_knowledge_get(int index);

/* === Retrieval === */

/**
 * Retrieve documents from the first enabled knowledge base.
 * Caller must free() the returned array.
 *
 * @param query  Search query
 * @param top_k  Maximum results
 * @param count  [out] Actual result count
 * @return Array of as_document_t, or NULL
 */
as_document_t* as_rag_retrieve(const char *query, int top_k, int *count);

/**
 * Build a retrieval context string for injection into the system prompt.
 * Queries all registered knowledge bases and concatenates results.
 * Caller must free() the returned string.
 *
 * @param query      Search query
 * @param max_chars  Maximum characters in the returned context
 * @return Allocated string with formatted context, or NULL
 */
char* as_rag_build_context(const char *query, int max_chars);

#ifdef __cplusplus
}
#endif
