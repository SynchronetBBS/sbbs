#include <stdarg.h>
#include <stdio.h>		/* NULL */
#include <stdlib.h>
#include <string.h>

#include "gen_defs.h"
#include "genwrap.h"
#include "xpbeep.h"

#include "ciolib.h"
#include "keys.h"
#include "vidmodes.h"

#ifdef main
	#undef main
#endif
#include "SDL.h"
#include "SDL_thread.h"
extern int	CIOLIB_main(int argc, char **argv);

/********************************************************/
/* Low Level Stuff										*/
/* This should all be called from the same thread!		*/
/********************************************************/

SDL_Surface	*win=NULL;
SDL_mutex *sdl_keylock;
SDL_sem *sdl_key_pending;
SDL_sem *sdl_init_complete;
int	sdl_init_good=0;
int sdl_updated;
int sdl_exitcode=0;

SDL_Surface *sdl_font=NULL;
SDL_Surface	*sdl_cursor=NULL;

static int lastcursor_x=0;
static int lastcursor_y=0;

unsigned short *last_vmem=NULL;

struct video_stats vstat;
int fullscreen=0;

/* 256 bytes so I can cheat */
unsigned char		sdl_keybuf[256];		/* Keyboard buffer */
unsigned char		sdl_key=0;				/* Index into keybuf for next key in buffer */
unsigned char		sdl_keynext=0;			/* Index into keybuf for next free position */

struct sdl_keyvals {
	int	keysym
		,key
		,shift
		,ctrl
		,alt;
};

struct sdl_drawchar {
	int  x
		,y
		,ch
		,fg
		,bg
		,blink;
};

enum {
	 SDL_USEREVENT_UPDATERECT
	,SDL_USEREVENT_SETTITLE
	,SDL_USEREVENT_SETVIDMODE
	,SDL_USEREVENT_SHOWMOUSE
	,SDL_USEREVENT_HIDEMOUSE
	,SDL_USEREVENT_INIT
};

