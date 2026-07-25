#include <cstdio>
#include <cstdint>
#include <cinttypes>

#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "hal_touch.hpp"
#include "hal_timer.hpp"
#include "touch_button.hpp"

#if __has_include("driver/touch_sens.h")
#include "driver/touch_sens.h"
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
                    ESP_LOGI(TAG, "Event Detected: [SINGLE CLICK]");
                    break;
                case ui_inputs::ButtonClickType::DOUBLE_CLICK:
                    ESP_LOGI(TAG, "Event Detected: [DOUBLE CLICK]");
                    break;
                case ui_inputs::ButtonClickType::LONG_CLICK:
                    ESP_LOGI(TAG, "Event Detected: [LONG PRESS / HOLD]");
                    break;
                case ui_inputs::ButtonClickType::HOLD_REPEAT:
                    ESP_LOGI(TAG, "Event Detected: [HOLD REPEAT]");
                    break;
                default:
                    break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // Poll every 10ms
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Initializing Basic Touch Example");

    static idf_hals::HalTouch hal_touch;
    static idf_hals::TimerHAL hal_timer;

#if __has_include("driver/touch_sens.h")
    touch_sensor_handle_t sens_handle = nullptr;
    touch_channel_handle_t chan_handle = nullptr;

    // 1. Controller & Channel configuration based on hardware version
#if SOC_TOUCH_SENSOR_VERSION == 1
    touch_sensor_sample_config_t sample_cfg[TOUCH_SAMPLE_CFG_NUM] = {
        TOUCH_SENSOR_V1_DEFAULT_SAMPLE_CONFIG(5.0, TOUCH_VOLT_LIM_L_0V5, TOUCH_VOLT_LIM_H_1V7)
    };
    touch_channel_config_t chan_cfg = {
        .abs_active_thresh = {1000},
        .charge_speed = TOUCH_CHARGE_SPEED_7,
        .init_charge_volt = TOUCH_INIT_CHARGE_VOLT_DEFAULT,
        .group = TOUCH_CHAN_TRIG_GROUP_BOTH
    };
    ui_inputs::TouchTriggerMode trigger_mode = ui_inputs::TouchTriggerMode::BELOW_BASELINE;
#else
    touch_sensor_sample_config_t sample_cfg[TOUCH_SAMPLE_CFG_NUM] = {
        TOUCH_SENSOR_V2_DEFAULT_SAMPLE_CONFIG(500, TOUCH_VOLT_LIM_L_0V5, TOUCH_VOLT_LIM_H_2V2)
    };
    touch_channel_config_t chan_cfg = {
        .active_thresh = {2000},
        .charge_speed = TOUCH_CHARGE_SPEED_7,
        .init_charge_volt = TOUCH_INIT_CHARGE_VOLT_DEFAULT
    };
    ui_inputs::TouchTriggerMode trigger_mode = ui_inputs::TouchTriggerMode::ABOVE_BASELINE;
#endif

    touch_sensor_config_t sens_cfg = TOUCH_SENSOR_DEFAULT_BASIC_CONFIG(TOUCH_SAMPLE_CFG_NUM, sample_cfg);
    ESP_ERROR_CHECK(hal_touch.new_controller(&sens_cfg, &sens_handle));

    // 2. Channel Configuration (Touch Channel 0 / TOUCH_MIN_CHAN_ID)
    // GPIO Mapping:
    // ESP32 (V1): TOUCH_MIN_CHAN_ID (Channel 0) = GPIO 4
    // ESP32-S3 (V2): TOUCH_MIN_CHAN_ID (Channel 1) = GPIO 1
    int chan_id = TOUCH_MIN_CHAN_ID;
    ESP_ERROR_CHECK(hal_touch.new_channel(sens_handle, chan_id, &chan_cfg, &chan_handle));

    // 3. Filter Configuration
    touch_sensor_filter_config_t filter_cfg = TOUCH_SENSOR_DEFAULT_FILTER_CONFIG();
    ESP_ERROR_CHECK(hal_touch.config_filter(sens_handle, &filter_cfg));

    // 4. Enable Controller
    ESP_ERROR_CHECK(hal_touch.enable(sens_handle));

    // 5. Read initial baseline capacitance
    vTaskDelay(pdMS_TO_TICKS(50)); // Allow filters to settle
    uint32_t initial_baseline = 0;
    ESP_ERROR_CHECK(hal_touch.read_channel_data(chan_handle, TOUCH_CHAN_DATA_TYPE_SMOOTH, &initial_baseline));
    ESP_LOGI(TAG, "Touch Channel %d Initial Baseline: %" PRIu32, chan_id, initial_baseline);

    // 6. Create TouchButton instance
    ui_inputs::TouchButtonConfig btn_config;
    btn_config.trigger_mode = trigger_mode;
    btn_config.threshold_delta = 100;

    static ui_inputs::TouchButton touch_btn(hal_touch, hal_timer, chan_handle, initial_baseline, btn_config);
    ESP_ERROR_CHECK(touch_btn.init());

    // 7. Launch FreeRTOS task to monitor input
    xTaskCreate(touch_task, "touch_task", 4096, &touch_btn, 5, nullptr);
#else
    ESP_LOGE(TAG, "Touch Sensor driver (driver/touch_sens.h) is not supported on this platform.");
#endif
}
