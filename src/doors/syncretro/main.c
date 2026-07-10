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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Pace the CORE to its native rate. Video presentation is frame-dropped
 * downstream (syncretro_io.c's DSR-ACK gate); audio, when M4 enables it, stays
 * real-time. Slowing retro_run() to match the pipe instead would pitch-shift the
 * audio and run the game in slow motion -- DESIGN.md sec 3's "frame-rate
 * decision".
 *
 * Absolute deadlines (TIMER_ABSTIME) accumulated from a fixed epoch, NOT a
 * relative sleep per frame: a relative sleep pays for the frame's own compute
 * time twice (once running it, once sleeping a full period afterward) and the
 * error compounds, so a 60 fps core would drift slow forever. Recomputing the
 * next deadline from the previous one keeps the long-run average exact.
 *
 * When a frame overruns its budget (a slow sixel encode, a stalled write) the
 * deadline is already in the past, clock_nanosleep() returns at once, and we do
 * NOT try to catch up by running frames back to back -- the deadline is reset to
 * now, so an overrun costs that frame's lateness and nothing more. Catching up
 * would fast-forward the game.
 */
static void sr_pace_to_rate(double fps)
{
	static struct timespec next;
	static int             armed;
	long                   period_ns;
	struct timespec        now;

	if (fps <= 0.0)
		fps = 60.0;                     /* a core with no timing: assume 60 */
	period_ns = (long)(1000000000.0 / fps);

	if (!armed) {
		clock_gettime(CLOCK_MONOTONIC, &next);
		armed = 1;
	}

	next.tv_nsec += period_ns;
	while (next.tv_nsec >= 1000000000L) {
		next.tv_nsec -= 1000000000L;
		next.tv_sec++;
	}

	clock_gettime(CLOCK_MONOTONIC, &now);
	if (now.tv_sec > next.tv_sec
	    || (now.tv_sec == next.tv_sec && now.tv_nsec > next.tv_nsec)) {
		next = now;                     /* overran: resync, never fast-forward */
		return;
	}
	while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL) == EINTR)
		;                               /* a signal must not shorten the frame */
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

static void sr_screen_help(void)
{
	const char *key, *desc;
	char        line[80];
	int         i;

	sr_puts("\x1b[2J\x1b[H");   /* clear + home */
	sr_puts("SyncRetro -- keys\r\n\r\n");
	for (i = 0; sr_bind_help_line(i, &key, &desc); i++) {
		snprintf(line, sizeof line, "  %-18s %s\r\n", key, desc);
		sr_puts(line);
	}
	sr_puts("\r\n  Press any key to return to the game.\r\n");
	sr_io_out_flush();
}

static void sr_screen_paused(void)
{
	sr_puts("\x1b[2J\x1b[H");
	sr_puts("SyncRetro -- PAUSED\r\n\r\n");
	sr_puts("  Space   resume\r\n");
	sr_puts("  ?       keys\r\n");
	sr_puts("  Ctrl-Q  quit\r\n");
	sr_io_out_flush();
}

/* Sleep out the frame budget while the core is not running. */
static void sr_idle_ms(long ms)
{
	struct timespec ts;

	ts.tv_sec  = ms / 1000;
	ts.tv_nsec = (ms % 1000) * 1000000L;
	while (nanosleep(&ts, &ts) == -1 && errno == EINTR)
		;
}

int main(int argc, char **argv)
{
	rc_core_t core;
	int       rc = 1;

	if (sr_door_setup(argc, argv) != 0)
		return 1;
	sr_door_sanitize_argv(&argc, argv);
	sr_config_apply();

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
		int helping = 0;   /* the key legend is up (may sit on top of paused) */

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
					/* Any bound key returns -- including Space and '?', which is
					 * why sr_input_set_suspended() clears the anykey latch when
					 * the screen opens. */
					if (action != SR_DOOR_NONE || sr_input_take_anykey()) {
						helping = 0;
						if (paused) {
							sr_screen_paused();
						} else {
							sr_input_set_suspended(0);
							sr_io_invalidate();
						}
					}
				} else if (action == SR_DOOR_PAUSE) {
					paused = 0;
					sr_input_set_suspended(0);
					sr_io_invalidate();
				} else if (action == SR_DOOR_HELP) {
					/* Drop any game key struck while paused: it set the anykey
					 * latch (swallowed, not held) and would otherwise dismiss
					 * help on the very next iteration. The running->help path
					 * clears the latch via sr_input_set_suspended(1); we are
					 * already suspended, so drain it directly. */
					(void)sr_input_take_anykey();
					helping = 1;
					sr_screen_help();
				} else if (action == SR_DOOR_RESET) {
					core.reset();
					paused = 0;
					sr_input_set_suspended(0);
					sr_io_invalidate();
				}
				sr_idle_ms(16);
				continue;
			}

			core.run();             /* -> video/audio/input callbacks fire */

			action = sr_input_take_action();
			if (action == SR_DOOR_PAUSE || action == SR_DOOR_HELP) {
				sr_input_release_all();     /* no key survives the screen */
				sr_input_set_suspended(1);
				if (action == SR_DOOR_PAUSE) {
					paused = 1;
					sr_screen_paused();
				} else {
					helping = 1;
					sr_screen_help();
				}
				continue;
			}
			if (action == SR_DOOR_RESET) {
				sr_input_release_all();
				core.reset();
				sr_io_invalidate();
			}
			sr_pace_to_rate(core.av.timing.fps);
		}
	}
	rc = 0;

done:
	sr_io_leave();                      /* restore the BBS terminal, always */
	rc_core_close(&core);
	return rc;
}
