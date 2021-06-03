/*
 * This file contains ONLY the functions that are called from the
 * event thread.
 */
 
#include <math.h>
#include <unistd.h>
#include <stdbool.h>

#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>

#include <threadwrap.h>
#include <genwrap.h>
#include <dirwrap.h>

#include "vidmodes.h"

#include "ciolib.h"
#define BITMAP_CIOLIB_DRIVER
#include "bitmap_con.h"
#include "link_list.h"
#include "scale.h"
#include "x_events.h"
#include "x_cio.h"
#include "utf8_codepages.h"

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
Atom	copybuf_format;
Atom	pastebuf_format;

/*
 * Local variables
 */

/* Sets the atom to be used for copy/paste operations */
#define CONSOLE_CLIPBOARD	XA_PRIMARY
static Atom WM_DELETE_WINDOW=0;

static Display *dpy=NULL;
static Window win;
static Cursor curs = None;
static Visual visual;
static bool VisualIsRGB8 = false;
static XImage *xim;
static XIM im;
static XIC ic;
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
static struct graphics_buffer *last = NULL;

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
    {	CIO_KEY_IC, CIO_KEY_SHIFT_IC, CIO_KEY_CTRL_IC, CIO_KEY_ALT_IC}, /* key 82 - insert */
    {	CIO_KEY_DC, CIO_KEY_SHIFT_DC, CIO_KEY_CTRL_DC, CIO_KEY_ALT_IC}, /* key 83 - delete */
    {	0xffff, 0xffff, 0xffff, 0xffff }, /* key 84 - sys key */
    {	0xffff, 0xffff, 0xffff, 0xffff }, /* key 85 */
    {	0xffff, 0xffff, 0xffff, 0xffff }, /* key 86 */
    {	0x8500, 0x5787, 0x8900, 0x8b00 }, /* key 87 - F11 */
    {	0x8600, 0x5888, 0x8a00, 0x8c00 }, /* key 88 - F12 */
};

static void resize_xim(void)
{
	int width = bitmap_width * x_cvstat.scaling;
	int height = bitmap_height * x_cvstat.scaling;

	aspect_correct(&width, &height, x_cvstat.aspect_width, x_cvstat.aspect_height);
	if (xim) {
		if (width == xim->width
		    && height == xim->height) {
			if (last) {
				release_buffer(last);
				last = NULL;
			}
			x11.XFillRectangle(dpy, win, gc, 0, 0, x11_window_width, x11_window_height);
			return;
		}
#ifdef XDestroyImage
		XDestroyImage(xim);
#else
		x11.XDestroyImage(xim);
#endif
	}
	if (last) {
		release_buffer(last);
		last = NULL;
	}
	xim = x11.XCreateImage(dpy, &visual, depth, ZPixmap, 0, NULL, width, height, 32, 0);
	xim->data=(char *)calloc(1, xim->bytes_per_line*xim->height);
	x11.XFillRectangle(dpy, win, gc, 0, 0, x11_window_width, x11_window_height);
}

/* Swiped from FreeBSD libc */
static int
my_fls(unsigned long mask)
{
        int bit;

        if (mask == 0)
                return (0);
        for (bit = 1; mask != 1; bit++)
                mask = mask >> 1;
        return (bit);
}

