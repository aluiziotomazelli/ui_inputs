# UI Inputs — API Reference

Complete API documentation for the `ui_inputs` component. This reference covers interfaces, concrete classes, data types, configurations, and method contracts.

---

## Table of Contents

- [Core Interfaces](#core-interfaces)
  - [`IButton`](#ibutton)
  - [`IRotaryEncoder`](#irotaryencoder)
  - [`ISwitch`](#iswitch)
- [Mechanical Button (`Button`)](#mechanical-button-button)
  - [`ButtonConfig`](#buttonconfig)
  - [`Button`](#button-class)
- [Rotary Encoder (`RotaryEncoder`)](#rotary-encoder-rotaryencoder)
  - [`RotaryEncoderConfig`](#rotaryencoderconfig)
  - [`RotaryEncoder`](#rotaryencoder-class)
- [Touch Button (`TouchButton`)](#touch-button-touchbutton)
  - [`TouchTriggerMode`](#touchtriggermode)
  - [`TouchButtonConfig`](#touchbuttonconfig)
  - [`TouchButton`](#touchbutton-class)
- [Bistable Switch (`Switch`)](#bistable-switch-switch)
  - [`SwitchConfig`](#switchconfig)
  - [`Switch`](#switch-class)

---

## Core Interfaces

### `IButton`

Abstract interface representing a push button or trigger input.

```cpp
enum class ButtonClickType {
    NONE_CLICK,
    CLICK,
    DOUBLE_CLICK,
    LONG_CLICK,
    VERY_LONG_CLICK,
    HOLD_REPEAT
};

class IButton {
public:
    virtual ~IButton() = default;
    virtual void update() = 0;
    virtual ButtonClickType get_last_click() = 0;
};
```

---

### `IRotaryEncoder`

Abstract interface representing a rotary encoder control.

```cpp
class IRotaryEncoder {
public:
    virtual ~IRotaryEncoder() = default;
    virtual void update() = 0;
    virtual int32_t get_steps() = 0;
};
```

---

### `ISwitch`

Abstract interface representing a bistable toggle switch or limit switch.

```cpp
enum class SwitchState {
    OPEN,
    CLOSED
};

enum class SwitchEvent {
    NONE,
    CHANGED_TO_OPEN,
    CHANGED_TO_CLOSED
};

class ISwitch {
public:
    virtual ~ISwitch() = default;
    virtual void update() = 0;
    virtual SwitchState get_state() const = 0;
    virtual SwitchEvent get_last_event() = 0;
};
```

---

## Mechanical Button (`Button`)

### `ButtonConfig`

Configuration struct for timing and sensitivity of mechanical buttons.

```cpp
struct ButtonConfig {
    uint32_t debounce_press_ms{20};        ///< Debounce filter time on press
    uint32_t debounce_release_ms{20};      ///< Debounce filter time on release
    uint32_t double_click_timeout_ms{300};  ///< Max interval between clicks to detect double click
    uint32_t long_click_time_ms{1000};     ///< Press duration to trigger LONG_CLICK
    uint32_t very_long_click_time_ms{3000};///< Press duration to trigger VERY_LONG_CLICK
    uint32_t max_click_time_ms{5000};      ///< Press duration after which error state resets button
};
```

### `Button` Class

```cpp
class Button : public IButton {
public:
    Button(idf_hals::IGpioHAL& gpio_hal,
           idf_hals::ITimerHAL& timer_hal,
           gpio_num_t pin,
           bool active_low,
           const ButtonConfig& config = ButtonConfig{});

    void update() override;
    ButtonClickType get_last_click() override;
};
```

---

## Rotary Encoder (`RotaryEncoder`)

### `RotaryEncoderConfig`

Configuration struct for encoder step resolution and velocity acceleration.

```cpp
enum class DetentMode {
    FULL_STEP,  ///< 4 phase transitions per detent step
    HALF_STEP   ///< 2 phase transitions per detent step
};

struct RotaryEncoderConfig {
    DetentMode detent_mode{DetentMode::FULL_STEP};
    bool enable_acceleration{true};      ///< Enable non-linear dynamic acceleration
    uint32_t accel_threshold_ms{50};     ///< Max ms between steps to trigger acceleration multiplier
    uint32_t accel_multiplier{2};        ///< Multiplier added per fast step
};
```

### `RotaryEncoder` Class

```cpp
class RotaryEncoder : public IRotaryEncoder {
public:
    RotaryEncoder(idf_hals::IGpioHAL& gpio_hal,
                  idf_hals::ITimerHAL& timer_hal,
                  gpio_num_t pin_a,
                  gpio_num_t pin_b,
                  const RotaryEncoderConfig& config = RotaryEncoderConfig{});

    void update() override;
    int32_t get_steps() override;
};
```

---

## Touch Button (`TouchButton`)

### `TouchTriggerMode`

Determines how capacitance changes correlate to touch detection depending on ESP-IDF hardware architecture:

```cpp
enum class TouchTriggerMode {
    ABOVE_BASELINE, ///< ESP32-S2 / ESP32-S3 (HW V2): Reading rises on touch (raw > baseline + delta)
    BELOW_BASELINE  ///< ESP32 (HW V1): Reading drops on touch (raw < baseline - delta)
};
```

### `TouchButtonConfig`

```cpp
struct TouchButtonConfig {
    TouchTriggerMode trigger_mode{TouchTriggerMode::ABOVE_BASELINE};
    uint32_t threshold_delta{100};               ///< Capacitance delta needed to trigger touch
    uint32_t debounce_press_ms{20};              ///< Debounce time on press
    uint32_t debounce_release_ms{20};            ///< Debounce time on release
    uint32_t hold_time_ms{1000};                 ///< Duration before emitting LONG_CLICK
    uint32_t hold_repeat_interval_ms{200};       ///< Repeat rate when holding touch
    bool enable_hold_repeat{true};               ///< Enable continuous HOLD_REPEAT events
    uint32_t recalibration_interval_ms{600000};  ///< Recalibration interval (10 minutes)
};
```

### `TouchButton` Class

```cpp
class TouchButton : public IButton {
public:
    TouchButton(idf_hals::ITouchHAL& touch_hal,
                idf_hals::ITimerHAL& timer_hal,
                touch_channel_handle_t chan_handle,
                uint32_t initial_baseline,
                const TouchButtonConfig& config = TouchButtonConfig{});

    void update() override;
    ButtonClickType get_last_click() override;
    uint32_t get_baseline() const;
};
```

---

## Bistable Switch (`Switch`)

### `SwitchConfig`

```cpp
struct SwitchConfig {
    uint32_t debounce_ms{20}; ///< Debounce time in milliseconds
};
```

### `Switch` Class

```cpp
class Switch : public ISwitch {
public:
    Switch(idf_hals::IGpioHAL& gpio_hal,
           idf_hals::ITimerHAL& timer_hal,
           gpio_num_t pin,
           bool active_low,
           const SwitchConfig& config = SwitchConfig{});

    void update() override;
    SwitchState get_state() const override;
    SwitchEvent get_last_event() override;
};
```
