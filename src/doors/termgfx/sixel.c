// DECSIXEL encoder for syncdoom's "sixel" render tier. Doom renders from a fixed
// 256-color palette, so we feed the encoder the native 8-bit indices plus that
// palette and map sixel color registers 1:1 to indices (register N == Doom color
// N). The palette is therefore identical every frame and is (re)defined only when
// Doom actually changes it (see emit_palette) -- not redefined on every draw.
//
// Sixel layout: ESC P q  "1;1;W;H  [#i;2;r;g;b...]  <bands>  ST.
// A band is 6 pixel rows; within a band each color register that appears is
// written as a full-width run of sixel bytes (0x3F + 6-bit column mask), with
// '$' (graphics CR) overlaying the next color on the same band and '-' (graphics
// NL) advancing to the next band. Runs are RLE-compressed (!count char).

#include "sixel.h"
#include <stdlib.h>
#include <string.h>

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

size_t sixel_encode(uint8_t **buf, size_t *cap, const uint8_t *idx, int w, int h,
                    const uint8_t *pal, int emit_palette)
{
	return sixel_encode_aspect(buf, cap, idx, w, h, 1, 1, pal, emit_palette);
}

size_t sixel_encode_aspect(uint8_t **buf, size_t *cap, const uint8_t *idx, int w, int h,
                           int pan, int pad, const uint8_t *pal, int emit_palette)
{
	obuf_t  o;
	int     x, y, band, bands, c;
	uint8_t present[256];

	o.b = *buf; o.cap = *cap; o.len = 0;

	// --- header: intro, raster attributes (pixel aspect pan:pad, W x H) ---
	ob_put(&o, "\x1bPq");
	ob_putc(&o, '"'); ob_putn(&o, pan); ob_putc(&o, ';'); ob_putn(&o, pad);
	ob_putc(&o, ';'); ob_putn(&o, w); ob_putc(&o, ';'); ob_putn(&o, h);

	// --- palette: registers map 1:1 to indices; (re)define only when asked ---
	if (emit_palette) {
		for (c = 0; c < 256; c++) {
			ob_putc(&o, '#'); ob_putn(&o, c); ob_put(&o, ";2;");
			ob_putn(&o, (pal[c * 3 + 0] * 100 + 127) / 255); ob_putc(&o, ';');   // 0..100%
			ob_putn(&o, (pal[c * 3 + 1] * 100 + 127) / 255); ob_putc(&o, ';');
			ob_putn(&o, (pal[c * 3 + 2] * 100 + 127) / 255);
		}
	}

	// --- bands of 6 rows ---
	bands = (h + 5) / 6;
	for (band = 0; band < bands; band++) {
		int y0 = band * 6;
		int rows = (h - y0 < 6) ? (h - y0) : 6;
		int first = 1;

		memset(present, 0, sizeof(present));
		for (y = y0; y < y0 + rows; y++) {
			const uint8_t *ri = idx + (size_t)y * w;
			for (x = 0; x < w; x++) present[ri[x]] = 1;
		}
		for (c = 0; c < 256; c++) {
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
					if (idx[(size_t)(y0 + r) * w + x] == c)
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
