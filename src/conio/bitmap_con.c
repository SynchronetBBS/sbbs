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
#include "keys.h"
#include "vidmodes.h"
#include "allfonts.h"
#include "bitmap_con.h"

static char *screen=NULL;
int screenwidth=0;
int screenheight=0;
#define PIXEL_OFFSET(x,y)	( (y)*screenwidth+(x) )

static int default_font=-99;
static int current_font=-99;
static int current_secondary_font=-99;
static int bitmap_initialized=0;
struct video_stats vstat;
static int *damaged=NULL;

struct bitmap_callbacks {
	void	(*drawrect)		(int xpos, int ypos, int width, int height, unsigned char *data);
	void	(*flush)		(void);
};

pthread_mutex_t		vstatlock;
pthread_mutex_t		screenlock;
static struct bitmap_callbacks callbacks;
static unsigned char *font;
static unsigned char *secondary_font;
static unsigned char space=' ';
int force_redraws=0;

struct rectangle {
	int x;
	int y;
	int width;
	int height;
};

static int update_rect(int sx, int sy, int width, int height, int force);

/* Blinker Thread */
static void blinker_thread(void *data)
{
	int count=0;

	while(1) {
		do {
			SLEEP(10);
		} while(screen==NULL);
		count++;
		pthread_mutex_lock(&vstatlock);
		if(count==50) {
			if(vstat.blink)
				vstat.blink=FALSE;
			else
				vstat.blink=TRUE;
			count=0;
		}
		if(force_redraws)
			update_rect(0,0,0,0,force_redraws--);
		else
			update_rect(0,0,0,0,FALSE);
		pthread_mutex_unlock(&vstatlock);
		callbacks.flush();
	}
}

/*
 * MUST be called only once and before any other bitmap functions
 */
