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

static uint32_t *screen=NULL;
int screenwidth=0;
int screenheight=0;
#define PIXEL_OFFSET(x,y)	( (y)*screenwidth+(x) )

static int default_font=-99;
static int current_font[4]={-99, -99, -99, -99};
static int bitmap_initialized=0;
pthread_mutex_t vmem_lock;
struct video_stats vstat;

struct bitmap_callbacks {
	void	(*drawrect)		(int xpos, int ypos, int width, int height, uint32_t *data);
	void	(*flush)		(void);
};

pthread_mutex_t		vstatlock;
pthread_mutex_t		screenlock;
static struct bitmap_callbacks callbacks;
static unsigned char *font[4];
static unsigned char space=' ';
int force_redraws=0;
int update_pixels = 0;

struct rectangle {
	int x;
	int y;
	int width;
	int height;
};

static int update_rect(int sx, int sy, int width, int height, int force);
static void bitmap_draw_cursor(struct video_stats *vs);
static int bitmap_draw_one_char(struct video_stats *vs, unsigned int xpos, unsigned int ypos);

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
	pthread_mutex_lock(&screenlock);
	ret=screen;
	pthread_mutex_unlock(&screenlock);
	return(ret);
}

static struct vstat_vmem *lock_vmem(struct video_stats *vs)
{
	struct vstat_vmem *ret;
	pthread_mutex_lock(&vstatlock);
	ret = get_vmem(vs);
	pthread_mutex_unlock(&vstatlock);
	pthread_mutex_lock(&vmem_lock);
	return ret;
}

static void unlock_vmem(struct vstat_vmem *vm)
{
	pthread_mutex_unlock(&vmem_lock);
	release_vmem(vm);
}

/* Blinker Thread */
static void blinker_thread(void *data)
{
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
		if(force_redraws)
			update_rect(0,0,0,0,force_redraws--);
		else
			update_rect(0,0,0,0,FALSE);
		if (update_pixels)
			send_rectangle(&vstat, 0, 0, screenwidth, screenheight, update_pixels--);
		callbacks.flush();
	}
}

/*
 * MUST be called only once and before any other bitmap functions
 */
int bitmap_init(void (*drawrect_cb) (int xpos, int ypos, int width, int height, uint32_t *data)
				,void (*flush_cb) (void))
{
	if(bitmap_initialized)
		return(-1);
	pthread_mutex_init(&vmem_lock, NULL);
	pthread_mutex_init(&vstatlock, NULL);
	pthread_mutex_init(&screenlock, NULL);
	pthread_mutex_lock(&vmem_lock);
	vstat.vmem=NULL;
	vstat.flags = VIDMODES_FLAG_PALETTE_VMEM;
	pthread_mutex_unlock(&vmem_lock);

	callbacks.drawrect=drawrect_cb;
	callbacks.flush=flush_cb;
	bitmap_initialized=1;
	_beginthread(blinker_thread,0,NULL);

	return(0);
}

/*
 * This function is intended to be called from the driver.
 * as a result, it cannot block waiting for driver status
 *
 * Care MUST be taken to avoid deadlocks...
 */
