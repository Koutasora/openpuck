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
		CHECK(m && m->voice[0].count >= 2 && m->voice[0].count <= 6);
		CHECK(m && soundsMelodyValid(*m));
		// the motifs sound on both trackpads, as before voices existed
		CHECK(m && !memcmp(&m->voice[0], &m->voice[1], sizeof(Voice)));
		CHECK(m && !m->voice[2].count && !m->voice[3].count);
	}
	c.event[SE_CONNECT] = SOUND_NONE;
	CHECK(!soundsFor(c, SE_CONNECT));
	c.event[SE_CONNECT] = 3;
	memset(&c.melody[3], 0, sizeof c.melody[3]);
	CHECK(!soundsFor(c, SE_CONNECT));
	// a melody with only voice 3 is not empty
	c.melody[3].voice[3].count = 1;
	c.melody[3].voice[3].note[0] = { 440, 100 };
	CHECK(soundsFor(c, SE_CONNECT));
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
	uint8_t voice[80];
	uint16_t freq[80];
	uint16_t ms[80];
	uint32_t at[80];
};

static Log run(const Melody &m, uint32_t start)
{
	SoundPlayer p;
	Log l;
	memset(&l, 0, sizeof l);
	soundStart(p, m, start);
	for (uint32_t t = start; t < start + 40000 && soundPlaying(p); t++)
		while (soundDue(p, t)) {
			SoundStep s = soundStep(p, t);
			// a voice that ended silently makes no action
			if (s.action != SA_NONE && l.n < 80) {
				l.act[l.n] = s.action;
				l.voice[l.n] = s.voice;
				l.freq[l.n] = s.freq;
				l.ms[l.n] = s.ms;
				l.at[l.n++] = t - start;
			}
		}
	return l;
}

static void testPlayer()
{
	Melody m;
	memset(&m, 0, sizeof m);
	Voice &v0 = m.voice[0];
	v0.count = 3;
	v0.note[0] = { 440, 100 };
	v0.note[1] = { 0, 50 };
	v0.note[2] = { 880, 200 };
	Log l = run(m, 1000);
	// tone, off (rest), tone, stop (end of the whole melody) at the right moments
	CHECK(l.n == 4);
	CHECK(l.act[0] == SA_TONE && l.freq[0] == 440 && l.ms[0] == 100 &&
	      l.at[0] == 0);
	CHECK(l.act[1] == SA_OFF && l.voice[1] == 0 && l.at[1] == 100);
	CHECK(l.act[2] == SA_TONE && l.freq[2] == 880 && l.at[2] == 150);
	CHECK(l.act[3] == SA_STOP && l.at[3] == 350);

	// nothing before the first call is due, one action per call
	SoundPlayer p;
	soundStart(p, m, 500);
	CHECK(soundStep(p, 499).action == SA_NONE);
	CHECK(!soundDue(p, 499) && soundDue(p, 500));
	CHECK(soundStep(p, 500).action == SA_TONE);
	CHECK(soundStep(p, 500).action == SA_NONE);
	CHECK(soundStep(p, 599).action == SA_NONE);
	CHECK(soundStep(p, 600).action == SA_OFF);

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
	CHECK(soundStep(p, 0x30u).action == SA_OFF);

	// a restarted player forgets the old melody
	soundStart(p, m, 0);
	soundStep(p, 0);
	soundStart(p, m, 10);
	CHECK(soundStep(p, 10).action == SA_TONE);
}

