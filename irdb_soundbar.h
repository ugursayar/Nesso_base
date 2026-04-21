#pragma once
// ============================================================
// irdb_soundbar.h — Soundbar IR database entries
//
// Sources:
//   IRDB/Soundbars/Sony/Sony_Soundbar_RMT-AH411U.ir  (SIRC15 parsed)
//   IRDB/Soundbars/Sony/SONY_RMT-AH412U.ir            (SIRC15 parsed)
//   IRDB/Soundbars/Sony/Sony_MHC-GS300AV.ir           (SIRC15 parsed)
//   IRDB/Soundbars/Sony/Sony_HW-T400.ir               (raw timing)
//
// SIRC15 encoding: code = (device_address << 7) | command
//   device 0x30: power/vol/mute/enter buttons
//   device 0xB0: input/nav/menu buttons
//   device 0x10: MHC-GS300AV main controls
//
// HW-T400 raw arrays contain one complete button-press transmission
// (all timing values in µs before the first inter-transmission gap).
// Each array is 77 values (mark/space pairs, starting with a mark).
// ============================================================
#include "irdb_types.h"

// ── Bose ──────────────────────────────────────────────────────────────
static const IRDev _ir_BOSE_SB = {
  "Bose", "Soundbar", "Wave/Solo",
  IRDB_NEC, 32, 0,
  0x5DD232CD, 0x5DD2C03F, 0x5DD240BF, 0x5DD2807F, 0x5DD29867, 0x5DD218E7,
  0,0,0,0,0,0,0,0
};

// ── Sony RMT-AH411U (SIRC15, HT-series soundbars) ────────────────────
// SIRC20 buttons (EQ modes, play/pause, track) are not representable
// in the single-protocol IRDev struct and are omitted.
static const IRDev _ir_SONY_SB_AH411U = {
  "Sony", "Soundbar", "RMT-AH411U",
  IRDB_SONY, 15, 0,
  // power  volUp  volDown  mute   chUp  chDown
  0x1815, 0x1812, 0x1813, 0x1814,   0,     0,
  // input   menu    ok    navUp  navDown  navLeft  navRight  back
  0x5869,    0,  0x180C, 0x5878, 0x5879,    0,       0,       0
};

// ── Sony RMT-AH412U (SIRC15, 5.1ch soundbar, adds Menu) ──────────────
// Back button uses SIRC20 — omitted.
static const IRDev _ir_SONY_SB_AH412U = {
  "Sony", "Soundbar", "RMT-AH412U",
  IRDB_SONY, 15, 0,
  // power  volUp  volDown  mute   chUp  chDown
  0x1815, 0x1812, 0x1813, 0x1814,   0,     0,
  // input   menu    ok    navUp  navDown  navLeft  navRight  back
  0x5869, 0x5877, 0x180C, 0x5878, 0x5879,   0,       0,       0
};

// ── Sony MHC-GS300AV (SIRC15, mini hi-fi, device=0x10) ───────────────
// No mute; input mapped to CD. Tuner preset/play use SIRC20 — omitted.
static const IRDev _ir_SONY_SB_GS300AV = {
  "Sony", "Soundbar", "MHC-GS300AV",
  IRDB_SONY, 15, 0,
  // power  volUp  volDown  mute  chUp  chDown
  0x0815, 0x0812, 0x0813,   0,    0,     0,
  // input   menu   ok  navUp  navDown  navLeft  navRight  back
  0x0825,    0,     0,    0,     0,       0,       0,       0
};

// ── Sony HW-T400 — raw timing arrays ─────────────────────────────────
// All values in µs. Each array = complete single button-press
// transmission (two frames, 77 values, before the first ~55 ms gap).

static const uint16_t _hwt400_power[] = {
  4549,4469,542,468,544,467,545,465,537,473,519,1481,513,1485,540,471,
  541,469,512,1487,517,1482,512,1487,517,1482,543,467,545,465,547,463,
  539,472,519,4489,543,468,544,466,546,464,538,473,539,470,542,468,544,
  466,546,465,516,1482,512,1487,517,1482,543,468,513,1485,519,1480,514,
  1485,519,1480,545,465,547,463,539,472,520,1479,546
};

static const uint16_t _hwt400_mute[] = {
  4555,4465,546,464,538,472,540,470,542,468,513,1486,518,1481,544,467,
  545,465,516,1483,511,1488,516,1483,511,1488,537,473,539,472,540,470,
  542,468,513,4496,546,464,538,473,539,471,541,469,512,1487,538,472,540,
  470,542,469,512,1486,518,1481,544,1455,570,441,571,439,542,1457,547,
  1452,542,1457,568,442,570,440,572,438,543,1456,569
};