const struct sdl_keyvals sdl_keyval[] =
{
	{SDLK_BACKSPACE, 0x08, 0x08, 0x7f, 0x0e00},
	{SDLK_TAB, 0x09, 0x0f00, 0x9400, 0xa500},
	{SDLK_RETURN, 0x0d, 0x0d, 0x0a, 0xa600},
	{SDLK_ESCAPE, 0x1b, 0x1b, 0x1b, 0x0100},
	{SDLK_SPACE, 0x20, 0x20, 0x0300, 0x20,},
	{SDLK_0, '0', ')', 0, 0x8100},
	{SDLK_1, '1', '!', 0, 0x7800},
	{SDLK_2, '2', '@', 0x0300, 0x7900},
	{SDLK_3, '3', '#', 0, 0x7a00},
	{SDLK_4, '4', '$', 0, 0x7b00},
	{SDLK_5, '5', '%', 0, 0x7c00},
	{SDLK_6, '6', '^', 0x1e, 0x7d00},
	{SDLK_7, '7', '&', 0, 0x7e00},
	{SDLK_8, '8', '*', 0, 0x7f00},
	{SDLK_9, '9', '(', 0, 0x8000},
	{SDLK_a, 'a', 'A', 0x01, 0x1e00},
	{SDLK_b, 'b', 'B', 0x02, 0x3000},
	{SDLK_c, 'c', 'C', 0x03, 0x2e00},
	{SDLK_d, 'd', 'D', 0x04, 0x2000},
	{SDLK_e, 'e', 'E', 0x05, 0x1200},
	{SDLK_f, 'f', 'F', 0x06, 0x2100},
	{SDLK_g, 'g', 'G', 0x07, 0x2200},
	{SDLK_h, 'h', 'H', 0x08, 0x2300},
	{SDLK_i, 'i', 'I', 0x09, 0x1700},
	{SDLK_j, 'j', 'J', 0x0a, 0x2400},
	{SDLK_k, 'k', 'K', 0x0b, 0x2500},
	{SDLK_l, 'l', 'L', 0x0c, 0x2600},
	{SDLK_m, 'm', 'M', 0x0d, 0x3200},
	{SDLK_n, 'n', 'N', 0x0e, 0x3100},
	{SDLK_o, 'o', 'O', 0x0f, 0x1800},
	{SDLK_p, 'p', 'P', 0x10, 0x1900},
	{SDLK_q, 'q', 'Q', 0x11, 0x1000},
	{SDLK_r, 'r', 'R', 0x12, 0x1300},
	{SDLK_s, 's', 'S', 0x13, 0x1f00},
	{SDLK_t, 't', 'T', 0x14, 0x1400},
	{SDLK_u, 'u', 'U', 0x15, 0x1600},
	{SDLK_v, 'v', 'V', 0x16, 0x2f00},
	{SDLK_w, 'w', 'W', 0x17, 0x1100},
	{SDLK_x, 'x', 'X', 0x18, 0x2d00},
	{SDLK_y, 'y', 'Y', 0x19, 0x1500},
	{SDLK_z, 'z', 'Z', 0x1a, 0x2c00},
	{SDLK_PAGEUP, 0x4900, 0x4900, 0x8400, 0x9900},
	{SDLK_PAGEDOWN, 0x5100, 0x5100, 0x7600, 0xa100},
	{SDLK_END, 0x4f00, 0x4f00, 0x7500, 0x9f00},
	{SDLK_HOME, 0x4700, 0x4700, 0x7700, 0x9700},
	{SDLK_LEFT, 0x4b00, 0x4b00, 0x7300, 0x9b00},
	{SDLK_UP, 0x4800, 0x4800, 0x8d00, 0x9800},
	{SDLK_RIGHT, 0x4d00, 0x4d00, 0x7400, 0x9d00},
	{SDLK_DOWN, 0x5000, 0x5000, 0x9100, 0xa000},
	{SDLK_INSERT, 0x5200, 0x5200, 0x9200, 0xa200},
	{SDLK_DELETE, 0x5300, 0x5300, 0x9300, 0xa300},
	{SDLK_KP0, 0x5200, 0x5200, 0x9200, 0},
	{SDLK_KP1, 0x4f00, 0x4f00, 0x7500, 0},
	{SDLK_KP2, 0x5000, 0x5000, 0x9100, 0},
	{SDLK_KP3, 0x5100, 0x5100, 0x7600, 0},
	{SDLK_KP4, 0x4b00, 0x4b00, 0x7300, 0},
	{SDLK_KP5, 0x4c00, 0x4c00, 0x8f00, 0},
	{SDLK_KP6, 0x4d00, 0x4d00, 0x7400, 0},
	{SDLK_KP7, 0x4700, 0x4700, 0x7700, 0},
	{SDLK_KP8, 0x4800, 0x4800, 0x8d00, 0},
	{SDLK_KP9, 0x4900, 0x4900, 0x8400, 0},
	{SDLK_KP_MULTIPLY, '*', '*', 0x9600, 0x3700},
	{SDLK_KP_PLUS, '+', '+', 0x9000, 0x4e00},
	{SDLK_KP_MINUS, '-', '-', 0x8e00, 0x4a00},
	{SDLK_KP_PERIOD, '.', '.', 0x5300, 0x9300},
	{SDLK_KP_DIVIDE, '/', '/', 0x9500, 0xa400},
	{SDLK_F1, 0x3b00, 0x5400, 0x5e00, 0x6800},
	{SDLK_F2, 0x3c00, 0x5500, 0x5f00, 0x6900},
	{SDLK_F3, 0x3d00, 0x5600, 0x6000, 0x6a00},
	{SDLK_F4, 0x3e00, 0x5700, 0x6100, 0x6b00},
	{SDLK_F5, 0x3f00, 0x5800, 0x6200, 0x6c00},
	{SDLK_F6, 0x4000, 0x5900, 0x6300, 0x6d00},
	{SDLK_F7, 0x4100, 0x5a00, 0x6400, 0x6e00},
	{SDLK_F8, 0x4200, 0x5b00, 0x6500, 0x6f00},
	{SDLK_F9, 0x4300, 0x5c00, 0x6600, 0x7000},
	{SDLK_F10, 0x4400, 0x5d00, 0x6700, 0x7100},
	{SDLK_F11, 0x8500, 0x8700, 0x8900, 0x8b00},
	{SDLK_F12, 0x8600, 0x8800, 0x8a00, 0x8c00},
	{SDLK_BACKSLASH, '\\', '|', 0x1c, 0x2b00},
	{SDLK_SLASH, '/', '?', 0, 0x3500},
	{SDLK_MINUS, '-', '_', 0x1f, 0x8200},
	{SDLK_EQUALS, '=', '+', 0, 0x8300},
	{SDLK_LEFTBRACKET, '[', '{', 0x1b, 0x1a00},
	{SDLK_RIGHTBRACKET, ']', '}', 0x1d, 0x1b00},
	{SDLK_SEMICOLON, ';', ':', 0, 0x2700},
	{SDLK_BACKSLASH, '\'', '"', 0, 0x2800},
	{SDLK_COMMA, ',', '<', 0, 0x3300},
	{SDLK_PERIOD, '.', '>', 0, 0x3400},
	{SDLK_BACKQUOTE, '`', '~', 0, 0x2900},
	{0, 0, 0, 0, 0}	/** END **/
};
const int sdl_tabs[10]={9,17,25,33,41,49,57,65,73,80};