static void testVoices()
{
	CHECK(soundsSide(0) == 0 && soundsSide(1) == 1 && soundsSide(2) == 3 &&
	      soundsSide(3) == 4);

	// four voices share the start; every one has its own timing
	Melody m;
	memset(&m, 0, sizeof m);
	m.voice[0].count = 2; // 100 + 100: ends at 200
	m.voice[0].note[0] = { 440, 100 };
	m.voice[0].note[1] = { 550, 100 };
	m.voice[1].count = 3; // tone, rest, tone: ends at 300
	m.voice[1].note[0] = { 660, 100 };
	m.voice[1].note[1] = { 0, 100 };
	m.voice[1].note[2] = { 770, 100 };
	m.voice[2].count = 0; // silent voice: no events
	m.voice[3].count = 1; // one long note: ends at 400, the last one
	m.voice[3].note[0] = { 880, 400 };
	Log l = run(m, 5000);
	int tones[SOUNDS_VOICES] = { 0 }, offs[SOUNDS_VOICES] = { 0 },
	    stops = 0;
	for (int i = 0; i < l.n; i++) {
		const int v = l.voice[i];
		if (l.act[i] == SA_TONE)
			tones[v]++;
		else if (l.act[i] == SA_OFF)
			offs[v]++;
		else
			stops++;
		CHECK(v != 2 || l.act[i] == SA_STOP);
	}
	CHECK(tones[0] == 2 && tones[1] == 2 && tones[2] == 0 && tones[3] == 1);
	// voice 0 off at its end (200); voice 1 off at its rest (100) and again not at its end after a tone: 300
	CHECK(offs[0] == 1 && offs[1] == 2 && offs[2] == 0 && offs[3] == 0);
	CHECK(stops == 1);
	// the starts are all at 0, then 100 (voices 0 and 1), 200 (voice 1)
	int k = 0;
	CHECK(l.act[k] == SA_TONE && l.at[k] == 0);
	int starts0 = 0;
	for (int i = 0; i < l.n; i++)
		if (l.act[i] == SA_TONE && l.at[i] == 0)
			starts0++;
	CHECK(starts0 == 3);
	for (int i = 0; i < l.n; i++) {
		if (l.act[i] == SA_TONE && l.voice[i] == 0 && l.freq[i] == 550)
			CHECK(l.at[i] == 100);
		if (l.act[i] == SA_OFF && l.voice[i] == 0)
			CHECK(l.at[i] == 200);
		if (l.act[i] == SA_OFF && l.voice[i] == 1 && l.at[i] == 100)
			CHECK(l.freq[i] == 0);
		if (l.act[i] == SA_TONE && l.voice[i] == 1 && l.freq[i] == 770)
			CHECK(l.at[i] == 200);
		if (l.act[i] == SA_OFF && l.voice[i] == 1 && l.at[i] != 100)
			CHECK(l.at[i] == 300);
	}
	// the zero rumble stop comes with the last voice, not before
	CHECK(l.act[l.n - 1] == SA_STOP && l.voice[l.n - 1] == 3 &&
	      l.at[l.n - 1] == 400);

	// a late loop does not shift the voices: due times count from the start
	SoundPlayer p;
	soundStart(p, m, 0);
	CHECK(soundStep(p, 0).action == SA_TONE);
	CHECK(soundStep(p, 0).action == SA_TONE);
	CHECK(soundStep(p, 0).action == SA_TONE);
	CHECK(soundStep(p, 0).action == SA_NONE);
	// 130 ms late: voice 0 (second note) and voice 1 (its rest) were due at 100, the next ones still at 200
	SoundStep a = soundStep(p, 130);
	SoundStep b = soundStep(p, 130);
	CHECK(a.action == SA_TONE && a.voice == 0 && a.freq == 550);
	CHECK(b.action == SA_OFF && b.voice == 1);
	CHECK(soundStep(p, 130).action == SA_NONE);
	CHECK(!soundDue(p, 199) && soundDue(p, 200));

	// a voice that ends on a rest has nothing left to silence
	Melody r;
	memset(&r, 0, sizeof r);
	r.voice[0].count = 2;
	r.voice[0].note[0] = { 440, 100 };
	r.voice[0].note[1] = { 0, 100 };
	r.voice[1].count = 1;
	r.voice[1].note[0] = { 440, 300 };
	l = run(r, 0);
	CHECK(l.n == 4); // tone, tone, off (rest of voice 0), stop (voice 1)
	CHECK(l.act[2] == SA_OFF && l.voice[2] == 0 && l.at[2] == 100);
	CHECK(l.act[3] == SA_STOP && l.at[3] == 300);

	// one voice only, on voice 3
	Melody o;
	memset(&o, 0, sizeof o);
	o.voice[3].count = 1;
	o.voice[3].note[0] = { 300, 50 };
	l = run(o, 0);
	CHECK(l.n == 2 && l.act[0] == SA_TONE && l.voice[0] == 3 &&
	      l.act[1] == SA_STOP && l.at[1] == 50);
}

