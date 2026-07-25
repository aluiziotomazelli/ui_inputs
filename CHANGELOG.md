# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2026-07-24

### Added
- Initial release of the `ui_inputs` component.
- `Button`: Mechanical push button state machine supporting single click, double click, long click, very long click, active-high/low polarity, and digital noise debouncing.
- `RotaryEncoder`: Quadrature encoder decoder supporting full-step and half-step detent modes, step accumulation, and dynamic acceleration.
- `TouchButton`: Capacitive touch button FSM supporting ESP32 (HW V1) and ESP32-S3 (HW V2) trigger modes, hold repeat events, and passive baseline recalibration via `ITimerHAL`.
- `Switch`: Bistable toggle switch state machine for level debouncing and state transition events (`CHANGED_TO_OPEN`, `CHANGED_TO_CLOSED`).
- Pure interface abstractions (`IButton`, `IRotaryEncoder`, `ISwitch`) enabling dependency injection.
- Complete Google Test / GMock host unit test suite (26 passing tests) running natively on Linux.

[0.1.0]: https://github.com/aluiziotomazelli/ui_inputs/releases/tag/v0.1.0
