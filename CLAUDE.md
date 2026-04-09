# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Nesso_base** is Arduino firmware for the **Arduino Nesso N1** handheld controller — a WiFi-enabled base station that controls a remote robot platform via UDP. It features a 240×135px TFT display with a 4-mode menu, battery monitoring, and gamepad input.

## Build & Flash

This is a standard Arduino sketch (`.ino`). There is no Makefile or PlatformIO config.

1. Open `Nesso_base.ino` in **Arduino IDE**
2. Install the **Nesso N1 board support package**
3. Install required libraries: `Adafruit seesaw`, `Arduino_Nesso_N1`
4. Select the Nesso N1 board, set baud to **115200**
5. Compile & Upload via Arduino IDE

To enable debug output, change `#define DEBUG 0` to `#define DEBUG 1` near the top of `Nesso_base.ino`.

## Architecture

All firmware lives in a single sketch: `Nesso_base.ino` (~800 lines). `arduino_secrets.h` holds WiFi credentials.

### 4-Mode Menu System

Navigation uses KEY1 (forward) and KEY2 (backward) with 500ms debounce. Modes are defined in the `MainFunctions` enum:

| Value | Mode | Description |
|---|---|---|
| 0 | `FUNCTION_MAIN` | Clock + WiFi status via NTP (UTC+3) |
| 1 | `FUNCTION_MATRIX` | Matrix rain animation |
| 2 | `FUNCTION_BATTERY` | Battery voltage, charge %, uptime |
| 3 | `FUNCTION_CONTROLLER` | Gamepad → UDP motor commands |

### Robot Control Protocol

The controller mode reads an **Adafruit seesaw mini gamepad** (I2C address `0x50`) and transmits a `ControlCommand` struct (two `int16_t` values, range −255 to +255) via **UDP to 192.168.1.27:8889**.

- Horizontal stick → synchronized forward/reverse (both motors same power)
- Vertical stick → differential steering (motors opposite → rotation)
- Joystick zero point is calibrated on first read

### Key Hardware Abstractions

- `NessoBattery` — battery voltage, charge %, and charge state management
- `NessoDisplay` / `LGFX_Sprite` — display rendering; sprite used for battery and controller modes for efficiency
- `WiFiEvent` callback runs on a FreeRTOS task for async WiFi state handling

### Battery Charging Logic

Charging is enabled when external power is detected (`VIN_DETECT`). It is disabled at ≥80% or ≥99% charge. Voltage color thresholds: green >3.7V, orange 3.3–3.7V, red <3.3V.

### Network Configuration (hardcoded)

| Setting | Value |
|---|---|
| UDP target | `192.168.1.27:8889` |
| NTP server | `pool.ntp.org` |
| Timezone offset | UTC+3 (10800 s) |

Credentials are in `arduino_secrets.h` (`SECRET_SSID`, `SECRET_PASS`).
