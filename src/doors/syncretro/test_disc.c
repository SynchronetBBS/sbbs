/* test_disc.c -- the Intellivision's disc and keypad as a KEYBOARD player sees
 * them, driven through the door's real input path (SyncTERM physical-key
 * reports in, RetroPad values out) and decoded with the CORE's own arithmetic.
 *
 * Two claims. First, the numeric keypad is the Intellivision keypad BY
 * POSITION, not by printed digit: the PC numpad has 7 8 9 across the top and
 * the Intellivision has 1 2 3 there, so a cartridge that reads the keypad as a
 * direction (Tron Deadly Discs aims the throw by key position) must see the
 * top-left numpad key as top-left. The number row has no shape and stays by
 * digit.
 *
 * Second, the disc is a 16-position ring and four arrow keys reach only eight
 * of them through the d-pad. Shift turns the held direction one detent
 * clockwise, on the left analog stick, which FreeIntv reads as the disc; while
 * that stick speaks, the d-pad bits for that controller are withheld, because
 * the core ORs both into the same controller word.
 *
 * core16() / core_digit() below are line-for-line copies of FreeIntv
 * controller.c's analog blocks; if upstream changes that math, this fails and
 * the vectors need redoing. Only the platform read seam is stubbed.
 *
 * Copyright(C) 2026 Rob Swindell / SyncRetro.  GPL-2.0.
 */
#include "syncretro.h"
#include "syncretro_binds.h"
#include "syncretro_games.h"
#include "syncretro_profile.h"
#include "syncretro_plat.h"
#include "libretro.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int failures;

#define CHECK(cond) \
		do { \
			if (!(cond)) { \
				printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
				failures++; \
			} \
		} while (0)

/* --- the stubbed seam (see test_axis.c) ----------------------------------- */
static const char *g_feed;

int sr_plat_read(int fd, void *buf, size_t len)
{
	size_t n;

	(void)fd;
	if (g_feed == NULL || *g_feed == '\0')
		return SR_IO_AGAIN;
	n = strlen(g_feed);
	if (n > len)
		n = len;
	memcpy(buf, g_feed, n);
	g_feed = NULL;
	return (int)n;
}

static void type(const char *keys)
{
	g_feed = keys;
	sr_input_pump();
}

int  sr_io_in_fd(void)                      { return 3; }
int  sr_io_out_flush(void)                  { return 0; }
void sr_io_set_canvas(int w, int h)         { (void)w; (void)h; }
void sr_io_set_gfx_canvas(int w, int h)     { (void)w; (void)h; }
void sr_io_set_grid(int c, int r)           { (void)c; (void)r; }
void sr_io_set_cterm_ver(int ver)           { (void)ver; }
void sr_io_set_sdm_probed(int probed)       { (void)probed; }
void sr_io_note_syncterm(void)              { }
int  sr_io_take_grid_probe(void)            { return 0; }
void sr_io_pace_ack(void)                   { }
void sr_out_put(const void *buf, size_t n)  { (void)buf; (void)n; }
int  sr_door_idle_wake(void)                { return 0; }
void sr_door_carrier_lost(void)             { }
void sr_audio_caps(int a, int b)            { (void)a; (void)b; }
void sr_audio_set_blob_ok(int on)           { (void)on; }
void sr_audio_underrun(void)                { }
static int      g_rotate; /* a [disc] rotate cart is loaded */
int  sr_config_disc_rotate(void)            { return g_rotate; }
const char *sr_config_launch_dir(void)      { return "."; }

static uint32_t g_now_ms = 1000;
uint32_t sr_plat_now_ms(void)               { return g_now_ms; }

/* --- the core's arithmetic ------------------------------------------------- */

/* FreeIntv controller.c, the left-analog block: 16-way disc, 0 = North,
 * clockwise. -1 for a centred stick. */
static int core16(int16_t ax, int16_t ay)
{
	int    Lx = ax / 8192;
	int    Ly = ay / 8192;
	double theta;
	int    norm;

	if (Lx == 0 && Ly == 0)
		return -1;
	theta = atan2((double)Ly, (double)Lx) + M_PI;
	if (theta < 0.0)
		theta = 0.0;
	norm = (int)floor((theta / (2 * M_PI)) * 15.0);
	norm -= 3;
	if (norm < 0)
		norm += 16;
	return norm & 0x0F;
}

/* The right-analog block: keypad digit, 0 for a centred stick. */
static const int kd[8] = { 2, 3, 6, 9, 8, 7, 4, 1 };

static int core_digit(int16_t ax, int16_t ay)
{
	int    Rx = ax / 8192;
	int    Ry = ay / 8192;
	double theta;
	int    norm;

	if (Rx == 0 && Ry == 0)
		return 0;
	theta = atan2((double)Ry, (double)Rx) + M_PI;
	if (theta < 0.0)
		theta = 0.0;
	norm = (int)floor((theta / (2 * M_PI)) * 7.0);
	norm -= 1;
	if (norm < 0)
		norm += 8;
	return kd[norm & 0x07];
}

