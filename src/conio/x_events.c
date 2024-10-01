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

static void resize_window();

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
int x11_initialized=0;
static sem_t	event_thread_complete;
static int	terminate = 0;
Atom	copybuf_format;
static Atom	pastebuf_format;
bool xrender_found;
bool xinerama_found;
bool xrandr_found;
bool x_internal_scaling = true;
bool got_first_resize;

/*
 * Local variables
 */

enum UsedAtom {
	// UTF8_STRING: https://www.pps.jussieu.fr/~jch/software/UTF8_STRING/UTF8_STRING.text
	ATOM_UTF8_STRING,
	// ICCCM: https://x.org/releases/X11R7.6/doc/xorg-docs/specs/ICCCM/icccm.html
	ATOM_CLIPBOARD,
	ATOM_TARGETS,
	ATOM_WM_CLASS,
	ATOM_WM_DELETE_WINDOW,
	// EWMH: https://specifications.freedesktop.org/wm-spec/wm-spec-1.3.html
	ATOM__NET_FRAME_EXTENTS,
	ATOM__NET_SUPPORTED,
	ATOM__NET_SUPPORTING_WM_CHECK,
	ATOM__NET_WM_DESKTOP,
	ATOM__NET_WM_ICON,
	ATOM__NET_WM_ICON_NAME,
	ATOM__NET_WM_NAME,
	ATOM__NET_WM_PID,
	ATOM__NET_WM_PING,
	ATOM__NET_WM_STATE,
	ATOM__NET_WM_STATE_FULLSCREEN,
	ATOM__NET_WM_STATE_MAXIMIZED_VERT,
	ATOM__NET_WM_STATE_MAXIMIZED_HORZ,
	ATOM__NET_WM_WINDOW_TYPE,
	ATOM__NET_WM_WINDOW_TYPE_NORMAL,
	ATOM__NET_WORKAREA,
	ATOMCOUNT
};

enum AtomStandard {
	UTF8_ATOM,
	ICCCM_ATOM,
	EWMH_ATOM
};

static uint32_t supported_standards;

#define XA_UTF8_STRING (0x10000000UL)

static struct AtomDef {
	const char * const name;
	const enum AtomStandard standard;
	Atom req_type;
	const int format;
	Atom atom;
	bool supported;
} SupportedAtoms[ATOMCOUNT] = {
	// UTF8_STRING: https://www.pps.jussieu.fr/~jch/software/UTF8_STRING/UTF8_STRING.text
	{"UTF8_STRING", UTF8_ATOM, None, 0, None, false},
	// ICCCM: https://x.org/releases/X11R7.6/doc/xorg-docs/specs/ICCCM/icccm.html
	{"CLIPBOARD", ICCCM_ATOM, XA_ATOM, 32, None, false},
	{"TARGETS", ICCCM_ATOM, XA_ATOM, 32, None, false},
	{"WM_CLASS", ICCCM_ATOM, XA_STRING, 8, None, false},
	{"WM_DELETE_WINDOW", ICCCM_ATOM, None, 0, None, false},
	// EWMH: https://specifications.freedesktop.org/wm-spec/wm-spec-1.3.html
	{"_NET_FRAME_EXTENTS", EWMH_ATOM, XA_CARDINAL, 32, None, false},
	{"_NET_SUPPORTED", EWMH_ATOM, XA_ATOM, 32, None, false},
	{"_NET_SUPPORTING_WM_CHECK", EWMH_ATOM, XA_WINDOW, 32, None, false},
	{"_NET_WM_DESKTOP", EWMH_ATOM, XA_CARDINAL, 32, None, false},
	{"_NET_WM_ICON", EWMH_ATOM, XA_CARDINAL, 32, None, false},
	{"_NET_WM_ICON_NAME", EWMH_ATOM, XA_UTF8_STRING, 8, None, false},
	{"_NET_WM_NAME", EWMH_ATOM, XA_UTF8_STRING, 8, None, false},
	{"_NET_WM_PID", EWMH_ATOM, XA_CARDINAL, 32, None, false},
	{"_NET_WM_PING", EWMH_ATOM, None, 0, None, false},
	{"_NET_WM_STATE", EWMH_ATOM, XA_ATOM, 32, None, false},
	{"_NET_WM_STATE_FULLSCREEN", EWMH_ATOM, None, 0, None, false},
	{"_NET_WM_STATE_MAXIMIZED_VERT", EWMH_ATOM, None, 0, None, false},
	{"_NET_WM_STATE_MAXIMIZED_HORZ", EWMH_ATOM, None, 0, None, false},
	{"_NET_WM_WINDOW_TYPE", EWMH_ATOM, XA_ATOM, 32, None, false},
	{"_NET_WM_WINDOW_TYPE_NORMAL", EWMH_ATOM, None, 0, None, false},
	{"_NET_WORKAREA", EWMH_ATOM, XA_CARDINAL, 4, None, false},
};

#define A(name) SupportedAtoms[ATOM_ ## name].atom

/* Sets the atom to be used for copy/paste operations */
static const long _NET_WM_STATE_REMOVE = 0;
static const long _NET_WM_STATE_ADD = 1;

static Display *dpy=NULL;
static Window win;
static Cursor curs = None;
static Visual *visual;
static Colormap wincmap;
static Pixmap icn;
static Pixmap icn_mask;
static bool VisualIsRGB8 = false;
static XImage *xim;
static XIM im;
static XIC ic;
static unsigned int depth=0;
static int xfd;
static unsigned long black;
static unsigned long white;
struct video_stats x_cvstat;
static unsigned long base_pixel;
static int r_shift;
static int g_shift;
static int b_shift;
static struct graphics_buffer *last = NULL;
pthread_mutex_t	last_mutex;
#ifdef WITH_XRENDER
static XRenderPictFormat *xrender_pf = NULL;
static Pixmap xrender_pm = None;
static Picture xrender_src_pict = None;
static Picture xrender_dst_pict = None;
#endif
static bool fullscreen;
static bool fullscreen_pending;
static Window parent;
static Window root;
static char *wm_wm_name;
static Atom copy_paste_selection = XA_PRIMARY;
static bool map_pending = true;
static int pending_width, pending_height;
static int saved_xpos = -1, saved_ypos = -1;
static int saved_width = -1, saved_height = -1;
static double saved_scaling = 0.0;

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

static struct AtomDef *
get_atom_def(Atom atom)
{
	enum UsedAtom a;

	for (a = 0; a < ATOMCOUNT; a++) {
		if (SupportedAtoms[a].atom == atom)
			return &SupportedAtoms[a];
	}

	return NULL;
}

