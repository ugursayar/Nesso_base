# Nesso N1 — Usage Guide

## Navigation

| Action | Effect |
|---|---|
| **KEY1** short press | Next screen |
| **KEY2** short press | Previous screen |
| **KEY1** long press | Open settings (BT / LoRa screens only) |
| **KEY2** long press | Close settings without saving |
| **Swipe left/right** | Next / previous screen |
| **Swipe up/down** | Scroll list (BT, LoRa) or scroll settings rows |
| **Tilt device** | Auto-rotates between landscape and portrait |

Settings panels have **APPLY** (left button) and **CANCEL** (right button) at the bottom, tappable.

---

## Screens

### 1 — Clock / WiFi (`FUNCTION_MAIN`)

Displays the current time synced via NTP (UTC+3, pool.ntp.org) and WiFi connection status in the header. The clock shows hours and minutes in large text; the date is shown below. The header always shows battery, WiFi, and Bluetooth icons regardless of which screen is active.

---

### 2 — Battery (`FUNCTION_BATTERY`)

Shows:
- **Voltage** in volts (color-coded: green >3.7 V, orange 3.3–3.7 V, red <3.3 V)
- **Charge level** as a percentage
- **Uptime** since last boot
- Charging state (charging / discharging / full)

Charging is enabled automatically when external power (USB) is connected. It stops at ≥80% while charging to preserve battery health, and again at 99% when topping off.

---

### 3 — Controller (`FUNCTION_CONTROLLER`)

Reads an **Adafruit seesaw mini gamepad** (I2C, address 0x50) and sends motor commands to the robot via **UDP to 192.168.1.27:8889**.

The command is a `ControlCommand` struct: two `int16_t` values (range −255 to +255).

| Stick direction | Robot behavior |
|---|---|
| Horizontal | Forward / reverse (both motors equal power) |
| Vertical | Differential steering (motors opposite → rotation) |

The joystick zero point is calibrated on the first read after entering this screen.

---

### 4 — Bluetooth Scanner (`FUNCTION_BT`)

Continuously scans for nearby Bluetooth Low Energy (BLE) devices and lists them sorted by signal strength (strongest at top).

