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

#include <errno.h>
#include <stdio.h>
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

	for (;;) {
		if (sr_door_should_exit())
			break;
		core.run();                     /* -> video/audio/input callbacks fire */
		sr_pace_to_rate(core.av.timing.fps);
	}
	rc = 0;

done:
	sr_io_leave();                      /* restore the BBS terminal, always */
	rc_core_close(&core);
	return rc;
}
