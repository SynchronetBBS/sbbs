/*
 * Copyright (c) 1992, 1993, 1996
 *      Berkeley Software Design, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Berkeley Software
 *      Design, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY Berkeley Software Design, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Berkeley Software Design, Inc. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/user.h>

#include <fcntl.h>
#include <limits.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>	/* sysconf() */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "console.h"
#include "vparams.h"

/* Console definition variables */
BYTE VideoMode;
int FW, FH, FD;
WORD DpyCols;
BYTE DpyRows;
BYTE *palette;
WORD DpyPageSize;
BYTE CursStart;
BYTE CursEnd;
WORD CharHeight;
WORD NumColors;
BYTE VertResolution;
WORD *vmem;
int vattr;
BYTE VGA_ATC[ATC_Size];
BYTE VGA_CRTC[CRTC_Size];
static int show = 1;
static int blink = 1;
BYTE CursRow0=0;
BYTE CursCol0=0;
typedef struct TextLine {
    u_short	*data;
    u_char	max_length;	/* Not used, but here for future use */
    u_char	changed:1;
} TextLine;
TextLine *lines = NULL;

/* Indices into the video parameter table. We will use that array to
   initialize the registers on startup and when the video mode changes. */
#define CRTC_Ofs	10
#define ATC_Ofs		35
#define TSC_Ofs		5
#define GDC_Ofs		55
#define MiscOutput_Ofs	9

/* X Variables */
Display *dpy=NULL;
Window win;
XFontStruct *font;
XImage *xi = 0;
Visual *visual;
unsigned int depth;
unsigned long black;
unsigned long white;
int FW, FH, FD;
GC gc;
GC cgc;
int xfd;

/* LUT for the vram -> XImage conversion */
BYTE lut[4][256][8];

/* X pixel values for the RGB triples */
DWORD pixels[16];

const char *xfont;


/* Table of supported video modes. */
vmode_t vmodelist[] = {
    {0x00, 0x17, TEXT, 16, 8, 2, 0xb8000, FONT8x16},
    {0x01, 0x17, TEXT, 16, 8, 2, 0xb8000, FONT8x16},
    {0x02, 0x18, TEXT, 16, 8, 2, 0xb8000, FONT8x16},
    {0x03, 0x18, TEXT, 16, 8, 2, 0xb8000, FONT8x16},
    {0x04, 0x04, TEXT, 4, 1, 0, 0xb8000, FONT8x8},
    {0x05, 0x05, TEXT, 4, 1, 0, 0xb8000, FONT8x8},
    {0x06, 0x06, TEXT, 2, 1, 0, 0xb8000, FONT8x8},
    {0x07, 0x19, TEXT, 1, 8, 2, 0xb0000, FONT8x16},
    {0x08, 0x08, NOMODE, 0, 0, 0, 0, 0},
    {0x09, 0x09, NOMODE, 0, 0, 0, 0, 0},
    {0x0a, 0x0a, NOMODE, 0, 0, 0, 0, 0},
    {0x0b, 0x0b, NOMODE, 0, 0, 0, 0, 0},
    {0x0c, 0x0c, NOMODE, 0, 0, 0, 0, 0},
    {0x0d, 0x0d, TEXT, 16, 8, 0, 0xa0000, FONT8x8},
    {0x0e, 0x0e, TEXT, 16, 4, 0, 0xa0000, FONT8x8},
    {0x0f, 0x11, TEXT, 1, 2, 1, 0xa0000, FONT8x14},
    {0x10, 0x12, TEXT, 16, 2, 1, 0xa0000, FONT8x14},
    {0x11, 0x1a, TEXT, 2, 1, 3, 0xa0000, FONT8x16},
    {0x12, 0x1b, TEXT, 16, 1, 3, 0xa0000, FONT8x16},
    /*     {0x13, 0x1c, GRAPHICS, 256, 1, 0, 0xa0000, FONT8x8}, */
};

#define NUMMODES	(sizeof(vmodelist) / sizeof(vmode_t))

static	fd_set	fdset;		/* File Descriptors to select on */

/* Keyboard stuff */
WORD	keybuf[0x25];
#define	K_NEXT		keybuf[0x21] /* *(u_short *)0x41a */
#define	K_FREE		keybuf[0x22] /* *(u_short *)0x41c */
#define	K_BUFSTARTP	keybuf[0x23] /* *(u_short *)0x480 */
#define	K_BUFENDP	keybuf[0x24] /* *(u_short *)0x482 */
#define	K_BUFSTART	(&keybuf[K_BUFSTARTP]) /* ((u_short *)(0x400 + K_BUFSTARTP)) */
#define	K_BUFEND	(&keybuf[K_BUFENDP]) /* ((u_short *)(0x400 + keybuf[3])) */
#define	K_BUF(i)	keybuf[i] /* *((u_short *)((u_char *)0x400 + (i))) */

