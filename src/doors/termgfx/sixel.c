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

#include <stdio.h>
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
	// P2=1 (transparent) ensures terminals do not pre-fill or pre-clear the raster
	// (seen in Windows Terminal white-block flashes and Foot cell-clearing bars).
	// P1=0 and P3=0 are defaults; raster attributes override aspect anyway.
	// All termgfx-consuming doors paint every pixel of every declared raster.
	ob_put(&o, "\x1bP0;1;0q");
	ob_putc(&o, '"'); ob_putn(&o, pan); ob_putc(&o, ';'); ob_putn(&o, pad);
	ob_putc(&o, ';'); ob_putn(&o, w); ob_putc(&o, ';'); ob_putn(&o, h);

	// --- palette: registers map 1:1 to indices; define per emit_palette mode.
	// SIXEL_PAL_USED emits only the registers this image references (a whole-
	// image scan), at their original index -- so the band data below, which
	// references original indices, needs no change and the numbering matches
	// FULL exactly. Every referenced index is in `used[]` by construction, so
	// a register-resetting terminal always has every color it draws with. ---
	if (emit_palette != SIXEL_PAL_NONE) {
		uint8_t used[256];

		if (emit_palette == SIXEL_PAL_USED) {
			long i, npx = (long)w * (long)h;

			memset(used, 0, sizeof(used));
			for (i = 0; i < npx; i++)
				used[idx[i]] = 1;
		} else {
			memset(used, 1, sizeof(used));   // SIXEL_PAL_FULL: all 256
		}
		for (c = 0; c < 256; c++) {
			if (!used[c])
				continue;
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

// --- vertical-scaling probe (see sixel.h) ------------------------------------

// A 1px-wide, 36px-tall (6 band) sliver: small, quick, and tall enough that the
// pan=1 and pan=2 copies land on DIFFERENT character rows for every cell height
// the terminals use (8, 14, 16) -- 36px vs 72px is 3-vs-5 rows at 16, 5-vs-9 at 8.
#define VSCALE_BANDS  6

static size_t vscale_sliver(char *p, size_t room, int pan)
{
	size_t n = 0;
	int    i;

	// ESC P 0;1;0 q "pan;1;1;36  #0;2;0;0;0  then one all-six-pixels column per band.
	n += (size_t)snprintf(p + n, room - n, "\x1bP0;1;0q\"%d;1;1;%d#0;2;0;0;0",
	                      pan, VSCALE_BANDS * 6);
	for (i = 0; i < VSCALE_BANDS; i++)
		n += (size_t)snprintf(p + n, room - n, "%s#0~", i ? "-" : "");
	n += (size_t)snprintf(p + n, room - n, "\x1b\\");
	return n;
}

size_t termgfx_sixel_vscale_probe(char *buf, size_t cap)
{
	size_t n = 0;

	if (cap < 128)
		return 0;

	// Sixel scrolling ON (DECSDM reset) or the cursor never moves and the probe
	// would read "does not scale" on a terminal that does.  This is the doors'
	// normal state (termgfx_term_enter), so it is a no-op there -- but the probe
	// must not depend on the caller having got there first.
	n += (size_t)snprintf(buf + n, cap - n, "\x1b[?80l\x1b[H\x1b[6n");
	n += vscale_sliver(buf + n, cap - n, 1);                    // unscaled reference
	n += (size_t)snprintf(buf + n, cap - n, "\x1b[6n");         // row after it
	n += vscale_sliver(buf + n, cap - n, 2);                    // ask for 2x vertical
	n += (size_t)snprintf(buf + n, cap - n, "\x1b[6n\x1b[H");   // row after that; tidy up
	return n;
}

int termgfx_sixel_vscale_parse(const char *acc, size_t len)
{
	int    row[3];
	int    found = 0;
	size_t i = 0;

	// Pick out the first three cursor-position reports: ESC [ <row> ; <col> R.
	while (i + 3 < len && found < 3) {
		if (acc[i] == '\x1b' && acc[i + 1] == '[') {
			size_t j = i + 2;
			int    r = 0, digits = 0;

			while (j < len && acc[j] >= '0' && acc[j] <= '9') {
				r = r * 10 + (acc[j] - '0');
				digits++;
				j++;
			}
			if (digits > 0 && j < len && acc[j] == ';') {
				j++;
				while (j < len && acc[j] >= '0' && acc[j] <= '9')
					j++;
				if (j < len && acc[j] == 'R') {
					row[found++] = r;
					i = j + 1;
					continue;
				}
			}
		}
		i++;
	}
	return termgfx_sixel_vscale_verdict(row, found);
}

int termgfx_sixel_vscale_verdict(const int *rows, int nrows)
{
	if (rows == NULL || nrows < 3)
		return -1;                       // not all three reports are in hand yet

	// The pan=2 copy advanced the cursor further than the pan=1 copy => pan honored.
	return (rows[2] - rows[1]) > (rows[1] - rows[0]) ? 1 : 0;
}