/* Called from all threads */
void sdl_user_func(int func, ...)
{
	unsigned int	*i;
	va_list argptr;
	void	**args;
	SDL_Event	ev;

	ev.type=SDL_USEREVENT;
	ev.user.data1=NULL;
	ev.user.data2=NULL;
	ev.user.code=func;
	va_start(argptr, func);
	switch(func) {
		case SDL_USEREVENT_UPDATERECT:
			/* Only send event if the last event wasn't already handled */
			if(sdl_updated) {
				if(SDL_PeepEvents(&ev, 1, SDL_ADDEVENT, 0xffffffff)==1);
					sdl_updated=0;
			}
			break;
		case SDL_USEREVENT_SETTITLE:
			if((ev.user.data1=strdup(va_arg(argptr, char *)))==NULL) {
				va_end(argptr);
				return;
			}
			while(SDL_PeepEvents(&ev, 1, SDL_ADDEVENT, 0xffffffff)!=1);
			break;
		case SDL_USEREVENT_SETVIDMODE:
			if((ev.user.data1=(void *)malloc(sizeof(int)))==NULL) {
				va_end(argptr);
				return;
			}
			*((int *)ev.user.data1)=va_arg(argptr, int);
			if((ev.user.data2=(void *)malloc(sizeof(int)))==NULL) {
				free(ev.user.data1);
				va_end(argptr);
				return;
			}
			*((int *)ev.user.data2)=va_arg(argptr, int);
			while(SDL_PeepEvents(&ev, 1, SDL_ADDEVENT, 0xffffffff)!=1);
			break;
		case SDL_USEREVENT_SHOWMOUSE:
			while(SDL_PeepEvents(&ev, 1, SDL_ADDEVENT, 0xffffffff)!=1);
			break;
		case SDL_USEREVENT_HIDEMOUSE:
			while(SDL_PeepEvents(&ev, 1, SDL_ADDEVENT, 0xffffffff)!=1);
			break;
		case SDL_USEREVENT_INIT:
			while(SDL_PeepEvents(&ev, 1, SDL_ADDEVENT, 0xffffffff)!=1);
			break;
	}
	va_end(argptr);
}

/* Blinker Thread */
int sdl_blinker_thread(void *data)
{
	while(1) {
		SLEEP(500);
		if(vstat.blink)
			vstat.blink=FALSE;
		else
			vstat.blink=TRUE;
		sdl_user_func(SDL_USEREVENT_UPDATERECT,0,0,0,0);
	}
}

/* Called from main thread only (Passes Event) */
int sdl_init_mode(int mode)
{
    struct video_params vmode;
    int idx;			/* Index into vmode */
    int i;

	if(load_vmode(&vstat, mode))
		return(-1);

	sdl_user_func(SDL_USEREVENT_SETVIDMODE,vstat.charwidth*vstat.cols*vstat.scaling, vstat.charheight*vstat.rows*vstat.scaling);

	/* Initialize video memory with black background, white foreground */
	for (i = 0; i < vstat.cols*vstat.rows; ++i)
	    vstat.vmem[i] = 0x0700;
	vstat.currattr=7;

	sdl_user_func(SDL_USEREVENT_UPDATERECT,0,0,0,0);

	vstat.mode=mode;

    return(0);
}

/* Called from main thread only (Passes Event) */
int sdl_draw_char(unsigned short vch, int xpos, int ypos, int update)
{
	vstat.vmem[ypos*vstat.cols+xpos]=vch;

	if(update) {
		sdl_user_func(SDL_USEREVENT_UPDATERECT,xpos*vstat.charwidth*vstat.scaling,ypos*vstat.charheight*vstat.scaling,vstat.charwidth*vstat.scaling,vstat.charheight*vstat.scaling);
	}

	return(0);
}

/* Called from main thread only (Passes Event) */
int sdl_init(void)
{
	vstat.vmem=NULL;
	vstat.scaling=1;

	sdl_updated=1;

	sdl_init_mode(3);

	atexit(SDL_Quit);

	sdl_user_func(SDL_USEREVENT_INIT);

	SDL_SemWait(sdl_init_complete);
	if(sdl_init_good)
		return(0);

	return(-1);
}

/********************************************************/
/* High Level Stuff										*/
/********************************************************/

/* Called from main thread only (Passes Event) */
int sdl_puttext(int sx, int sy, int ex, int ey, void *fill)
{
	int x,y;
	unsigned char *out;
	WORD	sch;
	struct text_info	ti;

	gettextinfo(&ti);

	if(		   sx < 1
			|| sy < 1
			|| ex < 1
			|| ey < 1
			|| sx > ti.screenwidth
			|| sy > ti.screenheight
			|| sx > ex
			|| sy > ey
			|| ex > ti.screenwidth
			|| ey > ti.screenheight
			|| fill==NULL)
		return(0);

	out=fill;
	for(y=sy-1;y<ey;y++) {
		for(x=sx-1;x<ex;x++) {
			sch=*(out++);
			sch |= (*(out++))<<8;
			sdl_draw_char(sch,x,y,FALSE);
		}
	}
	sdl_user_func(SDL_USEREVENT_UPDATERECT
			,(sx-1)*vstat.charwidth*vstat.scaling
			,(sy-1)*vstat.charheight*vstat.scaling
			,(ex-sx+1)*vstat.charwidth*vstat.scaling
			,(ey-sy+1)*vstat.charheight*vstat.scaling
	);
	return(1);
}

