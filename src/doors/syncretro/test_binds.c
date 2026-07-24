/* test_binds.c -- the binding table is the single source of truth for both the
 * key handler and the help screen. These assertions pin the bindings that
 * M2_INPUT.md sec 3 promises the player. */
#include "syncretro_binds.h"
#include "syncretro_profile.h"
#include "syncretro_games.h"
#include "libretro.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void write_arcade_fixture(void)
{
	FILE *f = fopen("games.ini", "w");

	fputs("[bzone]\n"
	      "name     = Battlezone\n"
	      "button.Y = Fire\n"
	      "stick2   = Right tread\n", f);
	fclose(f);
}

#define CHECK(cond) \
		do { \
			if (!(cond)) { \
				printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
				failures++; \
			} \
		} while (0)

static void check_act(int c, sr_act_t want_act, int want_id, int want_port)
{
	int      id   = -12345;
	int      port = -12345;
	sr_act_t act  = sr_bind_lookup(c, &id, &port);

	if (act != want_act || id != want_id || port != want_port) {
		printf("FAIL lookup(0x%02x): act=%d id=%d port=%d, want act=%d id=%d port=%d\n",
		       c, (int)act, id, port, (int)want_act, want_id, want_port);
		failures++;
	}
}

int main(void)
{
	const char *key, *desc;
	int         i, lines = 0;

	/* --- profile selection ---------------------------------------------------
	 * The lobby's -profile wins; a bare run infers from the core's own name; an
	 * unknown -profile warns and falls through to inference rather than dying. */
	sr_profile_select("pad", NULL);
	CHECK(sr_profile() == SR_PROFILE_PAD);
	sr_profile_select("intv", NULL);
	CHECK(sr_profile() == SR_PROFILE_INTV);

	/* THE casing trap: FreeIntv reports "freeintv", lower-case -- not the
	 * "FreeIntv" spelling used in its repo, RetroArch's core list and our docs.
	 * A case-sensitive compare would drop the Intellivision to `pad` SILENTLY,
	 * losing its keypad with no error anywhere. */
	sr_profile_select(NULL, "freeintv");
	CHECK(sr_profile() == SR_PROFILE_INTV);
	sr_profile_select(NULL, "FreeIntv");
	CHECK(sr_profile() == SR_PROFILE_INTV);

	sr_profile_select(NULL, "FCEUmm");
	CHECK(sr_profile() == SR_PROFILE_PAD);        /* the NES is a gamepad */
	sr_profile_select(NULL, NULL);
	CHECK(sr_profile() == SR_PROFILE_PAD);        /* an unknown core plays anyway */
	sr_profile_select(NULL, "SomeCoreWeHaveNeverSeen");
	CHECK(sr_profile() == SR_PROFILE_PAD);
	sr_profile_select("nonsense", "freeintv");    /* typo: infer, do not die */
	CHECK(sr_profile() == SR_PROFILE_INTV);

	/* The Intellivision alone reads the analog stick, and alone gives the arrows
	 * to a SECOND controller. */
	sr_profile_select("intv", NULL);
	CHECK(sr_profile_analog() == 1);
	CHECK(sr_profile_arrow_port() == 1);
	sr_profile_select("pad", NULL);
	CHECK(sr_profile_analog() == 0);
	CHECK(sr_profile_arrow_port() == 0);          /* a solo player's own d-pad */

	/* --- the Intellivision's table (the rest of this file) ------------------- */
	sr_profile_select("intv", NULL);

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

	/* Player 1 (controller 0): disc. */
	check_act('w', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_UP,    0);
	check_act('a', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_LEFT,  0);
	check_act('s', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_DOWN,  0);
	check_act('d', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_RIGHT, 0);

	/* Player 1 action buttons. */
	check_act('z', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_A, 0);
	check_act('x', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_B, 0);
	check_act('c', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_Y, 0);

	/* Player 2 (controller 1) action buttons: , . / */
	check_act(',', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_A, 1);
	check_act('.', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_B, 1);
	check_act('/', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_Y, 1);

	/* Player 1 analog keypad digits: id is the digit itself. */
	check_act('1', SR_ACT_DIGIT, 1, 0);
	check_act('4', SR_ACT_DIGIT, 4, 0);
	check_act('9', SR_ACT_DIGIT, 9, 0);

	/* ...but 5 and 0 are button bits, never digits. */
	check_act('5', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_R3, 0);
	check_act('0', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_L3, 0);

	/* Clear / Enter. Both Backspace encodings must work. */
	check_act(0x08, SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_L2, 0);
	check_act(0x7f, SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_L2, 0);
	check_act('\r', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_R2, 0);
	check_act('\n', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_R2, 0);

	/* Controller swap -- its own action now (was SELECT sent to the core). */
	check_act('\t', SR_ACT_SWAP, 0, 0);

	/* Door actions. Help is '?' -- NOT Ctrl-H, which IS 0x08 (Backspace). */
	check_act(' ', SR_ACT_DOOR, SR_DOOR_PAUSE, 0);
	check_act('?', SR_ACT_DOOR, SR_DOOR_HELP,  0);
	check_act(0x13, SR_ACT_DOOR, SR_DOOR_STATS, 0);   /* Ctrl-S */
	check_act(0x12, SR_ACT_DOOR, SR_DOOR_RESET, 0);   /* Ctrl-R */
	check_act(0x11, SR_ACT_DOOR, SR_DOOR_QUIT,  0);   /* Ctrl-Q */

	/* The core's mini-keypad and its framebuffer pause/help are unreachable:
	 * nothing may bind L, R, X or START. */
	for (i = 1; i < 128; i++) {
		int      id   = -1;
		int      port = -1;
		sr_act_t act  = sr_bind_lookup(i, &id, &port);

		if (act != SR_ACT_PAD)
			continue;
		CHECK(port == 0 || port == 1);
		CHECK(id != RETRO_DEVICE_ID_JOYPAD_L);
		CHECK(id != RETRO_DEVICE_ID_JOYPAD_R);
		CHECK(id != RETRO_DEVICE_ID_JOYPAD_X);
		CHECK(id != RETRO_DEVICE_ID_JOYPAD_START);
	}

	/* Unbound keys stay unbound. */
	check_act('q', SR_ACT_NONE, 0, 0);   /* freed: used to trip the mini-keypad */
	check_act('e', SR_ACT_NONE, 0, 0);   /* freed: likewise */
	check_act('v', SR_ACT_NONE, 0, 0);
	check_act(0, SR_ACT_NONE, 0, 0);

	/* Help lines exist, are non-empty, and terminate. */
	for (i = 0; sr_bind_help_line(i, &key, &desc); i++) {
		CHECK(key != NULL && key[0] != '\0');
		CHECK(desc != NULL && desc[0] != '\0');
		lines++;
	}
	CHECK(lines >= 8);
	CHECK(sr_bind_help_line(lines, &key, &desc) == 0);

	/* --- the gamepad's table -------------------------------------------------
	 * The whole NES: d-pad, B, A, Select, Start. Z=B and X=A come from fceumm's
	 * own SET_INPUT_DESCRIPTORS (id 0 = "B", id 8 = "A"), not from a guess. */
	sr_profile_select("pad", NULL);

	check_act('w', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_UP,     0);
	check_act('a', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_LEFT,   0);
	check_act('s', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_DOWN,   0);
	check_act('d', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_RIGHT,  0);
	check_act('z', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_B,      0);
	check_act('x', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_A,      0);
	check_act('\r', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_START,  0);
	check_act('\n', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_START,  0);
	check_act(0x08, SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_SELECT, 0);
	check_act(0x7f, SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_SELECT, 0);

	/* Case folding still applies to the pad table. */
	check_act(sr_bind_fold('X'), SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_A, 0);

	/* The door keys are the SAME on every console: a player who learns Ctrl-Q on
	 * one does not have to relearn it on the next. */
	check_act(' ',  SR_ACT_DOOR, SR_DOOR_PAUSE, 0);
	check_act('?',  SR_ACT_DOOR, SR_DOOR_HELP,  0);
	check_act(0x11, SR_ACT_DOOR, SR_DOOR_QUIT,  0);
	check_act(0x12, SR_ACT_DOOR, SR_DOOR_RESET, 0);
	check_act(0x13, SR_ACT_DOOR, SR_DOOR_STATS, 0);
	check_act('\t', SR_ACT_SWAP, 0, 0);

	/* NO keypad digits on a gamepad: the number row is UNBOUND, and must not
	 * reach the core as a phantom analog deflection. */
	check_act('1', SR_ACT_NONE, 0, 0);
	check_act('5', SR_ACT_NONE, 0, 0);
	check_act('9', SR_ACT_NONE, 0, 0);
	check_act('0', SR_ACT_NONE, 0, 0);

	/* The Intellivision's player-2 buttons are not bound here either. */
	check_act(',', SR_ACT_NONE, 0, 0);
	check_act('.', SR_ACT_NONE, 0, 0);

	/* The help screen switches WITH the table -- one table drives both, so they
	 * cannot drift. The pad's help must not mention a keypad. */
	lines = 0;
	for (i = 0; sr_bind_help_line(i, &key, &desc); i++) {
		CHECK(key != NULL && desc != NULL);
		CHECK(strstr(desc, "keypad") == NULL);
		lines++;
	}
	CHECK(lines > 0);

	/* --- the arcade cabinet's table ------------------------------------------
	 * Same wiring as `pad` for the stick and buttons -- what differs is the two
	 * that start a game (a cabinet does nothing until a coin goes in) and the
	 * SECOND STICK, which a keyboard can only reach as an analog deflection. */
	sr_profile_select("arcade", NULL);
	CHECK(sr_profile() == SR_PROFILE_ARCADE);
	CHECK(sr_profile_analog() == 0);   /* the disc/keypad flag stays OFF here */

	check_act('w', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_UP,     0);
	check_act('s', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_DOWN,   0);
	check_act('z', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_B,      0);
	check_act(0x08, SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_SELECT, 0);   /* INSERT COIN */
	check_act('\r', SR_ACT_PAD, RETRO_DEVICE_ID_JOYPAD_START,  0);

	/* The second stick. MAME 2003-Plus puts a twin-stick cabinet's other stick
	 * on the RetroPad's right stick and NOWHERE else, so these two keys are the
	 * only way a terminal player reaches it -- without them Battlezone's right
	 * tread is dead and the tank can pivot but never drive. */
	check_act('i', SR_ACT_AXIS, SR_AXIS_RIGHT_Y_NEG, 0);
	check_act('k', SR_ACT_AXIS, SR_AXIS_RIGHT_Y_POS, 0);

	/* An axis id is NOT a button id. It rides the pad state array in slots above
	 * the last RetroPad id, and sr_input_state() refuses to send anything that
	 * high as a button -- if these ever collided, pressing I would press R3. */
	CHECK(SR_AXIS_RIGHT_Y_NEG > RETRO_DEVICE_ID_JOYPAD_R3);
	CHECK(SR_AXIS_RIGHT_Y_POS > RETRO_DEVICE_ID_JOYPAD_R3);
	CHECK(SR_AXIS_RIGHT_Y_NEG != SR_AXIS_RIGHT_Y_POS);

	/* Nothing else in the cabinet table may resolve to an axis: two keys, and
	 * the six button keys must stay buttons. */
	for (i = 1; i < 128; i++) {
		int      id   = -1;
		int      port = -1;
		sr_act_t act  = sr_bind_lookup(i, &id, &port);

		if (act != SR_ACT_AXIS)
			continue;
		CHECK(i == 'i' || i == 'k');
		CHECK(port == 0);
	}

	/* The second stick is the CABINET's. A cartridge console must not grow two
	 * phantom keys: on those profiles the core reads a centred stick, and I / K
	 * stay unbound exactly as they were. */
	sr_profile_select("pad", NULL);
	check_act('i', SR_ACT_NONE, 0, 0);
	check_act('k', SR_ACT_NONE, 0, 0);
	sr_profile_select("intv", NULL);
	check_act('i', SR_ACT_NONE, 0, 0);
	check_act('k', SR_ACT_NONE, 0, 0);

	/* --- per-cabinet help labels ---------------------------------------------
	 * With no cabinet loaded the button rows stay GROUPED, exactly as they are
	 * for every game whose controls nobody has measured. */
	sr_profile_select("arcade", NULL);
	sr_games_load("/nonexistent-directory", NULL);
	lines = 0;
	for (i = 0; sr_bind_help_line(i, &key, &desc); i++) {
		if (strcmp(key, "Z X") == 0)
			lines++;
		CHECK(strcmp(key, "I K") != 0);   /* no second stick: no I/K line */
	}
	CHECK(lines == 1);                    /* the grouped row is present */

	/* A labelled cabinet: one line per key, the label as its description, and
	 * every unlabelled button GONE -- on Battlezone they do nothing, and listing
	 * them is the confusion this file removes. */
	write_arcade_fixture();               /* [bzone] button.Y = Fire, stick2 */
	sr_games_load(".", "bzone.zip");
	{
		int saw_fire = 0, saw_stick2 = 0, saw_group = 0, saw_dead = 0;

		for (i = 0; sr_bind_help_line(i, &key, &desc); i++) {
			if (strcmp(key, "C") == 0 && strcmp(desc, "Fire") == 0)
				saw_fire = 1;
			if (strcmp(key, "I K") == 0 && strcmp(desc, "Right tread") == 0)
				saw_stick2 = 1;
			if (strcmp(key, "Z X") == 0 || strcmp(key, "C V") == 0)
				saw_group = 1;
			if (strcmp(key, "Z") == 0 || strcmp(key, "V") == 0)
				saw_dead = 1;
		}
		CHECK(saw_fire);
		CHECK(saw_stick2);
		CHECK(!saw_group);   /* grouping is off once anything is labelled */
		CHECK(!saw_dead);    /* unlabelled buttons are omitted, not renumbered */
	}
	remove("games.ini");

	printf("%s: %d failure(s)\n", failures ? "FAIL" : "ok", failures);
	return failures != 0;
}
