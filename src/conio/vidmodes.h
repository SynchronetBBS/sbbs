/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2004 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef _VIDMODES_H_
#define _VIDMODES_H_

#if (defined CIOLIB_IMPORTS)
 #undef CIOLIB_IMPORTS
#endif
#if (defined CIOLIB_EXPORTS)
 #undef CIOLIB_EXPORTS
#endif

#include "ciolib.h"

#define TOTAL_DAC_SIZE	34

/* Entry type for the DAC table. */
struct dac_colors {
	unsigned char red;
	unsigned char green;
	unsigned char blue;
};

struct  video_params {
	int	mode;
	int palette;
	int	cols;
	int rows;
	int	curs_start;
	int curs_end;
	int charheight;
	int charwidth;
};

struct video_stats {
	int rows;
	int cols;
	int curs_row;
	int curs_col;
	int curs_start;
	int curs_end;
	int curs_blink;
	int curs_visible;
	int default_curs_start;
	int default_curs_end;
	int mode;
	int charheight;
	int charwidth;
	int bright_background;
	int blink;
	int no_bright;
	int bright_altcharset;
	int currattr;
	int scaling;
	struct dac_colors dac_colors[256];
	unsigned char palette[16];
	unsigned short *vmem;
};

enum {
	 MONO_PALETTE
	,GREYSCALE_PALETTE
	,COLOUR_PALETTE
	,C64_PALETTE
	,ATARI_PALETTE
};

extern struct video_params vparams[49];
#define NUMMODES      (sizeof(vparams) / sizeof(struct video_params))
extern unsigned char palettes[5][16];
extern struct dac_colors dac_default[TOTAL_DAC_SIZE];
extern char vga_font_bitmap[4096];
extern char vga_font_bitmap14[3584];
extern char vga_font_bitmap8[2048];

#ifdef __cplusplus
extern "C" {
#endif
int find_vmode(int mode);
int load_vmode(struct video_stats *vs, int mode);
#ifdef __cplusplus
}
#endif

#endif
