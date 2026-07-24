#pragma once

#include <gmock/gmock.h>
#include "interfaces/i_rotary_encoder.hpp"

namespace ui_inputs {

class MockIRotaryEncoder : public IRotaryEncoder {
public:
    MOCK_METHOD(void, update, (), (override));
    MOCK_METHOD(int32_t, get_steps, (), (override));
};

} // namespace ui_inputs
