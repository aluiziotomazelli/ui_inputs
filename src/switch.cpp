#include "switch.hpp"

#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"

static const char *TAG = "Switch";

namespace ui_inputs {

Switch::Switch(idf_hals::IGpioHAL& gpio_hal,
               idf_hals::ITimerHAL& timer_hal,
               gpio_num_t pin,
               bool active_low,
               const SwitchConfig& config)
    : gpio_hal_(gpio_hal),
      timer_hal_(timer_hal),
      pin_(pin),
      active_low_(active_low),
      config_(config),
      fsm_state_(FsmState::STABLE),
      current_state_(SwitchState::OPEN),
      pending_state_(SwitchState::OPEN),
      state_change_time_ms_(0),
      last_event_(SwitchEvent::NONE) {
    
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

    int32_t level = gpio_hal_.get_level(pin_);
    int32_t closed_level = active_low_ ? 0 : 1;
    current_state_ = (level == closed_level) ? SwitchState::CLOSED : SwitchState::OPEN;
    pending_state_ = current_state_;
}

void Switch::update() {
    uint32_t now = timer_hal_.get_time_us() / 1000;
    int32_t current_level = gpio_hal_.get_level(pin_);
    int32_t closed_level = active_low_ ? 0 : 1;

    SwitchState sampled_state = (current_level == closed_level) ? SwitchState::CLOSED : SwitchState::OPEN;
    last_event_ = SwitchEvent::NONE;

    switch (fsm_state_) {
    case FsmState::STABLE:
        if (sampled_state != current_state_) {
            pending_state_ = sampled_state;
            state_change_time_ms_ = now;
            fsm_state_ = FsmState::DEBOUNCING;
            ESP_LOGD(TAG, "Switch state change detected (Pin: %d), starting debounce", pin_);
        }
        break;

    case FsmState::DEBOUNCING:
        if (sampled_state != pending_state_) {
            fsm_state_ = FsmState::STABLE;
            ESP_LOGD(TAG, "Switch state bounced back (Pin: %d), cancelling debounce", pin_);
        } else if (now - state_change_time_ms_ > config_.debounce_ms) {
            current_state_ = pending_state_;
            fsm_state_ = FsmState::STABLE;
            last_event_ = (current_state_ == SwitchState::CLOSED) ? 
                          SwitchEvent::CHANGED_TO_CLOSED : SwitchEvent::CHANGED_TO_OPEN;
            ESP_LOGD(TAG, "Switch state confirmed: %s (Pin: %d)",
                     (current_state_ == SwitchState::CLOSED) ? "CLOSED" : "OPEN", pin_);
        }
        break;
    }
}

SwitchState Switch::get_state() const {
    return current_state_;
}

SwitchEvent Switch::get_last_event() {
    SwitchEvent evt = last_event_;
    last_event_ = SwitchEvent::NONE;
    return evt;
}

} // namespace ui_inputs
