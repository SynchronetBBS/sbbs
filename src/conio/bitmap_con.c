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

static char *screen;
int screenwidth;
int screenheight;
#define PIXEL_OFFSET(x,y)	( (y)*screenwidth+(x) )

static int current_font=-99;
struct video_stats vstat;

struct bitmap_callbacks {
	void	(*drawrect)		(int xpos, int ypos, int width, int height, unsigned char *data);
	void	(*flush)		(void);
};

pthread_mutex_t		vstatlock;
pthread_mutex_t		screenlock;
static struct bitmap_callbacks callbacks;
static unsigned char *font;

struct rectangle {
	int x;
	int y;
	int width;
	int height;
};

static int update_rect(int sx, int sy, int width, int height, int force, int calls_send);

/* Blinker Thread */
static void blinker_thread(void *data)
{
	int count=0;

	while(1) {
		SLEEP(10);
		count++;
		pthread_mutex_lock(&vstatlock);
		if(count==50) {
			if(vstat.blink)
				vstat.blink=FALSE;
			else
				vstat.blink=TRUE;
			count=0;
		}
		update_rect(0,0,0,0,FALSE,TRUE);
		pthread_mutex_unlock(&vstatlock);
		callbacks.flush();
	}
}

int bitmap_init(void (*drawrect_cb) (int xpos, int ypos, int width, int height, unsigned char *data)
				,void (*flush_cb) (void))
{
	pthread_mutex_init(&vstatlock, NULL);
	pthread_mutex_init(&screenlock, NULL);
	pthread_mutex_lock(&vstatlock);
	vstat.vmem=NULL;
	pthread_mutex_unlock(&vstatlock);

	callbacks.drawrect=drawrect_cb;
	callbacks.flush=flush_cb;
	_beginthread(blinker_thread,0,NULL);

	return(0);
}

void send_rectangle(int xoffset, int yoffset, int width, int height, int force)
{
	unsigned char *rect;
	int pixel=0;
	int inpixel;
	int x,y;

	pthread_mutex_lock(&screenlock);
	if(callbacks.drawrect) {
#ifdef PARANOIA
		if(xoffset < 0 || xoffset >= screenwidth || yoffset < 0 || yoffset >= screenheight || width <= 0 || width > screenwidth || height <=0 || height >screenheight) {
			pthread_mutex_unlock(&screenlock);
			return;
		}
#endif

		rect=(unsigned char *)malloc(width*height*sizeof(unsigned char));
		if(!rect) {
			pthread_mutex_unlock(&screenlock);
			return;
		}

		for(y=0; y<height; y++) {
			inpixel=PIXEL_OFFSET(xoffset, yoffset+y);
			for(x=0; x<width; x++) {
				rect[pixel++]=vstat.palette[screen[inpixel++]];
			}
		}
		callbacks.drawrect(xoffset,yoffset,width,height,rect);
	}
	pthread_mutex_unlock(&screenlock);
}

