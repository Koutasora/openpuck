#include "binds.h"
#include <string.h>

BindsCfg g_binds;

static const uint32_t PADDLE_BIT[BINDS_BACKS] = { TB_L4, TB_R4, TB_L5, TB_R5 };

// Per slot and paddle: is a macro playing, which one, at which step, since when.
struct MacroRun {
	bool run;
	uint8_t idx, step;
	uint32_t stepStartMs;
};
static MacroRun g_run[NSLOT][BINDS_BACKS];
static uint32_t g_prevPaddles[NSLOT]; // paddle bits of the previous call (rising-edge detection)

void bindsResetRuntime()
{
	memset(g_run, 0, sizeof g_run);
	memset(g_prevPaddles, 0, sizeof g_prevPaddles);
}

void bindsInitDefaults(BindsCfg &c, const uint8_t (*back)[BINDS_BACKS])
{
	static const uint8_t DEF[BINDS_BACKS] = { 5, 6, 7, 8 };
	memset(&c, 0, sizeof c);
	for (int p = 0; p < BINDS_PROFILES; p++)
		for (int e = 0; e < ET_COUNT; e++)
			for (int i = 0; i < BINDS_BACKS; i++) {
				BindAction &a = c.act[p][e][i];
				a.type = BA_BUTTON;
				a.arg = back ? back[e][i] : DEF[i];
				if (a.arg > BINDS_CODE_MAX)
					a.arg = 0;
			}
}

uint8_t bindsBackCode(const BindsCfg &c, uint8_t etype, uint8_t i)
{
	if (etype >= ET_COUNT || i >= BINDS_BACKS)
		return 0;
	const BindAction &a = c.act[c.active][etype][i];
	return a.type == BA_BUTTON ? a.arg : 0;
}

bool bindsSetAction(BindsCfg &c, uint8_t profile, uint8_t etype, uint8_t i,
		    uint8_t type, uint8_t arg, uint32_t mask)
{
	if (profile >= BINDS_PROFILES || etype >= ET_COUNT || i >= BINDS_BACKS)
		return false;
	BindAction &a = c.act[profile][etype][i];
	switch (type) {
	case BA_BUTTON:
		if (arg > BINDS_CODE_MAX)
			return false;
		a.arg = arg;
		a.mask = 0;
		break;
	case BA_COMBO:
		a.arg = 0;
		a.mask = mask;
		break;
	case BA_MACRO:
		if (arg >= BINDS_MACROS)
			return false;
		a.arg = arg;
		a.mask = 0;
		break;
	default:
		return false;
	}
	a.type = type;
	return true;
}

bool bindsSetMacro(BindsCfg &c, uint8_t idx, const MacroStep *steps, uint8_t n)
{
	if (idx >= BINDS_MACROS || n > BINDS_STEPS || (n && !steps))
		return false;
	MacroDef &m = c.macro[idx];
	memset(&m, 0, sizeof m);
	m.count = n;
	for (int k = 0; k < n; k++)
		m.step[k] = steps[k];
	return true;
}

// Advance a running macro to nowMs; clears run once it has played out. uint32 subtraction keeps it wrap-safe.
static void macroAdvance(MacroRun &r, const MacroDef &m, uint32_t nowMs)
{
	while (r.run) {
		if (r.step >= m.count) {
			r.run = false;
			break;
		}
		uint32_t dur = m.step[r.step].ms;
		if ((uint32_t)(nowMs - r.stepStartMs) < dur)
			break;
		r.stepStartMs += dur;
		r.step++;
	}
}

uint32_t bindsApply(uint8_t slot, uint32_t raw, uint8_t etype, uint32_t nowMs)
{
	if (etype >= ET_COUNT || slot >= NSLOT)
		return raw;
	const uint32_t held = raw & CHORD_BACK4;
	const uint32_t rising = held & ~g_prevPaddles[slot];
	g_prevPaddles[slot] = held;
	const bool chord = held == CHORD_BACK4;

	uint32_t out = raw;
	for (int i = 0; i < BINDS_BACKS; i++) {
		const BindAction &a = g_binds.act[g_binds.active][etype][i];
		MacroRun &r = g_run[slot][i];
		if (a.type == BA_MACRO && !r.run && !chord &&
		    (rising & PADDLE_BIT[i]) && g_binds.macro[a.arg].count) {
			r.run = true;
			r.idx = a.arg;
			r.step = 0;
			r.stepStartMs = nowMs;
		}
		if (r.run) {
			// a started macro finishes even if the paddle was re-bound or released meanwhile
			const MacroDef &m = g_binds.macro[r.idx];
			macroAdvance(r, m, nowMs);
			if (r.run && !chord)
				out |= m.step[r.step].mask;
		}
		if (chord)
			continue;
		if (a.type == BA_COMBO) {
			out &= ~PADDLE_BIT[i];
			if (held & PADDLE_BIT[i])
				out |= a.mask;
		} else if (a.type == BA_MACRO) {
			out &= ~PADDLE_BIT[i];
		}
	}
	return out;
}

