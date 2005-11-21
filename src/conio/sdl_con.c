#if (defined(__MACH__) && defined(__APPLE__))
#include <Carbon/Carbon.h>
#endif

#include <stdarg.h>
#include <stdio.h>		/* NULL */
#include <stdlib.h>
#include <string.h>
#ifdef __unix__
#include <dlfcn.h>
#endif

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

#ifdef main
	#undef main
#endif
#include "SDL.h"
#include "SDL_thread.h"

#include "sdlfuncs.h"

#ifndef _WIN32
struct sdlfuncs sdl;
#endif

extern int	CIOLIB_main(int argc, char **argv);

/********************************************************/
/* Low Level Stuff										*/
/* This should all be called from the same thread!		*/
/********************************************************/

SDL_Surface	*win=NULL;
SDL_mutex *sdl_keylock;
SDL_mutex *sdl_updlock;
SDL_mutex *sdl_vstatlock;
SDL_mutex *sdl_ufunc_lock;
SDL_sem *sdl_key_pending;
SDL_sem *sdl_init_complete;
SDL_sem *sdl_ufunc_ret;
int sdl_ufunc_retval;
int	sdl_init_good=0;
int sdl_updated;
int sdl_exitcode=0;

SDL_Surface *sdl_font=NULL;
SDL_Surface	*sdl_cursor=NULL;

static int lastcursor_x=0;
static int lastcursor_y=0;
static int sdl_current_font=-99;
static int lastfg=-1;
static int lastbg=-1;

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
	,SDL_USEREVENT_SETNAME
	,SDL_USEREVENT_SETVIDMODE
	,SDL_USEREVENT_SHOWMOUSE
	,SDL_USEREVENT_HIDEMOUSE
	,SDL_USEREVENT_INIT
	,SDL_USEREVENT_COPY
	,SDL_USEREVENT_PASTE
	,SDL_USEREVENT_LOADFONT
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
	{SDLK_KP_PERIOD, 0x7f, 0x7f, 0x5300, 0x9300},
	{SDLK_KP_DIVIDE, '/', '/', 0x9500, 0xa400},
	{SDLK_KP_ENTER, 0x0d, 0x0d, 0x0a, 0xa600},
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

/* *nix copy/paste stuff */
SDL_sem	*sdl_pastebuf_set;
SDL_sem	*sdl_pastebuf_copied;
SDL_mutex	*sdl_copybuf_mutex;
char *sdl_copybuf=NULL;
char *sdl_pastebuf=NULL;

#if !defined(NO_X) && defined(__unix__)
#include "SDL_syswm.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>

#define CONSOLE_CLIPBOARD	XA_PRIMARY

int sdl_x11available=0;

/* X functions */
struct x11 {
	int		(*XFree)		(void *data);
	Window	(*XGetSelectionOwner)	(Display*, Atom);
	int		(*XConvertSelection)	(Display*, Atom, Atom, Atom, Window, Time);
	int		(*XGetWindowProperty)	(Display*, Window, Atom, long, long, Bool, Atom, Atom*, int*, unsigned long *, unsigned long *, unsigned char **);
	int		(*XChangeProperty)		(Display*, Window, Atom, Atom, int, int, _Xconst unsigned char*, int);
	Status	(*XSendEvent)	(Display*, Window, Bool, long, XEvent*);
	int		(*XSetSelectionOwner)	(Display*, Atom, Window, Time);
};
struct x11 sdl_x11;
#endif

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
			sdl.mutexP(sdl_updlock);
			if(sdl_updated) {
				if(sdl.PeepEvents(&ev, 1, SDL_ADDEVENT, 0xffffffff)==1);
					sdl_updated=0;
			}
			sdl.mutexV(sdl_updlock);
			break;
		case SDL_USEREVENT_SETNAME:
			if((ev.user.data1=strdup(va_arg(argptr, char *)))==NULL) {
				va_end(argptr);
				return;
			}
			while(sdl.PeepEvents(&ev, 1, SDL_ADDEVENT, 0xffffffff)!=1);
			break;
		case SDL_USEREVENT_SETTITLE:
			if((ev.user.data1=strdup(va_arg(argptr, char *)))==NULL) {
				va_end(argptr);
				return;
			}
			while(sdl.PeepEvents(&ev, 1, SDL_ADDEVENT, 0xffffffff)!=1);
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
			while(sdl.PeepEvents(&ev, 1, SDL_ADDEVENT, 0xffffffff)!=1);
			break;
		case SDL_USEREVENT_COPY:
		case SDL_USEREVENT_PASTE:
		case SDL_USEREVENT_SHOWMOUSE:
		case SDL_USEREVENT_HIDEMOUSE:
		case SDL_USEREVENT_INIT:
			while(sdl.PeepEvents(&ev, 1, SDL_ADDEVENT, 0xffffffff)!=1);
			break;
	}
	va_end(argptr);
}

/* Called from main thread only */
int sdl_user_func_ret(int func, ...)
{
	unsigned int	*i;
	va_list argptr;
	void	**args;
	SDL_Event	ev;
	int		passed=FALSE;
	char	*p;

	sdl.mutexP(sdl_ufunc_lock);
	ev.type=SDL_USEREVENT;
	ev.user.data1=NULL;
	ev.user.data2=NULL;
	ev.user.code=func;
	va_start(argptr, func);
	switch(func) {
		case SDL_USEREVENT_LOADFONT:
			p=va_arg(argptr, char *);
			if(p!=NULL) {
				if((ev.user.data1=strdup(p))==NULL) {
					va_end(argptr);
					return;
				}
			}
			while(sdl.PeepEvents(&ev, 1, SDL_ADDEVENT, 0xffffffff)!=1);
			passed=TRUE;
			break;
	}
	if(passed)
		sdl.SemWait(sdl_ufunc_ret);
	else
		sdl_ufunc_retval=-1;
	sdl.mutexV(sdl_ufunc_lock);
	va_end(argptr);
	return(sdl_ufunc_retval);
}

#if (defined(__MACH__) && defined(__APPLE__))
int sdl_using_quartz=0;
#endif