int bitmap_init_mode(int mode, int *width, int *height)
{
    int i;
	uint32_t *newscreen;

	if(!bitmap_initialized)
		return(-1);

	pthread_mutex_lock(&screenlock);
	pthread_mutex_lock(&vstatlock);
	pthread_mutex_lock(&vmem_lock);

	if(load_vmode(&vstat, mode)) {
		pthread_mutex_unlock(&vmem_lock);
		pthread_mutex_unlock(&vstatlock);
		pthread_mutex_unlock(&screenlock);
		return(-1);
	}

	/* Initialize video memory with black background, white foreground */
	for (i = 0; i < vstat.cols*vstat.rows; ++i) {
	    vstat.vmem->vmem[i] = 0x0700;
	    vstat.vmem->bgvmem[i] = vstat.palette[0];
	    vstat.vmem->fgvmem[i] = vstat.palette[7];
	}
	pthread_mutex_unlock(&vmem_lock);

	screenwidth=vstat.charwidth*vstat.cols;
	if(width)
		*width=screenwidth;
	screenheight=vstat.charheight*vstat.rows;
	if(height)
		*height=screenheight;
	newscreen=realloc(screen, screenwidth*screenheight*sizeof(screen[0]));

	if(!newscreen) {
		pthread_mutex_unlock(&vstatlock);
		pthread_mutex_unlock(&screenlock);
		return(-1);
	}
	screen=newscreen;
	memset_u32(screen,vstat.palette[0],screenwidth*screenheight);
	pthread_mutex_unlock(&vstatlock);
	pthread_mutex_unlock(&screenlock);
	for (i=0; i<sizeof(current_font)/sizeof(current_font[0]); i++)
		current_font[i]=default_font;
	bitmap_loadfont(NULL);

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
 * Send by ciolib side, should not block in driver
 * Generally, if the driver may block on a rectangle draw, the updates
 * should be cached until flush is called.
 */
void send_rectangle(struct video_stats *vs, int xoffset, int yoffset, int width, int height, int force)
{
	uint32_t *rect;
	int pixel=0;
	int inpixel;
	int x,y;

	if(!bitmap_initialized)
		return;
	pthread_mutex_lock(&screenlock);
	if(callbacks.drawrect) {
		if(xoffset < 0 || xoffset >= screenwidth || yoffset < 0 || yoffset >= screenheight || width <= 0 || width > screenwidth || height <=0 || height >screenheight)
			goto end;

		rect=(uint32_t *)malloc(width*height*sizeof(rect[0]));
		if(!rect)
			goto end;

		for(y=0; y<height; y++) {
			inpixel=PIXEL_OFFSET(xoffset, yoffset+y);
			for(x=0; x<width; x++)
				rect[pixel++]=screen[inpixel++];
		}
		pthread_mutex_unlock(&screenlock);
		callbacks.drawrect(xoffset,yoffset,width,height,rect);
		return;
	}
end:
	pthread_mutex_unlock(&screenlock);
}

/********************************************************/
/* High Level Stuff										*/
/********************************************************/

/* Called from main thread only (Passes Event) */

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
	force_redraws++;
}

int bitmap_attr2palette(uint8_t attr, uint32_t *fgp, uint32_t *bgp)
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
	int col, row;

	bitmap_attr2palette(cell>>8, &fg, &bg);

	vmem_ptr->vmem[pos] = cell;
	vmem_ptr->fgvmem[pos] = fg;
	vmem_ptr->bgvmem[pos] = bg;
	row = pos / vstat.cols;
	col = pos - (row * vstat.cols);
	pthread_mutex_unlock(&vmem_lock);
	bitmap_draw_one_char(&vstat, col+1, row+1);
	pthread_mutex_lock(&vmem_lock);
	update_pixels++;
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
			|| toy>cio_textinfo.screenheight)
		return(0);

	if(toy > y)
		direction=-1;

	sourcepos=(y-1)*cio_textinfo.screenwidth+(x-1);
	destoffset=(((toy-1)*cio_textinfo.screenwidth+(tox-1))-sourcepos);

	pthread_mutex_lock(&screenlock);
	vmem_ptr = lock_vmem(&vstat);
	for(cy=(direction==-1?(height-1):0); cy<height && cy>=0; cy+=direction) {
		memmove(&(vmem_ptr->vmem[sourcepos+destoffset]), &(vmem_ptr->vmem[sourcepos]), sizeof(vmem_ptr->vmem[0])*width);
		memmove(&(vmem_ptr->fgvmem[sourcepos+destoffset]), &(vmem_ptr->fgvmem[sourcepos]), sizeof(vmem_ptr->fgvmem[0])*width);
		memmove(&(vmem_ptr->bgvmem[sourcepos+destoffset]), &(vmem_ptr->bgvmem[sourcepos]), sizeof(vmem_ptr->bgvmem[0])*width);
		sourcepos += direction * cio_textinfo.screenwidth;
	}
	if (vstat.curs_row >= y && vstat.curs_row <= ey &&
	    vstat.curs_col >= x && vstat.curs_col <= ex)
		bitmap_draw_one_char(&vstat, vstat.curs_col, vstat.curs_row);
	ssourcepos=(y-1)     *cio_textinfo.screenwidth*vstat.charwidth*vstat.charheight + (x-1)  *vstat.charwidth;
	sdestoffset=(((toy-1)*cio_textinfo.screenwidth*vstat.charwidth*vstat.charheight + (tox-1)*vstat.charwidth)-ssourcepos);
	for(screeny=(direction==-1?(height-1)*vstat.charheight:0); screeny<height*vstat.charheight && screeny>=0; screeny+=direction) {
		memmove(&(screen[ssourcepos+sdestoffset]), &(screen[ssourcepos]), sizeof(screen[0])*width*vstat.charwidth);
		ssourcepos += direction * cio_textinfo.screenwidth*vstat.charwidth;
	}

	unlock_vmem(vmem_ptr);
	pthread_mutex_unlock(&screenlock);

	return(1);
}