// Blob v1 as the older firmware wrote it: one voice per melody.
static size_t makeV1(const SoundsCfg &c, uint8_t *buf)
{
	uint8_t *p = buf;
	*p++ = SOUNDS_MAGIC;
	*p++ = 1;
	*p++ = (uint8_t)(SOUNDS_V1_PAYLOAD_BYTES & 0xFF);
	*p++ = (uint8_t)(SOUNDS_V1_PAYLOAD_BYTES >> 8);
	const uint8_t *payload = p;
	*p++ = c.confirm;
	*p++ = (uint8_t)c.gainDb;
	for (int e = 0; e < SE_COUNT; e++)
		*p++ = c.event[e];
	for (int m = 0; m < SOUNDS_MELODIES; m++) {
		const Voice &d = c.melody[m].voice[0];
		*p++ = d.count;
		for (int k = 0; k < SOUNDS_NOTES; k++) {
			*p++ = (uint8_t)d.note[k].freq;
			*p++ = (uint8_t)(d.note[k].freq >> 8);
			*p++ = (uint8_t)d.note[k].ms;
			*p++ = (uint8_t)(d.note[k].ms >> 8);
		}
	}
	uint16_t s = 0;
	for (const uint8_t *q = payload; q < p; q++)
		s = (uint16_t)(s + *q);
	*p++ = (uint8_t)s;
	*p++ = (uint8_t)(s >> 8);
	return (size_t)(p - buf);
}

static void fixSum(uint8_t *bad, size_t payload)
{
	uint16_t s = 0;
	for (size_t i = 4; i < 4 + payload; i++)
		s = (uint16_t)(s + bad[i]);
	bad[4 + payload] = (uint8_t)s;
	bad[5 + payload] = (uint8_t)(s >> 8);
}

