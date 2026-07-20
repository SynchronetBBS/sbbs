/* test_termgfx_present_rgbx.c -- headless truecolor present via
 * SYNCSCUMM_SIXELOUT capture mode. Feeds known 32-bit R,G,B,X frames through
 * termgfx_termio_present_rgbx() and decodes the captured sixel's palette
 * registers, proving the R,G,B,X byte order (pad byte dropped), the
 * 0-255 -> 0-100 sixel scale conversion, and that the quantizer sees
 * distinct colors. cc'd + run by unit_termgfx_present_rgbx.sh.
 *
 * Uses a native 64x48 frame -- deliberately NOT 320x200 -- to exercise the
 * w/h-parameterized geometry present_rgbx() takes (unlike present(), which
 * is fixed to TERMGFX_TERMIO_FB_W x TERMGFX_TERMIO_FB_H). */
#include "termgfx_termio.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define W 64
#define H 48

/* Read an entire file into a NUL-terminated malloc'd buffer. */
static char *slurp(const char *path)
{
	FILE *f = fopen(path, "rb");
	long  n;
	char *buf;

	assert(f != NULL);
	assert(fseek(f, 0, SEEK_END) == 0);
	n = ftell(f);
	assert(n > 0);
	assert(fseek(f, 0, SEEK_SET) == 0);
	buf = (char *)malloc((size_t)n + 1);
	assert(buf != NULL);
	assert(fread(buf, 1, (size_t)n, f) == (size_t)n);
	buf[n] = '\0';
	fclose(f);
	return buf;
}

/* Sixel's palette registers are on a 0-100 scale, not 0-255 (sixel.c's
 * emit_palette loop: (v*100+127)/255 -- rounded, not truncated). Assert the
 * "#<register>;2;r;g;b" definition for `r,g,b` (0-255, source scale) appears
 * SOMEWHERE in `hay`. Match only the ";2;r;g;b" suffix, not the leading
 * "#<n>": the register number is a quantizer implementation detail (which
 * grid cell sorts first), and every unused register the encoder pads the
 * 256-entry palette dump out with is black (";2;0;0;0"), so the suffix
 * alone can't collide with those. */
static void assert_has_register(const char *hay, int r, int g, int b)
{
	char needle[32];
	int  sr = (r * 100 + 127) / 255;
	int  sg = (g * 100 + 127) / 255;
	int  sb = (b * 100 + 127) / 255;

	snprintf(needle, sizeof needle, ";2;%d;%d;%d", sr, sg, sb);
	assert(strstr(hay, needle) != NULL);
}

/* Fill a W*H XRGB (R,G,B,X, stride 4) frame. If `b_r` < 0 the whole frame is
 * solid (r,g,b); otherwise the left half is (r,g,b) and the right half is
 * (b_r,b_g,b_b). `garbage_x`: 0 => X=255 everywhere, nonzero => X carries a
 * per-pixel value derived from the pixel index (the pad-byte-garbage
 * variant), which must have zero effect on the encoded output. */
static void fill_frame(uint8_t *buf, int r, int g, int b,
                        int b_r, int b_g, int b_b, int garbage_x)
{
	int x, y;

	for (y = 0; y < H; y++) {
		for (x = 0; x < W; x++) {
			int      i     = y * W + x;
			uint8_t *p     = buf + (size_t)i * 4;
			int      right = (b_r >= 0) && (x >= W / 2);

			p[0] = (uint8_t)(right ? b_r : r);
			p[1] = (uint8_t)(right ? b_g : g);
			p[2] = (uint8_t)(right ? b_b : b);
			p[3] = garbage_x ? (uint8_t)(i & 0xff) : 255;
		}
	}
}

int main(void)
{
	char           tmpl[] = "/tmp/sst_rgbx_XXXXXX";
	int            fd;
	static uint8_t frame[W * H * 4];
	char          *argv[2];
	char          *all;
	char          *p1, *p2, *p3, *end;

	fd = mkstemp(tmpl);
	assert(fd >= 0);
	close(fd);
	setenv("SYNCSCUMM_SIXELOUT", tmpl, 1);

	argv[0] = (char *)"test_termgfx_present_rgbx";
	argv[1] = NULL;
	assert(termgfx_termio_init(1, argv) == 1);

	/* (1) solid color: R=200,G=40,B=10, X=255 everywhere. */
	fill_frame(frame, 200, 40, 10, -1, -1, -1, 0);
	termgfx_termio_present_rgbx(frame, W, H);

	/* (2) two colors: left half A=(200,40,10), right half B=(10,40,200). */
	fill_frame(frame, 200, 40, 10, 10, 40, 200, 0);
	termgfx_termio_present_rgbx(frame, W, H);

	/* (3) the same solid frame as (1), but X carries per-pixel garbage
	 * instead of a constant 255 -- must produce byte-identical output. */
	fill_frame(frame, 200, 40, 10, -1, -1, -1, 1);
	termgfx_termio_present_rgbx(frame, W, H);

	termgfx_termio_shutdown();

	/* Each present_rgbx() call in capture mode writes one self-contained
	 * "\x1bP0;1;0q..." sixel block (sixel_encode()'s header, see sixel.c);
	 * split the concatenated 3-frame capture back into its calls by that
	 * fixed introducer so each assertion below reads only its own frame. */
	all = slurp(tmpl);
	unlink(tmpl);

	p1 = strstr(all, "\x1bP0;1;0q");
	assert(p1 != NULL);
	p2 = strstr(p1 + 1, "\x1bP0;1;0q");
	assert(p2 != NULL);
	p3 = strstr(p2 + 1, "\x1bP0;1;0q");
	assert(p3 != NULL);
	end = all + strlen(all);

	/* --- (1) solid: the byte order and scale conversion --- */
	{
		char save = *p2;
		*p2 = '\0';
		assert_has_register(p1, 200, 40, 10);
		*p2 = save;
	}
	printf("PRESENT_RGBX OK (solid)\n");

	/* --- (2) two colors: BOTH registers present, distinct --- */
	{
		char save = *p3;
		*p3 = '\0';
		assert_has_register(p2, 200, 40, 10);
		assert_has_register(p2, 10, 40, 200);
		*p3 = save;
	}
	printf("PRESENT_RGBX OK (twocolor)\n");

	/* --- (3) pad byte ignored: byte-identical to block (1) --- */
	{
		size_t len1 = (size_t)(p2 - p1);
		size_t len3 = (size_t)(end - p3);

		assert(len1 == len3);
		assert(memcmp(p1, p3, len1) == 0);
	}
	printf("PRESENT_RGBX OK (padignored)\n");

	free(all);
	return 0;
}
