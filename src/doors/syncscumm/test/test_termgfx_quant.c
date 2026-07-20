#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "termgfx_quant.h"

int main(void)
{
	enum { W = 64, H = 64 };
	static uint8_t rgb[W * H * 3], idx[W * H], pal[768];
	int x, y, n;
	/* 4-band synthetic image: exact colors must survive quantization */
	for (y = 0; y < H; y++) for (x = 0; x < W; x++) {
		uint8_t *p = rgb + (y * W + x) * 3;
		switch ((y / 16) & 3) {
		case 0: p[0] = 255; p[1] = 0;   p[2] = 0;   break;
		case 1: p[0] = 0;   p[1] = 255; p[2] = 0;   break;
		case 2: p[0] = 0;   p[1] = 0;   p[2] = 255; break;
		case 3: p[0] = 255; p[1] = 255; p[2] = 255; break;
		}
	}
	n = termgfx_quant_rgb(rgb, W, H, idx, pal);
	assert(n >= 4 && n <= 256);
	/* every pixel must round-trip exactly (only 4 distinct colors) */
	for (y = 0; y < H; y++) for (x = 0; x < W; x++) {
		const uint8_t *want = rgb + (y * W + x) * 3;
		const uint8_t *got = pal + idx[y * W + x] * 3;
		assert(memcmp(want, got, 3) == 0);
	}
	/* stress: 10k random colors must not crash and must fill <= 256 */
	srand(42);
	for (x = 0; x < W * H * 3; x++) rgb[x] = (uint8_t)rand();
	n = termgfx_quant_rgb(rgb, W, H, idx, pal);
	assert(n > 0 && n <= 256);
	printf("TERMGFX_QUANT OK (%d colors)\n", n);
	return 0;
}
