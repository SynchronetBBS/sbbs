#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "ciolib.h"
#include "scale.h"
#include "xbr.h"

#ifdef _MSC_VER
#pragma warning(disable : 4244 4267 4018)
#endif

static void pointy_scale3(const uint32_t* src, uint32_t* dest, const int width, const int height);
static void pointy_scale5(const uint32_t* src, uint32_t* dest, const int width, const int height);
static void pointy_scale_odd(const uint32_t* src, uint32_t* dest, const int width, const int height, const int mult);
static void interpolate_height(uint32_t const* src, uint32_t* dst, const int width, const int height, const int newheight);
static void interpolate_width(uint32_t const* src, uint32_t* dst, const int width, const int height, const int newwidth);
static void multiply_scale(uint32_t const* src, uint32_t* dst, const int width, const int height, const int xmult, const int ymult);

static struct graphics_buffer *free_list;

/*
 * Corrects width/height to have the specified aspect ratio
 * any fit inside the specified rectangle
 */
void
aspect_fix_wc(int *x, int *y, bool wc, int aspect_width, int aspect_height)
{
	int bestx, besty;

	if (aspect_width == 0 || aspect_height == 0)
		return;
	bestx = lround((double)*y * aspect_width / aspect_height);
	besty = lround((double)*x * aspect_height / aspect_width);

	if (wc)
		*y = besty;
	else
		*x = bestx;
}

/*
 * Corrects width/height to have the specified aspect ratio
 * any fit inside the specified rectangle
 */
void
aspect_fix_inside(int *x, int *y, int aspect_width, int aspect_height)
{
	int bestx, besty;

	if (aspect_width == 0 || aspect_height == 0)
		return;
	bestx = lround((double)*y * aspect_width / aspect_height);
	besty = lround((double)*x * aspect_height / aspect_width);

	if (besty <= *y)
		*y = besty;
	else if (bestx <= *x)
		*x = bestx;
	else {
		fprintf(stderr, "Unable to fix %dx%d at ratio %d:%d (best %d, %d)\n", *x, *y, aspect_width, aspect_height, bestx, besty);
	}
}

/*
 * Corrects width/height to have the specified aspect ratio
 */
void
aspect_fix(int *x, int *y, int aspect_width, int aspect_height)
{
	int bestx, besty;

	// Nothing we can do here...
	if (aspect_width == 0 || aspect_height == 0)
		return;
	bestx = lround((double)*y * aspect_width / aspect_height);
	besty = lround((double)*x * aspect_height / aspect_width);

	if (bestx < *x && besty > 0)
		*y = besty;
	else
		*x = bestx;
}

/*
 * Corrects width/height to have the specified aspect ratio
 */
void
aspect_fix_low(int *x, int *y, int aspect_width, int aspect_height)
{
	int bestx, besty;

	// Nothing we can do here...
	if (aspect_width == 0 || aspect_height == 0)
		return;
	bestx = lround((double)*y * aspect_width / aspect_height);
	besty = lround((double)*x * aspect_height / aspect_width);

	if (bestx < *x && bestx > 0)
		*x = bestx;
	else
		*y = besty;
}

/*
 * Given a width/height of a source image, adjust it to match the current aspect
 * ratio.  Will not reduce either number
 */
void
aspect_correct(int *x, int *y, int aspect_width, int aspect_height)
{
	int width = *x;
	int height;

	if (!aspect_height || !aspect_width)
		return;
	height = lround((double)(width * aspect_height) / aspect_width);
	if (height < *y) {
		height = *y;
		width = lround(((double)height * aspect_width) / aspect_height);
	}

	*x = width;
	*y = height;
}

/*
 * Essentially the opposite of the above.  Given an output width/height, translates to
 * the size of the source image.
 *
 * Note that this is much trickier as the "source" bitmap may have been integer scaled
 * differently in both directions... so what this does is reverse the aspect ratio
 * calculation, then find the next lowest even multiple of the mode bitmap size.
 */