**List view:**
- Each row shows the **device name** (or MAC address if the device doesn't broadcast a name) and **RSSI** in dBm, color-coded:
  - Green: > −60 dBm (strong)
  - Orange: −60 to −80 dBm (medium)
  - Red: < −80 dBm (weak)
- Newly seen devices (within 3 s) appear in white; older ones in gray
- Scroll up/down with swipe to see more than the visible rows
- **Tap any row** to open the detail view for that device

**Detail view** (after tapping a device):
- Shows full MAC address, RSSI, whether the device accepts connections, and when it was last seen
- If Debug Log is ON in settings, also shows raw manufacturer data bytes
- **Tap anywhere** to return to the list

**Why do many devices show a MAC address instead of a name?**  
Two reasons:

1. **Classic Bluetooth vs BLE.** When you make a phone "discoverable" via the OS Bluetooth settings, that enables *Classic Bluetooth (BR/EDR)* discoverability — which is a completely different radio protocol. The Nesso BLE scanner can only see *BLE advertising packets*; it cannot see Classic Bluetooth devices at all. To see a phone in the list, the phone must be running an app that actively advertises over BLE (e.g. nRF Connect in advertiser mode).

2. **Privacy.** Even phones that do advertise over BLE typically omit their name from advertising packets and use rotating random MAC addresses. This is a privacy feature built into Android and iOS. The name may appear in a scan response if active scan mode is enabled, but many devices don't respond to scan requests.

**"CONN" / "Connectable: YES"** means the device advertises that it accepts BLE connection requests. Broadcast-only devices (beacons, sensors) set this to NO.

**Pair Mode (advertising as NESSO):**  
When Pair Mode is ON, the Nesso N1 advertises itself as a connectable BLE device named "NESSO". Other devices can discover and connect to it. The Nesso has a minimal GATT server (no application services), so the remote device will connect and see an empty service list. This is enough for basic BLE connectivity; full GATT services are not implemented.

Note: making the Nesso "discoverable" over Classic Bluetooth (the standard phone pairing screen) is not supported — it is a BLE-only device.

**Settings** (KEY1 long press):

| Setting | Options | Description |
|---|---|---|
| SCAN MODE | ACTIVE / PASSIVE | Active mode sends scan requests and may receive device names in scan responses. Passive mode only listens to raw advertising packets (lower power, fewer names). |
| RSSI FILTER | −70 dBm / −80 dBm / −90 dBm / OFF | Hides devices weaker than the threshold. OFF shows everything. |
| DEBUG LOG | ON / OFF | Shows raw manufacturer data bytes in the detail view. |
| PAIR MODE | ON / OFF | Makes the Nesso N1 advertise as "NESSO" and accept incoming BLE connections. |

Up to **10 devices** are tracked at once. When the log is full, the oldest-seen device is evicted to make room for new ones.

---

### 5 — LoRa Scanner (`FUNCTION_LORA`)

Listens for incoming LoRa packets on the configured frequency and displays them as a scrollable log (newest at top).

Each entry shows:
- RSSI (dBm) and SNR (dB) of the received packet
- Packet size in bytes
- Raw content decoded as printable ASCII (non-printable bytes shown as `.`)
- Time since received

**Settings** (KEY1 long press):

| Setting | Options | Description |
|---|---|---|
| PRESET | LONG_FAST / LONG_SLOW / MEDIUM / SHORT_FAST | LoRa modulation preset (bandwidth + spreading factor). |
| FREQUENCY | Various | RF center frequency. |
| AUTO REPLY | ON / OFF | Automatically sends a short reply packet when a packet is received. |
| DEDUP | ON / OFF | Suppresses duplicate packets with the same content received in quick succession. |

---

### 6 — IR Remote (`FUNCTION_IR`)

Turns the Nesso N1 into a universal IR blaster via the built-in IR LED on **GPIO 9** (`IR_TX_PIN`). Supports 56 devices across four categories (TVs, AV receivers, soundbars, projectors).

#### Navigation

The IR UI has four levels:

| Level | Screen | How to advance |
|---|---|---|
| 0 | **Brand list** | Tap a brand row |
| 1 | **Device type list** | Tap a type row |
| 2 | **Device list (checkboxes)** | Tap rows to select/deselect; tap **TEST** on the right edge of a row to fire the Power code immediately; tap **DONE** when ready |
| 3 | **Remote view** | Tap on-screen buttons to transmit IR |

- **Tap the title bar** at any level to go back one level.
- **Swipe up/down** in lists to scroll.
- **Swipe up/down** in the remote view to cycle between multiple selected devices.
- The selection is **persisted to NVS** and survives power-off.

#### Remote Button Layouts

Three layouts are used automatically based on which codes the device supports:

- **Layout A — Basic:** POWER · VOL+ / CH+ · VOL− / CH− · MUTE (+ INPUT if available)
- **Layout B — Basic + ext:** adds MENU and OK/BACK row
- **Layout C — Full nav:** POWER · VOL+/↑/CH+ · ←/OK/→ · VOL−/↓/CH− · MUTE/INPUT/MENU/BACK

#### Serial Commands

```
ir list                   — list all devices with their index numbers
ir send <N> <func>        — send a function to device index N
                            func: power | volup | voldn | mute | chup | chdn
ir select <N>             — add device N to the selection
ir deselect <N>           — remove device N from the selection
```

---

#### Supported Device List (56 entries)

##### TVs (31 devices)

| Index | Brand | Model / Variant | Protocol | Bits | Buttons |
|---|---|---|---|---|---|
| 0 | ADLER | 2/-1 | NEC | 32 | POWER/VOL/MUTE (no CH) |
| 1 | Arcelik | standard | NEC | 32 | Basic only |
| 2 | Beko | standard | NEC | 32 | Basic only |
| 3 | Coby | 0/127 | NEC | 32 | Basic only |
| 4 | Emerson | standard | NEC | 32 | Basic only |
| 5 | Fast | 28/-1 | RC5 | 12 | POWER/VOL/MUTE/CH− (no CH+) |
| 6 | Fisher | 56/-1 | NEC | 32 | Basic only |
| 7 | Grundig | RC5 | RC5 | 12 | Basic + INPUT/MENU/BACK |
| 8 | Haier | standard | NEC | 32 | Basic only |
| 9 | Hisense | standard | NEC | 32 | Basic only |
| 10 | Hitachi | standard | NEC | 32 | Basic only |
| 11 | Insignia | 134/5 | NEC | 32 | POWER/VOL−/MUTE/CH (no VOL+) |
| 12 | JVC | standard | JVC | 16 | Basic only |
| 13 | LG | OLED / NanoCell | NEC | 32 | Full nav |
| 14 | Loewe | RC5 | RC5 | 12 | Basic only |
| 15 | LXI | 4/-1 | NEC | 32 | Basic only |
| 16 | Magnavox | RC5 | RC5 | 12 | Basic only |
| 17 | Memorex | 4/-1 | NEC | 32 | Basic only |
| 18 | Mitsubishi | Sharp IR | SHARP | 15 | Basic only |
| 19 | Panasonic | TX series | NEC | 32 | Basic only |
| 20 | Philips | RC5 | RC5 | 12 | Basic + INPUT/MENU/BACK |
| 21 | Proton | 4/-1 | NEC | 32 | Basic only |
| 22 | Samsung | Smart / QLED | SAMSUNG | 32 | Full nav |
| 23 | Sanyo | standard | NEC | 32 | Basic only |
| 24 | Sharp | Aquos | SHARP | 15 | Basic only |
| 25 | Sony | SIRC-12 | SONY | 12 | Full nav |
| 26 | Sony | Bravia / SIRC-15 | SONY | 15 | Full nav |
| 27 | TCL | P / C series | NEC | 32 | Basic only |
| 28 | Toshiba | standard | NEC | 32 | Basic + INPUT/MENU/BACK |
| 29 | Vestel | NEC variant | NEC | 32 | Basic only |
| 30 | Vivax | 2/-1 | NEC | 32 | POWER/VOL/MUTE (no CH) |

##### AV Receivers (22 devices)

| Index | Brand | Model / Variant | Protocol | Bits | Buttons |
|---|---|---|---|---|---|
| 31 | Adcom | 26/-1 | NEC | 32 | POWER/VOL/MUTE (no CH) |
| 32 | Aiwa | 16/-1 | SONY | 12 | POWER/VOL/MUTE (no CH) |
| 33 | Arcam | 16/-1 | RC5 | 12 | POWER/VOL/MUTE/CH+ (no CH−) |
| 34 | BnK Components | 27/78 | NEC | 32 | Basic only |
| 35 | Cambridge Audio | 192/192 | NEC | 32 | POWER/VOL/MUTE/CH− (no CH+) |
| 36 | Carver | 135/123 | NEC | 32 | POWER/VOL/MUTE (no CH) |
| 37 | Cary Audio | 19/-1 | RC5 | 12 | POWER/VOL/MUTE (no CH) |
| 38 | Denon | AVR series | NEC | 32 | Basic only |
| 39 | Harman Kardon | 128/112 | NEC | 32 | Basic only |
| 40 | Integra | 210/109 | NEC | 32 | Basic only |
| 41 | Kenwood | 184/-1 | NEC | 32 | Basic only |
| 42 | Kinergetics Research | 0/-1 | RC5 | 12 | VOL/MUTE/CH (no POWER) |
| 43 | Lexicon | 130/11 | NEC | 32 | POWER/VOL/MUTE (no CH) |
| 44 | Marantz | SR / PM series | RC5 | 12 | Basic only |
| 45 | Myryad | 16/-1 | RC5 | 12 | POWER/VOL/MUTE (no CH) |
| 46 | NAD | 135/124 | NEC | 32 | POWER/VOL/MUTE (no CH) |
| 47 | Nakamichi | 130/93 | NEC | 32 | Basic only |
| 48 | Onkyo | TX-NR series | NEC | 32 | Basic only |
| 49 | Onkyo Integra | 210/109 | NEC | 32 | Basic only |
| 50 | Parasound | 3/240 | NEC | 32 | POWER/VOL/MUTE/CH+ (no CH−) |
| 51 | Pioneer | VSX series | NEC | 32 | Basic only |
| 52 | Yamaha | RX-V / RX-A | NEC | 32 | Basic only |

##### Soundbars (1 device)

| Index | Brand | Model / Variant | Protocol | Bits | Buttons |
|---|---|---|---|---|---|
| 53 | Bose | Wave / Solo | NEC | 32 | Basic only |

##### Projectors (2 devices)

| Index | Brand | Model / Variant | Protocol | Bits | Buttons |
|---|---|---|---|---|---|
| 54 | Digital Projection | 32/-1 | NEC | 32 | POWER/VOL/MUTE (no CH) |
| 55 | Epson | 131/85 | NEC | 32 | POWER/VOL/MUTE (no CH) |

#### Protocol Reference

| Protocol | Description |
|---|---|
| SAMSUNG | Samsung 32-bit |
| NEC | NEC 32-bit (NEC1/NEC2/Kaseikyo variants) |
| SONY | Sony SIRC — 12-bit or 15-bit frame |
| RC5 | Philips RC5 12-bit |
| JVC | JVC 16-bit |
| SHARP | Sharp 15-bit — uses a separate device address field |

---

### 7 — Matrix (`FUNCTION_MATRIX`)

A Matrix-style green digital rain animation. **Tap the screen** to toggle the buzzer playing a musical theme.

---

### 8 — Vader (`FUNCTION_VADER`)

Displays Darth Vader artwork. **Tap** to toggle the Imperial March on the buzzer.

---

### 9 — Obi-Wan (`FUNCTION_OBIWAN`)

Displays Obi-Wan Kenobi artwork. **Tap** to toggle the Obi-Wan theme on the buzzer.

---

## Header Icons (always visible)

| Icon | Meaning |
|---|---|
| Battery bar | Current charge level; green/orange/red by voltage |
| WiFi symbol | Connected (white) / connecting (gray) / disconnected (red) |
| BT symbol | Blue blink = scanning; dim blue = initialized but stopped; gray = off |

---

## Serial Command Interface

The Nesso N1 accepts line-based commands over two channels simultaneously:

- **USB Serial** — 115200 baud. Open any Serial Monitor (Arduino IDE, PuTTY, screen, etc.)
- **BLE UART** — Nordic UART Service (NUS). Use **nRF UART** (Android/iOS) or **nRF Connect** → UART plugin, or any NUS-compatible terminal. Enable Pair Mode in BT settings first, then connect from the client app.

On startup and each time you navigate to a new screen, the relevant commands are printed automatically. Type `help` at any time for the full list.

### Commands

| Command | Description |
|---|---|
| `help` | Full command reference |
| `status` | Current screen, WiFi, BT, and BLE UART state |
| `next` / `prev` | Navigate to the next / previous screen |
| `goto <screen>` | Jump to a screen: `main` `battery` `controller` `bt` `lora` `matrix` `vader` `obiwan` |
| `clock` | Current time and date (NTP, UTC+3) |
| `battery` | Voltage, charge %, status, uptime |
| `controller` | Current joystick position and last motor command |
| `send <L> <R>` | Transmit a motor command directly (values −255 to 255) |
| `bt list` | List all discovered BLE devices, sorted by signal strength |
| `bt detail <N>` | Full detail for device N (0-based) |
| `bt scan on\|off` | Start or stop the BLE scanner |
| `bt pair on\|off` | Toggle Pair Mode (advertise as NESSO, enables BLE UART) |
| `bt mode active\|passive` | Active scan requests device names; passive does not |
| `bt filter -70\|-80\|-90\|off` | RSSI threshold filter |
| `lora list` | List received LoRa packets with RSSI, SNR, and decoded text |
| `lora reply on\|off` | Toggle auto-reply ACK |
| `lora dedup on\|off` | Toggle duplicate packet suppression |
| `lora preset 0-3` | `0`=LONG_FAST `1`=LONG_SLOW `2`=MED_FAST `3`=SHORT_FAST |
| `music on\|off` | Play/stop the melody (matrix, vader, obiwan screens only) |

---

## Debug Output

Set `#define DEBUG 1` in `Nesso_base.ino` and open the Serial Monitor at **115200 baud** to see verbose runtime logs. Serial commands work regardless of the DEBUG setting.