/* Called from main thread only */
int sdl_gettext(int sx, int sy, int ex, int ey, void *fill)
{
	int x,y;
	unsigned char *out;
	WORD	sch;
	struct text_info	ti;

	gettextinfo(&ti);

	if(		   sx < 1
			|| sy < 1
			|| ex < 1
			|| ey < 1
			|| sx > ti.screenwidth
			|| sy > ti.screenheight
			|| sx > ex
			|| sy > ey
			|| ex > ti.screenwidth
			|| ey > ti.screenheight
			|| fill==NULL)
		return(0);

	out=fill;
	for(y=sy-1;y<ey;y++) {
		for(x=sx-1;x<ex;x++) {
			sch=vstat.vmem[y*vstat.cols+x];
			*(out++)=sch & 0xff;
			*(out++)=sch >> 8;
		}
	}
	return(1);
}

/* Called from main thread only */
void sdl_textattr(int attr)
{
	vstat.currattr=attr;
}

/* Called from main thread only */
int sdl_kbhit(void)
{
	int ret;

	SDL_mutexP(sdl_keylock);
	ret=(sdl_key!=sdl_keynext);
	SDL_mutexV(sdl_keylock);
	return(ret);
}

/* Called from main thread only */
void sdl_delay(long msec)
{
	SLEEP(msec);
}

/* Called from main thread only */
int sdl_wherey(void)
{
	return(vstat.curs_row+1);
}

/* Called from main thread only */
int sdl_wherex(void)
{
	return(vstat.curs_col+1);
}

/* Called from BOTH THREADS */
int sdl_beep(void)
{
	/* ToDo BEEP! */
	BEEP(440,100);
	return(0);
}

/* Put the character _c on the screen at the current cursor position. 
 * The special characters return, linefeed, bell, and backspace are handled
 * properly, as is line wrap and scrolling. The cursor position is updated. 
 */
/* Called from main thread only */
int sdl_putch(int ch)
{
	struct text_info ti;
	WORD sch;
	int i;

	sch=(vstat.currattr<<8)|ch;

	switch(ch) {
		case '\r':
			gettextinfo(&ti);
			vstat.curs_col=ti.winleft-1;
			break;
		case '\n':
			gettextinfo(&ti);
			if(wherey()==ti.winbottom-ti.wintop+1)
				wscroll();
			else
				vstat.curs_row++;
			break;
		case '\b':
			if(vstat.curs_col>0)
				vstat.curs_col--;
			sdl_draw_char((vstat.currattr<<8)|' ',vstat.curs_col,vstat.curs_row,TRUE);
			break;
		case 7:		/* Bell */
			sdl_beep();
			break;
		case '\t':
			for(i=0;i<10;i++) {
				if(sdl_tabs[i]>wherex()) {
					while(wherex()<sdl_tabs[i]) {
						putch(' ');
					}
					break;
				}
			}
			if(i==10) {
				putch('\r');
				putch('\n');
			}
			break;
		default:
			gettextinfo(&ti);
			if(wherey()==ti.winbottom-ti.wintop+1
					&& wherex()==ti.winright-ti.winleft+1) {
				sdl_draw_char(sch,vstat.curs_col,vstat.curs_row,TRUE);
				wscroll();
				gotoxy(ti.winleft,wherey());
			}
			else {
				if(wherex()==ti.winright-ti.winleft+1) {
					sdl_draw_char(sch,vstat.curs_col,vstat.curs_row,TRUE);
					gotoxy(ti.winleft,ti.cury+1);
				}
				else {
					sdl_draw_char(sch,vstat.curs_col,vstat.curs_row,TRUE);
					gotoxy(ti.curx+1,ti.cury);
				}
			}
			break;
	}

	return(ch);
}

/* Called from main thread only */
void sdl_gotoxy(int x, int y)
{
	vstat.curs_row=y-1;
	vstat.curs_col=x-1;
}

/* Called from main thread only */
void sdl_gettextinfo(struct text_info *info)
{
	info->currmode=vstat.mode;
	info->screenheight=vstat.rows;
	info->screenwidth=vstat.cols;
	info->curx=sdl_wherex();
	info->cury=sdl_wherey();
	info->attribute=vstat.currattr;
}

/* Called from main thread only */
void sdl_setcursortype(int type)
{
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
}

