/* $Id: vidmodes.c,v 1.33 2020/06/27 00:04:45 deuce Exp $ */

#include <stdlib.h>
#include <stdbool.h>

/* xpdev: */
#include <gen_defs.h>	/* FREE_AND_NULL */

#include "scale.h"
#include "vidmodes.h"

// TODO: Pretty much all the 1:1 aspect ratios are wrong...
struct video_params vparams[] = {
	/* BW 40x25 */
	{BW40, GREYSCALE_PALETTE,             40, 25, 14, 15, 16, 8, 7, 0,  4, 3, 320,  200},
	/* CO 40x25 */
	{C40, COLOUR_PALETTE,                 40, 25, 14, 15, 16, 8, 7, 0,  4, 3, 320,  200},
	/* BW 80x25 */
	{BW80, GREYSCALE_PALETTE,             80, 25, 14, 15, 16, 8, 7, 0,  4, 3, 640,  400},
	/* CO 80x25 */
	{C80, COLOUR_PALETTE,                 80, 25, 14, 15, 16, 8, 7, 0,  4, 3, 640,  400},
	/* MONO */
	{MONO, 0,                             80, 25, 14, 15, 16, 8, 7, 0,  4, 3, 640,  400},
	/* CO 40x14 */
	{C40X14, COLOUR_PALETTE,              40, 14, 14, 15, 16, 8, 7, 0,  4, 3, 320,  224},
	/* CO 40x21 */
	{C40X21, COLOUR_PALETTE,              40, 21, 14, 15, 16, 8, 7, 0,  4, 3, 320,  336},
	/* CO 40x28 */
	{C40X28, COLOUR_PALETTE,              40, 28, 12, 13, 14, 8, 7, 0,  4, 3, 320,  392},
	/* CO 40x43 */
	{C40X43, COLOUR_PALETTE,              40, 43,  7,  7,  8, 8, 7, 0,  4, 3, 320,  774},
	/* CO 40x50 */
	{C40X50, COLOUR_PALETTE,              40, 50,  7,  7,  8, 8, 7, 0,  4, 3, 320,  400},
	/* CO 40x60 */
	{C40X60, COLOUR_PALETTE,              40, 60,  7,  7,  8, 8, 7, 0,  4, 3, 320,  480},
	/* CO 80x14 */
	{C80X14, COLOUR_PALETTE,              80, 14, 14, 15, 16, 8, 7, 0,  4, 3, 640,  224},
	/* CO 80x21 */
	{C80X21, COLOUR_PALETTE,              80, 21, 14, 15, 16, 8, 7, 0,  4, 3, 640,  336},
	/* CO 80x28 */
	{C80X28, COLOUR_PALETTE,              80, 28, 12, 13, 14, 8, 7, 0,  4, 3, 640,  392},
	/* CO 80x30 */
	{C80X30, COLOUR_PALETTE,              80, 30, 14, 15, 16, 8, 7, 0,  4, 3, 640,  480},
	/* CO 80x43 */
	{C80X43, COLOUR_PALETTE,              80, 43,  7,  7,  8, 8, 7, 0,  4, 3, 640,  350},
	/* EGA 80x25 */
	{EGA80X25, COLOUR_PALETTE,            80, 25, 12, 13, 14, 8, 7, 0,  4, 3, 640,  350},
	/* CO 80x50 */
	{C80X50, COLOUR_PALETTE,              80, 50,  7,  7,  8, 8, 7, 0,  4, 3, 640,  400},
	/* CO 80x60 */
	{C80X60, COLOUR_PALETTE,              80, 60,  7,  7,  8, 8, 7, 0,  4, 3, 640,  480},
	/* B 40x14 */
	{BW40X14, GREYSCALE_PALETTE,          40, 14, 14, 15, 16, 8, 7, 0,  4, 3, 320,  224},
	/* BW 40x21 */
	{BW40X21, GREYSCALE_PALETTE,          40, 21, 14, 15, 16, 8, 7, 0,  4, 3, 320,  336},
	/* BW 40x28 */
	{BW40X28, GREYSCALE_PALETTE,          40, 28, 12, 13, 14, 8, 7, 0,  4, 3, 320,  392},
	/* BW 40x43 */
	{BW40X43, GREYSCALE_PALETTE,          40, 43,  7,  7, 14, 8, 7, 0,  4, 3, 320,  350},
	/* BW 40x50 */
	{BW40X50, GREYSCALE_PALETTE,          40, 50,  7,  7,  8, 8, 7, 0,  4, 3, 320,  400},
	/* BW 40x60 */
	{BW40X60, GREYSCALE_PALETTE,          40, 60,  7,  7,  8, 8, 7, 0,  4, 3, 320,  480},
	/* BW 80x14 */
	{BW80X14, GREYSCALE_PALETTE,          80, 14, 14, 15, 16, 8, 7, 0,  4, 3, 640,  224},
	/* BW 80x21 */
	{BW80X21, GREYSCALE_PALETTE,          80, 21, 14, 15, 16, 8, 7, 0,  4, 3, 640,  336},
	/* BW 80x28 */
	{BW80X28, GREYSCALE_PALETTE,          80, 28, 12, 13, 14, 8, 7, 0,  4, 3, 640,  392},
	/* BW 80x43 */
	{BW80X43, GREYSCALE_PALETTE,          80, 43,  7,  7, 14, 8, 7, 0,  4, 3, 640,  350},
	/* BW 80x50 */
	{BW80X50, GREYSCALE_PALETTE,          80, 50,  7,  7,  8, 8, 7, 0,  4, 3, 640,  400},
	/* BW 80x60 */
	{BW80X60, GREYSCALE_PALETTE,          80, 60,  7,  7,  8, 8, 7, 0,  4, 3, 640,  480},
	/* MONO 80x14 */
	{MONO14, MONO_PALETTE,                80, 14, 14, 15, 16, 8, 7, 0,  4, 3, 640,  224},
	/* MONO 80x21 */
	{MONO21, MONO_PALETTE,                80, 21, 14, 15, 16, 8, 7, 0,  4, 3, 640,  336},
	/* MONO 80x28 */
	{MONO28, MONO_PALETTE,                80, 28, 12, 13, 14, 8, 7, 0,  4, 3, 640,  392},
	/* MONO 80x43 */
	{MONO43, MONO_PALETTE,                80, 43,  7,  7, 14, 8, 7, 0,  4, 3, 640,  350},
	/* MONO 80x50 */
	{MONO50, MONO_PALETTE,                80, 50,  7,  7,  8, 8, 7, 0,  4, 3, 640,  400},
	/* MONO 80x60 */
	{MONO60, MONO_PALETTE,                80, 60,  7,  7,  8, 8, 7, 0,  4, 3, 640,  480},
	/* Magical C4350 Mode */
	{C4350, COLOUR_PALETTE,               80, 50,  7,  7,  8, 8, 7, 0,  4, 3, 640,  400},
	/* Commodore 64 40x25 mode */
	// This is the NTSC aspect ratio... the pixel aspect ratio is about 75:100 due to the
	// borders.
	{C64_40X25, C64_PALETTE,              40, 25,  0,  7,  8, 8, 0x6e, CIOLIB_VIDEO_BGBRIGHT|CIOLIB_VIDEO_NOBLINK,  6, 5, 320, 200},
	/* Commodore 128 40x25 mode */
	// The C128 uses the same aspect ratio as the C64 in 40 column mode
	{C128_40X25, C64_PALETTE,             40, 25,  0,  7,  8, 8, 0xbd, CIOLIB_VIDEO_BGBRIGHT|CIOLIB_VIDEO_NOBLINK,  6, 5, 320, 200},
	/* Commodore 128 80x25 mode */
	// But in 80-column mode is a 4:3 CGA style output.
	{C128_80X25, COLOUR_PALETTE,          80, 25,  0,  7,  8, 8, 7,    CIOLIB_VIDEO_BGBRIGHT|CIOLIB_VIDEO_NOBLINK,  4, 3, 640, 200},
	/* Atari 800 40x24 mode */
	{ATARI_40X24, ATARI_PALETTE,          40, 24,  0,  7,  8, 8, 7, 0,  4, 3, 320,  192},
	/* Atari 800 XEP80 80x25 mode */
	{ATARI_80X25, GREYSCALE_PALETTE,      80, 25,  0, 15, 16, 8, 7, 0,  4, 3, 640,  400},
	/* VESA 21x132 mode */
	{VESA_132X21, COLOUR_PALETTE,        132, 21, 14, 15, 16, 8, 7, 0,  4, 3, 1056, 336},
	/* VESA 25x132 mode */
	{VESA_132X25, COLOUR_PALETTE,        132, 25, 14, 15, 16, 8, 7, 0,  4, 3, 1056, 400},
	/* VESA 28x132 mode */
	{VESA_132X28, COLOUR_PALETTE,        132, 28, 12, 13, 14, 8, 7, 0,  4, 3, 1056, 392},
	/* VESA 30x132 mode */
	{VESA_132X30, COLOUR_PALETTE,        132, 30, 14, 15, 16, 8, 7, 0,  4, 3, 1056, 480},
	/* VESA 34x132 mode */
	{VESA_132X34, COLOUR_PALETTE,        132, 34, 12, 13, 14, 8, 7, 0,  4, 3, 1056, 476},
	/* VESA 43x132 mode */
	{VESA_132X43, COLOUR_PALETTE,        132, 43,  7,  7,  8, 8, 7, 0,  4, 3, 1056, 350},
	/* VESA 50x132 mode */
	{VESA_132X50, COLOUR_PALETTE,        132, 50,  7,  7,  8, 8, 7, 0,  4, 3, 1056, 400},
	/* VESA 60x132 mode */
	{VESA_132X60, COLOUR_PALETTE,        132, 60,  7,  7,  8, 8, 7, 0,  4, 3, 1056, 480},
	/* Awesome modes */
	{ST132X37_16_9, COLOUR_PALETTE,      132, 37, 14, 15, 16, 8, 7, 0, 16, 9, 1056, 600},
	{ST132X52_5_4, COLOUR_PALETTE,       132, 52, 14, 15, 16, 8, 7, 0,  5, 4, 1056, 832},
	/* Stupid modes */
	{VGA80X25, COLOUR_PALETTE,            80, 25, 14, 15, 16, 9, 7, CIOLIB_VIDEO_EXPAND | CIOLIB_VIDEO_LINE_GRAPHICS_EXPAND, 4, 3, 720, 400},
	/* DigitalMon */
	{LCD80X25, COLOUR_PALETTE,            80, 25, 14, 15, 16, 8, 7, 0,  8, 5, 640,  400},
	/* Prestel */
	// Video data is transmitted for 40us of the 52us available to PAL.
	// There's 500 lines of the 625 available to PAL
	// This gives a 3077:2400 aspect ratio
	{PRESTEL_40X25, PRESTEL_PALETTE,      40, 25, 14, 15, 20, 12, 7, 0,  3077, 2400, 480, 500},
	/* Atari ST 40X25 */
	{ATARIST_40X25, ATARI_PALETTE_16,     40, 25, 0,  7,  8, 8, 15, CIOLIB_VIDEO_BGBRIGHT | CIOLIB_VIDEO_NOBLINK, 4, 3, 320, 200},
	/* Atari ST 80X25 */
	{ATARIST_80X25, ATARI_PALETTE_4,      80, 25, 0,  7,  8, 8, 15, CIOLIB_VIDEO_BGBRIGHT | CIOLIB_VIDEO_NOBLINK, 4, 3, 640, 200},
	/* Atari ST 80X25 Mono */
	{ATARIST_80X25_MONO, ATARI_PALETTE_2, 80, 25, 0, 15, 16, 8, 15, CIOLIB_VIDEO_BGBRIGHT | CIOLIB_VIDEO_NOBLINK, 4, 3, 640, 400},

