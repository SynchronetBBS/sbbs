#ifndef INC_1OOM_LBXGFX_H
#define INC_1OOM_LBXGFX_H

#include "types.h"
#include "bits.h"

#define lbxgfx_get_w(_data_) GET_LE_16(&((_data_)[0x00]))
#define lbxgfx_get_h(_data_) GET_LE_16(&((_data_)[0x02]))
#define lbxgfx_get_frame(_data_) GET_LE_16(&((_data_)[0x04]))
#define lbxgfx_set_frame(_data_, _v_) SET_LE_16(&((_data_)[0x04]), (_v_))
#define lbxgfx_set_frame_0(_data_) lbxgfx_set_frame((_data_), 0)
#define lbxgfx_get_frames(_data_) GET_LE_16(&((_data_)[0x06]))
#define lbxgfx_get_frames2(_data_) GET_LE_16(&((_data_)[0x08]))
#define lbxgfx_get_ehandle(_data_) ((_data_)[0x0a])
#define lbxgfx_get_epage(_data_) ((_data_)[0x0b])
#define lbxgfx_set_epage(_data_, _v_) ((_data_)[0x0b] = (_v_))
#define lbxgfx_get_offs0c(_data_) GET_LE_16(&((_data_)[0x0c]))
#define lbxgfx_get_paloffs(_data_) GET_LE_16(&((_data_)[0x0e]))
#define lbxgfx_has_palette(_data_) (lbxgfx_get_paloffs(_data_) != 0)
#define lbxgfx_get_indep(_data_) ((_data_)[0x10])
#define lbxgfx_get_format(_data_) ((_data_)[0x11])
#define lbxgfx_get_paldataoffs(_data_) GET_LE_16(&((_data_)[lbxgfx_get_paloffs(_data_)+0x00]))
#define lbxgfx_get_palfirst(_data_) GET_LE_16(&((_data_)[lbxgfx_get_paloffs(_data_)+0x02]))
#define lbxgfx_get_palnum(_data_) GET_LE_16(&((_data_)[lbxgfx_get_paloffs(_data_)+0x04]))
#define lbxgfx_get_paloffs06(_data_) GET_LE_16(&((_data_)[lbxgfx_get_paloffs(_data_)+0x06]))
#define lbxgfx_get_palptr(_data_) ((_data_) + lbxgfx_get_paldataoffs(_data_))
#define lbxgfx_get_frameoffs0(_data_, _frame_) (GET_LE_32(&((_data_)[0x12 + 4 * (_frame_)])))
#define lbxgfx_get_frameoffs1(_data_, _frame_) (GET_LE_32(&((_data_)[0x16 + 4 * (_frame_)])))
#define lbxgfx_get_frameptr(_data_, _frame_) ((_data_) + lbxgfx_get_frameoffs0((_data_), (_frame_)))
#define lbxgfx_get_frameclearflag(_data_, _frame_) (*lbxgfx_get_frameptr((_data_), (_frame_)))

extern void lbxgfx_draw_frame(int x, int y, uint8_t *data, uint16_t pitch, int scale);
extern void lbxgfx_draw_frame_pal(int x, int y, uint8_t *data, uint16_t pitch, int scale);
extern void lbxgfx_draw_frame_offs(int x, int y, uint8_t *data, int lx0, int ly0, int lx1, int ly1, uint16_t pitch, int scale);
extern void lbxgfx_draw_frame_offs_delay(int x, int y, bool next, uint8_t *data, int lx0, int ly0, int lx1, int ly1, uint16_t pitch, int scale);
extern void lbxgfx_draw_frame_do(uint8_t *p, uint8_t *data, uint16_t pitch, int scale);

extern void lbxgfx_set_new_frame(uint8_t *data, uint16_t newframe);

extern void lbxgfx_apply_colortable(int x0, int y0, int x1, int y1, uint8_t ctbli, uint16_t pitch, int scale);

extern void lbxgfx_apply_palette(uint8_t *data);

#endif
