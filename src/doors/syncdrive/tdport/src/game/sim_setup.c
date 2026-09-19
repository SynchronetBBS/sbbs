/* simulation: stage setup 0x4792 and car_data_ptr 0x493D (port/spec/simulation.md §1, §2;
 * scene_render.md §3.2/§3.3 for the buffers and sprite handle tables filled here). */
#include "game.h"
#include "../platform/gfx.h"
#include "../platform/timer.h"
#include "../platform/res.h"

static inline u16 setlo(u16 w, u8 v) { return (u16)((w & 0xFF00) | v); }

/* 0x4792 stage_enter_install_isr — simulation.md §1 */
void stage_enter_install_isr(void)
{
    u16 ax, bx, cx, dx, si, di;
    FarPtr d, s;

    /* road buffer: 40 bytes x 112 rows, planes 0-2 (TDCGA: 80 bytes, 2bpp; the call has no third argument) */
    d = gfx_create_buffer(EGA_CGA(0x28, 0x50), 0x70, EGA_CGA(7, 0));
    far_wr(DGROUP, DS_road_buf_desc, d);
    ax = rd16(CODE_SEG, (u16)(d.off + 2));      /* mov ax, cs:[bx+2] */
    cx = rd16(CODE_SEG, d.off);                 /* mov cx, cs:[bx]   */
    dx = rd16(CODE_SEG, (u16)(d.off + 0xA));    /* mov dx, cs:[bx+0Ah] */
    DSW(DS_road_buf_sprite) = cx;
    DSW(DS_road_buf_sprite + 2) = ax;
    DSW(DS_road_buf_rowtab) = dx;

    /* sprite handle tables (scene_render.md §3.3) */
#if TD_CGA
    /* TDCGA 0x46F7: xroada list (116 names, no gvrm); xroadb.cmp (in g_xroadC) holds the xroadb and
     * xroadc sprites under one 132-name list, so the handles run on contiguously from DS:1146. */
    res_find_list(far_rd(DGROUP, DS_g_xroadA), (const char *)mp(DGROUP, 0x0AFE), DGROUP, DS_xroada_handles);
    res_find_list(far_rd(DGROUP, DS_g_xroadC), (const char *)mp(DGROUP, 0x0CCF), DGROUP, DS_xroadb_handles);
#else
    res_find_list(far_rd(DGROUP, DS_g_xroadA), (const char *)mp(DGROUP, 0x0B16), DGROUP, DS_xroada_handles);
    res_find_list(far_rd(DGROUP, DS_g_xroadB), (const char *)mp(DGROUP, 0x0CEB), DGROUP, DS_xroadb_handles);
    res_find_list(far_rd(DGROUP, DS_g_xroadC), (const char *)mp(DGROUP, 0x0DB4), DGROUP, DS_xroadc_handles);
#endif
    res_find_list(far_rd(DGROUP, DS_g_carArchive), (const char *)mp(DGROUP, EGA_CGA(0x0EFD, 0x0EE0)), DGROUP, DS_car_handles);
    if (DSW(EGA_CGA(0x27FB, 0x27CE)) /* car+16C needle gauges */ != 1)   /* TDCGA list: 0-9, spdo, tach */
        res_find_list(far_rd(DGROUP, DS_g_carArchive), (const char *)mp(DGROUP, EGA_CGA(0x0F62, 0x0F45)), DGROUP, DS_digit_handles);

    /* instrument buffer from the `inst` sprite header (car_handles k13, DS:13B7) */
    s = far_rd(DGROUP, DS_car_handles + 13 * 4);
    ax = rd16(s.seg, (u16)(s.off + 8));
    cx = rd16(s.seg, (u16)(s.off + 0xA));
    si = rd16(s.seg, (u16)(s.off + 2));
    di = rd16(s.seg, s.off);
    DSW(DS_inst_x) = ax;
    bx = DSB(EGA_CGA(0x27FD, 0x27D0));          /* xor bh,bh; mov bl,[27FD] */
    bx = (u16)(bx - ax);
    DSW(DS_speedo_pivot) = bx;
    bx = setlo(bx, DSB(EGA_CGA(0x27FF, 0x27D2)));                /* bh kept from the previous result */
    bx = (u16)(bx - ax);
    DSW(DS_tach_pivot) = bx;
    DSW(DS_inst_y) = cx;
    bx = setlo(bx, DSB(EGA_CGA(0x27FE, 0x27D1)));
    bx = (u16)(bx - cx);
    DSW(DS_speedo_pivot + 2) = bx;
    bx = setlo(bx, DSB(EGA_CGA(0x2800, 0x27D3)));
    bx = (u16)(bx - cx);
    DSW(DS_tach_pivot + 2) = bx;
    DSW(DS_inst_h) = si;
    DSW(DS_inst_wbytes) = di;
    d = gfx_create_buffer(di, si, EGA_CGA(0xF, 0));      /* TDCGA: two arguments */
    far_wr(DGROUP, DS_buf_desc_b, d);
    DSW(DS_inst_buf_sprite) = rd16(d.seg, d.off);
    DSW(DS_inst_buf_sprite + 2) = rd16(d.seg, (u16)(d.off + 2));

    /* gear box buffer from the `gbox` sprite header (car_handles k4, DS:1393) */
    s = far_rd(DGROUP, DS_car_handles + 4 * 4);
    DSW(DS_gbox_x) = rd16(s.seg, (u16)(s.off + 8));
    DSW(DS_gbox_y) = rd16(s.seg, (u16)(s.off + 0xA));
    si = rd16(s.seg, (u16)(s.off + 2));
    di = rd16(s.seg, s.off);
    d = gfx_create_buffer(di, si, EGA_CGA(0xF, 0));      /* TDCGA: two arguments */
    far_wr(DGROUP, DS_buf_desc_c, d);
    DSW(DS_gbox_buf_sprite) = rd16(d.seg, d.off);
    DSW(DS_gbox_buf_sprite + 2) = rd16(d.seg, (u16)(d.off + 2));

    timer_install_drive();
    snd_set_loop(far_make(DGROUP, DS_SONG_INGAME));

    /* PORT: the original saves INT 0 into DS:0934, INT 8 (now 0x6A1F) into CS:3B18, then installs the
     * divide-error handler DGROUP:1423 and 0x3B1F on INT 8. The INT 0 behaviour lives in the division
     * helpers (mem.h); chaining to the old INT 8 is sim_timer_isr calling timer_isr(). */
    timer_set_driving_isr(sim_timer_isr);
}

/* 0x493D car_data_ptr */
u16 car_data_ptr(void)
{
    return EGA_CGA(0x268F, 0x2662);
}