static int disc(unsigned port)
{
	return core16(sr_pad_get(port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT,
	                         RETRO_DEVICE_ID_ANALOG_X),
	              sr_pad_get(port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT,
	                         RETRO_DEVICE_ID_ANALOG_Y));
}

static int digit(unsigned port)
{
	return core_digit(sr_pad_get(port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT,
	                             RETRO_DEVICE_ID_ANALOG_X),
	                  sr_pad_get(port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT,
	                             RETRO_DEVICE_ID_ANALOG_Y));
}

static int button(unsigned port, unsigned id)
{
	return sr_pad_get(port, RETRO_DEVICE_JOYPAD, 0, id);
}

/* --- SyncTERM physical-key reports ----------------------------------------- */

#define KEY_LSHIFT 42
#define KEY_RSHIFT 54
#define KEY_KP7    71
#define KEY_KP8    72
#define KEY_KP9    73
#define KEY_KP4    75
#define KEY_KP6    77
#define KEY_KP1    79
#define KEY_KP2    80
#define KEY_KP3    81
#define KEY_UP     103
#define KEY_LEFT   105
#define KEY_RIGHT  106
#define KEY_DOWN   108
#define KEY_W      17

static void press(int code)
{
	char seq[16];

	snprintf(seq, sizeof seq, "\x1b[=%dK", code);
	type(seq);
}

static void release(int code)
{
	char seq[16];

	snprintf(seq, sizeof seq, "\x1b[=%dk", code);
	type(seq);
}

#define UP    RETRO_DEVICE_ID_JOYPAD_UP
#define DOWN  RETRO_DEVICE_ID_JOYPAD_DOWN
#define LEFT  RETRO_DEVICE_ID_JOYPAD_LEFT
#define RIGHT RETRO_DEVICE_ID_JOYPAD_RIGHT

/* One row of the 16-way table: the arrows held, the d-pad detent they make on
 * their own, and the detent Shift turns that into. */
static const struct {
	int a, b;        /* evdev codes, b = 0 for a single arrow */
	int plain;       /* 16-way index the d-pad bits mean, 0 = N clockwise */
} g_dirs[8] = {
	{ KEY_UP,    0,         0  },   /* N  -> NNE */
	{ KEY_UP,    KEY_RIGHT, 2  },   /* NE -> ENE */
	{ KEY_RIGHT, 0,         4  },   /* E  -> ESE */
	{ KEY_DOWN,  KEY_RIGHT, 6  },   /* SE -> SSE */
	{ KEY_DOWN,  0,         8  },   /* S  -> SSW */
	{ KEY_DOWN,  KEY_LEFT,  10 },   /* SW -> WSW */
	{ KEY_LEFT,  0,         12 },   /* W  -> WNW */
	{ KEY_UP,    KEY_LEFT,  14 }    /* NW -> NNW */
};

static int held(int code)
{
	switch (code) {
		case KEY_UP:    return button(1, UP);
		case KEY_DOWN:  return button(1, DOWN);
		case KEY_LEFT:  return button(1, LEFT);
		case KEY_RIGHT: return button(1, RIGHT);
	}
	return 0;
}

