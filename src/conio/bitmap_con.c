/* $Id$ */
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

static struct bitmap_screen {
	uint32_t *screen;
	int		screenwidth;
	int		screenheight;
	pthread_mutex_t	screenlock;
} screen;
/* The read lock must be held here. */
#define PIXEL_OFFSET(screen, x, y)	( (y)*screen.screenwidth+(x) )

static int default_font=-99;
static int current_font[4]={-99, -99, -99, -99};
static int bitmap_initialized=0;

struct video_stats vstat;

struct bitmap_callbacks {
	void	(*drawrect)		(int xpos, int ypos, int width, int height, uint32_t *data);
	void	(*flush)		(void);
	pthread_mutex_t lock;
	unsigned rects;
};

pthread_mutex_t		vstatlock;
static struct bitmap_callbacks callbacks;
static unsigned char *font[4];
static unsigned char space=' ';
static int force_redraws=0;
static int update_pixels = 0;

struct rectangle {
	int x;
	int y;
	int width;
	int height;
};

static int update_rect(int sx, int sy, int width, int height, int force);
static void bitmap_draw_cursor(struct video_stats *vs);
static int bitmap_draw_one_char(struct video_stats *vs, unsigned int xpos, unsigned int ypos);
static int bitmap_loadfont_locked(char *filename);
static void *get_rectangle_locked(struct video_stats *vs, int xoffset, int yoffset, int width, int height, int force);

