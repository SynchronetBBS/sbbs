#ifndef BITMAP_CON_H
#define BITMAP_CON_H

#include "vidmodes.h"
#include "threadwrap.h"

extern struct video_stats vstat;
extern pthread_mutex_t vstatlock;

#ifndef BITMAP_CIOLIB_DRIVER
/* Called from ciolib */
int bitmap_puttext(int sx, int sy, int ex, int ey, void *fill);
int bitmap_pputtext(int sx, int sy, int ex, int ey, void *fill, uint32_t *fg, uint32_t *bg);
int bitmap_pgettext(int sx, int sy, int ex, int ey, void *fill, uint32_t *fg, uint32_t *bg);
void bitmap_gotoxy(int x, int y);
void bitmap_setcursortype(int type);
int bitmap_setfont(int font, int force, int font_no);
int bitmap_getfont(void);
int bitmap_loadfont(char *filename);
int bitmap_movetext(int x, int y, int ex, int ey, int tox, int toy);
void bitmap_clreol(void);
void bitmap_clrscr(void);
void bitmap_getcustomcursor(int *s, int *e, int *r, int *b, int *v);
void bitmap_setcustomcursor(int s, int e, int r, int b, int v);
int bitmap_getvideoflags(void);
void bitmap_setvideoflags(int flags);
int bitmap_attr2palette(uint8_t attr, uint32_t *fgp, uint32_t *bgp);
int bitmap_setpixel(uint32_t x, uint32_t y, uint32_t colour);
int bitmap_setpixels(uint32_t sx, uint32_t sy, uint32_t ex, uint32_t ey, uint32_t x_off, uint32_t y_off, struct ciolib_pixels *, void *mask);
struct ciolib_pixels *bitmap_getpixels(uint32_t sx, uint32_t sy, uint32_t ex, uint32_t ey);
uint32_t *bitmap_get_modepalette(uint32_t p[16]);
int bitmap_set_modepalette(uint32_t p[16]);
#endif

#ifdef BITMAP_CIOLIB_DRIVER
/* Called from drivers */
int bitmap_drv_init_mode(int mode, int *width, int *height);
int bitmap_drv_init(void (*drawrect_cb) (int xpos, int ypos, int width, int height, uint32_t *data)
				,void (*flush) (void));
void bitmap_drv_request_pixels(void);
void bitmap_drv_request_some_pixels(int x, int y, int width, int height);
#endif

#endif
