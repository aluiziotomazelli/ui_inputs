#pragma once

#include <cstdint>
#include "interfaces/i_button.hpp"
#include "interfaces/i_hal_gpio.hpp"
#include "interfaces/i_hal_timer.hpp"

namespace ui_inputs {

/**
 * @brief Configuration parameters for the button
 */
struct ButtonConfig {
    uint32_t debounce_press_ms{20};
    uint32_t debounce_release_ms{20};
    uint32_t double_click_ms{300};
    uint32_t long_click_ms{1000};
    uint32_t very_long_click_ms{3000};
    uint32_t timeout_ms{6000};
    bool enable_internal_pull{true};
};

/**
 * @brief Button implementation using FSM and HAL interfaces
 */
class Button : public IButton {
public:
    Button(idf_hals::IGpioHAL& gpio_hal,
           idf_hals::ITimerHAL& timer_hal,
           gpio_num_t pin,
           bool active_low,
           const ButtonConfig& config = ButtonConfig{});

    esp_err_t init() override;
    esp_err_t deinit() override;
    void update() override;
    ButtonClickType get_last_click() override;

private:
    enum class State {
        WAIT_FOR_PRESS,
        DEBOUNCE_PRESS,
        WAIT_FOR_RELEASE,
        DEBOUNCE_RELEASE,
        WAIT_FOR_DOUBLE,
        TIMEOUT_WAIT_FOR_RELEASE
    };

    idf_hals::IGpioHAL& gpio_hal_;
    idf_hals::ITimerHAL& timer_hal_;
    gpio_num_t pin_;
    bool active_low_;
    ButtonConfig config_;

    State state_;
    uint32_t last_time_ms_;
    uint32_t press_start_time_ms_;
    bool first_click_;
    bool is_initialized_;
    
    ButtonClickType last_click_type_;
};

} // namespace ui_inputs
