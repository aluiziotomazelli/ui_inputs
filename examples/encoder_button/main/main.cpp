#include <cstdio>
#include <cstdint>
#include <cinttypes>

#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "hal_gpio.hpp"
#include "hal_timer.hpp"
#include "hal_pcnt.hpp"
#include "rotary_encoder.hpp"
#include "button.hpp"

static const char* TAG = "EncoderButtonExample";

// GPIO Pin definitions for EC11 Rotary Encoder with integrated Push Button
static constexpr gpio_num_t ENCODER_PIN_A = GPIO_NUM_18; // CLK Pin
static constexpr gpio_num_t ENCODER_PIN_B = GPIO_NUM_19; // DT Pin
static constexpr gpio_num_t BUTTON_PIN = GPIO_NUM_21;    // SW (Push Button) Pin

struct EncoderButtonHandles
{
    ui_inputs::RotaryEncoder* encoder;
    ui_inputs::Button* button;
};

/**
 * @brief FreeRTOS Task that periodically polls the rotary encoder (PCNT hardware) and integrated push button.
 */
static void encoder_button_task(void* pvParameters)
{
    auto* handles = static_cast<EncoderButtonHandles*>(pvParameters);
    auto* encoder = handles->encoder;
    auto* button = handles->button;

    ESP_LOGI(TAG, "Encoder + Push Button task started.");
    ESP_LOGI(TAG, "Pins configured: Pin A=GPIO%d, Pin B=GPIO%d, SW=GPIO%d", ENCODER_PIN_A, ENCODER_PIN_B, BUTTON_PIN);

    int32_t current_position = 0;

    while (true) {
        // 1. Update component state machines
        encoder->update();
        button->update();

        // 2. Process Rotary Encoder rotation steps
        int32_t delta_steps = encoder->get_steps();
        if (delta_steps != 0) {
            current_position += delta_steps;
            if (delta_steps > 0) {
                ESP_LOGI(
                    TAG,
                    ">>> ROTATION [CLOCKWISE] | Delta: +%" PRId32 " | Total Position: %" PRId32 " <<<",
                    delta_steps,
                    current_position);
            }
            else {
                ESP_LOGI(
                    TAG,
                    ">>> ROTATION [COUNTER-CLOCKWISE] | Delta: %" PRId32 " | Total Position: %" PRId32 " <<<",
                    delta_steps,
                    current_position);
            }
        }

        // 3. Process Push Button click events
        ui_inputs::ButtonClickType click_type = button->get_last_click();
        if (click_type != ui_inputs::ButtonClickType::NONE_CLICK) {
            switch (click_type) {
            case ui_inputs::ButtonClickType::CLICK:
                ESP_LOGI(TAG, ">>> BUTTON EVENT: [SINGLE CLICK] <<<");
                break;
            case ui_inputs::ButtonClickType::DOUBLE_CLICK:
                ESP_LOGI(TAG, ">>> BUTTON EVENT: [DOUBLE CLICK] <<<");
                break;
            case ui_inputs::ButtonClickType::LONG_CLICK:
                ESP_LOGI(TAG, ">>> BUTTON EVENT: [LONG PRESS] <<<");
                break;
            case ui_inputs::ButtonClickType::VERY_LONG_CLICK:
                ESP_LOGI(TAG, ">>> BUTTON EVENT: [VERY LONG PRESS] <<<");
                break;
            default:
                break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // Poll inputs every 10ms
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Initializing Rotary Encoder (PCNT Hardware) + Integrated Push Button Example");

    // Instantiating Hardware Abstraction Layer implementations
    static idf_hals::GpioHAL hal_gpio;
    static idf_hals::TimerHAL hal_timer;
    static idf_hals::HalPcnt hal_pcnt;

    // Instantiate Rotary Encoder component using ESP32 PCNT Hardware decoding
    ui_inputs::RotaryEncoderConfig encoder_cfg;
    encoder_cfg.half_step_mode = true;
    encoder_cfg.acceleration_enabled = true;
    encoder_cfg.glitch_filter_ns = 1000; // 1us hardware glitch filter

    static ui_inputs::RotaryEncoder encoder(hal_pcnt, hal_timer, ENCODER_PIN_A, ENCODER_PIN_B, encoder_cfg);
    ESP_ERROR_CHECK(encoder.init());

    // Instantiate Push Button component (Active Low with internal pull-up)
    ui_inputs::ButtonConfig btn_cfg;
    btn_cfg.debounce_press_ms = 20;
    btn_cfg.debounce_release_ms = 20;
    btn_cfg.double_click_ms = 300;
    btn_cfg.long_click_ms = 1000;
    btn_cfg.enable_internal_pull = true;

    static ui_inputs::Button button(hal_gpio, hal_timer, BUTTON_PIN, true /* active_low */, btn_cfg);
    ESP_ERROR_CHECK(button.init());

    static EncoderButtonHandles handles = {&encoder, &button};

    // Launch FreeRTOS Task
    xTaskCreate(encoder_button_task, "encoder_btn_task", 4096, &handles, 5, nullptr);
}
