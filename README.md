# Nesso_base

Arduino firmware for the **Arduino Nesso N1** handheld controller — a WiFi-enabled base station that drives a remote robot platform over a selectable wireless link (WiFi-UDP/TCP, BLE, LoRa). Features a 240×135px TFT display with a multi-mode menu, battery monitoring, single- or dual-stick gamepad input (Adafruit seesaw and/or M5Stack Mini JoyC HAT), IR and 433 MHz RF remote control, RFID card scanning, and a web-based file manager.

## Table of Contents

- [Hardware Requirements](#hardware-requirements)
- [Build & Flash](#build--flash)
- [Python Scripts](#python-scripts)
- [Menu System](#menu-system)
- [Speaker Hat 2](#speaker-hat-2-max98357a)
- [IR Remote System](#ir-remote-system)
- [RF433 Remote System](#rf433-remote-system)
- [RFID2 Unit](#rfid2-unit)
- [Web File Manager](#web-file-manager)
- [Robot Control](#robot-control)
- [Serial Command Reference](#serial-command-reference)
- [Network Configuration](#network-configuration)

---

## Hardware Requirements

- **Arduino Nesso N1** board (ESP32-C6)
- Optional accessories (GROVE PORT.CUSTOM):
  - M5Stack IR Unit (U002) — IR remote learn/transmit
  - M5Stack RF433T + RF433R units (Y-cable) — 433 MHz remote learn/transmit
  - M5Stack RFID2 Unit (WS1850S) — ISO/IEC 14443-A card scanning
- Optional HATs (HAT bus, SDA=GPIO 6 / SCL=GPIO 7):
  - M5Stack Speaker Hat 2 (MAX98357A) — high-quality I2S audio output
  - M5StickC Mini JoyC HAT (STM32F030, I2C `0x54`) — front-mounted joystick for robot control
- Optional controller:
  - Adafruit seesaw mini gamepad (I2C `0x50`) — stick + 6 buttons

**Required libraries:** `Adafruit seesaw`, `Arduino_Nesso_N1`, `MFRC522_I2C` (for RFID2), `NessoLink` (control-frame codec — [github.com/ugursayar/NessoLink](https://github.com/ugursayar/NessoLink), install into your Arduino `libraries/` folder)

---

## Build & Flash

All firmware lives in `Nesso_base.ino`. WiFi credentials go in `arduino_secrets.h` (copy from `arduino_secrets.h.example`).

### Compile

```powershell
arduino-cli compile --fqbn "esp32:esp32:arduino_nesso_n1" --build-path "D:/Nesso_base/build" Nesso_base.ino
```

> `--build-path` is required with arduino-cli v1.x — without it, a caching bug in v1.4.1 corrupts the ESP32 BLE library archive, causing a linker error.

### Upload firmware

```powershell
arduino-cli upload --fqbn "esp32:esp32:arduino_nesso_n1" --port COM3 --build-path "D:/Nesso_base/build" Nesso_base.ino
```

### Build & flash LittleFS image

The `data/` directory is the LittleFS filesystem root. Rebuild and reflash the image after adding or changing files under `data/`.

```powershell
# Build image (size matches the spiffs partition: 0x9E0000 bytes)
& "D:\packages\arduino\data\packages\esp32\tools\mklittlefs\4.0.2-db0513a\mklittlefs.exe" `
    -c data -b 4096 -p 256 -s 0x9E0000 littlefs.bin

# Flash image to spiffs partition offset 0x610000
& "D:\packages\arduino\data\packages\esp32\tools\esptool_py\5.2.0\esptool.exe" `
    --chip esp32c6 --port COM3 --baud 460800 write_flash 0x610000 littlefs.bin
```

Adjust `COM3` to the actual port shown in Device Manager. The LittleFS flash step is only needed when `data/` contents change; firmware uploads do not touch it.

### Partition layout

| Name   | Offset   | Size     | Notes               |
|--------|----------|----------|---------------------|
| nvs    | 0x9000   | 0x5000   | NVS key-value store |
| app0   | 0x10000  | 0x300000 | Firmware (OTA slot 0) |
| app1   | 0x310000 | 0x300000 | Firmware (OTA slot 1) |
| spiffs | 0x610000 | 0x9E0000 | LittleFS data       |

### Arduino IDE (alternative)

1. Install the **Nesso N1 board support package**
2. Install required libraries: `Adafruit seesaw`, `Arduino_Nesso_N1`
3. Select the Nesso N1 board, set baud to **115200**
4. Compile & Upload

To enable debug output, change `#define DEBUG 0` to `#define DEBUG 1` near the top of `Nesso_base.ino`.

---

## Python Scripts

All scripts run with standard Python 3. Only `nesso_terminal.py` requires a third-party package.

### `build_littlefs.py` — build (and optionally flash) the LittleFS image

Copies a hardcoded list of files from the full `data/irdb/` library into a temporary staging directory, then runs `mklittlefs` to produce `littlefs.bin`.

```powershell
python build_littlefs.py           # build littlefs.bin only
python build_littlefs.py --flash   # build + flash to COM3
```

Edit the `INCLUDE_FILES` list at the top of the script to add or remove files. Paths are relative to `data/` (the LittleFS root), e.g.:

```
"irdb/TV/Samsung/Samsung_BN59-01315B.ir"
```

### `gen_irdb_all.py` — download the full IR file library

The full `data/irdb/` library (2795 `.ir` files) is **committed to the repo**, so you only need this script to refresh or extend it. It downloads `.ir` files from **Flipper-IRDB** (`Lucaslhm/Flipper-IRDB`) into `data/irdb/<Type>/<Brand>/<Model>.ir`. Existing files are never overwritten, so the script is safe to re-run.

```powershell
python gen_irdb_all.py
```

> The full library (~2800 files, ~9.9 MB) exceeds the 9.87 MB LittleFS partition. Use `build_littlefs.py` to create a curated image. Files not in the image can be pushed to the device at any time with the `fs upload` serial command.

### `nesso_terminal.py` — BLE UART terminal

A PC-side terminal that connects to the Nesso N1 over Bluetooth (Nordic UART Service) instead of USB.

```powershell
pip install bleak
python nesso_terminal.py
```

Once connected, all serial commands work identically over BLE.

### Legacy scripts (`gen_irdb.py` / `gen_irdb2–5.py`)

These scripts generate C header files from IR CSV data and are no longer used by the firmware. Kept for reference only — use `gen_irdb_all.py` instead.

---

## Menu System

Navigate with **KEY1** (forward) and **KEY2** (backward), 500 ms debounce. **Long-press KEY1** opens the settings overlay for the current screen.

| Mode | Description |
|------|-------------|
| `FUNCTION_MAIN` | Clock + WiFi status via NTP (UTC+3) |
| `FUNCTION_CONTROLLER` | Gamepad / Mini JoyC → robot commands over selectable link (UDP/BLE/TCP/LoRa) |
| `FUNCTION_BT` | Bluetooth scanner |
| `FUNCTION_WIFI` | WiFi network scanner |
| `FUNCTION_LORA` | LoRa / Meshtastic |
| `FUNCTION_RFID2` | M5Stack RFID2 Unit — hidden when unit not connected |
| `FUNCTION_IR` | IR remote control |
| `FUNCTION_RF433` | 433 MHz RF remote — disabled by default, enable in device settings |
| `FUNCTION_MEDIA` | Matrix rain / Vader / Obi-Wan |
| `FUNCTION_BATTERY` | Battery status + device settings |

**Device settings** (long-press KEY1 on battery screen): DIM TIMEOUT, SLEEP TIMEOUT, LOW BAT SLEEP, UI CLICKS, RF433, SPEAKER, VOLUME, RESET.

---

## Speaker Hat 2 (MAX98357A)

Enable from Battery screen → long-press KEY1 → **SPEAKER**. When enabled, all audio routes through the I2S amplifier instead of the onboard buzzer. Set volume with the **VOLUME** item (25 / 50 / 75 / 100 %, default 75 %).

**HAT port wiring:**

| Signal | GPIO | Notes |
|--------|------|-------|
| BCLK   | GPIO 7 (G7) | Bit clock |
| LRCLK  | GPIO 6 (G6) | Word select |
| DATA   | GPIO 2 (G2) | Serial data |

**Features over buzzer:**
- Sine wave synthesis — smooth, musical tones
- 5 ms attack/release envelopes — no click artifacts
- Vader March and Obi-Wan theme at full audio quality
- Boot chime: three-note C major arpeggio (C5–E5–G5) at startup

**NVS keys:** `spkOn` (bool), `spkVol` (uint8 1–4)

---

## IR Remote System

IR device files (`.ir` format) are stored in LittleFS under `/irdb/` and scanned at boot. The UI is two levels:

1. **File list** — scrollable directory browser; tap a file to open the remote
2. **Remote** — contextual button layout; tap a button to transmit, swipe vertically to scroll

The active device persists across reboots (NVS key `irSel2`). Tap the title bar to return to the file list.

### Button layout

Buttons are auto-classified by label and arranged in a fixed priority order:

```
┌──────────────────────────┐
│         Power            │  full-width · red
├─────────────┬────────────┤
│   Vol  +    │   Chan +   │  half each · green/blue
├─────────────┼────────────┤
│   Vol  -    │   Chan -   │
├─────────────┴────────────┤
│           Mute           │  full-width
├──────────────────────────┤
│       ▲  Up              │
│ ◀ Left │  OK  │ Right ▶  │  D-pad cross
│       ▼  Down            │
├──────────────────────────┤
│   Menu    │    Home      │  half each · cyan
├──────────────────────────┤
│  ◀◀  │  ▶⏸  │  ▶▶       │  third-width · amber
├──────────────────────────┤
│  7   │   8   │   9       │
│  4   │   5   │   6       │  numpad · grey
│  1   │   2   │   3       │
│        0                 │
├──────────────────────────┤
│ Source   │   Input       │  generic catch-all
└──────────────────────────┘
```

Groups with no matching buttons are omitted. The D-pad maintains its + shape even when some directions are absent.

### File formats

Two `.ir` formats are supported:

**Parsed** (`type: parsed`) — uses `protocol:`, `address:`, `command:` fields.
Supported protocols: `NEC`, `SAMSUNG`/`SAMSUNG32`, `SIRC12`/`SIRC15`/`SIRC20`, `RC5`, `RC6`, `LG`, `JVC`.

**Raw** (`type: raw`) — uses a `data:` field with microsecond timings. Only the first frame is used. Raw signals are sent 3× with 45 ms spacing (Sony repeat requirement).

Address and command values are **little-endian hex bytes**, e.g. `address: 30 00 00 00` → `0x00000030`.

### Adding IR files

**Over serial (no reflash):**
```
fs mkdir /irdb/Soundbars/Sony
fs upload /irdb/Soundbars/Sony/MyRemote.ir
<paste file contents>
---END---
```

**Via full reflash:** drop `.ir` files under `data/irdb/`, then run `build_littlefs.py --flash`.

**Via learn mode:** capture signals from any physical remote using the M5Stack IR Unit (see below).

### IR Learn Mode (M5Stack IR Unit)

The **M5Stack IR Unit (U002)** plugs into the GROVE PORT.CUSTOM and provides a 38 kHz demodulated IR receiver.

| GROVE signal | GPIO | Direction |
|---|---|---|
| G4 (IR_RX) | GPIO 4 | input — demodulated receive |
| G5 (IR_TX) | GPIO 5 | output — M5 unit transmitter |

**Custom remote workflow:**

```
ir custom new Living Room   # creates /irdb/Custom/Living_Room.ir
ir learn start              # powers GROVE, arms receiver (amber dot blinks)
<point remote at M5 unit, press Power>
# serial prints: Captured: NEC  addr=0x0020  cmd=0x08
ir learn bind Power         # saves button
ir learn bind Vol+          # capture and bind more buttons
ir learn stop               # cuts GROVE power
```

**Settings overlay** (long-press KEY1 in IR screen): NEW REMOTE, SELECT REMOTE, DEL REMOTE, CAPTURE BUTTON, AUTO BIND, DELETE BUTTON, RESET.

**Touch-to-bind:** while learn mode is active and a signal is captured (green `BIND` indicator), tapping an existing button rebinds it to the captured signal.

**Auto-bind:** AUTO BIND mode auto-names captures as Btn_01, Btn_02, … without requiring a `bind` command.

**Delete mode:** pulsing red `DEL` dot in title bar — tap any button to remove it from the custom remote.

### IR serial commands

| Command | Description |
|---|---|
| `ir list` | Print all scanned `.ir` files (`*` = currently loaded) |
| `ir select <N>` | Load device N and open the remote UI |
| `ir send <N> <label>` | Load device N and send the named button |
| `ir custom new [name]` | Create a new custom remote |
| `ir custom list` | List all custom remotes |
| `ir rename <new name>` | Rename the currently loaded custom remote |
| `ir del` | Delete the currently loaded remote file |
| `ir reload` | Re-scan `/irdb/` without reboot |
| `ir pin` | Show IR TX / RX GPIO numbers |
| `ir btn del <label>` | Delete a button from the loaded custom remote |
| `ir btn rename <old> <new>` | Rename a button in the loaded custom remote |
| `ir learn start` | Power GROVE and arm M5 IR Unit receiver |
| `ir learn stop` | Stop capture, cut GROVE power |
| `ir learn bind <label>` | Bind last captured signal to a button label |
| `ir learn show` | Print details of the last captured signal |

---

## RF433 Remote System

433 MHz OOK ASK remote control using M5Stack RF433T (SYN115) and RF433R (SYN531R) on a Y-cable.

**RF433 is disabled by default.** Enable from Battery screen → long-press KEY1 → **RF433**, or via `rf433 enable` serial command.

**GROVE wiring:**

| GROVE signal | GPIO | Module |
|---|---|---|
| G4 (Yellow) | GPIO 4 | RF433R — demodulated RX |
| G5 (White) | GPIO 5 | RF433T — TX data input |

### File format

Custom remotes are saved as `.sub` files in `/rf433db/Custom/`:

```
Filetype: Nesso SubGhz Remote
Version: 1
Frequency: 433920000
#
name: Button1
type: raw
RAW_Data: 450 -1350 450 -450 1350 -450 ...
```

`RAW_Data` values follow Flipper SubGhz convention: positive = HIGH pulse (µs), negative = LOW pulse.

### Learn workflow

```
rf433 custom new Garage      # creates /rf433db/Custom/Garage.sub
rf433 learn start            # drives TX LOW, powers GROVE, arms receiver
<press button on physical remote>
# serial prints: [RF433] Captured: 512 pulses T=300us
rf433 learn bind Open        # saves button
rf433 learn stop             # cuts GROVE power
```

**Auto-bind** (UI): Long-press KEY1 → RF433 settings → AUTO BIND. Captures auto-named Btn_01, Btn_02, … with a 2-second pause between captures.

**Touch-to-bind:** while learn mode is active, tap an existing button to rebind it to the last captured signal.

### RF433 serial commands

| Command | Description |
|---|---|
| `rf433 list` | List all scanned `.sub` / `.433` files |
| `rf433 select <N>` | Load remote N and open UI |
| `rf433 send <N> <label>` | Transmit button from remote N |
| `rf433 custom new [name]` | Create new custom remote |
| `rf433 custom list` | List custom remotes |
| `rf433 rename <new name>` | Rename the loaded custom remote file |
| `rf433 del` | Delete the currently loaded remote file |
| `rf433 reload` | Re-scan `/rf433db/` |
| `rf433 btn del <label>` | Delete a button from the loaded custom remote |
| `rf433 btn rename <old> <new>` | Rename a button in the loaded custom remote |
| `rf433 learn start` | Drive TX LOW, power GROVE, arm receiver |
| `rf433 learn stop` | Stop capture, cut GROVE power |
| `rf433 learn bind <label>` | Bind last capture to button label |
| `rf433 learn show` | Print last captured signal info |
| `rf433 enable` | Enable RF433 function |
| `rf433 disable` | Disable RF433 function |

---

## RFID2 Unit

The **M5Stack RFID2 Unit (WS1850S)** reads ISO/IEC 14443-A cards (MIFARE Classic, Ultralight, NTAG, etc.) over I2C. Scanned UIDs are saved to `.rfid` files under `/rfid2db/`.

The RFID2 screen is hidden from navigation when the unit is not connected. Connecting after boot is supported — no reboot needed.

**GROVE PORT.CUSTOM wiring:**

| GROVE signal | GPIO | Direction |
|---|---|---|
| Yellow (SDA) | GPIO 5 | I2C data |
| Gray (SCL) | GPIO 4 | I2C clock |

**I2C address:** 0x28 | **Library:** `MFRC522_I2C` v1.0.0

### UI levels

| Level | Description |
|---|---|
| Main | NFC icon + "WAITING…"; shows UID + card type for 3 s after a read. Blinking `REC` dot in record mode. |
| File list | Scrollable browser of `.rfid` files. Tap a file to open its card list. Tap title bar to return to Main. |
| Card list | Cards stored in the loaded file. In delete mode, tapping a card removes it immediately. |

Long-press KEY1 opens the settings overlay: NEW FILE, VIEW FILES, DEL FILE, RECORD CARD, DELETE CARD, RESET.

### Record mode

When active, every new card tap is auto-saved to the loaded file as `Card_01`, `Card_02`, … Enable via the settings overlay or `rfid2 record start`.

### `.rfid` file format

```
Filetype: Nesso RFID Database
Version: 1
#
name: Card_01
uid: E3:ED:B5:19
sak: 08
type: MIFARE 1KB
```

Each card block is separated by `#`. Fields: `name` (up to 23 chars), `uid` (hex colon-separated), `sak` (hex byte), `type`.

### RFID2 serial commands

| Command | Description |
|---|---|
| `rfid2 list` | List all `.rfid` files (`*` = currently loaded) |
| `rfid2 reload` | Re-scan `/rfid2db/` without reboot |
| `rfid2 select <N>` | Load file N and open RFID screen |
| `rfid2 new [name]` | Create new `.rfid` file and start record mode |
| `rfid2 del` | Delete the currently loaded file |
| `rfid2 rename <new name>` | Rename the loaded file |
| `rfid2 card list` | List cards in the loaded file |
| `rfid2 card del <label>` | Delete a card by label or UID |
| `rfid2 card rename <old> <new>` | Rename a card |
| `rfid2 record start` | Start auto-saving scanned cards |
| `rfid2 record stop` | Stop record mode |
| `rfid2 scan` | I2C bus scan on GROVE port |
| `rfid2 probe` | Probe address 0x28 and update availability flag |

---

## Web File Manager

A browser-based file manager starts automatically when WiFi connects (port 80). Navigate to `http://<device-ip>/` in any browser. Type `webfm` in the serial terminal to print the current URL.

| Feature | How to use |
|---|---|
| Browse | Click folders; use breadcrumb or `..` row to go up |
| Upload | Click **↑ Upload**; supports multiple files at once |
| Download | Click **dl** next to any file |
| Delete | Click **del** — directories must be empty |
| New folder | Click **+ Folder** |
| Rename/move | Click a file row to select it, type new name, click **Rename** |

Uploading or deleting `.ir` files auto-triggers a rescan of `/irdb/`.

---

## Robot Control

Controller mode reads a joystick and transmits motor + button commands to a remote robot over a selectable wireless link.

- **Forward/back** (vertical stick) → both motors same power
- **Turn** (horizontal stick) → motors opposite (rotation)

### Supported controllers

| Controller | Bus | Notes |
|---|---|---|
| Adafruit seesaw mini gamepad | I2C `0x50` (main Wire) | Stick + A/B/X/Y/SELECT/START |
| M5Stack Mini JoyC HAT (STM32F030) | I2C `0x54` (HAT bus, GPIO 6/7) | Stick + button; self-powered |

One connected → it drives directly. **Both** connected → **dual-stick**: one stick drives the motors, the other is an **aux** stick (camera/turret → `auxX`/`auxY`). `PRIMARY` selects which drives (default: Mini JoyC). The Mini JoyC always follows screen rotation; the seesaw follows it only in attached `MOUNT` modes (see below).

### Wire protocol — RemoteFrame v1

Every link sends the same **15-byte little-endian frame**, encoded by the shared **[NessoLink](https://github.com/ugursayar/NessoLink)** library (`nessoEncode()`); the robot receiver decodes it with the same library (`nessoDecode()`). Magic + version + CRC let it validate:

| off | field | type | notes |
|---|---|---|---|
| 0 | magic | u8 | `0xA5` |
| 1 | version | u8 | `1` |
| 2 | seq | u8 | rolling sequence (dedup / loss detection) |
| 3–4 | leftMotor | i16 | −255..255 |
| 5–6 | rightMotor | i16 | −255..255 |
| 7–8 | auxX | i16 | aux stick X (0 unless dual-stick) |
| 9–10 | auxY | i16 | aux stick Y |
| 11–12 | buttons | u16 | bitfield, 1 = pressed: A=0, B=1, X=2, Y=3, SELECT=4, START=5, STICK=6 |
| 13 | flags | u8 | bit0 = aux stick present |
| 14 | crc8 | u8 | poly `0x07`, init `0x00`, over bytes 0..13 |

Sent continuously at ~10 Hz (even when centered) so the receiver can implement a failsafe (stop motors if no valid frame for N ms).

### Transport links (`TX LINK`)

| Link | Status | Notes |
|---|---|---|
| **WiFi-UDP** | default | to `robot_ip:udp_port` (8889); lowest latency, same LAN |
| **BLE** | working | notifies the NESSO BLE UART characteristic; needs a connected central (WiFi contends — one radio) |
| **WiFi-TCP** | minimal | lazy connect to `robot_ip:tcp_port` (8890) |
| **LoRa** | working (low-rate) | async (non-blocking) via NessoLink; the SX1262 is shared with the LoRa scanner, so it's armed on demand (`loraTxArm()`) and the two screens hand the radio back and forth. SF11/BW250 + EU868 ~1% duty cycle cap it at a few Hz — a command link, not live driving. Matching receiver: the **Nesso_ADV_Receiver** project (M5 Cardputer ADV + Cap LoRa 1262) |

### Configuration

Open controller settings with **long-press KEY1**, or use the `ctrl …` serial commands. All settings persist to NVS.

| Setting | Options |
|---|---|
| **CALIBRATE** | Mini JoyC: write center to its STM32 flash · seesaw: re-zero on next read |
| **DEADZONE** | Mini JoyC deadzone: 8 / 16 / 30 / 50 |
| **SWAP XY / INVERT X / INVERT Y** | drive-stick axis transforms |
| **TX LINK** | wireless link (table above) |
| **MOUNT** (seesaw) | `SIDE` / `BACK` — attached, follows screen rotation · `DET-PORT` / `DET-LAND` — detached, fixed to the stick |
| **PRIMARY** (dual-stick) | which stick drives: `JOYC` / `PAD` |

---

## Serial Command Reference

Connect at **115200 baud**. Commands work over both USB serial and BLE UART (`nesso_terminal.py`). Type `help` on the device for the full list.

### Navigation / General

| Command | Description |
|---|---|
| `help` | Print full command reference |
| `status` | Current state summary (screen, WiFi, BT, BLE-UART) |
| `next` | Advance to next screen (same as KEY1) |
| `prev` | Go to previous screen (same as KEY2) |
| `goto <screen>` | Jump to screen: `main`, `controller`, `bt`, `wifi`, `lora`, `rfid2`, `ir`, `rf433`, `media`, `matrix`, `vader`, `obiwan`, `battery` |
| `clock` | Print current date/time (NTP) |
| `battery` | Print voltage, percentage, charge state, uptime |
| `webfm` | Print web file manager URL |

### Controller (`ctrl`)

| Command | Description |
|---|---|
| `controller` | Print joystick position + last motor values |
| `send <L> <R>` | Send a motor command directly (−255..255) |
| `ctrl` | Show device, axis flags, link, mount, primary |
| `ctrl invertx on\|off` | Invert turn (X) axis |
| `ctrl inverty on\|off` | Invert forward/back (Y) axis |
| `ctrl swap on\|off` | Swap X/Y |
| `ctrl dz 0-3` | Mini JoyC deadzone (8 / 16 / 30 / 50) |
| `ctrl calibrate` | Calibrate Mini JoyC center → STM32 flash |
| `ctrl link udp\|ble\|tcp\|lora` | Select wireless link |
| `ctrl primary joyc\|pad` | Dual-stick: which stick drives |
| `ctrl ssmount 0-3` | Seesaw mount: `side`/`back` (attached) · `detport`/`detland` (detached) |

### Filesystem (`fs`)

| Command | Description |
|---|---|
| `fs info` | LittleFS total / used / free bytes |
| `fs ls [path]` | List directory contents (default `/`) |
| `fs cat <path>` | Print file contents |
| `fs rm <path>` | Delete a file |
| `fs mkdir <path>` | Create a directory (parent must exist) |
| `fs mv <src> <dst>` | Rename or move a file |
| `fs upload <path>` | Upload a text file — paste content then send `---END---` on its own line |

**Upload example:**
```
fs mkdir /irdb/Soundbars/Sony
fs upload /irdb/Soundbars/Sony/MyRemote.ir
<paste file contents>
---END---
```

After any `fs rm`, `fs mv`, or `fs upload` of a `.ir` file, `/irdb` is automatically rescanned.

---

## Network Configuration

| Setting | Value |
|---|---|
| UDP target | `192.168.1.27:8889` (config.json `robot_ip` / `udp_port`) |
| TCP target | `192.168.1.27:8890` (config.json `robot_ip` / `tcp_port`) |
| NTP server | `pool.ntp.org` |
| Timezone offset | UTC+3 (10800 s) |

WiFi credentials are stored in `arduino_secrets.h` (`SECRET_SSID`, `SECRET_PASS`). Copy `arduino_secrets.h.example` and fill in your credentials.
