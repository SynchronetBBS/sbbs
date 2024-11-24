#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>		/* NULL */
#include <stdlib.h>
#include <string.h>

#include "gen_defs.h"
#include "genwrap.h"
#include "dirwrap.h"
#include "xpbeep.h"
#include "threadwrap.h"
#include <xp_dl.h>

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
#include "scale.h"

#include "SDL.h"

#include "sdlfuncs.h"
#include "sdl_con.h"

#ifdef _MSC_VER
#pragma warning(disable : 4244 4267 4018)
#endif

int bitmap_width,bitmap_height;

/* 256 bytes so I can cheat */
unsigned char		sdl_keybuf[256];		/* Keyboard buffer */
unsigned char		sdl_key=0;				/* Index into keybuf for next key in buffer */
unsigned char		sdl_keynext=0;			/* Index into keybuf for next free position */

int sdl_exitcode=0;

bool internal_scaling = true;	// Protected by the win mutex

SDL_Window	*win=NULL;
SDL_Cursor	*curs=NULL;
SDL_Renderer	*renderer=NULL;
SDL_Texture	*texture=NULL;
pthread_mutex_t win_mutex;
pthread_mutex_t sdl_mode_mutex;
bool sdl_mode;
SDL_Surface	*sdl_icon=NULL;

sem_t sdl_ufunc_ret;
sem_t sdl_ufunc_rec;
pthread_mutex_t sdl_ufunc_mtx;
int sdl_ufunc_retval;

sem_t sdl_flush_sem;
int pending_updates=0;

int fullscreen = 0;

int	sdl_init_good=0;
pthread_mutex_t sdl_keylock;
sem_t sdl_key_pending;
static unsigned int sdl_pending_mousekeys=0;
static bool sdl_sync_initialized;

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

#ifdef __DARWIN__
static int mac_width;
static int mac_height;
#define UPDATE_WINDOW_SIZE sdl.GetWindowSize(win, &mac_width, &mac_height)
#else
#define UPDATE_WINDOW_SIZE
#endif

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
	,SDL_USEREVENT_MOUSEPOINTER
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
	{SDLK_INSERT, CIO_KEY_IC, CIO_KEY_SHIFT_IC, CIO_KEY_CTRL_IC, CIO_KEY_ALT_IC},
	{SDLK_DELETE, CIO_KEY_DC, CIO_KEY_SHIFT_DC, CIO_KEY_CTRL_DC, CIO_KEY_ALT_DC},
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
static void setup_surfaces(struct video_stats *vs);
static int sdl_initsync(void);

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
			case SDL_USEREVENT_MOUSEPOINTER:
				ev.user.data1 = (void *)(intptr_t)va_arg(argptr, int);
				break;
			case SDL_USEREVENT_SHOWMOUSE:
			case SDL_USEREVENT_HIDEMOUSE:
			case SDL_USEREVENT_FLUSH:
				break;
			default:
				pthread_mutex_unlock(&sdl_ufunc_mtx);
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
			case SDL_USEREVENT_SETVIDMODE:
				ev.user.data1 = (void *)(intptr_t)va_arg(argptr, int);
				ev.user.data2 = (void *)(intptr_t)va_arg(argptr, int);
				while(sdl.PeepEvents(&ev, 1, SDL_ADDEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT)!=1)
					YIELD();
				break;
			case SDL_USEREVENT_GETWINPOS:
				ev.user.data1 = va_arg(argptr, void *);
				ev.user.data2 = va_arg(argptr, void *);
				// Fallthrough
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
	if (!sdl_sync_initialized)
		sdl_initsync();
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

static bool
sdl_get_bounds(int *w, int *h)
{
	SDL_Rect r;
	int ABUw, ABUh;
	int pixelw, pixelh;

	if (sdl.GetDisplayUsableBounds(0, &r) != 0)
		return false;
	sdl.GetWindowSize(win, &ABUw, &ABUh);
	sdl.GetWindowSizeInPixels(win, &pixelw, &pixelh);
	if (pixelw == 0 || pixelh == 0 || ABUw == 0 || ABUh == 0) {
		*w = r.w;
		*h = r.h;
		return true;
	}
	*w = ((uint64_t)r.w) * ABUw / pixelw;
	*h = ((uint64_t)r.h) * ABUh / pixelh;
	return true;
}

