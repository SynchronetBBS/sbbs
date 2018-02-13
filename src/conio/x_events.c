/*
 * This file contains ONLY the functions that are called from the
 * event thread.
 */
 
#include <unistd.h>
#include <stdbool.h>

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>

#include <threadwrap.h>
#include <genwrap.h>
#include <dirwrap.h>

#include "vidmodes.h"

#include "ciolib.h"
#define BITMAP_CIOLIB_DRIVER
#include "bitmap_con.h"
#include "link_list.h"
#include "x_events.h"
#include "x_cio.h"

/*
 * Exported variables 
 */

int local_pipe[2];			/* Used for passing local events */
int key_pipe[2];			/* Used for passing keyboard events */

struct x11 x11;

char 	*copybuf;
pthread_mutex_t	copybuf_mutex;
char 	*pastebuf;
sem_t	pastebuf_set;
sem_t	pastebuf_used;
sem_t	init_complete;
sem_t	mode_set;
int x11_window_xpos;
int x11_window_ypos;
int x11_window_width;
int x11_window_height;
int x11_initialized=0;
sem_t	event_thread_complete;
int	terminate = 0;
/*
 * Local variables
 */

/* Sets the atom to be used for copy/paste operations */
#define CONSOLE_CLIPBOARD	XA_PRIMARY
static Atom WM_DELETE_WINDOW=0;

static Display *dpy=NULL;
static Window win;
static Visual visual;
static XImage *xim;
static unsigned int depth=0;
static int xfd;
static unsigned long black;
static unsigned long white;
static int bitmap_width=0;
static int bitmap_height=0;
static int old_scaling = 0;
struct video_stats x_cvstat;
static unsigned long base_pixel;
static int r_shift;
static int g_shift;
static int b_shift;

/* Array of Graphics Contexts */
static GC gc;

static WORD Ascii2Scan[] = {
 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
 0x000e, 0x000f, 0xffff, 0xffff, 0xffff, 0x001c, 0xffff, 0xffff,
 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
 0xffff, 0xffff, 0xffff, 0x0001, 0xffff, 0xffff, 0xffff, 0xffff,
 0x0039, 0x0102, 0x0128, 0x0104, 0x0105, 0x0106, 0x0108, 0x0028,
 0x010a, 0x010b, 0x0109, 0x010d, 0x0033, 0x000c, 0x0034, 0x0035,
 0x000b, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008,
 0x0009, 0x000a, 0x0127, 0x0027, 0x0133, 0x000d, 0x0134, 0x0135,
 0x0103, 0x011e, 0x0130, 0x012e, 0x0120, 0x0112, 0x0121, 0x0122,
 0x0123, 0x0117, 0x0124, 0x0125, 0x0126, 0x0132, 0x0131, 0x0118,
 0x0119, 0x0110, 0x0113, 0x011f, 0x0114, 0x0116, 0x012f, 0x0111,
 0x012d, 0x0115, 0x012c, 0x001a, 0x002b, 0x001b, 0x0107, 0x010c,
 0x0029, 0x001e, 0x0030, 0x002e, 0x0020, 0x0012, 0x0021, 0x0022,
 0x0023, 0x0017, 0x0024, 0x0025, 0x0026, 0x0032, 0x0031, 0x0018,
 0x0019, 0x0010, 0x0013, 0x001f, 0x0014, 0x0016, 0x002f, 0x0011,
 0x002d, 0x0015, 0x002c, 0x011a, 0x012b, 0x011b, 0x0129, 0xffff,
};

