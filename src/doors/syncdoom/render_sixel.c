// DECSIXEL encoder for syncdoom's "sixel" render tier. Doom renders from a 256-
// color palette, so every pixel of a frame is one of <=256 colors -- we extract
// exactly those per frame (a hash over the packed RGB) and emit them as sixel
// color registers, giving a pixel-perfect, dither-free image. This is the
// graphics tier for terminals that speak sixel but not SyncTERM's JXL/PPM APC
// (xterm -ti vt340, mlterm, foot, WezTerm, recent Windows Terminal, ...).
//
// Sixel layout we emit: ESC P q  "1;1;W;H  #i;2;r;g;b...  <bands>  then ST.
// A band is 6 pixel rows; within a band each color register that appears is
// written as a full-width run of sixel bytes (0x3F + 6-bit column mask), with
// '$' (graphics CR) overlaying the next color on the same band and '-' (graphics
// NL) advancing to the next band. Runs are RLE-compressed (!count char).

#include "render_sixel.h"
#include <stdlib.h>
#include <string.h>

#define SIXEL_MAXCOL 256
#define SIXEL_HASH   4096        // open-addressing; load factor stays tiny (<=256)

// Growable output buffer wrapping the caller's (buf,cap).
typedef struct { uint8_t *b; size_t cap, len; } obuf_t;

static void ob_ensure(obuf_t *o, size_t extra)
{
	if (o->len + extra <= o->cap)
		return;
	{
		size_t nc = o->cap ? o->cap : 65536;
		while (nc < o->len + extra) nc *= 2;
		o->b = (uint8_t *)realloc(o->b, nc);
		o->cap = nc;
	}
}

static void ob_putc(obuf_t *o, int c)        { ob_ensure(o, 1); o->b[o->len++] = (uint8_t)c; }
static void ob_put(obuf_t *o, const char *s) { size_t n = strlen(s); ob_ensure(o, n); memcpy(o->b + o->len, s, n); o->len += n; }

static void ob_putn(obuf_t *o, int v)        // unsigned decimal
{
	char t[12];
	int  n = 0;
	if (v == 0) { ob_putc(o, '0'); return; }
	while (v > 0) { t[n++] = (char)('0' + v % 10); v /= 10; }
	ob_ensure(o, (size_t)n);
	while (n > 0) o->b[o->len++] = (uint8_t)t[--n];
}

// Emit one RLE run of sixel byte `ch` repeated `len` times.
static void ob_run(obuf_t *o, int ch, int len)
{
	if (len <= 0)
		return;
	if (len > 3) { ob_putc(o, '!'); ob_putn(o, len); ob_putc(o, ch); }
	else         { while (len-- > 0) ob_putc(o, ch); }
}

// Per-frame palette + index buffer (static, reused across frames).
static uint8_t  s_pr[SIXEL_MAXCOL], s_pg[SIXEL_MAXCOL], s_pb[SIXEL_MAXCOL];
static int32_t  s_hidx[SIXEL_HASH];      // palette index for slot, -1 = empty
static uint32_t s_hcol[SIXEL_HASH];      // packed color occupying the slot
static uint8_t *s_idx = NULL;            // w*h palette indices
static size_t   s_idx_cap = 0;

// Nearest existing palette entry (only used if a frame ever exceeds 256 colors,
// which vanilla Doom's paletted output never does -- a safety net, not a path).
static int nearest(int ncol, int r, int g, int b)
{
	int best = 0, bestd = 1 << 30, i;
	for (i = 0; i < ncol; i++) {
		int dr = s_pr[i] - r, dg = s_pg[i] - g, db = s_pb[i] - b;
		int d  = dr * dr + dg * dg + db * db;
		if (d < bestd) { bestd = d; best = i; }
	}
	return best;
}

size_t sixel_encode(uint8_t **buf, size_t *cap, const uint8_t *rgb, int w, int h)
{
	obuf_t         o;
	int            ncol = 0, npix = w * h, p, x, y, band, bands, c;
	const uint8_t *src;
	uint8_t        present[SIXEL_MAXCOL];

	o.b = *buf; o.cap = *cap; o.len = 0;

	if ((size_t)npix > s_idx_cap) { s_idx = (uint8_t *)realloc(s_idx, (size_t)npix); s_idx_cap = (size_t)npix; }

	// --- pass 1: build the exact palette and the per-pixel index buffer ---
	for (p = 0; p < SIXEL_HASH; p++) s_hidx[p] = -1;
	src = rgb;
	for (p = 0; p < npix; p++) {
		int      r = src[0], g = src[1], b = src[2];
		uint32_t col = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
		uint32_t hh  = (col * 2654435761u) & (SIXEL_HASH - 1);
		int      idx;
		src += 3;
		while (s_hidx[hh] >= 0 && s_hcol[hh] != col) hh = (hh + 1) & (SIXEL_HASH - 1);
		if (s_hidx[hh] >= 0) {
			idx = s_hidx[hh];
		} else if (ncol < SIXEL_MAXCOL) {
			idx = ncol++;
			s_pr[idx] = (uint8_t)r; s_pg[idx] = (uint8_t)g; s_pb[idx] = (uint8_t)b;
			s_hcol[hh] = col; s_hidx[hh] = idx;
		} else {
			idx = nearest(ncol, r, g, b);
		}
		s_idx[p] = (uint8_t)idx;
	}

	// --- header: intro, raster attributes (square pixels, W x H), palette ---
	ob_put(&o, "\x1bPq");
	ob_putc(&o, '"'); ob_put(&o, "1;1;"); ob_putn(&o, w); ob_putc(&o, ';'); ob_putn(&o, h);
	for (c = 0; c < ncol; c++) {
		ob_putc(&o, '#'); ob_putn(&o, c); ob_put(&o, ";2;");
		ob_putn(&o, (s_pr[c] * 100 + 127) / 255); ob_putc(&o, ';');   // sixel color = 0..100%
		ob_putn(&o, (s_pg[c] * 100 + 127) / 255); ob_putc(&o, ';');
		ob_putn(&o, (s_pb[c] * 100 + 127) / 255);
	}

	// --- bands of 6 rows ---
	bands = (h + 5) / 6;
	for (band = 0; band < bands; band++) {
		int y0 = band * 6;
		int rows = (h - y0 < 6) ? (h - y0) : 6;
		int first = 1;

		memset(present, 0, (size_t)ncol);
		for (y = y0; y < y0 + rows; y++) {
			const uint8_t *ri = s_idx + (size_t)y * w;
			for (x = 0; x < w; x++) present[ri[x]] = 1;
		}
		for (c = 0; c < ncol; c++) {
			int runch = -1, runlen = 0;
			if (!present[c])
				continue;
			if (!first)
				ob_putc(&o, '$');             // graphics CR: overlay next color on this band
			first = 0;
			ob_putc(&o, '#'); ob_putn(&o, c);
			for (x = 0; x < w; x++) {
				int bits = 0, r;
				for (r = 0; r < rows; r++)
					if (s_idx[(size_t)(y0 + r) * w + x] == c)
						bits |= (1 << r);
				{
					int ch = 0x3F + bits;
					if (ch == runch) { runlen++; }
					else { ob_run(&o, runch, runlen); runch = ch; runlen = 1; }
				}
			}
			if (runch != 0x3F)
				ob_run(&o, runch, runlen);                  // drop a trailing all-empty run
		}
		ob_putc(&o, '-');                     // graphics NL: next band
	}
	ob_put(&o, "\x1b\\");                      // ST

	*buf = o.b; *cap = o.cap;
	return o.len;
}
