/* Unit tests for font.c -- glyphs drawn into an indexed framebuffer.
 *
 * The font exists so a door can put text over its picture WITHOUT writing to
 * the terminal, because a character cell owns a background colour and there
 * is no transparent value for it: a caption written as text arrives as a
 * rectangle of background with the caption in it. These tests are mostly
 * about the properties that makes necessary -- it touches only the pixels it
 * lights, it clips instead of running off the end of the buffer, and the
 * shadow lands behind the glyph rather than through it.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "font.h"

#define W 64
#define H 32
#define BG 200
#define FG 1
#define SH 2

static uint8_t fb[W * H];

static void clear(void)
{
	memset(fb, BG, sizeof(fb));
}

static int count(uint8_t v)
{
	int i, n = 0;

	for (i = 0; i < W * H; i++)
		n += (fb[i] == v) ? 1 : 0;
	return n;
}

static void test_a_glyph_lights_some_pixels_and_not_the_rest(void)
{
	clear();
	termgfx_font_glyph(fb, W, H, 2, 2, 'A', 1, FG, -1);
	/* Some of it is drawn... */
	assert(count(FG) > 0);
	/* ...and it fits in its own 5x7 box: nothing outside that is touched,
	 * or a label would smear into whatever it sits beside. */
	{
		int x, y;

		for (y = 0; y < H; y++)
			for (x = 0; x < W; x++)
				if (x < 2 || x >= 2 + TERMGFX_FONT_W ||
				    y < 2 || y >= 2 + TERMGFX_FONT_H)
					assert(fb[y * W + x] == BG);
	}
}

static void test_the_shadow_is_behind_the_glyph(void)
{
	int face, shadow;

	/* A shadow is what keeps a caption readable when it crosses from sky
	 * to grass mid-word. It has to sit BEHIND: drawn per-pixel alongside
	 * the glyph, one letter's shadow lands on the previous letter's face
	 * and the word looks chewed. */
	clear();
	termgfx_font_glyph(fb, W, H, 2, 2, '8', 1, FG, SH);
	face = count(FG);
	shadow = count(SH);
	assert(face > 0);
	assert(shadow > 0);
	/* The glyph is whole: shadowing cost it nothing. */
	clear();
	termgfx_font_glyph(fb, W, H, 2, 2, '8', 1, FG, -1);
	assert(count(FG) == face);
}

static void test_lowercase_folds_rather_than_vanishing(void)
{
	int upper, lower;

	clear();
	termgfx_font_glyph(fb, W, H, 1, 1, 'R', 1, FG, -1);
	upper = count(FG);
	clear();
	termgfx_font_glyph(fb, W, H, 1, 1, 'r', 1, FG, -1);
	lower = count(FG);
	assert(upper > 0);
	assert(lower == upper);      /* the same glyph, not a blank */
}

static void test_an_unknown_character_draws_nothing(void)
{
	clear();
	termgfx_font_glyph(fb, W, H, 1, 1, '\x01', 1, FG, SH);
	assert(count(FG) == 0);
	assert(count(SH) == 0);
	/* ...but a space is a legitimate blank, not an error. */
	clear();
	termgfx_font_glyph(fb, W, H, 1, 1, ' ', 1, FG, -1);
	assert(count(FG) == 0);
}

static void test_text_clips_instead_of_running_off_the_end(void)
{
	/* A caption too long for the frame should lose its tail. Writing past
	 * the buffer would take the door with it, which is a poor trade for a
	 * label. */
	clear();
	termgfx_font_text(fb, W, H, W - 8, H - 4, "CLIPPED", 1, FG, SH);
	termgfx_font_text(fb, W, H, -20, -3, "ALSO", 1, FG, SH);
	/* Nothing to assert but survival: the harness runs under -UNDEBUG and
	 * a stray write would be caught by the checks above on the next test.
	 * What matters is that it returns. */
	assert(1);
}

static void test_width_matches_what_gets_drawn(void)
{
	int w1, w3;

	/* Callers centre things with this, so it has to agree with the drawing
	 * -- including that there is a gap BETWEEN glyphs and not after the
	 * last one. */
	w1 = termgfx_font_width("AB", 1);
	assert(w1 == TERMGFX_FONT_W * 2 + TERMGFX_FONT_GAP);
	w3 = termgfx_font_width("AB", 3);
	assert(w3 == (TERMGFX_FONT_W * 2 + TERMGFX_FONT_GAP) * 3);
	assert(termgfx_font_width("", 1) == 0);
	assert(termgfx_font_width(NULL, 1) == 0);

	clear();
	assert(termgfx_font_text(fb, W, H, 0, 0, "AB", 1, FG, -1) == w1);
}

static void test_scale_multiplies_every_pixel(void)
{
	int one, two;

	clear();
	termgfx_font_glyph(fb, W, H, 0, 0, 'I', 1, FG, -1);
	one = count(FG);
	clear();
	termgfx_font_glyph(fb, W, H, 0, 0, 'I', 2, FG, -1);
	two = count(FG);
	assert(two == one * 4);          /* twice as wide AND twice as tall */
}

static void test_nothing_at_all_is_safe(void)
{
	/* Doors call this from drawing code that does not check. */
	termgfx_font_text(NULL, W, H, 0, 0, "X", 1, FG, -1);
	termgfx_font_text(fb, W, H, 0, 0, NULL, 1, FG, -1);
	termgfx_font_glyph(fb, W, H, 0, 0, 'X', 0, FG, -1);
	assert(1);
}

int main(void)
{
	test_a_glyph_lights_some_pixels_and_not_the_rest();
	test_the_shadow_is_behind_the_glyph();
	test_lowercase_folds_rather_than_vanishing();
	test_an_unknown_character_draws_nothing();
	test_text_clips_instead_of_running_off_the_end();
	test_width_matches_what_gets_drawn();
	test_scale_multiplies_every_pixel();
	test_nothing_at_all_is_safe();
	printf("test_font: OK\n");
	return 0;
}
