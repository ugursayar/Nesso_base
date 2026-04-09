import urllib.request, json, sys, time

def bitrev8(b):
    b &= 0xFF
    b = ((b & 0xF0) >> 4) | ((b & 0x0F) << 4)
    b = ((b & 0xCC) >> 2) | ((b & 0x33) << 2)
    b = ((b & 0xAA) >> 1) | ((b & 0x55) << 1)
    return b

def nec_code(device, subdevice, func):
    sub = ((~device) & 0xFF) if subdevice == -1 else (subdevice & 0xFF)
    return (bitrev8(device) << 24) | (bitrev8(sub) << 16) | (bitrev8(func) << 8) | bitrev8((~func) & 0xFF)

def rc5_code(device, func):
    return (device << 6) | func

def sony_code(device, func):
    return (device << 7) | func

def fetch_url(url):
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
        with urllib.request.urlopen(req, timeout=10) as r:
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
        name, proto, dev, sub, fn = parts[0], parts[1], parts[2], parts[3], parts[4]
        try:
            d = int(dev); s = int(sub); f = int(fn)
        except:
            continue
        result[name.upper().strip()] = (proto, d, s, f)
    return result

def find_code(parsed, *keys):
    for k in keys:
        ku = k.upper()
        if ku in parsed:
            return parsed[ku]
    for k in keys:
        ku = k.upper()
        for pk in parsed:
            if ku in pk:
                return parsed[pk]
    return None

def get_codes(parsed):
    power = find_code(parsed, "POWER","POWER TOGGLE","ON/OFF","STANDBY","POWER ON/OFF","POWER (MAIN)")
    vup   = find_code(parsed, "VOLUME +","VOLUME UP","VOL+","VOLUP","VOL +","VOLUME^","VOLUME UP/CURSOR RT","VOL+/CURSOR RT")
    vdn   = find_code(parsed, "VOLUME -","VOLUME DOWN","VOL-","VOLDN","VOL -","VOLUMEV")
    mute  = find_code(parsed, "MUTE","MUTING","VOLUME - MUTE","KEY_MUTE","MUTE TOGGLE")
    cup   = find_code(parsed, "CHANNEL +","CHANNEL UP","CH+","CH UP","CHANNEL+","CHUP","PRESET UP","PRESET +","TUNER STATION + (CH)")
    cdn   = find_code(parsed, "CHANNEL -","CHANNEL DOWN","CH-","CH DOWN","CHANNEL-","CHDN","PRESET DOWN","PRESET DN","TUNER STATION - (CH)")

    found = [x for x in [power, vup, vdn, mute, cup, cdn] if x is not None]
    if len(found) < 4:
        return None

    def encode(entry, fallback=None):
        e = entry if entry is not None else fallback
        if e is None:
            return 0
        proto, d, s, f = e
        p = proto.upper()
        if "NEC" in p or "SAMSUNG" in p:
            return nec_code(d, s, f)
        elif "RC5" in p:
            return rc5_code(d, f)
        elif "SONY" in p:
            return sony_code(d, f)
        elif "SHARP" in p:
            return f
        elif "JVC" in p:
            return nec_code(d, s, f)
        elif "LG" in p:
            return nec_code(d, s, f)
        return 0

    sample = found[0]
    proto_str = sample[0].upper()
    sharp_addr = sample[1] if "SHARP" in proto_str else 0

    return (sample[0], sharp_addr,
            encode(power), encode(vup, power), encode(vdn, power),
            encode(mute, power), encode(cup, cdn), encode(cdn, cup))

BASE = "https://raw.githubusercontent.com/probonopd/irdb/master/codes"
API  = "https://api.github.com/repos/probonopd/irdb/contents/codes"

DTYPE_DISPLAY = {
    "TV":"TV","Soundbar":"Soundbar","Sound_Bar":"Soundbar",
    "Receiver":"AV Rcvr","AV_Receiver":"AV Rcvr",
    "DVD_Player":"DVD","Blu-ray_Player":"Blu-ray","Projector":"Projector",
    "Set_Top_Box":"STB","Media_Streamer":"Media"
}
DTYPE_PRIORITY = ["TV","Soundbar","Sound_Bar","Receiver","AV_Receiver","DVD_Player","Blu-ray_Player","Projector"]

