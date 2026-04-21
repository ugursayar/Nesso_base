#pragma once
// ============================================================
// irdb_tv.h — TV IR database entries
// ============================================================
#include "irdb_types.h"

static const IRDev _ir_ADLER_TV        = {"ADLER",    "TV","2/-1",             IRDB_NEC,    32,0, 0x40BF50AF,0x40BF8A75,0x40BF58A7,0x40BFE01F,0,0,            0,0,0,0,0,0,0,0};
static const IRDev _ir_ARCELIK_TV      = {"Arcelik",  "TV","standard",         IRDB_NEC,    32,0, 0x9B6748B7,0x9B6740BF,0x9B67C03F,0x9B670CF3,0x9B6700FF,0x9B67807F, 0,0,0,0,0,0,0,0};
static const IRDev _ir_BEKO_TV         = {"Beko",     "TV","standard",         IRDB_NEC,    32,0, 0x9B6748B7,0x9B6740BF,0x9B67C03F,0x9B670CF3,0x9B6700FF,0x9B67807F, 0,0,0,0,0,0,0,0};
static const IRDev _ir_COBY_TV         = {"Coby",     "TV","0/127",            IRDB_NEC,    32,0, 0x00FE50AF,0x00FE7887,0x00FEFA05,0x00FE32CD,0x00FEF807,0x00FE3AC5, 0,0,0,0,0,0,0,0};
static const IRDev _ir_EMERSON_TV      = {"Emerson",  "TV","standard",         IRDB_NEC,    32,0, 0x00FFF807,0x00FFC837,0x00FFF00F,0x00FFE817,0x00FFD02F,0x00FFE01F, 0,0,0,0,0,0,0,0};
static const IRDev _ir_FAST_TV         = {"Fast",     "TV","28/-1",            IRDB_RC5,    12,0, 0x070C,0x0742,0x0741,0x070D,0,0x075F,                           0,0,0,0,0,0,0,0};
static const IRDev _ir_FISHER_TV       = {"Fisher",   "TV","56/-1",            IRDB_NEC,    32,0, 0x1CE348B7,0x1CE3708F,0x1CE3F00F,0x1CE318E7,0x1CE350AF,0x1CE3D02F, 0,0,0,0,0,0,0,0};
static const IRDev _ir_GRUNDIG_TV      = {"Grundig",  "TV","RC5",              IRDB_RC5,    12,0, 0x000C,0x0010,0x0011,0x000D,0x0020,0x0021,                     0x0038,0x002E,0,0x001C,0x001D,0x002C,0x002B,0x000F};
static const IRDev _ir_HAIER_TV        = {"Haier",    "TV","standard",         IRDB_NEC,    32,0, 0x60DF0CF3,0x60DF40BF,0x60DFC03F,0x60DF48B7,0x60DF00FF,0x60DF807F, 0,0,0,0,0,0,0,0};
static const IRDev _ir_HISENSE_TV      = {"Hisense",  "TV","standard",         IRDB_NEC,    32,0, 0x00FF38C7,0x00FF40BF,0x00FFC03F,0x00FF906F,0x00FF00FF,0x00FF807F, 0,0,0,0,0,0,0,0};
static const IRDev _ir_HITACHI_TV      = {"Hitachi",  "TV","standard",         IRDB_NEC,    32,0, 0x0AF5E817,0x0AF548B7,0x0AF5A857,0x0AF5D02F,0x0AF59867,0x0AF518E7, 0,0,0,0,0,0,0,0};
static const IRDev _ir_INSIGNIA_TV     = {"Insignia", "TV","134/5",            IRDB_NEC,    32,0, 0x61A0F00F,0,0x61A0B04F,0x61A0708F,0x61A050AF,0x61A0D02F,         0,0,0,0,0,0,0,0};
static const IRDev _ir_JVC_TV          = {"JVC",      "TV","standard",         IRDB_JVC,    16,0, 0xC5E8,0xC508,0xC588,0xC518,0xC538,0xC5B8,                     0,0,0,0,0,0,0,0};
static const IRDev _ir_LG_TV           = {"LG",       "TV","OLED/NanoCell",    IRDB_NEC,    32,0, 0x20DF10EF,0x20DF40BF,0x20DFC03F,0x20DF906F,0x20DF00FF,0x20DF807F, 0x20DF19E6,0x20DFC23D,0x20DF22DD,0x20DF02FD,0x20DF827D,0x20DFE01F,0x20DF609F,0x20DFDA25};
static const IRDev _ir_LOEWE_TV        = {"Loewe",    "TV","RC5",              IRDB_RC5,    12,0, 0x000C,0x0010,0x0011,0x000D,0x0020,0x0021,                     0,0,0,0,0,0,0,0};
static const IRDev _ir_LXI_TV          = {"LXI",      "TV","4/-1",             IRDB_NEC,    32,0, 0x20DF10EF,0x20DF40BF,0x20DFC03F,0x20DF906F,0x20DF00FF,0x20DF807F, 0,0,0,0,0,0,0,0};
static const IRDev _ir_MAGNAVOX_TV     = {"Magnavox", "TV","RC5",              IRDB_RC5,    12,0, 0x000C,0x0010,0x0011,0x000D,0x0020,0x0021,                     0,0,0,0,0,0,0,0};
static const IRDev _ir_MEMOREX_TV      = {"Memorex",  "TV","4/-1",             IRDB_NEC,    32,0, 0x20DF10EF,0x20DF40BF,0x20DFC03F,0x20DF906F,0x20DF00FF,0x20DF807F, 0,0,0,0,0,0,0,0};
static const IRDev _ir_MITSUBISHI_TV   = {"Mitsubishi","TV","Sharp IR",        IRDB_SHARP,  15,1, 22,20,21,23,17,18,                                             0,0,0,0,0,0,0,0};
static const IRDev _ir_PANASONIC_TV    = {"Panasonic","TV","TX series",        IRDB_NEC,    32,0, 0x40040100,0x40040200,0x40041200,0x40040900,0x40040800,0x40041800, 0,0,0,0,0,0,0,0};
static const IRDev _ir_PHILIPS_TV      = {"Philips",  "TV","RC5",              IRDB_RC5,    12,0, 0x000C,0x0010,0x0011,0x000D,0x0020,0x0021,                     0x0038,0x002E,0,0x001C,0x001D,0x002C,0x002B,0x000F};
static const IRDev _ir_PROTON_TV       = {"Proton",   "TV","4/-1",             IRDB_NEC,    32,0, 0x20DF10EF,0x20DF40BF,0x20DFC03F,0x20DF906F,0x20DF00FF,0x20DF807F, 0,0,0,0,0,0,0,0};
static const IRDev _ir_SAMSUNG_TV      = {"Samsung",  "TV","Smart/QLED",       IRDB_SAMSUNG,32,0, 0xE0E040BF,0xE0E0E01F,0xE0E0D02F,0xE0E0F00F,0xE0E048B7,0xE0E008F7, 0xE0E0807F,0xE0E058A7,0xE0E016E9,0xE0E006F9,0xE0E08679,0xE0E0A659,0xE0E046B9,0xE0E01AE5};
static const IRDev _ir_SANYO_TV        = {"Sanyo",    "TV","standard",         IRDB_NEC,    32,0, 0x1CE348B7,0x1CE3708F,0x1CE3F00F,0x1CE318E7,0x1CE350AF,0x1CE3D02F, 0,0,0,0,0,0,0,0};
static const IRDev _ir_SHARP_TV        = {"Sharp",    "TV","Aquos",            IRDB_SHARP,  15,1, 0x45,0x07,0x0B,0x6B,0xC5,0xC1,                                0,0,0,0,0,0,0,0};
// Sony SIRC-12 (device=1)
static const IRDev _ir_SONY_TV_SIRC12  = {"Sony",     "TV","SIRC-12",          IRDB_SONY,   12,0, 0x95,0x92,0x93,0x94,0x90,0x91,                                0x00A4,0x00E0,0x008B,0x00F4,0x00F5,0x00B4,0x00B3,0x00E3};
// Sony Bravia SIRC-15 (same codes, 15-bit frame)
static const IRDev _ir_SONY_TV_BRAVIA  = {"Sony",     "TV","Bravia/SIRC-15",   IRDB_SONY,   15,0, 0x95,0x92,0x93,0x94,0x90,0x91,                                0x00A4,0x00E0,0x008B,0x00F4,0x00F5,0x00B4,0x00B3,0x00E3};
static const IRDev _ir_TCL_TV          = {"TCL",      "TV","P/C series",       IRDB_NEC,    32,0, 0xE31EB14E,0xE31EA15E,0xE31E619E,0xE31EE11E,0xE31EC13E,0xE31E41BE, 0,0,0,0,0,0,0,0};
static const IRDev _ir_TOSHIBA_TV      = {"Toshiba",  "TV","standard",         IRDB_NEC,    32,0, 0x02FD48B7,0x02FD58A7,0x02FD7887,0x02FD08F7,0x02FDD827,0x02FDF807, 0x02FDF00F,0x02FD01FE,0x02FDE817,0,0,0,0,0x02FD1AE5};
static const IRDev _ir_VESTEL_TV       = {"Vestel",   "TV","NEC variant",      IRDB_NEC,    32,0, 0x56AB0CF3,0x56AB40BF,0x56ABC03F,0x56AB48B7,0x56AB00FF,0x56AB807F, 0,0,0,0,0,0,0,0};
static const IRDev _ir_VIVAX_TV        = {"Vivax",    "TV","2/-1",             IRDB_NEC,    32,0, 0x40BF50AF,0x40BF8A75,0x40BF58A7,0x40BFE01F,0,0,                0,0,0,0,0,0,0,0};

