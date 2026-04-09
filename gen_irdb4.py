"""
Comprehensive irdb scraper using only raw.githubusercontent.com (no API).
Tries device numbers 0-30 with subdevice -1 for each brand/type combo.
Uses a comprehensive hardcoded brand list from the irdb repository.
"""
import urllib.request, sys
from concurrent.futures import ThreadPoolExecutor, as_completed

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

RAW = "https://raw.githubusercontent.com/probonopd/irdb/master/codes"

def fetch(url):
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
        with urllib.request.urlopen(req, timeout=8) as r:
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

def encode(entry):
    if entry is None: return 0
    proto, d, s, f = entry
    pu = proto.upper()
    if any(x in pu for x in ("NEC","SAMSUNG","LG","JVC")): return nec_code(d, s, f)
    if "RC5" in pu or "RC-5" in pu: return rc5_code(d, f)
    if "SONY" in pu: return sony_code(d, f)
    if "SHARP" in pu: return f
    return 0

def try_csv(brand, dtype, csv_name):
    url = f"{RAW}/{urllib.request.quote(brand)}/{dtype}/{urllib.request.quote(csv_name)}"
    text = fetch(url)
    if not text or "404" in text[:40] or len(text) < 20: return None
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
    elif irproto == "IRDB_RC5":  nbits = 12
    elif irproto == "IRDB_JVC":  nbits = 16
    elif irproto == "IRDB_SHARP": nbits = 15
    else: nbits = 32

    pw = encode(power); vu = encode(vup); vd = encode(vdn)
    mt = encode(mute);  cu = encode(cup); cd = encode(cdn)
    if pw == 0 and vu == 0: return None

    dev_name = csv_name.replace(".csv","").replace(",","/")
    return (irproto, nbits, addr, pw, vu, vd, mt, cu, cd, dev_name)

# Comprehensive brand list from irdb with (brand, dtype, display, specific_csvs)
# specific_csvs: try these first before the generic range scan
BRANDS = [
    # ── TVs ──────────────────────────────────────────────────────────────────
    ("Acer",          "TV",       "TV",      []),
    ("Advent",        "TV",       "TV",      []),
    ("Aiwa",          "TV",       "TV",      []),
    ("Akai",          "TV",       "TV",      []),
    ("Akira",         "TV",       "TV",      []),
    ("Alba",          "TV",       "TV",      []),
    ("ADLER",         "TV",       "TV",      ["2,-1.csv"]),
    ("Amstrad",       "TV",       "TV",      []),
    ("AOC",           "TV",       "TV",      []),
    ("Apex",          "TV",       "TV",      []),
    ("Audiovox",      "TV",       "TV",      []),
    ("Bang Olufsen",  "TV",       "TV",      []),
    ("Belson",        "TV",       "TV",      []),
    ("Benq",          "TV",       "TV",      []),
    ("Blaupunkt",     "TV",       "TV",      []),
    ("Bush",          "TV",       "TV",      []),
    ("Changhong",     "TV",       "TV",      []),
    ("Coby",          "TV",       "TV",      []),
    ("Curtis",        "TV",       "TV",      []),
    ("Daewoo",        "TV",       "TV",      []),
    ("Dell",          "TV",       "TV",      []),
    ("Dual",          "TV",       "TV",      []),
    ("Element",       "TV",       "TV",      []),
    ("Elbe",          "TV",       "TV",      []),
    ("Finlux",        "TV",       "TV",      []),
    ("Funai",         "TV",       "TV",      []),
    ("FUNAI",         "TV",       "TV",      []),
    ("GE",            "TV",       "TV",      []),
    ("Goodmans",      "TV",       "TV",      []),
    ("Hannspree",     "TV",       "TV",      []),
    ("Hyundai",       "TV",       "TV",      []),
    ("Iiyama",        "TV",       "TV",      []),
    ("Insignia",      "TV",       "TV",      ["134,5.csv"]),
    ("ITT",           "TV",       "TV",      []),
    ("Medion",        "TV",       "TV",      []),
    ("Metz",          "TV",       "TV",      []),
    ("NEC",           "TV",       "TV",      []),
    ("Nokia",         "TV",       "TV",      []),
    ("Nordmende",     "TV",       "TV",      []),
    ("Orion",         "TV",       "TV",      []),
    ("Philco",        "TV",       "TV",      []),
    ("Polaroid",      "TV",       "TV",      []),
    ("Proview",       "TV",       "TV",      []),
    ("RCA",           "TV",       "TV",      ["15,-1.csv","14,-1.csv","7,-1.csv"]),
    ("Roadstar",      "TV",       "TV",      []),
    ("Saba",          "TV",       "TV",      []),
    ("Salora",        "TV",       "TV",      []),
    ("Schneider",     "TV",       "TV",      []),
    ("Sylvania",      "TV",       "TV",      []),
    ("TELEFUNKEN",    "TV",       "TV",      []),
    ("Telefunken",    "TV",       "TV",      []),
    ("Thomson",       "TV",       "TV",      []),
    ("Vestel",        "TV",       "TV",      []),  # NOTE: in SKIP but let's keep for now
    ("ViewSonic",     "TV",       "TV",      []),
    ("Vizio",         "TV",       "TV",      ["4,-1.csv"]),
    ("Winkel",        "TV",       "TV",      []),
    ("Zenith",        "TV",       "TV",      []),
    # ── AV Receivers / Amplifiers ────────────────────────────────────────────
    ("Adcom",         "Receiver", "AV Rcvr", ["26,-1.csv"]),
    ("Arcam",         "Receiver", "AV Rcvr", ["16,-1.csv"]),
    ("Cambridge Audio","Receiver","AV Rcvr", []),
    ("Harman Kardon", "Receiver", "AV Rcvr", []),
    ("Integra",       "Receiver", "AV Rcvr", []),
    ("Kenwood",       "Receiver", "AV Rcvr", []),
    ("Luxman",        "Receiver", "AV Rcvr", []),
    ("NAD",           "Receiver", "AV Rcvr", []),
    ("Nad",           "Receiver", "AV Rcvr", []),
    ("Onkyo Integra", "Receiver", "AV Rcvr", ["210,109.csv"]),
    ("Rotel",         "Receiver", "AV Rcvr", []),
    ("Sherwood",      "Receiver", "AV Rcvr", []),
    ("Teac",          "Receiver", "AV Rcvr", []),
    # ── Soundbars ────────────────────────────────────────────────────────────
    ("Aiwa",          "Soundbar", "Soundbar",[]),
    ("LG",            "Soundbar", "Soundbar",[]),  # LG soundbar (different from LG TV)
    ("Samsung",       "Soundbar", "Soundbar",[]),  # Samsung soundbar
    ("Vizio",         "Soundbar", "Soundbar",[]),
]

