#include <stdarg.h>
#include <stdio.h>		/* NULL */
#include <stdlib.h>
#include <string.h>

#include "gen_defs.h"
#include "genwrap.h"
#include "dirwrap.h"
#include "xpbeep.h"
#include "threadwrap.h"
#ifdef __unix__
#include <xp_dl.h>
#endif

#if (defined CIOLIB_IMPORTS)
 #undef CIOLIB_IMPORTS
#endif
#if (defined CIOLIB_EXPORTS)
 #undef CIOLIB_EXPORTS
#endif

#include "ciolib.h"
#include "utf8_codepages.h"
#include "vidmodes.h"
#define BITMAP_CIOLIB_DRIVER
#include "bitmap_con.h"

#include "SDL.h"

#include "sdlfuncs.h"

int bitmap_width,bitmap_height;

/* 256 bytes so I can cheat */
unsigned char		sdl_keybuf[256];		/* Keyboard buffer */
unsigned char		sdl_key=0;				/* Index into keybuf for next key in buffer */
unsigned char		sdl_keynext=0;			/* Index into keybuf for next free position */

int sdl_exitcode=0;

SDL_Window	*win=NULL;
SDL_Renderer	*renderer=NULL;
SDL_Texture	*texture=NULL;
pthread_mutex_t win_mutex;
SDL_Surface	*sdl_icon=NULL;

sem_t sdl_ufunc_ret;
sem_t sdl_ufunc_rec;
pthread_mutex_t sdl_ufunc_mtx;
int sdl_ufunc_retval;

sem_t sdl_flush_sem;
int pending_updates=0;

int fullscreen=0;

int	sdl_init_good=0;
pthread_mutex_t sdl_keylock;
sem_t sdl_key_pending;
static unsigned int sdl_pending_mousekeys=0;

static struct video_stats cvstat;

struct sdl_keyvals {
	int	keysym
		,key
		,shift
		,ctrl
		,alt;
};

static pthread_mutex_t sdl_headlock;
static struct rectlist *update_list = NULL;
static struct rectlist *update_list_tail = NULL;

enum {
	 SDL_USEREVENT_FLUSH
	,SDL_USEREVENT_SETTITLE
	,SDL_USEREVENT_SETNAME
	,SDL_USEREVENT_SETICON
	,SDL_USEREVENT_SETVIDMODE
	,SDL_USEREVENT_SHOWMOUSE
	,SDL_USEREVENT_HIDEMOUSE
	,SDL_USEREVENT_INIT
	,SDL_USEREVENT_QUIT
	,SDL_USEREVENT_GETWINPOS
};

const struct sdl_keyvals sdl_keyval[] =
{
	{SDLK_BACKSPACE, 0x08, 0x08, 0x7f, 0x0e00},
	{SDLK_TAB, 0x09, 0x0f00, 0x9400, 0xa500},
	{SDLK_RETURN, 0x0d, 0x0d, 0x0a, 0xa600},
	{SDLK_ESCAPE, 0x1b, 0x1b, 0x1b, 0x0100},
	{SDLK_SPACE, 0x20, 0x20, 0x0300, 0x20},
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
	{SDLK_KP_0, 0x5200, 0x5200, 0x9200, 0},
	{SDLK_KP_1, 0x4f00, 0x4f00, 0x7500, 0},
	{SDLK_KP_2, 0x5000, 0x5000, 0x9100, 0},
	{SDLK_KP_3, 0x5100, 0x5100, 0x7600, 0},
	{SDLK_KP_4, 0x4b00, 0x4b00, 0x7300, 0},
	{SDLK_KP_5, 0x4c00, 0x4c00, 0x8f00, 0},
	{SDLK_KP_6, 0x4d00, 0x4d00, 0x7400, 0},
	{SDLK_KP_7, 0x4700, 0x4700, 0x7700, 0},
	{SDLK_KP_8, 0x4800, 0x4800, 0x8d00, 0},
	{SDLK_KP_9, 0x4900, 0x4900, 0x8400, 0},
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
	{SDLK_QUOTE, '\'', '"', 0, 0x2800},
	{SDLK_COMMA, ',', '<', 0, 0x3300},
	{SDLK_PERIOD, '.', '>', 0, 0x3400},
	{SDLK_BACKQUOTE, '`', '~', 0, 0x2900},
	{0, 0, 0, 0, 0}	/** END **/
};