static void
initialize_atoms(void)
{
	enum UsedAtom a;
	Atom atr;
	int afr;
	unsigned long nir;
	unsigned long bytes_left = 4;
	unsigned char *prop = NULL;
	Window w;
	Atom atom;
	struct AtomDef *ad;
	long offset;

	supported_standards |= (1 << UTF8_ATOM);
	supported_standards |= (1 << ICCCM_ATOM);

	for (a = 0; a < ATOMCOUNT; a++) {
		SupportedAtoms[a].atom = x11.XInternAtom(dpy, (char *)SupportedAtoms[a].name, False);
	}

	if (A(UTF8_STRING) == None)
		supported_standards &= ~(1 << UTF8_ATOM);

	if (supported_standards & (1 << UTF8_ATOM)) {
		if (x11.XGetWindowProperty(dpy, root, A(_NET_SUPPORTING_WM_CHECK), 0, 1, False, XA_WINDOW, &atr, &afr, &nir, &bytes_left, &prop) == Success) {
			if (atr == XA_WINDOW) {
				if (nir == 1) {
					w = *(Window *)prop;
					x11.XFree(prop);
					prop = NULL;
					if (x11.XGetWindowProperty(dpy, w, A(_NET_SUPPORTING_WM_CHECK), 0, 1, False, AnyPropertyType, &atr, &afr, &nir, &bytes_left, &prop) == Success) {
						if (atr == XA_WINDOW) {
							if (nir == 1) {
								if (w == *(Window *)prop) {
									supported_standards |= (1 << EWMH_ATOM);
								}
							}
						}
					}
					if (prop != NULL)
						x11.XFree(prop);
					if (x11.XGetWindowProperty(dpy, w, A(_NET_WM_NAME), 0, 0, False, AnyPropertyType, &atr, &afr, &nir, &bytes_left, &prop) == Success) {
						if (prop != NULL)
							x11.XFree(prop);
						if (afr == 8) {
							if (wm_wm_name != NULL)
								free(wm_wm_name);
							wm_wm_name = malloc(bytes_left + 1);
							if (x11.XGetWindowProperty(dpy, w, A(_NET_WM_NAME), 0, bytes_left, False, AnyPropertyType, &atr, &afr, &nir, &bytes_left, &prop) == Success) {
								if (afr == 8) {
									memcpy(wm_wm_name, prop, nir);
									wm_wm_name[nir] = 0;
								}
							}
						}
					}
				}
			}
			if (prop != NULL)
				x11.XFree(prop);
		}
	}

	if (!(supported_standards & (1 << EWMH_ATOM))) {
		for (a = 0; a < ATOMCOUNT; a++) {
			if (SupportedAtoms[a].standard == EWMH_ATOM)
				SupportedAtoms[a].atom = None;
		}
	}
	else {
		for (offset = 0, bytes_left = 1; bytes_left; offset += nir) {
			if (x11.XGetWindowProperty(dpy, root, A(_NET_SUPPORTED), offset, 1, False, XA_ATOM, &atr, &afr, &nir, &bytes_left, &prop) != Success)
				break;
			if (atr != XA_ATOM) {
				x11.XFree(prop);
				break;
			}
			atom = *(Atom *)prop;
			x11.XFree(prop);
			if (atom != None) {
				ad = get_atom_def(atom);
				if (ad != NULL)
					ad->supported = true;
			}
		}
		for (a = 0; a < ATOMCOUNT; a++) {
			if (SupportedAtoms[a].atom != None) {
				if (SupportedAtoms[a].standard != EWMH_ATOM)
					SupportedAtoms[a].supported = true;
				if (!SupportedAtoms[a].supported) {
					SupportedAtoms[a].atom = None;
				}
			}
		}
	}

	for (a = 0; a < ATOMCOUNT; a++) {
		if (SupportedAtoms[a].req_type == XA_UTF8_STRING) {
			if (A(UTF8_STRING) == None) {
				SupportedAtoms[a].supported = false;
				SupportedAtoms[a].atom = None;
			}
			else {
				SupportedAtoms[a].req_type = A(UTF8_STRING);
			}
		}
	}

	/*
	 * ChromeOS doesn't do anything reasonable with PRIMARY...
	 * Hack in use of CLIPBOARD instead.
	 */
	if (wm_wm_name != NULL && strcmp(wm_wm_name, "Sommelier") == 0) {
		if (A(CLIPBOARD) != None)
			copy_paste_selection = A(CLIPBOARD);
	}
}

static bool
set_win_property(enum UsedAtom atom, Atom type, int format, int action, const void *value, int count)
{
	struct AtomDef *ad = &SupportedAtoms[atom];

	if (ad->atom == None)
		return false;
	if (!(supported_standards & (1 << ad->standard)))
		return false;
	return (x11.XChangeProperty(dpy, win, ad->atom, type, format, action, (unsigned char *)value, count) != 0);
}

static bool
fullscreen_geometry(int *x_org, int *y_org, int *width, int *height)
{
	Window root;
	uint64_t dummy;
	unsigned int rw, rh;
	int wx, wy;
#if defined(WITH_XRANDR) || defined(WITH_XINERAMA)
	Window cr;
	int cx, cy;
	int i;
#endif
#ifdef WITH_XRANDR
	static XRRScreenResources *xrrsr = NULL;
	XRRCrtcInfo *xrrci = NULL;
	int searched = 0;
	bool found;
#endif
#ifdef WITH_XINERAMA
	int nscrn;
	XineramaScreenInfo *xsi;
#endif

	if (win == 0)
		return false;

	if (x11.XGetGeometry(dpy, win, (void *)&root, &wx, &wy, &rw, &rh, (void *)&dummy, (void *)&dummy) == 0)
		return false;

#if defined(WITH_XRANDR) || defined(WITH_XINERAMA)
	x11.XTranslateCoordinates(dpy, win, root, wx, wy, &cx, &cy, &cr);
	cx += rw / 2;
	cy += rh / 2;
#endif

#ifdef WITH_XRANDR
	// We likely don't actually need Ximerama... this is always better and almost certainly present
	if (xrandr_found) {
		searched = 0;
		found = false;
		while (searched < 10 && found == false) {
			searched++;
			if (xrrsr == NULL)
				xrrsr = x11.XRRGetScreenResources(dpy, root);
			if (xrrsr == NULL)
				break;
			for (i = 0; i < xrrsr->ncrtc; i++) {
				if (xrrci != NULL)
					x11.XRRFreeCrtcInfo(xrrci);
				xrrci = x11.XRRGetCrtcInfo(dpy, xrrsr, xrrsr->crtcs[i]);
				if (xrrci == NULL) {
					x11.XRRFreeScreenResources(xrrsr);
					xrrsr = NULL;
					break;
				}
				if (cx >= xrrci->x && cx < xrrci->x + xrrci->width
				    && cy >= xrrci->y && cy < xrrci->y + xrrci->height) {
					found = true;
					break;
				}
			}
		}
		if (xrrsr != NULL && !found) {
			if (xrrci != NULL)
				x11.XRRFreeCrtcInfo(xrrci);
			xrrci = x11.XRRGetCrtcInfo(dpy, xrrsr, xrrsr->crtcs[0]);
			if (xrrci != NULL)
				found = true;
		}
		if (found) {
			if (x_org)
				*x_org = xrrci->x;
			if (y_org)
				*y_org = xrrci->y;
			if (width)
				*width = xrrci->width;
			if (height)
				*height = xrrci->height;
			if (xrrci != NULL)
				x11.XRRFreeCrtcInfo(xrrci);
			return true;
		}
	}
#endif
#ifdef WITH_XINERAMA
	if (xinerama_found) {
		// NOTE: Xinerama is limited to a short for the entire screen dimensions.
		if ((xsi = x11.XineramaQueryScreens(dpy, &nscrn)) != NULL && nscrn != 0) {
			for (i = 0; i < nscrn; i++) {
				if (cx >= xsi[i].x_org && cx < xsi[i].x_org + xsi[i].width
				    && cy >= xsi[i].y_org && cy < xsi[i].y_org + xsi[i].height) {
					break;
				}
			}
			if (i == nscrn)
				i = 0;
			if (x_org)
				*x_org = xsi[i].x_org;
			if (y_org)
				*y_org = xsi[i].y_org;
			if (width)
				*width = xsi[i].width;
			if (height)
				*height = xsi[i].height;
			x11.XFree(xsi);
			return true;
		}
	}
#endif
	if (x_org)
		*x_org = 0;
	if (y_org)
		*y_org = 0;
	if (width)
		*width = rw;
	if (height)
		*height = rh;
	return true;
}

static bool
get_frame_extents(int *l, int *r, int *t, int *b)
{
	long *extents;
	unsigned char *prop;
	unsigned long nir;
	unsigned long bytes_left;
	Atom atr;
	int afr;
	bool ret = false;

	if (A(_NET_FRAME_EXTENTS) != None && win != 0) {
		if (x11.XGetWindowProperty(dpy, win, A(_NET_FRAME_EXTENTS), 0, 4, False, XA_CARDINAL, &atr, &afr, &nir, &bytes_left, &prop) == Success) {
			if (atr == XA_CARDINAL && afr == 32 && nir == 4) {
				extents = (long *)prop;
				if (l)
					*l = extents[0];
				if (r)
					*r = extents[1];
				if (t)
					*t = extents[2];
				if (b)
					*b = extents[3];
				ret = true;
			}
			x11.XFree(prop);
		}
	}
	return ret;
}

