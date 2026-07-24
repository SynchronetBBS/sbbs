/* syncretro_binds.c -- see syncretro_binds.h. */
#include "syncretro_binds.h"
#include "syncretro_profile.h"
#include "libretro.h"    /* RETRO_DEVICE_ID_JOYPAD_* */

#include <stddef.h>

/* `chars` is the set of folded ASCII keys that trigger this row. `port` is the
 * controller for SR_ACT_PAD / SR_ACT_DIGIT (0 = player 1, 1 = player 2); it is
 * ignored for the door rows. `keyname` and `desc` are non-NULL only on the row
 * that REPRESENTS a group in the help screen -- the alias rows below it stay
 * silent, so the help reads one line per group rather than one per key.
 *
 * Both Intellivision hand controllers are driven at once (see syncretro_input.c
 * / M2_INPUT.md): player 1 on the left-hand keys below, player 2 on the arrows
 * (disc), , . / (buttons) and numeric keypad (digits). Only player 1's keys and
 * player 2's action buttons are ASCII bytes and so live here; player 2's arrows
 * and numpad arrive as CSI / evdev / kitty sequences and are routed to port 1 in
 * syncretro_input.c. Tab swaps which core port each player's keys drive.
 *
 * For SR_ACT_DIGIT the id is derived from the character, not from `id`. */
typedef struct {
	const char *chars;
	sr_act_t act;
	int id;
	int port;
	const char *keyname;
	const char *desc;
} sr_bind_row_t;

/* --- the Intellivision (SR_PROFILE_INTV) -------------------------------------
 *
 * The action buttons are RetroPad A, B and Y. The core's help screen calls them
 * right / left / top, but controller.c asserts B_LEFT from A and B_RIGHT from
 * B -- the two disagree, and only a BIOS run settles it. So the help says
 * "action buttons" and names none of them. Do not guess. */
static const sr_bind_row_t g_binds[] = {
	/* --- Player 1 (one controller): W A S D + Z X C + the number row --- */
	{ "w",        SR_ACT_PAD,   RETRO_DEVICE_ID_JOYPAD_UP,     0, "W A S D \1 arrows", "disc (player 1 \1 2)" },
	{ "a",        SR_ACT_PAD,   RETRO_DEVICE_ID_JOYPAD_LEFT,   0, NULL,               NULL },
	{ "s",        SR_ACT_PAD,   RETRO_DEVICE_ID_JOYPAD_DOWN,   0, NULL,               NULL },
	{ "d",        SR_ACT_PAD,   RETRO_DEVICE_ID_JOYPAD_RIGHT,  0, NULL,               NULL },

	{ "z",        SR_ACT_PAD,   RETRO_DEVICE_ID_JOYPAD_A,      0, "Z X C \1 , . /",    "action buttons" },
	{ "x",        SR_ACT_PAD,   RETRO_DEVICE_ID_JOYPAD_B,      0, NULL,               NULL },
	{ "c",        SR_ACT_PAD,   RETRO_DEVICE_ID_JOYPAD_Y,      0, NULL,               NULL },

	/* 5 and 0 are absent here on purpose: they are button bits, two rows down. */
	{ "12346789", SR_ACT_DIGIT, 0,                             0, "1-9,0 \1 num pad",  "keypad digits" },
	{ "5",        SR_ACT_PAD,   RETRO_DEVICE_ID_JOYPAD_R3,     0, NULL,               NULL },
	{ "0",        SR_ACT_PAD,   RETRO_DEVICE_ID_JOYPAD_L3,     0, NULL,               NULL },

	/* Terminals disagree: some send BS (0x08) for Backspace, some DEL (0x7f). */
	{ "\010\177", SR_ACT_PAD,   RETRO_DEVICE_ID_JOYPAD_L2,     0, "Bksp \1 KP Del",    "keypad Clear" },
	{ "\r\n",     SR_ACT_PAD,   RETRO_DEVICE_ID_JOYPAD_R2,     0, "Enter \1 KP Enter", "keypad Enter" },

	/* --- Player 2 action buttons. Its disc (arrows) and keypad (numpad) are
	 * decoded in syncretro_input.c, not here; these rows are silent in the help
	 * because player 1's rows above already name them. --- */
	{ ",",        SR_ACT_PAD,   RETRO_DEVICE_ID_JOYPAD_A,      1, NULL,               NULL },
	{ ".",        SR_ACT_PAD,   RETRO_DEVICE_ID_JOYPAD_B,      1, NULL,               NULL },
	{ "/",        SR_ACT_PAD,   RETRO_DEVICE_ID_JOYPAD_Y,      1, NULL,               NULL },

	/* --- door --- */
	{ "+=",       SR_ACT_DOOR,  SR_DOOR_VOL_UP,                0, "+ -",              "volume up / down (0 = off)" },
	{ "-_",       SR_ACT_DOOR,  SR_DOOR_VOL_DOWN,              0, NULL,               NULL },
	{ "\t",       SR_ACT_SWAP,  0,                             0, "Tab",              "swap the two controllers" },
	{ " ",        SR_ACT_DOOR,  SR_DOOR_PAUSE,                 0, "Space",            "pause / resume" },
	{ "?",        SR_ACT_DOOR,  SR_DOOR_HELP,                  0, "?",                "this help" },
	{ "\023",     SR_ACT_DOOR,  SR_DOOR_STATS,                 0, "Ctrl-S",           "stats overlay" },
	{ "\022",     SR_ACT_DOOR,  SR_DOOR_RESET,                 0, "Ctrl-R",           "reset the console" },
	{ "\021",     SR_ACT_DOOR,  SR_DOOR_QUIT,                  0, "Ctrl-Q",           "quit" },
	/* F4 has no BYTES: it arrives as an escape sequence (or a physical-key
	 * report), and syncretro_input.c dispatches it directly. The empty `chars`
	 * makes this row unmatchable -- it exists so the help screen lists the key. */
	{ "",        SR_ACT_NONE, 0,                             0, "F4",               "render tier (sixel / text)" }
};

