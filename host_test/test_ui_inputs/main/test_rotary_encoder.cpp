#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "interfaces/i_hal_gpio.hpp"
#include "rotary_encoder.hpp"
#include "mock_hal_pcnt.hpp"
#include "mock_hal_timer.hpp"

#ifndef GPIO_NUM_5
#define GPIO_NUM_5 static_cast<gpio_num_t>(5)
#endif
#ifndef GPIO_NUM_6
#define GPIO_NUM_6 static_cast<gpio_num_t>(6)
#endif

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::_;

namespace ui_inputs {

class RotaryEncoderTest : public ::testing::Test {
protected:
    NiceMock<idf_hals::MockPcntHAL> mock_pcnt_;
    NiceMock<idf_hals::MockTimerHAL> mock_timer_;
    gpio_num_t pin_a_{GPIO_NUM_5};
    gpio_num_t pin_b_{GPIO_NUM_6};
    uint64_t current_time_us_{1000000}; // 1000ms
    int mock_count_{0};

    void SetUp() override {
        ON_CALL(mock_timer_, get_time_us())
            .WillByDefault([this]() { return current_time_us_; });

        ON_CALL(mock_pcnt_, new_unit(_, _))
            .WillByDefault([](const pcnt_unit_config_t*, pcnt_unit_handle_t* ret_unit) {
                if (ret_unit) *ret_unit = reinterpret_cast<pcnt_unit_handle_t>(0x100);
                return ESP_OK;
            });

        ON_CALL(mock_pcnt_, new_channel(_, _, _))
            .WillByDefault([](pcnt_unit_handle_t, const pcnt_chan_config_t*, pcnt_channel_handle_t* ret_chan) {
                if (ret_chan) *ret_chan = reinterpret_cast<pcnt_channel_handle_t>(0x200);
                return ESP_OK;
            });

        ON_CALL(mock_pcnt_, unit_get_count(_, _))
            .WillByDefault([this](pcnt_unit_handle_t, int* val) {
                if (val) *val = mock_count_;
                return ESP_OK;
            });

        ON_CALL(mock_pcnt_, unit_clear_count(_))
            .WillByDefault([this](pcnt_unit_handle_t) {
                mock_count_ = 0;
                return ESP_OK;
            });
    }

    void simulate_pulse_count(int count) {
        mock_count_ = count;
    }

