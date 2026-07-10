/* syncretro_binds.c -- see syncretro_binds.h. */
#include "syncretro_binds.h"
#include "libretro.h"    /* RETRO_DEVICE_ID_JOYPAD_* */

#include <stddef.h>

/* `chars` is the set of folded ASCII keys that trigger this row. `keyname` and
 * `desc` are non-NULL only on the row that REPRESENTS a group in the help
 * screen -- the alias rows below it stay silent, so the help reads
 * "W A S D / arrows   disc" rather than four separate lines.
 *
 * For SR_ACT_DIGIT the id is derived from the character, not from `id`. */
typedef struct {
	const char *chars;
	sr_act_t act;
	int id;
	const char *keyname;
	const char *desc;
} sr_bind_row_t;

/* The action buttons are RetroPad A, B and Y. The core's help screen calls them
 * right / left / top, but controller.c asserts B_LEFT from A and B_RIGHT from
 * B -- the two disagree, and only a BIOS run settles it. So the help says
 * "action buttons" and names none of them. Do not guess. */
static const sr_bind_row_t g_binds[] = {
	{ "w",        SR_ACT_PAD,   RETRO_DEVICE_ID_JOYPAD_UP,     "W A S D / arrows", "disc (16-way)" },
	{ "a",        SR_ACT_PAD,   RETRO_DEVICE_ID_JOYPAD_LEFT,   NULL,               NULL },
	{ "s",        SR_ACT_PAD,   RETRO_DEVICE_ID_JOYPAD_DOWN,   NULL,               NULL },
	{ "d",        SR_ACT_PAD,   RETRO_DEVICE_ID_JOYPAD_RIGHT,  NULL,               NULL },

	{ "z",        SR_ACT_PAD,   RETRO_DEVICE_ID_JOYPAD_A,      "Z X C",            "action buttons" },
	{ "x",        SR_ACT_PAD,   RETRO_DEVICE_ID_JOYPAD_B,      NULL,               NULL },
	{ "c",        SR_ACT_PAD,   RETRO_DEVICE_ID_JOYPAD_Y,      NULL,               NULL },

	/* 5 and 0 are absent here on purpose: they are button bits, two rows down. */
	{ "12346789", SR_ACT_DIGIT, 0,                             "1 - 9, 0",         "keypad digits" },
	{ "5",        SR_ACT_PAD,   RETRO_DEVICE_ID_JOYPAD_R3,     NULL,               NULL },
	{ "0",        SR_ACT_PAD,   RETRO_DEVICE_ID_JOYPAD_L3,     NULL,               NULL },

	/* Terminals disagree: some send BS (0x08) for Backspace, some DEL (0x7f). */
	{ "\010\177", SR_ACT_PAD,   RETRO_DEVICE_ID_JOYPAD_L2,     "Backspace",        "keypad Clear" },
	{ "\r\n",     SR_ACT_PAD,   RETRO_DEVICE_ID_JOYPAD_R2,     "Enter",            "keypad Enter" },

	{ "\t",       SR_ACT_PAD,   RETRO_DEVICE_ID_JOYPAD_SELECT, "Tab",              "swap left/right controller" },

	{ " ",        SR_ACT_DOOR,  SR_DOOR_PAUSE,                 "Space",            "pause / resume" },
	{ "?",        SR_ACT_DOOR,  SR_DOOR_HELP,                  "?",                "this help" },
	{ "\022",     SR_ACT_DOOR,  SR_DOOR_RESET,                 "Ctrl-R",           "reset the console" },
	{ "\021",     SR_ACT_DOOR,  SR_DOOR_QUIT,                  "Ctrl-Q",           "quit" }
};

#define SR_NBINDS ((int)(sizeof g_binds / sizeof g_binds[0]))

int sr_bind_fold(int c)
{
	if (c >= 'A' && c <= 'Z')
		return c - 'A' + 'a';
	return c;
}

sr_act_t sr_bind_lookup(int c, int *id)
{
	int i, j;

	*id = 0;
	if (c <= 0)
		return SR_ACT_NONE;   /* NUL would match every row's terminator */

	for (i = 0; i < SR_NBINDS; i++)
		for (j = 0; g_binds[i].chars[j] != '\0'; j++)
			if ((unsigned char)g_binds[i].chars[j] == (unsigned)c) {
				*id = (g_binds[i].act == SR_ACT_DIGIT) ? c - '0' : g_binds[i].id;
				return g_binds[i].act;
			}
	return SR_ACT_NONE;
}

int sr_bind_help_line(int i, const char **key, const char **desc)
{
	int n;

	for (n = 0; n < SR_NBINDS; n++) {
		if (g_binds[n].keyname == NULL)
			continue;
		if (i-- == 0) {
			*key  = g_binds[n].keyname;
			*desc = g_binds[n].desc;
			return 1;
		}
	}
	return 0;
}
