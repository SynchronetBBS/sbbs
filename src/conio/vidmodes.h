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
	int colour;
	int	cols;
	int rows;
	int	curs_start;
	int curs_end;
	int charheight;
	int charwidth;
};

struct video_params vparams[] = {
	/* BW 40x25 */
	{BW40, 0, 40, 25, 14, 15, 16, 8},
	/* CO 40x25 */
	{C40, 1, 40, 25, 14, 15, 16, 8},
	/* BW 80x25 */
	{BW80, 0, 80, 25, 14, 15, 16, 8},
	/* CO 80x25 */
	{C80, 1, 80, 25, 14, 15, 16, 8},
	/* CO 80x14 */
	{C80X14, 1, 80, 14, 14, 15, 16, 8},
	/* CO 80x21 */
	{C80X21, 1, 80, 21, 14, 15, 16, 8},
	/* CO 80x28 */
	{C80X28, 1, 80, 28, 12, 13, 14, 8},
	/* CO 80x43 */
	{C80X43, 1, 80, 43, 7, 7, 8, 8},
	/* CO 80x50 */
	{C80X50, 1, 80, 50, 7, 7, 8, 8},
	/* CO 80x60 */
	{C80X60, 1, 80, 60, 7, 7, 8, 8},
};

unsigned char palettes[][16] = {
	/* Mono */
	{ 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
	  0x10, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18
	},
	/* Colour */
	{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07, 
	  0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f
	}
};

/* The default DAC table for the EGA and VGA 16 color modes. This table is
   installed on startup. The values are taken from the output of 'scon -p
   list'. */
struct dac_colors dac_default64[] = {
	{0, 0, 0},    {0, 0, 42},   {0, 42, 0},   {0, 42, 42},
	{42, 0, 0},   {42, 0, 42},  {42, 42, 0},  {42, 42, 42},
	{0, 0, 21},   {0, 0, 63},   {0, 42, 21},  {0, 42, 63},
	{42, 0, 21},  {42, 0, 63},  {42, 42, 21}, {42, 42, 63},
	{0, 21, 0},   {0, 21, 42},  {0, 63, 0},   {0, 63, 42},
	{42, 21, 0},  {42, 21, 42}, {42, 63, 0},  {42, 63, 42},
	{0, 21, 21},  {0, 21, 63},  {0, 63, 21},  {0, 63, 63},
	{42, 21, 21}, {42, 21, 63}, {42, 63, 21}, {42, 63, 63},
	{21, 0, 0},   {21, 0, 42},  {21, 42, 0},  {21, 42, 42},
	{63, 0, 0},   {63, 0, 42},  {63, 42, 0},  {63, 42, 42},
	{21, 0, 21},  {21, 0, 63},  {21, 42, 21}, {21, 42, 63},
	{63, 0, 21},  {63, 0, 63},  {63, 42, 21}, {63, 42, 63},
	{21, 21, 0},  {21, 21, 42}, {21, 63, 0},  {21, 63, 42},
	{63, 21, 0},  {63, 21, 42}, {63, 63, 0},  {63, 63, 42},
	{21, 21, 21}, {21, 21, 63}, {21, 63, 21}, {21, 63, 63},
	{63, 21, 21}, {63, 21, 63}, {63, 63, 21}, {63, 63, 63}
};
