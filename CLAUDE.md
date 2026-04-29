# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Nesso_base** is Arduino firmware for the **Arduino Nesso N1** handheld controller — a WiFi-enabled base station that controls a remote robot platform via UDP. It features a 240×135px TFT display with a multi-mode menu, battery monitoring, gamepad input, and an IR remote control system backed by LittleFS file storage.

## Build & Flash

This is a standard Arduino sketch (`.ino`). Build toolchain is **arduino-cli**.

### Compile

```powershell
arduino-cli compile --fqbn "esp32:esp32:arduino_nesso_n1" --build-path "D:/Nesso_base/build" Nesso_base.ino
```

> **Note:** `--build-path` is required with arduino-cli v1.x on this project. Without it, a caching bug in v1.4.1 corrupts the ESP32 BLE library archive (`BLE2901.cpp.o` gets a symbol table instead of an ELF), causing a linker error at the final link step.

### Upload firmware

```powershell
arduino-cli upload --fqbn "esp32:esp32:arduino_nesso_n1" --port COM3 --build-path "D:/Nesso_base/build" Nesso_base.ino
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

## Python Scripts

All scripts run with standard Python 3. Only `nesso_terminal.py` requires a third-party package.

### `build_littlefs.py` — build (and optionally flash) the LittleFS image

Copies a hardcoded list of files from the full `data/irdb/` library into a
temporary staging directory, then runs `mklittlefs` to produce `littlefs.bin`.

```powershell
python build_littlefs.py           # build littlefs.bin only
python build_littlefs.py --flash   # build + flash to COM3
```

Edit the `INCLUDE_FILES` list at the top of the script to add or remove files.
Paths are relative to `data/` (the LittleFS root), e.g.:
```
"irdb/TV/Samsung/Samsung_BN59-01315B.ir"
```

### `gen_irdb_all.py` — download the full IR file library

Downloads `.ir` files from **Flipper-IRDB** (`Lucaslhm/Flipper-IRDB`) into
`data/irdb/<Type>/<Brand>/<Model>.ir`. Existing files are never overwritten,
so the script is safe to re-run after the repo adds new devices.

```powershell
python gen_irdb_all.py
```

The `_Converted_/` subtree inside Flipper-IRDB is intentionally skipped (it is
a duplicate of `probonopd/irdb` data in a non-standard directory layout).
Brands already present from Flipper-IRDB are also skipped when checking
`probonopd/irdb`, so there is no duplication.

> **Note:** The full library (~2800 files, ~9.9 MB) exceeds the 9.87 MB
> LittleFS partition. Use `build_littlefs.py` to create a curated image.
> Files not in the image can be pushed to the device at any time with the
> `fs upload` serial command.

### `nesso_terminal.py` — BLE UART terminal

A PC-side terminal that connects to the Nesso N1 over Bluetooth (Nordic UART
Service) instead of USB. Requires the `bleak` BLE library:

```powershell
pip install bleak
python nesso_terminal.py
```

Once connected, all serial commands documented in the Serial Command Reference
section work identically over BLE.

### `gen_irdb.py` / `gen_irdb2.py` / `gen_irdb3.py` / `gen_irdb4.py` / `gen_irdb5.py` — legacy IR header generators (unused)

These five scripts download IR codes from `probonopd/irdb` (CSV format) and
output C struct initialisers for an `IREntry IRDB[]` array (`irdb.h`,
`irdb_tv.h`, etc.). **The current firmware does not use these headers** — the
IR system reads `.ir` files from LittleFS instead. The scripts are kept for
reference but should not be run; use `gen_irdb_all.py` instead.

## Architecture

All firmware lives in a single sketch: `Nesso_base.ino`. `arduino_secrets.h` holds WiFi credentials.

### Menu System

Navigation uses KEY1 (forward) and KEY2 (backward) with 500ms debounce. Modes are defined in the `MainFunctions` enum:

| Value | Mode | Description |
|---|---|---|
| 0 | `FUNCTION_MAIN` | Clock + WiFi status via NTP (UTC+3) |
| 1 | `FUNCTION_CONTROLLER` | Gamepad → UDP motor commands |
| 2 | `FUNCTION_BT` | Bluetooth scanner |
| 3 | `FUNCTION_WIFI` | WiFi network scanner |
| 4 | `FUNCTION_LORA` | LoRa / Meshtastic |
| 5 | `FUNCTION_IR` | IR remote control |
| 6 | `FUNCTION_RF433` | 433 MHz RF remote — enabled in device settings (disabled by default) |
| 7 | `FUNCTION_MEDIA` | Matrix rain / Vader / Obi-Wan |
| 8 | `FUNCTION_BATTERY` | Battery + device settings (long-press KEY1) |

### IR Remote System

IR device files (`.ir` format) are stored in LittleFS under `/irdb/` and scanned at boot by `irScanFiles()`. The UI is two levels:

1. **File list** — scrollable directory browser; navigate folders, tap a file to open the remote.
2. **Remote** — contextual button layout for the selected device; tap a button to transmit. Swipe vertically to scroll.

The active device is persisted to NVS as `irSel2` (path string). Tap the title bar to return to the file list.

#### Remote button layout

Buttons are classified by label and arranged in a fixed priority order regardless of how they appear in the `.ir` file. The layout is pre-computed once in `irBuildLayout()` when a device loads and stored in `irLayout[IR_MAX_BUTTONS]` as pixel-exact `{x, y, w, h}` per button. `irLayoutH` holds the total content height for the scroll bar.

```
┌──────────────────────────┐
│         Power            │  full-width · red   · r=6
├─────────────┬────────────┤
│   Vol  +    │   Chan +   │  half each · green/blue · r=4
├─────────────┼────────────┤
│   Vol  -    │   Chan -   │
├─────────────┴────────────┤
│           Mute           │  full-width (pairs if >1) · r=4
├──────────────────────────┤
│       ▲  Up              │  ┐
│ ◀ Left │  OK  │ Right ▶  │  ├ D-pad cross — third-width each,
│       ▼  Down            │  ┘ corners blank, OK centred
├──────────────────────────┤
│   Menu    │    Home      │  half each
├──────────────────────────┤
│  ◀◀  │  ▶⏸  │  ▶▶       │  third-width · amber
├──────────────────────────┤
│  7   │   8   │   9       │  ┐
│  4   │   5   │   6       │  ├ numpad · third-width · grey
│  1   │   2   │   3       │  │
│        0                 │  ┘ 0 centred in 3-col grid
├──────────────────────────┤
│ Source   │   Input       │  generic 2-per-row catch-all
└──────────────────────────┘
```

Groups with no matching buttons are omitted entirely (no blank rows). The D-pad cross preserves blank corner cells even when some directions are absent, maintaining the + shape.

**Button classification** (`irClassBtn` + `irBuildLayout` in `Nesso_base.ino`):

| Group | Label match (case-insensitive) | Width | Color |
|---|---|---|---|
| Power | `pow`, `pwr`, `standby` | full | Red |
| Mute | `mute`, `silent` | full / paired half | Orange-red |
| Vol + | `vol` + (`+` / `up` / `inc`) | half | Green |
| Vol − | `vol` + (`-` / `dn` / `down`) | half | Green |
| Chan + | `ch`/`chan`/`prog` + (`+` / `up`) | half | Blue |
| Chan − | `ch`/`chan`/`prog` + (`-` / `dn`) | half | Blue |
| OK | exact `ok`, `enter`, `select` | full | Teal |
| Nav Up/Down/Left/Right | `up`, `down`/`dn`, `left`, `right` | third (cross) | Cyan |
| Menu / Home / Back | `menu`, `home`, `back`, `exit` | half | Cyan |
| Media | `play`, `pause`, `stop`, `rewind`, `forward`, `ff`, `rew`, `next`, `prev`, `skip` | third | Amber |
| Numbers | single digit `0`–`9` | third | Grey |
| Generic | everything else | half | Amber |

**Pixel-based scrolling:** `irBtnPageOff` is a pixel offset (not a row index). Swipe magnitude scales at `dy × 8` pixels. Maximum scroll = `irLayoutH − areaH`.

**Flash highlight:** stored as `irFlashIdx` (button index); the glow tracks the button correctly during scroll.

#### Adding IR files

**Over serial (no reflash needed):** use the `fs upload` command — see the Filesystem Serial Commands section below.

**Via full reflash:** drop `.ir` files anywhere under `data/irdb/` (subdirectories are fine), then use `build_littlefs.py` to build and flash the image. No firmware change needed.

**Via M5Stack IR Unit learn mode:** record signals from any physical remote directly onto the device — see [IR Learn Mode (M5Stack IR Unit)](#ir-learn-mode-m5stack-ir-unit) below.

Two `.ir` formats are supported:

- **Parsed** (`type: parsed`) — uses `protocol:`, `address:`, `command:` fields. Supported protocols: `NEC`, `SAMSUNG`/`SAMSUNG32`, `SIRC12`/`SIRC15`/`SIRC20`, `RC5`, `RC6`, `LG`, `JVC`.
- **Raw** (`type: raw`) — uses a `data:` field with microsecond timings. Only the first frame is used (gap threshold > 15 000 µs).

Address and command values in parsed files are **little-endian hex bytes**, e.g. `address: 30 00 00 00` → `0x00000030`.

#### IR serial commands (115200 baud)

| Command | Description |
|---|---|
| `ir list` | Print all scanned `.ir` files (`*` = currently loaded) |
| `ir select <N>` | Load device N and open the remote UI |
| `ir reload` | Re-scan `/irdb/` without reboot |
| `ir send <N> <label>` | Load device N and send the named button (exact label) |
| `ir pin` | Show IR TX / RX GPIO numbers |
| `ir custom new [name]` | Create a new custom remote at `/irdb/Custom/<name>.ir` |
| `ir custom list` | List all custom remotes |
| `ir learn start` | Power up GROVE port and arm M5 IR Unit receiver |
| `ir learn stop` | Stop capture, cut GROVE power |
| `ir learn bind <label>` | Bind last captured signal to a button label |
| `ir learn show` | Print details of the last captured signal |

#### Raw signal notes

Raw signals (`type: raw`) are sent **3 times with 45 ms spacing** to match Sony's repeat requirement. Signals with `rawLen < 8` are silently skipped (too short to be valid — usually a bad capture).

#### IR Learn Mode (M5Stack IR Unit)

The **M5Stack IR Unit (U002)** plugs into the Nesso N1 GROVE port (PORT.CUSTOM) and provides a 38 kHz demodulated IR receiver for capturing signals from any physical remote.

**GROVE port wiring (Nesso N1 PORT.CUSTOM):**

| GROVE signal | Nesso N1 GPIO | Firmware constant | Direction |
|---|---|---|---|
| G4 (IR_RX) | GPIO 4 | `IR_RECV_PIN` | input — demodulated receive |
| G5 (IR_TX) | GPIO 5 | `IR_UNIT_TX_PIN` | output — M5 unit transmitter |

> **GROVE power:** the GROVE 5V rail is off by default (gated by `GROVE_POWER_EN`, an I2C expander pin on the PI4IOE5V6408 at 0x44). `irLearnStart()` calls `pinMode(GROVE_POWER_EN, OUTPUT)` + `digitalWrite(GROVE_POWER_EN, HIGH)` before arming the receiver ISR. `irLearnStop()` cuts GROVE power.

**Custom remote workflow:**

```
ir custom new Living Room   # creates /irdb/Custom/Living_Room.ir, loads it
ir learn start              # powers GROVE, arms receiver (amber dot blinks in title)
<point remote at M5 unit, press Power>
# serial prints: Captured: NEC  addr=0x0020  cmd=0x08
ir learn bind Power         # saves button, UI switches to remote view
<press Vol+ on remote>
ir learn bind Vol+
ir learn stop               # cuts GROVE power
```

**Adding buttons to an existing custom remote:**

```
ir custom list              # find the remote name
ir select <N>               # load it (N from 'ir list')
ir learn start
ir learn bind <label>
ir learn stop
```

**Touch-to-bind:** while learn mode is active and a signal has been captured (green `BIND` indicator), tapping an existing button in the remote UI **rebinds** that button to the captured signal instead of transmitting.

**Duplicate detection (decoded signals only):**
- Same IR code on a different button → that button is renamed to the new label.
- Same label → existing button's code is overwritten.
- Library files (outside `/irdb/Custom/`) are write-protected; `ir learn bind` refuses with an error.

**Signal storage:**
- If IRremote decodes a known protocol (NEC, SAMSUNG32, SIRC, RC5, RC6, LG, JVC), the button is saved as `type: parsed` — compact and reliable.
- Unknown protocols fall back to `type: raw` with 38 000 Hz frequency.

#### Key constants (top of sketch, before last `#include`)

