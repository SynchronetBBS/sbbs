/* main.c -- SyncRetro frontend entry point.
 *
 * Order (DESIGN.md sec 3):
 *   1. sr_door_setup()          resolve socket / time / name / home / core / rom
 *   2. sr_door_sanitize_argv()  strip the door's own args
 *   3. sr_config_apply()        per-user sandbox + system/save dirs
 *   4. rc_core_open()           dlopen the libretro core
 *   5. sr_io_init()             terminal enter + probe (geometry / caps)
 *   6. rc_core_load_game()      install callbacks, retro_init, load ROM
 *   7. loop: rc_core.run() at native fps; termgfx drops video frames as needed
 *   8. teardown (always): unload core + restore the BBS terminal
 */
#include "retro_core.h"
#include "syncretro.h"
#include "syncretro_binds.h"
#include "syncretro_audio.h"
#include "syncretro_plat.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Pace the CORE to its native rate. Video presentation is frame-dropped
 * downstream (syncretro_io.c's DSR-ACK gate); audio, when M4 enables it, stays
 * real-time. Slowing retro_run() to match the pipe instead would pitch-shift the
 * audio and run the game in slow motion -- DESIGN.md sec 3's "frame-rate
 * decision".
 *
 * Absolute deadlines accumulated from a fixed epoch, NOT a relative sleep per
 * frame: a relative sleep pays for the frame's own compute time twice (once
 * running it, once sleeping a full period afterward) and the error compounds, so
 * a 60 fps core would drift slow forever. Recomputing the next deadline from the
 * previous one keeps the long-run average exact.
 *
 * The deadline lives in the monotonic-microsecond domain (sr_plat_now_us),
 * reframed from the POSIX build's clock_nanosleep(TIMER_ABSTIME) so it ports to
 * a platform with only a millisecond sleep: we sleep the whole-millisecond part
 * of the remaining time and let the loop's recheck absorb the sub-millisecond
 * rounding. Accumulating `next` regardless of how long a sleep actually took is
 * what keeps that rounding from becoming drift.
 *
 * When a frame overruns its budget (a slow sixel encode, a stalled write) the
 * deadline is already in the past, we do NOT try to catch up by running frames
 * back to back -- the deadline is reset to now, so an overrun costs that frame's
 * lateness and nothing more. Catching up would fast-forward the game.
 */
static void sr_pace_to_rate(double fps)
{
	static int64_t next_us;
	static int     armed;
	int64_t        period_us, now;

	if (fps <= 0.0)
		fps = 60.0;                     /* a core with no timing: assume 60 */
	period_us = (int64_t)(1000000.0 / fps);

	if (!armed) {
		next_us = sr_plat_now_us();
		armed = 1;
	}

	next_us += period_us;

	now = sr_plat_now_us();
	if (now > next_us) {
		next_us = now;                  /* overran: resync, never fast-forward */
		return;
	}
	/* Sleep the whole-ms remainder, then recheck: a millisecond-granularity
	 * sleep can wake early or late, so loop until the deadline is reached (or
	 * passed). A sub-ms remainder is left unslept rather than busy-spun -- `next`
	 * still advanced by a full period, so the average stays exact. */
	for (;;) {
		int64_t rem = next_us - sr_plat_now_us();

		if (rem < 1000)
			break;
		sr_plat_sleep_ms((int)(rem / 1000));
	}
}

/* The door owns pause and help. FreeIntv has its own -- reachable from RetroPad
 * START, painted into the framebuffer -- but its help screen describes the
 * core's RetroPad convention, which our bindings replace, and we could not
 * intercept a word of it. See M2_INPUT.md sec 2.
 *
 * Both screens are plain ANSI text over a cleared screen, NOT pixels. That
 * works on every graphics tier and on a client with no graphics at all. Leaving
 * either screen invalidates the frame cache, so the game repaints in full
 * rather than diffing against a frame the player never saw. */

static void sr_puts(const char *s)
{
	sr_out_put(s, strlen(s));
}

/* One screen serves pause, '?' and an unbound key: a player who reaches any of
 * the three wants the same thing -- the list of keys. Only the banner and the
 * footer differ, because only pause needs a specific key to leave. */
static void sr_screen_keys(int paused)
{
	const char *key, *desc;
	char        line[80];
	int         i;

	sr_puts("\x1b[2J\x1b[H");   /* clear + home */
	sr_puts(paused ? "SyncRetro -- PAUSED\r\n\r\n" : "SyncRetro -- keys\r\n\r\n");
	for (i = 0; sr_bind_help_line(i, &key, &desc); i++) {
		snprintf(line, sizeof line, "  %-18s %s\r\n", key, desc);
		sr_puts(line);
	}
	/* Which controller a cartridge reads is per-game, not a convention: both
	 * Pac-Man ports ignore the left one entirely (start = Tab, then 1), while
	 * Astrosmash reads either.  So the hint has to be the general one. */
	sr_puts("\r\n  Some cartridges use the RIGHT hand controller, even for one\r\n");
	sr_puts("  player.  If a game ignores you, press Tab and try again.\r\n");
	sr_puts("  Most games start on a keypad digit, not an action button.\r\n");
	sr_puts(paused ? "\r\n  Press Space to resume.\r\n"
	               : "\r\n  Press any key to return to the game.\r\n");
	sr_io_out_flush();
}

