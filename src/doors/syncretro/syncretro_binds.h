/* syncretro_binds.h -- every key the player can press, in one table.
 *
 * Two consumers read it: syncretro_input.c (which key does what) and main.c's
 * help screen (what the player is told). They cannot drift apart, because there
 * is only one table.
 *
 * Bare keys play the game; Ctrl keys drive the door. Function keys are avoided:
 * too many terminals mangle them.
 *
 * NOT bound, deliberately: RetroPad L, R, X and START. Those are FreeIntv's
 * mini-keypad (a 1-bit grid walked by a cursor that steals the disc) and its
 * own pause/help OSD (drawn into the framebuffer, describing bindings this
 * table invalidates). The door supersedes both. See M2_INPUT.md sec 2.
 *
 * Ctrl-H is NOT the help key: Ctrl-H is 0x08, which is Backspace, which is
 * keypad Clear. Help is '?'.
 */
#ifndef SYNCRETRO_BINDS_H_
#define SYNCRETRO_BINDS_H_

typedef enum {
	SR_ACT_NONE = 0,   /* unbound: the key must never reach the core */
	SR_ACT_PAD,        /* id = RETRO_DEVICE_ID_JOYPAD_* */
	SR_ACT_DIGIT,      /* id = 1..9 (never 5): an analog keypad digit */
	SR_ACT_DOOR        /* id = SR_DOOR_* */
} sr_act_t;

enum {
	SR_DOOR_NONE = 0,
	SR_DOOR_PAUSE,
	SR_DOOR_HELP,
	SR_DOOR_RESET,
	SR_DOOR_QUIT
};

/* Fold an ASCII key for lookup. ALPHA ONLY: 'A'-'Z' -> lowercase, everything
 * else returned unchanged. The obvious `c | 0x20` corrupts control characters
 * ('\r' -> '-', '\t' -> ')') and is why Enter and Tab were dead before M2. */
int sr_bind_fold(int c);

/* Resolve a folded ASCII key. Returns SR_ACT_NONE (and sets *id to 0) when the
 * key is unbound. *id must not be NULL. */
sr_act_t sr_bind_lookup(int c, int *id);

/* Walk the help lines: returns 0 once i is past the end, else 1 and sets *key
 * and *desc to static strings. */
int sr_bind_help_line(int i, const char **key, const char **desc);

#endif /* SYNCRETRO_BINDS_H_ */
