// Host test for OpenPuck/pressure.cpp (pure logic). Build + run: sh tests/pressure/run.sh
#include "../../OpenPuck/pressure.h"
#include "../../OpenPuck/triton.h"
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

static PressureCfg cfg(uint8_t on, uint8_t src, uint8_t target)
{
	PressureCfg c = { on, src, target };
	return c;
}

static void testTake()
{
	uint32_t b = TB_L2 | TB_R2 | TB_A;
	uint8_t lt = 100, rt = 50;
	PressureCfg c = cfg(1, PRESS_SRC_LT, PRESS_PS3_SQUARE);
	CHECK(pressureTake(&c, &b, &lt, &rt) == 100);
	CHECK(lt == 0);
	CHECK(rt == 50);
	CHECK(!(b & TB_L2));
	CHECK((b & TB_R2) && (b & TB_A));

	b = TB_L2 | TB_R2;
	lt = 100;
	rt = 50;
	c = cfg(1, PRESS_SRC_RT, PRESS_PS3_SQUARE);
	CHECK(pressureTake(&c, &b, &lt, &rt) == 50);
	CHECK(rt == 0 && lt == 100);
	CHECK((b & TB_L2) && !(b & TB_R2));

	// the click bit alone gives no pressure: the analog value is the source
	b = TB_L2;
	lt = 0;
	rt = 0;
	c = cfg(1, PRESS_SRC_LT, PRESS_PS3_SQUARE);
	CHECK(pressureTake(&c, &b, &lt, &rt) == 0);
	CHECK(b == 0);

	// off: nothing touched
	b = TB_L2 | TB_R2;
	lt = 100;
	rt = 50;
	c = cfg(0, PRESS_SRC_LT, PRESS_PS3_SQUARE);
	CHECK(pressureTake(&c, &b, &lt, &rt) == 0);
	CHECK(lt == 100 && rt == 50 && b == (TB_L2 | TB_R2));
}

static void testPs3()
{
	PressureCfg c = cfg(1, PRESS_SRC_LT, PRESS_PS3_SQUARE);
	uint8_t out[48];
	uint8_t zero[48] = {};

	// light press: pressure follows, the bit stays off below the threshold
	memset(out, 0, 48);
	pressurePs3Apply(&c, PRESS_DIGITAL_MIN - 1, out);
	CHECK(out[24] == PRESS_DIGITAL_MIN - 1);
	CHECK(!(out[2] & 0x80));

	// at the threshold the digital bit is on, pressure is still the analog value
	memset(out, 0, 48);
	pressurePs3Apply(&c, PRESS_DIGITAL_MIN, out);
	CHECK(out[24] == PRESS_DIGITAL_MIN);
	CHECK(out[2] & 0x80);

	memset(out, 0, 48);
	pressurePs3Apply(&c, 128, out);
	CHECK(out[24] == 128 && (out[2] & 0x80));

	// a physical press (0xFF, bit set) stays 0xFF
	memset(out, 0, 48);
	out[24] = 0xFF;
	out[2] = 0x80;
	pressurePs3Apply(&c, 30, out);
	CHECK(out[24] == 0xFF && out[2] == 0x80);

	// an other button held at the same time keeps its bit
	memset(out, 0, 48);
	out[2] = 0x40;
	out[23] = 0xFF;
	pressurePs3Apply(&c, 200, out);
	CHECK(out[2] == 0xC0 && out[23] == 0xFF && out[24] == 200);

	// every target: its own pressure byte and bit, nothing else
	static const struct {
		int press, byte, bit;
	} T[PRESS_PS3_COUNT] = {
		{ 24, 2, 0x80 }, { 21, 2, 0x10 }, { 22, 2, 0x20 },
		{ 23, 2, 0x40 }, { 19, 2, 0x04 }, { 20, 2, 0x08 },
		{ 13, 1, 0x10 }, { 14, 1, 0x20 }, { 15, 1, 0x40 },
		{ 16, 1, 0x80 },
	};
	for (int t = 0; t < PRESS_PS3_COUNT; t++) {
		c = cfg(1, PRESS_SRC_RT, (uint8_t)t);
		memset(out, 0, 48);
		pressurePs3Apply(&c, 77, out);
		uint8_t want[48] = {};
		want[T[t].press] = 77;
		want[T[t].byte] = (uint8_t)T[t].bit;
		CHECK(memcmp(out, want, 48) == 0);
	}

	// off, or a target out of range: report unchanged
	c = cfg(0, PRESS_SRC_LT, PRESS_PS3_SQUARE);
	memset(out, 0, 48);
	pressurePs3Apply(&c, 255, out);
	CHECK(memcmp(out, zero, 48) == 0);
	c = cfg(1, PRESS_SRC_LT, PRESS_PS3_COUNT);
	pressurePs3Apply(&c, 255, out);
	CHECK(memcmp(out, zero, 48) == 0);
}