#define SR_NBINDS ((int)(sizeof g_binds / sizeof g_binds[0]))

/* --- the gamepad (SR_PROFILE_PAD) -------------------------------------------
 *
 * A plain RetroPad, and so the whole of the NES: d-pad, B, A, Select, Start.
 * The extra face and shoulder buttons are bound because a 4- or 6-button console
 * (SMS, Genesis, PC Engine) is the same profile with more fingers -- binding them
 * now costs nothing and means those consoles are an install directory, not a
 * patch.
 *
 * Z=B and X=A is not a guess: fceumm's SET_INPUT_DESCRIPTORS names id 0 "B" and
 * id 8 "A", and the physical NES pad puts B on the left of A -- which is how Z
 * and X sit on the keyboard.
 *
 * Backspace = Select mirrors the Intellivision profile's Backspace = keypad
 * Clear: the same physical key keeps the same *role* (the odd one out) across
 * consoles, so muscle memory does not fight.
 *
 * ONE controller, not two. There is one player at one keyboard, so the arrows
 * are HIS d-pad (port 0 -- see sr_profile_arrow_port()), not a phantom second
 * player's. Tab still swaps ports, for the rare cartridge that reads port 2. */
static const sr_bind_row_t g_binds_pad[] = {
	{ "w",        SR_ACT_PAD,  RETRO_DEVICE_ID_JOYPAD_UP,     0, "W A S D \1 arrows", "d-pad" },
	{ "a",        SR_ACT_PAD,  RETRO_DEVICE_ID_JOYPAD_LEFT,   0, NULL,               NULL },
	{ "s",        SR_ACT_PAD,  RETRO_DEVICE_ID_JOYPAD_DOWN,   0, NULL,               NULL },
	{ "d",        SR_ACT_PAD,  RETRO_DEVICE_ID_JOYPAD_RIGHT,  0, NULL,               NULL },

	{ "z",        SR_ACT_PAD,  RETRO_DEVICE_ID_JOYPAD_B,      0, "Z",                "B" },
	{ "x",        SR_ACT_PAD,  RETRO_DEVICE_ID_JOYPAD_A,      0, "X",                "A" },
	/* Idle on the NES; alive on a 4-/6-button console. */
	{ "c",        SR_ACT_PAD,  RETRO_DEVICE_ID_JOYPAD_Y,      0, "C V",              "Y X (if the console has them)" },
	{ "v",        SR_ACT_PAD,  RETRO_DEVICE_ID_JOYPAD_X,      0, NULL,               NULL },
	{ "q",        SR_ACT_PAD,  RETRO_DEVICE_ID_JOYPAD_L,      0, "Q E",              "L R (if the console has them)" },
	{ "e",        SR_ACT_PAD,  RETRO_DEVICE_ID_JOYPAD_R,      0, NULL,               NULL },

	{ "\r\n",     SR_ACT_PAD,  RETRO_DEVICE_ID_JOYPAD_START,  0, "Enter",            "Start" },
	/* Terminals disagree: some send BS (0x08) for Backspace, some DEL (0x7f). */
	{ "\010\177", SR_ACT_PAD,  RETRO_DEVICE_ID_JOYPAD_SELECT, 0, "Bksp",             "Select" },

	/* --- door --- */
	{ "+=",       SR_ACT_DOOR, SR_DOOR_VOL_UP,                0, "+ -",              "volume up / down (0 = off)" },
	{ "-_",       SR_ACT_DOOR, SR_DOOR_VOL_DOWN,              0, NULL,               NULL },
	{ "\t",       SR_ACT_SWAP, 0,                             0, "Tab",              "swap the two controller ports" },
	{ " ",        SR_ACT_DOOR, SR_DOOR_PAUSE,                 0, "Space",            "pause / resume" },
	{ "?",        SR_ACT_DOOR, SR_DOOR_HELP,                  0, "?",                "this help" },
	{ "\023",     SR_ACT_DOOR, SR_DOOR_STATS,                 0, "Ctrl-S",           "stats overlay" },
	{ "\022",     SR_ACT_DOOR, SR_DOOR_RESET,                 0, "Ctrl-R",           "reset the console" },
	{ "\021",     SR_ACT_DOOR, SR_DOOR_QUIT,                  0, "Ctrl-Q",           "quit" },
	/* F4 has no BYTES: it arrives as an escape sequence (or a physical-key
	 * report), and syncretro_input.c dispatches it directly. The empty `chars`
	 * makes this row unmatchable -- it exists so the help screen lists the key. */
	{ "",        SR_ACT_NONE, 0,                             0, "F4",               "render tier (sixel / text)" }
};

