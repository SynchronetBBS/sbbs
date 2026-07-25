/* test_quant.c -- a sixel colour REGISTER must keep its meaning between frames.
 *
 * SyncRetro asks the terminal for SHARED colour registers (`CSI ? 1070 l`), so
 * redefining register k instantly recolours every pixel already on screen that
 * was drawn with k. The palette is emitted BEFORE the new pixel data, so any
 * register whose meaning changes paints the old image in the wrong colour for
 * one frame. Do that a dozen times a second -- 4-TRIS's title animation adds and
 * drops colours at ~12 Hz -- and the screen flickers.
 *
 * Hence the invariant these tests pin: a colour that survives from one frame to
 * the next keeps its register, no matter what else joins or leaves the frame.
 */
#include "syncretro_quant.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond) \
	do { \
		if (!(cond)) { \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			failures++; \
		} \
	} while (0)

#define BLACK 0x00, 0x00, 0x00
#define RED   0xff, 0x00, 0x00
#define GREEN 0x00, 0xff, 0x00
#define BLUE  0x00, 0x00, 0xff
#define WHITE 0xff, 0xff, 0xff

/* Register holding colour (r,g,b), or -1. Only entry 0 may legitimately be
 * black, so scan every entry and let the caller reason about duplicates. */
static int reg_of(const uint8_t *pal, int r, int g, int b, int used)
{
	int k;

	for (k = 0; k < used; k++)
		if (pal[3 * k] == r && pal[3 * k + 1] == g && pal[3 * k + 2] == b)
			return k;
	return -1;
}