void sdl_video_event_thread(void *data);

static void sdl_user_func(int func, ...)
{
	va_list argptr;
	SDL_Event	ev;
	int rv;

	ev.type=SDL_USEREVENT;
	ev.user.data1=NULL;
	ev.user.data2=NULL;
	ev.user.code=func;
	pthread_mutex_lock(&sdl_ufunc_mtx);
	while (1) {
		va_start(argptr, func);
		switch(func) {
			case SDL_USEREVENT_SETICON:
				ev.user.data1=va_arg(argptr, void *);
				if((ev.user.data2=(unsigned long *)malloc(sizeof(unsigned long)))==NULL) {
					pthread_mutex_unlock(&sdl_ufunc_mtx);
					va_end(argptr);
					return;
				}
				*(unsigned long *)ev.user.data2=va_arg(argptr, unsigned long);
				break;
			case SDL_USEREVENT_SETNAME:
			case SDL_USEREVENT_SETTITLE:
				if((ev.user.data1=strdup(va_arg(argptr, char *)))==NULL) {
					pthread_mutex_unlock(&sdl_ufunc_mtx);
					va_end(argptr);
					return;
				}
				break;
			case SDL_USEREVENT_SHOWMOUSE:
			case SDL_USEREVENT_HIDEMOUSE:
			case SDL_USEREVENT_FLUSH:
				break;
			default:
				va_end(argptr);
				return;
		}
		va_end(argptr);
		while((rv = sdl.PeepEvents(&ev, 1, SDL_ADDEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT))!=1)
			YIELD();
		break;
	}
	pthread_mutex_unlock(&sdl_ufunc_mtx);
}

/* Called from main thread only */
static int sdl_user_func_ret(int func, ...)
{
	int rv;
	va_list argptr;
	SDL_Event	ev;

	ev.type=SDL_USEREVENT;
	ev.user.data1=NULL;
	ev.user.data2=NULL;
	ev.user.code=func;
	va_start(argptr, func);
	pthread_mutex_lock(&sdl_ufunc_mtx);
	/* Drain the swamp */
	while(1) {
		switch(func) {
			case SDL_USEREVENT_GETWINPOS:
				ev.user.data1 = va_arg(argptr, void *);
				ev.user.data2 = va_arg(argptr, void *);
				// Fallthrough
			case SDL_USEREVENT_SETVIDMODE:
			case SDL_USEREVENT_INIT:
			case SDL_USEREVENT_QUIT:
				while(sdl.PeepEvents(&ev, 1, SDL_ADDEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT)!=1)
					YIELD();
				break;
			default:
				pthread_mutex_unlock(&sdl_ufunc_mtx);
				va_end(argptr);
				return -1;
		}
		rv = sem_wait(&sdl_ufunc_ret);
		if(rv==0)
			break;
	}
	pthread_mutex_unlock(&sdl_ufunc_mtx);
	va_end(argptr);
	return(sdl_ufunc_retval);
}

void exit_sdl_con(void)
{
	// Avoid calling exit(0) from an atexit() function...
	ciolib_reaper = 0;
	sdl_user_func_ret(SDL_USEREVENT_QUIT);
}

void sdl_copytext(const char *text, size_t buflen)
{
	sdl.SetClipboardText(text);
}

char *sdl_getcliptext(void)
{
	uint8_t *u8;
	u8 = (uint8_t *)sdl.GetClipboardText();
	char *ret = strdup((char *)u8);
	sdl.free(u8);
	return ret;
}

void sdl_drawrect(struct rectlist *data)
{
	if(sdl_init_good) {
		data->next = NULL;
		pthread_mutex_lock(&sdl_headlock);
		if (update_list == NULL)
			update_list = update_list_tail = data;
		else {
			update_list_tail->next = data;
			update_list_tail = data;
		}
		pthread_mutex_unlock(&sdl_headlock);
	}
	else
		bitmap_drv_free_rect(data);
}

void sdl_flush(void)
{
	sdl_user_func(SDL_USEREVENT_FLUSH);
}

