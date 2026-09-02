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
& "D:\packages\arduino\data\packages\esp32\tools\esptool_py\5.3.0\esptool.exe" --chip esp32c6 --port COM3 --baud 460800 write_flash 0x610000 littlefs.bin
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

**Both sprites live in static buffers — never the heap.** `createStatusSprite()` calls `setBuffer()` against two file-scope arrays (`statusSpriteBuf`, `headerSpriteBuf`), so a rotation change just re-points the sprites and **cannot fail**:

```cpp
alignas(4) static uint8_t statusSpriteBuf[135 * (240 - SPRITE_Y) * 2];  // portrait-sized
alignas(4) static uint8_t headerSpriteBuf[240 * SPRITE_Y * 2];
```

They are sized for **portrait** (135×218); landscape (240×113) is smaller and fits inside the same allocation. This replaced `createSprite()`/`deleteSprite()`, which was the root cause of the "empty screen after a long sleep" bug: the portrait status sprite needs **58,860 contiguous bytes**, and after hours of WiFi/BLE heap churn the measured largest free block sat within ~520 bytes of that — so the realloc failed silently on the rotation that followed a wake. Do **not** reintroduce heap allocation here; if a sprite needs to grow, grow the static buffer (and keep it sized for the portrait case). `alignas(4)` matters — LovyanGFX assumes aligned pixel buffers.

The `heap` serial command reports free / min-ever / largest-block and confirms the sprite is `static`; it's the diagnostic for anything in this area.

### IMU (BMI270) — accelerometer **and gyroscope**

The gyro is live on this board. `MyBoschSensor::begin()` in `Arduino_Nesso_N1.h` passes `BOSCH_ACCELEROMETER_ONLY`, which reads like "no gyro" but only skips the **BMM150 magnetometer** init — the core's `configure_sensor()` override enables `BMI2_ACCEL` *and* `BMI2_GYRO` at 25 Hz ODR. `IMU.readGyroscope()` works with no library patch (confirmed on hardware). There is **no magnetometer**, so there is no absolute heading: yaw is a **rate** only, and integrating it drifts. Never present an integrated yaw as a compass.

**One reader.** `imuPoll()` (called from `loop()`, self-rate-limited to `IMU_POLL_MS` = 40 ms, skipped while `displayOff`) is the *only* thing that touches the IMU; everything else — `updateOrientation()`, the SENSOR screen, the tilt stick, the RemoteFrame, the `imu` serial command — reads its cached values. This is mandatory, not tidiness: `BoschSensorClass` caches the data-ready interrupt status in a shared `_int_status` and clears the bit it just consumed, so two callers each swallow the other's samples and orientation silently stalls. `updateOrientation()` used to do its own `accelerationAvailable()`/`readAcceleration()` pair and was converted to cached reads.

**Screen frame.** All derived values work in the display's rotation-0 frame:

```
screen-UP = body +X      screen-RIGHT = body -Y      out of screen = body +Z
```

This is derived, not guessed: `az ≈ +1 g` lying flat (measured) fixes +Z as out-of-screen given that an accelerometer reads +1 g on the axis pointing *away* from earth; the documented `ax > 0 → portrait normal` then fixes +X as screen-up; and right-handedness of the BMI270 forces screen-right = −Y (since `(−Y) × (+X) = +Z`). Yaw follows: clockwise seen from the front is *negative* about +Z, hence `imuYawDps = -imuGz`.

**Attitude** (`imuPitchDeg` / `imuRollDeg`, `+` = nose up / right side down) is measured as rotation **away from a zero reference**, not as absolute tilt, so the operator can calibrate in whatever posture they hold the device. Each is a signed angle between the current and reference gravity directions projected onto the plane its rotation moves them in (`atan2` per plane). **The `atan2`-per-plane form is the point** — the usual `asin`-of-a-component form peaks at 90° and folds back, so "tilt further forward" would read as "tilt back" for anyone holding the device upright instead of flat. Roll is genuinely unobservable when gravity lies along the screen-vertical axis (device bolt upright): rotating about the gravity vector moves nothing an accelerometer can see, and with no magnetometer there is nothing else to ask. It reads 0 there, correctly.

Both are then rotation-adapted through **the same `applyStickRotation()` table the joystick modules use** (as tenths of a degree, so it stays in `int16`) — one copy of that table, forward-declared in the IMU section.

The zero reference is **RAM-only by design** (a hold captured days ago is not the hold you have now) and resets to "flat, screen up" — which makes the angles true attitude — on every boot.

### Calibration

**One action, every device.** `ctrlCalibrateAll()` is what the CALIBRATE row and `ctrl calibrate` both run, and it uses **independent `if`s, never `else if`**. It used to be `if (miniJoyCAvailable) {…} else { seesaw }`, so on the common seesaw + Mini JoyC pair the seesaw's centre was *never* recaptured — CALIBRATE silently did half its job on the setup most likely to need it, and a seesaw a few counts off centre is exactly what makes a robot creep instead of stop. Same trap `remoteButtonBits()` documents for merging button sources. It returns a bit-per-`CtrlStickDev` mask so callers report what actually happened. Per device: Mini JoyC → HW centre into STM32 flash; seesaw → re-arm the lazy centre capture; **Joystick v1.1 → re-arm its lazy centre capture** (it hands out raw ADC and does *not* self-centre); JoyStick2 → nothing (self-centring); IMU → `imuCalibrate()` (attitude zero + gyro bias).

**Calibration can refuse, and that is the point.** Both IMU routines return `bool` and change nothing on failure:

- `imuZeroAttitude()` rejects when `||a| − 1 g| > IMU_CAL_MAX_G_ERR`. While the device is being moved the accelerometer isn't measuring gravity alone, so capturing it would pin the zero to a direction the operator never intends to hold.
- `imuZeroGyro()` rejects when peak-to-peak on any axis exceeds `IMU_CAL_MAX_GYRO_PP`. An offset that stays put is bias; one that wanders is motion. A bias captured mid-motion is worse than none — silently wrong for the whole session, yaw never reads 0 at rest, and the STILL/MOVING indicator lies about the very thing it detects.

Gyro bias is measured **every boot, never persisted** — it drifts with temperature. A refusal at boot logs to the splash, because a yaw that reads a few deg/s at rest otherwise looks like a bug rather than a skipped calibration.

**`imuZeroExplicit` vs `imuZeroed`.** The controller screen auto-zeros on entry, and that must be keyed on `imuZeroExplicit` — the flag set *only* by a deliberate calibration (CALIBRATE, ZERO HERE, a tap on the SENSOR screen, `imu zero`). Gating on `imuZeroed` instead makes the auto-zero fire exactly once per boot, because the auto-zero sets `imuZeroed` itself: leave the screen, come back holding the device differently, and you get back the standing deflection the auto-zero exists to prevent. `imu level` / LEVEL REF clears the pin and hands the auto-zero back. The entry path deliberately does **not** re-bias the gyro — that blocks ~250 ms on a screen change.

### Controller: IMU as a tilt stick + frame axes

