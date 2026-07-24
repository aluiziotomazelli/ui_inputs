#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "button.hpp"
#include "mock_hal_gpio.hpp"
#include "mock_hal_timer.hpp"

using ::testing::NiceMock;
using ::testing::Return;

namespace ui_inputs {

class ButtonTest : public ::testing::Test {
protected:
    NiceMock<idf_hals::MockGpioHAL> mock_gpio_;
    NiceMock<idf_hals::MockTimerHAL> mock_timer_;
    gpio_num_t pin_{GPIO_NUM_4};
    uint64_t current_time_us_{1000000}; // Start at 1000ms

    void SetUp() override {
        ON_CALL(mock_timer_, get_time_us())
            .WillByDefault([this]() { return current_time_us_; });
    }

    void advance_time_ms(uint32_t ms) {
        current_time_us_ += static_cast<uint64_t>(ms) * 1000;
    }
};

TEST_F(ButtonTest, SingleClickDetected) {
    ButtonConfig config;
    config.debounce_press_ms = 20;
    config.debounce_release_ms = 20;
    config.double_click_ms = 300;
    config.long_click_ms = 1000;
    config.very_long_click_ms = 3000;

    Button button(mock_gpio_, mock_timer_, pin_, true, config); // active_low = true

    // Initial state: released (level = 1)
    EXPECT_CALL(mock_gpio_, get_level(pin_)).WillRepeatedly(Return(1));
    button.update();
    EXPECT_EQ(button.get_last_click(), ButtonClickType::NONE_CLICK);

    // 1. Press button (level = 0)
    EXPECT_CALL(mock_gpio_, get_level(pin_)).WillRepeatedly(Return(0));
    button.update(); // Enters DEBOUNCE_PRESS
    EXPECT_EQ(button.get_last_click(), ButtonClickType::NONE_CLICK);

    // 2. Advance past press debounce (25ms)
    advance_time_ms(25);
    button.update(); // Enters WAIT_FOR_RELEASE
    EXPECT_EQ(button.get_last_click(), ButtonClickType::NONE_CLICK);

    // 3. Release button (level = 1)
    EXPECT_CALL(mock_gpio_, get_level(pin_)).WillRepeatedly(Return(1));
    button.update(); // Enters DEBOUNCE_RELEASE
    EXPECT_EQ(button.get_last_click(), ButtonClickType::NONE_CLICK);

    // 4. Advance past release debounce (25ms)
    advance_time_ms(25);
    button.update(); // Enters WAIT_FOR_DOUBLE
    EXPECT_EQ(button.get_last_click(), ButtonClickType::NONE_CLICK);

    // 5. Advance past double click window (350ms)
    advance_time_ms(350);
    button.update(); // Returns SINGLE_CLICK
    EXPECT_EQ(button.get_last_click(), ButtonClickType::CLICK);
}

TEST_F(ButtonTest, DoubleClickDetected) {
    ButtonConfig config;
    config.debounce_press_ms = 20;
    config.debounce_release_ms = 20;
    config.double_click_ms = 300;

    Button button(mock_gpio_, mock_timer_, pin_, true, config);

    // First click
    EXPECT_CALL(mock_gpio_, get_level(pin_)).WillRepeatedly(Return(0));
    button.update();
    advance_time_ms(25);
    button.update();

    EXPECT_CALL(mock_gpio_, get_level(pin_)).WillRepeatedly(Return(1));
    button.update();
    advance_time_ms(25);
    button.update(); // In WAIT_FOR_DOUBLE state

    // Second click before double_click_ms window expires
    advance_time_ms(50);
    EXPECT_CALL(mock_gpio_, get_level(pin_)).WillRepeatedly(Return(0));
    button.update(); // Second press detected

    advance_time_ms(25);
    button.update(); // In WAIT_FOR_RELEASE

    EXPECT_CALL(mock_gpio_, get_level(pin_)).WillRepeatedly(Return(1));
    button.update(); // DEBOUNCE_RELEASE
    advance_time_ms(25);
    button.update(); // WAIT_FOR_DOUBLE

    // Wait past double click timeout
    advance_time_ms(350);
    button.update();
    EXPECT_EQ(button.get_last_click(), ButtonClickType::DOUBLE_CLICK);
}

TEST_F(ButtonTest, LongClickDetected) {
    ButtonConfig config;
    config.debounce_press_ms = 20;
    config.long_click_ms = 1000;
    config.very_long_click_ms = 3000;

    Button button(mock_gpio_, mock_timer_, pin_, true, config);

    // Press
    EXPECT_CALL(mock_gpio_, get_level(pin_)).WillRepeatedly(Return(0));
    button.update();
    advance_time_ms(25);
    button.update(); // WAIT_FOR_RELEASE

    // Hold for 1500ms (> 1000ms long_click_ms, < 3000ms very_long_click_ms)
    advance_time_ms(1500);

    // Release
    EXPECT_CALL(mock_gpio_, get_level(pin_)).WillRepeatedly(Return(1));
    button.update();
    EXPECT_EQ(button.get_last_click(), ButtonClickType::LONG_CLICK);
}

TEST_F(ButtonTest, VeryLongClickDetected) {
    ButtonConfig config;
    config.debounce_press_ms = 20;
    config.long_click_ms = 1000;
    config.very_long_click_ms = 3000;

    Button button(mock_gpio_, mock_timer_, pin_, true, config);

    // Press
    EXPECT_CALL(mock_gpio_, get_level(pin_)).WillRepeatedly(Return(0));
    button.update();
    advance_time_ms(25);
    button.update(); // WAIT_FOR_RELEASE

    // Hold for 3500ms (> 3000ms very_long_click_ms)
    advance_time_ms(3500);

    // Release
    EXPECT_CALL(mock_gpio_, get_level(pin_)).WillRepeatedly(Return(1));
    button.update();
    EXPECT_EQ(button.get_last_click(), ButtonClickType::VERY_LONG_CLICK);
}

TEST_F(ButtonTest, PressGlitchFiltered) {
    ButtonConfig config;
    config.debounce_press_ms = 20;

    Button button(mock_gpio_, mock_timer_, pin_, true, config);

    // Press
    EXPECT_CALL(mock_gpio_, get_level(pin_)).WillRepeatedly(Return(0));
    button.update(); // DEBOUNCE_PRESS

    // Release before debounce finishes (10ms)
    advance_time_ms(10);
    EXPECT_CALL(mock_gpio_, get_level(pin_)).WillRepeatedly(Return(1));

    advance_time_ms(15); // Total 25ms passed
    button.update(); // Checks level, finds it released -> back to WAIT_FOR_PRESS

    EXPECT_EQ(button.get_last_click(), ButtonClickType::NONE_CLICK);
}

} // namespace ui_inputs