static void memset_u32(void *buf, uint32_t u, size_t len)
{
	size_t i;
	char *cbuf = buf;

	for (i = 0; i < len; i++) {
		memcpy(cbuf, &u, sizeof(uint32_t));
		cbuf += sizeof(uint32_t);
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

static int check_redraw(void)
{
	int ret;

	pthread_mutex_lock(&vstatlock);
	ret = force_redraws;
	force_redraws = 0;
	pthread_mutex_unlock(&vstatlock);
	return ret;
}

static void request_pixels_locked(void)
{
	update_pixels = 1;
}

static int check_pixels(void)
{
	int ret;

	pthread_mutex_lock(&screen.screenlock);
	ret = update_pixels;
	update_pixels = 0;
	pthread_mutex_unlock(&screen.screenlock);
	return ret;
}

static void cb_flush(void)
{
	pthread_mutex_lock(&callbacks.lock);
	if (callbacks.rects) {
		callbacks.flush();
		callbacks.rects = 0;
	}
	pthread_mutex_unlock(&callbacks.lock);
}

static void	cb_drawrect(int xpos, int ypos, int width, int height, uint32_t *data)
{
	if (data == NULL)
		return;
	pthread_mutex_lock(&callbacks.lock);
	callbacks.drawrect(xpos, ypos, width, height, data);
	callbacks.rects++;
	pthread_mutex_unlock(&callbacks.lock);
}

/* Blinker Thread */
static void blinker_thread(void *data)
{
	void *rect;
	int count=0;
	int ccol = 0;
	int crow = 0;
	int update = 0;

	SetThreadName("Blinker");
	while(1) {
		do {
			SLEEP(10);
		} while(locked_screen_check()==NULL);
		count++;
		if(count==50) {
			update = 1;
			pthread_mutex_lock(&vstatlock);
			if(vstat.blink)
				vstat.blink=FALSE;
			else
				vstat.blink=TRUE;
			count=0;
			pthread_mutex_unlock(&vstatlock);
		}
		/* Lock out both ends while we handle shit */
		if (check_redraw()) {
			if (update_rect(0,0,0,0,TRUE))
				request_redraw();
		}
		else {
			if (ccol != vstat.curs_col || crow != vstat.curs_row) {
				update = 1;
				ccol = vstat.curs_col;
				crow = vstat.curs_row;
			}
			if (update)
				update_rect(0,0,0,0,FALSE);
			update = 0;
		}
		if (check_pixels()) {
			pthread_mutex_lock(&screen.screenlock);
			rect = get_rectangle_locked(&vstat, 0, 0, screen.screenwidth, screen.screenheight, FALSE);
			pthread_mutex_unlock(&screen.screenlock);
			cb_drawrect(0, 0, screen.screenwidth, screen.screenheight, rect);
		}
		cb_flush();
	}
}

/*
 * Send by ciolib side, should not block in driver
 * Generally, if the driver may block on a rectangle draw, the updates
 * should be cached until flush is called.
 */

static void *get_rectangle_locked(struct video_stats *vs, int xoffset, int yoffset, int width, int height, int force)
{
	uint32_t *rect;
	int pixel=0;
	int inpixel;
	int x,y;

	if(callbacks.drawrect) {
		if(xoffset < 0 || xoffset >= screen.screenwidth || yoffset < 0 || yoffset >= screen.screenheight || width <= 0 || width > screen.screenwidth || height <=0 || height >screen.screenheight)
			return NULL;

		rect=(uint32_t *)malloc(width*height*sizeof(rect[0]));
		if(!rect)
			return NULL;

		for(y=0; y<height; y++) {
			inpixel=PIXEL_OFFSET(screen, xoffset, yoffset+y);
			for(x=0; x<width; x++)
				rect[pixel++]=screen.screen[inpixel++];
		}
		return rect;
	}
	return NULL;
}

static void send_rectangle(struct video_stats *vs, int xoffset, int yoffset, int width, int height, int force)
{
	void *rect;

	if(!bitmap_initialized)
		return;
	pthread_mutex_lock(&screen.screenlock);
	rect = get_rectangle_locked(vs,xoffset,yoffset,width,height,force);
	pthread_mutex_unlock(&screen.screenlock);
	cb_drawrect(xoffset, yoffset, width, height, rect);
	return;
}

static void send_text_rectangle(int xoffset, int yoffset, int width, int height, int force)
{
	if(!bitmap_initialized)
		return;
	pthread_mutex_lock(&vstatlock);
	xoffset *= vstat.charwidth;
	width *= vstat.charwidth;
	yoffset *= vstat.charheight;
	height *= vstat.charheight;
	pthread_mutex_unlock(&vstatlock);
	send_rectangle(&vstat,xoffset,yoffset,width,height,force);
}

/********************************************************/
/* High Level Stuff										*/
/********************************************************/

/* Called from main thread only (Passes Event) */

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

	return 0;
}

static void set_vmem_cell(struct vstat_vmem *vmem_ptr, size_t pos, uint16_t cell)
{
	uint32_t fg;
	uint32_t bg;

	bitmap_attr2palette_locked(cell>>8, &fg, &bg);

	vmem_ptr->vmem[pos] = cell;
	vmem_ptr->fgvmem[pos] = fg;
	vmem_ptr->bgvmem[pos] = bg;
}

/* Called from event thread only */
static int bitmap_loadfont_locked(char *filename)
{
	static char current_filename[MAX_PATH];
	unsigned int fontsize;
	int fw;
	int fh;
	int i;
	FILE	*fontfile=NULL;

	if(!bitmap_initialized)
		return(-1);
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
		return(-1);

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

	request_redraw_locked();
    return(0);

error_return:
	for (i=0; i<sizeof(font)/sizeof(font[0]); i++)
		FREE_AND_NULL(font[i]);
	if(fontfile)
		fclose(fontfile);
	return(-1);
}

static void bitmap_draw_cursor(struct video_stats *vs)
{
	int y;
	int pixel;
	int xoffset,yoffset;
	int yo, xw, yw;
	void *rect;

	if(!bitmap_initialized)
		return;
	if(vs->curs_visible) {
		if(vs->blink || (!vs->curs_blink)) {
			if(vs->curs_start<=vs->curs_end) {
				xoffset=(vs->curs_col-1)*vs->charwidth;
				yoffset=(vs->curs_row-1)*vs->charheight;
				if(xoffset < 0 || yoffset < 0)
					return;

				pthread_mutex_lock(&screen.screenlock);
				for(y=vs->curs_start; y<=vs->curs_end; y++) {
					if(xoffset < screen.screenwidth && (yoffset+y) < screen.screenheight) {
						pixel=PIXEL_OFFSET(screen, xoffset, yoffset+y);
						memset_u32(screen.screen+pixel,ciolib_fg,vs->charwidth);
					}
				}
				yo = yoffset+vs->curs_start;
				xw = vs->charwidth;
				yw = vs->curs_end-vs->curs_start+1;
				rect = get_rectangle_locked(vs, xoffset, yo, xw, yw,FALSE);

				pthread_mutex_unlock(&screen.screenlock);

				cb_drawrect(xoffset, yo, xw, yw, rect);
				return;
			}
		}
	}
}

/*
 * IF vs == &vstat, vstatlock needs to be held.
 * If not, not
 */
static int bitmap_draw_one_char(struct video_stats *vs, unsigned int xpos, unsigned int ypos)
{
	uint32_t fg;
	uint32_t bg;
	int		xoffset=(xpos-1)*vs->charwidth;
	int		yoffset=(ypos-1)*vs->charheight;
	int		x;
	int		y;
	int		fontoffset;
	int		altfont;
	unsigned char *this_font;
	WORD	sch;
	struct vstat_vmem *vmem_ptr;

	if(!bitmap_initialized) {
		return(-1);
	}

	vmem_ptr = vs->vmem;

	if(!vmem_ptr) {
		return(-1);
	}

	pthread_mutex_lock(&screen.screenlock);

	if ((xoffset + vs->charwidth > screen.screenwidth) || yoffset + vs->charheight > screen.screenheight) {
		pthread_mutex_unlock(&screen.screenlock);
		return(-1);
	}

	if(!screen.screen) {
		pthread_mutex_unlock(&screen.screenlock);
		return(-1);
	}

	sch=vmem_ptr->vmem[(ypos-1)*cio_textinfo.screenwidth+(xpos-1)];
	fg = vmem_ptr->fgvmem[(ypos-1)*cio_textinfo.screenwidth+(xpos-1)];
	bg = vmem_ptr->bgvmem[(ypos-1)*cio_textinfo.screenwidth+(xpos-1)];

	altfont = (sch>>11 & 0x01) | ((sch>>14) & 0x02);
	if (!vs->bright_altcharset)
		altfont &= ~0x01;
	if (!vs->blink_altcharset)
		altfont &= ~0x02;
	this_font=font[altfont];
	if (this_font == NULL)
		this_font = font[0];
	fontoffset=(sch&0xff)*vs->charheight;

	for(y=0; y<vs->charheight; y++) {
		memset_u32(&screen.screen[PIXEL_OFFSET(screen, xoffset, yoffset+y)],bg,vs->charwidth);
		if ((!((sch & 0x8000) && !vstat.blink)) || vstat.no_blink) {
			for(x=0; x<vs->charwidth; x++) {
				if(this_font[fontoffset] & (0x80 >> x))
					screen.screen[PIXEL_OFFSET(screen, xoffset+x, yoffset+y)]=fg;
			}
		}
		fontoffset++;
	}
	pthread_mutex_unlock(&screen.screenlock);

	return(0);
}

/*
 * Copies the current vmem into a temporary buffer.
 * Compares each cell with the previous buffer.
 * Any changed cells update the screen.
 * The temporary buffer becomes the previous buffer
 * 
 * Most of the hard work is in combining rectangles.
 */
static int update_rect(int sx, int sy, int width, int height, int force)
{
	int x,y;
	unsigned int pos;
	int	redraw_cursor=0;
	int	lastcharupdated=0;
	static unsigned short *last_vmem=NULL;
	static uint32_t *last_fgvmem=NULL;
	static uint32_t *last_bgvmem=NULL;
	static struct video_stats vs;
	struct rectangle this_rect;
	int this_rect_used=0;
	struct rectangle last_rect;
	int last_rect_used=0;
	struct vstat_vmem *vmem_ptr;
	int bright_attr_changed=0;
	int blink_attr_changed=0;

	if(!bitmap_initialized)
		return(-1);

	if(sx<=0)
		sx=1;
	if(sy<=0)
		sy=1;
	if(width<=0 || width>cio_textinfo.screenwidth)
		width=cio_textinfo.screenwidth;
	if(height<=0 || height>cio_textinfo.screenheight)
		height=cio_textinfo.screenheight;

	pthread_mutex_lock(&vstatlock);

	if (vstat.vmem == NULL) {
		pthread_mutex_unlock(&vstatlock);
		return -1;
	}

	if(vstat.vmem->vmem == NULL || vstat.vmem->fgvmem == NULL || vstat.vmem->bgvmem == NULL) {
		pthread_mutex_unlock(&vstatlock);
		return -1;
	}

	/* If the size has changed, realloc buffers to fit. */
	if(vs.cols!=vstat.cols || vs.rows != vstat.rows || last_vmem==NULL ||
	    last_fgvmem==NULL || last_bgvmem==NULL) {
		void *p;

		p=realloc(last_vmem, vstat.cols*vstat.rows*sizeof(unsigned short));
		if(p==NULL)
			return(-1);
		last_vmem=p;

		p=realloc(last_fgvmem, vstat.cols*vstat.rows*sizeof(last_fgvmem[0]));
		if(p==NULL)
			return(-1);
		last_fgvmem=p;

		p=realloc(last_bgvmem, vstat.cols*vstat.rows*sizeof(last_bgvmem[0]));
		if(p==NULL)
			return(-1);
		last_bgvmem=p;

		/* Clear out the newly allocated buffers (why?) */
		memset(last_vmem, 255, vstat.cols*vstat.rows*sizeof(last_vmem[0]));
		memset(last_fgvmem, 255, vstat.cols*vstat.rows*sizeof(last_fgvmem[0]));
		memset(last_bgvmem, 255, vstat.cols*vstat.rows*sizeof(last_bgvmem[0]));

		/* Force a full redraw */
		sx=1;
		sy=1;
		width=vstat.cols;
		height=vstat.rows;
		force=1;
		vs.cols=vstat.cols;
		vs.rows=vstat.rows;
	}

	/* Redraw cursor? */
	if(vstat.blink != vs.blink
			|| vstat.curs_col!=vs.curs_col
			|| vstat.curs_row!=vs.curs_row
			|| vstat.curs_start!=vs.curs_start
			|| vstat.curs_end!=vs.curs_end)
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

	/* Copy the vmems into cur */
	vmem_ptr = get_vmem(&vstat);

	if (hold_update)
		redraw_cursor = 0;

	/* 
	 * Now we go through each character seeing if it's changed (or force is set)
	 * We combine updates into rectangles by lines...
	 * 
	 * First, in the same line, we build this_rect.
	 * At the end of the line, if this_rect is the same width as the screen,
	 * we add it to last_rect.
	 */

	for(y=0;y<height;y++) {
		pos=(sy+y-1)*vstat.cols+(sx-1);
		for(x=0;x<width;x++) {
			/* Last this char been updated? */
			if(force																/* Forced */
					|| (vstat.vmem->vmem[pos] != last_vmem[pos])					/* Character and/or attribute */
					|| vstat.vmem->fgvmem[pos] != last_fgvmem[pos]					/* FG colour */
					|| vstat.vmem->bgvmem[pos] != last_bgvmem[pos]					/* BG colour */
					|| ((vstat.vmem->vmem[pos] & 0x8000) && (blink_attr_changed ||
							((vstat.blink != vs.blink) && (!vstat.no_blink)))) 	/* Blinking char */
					|| ((vstat.vmem->vmem[pos] & 0x0800) && bright_attr_changed)	/* Bright char */
					|| (redraw_cursor && ((vs.curs_col==sx+x && vs.curs_row==sy+y) || (vstat.curs_col==sx+x && vstat.curs_row==sy+y)))	/* Cursor */
					) {
				last_vmem[pos] = vstat.vmem->vmem[pos];
				last_fgvmem[pos] = vstat.vmem->fgvmem[pos];
				last_bgvmem[pos] = vstat.vmem->bgvmem[pos];
				bitmap_draw_one_char(&vstat, sx+x,sy+y);

				if(!redraw_cursor && sx+x==vstat.curs_col && sy+y==vstat.curs_row)
					redraw_cursor=1;

				/* If the last char was updated, we can add to the existing rectangle */
				if(lastcharupdated) {
					this_rect.width+=vstat.charwidth;
					lastcharupdated++;
				}
				else {
					/* Otherwise, send the old rectangle, and start a new one. */
					if(this_rect_used)
						send_rectangle(&vstat, this_rect.x, this_rect.y, this_rect.width, this_rect.height,FALSE);
					this_rect.x=(sx+x-1)*vstat.charwidth;
					this_rect.y=(sy+y-1)*vstat.charheight;
					this_rect.width=vstat.charwidth;
					this_rect.height=vstat.charheight;
					this_rect_used=1;
					lastcharupdated++;
				}
			}
			else {
				/* If this char wasn't updated, sent the rectangles */
				if(last_rect_used) {
					send_rectangle(&vstat, last_rect.x, last_rect.y, last_rect.width, last_rect.height, FALSE);
					last_rect_used=0;
				}
				if(this_rect_used) {
					send_rectangle(&vstat, this_rect.x, this_rect.y, this_rect.width, this_rect.height,FALSE);
					this_rect_used=0;
				}
				lastcharupdated=0;
			}
			pos++;
		}
		/* If ALL chars in the line were used, add to last_rect */
		if(lastcharupdated==width) {
			if(last_rect_used) {
				last_rect.height += vstat.charheight;
				this_rect_used=0;
			}
			else {
				last_rect=this_rect;
				last_rect_used=1;
				this_rect_used=0;
			}
		}
		/* Otherwise send any stale line buffers */
		else
		{
			if(last_rect_used) {
				send_rectangle(&vstat, last_rect.x, last_rect.y, last_rect.width, last_rect.height, FALSE);
				last_rect_used=0;
			}
			if(this_rect_used) {
				send_rectangle(&vstat, this_rect.x, this_rect.y, this_rect.width, this_rect.height, FALSE);
				this_rect_used=0;
			}
		}
		lastcharupdated=0;
	}

	if(last_rect_used)
		send_rectangle(&vstat, last_rect.x, last_rect.y, last_rect.width, last_rect.height, FALSE);
	if(this_rect_used) {
		send_rectangle(&vstat, this_rect.x, this_rect.y, this_rect.width, this_rect.height, FALSE);
	}
	release_vmem(vmem_ptr);

	/* Did we redraw over the cursor?  If so, update cursor info */
	if(redraw_cursor)
		bitmap_draw_cursor(&vstat);

	pthread_mutex_unlock(&vstatlock);

	vs = vstat;

	return(0);
}

/**********************/
/* Called from ciolib */
/**********************/
int bitmap_puttext(int sx, int sy, int ex, int ey, void *fill)
{
	int i, ret;
	uint16_t *buf = fill;
	uint32_t *fg;
	uint32_t *bg;

	fg = malloc((ex-sx+1)*(ey-sy+1)*sizeof(fg[0]));
	if (fg == NULL) {
		return 0;
	}
	bg = malloc((ex-sx+1)*(ey-sy+1)*sizeof(bg[0]));
	if (bg == NULL) {
		free(fg);
		return 0;
	}
	for (i=0; i<(ex-sx+1)*(ey-sy+1); i++)
		bitmap_attr2palette(buf[i]>>8, &fg[i], &bg[i]);
	ret = bitmap_pputtext(sx,sy,ex,ey,fill,fg,bg);
	free(fg);
	free(bg);
	return ret;
}

int bitmap_pputtext(int sx, int sy, int ex, int ey, void *fill, uint32_t *fg, uint32_t *bg)
{
	int x,y;
	unsigned char *out;
	uint32_t *fgout;
	uint32_t *bgout;
	WORD	sch;
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
			|| fill==NULL) {
		return(0);
	}

	pthread_mutex_lock(&vstatlock);
	vmem_ptr = get_vmem(&vstat);
	out=fill;
	fgout = fg;
	bgout = bg;
	for(y=sy-1;y<ey;y++) {
		for(x=sx-1;x<ex;x++) {
			sch=*(out++);
			sch |= (*(out++))<<8;
			set_vmem_cell(vmem_ptr, y*cio_textinfo.screenwidth+x, sch);
			if (fg)
				vmem_ptr->fgvmem[y*cio_textinfo.screenwidth+x] = *(fgout++);
			if (bg)
				vmem_ptr->bgvmem[y*cio_textinfo.screenwidth+x] = *(bgout++);
			bitmap_draw_one_char(&vstat, x+1, y+1);
		}
	}
	release_vmem(vmem_ptr);
	pthread_mutex_unlock(&vstatlock);
	send_text_rectangle(sx-1, sy-1, ex-sx + 1, ey - sy + 1, FALSE);
	return(1);
}