static struct {
    WORD	base;
    WORD	shift;
    WORD	ctrl;
    WORD	alt;
} ScanCodes[] = {
    {	0xffff, 0xffff, 0xffff, 0xffff }, /* key  0 */
    {	0x011b, 0x011b, 0x011b, 0xffff }, /* key  1 - Escape key */
    {	0x0231, 0x0221, 0xffff, 0x7800 }, /* key  2 - '1' */
    {	0x0332, 0x0340, 0x0300, 0x7900 }, /* key  3 - '2' - special handling */
    {	0x0433, 0x0423, 0xffff, 0x7a00 }, /* key  4 - '3' */
    {	0x0534, 0x0524, 0xffff, 0x7b00 }, /* key  5 - '4' */
    {	0x0635, 0x0625, 0xffff, 0x7c00 }, /* key  6 - '5' */
    {	0x0736, 0x075e, 0x071e, 0x7d00 }, /* key  7 - '6' */
    {	0x0837, 0x0826, 0xffff, 0x7e00 }, /* key  8 - '7' */
    {	0x0938, 0x092a, 0xffff, 0x7f00 }, /* key  9 - '8' */
    {	0x0a39, 0x0a28, 0xffff, 0x8000 }, /* key 10 - '9' */
    {	0x0b30, 0x0b29, 0xffff, 0x8100 }, /* key 11 - '0' */
    {	0x0c2d, 0x0c5f, 0x0c1f, 0x8200 }, /* key 12 - '-' */
    {	0x0d3d, 0x0d2b, 0xffff, 0x8300 }, /* key 13 - '=' */
    {	0x0e08, 0x0e08, 0x0e7f, 0xffff }, /* key 14 - backspace */
    {	0x0f09, 0x0f00, 0xffff, 0xffff }, /* key 15 - tab */
    {	0x1071, 0x1051, 0x1011, 0x1000 }, /* key 16 - 'Q' */
    {	0x1177, 0x1157, 0x1117, 0x1100 }, /* key 17 - 'W' */
    {	0x1265, 0x1245, 0x1205, 0x1200 }, /* key 18 - 'E' */
    {	0x1372, 0x1352, 0x1312, 0x1300 }, /* key 19 - 'R' */
    {	0x1474, 0x1454, 0x1414, 0x1400 }, /* key 20 - 'T' */
    {	0x1579, 0x1559, 0x1519, 0x1500 }, /* key 21 - 'Y' */
    {	0x1675, 0x1655, 0x1615, 0x1600 }, /* key 22 - 'U' */
    {	0x1769, 0x1749, 0x1709, 0x1700 }, /* key 23 - 'I' */
    {	0x186f, 0x184f, 0x180f, 0x1800 }, /* key 24 - 'O' */
    {	0x1970, 0x1950, 0x1910, 0x1900 }, /* key 25 - 'P' */
    {	0x1a5b, 0x1a7b, 0x1a1b, 0xffff }, /* key 26 - '[' */
    {	0x1b5d, 0x1b7d, 0x1b1d, 0xffff }, /* key 27 - ']' */
    {	0x1c0d, 0x1c0d, 0x1c0a, 0xffff }, /* key 28 - CR */
    {	0xffff, 0xffff, 0xffff, 0xffff }, /* key 29 - control */
    {	0x1e61, 0x1e41, 0x1e01, 0x1e00 }, /* key 30 - 'A' */
    {	0x1f73, 0x1f53, 0x1f13, 0x1f00 }, /* key 31 - 'S' */
    {	0x2064, 0x2044, 0x2004, 0x2000 }, /* key 32 - 'D' */
    {	0x2166, 0x2146, 0x2106, 0x2100 }, /* key 33 - 'F' */
    {	0x2267, 0x2247, 0x2207, 0x2200 }, /* key 34 - 'G' */
    {	0x2368, 0x2348, 0x2308, 0x2300 }, /* key 35 - 'H' */
    {	0x246a, 0x244a, 0x240a, 0x2400 }, /* key 36 - 'J' */
    {	0x256b, 0x254b, 0x250b, 0x2500 }, /* key 37 - 'K' */
    {	0x266c, 0x264c, 0x260c, 0x2600 }, /* key 38 - 'L' */
    {	0x273b, 0x273a, 0xffff, 0xffff }, /* key 39 - ';' */
    {	0x2827, 0x2822, 0xffff, 0xffff }, /* key 40 - ''' */
    {	0x2960, 0x297e, 0xffff, 0xffff }, /* key 41 - '`' */
    {	0xffff, 0xffff, 0xffff, 0xffff }, /* key 42 - left shift */
    {	0x2b5c, 0x2b7c, 0x2b1c, 0xffff }, /* key 43 - '' */
    {	0x2c7a, 0x2c5a, 0x2c1a, 0x2c00 }, /* key 44 - 'Z' */
    {	0x2d78, 0x2d58, 0x2d18, 0x2d00 }, /* key 45 - 'X' */
    {	0x2e63, 0x2e43, 0x2e03, 0x2e00 }, /* key 46 - 'C' */
    {	0x2f76, 0x2f56, 0x2f16, 0x2f00 }, /* key 47 - 'V' */
    {	0x3062, 0x3042, 0x3002, 0x3000 }, /* key 48 - 'B' */
    {	0x316e, 0x314e, 0x310e, 0x3100 }, /* key 49 - 'N' */
    {	0x326d, 0x324d, 0x320d, 0x3200 }, /* key 50 - 'M' */
    {	0x332c, 0x333c, 0xffff, 0xffff }, /* key 51 - ',' */
    {	0x342e, 0x343e, 0xffff, 0xffff }, /* key 52 - '.' */
    {	0x352f, 0x353f, 0xffff, 0xffff }, /* key 53 - '/' */
    {	0xffff, 0xffff, 0xffff, 0xffff }, /* key 54 - right shift - */
    {	0x372a, 0xffff, 0x3772, 0xffff }, /* key 55 - prt-scr - */
    {	0xffff, 0xffff, 0xffff, 0xffff }, /* key 56 - Alt - */
    {	0x3920, 0x3920, 0x3920, 0x3920 }, /* key 57 - space bar */
    {	0xffff, 0xffff, 0xffff, 0xffff }, /* key 58 - caps-lock -  */
    {	0x3b00, 0x5400, 0x5e00, 0x6800 }, /* key 59 - F1 */
    {	0x3c00, 0x5500, 0x5f00, 0x6900 }, /* key 60 - F2 */
    {	0x3d00, 0x5600, 0x6000, 0x6a00 }, /* key 61 - F3 */
    {	0x3e00, 0x5700, 0x6100, 0x6b00 }, /* key 62 - F4 */
    {	0x3f00, 0x5800, 0x6200, 0x6c00 }, /* key 63 - F5 */
    {	0x4000, 0x5900, 0x6300, 0x6d00 }, /* key 64 - F6 */
    {	0x4100, 0x5a00, 0x6400, 0x6e00 }, /* key 65 - F7 */
    {	0x4200, 0x5b00, 0x6500, 0x6f00 }, /* key 66 - F8 */
    {	0x4300, 0x5c00, 0x6600, 0x7000 }, /* key 67 - F9 */
    {	0x4400, 0x5d00, 0x6700, 0x7100 }, /* key 68 - F10 */
    {	0xffff, 0xffff, 0xffff, 0xffff }, /* key 69 - num-lock - */
    {	0xffff, 0xffff, 0xffff, 0xffff }, /* key 70 - scroll-lock -  */
    {	0x4700, 0x4737, 0x7700, 0xffff }, /* key 71 - home */
    {	0x4800, 0x4838, 0x8d00, 0x9800 }, /* key 72 - cursor up */
    {	0x4900, 0x4939, 0x8400, 0xffff }, /* key 73 - page up */
    {	0x4a2d, 0x4a2d, 0xffff, 0xffff }, /* key 74 - minus sign */
    {	0x4b00, 0x4b34, 0x7300, 0xffff }, /* key 75 - cursor left */
    {	0xffff, 0x4c35, 0xffff, 0xffff }, /* key 76 - center key */
    {	0x4d00, 0x4d36, 0x7400, 0xffff }, /* key 77 - cursor right */
    {	0x4e2b, 0x4e2b, 0xffff, 0xffff }, /* key 78 - plus sign */
    {	0x4f00, 0x4f31, 0x7500, 0xffff }, /* key 79 - end */
    {	0x5000, 0x5032, 0x9100, 0xa000 }, /* key 80 - cursor down */
    {	0x5100, 0x5133, 0x7600, 0xffff }, /* key 81 - page down */
    {	0x5200, 0x5230, 0xffff, 0xffff }, /* key 82 - insert */
    {	0x5300, 0x532e, 0xffff, 0xffff }, /* key 83 - delete */
    {	0xffff, 0xffff, 0xffff, 0xffff }, /* key 84 - sys key */
    {	0xffff, 0xffff, 0xffff, 0xffff }, /* key 85 */
    {	0xffff, 0xffff, 0xffff, 0xffff }, /* key 86 */
    {	0x8500, 0x5787, 0x8900, 0x8b00 }, /* key 87 - F11 */
    {	0x8600, 0x5888, 0x8a00, 0x8c00 }, /* key 88 - F12 */
};