#if !defined(NO_X) && defined(__unix__)
int sdl_using_x11()
{
	char	driver[16];

	if(sdl.VideoDriverName(driver, sizeof(driver))==NULL)
		return(FALSE);
	if(!strcmp(driver,"x11"))
		return(TRUE);
	if(!strcmp(driver,"dga"))
		return(TRUE);
	return(FALSE);
}
#endif

void sdl_copytext(const char *text, size_t buflen)
{
#if (defined(__MACH__) && defined(__APPLE__))
	if(sdl_using_quartz) {
		sdl.mutexP(sdl_copybuf_mutex);
		FREE_AND_NULL(sdl_copybuf);

		sdl_copybuf=(char *)malloc(buflen+1);
		if(sdl_copybuf!=NULL) {
			strcpy(sdl_copybuf, text);
			sdl_user_func(SDL_USEREVENT_COPY,0,0,0,0);
		}
		sdl.mutexV(sdl_copybuf_mutex);
		return;
	}
#endif

#if !defined(NO_X) && defined(__unix__)
	if(sdl_x11available && sdl_using_x11()) {
		sdl.mutexP(sdl_copybuf_mutex);
		FREE_AND_NULL(sdl_copybuf);

		sdl_copybuf=(char *)malloc(buflen+1);
		if(sdl_copybuf!=NULL) {
			strcpy(sdl_copybuf, text);
			sdl_user_func(SDL_USEREVENT_COPY,0,0,0,0);
		}
		sdl.mutexV(sdl_copybuf_mutex);
		return;
	}
#endif

	sdl.mutexP(sdl_copybuf_mutex);
	FREE_AND_NULL(sdl_copybuf);

	sdl_copybuf=(char *)malloc(buflen+1);
	if(sdl_copybuf!=NULL)
		strcpy(sdl_copybuf, text);
	sdl.mutexV(sdl_copybuf_mutex);
	return;
}

char *sdl_getcliptext(void)
{
	char *ret=NULL;

#if (defined(__MACH__) && defined(__APPLE__))
	if(sdl_using_quartz) {
		sdl_user_func(SDL_USEREVENT_PASTE,0,0,0,0);
		sdl.SemWait(sdl_pastebuf_set);
		if(sdl_pastebuf!=NULL) {
			ret=(char *)malloc(strlen(sdl_pastebuf)+1);
			if(ret!=NULL)
				strcpy(ret,sdl_pastebuf);
			sdl.SemPost(sdl_pastebuf_copied);
		}
		return(ret);

	}
#endif

#if !defined(NO_X) && defined(__unix__)
	if(sdl_x11available && sdl_using_x11()) {
		sdl_user_func(SDL_USEREVENT_PASTE,0,0,0,0);
		sdl.SemWait(sdl_pastebuf_set);
		if(sdl_pastebuf!=NULL) {
			ret=(char *)malloc(strlen(sdl_pastebuf)+1);
			if(ret!=NULL)
				strcpy(ret,sdl_pastebuf);
			sdl.SemPost(sdl_pastebuf_copied);
		}
		return(ret);
	}
#endif
	sdl.mutexP(sdl_copybuf_mutex);
	ret=(char *)malloc(strlen(sdl_pastebuf)+1);
	if(ret!=NULL)
		strcpy(ret,sdl_copybuf);
	sdl.mutexV(sdl_copybuf_mutex);
	return(ret);
}

/* Blinker Thread */
int sdl_blinker_thread(void *data)
{
	while(1) {
		SLEEP(500);
		sdl.mutexP(sdl_vstatlock);
		if(vstat.blink)
			vstat.blink=FALSE;
		else
			vstat.blink=TRUE;
		sdl.mutexV(sdl_vstatlock);
		sdl_user_func(SDL_USEREVENT_UPDATERECT,0,0,0,0);
	}
}

/* Called from main thread only (Passes Event) */
int sdl_init_mode(int mode)
{
    struct video_params vmode;
    int idx;			/* Index into vmode */
    int i;

	sdl.mutexP(sdl_vstatlock);
	if(load_vmode(&vstat, mode))
		return(-1);

	sdl_user_func(SDL_USEREVENT_SETVIDMODE,vstat.charwidth*vstat.cols*vstat.scaling, vstat.charheight*vstat.rows*vstat.scaling);

	/* Initialize video memory with black background, white foreground */
	for (i = 0; i < vstat.cols*vstat.rows; ++i)
	    vstat.vmem[i] = 0x0700;
	vstat.currattr=7;

	vstat.mode=mode;
	sdl.mutexV(sdl_vstatlock);

	sdl_user_func(SDL_USEREVENT_UPDATERECT,0,0,0,0);

    return(0);
}

/* Called from main thread only (Passes Event) */
int sdl_draw_char(unsigned short vch, int xpos, int ypos, int update)
{
	sdl.mutexP(sdl_vstatlock);
	vstat.vmem[ypos*vstat.cols+xpos]=vch;
	sdl.mutexV(sdl_vstatlock);

	if(update) {
#if 0	/* Currently, an update always updates the whole screen... so don't hold the mutex */
		sdl_user_func(SDL_USEREVENT_UPDATERECT,xpos*vstat.charwidth*vstat.scaling,ypos*vstat.charheight*vstat.scaling,vstat.charwidth*vstat.scaling,vstat.charheight*vstat.scaling);
#else
		sdl_user_func(SDL_USEREVENT_UPDATERECT,0,0,0,0);
#endif
	}

	return(0);
}

