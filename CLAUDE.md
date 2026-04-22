# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Nesso_base** is Arduino firmware for the **Arduino Nesso N1** handheld controller — a WiFi-enabled base station that controls a remote robot platform via UDP. It features a 240×135px TFT display with a multi-mode menu, battery monitoring, gamepad input, and an IR remote control system backed by LittleFS file storage.

## Build & Flash

This is a standard Arduino sketch (`.ino`). Build toolchain is **arduino-cli**.

### Compile

```powershell
arduino-cli compile --fqbn "esp32:esp32:arduino_nesso_n1" Nesso_base.ino
```

### Upload firmware

```powershell
arduino-cli upload --fqbn "esp32:esp32:arduino_nesso_n1" --port COM3 Nesso_base.ino
```

### Build & flash LittleFS image

The `data/` directory is the LittleFS filesystem root. After adding or changing files under `data/`, rebuild and reflash the image.

```powershell
# Build image (size matches the spiffs partition: 0x9E0000 bytes)
& "D:\packages\arduino\data\packages\esp32\tools\mklittlefs\4.0.2-db0513a\mklittlefs.exe" `
    -c data -b 4096 -p 256 -s 0x9E0000 littlefs.bin

# Flash image to spiffs partition offset 0x610000
& "D:\packages\arduino\data\packages\esp32\tools\esptool_py\5.2.0\esptool.exe" `
    --chip esp32c6 --port COM3 --baud 460800 write_flash 0x610000 littlefs.bin
```

Adjust `COM3` to the actual port shown in Device Manager. The LittleFS flash step is only needed when `data/` contents change; firmware uploads do not touch it.

### Partition layout (`partitions.csv`)

| Name     | Offset     | Size       | Notes                  |
|----------|------------|------------|------------------------|
| nvs      | 0x9000     | 0x5000     | NVS key-value store    |
| app0     | 0x10000    | 0x300000   | Firmware (OTA slot 0)  |
| app1     | 0x310000   | 0x300000   | Firmware (OTA slot 1)  |
| spiffs   | 0x610000   | 0x9E0000   | LittleFS data          |

### Arduino IDE (alternative)

1. Install the **Nesso N1 board support package**
2. Install required libraries: `Adafruit seesaw`, `Arduino_Nesso_N1`
3. Select the Nesso N1 board, set baud to **115200**
4. Compile & Upload via Arduino IDE

To enable debug output, change `#define DEBUG 0` to `#define DEBUG 1` near the top of `Nesso_base.ino`.

## Architecture

All firmware lives in a single sketch: `Nesso_base.ino`. `arduino_secrets.h` holds WiFi credentials.

### Menu System

Navigation uses KEY1 (forward) and KEY2 (backward) with 500ms debounce. Modes are defined in the `MainFunctions` enum:

| Value | Mode | Description |
|---|---|---|
| 0 | `FUNCTION_MAIN` | Clock + WiFi status via NTP (UTC+3) |
| 1 | `FUNCTION_MATRIX` | Matrix rain animation |
| 2 | `FUNCTION_BATTERY` | Battery voltage, charge %, uptime |
| 3 | `FUNCTION_CONTROLLER` | Gamepad → UDP motor commands |
| 4 | `FUNCTION_IR` | IR remote control |

### IR Remote System

IR device files (`.ir` format) are stored in LittleFS under `/irdb/` and scanned at boot by `irScanFiles()`. The UI is two levels:

1. **File list** — scrollable list of all `.ir` files found under `/irdb/` (recursive)
2. **Remote** — 2-column button grid for the selected device; tap a button to transmit

The active device is persisted to NVS as `irSel2` (int8_t, index into the scanned file list).

#### Adding IR files

**Over serial (no reflash needed):** use the `fs upload` command — see the Filesystem Serial Commands section below.

**Via full reflash:** drop `.ir` files anywhere under `data/irdb/` (subdirectories are fine), rebuild the LittleFS image, and reflash it. No firmware change needed.

Two `.ir` formats are supported:

- **Parsed** (`type: parsed`) — uses `protocol:`, `address:`, `command:` fields. Supported protocols: `NEC`, `SAMSUNG`/`SAMSUNG32`, `SIRC12`/`SIRC15`/`SIRC20`, `RC5`, `RC6`, `LG`, `JVC`.
- **Raw** (`type: raw`) — uses a `data:` field with microsecond timings. Only the first frame is used (gap threshold > 15 000 µs).

Address and command values in parsed files are **little-endian hex bytes**, e.g. `address: 30 00 00 00` → `0x00000030`.