```cpp
#define IR_MAX_FILES   128  // max .ir files scanned at boot
#define IR_MAX_BUTTONS 48   // max buttons per device
#define IR_MAX_RAW_LEN 128  // max raw timing entries
#define IR_RECV_PIN    4    // GROVE G4 — M5 IR Unit demodulated receive
#define IR_UNIT_TX_PIN 5    // GROVE G5 — M5 IR Unit transmitter (reference)
```

> **Arduino preprocessor note:** `enum IRFileProto`, `struct IRButton`, `struct IRFileEntry`, `enum BtnType`, `struct IRBtnPos`, and `struct IRLearnData` are all defined *before* `#include <ArduinoJson.h>` (the last `#include` in the sketch). This is required so the auto-generated function prototypes that Arduino inserts after the last `#include` can reference these types. Do not move them below that include.

### RF433 Remote System

433 MHz OOK ASK remote control using M5Stack Unit RF433T (SYN115 transmitter) and RF433R (SYN531R receiver) connected via a Y-cable to the GROVE port.

**GROVE wiring:**

| GROVE signal | GPIO | Firmware constant | Module |
|---|---|---|---|
| G4 (Yellow) | GPIO 4 | `RF433_RX_PIN` | RF433R — demodulated RX output |
| G5 (White) | GPIO 5 | `RF433_TX_PIN` | RF433T — TX data input |