/* Called from main thread only */
int bitmap_pgettext(int sx, int sy, int ex, int ey, void *fill, uint32_t *fg, uint32_t *bg)
{
	int x,y;
	unsigned char *out;
	uint32_t *fgout;
	uint32_t *bgout;
	WORD	sch;
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
	out=fill;
	fgout=fg;
	bgout=bg;
	for(y=sy-1;y<ey;y++) {
		for(x=sx-1;x<ex;x++) {
			sch=vmem_ptr->vmem[y*cio_textinfo.screenwidth+x];
			*(out++)=sch & 0xff;
			*(out++)=sch >> 8;
			if (fg)
				*(fgout++) = vmem_ptr->fgvmem[y*cio_textinfo.screenwidth+x];
			if (bg)
				*(bgout++) = vmem_ptr->bgvmem[y*cio_textinfo.screenwidth+x];
		}
	}
	release_vmem(vmem_ptr);
	pthread_mutex_unlock(&vstatlock);
	return(1);
}

/* Called from main thread only */
void bitmap_gotoxy(int x, int y)
{
	if(!bitmap_initialized)
		return;
	/* Move cursor location */
	cio_textinfo.curx=x;
	cio_textinfo.cury=y;
	if(!hold_update) {
		/* Move visible cursor */
		pthread_mutex_lock(&vstatlock);
		vstat.curs_col=x+cio_textinfo.winleft-1;
		vstat.curs_row=y+cio_textinfo.wintop-1;
		pthread_mutex_unlock(&vstatlock);
	}
}