	/* Custom mode */
	{CIOLIB_MODE_CUSTOM, COLOUR_PALETTE,  80, 25, 14, 15, 16, 8, 7, 0,  0, 0,   -1,  -1},
};

uint32_t palettes[9][16] = {
	/* Mono */
	{ 0x00, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	  0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07
	},
	/* Black and White */
	{ 0x00, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
	  0x08, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f
	},
	/* Colour */
	{ 0x00, 0x04, 0x02, 0x06, 0x01, 0x05, 0x03, 0x07, 
	  0x08, 0x0c, 0x0a, 0x0e, 0x09, 0x0d, 0x0b, 0x0f
	},
	/* C64 */
	{ 0x100, 0x101, 0x102, 0x103, 0x104, 0x105, 0x106, 0x107, 
	  0x108, 0x109, 0x10a, 0x10b, 0x10c, 0x10d, 0x10e, 0x10f
	},
	/* Atari */
	{ 0x110, 0x111, 0x111, 0x111, 0x111, 0x111, 0x111, 0x111,
	  0x111, 0x111, 0x111, 0x111, 0x111, 0x111, 0x111, 0x111
	},
	/* Prestel */
	{ 0, 21, 46, 51, 196, 201, 226, 15,
	  0, 21, 46, 51, 196, 201, 226, 15
	},
	/* Atari ST 16-colour */
	{  15, 196,  46, 226,  21, 201,  51, 274,
	  275, 276, 277, 278, 279, 280, 281,   0},
	/* Atari ST 4-colour */
	{ 15, 196, 46, 0, 15, 196, 46, 0,
	  15, 196, 46, 0, 15, 196, 46, 0},
	/* Atari ST 2-colour */
	{ 15, 0, 15, 0, 15, 0, 15, 0,
	  15, 0, 15, 0, 15, 0, 15, 0},
};

