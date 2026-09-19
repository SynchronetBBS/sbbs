/* scene_render: cockpit — gear box, radar detector, steering wheel, wheel marker, instruments.
 * Port of TDEGA image 0x351A..0x38EA (port/spec/scene_render.md §4.8). */
#include "scene.h"
#include "../platform/timer.h"

/* 0x38CD draw_digit (BL = digit, DX = screen x): OR blit DIGITS[BL] at (x - inst_x, 0x20). */
static void draw_digit(u8 bl, u16 dx)
{
    dx = (u16)(dx - DSW(DS_inst_x));
    u16 bx = (u16)((u16)bl << 2);                    /* digits >= 15 read past the table (as the asm) */
    blit_or_hot(scene_ds_far((u16)(DS_digit_handles + bx)), (s16)dx, 0x20);
}

/* 0x351A draw_gear_box */
void draw_gear_box(void)
{
    u8 al = DSB(DS_gearbox_display_toggle);
    if (al != DSB(DS_gearbox_prev)) {
        DSB(DS_gearbox_prev) = al;
        if (al == 1) goto redraw;
        goto closed;
    }
    if (DSB(DS_gearbox_display_toggle) != 1) {
        if (DSB(DS_dash_timer) == 0) return;
        if (--DSB(DS_dash_timer) == 0) goto closed;
    }
    if (DSB(DS_gearbox_dirty) != 1) return;
    goto redraw;

closed:
    select_screen();
    blit_copy_own(scene_ds_far(EGA_CGA(0x138F, 0x1362)));                                 /* CAR gbo0 */
    return;

redraw:
    DSB(DS_gearbox_dirty) = 0;
    gfx_select_target(far_make(CODE_SEG, DSW(DS_buf_desc_c)));
    blit_copy_hot(scene_ds_far(EGA_CGA(0x1393, 0x1366)), 0, 0);                           /* CAR gbox */
    {
        s16 x = (s16)(u16)(DSW(DS_knob_x) - DSW(DS_gbox_x));
        s16 y = (s16)(u16)(DSW(DS_knob_y) - DSW(DS_gbox_y));
        blit_and_clip_hot(scene_ds_far(EGA_CGA(0x139B, 0x136E)), x, y);                   /* gnab */
        blit_or_clip_hot(scene_ds_far(EGA_CGA(0x1397, 0x136A)), x, y);                    /* gnob, same stacked x,y */
    }
    select_screen();
    blit_copy_hot(scene_ds_far(DS_gbox_buf_sprite), (s16)DSW(DS_gbox_x), (s16)DSW(DS_gbox_y));
}