/* Called from ciolib thread only */
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
			break;
		default:
		    vstat.curs_start = vstat.default_curs_start;
		    vstat.curs_end = vstat.default_curs_end;
			break;
	}
	pthread_mutex_unlock(&vstatlock);
}

int bitmap_setfont(int font, int force, int font_num)
{
	int changemode=0;
	int	newmode=-1;
	struct text_info ti;
	char	*old;
	uint32_t *oldf;
	uint32_t *oldb;
	int		ow,oh;
	int		row,col;
	char	*new;
	uint32_t *newf;
	uint32_t *newb;
	int		attr;
	char	*pold;
	uint32_t *poldf;
	uint32_t *poldb;
	char	*pnew;
	uint32_t *pnewf;
	uint32_t *pnewb;
	int		result = CIOLIB_SETFONT_CHARHEIGHT_NOT_SUPPORTED;

	if(!bitmap_initialized)
		return(CIOLIB_SETFONT_NOT_INITIALIZED);
	if(font < 0 || font>(sizeof(conio_fontdata)/sizeof(struct conio_font_data_struct)-2)) {
		return(CIOLIB_SETFONT_INVALID_FONT);
	}

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
	if(changemode && (newmode==-1 || font_num > 1)) {
		result = CIOLIB_SETFONT_ILLEGAL_VIDMODE_CHANGE;
		goto error_return;
	}
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

		old=malloc(ow*oh*2);
		oldf=malloc(ow*oh*sizeof(oldf[0]));
		oldb=malloc(ow*oh*sizeof(oldf[0]));
		if(old && oldf && oldb) {
			pgettext(1,1,ow,oh,old,oldf,oldb);
			textmode(newmode);
			new=malloc(ti.screenwidth*ti.screenheight*2);
			newf=malloc(ti.screenwidth*ti.screenheight*sizeof(newf[0]));
			newb=malloc(ti.screenwidth*ti.screenheight*sizeof(newb[0]));
			if(!new) {
				free(old);
				FREE_AND_NULL(oldf);
				FREE_AND_NULL(oldb);
				return CIOLIB_SETFONT_MALLOC_FAILURE;
			}
			pold=old;
			poldf=oldf;
			poldb=oldb;
			pnew=new;
			pnewf=newf;
			pnewb=newb;
			for(row=0; row<ti.screenheight; row++) {
				for(col=0; col<ti.screenwidth; col++) {
					if(row < oh) {
						if(col < ow) {
							*(new++)=*(old++);
							*(new++)=*(old++);
							*(newf++)=*(oldf++);
							*(newb++)=*(oldb++);
						}
						else {
							*(new++)=space;
							*(new++)=attr;
							*(newf++) = ciolib_fg;
							*(newb++) = ciolib_bg;
						}
					}
					else {
						*(new++)=space;
						*(new++)=attr;
						*(newf++) = ciolib_fg;
						*(newb++) = ciolib_bg;
					}
				}
				if(row < oh) {
					for(;col<ow;col++) {
						old++;
						old++;
						oldf++;
						oldb++;
					}
				}
			}
			pputtext(1,1,ti.screenwidth,ti.screenheight,pnew,pnewf,pnewb);
			free(pnew);
			free(pnewf);
			free(pnewb);
			free(pold);
			free(poldf);
			free(poldb);
		}
		else {
			FREE_AND_NULL(old);
			FREE_AND_NULL(oldf);
			FREE_AND_NULL(oldb);
		}
	}
	bitmap_loadfont_locked(NULL);
	pthread_mutex_unlock(&vstatlock);
	return(CIOLIB_SETFONT_SUCCESS);

