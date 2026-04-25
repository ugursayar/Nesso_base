# Nesso N1 — Usage Guide

## Navigation

| Action | Effect |
|---|---|
| **KEY1** short press | Next screen |
| **KEY2** short press | Previous screen |
| **KEY1** long press | Open settings overlay (Battery screen → device settings) |
| **KEY2** long press | Close settings without saving |
| **Swipe left / right** | Next / previous screen |
| **Swipe up / down** | Scroll lists, remote buttons, or settings rows |
| **Tilt device** | Auto-rotates between portrait and landscape |

Settings overlays have **APPLY** and **CANCEL** buttons at the bottom.

---

## Screens

### 1 — Clock / WiFi (`FUNCTION_MAIN`)

Current time synced via NTP (UTC+3, pool.ntp.org) in large text with the date below. The header bar always shows battery, WiFi, and Bluetooth icons regardless of which screen is active.

---

### 2 — Controller (`FUNCTION_CONTROLLER`)

Reads an **Adafruit seesaw mini gamepad** (I2C 0x50) and sends motor commands to the robot via **UDP to 192.168.1.27:8889** (configurable in `/config.json`).

| Stick direction | Robot behavior |
|---|---|
| Horizontal | Forward / reverse (both motors equal power) |
| Vertical | Differential steering (motors in opposite directions → rotation) |

The joystick zero point is calibrated on the first read after entering this screen.

---

### 3 — Bluetooth Scanner (`FUNCTION_BT`)

Continuously scans for BLE advertising packets and lists devices sorted by signal strength.

**List view:**
- Each row: device name (or MAC if unnamed) + RSSI color-coded green / orange / red
- White = seen within 3 s; gray = older
- **Tap a row** to open the detail view
- **Swipe up/down** to scroll

**Detail view:**
- Full MAC, RSSI, connectable flag, last-seen time
- Debug mode adds raw manufacturer data bytes
- **Tap anywhere** to return to the list

**Settings** (KEY1 long press):

| Setting | Options | Description |
|---|---|---|
| SCAN MODE | ACTIVE / PASSIVE | Active sends scan requests, may get device names. Passive listens only. |
| RSSI FILTER | −70 / −80 / −90 dBm / OFF | Hides devices weaker than threshold. |
| DEBUG LOG | ON / OFF | Show raw manufacturer data in detail view. |
| PAIR MODE | ON / OFF | Advertise Nesso N1 as "NESSO" and accept BLE connections (enables BLE UART). |
| STARTUP | ON / OFF | Auto-init BLE stack at boot. |

> **Why do devices show as MAC addresses?** Phones on the OS Bluetooth settings screen use *Classic Bluetooth (BR/EDR)* discoverability — a different protocol. This scanner only sees *BLE* advertising packets. Phones that do advertise BLE typically rotate random MACs and omit their name for privacy.

Up to **10 devices** tracked at once. Oldest-seen entry is evicted when full.

---

### 4 — WiFi Scanner (`FUNCTION_WIFI`)

Scans for nearby 2.4 GHz/5 GHz WiFi networks.

- **Tap the scan button** (or tap the screen when no scan is running) to start
- **Tap the blinking dot** to cancel a running scan
- Each row shows: SSID, RSSI, channel, security type
- **Swipe up/down** to scroll

**Settings** (KEY1 long press):

| Setting | Options | Description |
|---|---|---|
| DEBUG | ON / OFF | Print each discovered AP to the serial terminal. |
| AUTO SCAN | ON / OFF | Restart scan automatically after each result. |

---

### 5 — LoRa Scanner (`FUNCTION_LORA`)

Listens for LoRa packets (Meshtastic-compatible format by default) and logs them newest-first.

Each entry: RSSI · SNR · size · decoded ASCII text · age

**Settings** (KEY1 long press):

| Setting | Options | Description |
|---|---|---|
| PRESET | LONG_FAST / LONG_SLOW / MED_FAST / SHORT_FAST | Bandwidth + spreading factor. |
| FREQUENCY | EU 868 / US 915 variants | RF center frequency. |
| AUTO REPLY | ON / OFF | Send a short ACK when a packet arrives. |
| DEDUP | ON / OFF | Suppress packets with the same ID received twice. |

---

### 6 — IR Remote (`FUNCTION_IR`)

Universal IR blaster backed by a LittleFS file library. Device files (`.ir` format, Flipper-Zero compatible) are stored under `/irdb/` on the device.

#### Directory browser (level 0)

