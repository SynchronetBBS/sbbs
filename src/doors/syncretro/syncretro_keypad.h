/* syncretro_keypad.h -- the Intellivision keypad, as a RetroPad analog vector.
 *
 * The ONLY FreeIntv-specific policy in the door. M3 (multi-core) generalizes
 * this into a per-core profile; until then it is deliberately one small file.
 *
 * Digits 1,2,3,4,6,7,8,9 reach the core through the RIGHT analog stick's ANGLE
 * (FreeIntv controller.c:134-147). Keypad 5, 0, Clear and Enter are ordinary
 * button bits (R3, L3, L2, R2) and are bound in syncretro_binds.c, not here.
 *
 * Only one analog digit can be held at a time -- a stick has one direction --
 * so the last press wins. That is invisible in practice: nobody presses two
 * keypad keys at once. Keypad bits live in a different part of the controller
 * word from the disc, so a digit may be held WHILE steering.
 */
#ifndef SYNCRETRO_KEYPAD_H_
#define SYNCRETRO_KEYPAD_H_

#include <stdint.h>

/* Full-scale axis magnitude. The core integer-divides the axis by 8192 before
 * atan2(), so a full-scale vector cannot be rounded across a bucket boundary. */
#define SR_KP_AXIS 32767

/* Native paths (evdev/kitty): held until the explicit key-up. */
void sr_keypad_press(int digit);
void sr_keypad_release(int digit);

/* Byte path: press + arm the auto-release dwell, mirroring sr_pad_tap(). */
void sr_keypad_tap(int digit, uint32_t now_ms);
void sr_keypad_expire(uint32_t now_ms);

/* Drop the held digit: pause entry, reset, teardown. */
void sr_keypad_release_all(void);

/* The retro_input_state body for RETRO_DEVICE_ANALOG. Returns 0 for the LEFT
 * index (that is the disc) and for every id but ANALOG_X / ANALOG_Y. */
int16_t sr_keypad_analog(unsigned index, unsigned id);

#endif /* SYNCRETRO_KEYPAD_H_ */