error_return:
	pthread_mutex_unlock(&vstatlock);
	return(result);
}

int bitmap_getfont(void)
{
	return(current_font[0]);
}

int bitmap_loadfont(char *filename)
{
	int ret;

	pthread_mutex_lock(&vstatlock);
	ret = bitmap_loadfont_locked(filename);
	pthread_mutex_unlock(&vstatlock);
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

	sourcepos=(y-1)*cio_textinfo.screenwidth+(x-1);
	destoffset=(((toy-1)*cio_textinfo.screenwidth+(tox-1))-sourcepos);

	pthread_mutex_lock(&vstatlock);
	vmem_ptr = get_vmem(&vstat);
	if (vstat.curs_row >= y && vstat.curs_row <= ey &&
	    vstat.curs_col >= x && vstat.curs_col <= ex)
		bitmap_draw_one_char(&vstat, vstat.curs_col, vstat.curs_row);
	for(cy=(direction==-1?(height-1):0); cy<height && cy>=0; cy+=direction) {
		memmove(&(vmem_ptr->vmem[sourcepos+destoffset]), &(vmem_ptr->vmem[sourcepos]), sizeof(vmem_ptr->vmem[0])*width);
		memmove(&(vmem_ptr->fgvmem[sourcepos+destoffset]), &(vmem_ptr->fgvmem[sourcepos]), sizeof(vmem_ptr->fgvmem[0])*width);
		memmove(&(vmem_ptr->bgvmem[sourcepos+destoffset]), &(vmem_ptr->bgvmem[sourcepos]), sizeof(vmem_ptr->bgvmem[0])*width);
		sourcepos += direction * cio_textinfo.screenwidth;
	}
	ssourcepos=(y-1)     *cio_textinfo.screenwidth*vstat.charwidth*vstat.charheight + (x-1)  *vstat.charwidth;
	sdestoffset=(((toy-1)*cio_textinfo.screenwidth*vstat.charwidth*vstat.charheight + (tox-1)*vstat.charwidth)-ssourcepos);
	pthread_mutex_lock(&screen.screenlock);
	for(screeny=(direction==-1?(height-1)*vstat.charheight:0); screeny<height*vstat.charheight && screeny>=0; screeny+=direction) {
		memmove(&(screen.screen[ssourcepos+sdestoffset]), &(screen.screen[ssourcepos]), sizeof(screen.screen[0])*width*vstat.charwidth);
		ssourcepos += direction * cio_textinfo.screenwidth*vstat.charwidth;
	}
	/* TODO: Just resend the whole screen... */
	request_pixels_locked();
	pthread_mutex_unlock(&screen.screenlock);
	release_vmem(vmem_ptr);
	pthread_mutex_unlock(&vstatlock);

	return(1);
}

