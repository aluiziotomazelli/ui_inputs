#include "rotary_encoder.hpp"

#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"

static const char* TAG = "RotaryEncoder";

namespace ui_inputs {

RotaryEncoder::RotaryEncoder(
    idf_hals::IPcntHAL& pcnt_hal,
    idf_hals::ITimerHAL& timer_hal,
    gpio_num_t pin_a,
    gpio_num_t pin_b,
    const RotaryEncoderConfig& config)
    : pcnt_hal_(pcnt_hal)
    , timer_hal_(timer_hal)
    , pin_a_(pin_a)
    , pin_b_(pin_b)
    , config_(config)
    , pcnt_unit_(nullptr)
    , pcnt_chan_a_(nullptr)
    , pcnt_chan_b_(nullptr)
    , last_step_time_ms_(0)
    , accumulated_pulses_(0)
    , accumulated_steps_(0)
    , is_initialized_(false)
{
}

esp_err_t RotaryEncoder::init()
{
    if (is_initialized_) {
        return ESP_OK;
    }

    esp_err_t err = ESP_OK;

    // 1. Create PCNT Unit
    pcnt_unit_config_t unit_cfg = {};
    unit_cfg.low_limit = -30000;
    unit_cfg.high_limit = 30000;
    err = pcnt_hal_.new_unit(&unit_cfg, &pcnt_unit_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create PCNT unit: %d", err);
        return err;
    }

    // 2. Set Glitch Filter
    if (config_.glitch_filter_ns > 0) {
        pcnt_glitch_filter_config_t filter_cfg = {};
        filter_cfg.max_glitch_ns = config_.glitch_filter_ns;
        err = pcnt_hal_.unit_set_glitch_filter(pcnt_unit_, &filter_cfg);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set PCNT glitch filter: %d", err);
            pcnt_hal_.del_unit(pcnt_unit_);
            pcnt_unit_ = nullptr;
            return err;
        }
    }

    // 3. Create Channel A (Edge=PinA, Level=PinB)
    pcnt_chan_config_t chan_a_cfg = {};
    chan_a_cfg.edge_gpio_num = pin_a_;
    chan_a_cfg.level_gpio_num = pin_b_;
    err = pcnt_hal_.new_channel(pcnt_unit_, &chan_a_cfg, &pcnt_chan_a_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create PCNT channel A: %d", err);
        pcnt_hal_.del_unit(pcnt_unit_);
        pcnt_unit_ = nullptr;
        return err;
    }

    // 4. Create Channel B (Edge=PinB, Level=PinA)
    pcnt_chan_config_t chan_b_cfg = {};
    chan_b_cfg.edge_gpio_num = pin_b_;
    chan_b_cfg.level_gpio_num = pin_a_;
    err = pcnt_hal_.new_channel(pcnt_unit_, &chan_b_cfg, &pcnt_chan_b_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create PCNT channel B: %d", err);
        pcnt_hal_.del_channel(pcnt_chan_a_);
        pcnt_hal_.del_unit(pcnt_unit_);
        pcnt_chan_a_ = nullptr;
        pcnt_unit_ = nullptr;
        return err;
    }

    // 5. Configure Quadrature 4x decoding actions
    pcnt_hal_.channel_set_edge_action(pcnt_chan_a_, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
    pcnt_hal_.channel_set_level_action(pcnt_chan_a_, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

    pcnt_hal_.channel_set_edge_action(pcnt_chan_b_, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE);
    pcnt_hal_.channel_set_level_action(pcnt_chan_b_, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

    // 6. Enable and Start Unit
    err = pcnt_hal_.unit_enable(pcnt_unit_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable PCNT unit: %d", err);
        return err;
    }

    err = pcnt_hal_.unit_clear_count(pcnt_unit_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to clear PCNT unit count: %d", err);
        return err;
    }

    err = pcnt_hal_.unit_start(pcnt_unit_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start PCNT unit: %d", err);
        return err;
    }

    is_initialized_ = true;
    ESP_LOGI(TAG, "RotaryEncoder PCNT initialized on pins A:%d, B:%d", pin_a_, pin_b_);
    return ESP_OK;
}

esp_err_t RotaryEncoder::deinit()
{
    if (!is_initialized_) {
        return ESP_OK;
    }

    pcnt_hal_.unit_stop(pcnt_unit_);
    pcnt_hal_.unit_disable(pcnt_unit_);
    if (pcnt_chan_a_) {
        pcnt_hal_.del_channel(pcnt_chan_a_);
        pcnt_chan_a_ = nullptr;
    }
    if (pcnt_chan_b_) {
        pcnt_hal_.del_channel(pcnt_chan_b_);
        pcnt_chan_b_ = nullptr;
    }
    if (pcnt_unit_) {
        pcnt_hal_.del_unit(pcnt_unit_);
        pcnt_unit_ = nullptr;
    }

    is_initialized_ = false;
    return ESP_OK;
}

int32_t RotaryEncoder::map_value(int32_t x, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max)
{
    if (in_min == in_max)
        return out_min;
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void RotaryEncoder::update()
{
    if (!is_initialized_) {
        ESP_LOGE(TAG, "Rotary encoder not initialized");
        return;
    }

    int32_t count = 0;
    if (pcnt_hal_.unit_get_count(pcnt_unit_, &count) != ESP_OK) {
        return;
    }

    if (count != 0) {
        pcnt_hal_.unit_clear_count(pcnt_unit_);
        accumulated_pulses_ += count;
    }

    int32_t divider = config_.half_step_mode ? 2 : 4;
    int32_t raw_steps = accumulated_pulses_ / divider;

    if (raw_steps != 0) {
        accumulated_pulses_ -= raw_steps * divider;

        int32_t current_multiplier_val = 1;

        if (config_.acceleration_enabled) {
            uint32_t current_time_ms = timer_hal_.get_time_us() / 1000;
            uint32_t turn_interval_ms = current_time_ms - last_step_time_ms_;

            if (turn_interval_ms < config_.accel_gap_ms && last_step_time_ms_ != 0) {
                current_multiplier_val = map_value(
                    config_.accel_gap_ms - turn_interval_ms,
                    1,
                    config_.accel_gap_ms,
                    1,
                    config_.accel_max_multiplier + 1);

                if (current_multiplier_val < 1)
                    current_multiplier_val = 1;
                if (current_multiplier_val > config_.accel_max_multiplier)
                    current_multiplier_val = config_.accel_max_multiplier;
            }
            last_step_time_ms_ = current_time_ms;
        }
        else {
            last_step_time_ms_ = timer_hal_.get_time_us() / 1000;
        }

        raw_steps *= current_multiplier_val;
        accumulated_steps_ += raw_steps;
    }
}

int32_t RotaryEncoder::get_steps()
{
    int32_t steps = accumulated_steps_;
    accumulated_steps_ = 0;
    return steps;
}

} // namespace ui_inputs
