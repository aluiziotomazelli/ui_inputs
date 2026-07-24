#pragma once

#include <cstdint>

namespace ui_inputs {

/**
 * @brief Interface for rotary encoder components
 */
class IRotaryEncoder {
public:
    virtual ~IRotaryEncoder() = default;

    /**
     * @brief Updates the rotary encoder state machine.
     * Must be called periodically to poll the encoder state.
     */
    virtual void update() = 0;

    /**
     * @brief Gets the accumulated steps since the last call to this function.
     * @return Number of steps rotated (positive for CW, negative for CCW).
     */
    virtual int32_t get_steps() = 0;
};

} // namespace ui_inputs
