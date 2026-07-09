/* retro_core.c -- dlopen a libretro core and resolve its ABI.
 *
 * This is the one place that touches the core .so. Everything else drives the
 * core through the rc_core_t function pointers. See retro_core.h / DESIGN.md.
 */
#include "retro_core.h"
#include "syncretro.h"   /* sr_bridge_install(), retro_env callback owner */

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Resolve `sym` into c->field; on failure log and jump to fail. The
 * cast-through-void** is the standard (POSIX-blessed) dlsym idiom. */
#define RC_RESOLVE(field, sym)                                            \
	do {                                                                  \
		*(void **)(&c->field) = dlsym(c->dl, sym);                        \
		if (c->field == NULL) {                                           \
			fprintf(stderr, "syncretro: core is missing '%s'\n", sym);    \
			goto fail;                                                    \
		}                                                                 \
	} while (0)

int rc_core_open(rc_core_t *c, const char *path)
{
	memset(c, 0, sizeof(*c));

	c->dl = dlopen(path, RTLD_NOW | RTLD_LOCAL);
	if (c->dl == NULL) {
		fprintf(stderr, "syncretro: cannot load core '%s': %s\n", path, dlerror());
		return -1;
	}

	RC_RESOLVE(set_environment,        "retro_set_environment");
	RC_RESOLVE(set_video_refresh,      "retro_set_video_refresh");
	RC_RESOLVE(set_audio_sample,       "retro_set_audio_sample");
	RC_RESOLVE(set_audio_sample_batch, "retro_set_audio_sample_batch");
	RC_RESOLVE(set_input_poll,         "retro_set_input_poll");
	RC_RESOLVE(set_input_state,        "retro_set_input_state");
	RC_RESOLVE(api_version,            "retro_api_version");
	RC_RESOLVE(get_system_info,        "retro_get_system_info");
	RC_RESOLVE(get_system_av_info,     "retro_get_system_av_info");
	RC_RESOLVE(init,                   "retro_init");
	RC_RESOLVE(deinit,                 "retro_deinit");
	RC_RESOLVE(load_game,              "retro_load_game");
	RC_RESOLVE(unload_game,            "retro_unload_game");
	RC_RESOLVE(run,                    "retro_run");
	RC_RESOLVE(reset,                  "retro_reset");

	if (c->api_version() != RETRO_API_VERSION) {
		fprintf(stderr, "syncretro: core libretro API %u != frontend %u\n",
		        c->api_version(), (unsigned)RETRO_API_VERSION);
		goto fail;
	}
	return 0;

fail:
	dlclose(c->dl);
	c->dl = NULL;
	return -1;
}

/* Slurp `path` into a malloc'd buffer. Returns NULL (and logs) on any failure. */
static void *rc_read_file(const char *path, size_t *size_out)
{
	FILE * f = fopen(path, "rb");
	void * buf;
	long   len;
	size_t got;

	if (f == NULL) {
		fprintf(stderr, "syncretro: cannot open ROM '%s'\n", path);
		return NULL;
	}
	if (fseek(f, 0, SEEK_END) != 0 || (len = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
		fprintf(stderr, "syncretro: cannot size ROM '%s'\n", path);
		fclose(f);
		return NULL;
	}
	buf = malloc((size_t)len ? (size_t)len : 1);
	if (buf == NULL) {
		fclose(f);
		return NULL;
	}
	got = fread(buf, 1, (size_t)len, f);
	fclose(f);
	if (got != (size_t)len) {
		fprintf(stderr, "syncretro: short read on ROM '%s'\n", path);
		free(buf);
		return NULL;
	}
	*size_out = got;
	return buf;
}

int rc_core_load_game(rc_core_t *c, const char *rom_path)
{
	struct retro_game_info   info;
	struct retro_system_info si;

	/* Install callbacks first: the environment cb in particular is queried by
	 * many cores DURING retro_init/retro_load_game (pixel format, dirs, ...). */
	sr_bridge_install(c);

	c->init();

	memset(&si, 0, sizeof(si));
	c->get_system_info(&si);

	memset(&info, 0, sizeof(info));
	info.path = rom_path;   /* NULL is valid for no-content cores */

	/* need_fullpath = true (FreeIntv): the core opens the file itself and
	 * info.data must stay NULL. Otherwise the frontend owns the load, and the
	 * core reads the bytes we hand it -- see rc_core_t.rom_data on why the
	 * buffer outlives this call. */
	if (rom_path != NULL && !si.need_fullpath) {
		c->rom_data = rc_read_file(rom_path, &c->rom_size);
		if (c->rom_data == NULL)
			return -1;
		info.data = c->rom_data;
		info.size = c->rom_size;
	}

	if (!c->load_game(&info)) {
		fprintf(stderr, "syncretro: core rejected ROM '%s'\n",
		        rom_path ? rom_path : "(none)");
		return -1;
	}
	c->game_loaded = true;

	memset(&c->av, 0, sizeof(c->av));
	c->get_system_av_info(&c->av);
	return 0;
}

void rc_core_close(rc_core_t *c)
{
	if (c->dl == NULL)
		return;
	if (c->game_loaded && c->unload_game)
		c->unload_game();
	if (c->deinit)
		c->deinit();
	dlclose(c->dl);
	c->dl = NULL;
	c->game_loaded = false;
	free(c->rom_data);            /* only now: the core may have read it all session */
	c->rom_data = NULL;
	c->rom_size = 0;
}