void
aspect_reverse(int *x, int *y, int scrnwidth, int scrnheight, int aspect_width, int aspect_height)
{
	int width = *x;
	int height = *y;
	int cheight;
	int cwidth;

	if (!aspect_height || !aspect_width) {
		width = scrnwidth * (*x / scrnwidth);
		if (width < scrnwidth)
			width = scrnwidth;
		height = scrnheight * (*x / scrnheight);
		if (height < scrnheight)
			height = scrnheight;
		return;
	}
	// First, find the "controlling" dimension... the one that won't be scaled (ie: the one that gets smaller)
	cwidth = lround((double)(height * aspect_width) / aspect_height * scrnwidth / scrnheight);
	cheight = lround((double)(width * aspect_height) / aspect_width * scrnheight / scrnwidth);
	if (cwidth > width) {
		// Width controls, so this is simply finding the largest width multiple that fits in the box
		width = scrnwidth * (*x / scrnwidth);
		if (width < scrnwidth)
			width = scrnwidth;

		// Now we need to find the largest bitmap height that would fit in the output height
		// So, we scale the height to bitmap size...
		height = lround((double)*y / ((double)scrnwidth / scrnheight) * ((double)aspect_width / aspect_height));
		// And do the same calculation...
		height = lround((double)scrnheight * ((double)height / scrnheight));
	}
	else if (cheight > height) {
		// Height controls
		height = scrnheight * (*x / scrnheight);
		if (height < scrnheight)
			height = scrnheight;

		width = lround((double)*x / ((double)scrnheight / scrnwidth) * ((double)aspect_height / aspect_width));
		width = lround((double)scrnwidth * ((double)width / scrnwidth));
	}

	*x = width;
	*y = height;
}

struct graphics_buffer *
get_buffer(void)
{
	struct graphics_buffer* ret = NULL;

	if (free_list) {
		ret = free_list;
		free_list = free_list->next;
		ret->next = NULL;
		return ret;
	}

	ret = calloc(1, sizeof(struct graphics_buffer));
	return ret;
}

void
release_buffer(struct graphics_buffer *buf)
{
	if (buf == NULL)
		return;
	buf->next = free_list;
	free_list = buf;
}

void
calc_scaling_factors(int *x, int *y, int winwidth, int winheight, int aspect_width, int aspect_height, int scrnwidth, int scrnheight)
{
	aspect_fix_low(&winwidth, &winheight, aspect_width, aspect_height);
	aspect_reverse(&winwidth, &winheight, scrnwidth, scrnheight, aspect_width, aspect_height);
	*x = winwidth / scrnwidth;
	*y = winheight / scrnheight;
	if (*x < 1 || *x > 14)
		*x = 1;
	if (*y < 1 || *y > 14)
		*y = 1;
}

