#ifndef INC_1OOM_GFXAUX_H
#define INC_1OOM_GFXAUX_H

#include "types.h"

struct gfx_aux_s {
    int w;
    int h;
    int frame;
    int size;
    uint8_t *data;
};

extern void gfx_aux_setup_wh(struct gfx_aux_s *aux, int w, int h);
extern void gfx_aux_setup(struct gfx_aux_s *aux, const uint8_t *data, int frame);
extern void gfx_aux_free(struct gfx_aux_s *aux);
extern void gfx_aux_flipx(struct gfx_aux_s *aux);
extern void gfx_aux_scale(struct gfx_aux_s *aux, int xscale, int yscale);
extern void gfx_aux_color_replace(struct gfx_aux_s *aux, uint8_t from, uint8_t to);
extern void gfx_aux_color_non0(struct gfx_aux_s *aux, uint8_t color);
extern void gfx_aux_recolor_ctbl(struct gfx_aux_s *aux, const uint8_t *ctbl, int ctbllen);
extern void gfx_aux_overlay(int x, int y, struct gfx_aux_s *dest, struct gfx_aux_s *src);
extern void gfx_aux_overlay_clear_unused(int x, int y, struct gfx_aux_s *dest, struct gfx_aux_s *src);
extern void gfx_aux_copy(struct gfx_aux_s *dest, struct gfx_aux_s *src);
extern void gfx_aux_draw_cloak(struct gfx_aux_s *aux, uint8_t percent, uint16_t rndv);
extern void gfx_aux_draw_frame_to(uint8_t *data, struct gfx_aux_s *aux);
extern void gfx_aux_draw_frame_from(int x, int y, struct gfx_aux_s *aux, uint16_t pitch, int scale);
extern void gfx_aux_draw_frame_from_limit(int x, int y, struct gfx_aux_s *aux, int lx0, int ly0, int lx1, int ly1, uint16_t pitch, int scale);
extern void gfx_aux_draw_frame_from_rotate_limit(int x0, int y0, int x1, int y1, struct gfx_aux_s *aux, int lx0, int ly0, int lx1, int ly1, uint16_t pitch, int scale);

#endif
