// pressure.h -- analog trigger -> pressure of one button, for the PS3 (DS3) and Original Xbox modes.
//
// Both report an analog pressure byte per face button, and some games use it (MGS2: a light press on Square
// aims, a full press fires). A trigger chosen here drives that byte of a chosen button instead of acting as
// L2/R2. Pure logic, no Arduino, so tests/pressure builds it on the host; the two modes only call it.
#pragma once
#include <stdint.h>

#define PRESS_PS3 0
#define PRESS_XOG 1
#define PRESS_MODES 2

// Source trigger.
#define PRESS_SRC_LT 0
#define PRESS_SRC_RT 1

// PS3 targets (output buttons, after the A/B swap: Square is Square on the host).
#define PRESS_PS3_SQUARE 0
#define PRESS_PS3_TRIANGLE 1
#define PRESS_PS3_CIRCLE 2
#define PRESS_PS3_CROSS 3
#define PRESS_PS3_L1 4
#define PRESS_PS3_R1 5
#define PRESS_PS3_UP 6
#define PRESS_PS3_RIGHT 7
#define PRESS_PS3_DOWN 8
#define PRESS_PS3_LEFT 9
#define PRESS_PS3_COUNT 10

// Original Xbox targets, in the order of the report fields passed to pressureXogApply.
#define PRESS_XOG_A 0
#define PRESS_XOG_B 1
#define PRESS_XOG_X 2
#define PRESS_XOG_Y 3
#define PRESS_XOG_WHITE 4
#define PRESS_XOG_BLACK 5
#define PRESS_XOG_COUNT 6

// The PS3 digital bit of the target comes on from this pressure. Low, so that a light press already holds
// the button (a game reading only the bit sees it), but above the noise of the trigger at rest.
#define PRESS_DIGITAL_MIN 10

struct PressureCfg {
	uint8_t on; // 0/1
	uint8_t src; // PRESS_SRC_*
	uint8_t target; // PRESS_PS3_* / PRESS_XOG_*
};
extern PressureCfg g_pressCfg[PRESS_MODES];

// in-range check for one field: k 0 = on, 1 = src, 2 = target
bool pressureFieldValid(uint8_t mode, uint8_t k, uint8_t v);
// Loads a stored triple. 0xFF (a cfg.bin that predates the setting) or any out-of-range value gives the
// default: off.
void pressureCfgLoad(PressureCfg *c, uint8_t mode, const uint8_t raw[3]);

// Takes the source trigger out of the L2/R2 path: returns its analog value (0 if the setting is off), zeroes
// that trigger in *lt / *rt and drops its click bit from *buttons (TB_*, before any back-paddle remap, so a
// paddle remapped to L2/R2 still works). Setting off: returns 0 and changes nothing.
uint8_t pressureTake(const PressureCfg *c, uint32_t *buttons, uint8_t *lt,
		     uint8_t *rt);

// DS3 input payload (48 B, ds3Build): raises the target's pressure byte to at least pv and sets its digital bit
// from PRESS_DIGITAL_MIN. A physical press (0xFF) stays 0xFF. Setting off: no change.
void pressurePs3Apply(const PressureCfg *c, uint8_t pv, uint8_t out[48]);

// Original Xbox: f = {a, b, x, y, white, black} pressure bytes of the report.
void pressureXogApply(const PressureCfg *c, uint8_t pv, uint8_t *const f[6]);
