#pragma once

#include <cstdint>
#include "esp_err.h"

namespace ui_inputs {

/**
 * @brief Interface for rotary encoder components
 */
class IRotaryEncoder {
public:
    virtual ~IRotaryEncoder() = default;

    /**
     * @brief Initializes the rotary encoder hardware resources.
     * @return ESP_OK on success, or appropriate error code.
     */
    virtual esp_err_t init() = 0;

    /**
     * @brief Deinitializes and releases the rotary encoder hardware resources.
     * @return ESP_OK on success.
     */
    virtual esp_err_t deinit() = 0;

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
