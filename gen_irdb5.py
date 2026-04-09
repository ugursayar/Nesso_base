"""
Comprehensive irdb scraper:
- 1 API call to get brand list (codes/ directory)
- HTML scraping to find type subfolders and CSV filenames
- raw.githubusercontent.com for CSV content (no rate limit)
"""
import urllib.request, json, sys, re, time
from concurrent.futures import ThreadPoolExecutor, as_completed
from collections import defaultdict

def bitrev8(b):
    b &= 0xFF
    b = ((b & 0xF0) >> 4) | ((b & 0x0F) << 4)
    b = ((b & 0xCC) >> 2) | ((b & 0x33) << 2)
    b = ((b & 0xAA) >> 1) | ((b & 0x55) << 1)
    return b

def nec_code(device, subdevice, func):
    sub = ((~device) & 0xFF) if subdevice == -1 else (subdevice & 0xFF)
    return (bitrev8(device) << 24) | (bitrev8(sub) << 16) | (bitrev8(func) << 8) | bitrev8((~func) & 0xFF)

def rc5_code(device, func):   return (device << 6) | func
def sony_code(device, func):  return (device << 7) | func

RAW  = "https://raw.githubusercontent.com/probonopd/irdb/master/codes"
API  = "https://api.github.com/repos/probonopd/irdb/contents/codes"
HTML = "https://github.com/probonopd/irdb/tree/master/codes"

HEADERS_API  = {"User-Agent": "Mozilla/5.0", "Accept": "application/vnd.github.v3+json"}
HEADERS_HTML = {"User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"}

def fetch_api(url):
    try:
        req = urllib.request.Request(url, headers=HEADERS_API)
        with urllib.request.urlopen(req, timeout=15) as r:
            return json.loads(r.read())
    except Exception as e:
        print(f"API error {url}: {e}", file=sys.stderr)
        return None

def fetch_html(url):
    try:
        req = urllib.request.Request(url, headers=HEADERS_HTML)
        with urllib.request.urlopen(req, timeout=12) as r:
            return r.read().decode("utf-8", errors="replace")
    except:
        return None

def fetch_raw(url):
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
        with urllib.request.urlopen(req, timeout=8) as r:
            return r.read().decode("utf-8", errors="replace")
    except:
        return None

def html_entries(html):
    """Extract (name, is_dir) from GitHub tree HTML."""
    if not html: return []
    dirs  = list(dict.fromkeys(re.findall(r'aria-label=\"([^\"]+) \(Directory\)\"', html)))
    files = list(dict.fromkeys(re.findall(r'aria-label=\"([^\"]+) \(File\)\"', html)))
    return [(d.rstrip(","), True) for d in dirs] + [(f.rstrip(","), False) for f in files]

def parse_csv(text):
    result = {}
    if not text: return result
    for line in text.splitlines():
        parts = [p.strip() for p in line.split(",")]
        if len(parts) < 5: continue
        try: d, s, f = int(parts[2]), int(parts[3]), int(parts[4])
        except: continue
        result[parts[0].upper().strip()] = (parts[1], d, s, f)
    return result

def find_code(p, *keys):
    for k in keys:
        if k.upper() in p: return p[k.upper()]
    for k in keys:
        ku = k.upper()
        for pk in p:
            if ku in pk: return p[pk]
    return None

PROTO_MAP = {
    "NEC":"IRDB_NEC","NEC1":"IRDB_NEC","NEC2":"IRDB_NEC","NECX1":"IRDB_NEC","NECX2":"IRDB_NEC",
    "SAMSUNG":"IRDB_SAMSUNG","SAMSUNG32":"IRDB_SAMSUNG",
    "RC5":"IRDB_RC5","RC-5":"IRDB_RC5",
    "SONY12":"IRDB_SONY","SONY15":"IRDB_SONY","SONY20":"IRDB_SONY","SONY":"IRDB_SONY",
    "JVC":"IRDB_JVC","SHARP":"IRDB_SHARP","LG":"IRDB_LG",
}

def encode(entry):
    if entry is None: return 0
    proto, d, s, f = entry
    pu = proto.upper()
    if any(x in pu for x in ("NEC","SAMSUNG","LG","JVC")): return nec_code(d, s, f)
    if "RC5" in pu or "RC-5" in pu: return rc5_code(d, f)
    if "SONY" in pu: return sony_code(d, f)
    if "SHARP" in pu: return f
    return 0

DTYPE_PRIORITY = ["TV", "Soundbar", "Sound_Bar", "Receiver", "AV_Receiver",
                  "DVD_Player", "Blu-ray_Player", "Projector"]
DTYPE_DISPLAY  = {
    "TV":"TV", "Soundbar":"Soundbar", "Sound_Bar":"Soundbar",
    "Receiver":"AV Rcvr", "AV_Receiver":"AV Rcvr",
    "DVD_Player":"DVD", "Blu-ray_Player":"Blu-ray",
    "Projector":"Projector",
}

SKIP = {
    "Samsung","LG","Sony","Philips","Grundig","Panasonic","Sharp","Toshiba",
    "TCL","Vestel","Arcelik","Beko","Hisense","Haier","JVC","Pioneer","Denon",
    "Hitachi","Sanyo","Emerson","Magnavox","Loewe","Mitsubishi","Onkyo","Yamaha",
    "Marantz","Bose",
    "Onkyo Integra","ADLER","Insignia","Adcom","Arcam","NEC",
}