#define SR_NBINDS_PAD ((int)(sizeof g_binds_pad / sizeof g_binds_pad[0]))

/* --- the arcade cabinet (SR_PROFILE_ARCADE) ----------------------------------
 *
 * The SAME RetroPad bindings as `pad`, deliberately: a control panel is a stick
 * and a row of buttons, and MAME reads them through a RetroPad like any other
 * core. This table exists because the WORDS in the one above are a console's,
 * and a cabinet player reading them is misled at the moment it matters most --
 * nothing happens on a cabinet until a coin goes in, and a help screen that
 * calls the coin slot "Select" never says so.
 *
 * BUTTON NUMBERS, NOT LETTERS. MAME 2003-Plus reports what a driver wants
 * ("Supports 6 distinct button controls" for Street Fighter II, 0 for Pac-Man)
 * but sends no SET_INPUT_DESCRIPTORS, so the core cannot tell us WHICH arcade
 * button each RetroPad id became. Numbering them 1-6 in the core's own order is
 * therefore the most the door can honestly claim; naming them "punch" and
 * "kick" would be a guess, and a wrong guess on a help screen is worse than a
 * vague one. A game that uses fewer simply ignores the rest.
 *
 * No Tab. `pad` swaps the two controller ports for the rare cartridge that
 * reads port 2; on a cabinet that silently moves the player onto player two's
 * controls, where his coin and start buttons are different inputs again -- a
 * dead game and no clue why. */
