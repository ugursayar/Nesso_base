#pragma once
// ============================================================
// irdb.h — Combined IR device database
//
// Includes all category sub-files and builds the unified
// IRDB[] array of IREntry elements.
//
// Sub-files:
//   irdb_types.h      — shared type definitions and helpers
//   irdb_tv.h         — TV entries        (parsed: NEC/Sony/RC5/Sharp/JVC/Samsung)
//   irdb_av_rcvr.h    — AV Receiver entries (parsed: NEC/RC5/Sony)
//   irdb_soundbar.h   — Soundbar entries  (parsed: NEC/Sony  + raw: HW-T400)
//   irdb_projector.h  — Projector entries (parsed: NEC)
// ============================================================

#include "irdb_types.h"
#include "irdb_tv.h"
#include "irdb_av_rcvr.h"
#include "irdb_soundbar.h"
#include "irdb_projector.h"

static const IREntry IRDB[] = {
  // ── TVs ────────────────────────────────────────────────────────────
  IRDB_TV_ENTRIES,
  // ── AV Receivers ───────────────────────────────────────────────────
  IRDB_AVRCVR_ENTRIES,
  // ── Soundbars ──────────────────────────────────────────────────────
  IRDB_SOUNDBAR_ENTRIES,
  // ── Projectors ─────────────────────────────────────────────────────
  IRDB_PROJECTOR_ENTRIES,
};

static const int IRDB_COUNT = (int)(sizeof(IRDB) / sizeof(IRDB[0]));