struct graphics_buffer *
do_scale(struct rectlist* rect, int fwidth, int fheight)
{
	struct graphics_buffer* ret1 = get_buffer();
	struct graphics_buffer* ret2 = get_buffer();
	int pointymult = 1;
	int pointy5 = 0;
	int pointy3 = 0;
	int xbr2 = 0;
	int xbr4 = 0;
	int ymult = 1;
	int xmult = 1;
	int total_xscaling = 1;
	int total_yscaling = 1;
	int minscale = 1;
	struct graphics_buffer *ctarget;
	struct graphics_buffer *csrc;
	uint32_t* nt;
	bool swapxy = false;

	if (rect->rect.width == 0 || rect->rect.height == 0 || fwidth == 0 || fheight == 0)
		return NULL;

	int xscale = fwidth / rect->rect.width;
	int yscale = fheight / rect->rect.height;

	if (xscale > yscale) {
		swapxy = true;
		total_xscaling = xscale;
		xscale = yscale;
		yscale = total_xscaling;
	}
	total_xscaling = xscale;
	xscale = 1;
	total_yscaling = yscale;
	yscale = 1;
	if (total_xscaling < total_yscaling)
		minscale = total_xscaling;
	else
		minscale = total_yscaling;
	// If x/y scaling isn't a simple multiple, block scale everything...
	if (!(cio_api.options & CONIO_OPT_BLOCKY_SCALING)) {
		if ((minscale & 1) == 1 && minscale > 5) {
			pointymult = total_xscaling;
			total_xscaling /= pointymult;
			xscale *= pointymult;
			total_yscaling /= pointymult;
			yscale *= pointymult;
			minscale /= pointymult;
		}
		while (minscale > 1 && ((minscale % 5) == 0) && ((minscale % 5) == 0)) {
			pointy5++;
			total_xscaling /= 5;
			xscale *= 5;
			total_yscaling /= 5;
			yscale *= 5;
			minscale /= 5;
		}
		while (minscale > 1 && ((minscale % 3) == 0) && ((minscale % 3) == 0)) {
			pointy3++;
			total_xscaling /= 3;
			xscale *= 3;
			total_yscaling /= 3;
			yscale *= 3;
			minscale /= 3;
		}
		while (minscale > 1 && ((minscale % 4) == 0) && ((minscale % 4) == 0)) {
			xbr4++;
			total_xscaling /= 4;
			xscale *= 4;
			total_yscaling /= 4;
			yscale *= 4;
			minscale /= 4;
		}
		while (minscale > 1 && ((minscale % 2) == 0) && ((minscale % 2) == 0)) {
			xbr2++;
			total_xscaling /= 2;
			xscale *= 2;
			total_yscaling /= 2;
			yscale *= 2;
			minscale /= 2;
		}
	}

	xmult = total_xscaling;
	xscale *= xmult;
	total_xscaling = 1;

	ymult = total_yscaling;
	yscale *= ymult;
	total_yscaling = 1;

	if (swapxy) {
		int tmp;

		tmp = ymult;
		ymult = xmult;
		xmult = tmp;

		tmp = xscale;
		xscale = yscale;
		yscale = tmp;
	}

	// Now make sure target is big enough...
	size_t needsz = fwidth * fheight * sizeof(uint32_t);
	if (needsz > ret1->sz) {
		nt = realloc(ret1->data, needsz);
		if (nt == NULL) {
			release_buffer(ret1);
			release_buffer(ret2);
			return NULL;
		}
		ret1->data = nt;
		ret1->sz = needsz;
	}

	if (needsz > ret2->sz) {
		nt = realloc(ret2->data, needsz);
		if (nt == NULL) {
			release_buffer(ret1);
			release_buffer(ret2);
			return NULL;
		}
		ret2->data = nt;
		ret2->sz = needsz;
	}

	// Copy rect into first buffer
	// TODO: Unify bitmap rects and scaling buffers so this can just whomp on over.
	csrc = ret1;
	ctarget = ret2;
	memcpy(csrc->data, rect->data, rect->rect.width * rect->rect.height * sizeof(rect->data[0]));
	csrc->w = rect->rect.width;
	csrc->h = rect->rect.height;

#if 0
fprintf(stderr, "Plan:\n"
"start:       %zux%zu\n"
"pointymulti: %d\n"
"pointy5:     %d\n"
"pointy3:     %d\n"
"xBR4:        %d\n"
"xBR2:        %d\n"
"Multiply:    %dx%d\n"
"hinterp:     %zu -> %d\n"
"winterp:     %zu -> %d\n",
csrc->w, csrc->h, pointymult, pointy5, pointy3, xbr4, xbr2, xmult, ymult, csrc->h * yscale, fheight, csrc->w * xscale, fwidth);
#endif
	// And scale...
	if (ymult != 1 || xmult != 1) {
		multiply_scale(csrc->data, ctarget->data, csrc->w, csrc->h, xmult, ymult);
		ctarget->w = csrc->w * xmult;
		ctarget->h = csrc->h * ymult;
		ymult = 1;
		xmult = 1;
		csrc = ctarget;
		if (ctarget == ret1)
			ctarget = ret2;
		else
			ctarget = ret1;
	}
	if (pointymult > 1 && pointymult & 1) {
		pointy_scale_odd(csrc->data, ctarget->data, csrc->w, csrc->h, pointymult);
		ctarget->w = csrc->w * pointymult;
		ctarget->h = csrc->h * pointymult;
		pointymult = 1;
		csrc = ctarget;
		if (ctarget == ret1)
			ctarget = ret2;
		else
			ctarget = ret1;
	}
	while (pointy5 > 0) {
		pointy_scale5(csrc->data, ctarget->data, csrc->w, csrc->h);
		pointy5--;
		ctarget->w = csrc->w * 5;
		ctarget->h = csrc->h * 5;
		csrc = ctarget;
		if (ctarget == ret1)
			ctarget = ret2;
		else
			ctarget = ret1;
	}
	while (pointy3 > 0) {
		pointy_scale3(csrc->data, ctarget->data, csrc->w, csrc->h);
		pointy3--;
		ctarget->w = csrc->w * 3;
		ctarget->h = csrc->h * 3;
		csrc = ctarget;
		if (ctarget == ret1)
			ctarget = ret2;
		else
			ctarget = ret1;
	}
	while (xbr4 > 0) {
		xbr_filter(csrc->data, ctarget->data, csrc->w, csrc->h, 4);
		xbr4--;
		ctarget->w = csrc->w * 4;
		ctarget->h = csrc->h * 4;
		csrc = ctarget;
		if (ctarget == ret1)
			ctarget = ret2;
		else
			ctarget = ret1;
	}
	while (xbr2 > 0) {
		xbr_filter(csrc->data, ctarget->data, csrc->w, csrc->h, 2);
		xbr2--;
		ctarget->w = csrc->w * 2;
		ctarget->h = csrc->h * 2;
		csrc = ctarget;
		if (ctarget == ret1)
			ctarget = ret2;
		else
			ctarget = ret1;
	}

	// And finally, interpolate if needed
	if (fheight != csrc->h) {
		interpolate_height(csrc->data, ctarget->data, csrc->w, csrc->h, fheight);
		ctarget->h = fheight;
		ctarget->w = csrc->w;
		csrc = ctarget;
		if (ctarget == ret1)
			ctarget = ret2;
		else
			ctarget = ret1;
	}
	
	if (fwidth != csrc->w) {
		interpolate_width(csrc->data, ctarget->data, csrc->w, csrc->h, fwidth);
		ctarget->h = csrc->h;
		ctarget->w = fwidth;
		csrc = ctarget;
		if (ctarget == ret1)
			ctarget = ret2;
		else
			ctarget = ret1;
	}

	release_buffer(ctarget);
	return csrc;
}

