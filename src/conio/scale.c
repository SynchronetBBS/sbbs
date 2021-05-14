#include "ciolib.h"
#include "scale.h"
#include "xbr.h"

uint32_t r2y[1<<24];
uint32_t y2r[1<<24];
static int r2y_inited;

static void pointy_scale3(uint32_t* src, uint32_t* dest, int width, int height);
static void pointy_scale5(uint32_t* src, uint32_t* dest, int width, int height);
static void pointy_scale_odd(uint32_t* src, uint32_t* dest, int width, int height, int mult);
static void interpolate_height(uint32_t* src, uint32_t* dst, int width, int height, int newheight);
static void interpolate_width(uint32_t* src, uint32_t* dst, int width, int height, int newwidth);
static void multiply_scale(uint32_t* src, uint32_t* dst, int width, int height, int xmult, int ymult);

static struct graphics_buffer *free_list;

#define CLAMP(x) do { \
	if (x < 0) \
		x = 0; \
	else if (x > 255) \
		x = 255; \
} while(0)

void
init_r2y(void)
{
	int r, g, b;
	int y, u, v;
	const double luma = 255.0 / 219;
	const double col  = 255.0 / 224;

	if (r2y_inited)
		return;
	for (r = 0; r < 256; r++) {
		for (g = 0; g < 256; g++) {
			for (b = 0; b < 256; b++) {
				y =  16 + ( 65.738 * r + 129.057 * g +  25.064 * b + 128) / 256;
				CLAMP(y);
				u = 128 + (-37.945 * r -  74.494 * g + 112.439 * b + 128) / 256;
				CLAMP(u);
				v = 128 + (112.439 * r -  94.154 * g -  18.285 * b + 128) / 256;
				CLAMP(v);

				r2y[(r<<16) | (g<<8) | b] = (y<<16)|(u<<8)|v;
			}
		}
	}
	for (y = 0; y < 256; y++) {
		for (u = 0; u < 256; u++) {
			for (v = 0; v < 256; v++) {
				const int c = y - 16;
				const int d = u - 128;
				const int e = v - 128;
				r = luma * c                                     + col * 1.402 * e;
				CLAMP(r);
				g = luma * c - col * 1.772 * (0.114 / 0.587) * d - col * 1.402 * (0.299 / 0.587) * e;
				CLAMP(g);
				b = luma * c + col * 1.772 * d;
				CLAMP(b);

				y2r[(y<<16) | (u<<8) | v] = (r<<16)|(g<<8)|b;
			}
		}
	}
	r2y_inited = 1;
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
	buf->next = free_list;
	free_list = buf;
}