struct dac_colors dac_default[TOTAL_DAC_SIZE] = {
	/* "System" colours */
	{0, 0, 0},       // Black
	{168, 0, 0},     // Red
	{0, 168, 0},     // Green
	{168, 84, 0},    // Brown
	{0, 0, 168},     // Blue
	{168, 0, 168},   // Magenta
	{0, 168, 168},   // Cyan
	{168, 168, 168}, // Light Gray

	{84, 84, 84},    // Dark Gray
	{255, 84, 84},   // Light Red
	{84, 255, 84},   // Light Green
	{255, 255, 84},  // Yellow
	{84, 84, 255},   // Light Blue
	{255, 84, 255},  // Light Magenta
	{84, 255, 255},  // Light Cyan
	{255, 255, 255}, // White

	/* XTerm 256 colour palette */
	{0, 0, 0}, {0, 0, 95}, {0, 0, 135}, {0, 0, 175},		// 16
	{0, 0, 215}, {0, 0, 255}, {0, 95, 0}, {0, 95, 95},
	{0, 95, 135}, {0, 95, 175}, {0, 95, 215}, {0, 95, 255},
	{0, 135, 0}, {0, 135, 95}, {0, 135, 135}, {0, 135, 175},
	{0, 135, 215}, {0, 135, 255}, {0, 175, 0}, {0, 175, 95},	// 32
	{0, 175, 135}, {0, 175, 175}, {0, 175, 215}, {0, 175, 255},
	{0, 215, 0}, {0, 215, 95}, {0, 215, 135}, {0, 215, 175},
	{0, 215, 215}, {0, 215, 255}, {0, 255, 0}, {0, 255, 95},
	{0, 255, 135}, {0, 255, 175}, {0, 255, 215}, {0, 255, 255},	// 48
	{95, 0, 0}, {95, 0, 95}, {95, 0, 135}, {95, 0, 175},
	{95, 0, 215}, {95, 0, 255}, {95, 95, 0}, {95, 95, 95},
	{95, 95, 135}, {95, 95, 175}, {95, 95, 215}, {95, 95, 255},
	{95, 135, 0}, {95, 135, 95}, {95, 135, 135}, {95, 135, 175},	// 64
	{95, 135, 215}, {95, 135, 255}, {95, 175, 0}, {95, 175, 95},
	{95, 175, 135}, {95, 175, 175}, {95, 175, 215}, {95, 175, 255},
	{95, 215, 0}, {95, 215, 95}, {95, 215, 135}, {95, 215, 175},
	{95, 215, 215}, {95, 215, 255}, {95, 255, 0}, {95, 255, 95},	// 80
	{95, 255, 135}, {95, 255, 175}, {95, 255, 215}, {95, 255, 255},
	{135, 0, 0}, {135, 0, 95}, {135, 0, 135}, {135, 0, 175},
	{135, 0, 215}, {135, 0, 255}, {135, 95, 0}, {135, 95, 95},
	{135, 95, 135}, {135, 95, 175}, {135, 95, 215}, {135, 95, 255},	// 96
	{135, 135, 0}, {135, 135, 95}, {135, 135, 135}, {135, 135, 175},
	{135, 135, 215}, {135, 135, 255}, {135, 175, 0}, {135, 175, 95},
	{135, 175, 135}, {135, 175, 175}, {135, 175, 215}, {135, 175, 255},
	{135, 215, 0}, {135, 215, 95}, {135, 215, 135}, {135, 215, 175},	// 112
	{135, 215, 215}, {135, 215, 255}, {135, 255, 0}, {135, 255, 95},
	{135, 255, 135}, {135, 255, 175}, {135, 255, 215}, {135, 255, 255},
	{175, 0, 0}, {175, 0, 95}, {175, 0, 135}, {175, 0, 175},
	{175, 0, 215}, {175, 0, 255}, {175, 95, 0}, {175, 95, 95},	// 128
	{175, 95, 135}, {175, 95, 175}, {175, 95, 215}, {175, 95, 255},
	{175, 135, 0}, {175, 135, 95}, {175, 135, 135}, {175, 135, 175},
	{175, 135, 215}, {175, 135, 255}, {175, 175, 0}, {175, 175, 95},
	{175, 175, 135}, {175, 175, 175}, {175, 175, 215}, {175, 175, 255},	// 144
	{175, 215, 0}, {175, 215, 95}, {175, 215, 135}, {175, 215, 175},
	{175, 215, 215}, {175, 215, 255}, {175, 255, 0}, {175, 255, 95},
	{175, 255, 135}, {175, 255, 175}, {175, 255, 215}, {175, 255, 255},
	{215, 0, 0}, {215, 0, 95}, {215, 0, 135}, {215, 0, 175},	// 160
	{215, 0, 215}, {215, 0, 255}, {215, 95, 0}, {215, 95, 95},
	{215, 95, 135}, {215, 95, 175}, {215, 95, 215}, {215, 95, 255},
	{215, 135, 0}, {215, 135, 95}, {215, 135, 135}, {215, 135, 175},
	{215, 135, 215}, {215, 135, 255}, {215, 175, 0}, {215, 175, 95},	// 176
	{215, 175, 135}, {215, 175, 175}, {215, 175, 215}, {215, 175, 255},
	{215, 215, 0}, {215, 215, 95}, {215, 215, 135}, {215, 215, 175},
	{215, 215, 215}, {215, 215, 255}, {215, 255, 0}, {215, 255, 95},
	{215, 255, 135}, {215, 255, 175}, {215, 255, 215}, {215, 255, 255},	// 192
	{255, 0, 0}, {255, 0, 95}, {255, 0, 135}, {255, 0, 175},
	{255, 0, 215}, {255, 0, 255}, {255, 95, 0}, {255, 95, 95},
	{255, 95, 135}, {255, 95, 175}, {255, 95, 215}, {255, 95, 255},
	{255, 135, 0}, {255, 135, 95}, {255, 135, 135}, {255, 135, 175},	// 208
	{255, 135, 215}, {255, 135, 255}, {255, 175, 0}, {255, 175, 95},
	{255, 175, 135}, {255, 175, 175}, {255, 175, 215}, {255, 175, 255},
	{255, 215, 0}, {255, 215, 95}, {255, 215, 135}, {255, 215, 175},
	{255, 215, 215}, {255, 215, 255}, {255, 255, 0}, {255, 255, 95},	// 224
	{255, 255, 135}, {255, 255, 175}, {255, 255, 215}, {255, 255, 255},
	{8, 8, 8}, {18, 18, 18}, {28, 28, 28}, {38, 38, 38},
	{48, 48, 48}, {58, 58, 58}, {68, 68, 68}, {78, 78, 78},
	{88, 88, 88}, {98, 98, 98}, {108, 108, 108}, {118, 118, 118},	// 240
	{128, 128, 128}, {138, 138, 138}, {148, 148, 148}, {158, 158, 158},
	{168, 168, 168}, {178, 178, 178}, {188, 188, 188}, {198, 198, 198},
	{208, 208, 208}, {218, 218, 218}, {228, 228, 228}, {238, 238, 238}, 

