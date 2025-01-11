/* $Id: bitmap_con.c,v 1.148 2020/06/27 00:04:44 deuce Exp $ */

#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>		/* NULL */
#include <stdlib.h>
#include <string.h>

#include "threadwrap.h"
#include "semwrap.h"
#include "gen_defs.h"
#include "genwrap.h"
#include "dirwrap.h"
#include "xpbeep.h"
#include "scale.h"

#if (defined CIOLIB_IMPORTS)
 #undef CIOLIB_IMPORTS
#endif
#if (defined CIOLIB_EXPORTS)
 #undef CIOLIB_EXPORTS
#endif

#include "ciolib.h"
#include "vidmodes.h"
#include "bitmap_con.h"

#ifdef _MSC_VER
#pragma warning(disable : 4244 4267 4018)
#endif

static uint32_t palette[65536];

#if 0

static int dbg_pthread_mutex_lock(pthread_mutex_t *lptr, unsigned line)
{
	int ret = pthread_mutex_lock(lptr);

	if (ret)
		fprintf(stderr, "pthread_mutex_lock() returned %d at %u\n", ret, line);
	return ret;
}

static int dbg_pthread_mutex_unlock(pthread_mutex_t *lptr, unsigned line)
{
	int ret = pthread_mutex_unlock(lptr);

	if (ret)
		fprintf(stderr, "pthread_mutex_lock() returned %d at %u\n", ret, line);
	return ret;
}

static int dbg_pthread_mutex_trylock(pthread_mutex_t *lptr, unsigned line)
{
	int ret = pthread_mutex_trylock(lptr);

	if (ret)
		fprintf(stderr, "pthread_mutex_lock() returned %d at %u\n", ret, line);
	return ret;
}

#define pthread_mutex_lock(a)		dbg_pthread_mutex_lock(a, __LINE__)
#define pthread_mutex_unlock(a)		dbg_pthread_mutex_unlock(a, __LINE__)
#define pthread_mutex_trylock(a)	dbg_pthread_trymutex_lock(a, __LINE__)

#endif

/* Structs */

pthread_mutex_t screenlock;
struct bitmap_screen {
	//uint32_t *screen;
	int		screenwidth;
	int		screenheight;
	int		toprow;
	int		update_pixels;
	struct rectlist *rect;
};

struct bitmap_callbacks {
	void	(*drawrect)		(struct rectlist *data);
	void	(*flush)		(void);
	pthread_mutex_t lock;
	unsigned rects;
};

/* Static globals */

static int default_font=-99;
static int current_font[4]={-99, -99, -99, -99};
static int bitmap_initialized=0;
static struct bitmap_screen screena;
static struct bitmap_screen screenb;
struct video_stats vstat;
static struct bitmap_callbacks callbacks;
static unsigned char *font[4];
static int force_redraws=0;
static int force_cursor=0;
static struct rectlist *free_rects;
static pthread_mutex_t free_rect_lock;
static bool throttled;
static struct vmem_cell *bitmap_drawn;
static int outstanding_rects;
// win32gdi requires two rects...
#define MAX_OUTSTANDING 2

/* Exported globals */

pthread_mutex_t		vstatlock;

/* Forward declarations */

static int bitmap_loadfont_locked(const char *filename);
static struct vmem_cell *set_vmem_cell(struct vstat_vmem *vmem_ptr, size_t pos, uint16_t cell, uint32_t fg, uint32_t bg);
static int bitmap_attr2palette_locked(uint8_t attr, uint32_t *fgp, uint32_t *bgp);
static void	cb_drawrect(struct rectlist *data);
static void request_redraw_locked(void);
static void request_redraw(void);
static void memset_u32(void *buf, uint32_t u, size_t len);
static void cb_flush(void);
static int check_redraw(void);
static void blinker_thread(void *data);
static __inline void both_screens(int blink, struct bitmap_screen** current, struct bitmap_screen** noncurrent);
static int update_from_vmem(int force);
static uint32_t color_value(uint32_t col);
void bitmap_drv_free_rect(struct rectlist *rect);
static void bitmap_draw_vmem(int sx, int sy, int ex, int ey, struct vmem_cell *fill);

/**************************************************************/
/* These functions get called from the driver and ciolib only */
/**************************************************************/

// vstatlock must be held
static int bitmap_loadfont_locked(const char *filename)
{
	static char current_filename[MAX_PATH];
	unsigned int fontsize;
	int fdw;
	int fw;
	int fh;
	int i;
	FILE	*fontfile=NULL;

	if(!bitmap_initialized)
		return(0);
	if(current_font[0]==-99 || current_font[0]>(sizeof(conio_fontdata)/sizeof(struct conio_font_data_struct)-2)) {
		for(i=0; conio_fontdata[i].desc != NULL; i++) {
			if(!strcmp(conio_fontdata[i].desc, "Codepage 437 English")) {
				current_font[0]=i;
				break;
			}
		}
		if(conio_fontdata[i].desc==NULL)
			current_font[0]=0;
	}
	if(current_font[0]==-1)
		filename=current_filename;
	else if(conio_fontdata[current_font[0]].desc==NULL)
		return(0);

	for (i=1; i<sizeof(current_font)/sizeof(current_font[0]); i++) {
		if(current_font[i] == -1)
			;
		else if (current_font[i] < 0)
			current_font[i]=current_font[0];
		else if(conio_fontdata[current_font[i]].desc==NULL)
			current_font[i]=current_font[0];
	}

	fh=vstat.charheight;
	fdw = vstat.charwidth - ((vstat.flags & VIDMODES_FLAG_EXPAND) ? 1 : 0);
	fw = fdw / 8 + (fdw % 8 ? 1 : 0);

	fontsize=fw*fh*256*sizeof(unsigned char);

	for (i=0; i<sizeof(font)/sizeof(font[0]); i++) {
		if(font[i])
			FREE_AND_NULL(font[i]);
		if((font[i]=(unsigned char *)malloc(fontsize))==NULL)
			goto error_return;
	}

	if(filename != NULL) {
		if(flength(filename)!=fontsize)
			goto error_return;
		if((fontfile=fopen(filename,"rb"))==NULL)
			goto error_return;
		if(fread(font[0], 1, fontsize, fontfile)!=fontsize)
			goto error_return;
		fclose(fontfile);
		fontfile=NULL;
		current_font[0]=-1;
		if(filename != current_filename)
			SAFECOPY(current_filename,filename);
		for (i=1; i<sizeof(font)/sizeof(font[0]); i++) {
			if (current_font[i]==-1)
				memcpy(font[i], font[0], fontsize);
		}
	}
	for (i=0; i<sizeof(font)/sizeof(font[0]); i++) {
		if (current_font[i] < 0)
			continue;
		switch(fdw) {
			case 8:
				switch(vstat.charheight) {
					case 8:
						if(conio_fontdata[current_font[i]].eight_by_eight==NULL) {
							if (i==0)
								goto error_return;
							else
								FREE_AND_NULL(font[i]);
						}
						else
							memcpy(font[i], conio_fontdata[current_font[i]].eight_by_eight, fontsize);
						break;
					case 14:
						if(conio_fontdata[current_font[i]].eight_by_fourteen==NULL) {
							if (i==0)
								goto error_return;
							else
								FREE_AND_NULL(font[i]);
						}
						else
							memcpy(font[i], conio_fontdata[current_font[i]].eight_by_fourteen, fontsize);
						break;
					case 16:
						if(conio_fontdata[current_font[i]].eight_by_sixteen==NULL) {
							if (i==0)
								goto error_return;
							else
								FREE_AND_NULL(font[i]);
						}
						else
							memcpy(font[i], conio_fontdata[current_font[i]].eight_by_sixteen, fontsize);
						break;
					default:
						goto error_return;
				}
				break;
			default:
				goto error_return;
		}
	}

    return(1);

error_return:
	for (i=0; i<sizeof(font)/sizeof(font[0]); i++)
		FREE_AND_NULL(font[i]);
	if(fontfile)
		fclose(fontfile);
	return(0);
}

/***************************************************/
/* These functions get called from the driver only */
/***************************************************/

/***********************************************/
/* These functions get called from ciolib only */
/***********************************************/

static int
bitmap_vmem_puttext_locked(int sx, int sy, int ex, int ey, struct vmem_cell *fill)
{
	int x,y;
	struct vstat_vmem *vmem_ptr;
	struct vmem_cell *vc;
	struct vmem_cell *fi = fill;

	if(!bitmap_initialized)
		return(0);

	if(		   sx < 1
			|| sy < 1
			|| ex < 1
			|| ey < 1
			|| sx > ex
			|| sy > ey
			|| ex > vstat.cols
			|| ey > vstat.rows
			|| fill==NULL) {
		return(0);
	}

	vmem_ptr = get_vmem(&vstat);
	for(y=sy-1;y<ey;y++) {
		for(x=sx-1;x<ex;x++) {
			vc = &vmem_ptr->vmem[y*vstat.cols+x];
			memcpy(vc, fi++, sizeof(*fi));
		}
	}
	release_vmem(vmem_ptr);
	return(1);
}