static void testBlob()
{
	CHECK(SOUNDS_BLOB_BYTES == 4144 && SOUNDS_PAYLOAD_BYTES == 4138);
	CHECK(SOUNDS_V1_BLOB_BYTES == 1048);
	SoundsCfg c, d;
	soundsInitDefaults(c);
	static uint8_t buf[SOUNDS_BLOB_BYTES + 8];
	CHECK(soundsSerialize(c, buf, SOUNDS_BLOB_BYTES - 1) == 0);
	CHECK(soundsSerialize(c, buf, sizeof buf) == SOUNDS_BLOB_BYTES);
	CHECK(buf[0] == SOUNDS_MAGIC && buf[1] == 2);
	CHECK((buf[2] | (buf[3] << 8)) == SOUNDS_PAYLOAD_BYTES);

	c.confirm = SC_BOTH;
	c.gainDb = -20;
	c.event[SE_CONNECT] = SOUND_NONE;
	memset(&c.melody[7], 0, sizeof c.melody[7]);
	c.melody[7].voice[0].count = 1;
	c.melody[7].voice[0].note[0] = { 1234, 4321 };
	c.melody[7].voice[3].count = 2;
	c.melody[7].voice[3].note[0] = { 0, 30 };
	c.melody[7].voice[3].note[1] = { 4000, 5000 };
	c.melody[2].voice[2].count = 1;
	c.melody[2].voice[2].note[0] = { 99, 77 };
	// notes past count are not written
	c.melody[2].voice[2].note[5] = { 1, 1 };
	CHECK(soundsSerialize(c, buf, sizeof buf) == SOUNDS_BLOB_BYTES);
	memset(&d, 0, sizeof d);
	CHECK(soundsDeserialize(d, buf, SOUNDS_BLOB_BYTES));
	CHECK(d.confirm == SC_BOTH && d.gainDb == -20);
	CHECK(d.event[SE_CONNECT] == SOUND_NONE && d.event[0] == 0);
	CHECK(d.melody[7].voice[0].count == 1 &&
	      d.melody[7].voice[0].note[0].freq == 1234 &&
	      d.melody[7].voice[0].note[0].ms == 4321);
	CHECK(d.melody[7].voice[3].count == 2 &&
	      d.melody[7].voice[3].note[0].freq == 0 &&
	      d.melody[7].voice[3].note[1].freq == 4000 &&
	      d.melody[7].voice[3].note[1].ms == 5000);
	CHECK(!d.melody[7].voice[1].count && !d.melody[7].voice[2].count);
	CHECK(d.melody[2].voice[2].count == 1 &&
	      d.melody[2].voice[2].note[0].freq == 99 &&
	      d.melody[2].voice[2].note[5].freq == 0);
	CHECK(d.melody[0].voice[0].count == 2 &&
	      d.melody[0].voice[0].note[1].freq == 784 &&
	      d.melody[0].voice[1].count == 2);

	// the chunked reader gives the same bytes as the whole blob
	static uint8_t pieced[SOUNDS_BLOB_BYTES];
	size_t got = 0, n;
	uint8_t chunk[96];
	while ((n = soundsSerializeAt(c, got, chunk, sizeof chunk))) {
		memcpy(pieced + got, chunk, n);
		got += n;
	}
	CHECK(got == SOUNDS_BLOB_BYTES && !memcmp(pieced, buf, got));
	CHECK(soundsSerializeAt(c, SOUNDS_BLOB_BYTES, chunk, 1) == 0);
	CHECK((SOUNDS_BLOB_BYTES + 95) / 96 == 44);

	// wrong length, magic, version, checksum, every field range: rejected whole, target untouched
	SoundsCfg keep = d;
	static uint8_t bad[SOUNDS_BLOB_BYTES];
	CHECK(!soundsDeserialize(d, buf, SOUNDS_BLOB_BYTES - 1));
	CHECK(!soundsDeserialize(d, nullptr, SOUNDS_BLOB_BYTES));
	memcpy(bad, buf, sizeof bad);
	bad[0] ^= 1;
	CHECK(!soundsDeserialize(d, bad, sizeof bad));
	memcpy(bad, buf, sizeof bad);
	bad[1] = 3;
	CHECK(!soundsDeserialize(d, bad, sizeof bad));
	memcpy(bad, buf, sizeof bad);
	bad[1] = 1; // v1 header on a v2 length
	CHECK(!soundsDeserialize(d, bad, sizeof bad));
	memcpy(bad, buf, sizeof bad);
	bad[10] ^= 1; // payload byte, checksum no longer matches
	CHECK(!soundsDeserialize(d, bad, sizeof bad));
	CHECK(!memcmp(&d, &keep, sizeof d));

	const int mel7v3 = 4 + 2 + SE_COUNT + 7 * SOUNDS_MELODY_BYTES +
			   3 * SOUNDS_VOICE_BYTES;
	struct {
		int off;
		uint8_t v;
	} range[] = {
		{ 4, 4 }, // confirm
		{ 5, 0x01 }, // gain +1 dB
		{ 5, 0xC0 }, // gain -64 dB
		{ 6, 8 }, // event -> melody 8
		{ 14, 33 }, // note count of melody 0 voice 0
		// note count of the last voice of the last melody
		{ mel7v3, 33 },
	};
	for (auto &r : range) {
		memcpy(bad, buf, sizeof bad);
		bad[r.off] = r.v;
		// fix the checksum so only the range rule can reject it
		fixSum(bad, SOUNDS_PAYLOAD_BYTES);
		CHECK(!soundsDeserialize(d, bad, sizeof bad));
	}
	CHECK(!memcmp(&d, &keep, sizeof d));
	// note out of range (frequency, zero / too long duration) in the first note of melody 0, and in
	// the second note of the last voice (a bad v2 blob is rejected whole, whichever voice is wrong)
	const int note0 = 4 + 2 + SE_COUNT + 1;
	const int note73 = mel7v3 + 1 + SOUNDS_NOTE_BYTES;
	struct {
		int off;
		uint8_t lo, hi;
	} note[] = { { note0, 0xA1, 0x0F },
		     { note0 + 2, 0, 0 },
		     { note0 + 2, 0x89, 0x13 },
		     { note73 + 2, 0x89, 0x13 },
		     { note73, 0xA1, 0x0F } };
	for (auto &r : note) {
		memcpy(bad, buf, sizeof bad);
		bad[r.off] = r.lo;
		bad[r.off + 1] = r.hi;
		fixSum(bad, SOUNDS_PAYLOAD_BYTES);
		CHECK(!soundsDeserialize(d, bad, sizeof bad));
	}
	CHECK(!memcmp(&d, &keep, sizeof d));

	// load: a bad file gives the defaults, a good one the saved values
	SoundsCfg l;
	CHECK(!soundsLoad(l, bad, sizeof bad));
	CHECK(l.confirm == SC_VIBRATION && l.melody[0].voice[0].count == 2);
	CHECK(!soundsLoad(l, nullptr, 0));
	CHECK(soundsLoad(l, buf, SOUNDS_BLOB_BYTES) && l.confirm == SC_BOTH);
}

