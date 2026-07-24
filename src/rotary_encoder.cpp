#include "rotary_encoder.hpp"

#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"

static const char *TAG = "RotaryEncoder";

namespace ui_inputs {

#define R_START     0x0
#define DIR_CW      0x10
#define DIR_CCW     0x20

#define FS_R_CW_FINAL   0x1
#define FS_R_CW_BEGIN   0x2
#define FS_R_CW_NEXT    0x3
#define FS_R_CCW_BEGIN  0x4
#define FS_R_CCW_FINAL  0x5
#define FS_R_CCW_NEXT   0x6

#define H_CCW_BEGIN     0x1
#define H_CW_BEGIN      0x2
#define H_START_M       0x3
#define H_CW_BEGIN_M    0x4
#define H_CCW_BEGIN_M   0x5

static const unsigned char ttable_full_step[7][4] = {
    {R_START,    FS_R_CW_BEGIN, FS_R_CCW_BEGIN, R_START},
    {FS_R_CW_NEXT, R_START,    FS_R_CW_FINAL, R_START | DIR_CW},
    {FS_R_CW_NEXT, FS_R_CW_BEGIN, R_START,    R_START},
    {FS_R_CW_NEXT, FS_R_CW_BEGIN, FS_R_CW_FINAL, R_START},
    {FS_R_CCW_NEXT, R_START,    FS_R_CCW_BEGIN, R_START},
    {FS_R_CCW_NEXT, FS_R_CCW_FINAL, R_START,    R_START | DIR_CCW},
    {FS_R_CCW_NEXT, FS_R_CCW_FINAL, FS_R_CCW_BEGIN, R_START},
};

static const unsigned char ttable_half_step[6][4] = {
    {H_START_M, H_CW_BEGIN,  H_CCW_BEGIN, R_START},
    {H_START_M | DIR_CCW, R_START, H_CCW_BEGIN, R_START},
    {H_START_M | DIR_CW, H_CW_BEGIN, R_START, R_START},
    {H_START_M, H_CCW_BEGIN_M, H_CW_BEGIN_M, R_START},
    {H_START_M, H_START_M, H_CW_BEGIN_M, R_START | DIR_CW},
    {H_START_M, H_CCW_BEGIN_M, H_START_M, R_START | DIR_CCW}
};

RotaryEncoder::RotaryEncoder(idf_hals::IGpioHAL& gpio_hal,
                             idf_hals::ITimerHAL& timer_hal,
                             gpio_num_t pin_a,
                             gpio_num_t pin_b,
                             const RotaryEncoderConfig& config)
    : gpio_hal_(gpio_hal),
      timer_hal_(timer_hal),
      pin_a_(pin_a),
      pin_b_(pin_b),
      config_(config),
      rotary_state_(R_START),
      last_step_time_ms_(0),
      accumulated_steps_(0) {
}

long RotaryEncoder::map_value(long x, long in_min, long in_max, long out_min, long out_max) {
    if (in_min == in_max) return out_min;
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void RotaryEncoder::update() {
    uint8_t current_pin_states = (gpio_hal_.get_level(pin_a_) << 1) | gpio_hal_.get_level(pin_b_);
    
    const unsigned char(*current_ttable)[4] = config_.half_step_mode ? ttable_half_step : ttable_full_step;

    rotary_state_ = current_ttable[rotary_state_ & 0x0F][current_pin_states];

    uint8_t direction = rotary_state_ & 0x30;
    int steps = 0;

    if (direction == DIR_CW) {
        steps = 1;
    } else if (direction == DIR_CCW) {
        steps = -1;
    }

    if (steps != 0) {
        int current_multiplier_val = 1;

        if (config_.acceleration_enabled) {
            uint32_t current_time_ms = timer_hal_.get_time_us() / 1000;
            uint32_t turn_interval_ms = current_time_ms - last_step_time_ms_;

            if (turn_interval_ms < config_.accel_gap_ms && last_step_time_ms_ != 0) {
                current_multiplier_val = map_value(
                    config_.accel_gap_ms - turn_interval_ms,
                    1, config_.accel_gap_ms,
                    1, config_.accel_max_multiplier + 1);

                if (current_multiplier_val < 1) current_multiplier_val = 1;
                if (current_multiplier_val > config_.accel_max_multiplier) current_multiplier_val = config_.accel_max_multiplier;
            }
            last_step_time_ms_ = current_time_ms;
        } else {
            last_step_time_ms_ = timer_hal_.get_time_us() / 1000;
        }

        steps *= current_multiplier_val;
        accumulated_steps_ += steps;
    }
}

int32_t RotaryEncoder::get_steps() {
    int32_t steps = accumulated_steps_;
    accumulated_steps_ = 0;
    return steps;
}

} // namespace ui_inputs