// vstatlock must be held
static struct vmem_cell *
set_vmem_cell(struct vstat_vmem *vmem_ptr, size_t pos, uint16_t cell, uint32_t fg, uint32_t bg)
{
	int		altfont;
	int		font;

	bitmap_attr2palette_locked(cell>>8, fg == 0xffffff ? &fg : NULL, bg == 0xffffff ? &bg : NULL);

	altfont = (cell>>11 & 0x01) | ((cell>>14) & 0x02);
	if (!vstat.bright_altcharset)
		altfont &= ~0x01;
	if (!vstat.blink_altcharset)
		altfont &= ~0x02;
	font=current_font[altfont];
	if (font == -99)
		font = default_font;
	if (font < 0 || font > 255)
		font = 0;

	vmem_ptr->vmem[pos].legacy_attr = cell >> 8;
	vmem_ptr->vmem[pos].ch = cell & 0xff;
	vmem_ptr->vmem[pos].fg = fg;
	vmem_ptr->vmem[pos].bg = bg;
	vmem_ptr->vmem[pos].font = font;
	return &vmem_ptr->vmem[pos];
}

static int bitmap_attr2palette_locked(uint8_t attr, uint32_t *fgp, uint32_t *bgp)
{
	uint32_t fg = attr & 0x0f;
	uint32_t bg = (attr >> 4) & 0x0f;

	if(!vstat.bright_background)
		bg &= 0x07;
	if(vstat.no_bright)
		fg &= 0x07;

	if (fgp)
		*fgp = vstat.palette[fg];
	if (bgp)
		*bgp = vstat.palette[bg];

	return 1;
}

/**********************************************************************/
/* These functions get called from ciolib and the blinker thread only */
/**********************************************************************/

static int
cursor_visible_locked(void)
{
	if (!vstat.curs_visible)
		return 0;
	if (vstat.curs_start > vstat.curs_end)
		return 0;
	if (vstat.curs_blinks) {
		if (vstat.curs_blink)
			return 1;
		return 0;
	}
	return 1;
}

static void	cb_drawrect(struct rectlist *data)
{
	int x, y;
	uint32_t *pixel;
	uint32_t cv;
	int curs_start;
	int curs_end;
	int curs_row;
	int curs_col;
	int charheight;
	int charwidth;

	if (data == NULL)
		return;
	/*
	 * Draw the cursor if it's visible
	 * 1) It's located at vstat.curs_col/vstat.curs_row.
	 * 2) The size is defined by vstat.curs_start and vstat.curs_end...
	 *    the are both rows from the top of the cell.
	 *    If vstat.curs_start > vstat.curs_end, the cursor is not shown.
	 * 3) If vstat.curs_visible is false, the cursor is not shown.
	 * 4) If vstat.curs_blinks is false, the cursor does not blink.
	 * 5) When blinking, the cursor is shown when vstat.blink is true.
	 */
	pthread_mutex_lock(&vstatlock);
	curs_start = vstat.curs_start;
	curs_end = vstat.curs_end;
	curs_row = vstat.curs_row;
	curs_col = vstat.curs_col;
	charheight = vstat.charheight;
	charwidth = vstat.charwidth;
	if (cursor_visible_locked()) {
		pthread_mutex_unlock(&vstatlock);
		cv = color_value(ciolib_fg);
		for (y = curs_start; y <= curs_end; y++) {
			pixel = &data->data[((curs_row - 1) * charheight + y) * data->rect.width + (curs_col - 1) * charwidth];
			for (x = 0; x < charwidth; x++) {
				*(pixel++) = cv;
			}
		}
	}
	else
		pthread_mutex_unlock(&vstatlock);
	pthread_mutex_lock(&callbacks.lock);
	callbacks.drawrect(data);
	callbacks.rects++;
	pthread_mutex_unlock(&callbacks.lock);
}

static void request_redraw_locked(void)
{
	force_redraws = 1;
}

static void request_redraw(void)
{
	pthread_mutex_lock(&vstatlock);
	request_redraw_locked();
	pthread_mutex_unlock(&vstatlock);
}

/*
 * Called with the screen lock held
 */
static struct rectlist *alloc_full_rect(struct bitmap_screen *screen, bool allow_throttle)
{
	struct rectlist * ret;

	pthread_mutex_lock(&free_rect_lock);
	if (allow_throttle) {
		if (throttled) {
			pthread_mutex_unlock(&free_rect_lock);
			return NULL;
		}
		if (outstanding_rects >= MAX_OUTSTANDING) {
			throttled = true;
			pthread_mutex_unlock(&free_rect_lock);
			return NULL;
		}
	}
	while (free_rects) {
		if (free_rects->rect.width == screen->screenwidth && free_rects->rect.height == screen->screenheight) {
			ret = free_rects;
			free_rects = free_rects->next;
			ret->next = NULL;
			ret->rect.x = ret->rect.y = 0;
			ret->throttle = allow_throttle;
			if (allow_throttle)
				outstanding_rects++;
			pthread_mutex_unlock(&free_rect_lock);
			return ret;
		}
		else {
			free(free_rects->data);
			ret = free_rects->next;
			free(free_rects);
			free_rects = ret;
		}
	}
	pthread_mutex_unlock(&free_rect_lock);

	ret = malloc(sizeof(struct rectlist));
	if (ret) {
		ret->next = NULL;
		ret->throttle = allow_throttle;
		ret->rect.x = 0;
		ret->rect.y = 0;
		ret->rect.width = screen->screenwidth;
		ret->rect.height = screen->screenheight;
		ret->data = malloc(ret->rect.width * ret->rect.height * sizeof(ret->data[0]));
		if (ret->data == NULL)
			FREE_AND_NULL(ret);
	}
	pthread_mutex_lock(&free_rect_lock);
	if (allow_throttle) {
		if (ret)
			outstanding_rects++;
	}
	pthread_mutex_unlock(&free_rect_lock);
	return ret;
}

static uint32_t color_value(uint32_t col)
{
	if (col & 0x80000000)
		return col & 0xffffff;
	if ((col & 0xffffff) < sizeof(palette) / sizeof(palette[0]))
		return palette[col & 0xffffff] & 0xffffff;
	fprintf(stderr, "Invalid colour value: %08x\n", col);
	return 0;
}

static struct rectlist *get_full_rectangle_locked(struct bitmap_screen *screen)
{
	struct rectlist *rect;
	size_t sz = screen->screenwidth * screen->screenheight;
	size_t pos, spos;

	// TODO: Some sort of caching here would make things faster...?
	if(callbacks.drawrect) {
		rect = alloc_full_rect(screen, true);
		if (!rect)
			return rect;
		for (pos = 0, spos = screen->screenwidth * screen->toprow; pos < sz; pos++, spos++) {
			if (spos >= sz)
				spos -= sz;
			rect->data[pos] = color_value(screen->rect->data[spos]);
		}
		return rect;
	}
	return NULL;
}

static void memset_u32(void *buf, uint32_t u, size_t len)
{
	size_t i;
	char *cbuf = buf;

	for (i = 0; i < len; i++) {
		memcpy(cbuf, &u, sizeof(uint32_t));
		cbuf += sizeof(uint32_t);
	}
}

/* The read lock must be held here. */
static int
pixel_offset(struct bitmap_screen *screen, int x, int y)
{
	y += screen->toprow;
	if (y >= screen->screenheight)
		y -= screen->screenheight;
	return y * screen->screenwidth + x;
}

struct charstate {
	unsigned char *font;
	uint32_t afc;
	uint32_t bfc;
	uint32_t bg;
	uint32_t fontoffset;
	int8_t extra_rows;
	bool double_height;
	bool gexpand;
};

struct blockstate {
	int pixeloffset;
	int maxpix;
	int font_data_width;
	uint32_t cheat_colour;
	bool expand;
};

static bool
can_cheat(struct blockstate *bs, struct vmem_cell *vc)
{
	return vc->bg == bs->cheat_colour && (vc->ch == ' ') && (vc->font < CONIO_FIRST_FREE_FONT) && !(vc->bg & 0x02000000);
}

// Returns false if we can't chate, true otherwise
static void
calc_charstate(struct blockstate *bs, struct vmem_cell *vc, struct charstate *cs, int xpos, int ypos)
{
	bool not_hidden = true;

	if (vstat.forced_font) {
		cs->font = vstat.forced_font;
	}
	else {
		if (current_font[0] == -1)
			cs->font = font[0];
		else {
			switch (vstat.charheight) {
				case 8:
					cs->font = (unsigned char *)conio_fontdata[vc->font].eight_by_eight;
					break;
				case 14:
					cs->font = (unsigned char *)conio_fontdata[vc->font].eight_by_fourteen;
					break;
				case 16:
					cs->font = (unsigned char *)conio_fontdata[vc->font].eight_by_sixteen;
					break;
				default:
					assert(0);
			}
		}
	}
	if (cs->font == NULL) {
		cs->font = font[0];
	}
	assert(cs->font);
	bool draw_fg = ((!(vc->legacy_attr & 0x80)) || vstat.no_blink);
	cs->fontoffset = (vc->ch) * (vstat.charheight * ((bs->font_data_width + 7) / 8));
	cs->double_height = false;
	if ((vstat.flags & VIDMODES_FLAG_LINE_GRAPHICS_EXPAND) && (vc->ch) >= 0xC0 && (vc->ch) <= 0xDF)
		cs->gexpand = true;
	else
		cs->gexpand = false;
	uint32_t fg = vc->fg;
	cs->bg = vc->bg;
	cs->extra_rows = 0;

	if (vstat.mode == PRESTEL_40X24 && (vc->bg & 0x02000000)) {
		struct vstat_vmem *vmem_ptr = get_vmem(&vstat);
		unsigned char lattr = vc->legacy_attr;
		int x, y;
		bool top = false;
		bool bottom = false;

		for (y = 0; y < ypos; y++) {
			if (top) {
				bottom = true;
				top = false;
			}
			else {
				if (bottom)
					bottom = false;
				for (x = 0; x < vstat.cols; x++) {
					if (vmem_ptr->vmem[y * vstat.cols + x].bg & 0x01000000) {
						top = true;
						break;
					}
				}
			}
		}
		if (bottom) {
			if (vmem_ptr->vmem[(ypos - 2) * vstat.cols + (xpos - 1)].bg & 0x01000000) {
				cs->double_height = true;
				cs->extra_rows = -(vstat.charheight);
				cs->fontoffset = (vmem_ptr->vmem[(ypos - 2) * vstat.cols + (xpos - 1)].ch) * (vstat.charheight * ((bs->font_data_width + 7) / 8));
				// TODO: Update FS etc.
			}
			else {
				// Draw as space if not double-bottom
				cs->fontoffset=(32) * (vstat.charheight * ((bs->font_data_width + 7) / 8));
			}
			fg = vmem_ptr->vmem[(ypos - 2) * vstat.cols + (xpos - 1)].fg;
			cs->bg = vmem_ptr->vmem[(ypos - 2) * vstat.cols + (xpos - 1)].bg;
			lattr = vmem_ptr->vmem[(ypos - 2) * vstat.cols + (xpos - 1)].legacy_attr;
		}
		else {
			if (ypos != vstat.rows) {
				if (vmem_ptr->vmem[(ypos - 1) * vstat.cols + (xpos - 1)].bg & 0x01000000) {
					top = true;
					cs->double_height = true;
				}
			}
		}
		release_vmem(vmem_ptr);
		if (lattr & 0x08) {
			if (!(cio_api.options & CONIO_OPT_PRESTEL_REVEAL)) {
				draw_fg = false;
				not_hidden = false;
			}
		}
	}
	cs->afc = draw_fg ? fg : cs->bg;
	cs->bfc = not_hidden ? fg : cs->bg;
}

