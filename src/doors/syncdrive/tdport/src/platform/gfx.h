#pragma once
/* Planar EGA graphics — port of TDEGA image 0x4941..0x9A69 graphics routines (port/spec/platform.md §4.1–4.4a).
 *
 * Model: a draw *target* is the 12-word descriptor used by the original, stored in code-segment memory:
 *   +00 hdr_off, +02..+08 plane_seg[4] (0 = absent, 0xA000 = EGA screen), +0A rowtab (CS offset),
 *   +0C clip_x0, +0E clip_x1 (byte columns, exclusive), +10 clip_y0, +12 clip_y1 (rows, exclusive),
 *   +14 stride, +16 pad, +18 row table.
 * The selected target is copied to CS:5A64..5A7B (game code pokes the clip there). Plane segment 0xA000
 * addresses the four 40x200 EGA planes held by this module; any other segment is RAM in mem[].
 * Sprites, off-screen buffers and targets are FarPtrs into mem[].
 */
#include "../mem.h"

#define GFX_SCREEN_DESC_OFF  EGA_CGA(0x5A7C, 0x5750)      /* CS: screen descriptor */
#define GFX_CUR_OFF          EGA_CGA(0x5A64, 0x5738)      /* CS: live copy of the selected descriptor */
#define GFX_CUR_CLIP_X0      EGA_CGA(0x5A70, 0x5744)
#define GFX_CUR_CLIP_X1      EGA_CGA(0x5A72, 0x5746)
#define GFX_CUR_CLIP_Y0      EGA_CGA(0x5A74, 0x5748)
#define GFX_CUR_CLIP_Y1      EGA_CGA(0x5A76, 0x574A)
#define GFX_CUR_STRIDE       EGA_CGA(0x5A78, 0x574C)
static inline FarPtr gfx_screen_desc(void) { return far_make(CODE_SEG, GFX_SCREEN_DESC_OFF); }

/* Port setup: EGA planes, descriptor pool/screen descriptor state, host frame source. Call once after
 * mem_load_exe() and host_init(). */
void gfx_init(void);

#if TD_CGA
/* TDCGA: the graphics layer is gfx_cga.c (CGA mode 4, 2 bits per pixel, screen at B800h).
 * main() calls these when started as "tdcga herc" (TD_HERC builds). */
void gfx_herc_init(void);                                               /* TDCGA 0x7587 */
void gfx_herc_shutdown(void);                                           /* TDCGA 0x75D7 */
/* TDCGA differences in the shared API (details: port/cga/graphics.md):
 *  - a target has one 2bpp bitmap (plane_seg[0]); clip x and byte columns are 4-pixel bytes (screen 0x50);
 *  - fill_rect / clear_clip / clear_screen take a 16-bit pattern word (one byte per row parity); the u8
 *    'colour' versions below pass colour * 0x0101, so colour is a 2bpp byte pattern (00/55/AA/FF, or
 *    e.g. 0x88) rather than a colour index. draw_line / draw_rect_outline 'colour' is the same kind of
 *    byte. gfx_set_text_colours still takes colour indices 0-3;
 *  - gfx_create_buffer ignores plane_mask; blit_*_own of the clipped families does not mask header x
 *    with 0xFFFC, the unclipped ones do;
 *  - gfx_dissolve and grab_into_sprite_raw work on the current target, not only the screen;
 *  - gfx_set_palette and gfx_scroll_window do not exist (TDCGA uses blit_copy_clip_raw to scroll). */
void gfx_fill_rect_pat(s16 x, s16 y, s16 w, s16 h, u16 pattern);        /* TDCGA 0x4857: even rows high byte */
void gfx_clear_screen_pat(u16 pattern);                                 /* TDCGA 0x4AD8 */
void gfx_clear_clip_pat(u16 pattern);                                   /* TDCGA 0x62ED: even rows low byte */
#endif
#if TD_HERC
/* Port option: phosphor colour of the emulated monochrome monitor ("green", "amber", "white").
 * Returns false for an unknown name. May be called before gfx_init(). */
bool gfx_set_monitor(const char *name);
#endif

/* Fills xrgb (320x200) from the EGA planes through the current palette; true if VRAM or the palette
 * changed since the last call. Installed with host_set_frame_source(). */
bool gfx_compose(u32 *xrgb);

