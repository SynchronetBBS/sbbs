#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>		/* NULL */
#include <stdlib.h>
#include <string.h>

#include "threadwrap.h"
#include "rwlockwrap.h"
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
static protected_int32_t videoflags;
static int real_screenwidth = 80;
static bool have_blink; // true if there's any blinking characters on the screen

/* Exported globals */

rwlock_t		vstatlock;

/* Forward declarations */

static int bitmap_loadfont_locked(const char *filename);
static struct vmem_cell * set_vmem_cell(size_t x, size_t y, uint16_t cell, uint32_t fg, uint32_t bg);
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
			case 12:
				switch (vstat.charheight) {
					case 20:
						if(conio_fontdata[current_font[i]].twelve_by_twenty==NULL) {
							if (i==0)
								goto error_return;
							else
								FREE_AND_NULL(font[i]);
						}
						else
							memcpy(font[i], conio_fontdata[current_font[i]].twelve_by_twenty, fontsize);
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
	struct vmem_cell *vc;
	struct vmem_cell *fi = fill;
	bool fullredraw = false;

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

	vstat.vmem->changed = true;
	for(y=sy-1;y<ey;y++) {
		vc = vmem_cell_ptr(vstat.vmem, sx - 1, y);
		for(x=sx-1;x<ex;x++) {
			if (vstat.mode == PRESTEL_40X25 && ((vc->bg & CIOLIB_BG_PRESTEL) || (fi->bg & CIOLIB_BG_PRESTEL))) {
				if ((vc->bg & CIOLIB_BG_DOUBLE_HEIGHT) != (fi->bg & CIOLIB_BG_DOUBLE_HEIGHT)) {
					// *ANY* change to double-height potentially changes
					// *EVERY* character on the screen
					fullredraw = true;
				}
			}
			*vc = *(fi++);
			if (vstat.mode == PRESTEL_40X25 && (vc->bg & CIOLIB_BG_PRESTEL)) {
				if (vc->legacy_attr & 0x08) {
					if (cio_api.options & CONIO_OPT_PRESTEL_REVEAL)
						vc->bg |= CIOLIB_BG_REVEAL;
					else
						vc->bg &= ~CIOLIB_BG_REVEAL;
				}
			}
			vc = vmem_next_ptr(vstat.vmem, vc);
		}
	}
	if (fullredraw)
		request_redraw_locked();
	return(1);
}

// vstatlock must be held
static struct vmem_cell *
set_vmem_cell(size_t x, size_t y, uint16_t cell, uint32_t fg, uint32_t bg)
{
	int		altfont;
	int		font;

	vstat.vmem->changed = true;
	bitmap_attr2palette_locked(cell>>8, fg == 0xffffff ? &fg : NULL, bg == 0xffffff ? &bg : NULL);

	altfont = (cell>>11 & 0x01) | ((cell>>14) & 0x02);
	if (!vstat.bright_altcharset)
		altfont &= ~0x01;
	if (!vstat.blink_altcharset)
		altfont &= ~0x02;
	if (vstat.forced_font) {
		if (altfont == 1 && !vstat.forced_font2)
			altfont = 0;
		if (altfont == 2 && !vstat.forced_font3)
			altfont = 0;
		if (altfont == 3 && !vstat.forced_font4)
			altfont = 0;
		font = altfont;
	}
	else
		font=current_font[altfont];
	if (font == -99)
		font = default_font;
	if (font < 0 || font > 255)
		font = 0;

	struct vmem_cell *vc = vmem_cell_ptr(vstat.vmem, x, y);
	vc->legacy_attr = cell >> 8;
	vc->ch = cell & 0xff;
	vc->fg = fg;
	if (vstat.mode == PRESTEL_40X25 && ((vc->bg & CIOLIB_BG_PRESTEL) || (bg & CIOLIB_BG_PRESTEL))) {
		if (vc->legacy_attr & 0x08) {
			if (cio_api.options & CONIO_OPT_PRESTEL_REVEAL)
				vc->bg |= CIOLIB_BG_REVEAL;
			else
				vc->bg &= ~CIOLIB_BG_REVEAL;
		}
		if ((vc->bg & CIOLIB_BG_DOUBLE_HEIGHT) != (bg & CIOLIB_BG_DOUBLE_HEIGHT)) {
			// *ANY* change to double-height potentially changes
			// *EVERY* character on the screen
			request_redraw_locked();
		}
	}
	if (vstat.mode == PRESTEL_40X25 && (vc->bg & CIOLIB_BG_PRESTEL)) {
		if (vc->legacy_attr & 0x08) {
			if (cio_api.options & CONIO_OPT_PRESTEL_REVEAL)
				vc->bg |= CIOLIB_BG_REVEAL;
			else
				vc->bg &= ~CIOLIB_BG_REVEAL;
		}
	}
	vc->bg = bg;
	vc->font = font;
	return vc;
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
	assert_rwlock_rdlock(&vstatlock);
	if (cursor_visible_locked()) {
		if (vstat.mode == PRESTEL_40X25)
			cv = 0x80FFFFFF;
		else
			cv = color_value(ciolib_fg);
		curs_start = vstat.curs_start;
		curs_end = vstat.curs_end;
		curs_row = vstat.curs_row;
		curs_col = vstat.curs_col;
		charheight = vstat.charheight;
		charwidth = vstat.charwidth;
		assert_rwlock_unlock(&vstatlock);
		for (y = curs_start; y <= curs_end; y++) {
			pixel = &data->data[((curs_row - 1) * charheight + y) * data->rect.width + (curs_col - 1) * charwidth];
			for (x = 0; x < charwidth; x++) {
				*(pixel++) = cv;
			}
		}
	}
	else
		assert_rwlock_unlock(&vstatlock);
	assert_pthread_mutex_lock(&callbacks.lock);
	callbacks.drawrect(data);
	callbacks.rects++;
	assert_pthread_mutex_unlock(&callbacks.lock);
}

static void request_redraw_locked(void)
{
	force_redraws = 1;
}

static void request_redraw(void)
{
	assert_rwlock_wrlock(&vstatlock);
	request_redraw_locked();
	assert_rwlock_unlock(&vstatlock);
}

/*
 * Called with the screen lock held
 */
static struct rectlist *alloc_full_rect(struct bitmap_screen *screen, bool allow_throttle)
{
	struct rectlist * ret;

	assert_pthread_mutex_lock(&free_rect_lock);
	if (allow_throttle) {
		if (throttled) {
			assert_pthread_mutex_unlock(&free_rect_lock);
			return NULL;
		}
		if (outstanding_rects >= MAX_OUTSTANDING) {
			throttled = true;
			assert_pthread_mutex_unlock(&free_rect_lock);
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
			assert_pthread_mutex_unlock(&free_rect_lock);
			return ret;
		}
		else {
			free(free_rects->data);
			ret = free_rects->next;
			free(free_rects);
			free_rects = ret;
		}
	}
	assert_pthread_mutex_unlock(&free_rect_lock);

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
	assert_pthread_mutex_lock(&free_rect_lock);
	if (allow_throttle) {
		if (ret)
			outstanding_rects++;
	}
	assert_pthread_mutex_unlock(&free_rect_lock);
	return ret;
}

