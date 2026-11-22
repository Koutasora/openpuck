// Host test for OpenPuck/sounds.cpp (pure logic). Build + run: sh tests/sounds/run.sh
#include "../../OpenPuck/config.h"
#include "../../OpenPuck/sounds.h"
#include <stdio.h>
#include <string.h>

static int g_fail = 0, g_n = 0;
#define CHECK(c)                                                            \
	do {                                                                \
		g_n++;                                                      \
		if (!(c)) {                                                 \
			g_fail++;                                           \
			printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); \
		}                                                           \
	} while (0)

static void testDefaults()
{
	SoundsCfg c;
	soundsInitDefaults(c);
	CHECK(c.confirm == SC_VIBRATION && c.gainDb == SOUNDS_GAIN_DEFAULT);
	for (int e = 0; e < SE_COUNT; e++) {
		const Melody *m = soundsFor(c, (uint8_t)e);
		CHECK(m && m->count >= 2 && m->count <= 6);
		CHECK(m && soundsMelodyValid(*m));
	}
	c.event[SE_CONNECT] = SOUND_NONE;
	CHECK(!soundsFor(c, SE_CONNECT));
	c.event[SE_CONNECT] = 3;
	c.melody[3].count = 0;
	CHECK(!soundsFor(c, SE_CONNECT));
	CHECK(!soundsFor(c, SE_COUNT));
}

static void testModeFamilies()
{
	CHECK(soundsModeEvent(MODE_PS5) == SE_MODE_PLAYSTATION);
	CHECK(soundsModeEvent(MODE_PS3) == SE_MODE_PLAYSTATION);
	CHECK(soundsModeEvent(MODE_PS5_AUTH) == SE_MODE_PLAYSTATION);
	CHECK(soundsModeEvent(MODE_HIDGYRO) == SE_MODE_PLAYSTATION);
	CHECK(soundsModeEvent(MODE_XBOX) == SE_MODE_XBOX);
	CHECK(soundsModeEvent(MODE_XBOX_OG) == SE_MODE_XBOX);
	CHECK(soundsModeEvent(MODE_SW_HORI) == SE_MODE_SWITCH);
	CHECK(soundsModeEvent(MODE_SW_PRO) == SE_MODE_SWITCH);
	CHECK(soundsModeEvent(MODE_STEAM) == SE_MODE_OTHER);
	CHECK(soundsModeEvent(MODE_LIZARD) == SE_MODE_OTHER);
	CHECK(soundsModeEvent(MODE_DINPUT) == SE_MODE_OTHER);
	CHECK(soundsModeEvent(MODE_SINPUT) == SE_MODE_OTHER);
}

// Runs the player to the end with a 1 ms clock and records the actions.
struct Log {
	int n;
	SoundAction act[80];
	uint16_t freq[80];
	uint32_t at[80];
};

static Log run(const Melody &m, uint32_t start)
{
	SoundPlayer p;
	Log l;
	memset(&l, 0, sizeof l);
	soundStart(p, m, start);
	for (uint32_t t = start; t < start + 40000 && soundPlaying(p); t++) {
		SoundStep s = soundStep(p, t);
		if (s.action != SA_NONE && l.n < 80) {
			l.act[l.n] = s.action;
			l.freq[l.n] = s.freq;
			l.at[l.n++] = t - start;
		}
	}
	return l;
}