#### IR serial commands (115200 baud)

| Command | Description |
|---|---|
| `ir list` | Print all scanned `.ir` files (`*` = currently loaded) |
| `ir reload` | Re-scan `/irdb/` without reboot |
| `ir send <N> <label>` | Load device N and send the named button |
| `ir pin` | Show IR TX GPIO info |

#### Raw signal notes

Raw signals (`type: raw`) are sent **3 times with 45 ms spacing** to match Sony's repeat requirement. Signals with `rawLen < 8` are silently skipped (too short to be valid — usually a bad capture).

#### Key constants (top of sketch, before last `#include`)

```cpp
#define IR_MAX_FILES   32   // max .ir files scanned
#define IR_MAX_BUTTONS 48   // max buttons per device
#define IR_MAX_RAW_LEN 128  // max raw timing entries
```

> **Arduino preprocessor note:** `enum IRFileProto`, `struct IRButton`, and `struct IRFileEntry` are defined *before* `#include <ArduinoJson.h>` (the last `#include` in the sketch). This is required so the auto-generated function prototypes that Arduino inserts after the last `#include` can reference these types. Do not move them below that include.

### Serial Command Reference

Connect at **115200 baud**. Type `help` for the full list. Commands work over both USB serial and BLE UART.

#### Filesystem (`fs`)

| Command | Description |
|---|---|
| `fs info` | LittleFS total / used / free bytes |
| `fs ls [path]` | List directory contents (default `/`) |
| `fs cat <path>` | Print file contents to terminal |
| `fs rm <path>` | Delete a file |
| `fs mkdir <path>` | Create a directory (parent must exist) |
| `fs mv <src> <dst>` | Rename or move a file |
| `fs upload <path>` | Upload a text file — paste content then send `---END---` on its own line |

**Upload workflow** — add a new `.ir` file without reflashing:
```
fs mkdir /irdb/Soundbars/Sony
fs upload /irdb/Soundbars/Sony/MyRemote.ir
<paste file contents>
---END---
```
The device prints byte count and auto-rescans `/irdb` on success.

**Implementation notes:**
- `fsUploadFeed()` processes upload bytes char-by-char so long `data:` lines (hundreds of chars) never overflow the 160-byte command buffer.
- The `---END---` sentinel is detected in a 12-byte line buffer; if a line exceeds that, it is streamed directly to LittleFS and cannot be the sentinel.
- After any `fs rm`, `fs mv`, or `fs upload` of a `.ir` file, `/irdb` is automatically rescanned.

#### IR Remote (`ir`)

| Command | Description |
|---|---|
| `ir list` | Print all scanned `.ir` files (`*` = currently loaded) |
| `ir select <N>` | Load device N and open the remote UI |
| `ir reload` | Re-scan `/irdb/` without reboot |
| `ir send <N> <label>` | Load device N and send the named button (exact label) |
| `ir pin` | Show IR TX GPIO number |

### Web File Manager

A browser-based file manager starts automatically when WiFi connects (port 80). Navigate to `http://<device-ip>/` in any browser.

| Feature | Details |
|---|---|
| Browse | Click folders to enter; breadcrumb / `..` row to go up |
| Upload | Click **↑ Upload** button; supports multiple files at once |
| Download | Click **dl** link next to any file |
| Delete | Click **del** — directories must be empty |
| New folder | Click **+ Folder**, enter name in prompt |
| Rename/move | Click a file row to select it, type new name in the "new name" box, click **Rename** |
| URL command | Type `webfm` in the serial terminal to print the current URL |

Uploading or deleting `.ir` files auto-triggers a rescan of `/irdb/`. The HTML is embedded in firmware (`web_fm_html.h`); it does not need a file in LittleFS.

**Implementation notes:**
- `webFMPendingStart` flag is set in `WiFiEvent` (FreeRTOS task) and consumed in `loop()` to safely call `WebServer.begin()` from the main task.
- HTML is stored in `web_fm_html.h` as a PROGMEM constant to avoid confusing the Arduino ctags preprocessor (which would misparse JS function signatures in the `.ino` file).

### Robot Control Protocol

The controller mode reads an **Adafruit seesaw mini gamepad** (I2C address `0x50`) and transmits a `ControlCommand` struct (two `int16_t` values, range −255 to +255) via **UDP to 192.168.1.27:8889**.

- Horizontal stick → synchronized forward/reverse (both motors same power)
- Vertical stick → differential steering (motors opposite → rotation)
- Joystick zero point is calibrated on first read

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
