#pragma once

#include <cstdint>
#include "interfaces/i_rotary_encoder.hpp"
#include "interfaces/i_hal_gpio.hpp"
#include "interfaces/i_hal_timer.hpp"

namespace ui_inputs {

/**
 * @brief Configuration parameters for the rotary encoder
 */
struct RotaryEncoderConfig {
    bool half_step_mode{true};
    bool acceleration_enabled{true};
    uint16_t accel_gap_ms{50};
    uint32_t accel_multiplier{2};        ///< Multiplier added per fast step
    bool enable_internal_pull{true};       ///< Enable internal pull-up resistors for pins A and B
};

/**
 * @brief Rotary encoder implementation using FSM and HAL interfaces
 */
class RotaryEncoder : public IRotaryEncoder {
public:
    RotaryEncoder(idf_hals::IGpioHAL& gpio_hal,
                  idf_hals::ITimerHAL& timer_hal,
                  gpio_num_t pin_a,
                  gpio_num_t pin_b,
                  const RotaryEncoderConfig& config = RotaryEncoderConfig{});

    void update() override;
    int32_t get_steps() override;

private:
    int32_t map_value(int32_t x, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max);

    idf_hals::IGpioHAL& gpio_hal_;
    idf_hals::ITimerHAL& timer_hal_;
    gpio_num_t pin_a_;
    gpio_num_t pin_b_;
    RotaryEncoderConfig config_;

    uint8_t rotary_state_;
    uint32_t last_step_time_ms_;
    int32_t accumulated_steps_;
};

} // namespace ui_inputs