	/* C64 colours */
	/* Black, White, Red, Cyan, Purple, Green, Blue, Yellow */
	/* Orange, Brown, Lt Red, Dk Grey, Grey, Lt Green, Lt Blue, Lt Grey */
#if 0
	/* Taken from CTerm source */
	{0x00, 0x00, 0x00},	// Black
	{0xFD, 0xFE, 0xFC},	// White
	{0xBE, 0x1A, 0x24},	// Red
	{0x30, 0xE6, 0xC6},	// Cyan
	{0xB4, 0x1A, 0xE2},	// Purple
	{0x1F, 0xD2, 0x1E},	// Green
	{0x21, 0x1B, 0xAE},	// Blue
	{0xDF, 0xF6, 0x0A},	// Yellow
	{0xB8, 0x41, 0x04},	// Orange
	{0x6A, 0x33, 0x04},	// Brown
	{0xFE, 0x4A, 0x57},	// Light Red
	{0x42, 0x45, 0x40},	// Dark Grey
	{0x70, 0x74, 0x6F},	// Grey
	{0x59, 0xFE, 0x59},	// Light Green
	{0x5F, 0x53, 0xFE},	// Light Blue
	{0xA4, 0xA7, 0xA2},	// Light Grey
#else
	/*
	 * Taken from https://www.pepto.de/projects/colorvic/
	 * a much more convincing source.
	 */
	{0x00, 0x00, 0x00},	// 256
	{0xFF, 0xFF, 0xFF},
	{0x81, 0x33, 0x38},
	{0x75, 0xCE, 0xC8},
	{0x8E, 0x3C, 0x97},
	{0x56, 0xAC, 0x4D},
	{0x2E, 0x2C, 0x9B},
	{0xED, 0xF1, 0x71},
	{0x8E, 0x50, 0x29},
	{0x55, 0x38, 0x00},
	{0xC4, 0x6C, 0x71},
	{0x4A, 0x4A, 0x4A},
	{0x7B, 0x7B, 0x7B},
	{0xA9, 0xFF, 0x9F},
	{0x70, 0x6D, 0xEB},
	{0xB2, 0xB2, 0xB2},
#endif

