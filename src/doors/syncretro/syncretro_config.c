/* syncretro_config.c -- per-user sandbox + system/save dir resolution.
 *
 * Creates the -home sandbox and resolves the BIOS (system) + save dirs the core
 * asks for via retro_env.c's GET_SYSTEM_DIRECTORY / GET_SAVE_DIRECTORY. Mirrors
 * ../syncmoo1/syncmoo1_config.c (which pins 1oom's data + user paths the same
 * way). See DESIGN.md sec 10.
 *
 * Both directories are ABSOLUTIZED here, before the chdir below moves cwd. The
 * core keeps the pointers it is handed and re-reads them whenever it builds a
 * path (FreeIntv joins the system dir onto "exec.bin" inside retro_init(), and
 * cores re-join the save dir on every SRAM flush), so a relative path resolved
 * against the launch directory would silently start naming a different place
 * once we were inside the sandbox. This is the same absolutize-before-anything-
 * moves discipline syncmoo1_config.c documents.
 *
 * Resolution, highest precedence first:
 *   system dir:  $SYNCRETRO_SYSTEM  >  the door's launch directory
 *   save dir:    -home <dir>        >  the system dir
 *   core .so:    as given           >  <launch>/<given>
 *   ROM:         as given           >  <launch>/roms/<given>  >  <launch>/<given>
 *
 * The launch-directory default is the useful one for a BBS: with an SCFG
 * start-up directory set, xtrn.cpp's external() chdir()s the forked child there
 * before exec, so cwd IS the door's own dir -- e.g. xtrn/syncivision, the
 * natural place for a sysop to drop exec.bin/grom.bin beside the core .so, with
 * the cartridges in a roms/ sub-directory. That layout is why the ROM search
 * includes roms/: `syncretro -core freeintv_libretro.so 4-tris.rom` works from
 * the door's own directory with no path spelled out.
 *
 * The CORE and ROM must be absolutized here for the same reason the dirs are,
 * and it is easy to miss: the chdir below happens before main.c ever calls
 * rc_core_open()/rc_core_load_game(), so a relative `roms/4-tris.rom` handed
 * straight through would be looked up inside the per-user sandbox and not found.
 *
 * Nothing here is fatal. A door that cannot create its save dir should still let
 * the player play; it just won't keep their SRAM. Every failure is logged and
 * the resolution falls through to the next candidate.
 */
#include "syncretro.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dirwrap.h"   /* xpdev: mkpath() -- recursive mkdir */
#include "ini_file.h"        /* xpdev: iniReadFile / iniGet* / strListFree */
#include "audio_mgr.h"       /* termgfx: TERMGFX_MUSIC_QUALITY_DEFAULT */

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static char g_system_dir[PATH_MAX];   /* BIOS: shared, read-only, per-install */
static char g_save_dir[PATH_MAX];     /* per-user SRAM + save states */
static char g_core_path[PATH_MAX];    /* the .so to dlopen, absolute */
static char g_rom_path[PATH_MAX];     /* the cartridge, absolute */

/* Where the sysop keeps cartridges, relative to the door's launch directory. */
#define SR_ROM_SUBDIR "roms"

/* syncretro.ini, [audio]. Defaults chosen in M4_AUDIO.md sec 4; the quality
 * default is termgfx's music constant, which the M4 measurements independently
 * landed on. Mirrors ../syncduke/syncduke_config.c's pattern exactly. */
static int    g_audio_enabled   = 1;
static double g_audio_quality   = TERMGFX_MUSIC_QUALITY_DEFAULT;
static int    g_audio_volume    = 100;
static int    g_audio_chunk_ms  = 100;
static int    g_audio_prebuffer = 3;

static int sr_clamp_int(int v, int lo, int hi)
{
	if (v < lo)
		return lo;
	if (v > hi)
		return hi;
	return v;
}

static void sr_config_read_ini(void)
{
	FILE *f = fopen("syncretro.ini", "r");

	if (f != NULL) {
		str_list_t ini = iniReadFile(f);

		fclose(f);
		g_audio_enabled   = iniGetBool(ini, "audio", "enabled", TRUE);
		g_audio_quality   = iniGetFloat(ini, "audio", "quality",
		                                TERMGFX_MUSIC_QUALITY_DEFAULT);
		g_audio_volume    = iniGetInteger(ini, "audio", "volume", 100);
		g_audio_chunk_ms  = iniGetInteger(ini, "audio", "chunk_ms", 100);
		g_audio_prebuffer = iniGetInteger(ini, "audio", "prebuffer", 3);
		strListFree(&ini);
	}
	/* Positive test, not two one-sided comparisons: NaN compares false under
	 * both < and >, and iniGetFloat() is atof()-backed, so `quality = nan`
	 * would otherwise sail through unclamped and into the encoder. */
	if (!(g_audio_quality >= 0.01 && g_audio_quality <= 1.0))
		g_audio_quality = TERMGFX_MUSIC_QUALITY_DEFAULT;
	g_audio_volume    = sr_clamp_int(g_audio_volume, 0, 100);
	g_audio_chunk_ms  = sr_clamp_int(g_audio_chunk_ms, 50, 250);
	g_audio_prebuffer = sr_clamp_int(g_audio_prebuffer, 2, 8);
}

int    sr_config_audio_enabled(void)   { return g_audio_enabled; }
double sr_config_audio_quality(void)   { return g_audio_quality; }
int    sr_config_audio_volume(void)    { return g_audio_volume; }
int    sr_config_audio_chunk_ms(void)  { return g_audio_chunk_ms; }
int    sr_config_audio_prebuffer(void) { return g_audio_prebuffer; }

