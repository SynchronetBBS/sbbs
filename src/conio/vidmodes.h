/* $Id: */

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

#include "ciolib.h"

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

enum {
	 MONO_PALETTE
	,GREYSCALE_PALETTE
	,COLOUR_PALETTE
};

static struct video_params vparams[] = {
	/* BW 40x25 */
	{BW40, GREYSCALE_PALETTE, 40, 25, 14, 15, 16, 8},
	/* CO 40x25 */
	{C40, COLOUR_PALETTE, 40, 25, 14, 15, 16, 8},
	/* BW 80x25 */
	{BW80, GREYSCALE_PALETTE, 80, 25, 14, 15, 16, 8},
	/* CO 80x25 */
	{C80, COLOUR_PALETTE, 80, 25, 14, 15, 16, 8},
	/* MONO */
	{MONO, 0, 80, 25, 14, 15, 16, 8},
	/* CO 40x14 */
	{C40X14, COLOUR_PALETTE, 40, 14, 14, 15, 16, 8},
	/* CO 40x21 */
	{C40X21, COLOUR_PALETTE, 40, 21, 14, 15, 16, 8},
	/* CO 40x28 */
	{C40X28, COLOUR_PALETTE, 40, 28, 12, 13, 14, 8},
	/* CO 40x43 */
	{C40X43, COLOUR_PALETTE, 40, 43, 7, 7, 8, 8},
	/* CO 40x50 */
	{C40X50, COLOUR_PALETTE, 40, 50, 7, 7, 8, 8},
	/* CO 40x60 */
	{C40X60, COLOUR_PALETTE, 40, 60, 7, 7, 8, 8},
	/* CO 80x14 */
	{C80X14, COLOUR_PALETTE, 80, 14, 14, 15, 16, 8},
	/* CO 80x21 */
	{C80X21, COLOUR_PALETTE, 80, 21, 14, 15, 16, 8},
	/* CO 80x28 */
	{C80X28, COLOUR_PALETTE, 80, 28, 12, 13, 14, 8},
	/* CO 80x43 */
	{C80X43, COLOUR_PALETTE, 80, 43, 7, 7, 8, 8},
	/* CO 80x50 */
	{C80X50, COLOUR_PALETTE, 80, 50, 7, 7, 8, 8},
	/* CO 80x60 */
	{C80X60, COLOUR_PALETTE, 80, 60, 7, 7, 8, 8},
	/* B 40x14 */
	{BW40X14, GREYSCALE_PALETTE, 40, 14, 14, 15, 16, 8},
	/* BW 40x21 */
	{BW40X21, GREYSCALE_PALETTE, 40, 21, 14, 15, 16, 8},
	/* BW 40x28 */
	{BW40X28, GREYSCALE_PALETTE, 40, 28, 12, 13, 14, 8},
	/* BW 40x43 */
	{BW40X43, GREYSCALE_PALETTE, 40, 43, 7, 7, 8, 8},
	/* BW 40x50 */
	{BW40X50, GREYSCALE_PALETTE, 40, 50, 7, 7, 8, 8},
	/* BW 40x60 */
	{BW40X60, GREYSCALE_PALETTE, 40, 60, 7, 7, 8, 8},
	/* BW 80x14 */
	{BW80X14, GREYSCALE_PALETTE, 80, 14, 14, 15, 16, 8},
	/* BW 80x21 */
	{BW80X21, GREYSCALE_PALETTE, 80, 21, 14, 15, 16, 8},
	/* BW 80x28 */
	{BW80X28, GREYSCALE_PALETTE, 80, 28, 12, 13, 14, 8},
	/* BW 80x43 */
	{BW80X43, GREYSCALE_PALETTE, 80, 43, 7, 7, 8, 8},
	/* BW 80x50 */
	{BW80X50, GREYSCALE_PALETTE, 80, 50, 7, 7, 8, 8},
	/* BW 80x60 */
	{BW80X60, GREYSCALE_PALETTE, 80, 60, 7, 7, 8, 8},
	/* MONO 80x14 */
	{MONO14, MONO_PALETTE, 80, 14, 14, 15, 16, 8},
	/* MONO 80x21 */
	{MONO21, MONO_PALETTE, 80, 21, 14, 15, 16, 8},
	/* MONO 80x28 */
	{MONO28, MONO_PALETTE, 80, 28, 12, 13, 14, 8},
	/* MONO 80x43 */
	{MONO43, MONO_PALETTE, 80, 43, 7, 7, 8, 8},
	/* MONO 80x50 */
	{MONO50, MONO_PALETTE, 80, 50, 7, 7, 8, 8},
	/* MONO 80x60 */
	{MONO60, MONO_PALETTE, 80, 60, 7, 7, 8, 8},
	/* Magical C4350 Mode */
	{C4350, COLOUR_PALETTE, 80, 50, 7, 7, 8, 8},
};

#define NUMMODES      (sizeof(vparams) / sizeof(struct video_params))

static unsigned char palettes[][16] = {
	/* Mono */
	{ 0x00, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	  0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07
	},
	/* Black and White */
	{ 0x00, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	  0x08, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f
	},
	/* Colour */
	{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 
	  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
	}
};

/* Entry type for the DAC table. Each value is actually 6 bits wide. */
struct dac_colors {
	BYTE red;
	BYTE green;
	BYTE blue;
};

static struct dac_colors dac_default16[] = {
	{0, 0, 0},    {0, 0, 42},   {0, 42, 0},   {0, 42, 42},
	{42, 0, 0},   {42, 0, 42},  {42, 21, 0},  {42, 42, 42},
	{21, 21, 21}, {21, 21, 63}, {21, 63, 21}, {21, 63, 63},
	{63, 21, 21}, {63, 21, 63}, {63, 63, 21}, {63, 63, 63}
};

static int find_vmode(int mode)
{
    unsigned i;

	for (i = 0; i < NUMMODES; i++)
		if (vparams[i].mode == mode)
			return i;

	return -1;
}

#endif
