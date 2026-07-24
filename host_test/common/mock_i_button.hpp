#pragma once

#include <gmock/gmock.h>
#include "interfaces/i_button.hpp"

namespace ui_inputs {

class MockIButton : public IButton {
public:
    MOCK_METHOD(void, update, (), (override));
    MOCK_METHOD(ButtonClickType, get_last_click, (), (override));
};

} // namespace ui_inputs
