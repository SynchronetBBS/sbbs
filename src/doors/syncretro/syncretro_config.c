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

#include "dirwrap.h"   /* xpdev: mkpath(), FULLPATH(), fexist(), globi() */
#include "retro_core.h"      /* rc_core_ext(): the platform's core extension */
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
static int  g_pace_depth;       /* [video] pace_depth, 0 = auto (AIMD) */
static int  g_palette_subset = 1;   /* [video] palette_subset */
static int  g_dirty_log;        /* [debug] dirty_log, default FALSE */
static int  g_input_device;     /* [input] device: 0 = leave the core's default */
static char g_aspect[32] = "core";    /* [video] aspect: core|square|4:3|<decimal> */
static char g_core_path[PATH_MAX];    /* the .so to dlopen, absolute */
static char g_rom_path[PATH_MAX];     /* the cartridge, absolute */

/* [console] -- what console this install IS, read from syncretro.ini (or the
 * sysop's syncretro.local.ini overlay) by sr_config_read(). Read by the lobby
 * too: these are the facts both halves of the door need, which is exactly why
 * they cannot travel on the command line (260 bytes on Windows, truncated in
 * silence). Empty means the key is absent from both files, and each caller
 * falls back on its own. */
static char g_con_name[64];           /* [console] name:    "Intellivision" */
static char g_con_short[32];          /* [console] short:   "Intv" / "NES" */
static char g_con_core[128];          /* [console] core:    "freeintv_libretro" */
static char g_con_profile[32];        /* [console] profile: "intv" */

/* Widest console name a who's-online column will take before the short name
 * reads better: "Intellivision" fits, "Nintendo Entertainment System" does not.
 * exec/load/syncretro_lobby.js applies the SAME rule to the same two keys for
 * the line it logs -- the two must agree, or a cartridge is logged under one
 * console name and shown online under another. Change both together. */
#define SR_CONSOLE_LABEL_MAX 20

/* syncretro.ini, [idle] -- the idle-USER timeout and its countdown, in seconds.
 * Its own declaration group rather than joined to the block above: `unsigned` is
 * a wider type, and folding it in there re-aligns a dozen untouched lines. */
/* SR_IDLE_DEFAULT is the shipped policy, not just an ini fallback: with no
 * [idle] section at all a door still times an idle player out after 10 minutes.
 * Deliberately lenient -- a player reading a hint screen, watching an attract
 * loop or answering the door is not gone, and a false termination is far worse
 * than a node held a few extra minutes. Set `timeout = 0` to disable outright.
 *
 * exec/load/syncretro_lobby.js CARRIES THE SAME NUMBER for the lobby path (it
 * always passes -i, so its fallback governs there). The two must agree; change
 * both together. */
#define SR_IDLE_DEFAULT 600           /* 10 minutes */

static unsigned g_idle_timeout = SR_IDLE_DEFAULT;   /* [idle] timeout; 0 = off */
static unsigned g_idle_warn = 60;     /* [idle] warn */

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

/* The shipped file and the sysop's overlay resolve as if the local file's lines
 * were APPENDED to the shipped one: a key present in the local file wins at the
 * SAME SCOPE. So a local [video] key overrides the shipped [video] key -- but a
 * local root key does not reach past a shipped section key, because specificity
 * is decided before locality. Same rule, and the same reason, as
 * games.ini/games.local.ini; see sr_games_str() in syncretro_games.c. */
static str_list_t sr_config_read_one(const char *dir, const char *name)
{
	char       path[PATH_MAX];
	str_list_t ini;
	FILE *     f;

	snprintf(path, sizeof path, "%s/%s", dir, name);
	f = fopen(path, "r");
	if (f == NULL)
		return NULL;
	ini = iniReadFile(f);
	fclose(f);
	return ini;
}

static const char *sr_cfg_str(str_list_t base, str_list_t local,
                              const char *section, const char *key,
                              const char *deflt, char *val)
{
	if (local != NULL && iniKeyExists(local, section, key))
		return iniGetString(local, section, key, deflt, val);
	if (base != NULL)
		return iniGetString(base, section, key, deflt, val);
	snprintf(val, INI_MAX_VALUE_LEN, "%s", deflt);
	return val;
}

static int sr_cfg_bool(str_list_t base, str_list_t local, const char *section,
                       const char *key, int deflt)
{
	if (local != NULL && iniKeyExists(local, section, key))
		return iniGetBool(local, section, key, deflt);
	if (base != NULL)
		return iniGetBool(base, section, key, deflt);
	return deflt;
}

static long sr_cfg_int(str_list_t base, str_list_t local, const char *section,
                       const char *key, long deflt)
{
	if (local != NULL && iniKeyExists(local, section, key))
		return iniGetInteger(local, section, key, deflt);
	if (base != NULL)
		return iniGetInteger(base, section, key, deflt);
	return deflt;
}

static double sr_cfg_float(str_list_t base, str_list_t local,
                           const char *section, const char *key, double deflt)
{
	if (local != NULL && iniKeyExists(local, section, key))
		return iniGetFloat(local, section, key, deflt);
	if (base != NULL)
		return iniGetFloat(base, section, key, deflt);
	return deflt;
}

