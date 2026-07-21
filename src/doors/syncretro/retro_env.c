/* retro_env.c -- the retro_environment_t callback (frontend policy).
 *
 * The core queries the frontend through this one entry point during init, load,
 * and run. We answer the handful that matter for a terminal frontend and return
 * false for everything else (the core then adapts). See DESIGN.md sec 5.
 */
#include "retro_core.h"
#include "retro_options.h"
#include "syncretro.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>    /* getenv() -- SYNCRETRO_CORE_DEBUG */
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

		/* --- core options (retro_options.c) --------------------------------
		 * We do NOT answer GET_CORE_OPTIONS_VERSION, which libretro defines as
		 * "version 0", so a conforming core declares through SET_VARIABLES and
		 * that is the arm that runs in practice. The v1/v2 arms are here because
		 * a core that skips the version handshake would otherwise leave the
		 * store EMPTY and silent -- the exact failure this file used to have. */
		case RETRO_ENVIRONMENT_SET_VARIABLES:
			ro_declare_v0(data);
			return true;

		case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
			ro_declare_v1(data);
			return true;

		case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
			ro_declare_v1(((const struct retro_core_options_intl *)data)->us);
			return true;

		case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
			ro_declare_v2(data);
			return true;

		case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:
			ro_declare_v2(((const struct retro_core_options_v2_intl *)data)->us);
			return true;

		/* Answering false here is NOT the same as leaving the core on its own
		 * defaults, however the docs read: MAME 2003-Plus answers an unset
		 * sample-rate option with 0 Hz and then emits no audio at all (measured
		 * -- probe_core.c). Hand back the pinned value, or the core's own
		 * advertised default. A key we never saw declared still gets false. */
		case RETRO_ENVIRONMENT_GET_VARIABLE: {
			struct retro_variable *var = data;
			const char *           val;

			if (var == NULL || var->key == NULL)
				return false;
			if ((val = ro_option_get(var->key)) == NULL)
				return false;
			var->value = val;
			return true;
		}

		/* Polled EVERY FRAME by some cores (MAME 2003-Plus does). Nothing
		 * changes an option mid-session, so answer a definite no rather than
		 * leaving the call unanswered. */
		case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
			*(bool *)data = ro_options_changed() ? true : false;
			return true;

		/* TODO(M5): surface input descriptors + controller info:
		 *   RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS  (drives the key overlay)
		 *   RETRO_ENVIRONMENT_SET_CONTROLLER_INFO
		 *   RETRO_ENVIRONMENT_GET_CAN_DUPE  -> true (we drop frames; DESIGN.md sec 3)
		 */

		default:
			return false;
	}
}

/* Route core log output to the door log. TODO: forward to the termgfx/sbbs_node
 * logger with the right level; for now, stderr.
 *
 * DEBUG IS DROPPED, AND THAT IS A PERFORMANCE FIX, NOT TIDINESS. This callback
 * used to `(void)level` and forward everything. FreeIntv and FCEUmm say almost
 * nothing at debug level, so it cost nothing and looked harmless for three
 * milestones. MAME 2003-Plus logs a line EVERY TIME the game pokes an unmapped
 * address -- and Pac-Man pokes its watchdog register constantly, so a live
 * session produced ~1900 copies of one message. Each became a vfprintf to the
 * door's stderr, which the BBS redirects to data/syncretro/syncretro_n<node>.log
 * -- a file that on a multi-host install lives on an SMB share. A synchronous
 * network write inside the frame loop, several times a frame, is enough to blow
 * the frame budget: the game visibly slowed and the audio FIFO starved.
 *
 * The core's own speed was never the problem -- measured at ~1100 fps, 18x
 * real-time. We were spending the frame budget logging.
 *
 * INFO is KEPT: a core's info lines are bounded (paths, the matched driver) and
 * they are what makes a failed load diagnosable. Set SYNCRETRO_CORE_DEBUG=1 to
 * get the debug firehose back when diagnosing a core. */
static int env_log_debug(void)
{
	static int on = -1;
	const char *s;

	if (on < 0) {
		s  = getenv("SYNCRETRO_CORE_DEBUG");
		on = (s != NULL && *s != '\0' && *s != '0') ? 1 : 0;
	}
	return on;
}

static void env_log(enum retro_log_level level, const char *fmt, ...)
{
	va_list ap;

	if (level == RETRO_LOG_DEBUG && !env_log_debug())
		return;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}