void bitmap_clreol(void)
{
	int pos,x;
	WORD fill=(cio_textinfo.attribute<<8)|space;
	struct vstat_vmem *vmem_ptr;

	pos=(cio_textinfo.cury+cio_textinfo.wintop-2)*cio_textinfo.screenwidth;
	vmem_ptr = lock_vmem(&vstat);
	for(x=cio_textinfo.curx+cio_textinfo.winleft-2; x<cio_textinfo.winright; x++)
		set_vmem_cell(vmem_ptr, pos+x, fill);
	unlock_vmem(vmem_ptr);
}

void bitmap_clrscr(void)
{
	int x,y;
	WORD fill=(cio_textinfo.attribute<<8)|space;
	struct vstat_vmem *vmem_ptr;

	vmem_ptr = lock_vmem(&vstat);
	for(y=cio_textinfo.wintop-1; y<cio_textinfo.winbottom; y++) {
		for(x=cio_textinfo.winleft-1; x<cio_textinfo.winright; x++)
			set_vmem_cell(vmem_ptr, y*cio_textinfo.screenwidth+x, fill);
	}
	unlock_vmem(vmem_ptr);
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
			|| fill==NULL)
		return(0);

	vmem_ptr = lock_vmem(&vstat);
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
		}
	}
	unlock_vmem(vmem_ptr);
	return(1);
}

int bitmap_puttext(int sx, int sy, int ex, int ey, void *fill)
{
	int i, ret;
	uint16_t *buf = fill;
	uint32_t *fg;
	uint32_t *bg;

	fg = malloc((ex-sx+1)*(ey-sy+1)*sizeof(fg[0]));
	if (fg == NULL)
		return 0;
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
			|| fill==NULL)
		return(0);

	vmem_ptr = lock_vmem(&vstat);
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
	unlock_vmem(vmem_ptr);
	return(1);
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
	int		ow,oh;
	int		row,col;
	char	*new;
	int		attr;
	char	*pold;
	char	*pnew;
	int		result = CIOLIB_SETFONT_CHARHEIGHT_NOT_SUPPORTED;

	if(!bitmap_initialized)
		return(CIOLIB_SETFONT_NOT_INITIALIZED);
	if(font < 0 || font>(sizeof(conio_fontdata)/sizeof(struct conio_font_data_struct)-2))
		return(CIOLIB_SETFONT_INVALID_FONT);

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
	pthread_mutex_unlock(&vstatlock);

	if(changemode) {
		gettextinfo(&ti);

		attr=ti.attribute;
		ow=ti.screenwidth;
		oh=ti.screenheight;

		old=malloc(ow*oh*2);
		if(old) {
			gettext(1,1,ow,oh,old);
			textmode(newmode);
			new=malloc(ti.screenwidth*ti.screenheight*2);
			if(!new) {
				free(old);
				return CIOLIB_SETFONT_MALLOC_FAILURE;
			}
			pold=old;
			pnew=new;
			for(row=0; row<ti.screenheight; row++) {
				for(col=0; col<ti.screenwidth; col++) {
					if(row < oh) {
						if(col < ow) {
							*(new++)=*(old++);
							*(new++)=*(old++);
						}
						else {
							*(new++)=space;
							*(new++)=attr;
						}
					}
					else {
						*(new++)=space;
						*(new++)=attr;
					}
				}
				if(row < oh) {
					for(;col<ow;col++) {
						old++;
						old++;
					}
				}
			}
			puttext(1,1,ti.screenwidth,ti.screenheight,new);
			free(pnew);
			free(pold);
		}
	}
	bitmap_loadfont(NULL);
	return(CIOLIB_SETFONT_SUCCESS);

