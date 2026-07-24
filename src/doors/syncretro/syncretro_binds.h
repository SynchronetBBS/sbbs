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
	SR_ACT_PAD,        /* id = RETRO_DEVICE_ID_JOYPAD_* on controller `port` */
	SR_ACT_DIGIT,      /* id = 1..9 (never 5): an analog keypad digit on `port` */
	SR_ACT_AXIS,       /* id = SR_AXIS_* : an analog DEFLECTION on `port` */
	SR_ACT_DOOR,       /* id = SR_DOOR_* */
	SR_ACT_SWAP        /* Tab: swap which core port each player's keys drive */
} sr_act_t;

/* SR_ACT_AXIS ids -- a stick deflection a KEY has to produce, because the core
 * put a control there and nowhere else.
 *
 * A twin-stick cabinet (Battlezone's two treads, Robotron's aim) is the case:
 * MAME 2003-Plus binds the second stick to the RetroPad's RIGHT stick only --
 * no button, no d-pad, and no core option moves it (mame_remapping,
 * input_interface and digital_joy_centering were all measured against it) -- so
 * on a keyboard half the control panel is unreachable without this. Battlezone
 * could pivot but never drive straight, because only one tread answered.
 *
 * These are NOT RetroPad button ids and must never be sent as one. They are
 * state slots ABOVE the last real id (R3 = 15) so they ride the pad's existing
 * press/release/dwell machinery; syncretro_input.c pins that with a build-time
 * check and refuses to hand any id this high to the core as a button. */
#define SR_AXIS_RIGHT_Y_NEG 16   /* right stick UP    (libretro Y is DOWN-positive) */
#define SR_AXIS_RIGHT_Y_POS 17   /* right stick DOWN */

enum {
	SR_DOOR_NONE = 0,
	SR_DOOR_PAUSE,
	SR_DOOR_HELP,
	SR_DOOR_RESET,
	SR_DOOR_QUIT,
	SR_DOOR_STATS,     /* Ctrl-S: toggle the live stats overlay */
	SR_DOOR_TIER,      /* F4: step to the next render tier (sixel <-> the text tiers) */
	SR_DOOR_VOL_UP,    /* '+' : louder */
	SR_DOOR_VOL_DOWN   /* '-' : quieter, and at 0 we stop SENDING audio at all */
};

/* Fold an ASCII key for lookup. ALPHA ONLY: 'A'-'Z' -> lowercase, everything
 * else returned unchanged. The obvious `c | 0x20` corrupts control characters
 * ('\r' -> '-', '\t' -> ')') and is why Enter and Tab were dead before M2. */
int sr_bind_fold(int c);

/* Resolve a folded ASCII key. Returns SR_ACT_NONE (and sets *id to 0) when the
 * key is unbound. Sets *port to the controller (0 = player 1, 1 = player 2) for
 * SR_ACT_PAD / SR_ACT_DIGIT / SR_ACT_AXIS; 0 otherwise. Both out-params must be
 * non-NULL. */
sr_act_t sr_bind_lookup(int c, int *id, int *port);

/* Walk the help lines: returns 0 once i is past the end, else 1 and sets *key
 * and *desc to static strings. */
int sr_bind_help_line(int i, const char **key, const char **desc);

#endif /* SYNCRETRO_BINDS_H_ */
