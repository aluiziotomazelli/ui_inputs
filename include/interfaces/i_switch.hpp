#pragma once

#include <cstdint>
#include "esp_err.h"

namespace ui_inputs {

/**
 * @brief Current state of the switch
 */
enum class SwitchState {
    OPEN,    ///< Switch is open (inactive)
    CLOSED   ///< Switch is closed (active)
};

/**
 * @brief Transition event detected on the switch
 */
enum class SwitchEvent {
    NONE,               ///< No state change detected
    CHANGED_TO_OPEN,    ///< Switch transitioned from closed to open
    CHANGED_TO_CLOSED   ///< Switch transitioned from open to closed
};

/**
 * @interface ISwitch
 * @brief Interface for bistable switch inputs
 */
class ISwitch {
public:
    virtual ~ISwitch() = default;

    /**
     * @brief Initializes the switch hardware resources.
     * @return ESP_OK on success, or appropriate error code.
     */
    virtual esp_err_t init() = 0;

    /**
     * @brief Deinitializes and releases the switch hardware resources.
     * @return ESP_OK on success.
     */
    virtual esp_err_t deinit() = 0;

    /**
     * @brief Polls and updates the switch state machine.
     */
    virtual void update() = 0;

    /**
     * @brief Gets current debounced state of the switch.
     * @return SwitchState::CLOSED or SwitchState::OPEN
     */
    virtual SwitchState get_state() const = 0;

    /**
     * @brief Gets and clears the last state transition event.
     * @return SwitchEvent
     */
    virtual SwitchEvent get_last_event() = 0;
};

} // namespace ui_inputs