PROTO_MAP = {
    "NEC":"IRDB_NEC","NEC1":"IRDB_NEC","NEC2":"IRDB_NEC","NECX2":"IRDB_NEC",
    "SAMSUNG":"IRDB_SAMSUNG","SAMSUNG32":"IRDB_SAMSUNG",
    "RC5":"IRDB_RC5","RC-5":"IRDB_RC5",
    "SONY12":"IRDB_SONY","SONY15":"IRDB_SONY","SONY20":"IRDB_SONY","SONY":"IRDB_SONY",
    "JVC":"IRDB_JVC","SHARP":"IRDB_SHARP","LG":"IRDB_LG",
}
SUPPORTED = set(PROTO_MAP.keys())

SKIP = {"Samsung","LG","Sony","Philips","Grundig","Panasonic","Sharp","Toshiba",
        "TCL","Vestel","Arcelik","Beko","Hisense","Haier","JVC","Pioneer","Denon",
        "Hitachi","Sanyo","Emerson","Magnavox","Loewe","Mitsubishi","Onkyo","Yamaha",
        "Marantz","Bose"}

print("Fetching brand list...", file=sys.stderr)
brands_text = fetch_url(API)
brands = [d["name"] for d in json.loads(brands_text) if d["type"]=="dir"]
print(f"Total brands: {len(brands)}", file=sys.stderr)

results = []
for brand in brands:
    if brand in SKIP:
        continue
    brand_enc = urllib.request.quote(brand)
    subtypes_text = fetch_url(f"{API}/{brand_enc}")
    if not subtypes_text:
        continue
    try:
        subtypes = [d["name"] for d in json.loads(subtypes_text) if d["type"]=="dir"]
    except:
        continue

    for dtype in DTYPE_PRIORITY:
        if dtype not in subtypes:
            continue
        disp_type = DTYPE_DISPLAY.get(dtype, dtype)
        csvs_text = fetch_url(f"{API}/{brand_enc}/{dtype}")
        if not csvs_text:
            continue
        try:
            csvs = [f["name"] for f in json.loads(csvs_text) if f["name"].endswith(".csv")]
        except:
            continue
        if not csvs:
            continue

        success = False
        for csv_name in csvs[:4]:
            url = f"{BASE}/{brand_enc}/{dtype}/{urllib.request.quote(csv_name)}"
            text = fetch_url(url)
            parsed = parse_csv(text)
            codes = get_codes(parsed)
            if not codes:
                continue
            proto_str, addr, pw, vu, vd, mt, cu, cd = codes
            pu = proto_str.upper().split(",")[0].strip()
            irproto = PROTO_MAP.get(pu)
            if not irproto:
                continue

            # nbits
            if irproto == "IRDB_SONY":
                nbits = 12 if "12" in proto_str else 15 if "15" in proto_str else 12
            elif irproto == "IRDB_RC5":
                nbits = 12
            elif irproto == "IRDB_JVC":
                nbits = 16
            elif irproto == "IRDB_SHARP":
                nbits = 15
            else:
                nbits = 32

            dev_name = csv_name.replace(".csv","").replace(",","/")
            results.append((brand, disp_type, dev_name, irproto, nbits, addr, pw, vu, vd, mt, cu, cd))
            print(f"  OK: {brand}/{disp_type} [{proto_str}] {csv_name}", file=sys.stderr)
            success = True
            break
        if success:
            break  # one dtype per brand

print(f"\nNew entries: {len(results)}", file=sys.stderr)

def fmt(v, nbits):
    if nbits == 32:
        return f"0x{v:08X}"
    elif nbits <= 16:
        return f"0x{v:04X}"
    else:
        return str(v)

print("\n// === AUTO-GENERATED from probonopd/irdb ===")
for r in sorted(results, key=lambda x: (x[1], x[0])):
    brand, dtype, name, proto, nbits, addr, pw, vu, vd, mt, cu, cd = r
    b32 = nbits == 32
    print(f'  {{"{brand:<12}","{dtype:<8}","{name:<14}",{proto:<14},{nbits:2},{addr},'
          f'{fmt(pw,nbits)},{fmt(vu,nbits)},{fmt(vd,nbits)},{fmt(mt,nbits)},{fmt(cu,nbits)},{fmt(cd,nbits)}}},')