/* Get a connection to the X server and create the window. */
static int init_window()
{
	XGCValues gcv;
	int i;
	XWMHints *wmhints;
	XClassHint *classhints;
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
	x11.utf8 = x11.XInternAtom(dpy, "UTF8_STRING", False);
	x11.targets = x11.XInternAtom(dpy, "TARGETS", False);

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
		/*
		 * TODO: Set VisualIsRGB8 if appropriate...
		 * "appropriate" in this context means it's a sequence of
		 * unpadded uint32_t values in XXRRGGBB format where XX is
		 * ignored, and RR, GG, and BB are Red, Green, Blue values
		 * respectively.
		 */
		base_pixel = ULONG_MAX;
		base_pixel &= ~visual.red_mask;
		base_pixel &= ~visual.green_mask;
		base_pixel &= ~visual.blue_mask;
		r_shift = my_fls(visual.red_mask)-16;
		g_shift = my_fls(visual.green_mask)-16;
		b_shift = my_fls(visual.blue_mask)-16;
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
			      640*x_cvstat.scaling, 400*x_cvstat.scaling, 2, depth, InputOutput, &visual, CWColormap | CWBorderPixel | CWBackPixel, &wa);

	classhints=x11.XAllocClassHint();
	if (classhints)
		classhints->res_name = classhints->res_class = "CIOLIB";
	wmhints=x11.XAllocWMHints();
	if(wmhints) {
		wmhints->initial_state=NormalState;
		wmhints->flags = (StateHint/* | IconPixmapHint | IconMaskHint*/ | InputHint);
		wmhints->input = True;
		x11.XSetWMProperties(dpy, win, NULL, NULL, 0, 0, NULL, wmhints, classhints);
		x11.XFree(wmhints);
	}
	im = x11.XOpenIM(dpy, NULL, "CIOLIB", "CIOLIB");
	if (im != NULL) {
		ic = x11.XCreateIC(im, XNClientWindow, win, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, NULL);
		if (ic)
			x11.XSetICFocus(ic);
	}

	if (classhints)
		x11.XFree(classhints);

	WM_DELETE_WINDOW = x11.XInternAtom(dpy, "WM_DELETE_WINDOW", False);

	gcv.function = GXcopy;
	gcv.foreground = black | 0xff000000;
	gcv.background = white;
	gcv.graphics_exposures = False;
	gc=x11.XCreateGC(dpy, win, GCFunction | GCForeground | GCBackground | GCGraphicsExposures, &gcv);

	x11.XSelectInput(dpy, win, KeyReleaseMask | KeyPressMask |
		     ExposureMask | ButtonPressMask
		     | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask);

	x11.XStoreName(dpy, win, "SyncConsole");
	x11.XSetWMProtocols(dpy, win, &WM_DELETE_WINDOW, 1);

	return(0);
}

/*
 * Actually maps (shows) the window
 */
static void map_window()
{
	XSizeHints *sh;
	int minwidth = bitmap_width;
	int minheight = bitmap_height;

	sh = x11.XAllocSizeHints();
	if (sh == NULL) {
		fprintf(stderr, "Could not get XSizeHints structure");
		exit(1);
	}

	sh->base_width = bitmap_width * x_cvstat.scaling;
	sh->base_height = bitmap_height * x_cvstat.scaling;

	aspect_correct(&sh->base_width, &sh->base_height, x_cvstat.aspect_width, x_cvstat.aspect_height);
	aspect_correct(&minwidth, &minheight, x_cvstat.aspect_width, x_cvstat.aspect_height);

	sh->min_width = sh->width_inc = sh->min_aspect.x = sh->max_aspect.x = minwidth;
	sh->min_height = sh->height_inc = sh->min_aspect.y = sh->max_aspect.y = minheight;

	sh->flags = USSize | PMinSize | PSize | PResizeInc | PAspect;

	x11.XSetWMNormalHints(dpy, win, sh);
	x11.XMapWindow(dpy, win);

	x11.XFree(sh);

	return;
}

/* Resize the window. This function is called after a mode change. */
static void resize_window()
{
	int width = bitmap_width * x_cvstat.scaling;
	int height = bitmap_height * x_cvstat.scaling;

	aspect_correct(&width, &height, x_cvstat.aspect_width, x_cvstat.aspect_height);
	x11.XResizeWindow(dpy, win, width, height);
	resize_xim();

	return;
}

static void init_mode_internal(int mode)
{
	int oldcols;

	oldcols=x_cvstat.cols;

	pthread_mutex_lock(&blinker_lock);
	pthread_mutex_lock(&vstatlock);
	if (last) {
		release_buffer(last);
		last = NULL;
	}
	bitmap_drv_init_mode(mode, &bitmap_width, &bitmap_height);

	/* Deal with 40 col doubling */
	if(oldcols != vstat.cols) {
		if(oldcols == 40)
			vstat.scaling /= 2;
		if(vstat.cols == 40)
			vstat.scaling *= 2;
	}
	if(vstat.scaling < 1)
		vstat.scaling = 1;

	x_cvstat = vstat;
	pthread_mutex_unlock(&vstatlock);
	pthread_mutex_unlock(&blinker_lock);
	map_window();
}

static void check_scaling(void)
{
	if (old_scaling != x_cvstat.scaling) {
		resize_window();
		old_scaling = x_cvstat.scaling;
	}
}

static int init_mode(int mode)
{
	init_mode_internal(mode);
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
    if(init_window())
		return(-1);

	bitmap_drv_init(x11_drawrect, x11_flush);

    /* Initialize mode 3 (text, 80x25, 16 colors) */
    init_mode_internal(3);

    return(0);
}

