/* syncretro_quant.c -- RGB24 -> 256-color indexed. See syncretro_quant.h. */
#include "syncretro_quant.h"

#include <stdlib.h>
#include <string.h>

/* Open-addressed color set / color->index map. Capacity is a power of two well
 * above the 256-color ceiling, so the load factor stays under 1/4 and the linear
 * probe stays short. SR_HASH_EMPTY is not a representable RGB24 value. */
#define SR_HASH_BITS  10
#define SR_HASH_SIZE  (1 << SR_HASH_BITS)
#define SR_HASH_MASK  (SR_HASH_SIZE - 1)
#define SR_HASH_EMPTY 0xffffffffu

#define SR_MAX_COLORS 256

/* Fixed-palette layout (CUBE mode): a 6x6x6 RGB cube, then a 40-step gray ramp. */
#define SR_CUBE_LEVELS 6
#define SR_CUBE_SIZE   (SR_CUBE_LEVELS * SR_CUBE_LEVELS * SR_CUBE_LEVELS)   /* 216 */
#define SR_GRAY_STEPS  (SR_MAX_COLORS - SR_CUBE_SIZE)                       /* 40  */

/* Knuth multiplicative hash, folded to the table's bit width. */
static unsigned sr_hash(uint32_t c)
{
	return (unsigned)((c * 2654435761u) >> (32 - SR_HASH_BITS)) & SR_HASH_MASK;
}

/* Collect the frame's distinct colors, ascending, into `colors`. Returns the
 * count, or -1 the moment a (SR_MAX_COLORS + 1)-th color appears -- the caller
 * then takes the CUBE path and this pass's work is simply discarded. */
static int sr_collect_colors(const uint8_t *rgb, long npx, uint32_t *colors)
{
	uint32_t tbl[SR_HASH_SIZE];
	int      n = 0;
	long     i;

	memset(tbl, 0xff, sizeof(tbl));
	for (i = 0; i < npx; i++) {
		uint32_t c = ((uint32_t)rgb[i * 3] << 16)
		             | ((uint32_t)rgb[i * 3 + 1] << 8)
		             |  (uint32_t)rgb[i * 3 + 2];
		unsigned h = sr_hash(c);

		while (tbl[h] != SR_HASH_EMPTY && tbl[h] != c)
			h = (h + 1) & SR_HASH_MASK;
		if (tbl[h] == c)
			continue;
		if (n == SR_MAX_COLORS)
			return -1;      /* too many colors: the caller falls back to CUBE */
		tbl[h] = c;
		colors[n++] = c;
	}
	return n;
}

static int sr_cmp_u32(const void *a, const void *b)
{
	uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;

	return (x > y) - (x < y);
}

/* EXACT: `colors` (sorted, `n` entries) becomes the palette 1:1, and every pixel
 * maps back through a color->index table built from it. */
static void sr_map_exact(const uint8_t *rgb, long npx, uint8_t *idx, uint8_t *pal,
                         const uint32_t *colors, int n)
{
	uint32_t key[SR_HASH_SIZE];
	uint8_t  val[SR_HASH_SIZE];
	int      i;
	long     p;

	memset(key, 0xff, sizeof(key));
	memset(pal, 0, SR_MAX_COLORS * 3);
	for (i = 0; i < n; i++) {
		unsigned h = sr_hash(colors[i]);

		while (key[h] != SR_HASH_EMPTY)
			h = (h + 1) & SR_HASH_MASK;
		key[h] = colors[i];
		val[h] = (uint8_t)i;

		pal[i * 3]     = (uint8_t)(colors[i] >> 16);
		pal[i * 3 + 1] = (uint8_t)(colors[i] >> 8);
		pal[i * 3 + 2] = (uint8_t)colors[i];
	}

	for (p = 0; p < npx; p++) {
		uint32_t c = ((uint32_t)rgb[p * 3] << 16)
		             | ((uint32_t)rgb[p * 3 + 1] << 8)
		             |  (uint32_t)rgb[p * 3 + 2];
		unsigned h = sr_hash(c);

		while (key[h] != c)          /* always present: pass 1 interned every color */
			h = (h + 1) & SR_HASH_MASK;
		idx[p] = val[h];
	}
}

