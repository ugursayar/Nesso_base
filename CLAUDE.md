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

The controller mode auto-detects one of two supported joysticks at boot and transmits a `ControlCommand` struct (two `int16_t` values, range −255 to +255) via **UDP to 192.168.1.27:8889**.

| Joystick | I2C address | Detection order | Buttons |
|---|---|---|---|
| Adafruit seesaw mini gamepad | `0x50` | First | A/B/X/Y/SEL/STA |
| M5 Joystick HAT | `0x38` | Fallback | Stick click |

- Physical Y axis (push forward/back) → synchronized forward/reverse (both motors same power)
- Physical X axis (push left/right) → differential steering (motors opposite → rotation)
- Zero point is calibrated on first read for both joystick types
- `activeJoystick` enum (`JOY_NONE`, `JOY_SEESAW`, `JOY_M5`) tracks which was found

#### M5 Joystick HAT protocol (I2C `0x38`)

- Register `0x02` (read 3 bytes): `int8_t x` (−127..127), `int8_t y` (−127..127), `uint8_t btn` (0=pressed, 1=released)
- No explicit initialisation required; operates in normal mode by default
- Calibration register `0x03` (write-only): `0x00`=normal, `0x01`=set centre, `0x02`=capture max range, `0x03`=save to flash + restore normal

#### Hardware calibration flow

Calibration is a 3-phase state machine (`M5CalState`) running every 100 ms in the controller loop:

| Phase | Duration | Register write | Display overlay |
|---|---|---|---|
| `CAL_CENTER` | 2 s | `0x03 ← 0x01` | "RELEASE STICK — setting center point…" |
| `CAL_MAX` | 5 s | `0x03 ← 0x02` | "ROTATE FULL CIRCLE" + progress bar |
| `CAL_SAVING` | 0.5 s | `0x03 ← 0x03` | "SAVING…" |

After saving, `calibrationComplete` is reset so the software zero-point re-calibrates on the next read.

**Triggers:** serial command `joy cal` (from any screen) · hold stick button ≥ 1.5 s (controller screen only)
#### Pin connection — Hat Joystick ↔ Arduino Nesso N1 (Stick-Bus)

All connections are fixed (cannot be changed).

| Hat-Bus | Nesso N1 Stick-Bus | Notes |
|---|---|---|
| I2C_SDA | G6 | Fixed connect |
| I2C_SCL | G7 | Fixed connect |
| GND | GND | Fixed connect |
| — | 3V3 | NG / used by other peripherals |
| — | BAT | NG / used by other peripherals |
| — | 5V | NG / used by other peripherals |

`Wire.begin()` uses the board defaults which map to G6/G7 — no custom pin assignment needed.

- References: [M5Stack HAT Joystick docs](https://docs.m5stack.com/en/hat/hat-joystick) · [example sketch](https://github.com/m5stack/M5StickC/blob/master/examples/Hat/Joystick/Joystick.ino) · [M5StickC repo](https://github.com/m5stack/M5StickC)

### Key Hardware Abstractions

- `NessoBattery` — battery voltage, charge %, and charge state management
- `NessoDisplay` / `LGFX_Sprite` — display rendering; sprite used for battery and controller modes for efficiency
- `WiFiEvent` callback runs on a FreeRTOS task for async WiFi state handling

### Battery Percentage Calculation

Battery percentage is calculated from voltage using `voltageToPercent()` — a piecewise linear LiPo discharge curve (3.0V→0%, 4.2V→100%). The raw `battery.getChargeLevel()` from the library is **not used for display or logic** because it is unreliable: tested at 3.89V it reported 98% while the voltage-based curve correctly returned ~63%. The library value is kept in `chargeLevel` and shown as a tiny `lib:XX%` debug label on the battery screen for comparison.

### Battery Charging Logic

Charging is enabled when external power is detected (`VIN_DETECT`). Voltage color thresholds: green >3.7V, orange 3.3–3.7V, red <3.3V.

### Network Configuration (hardcoded)

| Setting | Value |
|---|---|
| UDP target | `192.168.1.27:8889` |
| NTP server | `pool.ntp.org` |
| Timezone offset | UTC+3 (10800 s) |

Credentials are in `arduino_secrets.h` (`SECRET_SSID`, `SECRET_PASS`).