static const uint16_t _hwt400_vol_up[] = {
  4552,4468,574,436,566,444,568,442,570,441,540,1458,546,1453,572,439,
  573,437,544,1455,539,1460,544,1456,548,1450,575,436,566,444,568,442,
  570,440,541,4468,594,416,565,445,567,443,569,441,540,1459,545,1453,
  541,1459,566,444,547,1452,542,1457,547,1452,593,417,564,446,566,444,
  568,442,539,1460,565,446,566,444,568,442,539,1460,565
};

static const uint16_t _hwt400_vol_dn[] = {
  4546,4473,538,473,539,470,542,469,543,467,514,1484,520,1479,545,465,
  547,463,518,1481,513,1486,518,1481,513,1487,538,472,540,470,542,469,
  543,466,515,4495,546,463,539,472,540,470,542,468,544,466,546,491,521,
  462,519,1480,513,1485,519,1480,514,1486,539,472,519,1479,515,1484,520,
  1479,546,464,548,463,539,471,541,469,512,1487,548
};

static const uint16_t _hwt400_play_pause[] = {
  4548,4471,540,471,541,468,544,467,545,465,516,1482,512,1488,537,473,
  539,472,519,1479,514,1484,520,1479,515,1485,539,470,542,468,544,466,
  546,464,517,4493,538,472,540,470,542,468,544,466,546,464,517,1482,543,
  468,513,544,471,469,574,436,514,1485,519,1480,545,465,547,1452,573,
  438,543,1456,548,1451,543,1456,569,442,570
};

static const uint16_t _hwt400_track_fwd[] = {
  4551,4468,542,468,544,466,546,465,537,473,518,1480,513,1486,538,472,
  540,470,511,1488,516,1483,511,1488,516,1484,541,469,543,467,545,466,
  546,464,517,4492,539,471,541,469,543,468,544,466,546,464,538,473,549,
  1449,513,1486,538,471,541,470,542,468,513,1485,519,1481,513,1486,538,
  472,540,470,511,1488,516,1483,510,1488,547,464,538
};

static const uint16_t _hwt400_track_back[] = {
  4549,4471,540,470,542,495,517,493,519,491,490,1482,512,1487,548,463,
  539,471,520,1479,566,1432,520,1479,515,1484,541,470,542,468,544,492,
  520,491,490,4492,539,497,515,495,517,494,518,492,489,1483,521,1478,
  547,463,518,1481,544,467,545,491,521,463,518,1481,544,466,546,465,516,
  1482,543,468,513,1486,518,1481,513,1486,538,472,540
};

static const uint16_t _hwt400_bluetooth[] = {
  4547,4472,539,471,541,495,517,494,518,492,489,1484,520,1479,546,463,
  549,488,493,1480,514,1485,519,1480,514,1486,539,471,541,495,517,493,
  519,491,490,4494,547,488,514,497,515,495,517,493,519,491,490,1483,521,
  1478,547,463,518,1481,544,466,546,491,490,1483,521,1478,547,489,513,
  497,494,1479,546,490,491,1482,522,1477,548,489,513
};

#define _HWT400_LEN(arr) ((uint16_t)(sizeof(arr)/sizeof((arr)[0])))

static const IRDevRaw _ir_SONY_SB_HWT400 = {
  "Sony", "Soundbar", "HW-T400",
  // power
  { _hwt400_power,      _HWT400_LEN(_hwt400_power),      38000 },
  // volUp
  { _hwt400_vol_up,     _HWT400_LEN(_hwt400_vol_up),     38000 },
  // volDown
  { _hwt400_vol_dn,     _HWT400_LEN(_hwt400_vol_dn),     38000 },
  // mute
  { _hwt400_mute,       _HWT400_LEN(_hwt400_mute),       38000 },
  // chUp
  IR_RAW_NONE,
  // chDown
  IR_RAW_NONE,
  // input (Bluetooth source)
  { _hwt400_bluetooth,  _HWT400_LEN(_hwt400_bluetooth),  38000 },
  // menu
  IR_RAW_NONE,
  // ok (Play/Pause)
  { _hwt400_play_pause, _HWT400_LEN(_hwt400_play_pause), 38000 },
  // navUp
  IR_RAW_NONE,
  // navDown
  IR_RAW_NONE,
  // navLeft (Track Back)
  { _hwt400_track_back, _HWT400_LEN(_hwt400_track_back), 38000 },
  // navRight (Track Forward)
  { _hwt400_track_fwd,  _HWT400_LEN(_hwt400_track_fwd),  38000 },
  // back
  IR_RAW_NONE
};

#define IRDB_SOUNDBAR_ENTRIES         \
  IR_ENTRY(_ir_BOSE_SB),             \
  IR_ENTRY(_ir_SONY_SB_AH411U),      \
  IR_ENTRY(_ir_SONY_SB_AH412U),      \
  IR_ENTRY(_ir_SONY_SB_GS300AV),     \
  IR_ENTRY_RAW(_ir_SONY_SB_HWT400)
