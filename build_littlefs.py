"""
Build a LittleFS image from a hardcoded list of files.

The full IR library lives in data/irdb/ (2000+ files, too large for the
9.87 MB LittleFS partition).  This script copies only the files listed in
INCLUDE_FILES to a temporary staging directory, builds littlefs.bin, then
cleans up.

Edit INCLUDE_FILES below to add or remove files.  Paths are relative to
data/ (the LittleFS root), so a line like:
    "irdb/TV/Samsung/Samsung_BN59-01315B.ir"
ends up at /irdb/TV/Samsung/Samsung_BN59-01315B.ir on the device.

Usage:
    python build_littlefs.py          # build only
    python build_littlefs.py --flash  # build + flash to COM3
"""

import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

# ── Tool paths (adjust if different on your machine) ──────────────────────────
MKLITTLEFS = (
    r"D:\packages\arduino\data\packages\esp32\tools\mklittlefs"
    r"\4.0.2-db0513a\mklittlefs.exe"
)
ESPTOOL = (
    r"D:\packages\arduino\data\packages\esp32\tools\esptool_py"
    r"\5.2.0\esptool.exe"
)
FLASH_PORT   = "COM3"
FLASH_OFFSET = "0x610000"
PART_SIZE    = "0x9E0000"   # spiffs partition size

# ── Files to include in the LittleFS image ────────────────────────────────────
# Paths are relative to data/ (= LittleFS root).
# Add as many as you like — the script will warn if the image gets too large.

INCLUDE_FILES = [
    # ── Sony soundbars ────────────────────────────────────────────────────────
    "irdb/Soundbars/Sony/Sony_HW-T400.ir",
    "irdb/Soundbars/Sony/Sony_MHC-GS300AV.ir",
    "irdb/Soundbars/Sony/Sony_Old_XBR.ir",
    "irdb/Soundbars/Sony/Sony_RDH-GTK33IP.ir",
    "irdb/Soundbars/Sony/SONY_RMT-AH412U.ir",
    "irdb/Soundbars/Sony/Sony_RMT-AH509U.ir",
    "irdb/Soundbars/Sony/Sony_Soundbar_HT-XT3.ir",
    "irdb/Soundbars/Sony/Sony_Soundbar_RMT-AH411U.ir",

    # ── Telefunken TVs ────────────────────────────────────────────────────────
    "irdb/TV/Telefunken/Telefunken_D40F294R4CW.ir",
    "irdb/TV/Telefunken/Telefunken_d32f660x5cwi.ir",
    "irdb/TV/Telefunken/Telefunken_L55U405B4CW.ir",
    "irdb/TV/Telefunken/Telefunken_L65F249i3C.ir",
    "irdb/TV/Telefunken/Telefunken_TF-LED19S64T2.ir",
    "irdb/TV/Telefunken/Telefunken_TF-LED32S54T2.ir",

    # ── Add more files here ───────────────────────────────────────────────────
    # Browse data/irdb/ for available files, then paste paths in the same
    # "irdb/<Type>/<Brand>/<Model>.ir" format.
    #
    # Examples:
    # "irdb/TV/Samsung/Samsung_BN59-01315B.ir",
    # "irdb/TV/LG/LG_AKB75095307.ir",
    # "irdb/AC/Samsung/Samsung_AR09BQHQASINEE.ir",
]

# ── Build ─────────────────────────────────────────────────────────────────────

ROOT      = Path(__file__).parent
DATA_ROOT = ROOT / "data"
OUT_BIN   = ROOT / "littlefs.bin"


def main():
    flash = "--flash" in sys.argv

    # Verify all listed files exist
    missing = [f for f in INCLUDE_FILES if not (DATA_ROOT / f).exists()]
    if missing:
        print("ERROR: missing files:")
        for m in missing:
            print(f"  {m}")
        sys.exit(1)

    with tempfile.TemporaryDirectory(prefix="lfs_stage_") as stage_dir:
        stage = Path(stage_dir)

        # Copy listed files into staging tree
        for rel in INCLUDE_FILES:
            src = DATA_ROOT / rel
            dst = stage / rel
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)

        total_bytes = sum((stage / f).stat().st_size for f in INCLUDE_FILES)
        print(f"Staging {len(INCLUDE_FILES)} files  ({total_bytes/1024:.1f} KB)")

        # Build image
        cmd = [
            MKLITTLEFS,
            "-c", str(stage),
            "-b", "4096",
            "-p", "256",
            "-s", PART_SIZE,
            str(OUT_BIN),
        ]
        print(f"Building {OUT_BIN.name} …")
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print("mklittlefs failed:")
            print(result.stderr or result.stdout)
            sys.exit(1)
        print(f"  OK — {OUT_BIN.stat().st_size / 1024 / 1024:.2f} MB")

    if flash:
        cmd = [
            ESPTOOL,
            "--chip", "esp32c6",
            "--port", FLASH_PORT,
            "--baud", "460800",
            "write_flash", FLASH_OFFSET,
            str(OUT_BIN),
        ]
        print(f"Flashing to {FLASH_PORT} …")
        subprocess.run(cmd, check=True)
        print("Done.")
    else:
        print(f"Run with --flash to write to {FLASH_PORT}.")


if __name__ == "__main__":
    main()
