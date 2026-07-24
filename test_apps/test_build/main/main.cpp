#include "esp_log.h"
#include "button.hpp"
#include "rotary_encoder.hpp"
#include "hal_gpio.hpp"
#include "hal_timer.hpp"

extern "C" void app_main(void)
{
    ESP_LOGI("main", "UI Inputs Build Test");

    // 1. Declare the tools (stateless HALs)
    static idf_hals::GpioHAL gpio;
    static idf_hals::TimerHAL timer;

    // 2. Configure and create components
    ui_inputs::ButtonConfig btn_cfg;
    ui_inputs::Button btn(gpio, timer, GPIO_NUM_4, true, btn_cfg);
    
    ui_inputs::RotaryEncoderConfig enc_cfg;
    ui_inputs::RotaryEncoder enc(gpio, timer, GPIO_NUM_5, GPIO_NUM_6, enc_cfg);

    // Call update to verify compilation
    btn.update();
    enc.update();
    
    ESP_LOGI("main", "Build test passed");
}
