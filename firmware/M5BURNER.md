# M5Burner entry — publish notes

Source of truth for the text and settings used on the M5Burner firmware entry.
Publishing itself happens in the M5Burner desktop app (M5Stack account login);
update this file first, then copy from here into the portal.

## Entry settings

| field | value |
|---|---|
| category / device | Nesso-N1 / Arduino Nesso N1 |
| firmware file | `firmware/Nesso_base_v1.2.0.bin` (compact merge — keeps LittleFS, resets NVS; see below) |
| flash address | `0x0` |
| baud | `921600` |
| cover image | `assets/cover.png` (regenerate with `python gen_cover.py`, 420×300) |
| version | `1.2.0` (keep in sync with `m5burner.json`) |
| NessoLink (robot side) | **`1.4.0` or newer** — see below; keep in sync with the description text |

## Rebuilding the image

The published file is a **compact merge** — bootloader + partitions + boot_app0 + app, ending
after the app (`0x1EE9D0` for v1.2.0) so flashing it at `0x0` leaves the LittleFS partition at
`0x610000` untouched. Do NOT publish arduino-cli's own `Nesso_base.ino.merged.bin`: that one is
padded to the full 16 MB flash and would erase the filesystem.

**What a flash keeps and what it wipes.** Verified on hardware, v1.2.0:

| region | offset | flashing this image |
|---|---|---|
| NVS (device settings) | `0x9000`–`0xDFFF` | **ERASED** — it falls in the `0x8C00`–`0xE000` gap between boot_app0 and the partition table, which the merge fills with `0xFF` |
| LittleFS (`/irdb`, `/rf433db`, `/rfid2db`, `/ctrldb`, `config.json`) | `0x610000`+ | kept |

So an update resets the NVS-backed settings — controller axis flags, deadzone, device order,
TX link, screen lock, BT/LoRa preferences, and the *pointer* to the active controller profile.
It does **not** lose data: the profiles themselves are files under `/ctrldb`, and the robot
endpoint lives in `config.json`, so a user re-selects their profile rather than rebuilding it.
Say this in the entry text — a silent settings reset reads as a bug.

This is a property of flashing a single image at `0x0`, not something the merge chose: NVS sits
between the parts, so any contiguous image spanning them overwrites it. Preserving it would
mean publishing the parts separately, which the M5Burner entry format does not do.

```powershell
arduino-cli compile --clean --fqbn "esp32:esp32:arduino_nesso_n1" --build-path "D:/Nesso_base/build" Nesso_base.ino
copy build\Nesso_base.ino.bootloader.bin firmware\Nesso_base_0x0.bin
copy build\Nesso_base.ino.partitions.bin firmware\Nesso_base_0x8000.bin
copy build\boot_app0.bin                 firmware\Nesso_base_0xe000.bin
copy build\Nesso_base.ino.bin            firmware\Nesso_base_0x10000.bin
& "D:\packages\arduino\data\packages\esp32\tools\esptool_py\5.3.1\esptool.exe" --chip esp32c6 merge-bin `
    -o firmware\Nesso_base_v1.2.0.bin `
    0x0 firmware\Nesso_base_0x0.bin 0x8000 firmware\Nesso_base_0x8000.bin `
    0xe000 firmware\Nesso_base_0xe000.bin 0x10000 firmware\Nesso_base_0x10000.bin
