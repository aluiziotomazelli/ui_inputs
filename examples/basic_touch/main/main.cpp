#include <cstdio>
#include <cstdint>

#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "hal_touch.hpp"
#include "hal_timer.hpp"
#include "touch_button.hpp"

#if __has_include("driver/touch_sens.h")
#include "driver/touch_sens.h"
#include "soc/touch_sensor_channel.h"
#endif

static const char *TAG = "BasicTouchExample";

/**
 * @brief FreeRTOS Task that periodically updates and checks TouchButton events.
 */
static void touch_task(void *pvParameters) {
    auto *touch_button = static_cast<ui_inputs::TouchButton *>(pvParameters);

    ESP_LOGI(TAG, "Touch Button Task started. Monitoring touch input...");

    while (true) {
        touch_button->update();

        ui_inputs::ButtonClickType click_type = touch_button->get_last_click();
        if (click_type != ui_inputs::ButtonClickType::NONE_CLICK) {
            switch (click_type) {
                case ui_inputs::ButtonClickType::CLICK:
                    ESP_LOGI(TAG, ">>> EVENT DETECTED: [SINGLE CLICK] <<<");
                    break;
                case ui_inputs::ButtonClickType::LONG_CLICK:
                    ESP_LOGI(TAG, ">>> EVENT DETECTED: [LONG PRESS / HOLD] <<<");
                    break;
                case ui_inputs::ButtonClickType::HOLD_REPEAT:
                    ESP_LOGI(TAG, ">>> EVENT DETECTED: [HOLD REPEAT] <<<");
                    break;
                default:
                    break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // Poll every 10ms
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Initializing Basic Touch Example (Self-Contained)");

    static idf_hals::HalTouch hal_touch;
    static idf_hals::TimerHAL hal_timer;

#if __has_include("driver/touch_sens.h")
    ui_inputs::TouchButtonConfig btn_config;
    btn_config.threshold_delta = 150; // Adjusted threshold for ideal sensitivity
#if SOC_TOUCH_SENSOR_VERSION == 1
    btn_config.trigger_mode = ui_inputs::TouchTriggerMode::BELOW_BASELINE;
#else
    btn_config.trigger_mode = ui_inputs::TouchTriggerMode::ABOVE_BASELINE;
#endif

    // Instantiate TouchButton directly using TOUCH_PAD_GPIO4_CHANNEL (GPIO 4 on ESP32)
    static ui_inputs::TouchButton touch_btn(hal_touch, hal_timer, TOUCH_PAD_GPIO4_CHANNEL, btn_config);

    // .init() automatically allocates global controller (if 1st instance), creates channel, warms up filters & starts scanning!
    ESP_ERROR_CHECK(touch_btn.init());

    // Launch FreeRTOS task to monitor input
    xTaskCreate(touch_task, "touch_task", 4096, &touch_btn, 5, nullptr);
#else
    ESP_LOGE(TAG, "Touch Sensor driver is not supported on this target.");
#endif
}