struct graphics_buffer *
do_scale(struct rectlist* rect, int xscale, int yscale, double ratio)
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
	struct graphics_buffer *ctarget;
	struct graphics_buffer *csrc;
	uint32_t* nt;
	int fheight;
	int fwidth;
	bool swapxy = false;

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
	if (!(cio_api.options & CONIO_OPT_BLOCKY_SCALING)) {
		if ((total_xscaling & 1) == 1 && (total_xscaling == total_yscaling || (total_yscaling % total_xscaling == 0))) {
			pointymult = total_xscaling;
			total_xscaling /= pointymult;
			xscale *= pointymult;
			total_yscaling /= pointymult;
			yscale *= pointymult;
		}
		while (total_xscaling > 1 && ((total_xscaling % 5) == 0) && ((total_yscaling % 5) == 0)) {
			pointy5++;
			total_xscaling /= 5;
			xscale *= 5;
			total_yscaling /= 5;
			yscale *= 5;
		}
		while (total_xscaling > 1 && ((total_xscaling % 3) == 0) && ((total_yscaling % 3) == 0)) {
			pointy3++;
			total_xscaling /= 3;
			xscale *= 3;
			total_yscaling /= 3;
			yscale *= 3;
		}
		while (total_xscaling > 1 && ((total_xscaling % 4) == 0) && ((total_yscaling % 4) == 0)) {
			xbr4++;
			total_xscaling /= 4;
			xscale *= 4;
			total_yscaling /= 4;
			yscale *= 4;
		}
		while (total_xscaling > 1 && ((total_xscaling % 2) == 0) && ((total_yscaling % 2) == 0)) {
			xbr2++;
			total_xscaling /= 2;
			xscale *= 2;
			total_yscaling /= 2;
			yscale *= 2;
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

	// Calculate the scaled height from ratio...
	if (ratio < 1)
		fheight = lround((double)(rect->rect.height * (yscale)) / ratio);
	else
		fheight = rect->rect.height * yscale;

	if (ratio > 1)
		fwidth = lround((double)(rect->rect.width * (xscale)) / ratio);
	else
		fwidth = rect->rect.width * xscale;

	// Now make sure target is big enough...
	size_t needsz = fwidth * fheight * sizeof(uint32_t);
	if (needsz > ret1->sz) {
		nt = realloc(ret1->data, needsz);
		if (nt == NULL)
			return NULL;
		ret1->data = nt;
		ret1->sz = needsz;
	}

	if (needsz > ret2->sz) {
		nt = realloc(ret2->data, needsz);
		if (nt == NULL)
			return NULL;
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
	if (ratio < 1) {
		interpolate_height(csrc->data, ctarget->data, csrc->w, csrc->h, fheight);
		ctarget->h = fheight;
		ctarget->w = csrc->w;
		csrc = ctarget;
		if (ctarget == ret1)
			ctarget = ret2;
		else
			ctarget = ret1;
	}

	if (ratio > 1) {
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
pointy_scale_odd(uint32_t* src, uint32_t* dest, int width, int height, int mult)
{
	int x, y;
	uint32_t* s;
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
pointy_scale5(uint32_t* src, uint32_t* dest, int width, int height)
{
	int x, y;
	uint32_t* s;
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
pointy_scale3(uint32_t* src, uint32_t* dest, int width, int height)
{
	int x, y;
	uint32_t* s;
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

static __attribute__((always_inline))
uint32_t blend(const uint32_t c1, const uint32_t c2, const double weight)
{
	uint8_t yuv1[4];
	uint8_t yuv2[4];
	int y, u, v;
	const double iw = 1.0 - weight;

	*(uint32_t *)yuv1 = r2y[c1];
	*(uint32_t *)yuv2 = r2y[c2];
#ifdef __BIG_ENDIAN__
	y = yuv1[1] * iw + yuv2[1] * weight;
	u = yuv1[2] * iw + yuv2[2] * weight;
	v = yuv1[3] * iw + yuv2[3] * weight;
#else
	y = yuv1[2] * iw + yuv2[2] * weight;
	u = yuv1[1] * iw + yuv2[1] * weight;
	v = yuv1[0] * iw + yuv2[0] * weight;
#endif
	CLAMP(y);
	CLAMP(u);
	CLAMP(v);

	return y2r[(y<<16)|(u<<8)|v];
}

/*
 * This does non-integer *width* scaling.  It does not scale in the other
 * direction.  This does the interpolation using Y'UV to prevent dimming of
 * pixels.
 */
static void
interpolate_width(uint32_t* src, uint32_t* dst, int width, int height, int newwidth)
{
	int x, y;
	const double mult = (double)width / newwidth;

	for (y = 0; y < height; y++) {
		for (x = 0; x < newwidth; x++) {
			// First, calculate which two pixels this is between.
			const double xpos = mult * x;
			const int xposi = xpos;
			if (x == xpos) {
				// Exact match!
				*dst = src[width * y + x];
			}
			else {
				const double weight = xpos - xposi;
				// Now pick the two pixels
				const uint32_t pix1 = src[y * width + xposi] & 0xffffff;
				uint32_t pix2;
				if (xposi < width - 1)
					pix2 = src[y * width + xposi + 1] & 0xffffff;
				else
					pix2 = src[y * width + xposi] & 0xffffff;
				if (pix1 == pix2)
					*dst = pix1;
				else {
					*dst = blend(pix1, pix2, weight);
				}
			}
			dst++;
		}
	}
}

/*
 * This does non-integer *height* scaling.  It does not scale in the other
 * direction.  This does the interpolation using Y'UV to prevent dimming of
 * pixels.
 */
static void
interpolate_height(uint32_t* src, uint32_t* dst, int width, int height, int newheight)
{
	int x, y;
	const double mult = (double)height / newheight;

	for (y = 0; y < newheight; y++) {
		for (x = 0; x < width; x++) {
			// First, calculate which two pixels this is between.
			const double ypos = mult * y;
			const int yposi = ypos;
			if (y == ypos) {
				// Exact match!
				*dst = src[width * yposi + x];
			}
			else {
				const double weight = ypos - yposi;
				// Now pick the two pixels
				const uint32_t pix1 = src[yposi * width + x] & 0xffffff;
				uint32_t pix2;
				if (yposi < height - 1)
					pix2 = src[(yposi + 1) * width + x] & 0xffffff;
				else
					pix2 = src[yposi * width + x] & 0xffffff;
				if (pix1 == pix2)
					*dst = pix1;
				else {
					*dst = blend(pix1, pix2, weight);
				}
			}
			dst++;
		}
	}
}

static void
multiply_scale(uint32_t* src, uint32_t* dst, int width, int height, int xmult, int ymult)
{
	int nheight = height * ymult;
	int nwidth = width * xmult;
	int x, y;
	int mx, my;
	uint32_t* slstart;

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
