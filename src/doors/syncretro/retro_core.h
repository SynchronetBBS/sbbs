/* retro_core.h -- thin wrapper over a loaded libretro core's ABI.
 *
 * A libretro core is an external, unmodified shared object (FreeIntv_libretro.so,
 * fceumm_libretro.so, ...). SyncRetro drives it THROUGH this struct only; the
 * core .so is never patched (there is no vendored engine source -- see
 * CLAUDE.md and DESIGN.md sec 1). The one vendored artifact is libretro.h, the
 * MIT-licensed API header (PROVENANCE.md).
 */
#ifndef SYNCRETRO_RETRO_CORE_H_
#define SYNCRETRO_RETRO_CORE_H_

#include <stdbool.h>
#include "libretro.h"   /* VENDORED MIT-licensed libretro API header */

/* A loaded core: dlopen handle, the resolved retro_* entry points, and the
 * geometry/timing captured from retro_get_system_av_info() at load.
 *
 * The `rc_core` tag is load-bearing: syncretro.h forward-declares
 * `struct rc_core` so sr_bridge_install() can take one without pulling
 * libretro.h into the door-glue contract. */
typedef struct rc_core {
	void *dl;                               /* dlopen() handle */

	/* callback setters (must be called before init) */
	void (*set_environment)(retro_environment_t);
	void (*set_video_refresh)(retro_video_refresh_t);
	void (*set_audio_sample)(retro_audio_sample_t);
	void (*set_audio_sample_batch)(retro_audio_sample_batch_t);
	void (*set_input_poll)(retro_input_poll_t);
	void (*set_input_state)(retro_input_state_t);

	/* lifecycle */
	unsigned (*api_version)(void);
	void (*get_system_info)(struct retro_system_info *);
	void (*get_system_av_info)(struct retro_system_av_info *);
	void (*init)(void);
	void (*deinit)(void);
	bool (*load_game)(const struct retro_game_info *);
	void (*unload_game)(void);
	void (*run)(void);
	void (*reset)(void);

	struct retro_system_av_info av;         /* captured after load_game() */
	bool game_loaded;

	/* ROM image, for a core with need_fullpath = false. Held for the core's
	 * whole lifetime, not just across load_game(): libretro lets a core keep
	 * pointing into this buffer rather than copying it, and freeing it early
	 * would leave the core reading a dangling pointer for the whole session. */
	void  *rom_data;
	size_t rom_size;
} rc_core_t;

/* dlopen `path` and resolve every required entry point. Verifies
 * retro_api_version() == RETRO_API_VERSION. Returns 0 on success, -1 on any
 * missing symbol / dlopen failure / API mismatch (diagnostic to stderr). */
int rc_core_open(rc_core_t *c, const char *path);

/* Wire the six callbacks (from retro_bridge.c), retro_init(), load the ROM, and
 * capture av_info. `rom_path` may be NULL for cores that need no content.
 * Returns 0 on success, -1 on failure. Call AFTER rc_core_open(). */
int rc_core_load_game(rc_core_t *c, const char *rom_path);

/* unload_game() + deinit() + dlclose(), best-effort, idempotent. */
void rc_core_close(rc_core_t *c);

#endif /* SYNCRETRO_RETRO_CORE_H_ */