/* 0x35C6 draw_dashboard_dynamic (target = screen) */
void draw_dashboard_dynamic(void)
{
    /* ---- radar detector */
    FarPtr rad;
    if (DSB(DS_g_stageTime) & 8) {                                        /* DS:80A4 tick count */
        u16 bx = DSW(DS_snapshot_radar_trap_pos);                         /* slot 11 snapshot */
        bool lit = false;
        if (bx != 0) {
            bx--;
            if (!((s16)bx < (s16)DSW(DS_r_road_ptr))) {                   /* sub bx,[186F] ; jl */
                bx = (u16)(bx - DSW(DS_r_road_ptr));
                snd_play_oneshot(far_make(DGROUP, EGA_CGA(0x0B0F, 0x0AF7)));
                bx = (u16)((bx >> 2) & 0xFC);
                rad = scene_ds_far((u16)(EGA_CGA(0x13C3, 0x1396) + bx));                  /* rad5, rad4, ... */
                lit = true;
            }
        }
        if (!lit) rad = scene_ds_far(EGA_CGA(0x13BF, 0x1392));                             /* rad0 */
    } else {
        rad = scene_ds_far(EGA_CGA(0x13D7, 0x13AA));                                       /* radb */
    }
    blit_copy_own(rad);

    /* ---- steering wheel pose (XOR toggles) */
    {
        s16 st = DSS(DS_steer_angle);
        u8 bl = st > 0 ? 0 : (st == 0 ? 1 : 2);
        if (bl != DSB(DS_wheel_state)) {
            u8 cl = (u8)(bl | DSB(DS_wheel_state));
            if (!(cl & 1)) bl = 1;                                        /* never 0 <-> 2 in one frame */
            cl = bl;
            bl = (u8)((bl | DSB(DS_wheel_state)) & 2);
            DSB(DS_wheel_state) = cl;
            FarPtr w = scene_ds_far((u16)(EGA_CGA(0x13DF, 0x13B2) + ((u16)bl << 1)));     /* whl1 / whl3 */
            select_screen();
            blit_xor_own(w);
        }
    }

    /* ---- wheel marker on a 90 px radius, with save-under */
    {
        u16 bx = DSW(DS_steer_angle);
        u8 neg = 0;
        if ((s16)bx < 0) { bx = (u16)-bx; neg = 1; }
        bx = (u16)(bx << 2);
        u8 k = (u8)(bx >> 8);
        if (!((s8)k < 0x3C)) k = 0x3C;                   /* k == 60 reads one past wheel_sin90 (quirk) */
        u16 dx = DSB((u16)(DS_wheel_sin90 + k));
        if (!neg) dx = (u16)-dx;
        u16 ax = DSW(EGA_CGA(0x269D, 0x2670));                                               /* car wheel y */
        ax = (u16)((ax & 0xFF00) | (u8)((u8)ax - DSB((u16)(DS_wheel_cos90 + k))));
        dx = (u16)(dx + DSW(EGA_CGA(0x269B, 0x266E)));                                       /* car wheel x */

        u16 sy = (u16)(ax - 2);
        u16 sx = (u16)((u16)(dx - 2) & EGA_CGA(0xFFF8, 0xFFFC));   /* byte-aligned save-under */
        u16 oldx = DSW(DS_marker_save_x), oldy = DSW(DS_marker_save_y);
        DSW(DS_marker_save_y) = sy;
        DSW(DS_marker_save_x) = sx;
        FarPtr save = far_make(DGROUP, DS_marker_save_sprite);
        if (DSB(DS_marker_saved) != 0) blit_copy_hot(save, (s16)oldx, (s16)oldy);
        grab_into_sprite_raw(save, (s16)sx, (s16)sy);
        blit_and_hot(scene_ds_far(EGA_CGA(0x138B, 0x135E)), (s16)dx, (s16)ax);             /* dota */
        blit_or_hot(scene_ds_far(EGA_CGA(0x1387, 0x135A)), (s16)dx, (s16)ax);              /* dot  */
        DSB(DS_marker_saved) = 1;
    }

    /* ---- instruments (only when flagged) */
    if (DSB(DS_gauges_dirty) != 1) return;
    DSB(DS_gauges_dirty) = 0;
    gfx_select_target(far_make(CODE_SEG, DSW(DS_buf_desc_b)));
    blit_copy_hot(scene_ds_far(EGA_CGA(0x13B7, 0x138A)), 0, 0);                            /* inst */

    if (DSW(EGA_CGA(0x27FB, 0x27CE)) == 1) {                                                 /* needle gauges */
        u16 bx = DSB(DS_g_carSpeedFixed + 1);                               /* mph (DS:0928) */
        if (DSW(DS_g_selectedCar) == 2) {                                   /* units_mode */
            bool ge = (s16)bx >= 0x12;
            bx = (u16)(bx - 0x12);
            if (!ge) bx = 0;
        }
        if ((s16)bx >= 0xA0) bx = 0xA0;
        u16 ax = DSW((u16)(EGA_CGA(0x2809, 0x27DC) + (bx << 1)));
        u16 by = (u16)(ax >> 8);
        ax &= 0xFF;
        ax = (u16)(ax - DSW(DS_inst_x));
        by = (u16)(by - DSW(DS_inst_y));
        gfx_draw_line((s16)DSW(DS_speedo_pivot), (s16)DSW(DS_speedo_pivot + 2), (s16)ax, (s16)by, 0xFF);

        bx = DSW(DS_rpm);
        if (!(bx < DSW(DS_car_rpm_limit))) bx = DSW(DS_car_rpm_limit);
        bx = (u16)((bx >> 6) << 1);
        ax = DSW((u16)(EGA_CGA(0x29B7, 0x298A) + bx));
        by = (u16)(ax >> 8);
        ax &= 0xFF;
        ax = (u16)(ax - DSW(DS_inst_x));
        by = (u16)(by - DSW(DS_inst_y));
        gfx_draw_line((s16)DSW(DS_tach_pivot), (s16)DSW(DS_tach_pivot + 2), (s16)ax, (s16)by, 0xFF);

        u16 w = (u16)((u16)DSB(DS_wheel_state) << 3);
        FarPtr inl = scene_ds_far((u16)(EGA_CGA(0x139F, 0x1372) + w));
        FarPtr ina = scene_ds_far((u16)(EGA_CGA(0x13A3, 0x1376) + w));
        blit_and_own(ina);
        blit_or_own(inl);
    } else {                                                                /* digital cluster */
        u8 al = DSB(DS_g_carSpeedFixed + 1);
        if (al >= 0x56) al = 0x56;
        u16 ax = (u16)(div16_8((u16)((u16)al << 1), 5) & 0xFF);
        ax = (u16)-(u16)(ax + 1);
        ax = (u16)(ax + DSW(DS_inst_h));
        CSW(GFX_CUR_CLIP_Y0) = ax;
        blit_or_clip_own(scene_ds_far(EGA_CGA(0x140F, 0x13E2)));                           /* spdo bar */
        CSW(GFX_CUR_CLIP_Y0) = 0;

        ax = DSW(DS_rpm);
        if (ax >= 0x1C20) ax = 0x1C20;
        ax = (u16)(ax >> 3);
        ax = (u16)(div16_8(ax, 100) & 0xFF);
#if TD_CGA
        /* TDCGA 0x3785: one tach bar, always clipped to rpm/800 of 9 bytes */
        CSW(GFX_CUR_CLIP_X1) = (u16)(DSW(DS_inst_wbytes) + ax - 9);
        blit_or_clip_own(scene_ds_far(0x13E6));
#else
        u16 bx = 0x1413;
        bool cf = (ax & 1) != 0;
        ax = (u16)((s16)ax >> 1);
        if (cf || ax == 0 || (u8)ax == 4)
            CSW(GFX_CUR_CLIP_X1) = (u16)(DSW(DS_inst_wbytes) + ax - 4);
        else
            bx = (u16)(bx + (ax << 2));                                     /* tac1..tac3 */
        blit_or_clip_own(scene_ds_far(bx));
#endif
        CSW(GFX_CUR_CLIP_X1) = DSW(DS_inst_wbytes);

        al = DSB(DS_g_carSpeedFixed + 1);
        u16 qr;
        if (al >= 0x64) {
            al = (u8)(al - 0x64);
            draw_digit(1, 0x4C);
            qr = div16_8(al, 10);
            draw_digit((u8)qr, 0x52);
        } else {
            qr = div16_8(al, 10);
            if ((u8)qr != 0) draw_digit((u8)qr, 0x52);
        }
        draw_digit((u8)(qr >> 8), 0x58);

        ax = DSW(DS_rpm);
        if (ax >= 0x2710) ax = 0x270F;
        ax = (u16)(div16_8(ax, 100) & 0xFF);
        qr = div16_8(ax, 10);
        if ((u8)qr != 0) draw_digit((u8)qr, 0xB9);
        draw_digit((u8)(qr >> 8), 0xBF);
    }
    select_screen();
    blit_copy_hot(scene_ds_far(DS_inst_buf_sprite), (s16)DSW(DS_inst_x), (s16)DSW(DS_inst_y));
}