static void testPlayer()
{
	Melody m;
	memset(&m, 0, sizeof m);
	m.count = 3;
	m.note[0] = { 440, 100 };
	m.note[1] = { 0, 50 };
	m.note[2] = { 880, 200 };
	Log l = run(m, 1000);
	// tone, stop (rest), tone, stop (end) at the right moments
	CHECK(l.n == 4);
	CHECK(l.act[0] == SA_TONE && l.freq[0] == 440 && l.at[0] == 0);
	CHECK(l.act[1] == SA_STOP && l.at[1] == 100);
	CHECK(l.act[2] == SA_TONE && l.freq[2] == 880 && l.at[2] == 150);
	CHECK(l.act[3] == SA_STOP && l.at[3] == 350);

	// nothing before the first call is due, one action per call
	SoundPlayer p;
	soundStart(p, m, 500);
	CHECK(soundStep(p, 499).action == SA_NONE);
	CHECK(soundStep(p, 500).action == SA_TONE);
	CHECK(soundStep(p, 500).action == SA_NONE);
	CHECK(soundStep(p, 599).action == SA_NONE);
	CHECK(soundStep(p, 600).action == SA_STOP);

	// the last note is never lost, even when the loop was late for it
	soundStart(p, m, 0);
	soundStep(p, 0);
	soundStep(p, 100);
	soundStep(p, 150);
	CHECK(soundPlaying(p));
	CHECK(soundStep(p, 5000).action == SA_STOP);
	CHECK(!soundPlaying(p));
	CHECK(soundStep(p, 9000).action == SA_NONE);

	// empty melody does not start
	Melody e;
	memset(&e, 0, sizeof e);
	soundStart(p, e, 0);
	CHECK(!soundPlaying(p));

	// millis() rollover in the middle of a note
	soundStart(p, m, 0xFFFFFFC0u);
	CHECK(soundStep(p, 0xFFFFFFC0u).action == SA_TONE);
	CHECK(soundStep(p, 0xFFFFFFF0u).action == SA_NONE);
	CHECK(soundStep(p, 0x30u).action == SA_STOP);

	// a restarted player forgets the old melody
	soundStart(p, m, 0);
	soundStep(p, 0);
	soundStart(p, m, 10);
	CHECK(soundStep(p, 10).action == SA_TONE);
}

static void testBlob()
{
	SoundsCfg c, d;
	soundsInitDefaults(c);
	uint8_t buf[SOUNDS_BLOB_BYTES + 8];
	CHECK(soundsSerialize(c, buf, SOUNDS_BLOB_BYTES - 1) == 0);
	CHECK(soundsSerialize(c, buf, sizeof buf) == SOUNDS_BLOB_BYTES);
	CHECK(buf[0] == SOUNDS_MAGIC && buf[1] == SOUNDS_VERSION);

	c.confirm = SC_BOTH;
	c.gainDb = -20;
	c.event[SE_CONNECT] = SOUND_NONE;
	c.melody[7].count = 1;
	c.melody[7].note[0] = { 1234, 4321 };
	CHECK(soundsSerialize(c, buf, sizeof buf) == SOUNDS_BLOB_BYTES);
	memset(&d, 0, sizeof d);
	CHECK(soundsDeserialize(d, buf, SOUNDS_BLOB_BYTES));
	CHECK(d.confirm == SC_BOTH && d.gainDb == -20);
	CHECK(d.event[SE_CONNECT] == SOUND_NONE && d.event[0] == 0);
	CHECK(d.melody[7].count == 1 && d.melody[7].note[0].freq == 1234 &&
	      d.melody[7].note[0].ms == 4321);
	CHECK(d.melody[0].count == 2 && d.melody[0].note[1].freq == 784);

	// wrong length, magic, version, checksum, every field range: rejected whole, target untouched
	SoundsCfg keep = d;
	uint8_t bad[SOUNDS_BLOB_BYTES];
	CHECK(!soundsDeserialize(d, buf, SOUNDS_BLOB_BYTES - 1));
	CHECK(!soundsDeserialize(d, nullptr, SOUNDS_BLOB_BYTES));
	memcpy(bad, buf, sizeof bad);
	bad[0] ^= 1;
	CHECK(!soundsDeserialize(d, bad, sizeof bad));
	memcpy(bad, buf, sizeof bad);
	bad[1] = 2;
	CHECK(!soundsDeserialize(d, bad, sizeof bad));
	memcpy(bad, buf, sizeof bad);
	bad[10] ^= 1; // payload byte, checksum no longer matches
	CHECK(!soundsDeserialize(d, bad, sizeof bad));
	CHECK(!memcmp(&d, &keep, sizeof d));

	struct {
		int off;
		uint8_t v;
	} range[] = {
		{ 4, 4 }, // confirm
		{ 5, 0x01 }, // gain +1 dB
		{ 5, 0xC0 }, // gain -64 dB
		{ 6, 8 }, // event -> melody 8
		{ 14, 33 }, // note count
	};
	for (auto &r : range) {
		SoundsCfg x = c;
		(void)x;
		memcpy(bad, buf, sizeof bad);
		bad[r.off] = r.v;
		// fix the checksum so only the range rule can reject it
		uint16_t s = 0;
		for (int i = 4; i < 4 + SOUNDS_PAYLOAD_BYTES; i++)
			s = (uint16_t)(s + bad[i]);
		bad[4 + SOUNDS_PAYLOAD_BYTES] = (uint8_t)s;
		bad[5 + SOUNDS_PAYLOAD_BYTES] = (uint8_t)(s >> 8);
		CHECK(!soundsDeserialize(d, bad, sizeof bad));
	}
	// note out of range (frequency, zero / too long duration) in the first note of melody 0
	const int note0 = 4 + 2 + SE_COUNT + 1;
	struct {
		int off;
		uint8_t lo, hi;
	} note[] = { { note0, 0xA1, 0x0F },
		     { note0 + 2, 0, 0 },
		     { note0 + 2, 0x89, 0x13 } };
	for (auto &r : note) {
		memcpy(bad, buf, sizeof bad);
		bad[r.off] = r.lo;
		bad[r.off + 1] = r.hi;
		uint16_t s = 0;
		for (int i = 4; i < 4 + SOUNDS_PAYLOAD_BYTES; i++)
			s = (uint16_t)(s + bad[i]);
		bad[4 + SOUNDS_PAYLOAD_BYTES] = (uint8_t)s;
		bad[5 + SOUNDS_PAYLOAD_BYTES] = (uint8_t)(s >> 8);
		CHECK(!soundsDeserialize(d, bad, sizeof bad));
	}

	// load: a bad file gives the defaults, a good one the saved values
	SoundsCfg l;
	CHECK(!soundsLoad(l, bad, sizeof bad));
	CHECK(l.confirm == SC_VIBRATION && l.melody[0].count == 2);
	CHECK(!soundsLoad(l, nullptr, 0));
	CHECK(soundsLoad(l, buf, SOUNDS_BLOB_BYTES) && l.confirm == SC_BOTH);
	CHECK(SOUNDS_BLOB_BYTES == 1048);
}

