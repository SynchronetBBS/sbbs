#pragma once
/* Private helpers of the scene_render module (game/scene*.c). Not a shared header. */
#include "game.h"
#include "../platform/gfx.h"

/* Far pointer stored in DGROUP (sprite handle tables, buffer sprites). */
static inline FarPtr scene_ds_far(u16 off) { return far_rd(DGROUP, off); }

/* Plane segment k of the current target copy (CS:5A66 + 2k). EGA only: TDCGA's road code writes the
 * 2bpp road buffer in segment DS:[0896] (scene_road_seg). */
static inline u16 scene_cur_plane(int k) { return CSW((u16)(GFX_CUR_OFF + 2 + 2 * k)); }
static inline u16 scene_road_seg(void)   { return DSW((u16)(DS_road_buf_sprite + 2)); }

/* Row offset of row y in the current road buffer: CS:[DS:0898 + 2y]. */
static inline u16 scene_rowtab(u16 y2) { return CSW((u16)(DSW(DS_road_buf_rowtab) + y2)); }

/* fill_scenery / mirror background helpers: `rep stosb` of 0xFF, DI wraps inside the segment. */
static inline u16 scene_stos_ff(u16 seg, u16 di, u16 nbytes)
{
    while (nbytes--) { wr8(seg, di, 0xFF); di++; }
    return di;
}
