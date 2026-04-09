"""
Fetch IR codes from irdb using raw content URLs (no API rate limit).
Tries common CSV filenames for each brand's TV/AV folder.
"""
import urllib.request, sys, time
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

BASE = "https://raw.githubusercontent.com/probonopd/irdb/master/codes"

def fetch(url):
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
        with urllib.request.urlopen(req, timeout=8) as r:
            return r.read().decode("utf-8", errors="replace")
    except:
        return None

def parse_csv(text):
    result = {}
    if not text:
        return result
    for line in text.splitlines():
        parts = [p.strip() for p in line.split(",")]
        if len(parts) < 5:
            continue
        try:
            d, s, f = int(parts[2]), int(parts[3]), int(parts[4])
        except:
            continue
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

def encode(entry, proto_hint="NEC"):
    if entry is None: return 0
    proto, d, s, f = entry
    pu = proto.upper()
    if any(x in pu for x in ("NEC","SAMSUNG")): return nec_code(d, s, f)
    if "RC5" in pu or "RC-5" in pu:             return rc5_code(d, f)
    if "SONY" in pu:                             return sony_code(d, f)
    if "SHARP" in pu:                            return f
    if "JVC" in pu:                              return nec_code(d, s, f)
    if "LG" in pu:                               return nec_code(d, s, f)
    return 0

PROTO_MAP = {
    "NEC":"IRDB_NEC","NEC1":"IRDB_NEC","NEC2":"IRDB_NEC","NECX1":"IRDB_NEC","NECX2":"IRDB_NEC",
    "SAMSUNG":"IRDB_SAMSUNG",
    "RC5":"IRDB_RC5","RC-5":"IRDB_RC5",
    "SONY12":"IRDB_SONY","SONY15":"IRDB_SONY","SONY20":"IRDB_SONY","SONY":"IRDB_SONY",
    "JVC":"IRDB_JVC","SHARP":"IRDB_SHARP","LG":"IRDB_LG",
}

def try_csv(brand, dtype, csv_name):
    url = f"{BASE}/{urllib.request.quote(brand)}/{dtype}/{urllib.request.quote(csv_name)}"
    text = fetch(url)
    if not text or "404" in text[:20]:
        return None
    parsed = parse_csv(text)

    power = find_code(parsed, "POWER","POWER TOGGLE","ON/OFF","STANDBY","POWER ON/OFF","POWER (MAIN)","POWER (RCVR)")
    vup   = find_code(parsed, "VOLUME +","VOLUME UP","VOL+","VOLUP","VOL +","VOLUME^","VOL+/CURSOR RT")
    vdn   = find_code(parsed, "VOLUME -","VOLUME DOWN","VOL-","VOLDN","VOL -","VOLUMEV")
    mute  = find_code(parsed, "MUTE","MUTING","VOLUME - MUTE","KEY_MUTE","MUTE TOGGLE")
    cup   = find_code(parsed, "CHANNEL +","CHANNEL UP","CH+","CH UP","CHANNEL+","CHUP","PRESET UP","PRESET +","TUNER STATION + (CH)")
    cdn   = find_code(parsed, "CHANNEL -","CHANNEL DOWN","CH-","CH DOWN","CHANNEL-","CHDN","PRESET DOWN","PRESET DN","TUNER STATION - (CH)")

    found = [x for x in [power, vup, vdn, mute, cup, cdn] if x is not None]
    if len(found) < 4:
        return None

    sample = found[0]
    pu = sample[0].upper().split(",")[0].strip()
    irproto = PROTO_MAP.get(pu)
    if not irproto:
        return None

    addr = sample[1] if "SHARP" in pu else 0
    nbits = (12 if "12" in sample[0] else 15 if "15" in sample[0] else 12) if irproto == "IRDB_SONY" else \
            12 if irproto == "IRDB_RC5" else 16 if irproto == "IRDB_JVC" else \
            15 if irproto == "IRDB_SHARP" else 32

    return (irproto, nbits, addr,
            encode(power), encode(vup,pu), encode(vdn,pu),
            encode(mute,pu), encode(cup,pu), encode(cdn,pu))

# ----------------------------------------------------------------
# Build candidate list: (brand, dtype_folder, display_type, csv_candidates)
# Generated from known irdb structure + common filename patterns
# ----------------------------------------------------------------