/* Called from main thread only (Passes Event) */
int sdl_init(int mode)
{
#if !defined(NO_X) && defined(__unix__)
	void *dl;
#endif

	if(!sdl.gotfuncs)
		return(-1);

	sdl.mutexP(sdl_vstatlock);
	vstat.vmem=NULL;
	vstat.scaling=1;
	sdl.mutexV(sdl_vstatlock);

	sdl.mutexP(sdl_updlock);
	sdl_updated=1;
	sdl.mutexV(sdl_updlock);

	if(mode==CIOLIB_MODE_SDL_FULLSCREEN)
		fullscreen=1;

	sdl_init_mode(3);

	atexit(sdl.Quit);

	sdl_user_func(SDL_USEREVENT_INIT);

	sdl.SemWait(sdl_init_complete);
	if(sdl_init_good) {
		cio_api.mode=fullscreen?CIOLIB_MODE_SDL_FULLSCREEN:CIOLIB_MODE_SDL;
#ifdef _WIN32
		FreeConsole();
#endif
#if !defined(NO_X) && defined(__unix__)
	#if defined(__APPLE__) && defined(__MACH__) && defined(__POWERPC__)
		if((dl=dlopen("/usr/X11R6/lib/libX11.dylib",RTLD_LAZY|RTLD_GLOBAL))!=NULL) {
	#else
		if((dl=dlopen("libX11.so",RTLD_LAZY))!=NULL) {
	#endif
			sdl_x11available=TRUE;
			if(sdl_x11available && (sdl_x11.XFree=dlsym(dl,"XFree"))==NULL) {
				dlclose(dl);
				sdl_x11available=FALSE;
			}
			if(sdl_x11available && (sdl_x11.XGetSelectionOwner=dlsym(dl,"XGetSelectionOwner"))==NULL) {
				dlclose(dl);
				sdl_x11available=FALSE;
			}
			if(sdl_x11available && (sdl_x11.XConvertSelection=dlsym(dl,"XConvertSelection"))==NULL) {
				dlclose(dl);
				sdl_x11available=FALSE;
			}
			if(sdl_x11available && (sdl_x11.XGetWindowProperty=dlsym(dl,"XGetWindowProperty"))==NULL) {
				dlclose(dl);
				sdl_x11available=FALSE;
			}
			if(sdl_x11available && (sdl_x11.XChangeProperty=dlsym(dl,"XChangeProperty"))==NULL) {
				dlclose(dl);
				sdl_x11available=FALSE;
			}
			if(sdl_x11available && (sdl_x11.XSendEvent=dlsym(dl,"XSendEvent"))==NULL) {
				dlclose(dl);
				sdl_x11available=FALSE;
			}
			if(sdl_x11available && (sdl_x11.XSetSelectionOwner=dlsym(dl,"XSetSelectionOwner"))==NULL) {
				dlclose(dl);
				sdl_x11available=FALSE;
			}
		}
		if(sdl_x11available)
			sdl.EventState(SDL_SYSWMEVENT, SDL_ENABLE);
#endif
		return(0);
	}

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
	sdl.mutexP(sdl_vstatlock);
	for(y=sy-1;y<ey;y++) {
		for(x=sx-1;x<ex;x++) {
			sch=*(out++);
			sch |= (*(out++))<<8;
			vstat.vmem[y*vstat.cols+x]=sch;
		}
	}
	sdl.mutexV(sdl_vstatlock);
	sdl_user_func(SDL_USEREVENT_UPDATERECT,0,0,0,0);
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
	sdl.mutexP(sdl_vstatlock);
	for(y=sy-1;y<ey;y++) {
		for(x=sx-1;x<ex;x++) {
			sch=vstat.vmem[y*vstat.cols+x];
			*(out++)=sch & 0xff;
			*(out++)=sch >> 8;
		}
	}
	sdl.mutexV(sdl_vstatlock);
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

	sdl.mutexP(sdl_keylock);
	ret=(sdl_key!=sdl_keynext);
	sdl.mutexV(sdl_keylock);
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

	sdl.mutexP(sdl_vstatlock);
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
	sdl.mutexV(sdl_vstatlock);

	return(ch);
}

/* Called from main thread only */
void sdl_gotoxy(int x, int y)
{
	sdl.mutexP(sdl_vstatlock);
	vstat.curs_row=y-1;
	vstat.curs_col=x-1;
	sdl.mutexV(sdl_vstatlock);
}

/* Called from main thread only */
void sdl_gettextinfo(struct text_info *info)
{
	sdl.mutexP(sdl_vstatlock);
	info->currmode=vstat.mode;
	info->screenheight=vstat.rows;
	info->screenwidth=vstat.cols;
	info->curx=sdl_wherex();
	info->cury=sdl_wherey();
	info->attribute=vstat.currattr;
	sdl.mutexV(sdl_vstatlock);
}

/* Called from main thread only */
void sdl_setcursortype(int type)
{
	sdl.mutexP(sdl_vstatlock);
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
	sdl.mutexV(sdl_vstatlock);
}

/* Called from main thread only */
int sdl_getch(void)
{
	int ch;

	sdl.SemWait(sdl_key_pending);
	sdl.mutexP(sdl_keylock);
	ch=sdl_keybuf[sdl_key++];
	sdl.mutexV(sdl_keylock);
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
		sdl_getch();
	}
}

/* Called from main thread only */
void sdl_textmode(int mode)
{
	sdl_init_mode(mode);
}

