#include <esp_log.h>
#include <esp_err.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "application.h"
#if CONFIG_XIAOZHI_AGENTSCOPE_DEBUG_TRIGGER
namespace agentscope_debug { void Start(); }
#endif

#define TAG "main"

extern "C" void app_main(void)
{
    // Initialize NVS flash for WiFi configuration
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Erasing NVS flash to fix corruption");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize and run the application
    auto& app = Application::GetInstance();
    app.Initialize();
#if CONFIG_XIAOZHI_AGENTSCOPE_DEBUG_TRIGGER
    // G4 取证脚手架：Initialize 之后适配器已存在，串口触发才有意义。
    agentscope_debug::Start();
#endif
    app.Run();  // This function runs the main event loop and never returns
}