error_return:
	pthread_mutex_unlock(&vstatlock);
	return(result);
}

int bitmap_getfont(void)
{
	return(current_font[0]);
}

void bitmap_setscaling(int new_value)
{
	pthread_mutex_lock(&vstatlock);
	if(new_value > 0)
		vstat.scaling = new_value;
	pthread_mutex_unlock(&vstatlock);
}

int bitmap_getscaling(void)
{
	int ret;

	pthread_mutex_lock(&vstatlock);
	ret = vstat.scaling;
	pthread_mutex_unlock(&vstatlock);
	return ret;
}

/* Called from event thread only */
int bitmap_loadfont(char *filename)
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

	pthread_mutex_lock(&vstatlock);
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

	force_redraws++;
	pthread_mutex_unlock(&vstatlock);
    return(0);

error_return:
	for (i=0; i<sizeof(font)/sizeof(font[0]); i++)
		FREE_AND_NULL(font[i]);
	if(fontfile)
		fclose(fontfile);
	pthread_mutex_unlock(&vstatlock);
	return(-1);
}

static void bitmap_draw_cursor(struct video_stats *vs)
{
	int y;
	char attr;
	int pixel;
	int xoffset,yoffset;
	int yo, xw, yw;

	if(!bitmap_initialized)
		return;
	if(vs->curs_visible) {
		if(vs->blink || (!vs->curs_blink)) {
			if(vs->curs_start<=vs->curs_end) {
				xoffset=(vs->curs_col-1)*vs->charwidth;
				yoffset=(vs->curs_row-1)*vs->charheight;
				if(xoffset < 0 || yoffset < 0)
					return;
				attr=cio_textinfo.attribute&0x0f;
	
				pthread_mutex_lock(&screenlock);
				for(y=vs->curs_start; y<=vs->curs_end; y++) {
					if(xoffset < screenwidth && (yoffset+y) < screenheight) {
						pixel=PIXEL_OFFSET(xoffset, yoffset+y);
						memset_u32(screen+pixel,vs->palette[attr],vs->charwidth);
					}
				}
				pthread_mutex_unlock(&screenlock);
				yo = yoffset+vs->curs_start;
				xw = vs->charwidth;
				yw = vs->curs_end-vs->curs_start+1;
				send_rectangle(vs, xoffset, yo, xw, yw,FALSE);
				return;
			}
		}
	}
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

	if(!bitmap_initialized)
		return(-1);

	if(!vs->vmem) {
		return(-1);
	}

	pthread_mutex_lock(&screenlock);

	if ((xoffset + vs->charwidth > screenwidth) || yoffset + vs->charheight > screenheight) {
		pthread_mutex_unlock(&screenlock);
		return(-1);
	}

	if(!screen) {
		pthread_mutex_unlock(&screenlock);
		return(-1);
	}

	vmem_ptr = lock_vmem(vs);

	sch=vmem_ptr->vmem[(ypos-1)*cio_textinfo.screenwidth+(xpos-1)];
	fg = vmem_ptr->fgvmem[(ypos-1)*cio_textinfo.screenwidth+(xpos-1)];
	bg = vmem_ptr->bgvmem[(ypos-1)*cio_textinfo.screenwidth+(xpos-1)];

	unlock_vmem(vmem_ptr);

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
		memset_u32(&screen[PIXEL_OFFSET(xoffset, yoffset+y)],bg,vs->charwidth);
		for(x=0; x<vs->charwidth; x++) {
			if(this_font[fontoffset] & (0x80 >> x))
				screen[PIXEL_OFFSET(xoffset+x, yoffset+y)]=fg;
		}
		fontoffset++;
	}
	pthread_mutex_unlock(&screenlock);

	return(0);
}

