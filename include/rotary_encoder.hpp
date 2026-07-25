#pragma once

#include <cstdint>
#include "interfaces/i_rotary_encoder.hpp"
#include "interfaces/i_hal_gpio.hpp"
#include "interfaces/i_hal_pcnt.hpp"
#include "interfaces/i_hal_timer.hpp"

namespace ui_inputs {

/**
 * @brief Configuration parameters for the rotary encoder
 */
struct RotaryEncoderConfig {
    bool half_step_mode{true};
    bool acceleration_enabled{true};
    uint16_t accel_gap_ms{50};
    uint8_t accel_max_multiplier{5};
    uint32_t glitch_filter_ns{1000}; ///< Hardware glitch filter threshold in nanoseconds (default 1us)
};

/**
 * @brief Rotary encoder implementation using ESP32 Hardware PCNT (Pulse Counter)
 */
class RotaryEncoder : public IRotaryEncoder {
public:
    RotaryEncoder(idf_hals::IPcntHAL& pcnt_hal,
                  idf_hals::ITimerHAL& timer_hal,
                  gpio_num_t pin_a,
                  gpio_num_t pin_b,
                  const RotaryEncoderConfig& config = RotaryEncoderConfig{});

    esp_err_t init() override;
    esp_err_t deinit() override;
    void update() override;
    int32_t get_steps() override;

private:
    int32_t map_value(int32_t x, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max);

    idf_hals::IPcntHAL& pcnt_hal_;
    idf_hals::ITimerHAL& timer_hal_;
    gpio_num_t pin_a_;
    gpio_num_t pin_b_;
    RotaryEncoderConfig config_;

    pcnt_unit_handle_t pcnt_unit_{nullptr};
    pcnt_channel_handle_t pcnt_chan_a_{nullptr};
    pcnt_channel_handle_t pcnt_chan_b_{nullptr};

    uint32_t last_step_time_ms_{0};
    int32_t accumulated_steps_{0};
    bool is_initialized_{false};
};

} // namespace ui_inputs
