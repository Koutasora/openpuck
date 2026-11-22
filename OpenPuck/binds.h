// binds.h -- back-paddle (L4/R4/L5/R5) actions: single button, button combo, or macro; 3 switchable profiles.
//
// Pure logic: no Arduino, no hardware, time comes in as an argument, so tests/binds builds it with a host g++.
// bindsApply() runs ONCE per decoded input report in rf_link.cpp, before any emulation mode sees the buttons:
//   BUTTON: the paddle bit is left alone; the modes translate it through g_back[] as before (g_back[] mirrors
//           the BUTTON codes of the active profile -- see binds_fw.cpp).
//   COMBO / MACRO: the paddle bit is cleared and the TB_* bits of the combo / current macro step are ORed in,
//           so every mode (push or stream) just sees ordinary buttons.
//   KEY / MOUSE: the paddle bit is cleared and the action is reported through BindsKbm instead; the caller sends it
//           to a USB keyboard / mouse (only on a PC host, see host_detect.h) -- elsewhere it does nothing.
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
// arg = button code 0..BINDS_CODE_MAX (see TypeCfg in config.h), 0 = none
#define BA_BUTTON 0
// mask = TB_* bits pressed together; paddle bits (CHORD_BACK4) are refused everywhere
#define BA_COMBO 1
#define BA_MACRO 2 // arg = macro index
// arg = HID keyboard usage BINDS_KEY_MIN..BINDS_KEY_MAX, mask = modifier byte (KM_*); held while the paddle is held
#define BA_KEY 3
// arg = BMO_* code. Buttons are held with the paddle; wheel codes are notches (see below)
#define BA_MOUSE 4
#define BINDS_CODE_MAX 20

// usage 0..3 are the HID rollover / error codes, 0xE0..0xE7 are the modifier keys themselves
#define BINDS_KEY_MIN 0x04
#define BINDS_KEY_MAX 0xE7
// KEY mask = the HID keyboard modifier byte
#define KM_LCTRL 0x01u
#define KM_LSHIFT 0x02u
#define KM_LALT 0x04u
#define KM_LGUI 0x08u
#define KM_RCTRL 0x10u
#define KM_RSHIFT 0x20u
#define KM_RALT 0x40u
#define KM_RGUI 0x80u

// BA_MOUSE arg. 1..5 = HID mouse buttons 1..5 (held with the paddle). 6 / 7 = one wheel notch up / down per press.
// 8 / 9 = same, but repeated every BINDS_WHEEL_REPEAT_MS while the paddle stays held.
#define BMO_LEFT 1
#define BMO_RIGHT 2
#define BMO_MIDDLE 3
#define BMO_BACK 4
#define BMO_FORWARD 5
#define BMO_WHEEL_UP 6
#define BMO_WHEEL_DOWN 7
#define BMO_WHEEL_UP_REP 8
#define BMO_WHEEL_DOWN_REP 9
#define BMO_MAX 9
#define BINDS_WHEEL_REPEAT_MS 100u
#define BINDS_KEYS_MAX 6 // boot-keyboard report: 6 simultaneous keys

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

// What bindsApply asks the USB keyboard / mouse to do for one slot, right now.
struct BindsKbm {
	uint8_t mods; // keyboard modifier byte
	uint8_t keys[BINDS_KEYS_MAX]; // pressed usages, 0 = free
	// mouse buttons, bit n = button n+1 (L R M Back Forward)
	uint8_t buttons;
	// notches to emit with THIS call (+ up); 0 on every other call
	int8_t wheel;
};

// All profiles = BUTTON actions with the given codes (back[et][i]); NULL = 5,6,7,8 (LB,RB,L3,R3). No macros.
void bindsInitDefaults(BindsCfg &c, const uint8_t (*back)[BINDS_BACKS]);

// Apply the active profile to the raw buttons of one slot. etype = ET_* of the active mode (ET_NONE: unchanged).
// Call on every report (also repeats): macro playback follows nowMs, not button edges. All four paddles held
// (CHORD_BACK4, the mode/profile chord) returns raw untouched. kbm (optional) is filled with the KEY / MOUSE
// state of the slot; it is all zero for ET_NONE and during the chord.
uint32_t bindsApply(uint8_t slot, uint32_t rawButtons, uint8_t etype,
		    uint32_t nowMs, BindsKbm *kbm = nullptr);
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
// Version 2 has the layout of version 1; it only adds the action types BA_KEY / BA_MOUSE. Version 1 blobs are
// still read (types above BA_MACRO are rejected in them); serializing always writes version 2.
#define BINDS_MAGIC 0xB1
#define BINDS_VERSION 2
#define BINDS_ACTION_BYTES 6
#define BINDS_STEP_BYTES 6
#define BINDS_PAYLOAD_BYTES                                                 \
	(1 + BINDS_PROFILES * ET_COUNT * BINDS_BACKS * BINDS_ACTION_BYTES + \
	 BINDS_MACROS * (2 + BINDS_STEPS * BINDS_STEP_BYTES))
#define BINDS_BLOB_BYTES (4 + BINDS_PAYLOAD_BYTES + 2)

// Returns BINDS_BLOB_BYTES, or 0 when cap is too small.
size_t bindsSerialize(const BindsCfg &c, uint8_t *buf, size_t cap);
// Validates fully (header, length, checksum, every field range); on failure returns false and leaves c untouched.
bool bindsDeserialize(BindsCfg &c, const uint8_t *buf, size_t len);
// bindsDeserialize, but a bad blob yields the defaults. Returns true when the blob was valid.
bool bindsLoad(BindsCfg &c, const uint8_t *buf, size_t len,
	       const uint8_t (*back)[BINDS_BACKS]);
