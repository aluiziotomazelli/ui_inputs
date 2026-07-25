#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "touch_button.hpp"
#include "mock_hal_touch.hpp"
#include "mock_hal_timer.hpp"

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::_;
using ::testing::SetArgPointee;

namespace ui_inputs {

class TouchButtonTest : public ::testing::Test {
protected:
    NiceMock<idf_hals::MockTouchHAL> mock_touch_;
    NiceMock<idf_hals::MockTimerHAL> mock_timer_;
    touch_channel_handle_t chan_handle_{reinterpret_cast<touch_channel_handle_t>(0x1234)};
    uint64_t current_time_us_{1000000}; // 1000ms

    void SetUp() override {
        ON_CALL(mock_timer_, get_time_us())
            .WillByDefault([this]() { return current_time_us_; });
    }

    void set_raw_value(uint32_t raw_val) {
        EXPECT_CALL(mock_touch_, read_channel_data(chan_handle_, TOUCH_CHAN_DATA_TYPE_SMOOTH, _))
            .WillRepeatedly(DoAll(SetArgPointee<2>(raw_val), Return(ESP_OK)));
    }

    void advance_time_ms(uint32_t ms) {
        current_time_us_ += static_cast<uint64_t>(ms) * 1000;
    }
};

TEST_F(TouchButtonTest, SingleTouchClickAboveBaseline) {
    TouchButtonConfig config;
    config.trigger_mode = TouchTriggerMode::ABOVE_BASELINE; // ESP32-S3 HW V2
    config.threshold_delta = 100;
    config.debounce_press_ms = 20;
    config.debounce_release_ms = 20;

    uint32_t baseline = 1000;
    TouchButton touch(mock_touch_, mock_timer_, chan_handle_, baseline, config);

    // Initial state: untouched (1000)
    set_raw_value(1000);
    touch.update();
    EXPECT_EQ(touch.get_last_click(), ButtonClickType::NONE_CLICK);

    // 1. Touch detected: raw_val = 1200 (> 1000 + 100)
    set_raw_value(1200);
    touch.update(); // DEBOUNCE_PRESS
    EXPECT_EQ(touch.get_last_click(), ButtonClickType::NONE_CLICK);

    // 2. Advance past press debounce (25ms)
    advance_time_ms(25);
    touch.update(); // WAIT_FOR_RELEASE_OR_HOLD
    EXPECT_EQ(touch.get_last_click(), ButtonClickType::NONE_CLICK);

    // 3. Release touch: raw_val = 1000
    set_raw_value(1000);
    touch.update(); // DEBOUNCE_RELEASE
    EXPECT_EQ(touch.get_last_click(), ButtonClickType::NONE_CLICK);

    // 4. Advance past release debounce (25ms)
    advance_time_ms(25);
    touch.update(); // CLICK
    EXPECT_EQ(touch.get_last_click(), ButtonClickType::CLICK);
}

TEST_F(TouchButtonTest, SingleTouchClickBelowBaseline) {
    TouchButtonConfig config;
    config.trigger_mode = TouchTriggerMode::BELOW_BASELINE; // ESP32 HW V1
    config.threshold_delta = 100;
    config.debounce_press_ms = 20;
    config.debounce_release_ms = 20;

    uint32_t baseline = 1000;
    TouchButton touch(mock_touch_, mock_timer_, chan_handle_, baseline, config);

    // Initial state: untouched (1000)
    set_raw_value(1000);
    touch.update();

    // Touch detected: raw_val = 800 (< 1000 - 100)
    set_raw_value(800);
    touch.update();
    advance_time_ms(25);
    touch.update(); // WAIT_FOR_RELEASE_OR_HOLD

    // Release: raw_val = 1000
    set_raw_value(1000);
    touch.update();
    advance_time_ms(25);
    touch.update();
    EXPECT_EQ(touch.get_last_click(), ButtonClickType::CLICK);
}

TEST_F(TouchButtonTest, TouchHoldAndHoldRepeat) {
    TouchButtonConfig config;
    config.trigger_mode = TouchTriggerMode::ABOVE_BASELINE;
    config.threshold_delta = 100;
    config.debounce_press_ms = 20;
    config.hold_time_ms = 1000;
    config.hold_repeat_interval_ms = 200;
    config.enable_hold_repeat = true;

    TouchButton touch(mock_touch_, mock_timer_, chan_handle_, 1000, config);

    // Touch
    set_raw_value(1200);
    touch.update();
    advance_time_ms(25);
    touch.update(); // WAIT_FOR_RELEASE_OR_HOLD

    // Hold past hold_time_ms (1100ms)
    advance_time_ms(1100);
    touch.update();
    EXPECT_EQ(touch.get_last_click(), ButtonClickType::LONG_CLICK);

    // Continue holding past hold_repeat_interval_ms (250ms)
    advance_time_ms(250);
    touch.update();
    EXPECT_EQ(touch.get_last_click(), ButtonClickType::HOLD_REPEAT);

    // Continue holding past another interval (250ms)
    advance_time_ms(250);
    touch.update();
    EXPECT_EQ(touch.get_last_click(), ButtonClickType::HOLD_REPEAT);

    // Release
    set_raw_value(1000);
    touch.update();
    advance_time_ms(25);
    touch.update();
    // After hold, release should not generate a CLICK
    EXPECT_EQ(touch.get_last_click(), ButtonClickType::NONE_CLICK);
}

TEST_F(TouchButtonTest, TouchGlitchFilter) {
    TouchButtonConfig config;
    config.debounce_press_ms = 20;

    TouchButton touch(mock_touch_, mock_timer_, chan_handle_, 1000, config);

    // Touch
    set_raw_value(1200);
    touch.update(); // DEBOUNCE_PRESS

    // Release before debounce passes (10ms)
    advance_time_ms(10);
    set_raw_value(1000);
    advance_time_ms(15);
    touch.update(); // Glitch filtered -> WAIT_FOR_PRESS

    EXPECT_EQ(touch.get_last_click(), ButtonClickType::NONE_CLICK);
}

TEST_F(TouchButtonTest, PassiveRecalibration) {
    TouchButtonConfig config;
    config.recalibration_interval_ms = 600000; // 10 minutes

    uint32_t initial_baseline = 1000;
    TouchButton touch(mock_touch_, mock_timer_, chan_handle_, initial_baseline, config);

    // Initial check
    set_raw_value(1050);
    touch.update(); // Registers initial recalib timestamp

    // Advance 600,001ms (beyond recalibration interval)
    advance_time_ms(600001);
    set_raw_value(1050); // Ambient level shifted to 1050
    touch.update();

    // Baseline should now be recalibrated to 1050
    EXPECT_EQ(touch.get_baseline(), 1050);
}

} // namespace ui_inputs