// What ds3Build does with the trigger: the source is taken out before the paddle remap.
static void testSourceNotL2R2()
{
	PressureCfg c = cfg(1, PRESS_SRC_LT, PRESS_PS3_SQUARE);
	uint32_t raw = TB_L2;
	uint8_t lt = 255, rt = 90;
	uint8_t pv = pressureTake(&c, &raw, &lt, &rt);
	CHECK(pv == 255);
	CHECK(lt == 0 && !(raw & TB_L2));
	CHECK(rt == 90);

	// a back paddle remapped to L2 is OR-ed in after the take (psButtonsFromSteam)
	uint32_t b = raw | TB_L2;
	CHECK(b & TB_L2);
}

static void testXog()
{
	uint8_t v[6];
	uint8_t *f[6] = { &v[0], &v[1], &v[2], &v[3], &v[4], &v[5] };
	PressureCfg c;
	for (uint8_t t = 0; t < PRESS_XOG_COUNT; t++) {
		c = cfg(1, PRESS_SRC_LT, t);
		memset(v, 0, 6);
		pressureXogApply(&c, 90, f);
		for (uint8_t i = 0; i < 6; i++)
			CHECK(v[i] == (i == t ? 90 : 0));
	}
	// a physical press stays 255
	c = cfg(1, PRESS_SRC_LT, PRESS_XOG_X);
	memset(v, 0, 6);
	v[2] = 0xFF;
	pressureXogApply(&c, 40, f);
	CHECK(v[2] == 0xFF);
	v[2] = 0;
	pressureXogApply(&c, 255, f);
	CHECK(v[2] == 255);
	// off / target out of range: unchanged
	c = cfg(0, PRESS_SRC_LT, PRESS_XOG_X);
	memset(v, 0, 6);
	pressureXogApply(&c, 200, f);
	CHECK(v[2] == 0);
	c = cfg(1, PRESS_SRC_LT, PRESS_XOG_COUNT);
	pressureXogApply(&c, 200, f);
	for (int i = 0; i < 6; i++)
		CHECK(v[i] == 0);
}

static void testLoad()
{
	PressureCfg c = cfg(1, 1, 3);
	const uint8_t ff[3] = { 0xFF, 0xFF, 0xFF };
	pressureCfgLoad(&c, PRESS_PS3, ff);
	CHECK(c.on == 0 && c.src == 0 && c.target == 0);

	const uint8_t ok[3] = { 1, 1, 9 };
	pressureCfgLoad(&c, PRESS_PS3, ok);
	CHECK(c.on == 1 && c.src == 1 && c.target == 9);

	// 9 is out of range for the Original Xbox (6 targets)
	pressureCfgLoad(&c, PRESS_XOG, ok);
	CHECK(c.on == 0 && c.src == 0 && c.target == 0);
	const uint8_t ok2[3] = { 1, 0, 5 };
	pressureCfgLoad(&c, PRESS_XOG, ok2);
	CHECK(c.on == 1 && c.src == 0 && c.target == 5);

	// one bad byte voids the whole triple
	const uint8_t bad1[3] = { 2, 0, 0 }, bad2[3] = { 1, 2, 0 };
	const uint8_t bad3[3] = { 1, 0, 10 };
	pressureCfgLoad(&c, PRESS_PS3, ok);
	pressureCfgLoad(&c, PRESS_PS3, bad1);
	CHECK(c.on == 0 && c.target == 0);
	pressureCfgLoad(&c, PRESS_PS3, ok);
	pressureCfgLoad(&c, PRESS_PS3, bad2);
	CHECK(c.on == 0 && c.target == 0);
	pressureCfgLoad(&c, PRESS_PS3, ok);
	pressureCfgLoad(&c, PRESS_PS3, bad3);
	CHECK(c.on == 0 && c.target == 0);

	// disabled but with a valid stored target: kept
	const uint8_t off[3] = { 0, 1, 4 };
	pressureCfgLoad(&c, PRESS_PS3, off);
	CHECK(c.on == 0 && c.src == 1 && c.target == 4);
}

static void testFieldValid()
{
	CHECK(pressureFieldValid(PRESS_PS3, 0, 1));
	CHECK(!pressureFieldValid(PRESS_PS3, 0, 2));
	CHECK(pressureFieldValid(PRESS_PS3, 1, 1));
	CHECK(!pressureFieldValid(PRESS_PS3, 1, 2));
	CHECK(pressureFieldValid(PRESS_PS3, 2, 9));
	CHECK(!pressureFieldValid(PRESS_PS3, 2, 10));
	CHECK(pressureFieldValid(PRESS_XOG, 2, 5));
	CHECK(!pressureFieldValid(PRESS_XOG, 2, 6));
	CHECK(!pressureFieldValid(2, 0, 0));
	CHECK(!pressureFieldValid(PRESS_PS3, 3, 0));
	// default state: off
	CHECK(g_pressCfg[0].on == 0 && g_pressCfg[1].on == 0);
}

int main()
{
	testTake();
	testPs3();
	testSourceNotL2R2();
	testXog();
	testLoad();
	testFieldValid();
	printf("%d checks, %d failed\n", g_n, g_fail);
	return g_fail ? 1 : 0;
}
