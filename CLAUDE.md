# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Nesso_base** is Arduino firmware for the **Arduino Nesso N1** handheld controller — a WiFi-enabled base station that controls a remote robot platform via UDP. It features a 240×135px TFT display with a 9-mode menu, battery monitoring, gamepad input, LoRa (Meshtastic), Bluetooth LE scanning, IR blaster, and internet radio via the M5 Speaker 2 HAT.

## Build & Flash

### Preferred: build.sh (arduino-cli)

```bash
bash build.sh            # compile only
bash build.sh upload     # compile + upload to COM36
bash build.sh upload COM42  # compile + upload to a specific port
```

`build.sh` uses `arduino-cli` with `--libraries ./libraries` so the sketch-local patched copy of **ESP32-audioI2S** is used instead of the global library. Never modify the library under the global Arduino libraries path — patch the local copy at `libraries/ESP32-audioI2S-master/` instead.

### Alternative: Arduino IDE

1. Open `Nesso_base.ino` in **Arduino IDE**
2. Install the **Nesso N1 board support package**
3. Install required libraries: `Adafruit seesaw`, `Arduino_Nesso_N1`, `RadioLib`, `IRremoteESP8266`, `ESP32-audioI2S`
4. Select the Nesso N1 board, set baud to **115200**
5. Compile & Upload via Arduino IDE

> The Arduino IDE will automatically prefer `libraries/ESP32-audioI2S-master/` (sketch-local) over the globally installed version.

To enable debug output, change `#define DEBUG 0` to `#define DEBUG 1` near the top of `Nesso_base.ino`.

## Architecture

All firmware lives in a single sketch: `Nesso_base.ino` (~6000 lines). `arduino_secrets.h` holds WiFi credentials.

### 9-Mode Menu System

Navigation uses KEY1 (forward) and KEY2 (backward) with debounce. Long-press KEY1 opens a settings overlay on screens that support it; long-press KEY2 cancels/exits settings. Modes are defined in the `MainFunctions` enum:

| Value | Mode | Description |
|---|---|---|
| 0 | `FUNCTION_MAIN` | Clock + WiFi/BT status via NTP (UTC+3) |
| 1 | `FUNCTION_CONTROLLER` | Gamepad → UDP motor commands |
| 2 | `FUNCTION_BT` | Bluetooth LE scanner + UART server |
| 3 | `FUNCTION_WIFI` | WiFi scan + connection management |
| 4 | `FUNCTION_LORA` | LoRa / Meshtastic packet monitor |
| 5 | `FUNCTION_IR` | IR remote blaster (IRDB library) |
| 6 | `FUNCTION_MEDIA` | Matrix rain + Star Wars melodies (sub-screens via swipe) |
| 7 | `FUNCTION_BATTERY` | Battery voltage, charge %, uptime; long-press = device settings |
| 8 | `FUNCTION_RADIO` | Internet radio via M5 Speaker 2 HAT (I2S / MAX98357A) |

`FUNCTION_CONTROLLER` is skipped in navigation when no seesaw joystick is detected at boot.

### Robot Control Protocol

The controller mode reads an **Adafruit seesaw mini gamepad** (I2C address `0x50`) and transmits a `ControlCommand` struct (two `int16_t` values, range −255 to +255) via **UDP to 192.168.1.27:8889**.

- Horizontal stick → synchronized forward/reverse (both motors same power)
- Vertical stick → differential steering (motors opposite → rotation)
- Joystick zero point is calibrated on first read

### Key Hardware Abstractions

- `NessoBattery` — battery voltage, charge %, and charge state management (AW32001 charger + BQ27220 gauge)
- `NessoDisplay` / `LGFX_Sprite` — display rendering; `statusSprite` is a full-width sprite pushed to `SPRITE_Y=22` each frame, leaving a 22px header row for the battery/WiFi/BT icons
- `NessoTouch` — FT6x36 capacitive touch controller; touch coordinates are rotation-corrected via `transformTouch()`
- `SX1262` (RadioLib) — LoRa radio, SPI-shared with the display (CS=23, IRQ=15, BUSY=19)
- `IRsend` — built-in IR blaster on GPIO 9 (`IR_TX_PIN`)
- `Audio` (ESP32-audioI2S) — I2S streaming to M5 Speaker 2 HAT; pins defined as `SPK2_BCLK=26`, `SPK2_LRC=0`, `SPK2_DOUT=25`
- `BLEScan` / `BLEServer` — BLE scan + Nordic UART Service (NUS) for wireless serial commands

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

### M5 Speaker 2 HAT (FUNCTION_RADIO)

The HAT uses the **MAX98357A** I2S amplifier — no I2C address, so hardware detection is not possible. Two independent flags gate radio functionality:

| Variable | NVS key | Default | Meaning |
|---|---|---|---|
| `speakerHatEnabled` | `spkHatEn` | `false` | Speaker HAT physically attached; enables I2S hardware |
| `webRadioEnabled` | `radioEn` | `false` | Web radio streaming allowed; requires `speakerHatEnabled` |

Turning Speaker HAT off automatically forces Web Radio off. Web Radio cannot be enabled while Speaker HAT is off.

**Enabling:** Both toggles live in `FUNCTION_BATTERY` device settings (long-press KEY1 on the Battery screen → **SPEAKER HAT** then **WEB RADIO**). The WEB RADIO row is greyed out and shows "N/A" until Speaker HAT is on. Serial equivalents: `radio hat on|off` and `radio web on|off`.

**Radio screen states:**
1. Speaker HAT off → "Speaker HAT disabled" guidance
2. Speaker HAT on, Web Radio off → "Web Radio disabled" guidance
3. Both on, WiFi connected → normal playback UI

`audio.loop()` is called every iteration of `loop()` when `radioPlaying` is true, regardless of which screen is active. Audio stops automatically when navigating away from FUNCTION_RADIO.

**Audio event callbacks** (`audio_showstation`, `audio_showstreamtitle`, etc.) are global free functions required by the ESP32-audioI2S library — they cannot be class members.

**Preset stations** (index 1–5): Capital FM, BBC Radio 1, BBC Radio 2, Classic FM, SomaFM Groovesalad.

**Radio screen settings** (long-press KEY1 on Radio screen): station selection and volume only. HAT and Web Radio toggles are intentionally absent here — they live in device settings.

### Settings Overlays

Long-press KEY1 enters a settings overlay on: `FUNCTION_WIFI`, `FUNCTION_BT`, `FUNCTION_LORA`, `FUNCTION_BATTERY`, and `FUNCTION_RADIO`. KEY1 short cycles the selected item's value; KEY2 short moves the cursor; KEY1 long applies and exits; KEY2 long cancels.

### Serial / BLE UART Command Interface

All screens respond to contextual serial commands. Type `help` for a full list. Commands are dispatched in `serialHandleCommand()`. Each function has a `serialHandleXxx()` handler and a `serialPrintFunctionHelp()` case. The BLE UART (Nordic NUS) accepts the same commands wirelessly.