BYTE	K1_STATUS;
#define	K1_RSHIFT	0x01
#define	K1_LSHIFT	0x02
#define	K1_SHIFT	0x03
#define	K1_CTRL		0x04
#define	K1_ALT		0x08
#define	K1_SLOCK	0x10		/* Active */
#define	K1_NLOCK	0x20		/* Active */
#define	K1_CLOCK	0x40		/* Active */
#define	K1_INSERT	0x80		/* Active */

BYTE	K2_STATUS;
#define	K2_LCTRL	0x01
#define	K2_LALT		0x02
#define	K2_SYSREQ	0x04
#define	K2_PAUSE	0x08
#define	K2_SLOCK	0x10		/* Actually held down */
#define	K2_NLOCK	0x20		/* Actually held down */
#define	K2_CLOCK	0x40		/* Actually held down */
#define	K2_INSERT	0x80		/* Actually held down */

BYTE	K3_STATUS;
#define	K3_E1		0x01		/* Last code read was e1 */
#define	K3_E2		0x02		/* Last code read was e2 */
#define	K3_RCTRL	0x04
#define	K3_RALT		0x08
#define	K3_ENHANCED	0x10
#define	K3_FORCENLOCK	0x20
#define	K3_TWOBYTE	0x40		/* last code was first of 2 */
#define	K3_READID	0x80		/* read ID in progress */

BYTE	K4_STATUS;
#define	K4_SLOCK_LED	0x01
#define	K4_NLOCK_LED	0x02
#define	K4_CLOCK_LED	0x04
#define	K4_ACK		0x10		/* ACK recieved from keyboard */
#define	K4_RESEND	0x20		/* RESEND recieved from keyboard */
#define	K4_LED		0x40		/* LED update in progress */
#define	K4_ERROR	0x80

int flipdelete = 0;		/* Flip meaning of delete and backspace */
static WORD break_code = 0x00;