static const sr_bind_row_t g_binds_arcade[] = {
	{ "w",        SR_ACT_PAD,  RETRO_DEVICE_ID_JOYPAD_UP,     0, "W A S D \1 arrows", "joystick" },
	{ "a",        SR_ACT_PAD,  RETRO_DEVICE_ID_JOYPAD_LEFT,   0, NULL,               NULL },
	{ "s",        SR_ACT_PAD,  RETRO_DEVICE_ID_JOYPAD_DOWN,   0, NULL,               NULL },
	{ "d",        SR_ACT_PAD,  RETRO_DEVICE_ID_JOYPAD_RIGHT,  0, NULL,               NULL },

	{ "z",        SR_ACT_PAD,  RETRO_DEVICE_ID_JOYPAD_B,      0, "Z X",              "buttons 1 and 2" },
	{ "x",        SR_ACT_PAD,  RETRO_DEVICE_ID_JOYPAD_A,      0, NULL,               NULL },
	{ "c",        SR_ACT_PAD,  RETRO_DEVICE_ID_JOYPAD_Y,      0, "C V",              "buttons 3 and 4" },
	{ "v",        SR_ACT_PAD,  RETRO_DEVICE_ID_JOYPAD_X,      0, NULL,               NULL },
	{ "q",        SR_ACT_PAD,  RETRO_DEVICE_ID_JOYPAD_L,      0, "Q E",              "buttons 5 and 6" },
	{ "e",        SR_ACT_PAD,  RETRO_DEVICE_ID_JOYPAD_R,      0, NULL,               NULL },

	/* THE SECOND STICK, and the one binding here that is not a button.
	 *
	 * A twin-stick cabinet drives each half of the machine from its own stick:
	 * Battlezone's two treads, Robotron's move-and-aim. MAME 2003-Plus reads the
	 * second one off the RetroPad's RIGHT ANALOG STICK and nothing else, so a
	 * keyboard cannot reach it through the d-pad or any button -- measured, not
	 * assumed: every input-related core option (mame_remapping, input_interface,
	 * digital_joy_centering) leaves it exactly where it is. Battlezone without
	 * these two keys answers on one tread only, which pivots but never drives.
	 *
	 * W/S is the left stick, I/K the right -- the same two fingers each hand
	 * would use on the cabinet. The help says "twin-stick" rather than naming
	 * treads: the core sends no SET_INPUT_DESCRIPTORS, so the door cannot tell
	 * WHICH game it is running, and a game with one stick simply ignores these
	 * (its driver never reads the axis). Same reason the buttons are numbered. */
	{ "i",        SR_ACT_AXIS, SR_AXIS_RIGHT_Y_NEG,           0, "I K",              "2nd stick up / down (twin-stick cabinets)" },
	{ "k",        SR_ACT_AXIS, SR_AXIS_RIGHT_Y_POS,           0, NULL,               NULL },

	/* The two that actually start a game, named as the cabinet names them.
	 * Terminals disagree about Backspace (BS 0x08 vs DEL 0x7f), so both. */
	{ "\010\177", SR_ACT_PAD,  RETRO_DEVICE_ID_JOYPAD_SELECT, 0, "Bksp",             "INSERT COIN" },
	{ "\r\n",     SR_ACT_PAD,  RETRO_DEVICE_ID_JOYPAD_START,  0, "Enter",            "1-player start (after a coin)" },

	/* --- door --- */
	{ "+=",       SR_ACT_DOOR, SR_DOOR_VOL_UP,                0, "+ -",              "volume up / down (0 = off)" },
	{ "-_",       SR_ACT_DOOR, SR_DOOR_VOL_DOWN,              0, NULL,               NULL },
	{ " ",        SR_ACT_DOOR, SR_DOOR_PAUSE,                 0, "Space",            "pause / resume" },
	{ "?",        SR_ACT_DOOR, SR_DOOR_HELP,                  0, "?",                "this help" },
	{ "\023",     SR_ACT_DOOR, SR_DOOR_STATS,                 0, "Ctrl-S",           "stats overlay" },
	{ "\022",     SR_ACT_DOOR, SR_DOOR_RESET,                 0, "Ctrl-R",           "reset the machine" },
	{ "\021",     SR_ACT_DOOR, SR_DOOR_QUIT,                  0, "Ctrl-Q",           "quit" },
	/* F4 has no BYTES: it arrives as an escape sequence (or a physical-key
	 * report) and syncretro_input.c dispatches it directly. The empty `chars`
	 * makes this row unmatchable -- it exists so the help screen lists the key. */
	{ "",         SR_ACT_NONE, 0,                             0, "F4",               "render tier (sixel / text)" }
};

#define SR_NBINDS_ARCADE ((int)(sizeof g_binds_arcade / sizeof g_binds_arcade[0]))

/* The active table. One table drives BOTH the key handler and the help screen --
 * the M2 invariant -- so switching profiles switches both, together, and they
 * cannot drift apart. */
static const sr_bind_row_t *sr_bind_table(int *n)
{
	if (sr_profile() == SR_PROFILE_INTV) {
		*n = SR_NBINDS;
		return g_binds;
	}
	if (sr_profile() == SR_PROFILE_ARCADE) {
		*n = SR_NBINDS_ARCADE;
		return g_binds_arcade;
	}
	*n = SR_NBINDS_PAD;
	return g_binds_pad;
}

int sr_bind_fold(int c)
{
	if (c >= 'A' && c <= 'Z')
		return c - 'A' + 'a';
	return c;
}

sr_act_t sr_bind_lookup(int c, int *id, int *port)
{
	int                  n, i, j;
	const sr_bind_row_t *t = sr_bind_table(&n);

	*id   = 0;
	*port = 0;
	if (c <= 0)
		return SR_ACT_NONE;   /* NUL would match every row's terminator */

	for (i = 0; i < n; i++)
		for (j = 0; t[i].chars[j] != '\0'; j++)
			if ((unsigned char)t[i].chars[j] == (unsigned)c) {
				*id   = (t[i].act == SR_ACT_DIGIT) ? c - '0' : t[i].id;
				*port = t[i].port;
				return t[i].act;
			}
	return SR_ACT_NONE;
}

int sr_bind_help_line(int i, const char **key, const char **desc)
{
	int                  n, m;
	const sr_bind_row_t *t = sr_bind_table(&n);

	for (m = 0; m < n; m++) {
		if (t[m].keyname == NULL)
			continue;
		if (i-- == 0) {
			*key  = t[m].keyname;
			*desc = t[m].desc;
			return 1;
		}
	}
	return 0;
}
