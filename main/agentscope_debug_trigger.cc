// AgentScope 调试触发通道（仅 CONFIG_XIAOZHI_AGENTSCOPE_DEBUG_TRIGGER=y 时编入）。
// 用途：在真机上驱动 S2 文本轮次与凭证写入，为 G4 设备门取证提供可复跑的输入面。
// 这是测试档，默认关闭（Kconfig default n），不参与发布固件的语音路由。
// 串口命令（USB-Serial-JTAG）：
//   as:<text> / st:<text>  触发一轮本地文本会话
//   key:<key>             写入 NVS agentscope/api_key
//   model:<name>          写入 NVS agentscope/model
#include "sdkconfig.h"
#if CONFIG_XIAOZHI_AGENTSCOPE_ADAPTER && CONFIG_XIAOZHI_AGENTSCOPE_DEBUG_TRIGGER

#include <cstdio>
#include <cstring>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs.h"
#include "driver/usb_serial_jtag.h"
#include "application.h"

namespace {

constexpr const char* TAG = "as_debug";
constexpr size_t kLineMax = 512;
constexpr uint32_t kTaskStack = 4096;
constexpr UBaseType_t kTaskPriority = 4;

void write_nvs_str(const char* key, const char* value) {
    nvs_handle_t handle = 0;
    if (nvs_open("agentscope", NVS_READWRITE, &handle) != ESP_OK) {
        ESP_LOGE(TAG, "nvs open failed for key %s", key);
        return;
    }
    esp_err_t status = nvs_set_str(handle, key, value);
    if (status == ESP_OK) {
        status = nvs_commit(handle);
    }
    nvs_close(handle);
    ESP_LOGI(TAG, "%s %s (len=%u)", key, status == ESP_OK ? "written" : "write failed",
             static_cast<unsigned>(strlen(value)));
}

void handle_line(const std::string& line) {
    const auto colon = line.find(':');
    if (colon == std::string::npos) {
        ESP_LOGW(TAG, "ignored %u-byte line (expected <cmd>:<value>)",
                 static_cast<unsigned>(line.size()));
        return;
    }
    const std::string cmd = line.substr(0, colon);
    const std::string value = line.substr(colon + 1);
    if (cmd == "as" || cmd == "st") {
        const bool queued = Application::GetInstance().RequestLocalTurn(value);
        ESP_LOGI(TAG, "%s turn %s", cmd.c_str(), queued ? "queued" : "rejected");
    } else if (cmd == "key") {
        write_nvs_str("api_key", value.c_str());
    } else if (cmd == "model") {
        write_nvs_str("model", value.c_str());
    } else {
        ESP_LOGW(TAG, "unknown command %s", cmd.c_str());
    }
}

void trigger_task(void*) {
    usb_serial_jtag_driver_config_t config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    config.rx_buffer_size = 1024;
    config.tx_buffer_size = 1024;
    if (usb_serial_jtag_driver_install(&config) != ESP_OK) {
        ESP_LOGE(TAG, "usb_serial_jtag driver install failed; trigger disabled");
        vTaskDelete(nullptr);
        return;
    }
    ESP_LOGI(TAG, "debug trigger ready (as:/st:/key:/model:)");
    std::string line;
    line.reserve(kLineMax);
    uint8_t chunk[128];
    while (true) {
        const int read = usb_serial_jtag_read_bytes(chunk, sizeof(chunk), pdMS_TO_TICKS(200));
        for (int i = 0; i < read; ++i) {
            const char c = static_cast<char>(chunk[i]);
            if (c == '\r') {
                continue;
            }
            if (c == '\n') {
                if (!line.empty()) {
                    handle_line(line);
                    line.clear();
                }
                continue;
            }
            if (line.size() < kLineMax) {
                line.push_back(c);
            }
        }
    }
}

}  // namespace

namespace agentscope_debug {
/// 启动调试触发任务；仅在测试档调用。
void Start() {
    xTaskCreate(trigger_task, "as_debug", kTaskStack, nullptr, kTaskPriority, nullptr);
}
}  // namespace agentscope_debug

#endif  // CONFIG_XIAOZHI_AGENTSCOPE_ADAPTER && CONFIG_XIAOZHI_AGENTSCOPE_DEBUG_TRIGGER