int main(void)
{
	/* 2x2 frames. Frame A has 3 colours; frame B adds BLUE, which sorts BETWEEN
	 * black and green -- so a palette sorted by colour value would shove green
	 * and red up a register each. That is the flicker. */
	static const uint8_t frame_a[] = { BLACK, RED, GREEN, BLACK };
	static const uint8_t frame_b[] = { BLACK, RED, GREEN, BLUE  };
	static const uint8_t frame_c[] = { BLACK, RED, BLACK, BLACK };   /* colours leave */
	static const uint8_t frame_d[] = { BLACK, RED, GREEN, BLUE  };   /* and come back */
	uint8_t idx[4], pal[768], pal_a[768], pal_b[768];
	int     r_black, r_red, r_green, r_blue;

	sr_quant_reset();

	/* Frame A: three colours, three registers. */
	CHECK(sr_quant_rgb_to_indexed(frame_a, 2, 2, idx, pal) == 1);
	memcpy(pal_a, pal, sizeof pal);
	r_black = reg_of(pal, BLACK, 256);
	r_red   = reg_of(pal, RED, 256);
	r_green = reg_of(pal, GREEN, 256);
	CHECK(r_black >= 0 && r_red >= 0 && r_green >= 0);
	CHECK(r_black != r_red && r_red != r_green && r_black != r_green);
	/* The index plane must agree with the palette. */
	CHECK(idx[0] == r_black && idx[1] == r_red && idx[2] == r_green && idx[3] == r_black);

	/* Frame B adds BLUE. THE INVARIANT: every colour carried over from frame A
	 * keeps the register it already had. Under a sorted palette this fails --
	 * BLUE sorts before GREEN and displaces it. */
	CHECK(sr_quant_rgb_to_indexed(frame_b, 2, 2, idx, pal) == 1);
	memcpy(pal_b, pal, sizeof pal);
	CHECK(reg_of(pal, BLACK, 256) == r_black);
	CHECK(reg_of(pal, RED, 256)   == r_red);
	CHECK(reg_of(pal, GREEN, 256) == r_green);
	r_blue = reg_of(pal, BLUE, 256);
	CHECK(r_blue >= 0);
	CHECK(r_blue != r_black && r_blue != r_red && r_blue != r_green);
	CHECK(idx[3] == r_blue);

	/* The registers frame A defined are byte-identical in frame B's palette, so
	 * the present path re-sends only the newly defined one -- or, since it
	 * compares the whole table, one small palette. What it must NEVER do is
	 * redefine a register that frame A's pixels are still drawn with. */
	CHECK(memcmp(pal_a + 3 * r_black, pal_b + 3 * r_black, 3) == 0);
	CHECK(memcmp(pal_a + 3 * r_red,   pal_b + 3 * r_red,   3) == 0);
	CHECK(memcmp(pal_a + 3 * r_green, pal_b + 3 * r_green, 3) == 0);

	/* Frame C drops GREEN and BLUE entirely. Their registers must NOT be handed
	 * to anyone else while the session lives -- reassigning a register is the
	 * whole bug, and a colour that vanished this frame may return next frame. */
	CHECK(sr_quant_rgb_to_indexed(frame_c, 2, 2, idx, pal) == 1);
	CHECK(reg_of(pal, BLACK, 256) == r_black);
	CHECK(reg_of(pal, RED, 256)   == r_red);
	CHECK(idx[0] == r_black && idx[1] == r_red);

	/* Frame D brings them back. They must land on the registers they held. */
	CHECK(sr_quant_rgb_to_indexed(frame_d, 2, 2, idx, pal) == 1);
	CHECK(reg_of(pal, GREEN, 256) == r_green);
	CHECK(reg_of(pal, BLUE, 256)  == r_blue);
	CHECK(idx[2] == r_green && idx[3] == r_blue);

	/* A reset forgets everything: a fresh session starts from register 0. */
	sr_quant_reset();
	CHECK(sr_quant_rgb_to_indexed(frame_c, 2, 2, idx, pal) == 1);
	CHECK(reg_of(pal, BLACK, 256) == 0);
	CHECK(reg_of(pal, RED, 256) == 1);

	/* An all-one-colour frame still reproduces exactly. */
	sr_quant_reset();
	{
		static const uint8_t solid[] = { WHITE, WHITE, WHITE, WHITE };

		CHECK(sr_quant_rgb_to_indexed(solid, 2, 2, idx, pal) == 1);
		CHECK(reg_of(pal, WHITE, 256) == 0);
		CHECK(idx[0] == 0 && idx[3] == 0);
	}

	/* --- a FULL register map ------------------------------------------------
	 *
	 * Street Fighter II's attract-mode fades re-tint every colour every frame,
	 * so the map saturates within seconds and every frame after that needs
	 * registers it cannot get. What happens then decides whether the screen
	 * flickers: taking the registers the OLDEST frames used costs nothing,
	 * because nothing on screen is drawn with them any more. */
	{
		uint8_t full[256 * 3];        /* 256 distinct colours, none pure black */
		uint8_t fidx[256];
		uint8_t pal_full[768], pal_evict[768], pal_third[768];
		uint8_t few[4 * 3];
		int     i, changed, r10, r11, r12, r_new, r_newer;

		sr_quant_reset();
		for (i = 0; i < 256; i++) {
			full[i * 3]     = (uint8_t)i;
			full[i * 3 + 1] = 1;
			full[i * 3 + 2] = 2;
		}
		CHECK(sr_quant_rgb_to_indexed(full, 16, 16, fidx, pal) == 1);
		memcpy(pal_full, pal, sizeof pal);

		/* Every register is now taken. This frame keeps three of the colours and
		 * introduces one more, so exactly one register has to be taken away. */
		memcpy(few + 0, full + 10 * 3, 3);
		memcpy(few + 3, full + 11 * 3, 3);
		memcpy(few + 6, full + 12 * 3, 3);
		few[9] = 0; few[10] = 0xff; few[11] = 0;                /* GREEN, new */

		CHECK(sr_quant_rgb_to_indexed(few, 2, 2, idx, pal) == 1);
		memcpy(pal_evict, pal, sizeof pal);

		r10 = reg_of(pal, full[30], full[31], full[32], 256);
		r11 = reg_of(pal, full[33], full[34], full[35], 256);
		r12 = reg_of(pal, full[36], full[37], full[38], 256);
		CHECK(r10 >= 0 && r11 >= 0 && r12 >= 0);
		CHECK(idx[0] == r10 && idx[1] == r11 && idx[2] == r12);

		/* A carried-over colour keeps its register even when the map is full. */
		CHECK(reg_of(pal_full, full[30], full[31], full[32], 256) == r10);
		CHECK(reg_of(pal_full, full[33], full[34], full[35], 256) == r11);
		CHECK(reg_of(pal_full, full[36], full[37], full[38], 256) == r12);

		/* ONE register redefined, not all 256. Rebuilding the map from this frame
		 * -- what this module used to do when it ran out -- recolours everything
		 * the terminal is still holding, which IS the flicker. */
		changed = 0;
		for (i = 0; i < 256; i++)
			if (memcmp(pal_full + 3 * i, pal_evict + 3 * i, 3) != 0)
				changed++;
		CHECK(changed == 1);

		r_new = reg_of(pal, GREEN, 256);
		CHECK(r_new >= 0);
		CHECK(r_new != r10 && r_new != r11 && r_new != r12);

		/* Another new colour, with the map still full. The registers the PREVIOUS
		 * frame drew with are the ones the terminal is displaying right now, so
		 * they are the last that may be taken -- an older one always goes first. */
		few[9] = 0; few[10] = 0; few[11] = 0xff;                /* BLUE, new */
		CHECK(sr_quant_rgb_to_indexed(few, 2, 2, idx, pal) == 1);
		memcpy(pal_third, pal, sizeof pal);

		r_newer = reg_of(pal, BLUE, 256);
		CHECK(r_newer >= 0);
		CHECK(r_newer != r10 && r_newer != r11 && r_newer != r12 && r_newer != r_new);
		CHECK(memcmp(pal_evict + 3 * r10,   pal_third + 3 * r10,   3) == 0);
		CHECK(memcmp(pal_evict + 3 * r11,   pal_third + 3 * r11,   3) == 0);
		CHECK(memcmp(pal_evict + 3 * r12,   pal_third + 3 * r12,   3) == 0);
		CHECK(memcmp(pal_evict + 3 * r_new, pal_third + 3 * r_new, 3) == 0);

		/* A frame of 256 colours none of which are mapped has to take every
		 * register there is. It must still reproduce exactly. */
		for (i = 0; i < 256; i++) {
			full[i * 3]     = 3;
			full[i * 3 + 1] = (uint8_t)i;
			full[i * 3 + 2] = 4;
		}
		CHECK(sr_quant_rgb_to_indexed(full, 16, 16, fidx, pal) == 1);
		for (i = 0; i < 256; i++)
			CHECK(pal[fidx[i] * 3] == 3 && pal[fidx[i] * 3 + 1] == (uint8_t)i
			      && pal[fidx[i] * 3 + 2] == 4);
	}

	printf("%s: %d failure(s)\n", failures ? "FAIL" : "ok", failures);
	return failures != 0;
}
