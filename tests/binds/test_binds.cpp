// Host test for OpenPuck/binds.cpp (pure logic). Build + run: sh tests/binds/run.sh
#include "../../OpenPuck/binds.h"
#include <stdio.h>
#include <string.h>

static int g_fail = 0, g_n = 0;
#define CHECK(c)                                                               \
	do {                                                                   \
		g_n++;                                                         \
		if (!(c)) {                                                    \
			g_fail++;                                              \
			printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c);    \
		}                                                              \
	} while (0)

static void fresh()
{
	bindsInitDefaults(g_binds, nullptr);
	bindsResetRuntime();
}

static void testButtonPassthrough()
{
	fresh();
	uint32_t in = TB_L4 | TB_A;
	CHECK(bindsApply(0, in, ET_XBOX, 100) == in);
	CHECK(bindsBackCode(g_binds, ET_XBOX, 0) == 5);
	CHECK(bindsBackCode(g_binds, ET_DS5, 3) == 8);
}

static void testNone()
{
	fresh();
	bindsSetAction(g_binds, 0, ET_XBOX, 0, BA_COMBO, 0, TB_A | TB_B);
	uint32_t in = TB_L4 | TB_X;
	CHECK(bindsApply(0, in, ET_NONE, 0) == in);
	CHECK(bindsApply(0, in, ET_COUNT, 0) == in);
}

static void testCombo()
{
	fresh();
	CHECK(bindsSetAction(g_binds, 0, ET_XBOX, 1, BA_COMBO, 0,
			     TB_LB | TB_RB | TB_A));
	uint32_t out = bindsApply(0, TB_R4 | TB_Y, ET_XBOX, 0);
	CHECK(out == (TB_LB | TB_RB | TB_A | TB_Y));
	CHECK(!(out & TB_R4));
	// released: nothing left
	CHECK(bindsApply(0, TB_Y, ET_XBOX, 4) == TB_Y);
	// g_back must read "none" for a combo
	CHECK(bindsBackCode(g_binds, ET_XBOX, 1) == 0);
	// other type keeps its BUTTON default
	CHECK(bindsApply(0, TB_R4, ET_DS5, 8) == TB_R4);
}

static void testComboTriggersAndVirtual()
{
	fresh();
	bindsSetAction(g_binds, 0, ET_DS5, 0, BA_COMBO, 0, TB_L2 | TB_TOUCH | TB_MUTE);
	CHECK(bindsApply(0, TB_L4, ET_DS5, 0) == (TB_L2 | TB_TOUCH | TB_MUTE));
}

static void testMacro()
{
	fresh();
	MacroStep s[3] = { { TB_A, 50 }, { 0, 30 }, { TB_B | TB_X, 40 } };
	CHECK(bindsSetMacro(g_binds, 2, s, 3));
	CHECK(bindsSetAction(g_binds, 0, ET_SWITCH, 2, BA_MACRO, 2, 0));
	uint32_t t = 1000;
	// press at t=1000
	CHECK(bindsApply(1, TB_L5, ET_SWITCH, t) == TB_A);
	// release does not abort
	CHECK(bindsApply(1, 0, ET_SWITCH, t + 49) == TB_A);
	CHECK(bindsApply(1, 0, ET_SWITCH, t + 50) == 0); // pause step
	CHECK(bindsApply(1, 0, ET_SWITCH, t + 79) == 0);
	CHECK(bindsApply(1, 0, ET_SWITCH, t + 80) == (TB_B | TB_X));
	CHECK(bindsApply(1, 0, ET_SWITCH, t + 119) == (TB_B | TB_X));
	CHECK(bindsApply(1, 0, ET_SWITCH, t + 120) == 0); // finished
	CHECK(bindsApply(1, 0, ET_SWITCH, t + 500) == 0);
	// physical paddle bit is never leaked
	CHECK(bindsApply(1, TB_L5, ET_SWITCH, t + 1000) == TB_A);
	// re-press while running is ignored (does not restart)
	CHECK(bindsApply(1, 0, ET_SWITCH, t + 1010) == TB_A);
	CHECK(bindsApply(1, TB_L5, ET_SWITCH, t + 1020) == TB_A);
	CHECK(bindsApply(1, 0, ET_SWITCH, t + 1030) == TB_A);
	// step 1 of the ORIGINAL start
	CHECK(bindsApply(1, 0, ET_SWITCH, t + 1050) == 0);
	// another slot is independent
	CHECK(bindsApply(0, 0, ET_SWITCH, t + 1060) == 0);
}

