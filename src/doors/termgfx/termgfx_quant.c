/* termgfx_quant.c -- deterministic median-cut RGB888 -> 256-color quantizer.
 *
 * Histograms into a 15-bit (5:5:5) grid; when the frame already uses <=256
 * distinct grid cells the mapping is exact -- every pixel's original color
 * survives losslessly, the common case for SCUMM's flat-shaded GUI widgets.
 * Otherwise classic median-cut buckets the grid cells into 256 boxes and a
 * brute-force nearest-color lookup table maps the full 15-bit space to a
 * palette index. */
#include <stdlib.h>
#include <string.h>

#include "termgfx_quant.h"

#define TERMGFX_QUANT_BUCKETS (1 << 15)   /* 5:5:5 = 32768 grid cells */

typedef struct {
	uint32_t r_sum, g_sum, b_sum;
	uint32_t count;
	uint16_t bucket;   /* original 15-bit grid index, for determinism */
} termgfx_quant_entry_t;

typedef struct { int start, end; } termgfx_quant_box_t;

static int termgfx_bucket_of(uint8_t r, uint8_t g, uint8_t b)
{
	return ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
}

/* Sort key for termgfx_entry_cmp(): the axis picked by the current split. */
static int g_sort_axis;

static int termgfx_entry_cmp(const void *pa, const void *pb)
{
	const termgfx_quant_entry_t *a = (const termgfx_quant_entry_t *)pa;
	const termgfx_quant_entry_t *b = (const termgfx_quant_entry_t *)pb;
	uint32_t va, vb;
	switch (g_sort_axis) {
	case 0: va = a->r_sum / a->count; vb = b->r_sum / b->count; break;
	case 1: va = a->g_sum / a->count; vb = b->g_sum / b->count; break;
	default: va = a->b_sum / a->count; vb = b->b_sum / b->count; break;
	}
	if (va != vb)
		return (va < vb) ? -1 : 1;
	/* Stable tiebreak on the original bucket index -- keeps qsort's
	 * (unspecified-order) output deterministic run to run. */
	return (a->bucket < b->bucket) ? -1 : (a->bucket > b->bucket ? 1 : 0);
}

static uint32_t termgfx_box_weight(const termgfx_quant_entry_t *e, termgfx_quant_box_t box)
{
	uint32_t w = 0;
	int i;
	for (i = box.start; i < box.end; i++)
		w += e[i].count;
	return w;
}

/* Widest channel (by mean-value range) across box's entries; 0=r,1=g,2=b. */
static int termgfx_box_axis(const termgfx_quant_entry_t *e, termgfx_quant_box_t box)
{
	uint32_t rmin = 255, rmax = 0, gmin = 255, gmax = 0, bmin = 255, bmax = 0;
	uint32_t rr, gr, br;
	int i;
	for (i = box.start; i < box.end; i++) {
		uint32_t r = e[i].r_sum / e[i].count;
		uint32_t g = e[i].g_sum / e[i].count;
		uint32_t b = e[i].b_sum / e[i].count;
		if (r < rmin) rmin = r;
		if (r > rmax) rmax = r;
		if (g < gmin) gmin = g;
		if (g > gmax) gmax = g;
		if (b < bmin) bmin = b;
		if (b > bmax) bmax = b;
	}
	rr = rmax - rmin;
	gr = gmax - gmin;
	br = bmax - bmin;
	if (rr >= gr && rr >= br)
		return 0;
	return (gr >= br) ? 1 : 2;
}

/* Splits *box in place at its weighted median along the widest axis: box
 * shrinks to the left half, the right half is returned. A box holding a
 * single distinct color (start/end 1 apart) can't split -- returns a
 * zero-length box at box->end, which the caller's weight-based picker
 * never selects again (weight-of-nothing sorts last). */
static termgfx_quant_box_t termgfx_box_split(termgfx_quant_entry_t *e, termgfx_quant_box_t *box)
{
	termgfx_quant_box_t right;
	uint32_t total, half, acc;
	int mid, i;

	right.start = right.end = box->end;
	if (box->end - box->start < 2)
		return right;

	g_sort_axis = termgfx_box_axis(e, *box);
	qsort(e + box->start, (size_t)(box->end - box->start), sizeof(*e), termgfx_entry_cmp);

	total = termgfx_box_weight(e, *box);
	half = total / 2;
	acc = 0;
	mid = box->start + 1;   /* default split keeps both halves non-empty */
	for (i = box->start; i < box->end - 1; i++) {
		acc += e[i].count;
		if (acc >= half) {
			mid = i + 1;
			break;
		}
	}
	right.start = mid;
	right.end = box->end;
	box->end = mid;
	return right;
}

