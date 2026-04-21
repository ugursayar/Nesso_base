#pragma once
// ============================================================
// irdb_projector.h — Projector IR database entries
// ============================================================
#include "irdb_types.h"

static const IRDev _ir_DIGIPROJ_PROJ = {
  "DigiProj", "Projector", "32/-1",
  IRDB_NEC, 32, 0,
  0x04FB00FF, 0x04FB609F, 0x04FB50AF, 0x04FB708F, 0, 0,
  0,0,0,0,0,0,0,0
};

static const IRDev _ir_EPSON_PROJ = {
  "Epson", "Projector", "131/85",
  IRDB_NEC, 32, 0,
  0xC1AA09F6, 0xC1AA19E6, 0xC1AA9966, 0xC1AAC936, 0, 0,
  0,0,0,0,0,0,0,0
};

#define IRDB_PROJECTOR_ENTRIES  \
  IR_ENTRY(_ir_DIGIPROJ_PROJ),  \
  IR_ENTRY(_ir_EPSON_PROJ)