int main(void)
{
	int i;

	sr_profile_select("intv", NULL);

	/* The number row has no shape: a typed 7 is keypad 7. Byte path, so a tap --
	 * and before the native path is up, which ignores bare bytes. */
	type("7");
	CHECK(digit(0) == 7);
	CHECK(digit(1) == 0);
	g_now_ms += 1000;
	sr_input_pump();
	CHECK(digit(0) == 0);

	/* SyncTERM's CTDA reply, cap 8: the door enables physical-key reports. The
	 * enable-time settle window then drops press edges; step the clock past it. */
	type("\x1b[<0;8c");
	g_now_ms += 1000;
	sr_input_pump();

	/* --- the numeric keypad, by POSITION ------------------------------------ */

	/* Top row of the numpad is the top row of the Intellivision keypad. */
	press(KEY_KP7);   CHECK(digit(1) == 1);   release(KEY_KP7);
	press(KEY_KP8);   CHECK(digit(1) == 2);   release(KEY_KP8);
	press(KEY_KP9);   CHECK(digit(1) == 3);   release(KEY_KP9);
	/* The middle row has nowhere to go. */
	press(KEY_KP4);   CHECK(digit(1) == 4);   release(KEY_KP4);
	press(KEY_KP6);   CHECK(digit(1) == 6);   release(KEY_KP6);
	/* Bottom row is the bottom row. */
	press(KEY_KP1);   CHECK(digit(1) == 7);   release(KEY_KP1);
	press(KEY_KP2);   CHECK(digit(1) == 8);   release(KEY_KP2);
	press(KEY_KP3);   CHECK(digit(1) == 9);   release(KEY_KP3);
	CHECK(digit(1) == 0);   /* every release landed: nothing left held */
	CHECK(digit(0) == 0);   /* and player 1's keypad never saw any of it */

	/* --- the disc: eight from the arrows, sixteen with Shift ----------------- */

	/* Arrows alone are d-pad bits and the stick stays centred, exactly as before
	 * any of this existed. */
	press(KEY_UP);
	CHECK(button(1, UP) == 1);
	CHECK(disc(1) == -1);

	/* Shift: the held direction turns one detent clockwise, on the stick, and
	 * the d-pad bit is withheld so the core does not OR N into NNE. */
	press(KEY_LSHIFT);
	CHECK(disc(1) == 1);
	CHECK(button(1, UP) == 0);

	/* Add Left: NW, which Shift turns into NNW. */
	press(KEY_LEFT);
	CHECK(disc(1) == 15);
	CHECK(button(1, UP) == 0);
	CHECK(button(1, LEFT) == 0);

	/* Shift comes back up: the stick centres, the bits reappear, the keys are
	 * still held. */
	release(KEY_LSHIFT);
	CHECK(disc(1) == -1);
	CHECK(button(1, UP) == 1);
	CHECK(button(1, LEFT) == 1);
	release(KEY_UP);
	release(KEY_LEFT);
	CHECK(button(1, UP) == 0);
	CHECK(button(1, LEFT) == 0);

	/* Every one of the eight d-pad directions, plain and shifted. */
	for (i = 0; i < 8; i++) {
		press(g_dirs[i].a);
		if (g_dirs[i].b)
			press(g_dirs[i].b);
		CHECK(disc(1) == -1);
		CHECK(held(g_dirs[i].a) == 1);
		press(KEY_RSHIFT);   /* either Shift key */
		if (disc(1) != g_dirs[i].plain + 1)
			printf("  dir %d: shifted detent %d, wanted %d\n", i, disc(1), g_dirs[i].plain + 1);
		CHECK(disc(1) == g_dirs[i].plain + 1);
		CHECK(held(g_dirs[i].a) == 0);
		if (g_dirs[i].b)
			CHECK(held(g_dirs[i].b) == 0);
		release(KEY_RSHIFT);
		CHECK(disc(1) == -1);
		release(g_dirs[i].a);
		if (g_dirs[i].b)
			release(g_dirs[i].b);
	}

	/* Shift with no direction held is nothing at all. */
	press(KEY_LSHIFT);
	CHECK(disc(1) == -1);
	CHECK(disc(0) == -1);
	CHECK(button(1, UP) == 0);

	/* Opposites cancel: Up+Down under Shift has no direction to turn, so the
	 * stick stays centred and the bits pass through untouched. */
	press(KEY_UP);
	press(KEY_DOWN);
	CHECK(disc(1) == -1);
	CHECK(button(1, UP) == 1);
	CHECK(button(1, DOWN) == 1);
	release(KEY_UP);
	release(KEY_DOWN);

	/* Player 1's W is the same disc on the other controller. evdev reports the
	 * physical key, so Shift-W arrives as the W keycode with Shift held. */
	press(KEY_W);
	CHECK(disc(0) == 1);
	CHECK(button(0, UP) == 0);
	CHECK(disc(1) == -1);
	release(KEY_LSHIFT);
	CHECK(disc(0) == -1);
	CHECK(button(0, UP) == 1);
	release(KEY_W);
	CHECK(button(0, UP) == 0);

	/* --- the paddle sweep rides the same detents ------------------------------ */

	/* On a [disc] rotate cart a held Right alternates E and ESE, a held Left W
	 * and WNW: two adjacent detents whose horizontal component agrees, so the
	 * paddle steps on every change. Each frame's pump is the core's input poll. */
	g_rotate = 1;
	{
		int seen[16] = { 0 }, f;

		press(KEY_RIGHT);
		for (f = 0; f < 12; f++) {
			sr_input_pump();
			if (disc(1) >= 0)
				seen[disc(1)]++;
			CHECK(disc(1) == 4 || disc(1) == 5);
		}
		CHECK(seen[4] > 0 && seen[5] > 0);
		release(KEY_RIGHT);
		memset(seen, 0, sizeof seen);
		press(KEY_LEFT);
		for (f = 0; f < 12; f++) {
			sr_input_pump();
			if (disc(1) >= 0)
				seen[disc(1)]++;
			CHECK(disc(1) == 12 || disc(1) == 13);
		}
		CHECK(seen[12] > 0 && seen[13] > 0);
		release(KEY_LEFT);
		sr_input_pump();
		CHECK(disc(1) == -1);
	}
	g_rotate = 0;

	/* --- a gamepad console has no disc ---------------------------------------- */

	/* Shift must not deflect a stick the console never reads: the arrows are a
	 * d-pad on port 0 and stay one. */
	sr_profile_select("pad", NULL);
	press(KEY_UP);
	press(KEY_LSHIFT);
	CHECK(button(0, UP) == 1);
	CHECK(sr_pad_get(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT,
	                 RETRO_DEVICE_ID_ANALOG_X) == 0);
	CHECK(sr_pad_get(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT,
	                 RETRO_DEVICE_ID_ANALOG_Y) == 0);
	release(KEY_LSHIFT);
	release(KEY_UP);

	/* ...and its numpad is a second d-pad, by position, as it always was. */
	press(KEY_KP8);
	CHECK(button(0, UP) == 1);
	release(KEY_KP8);
	CHECK(button(0, UP) == 0);

	printf("%s: %d failure(s)\n", failures ? "FAIL" : "ok", failures);
	return failures != 0;
}
