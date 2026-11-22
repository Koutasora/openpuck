// binds_fw.h -- firmware side of binds.h: persistence (binds.bin), g_back[] sync, profile chord, confirm buzz.
//
// binds.bin is its own InternalFS file, so cfg.bin / struct Cfg stay exactly as they were. Without a (valid)
// binds.bin every profile starts as the BUTTON codes currently in g_type[].back. g_type[et].back and g_back[]
// always mirror the BUTTON codes of the ACTIVE profile (0 for combo/macro paddles), which keeps the old
// WebUSB fields 5..8 / 40+ and every mode's g_back[] translation working unchanged.
#pragma once
#include <stdint.h>
#include "binds.h"

// Load binds.bin (or defaults from g_type[].back) and sync g_type[].back / g_back[]. Call after loadCfg().
void bindsFwInit();
// Persist g_binds to binds.bin.
void bindsFwSave();
// After g_binds changed: rewrite g_type[].back + live mirrors from the active profile, persist.
void bindsFwCommit();
// Old panel / console write of a plain paddle code: sets a BUTTON action in the ACTIVE profile.
// A code binds does not accept (> BINDS_CODE_MAX) is refused and the g_type mirror is put back from binds.
void bindsFwSetButton(uint8_t etype, uint8_t paddle, uint8_t code);
// Switch profile (0..BINDS_PROFILES-1): commit + N short buzzes (N = profile number). False if out of range.
bool bindsFwSetProfile(uint8_t profile);

// rf_link.cpp, once per input report of `slot` (raw = decoded 0x45 buttons): remember the raw state, apply the
// active profile, and return the buttons every mode must see. Also holds g_back[] at 0 while all four paddles
// are held (that chord must not leak paddle-mapped buttons to the host).
uint32_t bindsFwFilter(uint8_t slot, uint32_t raw);
// rf_link.cpp, same call site: back4 + RB = next profile, back4 + LB = previous. Debounced, once per hold.
// Any mode with an emulated type (g_etype < ET_COUNT), console modes too. Returns true while LB/RB must be hidden from the host (back4 held).
bool bindsFwProfileChord(uint8_t slot, uint32_t raw);
// Main loop: plays the confirmation buzzes without blocking.
void bindsFwTask();

// Counters since boot for the diagnostics frame (WebUSB 0x2C).
struct BindsDiag {
	uint32_t calls; // bindsFwFilter calls
	uint32_t lastRaw, lastOut; // buttons before / after binds, last call
	uint32_t rawSeen,
		outSeen; // every button bit ever seen before / after binds
	uint32_t nonZero; // calls whose KEY / MOUSE state was not empty
	BindsKbm lastNonZero; // that state, last time
};
extern BindsDiag g_bindsDiag;

// Last raw buttons per slot (before binds), for the live view over WebUSB.
extern uint32_t g_rawButtons[NSLOT];

// WebUSB blob transfer. Read: chunk idx of the serialized blob (BINDS_CHUNK bytes each).
#define BINDS_CHUNK 96
#define BINDS_CHUNKS ((BINDS_BLOB_BYTES + BINDS_CHUNK - 1) / BINDS_CHUNK)
// Copies chunk idx into out, returns its length (0 = idx out of range).
uint8_t bindsFwReadChunk(uint8_t idx, uint8_t *out);
// Write: stage chunk idx (idx 0 restarts staging). False on bad idx/length/order.
bool bindsFwStageChunk(uint8_t idx, const uint8_t *data, uint8_t len);
// Validate + apply + persist the staged blob. False when incomplete or invalid (current binds stay).
bool bindsFwStageCommit();
