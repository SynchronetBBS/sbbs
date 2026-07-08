#ifndef INC_1OOM_LBXFONT_H
#define INC_1OOM_LBXFONT_H

#include "types.h"

extern int lbxfont_init(void);
extern void lbxfont_shutdown(void);

extern void lbxfont_select(uint8_t a0, uint8_t a1, uint8_t a2, uint8_t a3);
extern void lbxfont_select_set_12_1(uint8_t a0, uint8_t a1, uint8_t a2, uint8_t a3);
extern void lbxfont_select_set_12_4(uint8_t a0, uint8_t a1, uint8_t a2, uint8_t a3);
extern void lbxfont_select_set_12_5(uint8_t a0, uint8_t a1, uint8_t a2, uint8_t a3);

extern uint16_t lbxfont_get_height(void);
extern int lbxfont_get_gap_h(void);
extern void lbxfont_set_gap_h(uint16_t value);

extern void lbxfont_select_subcolors(uint16_t a0);
extern void lbxfont_select_subcolors_0(void);
extern void lbxfont_select_subcolors_13not1(void);
extern void lbxfont_select_subcolors_13not2(void);

extern void lbxfont_set_colors(const uint8_t *colorptr);
extern void lbxfont_set_colors_n(const uint8_t *colorptr, int num);
extern void lbxfont_set_color_c_n(uint8_t color, int num);
extern void lbxfont_set_color0(uint8_t color);
extern void lbxfont_set_temp_color(uint8_t color);
extern void lbxfont_set_14_24(uint8_t color1, uint8_t color2);

extern void lbxfont_set_space_w(int w);
extern int lbxfont_get_gap_w(void);
extern int lbxfont_get_char_w(char c);
extern int lbxfont_calc_str_width(const char *str);
extern int lbxfont_calc_split_str_h(int maxw, const char *str);

extern uint16_t lbxfont_plotchar(char c, uint8_t *buf, uint16_t pitch, int scale);

extern int lbxfont_print_str_normal(int x, int y, const char *str, uint16_t pitch, int scale);
extern int lbxfont_print_str_center(int x, int y, const char *str, uint16_t pitch, int scale);
extern int lbxfont_print_str_right(int x, int y, const char *str, uint16_t pitch, int scale);
extern void lbxfont_print_str_split(int x, int y, int maxw, const char *str, int type, uint16_t pitch, uint16_t maxy, int scale);
extern int lbxfont_print_str_normal_limit(int x, int y, const char *str, int lx0, int ly0, int lx1, int ly1, uint16_t pitch, int scale);
extern int lbxfont_print_str_center_limit(int x, int y, const char *str, int lx0, int ly0, int lx1, int ly1, uint16_t pitch, int scale);
extern int lbxfont_print_num_normal(int x, int y, int num, uint16_t pitch, int scale);
extern int lbxfont_print_num_center(int x, int y, int num, uint16_t pitch, int scale);
extern int lbxfont_print_num_right(int x, int y, int num, uint16_t pitch, int scale);
extern int lbxfont_print_range_right(int x, int y, int num0, int num1, uint16_t pitch, int scale);

extern uint8_t lbxfont_get_current_fontnum(void);
extern uint8_t lbxfont_get_current_fonta2(void);
extern uint8_t lbxfont_get_current_fonta2b(void);
extern uint8_t lbxfont_get_current_fonta4(void);

#endif