#define IRDB_TV_ENTRIES \
  IR_ENTRY(_ir_ADLER_TV),       \
  IR_ENTRY(_ir_ARCELIK_TV),     \
  IR_ENTRY(_ir_BEKO_TV),        \
  IR_ENTRY(_ir_COBY_TV),        \
  IR_ENTRY(_ir_EMERSON_TV),     \
  IR_ENTRY(_ir_FAST_TV),        \
  IR_ENTRY(_ir_FISHER_TV),      \
  IR_ENTRY(_ir_GRUNDIG_TV),     \
  IR_ENTRY(_ir_HAIER_TV),       \
  IR_ENTRY(_ir_HISENSE_TV),     \
  IR_ENTRY(_ir_HITACHI_TV),     \
  IR_ENTRY(_ir_INSIGNIA_TV),    \
  IR_ENTRY(_ir_JVC_TV),         \
  IR_ENTRY(_ir_LG_TV),          \
  IR_ENTRY(_ir_LOEWE_TV),       \
  IR_ENTRY(_ir_LXI_TV),         \
  IR_ENTRY(_ir_MAGNAVOX_TV),    \
  IR_ENTRY(_ir_MEMOREX_TV),     \
  IR_ENTRY(_ir_MITSUBISHI_TV),  \
  IR_ENTRY(_ir_PANASONIC_TV),   \
  IR_ENTRY(_ir_PHILIPS_TV),     \
  IR_ENTRY(_ir_PROTON_TV),      \
  IR_ENTRY(_ir_SAMSUNG_TV),     \
  IR_ENTRY(_ir_SANYO_TV),       \
  IR_ENTRY(_ir_SHARP_TV),       \
  IR_ENTRY(_ir_SONY_TV_SIRC12), \
  IR_ENTRY(_ir_SONY_TV_BRAVIA), \
  IR_ENTRY(_ir_TCL_TV),         \
  IR_ENTRY(_ir_TOSHIBA_TV),     \
  IR_ENTRY(_ir_VESTEL_TV),      \
  IR_ENTRY(_ir_VIVAX_TV)