static u_short Ascii2Scan[] = {
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

struct {
    u_short	base;
    u_short	shift;
    u_short	ctrl;
    u_short	alt;
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
    {	0x4800, 0x4838, 0xffff, 0xffff }, /* key 72 - cursor up */
    {	0x4900, 0x4939, 0x8400, 0xffff }, /* key 73 - page up */
    {	0x4a2d, 0x4a2d, 0xffff, 0xffff }, /* key 74 - minus sign */
    {	0x4b00, 0x4b34, 0x7300, 0xffff }, /* key 75 - cursor left */
    {	0xffff, 0x4c35, 0xffff, 0xffff }, /* key 76 - center key */
    {	0x4d00, 0x4d36, 0x7400, 0xffff }, /* key 77 - cursor right */
    {	0x4e2b, 0x4e2b, 0xffff, 0xffff }, /* key 78 - plus sign */
    {	0x4f00, 0x4f31, 0x7500, 0xffff }, /* key 79 - end */
    {	0x5000, 0x5032, 0xffff, 0xffff }, /* key 80 - cursor down */
    {	0x5100, 0x5133, 0x7600, 0xffff }, /* key 81 - page down */
    {	0x5200, 0x5230, 0xffff, 0xffff }, /* key 82 - insert */
    {	0x5300, 0x532e, 0xffff, 0xffff }, /* key 83 - delete */
    {	0xffff, 0xffff, 0xffff, 0xffff }, /* key 84 - sys key */
    {	0xffff, 0xffff, 0xffff, 0xffff }, /* key 85 */
    {	0xffff, 0xffff, 0xffff, 0xffff }, /* key 86 */
    {	0x8500, 0x5787, 0x8900, 0x8b00 }, /* key 87 - F11 */
    {	0x8600, 0x5888, 0x8a00, 0x8c00 }, /* key 88 - F12 */
};

/* Mouse Stuff */
typedef struct {
    WORD		hardcursor:1;
    WORD		installed:1;
    WORD		cursor:1;
    WORD		show:1;
    WORD		buttons:3;

    WORD		init;
    WORD		start;
    WORD		end;
    WORD		hmickey;
    WORD		vmickey;
    WORD		doubling;
    DWORD		handler;
    WORD		mask;
    DWORD		althandler[3];
    WORD		altmask[3];
    struct {
	WORD	x;
	WORD	y;
	WORD	w;
	WORD	h;
    }	range, exclude;
    WORD		x;
    WORD		y;
    WORD		lastx;
    WORD		lasty;
    
    WORD		downs[3];
    WORD		ups[3];
} mouse_t;

mouse_t		mouse_status;

#define	HWM	16

void tty_pause()
{
    sigset_t set;

    sigprocmask(0, 0, &set);
    sigdelset(&set, SIGIO);
    sigdelset(&set, SIGALRM);
    sigsuspend(&set);
}

volatile int	poll_cnt = 0;

void
wakeup_poll(void)
{
    if (poll_cnt <= 0)
	poll_cnt = HWM;
}

void
reset_poll(void)
{
    poll_cnt = HWM;
}

void
sleep_poll(void)
{
    if (--poll_cnt <= 0) {
	poll_cnt = 0;
	while (KbdEmpty() && poll_cnt <= 0) {
	    if (KbdEmpty() && poll_cnt <= 0)
		tty_pause();
	}
    }
}

static void
setgc(u_short attr)
{
	XGCValues v;
	if (blink && !show && (attr & 0x8000))
		v.foreground = pixels[(attr >> 12) & 0x07];
	else
		v.foreground = pixels[(attr >> 8) & 0x0f];

	v.background = pixels[(attr >> 12) & (blink ? 0x07 : 0x0f)];
	XChangeGC(dpy, gc, GCForeground|GCBackground, &v);
}

static void
video_update_text()
{
    static int or = -1;
    static int oc = -1;

    static char buf[256];
    int r, c;
    int attr = vmem[0] & 0xff00;
    XGCValues v;

	wakeup_poll();	/* Wake up anyone waiting on kbd poll */

/*	show ^= 1; */

	setgc(attr);

	for (r = 0; r < (DpyRows+1); ++r) {
	    int cc = 0;

	    if (!lines[r].changed) {
		if ((r == or || r == CursRow0) && (or != CursRow0 || oc !=CursCol0))
		    lines[r].changed = 1;
		else {
		    for (c = 0; c < DpyCols; ++c) {
			if (lines[r].data[c] != vmem[r * DpyCols + c]) {
			    lines[r].changed = 1;
			    break;
			}
			if (blink && lines[r].data[c] & 0x8000) {
			    lines[r].changed = 1;
			    break;
			}
		    }
		}
	    }

	    if (!lines[r].changed)
		continue;

	    reset_poll();
	    lines[r].changed = 0;
	    memcpy(lines[r].data,
		   &vmem[r * DpyCols], sizeof(u_short) * DpyCols);

	    for (c = 0; c < DpyCols; ++c) {
		int cv = vmem[r * DpyCols + c];
		if ((cv & 0xff00) != attr) {
		    if (cc < c)
			XDrawImageString(dpy, win, gc,
					 2 + cc * FW,
					 2 + (r + 1) * FH,
					 buf + cc, c - cc);
		    cc = c;
		    attr = cv  & 0xff00;
		    setgc(attr);
		}
		buf[c] = (cv & 0xff) ? cv & 0xff : ' ';
	    }
	    if (cc < c) {
		XDrawImageString(dpy, win, gc,
				 2 + cc * FW,
				 2 + (r + 1) * FH,
				 buf + cc, c - cc);
	    }
	}
	or =CursRow0;
	oc =CursCol0;

	if (CursStart <= CursEnd && CursEnd <= FH &&
	    show &&CursRow0 < (DpyRows+1) &&CursCol0 < DpyCols) {

	    attr = vmem[CursRow0 * DpyCols +CursCol0] & 0xff00;
	    v.foreground = pixels[(attr >> 8) & 0x0f] ^
		pixels[(attr >> 12) & (blink ? 0x07 : 0x0f)];
	    if (v.foreground) {
		v.function = GXxor;
	    } else {
		v.foreground = pixels[7];
		v.function = GXcopy;
	    }
	    XChangeGC(dpy, cgc, GCForeground | GCFunction, &v);
	    XFillRectangle(dpy, win, cgc,
			   2 +CursCol0 * FW,
			   2 + CursRow0 * FH + CursStart + FD,
			   FW, CursEnd + 1 - CursStart);
	}

	XFlush(dpy);
}

void
video_update(void *unused)
{
    sigset_t sigset;
	static int icnt = 3;

    sigemptyset(&sigset);
    sigaddset(&sigset, SIGIO);
    sigaddset(&sigset, SIGALRM);
    sigprocmask(SIG_BLOCK, &sigset, 0);

    	if (--icnt == 0) {
	    icnt = 6;
	show ^= 1;

	}
	    /* quick and dirty */
		video_update_text();

    sigemptyset(&sigset);
    sigaddset(&sigset, SIGIO);
    sigaddset(&sigset, SIGALRM);
    sigprocmask(SIG_UNBLOCK, &sigset, 0);
}

/*
** periodic updates
*/
void
sigalrm(int sig)
{
    video_update(NULL);
}

/* Get memory for the text line buffer. */
void
get_lines()
{
    int i;

    if (lines == NULL) {
	lines = (TextLine *)malloc(sizeof(TextLine) * (DpyRows+1));
	if (lines == NULL)
	    err(1, "Could not allocate data structure for text lines\n");

	for (i = 0; i < (DpyRows+1); ++i) {
	    lines[i].max_length = DpyCols;
	    lines[i].data = (u_short *)malloc(DpyCols * sizeof(u_short));
	    if (lines[i].data == NULL)
		err(1, "Could not allocate data structure for text lines\n");
	    lines[i].changed = 1;
	}
    } else {
	lines = (TextLine *)realloc(lines, sizeof(TextLine) * (DpyRows+1));
	if (lines == NULL)
	    err(1, "Could not allocate data structure for text lines\n");

	for (i = 0; i < (DpyRows+1); ++i) {
	    lines[i].max_length = DpyCols;
	    lines[i].data = (u_short *)realloc(lines[i].data,
					       DpyCols * sizeof(u_short));
	    if (lines[i].data == NULL)
		err(1, "Could not allocate data structure for text lines\n");
	    lines[i].changed = 1;
	}
    }
}

static void
Failure(void *arg)
{
        fprintf(stderr, "X Connection shutdown\n");
	exit(1);
}

void
KbdWrite(WORD code)
{
	int kf;

	kf = K_FREE + 2;
	if (kf == K_BUFENDP)
		kf = K_BUFSTARTP;

	if (kf == K_NEXT) {
		XBell(dpy, 0);
		return;
	}
	K_BUF(K_FREE) = code;
	K_FREE = kf;
}

void tty_beep(void)
{
	XBell(dpy, 0);
}

static int
video_event(XEvent *ev)
{
	switch (ev->type) {
	case MotionNotify: {
		XMotionEvent *me = (XMotionEvent *)ev;
		me->x -= 2;
		me->y -= 2;

		mouse_status.x = (me->x < mouse_status.range.x)
				    ? mouse_status.range.x
				    : (me->x > mouse_status.range.w)
				    ? mouse_status.range.w : me->x;
		mouse_status.y = (me->y < mouse_status.range.y)
				    ? mouse_status.range.y
				    : (me->y > mouse_status.range.h)
				    ? mouse_status.range.h : me->y;
		break;
	    }
	case ButtonRelease: {
		XButtonEvent *be = (XButtonEvent *)ev;
		be->x -= 2;
		be->y -= 2;

		if (be->button < 3)
		    mouse_status.ups[be->button]++;

		mouse_status.x = (be->x < mouse_status.range.x)
				    ? mouse_status.range.x
				    : (be->x > mouse_status.range.w)
				    ? mouse_status.range.w : be->x;
		mouse_status.y = (be->y < mouse_status.range.y)
				    ? mouse_status.range.y
				    : (be->y > mouse_status.range.h)
				    ? mouse_status.range.h : be->y;
		break;
	    }
	case ButtonPress: {
		XButtonEvent *be = (XButtonEvent *)ev;
		be->x -= 2;
		be->y -= 2;

		if (be->button < 3)
		    mouse_status.downs[be->button]++;

		mouse_status.x = (be->x < mouse_status.range.x)
				    ? mouse_status.range.x
				    : (be->x > mouse_status.range.w)
				    ? mouse_status.range.w : be->x;
		mouse_status.y = (be->y < mouse_status.range.y)
				    ? mouse_status.range.y
				    : (be->y > mouse_status.range.h)
				    ? mouse_status.range.h : be->y;

		break;
	    }
        case NoExpose:
                break;
        case GraphicsExpose:
        case Expose: {
		int r;
		for (r = 0; r < (DpyRows+1); ++r)
		    lines[r].changed = 1;
		break;
	    }
	case KeyRelease: {
		static char buf[128];
		KeySym ks;

		break_code |= 0x80;

    	    	if (!(ev->xkey.state & ShiftMask)) {
		    K1_STATUS &= ~K1_LSHIFT;
		    K1_STATUS &= ~K1_RSHIFT;
		}
    	    	if (!(ev->xkey.state & ControlMask)) {
			K1_STATUS &= ~K1_CTRL;
			K2_STATUS &= ~K2_LCTRL;
			K3_STATUS &= ~K3_RCTRL;
		}
    	    	if (!(ev->xkey.state & Mod1Mask)) {
                        K1_STATUS &= ~K1_ALT;
                        K2_STATUS &= ~K2_LALT;
                        K3_STATUS &= ~K3_RALT;
		}
    	    	if (!(ev->xkey.state & LockMask)) {
                        K2_STATUS &= ~K2_CLOCK;
		}

		XLookupString((XKeyEvent *)ev, buf, sizeof(buf), &ks, 0);
		switch (ks) {
		case XK_Shift_L:
			K1_STATUS &= ~K1_LSHIFT;
			break;
		case XK_Shift_R:
			K1_STATUS &= ~K1_RSHIFT;
			break;
		case XK_Control_L:
			K1_STATUS &= ~K1_CTRL;
			K2_STATUS &= ~K2_LCTRL;
			break;
		case XK_Control_R:
			K1_STATUS &= ~K1_CTRL;
			K3_STATUS &= ~K3_RCTRL;
			break;
		case XK_Alt_L:
			K1_STATUS &= ~K1_ALT;
			K2_STATUS &= ~K2_LALT;
			break;
		case XK_Alt_R:
			K1_STATUS &= ~K1_ALT;
			K3_STATUS &= ~K3_RALT;
			break;
		case XK_Scroll_Lock:
			K2_STATUS &= ~K2_SLOCK;
			break;
		case XK_Num_Lock:
			K2_STATUS &= ~K2_NLOCK;
			break;
		case XK_Caps_Lock:
			K2_STATUS &= ~K2_CLOCK;
			break;
		case XK_Insert:
			K2_STATUS &= ~K2_INSERT;
			break;
		}
		return(1);
	    }
	case KeyPress: {
		static char buf[128];
		KeySym ks;
		int n;
    	    	int nlock = 0;
		u_short scan = 0xffff;

    	    	if (!(ev->xkey.state & ShiftMask)) {
		    K1_STATUS &= ~K1_LSHIFT;
		    K1_STATUS &= ~K1_RSHIFT;
		}
    	    	if (!(ev->xkey.state & ControlMask)) {
			K1_STATUS &= ~K1_CTRL;
			K2_STATUS &= ~K2_LCTRL;
			K3_STATUS &= ~K3_RCTRL;
		}
    	    	if (!(ev->xkey.state & Mod1Mask)) {
                        K1_STATUS &= ~K1_ALT;
                        K2_STATUS &= ~K2_LALT;
                        K3_STATUS &= ~K3_RALT;
		}
    	    	if (!(ev->xkey.state & LockMask)) {
                        K2_STATUS &= ~K2_CLOCK;
		}

		n = XLookupString((XKeyEvent *)ev, buf, sizeof(buf), &ks, 0);

		switch (ks) {
		case XK_Shift_L:
			K1_STATUS |= K1_LSHIFT;
			break;
		case XK_Shift_R:
			K1_STATUS |= K1_RSHIFT;
			break;
		case XK_Control_L:
			K1_STATUS |= K1_CTRL;
			K2_STATUS |= K2_LCTRL;
			break;
		case XK_Control_R:
			K1_STATUS |= K1_CTRL;
			K3_STATUS |= K3_RCTRL;
			break;
		case XK_Alt_L:
			K1_STATUS |= K1_ALT;
			K2_STATUS |= K2_LALT;
			break;
		case XK_Alt_R:
			K1_STATUS |= K1_ALT;
			K3_STATUS |= K3_RALT;
			break;
		case XK_Scroll_Lock:
			K1_STATUS ^= K1_SLOCK;
			K2_STATUS |= K2_SLOCK;
			break;
		case XK_Num_Lock:
			K1_STATUS ^= K1_NLOCK;
			K2_STATUS |= K2_NLOCK;
			break;
		case XK_Caps_Lock:
			K1_STATUS ^= K1_CLOCK;
			K2_STATUS |= K2_CLOCK;
			break;
		case XK_Insert:
		case XK_KP_Insert:
			K1_STATUS ^= K1_INSERT;
			K2_STATUS |= K2_INSERT;
			scan = 82;
			goto docode;

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
    	    	/* case XK_Insert: This is above */
			scan = 82;
			goto docode;

    	    	case XK_KP_Decimal:
			nlock = 1;
			scan = 83;
			goto docode;

    	    	case XK_Delete:
    	    	case XK_KP_Delete:
			scan = flipdelete ? 14 : 83;
			goto docode;

		case XK_BackSpace:
			scan = flipdelete ? 83 : 14;
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
    	    	    	if ((K1_STATUS&(K1_ALT|K1_CTRL)) == (K1_ALT|K1_CTRL)) {
				if (ks == 'R' || ks == 'r') {
                                    kill(getpid(), SIGALRM);	/* redraw */
				    break;
				}
			}
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

    	    	    	if ((K1_STATUS & K1_SHIFT) || (scan & 0x100)) {
			    scan = ScanCodes[scan & 0xff].shift;
			} else if (K1_STATUS & K1_CTRL) {
			    scan = ScanCodes[scan & 0xff].ctrl;
			} else if (K1_STATUS & K1_ALT) {
			    scan = ScanCodes[scan & 0xff].alt;
			}  else
			    scan = ScanCodes[scan & 0xff].base;

			break;
		}
		if (scan != 0xffff) {
			break_code = scan >> 8;
			KbdWrite(scan);
		}
		return(1);
	    }

	default:
		break;
	}
    	return(0);
}

void
video_async_event(int sig)
{
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGIO);
    sigaddset(&sigset, SIGALRM);
    sigprocmask(SIG_BLOCK, &sigset, 0);

	for (;;) {
                int x;
                fd_set fdset;
                XEvent ev;  
                static struct timeval tv;
				tv.tv_sec=0;
				tv.tv_usec=0;
                /*
                 * Handle any events just sitting around...
                 */
                XFlush(dpy);
                while (QLength(dpy) > 0) {
                        XNextEvent(dpy, &ev);
                        video_event(&ev);
                }

                FD_ZERO(&fdset);
                FD_SET(xfd, &fdset);

                x = select(xfd+1, &fdset, 0, 0, &tv);

                switch (x) {  
                case -1:
                        /*
                         * Errno might be wrong, so we just select again.
                         * This could cause a problem is something really
                         * was wrong with select....
                         */

                        perror("select");
						sigprocmask(SIG_UNBLOCK, &sigset, 0);
                        return;
                case 0:
			XFlush(dpy);
                        return;
                default:
                        if (FD_ISSET(xfd, &fdset)) {
                                do {
                                        XNextEvent(dpy, &ev);
                                        video_event(&ev);
                                } while (QLength(dpy));
                        }
                        break;
                }
        }
		sigprocmask(SIG_UNBLOCK, &sigset, 0);
}

