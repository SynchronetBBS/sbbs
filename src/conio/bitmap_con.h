#ifndef BITMAP_CON_H
#define BITMAP_CON_H

#include "vidmodes.h"
#include "threadwrap.h"

struct rectangle {
	int x;
	int y;
	int width;
	int height;
};

struct rectlist {
	struct rectangle rect;
	uint32_t *data;
	struct rectlist *next;
	bool throttle;
};

extern struct video_stats vstat;
extern pthread_mutex_t vstatlock;

#ifdef BITMAP_CIOLIB_DRIVER
/* Called from drivers */
int bitmap_drv_init_mode(int mode, int *width, int *height, int maxwidth, int maxheight);
int bitmap_drv_init(void (*drawrect_cb) (struct rectlist *data)
				,void (*flush) (void));
void bitmap_drv_request_pixels(void);
void bitmap_drv_request_some_pixels(int x, int y, int width, int height);
void bitmap_drv_free_rect(struct rectlist *rect);
void bitmap_snap(bool grow, int maxwidth, int maxheight);
void bitmap_get_scaled_win_size(double scale, int *w, int *h, int maxwidth, int maxheight);
int bitmap_largest_mult_inside(int maxwidth, int maxheight);
double bitmap_double_mult_inside(int maxwidth, int maxheight);
#else
/* Called from ciolib */
int bitmap_puttext(int sx, int sy, int ex, int ey, void *fill);
int bitmap_vmem_puttext(int sx, int sy, int ex, int ey, struct vmem_cell *fill);
int bitmap_vmem_gettext(int sx, int sy, int ex, int ey, struct vmem_cell *fill);
void bitmap_gotoxy(int x, int y);
void bitmap_setcursortype(int type);
int bitmap_setfont(int font, int force, int font_no);
int bitmap_getfont(int fnum);
int bitmap_loadfont(const char *filename);
int bitmap_movetext(int x, int y, int ex, int ey, int tox, int toy);
void bitmap_clreol(void);
void bitmap_clrscr(void);
void bitmap_getcustomcursor(int *s, int *e, int *r, int *b, int *v);
void bitmap_setcustomcursor(int s, int e, int r, int b, int v);
int bitmap_getvideoflags(void);
void bitmap_setvideoflags(int flags);
int bitmap_attr2palette(uint8_t attr, uint32_t *fgp, uint32_t *bgp);
int bitmap_setpixel(uint32_t x, uint32_t y, uint32_t colour);
int bitmap_setpixels(uint32_t sx, uint32_t sy, uint32_t ex, uint32_t ey, uint32_t x_off, uint32_t y_off, uint32_t mx_off, uint32_t my_off, struct ciolib_pixels *, struct ciolib_mask *mask);
struct ciolib_pixels *bitmap_getpixels(uint32_t sx, uint32_t sy, uint32_t ex, uint32_t ey, int force);
int bitmap_get_modepalette(uint32_t p[16]);
int bitmap_set_modepalette(uint32_t p[16]);
uint32_t bitmap_map_rgb(uint16_t r, uint16_t g, uint16_t b);
void bitmap_replace_font(uint8_t id, char *name, void *data, size_t size);
int bitmap_setpalette(uint32_t index, uint16_t r, uint16_t g, uint16_t b);
#endif

#endif
