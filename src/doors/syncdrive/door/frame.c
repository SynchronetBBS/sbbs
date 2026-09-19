/* frame.c -- fixed 16-color palette and XRGB -> index conversion. */
#include <string.h>
#include "frame.h"

static uint32_t colors[FRAME_COLORS];
static int      colors_ready;

/* Same decode as tdport/src/platform/gfx.c ega_rgb(): a 200-line EGA monitor
 * reads bit0 B, bit1 G, bit2 R, bit4 intensity, and shows R+G without
 * intensity as brown. */
static uint32_t ega200_rgb(unsigned v)
{
	uint32_t r = (v & 4) ? 0xAA : 0;
	uint32_t g = (v & 2) ? 0xAA : 0;
	uint32_t b = (v & 1) ? 0xAA : 0;

	if ((v & 0x17) == 0x06)
		g = 0x55;
	if (v & 0x10) {
		r += 0x55;
		g += 0x55;
		b += 0x55;
	}
	return r << 16 | g << 8 | b;
}

static void colors_init(void)
{
	int i;

	if (colors_ready)
		return;
	for (i = 0; i < FRAME_COLORS; i++)
		colors[i] = ega200_rgb((unsigned)((i & 7) | ((i & 8) << 1)));
	colors_ready = 1;
}

uint32_t frame_color(int i)
{
	colors_init();
	return colors[i & (FRAME_COLORS - 1)];
}

void frame_palette(uint8_t pal768[768])
{
	int i;

	colors_init();
	memset(pal768, 0, 768);
	for (i = 0; i < FRAME_COLORS; i++) {
		pal768[i * 3 + 0] = (uint8_t)(colors[i] >> 16);
		pal768[i * 3 + 1] = (uint8_t)(colors[i] >> 8);
		pal768[i * 3 + 2] = (uint8_t)colors[i];
	}
}

int frame_index(uint32_t xrgb)
{
	int  i, best = 0;
	long best_d = -1;

	colors_init();
	xrgb &= 0xFFFFFF;
	for (i = 0; i < FRAME_COLORS; i++) {
		if (colors[i] == xrgb)
			return i;
	}
	for (i = 0; i < FRAME_COLORS; i++) {
		long dr = (long)((xrgb >> 16) & 0xFF) - (long)((colors[i] >> 16) & 0xFF);
		long dg = (long)((xrgb >> 8) & 0xFF) - (long)((colors[i] >> 8) & 0xFF);
		long db = (long)(xrgb & 0xFF) - (long)(colors[i] & 0xFF);
		long d  = dr * dr + dg * dg + db * db;

		if (best_d < 0 || d < best_d) {
			best_d = d;
			best   = i;
		}
	}
	return best;
}

void frame_convert(const uint32_t *xrgb, uint8_t *idx, size_t n)
{
	uint32_t last_c = 0;
	uint8_t  last_i = 0;
	size_t   k;

	last_i = (uint8_t)frame_index(0);
	for (k = 0; k < n; k++) {
		uint32_t c = xrgb[k] & 0xFFFFFF;

		if (c != last_c) {
			last_c = c;
			last_i = (uint8_t)frame_index(c);
		}
		idx[k] = last_i;
	}
}