- **`ctrlImuStick`** (NVS `ctrlImu`, "TILT STK" row, `ctrl tilt on|off`) — **default ON.** The IMU joins the stick roster as `CTRL_DEV_IMU` (label `TILT`, `CTRL_DEV_COUNT` is now **4**), so it shows up as another disc and can drive or sit in an aux slot like any joystick. On by default because the BMI270 is soldered to the board — it is simply always one of the sticks you have. Two consequences: `connectedStickCount()` is never 0 (the controller screen is always reachable, and you can drive by tilt with nothing plugged in), and with two physical sticks the IMU becomes **aux2**, so frames go v1 → v2. That's decoder-compatible with any NessoLink ≥ 1.1.0 receiver, but a robot that already acts on aux2 will see it move with device tilt.
- **`ctrlImuFrame`** (NVS `ctrlImuTx`, "IMU TX" row, `ctrl imutx on|off`) — **default OFF**, unlike the tilt stick, because this one is *not* wire-safe: attitude rides in the frame's dedicated IMU fields, which promotes **every** frame to v3 (25 B), and any receiver built against a pre-1.2.0 `NessoFrame.h` **rejects** it (unknown version byte — it fails closed, but it does stop). Re-vendor the header on the robot first. Also ~30% more LoRa airtime on a duty-cycle-bound link.
- **`ctrlTiltRangeIdx`** (NVS `ctrlTiltR`, "TILT RNG", `ctrl tiltrange 0-3`) — 15/25/35/45° of tilt for full deflection.

**The tilt disc shows three axes, not two** (`drawTiltViz()`, used wherever `ctrlDiscDev[i] == CTRL_DEV_IMU`). A joystick disc says everything about a 2-axis device; the IMU needs more, so it adds a rolling/sliding **horizon line** (drawn from raw attitude, so it keeps moving past full stick deflection) and a **yaw needle on the rim** — straight up = not turning, swinging clockwise for positive yaw, full scale at `NESSO_IMU_YAW_FS_DPS`. The bubble still comes from the post-deadzone stick values, so what you see is what gets mixed and transmitted, exactly like the other discs. `ctrlDiscDev[3]` ({drive, aux1, aux2}, resolved in `readGamePad()` alongside the labels) is what tells the render which disc is the IMU — a label string comparison would work but breaks silently if two devices are ever named the same.

`renderController()`'s single-stick path also branches on this (`tiltOnly`): with no joystick at all the IMU drives, and without the branch it would fall through to the seesaw face-button cluster and paint four buttons for a device that isn't connected.

`readImuAxes()` returns **screen-relative** `(rx, ry)` directly and must **not** go through `applyStickRotation()` — `imuUpdateAttitude()` already rotation-adapted it, and rotating twice puts "forward" 90° off in landscape. Gesture: tilt the top **down/away** to go forward, drop the **right** edge to turn right.

