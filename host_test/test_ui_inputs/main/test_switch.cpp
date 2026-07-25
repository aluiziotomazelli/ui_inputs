#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "switch.hpp"
#include "mock_hal_gpio.hpp"
#include "mock_hal_timer.hpp"

using ::testing::NiceMock;
using ::testing::Return;

namespace ui_inputs {

class SwitchTest : public ::testing::Test {
protected:
    NiceMock<idf_hals::MockGpioHAL> mock_gpio_;
    NiceMock<idf_hals::MockTimerHAL> mock_timer_;
    gpio_num_t pin_{GPIO_NUM_12};
    uint64_t current_time_us_{1000000}; // 1000ms

    void SetUp() override {
        ON_CALL(mock_timer_, get_time_us())
            .WillByDefault([this]() { return current_time_us_; });
    }

    void set_level(int level) {
        EXPECT_CALL(mock_gpio_, get_level(pin_)).WillRepeatedly(Return(level));
    }

    void advance_time_ms(uint32_t ms) {
        current_time_us_ += static_cast<uint64_t>(ms) * 1000;
    }
};

TEST_F(SwitchTest, InitialStateDetection) {
    // Active low, initial level = 0 (closed)
    set_level(0);
    Switch sw_closed(mock_gpio_, mock_timer_, pin_, true);
    EXPECT_EQ(sw_closed.init(), ESP_OK);
    EXPECT_EQ(sw_closed.get_state(), SwitchState::CLOSED);

    // Active low, initial level = 1 (open)
    set_level(1);
    Switch sw_open(mock_gpio_, mock_timer_, pin_, true);
    EXPECT_EQ(sw_open.init(), ESP_OK);
    EXPECT_EQ(sw_open.get_state(), SwitchState::OPEN);
}

TEST_F(SwitchTest, TransitionToClosedWithDebounce) {
    SwitchConfig config;
    config.debounce_ms = 20;

    // Start open (level = 1, active_low = true)
    set_level(1);
    Switch sw(mock_gpio_, mock_timer_, pin_, true, config);
    EXPECT_EQ(sw.init(), ESP_OK);
    EXPECT_EQ(sw.get_state(), SwitchState::OPEN);

    // Level changes to closed (0)
    set_level(0);
    sw.update(); // DEBOUNCING
    EXPECT_EQ(sw.get_state(), SwitchState::OPEN); // State not yet confirmed
    EXPECT_EQ(sw.get_last_event(), SwitchEvent::NONE);

    // Advance past debounce time (25ms)
    advance_time_ms(25);
    sw.update(); // State confirmed CLOSED

    EXPECT_EQ(sw.get_state(), SwitchState::CLOSED);
    EXPECT_EQ(sw.get_last_event(), SwitchEvent::CHANGED_TO_CLOSED);
    // Subsequent event read should return NONE
    EXPECT_EQ(sw.get_last_event(), SwitchEvent::NONE);
}

TEST_F(SwitchTest, TransitionToOpenWithDebounce) {
    SwitchConfig config;
    config.debounce_ms = 20;

    // Start closed (level = 0, active_low = true)
    set_level(0);
    Switch sw(mock_gpio_, mock_timer_, pin_, true, config);
    EXPECT_EQ(sw.init(), ESP_OK);
    EXPECT_EQ(sw.get_state(), SwitchState::CLOSED);

    // Level changes to open (1)
    set_level(1);
    sw.update(); // DEBOUNCING
    EXPECT_EQ(sw.get_state(), SwitchState::CLOSED);

    // Advance past debounce time (25ms)
    advance_time_ms(25);
    sw.update(); // State confirmed OPEN

    EXPECT_EQ(sw.get_state(), SwitchState::OPEN);
    EXPECT_EQ(sw.get_last_event(), SwitchEvent::CHANGED_TO_OPEN);
}

TEST_F(SwitchTest, GlitchFiltered) {
    SwitchConfig config;
    config.debounce_ms = 20;

    set_level(1); // Open
    Switch sw(mock_gpio_, mock_timer_, pin_, true, config);
    EXPECT_EQ(sw.init(), ESP_OK);

    // Level changes to 0 (closed)
    set_level(0);
    sw.update(); // DEBOUNCING

    // Level bounces back to 1 before debounce finishes (10ms)
    advance_time_ms(10);
    set_level(1);
    sw.update(); // Bounce detected -> back to STABLE OPEN

    advance_time_ms(20);
    sw.update();
    EXPECT_EQ(sw.get_state(), SwitchState::OPEN);
    EXPECT_EQ(sw.get_last_event(), SwitchEvent::NONE);
}

TEST_F(SwitchTest, InitAndDeinit) {
    set_level(1);
    Switch sw(mock_gpio_, mock_timer_, pin_, true);
    EXPECT_EQ(sw.init(), ESP_OK);
    EXPECT_EQ(sw.deinit(), ESP_OK);
}

} // namespace ui_inputs