static void testBlobV1()
{
	SoundsCfg c, d;
	soundsInitDefaults(c);
	c.confirm = SC_SOUND;
	c.gainDb = -12;
	c.event[SE_CONNECT] = 4;
	memset(&c.melody[5], 0, sizeof c.melody[5]);
	c.melody[5].voice[0].count = 2;
	c.melody[5].voice[0].note[0] = { 500, 40 };
	c.melody[5].voice[0].note[1] = { 0, 5000 };
	uint8_t v1[SOUNDS_V1_BLOB_BYTES + 4];
	CHECK(makeV1(c, v1) == SOUNDS_V1_BLOB_BYTES);

	memset(&d, 0, sizeof d);
	CHECK(soundsDeserialize(d, v1, SOUNDS_V1_BLOB_BYTES));
	CHECK(d.confirm == SC_SOUND && d.gainDb == -12 &&
	      d.event[SE_CONNECT] == 4);
	// the melody lands in voices 0 and 1, voices 2 and 3 stay empty
	for (int m = 0; m < SOUNDS_MELODIES; m++) {
		CHECK(!memcmp(&d.melody[m].voice[0], &c.melody[m].voice[0],
			      sizeof(Voice)));
		CHECK(!memcmp(&d.melody[m].voice[1], &c.melody[m].voice[0],
			      sizeof(Voice)));
		CHECK(!d.melody[m].voice[2].count &&
		      !d.melody[m].voice[3].count);
	}
	CHECK(d.melody[5].voice[1].count == 2 &&
	      d.melody[5].voice[1].note[1].ms == 5000);
	// saving it again gives a v2 blob
	static uint8_t v2[SOUNDS_BLOB_BYTES];
	CHECK(soundsSerialize(d, v2, sizeof v2) == SOUNDS_BLOB_BYTES &&
	      v2[1] == 2);
	CHECK(soundsLoad(d, v1, SOUNDS_V1_BLOB_BYTES));

	// a bad v1 blob is rejected whole
	SoundsCfg keep = d;
	uint8_t bad[sizeof v1];
	memcpy(bad, v1, sizeof bad);
	bad[10] ^= 1; // checksum
	CHECK(!soundsDeserialize(d, bad, SOUNDS_V1_BLOB_BYTES));
	memcpy(bad, v1, sizeof bad);
	bad[14] = 33; // note count
	fixSum(bad, SOUNDS_V1_PAYLOAD_BYTES);
	CHECK(!soundsDeserialize(d, bad, SOUNDS_V1_BLOB_BYTES));
	CHECK(!soundsDeserialize(d, v1, SOUNDS_V1_BLOB_BYTES - 1));
	CHECK(!soundsDeserialize(d, v1, SOUNDS_V1_BLOB_BYTES + 1));
	CHECK(!memcmp(&d, &keep, sizeof d));
}

static void testPreview()
{
	uint8_t b[1 + 2 * SOUNDS_NOTE_BYTES] = { 2, 0xB8, 0x01, 100, 0,
						 0, 0,	  50,	0 };
	Melody m;
	memset(&m, 0, sizeof m);
	CHECK(soundsParseMelody(m, b, sizeof b));
	CHECK(m.voice[0].count == 2 && m.voice[0].note[0].freq == 440 &&
	      m.voice[0].note[0].ms == 100 && m.voice[0].note[1].freq == 0 &&
	      m.voice[0].note[1].ms == 50);
	// a preview is one voice, played on both trackpads
	CHECK(!memcmp(&m.voice[0], &m.voice[1], sizeof(Voice)));
	CHECK(!m.voice[2].count && !m.voice[3].count);
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
	      m.voice[0].note[1].ms == SOUNDS_NOTE_MS_MIN);
	uint8_t z = 0;
	CHECK(soundsParseMelody(m, &z, 1) && soundsMelodyEmpty(m));
}

int main()
{
	testDefaults();
	testModeFamilies();
	testPlayer();
	testVoices();
	testBlob();
	testBlobV1();
	testPreview();
	printf("%d checks, %d failed\n", g_n, g_fail);
	return g_fail ? 1 : 0;
}
