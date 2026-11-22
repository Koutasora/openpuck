// binds.h -- back-paddle (L4/R4/L5/R5) actions: single button, button combo, or macro; 3 switchable profiles.
//
// Pure logic: no Arduino, no hardware, time comes in as an argument, so tests/binds builds it with a host g++.
// bindsApply() runs ONCE per decoded input report in rf_link.cpp, before any emulation mode sees the buttons:
//   BUTTON: the paddle bit is left alone; the modes translate it through g_back[] as before (g_back[] mirrors
//           the BUTTON codes of the active profile -- see binds_fw.cpp).
//   COMBO / MACRO: the paddle bit is cleared and the TB_* bits of the combo / current macro step are ORed in,
//           so every mode (push or stream) just sees ordinary buttons.
// Macros are shared by all profiles (pool of BINDS_MACROS). A macro fires on the paddle press and plays to the
// end once; releasing does not abort it and a press while it is running is ignored.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "config.h" // ET_COUNT
#include "triton.h" // TB_*, CHORD_BACK4

#define BINDS_PROFILES 3
#define BINDS_BACKS 4 // L4, R4, L5, R5 (same order as g_back[])
#define BINDS_MACROS 8
#define BINDS_STEPS 16

// BindAction.type
#define BA_BUTTON 0 // arg = button code 0..BINDS_CODE_MAX (see TypeCfg in config.h), 0 = none
#define BA_COMBO 1 // mask = TB_* bits pressed together
#define BA_MACRO 2 // arg = macro index
#define BINDS_CODE_MAX 20

// MacroDef.flags: 0 = play once. BM_REPEAT is reserved for a future "repeat while held" mode, NOT implemented
// (the loader rejects non-zero flags).
#define BM_REPEAT 0x01u

struct BindAction {
	uint8_t type; // BA_*
	uint8_t arg;
	uint32_t mask;
};

struct MacroStep {
	uint32_t mask; // TB_* bits held during this step (0 = pause)
	uint16_t ms; // step duration
};

struct MacroDef {
	uint8_t flags;
	uint8_t count; // 0 = macro not defined
	MacroStep step[BINDS_STEPS];
};

struct BindsCfg {
	uint8_t active; // active profile 0..BINDS_PROFILES-1
	BindAction act[BINDS_PROFILES][ET_COUNT][BINDS_BACKS];
	MacroDef macro[BINDS_MACROS];
};

extern BindsCfg g_binds;

// All profiles = BUTTON actions with the given codes (back[et][i]); NULL = 5,6,7,8 (LB,RB,L3,R3). No macros.
void bindsInitDefaults(BindsCfg &c, const uint8_t (*back)[BINDS_BACKS]);

// Apply the active profile to the raw buttons of one slot. etype = ET_* of the active mode (ET_NONE: unchanged).
// Call on every report (also repeats): macro playback follows nowMs, not button edges. All four paddles held
// (CHORD_BACK4, the mode/profile chord) returns raw untouched.
uint32_t bindsApply(uint8_t slot, uint32_t rawButtons, uint8_t etype,
		    uint32_t nowMs);
// Forget running macros / edge state (tests).
void bindsResetRuntime();

// Code g_back[i] must hold for paddle i: the BUTTON code of the active profile, 0 for COMBO / MACRO.
uint8_t bindsBackCode(const BindsCfg &c, uint8_t etype, uint8_t i);
// Set an action; false (unchanged) when out of range.
bool bindsSetAction(BindsCfg &c, uint8_t profile, uint8_t etype, uint8_t i,
		    uint8_t type, uint8_t arg, uint32_t mask);
// Replace macro `idx` with n steps (n = 0 clears it); false when out of range.
bool bindsSetMacro(BindsCfg &c, uint8_t idx, const MacroStep *steps, uint8_t n);

// ---- blob: [0]=magic [1]=version [2..3]=payload length LE, payload, u16 checksum LE (sum of payload bytes) ----
// payload: active, act[profile][etype][paddle] as {type, arg, mask u32 LE}, macro[] as
//          {flags, count, 16 x {mask u32 LE, ms u16 LE}}
#define BINDS_MAGIC 0xB1
#define BINDS_VERSION 1
#define BINDS_ACTION_BYTES 6
#define BINDS_STEP_BYTES 6
#define BINDS_PAYLOAD_BYTES                                                    \
	(1 + BINDS_PROFILES * ET_COUNT * BINDS_BACKS * BINDS_ACTION_BYTES +    \
	 BINDS_MACROS * (2 + BINDS_STEPS * BINDS_STEP_BYTES))
#define BINDS_BLOB_BYTES (4 + BINDS_PAYLOAD_BYTES + 2)

// Returns BINDS_BLOB_BYTES, or 0 when cap is too small.
size_t bindsSerialize(const BindsCfg &c, uint8_t *buf, size_t cap);
// Validates fully (header, length, checksum, every field range); on failure returns false and leaves c untouched.
bool bindsDeserialize(BindsCfg &c, const uint8_t *buf, size_t len);
// bindsDeserialize, but a bad blob yields the defaults. Returns true when the blob was valid.
bool bindsLoad(BindsCfg &c, const uint8_t *buf, size_t len,
	       const uint8_t (*back)[BINDS_BACKS]);