int termgfx_quant_rgb(const uint8_t *rgb, int w, int h, uint8_t *out_idx, uint8_t *out_pal768)
{
	termgfx_quant_entry_t *buckets;
	int32_t *bucket_idx;   /* 15-bit grid slot -> entries[]/box index, -1 = empty */
	int npix = w * h, i, nentries, nboxes = 0;

	memset(out_pal768, 0, 768);

	buckets = (termgfx_quant_entry_t *)calloc(TERMGFX_QUANT_BUCKETS, sizeof(*buckets));
	bucket_idx = (int32_t *)malloc(TERMGFX_QUANT_BUCKETS * sizeof(*bucket_idx));
	if (!buckets || !bucket_idx) {
		free(buckets);
		free(bucket_idx);
		memset(out_idx, 0, (size_t)npix);
		return 0;
	}
	for (i = 0; i < TERMGFX_QUANT_BUCKETS; i++)
		bucket_idx[i] = -1;

	for (i = 0; i < npix; i++) {
		uint8_t r = rgb[i * 3 + 0], g = rgb[i * 3 + 1], b = rgb[i * 3 + 2];
		int bk = termgfx_bucket_of(r, g, b);
		buckets[bk].r_sum += r;
		buckets[bk].g_sum += g;
		buckets[bk].b_sum += b;
		buckets[bk].count++;
	}

	/* Compact the sparse 32768-slot histogram down to its nentries
	 * non-empty buckets, in ascending grid order (determinism). Safe to
	 * compact in place: nentries <= i always, so the write never clobbers
	 * a slot not yet read. */
	nentries = 0;
	for (i = 0; i < TERMGFX_QUANT_BUCKETS; i++) {
		if (buckets[i].count) {
			buckets[nentries] = buckets[i];
			buckets[nentries].bucket = (uint16_t)i;
			nentries++;
		}
	}

	if (nentries <= 256) {
		/* Exact path: one palette entry per distinct color -- lossless,
		 * since every pixel sharing a grid cell has the same original
		 * value whenever the source is already flat-shaded. */
		for (i = 0; i < nentries; i++) {
			out_pal768[i * 3 + 0] = (uint8_t)(buckets[i].r_sum / buckets[i].count);
			out_pal768[i * 3 + 1] = (uint8_t)(buckets[i].g_sum / buckets[i].count);
			out_pal768[i * 3 + 2] = (uint8_t)(buckets[i].b_sum / buckets[i].count);
			bucket_idx[buckets[i].bucket] = i;
		}
		for (i = 0; i < npix; i++) {
			int bk = termgfx_bucket_of(rgb[i * 3 + 0], rgb[i * 3 + 1], rgb[i * 3 + 2]);
			out_idx[i] = (uint8_t)bucket_idx[bk];
		}
		free(buckets);
		free(bucket_idx);
		return nentries;
	}

	/* Median-cut: repeatedly split the heaviest splittable box until 256
	 * boxes exist. nentries > 256 guarantees that's reachable -- the
	 * initial box holds > 256 distinct colors, so it (and its
	 * descendants) can always split at least until 256 boxes exist. */
	{
		termgfx_quant_box_t *boxes = (termgfx_quant_box_t *)malloc(256 * sizeof(*boxes));
		uint8_t *lut;
		int nb = 1;

		if (!boxes) {
			free(buckets);
			free(bucket_idx);
			memset(out_idx, 0, (size_t)npix);
			return 0;
		}
		boxes[0].start = 0;
		boxes[0].end = nentries;

		while (nb < 256) {
			int pick = -1;
			uint32_t bestw = 0;
			for (i = 0; i < nb; i++) {
				uint32_t wgt;
				if (boxes[i].end - boxes[i].start < 2)
					continue;
				wgt = termgfx_box_weight(buckets, boxes[i]);
				if (wgt > bestw) {
					bestw = wgt;
					pick = i;
				}
			}
			if (pick < 0)
				break;   /* every remaining box is a singleton */
			boxes[nb] = termgfx_box_split(buckets, &boxes[pick]);
			nb++;
		}
		nboxes = nb;

		for (i = 0; i < nboxes; i++) {
			uint64_t rs = 0, gs = 0, bs = 0;
			uint32_t cnt = 0;
			int j;
			for (j = boxes[i].start; j < boxes[i].end; j++) {
				rs += buckets[j].r_sum;
				gs += buckets[j].g_sum;
				bs += buckets[j].b_sum;
				cnt += buckets[j].count;
				bucket_idx[buckets[j].bucket] = i;
			}
			out_pal768[i * 3 + 0] = (uint8_t)(rs / cnt);
			out_pal768[i * 3 + 1] = (uint8_t)(gs / cnt);
			out_pal768[i * 3 + 2] = (uint8_t)(bs / cnt);
		}

		/* Nearest-palette lookup table over the full 15-bit space: grid
		 * cells the frame actually used already have their box index
		 * (set above); cells the frame never touched are resolved by
		 * brute-force nearest search against the final palette, so the
		 * table is general rather than sparse. */
		lut = (uint8_t *)malloc(TERMGFX_QUANT_BUCKETS);
		if (lut) {
			for (i = 0; i < TERMGFX_QUANT_BUCKETS; i++) {
				if (bucket_idx[i] >= 0) {
					lut[i] = (uint8_t)bucket_idx[i];
					continue;
				}
				{
					int r = ((i >> 10) & 31) << 3, g = ((i >> 5) & 31) << 3, b = (i & 31) << 3;
					int best = 0, bestd = -1, k;
					for (k = 0; k < nboxes; k++) {
						int dr = r - out_pal768[k * 3 + 0];
						int dg = g - out_pal768[k * 3 + 1];
						int db = b - out_pal768[k * 3 + 2];
						int d = dr * dr + dg * dg + db * db;
						if (bestd < 0 || d < bestd) {
							bestd = d;
							best = k;
						}
					}
					lut[i] = (uint8_t)best;
				}
			}
			for (i = 0; i < npix; i++)
				out_idx[i] = lut[termgfx_bucket_of(rgb[i * 3 + 0], rgb[i * 3 + 1], rgb[i * 3 + 2])];
			free(lut);
		} else {
			/* OOM building the LUT: fall back to the sparse map alone
			 * (every actually-used grid cell still has a box index). */
			for (i = 0; i < npix; i++) {
				int bk = termgfx_bucket_of(rgb[i * 3 + 0], rgb[i * 3 + 1], rgb[i * 3 + 2]);
				out_idx[i] = (uint8_t)(bucket_idx[bk] >= 0 ? bucket_idx[bk] : 0);
			}
		}
		free(boxes);
	}

	free(buckets);
	free(bucket_idx);
	return nboxes;
}