/* Called from main thread only */
int sdl_getch(void)
{
	int ch;

	SDL_SemWait(sdl_key_pending);
	SDL_mutexP(sdl_keylock);
	ch=sdl_keybuf[sdl_key++];
	SDL_mutexV(sdl_keylock);
	return(ch);
}

/* Called from main thread only */
int sdl_getche(void)
{
	int ch;

	while(1) {
		ch=sdl_getch();
		if(ch) {
			putch(ch);
			return(ch);
		}
		ch=sdl_getch();
	}
}

/* Called from main thread only */
void sdl_textmode(int mode)
{
	sdl_init_mode(mode);
}

/* Called from main thread only (Passes Event) */
int sdl_settitle(const char *title)
{
	sdl_user_func(SDL_USEREVENT_SETTITLE,title);
	return(0);
}

int sdl_showmouse(void)
{
	sdl_user_func(SDL_USEREVENT_SHOWMOUSE);
	return(1);
}

int sdl_hidemouse(void)
{
	sdl_user_func(SDL_USEREVENT_HIDEMOUSE);
	return(0);
}

#if 0		/* ToDo Copy/Paste */
void sdl_copytext(const char *text, size_t buflen)
{
	pthread_mutex_lock(&copybuf_mutex);
	if(copybuf!=NULL) {
		free(copybuf);
		copybuf=NULL;
	}

	copybuf=(char *)malloc(buflen+1);
	if(copybuf!=NULL) {
		strcpy(copybuf, text);
		sem_post(&copybuf_set);
	}
	pthread_mutex_unlock(&copybuf_mutex);
	return;
}

char *sdl_getcliptext(void)
{
	char *ret=NULL;

	sem_post(&pastebuf_request);
	sem_wait(&pastebuf_set);
	if(pastebuf!=NULL) {
		ret=(char *)malloc(strlen(pastebuf)+1);
		if(ret!=NULL)
			strcpy(ret,pastebuf);
	}
	sem_post(&pastebuf_request);
	return(ret);
}
#endif

/* Called from event thread only */
void sdl_add_key(unsigned int keyval)
{
	if(keyval==0xa600) {
		fullscreen=!fullscreen;
		sdl_user_func(SDL_USEREVENT_SETVIDMODE,vstat.charwidth*vstat.cols*vstat.scaling, vstat.charheight*vstat.rows*vstat.scaling);
		return;
	}
	if(keyval <= 0xffff) {
		SDL_mutexP(sdl_keylock);
		if(sdl_keynext+1==sdl_key) {
			sdl_beep();
			SDL_mutexV(sdl_keylock);
			return;
		}
		if((sdl_keynext+2==sdl_key) && keyval > 0xff) {
			sdl_beep();
			SDL_mutexV(sdl_keylock);
			return;
		}
		sdl_keybuf[sdl_keynext++]=keyval & 0xff;
		SDL_SemPost(sdl_key_pending);
		if(keyval>0xff) {
			sdl_keybuf[sdl_keynext++]=keyval >> 8;
			SDL_SemPost(sdl_key_pending);
		}
		SDL_mutexV(sdl_keylock);
	}
}

/* Called from event thread only */
int sdl_load_font(char *filename)
{
	unsigned char *font;
	unsigned int fontsize;
	int fw;
	int fh;
	int	ch;
	int x;
	int y;
	int charrow;
	int charcol;
	SDL_Rect r;

	/* I don't actually do this yet! */
	if(filename != NULL)
		return(-1);

	fh=vstat.charheight;
	fw=vstat.charwidth/8+(vstat.charwidth%8?1:0);

	fontsize=fw*fh*256*sizeof(unsigned char);

	if((font=(unsigned char *)malloc(fontsize))==NULL)
		return(-1);

	switch(vstat.charwidth) {
		case 8:
			switch(vstat.charheight) {
				case 8:
					memcpy(font, vga_font_bitmap8, fontsize);
					break;
				case 14:
					memcpy(font, vga_font_bitmap14, fontsize);
					break;
				case 16:
					memcpy(font, vga_font_bitmap, fontsize);
					break;
				default:
					return(-1);
			}
			break;
		default:
			return(-1);
	}

	if(sdl_font!=NULL)
		SDL_FreeSurface(sdl_font);
	sdl_font=SDL_CreateRGBSurface(SDL_HWSURFACE|SDL_SRCCOLORKEY, vstat.charwidth, vstat.charheight*256, 8, 0, 0, 0, 0);
	for(ch=0; ch<256; ch++) {
		for(charrow=0; charrow<vstat.charheight; charrow++) {
			for(charcol=0; charcol<vstat.charheight; charcol++) {
				if(font[(ch*vstat.charheight+charrow)*fw+(charcol/8)] & (1<<(charcol%8))) {
					r.x=charcol*vstat.scaling;
					r.y=(ch*vstat.charheight+charrow)*vstat.scaling;
					r.w=vstat.scaling;
					r.h=vstat.scaling;
					SDL_FillRect(sdl_font, &r, 1);
				}
			}
		}
	}

    return(0);
}