static void
draw_char_row(struct blockstate *bs, struct charstate *cs, uint32_t y)
{
	bool fbb;

	uint8_t fb = cs->font[cs->fontoffset];
	for(unsigned x = 0; x < vstat.charwidth; x++) {
		unsigned bitnum = x & 0x07;
		if (bs->expand && x == bs->font_data_width) {
			if (cs->gexpand)
				fbb = cs->font[cs->fontoffset - 1] & (0x80 >> ((x - 1) & 7));
			else
				fbb = 0;
		}
		else
			fbb = fb & (0x80 >> bitnum);

		if (bitnum == 7) {
			cs->fontoffset++;
			fb = cs->font[cs->fontoffset];
		}

		uint32_t ac, bc;

		if (fbb) {
			ac = cs->afc;
			bc = cs->bfc;
		}
		else {
			ac = cs->bg;
			bc = cs->bg;
		}

		if (screena.rect->data[bs->pixeloffset] != ac) {
			screena.rect->data[bs->pixeloffset] = ac;
			screena.update_pixels = 1;
		}
		if (screenb.rect->data[bs->pixeloffset] != bc) {
			screenb.rect->data[bs->pixeloffset] = bc;
			screenb.update_pixels = 1;
		}

		bs->pixeloffset++;
		assert(bs->pixeloffset < bs->maxpix || x == (vstat.charwidth - 1));
	}
}

static void
draw_char_row_double(struct blockstate *bs, struct charstate *cs, uint32_t y)
{
	bool fbb;

	ssize_t pixeloffset = bs->pixeloffset + (cs->extra_rows * screena.screenwidth);
	if (pixeloffset >= bs->maxpix)
		pixeloffset -= bs->maxpix;
	if (pixeloffset < 0)
		pixeloffset += bs->maxpix;
	ssize_t pixeloffset2 = bs->pixeloffset + (cs->extra_rows + 1) * screena.screenwidth;
	if (pixeloffset2 < 0)
		pixeloffset2 += bs->maxpix;
	if (pixeloffset2 >= bs->maxpix)
		pixeloffset2 -= bs->maxpix;

	uint8_t fb = cs->font[cs->fontoffset];
	for(unsigned x = 0; x < vstat.charwidth; x++) {
		unsigned bitnum = x & 0x07;
		if (bs->expand && x == bs->font_data_width) {
			if (cs->gexpand)
				fbb = cs->font[cs->fontoffset - 1] & (0x80 >> ((x - 1) & 7));
			else
				fbb = 0;
		}
		else
			fbb = fb & (0x80 >> bitnum);

		if (bitnum == 7) {
			cs->fontoffset++;
			fb = cs->font[cs->fontoffset];
		}

		uint32_t ac, bc;

		if (fbb) {
			ac = cs->afc;
			bc = cs->bfc;
		}
		else {
			ac = cs->bg;
			bc = cs->bg;
		}

		if (screena.rect->data[pixeloffset] != ac) {
			screena.rect->data[pixeloffset] = ac;
			screena.update_pixels = 1;
		}
		if (screenb.rect->data[pixeloffset] != bc) {
			screenb.rect->data[pixeloffset] = bc;
			screenb.update_pixels = 1;
		}
		pixeloffset++;
		assert(pixeloffset < bs->maxpix || x == (vstat.charwidth - 1));
		if (screena.rect->data[pixeloffset2] != ac) {
			screena.rect->data[pixeloffset2] = ac;
			screena.update_pixels = 1;
		}
		if (screenb.rect->data[pixeloffset2] != bc) {
			screenb.rect->data[pixeloffset2] = bc;
			screenb.update_pixels = 1;
		}
		pixeloffset2++;
		assert(pixeloffset2 < bs->maxpix || x == (vstat.charwidth - 1));
	}
	cs->extra_rows++;
	bs->pixeloffset += vstat.charwidth;
	assert(bs->pixeloffset <= bs->maxpix);
}

static void
bitmap_draw_vmem(int sx, int sy, int ex, int ey, struct vmem_cell *fill)
{
	assert(sx <= ex);
	assert(sy <= ey);
	struct charstate charstate[255]; // ciolib only supports 255 columns
	struct blockstate bs;
	size_t vwidth = ex - sx + 1;
	size_t vheight  = ey - sy + 1;

	size_t xoffset = (sx-1) * vstat.charwidth;
	size_t yoffset = (sy-1) * vstat.charheight;
	assert(xoffset + vstat.charwidth <= screena.screenwidth);
	assert(xoffset + vstat.charwidth <= screenb.screenwidth);
	assert(yoffset + vstat.charheight <= screena.screenheight);
	assert(yoffset + vstat.charheight <= screenb.screenheight);
	bs.maxpix = screena.screenwidth * screena.screenheight;
	bs.expand = vstat.flags & VIDMODES_FLAG_EXPAND;
	bs.font_data_width = vstat.charwidth - (bs.expand ? 1 : 0);

	pthread_mutex_lock(&screenlock);
	bs.pixeloffset = pixel_offset(&screena, xoffset, yoffset);
	bs.cheat_colour = fill[0].bg;
	size_t rsz = screena.screenwidth - vstat.charwidth * vwidth;

	// Fill in charstate for this pass
	bool cheat = true; // If the whole thing is spaces in compiled in fonts, we can just fill.
	for (size_t vy = 0; vy < vheight; vy++) {
		for (size_t vx = 0; vx < vwidth; vx++) {
			if (!can_cheat(&bs, &fill[vy * vwidth + vx])) {
				cheat = false;
				break;
			}
		}
		if (!cheat)
			break;
	}
	if (cheat) {
		size_t ylim = vheight * vstat.charheight;
		size_t xlim = vwidth * vstat.charwidth;
		for (uint32_t y = 0; y < ylim; y++) {
			for (size_t vx = 0; vx < xlim; vx++) {
				screena.rect->data[bs.pixeloffset] = bs.cheat_colour;
				bs.pixeloffset++;
				assert(bs.pixeloffset < bs.maxpix || vx == (xlim - 1));
			}
			bs.pixeloffset += rsz;
			if (bs.pixeloffset >= bs.maxpix)
				bs.pixeloffset -= bs.maxpix;
		}
		screena.update_pixels = 1;
		bs.pixeloffset = pixel_offset(&screena, xoffset, yoffset);
		for (uint32_t y = 0; y < ylim; y++) {
			for (size_t vx = 0; vx < xlim; vx++) {
				screenb.rect->data[bs.pixeloffset] = bs.cheat_colour;
				bs.pixeloffset++;
				assert(bs.pixeloffset < bs.maxpix || vx == (xlim - 1));
			}
			bs.pixeloffset += rsz;
			if (bs.pixeloffset >= bs.maxpix)
				bs.pixeloffset -= bs.maxpix;
		}
		screenb.update_pixels = 1;
		for (size_t vy = 0; vy < vheight; vy++) {
			for (size_t vx = 0; vx < vwidth; vx++) {
				memcpy(&bitmap_drawn[(sy - 1 + vy) * vstat.cols + (sx - 1 + vx)], &fill[vy * vwidth + vx], sizeof(*bitmap_drawn));
			}
		}
	}
	else {
		for (size_t vy = 0; vy < vheight; vy++) {
			// Fill in charstate for this pass
			for (size_t vx = 0; vx < vwidth; vx++) {
				memcpy(&bitmap_drawn[(sy - 1 + vy) * vstat.cols + (sx - 1 + vx)], &fill[vy * vwidth + vx], sizeof(*bitmap_drawn));
				calc_charstate(&bs, &fill[vy * vwidth + vx], &charstate[vx], sx + vx, sy + vy);
			}
			// Draw the characters...
			for (uint32_t y = 0; y < vstat.charheight; y++) {
				for (size_t vx = 0; vx < vwidth; vx++) {
					if (charstate[vx].double_height)
						draw_char_row_double(&bs, &charstate[vx], y);
					else
						draw_char_row(&bs, &charstate[vx], y);
				}
				bs.pixeloffset += rsz;
				if (bs.pixeloffset >= bs.maxpix)
					bs.pixeloffset -= bs.maxpix;
			}
		}
	}
	pthread_mutex_unlock(&screenlock);
}