Scrollable folder tree starting at `/irdb/`. Folders are shown with `>` prefix; tap to enter. Tap a `.ir` file to load it and open the remote view. Tap the title bar to go up one directory. The last-opened file is persisted to NVS and reopened automatically on the next boot.

#### Remote view (level 1)

Buttons are arranged automatically by label into priority groups:

```
┌──────────────────────────┐
│         Power            │  full-width · red
├─────────────┬────────────┤
│   Vol  +    │   Chan +   │  half · green / blue
├─────────────┼────────────┤
│   Vol  -    │   Chan -   │
├─────────────┴────────────┤
│           Mute           │  full-width
├──────────────────────────┤
│       ▲  Up              │
│ ◀ Left │  OK  │ Right ▶  │  D-pad cross, corners blank
│       ▼  Down            │
├──────────────────────────┤
│   Menu    │    Home      │
├──────────────────────────┤
│  ◀◀  │  ▶⏸  │  ▶▶       │  media · amber
├──────────────────────────┤
│  7   │   8   │   9       │
│  4   │   5   │   6       │  numpad · grey
│  1   │   2   │   3       │
│        0                 │
├──────────────────────────┤
│ Source   │   Input       │  generic catch-all
└──────────────────────────┘
```

- **Tap a button** to transmit that signal
- **Swipe up/down** to scroll when buttons extend off-screen
- **Tap the title bar** to return to the directory browser
- A blinking red dot in the title bar indicates an IR transmission in progress

#### Learn mode — M5Stack IR Unit (U002)

Plug the M5 IR Unit into the **GROVE port** (G4 = receive, G5 = transmit). Use serial commands to record signals from any physical remote and build custom button layouts.

**Workflow — create a new custom remote:**
```
ir custom new <name>       # creates /irdb/Custom/<name>.ir and loads it
ir learn start             # powers GROVE, arms receiver (amber dot blinks)
<point remote at M5 unit, press Power>
ir learn bind Power        # captures and saves — button appears in UI
<press Vol+ on remote>
ir learn bind Vol+
ir learn stop              # cuts GROVE power when done
```

**Workflow — add buttons to an existing custom remote:**
```
ir custom list             # see saved remotes
ir select <N>              # load by index (from 'ir list')
ir learn start
ir learn bind <label>
ir learn stop
```

**Touch-to-bind:** while learn mode is active and a signal has just been captured (green `BIND` indicator), tapping an existing button in the remote UI **rebinds** it to the captured signal instead of transmitting.

**Duplicate handling:** if the same IR code is captured and bound to a new label, the old button with that code is automatically renamed. Library files (outside `/irdb/Custom/`) cannot be overwritten.

---

### 7 — Media (`FUNCTION_MEDIA`)

Three sub-screens merged into one; **swipe up/down** to cycle between them:

- **Matrix rain** — green digital rain animation
- **Vader** — Darth Vader artwork + Imperial March on the buzzer
- **Obi-Wan** — Obi-Wan Kenobi artwork + Star Wars theme on the buzzer

**Tap the screen** to toggle the buzzer melody on/off.

---

### 8 — Battery (`FUNCTION_BATTERY`)

| Field | Description |
|---|---|
| Voltage | Color-coded: green >3.7 V · orange 3.3–3.7 V · red <3.3 V |
| Charge % | Calculated from voltage curve (piecewise linear LiPo model) |
| Uptime | Time since last boot |
| Status | CHARGING / FULL / IDLE / PRE-CHARGE |

**Long press KEY1** opens the device settings overlay:

| Setting | Options | Description |
|---|---|---|
| DIM | 30 s / 60 s / 2 min / OFF | Dim display after inactivity. |
| SLEEP | 2 min / 5 min / 10 min / OFF | Turn display off after inactivity. |
| LOW BAT | 5% / 10% / OFF | Shut display off at critically low battery. |
| UI CLICK | ON / OFF | Buzzer click on key / tap events. |

---

## Header Icons (always visible)

| Icon | Meaning |
|---|---|
| Battery bar | Charge level; color follows voltage threshold |
| WiFi symbol | Connected (white) / connecting (gray) / off / disconnected (red) |
| BT symbol | Blue blink = scanning; dim = idle but initialized; gray = off |

---

## Web File Manager

When WiFi is connected, a file manager is available at `http://<device-ip>/` (port 80). Type `webfm` in the serial terminal to print the current URL.

| Feature | How |
|---|---|
| Browse | Click folder names; `..` row goes up |
| Upload | Click **↑ Upload**; multiple files at once supported |
| Download | Click **dl** next to any file |
| Delete | Click **del** (directories must be empty first) |
| New folder | Click **+ Folder** |
| Rename / move | Click a file row to select it, type a new name, click **Rename** |