static void resize_xim(void)
{
	if (xim) {
#ifdef XDestroyImage
		XDestroyImage(xim);
#else
		x11.XDestroyImage(xim);
#endif
	}
    xim=x11.XCreateImage(dpy,&visual,depth,ZPixmap,0,NULL,bitmap_width*x_cvstat.scaling,bitmap_height*x_cvstat.scaling*x_cvstat.vmultiplier,32,0);
    xim->data=(char *)malloc(xim->bytes_per_line*xim->height);
}

/* Get a connection to the X server and create the window. */
static int init_window()
{
    XGCValues gcv;
    int i;
	XWMHints *wmhints;
	int ret;
	int best=-1;
	int best_depth=0;
	int best_cmap=0;
	XVisualInfo template = {0};
	XVisualInfo *vi;

	dpy = x11.XOpenDisplay(NULL);
    if (dpy == NULL) {
		return(-1);
	}
    xfd = ConnectionNumber(dpy);

	template.screen = DefaultScreen(dpy);
	template.class = TrueColor;
	vi = x11.XGetVisualInfo(dpy, VisualScreenMask | VisualClassMask, &template, &ret);
	for (i=0; i<ret; i++) {
		if (vi[i].depth >= best_depth && vi[i].colormap_size >= best_cmap) {
			best = i;
			best_depth = vi[i].depth;
		}
	}
	if (best != -1) {
		visual = *vi[best].visual;
		base_pixel = ULONG_MAX;
		base_pixel &= ~visual.red_mask;
		base_pixel &= ~visual.green_mask;
		base_pixel &= ~visual.blue_mask;
		r_shift = fls(visual.red_mask)-16;
		g_shift = fls(visual.green_mask)-16;
		b_shift = fls(visual.blue_mask)-16;
	}
	else {
		fprintf(stderr, "Unable to find TrueColor visual\n");
		x11.XFree(vi);
		return -1;
	}
	x11.XFree(vi);

	/* Allocate black and white */
	black=BlackPixel(dpy, DefaultScreen(dpy));
	white=WhitePixel(dpy, DefaultScreen(dpy));

    /* Create window, but defer setting a size and GC. */
	XSetWindowAttributes wa = {0};
	wa.colormap = x11.XCreateColormap(dpy, DefaultRootWindow(dpy), &visual, AllocNone);
	wa.background_pixel = black;
	wa.border_pixel = black;
	depth = best_depth;
    win = x11.XCreateWindow(dpy, DefaultRootWindow(dpy), 0, 0,
			      640*x_cvstat.scaling, 400*x_cvstat.scaling*x_cvstat.vmultiplier, 2, depth, InputOutput, &visual, CWColormap | CWBorderPixel | CWBackPixel, &wa);

	wmhints=x11.XAllocWMHints();
	if(wmhints) {
		wmhints->initial_state=NormalState;
		wmhints->flags = (StateHint | IconPixmapHint | IconMaskHint | InputHint);
		wmhints->input = True;
		x11.XSetWMProperties(dpy, win, NULL, NULL, 0, 0, NULL, wmhints, NULL);
	}

	WM_DELETE_WINDOW = x11.XInternAtom(dpy, "WM_DELETE_WINDOW", False);

	gcv.function = GXcopy;
    gcv.foreground = white;
    gcv.background = black;
	gcv.graphics_exposures = False;

	gc=x11.XCreateGC(dpy, win, GCFunction | GCForeground | GCBackground | GCGraphicsExposures, &gcv);

    x11.XSelectInput(dpy, win, KeyReleaseMask | KeyPressMask |
		     ExposureMask | ButtonPressMask
		     | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask);

    x11.XStoreName(dpy, win, "SyncConsole");
	x11.XSetWMProtocols(dpy, win, &WM_DELETE_WINDOW, 1);

	resize_xim();

	return(0);
}