/* Resize the window, using information from 'vga_status[]'. This function is
   called after a mode change. */
void
resize_window()
{
    XSizeHints *sh;
    vmode_t vmode;
    
    sh = XAllocSizeHints();
    if (sh == NULL)
	err(1, "Could not get XSizeHints structure");
    
    vmode = vmodelist[find_vmode(VideoMode)];
	sh->base_width = FW * DpyCols + 4;
	sh->base_height = FH * (DpyRows+1) + 4;
	sh->base_width += 4;
	sh->base_height += 4;

    sh->min_width = sh->max_width = sh->base_width;
    sh->min_height = sh->max_height = sh->base_height;
    sh->flags = USSize | PMinSize | PMaxSize | PSize;

    XSetWMNormalHints(dpy, win, sh);
    XResizeWindow(dpy, win, sh->base_width, sh->base_height);
    XMapWindow(dpy, win);
    XFlush(dpy);
    
    XFree(sh);
    
    return;
}

int
load_font()
{
    XGCValues gcv;
    
    if (!xfont)
	xfont = FONTVGA;

    font = XLoadQueryFont(dpy, xfont);

    if (font == NULL)
	font = XLoadQueryFont(dpy, FONTVGA);

    if (font == NULL) {
		return(-1);
	err(1, "Could not open font ``%s''\n", xfont);
    }

    gcv.font = font->fid;
    XChangeGC(dpy, gc, GCFont, &gcv);
    
    FW = font->max_bounds.width;
    FH = font->max_bounds.ascent + font->max_bounds.descent;
    FD = font->max_bounds.descent;

    return(0);
}

