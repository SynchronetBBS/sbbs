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


#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/user.h>

#include <gen_defs.h>

/* Types. 'NOMODE' is one of the 'forbidden' internal modes. */
#define TEXT		0
#define GRAPHICS	1
#define NOMODE		-1

/* Font names */
#define	FONTVGA		"vga"
#define FONT8x8		"vga8x8"
#define FONT8x14	"vga8x14"
#define FONT8x16	"vga8x16"	/* same as FONTVGA */

/* VGA Stuff */
/* CRTC registers
   
   We use the VGA register functions and don't care about the MDA. We also
   leave out the undocumented registers at 0x22, 0x24, 0x3?. */
#define	CRTC_HorzTotal		0x00
#define	CRTC_HorzDispEnd	0x01
#define CRTC_StartHorzBlank	0x02
#define	CRTC_EndHorzBlank	0x03
#define	CRTC_StartHorzRetrace	0x04
#define	CRTC_EndHorzRetrace	0x05
#define	CRTC_VertTotal		0x06
#define	CRTC_Overflow		0x07
#define	CRTC_ResetRowScan	0x08
#define	CRTC_MaxScanLine	0x09
#define	CRTC_CursStart		0x0a
#define	CRTC_CursEnd		0x0b
#define	CRTC_StartAddrHi	0x0c
#define	CRTC_StartAddrLo	0x0d
#define	CRTC_CurLocHi		0x0e
#define	CRTC_CurLocLo		0x0f
#define CRTC_StartVertRetrace	0x10
#define CRTC_EndVertRetrace	0x11
#define CRTC_VertDispEnd	0x12
#define CRTC_Offset		0x13
#define CRTC_UnderlineLoc	0x14
#define CRTC_StartVertBlank	0x15
#define CRTC_EndVertBlank	0x16
#define CRTC_ModeCtrl		0x17
#define CRTC_LineCompare	0x18

#define CRTC_Size		0x19

/* ATC registers

   The palette registers are here for completeness. We'll always use a
   separate array 'palette[]' to access them in our code. */
#define ATC_Palette0		0x00
#define ATC_Palette1		0x01
#define ATC_Palette2		0x02
#define ATC_Palette3		0x03
#define ATC_Palette4		0x04
#define ATC_Palette5		0x05
#define ATC_Palette6		0x06
#define ATC_Palette7		0x07
#define ATC_Palette8		0x08
#define ATC_Palette9		0x09
#define ATC_PaletteA		0x0a
#define ATC_PaletteB		0x0b
#define ATC_PaletteC		0x0c
#define ATC_PaletteD		0x0d
#define ATC_PaletteE		0x0e
#define ATC_PaletteF		0x0f
#define ATC_ModeCtrl		0x10
#define ATC_OverscanColor	0x11
#define ATC_ColorPlaneEnable	0x12
#define ATC_HorzPixelPanning	0x13
#define ATC_ColorSelect		0x14

#define ATC_Size		0x15

enum {
	AS_RD = 1,
	AS_WR = 2
};

/* standard register representation */
typedef union 
{
    DWORD	r_ex;
    struct 
    {
	WORD	r_x;
	WORD	:16;
    } r_w;
    struct
    {
	BYTE	r_l;
	BYTE	r_h;
	WORD :16;
    } r_b;
} reg86_t;

/*
 * Video modes
 *
 * This started as a big 'switch' statement in 'video.c:init_mode()' which
 * soon became too ugly and unmanagable. So, we collect all mode related
 * information in one table and define a couple of helper function to access
 * it. This will also benefit the VESA support, whenever we get to that.
 */
typedef struct {
    int modenumber;		/* Mode number */
    int paramindex;		/* Index into the parameter table */
    int type;			/* Text or Graphics */
    int numcolors;		/* Number of colors */
    int numpages;		/* Number of display pages */
    int vrescode;		/* 0 = 200, 1 = 350, 2 = 400, 3 = 480 */
    DWORD vmemaddr;		/* Video memory address */
    const char *fontname;	/* Font name */
} vmode_t;

/* Entry type for the DAC table. Each value is actually 6 bits wide. */
struct dac_colors {
    BYTE red;
    BYTE green;
    BYTE blue;
};

/* We need a working copy of the default DAC table. This is filled from
   'dac_default{64,256}[]' in 'video.c:init_vga()'. */
struct dac_colors *dac_rgb;

extern WORD *vmem;

extern BYTE CursRow0;
extern BYTE CursCol0;
extern BYTE CursStart;
extern BYTE CursEnd;

extern BYTE VideoMode;
extern WORD DpyCols;
extern BYTE DpyRows;

extern int FH;

extern int x_nextchar;

extern BYTE VGA_CRTC[CRTC_Size];

int init_window();
int video_init();
int init_mode(int mode);
int tty_read(int flag);
int tty_peek(int flag);
int tty_kbhit(void);
void tty_beep(void);
void x_win_title(const char *title);

#define	TTYF_BLOCK	0x00000008
#define	TTYF_POLL	0x00000010

#endif