/*
 * Actually maps (shows) the window
 */
static void map_window()
{
    XSizeHints *sh;

    sh = x11.XAllocSizeHints();
    if (sh == NULL) {
		fprintf(stderr, "Could not get XSizeHints structure");
		exit(1);
	}

	sh->base_width = bitmap_width*x_cvstat.scaling;
	sh->base_height = bitmap_height*x_cvstat.scaling*x_cvstat.vmultiplier;

    sh->min_width = sh->width_inc = sh->min_aspect.x = sh->max_aspect.x = bitmap_width;
    sh->min_height = sh->height_inc = sh->min_aspect.y = sh->max_aspect.y = bitmap_height;
    sh->flags = USSize | PMinSize | PSize | PResizeInc | PAspect;

    x11.XSetWMNormalHints(dpy, win, sh);
    x11.XMapWindow(dpy, win);

    x11.XFree(sh);

	bitmap_drv_request_pixels();

    return;
}

/* Resize the window. This function is called after a mode change. */
static void resize_window()
{
    x11.XResizeWindow(dpy, win, bitmap_width*x_cvstat.scaling, bitmap_height*x_cvstat.scaling*x_cvstat.vmultiplier);
    resize_xim();

    return;
}

static int init_mode(int mode)
{
    int oldcols;
	int oldwidth=bitmap_width;
	int oldheight=bitmap_height;

	oldcols=x_cvstat.cols;

	pthread_mutex_lock(&vstatlock);
	bitmap_drv_init_mode(mode, &bitmap_width, &bitmap_height);
	x_cvstat = vstat;
	pthread_mutex_unlock(&vstatlock);

	/* Deal with 40 col doubling */
	if(oldcols != x_cvstat.cols) {
		if(oldcols == 40)
			x_cvstat.scaling /= 2;
		if(x_cvstat.cols == 40)
			x_cvstat.scaling *= 2;
	}

	if(x_cvstat.scaling < 1)
		x_setscaling(1);

    map_window();
    /* Resize window if necessary. */
	if((!(bitmap_width == 0 && bitmap_height == 0)) && (oldwidth != bitmap_width || oldheight != bitmap_height))
		resize_window();
	bitmap_drv_request_pixels();

	sem_post(&mode_set);
    return(0);
}

