/* test_keypad.c -- does our analog vector for each keypad digit decode back to
 * that digit under the CORE's own arithmetic?
 *
 * The core is a black box at runtime and needs a BIOS to exercise a cartridge,
 * so this test is the real gate on the digit map: core_digit() below is a
 * line-for-line copy of FreeIntv controller.c's right-analog block. If the
 * upstream core changes that math, this test fails and the vectors need redoing.
 *
 * Build: see CMakeLists.txt's SYNCRETRO_TESTS block.
 */
#include "syncretro_keypad.h"
#include "libretro.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int failures;

#define CHECK(cond) \
	do { \
		if (!(cond)) { \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			failures++; \
		} \
	} while (0)

/* FreeIntv controller.c:134-147, verbatim in behavior. */
static const int kd[8] = { 2, 3, 6, 9, 8, 7, 4, 1 };

static int core_digit(int16_t ax, int16_t ay)
{
	int    Rx = ax / 8192;   /* C integer division truncates toward zero */
	int    Ry = ay / 8192;
	double theta;
	int    norm;

	if (Rx == 0 && Ry == 0)
		return 0;
	theta = atan2((double)Ry, (double)Rx) + M_PI;
	if (theta < 0.0)
		theta = 0.0;
	norm = (int)floor((theta / (2 * M_PI)) * 7.0);
	norm -= 1;
	if (norm < 0)
		norm += 8;
	return kd[norm & 0x07];
}

static int decode_held(void)
{
	int16_t x = sr_keypad_analog(RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);
	int16_t y = sr_keypad_analog(RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);

	return core_digit(x, y);
}

int main(void)
{
	static const int digits[8] = { 1, 2, 3, 4, 6, 7, 8, 9 };
	int              i, seen[10];

	memset(seen, 0, sizeof seen);

	/* Nothing held: the stick is centered. */
	sr_keypad_release_all();
	CHECK(decode_held() == 0);

	/* Every analog digit round-trips through the core's own math. */
	for (i = 0; i < 8; i++) {
		sr_keypad_press(digits[i]);
		CHECK(decode_held() == digits[i]);
		seen[digits[i]]++;
		sr_keypad_release(digits[i]);
		CHECK(decode_held() == 0);
	}
	for (i = 0; i < 8; i++)
		CHECK(seen[digits[i]] == 1);   /* collision-free */

	/* 5 and 0 are button bits, not analog: pressing them moves nothing. */
	sr_keypad_release_all();
	sr_keypad_press(5);
	CHECK(decode_held() == 0);
	sr_keypad_press(0);
	CHECK(decode_held() == 0);

	/* The left stick is the disc; the keypad never touches it. */
	sr_keypad_release_all();
	sr_keypad_press(7);
	CHECK(sr_keypad_analog(RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X) == 0);
	CHECK(sr_keypad_analog(RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y) == 0);

	/* Last press wins; releasing a digit that is not held changes nothing. */
	sr_keypad_release_all();
	sr_keypad_press(1);
	sr_keypad_press(9);
	CHECK(decode_held() == 9);
	sr_keypad_release(1);
	CHECK(decode_held() == 9);
	sr_keypad_release(9);
	CHECK(decode_held() == 0);

	/* Byte path: a tap self-releases once its dwell elapses. */
	sr_keypad_release_all();
	sr_keypad_tap(3, 1000);
	CHECK(decode_held() == 3);
	sr_keypad_expire(1100);
	CHECK(decode_held() == 3);   /* still inside the dwell */
	sr_keypad_expire(1300);
	CHECK(decode_held() == 0);   /* dwell elapsed */

	/* An out-of-range digit is ignored, not asserted. */
	sr_keypad_release_all();
	sr_keypad_press(11);
	CHECK(decode_held() == 0);
	sr_keypad_press(-1);
	CHECK(decode_held() == 0);

	/* An out-of-range digit pressed while a valid one is held must not disturb
	 * the held digit -- it is dropped, not "last press wins". */
	sr_keypad_release_all();
	sr_keypad_press(3);
	CHECK(decode_held() == 3);
	sr_keypad_press(5);
	CHECK(decode_held() == 3);
	sr_keypad_press(0);
	CHECK(decode_held() == 3);
	sr_keypad_press(11);
	CHECK(decode_held() == 3);
	sr_keypad_press(-1);
	CHECK(decode_held() == 3);

	printf("%s: %d failure(s)\n", failures ? "FAIL" : "ok", failures);
	return failures != 0;
}
