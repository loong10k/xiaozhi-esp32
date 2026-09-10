/*
 * test_hook.c -- Unit tests for as_hook system.
 */
#include "as_hook.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

static int s_pre_called = 0;
static int s_post_called = 0;

static as_hook_response_t counting_pre_hook(const char *tool, const cJSON *data, void *ud) {
    (void)tool; (void)data; (void)ud;
    s_pre_called++;
    as_hook_response_t r = { .result = AS_HOOK_CONTINUE, .modified_data = NULL };
    return r;
}

static as_hook_response_t counting_post_hook(const char *tool, const cJSON *data, void *ud) {
    (void)tool; (void)data; (void)ud;
    s_post_called++;
    as_hook_response_t r = { .result = AS_HOOK_CONTINUE, .modified_data = NULL };
    return r;
}

static as_hook_response_t skip_hook(const char *tool, const cJSON *data, void *ud) {
    (void)tool; (void)data; (void)ud;
    as_hook_response_t r = { .result = AS_HOOK_SKIP, .modified_data = NULL };
    return r;
}

static void test_registration(void) {
    printf("  test_registration... ");

    as_hook_init();
    assert(as_hook_register(AS_HOOK_PRE_EXEC, 100, counting_pre_hook, NULL) == 0);
    assert(as_hook_register(AS_HOOK_POST_EXEC, 100, counting_post_hook, NULL) == 0);

    /* Duplicate fn should succeed (different slot) */
    assert(as_hook_register(AS_HOOK_PRE_EXEC, 200, counting_pre_hook, NULL) == 0);

    printf("PASS\n");
}

static void test_chain_execution(void) {
    printf("  test_chain_execution... ");

    as_hook_init();
    s_pre_called = 0;
    s_post_called = 0;

    as_hook_register(AS_HOOK_PRE_EXEC, 100, counting_pre_hook, NULL);
    as_hook_register(AS_HOOK_POST_EXEC, 100, counting_post_hook, NULL);

    as_hook_response_t r;

    r = as_hook_execute(AS_HOOK_PRE_EXEC, "test_tool", NULL);
    assert(r.result == AS_HOOK_CONTINUE);
    assert(s_pre_called == 1);

    r = as_hook_execute(AS_HOOK_POST_EXEC, "test_tool", NULL);
    assert(r.result == AS_HOOK_CONTINUE);
    assert(s_post_called == 1);

    /* No hooks for ON_TELEMETRY */
    r = as_hook_execute(AS_HOOK_ON_TELEMETRY, "test_tool", NULL);
    assert(r.result == AS_HOOK_CONTINUE);

    printf("PASS\n");
}

static void test_skip(void) {
    printf("  test_skip... ");

    as_hook_init();
    as_hook_register(AS_HOOK_PRE_EXEC, 50, skip_hook, NULL);

    as_hook_response_t r = as_hook_execute(AS_HOOK_PRE_EXEC, "blocked_tool", NULL);
    assert(r.result == AS_HOOK_SKIP);

    printf("PASS\n");
}

static void test_unregistration(void) {
    printf("  test_unregistration... ");

    as_hook_init();
    s_pre_called = 0;

    as_hook_register(AS_HOOK_PRE_EXEC, 100, counting_pre_hook, NULL);
    as_hook_execute(AS_HOOK_PRE_EXEC, "t", NULL);
    assert(s_pre_called == 1);

    assert(as_hook_unregister(AS_HOOK_PRE_EXEC, counting_pre_hook) == 0);
    s_pre_called = 0;
    as_hook_execute(AS_HOOK_PRE_EXEC, "t", NULL);
    assert(s_pre_called == 0);

    /* Unregister non-existent */
    assert(as_hook_unregister(AS_HOOK_PRE_EXEC, counting_pre_hook) == -1);

    printf("PASS\n");
}

static void test_clear(void) {
    printf("  test_clear... ");

    as_hook_init();
    s_pre_called = 0;

    as_hook_register(AS_HOOK_PRE_EXEC, 100, counting_pre_hook, NULL);
    as_hook_register(AS_HOOK_POST_EXEC, 100, counting_post_hook, NULL);

    as_hook_clear(AS_HOOK_PRE_EXEC);
    as_hook_execute(AS_HOOK_PRE_EXEC, "t", NULL);
    assert(s_pre_called == 0);

    /* POST_EXEC still works */
    s_post_called = 0;
    as_hook_execute(AS_HOOK_POST_EXEC, "t", NULL);
    assert(s_post_called == 1);

    /* Clear all */
    as_hook_clear_all();
    s_post_called = 0;
    as_hook_execute(AS_HOOK_POST_EXEC, "t", NULL);
    assert(s_post_called == 0);

    printf("PASS\n");
}

int main(void) {
    printf("=== test_hook ===\n");
    test_registration();
    test_chain_execution();
    test_skip();
    test_unregistration();
    test_clear();
    printf("All tests passed!\n");
    return 0;
}