/* ---- primitives */
void gfx_fill_rect(s16 x, s16 y, s16 w, s16 h, u8 colour);              /* 0x4941 */
void gfx_clear_screen(u8 colour);                                       /* 0x4B98 */
void gfx_draw_text(const char *s, s16 x, s16 y);                        /* 0x4BD0 */
void gfx_draw_text_at_cursor(const char *s);                            /* 0x4BE7 */
void gfx_set_palette(u16 ds_table);                                     /* 0x4D84 */
void gfx_init_ega(void);                                                /* 0x4D96 */
void gfx_set_clip(FarPtr desc, s16 x0, s16 x1, s16 y0, s16 y1);         /* 0x50A1 */
void gfx_shutdown(void);                                                /* 0x50FE */
void gfx_select_target(FarPtr desc);                                    /* 0x5128 */
FarPtr gfx_create_buffer(u16 w_bytes, u16 h, u16 plane_mask);           /* 0x5150 */
void gfx_set_text_cursor(s16 x, s16 y);                                 /* 0x74E3 */
void gfx_clear_clip(u8 colour);                                         /* 0x74F4 */
void gfx_set_text_colours(u16 fg, u16 bg);                              /* 0x6C85 */
void gfx_dissolve(FarPtr spr, u8 phase);                                /* 0x768B */
void gfx_free_buffer(FarPtr desc);                                      /* 0x79FF */
void gfx_draw_line(s16 x0, s16 y0, s16 x1, s16 y1, u8 colour);          /* 0x85D5 */
void gfx_grab_screen(s16 sx, s16 sy, s16 dx, s16 dy, s16 w_bytes, s16 h); /* 0x8B08 */
void grab_into_sprite_raw(FarPtr spr, s16 x, s16 y);                    /* 0x8C00 */
void gfx_scroll_window(s16 x, s16 y, s16 w, s16 h, s16 row_step, FarPtr spr, s16 srow); /* 0x91CF */
void draw_text_centered(const char *s, s16 y);                          /* 0x9507 */
void draw_rect_outline(s16 x0, s16 y0, s16 x1, s16 y1, u8 colour);      /* 0x9530 */
void draw_glyph(s16 x, s16 y, s16 idx);                                 /* 0x9A69 */
void gfx_target_save(void);                                             /* 0xA3B7 */
void gfx_target_restore(void);                                          /* 0xA3CB */

/* ---- sprite blitters: hot = at (x-hot_x, y-hot_y), raw = at (x, y), own = at header (x & ~3, y) */
void blit_copy_clip_hot(FarPtr spr, s16 x, s16 y);                      /* 0x5D84 REPLACE clipped */
void blit_copy_clip_raw(FarPtr spr, s16 x, s16 y);                      /* 0x5DB6 */
void blit_copy_clip_own(FarPtr spr);                                    /* 0x5DE2 */
void blit_or_clip_hot(FarPtr spr, s16 x, s16 y);                        /* 0x4DF1 OR clipped */
void blit_or_clip_raw(FarPtr spr, s16 x, s16 y);                        /* 0x4E05 */
void blit_or_clip_own(FarPtr spr);                                      /* 0x4E19 */
void blit_and_clip_hot(FarPtr spr, s16 x, s16 y);                       /* 0x8308 AND clipped */
void blit_and_clip_raw(FarPtr spr, s16 x, s16 y);                       /* 0x831C */
void blit_and_clip_own(FarPtr spr);                                     /* 0x8330 */
void blit_xor_clip_hot(FarPtr spr, s16 x, s16 y);                       /* 0x8058 XOR clipped */
void blit_xor_clip_raw(FarPtr spr, s16 x, s16 y);                       /* 0x806C */
void blit_xor_clip_own(FarPtr spr);                                     /* 0x8080 */
void blit_copy_hot(FarPtr spr, s16 x, s16 y);                           /* 0x6CBE REPLACE unclipped */
void blit_copy_raw(FarPtr spr, s16 x, s16 y);                           /* 0x6CF0 */
void blit_copy_own(FarPtr spr);                                         /* 0x6D1C */
void blit_or_hot(FarPtr spr, s16 x, s16 y);                             /* 0x7A37 OR unclipped */
void blit_or_raw(FarPtr spr, s16 x, s16 y);                             /* 0x7A4B */
void blit_or_own(FarPtr spr);                                           /* 0x7A5F */
void blit_and_hot(FarPtr spr, s16 x, s16 y);                            /* 0x7C3B AND unclipped */
void blit_and_raw(FarPtr spr, s16 x, s16 y);                            /* 0x7C4F */
void blit_and_own(FarPtr spr);                                          /* 0x7C63 */
void blit_xor_hot(FarPtr spr, s16 x, s16 y);                            /* 0x7E54 XOR unclipped */
void blit_xor_raw(FarPtr spr, s16 x, s16 y);                            /* 0x7E68 */
void blit_xor_own(FarPtr spr);                                          /* 0x7E7C */