	/* Atari Colours */
	/* BG, FG */
	{0, 81, 129}, {96, 183, 231},

	/* Atari ST Colours (translated from RGB333) */
	{182, 182, 182}, {109, 109, 109}, {255, 109, 109}, {109, 255, 109},
	{255, 255, 109}, {109, 109, 255}, {255, 109, 255}, {109, 255, 255},
};

int find_vmode(int mode)
{
    unsigned i;

	if(mode==_ORIGMODE)
		mode=C80;
	for (i = 0; i < NUMMODES; i++)
		if (vparams[i].mode == mode)
			return i;

	return -1;
}

struct vstat_vmem *get_vmem(struct video_stats *vs)
{
	vs->vmem->refcount++;
	return vs->vmem;
}

void release_vmem(struct vstat_vmem *vm)
{
	if (vm == NULL)
		return;
	if (vm->refcount)
		vm->refcount--;
	if (vm->refcount == 0) {
		FREE_AND_NULL(vm->vmem);
		FREE_AND_NULL(vm);
	}
}

static struct vstat_vmem *new_vmem(int cols, int rows, bool palette)
{
	struct vstat_vmem *ret = malloc(sizeof(struct vstat_vmem));

	if (ret == NULL)
		return ret;
	ret->refcount = 1;
	ret->top_row = 0;
	ret->width = cols;
	ret->height = rows;
	ret->count = cols * rows;
	ret->vmem = malloc(ret->count * sizeof(ret->vmem[0]));
	if (ret->vmem == NULL) {
		free(ret);
		return NULL;
	}