/* realpath() into `dst`, falling back to a verbatim copy when it can't resolve
 * (a path that doesn't exist yet, say). Returns 1 if `dst` is absolute. */
static int sr_absolutize(char *dst, size_t sz, const char *src)
{
	char abs[PATH_MAX];

	if (realpath(src, abs) != NULL) {
		snprintf(dst, sz, "%s", abs);
		return 1;
	}
	snprintf(dst, sz, "%s", src);
	return src[0] == '/';
}

static int sr_exists(const char *path)
{
	return access(path, R_OK) == 0;
}

/* Resolve `name` to an absolute, existing file by trying it as given first, then
 * each candidate directory in `dirs` (NULL-terminated). Falls back to the
 * absolutized as-given form, so the caller still gets a usable path -- and the
 * core's own "cannot open" diagnostic names something a sysop can act on -- when
 * nothing matched. An absolute `name` never consults `dirs`. */
static void sr_resolve_file(char *dst, size_t sz, const char *name,
                            const char *const *dirs)
{
	char cand[PATH_MAX];
	int  i;

	if (sr_exists(name) || name[0] == '/') {
		sr_absolutize(dst, sz, name);
		return;
	}
	for (i = 0; dirs != NULL && dirs[i] != NULL; i++) {
		snprintf(cand, sizeof cand, "%s/%s", dirs[i], name);
		if (sr_exists(cand)) {
			sr_absolutize(dst, sz, cand);
			return;
		}
	}
	sr_absolutize(dst, sz, name);   /* not found: let the open() report it by name */
}

int sr_config_apply(void)
{
	const char *sys  = getenv("SYNCRETRO_SYSTEM");
	const char *home = sr_door_home();
	const char *core = sr_door_core_path();
	const char *rom  = sr_door_rom_path();
	int         rc   = 0;
	char        cwd[PATH_MAX];
	/* cwd + '/' + "roms" + NUL: sized so the join below cannot truncate (and so
	 * GCC can see that, rather than warning about it under -Werror). */
	char        romdir[PATH_MAX + sizeof(SR_ROM_SUBDIR) + 1];

	/* The launch directory, captured before anything can move cwd. */
	if (getcwd(cwd, sizeof cwd) == NULL)
		snprintf(cwd, sizeof cwd, ".");

	/* syncretro.ini lives in the launch directory, not the per-user sandbox --
	 * must run before the chdir() in step 3 below moves cwd there. */
	sr_config_read_ini();

	/* --- 1. system (BIOS) dir: shared, read-only, per-install ---------------
	 * Answered to the core as GET_SYSTEM_DIRECTORY. A core that can't find its
	 * BIOS here still loads and runs (FreeIntv logs "Failed loading Executive
	 * BIOS" and halts its CPU on a diagnostic screen) -- so a missing BIOS is a
	 * content problem for the sysop, not a startup failure for the door. */
	g_system_dir[0] = '\0';
	if (sys != NULL && sys[0] != '\0') {
		if (!sr_absolutize(g_system_dir, sizeof g_system_dir, sys))
			fprintf(stderr, "syncretro: SYNCRETRO_SYSTEM '%s' did not resolve -- "
			        "using it as given\n", sys);
	} else {
		snprintf(g_system_dir, sizeof g_system_dir, "%s", cwd);
	}

	/* --- 2. the core .so and the ROM ---------------------------------------
	 * Absolutized against the launch dir NOW, before the chdir below. The ROM
	 * additionally searches <launch>/roms/, the conventional cartridge dir. */
	if (core != NULL) {
		const char *const dirs[] = { cwd, NULL };

		sr_resolve_file(g_core_path, sizeof g_core_path, core, dirs);
	}
	if (rom != NULL) {
		const char *const dirs[] = { romdir, cwd, NULL };

		snprintf(romdir, sizeof romdir, "%s/%s", cwd, SR_ROM_SUBDIR);
		sr_resolve_file(g_rom_path, sizeof g_rom_path, rom, dirs);
	}

	/* --- 3. per-user sandbox: save states + SRAM ---------------------------
	 * mkpath() FIRST (creates any missing components, e.g. a brand-new user's
	 * nested data/user/0042/retro/), so the absolutize below has something to
	 * resolve. The chdir keeps any cwd-relative I/O a core might do inside the
	 * sandbox; the absolute save dir handed to GET_SAVE_DIRECTORY is what
	 * actually isolates the saves.
	 *
	 * No -home: the save dir falls back to the system dir, which is where a
	 * dev run's cwd already is. Never NULL -- retro_env.c would have to
	 * substitute "." and a core that ignores the answer's lifetime would then
	 * write saves wherever cwd happens to point. */
	if (home != NULL) {
		if (mkpath(home) != 0) {
			fprintf(stderr, "syncretro: cannot create per-user home '%s'\n", home);
			rc = -1;   /* logged, not fatal */
		}
		sr_absolutize(g_save_dir, sizeof g_save_dir, home);
		if (chdir(home) != 0) {
			fprintf(stderr, "syncretro: cannot enter per-user home '%s'\n", home);
			rc = -1;   /* logged, not fatal */
		}
	} else {
		snprintf(g_save_dir, sizeof g_save_dir, "%s", g_system_dir);
	}

	return rc;
}

const char *sr_config_system_dir(void)
{
	return g_system_dir[0] ? g_system_dir : NULL;
}

const char *sr_config_save_dir(void)
{
	return g_save_dir[0] ? g_save_dir : NULL;
}

const char *sr_config_core_path(void)
{
	return g_core_path[0] ? g_core_path : NULL;
}

const char *sr_config_rom_path(void)
{
	return g_rom_path[0] ? g_rom_path : NULL;
}