static void
pointy_scale_odd(const uint32_t* src, uint32_t* dest, const int width, const int height, const int mult)
{
	int x, y;
	const uint32_t* s;
	uint32_t* d;
	int prevline, prevcol, nextline, nextcol;
	int i, j;
	int mid = mult / 2;
	int multoff = mult - 1;
	int dline = width * mult;
	int dbott;
	int dstripe = dline * mult;

	s = src;
	d = dest;
	prevline = 0;
	nextline = width;
	for (y = 0; y < height; y++) {
		if (y == height - 1)
			nextline = 0;
		prevcol = 0;
		nextcol = 1;
		for (x = 0; x < width; x++) {
			if (x == width - 1)
				nextcol = 0;

			for (i = 0; i < mid; i++) {
				d = &dest[dstripe * y + dline * i + x * mult];
				dbott = dline * (multoff - i * 2);

				for (j = 0; j < mid - i; j++) {
					if (s[prevline + prevcol] == s[0]) {
						d[j] = s[0];
					}
					else if (s[prevline] == s[prevcol]) {
						d[j] = s[prevcol];
					}
					else {
						d[j] = s[0];
					}

					if (s[prevline + nextcol] == s[0]) {
						d[multoff - j] = s[0];
					}
					else if (s[prevline] == s[nextcol]) {
						d[multoff - j] = s[nextcol];
					}
					else {
						d[multoff - j] = s[0];
					}

					if (s[prevcol + nextline] == s[0]) {
						d[dbott + j] = s[0];
					}
					else if(s[prevcol] == s[nextline]) {
						d[dbott + j] = s[prevcol];
					}
					else {
						d[dbott + j] = s[0];
					}

					if (s[nextcol + nextline] == s[0]) {
						d[dbott + multoff - j] = s[0];
					}
					else if (s[nextcol] == s[nextline]) {
						d[dbott + multoff - j] = s[nextcol];
					}
					else {
						d[dbott + multoff - j] = s[0];
					}
				}

				// And the rest is always kept the same
				for (; j < mid; j++) {
					d[j] = s[0];
					d[multoff - j] = s[0];
					d[dbott + j] = s[0];
					d[dbott + multoff - j] = s[0];
				}

				// And the middle dot.
				d[j] = s[0];
				d[dbott + j] = s[0];
			}

			d = &dest[dstripe * y + dline * i + x * mult];

			for (j = 0; j < mid; j++) {
				d[j] = s[0];
				d[multoff - j] = s[0];
			}
			d[j] = s[0];

			s++;

			if (x == 0)
				prevcol = -1;
		}
		if (y == 0)
			prevline = -width;
	}
}

