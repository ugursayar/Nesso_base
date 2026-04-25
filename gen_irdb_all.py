"""
Download .ir files from Flipper-IRDB and probonopd/irdb into data/irdb/.

Flipper-IRDB files are already in firmware-compatible format — downloaded directly.
probonopd/irdb CSV entries are converted to parsed .ir format for brands/types
not covered by Flipper-IRDB.

Output layout: data/irdb/<Type>/<Brand>/<Model>.ir
Existing files are never overwritten.
"""

import re
import sys
import time
import json
import urllib.request
import urllib.error
import urllib.parse
from pathlib import Path

OUT_ROOT = Path(__file__).parent / "data" / "irdb"

# --- Flipper-IRDB top-level paths to skip ---
# _Converted_ = auto-converted probonopd CSV data; handled separately
FLIPPER_SKIP_PREFIXES = {"_Converted_", "APs", "Toys"}

# --- Flipper-IRDB category → local folder name ---
FLIPPER_CATEGORY_MAP = {
    "TVs":                        "TV",
    "SoundBars":                  "Audio",
    "Audio_and_Video_Receivers":  "Audio",
    "Speakers":                   "Audio",
    "Projectors":                 "Projector",
    "ACs":                        "AC",
    "Cameras":                    "Camera",
    "Fans":                       "Fan",
    "Lights":                     "Light",
    "LED_Lighting":               "Light",
    "DVD_Players":                "Media",
    "Blu-Ray":                    "Media",
    "VCR":                        "Media",
    "CD_Players":                 "Media",
    "Multimedia":                 "Media",
    "Streaming_Devices":          "Media",
    "Monitors":                   "Monitor",
    "Cable_Boxes":                "Media",
    "DVB-T":                      "Media",
    "Heaters":                    "Heater",
    "Humidifiers":                "Misc",
    "Air_Purifiers":              "Misc",
    "Vacuum_Cleaners":            "Misc",
    "Miscellaneous":              "Misc",
    "CCTV":                       "Misc",
    "Consoles":                   "Media",
}

# --- probonopd/irdb category → local folder name ---
PROBONO_CATEGORY_MAP = {
    "TV":           "TV",
    "DVD":          "Media",
    "Blu-ray":      "Media",
    "DVR":          "Media",
    "STB":          "Media",
    "Projector":    "Projector",
    "AC":           "AC",
}

GITHUB_API   = "https://api.github.com"
FLIPPER_REPO = "Lucaslhm/Flipper-IRDB"
FLIPPER_RAW  = "https://raw.githubusercontent.com/Lucaslhm/Flipper-IRDB/main"
PROBONO_REPO = "probonopd/irdb"
PROBONO_RAW  = "https://raw.githubusercontent.com/probonopd/irdb/master/codes"

# Supported probonopd protocols → firmware protocol names
PROBONO_PROTO_MAP = {
    "NEC":      "NEC",
    "SAMSUNG":  "SAMSUNG32",
    "SONY":     "SIRC20",
    "RC5":      "RC5",
    "RC6":      "RC6",
    "JVC":      "JVC",
    "LG":       "LG",
}

# ── Helpers ───────────────────────────────────────────────────────────────────

def _get(url, retries=3, delay=1.0):
    headers = {"User-Agent": "Nesso-irdb-downloader/1.0"}
    for attempt in range(retries):
        try:
            req = urllib.request.Request(url, headers=headers)
            with urllib.request.urlopen(req, timeout=20) as r:
                return r.read()
        except urllib.error.HTTPError as e:
            if e.code in (404, 403):
                return None
            if attempt < retries - 1:
                time.sleep(delay * (attempt + 1))
            else:
                print(f"  HTTP {e.code}: {url}", file=sys.stderr)
                return None
        except Exception as e:
            if attempt < retries - 1:
                time.sleep(delay * (attempt + 1))
            else:
                print(f"  Error: {e} — {url}", file=sys.stderr)
                return None


