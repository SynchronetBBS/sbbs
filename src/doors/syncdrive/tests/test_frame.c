/* Unit tests for frame.c: the fixed 16-color EGA 200-line palette and the
 * XRGB -> index conversion. */
#include <assert.h>
#include <string.h>
#include "frame.h"

int main(void)
{
	uint8_t  pal[768];
	uint32_t px[5];
	uint8_t  idx[5];
	int      i, j;

	/* Every entry maps back to itself, and all 16 are distinct. */
	for (i = 0; i < FRAME_COLORS; i++) {
		assert(frame_index(frame_color(i)) == i);
		for (j = 0; j < i; j++)
			assert(frame_color(i) != frame_color(j));
	}

	/* Spot values of the 200-line decode. */
	assert(frame_color(0) == 0x000000);
	assert(frame_color(1) == 0x0000AA);    /* blue */
	assert(frame_color(6) == 0xAA5500);    /* brown, not dark yellow */
	assert(frame_color(7) == 0xAAAAAA);
	assert(frame_color(8) == 0x555555);
	assert(frame_color(14) == 0xFFFF55);   /* yellow */
	assert(frame_color(15) == 0xFFFFFF);

	/* The pad byte of XRGB is ignored; an off-palette color goes to the
	 * nearest entry. */
	assert(frame_index(0xFF0000AA) == 1);
	assert(frame_index(0x00AB5601) == 6);

	/* Palette triples, unused entries zero. */
	memset(pal, 0x7E, sizeof pal);
	frame_palette(pal);
	assert(pal[6 * 3 + 0] == 0xAA && pal[6 * 3 + 1] == 0x55 && pal[6 * 3 + 2] == 0x00);
	assert(pal[15 * 3 + 0] == 0xFF && pal[15 * 3 + 1] == 0xFF && pal[15 * 3 + 2] == 0xFF);
	for (i = FRAME_COLORS * 3; i < 768; i++)
		assert(pal[i] == 0);

	/* Bulk conversion. */
	px[0] = frame_color(3);
	px[1] = frame_color(3);
	px[2] = frame_color(12);
	px[3] = 0x000000;
	px[4] = frame_color(15);
	frame_convert(px, idx, 5);
	assert(idx[0] == 3 && idx[1] == 3 && idx[2] == 12 && idx[3] == 0 && idx[4] == 15);
	return 0;
}
