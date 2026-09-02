# M5Burner entry — publish notes

Source of truth for the text and settings used on the M5Burner firmware entry.
Publishing itself happens in the M5Burner desktop app (M5Stack account login);
update this file first, then copy from here into the portal.

## Entry settings

| field | value |
|---|---|
| category / device | Nesso-N1 / Arduino Nesso N1 |
| firmware file | `firmware/Nesso_base_v1.1.0.bin` (compact merge — leaves LittleFS intact) |
| flash address | `0x0` |
| baud | `921600` |
| cover image | `assets/cover.png` (regenerate with `python gen_cover.py`, 420×300) |
| version | `1.1.0` (keep in sync with `m5burner.json`) |
| NessoLink (robot side) | **`1.1.2` or newer** — see below; keep in sync with the description text |

## Description (v1.1.0)

Arduino firmware for the **Arduino Nesso N1** handheld controller — a
WiFi/BLE/LoRa-enabled base station that controls a remote robot platform over a
selectable wireless link: WiFi-UDP, TCP, BLE, or LoRa (CRC-checked NessoLink
RemoteFrame protocol with receiver failsafe). Features a 240×135px TFT display
with a multi-mode menu, battery monitoring, 1–3 joystick input (Adafruit seesaw
gamepad, M5Stack Mini JoyC HAT, and/or M5Stack Unit JoyStick2 — one drive stick
plus up to two aux sticks, with per-stick orientation adaptation, calibration,
and deadzone settings), IR and 433 MHz RF remote control, RFID card scanning,
Speaker Hat 2 (MAX98357A) audio, an experimental BLE HID gamepad mode, and a
web-based file manager.

Robot side: control frames are encoded by the **NessoLink** Arduino library —
install it from the IDE Library Manager (or `arduino-cli lib install NessoLink`,
source: github.com/ugursayar/NessoLink) and decode with `nessoDecode()`. The
library ships ready-made receiver examples for ESP32 (`RobotReceiverUDP/TCP/BLE`),
Arduino Uno R4 WiFi (`UnoR4ReceiverUDP/TCP/BLE`), and M5 Cardputer ADV + Cap
LoRa 1262 (`CardputerAdvLoRaReceiver`), so a robot can be up and driving with a
few lines around `applyMotors()`.

**Requires NessoLink 1.1.2 or newer on the robot.** This build emits RemoteFrame
**v1/v2** only, so 1.1.2 is a floor rather than an exact pin — it is the first
release whose `NessoFrame.h` states the aux stick axis contract, and without that
a receiver can disagree with the transmitter about what `auxX`/`auxY` mean while
both sides still pass CRC. Newer releases decode v1/v2 unchanged, so installing
the current Library Manager version is always right; the floor only rules out
1.1.0 and 1.1.1, never an upgrade.

## What's new in v1.1.0

- Selectable robot TX link: WiFi-UDP / TCP / BLE / LoRa (NessoLink RemoteFrame v1/v2,
  robot needs NessoLink >= 1.1.2)
- Up to three joysticks at once — seesaw gamepad, Mini JoyC HAT, Unit JoyStick2:
  one drive + two aux sticks, per-stick rotation adaptation, selectable primary
- Controller settings screen: calibration, deadzone, axis swap/invert, TX link,
  stick mount, primary stick
- Speaker Hat 2 (MAX98357A) audio support
- Experimental BLE HID gamepad mode (off by default)
- Smoother drive mixer and non-blocking TCP reconnects; button-read and boot
  robustness fixes
