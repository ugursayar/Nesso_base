#pragma once
// ============================================================
// irdb_av_rcvr.h — AV Receiver IR database entries
// ============================================================
#include "irdb_types.h"

static const IRDev _ir_ADCOM_AVR       = {"Adcom",     "AV Rcvr","26/-1",          IRDB_NEC, 32,0, 0x58A701FE,0x58A7AB54,0x58A78B74,0x58A741BE,0,0x58A7E31C,           0,0,0,0,0,0,0,0};
// Aiwa: Sony SIRC-12, device=16 (address 0x10)
static const IRDev _ir_AIWA_AVR        = {"Aiwa",      "AV Rcvr","16/-1",          IRDB_SONY,12,0, 0x0815,0x0812,0x0813,0x0814,0,0,                                   0,0,0,0,0,0,0,0};
static const IRDev _ir_ARCAM_AVR       = {"Arcam",     "AV Rcvr","16/-1",          IRDB_RC5, 12,0, 0x040C,0x0410,0x0411,0x0477,0x0438,0,                              0,0,0,0,0,0,0,0};
static const IRDev _ir_BNK_AVR         = {"BnK Comp",  "AV Rcvr","27/78",          IRDB_NEC, 32,0, 0xD872807F,0xD87224DB,0xD872C43B,0xD872C03F,0xD87218E7,0xD872E817,  0,0,0,0,0,0,0,0};
static const IRDev _ir_CAMBRIDGE_AVR   = {"Cambridge", "AV Rcvr","192/192",        IRDB_NEC, 32,0, 0x030330CF,0x0303C03F,0x0303F807,0,0,0x0303B04F,                    0,0,0,0,0,0,0,0};
static const IRDev _ir_CARVER_AVR      = {"Carver",    "AV Rcvr","135/123",        IRDB_NEC, 32,0, 0xE1DE02FD,0xE1DE42BD,0xE1DEC23D,0xE1DEDA25,0,0,                   0,0,0,0,0,0,0,0};
static const IRDev _ir_CARY_AVR        = {"Cary Audio","AV Rcvr","19/-1",          IRDB_RC5, 12,0, 0x04CC,0x04D0,0x04D1,0x04F9,0,0,                                   0,0,0,0,0,0,0,0};
static const IRDev _ir_DENON_AVR       = {"Denon",     "AV Rcvr","AVR series",     IRDB_NEC, 32,0, 0x4BB640BF,0x4BB658A7,0x4BB6D827,0x4BB6A05F,0x4BB6E21D,0x4BB6629D,  0,0,0,0,0,0,0,0};
static const IRDev _ir_HARMAN_AVR      = {"Harman Kar","AV Rcvr","128/112",        IRDB_NEC, 32,0, 0x010E03FC,0x010EE31C,0x010E13EC,0x010E837C,0x010EBD42,0x010E7D82,  0,0,0,0,0,0,0,0};
static const IRDev _ir_INTEGRA_AVR     = {"Integra",   "AV Rcvr","210/109",        IRDB_NEC, 32,0, 0x4BB620DF,0x4BB640BF,0x4BB6C03F,0x4BB633CC,0x4BB600FF,0x4BB6807F,  0,0,0,0,0,0,0,0};
static const IRDev _ir_KENWOOD_AVR     = {"Kenwood",   "AV Rcvr","184/-1",         IRDB_NEC, 32,0, 0x1DE2B946,0x1DE2D926,0x1DE259A6,0x1DE239C6,0x1DE29966,0x1DE231CE,  0,0,0,0,0,0,0,0};
static const IRDev _ir_KINERGETIC_AVR  = {"Kinergetic","AV Rcvr","0/-1",           IRDB_RC5, 12,0, 0,0x000C,0x000B,0x0003,0x0007,0x0008,                              0,0,0,0,0,0,0,0};
static const IRDev _ir_LEXICON_AVR     = {"Lexicon",   "AV Rcvr","130/11",         IRDB_NEC, 32,0, 0x41D059A6,0x41D0E817,0x41D06897,0x41D0A857,0,0,                    0,0,0,0,0,0,0,0};
static const IRDev _ir_MARANTZ_AVR     = {"Marantz",   "AV Rcvr","SR/PM series",   IRDB_RC5, 12,0, 0x040C,0x0410,0x0411,0x040D,0x0420,0x0421,                         0,0,0,0,0,0,0,0};
static const IRDev _ir_MYRYAD_AVR      = {"Myryad",    "AV Rcvr","16/-1",          IRDB_RC5, 12,0, 0x040C,0x0410,0x0411,0x040D,0,0,                                   0,0,0,0,0,0,0,0};
static const IRDev _ir_NAD_AVR         = {"NAD",       "AV Rcvr","135/124",        IRDB_NEC, 32,0, 0xE13E01FE,0xE13E11EE,0xE13E31CE,0xE13E29D6,0,0,                   0,0,0,0,0,0,0,0};
static const IRDev _ir_NAKAMICHI_AVR   = {"Nakamichi", "AV Rcvr","130/93",         IRDB_NEC, 32,0, 0x41BA20DF,0x41BAA05F,0x41BA08F7,0x41BA30CF,0x41BA3AC5,0x41BABA45,  0,0,0,0,0,0,0,0};
static const IRDev _ir_ONKYO_AVR       = {"Onkyo",     "AV Rcvr","TX-NR series",   IRDB_NEC, 32,0, 0x4BB620DF,0x4BB640BF,0x4BB6C03F,0x4BB6A05F,0x4BB600FF,0x4BB6807F,  0,0,0,0,0,0,0,0};
static const IRDev _ir_ONKYO_INTG_AVR  = {"Onkyo Intg","AV Rcvr","210/109",        IRDB_NEC, 32,0, 0x4BB620DF,0x4BB640BF,0x4BB6C03F,0x4BB6A05F,0x4BB600FF,0x4BB6807F,  0,0,0,0,0,0,0,0};
static const IRDev _ir_PARASOUND_AVR   = {"Parasound", "AV Rcvr","3/240",          IRDB_NEC, 32,0, 0xC00F51AE,0xC00F43BC,0xC00FC33C,0xC00F936C,0xC00FD32C,0,           0,0,0,0,0,0,0,0};
static const IRDev _ir_PIONEER_AVR     = {"Pioneer",   "AV Rcvr","VSX series",     IRDB_NEC, 32,0, 0xA55AE21D,0xA55A18E7,0xA55A9867,0xA55A48B7,0xA55A58A7,0xA55AD827,  0,0,0,0,0,0,0,0};
static const IRDev _ir_YAMAHA_AVR      = {"Yamaha",    "AV Rcvr","RX-V/RX-A",     IRDB_NEC, 32,0, 0x1EE1F00F,0x1EE17887,0x1EE1F807,0x1EE139C6,0x1EE1D827,0x1EE138C7,  0,0,0,0,0,0,0,0};