static void
pointy_scale5(const uint32_t* src, uint32_t* dest, const int width, const int height)
{
	int x, y;
	const uint32_t* s;
	uint32_t* d;
	int w5 = width * 5;
	int w10 = width * 10;
	int w15 = width * 15;
	int w20 = width * 20;
	int prevline, prevcol, nextline, nextcol;

	s = src;
	d = dest;
	prevline = 0;
	nextline = width;
	for (y = 0; y < height; y++) {
		if (y == height - 1)
			nextline = 0;
		prevcol = 0;
		nextcol = 1;
		for (x = 0; x < width; x++) {
			if (x == width - 1)
				nextcol = 0;

			if (s[prevline + prevcol] == s[0]) {
				d[0] = s[0];
				d[1] = s[0];
				d[w5] = s[0];
			}
			else if (s[prevcol] == s[prevline]) {
				d[0] = s[prevcol];
				d[1] = s[prevcol];
				d[w5] = s[prevcol];
			}
			else {
				d[0] = *s;
				d[1] = *s;
				d[w5] = *s;
			}

			// Top-middle stays OG.
			d[2] = *s;
			d[w5+1] = *s;
			d[w5+2] = *s;
			d[w5+3] = *s;

			// And so on around the outside (round the outside)
			if (s[prevline + nextcol] == s[0]) {
				d[3] = s[0];
				d[4] = s[0];
				d[w5 + 4] = s[0];
			}
			else if (s[nextcol] == s[prevline]) {
				d[3] = s[nextcol];
				d[4] = s[nextcol];
				d[w5 + 4] = s[nextcol];
			}
			else {
				d[3] = s[0];
				d[4] = s[0];
				d[w5 + 4] = s[0];
			}

			d[w10] = *s;
			d[w10+1] = *s;
			d[w10+2] = *s;
			d[w10+3] = *s;
			d[w10+4] = *s;

			if (s[prevcol + nextline] == s[0]) {
				d[w15] = s[0];
				d[w20] = s[0];
				d[w20 + 1] = s[0];
			}
			else if(s[prevcol] == s[nextline]) {
				d[w15] = s[prevcol];
				d[w20] = s[prevcol];
				d[w20 + 1] = s[prevcol];
			}
			else {
				d[w15] = s[0];
				d[w20] = s[0];
				d[w20 + 1] = s[0];
			}

			d[w15 + 1] = *s;
			d[w15 + 2] = *s;
			d[w15 + 3] = *s;
			d[w20 + 2] = *s;

			if (s[nextcol + nextline] == s[0]) {
				d[w15 + 4] = s[0];
				d[w20 + 3] = s[0];
				d[w20 + 4] = s[0];
			}
			else if (s[nextcol] == s[nextline]) {
				d[w15 + 4] = s[nextcol];
				d[w20 + 3] = s[nextcol];
				d[w20 + 4] = s[nextcol];
			}
			else {
				d[w15 + 4] = s[0];
				d[w20 + 3] = s[0];
				d[w20 + 4] = s[0];
			}

			d += 5;
			s++;

			if (x == 0)
				prevcol = -1;
		}
		d += w20;
		if (y == 0)
			prevline = -width;
	}
}

