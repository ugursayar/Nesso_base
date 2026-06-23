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

### USB-CDC serial must not block boot

`Serial` is USB Serial/JTAG (`ARDUINO_USB_CDC_ON_BOOT=1`, `ARDUINO_USB_MODE=1`), **not** a UART. When its TX ring fills with no host draining it, `HWCDC::write()` blocks up to ~20 × `tx_timeout_ms` (default 100 ms) **per call**. Right after flashing the CDC still reads as "connected" but nothing reads it, so the boot-time `serialPrintHelp()` dump would stall `setup()` for *minutes* at the splash ("hangs until you reset it" — a manual reset masked it because the CDC then enumerated as not-connected and writes dropped). `setup()` calls **`Serial.setTxTimeoutMs(0)` immediately after `Serial.begin()`** so a full ring drops bytes instead of blocking. Keep that call — any large boot-time serial output depends on it.

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

`spkPlayBootChime()` (called from `setup()`, where blocking is acceptable) waits for the chime to drain but is **hard-capped at 1.5 s** with a `spkStop()` cleanup on timeout, so a stalled I2S DMA can never hang boot. Only runs when `spkEnabled` (NVS `spkOn`, default off).

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

### Mini JoyC HAT (M5Stack, STM32F030, I2C 0x54)

Connects via HAT bus: **SDA=GPIO6, SCL=GPIO7** (not Grove port). Self-powered by its own 200mAh LiPo — no `GROVE_POWER_EN` needed. The HAT's BAT pin IS wired to its battery, but the Nesso N1 marks BAT as "NG/Used" (its own LiPo is on the same pin), so no charging path exists between devices.

Wire switching: `miniJoyCWireHat()` / `miniJoyCWireRestore()` temporarily reassign the HP I2C to GPIO6/7.

Register mapping (verified from STM32 internal firmware source `m5stack/M5Unit-MiniJoyC-Internal-FW`):
- **Reg 0x00** → raw PA.2 = **VERTICAL axis** (push-UP decreases value; confusingly named "X" in firmware)
- **Reg 0x02** → raw PA.1 = **HORIZONTAL axis** (push-RIGHT increases value; named "Y" in firmware)
- **Reg 0x10/0x11** → calibrated VERTICAL ±512 (STM32 applies x_mid from flash)
- **Reg 0x12/0x13** → calibrated HORIZONTAL ±512 (STM32 applies y_mid from flash)
- **Reg 0x20** → calibrated VERTICAL ±128 (1-byte signed)
- **Reg 0x21** → calibrated HORIZONTAL ±128 (1-byte signed)

Runtime reads use **calibrated registers 0x10/0x12** (not raw ADC). The raw `horiz`/`vert` values feed the **same rotation table as the seesaw** (`switch(currentRotation)` in `readGamePad()`) so orientation adaptation is shared. Key subtlety: the HAT is **front-mounted** (rotates with the screen) while the seesaw is **back-mounted**; empirically the HAT's vertical axis is fed **as-is** (`powery = vert`, NOT negated) so that the shared table is correct in portrait. Verified at rotation 2 (the device's natural portrait orientation, ax<0): all user transform flags OFF gives correct forward/turn. No software calibration; the CALIBRATE settings action writes new midpoints to STM32 flash via reg 0x58 (vertical mid) and 0x5A (horizontal mid).

Default axis transforms (`ctrlSwapXY`/`ctrlInvertX`/`ctrlInvertY`/`ctrlDeadzoneIdx`) are all OFF / default and **persisted to NVS** (keys `ctrlSXY`/`ctrlIX`/`ctrlIY`/`ctrlDZ`) on APPLY. Adjustable from the Controller settings screen (KEY1 long-press) or via serial: `ctrl invertx|inverty|swap on|off`, `ctrl dz 0-3`, `ctrl calibrate`. The flags are global (all rotations); landscape may need per-axis tuning since they can't differ per-rotation.

### Remote control transport layer

All control commands flow through `transmitRemoteCommand(L, R)` → a transport dispatcher selected by `remoteTransportIdx` (`enum RemoteTransport`, persisted to NVS key `txLink`). The payload is the **15-byte versioned RemoteFrame v1**, encoded by the **NessoLink library** (`#include <NessoFrame.h>` → `NessoFrame` + `nessoEncode()`) — the same codec the robot receiver uses (repo: `github.com/ugursayar/NessoLink`, installed at `D:\packages\arduino\user\libraries\NessoLink`). The firmware only fills a `NessoFrame` (motors from `transmitCmd`, `remoteAux*`, `remoteButtonBits()`, `++remoteSeq`); the byte layout, CRC-8, and `NESSO_*` constants live in the library. Wire layout (little-endian, CRC-8 checked):

