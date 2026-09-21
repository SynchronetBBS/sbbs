/* keymap.h -- termgfx key events -> the engine's BIOS keyboard (INT 16h key
 * words) and held-key table (XT scan codes), plus the door's own keys. */
#ifndef SYNCDRIVE_KEYMAP_H_
#define SYNCDRIVE_KEYMAP_H_

#include <stdint.h>
#include "termgfx_termio.h"

enum { KEYMAP_ACT_NONE, KEYMAP_ACT_SOUND_TOGGLE, KEYMAP_ACT_FIT_CYCLE, KEYMAP_ACT_HELP };

typedef struct {
	uint16_t bios;
	uint8_t xt;
	int action;
	int down;
} keymap_result_t;

keymap_result_t keymap_translate(const termgfx_input_event_t *ev);
uint16_t        keymap_sound_toggle_key(int sound_on);

#endif