static int video_init()
{
    /* If we are running under X, get a connection to the X server and create
       an empty window of size (1, 1). It makes a couple of init functions a
       lot easier. */
	if(x_cvstat.scaling<1)
		x_setscaling(1);
	if(x_cvstat.vmultiplier<1)
		x_cvstat.vmultiplier=1;
    if(init_window())
		return(-1);

	bitmap_drv_init(x11_drawrect, x11_flush);

    /* Initialize mode 3 (text, 80x25, 16 colors) */
    if(init_mode(3)) {
		return(-1);
	}

	sem_wait(&mode_set);

    return(0);
}

static void local_draw_rect(struct rectlist *rect)
{
	int x,y,xscale,yscale;
	unsigned int r, g, b;
	unsigned long pixel;

	/* TODO: Translate into local colour depth */
	for(y=0;y<rect->rect.height;y++) {
		for(x=0; x<rect->rect.width; x++) {
			for(yscale=0; yscale<x_cvstat.scaling*x_cvstat.vmultiplier; yscale++) {
				for(xscale=0; xscale<x_cvstat.scaling; xscale++) {
					r = rect->data[y*rect->rect.width+x] >> 16 & 0xff;
					g = rect->data[y*rect->rect.width+x] >> 8 & 0xff;
					b = rect->data[y*rect->rect.width+x] & 0xff;
					r = (r<<8)|r;
					g = (g<<8)|g;
					b = (b<<8)|b;
					pixel = base_pixel;
					if (r_shift >= 0)
						pixel |= (r << r_shift) & visual.red_mask;
					else
						pixel |= (r >> (0-r_shift)) & visual.red_mask;
					if (g_shift >= 0)
						pixel |= (g << g_shift) & visual.green_mask;
					else
						pixel |= (g >> (0-g_shift)) & visual.green_mask;
					if (b_shift >= 0)
						pixel |= (b << b_shift) & visual.blue_mask;
					else
						pixel |= (b >> (0-b_shift)) & visual.blue_mask;
#ifdef XPutPixel
					XPutPixel(xim,(x+rect->rect.x)*x_cvstat.scaling+xscale,(y+rect->rect.y)*x_cvstat.scaling*x_cvstat.vmultiplier+yscale,pixel);
#else
					x11.XPutPixel(xim,(x+rect->rect.x)*x_cvstat.scaling+xscale,(y+rect->rect.y)*x_cvstat.scaling*x_cvstat.vmultiplier+yscale,pixel);
#endif
				}
			}
		}
	}

	x11.XPutImage(dpy,win,gc,xim,rect->rect.x*x_cvstat.scaling,rect->rect.y*x_cvstat.scaling*x_cvstat.vmultiplier,rect->rect.x*x_cvstat.scaling,rect->rect.y*x_cvstat.scaling*x_cvstat.vmultiplier,rect->rect.width*x_cvstat.scaling,rect->rect.height*x_cvstat.scaling*x_cvstat.vmultiplier);

	bitmap_drv_free_rect(rect);
}

static void handle_resize_event(int width, int height)
{
	int newFSH=1;
	int newFSW=1;

	// No change
	if((width == x_cvstat.charwidth * x_cvstat.cols * x_cvstat.scaling)
			&& (height == x_cvstat.charheight * x_cvstat.rows * x_cvstat.scaling*x_cvstat.vmultiplier))
		return;

	newFSH=width/bitmap_width;
	newFSW=height/bitmap_height;
	if(newFSW<1)
		newFSW=1;
	if(newFSH<1)
		newFSH=1;
	if(newFSH<newFSW)
		x_setscaling(newFSH);
	else
		x_setscaling(newFSW);
	old_scaling = x_cvstat.scaling;
	if(x_cvstat.scaling > 16)
		x_setscaling(16);
	/*
	 * We only need to resize if the width/height are not even multiples
	 * Otherwise, we can simply resend everything
	 */
	if((width % (x_cvstat.charwidth * x_cvstat.cols) != 0)
			|| (height % (x_cvstat.charheight * x_cvstat.rows) != 0)) {
		resize_window();
	}
	bitmap_drv_request_pixels();
}

static void expose_rect(int x, int y, int width, int height)
{
	int sx,sy,ex,ey;

	sx=x/x_cvstat.scaling;
	sy=y/(x_cvstat.scaling*x_cvstat.vmultiplier);

	ex=x+width-1;
	ey=y+height-1;
	if((ex+1)%x_cvstat.scaling) {
		ex += x_cvstat.scaling-(ex%x_cvstat.scaling);
	}
	if((ey+1)%(x_cvstat.scaling*x_cvstat.vmultiplier)) {
		ey += x_cvstat.scaling*x_cvstat.vmultiplier-(ey%(x_cvstat.scaling*x_cvstat.vmultiplier));
	}
	ex=ex/x_cvstat.scaling;
	ey=ey/(x_cvstat.scaling*x_cvstat.vmultiplier);

	bitmap_drv_request_some_pixels(sx, sy, ex-sx+1, ey-sy+1);
}