| off | field | type | notes |
|----|----|----|----|
| 0 | magic | u8 | `0xA5` |
| 1 | version | u8 | `NESSO_PROTO_VER` (1) |
| 2 | seq | u8 | rolling, for dedup/loss detection |
| 3–4 | leftMotor | i16LE | -255..255 |
| 5–6 | rightMotor | i16LE | -255..255 |
| 7–8 | auxX | i16LE | right-stick X, -515..515 (0 until dual-stick) |
| 9–10 | auxY | i16LE | right-stick Y (0 until dual-stick) |
| 11–12 | buttons | u16LE | bitfield, 1=pressed: A=0,B=1,X=2,Y=3,SEL=4,START=5,STICK=6 |
| 13 | flags | u8 | bit0=right-stick present; rest reserved |
| 14 | crc8 | u8 | poly 0x07, init 0x00, over bytes 0..13 |

Sent at the readGamePad rate (~10 Hz) including when centered, so the receiver can implement a failsafe timeout. **The receiver firmware must parse this frame** — use the same NessoLink library (`nessoDecode()`). Backends:
- **`TX_WIFI_UDP`** (default, fully working) — `udp` to `targetIpAddress:udpPort`. Unchanged from the original single-path implementation.
- **`TX_BLE`** (working) — routes through the library's `NessoBleLink` (`#include <NessoLinkBLE.h>`, global `bleLink`). `txBle()` lazily injects two sinks into `bleLink.begin()`: a notify sink (writes/notifies the existing NESSO BLE UART TX characteristic `pBLETxChar`) and a ready predicate (`bleUartReady && pBLETxChar && btConnected`). The link never owns the BLE stack.
- **`TX_WIFI_TCP`** (minimal) — lazy `NetworkClient` connect to `targetIpAddress:tcpPort` (default 8890, in config.json `tcp_port`), `setNoDelay(true)`.
- **`TX_LORA`** (live) — routes through the library's `NessoLoRaLink` (`#include <NessoLinkLoRa.h>`, global `loraLink`), which does the async `startTransmit()` + duty-cycle rate gating (non-blocking; drops frames rather than stalling). `txLora()` lazily calls `loraTxArm()` the first time it is needed: it powers the SX1262, `lora.begin()`s it (unless the scanner already did), points the antenna switch at TX, and hands the radio to `loraLink.begin(&lora, LORA_TX_MIN_INTERVAL_MS)` — which installs the **TxDone** DIO1 action. The scanner and the controller are **mutually-exclusive screens sharing one radio and one DIO1 line**, so the radio-lifecycle handoff is managed on entry: `initLora()` clears `loraTxArmed`, re-points the antenna at RX, and re-installs the scanner's **RxDone** `loraISR`; the next `txLora()` re-arms. Every `txLora()` re-runs `SPI.begin(…, LORA_CS)` because the display reconfigures the shared bus. At SF11/BW250 a frame is ~150–200 ms airtime and EU868 ~1% duty cycle caps it at a few Hz — a low-rate command/telemetry link, not a live joystick path. Matching receiver: the NessoLink `CardputerAdvLoRaReceiver` example (M5 Cardputer ADV + Cap LoRa 1262).

Selectable from Controller settings ("TX LINK" row, item 6) or serial `ctrl link udp|ble|tcp|lora`. ESP32-C6 has one radio — BLE and WiFi coexist but contend, so BLE mode generally wants WiFi off.

### Dual-stick (seesaw + Mini JoyC together)

When both are present (`joystickAvailable && miniJoyCAvailable`) the controller runs **dual-stick**: one stick drives (full pipeline → `leftMotor`/`rightMotor`), the other is the **aux** stick (no drive flags → `auxX`/`auxY` + frame `flags` bit0). `readGamePad()` reads both each cycle via `readSeesawAxes()` / `readMiniJoyCAxes()`, computes a **screen-relative (rx,ry) per physical stick**, then assigns drive/aux roles. `dualPrimaryMiniJoyC` (NVS `dualPriMJC`, default true = Mini JoyC drives) selects which is primary; set via serial `ctrl primary joyc|pad`. Both stick buttons are read and merged into the frame bitfield. `renderController()` branches to `renderControllerDual()` showing two stick discs (DRIVE/AUX) when both are connected.

**Per-stick rotation adaptation:** the Mini JoyC *always* rotation-adapts (front-mounted). The seesaw uses a **stick-mount** model (`seesawMountIdx`, NVS `ssMount`): a base transform (`applySeesawOrient()`) → then, **only for ATTACHED mounts**, the screen-rotation table (`applyStickRotation()`). The attached/detached distinction is the key insight — an attached stick rotates *with* the device so it must reflect screen orientation; a detached stick is held independently so it must **not** follow device rotation.

| `seesawMountIdx` | label | base transform | screen rotation |
|----|----|----|----|
| 0 `SS_STICK_SIDE` (default) | SIDE | bx=−px, by=py | **ON** (attached) |
| 1 `SS_STICK_BACK` | BACK | bx=px, by=py | **ON** (attached) |
| 2 `SS_DETACHED_PORTRAIT` | DET-PORT | bx=−px, by=py | OFF (detached) |
| 3 `SS_DETACHED_LANDSCAPE` | DET-LAND | bx=py, by=px | OFF (detached) |