GROVE 5V power (`GROVE_POWER_EN`) is enabled only during learn mode and briefly during transmission.

> **TX pin must be LOW before GROVE power-on.** The SYN115 transmitter keys its 433 MHz carrier whenever its DATA pin is high or floating. With TX and RX on a Y-cable, a floating GPIO 5 causes severe self-interference on the SYN531R. `rf433LearnStart()` drives `RF433_TX_PIN` LOW before enabling GROVE power to prevent this.

**Feature enable:** RF433 is **disabled by default** and hidden from navigation. Enable it from the Battery/Device settings screen (long-press KEY1 on battery screen → RF433 item) or via `rf433 enable` serial command.

**File format:** Custom remotes are saved as `.sub` files in `/rf433db/Custom/`. Legacy `.433` files are also read. Format:

```
Filetype: Nesso SubGhz Remote
Version: 1
Frequency: 433920000
#
name: Button1
type: raw
RAW_Data: 450 -1350 450 -450 1350 -450 ...
```

Only `type: raw` is used. `RAW_Data` values follow Flipper SubGhz convention: positive = HIGH pulse duration (µs), negative = LOW pulse duration. Values alternate HIGH/LOW starting from the first mark. The reader also accepts the legacy `data:` field (all-positive, unsigned).

**UI:** Two-level browser identical to IR — file list → remote button grid (2 columns, green accent). Tap title bar to go back to list.