/* Called from events thread only */
int sdl_setup_colours(SDL_Surface *surf, int xor)
{
	int i;
	int ret=0;
	SDL_Color	co[16];

	for(i=0; i<16; i++) {
		co[i^xor].r=dac_default256[vstat.palette[i]].red;
		co[i^xor].g=dac_default256[vstat.palette[i]].green;
		co[i^xor].b=dac_default256[vstat.palette[i]].blue;
	}
	SDL_SetColors(surf, co, 0, 16);
	return(ret);
}

/* Called from events thread only */
void sdl_draw_cursor(void)
{
	SDL_Rect	src;
	SDL_Rect	dst;
	int	x;
	int	y;

	if(vstat.blink && vstat.curs_start<=vstat.curs_end) {
		dst.x=0;
		dst.y=0;
		src.x=vstat.curs_col*vstat.charwidth*vstat.scaling;
		src.y=(vstat.curs_row*vstat.charheight+vstat.curs_start)*vstat.scaling;
		src.w=dst.w=vstat.charwidth*vstat.scaling;
		src.h=dst.h=(vstat.curs_end-vstat.curs_start+1)*vstat.scaling;
		sdl_setup_colours(sdl_cursor, 0);
		SDL_BlitSurface(win, &src, sdl_cursor, &dst);
		sdl_setup_colours(sdl_cursor, vstat.currattr&0x07);
		SDL_BlitSurface(sdl_cursor, &dst, win, &src);
		lastcursor_x=vstat.curs_col;
		lastcursor_y=vstat.curs_row;
	}
}

/* Called from event thread */
int sdl_draw_one_char(unsigned short sch, unsigned int x, unsigned int y)
{
	SDL_Color	co;
	SDL_Rect	src;
	SDL_Rect	dst;
	unsigned char	ch;

	ch=(sch >> 8) & 0x0f;
	co.r=dac_default256[vstat.palette[ch]].red;
	co.g=dac_default256[vstat.palette[ch]].green;
	co.b=dac_default256[vstat.palette[ch]].blue;
	SDL_SetColors(sdl_font, &co, 1, 1);
	ch=(sch >> 12) & 0x07;
	co.r=dac_default256[vstat.palette[ch]].red;
	co.g=dac_default256[vstat.palette[ch]].green;
	co.b=dac_default256[vstat.palette[ch]].blue;
	SDL_SetColors(sdl_font, &co, 0, 1);
	dst.x=x*vstat.charwidth*vstat.scaling;
	dst.y=y*vstat.charheight*vstat.scaling;
	dst.w=vstat.charwidth*vstat.scaling;
	dst.h=vstat.charheight*vstat.scaling;
	src.x=0;
	src.w=vstat.charwidth;
	src.h=vstat.charheight;
	src.y=vstat.charheight*vstat.scaling;
	ch=sch & 0xff;
	if((sch >>15) && !(vstat.blink))
		src.y *= ' ';
	else
		src.y *= ch;
	SDL_BlitSurface(sdl_font, &src, win, &dst);
	return(0);
}

/* Called from event thread only, */
int sdl_full_screen_redraw(void)
{
	static int last_blink;
	int this_blink;
	int x;
	int y;
	unsigned int pos;
	unsigned short *newvmem;
	unsigned short *p;
	SDL_Rect	*rects;
	int rcount=0;

	this_blink=vstat.blink;
	if((newvmem=(unsigned short *)malloc(vstat.cols*vstat.rows*sizeof(unsigned short)))==NULL)
		return(-1);
	memcpy(newvmem, vstat.vmem, vstat.cols*vstat.rows*sizeof(unsigned short));
	sdl_updated=1;
	rects=(SDL_Rect *)malloc(sizeof(SDL_Rect)*vstat.cols*vstat.rows);
	/* Redraw all chars */
	pos=0;
	for(y=0;y<vstat.rows;y++) {
		for(x=0;x<vstat.cols;x++) {
			if((last_vmem==NULL)
					|| (last_vmem[pos] != newvmem[pos]) 
					|| (last_blink != this_blink && newvmem[pos]>>15) 
					|| (lastcursor_x==x && lastcursor_y==y)
					|| (vstat.curs_col==x && vstat.curs_row==y)
					) {
				sdl_draw_one_char(newvmem[pos],x,y);
				rects[rcount].x=x*vstat.charwidth*vstat.scaling;
				rects[rcount].y=y*vstat.charheight*vstat.scaling;
				rects[rcount].w=vstat.charwidth*vstat.scaling;
				rects[rcount++].h=vstat.charheight*vstat.scaling;
			}
			pos++;
		}
	}
	last_blink=this_blink;
	p=last_vmem;
	last_vmem=newvmem;
	free(p);

	sdl_draw_cursor();
	if(rcount)
		SDL_UpdateRects(win,rcount,rects);
	free(rects);
	return(0);
}

