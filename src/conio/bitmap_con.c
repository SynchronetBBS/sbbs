/* $Id$ */

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

#if (defined CIOLIB_IMPORTS)
 #undef CIOLIB_IMPORTS
#endif
#if (defined CIOLIB_EXPORTS)
 #undef CIOLIB_EXPORTS
#endif

#include "ciolib.h"
#include "vidmodes.h"
#include "bitmap_con.h"

struct palette_entry {
	uint8_t	red;
	uint8_t	green;
	uint8_t	blue;
};

static struct palette_entry palette[65536];

#if 0

int dbg_pthread_mutex_lock(pthread_mutex_t *lptr, unsigned line)
{
	int ret = pthread_mutex_lock(lptr);

	if (ret)
		fprintf(stderr, "pthread_mutex_lock() returned %d at %u\n", ret, line);
	return ret;
}

int dbg_pthread_mutex_unlock(pthread_mutex_t *lptr, unsigned line)
{
	int ret = pthread_mutex_unlock(lptr);

	if (ret)
		fprintf(stderr, "pthread_mutex_lock() returned %d at %u\n", ret, line);
	return ret;
}

int dbg_pthread_mutex_trylock(pthread_mutex_t *lptr, unsigned line)
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
static struct bitmap_screen screen;
struct video_stats vstat;
static struct bitmap_callbacks callbacks;
static unsigned char *font[4];
static unsigned char space=' ';
static int force_redraws=0;
static int update_pixels = 0;
static pthread_mutex_t blinker_lock;
struct rectlist *free_rects;

/* The read lock must be held here. */
#define PIXEL_OFFSET(screen, x, y)	( (y)*screen.screenwidth+(x) )

/* Exported globals */

pthread_mutex_t		vstatlock;

/* Forward declarations */

static int bitmap_loadfont_locked(char *filename);
static void set_vmem_cell(struct vstat_vmem *vmem_ptr, size_t pos, uint16_t cell, uint32_t fg, uint32_t bg);
static int bitmap_attr2palette_locked(uint8_t attr, uint32_t *fgp, uint32_t *bgp);
static void	cb_drawrect(struct rectlist *data);
static void request_redraw_locked(void);
static void request_redraw(void);
static void memset_u32(void *buf, uint32_t u, size_t len);
static int bitmap_draw_one_char(unsigned int xpos, unsigned int ypos);
static int bitmap_draw_one_char_cursor(unsigned int xpos, unsigned int ypos);
static void cb_flush(void);
static int check_redraw(void);
static void blinker_thread(void *data);
static __inline void *locked_screen_check(void);
static BOOL bitmap_draw_cursor(void);
static int update_from_vmem(int force);
static int bitmap_vmem_puttext_locked(int sx, int sy, int ex, int ey, struct vmem_cell *fill);
static uint32_t color_value(uint32_t col);

/**************************************************************/
/* These functions get called from the driver and ciolib only */
/**************************************************************/

static int bitmap_loadfont_locked(char *filename)
{
	static char current_filename[MAX_PATH];
	unsigned int fontsize;
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
	fw=vstat.charwidth/8+(vstat.charwidth%8?1:0);

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
		switch(vstat.charwidth) {
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
			bitmap_draw_one_char_cursor(x+1, y+1);
		}
	}
	release_vmem(vmem_ptr);
	pthread_mutex_unlock(&vstatlock);
	return(1);
}

static void set_vmem_cell(struct vstat_vmem *vmem_ptr, size_t pos, uint16_t cell, uint32_t fg, uint32_t bg)
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

/*
 * So, here's the deal with the cursor...
 * 
 * 1) It's located at vstat.curs_col/vstat.curs_row.
 * 2) The size is defined by vstat.curs_start and vstat.curs_end...
 *    the are both rows from the top of the cell.
 *    If vstat.curs_start > vstat.curs_end, the cursor is not shown.
 * 3) If vstat.curs_visible is false, the cursor is not shown.
 * 4) If vstat.curs_blinks is false, the cursor does not blink.
 * 5) When blinking, the cursor is shown when vstat.blink is true.
 * 6) The *ONLY* thing that should be changing vstat.curs_col or
 *    vstat.curs_row is bitmap_gotoxy().
 * 7) bitmap_gotoxy() only moves the cursor if hold_update is true.
 * 8) bitmap_draw_cursor() is the only thing that draws the cursor.
 * 9) To erase a cursor, you redraw the cell it's in using
 *    bitmap_draw_one_char().
 * 10) The blinker thread via update_from_rect() will redraw the cursor.
 */