static void
local_draw_rect(struct rectlist *rect)
{
	int x, y, xoff = 0, yoff = 0;
	unsigned int r, g, b;
	unsigned long pixel;
	int cleft;
	int cright = -100;
	int ctop;
	int cbottom = -1;
	int idx;
	uint32_t last_pixel = 0x55555555;
	struct graphics_buffer *source;

	if (bitmap_width != rect->rect.width || bitmap_height != rect->rect.height) {
		bitmap_drv_free_rect(rect);
		return;
	}

	xoff = (x11_window_width - xim->width) / 2;
	if (xoff < 0)
		xoff = 0;
	yoff = (x11_window_height - xim->height) / 2;
	if (yoff < 0)
		yoff = 0;

	// Scale...
	source = do_scale(rect, x_cvstat.scaling, x_cvstat.scaling, x_cvstat.aspect_width, x_cvstat.aspect_height);
	bitmap_drv_free_rect(rect);
	if (source == NULL)
		return;
	cleft = source->w;
	ctop = source->h;

	xoff = (x11_window_width - source->w) / 2;
	if (xoff < 0)
		xoff = 0;
	yoff = (x11_window_height - source->h) / 2;
	if (yoff < 0)
		yoff = 0;

	if (last && (last->w != source->w || last->h != source->h)) {
		release_buffer(last);
		last = NULL;
	}

	/* TODO: Translate into local colour depth */
	idx = 0;

	for (y = 0; y < source->h; y++) {
		for (x = 0; x < source->w; x++) {
			if (last) {
				if (last->data[idx] != source->data[idx]) {
					if (x < cleft)
						cleft = x;
					if (x > cright)
						cright = x;
					if (y < ctop)
						ctop = y;
					if (y > cbottom)
						cbottom = y;
				}
				else {
					idx++;
					continue;
				}
			}
			if (VisualIsRGB8) {
				pixel = source->data[idx];
				((uint32_t*)xim->data)[idx] = pixel;
			}
			else {
				if (last_pixel != source->data[idx]) {
					last_pixel = source->data[idx];
					r = source->data[idx] >> 16 & 0xff;
					g = source->data[idx] >> 8 & 0xff;
					b = source->data[idx] & 0xff;
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
				}
#ifdef XPutPixel
				XPutPixel(xim, x, y, pixel);
#else
				x11.XPutPixel(xim, x, y, pixel);
#endif
			}
			idx++;
		}
		/* This line was changed */
		// TODO: Previously this did one update per display line...
		if (last && cright >= 0 && (cbottom != y || y == source->h - 1)) {
			x11.XPutImage(dpy, win, gc, xim, cleft, ctop
			    , cleft + xoff, ctop + yoff
			    , (cright - cleft + 1), (cbottom - ctop + 1));
			cleft = source->w;
			cright = cbottom = -100;
			ctop = source->h;
		}
	}

	if (last == NULL)
		x11.XPutImage(dpy, win, gc, xim, 0, 0, xoff, yoff, source->w, source->h);
	else
		release_buffer(last);
	last = source;
}

static void handle_resize_event(int width, int height)
{
	int newFSH=1;
	int newFSW=1;

	aspect_fix(&width, &height, x_cvstat.aspect_width, x_cvstat.aspect_height);
	newFSH=width / bitmap_width;
	newFSW=height / bitmap_height;
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
	 * We only need to resize if the width/height are not even multiples,
	 * or if the two axis don't scale the same way.
	 * Otherwise, we can simply resend everything
	 */
	if (newFSH != newFSW)
		resize_window();
	else
		resize_xim();
	bitmap_drv_request_pixels();
}

