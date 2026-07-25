#pragma once

#include <cstdint>
#include "interfaces/i_button.hpp"
#include "interfaces/i_hal_touch.hpp"
#include "interfaces/i_hal_timer.hpp"

namespace ui_inputs {

/**
 * @brief Specifies how touch detection is evaluated relative to the baseline capacitance.
 * 
 * @note Hardware Version Differences:
 * - ESP32 (Hardware V1): Capacitance increase causes raw counter reading to DECREASE.
 *   Use @c TouchTriggerMode::BELOW_BASELINE.
 * - ESP32-S2 / ESP32-S3 (Hardware V2): Capacitance increase causes raw reading to INCREASE.
 *   Use @c TouchTriggerMode::ABOVE_BASELINE.
 */
enum class TouchTriggerMode {
    ABOVE_BASELINE,  ///< Active state when reading exceeds baseline + threshold (ESP32-S3 / HW V2)
    BELOW_BASELINE   ///< Active state when reading falls below baseline - threshold (ESP32 / HW V1)
};

/**
 * @brief Configuration parameters for TouchButton component.
 */
struct TouchButtonConfig {
    TouchTriggerMode trigger_mode{TouchTriggerMode::ABOVE_BASELINE}; ///< Trigger mode (Default: ABOVE_BASELINE for ESP32-S3)
    uint32_t debounce_press_ms{20};          ///< Press debounce time in milliseconds
    uint32_t debounce_release_ms{20};        ///< Release debounce time in milliseconds
    uint32_t hold_time_ms{1000};             ///< Time in milliseconds to trigger initial hold (LONG_CLICK)
    uint32_t hold_repeat_interval_ms{200};   ///< Interval in milliseconds between repeated hold events (HOLD_REPEAT)
    bool enable_hold_repeat{true};           ///< Enable periodic HOLD_REPEAT events while held down
    uint32_t threshold_delta{100};           ///< Raw delta from baseline required to register touch
    uint32_t recalibration_interval_ms{600000}; ///< Recalibration interval in ms (Default: 10 minutes)
};

/**
 * @brief TouchButton implementation using FSM and HAL interfaces
 */
class TouchButton : public IButton {
public:
    TouchButton(idf_hals::ITouchHAL& touch_hal,
                idf_hals::ITimerHAL& timer_hal,
                int channel_id,
                const TouchButtonConfig& config = TouchButtonConfig{});

    esp_err_t init() override;
    esp_err_t deinit() override;
    void update() override;
    ButtonClickType get_last_click() override;

    /**
     * @brief Gets current baseline capacitance reading.
     * @return Baseline value.
     */
    uint32_t get_baseline() const { return baseline_; }

private:
    bool is_touched(uint32_t raw_val) const;

    enum class State {
        WAIT_FOR_PRESS,
        DEBOUNCE_PRESS,
        WAIT_FOR_RELEASE_OR_HOLD,
        DEBOUNCE_RELEASE
    };

    static touch_sensor_handle_t s_sens_handle_;
    static uint8_t s_active_instances_;

    idf_hals::ITouchHAL& touch_hal_;
    idf_hals::ITimerHAL& timer_hal_;
    int channel_id_;
    touch_channel_handle_t chan_handle_;
    TouchButtonConfig config_;

    State state_;
    uint32_t baseline_;
    uint32_t last_time_ms_;
    uint32_t press_start_time_ms_;
    uint32_t last_hold_event_ms_;
    uint32_t last_recalib_time_ms_;
    bool hold_generated_;
    bool is_initialized_;

    ButtonClickType last_click_type_;
};

} // namespace ui_inputs
