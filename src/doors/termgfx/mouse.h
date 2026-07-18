#ifndef TERMGFX_MOUSE_H_
#define TERMGFX_MOUSE_H_
#include "sgrmouse.h"

typedef struct {
	int pixels;    /* SGR-Pixels (DEC 1016) confirmed/auto-detected active */
} termgfx_mouse_t;

/* One classified SGR report, in the terminal's OWN units (canvas pixels
 * when .pixels, else 1-based text cells). The door maps x/y to game coords. */
typedef struct {
	termgfx_sgr_kind_t kind;   /* MOVE / BUTTON / WHEEL */
	int button;                /* BUTTON: 0 left, 1 middle, 2 right */
	int wheel;                 /* WHEEL: -1 up, +1 down */
	int col, row;              /* raw report coords (1-based), pass-through */
	int pixels;                /* snapshot of the pixel-mode latch at report time */
} termgfx_mouse_report_t;

/* Escape strings the door emits. enable turns on motion tracking + SGR +
 * SGR-Pixels and probes 1016 via DECRQM; restore turns them back off. */
extern const char *const termgfx_mouse_enable;    /* "\x1b[?1003h\x1b[?1006h\x1b[?1016h\x1b[?1016$p" */
extern const char *const termgfx_mouse_restore;   /* "\x1b[?1003l\x1b[?1006l\x1b[?1016l" */

/* Feed a DECRPM reply (ESC[?<mode>;<Ps>$y) parsed into params[n]. Latches
 * pixel mode when it confirms mode 1016 is set (Ps==1 or Ps==3). No-op
 * otherwise. */
void termgfx_mouse_on_decrpm(termgfx_mouse_t *m, const int *params, int n);

/* Auto-detect latch: the caller (which owns the text-grid dimensions) calls
 * this when a report coord exceeds the grid -- proof of pixel reporting. */
void termgfx_mouse_note_pixel_report(termgfx_mouse_t *m);
int  termgfx_mouse_pixels(const termgfx_mouse_t *m);

/* Classify one SGR report's button field `b` (via termgfx_sgr_classify) and
 * package it with the raw coords. release is set for the 'm'-terminated form
 * (button-up); it does not change classification, the door uses it. */
void termgfx_mouse_report(termgfx_mouse_t *m, int b, int col, int row,
                          int release, termgfx_mouse_report_t *out);
#endif /* TERMGFX_MOUSE_H_ */