/* The 6 cube levels are 0, 51, ..., 255; the gray ramp spans 0..255 in 40 steps.
 * Both are exact at the endpoints, so pure black and pure white are lossless in
 * CUBE mode too (they are the colors a letterboxed frame's bars are made of). */
static int sr_cube_level(int v)     { return (v * (SR_CUBE_LEVELS - 1) + 127) / 255; }
static int sr_cube_value(int lvl)   { return lvl * 255 / (SR_CUBE_LEVELS - 1); }
static int sr_gray_step(int v)      { return (v * (SR_GRAY_STEPS - 1) + 127) / 255; }
static int sr_gray_value(int step)  { return step * 255 / (SR_GRAY_STEPS - 1); }

static void sr_build_cube_palette(uint8_t *pal)
{
	int r, g, b, i;

	for (r = 0; r < SR_CUBE_LEVELS; r++)
		for (g = 0; g < SR_CUBE_LEVELS; g++)
			for (b = 0; b < SR_CUBE_LEVELS; b++) {
				i = (r * SR_CUBE_LEVELS + g) * SR_CUBE_LEVELS + b;
				pal[i * 3]     = (uint8_t)sr_cube_value(r);
				pal[i * 3 + 1] = (uint8_t)sr_cube_value(g);
				pal[i * 3 + 2] = (uint8_t)sr_cube_value(b);
			}
	for (i = 0; i < SR_GRAY_STEPS; i++) {
		int v = sr_gray_value(i);

		pal[(SR_CUBE_SIZE + i) * 3]     = (uint8_t)v;
		pal[(SR_CUBE_SIZE + i) * 3 + 1] = (uint8_t)v;
		pal[(SR_CUBE_SIZE + i) * 3 + 2] = (uint8_t)v;
	}
}

static int sr_sqdist(int r1, int g1, int b1, int r2, int g2, int b2)
{
	int dr = r1 - r2, dg = g1 - g2, db = b1 - b2;

	return dr * dr + dg * dg + db * db;
}

/* CUBE: map each pixel to the nearer of its cube cell and its gray-ramp step.
 * The gray ramp is what keeps near-neutral gradients (shadows, fades) from
 * banding onto the cube's coarse 51-value spacing. */
static void sr_map_cube(const uint8_t *rgb, long npx, uint8_t *idx, uint8_t *pal)
{
	long p;

	sr_build_cube_palette(pal);
	for (p = 0; p < npx; p++) {
		int r = rgb[p * 3], g = rgb[p * 3 + 1], b = rgb[p * 3 + 2];
		int cr = sr_cube_level(r), cg = sr_cube_level(g), cb = sr_cube_level(b);
		int gs = sr_gray_step((r + g + b) / 3);
		int gv = sr_gray_value(gs);
		int cube_err = sr_sqdist(r, g, b, sr_cube_value(cr), sr_cube_value(cg), sr_cube_value(cb));
		int gray_err = sr_sqdist(r, g, b, gv, gv, gv);

		if (gray_err < cube_err)
			idx[p] = (uint8_t)(SR_CUBE_SIZE + gs);
		else
			idx[p] = (uint8_t)((cr * SR_CUBE_LEVELS + cg) * SR_CUBE_LEVELS + cb);
	}
}

int sr_quant_rgb_to_indexed(const uint8_t *rgb, int w, int h,
                            uint8_t *idx, uint8_t *pal)
{
	uint32_t colors[SR_MAX_COLORS];
	long     npx = (long)w * (long)h;
	int      n;

	if (npx <= 0) {
		memset(pal, 0, SR_MAX_COLORS * 3);
		return 1;
	}

	n = sr_collect_colors(rgb, npx, colors);
	if (n < 0) {
		sr_map_cube(rgb, npx, idx, pal);
		return 0;
	}

	/* Sorted, so the palette is a function of the color SET alone -- a frame
	 * redrawn from the same colors yields a byte-identical palette, which is
	 * what lets the present path skip re-sending the sixel color registers. */
	qsort(colors, (size_t)n, sizeof(colors[0]), sr_cmp_u32);
	sr_map_exact(rgb, npx, idx, pal, colors, n);
	return 1;
}
