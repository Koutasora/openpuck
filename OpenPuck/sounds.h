// sounds.h -- melodies played through the controller's haptic actuator (HAPTIC_LFO_TONE), assigned to events.
//
// Pure logic: no Arduino, no hardware, time comes in as an argument, so tests/sounds builds it with a host g++.
// A melody holds SOUNDS_VOICES voices, one per haptic actuator, played together. A voice is a list of notes
// {freq Hz, ms}; freq 0 is a rest. SoundPlayer steps through all voices without blocking: the caller asks for the
// next action each loop() and sends it (sounds_fw.cpp).
#pragma once
#include <stdint.h>
#include <stddef.h>

#define SOUNDS_MELODIES 8
#define SOUNDS_NOTES 32
// Voice v plays on HAPTIC_LFO_TONE side soundsSide(v): left trackpad, right trackpad, left grip, right grip.
#define SOUNDS_VOICES 4
#define SOUNDS_FREQ_MAX 4000
#define SOUNDS_NOTE_MS_MAX 5000
// Every note and rest of a voice is one relay entry per controller (0x83); the relay drains about one entry per
// poll cycle (~4 ms), so shorter ones would flood the queue and push out the other frames.
#define SOUNDS_NOTE_MS_MIN 30
// volume = gain_db of the tone report, 0 = loudest
#define SOUNDS_GAIN_MIN (-60)
#define SOUNDS_GAIN_DEFAULT (-6)

// Events a melody can be assigned to. Profile events are one per profile (binds.h BINDS_PROFILES = 3); the
// mode events are per controller family of the mode that was just entered.
#define SE_PROFILE1 0
#define SE_PROFILE2 1
#define SE_PROFILE3 2
#define SE_MODE_PLAYSTATION 3
#define SE_MODE_XBOX 4
#define SE_MODE_SWITCH 5
#define SE_MODE_OTHER 6 // Steam, Lizard, DirectInput, SInput
#define SE_CONNECT 7 // a controller connected
#define SE_COUNT 8
#define SOUND_NONE 0xFF

// SoundsCfg.confirm: what confirms a profile switch. Melodies of the other events play when SOUND is included.
#define SC_OFF 0
#define SC_VIBRATION 1
#define SC_SOUND 2
#define SC_BOTH 3

struct SoundNote {
	uint16_t freq; // Hz, 0 = rest
	uint16_t ms;
};

struct Voice {
	uint8_t count; // 0 = silent
	SoundNote note[SOUNDS_NOTES];
};

struct Melody {
	Voice voice[SOUNDS_VOICES];
};

struct SoundsCfg {
	uint8_t confirm; // SC_*
	int8_t gainDb; // SOUNDS_GAIN_MIN..0
	uint8_t event[SE_COUNT]; // melody index or SOUND_NONE
	Melody melody[SOUNDS_MELODIES];
};

extern SoundsCfg g_sounds;

// Own short motifs, melody i for event i.
void soundsInitDefaults(SoundsCfg &c);
// SE_MODE_* of a USB mode (config.h MODE_*).
uint8_t soundsModeEvent(uint8_t usbMode);
// Melody assigned to an event, nullptr when none or empty.
const Melody *soundsFor(const SoundsCfg &c, uint8_t event);
// HAPTIC_LFO_TONE side of a voice.
uint8_t soundsSide(uint8_t voice);
// Empty = every voice has no notes.
bool soundsMelodyEmpty(const Melody &m);
// Range check of one melody (note counts, frequencies, durations).
bool soundsMelodyValid(const Melody &m);

// ---- player ----
enum SoundAction {
	SA_NONE, // nothing due yet (or finished)
	SA_TONE, // start a tone on the side of `voice`: freq, ms
	// silence one side: a rest or the end of that voice. A tone may outlive its note if the controller ignores
	// the duration, so every gap is closed explicitly.
	SA_OFF,
	// silence everything: the end of the whole melody
	SA_STOP,
};

struct SoundStep {
	SoundAction action;
	uint8_t voice;
	uint16_t freq;
	uint16_t ms;
};

struct SoundPlayer {
	Melody melody; // own copy: the config may change while it plays
	uint8_t next[SOUNDS_VOICES]; // next note per voice
	// when each voice's next note starts, counted from the common start
	uint32_t dueMs[SOUNDS_VOICES];
	bool done[SOUNDS_VOICES]; // voice finished
	bool silent[SOUNDS_VOICES]; // the side is already quiet
	bool playing;
};

void soundStart(SoundPlayer &p, const Melody &m, uint32_t nowMs);
bool soundPlaying(const SoundPlayer &p);
// True when soundStep would return an action.
bool soundDue(const SoundPlayer &p, uint32_t nowMs);
// At most one action per call (the most overdue voice first); call while soundDue. Due times add up from the
// start, so a late action does not move the later ones. Time is compared as a signed difference (rollover safe).
SoundStep soundStep(SoundPlayer &p, uint32_t nowMs);

// ---- blob: [0]=magic [1]=version [2..3]=payload length LE, payload, u16 checksum LE (sum of payload bytes) ----
// payload v2: confirm, gainDb (s8), event[SE_COUNT], then for every melody and voice
// {count, SOUNDS_NOTES x {freq u16 LE, ms u16 LE}}. Notes past count are written as zeros and ignored when read.
// Version 1 had one voice per melody; it is still read, into voices 0 and 1 (both trackpads, as it always played).
#define SOUNDS_MAGIC 0xB8
#define SOUNDS_VERSION 2
#define SOUNDS_NOTE_BYTES 4
#define SOUNDS_VOICE_BYTES (1 + SOUNDS_NOTES * SOUNDS_NOTE_BYTES)
#define SOUNDS_MELODY_BYTES (SOUNDS_VOICES * SOUNDS_VOICE_BYTES)
#define SOUNDS_PAYLOAD_BYTES \
	(2 + SE_COUNT + SOUNDS_MELODIES * SOUNDS_MELODY_BYTES)
#define SOUNDS_BLOB_BYTES (4 + SOUNDS_PAYLOAD_BYTES + 2)
#define SOUNDS_V1_PAYLOAD_BYTES \
	(2 + SE_COUNT + SOUNDS_MELODIES * SOUNDS_VOICE_BYTES)
#define SOUNDS_V1_BLOB_BYTES (4 + SOUNDS_V1_PAYLOAD_BYTES + 2)

// Returns SOUNDS_BLOB_BYTES, or 0 when cap is too small.
size_t soundsSerialize(const SoundsCfg &c, uint8_t *buf, size_t cap);
// Bytes off.. of the blob, at most n; returns how many were written. Lets the firmware send and save the blob
// without a RAM copy of it.
size_t soundsSerializeAt(const SoundsCfg &c, size_t off, uint8_t *out,
			 size_t n);
// Validates fully; accepts version 2 and 1. On failure returns false and leaves c untouched.
bool soundsDeserialize(SoundsCfg &c, const uint8_t *buf, size_t len);
// soundsDeserialize, but a bad blob yields the defaults. Returns true when the blob was valid.
bool soundsLoad(SoundsCfg &c, const uint8_t *buf, size_t len);

// Melody sent by the page for a one-off preview: [count][count x {freq u16 LE, ms u16 LE}], one voice, played
// as voices 0 and 1. False when invalid.
bool soundsParseMelody(Melody &m, const uint8_t *buf, size_t len);
