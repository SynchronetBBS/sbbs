#ifndef INC_1OOM_UICURSOR_H
#define INC_1OOM_UICURSOR_H

#include "types.h"

typedef enum {
    UI_CURSOR_SCALE_MODE_NORMAL,
    UI_CURSOR_SCALE_MODE_STARMAP,
} ui_cursor_scale_mode_t;

typedef struct ui_cursor_area_s {
    uint8_t cursor_i;
    uint8_t mouseoff;
    uint8_t cursor_scale_mode;
    uint16_t x0;
    uint16_t y0;
    uint16_t x1;    /* inclusive */
    uint16_t y1;    /* inclusive */
} ui_cursor_area_t;

extern ui_cursor_area_t ui_cursor_area_all_i0;
extern ui_cursor_area_t ui_cursor_area_all_i1;

#define UI_CURSOR_AREA_NUM 11
extern ui_cursor_area_t ui_cursor_area_tbl[UI_CURSOR_AREA_NUM];   /* not const! */

extern uint16_t ui_cursor_gfx_i;
extern uint16_t ui_cursor_mouseoff;

extern void ui_cursor_init(int scale);
extern void ui_cursor_setup_area(int num, ui_cursor_area_t *area);
extern void ui_cursor_update_gfx_i(int mx, int my);
extern void ui_cursor_store_bg1(int mx, int my);
extern void ui_cursor_store_bg0(int mx, int my);
extern void ui_cursor_draw1(int mx, int my);
extern void ui_cursor_draw0(int mx, int my);
extern void ui_cursor_erase0(void);
extern void ui_cursor_erase1(void);
extern void ui_cursor_copy_bg1_to_bg0(void);
extern void ui_cursor_refresh(int mx, int my);

#endif