	return ret;
}

struct vmem_cell *
vmem_cell_ptr(struct vstat_vmem *vm, int x, int y)
{
	size_t off = y * vm->width + x;
	off += vm->top_row * vm->width;
	if (off >= vm->count)
		off -= vm->count;
	return &vm->vmem[off];
}

int
vmem_cell_offset(struct vstat_vmem *vm, int x, int y)
{
	size_t off = y * vm->width + x;
	off += vm->top_row * vm->width;
	if (off >= vm->count)
		off -= vm->count;
	return off;
}

struct vmem_cell *
vmem_next_ptr(struct vstat_vmem *vm, struct vmem_cell *c)
{
	if (c == &vm->vmem[vm->count - 1])
		return &vm->vmem[0];
	return ++c;
}

struct vmem_cell *
vmem_next_row_ptr(struct vstat_vmem *vm, struct vmem_cell *c)
{
	c += vm->width;
	if (c >= &vm->vmem[vm->count])
		c -= vm->count;
	return c;
}

struct vmem_cell *
vmem_prev_row_ptr(struct vstat_vmem *vm, struct vmem_cell *c)
{
	c -= vm->width;
	if (c < &vm->vmem[0])
		c += vm->count;
	return c;
}

int
vmem_next_offset(struct vstat_vmem *vm, int off)
{
	if (off == vm->count - 1)
		return 0;
	return ++off;
}