**Tilt has its own deadzone floor** (`CTRL_TILT_MIN_DEADZONE` = 30, ≈1.5° at the 25° range) taken as `max()` with the shared deadzone. The shared value is sized for a stick that springs back to a hard mechanical centre; a hand has no such stop, and at the default index 16 counts is 0.8° — inside normal hand tremor. Without the floor the tilt axis never reads exactly 0, those counts survive the arcade mix, and **the robot creeps instead of stopping** (see [Guaranteeing the robot stops](#guaranteeing-the-robot-stops)).

For the same reason `initGamePad()` **auto-zeros on entry** when the tilt stick is active and no reference has been captured this session: after a reboot the reference is "level", and walking onto the controller screen holding the device at a natural 30-40° reading angle would otherwise hand the drive mix a large standing deflection and drive the robot off on entry. Mirrors the seesaw's lazy centre capture.

Note the frame carries only **two** aux slots, so with four sticks connected one is read but unused.

### FUNCTION_SENSOR — the IMU screen

Sits at enum index **2, immediately after `FUNCTION_CONTROLLER`** (everything below it shifted up by one; `FUNCTION_BATTERY` stays last so its long-press still opens device settings). It's placed there because the controller *uses* this sensor — zeroing the tilt stick and checking the axes are one screen away from driving. All screen references are symbolic, so the renumber only touched the enum itself and the `serialPrintStatus()` name table, which is indexed by `currentFunction` and must stay in enum order.

Attitude indicator (horizon line rolls with roll, slides with pitch) + the same `(forward, turn)` bubble the stick discs draw, pitch/roll/yaw-rate/|a| readout, and six single-line signed bars for the **raw sensor** X/Y/Z of accel and gyro. Raw axes are deliberately the sensor's own frame, not the screen frame: that's the ground truth a datasheet or bug report is written against, while everything above it is the derived interpretation.

Rows are 12 px and drawn **only while they fit** (`SENSOR_ROW_H`), so the 113 px landscape sprite truncates instead of scribbling over the footer; landscape also reclaims the footer strip for the sixth row (GZ, the yaw axis) and moves the status text into the dead space under the disc. A label-above-bar layout needs 18 px and overflows even the 218 px portrait sprite — that's why the rows are single-line.

**This screen is the bench check for the axis conventions.** Tip the top up → PITCH positive; drop the right edge → ROLL positive; turn clockwise as you look at the screen → YAW positive. `imu` prints the same values over serial.

Tap = zero to the current hold. KEY1 long-press opens settings (`SENSOR_SET_ITEMS` = 6: ZERO HERE / LEVEL REF / GYRO ZERO / TILT STK / IMU TX / SCR LOCK). The last three are the *same* flags the controller settings expose — one sensor, two consumers, and a second copy of the state would be a bug waiting to happen. This panel dispatches by **index** (unlike the controller's, which dispatches on a row action), so the rows are named in `enum SensorSetRow` and the label table, the value table and the activate switch all use those names: the row set is fixed, which makes index dispatch fine, but a bare `5` in one of the three places and not the others is how a tap silently actions the wrong setting. `sensorSettingsHeaderActive` restores the battery/WiFi header when the panel closes, exactly like `ctrlSettingsHeaderActive`.

### Battery percentage

Use `voltageToPercent()` (piecewise linear LiPo curve, 3.0V→0% / 4.2V→100%). Do **not** use `battery.getChargeLevel()` — tested at 3.89V it returned 98% while voltage-based correctly gave ~63%. The library value is kept only as a debug label on the battery screen.

### ESP32-C6 I2C constraint (RFID2)

ESP32-C6 has only one HP I2C controller (`Wire`). The LP I2C SDA is hardware-locked to GPIO 6, unusable for GPIO 5. RFID2 operations briefly switch `Wire` to GPIO 5/4 via `rfid2WireGrove()` / `rfid2WireRestore()`, which call `Wire.end()` + `Wire.begin()`. `Wire.end()` is mandatory before `Wire.begin()` with different pins — omitting it leaves the bus silently stuck on the old pins.

**`GROVE_POWER_EN` is an I2C *expander* pin, and the expander is on the MAIN bus.** `digitalWrite(GROVE_POWER_EN, …)` is a register write sent over the global `Wire` (see `expander.cpp`; it calls `Wire.begin(SDA, SCL)` exactly once, then writes to whatever pins `Wire` currently holds). So **`Wire` must be back on the main bus before any GROVE power write** — do it while re-pinned to the HAT or Grove bus and the write goes nowhere, silently, and the rail never changes state. This bit `serialHandleI2c()`, which turned GROVE power on straight after the HAT-bus scan: the rail stayed off and the Grove scan reported `(none)` for a Unit Joystick v1.1 that was plugged in, detected at boot and driving the controller screen. The bug hid for as long as it did because the scan *does* work whenever something else already holds the rail on — the controller screen holds it for the whole session. Every other `digitalWrite(GROVE_POWER_EN, …)` site is preceded by a `*WireRestore()`; keep it that way.

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

#define MINIJOYC_ADDR   0x54   // Mini JoyC HAT, HAT bus G6/G7
#define JOYSTICK2_ADDR  0x63   // Unit JoyStick2, Grove bus G5/G4 (+ GROVE_POWER_EN)
#define JOYSTICK1_ADDR  0x52   // Unit Joystick v1.1, Grove bus G5/G4 (+ GROVE_POWER_EN)
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

### Unit JoyStick2 (M5Stack, STM32G030, I2C 0x63)

A **third** joystick input. It's a Grove I2C unit, so it shares the **Grove bus (SDA=GPIO5, SCL=GPIO4)** and `GROVE_POWER_EN` (5V) with the RFID2 unit — different addresses (0x63 vs 0x28), but physically only one device fits the Grove port, so in practice JoyStick2 / RFID2 / IR / RF433 are mutually exclusive there. It reuses `rfid2WireGrove()`/`rfid2WireRestore()` (aliased `joystick2WireGrove()`/`joystick2WireRestore()`) to re-pin `Wire` to GPIO5/4 around each access. Probed at boot (`joystick2Available`) inside the same GROVE-power window as the RFID2 probe. GROVE power is held **HIGH for the whole controller session** (`initGamePad()` powers it; the leave-controller block drops it unless IR/RF433 hold it).

**Cold-boot settle gotcha (this is what made it "not appear"):** the STM32G030 needs **~300 ms after power-on** before it ACKs on I2C — far longer than the RFID2's ~100 ms. A single probe ~150 ms in NAKs and the unit gets marked absent *for the whole session* (boot-only probe). So **both** probe sites retry: the boot probe loops up to 6× (`delay(60)` between tries, ~360 ms total), and `initGamePad()` waits 120 ms + retries 6× after powering GROVE (GROVE power is *cold* on every controller-screen entry). Keep these retry loops — without them detection is flaky. The `i2c` serial command (scans all three buses) is the diagnostic for this; its GROVE scan waits 400 ms for the same reason.

Register map (M5Unit-JoyStick2 library):
- **Reg 0x50** → offset/centered **12-bit axes**, `int16` LE X (0x50/0x51) then Y (0x52/0x53), ~±2048, auto-centered by the unit's stored calibration (**no software zeroing**). Read in one 4-byte transaction by `joystick2ReadAxes()`.
- **Reg 0x20** → button, 1 byte (0=pressed). Read fail-safe to "released" on NAK (0xFF).
- **Reg 0x30** → RGB LED, uint32 LE (cosmetic idle/pressed indicator; channel order not verified).

`readJoystick2Axes()` normalises the ±2048 reading to the ±512 scale the **shared deadzone** (`CTRL_DEADZONE_VALS`) expects, then to ±515 like the other sticks, in the **same (px=horizontal-right-positive, py=vertical-up-negative) convention as the Mini JoyC** so it feeds the shared `applyStickRotation()` table identically (always rotation-adapts, like the front-mounted Mini JoyC). **Axis mapping (verified on hardware):** the unit's **Y register drives screen-horizontal and its X register screen-vertical (interchanged), and both are inverted** — i.e. `horiz = jy/4`, `vert = -jx/4`. If a future unit reads differently, that one function is where to adjust.

### Unit Joystick v1.1 (M5Stack, MEGA8A, I2C 0x52)

The **original** M5 joystick unit — not JoyStick2. A Grove I2C unit like JoyStick2 and RFID2,
so it shares the Grove bus (SDA=GPIO5, SCL=GPIO4) and `GROVE_POWER_EN` (5V) and reuses
`rfid2WireGrove()`/`rfid2WireRestore()` (aliased `joystick1WireGrove()`/`joystick1WireRestore()`).
Different address from JoyStick2, so the two *can* coexist behind an I2C hub; one Grove port
means one unit in practice. Probed at boot inside the same GROVE-power window as the others.

**There is no register file.** A 3-byte read of the device address is the whole protocol:
`[0] X 0..255`, `[1] Y 0..255`, `[2] button 0/1`. Two things follow, and both differ from
JoyStick2:

- **The axes are raw 8-bit ADC — the unit does not centre them** (JoyStick2's `0x50` does). So
  `readJoystick1Axes()` **lazy-captures the resting centre**, exactly like the seesaw and for
  exactly the same reason: without it a released stick never mixes to a clean zero and the
  robot creeps instead of stopping (see [Guaranteeing the robot stops](#guaranteeing-the-robot-stops)).
  Re-armed on every controller-screen entry and by CALIBRATE. Only a reading inside
  `JOYSTICK1_CENTRE_MIN..MAX` (80..176) is accepted as a centre — a capture taken while the
  stick is held would bake that deflection in and make the robot creep the *other* way.
- **No LED and no calibration register**, so nothing is ever written. Axes and button arrive in
  one transaction — one bus window per cycle, like the other units.

**Axis mapping and button polarity, both measured on hardware 2026-09-02:**

- *Axes.* The **Y byte rises as the stick is pushed away** (forward) and the **X byte rises to
  the right** (rotate CW). The two lines at the end of `readJoystick1Axes()` are the only place
  this lives — `px` is FORWARD and `py` is TURN going into `applyStickRotation()`, see the
  `*DisplayX/Y` note under
  [Remote control transport layer](#remote-control-transport-layer). The first guess had
  forward inverted; both axes reach the full 0..255 span, so a stick that reads dead on one
  axis is a wiring or unit fault, not a mapping one.
- *Button.* `JOYSTICK1_BTN_PRESSED` is **1** — this unit is **not** active-low like the Mini
  JoyC and JoyStick2. The byte never left 0 across 200+ samples at rest and a full two-axis
  sweep, so 0 is the released level, and a press was then confirmed on hardware.

  The read also **arms itself**: `joystick1BtnArmed` is set only once the unit has been seen
  reporting the released level, and until then the button never registers. That is not
  belt-and-braces — it is what turned the original *wrong* polarity guess into a no-op instead
  of a permanently "pressed" button latched into every frame the robot receives. Keep it: it
  makes this constant safe to flip from the bench.

`ctrl` prints `joy1 raw x/y/btn` and the captured centre while the controller screen is live —
that line is the bench check for both. Note the raw values only refresh while `readGamePad()`
is running, so read them **on the controller screen**; elsewhere they are the boot defaults.

**Both Grove sticks are probed in ONE interleaved retry loop at boot** (~600 ms budget, 12 ×
50 ms, exits as soon as both answer), not two loops in series. They share a bus and a power
window, so the pair costs one settle rather than two — but the real reason is an ordering trap:
probed second in series, Joystick v1.1 was only ever detected because an *absent* JoyStick2
burned 360 ms of its own retries first. Plug a JoyStick2 in and that accidental padding
disappears. The probe is boot-only, so giving up early marks the unit absent for the whole
session.

### Remote control transport layer

All control commands flow through `transmitRemoteCommand(L, R)` → a transport dispatcher selected by `remoteTransportIdx` (`enum RemoteTransport`, persisted to NVS key `txLink`). The payload is the **versioned RemoteFrame**, encoded by the **NessoLink library** (`#include <NessoFrame.h>` → `NessoFrame` + `nessoEncode()`) — the same codec the robot receiver uses (repo: `github.com/ugursayar/NessoLink`, public and in the Arduino Library Registry since 1.1.0; local dev clone at `D:\packages\arduino\user\libraries\NessoLink`). The firmware only fills a `NessoFrame` (motors from `transmitCmd`, `remoteAux*`/`remoteAux2*`, `remoteButtonBits()`, `++remoteSeq`); the byte layout, CRC-8, and `NESSO_*` constants live in the library. `nessoEncode()` emits the **minimal** version that fits the data: **v1 (15 bytes)** for drive + ≤1 aux stick, **v2 (19 bytes)** with a second aux stick, **v3 (25 bytes)** with IMU attitude, **v4 (29 bytes)** with a third aux stick. `nessoDecode()` accepts all four, so a setup that doesn't use the newer fields stays byte-compatible with existing receivers. Each newer version is a pure append — v3 keeps v2's aux2 slots and v4 keeps both, zero-filled when unused. Wire layout (little-endian, CRC-8 checked):

| off | field | type | notes |
|----|----|----|----|
| 0 | magic | u8 | `0xA5` |
| 1 | version | u8 | `1` (v1) or `2` (v2) |
| 2 | seq | u8 | rolling, for dedup/loss detection |
| 3–4 | leftMotor | i16LE | -255..255 |
| 5–6 | rightMotor | i16LE | -255..255 |
| 7–8 | auxX | i16LE | aux stick 1 X, **-255..255, + = RIGHT** (0 unless `hasAux`) |
| 9–10 | auxY | i16LE | aux stick 1 Y, **-255..255, + = UP** (0 unless `hasAux`) |
| 11–12 | buttons | u16LE | bitfield, 1=pressed: A=0,B=1,X=2,Y=3,SEL=4,START=5,STICK=6,STICK2=7 |
| 13 | flags | u8 | bit0=aux1, bit1=aux2, bit2=imu present; rest reserved |
| 14 | crc8 (v1) | u8 | poly 0x07, init 0x00, over bytes 0..13 — **v1 ends here** |
| 14–15 | aux2X (v2+) | i16LE | aux stick 2 X, -255..255, + = RIGHT (0 unless `hasAux2`) |
| 16–17 | aux2Y (v2+) | i16LE | aux stick 2 Y, -255..255, + = UP |
| 18 | crc8 (v2) | u8 | poly 0x07, init 0x00, over bytes 0..17 — **v2 ends here** |
| 18–19 | imuPitch (v3) | i16LE | -255..255, + = nose UP (0 unless `hasImu`) |
| 20–21 | imuRoll (v3) | i16LE | -255..255, + = RIGHT side DOWN |
| 22–23 | imuYaw (v3) | i16LE | -255..255, **rate** not angle, + = CW seen from the front |
| 24 | crc8 (v3) | u8 | poly 0x07, init 0x00, over bytes 0..23 — **v3 ends here** |
| 24–25 | aux3X (v4) | i16LE | aux stick 3 X, -255..255, + = RIGHT (0 unless `hasAux3`) |
| 26–27 | aux3Y (v4) | i16LE | aux stick 3 Y, -255..255, + = UP |
| 28 | crc8 (v4) | u8 | poly 0x07, init 0x00, over bytes 0..27 |

**The transmitter never promotes the wire version implicitly.** v3 needs `ctrlImuFrame` ("IMU TX"), v4 needs `ctrlAux3Frame` ("AUX3 TX"), both **default OFF**. This is not caution for its own sake: `nessoDecode()` rejects an unknown version byte, and `transmitRemoteStop()` deliberately does *not* clear `remoteHasAux*`/`remoteHasImu` — so a version the receiver rejects makes the **all-stop frame unreceivable**. Auto-promoting on hotplug would mean plugging in a joystick silently disabled the robot's stop. A 4th device with AUX3 TX off is read, drawn, and labelled **SPARE**.

Aux slots are **role-typed, not device-typed** — which device sits in a slot is now user configuration. Receivers must be written against "aux slot 2", never "the Mini JoyC". Button bits *are* device-bound and are **not** renumbered by reordering (`NESSO_BTN_STICK` is always the Mini JoyC click, `NESSO_BTN_STICK2` the JoyStick2 click, `NESSO_BTN_STICK3` — bit 8, added in NessoLink after 1.3.0 — the Joystick v1.1 click). STICK3 names a bit that was already on the wire as a reserved zero inside the existing 16-bit field: no frame version, length or CRC coverage changes, and an older receiver simply never sees it set. A tilt/IMU input in an aux slot rides as post-deadzone **deflection**, never as attitude counts — attitude only ever appears in the `imu*` fields, and both can be present in one frame from the same sensor.

**Every axis in the frame is -255..255** — motors, aux and IMU alike — so a receiver needs exactly one dead-zone constant, and no sensor's raw span ever reaches the wire (`auxAxisToWire()` / `imuAngleToWire()` / `imuYawToWire()` rescale; swapping in a unit with a different range must not silently change what the protocol means). IMU counts convert back with a full scale **baked into the protocol**, not a local constant: `NESSO_IMU_ANGLE_FS_DEG` (90°) and `NESSO_IMU_YAW_FS_DPS` (250 °/s), via `nessoImuDeg()` / `nessoImuYawDps()`. Specified in `NessoFrame.h` since NessoLink **1.1.2** (IMU since **1.2.0**) — the header is vendored verbatim into receiver sketches, so re-vendor both ends together or they silently disagree.

Frame pitch/roll are screen-relative angles measured against the transmitter's **calibrated zero attitude** (the operator's neutral hold), not geodetic level; an uncalibrated transmitter zeroes to "flat, screen up", which makes them true attitude. Stream transports get a version's length from `nessoFrameLen(ver)` — the TCP receiver examples used an inline version→length table that would have silently discarded every v3 frame.

**The `*DisplayX/Y` pairs are NOT screen X/Y — they are `(forward, turn)`.** Everything downstream of `applyStickRotation()` carries the vertical axis in the X field (up positive) and the horizontal axis in the Y field (right positive): that is what the arcade mixer (`thr = rx`, `turn = ry`), `drawStickViz()` and the FORWARD/ROTATE label all consume. Anything leaving the device in a standard X/Y convention must **swap them** — `hidSendGamepad()` does, and so does the aux → frame mapping in `readGamePad()` (`auxX = auxAxisToWire(aux2DisplayY)`, `auxY = auxAxisToWire(aux2DisplayX)`). Copying a `*Display*` pair straight into a frame's X/Y fields draws a **correct disc on screen while transmitting the axes transposed** — a shipped bug (fixed 2026-08-06), and the failure mode is deceptive precisely because the screen looks right. `ctrl` with no argument prints a live `axes` line (drive as fwd/turn, aux as it goes on the wire) to check this without a receiver.

The `ctrlInvertX/InvertY/SwapXY` flags are deliberately **not** applied to aux sticks. Each device already normalises into the shared screen-relative frame at read time (`applySeesawOrient` + `applyStickRotation`), so aux tracks screen orientation on its own; the `ctrl*` flags sit above that as a drive-mix escape hatch bound to whichever stick is *primary*. Propagating them would re-transpose the aux axes the moment someone toggled SWAP XY to tune drive feel. (Making them genuinely per-device — 3× NVS keys, 3× settings rows — is a separate change; the role-scoping is a known wart, not the cause of the transposition.)

Size send/decode buffers with `NESSO_FRAME_MAX_LEN` (now **29**, was 25) and use the length `nessoEncode()` returns. Sent at the readGamePad rate (~10 Hz) including when centered, so the receiver can implement a failsafe timeout. **The receiver firmware must parse this frame** — use the same NessoLink library (`nessoDecode()`). Backends:
- **`TX_WIFI_UDP`** (default, fully working) — `udp` to `targetIpAddress:udpPort`. Unchanged from the original single-path implementation.
- **`TX_BLE`** (working) — routes through the library's `NessoBleLink` (`#include <NessoLinkBLE.h>`, global `bleLink`). `txBle()` lazily injects two sinks into `bleLink.begin()`: a notify sink (writes/notifies the existing NESSO BLE UART TX characteristic `pBLETxChar`) and a ready predicate (`bleUartReady && pBLETxChar && btConnected`). The link never owns the BLE stack.
- **`TX_WIFI_TCP`** (minimal) — lazy `NetworkClient` connect to `targetIpAddress:tcpPort` (default 8890, in config.json `tcp_port`), `setNoDelay(true)`.
- **`TX_LORA`** (live) — routes through the library's `NessoLoRaLink` (`#include <NessoLinkLoRa.h>`, global `loraLink`), which does the async `startTransmit()` + duty-cycle rate gating (non-blocking; drops frames rather than stalling). `txLora()` lazily calls `loraTxArm()` the first time it is needed: it powers the SX1262, `lora.begin()`s it (unless the scanner already did), points the antenna switch at TX, and hands the radio to `loraLink.begin(&lora, LORA_TX_MIN_INTERVAL_MS)` — which installs the **TxDone** DIO1 action. The scanner and the controller are **mutually-exclusive screens sharing one radio and one DIO1 line**, so the radio-lifecycle handoff is managed on entry: `initLora()` clears `loraTxArmed`, re-points the antenna at RX, and re-installs the scanner's **RxDone** `loraISR`; the next `txLora()` re-arms. Every `txLora()` re-runs `SPI.begin(…, LORA_CS)` because the display reconfigures the shared bus. At SF11/BW250 a frame is ~150–200 ms airtime and EU868 ~1% duty cycle caps it at a few Hz — a low-rate command/telemetry link, not a live joystick path. Matching receiver: the NessoLink `CardputerAdvLoRaReceiver` example (M5 Cardputer ADV + Cap LoRa 1262).

Selectable from Controller settings ("TX LINK" row, item 6) or serial `ctrl link udp|ble|tcp|lora`. ESP32-C6 has one radio — BLE and WiFi coexist but contend, so BLE mode generally wants WiFi off.

### Guaranteeing the robot stops

Two independent mechanisms, because a released stick that doesn't stop the robot is the worst failure this firmware has:

**1. A released stick must mix to exactly zero.** Every stick applies the shared deadzone (`CTRL_DEADZONE_VALS[ctrlDeadzoneIdx]`) at read time — including the **seesaw** and the **Unit Joystick v1.1**, whose centres are only *lazy-captured* on the first read, and the **IMU tilt stick**, which additionally takes a `max()` with `CTRL_TILT_MIN_DEADZONE` because a hand has no mechanical centre to spring back to (see [Controller: IMU as a tilt stick](#controller-imu-as-a-tilt-stick--frame-axes)). Without it a released seesaw sits a few ADC counts off centre, those counts survive the arcade mix, and the frame carries L/R of ±1..3 forever (a slow creep, not a stop). Because a centred stick mixes to 0 and `readGamePad()` transmits every ~100 ms **even when centred**, the release itself needs nothing else — the next frame is already a stop. `ctrlHasTunableStick()` therefore returns true for any connected stick (it gates the DEADZONE/CALIBRATE rows).

**2. An explicit all-stop when the drive loop stops running.** `transmitRemoteStop()` (motors 0, aux centred, buttons cleared) fires wherever `readGamePad()` stops being called with a non-zero command as the robot's last instruction: **navigating away from the controller screen**, **opening its settings panel** (the panel doesn't poll the stick), and **losing the drive stick mid-session**. It's guarded by `remoteTxActive` — set by `transmitRemoteCommand()`, cleared here — so it's a no-op unless something was actually driving, and fires exactly once.

The whole leave-the-controller-screen block is keyed on **`if (currentFunction != FUNCTION_CONTROLLER)`, deliberately NOT `lastFunction == FUNCTION_CONTROLLER`** like the other screens' cleanup blocks beside it. Three paths set `lastFunction = -1` *before* `renderFunction()` compares it, and each would silently swallow the work: `serialGoto()` (sets it alongside `currentFunction`, so `goto <screen>` would **always** miss), `resetActivity()` on wake (a single swipe that both wakes a dimmed screen and navigates away — `checkTouch()` runs before `renderFunction()`), and `updateOrientation()` on rotation. Same trap as `g_spriteY` above: **never gate exit work on `lastFunction`.**

Since the block then runs on every frame of every other screen, each piece is edge-triggered by its own "I own this" flag instead: `remoteTxActive` (set by `readGamePad()`) for the all-stop, `ctrlSessionActive` (set by `initGamePad()`) for the Mini JoyC / JoyStick2 LED-off and the `GROVE_POWER_EN` drop. Both clear on the way out, so each runs exactly once however the screen was left.

`remoteTxActive` is set by **`readGamePad()`, not `transmitRemoteCommand()`** — it means "the drive loop owns the link". The serial `send L R` test command also routes through `transmitRemoteCommand()`, so setting it there would make the next `renderFunction()` cancel a manual `send` with an immediate all-stop.

Delivery is verified, not fire-and-forget: every `tx*()` backend and `transmitRemoteCommand()` now **return `bool`** (transport accepted the frame), and the stop retries until 2 frames are accepted. A dropped stop is a running robot — UDP loses packets, BLE may be disconnected, LoRa's rate gate refuses frames. LoRa is special-cased to **one frame plus a `LORA_TX_DRAIN_MS` wait**: retrying inside its TX interval only burns attempts, and the caller may repoint the antenna switch (controller → LoRa scanner) while the packet is still airborne. The whole thing blocks the main loop, so it is time-bounded to stay invisible on a screen change.

A receiver failsafe timeout is still the last line of defence — this makes the normal paths not *need* it.

### Device order (`ctrlDevOrder`) — the role model

`uint8_t ctrlDevOrder[CTRL_DEV_COUNT]` is a **permutation** of `CtrlStickDev`, most-important first, persisted as a `CTRL_DEV_COUNT`-byte NVS blob (`ctrlOrd`). Every `readGamePad()` cycle `ctrlOrderResolveRoles()` compacts it over the *connected* devices to produce roles **DRIVE, AUX 1, AUX 2, AUX 3** (`ctrlRoleDev[]` / `ctrlRoleCount`).

**`CTRL_ROLE_COUNT` (4) is not `CTRL_DEV_COUNT` (5).** The roster is allowed to grow past the number of slots the frame has, so resolution stops at four roles and a connected device that compacts past AUX 3 holds **none** — still read (the read loop runs before roles are assigned), but not drawn and not transmitted. `ctrl order` says so out loud, because a stick that is plugged in, powered and simply ignored looks exactly like a dead unit. Everything role-shaped (`ctrlRoleDev[]`, `ctrlDiscDev[]`, `CTRL_ROLE_LABELS[]`, the disc arrays in `renderControllerMulti()`, `CTRL_SET_MAX_ROWS`) is sized by `CTRL_ROLE_COUNT`; only the order blob and the device tables are sized by `CTRL_DEV_COUNT`.

**Enum order ≠ default priority, and that is the point.** `CtrlStickDev` values are *persisted* — they are the NVS blob's bytes and the integers in a profile's `order` array — so a new device is **appended** to the enum or every stored order silently remaps to different hardware. The compiled default ranking lives separately in `CTRL_DEV_DEFAULT_ORDER` (`JOYC, PAD, JOY2, JOY1, TILT`), which is what `ctrlOrderDefault()` walks. It puts every physical stick **ahead of the IMU** deliberately: the IMU is soldered on and therefore always "connected", so a device ranked below it would take the last role or none the moment it was plugged in — plug in a joystick, find the tilt stick still driving.

**A stored order shorter than the roster is extended, not discarded.** `ctrlOrderExtend()` keeps the stored devices where they are and puts the missing ones at the end in default-priority order — **except that a trailing IMU stays last**. That exception is the whole reason it is not a plain append: the IMU is always connected, so a new stick appended behind it comes last, and someone whose only stick is that new unit would plug it in and find the tilt stick driving. It is *only* the trailing case, because a user who moved the IMU up the list has expressed a preference about who drives, and a device they have never seen must not displace it. Both the NVS blob and a profile's `order` array go through it, so a firmware update never silently resets the mapping.

This replaced the scalar `ctrlPrimaryDev`, which could only express "who drives" **and was rewritten by four separate snap-to-connected sites**. That silently destroyed user intent: unplug the stick you had chosen as primary and the next `saveSettings()` persisted the substitute. A stored permutation needs no snapping — compaction happens at read time and is never written back, so an unplugged device keeps its position and returns to it when re-plugged. All four snap sites are gone; `ctrlPrimaryDev()` is now a *function* returning `ctrlRoleDev[0]`, deliberately so the compiler catches any reintroduced assignment.

`ctrlOrderSetRole()` **rotate-inserts** rather than swapping — a swap would fling the displaced device to wherever the incoming one came from, which reads as random when stepping a settings row through the device list. `ctrlOrderValid()` is load-bearing, not defensive dressing: the bytes index `devOn[]` and `CTRL_DEV_LABELS[]`, so a corrupt blob would read past the end of both.

Migration order on load: `ctrlOrd` (validated permutation) → `ctrlPrim` (old scalar) → `dualPriMJC` (older bool) → compiled default. `saveSettings()` still writes `ctrlPrim = ctrlDevOrder[0]` for one release so a firmware downgrade still drives the right stick.

**A drive-device change forces a stop.** `ctrlLastDriveDev` is compared every cycle; when it changes (reorder, unplug, hotplug) `readGamePad()` fires `transmitRemoteStop()` and returns, picking the new stick up on the next tick. Without it the robot jumps straight from the old stick's last command to the new stick's resting deflection with no zero in between.

Settings: one row per filled role (DRIVE / AUX 1 / AUX 2 / AUX 3), KEY1 cycles which **connected** device holds it. Serial: `ctrl order`, `ctrl order pad,joyc,joy2,tilt` (any subset; the only way to position a *disconnected* device), `ctrl drive|aux1|aux2|aux3 <dev>`, and `ctrl primary` kept as an alias.

### Controller profiles

Named snapshots of "which robot am I driving, and how is my rig mapped to it" — stored on LittleFS as JSON at `/ctrldb/<name>.json`, one NVS pointer key (`ctrlProf`) for the active one. Max 8, names `[A-Za-z0-9_-]` up to 12 chars, **rejected not sanitised** (silently rewriting the name makes the file the user later looks for not the one that exists).

**Profile-scoped:** device order, `ctrlSwapXY/InvertX/InvertY`, deadzone, `seesawMountIdx`, tilt range, `ctrlImuStick`, `ctrlImuFrame`, `ctrlAux3Frame`, `remoteTransportIdx`, and the endpoint (`targetIpAddress`/`udpPort`/`tcpPort`). **Global:** `ctrlScreenLock` (handheld ergonomics, not robot identity — and it governs the sensor screen too, which has nothing to do with any robot), `hidGamepadEnabled` (needs a reboot — a profile field that didn't apply immediately would break the contract), LoRa preset/frequency (owned by the scanner screen).

`ctrlProfApply()` is the **only** apply path, and its ordering matters: `transmitRemoteStop()` **first**, before `remoteTransportIdx` changes, or the robot on the old link never hears a stop while the eventual exit-stop goes out on the new one. Then the TCP client drop, then the field assignments, then `imuCalibrate()` if the tilt stick just came on, then `saveSettings()` so NVS mirrors the live state. Writes are **temp-then-rename** — `saveConfig()` writes in place and a truncated JSON still *parses*, which would silently yield a partial axis mapping on the next boot. Fields use the `doc["k"] | default` fallback, so a hand-edited or partial file loads with defaults rather than failing whole.

Boot order matters: `loadSettings()` (NVS) runs *before* `LittleFS.begin()`, so NVS necessarily wins first and the profile is re-applied on top once the FS is up. A missing profile keeps the NVS settings and clears the pointer — never boot into an unconfigured mapping.

The browser is a **sub-level inside `NAV_SETTINGS`** (`ctrlSetLevel`), not a new `navState`, so the `transmitRemoteStop()` that fires on entering settings keeps the drive loop stopped for the whole browsing session. No on-device rename or delete: this firmware has no text entry, and the only long-press available is KEY1 = APPLY — binding "destroy this profile" to the apply button is how people lose configurations. Use `ctrl prof del` or the web file manager.

### Multi-stick (seesaw + Mini JoyC + a Grove unit + the IMU tilt stick)

**Five in the roster, four roles, three connectors.** The joysticks sit on three distinct connectors (main `GPIO10/8`, HAT `GPIO6/7`, Grove `GPIO5/4`) driven by the one HP I2C peripheral, time-multiplexed by re-pinning — so three physical sticks *can* be connected at once, and the IMU makes four live devices. `CTRL_DEV_COUNT` is **5** because the Grove port has two supported units (JoyStick2 `0x63` and Joystick v1.1 `0x52`); they are different addresses, so both can be present behind an I2C hub, but one port normally means one unit. The frame carries **four** roles (`CTRL_ROLE_COUNT`), so a fifth connected device holds none — see [Device order](#device-order-ctrldevorder--the-role-model). Two conflicts to know: the **Mini JoyC HAT and Speaker Hat 2 share GPIO6/7** (only one fits the slot, but nothing in software stops both being enabled), and **a Grove stick excludes RFID2 / IR / RF433** (one Grove port). Note also that two Grove sticks on a hub cost **two** bus-switch windows per cycle, not one.

**Cycle budget.** `readSeesawAxes()` used to take 4 samples with `delay(10)` between them — ~40 ms of the 100 ms tick spent in `delay()` alone, before the two bus-switch windows. It now keeps a **4-deep rolling ring, one sample per cycle**: same smoothing, same effective time constant (the cycle *is* the sample interval), no delay. Measured cycle went from tens of ms to **~7 ms** with three devices live. `ctrlCycleMs` is reported by the `ctrl` command — it is the number that says whether the ~10 Hz cadence the receiver failsafe is sized against is still being met.


Up to **three** joysticks can be connected at once — they live on three separate I2C buses (seesaw = main GPIO10/8, Mini JoyC = HAT GPIO6/7, JoyStick2 *or* Joystick v1.1 = Grove GPIO5/4) so any combination coexists. `connectedStickCount()` (0..`CTRL_DEV_COUNT`) drives controller-screen visibility, the role logic, and the settings rows.

**Roles** (resolved every `readGamePad()` cycle): one stick **drives** (full pipeline → `leftMotor`/`rightMotor`), the rest become **aux** sticks. The `CtrlStickDev` enum (`SEESAW=0, MINIJOYC=1, JOYSTICK2=2, IMU=3`) is the **canonical order** used for aux assignment. The drive device is `ctrlPrimaryDev` (NVS `ctrlPrim`, default `MINIJOYC`; migrated from the old `dualPriMJC` bool) **if it's connected**, else the first connected device; the remaining connected devices become **aux1, aux2** in canonical order. Aux1 → `auxX`/`auxY` + `flags` bit0; aux2 → `aux2X`/`aux2Y` + `flags` bit1 (which promotes the frame to v2). `readGamePad()` reads each connected stick into a per-device screen-relative `(rx,ry)` (`devRx[]`/`devRy[]`), then assigns roles. `ctrlDriveLabel`/`ctrlAux1Label`/`ctrlAux2Label` (device names) are resolved for the render. Set the primary via the "PRIMARY" settings row (cycles connected devices, shown when ≥2 sticks) or serial `ctrl primary pad|joyc|joy2|tilt`. All stick click-buttons merge into the frame (Mini JoyC → `STICK`, JoyStick2 → `STICK2`; the tilt stick has no button). With four sticks connected only three get roles — the frame has two aux slots — so the fourth is read but unused. `renderController()` branches to `renderControllerMulti(n)` showing **n stick discs** (drive + aux1 [+ aux2]) when ≥2 are connected.

**Per-stick rotation adaptation:** the Mini JoyC *always* rotation-adapts (front-mounted); the IMU tilt stick is rotation-adapted upstream in `imuUpdateAttitude()` and must therefore **skip** `applyStickRotation()` in `readGamePad()`. The seesaw uses a **stick-mount** model (`seesawMountIdx`, NVS `ssMount`): a base transform (`applySeesawOrient()`) → then, **only for ATTACHED mounts**, the screen-rotation table (`applyStickRotation()`). The attached/detached distinction is the key insight — an attached stick rotates *with* the device so it must reflect screen orientation; a detached stick is held independently so it must **not** follow device rotation. Note that `currentRotation` is frozen on this screen by default — see [Controller screen lock](#controller-screen-lock-ctrlscreenlock) — so rotation adaptation is pinned to the orientation the screen was entered in.

| `seesawMountIdx` | label | base transform | screen rotation |
|----|----|----|----|
| 0 `SS_STICK_SIDE` (default) | SIDE | bx=−px, by=py | **ON** (attached) |
| 1 `SS_STICK_BACK` | BACK | bx=px, by=py | **ON** (attached) |
| 2 `SS_DETACHED_PORTRAIT` | DET-PORT | bx=−px, by=py | OFF (detached) |
| 3 `SS_DETACHED_LANDSCAPE` | DET-LAND | bx=py, by=px | OFF (detached) |

`seesawAppliesRotation()` returns true for side/back only. SIDE/BACK are confirmed working (rotation-adapting). The DETACHED base signs are best-guesses — verify by testing. Selectable via the "MOUNT" settings row (shown only when a seesaw is present) or serial `ctrl ssmount 0-3` (side/back/detport/detland). The Mini JoyC and JoyStick2 are unaffected (both always rotation-adapt).

Bus cost: reading all connected sticks per cycle includes the seesaw's 40 ms `analogRead` averaging **plus** a `Wire.end()/begin()` switch for each of the Mini JoyC (HAT bus) and JoyStick2 (Grove bus) — fine at the ~10 Hz poll. Order: seesaw (main bus) → Mini JoyC (HAT, restore) → JoyStick2 (Grove, restore), so `Wire` is back on the main bus at cycle end.

`renderControllerMulti(n)` draws **n discs evenly across the width** (disc 1 = drive, 2 = aux1, 3 = aux2, each labelled with its device name) in both orientations: radius shrinks with stick count so three fit the 135px portrait width (and the 240px landscape width). Below them: the drive action string, L/R motor values, and a button row. **Button layout differs by orientation:** portrait draws a `SEL/STA/JC/J2` row plus the seesaw face cluster (Y/X/A/B) as a diamond below; landscape (only ~113px tall — no room for the diamond) **flattens everything into one bottom row** (`SEL STA A B X Y JC J2`) with the spacing auto-shrinking (`constrain((sw-28)/nb, 16, 26)`) to fit. Forgetting the landscape case is how face buttons "disappear" when rotating.

### Controller button reading (don't let it stick or drop)

Three traps, all fixed and all easy to reintroduce:
- **Seesaw read must be probe-guarded.** `Adafruit_seesaw::digitalReadBulk()` reads into an *uninitialised* buffer and ignores the I2C result — on a NAK it returns stack garbage, which (buttons are active-LOW) latches random "pressed" bits into `gamepadButtons` that persist on screen *and* in the TX frame (stuck buttons). `readGamePadButtons()` probes `0x50` with `Wire.endTransmission()` first and only trusts `digitalReadBulk()` when the seesaw ACKs; otherwise it keeps the last good snapshot. The Mini JoyC HAT-bus re-pinning makes these NAKs more likely.
- **Sample buttons in lockstep with the frame.** `readGamePadButtons()` runs in the same 100 ms tick as `readGamePad()` and **before** it (which builds/sends the frame). A slower separate timer left up to its period of stale "pressed" frames after release.
- **`remoteButtonBits()` must MERGE all sources, not `else if`.** The seesaw block, the Mini JoyC `STICK` bit, and the JoyStick2 `STICK2` bit are independent `if`s — with `else if` a thumbstick button would be silently dropped from the frame in multi-stick mode (the device screen still showed it because it reads `miniJoyCBtn`/`joystick2Btn` directly). Each thumbstick button is read inside its own axes read (`readMiniJoyCAxes()` / `readJoystick2Axes()`, same bus window as the axes — one re-pin per cycle) and fails safe to "released" on a bad read (`0xFF`), so neither can stick the way the seesaw can.

### BLE HID gamepad mode ("standard controller")

`hidGamepadEnabled` (NVS `hidPad`, **default OFF**) makes the device a **standard BLE HID gamepad** — `readGamePad()` calls `hidSendGamepad()` (HID report: 16 buttons + 4×16-bit axes, see `HID_GAMEPAD_REPORT_MAP`) instead of `transmitRemoteCommand()`. Toggle via serial `ctrl hid on|off`. **Entirely guarded** — when off, no HID code runs and existing behaviour is byte-identical.

Non-obvious constraints (the reasons it's gated, not free-running):
- **Bonding conflict.** HID-over-GATT requires encryption/bonding, but the BLE stack is deliberately `ESP_LE_AUTH_NO_BOND` (to avoid stale-key GATT errors on reconnect). `btInitStack()` switches to `ESP_LE_AUTH_REQ_SC_BOND` **only** when HID is on — so HID and the normal UART/scan use case can't both have their preferred security at once.
- **Created at init only.** The `BLEHIDDevice` is built inside `btInitStack()`, which runs once. Enabling HID therefore needs a **reboot with BT started** (BT-on-boot ON). The serial command saves the flag and says so.
- **No USB HID.** ESP32-C6 has only a fixed-function USB Serial/JTAG controller (no USB-OTG), so a USB gamepad is impossible on this silicon — BLE HID is the only standard-controller path. C6 is also BLE-only (no Classic BT), so consoles won't pair; works on PC (DirectInput — Steam Input maps it) / Android.
- **Untested on hardware** as written — report-map/axis-sign/bonding details may need iteration with a real host.

### Screen lock (`ctrlScreenLock`) — controller *and* sensor

**Default ON** (NVS `ctrlLock`). While `currentFunction` is **`FUNCTION_CONTROLLER` or `FUNCTION_SENSOR`**, `updateOrientation()` returns immediately, so the screen holds whatever orientation it was navigated in until you leave.

Those are precisely the two screens you **tilt the device on deliberately**, which is why they are the two that need it:

- **CONTROLLER** — driving tilts the device constantly, and a rotation mid-drive both redraws the screen and flips `applyStickRotation()` under the user's thumb for every rotation-adapting stick.
- **SENSOR** — reading pitch/roll means tilting past the landscape threshold, so the screen would rotate *during the very measurement being checked*, re-mapping the axes (`imuUpdateAttitude()` is rotation-adapted) mid-reading. This screen is the bench test the whole IMU section is written against; that test only means something in one fixed frame.

**One flag, not two.** The sensor panel's TILT STK and IMU TX rows already share their state with the controller's for the same reason — one sensor, two consumers — and a second copy of "hold the orientation" would just be another thing to keep in sync. The NVS key keeps its `ctrlLock` name so existing settings migrate untouched, and the variable keeps its `ctrl` prefix rather than churn every call site.

The guard is keyed on `currentFunction` (not `navState`), so it covers both settings panels too, and it sits **after** the `displayOff/displayDimmed` freeze so the sleep behaviour is unchanged. Turning it off resumes normal auto-rotation within one 300 ms tick. Toggled from the "SCR LOCK" row on **either** panel, or serial `ctrl lock on|off` — a lock reachable only from the controller panel is a lock you will not find when the sensor screen refuses to turn.

The Controller **settings screen** is device-aware: `controllerSettingsItemCount()` = `ctrlSetBuildRows()`, a table of `CTRL_SET_FIXED_ROWS` (**13**: PROFILE, PROFILES, CALIBRATE, DEADZONE, SWAP XY, INVERT X, INVERT Y, TX LINK, SCR LOCK, TILT STK, TILT RNG, IMU TX, AUX3 TX) **+ MOUNT** when a seesaw is present **+ one row per filled role** (DRIVE / AUX 1 / AUX 2 / AUX 3) when ≥2 devices are connected.

**Row position is data, not index arithmetic.** `ctrlSetBuildRows()` fills `ctrlSetRows[]` with `{label, value, disabled, action, role}`, and the renderer, the KEY1 action handler, the KEY2 cursor wrap, the swipe bounds and the tap hit-test all consume that one table. Before this, each of those carried its own copy of the layout — a hardcoded array size, a hand-walked "dynamic rows start at index 7" `default:` case, and a `disabled` mask written as index literals — so adding a row meant editing all of them in step, and missing one gave either a stack overflow or a tap that silently actioned the wrong setting. **Dispatch on `row.action`, never on `settingsCursor`.** The three IMU rows are always present rather than gated on `imuAvailable`: the BMI270 is soldered on, so a missing one means a failed init, and a greyed-out row says that far more usefully than a row that silently disappears. `CTRL_SET_FIXED_ROWS`, `CTRL_SET_MAX_ROWS`, `SENSOR_SET_ITEMS` and the row struct are declared near the top of the sketch because the key/tap/swipe handlers run long before the render code.

`settingsCursor` is **clamped** at the top of `renderControllerSettings()` and the tap handler. Nothing clamped it before, which was survivable only while the row count never shrank under a parked cursor — the PROFILE row and the per-role rows make it shrink (switch a profile, unplug a device), and an out-of-range cursor renders a blank, untappable panel. Title is "5-STICK/QUAD/TRIPLE/DUAL STICK SETTINGS" by count, else "MINIJOYC/JOYSTICK2/JOYSTICK/GAMEPAD/TILT SETTINGS". The single-stick render path resolves *which* solo unit from `ctrlDiscDev[0]` (the resolved drive device) rather than a chain of availability flags — only one device is connected on that path, and the flag chain had to be extended for every new module. CALIBRATE/DEADZONE are active whenever any stick is connected (`ctrlHasTunableStick()`): the deadzone is shared by all three (see [Guaranteeing the robot stops](#guaranteeing-the-robot-stops)), and CALIBRATE runs `ctrlCalibrateAll()`, which re-centres **every** connected device in one action (see [Calibration](#calibration)) — not just the first one that matches. The count is centralized in that helper — render, tap, cursor-wrap (`onKey2Short`), and swipe-bound (`onSwipe`) all call it. Serial equivalents: `ctrl order` (devices: `pad|joyc|joy2|joy1|tilt`), `ctrl drive|aux1|aux2|aux3 <dev>`, `ctrl ssmount 0-3`, `ctrl dz`, `ctrl calibrate`, `ctrl lock on|off`, `ctrl tilt on|off`, `ctrl tiltrange 0-3`, `ctrl imutx on|off`, `ctrl aux3tx on|off`, `ctrl prof ...`, plus `imu` / `imu zero` / `imu level`.