    void advance_time_ms(uint32_t ms) {
        current_time_us_ += static_cast<uint64_t>(ms) * 1000;
    }
};

TEST_F(RotaryEncoderTest, ClockwiseFullStep) {
    RotaryEncoderConfig config;
    config.half_step_mode = false; // 4 pulses per step
    config.acceleration_enabled = false;

    RotaryEncoder encoder(mock_pcnt_, mock_timer_, pin_a_, pin_b_, config);
    EXPECT_EQ(encoder.init(), ESP_OK);

    // Initial state: 0 count
    simulate_pulse_count(0);
    encoder.update();
    EXPECT_EQ(encoder.get_steps(), 0);

    // Hardware PCNT counts +4 pulses for 1 CW full step
    simulate_pulse_count(4);
    encoder.update();

    EXPECT_EQ(encoder.get_steps(), 1);
}

TEST_F(RotaryEncoderTest, CounterClockwiseFullStep) {
    RotaryEncoderConfig config;
    config.half_step_mode = false; // 4 pulses per step
    config.acceleration_enabled = false;

    RotaryEncoder encoder(mock_pcnt_, mock_timer_, pin_a_, pin_b_, config);
    EXPECT_EQ(encoder.init(), ESP_OK);

    // Hardware PCNT counts -4 pulses for 1 CCW full step
    simulate_pulse_count(-4);
    encoder.update();

    EXPECT_EQ(encoder.get_steps(), -1);
}

TEST_F(RotaryEncoderTest, HalfStepModeCW) {
    RotaryEncoderConfig config;
    config.half_step_mode = true; // 2 pulses per step
    config.acceleration_enabled = false;

    RotaryEncoder encoder(mock_pcnt_, mock_timer_, pin_a_, pin_b_, config);
    EXPECT_EQ(encoder.init(), ESP_OK);

    // 2 pulses = 1 half step CW
    simulate_pulse_count(2);
    encoder.update();
    EXPECT_EQ(encoder.get_steps(), 1);

    // Another 2 pulses = 1 half step CW
    simulate_pulse_count(2);
    encoder.update();
    EXPECT_EQ(encoder.get_steps(), 1);
}

TEST_F(RotaryEncoderTest, HalfStepModeCCW) {
    RotaryEncoderConfig config;
    config.half_step_mode = true; // 2 pulses per step
    config.acceleration_enabled = false;

    RotaryEncoder encoder(mock_pcnt_, mock_timer_, pin_a_, pin_b_, config);
    EXPECT_EQ(encoder.init(), ESP_OK);

    // -2 pulses = 1 half step CCW
    simulate_pulse_count(-2);
    encoder.update();
    EXPECT_EQ(encoder.get_steps(), -1);

    // Another -2 pulses = 1 half step CCW
    simulate_pulse_count(-2);
    encoder.update();
    EXPECT_EQ(encoder.get_steps(), -1);
}

TEST_F(RotaryEncoderTest, StepAcceleration) {
    RotaryEncoderConfig config;
    config.half_step_mode = false;
    config.acceleration_enabled = true;
    config.accel_gap_ms = 50;
    config.accel_max_multiplier = 5;

    RotaryEncoder encoder(mock_pcnt_, mock_timer_, pin_a_, pin_b_, config);
    EXPECT_EQ(encoder.init(), ESP_OK);

    // First full step (+4 pulses)
    simulate_pulse_count(4);
    encoder.update();
    EXPECT_EQ(encoder.get_steps(), 1);

    // Second full step (+4 pulses) very quickly (10ms later)
    advance_time_ms(10);
    simulate_pulse_count(4);
    encoder.update();

    int32_t accel_steps = encoder.get_steps();
    EXPECT_GT(accel_steps, 1);
    EXPECT_LE(accel_steps, config.accel_max_multiplier);
}

TEST_F(RotaryEncoderTest, AccelerationDisabled) {
    RotaryEncoderConfig config;
    config.half_step_mode = false;
    config.acceleration_enabled = false;

    RotaryEncoder encoder(mock_pcnt_, mock_timer_, pin_a_, pin_b_, config);
    EXPECT_EQ(encoder.init(), ESP_OK);

    // First full step (+4 pulses)
    simulate_pulse_count(4);
    encoder.update();
    EXPECT_EQ(encoder.get_steps(), 1);

    // Second full step (+4 pulses) rapidly (10ms later)
    advance_time_ms(10);
    simulate_pulse_count(4);
    encoder.update();

    EXPECT_EQ(encoder.get_steps(), 1);
}

TEST_F(RotaryEncoderTest, GetStepsClearsAccumulator) {
    RotaryEncoderConfig config;
    config.half_step_mode = false;
    config.acceleration_enabled = false;

    RotaryEncoder encoder(mock_pcnt_, mock_timer_, pin_a_, pin_b_, config);
    EXPECT_EQ(encoder.init(), ESP_OK);

    simulate_pulse_count(4);
    encoder.update();

    EXPECT_EQ(encoder.get_steps(), 1);
    // Second call should return 0
    EXPECT_EQ(encoder.get_steps(), 0);
}

TEST_F(RotaryEncoderTest, InitAndDeinit) {
    RotaryEncoder encoder(mock_pcnt_, mock_timer_, pin_a_, pin_b_);
    EXPECT_EQ(encoder.init(), ESP_OK);
    EXPECT_EQ(encoder.deinit(), ESP_OK);
}

} // namespace ui_inputs