static void testMacroWrapAndUndefined()
{
	fresh();
	MacroStep s[1] = { { TB_Y, 100 } };
	bindsSetMacro(g_binds, 0, s, 1);
	bindsSetAction(g_binds, 0, ET_XBOX, 0, BA_MACRO, 0, 0);
	bindsSetAction(g_binds, 0, ET_XBOX, 1, BA_MACRO, 5, 0); // macro 5 empty
	uint32_t t = 0xFFFFFFF0u; // millis() rollover mid-macro
	CHECK(bindsApply(0, TB_L4, ET_XBOX, t) == TB_Y);
	CHECK(bindsApply(0, 0, ET_XBOX, t + 50) == TB_Y);
	CHECK(bindsApply(0, 0, ET_XBOX, t + 100) == 0);
	// empty macro: paddle swallowed, nothing output
	CHECK(bindsApply(0, TB_R4, ET_XBOX, 5000) == 0);
}

static void testChordUntouched()
{
	fresh();
	bindsSetAction(g_binds, 0, ET_XBOX, 0, BA_COMBO, 0, TB_A);
	MacroStep s[1] = { { TB_B, 100 } };
	bindsSetMacro(g_binds, 0, s, 1);
	bindsSetAction(g_binds, 0, ET_XBOX, 1, BA_MACRO, 0, 0);
	uint32_t in = CHORD_BACK4 | TB_RB | TB_A;
	CHECK(bindsApply(0, in, ET_XBOX, 0) == in);
	// holding one paddle after the chord must not start the macro (no rising edge)
	CHECK(bindsApply(0, TB_R4, ET_XBOX, 10) == 0);
	CHECK(bindsApply(0, 0, ET_XBOX, 200) == 0);
}

static void testSerialization()
{
	fresh();
	g_binds.active = 2;
	bindsSetAction(g_binds, 1, ET_DS5, 3, BA_COMBO, 0, TB_LB | TB_RB);
	bindsSetAction(g_binds, 2, ET_XBOX, 0, BA_MACRO, 7, 0);
	MacroStep s[2] = { { TB_A, 1234 }, { 0, 65535 } };
	bindsSetMacro(g_binds, 7, s, 2);
	uint8_t buf[BINDS_BLOB_BYTES];
	CHECK(bindsSerialize(g_binds, buf, sizeof buf) == BINDS_BLOB_BYTES);
	CHECK(bindsSerialize(g_binds, buf, sizeof buf - 1) == 0);
	BindsCfg back;
	memset(&back, 0x55, sizeof back);
	CHECK(bindsDeserialize(back, buf, sizeof buf));
	CHECK(back.active == 2);
	CHECK(back.act[1][ET_DS5][3].type == BA_COMBO &&
	      back.act[1][ET_DS5][3].mask == (TB_LB | TB_RB));
	CHECK(back.act[2][ET_XBOX][0].type == BA_MACRO && back.act[2][ET_XBOX][0].arg == 7);
	CHECK(back.macro[7].count == 2 && back.macro[7].step[0].ms == 1234 &&
	      back.macro[7].step[1].ms == 65535 && back.macro[7].step[0].mask == TB_A);
	uint8_t buf2[BINDS_BLOB_BYTES];
	CHECK(bindsSerialize(back, buf2, sizeof buf2) == BINDS_BLOB_BYTES);
	CHECK(memcmp(buf, buf2, sizeof buf) == 0);
}

