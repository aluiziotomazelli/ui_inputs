#include "touch_button.hpp"

#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"

#if __has_include("driver/touch_sens.h")
#include "driver/touch_sens.h"
#endif

static const char* TAG = "TouchButton";

namespace ui_inputs {

touch_sensor_handle_t TouchButton::s_sens_handle_ = nullptr;
uint8_t TouchButton::s_active_instances_ = 0;

TouchButton::TouchButton(
    idf_hals::ITouchHAL& touch_hal,
    idf_hals::ITimerHAL& timer_hal,
    int channel_id,
    const TouchButtonConfig& config)
    : touch_hal_(touch_hal)
    , timer_hal_(timer_hal)
    , channel_id_(channel_id)
    , chan_handle_(nullptr)
    , config_(config)
    , state_(State::WAIT_FOR_PRESS)
    , baseline_(0)
    , last_time_ms_(0)
    , press_start_time_ms_(0)
    , last_hold_event_ms_(0)
    , last_recalib_time_ms_(0)
    , hold_generated_(false)
    , is_initialized_(false)
    , last_click_type_(ButtonClickType::NONE_CLICK)
{
}

esp_err_t TouchButton::init()
{
    if (is_initialized_) {
        return ESP_OK;
    }

    esp_err_t err = ESP_OK;

    // 1. Allocate global controller lazily on first TouchButton instance
    if (s_sens_handle_ == nullptr) {
        touch_sensor_config_t sens_cfg{};
        touch_sensor_filter_config_t filter_cfg{};
#if __has_include("driver/touch_sens.h")
#if SOC_TOUCH_SENSOR_VERSION == 1
        touch_sensor_sample_config_t sample_cfg[TOUCH_SAMPLE_CFG_NUM] = {
            TOUCH_SENSOR_V1_DEFAULT_SAMPLE_CONFIG(5.0, TOUCH_VOLT_LIM_L_0V5, TOUCH_VOLT_LIM_H_1V7)};
#else
        touch_sensor_sample_config_t sample_cfg[TOUCH_SAMPLE_CFG_NUM] = {
            TOUCH_SENSOR_V2_DEFAULT_SAMPLE_CONFIG(500, TOUCH_VOLT_LIM_L_0V5, TOUCH_VOLT_LIM_H_2V2)};
#endif
        sens_cfg = TOUCH_SENSOR_DEFAULT_BASIC_CONFIG(TOUCH_SAMPLE_CFG_NUM, sample_cfg);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
        filter_cfg = TOUCH_SENSOR_DEFAULT_FILTER_CONFIG();
#pragma GCC diagnostic pop
#endif

        err = touch_hal_.new_controller(&sens_cfg, &s_sens_handle_);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create touch controller: %d", err);
            return err;
        }

        err = touch_hal_.config_filter(s_sens_handle_, &filter_cfg);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to config touch filter: %d", err);
            touch_hal_.del_controller(s_sens_handle_);
            s_sens_handle_ = nullptr;
            return err;
        }
    }
    else {
        // Controller exists; temporarily disable scanning/controller to add new channel
        touch_hal_.stop_continuous_scanning(s_sens_handle_);
        touch_hal_.disable(s_sens_handle_);
    }

    // 2. Allocate channel while controller is disabled (INIT state)
    touch_channel_config_t chan_cfg{};
#if __has_include("driver/touch_sens.h")
#if SOC_TOUCH_SENSOR_VERSION == 1
    chan_cfg = touch_channel_config_t{
        .abs_active_thresh = {1000},
        .charge_speed = TOUCH_CHARGE_SPEED_7,
        .init_charge_volt = TOUCH_INIT_CHARGE_VOLT_DEFAULT,
        .group = TOUCH_CHAN_TRIG_GROUP_BOTH};