/* Calculate 16 bit RGB values for X from the 6 bit DAC values and the
   palette. This works for 16 and 256 color modes, although we don't really
   support the latter yet. */
static void
dac2rgb(XColor *color, int i)
{
    int n, m;

    /* For the 16 color modes, check bit 7 of the Mode Control register in
       the ATC. If set, we take bits 0-3 of the Color Select register and
       bits 0-3 of the palette register 'i' to build the index into the
       DAC table; otherwise, bits 2 and 3 of the CS reg and bits 0-5 of
       the palette register are used. Note that the entries in 'palette[]'
       are supposed to be already masked to 6 bits. */
    if (VGA_ATC[ATC_ModeCtrl] & 0x80) {
	n = VGA_ATC[ATC_ColorSelect] & 0x0f;
	m = palette[i] & 0x0f;
    } else {
	n = VGA_ATC[ATC_ColorSelect] & 0x0c;
	m = palette[i];
    }
    color->red   = dac_rgb[16*n + m].red << 10;
    color->green = dac_rgb[16*n + m].green << 10;
    color->blue  = dac_rgb[16*n + m].blue << 10;
}

/* Calculate 'pixels[]' from the current DAC table and palette.

   To do: do not use 'pixels[]', use an array of 'XColor's which we can
   allocate and free on demand. Install a private colormap if necessary. */