static int x11_event(XEvent *ev)
{
	switch (ev->type) {
		case ClientMessage:
			if (ev->xclient.format == 32 && ev->xclient.data.l[0] == WM_DELETE_WINDOW) {
				uint16_t key=CIO_KEY_QUIT;
				write(key_pipe[1], &key, 2);
			}
			break;
		/* Graphics related events */
		case ConfigureNotify:
			x11_window_xpos=ev->xconfigure.x;
			x11_window_ypos=ev->xconfigure.y;
			x11_window_width=ev->xconfigure.width;
			x11_window_height=ev->xconfigure.height;
			handle_resize_event(ev->xconfigure.width, ev->xconfigure.height);
			break;
        case NoExpose:
                break;
        case GraphicsExpose:
			expose_rect(ev->xgraphicsexpose.x, ev->xgraphicsexpose.y, ev->xgraphicsexpose.width, ev->xgraphicsexpose.height);
			break;
        case Expose:
			expose_rect(ev->xexpose.x, ev->xexpose.y, ev->xexpose.width, ev->xexpose.height);
			break;

		/* Copy/Paste events */
		case SelectionClear:
			{
				XSelectionClearEvent *req;

				req=&(ev->xselectionclear);
				pthread_mutex_lock(&copybuf_mutex);
				if(req->selection==CONSOLE_CLIPBOARD)
					FREE_AND_NULL(copybuf);
				pthread_mutex_unlock(&copybuf_mutex);
			}
			break;
		case SelectionNotify:
			{
				int format;
				unsigned long len, bytes_left, dummy;
				Atom type;

				if(ev->xselection.selection != CONSOLE_CLIPBOARD)
					break;
				if(ev->xselection.requestor!=win)
					break;
				if(ev->xselection.property) {
					x11.XGetWindowProperty(dpy, win, ev->xselection.property, 0, 0, 0, AnyPropertyType, &type, &format, &len, &bytes_left, (unsigned char **)(&pastebuf));
					if(bytes_left > 0 && format==8)
						x11.XGetWindowProperty(dpy, win, ev->xselection.property,0,bytes_left,0,AnyPropertyType,&type,&format,&len,&dummy,(unsigned char **)&pastebuf);
					else
						pastebuf=NULL;
				}
				else
					pastebuf=NULL;

				/* Set paste buffer */
				sem_post(&pastebuf_set);
				sem_wait(&pastebuf_used);
				x11.XFree(pastebuf);
				pastebuf=NULL;
			}
			break;
		case SelectionRequest:
			{
				XSelectionRequestEvent *req;
				XEvent respond;

				req=&(ev->xselectionrequest);
				pthread_mutex_lock(&copybuf_mutex);
				if(copybuf==NULL) {
					respond.xselection.property=None;
				}
				else {
					if(req->target==XA_STRING) {
						x11.XChangeProperty(dpy, req->requestor, req->property, XA_STRING, 8, PropModeReplace, (unsigned char *)copybuf, strlen(copybuf));
						respond.xselection.property=req->property;
					}
					else
						respond.xselection.property=None;
				}
				respond.xselection.type=SelectionNotify;
				respond.xselection.display=req->display;
				respond.xselection.requestor=req->requestor;
				respond.xselection.selection=req->selection;
				respond.xselection.target=req->target;
				respond.xselection.time=req->time;
				x11.XSendEvent(dpy,req->requestor,0,0,&respond);
				pthread_mutex_unlock(&copybuf_mutex);
			}
			break;

		/* Mouse Events */
		case MotionNotify:
			{
				XMotionEvent *me = (XMotionEvent *)ev;

				me->x/=x_cvstat.scaling;
				me->x/=x_cvstat.charwidth;
				me->y/=x_cvstat.scaling;
				me->y/=x_cvstat.vmultiplier;
				me->y/=x_cvstat.charheight;
				me->x++;
				me->y++;
				if(me->x<1)
					me->x=1;
				if(me->y<1)
					me->y=1;
				if(me->x>x_cvstat.cols)
					me->x=x_cvstat.cols;
				if(me->y>x_cvstat.rows+1)
					me->y=x_cvstat.rows+1;
				ciomouse_gotevent(CIOLIB_MOUSE_MOVE,me->x,me->y);
	    	}
			break;
		case ButtonRelease:
			{
				XButtonEvent *be = (XButtonEvent *)ev;

				be->x/=x_cvstat.scaling;
				be->x/=x_cvstat.charwidth;
				be->y/=x_cvstat.scaling;
				be->y/=x_cvstat.vmultiplier;
				be->y/=x_cvstat.charheight;
				be->x++;
				be->y++;
				if(be->x<1)
					be->x=1;
				if(be->y<1)
					be->y=1;
				if(be->x>x_cvstat.cols)
					be->x=x_cvstat.cols;
				if(be->y>x_cvstat.rows+1)
					be->y=x_cvstat.rows+1;
				if (be->button <= 3) {
					ciomouse_gotevent(CIOLIB_BUTTON_RELEASE(be->button),be->x,be->y);
				}
	    	}
			break;
		case ButtonPress:
			{
				XButtonEvent *be = (XButtonEvent *)ev;

				be->x/=x_cvstat.scaling;
				be->x/=x_cvstat.charwidth;
				be->y/=x_cvstat.scaling;
				be->y/=x_cvstat.vmultiplier;
				be->y/=x_cvstat.charheight;
				be->x++;
				be->y++;
				if(be->x<1)
					be->x=1;
				if(be->y<1)
					be->y=1;
				if(be->x>x_cvstat.cols)
					be->x=x_cvstat.cols;
				if(be->y>x_cvstat.rows+1)
					be->y=x_cvstat.rows+1;
				if (be->button <= 3) {
					ciomouse_gotevent(CIOLIB_BUTTON_PRESS(be->button),be->x,be->y);
				}
	    	}
			break;

		/* Keyboard Events */
		case KeyPress:
			{
				static char buf[128];
				KeySym ks;
				int nlock = 0;
				WORD scan = 0xffff;

				x11.XLookupString((XKeyEvent *)ev, buf, sizeof(buf), &ks, 0);

				switch (ks) {
				
					case XK_Escape:
						scan = 1;
						goto docode;

					case XK_Tab:
					case XK_ISO_Left_Tab:
						scan = 15;
						goto docode;
			
					case XK_Return:
					case XK_KP_Enter:
						scan = 28;
						goto docode;

					case XK_Print:
						scan = 55;
						goto docode;

					case XK_F1:
					case XK_F2:
					case XK_F3:
					case XK_F4:
					case XK_F5:
					case XK_F6:
					case XK_F7:
					case XK_F8:
					case XK_F9:
					case XK_F10:
						scan = ks - XK_F1 + 59;
						goto docode;

					case XK_KP_7:
						nlock = 1;
					case XK_Home:
					case XK_KP_Home:
						scan = 71;
						goto docode;

					case XK_KP_8:
						nlock = 1;
					case XK_Up:
					case XK_KP_Up:
						scan = 72;
						goto docode;

					case XK_KP_9:
						nlock = 1;
					case XK_Prior:
					case XK_KP_Prior:
						scan = 73;
						goto docode;

					case XK_KP_Subtract:
						scan = 74;
						goto docode;

					case XK_KP_4:
						nlock = 1;
					case XK_Left:
					case XK_KP_Left:
						scan = 75;
						goto docode;

					case XK_KP_5:
						nlock = 1;
					case XK_Begin:
					case XK_KP_Begin:
						scan = 76;
						goto docode;

					case XK_KP_6:
						nlock = 1;
					case XK_Right:
					case XK_KP_Right:
						scan = 77;
						goto docode;

					case XK_KP_Add:
						scan = 78;
						goto docode;

					case XK_KP_1:
						nlock = 1;
					case XK_End:
					case XK_KP_End:
						scan = 79;
						goto docode;

					case XK_KP_2:
						nlock = 1;
					case XK_Down:
					case XK_KP_Down:
						scan = 80;
						goto docode;

					case XK_KP_3:
						nlock = 1;
					case XK_Next:
					case XK_KP_Next:
						scan = 81;
						goto docode;

					case XK_KP_0:
						nlock = 1;
					case XK_Insert:
					case XK_KP_Insert:
						scan = 82;
						goto docode;

					case XK_KP_Decimal:
						nlock = 1;
						scan = 83;
						goto docode;

					case XK_Delete:
					case XK_KP_Delete:
						/* scan = flipdelete ? 14 : 83; */
						scan = 83;
						goto docode;

					case XK_BackSpace:
						/* scan = flipdelete ? 83 : 14; */
						scan = 14;
						goto docode;

					case XK_F11:
						scan = 87;
						goto docode;
					case XK_F12:
						scan = 88;
						goto docode;


					case XK_KP_Divide:
						scan = Ascii2Scan['/'];
						goto docode;

					case XK_KP_Multiply:
						scan = Ascii2Scan['*'];
						goto docode;

					default:
						if (ks < ' ' || ks > '~')
							break;
						scan = Ascii2Scan[ks]; 
						docode:
						if (nlock)
							scan |= 0x100;

						if ((scan & ~0x100) > 88) {
							scan = 0xffff;
							break;
						}

						if (ev->xkey.state & Mod1Mask) {
							scan = ScanCodes[scan & 0xff].alt;
						} else if ((ev->xkey.state & ShiftMask) || (scan & 0x100)) {
							scan = ScanCodes[scan & 0xff].shift;
						} else if (ev->xkey.state & ControlMask) {
							scan = ScanCodes[scan & 0xff].ctrl;
						}  else
							scan = ScanCodes[scan & 0xff].base;

						break;
				}
				if (scan != 0xffff) {
					uint16_t key=scan;
					write(key_pipe[1], &key, (scan&0xff)?1:2);
				}
				return(1);
			}
		default:
			break;
	}
	return(0);
}