static BOOL bitmap_draw_cursor(void)
{
	int x,y;
	int pixel;
	int xoffset,yoffset;
	BOOL ret = FALSE;

	if(!bitmap_initialized)
		return ret;
	if(vstat.curs_visible) {
		if(vstat.blink || (!vstat.curs_blinks)) {
			if(vstat.curs_start<=vstat.curs_end) {
				xoffset=(vstat.curs_col-1)*vstat.charwidth;
				yoffset=(vstat.curs_row-1)*vstat.charheight;
				if(xoffset < 0 || yoffset < 0)
					return ret;

				pthread_mutex_lock(&screen.screenlock);
				for(y=vstat.curs_start; y<=vstat.curs_end; y++) {
					if(xoffset < screen.screenwidth && (yoffset+y) < screen.screenheight) {
						pixel=PIXEL_OFFSET(screen, xoffset, yoffset+y);
						for (x = 0; x < vstat.charwidth; x++) {
							if (screen.screen[pixel] != ciolib_fg) {
								ret = TRUE;
								screen.screen[pixel] = ciolib_fg;
							}
							pixel++;
						}
					}
				}
				if (ret)
					update_pixels = 1;
				pthread_mutex_unlock(&screen.screenlock);
			}
		}
	}
	return ret;
}

static int bitmap_draw_one_char_cursor(unsigned int xpos, unsigned int ypos)
{
	if (bitmap_draw_one_char(xpos, ypos) == -1)
		return -1;
	if (xpos == vstat.curs_col && ypos == vstat.curs_row)
		return bitmap_draw_cursor();
	return 0;
}

