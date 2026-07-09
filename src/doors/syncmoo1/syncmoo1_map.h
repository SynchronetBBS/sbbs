/* syncmoo1_map.h -- terminal input -> 1oom mookey_t/mod/char mapping.
 *
 * Pure mapping layer (no engine/socket/termgfx dependency), so it can be
 * unit-tested in isolation: see tests/test_map.c. Two entry points cover
 * the two shapes of decoded terminal input the syncmoo1 door I/O layer
 * produces: a plain byte (sm_map_ascii) and a parsed CSI sequence
 * (sm_map_csi, params + final byte, no leading ESC/[ and no trailing
 * final passed separately).
 */
#ifndef SYNCMOO1_MAP_H
#define SYNCMOO1_MAP_H

#include <stdint.h>
#include <stdbool.h>

#include "kbd.h"

/* Map a single decoded input byte (printable ASCII or a control byte) to
 * a 1oom key + modifier bits + the literal char to report (e.g. 'A' for
 * shifted 'a'). Returns false if the byte has no mapping. */
bool sm_map_ascii(uint8_t c, mookey_t *key, uint32_t *mod, char *ch);

/* Map a parsed CSI sequence (params[0..nparams-1], final byte -- e.g.
 * "ESC [ 1 5 ~" is params={15}, nparams=1, final='~') to a 1oom key +
 * modifier bits. Returns false if the sequence has no mapping. */
bool sm_map_csi(const int *params, int nparams, char final, mookey_t *key,
                uint32_t *mod);

/* Geometry needed to map an SGR mouse report to a 320x200 game coord:
 * ew/eh/dx/dy describe the image (viewport) rect in canvas pixels, cw/ch
 * are the real probed terminal cell size in pixels, and pixel_mode picks
 * between cell-granular reports (col/row are terminal character cells)
 * and pixel-granular reports (col/row are canvas pixels directly). */
typedef struct {
    int ew, eh;      /* image rect size in canvas px */
    int dx, dy;      /* image rect origin in canvas px */
    int cw, ch;      /* real probed cell size in canvas px */
    int pixel_mode;  /* nonzero: col/row are already canvas pixels */
} sm_geom_t;

/* Map a 1-based SGR mouse col/row to a game coord in [0,319]x[0,199].
 * Pure arithmetic (syncconquer door_input.c:299-339 port): no engine,
 * socket, or termgfx dependency. */
void sm_map_mouse(const sm_geom_t *g, int col, int row, int *gx, int *gy);

#endif