/***********************************************************/
/* These functions get called from the blinker thread only */
/***********************************************************/

static void cb_flush(void)
{
	pthread_mutex_lock(&callbacks.lock);
	if (callbacks.rects) {
		callbacks.flush();
		callbacks.rects = 0;
	}
	pthread_mutex_unlock(&callbacks.lock);
}

static int check_redraw(void)
{
	int ret;

	pthread_mutex_lock(&vstatlock);
	ret = force_redraws;
	force_redraws = 0;
	pthread_mutex_unlock(&vstatlock);
	return ret;
}

/* Blinker Thread */
static void blinker_thread(void *data)
{
	void *rect;
	int count=0;
	int curs_changed;
	int blink_changed;
	struct bitmap_screen *screen;
	struct bitmap_screen *ncscreen;
	int lfc;
	int blink;

	SetThreadName("Blinker");
	while(1) {
		curs_changed = 0;
		blink_changed = 0;
		SLEEP(10);
		count++;

		pthread_mutex_lock(&vstatlock);
		if (count==25) {
			curs_changed = cursor_visible_locked();
			if(vstat.curs_blink)
				vstat.curs_blink=FALSE;
			else
				vstat.curs_blink=TRUE;
			curs_changed = (curs_changed != cursor_visible_locked());
		}
		if(count==50) {
			if(vstat.blink)
				vstat.blink=FALSE;
			else
				vstat.blink=TRUE;
			blink_changed = 1;
			curs_changed = cursor_visible_locked();
			if(vstat.curs_blink)
				vstat.curs_blink=FALSE;
			else
				vstat.curs_blink=TRUE;
			curs_changed = (curs_changed != cursor_visible_locked());
			count=0;
		}
		lfc = force_cursor;
		force_cursor = 0;
		blink = vstat.blink;
		pthread_mutex_unlock(&vstatlock);

		if (check_redraw()) {
			if (update_from_vmem(TRUE))
				request_redraw();
		}
		else {
			if (update_from_vmem(FALSE))
				request_redraw();
		}
		pthread_mutex_lock(&screenlock);
		both_screens(blink, &screen, &ncscreen);
		if (screen->rect == NULL) {
			pthread_mutex_unlock(&screenlock);
			continue;
		}
		// TODO: Maybe we can optimize the blink_changed forced update?
		if (screen->update_pixels || curs_changed || blink_changed || lfc) {
			rect = get_full_rectangle_locked(screen);
			/*
			 * TODO: It would be more effective to wait when we're bing throttled
			 *       and make up for cursor/blink based on elapsed time, but that's
			 *       getting complicated enough that I don't want do do a quick
			 *       hack for it.  Ideally this would be done as part of pegging
			 *       the blink rate to the wall clock rather than the free-running
			 *       sleep-based method it currently uses.
			 */
			if (rect) {
				// If the other screen is update_pixels == 2, clear it.
				if (ncscreen->update_pixels == 2)
					ncscreen->update_pixels = 0;
				screen->update_pixels = 0;
				pthread_mutex_unlock(&screenlock);
				cb_drawrect(rect);
				cb_flush();
			}
			else
				pthread_mutex_unlock(&screenlock);
		}
		else {
			pthread_mutex_unlock(&screenlock);
		}
	}
}

static __inline struct bitmap_screen *noncurrent_screen_locked(int blink)
{
	if (blink)
		return &screenb;
	return &screena;
}

static __inline struct bitmap_screen *current_screen_locked(int blink)
{
	if (blink)
		return &screena;
	return(&screenb);
}

static __inline void both_screens(int blink, struct bitmap_screen** current, struct bitmap_screen** noncurrent)
{
	*current = current_screen_locked(blink);
	*noncurrent = noncurrent_screen_locked(blink);
}

static bool
same_cell(struct vmem_cell *bitmap_cell, struct vmem_cell *c2)
{
	if (bitmap_cell->ch != c2->ch)
		return false;
	if (bitmap_cell->bg != c2->bg)
		return false;
	if (bitmap_cell->fg != c2->fg)
		return false;
	if (bitmap_cell->fg & 0x04000000)	// Dirty.
		return false;
	if (bitmap_cell->font != c2->font)
		return false;
	if (bitmap_cell->legacy_attr != c2->legacy_attr)
		return false;
	return true;
}

/*
 * Updates any changed cells... blinking, modified flags, and the cursor
 * Is also used (with force = TRUE) to completely redraw the screen from
 * vmem (such as in the case of a font load).
 */
static int update_from_vmem(int force)
{
	static struct video_stats vs;
	struct vstat_vmem *vmem_ptr;
	int x,y,width,height;
	unsigned int pos;

	int bright_attr_changed=0;
	int blink_attr_changed=0;

	if(!bitmap_initialized)
		return(-1);

	pthread_mutex_lock(&vstatlock);

	if (vstat.vmem == NULL) {
		pthread_mutex_unlock(&vstatlock);
		return -1;
	}

	if(vstat.vmem->vmem == NULL) {
		pthread_mutex_unlock(&vstatlock);
		return -1;
	}

	/* If we change window size, redraw everything */
	if(bitmap_drawn == NULL || vs.cols!=vstat.cols || vs.rows != vstat.rows) {
		struct vmem_cell *newl = realloc(bitmap_drawn, sizeof(struct vmem_cell) * vstat.cols * vstat.rows);
		if (newl == NULL) {
			vs.cols = 0;
			vs.rows = 0;
			free(bitmap_drawn);
			pthread_mutex_unlock(&vstatlock);
			return -1;
		}
		bitmap_drawn = newl;
		memset(bitmap_drawn, 0x04, sizeof(struct vmem_cell) * vstat.cols * vstat.rows);
		/* Force a full redraw */
		force=1;
	}
	width=vstat.cols;
	height=vstat.rows;

	/* Did the meaning of the blink bit change? */
	if (vstat.bright_background != vs.bright_background ||
			vstat.no_blink != vs.no_blink ||
			vstat.blink_altcharset != vs.blink_altcharset)
	    blink_attr_changed = 1;

	/* Did the meaning of the bright bit change? */
	if (vstat.no_bright != vs.no_bright ||
			vstat.bright_altcharset != vs.bright_altcharset)
		bright_attr_changed = 1;

	/* Get vmem pointer */
	vmem_ptr = get_vmem(&vstat);

	/*
	 * Now we go through each character seeing if it's changed (or force is set)
	 * We combine updates into rectangles by lines...
	 *
	 * First, in the same line, we build this_rect.
	 * At the end of the line, if this_rect is the same width as the screen,
	 * we add it to last_rect.
	 */

	int sx = 0;
	int ex = 0;
	struct vmem_cell *f;
	for(y=0;y<height;y++) {
		pos=y*width;
		for(x=0;x<width;x++) {
			/* Last this char been updated? */
			if(force || !same_cell(&bitmap_drawn[pos], &vmem_ptr->vmem[pos])
			    || ((vmem_ptr->vmem[pos].legacy_attr & 0x80)
				&& blink_attr_changed)
			    || ((vmem_ptr->vmem[pos].legacy_attr & 0x08) && bright_attr_changed))
			    {
				ex = x + 1;
				if (sx == 0) {
					sx = ex;
					f = &vmem_ptr->vmem[pos];
				}
			}
			else {
				if (sx) {
					bitmap_draw_vmem(sx, y + 1, ex, y + 1, f);
					sx = ex = 0;
				}
			}
			pos++;
		}
		if (sx) {
			bitmap_draw_vmem(sx, y + 1, ex, y + 1, f);
			sx = ex = 0;
		}
	}
	release_vmem(vmem_ptr);
	vs = vstat;
	pthread_mutex_unlock(&vstatlock);

	return(0);
}

/*************************************/

/**********************/
/* Called from ciolib */
/**********************/
int bitmap_puttext(int sx, int sy, int ex, int ey, void *fill)
{
	size_t x, y;
	int ret = 1;
	uint16_t *buf = fill;
	struct vstat_vmem *vmem_ptr;

	if(!bitmap_initialized)
		return(0);

	if (sx < 1
	    || sy < 1
	    || ex < 1
	    || ey < 1
	    || sx > ex
	    || sy > ey
	    || ex > cio_textinfo.screenwidth
	    || ey > cio_textinfo.screenheight
	    || fill==NULL) {
		return(0);
	}

	struct vmem_cell *va = malloc(((ex - sx + 1) * (ey - sy + 1))*sizeof(struct vmem_cell));
	if (va == NULL)
		return 0;
	pthread_mutex_lock(&vstatlock);
	vmem_ptr = get_vmem(&vstat);
	size_t c = 0;
	for (y = sy - 1; y < ey; y++) {
		for (x = sx - 1; x < ex; x++) {
			va[c++] = *set_vmem_cell(vmem_ptr, y * vstat.cols + x, *(buf++), 0x00ffffff, 0x00ffffff);
		}
	}
	release_vmem(vmem_ptr);
	pthread_mutex_unlock(&vstatlock);
	free(va);
	return ret;
}

int
bitmap_vmem_puttext(int sx, int sy, int ex, int ey, struct vmem_cell *fill)
{
	int ret;

	pthread_mutex_lock(&vstatlock);
	ret = bitmap_vmem_puttext_locked(sx, sy, ex, ey, fill);
	pthread_mutex_unlock(&vstatlock);
	return ret;
}