void check_scaling(void)
{
	if (old_scaling != x_cvstat.scaling) {
		resize_window();
		old_scaling = x_cvstat.scaling;
	}
}

static void x11_terminate_event_thread(void)
{
	terminate = 1;
	sem_wait(&event_thread_complete);
}

static void readev(struct x11_local_event *lev)
{
	fd_set	rfd;
	int ret;
	int rcvd = 0;
	char *buf = (char *)lev;

	FD_ZERO(&rfd);
	FD_SET(local_pipe[0], &rfd);

	while (rcvd < sizeof(*lev)) {
		select(local_pipe[0]+1, &rfd, NULL, NULL, NULL);
		ret = read(local_pipe[0], buf+rcvd, sizeof(*lev) - rcvd);
		if (ret > 0)
			rcvd += ret;
	}
}

void x11_event_thread(void *args)
{
	int x;
	int high_fd;
	fd_set fdset;
	XEvent ev;
	static struct timeval tv;

	SetThreadName("X11 Events");
	if(video_init()) {
		sem_post(&init_complete);
		return;
	}
	x11_initialized=1;
	sem_init(&event_thread_complete, 0, 0);
	atexit(x11_terminate_event_thread);
	sem_post(&init_complete);

	if(local_pipe[0] > xfd)
		high_fd=local_pipe[0];
	else
		high_fd=xfd;

	for (;!terminate;) {
		check_scaling();

		tv.tv_sec=0;
		tv.tv_usec=54925; /* was 54925 (was also 10) */ 

		/*
		 * Handle any events just sitting around...
		 */
		while (QLength(dpy) > 0) {
			x11.XNextEvent(dpy, &ev);
			x11_event(&ev);
		}

		FD_ZERO(&fdset);
		FD_SET(xfd, &fdset);
		FD_SET(local_pipe[0], &fdset);

		x = select(high_fd+1, &fdset, 0, 0, &tv);

		switch (x) {
			case -1:
				/*
				* Errno might be wrong, so we just select again.
				* This could cause a problem is something really
				* was wrong with select....
				*/

				/* perror("select"); */
				break;
			case 0:
				/* Timeout */
				break;
			default:
				if (FD_ISSET(xfd, &fdset)) {
					x11.XNextEvent(dpy, &ev);
					x11_event(&ev);
				}
				if(FD_ISSET(local_pipe[0], &fdset)) {
					struct x11_local_event lev;

					readev(&lev);
					switch(lev.type) {
						case X11_LOCAL_SETMODE:
							init_mode(lev.data.mode);
							break;
						case X11_LOCAL_SETNAME:
							x11.XSetIconName(dpy, win, lev.data.name);
							x11.XFlush(dpy);
							break;
						case X11_LOCAL_SETTITLE:
							x11.XStoreName(dpy, win, lev.data.title);
							x11.XFlush(dpy);
							break;
						case X11_LOCAL_COPY:
							x11.XSetSelectionOwner(dpy, CONSOLE_CLIPBOARD, win, CurrentTime);
							break;
						case X11_LOCAL_PASTE: 
							{
								Window sowner=None;

								sowner=x11.XGetSelectionOwner(dpy, CONSOLE_CLIPBOARD);
								if(sowner==win) {
									/* Get your own primary selection */
									if(copybuf==NULL)
										pastebuf=NULL;
									else
										pastebuf=strdup(copybuf);
									/* Set paste buffer */
									sem_post(&pastebuf_set);
									sem_wait(&pastebuf_used);
									FREE_AND_NULL(pastebuf);
								}
								else if(sowner!=None) {
									x11.XConvertSelection(dpy, CONSOLE_CLIPBOARD, XA_STRING, XA_STRING, win, CurrentTime);
								}
								else {
									/* Set paste buffer */
									pastebuf=NULL;
									sem_post(&pastebuf_set);
									sem_wait(&pastebuf_used);
								}
							}
							break;
						case X11_LOCAL_DRAWRECT:
							local_draw_rect(lev.data.rect);
							break;
						case X11_LOCAL_FLUSH:
							x11.XFlush(dpy);
							break;
						case X11_LOCAL_BEEP:
							x11.XBell(dpy, 100);
							break;
					}
				}
		}
	}
	x11.XCloseDisplay(dpy);
	sem_post(&event_thread_complete);
}
