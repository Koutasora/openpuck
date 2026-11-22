#include "pressure.h"
#include "triton.h"

PressureCfg g_pressCfg[PRESS_MODES] = {};

bool pressureFieldValid(uint8_t mode, uint8_t k, uint8_t v)
{
	if (mode >= PRESS_MODES)
		return false;
	if (k == 0 || k == 1)
		return v <= 1;
	return k == 2 &&
	       v < (mode == PRESS_PS3 ? PRESS_PS3_COUNT : PRESS_XOG_COUNT);
}

void pressureCfgLoad(PressureCfg *c, uint8_t mode, const uint8_t raw[3])
{
	bool ok = true;
	for (uint8_t k = 0; k < 3; k++)
		ok = ok && pressureFieldValid(mode, k, raw[k]);
	c->on = ok ? raw[0] : 0;
	c->src = ok ? raw[1] : 0;
	c->target = ok ? raw[2] : 0;
}

uint8_t pressureTake(const PressureCfg *c, uint32_t *buttons, uint8_t *lt,
		     uint8_t *rt)
{
	if (!c->on)
		return 0;
	uint8_t pv;
	if (c->src == PRESS_SRC_LT) {
		pv = *lt;
		*lt = 0;
		*buttons &= ~(uint32_t)TB_L2;
	} else {
		pv = *rt;
		*rt = 0;
		*buttons &= ~(uint32_t)TB_R2;
	}
	return pv;
}

void pressurePs3Apply(const PressureCfg *c, uint8_t pv, uint8_t out[48])
{
	// pressure byte offset, digital byte offset and bit, per PRESS_PS3_* target
	static const struct {
		uint8_t press, byte, bit;
	} T[PRESS_PS3_COUNT] = {
		{ 24, 2, 0x80 }, { 21, 2, 0x10 }, { 22, 2, 0x20 },
		{ 23, 2, 0x40 }, { 19, 2, 0x04 }, { 20, 2, 0x08 },
		{ 13, 1, 0x10 }, { 14, 1, 0x20 }, { 15, 1, 0x40 },
		{ 16, 1, 0x80 },
	};
	if (!c->on || c->target >= PRESS_PS3_COUNT)
		return;
	const auto &t = T[c->target];
	if (pv > out[t.press])
		out[t.press] = pv;
	if (pv >= PRESS_DIGITAL_MIN)
		out[t.byte] |= t.bit;
}

void pressureXogApply(const PressureCfg *c, uint8_t pv, uint8_t *const f[6])
{
	if (!c->on || c->target >= PRESS_XOG_COUNT)
		return;
	if (pv > *f[c->target])
		*f[c->target] = pv;
}