/* Called from main thread only (Passes Event) */
int sdl_setname(const char *name)
{
	sdl_user_func(SDL_USEREVENT_SETNAME,name);
	return(0);
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

int sdl_loadfont(char *filename)
{
	int retval;

	retval=sdl_user_func_ret(SDL_USEREVENT_LOADFONT,filename);
	return(retval);
}

int sdl_setfont(int font, int force)
{
	int changemode=0;

	if(font < 0 || font>(sizeof(conio_fontdata)/sizeof(struct conio_font_data_struct)-2))
		return(-1);

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
	sdl_current_font=font;
	if(changemode)
		sdl_init_mode(3);
	return(sdl_user_func_ret(SDL_USEREVENT_LOADFONT,NULL));
}

int sdl_getfont(void)
{
	return(sdl_current_font);
}

/* Called from event thread only */
void sdl_add_key(unsigned int keyval)
{
	if(keyval==0xa600) {
		fullscreen=!fullscreen;
		cio_api.mode=fullscreen?CIOLIB_MODE_SDL_FULLSCREEN:CIOLIB_MODE_SDL;
		sdl.mutexP(sdl_vstatlock);
		sdl_user_func(SDL_USEREVENT_SETVIDMODE,vstat.charwidth*vstat.cols, vstat.charheight*vstat.rows);
		sdl.mutexV(sdl_vstatlock);
		return;
	}
	if(keyval <= 0xffff) {
		sdl.mutexP(sdl_keylock);
		if(sdl_keynext+1==sdl_key) {
			sdl_beep();
			sdl.mutexV(sdl_keylock);
			return;
		}
		if((sdl_keynext+2==sdl_key) && keyval > 0xff) {
			sdl_beep();
			sdl.mutexV(sdl_keylock);
			return;
		}
		sdl_keybuf[sdl_keynext++]=keyval & 0xff;
		sdl.SemPost(sdl_key_pending);
		if(keyval>0xff) {
			sdl_keybuf[sdl_keynext++]=keyval >> 8;
			sdl.SemPost(sdl_key_pending);
		}
		sdl.mutexV(sdl_keylock);
	}
}

/* Called from event thread only */
int sdl_load_font(char *filename)
{
	static char current_filename[MAX_PATH];
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
	FILE	*fontfile;

	if(sdl_current_font==-99 || sdl_current_font>(sizeof(conio_fontdata)/sizeof(struct conio_font_data_struct)-2)) {
		for(x=0; conio_fontdata[x].desc != NULL; x++) {
			if(!strcmp(conio_fontdata[x].desc, "Codepage 437 English")) {
				sdl_current_font=x;
				break;
			}
		}
		if(conio_fontdata[x].desc==NULL)
			sdl_current_font=0;
	}
	if(sdl_current_font==-1)
		filename=current_filename;
	else if(conio_fontdata[sdl_current_font].desc==NULL)
		return(-1);

	sdl.mutexP(sdl_vstatlock);
	fh=vstat.charheight;
	fw=vstat.charwidth/8+(vstat.charwidth%8?1:0);

	fontsize=fw*fh*256*sizeof(unsigned char);

	if((font=(unsigned char *)malloc(fontsize))==NULL) {
		sdl.mutexV(sdl_vstatlock);
		return(-1);
	}

	if(filename != NULL) {
		if(flength(filename)!=fontsize) {
			sdl.mutexV(sdl_vstatlock);
			free(font);
			return(-1);
		}
		if((fontfile=fopen(filename,"rb"))==NULL) {
			sdl.mutexV(sdl_vstatlock);
			free(font);
			return(-1);
		}
		if(fread(font, 1, fontsize, fontfile)!=fontsize) {
			sdl.mutexV(sdl_vstatlock);
			free(font);
			fclose(fontfile);
			return(-1);
		}
		fclose(fontfile);
		sdl_current_font=-1;
		if(filename != current_filename)
			SAFECOPY(current_filename,filename);
	}
	else {
		switch(vstat.charwidth) {
			case 8:
				switch(vstat.charheight) {
					case 8:
						if(conio_fontdata[sdl_current_font].eight_by_eight==NULL) {
							sdl.mutexV(sdl_vstatlock);
							free(font);
							return(-1);
						}
						memcpy(font, conio_fontdata[sdl_current_font].eight_by_eight, fontsize);
						break;
					case 14:
						if(conio_fontdata[sdl_current_font].eight_by_fourteen==NULL) {
							sdl.mutexV(sdl_vstatlock);
							free(font);
							return(-1);
						}
						memcpy(font, conio_fontdata[sdl_current_font].eight_by_fourteen, fontsize);
						break;
					case 16:
						if(conio_fontdata[sdl_current_font].eight_by_sixteen==NULL) {
							sdl.mutexV(sdl_vstatlock);
							free(font);
							return(-1);
						}
						memcpy(font, conio_fontdata[sdl_current_font].eight_by_sixteen, fontsize);
						break;
					default:
						sdl.mutexV(sdl_vstatlock);
						free(font);
						return(-1);
				}
				break;
			default:
				sdl.mutexV(sdl_vstatlock);
				free(font);
				return(-1);
		}
	}

	if(sdl_font!=NULL)
		sdl.FreeSurface(sdl_font);
	sdl_font=sdl.CreateRGBSurface(SDL_SWSURFACE|SDL_SRCCOLORKEY, vstat.charwidth, vstat.charheight*256, 8, 0, 0, 0, 0);
	if(sdl_font == NULL) {
		sdl.mutexV(sdl_vstatlock);
		free(font);
    	return(-1);
	}
	else {
		for(ch=0; ch<256; ch++) {
			for(charrow=0; charrow<vstat.charheight; charrow++) {
				for(charcol=0; charcol<vstat.charheight; charcol++) {
					if(font[(ch*vstat.charheight+charrow)*fw+(charcol/8)] & (0x80 >> (charcol%8))) {
						r.x=charcol*vstat.scaling;
						r.y=(ch*vstat.charheight+charrow)*vstat.scaling;
						r.w=vstat.scaling;
						r.h=vstat.scaling;
						sdl.FillRect(sdl_font, &r, 1);
					}
				}
			}
		}
	}
	sdl.mutexV(sdl_vstatlock);
	free(font);
	lastfg=-1;
	lastbg=-1;
    return(0);
}

/* Called from events thread only */
int sdl_setup_colours(SDL_Surface *surf, int xor)
{
	int i;
	int ret=0;
	SDL_Color	co[16];

	sdl.mutexP(sdl_vstatlock);
	for(i=0; i<16; i++) {
		co[i^xor].r=dac_default256[vstat.palette[i]].red;
		co[i^xor].g=dac_default256[vstat.palette[i]].green;
		co[i^xor].b=dac_default256[vstat.palette[i]].blue;
	}
	sdl.mutexV(sdl_vstatlock);
	sdl.SetColors(surf, co, 0, 16);
	return(ret);
}

/* Called from events thread only */
void sdl_draw_cursor(void)
{
	SDL_Rect	src;
	SDL_Rect	dst;
	int	x;
	int	y;

	sdl.mutexP(sdl_vstatlock);
	if(vstat.blink && vstat.curs_start<=vstat.curs_end) {
		dst.x=0;
		dst.y=0;
		src.x=vstat.curs_col*vstat.charwidth*vstat.scaling;
		src.y=(vstat.curs_row*vstat.charheight+vstat.curs_start)*vstat.scaling;
		src.w=dst.w=vstat.charwidth*vstat.scaling;
		src.h=dst.h=(vstat.curs_end-vstat.curs_start+1)*vstat.scaling;
		sdl_setup_colours(sdl_cursor, 0);
		sdl.BlitSurface(win, &src, sdl_cursor, &dst);
		sdl_setup_colours(sdl_cursor, vstat.currattr&0x07);
		sdl.BlitSurface(sdl_cursor, &dst, win, &src);
		lastcursor_x=vstat.curs_col;
		lastcursor_y=vstat.curs_row;
	}
	sdl.mutexV(sdl_vstatlock);
}

/* Called from event thread */
/* ONLY Called from sdl_full_screen_redraw() which holds the mutex... */
int sdl_draw_one_char(unsigned short sch, unsigned int x, unsigned int y, struct video_stats *vs)
{
	SDL_Color	co;
	SDL_Rect	src;
	SDL_Rect	dst;
	unsigned char	ch;

	ch=(sch >> 8) & 0x0f;
	if(lastfg!=ch) {
		co.r=dac_default256[vs->palette[ch]].red;
		co.g=dac_default256[vs->palette[ch]].green;
		co.b=dac_default256[vs->palette[ch]].blue;
		sdl.SetColors(sdl_font, &co, 1, 1);
		lastfg=ch;
	}
	ch=(sch >> 12) & 0x07;
	if(lastbg!=ch) {
		co.r=dac_default256[vs->palette[ch]].red;
		co.g=dac_default256[vs->palette[ch]].green;
		co.b=dac_default256[vs->palette[ch]].blue;
		sdl.SetColors(sdl_font, &co, 0, 1);
		lastbg=ch;
	}
	dst.x=x*vs->charwidth*vs->scaling;
	dst.y=y*vs->charheight*vs->scaling;
	dst.w=vs->charwidth*vs->scaling;
	dst.h=vs->charheight*vs->scaling;
	src.x=0;
	src.w=vs->charwidth;
	src.h=vs->charheight;
	src.y=vs->charheight*vs->scaling;
	ch=sch & 0xff;
	if((sch >>15) && !(vs->blink))
		src.y *= ' ';
	else
		src.y *= ch;
	if(sdl_font != NULL)
		sdl.BlitSurface(sdl_font, &src, win, &dst);
	return(0);
}

/* Called from event thread only, */
int sdl_full_screen_redraw(void)
{
	static int last_blink;
	int x;
	int y;
	unsigned int pos;
	unsigned short *newvmem;
	unsigned short *p;
	SDL_Rect	*rects;
	int rcount=0;
	struct video_stats vs;

	sdl.mutexP(sdl_vstatlock);
	memcpy(&vs, &vstat, sizeof(vs));
	if((newvmem=(unsigned short *)malloc(vs.cols*vs.rows*sizeof(unsigned short)))==NULL)
		return(-1);
	memcpy(newvmem, vs.vmem, vs.cols*vs.rows*sizeof(unsigned short));
	sdl.mutexV(sdl_vstatlock);
	rects=(SDL_Rect *)malloc(sizeof(SDL_Rect)*vs.cols*vs.rows);
	if(rects==NULL)
		return(-1);
	sdl.mutexP(sdl_updlock);
	sdl_updated=1;
	sdl.mutexV(sdl_updlock);
	/* Redraw all chars */
	pos=0;
	for(y=0;y<vs.rows;y++) {
		for(x=0;x<vs.cols;x++) {
			if((last_vmem==NULL)
					|| (last_vmem[pos] != newvmem[pos]) 
					|| (last_blink != vs.blink && newvmem[pos]>>15) 
					|| (lastcursor_x==x && lastcursor_y==y)
					|| (vs.curs_col==x && vs.curs_row==y)
					) {
				sdl_draw_one_char(newvmem[pos],x,y,&vs);
				rects[rcount].x=x*vs.charwidth*vs.scaling;
				rects[rcount].y=y*vs.charheight*vs.scaling;
				rects[rcount].w=vs.charwidth*vs.scaling;
				rects[rcount++].h=vs.charheight*vs.scaling;
			}
			pos++;
		}
	}

	last_blink=vs.blink;
	p=last_vmem;
	last_vmem=newvmem;
	free(p);

	sdl_draw_cursor();
	if(rcount)
		sdl.UpdateRects(win,rcount,rects);
	free(rects);
	return(0);
}

/* Called from event thread only */
unsigned int sdl_get_char_code(unsigned int keysym, unsigned int mod, unsigned int unicode)
{
	int i;

#ifdef __DARWIN__
	if(unicode==\x7f) {
		unicode=0x0f;
		keysym=SDLK_DELETE;
	}
#endif
	if((mod & KMOD_META|KMOD_ALT) && (mod & KMOD_CTRL) && unicode && (unicode < 256))
		return(unicode);
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
#ifdef _WIN32
	if((mod & (KMOD_META|KMOD_ALT)) && (unicode=='\t'))
		return(0x01ffff);
#endif
	if(unicode  && unicode < 256)
		return(unicode);
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
	while(sdl.PeepEvents(&ev, 1, SDL_ADDEVENT, 0xffffffff)!=1);
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
	char	drivername[64];

#ifndef _WIN32
	load_sdl_funcs(&sdl);
#endif

	if(sdl.gotfuncs) {
#ifdef _WIN32
		/* Fail to windib (ie: No mouse attached) */
		if(sdl.Init(SDL_INIT_VIDEO)) {
			if(getenv("SDL_VIDEODRIVER")==NULL) {
				putenv("SDL_VIDEODRIVER=windib");
				WinExec(GetCommandLine(), SW_SHOWDEFAULT);
				exit(0);
			}
			sdl.gotfuncs=FALSE;
		}
#else
		if(sdl.Init(SDL_INIT_VIDEO))
			sdl.gotfuncs=FALSE;
#endif
	}
	if(sdl.VideoDriverName(drivername, sizeof(drivername))!=NULL) {
		/* Unacceptable drivers */
		if(!strcmp(drivername,"aalib"))
			sdl.gotfuncs=FALSE;
		if(!strcmp(drivername,"dummy"))
			sdl.gotfuncs=FALSE;
	}

	if(sdl.gotfuncs) {
		mp.argc=argc;
		mp.argv=argv;

		sdl_key_pending=sdl.SDL_CreateSemaphore(0);
		sdl_init_complete=sdl.SDL_CreateSemaphore(0);
		sdl_ufunc_ret=sdl.SDL_CreateSemaphore(0);
		sdl_updlock=sdl.SDL_CreateMutex();
		sdl_keylock=sdl.SDL_CreateMutex();
		sdl_vstatlock=sdl.SDL_CreateMutex();
		sdl_ufunc_lock=sdl.SDL_CreateMutex();
#if !defined(NO_X) && defined(__unix__)
		sdl_pastebuf_set=sdl.SDL_CreateSemaphore(0);
		sdl_pastebuf_copied=sdl.SDL_CreateSemaphore(0);
		sdl_copybuf_mutex=sdl.SDL_CreateMutex();
#endif
		sdl.CreateThread(sdl_runmain, &mp);

		while(1) {
			if(sdl.WaitEvent(&ev)==1) {
				switch (ev.type) {
					case SDL_ACTIVEEVENT:		/* Focus change */
						break;
					case SDL_KEYDOWN:			/* Keypress */
						if(ev.key.keysym.unicode > 0 && ev.key.keysym.unicode <= 0x7f) {		/* ASCII Key (Whoopee!) */
							/* ALT-TAB stuff doesn't work correctly inder Win32,
							 * seems ot pass a whole slew of TABs though here.
							 * Kludge-fix 'em by ignoring all ALT-TAB keystrokes
							 * that appear to be a tab */
							if(ev.key.keysym.unicode=='\t' && ev.key.keysym.mod & KMOD_ALT)
								break;
							/* Need magical handling here... 
							 * if ALT is pressed, run 'er through 
							 * sdl_get_char_code() ANYWAYS unless
							 * both right ALT and left controll are
							 * pressed in which case it may be an
							 * AltGr combo */
							if((ev.key.keysym.mod & (KMOD_RALT))==(KMOD_RALT)) {
								sdl_add_key(ev.key.keysym.unicode);
							}
							else if(ev.key.keysym.mod & (KMOD_META|KMOD_ALT)) {
								sdl_add_key(sdl_get_char_code(ev.key.keysym.sym, ev.key.keysym.mod, ev.key.keysym.unicode));
							}
							else {
								sdl_add_key(ev.key.keysym.unicode&0x7f);
							}
						}
						else 
							if((ev.key.keysym.mod & KMOD_NUM) && ev.key.keysym.sym >= SDLK_KP0 && ev.key.keysym.sym <= SDLK_KP9) {
								sdl_add_key(ev.key.keysym.sym - SDLK_KP0 + '0');
							}
							else if((ev.key.keysym.mod & KMOD_NUM) && ev.key.keysym.sym == SDLK_KP_PERIOD) {
								sdl_add_key('.');
							}
							else {
								sdl_add_key(sdl_get_char_code(ev.key.keysym.sym, ev.key.keysym.mod, ev.key.keysym.unicode));
							}
						break;
					case SDL_KEYUP:				/* Ignored (handled in KEYDOWN event) */
						break;
					case SDL_MOUSEMOTION:
						if(!ciolib_mouse_initialized)
							break;
						sdl.mutexP(sdl_vstatlock);
						ciomouse_gotevent(CIOLIB_MOUSE_MOVE,ev.motion.x/(vstat.charwidth*vstat.scaling)+1,ev.motion.y/(vstat.charheight*vstat.scaling)+1);
						sdl.mutexV(sdl_vstatlock);
						break;
					case SDL_MOUSEBUTTONDOWN:
						if(!ciolib_mouse_initialized)
							break;
						switch(ev.button.button) {
							case SDL_BUTTON_LEFT:
								sdl.mutexP(sdl_vstatlock);
								ciomouse_gotevent(CIOLIB_BUTTON_PRESS(1),ev.button.x/(vstat.charwidth*vstat.scaling)+1,ev.button.y/(vstat.charheight*vstat.scaling)+1);
								sdl.mutexV(sdl_vstatlock);
								break;
							case SDL_BUTTON_MIDDLE:
								sdl.mutexP(sdl_vstatlock);
								ciomouse_gotevent(CIOLIB_BUTTON_PRESS(2),ev.button.x/(vstat.charwidth*vstat.scaling)+1,ev.button.y/(vstat.charheight*vstat.scaling)+1);
								sdl.mutexV(sdl_vstatlock);
								break;
							case SDL_BUTTON_RIGHT:
								sdl.mutexP(sdl_vstatlock);
								ciomouse_gotevent(CIOLIB_BUTTON_PRESS(3),ev.button.x/(vstat.charwidth*vstat.scaling)+1,ev.button.y/(vstat.charheight*vstat.scaling)+1);
								sdl.mutexV(sdl_vstatlock);
								break;
						}
						break;
					case SDL_MOUSEBUTTONUP:
						if(!ciolib_mouse_initialized)
							break;
						switch(ev.button.button) {
							case SDL_BUTTON_LEFT:
								sdl.mutexP(sdl_vstatlock);
								ciomouse_gotevent(CIOLIB_BUTTON_RELEASE(1),ev.button.x/(vstat.charwidth*vstat.scaling)+1,ev.button.y/(vstat.charheight*vstat.scaling)+1);
								sdl.mutexV(sdl_vstatlock);
								break;
							case SDL_BUTTON_MIDDLE:
								sdl.mutexP(sdl_vstatlock);
								ciomouse_gotevent(CIOLIB_BUTTON_RELEASE(2),ev.button.x/(vstat.charwidth*vstat.scaling)+1,ev.button.y/(vstat.charheight*vstat.scaling)+1);
								sdl.mutexV(sdl_vstatlock);
								break;
							case SDL_BUTTON_RIGHT:
								sdl.mutexP(sdl_vstatlock);
								ciomouse_gotevent(CIOLIB_BUTTON_RELEASE(3),ev.button.x/(vstat.charwidth*vstat.scaling)+1,ev.button.y/(vstat.charheight*vstat.scaling)+1);
								sdl.mutexV(sdl_vstatlock);
								break;
						}
						break;
					case SDL_QUIT:
						return(sdl_exitcode);
					case SDL_VIDEORESIZE:
						if(ev.resize.w > 0 && ev.resize.h > 0) {
							FREE_AND_NULL(last_vmem);
							sdl.mutexP(sdl_vstatlock);
							vstat.scaling=(int)(ev.resize.w/(vstat.charwidth*vstat.cols));
							if(vstat.scaling < 1)
								vstat.scaling=1;
							if(fullscreen)
								win=sdl.SetVideoMode(
									 vstat.charwidth*vstat.cols*vstat.scaling
									,vstat.charheight*vstat.rows*vstat.scaling
									,32
									,SDL_SWSURFACE|SDL_HWPALETTE|SDL_FULLSCREEN
								);
							else
								win=sdl.SetVideoMode(
									 vstat.charwidth*vstat.cols*vstat.scaling
									,vstat.charheight*vstat.rows*vstat.scaling
									,8
									,SDL_HWSURFACE|SDL_HWPALETTE|SDL_RESIZABLE
								);
							if(win!=NULL) {
	#if (defined(__MACH__) && defined(__APPLE__))
								char	driver[16];
								if(sdl.VideoDriverName(driver, sizeof(driver))!=NULL) {
									if(!strcmp(driver,"Quartz"))
										sdl_using_quartz=TRUE;
								}
	#endif

								if(sdl_cursor!=NULL)
									sdl.FreeSurface(sdl_cursor);
								sdl_cursor=sdl.CreateRGBSurface(SDL_SWSURFACE|SDL_SRCCOLORKEY, vstat.charwidth, vstat.charheight, 8, 0, 0, 0, 0);
						    	/* Update font. */
						    	sdl_load_font(NULL);
						    	sdl_setup_colours(win,0);
								sdl_full_screen_redraw();
							}
							else if(sdl_init_good) {
								ev.type=SDL_QUIT;
								sdl_exitcode=1;
								sdl.PeepEvents(&ev, 1, SDL_ADDEVENT, 0xffffffff);
							}
							sdl.mutexV(sdl_vstatlock);
						}
						break;
					case SDL_VIDEOEXPOSE:
						FREE_AND_NULL(last_vmem);
						sdl_full_screen_redraw();
						break;
					case SDL_USEREVENT: {
						/* Tell SDL to do various stuff... */
						switch(ev.user.code) {
							case SDL_USEREVENT_LOADFONT:
								FREE_AND_NULL(last_vmem);
								sdl_ufunc_retval=sdl_load_font((char *)ev.user.data1);
								FREE_AND_NULL(ev.user.data1);
								sdl.SemPost(sdl_ufunc_ret);
								/* Fallthough */
							case SDL_USEREVENT_UPDATERECT:
								sdl_full_screen_redraw();
								break;
							case SDL_USEREVENT_SETNAME:
								sdl.WM_SetCaption((char *)ev.user.data1,(char *)ev.user.data1);
								free(ev.user.data1);
								break;
							case SDL_USEREVENT_SETTITLE:
								sdl.WM_SetCaption((char *)ev.user.data1,NULL);
								free(ev.user.data1);
								break;
							case SDL_USEREVENT_SETVIDMODE:
								FREE_AND_NULL(last_vmem);
								sdl.mutexP(sdl_vstatlock);
								if(fullscreen)
									win=sdl.SetVideoMode(
										 vstat.charwidth*vstat.cols*vstat.scaling
										,vstat.charheight*vstat.rows*vstat.scaling
										,8
										,SDL_SWSURFACE|SDL_HWPALETTE|SDL_FULLSCREEN
									);
								else
									win=sdl.SetVideoMode(
										 vstat.charwidth*vstat.cols*vstat.scaling
										,vstat.charheight*vstat.rows*vstat.scaling
										,8
										,SDL_HWSURFACE|SDL_HWPALETTE|SDL_RESIZABLE
									);
								if(win!=NULL) {
	#if (defined(__MACH__) && defined(__APPLE__))
									char	driver[16];
									if(sdl.VideoDriverName(driver, sizeof(driver))!=NULL) {
										if(!strcmp(driver,"Quartz"))
											sdl_using_quartz=TRUE;
									}
	#endif
									vstat.scaling=(int)(win->w/(vstat.charwidth*vstat.cols));
									if(vstat.scaling < 1)
										vstat.scaling=1;
									sdl_setup_colours(win,0);
									if(sdl_cursor!=NULL)
										sdl.FreeSurface(sdl_cursor);
									sdl_cursor=sdl.CreateRGBSurface(SDL_SWSURFACE|SDL_SRCCOLORKEY, vstat.charwidth, vstat.charheight, 8, 0, 0, 0, 0);
									/* Update font. */
									sdl_load_font(NULL);
									sdl_full_screen_redraw();
								}
								else if(sdl_init_good) {
									ev.type=SDL_QUIT;
									sdl_exitcode=1;
									sdl.PeepEvents(&ev, 1, SDL_ADDEVENT, 0xffffffff);
								}
								free(ev.user.data1);
								free(ev.user.data2);
								sdl.mutexV(sdl_vstatlock);
								break;
							case SDL_USEREVENT_HIDEMOUSE:
								sdl.ShowCursor(SDL_DISABLE);
								break;
							case SDL_USEREVENT_SHOWMOUSE:
								sdl.ShowCursor(SDL_ENABLE);
								break;
							case SDL_USEREVENT_INIT:
								if(!sdl_init_good) {
									if(sdl.WasInit(SDL_INIT_VIDEO)==SDL_INIT_VIDEO) {
										if(win != NULL) {
											sdl.EnableUNICODE(1);
											sdl.EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

											sdl.CreateThread(sdl_blinker_thread, NULL);
											sdl.CreateThread(sdl_mouse_thread, NULL);
											sdl_init_good=1;
										}
									}
								}
								sdl.SemPost(sdl_init_complete);
								break;
							case SDL_USEREVENT_COPY:
	#if (defined(__MACH__) && defined(__APPLE__))
								if(sdl_using_quartz) {
									ScrapRef	scrap;
									sdl.mutexP(sdl_copybuf_mutex);
									if(sdl_copybuf!=NULL) {
										if(!ClearCurrentScrap()) {		/* purge the current contents of the scrap. */
											if(!GetCurrentScrap(&scrap)) {		/* obtain a reference to the current scrap. */
												PutScrapFlavor(scrap, kScrapFlavorTypeText, kScrapFlavorMaskTranslated /* kScrapFlavorMaskNone */, strlen(sdl_copybuf), sdl_copybuf); 		/* write the data to the scrap */
											}
										}
									}
									FREE_AND_NULL(sdl_copybuf);
									sdl.mutexV(sdl_copybuf_mutex);
									break;
								}
	#endif

	#if !defined(NO_X) && defined(__unix__)
								if(sdl_x11available && sdl_using_x11()) {
									SDL_SysWMinfo	wmi;

									SDL_VERSION(&(wmi.version));
									sdl.GetWMInfo(&wmi);
									sdl_x11.XSetSelectionOwner(wmi.info.x11.display, CONSOLE_CLIPBOARD, wmi.info.x11.window, CurrentTime);
									break;
								}
	#endif
								break;
							case SDL_USEREVENT_PASTE:
	#if (defined(__MACH__) && defined(__APPLE__))
								if(sdl_using_quartz) {
									ScrapRef	scrap;
									UInt32	fl;
									Size		scraplen;

									FREE_AND_NULL(sdl_pastebuf);
									if(!GetCurrentScrap(&scrap)) {		/* obtain a reference to the current scrap. */
										if(!GetScrapFlavorFlags(scrap, kScrapFlavorTypeText, &fl) && (fl & kScrapFlavorMaskTranslated)) {
											if(!GetScrapFlavorSize(scrap, kScrapFlavorTypeText, &scraplen)) {
												sdl_pastebuf=(char *)malloc(scraplen+1);
												if(sdl_pastebuf!=NULL) {
													if(GetScrapFlavorData(scrap, kScrapFlavorTypeText, &scraplen, sdl_pastebuf)) {
														FREE_AND_NULL(sdl_pastebuf);
													}
												}
											}
										}
									}
									sdl.SemPost(sdl_pastebuf_set);
									sdl.SemWait(sdl_pastebuf_copied);
									break;
								}
	#endif

	#if !defined(NO_X) && defined(__unix__)
								if(sdl_x11available && sdl_using_x11()) {
									Window sowner=None;
									SDL_SysWMinfo	wmi;

									SDL_VERSION(&(wmi.version));
									sdl.GetWMInfo(&wmi);

									sowner=sdl_x11.XGetSelectionOwner(wmi.info.x11.display, CONSOLE_CLIPBOARD);
									if(sowner==wmi.info.x11.window) {
										/* Get your own primary selection */
										if(sdl_copybuf==NULL) {
											FREE_AND_NULL(sdl_pastebuf);
										}
										else
											sdl_pastebuf=(unsigned char *)malloc(strlen(sdl_copybuf)+1);
										if(sdl_pastebuf!=NULL)
											strcpy(sdl_pastebuf,sdl_copybuf);
										/* Set paste buffer */
										sdl.SemPost(sdl_pastebuf_set);
										sdl.SemWait(sdl_pastebuf_copied);
										FREE_AND_NULL(sdl_pastebuf);
									}
									else if(sowner!=None) {
										sdl_x11.XConvertSelection(wmi.info.x11.display, CONSOLE_CLIPBOARD, XA_STRING, None, wmi.info.x11.window, CurrentTime);
									}
									else {
										/* Set paste buffer */
										FREE_AND_NULL(sdl_pastebuf);
										sdl.SemPost(sdl_pastebuf_set);
										sdl.SemWait(sdl_pastebuf_copied);
									}
									break;
								}
	#else
								break;
	#endif
						}
						break;
					}
					case SDL_SYSWMEVENT:			/* ToDo... This is where Copy/Paste needs doing */
	#if !defined(NO_X) && defined(__unix__)
						if(sdl_x11available && sdl_using_x11()) {
							XEvent *e;
							e=&ev.syswm.msg->event.xevent;
							switch(e->type) {
								case SelectionClear: {
										XSelectionClearEvent *req;

										req=&(e->xselectionclear);
										sdl.mutexP(sdl_copybuf_mutex);
										if(req->selection==CONSOLE_CLIPBOARD) {
											FREE_AND_NULL(sdl_copybuf);
										}
										sdl.mutexV(sdl_copybuf_mutex);
										break;
								}
								case SelectionNotify: {
										int format=0;
										unsigned long len, bytes_left, dummy;
										Atom type;
										XSelectionEvent *req;
										SDL_SysWMinfo	wmi;

										SDL_VERSION(&(wmi.version));
										sdl.GetWMInfo(&wmi);
										req=&(e->xselection);
										if(req->requestor!=wmi.info.x11.window)
											break;
										sdl_x11.XGetWindowProperty(wmi.info.x11.display, wmi.info.x11.window, req->property, 0, 0, 0, AnyPropertyType, &type, &format, &len, &bytes_left, (unsigned char **)(&sdl_pastebuf));
										if(bytes_left > 0 && format==8)
											sdl_x11.XGetWindowProperty(wmi.info.x11.display, wmi.info.x11.window, req->property,0,bytes_left,0,AnyPropertyType,&type,&format,&len,&dummy,(unsigned char **)&sdl_pastebuf);
										else {
											FREE_AND_NULL(sdl_pastebuf);
										}

										/* Set paste buffer */
										sdl.SemPost(sdl_pastebuf_set);
										sdl.SemWait(sdl_pastebuf_copied);
										if(sdl_pastebuf!=NULL) {
											sdl_x11.XFree(sdl_pastebuf);
											sdl_pastebuf=NULL;
										}
										break;
								}
								case SelectionRequest: {
										XSelectionRequestEvent *req;
										XEvent respond;

										req=&(e->xselectionrequest);
										sdl.mutexP(sdl_copybuf_mutex);
										if(sdl_copybuf==NULL) {
											respond.xselection.property=None;
										}
										else {
											if(req->target==XA_STRING) {
												sdl_x11.XChangeProperty(req->display, req->requestor, req->property, XA_STRING, 8, PropModeReplace, (unsigned char *)sdl_copybuf, strlen(sdl_copybuf));
												respond.xselection.property=req->property;
											}
											else
												respond.xselection.property=None;
										}
										sdl.mutexV(sdl_copybuf_mutex);
										respond.xselection.type=SelectionNotify;
										respond.xselection.display=req->display;
										respond.xselection.requestor=req->requestor;
										respond.xselection.selection=req->selection;
										respond.xselection.target=req->target;
										respond.xselection.time=req->time;
										sdl_x11.XSendEvent(req->display,req->requestor,0,0,&respond);
										break;
								}
							}	/* switch */
						}	/* usingx11 */
	#endif				

					/* Ignore this stuff */
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
	else {
		return(CIOLIB_main(argc, argv));
	}
}
