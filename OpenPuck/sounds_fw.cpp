#include "sounds_fw.h"
#include "haptics.h"
#include <Arduino.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include <string.h>
using namespace Adafruit_LittleFS_Namespace;

#define SOUNDS_FILE "/sounds.bin"
// Present only between a mode switch and the first melody after it: RAM does not survive the reboot.
#define SOUNDS_MODE_FILE "/sndmode.bin"
// The controller reconnects in bursts when the link flaps; one connect melody per slot in this time is enough.
#define CONNECT_MIN_GAP_MS 20000u

// OUTPUT report 0x83 sides. Which value means "both" is not documented, so each tone goes out once per side.
#define TONE_SIDES 2

// Stage buffers are static: loop task stack is small and the blob is ~1 KB.
static uint8_t s_io[SOUNDS_BLOB_BYTES];
static uint8_t s_stage[SOUNDS_BLOB_BYTES];
static uint16_t s_staged;

static SoundPlayer s_player;
static uint8_t s_mask; // slots the running melody goes to
static uint8_t s_pendConnect; // slots waiting for their connect melody
static uint32_t s_lastConnectMs[NSLOT];
static bool s_pendMode;

static bool slotReady(uint8_t s)
{
	return !haptic82Blocked((int)s);
}

static uint8_t readyMask()
{
	uint8_t m = 0;
	for (uint8_t s = 0; s < NSLOT; s++)
		if (slotReady(s))
			m |= (uint8_t)(1u << s);
	return m;
}

// The controller's haptic latches: a melody always ends with the same zero rumble report the profile buzz uses.
static void sendStop(uint8_t mask)
{
	static const uint8_t zero[9] = { 0 };
	for (uint8_t s = 0; s < NSLOT; s++)
		if (mask & (1u << s))
			relayEnqueue(0x80, zero, sizeof zero, true, s);
}

static void sendTone(uint8_t mask, uint16_t freq, uint16_t ms)
{
	uint8_t p[9];
	p[1] = (uint8_t)g_sounds.gainDb;
	p[2] = (uint8_t)freq;
	p[3] = (uint8_t)(freq >> 8);
	p[4] = (uint8_t)ms;
	p[5] = (uint8_t)(ms >> 8);
	// no LFO
	p[6] = p[7] = p[8] = 0;
	for (uint8_t s = 0; s < NSLOT; s++) {
		if (!(mask & (1u << s)) || !hapticLinkUp((int)s))
			continue;
		for (uint8_t side = 0; side < TONE_SIDES; side++) {
			p[0] = side;
			relayEnqueue(0x83, p, sizeof p, true, s);
		}
	}
}

static bool start(const Melody &m, uint8_t mask)
{
	if (!m.count || !mask)
		return false;
	if (soundPlaying(s_player))
		sendStop(s_mask);
	s_mask = mask;
	soundStart(s_player, m, millis());
	return true;
}

void soundsFwTask()
{
	if (soundPlaying(s_player)) {
		const SoundStep st = soundStep(s_player, millis());
		if (st.action == SA_TONE)
			sendTone(s_mask, st.freq, st.ms);
		else if (st.action == SA_STOP)
			sendStop(s_mask);
		return;
	}
	if (!s_pendMode && !s_pendConnect)
		return;
	const uint8_t ready = readyMask();
	if (!ready)
		return;
	if (s_pendMode) {
		s_pendMode = false;
		InternalFS.remove(SOUNDS_MODE_FILE);
		// the mode melody replaces the connect one: both fire on the first connect after the reboot
		s_pendConnect = 0;
		const Melody *m =
			soundsFor(g_sounds, soundsModeEvent(g_usbMode));
		if (m)
			start(*m, ready);
		return;
	}
	const uint8_t go = s_pendConnect & ready;
	if (!go)
		return;
	s_pendConnect &= (uint8_t)~go;
	const Melody *m = soundsFor(g_sounds, SE_CONNECT);
	if (m)
		start(*m, go);
}

bool soundsFwPlayEvent(uint8_t event)
{
	if (!(g_sounds.confirm & SC_SOUND))
		return false;
	const Melody *m = soundsFor(g_sounds, event);
	return m && start(*m, readyMask());
}

