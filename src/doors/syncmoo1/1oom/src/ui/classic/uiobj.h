#ifndef INC_1OOM_UIOBJ_H
#define INC_1OOM_UIOBJ_H

#include "kbd.h"
#include "types.h"

#define UIOBJI_NONE 0
#define UIOBJI_ESC  -1
#define UIOBJI_INVALID  -1000
#define UIOBJI_OUTSIDE  -10000

#define UIOBJI_SET_TBL_INVALID(name) do { for (int i = 0; i < (sizeof(name)/sizeof(name[0])); ++i) { name[i] = UIOBJI_INVALID; } } while (0)
#define UIOBJI_SET_TBL2_INVALID(n0_, n1_) do { for (int i_ = 0; i_ < (sizeof(n0_)/sizeof(n0_[0])); ++i_) { n0_[i_] = UIOBJI_INVALID; n1_[i_] = UIOBJI_INVALID; } } while (0)
#define UIOBJI_SET_TBL3_INVALID(n0_, n1_, n2_) do { for (int i_ = 0; i_ < (sizeof(n0_)/sizeof(n0_[0])); ++i_) { n0_[i_] = UIOBJI_INVALID; n1_[i_] = UIOBJI_INVALID; n2_[i_] = UIOBJI_INVALID; } } while (0)
#define UIOBJI_SET_TBL4_INVALID(n0_, n1_, n2_, n3_) do { for (int i_ = 0; i_ < (sizeof(n0_)/sizeof(n0_[0])); ++i_) { n0_[i_] = UIOBJI_INVALID; n1_[i_] = UIOBJI_INVALID; n2_[i_] = UIOBJI_INVALID; n3_[i_] = UIOBJI_INVALID; } } while (0)
#define UIOBJI_SET_TBL5_INVALID(n0_, n1_, n2_, n3_, n4_) do { for (int i_ = 0; i_ < (sizeof(n0_)/sizeof(n0_[0])); ++i_) { n0_[i_] = UIOBJI_INVALID; n1_[i_] = UIOBJI_INVALID; n2_[i_] = UIOBJI_INVALID; n3_[i_] = UIOBJI_INVALID; n4_[i_] = UIOBJI_INVALID; } } while (0)
#define UIOBJI_SET_TBL6_INVALID(n0_, n1_, n2_, n3_, n4_, n5_) do { for (int i_ = 0; i_ < (sizeof(n0_)/sizeof(n0_[0])); ++i_) { n0_[i_] = UIOBJI_INVALID; n1_[i_] = UIOBJI_INVALID; n2_[i_] = UIOBJI_INVALID; n3_[i_] = UIOBJI_INVALID; n4_[i_] = UIOBJI_INVALID; n5_[i_] = UIOBJI_INVALID; } } while (0)

/* HACK for lbxgfx_draw_frame_offs params */
extern int uiobj_minx;
extern int uiobj_miny;
extern int uiobj_maxx;
extern int uiobj_maxy;

extern void uiobj_table_clear(void);
extern void uiobj_table_set_last(int16_t oi);
extern void uiobj_table_num_store(void);
extern void uiobj_table_num_restore(void);
extern int16_t uiobj_handle_input_cond(void);

extern void uiobj_finish_frame(void);
extern void uiobj_set_downcount(int16_t v);
extern void uiobj_set_xyoff(int xoff, int yoff);
extern void uiobj_set_limits(int minx, int miny, int maxx, int maxy);
extern void uiobj_set_limits_all(void);
extern void uiobj_set_focus_forced(int16_t uiobji);
extern void uiobj_set_focus(int16_t uiobji);
extern void uiobj_set_help_id(int16_t v);
extern int16_t uiobj_get_clicked_oi(void);
extern void uiobj_set_skip_delay(bool v);

extern int16_t uiobj_find_obj_at_cursor(void);
extern int16_t uiobj_at_cursor(void);