```

(`merge_bin` with an underscore still works but is deprecated in esptool 5.x.)

## Publishing checklist

1. Rebuild the image (above) and commit the refreshed `firmware/*.bin`.
2. Bump `version` in **both** `m5burner.json` and the table above.
3. Regenerate the cover (`python gen_cover.py`) after bumping `VERSION`/`TAGLINE` in that
   script — it renders the version onto the card, so a stale cover advertises the old build.
   It is a **separate upload** in the portal from the firmware file.
4. **Copy the description and "What's new" text below into the M5Burner portal** — this file
   is only the source of truth; editing it changes nothing users see until it is pasted into
   the desktop app.
5. Flash the merged image at `0x0` on a real device and confirm it boots — the part offsets
   can be verified statically, but only a flash proves the packaging.
6. Re-check the NessoLink floor: it tracks what the firmware *can emit*, not what it emits by
   default. IMU TX promotes frames to v3 and AUX3 TX to v4, so a build that merely exposes
   those rows raises the floor even though both default OFF.

## Description (v1.2.0)

Arduino firmware for the **Arduino Nesso N1** handheld controller — a
WiFi/BLE/LoRa-enabled base station that controls a remote robot platform over a
selectable wireless link: WiFi-UDP, TCP, BLE, or LoRa (CRC-checked NessoLink
RemoteFrame protocol with receiver failsafe). Features a 240×135px TFT display
with a multi-mode menu, battery monitoring, and up to **four simultaneous
control inputs** — Adafruit seesaw gamepad, M5Stack Mini JoyC HAT, M5Stack Unit
JoyStick2, M5Stack Unit Joystick v1.1, and the board's own BMI270 IMU as a tilt
stick — with one drive stick plus up to three aux sticks, a configurable device
order, per-stick orientation adaptation, calibration and deadzone settings, and
named controller profiles. Also: a BMI270 sensor screen (attitude, gyro rates,
raw axes), IR and 433 MHz RF remote control, RFID card scanning, Speaker Hat 2
(MAX98357A) audio, an experimental BLE HID gamepad mode, and a web-based file
manager.

Robot side: control frames are encoded by the **NessoLink** Arduino library —
install it from the IDE Library Manager (or `arduino-cli lib install NessoLink`,
source: github.com/ugursayar/NessoLink) and decode with `nessoDecode()`. The
library ships ready-made receiver examples for ESP32 (`RobotReceiverUDP/TCP/BLE`),
Arduino Uno R4 WiFi (`UnoR4ReceiverUDP/TCP/BLE`), and M5 Cardputer ADV + Cap
LoRa 1262 (`CardputerAdvLoRaReceiver`), so a robot can be up and driving with a
few lines around `applyMotors()`.

**Requires NessoLink 1.4.0 or newer on the robot.** A floor rather than an exact
pin — newer releases decode older frames unchanged, so installing the current
Library Manager version is always right. The floor is 1.4.0 because it tracks
what this build *can* emit, not what it emits by default: frames are v1/v2 out
of the box, but turning on **IMU TX** promotes every frame to v3 and **AUX3 TX**
to v4, and `nessoDecode()` rejects a version byte it does not know — it fails
closed, but it does stop, and the all-stop frame travels the same path. Both
rows default OFF precisely so this cannot happen by accident, and the on-device
toggles say which NessoLink version they need.

## What's new in v1.2.0

- **M5Stack Unit Joystick v1.1** (I2C `0x52`) joins the roster, alongside JoyStick2
  on the same Grove port
- **The built-in BMI270 IMU is a control input** — tilt the device to drive, with a
  selectable tilt range; on by default since the sensor is soldered to the board
- **New SENSOR screen**: attitude indicator, pitch / roll / yaw-rate, and raw
  accelerometer + gyroscope axes; tap to zero to your current hold
- **Configurable device order** — DRIVE / AUX 1 / AUX 2 / AUX 3 are handed out over
  whatever is connected, and unplugging a device no longer overwrites your choice
- **Controller profiles**: named snapshots of the whole mapping plus the robot's
  IP/ports, so switching rigs or robots is one tap
- **Screen lock** (on by default) — the Controller and Sensor screens hold the
  orientation you entered them in, so tilting never rotates the display or remaps
  the axes mid-drive or mid-reading
- RemoteFrame **v3** (transmitter IMU attitude) and **v4** (a third aux stick),
  both opt-in and both requiring a matching NessoLink on the robot
- **Guaranteed stop**: an explicit all-stop whenever the drive loop stops running —
  leaving the screen, opening settings, or losing the drive stick — retried until
  the transport accepts it
- Fixes: aux stick axes were transmitted transposed and at the wrong scale; the
  screen could come back empty after a long sleep (sprites moved to static buffers);
  the status header did not return when the controller settings panel closed