static void testPreview()
{
	uint8_t b[1 + 2 * SOUNDS_NOTE_BYTES] = { 2, 0xB8, 0x01, 100, 0,
						 0, 0,	  50,	0 };
	Melody m;
	memset(&m, 0, sizeof m);
	CHECK(soundsParseMelody(m, b, sizeof b));
	CHECK(m.count == 2 && m.note[0].freq == 440 && m.note[0].ms == 100 &&
	      m.note[1].freq == 0 && m.note[1].ms == 50);
	Melody keep = m;
	CHECK(!soundsParseMelody(m, b, sizeof b - 1)); // short
	CHECK(!soundsParseMelody(m, b, sizeof b + 1)); // long
	b[0] = 33;
	CHECK(!soundsParseMelody(m, b, sizeof b));
	b[0] = 2;
	b[7] = 0; // note 1 duration 0
	b[8] = 0;
	CHECK(!soundsParseMelody(m, b, sizeof b));
	b[7] = SOUNDS_NOTE_MS_MIN - 1; // shorter than the minimum
	CHECK(!soundsParseMelody(m, b, sizeof b));
	CHECK(!memcmp(&m, &keep, sizeof m));
	b[7] = SOUNDS_NOTE_MS_MIN;
	CHECK(soundsParseMelody(m, b, sizeof b) &&
	      m.note[1].ms == SOUNDS_NOTE_MS_MIN);
	uint8_t z = 0;
	CHECK(soundsParseMelody(m, &z, 1) && m.count == 0);
}

int main()
{
	testDefaults();
	testModeFamilies();
	testPlayer();
	testBlob();
	testPreview();
	printf("%d checks, %d failed\n", g_n, g_fail);
	return g_fail ? 1 : 0;
}
