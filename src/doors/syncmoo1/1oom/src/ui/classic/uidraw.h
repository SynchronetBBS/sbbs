#ifndef INC_1OOM_UIDRAW_H
#define INC_1OOM_UIDRAW_H

#include "game_types.h"
#include "types.h"

extern int ui_draw_finish_mode;
extern const uint8_t tbl_banner_color[BANNER_NUM];
extern const uint8_t tbl_banner_color2[BANNER_NUM];
extern const uint8_t tbl_banner_fontparam[BANNER_NUM];

extern void ui_draw_erase_buf(void);
extern void ui_draw_copy_buf(void);
extern void ui_draw_color_buf(uint8_t color);
extern void ui_draw_pixel(int x, int y,  uint8_t color, int scale);
extern void ui_draw_filled_rect(int x0, int y0, int x1, int y1, uint8_t color, int scale);
extern void ui_draw_line1(int x0, int y0, int x1, int y1, uint8_t color, int scale);
extern void ui_draw_line_ctbl(int x0, int y0, int x1, int y1, const uint8_t *colortbl, int colornum, int pos, int scale);
extern void ui_draw_line_limit(int x0, int y0, int x1, int y1, uint8_t color, int scale);
extern void ui_draw_line_limit_ctbl(int x0, int y0, int x1, int y1, const uint8_t *colortbl, int colornum, int pos, int scale);
extern void ui_draw_slider(int x, int y, int w, int wdiv, int xoff, uint8_t color, int scale);
extern void ui_draw_box1(int x0, int y0, int x1, int y1, uint8_t color1, uint8_t color2, int scale);
extern void ui_draw_box2(int x0, int y0, int x1, int y1, uint8_t color1, uint8_t color2, uint8_t color3, uint8_t color4, int scale);
extern void ui_draw_box_fill(int x0, int y0, int x1, int y1, const uint8_t *colorptr, uint8_t color0, uint16_t colorhalf, uint16_t ac, uint8_t colorpos, int scale);
extern void ui_draw_box_grain(int x0, int y0, int x1, int y1, uint8_t color0, uint8_t color1, uint8_t ae, int scale);
extern void ui_draw_text_overlay(int x, int y, const char *str);
extern void ui_draw_finish(void);
extern void ui_draw_stars(int x, int y, int xoff1, int xoff2, int scale);
extern void ui_draw_set_stars_xoffs(bool flag_right);
extern void ui_draw_textbox_2str(const char *str1, const char *str2, int y0, int scale);

#endif
