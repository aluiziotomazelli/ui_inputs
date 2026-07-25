#pragma once

#include <cstdint>
#include "interfaces/i_switch.hpp"
#include "interfaces/i_hal_gpio.hpp"
#include "interfaces/i_hal_timer.hpp"

namespace ui_inputs {

/**
 * @brief Configuration parameters for Switch component.
 */
struct SwitchConfig {
    uint32_t debounce_ms{20}; ///< Debounce time in milliseconds
    bool enable_internal_pull{true}; ///< Enable internal pull-up/down resistor based on active_low
};

/**
 * @brief Switch component implementation for bistable inputs using FSM and HAL.
 */
class Switch : public ISwitch {
public:
    Switch(idf_hals::IGpioHAL& gpio_hal,
           idf_hals::ITimerHAL& timer_hal,
           gpio_num_t pin,
           bool active_low,
           const SwitchConfig& config = SwitchConfig{});

    esp_err_t init() override;
    esp_err_t deinit() override;
    void update() override;
    SwitchState get_state() const override;
    SwitchEvent get_last_event() override;

private:
    enum class FsmState {
        STABLE,
        DEBOUNCING
    };

    idf_hals::IGpioHAL& gpio_hal_;
    idf_hals::ITimerHAL& timer_hal_;
    gpio_num_t pin_;
    bool active_low_;
    SwitchConfig config_;

    FsmState fsm_state_;
    SwitchState current_state_;
    SwitchState pending_state_;
    uint32_t state_change_time_ms_;
    SwitchEvent last_event_;
    bool is_initialized_;
};

} // namespace ui_inputs