static void	cb_drawrect(struct rectlist *data)
{
	if (data == NULL)
		return;
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
static struct rectlist *alloc_full_rect(void)
{
	struct rectlist * ret;

	while (free_rects) {
		if (free_rects->rect.width == screen.screenwidth && free_rects->rect.height == screen.screenheight) {
			ret = free_rects;
			ret->rect.x = ret->rect.y = 0;
			free_rects = free_rects->next;
			return ret;
		}
		else {
			free(free_rects->data);
			ret = free_rects->next;
			free(free_rects);
			free_rects = ret;
		}
	}

	ret = malloc(sizeof(struct rectlist));
	ret->rect.x = 0;
	ret->rect.y = 0;
	ret->rect.width = screen.screenwidth;
	ret->rect.height = screen.screenheight;
	ret->data = malloc(ret->rect.width * ret->rect.height * sizeof(ret->data[0]));
	if (ret->data == NULL)
		FREE_AND_NULL(ret);
	return ret;
}

static uint32_t color_value(uint32_t col)
{
	if (col & 0x80000000)
		return col;
	return (0xff << 24) | (palette[col].red << 16) | (palette[col].green << 8) | palette[col].blue;
}

static struct rectlist *get_full_rectangle_locked(void)
{
	size_t i;
	struct rectlist *rect = alloc_full_rect();

	if(callbacks.drawrect) {
		if (!rect)
			return rect;
		for (i=0; i<screen.screenwidth*screen.screenheight; i++)
			rect->data[i] = color_value(screen.screen[i]);
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
	int		xoffset=(xpos-1)*vstat.charwidth;
	int		yoffset=(ypos-1)*vstat.charheight;
	int		x;
	int		y;
	int		fontoffset;
	unsigned char *this_font;
	WORD	sch;
	struct vstat_vmem *vmem_ptr;
	BOOL	changed = FALSE;

	if(!bitmap_initialized) {
		return(-1);
	}

	vmem_ptr = vstat.vmem;

	if(!vmem_ptr) {
		return(-1);
	}

	pthread_mutex_lock(&screen.screenlock);

	if ((xoffset + vstat.charwidth > screen.screenwidth) || yoffset + vstat.charheight > screen.screenheight) {
		pthread_mutex_unlock(&screen.screenlock);
		return(-1);
	}

	if(!screen.screen) {
		pthread_mutex_unlock(&screen.screenlock);
		return(-1);
	}

	sch=vmem_ptr->vmem[(ypos-1)*cio_textinfo.screenwidth+(xpos-1)].legacy_attr << 8 | vmem_ptr->vmem[(ypos-1)*cio_textinfo.screenwidth+(xpos-1)].ch;
	fg = vmem_ptr->vmem[(ypos-1)*cio_textinfo.screenwidth+(xpos-1)].fg;
	bg = vmem_ptr->vmem[(ypos-1)*cio_textinfo.screenwidth+(xpos-1)].bg;

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
	}
	if (this_font == NULL)
		this_font = font[0];
	fontoffset=(sch&0xff)*vstat.charheight;

	for(y=0; y<vstat.charheight; y++) {
		if ((!((sch & 0x8000) && !vstat.blink)) || vstat.no_blink) {
			for(x=0; x<vstat.charwidth; x++) {
				if(this_font[fontoffset] & (0x80 >> x)) {
					if (screen.screen[PIXEL_OFFSET(screen, xoffset+x, yoffset+y)]!=fg) {
						changed=TRUE;
						screen.screen[PIXEL_OFFSET(screen, xoffset+x, yoffset+y)]=fg;
					}
				}
				else
					if (screen.screen[PIXEL_OFFSET(screen, xoffset+x, yoffset+y)]!=bg) {
						changed=TRUE;
						screen.screen[PIXEL_OFFSET(screen, xoffset+x, yoffset+y)]=bg;
					}
			}
		}
		fontoffset++;
	}
	if (changed)
		update_pixels = 1;
	pthread_mutex_unlock(&screen.screenlock);

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

	SetThreadName("Blinker");
	while(1) {
		do {
			SLEEP(10);
		} while(locked_screen_check()==NULL);
		count++;
		if(count==50) {
			pthread_mutex_lock(&vstatlock);
			if(vstat.blink)
				vstat.blink=FALSE;
			else
				vstat.blink=TRUE;
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
		pthread_mutex_lock(&screen.screenlock);
		if (update_pixels) {
			rect = get_full_rectangle_locked();
			update_pixels = 0;
			pthread_mutex_unlock(&screen.screenlock);
			cb_drawrect(rect);
		}
		else
			pthread_mutex_unlock(&screen.screenlock);
		cb_flush();
		pthread_mutex_unlock(&blinker_lock);
	}
}

static __inline void *locked_screen_check(void)
{
	void *ret;
	pthread_mutex_lock(&screen.screenlock);
	ret=screen.screen;
	pthread_mutex_unlock(&screen.screenlock);
	return(ret);
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

	int	redraw_cursor=0;
	int	lastcharupdated=0;
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

	/* Redraw cursor? */
	if(vstat.curs_visible							// Visible
			&& vstat.curs_start <= vstat.curs_end	// Should be drawn
			&& vstat.curs_blinks						// Is blinking
			&& vstat.blink != vs.blink)				// Blink has changed
		redraw_cursor=1;

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
			if(force																/* Forced */
					|| ((vstat.vmem->vmem[pos].legacy_attr & 0x80) && (blink_attr_changed ||
							((vstat.blink != vs.blink) && (!vstat.no_blink)))) 	/* Blinking char */
					|| ((vstat.vmem->vmem[pos].legacy_attr & 0x08) && bright_attr_changed)	/* Bright char */
					|| (redraw_cursor && (vstat.curs_col==x+1 && vstat.curs_row==y+1))	/* Cursor */
					) {
				bitmap_draw_one_char_cursor(x+1,y+1);
			}
			pos++;
		}
		lastcharupdated=0;
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
			bitmap_draw_one_char_cursor(x+1, y+1);
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
	cio_textinfo.curx=x;
	cio_textinfo.cury=y;
	/* Draw new cursor */
	if(!hold_update) {
		/* Erase the current cursor */
		if (vstat.curs_col != x+cio_textinfo.winleft-1 || vstat.curs_row != y+cio_textinfo.wintop-1) {
			if (vstat.curs_visible && vstat.curs_start <= vstat.curs_end)
				bitmap_draw_one_char(vstat.curs_col, vstat.curs_row);
		}
		pthread_mutex_lock(&vstatlock);
		if (vstat.curs_col != x+cio_textinfo.winleft-1 || vstat.curs_row != y+cio_textinfo.wintop-1) {
			vstat.curs_col=x+cio_textinfo.winleft-1;
			vstat.curs_row=y+cio_textinfo.wintop-1;
			bitmap_draw_cursor();
		}
		pthread_mutex_unlock(&vstatlock);
	}
	pthread_mutex_unlock(&blinker_lock);
}

void bitmap_setcursortype(int type)
{
	int oldstart = vstat.curs_start;
	int oldend = vstat.curs_end;

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
			break;
		default:
		    vstat.curs_start = vstat.default_curs_start;
		    vstat.curs_end = vstat.default_curs_end;
			break;
	}
	if (vstat.curs_visible) {
		if (oldstart != vstat.curs_start || oldend != vstat.curs_end) {
			/* Erase the current cursor */
			if (oldstart <= oldend) /* Only if it's drawn */
				bitmap_draw_one_char(vstat.curs_col, vstat.curs_row);
			/* Draw new cursor */
			bitmap_draw_cursor();
		}
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
			if(font==36 /* ATARI */)
				space=0;
			else
				space=' ';
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
							new->ch=space;
							new->legacy_attr=attr;
							new->font = font;
							new->fg = ciolib_fg;
							new->bg = ciolib_bg;
							new++;
						}
					}
					else {
							new->ch=space;
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
	BOOL redraw_cursor = FALSE;

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
		sourcepos=(y+height-2)*cio_textinfo.screenwidth+(x+width-2);
		destoffset=(((toy+height-2)*cio_textinfo.screenwidth+(tox+width-2))-sourcepos);
	}
	else {
		sourcepos=(y-1)*cio_textinfo.screenwidth+(x-1);
		destoffset=(((toy-1)*cio_textinfo.screenwidth+(tox-1))-sourcepos);
	}

	pthread_mutex_lock(&vstatlock);
	vmem_ptr = get_vmem(&vstat);
	/* Erase the current cursor... maybe... */
	if (vstat.curs_row >= y && vstat.curs_row <= ey
			&& vstat.curs_col >= x && vstat.curs_col <= ex					/* Is it in the box?  WHAT'S IN THE BOX? */
			&& vstat.curs_visible && vstat.curs_start <= vstat.curs_end) {	/* Also, is it on the screen? */
		bitmap_draw_one_char(vstat.curs_col, vstat.curs_row);
		redraw_cursor = 1;
	}
	for(cy=0; cy<height; cy++) {
		memmove(&(vmem_ptr->vmem[sourcepos+destoffset]), &(vmem_ptr->vmem[sourcepos]), sizeof(vmem_ptr->vmem[0])*width);
		sourcepos += direction * cio_textinfo.screenwidth;
	}

	if (direction == -1) {
		ssourcepos=((y+height-1)     *vstat.charheight-1)*cio_textinfo.screenwidth*vstat.charwidth + (x-1)  *vstat.charwidth;
		sdestoffset=((((toy+height-1)*vstat.charheight-1)*cio_textinfo.screenwidth*vstat.charwidth + (tox-1)*vstat.charwidth)-ssourcepos);
	}
	else {
		ssourcepos=(y-1)     *cio_textinfo.screenwidth*vstat.charwidth*vstat.charheight + (x-1)  *vstat.charwidth;
		sdestoffset=(((toy-1)*cio_textinfo.screenwidth*vstat.charwidth*vstat.charheight + (tox-1)*vstat.charwidth)-ssourcepos);
	}
	pthread_mutex_lock(&screen.screenlock);
	for(screeny=0; screeny < height*vstat.charheight; screeny++) {
		memmove(&(screen.screen[ssourcepos+sdestoffset]), &(screen.screen[ssourcepos]), sizeof(screen.screen[0])*width*vstat.charwidth);
		ssourcepos += direction * cio_textinfo.screenwidth*vstat.charwidth;
	}
	update_pixels = 1;
	pthread_mutex_unlock(&screen.screenlock);
	if (redraw_cursor)
		bitmap_draw_cursor();

	release_vmem(vmem_ptr);
	pthread_mutex_unlock(&vstatlock);
	pthread_mutex_unlock(&blinker_lock);

	return(1);
}