SKIP = {
    "Samsung","LG","Sony","Philips","Grundig","Panasonic","Sharp","Toshiba",
    "TCL","Arcelik","Beko","Hisense","Haier","JVC","Pioneer","Denon",
    "Hitachi","Sanyo","Emerson","Magnavox","Loewe","Mitsubishi","Onkyo","Yamaha",
    "Marantz","Bose",
    # from gen_irdb2 results
    "Adcom","Arcam","Onkyo Integra","ADLER","Insignia",
}

# Build work items: each is (brand, dtype, disp, csv_name)
work_items = []
for brand, dtype, disp, specific in BRANDS:
    if brand in SKIP: continue
    # Try specific CSVs first, then generic range
    csvs = list(specific)
    for dev in range(0, 31):
        csvs.append(f"{dev},-1.csv")
    # also try some two-byte combinations common in AV receivers
    for dev in [120, 122, 64, 68, 80, 128, 210, 48, 32, 16, 8, 4, 2, 1]:
        csvs.append(f"{dev},-1.csv")
    for csv_name in csvs:
        work_items.append((brand, dtype, disp, csv_name))

print(f"Checking {len(work_items)} URLs across {len([b for b,*_ in BRANDS if b not in SKIP])} brands...", file=sys.stderr)

def worker(item):
    brand, dtype, disp, csv_name = item
    result = try_csv(brand, dtype, csv_name)
    if result:
        irproto, nbits, addr, pw, vu, vd, mt, cu, cd, dev_name = result
        return (brand, disp, dev_name, irproto, nbits, addr, pw, vu, vd, mt, cu, cd)
    return None

results = []
seen_brands = set()

with ThreadPoolExecutor(max_workers=16) as ex:
    futures = {ex.submit(worker, item): item for item in work_items}
    for fut in as_completed(futures):
        r = fut.result()
        if r and r[0] not in seen_brands:
            seen_brands.add(r[0])
            results.append(r)
            print(f"  OK: {r[0]}/{r[1]} [{r[3]}] dev={r[2]}", file=sys.stderr)

print(f"\nNew entries: {len(results)}", file=sys.stderr)

def fmt(v, nbits):
    return f"0x{v:08X}" if nbits == 32 else f"0x{v:04X}"

print("\n// === AUTO-GENERATED from probonopd/irdb ===")
for r in sorted(results, key=lambda x: (x[1], x[0])):
    brand, dtype, name, proto, nbits, addr, pw, vu, vd, mt, cu, cd = r
    print(f'  {{"{brand:<12}","{dtype:<8}","{name:<14}",{proto:<14},{nbits:2},{addr},'
          f'{fmt(pw,nbits)},{fmt(vu,nbits)},{fmt(vd,nbits)},{fmt(mt,nbits)},{fmt(cu,nbits)},{fmt(cd,nbits)}}},')