static void
pointy_scale3(const uint32_t* src, uint32_t* dest, const int width, const int height)
{
	int x, y;
	const uint32_t* s;
	uint32_t* d;
	int w3 = width * 3;
	int w6 = width * 6;
	int prevline, prevcol, nextline, nextcol;

	s = src;
	d = dest;
	prevline = 0;
	nextline = width;
	for (y = 0; y < height; y++) {
		if (y == height - 1)
			nextline = 0;
		prevcol = 0;
		nextcol = 1;
		for (x = 0; x < width; x++) {
			if (x == width - 1)
				nextcol = 0;

			// Top-left is filled if both left and top are the same.
			if (s[prevline + prevcol] == s[0])
				d[0] = s[0];
			else if (s[prevcol] == s[prevline])
				d[0] = s[prevcol];
			else
				d[0] = *s;

			// Top-middle stays OG.
			d[1] = *s;

			// And so on around the outside (round the outside)
			if (s[prevline + nextcol] == s[0])
				d[2] = s[0];
			else if (s[nextcol] == s[prevline])
				d[2] = s[nextcol];
			else
				d[2] = *s;

			d[w3] = *s;
			d[w3 + 1] = *s;
			d[w3 + 2] = *s;

			if (s[prevcol + nextline] == s[0])
				d[w6] = s[0];
			else if(s[prevcol] == s[nextline])
				d[w6] = s[prevcol];
			else
				d[w6] = *s;

			d[w6 + 1] = *s;

			if (s[nextcol + nextline] == s[0])
				d[w6 + 2] = s[0];
			else if (s[nextcol] == s[nextline])
				d[w6 + 2] = s[nextcol];
			else
				d[w6 + 2] = s[0];

			d += 3;
			s++;

			if (x == 0)
				prevcol = -1;
		}
		d += w6;
		if (y == 0)
			prevline = -width;
	}
}

struct YCoCg_data {
	uint32_t Y; // 0 to 255
	int32_t Co;  // -255 to 255
	int32_t Cg;  // -255 to 255
};

static inline void
RGB_to_YCoCg(const uint32_t RGB, struct YCoCg_data * const YCoCg)
{
	const uint8_t R = (RGB >> 16) & 0xFF;
	const uint8_t G = (RGB >> 8) & 0xFF;
	const uint8_t B = (RGB) & 0xFF;

	YCoCg->Co = R - B; // -255 to 255
	const uint8_t tmp = B + (YCoCg->Co / 2);
	YCoCg->Cg = G - tmp;
	YCoCg->Y = tmp + (YCoCg->Cg / 2);
}

static inline uint32_t
YCoCg_to_RGB(struct YCoCg_data const * const YCoCg)
{
	const int16_t tmp = YCoCg->Y - (YCoCg->Cg / 2);
	const int16_t Gi = YCoCg->Cg + tmp;
	const int16_t Bi = tmp - (YCoCg->Co / 2);
	const int16_t Ri = Bi + YCoCg->Co;
	const uint32_t R = (Ri < 0) ? 0 : ((Ri > 255) ? 255 : Ri);
	const uint32_t G = (Gi < 0) ? 0 : ((Gi > 255) ? 255 : Gi);
	const uint32_t B = (Bi < 0) ? 0 : ((Bi > 255) ? 255 : Bi);
	return (R << 16) | (G << 8) | B;
}

static inline uint32_t
blend_YCoCg(const uint32_t c1, const uint32_t c2, const uint16_t weight)
{
	const uint16_t iw = 65535 - weight;

	struct YCoCg_data ycc1;
	struct YCoCg_data ycc2;
	struct YCoCg_data ycc3;

	RGB_to_YCoCg(c1, &ycc1);
	RGB_to_YCoCg(c2, &ycc2);

	ycc3.Y = (ycc1.Y * iw + ycc2.Y * weight) / 65535;
	ycc3.Co = (ycc1.Co * iw + ycc2.Co * weight) / 65535;
	ycc3.Cg = (ycc1.Cg * iw + ycc2.Cg * weight) / 65535;

	return YCoCg_to_RGB(&ycc3);
}

/*
 * This does non-integer *width* scaling.  It does not scale in the other
 * direction.  This does the interpolation using Y'UV to prevent dimming of
 * pixels.
 */
