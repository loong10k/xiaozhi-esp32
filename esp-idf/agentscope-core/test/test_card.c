/*
 * test_card.c -- Unit tests for as_card AgentCard.
 */
#include "as_card.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

static void test_init(void) {
    printf("  test_init... ");

    as_agent_card_t card;
    as_card_init(&card, "ESP32-001", "Living Room Sensor",
                 "Temperature and humidity monitor", "1.0.0", "esp32-s3");

    assert(strcmp(card.device_id, "ESP32-001") == 0);
    assert(strcmp(card.name, "Living Room Sensor") == 0);
    assert(card.skill_count == 0);

    as_card_set_location(&card, "Living Room");
    assert(strcmp(card.location, "Living Room") == 0);

    printf("PASS\n");
}

static void test_skill_registration(void) {
    printf("  test_skill_registration... ");

    as_agent_card_t card;
    as_card_init(&card, "D001", "Test", "Desc", "1.0", "esp32");

    const char *tools1[] = {"read_temp", "read_humidity"};
    assert(as_card_add_skill(&card, "environmental", "Sensor reading", tools1, 2) == 0);
    assert(card.skill_count == 1);

    const char *tools2[] = {"set_led", "set_gpio"};
    assert(as_card_add_skill(&card, "actuators", "Device control", tools2, 2) == 0);
    assert(card.skill_count == 2);

    /* Overflow */
    for (int i = 2; i < CONFIG_AGENTSCOPE_CORE_CARD_MAX_SKILLS; i++) {
        as_card_add_skill(&card, "fill", "fill", NULL, 0);
    }
    assert(as_card_add_skill(&card, "overflow", "test", NULL, 0) == -1);

    printf("PASS\n");
}

static void test_to_json(void) {
    printf("  test_to_json... ");

    as_agent_card_t card;
    as_card_init(&card, "ESP-42", "Smart Box", "IoT controller", "2.0.0", "esp-box-3");

    const char *tools[] = {"read_temperature", "set_led"};
    as_card_add_skill(&card, "sensors", "Environmental sensors", tools, 2);

    cJSON *json = as_agent_card_to_json(&card);
    assert(json != NULL);

    assert(strcmp(cJSON_GetObjectItem(json, "name")->valuestring, "Smart Box") == 0);
    assert(strcmp(cJSON_GetObjectItem(json, "version")->valuestring, "2.0.0") == 0);

    cJSON *caps = cJSON_GetObjectItem(json, "capabilities");
    assert(caps != NULL);
    assert(cJSON_IsFalse(cJSON_GetObjectItem(caps, "streaming")));
    assert(cJSON_IsTrue(cJSON_GetObjectItem(caps, "pushNotifications")));

    cJSON *skills = cJSON_GetObjectItem(json, "skills");
    assert(cJSON_GetArraySize(skills) == 1);

    cJSON *transports = cJSON_GetObjectItem(json, "transport");
    assert(cJSON_GetArraySize(transports) == 1);

    cJSON_Delete(json);
    printf("PASS\n");
}

static void test_capability_manifest(void) {
    printf("  test_capability_manifest... ");

    as_agent_card_t card;
    as_card_init(&card, "DEV-01", "Sensor Node", "Monitors environment", "1.0.0", "esp32-c3");
    as_card_set_location(&card, "Kitchen");

    cJSON *m = as_agent_card_build_capability_manifest(&card);
    assert(m != NULL);
    assert(strcmp(cJSON_GetObjectItem(m, "device_id")->valuestring, "DEV-01") == 0);
    assert(strcmp(cJSON_GetObjectItem(m, "location")->valuestring, "Kitchen") == 0);
    assert(cJSON_GetObjectItem(m, "total_tools")->valuedouble == 0);

    cJSON_Delete(m);
    printf("PASS\n");
}

int main(void) {
    printf("=== test_card ===\n");
    test_init();
    test_skill_registration();
    test_to_json();
    test_capability_manifest();
    printf("All tests passed!\n");
    return 0;
}