/* Sleep out the frame budget while the core is not running. */
static void sr_idle_ms(long ms)
{
	sr_plat_sleep_ms((int)ms);
}

int main(int argc, char **argv)
{
	rc_core_t core;
	int       rc = 1;

	if (sr_door_setup(argc, argv) != 0)
		return 1;
	sr_door_sanitize_argv(&argc, argv);
	sr_config_apply();
	sr_audio_init();                    /* reads syncretro.ini; no I/O yet */

	/* sr_config_apply() has already chdir'd into the per-user sandbox, so the
	 * core and ROM must come from IT (absolutized against the launch dir), not
	 * from the raw sr_door_*_path() command-line values. */
	if (sr_config_core_path() == NULL) {
		fprintf(stderr, "syncretro: no libretro core (use -core <path> or $SYNCRETRO_CORE)\n");
		return 1;
	}
	if (rc_core_open(&core, sr_config_core_path()) != 0)
		return 1;

	if (sr_io_init(sr_door_socket()) != 0)
		goto done;

	if (rc_core_load_game(&core, sr_config_rom_path()) != 0)
		goto done;

	{
		int paused  = 0;   /* Space: the core is not being run */
		int helping = 0;   /* the key legend is up ('?', or an unbound key) */

		for (;;) {
			int action;

			if (sr_door_should_exit())
				break;

			/* A door screen is up: the core is not running, so nothing polls the
			 * socket for us, and game keys must be swallowed rather than held. */
			if (paused || helping) {
				sr_input_pump();
				action = sr_input_take_action();

				if (helping) {
					/* Any key returns -- bound or not, which is why
					 * sr_input_set_suspended() clears the anykey latch when the
					 * screen opens. */
					if (action != SR_DOOR_NONE || sr_input_take_anykey()) {
						helping = 0;
						sr_input_set_suspended(0);
						sr_audio_pause(0);
						sr_io_invalidate();
					}
				} else if (action == SR_DOOR_PAUSE) {
					paused = 0;
					sr_input_set_suspended(0);
					sr_audio_pause(0);
					sr_io_invalidate();
				} else if (action == SR_DOOR_HELP) {
					/* The pause screen already IS the key legend, so '?' behind it
					 * has nothing to add. Swallow it, and swallow any game key
					 * struck behind the screen along with it. */
					(void)sr_input_take_anykey();
				} else if (action == SR_DOOR_RESET) {
					core.reset();
					sr_audio_reset();       /* queued audio outlived its game */
					paused = 0;
					sr_input_set_suspended(0);
					sr_audio_pause(0);
					sr_io_invalidate();
				}
				sr_idle_ms(16);
				continue;
			}

			/* Once a second under keytrace: is the core still being run, and what
			 * pad does it see? Separates "the door stalled" from "the core has
			 * the input and draws nothing" -- the two look identical on screen. */
			{
				static unsigned nrun;

				if ((nrun++ % 60) == 0)
					sr_trace("core.run #%u pad L=%d R=%d U=%d D=%d", nrun,
					         sr_pad_get(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT),
					         sr_pad_get(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT),
					         sr_pad_get(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP),
					         sr_pad_get(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN));
			}
			core.run();             /* -> video/audio/input callbacks fire */

			action = sr_input_take_action();
			if (action == SR_DOOR_PAUSE || action == SR_DOOR_HELP) {
				sr_input_release_all();     /* no key survives the screen */
				sr_input_set_suspended(1);
				sr_audio_pause(1);          /* core stops -> FIFO drains on purpose */
				if (action == SR_DOOR_PAUSE) {
					paused = 1;
					sr_screen_keys(1);
				} else {
					helping = 1;
					sr_screen_keys(0);
				}
				continue;
			}
			if (action == SR_DOOR_RESET) {
				sr_input_release_all();
				core.reset();
				sr_audio_reset();           /* queued audio outlived its game */
				sr_io_invalidate();
			}
			sr_pace_to_rate(core.av.timing.fps);
		}
	}
	rc = 0;

done:
	sr_audio_shutdown();                /* A;Flush + telemetry, before the restore */
	sr_io_leave();                      /* restore the BBS terminal, always */
	rc_core_close(&core);
	return rc;
}
