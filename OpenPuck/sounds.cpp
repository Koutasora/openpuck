#include "sounds.h"
#include "config.h" // MODE_*
#include <string.h>

SoundsCfg g_sounds;

// The motifs sound on both trackpads, like a one-voice preview.
static void setMelody(Melody &m, const SoundNote *n, uint8_t count)
{
	memset(&m, 0, sizeof m);
	m.voice[0].count = count;
	memcpy(m.voice[0].note, n, count * sizeof n[0]);
	m.voice[1] = m.voice[0];
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

uint8_t soundsSide(uint8_t voice)
{
	// side 2 (both trackpads) is skipped: voices 0 and 1 already cover them
	static const uint8_t side[SOUNDS_VOICES] = { 0, 1, 3, 4 };
	return side[voice % SOUNDS_VOICES];
}

bool soundsMelodyEmpty(const Melody &m)
{
	for (int v = 0; v < SOUNDS_VOICES; v++)
		if (m.voice[v].count)
			return false;
	return true;
}

const Melody *soundsFor(const SoundsCfg &c, uint8_t event)
{
	if (event >= SE_COUNT || c.event[event] >= SOUNDS_MELODIES)
		return nullptr;
	const Melody &m = c.melody[c.event[event]];
	return soundsMelodyEmpty(m) ? nullptr : &m;
}

static bool voiceValid(const Voice &d)
{
	if (d.count > SOUNDS_NOTES)
		return false;
	for (int i = 0; i < d.count; i++)
		if (d.note[i].freq > SOUNDS_FREQ_MAX ||
		    d.note[i].ms < SOUNDS_NOTE_MS_MIN ||
		    d.note[i].ms > SOUNDS_NOTE_MS_MAX)
			return false;
	return true;
}

bool soundsMelodyValid(const Melody &m)
{
	for (int v = 0; v < SOUNDS_VOICES; v++)
		if (!voiceValid(m.voice[v]))
			return false;
	return true;
}

void soundStart(SoundPlayer &p, const Melody &m, uint32_t nowMs)
{
	p.melody = m;
	p.playing = false;
	for (int v = 0; v < SOUNDS_VOICES; v++) {
		p.next[v] = 0;
		p.dueMs[v] = nowMs;
		p.done[v] = m.voice[v].count == 0;
		p.silent[v] = true;
		if (!p.done[v])
			p.playing = true;
	}
}

bool soundPlaying(const SoundPlayer &p)
{
	return p.playing;
}

// The most overdue unfinished voice, or -1 when none is due.
static int dueVoice(const SoundPlayer &p, uint32_t nowMs)
{
	int best = -1;
	int32_t late = -1;
	for (int v = 0; v < SOUNDS_VOICES; v++) {
		const int32_t d = (int32_t)(nowMs - p.dueMs[v]);
		if (!p.done[v] && d >= 0 && d > late) {
			best = v;
			late = d;
		}
	}
	return p.playing ? best : -1;
}

bool soundDue(const SoundPlayer &p, uint32_t nowMs)
{
	return dueVoice(p, nowMs) >= 0;
}

SoundStep soundStep(SoundPlayer &p, uint32_t nowMs)
{
	SoundStep s = { SA_NONE, 0, 0, 0 };
	for (;;) {
		const int v = dueVoice(p, nowMs);
		if (v < 0)
			return s;
		Voice &d = p.melody.voice[v];
		s.voice = (uint8_t)v;
		if (p.next[v] < d.count) {
			const SoundNote &n = d.note[p.next[v]++];
			p.dueMs[v] += n.ms;
			p.silent[v] = !n.freq;
			s.action = n.freq ? SA_TONE : SA_OFF;
			s.freq = n.freq;
			s.ms = n.ms;
			return s;
		}
		// the last note has run out
		p.done[v] = true;
		p.playing = false;
		for (int i = 0; i < SOUNDS_VOICES; i++)
			if (!p.done[i])
				p.playing = true;
		if (!p.playing) {
			s.action = SA_STOP;
			return s;
		}
		// a voice that ended on a rest has nothing left to silence
		if (!p.silent[v]) {
			s.action = SA_OFF;
			return s;
		}
	}
}

static uint16_t sum16(const uint8_t *p, size_t n)
{
	uint16_t s = 0;
	while (n--)
		s = (uint16_t)(s + *p++);
	return s;
}

static uint8_t blobByte(const SoundsCfg &c, size_t i)
{
	if (i == 0)
		return SOUNDS_MAGIC;
	if (i == 1)
		return SOUNDS_VERSION;
	if (i == 2)
		return (uint8_t)(SOUNDS_PAYLOAD_BYTES & 0xFF);
	if (i == 3)
		return (uint8_t)(SOUNDS_PAYLOAD_BYTES >> 8);
	i -= 4;
	if (i == 0)
		return c.confirm;
	if (i == 1)
		return (uint8_t)c.gainDb;
	if (i < 2 + SE_COUNT)
		return c.event[i - 2];
	i -= 2 + SE_COUNT;
	const Voice &d =
		c.melody[i / SOUNDS_MELODY_BYTES]
			.voice[i % SOUNDS_MELODY_BYTES / SOUNDS_VOICE_BYTES];
	i %= SOUNDS_VOICE_BYTES;
	if (i == 0)
		return d.count;
	i--;
	const size_t k = i / SOUNDS_NOTE_BYTES;
	if (k >= d.count)
		return 0;
	const uint16_t f = i % SOUNDS_NOTE_BYTES < 2 ? d.note[k].freq :
						       d.note[k].ms;
	return (uint8_t)(i % 2 ? f >> 8 : f);
}

size_t soundsSerializeAt(const SoundsCfg &c, size_t off, uint8_t *out, size_t n)
{
	if (off >= SOUNDS_BLOB_BYTES)
		return 0;
	if (n > SOUNDS_BLOB_BYTES - off)
		n = SOUNDS_BLOB_BYTES - off;
	uint16_t sum = 0;
	// the checksum is at the end and needs the whole payload
	const size_t sumAt = 4 + SOUNDS_PAYLOAD_BYTES;
	if (off + n > sumAt)
		for (size_t i = 4; i < sumAt; i++)
			sum = (uint16_t)(sum + blobByte(c, i));
	for (size_t i = 0; i < n; i++) {
		const size_t at = off + i;
		out[i] = at < sumAt ? blobByte(c, at) :
				      (uint8_t)(sum >> (8 * (at - sumAt)));
	}
	return n;
}

size_t soundsSerialize(const SoundsCfg &c, uint8_t *buf, size_t cap)
{
	if (cap < SOUNDS_BLOB_BYTES)
		return 0;
	return soundsSerializeAt(c, 0, buf, SOUNDS_BLOB_BYTES);
}

// Reads one voice from the blob and range-checks it. d == nullptr only checks: the blob is validated in
// place first and decoded after, so no second copy of the config is ever on the small loop stack.
static bool readVoice(Voice *d, const uint8_t *&p, bool padded)
{
	const uint8_t count = *p++;
	if (count > SOUNDS_NOTES)
		return false;
	bool ok = true;
	// only count notes are read: a preview holds no padding
	for (int k = 0; k < count; k++) {
		const uint8_t *q = p + k * SOUNDS_NOTE_BYTES;
		const SoundNote n = { (uint16_t)(q[0] | (q[1] << 8)),
				      (uint16_t)(q[2] | (q[3] << 8)) };
		if (n.freq > SOUNDS_FREQ_MAX || n.ms < SOUNDS_NOTE_MS_MIN ||
		    n.ms > SOUNDS_NOTE_MS_MAX)
			ok = false;
		if (d)
			d->note[k] = n;
	}
	// a stored voice always takes SOUNDS_NOTES slots, a preview only its notes
	p += (padded ? SOUNDS_NOTES : count) * SOUNDS_NOTE_BYTES;
	if (d)
		d->count = count;
	return ok;
}

// Walks the payload; with c == nullptr it validates only.
static bool readPayload(SoundsCfg *c, const uint8_t *p, bool v1)
{
	const uint8_t confirm = *p++;
	const int8_t gainDb = (int8_t)*p++;
	if (confirm > SC_BOTH || gainDb < SOUNDS_GAIN_MIN || gainDb > 0)
		return false;
	for (int e = 0; e < SE_COUNT; e++) {
		if (p[e] >= SOUNDS_MELODIES && p[e] != SOUND_NONE)
			return false;
		if (c)
			c->event[e] = p[e];
	}
	p += SE_COUNT;
	if (c) {
		c->confirm = confirm;
		c->gainDb = gainDb;
	}
	for (int m = 0; m < SOUNDS_MELODIES; m++) {
		Melody *d = c ? &c->melody[m] : nullptr;
		if (v1) {
			if (!readVoice(d ? &d->voice[0] : nullptr, p, true))
				return false;
			if (d)
				d->voice[1] = d->voice[0];
			continue;
		}
		for (int v = 0; v < SOUNDS_VOICES; v++)
			if (!readVoice(d ? &d->voice[v] : nullptr, p, true))
				return false;
	}
	return true;
}

bool soundsDeserialize(SoundsCfg &c, const uint8_t *buf, size_t len)
{
	if (!buf || len < 4 || buf[0] != SOUNDS_MAGIC)
		return false;
	const bool v1 = buf[1] == 1;
	const size_t payloadLen = v1 ? SOUNDS_V1_PAYLOAD_BYTES :
				       SOUNDS_PAYLOAD_BYTES;
	if ((buf[1] != SOUNDS_VERSION && !v1) || len != 4 + payloadLen + 2 ||
	    (buf[2] | (buf[3] << 8)) != (int)payloadLen)
		return false;
	const uint8_t *payload = buf + 4;
	if (sum16(payload, payloadLen) !=
	    (uint16_t)(payload[payloadLen] | (payload[payloadLen + 1] << 8)))
		return false;
	if (!readPayload(nullptr, payload, v1))
		return false;
	memset(&c, 0, sizeof c);
	return readPayload(&c, payload, v1);
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
	// the count byte and the notes are laid out as in a blob voice
	const uint8_t *p = buf;
	if (!readVoice(nullptr, p, false))
		return false;
	memset(&m, 0, sizeof m);
	p = buf;
	readVoice(&m.voice[0], p, false);
	m.voice[1] = m.voice[0];
	return true;
}