static void
interpolate_width(uint32_t const* src, uint32_t* dst, const int width, const int height, const int newwidth)
{
	int x, y;
	const uint64_t mult = (((uint64_t)width) << 16) / newwidth;
	uint32_t *s = dst;
	const int wm1 = width - 1;

	int srow_start = 0;
	int drow_start = 0;
	for (y = 0; y < height; y++) {
		uint64_t xpos = 0;
		dst = &s[drow_start];
		for (x = 0; x < newwidth; x++) {
			// First, calculate which two pixels this is between.
			const int xposi = xpos >> 16;
			const uint16_t weight = xpos & 0xFFFF;
			const int yposi = srow_start + xposi;
			if (weight == 0) {
				// Exact match!
				*dst = src[yposi];
			}
			else {
				// Now pick the two pixels
				const uint32_t pix1 = src[yposi];
				uint32_t pix2;
				if (xposi < wm1)
					pix2 = src[yposi + 1];
				else
					pix2 = src[yposi];
				if (pix1 == pix2)
					*dst = pix1;
				else {
					*dst = blend_YCoCg(pix1, pix2, weight);
				}
			}
			xpos += mult;
			dst++;
		}
		srow_start += width;
		drow_start += newwidth;
	}
}

/*
 * This does non-integer *height* scaling.  It does not scale in the other
 * direction.  This does the interpolation using Y'UV to prevent dimming of
 * pixels.
 */
static void
interpolate_height(uint32_t const* src, uint32_t* dst, const int width, const int height, const int newheight)
{
	int x, y;
	const uint64_t mult = (((uint64_t)height) << 16) / newheight;
	uint64_t ypos = 0;
	int last_yposi = 0;
	int ywn = width;
	static uint32_t *nline = NULL;
	static uint32_t *tline = NULL;
	static size_t nsz = 0;
	static size_t tsz = 0;
	uint32_t *stmp;

	if (nsz < width * 4) {
		stmp = realloc(nline, width * 4);
		if (stmp == NULL)
			goto fail;
		nline = stmp;
		nsz = width * 4;
	}
	if (tsz < width * 4) {
		stmp = realloc(tline, width * 4);
		if (stmp == NULL)
			goto fail;
		tline = stmp;
		tsz = width * 4;
	}

	memcpy(tline, src, width * sizeof(*tline));
	memcpy(nline, src + width, width * sizeof(*tline));
	for (y = 0; y < newheight; y++) {
		const int yposi = ypos >> 16;
		const uint16_t weight = ypos & 0xffff;
		if (yposi != last_yposi) {
			ywn += width;
			last_yposi = yposi;
			stmp = tline;
			tline = nline;
			nline = stmp;
			memcpy(nline, &src[ywn], nsz);
		}
		if (weight == 0 || yposi >= height - 1) {
			memcpy(dst, tline, tsz);
			dst += width;
		}
		else {
			for (x = 0; x < width; x++) {
				// Now pick the two pixels
				const uint32_t pix1 = tline[x];
				const uint32_t pix2 = nline[x];
				if (pix1 == pix2)
					*dst = pix1;
				else
					*dst = blend_YCoCg(pix1, pix2, weight);
				dst++;
			}
		}
		ypos += mult;
	}

	return;
fail:
	free(nline);
	free(tline);
	nline = NULL;
	tline = NULL;
	nsz = 0;
	tsz = 0;
	memcpy(dst, src, width * height * sizeof(*src));
	fprintf(stderr, "Allocation failure in interpolate_height()!");
}

static void
multiply_scale(uint32_t const* src, uint32_t* dst, const int width, const int height, const int xmult, const int ymult)
{
	int x, y;
	int mx, my;
	uint32_t const* slstart;

	for (y = 0; y < height; y++) {
		slstart = src;
		for (my = 0; my < ymult; my++) {
			src = slstart;
			for (x = 0; x < width; x++) {
				for (mx = 0; mx < xmult; mx++) {
					*dst = *src;
					dst++;
				}
				src++;
			}
		}
	}
}