static bool
x11_get_maxsize(int *w, int *h)
{
	int i;
	long offset;
	int maxw = 0, maxh = 0;
	Atom atr;
	int afr;
	long *ret;
	unsigned long nir;
	unsigned long bytes_left;
	unsigned char *prop;
	long desktop = -1;
	int l, r, t, b;

	if (dpy == NULL)
		return false;
	if (fullscreen) {
		pthread_mutex_lock(&vstatlock);
		if (w)
			*w = vstat.winwidth;
		if (h)
			*h = vstat.winheight;
		pthread_mutex_unlock(&vstatlock);
		return true;
	}
	else {
		// First, try to get _NET_WORKAREA...
		if (A(_NET_WM_DESKTOP) != None && win != 0) {
			if (x11.XGetWindowProperty(dpy, win, A(_NET_WM_DESKTOP), 0, 1, False, XA_CARDINAL, &atr, &afr, &nir, &bytes_left, &prop) == Success) {
				if (atr == XA_CARDINAL && afr == 32 && nir > 0) {
					desktop = *(long *)prop;
				}
				x11.XFree(prop);
			}
		}
		if (A(_NET_WORKAREA) != None) {
			for (i = 0, offset = 0, bytes_left = 1; bytes_left; i++, offset += nir) {
				if (x11.XGetWindowProperty(dpy, root, A(_NET_WORKAREA), offset, 4, False, XA_CARDINAL, &atr, &afr, &nir, &bytes_left, &prop) != Success)
					break;
				if (atr != XA_CARDINAL) {
					x11.XFree(prop);
					break;
				}
				ret = (long *)prop;
				if (desktop == -1) {
					if (nir >= 3) {
						if (ret[2] > maxw)
							maxw = ret[2];
					}
					if (nir >= 4) {
						if (ret[3] > maxh)
							maxh = ret[3];
					}
				}
				else if (desktop == i) {
					if (nir >= 3)
						maxw = ret[2];
					if (nir >= 4)
						maxh = ret[3];
					x11.XFree(prop);
					break;
				}
				x11.XFree(prop);
			}
		}
		if (maxw > 0 && maxh > 0) {
			if (get_frame_extents(&l, &r, &t, &b)) {
				maxw -= l + r;
				maxh -= t + b;
			}
		}
	}

	// Couldn't get work area, get size of screen instead. :(
	if (maxw == 0 || maxh == 0) {
		// We could just use root window size here...
		fullscreen_geometry(NULL, NULL, &maxw, &maxh);
	}

	if (maxw != 0 && maxh != 0) {
		*w = maxw;
		*h = maxh;
		return true;
	}
	return false;
}

static void
resize_pictures(void)
{
#ifdef WITH_XRENDER
	int iw, ih;

	if (xrender_found) {
		if (xrender_pf == NULL)
			xrender_pf = x11.XRenderFindStandardFormat(dpy, PictStandardRGB24);
		if (xrender_pf == NULL)
			xrender_pf = x11.XRenderFindVisualFormat(dpy, visual);
		if (xrender_pf == NULL)
			xrender_pf = x11.XRenderFindVisualFormat(dpy, DefaultVisual(dpy, DefaultScreen(dpy)));

		if (xrender_pm != None)
			x11.XFreePixmap(dpy, xrender_pm);
		xrender_pm = x11.XCreatePixmap(dpy, win, x_cvstat.scrnwidth, x_cvstat.scrnheight, depth);

		if (xrender_src_pict != None)
			x11.XRenderFreePicture(dpy, xrender_src_pict);
		if (xrender_dst_pict != None)
			x11.XRenderFreePicture(dpy, xrender_dst_pict);
		XRenderPictureAttributes pa;
		xrender_src_pict = x11.XRenderCreatePicture(dpy, xrender_pm, xrender_pf, 0, &pa);
		xrender_dst_pict = x11.XRenderCreatePicture(dpy, win, xrender_pf, 0, &pa);
		x11.XRenderSetPictureFilter(dpy, xrender_src_pict, "best", NULL, 0);
		pthread_mutex_lock(&vstatlock);
		bitmap_get_scaled_win_size(vstat.scaling, &iw, &ih, 0, 0);
		pthread_mutex_unlock(&vstatlock);
		XTransform transform_matrix = {{
		  {XDoubleToFixed((double)vstat.scrnwidth / iw), XDoubleToFixed(0), XDoubleToFixed(0)},
		  {XDoubleToFixed(0), XDoubleToFixed((double)vstat.scrnheight / ih), XDoubleToFixed(0)},
		  {XDoubleToFixed(0), XDoubleToFixed(0), XDoubleToFixed(1.0)}
		}};
		x11.XRenderSetPictureTransform(dpy, xrender_src_pict, &transform_matrix);
	}
#endif
}

static void
free_last(void)
{
	pthread_mutex_lock(&last_mutex);
	if (last) {
		release_buffer(last);
		last = NULL;
	}
	pthread_mutex_unlock(&last_mutex);
}

