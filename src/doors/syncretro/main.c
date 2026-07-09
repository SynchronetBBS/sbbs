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

#include <stdio.h>
#include <time.h>

/* Sleep so the core runs at ~fps (native emulation rate). Video presentation is
 * frame-dropped downstream (termgfx AIMD); audio, when enabled, stays real-time.
 * TODO(M1): a real monotonic-clock pacer; this is the shape. */
static void sr_pace_to_rate(double fps)
{
	(void)fps;
	/* TODO(M1): clock_nanosleep(CLOCK_MONOTONIC, ...) to the next frame boundary. */
}

int main(int argc, char **argv)
{
	rc_core_t core;
	int rc = 1;

	if (sr_door_setup(argc, argv) != 0)
		return 1;
	sr_door_sanitize_argv(&argc, argv);
	sr_config_apply();

	if (sr_door_core_path() == NULL) {
		fprintf(stderr, "syncretro: no libretro core (use -core <path> or door config)\n");
		return 1;
	}
	if (rc_core_open(&core, sr_door_core_path()) != 0)
		return 1;

	if (sr_io_init(sr_door_socket()) != 0)
		goto done;

	if (rc_core_load_game(&core, sr_door_rom_path()) != 0)
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
