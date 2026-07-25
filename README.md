# UI Inputs

[![ESP-IDF Build](https://github.com/aluiziotomazelli/ui_inputs/actions/workflows/build.yml/badge.svg)](https://github.com/aluiziotomazelli/ui_inputs/actions/workflows/badge.svg)
[![Host Tests](https://github.com/aluiziotomazelli/ui_inputs/actions/workflows/host_test.yml/badge.svg)](https://github.com/aluiziotomazelli/ui_inputs/actions/workflows/host_test.yml)
[![Coverage](https://img.shields.io/badge/coverage-100%25-brightgreen)](https://aluiziotomazelli.github.io/ui_inputs/index.html)

A lightweight, dependency-injected C++ user interface input library for ESP-IDF. Provides debounced push buttons, rotary encoders with dynamic acceleration, capacitive touch buttons with passive baseline recalibration, and bistable toggle switches. Fully decoupled from hardware peripherals via HAL interfaces (`idf_hals`) and 100% testable on host Linux environments.

---

## 📑 Documentation & API Reference

Detailed API documentation and method contracts are available in **[API.md](API.md)**.

---

## Overview

`ui_inputs` provides non-blocking, object-oriented state machines for physical user input hardware:
* **Mechanical Push Buttons (`Button`)**: Polled state machine handling single clicks, double clicks, long clicks, very long clicks, active-high/low polarity, and noise filtering.
* **Rotary Encoders (`RotaryEncoder`)**: Quadrature decoder supporting full-step and half-step detent modes, step accumulation, and non-linear dynamic velocity acceleration.
* **Capacitive Touch Buttons (`TouchButton`)**: Non-blocking touch FSM with hardware V1 (ESP32) vs V2 (ESP32-S3) trigger threshold support, hold repeat events, and zero-timer passive baseline recalibration.
* **Bistable Toggle Switches (`Switch`)**: Non-blocking level debouncer for toggle switches, limit switches, and DIP switches, detecting state transitions (`CHANGED_TO_OPEN`, `CHANGED_TO_CLOSED`).

---

## Architecture

```
┌────────────────────────────────────────────────────────────────────────┐
│                        User Interface Layer                            │
│           (Button, RotaryEncoder, TouchButton, Switch)                 │
└────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│                        Hardware Abstraction Layer                      │
│             (IGpioHAL, ITimerHAL, ITouchHAL via idf_hals)              │
└────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│                           ESP-IDF Drivers                              │
│       (driver/gpio.h, esp_timer.h, driver/touch_sens.h)                │
└────────────────────────────────────────────────────────────────────────┘
```

---

## Key Features

- **Zero FreeRTOS Task Spawning**: All components use passive `update()` state machines driven by polling. No background task stacks allocated per button or switch!
- **100% Host Testable**: Hardware peripherals are abstracted behind pure interfaces (`idf_hals`). The full test suite runs natively on Linux with Google Test and GMock.
- **Cross-Platform Touch Support**: Fully supports ESP32 (HW V1, capacitance drop) and ESP32-S2/S3 (HW V2, capacitance rise).
- **Dynamic Acceleration**: Rotary encoder velocity detection dynamically increases step count during fast rotations.
- **Robust Debouncing**: Integrated digital noise filtering for mechanical contact bounce and capacitive glitch protection.

---

## Requirements

- **Framework**: ESP-IDF v5.1+
- **Language**: C++20 / C++17
- **Target Hardware**: ESP32, ESP32-S2, ESP32-S3, ESP32-C3
- **Dependencies**: `idf_hals` submodule (`https://github.com/aluiziotomazelli/idf_hals.git`)
- **Host Testing**: Google Test and Google Mock

---

## Quick Start Guide

### 1. Installation

Add the component and its submodules to your ESP-IDF project:
```bash
git submodule add https://github.com/aluiziotomazelli/ui_inputs.git components/ui_inputs
git submodule update --init --recursive
```

### 2. Basic Example Usage

```cpp
#include "hal_gpio.hpp"
#include "hal_timer.hpp"
#include "button.hpp"
#include "rotary_encoder.hpp"

using namespace ui_inputs;
using namespace idf_hals;

extern "C" void app_main(void)
{
    // 1. Instantiate HAL wrappers
    HalGpio gpio_hal;
    HalTimer timer_hal;

    // 2. Instantiate and initialize push button
    Button btn(gpio_hal, timer_hal, GPIO_NUM_0, true);
    ESP_ERROR_CHECK(btn.init());

    // 3. Main polling loop
    while (1) {
        btn.update();

        ButtonClickType click = btn.get_last_click();
        if (click == ButtonClickType::CLICK) {
            ESP_LOGI("APP", "Single click detected!");
        } else if (click == ButtonClickType::DOUBLE_CLICK) {
            ESP_LOGI("APP", "Double click detected!");
        } else if (click == ButtonClickType::LONG_CLICK) {
            ESP_LOGI("APP", "Long press detected!");
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

---

## Project Structure

```
ui_inputs/
├── include/
│   ├── interfaces/              # Pure abstract interfaces
│   │   ├── i_button.hpp
│   │   ├── i_rotary_encoder.hpp
│   │   └── i_switch.hpp
│   ├── button.hpp               # Mechanical Push Button class
│   ├── rotary_encoder.hpp       # Rotary Encoder class
│   ├── touch_button.hpp        # Capacitive Touch Button class
│   └── switch.hpp               # Bistable Toggle Switch class
├── src/
│   ├── button.cpp
│   ├── rotary_encoder.cpp
│   ├── touch_button.cpp
│   └── switch.cpp
├── external/
│   └── idf_hals/                # Submodule for ESP-IDF hardware HAL interfaces
├── host_test/                   # GTest suite running natively on Linux host
│   └── test_ui_inputs/
├── API.md                       # Comprehensive API Reference
├── CHANGELOG.md                 # Project version release logs
└── idf_component.yml            # ESP-IDF component registry manifest
```

---

## Testing

### Host-Based Unit Tests

Run the host test suite on Linux (x86_64):
```bash
cd host_test/test_ui_inputs
. ~/dev/esp/esp-idf/export.sh
idf.py --preview set-target linux
idf.py build
./build/test_ui_inputs.elf
```

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
