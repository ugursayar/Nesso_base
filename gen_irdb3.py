"""
Fetch IR codes from irdb using a single GitHub tree API call to enumerate
all CSV files, then download raw content (no per-file API calls).
"""
import urllib.request, json, sys, time, re
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

RAW_BASE = "https://raw.githubusercontent.com/probonopd/irdb/master/codes"
API_TREE  = "https://api.github.com/repos/probonopd/irdb/git/trees/master?recursive=1"

def fetch(url, timeout=10):
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.read().decode("utf-8", errors="replace")
    except:
        return None

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

def encode(entry, proto_hint=""):
    if entry is None: return 0
    proto, d, s, f = entry
    pu = proto.upper()
    if any(x in pu for x in ("NEC","SAMSUNG","LG","JVC")): return nec_code(d, s, f)
    if "RC5" in pu or "RC-5" in pu: return rc5_code(d, f)
    if "SONY" in pu: return sony_code(d, f)
    if "SHARP" in pu: return f
    return 0

DTYPE_PRIORITY = ["TV", "Soundbar", "Sound_Bar", "Receiver", "AV_Receiver",
                  "DVD_Player", "Blu-ray_Player", "Projector", "Set_Top_Box"]
DTYPE_DISPLAY  = {
    "TV":"TV", "Soundbar":"Soundbar", "Sound_Bar":"Soundbar",
    "Receiver":"AV Rcvr", "AV_Receiver":"AV Rcvr",
    "DVD_Player":"DVD", "Blu-ray_Player":"Blu-ray",
    "Projector":"Projector", "Set_Top_Box":"STB",
}

SKIP = {
    "Samsung","LG","Sony","Philips","Grundig","Panasonic","Sharp","Toshiba",
    "TCL","Vestel","Arcelik","Beko","Hisense","Haier","JVC","Pioneer","Denon",
    "Hitachi","Sanyo","Emerson","Magnavox","Loewe","Mitsubishi","Onkyo","Yamaha",
    "Marantz","Bose",
    # already added via gen_irdb2
    "Onkyo Integra","ADLER","Insignia","Adcom","Arcam",
}

print("Fetching full irdb tree (1 API call)...", file=sys.stderr)
tree_text = fetch(API_TREE, timeout=30)
if not tree_text:
    print("ERROR: could not fetch tree", file=sys.stderr)
    sys.exit(1)

tree_data = json.loads(tree_text)
if tree_data.get("truncated"):
    print("WARNING: tree was truncated!", file=sys.stderr)

# Build: brand -> dtype -> [csv_paths_relative]
brand_types = defaultdict(lambda: defaultdict(list))
for item in tree_data.get("tree", []):
    path = item.get("path","")
    if not path.startswith("codes/") or not path.endswith(".csv"):
        continue
    parts = path.split("/")
    if len(parts) != 4:  # codes/Brand/Type/file.csv
        continue
    _, brand, dtype, csv_name = parts
    brand_types[brand][dtype].append(csv_name)

print(f"Found {len(brand_types)} brands in irdb", file=sys.stderr)

# For each brand, pick best dtype, try up to 3 CSVs
def process_brand(brand):
    if brand in SKIP:
        return None
    dtypes = brand_types[brand]
    for dtype in DTYPE_PRIORITY:
        if dtype not in dtypes:
            continue
        disp = DTYPE_DISPLAY.get(dtype, dtype)
        csvs = sorted(dtypes[dtype])[:5]  # try first 5 (sorted alphabetically)
        for csv_name in csvs:
            url = f"{RAW_BASE}/{urllib.request.quote(brand)}/{dtype}/{urllib.request.quote(csv_name)}"
            text = fetch(url)
            if not text or text.strip().startswith("404"): continue
            parsed = parse_csv(text)

            power = find_code(parsed, "POWER","POWER TOGGLE","ON/OFF","STANDBY","POWER ON/OFF","POWER (MAIN)","POWER (RCVR)")
            vup   = find_code(parsed, "VOLUME +","VOLUME UP","VOL+","VOLUP","VOL +","VOLUME^","VOL+/CURSOR RT")
            vdn   = find_code(parsed, "VOLUME -","VOLUME DOWN","VOL-","VOLDN","VOL -","VOLUMEV")
            mute  = find_code(parsed, "MUTE","MUTING","VOLUME - MUTE","KEY_MUTE","MUTE TOGGLE")
            cup   = find_code(parsed, "CHANNEL +","CHANNEL UP","CH+","CH UP","CHANNEL+","CHUP","PRESET UP","PRESET +","TUNER STATION + (CH)")
            cdn   = find_code(parsed, "CHANNEL -","CHANNEL DOWN","CH-","CH DOWN","CHANNEL-","CHDN","PRESET DOWN","PRESET DN","TUNER STATION - (CH)")

            found = [x for x in [power, vup, vdn, mute, cup, cdn] if x is not None]
            if len(found) < 4: continue

            sample = found[0]
            pu = sample[0].upper().split(",")[0].strip()
            irproto = PROTO_MAP.get(pu)
            if not irproto: continue

            addr = sample[1] if "SHARP" in pu else 0
            if irproto == "IRDB_SONY":
                nbits = 12 if "12" in sample[0] else 15 if "15" in sample[0] else 12
            elif irproto == "IRDB_RC5":  nbits = 12
            elif irproto == "IRDB_JVC":  nbits = 16
            elif irproto == "IRDB_SHARP": nbits = 15
            else: nbits = 32

            dev_name = csv_name.replace(".csv","").replace(",","/")
            pw = encode(power); vu = encode(vup); vd = encode(vdn)
            mt = encode(mute);  cu = encode(cup); cd = encode(cdn)
            if pw == 0 and vu == 0: continue
            return (brand, disp, dev_name, irproto, nbits, addr, pw, vu, vd, mt, cu, cd)
        # if we found a dtype but no valid CSV, try next dtype
    return None

brands = [b for b in brand_types if b not in SKIP]
print(f"Processing {len(brands)} brands (skipping {len(SKIP)} already done)...", file=sys.stderr)

results = []
seen = set()

with ThreadPoolExecutor(max_workers=12) as ex:
    futures = {ex.submit(process_brand, b): b for b in brands}
    for fut in as_completed(futures):
        r = fut.result()
        if r and r[0] not in seen:
            seen.add(r[0])
            results.append(r)
            print(f"  OK: {r[0]}/{r[1]} [{r[3]}]", file=sys.stderr)

print(f"\nNew entries: {len(results)}", file=sys.stderr)

def fmt(v, nbits):
    return f"0x{v:08X}" if nbits == 32 else f"0x{v:04X}"

print("\n// === AUTO-GENERATED from probonopd/irdb ===")
for r in sorted(results, key=lambda x: (x[1], x[0])):
    brand, dtype, name, proto, nbits, addr, pw, vu, vd, mt, cu, cd = r
    print(f'  {{"{brand:<12}","{dtype:<8}","{name:<14}",{proto:<14},{nbits:2},{addr},'
          f'{fmt(pw,nbits)},{fmt(vu,nbits)},{fmt(vd,nbits)},{fmt(mt,nbits)},{fmt(cu,nbits)},{fmt(cd,nbits)}}},')