int bitmap_vmem_gettext(int sx, int sy, int ex, int ey, struct vmem_cell *fill)
{
	int x,y;
	struct vstat_vmem *vmem_ptr;

	if(!bitmap_initialized)
		return(0);

	if(		   sx < 1
			|| sy < 1
			|| ex < 1
			|| ey < 1
			|| sx > ex
			|| sy > ey
			|| ex > cio_textinfo.screenwidth
			|| ey > cio_textinfo.screenheight
			|| fill==NULL) {
		return(0);
	}

	pthread_mutex_lock(&vstatlock);
	vmem_ptr = get_vmem(&vstat);
	for(y=sy-1;y<ey;y++) {
		for(x=sx-1;x<ex;x++)
			memcpy(fill++, &vmem_ptr->vmem[y*vstat.cols+x], sizeof(*fill));
	}
	release_vmem(vmem_ptr);
	pthread_mutex_unlock(&vstatlock);
	return(1);
}

void bitmap_gotoxy(int x, int y)
{
	if(!bitmap_initialized)
		return;
	/* Move cursor location */
	pthread_mutex_lock(&vstatlock);
	if (vstat.curs_col != x + cio_textinfo.winleft - 1 || vstat.curs_row != y + cio_textinfo.wintop - 1) {
		cio_textinfo.curx=x;
		cio_textinfo.cury=y;
		vstat.curs_col = x + cio_textinfo.winleft - 1;
		vstat.curs_row = y + cio_textinfo.wintop - 1;
		if (cursor_visible_locked())
			force_cursor = 1;
	}
	pthread_mutex_unlock(&vstatlock);
}

void bitmap_setcursortype(int type)
{
	if(!bitmap_initialized)
		return;
	pthread_mutex_lock(&vstatlock);
	switch(type) {
		case _NOCURSOR:
			vstat.curs_start=0xff;
			vstat.curs_end=0;
			break;
		case _SOLIDCURSOR:
			vstat.curs_start=0;
			vstat.curs_end=vstat.charheight-1;
			force_cursor = 1;
			break;
		default:
		    vstat.curs_start = vstat.default_curs_start;
		    vstat.curs_end = vstat.default_curs_end;
			force_cursor = 1;
			break;
	}
	pthread_mutex_unlock(&vstatlock);
}

int bitmap_setfont(int font, int force, int font_num)
{
	int changemode=0;
	int	newmode=-1;
	struct text_info ti;
	int		ow,oh;
	int		row,col;
	int		attr;
	struct vmem_cell	*old;
	struct vmem_cell	*new;
	struct vmem_cell	*pold;
	struct vmem_cell	*pnew;

	if(!bitmap_initialized)
		return(0);
	if(font < 0 || font>(sizeof(conio_fontdata)/sizeof(struct conio_font_data_struct)-2))
		return(0);

	if(conio_fontdata[font].eight_by_sixteen!=NULL)
		newmode=C80;
	else if(conio_fontdata[font].eight_by_fourteen!=NULL)
		newmode=C80X28;
	else if(conio_fontdata[font].eight_by_eight!=NULL)
		newmode=C80X50;

	pthread_mutex_lock(&vstatlock);
	switch(vstat.charheight) {
		case 8:
			if(conio_fontdata[font].eight_by_eight==NULL) {
				if(!force)
					goto error_return;
				else
					changemode=1;
			}
			break;
		case 14:
			if(conio_fontdata[font].eight_by_fourteen==NULL) {
				if(!force)
					goto error_return;
				else
					changemode=1;
			}
			break;
		case 16:
			if(conio_fontdata[font].eight_by_sixteen==NULL) {
				if(!force)
					goto error_return;
				else
					changemode=1;
			}
			break;
	}
	if(changemode && (newmode==-1 || font_num > 1))
		goto error_return;
	switch(font_num) {
		case 0:
			default_font=font;
			/* Fall-through */
		case 1:
			current_font[0]=font;
			break;
		case 2:
		case 3:
		case 4:
			current_font[font_num-1]=font;
			break;
	}

	if(changemode) {
		ti = cio_textinfo;

		attr=ti.attribute;
		ow=ti.screenwidth;
		oh=ti.screenheight;

		old=malloc(ow*oh*sizeof(*old));
		if(old) {
			bitmap_vmem_gettext(1,1,ow,oh,old);
			/* coverity[sleep:SUPPRESS] */
			textmode(newmode);
			new=malloc(ti.screenwidth*ti.screenheight*sizeof(*new));
			if(!new) {
				free(old);
				pthread_mutex_unlock(&vstatlock);
				return 0;
			}
			pold=old;
			pnew=new;
			for(row=0; row<ti.screenheight; row++) {
				for(col=0; col<ti.screenwidth; col++) {
					if(row < oh) {
						if(col < ow) {
							memcpy(new, old, sizeof(*old));
							new->font = font;
							new++;
							old++;
						}
						else {
							new->ch=' ';
							new->legacy_attr=attr;
							new->font = font;
							new->fg = ciolib_fg;
							new->bg = ciolib_bg;
							new++;
						}
					}
					else {
							new->ch=' ';
							new->legacy_attr=attr;
							new->font = font;
							new->fg = ciolib_fg;
							new->bg = ciolib_bg;
							new++;
					}
				}
				if(row < oh) {
					for(;col<ow;col++)
						old++;
				}
			}
			bitmap_vmem_puttext_locked(1,1,ti.screenwidth,ti.screenheight,pnew);
			free(pnew);
			free(pold);
		}
		else {
			FREE_AND_NULL(old);
		}
	}
	bitmap_loadfont_locked(NULL);
	pthread_mutex_unlock(&vstatlock);
	return(1);

error_return:
	pthread_mutex_unlock(&vstatlock);
	return(0);
}

int bitmap_getfont(int font_num)
{
	int ret;

	if (font_num == 0)
		ret = default_font;
	else if (font_num > 4)
		ret = -1;
	else
		ret = current_font[font_num - 1];

	return ret;
}

int bitmap_loadfont(const char *filename)
{
	int ret;

	pthread_mutex_lock(&vstatlock);
	ret = bitmap_loadfont_locked(filename);
	pthread_mutex_unlock(&vstatlock);
	return ret;
}

static void
bitmap_movetext_screen(int x, int y, int tox, int toy, int direction, int height, int width)
{
	int32_t sdestoffset;
	ssize_t ssourcepos;
	int step;
	int32_t screeny;

	pthread_mutex_lock(&screenlock);
	if (width == vstat.cols && (height > vstat.rows / 2) && toy == 1) {
		screena.toprow += (y - toy) * vstat.charheight;
		if (screena.toprow >= screena.screenheight)
			screena.toprow -= screena.screenheight;
		if (screena.toprow < 0)
			screena.toprow += screena.screenheight;
		screenb.toprow += (y - toy) * vstat.charheight;
		if (screenb.toprow >= screenb.screenheight)
			screenb.toprow -= screenb.screenheight;
		if (screena.toprow < 0)
			screena.toprow += screena.screenheight;

		height = vstat.rows - height;
		toy = vstat.rows - (height - 1);
		// Fill the bits with impossible data so they're redrawn
		memset(&bitmap_drawn[(toy - 1) * vstat.cols], 0x04, sizeof(*bitmap_drawn) * height * vstat.cols);
		pthread_mutex_unlock(&screenlock);
		return;
	}

	int maxpos = screena.screenwidth * screena.screenheight;
	if (direction == -1) {
		ssourcepos = ((y + height - 1)      * vstat.charheight - 1) * vstat.scrnwidth + (x -   1) * vstat.charwidth;
		sdestoffset = ((((toy + height - 1) * vstat.charheight - 1) * vstat.scrnwidth + (tox - 1) * vstat.charwidth) - ssourcepos);
	}
	else {
		ssourcepos=(y - 1)     * vstat.scrnwidth * vstat.charheight + (x - 1)  * vstat.charwidth;
		sdestoffset=(((toy - 1) * vstat.scrnwidth * vstat.charheight + (tox - 1) * vstat.charwidth) - ssourcepos);
	}
	ssourcepos += screena.toprow * screena.screenwidth;
	step = direction * vstat.scrnwidth;
	for(screeny=0; screeny < height*vstat.charheight; screeny++) {
		if (ssourcepos >= maxpos)
			ssourcepos -= maxpos;
		if (ssourcepos < 0)
			ssourcepos += maxpos;
		int dest = ssourcepos + sdestoffset;
		if (dest >= maxpos)
			dest -= maxpos;
		if (dest < 0)
			dest += maxpos;
		memmove(&(screena.rect->data[dest]), &(screena.rect->data[ssourcepos]), sizeof(screena.rect->data[0])*width*vstat.charwidth);
		memmove(&(screenb.rect->data[dest]), &(screenb.rect->data[ssourcepos]), sizeof(screenb.rect->data[0])*width*vstat.charwidth);
		ssourcepos += step;
	}
	screena.update_pixels = 1;
	screenb.update_pixels = 1;
	pthread_mutex_unlock(&screenlock);
}