def _api_get(url):
    """GitHub API GET — handles 403 rate limit with wait."""
    data = _get(url)
    if data is None:
        return None
    obj = json.loads(data)
    if isinstance(obj, dict) and "message" in obj:
        msg = obj["message"]
        if "rate limit" in msg.lower():
            reset = obj.get("documentation_url", "")
            print(f"  GitHub rate limit hit. Wait 60s …", file=sys.stderr)
            time.sleep(60)
            data = _get(url)
            return json.loads(data) if data else None
        print(f"  API error: {msg}", file=sys.stderr)
        return None
    return obj


def _save(path: Path, content: bytes) -> bool:
    if path.exists():
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content)
    return True


def _safe_name(name: str) -> str:
    name = name.strip()
    name = re.sub(r'[<>:"/\\|?*()]', "_", name)
    name = re.sub(r"\s+", "_", name)
    return name or "Unknown"


# ── Flipper-IRDB ──────────────────────────────────────────────────────────────

def fetch_flipper_tree():
    print("Fetching Flipper-IRDB file tree …")
    url = f"{GITHUB_API}/repos/{FLIPPER_REPO}/git/trees/main?recursive=1"
    tree_obj = _api_get(url)
    if not tree_obj:
        print("  Failed — skipping Flipper-IRDB.", file=sys.stderr)
        return []

    tree = tree_obj.get("tree", [])
    entries = []
    for item in tree:
        if item.get("type") != "blob":
            continue
        p = item["path"]
        if not p.lower().endswith(".ir"):
            continue
        parts = p.split("/")
        if len(parts) < 3:
            continue
        category = parts[0]
        if category in FLIPPER_SKIP_PREFIXES:
            continue
        brand    = parts[1]
        filename = parts[-1]
        entries.append((category, brand, filename, p))

    print(f"  Found {len(entries)} .ir files (excluding _Converted_).")
    return entries


def download_flipper():
    entries = fetch_flipper_tree()
    written = skipped_existing = skipped_category = 0

    for category, brand, filename, api_path in entries:
        local_type  = FLIPPER_CATEGORY_MAP.get(category, _safe_name(category))
        local_brand = _safe_name(brand)
        model       = Path(filename).stem
        local_file  = OUT_ROOT / local_type / local_brand / f"{_safe_name(model)}.ir"

        if local_file.exists():
            skipped_existing += 1
            continue

        raw_url = f"{FLIPPER_RAW}/{urllib.parse.quote(api_path)}"
        content = _get(raw_url)
        if content is None:
            continue

        if _save(local_file, content):
            written += 1
            if written % 200 == 0:
                print(f"  … {written} files written")
        else:
            skipped_existing += 1

    print(f"Flipper-IRDB: {written} new, {skipped_existing} already existed, "
          f"{skipped_category} skipped (category).")
    return written


# ── probonopd/irdb ────────────────────────────────────────────────────────────

def _hex_le(value: int, byte_width: int) -> str:
    b = value.to_bytes(byte_width, "little")
    return " ".join(f"{x:02X}" for x in b)


def _probono_csv_to_ir(csv_text: str) -> str | None:
    lines = csv_text.strip().splitlines()
    if not lines:
        return None

    header = [h.strip().lower() for h in lines[0].split(",")]
    try:
        idx_fn   = header.index("functionname")
        idx_dev  = header.index("device")
        idx_sub  = header.index("subdevice")
        idx_cmd  = header.index("function")
        idx_prot = header.index("protocol")
    except ValueError:
        return None

    buttons = []
    seen = set()
    for row in lines[1:]:
        cols = row.split(",")
        if len(cols) <= max(idx_fn, idx_dev, idx_sub, idx_cmd, idx_prot):
            continue
        proto_raw = cols[idx_prot].strip().upper()
        proto = PROBONO_PROTO_MAP.get(proto_raw)
        if proto is None:
            continue
        label = cols[idx_fn].strip()
        if not label or label in seen:
            continue
        seen.add(label)
        try:
            device    = int(cols[idx_dev].strip())
            subdevice = int(cols[idx_sub].strip()) if cols[idx_sub].strip() not in ("-1", "") else 0
            command   = int(cols[idx_cmd].strip())
        except ValueError:
            continue
        address = (device & 0xFF) | ((subdevice & 0xFF) << 8)
        buttons.append((label, proto, address, command))

    if not buttons:
        return None

    out = ["Filetype: IR signals file", "Version: 1"]
    for label, proto, address, command in buttons:
        out += ["#", f"name: {label}", "type: parsed", f"protocol: {proto}",
                f"address: {_hex_le(address, 4)}", f"command: {_hex_le(command, 4)}", ""]
    return "\n".join(out)


