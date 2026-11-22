#include "sounds.h"
#include "config.h" // MODE_*
#include <string.h>

SoundsCfg g_sounds;

static void setMelody(Melody &m, const SoundNote *n, uint8_t count)
{
	memset(&m, 0, sizeof m);
	m.count = count;
	memcpy(m.note, n, count * sizeof n[0]);
}

void soundsInitDefaults(SoundsCfg &c)
{
	memset(&c, 0, sizeof c);
	c.confirm = SC_VIBRATION;
	c.gainDb = SOUNDS_GAIN_DEFAULT;
	for (int i = 0; i < SE_COUNT; i++)
		c.event[i] = (uint8_t)i;
	// Profile N rises through N+1 notes. The rest are short, each family with its own contour.
	static const SoundNote p1[] = { { 523, 100 }, { 784, 150 } };
	static const SoundNote p2[] = { { 523, 90 },
					{ 659, 90 },
					{ 784, 150 } };
	static const SoundNote p3[] = {
		{ 523, 80 }, { 659, 80 }, { 784, 80 }, { 1047, 150 }
	};
	static const SoundNote ps[] = {
		{ 392, 90 }, { 523, 90 }, { 440, 90 }, { 659, 160 }
	};
	static const SoundNote xb[] = {
		{ 330, 120 }, { 0, 40 }, { 440, 120 }, { 587, 200 }
	};
	static const SoundNote sw[] = {
		{ 494, 80 }, { 587, 80 }, { 440, 80 }, { 698, 160 }
	};
	static const SoundNote ot[] = { { 349, 100 },
					{ 440, 100 },
					{ 349, 180 } };
	static const SoundNote cn[] = { { 294, 90 },
					{ 440, 90 },
					{ 587, 200 } };
	setMelody(c.melody[SE_PROFILE1], p1, 2);
	setMelody(c.melody[SE_PROFILE2], p2, 3);
	setMelody(c.melody[SE_PROFILE3], p3, 4);
	setMelody(c.melody[SE_MODE_PLAYSTATION], ps, 4);
	setMelody(c.melody[SE_MODE_XBOX], xb, 4);
	setMelody(c.melody[SE_MODE_SWITCH], sw, 4);
	setMelody(c.melody[SE_MODE_OTHER], ot, 3);
	setMelody(c.melody[SE_CONNECT], cn, 3);
}

uint8_t soundsModeEvent(uint8_t usbMode)
{
	switch (usbMode) {
	case MODE_PS5:
	case MODE_HIDGYRO:
	case MODE_PS5_GAME:
	case MODE_DS4_GAME:
	case MODE_PS3:
	case MODE_PS5_AUTH:
		return SE_MODE_PLAYSTATION;
	case MODE_XBOX:
	case MODE_XBOX_OG:
		return SE_MODE_XBOX;
	case MODE_SW_HORI:
	case MODE_SW_PRO:
		return SE_MODE_SWITCH;
	default:
		return SE_MODE_OTHER;
	}
}

const Melody *soundsFor(const SoundsCfg &c, uint8_t event)
{
	if (event >= SE_COUNT || c.event[event] >= SOUNDS_MELODIES)
		return nullptr;
	const Melody &m = c.melody[c.event[event]];
	return m.count ? &m : nullptr;
}

bool soundsMelodyValid(const Melody &m)
{
	if (m.count > SOUNDS_NOTES)
		return false;
	for (int i = 0; i < m.count; i++)
		if (m.note[i].freq > SOUNDS_FREQ_MAX || !m.note[i].ms ||
		    m.note[i].ms > SOUNDS_NOTE_MS_MAX)
			return false;
	return true;
}

void soundStart(SoundPlayer &p, const Melody &m, uint32_t nowMs)
{
	p.melody = m;
	p.next = 0;
	p.dueMs = nowMs;
	p.playing = m.count > 0;
	p.ending = false;
}

bool soundPlaying(const SoundPlayer &p)
{
	return p.playing;
}

SoundStep soundStep(SoundPlayer &p, uint32_t nowMs)
{
	SoundStep s = { SA_NONE, 0, 0 };
	if (!p.playing || (int32_t)(nowMs - p.dueMs) < 0)
		return s;
	if (p.ending) {
		p.playing = false;
		s.action = SA_STOP;
		return s;
	}
	const SoundNote &n = p.melody.note[p.next++];
	p.dueMs = nowMs + n.ms;
	if (p.next >= p.melody.count)
		p.ending = true;
	if (n.freq) {
		s.action = SA_TONE;
		s.freq = n.freq;
		s.ms = n.ms;
	} else {
		s.action = SA_STOP;
	}
	return s;
}

