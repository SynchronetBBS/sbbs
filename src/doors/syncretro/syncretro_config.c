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
#include <ctype.h>
#include <string.h>

#ifdef _WIN32
  #include <direct.h>   /* _getcwd, _chdir (POSIX names via _CRT_NONSTDC_NO_DEPRECATE) */
  #include <io.h>
#else
  #include <unistd.h>   /* getcwd, chdir */
#endif

#include "dirwrap.h"   /* xpdev: mkpath() recursive mkdir, FULLPATH(), fexist() */
#include "ini_file.h"        /* xpdev: iniReadFile / iniGet* / strListFree */
#include "audio_mgr.h"       /* termgfx: TERMGFX_MUSIC_QUALITY_DEFAULT */

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* [disc] rotate: comma-separated, case-insensitive substrings of a cartridge's
 * FILENAME. A match means this cart reads the disc as a rotation (Brickout!'s
 * paddle steps once per change of disc value, so a held direction never moves
 * it) and the door must sweep the disc while a direction key is held. */
static char g_disc_rotate[512];
static int  g_disc_is_rotate;         /* does the loaded ROM match? */

static char g_launch_dir[PATH_MAX];   /* where the door was started: cwd BEFORE the chdir */
static char g_system_dir[PATH_MAX];   /* BIOS: shared, read-only, per-install */
static char g_save_dir[PATH_MAX];     /* per-user SRAM + save states */
static int  g_dirty_rect = 1;   /* [video] dirty_rect */
static int  g_palette_subset = 1;   /* [video] palette_subset */
static int  g_dirty_log;        /* [debug] dirty_log, default FALSE */
static int  g_input_device;     /* [input] device: 0 = leave the core's default */
static char g_aspect[32] = "core";    /* [video] aspect: core|square|4:3|<decimal> */
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

/* Case-insensitive strstr. Not strcasestr(): that is a GNU extension, absent
 * under MSVC and strict C, and this file must build on the Windows port. */
static const char *sr_stristr(const char *hay, const char *needle)
{
	size_t n = strlen(needle);

	if (n == 0)
		return hay;
	for (; *hay != '\0'; hay++) {
		size_t i;

		for (i = 0; i < n; i++)
			if (tolower((unsigned char)hay[i]) != tolower((unsigned char)needle[i]))
				break;
		if (i == n)
			return hay;
	}
	return NULL;
}

/* Does `path`'s filename contain any of `list`'s comma-separated substrings?
 * Case-insensitive, whitespace around an item ignored, empty list never matches.
 * Deliberately a substring test on the FILENAME: cartridge dumps carry version
 * and dump markers ("Brickout! (1981) (Mattel).int"), so a sysop writes the
 * title and not the whole name. */
static int sr_disc_matches(const char *path, const char *list)
{
	const char *name;
	const char *p = list;

	if (path == NULL || list == NULL || *list == '\0')
		return 0;
	name = strrchr(path, '/');
	name = (name != NULL) ? name + 1 : path;

	while (*p != '\0') {
		char        item[128];
		size_t      n = 0;
		const char *q;

		while (*p == ' ' || *p == '\t' || *p == ',')
			p++;
		while (p[n] != '\0' && p[n] != ',')
			n++;
		q = p + n;                       /* one past the item */
		while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\t'))
			n--;                         /* trailing space */
		if (n > 0 && n < sizeof item) {
			memcpy(item, p, n);
			item[n] = '\0';
			if (sr_stristr(name, item) != NULL)
				return 1;
		}
		p = q;
	}
	return 0;
}