void
update_pixels()
{
    int i;
	Colormap cm;

    /* We support only 16 colors for now. */
    for (i = 0; i < 16; i++) {
	XColor color;

	dac2rgb(&color, i);
	if (XAllocColor(dpy, DefaultColormap(dpy, DefaultScreen(dpy)), &color)) {
	    pixels[i] = color.pixel;
	} else if (i < 7)
	    pixels[i] = BlackPixel(dpy, DefaultScreen(dpy));
	else
	    pixels[i] = WhitePixel(dpy, DefaultScreen(dpy));
    }
}

/* Find the requested mode in the 'vmodelist' table. This function returns the
   index into this table; we will also use the index for accessing the
   'videoparams' array. */
int find_vmode(int mode)
{
    unsigned i;

    for (i = 0; i < NUMMODES; i++)
	if (vmodelist[i].modenumber == mode)
	    return i;
	
    return -1;
}

int
init_mode(int mode)
{
    vmode_t vmode;
    int idx;			/* Index into vmode */
    int pidx;			/* Index into videoparams */
    int i;
    
    idx = find_vmode(mode & 0x7f);
    if (idx == -1 || vmodelist[idx].type == NOMODE) {
		return(-1);
	}
    vmode = vmodelist[idx];
    pidx = vmode.paramindex;

    /* Preset VGA registers. */
    memcpy(VGA_CRTC, (u_int8_t *)&videoparams[pidx][CRTC_Ofs],
	   sizeof(VGA_CRTC));
    memcpy(VGA_ATC, (u_int8_t *)&videoparams[pidx][ATC_Ofs],
	   sizeof(VGA_ATC));

    VideoMode=mode & 0x7f;
    DpyCols = (WORD)videoparams[pidx][0];
    DpyPageSize = *(WORD *)&videoparams[pidx][3];
    CursStart = VGA_CRTC[CRTC_CursStart];
    CursEnd = VGA_CRTC[CRTC_CursEnd];
    DpyRows = videoparams[pidx][1];
    CharHeight = videoparams[pidx][2];

    NumColors = vmode.numcolors;
    VertResolution = vmode.vrescode;
    vmem = (WORD *)malloc(64*1024);

    /* Load 'pixels[]' from default DAC values. */
    update_pixels();

    /* Update font. */
    xfont = vmode.fontname;
    if(load_font())
		return(-1);

    /* Resize window if necessary. */
    resize_window();

	get_lines();
	if (mode & 0x80)
	    return;
	/* Initialize video memory with black background, white foreground */
	vattr = 0x0700;
	for (i = 0; i < DpyPageSize / 2; ++i)
	    vmem[i] = vattr;

    return(0);
}

