#pragma once
// ============================================================
// irdb_types.h — IR database shared types and helper functions
// ============================================================

// ── Parsed-protocol enum ─────────────────────────────────────────────
enum IRDBProto : uint8_t {
  IRDB_SAMSUNG = 0,
  IRDB_NEC,
  IRDB_SONY,
  IRDB_RC5,
  IRDB_LG,
  IRDB_JVC,
  IRDB_SHARP,
  IRDB_RC6,
  IRDB_RAW     // raw timing array — see IRDevRaw
};

// ── Protocol display names (index matches IRDBProto) ─────────────────
static const char* const IRDB_PROTO_NAMES[] = {
  "SMSG", "NEC", "SONY", "RC5", "LG", "JVC", "SHRP", "RC6", "RAW"
};

// ── Button identifiers (protocol-neutral) ────────────────────────────
enum IRButton : uint8_t {
  IR_BTN_POWER  = 0,
  IR_BTN_VOL_UP = 1,
  IR_BTN_VOL_DN = 2,
  IR_BTN_MUTE   = 3,
  IR_BTN_CH_UP  = 4,
  IR_BTN_CH_DN  = 5,
  IR_BTN_INPUT  = 6,
  IR_BTN_MENU   = 7,
  IR_BTN_OK     = 8,
  IR_BTN_NAV_UP = 9,
  IR_BTN_NAV_DN = 10,
  IR_BTN_NAV_L  = 11,
  IR_BTN_NAV_R  = 12,
  IR_BTN_BACK   = 13,
  IR_BTN_COUNT  = 14,
  IR_BTN_NONE   = 0xFF
};

// ── Parsed-protocol device entry ─────────────────────────────────────
// Code encoding:
//   NEC 32-bit : bitrev8(dev)<<24 | bitrev8(sub)<<16 | bitrev8(cmd)<<8 | bitrev8(~cmd)
//   Sony SIRC  : (device<<7)|command  (nbits = 12 / 15 / 20)
//   RC5 12-bit : (device<<6)|command
//   Sharp      : addr + command passed to sendSharp(addr, cmd, nbits)
//   0          = button not available on this remote
struct IRDev {
  const char*  brand;
  const char*  type;   // "TV", "Soundbar", "AV Rcvr", "Projector"
  const char*  name;
  IRDBProto    proto;
  uint8_t      nbits;
  uint16_t     addr;   // Sharp: IR address; others: 0
  uint32_t     power, volUp, volDown, mute, chUp, chDown;
  uint32_t     input, menu, ok;
  uint32_t     navUp, navDown, navLeft, navRight;
  uint32_t     back;
};

// ── Raw timing signal ─────────────────────────────────────────────────
// data[] = alternating mark/space timings in microseconds, starting with
//          a mark pulse.  len = number of values.  freq = carrier Hz.
// A null data pointer means this button is not available.
struct IRRawSignal {
  const uint16_t* data;
  uint16_t        len;
  uint16_t        freq;  // carrier Hz, typically 38000
};

#define IR_RAW_NONE { nullptr, 0, 0 }

// ── Raw-protocol device entry ─────────────────────────────────────────
struct IRDevRaw {
  const char*   brand;
  const char*   type;
  const char*   name;
  IRRawSignal   power, volUp, volDown, mute, chUp, chDown;
  IRRawSignal   input, menu, ok;
  IRRawSignal   navUp, navDown, navLeft, navRight;
  IRRawSignal   back;
};

// ── Unified IRDB entry ────────────────────────────────────────────────
struct IREntry {
  bool            isRaw;
  const IRDev*    dev;   // non-null when !isRaw
  const IRDevRaw* raw;   // non-null when  isRaw
};

// Convenience initializer macros
#define IR_ENTRY(d)     { false, &(d), nullptr }
#define IR_ENTRY_RAW(r) { true,  nullptr, &(r) }

// ── Accessors ─────────────────────────────────────────────────────────

inline const char* irBrand(const IREntry& e) {
  return e.isRaw ? e.raw->brand : e.dev->brand;
}
inline const char* irType(const IREntry& e) {
  return e.isRaw ? e.raw->type : e.dev->type;
}
inline const char* irName(const IREntry& e) {
  return e.isRaw ? e.raw->name : e.dev->name;
}
inline IRDBProto irProto(const IREntry& e) {
  return e.isRaw ? IRDB_RAW : e.dev->proto;
}

// Returns the parsed code for btn (0 if unavailable or raw device).
inline uint32_t irGetCode(const IREntry& e, IRButton btn) {
  if (e.isRaw) return 0;
  const IRDev& d = *e.dev;
  switch (btn) {
    case IR_BTN_POWER:  return d.power;
    case IR_BTN_VOL_UP: return d.volUp;
    case IR_BTN_VOL_DN: return d.volDown;
    case IR_BTN_MUTE:   return d.mute;
    case IR_BTN_CH_UP:  return d.chUp;
    case IR_BTN_CH_DN:  return d.chDown;
    case IR_BTN_INPUT:  return d.input;
    case IR_BTN_MENU:   return d.menu;
    case IR_BTN_OK:     return d.ok;
    case IR_BTN_NAV_UP: return d.navUp;
    case IR_BTN_NAV_DN: return d.navDown;
    case IR_BTN_NAV_L:  return d.navLeft;
    case IR_BTN_NAV_R:  return d.navRight;
    case IR_BTN_BACK:   return d.back;
    default:            return 0;
  }
}

// Returns pointer to the raw signal for btn (nullptr if unavailable or parsed).
inline const IRRawSignal* irGetRaw(const IREntry& e, IRButton btn) {
  if (!e.isRaw) return nullptr;
  const IRDevRaw& r = *e.raw;
  switch (btn) {
    case IR_BTN_POWER:  return &r.power;
    case IR_BTN_VOL_UP: return &r.volUp;
    case IR_BTN_VOL_DN: return &r.volDown;
    case IR_BTN_MUTE:   return &r.mute;
    case IR_BTN_CH_UP:  return &r.chUp;
    case IR_BTN_CH_DN:  return &r.chDown;
    case IR_BTN_INPUT:  return &r.input;
    case IR_BTN_MENU:   return &r.menu;
    case IR_BTN_OK:     return &r.ok;
    case IR_BTN_NAV_UP: return &r.navUp;
    case IR_BTN_NAV_DN: return &r.navDown;
    case IR_BTN_NAV_L:  return &r.navLeft;
    case IR_BTN_NAV_R:  return &r.navRight;
    case IR_BTN_BACK:   return &r.back;
    default:            return nullptr;
  }
}

// Returns true if the button has a signal for this entry.
inline bool irButtonAvailable(const IREntry& e, IRButton btn) {
  if (e.isRaw) {
    const IRRawSignal* sig = irGetRaw(e, btn);
    return sig && sig->data && sig->len > 0;
  }
  return irGetCode(e, btn) != 0;
}

inline bool irHasNav(const IREntry& e) {
  return irButtonAvailable(e, IR_BTN_NAV_UP);
}
inline bool irHasExt(const IREntry& e) {
  return irButtonAvailable(e, IR_BTN_INPUT) ||
         irButtonAvailable(e, IR_BTN_MENU)  ||
         irButtonAvailable(e, IR_BTN_OK)    ||
         irButtonAvailable(e, IR_BTN_BACK)  ||
         irHasNav(e);
}
