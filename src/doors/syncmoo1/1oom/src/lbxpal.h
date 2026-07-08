#ifndef INC_1OOM_LBXPAL_H
#define INC_1OOM_LBXPAL_H

#include "types.h"

#define LBXPAL_NUM_PALETTES 10

extern uint8_t lbxpal_palette[256 * 3];
extern uint8_t lbxpal_update_flag[256];
extern uint8_t lbxpal_colortable[0x18][256];

extern uint8_t *lbxpal_palette_inlbx;
extern uint8_t *lbxpal_fontcolors;
extern uint8_t *lbxpal_cursors;

extern int lbxpal_init(void);
extern void lbxpal_shutdown(void);

extern void lbxpal_select(int pal_index, int first/*or -1*/, int last);
extern void lbxpal_set_palette(uint8_t *pal, int first, int num);
extern void lbxpal_set_update_range(int from, int to);
extern void lbxpal_update(void);
extern void lbxpal_build_colortables(void);
extern uint8_t lbxpal_find_closest(uint8_t r, uint8_t g, uint8_t b);

#endif