// ---- blob ----
static void put32(uint8_t *&p, uint32_t v)
{
	*p++ = (uint8_t)v;
	*p++ = (uint8_t)(v >> 8);
	*p++ = (uint8_t)(v >> 16);
	*p++ = (uint8_t)(v >> 24);
}
static uint32_t get32(const uint8_t *&p)
{
	uint32_t v = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
		     ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
	p += 4;
	return v;
}
static uint16_t sum16(const uint8_t *p, size_t n)
{
	uint16_t s = 0;
	while (n--)
		s = (uint16_t)(s + *p++);
	return s;
}

size_t bindsSerialize(const BindsCfg &c, uint8_t *buf, size_t cap)
{
	if (cap < BINDS_BLOB_BYTES)
		return 0;
	uint8_t *p = buf;
	*p++ = BINDS_MAGIC;
	*p++ = BINDS_VERSION;
	*p++ = (uint8_t)(BINDS_PAYLOAD_BYTES & 0xFF);
	*p++ = (uint8_t)(BINDS_PAYLOAD_BYTES >> 8);
	const uint8_t *payload = p;
	*p++ = c.active;
	for (int a = 0; a < BINDS_PROFILES; a++)
		for (int e = 0; e < ET_COUNT; e++)
			for (int i = 0; i < BINDS_BACKS; i++) {
				const BindAction &x = c.act[a][e][i];
				*p++ = x.type;
				*p++ = x.arg;
				put32(p, x.mask);
			}
	for (int m = 0; m < BINDS_MACROS; m++) {
		*p++ = c.macro[m].flags;
		*p++ = c.macro[m].count;
		for (int k = 0; k < BINDS_STEPS; k++) {
			put32(p, c.macro[m].step[k].mask);
			*p++ = (uint8_t)c.macro[m].step[k].ms;
			*p++ = (uint8_t)(c.macro[m].step[k].ms >> 8);
		}
	}
	uint16_t s = sum16(payload, BINDS_PAYLOAD_BYTES);
	*p++ = (uint8_t)s;
	*p++ = (uint8_t)(s >> 8);
	return BINDS_BLOB_BYTES;
}

bool bindsDeserialize(BindsCfg &c, const uint8_t *buf, size_t len)
{
	if (!buf || len != BINDS_BLOB_BYTES || buf[0] != BINDS_MAGIC ||
	    buf[1] != BINDS_VERSION ||
	    (buf[2] | (buf[3] << 8)) != BINDS_PAYLOAD_BYTES)
		return false;
	const uint8_t *payload = buf + 4;
	if (sum16(payload, BINDS_PAYLOAD_BYTES) !=
	    (uint16_t)(payload[BINDS_PAYLOAD_BYTES] |
		       (payload[BINDS_PAYLOAD_BYTES + 1] << 8)))
		return false;
	BindsCfg t;
	memset(&t, 0, sizeof t);
	const uint8_t *p = payload;
	t.active = *p++;
	if (t.active >= BINDS_PROFILES)
		return false;
	for (int a = 0; a < BINDS_PROFILES; a++)
		for (int e = 0; e < ET_COUNT; e++)
			for (int i = 0; i < BINDS_BACKS; i++) {
				uint8_t type = p[0], arg = p[1];
				p += 2;
				uint32_t mask = get32(p);
				// the setter holds the one set of range rules
				if (!bindsSetAction(t, (uint8_t)a, (uint8_t)e,
						    (uint8_t)i, type, arg,
						    mask))
					return false;
			}
	for (int m = 0; m < BINDS_MACROS; m++) {
		MacroDef &d = t.macro[m];
		d.flags = *p++;
		d.count = *p++;
		if (d.flags || d.count > BINDS_STEPS)
			return false;
		for (int k = 0; k < BINDS_STEPS; k++) {
			d.step[k].mask = get32(p);
			d.step[k].ms = (uint16_t)(p[0] | (p[1] << 8));
			p += 2;
		}
	}
	c = t;
	return true;
}

bool bindsLoad(BindsCfg &c, const uint8_t *buf, size_t len,
	       const uint8_t (*back)[BINDS_BACKS])
{
	if (bindsDeserialize(c, buf, len))
		return true;
	bindsInitDefaults(c, back);
	return false;
}