static int sdl_init_mode(int mode)
{
	int oldcols;

	if (mode != CIOLIB_MODE_CUSTOM) {
		pthread_mutex_lock(&vstatlock);
		if (mode == vstat.mode) {
			pthread_mutex_unlock(&vstatlock);
			return 0;
		}
		pthread_mutex_unlock(&vstatlock);
	}

	sdl_user_func(SDL_USEREVENT_FLUSH);

	pthread_mutex_lock(&blinker_lock);
	pthread_mutex_lock(&vstatlock);
	oldcols = cvstat.cols;
	bitmap_drv_init_mode(mode, &bitmap_width, &bitmap_height);
	vstat.winwidth = ((double)cvstat.winwidth / (cvstat.cols * cvstat.charwidth)) * (vstat.cols * vstat.charwidth);
	vstat.winheight = ((double)cvstat.winheight / (cvstat.rows * cvstat.charheight * cvstat.vmultiplier)) * (vstat.rows * vstat.charheight * vstat.vmultiplier);
	if (oldcols != vstat.cols) {
		if (oldcols == 40) {
			vstat.winwidth /= 2;
			vstat.winheight /= 2;
		}
		if (vstat.cols == 40) {
			vstat.winwidth *= 2;
			vstat.winheight *= 2;
		}
	}
	if (vstat.winwidth < vstat.charwidth * vstat.cols)
		vstat.winwidth = vstat.charwidth * vstat.cols;
	if (vstat.winheight < vstat.charheight * vstat.rows)
		vstat.winheight = vstat.charheight * vstat.rows;
	if(vstat.vmultiplier < 1)
		vstat.vmultiplier = 1;

	cvstat = vstat;
	pthread_mutex_unlock(&vstatlock);
	pthread_mutex_unlock(&blinker_lock);

	sdl_user_func_ret(SDL_USEREVENT_SETVIDMODE);

	return(0);
}

/* Called from main thread only (Passes Event) */
int sdl_init(int mode)
{
	bitmap_drv_init(sdl_drawrect, sdl_flush);

	if(mode==CIOLIB_MODE_SDL_FULLSCREEN)
		fullscreen=1;
	// Needs to be *after* bitmap_drv_init()
#if defined(__DARWIN__)
	sem_post(&startsdl_sem);
#else
	_beginthread(sdl_video_event_thread, 0, NULL);
#endif
	sdl_user_func_ret(SDL_USEREVENT_INIT);
	sdl_init_mode(3);

	if(sdl_init_good) {
		cio_api.mode=fullscreen?CIOLIB_MODE_SDL_FULLSCREEN:CIOLIB_MODE_SDL;
#ifdef _WIN32
		FreeConsole();
#endif
		cio_api.options |= CONIO_OPT_PALETTE_SETTING | CONIO_OPT_SET_TITLE | CONIO_OPT_SET_NAME | CONIO_OPT_SET_ICON;
		return(0);
	}

	ciolib_reaper = 0;
	sdl_user_func_ret(SDL_USEREVENT_QUIT);
	return(-1);
}

void sdl_setwinsize_locked(int w, int h)
{
	if (w > 16384)
		w = 16384;
	if (h > 16384)
		h = 16384;
	if (w < cvstat.charwidth * cvstat.cols)
		w = cvstat.charwidth * cvstat.cols;
	if (h < cvstat.charheight * cvstat.rows)
		h = cvstat.charheight * cvstat.rows;
	cvstat.winwidth = vstat.winwidth = w;
	cvstat.winheight = vstat.winheight = h;
}

void sdl_setwinsize(int w, int h)
{
	pthread_mutex_lock(&vstatlock);
	sdl_setwinsize_locked(w, h);
	pthread_mutex_unlock(&vstatlock);
}

void sdl_setwinposition(int x, int y)
{
	pthread_mutex_lock(&win_mutex);
	sdl.SetWindowPosition(win, x, y);
	pthread_mutex_unlock(&win_mutex);
}

void sdl_getwinsize_locked(int *w, int *h)
{
	if (w)
		*w = cvstat.winwidth;
	if (h)
		*h = cvstat.winheight;
}

void sdl_getwinsize(int *w, int *h)
{
	pthread_mutex_lock(&vstatlock);
	sdl_getwinsize_locked(w, h);
	pthread_mutex_unlock(&vstatlock);
}

/* Called from main thread only */
int sdl_kbhit(void)
{
	int ret;

	pthread_mutex_lock(&sdl_keylock);
	ret=(sdl_key!=sdl_keynext);
	pthread_mutex_unlock(&sdl_keylock);
	return(ret);
}