def try_csv_content(brand, dtype, csv_name):
    url = f"{RAW}/{urllib.request.quote(brand)}/{dtype}/{urllib.request.quote(csv_name)}"
    text = fetch_raw(url)
    if not text or len(text) < 30: return None
    parsed = parse_csv(text)
    if not parsed: return None

    power = find_code(parsed, "POWER","POWER TOGGLE","ON/OFF","STANDBY","POWER ON/OFF","POWER (MAIN)","POWER (RCVR)")
    vup   = find_code(parsed, "VOLUME +","VOLUME UP","VOL+","VOLUP","VOL +","VOLUME^","VOL+/CURSOR RT")
    vdn   = find_code(parsed, "VOLUME -","VOLUME DOWN","VOL-","VOLDN","VOL -","VOLUMEV")
    mute  = find_code(parsed, "MUTE","MUTING","VOLUME - MUTE","KEY_MUTE","MUTE TOGGLE")
    cup   = find_code(parsed, "CHANNEL +","CHANNEL UP","CH+","CH UP","CHANNEL+","CHUP","PRESET UP","PRESET +","TUNER STATION + (CH)")
    cdn   = find_code(parsed, "CHANNEL -","CHANNEL DOWN","CH-","CH DOWN","CHANNEL-","CHDN","PRESET DOWN","PRESET DN","TUNER STATION - (CH)")

    found = [x for x in [power, vup, vdn, mute, cup, cdn] if x is not None]
    if len(found) < 4: return None

    sample = found[0]
    pu = sample[0].upper().split(",")[0].strip()
    irproto = PROTO_MAP.get(pu)
    if not irproto: return None

    addr = sample[1] if "SHARP" in pu else 0
    if irproto == "IRDB_SONY":
        nbits = 12 if "12" in sample[0] else 15 if "15" in sample[0] else 12
    elif irproto == "IRDB_RC5":   nbits = 12
    elif irproto == "IRDB_JVC":   nbits = 16
    elif irproto == "IRDB_SHARP": nbits = 15
    else: nbits = 32

    pw = encode(power); vu = encode(vup); vd = encode(vdn)
    mt = encode(mute);  cu = encode(cup); cd = encode(cdn)
    if pw == 0 and vu == 0: return None

    dev_name = csv_name.replace(".csv","").replace(",","/")
    return (irproto, nbits, addr, pw, vu, vd, mt, cu, cd, dev_name)

def get_csv_list_for_type(brand, dtype):
    """Get list of CSV filenames by scraping GitHub HTML."""
    url = f"{HTML}/{urllib.request.quote(brand)}/{dtype}"
    html = fetch_html(url)
    if not html: return []
    entries = html_entries(html)
    return [name for name, is_dir in entries if name.endswith(".csv") and not is_dir]

def process_brand(brand):
    if brand in SKIP: return None
    # Get subfolders by scraping HTML
    url = f"{HTML}/{urllib.request.quote(brand)}"
    html = fetch_html(url)
    if not html: return None
    entries = html_entries(html)
    subdirs = [name for name, is_dir in entries if is_dir]

    for dtype in DTYPE_PRIORITY:
        if dtype not in subdirs: continue
        disp = DTYPE_DISPLAY.get(dtype, dtype)
        csvs = get_csv_list_for_type(brand, dtype)
        if not csvs: continue
        for csv_name in csvs[:6]:  # try up to 6 CSVs
            result = try_csv_content(brand, dtype, csv_name)
            if result:
                irproto, nbits, addr, pw, vu, vd, mt, cu, cd, dev_name = result
                return (brand, disp, dev_name, irproto, nbits, addr, pw, vu, vd, mt, cu, cd)
    return None

# ── Step 1: Get brand list using 1 API call ───────────────────────────────
print("Getting brand list from GitHub API...", file=sys.stderr)
data = fetch_api(API)
if not data:
    print("API failed. Try again when rate limit resets.", file=sys.stderr)
    sys.exit(1)

brands = [d["name"] for d in data if d["type"] == "dir"]
print(f"Total brands: {len(brands)}, skipping {len([b for b in brands if b in SKIP])}", file=sys.stderr)
to_process = [b for b in brands if b not in SKIP]
print(f"Processing: {len(to_process)} brands", file=sys.stderr)

# ── Step 2: Process brands in parallel ───────────────────────────────────
results = []
seen = set()

with ThreadPoolExecutor(max_workers=10) as ex:
    futures = {ex.submit(process_brand, b): b for b in to_process}
    done = 0
    for fut in as_completed(futures):
        done += 1
        r = fut.result()
        if r and r[0] not in seen:
            seen.add(r[0])
            results.append(r)
            print(f"  [{done}/{len(to_process)}] OK: {r[0]}/{r[1]} [{r[3]}]", file=sys.stderr)
        elif done % 50 == 0:
            print(f"  [{done}/{len(to_process)}] checked...", file=sys.stderr)

print(f"\nNew entries: {len(results)}", file=sys.stderr)

def fmt(v, nbits):
    return f"0x{v:08X}" if nbits == 32 else f"0x{v:04X}"

print("\n// === AUTO-GENERATED from probonopd/irdb ===")
for r in sorted(results, key=lambda x: (x[1], x[0])):
    brand, dtype, name, proto, nbits, addr, pw, vu, vd, mt, cu, cd = r
    print(f'  {{"{brand:<12}","{dtype:<8}","{name:<14}",{proto:<14},{nbits:2},{addr},'
          f'{fmt(pw,nbits)},{fmt(vu,nbits)},{fmt(vd,nbits)},{fmt(mt,nbits)},{fmt(cu,nbits)},{fmt(cd,nbits)}}},')
