/* retro_env.c -- the retro_environment_t callback (frontend policy).
 *
 * The core queries the frontend through this one entry point during init, load,
 * and run. We answer the handful that matter for a terminal frontend and return
 * false for everything else (the core then adapts). See DESIGN.md sec 5.
 */
#include "retro_core.h"
#include "syncretro.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void env_log(enum retro_log_level level, const char *fmt, ...);

bool sr_environment(unsigned cmd, void *data)
{
	switch (cmd) {

		case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
			const enum retro_pixel_format *fmt = data;
			/* We can convert any of the three (retro_bridge.c). Accept all. */
			sr_bridge_set_pixfmt((int)*fmt);
			return true;
		}

		/* Never hand a core a NULL directory. libretro permits it (NULL == "not
		 * set"), but real cores don't all check: FreeIntv's retro_init() feeds
		 * this straight to fill_pathname_join() -> strlcpy(NULL) and segfaults,
		 * before we ever reach retro_load_game(). Returning false is no safer --
		 * the core reads its own uninitialized/NULL variable. So always answer
		 * with a valid path; "." is the door's cwd, which sr_config_apply() has
		 * already moved to the per-user sandbox. */
		case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: {
			const char *dir = sr_config_system_dir();    /* BIOS dir */
			*(const char **)data = dir != NULL ? dir : ".";
			return true;
		}

		case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: {
			const char *dir = sr_config_save_dir();      /* per-user SRAM/state */
			*(const char **)data = dir != NULL ? dir : ".";
			return true;
		}

		case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
			struct retro_log_callback *cb = data;
			cb->log = env_log;
			return true;
		}

		case RETRO_ENVIRONMENT_SET_HW_RENDER:
			/* Software cores only -- no GL/Vulkan in a terminal (DESIGN.md sec 12). */
			return false;

		/* TODO(M2/M3): surface core options + input descriptors:
		 *   RETRO_ENVIRONMENT_GET_VARIABLE / SET_VARIABLES / SET_CORE_OPTIONS(_V2)
		 *   RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS  (drives the key overlay)
		 *   RETRO_ENVIRONMENT_SET_CONTROLLER_INFO
		 *   RETRO_ENVIRONMENT_GET_CAN_DUPE  -> true (we drop frames; DESIGN.md sec 3)
		 */

		default:
			return false;
	}
}

/* Route core log output to the door log. TODO: forward to the termgfx/sbbs_node
 * logger with the right level; for now, stderr. */
static void env_log(enum retro_log_level level, const char *fmt, ...)
{
	va_list ap;
	(void)level;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}