static double sr_cfg_duration(str_list_t base, str_list_t local,
                              const char *section, const char *key,
                              double deflt)
{
	if (local != NULL && iniKeyExists(local, section, key))
		return iniGetDuration(local, section, key, deflt);
	if (base != NULL)
		return iniGetDuration(base, section, key, deflt);
	return deflt;
}

/* [options] merges BY NAME: a name in the local file wins, names only in the
 * shipped file survive. sr_option_apply() keeps the LAST matching pin
 * (retro_options.c), so appending the local file's options after the shipped
 * file's is what makes the local one win -- the order here is load-bearing. */
static void sr_config_pin_options(str_list_t ini)
{
	named_string_t **opts;
	char             kv[INI_MAX_VALUE_LEN * 2];
	size_t           i;

	if (ini == NULL)
		return;
	opts = iniGetNamedStringList(ini, "options");
	if (opts == NULL)
		return;
	for (i = 0; opts[i] != NULL; i++) {
		snprintf(kv, sizeof kv, "%s=%s", opts[i]->name,
		         opts[i]->value != NULL ? opts[i]->value : "");
		sr_option_pin(kv);
	}
	iniFreeNamedStringList(opts);
}

void sr_config_read(const char *dir)
{
	str_list_t base;
	str_list_t local;
	char       val[INI_MAX_VALUE_LEN];

	if (dir == NULL)
		dir = ".";
	base  = sr_config_read_one(dir, "syncretro.ini");
	local = sr_config_read_one(dir, "syncretro.local.ini");

	sr_cfg_str(base, local, "console", "name", "", val);
	snprintf(g_con_name, sizeof g_con_name, "%s", val);
	sr_cfg_str(base, local, "console", "short", "", val);
	snprintf(g_con_short, sizeof g_con_short, "%s", val);
	sr_cfg_str(base, local, "console", "core", "", val);
	snprintf(g_con_core, sizeof g_con_core, "%s", val);
	sr_cfg_str(base, local, "console", "profile", "", val);
	snprintf(g_con_profile, sizeof g_con_profile, "%s", val);

	sr_cfg_str(base, local, "disc", "rotate", "", val);
	snprintf(g_disc_rotate, sizeof g_disc_rotate, "%s", val);
	sr_cfg_str(base, local, "video", "aspect", "core", val);
	snprintf(g_aspect, sizeof g_aspect, "%s", val);

	g_audio_enabled   = sr_cfg_bool(base, local, "audio", "enabled", TRUE);
	g_audio_quality   = sr_cfg_float(base, local, "audio", "quality",
	                                 TERMGFX_MUSIC_QUALITY_DEFAULT);
	g_audio_volume    = (int)sr_cfg_int(base, local, "audio", "volume", 100);
	g_audio_chunk_ms  = (int)sr_cfg_int(base, local, "audio", "chunk_ms", 100);
	g_audio_prebuffer = (int)sr_cfg_int(base, local, "audio", "prebuffer", 3);

	g_dirty_rect     = sr_cfg_bool(base, local, "video", "dirty_rect", TRUE);
	/* pace_depth -- how many frames may be in flight at once. 0 (the default)
	 * lets the AIMD pacer choose from the measured round-trip. Pinning it to 1
	 * is a diagnostic: on the sixel tier a terminal draws a frame
	 * progressively, so a second frame arriving mid-draw composites two frames
	 * on screen -- which reads as tearing on scrolling content. */
	g_pace_depth     = (int)sr_cfg_int(base, local, "video", "pace_depth", 0);
	g_palette_subset = sr_cfg_bool(base, local, "video", "palette_subset", TRUE);

	/* iniGetDuration() so "15m"/"900"/"1h" all work, and so a bare number means
	 * SECONDS here exactly as it does in the lobby. */
	g_idle_timeout = (unsigned)sr_cfg_duration(base, local, "idle", "timeout",
	                                           SR_IDLE_DEFAULT);
	g_idle_warn    = (unsigned)sr_cfg_duration(base, local, "idle", "warn", 60);

	g_dirty_log    = sr_cfg_bool(base, local, "debug", "dirty_log", FALSE);
	/* [input] device -- a RETRO_DEVICE id to hand the core for both ports. 0
	 * (the default) means say NOTHING, leaving whatever the core chose: a wrong
	 * device id silently rewires every button. */
	g_input_device = (int)sr_cfg_int(base, local, "input", "device", 0);

	sr_config_pin_options(base);
	sr_config_pin_options(local);

	strListFree(&base);
	strListFree(&local);

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
int    sr_config_pace_depth(void)      { return g_pace_depth; }
int    sr_config_palette_subset(void)  { return g_palette_subset; }
unsigned sr_config_idle_timeout(void)  { return g_idle_timeout; }
unsigned sr_config_idle_warn(void)     { return g_idle_warn; }
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

/* The core to load when nobody named one: the single "*_libretro<ext>" sitting
 * in the door's own directory.
 *
 * Every SyncRetro install is one console, and a console install holds exactly
 * one core -- which is why the lobby had been spending 28 characters of command
 * line naming a file the door is standing next to. That line is assembled by
 * Synchronet into a 260-byte buffer on Windows and truncated there in silence,
 * and the ROM argument, being last, is what falls off the end.
 *
 * Exactly one match is required. Zero leaves the core unset, and main.c reports
 * that as it always has. Two or more is a genuinely ambiguous install, so it
 * names them and leaves the core unset rather than picking: guessing wrong here
 * boots the wrong console. -core overrides all of this and is still the answer
 * for an install that keeps several. */
static int sr_glob_one(char *dst, size_t sz, const char *pattern, int complain)
{
	glob_t g;
	size_t i;
	int    found = 0;

	memset(&g, 0, sizeof g);
	if (globi(pattern, 0, NULL, &g) != 0)
		return 0;
	if (g.gl_pathc == 1) {
		snprintf(dst, sz, "%s", g.gl_pathv[0]);
		found = 1;
	} else if (g.gl_pathc > 1 && complain) {
		fprintf(stderr, "syncretro: %u libretro cores match %s -- name one with"
		        " [console] core in console.ini, or pass -core:\n",
		        (unsigned)g.gl_pathc, pattern);
		for (i = 0; i < g.gl_pathc; i++)
			fprintf(stderr, "syncretro:   %s\n", g.gl_pathv[i]);
	}
	globfree(&g);
	return found;
}

/* Find the core when the command line did not name one. `stem` is console.ini's
 * core name, or the wildcard "*_libretro" when it has none either -- a console
 * install holds exactly one core, which is why the lobby had been spending 28
 * characters of a 260-character command line naming a file the door is standing
 * next to.
 *
 * Both layouts are searched, because deploy.js and getcore.js may put the core
 * at the door root OR in the per-target "<os>-<arch>" sub-dir beside the binary,
 * and the door has no way to know which target string was used. Ambiguity is
 * only reported for the wildcard: an explicitly named core matching twice is not
 * a question anyone needs asking. */
static int sr_find_core(char *dst, size_t sz, const char *dir, const char *stem)
{
	char pattern[PATH_MAX];
	int  wild = (strchr(stem, '*') != NULL);

	snprintf(pattern, sizeof pattern, "%s/%s%s", dir, stem, rc_core_ext());
	if (sr_glob_one(dst, sz, pattern, wild))
		return 1;
	snprintf(pattern, sizeof pattern, "%s/*/%s%s", dir, stem, rc_core_ext());
	return sr_glob_one(dst, sz, pattern, wild);
}

int sr_config_apply(void)
{
	const char *sys  = getenv("SYNCRETRO_SYSTEM");
	const char *home = sr_door_home();
	const char *core = sr_door_core_path();
	const char *rom  = sr_door_rom_path();
	int         rc   = 0;
	char        cwd[PATH_MAX];
	char        corebuf[160];   /* console.ini's core name + the platform extension */
	/* cwd + '/' + "roms" + NUL: sized so the join below cannot truncate (and so
	 * GCC can see that, rather than warning about it under -Werror). */
	char        romdir[PATH_MAX + sizeof(SR_ROM_SUBDIR) + 1];

	/* The launch directory, captured before anything can move cwd. */
	if (getcwd(cwd, sizeof cwd) == NULL)
		snprintf(cwd, sizeof cwd, ".");
	snprintf(g_launch_dir, sizeof g_launch_dir, "%s", cwd);

	/* syncretro.ini lives in the launch directory, not the per-user sandbox --
	 * must run before the chdir() in step 3 below moves cwd there. */
	sr_config_read(cwd);

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
	/* -core wins, then console.ini's core name (spelled WITHOUT an extension, so
	 * one shipped file serves every platform), then the lone core in the install.
	 * The command line used to be the only source, at 28 characters a launch. */
	if (core != NULL) {
		const char *const dirs[] = { cwd, NULL };

		sr_resolve_file(g_core_path, sizeof g_core_path, core, dirs);
	} else {
		char        found[PATH_MAX];
		const char *stem = g_con_core[0] != '\0' ? g_con_core : "*_libretro";

		if (sr_find_core(found, sizeof found, cwd, stem)) {
			sr_absolutize(g_core_path, sizeof g_core_path, found);
		} else if (g_con_core[0] != '\0') {
			/* Named, but not installed. Record it anyway so the load failure
			 * names the file the sysop is missing rather than saying nothing. */
			const char *const dirs[] = { cwd, NULL };

			snprintf(corebuf, sizeof corebuf, "%s%s", g_con_core, rc_core_ext());
			sr_resolve_file(g_core_path, sizeof g_core_path, corebuf, dirs);
		}
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

const char *sr_config_console_name(void)
{
	if (g_con_name[0] != '\0' && strlen(g_con_name) <= SR_CONSOLE_LABEL_MAX)
		return g_con_name;
	if (g_con_short[0] != '\0')
		return g_con_short;
	return g_con_name[0] ? g_con_name : NULL;
}

const char *sr_config_profile(void)
{
	return g_con_profile[0] ? g_con_profile : NULL;
}

const char *sr_config_rom_path(void)
{
	return g_rom_path[0] ? g_rom_path : NULL;
}
