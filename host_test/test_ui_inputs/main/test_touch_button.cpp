#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "touch_button.hpp"
#include "mock_hal_touch.hpp"
#include "mock_hal_timer.hpp"

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::DoAll;
using ::testing::SetArgPointee;
using ::testing::_;

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

        ON_CALL(mock_touch_, new_controller(_, _))
            .WillByDefault([](const touch_sensor_config_t*, touch_sensor_handle_t* ret_handle) {
                if (ret_handle) *ret_handle = reinterpret_cast<touch_sensor_handle_t>(0x5678);
                return ESP_OK;
            });
        ON_CALL(mock_touch_, config_filter(_, _)).WillByDefault(Return(ESP_OK));
        ON_CALL(mock_touch_, enable(_)).WillByDefault(Return(ESP_OK));
        ON_CALL(mock_touch_, disable(_)).WillByDefault(Return(ESP_OK));
        ON_CALL(mock_touch_, del_controller(_)).WillByDefault(Return(ESP_OK));
        ON_CALL(mock_touch_, new_channel(_, _, _, _))
            .WillByDefault([this](touch_sensor_handle_t, int, const touch_channel_config_t*, touch_channel_handle_t* ret_handle) {
                if (ret_handle) *ret_handle = chan_handle_;
                return ESP_OK;
            });
        ON_CALL(mock_touch_, del_channel(_)).WillByDefault(Return(ESP_OK));
        ON_CALL(mock_touch_, trigger_oneshot_scanning(_, _)).WillByDefault(Return(ESP_OK));
        ON_CALL(mock_touch_, start_continuous_scanning(_)).WillByDefault(Return(ESP_OK));
        ON_CALL(mock_touch_, stop_continuous_scanning(_)).WillByDefault(Return(ESP_OK));
        ON_CALL(mock_touch_, read_channel_data(_, TOUCH_CHAN_DATA_TYPE_SMOOTH, _))
            .WillByDefault(DoAll(SetArgPointee<2>(1000), Return(ESP_OK)));
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

    TouchButton touch(mock_touch_, mock_timer_, 0, config);
    EXPECT_EQ(touch.init(), ESP_OK);
    EXPECT_EQ(touch.get_baseline(), 1000);

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

    EXPECT_EQ(touch.deinit(), ESP_OK);
}

TEST_F(TouchButtonTest, SingleTouchClickBelowBaseline) {
    TouchButtonConfig config;
    config.trigger_mode = TouchTriggerMode::BELOW_BASELINE; // ESP32 HW V1
    config.threshold_delta = 100;
    config.debounce_press_ms = 20;
    config.debounce_release_ms = 20;

    TouchButton touch(mock_touch_, mock_timer_, 0, config);
    EXPECT_EQ(touch.init(), ESP_OK);

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

    EXPECT_EQ(touch.deinit(), ESP_OK);
}

TEST_F(TouchButtonTest, TouchHoldAndHoldRepeat) {
    TouchButtonConfig config;
    config.trigger_mode = TouchTriggerMode::ABOVE_BASELINE;
    config.threshold_delta = 100;
    config.debounce_press_ms = 20;
    config.hold_time_ms = 1000;
    config.hold_repeat_interval_ms = 200;
    config.enable_hold_repeat = true;

    TouchButton touch(mock_touch_, mock_timer_, 0, config);
    EXPECT_EQ(touch.init(), ESP_OK);

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

    EXPECT_EQ(touch.deinit(), ESP_OK);
}

TEST_F(TouchButtonTest, TouchGlitchFilter) {
    TouchButtonConfig config;
    config.debounce_press_ms = 20;

    TouchButton touch(mock_touch_, mock_timer_, 0, config);
    EXPECT_EQ(touch.init(), ESP_OK);

    // Touch
    set_raw_value(1200);
    touch.update(); // DEBOUNCE_PRESS

    // Release before debounce passes (10ms)
    advance_time_ms(10);
    set_raw_value(1000);
    advance_time_ms(15);
    touch.update(); // Glitch filtered -> WAIT_FOR_PRESS

    EXPECT_EQ(touch.get_last_click(), ButtonClickType::NONE_CLICK);

    EXPECT_EQ(touch.deinit(), ESP_OK);
}

TEST_F(TouchButtonTest, PassiveRecalibration) {
    TouchButtonConfig config;
    config.recalibration_interval_ms = 600000; // 10 minutes

    TouchButton touch(mock_touch_, mock_timer_, 0, config);
    EXPECT_EQ(touch.init(), ESP_OK);

    // Initial check
    set_raw_value(1050);
    touch.update(); // Registers initial recalib timestamp

    // Advance 600,001ms (beyond recalibration interval)
    advance_time_ms(600001);
    set_raw_value(1050); // Ambient level shifted to 1050
    touch.update();

    // Baseline should now be recalibrated to 1050
    EXPECT_EQ(touch.get_baseline(), 1050);

    EXPECT_EQ(touch.deinit(), ESP_OK);
}

TEST_F(TouchButtonTest, FallbackToRawDataType) {
    TouchButtonConfig config;
    TouchButton touch(mock_touch_, mock_timer_, 0, config);

    // During init(), SMOOTH fails, so baseline reads from RAW (1000)
    EXPECT_CALL(mock_touch_, read_channel_data(chan_handle_, TOUCH_CHAN_DATA_TYPE_SMOOTH, _))
        .WillRepeatedly(Return(ESP_FAIL));
    EXPECT_CALL(mock_touch_, read_channel_data(chan_handle_, TOUCH_CHAN_DATA_TYPE_RAW, _))
        .WillRepeatedly(DoAll(SetArgPointee<2>(1000), Return(ESP_OK)));

    EXPECT_EQ(touch.init(), ESP_OK);

    // During update(), SMOOTH fails, RAW succeeds with 1200
    EXPECT_CALL(mock_touch_, read_channel_data(chan_handle_, TOUCH_CHAN_DATA_TYPE_RAW, _))
        .WillRepeatedly(DoAll(SetArgPointee<2>(1200), Return(ESP_OK)));

    touch.update(); // DEBOUNCE_PRESS
    advance_time_ms(25);
    touch.update();
    EXPECT_EQ(touch.get_last_click(), ButtonClickType::NONE_CLICK);

    EXPECT_EQ(touch.deinit(), ESP_OK);
}

TEST_F(TouchButtonTest, MultipleInstancesLifecycle) {
    TouchButtonConfig config;
    TouchButton btn1(mock_touch_, mock_timer_, 0, config);
    TouchButton btn2(mock_touch_, mock_timer_, 1, config);

    EXPECT_EQ(btn1.init(), ESP_OK);
    EXPECT_EQ(btn2.init(), ESP_OK);

    EXPECT_EQ(btn1.deinit(), ESP_OK);
    EXPECT_EQ(btn2.deinit(), ESP_OK);
}

} // namespace ui_inputs
