/* $Id: vidmodes.h,v 1.33 2020/06/27 00:04:45 deuce Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
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

#define TOTAL_DAC_SIZE	274

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
	int	vmultiplier;
	int	default_attr;
	int	flags;
	int	scale_numerator;
	int	scale_denominator;
	int	xres;
	int	yres;
};

struct vstat_vmem {
	unsigned refcount;
	struct vmem_cell *vmem;
};

struct video_stats {
	int rows;
	int cols;
	int curs_row;
	int curs_col;
	int curs_start;
	int curs_end;
	int curs_blinks;
	int curs_visible;
	int default_curs_start;
	int default_curs_end;
	int mode;
	int charheight;
	int charwidth;
	int bright_background;
	int blink;
	int curs_blink;
	int no_bright;
	int no_blink;
	int bright_altcharset;
	int blink_altcharset;
	int currattr;
	int scaling;
	int vmultiplier;
	int scrnwidth;
	int scrnheight;
	int winwidth;
	int winheight;
	int scale_numerator;
	int scale_denominator;
	uint32_t flags;
#define VIDMODES_FLAG_PALETTE_VMEM         1
#define VIDMODES_FLAG_EXPAND               2
#define VIDMODES_FLAG_LINE_GRAPHICS_EXPAND 4
	uint32_t palette[16];
	struct vstat_vmem *vmem;
	uint8_t *forced_font;
};

enum {
	 MONO_PALETTE
	,GREYSCALE_PALETTE
	,COLOUR_PALETTE
	,C64_PALETTE
	,ATARI_PALETTE
};

extern struct video_params vparams[55];
#define NUMMODES      (sizeof(vparams) / sizeof(struct video_params))
extern uint32_t palettes[5][16];
extern struct dac_colors dac_default[TOTAL_DAC_SIZE];
extern char vga_font_bitmap[4096];
extern char vga_font_bitmap14[3584];
extern char vga_font_bitmap8[2048];

#ifdef __cplusplus
extern "C" {
#endif
int find_vmode(int mode);
struct vstat_vmem *get_vmem(struct video_stats *vs);
void release_vmem(struct vstat_vmem *vm);
int load_vmode(struct video_stats *vs, int mode);
#ifdef __cplusplus
}
#endif

#endif