/* Prepare the LUT for the VRAM -> XImage conversion. */
static void
prepare_lut()
{
    int i, j, k;

    for (i = 0; i < 4; i++) {
	for (j = 0; j < 256; j++) {
	    for (k = 0; k < 8; k++) {
		lut[i][j][7 - k] = ((j & (1 << k)) ? (1 << i) : 0);
	    }
	}
    }

    return;
}

int
timer_init(void)
{
    struct itimerval	itv;
    struct timeval	tv;
    time_t		tv_sec;
    struct timezone	tz;

    signal(SIGALRM, sigalrm);
    itv.it_interval.tv_sec = 0;
    itv.it_interval.tv_usec = 54925;	/* 1193182/65536 times per second */
    itv.it_value.tv_sec = 0;
    itv.it_value.tv_usec = 54925;	/* 1193182/65536 times per second */
    setitimer(ITIMER_REAL, &itv, 0);
	return(0);
}

/* Get a connection to the X server and create the window. */
int
init_window()
{
    XGCValues gcv;
    int i;

    {
	/*
	 * Arg...  I can no longer change X's fd out from under it.
	 * Open up all the available fd's, leave 3 behind for X
	 * to play with, open X and then release all the other fds
	 */
	int nfds = sysconf(_SC_OPEN_MAX);
	int *fds = (int *)malloc(sizeof(int) * nfds);
	i = 0;
	if (fds)
	    for (i = 0; i < nfds && (i == 0 || fds[i-1] < 63); ++i)
		if ((fds[i] = open(_PATH_DEVNULL, 0)) < 0)
		    break;
	/*
	 * Leave 3 fds behind for X to play with
	 */
	if (i > 0) close(fds[--i]);
	if (i > 0) close(fds[--i]);
	if (i > 0) close(fds[--i]);

	dpy = XOpenDisplay(NULL);

	while (i > 0)
	    close(fds[--i]);
    }
    if (dpy == NULL) {
		return(-1);
	}
    xfd = ConnectionNumber(dpy);

	if (fcntl(xfd, F_SETOWN, getpid()) < 0) {
/*@*/                   perror("SETOWN");
	}
	fcntl(xfd,F_SETFL,fcntl(xfd,F_GETFL) | FASYNC);

	signal(SIGIO, video_async_event);

    /* Create window, but defer setting a size and GC. */
    win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0,
			      1, 1, 2, black, black);

    gcv.foreground = white;
    gcv.background = black;
    gc = XCreateGC(dpy, win, GCForeground | GCBackground, &gcv);

    gcv.foreground = 1;
    gcv.background = 0;
    gcv.function = GXxor;
    cgc = XCreateGC(dpy, win, GCForeground|GCBackground|GCFunction, &gcv);

    XSelectInput(dpy, win, KeyReleaseMask | KeyPressMask |
		     ExposureMask | ButtonPressMask
		     | ButtonReleaseMask | PointerMotionMask );

    XStoreName(dpy, win, "SyncConsole");

    /* Get the default visual and depth for later use. */
    depth = DefaultDepth(dpy, DefaultScreen(dpy));
    visual = DefaultVisual(dpy, DefaultScreen(dpy));

    prepare_lut();

	return(0);
}