static int sdl_init_mode(int mode, bool init)
{
	int w, h;

	if (mode != CIOLIB_MODE_CUSTOM) {
		pthread_mutex_lock(&vstatlock);
		if (mode == vstat.mode && !init) {
			pthread_mutex_unlock(&vstatlock);
			return 0;
		}
		pthread_mutex_unlock(&vstatlock);
	}

	sdl_user_func(SDL_USEREVENT_FLUSH);

	pthread_mutex_lock(&vstatlock);
	if (!sdl_get_bounds(&w, &h)) {
		w = 0;
		h = 0;
	}
	bitmap_drv_init_mode(mode, &bitmap_width, &bitmap_height, w, h);
	if (ciolib_initial_scaling < 1.0) {
		ciolib_initial_scaling = vstat.scaling;
		if (ciolib_initial_scaling < 1.0)
			ciolib_initial_scaling = 1.0;
	}
	pthread_mutex_lock(&sdl_mode_mutex);
	sdl_mode = true;
	pthread_mutex_unlock(&sdl_mode_mutex);
	pthread_mutex_unlock(&vstatlock);

	sdl_user_func_ret(SDL_USEREVENT_SETVIDMODE, vstat.winwidth, vstat.winheight);

	return(0);
}

/* Called from main thread only (Passes Event) */
int sdl_init(int mode)
{
	bitmap_drv_init(sdl_drawrect, sdl_flush);

	if(mode==CIOLIB_MODE_SDL_FULLSCREEN)
		fullscreen = 1;
	// Needs to be *after* bitmap_drv_init()
#if defined(__DARWIN__)
	sem_post(&startsdl_sem);
#else
	_beginthread(sdl_video_event_thread, 0, NULL);
#endif
	sdl_user_func_ret(SDL_USEREVENT_INIT);
	sdl_init_mode(ciolib_initial_mode, true);

	if(sdl_init_good) {
		cio_api.mode=fullscreen?CIOLIB_MODE_SDL_FULLSCREEN:CIOLIB_MODE_SDL;
#ifdef _WIN32
		FreeConsole();
#endif
		cio_api.options |= CONIO_OPT_PALETTE_SETTING | CONIO_OPT_SET_TITLE | CONIO_OPT_SET_NAME | CONIO_OPT_SET_ICON | CONIO_OPT_EXTERNAL_SCALING;
		return(0);
	}

	ciolib_reaper = 0;
	sdl_user_func_ret(SDL_USEREVENT_QUIT);
	return(-1);
}

static void
update_cvstat(struct video_stats *vs)
{
	if (vs != NULL && vs != &vstat) {
		vstat.scaling = sdl_getscaling();
		pthread_mutex_lock(&vstatlock);
		*vs = vstat;
		pthread_mutex_unlock(&vstatlock);
	}
}

static void internal_setwinsize(struct video_stats *vs, bool force)
{
	int w, h;
	bool changed = true;

	w = vs->winwidth;
	h = vs->winheight;
	update_cvstat(vs);
	if (w > 16384)
		w = 16384;
	if (h > 16384)
		h = 16384;
	if (w < vs->scrnwidth)
		w = vs->scrnwidth;
	if (h < vs->scrnheight)
		h = vs->scrnheight;
	pthread_mutex_lock(&win_mutex);
	pthread_mutex_lock(&vstatlock);
	if (w == vstat.winwidth && h == vstat.winheight)
		changed = force;
	else {
		vs->winwidth = vstat.winwidth = w;
		vs->winheight = vstat.winheight = h;
	}
	if (changed) {
		pthread_mutex_unlock(&vstatlock);
		pthread_mutex_unlock(&win_mutex);
	}
	else {
		sdl.GetWindowSizeInPixels(win, &w, &h);
		UPDATE_WINDOW_SIZE;
		if (w != vs->winwidth || h != vs->winheight) {
			vs->winwidth = w;
			vs->winheight = h;
			changed = true;
		}
		pthread_mutex_unlock(&vstatlock);
		pthread_mutex_unlock(&win_mutex);
		vstat.scaling = sdl_getscaling();
	}
	if (changed)
		setup_surfaces(vs);
}

void sdl_setwinsize(int w, int h)
{
	sdl_user_func_ret(SDL_USEREVENT_SETVIDMODE, w, h);
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
		*w = vstat.winwidth;
	if (h)
		*h = vstat.winheight;
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
	sdl_init_mode(mode, false);
}