static uint32_t color_value(uint32_t col)
{
	if (col & CIOLIB_COLOR_RGB)
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
	assert(screen->toprow < screen->screenheight);
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
	bool slow;
	bool gexpand;
	bool did_top;
	bool double_height;
	bool sep;
	bool top_half;
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
	return vc->bg == bs->cheat_colour && (vc->ch == ' ') && (vc->font < CONIO_FIRST_FREE_FONT) && !(vc->bg & CIOLIB_BG_PRESTEL);
}

static void
calc_charstate(struct blockstate *bs, struct vmem_cell *vc, struct charstate *cs, int xpos, int ypos)
{
	bool not_hidden = true;
	cs->slow = bs->font_data_width != 8;
	cs->bg = vc->bg;
	cs->did_top = false;
	cs->sep = false;
	cs->double_height = false;
	cs->top_half = true;

	if (vstat.forced_font) {
		switch (vc->font) {
			case 0:
				cs->font = vstat.forced_font;
				break;
			case 1:
				cs->font = vstat.forced_font2;
				break;
			case 2:
				cs->font = vstat.forced_font3;
				break;
			case 3:
				cs->font = vstat.forced_font4;
				break;
		}
	}
	else {
		if (current_font[0] == -1) {
			cs->font = font[0];
		}
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
				case 20:
					cs->font = (unsigned char *)conio_fontdata[vc->font].twelve_by_twenty;
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
	if (bs->expand) {
		cs->slow = true;
		if (vc->ch >= 0xC0 && vc->ch <= 0xDF)
			cs->gexpand = true;
		else
			cs->gexpand = false;
	}
	uint32_t fg = vc->fg;

	if (vstat.mode == PRESTEL_40X25 && (vc->bg & CIOLIB_BG_PRESTEL)) {
		bool top = false;
		bool bottom = false;
		unsigned char lattr;

		cs->slow = true;
		if (vc->bg & CIOLIB_BG_SEPARATED && vc->ch >= 160)
			cs->sep = true;
		// Start at the first cell...
		struct vmem_cell *pvc = vmem_cell_ptr(vstat.vmem, 0, 0);
		// And check all the rows including this one.
		for (int y = 0; y < ypos; y++) {
			// If the previous line was a top line, this one is a bottom.
			if (top) {
				bottom = true;
				top = false;
			}
			else {
				// If the previous line was a bottom, this is not a bottom
				if (bottom)
					bottom = false;
				// Check for any of these being tops...
				pvc = vmem_cell_ptr(vstat.vmem, 0, y);
				for (int x = 0; x < vstat.cols; x++) {
					// If there's at least one top, this is a top row
					if (pvc->bg & CIOLIB_BG_DOUBLE_HEIGHT) {
						top = true;
						break;
					}
					pvc = vmem_next_ptr(vstat.vmem, pvc);
				}
			}
		}

		// If this is the last row, it can't be a top...
		if (ypos == vstat.rows)
			top = false;

		// If this is a bottom row...
		if (bottom) {
			/*
			 * TODO: With Commstar, it copies to the next
			 * row when it's initially written, so if you
			 * overwrite the double height on the first row,
			 * the second row retains the double height and
			 * becomes tops.
			 *
			 * This violates the Prestel Terminal Specification,
			 * so for now I'm doing this the specified way
			 * rather than emulating the Commstar bug because
			 * it would be too hard to implement in this
			 * implementation.  For v2 however, this should
			 * likely be fixed to have the Commstar bug since
			 * I expect that will be how the Prestel terminals
			 * that use the SAA5050 (ie: most of them) do it.
			 */
			if (vc->bg & CIOLIB_BG_PRESTEL_TERMINAL) {
				pvc = vmem_prev_row_ptr(vstat.vmem, vc);
				cs->bg = pvc->bg;
				fg = pvc->fg;
				if (pvc->bg & CIOLIB_BG_SEPARATED && pvc->ch >= 160)
					cs->sep = true;
			}
			else
				pvc = vc;
			assert(!top);
			if (pvc->bg & CIOLIB_BG_DOUBLE_HEIGHT) {
				cs->double_height = true;
				cs->fontoffset = (pvc->ch) * (vstat.charheight * ((bs->font_data_width + 7) / 8)) + ((vstat.charheight * ((bs->font_data_width + 7) / 8)) / 2);
			}
			else {
				// Draw as space if not double-bottom
				cs->fontoffset=(32) * (vstat.charheight * ((bs->font_data_width + 7) / 8));
			}
			cs->top_half = false;
			lattr = pvc->legacy_attr;
			// Handle blink
			if (vc->bg & CIOLIB_BG_PRESTEL_TERMINAL)
				draw_fg = ((!(lattr & 0x80)) || vstat.no_blink);
		}
		// If not a bottom (either a top or no doubles)
		else {
			// And it's a top row...
			if (top) {
				// And it's double-height
				if (vc->bg & CIOLIB_BG_DOUBLE_HEIGHT) {
					//assert(top);
					// Draw it as is where is
					cs->double_height = true;
				}
			}
			lattr = vc->legacy_attr;
		}
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

/*
 * This only draws 8-bit-wide characters, doesn't mark screens as damaged,
 * doesn't support expanded or double-height, etc.
 * Basically, this is the happy path
 */
static void
draw_char_row_fast(struct blockstate *bs, struct charstate *cs)
{
	const uint8_t mask[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
	const uint8_t fb = cs->font[cs->fontoffset];
	int pixeloffset = bs->pixeloffset;

	for(unsigned x = 0; x < 8; x++) {
		const bool fbb = fb & mask[x];

		if (fbb) {
			screena.rect->data[pixeloffset] = cs->afc;
			screenb.rect->data[pixeloffset] = cs->bfc;
		}
		else {
			screena.rect->data[pixeloffset] = screenb.rect->data[pixeloffset] = cs->bg;
		}

		pixeloffset++;
		assert(pixeloffset < bs->maxpix || x == (vstat.charwidth - 1));
	}
	bs->pixeloffset = pixeloffset;
	cs->fontoffset++;
}

static void
draw_char_row_slow(struct blockstate *bs, struct charstate *cs, uint32_t y)
{
	bool fbb;
	int pixeloffset;

	pixeloffset = bs->pixeloffset;
	if (cs->double_height) {
		y /= 2;
		if (!cs->top_half)
			y += vstat.charheight / 2;
	}

	uint8_t fb = cs->font[cs->fontoffset];
	for(unsigned x = 0; x < vstat.charwidth; x++) {
		unsigned bitnum = x & 0x07;
		if (bs->expand && x == bs->font_data_width) {
			if (cs->gexpand && x > 0)
				fbb = cs->font[cs->fontoffset - 1] & (0x80 >> ((x - 1) & 7));
			else
				fbb = 0;
		}
		else {
			if (bitnum == 0 && x != 0) {
				cs->fontoffset++;
				fb = cs->font[cs->fontoffset];
			}
			fbb = fb & (0x80 >> bitnum);
		}

		if (x == (bs->font_data_width - 1)) {
			cs->fontoffset++;
			fb = cs->font[cs->fontoffset];
		}

		if (cs->sep) {
			if (x == 0 || x == 1 || x == 6 || x == 7 || y == 4 || y == 5 || y == 12 || y == 13 || y == 18 || y == 19)
				fbb = false;
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
	}
	if (cs->double_height) {
		// Go back for next row
		if (!cs->did_top) {
			cs->fontoffset -= ((bs->font_data_width + 7) / 8);
		}
		cs->did_top = !cs->did_top;
	}
	bs->pixeloffset = pixeloffset;
}

static void
bitmap_draw_vmem_locked(int sx, int sy, int ex, int ey, struct vmem_cell *fill)
{
	assert(sx <= ex);
	assert(sy <= ey);
	struct charstate charstate[510]; // ciolib only supports 255 columns, but syncview goes to 510!
	struct blockstate bs;
	unsigned vwidth = ex - sx + 1;
	unsigned vheight  = ey - sy + 1;

	unsigned xoffset = (sx-1) * vstat.charwidth;
	unsigned yoffset = (sy-1) * vstat.charheight;
	bs.expand = vstat.flags & VIDMODES_FLAG_EXPAND;
	bs.font_data_width = vstat.charwidth - (bs.expand ? 1 : 0);

	assert(xoffset + vstat.charwidth <= screena.screenwidth);
	assert(xoffset + vstat.charwidth <= screenb.screenwidth);
	assert(yoffset + vstat.charheight <= screena.screenheight);
	assert(yoffset + vstat.charheight <= screenb.screenheight);
	bs.maxpix = screena.screenwidth * screena.screenheight;

	bs.pixeloffset = pixel_offset(&screena, xoffset, yoffset);
	bs.cheat_colour = fill[0].bg;
	unsigned rsz = screena.screenwidth - vstat.charwidth * vwidth;

	// Fill in charstate for this pass
	bool cheat = true;
	if (vstat.mode == PRESTEL_40X25) {
		cheat = false;
	}
	else {
		for (unsigned vy = 0; vy < vheight; vy++) {
			for (unsigned vx = 0; vx < vwidth; vx++) {
				if (!can_cheat(&bs, &fill[vy * vwidth + vx])) {
					cheat = false;
					break;
				}
			}
			if (!cheat)
				break;
		}
	}
	if (cheat) {
		const unsigned ylim = vheight * vstat.charheight;
		const unsigned xlim = vwidth * vstat.charwidth;
		const int opo = bs.pixeloffset;
		for (unsigned y = 0; y < ylim; y++) {
			for (unsigned vx = 0; vx < xlim; vx++) {
				screena.rect->data[bs.pixeloffset] = bs.cheat_colour;
				bs.pixeloffset++;
				assert(bs.pixeloffset < bs.maxpix || vx == (xlim - 1));
			}
			bs.pixeloffset += rsz;
			if (bs.pixeloffset >= bs.maxpix)
				bs.pixeloffset -= bs.maxpix;
		}
		screena.update_pixels = 1;
		bs.pixeloffset = opo;
		for (unsigned y = 0; y < ylim; y++) {
			for (unsigned vx = 0; vx < xlim; vx++) {
				screenb.rect->data[bs.pixeloffset] = bs.cheat_colour;
				bs.pixeloffset++;
				assert(bs.pixeloffset < bs.maxpix || vx == (xlim - 1));
			}
			bs.pixeloffset += rsz;
			if (bs.pixeloffset >= bs.maxpix)
				bs.pixeloffset -= bs.maxpix;
		}
		screenb.update_pixels = 1;
		int foff = 0;
		for (unsigned vy = 0; vy < vheight; vy++) {
			int coff = vmem_cell_offset(vstat.vmem, sx - 1, sy - 1 + vy);
			for (unsigned vx = 0; vx < vwidth; vx++) {
				if (bitmap_drawn)
					bitmap_drawn[coff] = fill[foff++];
				coff = vmem_next_offset(vstat.vmem, coff);
			}
		}
	}
	else {
		int foff = 0;
		bool didfast = false;
		for (unsigned vy = 0; vy < vheight; vy++) {
			// Fill in charstate for this pass
			int coff = vmem_cell_offset(vstat.vmem, sx - 1, sy - 1 + vy);
			for (unsigned vx = 0; vx < vwidth; vx++) {
				if (bitmap_drawn)
					bitmap_drawn[coff] = fill[foff++];
				coff = vmem_next_offset(vstat.vmem, coff);
				calc_charstate(&bs, &fill[vy * vwidth + vx], &charstate[vx], sx + vx, sy + vy);
				if (charstate[vx].slow == false)
					didfast = true;
			}
			// Draw the characters...
			for (unsigned y = 0; y < vstat.charheight; y++) {
				for (unsigned vx = 0; vx < vwidth; vx++) {
					if (charstate[vx].slow)
						draw_char_row_slow(&bs, &charstate[vx], y);
					else {
						draw_char_row_fast(&bs, &charstate[vx]);
					}
				}
				bs.pixeloffset += rsz;
				if (bs.pixeloffset >= bs.maxpix)
					bs.pixeloffset -= bs.maxpix;
			}
		}
		if (didfast) {
			screena.update_pixels = true;
			screenb.update_pixels = true;
		}
	}
}

static void
bitmap_draw_vmem(int sx, int sy, int ex, int ey, struct vmem_cell *fill)
{
	assert_pthread_mutex_lock(&screenlock);
	bitmap_draw_vmem_locked(sx, sy, ex, ey, fill);
	assert_pthread_mutex_unlock(&screenlock);
}

/***********************************************************/
/* These functions get called from the blinker thread only */
/***********************************************************/

static void cb_flush(void)
{
	assert_pthread_mutex_lock(&callbacks.lock);
	if (callbacks.rects) {
		callbacks.flush();
		callbacks.rects = 0;
	}
	assert_pthread_mutex_unlock(&callbacks.lock);
}

static int check_redraw(void)
{
	int ret;

	assert_rwlock_wrlock(&vstatlock);
	ret = force_redraws;
	force_redraws = 0;
	assert_rwlock_unlock(&vstatlock);
	return ret;
}

/* Blinker Thread */
static void blinker_thread(void *data)
{
	void *rect;
	uint64_t next_blink = 0;
	uint64_t next_cursor = 0;
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
		uint64_t now = xp_timer64();

		assert_rwlock_wrlock(&vstatlock);
		switch (vstat.mode) {
			case PRESTEL_40X25:
				if (next_cursor < now) {
					curs_changed = cursor_visible_locked();
					if (vstat.curs_blink) {
						vstat.curs_blink=FALSE;
					}
					else {
						vstat.curs_blink=TRUE;
					}
					// Timings derived from Keyops Model B
					next_cursor = now + 314;
					curs_changed = (curs_changed != cursor_visible_locked());
					if (next_blink < now) {
						if (vstat.blink) {
							vstat.blink=FALSE;
							vstat.curs_blink = TRUE;
							next_blink = now + 942;
						}
						else {
							vstat.blink=TRUE;
							vstat.curs_blink = FALSE;
							next_blink = now + 314;
						}
						blink_changed = have_blink;
					}
				}
				break;
			case C64_40X25:
			case C128_40X25:
			case C128_80X25:
				if (next_cursor < now) {
					curs_changed = cursor_visible_locked();
					if (vstat.curs_blink) {
						vstat.curs_blink=FALSE;
						vstat.blink=TRUE;
					}
					else {
						vstat.curs_blink=TRUE;
						vstat.blink=FALSE;
					}
					blink_changed = have_blink;
					curs_changed = (curs_changed != cursor_visible_locked());
					next_cursor = now + 333;
				}
				break;
			case ATARI_40X24:
			case ATARI_80X25:
				// No blinking!
				vstat.curs_blink=TRUE;
				vstat.blink=FALSE;
				break;
			case ATARIST_40X25:
			case ATARIST_80X25:
			case ATARIST_80X25_MONO:
				// Cursor blinks
				if (next_cursor < now) {
					vstat.curs_blink = !vstat.curs_blink;
					curs_changed = true;
					next_cursor = now + 500;
				}
				// No text blinking!
				vstat.blink=FALSE;
				break;
			default:
				if (next_cursor < now) {
					curs_changed = cursor_visible_locked();
					if (vstat.curs_blink) {
						vstat.curs_blink=FALSE;
					}
					else {
						vstat.curs_blink=TRUE;
					}
					curs_changed = (curs_changed != cursor_visible_locked());
					next_cursor = now + 133;
					if (next_blink < now) {
						if (vstat.blink) {
							vstat.blink=FALSE;
						}
						else {
							vstat.blink=TRUE;
						}
						next_blink = now + 266;
						blink_changed = have_blink;
					}
				}
				break;
		}
		lfc = force_cursor;
		force_cursor = 0;
		blink = vstat.blink;
		assert_pthread_mutex_lock(&screenlock);
		if (screena.rect == NULL) {
			assert_pthread_mutex_unlock(&screenlock);
			assert_rwlock_unlock(&vstatlock);
			continue;
		}
		assert_pthread_mutex_unlock(&screenlock);
		if (curs_changed || blink_changed || lfc)
			vstat.vmem->changed = true;
		assert_rwlock_unlock(&vstatlock);

		if (check_redraw()) {
			if (update_from_vmem(TRUE))
				request_redraw();
		}
		else {
			if (update_from_vmem(FALSE))
				request_redraw();
		}
		assert_pthread_mutex_lock(&screenlock);
		both_screens(blink, &screen, &ncscreen);
		if (screen->rect == NULL) {
			assert_pthread_mutex_unlock(&screenlock);
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
				assert_pthread_mutex_unlock(&screenlock);
				cb_drawrect(rect);
				cb_flush();
			}
			else
				assert_pthread_mutex_unlock(&screenlock);
		}
		else {
			assert_pthread_mutex_unlock(&screenlock);
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
	// Handles reveal/unreveal updates, modifies vmem
	if (vstat.mode == PRESTEL_40X25 && (c2->bg & CIOLIB_BG_PRESTEL)) {
		if (c2->legacy_attr & 0x08) {
			if (cio_api.options & CONIO_OPT_PRESTEL_REVEAL)
				c2->bg |= CIOLIB_BG_REVEAL;
			else
				c2->bg &= ~CIOLIB_BG_REVEAL;
		}
	}
	if (bitmap_cell->bg & CIOLIB_BG_DIRTY)	// Dirty.
		return false;
	if (bitmap_cell->bg != c2->bg)
		return false;
	if (bitmap_cell->fg != c2->fg)
		return false;
	if (bitmap_cell->font != c2->font)
		return false;
	if (bitmap_cell->legacy_attr != c2->legacy_attr)
		return false;
	return true;
}

static void
bitmap_draw_from_vmem(int sx, int sy, int ex, int ey, bool locked)
{
	int so = vmem_cell_offset(vstat.vmem, sx - 1, sy - 1);
	int eo = vmem_cell_offset(vstat.vmem, ex - 1, ey - 1);
	// Draw first chunk
	if (eo < so) {
		int rows = vstat.vmem->top_row - sy + 1;
		int ney = vstat.vmem->height - rows;
		if (locked)
			bitmap_draw_vmem_locked(sx, sy, ex, ney, &vstat.vmem->vmem[so]);
		else
			bitmap_draw_vmem(sx, sy, ex, ney, &vstat.vmem->vmem[so]);
		so = 0;
		sy += rows;
	}

	// Draw last chunk
	if (locked)
		bitmap_draw_vmem_locked(sx, sy, ex, ey, &vstat.vmem->vmem[so]);
	else
		bitmap_draw_vmem(sx, sy, ex, ey, &vstat.vmem->vmem[so]);
}

/*
 * Checks if there's a blinking character anywhere on the screen and sets
 * have_blink appropriately.
 */
static void
check_blink_locked(void)
{
	have_blink = false;
	if (vstat.no_blink)
		return;

	struct vmem_cell *vc = vstat.vmem->vmem;
	size_t cells = vstat.rows * vstat.cols;
	for (int cell = 0; cell < cells; cell++) {
		if ((vc->legacy_attr & 0x80) && vc->bg != vc->fg) {
			have_blink = true;
			return;
		}
		vc++;
	}
}

/*
 * Updates any changed cells... blinking, modified flags, and the cursor
 * Is also used (with force = TRUE) to completely redraw the screen from
 * vmem (such as in the case of a font load).
 */
static int update_from_vmem(int force)
{
	static struct {
		int cols;
		int rows;
		int bright_background;
		int no_blink;
		int blink_altcharset;
		int no_bright;
		int bright_altcharset;
	} vs;
	int width,height;

	if(!bitmap_initialized)
		return(-1);

	assert_rwlock_rdlock(&vstatlock);

	if (vstat.vmem == NULL) {
		assert_rwlock_unlock(&vstatlock);
		return -1;
	}

	if(vstat.vmem->vmem == NULL) {
		assert_rwlock_unlock(&vstatlock);
		return -1;
	}

	if(!vstat.vmem->changed) {
		assert_rwlock_unlock(&vstatlock);
		return 0;
	}
	vstat.vmem->changed = false;

	/* If we change window size, redraw everything */
	if(bitmap_drawn == NULL || vs.cols!=vstat.cols || vs.rows != vstat.rows) {
		struct vmem_cell *newl = realloc(bitmap_drawn, sizeof(struct vmem_cell) * vstat.cols * vstat.rows);
		if (newl == NULL) {
			vs.cols = 0;
			vs.rows = 0;
			free(bitmap_drawn);
			assert_rwlock_unlock(&vstatlock);
			return -1;
		}
		bitmap_drawn = newl;
		memset(bitmap_drawn, 0x04, sizeof(struct vmem_cell) * vstat.cols * vstat.rows);
		/* Force a full redraw */
		force=1;
	}
	width=vstat.cols;
	height=vstat.rows;

	check_blink_locked();
	if (force || bitmap_drawn == NULL) {
		bitmap_draw_from_vmem(1, 1, width, height, false);
	}
	else {
		unsigned int pos = vmem_cell_offset(vstat.vmem, 0, 0);
		bool bb_attr_changed = false;

		/* Did the meaning of the blink bit change? */
		if (vstat.bright_background != vs.bright_background ||
				vstat.no_blink != vs.no_blink ||
				vstat.blink_altcharset != vs.blink_altcharset)
			bb_attr_changed = true;

		/* Did the meaning of the bright bit change? */
		if (vstat.no_bright != vs.no_bright ||
				vstat.bright_altcharset != vs.bright_altcharset)
			bb_attr_changed = true;

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
		for(int y=0;y<height;y++) {
			for(int x=0;x<width;x++) {
				/* Has this char been updated? */
				bool blink_set = vstat.vmem->vmem[pos].legacy_attr & 0x80;
				if(!same_cell(&bitmap_drawn[pos], &vstat.vmem->vmem[pos])
				    || (bb_attr_changed && blink_set)
				    ) {
					ex = x + 1;
					if (sx == 0) {
						sx = ex;
					}
				}
				else {
					if (sx) {
						bitmap_draw_from_vmem(sx, y + 1, ex, y + 1, false);
						sx = ex = 0;
					}
				}
				pos = vmem_next_offset(vstat.vmem, pos);
			}
			if (sx) {
				bitmap_draw_from_vmem(sx, y + 1, ex, y + 1, false);
				sx = ex = 0;
			}
		}
	}
	vs.cols = vstat.cols;
	vs.rows = vstat.rows;
	vs.bright_background = vstat.bright_background;
	vs.no_blink = vstat.no_blink;
	vs.blink_altcharset = vstat.blink_altcharset;
	vs.no_bright = vstat.no_bright;
	vs.bright_altcharset = vstat.bright_altcharset;
	assert_rwlock_unlock(&vstatlock);

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

	if(!bitmap_initialized)
		return(0);

	if (sx < 1
	    || sy < 1
	    || ex < 1
	    || ey < 1
	    || sx > ex
	    || sy > ey
	    || ex > real_screenwidth
	    || ey > cio_textinfo.screenheight
	    || fill==NULL) {
		return(0);
	}

	assert_rwlock_wrlock(&vstatlock);
	for (y = sy - 1; y < ey; y++) {
		for (x = sx - 1; x < ex; x++) {
			set_vmem_cell(x, y, *(buf++), 0x00ffffff, 0x00ffffff);
		}
	}
	assert_rwlock_unlock(&vstatlock);
	return ret;
}

int
bitmap_vmem_puttext(int sx, int sy, int ex, int ey, struct vmem_cell *fill)
{
	int ret;

	assert_rwlock_wrlock(&vstatlock);
	ret = bitmap_vmem_puttext_locked(sx, sy, ex, ey, fill);
	assert_rwlock_unlock(&vstatlock);
	return ret;
}

static bool
bitmap_vmem_gettext_invalid(int sx, int sy, int ex, int ey, struct vmem_cell *fill)
{
	if(!bitmap_initialized)
		return true;

	if(		   sx < 1
			|| sy < 1
			|| ex < 1
			|| ey < 1
			|| sx > ex
			|| sy > ey
			|| ex > real_screenwidth
			|| ey > cio_textinfo.screenheight
			|| fill==NULL) {
		return true;
	}
	return false;
}

static int
bitmap_vmem_gettext_locked_inner(int sx, int sy, int ex, int ey, struct vmem_cell *fill)
{
	int x,y;

	for(y=sy-1;y<ey;y++) {
		struct vmem_cell *vc = vmem_cell_ptr(vstat.vmem, sx - 1, y);
		for(x=sx-1;x<ex;x++) {
			*(fill++) = *vc;
			vc = vmem_next_ptr(vstat.vmem, vc);
		}
	}
	return(1);
}

static int
bitmap_vmem_gettext_locked(int sx, int sy, int ex, int ey, struct vmem_cell *fill)
{
	if (bitmap_vmem_gettext_invalid(sx, sy, ex, ey, fill))
		return 0;

	return bitmap_vmem_gettext_locked_inner(sx, sy, ex, ey, fill);
}

int bitmap_vmem_gettext(int sx, int sy, int ex, int ey, struct vmem_cell *fill)
{
	if (bitmap_vmem_gettext_invalid(sx, sy, ex, ey, fill))
		return 0;

	assert_rwlock_rdlock(&vstatlock);
	int ret = bitmap_vmem_gettext_locked_inner(sx, sy, ex, ey, fill);
	assert_rwlock_unlock(&vstatlock);
	return(ret);
}

void bitmap_gotoxy(int x, int y)
{
	if(!bitmap_initialized)
		return;
	/* Move cursor location */
	assert_rwlock_wrlock(&vstatlock);
	if (vstat.curs_col != x + cio_textinfo.winleft - 1 || vstat.curs_row != y + cio_textinfo.wintop - 1) {
		cio_textinfo.curx=x;
		cio_textinfo.cury=y;
		vstat.curs_col = x + cio_textinfo.winleft - 1;
		vstat.curs_row = y + cio_textinfo.wintop - 1;
		if (cursor_visible_locked())
			force_cursor = 1;
	}
	assert_rwlock_unlock(&vstatlock);
}

void bitmap_setcursortype(int type)
{
	if(!bitmap_initialized)
		return;
	assert_rwlock_wrlock(&vstatlock);
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
	assert_rwlock_unlock(&vstatlock);
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

	if(conio_fontdata[font].eight_by_sixteen!=NULL) {
		newmode=C80;
		if (conio_fontdata[font].cp == CIOLIB_PRESTEL)
			newmode=PRESTEL_40X25;
	}
	else if(conio_fontdata[font].eight_by_fourteen!=NULL)
		newmode=C80X28;
	else if(conio_fontdata[font].eight_by_eight!=NULL)
		newmode=C80X50;

	assert_rwlock_wrlock(&vstatlock);
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
		case 20:
			if(conio_fontdata[font].twelve_by_twenty==NULL) {
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

		// Tell coverity this assignment is not protected by the lock
		// coverity[def:SUPPRESS]
		old=malloc(ow*oh*sizeof(*old));
		if(old) {
			/*
			 * Passing old to bitmap_vmem_gettext_locked()
			 * gets Coverity worked up about old being accessed
			 * after an unlock/lock, so we have the suppressions
			 * below.
			 */
			bitmap_vmem_gettext_locked(1,1,ow,oh,old);
			assert_rwlock_unlock(&vstatlock);
			textmode(newmode);
			assert_rwlock_wrlock(&vstatlock);
			new=malloc(ti.screenwidth*ti.screenheight*sizeof(*new));
			if(!new) {
				free(old);
				assert_rwlock_unlock(&vstatlock);
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
	assert_rwlock_unlock(&vstatlock);
	return(1);

error_return:
	assert_rwlock_unlock(&vstatlock);
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

	assert_rwlock_wrlock(&vstatlock);
	ret = bitmap_loadfont_locked(filename);
	assert_rwlock_unlock(&vstatlock);
	return ret;
}

static void
bitmap_movetext_screen(int x, int y, int tox, int toy, int direction, int height, int width)
{
	int32_t sdestoffset;
	ssize_t ssourcepos;
	int step;
	int32_t screeny;

	int pheight = height * vstat.charheight;
	int ptoy = (toy - 1) * vstat.charheight;
	int py = (y - 1) * vstat.charheight;
	int ptox = (tox - 1) * vstat.charwidth;
	int px = (x - 1) * vstat.charwidth;
	assert_pthread_mutex_lock(&screenlock);
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

		int yoff = toy - y;
		height = vstat.rows - height;
		toy = vstat.rows - (height - 1);
		// Fill the bits with impossible data so they're redrawn
		int bdoff = vmem_cell_offset(vstat.vmem, 0, toy - 1);
		for (int vy = 0; vy < height; vy++) {
			if (bitmap_drawn)
				memset(&bitmap_drawn[bdoff], 0x04, sizeof(*bitmap_drawn) * vstat.cols);
			bdoff = vmem_next_row_offset(vstat.vmem, bdoff);
		}
		if (vstat.charheight * vstat.rows == screena.screenheight) {
			assert_pthread_mutex_unlock(&screenlock);
			return;
		}
		// Move stuff below the bottom row of text back
		pheight = screena.screenheight - (vstat.charheight * vstat.rows);
		ptoy = screena.screenheight - pheight;
		py = ptoy + (yoff * vstat.charheight);
		if (py < 0)
			py += screena.screenheight;
		if (py >= screena.screenheight)
			py -= screena.screenheight;
	}

	int maxpos = screena.screenwidth * screena.screenheight;
	if (direction == -1) {
		ssourcepos =   (py   + pheight - 1) * vstat.scrnwidth + px;
		sdestoffset = ((ptoy + pheight - 1) * vstat.scrnwidth + ptox) - ssourcepos;
	}
	else {
		ssourcepos =   py   * vstat.scrnwidth + px;
		sdestoffset = (ptoy * vstat.scrnwidth + ptox) - ssourcepos;
	}
	ssourcepos += screena.toprow * screena.screenwidth;
	if (ssourcepos >= maxpos)
		ssourcepos -= maxpos;
	step = direction * vstat.scrnwidth;
	for(screeny=0; screeny < pheight; screeny++) {
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
	assert_pthread_mutex_unlock(&screenlock);
}

int bitmap_movetext(int x, int y, int ex, int ey, int tox, int toy)
{
	bool scrolldown = false;
	int	cy;
	int width=ex-x+1;
	int height=ey-y+1;
	int soff;
	int doff;

	if(		   x<1
			|| y<1
			|| ex<1
			|| ey<1
			|| tox<1
			|| toy<1
			|| x>real_screenwidth
			|| ex>real_screenwidth
			|| tox>real_screenwidth
			|| (tox + width - 1) > real_screenwidth
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
		scrolldown = true;
	int otoy = toy;
	int oy = y;
	int oheight = height;
	bool oscrolldown = scrolldown;

	assert_rwlock_wrlock(&vstatlock);
	vstat.vmem->changed = true;
	if (width == vstat.cols && height > vstat.rows / 2 && toy == 1) {
		vstat.vmem->top_row += (y - toy);
		if (vstat.vmem->top_row >= vstat.vmem->height)
			vstat.vmem->top_row -= vstat.vmem->height;
		if (vstat.vmem->top_row < 0)
			vstat.vmem->top_row += vstat.vmem->height;

		// Set up the move back down...
		scrolldown = !scrolldown;
		height = vstat.rows - height - 1;
		toy = vstat.rows - (height - 1);
		y = vstat.rows - height;
	}
	if (scrolldown) {
		soff = vmem_cell_offset(vstat.vmem, x - 1, y + height - 2);
		doff = vmem_cell_offset(vstat.vmem, tox - 1, toy + height - 2);
	}
	else {
		soff = vmem_cell_offset(vstat.vmem, x - 1, y - 1);
		doff = vmem_cell_offset(vstat.vmem, tox - 1, toy - 1);
	}
	for(cy=0; cy<height; cy++) {
		memmove(&vstat.vmem->vmem[doff], &vstat.vmem->vmem[soff], sizeof(vstat.vmem->vmem[0])*width);
		if (bitmap_drawn)
			memmove(&bitmap_drawn[doff], &bitmap_drawn[soff], sizeof(vstat.vmem->vmem[0])*width);
		if (scrolldown) {
			soff = vmem_prev_row_offset(vstat.vmem, soff);
			doff = vmem_prev_row_offset(vstat.vmem, doff);
		}
		else {
			soff = vmem_next_row_offset(vstat.vmem, soff);
			doff = vmem_next_row_offset(vstat.vmem, doff);
		}
	}

	// Make the whole thing redraw
	if (vstat.mode == PRESTEL_40X25) {
		if (bitmap_drawn)
			memset(bitmap_drawn, 0x04, sizeof(struct vmem_cell) * vstat.cols * vstat.rows);
	}
	else
		bitmap_movetext_screen(x, oy, tox, otoy, oscrolldown ? -1 : 1, oheight, width);
	assert_rwlock_unlock(&vstatlock);

	return(1);
}

void bitmap_clreol(void)
{
	int x;
	WORD fill=(cio_textinfo.attribute<<8)|' ';
	int row;

	if(!bitmap_initialized)
		return;

	row = cio_textinfo.cury + cio_textinfo.wintop - 1;
	assert_rwlock_wrlock(&vstatlock);
	for(x=cio_textinfo.curx+cio_textinfo.winleft-2; x<cio_textinfo.winright; x++) {
		set_vmem_cell(x, row - 1, fill, ciolib_fg, ciolib_bg);
	}
	assert_rwlock_unlock(&vstatlock);
}

void bitmap_clrscr(void)
{
	size_t x, y;
	WORD fill = (cio_textinfo.attribute << 8) | ' ';
	int rows, cols;

	if(!bitmap_initialized)
		return;
	assert_rwlock_wrlock(&vstatlock);
	rows = vstat.rows;
	cols = vstat.cols;
	for (y = cio_textinfo.wintop - 1; y < cio_textinfo.winbottom && y < rows; y++) {
		for (x = cio_textinfo.winleft - 1; x < cio_textinfo.winright && x < cols; x++) {
			set_vmem_cell(x, y, fill, ciolib_fg, ciolib_bg);
		}
	}
	assert_rwlock_unlock(&vstatlock);
}

void bitmap_getcustomcursor(int *s, int *e, int *r, int *b, int *v)
{
	assert_rwlock_rdlock(&vstatlock);
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
	assert_rwlock_unlock(&vstatlock);
}

void bitmap_setcustomcursor(int s, int e, int r, int b, int v)
{
	double ratio;

	assert_rwlock_wrlock(&vstatlock);
	if(r==0)
		ratio=1;
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
	assert_rwlock_unlock(&vstatlock);
}

static void
setvideoflags_from_vstat(void)
{
	int flags = 0;

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
	protected_int32_set(&videoflags, flags);
}

int bitmap_getvideoflags(void)
{
	return protected_int32_value(videoflags);
}

void bitmap_setvideoflags(int flags)
{
	assert_rwlock_wrlock(&vstatlock);
	protected_int32_set(&videoflags, flags);
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
	assert_rwlock_unlock(&vstatlock);
}

int bitmap_attr2palette(uint8_t attr, uint32_t *fgp, uint32_t *bgp)
{
	int ret;

	assert_rwlock_rdlock(&vstatlock);
	ret = bitmap_attr2palette_locked(attr, fgp, bgp);
	assert_rwlock_unlock(&vstatlock);

	return ret;
}

int bitmap_setpixel(uint32_t x, uint32_t y, uint32_t colour)
{
	int xchar = x / vstat.charwidth;
	int ychar = y / vstat.charheight;

	assert_rwlock_wrlock(&vstatlock);
	vstat.vmem->changed = true;
	assert_pthread_mutex_lock(&screenlock);
	if (screena.rect == NULL || screenb.rect == NULL || x >= screena.screenwidth || y >= screena.screenheight) {
		assert_pthread_mutex_unlock(&screenlock);
		assert_rwlock_unlock(&vstatlock);
		return 0;
	}
	if (xchar < vstat.cols && ychar < vstat.rows) {
		int off = vmem_cell_offset(vstat.vmem, xchar, ychar);
		if (bitmap_drawn == NULL || !same_cell(&bitmap_drawn[off], &vstat.vmem->vmem[off])) {
			bitmap_draw_from_vmem(xchar + 1, ychar + 1, xchar + 1, ychar + 1, true);
		}
		vstat.vmem->vmem[off].bg |= CIOLIB_BG_PIXEL_GRAPHICS;
		if (bitmap_drawn)
			bitmap_drawn[off].bg |= CIOLIB_BG_PIXEL_GRAPHICS;
	}
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
	assert_pthread_mutex_unlock(&screenlock);
	assert_rwlock_unlock(&vstatlock);

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

	if (y_off > pixels->height)
		return 0;
	if (x_off > pixels->width)
		return 0;

	width = ex - sx + 1;
	height = ey - sy + 1;

	if (width + x_off > pixels->width)
		return 0;

	if (height + y_off > pixels->height)
		return 0;

	if (mask != NULL) {
		if (mx_off > mask->width)
			return 0;
		if (my_off > mask->height)
			return 0;
		if (width + mx_off > mask->width)
			return 0;
		if (height + my_off > mask->height)
			return 0;
	}

	assert_rwlock_wrlock(&vstatlock);
	vstat.vmem->changed = true;
	assert_pthread_mutex_lock(&screenlock);
	if (ex > screena.screenwidth || ey > screena.screenheight) {
		assert_pthread_mutex_unlock(&screenlock);
		assert_rwlock_unlock(&vstatlock);
		return 0;
	}

	int charsx = sx / vstat.charwidth;
	int charx = charsx;
	int chary = sy / vstat.charheight;
	int cpx = sx % vstat.charwidth;
	int cpy = sy % vstat.charheight;
	bool xupdated = false;
	bool yupdated = false;
	int off = INT_MIN;
	int crows = vstat.rows * vstat.charheight;
	int ccols = vstat.cols * vstat.charwidth;
	for (y = sy; y <= ey; y++) {
		pos = pixels->width*(y-sy+y_off)+x_off;
		bool in_text_area = y < crows;
		if (in_text_area && !yupdated) {
			charx = charsx;
			off = vmem_cell_offset(vstat.vmem, charx, chary);
		}
		if (mask == NULL) {
			for (x = sx; x <= ex; x++) {
				if (x >= ccols)
					in_text_area = false;
				if (in_text_area) {
					if (!yupdated) {
						if (!xupdated) {
							if (bitmap_drawn == NULL || !same_cell(&bitmap_drawn[off], &vstat.vmem->vmem[off])) {
								bitmap_draw_from_vmem(charx + 1, chary + 1, charx + 1, chary + 1, true);
							}
							if (vstat.vmem && vstat.vmem->vmem) {
								vstat.vmem->vmem[off].bg |= CIOLIB_BG_PIXEL_GRAPHICS;
							}
							if (bitmap_drawn) {
								bitmap_drawn[off].bg |= CIOLIB_BG_PIXEL_GRAPHICS;
							}
							xupdated = true;
						}
					}
					if (++cpx >= vstat.charwidth) {
						cpx = 0;
						charx++;
						xupdated = false;
						assert(off >= 0);
						off = vmem_next_offset(vstat.vmem, off);
					}
				}
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
				if (x >= ccols)
					in_text_area = false;
				if (in_text_area) {
					if (!yupdated) {
						if (!xupdated) {
							if (bitmap_drawn == NULL || !same_cell(&bitmap_drawn[off], &vstat.vmem->vmem[off])) {
								bitmap_draw_from_vmem(charx + 1, chary + 1, charx + 1, chary + 1, true);
							}
							if (vstat.vmem && vstat.vmem->vmem) {
								vstat.vmem->vmem[off].bg |= CIOLIB_BG_PIXEL_GRAPHICS;
							}
							if (bitmap_drawn) {
								bitmap_drawn[off].bg |= CIOLIB_BG_PIXEL_GRAPHICS;
							}
							xupdated = true;
						}
					}
					if (++cpx >= vstat.charwidth) {
						cpx = 0;
						charx++;
						xupdated = false;
						off = vmem_next_offset(vstat.vmem, off);
					}
				}
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
		if (y < crows) {
			cpy++;
			if (cpy >= vstat.charheight) {
				chary++;
				cpy = 0;
				yupdated = false;
				xupdated = false;
			}
			else
				yupdated = true;
		}
	}
	assert_pthread_mutex_unlock(&screenlock);
	assert_rwlock_unlock(&vstatlock);

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
	assert_pthread_mutex_lock(&screenlock);
	if (ex >= screena.screenwidth || ey >= screena.screenheight ||
	    ex >= screenb.screenwidth || ey >= screenb.screenheight) {
		assert_pthread_mutex_unlock(&screenlock);
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
	assert_pthread_mutex_unlock(&screenlock);

	return pixels;
}

int bitmap_get_modepalette(uint32_t p[16])
{
	assert_rwlock_rdlock(&vstatlock);
	memcpy(p, vstat.palette, sizeof(vstat.palette));
	assert_rwlock_unlock(&vstatlock);
	return 1;
}

int bitmap_set_modepalette(uint32_t p[16])
{
	assert_rwlock_wrlock(&vstatlock);
	memcpy(vstat.palette, p, sizeof(vstat.palette));
	assert_rwlock_unlock(&vstatlock);
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

	assert_pthread_mutex_lock(&screenlock);
	switch (size) {
		case 10240:
			FREE_AND_NULL(conio_fontdata[id].twelve_by_twenty);
			conio_fontdata[id].twelve_by_twenty=data;
			FREE_AND_NULL(conio_fontdata[id].desc);
			conio_fontdata[id].desc=name;
			break;
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
	assert_pthread_mutex_unlock(&screenlock);
	request_redraw();
}

int bitmap_setpalette(uint32_t index, uint16_t r, uint16_t g, uint16_t b)
{
	if (index > 65535)
		return 0;

	assert_pthread_mutex_lock(&screenlock);
	palette[index] = (0xff << 24) | ((r>>8) << 16) | ((g>>8) << 8) | (b>>8);
	screena.update_pixels = 1;
	screenb.update_pixels = 1;
	assert_pthread_mutex_unlock(&screenlock);
	return 1;
}

// Called with vstatlock
static int init_screens(int *width, int *height)
{
	assert_pthread_mutex_lock(&screenlock);
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
		assert_pthread_mutex_unlock(&screenlock);
		return(-1);
	}
	screena.toprow = 0;
	screenb.rect = alloc_full_rect(&screenb, false);
	if (screenb.rect == NULL) {
		bitmap_drv_free_rect(screena.rect);
		screena.rect = NULL;
		assert_pthread_mutex_unlock(&screenlock);
		return(-1);
	}
	screenb.toprow = 0;
	memset_u32(screena.rect->data, color_value(vstat.palette[0]), screena.rect->rect.width * screena.rect->rect.height);
	memset_u32(screenb.rect->data, color_value(vstat.palette[0]), screenb.rect->rect.width * screenb.rect->rect.height);
	assert_pthread_mutex_unlock(&screenlock);
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

static void
integer_scale(int maxwidth, int maxheight, int64_t os)
{
	int64_t bs;
	int64_t ls;
	int64_t ns;
	int w, h;

	int mult = 1;
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
}

const double precision = 0.00005;
const double tolerance = 0.0125;

static void
double_scale(int maxwidth, int maxheight, int64_t os)
{
	int64_t bs;
	int64_t ls;
	int64_t ns;
	int w, h;

	double mult = 1.0;
	double step = 1.0;
	bitmap_get_scaled_win_size(mult, &w, &h, maxwidth, maxheight);
	bs = ((int64_t)w * w) + ((int64_t)h * h);
	ls = bs;
	ns = bs;
	while (ns < os) {
		mult += step;
		bitmap_get_scaled_win_size(mult, &w, &h, 0, 0);
		if ((maxwidth > 0) && (w > maxwidth)) {
			mult -= step;
			if (step <= precision) {
				ns = ls;
				break;
			}
			step /= 2;
			continue;
		}
		if (w == maxwidth)
			break;
		if ((maxheight > 0) && (h > maxheight)) {
			mult -= step;
			if (step <= precision) {
				ns = ls;
				break;
			}
			step /= 2;
			continue;
		}
		if (h == maxheight)
			break;
		bs = ((int64_t)w * w) + ((int64_t)h * h);
		if (bs >= os && step > precision) {
			mult -= step;
			step /= 2;
			continue;
		}
		ls = ns;
		ns = bs;
	}
	long long lastDelta = llabs(os - ls);
	long long currDelta = llabs(ns - os);
	if (lastDelta <= currDelta) {
		if (mult > 1) {
			mult -= step;
			currDelta = lastDelta;
		}
	}
	bitmap_get_scaled_win_size(mult, &w, &h, maxwidth, maxheight);
	vstat.winwidth = w;
	vstat.winheight = h;
	vstat.scaling = mult;
}

static double oldMult = 0.0;
static double newMult = 0.0;
static int oldWWidth = 0;
static int oldWHeight = 0;
static int oldSWidth = 0;
static int oldSHeight = 0;

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

	if(!bitmap_initialized)
		return(-1);

	int prevSWidth = vstat.scrnwidth;
	int prevSHeight = vstat.scrnheight;

	if (mode == _ORIGMODE)
		mode = C80;
	if(load_vmode(&vstat, mode)) {
		return(-1);
	}
	setvideoflags_from_vstat();

	// Save the old diagonal (no point is sqrting here)
	os = ((int64_t)vstat.winwidth * vstat.winwidth) + ((int64_t)vstat.winheight * vstat.winheight);

	/* Initialize video memory with black background, white foreground */
	vstat.vmem->changed = true;
	for (i = 0; i < vstat.cols*vstat.rows; ++i) {
		if (i > 0)
			vstat.vmem->vmem[i] = vstat.vmem->vmem[0];
		else {
			vstat.vmem->vmem[i].ch = 0;
			vstat.vmem->vmem[i].legacy_attr = vstat.currattr;
			vstat.vmem->vmem[i].font = default_font == -99 ? 0 : default_font;
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

	if (vstat.cols > 0xff) {
		real_screenwidth = vstat.cols;
		cio_textinfo.screenwidth = 0xff;
	}
	else
		real_screenwidth = cio_textinfo.screenwidth = vstat.cols;

	cio_textinfo.curx=1;
	cio_textinfo.cury=1;
	cio_textinfo.winleft=1;
	cio_textinfo.wintop=1;
	cio_textinfo.winright=cio_textinfo.screenwidth;
	cio_textinfo.winbottom=cio_textinfo.screenheight;

	// Now calculate the closest diagonal new size that's smaller than max...
	double delta = fabs(vstat.scaling - round(vstat.scaling));
	if (delta < tolerance)
		integer_scale(maxwidth, maxheight, os);
	else {
		// If we're going back to the old scaling, and we haven't scaled, just restore the damn thing.
		if (oldSWidth == vstat.scrnwidth && oldSHeight == vstat.scrnheight && vstat.scaling == newMult) {
			vstat.winwidth = oldWWidth;
			vstat.winheight = oldWHeight;
			vstat.scaling = oldMult;
		}
		else {
			int w, h;
			// Tuck this away...
			oldSWidth = prevSWidth;
			oldSHeight = prevSHeight;
			oldWWidth = vstat.winwidth;
			oldWHeight = vstat.winheight;
			oldMult = vstat.scaling;
			double_scale(maxwidth, maxheight, os);
			// Round-trip the scaling so it doesn't change randomly later...
			bitmap_get_scaled_win_size(vstat.scaling, &w, &h, 0, 0);
			vstat.scaling = bitmap_double_mult_inside(w, h);
		}
		newMult = vstat.scaling;
	}

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
	protected_int32_init(&videoflags, 0);
	assert_pthread_mutex_init(&callbacks.lock, NULL);
	assert_rwlock_init(&vstatlock);
	assert_pthread_mutex_init(&screenlock, NULL);
	assert_pthread_mutex_init(&free_rect_lock, NULL);
	assert_rwlock_wrlock(&vstatlock);
	vstat.flags = VIDMODES_FLAG_PALETTE_VMEM;
	assert_pthread_mutex_lock(&screenlock);
	for (i = 0; i < sizeof(dac_default)/sizeof(struct dac_colors); i++) {
		palette[i] = (0xffU << 24) | (dac_default[i].red << 16) | (dac_default[i].green << 8) | dac_default[i].blue;
	}
	assert_pthread_mutex_unlock(&screenlock);
	assert_rwlock_unlock(&vstatlock);

	callbacks.drawrect=drawrect_cb;
	callbacks.flush=flush_cb;
	assert_pthread_mutex_lock(&callbacks.lock);
	callbacks.rects = 0;
	assert_pthread_mutex_unlock(&callbacks.lock);
	bitmap_initialized=1;
	_beginthread(blinker_thread,0,NULL);

	return(0);
}

void bitmap_drv_request_pixels(void)
{
	assert_pthread_mutex_lock(&screenlock);
	if (screena.update_pixels == 0)
		screena.update_pixels = 2;
	if (screenb.update_pixels == 0)
		screenb.update_pixels = 2;
	assert_pthread_mutex_unlock(&screenlock);
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
	assert_pthread_mutex_lock(&free_rect_lock);
	if (rect->throttle) {
		outstanding_rects--;
		if (outstanding_rects < MAX_OUTSTANDING && throttled) {
			throttled = false;
		}
	}
	rect->next = free_rects;
	free_rects = rect;
	assert_pthread_mutex_unlock(&free_rect_lock);
}