/* Called from event thread only */
unsigned int sdl_get_char_code(unsigned int keysym, unsigned int mod)
{
	int i;

	for(i=0;sdl_keyval[i].keysym;i++) {
		if(sdl_keyval[i].keysym==keysym) {
			if(mod & (KMOD_META|KMOD_ALT))
				return(sdl_keyval[i].alt);
			if(mod & KMOD_CTRL)
				return(sdl_keyval[i].ctrl);
			if(mod & KMOD_SHIFT)
				return(sdl_keyval[i].shift);
			return(sdl_keyval[i].key);
		}
	}
	return(0x01ffff);
}

/* Called from events thread only */
struct mainparams {
	int	argc;
	char	**argv;
};

/* Called from events thread only */
int sdl_runmain(void *data)
{
	struct mainparams *mp=data;
	SDL_Event	ev;

	sdl_exitcode=CIOLIB_main(mp->argc, mp->argv);
	ev.type=SDL_QUIT;
	while(SDL_PeepEvents(&ev, 1, SDL_ADDEVENT, 0xffffffff)!=1);
	return(0);
}

/* Mouse event/keyboard thread */
int sdl_mouse_thread(void *data)
{
	while(1) {
		if(mouse_wait())
			sdl_add_key(CIO_KEY_MOUSE);
	}
}