Uploading or deleting `.ir` files automatically triggers a rescan of `/irdb/`.

---

## Serial Command Interface

Commands are accepted simultaneously over:
- **USB Serial** — 115200 baud
- **BLE UART** — Nordic UART Service (NUS); enable Pair Mode in BT settings, then connect via nRF UART or nRF Connect

Type `help` at any time for the full reference. The relevant command set is printed automatically when navigating to each screen.

### General

| Command | Description |
|---|---|
| `help` | Full command reference |
| `status` | Screen, WiFi, BT, and BLE UART state |
| `next` / `prev` | Navigate screens |
| `goto <screen>` | Jump to: `main` `controller` `bt` `wifi` `lora` `ir` `media` `battery` |
| `clock` | Current time (NTP, UTC+3) |
| `battery` | Voltage, charge %, status, uptime |
| `send <L> <R>` | Transmit motor command directly (−255 to 255) |
| `music on\|off` | Toggle buzzer melody (media screen) |
| `webfm` | Print web file manager URL |
| `imu` | Single accelerometer snapshot |
| `imu debug on\|off` | Stream IMU readings every 300 ms |

### Bluetooth (`bt`)

| Command | Description |
|---|---|
| `bt list` | List discovered BLE devices (strongest first) |
| `bt detail <N>` | Full detail for device N |
| `bt scan on\|off` | Start / stop BLE scanner |
| `bt pair on\|off` | Toggle BLE advertising (Pair Mode) |
| `bt mode active\|passive` | Scan mode |
| `bt filter -70\|-80\|-90\|off` | RSSI threshold |

### WiFi (`wifi`)

| Command | Description |
|---|---|
| `wifi scan` | Start a WiFi network scan |
| `wifi list` | Print last scan results |
| `wifi ssid <ssid>` | Set WiFi SSID (persisted to NVS) |
| `wifi pass <pass>` | Set WiFi password |
| `wifi connect` | Reconnect with current credentials |
| `wifi status` | Connection status and IP |
| `wifi debug on\|off` | Log every discovered AP |
| `wifi auto on\|off` | Auto-restart scan after each result |

### LoRa (`lora`)

| Command | Description |
|---|---|
| `lora list` | Print packet log (RSSI, SNR, text) |
| `lora listen on\|off` | Start / stop receiver |
| `lora send <text>` | Transmit a text packet |
| `lora preset <0-3>` | 0=LONG_FAST 1=LONG_SLOW 2=MED_FAST 3=SHORT_FAST |
| `lora freq <N>` | Select frequency index |
| `lora reply on\|off` | Toggle auto-reply ACK |
| `lora dedup on\|off` | Toggle duplicate suppression |

### Filesystem (`fs`)

| Command | Description |
|---|---|
| `fs info` | LittleFS total / used / free bytes |
| `fs ls [path]` | List directory (default `/`) |
| `fs cat <path>` | Print file contents |
| `fs rm <path>` | Delete a file |
| `fs mkdir <path>` | Create a directory |
| `fs mv <src> <dst>` | Rename or move a file |
| `fs upload <path>` | Upload a text file — paste content, then send `---END---` on its own line |

**Upload workflow** — add a `.ir` file without reflashing:
```
fs mkdir /irdb/Soundbars/Sony
fs upload /irdb/Soundbars/Sony/MyRemote.ir
<paste file contents>
---END---
```
Auto-rescans `/irdb/` on completion.

### IR Remote (`ir`)

| Command | Description |
|---|---|
| `ir list` | List all `.ir` files in `/irdb/` (`*` = currently loaded) |
| `ir select <N>` | Load device N and open the remote UI |
| `ir reload` | Re-scan `/irdb/` without reboot |
| `ir send <N> <label>` | Load device N and send the named button |
| `ir pin` | Show IR TX / RX GPIO numbers |
| `ir custom new [name]` | Create a new custom remote at `/irdb/Custom/<name>.ir` |
| `ir custom list` | List all custom remotes |
| `ir learn start` | Power GROVE port and arm M5 IR Unit receiver |
| `ir learn stop` | Stop capture and cut GROVE power |
| `ir learn bind <label>` | Bind last captured signal to a button label |
| `ir learn show` | Print last captured signal details |

---

## Debug Output

Set `#define DEBUG 1` near the top of `Nesso_base.ino` and open the serial monitor at **115200 baud** to see verbose runtime logs. Serial commands work regardless of the DEBUG setting.