bool soundsFwPlaySlot(uint8_t n)
{
	return n < SOUNDS_MELODIES && start(g_sounds.melody[n], readyMask());
}

bool soundsFwPlayRaw(const uint8_t *buf, uint8_t len)
{
	Melody m;
	return soundsParseMelody(m, buf, len) && start(m, readyMask());
}

void soundsFwOnConnect(int slot)
{
	if (slot < 0 || slot >= NSLOT || !(g_sounds.confirm & SC_SOUND))
		return;
	const uint32_t now = millis();
	if (s_lastConnectMs[slot] &&
	    (uint32_t)(now - s_lastConnectMs[slot]) < CONNECT_MIN_GAP_MS)
		return;
	s_lastConnectMs[slot] = now ? now : 1;
	s_pendConnect |= (uint8_t)(1u << slot);
}

void soundsFwNoteModeSwitch()
{
	if (!(g_sounds.confirm & SC_SOUND))
		return;
	static const uint8_t flag[1] = { SOUNDS_MAGIC };
	InternalFS.remove(SOUNDS_MODE_FILE);
	File f(InternalFS);
	if (f.open(SOUNDS_MODE_FILE, FILE_O_WRITE)) {
		f.write(flag, sizeof flag);
		f.close();
	}
}

bool soundsFwVibrate()
{
	return g_sounds.confirm & SC_VIBRATION;
}

void soundsFwSave()
{
	if (!soundsSerialize(g_sounds, s_io, sizeof s_io))
		return;
	InternalFS.remove(SOUNDS_FILE);
	File f(InternalFS);
	if (f.open(SOUNDS_FILE, FILE_O_WRITE)) {
		f.write(s_io, SOUNDS_BLOB_BYTES);
		f.close();
	}
}

void soundsFwInit()
{
	int got = 0;
	File f(InternalFS);
	if (f.open(SOUNDS_FILE, FILE_O_READ)) {
		got = f.read(s_io, sizeof s_io);
		f.close();
	}
	soundsLoad(g_sounds, s_io, got > 0 ? (size_t)got : 0);
	File m(InternalFS);
	if (m.open(SOUNDS_MODE_FILE, FILE_O_READ)) {
		s_pendMode = true;
		m.close();
	}
}

bool soundsFwSetConfirm(uint8_t confirm)
{
	if (confirm > SC_BOTH)
		return false;
	g_sounds.confirm = confirm;
	soundsFwSave();
	return true;
}

bool soundsFwSetGain(int gainDb)
{
	if (gainDb < SOUNDS_GAIN_MIN || gainDb > 0)
		return false;
	g_sounds.gainDb = (int8_t)gainDb;
	soundsFwSave();
	return true;
}

// ---- WebUSB blob transfer ----
uint8_t soundsFwReadChunk(uint8_t idx, uint8_t *out)
{
	if (idx >= SOUNDS_CHUNKS ||
	    !soundsSerialize(g_sounds, s_io, sizeof s_io))
		return 0;
	size_t off = (size_t)idx * SOUNDS_CHUNK;
	size_t n = SOUNDS_BLOB_BYTES - off;
	if (n > SOUNDS_CHUNK)
		n = SOUNDS_CHUNK;
	memcpy(out, s_io + off, n);
	return (uint8_t)n;
}

bool soundsFwStageChunk(uint8_t idx, const uint8_t *data, uint8_t len)
{
	size_t off = (size_t)idx * SOUNDS_CHUNK;
	if (idx == 0)
		s_staged = 0;
	if (len == 0 || len > SOUNDS_CHUNK || off != s_staged ||
	    off + len > SOUNDS_BLOB_BYTES)
		return false;
	memcpy(s_stage + off, data, len);
	s_staged = (uint16_t)(off + len);
	return true;
}

bool soundsFwStageCommit()
{
	if (s_staged != SOUNDS_BLOB_BYTES ||
	    !soundsDeserialize(g_sounds, s_stage, s_staged)) {
		s_staged = 0;
		return false;
	}
	s_staged = 0;
	soundsFwSave();
	return true;
}