void bitmap_clreol(void)
{
	int pos,x;
	WORD fill=(cio_textinfo.attribute<<8)|space;
	struct vstat_vmem *vmem_ptr;
	int row;

	pthread_mutex_lock(&blinker_lock);
	row = cio_textinfo.cury + cio_textinfo.wintop - 1;
	pos=(row - 1)*cio_textinfo.screenwidth;
	pthread_mutex_lock(&vstatlock);
	vmem_ptr = get_vmem(&vstat);
	for(x=cio_textinfo.curx+cio_textinfo.winleft-2; x<cio_textinfo.winright; x++) {
		set_vmem_cell(vmem_ptr, pos+x, fill, ciolib_fg, ciolib_bg);
		bitmap_draw_one_char_cursor(x+1, row);
	}
	release_vmem(vmem_ptr);
	pthread_mutex_unlock(&vstatlock);
	pthread_mutex_unlock(&blinker_lock);
}

void bitmap_clrscr(void)
{
	int x,y;
	WORD fill=(cio_textinfo.attribute<<8)|space;
	struct vstat_vmem *vmem_ptr;

	pthread_mutex_lock(&blinker_lock);
	pthread_mutex_lock(&vstatlock);
	vmem_ptr = get_vmem(&vstat);
	for(y=cio_textinfo.wintop-1; y<cio_textinfo.winbottom; y++) {
		for(x=cio_textinfo.winleft-1; x<cio_textinfo.winright; x++) {
			set_vmem_cell(vmem_ptr, y*cio_textinfo.screenwidth+x, fill, ciolib_fg, ciolib_bg);
			bitmap_draw_one_char_cursor(x+1, y+1);
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
	int oldstart = vstat.curs_start;
	int oldend = vstat.curs_end;
	int oldblink = vstat.curs_blinks;
	int oldvisible = vstat.curs_visible;

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
	/* Did anything actually change? */
	if (oldstart != vstat.curs_start
			|| oldend != vstat.curs_end
			|| oldblink != vstat.curs_blinks
			|| oldvisible != vstat.curs_visible) {
		/* Erase the current cursor */
		if (oldvisible && oldstart <= oldend)
			bitmap_draw_one_char(vstat.curs_col, vstat.curs_row);
		/* Draw new cursor */
		bitmap_draw_cursor();
	}
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
	pthread_mutex_lock(&screen.screenlock);

	if (x < screen.screenwidth && y < screen.screenheight)
		screen.screen[PIXEL_OFFSET(screen, x, y)]=colour;
	update_pixels = 1;

	pthread_mutex_unlock(&screen.screenlock);
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
	pthread_mutex_lock(&screen.screenlock);
	if (ex > screen.screenwidth || ey > screen.screenheight) {
		pthread_mutex_unlock(&screen.screenlock);
		pthread_mutex_unlock(&blinker_lock);
		return 0;
	}

	for (y = sy; y <= ey; y++) {
		pos = pixels->width*(y-sy+y_off)+x_off;
		if (mask == NULL) {
			for (x = sx; x <= ex; x++) {
				screen.screen[PIXEL_OFFSET(screen, x, y)] = pixels->pixels[pos];
				pos++;
			}
		}
		else {
			for (x = sx; x <= ex; x++) {
				mask_byte = pos / 8;
				mask_bit = pos % 8;
				mask_bit = 0x80 >> mask_bit;
				if (m[mask_byte] & mask_bit)
					screen.screen[PIXEL_OFFSET(screen, x, y)] = pixels->pixels[pos];
				pos++;
			}
		}
	}

	update_pixels = 1;
	pthread_mutex_unlock(&screen.screenlock);
	pthread_mutex_unlock(&blinker_lock);

	return 1;
}

struct ciolib_pixels *bitmap_getpixels(uint32_t sx, uint32_t sy, uint32_t ex, uint32_t ey)
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

	pthread_mutex_lock(&blinker_lock);
	pthread_mutex_lock(&screen.screenlock);
	if (ex >= screen.screenwidth || ey >= screen.screenheight) {
		pthread_mutex_unlock(&screen.screenlock);
		pthread_mutex_unlock(&blinker_lock);
		free(pixels);
		return NULL;
	}

	for (y = sy; y <= ey; y++)
		memcpy(&pixels->pixels[width*(y-sy)], &screen.screen[PIXEL_OFFSET(screen, sx, y)], width * sizeof(pixels->pixels[0]));
	pthread_mutex_unlock(&screen.screenlock);
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
		return;
	}

	pthread_mutex_lock(&screen.screenlock);
	switch (size) {
		case 4096:
			FREE_AND_NULL(conio_fontdata[id].eight_by_sixteen);
			conio_fontdata[id].eight_by_sixteen=data;
			FREE_AND_NULL(conio_fontdata[id].desc);
			conio_fontdata[id].desc=name;
			update_pixels = 1;
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
	update_pixels = 1;
	pthread_mutex_unlock(&screen.screenlock);
	pthread_mutex_unlock(&blinker_lock);
}

int bitmap_setpalette(uint32_t index, uint16_t r, uint16_t g, uint16_t b)
{
	if (index > 65535)
		return 0;

	pthread_mutex_lock(&blinker_lock);
	pthread_mutex_lock(&screen.screenlock);
	palette[index].red = r>>8;
	palette[index].green = g>>8;
	palette[index].blue = b>>8;
	update_pixels = 1;
	pthread_mutex_unlock(&screen.screenlock);
	pthread_mutex_unlock(&blinker_lock);
	return 1;
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
	uint32_t *newscreen;

	if(!bitmap_initialized)
		return(-1);

	if(load_vmode(&vstat, mode))
		return(-1);

	/* Initialize video memory with black background, white foreground */
	for (i = 0; i < vstat.cols*vstat.rows; ++i) {
		vstat.vmem->vmem[i].ch = 0;
		vstat.vmem->vmem[i].legacy_attr = 7;
		bitmap_attr2palette_locked(7, &vstat.vmem->vmem[i].fg, &vstat.vmem->vmem[i].bg);
	}

	pthread_mutex_lock(&screen.screenlock);
	screen.screenwidth=vstat.charwidth*vstat.cols;
	if(width)
		*width=screen.screenwidth;
	screen.screenheight=vstat.charheight*vstat.rows;
	if(height)
		*height=screen.screenheight;
	newscreen=realloc(screen.screen, screen.screenwidth*screen.screenheight*sizeof(screen.screen[0]));

	if(!newscreen) {
		pthread_mutex_unlock(&screen.screenlock);
		return(-1);
	}
	screen.screen=newscreen;
	memset_u32(screen.screen,vstat.palette[0],screen.screenwidth*screen.screenheight);
	update_pixels = 1;
	pthread_mutex_unlock(&screen.screenlock);
	for (i=0; i<sizeof(current_font)/sizeof(current_font[0]); i++)
		current_font[i]=default_font;
	bitmap_loadfont_locked(NULL);

	cio_textinfo.attribute=7;
	cio_textinfo.normattr=7;
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
	pthread_mutex_init(&screen.screenlock, NULL);
	pthread_mutex_lock(&vstatlock);
	vstat.vmem=NULL;
	vstat.flags = VIDMODES_FLAG_PALETTE_VMEM;
	for (i = 0; i < sizeof(dac_default)/sizeof(struct dac_colors); i++) {
		palette[i].red = dac_default[i].red;
		palette[i].green = dac_default[i].green;
		palette[i].blue = dac_default[i].blue;
	}
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
	pthread_mutex_lock(&screen.screenlock);
	update_pixels = 1;
	pthread_mutex_unlock(&screen.screenlock);
}

void bitmap_drv_request_some_pixels(int x, int y, int width, int height)
{
	/* TODO: Some sort of queue here? */
	bitmap_drv_request_pixels();
}

void bitmap_drv_free_rect(struct rectlist *rect)
{
	pthread_mutex_lock(&screen.screenlock);
	rect->next = free_rects;
	free_rects = rect;
	pthread_mutex_unlock(&screen.screenlock);
}