/* Called from main thread only */
int sdl_getch(void)
{
	int ch;

	sem_wait(&sdl_key_pending);
	pthread_mutex_lock(&sdl_keylock);

	/* This always frees up space in keybuf for one more char */
	ch=sdl_keybuf[sdl_key++];
	/* If we have missed mouse keys, tack them on to the end of the buffer now */
	if(sdl_pending_mousekeys) {
		if(sdl_pending_mousekeys & 1)	/* Odd number... second char */
	       	sdl_keybuf[sdl_keynext++]=CIO_KEY_MOUSE >> 8;
		else							/* Even number... first char */
	        sdl_keybuf[sdl_keynext++]=CIO_KEY_MOUSE & 0xff;
        sem_post(&sdl_key_pending);
		sdl_pending_mousekeys--;
	}
	pthread_mutex_unlock(&sdl_keylock);
	return(ch);
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
int sdl_seticon(const void *icon, unsigned long size)
{
	sdl_user_func(SDL_USEREVENT_SETICON,icon,size);
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

int sdl_get_window_info(int *width, int *height, int *xpos, int *ypos)
{
	int wx, wy;

	if (xpos || ypos) {
		sdl_user_func_ret(SDL_USEREVENT_GETWINPOS, &wx, &wy);
		if(xpos)
			*xpos=wx;
		if(ypos)
			*ypos=wy;
	}

	if (width || height) {
		pthread_mutex_lock(&vstatlock);
		if(width)
			*width=cvstat.winwidth;
		if(height)
			*height=cvstat.winheight;
		pthread_mutex_unlock(&vstatlock);
	}

	return(1);
}

static void setup_surfaces_locked(void)
{
	int		flags=0;
	SDL_Event	ev;
	int charwidth, charheight, cols, rows, vmultiplier;

	if(fullscreen)
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	else
		flags |= SDL_WINDOW_RESIZABLE;

	pthread_mutex_lock(&win_mutex);
	charwidth = cvstat.charwidth;
	charheight = cvstat.charheight;
	cols = cvstat.cols;
	rows = cvstat.rows;
	vmultiplier = cvstat.vmultiplier;

	if (win == NULL) {
		// SDL2: This is slow sometimes... not sure why.
		if (sdl.CreateWindowAndRenderer(cvstat.winwidth, cvstat.winheight, flags, &win, &renderer) == 0) {
			if (texture) 
				sdl.DestroyTexture(texture);
			sdl.RenderClear(renderer);
			texture = sdl.CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, charwidth*cols, charheight*rows);
		}
		else {
			win = NULL;
			renderer = NULL;
		}
	}
	else {
		sdl.SetWindowMinimumSize(win, cvstat.charwidth * cvstat.cols, cvstat.charheight * cvstat.rows);
		sdl.SetWindowSize(win, cvstat.winwidth, cvstat.winheight);
		if (texture) 
			sdl.DestroyTexture(texture);
		texture = sdl.CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, charwidth*cols, charheight*rows);
	}
	sdl.SetWindowMinimumSize(win, cvstat.charwidth * cvstat.cols, cvstat.charheight * cvstat.rows);

	if(win!=NULL) {
		bitmap_drv_request_pixels();
	}
	else if(sdl_init_good) {
		ev.type=SDL_QUIT;
		sdl_exitcode=1;
		sdl.PeepEvents(&ev, 1, SDL_ADDEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);
	}
	pthread_mutex_unlock(&win_mutex);
}

static void setup_surfaces(void)
{
	pthread_mutex_lock(&vstatlock);
	setup_surfaces_locked();
	pthread_mutex_unlock(&vstatlock);
}

