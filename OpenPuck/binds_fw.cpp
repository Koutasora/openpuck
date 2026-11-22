#include "binds_fw.h"
#include "config.h"
#include "bonds.h"
#include "haptics.h"
#include "kbm_fw.h"
#include "triton.h"
#include <Arduino.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include <string.h>
using namespace Adafruit_LittleFS_Namespace;

#define BINDS_FILE "/binds.bin"

uint32_t g_rawButtons[NSLOT];

// Serialize / stage buffers are static: loop task stack is small and the blob is ~1.1 KB.
static uint8_t s_io[BINDS_BLOB_BYTES];
static uint8_t s_stage[BINDS_BLOB_BYTES];
static uint16_t s_staged; // bytes staged so far

// ---- confirmation buzz: N short pulses = profile number, stepped from loop() ----
#define PULSE_ON_MS 120u
#define PULSE_GAP_MS 200u
static uint8_t s_pulsesLeft = 0;
static bool s_pulseOn = false;
static uint32_t s_pulseNextMs = 0;

static void buzz(uint16_t amp)
{
	for (uint8_t s = 0; s < NSLOT; s++)
		if (hapticLinkUp((int)s))
			hapticSteamRumble(amp, amp, s);
}

void bindsFwTask()
{
	if (!s_pulsesLeft || (int32_t)(millis() - s_pulseNextMs) < 0)
		return;
	if (!s_pulseOn) {
		buzz(RUMBLE_TEST_AMP);
		s_pulseOn = true;
		s_pulseNextMs = millis() + PULSE_ON_MS;
	} else {
		// the controller's haptic latches: the stop is not optional
		buzz(0);
		s_pulseOn = false;
		s_pulsesLeft--;
		s_pulseNextMs = millis() + PULSE_GAP_MS;
	}
}

// ---- persistence ----
void bindsFwSave()
{
	if (!bindsSerialize(g_binds, s_io, sizeof s_io))
		return;
	InternalFS.remove(BINDS_FILE);
	File f(InternalFS);
	if (f.open(BINDS_FILE, FILE_O_WRITE)) {
		f.write(s_io, BINDS_BLOB_BYTES);
		f.close();
	}
}

static void syncBack()
{
	for (int e = 0; e < ET_COUNT; e++)
		for (int i = 0; i < BINDS_BACKS; i++)
			g_type[e].back[i] = bindsBackCode(g_binds, (uint8_t)e,
							  (uint8_t)i);
	applyActiveType();
}

void bindsFwInit()
{
	uint8_t back[ET_COUNT][BINDS_BACKS];
	for (int e = 0; e < ET_COUNT; e++)
		memcpy(back[e], g_type[e].back, BINDS_BACKS);
	int got = 0;
	File f(InternalFS);
	if (f.open(BINDS_FILE, FILE_O_READ)) {
		got = f.read(s_io, sizeof s_io);
		f.close();
	}
	bindsLoad(g_binds, s_io, got > 0 ? (size_t)got : 0, back);
	bindsResetRuntime();
	syncBack();
}

void bindsFwCommit()
{
	syncBack();
	bindsFwSave();
}

void bindsFwSetButton(uint8_t etype, uint8_t paddle, uint8_t code)
{
	if (bindsSetAction(g_binds, g_binds.active, etype, paddle, BA_BUTTON,
			   code, 0))
		bindsFwCommit();
}

bool bindsFwSetProfile(uint8_t profile)
{
	if (profile >= BINDS_PROFILES)
		return false;
	g_binds.active = profile;
	bindsFwCommit();
	s_pulsesLeft = (uint8_t)(profile + 1);
	s_pulseOn = false;
	s_pulseNextMs = millis();
	return true;
}

// ---- per-report hooks (rf_link.cpp) ----
uint32_t bindsFwFilter(uint8_t slot, uint32_t raw)
{
	if (slot >= NSLOT)
		return raw;
	g_rawButtons[slot] = raw;
	// While all four paddles are held (mode / profile chord) the paddles must not reach the host through
	// g_back[] either: park g_back[] at 0 for the duration, restore on release.
	static uint8_t held = 0;
	const uint8_t before = held;
	if (g_etype < ET_COUNT && (raw & CHORD_BACK4) == CHORD_BACK4)
		held |= (uint8_t)(1u << slot);
	else
		held &= (uint8_t)~(1u << slot);
	if (held != before) {
		for (int i = 0; i < BINDS_BACKS; i++)
			g_back[i] = held ? 0 : g_type[g_etype].back[i];
	}
	BindsKbm kbm;
	const uint32_t out = bindsApply(slot, raw, g_etype, millis(), &kbm);
	kbmFwSubmit(slot, kbm);
	return out;
}

bool bindsFwProfileChord(uint8_t slot, uint32_t raw)
{
	if (slot >= NSLOT || g_etype >= ET_COUNT)
		return false;
	// Same debounce idea as the mode chord (12 reports ~ 48 ms at 250 Hz), per slot, once per hold.
	static uint8_t cnt[NSLOT];
	static bool fired[NSLOT];
	const bool back4 = (raw & CHORD_BACK4) == CHORD_BACK4;
	const bool rb = raw & TB_RB, lb = raw & TB_LB;
	if (!back4 || rb == lb) { // not held, or both shoulders (ambiguous)
		cnt[slot] = 0;
		if (!back4 || (!rb && !lb))
			fired[slot] = false;
		return back4;
	}
	if (!fired[slot] && ++cnt[slot] >= 12) {
		fired[slot] = true;
		uint8_t p = g_binds.active;
		p = rb ? (uint8_t)((p + 1) % BINDS_PROFILES) :
			 (uint8_t)((p + BINDS_PROFILES - 1) % BINDS_PROFILES);
		bindsFwSetProfile(p);
	}
	return true;
}

// ---- WebUSB blob transfer ----
uint8_t bindsFwReadChunk(uint8_t idx, uint8_t *out)
{
	if (idx >= BINDS_CHUNKS || !bindsSerialize(g_binds, s_io, sizeof s_io))
		return 0;
	size_t off = (size_t)idx * BINDS_CHUNK;
	size_t n = BINDS_BLOB_BYTES - off;
	if (n > BINDS_CHUNK)
		n = BINDS_CHUNK;
	memcpy(out, s_io + off, n);
	return (uint8_t)n;
}

bool bindsFwStageChunk(uint8_t idx, const uint8_t *data, uint8_t len)
{
	size_t off = (size_t)idx * BINDS_CHUNK;
	if (idx == 0)
		s_staged = 0;
	if (len == 0 || len > BINDS_CHUNK || off != s_staged ||
	    off + len > BINDS_BLOB_BYTES)
		return false;
	memcpy(s_stage + off, data, len);
	s_staged = (uint16_t)(off + len);
	return true;
}

bool bindsFwStageCommit()
{
	if (s_staged != BINDS_BLOB_BYTES ||
	    !bindsDeserialize(g_binds, s_stage, s_staged)) {
		s_staged = 0;
		return false;
	}
	s_staged = 0;
	bindsResetRuntime();
	bindsFwCommit();
	return true;
}