int
vmem_next_row_offset(struct vstat_vmem *vm, int off)
{
	off += vm->width;
	if ((size_t)off >= vm->count)
		off -= vm->count;
	return off;
}

int
vmem_prev_row_offset(struct vstat_vmem *vm, int off)
{
	off -= vm->width;
	if (off < 0)
		off += vm->count;
	return off;
}

int load_vmode(struct video_stats *vs, int mode)
{
	int i;

	if(mode==64)
		mode=C80X50;
	if(mode==_ORIGMODE)
		mode=C80;
	i=find_vmode(mode);
	if(i==-1)
		return(-1);
	release_vmem(vs->vmem);
	vs->vmem=new_vmem(vparams[i].cols, vparams[i].rows, vs->flags & VIDMODES_FLAG_PALETTE_VMEM);
	if (vs->vmem == NULL)
		return -1;
	vs->rows=vparams[i].rows;
	vs->cols=vparams[i].cols;
	vs->curs_start=vparams[i].curs_start;
	vs->curs_end=vparams[i].curs_end;
	vs->default_curs_start=vparams[i].curs_start;
	vs->default_curs_end=vparams[i].curs_end;
	vs->curs_blinks=1;
	vs->curs_visible=1;
	vs->curs_row=1;
	vs->curs_col=1;
	vs->bright_background=vparams[i].flags & CIOLIB_VIDEO_BGBRIGHT;
	vs->no_bright=vparams[i].flags & CIOLIB_VIDEO_NOBRIGHT;
	vs->bright_altcharset=vparams[i].flags & CIOLIB_VIDEO_ALTCHARS;
	vs->no_blink=vparams[i].flags & CIOLIB_VIDEO_NOBLINK;
	vs->blink_altcharset=vparams[i].flags & CIOLIB_VIDEO_BLINKALTCHARS;
	if (vparams[i].flags & CIOLIB_VIDEO_EXPAND)
		vs->flags |= VIDMODES_FLAG_EXPAND;
	else
		vs->flags &= ~VIDMODES_FLAG_EXPAND;
	if (vparams[i].flags & CIOLIB_VIDEO_LINE_GRAPHICS_EXPAND)
		vs->flags |= VIDMODES_FLAG_LINE_GRAPHICS_EXPAND;
	else
		vs->flags &= ~VIDMODES_FLAG_LINE_GRAPHICS_EXPAND;
	if(vs->curs_row < 0)
		vs->curs_row=0;
	if(vs->curs_row >= vparams[i].rows)
		vs->curs_row=vparams[i].rows-1;
	if(vs->curs_col < 0)
		vs->curs_col=0;
	if(vs->curs_col >= vparams[i].cols)
		vs->curs_col=vparams[i].cols-1;
	memcpy(vs->palette, palettes[vparams[i].palette], sizeof(vs->palette));
	vs->charheight=vparams[i].charheight;
	vs->charwidth=vparams[i].charwidth;
	vs->mode=mode;
	vs->currattr = vparams[i].default_attr;
	vs->aspect_width = vparams[i].aspect_width;
	vs->aspect_height = vparams[i].aspect_height;
	if (vparams[i].xres > 0)
		vs->scrnwidth = vparams[i].xres;
	else
		vs->scrnwidth = vs->charwidth * vs->cols;
	if (vparams[i].yres > 0)
		vs->scrnheight = vparams[i].yres;
	else
		vs->scrnheight = vs->charheight * vs->rows;
	if (vs->aspect_width == 0 || vs->aspect_height == 0) {
		vs->aspect_width = vs->scrnwidth;
		vs->aspect_height = vs->scrnheight;
	}
	vs->forced_font = NULL;
	return(0);
}