TV_BRANDS = [
    # brand,  dtype,   display,  csv_guesses (first hit wins)
    ("ADLER",       "TV",         "TV",       ["2,-1.csv","0,-1.csv"]),
    ("AOC",         "TV",         "TV",       ["0,-1.csv","1,-1.csv","4,-1.csv"]),
    ("Aiwa",        "TV",         "TV",       ["0,-1.csv","1,-1.csv"]),
    ("Akai",        "TV",         "TV",       ["0,-1.csv","5,-1.csv","16,-1.csv"]),
    ("Amstrad",     "TV",         "TV",       ["0,-1.csv","1,-1.csv"]),
    ("Apex",        "TV",         "TV",       ["0,-1.csv","1,-1.csv"]),
    ("Audiovox",    "TV",         "TV",       ["0,-1.csv","1,-1.csv"]),
    ("Bang Olufsen","TV",         "TV",       ["0,-1.csv","1,-1.csv"]),
    ("Blaupunkt",   "TV",         "TV",       ["0,-1.csv","1,-1.csv"]),
    ("Changhong",   "TV",         "TV",       ["0,-1.csv","1,-1.csv"]),
    ("Daewoo",      "TV",         "TV",       ["0,-1.csv","1,-1.csv","3,-1.csv"]),
    ("Dual",        "TV",         "TV",       ["0,-1.csv","1,-1.csv"]),
    ("Finlux",      "TV",         "TV",       ["0,-1.csv","1,-1.csv"]),
    ("Funai",       "TV",         "TV",       ["0,-1.csv","1,-1.csv"]),
    ("FUNAI",       "TV",         "TV",       ["0,-1.csv","1,-1.csv"]),
    ("Insignia",    "TV",         "TV",       ["134,5.csv","0,-1.csv","1,-1.csv"]),
    ("ITT",         "TV",         "TV",       ["0,-1.csv","1,-1.csv"]),
    ("Kenwood",     "Receiver",   "AV Rcvr",  ["0,-1.csv","1,-1.csv"]),
    ("Metz",        "TV",         "TV",       ["0,-1.csv","1,-1.csv"]),
    ("NEC",         "TV",         "TV",       ["0,-1.csv","1,-1.csv"]),
    ("Nokia",       "TV",         "TV",       ["0,-1.csv","1,-1.csv"]),
    ("Nordmende",   "TV",         "TV",       ["0,-1.csv","1,-1.csv"]),
    ("Onkyo Integra","Receiver",  "AV Rcvr",  ["210,109.csv","0,-1.csv"]),
    ("Orion",       "TV",         "TV",       ["0,-1.csv","1,-1.csv"]),
    ("Philco",      "TV",         "TV",       ["0,-1.csv","1,-1.csv"]),
    ("Polaroid",    "TV",         "TV",       ["0,-1.csv","1,-1.csv","4,-1.csv"]),
    ("RCA",         "TV",         "TV",       ["15,-1.csv","14,-1.csv","7,-1.csv"]),
    ("Saba",        "TV",         "TV",       ["0,-1.csv","1,-1.csv"]),
    ("Salora",      "TV",         "TV",       ["0,-1.csv","1,-1.csv"]),
    ("Sylvania",    "TV",         "TV",       ["0,-1.csv","1,-1.csv"]),
    ("TELEFUNKEN",  "TV",         "TV",       ["0,-1.csv","1,-1.csv","7,-1.csv"]),
    ("Telefunken",  "TV",         "TV",       ["0,-1.csv","1,-1.csv"]),
    ("Thomson",     "TV",         "TV",       ["0,-1.csv","7,-1.csv","1,-1.csv"]),
    ("Vizio",       "TV",         "TV",       ["4,-1.csv","0,-1.csv","1,-1.csv"]),
    ("Winkel",      "TV",         "TV",       ["0,-1.csv","1,-1.csv"]),
    ("Zenith",      "TV",         "TV",       ["0,-1.csv","1,-1.csv"]),
    # AV Receivers
    ("Adcom",       "Receiver",   "AV Rcvr",  ["26,-1.csv","0,-1.csv"]),
    ("Arcam",       "Receiver",   "AV Rcvr",  ["16,-1.csv","0,-1.csv"]),
    ("Kenwood",     "Receiver",   "AV Rcvr",  ["0,-1.csv","1,-1.csv"]),
    ("Luxman",      "Receiver",   "AV Rcvr",  ["0,-1.csv","1,-1.csv"]),
    ("Nad",         "Receiver",   "AV Rcvr",  ["0,-1.csv","16,-1.csv"]),
    ("Onkyo Integra","Receiver",  "AV Rcvr",  ["210,109.csv","0,-1.csv"]),
    ("Rotel",       "Receiver",   "AV Rcvr",  ["0,-1.csv","16,-1.csv"]),
    # Soundbars
    ("Aiwa",        "Soundbar",   "Soundbar", ["0,-1.csv","1,-1.csv"]),
]

ALREADY_DONE = {
    "Samsung","LG","Sony","Philips","Grundig","Panasonic","Sharp","Toshiba",
    "TCL","Vestel","Arcelik","Beko","Hisense","Haier","JVC","Pioneer","Denon",
    "Hitachi","Sanyo","Emerson","Magnavox","Loewe","Mitsubishi","Onkyo","Yamaha",
    "Marantz","Bose"
}

def process_brand(item):
    brand, dtype, disp, csvs = item
    if brand in ALREADY_DONE:
        return None
    for csv_name in csvs:
        result = try_csv(brand, dtype, csv_name)
        if result:
            irproto, nbits, addr, pw, vu, vd, mt, cu, cd = result
            dev_name = csv_name.replace(".csv","").replace(",","/")
            return (brand, disp, dev_name, irproto, nbits, addr, pw, vu, vd, mt, cu, cd)
    return None

print("Fetching IR codes from irdb (raw content)...", file=sys.stderr)

results = []
seen_brands = set()

with ThreadPoolExecutor(max_workers=8) as ex:
    futures = {ex.submit(process_brand, item): item for item in TV_BRANDS}
    for fut in as_completed(futures):
        r = fut.result()
        if r and r[0] not in seen_brands and r[0] not in ALREADY_DONE:
            seen_brands.add(r[0])
            results.append(r)
            brand, disp, name, proto, *_ = r
            print(f"  OK: {brand}/{disp} [{proto}]", file=sys.stderr)

print(f"\nNew entries: {len(results)}", file=sys.stderr)

def fmt(v, nbits):
    if nbits == 32: return f"0x{v:08X}"
    return f"0x{v:04X}"

print("\n// === AUTO-GENERATED from probonopd/irdb ===")
for r in sorted(results, key=lambda x: (x[1], x[0])):
    brand, dtype, name, proto, nbits, addr, pw, vu, vd, mt, cu, cd = r
    if pw == 0 and vu == 0:
        continue
    print(f'  {{"{brand:<12}","{dtype:<8}","{name:<14}",{proto:<14},{nbits:2},{addr},'
          f'{fmt(pw,nbits)},{fmt(vu,nbits)},{fmt(vd,nbits)},{fmt(mt,nbits)},{fmt(cu,nbits)},{fmt(cd,nbits)}}},')