`seesawAppliesRotation()` returns true for side/back only. SIDE/BACK are confirmed working (rotation-adapting). The DETACHED base signs are best-guesses — verify by testing. Selectable via the "MOUNT" settings row (shown only when a seesaw is present) or serial `ctrl ssmount 0-3` (side/back/detport/detland). The Mini JoyC is unaffected.

Bus cost: reading both per cycle includes the seesaw's 40 ms `analogRead` averaging **plus** a Mini JoyC `Wire.end()/begin()` HAT-bus switch — fine at the ~10 Hz poll.

`renderControllerDual()` draws the two discs **side by side** (left = drive, right = aux) in both portrait and landscape, with an info strip below; landscape uses smaller discs (r=24) and a compact button row to fit the short height.

### Controller button reading (don't let it stick or drop)

Three traps, all fixed and all easy to reintroduce:
- **Seesaw read must be probe-guarded.** `Adafruit_seesaw::digitalReadBulk()` reads into an *uninitialised* buffer and ignores the I2C result — on a NAK it returns stack garbage, which (buttons are active-LOW) latches random "pressed" bits into `gamepadButtons` that persist on screen *and* in the TX frame (stuck buttons). `readGamePadButtons()` probes `0x50` with `Wire.endTransmission()` first and only trusts `digitalReadBulk()` when the seesaw ACKs; otherwise it keeps the last good snapshot. The Mini JoyC HAT-bus re-pinning makes these NAKs more likely.
- **Sample buttons in lockstep with the frame.** `readGamePadButtons()` runs in the same 100 ms tick as `readGamePad()` and **before** it (which builds/sends the frame). A slower separate timer left up to its period of stale "pressed" frames after release.
- **`remoteButtonBits()` must MERGE both sources, not `else if`.** The seesaw block and the Mini JoyC `STICK` bit are independent `if`s — with `else if` the JoyC stick button is silently dropped from the frame in dual-stick mode (the device screen still showed it because it reads `miniJoyCBtn` directly). The Mini JoyC button is read inside `readMiniJoyCAxes()` (the same HAT-bus window as its axes — one re-pin per cycle) and fails safe to "released" on a bad read (`miniJoyCReadByte` → `0xFF`), so it can't stick on the way the seesaw can.

### BLE HID gamepad mode ("standard controller")

`hidGamepadEnabled` (NVS `hidPad`, **default OFF**) makes the device a **standard BLE HID gamepad** — `readGamePad()` calls `hidSendGamepad()` (HID report: 16 buttons + 4×16-bit axes, see `HID_GAMEPAD_REPORT_MAP`) instead of `transmitRemoteCommand()`. Toggle via serial `ctrl hid on|off`. **Entirely guarded** — when off, no HID code runs and existing behaviour is byte-identical.

Non-obvious constraints (the reasons it's gated, not free-running):
- **Bonding conflict.** HID-over-GATT requires encryption/bonding, but the BLE stack is deliberately `ESP_LE_AUTH_NO_BOND` (to avoid stale-key GATT errors on reconnect). `btInitStack()` switches to `ESP_LE_AUTH_REQ_SC_BOND` **only** when HID is on — so HID and the normal UART/scan use case can't both have their preferred security at once.
- **Created at init only.** The `BLEHIDDevice` is built inside `btInitStack()`, which runs once. Enabling HID therefore needs a **reboot with BT started** (BT-on-boot ON). The serial command saves the flag and says so.
- **No USB HID.** ESP32-C6 has only a fixed-function USB Serial/JTAG controller (no USB-OTG), so a USB gamepad is impossible on this silicon — BLE HID is the only standard-controller path. C6 is also BLE-only (no Classic BT), so consoles won't pair; works on PC (DirectInput — Steam Input maps it) / Android.
- **Untested on hardware** as written — report-map/axis-sign/bonding details may need iteration with a real host.

The Controller **settings screen** is device-aware: `controllerSettingsItemCount()` = base 6 (CALIBRATE, DEADZONE, SWAP XY, INVERT X, INVERT Y, TX LINK) **+ MOUNT** when a seesaw is present (index 6) **+ PRIMARY** in dual mode (index 7). Fixed case indices stay valid because index 6 is only reachable with a seesaw and 7 only in dual. Title is "DUAL STICK SETTINGS" when both present. CALIBRATE/DEADZONE are active whenever `miniJoyCAvailable`. The count is centralized in that helper — render, tap, cursor-wrap (`onKey2Short`), and swipe-bound (`onSwipe`) all call it. Serial equivalents: `ctrl primary joyc|pad`, `ctrl ssmount 0-3`, `ctrl dz`, `ctrl calibrate`.