int bitmap_movetext(int x, int y, int ex, int ey, int tox, int toy)
{
	int	direction=1;
	int	cy;
	int	destoffset;
	int	sourcepos;
	int width=ex-x+1;
	int height=ey-y+1;
	struct vstat_vmem *vmem_ptr;
	int step;

	if(		   x<1
			|| y<1
			|| ex<1
			|| ey<1
			|| tox<1
			|| toy<1
			|| x>cio_textinfo.screenwidth
			|| ex>cio_textinfo.screenwidth
			|| tox>cio_textinfo.screenwidth
			|| (tox + width - 1) > cio_textinfo.screenwidth
			|| y>cio_textinfo.screenheight
			|| ey>cio_textinfo.screenheight
			|| toy>cio_textinfo.screenheight
			|| (toy + height - 1) > cio_textinfo.screenheight
			|| ex < x
			|| ey < y
			) {
		return(0);
	}

	if(toy > y)
		direction=-1;

	pthread_mutex_lock(&vstatlock);
	vmem_ptr = get_vmem(&vstat);
	if (width == vstat.cols) {
		sourcepos =  ((  y - 1) * vstat.cols + (  x - 1));
		destoffset = ((toy - 1) * vstat.cols + (tox - 1)) - sourcepos;
		memmove(&(vmem_ptr->vmem[sourcepos+destoffset]), &(vmem_ptr->vmem[sourcepos]), sizeof(vmem_ptr->vmem[0])*width*height);
		memmove(&(bitmap_drawn[sourcepos+destoffset]), &(bitmap_drawn[sourcepos]), sizeof(vmem_ptr->vmem[0])*width*height);
	}
	else {
		if (direction == -1) {
			sourcepos=(y+height-2)*vstat.cols+(x-1);
			destoffset=(((toy+height-2)*vstat.cols+(tox-1))-sourcepos);
		}
		else {
			sourcepos=(y-1)*vstat.cols+(x-1);
			destoffset=(((toy-1)*vstat.cols+(tox-1))-sourcepos);
		}
		step = direction * vstat.cols;
		for(cy=0; cy<height; cy++) {
			memmove(&(vmem_ptr->vmem[sourcepos+destoffset]), &(vmem_ptr->vmem[sourcepos]), sizeof(vmem_ptr->vmem[0])*width);
			memmove(&(bitmap_drawn[sourcepos+destoffset]), &(bitmap_drawn[sourcepos]), sizeof(vmem_ptr->vmem[0])*width);
			sourcepos += step;
		}
	}

	bitmap_movetext_screen(x, y, tox, toy, direction, height, width);
	release_vmem(vmem_ptr);
	pthread_mutex_unlock(&vstatlock);

	return(1);
}

void bitmap_clreol(void)
{
	int pos,x;
	WORD fill=(cio_textinfo.attribute<<8)|' ';
	struct vstat_vmem *vmem_ptr;
	int row;

	if(!bitmap_initialized)
		return;
	struct vmem_cell *va = malloc((cio_textinfo.winright - (cio_textinfo.curx + cio_textinfo.winleft - 1) + 1) * sizeof(struct vmem_cell));
	if (va == NULL)
		return;

	row = cio_textinfo.cury + cio_textinfo.wintop - 1;
	pos=(row - 1)*cio_textinfo.screenwidth;
	pthread_mutex_lock(&vstatlock);
	vmem_ptr = get_vmem(&vstat);
	int c = 0;
	for(x=cio_textinfo.curx+cio_textinfo.winleft-2; x<cio_textinfo.winright; x++) {
		va[c++] = *set_vmem_cell(vmem_ptr, pos+x, fill, ciolib_fg, ciolib_bg);
	}
	release_vmem(vmem_ptr);
	pthread_mutex_unlock(&vstatlock);
	free(va);
}

void bitmap_clrscr(void)
{
	size_t x, y;
	WORD fill = (cio_textinfo.attribute << 8) | ' ';
	struct vstat_vmem *vmem_ptr;
	size_t c = 0;
	int rows, cols;
	struct vmem_cell *va;

	if(!bitmap_initialized)
		return;
	va = malloc(((cio_textinfo.winright - cio_textinfo.winleft + 1) * (cio_textinfo.winbottom - cio_textinfo.wintop + 1)) * sizeof(struct vmem_cell));
	if (va == NULL)
		return;
	pthread_mutex_lock(&vstatlock);
	vmem_ptr = get_vmem(&vstat);
	rows = vstat.rows;
	cols = vstat.cols;
	for (y = cio_textinfo.wintop - 1; y < cio_textinfo.winbottom && y < rows; y++) {
		for (x = cio_textinfo.winleft - 1; x < cio_textinfo.winright && x < cols; x++) {
			va[c++] = *set_vmem_cell(vmem_ptr, y * cio_textinfo.screenwidth + x, fill, ciolib_fg, ciolib_bg);
		}
	}
	free(va);
	release_vmem(vmem_ptr);
	pthread_mutex_unlock(&vstatlock);
}

void bitmap_getcustomcursor(int *s, int *e, int *r, int *b, int *v)
{
	pthread_mutex_lock(&vstatlock);
	if(s)
		*s=vstat.curs_start;
	if(e)
		*e=vstat.curs_end;
	if(r)
		*r=vstat.charheight;
	if(b)
		*b=vstat.curs_blinks;
	if(v)
		*v=vstat.curs_visible;
	pthread_mutex_unlock(&vstatlock);
}

void bitmap_setcustomcursor(int s, int e, int r, int b, int v)
{
	double ratio;

	pthread_mutex_lock(&vstatlock);
	if(r==0)
		ratio=0;
	else
		ratio=vstat.charheight/r;
	if(s>=0)
		vstat.curs_start=s*ratio;
	if(e>=0)
		vstat.curs_end=e*ratio;
	if(b>=0)
		vstat.curs_blinks=b;
	if(v>=0)
		vstat.curs_visible=v;
	force_cursor = 1;
	pthread_mutex_unlock(&vstatlock);
}

int bitmap_getvideoflags(void)
{
	int flags=0;

	pthread_mutex_lock(&vstatlock);
	if(vstat.bright_background)
		flags |= CIOLIB_VIDEO_BGBRIGHT;
	if(vstat.no_bright)
		flags |= CIOLIB_VIDEO_NOBRIGHT;
	if(vstat.bright_altcharset)
		flags |= CIOLIB_VIDEO_ALTCHARS;
	if(vstat.no_blink)
		flags |= CIOLIB_VIDEO_NOBLINK;
	if(vstat.blink_altcharset)
		flags |= CIOLIB_VIDEO_BLINKALTCHARS;
	pthread_mutex_unlock(&vstatlock);
	return(flags);
}

void bitmap_setvideoflags(int flags)
{
	pthread_mutex_lock(&vstatlock);
	if(flags & CIOLIB_VIDEO_BGBRIGHT)
		vstat.bright_background=1;
	else
		vstat.bright_background=0;

	if(flags & CIOLIB_VIDEO_NOBRIGHT)
		vstat.no_bright=1;
	else
		vstat.no_bright=0;

	if(flags & CIOLIB_VIDEO_ALTCHARS)
		vstat.bright_altcharset=1;
	else
		vstat.bright_altcharset=0;

	if(flags & CIOLIB_VIDEO_NOBLINK)
		vstat.no_blink=1;
	else
		vstat.no_blink=0;

	if(flags & CIOLIB_VIDEO_BLINKALTCHARS)
		vstat.blink_altcharset=1;
	else
		vstat.blink_altcharset=0;
	pthread_mutex_unlock(&vstatlock);
}

int bitmap_attr2palette(uint8_t attr, uint32_t *fgp, uint32_t *bgp)
{
	int ret;

	pthread_mutex_lock(&vstatlock);
	ret = bitmap_attr2palette_locked(attr, fgp, bgp);
	pthread_mutex_unlock(&vstatlock);

	return ret;
}

int bitmap_setpixel(uint32_t x, uint32_t y, uint32_t colour)
{
	update_from_vmem(FALSE);
	pthread_mutex_lock(&vstatlock);
	size_t offset = y / vstat.charheight * vstat.cols + x / vstat.charwidth;
	struct vstat_vmem *vmem_ptr = get_vmem(&vstat);
	bitmap_drawn[offset].bg |= 0x04000000;
	vmem_ptr->vmem[offset].bg |= 0x04000000;
	release_vmem(vmem_ptr);
	pthread_mutex_lock(&screenlock);
	if (x < screena.screenwidth && y < screena.screenheight) {
		if (screena.rect->data[pixel_offset(&screena, x, y)] != colour) {
			screena.update_pixels = 1;
			screena.rect->data[pixel_offset(&screena, x, y)] = colour;
		}
	}

	if (x < screenb.screenwidth && y < screenb.screenheight) {
		if (screenb.rect->data[pixel_offset(&screenb, x, y)] != colour) {
			screenb.update_pixels = 1;
			screenb.rect->data[pixel_offset(&screenb, x, y)] = colour;
		}
	}
	pthread_mutex_unlock(&screenlock);
	pthread_mutex_unlock(&vstatlock);

	return 1;
}

