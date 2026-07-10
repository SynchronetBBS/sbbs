/* syncretro_keypad.c -- see syncretro_keypad.h. */
#include "syncretro_keypad.h"
#include "libretro.h"    /* RETRO_DEVICE_INDEX_ANALOG_* / RETRO_DEVICE_ID_ANALOG_* */

/* Byte-path dwell. Same value and rationale as syncretro_input.c's SR_HOLD_MS:
 * long enough that a tap registers for a frame or three at 60 fps. */
#define SR_KP_HOLD_MS 250

/* Unit vector per digit, in RetroPad axis space (Y is DOWN-positive). This is
 * the physical keypad grid: 1 2 3 across the top, 7 8 9 across the bottom.
 * Verified against the core's own arithmetic by test_keypad.c. Slots 0 and 5
 * are unused -- those keys are button bits (L3 / R3). */
static const signed char g_vec[10][2] = {
	{  0,  0 },   /* 0: L3, not analog */
	{ -1, -1 },   /* 1 */
	{  0, -1 },   /* 2 */
	{ +1, -1 },   /* 3 */
	{ -1,  0 },   /* 4 */
	{  0,  0 },   /* 5: R3, not analog */
	{ +1,  0 },   /* 6 */
	{ -1, +1 },   /* 7 */
	{  0, +1 },   /* 8 */
	{ +1, +1 }    /* 9 */
};

static int               g_digit; /* the held analog digit, or 0 for none */
static uint32_t          g_expire_ms; /* byte path only; 0 = no timer */

static int sr_kp_is_analog(int digit)
{
	return digit >= 1 && digit <= 9 && digit != 5;
}

void sr_keypad_press(int digit)
{
	if (!sr_kp_is_analog(digit))
		return;
	g_digit     = digit;   /* last press wins */
	g_expire_ms = 0;
}

void sr_keypad_release(int digit)
{
	/* Releasing a digit that is not the one held must change nothing: the evdev
	 * path can interleave down/up edges of two digits. */
	if (!sr_kp_is_analog(digit) || g_digit != digit)
		return;
	g_digit     = 0;
	g_expire_ms = 0;
}

void sr_keypad_tap(int digit, uint32_t now_ms)
{
	if (!sr_kp_is_analog(digit))
		return;
	g_digit     = digit;
	g_expire_ms = now_ms + SR_KP_HOLD_MS;
	if (g_expire_ms == 0)
		g_expire_ms = 1;   /* 0 is the "no timer" sentinel */
}

void sr_keypad_expire(uint32_t now_ms)
{
	if (g_expire_ms != 0 && (int32_t)(now_ms - g_expire_ms) >= 0) {
		g_digit     = 0;
		g_expire_ms = 0;
	}
}

void sr_keypad_release_all(void)
{
	g_digit     = 0;
	g_expire_ms = 0;
}

int16_t sr_keypad_analog(unsigned index, unsigned id)
{
	if (index != RETRO_DEVICE_INDEX_ANALOG_RIGHT || g_digit == 0)
		return 0;
	if (id == RETRO_DEVICE_ID_ANALOG_X)
		return (int16_t)(g_vec[g_digit][0] * SR_KP_AXIS);
	if (id == RETRO_DEVICE_ID_ANALOG_Y)
		return (int16_t)(g_vec[g_digit][1] * SR_KP_AXIS);
	return 0;
}