def fetch_probono_tree(category: str):
    """Use GitHub API to list CSV files under codes/<category>/."""
    url = f"{GITHUB_API}/repos/{PROBONO_REPO}/contents/codes/{urllib.parse.quote(category)}"
    brands_obj = _api_get(url)
    if not isinstance(brands_obj, list):
        return []

    results = []
    for brand_entry in brands_obj:
        if brand_entry.get("type") != "dir":
            continue
        brand = brand_entry["name"]
        brand_url = brand_entry.get("url") or \
            f"{GITHUB_API}/repos/{PROBONO_REPO}/contents/codes/{urllib.parse.quote(category)}/{urllib.parse.quote(brand)}"
        files_obj = _api_get(brand_url)
        if not isinstance(files_obj, list):
            continue
        for f in files_obj:
            if f.get("type") == "file" and f["name"].lower().endswith(".csv"):
                results.append((brand, f["name"], f.get("download_url") or
                    f"{PROBONO_RAW}/{urllib.parse.quote(category)}/{urllib.parse.quote(brand)}/{urllib.parse.quote(f['name'])}"))
    return results


def download_probono():
    print("Fetching probonopd/irdb (gap-fill for brands not in Flipper-IRDB) …")

    existing_brands: dict[str, set[str]] = {}
    if OUT_ROOT.exists():
        for type_dir in OUT_ROOT.iterdir():
            if type_dir.is_dir():
                existing_brands[type_dir.name] = {
                    d.name for d in type_dir.iterdir() if d.is_dir()
                }

    written = skipped_existing = skipped_covered = 0

    for probono_cat, local_type in PROBONO_CATEGORY_MAP.items():
        print(f"  probonopd/{probono_cat} …")
        entries = fetch_probono_tree(probono_cat)
        for brand, csv_name, download_url in entries:
            local_brand = _safe_name(brand)
            if local_type in existing_brands and local_brand in existing_brands[local_type]:
                skipped_covered += 1
                continue
            model = Path(csv_name).stem
            local_file = OUT_ROOT / local_type / local_brand / f"{_safe_name(model)}.ir"
            if local_file.exists():
                skipped_existing += 1
                continue
            csv_data = _get(download_url)
            if csv_data is None:
                continue
            ir_content = _probono_csv_to_ir(csv_data.decode("utf-8", errors="replace"))
            if ir_content is None:
                continue
            if _save(local_file, ir_content.encode("utf-8")):
                written += 1

    print(f"probonopd/irdb: {written} new, {skipped_existing} already existed, "
          f"{skipped_covered} skipped (brand already in Flipper-IRDB).")
    return written


# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    OUT_ROOT.mkdir(parents=True, exist_ok=True)
    print(f"Output directory: {OUT_ROOT}\n")

    total = download_flipper()
    print()
    total += download_probono()

    ir_files = list(OUT_ROOT.rglob("*.ir"))
    total_bytes = sum(f.stat().st_size for f in ir_files)
    print(f"\nDone — {total} new files written.")
    print(f"Total .ir files in data/irdb/: {len(ir_files)}")
    print(f"Total content size: {total_bytes/1024/1024:.2f} MB  "
          f"(LittleFS partition: 9.87 MB)")
    if len(ir_files) > 32:
        print(f"NOTE: firmware IR_MAX_FILES=32 — only first 32 files shown in UI.")
        print("      Increase IR_MAX_FILES in Nesso_base.ino to expose more.")


if __name__ == "__main__":
    main()
