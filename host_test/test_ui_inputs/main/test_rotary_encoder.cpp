#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "rotary_encoder.hpp"
#include "mock_hal_gpio.hpp"
#include "mock_hal_timer.hpp"

using ::testing::NiceMock;
using ::testing::Return;

namespace ui_inputs {

class RotaryEncoderTest : public ::testing::Test {
protected:
    NiceMock<idf_hals::MockGpioHAL> mock_gpio_;
    NiceMock<idf_hals::MockTimerHAL> mock_timer_;
    gpio_num_t pin_a_{GPIO_NUM_5};
    gpio_num_t pin_b_{GPIO_NUM_6};
    uint64_t current_time_us_{1000000}; // 1000ms

    void SetUp() override {
        ON_CALL(mock_timer_, get_time_us())
            .WillByDefault([this]() { return current_time_us_; });
    }

    void set_pins(int level_a, int level_b) {
        EXPECT_CALL(mock_gpio_, get_level(pin_a_)).WillRepeatedly(Return(level_a));
        EXPECT_CALL(mock_gpio_, get_level(pin_b_)).WillRepeatedly(Return(level_b));
    }

    void advance_time_ms(uint32_t ms) {
        current_time_us_ += static_cast<uint64_t>(ms) * 1000;
    }
};

TEST_F(RotaryEncoderTest, ClockwiseFullStep) {
    RotaryEncoderConfig config;
    config.half_step_mode = false;
    config.acceleration_enabled = false;

    RotaryEncoder encoder(mock_gpio_, mock_timer_, pin_a_, pin_b_, config);

    // Initial state (0, 0)
    set_pins(0, 0);
    encoder.update();
    EXPECT_EQ(encoder.get_steps(), 0);

    // CW sequence: (0,0) -> (0,1) -> (1,1) -> (1,0) -> (0,0)
    set_pins(0, 1);
    encoder.update();

    set_pins(1, 1);
    encoder.update();

    set_pins(1, 0);
    encoder.update();

    set_pins(0, 0);
    encoder.update();

    EXPECT_EQ(encoder.get_steps(), 1);
}

TEST_F(RotaryEncoderTest, CounterClockwiseFullStep) {
    RotaryEncoderConfig config;
    config.half_step_mode = false;
    config.acceleration_enabled = false;

    RotaryEncoder encoder(mock_gpio_, mock_timer_, pin_a_, pin_b_, config);

    // Initial state (0, 0)
    set_pins(0, 0);
    encoder.update();
    EXPECT_EQ(encoder.get_steps(), 0);

    // CCW sequence: (0,0) -> (1,0) -> (1,1) -> (0,1) -> (0,0)
    set_pins(1, 0);
    encoder.update();

    set_pins(1, 1);
    encoder.update();

    set_pins(0, 1);
    encoder.update();

    set_pins(0, 0);
    encoder.update();

    EXPECT_EQ(encoder.get_steps(), -1);
}

TEST_F(RotaryEncoderTest, HalfStepModeCW) {
    RotaryEncoderConfig config;
    config.half_step_mode = true;
    config.acceleration_enabled = false;

    RotaryEncoder encoder(mock_gpio_, mock_timer_, pin_a_, pin_b_, config);

    // Half step produces steps on intermediate state changes
    set_pins(0, 0);
    encoder.update();

    set_pins(0, 1);
    encoder.update();

    set_pins(1, 1);
    encoder.update();

    // Steps should be detected before full 4-state cycle completes
    EXPECT_GT(encoder.get_steps(), 0);
}

TEST_F(RotaryEncoderTest, HalfStepModeCCW) {
    RotaryEncoderConfig config;
    config.half_step_mode = true;
    config.acceleration_enabled = false;

    RotaryEncoder encoder(mock_gpio_, mock_timer_, pin_a_, pin_b_, config);

    set_pins(0, 0);
    encoder.update();

    set_pins(1, 0);
    encoder.update();

    set_pins(1, 1);
    encoder.update();

    EXPECT_LT(encoder.get_steps(), 0);
}

TEST_F(RotaryEncoderTest, StepAcceleration) {
    RotaryEncoderConfig config;
    config.half_step_mode = false;
    config.acceleration_enabled = true;
    config.accel_gap_ms = 50;
    config.accel_max_multiplier = 5;

    RotaryEncoder encoder(mock_gpio_, mock_timer_, pin_a_, pin_b_, config);

    // First full step CW
    set_pins(0, 0); encoder.update();
    set_pins(0, 1); encoder.update();
    set_pins(1, 1); encoder.update();
    set_pins(1, 0); encoder.update();
    set_pins(0, 0); encoder.update();
    int32_t first_steps = encoder.get_steps();
    EXPECT_EQ(first_steps, 1);

    // Second full step CW very quickly (10ms later)
    advance_time_ms(10);
    set_pins(0, 1); encoder.update();
    set_pins(1, 1); encoder.update();
    set_pins(1, 0); encoder.update();
    set_pins(0, 0); encoder.update();
    int32_t accel_steps = encoder.get_steps();
    
    // Multiplied step count should be > 1
    EXPECT_GT(accel_steps, 1);
    EXPECT_LE(accel_steps, config.accel_max_multiplier);
}

TEST_F(RotaryEncoderTest, AccelerationDisabled) {
    RotaryEncoderConfig config;
    config.half_step_mode = false;
    config.acceleration_enabled = false;

    RotaryEncoder encoder(mock_gpio_, mock_timer_, pin_a_, pin_b_, config);

    // First full step CW
    set_pins(0, 0); encoder.update();
    set_pins(0, 1); encoder.update();
    set_pins(1, 1); encoder.update();
    set_pins(1, 0); encoder.update();
    set_pins(0, 0); encoder.update();
    EXPECT_EQ(encoder.get_steps(), 1);

    // Second full step CW rapidly (10ms later)
    advance_time_ms(10);
    set_pins(0, 1); encoder.update();
    set_pins(1, 1); encoder.update();
    set_pins(1, 0); encoder.update();
    set_pins(0, 0); encoder.update();

    // Steps should remain 1 when acceleration is disabled
    EXPECT_EQ(encoder.get_steps(), 1);
}

TEST_F(RotaryEncoderTest, GetStepsClearsAccumulator) {
    RotaryEncoderConfig config;
    config.half_step_mode = false;
    config.acceleration_enabled = false;

    RotaryEncoder encoder(mock_gpio_, mock_timer_, pin_a_, pin_b_, config);

    set_pins(0, 0); encoder.update();
    set_pins(0, 1); encoder.update();
    set_pins(1, 1); encoder.update();
    set_pins(1, 0); encoder.update();
    set_pins(0, 0); encoder.update();

    EXPECT_EQ(encoder.get_steps(), 1);
    // Second call should return 0
    EXPECT_EQ(encoder.get_steps(), 0);
}

} // namespace ui_inputs