int bitmap_init_mode(int mode, int *width, int *height)
{
    int i;
	char *newscreen;

	pthread_mutex_lock(&vstatlock);

	if(load_vmode(&vstat, mode)) {
		pthread_mutex_unlock(&vstatlock);
		return(-1);
	}

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
	/* TODO: Re-enable this
	send_rectangle(0,0,screenwidth,screenheight,TRUE);
	*/
	pthread_mutex_unlock(&vstatlock);
	bitmap_loadfont(NULL);
	/* TODO: Remove this next line */
	pthread_mutex_lock(&vstatlock);
	update_rect(1,1,cio_textinfo.screenwidth,cio_textinfo.screenheight,TRUE,TRUE);
	pthread_mutex_unlock(&vstatlock);

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

/********************************************************/
/* High Level Stuff										*/
/********************************************************/

/* Called from main thread only (Passes Event) */
int bitmap_puttext(int sx, int sy, int ex, int ey, void *fill)
{
	int x,y;
	unsigned char *out;
	WORD	sch;

	pthread_mutex_lock(&vstatlock);
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
		pthread_mutex_unlock(&vstatlock);
		return(0);
	}

	out=fill;
	for(y=sy-1;y<ey;y++) {
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

	pthread_mutex_lock(&vstatlock);

	if(		   sx < 1
			|| sy < 1
			|| ex < 1
			|| ey < 1
			|| sx > ex
			|| sy > ey
			|| ex > cio_textinfo.screenwidth
			|| ey > cio_textinfo.screenheight
			|| fill==NULL) {
		pthread_mutex_unlock(&vstatlock);
		return(0);
	}

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

/* Called from main thread only */
void bitmap_setcursortype(int type)
{
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
	update_rect(cio_textinfo.curx+cio_textinfo.winleft-1,cio_textinfo.cury+cio_textinfo.wintop-1,1,1,TRUE,TRUE);
	pthread_mutex_unlock(&vstatlock);
}

int bitmap_setfont(int font, int force)
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

	if(font < 0 || font>(sizeof(conio_fontdata)/sizeof(struct conio_font_data_struct)-2))
		return(-1);

	if(conio_fontdata[font].eight_by_sixteen!=NULL)
		newmode=C80;
	else if(conio_fontdata[font].eight_by_fourteen!=NULL)
		newmode=C80X28;
	else if(conio_fontdata[font].eight_by_eight!=NULL)
		newmode=C80X50;

	switch(vstat.charheight) {
		case 8:
			if(conio_fontdata[font].eight_by_eight==NULL) {
				if(force)
					return(-1);
				else
					changemode=1;
			}
			break;
		case 14:
			if(conio_fontdata[font].eight_by_fourteen==NULL) {
				if(force)
					return(-1);
				else
					changemode=1;
			}
			break;
		case 16:
			if(conio_fontdata[font].eight_by_sixteen==NULL) {
				if(force)
					return(-1);
				else
					changemode=1;
			}
			break;
	}
	if(changemode && newmode==-1)
		return(-1);
	current_font=font;
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
							*(new++)=' ';
							*(new++)=attr;
						}
					}
					else {
						*(new++)=' ';
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
	int	ch;
	int x;
	int y;
	int charrow;
	int charcol;
	FILE	*fontfile;

	if(current_font==-99 || current_font>(sizeof(conio_fontdata)/sizeof(struct conio_font_data_struct)-2)) {
		for(x=0; conio_fontdata[x].desc != NULL; x++) {
			if(!strcmp(conio_fontdata[x].desc, "Codepage 437 English")) {
				current_font=x;
				break;
			}
		}
		if(conio_fontdata[x].desc==NULL)
			current_font=0;
	}
	if(current_font==-1)
		filename=current_filename;
	else if(conio_fontdata[current_font].desc==NULL)
		return(-1);

	pthread_mutex_lock(&vstatlock);
	fh=vstat.charheight;
	fw=vstat.charwidth/8+(vstat.charwidth%8?1:0);

	fontsize=fw*fh*256*sizeof(unsigned char);

	if(font)
		free(font);
	if((font=(unsigned char *)malloc(fontsize))==NULL) {
		pthread_mutex_unlock(&vstatlock);
		return(-1);
	}

	if(filename != NULL) {
		if(flength(filename)!=fontsize) {
			pthread_mutex_unlock(&vstatlock);
			free(font);
			return(-1);
		}
		if((fontfile=fopen(filename,"rb"))==NULL) {
			pthread_mutex_unlock(&vstatlock);
			free(font);
			return(-1);
		}
		if(fread(font, 1, fontsize, fontfile)!=fontsize) {
			pthread_mutex_unlock(&vstatlock);
			free(font);
			fclose(fontfile);
			return(-1);
		}
		fclose(fontfile);
		current_font=-1;
		if(filename != current_filename)
			SAFECOPY(current_filename,filename);
	}
	else {
		switch(vstat.charwidth) {
			case 8:
				switch(vstat.charheight) {
					case 8:
						if(conio_fontdata[current_font].eight_by_eight==NULL) {
							pthread_mutex_unlock(&vstatlock);
							free(font);
							return(-1);
						}
						memcpy(font, conio_fontdata[current_font].eight_by_eight, fontsize);
						break;
					case 14:
						if(conio_fontdata[current_font].eight_by_fourteen==NULL) {
							pthread_mutex_unlock(&vstatlock);
							free(font);
							return(-1);
						}
						memcpy(font, conio_fontdata[current_font].eight_by_fourteen, fontsize);
						break;
					case 16:
						if(conio_fontdata[current_font].eight_by_sixteen==NULL) {
							pthread_mutex_unlock(&vstatlock);
							free(font);
							return(-1);
						}
						memcpy(font, conio_fontdata[current_font].eight_by_sixteen, fontsize);
						break;
					default:
						pthread_mutex_unlock(&vstatlock);
						free(font);
						return(-1);
				}
				break;
			default:
				pthread_mutex_unlock(&vstatlock);
				free(font);
				return(-1);
		}
	}

	pthread_mutex_unlock(&vstatlock);
    return(0);
}

/* Called from events thread only */
static void bitmap_draw_cursor(int flush)
{
	int x;
	int y;
	int attr;
	int pixel;
	int xoffset,yoffset;
	int start,end;
	int width;

	if(vstat.blink && !hold_update) {
		if(vstat.curs_start<=vstat.curs_end) {
			xoffset=(cio_textinfo.curx+cio_textinfo.winleft-2)*vstat.charwidth;
			yoffset=(cio_textinfo.cury+cio_textinfo.wintop-2)*vstat.charheight;
			attr=cio_textinfo.attribute&0x0f;
			start=vstat.curs_start;
			end=vstat.curs_end;
			width=vstat.charwidth;

			pthread_mutex_lock(&screenlock);
			for(y=start; y<=end; y++) {
				pixel=PIXEL_OFFSET(xoffset, yoffset+y);
				for(x=0; x<width; x++)
					screen[pixel++]=attr;
			}
			pthread_mutex_unlock(&screenlock);
			send_rectangle(xoffset, yoffset+vstat.curs_start, vstat.charwidth, vstat.curs_end-vstat.curs_start+1,FALSE);
//			if(flush && callbacks.flush)
//				callbacks.flush();
		}
	}
}

