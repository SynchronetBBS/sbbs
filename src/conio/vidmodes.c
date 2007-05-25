/* $Id$ */

#include <stdlib.h>

#include "vidmodes.h"

struct video_params vparams[39] = {
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
	/* Commodore 64 40x25 mode */
	{C64_40X25, C64_PALETTE, 40, 25, 0, 7, 8, 8},
	/* Commodore 128 40x25 mode */
	{C128_40X25, COLOUR_PALETTE, 40, 25, 0, 7, 8, 8},
	/* Commodore 128 80x25 mode */
	{C128_80X25, COLOUR_PALETTE, 80, 25, 0, 7, 8, 8},
};

unsigned char palettes[4][16] = {
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
	},
	/* C64 */
	{ 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 
	  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
	}
};

struct dac_colors dac_default[32] = {
	{0, 0, 0},    {0, 0, 168},   {0, 168, 0},   {0, 168, 168},
	{168, 0, 0},   {168, 0, 168},  {168, 84, 0},  {168, 168, 168},
	{84, 84, 84}, {84, 84, 255}, {84, 255, 84}, {84, 255, 255},
	{255, 84, 84}, {255, 84, 255}, {255, 255, 84}, {255, 255, 255},
	/* C64 colours */
	/* Black, White, Red, Cyan, Purple, Green, Blue, Yellow */
	/* Orange, Brown, Lt Red, Dk Grey, Grey, Lt Green, Lt Blue, Lt Grey */
	{0x00, 0x00, 0x00}, {0xff, 0xff, 0xff}, {0x68, 0x37, 0x2b}, 
	{0x70, 0xa4, 0xb2}, {0x6f, 0x3d, 0x86}, {0x58, 0x8d, 0x43},
	{0x35, 0x29, 0x79}, {0xb8, 0xc7, 0x6f}, {0x6f, 0x4f, 0x25},
	{0x43, 0x39, 0x00}, {0x9a, 0x67, 0x59}, {0x44, 0x44, 0x44},
	{0x6c, 0x6c, 0x6c}, {0x9a, 0xd2, 0x84}, {0x6c, 0x5e, 0xb5},
	{0x95, 0x95, 0x95}
};

int find_vmode(int mode)
{
    unsigned i;

	for (i = 0; i < NUMMODES; i++)
		if (vparams[i].mode == mode)
			return i;

	return -1;
}

int load_vmode(struct video_stats *vs, int mode)
{
	int i;
	unsigned short *newvmem;

	i=find_vmode(mode);
	if(i==-1)
		return(-1);
	newvmem=(unsigned short *)realloc(vs->vmem, vparams[i].cols*vparams[i].rows*sizeof(unsigned short));
	if(newvmem==NULL)
		return(-1);
	vs->vmem=newvmem;
	vs->rows=vparams[i].rows;
	vs->cols=vparams[i].cols;
	vs->curs_start=vparams[i].curs_start;
	vs->curs_end=vparams[i].curs_end;
	vs->default_curs_start=vparams[i].curs_start;
	vs->default_curs_end=vparams[i].curs_end;
	if(vs->curs_row < 0)
		vs->curs_row=0;
	if(vs->curs_row >= vparams[i].rows)
		vs->curs_row=vparams[i].rows-1;
	if(vs->curs_col < 0)
		vs->curs_col=0;
	if(vs->curs_col >= vparams[i].cols)
		vs->curs_col=vparams[i].cols-1;
	vs->palette=palettes[vparams[i].palette];
	vs->charheight=vparams[i].charheight;
	vs->charwidth=vparams[i].charwidth;
	vs->mode=mode;
	return(0);
}