/* Called from event thread only */
static void sdl_add_key(unsigned int keyval)
{
	if(keyval==0xa600) {
		fullscreen=!fullscreen;
		cio_api.mode=fullscreen?CIOLIB_MODE_SDL_FULLSCREEN:CIOLIB_MODE_SDL;
		sdl.SetWindowFullscreen(win, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
		setup_surfaces();
		return;
	}
	if(keyval <= 0xffff) {
		pthread_mutex_lock(&sdl_keylock);
		if(sdl_keynext+1==sdl_key) {
			beep();
			pthread_mutex_unlock(&sdl_keylock);
			return;
		}
		if((sdl_keynext+2==sdl_key) && keyval > 0xff) {
			if(keyval==CIO_KEY_MOUSE)
				sdl_pending_mousekeys+=2;
			else
				beep();
			pthread_mutex_unlock(&sdl_keylock);
			return;
		}
		sdl_keybuf[sdl_keynext++]=keyval & 0xff;
		sem_post(&sdl_key_pending);
		if(keyval>0xff) {
			sdl_keybuf[sdl_keynext++]=keyval >> 8;
			sem_post(&sdl_key_pending);
		}
		pthread_mutex_unlock(&sdl_keylock);
	}
}

/* Called from event thread only */
static unsigned int sdl_get_char_code(unsigned int keysym, unsigned int mod)
{
	int expect;
	int i;

	/* We don't handle META */
	if (mod & KMOD_GUI)
		return(0x0001ffff);

	/* Glah! */
#ifdef __DARWIN__
	if(keysym==0x7f && !(mod & KMOD_CTRL)) {
		keysym=0x08;
		keysym=SDLK_BACKSPACE;
	}
#endif

	/* Find the SDL keysym */
	for(i=0;sdl_keyval[i].keysym;i++) {
		if(sdl_keyval[i].keysym==keysym) {
			/* KeySym found in table */

			/*
			 * Using the modifiers, look up the expected scan code.
			 */
			if(mod & KMOD_CTRL)
				expect=sdl_keyval[i].ctrl;
			else if(mod & KMOD_SHIFT) {
				if(mod & KMOD_CAPS)
					expect=sdl_keyval[i].key;
				else
					expect=sdl_keyval[i].shift;
			}
			else {
				if(mod & KMOD_CAPS && (toupper(sdl_keyval[i].key) == sdl_keyval[i].shift))
					expect=sdl_keyval[i].shift;
				else
					expect=sdl_keyval[i].key;
			}

			/*
			 * Now handle the ALT case so that expect will
			 * be what we expect to return
			 */
			if(mod & KMOD_ALT) {
				/* Yes, this is a "normal" ALT combo */
				if(keysym==expect)
					return(sdl_keyval[i].alt);

				/* AltGr apparently... give up */
				return(0x0001ffff);
			}

			return(expect);
		}
	}
	/*
	 * Well, we can't find it in our table, or it's a regular key.
	 */
	if(keysym > 0 && keysym < 128) {
		if (isalpha(keysym)) {
			/*
			 * If CAPS and SHIFT are not in the same state,
			 * upper-case.
			 */
			if(!!(mod & KMOD_CAPS) != !!(mod & KMOD_SHIFT))
				return toupper(keysym);
		}
		return keysym;
	}

	/* Give up.  It's not working out for us. */
	return(0x0001ffff);
}

static void
sdl_add_keys(uint8_t *utf8s)
{
	char *chars;
	char *p;

	chars = utf8_to_cp(getcodepage(), utf8s, '\x00', strlen((char *)utf8s), NULL);
	if (chars) {
		for (p = chars; *p; p++) {
			sdl_add_key(*((uint8_t *)p));
		}
		free(chars);
	}
}

/* Mouse event/keyboard thread */
static void sdl_mouse_thread(void *data)
{
	SetThreadName("SDL Mouse");
	while(1) {
		if(mouse_wait())
			sdl_add_key(CIO_KEY_MOUSE);
	}
}

static int win_to_text_xpos(int winpos)
{
	int ret;

	pthread_mutex_lock(&vstatlock);
	ret = winpos/(((float)cvstat.winwidth)/cvstat.cols)+1;
	if (ret > cvstat.cols)
		ret = cvstat.cols;
	if (ret < 1)
		ret = 1;
	pthread_mutex_unlock(&vstatlock);
	return ret;
}

static int win_to_text_ypos(int winpos)
{
	int ret;

	pthread_mutex_lock(&vstatlock);
	ret = winpos/(((float)cvstat.winheight)/cvstat.rows)+1;
	if (ret > cvstat.rows)
		ret = cvstat.rows;
	if (ret < 1)
		ret = 1;
	pthread_mutex_unlock(&vstatlock);
	return ret;
}

void sdl_video_event_thread(void *data)
{
	SDL_Event	ev;
	int		old_w, old_h;
	int		block_text = 0;

	pthread_mutex_lock(&vstatlock);
	old_w = cvstat.winwidth;
	old_h = cvstat.winheight;
	pthread_mutex_unlock(&vstatlock);

	while(1) {
		if(sdl.WaitEventTimeout(&ev, 1)!=1) {
			pthread_mutex_lock(&vstatlock);
			if (cvstat.winwidth != old_w || cvstat.winheight != old_h) {
				sdl_setwinsize_locked(cvstat.winwidth, cvstat.winheight);
				setup_surfaces_locked();
				old_w = cvstat.winwidth;
				old_h = cvstat.winheight;
				sdl_getwinsize_locked(&cvstat.winwidth, &cvstat.winheight);
			}
			pthread_mutex_unlock(&vstatlock);
		}
		else {
			switch (ev.type) {
				case SDL_KEYDOWN:			/* Keypress */
					if ((ev.key.keysym.mod & (KMOD_CTRL|KMOD_ALT|KMOD_GUI)) && !(ev.key.keysym.mod & KMOD_MODE)) {
						block_text = 1;
						if ((ev.key.keysym.mod & KMOD_ALT) &&
						    (ev.key.keysym.sym == SDLK_LEFT ||
						     ev.key.keysym.sym == SDLK_RIGHT ||
						     ev.key.keysym.sym == SDLK_UP ||
						     ev.key.keysym.sym == SDLK_DOWN)) {
							int w, h;
							pthread_mutex_lock(&vstatlock);
							w = cvstat.winwidth;
							h = cvstat.winheight;
							switch(ev.key.keysym.sym) {
								case SDLK_LEFT:
									if (w % (cvstat.charwidth * cvstat.cols)) {
										w = w - w % (cvstat.charwidth * cvstat.cols);
									}
									else {
										w -= (cvstat.charwidth * cvstat.cols);
										if (w < (cvstat.charwidth * cvstat.cols))
											w = cvstat.charwidth * cvstat.cols;
									}
									break;
								case SDLK_RIGHT:
									w = (w - w % (cvstat.charwidth * cvstat.cols)) + (cvstat.charwidth * cvstat.cols);
									break;
								case SDLK_UP:
									if (h % (cvstat.charheight * cvstat.rows * cvstat.vmultiplier)) {
										h = h - h % (cvstat.charheight * cvstat.rows * cvstat.vmultiplier);
									}
									else {
										h -= (cvstat.charheight * cvstat.rows * cvstat.vmultiplier);
										if (h < (cvstat.charheight * cvstat.rows * cvstat.vmultiplier))
											h = cvstat.charheight * cvstat.rows * cvstat.vmultiplier;
									}
									break;
								case SDLK_DOWN:
									h = (h - h % (cvstat.charheight * cvstat.rows * cvstat.vmultiplier)) + (cvstat.charheight * cvstat.rows * cvstat.vmultiplier);
									break;
							}
							if (w > 16384 || h > 16384)
								beep();
							else {
								cvstat.winwidth = w;
								cvstat.winheight = h;
							}
							pthread_mutex_unlock(&vstatlock);
							break;
						}
					}
					if (block_text || ev.key.keysym.sym < 0 || ev.key.keysym.sym > 128) {
						sdl_add_key(sdl_get_char_code(ev.key.keysym.sym, ev.key.keysym.mod));
					}
					else if (!isprint(ev.key.keysym.sym)) {
						if (ev.key.keysym.sym < 127)
							sdl_add_key(ev.key.keysym.sym);
					}
					break;
				case SDL_TEXTINPUT:
					if (!block_text)
						sdl_add_keys((uint8_t *)ev.text.text);
					break;
				case SDL_KEYUP:
					if (!(ev.key.keysym.mod & (KMOD_CTRL|KMOD_ALT|KMOD_GUI)))
						block_text = 0;
					break;
				case SDL_MOUSEMOTION:
					if(!ciolib_mouse_initialized)
						break;
					ciomouse_gotevent(CIOLIB_MOUSE_MOVE,win_to_text_xpos(ev.motion.x),win_to_text_ypos(ev.motion.y));
					break;
				case SDL_MOUSEBUTTONDOWN:
					if(!ciolib_mouse_initialized)
						break;
					switch(ev.button.button) {
						case SDL_BUTTON_LEFT:
							ciomouse_gotevent(CIOLIB_BUTTON_PRESS(1),win_to_text_xpos(ev.button.x),win_to_text_ypos(ev.button.y));
							break;
						case SDL_BUTTON_MIDDLE:
							ciomouse_gotevent(CIOLIB_BUTTON_PRESS(2),win_to_text_xpos(ev.button.x),win_to_text_ypos(ev.button.y));
							break;
						case SDL_BUTTON_RIGHT:
							ciomouse_gotevent(CIOLIB_BUTTON_PRESS(3),win_to_text_xpos(ev.button.x),win_to_text_ypos(ev.button.y));
							break;
					}
					break;
				case SDL_MOUSEBUTTONUP:
					if(!ciolib_mouse_initialized)
						break;
					switch(ev.button.button) {
						case SDL_BUTTON_LEFT:
							ciomouse_gotevent(CIOLIB_BUTTON_RELEASE(1),win_to_text_xpos(ev.button.x),win_to_text_ypos(ev.button.y));
							break;
						case SDL_BUTTON_MIDDLE:
							ciomouse_gotevent(CIOLIB_BUTTON_RELEASE(2),win_to_text_xpos(ev.button.x),win_to_text_ypos(ev.button.y));
							break;
						case SDL_BUTTON_RIGHT:
							ciomouse_gotevent(CIOLIB_BUTTON_RELEASE(3),win_to_text_xpos(ev.button.x),win_to_text_ypos(ev.button.y));
							break;
					}
					break;
				case SDL_QUIT:
					/*
					 * SDL2: Do we still need the reaper?
					 * This is what exit()s programs when the
					 * X is hit.
					 */
					if (ciolib_reaper)
						sdl_user_func(SDL_USEREVENT_QUIT);
					else
						sdl_add_key(CIO_KEY_QUIT);
					break;
				case SDL_WINDOWEVENT:
					switch(ev.window.event) {
						case SDL_WINDOWEVENT_SIZE_CHANGED:
							// SDL2: User resized window
						case SDL_WINDOWEVENT_RESIZED:
							{
								// SDL2: Something resized window
								const char *newh;

								pthread_mutex_lock(&vstatlock);
								if ((ev.window.data1 % (cvstat.charwidth * cvstat.cols)) || (ev.window.data2 % (cvstat.charheight * cvstat.rows)))
									newh = "2";
								else
									newh = "0";
								pthread_mutex_lock(&win_mutex);
								if (ev.window.event == SDL_WINDOWEVENT_RESIZED)
									sdl.GetWindowSize(win, &cvstat.winwidth, &cvstat.winheight);
								if (strcmp(newh, sdl.GetHint(SDL_HINT_RENDER_SCALE_QUALITY))) {
									sdl.SetHint(SDL_HINT_RENDER_SCALE_QUALITY, newh);
									sdl.DestroyTexture(texture);
									texture = sdl.CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, cvstat.charwidth*cvstat.cols, cvstat.charheight*cvstat.rows);
									bitmap_drv_request_pixels();
								}
								pthread_mutex_unlock(&win_mutex);
								pthread_mutex_unlock(&vstatlock);
								break;
							}
						case SDL_WINDOWEVENT_EXPOSED:
							bitmap_drv_request_pixels();
							break;
					}
					break;
				case SDL_USEREVENT: {
					struct rectlist *list;
					struct rectlist *old_next;
					switch(ev.user.code) {
						case SDL_USEREVENT_QUIT:
							sdl_ufunc_retval=0;
							if (ciolib_reaper)
								exit(0);
							sem_post(&sdl_ufunc_ret);
							return;
						case SDL_USEREVENT_FLUSH:
							pthread_mutex_lock(&win_mutex);
							if (win != NULL) {
								pthread_mutex_lock(&sdl_headlock);
								list = update_list;
								update_list = update_list_tail = NULL;
								pthread_mutex_unlock(&sdl_headlock);
								for (; list; list = old_next) {
									SDL_Rect src;

									old_next = list->next;
									if (list->next == NULL) {
										void *pixels;
										int pitch;
										int row;
										int tw, th;

										src.x = 0;
										src.y = 0;
										src.w = list->rect.width;
										src.h = list->rect.height;
										sdl.QueryTexture(texture, NULL, NULL, &tw, &th);
										sdl.LockTexture(texture, &src, &pixels, &pitch);
										if (pitch != list->rect.width * sizeof(list->data[0])) {
											// If this happens, we need to copy a row at a time...
											for (row = 0; row < list->rect.height && row < th; row++) {
												if (pitch < list->rect.width * sizeof(list->data[0]))
													memcpy(pixels, &list->data[list->rect.width * row], pitch);
												else
													memcpy(pixels, &list->data[list->rect.width * row], list->rect.width * sizeof(list->data[0]));
												pixels = (void *)((char*)pixels + pitch);
											}
										}
										else
											memcpy(pixels, list->data, list->rect.width * list->rect.height * sizeof(list->data[0]));
										sdl.UnlockTexture(texture);
										sdl.RenderCopy(renderer, texture, &src, NULL);
									}
									bitmap_drv_free_rect(list);
								}
								sdl.RenderPresent(renderer);
							}
							pthread_mutex_unlock(&win_mutex);
							break;
						case SDL_USEREVENT_SETNAME:
							pthread_mutex_lock(&win_mutex);
							sdl.SetWindowTitle(win, (char *)ev.user.data1);
							pthread_mutex_unlock(&win_mutex);
							free(ev.user.data1);
							break;
						case SDL_USEREVENT_SETICON:
							if(sdl_icon != NULL)
								sdl.FreeSurface(sdl_icon);
							sdl_icon=sdl.CreateRGBSurfaceFrom(ev.user.data1
									, *(unsigned long *)ev.user.data2
									, *(unsigned long *)ev.user.data2
									, 32
									, *(unsigned long *)ev.user.data2*4
									, *(DWORD *)"\377\0\0\0"
									, *(DWORD *)"\0\377\0\0"
									, *(DWORD *)"\0\0\377\0"
									, *(DWORD *)"\0\0\0\377"
							);
							pthread_mutex_lock(&win_mutex);
							sdl.SetWindowIcon(win, sdl_icon);
							pthread_mutex_unlock(&win_mutex);
							free(ev.user.data2);
							break;
						case SDL_USEREVENT_SETTITLE:
							pthread_mutex_lock(&win_mutex);
							sdl.SetWindowTitle(win, (char *)ev.user.data1);
							pthread_mutex_unlock(&win_mutex);
							free(ev.user.data1);
							break;
						case SDL_USEREVENT_SETVIDMODE:
							pthread_mutex_lock(&vstatlock);
							setup_surfaces_locked();
							old_w = cvstat.winwidth;
							old_h = cvstat.winheight;
							pthread_mutex_unlock(&vstatlock);
							sdl_ufunc_retval=0;
							sem_post(&sdl_ufunc_ret);
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
									pthread_mutex_lock(&win_mutex);
									_beginthread(sdl_mouse_thread, 0, NULL);
									sdl_init_good=1;
									pthread_mutex_unlock(&win_mutex);
								}
							}
							sdl_ufunc_retval=0;
							sem_post(&sdl_ufunc_ret);
							break;
						case SDL_USEREVENT_GETWINPOS:
							sdl.GetWindowPosition(win, ev.user.data1, ev.user.data2);
							sem_post(&sdl_ufunc_ret);
					}
					break;
				}
				case SDL_SYSWMEVENT:			/* ToDo... This is where Copy/Paste needs doing */

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
	return;
}

int sdl_initciolib(int mode)
{
#if defined(__DARWIN__)
	sem_post(&initsdl_sem);
	sem_wait(&initsdldone_sem);
	if (initsdl_ret) {
#else
	if(init_sdl_video()) {
#endif
		fprintf(stderr,"SDL Video Initialization Failed\n");
		return(-1);
	}
	sem_init(&sdl_key_pending, 0, 0);
	sem_init(&sdl_ufunc_ret, 0, 0);
	sem_init(&sdl_ufunc_rec, 0, 0);
	pthread_mutex_init(&sdl_ufunc_mtx, NULL);
	pthread_mutex_init(&sdl_headlock, NULL);
	pthread_mutex_init(&win_mutex, NULL);
	pthread_mutex_init(&sdl_keylock, NULL);
	return(sdl_init(mode));
}

void
sdl_beep(void)
{
        static unsigned char wave[2206];

	if (wave[2205] == 0) {
		xptone_makewave(440, wave, 2205, WAVE_SHAPE_SINE_SAW_HARM);
		wave[2205] = 1;
	}
        xp_play_sample(wave, 2205, TRUE);
}