#else
    chan_cfg = touch_channel_config_t{
        .active_thresh = {2000},
        .charge_speed = TOUCH_CHARGE_SPEED_7,
        .init_charge_volt = TOUCH_INIT_CHARGE_VOLT_DEFAULT};
#endif
#endif

    err = touch_hal_.new_channel(s_sens_handle_, channel_id_, &chan_cfg, &chan_handle_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create channel %d: %d", channel_id_, err);
        return err;
    }

    // 3. Enable controller now that channel is allocated
    err = touch_hal_.enable(s_sens_handle_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable touch controller: %d", err);
        return err;
    }

    // 4. Warm-up scans to populate channel filters before reading baseline
    for (int i = 0; i < 3; i++) {
        touch_hal_.trigger_oneshot_scanning(s_sens_handle_, 2000);
    }

    // Read initial baseline
    if (touch_hal_.read_channel_data(chan_handle_, TOUCH_CHAN_DATA_TYPE_SMOOTH, &baseline_) != ESP_OK) {
        touch_hal_.read_channel_data(chan_handle_, TOUCH_CHAN_DATA_TYPE_RAW, &baseline_);
    }

    // 5. Start continuous scanning
    touch_hal_.start_continuous_scanning(s_sens_handle_);

    s_active_instances_++;
    is_initialized_ = true;
    ESP_LOGI(TAG, "TouchButton channel %d initialized. Baseline: %" PRIu32, channel_id_, baseline_);
    return ESP_OK;
}

esp_err_t TouchButton::deinit()
{
    if (!is_initialized_) {
        return ESP_OK;
    }

    if (chan_handle_) {
        touch_hal_.del_channel(chan_handle_);
        chan_handle_ = nullptr;
    }

    if (s_active_instances_ > 0) {
        s_active_instances_--;
        if (s_active_instances_ == 0 && s_sens_handle_) {
            touch_hal_.stop_continuous_scanning(s_sens_handle_);
            touch_hal_.disable(s_sens_handle_);
            touch_hal_.del_controller(s_sens_handle_);
            s_sens_handle_ = nullptr;
            ESP_LOGI(TAG, "Global Touch Controller deallocated.");
        }
    }

    is_initialized_ = false;
    return ESP_OK;
}

bool TouchButton::is_touched(uint32_t raw_val) const
{
    if (config_.trigger_mode == TouchTriggerMode::ABOVE_BASELINE) {
        return raw_val > (baseline_ + config_.threshold_delta);
    }
    else {
        return raw_val < (baseline_ > config_.threshold_delta ? baseline_ - config_.threshold_delta : 0);
    }
}

void TouchButton::update()
{
    if (!is_initialized_) {
        return;
    }

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
        }
        else {
            if (last_recalib_time_ms_ == 0) {
                last_recalib_time_ms_ = now;
            }
            else if (now - last_recalib_time_ms_ >= config_.recalibration_interval_ms) {
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
            }
            else {
                state_ = State::WAIT_FOR_PRESS;
                ESP_LOGD(TAG, "Touch glitch filtered, back to WAIT_FOR_PRESS");
            }
        }
        break;

    case State::WAIT_FOR_RELEASE_OR_HOLD:
        if (!touched) {
            last_time_ms_ = now;
            state_ = State::DEBOUNCE_RELEASE;
        }
        else if (now - press_start_time_ms_ > config_.hold_time_ms) {
            if (!hold_generated_) {
                hold_generated_ = true;
                last_hold_event_ms_ = now;
                last_click_type_ = ButtonClickType::LONG_CLICK;
                ESP_LOGD(TAG, "TOUCH LONG_CLICK (HOLD)");
            }
            else if (config_.enable_hold_repeat && (now - last_hold_event_ms_ >= config_.hold_repeat_interval_ms)) {
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

ButtonClickType TouchButton::get_last_click()
{
    ButtonClickType click = last_click_type_;
    last_click_type_ = ButtonClickType::NONE_CLICK;
    return click;
}

} // namespace ui_inputs