extern void uiobj_set_callback_and_delay(void (*cb)(void *), void *data, uint16_t delay);
extern void uiobj_unset_callback(void);
extern void uiobj_do_callback(void);

extern int16_t uiobj_add_t0(uint16_t x, uint16_t y, const char *str, uint8_t *lbxdata, mookey_t key);
extern int16_t uiobj_add_t1(uint16_t x, uint16_t y, const char *str, uint8_t *lbxdata, int16_t *vptr, mookey_t key);
extern int16_t uiobj_add_t2(uint16_t x, uint16_t y, const char *str, uint8_t *lbxdata, int16_t *vptr, mookey_t key);
extern int16_t uiobj_add_t3(uint16_t x, uint16_t y, const char *str, uint8_t *lbxdata, int16_t *vptr, int16_t z18, mookey_t key);
extern int16_t uiobj_add_textinput(int x, int y, int w, char *buf, uint16_t buflen, uint8_t rcolor, bool alignr, bool allow_lcase, const uint8_t *colortbl, mookey_t key);
extern int16_t uiobj_add_slider_int(uint16_t x0, uint16_t y0, uint16_t vmin, uint16_t vmax, uint16_t w, uint16_t h, int16_t *vptr);
extern int16_t uiobj_add_slider_func(uint16_t x0, uint16_t y0, uint16_t vmin, uint16_t vmax, uint16_t w, uint16_t h, int16_t *vptr, void (*cb)(void *ctx, uint8_t slideri, int16_t value), void *ctx, uint8_t slideri);
extern int16_t uiobj_add_mousearea(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, mookey_t key);
extern int16_t uiobj_add_mousewheel(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, int16_t *vptr);
extern int16_t uiobj_add_mousearea_limited(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t scale, mookey_t key);
extern int16_t uiobj_add_mousearea_all(mookey_t key);
extern int16_t uiobj_add_inputkey(uint32_t key);
extern int16_t uiobj_add_alt_str(const char *str);
extern int16_t uiobj_add_ta(uint16_t x, uint16_t y, uint16_t w, const char *str, bool z12, int16_t *vptr, int16_t z18, uint8_t subtype, uint8_t sp0v, mookey_t key);
extern int16_t uiobj_add_tb(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t xscale, uint16_t yscale, int16_t *xptr, int16_t *yptr, uint8_t *zptr, uint8_t zmax);

extern void uiobj_dec_y1(int16_t oi);
extern void uiobj_ta_set_val_0(int16_t oi);
extern void uiobj_ta_set_val_1(int16_t oi);

extern int16_t uiobj_select_from_list1(int x, int y, int w, const char *title, char const * const *strtbl, int16_t *selptr, const bool *condtbl, uint8_t subtype, uint8_t sp0v, bool update_at_cursor);
extern int16_t uiobj_select_from_list2(int x, int y, int w, const char *title, char const * const *strtbl, int16_t *selptr, const bool *condtbl, int linenum, int upx, int upy, uint8_t *uplbx, int dnx, int dny, uint8_t *dnlbx, uint8_t subtype, uint8_t sp0v, bool update_at_cursor);
extern int16_t uiobj_select_from_list3(int x, int y, int w, const char *title, char const * const *strtbl, int16_t *selptr, const bool *condtbl, uint8_t subtype, uint8_t sp0v, bool update_at_cursor, bool add_switch, int sx, int sy);
extern int16_t uiobj_select_from_list4(int x, int y, int w, const char *title, char const * const *strtbl, int16_t *selptr, const bool *condtbl, int linenum, int upx, int upy, uint8_t *uplbx, int dnx, int dny, uint8_t *dnlbx, uint8_t subtype, uint8_t sp0v, bool update_at_cursor, bool add_switch, int sx, int sy);
extern bool uiobj_read_str(int x, int y, int w, char *buf, int buflen, uint8_t rcolor, bool alignr, const uint8_t *ctbl);

extern void uiobj_input_flush(void);
extern void uiobj_input_wait(void);

#endif