int bitmap_init(void (*drawrect_cb) (int xpos, int ypos, int width, int height, unsigned char *data)
				,void (*flush_cb) (void))
{
	if(bitmap_initialized)
		return(-1);
	pthread_mutex_init(&vstatlock, NULL);
	pthread_mutex_init(&screenlock, NULL);
	pthread_mutex_lock(&vstatlock);
	vstat.vmem=NULL;
	pthread_mutex_unlock(&vstatlock);

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
	char *newscreen;
	int *newdamaged;

	if(!bitmap_initialized)
		return(-1);

	pthread_mutex_lock(&vstatlock);

	if(load_vmode(&vstat, mode)) {
		pthread_mutex_unlock(&vstatlock);
		return(-1);
	}

	/* Initialize the damaged array */

	newdamaged=(int *)malloc(sizeof(int)*vstat.rows);
	if(newdamaged==NULL) {
		pthread_mutex_unlock(&vstatlock);
		return(-1);
	}
	damaged=newdamaged;

	/* Initialize video memory with black background, white foreground */
	for (i = 0; i < vstat.cols*vstat.rows; ++i)
	    vstat.vmem[i] = 0x0700;

	pthread_mutex_lock(&screenlock);
	screenwidth=vstat.charwidth*vstat.cols;
	if(width)
		*width=screenwidth;
	screenheight=vstat.charheight*vstat.rows;
	if(height)
		*height=screenheight;
	newscreen=realloc(screen, screenwidth*screenheight);
	if(!newscreen) {
		pthread_mutex_unlock(&screenlock);
		pthread_mutex_unlock(&vstatlock);
		return(-1);
	}
	screen=newscreen;
	memset(screen,vstat.palette[0],screenwidth*screenheight);
	pthread_mutex_unlock(&screenlock);
	pthread_mutex_unlock(&vstatlock);
	current_font=current_secondary_font=default_font;
	bitmap_loadfont(NULL);

	cio_textinfo.attribute=7;
	cio_textinfo.normattr=7;
	cio_textinfo.currmode=mode;
	cio_textinfo.screenheight=vstat.rows;
	cio_textinfo.screenwidth=vstat.cols;
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
void send_rectangle(int xoffset, int yoffset, int width, int height, int force)
{
	unsigned char *rect;
	int pixel=0;
	int inpixel;
	int x,y;

	if(!bitmap_initialized)
		return;
	pthread_mutex_lock(&screenlock);
	if(callbacks.drawrect) {
		if(xoffset < 0 || xoffset >= screenwidth || yoffset < 0 || yoffset >= screenheight || width <= 0 || width > screenwidth || height <=0 || height >screenheight)
			goto end;

		rect=(unsigned char *)malloc(width*height*sizeof(unsigned char));
		if(!rect)
			goto end;

		for(y=0; y<height; y++) {
			inpixel=PIXEL_OFFSET(xoffset, yoffset+y);
			for(x=0; x<width; x++)
				rect[pixel++]=vstat.palette[screen[inpixel++]];
		}
		callbacks.drawrect(xoffset,yoffset,width,height,rect);
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

	if(vstat.bright_background)
		flags |= CIOLIB_VIDEO_BGBRIGHT;
	if(vstat.no_bright)
		flags |= CIOLIB_VIDEO_NOBRIGHT;
	if(vstat.bright_altcharset)
		flags |= CIOLIB_VIDEO_ALTCHARS;
	return(flags);
}

void bitmap_setvideoflags(int flags)
{
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
}

int bitmap_movetext(int x, int y, int ex, int ey, int tox, int toy)
{
	int	direction=1;
	int	cy;
	int	sy;
	int	destoffset;
	int	sourcepos;
	int width=ex-x+1;
	int height=ey-y+1;

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

	pthread_mutex_lock(&vstatlock);
	for(cy=(direction==-1?(height-1):0); cy<height && cy>=0; cy+=direction) {
		damaged[toy+cy-1]=1;
		sourcepos=((y-1)+cy)*cio_textinfo.screenwidth+(x-1);
		memmove(&(vstat.vmem[sourcepos+destoffset]), &(vstat.vmem[sourcepos]), sizeof(vstat.vmem[0])*width);
	}
	pthread_mutex_unlock(&vstatlock);
	return(1);
}

void bitmap_clreol(void)
{
	int pos,x;
	WORD fill=(cio_textinfo.attribute<<8)|space;

	pos=(cio_textinfo.cury+cio_textinfo.wintop-2)*cio_textinfo.screenwidth;
	pthread_mutex_lock(&vstatlock);
	damaged[cio_textinfo.cury-1]=1;
	for(x=cio_textinfo.curx+cio_textinfo.winleft-2; x<cio_textinfo.winright; x++)
		vstat.vmem[pos+x]=fill;
	pthread_mutex_unlock(&vstatlock);
}

void bitmap_clrscr(void)
{
	int x,y;
	WORD fill=(cio_textinfo.attribute<<8)|space;

	pthread_mutex_lock(&vstatlock);
	for(y=cio_textinfo.wintop-1; y<cio_textinfo.winbottom; y++) {
		damaged[y]=1;
		for(x=cio_textinfo.winleft-1; x<cio_textinfo.winright; x++)
			vstat.vmem[y*cio_textinfo.screenwidth+x]=fill;
	}
	pthread_mutex_unlock(&vstatlock);
}

int bitmap_puttext(int sx, int sy, int ex, int ey, void *fill)
{
	int x,y;
	unsigned char *out;
	WORD	sch;

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
	out=fill;
	for(y=sy-1;y<ey;y++) {
		damaged[y]=1;
		for(x=sx-1;x<ex;x++) {
			sch=*(out++);
			sch |= (*(out++))<<8;
			vstat.vmem[y*cio_textinfo.screenwidth+x]=sch;
		}
	}
	pthread_mutex_unlock(&vstatlock);
	return(1);
}

/* Called from main thread only */
int bitmap_gettext(int sx, int sy, int ex, int ey, void *fill)
{
	int x,y;
	unsigned char *out;
	WORD	sch;

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

	pthread_mutex_lock(&vstatlock);
	out=fill;
	for(y=sy-1;y<ey;y++) {
		for(x=sx-1;x<ex;x++) {
			sch=vstat.vmem[y*cio_textinfo.screenwidth+x];
			*(out++)=sch & 0xff;
			*(out++)=sch >> 8;
		}
	}
	pthread_mutex_unlock(&vstatlock);
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

	if(!bitmap_initialized)
		return(-1);
	if(font < 0 || font>(sizeof(conio_fontdata)/sizeof(struct conio_font_data_struct)-2))
		return(-1);

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
				if(force)
					goto error_return;
				else
					changemode=1;
			}
			break;
		case 14:
			if(conio_fontdata[font].eight_by_fourteen==NULL) {
				if(force)
					goto error_return;
				else
					changemode=1;
			}
			break;
		case 16:
			if(conio_fontdata[font].eight_by_sixteen==NULL) {
				if(force)
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
			current_font=font;
			if(font==36 /* ATARI */)
				space=0;
			else
				space=' ';
			break;
		case 2:
			current_secondary_font=font;
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
	return(0);

error_return:
	pthread_mutex_unlock(&vstatlock);
	return(-1);
}

int bitmap_getfont(void)
{
	return(current_font);
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
	if(current_font==-99 || current_font>(sizeof(conio_fontdata)/sizeof(struct conio_font_data_struct)-2)) {
		for(i=0; conio_fontdata[i].desc != NULL; i++) {
			if(!strcmp(conio_fontdata[i].desc, "Codepage 437 English")) {
				current_font=i;
				break;
			}
		}
		if(conio_fontdata[i].desc==NULL)
			current_font=0;
	}
	if(current_font==-1)
		filename=current_filename;
	else if(conio_fontdata[current_font].desc==NULL)
		return(-1);

	if(current_secondary_font==-99)
		current_secondary_font=current_font;
	if(current_secondary_font==-1)
		;
	else if(conio_fontdata[current_secondary_font].desc==NULL)
		current_secondary_font=current_font;

	pthread_mutex_lock(&vstatlock);
	fh=vstat.charheight;
	fw=vstat.charwidth/8+(vstat.charwidth%8?1:0);

	fontsize=fw*fh*256*sizeof(unsigned char);

	if(font)
		FREE_AND_NULL(font);
	if(secondary_font)
		FREE_AND_NULL(secondary_font);
	if((font=(unsigned char *)malloc(fontsize))==NULL)
		goto error_return;
	if((secondary_font=(unsigned char *)malloc(fontsize))==NULL)
		goto error_return;

	if(filename != NULL) {
		if(flength(filename)!=fontsize)
			goto error_return;
		if((fontfile=fopen(filename,"rb"))==NULL)
			goto error_return;
		if(fread(font, 1, fontsize, fontfile)!=fontsize)
			goto error_return;
		fclose(fontfile);
		fontfile=NULL;
		current_font=-1;
		if(filename != current_filename)
			SAFECOPY(current_filename,filename);
		if(current_secondary_font==-1)
			memcpy(secondary_font, font, fontsize);
	}
	if(current_font != -1 || current_secondary_font != -1) {
		switch(vstat.charwidth) {
			case 8:
				switch(vstat.charheight) {
					case 8:
						if(current_font != -1) {
							if(conio_fontdata[current_font].eight_by_eight==NULL)
								goto error_return;
							memcpy(font, conio_fontdata[current_font].eight_by_eight, fontsize);
						}
						if(current_secondary_font != -1) {
							if(conio_fontdata[current_secondary_font].eight_by_eight==NULL) {
								FREE_AND_NULL(secondary_font);
							}
							else
								memcpy(secondary_font, conio_fontdata[current_secondary_font].eight_by_eight, fontsize);
						}
						break;
					case 14:
						if(current_font != -1) {
							if(conio_fontdata[current_font].eight_by_fourteen==NULL)
								goto error_return;
							memcpy(font, conio_fontdata[current_font].eight_by_fourteen, fontsize);
						}
						if(current_secondary_font != -1) {
							if(conio_fontdata[current_secondary_font].eight_by_fourteen==NULL) {
								FREE_AND_NULL(secondary_font);
							}
							else
								memcpy(secondary_font, conio_fontdata[current_secondary_font].eight_by_fourteen, fontsize);
						}
						break;
					case 16:
						if(current_font != -1) {
							if(conio_fontdata[current_font].eight_by_sixteen==NULL)
								goto error_return;
							memcpy(font, conio_fontdata[current_font].eight_by_sixteen, fontsize);
						}
						if(current_secondary_font != -1) {
							if(conio_fontdata[current_secondary_font].eight_by_sixteen==NULL) {
								FREE_AND_NULL(secondary_font);
							}
							else
								memcpy(secondary_font, conio_fontdata[current_secondary_font].eight_by_sixteen, fontsize);
						}
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
	FREE_AND_NULL(font);
	FREE_AND_NULL(secondary_font);
	if(fontfile)
		fclose(fontfile);
	pthread_mutex_unlock(&vstatlock);
	return(-1);
}

/* vstatlock is held */
static void bitmap_draw_cursor()
{
	int x;
	int y;
	char attr;
	int pixel;
	int xoffset,yoffset;
	int width;

	if(!bitmap_initialized)
		return;
	if(vstat.curs_visible) {
		if(vstat.blink || (!vstat.curs_blink)) {
			if(vstat.curs_start<=vstat.curs_end) {
				xoffset=(vstat.curs_col-1)*vstat.charwidth;
				yoffset=(vstat.curs_row-1)*vstat.charheight;
				if(xoffset < 0 || yoffset < 0)
					return;
				attr=cio_textinfo.attribute&0x0f;
				width=vstat.charwidth;
	
				pthread_mutex_lock(&screenlock);
				for(y=vstat.curs_start; y<=vstat.curs_end; y++) {
					if(xoffset < screenwidth && (yoffset+y) < screenheight) {
						pixel=PIXEL_OFFSET(xoffset, yoffset+y);
						for(x=0;x<vstat.charwidth;x++)
							screen[pixel++]=attr;
						//memset(screen+pixel,attr,width);
					}
				}
				pthread_mutex_unlock(&screenlock);
				send_rectangle(xoffset, yoffset+vstat.curs_start, vstat.charwidth, vstat.curs_end-vstat.curs_start+1,FALSE);
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

/* vstatlock is held */
static int bitmap_draw_one_char(unsigned int xpos, unsigned int ypos)
{
	int		fg;
	int		bg;
	int		xoffset=(xpos-1)*vstat.charwidth;
	int		yoffset=(ypos-1)*vstat.charheight;
	int		x;
	int		y;
	int		fontoffset;
	unsigned char *this_font;
	WORD	sch;

	if(!bitmap_initialized)
		return(-1);

	if(!screen)
		return(-1);

	if(!vstat.vmem)
		return(-1);

	if(!font)
		return(-1);

	sch=vstat.vmem[(ypos-1)*cio_textinfo.screenwidth+(xpos-1)];

	if(vstat.bright_background) {
		bg=(sch&0xf000)>>12;
		fg=(sch&0x0f00)>>8;
	}
	else {
		bg=(sch&0x7000)>>12;
		if(sch&0x8000 && vstat.blink)
			fg=bg;
		else
			fg=(sch&0x0f00)>>8;
	}
	this_font=font;
	if(vstat.bright_altcharset) {
		if(fg & 0x08) {
			this_font=secondary_font;
			if(this_font==NULL)
				this_font=font;
		}
	}
	if(vstat.no_bright)
		fg &= 0x07;
	fontoffset=(sch&0xff)*vstat.charheight;

	pthread_mutex_lock(&screenlock);
	for(y=0; y<vstat.charheight; y++) {
		memset(&screen[PIXEL_OFFSET(xoffset, yoffset+y)],bg,vstat.charwidth);
		for(x=0; x<vstat.charwidth; x++) {
			if(this_font[fontoffset] & (0x80 >> x))
				screen[PIXEL_OFFSET(xoffset+x, yoffset+y)]=fg;
		}
		fontoffset++;
	}
	pthread_mutex_unlock(&screenlock);

	return(0);
}

/* vstatlock is held */
static int update_rect(int sx, int sy, int width, int height, int force)
{
	int x,y;
	unsigned int pos;
	int	redraw_cursor=0;
	int	lastcharupdated=0;
	int fullredraw=0;
	static unsigned short *last_vmem=NULL;
	static struct video_stats vs;
	struct rectangle this_rect;
	int this_rect_used=0;
	struct rectangle last_rect;
	int last_rect_used=0;

	if(!bitmap_initialized)
		return(-1);

	if(sx==0 && sy==0 && width==0 && height==0)
		fullredraw=1;

	if(sx<=0)
		sx=1;
	if(sy<=0)
		sy=1;
	if(width<=0 || width>cio_textinfo.screenwidth)
		width=cio_textinfo.screenwidth;
	if(height<=0 || height>cio_textinfo.screenheight)
		height=cio_textinfo.screenheight;

	if(vs.cols!=vstat.cols || vs.rows != vstat.rows || last_vmem==NULL) {
		unsigned short *p;

		p=(unsigned short *)realloc(last_vmem, vstat.cols*vstat.rows*sizeof(unsigned short));
		if(p==NULL)
			return(-1);
		last_vmem=p;
		memset(last_vmem, 255, vstat.cols*vstat.rows*sizeof(unsigned short));
		sx=1;
		sy=1;
		width=vstat.cols;
		height=vstat.rows;
		force=1;
		vs.cols=vstat.cols;
		vs.rows=vstat.rows;
	}

	/* Redraw cursor */
	if(vstat.blink != vs.blink
			|| vstat.curs_col!=vs.curs_col
			|| vstat.curs_row!=vs.curs_row
			|| vstat.curs_start!=vs.curs_start
			|| vstat.curs_end!=vs.curs_end)
		redraw_cursor=1;

	for(y=0;y<height;y++) {
		if(force 
				|| (vstat.blink != vs.blink )
				|| (redraw_cursor && (vs.curs_row==sy+y || vstat.curs_row==sy+y))
				|| damaged[sy+y-1]) {
			damaged[sy+y-1]=0;
			pos=(sy+y-1)*vstat.cols+(sx-1);
			for(x=0;x<width;x++) {
				if(force
						|| (last_vmem[pos] != vstat.vmem[pos]) 					/* Different char */
						|| (vstat.blink != vs.blink && vstat.vmem[pos]>>15) 	/* Blinking char */
						|| (redraw_cursor && ((vs.curs_col==sx+x && vs.curs_row==sy+y) || (vstat.curs_col==sx+x && vstat.curs_row==sy+y)))	/* Cursor */
						) {
					last_vmem[pos] = vstat.vmem[pos];
					bitmap_draw_one_char(sx+x,sy+y);

					if(!redraw_cursor && sx+x==vstat.curs_col && sy+y==vstat.curs_row)
						redraw_cursor=1;

					if(lastcharupdated) {
						this_rect.width+=vstat.charwidth;
						lastcharupdated++;
					}
					else {
						if(this_rect_used) {
							send_rectangle(this_rect.x, this_rect.y, this_rect.width, this_rect.height,FALSE);
						}
						this_rect.x=(sx+x-1)*vstat.charwidth;
						this_rect.y=(sy+y-1)*vstat.charheight;
						this_rect.width=vstat.charwidth;
						this_rect.height=vstat.charheight;
						this_rect_used=1;
						lastcharupdated++;
					}
				}
				else {
					if(this_rect_used) {
						send_rectangle(this_rect.x, this_rect.y, this_rect.width, this_rect.height,FALSE);
						this_rect_used=0;
					}
					if(last_rect_used) {
						send_rectangle(last_rect.x, last_rect.y, last_rect.width, last_rect.height, FALSE);
						last_rect_used=0;
					}
					lastcharupdated=0;
				}
				pos++;
			}
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
				send_rectangle(last_rect.x, last_rect.y, last_rect.width, last_rect.height, FALSE);
				last_rect_used=0;
			}
			if(this_rect_used) {
				send_rectangle(this_rect.x, this_rect.y, this_rect.width, this_rect.height, FALSE);
				this_rect_used=0;
			}
		}
		lastcharupdated=0;
	}

	if(this_rect_used)
		send_rectangle(this_rect.x, this_rect.y, this_rect.width, this_rect.height, FALSE);
	if(last_rect_used)
		send_rectangle(last_rect.x, last_rect.y, last_rect.width, last_rect.height, FALSE);

	/* Did we redraw the cursor?  If so, update cursor info */
	vs=vstat;

	if(redraw_cursor)
		bitmap_draw_cursor();

	return(0);
}