static void expose_rect(int x, int y, int width, int height)
{
	int sx,sy,ex,ey;
	int xoff=0, yoff=0;

	xoff = (x11_window_width - xim->width) / 2;
	if (xoff < 0)
		xoff = 0;
	yoff = (x11_window_height - xim->height) / 2;
	if (yoff < 0)
		yoff = 0;

	if (xoff > 0 || yoff > 0) {
		if (x < xoff || y < yoff || x + width > xoff + xim->width || y + height > yoff + xim->height) {
			x11.XFillRectangle(dpy, win, gc, 0, 0, x11_window_width, yoff);
			x11.XFillRectangle(dpy, win, gc, 0, yoff, xoff, yoff + xim->height);
			x11.XFillRectangle(dpy, win, gc, xoff+xim->width, yoff, x11_window_width, yoff + xim->height);
			x11.XFillRectangle(dpy, win, gc, 0, yoff + xim->height, x11_window_width, x11_window_height);
		}
	}

	sx=(x-xoff)/x_cvstat.scaling;
	sy=(y-yoff)/(x_cvstat.scaling);
	if (sx < 0)
		sx = 0;
	if (sy < 0)
		sy = 0;

	ex=(x-xoff)+width-1;
	ey=(y-yoff)+height-1;
	if (ex < 0)
		ex = 0;
	if (ey < 0)
		ey = 0;
	if((ex+1)%x_cvstat.scaling) {
		ex += x_cvstat.scaling-(ex%x_cvstat.scaling);
	}
	if((ey+1)%(x_cvstat.scaling)) {
		ey += x_cvstat.scaling-(ey%(x_cvstat.scaling));
	}
	ex=ex/x_cvstat.scaling;
	ey=ey/(x_cvstat.scaling);

	/* Since we're exposing, we *have* to redraw */
	if (last) {
		release_buffer(last);
		last = NULL;
		bitmap_drv_request_some_pixels(sx, sy, ex-sx+1, ey-sy+1);
	}
	// Do nothing...
	if (sx == ex || sy == ey)
		return;
	bitmap_drv_request_some_pixels(sx, sy, ex-sx+1, ey-sy+1);
}

static bool
xlat_mouse_xy(int *x, int *y)
{
	int xoff, yoff;

	xoff = (x11_window_width - xim->width) / 2;
	if (xoff < 0)
		xoff = 0;
	yoff = (x11_window_height - xim->height) / 2;
	if (yoff < 0)
		yoff = 0;

	if (*x < xoff)
		return false;
	if (*y < yoff)
		return false;
	*x -= xoff;
	*y -= yoff;
	if (*x >= xim->width)
		return false;
	if (*y >= xim->height)
		return false;
	*x *= x_cvstat.scrnwidth;
	*y *= x_cvstat.scrnheight;
	*x /= xim->width;
	*y /= xim->height;
	return true;
}