/* Called from main thread only (Passes Event) */
void sdl_setname(const char *name)
{
	sdl_user_func(SDL_USEREVENT_SETNAME,name);
}

/* Called from main thread only (Passes Event) */
void sdl_seticon(const void *icon, unsigned long size)
{
	sdl_user_func(SDL_USEREVENT_SETICON,icon,size);
}

/* Called from main thread only (Passes Event) */
void sdl_settitle(const char *title)
{
	sdl_user_func(SDL_USEREVENT_SETTITLE,title);
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
			*width=vstat.winwidth;
		if(height)
			*height=vstat.winheight;
		pthread_mutex_unlock(&vstatlock);
	}

	return(1);
}

static void
sdl_bughack_minsize(int w, int h, bool new)
{
	static int lw = -1;
	static int lh = -1;

	if ((!new) && w == lw && h == lh)
		return;
	lw = w;
	lh = h;
	sdl.SetWindowMinimumSize(win, w, h);
}

static void setup_surfaces(struct video_stats *vs)
{
	int		flags=0;
	SDL_Event	ev;
	SDL_Texture *newtexture;
	int idealw;
	int idealh;
	int idealmw;
	int idealmh;
	int new_win = false;

	if(fullscreen)
		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	else
		flags |= SDL_WINDOW_RESIZABLE;
#if (SDL_MINOR_VERSION > 0) || (SDL_PATCHLEVEL >= 1)
        flags |= SDL_WINDOW_ALLOW_HIGHDPI;
#endif
	pthread_mutex_lock(&vstatlock);
	bitmap_get_scaled_win_size(1.0, &idealmw, &idealmh, 0, 0);
	pthread_mutex_unlock(&vstatlock);
	pthread_mutex_lock(&win_mutex);
	idealw = vs->winwidth;
	idealh = vs->winheight;
	sdl.SetHint(SDL_HINT_RENDER_SCALE_QUALITY, internal_scaling ? "0" : "2");

	if (win == NULL) {
		// SDL2: This is slow sometimes... not sure why.
		new_win = true;
		if (sdl.CreateWindowAndRenderer(vs->winwidth, vs->winheight, flags, &win, &renderer) == 0) {
			sdl.GetWindowSizeInPixels(win, &idealw, &idealh);
			UPDATE_WINDOW_SIZE;
			vs->winwidth = idealw;
			vs->winheight = idealh;
			sdl.RenderClear(renderer);
			if (internal_scaling) {
				newtexture = sdl.CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, idealw, idealh);
			}
			else {
				newtexture = sdl.CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, vs->scrnwidth, vs->scrnheight);
			}

			if (texture)
				sdl.DestroyTexture(texture);
			texture = newtexture;
		}
		else {
			win = NULL;
			renderer = NULL;
		}
	}
	else {
		sdl_bughack_minsize(idealmw, idealmh, false);
		// Don't change window size when maximized...
		if ((sdl.GetWindowFlags(win) & SDL_WINDOW_MAXIMIZED) == 0) {
			int ABUw, ABUh;
			int pixelw, pixelh;
			uint64_t tmpw, tmph;

			// Workaround for systems where setWindowSize() units aren't pixels (ie: macOS)
			sdl.GetWindowSize(win, &ABUw, &ABUh);
			sdl.GetWindowSizeInPixels(win, &pixelw, &pixelh);
			tmpw = ((uint64_t)idealw) * ABUw / pixelw;
			tmph = ((uint64_t)idealh) * ABUh / pixelh;
			idealw = tmpw;
			idealh = tmph;
			sdl.SetWindowSize(win, idealw, idealh);
		}
		sdl.GetWindowSizeInPixels(win, &idealw, &idealh);
		UPDATE_WINDOW_SIZE;
		vs->winwidth = idealw;
		vs->winheight = idealh;
		if (internal_scaling) {
			newtexture = sdl.CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, idealw, idealh);
		}
		else {
			newtexture = sdl.CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, vs->scrnwidth, vs->scrnheight);
		}
		sdl.RenderClear(renderer);
		if (texture)
			sdl.DestroyTexture(texture);
		texture = newtexture;
	}
	if (vs != &vstat) {
		pthread_mutex_lock(&vstatlock);
		vstat.winwidth = vs->winwidth;
		vstat.winheight = vs->winheight;
		pthread_mutex_unlock(&vstatlock);
	}
	sdl_bughack_minsize(idealmw, idealmh, new_win);

	if(win!=NULL) {
		bitmap_drv_request_pixels();
	}
	else if(sdl_init_good) {
		ev.type=SDL_QUIT;
		sdl_exitcode=1;
		sdl.PeepEvents(&ev, 1, SDL_ADDEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);
	}
	pthread_mutex_unlock(&win_mutex);
	vstat.scaling = sdl_getscaling();
	update_cvstat(vs);
}