static void testCorrupt()
{
	fresh();
	uint8_t buf[BINDS_BLOB_BYTES];
	bindsSerialize(g_binds, buf, sizeof buf);
	static const uint8_t DEFBACK[ET_COUNT][BINDS_BACKS] = {
		{ 1, 2, 3, 4 }, { 5, 6, 7, 8 }, { 9, 10, 11, 12 }, { 13, 14, 15, 16 }
	};
	BindsCfg c;
	// every kind of damage -> defaults built from the supplied table
	uint8_t bad[BINDS_BLOB_BYTES];
	for (int kind = 0; kind < 6; kind++) {
		memcpy(bad, buf, sizeof buf);
		size_t len = sizeof bad;
		switch (kind) {
		case 0: bad[0] ^= 0xFF; break; // magic
		case 1: bad[1] = 99; break; // version
		case 2: bad[3] ^= 1; break; // length field
		case 3: bad[40] ^= 0x10; break; // payload bit flip -> checksum
		case 4: len -= 1; break; // truncated
		case 5: // out-of-range value with a fixed-up checksum
			bad[4 + 1 + 0] = 9; // type 9
			{
				uint16_t s = 0;
				for (int i = 0; i < BINDS_PAYLOAD_BYTES; i++)
					s = (uint16_t)(s + bad[4 + i]);
				bad[4 + BINDS_PAYLOAD_BYTES] = (uint8_t)s;
				bad[4 + BINDS_PAYLOAD_BYTES + 1] = (uint8_t)(s >> 8);
			}
			break;
		}
		memset(&c, 0x77, sizeof c);
		CHECK(!bindsLoad(c, bad, len, DEFBACK));
		CHECK(c.active == 0 && c.act[2][ET_DS5][3].type == BA_BUTTON &&
		      c.act[2][ET_DS5][3].arg == 16 && c.act[0][ET_XBOX][0].arg == 1);
		CHECK(c.macro[0].count == 0);
	}
	CHECK(!bindsLoad(c, nullptr, 0, nullptr)); // missing file
	CHECK(c.act[0][ET_XBOX][0].arg == 5);
	// strict variant leaves the target untouched
	memset(&c, 0x77, sizeof c);
	CHECK(!bindsDeserialize(c, bad, 3));
	CHECK(c.active == 0x77);
}

static void testKey()
{
	fresh();
	BindsKbm k;
	CHECK(bindsSetAction(g_binds, 0, ET_DS5, 0, BA_KEY, 0x06,
			     KM_LCTRL | KM_RSHIFT)); // Ctrl+Shift+C
	CHECK(bindsSetAction(g_binds, 0, ET_DS5, 1, BA_KEY, 0x2C, 0)); // space
	uint32_t out = bindsApply(0, TB_L4 | TB_A, ET_DS5, 10, &k);
	CHECK(out == TB_A); // paddle bit is gone
	CHECK(k.mods == (KM_LCTRL | KM_RSHIFT) && k.keys[0] == 0x06 &&
	      k.keys[1] == 0 && k.buttons == 0 && k.wheel == 0);
	out = bindsApply(0, TB_L4 | TB_R4, ET_DS5, 14, &k);
	CHECK(out == 0);
	CHECK(k.keys[0] == 0x06 && k.keys[1] == 0x2C);
	// release: all clear
	bindsApply(0, 0, ET_DS5, 18, &k);
	CHECK(k.mods == 0 && k.keys[0] == 0 && k.keys[1] == 0);
	// a modifier key as the key itself
	CHECK(bindsSetAction(g_binds, 0, ET_DS5, 2, BA_KEY, 0xE1, 0));
	bindsApply(0, TB_L5, ET_DS5, 22, &k);
	CHECK(k.keys[0] == 0xE1 && k.mods == 0);
	// other slots do not see it
	BindsKbm k2;
	bindsApply(1, 0, ET_DS5, 22, &k2);
	CHECK(k2.keys[0] == 0 && k2.mods == 0);
	// ranges: usage 0..3 and above 0xE7, mask above 8 bits
	CHECK(!bindsSetAction(g_binds, 0, ET_DS5, 0, BA_KEY, 0x03, 0));
	CHECK(!bindsSetAction(g_binds, 0, ET_DS5, 0, BA_KEY, 0xE8, 0));
	CHECK(!bindsSetAction(g_binds, 0, ET_DS5, 0, BA_KEY, 0x04, 0x100));
	// unchanged by the failures
	CHECK(g_binds.act[0][ET_DS5][0].arg == 0x06);
	CHECK(bindsBackCode(g_binds, ET_DS5, 0) == 0);
	// ET_NONE: no kbm, buttons untouched
	CHECK(bindsApply(0, TB_L4, ET_NONE, 30, &k) == TB_L4);
	CHECK(k.keys[0] == 0 && k.mods == 0);
}