static int x11_event(XEvent *ev)
{
	if (x11.XFilterEvent(ev, win))
		return 0;
	switch (ev->type) {
		case ClientMessage:
			if (ev->xclient.format == 32 && ev->xclient.data.l[0] == WM_DELETE_WINDOW) {
				uint16_t key=CIO_KEY_QUIT;
				write(key_pipe[1], &key, 2);
			}
			break;
		/* Graphics related events */
		case ConfigureNotify: {
			int width, height;

			if (x11_window_xpos != ev->xconfigure.x || x11_window_ypos != ev->xconfigure.y) {
				x11_window_xpos=ev->xconfigure.x;
				x11_window_ypos=ev->xconfigure.y;
			}
			if (x11_window_width != ev->xconfigure.width || x11_window_height != ev->xconfigure.height) {
				x11_window_width=ev->xconfigure.width;
				x11_window_height=ev->xconfigure.height;
				handle_resize_event(ev->xconfigure.width, ev->xconfigure.height);
				break;
			}
			width = bitmap_width * x_cvstat.scaling;
			height = bitmap_height * x_cvstat.scaling;

			aspect_correct(&width, &height, x_cvstat.aspect_width, x_cvstat.aspect_height);
			if (ev->xconfigure.width != width || ev->xconfigure.height != height) {
				// We can't have the size we requested... accept the size we got.
				int newFSH=1;
				int newFSW=1;

				width = ev->xconfigure.width;
				height = ev->xconfigure.height;
				aspect_fix(&width, &height, x_cvstat.aspect_width, x_cvstat.aspect_height);
				newFSH=width / bitmap_width;
				newFSW=height / bitmap_height;
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

				resize_xim();
				bitmap_drv_request_pixels();
			}
			break;
		}
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

				if(ev->xselection.selection != CONSOLE_CLIPBOARD)
					break;
				if(ev->xselection.requestor!=win)
					break;
				if(ev->xselection.property) {
					x11.XGetWindowProperty(dpy, win, ev->xselection.property, 0, 0, True, AnyPropertyType, &pastebuf_format, &format, &len, &bytes_left, (unsigned char **)(&pastebuf));
					if(bytes_left > 0 && format==8) {
						x11.XGetWindowProperty(dpy, win, ev->xselection.property, 0, bytes_left, True, AnyPropertyType, &pastebuf_format, &format, &len, &dummy, (unsigned char **)&pastebuf);
						if (x11.utf8 && pastebuf_format == x11.utf8) {
							char *opb = pastebuf;
							pastebuf = (char *)utf8_to_cp(CIOLIB_ISO_8859_1, (uint8_t *)pastebuf, '?', strlen(pastebuf), NULL);
							if (pastebuf == NULL)
								pastebuf = opb;
							else
								x11.XFree(opb);
						}
					}
					else
						pastebuf=NULL;
				}
				else
					pastebuf=NULL;

				/* Set paste buffer */
				sem_post(&pastebuf_set);
				sem_wait(&pastebuf_used);
				if (x11.utf8 && pastebuf_format == x11.utf8)
					free(pastebuf);
				else
					x11.XFree(pastebuf);
				pastebuf=NULL;
			}
			break;
		case SelectionRequest:
			{
				XSelectionRequestEvent *req;
				XEvent respond;
				Atom supported[3];
				int count = 0;

				req=&(ev->xselectionrequest);
				pthread_mutex_lock(&copybuf_mutex);
				if (x11.targets == 0)
					x11.targets = x11.XInternAtom(dpy, "TARGETS", False);
				respond.xselection.property=None;
				if(copybuf!=NULL) {
					if(req->target==XA_STRING) {
						char *cpstr = utf8_to_cp(CIOLIB_ISO_8859_1, (uint8_t *)copybuf, '?', strlen(copybuf), NULL);
						if (cpstr != NULL) {
							x11.XChangeProperty(dpy, req->requestor, req->property, XA_STRING, 8, PropModeReplace, (uint8_t *)cpstr, strlen((char *)cpstr));
							respond.xselection.property=req->property;
							free(cpstr);
						}
					}
					else if(req->target == x11.utf8) {
						x11.XChangeProperty(dpy, req->requestor, req->property, x11.utf8, 8, PropModeReplace, (uint8_t *)copybuf, strlen((char *)copybuf));
						respond.xselection.property=req->property;
					}
					else if(req->target == x11.targets) {
						if (x11.utf8 == 0)
							x11.utf8 = x11.XInternAtom(dpy, "UTF8_STRING", False);

						supported[count++] = x11.targets;
						supported[count++] = XA_STRING;
						if (x11.utf8)
							supported[count++] = x11.utf8;
						x11.XChangeProperty(dpy, req->requestor, req->property, XA_ATOM, 32, PropModeReplace, (unsigned char *)supported, count);
						respond.xselection.property=req->property;
					}
				}
				respond.xselection.requestor=req->requestor;
				respond.xselection.selection=req->selection;
				respond.xselection.time=req->time;
				respond.xselection.target=req->target;
				respond.xselection.type=SelectionNotify;
				respond.xselection.display=req->display;
				x11.XSendEvent(dpy,req->requestor,0,0,&respond);
				x11.XFlush(dpy);
				pthread_mutex_unlock(&copybuf_mutex);
			}
			break;

		/* Mouse Events */
		case MotionNotify:
			{
				XMotionEvent *me = (XMotionEvent *)ev;
				if (!xlat_mouse_xy(&me->x, &me->y))
					break;
				int x_res = me->x;
				int y_res = me->y;

				me->x /= x_cvstat.charwidth;
				me->y /= x_cvstat.charheight;
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
				ciomouse_gotevent(CIOLIB_MOUSE_MOVE,me->x,me->y, x_res, y_res);
	    	}
			break;
		case ButtonRelease:
			{
				XButtonEvent *be = (XButtonEvent *)ev;
				if (!xlat_mouse_xy(&be->x, &be->y))
					break;
				int x_res = be->x;
				int y_res = be->y;

				be->x/=x_cvstat.charwidth;
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
					ciomouse_gotevent(CIOLIB_BUTTON_RELEASE(be->button),be->x,be->y, x_res, y_res);
				}
	    	}
			break;
		case ButtonPress:
			{
				XButtonEvent *be = (XButtonEvent *)ev;
				if (!xlat_mouse_xy(&be->x, &be->y))
					break;
				int x_res = be->x;
				int y_res = be->y;

				be->x/=x_cvstat.charwidth;
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
				if (be->button <= 5) {
					ciomouse_gotevent(CIOLIB_BUTTON_PRESS(be->button),be->x,be->y, x_res, y_res);
				}
	    	}
			break;

		/* Keyboard Events */
		case KeyPress:
			{
				static char buf[128];
				static wchar_t wbuf[128];
				KeySym ks;
				int nlock = 0;
				WORD scan = 0xffff;
				Status lus = 0;
				int cnt;
				int i;
				uint8_t ch;

				if (ic)
					cnt = x11.XwcLookupString(ic, (XKeyPressedEvent *)ev, wbuf, sizeof(wbuf)/sizeof(wbuf[0]), &ks, &lus);
				else {
					cnt = x11.XLookupString((XKeyEvent *)ev, buf, sizeof(buf), &ks, NULL);
					lus = XLookupKeySym;
				}

				switch(lus) {
					case XLookupNone:
						ks = 0xffff;
						break;
					case XLookupBoth:
					case XLookupChars:
						if (lus == XLookupChars || ((ev->xkey.state & (Mod1Mask | ControlMask)) == 0)) {
							for (i = 0; i < cnt; i++) {
								ch = cpchar_from_unicode_cpoint(getcodepage(), wbuf[i], 0);
								if (ch) {
									write(key_pipe[1], &ch, 1);
								}
							}
							break;
						}
						// Fallthrough
					case XLookupKeySym:
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
								if (ev->xkey.state & Mod1Mask) {
									if (x_cvstat.scaling > 1)
										x_cvstat.scaling--;
								}
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
								if (ev->xkey.state & Mod1Mask) {
									if (x_cvstat.scaling < 7)
										x_cvstat.scaling++;
								}
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
						break;
				}
				return(1);
			}
		default:
			break;
	}
	return(0);
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
	sem_init(&event_thread_complete, 0, 0);
	atexit(x11_terminate_event_thread);

	if(local_pipe[0] > xfd)
		high_fd=local_pipe[0];
	else
		high_fd=xfd;

	x11.XSync(dpy, False);
	x11_initialized=1;
	sem_post(&init_complete);
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
				* This could cause a problem if something really
				* was wrong with select....
				*/

				/* perror("select"); */
				break;
			case 0:
				/* Timeout */
				break;
			default:
				if (FD_ISSET(xfd, &fdset)) {
					// This blocks for the event...
					x11.XNextEvent(dpy, &ev);
					x11_event(&ev);
					// And this reads anything else from the queue.
					while (QLength(dpy) > 0) {
						x11.XNextEvent(dpy, &ev);
						x11_event(&ev);
					}
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
									else {
										pastebuf=strdup(copybuf);
										pastebuf_format = copybuf_format;
									}
									/* Set paste buffer */
									sem_post(&pastebuf_set);
									sem_wait(&pastebuf_used);
									FREE_AND_NULL(pastebuf);
								}
								else if(sowner!=None) {
									x11.XConvertSelection(dpy, CONSOLE_CLIPBOARD, x11.utf8 ? x11.utf8 : XA_STRING, x11.utf8 ? x11.utf8 : XA_STRING, win, CurrentTime);
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
						case X11_LOCAL_SETICON: {
							Atom wmicon = x11.XInternAtom(dpy, "_NET_WM_ICON", False);
							if (wmicon) {
								x11.XChangeProperty(dpy, win, wmicon, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)lev.data.icon_data, lev.data.icon_data[0] * lev.data.icon_data[1] + 2);
								x11.XFlush(dpy);
							}
							free(lev.data.icon_data);
							break;
						}
						case X11_LOCAL_MOUSEPOINTER: {
							unsigned shape = UINT_MAX;
							Cursor oc = curs;
							switch (lev.data.ptr) {
								case CIOLIB_MOUSEPTR_ARROW:
									// Use default
									break;
								case CIOLIB_MOUSEPTR_BAR:
									shape = XC_xterm;
									break;
							}
							if (shape == UINT_MAX)
								x11.XDefineCursor(dpy, win, None);
							else {
								curs = x11.XCreateFontCursor(dpy, shape);
								x11.XDefineCursor(dpy, win, curs);
							}
							if (oc != None && oc != curs)
								x11.XFreeCursor(dpy, oc);
							break;
						}
					}
				}
		}
	}
	x11.XCloseDisplay(dpy);
	sem_post(&event_thread_complete);
}