static void sr_config_read_ini(void)
{
	FILE *f = fopen("syncretro.ini", "r");

	if (f != NULL) {
		str_list_t ini = iniReadFile(f);
		char       rotate[INI_MAX_VALUE_LEN];   /* iniGetString()'s contract, not ours */

		fclose(f);
		char       aspect[INI_MAX_VALUE_LEN];

		iniGetString(ini, "disc", "rotate", "", rotate);
		snprintf(g_disc_rotate, sizeof g_disc_rotate, "%s", rotate);
		iniGetString(ini, "video", "aspect", "core", aspect);
		snprintf(g_aspect, sizeof g_aspect, "%s", aspect);
		g_audio_enabled   = iniGetBool(ini, "audio", "enabled", TRUE);
		g_audio_quality   = iniGetFloat(ini, "audio", "quality",
		                                TERMGFX_MUSIC_QUALITY_DEFAULT);
		g_audio_volume    = iniGetInteger(ini, "audio", "volume", 100);
		g_audio_chunk_ms  = iniGetInteger(ini, "audio", "chunk_ms", 100);
		g_audio_prebuffer = iniGetInteger(ini, "audio", "prebuffer", 3);
		g_dirty_rect      = iniGetBool(ini, "video", "dirty_rect", TRUE);
		g_palette_subset  = iniGetBool(ini, "video", "palette_subset", TRUE);
		/* [debug] dirty_log -- a per-frame trace of the dirty-rect decision (why
		 * a frame took the patch path or the full-repaint path), plus one
		 * startup line with the geometry that governs band alignment. Off by
		 * default: it is real per-frame stderr traffic, meant for a short
		 * diagnostic session (see README.md's Diagnostics section), not to run
		 * continuously. */
		g_dirty_log       = iniGetBool(ini, "debug", "dirty_log", FALSE);
		/* [input] device -- a RETRO_DEVICE id to hand the core for both ports.
		 * 0 (the default) means say NOTHING, leaving whatever the core chose:
		 * that is the behaviour the door has always had, and the safe one, since
		 * a wrong device id silently rewires every button. A core advertises the
		 * ids it accepts through SET_CONTROLLER_INFO, which probe_core dumps. */
		g_input_device    = iniGetInteger(ini, "input", "device", 0);

		/* [options] -- libretro core options this install pins.
		 *
		 * THIS IS WHY THEY ARE NOT ON THE COMMAND LINE. A door's command line is
		 * assembled by the BBS into a fixed buffer -- xtrn.cpp's
		 * `fullcmdline[MAX_PATH + 1]`, i.e. 260 usable characters -- and it is
		 * TRUNCATED there without a word. The lobby's line for one arcade game
		 * already runs to ~240 characters (binary path, socket, time, alias,
		 * core, profile, title, console, home, ROM path), so two pinned options
		 * pushed it to 334 and the tail simply vanished: the ROM argument fell
		 * off the end and the door reported "(no ROM)" for content that was
		 * plainly there in the log the BBS printed -- because the BBS logs the
		 * untruncated string and passes the truncated one.
		 *
		 * The ini has no such limit and is already per-install, so a console
		 * pins its options here. `-option` still exists for a door run by hand.
		 *
		 * iniGetNamedStringList(), NOT iniGetSection(): the latter hands back the
		 * section's RAW LINES, comments and all, and does not trim around the
		 * '='. Feeding those to sr_option_pin() pinned "; libretro core options,
		 * passed to MAME..." as a key and left the real ones unmatched (their
		 * names kept a trailing space). The named list is already parsed and
		 * trimmed, which is the whole reason it exists. */
		{
			named_string_t **opts = iniGetNamedStringList(ini, "options");
			char             kv[INI_MAX_VALUE_LEN * 2];
			size_t           i;

			if (opts != NULL) {
				for (i = 0; opts[i] != NULL; i++) {
					snprintf(kv, sizeof kv, "%s=%s", opts[i]->name,
					         opts[i]->value != NULL ? opts[i]->value : "");
					sr_option_pin(kv);
				}
				iniFreeNamedStringList(opts);
			}
		}
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

int    sr_config_dirty_rect(void)      { return g_dirty_rect; }
int    sr_config_palette_subset(void)  { return g_palette_subset; }
int    sr_config_dirty_log(void)       { return g_dirty_log; }
int    sr_config_input_device(void)    { return g_input_device; }
int    sr_config_audio_enabled(void)   { return g_audio_enabled; }
double sr_config_audio_quality(void)   { return g_audio_quality; }

/* [video] aspect -- what SHAPE the core's frame should be drawn at.
 *
 *   core    (default)  what the core reports: the NES's 1.219 (8:7 pixel aspect),
 *                      FreeIntv's 1.5714. This is what every other libretro
 *                      frontend does, so a game looks the way it does elsewhere.
 *   4:3                what a television actually showed (1.333). Wider than the
 *                      NES's reported 1.219; some people prefer it, and CRT
 *                      overscan makes it arguably the more honest number.
 *   square             the raw framebuffer, pixels unstretched (256x240 = 1.067).
 *                      This was the door's behavior before M3, and it is a
 *                      QUARTER too narrow on the NES.
 *   <decimal>          any width/height you like, e.g. 1.25.
 *
 * Returns 0 for "square", which sr_io_set_aspect() reads as "assume square". */
const char *sr_config_aspect_mode(void) { return g_aspect; }

double sr_config_aspect(double core_aspect)
{
	const char *a = g_aspect;
	double      v;

	if (a[0] == '\0' || stricmp(a, "core") == 0)
		return core_aspect;
	if (stricmp(a, "square") == 0 || stricmp(a, "native") == 0 || stricmp(a, "1:1") == 0)
		return 0.0;
	if (strcmp(a, "4:3") == 0)
		return 4.0 / 3.0;
	if (strcmp(a, "16:9") == 0)
		return 16.0 / 9.0;

	v = atof(a);
	if (v >= 0.1 && v <= 10.0)
		return v;

	fprintf(stderr, "syncretro: [video] aspect \"%s\" not understood; using the core's\n", a);
	return core_aspect;
}
int    sr_config_audio_volume(void)    { return g_audio_volume; }
int    sr_config_audio_chunk_ms(void)  { return g_audio_chunk_ms; }
int    sr_config_audio_prebuffer(void) { return g_audio_prebuffer; }

/* Canonicalize `src` into `dst`, falling back to a verbatim copy when it can't
 * resolve. Returns 1 if `dst` came back absolute.
 *
 * xpdev's FULLPATH(), not POSIX realpath(): MSVC has no realpath, and this door
 * already links xpdev. FULLPATH is also the more correct tool here -- it
 * canonicalizes LEXICALLY (against the current cwd) and does NOT require the path
 * to exist, which is exactly what sr_config_apply() needs when it absolutizes the
 * -home sandbox before creating it. Note the argument order is _fullpath's
 * (dest, src, size). */
static int sr_absolutize(char *dst, size_t sz, const char *src)
{
	char abs[PATH_MAX];

	if (FULLPATH(abs, src, sizeof abs) != NULL) {
		snprintf(dst, sz, "%s", abs);
		return 1;   /* FULLPATH always yields an absolute path */
	}
	snprintf(dst, sz, "%s", src);
	return 0;
}

static int sr_exists(const char *path)
{
	return fexist(path);   /* xpdev: portable existence check (no POSIX access()) */
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
	snprintf(g_launch_dir, sizeof g_launch_dir, "%s", cwd);

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
		g_disc_is_rotate = sr_disc_matches(g_rom_path, g_disc_rotate);
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

int sr_config_disc_rotate(void)
{
	return g_disc_is_rotate;
}

const char *sr_config_launch_dir(void)
{
	return g_launch_dir[0] ? g_launch_dir : NULL;
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
