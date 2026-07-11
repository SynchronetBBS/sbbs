/* syncretro_profile.h -- which console's controller the keyboard is pretending
 * to be.
 *
 * SyncRetro hosts any libretro core, but a keyboard is not a controller: the
 * mapping from keys to a RetroPad is a fact about the CONSOLE, and it cannot be
 * derived from the core. A core's SET_INPUT_DESCRIPTORS says it reads
 * JOYPAD_A; it does not say that FreeIntv's JOYPAD_A is the Intellivision's
 * LEFT action button, and it says nothing at all about the disc-angle keypad
 * encoding, which was reverse-engineered from FreeIntv's controller.c. So the
 * profiles are hand-written, and each one is correct. See M3_MULTICORE.md sec 4.
 *
 * Two ship:
 *
 *   SR_PROFILE_PAD    a plain RetroPad -- d-pad, buttons, Start, Select. The
 *                     DEFAULT, and correct for the NES, SMS, ColecoVision, PC
 *                     Engine, Genesis: a console whose controller is a gamepad
 *                     needs no new C at all, only an install directory.
 *
 *   SR_PROFILE_INTV   the Intellivision: twelve keypad keys riding the analog
 *                     stick's angle, a paddle disc swept per frame, and BOTH
 *                     hand controllers driven at once (many cartridges read only
 *                     the right one). See M2_INPUT.md.
 */
#ifndef SYNCRETRO_PROFILE_H_
#define SYNCRETRO_PROFILE_H_

typedef enum {
	SR_PROFILE_PAD = 0,
	SR_PROFILE_INTV
} sr_profile_id_t;

/* Resolve the profile. Call ONCE, after rc_core_load_game() and before the first
 * sr_input_pump(): the core's library_name does not exist until it is loaded.
 *
 *   1. `want` -- the lobby's -profile argument. It KNOWS which console it is.
 *   2. else the core's `library_name` ("freeintv" -> intv; anything else -> pad).
 *   3. else SR_PROFILE_PAD.
 *
 * Either may be NULL. An unrecognized `want` warns and falls through to (2):
 * a typo in an install should not take the door down.
 */
void sr_profile_select(const char *want, const char *library_name);

sr_profile_id_t sr_profile(void);

/* The console's name, for the door's help screen. */
const char *sr_profile_name(void);

/* Does this console read the ANALOG stick? Only the Intellivision does -- its
 * keypad digits are an angle and its paddle is a swept disc. On every other
 * console the analog device is dead, and the d-pad owns Left/Right outright. */
int sr_profile_analog(void);

/* Which controller the ARROW KEYS drive. The Intellivision drives both hand
 * controllers at once, so the arrows are player 2's disc (port 1). Everywhere
 * else there is one player at one keyboard, and the arrows are his d-pad
 * (port 0) -- a solo NES player pressing Up must move, not nudge a phantom
 * second controller. */
int sr_profile_arrow_port(void);

#endif /* SYNCRETRO_PROFILE_H_ */