void bitmap_clreol(void)
{
	int pos,x;
	WORD fill=(cio_textinfo.attribute<<8)|space;
	struct vstat_vmem *vmem_ptr;
	int row;

	row = cio_textinfo.cury + cio_textinfo.wintop - 1;
	pos=(row - 1)*cio_textinfo.screenwidth;
	pthread_mutex_lock(&vstatlock);
	vmem_ptr = get_vmem(&vstat);
	for(x=cio_textinfo.curx+cio_textinfo.winleft-2; x<cio_textinfo.winright; x++) {
		set_vmem_cell(vmem_ptr, pos+x, fill);
		vmem_ptr->fgvmem[pos+x] = ciolib_fg;
		vmem_ptr->bgvmem[pos+x] = ciolib_bg;
		bitmap_draw_one_char(&vstat, x+1, row);
	}
	release_vmem(vmem_ptr);
	pthread_mutex_unlock(&vstatlock);
	send_text_rectangle(cio_textinfo.curx+cio_textinfo.winleft-2, row-1, cio_textinfo.winright - cio_textinfo.curx+cio_textinfo.winleft - 3, 1, FALSE);
}

void bitmap_clrscr(void)
{
	int x,y;
	WORD fill=(cio_textinfo.attribute<<8)|space;
	struct vstat_vmem *vmem_ptr;

	pthread_mutex_lock(&vstatlock);
	vmem_ptr = get_vmem(&vstat);
	for(y=cio_textinfo.wintop-1; y<cio_textinfo.winbottom; y++) {
		for(x=cio_textinfo.winleft-1; x<cio_textinfo.winright; x++) {
			set_vmem_cell(vmem_ptr, y*cio_textinfo.screenwidth+x, fill);
			vmem_ptr->fgvmem[y*cio_textinfo.screenwidth+x] = ciolib_fg;
			vmem_ptr->bgvmem[y*cio_textinfo.screenwidth+x] = ciolib_bg;
			bitmap_draw_one_char(&vstat, x+1, y+1);
		}
	}
	release_vmem(vmem_ptr);
	pthread_mutex_unlock(&vstatlock);
	send_text_rectangle(cio_textinfo.winleft-1, cio_textinfo.wintop-1, cio_textinfo.winright - cio_textinfo.winleft + 1, cio_textinfo.winbottom - cio_textinfo.wintop + 1, TRUE);
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
		*b=vstat.curs_blink;
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
		vstat.curs_blink=b;
	if(v>=0)
		vstat.curs_visible=v;
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
	uint32_t *rect = NULL;

	if(callbacks.drawrect)
		rect=(uint32_t *)malloc(sizeof(rect[0]));

	pthread_mutex_lock(&screen.screenlock);

	if (callbacks.drawrect && rect == NULL)
		request_pixels_locked();

	if (x < screen.screenwidth && y < screen.screenheight)
		screen.screen[PIXEL_OFFSET(screen, x, y)]=colour;

	pthread_mutex_unlock(&screen.screenlock);

	if (rect) {
		rect[0]=colour;
		cb_drawrect(x,y,1,1,rect);
	}

	return 1;
}