int bitmap_setpixels(uint32_t sx, uint32_t sy, uint32_t ex, uint32_t ey, uint32_t x_off, uint32_t y_off, uint32_t mx_off, uint32_t my_off, struct ciolib_pixels *pixels, struct ciolib_mask *mask)
{
	uint32_t x, y;
	uint32_t width,height;
	int mask_bit;
	size_t mask_byte;
	size_t pos;
	size_t mpos;

	if (pixels == NULL)
		return 0;

	if (sx > ex || sy > ey)
		return 0;

	width = ex - sx + 1;
	height = ey - sy + 1;

	if (width + x_off > pixels->width)
		return 0;

	if (height + y_off > pixels->height)
		return 0;

	if (mask != NULL) {
		if (width + mx_off > mask->width)
			return 0;
		if (height + my_off > mask->height)
			return 0;
	}

	update_from_vmem(FALSE);
	pthread_mutex_lock(&vstatlock);
	struct vstat_vmem *vmem_ptr = get_vmem(&vstat);
	pthread_mutex_lock(&screenlock);
	if (ex > screena.screenwidth || ey > screena.screenheight) {
		pthread_mutex_unlock(&screenlock);
		return 0;
	}

	for (y = sy; y <= ey; y++) {
		pos = pixels->width*(y-sy+y_off)+x_off;
		size_t offset;
		if (mask == NULL) {
			for (x = sx; x <= ex; x++) {
				offset = y / vstat.charheight * vstat.cols + x / vstat.charwidth;
				bitmap_drawn[offset].bg |= 0x04000000;
				vmem_ptr->vmem[offset].bg |= 0x04000000;
				if (screena.rect->data[pixel_offset(&screena, x, y)] != pixels->pixels[pos]) {
					screena.rect->data[pixel_offset(&screena, x, y)] = pixels->pixels[pos];
					screena.update_pixels = 1;
				}
				if (pixels->pixelsb) {
					if (screenb.rect->data[pixel_offset(&screenb, x, y)] != pixels->pixelsb[pos]) {
						screenb.rect->data[pixel_offset(&screenb, x, y)] = pixels->pixelsb[pos];
						screenb.update_pixels = 1;
					}
				}
				else {
					if (screenb.rect->data[pixel_offset(&screenb, x, y)] != pixels->pixels[pos]) {
						screenb.rect->data[pixel_offset(&screenb, x, y)] = pixels->pixels[pos];
						screenb.update_pixels = 1;
					}
				}
				pos++;
			}
		}
		else {
			mpos = mask->width * (y - sy + my_off) + mx_off;
			for (x = sx; x <= ex; x++) {
				offset = y / vstat.charheight * vstat.cols + x / vstat.charwidth;
				bitmap_drawn[offset].bg |= 0x04000000;
				vmem_ptr->vmem[offset].bg |= 0x04000000;
				mask_byte = mpos / 8;
				mask_bit = mpos % 8;
				mask_bit = 0x80 >> mask_bit;
				if (mask->bits[mask_byte] & mask_bit) {
					if (screena.rect->data[pixel_offset(&screena, x, y)] != pixels->pixels[pos]) {
						screena.rect->data[pixel_offset(&screena, x, y)] = pixels->pixels[pos];
						screena.update_pixels = 1;
					}
					if (pixels->pixelsb) {
						if (screenb.rect->data[pixel_offset(&screenb, x, y)] != pixels->pixelsb[pos]) {
							screenb.rect->data[pixel_offset(&screenb, x, y)] = pixels->pixelsb[pos];
							screenb.update_pixels = 1;
						}
					}
					else {
						if (screenb.rect->data[pixel_offset(&screenb, x, y)] != pixels->pixels[pos]) {
							screenb.rect->data[pixel_offset(&screenb, x, y)] = pixels->pixels[pos];
							screenb.update_pixels = 1;
						}
					}
				}
				pos++;
				mpos++;
			}
		}
	}
	pthread_mutex_unlock(&screenlock);
	release_vmem(vmem_ptr);
	pthread_mutex_unlock(&vstatlock);

	return 1;
}

// TODO: Do we ever need to force anymore?
struct ciolib_pixels *bitmap_getpixels(uint32_t sx, uint32_t sy, uint32_t ex, uint32_t ey, int force)
{
	struct ciolib_pixels *pixels;
	uint32_t width,height;
	size_t y;

	if (sx > ex || sy > ey)
		return NULL;

	width = ex - sx + 1;
	height = ey - sy + 1;

	pixels = malloc(sizeof(*pixels));
	if (pixels == NULL)
		return NULL;

	pixels->width = width;
	pixels->height = height;

	pixels->pixels = malloc(sizeof(pixels->pixels[0])*(width)*(height));
	if (pixels->pixels == NULL) {
		free(pixels);
		return NULL;
	}

	pixels->pixelsb = malloc(sizeof(pixels->pixelsb[0])*(width)*(height));
	if (pixels->pixelsb == NULL) {
		free(pixels->pixels);
		free(pixels);
		return NULL;
	}

	update_from_vmem(force);
	pthread_mutex_lock(&screenlock);
	if (ex >= screena.screenwidth || ey >= screena.screenheight ||
	    ex >= screenb.screenwidth || ey >= screenb.screenheight) {
		pthread_mutex_unlock(&screenlock);
		free(pixels->pixelsb);
		free(pixels->pixels);
		free(pixels);
		return NULL;
	}

	for (y = sy; y <= ey; y++) {
		// TODO: This is the place where screen vs. buffer matters. :(
		memcpy(&pixels->pixels[width*(y-sy)], &screena.rect->data[pixel_offset(&screena, sx, y)], width * sizeof(pixels->pixels[0]));
		memcpy(&pixels->pixelsb[width*(y-sy)], &screenb.rect->data[pixel_offset(&screenb, sx, y)], width * sizeof(pixels->pixelsb[0]));
	}
	pthread_mutex_unlock(&screenlock);

	return pixels;
}

int bitmap_get_modepalette(uint32_t p[16])
{
	pthread_mutex_lock(&vstatlock);
	memcpy(p, vstat.palette, sizeof(vstat.palette));
	pthread_mutex_unlock(&vstatlock);
	return 1;
}

int bitmap_set_modepalette(uint32_t p[16])
{
	pthread_mutex_lock(&vstatlock);
	memcpy(vstat.palette, p, sizeof(vstat.palette));
	pthread_mutex_unlock(&vstatlock);
	return 1;
}

uint32_t bitmap_map_rgb(uint16_t r, uint16_t g, uint16_t b)
{
	return (0xff << 24) | ((r & 0xff00) << 8) | ((g & 0xff00)) | (b >> 8);
}

void bitmap_replace_font(uint8_t id, char *name, void *data, size_t size)
{
	if (id < CONIO_FIRST_FREE_FONT) {
		free(name);
		free(data);
		return;
	}

	pthread_mutex_lock(&screenlock);
	switch (size) {
		case 4096:
			FREE_AND_NULL(conio_fontdata[id].eight_by_sixteen);
			conio_fontdata[id].eight_by_sixteen=data;
			FREE_AND_NULL(conio_fontdata[id].desc);
			conio_fontdata[id].desc=name;
			break;
		case 3584:
			FREE_AND_NULL(conio_fontdata[id].eight_by_fourteen);
			conio_fontdata[id].eight_by_fourteen=data;
			FREE_AND_NULL(conio_fontdata[id].desc);
			conio_fontdata[id].desc=name;
			break;
		case 2048:
			FREE_AND_NULL(conio_fontdata[id].eight_by_eight);
			conio_fontdata[id].eight_by_eight=data;
			FREE_AND_NULL(conio_fontdata[id].desc);
			conio_fontdata[id].desc=name;
			break;
		default:
			free(name);
			free(data);
	}
	pthread_mutex_unlock(&screenlock);
	request_redraw();
}

int bitmap_setpalette(uint32_t index, uint16_t r, uint16_t g, uint16_t b)
{
	if (index > 65535)
		return 0;

	pthread_mutex_lock(&screenlock);
	palette[index] = (0xff << 24) | ((r>>8) << 16) | ((g>>8) << 8) | (b>>8);
	screena.update_pixels = 1;
	screenb.update_pixels = 1;
	pthread_mutex_unlock(&screenlock);
	return 1;
}

// Called with vstatlock
static int init_screens(int *width, int *height)
{
	pthread_mutex_lock(&screenlock);
	screena.screenwidth = vstat.scrnwidth;
	screenb.screenwidth = vstat.scrnwidth;
	if (width)
		*width = screena.screenwidth;
	screena.screenheight = vstat.scrnheight;
	screenb.screenheight = vstat.scrnheight;
	if (height)
		*height = screena.screenheight;
	screena.update_pixels = 1;
	screenb.update_pixels = 1;
	bitmap_drv_free_rect(screena.rect);
	bitmap_drv_free_rect(screenb.rect);
	screena.rect = alloc_full_rect(&screena, false);
	if (screena.rect == NULL) {
		pthread_mutex_unlock(&screenlock);
		return(-1);
	}
	screenb.rect = alloc_full_rect(&screenb, false);
	if (screenb.rect == NULL) {
		bitmap_drv_free_rect(screena.rect);
		screena.rect = NULL;
		pthread_mutex_unlock(&screenlock);
		return(-1);
	}
	memset_u32(screena.rect->data, color_value(vstat.palette[0]), screena.rect->rect.width * screena.rect->rect.height);
	memset_u32(screenb.rect->data, color_value(vstat.palette[0]), screenb.rect->rect.width * screenb.rect->rect.height);
	pthread_mutex_unlock(&screenlock);
	return(0);
}

/***********************/
/* Called from drivers */
/***********************/

// Must be called with vstatlock
static bool
bitmap_width_controls(void)
{
	bool wc;

	if (vstat.aspect_width == 0 || vstat.aspect_height == 0)
		wc = true;
	else
		wc = lround((double)(vstat.scrnheight * vstat.aspect_width) / vstat.aspect_height) <= vstat.scrnwidth;
	return wc;
}

// Must be called with vstatlock
void
bitmap_get_scaled_win_size_nomax(double scale, int *w, int *h)
{
	bool wc = bitmap_width_controls();

	if (scale < 1.0)
		scale = 1.0;
	*w = lround(vstat.scrnwidth * scale);
	*h = lround(vstat.scrnheight * scale);
	if (wc)
		*h = INT_MAX;
	else
		*w = INT_MAX;
	aspect_fix_wc(w, h, wc, vstat.aspect_width, vstat.aspect_height);
}

