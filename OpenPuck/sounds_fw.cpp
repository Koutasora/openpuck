#include "sounds_fw.h"
#include "haptics.h"
#include <Arduino.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include <string.h>
using namespace Adafruit_LittleFS_Namespace;

#define SOUNDS_FILE "/sounds.bin"
// Present only between a mode switch and the next boot: RAM does not survive the reboot. Removed as soon as it
// is read, so the melody can only play on the first connection after that boot, never days later.
#define SOUNDS_MODE_FILE "/sndmode.bin"
// The controller reconnects in bursts when the link flaps; one connect melody per slot in this time is enough.
#define CONNECT_MIN_GAP_MS 20000u

// A melody waits while a linked slot has more than this many entries queued, so it never pushes out other
// frames (imuForceRaw, id9, power-off). One due event adds one entry per controller and up to SOUNDS_VOICES
// events fall due together, so with 6 a full chord (4) still goes out in one loop on top of two entries left
// from the last one, and 25 of the 32 entries stay free. Given up after STALL_MAX_MS.
#define QUEUE_BUSY_MAX 6
#define STALL_MAX_MS 1000u

// Only the committed blob is staged here (loop task stack is small). Also holds the file while it is read at boot.
static uint8_t s_stage[SOUNDS_BLOB_BYTES];
static uint16_t s_staged;

static SoundPlayer s_player;
static uint8_t s_mask; // slots the running melody goes to
static uint8_t s_pendConnect; // slots waiting for their connect melody
static uint32_t s_lastConnectMs[NSLOT];
static bool s_pendMode;
static uint32_t s_stallSince; // 0 = the melody is not waiting for the queue

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

// HAPTIC_LFO_TONE {side, gain_db, freq u16, dur u16, lfo_freq u16, lfo_depth}: the controller ends the tone by
// itself after dur.
static void sendTone(uint8_t mask, uint8_t side, int8_t gainDb, uint16_t freq,
		     uint16_t dur)
{
	uint8_t p[9];
	p[0] = side;
	p[1] = (uint8_t)gainDb;
	p[2] = (uint8_t)freq;
	p[3] = (uint8_t)(freq >> 8);
	p[4] = (uint8_t)dur;
	p[5] = (uint8_t)(dur >> 8);
	// no LFO
	p[6] = p[7] = p[8] = 0;
	for (uint8_t s = 0; s < NSLOT; s++)
		if ((mask & (1u << s)) && hapticLinkUp((int)s))
			relayEnqueue(0x83, p, sizeof p, true, s);
}

// Stops one side and leaves the others playing: gain -128, freq 0, dur 0x8000 (verified on hardware).
static void sendOff(uint8_t mask, uint8_t side)
{
	sendTone(mask, side, -128, 0, 0x8000);
}

static bool queueBusy(uint8_t mask)
{
	for (uint8_t s = 0; s < NSLOT; s++)
		if ((mask & (1u << s)) && hapticLinkUp((int)s) &&
		    relayDepth(s) > QUEUE_BUSY_MAX)
			return true;
	return false;
}

static bool start(const Melody &m, uint8_t mask)
{
	if (soundsMelodyEmpty(m) || !mask)
		return false;
	if (soundPlaying(s_player))
		sendStop(s_mask);
	s_mask = mask;
	soundStart(s_player, m, millis());
	s_stallSince = 0;
	return true;
}

void soundsFwTask()
{
	if (soundPlaying(s_player)) {
		const uint32_t now = millis();
		// the voices that fall due together go out in one pass
		for (uint8_t i = 0; i < SOUNDS_VOICES; i++) {
			if (!soundDue(s_player, now)) {
				s_stallSince = 0;
				break;
			}
			if (queueBusy(s_mask)) {
				// delay rather than squeeze in; a queue that never drains (link lost) ends the melody
				if (!s_stallSince)
					s_stallSince = now ? now : 1;
				else if ((uint32_t)(now - s_stallSince) >
					 STALL_MAX_MS) {
					s_player.playing = false;
					s_stallSince = 0;
					sendStop(s_mask);
				}
				return;
			}
			s_stallSince = 0;
			const SoundStep st = soundStep(s_player, now);
			if (st.action == SA_TONE)
				sendTone(s_mask, soundsSide(st.voice),
					 g_sounds.gainDb, st.freq, st.ms);
			else if (st.action == SA_OFF)
				sendOff(s_mask, soundsSide(st.voice));
			else if (st.action == SA_STOP)
				sendStop(s_mask);
		}
		return;
	}
	if (!s_pendMode && !s_pendConnect)
		return;
	const uint8_t ready = readyMask();
	if (!ready)
		return;
	if (s_pendMode) {
		s_pendMode = false;
		// the setting may have been changed since the switch
		if (!(g_sounds.confirm & SC_SOUND))
			return;
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
	// static: a Melody is 520 bytes, too much for the loop task stack next to the call chain
	static Melody m;
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
	uint8_t buf[SOUNDS_CHUNK];
	InternalFS.remove(SOUNDS_FILE);
	File f(InternalFS);
	if (!f.open(SOUNDS_FILE, FILE_O_WRITE))
		return;
	for (size_t off = 0; off < SOUNDS_BLOB_BYTES; off += SOUNDS_CHUNK)
		f.write(buf, soundsSerializeAt(g_sounds, off, buf, sizeof buf));
	f.close();
}

void soundsFwInit()
{
	int got = 0;
	File f(InternalFS);
	if (f.open(SOUNDS_FILE, FILE_O_READ)) {
		got = f.read(s_stage, sizeof s_stage);
		f.close();
	}
	soundsLoad(g_sounds, s_stage, got > 0 ? (size_t)got : 0);
	File m(InternalFS);
	if (m.open(SOUNDS_MODE_FILE, FILE_O_READ)) {
		s_pendMode = true;
		m.close();
		InternalFS.remove(SOUNDS_MODE_FILE);
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
	if (idx >= SOUNDS_CHUNKS)
		return 0;
	return (uint8_t)soundsSerializeAt(g_sounds, (size_t)idx * SOUNDS_CHUNK,
					  out, SOUNDS_CHUNK);
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
	// the length is checked by soundsDeserialize: a v2 blob, or a v1 one from a page that predates voices
	if (!soundsDeserialize(g_sounds, s_stage, s_staged)) {
		s_staged = 0;
		return false;
	}
	s_staged = 0;
	soundsFwSave();
	return true;
}