static void testKeyDuplicates()
{
	fresh();
	BindsKbm k;
	for (int i = 0; i < BINDS_BACKS; i++)
		CHECK(bindsSetAction(g_binds, 0, ET_DS4, (uint8_t)i, BA_KEY, 0x04,
				     KM_LALT));
	bindsApply(0, CHORD_BACK4 & ~TB_R5, ET_DS4, 0, &k);
	CHECK(k.keys[0] == 0x04 && k.keys[1] == 0 && k.mods == KM_LALT);
}

static void testMouseButtons()
{
	fresh();
	BindsKbm k;
	CHECK(bindsSetAction(g_binds, 0, ET_DS5, 0, BA_MOUSE, BMO_LEFT, 0));
	CHECK(bindsSetAction(g_binds, 0, ET_DS5, 1, BA_MOUSE, BMO_RIGHT, 0));
	CHECK(bindsSetAction(g_binds, 0, ET_DS5, 2, BA_MOUSE, BMO_BACK, 0));
	CHECK(bindsSetAction(g_binds, 0, ET_DS5, 3, BA_MOUSE, BMO_FORWARD, 0));
	uint32_t out = bindsApply(0, TB_L4 | TB_L5, ET_DS5, 5, &k);
	CHECK(out == 0);
	CHECK(k.buttons == (1u | 8u) && k.wheel == 0);
	bindsApply(0, TB_R4 | TB_R5, ET_DS5, 9, &k);
	CHECK(k.buttons == (2u | 16u));
	bindsApply(0, 0, ET_DS5, 13, &k);
	CHECK(k.buttons == 0);
	CHECK(bindsSetAction(g_binds, 0, ET_DS5, 0, BA_MOUSE, BMO_MIDDLE, 0));
	bindsApply(0, TB_L4, ET_DS5, 17, &k);
	CHECK(k.buttons == 4u);
	CHECK(!bindsSetAction(g_binds, 0, ET_DS5, 0, BA_MOUSE, 0, 0));
	CHECK(!bindsSetAction(g_binds, 0, ET_DS5, 0, BA_MOUSE, BMO_MAX + 1, 0));
}

static void testWheel()
{
	fresh();
	BindsKbm k;
	CHECK(bindsSetAction(g_binds, 0, ET_DS5, 0, BA_MOUSE, BMO_WHEEL_UP, 0));
	CHECK(bindsSetAction(g_binds, 0, ET_DS5, 1, BA_MOUSE, BMO_WHEEL_DOWN, 0));
	// one notch on the press, none while held, one more per new press
	bindsApply(0, TB_L4, ET_DS5, 1000, &k);
	CHECK(k.wheel == 1);
	bindsApply(0, TB_L4, ET_DS5, 1004, &k);
	CHECK(k.wheel == 0);
	bindsApply(0, TB_L4, ET_DS5, 1500, &k);
	CHECK(k.wheel == 0);
	bindsApply(0, 0, ET_DS5, 1504, &k);
	bindsApply(0, TB_R4, ET_DS5, 1508, &k);
	CHECK(k.wheel == -1);
	bindsApply(0, 0, ET_DS5, 1512, &k);
	bindsApply(0, TB_L4, ET_DS5, 1516, &k);
	CHECK(k.wheel == 1);
	// both paddles at once cancel out within one call
	bindsApply(0, 0, ET_DS5, 1520, &k);
	bindsApply(0, TB_L4 | TB_R4, ET_DS5, 1524, &k);
	CHECK(k.wheel == 0);
}

