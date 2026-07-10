/* test_binds.c -- the binding table is the single source of truth for both the
 * key handler and the help screen. These assertions pin the bindings that
 * M2_INPUT.md sec 3 promises the player. */
#include "syncretro_binds.h"
#include "libretro.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond) \
	do { \
		if (!(cond)) { \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			failures++; \
		} \
	} while (0)

static void check_act(int c, sr_act_t want_act, int want_id)
{
	int      id  = -12345;
	sr_act_t act = sr_bind_lookup(c, &id);

	if (act != want_act || id != want_id) {
		printf("FAIL lookup(0x%02x): act=%d id=%d, want act=%d id=%d\n",
		       c, (int)act, id, (int)want_act, want_id);
		failures++;
	}
}

int main(void)
{
	const char *key, *desc;
	int         i, lines = 0;

	/* Case folding is ALPHA-ONLY. The old `c | 0x20` mangled '\r' into '-' and
	 * '\t' into ')', so Enter and Tab never reached the pad at all. */
	CHECK(sr_bind_fold('A') == 'a');
	CHECK(sr_bind_fold('Z') == 'z');
	CHECK(sr_bind_fold('a') == 'a');
	CHECK(sr_bind_fold('\r') == '\r');
	CHECK(sr_bind_fold('\t') == '\t');
	CHECK(sr_bind_fold('1') == '1');
	CHECK(sr_bind_fold('?') == '?');
	CHECK(sr_bind_fold(0x08) == 0x08);

	/* Disc. */
	check_act('w', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_UP);
	check_act('a', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_LEFT);
	check_act('s', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_DOWN);
	check_act('d', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_RIGHT);

	/* Action buttons. */
	check_act('z', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_A);
	check_act('x', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_B);
	check_act('c', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_Y);

	/* Analog keypad digits: id is the digit itself. */
	check_act('1', SR_ACT_DIGIT, 1);
	check_act('4', SR_ACT_DIGIT, 4);
	check_act('9', SR_ACT_DIGIT, 9);

	/* ...but 5 and 0 are button bits, never digits. */
	check_act('5', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_R3);
	check_act('0', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_L3);

	/* Clear / Enter. Both Backspace encodings must work. */
	check_act(0x08, SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_L2);
	check_act(0x7f, SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_L2);
	check_act('\r', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_R2);
	check_act('\n', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_R2);

	/* Controller swap. */
	check_act('\t', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_SELECT);

	/* Door actions. Help is '?' -- NOT Ctrl-H, which IS 0x08 (Backspace). */
	check_act(' ', SR_ACT_DOOR, SR_DOOR_PAUSE);
	check_act('?', SR_ACT_DOOR, SR_DOOR_HELP);
	check_act(0x12, SR_ACT_DOOR, SR_DOOR_RESET);   /* Ctrl-R */
	check_act(0x11, SR_ACT_DOOR, SR_DOOR_QUIT);    /* Ctrl-Q */

	/* The core's mini-keypad and its framebuffer pause/help are unreachable:
	 * nothing may bind L, R, X or START. */
	for (i = 1; i < 128; i++) {
		int      id  = -1;
		sr_act_t act = sr_bind_lookup(i, &id);

		if (act != SR_ACT_PAD)
			continue;
		CHECK(id != RETRO_DEVICE_ID_JOYPAD_L);
		CHECK(id != RETRO_DEVICE_ID_JOYPAD_R);
		CHECK(id != RETRO_DEVICE_ID_JOYPAD_X);
		CHECK(id != RETRO_DEVICE_ID_JOYPAD_START);
	}

	/* Unbound keys stay unbound. */
	check_act('q', SR_ACT_NONE, 0);   /* freed: used to trip the mini-keypad */
	check_act('e', SR_ACT_NONE, 0);   /* freed: likewise */
	check_act('v', SR_ACT_NONE, 0);
	check_act(0, SR_ACT_NONE, 0);

	/* Help lines exist, are non-empty, and terminate. */
	for (i = 0; sr_bind_help_line(i, &key, &desc); i++) {
		CHECK(key != NULL && key[0] != '\0');
		CHECK(desc != NULL && desc[0] != '\0');
		lines++;
	}
	CHECK(lines >= 8);
	CHECK(sr_bind_help_line(lines, &key, &desc) == 0);

	printf("%s: %d failure(s)\n", failures ? "FAIL" : "ok", failures);
	return failures != 0;
}
