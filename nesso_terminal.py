"""
NESSO N1 — BLE UART terminal
Uses the PC's built-in Bluetooth adapter via the bleak library.

Install:  pip install bleak
Run:      python nesso_terminal.py
"""

import asyncio
import sys
from bleak import BleakScanner, BleakClient

NUS_SERVICE = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
NUS_RX      = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"  # write  (PC → Nesso)
NUS_TX      = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  # notify (Nesso → PC)

def on_notify(_, data: bytearray):
    sys.stdout.write(data.decode("utf-8", errors="replace"))
    sys.stdout.flush()

async def find_nesso():
    print("Scanning for NESSO...")
    devices = await BleakScanner.discover(timeout=5.0, service_uuids=[NUS_SERVICE])
    if not devices:
        # Fallback: scan all and filter by name
        all_devices = await BleakScanner.discover(timeout=5.0)
        devices = [d for d in all_devices if d.name and "NESSO" in d.name.upper()]
    if not devices:
        print("NESSO not found. Make sure Pair Mode is ON on the device.")
        return None
    dev = devices[0]
    print(f"Found: {dev.name}  {dev.address}")
    return dev.address

async def run():
    address = await find_nesso()
    if not address:
        return

    print(f"Connecting to {address}...")
    async with BleakClient(address) as client:
        print("Connected. Type commands and press Enter. Ctrl+C to quit.\n")
        await client.start_notify(NUS_TX, on_notify)

        loop = asyncio.get_event_loop()
        while True:
            try:
                line = await loop.run_in_executor(None, sys.stdin.readline)
            except (EOFError, KeyboardInterrupt):
                break
            if not line:
                break
            data = (line.rstrip("\n") + "\n").encode("utf-8")
            await client.write_gatt_char(NUS_RX, data, response=False)

    print("Disconnected.")

if __name__ == "__main__":
    asyncio.run(run())
