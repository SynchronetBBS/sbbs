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

/* --- the persistent color -> register map (EXACT mode) ----------------------
 *
 * Registers are shared with whatever is already on the terminal's screen, so a
 * register's meaning must not change while pixels are drawn with it. See
 * syncretro_quant.h. The map only grows; it is rebuilt only when a frame's new
 * colors cannot fit in what is left. */
static uint32_t g_reg_color[SR_MAX_COLORS];   /* register -> color */
static uint8_t  g_reg_taken[SR_MAX_COLORS];
static int      g_reg_count;                  /* registers handed out so far */

void sr_quant_reset(void)
{
	memset(g_reg_taken, 0, sizeof(g_reg_taken));
	g_reg_count = 0;
}

/* The register holding `c`, or -1. Linear over the registers handed out: at most
 * 256 comparisons per DISTINCT color of a frame, never per pixel. */
static int sr_reg_find(uint32_t c)
{
	int k;

	for (k = 0; k < SR_MAX_COLORS; k++)
		if (g_reg_taken[k] && g_reg_color[k] == c)
			return k;
	return -1;
}

static int sr_reg_alloc(uint32_t c)
{
	int k;

	for (k = 0; k < SR_MAX_COLORS; k++)
		if (!g_reg_taken[k]) {
			g_reg_taken[k] = 1;
			g_reg_color[k] = c;
			g_reg_count++;
			return k;
		}
	return -1;                       /* caller rebuilds */
}

/* Give every color of this frame a register, preserving the ones it already had.
 * Returns 0 if the map had to be rebuilt from this frame alone (registers
 * exhausted) -- the caller does not care, but the distinction is real: a rebuild
 * is the one case where a live register changes meaning. */
static int sr_reg_assign_frame(const uint32_t *colors, int n)
{
	int i, rebuilt = 1;

	for (i = 0; i < n; i++)
		if (sr_reg_find(colors[i]) < 0 && sr_reg_alloc(colors[i]) < 0) {
			/* No room. Start over from this frame's colors: sorted, so the
			 * rebuild is at least deterministic. */
			uint32_t sorted[SR_MAX_COLORS];
			int      j;

			memcpy(sorted, colors, (size_t)n * sizeof(*sorted));
			qsort(sorted, (size_t)n, sizeof(sorted[0]), sr_cmp_u32);
			sr_quant_reset();
			for (j = 0; j < n; j++)
				(void)sr_reg_alloc(sorted[j]);
			rebuilt = 0;
			break;
		}
	return rebuilt;
}

/* EXACT: the palette is the register map, and every pixel maps through it. */
static void sr_map_exact(const uint8_t *rgb, long npx, uint8_t *idx, uint8_t *pal,
                         const uint32_t *colors, int n)
{
	uint32_t key[SR_HASH_SIZE];
	uint8_t  val[SR_HASH_SIZE];
	int      i, k;
	long     p;

	(void)sr_reg_assign_frame(colors, n);

	memset(key, 0xff, sizeof(key));
	memset(pal, 0, SR_MAX_COLORS * 3);

	/* The palette is the WHOLE map, not just this frame's colors: a register
	 * retired by this frame keeps its definition, so a color that comes back
	 * (a blinking sprite, a cycling title) lands on the register it always had
	 * and the terminal never has to redefine it. */
	for (k = 0; k < SR_MAX_COLORS; k++)
		if (g_reg_taken[k]) {
			pal[k * 3]     = (uint8_t)(g_reg_color[k] >> 16);
			pal[k * 3 + 1] = (uint8_t)(g_reg_color[k] >> 8);
			pal[k * 3 + 2] = (uint8_t)g_reg_color[k];
		}

	for (i = 0; i < n; i++) {
		unsigned h = sr_hash(colors[i]);
		int      r = sr_reg_find(colors[i]);

		while (key[h] != SR_HASH_EMPTY)
			h = (h + 1) & SR_HASH_MASK;
		key[h] = colors[i];
		val[h] = (uint8_t)r;         /* always >= 0: assign_frame placed them all */
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
		/* CUBE overwrites all 256 registers with the fixed palette, so every
		 * EXACT-mode assignment is void. Forget them: were the frame after this
		 * one to return to EXACT and reuse a stale register number, it would
		 * paint with whatever cube color the terminal now holds there. */
		sr_quant_reset();
		sr_map_cube(rgb, npx, idx, pal);
		return 0;
	}

	sr_map_exact(rgb, npx, idx, pal, colors, n);
	return 1;
}
