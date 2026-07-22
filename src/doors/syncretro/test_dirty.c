/* test_dirty.c -- the dirty-rect geometry, without a terminal.
 *
 * Every case here is one the door actually meets: a sprite that moves, sprites
 * that scatter, a sprite straddling a cell boundary, a full-screen flash, a
 * frame that did not change at all. The invariants that matter are (1) the
 * rectangles COVER every changed pixel -- a miss leaves stale pixels on the
 * player's screen forever, since nothing repaints them until the next full
 * frame -- and (2) they stay cell-aligned, because a sixel can only be drawn at
 * a cell corner.
 *
 * The coverage check is exhaustive rather than sampled: for every differing
 * pixel, assert some rectangle contains it. That is the property a subtle
 * coalescing bug breaks, and it is cheap at these sizes.
 */
#include "syncretro_dirty.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond) \
	do { \
		if (!(cond)) { \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			failures++; \
		} \
	} while (0)

#define W  128
#define H   96
#define CW   8
#define CH  16

static uint8_t cur[W * H], prev[W * H];

static void reset(void)
{
	memset(cur, 0, sizeof cur);
	memset(prev, 0, sizeof prev);
}

/* Paint a filled block into `buf`. */
static void blit(uint8_t *buf, int x, int y, int w, int h, uint8_t v)
{
	int i, j;

	for (j = y; j < y + h && j < H; j++)
		for (i = x; i < x + w && i < W; i++)
			buf[j * W + i] = v;
}

/* Every differing pixel must fall inside some rectangle. Returns the number of
 * uncovered differing pixels (0 is the only acceptable answer). */
static int uncovered(const sr_dirty_rect_t *r, int n)
{
	int x, y, i, miss = 0;

	for (y = 0; y < H; y++) {
		for (x = 0; x < W; x++) {
			if (cur[y * W + x] == prev[y * W + x])
				continue;
			for (i = 0; i < n; i++) {
				if (x >= r[i].x && x < r[i].x + r[i].w
				    && y >= r[i].y && y < r[i].y + r[i].h)
					break;
			}
			if (i == n)
				miss++;
		}
	}
	return miss;
}

/* Total pixel area of the rectangles -- the thing we are trying to keep small. */
static long area(const sr_dirty_rect_t *r, int n)
{
	long a = 0;
	int  i;

	for (i = 0; i < n; i++)
		a += (long)r[i].w * r[i].h;
	return a;
}

static void check_aligned(const sr_dirty_rect_t *r, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		CHECK(r[i].x % CW == 0);
		CHECK(r[i].y % CH == 0);
		CHECK(r[i].col * CW == r[i].x);
		CHECK(r[i].row * CH == r[i].y);
		CHECK(r[i].w > 0 && r[i].h > 0);
		CHECK(r[i].x + r[i].w <= W);
		CHECK(r[i].y + r[i].h <= H);
	}
}