**Learn workflow:**

```
rf433 custom new Garage          # creates /rf433db/Custom/Garage.sub, loads it
rf433 learn start                # drives TX LOW, powers GROVE, arms ISR on GPIO 4
<press button on remote>
# serial prints: [RF433] Captured: 512 pulses T=300us (sync gap)
#                [RF433] Decoded: 0x555503 (24-bit, T=300 us, ratio 1:3)
rf433 learn bind Open            # saves button, updates file
rf433 learn stop                 # cuts GROVE power
```

**Signal capture details:**
- ISR fires on every edge of GPIO 4 (CHANGE interrupt), records µs edge durations into a 512-entry ISR buffer (`RF433_MAX_ISR_LEN`).
- Buffer processes immediately when full; otherwise waits for 30 ms silence (end-of-packet gap).
- PT2262/EV1527 remotes are identified by a sync gap pulse in the 9 500–11 500 µs range; this is the primary accept criterion for T=300 µs remotes (e.g. fans, gate openers) that share T with ambient ISM traffic.
- If the remote signal arrived after ambient ISM already filled the first half of the buffer, the firmware falls back to storing the last 256 entries and re-checks for a decodable code.
- Leading pre-signal silence (> 10 ms) and trailing gap (> 15 ms) are trimmed automatically.
- Minimum valid capture: 8 pulses after trim.
- Signal is transmitted 3 × with 10 ms inter-frame gap.