static void testWheelRepeat()
{
	fresh();
	BindsKbm k;
	CHECK(bindsSetAction(g_binds, 0, ET_DS5, 0, BA_MOUSE, BMO_WHEEL_DOWN_REP,
			     0));
	bindsApply(0, TB_L4, ET_DS5, 2000, &k);
	CHECK(k.wheel == -1);
	bindsApply(0, TB_L4, ET_DS5, 2050, &k);
	CHECK(k.wheel == 0);
	bindsApply(0, TB_L4, ET_DS5, 2100, &k);
	CHECK(k.wheel == -1);
	bindsApply(0, TB_L4, ET_DS5, 2150, &k);
	CHECK(k.wheel == 0);
	// a gap in the reports gives one notch, not a burst
	bindsApply(0, TB_L4, ET_DS5, 3000, &k);
	CHECK(k.wheel == -1);
	bindsApply(0, TB_L4, ET_DS5, 3004, &k);
	CHECK(k.wheel == 0);
	// release stops it
	bindsApply(0, 0, ET_DS5, 3100, &k);
	CHECK(k.wheel == 0);
	bindsApply(0, 0, ET_DS5, 3300, &k);
	CHECK(k.wheel == 0);
	// wrap-safe timer
	bindsApply(0, TB_L4, ET_DS5, 0xFFFFFFC0u, &k);
	CHECK(k.wheel == -1);
	bindsApply(0, TB_L4, ET_DS5, 0xFFFFFFE0u, &k);
	CHECK(k.wheel == 0);
	bindsApply(0, TB_L4, ET_DS5, 0x40u, &k);
	CHECK(k.wheel == -1);
}

static void testKbmChordAndPlainCalls()
{
	fresh();
	BindsKbm k;
	CHECK(bindsSetAction(g_binds, 0, ET_DS5, 0, BA_KEY, 0x04, 0));
	CHECK(bindsSetAction(g_binds, 0, ET_DS5, 1, BA_MOUSE, BMO_LEFT, 0));
	// all four paddles = mode / profile chord: raw untouched, nothing for the host
	CHECK(bindsApply(0, CHORD_BACK4, ET_DS5, 0, &k) == CHORD_BACK4);
	CHECK(k.keys[0] == 0 && k.buttons == 0);
	// the old four-argument call still works and still clears the paddle
	bindsApply(0, 0, ET_DS5, 4);
	CHECK(bindsApply(0, TB_L4 | TB_R4, ET_DS5, 8) == 0);
}

