#pragma once

namespace ui_inputs {

/**
 * @brief Button click type enumeration
 */
enum class ButtonClickType {
    NONE_CLICK,          ///< No click detected
    CLICK,               ///< Single click
    DOUBLE_CLICK,        ///< Double click
    LONG_CLICK,          ///< Long press (1+ seconds)
    VERY_LONG_CLICK,     ///< Very long press (3+ seconds)
    HOLD_REPEAT,         ///< Periodic repeated hold event
    TIMEOUT,             ///< Press timeout
    ERROR_STATE          ///< Error state
};

/**
 * @brief Interface for button components
 */
class IButton {
public:
    virtual ~IButton() = default;

    /**
     * @brief Updates the button state machine.
     * Must be called periodically to poll the button state.
     */
    virtual void update() = 0;

    /**
     * @brief Gets the last detected click type from the most recent update.
     * @return The click type. Usually returns NONE_CLICK if no new click.
     */
    virtual ButtonClickType get_last_click() = 0;
};

} // namespace ui_inputs