/* Event Thread */
int main(int argc, char **argv)
{
	unsigned int i;
	SDL_Event	ev;
	struct mainparams mp;

	mp.argc=argc;
	mp.argv=argv;
	SDL_Init(SDL_INIT_VIDEO);

	sdl_key_pending=SDL_CreateSemaphore(0);
	sdl_init_complete=SDL_CreateSemaphore(0);
	sdl_init_complete=SDL_CreateSemaphore(0);

	SDL_CreateThread(sdl_runmain, &mp);

	while(1) {
		if(SDL_WaitEvent(&ev)==1) {
			switch (ev.type) {
				case SDL_ACTIVEEVENT:		/* Focus change */
					break;
				case SDL_KEYDOWN:			/* Keypress */
					if(ev.key.keysym.unicode > 0 && ev.key.keysym.unicode <= 0x7f) {		/* ASCII Key (Whoopee!) */
						/* Need magical handling here... 
						 * if ALT is pressed, run 'er through 
						 * sdl_get_char_code() ANYWAYS */
						if(ev.key.keysym.mod & (KMOD_META|KMOD_ALT))
							sdl_add_key(sdl_get_char_code(ev.key.keysym.sym, ev.key.keysym.mod));
						else
							sdl_add_key(ev.key.keysym.unicode&0x7f);
					}
					else 
						sdl_add_key(sdl_get_char_code(ev.key.keysym.sym, ev.key.keysym.mod));
					break;
				case SDL_KEYUP:				/* Ignored (handled in KEYDOWN event) */
					break;
				case SDL_MOUSEMOTION:
					ciomouse_gotevent(CIOLIB_MOUSE_MOVE,ev.motion.x/(vstat.charwidth*vstat.scaling)+1,ev.motion.y/(vstat.charheight*vstat.scaling)+1);
					break;
				case SDL_MOUSEBUTTONDOWN:
					switch(ev.button.button) {
						case SDL_BUTTON_LEFT:
							ciomouse_gotevent(CIOLIB_BUTTON_PRESS(1),ev.button.x/(vstat.charwidth*vstat.scaling)+1,ev.button.y/(vstat.charheight*vstat.scaling)+1);
							break;
						case SDL_BUTTON_MIDDLE:
							ciomouse_gotevent(CIOLIB_BUTTON_PRESS(2),ev.button.x/(vstat.charwidth*vstat.scaling)+1,ev.button.y/(vstat.charheight*vstat.scaling)+1);
							break;
						case SDL_BUTTON_RIGHT:
							ciomouse_gotevent(CIOLIB_BUTTON_PRESS(3),ev.button.x/(vstat.charwidth*vstat.scaling)+1,ev.button.y/(vstat.charheight*vstat.scaling)+1);
							break;
					}
					break;
				case SDL_MOUSEBUTTONUP:
					switch(ev.button.button) {
						case SDL_BUTTON_LEFT:
							ciomouse_gotevent(CIOLIB_BUTTON_RELEASE(1),ev.button.x/(vstat.charwidth*vstat.scaling)+1,ev.button.y/(vstat.charheight*vstat.scaling)+1);
							break;
						case SDL_BUTTON_MIDDLE:
							ciomouse_gotevent(CIOLIB_BUTTON_RELEASE(2),ev.button.x/(vstat.charwidth*vstat.scaling)+1,ev.button.y/(vstat.charheight*vstat.scaling)+1);
							break;
						case SDL_BUTTON_RIGHT:
							ciomouse_gotevent(CIOLIB_BUTTON_RELEASE(3),ev.button.x/(vstat.charwidth*vstat.scaling)+1,ev.button.y/(vstat.charheight*vstat.scaling)+1);
							break;
					}
					break;
				case SDL_QUIT:
					return(sdl_exitcode);
				case SDL_VIDEORESIZE:
					if(ev.resize.w > 0 && ev.resize.h > 0) {
						FREE_AND_NULL(last_vmem);
						vstat.scaling=(int)(ev.resize.w/(vstat.charwidth*vstat.cols));
						if(vstat.scaling < 1)
							vstat.scaling=1;
						win=SDL_SetVideoMode(vstat.charwidth*vstat.cols*vstat.scaling, vstat.charheight*vstat.rows*vstat.scaling, 8, SDL_HWSURFACE|SDL_HWPALETTE|(fullscreen?SDL_FULLSCREEN:0)|SDL_RESIZABLE|SDL_DOUBLEBUF);
						if(win!=NULL) {
							if(sdl_cursor!=NULL)
								SDL_FreeSurface(sdl_cursor);
							sdl_cursor=SDL_CreateRGBSurface(SDL_HWSURFACE|SDL_SRCCOLORKEY, vstat.charwidth, vstat.charheight, 8, 0, 0, 0, 0);
						    /* Update font. */
						    sdl_load_font(NULL);
						    sdl_setup_colours(win,0);
							sdl_full_screen_redraw();
						}
						else if(sdl_init_good) {
							ev.type=SDL_QUIT;
							sdl_exitcode=1;
							SDL_PeepEvents(&ev, 1, SDL_ADDEVENT, 0xffffffff);
						}
					}
					break;
				case SDL_VIDEOEXPOSE:
					FREE_AND_NULL(last_vmem);
					sdl_full_screen_redraw();
					break;
				case SDL_USEREVENT: {
					/* Tell SDL to do various stuff... */
					switch(ev.user.code) {
						case SDL_USEREVENT_UPDATERECT:
							sdl_full_screen_redraw();
							break;
						case SDL_USEREVENT_SETTITLE:
							SDL_WM_SetCaption((char *)ev.user.data1,NULL);
							free(ev.user.data1);
							break;
						case SDL_USEREVENT_SETVIDMODE:
							FREE_AND_NULL(last_vmem);
							win=SDL_SetVideoMode(*((int *)ev.user.data1),*((int *)ev.user.data2),8, SDL_HWSURFACE|SDL_HWPALETTE|(fullscreen?SDL_FULLSCREEN:0)|SDL_RESIZABLE|SDL_DOUBLEBUF);
							if(win!=NULL) {
								sdl_setup_colours(win,0);
								if(sdl_cursor!=NULL)
									SDL_FreeSurface(sdl_cursor);
								sdl_cursor=SDL_CreateRGBSurface(SDL_HWSURFACE|SDL_SRCCOLORKEY, vstat.charwidth, vstat.charheight, 8, 0, 0, 0, 0);
								/* Update font. */
								sdl_load_font(NULL);
							}
							else if(sdl_init_good) {
								ev.type=SDL_QUIT;
								sdl_exitcode=1;
								SDL_PeepEvents(&ev, 1, SDL_ADDEVENT, 0xffffffff);
							}
							free(ev.user.data1);
							free(ev.user.data2);
							break;
						case SDL_USEREVENT_HIDEMOUSE:
							SDL_ShowCursor(SDL_DISABLE);
							break;
						case SDL_USEREVENT_SHOWMOUSE:
							SDL_ShowCursor(SDL_ENABLE);
							break;
						case SDL_USEREVENT_INIT:
							if(!sdl_init_good) {
								if(SDL_WasInit(SDL_INIT_VIDEO)==SDL_INIT_VIDEO) {
									if(win != NULL) {
										SDL_EnableUNICODE(1);
										SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

										SDL_CreateThread(sdl_blinker_thread, NULL);
										SDL_CreateThread(sdl_mouse_thread, NULL);
										sdl_init_good=1;
									}
								}
							}
							SDL_SemPost(sdl_init_complete);
							break;
					}
					break;
				}
				/* Ignore this stuff */
				case SDL_SYSWMEVENT:			/* ToDo... This is where Copy/Paste needs doing */
				case SDL_JOYAXISMOTION:
				case SDL_JOYBALLMOTION:
				case SDL_JOYHATMOTION:
				case SDL_JOYBUTTONDOWN:
				case SDL_JOYBUTTONUP:
				default:
					break;
			}
		}
	}
}

