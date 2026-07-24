/* test_axis.c -- a KEY has to be able to deflect an analog stick, because a
 * twin-stick cabinet has a control that lives nowhere else.
 *
 * MAME 2003-Plus binds a cabinet's second stick (Battlezone's right tread,
 * Robotron's aim) to the RetroPad's RIGHT stick only -- not to a button, not to
 * the d-pad, and no core option moves it. Before SR_ACT_AXIS the door had no key
 * that produced an analog value on a cabinet, so half of Battlezone's control
 * panel was unreachable: the tank pivoted on its one live tread and could never
 * drive straight or reverse.
 *
 * test_binds.c pins which KEY resolves to which axis id. This pins the other
 * half -- that the key actually arrives at the core as a deflection -- by
 * driving the door's REAL input path end to end: bytes in through
 * sr_input_pump(), value out through sr_pad_get(), which is exactly what the
 * libretro callbacks do. Only the platform read seam is stubbed.
 *
 * The negative half matters as much as the positive one: every OTHER axis, and
 * every other console, must still read a CENTRED stick. A phantom deflection is
 * how a cabinet that reads both a stick and a d-pad starts drifting on its own.
 *
 * Copyright(C) 2026 Rob Swindell / SyncRetro.  GPL-2.0.
 */
#include "syncretro.h"
#include "syncretro_binds.h"
#include "syncretro_profile.h"
#include "syncretro_plat.h"
#include "libretro.h"

#include <stdint.h>
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

/* --- the stubbed seam ------------------------------------------------------
 * sr_input_pump() reads the player's keystrokes through sr_plat_read() on the
 * descriptor sr_io_in_fd() hands it. Feeding that pair IS feeding the door a
 * keystroke; everything between here and sr_pad_get() is the real code. */
static const char *g_feed;

int sr_plat_read(int fd, void *buf, size_t len)
{
	size_t n;

	(void)fd;
	if (g_feed == NULL || *g_feed == '\0')
		return SR_IO_AGAIN;         /* nothing typed: not a hangup */
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

/* Everything else syncretro_input.c calls, reduced to what a keystroke needs.
 * A stub that lies would make this test lie, so each returns the value that
 * means "nothing happening": no terminal capabilities, no door screen, no
 * clock drift. */
int  sr_io_in_fd(void)                      { return 3; }   /* any live fd */
int  sr_io_out_flush(void)                  { return 0; }
void sr_io_set_canvas(int w, int h)         { (void)w; (void)h; }
void sr_io_set_gfx_canvas(int w, int h)     { (void)w; (void)h; }
void sr_io_set_grid(int c, int r)           { (void)c; (void)r; }
int  sr_io_take_grid_probe(void)            { return 0; }
void sr_io_pace_ack(void)                   { }
void sr_out_put(const void *buf, size_t n)  { (void)buf; (void)n; }
int  sr_door_idle_wake(void)                { return 0; }
void sr_door_carrier_lost(void)             { }
void sr_audio_caps(int a, int b)            { (void)a; (void)b; }
void sr_audio_set_blob_ok(int on)           { (void)on; }
void sr_audio_underrun(void)                { }
int  sr_config_disc_rotate(void)            { return 0; }
const char *sr_config_launch_dir(void)      { return "."; }

/* A monotonic clock the test drives itself: the byte path arms a 250 ms
 * auto-release dwell, and a test that used the wall clock would either race it
 * or have to sleep. Here time only moves when a case says it does. */
static uint32_t g_now_ms = 1000;
uint32_t sr_plat_now_ms(void)               { return g_now_ms; }

int main(void)
{
	/* --- a cabinet ---------------------------------------------------------- */
	sr_profile_select("arcade", NULL);

	/* At rest the stick is centred. This is the state every cabinet whose driver
	 * never reads an axis must see forever. */
	CHECK(sr_pad_get(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT,
	                 RETRO_DEVICE_ID_ANALOG_Y) == 0);

	/* I deflects the right stick UP. Negative, because libretro's Y axis is
	 * DOWN-positive -- and up is the tread going FORWARD, which is the whole
	 * point: it is what Battlezone could not do. */
	type("i");
	CHECK(sr_pad_get(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT,
	                 RETRO_DEVICE_ID_ANALOG_Y) < 0);

	/* ...and it must not leak anywhere else: not onto the other axis of the same
	 * stick, not onto the LEFT stick (which on the Intellivision is the disc),
	 * and above all not as a BUTTON. The axis ids live in pad-array slots above
	 * the last RetroPad id; if sr_pad_get() ever passed one through as a button,
	 * pressing I would press R3 at the core. */
	CHECK(sr_pad_get(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT,
	                 RETRO_DEVICE_ID_ANALOG_X) == 0);
	CHECK(sr_pad_get(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT,
	                 RETRO_DEVICE_ID_ANALOG_Y) == 0);
	CHECK(sr_pad_get(0, RETRO_DEVICE_JOYPAD, 0, SR_AXIS_RIGHT_Y_NEG) == 0);
	CHECK(sr_pad_get(0, RETRO_DEVICE_JOYPAD, 0, SR_AXIS_RIGHT_Y_POS) == 0);
	CHECK(sr_pad_get(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3) == 0);

	/* The dwell expires and the stick centres itself. A tread left running
	 * because a key-up never came is a tank that drives off on its own. */
	g_now_ms += 5000;
	sr_input_pump();
	CHECK(sr_pad_get(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT,
	                 RETRO_DEVICE_ID_ANALOG_Y) == 0);

	/* K deflects the other way: the same tread in reverse. */
	type("k");
	CHECK(sr_pad_get(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT,
	                 RETRO_DEVICE_ID_ANALOG_Y) > 0);

	/* Both at once is a stick pulled two ways -- centred, not one side winning.
	 * On the cabinet it is both tread levers held against each other. */
	type("i");
	CHECK(sr_pad_get(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT,
	                 RETRO_DEVICE_ID_ANALOG_Y) == 0);

	/* The stick is the SECOND one. The d-pad keys keep driving the first, as
	 * buttons, exactly as they did before any of this existed. */
	g_now_ms += 5000;
	sr_input_pump();
	type("w");
	CHECK(sr_pad_get(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP) == 1);
	CHECK(sr_pad_get(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT,
	                 RETRO_DEVICE_ID_ANALOG_Y) == 0);

	/* --- a cartridge console ------------------------------------------------
	 * The other two consoles ship the SAME binary. On a gamepad profile I and K
	 * are unbound and the analog stick is dead, so a core that polls it reads a
	 * centred stick -- byte for byte what it read before the cabinet grew a
	 * second one. */
	g_now_ms += 5000;
	sr_input_pump();
	sr_profile_select("pad", NULL);
	type("i");
	CHECK(sr_pad_get(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT,
	                 RETRO_DEVICE_ID_ANALOG_Y) == 0);
	type("k");
	CHECK(sr_pad_get(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT,
	                 RETRO_DEVICE_ID_ANALOG_Y) == 0);

	printf("%s: %d failure(s)\n", failures ? "FAIL" : "ok", failures);
	return failures != 0;
}