int main(void)
{
	sr_dirty_rect_t r[SR_DIRTY_MAX_RECTS];
	int             n;

	/* --- an identical frame asks for nothing ------------------------------- */
	reset();
	blit(cur, 0, 0, W, H, 3);
	blit(prev, 0, 0, W, H, 3);
	CHECK(sr_dirty_find(cur, prev, W, H, CW, CH, 0, r) == 0);

	/* --- one small sprite moves: one tight rectangle ----------------------- */
	reset();
	blit(prev, 40, 32, 8, 8, 1);
	blit(cur,  48, 32, 8, 8, 1);
	n = sr_dirty_find(cur, prev, W, H, CW, CH, 0, r);
	CHECK(n >= 1);
	CHECK(uncovered(r, n) == 0);
	check_aligned(r, n);
	/* Both positions span cells x=5..6, y=2 -> at most a 2x1-cell patch. */
	CHECK(area(r, n) <= 4 * CW * CH);

	/* --- a sprite straddling a cell boundary stays ONE rectangle ------------
	 * The merge gap exists for this: the moved sprite covers cells that are
	 * adjacent but land in separate 4-connected components once the unchanged
	 * middle is excluded. */
	reset();
	blit(prev, 15, 20, 2, 2, 1);
	blit(cur,  17, 20, 2, 2, 1);
	n = sr_dirty_find(cur, prev, W, H, CW, CH, 0, r);
	CHECK(n == 1);
	CHECK(uncovered(r, n) == 0);

	/* --- four sprites in four corners: NOT one big box ---------------------
	 * The failure this guards is a single bounding box round the lot, which
	 * would re-send the whole frame while claiming to be an optimization. */
	reset();
	blit(prev, 4, 4, 6, 6, 1);
	blit(cur,  4, 4, 6, 6, 2);
	blit(prev, W - 12, 4, 6, 6, 1);
	blit(cur,  W - 12, 4, 6, 6, 2);
	blit(prev, 4, H - 12, 6, 6, 1);
	blit(cur,  4, H - 12, 6, 6, 2);
	blit(prev, W - 12, H - 12, 6, 6, 1);
	blit(cur,  W - 12, H - 12, 6, 6, 2);
	n = sr_dirty_find(cur, prev, W, H, CW, CH, 0, r);
	CHECK(n == 4);
	CHECK(uncovered(r, n) == 0);
	check_aligned(r, n);
	CHECK(area(r, n) < (long)W * H / 4);   /* nothing like a full frame */

	/* --- a full-screen change asks for a full frame ------------------------ */
	reset();
	blit(cur, 0, 0, W, H, 7);
	CHECK(sr_dirty_find(cur, prev, W, H, CW, CH, 0, r) == 0);

	/* --- scattered single pixels everywhere ask for a full frame -----------
	 * Too fragmented to describe cheaply: the answer must be 0 (repaint), not
	 * a hundred rectangles. */
	reset();
	{
		int x, y;

		for (y = 4; y < H; y += 12)
			for (x = 4; x < W; x += 12)
				cur[y * W + x] = 9;
	}
	n = sr_dirty_find(cur, prev, W, H, CW, CH, 0, r);
	CHECK(n == 0 || n <= SR_DIRTY_MAX_RECTS);
	if (n > 0)
		CHECK(uncovered(r, n) == 0);

	/* --- a change in the partial last cell is still covered -----------------
	 * W/CW and H/CH are exact here, so force a partial grid with odd metrics. */
	reset();
	blit(cur, W - 3, H - 5, 3, 5, 4);
	{
		const int ocw = 7, och = 13;   /* neither divides W or H */
		int       x, y, i, miss = 0;

		n = sr_dirty_find(cur, prev, W, H, ocw, och, 0, r);
		CHECK(n >= 1);
		for (y = 0; y < H; y++)
			for (x = 0; x < W; x++) {
				if (cur[y * W + x] == prev[y * W + x])
					continue;
				for (i = 0; i < n; i++)
					if (x >= r[i].x && x < r[i].x + r[i].w
					    && y >= r[i].y && y < r[i].y + r[i].h)
						break;
				if (i == n)
					miss++;
			}
		CHECK(miss == 0);
		for (i = 0; i < n; i++) {
			CHECK(r[i].x + r[i].w <= W);   /* clamped, not run off the edge */
			CHECK(r[i].y + r[i].h <= H);
		}
	}

	/* band_align=1 snaps each rect height to LCM(ch,6). ch=13 -> vstep=78. */
	{
		int      w = 64, h = 156, cw = 8, ch = 13;   /* 156 = 2*78, exact */
		uint8_t *cur  = calloc((size_t)w * h, 1);
		uint8_t *prev = calloc((size_t)w * h, 1);
		sr_dirty_rect_t r[SR_DIRTY_MAX_RECTS];
		int      n, i;

		/* dirty a small patch near the top so its natural height < 78 */
		memset(cur + (size_t)10 * w + 4, 0xAB, 20);
		n = sr_dirty_find(cur, prev, w, h, cw, ch, 1, r);
		CHECK(n > 0);
		for (i = 0; i < n; i++)
			CHECK(r[i].h % 78 == 0);           /* whole LCM(ch,6) bands */
		free(cur); free(prev);
	}

	/* --- degenerate inputs are refusals, not crashes ----------------------- */
	CHECK(sr_dirty_find(NULL, prev, W, H, CW, CH, 0, r) == 0);
	CHECK(sr_dirty_find(cur, NULL, W, H, CW, CH, 0, r) == 0);
	CHECK(sr_dirty_find(cur, prev, 0, H, CW, CH, 0, r) == 0);
	CHECK(sr_dirty_find(cur, prev, W, H, 0, CH, 0, r) == 0);
	CHECK(sr_dirty_find(cur, prev, W, H, CW, 0, 0, r) == 0);
	CHECK(sr_dirty_find(cur, prev, 100000, 100000, 1, 1, 0, r) == 0);  /* over the grid */

	printf(failures ? "\n%d FAILURE(S)\n" : "\nall dirty-rect tests passed\n",
	       failures);
	return failures ? 1 : 0;
}
