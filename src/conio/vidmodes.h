/*
 * Copyright (c) 1992, 1993, 1996
 *	Berkeley Software Design, Inc.  All rights reserved.
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
 *	This product includes software developed by Berkeley Software
 *	Design, Inc.
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
 *	BSDI vparams.h,v 2.2 1996/04/08 19:33:13 bostic Exp
 *
 * $FreeBSD: src/usr.bin/doscmd/vparams.h,v 1.1.6.1 2002/04/25 11:04:51 tg Exp $
 */

/* Collect some default parameters for the VGA emulator in this file. This is
   supposed to be included only from 'video.c' and needs some type definitions
   from 'video.h'. */

#include "ciolib.h"
#include "console.h"

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

struct video_params vparams[] = {
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

unsigned char palettes[][16] = {
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

struct dac_colors dac_default16[] = {
	{0, 0, 0},    {0, 0, 42},   {0, 42, 0},   {0, 42, 42},
	{42, 0, 0},   {42, 0, 42},  {42, 21, 0},  {42, 42, 42},
	{21, 21, 21}, {21, 21, 63}, {21, 63, 21}, {21, 63, 63},
	{63, 21, 21}, {63, 21, 63}, {63, 63, 21}, {63, 63, 63}
};