// Must be called with vstatlock
double
bitmap_double_mult_inside(int maxwidth, int maxheight)
{
	double mult = 1.0;
	double wmult = 1.0;
	double hmult = 1.0;
	int wmw, wmh;
	int hmw, hmh;
	int w, h;

	bitmap_get_scaled_win_size_nomax(1.0, &w, &h);
	wmult = (double)maxwidth / w;
	hmult = (double)maxheight / h;
	bitmap_get_scaled_win_size_nomax(wmult, &wmw, &wmh);
	bitmap_get_scaled_win_size_nomax(hmult, &hmw, &hmh);
	if (wmult < hmult) {
		if (hmw <= maxwidth && hmh <= maxheight)
			mult = hmult;
		else
			mult = wmult;
	}
	else if(hmult < wmult) {
		if (wmw <= maxwidth && wmh <= maxheight)
			mult = wmult;
		else
			mult = hmult;
	}
	else
		mult = hmult;
	// TODO: Allow below 1.0?
	if (mult < 1.0)
		mult = 1.0;
	return mult;
}

// Must be called with vstatlock
int
bitmap_largest_mult_inside(int maxwidth, int maxheight)
{
	return (int)bitmap_double_mult_inside(maxwidth, maxheight);
}

// Must be called with vstatlock
void
bitmap_get_scaled_win_size(double scale, int *w, int *h, int maxwidth, int maxheight)
{
	bool wc = bitmap_width_controls();
	double max;

	if (maxwidth == 0 && maxheight == 0) {
		bitmap_get_scaled_win_size_nomax(scale, w, h);
		return;
	}
	if (maxwidth < vstat.scrnwidth)
		maxwidth = vstat.scrnwidth;
	if (maxheight < vstat.scrnheight)
		maxheight = vstat.scrnheight;
	max = bitmap_double_mult_inside(maxwidth, maxheight);
	if (max < 1.0)
		max = 1.0;
	if (scale < 1.0)
		scale = 1.0;
	if (scale > max)
		scale = max;
	*w = lround(vstat.scrnwidth * scale);
	*h = lround(vstat.scrnheight * scale);
	if (wc)
		*h = INT_MAX;
	else
		*w = INT_MAX;
	if (*w > maxwidth && maxwidth > 0)
		*w = maxwidth;
	if (*h > maxheight && maxheight > 0)
		*h = maxheight;
	aspect_fix_wc(w, h, wc, vstat.aspect_width, vstat.aspect_height);
}

// Must be called with vstatlock
void
bitmap_snap(bool grow, int maxwidth, int maxheight)
{
	int mult;
	int wc;
	int cw;
	int cs;

	wc = bitmap_width_controls();
	if (wc) {
		mult = vstat.winwidth / vstat.scrnwidth;
		cw = vstat.winwidth;
		cs = vstat.scrnwidth;
	}
	else {
		mult = vstat.winheight / vstat.scrnheight;
		cw = vstat.winheight;
		cs = vstat.winwidth;
	}
	if (grow) {
		mult++;
	}
	else {
		if (cw % cs == 0)
			mult--;
	}
	if (mult < 1)
		mult = 1;
	do {
		bitmap_get_scaled_win_size(mult, &vstat.winwidth, &vstat.winheight, maxwidth, maxheight);
		mult--;
	} while ((vstat.winwidth > maxwidth || vstat.winheight > maxheight) && mult > 1);
}

/*
 * This function is intended to be called from the driver.
 * as a result, it cannot block waiting for driver status
 *
 * Must be called with vstatlock held.
 * Care MUST be taken to avoid deadlocks...
 * This is where the vmode bits used by the driver are modified...
 * the driver must be aware of this.
 * This is where the driver should grab vstatlock, then it should copy
 * out after this and only grab that lock again briefly to update
 * vstat.scaling.
 */
int bitmap_drv_init_mode(int mode, int *width, int *height, int maxwidth, int maxheight)
{
	int i;
	int64_t os;
	int64_t ls;
	int64_t ns;
	int64_t bs;
	int w, h;
	int mult;

	if(!bitmap_initialized)
		return(-1);

	if (mode == _ORIGMODE)
		mode = C80;
	if(load_vmode(&vstat, mode)) {
		return(-1);
	}

	// Save the old diagonal (no point is sqrting here)
	os = ((int64_t)vstat.winwidth * vstat.winwidth) + ((int64_t)vstat.winheight * vstat.winheight);

	/* Initialize video memory with black background, white foreground */
	for (i = 0; i < vstat.cols*vstat.rows; ++i) {
		if (i > 0)
			vstat.vmem->vmem[i] = vstat.vmem->vmem[0];
		else {
			vstat.vmem->vmem[i].ch = 0;
			vstat.vmem->vmem[i].legacy_attr = vstat.currattr;
			vstat.vmem->vmem[i].font = default_font;
			bitmap_attr2palette_locked(vstat.currattr, &vstat.vmem->vmem[i].fg, &vstat.vmem->vmem[i].bg);
		}
	}
	// Clear the bitmap draw cache
	FREE_AND_NULL(bitmap_drawn);

	if (init_screens(width, height))
		return -1;
	for (i=0; i<sizeof(current_font)/sizeof(current_font[0]); i++)
		current_font[i]=default_font;
	bitmap_loadfont_locked(NULL);

	cio_textinfo.attribute=vstat.currattr;
	cio_textinfo.normattr=vstat.currattr;
	cio_textinfo.currmode=mode;

	if (vstat.rows > 0xff)
		cio_textinfo.screenheight = 0xff;
	else
		cio_textinfo.screenheight = vstat.rows;

	if (vstat.cols > 0xff)
		cio_textinfo.screenwidth = 0xff;
	else
		cio_textinfo.screenwidth = vstat.cols;

	cio_textinfo.curx=1;
	cio_textinfo.cury=1;
	cio_textinfo.winleft=1;
	cio_textinfo.wintop=1;
	cio_textinfo.winright=cio_textinfo.screenwidth;
	cio_textinfo.winbottom=cio_textinfo.screenheight;

	// Now calculate the closest diagonal new size that's smaller than max...
	mult = 1;
	bitmap_get_scaled_win_size(mult, &w, &h, maxwidth, maxheight);
	bs = ((int64_t)w * w) + ((int64_t)h * h);
	ls = bs;
	ns = bs;
	while (ns < os) {
		mult++;
		bitmap_get_scaled_win_size(mult, &w, &h, 0, 0);
		if ((maxwidth > 0) && (w > maxwidth)) {
			mult--;
			ns = ls;
			break;
		}
		if (w == maxwidth)
			break;
		if ((maxheight > 0) && (h > maxheight)) {
			mult--;
			ns = ls;
			break;
		}
		if (h == maxheight)
			break;
		bs = ((int64_t)w * w) + ((int64_t)h * h);
		ls = ns;
		ns = bs;
	}
	if ((os - ls) <= (ns - os)) {
		if (mult > 1)
			mult--;
	}
	bitmap_get_scaled_win_size(mult, &w, &h, maxwidth, maxheight);
	vstat.winwidth = w;
	vstat.winheight = h;
	vstat.scaling = mult;

	return(0);
}

/*
 * MUST be called only once and before any other bitmap functions
 */
int bitmap_drv_init(void (*drawrect_cb) (struct rectlist *data)
				,void (*flush_cb) (void))
{
	int i;

	if(bitmap_initialized)
		return(-1);
	cio_api.options |= CONIO_OPT_LOADABLE_FONTS | CONIO_OPT_BLINK_ALT_FONT
			| CONIO_OPT_BOLD_ALT_FONT | CONIO_OPT_BRIGHT_BACKGROUND
			| CONIO_OPT_SET_PIXEL | CONIO_OPT_CUSTOM_CURSOR
			| CONIO_OPT_FONT_SELECT | CONIO_OPT_EXTENDED_PALETTE | CONIO_OPT_PALETTE_SETTING
			| CONIO_OPT_BLOCKY_SCALING;
	pthread_mutex_init(&callbacks.lock, NULL);
	pthread_mutex_init(&vstatlock, NULL);
	pthread_mutex_init(&screenlock, NULL);
	pthread_mutex_init(&free_rect_lock, NULL);
	pthread_mutex_lock(&vstatlock);
	vstat.flags = VIDMODES_FLAG_PALETTE_VMEM;
	pthread_mutex_lock(&screenlock);
	for (i = 0; i < sizeof(dac_default)/sizeof(struct dac_colors); i++) {
		palette[i] = (0xffU << 24) | (dac_default[i].red << 16) | (dac_default[i].green << 8) | dac_default[i].blue;
	}
	pthread_mutex_unlock(&screenlock);
	pthread_mutex_unlock(&vstatlock);

	callbacks.drawrect=drawrect_cb;
	callbacks.flush=flush_cb;
	pthread_mutex_lock(&callbacks.lock);
	callbacks.rects = 0;
	pthread_mutex_unlock(&callbacks.lock);
	bitmap_initialized=1;
	_beginthread(blinker_thread,0,NULL);

	return(0);
}

void bitmap_drv_request_pixels(void)
{
	pthread_mutex_lock(&screenlock);
	if (screena.update_pixels == 0)
		screena.update_pixels = 2;
	if (screenb.update_pixels == 0)
		screenb.update_pixels = 2;
	pthread_mutex_unlock(&screenlock);
}

void bitmap_drv_request_some_pixels(int x, int y, int width, int height)
{
	/* TODO: Some sort of queue here? */
	bitmap_drv_request_pixels();
}

void bitmap_drv_free_rect(struct rectlist *rect)
{
	if (rect == NULL)
		return;
	pthread_mutex_lock(&free_rect_lock);
	if (rect->throttle) {
		outstanding_rects--;
		if (outstanding_rects < MAX_OUTSTANDING && throttled) {
			throttled = false;
		}
	}
	rect->next = free_rects;
	free_rects = rect;
	pthread_mutex_unlock(&free_rect_lock);
}