int bitmap_setpixels(uint32_t sx, uint32_t sy, uint32_t ex, uint32_t ey, uint32_t x_off, uint32_t y_off, struct ciolib_pixels *pixels, void *mask)
{
	uint32_t *rect = NULL;
	uint32_t *rp;
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

	if(callbacks.drawrect)
		rp = rect=(uint32_t *)malloc(height * width * sizeof(rect[0]));

	pthread_mutex_lock(&screen.screenlock);

	/* If malloc() failed, redraw the whole screen */
	if (callbacks.drawrect && rect == NULL)
		request_pixels_locked();

	if (ex > screen.screenwidth || ey > screen.screenheight) {
		pthread_mutex_unlock(&screen.screenlock);
		return 0;
	}

	for (y = sy; y <= ey; y++) {
		pos = pixels->width*(y-sy+y_off)+x_off;
		if (mask == NULL) {
			memcpy(&screen.screen[PIXEL_OFFSET(screen, sx, y)], &pixels->pixels[pos], width * sizeof(pixels->pixels[0]));
			if (rect)
				memcpy(&rect[(y-sy)*width], &pixels->pixels[pos], width * sizeof(pixels->pixels[0]));
		}
		else {
			for (x = sx; x <= ex; x++) {
				pos++;
				mask_byte = pos / 8;
				mask_bit = pos % 8;
				mask_bit = 0x80 >> mask_bit;
				if (m[mask_byte] & mask_bit) {
					screen.screen[PIXEL_OFFSET(screen, x, y)] = pixels->pixels[pos];
					if (rect)
						*(rp++) = pixels->pixels[pos];
				}
				else {
					if (rect) {
						*(rp++) = screen.screen[PIXEL_OFFSET(screen, x, y)];
					}
				}
			}
		}
	}

	pthread_mutex_unlock(&screen.screenlock);

	if(rect) {
		cb_drawrect(sx,sy,width,height,rect);
	}

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

	pthread_mutex_lock(&screen.screenlock);
	if (ex >= screen.screenwidth || ey >= screen.screenheight) {
		pthread_mutex_unlock(&screen.screenlock);
		free(pixels);
		return NULL;
	}

	for (y = sy; y <= ey; y++)
		memcpy(&pixels->pixels[width*(y-sy)], &screen.screen[PIXEL_OFFSET(screen, sx, y)], width * sizeof(pixels->pixels[0]));
	pthread_mutex_unlock(&screen.screenlock);

	return pixels;
}

