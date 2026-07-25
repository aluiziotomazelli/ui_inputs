#include "touch_button.hpp"

#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"

static const char *TAG = "TouchButton";

namespace ui_inputs {

TouchButton::TouchButton(idf_hals::ITouchHAL& touch_hal,
                         idf_hals::ITimerHAL& timer_hal,
                         touch_channel_handle_t chan_handle,
                         uint32_t initial_baseline,
                         const TouchButtonConfig& config)
    : touch_hal_(touch_hal),
      timer_hal_(timer_hal),
      chan_handle_(chan_handle),
      config_(config),
      state_(State::WAIT_FOR_PRESS),
      baseline_(initial_baseline),
      last_time_ms_(0),
      press_start_time_ms_(0),
      last_hold_event_ms_(0),
      last_recalib_time_ms_(0),
      hold_generated_(false),
      last_click_type_(ButtonClickType::NONE_CLICK) {
}

bool TouchButton::is_touched(uint32_t raw_val) const {
    if (config_.trigger_mode == TouchTriggerMode::ABOVE_BASELINE) {
        return raw_val > (baseline_ + config_.threshold_delta);
    } else {
        return raw_val < (baseline_ > config_.threshold_delta ? baseline_ - config_.threshold_delta : 0);
    }
}

void TouchButton::update() {
    uint32_t now = timer_hal_.get_time_us() / 1000;
    uint32_t raw_val = 0;

    if (touch_hal_.read_channel_data(chan_handle_, TOUCH_CHAN_DATA_TYPE_SMOOTH, &raw_val) != ESP_OK) {
        touch_hal_.read_channel_data(chan_handle_, TOUCH_CHAN_DATA_TYPE_RAW, &raw_val);
    }

    bool touched = is_touched(raw_val);
    last_click_type_ = ButtonClickType::NONE_CLICK;

    switch (state_) {
    case State::WAIT_FOR_PRESS:
        if (touched) {
            press_start_time_ms_ = now;
            state_ = State::DEBOUNCE_PRESS;
            ESP_LOGD(TAG, "TOUCH DEBOUNCE START (Raw: %" PRIu32 ", Baseline: %" PRIu32 ")", raw_val, baseline_);
        } else {
            if (last_recalib_time_ms_ == 0) {
                last_recalib_time_ms_ = now;
            } else if (now - last_recalib_time_ms_ >= config_.recalibration_interval_ms) {
                last_recalib_time_ms_ = now;
                baseline_ = raw_val;
                ESP_LOGI(TAG, "Touch baseline recalibrated to %" PRIu32, baseline_);
            }
        }
        break;

    case State::DEBOUNCE_PRESS:
        if (now - press_start_time_ms_ > config_.debounce_press_ms) {
            if (touched) {
                state_ = State::WAIT_FOR_RELEASE_OR_HOLD;
                hold_generated_ = false;
                ESP_LOGD(TAG, "TOUCH WAIT_FOR_RELEASE_OR_HOLD");
            } else {
                state_ = State::WAIT_FOR_PRESS;
                ESP_LOGD(TAG, "Touch glitch filtered, back to WAIT_FOR_PRESS");
            }
        }
        break;

    case State::WAIT_FOR_RELEASE_OR_HOLD:
        if (!touched) {
            last_time_ms_ = now;
            state_ = State::DEBOUNCE_RELEASE;
        } else if (now - press_start_time_ms_ > config_.hold_time_ms) {
            if (!hold_generated_) {
                hold_generated_ = true;
                last_hold_event_ms_ = now;
                last_click_type_ = ButtonClickType::LONG_CLICK;
                ESP_LOGD(TAG, "TOUCH LONG_CLICK (HOLD)");
            } else if (config_.enable_hold_repeat &&
                       (now - last_hold_event_ms_ >= config_.hold_repeat_interval_ms)) {
                last_hold_event_ms_ = now;
                last_click_type_ = ButtonClickType::HOLD_REPEAT;
                ESP_LOGD(TAG, "TOUCH HOLD_REPEAT");
            }
        }
        break;

    case State::DEBOUNCE_RELEASE:
        if (now - last_time_ms_ > config_.debounce_release_ms) {
            state_ = State::WAIT_FOR_PRESS;
            if (!hold_generated_) {
                last_click_type_ = ButtonClickType::CLICK;
                ESP_LOGD(TAG, "TOUCH CLICK");
            }
            hold_generated_ = false;
        }
        break;
    }
}

ButtonClickType TouchButton::get_last_click() {
    ButtonClickType click = last_click_type_;
    last_click_type_ = ButtonClickType::NONE_CLICK;
    return click;
}

} // namespace ui_inputs
