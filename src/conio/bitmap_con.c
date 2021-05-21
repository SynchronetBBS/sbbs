/* $Id: bitmap_con.c,v 1.148 2020/06/27 00:04:44 deuce Exp $ */

#include <stdarg.h>
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

struct bitmap_screen {
	uint32_t *screen;
	int		screenwidth;
	int		screenheight;
	pthread_mutex_t	screenlock;
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
struct rectlist *free_rects;
pthread_mutex_t free_rect_lock;

/* The read lock must be held here. */
#define PIXEL_OFFSET(screen, x, y)	( (y)*(screen).screenwidth+(x) )

/* Exported globals */

pthread_mutex_t		vstatlock;
pthread_mutex_t blinker_lock;

/* Forward declarations */

static int bitmap_loadfont_locked(char *filename);
static void set_vmem_cell(struct vstat_vmem *vmem_ptr, size_t pos, uint16_t cell, uint32_t fg, uint32_t bg);
static int bitmap_attr2palette_locked(uint8_t attr, uint32_t *fgp, uint32_t *bgp);
static void	cb_drawrect(struct rectlist *data);
static void request_redraw_locked(void);
static void request_redraw(void);
static void memset_u32(void *buf, uint32_t u, size_t len);
static int bitmap_draw_one_char(unsigned int xpos, unsigned int ypos);
static void cb_flush(void);
static int check_redraw(void);
static void blinker_thread(void *data);
static __inline void both_screens(struct bitmap_screen** current, struct bitmap_screen** noncurrent);
static int update_from_vmem(int force);
static int bitmap_vmem_puttext_locked(int sx, int sy, int ex, int ey, struct vmem_cell *fill);
static uint32_t color_value(uint32_t col);
void bitmap_drv_free_rect(struct rectlist *rect);

/**************************************************************/
/* These functions get called from the driver and ciolib only */
/**************************************************************/

static int bitmap_loadfont_locked(char *filename)
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
	fdw = vstat.charwidth - (vstat.flags & VIDMODES_FLAG_EXPAND) ? 1 : 0;
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

static int bitmap_vmem_puttext_locked(int sx, int sy, int ex, int ey, struct vmem_cell *fill)
{
	int x,y;
	struct vstat_vmem *vmem_ptr;

	if(!bitmap_initialized)
		return(0);

	if(		   sx < 1
			|| sy < 1
			|| ex < 1
			|| ey < 1
			|| sx > cio_textinfo.screenwidth
			|| sy > cio_textinfo.screenheight
			|| sx > ex
			|| sy > ey
			|| ex > cio_textinfo.screenwidth
			|| ey > cio_textinfo.screenheight
			|| fill==NULL)
		return(0);

	pthread_mutex_lock(&vstatlock);
	vmem_ptr = get_vmem(&vstat);
	for(y=sy-1;y<ey;y++) {
		for(x=sx-1;x<ex;x++) {
			memcpy(&vmem_ptr->vmem[y*cio_textinfo.screenwidth+x], fill++, sizeof(*fill));
			bitmap_draw_one_char(x+1, y+1);
		}
	}
	release_vmem(vmem_ptr);
	pthread_mutex_unlock(&vstatlock);
	return(1);
}

static void
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
	if (cursor_visible_locked()) {
		cv = color_value(ciolib_fg);
		for (y = vstat.curs_start; y <= vstat.curs_end; y++) {
			pixel = &data->data[((vstat.curs_row - 1) * vstat.charheight + y) * data->rect.width + (vstat.curs_col - 1) * vstat.charwidth];
			for (x = 0; x < vstat.charwidth; x++) {
				*(pixel++) = cv;
			}
		}
	}
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
static struct rectlist *alloc_full_rect(struct bitmap_screen *screen)
{
	struct rectlist * ret;