/***********************/
/* Called from drivers */
/***********************/
/*
 * This function is intended to be called from the driver.
 * as a result, it cannot block waiting for driver status
 *
 * Care MUST be taken to avoid deadlocks...
 */
int bitmap_drv_init_mode(int mode, int *width, int *height)
{
    int i;
	uint32_t *newscreen;

	if(!bitmap_initialized)
		return(-1);

	pthread_mutex_lock(&vstatlock);

	if(load_vmode(&vstat, mode)) {
		pthread_mutex_unlock(&vstatlock);
		return(-1);
	}

	/* Initialize video memory with black background, white foreground */
	for (i = 0; i < vstat.cols*vstat.rows; ++i) {
	    vstat.vmem->vmem[i] = 0x0700;
	    vstat.vmem->bgvmem[i] = vstat.palette[0];
	    vstat.vmem->fgvmem[i] = vstat.palette[7];
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
		pthread_mutex_unlock(&vstatlock);
		return(-1);
	}
	screen.screen=newscreen;
	memset_u32(screen.screen,vstat.palette[0],screen.screenwidth*screen.screenheight);
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
	pthread_mutex_unlock(&vstatlock);

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
int bitmap_drv_init(void (*drawrect_cb) (int xpos, int ypos, int width, int height, uint32_t *data)
				,void (*flush_cb) (void))
{
	if(bitmap_initialized)
		return(-1);
	cio_api.options |= CONIO_OPT_LOADABLE_FONTS | CONIO_OPT_BLINK_ALT_FONT
			| CONIO_OPT_BOLD_ALT_FONT | CONIO_OPT_BRIGHT_BACKGROUND
			| CONIO_OPT_SET_PIXEL | CONIO_OPT_CUSTOM_CURSOR
			| CONIO_OPT_FONT_SELECT | CONIO_OPT_EXTENDED_PALETTE;
	pthread_mutex_init(&callbacks.lock, NULL);
	pthread_mutex_init(&vstatlock, NULL);
	pthread_mutex_init(&screen.screenlock, NULL);
	pthread_mutex_lock(&vstatlock);
	vstat.vmem=NULL;
	vstat.flags = VIDMODES_FLAG_PALETTE_VMEM;
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
	request_pixels_locked();
	pthread_mutex_unlock(&screen.screenlock);
}

void bitmap_drv_request_some_pixels(int x, int y, int width, int height)
{
	/* TODO: Some sort of queue here? */
	bitmap_drv_request_pixels();
}
