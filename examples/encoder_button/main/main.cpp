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

// GPIO Pin definitions for EC11 Rotary Encoder, integrated Push Button, and ESP32 DevKit BOOT button
static constexpr gpio_num_t ENCODER_PIN_A   = GPIO_NUM_18; // CLK Pin
static constexpr gpio_num_t ENCODER_PIN_B   = GPIO_NUM_19; // DT Pin
static constexpr gpio_num_t ENCODER_SW_PIN  = GPIO_NUM_21; // SW (Rotary Encoder Push Button) Pin
static constexpr gpio_num_t BOOT_BUTTON_PIN = GPIO_NUM_0;  // On-board BOOT Button Pin (ESP32 DevKitC)

struct EncoderButtonHandles
{
    ui_inputs::RotaryEncoder* encoder;
    ui_inputs::Button* encoder_button;
    ui_inputs::Button* boot_button;
};

/**
 * @brief FreeRTOS Task that periodically polls the rotary encoder (PCNT hardware) and both push buttons.
 */
static void encoder_button_task(void* pvParameters)
{
    auto* handles        = static_cast<EncoderButtonHandles*>(pvParameters);
    auto* encoder        = handles->encoder;
    auto* encoder_button = handles->encoder_button;
    auto* boot_button    = handles->boot_button;

    ESP_LOGI(TAG, "Encoder + Dual Push Button task started.");
    ESP_LOGI(TAG, "Pins configured: Encoder A=GPIO%d, B=GPIO%d | SW=GPIO%d | BOOT=GPIO%d", 
             ENCODER_PIN_A, ENCODER_PIN_B, ENCODER_SW_PIN, BOOT_BUTTON_PIN);

    int32_t current_position = 0;

    while (true) {
        // 1. Update component state machines
        encoder->update();
        encoder_button->update();
        boot_button->update();

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

        // 3. Process Rotary Encoder Push Button click events
        ui_inputs::ButtonClickType sw_click = encoder_button->get_last_click();
        if (sw_click != ui_inputs::ButtonClickType::NONE_CLICK) {
            switch (sw_click) {
            case ui_inputs::ButtonClickType::CLICK:
                ESP_LOGI(TAG, ">>> ENCODER SW EVENT: [SINGLE CLICK] <<<");
                break;
            case ui_inputs::ButtonClickType::DOUBLE_CLICK:
                ESP_LOGI(TAG, ">>> ENCODER SW EVENT: [DOUBLE CLICK] <<<");
                break;
            case ui_inputs::ButtonClickType::LONG_CLICK:
                ESP_LOGI(TAG, ">>> ENCODER SW EVENT: [LONG PRESS] <<<");
                break;
            case ui_inputs::ButtonClickType::VERY_LONG_CLICK:
                ESP_LOGI(TAG, ">>> ENCODER SW EVENT: [VERY LONG PRESS] <<<");
                break;
            default:
                break;
            }
        }

        // 4. Process DevKit BOOT Button click events
        ui_inputs::ButtonClickType boot_click = boot_button->get_last_click();
        if (boot_click != ui_inputs::ButtonClickType::NONE_CLICK) {
            switch (boot_click) {
            case ui_inputs::ButtonClickType::CLICK:
                ESP_LOGI(TAG, ">>> BOOT BUTTON EVENT: [SINGLE CLICK] <<<");
                break;
            case ui_inputs::ButtonClickType::DOUBLE_CLICK:
                ESP_LOGI(TAG, ">>> BOOT BUTTON EVENT: [DOUBLE CLICK] <<<");
                break;
            case ui_inputs::ButtonClickType::LONG_CLICK:
                ESP_LOGI(TAG, ">>> BOOT BUTTON EVENT: [LONG PRESS] <<<");
                break;
            case ui_inputs::ButtonClickType::VERY_LONG_CLICK:
                ESP_LOGI(TAG, ">>> BOOT BUTTON EVENT: [VERY LONG PRESS] <<<");
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
    ESP_LOGI(TAG, "Initializing Rotary Encoder (PCNT) + Encoder SW + DevKit BOOT Button Example");

    // Instantiating Hardware Abstraction Layer implementations
    static idf_hals::GpioHAL hal_gpio;
    static idf_hals::TimerHAL hal_timer;
    static idf_hals::HalPcnt hal_pcnt;

    // Instantiate Rotary Encoder component (Full-Step Mode)
    ui_inputs::RotaryEncoderConfig encoder_cfg;
    encoder_cfg.half_step_mode = false;  // Full-step mode (1 detent = 4 pulses)
    encoder_cfg.acceleration_enabled = true;
    encoder_cfg.glitch_filter_ns = 1000; // 1us hardware glitch filter

    static ui_inputs::RotaryEncoder encoder(hal_pcnt, hal_timer, ENCODER_PIN_A, ENCODER_PIN_B, encoder_cfg);
    ESP_ERROR_CHECK(encoder.init());

    // Instantiate Push Button component for Rotary Encoder SW (Active Low with internal pull-up)
    ui_inputs::ButtonConfig btn_cfg;
    btn_cfg.debounce_press_ms = 20;
    btn_cfg.debounce_release_ms = 20;
    btn_cfg.double_click_ms = 300;
    btn_cfg.long_click_ms = 1000;
    btn_cfg.enable_internal_pull = true;

    static ui_inputs::Button encoder_button(hal_gpio, hal_timer, ENCODER_SW_PIN, true /* active_low */, btn_cfg);
    ESP_ERROR_CHECK(encoder_button.init());

    // Instantiate Push Button component for DevKit BOOT Button (GPIO 0, Active Low with internal pull-up)
    static ui_inputs::Button boot_button(hal_gpio, hal_timer, BOOT_BUTTON_PIN, true /* active_low */, btn_cfg);
    ESP_ERROR_CHECK(boot_button.init());

    static EncoderButtonHandles handles = {&encoder, &encoder_button, &boot_button};

    // Launch FreeRTOS Task
    xTaskCreate(encoder_button_task, "encoder_btn_task", 4096, &handles, 5, nullptr);
}