**IR / RF433 mutual exclusion:** `irLearnStart()` refuses if `rf433LearnMode` is active, and `rf433LearnStart()` refuses if `irLearnMode` is active. Both use GPIO 4 via different ISR handlers.

**Key constants:**

```cpp
#define RF433_MAX_FILES   64
#define RF433_MAX_BUTTONS 32
#define RF433_MAX_RAW_LEN 256  // max pulses stored per button / learn capture
#define RF433_MAX_ISR_LEN 512  // ISR capture window (learn mode only — wider to outlast ambient fill)
#define RF433_RX_PIN      4    // GROVE G4
#define RF433_TX_PIN      5    // GROVE G5
```

**NVS keys:** `rf433On` (bool), `rf433Path` (string — last loaded remote path).

#### RF433 serial commands (`rf433`)

| Command | Description |
|---|---|
| `rf433 list` | List all scanned `.sub` / `.433` files |
| `rf433 select <N>` | Load remote N and open UI |
| `rf433 send <N> <label>` | Transmit button from remote N |
| `rf433 reload` | Re-scan `/rf433db/` |
| `rf433 custom new [name]` | Create new custom remote (`.sub`) |
| `rf433 custom list` | List custom remotes |
| `rf433 learn start` | Drive TX LOW, power GROVE, arm SYN531R receiver |
| `rf433 learn stop` | Stop capture, cut GROVE power |
| `rf433 learn bind <label>` | Bind last capture to button label |
| `rf433 learn show` | Print last captured signal info |
| `rf433 enable` | Enable RF433 function |
| `rf433 disable` | Disable RF433 function |

**Touch-to-bind:** While learn mode is active and a signal has been captured (green `BIND` indicator in title bar), tapping an existing button in the remote UI **rebinds** that button to the captured signal.

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
| `ir pin` | Show IR TX / RX GPIO numbers |
| `ir custom new [name]` | Create a new custom remote at `/irdb/Custom/<name>.ir` |
| `ir custom list` | List all custom remotes |
| `ir learn start` | Power GROVE port and arm M5 IR Unit receiver |
| `ir learn stop` | Stop capture, cut GROVE power |
| `ir learn bind <label>` | Bind last captured signal to a button label |
| `ir learn show` | Print details of last captured signal |

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