	pthread_mutex_lock(&free_rect_lock);
	while (free_rects) {
		if (free_rects->rect.width == screen->screenwidth && free_rects->rect.height == screen->screenheight) {
			ret = free_rects;
			free_rects = free_rects->next;
			ret->next = NULL;
			ret->rect.x = ret->rect.y = 0;
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
	ret->next = NULL;
	ret->rect.x = 0;
	ret->rect.y = 0;
	ret->rect.width = screen->screenwidth;
	ret->rect.height = screen->screenheight;
	ret->data = malloc(ret->rect.width * ret->rect.height * sizeof(ret->data[0]));
	if (ret->data == NULL)
		FREE_AND_NULL(ret);
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

	// TODO: Some sort of caching here would make things faster...?
	if(callbacks.drawrect) {
		rect = alloc_full_rect(screen);
		if (!rect)
			return rect;
		memcpy(rect->data, screen->rect->data, sizeof(*rect->data) * screen->screenwidth * screen->screenheight);
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

/*
 * vstatlock needs to be held.
 */
static int bitmap_draw_one_char(unsigned int xpos, unsigned int ypos)
{
	uint32_t fg;
	uint32_t bg;
	int fdw;
	int		xoffset=(xpos-1)*vstat.charwidth;
	int		yoffset=(ypos-1)*vstat.charheight;
	int		x;
	int		fdx;
	uint8_t fb = 0;
	int		y;
	int		fontoffset;
	int		pixeloffset;
	unsigned char *this_font;
	WORD	sch;
	struct vstat_vmem *vmem_ptr;
	BOOL	draw_fg = TRUE;

	if(!bitmap_initialized) {
		return(-1);
	}

	vmem_ptr = vstat.vmem;

	if(!vmem_ptr) {
		return(-1);
	}

	sch=vmem_ptr->vmem[(ypos-1)*cio_textinfo.screenwidth+(xpos-1)].legacy_attr << 8 | vmem_ptr->vmem[(ypos-1)*cio_textinfo.screenwidth+(xpos-1)].ch;
	fg = vmem_ptr->vmem[(ypos-1)*cio_textinfo.screenwidth+(xpos-1)].fg;
	bg = vmem_ptr->vmem[(ypos-1)*cio_textinfo.screenwidth+(xpos-1)].bg;

	if (vstat.forced_font) {
		this_font = vstat.forced_font;
	}
	else {
		if (current_font[0] == -1)
			this_font = font[0];
		else {
			switch (vstat.charheight) {
				case 8:
					this_font = (unsigned char *)conio_fontdata[vmem_ptr->vmem[(ypos-1)*cio_textinfo.screenwidth+(xpos-1)].font].eight_by_eight;
					break;
				case 14:
					this_font = (unsigned char *)conio_fontdata[vmem_ptr->vmem[(ypos-1)*cio_textinfo.screenwidth+(xpos-1)].font].eight_by_fourteen;
					break;
				case 16:
					this_font = (unsigned char *)conio_fontdata[vmem_ptr->vmem[(ypos-1)*cio_textinfo.screenwidth+(xpos-1)].font].eight_by_sixteen;
					break;
				default:
					return(-1);
			}
		}
	}
	if (this_font == NULL)
		this_font = font[0];
	fdw = vstat.charwidth - (vstat.flags & VIDMODES_FLAG_EXPAND) ? 1 : 0;
	fontoffset=(sch & 0xff) * (vstat.charheight * ((fdw + 7) / 8));

	pthread_mutex_lock(&screena.screenlock);
	pthread_mutex_lock(&screenb.screenlock);

	if ((xoffset + vstat.charwidth > screena.screenwidth) || (yoffset + vstat.charheight > screena.screenheight) ||
	    (xoffset + vstat.charwidth > screenb.screenwidth) || (yoffset + vstat.charheight > screenb.screenheight)) {
		pthread_mutex_unlock(&screenb.screenlock);
		pthread_mutex_unlock(&screena.screenlock);
		return(-1);
	}

	if((!screena.screen) || (!screenb.screen)) {
		pthread_mutex_unlock(&screenb.screenlock);
		pthread_mutex_unlock(&screena.screenlock);
		return(-1);
	}

	draw_fg = ((!(sch & 0x8000)) || vstat.no_blink);
	pixeloffset = PIXEL_OFFSET(screena, xoffset, yoffset);
	for(y=0; y<vstat.charheight; y++) {
		for(x=0; x<vstat.charwidth; x++) {
			fdx = x;
			fb = this_font[fontoffset];
			if ((x & 0x07) == 7)
				fontoffset++;
			if (vstat.flags & VIDMODES_FLAG_EXPAND) {
				if (x == vstat.charwidth - 1) {
					fontoffset--;
					fdx--;
					if (!(vstat.flags & VIDMODES_FLAG_LINE_GRAPHICS_EXPAND)) {
						fb = 0;
					}
					else if ((sch & 0xff) >= 0xC0 && (sch & 0xff) <= 0xDF) {
						fb = this_font[fontoffset];
					}
					else
						fb = 0;
					
				}
			}

			if(fb & (0x80 >> (fdx & 7)) && draw_fg) {
				if (screena.screen[pixeloffset] != fg) {
					screena.update_pixels = 1;
					screena.screen[pixeloffset] = fg;
					screena.rect->data[pixeloffset] = color_value(fg);
				}
			}
			else {
				if (screena.screen[pixeloffset] != bg) {
					screena.update_pixels = 1;
					screena.screen[pixeloffset] = bg;
					screena.rect->data[pixeloffset] = color_value(bg);
				}
			}

			if(fb & (0x80 >> (fdx & 7))) {
				if (screenb.screen[pixeloffset] != fg) {
					screenb.update_pixels = 1;
					screenb.screen[pixeloffset] = fg;
					screenb.rect->data[pixeloffset] = color_value(fg);
				}
			}
			else {
				if (screenb.screen[pixeloffset] != bg) {
					screenb.update_pixels = 1;
					screenb.screen[pixeloffset]=bg;
					screenb.rect->data[pixeloffset] = color_value(bg);
				}
			}
			pixeloffset++;
		}
		if (x & 0x07)
			fontoffset++;
		pixeloffset += screena.screenwidth - vstat.charwidth;
	}
	pthread_mutex_unlock(&screenb.screenlock);
	pthread_mutex_unlock(&screena.screenlock);

	return(0);
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

	SetThreadName("Blinker");
	while(1) {
		curs_changed = 0;
		blink_changed = 0;
		do {
			SLEEP(10);
			both_screens(&screen, &ncscreen);
		} while (screen->screen == NULL);
		count++;
		if (count==25) {
			pthread_mutex_lock(&vstatlock);
			curs_changed = cursor_visible_locked();
			if(vstat.curs_blink)
				vstat.curs_blink=FALSE;
			else
				vstat.curs_blink=TRUE;
			curs_changed = (curs_changed != cursor_visible_locked());
			pthread_mutex_unlock(&vstatlock);
		}
		if(count==50) {
			pthread_mutex_lock(&vstatlock);
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
			pthread_mutex_unlock(&vstatlock);
		}
		/* Lock out ciolib while we handle shit */
		pthread_mutex_lock(&blinker_lock);
		if (check_redraw()) {
			if (update_from_vmem(TRUE))
				request_redraw();
		}
		else {
			if (count==0)
				if (update_from_vmem(FALSE))
					request_redraw();
		}
		pthread_mutex_lock(&screen->screenlock);
		// TODO: Maybe we can optimize the blink_changed forced update?
		if (screen->update_pixels || curs_changed || blink_changed) {
			// If the other screen is update_pixels == 2, clear it.
			pthread_mutex_lock(&ncscreen->screenlock);
			if (ncscreen->update_pixels == 2)
				ncscreen->update_pixels = 0;
			pthread_mutex_unlock(&ncscreen->screenlock);
			rect = get_full_rectangle_locked(screen);
			screen->update_pixels = 0;
			pthread_mutex_unlock(&screen->screenlock);
			cb_drawrect(rect);
		}
		else {
			if (force_cursor) {
				rect = get_full_rectangle_locked(screen);
			}
			pthread_mutex_unlock(&screen->screenlock);
			if (force_cursor) {
				cb_drawrect(rect);
				force_cursor = 0;
			}
		}
		cb_flush();
		pthread_mutex_unlock(&blinker_lock);
	}
}

static __inline struct bitmap_screen *noncurrent_screen_locked(void)
{
	if (vstat.blink)
		return &screenb;
	return &screena;
}

static __inline struct bitmap_screen *current_screen_locked(void)
{
	if (vstat.blink)
		return &screena;
	return(&screenb);
}

static __inline void both_screens(struct bitmap_screen** current, struct bitmap_screen** noncurrent)
{
	pthread_mutex_lock(&vstatlock);
	*current = current_screen_locked();
	*noncurrent = noncurrent_screen_locked();
	pthread_mutex_unlock(&vstatlock);
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

	width=cio_textinfo.screenwidth;
	height=cio_textinfo.screenheight;

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
	if(vs.cols!=vstat.cols || vs.rows != vstat.rows) {
		/* Force a full redraw */
		width=vstat.cols;
		height=vstat.rows;
		force=1;
	}

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

	for(y=0;y<height;y++) {
		pos=y*vstat.cols;
		for(x=0;x<width;x++) {
			/* Last this char been updated? */
			if(force										/* Forced */
			    || ((vstat.vmem->vmem[pos].legacy_attr & 0x80) && blink_attr_changed)
			    || ((vstat.vmem->vmem[pos].legacy_attr & 0x08) && bright_attr_changed))	/* Bright char */
			    {
				bitmap_draw_one_char(x+1,y+1);
			}
			pos++;
		}
	}
	release_vmem(vmem_ptr);

	pthread_mutex_unlock(&vstatlock);

	vs = vstat;

	return(0);
}

/*************************************/

/**********************/
/* Called from ciolib */
/**********************/
int bitmap_puttext(int sx, int sy, int ex, int ey, void *fill)
{
	int x, y;
	int ret = 1;
	uint16_t *buf = fill;
	struct vstat_vmem *vmem_ptr;

	pthread_mutex_lock(&blinker_lock);

	pthread_mutex_lock(&vstatlock);
	vmem_ptr = get_vmem(&vstat);
	for(y=sy-1;y<ey;y++) {
		for(x=sx-1;x<ex;x++) {
			set_vmem_cell(vmem_ptr, y*cio_textinfo.screenwidth+x, *(buf++), 0x00ffffff, 0x00ffffff);
			bitmap_draw_one_char(x+1, y+1);
		}
	}
	release_vmem(vmem_ptr);
	pthread_mutex_unlock(&vstatlock);
	pthread_mutex_unlock(&blinker_lock);
	return ret;
}

int bitmap_vmem_puttext(int sx, int sy, int ex, int ey, struct vmem_cell *fill)
{
	int ret;

	if(!bitmap_initialized)
		return(0);

	pthread_mutex_lock(&blinker_lock);
	ret = bitmap_vmem_puttext_locked(sx, sy, ex, ey, fill);
	pthread_mutex_unlock(&blinker_lock);

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

	pthread_mutex_lock(&blinker_lock);
	pthread_mutex_lock(&vstatlock);
	vmem_ptr = get_vmem(&vstat);
	for(y=sy-1;y<ey;y++) {
		for(x=sx-1;x<ex;x++)
			memcpy(fill++, &vmem_ptr->vmem[y*cio_textinfo.screenwidth+x], sizeof(*fill));
	}
	release_vmem(vmem_ptr);
	pthread_mutex_unlock(&vstatlock);
	pthread_mutex_unlock(&blinker_lock);
	return(1);
}

void bitmap_gotoxy(int x, int y)
{
	if(!bitmap_initialized)
		return;
	/* Move cursor location */
	pthread_mutex_lock(&blinker_lock);
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
	pthread_mutex_unlock(&blinker_lock);
}

void bitmap_setcursortype(int type)
{
	if(!bitmap_initialized)
		return;
	pthread_mutex_lock(&blinker_lock);
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
	pthread_mutex_unlock(&blinker_lock);
}

int bitmap_setfont(int font, int force, int font_num)
{
	int changemode=0;
	int	newmode=-1;
	struct text_info ti;
	struct vmem_cell	*old;
	int		ow,oh;
	int		row,col;
	struct vmem_cell	*new;
	int		attr;
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

	pthread_mutex_lock(&blinker_lock);
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
		gettextinfo(&ti);

		attr=ti.attribute;
		ow=ti.screenwidth;
		oh=ti.screenheight;

		old=malloc(ow*oh*sizeof(*old));
		if(old) {
			bitmap_vmem_gettext(1,1,ow,oh,old);
			textmode(newmode);
			new=malloc(ti.screenwidth*ti.screenheight*sizeof(*new));
			if(!new) {
				free(old);
				pthread_mutex_unlock(&vstatlock);
				pthread_mutex_unlock(&blinker_lock);
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
			bitmap_vmem_puttext(1,1,ti.screenwidth,ti.screenheight,pnew);
			free(pnew);
			free(pold);
		}
		else {
			FREE_AND_NULL(old);
		}
	}
	bitmap_loadfont_locked(NULL);
	pthread_mutex_unlock(&vstatlock);
	pthread_mutex_unlock(&blinker_lock);
	return(1);

error_return:
	pthread_mutex_unlock(&vstatlock);
	pthread_mutex_unlock(&blinker_lock);
	return(0);
}

int bitmap_getfont(int font_num)
{
	int ret;

	pthread_mutex_lock(&blinker_lock);
	if (font_num == 0)
		ret = default_font;
	else if (font_num > 4)
		ret = -1;
	else
		ret = current_font[font_num - 1];
	pthread_mutex_unlock(&blinker_lock);

	return ret;
}

int bitmap_loadfont(char *filename)
{
	int ret;

	pthread_mutex_lock(&blinker_lock);
	pthread_mutex_lock(&vstatlock);
	ret = bitmap_loadfont_locked(filename);
	pthread_mutex_unlock(&vstatlock);
	pthread_mutex_unlock(&blinker_lock);
	return ret;
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
	int32_t sdestoffset;
	size_t ssourcepos;
	int32_t screeny;

	if(		   x<1
			|| y<1
			|| ex<1
			|| ey<1
			|| tox<1
			|| toy<1
			|| x>cio_textinfo.screenwidth
			|| ex>cio_textinfo.screenwidth
			|| tox>cio_textinfo.screenwidth
			|| y>cio_textinfo.screenheight
			|| ey>cio_textinfo.screenheight
			|| toy>cio_textinfo.screenheight) {
		return(0);
	}

	if(toy > y)
		direction=-1;

	pthread_mutex_lock(&blinker_lock);
	if (direction == -1) {
		sourcepos=(y+height-2)*cio_textinfo.screenwidth+(x-1);
		destoffset=(((toy+height-2)*cio_textinfo.screenwidth+(tox-1))-sourcepos);
	}
	else {
		sourcepos=(y-1)*cio_textinfo.screenwidth+(x-1);
		destoffset=(((toy-1)*cio_textinfo.screenwidth+(tox-1))-sourcepos);
	}

	pthread_mutex_lock(&vstatlock);
	vmem_ptr = get_vmem(&vstat);
	for(cy=0; cy<height; cy++) {
		memmove(&(vmem_ptr->vmem[sourcepos+destoffset]), &(vmem_ptr->vmem[sourcepos]), sizeof(vmem_ptr->vmem[0])*width);
		sourcepos += direction * cio_textinfo.screenwidth;
	}

	if (direction == -1) {
		ssourcepos=((y + height - 1)     * vstat.charheight - 1) * vstat.scrnwidth + (x - 1)  * vstat.charwidth;
		sdestoffset=((((toy + height - 1) * vstat.charheight - 1) * vstat.scrnwidth + (tox - 1) * vstat.charwidth) - ssourcepos);
	}
	else {
		ssourcepos=(y - 1)     * vstat.scrnwidth * vstat.charheight + (x - 1)  * vstat.charwidth;
		sdestoffset=(((toy - 1) * vstat.scrnwidth * vstat.charheight + (tox - 1) * vstat.charwidth) - ssourcepos);
	}

	pthread_mutex_lock(&screena.screenlock);
	for(screeny=0; screeny < height*vstat.charheight; screeny++) {
		memmove(&(screena.screen[ssourcepos+sdestoffset]), &(screena.screen[ssourcepos]), sizeof(screena.screen[0])*width*vstat.charwidth);
		memmove(&(screena.rect->data[ssourcepos+sdestoffset]), &(screena.rect->data[ssourcepos]), sizeof(screena.screen[0])*width*vstat.charwidth);
		ssourcepos += direction * vstat.scrnwidth;
	}
	screena.update_pixels = 1;
	pthread_mutex_unlock(&screena.screenlock);

	if (direction == -1) {
		ssourcepos=((y+height-1)     *vstat.charheight-1)*vstat.scrnwidth + (x-1)  *vstat.charwidth;
		sdestoffset=((((toy+height-1)*vstat.charheight-1)*vstat.scrnwidth + (tox-1)*vstat.charwidth)-ssourcepos);
	}
	else {
		ssourcepos=(y-1)     *vstat.scrnwidth*vstat.charheight + (x-1)  *vstat.charwidth;
		sdestoffset=(((toy-1)*vstat.scrnwidth*vstat.charheight + (tox-1)*vstat.charwidth)-ssourcepos);
	}

	pthread_mutex_lock(&screenb.screenlock);
	for(screeny=0; screeny < height*vstat.charheight; screeny++) {
		memmove(&(screenb.screen[ssourcepos+sdestoffset]), &(screenb.screen[ssourcepos]), sizeof(screenb.screen[0])*width*vstat.charwidth);
		memmove(&(screenb.rect->data[ssourcepos+sdestoffset]), &(screenb.rect->data[ssourcepos]), sizeof(screenb.screen[0])*width*vstat.charwidth);
		ssourcepos += direction * vstat.scrnwidth;
	}
	screenb.update_pixels = 1;
	pthread_mutex_unlock(&screenb.screenlock);

	release_vmem(vmem_ptr);
	pthread_mutex_unlock(&vstatlock);
	pthread_mutex_unlock(&blinker_lock);

	return(1);
}

void bitmap_clreol(void)
{
	int pos,x;
	WORD fill=(cio_textinfo.attribute<<8)|' ';
	struct vstat_vmem *vmem_ptr;
	int row;

	pthread_mutex_lock(&blinker_lock);
	row = cio_textinfo.cury + cio_textinfo.wintop - 1;
	pos=(row - 1)*cio_textinfo.screenwidth;
	pthread_mutex_lock(&vstatlock);
	vmem_ptr = get_vmem(&vstat);
	for(x=cio_textinfo.curx+cio_textinfo.winleft-2; x<cio_textinfo.winright; x++) {
		set_vmem_cell(vmem_ptr, pos+x, fill, ciolib_fg, ciolib_bg);
		bitmap_draw_one_char(x+1, row);
	}
	release_vmem(vmem_ptr);
	pthread_mutex_unlock(&vstatlock);
	pthread_mutex_unlock(&blinker_lock);
}

void bitmap_clrscr(void)
{
	int x,y;
	WORD fill=(cio_textinfo.attribute<<8)|' ';
	struct vstat_vmem *vmem_ptr;

	pthread_mutex_lock(&blinker_lock);
	pthread_mutex_lock(&vstatlock);
	vmem_ptr = get_vmem(&vstat);
	for(y = cio_textinfo.wintop-1; y < cio_textinfo.winbottom && y < vstat.rows; y++) {
		for(x=cio_textinfo.winleft-1; x<cio_textinfo.winright && x < vstat.cols; x++) {
			set_vmem_cell(vmem_ptr, y*cio_textinfo.screenwidth+x, fill, ciolib_fg, ciolib_bg);
			bitmap_draw_one_char(x+1, y+1);
		}
	}
	release_vmem(vmem_ptr);
	pthread_mutex_unlock(&vstatlock);
	pthread_mutex_unlock(&blinker_lock);
}

void bitmap_getcustomcursor(int *s, int *e, int *r, int *b, int *v)
{
	pthread_mutex_lock(&blinker_lock);
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
	pthread_mutex_unlock(&blinker_lock);
}

void bitmap_setcustomcursor(int s, int e, int r, int b, int v)
{
	double ratio;

	pthread_mutex_lock(&blinker_lock);
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
	pthread_mutex_unlock(&blinker_lock);
}

int bitmap_getvideoflags(void)
{
	int flags=0;

	pthread_mutex_lock(&blinker_lock);
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
	pthread_mutex_unlock(&blinker_lock);
	return(flags);
}

void bitmap_setvideoflags(int flags)
{
	pthread_mutex_lock(&blinker_lock);
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
	pthread_mutex_unlock(&blinker_lock);
}

int bitmap_attr2palette(uint8_t attr, uint32_t *fgp, uint32_t *bgp)
{
	int ret;

	pthread_mutex_lock(&blinker_lock);
	pthread_mutex_lock(&vstatlock);
	ret = bitmap_attr2palette_locked(attr, fgp, bgp);
	pthread_mutex_unlock(&vstatlock);
	pthread_mutex_unlock(&blinker_lock);

	return ret;
}

int bitmap_setpixel(uint32_t x, uint32_t y, uint32_t colour)
{
	pthread_mutex_lock(&blinker_lock);

	pthread_mutex_lock(&screena.screenlock);
	if (x < screena.screenwidth && y < screena.screenheight) {
		if (screena.screen[PIXEL_OFFSET(screena, x, y)] != colour) {
			screena.screen[PIXEL_OFFSET(screena, x, y)]=colour;
			screena.update_pixels = 1;
			screena.rect->data[PIXEL_OFFSET(screena, x, y)]=color_value(colour);
		}
	}
	pthread_mutex_unlock(&screena.screenlock);

	pthread_mutex_lock(&screenb.screenlock);
	if (x < screenb.screenwidth && y < screenb.screenheight) {
		if (screenb.screen[PIXEL_OFFSET(screenb, x, y)] != colour) {
			screenb.screen[PIXEL_OFFSET(screenb, x, y)]=colour;
			screenb.update_pixels = 1;
			screenb.rect->data[PIXEL_OFFSET(screenb, x, y)]=color_value(colour);
		}
	}
	pthread_mutex_unlock(&screenb.screenlock);

	pthread_mutex_unlock(&blinker_lock);

	return 1;
}

int bitmap_setpixels(uint32_t sx, uint32_t sy, uint32_t ex, uint32_t ey, uint32_t x_off, uint32_t y_off, struct ciolib_pixels *pixels, void *mask)
{
	uint32_t x, y;
	uint32_t width,height;
	char *m = mask;
	int mask_bit;
	size_t mask_byte;
	size_t pos;

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

	pthread_mutex_lock(&blinker_lock);
	pthread_mutex_lock(&screena.screenlock);
	pthread_mutex_lock(&screenb.screenlock);
	if (ex > screena.screenwidth || ey > screena.screenheight) {
		pthread_mutex_unlock(&screenb.screenlock);
		pthread_mutex_unlock(&screena.screenlock);
		pthread_mutex_unlock(&blinker_lock);
		return 0;
	}

	for (y = sy; y <= ey; y++) {
		pos = pixels->width*(y-sy+y_off)+x_off;
		if (mask == NULL) {
			for (x = sx; x <= ex; x++) {
				screena.screen[PIXEL_OFFSET(screena, x, y)] = pixels->pixels[pos];
				if (pixels->pixelsb) {
					screenb.screen[PIXEL_OFFSET(screenb, x, y)] = pixels->pixelsb[pos];
					screenb.rect->data[PIXEL_OFFSET(screenb, x, y)] = color_value(pixels->pixelsb[pos]);
				}
				else {
					screenb.screen[PIXEL_OFFSET(screenb, x, y)] = pixels->pixels[pos];
					screenb.rect->data[PIXEL_OFFSET(screenb, x, y)] = color_value(pixels->pixels[pos]);
				}
				pos++;
			}
		}
		else {
			for (x = sx; x <= ex; x++) {
				mask_byte = pos / 8;
				mask_bit = pos % 8;
				mask_bit = 0x80 >> mask_bit;
				if (m[mask_byte] & mask_bit) {
					screena.screen[PIXEL_OFFSET(screena, x, y)] = pixels->pixels[pos];
					screena.rect->data[PIXEL_OFFSET(screena, x, y)] = color_value(pixels->pixels[pos]);
					if (pixels->pixelsb) {
						screenb.screen[PIXEL_OFFSET(screenb, x, y)] = pixels->pixelsb[pos];
						screenb.rect->data[PIXEL_OFFSET(screenb, x, y)] = color_value(pixels->pixelsb[pos]);
					}
					else {
						screenb.screen[PIXEL_OFFSET(screenb, x, y)] = pixels->pixels[pos];
						screenb.rect->data[PIXEL_OFFSET(screenb, x, y)] = color_value(pixels->pixels[pos]);
					}
				}
				pos++;
			}
		}
	}
	screena.update_pixels = 1;
	screenb.update_pixels = 1;
	pthread_mutex_unlock(&screenb.screenlock);
	pthread_mutex_unlock(&screena.screenlock);
	pthread_mutex_unlock(&blinker_lock);

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

	pthread_mutex_lock(&blinker_lock);
	update_from_vmem(force);
	pthread_mutex_lock(&vstatlock);
	pthread_mutex_lock(&screena.screenlock);
	pthread_mutex_lock(&screenb.screenlock);
	if (ex >= screena.screenwidth || ey >= screena.screenheight ||
	    ex >= screenb.screenwidth || ey >= screenb.screenheight) {
		pthread_mutex_unlock(&screenb.screenlock);
		pthread_mutex_unlock(&screena.screenlock);
		pthread_mutex_unlock(&vstatlock);
		pthread_mutex_unlock(&blinker_lock);
		free(pixels->pixelsb);
		free(pixels->pixels);
		free(pixels);
		return NULL;
	}

	for (y = sy; y <= ey; y++) {
		memcpy(&pixels->pixels[width*(y-sy)], &screena.screen[PIXEL_OFFSET(screena, sx, y)], width * sizeof(pixels->pixels[0]));
		memcpy(&pixels->pixelsb[width*(y-sy)], &screenb.screen[PIXEL_OFFSET(screenb, sx, y)], width * sizeof(pixels->pixelsb[0]));
	}
	pthread_mutex_unlock(&screenb.screenlock);
	pthread_mutex_unlock(&screena.screenlock);
	pthread_mutex_unlock(&vstatlock);
	pthread_mutex_unlock(&blinker_lock);

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
	pthread_mutex_lock(&blinker_lock);

	if (id < CONIO_FIRST_FREE_FONT) {
		free(name);
		free(data);
		pthread_mutex_unlock(&blinker_lock);
		return;
	}

	pthread_mutex_lock(&screena.screenlock);
	pthread_mutex_lock(&screenb.screenlock);
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
	screena.update_pixels = 1;
	screenb.update_pixels = 1;
	pthread_mutex_unlock(&screenb.screenlock);
	pthread_mutex_unlock(&screena.screenlock);
	pthread_mutex_unlock(&blinker_lock);
}

int bitmap_setpalette(uint32_t index, uint16_t r, uint16_t g, uint16_t b)
{
	if (index > 65535)
		return 0;

	pthread_mutex_lock(&blinker_lock);
	pthread_mutex_lock(&screena.screenlock);
	pthread_mutex_lock(&screenb.screenlock);
	palette[index] = (0xff << 24) | ((r>>8) << 16) | ((g>>8) << 8) | (b>>8);
	screena.update_pixels = 1;
	screenb.update_pixels = 1;
	pthread_mutex_unlock(&screenb.screenlock);
	pthread_mutex_unlock(&screena.screenlock);
	pthread_mutex_unlock(&blinker_lock);
	return 1;
}

// Called with vstatlock
static int init_screen(struct bitmap_screen *screen, int *width, int *height)
{
	uint32_t *newscreen;

	pthread_mutex_lock(&screen->screenlock);
	screen->screenwidth = vstat.scrnwidth;
	if (width)
		*width = screen->screenwidth;
	screen->screenheight = vstat.scrnheight;
	if (height)
		*height = screen->screenheight;
	newscreen = realloc(screen->screen, screen->screenwidth * screen->screenheight * sizeof(screen->screen[0]));

	if (!newscreen) {
		pthread_mutex_unlock(&screen->screenlock);
		return(-1);
	}
	screen->screen = newscreen;
	memset_u32(screen->screen, vstat.palette[0], screen->screenwidth * screen->screenheight);
	screen->update_pixels = 1;
	bitmap_drv_free_rect(screen->rect);
	screen->rect = alloc_full_rect(screen);
	if (screen->rect == NULL) {
		pthread_mutex_unlock(&screen->screenlock);
		return(-1);
	}
	memset_u32(screen->rect->data, color_value(vstat.palette[0]), screen->rect->rect.width * screen->rect->rect.height);
	pthread_mutex_unlock(&screen->screenlock);
	return(0);
}

/***********************/
/* Called from drivers */
/***********************/

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
int bitmap_drv_init_mode(int mode, int *width, int *height)
{
	int i;

	if(!bitmap_initialized)
		return(-1);

	if(load_vmode(&vstat, mode)) {
		// TODO: WTF?
		//pthread_mutex_unlock(&blinker_lock);
		return(-1);
	}

	/* Initialize video memory with black background, white foreground */
	for (i = 0; i < vstat.cols*vstat.rows; ++i) {
		vstat.vmem->vmem[i].ch = 0;
		vstat.vmem->vmem[i].legacy_attr = vstat.currattr;
		vstat.vmem->vmem[i].font = default_font;
		bitmap_attr2palette_locked(vstat.currattr, &vstat.vmem->vmem[i].fg, &vstat.vmem->vmem[i].bg);
	}

	if (init_screen(&screena, width, height))
		return -1;
	if (init_screen(&screenb, width, height))
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
			| CONIO_OPT_FONT_SELECT | CONIO_OPT_EXTENDED_PALETTE | CONIO_OPT_PALETTE_SETTING;
	pthread_mutex_init(&blinker_lock, NULL);
	pthread_mutex_init(&callbacks.lock, NULL);
	pthread_mutex_init(&vstatlock, NULL);
	pthread_mutex_init(&screena.screenlock, NULL);
	pthread_mutex_init(&screenb.screenlock, NULL);
	pthread_mutex_init(&free_rect_lock, NULL);
	pthread_mutex_lock(&vstatlock);
	init_r2y();
	vstat.vmem=NULL;
	vstat.flags = VIDMODES_FLAG_PALETTE_VMEM;
	pthread_mutex_lock(&screena.screenlock);
	pthread_mutex_lock(&screenb.screenlock);
	for (i = 0; i < sizeof(dac_default)/sizeof(struct dac_colors); i++) {
		palette[i] = (0xff << 24) | (dac_default[i].red << 16) | (dac_default[i].green << 8) | dac_default[i].blue;
	}
	pthread_mutex_unlock(&screenb.screenlock);
	pthread_mutex_unlock(&screena.screenlock);
	pthread_mutex_unlock(&vstatlock);

	callbacks.drawrect=drawrect_cb;
	callbacks.flush=flush_cb;
	callbacks.rects = 0;
	bitmap_initialized=1;
	_beginthread(blinker_thread,0,NULL);

	return(0);
}

void bitmap_drv_request_pixels(void)
{
	pthread_mutex_lock(&screena.screenlock);
	if (screena.update_pixels == 0)
		screena.update_pixels = 2;
	pthread_mutex_unlock(&screena.screenlock);
	pthread_mutex_lock(&screenb.screenlock);
	if (screenb.update_pixels == 0)
		screenb.update_pixels = 2;
	pthread_mutex_unlock(&screenb.screenlock);
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
	rect->next = free_rects;
	free_rects = rect;
	pthread_mutex_unlock(&free_rect_lock);
}