#define IRDB_AVRCVR_ENTRIES \
  IR_ENTRY(_ir_ADCOM_AVR),      \
  IR_ENTRY(_ir_AIWA_AVR),       \
  IR_ENTRY(_ir_ARCAM_AVR),      \
  IR_ENTRY(_ir_BNK_AVR),        \
  IR_ENTRY(_ir_CAMBRIDGE_AVR),  \
  IR_ENTRY(_ir_CARVER_AVR),     \
  IR_ENTRY(_ir_CARY_AVR),       \
  IR_ENTRY(_ir_DENON_AVR),      \
  IR_ENTRY(_ir_HARMAN_AVR),     \
  IR_ENTRY(_ir_INTEGRA_AVR),    \
  IR_ENTRY(_ir_KENWOOD_AVR),    \
  IR_ENTRY(_ir_KINERGETIC_AVR), \
  IR_ENTRY(_ir_LEXICON_AVR),    \
  IR_ENTRY(_ir_MARANTZ_AVR),    \
  IR_ENTRY(_ir_MYRYAD_AVR),     \
  IR_ENTRY(_ir_NAD_AVR),        \
  IR_ENTRY(_ir_NAKAMICHI_AVR),  \
  IR_ENTRY(_ir_ONKYO_AVR),      \
  IR_ENTRY(_ir_ONKYO_INTG_AVR), \
  IR_ENTRY(_ir_PARASOUND_AVR),  \
  IR_ENTRY(_ir_PIONEER_AVR),    \
  IR_ENTRY(_ir_YAMAHA_AVR)