/* Initialize the VGA emulator

   XXX This is not nearly finished right now.
*/
static int
init_vga(void)
{
    int i;

    /* Zero-fill 'dac_rgb' on allocation; the default (EGA) table has only
       64 entries. */
    dac_rgb = (struct dac_colors *)calloc(256, sizeof(struct dac_colors));
    if (dac_rgb == NULL)
		return(-1);

    /* Copy the default DAC table to a working copy we can trash. */
    for (i = 0; i < 64; i++)
	dac_rgb[i] = dac_default64[i]; /* Structure copy */

    /* Point 'palette[]' to the Attribute Controller space. We will only use
       the first 16 slots. */
    palette = VGA_ATC;

	return(0);
}

int
kbd_init()
{
	K_BUFSTARTP = 0x0000;	/* Start of keyboard buffer */
	K_BUFENDP = 0x20;	/* End of keyboard buffer */
	K_NEXT = K_FREE = K_BUFSTARTP;
	
	return(0);
}

int
mouse_init(void)
{
	return(0);
}

int
console_init()
{
    int fd;
    int i;    
    sigset_t sigset;
    
	if(dpy!=NULL)
		return(0);

    sigemptyset(&sigset);
    sigaddset(&sigset, SIGIO);
    sigaddset(&sigset, SIGALRM);
    sigprocmask(SIG_BLOCK, &sigset, 0);

    if(kbd_init())
		return(-1);
    if(video_init())
		return(-1);
    if(mouse_init())
		return(-1);
    if(timer_init())
		return(-1);

    sigemptyset(&sigset);
    sigaddset(&sigset, SIGIO);
    sigaddset(&sigset, SIGALRM);
    sigprocmask(SIG_UNBLOCK, &sigset, 0);
	return(0);
}

int
video_init()
{
    /* If we are running under X, get a connection to the X server and create
       an empty window of size (1, 1). It makes a couple of init functions a
       lot easier. */
    if(init_window())
		return(-1);

    /* Set VGA emulator to a sane state */
    if(init_vga())
		return(-1);

    /* Initialize mode 3 (text, 80x25, 16 colors) */
    if(init_mode(3))
		return(-1);

    return(0);
}

WORD
KbdPeek()
{
	return(K_BUF(K_NEXT));
}

WORD
KbdRead()
{
	int kf = K_NEXT;

	K_NEXT = K_NEXT + 2;
	if (K_NEXT == K_BUFENDP)
		K_NEXT = K_BUFSTARTP;

	return(K_BUF(kf));
}

int
KbdEmpty()
{
	return(K_NEXT == K_FREE);
}

static int nextchar = 0;

int
tty_read(int flag)
{
    int r;

    if ((r = nextchar) != 0) {
	nextchar = 0;
	return(r & 0xff);
    }

    if (KbdEmpty()) {
	if (flag & TTYF_BLOCK) {
	    while (KbdEmpty())
		tty_pause();
	} else {
	    return(-1);
	}
    }

    r = KbdRead();
    if ((r & 0xff) == 0)
	nextchar = r >> 8;
    r &= 0xff;
    return(r & 0xff);
}

int
tty_peek(int flag)
{
	int c;

    	if (c == nextchar)
	    return(nextchar & 0xff);

	if (KbdEmpty()) {
		if (flag & TTYF_POLL) {
			sleep_poll();
			if (KbdEmpty())
				return(0);
		} else if (flag & TTYF_BLOCK) {
			while (KbdEmpty())
				tty_pause();
		} else
			return(0);
	}
	c = KbdPeek();
	return(0xff);
}

int
tty_kbhit(void)
{
	if(nextchar || !KbdEmpty())
		return(1);
	return(0);
}