static int update_rect(int sx, int sy, int width, int height, int force)
{
	int x,y;
	unsigned int pos;
	int	redraw_cursor=0;
	int	lastcharupdated=0;
	static unsigned short *last_vmem=NULL;
	static uint32_t *last_fgvmem=NULL;
	static uint32_t *last_bgvmem=NULL;
	static unsigned short *this_vmem=NULL;
	static uint32_t *this_fgvmem=NULL;
	static uint32_t *this_bgvmem=NULL;
	static struct video_stats vs;
	struct video_stats cvstat;
	struct vstat_vmem cvmem;
	struct rectangle this_rect;
	int this_rect_used=0;
	struct rectangle last_rect;
	int last_rect_used=0;
	struct vstat_vmem *vmem_ptr;

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
	if(vs.cols!=vstat.cols || vs.rows != vstat.rows || last_vmem==NULL || this_vmem == NULL ||
	    last_fgvmem==NULL || this_fgvmem == NULL || last_bgvmem==NULL || this_bgvmem == NULL) {
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

		memset(last_vmem, 255, vstat.cols*vstat.rows*sizeof(last_vmem[0]));
		memset(last_fgvmem, 255, vstat.cols*vstat.rows*sizeof(last_fgvmem[0]));
		memset(last_bgvmem, 255, vstat.cols*vstat.rows*sizeof(last_bgvmem[0]));
		sx=1;
		sy=1;
		width=vstat.cols;
		height=vstat.rows;
		force=1;
		vs.cols=vstat.cols;
		vs.rows=vstat.rows;

		p=realloc(this_vmem, vstat.cols*vstat.rows*sizeof(unsigned short));
		if(p==NULL)
			return(-1);
		this_vmem = p;

		p=realloc(this_fgvmem, vstat.cols*vstat.rows*sizeof(this_fgvmem[0]));
		if(p==NULL)
			return(-1);
		this_fgvmem = p;

		p=realloc(this_bgvmem, vstat.cols*vstat.rows*sizeof(this_bgvmem[0]));
		if(p==NULL)
			return(-1);
		this_bgvmem = p;
	}

	/* Redraw cursor */
	if(vstat.blink != vs.blink
			|| vstat.curs_col!=vs.curs_col
			|| vstat.curs_row!=vs.curs_row
			|| vstat.curs_start!=vs.curs_start
			|| vstat.curs_end!=vs.curs_end)
		redraw_cursor=1;
	cvstat = vstat;
	pthread_mutex_unlock(&vstatlock);
	vmem_ptr = get_vmem(&vstat);
	pthread_mutex_lock(&vmem_lock);
	cvstat.vmem = &cvmem;
	cvstat.vmem->refcount = 1;
	cvstat.vmem->vmem = this_vmem;
	cvstat.vmem->fgvmem = this_fgvmem;
	cvstat.vmem->bgvmem = this_bgvmem;
	memcpy(cvstat.vmem->vmem, vmem_ptr->vmem, vstat.cols*vstat.rows*sizeof(unsigned short));
	memcpy(cvstat.vmem->fgvmem, vmem_ptr->fgvmem, vstat.cols*vstat.rows*sizeof(vmem_ptr->fgvmem[0]));
	memcpy(cvstat.vmem->bgvmem, vmem_ptr->bgvmem, vstat.cols*vstat.rows*sizeof(vmem_ptr->bgvmem[0]));
	pthread_mutex_unlock(&vmem_lock);

	for(y=0;y<height;y++) {
		pos=(sy+y-1)*cvstat.cols+(sx-1);
		for(x=0;x<width;x++) {
			/* TODO: Need to deal with blink/bgbright here too... */
			if(force
					|| ((cvstat.blink != vs.blink) && (cvstat.vmem->vmem[pos]>>15) && (!cvstat.no_blink)) 	/* Blinking char */
					|| (redraw_cursor && ((vs.curs_col==sx+x && vs.curs_row==sy+y) || (cvstat.curs_col==sx+x && cvstat.curs_row==sy+y)))	/* Cursor */
					) {
				last_vmem[pos] = cvstat.vmem->vmem[pos];
				last_fgvmem[pos] = cvstat.vmem->fgvmem[pos];
				last_bgvmem[pos] = cvstat.vmem->bgvmem[pos];
				bitmap_draw_one_char(&cvstat, sx+x,sy+y);

				if(!redraw_cursor && sx+x==cvstat.curs_col && sy+y==cvstat.curs_row)
					redraw_cursor=1;

				if(lastcharupdated) {
					this_rect.width+=cvstat.charwidth;
					lastcharupdated++;
				}
				else {
					if(this_rect_used) {
						send_rectangle(&cvstat, this_rect.x, this_rect.y, this_rect.width, this_rect.height,FALSE);
					}
					this_rect.x=(sx+x-1)*cvstat.charwidth;
					this_rect.y=(sy+y-1)*cvstat.charheight;
					this_rect.width=cvstat.charwidth;
					this_rect.height=cvstat.charheight;
					this_rect_used=1;
					lastcharupdated++;
				}
			}
			else {
				if(last_rect_used) {
					send_rectangle(&cvstat, last_rect.x, last_rect.y, last_rect.width, last_rect.height, FALSE);
					last_rect_used=0;
				}
				if(this_rect_used) {
					send_rectangle(&cvstat, this_rect.x, this_rect.y, this_rect.width, this_rect.height,FALSE);
					this_rect_used=0;
				}
				lastcharupdated=0;
			}
			pos++;
		}
		/* If ALL chars in the line were used, add to last_rect */
		if(lastcharupdated==width) {
			if(last_rect_used) {
				last_rect.height += cvstat.charheight;
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
				send_rectangle(&cvstat, last_rect.x, last_rect.y, last_rect.width, last_rect.height, FALSE);
				last_rect_used=0;
			}
			if(this_rect_used) {
				send_rectangle(&cvstat, this_rect.x, this_rect.y, this_rect.width, this_rect.height, FALSE);
				this_rect_used=0;
			}
		}
		lastcharupdated=0;
	}

	if(last_rect_used)
		send_rectangle(&cvstat, last_rect.x, last_rect.y, last_rect.width, last_rect.height, FALSE);
	if(this_rect_used)
		send_rectangle(&cvstat, this_rect.x, this_rect.y, this_rect.width, this_rect.height, FALSE);

	vs = cvstat;

	/* Did we redraw the cursor?  If so, update cursor info */
	if(redraw_cursor)
		bitmap_draw_cursor(&cvstat);

	return(0);
}

int bitmap_setpixel(uint32_t x, uint32_t y, uint32_t colour)
{
	pthread_mutex_lock(&screenlock);
	if (x < screenwidth && y < screenheight)
		screen[PIXEL_OFFSET(x, y)]=colour;
	update_pixels++;
	pthread_mutex_unlock(&screenlock);
	return 1;
}
