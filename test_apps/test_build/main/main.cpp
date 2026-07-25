#include "esp_log.h"
#include "button.hpp"
#include "rotary_encoder.hpp"
#include "switch.hpp"
#include "touch_button.hpp"
#include "hal_gpio.hpp"
#include "hal_timer.hpp"
#include "hal_pcnt.hpp"
#include "hal_touch.hpp"

extern "C" void app_main(void)
{
    ESP_LOGI("main", "UI Inputs Build Test");

    // 1. Declare the tools (stateless HALs)
    static idf_hals::GpioHAL gpio;
    static idf_hals::TimerHAL timer;
    static idf_hals::HalPcnt pcnt;
    static idf_hals::HalTouch touch;

    // 2. Configure and create all UI input components
    ui_inputs::ButtonConfig btn_cfg;
    ui_inputs::Button btn(gpio, timer, GPIO_NUM_4, true, btn_cfg);
    
    ui_inputs::RotaryEncoderConfig enc_cfg;
    ui_inputs::RotaryEncoder enc(pcnt, timer, GPIO_NUM_5, GPIO_NUM_6, enc_cfg);

    ui_inputs::SwitchConfig sw_cfg;
    ui_inputs::Switch sw(gpio, timer, GPIO_NUM_7, true, sw_cfg);

    ui_inputs::TouchButtonConfig touch_cfg;
    ui_inputs::TouchButton touch_btn(touch, timer, 0 /* channel_id */, touch_cfg);

    // Call update to verify compilation
    btn.update();
    enc.update();
    sw.update();
    touch_btn.update();
    
    ESP_LOGI("main", "Build test passed");
}