/* Called from event thread only */
static void sdl_add_key(unsigned int keyval, struct video_stats *vs)
{
	int w, h;

	if (keyval == 0xe0)
		keyval = CIO_KEY_LITERAL_E0;
	if(keyval==0xa600 && vs != NULL) {
		pthread_mutex_lock(&win_mutex);
		fullscreen = !(sdl.GetWindowFlags(win) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP));
		pthread_mutex_unlock(&win_mutex);
		cio_api.mode=fullscreen?CIOLIB_MODE_SDL_FULLSCREEN:CIOLIB_MODE_SDL;
		update_cvstat(vs);
		sdl.SetWindowFullscreen(win, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
		sdl.SetWindowResizable(win, fullscreen ? SDL_FALSE : SDL_TRUE);

		// Get current window size
		sdl.GetWindowSizeInPixels(win, &w, &h);
		UPDATE_WINDOW_SIZE;
		// Limit to max window size if available
		if (!fullscreen)
			sdl_get_bounds(&w, &h);
		// Set size based on current max
		vs->scaling = bitmap_double_mult_inside(w, h);
		bitmap_get_scaled_win_size(vs->scaling, &vs->winwidth, &vs->winheight, w, h);
		setup_surfaces(vs);
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
			if(keyval==CIO_KEY_MOUSE) {
				sdl_pending_mousekeys+=2;
				pthread_mutex_unlock(&sdl_keylock);
			}
			else {
				pthread_mutex_unlock(&sdl_keylock);
				beep();
			}
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

static void
sdl_add_key_uc(unsigned int keyval, struct video_stats *vs)
{
	if (keyval < 128)
		keyval = cpchar_from_unicode_cpoint(getcodepage(), keyval, keyval);
	sdl_add_key(keyval, vs);
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
			if(mod & KMOD_ALT)
				expect = sdl_keyval[i].alt;
			else if(mod & KMOD_CTRL)
				expect=sdl_keyval[i].ctrl;
			else if(mod & KMOD_SHIFT) {
				if((mod & KMOD_CAPS) && keysym != '\t')
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
sdl_add_keys(uint8_t *utf8s, struct video_stats *vs)
{
	char *chars;
	char *p;

	chars = utf8_to_cp(getcodepage(), utf8s, '\x00', strlen((char *)utf8s), NULL);
	if (chars) {
		for (p = chars; *p; p++) {
			sdl_add_key(*((uint8_t *)p), vs);
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
			sdl_add_key(CIO_KEY_MOUSE, NULL);
	}
}

static int win_to_res_xpos(int winpos, struct video_stats *vs)
{
	int ret;
	int w, h;

	bitmap_get_scaled_win_size(vs->scaling, &w, &h, 0, 0);
#ifdef __DARWIN__
	winpos = winpos * vs->winwidth / mac_width;
#endif
	int xoff = (vs->winwidth - w) / 2;
	if (xoff < 0)
		xoff = 0;
	if (winpos < xoff)
		winpos = 0;
	else
		winpos -= xoff;
	if (winpos >= w)
		winpos = w - 1;
	ret = winpos * vs->scrnwidth / w;
	return ret;
}

static int win_to_res_ypos(int winpos, struct video_stats *vs)
{
	int ret;
	int w, h;

	bitmap_get_scaled_win_size(vs->scaling, &w, &h, 0, 0);
#ifdef __DARWIN__
	winpos = winpos * vs->winheight / mac_height;
#endif
	int yoff = (vs->winheight - h) / 2;
	if (yoff < 0)
		yoff = 0;
	if (winpos < yoff)
		winpos = 0;
	else
		winpos -= yoff;
	if (winpos >= h)
		winpos = h - 1;
	ret = winpos * vs->scrnheight / h;
	return ret;
}

static int win_to_text_xpos(int winpos, struct video_stats *vs)
{
	winpos = win_to_res_xpos(winpos, vs);
	return (winpos * vs->cols / vs->scrnwidth) + 1;
}

static int win_to_text_ypos(int winpos, struct video_stats *vs)
{
	winpos = win_to_res_ypos(winpos, vs);
	return (winpos * vs->rows / vs->scrnheight) + 1;
}

void sdl_video_event_thread(void *data)
{
	SDL_Event	ev;
	int		block_text = 0;
	static SDL_Keycode last_sym = SDLK_UNKNOWN;
	static Uint16 last_mod = 0;
	struct video_stats cvstat = vstat;

	while(1) {
		if(sdl.WaitEventTimeout(&ev, 1)!=1)
			continue;
		switch (ev.type) {
			case SDL_KEYDOWN:			/* Keypress */
				last_mod = ev.key.keysym.mod;
				last_sym = ev.key.keysym.sym;
				if ((ev.key.keysym.mod & (KMOD_CTRL|KMOD_ALT|KMOD_GUI)) && !(ev.key.keysym.mod & KMOD_MODE)) {
					block_text = 1;
					if ((ev.key.keysym.mod & KMOD_ALT) &&
					    (ev.key.keysym.sym == SDLK_LEFT ||
					     ev.key.keysym.sym == SDLK_RIGHT)) {
						pthread_mutex_lock(&win_mutex);
						if (sdl.GetWindowFlags(win) & SDL_WINDOW_MAXIMIZED)
							sdl.RestoreWindow(win);
						pthread_mutex_unlock(&win_mutex);
						int w, h;
						SDL_Rect r;
						if (!sdl_get_bounds(&w, &h)) {
							w = 0;
							h = 0;
						}
						pthread_mutex_lock(&vstatlock);
						bitmap_snap(ev.key.keysym.sym == SDLK_RIGHT, w, h);
						pthread_mutex_unlock(&vstatlock);
						update_cvstat(&cvstat);
						setup_surfaces(&cvstat);
						break;
					}
				}
				if (ev.key.keysym.mod & KMOD_RALT) {	// Possible AltGr, let textinput sort it out...
					block_text = 0;
					break;
				}
				if ((ev.key.keysym.mod & KMOD_SHIFT) && (ev.key.keysym.sym == '\t'))
					block_text = 1;
				if (block_text || ev.key.keysym.sym < 0 || ev.key.keysym.sym > 127) {
					// NUMLOCK makes 
					if (ev.key.keysym.sym == SDLK_KP_DIVIDE
					    || ev.key.keysym.sym == SDLK_KP_MULTIPLY
					    || ev.key.keysym.sym == SDLK_KP_MINUS
					    || ev.key.keysym.sym == SDLK_KP_PLUS)
						break;
					if ((ev.key.keysym.mod & KMOD_NUM) && ((ev.key.keysym.sym >= SDLK_KP_1 && ev.key.keysym.sym <= SDLK_KP_0) || (ev.key.keysym.sym == SDLK_KP_PERIOD)))
						break;
					sdl_add_key_uc(sdl_get_char_code(ev.key.keysym.sym, ev.key.keysym.mod), &cvstat);
				}
				else if (!isprint(ev.key.keysym.sym)) {
					if (ev.key.keysym.sym < 128)
						sdl_add_key_uc(ev.key.keysym.sym, &cvstat);
				}
				break;
			case SDL_TEXTINPUT:
				if (!block_text) {
					unsigned int charcode = sdl_get_char_code(last_sym, last_mod & ~(KMOD_ALT));
					// If the key is exactly what we would expect, use sdl_get_char_code()
					if (*(uint8_t *)ev.text.text == charcode)
						sdl_add_key_uc(sdl_get_char_code(last_sym, last_mod), &cvstat);
					else
						sdl_add_keys((uint8_t *)ev.text.text, &cvstat);
				}
				break;
			case SDL_KEYUP:
				last_mod = ev.key.keysym.mod;
				if (!(ev.key.keysym.mod & (KMOD_CTRL|KMOD_ALT|KMOD_GUI)))
					block_text = 0;
				break;
			case SDL_MOUSEMOTION:
				pthread_once(&ciolib_mouse_initialized, init_mouse);
				ciomouse_gotevent(CIOLIB_MOUSE_MOVE,win_to_text_xpos(ev.motion.x, &cvstat),win_to_text_ypos(ev.motion.y, &cvstat), win_to_res_xpos(ev.motion.x, &cvstat), win_to_res_ypos(ev.motion.y, &cvstat));
				break;
			case SDL_MOUSEBUTTONDOWN:
				pthread_once(&ciolib_mouse_initialized, init_mouse);
				switch(ev.button.button) {
					case SDL_BUTTON_LEFT:
						ciomouse_gotevent(CIOLIB_BUTTON_PRESS(1),win_to_text_xpos(ev.button.x, &cvstat),win_to_text_ypos(ev.button.y, &cvstat), win_to_res_xpos(ev.button.x, &cvstat), win_to_res_ypos(ev.button.y, &cvstat));
						break;
					case SDL_BUTTON_MIDDLE:
						ciomouse_gotevent(CIOLIB_BUTTON_PRESS(2),win_to_text_xpos(ev.button.x, &cvstat),win_to_text_ypos(ev.button.y, &cvstat), win_to_res_xpos(ev.button.x, &cvstat), win_to_res_ypos(ev.button.y, &cvstat));
						break;
					case SDL_BUTTON_RIGHT:
						ciomouse_gotevent(CIOLIB_BUTTON_PRESS(3),win_to_text_xpos(ev.button.x, &cvstat),win_to_text_ypos(ev.button.y, &cvstat), win_to_res_xpos(ev.button.x, &cvstat), win_to_res_ypos(ev.button.y, &cvstat));
						break;
				}
				break;
			case SDL_MOUSEWHEEL:
				pthread_once(&ciolib_mouse_initialized, init_mouse);
				if (ev.wheel.y) {
#if (SDL_MINOR_VERSION > 0) || (SDL_PATCHLEVEL > 3)
					if (ev.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
						ev.wheel.y = 0 - ev.wheel.y;
#endif
					if (ev.wheel.y > 0)
						ciomouse_gotevent(CIOLIB_BUTTON_PRESS(4), -1, -1, -1, -1);
					if (ev.wheel.y < 0)
						ciomouse_gotevent(CIOLIB_BUTTON_PRESS(5), -1, -1, -1, -1);
				}
				break;
			case SDL_MOUSEBUTTONUP:
				pthread_once(&ciolib_mouse_initialized, init_mouse);
				switch(ev.button.button) {
					case SDL_BUTTON_LEFT:
						ciomouse_gotevent(CIOLIB_BUTTON_RELEASE(1),win_to_text_xpos(ev.button.x, &cvstat),win_to_text_ypos(ev.button.y, &cvstat), win_to_res_xpos(ev.button.x, &cvstat), win_to_res_ypos(ev.button.y, &cvstat));
						break;
					case SDL_BUTTON_MIDDLE:
						ciomouse_gotevent(CIOLIB_BUTTON_RELEASE(2),win_to_text_xpos(ev.button.x, &cvstat),win_to_text_ypos(ev.button.y, &cvstat), win_to_res_xpos(ev.button.x, &cvstat), win_to_res_ypos(ev.button.y, &cvstat));
						break;
					case SDL_BUTTON_RIGHT:
						ciomouse_gotevent(CIOLIB_BUTTON_RELEASE(3),win_to_text_xpos(ev.button.x, &cvstat),win_to_text_ypos(ev.button.y, &cvstat), win_to_res_xpos(ev.button.x, &cvstat), win_to_res_ypos(ev.button.y, &cvstat));
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
				else {
					if (sdl_init_good)
						sdl_add_key(CIO_KEY_QUIT, &cvstat);
					else {
						sdl.QuitSubSystem(SDL_INIT_VIDEO);
						return;
					}
				}
				break;
			case SDL_WINDOWEVENT:
				switch(ev.window.event) {
					case SDL_WINDOWEVENT_MAXIMIZED:
					case SDL_WINDOWEVENT_RESTORED:
						// Fall-through
					case SDL_WINDOWEVENT_SIZE_CHANGED:
						// SDL2: User resized window
					case SDL_WINDOWEVENT_RESIZED:
						fullscreen = !!(sdl.GetWindowFlags(win) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP));
						sdl.SetWindowResizable(win, fullscreen ? SDL_FALSE : SDL_TRUE);
						cio_api.mode=fullscreen?CIOLIB_MODE_SDL_FULLSCREEN:CIOLIB_MODE_SDL;
						pthread_mutex_lock(&sdl_mode_mutex);
						if (sdl_mode) {
							pthread_mutex_unlock(&sdl_mode_mutex);
							break;
						}
						pthread_mutex_unlock(&sdl_mode_mutex);
						internal_setwinsize(&cvstat, false);
						break;
					case SDL_WINDOWEVENT_EXPOSED:
						bitmap_drv_request_pixels();
						break;
				}
				break;
			case SDL_USEREVENT: {
				struct rectlist *list;
				struct rectlist *old_next;
				switch(ev.user.code) {
					SDL_Rect dst;

					case SDL_USEREVENT_QUIT:
						sdl_ufunc_retval=0;
						if (ciolib_reaper)
							exit(0);
						sdl.QuitSubSystem(SDL_INIT_VIDEO);
						sem_post(&sdl_ufunc_ret);
						return;
					case SDL_USEREVENT_FLUSH:
						update_cvstat(&cvstat);
						pthread_mutex_lock(&vstatlock);
						// Get correct aspect ratio for dst...
						int sw = cvstat.scrnwidth;
						int sh = cvstat.scrnheight;
						bitmap_get_scaled_win_size(cvstat.scaling, &dst.w, &dst.h, cvstat.winwidth, cvstat.winheight);
						pthread_mutex_unlock(&vstatlock);
						pthread_mutex_lock(&win_mutex);
						if (win != NULL) {
							pthread_mutex_unlock(&win_mutex);
							pthread_mutex_lock(&sdl_headlock);
							pthread_mutex_lock(&sdl_mode_mutex);
							list = update_list;
							update_list = update_list_tail = NULL;
							bool skipit = sdl_mode;
							pthread_mutex_unlock(&sdl_mode_mutex);
							pthread_mutex_unlock(&sdl_headlock);
							for (; list; list = old_next) {
								SDL_Rect src;

								old_next = list->next;
								if (list->next == NULL && !skipit && sw == list->rect.width && sh == list->rect.height) {
									void *pixels;
									int pitch;
									int row;
									int tw, th;

									if (internal_scaling) {
										struct graphics_buffer *gb;
										gb = do_scale(list, dst.w, dst.h);
										src.x = 0;
										src.y = 0;
										src.w = gb->w;
										src.h = gb->h;
										if (sdl.QueryTexture(texture, NULL, NULL, &tw, &th) != 0)
											fprintf(stderr, "Unable to query texture (%s)\n", sdl.GetError());
										sdl.RenderClear(renderer);
										if (sdl.LockTexture(texture, &src, &pixels, &pitch) != 0)
											fprintf(stderr, "Unable to lock texture (%s)\n", sdl.GetError());
										if (pitch != gb->w * sizeof(gb->data[0])) {
											// If this happens, we need to copy a row at a time...
											for (row = 0; row < gb->h && row < th; row++) {
												if (pitch < gb->w * sizeof(gb->data[0]))
													memcpy(pixels, &gb->data[gb->w * row], pitch);
												else
													memcpy(pixels, &gb->data[gb->w * row], gb->w * sizeof(gb->data[0]));
												pixels = (void *)((char*)pixels + pitch);
											}
										}
										else {
											int ch = gb->h;
											if (ch > th)
												ch = th;
											memcpy(pixels, gb->data, gb->w * ch * sizeof(gb->data[0]));
										}
										sdl.UnlockTexture(texture);
										dst.x = (cvstat.winwidth - gb->w) / 2;
										dst.y = (cvstat.winheight - gb->h) / 2;
										release_buffer(gb);
									}
									else {
										src.x = 0;
										src.y = 0;
										src.w = list->rect.width;
										src.h = list->rect.height;
										if (sdl.QueryTexture(texture, NULL, NULL, &tw, &th) != 0)
											fprintf(stderr, "Unable to query texture (%s)\n", sdl.GetError());
										sdl.RenderClear(renderer);
										if (sdl.LockTexture(texture, &src, &pixels, &pitch) != 0)
											fprintf(stderr, "Unable to lock texture (%s)\n", sdl.GetError());
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
										else {
											int ch = list->rect.height;
											if (ch > th)
												ch = th;
											memcpy(pixels, list->data, list->rect.width * ch * sizeof(list->data[0]));
										}
										sdl.UnlockTexture(texture);
										//aspect_fix(&dst.w, &dst.h, cvstat.aspect_width, cvstat.aspect_height);
										dst.x = (cvstat.winwidth - dst.w) / 2;
										dst.y = (cvstat.winheight - dst.h) / 2;
									}
									if (sdl.RenderCopy(renderer, texture, &src, &dst))
										fprintf(stderr, "RenderCopy() failed! (%s)\n", sdl.GetError());
									sdl.RenderPresent(renderer);
								}
								bitmap_drv_free_rect(list);
							}
						}
						else
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
						pthread_mutex_lock(&sdl_mode_mutex);
						sdl_mode = false;
						pthread_mutex_unlock(&sdl_mode_mutex);

						cvstat.winwidth = (intptr_t)ev.user.data1;
						cvstat.winheight = (intptr_t)ev.user.data2;
						internal_setwinsize(&cvstat, true);
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
						break;
					case SDL_USEREVENT_MOUSEPOINTER:
					{
						int cid = INT_MIN;
						SDL_Cursor *oc = curs;
						switch((intptr_t)ev.user.data1) {
							case CIOLIB_MOUSEPTR_ARROW:
								break;	// Default
							case CIOLIB_MOUSEPTR_BAR:
								cid = SDL_SYSTEM_CURSOR_IBEAM;
								break;
						}
						if (cid == INT_MIN) {
							sdl.SetCursor(sdl.GetDefaultCursor());
							curs = NULL;
						}
						else {
							curs = sdl.CreateSystemCursor(cid);
							if (curs == NULL)
								sdl.SetCursor(sdl.GetDefaultCursor());
							else
								sdl.SetCursor(curs);
						}
						if (oc)
							sdl.FreeCursor(oc);
						break;
					}
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
	sdl.QuitSubSystem(SDL_INIT_VIDEO);
	return;
}

static int
sdl_initsync(void)
{
#if defined(__DARWIN__)
	if (initsdl_ret) {
#else
	if(init_sdl_video()) {
#endif
		return(-1);
	}
	sem_init(&sdl_key_pending, 0, 0);
	sem_init(&sdl_ufunc_ret, 0, 0);
	sem_init(&sdl_ufunc_rec, 0, 0);
	pthread_mutex_init(&sdl_ufunc_mtx, NULL);
	pthread_mutex_init(&sdl_headlock, NULL);
	pthread_mutex_init(&win_mutex, NULL);
	pthread_mutex_init(&sdl_keylock, NULL);
	pthread_mutex_init(&sdl_mode_mutex, NULL);
	sdl_sync_initialized = true;
	return 0;
}

int sdl_initciolib(int mode)
{
	if (sdl_initsync() == -1)
		return -1;
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

/* Called from main thread only (Passes Event) */
int sdl_mousepointer(enum ciolib_mouse_ptr type)
{
	sdl_user_func(SDL_USEREVENT_MOUSEPOINTER,type);
	return(0);
}

double
sdl_getscaling(void)
{
	double ret;

	// TODO: I hate having nested locks like this. :(
	pthread_mutex_lock(&vstatlock);
	ret = bitmap_double_mult_inside(vstat.winwidth, vstat.winheight);
	pthread_mutex_unlock(&vstatlock);
	return ret;
}

void
sdl_setscaling(double newval)
{
	int w, h;

	pthread_mutex_lock(&vstatlock);
	bitmap_get_scaled_win_size(newval, &w, &h, 0, 0);
	pthread_mutex_unlock(&vstatlock);
	sdl_setwinsize(w, h);
}

enum ciolib_scaling
sdl_getscaling_type(void)
{
	enum ciolib_scaling ret;

	pthread_mutex_lock(&vstatlock);
	ret = (internal_scaling ? CIOLIB_SCALING_INTERNAL : CIOLIB_SCALING_EXTERNAL);
	pthread_mutex_unlock(&vstatlock);
	return ret;
}

void
sdl_setscaling_type(enum ciolib_scaling newval)
{
	struct video_stats cvstat = vstat;
	int w, h;

	update_cvstat(&cvstat);
	pthread_mutex_lock(&vstatlock);
	if ((newval == CIOLIB_SCALING_INTERNAL) != internal_scaling) {
		internal_scaling = (newval == CIOLIB_SCALING_INTERNAL);
		w = vstat.winwidth;
		h = vstat.winheight;
		pthread_mutex_unlock(&vstatlock);
		sdl_user_func_ret(SDL_USEREVENT_SETVIDMODE, w, h);
	}
	else {
		pthread_mutex_unlock(&vstatlock);
	}
}
