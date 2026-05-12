# CLAUDE.md

Guidance for Claude Code when working in this repository. Full user documentation is in [README.md](README.md).

## Project Overview

**Nesso_base** is Arduino firmware for the **Arduino Nesso N1** (ESP32-C6) handheld controller. Single sketch: `Nesso_base.ino`. WiFi credentials in `arduino_secrets.h`.

## Build Commands

```powershell
# Compile
arduino-cli compile --fqbn "esp32:esp32:arduino_nesso_n1" --build-path "D:/Nesso_base/build" Nesso_base.ino

# Upload
arduino-cli upload --fqbn "esp32:esp32:arduino_nesso_n1" --port COM3 --build-path "D:/Nesso_base/build" Nesso_base.ino

# Build LittleFS image
& "D:\packages\arduino\data\packages\esp32\tools\mklittlefs\4.0.2-db0513a\mklittlefs.exe" -c data -b 4096 -p 256 -s 0x9E0000 littlefs.bin

# Flash LittleFS
& "D:\packages\arduino\data\packages\esp32\tools\esptool_py\5.2.0\esptool.exe" --chip esp32c6 --port COM3 --baud 460800 write_flash 0x610000 littlefs.bin
```

`--build-path` is required — arduino-cli v1.4.1 has a caching bug that corrupts the ESP32 BLE library archive without it.

## Key Implementation Notes

### Arduino preprocessor type ordering

`enum IRFileProto`, `struct IRButton`, `struct IRFileEntry`, `enum BtnType`, `struct IRBtnPos`, and `struct IRLearnData` are all defined **before** `#include <ArduinoJson.h>` (the last `#include` in the sketch). Arduino auto-generates function prototypes after the last `#include`; these types must be visible at that point. Do not move them below that include.

### Sprite / header architecture (`g_spriteY`)

Most screens push `statusSprite` at `y = SPRITE_Y` (22 px), leaving the top 22 px for the battery/WiFi/BT header. IR and RF433 use the full display height (`g_spriteY = 0`).

`renderFunction()` checks the desired `g_spriteY` value every frame (unconditionally — not gated on `lastFunction`) and resizes the sprite if it changed. This makes it immune to `updateOrientation()` setting `lastFunction = −1` mid-navigation. `initIR()` and `initRF433()` call `display.fillScreen()` before pushing the sprite at `y = 0`. All touch coordinate transforms in IR/RF433 use `sy − g_spriteY`.

### Battery percentage

Use `voltageToPercent()` (piecewise linear LiPo curve, 3.0V→0% / 4.2V→100%). Do **not** use `battery.getChargeLevel()` — tested at 3.89V it returned 98% while voltage-based correctly gave ~63%. The library value is kept only as a debug label on the battery screen.

### ESP32-C6 I2C constraint (RFID2)

ESP32-C6 has only one HP I2C controller (`Wire`). The LP I2C SDA is hardware-locked to GPIO 6, unusable for GPIO 5. RFID2 operations briefly switch `Wire` to GPIO 5/4 via `rfid2WireGrove()` / `rfid2WireRestore()`, which call `Wire.end()` + `Wire.begin()`. `Wire.end()` is mandatory before `Wire.begin()` with different pins — omitting it leaves the bus silently stuck on the old pins.

### Speaker (MAX98357A I2S)

Main-loop polling only — no FreeRTOS task. `spkUpdate()` is called every `loop()` iteration and writes up to 512 synthesised samples via `i2s_channel_write()` with `timeout_ms = 0` (non-blocking; samples drop rather than stall). A FreeRTOS task approach caused resets on the single-core ESP32-C6 and was replaced with this design.

### RF433 TX pin must be LOW before GROVE power-on

The SYN115 keys its 433 MHz carrier whenever its DATA pin is high or floating. With TX and RX on a Y-cable, a floating GPIO 5 causes self-interference on the SYN531R. `rf433LearnStart()` drives `RF433_TX_PIN` LOW before enabling GROVE power.

### IR / RF433 mutual exclusion

Both use GPIO 4 via different ISR handlers. `irLearnStart()` refuses if `rf433LearnMode` is active; `rf433LearnStart()` refuses if `irLearnMode` is active.

### Key constants

```cpp
#define IR_MAX_FILES   128
#define IR_MAX_BUTTONS 48
#define IR_MAX_RAW_LEN 128
#define IR_RECV_PIN    4    // GROVE G4
#define IR_UNIT_TX_PIN 5    // GROVE G5

#define RF433_MAX_FILES   64
#define RF433_MAX_BUTTONS 32
#define RF433_MAX_RAW_LEN 256
#define RF433_MAX_ISR_LEN 512
#define RF433_RX_PIN      4
#define RF433_TX_PIN      5
```