static uint16_t sum16(const uint8_t *p, size_t n)
{
	uint16_t s = 0;
	while (n--)
		s = (uint16_t)(s + *p++);
	return s;
}

size_t soundsSerialize(const SoundsCfg &c, uint8_t *buf, size_t cap)
{
	if (cap < SOUNDS_BLOB_BYTES)
		return 0;
	uint8_t *p = buf;
	*p++ = SOUNDS_MAGIC;
	*p++ = SOUNDS_VERSION;
	*p++ = (uint8_t)(SOUNDS_PAYLOAD_BYTES & 0xFF);
	*p++ = (uint8_t)(SOUNDS_PAYLOAD_BYTES >> 8);
	const uint8_t *payload = p;
	*p++ = c.confirm;
	*p++ = (uint8_t)c.gainDb;
	for (int e = 0; e < SE_COUNT; e++)
		*p++ = c.event[e];
	for (int m = 0; m < SOUNDS_MELODIES; m++) {
		*p++ = c.melody[m].count;
		for (int k = 0; k < SOUNDS_NOTES; k++) {
			*p++ = (uint8_t)c.melody[m].note[k].freq;
			*p++ = (uint8_t)(c.melody[m].note[k].freq >> 8);
			*p++ = (uint8_t)c.melody[m].note[k].ms;
			*p++ = (uint8_t)(c.melody[m].note[k].ms >> 8);
		}
	}
	const uint16_t s = sum16(payload, SOUNDS_PAYLOAD_BYTES);
	*p++ = (uint8_t)s;
	*p++ = (uint8_t)(s >> 8);
	return SOUNDS_BLOB_BYTES;
}

bool soundsDeserialize(SoundsCfg &c, const uint8_t *buf, size_t len)
{
	if (!buf || len != SOUNDS_BLOB_BYTES || buf[0] != SOUNDS_MAGIC ||
	    buf[1] != SOUNDS_VERSION ||
	    (buf[2] | (buf[3] << 8)) != SOUNDS_PAYLOAD_BYTES)
		return false;
	const uint8_t *payload = buf + 4;
	if (sum16(payload, SOUNDS_PAYLOAD_BYTES) !=
	    (uint16_t)(payload[SOUNDS_PAYLOAD_BYTES] |
		       (payload[SOUNDS_PAYLOAD_BYTES + 1] << 8)))
		return false;
	SoundsCfg t;
	memset(&t, 0, sizeof t);
	const uint8_t *p = payload;
	t.confirm = *p++;
	t.gainDb = (int8_t)*p++;
	if (t.confirm > SC_BOTH || t.gainDb < SOUNDS_GAIN_MIN || t.gainDb > 0)
		return false;
	for (int e = 0; e < SE_COUNT; e++) {
		t.event[e] = *p++;
		if (t.event[e] >= SOUNDS_MELODIES && t.event[e] != SOUND_NONE)
			return false;
	}
	for (int m = 0; m < SOUNDS_MELODIES; m++) {
		Melody &d = t.melody[m];
		d.count = *p++;
		if (d.count > SOUNDS_NOTES)
			return false;
		for (int k = 0; k < SOUNDS_NOTES; k++) {
			SoundNote n = { (uint16_t)(p[0] | (p[1] << 8)),
					(uint16_t)(p[2] | (p[3] << 8)) };
			p += SOUNDS_NOTE_BYTES;
			if (k < d.count)
				d.note[k] = n;
		}
		if (!soundsMelodyValid(d))
			return false;
	}
	c = t;
	return true;
}

bool soundsLoad(SoundsCfg &c, const uint8_t *buf, size_t len)
{
	if (soundsDeserialize(c, buf, len))
		return true;
	soundsInitDefaults(c);
	return false;
}

bool soundsParseMelody(Melody &m, const uint8_t *buf, size_t len)
{
	if (!buf || len < 1 || buf[0] > SOUNDS_NOTES ||
	    len != 1u + (size_t)buf[0] * SOUNDS_NOTE_BYTES)
		return false;
	Melody t;
	memset(&t, 0, sizeof t);
	t.count = buf[0];
	for (int k = 0; k < t.count; k++) {
		const uint8_t *p = buf + 1 + k * SOUNDS_NOTE_BYTES;
		t.note[k].freq = (uint16_t)(p[0] | (p[1] << 8));
		t.note[k].ms = (uint16_t)(p[2] | (p[3] << 8));
	}
	if (!soundsMelodyValid(t))
		return false;
	m = t;
	return true;
}