static void testMigrationV1()
{
	fresh();
	CHECK(bindsSetAction(g_binds, 1, ET_SWITCH, 2, BA_COMBO, 0, TB_A | TB_B));
	CHECK(bindsSetAction(g_binds, 2, ET_XBOX, 3, BA_BUTTON, 14, 0));
	const MacroStep s[2] = { { TB_A, 30 }, { 0, 40 } };
	CHECK(bindsSetMacro(g_binds, 3, s, 2));
	CHECK(bindsSetAction(g_binds, 1, ET_XBOX, 0, BA_MACRO, 3, 0));
	static uint8_t blob[BINDS_BLOB_BYTES];
	CHECK(bindsSerialize(g_binds, blob, sizeof blob) == BINDS_BLOB_BYTES);
	CHECK(blob[1] == 2);
	// a version 1 blob has the same layout and checksum, only the version byte differs
	uint8_t v1[BINDS_BLOB_BYTES];
	memcpy(v1, blob, sizeof v1);
	v1[1] = 1;
	BindsCfg c;
	memset(&c, 0, sizeof c);
	CHECK(bindsDeserialize(c, v1, sizeof v1));
	CHECK(c.act[1][ET_SWITCH][2].type == BA_COMBO &&
	      c.act[1][ET_SWITCH][2].mask == (TB_A | TB_B));
	CHECK(c.act[2][ET_XBOX][3].type == BA_BUTTON && c.act[2][ET_XBOX][3].arg == 14);
	CHECK(c.act[1][ET_XBOX][0].type == BA_MACRO && c.macro[3].count == 2);
	// re-saved, it is a version 2 blob with identical content
	uint8_t again[BINDS_BLOB_BYTES];
	CHECK(bindsSerialize(c, again, sizeof again) == BINDS_BLOB_BYTES);
	CHECK(again[1] == 2 && memcmp(again + 2, blob + 2, BINDS_BLOB_BYTES - 2) == 0);
	// version 1 cannot hold KEY / MOUSE, version 3 does not exist
	CHECK(bindsSetAction(g_binds, 0, ET_DS5, 0, BA_KEY, 0x04, 0));
	bindsSerialize(g_binds, blob, sizeof blob);
	memcpy(v1, blob, sizeof v1);
	v1[1] = 1;
	CHECK(!bindsDeserialize(c, v1, sizeof v1));
	CHECK(bindsDeserialize(c, blob, sizeof blob));
	blob[1] = 3;
	CHECK(!bindsDeserialize(c, blob, sizeof blob));
}

static void testSerializeKeyMouse()
{
	fresh();
	CHECK(bindsSetAction(g_binds, 2, ET_DS4, 1, BA_KEY, 0x52,
			     KM_LGUI | KM_RALT));
	CHECK(bindsSetAction(g_binds, 0, ET_DS5, 3, BA_MOUSE, BMO_WHEEL_UP_REP, 0));
	static uint8_t blob[BINDS_BLOB_BYTES];
	CHECK(bindsSerialize(g_binds, blob, sizeof blob) == BINDS_BLOB_BYTES);
	BindsCfg c;
	memset(&c, 0, sizeof c);
	CHECK(bindsDeserialize(c, blob, sizeof blob));
	CHECK(c.act[2][ET_DS4][1].type == BA_KEY && c.act[2][ET_DS4][1].arg == 0x52 &&
	      c.act[2][ET_DS4][1].mask == (KM_LGUI | KM_RALT));
	CHECK(c.act[0][ET_DS5][3].type == BA_MOUSE &&
	      c.act[0][ET_DS5][3].arg == BMO_WHEEL_UP_REP);
	// a damaged KEY (mask above 8 bits, fixed-up checksum) is rejected whole
	const int off = 4 + 1 + ((2 * ET_COUNT + ET_DS4) * BINDS_BACKS + 1) * 6;
	blob[off + 3] = 1;
	uint16_t s = 0;
	for (int i = 0; i < BINDS_PAYLOAD_BYTES; i++)
		s = (uint16_t)(s + blob[4 + i]);
	blob[4 + BINDS_PAYLOAD_BYTES] = (uint8_t)s;
	blob[4 + BINDS_PAYLOAD_BYTES + 1] = (uint8_t)(s >> 8);
	CHECK(!bindsDeserialize(c, blob, sizeof blob));
}

int main()
{
	testButtonPassthrough();
	testNone();
	testCombo();
	testComboTriggersAndVirtual();
	testMacro();
	testMacroWrapAndUndefined();
	testChordUntouched();
	testSerialization();
	testCorrupt();
	testKey();
	testKeyDuplicates();
	testMouseButtons();
	testWheel();
	testWheelRepeat();
	testKbmChordAndPlainCalls();
	testMigrationV1();
	testSerializeKeyMouse();
	printf("%d checks, %d failed\n", g_n, g_fail);
	return g_fail ? 1 : 0;
}