static void resize_xim(void)
{
	int width, height;

	resize_pictures();
	if (x_internal_scaling) {
		pthread_mutex_lock(&vstatlock);
		bitmap_get_scaled_win_size(x_cvstat.scaling, &width, &height, 0, 0);
		pthread_mutex_unlock(&vstatlock);
	}
	else {
		width = x_cvstat.scrnwidth;
		height = x_cvstat.scrnheight;
	}

	if (xim) {
		if (width == xim->width
		    && height == xim->height) {
			free_last();
			return;
		}
#ifdef XDestroyImage
		XDestroyImage(xim);
#else
		x11.XDestroyImage(xim);
#endif
	}
	free_last();
	xim = x11.XCreateImage(dpy, visual, depth, ZPixmap, 0, NULL, width, height, 32, 0);
	xim->data=(char *)calloc(1, xim->bytes_per_line*xim->height);
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

/*
 * Actually maps (shows) the window
 */
static void
map_window(bool mp)
{
	XSizeHints *sh;
	static int last_minw = 0;
	static int last_minh = 0;
	static int last_maxw = 0;
	static int last_maxh = 0;
	static int last_aspectx = 0;
	static int last_aspecty = 0;
	bool extents_changed = false;

	sh = x11.XAllocSizeHints();
	if (sh == NULL) {
		fprintf(stderr, "Could not get XSizeHints structure");
		exit(1);
	}
	sh->flags = 0;

	if (!fullscreen && !fullscreen_pending) {
		if (x11_get_maxsize(&sh->max_width,&sh->max_height)) {
			pthread_mutex_lock(&vstatlock);
			bitmap_get_scaled_win_size(bitmap_double_mult_inside(sh->max_width, sh->max_height), &sh->max_width, &sh->max_height, sh->max_width, sh->max_height);
		}
		else {
			pthread_mutex_lock(&vstatlock);
			bitmap_get_scaled_win_size(7.0, &sh->max_width, &sh->max_height, 0, 0);
		}
		if (sh->max_width != last_maxw)
			extents_changed = true;
		last_maxw = sh->max_width;
		if (!extents_changed && sh->max_height != last_maxh)
			extents_changed = true;
		last_maxh = sh->max_height;
		sh->flags |= PMaxSize;

		bitmap_get_scaled_win_size(x_cvstat.scaling, &sh->base_width, &sh->base_height, sh->max_width, sh->max_height);
		sh->flags |= PBaseSize;

		sh->width = sh->base_width;
		sh->height = sh->base_height;
		sh->flags |= PSize;
	}
	else
		pthread_mutex_lock(&vstatlock);

	bitmap_get_scaled_win_size(1.0, &sh->min_width, &sh->min_height, 0, 0);
	if (sh->min_width != last_minw)
		extents_changed = true;
	last_minw = sh->min_width;
	if (!extents_changed && sh->min_height != last_minh)
		extents_changed = true;
	last_minh = sh->min_height;
	sh->flags |= PMinSize;

	pthread_mutex_unlock(&vstatlock);

	if (x_cvstat.aspect_width != 0 && x_cvstat.aspect_height != 0) {
		sh->min_aspect.x = sh->max_aspect.x = x_cvstat.aspect_width;
		sh->min_aspect.y = sh->max_aspect.y = x_cvstat.aspect_height;
	}
	else {
		sh->min_aspect.x = sh->max_aspect.x = sh->min_width;
		sh->min_aspect.y = sh->max_aspect.y = sh->min_height;
	}
	if (sh->min_aspect.x != last_aspectx)
		extents_changed = true;
	last_aspectx = sh->min_aspect.x;
	if (sh->min_aspect.y != last_aspecty)
		extents_changed = true;
	last_aspecty = sh->min_aspect.y;

	sh->flags |= PAspect;

	/*
	 * It appears that herbstluftwm will give focus to anything calling
	 * XSetWMNormalHints(), so be careful to not do it in response to
	 * a _NET_FRAME_EXTENTS change since we get that when we're losing
	 * focus due to focus follows mouse, which results in SyncTERM and
	 * herbstluftwm fighting over if SyncTERM is focused or not
	 * (SyncTERM wins).
	 */
	if (extents_changed || mp)
		x11.XSetWMNormalHints(dpy, win, sh);
	pthread_mutex_lock(&vstatlock);
	vstat.scaling = x_cvstat.scaling;
	pthread_mutex_unlock(&vstatlock);
	if (mp)
		x11.XMapWindow(dpy, win);

	x11.XFree(sh);

	return;
}

static void
set_icon(const void *data, size_t width, XWMHints *hints)
{
	const uint32_t *idata = data;
	Pixmap opm = icn;
	Pixmap opmm = icn_mask;
	XGCValues gcv = {
		.function = GXcopy,
		.foreground = black | 0xff000000,
		.background = white
	};
	int x,y,i;
	GC igc, imgc;
	bool fail = false;
	unsigned short tmp;
	XColor fg;
	bool sethints = (hints == NULL);
	unsigned long lasti = ULONG_MAX, lastim = ULONG_MAX;

	// This is literally the wost possible way to create a pixmap. :)
	/*
	 * This whole mess was added to get the icon working on ChromeOS...
	 * as it happens, ChromeOS doesn't actually use the X11 icons at
	 * all for anything ever.  Instead it does some weird hackery in
	 * the icon theme and pulls the icon files out to the host.
	 * Leaving this here though since it is marginally "better" aside
	 * from the insane method to create a Pixmap.
	 */
	icn = x11.XCreatePixmap(dpy, root, width, width, depth);
	igc = x11.XCreateGC(dpy, icn, GCFunction | GCForeground | GCBackground | GCGraphicsExposures, &gcv);
	icn_mask = x11.XCreatePixmap(dpy, root, width, width, 1);
	imgc = x11.XCreateGC(dpy, icn_mask, GCFunction | GCForeground | GCBackground | GCGraphicsExposures, &gcv);
	fail = (!icn) || (!icn_mask);
	if (!fail) {
		for (x = 0, i = 0; x < width; x++) {
			for (y = 0; y < width; y++) {
				if (idata[i] & 0xff000000) {
					if (VisualIsRGB8)
						fg.pixel = idata[i] & 0xffffff;
					else {
						tmp = (idata[i] & 0xff);
						fg.red = tmp << 8 | tmp;
						tmp = (idata[i] & 0xff00) >> 8;
						fg.green = tmp << 8 | tmp;
						tmp = (idata[i] & 0xff0000) >> 16;
						fg.blue = tmp << 8 | tmp;
						fg.flags = DoRed | DoGreen | DoBlue;
						if (x11.XAllocColor(dpy, wincmap, &fg) == 0)
							fail = true;
					}
					if (fail)
						break;
					if (lasti != fg.pixel) {
						x11.XSetForeground(dpy, igc, fg.pixel);
						lasti = fg.pixel;
					}
					x11.XDrawPoint(dpy, icn, igc, y, x);
					if (lastim != white) {
						x11.XSetForeground(dpy, imgc, white);
						lastim = white;
					}
					x11.XDrawPoint(dpy, icn_mask, imgc, y, x);
				}
				else {
					if (lastim != black) {
						x11.XSetForeground(dpy, imgc, black);
						lastim = black;
					}
					x11.XDrawPoint(dpy, icn_mask, imgc, y, x);
				}
				i++;
			}
			if (fail)
				break;
		}
	}
	if (!fail) {
		if (!hints)
			hints = x11.XGetWMHints(dpy, win);
		if (!hints)
			hints = x11.XAllocWMHints();
		if (hints) {
			hints->flags |= IconPixmapHint | IconMaskHint;
			hints->icon_pixmap = icn;
			hints->icon_mask = icn_mask;
			if (sethints) {
				x11.XSetWMHints(dpy, win, hints);
				x11.XFree(hints);
			}
			if (opm)
				x11.XFreePixmap(dpy, opm);
			if (opmm)
				x11.XFreePixmap(dpy, opmm);
		}
	}
	x11.XFreeGC(dpy, igc);
	x11.XFreeGC(dpy, imgc);
}

/* Get a connection to the X server and create the window. */
static int init_window()
{
	XGCValues gcv;
	XWMHints *wmhints;
	XClassHint *classhints;
	int w, h;
	int mw, mh;
	int screen;
#if (defined(WITH_XRENDER) || defined(WITH_XINERAMA))
	int major, minor;
#endif
	unsigned long pid_l;
	Atom a;

	dpy = x11.XOpenDisplay(NULL);
	if (dpy == NULL) {
		return(-1);
	}
	root = DefaultRootWindow(dpy);
	x11.XSynchronize(dpy, False);

#ifdef WITH_XRENDER
	if (xrender_found && x11.XRenderQueryVersion(dpy, &major, &minor) == 0)
		xrender_found = false;
#endif
#ifdef WITH_XINERAMA
	if (xinerama_found && x11.XineramaQueryVersion(dpy, &major, &minor) == 0)
		xinerama_found = false;
#endif
#ifdef WITH_XRANDR
	if (xrandr_found && x11.XRRQueryVersion(dpy, &major, &minor) == 0)
		xrandr_found = false;
#endif

	xfd = ConnectionNumber(dpy);
	initialize_atoms();

	screen = DefaultScreen(dpy);
#ifdef DefaultVisual
	visual = DefaultVisual(dpy, screen);
#else
	visual = x11.DefaultVisual(dpy, screen);
#endif
#ifdef DefaultDepth
	depth = DefaultDepth(dpy, screen);
#else
	depth = x11.DefaultDepth(dpy, screen);
#endif
	base_pixel = ULONG_MAX;
	base_pixel &= ~visual->red_mask;
	base_pixel &= ~visual->green_mask;
	base_pixel &= ~visual->blue_mask;
	r_shift = my_fls(visual->red_mask)-16;
	g_shift = my_fls(visual->green_mask)-16;
	b_shift = my_fls(visual->blue_mask)-16;
	if (visual->red_mask == 0xff0000 && visual->green_mask == 0xff00 && visual->blue_mask == 0xff)
		VisualIsRGB8 = true;

	/* Allocate black and white */
	black=BlackPixel(dpy, DefaultScreen(dpy));
	white=WhitePixel(dpy, DefaultScreen(dpy));

	/* Create window, but defer setting a size and GC. */
	XSetWindowAttributes wa = {0};
	parent = root;
	wincmap = x11.XCreateColormap(dpy, root, visual, AllocNone);
	x11.XInstallColormap(dpy, wincmap);
	wa.colormap = wincmap;
	wa.background_pixel = black;
	wa.border_pixel = black;
	x11_get_maxsize(&mw, &mh);
	pthread_mutex_lock(&vstatlock);
	bitmap_get_scaled_win_size(x_cvstat.scaling, &w, &h, mw, mh);
	vstat.winwidth = x_cvstat.winwidth = w;
	vstat.winheight = x_cvstat.winheight = h;
	vstat.scaling = x_cvstat.scaling = bitmap_double_mult_inside(w, h);
	pthread_mutex_unlock(&vstatlock);
	win = x11.XCreateWindow(dpy, parent, 0, 0,
	    w, h, 2, depth, InputOutput, visual, CWColormap | CWBorderPixel | CWBackPixel, &wa);

	classhints=x11.XAllocClassHint();
	if (classhints) {
		classhints->res_name = (char *)ciolib_initial_program_name;
		classhints->res_class = (char *)ciolib_initial_program_class;
	}
	wmhints=x11.XAllocWMHints();
	if(wmhints) {
		wmhints->flags = 0;
		wmhints->initial_state=NormalState;
		wmhints->input = True;
		wmhints->flags |= (StateHint | InputHint);
		set_icon(ciolib_initial_icon, ciolib_initial_icon_width, wmhints);
		// TODO: We could get the window/icon name right at the start...
		x11.XSetWMProperties(dpy, win, NULL, NULL, 0, 0, NULL, wmhints, classhints);
		x11.XFree(wmhints);
	}
	set_win_property(ATOM__NET_WM_ICON, XA_CARDINAL, 32, PropModeReplace, ciolib_initial_icon, ciolib_initial_icon_width * ciolib_initial_icon_width);

	pid_l = getpid();
	set_win_property(ATOM__NET_WM_PID, XA_CARDINAL, 32, PropModeReplace, &pid_l, 1);
	a = A(_NET_WM_WINDOW_TYPE_NORMAL);
	if (a != None)
		set_win_property(ATOM__NET_WM_WINDOW_TYPE, XA_ATOM, 32, PropModeReplace, &a, 1);
	/*
	 * NOTE: Setting this before mapping the window is not something
	 *       that is specified in the EWMH, but it works on at least
	 *       xfwm4 and marco... and I think it would be insane to
	 *       implement _NET_WM_STATE_FULLSCREEN and *not* support
	 *       this.
	 */
	if (fullscreen)
		a = A(_NET_WM_STATE_FULLSCREEN);
	else
		a = None;
	if (a != None)
		set_win_property(ATOM__NET_WM_STATE, XA_ATOM, 32, PropModeReplace, &a, 1);

	im = x11.XOpenIM(dpy, NULL, classhints ? classhints->res_name : "CIOLIB", classhints ? classhints->res_class : "CIOLIB");
	if (im != NULL)
		ic = x11.XCreateIC(im, XNClientWindow, win, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, NULL);

	if (classhints)
		x11.XFree(classhints);

	gcv.function = GXcopy;
	gcv.foreground = black | 0xff000000;
	gcv.background = white;
	gcv.graphics_exposures = False;
	gc=x11.XCreateGC(dpy, win, GCFunction | GCForeground | GCBackground | GCGraphicsExposures, &gcv);

	x11.XSelectInput(dpy, win, KeyReleaseMask | KeyPressMask
		     | ExposureMask | ButtonPressMask | PropertyChangeMask
		     | ButtonReleaseMask | PointerMotionMask
		     | StructureNotifyMask | FocusChangeMask);

	x11.XStoreName(dpy, win, "SyncConsole");
	Atom protos[2];
	int i = 0;
	if (A(WM_DELETE_WINDOW) != None)
		protos[i++] = A(WM_DELETE_WINDOW);
	if (A(_NET_WM_PING) != None)
		protos[i++] = A(_NET_WM_PING);
	if (i)
		x11.XSetWMProtocols(dpy, win, protos, i);

	return(0);
}

static bool
send_fullscreen(bool set, int x, int y)
{
	XEvent ev = {0};
	bool ret = false;
	int l, t;

	if (A(_NET_WM_STATE) != None && A(_NET_WM_STATE_FULLSCREEN) != None) {
		if (fullscreen != set) {
			ev.xclient.type = ClientMessage;
			ev.xclient.serial = 0; // Populated by XSendEvent
			ev.xclient.send_event = True; // Populated by XSendEvent
			ev.xclient.display = dpy;
			ev.xclient.window = win;
			ev.xclient.message_type = A(_NET_WM_STATE);
			ev.xclient.format = 32;
			memset(&ev.xclient.data.l, 0, sizeof(ev.xclient.data.l));
			ev.xclient.data.l[0] = set ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
			ev.xclient.data.l[1] = A(_NET_WM_STATE_FULLSCREEN);
			ev.xclient.data.l[3] = 1;
			fullscreen_pending = true;
			if (set) {
				saved_xpos = x;
				saved_ypos = y;
				if (get_frame_extents(&l, NULL, &t, NULL)) {
					saved_xpos -= l;
					saved_ypos -= t;
				}
				pthread_mutex_lock(&vstatlock);
				saved_width = vstat.winwidth;
				saved_height = vstat.winheight;
				saved_scaling = vstat.scaling;
				pthread_mutex_unlock(&vstatlock);
			}
			ret = x11.XSendEvent(dpy, root, False, SubstructureNotifyMask | SubstructureRedirectMask, &ev) != 0;
			if (ret) {
				x11.XFlush(dpy);
			}
		}
		else
			ret = true;
	}
	return ret;
}

/* Resize the window. This function is called after a mode change. */
// It's also called after scaling is changed!
static void resize_window()
{
	int width;
	int height;
	int max_width, max_height;
	bool resize;
	double new_scaling;

	/*
	 * Don't allow resizing the window when we're in fullscreen mode
	 * or we're transitioning to/from fullscreen modes
	 */
	if (fullscreen || fullscreen_pending)
		return;
	pthread_mutex_lock(&vstatlock);
	cio_api.mode = CIOLIB_MODE_X;
	new_scaling = x_cvstat.scaling;
	bitmap_get_scaled_win_size(new_scaling, &width, &height, 0, 0);
	if (x11_get_maxsize(&max_width, &max_height)) {
		if (width > max_width || height > max_height) {
			new_scaling = bitmap_double_mult_inside(max_width, max_height);
			bitmap_get_scaled_win_size(new_scaling, &width, &height, 0, 0);
		}
	}
	new_scaling = bitmap_double_mult_inside(width, height);
	if (width == vstat.winwidth && height == vstat.winheight) {
		if (new_scaling != vstat.scaling) {
			vstat.scaling = x_cvstat.scaling = new_scaling;
			pthread_mutex_unlock(&vstatlock);
			resize_xim();
		}
		else
			pthread_mutex_unlock(&vstatlock);
		return;
	}
	bitmap_get_scaled_win_size(new_scaling, &width, &height, 0, 0);
	resize = new_scaling != vstat.scaling || width != vstat.winwidth || height != vstat.winheight;
	x_cvstat.scaling = vstat.scaling;
	if (resize) {
		pthread_mutex_unlock(&vstatlock);
		map_window(map_pending);
		pthread_mutex_lock(&vstatlock);
		x11.XResizeWindow(dpy, win, width, height);
		x_cvstat.winwidth = vstat.winwidth = width;
		x_cvstat.winheight = vstat.winheight = height;
		vstat.scaling = x_cvstat.scaling = bitmap_double_mult_inside(width, height);
	}
	pthread_mutex_unlock(&vstatlock);
	resize_xim();

	return;
}

static void init_mode_internal(int mode)
{
	int mw, mh;
	int ow, oh;

	x11_get_maxsize(&mw, &mh);
	free_last();
	pthread_mutex_lock(&vstatlock);
	ow = vstat.winwidth;
	oh = vstat.winheight;
	bitmap_drv_init_mode(mode, NULL, NULL, mw, mh);
	vstat.winwidth = ow;
	vstat.winheight = oh;
	vstat.scaling = bitmap_double_mult_inside(ow, oh);
	pthread_mutex_unlock(&vstatlock);
	resize_window();
	pthread_mutex_lock(&vstatlock);
	x_cvstat = vstat;
	pthread_mutex_unlock(&vstatlock);
	resize_xim();
	map_window(map_pending);
}

static void check_scaling(void)
{
	pthread_mutex_lock(&vstatlock);
	pthread_mutex_lock(&scalinglock);
	if (newscaling != 0) {
		x_cvstat.scaling = newscaling;
		newscaling = 0.0;
		pthread_mutex_unlock(&scalinglock);
		pthread_mutex_unlock(&vstatlock);
		resize_window();
	}
	else {
		pthread_mutex_unlock(&scalinglock);
		pthread_mutex_unlock(&vstatlock);
	}
}

static int init_mode(int mode)
{
	init_mode_internal(mode);
	bitmap_drv_request_pixels();

	sem_post(&mode_set);
	return(0);
}

static int video_init()
{
	int w, h;

	pthread_mutex_lock(&vstatlock);
	x_internal_scaling = (ciolib_initial_scaling_type == CIOLIB_SCALING_INTERNAL);
	if (ciolib_initial_scaling != 0.0) {
		if (ciolib_initial_scaling < 1.0) {
			if (x11_get_maxsize(&w, &h)) {
				ciolib_initial_scaling = bitmap_double_mult_inside(w, h);
			}
			if (ciolib_initial_scaling < 1.0) {
				ciolib_initial_scaling = 1.0;
			}
		}
		x_cvstat.scaling = vstat.scaling = ciolib_initial_scaling;
	}
	if (x_cvstat.scaling < 1.0 || vstat.scaling < 1.0)
		x_cvstat.scaling = vstat.scaling = 1.0;
	if(load_vmode(&vstat, ciolib_initial_mode)) {
		pthread_mutex_unlock(&vstatlock);
		return(-1);
	}
	x_cvstat = vstat;
	pthread_mutex_unlock(&vstatlock);
	if(init_window())
		return(-1);
	bitmap_drv_init(x11_drawrect, x11_flush);
	init_mode_internal(ciolib_initial_mode);

	return(0);
}

static void
local_draw_rect(struct rectlist *rect)
{
	int x, y, xoff = 0, yoff = 0;
	unsigned int r, g, b;
	unsigned long pixel = 0;
	int cleft;
	int cright = -100;
	int ctop;
	int cbottom = -1;
	int idx;
	uint32_t last_pixel = 0x55555555;
	struct graphics_buffer *source;
	int w, h;
	int dw, dh;
	uint32_t *source_data;
	bool internal_scaling = x_internal_scaling;

	if (x_cvstat.scrnwidth != rect->rect.width || x_cvstat.scrnheight != rect->rect.height || xim == NULL) {
		bitmap_drv_free_rect(rect);
		return;
	}

	// Scale...
	pthread_mutex_lock(&vstatlock);
	if (x_cvstat.winwidth != vstat.winwidth || x_cvstat.winheight != vstat.winheight) {
		pthread_mutex_unlock(&vstatlock);
		bitmap_drv_free_rect(rect);
		return;
	}
	bitmap_get_scaled_win_size(vstat.scaling, &w, &h, vstat.winwidth, vstat.winheight);
	pthread_mutex_unlock(&vstatlock);
	if (w < rect->rect.width || h < rect->rect.height) {
		bitmap_drv_free_rect(rect);
		return;
	}
	if (internal_scaling) {
		source = do_scale(rect, w, h);
		bitmap_drv_free_rect(rect);
		if (source == NULL)
			return;
		cleft = source->w;
		ctop = source->h;
		source_data = source->data;
		dw = source->w;
		dh = source->h;
	}
	else {
		source = NULL;
		cleft = w;
		ctop = h;
		source_data = rect->data;
		dw = rect->rect.width;
		dh = rect->rect.height;
	}

	pthread_mutex_lock(&vstatlock);
	w = vstat.winwidth;
	h = vstat.winheight;
	pthread_mutex_unlock(&vstatlock);
	xoff = (w - cleft) / 2;
	if (xoff < 0)
		xoff = 0;
	yoff = (h - ctop) / 2;
	if (yoff < 0)
		yoff = 0;

	if (last && (last->w != dw || last->h != dh)) {
		free_last();
	}

	/* TODO: Translate into local colour depth */
	idx = 0;

	pthread_mutex_lock(&last_mutex);
	for (y = 0; y < dh; y++) {
		for (x = 0; x < dw; x++) {
			if (last) {
				if (last->data[idx] != source_data[idx]) {
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
				pixel = source_data[idx];
				((uint32_t*)xim->data)[idx] = pixel;
			}
			else {
				if (last_pixel != source_data[idx]) {
					last_pixel = source_data[idx];
					r = source_data[idx] >> 16 & 0xff;
					g = source_data[idx] >> 8 & 0xff;
					b = source_data[idx] & 0xff;
					r = (r<<8)|r;
					g = (g<<8)|g;
					b = (b<<8)|b;
					pixel = base_pixel;
					if (r_shift >= 0)
						pixel |= (r << r_shift) & visual->red_mask;
					else
						pixel |= (r >> (0-r_shift)) & visual->red_mask;
					if (g_shift >= 0)
						pixel |= (g << g_shift) & visual->green_mask;
					else
						pixel |= (g >> (0-g_shift)) & visual->green_mask;
					if (b_shift >= 0)
						pixel |= (b << b_shift) & visual->blue_mask;
					else
						pixel |= (b >> (0-b_shift)) & visual->blue_mask;
				}
#ifdef XPutPixel
				XPutPixel(xim, x, y, pixel);
#else
				x11.XPutPixel(xim, x, y, pixel);
#endif
			}
			idx++;
		}
		if (internal_scaling) {
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
	}
	if (!internal_scaling)
		bitmap_drv_free_rect(rect);

	// TODO: We really only need to do this once after changing resolution...
	if (xoff > 0 || yoff > 0) {
		if (yoff != 0) {
			x11.XFillRectangle(dpy, win, gc, 0, 0, w, yoff);
		}
		if (xoff != 0) {
			x11.XFillRectangle(dpy, win, gc, 0, yoff, xoff, yoff + ctop);
		}
		// These clean up odd-numbered widths,
		x11.XFillRectangle(dpy, win, gc, xoff + cleft, yoff, w, yoff + ctop);
		x11.XFillRectangle(dpy, win, gc, 0, yoff + ctop, w, h);
	}
	if (internal_scaling || xrender_found == false) {
		if (last == NULL)
			x11.XPutImage(dpy, win, gc, xim, 0, 0, xoff, yoff, cleft, ctop);
		else {
			release_buffer(last);
			last = NULL;
		}
	}
	else {
		if (last != NULL) {
			release_buffer(last);
			last = NULL;
		}
#ifdef WITH_XRENDER
		x11.XPutImage(dpy, xrender_pm, gc, xim, 0, 0, 0, 0, dw, dh);
		x11.XRenderComposite(dpy, PictOpSrc, xrender_src_pict, 0, xrender_dst_pict,
				0, 0, 0, 0, xoff, yoff,
				cleft, ctop);
#else
		fprintf(stderr, "External scaling enabled without XRender support compiled in!\n");
#endif
	}
	last = source;
	pthread_mutex_lock(&last_mutex);
}

static void handle_resize_event(int width, int height, bool map)
{
	pthread_mutex_lock(&vstatlock);
	x_cvstat.winwidth = vstat.winwidth = width;
	x_cvstat.winheight = vstat.winheight = height;
	vstat.scaling = x_cvstat.scaling = bitmap_double_mult_inside(width, height);
	if (vstat.scaling > 16)
		vstat.scaling = 16;
	x_cvstat.scaling = vstat.scaling;
	pthread_mutex_unlock(&vstatlock);
	resize_xim();
	bitmap_drv_request_pixels();
	if (!got_first_resize) {
		if (!fullscreen) {
			pthread_mutex_lock(&vstatlock);
			vstat.scaling = bitmap_double_mult_inside(width, height);
			pthread_mutex_unlock(&vstatlock);
			resize_window();
		}
	}
}

static void expose_rect(int x, int y, int width, int height)
{
	int xoff=0, yoff=0;
	int w, h;
	int sw, sh;
	double s;

	if (xim == NULL) {
		fprintf(stderr, "Exposing NULL xim!\n");
		return;
	}
	pthread_mutex_lock(&vstatlock);
	w = vstat.winwidth;
	h = vstat.winheight;
	s = vstat.scaling;
	bitmap_get_scaled_win_size(s, &sw, &sh, 0, 0);
	pthread_mutex_unlock(&vstatlock);
	xoff = (w - sw) / 2;
	if (xoff < 0)
		xoff = 0;
	yoff = (h - sh) / 2;
	if (yoff < 0)
		yoff = 0;

	x11.XFillRectangle(dpy, win, gc, 0, 0, w, yoff);
	x11.XFillRectangle(dpy, win, gc, 0, yoff, xoff, yoff + sh);
	x11.XFillRectangle(dpy, win, gc, xoff + sw, yoff, w, yoff + sh);
	x11.XFillRectangle(dpy, win, gc, 0, yoff + sh, w, h);

	/* Since we're exposing, we *have* to redraw */
	free_last();
	bitmap_drv_request_pixels();
}

static bool
xlat_mouse_xy(int *x, int *y)
{
	int xoff, yoff;
	int xw, xh;

	pthread_mutex_lock(&vstatlock);
	bitmap_get_scaled_win_size(vstat.scaling, &xw, &xh, 0, 0);
	xoff = (vstat.winwidth - xw) / 2;
	if (xoff < 0)
		xoff = 0;
	yoff = (vstat.winheight - xh) / 2;
	if (yoff < 0)
		yoff = 0;
	pthread_mutex_unlock(&vstatlock);

	if (*x < xoff)
		*x = xoff;
	if (*y < yoff)
		*y = yoff;
	*x -= xoff;
	*y -= yoff;
	if (*x >= xw)
		*x = xw - 1;
	if (*y >= xh)
		*y = xh - 1;
	*x *= x_cvstat.scrnwidth;
	*y *= x_cvstat.scrnheight;
	*x /= xw;
	*y /= xh;
	return true;
}

static bool
net_wm_state_is_cb(bool (*cb)(Atom))
{
	bool is = false;
	Atom atr;
	int afr;
	Atom *ret;
	unsigned long nir;
	unsigned long bytes_left = 4;
	unsigned char *prop;
	size_t offset;

	if (A(_NET_WM_STATE) != None) {
		for (offset = 0; bytes_left; offset += nir) {
			if (x11.XGetWindowProperty(dpy, win, A(_NET_WM_STATE), offset, 1, False, XA_ATOM, &atr, &afr, &nir, &bytes_left, &prop) != Success)
				break;
			if (atr != XA_ATOM) {
				x11.XFree(prop);
				break;
			}
			ret = (Atom *)prop;
			if (nir > 0 && cb(*ret))
				is = true;
			x11.XFree(prop);
			if (is)
				break;
		}
	}
	return is;
}

static bool
is_maximized_cb(Atom a)
{
	if (a != None && (a == A(_NET_WM_STATE_MAXIMIZED_VERT) || a == A(_NET_WM_STATE_MAXIMIZED_HORZ)))
		return true;
	return false;
}

static bool
is_fullscreen_cb(Atom a)
{
	if (a != None && a == A(_NET_WM_STATE_FULLSCREEN))
		return true;
	return false;
}

static bool
is_maximized(void)
{
	return net_wm_state_is_cb(is_maximized_cb);
}

static bool
is_fullscreen(void)
{
	return net_wm_state_is_cb(is_fullscreen_cb);
}

static void
handle_configuration(int w, int h, bool map, bool se)
{
	bool resize = false;

	pthread_mutex_lock(&vstatlock);
	if (w != vstat.winwidth || h != vstat.winheight)
		resize = true;
	pthread_mutex_unlock(&vstatlock);
	if (resize)
		handle_resize_event(w, h, map);
	if (w && h && !se)
		got_first_resize = true;
}

static void
x11_event(XEvent *ev)
{
	bool resize;
	int x, y, w, h;

	if (x11.XFilterEvent(ev, win))
		return;
	switch (ev->type) {
		case ReparentNotify:
			parent = ev->xreparent.parent;
			break;
		case ClientMessage:
			if (ev->xclient.format == 32 && ev->xclient.data.l[0] == A(WM_DELETE_WINDOW) && A(WM_DELETE_WINDOW) != None) {
				uint16_t key=CIO_KEY_QUIT;
				// Bow to GCC
				if (write(key_pipe[1], &key, 2) == EXIT_FAILURE)
					return;
				else
					return;
			}
			else if(ev->xclient.format == 32 && ev->xclient.data.l[0] == A(_NET_WM_PING) && A(_NET_WM_PING) != None) {
				ev->xclient.window = root;
				x11.XSendEvent(dpy, ev->xclient.window, False, SubstructureNotifyMask | SubstructureRedirectMask, ev);
			}
			break;
		case PropertyNotify:
			if (A(_NET_FRAME_EXTENTS) != None) {
				if (ev->xproperty.atom == A(_NET_FRAME_EXTENTS)) {
					// Don't map the window in respose to _NET_FRAME_EXTENTS change
					map_window(false);
				}
			}
			if (A(_NET_WM_STATE) != None) {
				if (ev->xproperty.atom == A(_NET_WM_STATE)) {
					fullscreen_pending = false;
					if (is_fullscreen()) {
						if (!fullscreen) {
							fullscreen = true;
							if (fullscreen_geometry(&x, &y, &w, &h)) {
								if (x_cvstat.winwidth != w || x_cvstat.winheight != h) {
									x_cvstat.winwidth = w;
									x_cvstat.winheight = h;
									x_cvstat.scaling = bitmap_double_mult_inside(w, h);
									resize_xim();
								}
							}
						}
					}
					else {
						resize = false;
						if (fullscreen) {
							fullscreen = false;
							pthread_mutex_lock(&vstatlock);
							x_cvstat.scaling = saved_scaling;
							/*
							 * Mode may have changed while in fullscreen... recalculate scaling to
							 * fit inside the old window size
							 */
							bitmap_get_scaled_win_size(saved_scaling, &w, &h, saved_width, saved_height);
							if (w != vstat.winwidth || h != vstat.winheight)
								resize = true;
							pthread_mutex_unlock(&vstatlock);
							x_cvstat.winwidth = w;
							x_cvstat.winheight = h;
							x_cvstat.scaling = bitmap_double_mult_inside(w, h);
							if (resize) {
								x11.XMoveResizeWindow(dpy, win, saved_xpos, saved_ypos, w, h);
								resize_xim();
							}
							else
								x11.XMoveWindow(dpy, win, saved_xpos, saved_ypos);
						}
					}
				}
			}
			break;
		/* Graphics related events */
		case ConfigureNotify: {
			/*
			 * NOTE: The x/y values in the event are relative to root if send_event is true, and
			 * relative to the parent (which is the above member) if send_event is false.  Trying
			 * to translate from parent to root in here is a bad idea as there's a race condition.
			 * Basically, if we care about the x/y pos, we should not use it when send_event is
			 * false... if that happens, the position is unknown and we would need to explicitly
			 * query it, or wait for some other event (key, mouse, etc) to give us a root-relative
			 * value.
			 */
			if (ev->xconfigure.window == win) {
				if (map_pending) {
					pending_width = ev->xconfigure.width;
					pending_height = ev->xconfigure.height;
				}
				else {
					handle_configuration(ev->xconfigure.width, ev->xconfigure.height, false, ev->xconfigure.send_event);
				}
			}
			break;
		}
		case MapNotify:
			if (map_pending) {
				map_pending = false;
				handle_configuration(pending_width, pending_height, true, true);
			}
			break;
		case NoExpose:
			break;
		case GraphicsExpose:
			expose_rect(ev->xgraphicsexpose.x, ev->xgraphicsexpose.y, ev->xgraphicsexpose.width, ev->xgraphicsexpose.height);
			break;
		case Expose:
			if (ev->xexpose.count == 0)
				expose_rect(ev->xexpose.x, ev->xexpose.y, ev->xexpose.width, ev->xexpose.height);
			break;

		/* Focus Events */
		case FocusIn:
		case FocusOut:
			{
				if (ev->xfocus.mode == NotifyGrab)
					break;
				if (ev->xfocus.mode == NotifyUngrab)
					break;
				if (ev->xfocus.detail == NotifyInferior)
					break;
				if (ev->xfocus.detail == NotifyPointer)
					break;
				if (ev->type == FocusIn) {
					if (ic)
						x11.XSetICFocus(ic);
				}
				else {
					if (ic)
						x11.XUnsetICFocus(ic);
				}
				break;
			}

		/* Copy/Paste events */
		case SelectionClear:
			{
				XSelectionClearEvent *req;

				req=&(ev->xselectionclear);
				pthread_mutex_lock(&copybuf_mutex);
				if(req->selection==copy_paste_selection)
					FREE_AND_NULL(copybuf);
				pthread_mutex_unlock(&copybuf_mutex);
			}
			break;
		case SelectionNotify:
			{
				int format;
				unsigned long len, bytes_left, dummy;

				if(ev->xselection.selection != copy_paste_selection)
					break;
				if(ev->xselection.requestor!=win)
					break;
				if(ev->xselection.property) {
					x11.XGetWindowProperty(dpy, win, ev->xselection.property, 0, 0, True, AnyPropertyType, &pastebuf_format, &format, &len, &bytes_left, (unsigned char **)(&pastebuf));
					if(bytes_left > 0 && format==8) {
						x11.XGetWindowProperty(dpy, win, ev->xselection.property, 0, bytes_left, True, AnyPropertyType, &pastebuf_format, &format, &len, &dummy, (unsigned char **)&pastebuf);
						if (!(A(UTF8_STRING) && pastebuf_format == A(UTF8_STRING))) {
							char *opb = pastebuf;
							pastebuf = (char *)cp_to_utf8(CIOLIB_ISO_8859_1, pastebuf, strlen(pastebuf), NULL);
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
				if (A(UTF8_STRING) && pastebuf_format == A(UTF8_STRING))
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
					else if(req->target == A(UTF8_STRING)) {
						x11.XChangeProperty(dpy, req->requestor, req->property, A(UTF8_STRING), 8, PropModeReplace, (uint8_t *)copybuf, strlen((char *)copybuf));
						respond.xselection.property=req->property;
					}
					else if(req->target == A(TARGETS)) {
						supported[count++] = A(TARGETS);
						supported[count++] = XA_STRING;
						if (A(UTF8_STRING))
							supported[count++] = A(UTF8_STRING);
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
								if (wbuf[i] < 128)
									ch = cpchar_from_unicode_cpoint(getcodepage(), wbuf[i], wbuf[i]);
								else
									ch = cpchar_from_unicode_cpoint(getcodepage(), wbuf[i], 0);
								if (ch) {
									// Bow to GCC
									if (ch == 0xe0) // Double-up 0xe0
										if (write(key_pipe[1], &ch, 1) == -1)
											return;
									if (write(key_pipe[1], &ch, 1) == EXIT_SUCCESS)
										return;
									else
										return;
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
								if (ev->xkey.state & Mod1Mask) {
									// ALT-Enter, toggle full-screen
									send_fullscreen(!fullscreen, ev->xkey.x_root - ev->xkey.x, ev->xkey.y_root - ev->xkey.y);
								}
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
								if (ev->xkey.state & Mod1Mask && !is_maximized()) {
									double fval;
									double ival;
									fval = modf(x_cvstat.scaling, &ival);
									if (fval != 0.0) {
										if (ival < 1.0)
											ival = 1.0;
										x_setscaling(ival);
									}
									else if (ival > 1.0)
										x_setscaling(ival - 1.0);
									else
										x_setscaling(1.0);
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
								if (ev->xkey.state & Mod1Mask && !is_maximized()) {
									double ival;
									modf(x_cvstat.scaling, &ival);
									if (ival < 1.0)
										ival = 1.0;
									if (ival < 7.0) {
										int mw, mh, ms;
										x11_get_maxsize(&mw,&mh);
										pthread_mutex_lock(&vstatlock);
										ms = bitmap_largest_mult_inside(mw, mh);
										pthread_mutex_unlock(&vstatlock);
										if (ival + 1 <= ms)
											x_setscaling(ival + 1);
									}
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
								} else if (ev->xkey.state & ShiftMask) {
									scan = ScanCodes[scan & 0xff].shift;
								} else if (ev->xkey.state & ControlMask) {
									scan = ScanCodes[scan & 0xff].ctrl;
								} else if (scan & 0x100) {
									scan = ScanCodes[scan & 0xff].shift;
								}  else
									scan = ScanCodes[scan & 0xff].base;
								break;
						}
						if (scan != 0xffff) {
							uint16_t key=scan;
							if (key < 128)
								key = cpchar_from_unicode_cpoint(getcodepage(), key, key);
							// Bow to GCC
							if (key == 0xe0)
								key = CIO_KEY_LITERAL_E0;
							if (write(key_pipe[1], &key, (scan & 0xff) ? 1 : 2) != EXIT_SUCCESS)
								return;
							else
								return;
						}
						break;
				}
				return;
			}
		default:
			break;
	}
	return;
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
	int mode = (int)(intptr_t)args;
	int flush_count = 0;

	SetThreadName("X11 Events");
	if (mode == CIOLIB_MODE_X_FULLSCREEN)
		fullscreen = true;
	if(video_init()) {
		sem_post(&init_complete);
		return;
	}
	sem_init(&event_thread_complete, 0, 0);
	pthread_mutex_init(&last_mutex, NULL);

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
							set_win_property(ATOM__NET_WM_ICON_NAME, A(UTF8_STRING), 8, PropModeReplace, lev.data.name, strlen(lev.data.name));
							x11.XFlush(dpy);
							break;
						case X11_LOCAL_SETTITLE:
							x11.XStoreName(dpy, win, lev.data.title);
							set_win_property(ATOM__NET_WM_NAME, A(UTF8_STRING), 8, PropModeReplace, lev.data.name, strlen(lev.data.name));
							x11.XFlush(dpy);
							break;
						case X11_LOCAL_COPY:
							x11.XSetSelectionOwner(dpy, copy_paste_selection, win, CurrentTime);
							break;
						case X11_LOCAL_PASTE:
							{
								Window sowner=None;

								sowner=x11.XGetSelectionOwner(dpy, copy_paste_selection);
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
									x11.XConvertSelection(dpy, copy_paste_selection, A(UTF8_STRING) ? A(UTF8_STRING) : XA_STRING, A(UTF8_STRING) ? A(UTF8_STRING) : XA_STRING, win, CurrentTime);
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
							if (!got_first_resize) {
								if (++flush_count > 3)
									got_first_resize = true;
							}
							break;
						case X11_LOCAL_BEEP:
							x11.XBell(dpy, 100);
							break;
						case X11_LOCAL_SETICON: {
							// TODO: set_icon() is obsolete... delete
							set_icon(&lev.data.icon_data[2], lev.data.icon_data[0], NULL);
							set_win_property(ATOM__NET_WM_ICON, XA_CARDINAL, 32, PropModeReplace, lev.data.icon_data, lev.data.icon_data[0] * lev.data.icon_data[1] + 2);
							x11.XFlush(dpy);
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
						case X11_LOCAL_SETSCALING_TYPE:
							x_internal_scaling = (lev.data.st == CIOLIB_SCALING_INTERNAL);
							resize_xim();
							break;
					}
				}
		}
	}
	x11.XCloseDisplay(dpy);
	sem_post(&event_thread_complete);
}
