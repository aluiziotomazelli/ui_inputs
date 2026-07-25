#include "button.hpp"

#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"

static const char *TAG = "Button";

namespace ui_inputs {

Button::Button(idf_hals::IGpioHAL& gpio_hal,
               idf_hals::ITimerHAL& timer_hal,
               gpio_num_t pin,
               bool active_low,
               const ButtonConfig& config)
    : gpio_hal_(gpio_hal),
      timer_hal_(timer_hal),
      pin_(pin),
      active_low_(active_low),
      config_(config),
      state_(State::WAIT_FOR_PRESS),
      last_time_ms_(0),
      press_start_time_ms_(0),
      first_click_(false),
      last_click_type_(ButtonClickType::NONE_CLICK) {
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << pin_);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    if (config_.enable_internal_pull) {
        io_conf.pull_up_en = active_low_ ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = active_low_ ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE;
    } else {
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    }
    gpio_hal_.config(&io_conf);
}

void Button::update() {
    uint32_t now = timer_hal_.get_time_us() / 1000;
    int32_t pressed_level = active_low_ ? 0 : 1;
    int32_t released_level = active_low_ ? 1 : 0;
    
    int32_t current_level = gpio_hal_.get_level(pin_);

    last_click_type_ = ButtonClickType::NONE_CLICK;

    switch (state_) {
    case State::WAIT_FOR_PRESS:
        if (current_level == pressed_level) {
            press_start_time_ms_ = now;
            state_ = State::DEBOUNCE_PRESS;
            ESP_LOGD(TAG, "STARTING DEBOUNCE (Pin: %d)", pin_);
        }
        break;

    case State::DEBOUNCE_PRESS:
        if (now - press_start_time_ms_ > config_.debounce_press_ms) {
            if (current_level == pressed_level) {
                state_ = State::WAIT_FOR_RELEASE;
                ESP_LOGD(TAG, "WAIT_FOR_RELEASE (Pin: %d)", pin_);
            } else {
                state_ = State::WAIT_FOR_PRESS;
                ESP_LOGD(TAG, "Premature release, back to WAIT_FOR_PRESS (Pin: %d)", pin_);
            }
        }
        break;

    case State::WAIT_FOR_RELEASE:
        if (current_level == released_level) {
            uint32_t duration = now - press_start_time_ms_;

            if (duration > config_.very_long_click_ms) {
                state_ = State::WAIT_FOR_PRESS;
                last_click_type_ = ButtonClickType::VERY_LONG_CLICK;
            } else if (duration > config_.long_click_ms) {
                state_ = State::WAIT_FOR_PRESS;
                last_click_type_ = ButtonClickType::LONG_CLICK;
            } else {
                last_time_ms_ = now;
                state_ = State::DEBOUNCE_RELEASE;
            }
        } else if (now - press_start_time_ms_ > config_.timeout_ms) {
            state_ = State::TIMEOUT_WAIT_FOR_RELEASE;
            last_time_ms_ = now;
            ESP_LOGD(TAG, "TIMEOUT WAIT FOR RELEASE (Pin: %d)", pin_);
        }
        break;

    case State::DEBOUNCE_RELEASE:
        if (now - last_time_ms_ > config_.debounce_release_ms) {
            if (current_level == released_level) {
                state_ = State::WAIT_FOR_DOUBLE;
                ESP_LOGD(TAG, "DEBOUNCE RELEASED (Pin: %d)", pin_);
            } else {
                state_ = State::WAIT_FOR_DOUBLE;
                ESP_LOGD(TAG, "Pressed during DEBOUNCE_RELEASE (Pin: %d)", pin_);
            }
        }
        break;

    case State::WAIT_FOR_DOUBLE:
        if (current_level == pressed_level && !first_click_) {
            last_time_ms_ = now;
            first_click_ = true;
            state_ = State::DEBOUNCE_PRESS;
            ESP_LOGD(TAG, "Second click detected (Pin: %d)", pin_);
        } else if (now - last_time_ms_ > config_.double_click_ms) {
            state_ = State::WAIT_FOR_PRESS;
            if (first_click_) {
                first_click_ = false;
                last_click_type_ = ButtonClickType::DOUBLE_CLICK;
            } else {
                last_click_type_ = ButtonClickType::CLICK;
            }
        }
        break;

    case State::TIMEOUT_WAIT_FOR_RELEASE:
        if (current_level == released_level) {
            if (now - last_time_ms_ > config_.debounce_release_ms) {
                last_time_ms_ = now;
                state_ = State::WAIT_FOR_PRESS;
                ESP_LOGD(TAG, "TIMEOUT RELEASED (Pin: %d)", pin_);
                last_click_type_ = ButtonClickType::TIMEOUT;
            }
        } else {
            last_time_ms_ = now;
            if (now - press_start_time_ms_ > 2 * config_.timeout_ms) {
                state_ = State::WAIT_FOR_PRESS;
                ESP_LOGD(TAG, "BUTTON ERROR (Pin: %d)", pin_);
                last_click_type_ = ButtonClickType::ERROR_STATE;
            }
        }
        break;
    }
}

ButtonClickType Button::get_last_click() {
    ButtonClickType click = last_click_type_;
    last_click_type_ = ButtonClickType::NONE_CLICK;
    return click;
}

} // namespace ui_inputs