/* Called from main thread only */
void bitmap_gotoxy(int x, int y)
{
	static int lx=-1,ly=-1;

	pthread_mutex_lock(&vstatlock);
	if((x != cio_textinfo.curx) || (y != cio_textinfo.cury)) {
		vstat.curs_col=x+cio_textinfo.winleft-1;
		vstat.curs_row=y+cio_textinfo.wintop-1;
		cio_textinfo.curx=x;
		cio_textinfo.cury=y;
	}
	if(!hold_update) {
		/* Erase old cursor */
		if(lx != vstat.curs_col || ly != vstat.curs_row)
			update_rect(lx,ly,1,1,TRUE,TRUE);
		/* Draw new cursor */
		bitmap_draw_cursor(TRUE);
		lx=vstat.curs_col;
		ly=vstat.curs_row;
	}
	pthread_mutex_unlock(&vstatlock);
}

static int bitmap_draw_one_char(unsigned int xpos, unsigned int ypos)
{
	int		fg;
	int		bg;
	int		xoffset=(xpos-1)*vstat.charwidth;
	int		yoffset=(ypos-1)*vstat.charheight;
	int		x;
	int		y;
	int		fontoffset;
	WORD	sch;

	if(!screen)
		return(-1);

	if(!vstat.vmem)
		return(-1);

	if(!font)
		return(-1);

	sch=vstat.vmem[(ypos-1)*cio_textinfo.screenwidth+(xpos-1)];
	bg=(sch&0x7000)>>12;
	if(sch&0x8000 && vstat.blink)
		fg=bg;
	else
		fg=(sch&0x0f00)>>8;
	fontoffset=(sch&0xff)*vstat.charheight;

	pthread_mutex_lock(&screenlock);
	for(y=0; y<vstat.charheight; y++) {
		for(x=0; x<vstat.charwidth; x++) {
			if(font[fontoffset] & (0x80 >> x))
				screen[PIXEL_OFFSET(xoffset+x, yoffset+y)]=fg;
			else
				screen[PIXEL_OFFSET(xoffset+x, yoffset+y)]=bg;
		}
		fontoffset++;
	}
	pthread_mutex_unlock(&screenlock);

	return(0);
}

static int update_rect(int sx, int sy, int width, int height, int force, int calls_send)
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
	int	sent=FALSE;

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

	/* Redraw all chars */
	if(vstat.blink != vs.blink
			|| vstat.curs_col!=vs.curs_col
			|| vstat.curs_row!=vs.curs_row)
		redraw_cursor=1;

	for(y=0;y<height;y++) {
		pos=(sy+y-1)*vstat.cols+(sx-1);
		for(x=0;x<width;x++) {
			if(force
					|| (last_vmem[pos] != vstat.vmem[pos]) 					/* Different char */
					|| (vstat.blink != vs.blink && vstat.vmem[pos]>>15) 	/* Blinking char */
					|| (redraw_cursor && ((vs.curs_col==sx+x && vs.curs_row==sy+y) || (vstat.curs_col==sx+x && vstat.curs_row==sy+y)))	/* Cursor */
					) {
				last_vmem[pos] = vstat.vmem[pos];
				bitmap_draw_one_char(sx+x,sy+y);
				sent=TRUE;

				if(calls_send) {
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
				if(!redraw_cursor && sx+x==vstat.curs_col && sy+y==vstat.curs_row)
					redraw_cursor=1;
			}
			else {
				if(calls_send) {
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
			}
			pos++;
		}
		if(calls_send) {
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
	}

	if(calls_send) {
		if(this_rect_used) {
			send_rectangle(this_rect.x, this_rect.y, this_rect.width, this_rect.height, FALSE);
		}
		if(last_rect_used) {
			send_rectangle(last_rect.x, last_rect.y, last_rect.width, last_rect.height, FALSE);
		}
	}

	/* Did we redraw the cursor?  If so, update position */
	if(redraw_cursor) {
		vs.curs_col=vstat.curs_col;
		vs.curs_row=vstat.curs_row;
	}

	/* On full redraws, save the last blink value */
	if(fullredraw) {
		vs.blink=vstat.blink;
	}

	if(redraw_cursor)
		bitmap_draw_cursor(FALSE);

	return(0);
}
