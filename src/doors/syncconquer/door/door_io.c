/*
 * door_io.c -- SyncConquer socket I/O, present/pacing pipeline, graphics
 * tiers, and the minimal terminal-probe input parser.
 *
 * Structure and proven patterns cloned from syncduke_io.c/syncduke_door.c
 * (src/doors/syncduke/): the pre-main argv-grabbing constructor (no way to
 * add an argument pass to the vendored engine's own main() -- FACTS.md #10),
 * the non-blocking staged output buffer, the DSR-ACK + AIMD pipeline-depth
 * pacing model (termgfx/pace.h), and the sticky per-user flag-file pattern.
 * Diverges from SyncDuke per DESIGN.md: full-res sixel ONLY (no half-res
 * mode), and a second SyncTERM-only APC tier (PPM) alongside JXL and sixel.
 *
 * Sink selection (checked once, at first use):
 *   SYNCALERT_SIXELOUT=<path>  capture mode -- every door_io_present() call
 *                              OVERWRITES the file with one self-contained
 *                              full-res sixel frame (palette included, no
 *                              pacing/de-dupe), for offline verification.
 *   -s<fd> / SYNCALERT_SOCK    the door/telnet client fd.
 *   <path>/door32.sys          Synchronet's DOOR32.SYS drop file (%f) --
 *                              a bare arg whose basename is "door32.sys"
 *                              self-triggers the read (socket handle, user
 *                              alias, time-left minutes; Task 5).
 *   (neither)                  stdout (fd 1) -- dev/tty testing.
 *
 * Task 5 (BBS integration) args, all resolved in the SAME pre-main
 * constructor as -s<fd>/-home, and all neutered out of the engine's own
 * argv before its Parse_Command_Line() ever scans it (door_sanitize_argv()):
 *   -assets <dir>   the REDALERT.MIX/MAIN.MIX directory; else <the real
 *                   binary's own directory>/assets. Validated (existence +
 *                   sane size) BEFORE the engine's main() is allowed to run
 *                   at all -- see door_validate_assets_or_die().
 *   -audiocache <dir>  SyncTERM-side transcoded-audio disk cache (Task 4);
 *                   unified with SYNCALERT_AUDIO_CACHE (below) and with
 *                   soundio_termgfx.cpp's independent resolution of the
 *                   same directory -- door_resolve_audio_cache_dir().
 * Env (Task 5, in addition to Task 1-4's SYNCALERT_SIXELOUT/SYNCALERT_SOCK):
 *   SBBSCTRL / SBBSDATA / SBBSNNUM / SBBSNODE   set by Synchronet for
 *                   external programs -- sbbs_node.h's who's-online/paging
 *                   (Ctrl-U/Ctrl-P) and this file's per-node debug log
 *                   (data/syncalert/syncalert_n<node>.log) and default audio-
 *                   cache dir (<data>/syncalert/audio).
 *   SYNCALERT_AUDIO_CACHE   read by BOTH this file and soundio_termgfx.cpp;
 *                   exported by door_audio_ensure_init() once resolved, so
 *                   both consumers agree regardless of how it was resolved.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>   /* sig_atomic_t + SIGTERM: ISO C, so not in the #ifdef below */

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #include <io.h>
  #include <process.h>
  #include <sys/stat.h>
  #include <time.h>
  #define door_stat_t struct _stat
  #define door_stat   _stat
#else
  #include <errno.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <time.h>
  #include <sys/socket.h>
  #include <sys/stat.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #define door_stat_t struct stat
  #define door_stat   stat
#endif

#include "door.h"
#include "door_io.h"
#include "door_input.h"
#include "door_node.h"   /* Task 5: sbbs_node status/Ctrl-U/Ctrl-P */
#include "door32.h"      /* termgfx: the shared DOOR32.SYS parser */
#include "sbbs_node.h"   /* Task 5: sbbs_my_node() for the per-node log filename */
#include "keymode.h"   /* termgfx: key-mode negotiation (shared with door_input.c) */
#include "sixel.h"
#include "jxl.h"
#include "apc.h"
#include "caps.h"
#include "text.h"
#include "term.h"
#include "geometry.h"
#include "pace.h"
#include "audio_mgr.h"   /* termgfx: SyncTERM audio-APC manager (Task 4, digital SFX/music) */

/* xpdev cross-platform wrappers (see CLAUDE.md: prefer xpdev over hand-rolled
 * #ifdef _WIN32). genwrap.h gives portable stricmp/strnicmp. The socket layer
 * still hand-rolls its Windows/POSIX split -- converting it to sockwrap.h is a
 * larger pass (the I/O also branches socket-vs-tty on POSIX, so it isn't a
 * clean swap) and is left as a focused follow-up, ideally with the M3
 * multiplayer work that builds its netgame on sockwrap. */
#include "genwrap.h"
#include "ini_file.h"   /* xpdev: merge (not clobber) the per-user REDALERT.INI */

#define DOOR_FB_BYTES ((size_t)DOOR_FB_WIDTH * DOOR_FB_HEIGHT)

/* --- door args: -s<fd> / -home <dir> / DOOR32.SYS / -assets <dir> ---------
 * The vendored engine owns main() and its own command-line parser
 * (Parse_Command_Line, redalert/init.cpp), and it's not worth patching --
 * verified it silently ignores any argv element it doesn't recognize (no
 * exact "-?"/"-h" match, no "-X..."/"-CD"/... prefix match), so a glibc
 * .init_array constructor can grab argc/argv before main() runs and the
 * engine simply never notices our extra flags -- PROVIDED we also neuter our
 * own flags out of argv before the engine's Parse_Command_Line scans it (Task
 * 5: that scan is a strstr() substring match over every argv token, so an
 * unsanitized door arg like an absolute -home path can accidentally contain
 * "-SEED"/"-CD"/etc and misfire an engine option -- door_sanitize_argv()
 * below). */
/* ---- per-title identity ---------------------------------------------------
 * The SyncConquer door family builds as two separate binaries from this one
 * shared door: VanillaRA -> syncalert (Red Alert), VanillaTD -> syncdawn
 * (Tiberian Dawn). The per-target compile define SYNCCONQUER_TD (CMakeLists)
 * selects the game's branding, its game-data (MIX) manifest (below), and the
 * per-node data dir / log-file / cache basename -- so the two titles never
 * collide in data/ or the client's SyncTERM cache. Env-var names (SYNCALERT_*)
 * stay shared: they're internal/dev knobs, not sysop-facing. */
#if defined(SYNCCONQUER_TD)
#  define DOOR_SHORT_NAME   "syncdawn"
#  define DOOR_DISPLAY_NAME "SyncDawn"
#  define DOOR_GAME_NAME    "Tiberian Dawn"
#else   /* Red Alert (default) */
#  define DOOR_SHORT_NAME   "syncalert"
#  define DOOR_DISPLAY_NAME "SyncAlert"
#  define DOOR_GAME_NAME    "Red Alert"
#endif

static int g_argc;
static char **  g_argv;
static int      g_args_resolved;
static int      g_cli_sock = -1;
static char     g_home[512];
static char     g_audiocache[512];  /* -audiocache <dir> / SYNCALERT_AUDIO_CACHE (Task 4) */
static char     g_assets[512];      /* -assets <dir>; else <binary dir>/assets (door_resolve_assets_dir) */
static char     g_bindir[512];      /* the door binary's own directory (door_resolve_assets_dir); where <door>.ini lives */
static char     g_alias[64];        /* DOOR32.SYS line 7 (user alias), for node paging */
static uint32_t g_door_time_left_ms;/* DOOR32.SYS line 9 (minutes) / 0 = unknown */

static void door_grab_args(int argc, char **argv)
{
	g_argc = argc;
	g_argv = argv;
}

static void door_resolve_args(void);
static void door_early_setup(void);   /* asset validation, UserPath/DataPath, argv sanitize -- defined below door_mkpath() */

#ifdef _MSC_VER
/* MSVC has no __attribute__((constructor)); register a CRT initializer in
 * the .CRT$XCU table instead (walked before main()). __argc/__argv are
 * already populated by then. */
static void door_ctor(void)
{
	door_grab_args(__argc, __argv);
	door_resolve_args();
	door_early_setup();
}
#pragma section(".CRT$XCU", read)
__declspec(allocate(".CRT$XCU")) void (*door_ctor_p)(void) = door_ctor;
#if defined(_WIN64)
#pragma comment(linker, "/include:door_ctor_p")
#else
#pragma comment(linker, "/include:_door_ctor_p")
#endif
#else
/* glibc/clang pass (argc, argv, envp) to .init_array constructors. Args are
 * fully resolved (including asset validation / UserPath+DataPath / argv
 * sanitize) here too, BEFORE the vendored main() ever runs -- required for
 * the UserPath/DataPath redirection (the engine reads them at the very top
 * of main(), Paths.Init(), FACTS.md #9) and for the asset check to make the
 * missing-asset SIGSEGV genuinely unreachable rather than just early. */
__attribute__((constructor))
static void door_grab_args_ctor(int argc, char **argv, char **envp)
{
	(void)envp;
	door_grab_args(argc, argv);
	door_resolve_args();
	door_early_setup();
}
#endif

/* basename(path) == "door32.sys", case-insensitive (Synchronet's %f drop
 * file -- syncduke_door.c's is_door32_path()). */
static int door_is_door32_path(const char *s)
{
	const char *b = s + strlen(s);
	while (b > s && b[-1] != '/' && b[-1] != '\\')
		b--;
	return stricmp(b, "door32.sys") == 0;   /* xpdev genwrap.h: portable */
}

/* The drop file is parsed by termgfx (door32.c), shared with the sibling doors --
 * this was the fifth copy of the same twenty lines, and the comment above it
 * said as much ("Mirrors syncduke_door.c's read_door32()"). */
static void door_read_door32(const char *path)
{
	termgfx_door32_t d;
	const char      *why;

	if (termgfx_door32_read(path, &d) != 0)
		return;

	if (d.socket >= 0)
		g_cli_sock = d.socket;
	else if (d.stdio)
		/* A STDIO door: the BBS dup2'd our stderr onto the player's stream, so any
		 * diagnostic would be painted over the game. Send stderr to a file. */
		termgfx_stderr_capture(DOOR_SHORT_NAME, d.node);
	/* comm type 0 (local) needs no action: with no socket, door_io writes fd 1 and
	 * reads fd 0 -- which IS a stdio door (Synchronet's XTRN_STDIO, Mystic on
	 * *nix). Windows has no such path, and a Windows BBS hands a door a socket. */

	if (d.alias[0] != '\0')
		snprintf(g_alias, sizeof g_alias, "%s", d.alias);
	if (d.time_limit_ms > 0)
		g_door_time_left_ms = d.time_limit_ms;

	/* Say why a drop file is no use to us, rather than falling silently through to
	 * a sink the player cannot see. */
	if ((why = termgfx_door32_why_unusable(&d)) != NULL)
		fprintf(stderr, DOOR_SHORT_NAME ": %s is no use to this door: %s\n", path, why);
}

static void door_resolve_args(void)
{
	int i;

	if (g_args_resolved)
		return;
	g_args_resolved = 1;
	for (i = 1; i < g_argc; i++) {
		const char *a = g_argv[i];
		if (a[0] == '-' && a[1] == 's' && a[2] != '\0')
			g_cli_sock = atoi(a + 2);                                   /* -s<fd> */
		else if (strcmp(a, "-home") == 0 && i + 1 < g_argc)
			snprintf(g_home, sizeof g_home, "%s", g_argv[++i]);         /* -home <dir> */
		else if (strcmp(a, "-audiocache") == 0 && i + 1 < g_argc)
			snprintf(g_audiocache, sizeof g_audiocache, "%s", g_argv[++i]); /* -audiocache <dir> */
		else if (strcmp(a, "-assets") == 0 && i + 1 < g_argc)
			snprintf(g_assets, sizeof g_assets, "%s", g_argv[++i]);     /* -assets <dir> */
		else if (door_is_door32_path(a))
			door_read_door32(a);                                        /* Synchronet's %f drop file */
	}
	/* No -home: fall back to the launch directory, the way SyncDuke and
	 * SyncDOOM do. Anything else leaves door_setup_engine_paths() with no
	 * REDALERT.INI to write, so the engine never learns DataPath and dies in
	 * Bootstrap() with "failed to cache required mixfile 'LOCAL.MIX'" -- even
	 * with a perfectly good assets dir sitting right next to the binary. */
	if (g_home[0] == '\0')
		snprintf(g_home, sizeof g_home, "%s", ".");
}

/* Recursive mkdir (POSIX) / CreateDirectory (Win32) -- the ONLY thing this
 * door needs from a full mkpath()/dirwrap.h, so it gets its own tiny local
 * helper instead of pulling in xpdev (not otherwise linked here -- Task 5,
 * BBS integration, is expected to bring xpdev in for real; revisit then).
 * Tolerant of an already-existing directory at any level. */
static void door_mkpath(const char *dir)
{
	char   tmp[600];
	size_t i, n;

	n = snprintf(tmp, sizeof tmp, "%s", dir);
	if (n == 0 || n >= sizeof tmp)
		return;
	for (i = 1; i < n; i++) {
		if (tmp[i] != '/' && tmp[i] != '\\')
			continue;
		tmp[i] = '\0';
#ifdef _WIN32
		(void)CreateDirectoryA(tmp, NULL);
#else
		(void)mkdir(tmp, 0755);
#endif
		tmp[i] = '/';
	}
#ifdef _WIN32
	(void)CreateDirectoryA(tmp, NULL);
#else
	(void)mkdir(tmp, 0755);
#endif
}

/* Portable realpath(): POSIX realpath() / Win32 _fullpath(). `in` must exist
 * (both reject a nonexistent path); returns 1 on success, `out` untouched
 * (caller should leave its own default) on failure.
 *
 * The POSIX side MUST pass a NULL buffer (POSIX.1-2008: realpath() malloc()s
 * the result) rather than a local array. glibc requires a caller-supplied
 * buffer to hold at least PATH_MAX bytes, and _FORTIFY_SOURCE enforces that
 * with __realpath_chk(), which abort()s -- "*** buffer overflow detected ***"
 * -- whenever the destination's known object size is under PATH_MAX, no matter
 * how short the resolved path actually is. A `char buf[1024]` here therefore
 * killed the door at startup on every distro whose gcc defaults to
 * -D_FORTIFY_SOURCE (Ubuntu), while running fine where it does not (Debian). */
static int door_realpath(const char *in, char *out, size_t outsz)
{
#ifdef _WIN32
	char buf[_MAX_PATH];

	if (_fullpath(buf, in, sizeof buf) == NULL)
		return 0;
	snprintf(out, outsz, "%s", buf);
#else
	char *buf = realpath(in, NULL);

	if (buf == NULL)
		return 0;
	snprintf(out, outsz, "%s", buf);
	free(buf);
#endif
	return 1;
}

/* On Windows the DOOR32.SYS handle is a Winsock SOCKET, and Winsock must be
 * initialized per-process before ANY send()/recv() on it -- an uninitialized
 * process gets WSANOTINITIALISED (10093) on the first call, which the pump
 * reads as a client hangup and exits, dumping the caller straight back to the
 * BBS. xpdev's sockwrap doesn't self-init Winsock (see syncdoom.c), so bring
 * it up ourselves, once, from the pre-main constructor -- before door_io_init()
 * and before the engine's main(). Idempotent: safe to call from both
 * door_early_setup() and door_early_write(). No WSACleanup(); the door owns
 * Winsock for its whole lifetime and process exit releases it. */
static void door_wsa_ensure(void)
{
#ifdef _WIN32
	static int done;
	WSADATA    wsa;

	if (done || g_cli_sock < 0)
		return;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0)
		done = 1;
#endif
}

/* Raw write straight to the client fd/socket, no staging buffer -- used only
 * this early (pre-main constructor), before door_io_init() has resolved
 * g_fd/g_iosock or allocated the g_out staging buffer. Mirrors syncduke_
 * door.c's syncduke_door_splash() early-write pattern. `g_cli_sock` is
 * already resolved by the time this can run (door_resolve_args() ran first
 * in the same constructor). */
static void door_early_write(const char *s)
{
	size_t len = strlen(s);
#ifdef _WIN32
	if (g_cli_sock >= 0) {
		door_wsa_ensure();
		(void)send((SOCKET)g_cli_sock, s, (int)len, 0);
		return;
	}
	(void)_write(1, s, (unsigned)len);
#else
	int fd = (g_cli_sock >= 0) ? g_cli_sock : 1;
	signal(SIGPIPE, SIG_IGN);
	(void)write(fd, s, len);
#endif
}

/* --- Task 5: asset validation, before ANY engine code runs -----------------
 * Without REDALERT.MIX/MAIN.MIX in the resolved assets dir, the vendored
 * engine SIGSEGVs deep inside MFCD::Retrieve()'s callers (init.cpp's
 * Init_Game() opens both unconditionally, and dozens of *data.cpp callers
 * cast its NULL return straight into a shape/palette pointer with no NULL
 * check -- e.g. redalert/aadata.cpp's LRotorData = MFCD::Retrieve(...)).
 * Checked and enforced HERE, in the pre-main constructor, so a missing/
 * corrupt asset set is refused before the engine's own main() is ever
 * called -- the crash is unreachable, not just avoided in the common case. */
#define DOOR_MIN_REDALERT_MIX (5L * 1024 * 1024)     /* real file: ~25MB */
#define DOOR_MIN_MAIN_MIX     (50L * 1024 * 1024)    /* real file: ~454MB */

/* Per-title required-asset manifest: the MIX file(s) whose presence proves the
 * game data is installed, plus a size floor that rejects a truncated/stub file.
 * SYNCCONQUER_TD (syncdawn) needs CONQUER.MIX; the default (syncalert) needs
 * REDALERT.MIX + MAIN.MIX. door_assets_valid() / door_validate_assets_or_die()
 * iterate this so the door stays title-agnostic. */
typedef struct {
	const char *name;
	long        min_bytes;
} door_asset_req_t;
#if defined(SYNCCONQUER_TD)
static const door_asset_req_t DOOR_ASSET_REQ[] = {
	{ "CONQUER.MIX", 512L * 1024 },          /* real file ~2.4MB; floor rejects a stub */
};
#else
static const door_asset_req_t DOOR_ASSET_REQ[] = {
	{ "REDALERT.MIX", DOOR_MIN_REDALERT_MIX },
	{ "MAIN.MIX", DOOR_MIN_MAIN_MIX },
};
#endif
#define DOOR_ASSET_REQ_N ((int)(sizeof DOOR_ASSET_REQ / sizeof DOOR_ASSET_REQ[0]))

#define DOOR_ASSET_OK         1
#define DOOR_ASSET_MISSING    0     /* absent or below the size floor */
#define DOOR_ASSET_UNREADABLE (-1)  /* present but open() fails (permissions) */

static int door_check_asset(const char *dir, const char *name, long min_bytes)
{
	char        path[700];
	door_stat_t st;
	FILE *      fp;
	snprintf(path, sizeof path, "%s/%s", dir, name);
	if (door_stat(path, &st) != 0 || st.st_size < min_bytes)
		return DOOR_ASSET_MISSING;
	/* stat() alone is not enough: over an SMB/CIFS mount the mode bits are
	 * fabricated client-side (file_mode=), so a file the server refuses to
	 * open (e.g. the mode-400 MIX files bsdtar extracts from the RA95 ISO)
	 * still stats fine here and only fails at the engine's own fopen() --
	 * which the engine treats as file-not-found and later SIGSEGVs on.
	 * Probe readability with a real open. */
	if ((fp = fopen(path, "rb")) == NULL)
		return DOOR_ASSET_UNREADABLE;
	fclose(fp);
	return DOOR_ASSET_OK;
}

static int door_assets_valid(const char *dir)
{
	int i;

	if (dir[0] == '\0')
		return 0;
	for (i = 0; i < DOOR_ASSET_REQ_N; i++)
		if (door_check_asset(dir, DOOR_ASSET_REQ[i].name, DOOR_ASSET_REQ[i].min_bytes) != DOOR_ASSET_OK)
			return 0;
	return 1;
}

static const char *door_asset_desc(int status)
{
	if (status == DOOR_ASSET_OK)
		return "OK";
	if (status == DOOR_ASSET_UNREADABLE)
		return "unreadable (permissions)";
	return "missing/too small";
}

/* -assets <dir> if explicitly given (wins outright, no fallback), else a
 * short candidate cascade, first match wins:
 *   1. <the real binary's own directory>/assets -- "the argv/data dir"
 *      (DESIGN.md), matching xtrn/syncalert/assets/ next to the deployed
 *      binary/symlink; the documented, primary mechanism for a real install.
 *   2. <the real binary's own directory> itself -- syncduke's simpler
 *      beside-the-binary convention, in case a sysop drops the MIX files
 *      straight next to the binary instead of in an assets/ subdir.
 *   3. ./assets and 4. . (the launch CWD) -- dev/test convenience only (the
 *      Task 2-4 harnesses' cwd=<assets dir> launch convention keeps working
 *      unmodified).
 * MUST run before door_sanitize_argv() (which blanks -assets) and before
 * door_setup_engine_paths() (which mutates g_argv[0]) -- both would
 * otherwise erase the information this needs. */
static void door_resolve_assets_dir(void)
{
	char abs[600];
	char bindir[600];

	bindir[0] = '\0';
	if (g_argc > 0 && g_argv[0] != NULL && door_realpath(g_argv[0], abs, sizeof abs)) {
		char *slash = strrchr(abs, '/');
#ifdef _WIN32
		char *bslash = strrchr(abs, '\\');
		if (bslash != NULL && (slash == NULL || bslash > slash))
			slash = bslash;
#endif
		if (slash != NULL)
			*slash = '\0';
		snprintf(bindir, sizeof bindir, "%s", abs);
		snprintf(g_bindir, sizeof g_bindir, "%s", bindir);   /* remembered for <door>.ini (door_config_no_movies) */
	}

	if (g_assets[0] != '\0') {           /* -assets <dir>: resolve to absolute now, no fallback */
		if (door_realpath(g_assets, abs, sizeof abs))
			snprintf(g_assets, sizeof g_assets, "%s", abs);
		return;
	}

	if (bindir[0] != '\0') {
		snprintf(g_assets, sizeof g_assets, "%s/assets", bindir);
		if (door_assets_valid(g_assets))
			return;
		snprintf(g_assets, sizeof g_assets, "%s", bindir);
		if (door_assets_valid(g_assets))
			return;
	}
	/* The cwd-relative candidates must be made absolute here: the resolved
	 * dir outlives the launch cwd (door_setup_engine_paths() chdir()s to
	 * the per-user home and writes the dir into REDALERT.INI [Paths]), so
	 * a relative "./assets" would be resolved against the wrong directory
	 * by the engine. */
	if (door_assets_valid("./assets") && door_realpath("./assets", abs, sizeof abs)) {
		snprintf(g_assets, sizeof g_assets, "%s", abs);
		return;
	}
	if (door_assets_valid(".") && door_realpath(".", abs, sizeof abs)) {
		snprintf(g_assets, sizeof g_assets, "%s", abs);
		return;
	}
	/* Nothing validated: leave g_assets at its last (most descriptive)
	 * candidate -- <bindir>/assets if we had one -- so the fatal message
	 * below names a real, sysop-actionable path rather than "." or "". */
	if (bindir[0] != '\0')
		snprintf(g_assets, sizeof g_assets, "%s/assets", bindir);
}

/* The resolved game-data (MIX) directory -- the door's -assets dir. Exposed to
 * the engine because Tiberian Dawn's Init_Game must add it to the CCFileClass
 * file-search path before opening any mix: the engine CWD is the per-user -home
 * dir (where savegames + <game>.INI are written), NOT where the shared,
 * read-only game data lives. Red Alert bridges the same gap via its PathsClass
 * DataPath (written into REDALERT.INI by door_setup_engine_paths); TD's engine
 * never calls Paths.Init / Parse_Command_Line, so it reads this directly.
 * Returns "" until door_resolve_assets_dir() has run (pre-main constructor). */
const char *door_engine_data_dir(void)
{
	return g_assets;
}

static void door_validate_assets_or_die(void)
{
	int  i, all_ok = 1;
	char names[256];        /* "REDALERT.MIX and MAIN.MIX" / "CONQUER.MIX" */
	char statuses[512];     /* "REDALERT.MIX=OK, MAIN.MIX=missing/too small" */

	if (g_assets[0] == '\0') {
		door_early_write(
			"\r\n\x1b[1;31m" DOOR_DISPLAY_NAME ": can't locate the assets directory "
			"(no -assets given and the binary's own directory couldn't be "
			"resolved).\x1b[0m\r\n"
			"Ask the sysop to check the door's install (deploy / -assets).\r\n");
		fprintf(stderr, DOOR_SHORT_NAME ": FATAL: no assets dir resolved -- refusing to start\n");
		_exit(1);
	}

	names[0] = statuses[0] = '\0';
	for (i = 0; i < DOOR_ASSET_REQ_N; i++) {
		int    status = door_check_asset(g_assets, DOOR_ASSET_REQ[i].name, DOOR_ASSET_REQ[i].min_bytes);
		size_t nl = strlen(names), sl = strlen(statuses);
		if (status != DOOR_ASSET_OK)
			all_ok = 0;
		snprintf(names + nl, sizeof names - nl, "%s%s", i ? " and " : "", DOOR_ASSET_REQ[i].name);
		snprintf(statuses + sl, sizeof statuses - sl, "%s%s=%s", i ? ", " : "",
		         DOOR_ASSET_REQ[i].name, door_asset_desc(status));
	}
	if (all_ok)
		return;
	{
		char msg[900];
		snprintf(msg, sizeof msg,
		         "\r\n\x1b[1;31m" DOOR_DISPLAY_NAME ": " DOOR_GAME_NAME " game data is missing "
		         "or unusable.\x1b[0m\r\n"
		         "Expected readable %s in:\r\n  %s\r\n"
		         "Ask the sysop to run xtrn/" DOOR_SHORT_NAME "/fetch-assets.js to install "
		         "the freeware game data\r\n"
		         "(and to make sure they are readable by the BBS user).\r\n"
		         "(%s)\r\n",
		         names, g_assets, statuses);
		door_early_write(msg);
	}
	fprintf(stderr, DOOR_SHORT_NAME ": FATAL: assets missing/invalid in '%s' (%s) "
	        "-- refusing to start\n", g_assets, statuses);
	_exit(1);
}

/* Write into `out` a relative path from directory `from` to directory `to`
 * (both absolute). Lets the per-user REDALERT.INI store DataPath relative to the
 * per-user home -- which is also the engine's CWD -- instead of a host-specific
 * absolute path. That shared file is opened on both Vertrauen hosts (Linux
 * "/sbbs/..." and Windows "S:\\sbbs\\..."), where an absolute path from one host
 * is wrong on the other; the relative form is identical on both. Splits on
 * either separator, emits '/' (accepted by both engine builds), and returns 0
 * (caller keeps the absolute path) if the two share no common root -- e.g. a
 * different Windows drive. */
static int door_relpath(const char *from, const char *to, char *out, size_t outsz)
{
	char   fbuf[600], tbuf[600], *p;
	char * fc[80], *tc[80];
	int    fn = 0, tn = 0, i, common;
	size_t pos = 0;

	if (from == NULL || to == NULL || out == NULL || outsz == 0)
		return 0;
	snprintf(fbuf, sizeof fbuf, "%s", from);
	snprintf(tbuf, sizeof tbuf, "%s", to);

	/* Split into components in place, dropping empties so a leading separator
	 * and any doubled separators collapse (a "C:" drive stays its own token). */
	for (p = fbuf; *p != '\0'; ) {
		while (*p == '/' || *p == '\\')
			*p++ = '\0';
		if (*p == '\0')
			break;
		if (fn < (int)(sizeof fc / sizeof fc[0]))
			fc[fn++] = p;
		while (*p != '\0' && *p != '/' && *p != '\\')
			p++;
	}
	for (p = tbuf; *p != '\0'; ) {
		while (*p == '/' || *p == '\\')
			*p++ = '\0';
		if (*p == '\0')
			break;
		if (tn < (int)(sizeof tc / sizeof tc[0]))
			tc[tn++] = p;
		while (*p != '\0' && *p != '/' && *p != '\\')
			p++;
	}

	/* Length of the shared leading path (both paths come from the same door's
	 * realpath(), so a plain strcmp of the install-root components suffices). */
	for (common = 0; common < fn && common < tn && strcmp(fc[common], tc[common]) == 0; )
		common++;
	if (common == 0)
		return 0;   /* no shared root -> caller keeps the absolute path */

	out[0] = '\0';
	for (i = common; i < fn; i++) {   /* ".." up out of each leftover 'from' dir */
		int n = snprintf(out + pos, outsz - pos, "../");
		if (n < 0 || (size_t)n >= outsz - pos)
			return 0;
		pos += (size_t)n;
	}
	for (i = common; i < tn; i++) {   /* then down into the leftover 'to' dirs */
		int n = snprintf(out + pos, outsz - pos, "%s%s", tc[i], (i + 1 < tn) ? "/" : "");
		if (n < 0 || (size_t)n >= outsz - pos)
			return 0;
		pos += (size_t)n;
	}
	if (pos == 0)
		snprintf(out, outsz, ".");   /* same directory */
	return 1;
}

/* --- Task 5: per-user engine paths (FACTS.md #9) ---------------------------
 * The vendored Paths::Init("vanillara", "REDALERT.INI", "REDALERT.MIX",
 * args.ArgV[0]) (redalert/startup.cpp) resolves UserPath (where redalert.ini
 * and savegames land) and DataPath (where REDALERT.MIX/MAIN.MIX are read
 * from) by searching for a REDALERT.INI [Paths] override next to argv[0]'s
 * OWN directory first (PathsClass::Argv_Path(): realpath(dirname(argv[0]))
 * on POSIX) -- with no per-process env-var override on Windows
 * (PathsClass::User_Path() there is hardwired to CSIDL_APPDATA), argv[0] is
 * the only per-process (race-free between concurrently-running players)
 * lever available on BOTH platforms, so this writes a small REDALERT.INI
 * into the per-user -home dir and then rewrites g_argv[0] (the SAME memory
 * the vendored main() will read moments later) to a fabricated path whose
 * directory IS -home, so Argv_Path() finds it there. No vendored source
 * touched. Verified empirically on POSIX (task report); the Windows side is
 * best-effort/unverified -- see the task report's concerns (the WinMain
 * shim at redalert/startup.cpp:229 overwrites argv[0] itself on that build
 * configuration, which this environment can't build or test). */
static void door_setup_engine_paths(void)
{
	char  ini_path[700];
	char  abs_home[600];
	FILE *f;

	if (g_home[0] == '\0')
		return;   /* unreachable: door_resolve_args() defaults g_home to "." */

	door_mkpath(g_home);
#ifndef _WIN32
	if (chdir(g_home) != 0)
		fprintf(stderr, DOOR_SHORT_NAME ": cannot enter per-user dir '%s'\n", g_home);
#else
	(void)_chdir(g_home);
#endif
	if (!door_realpath(g_home, abs_home, sizeof abs_home))
		snprintf(abs_home, sizeof abs_home, "%s", g_home);   /* fall back to as-given */

	snprintf(ini_path, sizeof ini_path, "%s/REDALERT.INI", abs_home);
	/* MERGE, don't clobber. The old code rewrote this file from scratch every
	 * launch with only [Paths], which wiped the engine's own persisted settings
	 * (music/sound volume, etc.) AND left [Intro] PlayIntro absent -- so it
	 * defaulted true and the door auto-started a game (Special.IsFromInstall)
	 * every session instead of showing the main menu. Now: read the existing
	 * INI (xpdev), (re)write only the runtime [Paths], and seed [Intro]
	 * PlayIntro=false ONLY when it's absent so a fresh install boots to the menu
	 * while a sysop's (or the engine's own) later value survives. iniWriteFile()
	 * rewinds+truncates, so everything else in the file is preserved verbatim. */
	f = iniOpenFile(ini_path, TRUE);   /* r+ if it exists, w+ if new */
	if (f != NULL) {
		str_list_t ini = iniReadFile(f);

		/* UserPath is redundant: the argv[0] rewrite below makes PathsClass
		 * derive UserPath = the per-user home on its own (it falls back to
		 * Argv_Path() when [Paths]UserPath is absent). Don't bake an absolute
		 * path into this shared file -- and actively drop any that a previous
		 * build wrote, so stale absolutes don't linger. */
		iniRemoveKey(&ini, "Paths", "UserPath");
		if (g_assets[0] != '\0') {
			char rel[600];
			/* Store DataPath RELATIVE to the per-user home (the engine's CWD),
			 * never absolute: this INI is opened on both Vertrauen hosts and an
			 * absolute path from one is wrong on the other. Fall back to the
			 * absolute path only if a relative one can't be formed (no shared
			 * root, e.g. a different Windows drive). iniSetString overwrites any
			 * absolute DataPath a previous build left here. */
			if (door_relpath(abs_home, g_assets, rel, sizeof rel))
				iniSetString(&ini, "Paths", "DataPath", rel, NULL);
			else
				iniSetString(&ini, "Paths", "DataPath", g_assets, NULL);
		}
		if (!iniKeyExists(ini, "Intro", "PlayIntro"))
			iniSetBool(&ini, "Intro", "PlayIntro", FALSE, NULL);
		iniWriteFile(f, ini);
		iniFreeStringList(ini);
		fclose(f);
	} else
		fprintf(stderr, DOOR_SHORT_NAME ": couldn't open '%s' (UserPath redirection will fall back to the engine default)\n", ini_path);

	/* Rewrite argv[0] so PathsClass::Argv_Path() resolves -home as the
	 * search directory for the REDALERT.INI just written above. The
	 * fabricated filename need not exist -- Argv_Path() only realpath()s
	 * the DIRECTORY portion (strips the trailing component first). A
	 * process-lifetime leak (never freed) is fine -- one small strdup,
	 * one process, exits soon after the game does. */
	if (g_argc > 0) {
		char   fabricated[700];
		size_t n = snprintf(fabricated, sizeof fabricated, "%s/" DOOR_SHORT_NAME, abs_home);
		if (n > 0 && n < sizeof fabricated) {
			char *dup = strdup(fabricated);
			if (dup != NULL)
				g_argv[0] = dup;
		}
	}
}

/* --- Task 5: clean argv for the engine --------------------------------------
 * Parse_Command_Line (redalert/init.cpp:1438-1500) matches most of its
 * options via strstr(argv[i], "-XXX") -- a raw SUBSTRING scan over every
 * argv token, not just the ones it expects -- so an unsanitized door arg
 * (an absolute -home path, a DOOR32.SYS drop-file path, ...) could
 * accidentally contain "-CD"/"-SEED"/"-STEALTH"/"-MESSAGES"/"-ATTRACT"/
 * "-NOMOVIES" as a substring and silently mis-set an engine option. Rather
 * than shrink argc (which the vendored main() reads as a byval int from the
 * CRT/kernel BEFORE any constructor runs -- our own argc parameter here is
 * just a local copy, and there is no vendored call site to hand a mutable
 * `int *argc` through, unlike syncduke's game.c patch), every door-owned
 * token is NEUTERED in place: its g_argv[] slot is overwritten to point at
 * a static empty string, which fails every one of Parse_Command_Line's
 * checks harmlessly. -NOMOVIES is no longer forced by default (FMV cutscenes
 * play now); it's injected into the first neutered slot ONLY when the sysop
 * sets SYNCALERT_NOMOVIES (see below). */
/* Sysop config for movie suppression lives in the door's .ini, NOT an env var:
 * a sysop can't set an env var per-door (bbs.exec() uses execvp with the BBS's
 * own environment, no shell), so door behavior is configured via <door>.ini
 * beside the binary -- "[game] movies = false" suppresses FMV (heavy
 * full-motion bandwidth). Default is movies ON. SYNCALERT_NOMOVIES remains a
 * dev/testing override only. Returns 1 to suppress (inject -NOMOVIES), else 0.
 * The .ini is named after the binary: syncalert.ini / syncdawn.ini. */
static int door_config_no_movies(void)
{
	char        path[700];
	char        base[128];
	const char *p;
	FILE *      f;
	int         movies = TRUE;   /* default: play movies */

	if (getenv("SYNCALERT_NOMOVIES") != NULL)   /* dev/testing override */
		return 1;
	if (g_bindir[0] == '\0' || g_argc <= 0 || g_argv[0] == NULL)
		return 0;
	p = strrchr(g_argv[0], '/');
#ifdef _WIN32
	{
		const char *b = strrchr(g_argv[0], '\\');
		if (b != NULL && (p == NULL || b > p))
			p = b;
	}
#endif
	p = (p != NULL) ? p + 1 : g_argv[0];
	snprintf(base, sizeof base, "%s", p);
	{
		char *dot = strrchr(base, '.');   /* strip .exe */
		if (dot != NULL)
			*dot = '\0';
	}
	if (base[0] == '\0')
		return 0;
	snprintf(path, sizeof path, "%s/%s.ini", g_bindir, base);
	f = iniOpenFile(path, FALSE);   /* read-only; do NOT create */
	if (f != NULL) {
		movies = iniReadBool(f, "game", "movies", TRUE);
		fclose(f);
	}
	return movies ? 0 : 1;
}

static void door_sanitize_argv(void)
{
	static const char empty[] = "";
	int               i;
	int               movies_slot = -1; /* first neutered flag slot -- repurposed for -NOMOVIES */

	for (i = 1; i < g_argc; i++) {
		char *a          = g_argv[i];
		int   flag_index = i;      /* the flag token's own slot (before any value skip) */
		int   neuter      = 0;

		if (a[0] == '-' && a[1] == 's' && a[2] != '\0')
			neuter = 1;                                    /* -s<fd> */
		else if (strcmp(a, "-home") == 0 || strcmp(a, "-audiocache") == 0
		         || strcmp(a, "-assets") == 0) {
			neuter = 1;
			if (i + 1 < g_argc) {
				g_argv[i + 1] = (char *)empty;              /* also blank the value */
				i++;
			}
		} else if (door_is_door32_path(a))
			neuter = 1;                                     /* Synchronet's %f drop file */

		if (neuter) {
			if (movies_slot < 0)
				movies_slot = flag_index;
			g_argv[flag_index] = (char *)empty;
		}
	}
	/* FMV cutscenes default ON. A sysop can suppress them (silent VQA + heavy
	 * full-motion bandwidth) via the door's .ini -- "[game] movies = false"
	 * (door_config_no_movies), NOT an env var. When suppressing, -NOMOVIES goes
	 * into the first neutered slot so it's present exactly once regardless of
	 * the sysop's command line. */
	if (door_config_no_movies()) {
		if (movies_slot > 0)
			g_argv[movies_slot] = (char *)"-NOMOVIES";
		else
			fprintf(stderr, DOOR_SHORT_NAME ": movies disabled ([game] movies=false) but no "
			        "door-arg slot free to force -NOMOVIES (a bare/no-arg dev launch) -- "
			        "movies NOT suppressed this run\n");
	}

	/* Log the EXACT argv the engine's Parse_Command_Line() is about to scan
	 * -- the most direct way to verify door args never reach it (a build-
	 * time trace, not a temporary one: cheap, and useful for any future
	 * "why did the engine misparse an arg" report too). */
	{
		int  k;
		char logbuf[512];
		int  n = snprintf(logbuf, sizeof logbuf, DOOR_SHORT_NAME ": engine argv[%d] =", g_argc);
		for (k = 0; k < g_argc && n < (int)sizeof logbuf; k++)
			n += snprintf(logbuf + n, sizeof logbuf - n, " '%s'", g_argv[k]);
		fprintf(stderr, "%s\n", logbuf);
	}
}

/* --- Task 5: per-node debug log, syncduke convention -----------------------
 * data/syncalert/syncalert_n<node>.log (DESIGN.md's "Error handling & testing"
 * section) -- redirects stderr itself (freopen), so every EXISTING
 * fprintf(stderr, ...) diagnostic throughout door_io.c/soundio_termgfx.cpp
 * lands there with no per-call-site changes, when running as a real door
 * (SBBSDATA set by the BBS for external programs). A bare dev/test launch
 * (no SBBSDATA) leaves stderr exactly as it is today. Node tag uses "_n<node>"
 * (NOT ".<node>") to avoid collision with Synchronet's "<name>.<digit>.log"
 * rolled-over-log convention (syncduke_config.c:289-303). */
static void door_open_node_log(void)
{
	const char *data = getenv("SBBSDATA");
	char        logdir[560], logpath[600];
	int         node;

	if (data == NULL || data[0] == '\0')
		return;
	{
		size_t      n   = strlen(data);
		const char *sep = (n && (data[n - 1] == '/' || data[n - 1] == '\\')) ? "" : "/";
		snprintf(logdir, sizeof logdir, "%s%s" DOOR_SHORT_NAME, data, sep);
	}
	door_mkpath(logdir);
	node = sbbs_my_node();
	if (node > 0)
		snprintf(logpath, sizeof logpath, "%s/" DOOR_SHORT_NAME "_n%d.log", logdir, node);
	else
		snprintf(logpath, sizeof logpath, "%s/" DOOR_SHORT_NAME ".log", logdir);
	if (freopen(logpath, "a", stderr) == NULL)
		return;                             /* best-effort: stderr stays wherever it already was */
	/* A crash/hangup must still leave the tail on disk, so don't let the freopen'd
	 * stderr sit fully buffered. syncduke_config.c hit this too: MSVC's CRT rejects
	 * setvbuf(_IOLBF) with size 0 (it requires size >= 2 for line/full buffering)
	 * and fast-fails on the invalid parameter (c0000409), where glibc just ignores
	 * the size. Unbuffered is valid with size 0 and flushes every write -- the same
	 * goal. (MSVC would also silently downgrade _IOLBF to _IOFBF, which is exactly
	 * the buffering we're trying to avoid.) */
#ifdef _WIN32
	setvbuf(stderr, NULL, _IONBF, 0);
#else
	setvbuf(stderr, NULL, _IOLBF, 0);        /* line-buffered -- lines flush promptly */
#endif
}

/* Close the door's own console window (Windows).
 *
 * Synchronet spawns a native door with CREATE_NEW_CONSOLE (sbbs3/xtrn.cpp), so a
 * console window titled "<alias> running <cmd> on node N" pops up on the BBS
 * machine for every session. The door draws to the CLIENT's terminal over the
 * DOOR32.SYS socket and prints nothing there -- its diagnostics go to the
 * per-node log (door_open_node_log() above) -- so the window is pure noise.
 *
 * Synchronet can suppress it host-side too: the door's XTRN_NODISPLAY setting
 * makes xtrn.cpp spawn with CREATE_NO_WINDOW, so no window ever appears (not
 * even the brief flash before we get here), and xtrn/syncalert/install-xtrn.ini
 * sets it. We still detach, because a DOOR32.SYS door can be run by a BBS with
 * no such setting. The two compose: CREATE_NO_WINDOW still gives the process a
 * console, so the GetConsoleProcessList() == 1 test below holds and this just
 * frees an already-invisible console.
 *
 * Only detach from a console we OWN. GetConsoleProcessList() reports how many
 * processes are attached: exactly 1 (us) means the console was created for this
 * process and freeing it closes the window. More than 1 means we inherited a
 * shell's console (a developer running the door from cmd/bash), and detaching
 * would silently throw that developer's stdout away -- so leave it alone. Also
 * skipped for a bare dev launch with no door socket.
 *
 * FreeConsole() invalidates ALL three std streams -- including a stderr already
 * freopen()ed onto a file, whose writes then silently vanish -- so this must run
 * BEFORE door_open_node_log() redirects stderr, and it re-points every CRT
 * stream at NUL on the way out. The vendored engine still calls printf() on some
 * paths (e.g. Print_Error_Exit()), and those must land somewhere valid rather
 * than write to a closed handle. */
static int g_console_detached;

static void door_console_detach(void)
{
#ifdef _WIN32
	DWORD pids[2];
	DWORD n;

	if (g_cli_sock < 0)
		return;                     /* dev launch, no door socket -- keep the console */
	n = GetConsoleProcessList(pids, (DWORD)(sizeof pids / sizeof pids[0]));
	if (n != 1)
		return;                     /* no console (0), or shared with a shell (>1) */

	fflush(stdout);
	fflush(stderr);
	if (!FreeConsole())
		return;
	(void)freopen("NUL", "r", stdin);
	(void)freopen("NUL", "w", stdout);
	(void)freopen("NUL", "w", stderr);   /* door_open_node_log() re-points this at the log */
	g_console_detached = 1;
#endif
}

static void door_early_setup(void)
{
	door_wsa_ensure();             /* Windows: Winsock up before ANY send/recv on the door socket */
	door_console_detach();         /* Windows: close our own console -- BEFORE stderr is redirected,
	                                * because FreeConsole() invalidates stderr even when it already
	                                * points at a file (see door_console_detach()) */
	door_open_node_log();          /* every fprintf(stderr, ...) below lands in it too */
	if (g_console_detached)
		fprintf(stderr, DOOR_SHORT_NAME ": detached from the door's own console window\n");
	door_resolve_assets_dir();     /* before argv[0] is touched by anything below */
	door_validate_assets_or_die(); /* fatal on missing/corrupt REDALERT.MIX/MAIN.MIX */
	door_setup_engine_paths();     /* -home -> chdir + REDALERT.INI [Paths] + argv[0] rewrite */
	door_sanitize_argv();          /* neuter door args out of the engine's view; force -NOMOVIES */
}

/* --- staged output buffer + sink ------------------------------------------- */
static uint8_t *   g_out;
static size_t      g_out_len, g_out_cap, g_out_off;
static uint64_t    g_tx_bytes;   /* cumulative bytes staged for the wire (Ctrl-S throughput) */

static int         g_inited;
static int         g_file_mode;       /* SYNCALERT_SIXELOUT capture mode */
static const char *g_file;
#ifdef _WIN32
static SOCKET      g_iosock = INVALID_SOCKET;
static int         g_use_sock;
#else
static int         g_fd = 1;          /* the fd we WRITE to: socket, or stdout */
/* The fd we READ from. The same as g_fd on a socket (bidirectional), but NOT on
 * a STDIO door -- there the BBS gave us its pipes, so stdin is 0 and stdout is 1,
 * and reading fd 1 reads nothing. This door read g_fd, the WRITE fd: correct on a
 * socket, correct at a dev tty (a tty descriptor is bidirectional), and silently
 * deaf down a pipe. */
static int         g_fd_in = 0;
#endif

static void door_term_restore(void);   /* below; the hangup path runs it first */

/*
 * The client socket has closed or broken. Restore the terminal BEFORE _exit(),
 * then skip the engine's atexit cleanup (which could block on the dead socket).
 *
 * "The socket is already dead" holds for a WRITE-side hangup, but not for a
 * read-side one -- an EOF on read leaves the write direction open, and there we
 * owe the BBS its terminal back (physical key reports off, translated keys
 * restored, mouse tracking off, cursor and autowrap back). door_term_restore()
 * is idempotent and its drain is bounded, so a genuinely dead socket just costs
 * a failed write. Same policy as syncretro's sr_door_hangup() and syncdoom,
 * which reaches its restore through exit()/atexit.
 *
 * The `entered` guard makes it re-entrant-safe: door_term_restore()'s drain
 * calls door_out_flush(), which on a truly dead socket calls back in here.
 */
static void door_hangup(const char *why)
{
	static int entered;

	if (entered)
		_exit(0);   /* re-entered via the restore's drain on a dead socket */
	entered = 1;

	fprintf(stderr, DOOR_SHORT_NAME ": client hangup (%s) -- exiting\n", why ? why : "");
	fflush(stderr);
	door_term_restore();
	_exit(0);   /* skip the engine's atexit cleanup */
}

/* --- per-user "display fit" sticky preference ------------------------------
 * Aspect (default, letterbox) vs Fill (stretch to canvas) -- DESIGN.md. Made
 * sticky via a presence flag-file under -home, mirroring SyncDuke's full-res
 * flag-file pattern (syncduke_io.c:89-108). */
static int g_fit_fill;

static const char *door_flag_path(void)
{
	static char path[600];
	if (g_home[0] != '\0')
		snprintf(path, sizeof path, "%s/" DOOR_SHORT_NAME ".fill", g_home);
	else
		snprintf(path, sizeof path, DOOR_SHORT_NAME ".fill");
	return path;
}

#ifdef _WIN32
#  define DOOR_FIT_PRESENT() (_access(door_flag_path(), 0) == 0)
#  define DOOR_FIT_REMOVE()  _unlink(door_flag_path())
#else
#  define DOOR_FIT_PRESENT() (access(door_flag_path(), F_OK) == 0)
#  define DOOR_FIT_REMOVE()  unlink(door_flag_path())
#endif

static void door_save_fit(void)
{
	if (g_fit_fill) {
		FILE *f = fopen(door_flag_path(), "w");
		if (f)
			fclose(f);
	} else
		DOOR_FIT_REMOVE();
}

/* CTDA cap-8 "physical key event" (SyncTERM evdev) reports -- enabled from
 * door_csi_final()'s 'c' case below; door_input.c's evdev decoder queries
 * this (door_io_evdev_active()) to gate its own 'K'/'k' handling. */
/* The negotiated key mode (termgfx/keymode.h). Lives here because door_io.c
 * sees the CTDA reply that enables evdev and owns the leave path that undoes
 * it; door_input.c reaches it through door_io_keymode() to drive the kitty
 * side and to check the enable-time settle window. */
static termgfx_keymode_t g_km;

termgfx_keymode_t *door_io_keymode(void) { return &g_km; }

int door_io_evdev_active(void) { return termgfx_keymode_evdev_active(&g_km); }

static int g_status_type = -1;         /* pre-door DECSSDT status-line type captured from the
                                        * DECRQSS reply (-1 = not captured / unsupported); the
                                        * door hides the status line for a full 640x400 canvas
                                        * and restores this (or the default) on exit */

/* The per-session SyncTERM audio-APC manager (Task 4) -- created in
 * door_io_init(), read by door/soundio_termgfx.cpp via door_io_audio()
 * below, and torn down in door_term_restore() (forward-declares
 * door_out_flush() since that's defined further down this file). */
static termgfx_audio_t *g_audio;
static void             door_out_flush(void);
static void             door_out_put(const void *buf, size_t len);
static uint32_t         door_now_ms(void);
static void             door_audio_emit(void *ctx, const void *buf, size_t len, int stream);
static void             door_audio_ensure_init(void);
static void             door_stat_dump(const char *reason);   /* per-tier bandwidth summary */
static void             door_cell_size(int *cw, int *ch);      /* terminal cell px (canvas/grid) */
static int              door_term_is_utf8(void);                /* client charset (terminal.ini) */

static void door_sleep_ms(int ms)
{
#ifdef _WIN32
	Sleep((DWORD)ms);
#else
	struct timespec t;
	t.tv_sec  = ms / 1000;
	t.tv_nsec = (long)(ms % 1000) * 1000000L;
	nanosleep(&t, NULL);
#endif
}

/* Bounded blocking drain: retries door_out_flush() until the staged buffer
 * is fully sent or `timeout_ms` elapses, sleeping briefly between attempts
 * (the socket is non-blocking -- a single door_out_flush() call only ever
 * writes what fits in the kernel send buffer right now and returns). Task 5
 * fix: door_term_restore() previously called door_out_flush() exactly ONCE
 * and then wrote its own terminal-restore escapes with a SEPARATE raw
 * write()/send() (also unchecked/non-retried) -- under backpressure (e.g. a
 * large trailing music-store APC still queued when the client exits/times
 * out), that single flush could leave most of the frame/audio buffer
 * unsent, and the terminal-restore bytes appended after it were never
 * retried either, so the client's terminal was left in whatever mode the
 * door had negotiated. Now EVERYTHING (frames, audio, terminal-restore
 * escapes) goes through the one staged buffer and is drained by ONE bounded
 * blocking call at the very end of door_term_restore(). */
static void door_out_drain_blocking(int timeout_ms)
{
	uint32_t start = door_now_ms();
	for (;;) {
		door_out_flush();
		if (g_out_off >= g_out_len)
			return;                                  /* fully sent */
		if ((int32_t)(door_now_ms() - start) >= timeout_ms) {
			fprintf(stderr, DOOR_SHORT_NAME ": shutdown drain timed out after %dms "
			        "with %zu byte(s) unsent\n", timeout_ms, g_out_len - g_out_off);
			return;
		}
		door_sleep_ms(15);
	}
}

/* Best-effort terminal restore at exit: hand the BBS back a terminal with
 * sixel-scrolling/autowrap/cursor re-enabled, and undo the input-side
 * negotiations door_input.c drove (kitty keyboard flags, evdev physical-key
 * reports, SGR mouse tracking) so the BBS session behind us isn't left in a
 * mode it never asked for. Skipped in file-capture mode (no terminal on the
 * other end). Idempotent (safe to call more than once -- atexit AND the
 * SIGTERM/SIGHUP shutdown path below both reach this; the second call's
 * door_out_drain_blocking() is just a no-op flush of an already-empty
 * buffer). */
static void door_term_restore(void)
{
	if (g_file_mode)
		return;
#ifdef _WIN32
	if (!g_use_sock)
		return;
#else
	if (g_fd < 0)
		return;
#endif
	{
		static int stats_dumped;   /* door_term_restore() runs from atexit AND the
		                            * signal path -- dump the summary only once */
		if (!stats_dumped) {
			door_stat_dump("exit");
			stats_dumped = 1;
		}
	}
	termgfx_audio_music_stop(g_audio);    /* stop the looping Theme music ... */
	termgfx_audio_sfx_stop_all(g_audio);  /* ...and any pooled SFX channels */
	door_out_put("\x1b[?1003l\x1b[?1006l\x1b[?1016l", 24);
	{   /* undo whichever key mode was negotiated (termgfx); a no-op if none was */
		char   ks[TERMGFX_KEYMODE_SEQ_MAX];
		size_t kn = termgfx_keymode_restore(&g_km, ks, sizeof ks);

		if (kn > 0)
			door_out_put(ks, kn);
	}
	{   /* restore the status line we hid at entry: the captured pre-door type,
		 * or the usual default (indicator) if DECRQSS never answered */
		char   sb[8];
		size_t sn = termgfx_term_status_set(sb, sizeof sb, g_status_type >= 0 ? g_status_type : 1);
		door_out_put(sb, sn);
	}
	door_out_put(termgfx_term_leave, strlen(termgfx_term_leave));
	door_out_drain_blocking(2000);   /* bounded: never hang the exit path indefinitely on a dead peer */
}

/* --- Task 5: clean shutdown on SIGTERM/SIGHUP -------------------------------
 * Previously NO signal handler was installed at all (default disposition
 * kills the process without running atexit -- confirmed in the Task-3
 * report), so a BBS-initiated recycle/timeout/kill of the door skipped
 * door_term_restore() entirely, leaving the client's terminal in whatever
 * mode the door had negotiated (sixel scrolling off, kitty/evdev/mouse
 * modes still pushed). The handler itself only sets a flag (async-signal-
 * safety: sig_atomic_t is the one type safe to touch from a handler without
 * risking a re-entrant malloc/stdio call) -- the actual shutdown (bounded
 * drain + terminal restore + exit) runs from the main loop, checked at the
 * top of door_io_pump()/door_io_present() (called every engine tick via
 * Fill_Buffer_From_System()/the present hook), never from the handler. */
static volatile sig_atomic_t g_shutdown_req;

static void door_sig_shutdown(int sig)
{
	(void)sig;
	g_shutdown_req = 1;
}

/* Checked from door_io_pump()/door_io_present(); exit() (not _exit()) so the
 * atexit(door_term_restore) chain still runs -- door_term_restore() itself
 * is what actually does the bounded drain + terminal restore. */
static void door_check_shutdown(void)
{
	if (!g_shutdown_req)
		return;
	fprintf(stderr, DOOR_SHORT_NAME ": shutdown requested (SIGTERM/SIGHUP) -- exiting cleanly\n");
	exit(0);
}

/* --- Task 5: DOOR32.SYS time-left awareness (v1: warn only, no forced exit,
 * per the task brief) -- logs once, ~5 minutes (or half the total, if
 * shorter) before g_door_time_left_ms elapses from door_io_init(). A future
 * task can escalate this to an in-game overlay / forced save-and-exit; v1
 * just makes sure it's visible in the per-node log. */
static uint32_t g_time_start_ms;

static void door_check_time_limit(void)
{
	static int warned;
	uint32_t   elapsed, warn_before;

	if (g_door_time_left_ms == 0 || warned)
		return;
	warn_before = (g_door_time_left_ms > 300000) ? 300000 : g_door_time_left_ms / 2;
	elapsed     = door_now_ms() - g_time_start_ms;
	if (elapsed + warn_before >= g_door_time_left_ms) {
		warned = 1;
		fprintf(stderr, DOOR_SHORT_NAME ": time-left warning -- session time limit "
		        "(%u min) approaching\n", g_door_time_left_ms / 60000u);
	}
}

static void door_io_init(void)
{
	const char *s;

	g_inited = 1;
	door_resolve_args();
#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);   /* a write to a closed socket returns EPIPE, not a fatal signal */
	signal(SIGHUP,  door_sig_shutdown);
#endif
	signal(SIGTERM, door_sig_shutdown);   /* SIGTERM exists (signal()-installable) on both platforms */
	atexit(door_term_restore);
	g_fit_fill      = DOOR_FIT_PRESENT();
	g_time_start_ms = door_now_ms();
	door_audio_ensure_init();   /* Task 4 -- see door_audio_ensure_init()'s comment for why
	                             * this can't just live inline here. */
	door_node_init(g_home);     /* Task 5 -- sbbs_node status/Ctrl-U/Ctrl-P (no-op off a BBS) */

	if ((s = getenv("SYNCALERT_SIXELOUT")) != NULL) {   /* capture mode (offline test) */
		g_file = s;
		g_file_mode = 1;
		return;
	}

#ifdef _WIN32
	{
		int fd = (g_cli_sock >= 0) ? g_cli_sock : -1;
		if (fd < 0) {
			const char *e = getenv("SYNCALERT_SOCK");
			if (e != NULL)
				fd = atoi(e);
		}
		if (fd >= 0) {
			u_long nb  = 1;
			int    one = 1;
			int    sz  = 96 * 1024;
			g_iosock   = (SOCKET)fd;
			g_use_sock = 1;
			ioctlsocket(g_iosock, FIONBIO, &nb);
			setsockopt(g_iosock, IPPROTO_TCP, TCP_NODELAY, (char *)&one, sizeof one);
			setsockopt(g_iosock, SOL_SOCKET, SO_SNDBUF, (char *)&sz, sizeof sz);
		}
	}
#else
	{
		int fd = (g_cli_sock >= 0) ? g_cli_sock : -1;
		int fl;

		if (fd < 0) {
			if ((s = getenv("SYNCALERT_SOCK")) != NULL)
				fd = atoi(s);
		}
		if (fd >= 0) {
			int one = 1, sz = 96 * 1024;
			g_fd    = fd;
			g_fd_in = fd;              /* a socket is read AND written */
			setsockopt(g_fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
			setsockopt(g_fd, SOL_SOCKET, SO_SNDBUF, &sz, sizeof sz);
		}
		/* else g_fd stays 1 (stdout) and g_fd_in stays 0 (stdin): a STDIO door,
		 * which is also exactly what a dev/tty run wants. */
		if ((fl = fcntl(g_fd, F_GETFL, 0)) != -1)
			fcntl(g_fd, F_SETFL, fl | O_NONBLOCK);
		if (g_fd_in != g_fd && (fl = fcntl(g_fd_in, F_GETFL, 0)) != -1)
			fcntl(g_fd_in, F_SETFL, fl | O_NONBLOCK);
	}
#endif
}

static void door_out_put(const void *buf, size_t len)
{
	if (g_out_len + len > g_out_cap) {
		g_out_cap = g_out_len + len;
		g_out = realloc(g_out, g_out_cap);
	}
	memcpy(g_out + g_out_len, buf, len);
	g_out_len += len;
	g_tx_bytes += len;   /* every wire-bound byte (frame/palette/overlay/DSR/audio) */
}

static void door_out_puts(const char *s) { door_out_put(s, strlen(s)); }

/* --- audio (Task 4) ---------------------------------------------------------
 * g_audio (declared above, near g_km) is owned here, same as
 * syncduke_io.c's sd_audio: door/soundio_termgfx.cpp (the engine's
 * SoundImp_* backend) reaches it via door_io_audio() to turn engine-decoded
 * PCM into SFX/music APCs; this file feeds it inbound bytes (the cap-probe
 * reply + a client's C;L cache-list reply, door_io_pump() below) and emits
 * its output through the same staged buffer door_io_present() flushes --
 * audio rides the same FIFO as frames, no separate socket. */
static void door_audio_emit(void *ctx, const void *buf, size_t len, int stream)
{
	(void)ctx;
	(void)stream;   /* one staged buffer, no separate PRIO/BULK lane (yet) */
	door_out_put(buf, len);
}

/* Strip a trailing separator, then the last path component, leaving a
 * trailing separator -- mirrors termgfx/sbbs_node.c's path_pop() (same
 * convention: <data>/user/####/redalert/ -> pop x3 -> <data>/). Used only to
 * derive the audio-cache dir from -home when neither SBBSDATA nor an
 * explicit override is available. */
static void door_path_pop(char *p)
{
	size_t n = strlen(p);
	while (n && (p[n - 1] == '/' || p[n - 1] == '\\'))
		p[--n] = '\0';
	while (n && p[n - 1] != '/' && p[n - 1] != '\\')
		p[--n] = '\0';
}

/* --- Task 5: unify the audio-cache directory ------------------------------
 * Two independent consumers need to agree on the SAME on-disk directory:
 * this file's own termgfx_audio_set_music_cache_dir() (the SyncTERM-side
 * transcoded-OGG cache) and door/soundio_termgfx.cpp's sa_theme_cache_dir()
 * (the fingerprint/duration MAP files -- Task 4's own module, which reads
 * ONLY the SYNCALERT_AUDIO_CACHE env var, with no visibility into this
 * file's -audiocache CLI flag or g_audiocache at all -- flagged as a known
 * carry-forward divergence in the Task 4 report). Resolving it HERE, once,
 * with a priority order, and then exporting the result back out as
 * SYNCALERT_AUDIO_CACHE regardless of which input actually won, makes both
 * consumers converge on one directory no matter how the door was launched.
 * Priority: -audiocache CLI > SYNCALERT_AUDIO_CACHE env (already set by the
 * launcher) > SBBSDATA-derived <data>/syncalert/audio (real BBS install,
 * mirrors syncduke_io.c) > derived from -home (<data>/ recovered via three
 * door_path_pop()s, same as sbbs_node.c) > a bare relative default (dev/test
 * runs with neither). */
static void door_resolve_audio_cache_dir(char *out, size_t outsz)
{
	const char *env;
	const char *data;

	if (g_audiocache[0] != '\0') {
		snprintf(out, outsz, "%s", g_audiocache);
		return;
	}
	if ((env = getenv("SYNCALERT_AUDIO_CACHE")) != NULL && env[0] != '\0') {
		snprintf(out, outsz, "%s", env);
		return;
	}
	data = getenv("SBBSDATA");
	if (data != NULL && data[0] != '\0') {
		size_t      n   = strlen(data);
		const char *sep = (n && (data[n - 1] == '/' || data[n - 1] == '\\')) ? "" : "/";
		snprintf(out, outsz, "%s%s" DOOR_SHORT_NAME "/audio", data, sep);
		return;
	}
	if (g_home[0] != '\0') {
		char        tmp[560];
		size_t      n;
		const char *sep;
		snprintf(tmp, sizeof tmp, "%s", g_home);
		door_path_pop(tmp);   /* alert/ */
		door_path_pop(tmp);   /* ####/  */
		door_path_pop(tmp);   /* user/  -> <data>/ */
		if (tmp[0] != '\0') {
			n   = strlen(tmp);
			sep = (n && (tmp[n - 1] == '/' || tmp[n - 1] == '\\')) ? "" : "/";
			snprintf(out, outsz, "%s%s" DOOR_SHORT_NAME "/audio", tmp, sep);
			return;
		}
	}
	snprintf(out, outsz, "%s", "./audio-cache");
}

/* Idempotent (guarded by g_audio == NULL): called from BOTH door_io_init()
 * (the normal video-path entry) AND door_io_audio() below, because the
 * engine calls Audio_Init() -- which reaches our SoundImp_Init(), which
 * calls door_io_audio() -- from startup.cpp well BEFORE the first rendered
 * frame (Set_Video_Mode() runs strictly after it there), i.e. before
 * anything would otherwise have triggered door_io_init(). Without this,
 * SoundImp_Init() saw a NULL manager, soundio_common.cpp's Audio_Init()
 * returned false, and the engine permanently disabled all sound
 * (SampleType/LockedData.DigiHandle never got set) -- caught by the Task-4
 * harness (test_sfx_apc/test_music_cache both silently produced zero audio
 * APCs until this fix). door_resolve_args() is safe to call twice (it just
 * re-parses the same argv -- g_argc/g_argv are already valid this early,
 * captured by the __attribute__((constructor)) below, which runs before
 * main() itself). */
static void door_audio_ensure_init(void)
{
	if (g_audio != NULL)
		return;
	door_resolve_args();
	g_audio = termgfx_audio_create(door_audio_emit, NULL);
	termgfx_audio_set_cache_prefix(g_audio, DOOR_SHORT_NAME);   /* SyncTERM cache: syncalert/music|sfx/.. */
	{
		char dir[512];
		door_resolve_audio_cache_dir(dir, sizeof dir);
		door_mkpath(dir);
		termgfx_audio_set_music_cache_dir(g_audio, dir);
		/* Export the WINNING resolution back out so soundio_termgfx.cpp's
		 * independently-resolved sa_theme_cache_dir() (env-var-only) agrees
		 * with this directory too, however it was actually derived above. */
#ifdef _WIN32
		_putenv_s("SYNCALERT_AUDIO_CACHE", dir);
#else
		setenv("SYNCALERT_AUDIO_CACHE", dir, 1);
#endif
	}
}

termgfx_audio_t *door_io_audio(void)
{
	door_audio_ensure_init();
	return g_audio;
}

/* Truncate-write: the capture file always holds exactly the latest frame. */
static void door_out_flush_file(void)
{
	FILE *fp = fopen(g_file, "wb");
	if (fp) {
		fwrite(g_out, 1, g_out_len, fp);
		fclose(fp);
	}
	g_out_len = g_out_off = 0;
}

static void door_out_flush(void)
{
	if (g_file_mode) {
		door_out_flush_file();
		return;
	}
#ifdef _WIN32
	if (!g_use_sock) {
		g_out_len = g_out_off = 0;
		return;
	}
	while (g_out_off < g_out_len) {
		int n = send(g_iosock, (const char *)g_out + g_out_off,
		             (int)(g_out_len - g_out_off), 0);
		int e;
		if (n > 0) { g_out_off += (size_t)n; continue; }
		e = WSAGetLastError();
		if (e == WSAEWOULDBLOCK || e == WSAENOBUFS || e == WSAEINTR)
			break;      /* transient backpressure: keep the frame pending */
		{
			char r[64];
			snprintf(r, sizeof r, "client closed (send error wsa=%d)", e);
			door_hangup(r);
		}
		break;
	}
#else
	while (g_out_off < g_out_len) {
		ssize_t n = write(g_fd, g_out + g_out_off, g_out_len - g_out_off);
		if (n > 0) { g_out_off += (size_t)n; continue; }
		if (n < 0 && errno == EINTR)
			continue;
		if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
			break;      /* slow client: keep the frame pending */
		if (g_cli_sock >= 0)
			door_hangup("client closed (write error)");
		break;
	}
#endif
	if (g_out_off >= g_out_len)
		g_out_len = g_out_off = 0;
}

static void door_cursor_save_to(int row, int col)
{
	char wrap[24];
	int  n = snprintf(wrap, sizeof wrap, "\x1b" "7\x1b[%d;%dH", row, col);
	door_out_put(wrap, (size_t)n);
}
static void door_cursor_restore(void) { door_out_put("\x1b" "8", 2); }

/* --- monotonic clock (shared by pacing) ------------------------------------ */
static uint32_t door_now_ms(void);

/* door_input.c's evdev settle check needs this same clock domain. */
uint32_t door_io_now_ms(void) { return door_now_ms(); }

static uint32_t door_now_ms(void)
{
#ifdef _WIN32
	static LARGE_INTEGER freq;
	LARGE_INTEGER        c;
	if (freq.QuadPart == 0)
		QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&c);
	return (uint32_t)((uint64_t)c.QuadPart * 1000ULL / (uint64_t)freq.QuadPart);
#else
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint32_t)((uint64_t)t.tv_sec * 1000ULL + (uint64_t)t.tv_nsec / 1000000ULL);
#endif
}

/* --- terminal capability / geometry probe state ----------------------------
 * Filled in by door_csi_final() as replies to the startup probe come in
 * (termgfx_term_probe, "\x1b[c\x1b[<c" DA1+CTDA, termgfx_query_jxl). */
static int g_probe_replied;    /* any 'c' (DA1/CTDA) reply seen */
static int g_is_syncterm;      /* CTDA '<'/'=' marker, or a JXL-query answer */
static int g_have_sixel;       /* DA1/CTDA param 4 */
static int g_jxl_supported;    /* Q;JXL answered 1 */
static int g_img_blob_ok;      /* CTerm >= 1.329: draw JXL/PPM inline (no cache file) */
static int g_grid_rows, g_grid_cols;   /* text grid, from the 999;999 cursor-report fallback */
static int g_canvas_w, g_canvas_h;     /* pixel canvas -- exact (ESC[14t) or grid*8x16 estimate */
static int g_px_exact;                 /* g_canvas_w/h came from an exact ESC[4;h;wt reply */
static int g_mouse_pixels;             /* SGR-Pixels mouse (DEC 1016) confirmed active: mouse
                                        * reports are canvas PIXELS, not text cells (Windows
                                        * Terminal supports it; SyncTERM does not -> stays cells) */

/* --- graphics tiers (F4 cycles) --------------------------------------------
 * SIXEL/JXL/PPM are pixel tiers (PPM via SyncTERM's APC cache, for a client
 * with neither sixel nor JXL); HALF/QUADRANT/SEXTANT are termgfx text/block
 * tiers -- a SyncTERM-independent fallback. Full-res sixel ONLY (DESIGN.md):
 * no half-res + terminal-upscale mode, unlike SyncDuke/SyncDOOM. */
enum sa_tier { SA_SIXEL = 0, SA_JXL, SA_PPM, SA_HALF, SA_QUADRANT, SA_SEXTANT, SA_NTIERS };

static int sa_is_text_tier(int t) { return t >= SA_HALF; }

static const char *sa_tier_name(int t)
{
	static const char *const names[SA_NTIERS] = {
		"sixel", "jxl", "ppm", "half", "quadrant", "sextant"
	};
	return (t >= 0 && t < SA_NTIERS) ? names[t] : "?";
}

/* --- per-tier bandwidth stats ----------------------------------------------
 * Accumulate the encoded image/text payload each tier emits per frame, so a
 * sysop can see whether JXL actually buys anything over sixel (only matters
 * over a real remote link -- on a LAN both are instant). Dumped to the node
 * log at exit and on every F4 tier change (so cycling naturally produces a
 * side-by-side). Counts the tier's own frame payload -- not the palette/
 * overlay/DSR framing, which is the same regardless of tier. */
static struct {
	uint64_t frames;
	uint64_t bytes;
	size_t min;
	size_t max;
} g_tstat[SA_NTIERS];

static void door_stat_record(int tier, size_t n)
{
	if (tier < 0 || tier >= SA_NTIERS || n == 0)
		return;
	if (g_tstat[tier].frames == 0 || n < g_tstat[tier].min)
		g_tstat[tier].min = n;
	if (n > g_tstat[tier].max)
		g_tstat[tier].max = n;
	g_tstat[tier].frames++;
	g_tstat[tier].bytes += n;
}

static void door_stat_dump(const char *reason)
{
	int t, any = 0;

	for (t = 0; t < SA_NTIERS; t++) {
		if (g_tstat[t].frames == 0)
			continue;
		any = 1;
		fprintf(stderr,
		        DOOR_SHORT_NAME ": stats[%s] %s: %llu frames, %llu bytes, "
		        "avg %llu, min %zu, max %zu bytes/frame\n",
		        reason, sa_tier_name(t),
		        (unsigned long long)g_tstat[t].frames,
		        (unsigned long long)g_tstat[t].bytes,
		        (unsigned long long)(g_tstat[t].bytes / g_tstat[t].frames),
		        g_tstat[t].min, g_tstat[t].max);
	}
	if (!any)
		fprintf(stderr, DOOR_SHORT_NAME ": stats[%s]: no frames sent yet\n", reason);
}

static int  g_tier_force = -1;       /* -1 = auto; F4 sets an explicit tier */

static int  g_stats_overlay;         /* Ctrl-S: live debug stats strip (session-only) */
static char g_stats_last[512];       /* last emitted overlay bytes, for change-detection */

static int sa_auto_tier(void)
{
#ifdef WITH_JXL
	if (g_jxl_supported)
		return SA_JXL;
#endif
	if (g_have_sixel)
		return SA_SIXEL;
	if (g_is_syncterm)
		return SA_PPM;         /* SyncTERM without sixel/JXL: still gets the APC PPM tier */
	if (g_probe_replied)
		return SA_HALF;        /* answered DA1 but no sixel, not SyncTERM: text tier */
	return SA_SIXEL;           /* no reply yet: optimistic default (most BBS clients are SyncTERM) */
}

/* forward decls used by door_tier_cycle()/door_fit_toggle() to force a repaint */
static int g_have_last;
static int g_clear_pending;   /* emit a screen clear before the next frame (tier change) */
static int g_inflight;
static int g_dsr_h, g_dsr_t;

static void door_tier_cycle(void)
{
	int avail[8], n = 0, i, eff, cur = 0, jxl_ok = 0;

#ifdef WITH_JXL
	if (g_jxl_supported) {
		avail[n++] = SA_JXL;
		jxl_ok     = 1;
	}
#endif
	if (g_have_sixel || !g_probe_replied)
		avail[n++] = SA_SIXEL;
	/* PPM is a huge UNCOMPRESSED tier and, being a raw P6 with no colorspace
	 * tag, SyncTERM decodes it as BT.709 rather than our sRGB (the colors look
	 * off). It only earns its place as the SyncTERM fallback when JXL isn't
	 * available -- real SyncTERM has JXL -- so keep it OUT of the manual F4
	 * cycle whenever JXL is. sa_auto_tier() still selects it for a no-JXL
	 * SyncTERM, so that niche keeps working. */
	if (g_is_syncterm && !jxl_ok)
		avail[n++] = SA_PPM;
	avail[n++] = SA_HALF;   /* half-block glyph exists in BOTH charsets (U+2580 / 0xDF) */
	/* Quadrant/sextant glyphs are UTF-8 only -- no CP437 equivalent, so on a
	 * CP437 (or US-ASCII) client they'd emit raw UTF-8 as mojibake. Offer them
	 * only when the client is UTF-8 (matches syncduke/syncdoom). */
	if (door_term_is_utf8()) {
		avail[n++] = SA_QUADRANT;
		avail[n++] = SA_SEXTANT;
	}

	eff = (g_tier_force >= 0) ? g_tier_force : sa_auto_tier();
	for (i = 0; i < n; i++)
		if (avail[i] == eff)
			cur = i;
	door_stat_dump("tier-switch");   /* log the tier we're leaving, for JXL-vs-sixel comparison */
	g_tier_force = avail[(cur + 1) % n];

	/* tier switch: clear the pipeline + force a repaint so it shows at once,
	 * and request a screen clear before the next frame. Without the clear,
	 * switching FROM a text/block tier (which fills all 80x25 cells) TO an
	 * image tier leaves the old block characters in every cell the new image
	 * doesn't cover -- and an aspect-fitted sixel is letterboxed, so its
	 * border cells keep showing stale text. It looked like the graphics tier
	 * wouldn't come back; really the image was drawing over a floor of
	 * leftover mojibake. Clearing on every tier change also drops the prior
	 * image tier's pixels when the new tier centers differently. */
	g_inflight     = 0;
	g_dsr_t        = g_dsr_h;
	g_have_last    = 0;
	g_clear_pending = 1;
}

/* Exported so door_input.c can trigger the F4 tier cycle from the key paths
 * this file's own probe-reply parser never sees: an evdev keycode (SyncTERM
 * in CTDA cap-8 mode, which door_io_init() enables) or an rxvt-style ESC[14~
 * both arrive as game keys through door_input.c, not as the SS3/CSI "S" this
 * file matches -- so without this hook F4 was silently swallowed there. */
void door_io_tier_cycle(void)
{
	door_tier_cycle();
}

static void door_fit_toggle(void)
{
	g_fit_fill = !g_fit_fill;
	door_save_fit();
	g_have_last = 0;      /* force a full repaint at the new geometry */
	g_clear_pending = 1;  /* Fill (full-width) -> Aspect (narrower, centered) shrinks
	                       * the image; without a clear the old wider frame's pixels
	                       * bleed through the newly-exposed side margins (same reason
	                       * door_tier_cycle() clears on a centering change). */
}

/* Single dispatch point for the door-level Ctrl+letter hotkeys, shared by all
 * three input paths (this file's legacy raw-byte branch + door_input.c's evdev
 * and kitty decoders) so the mapping lives in exactly one place -- see the
 * door_io.h prototype for why the two native modes need it too, and for the
 * `commit` contract (act only on the press edge; return 1 for a hotkey letter
 * on every edge so the caller swallows repeat/release too). `letter` is a
 * lowercase 'a'..'z'. */
/* Music/score volume step, defined in the engine (options.cpp, extern "C").
 * The door's +/- hotkeys nudge Options.ScoreVolume because the Sound Controls
 * slider is unreachable with a cell-granular (SyncTERM) mouse. */
extern void SyncConquer_Music_Volume_Step(int dir);

/* Deferred client-side music-channel stop, defined in soundio_termgfx.cpp
 * (extern "C"). Ticked here each pump so a real Theme.Stop() actually silences
 * the looping score without gapping a theme change. */
extern void SoundImp_Music_Housekeep(void);

int door_io_hotkey(int letter, int commit)
{
	/* Classify first: is `letter` a door hotkey at all? */
	switch (letter) {
		case 'd': case 'f': case 'u': case 'p': case 's':
		case '+': case '=': case '-': case '_':   /* music volume up ('+'/'=') / down ('-'/'_') */
			break;
		default:
			return 0;   /* not a hotkey -- caller forwards it to the game */
	}
	if (!commit)
		return 1;       /* recognized, but this edge (repeat/release) doesn't fire */

	/* Actionable press edge: run it. */
	switch (letter) {
		case 'd': door_tier_cycle();            break;   /* F4 fallback: cycle graphics tier */
		case 'f': door_fit_toggle();            break;   /* Aspect <-> Fill display-fit */
		case 'u': door_node_userlist_request(); break;   /* who's-online overlay */
		case 'p': door_node_page_request();     break;   /* page-a-node compose */
		case '+': case '=': SyncConquer_Music_Volume_Step(1);  break;   /* music volume up */
		case '-': case '_': SyncConquer_Music_Volume_Step(-1); break;   /* music volume down */
		case 's':                                        /* live debug stats overlay */
			g_stats_overlay = !g_stats_overlay;
			g_stats_last[0] = '\0';   /* reset change-detection across the toggle */
			g_have_last     = 0;   /* repaint now so the strip shows... */
			g_clear_pending = 1;   /* ...and clear it cleanly on toggle-off */
			break;
	}
	return 1;
}

/* --- scale/pack helpers (dynamic buffers -- no fixed-size overflow risk) --- */
static int ensure_cap(uint8_t **buf, size_t *cap, size_t need)
{
	uint8_t *nb;
	if (need <= *cap)
		return 1;
	nb = realloc(*buf, need);
	if (nb == NULL)
		return 0;
	*buf = nb;
	*cap = need;
	return 1;
}

static uint8_t *g_idx_buf;  static size_t g_idx_cap;   /* scaled palette-index buffer (sixel) */
static uint8_t *g_rgb_buf;  static size_t g_rgb_cap;   /* scaled RGB888 buffer (jxl/ppm/text) */

/* Nearest-neighbor scale the native DOOR_FB_WIDTHxDOOR_FB_HEIGHT palette-index
 * framebuffer into g_idx_buf at ew x eh. */
static const uint8_t *door_scale_idx(const uint8_t *fb, int ew, int eh)
{
	int y;
	if (!ensure_cap(&g_idx_buf, &g_idx_cap, (size_t)ew * eh))
		return NULL;
	for (y = 0; y < eh; y++) {
		const uint8_t *row = fb + (size_t)(y * DOOR_FB_HEIGHT / eh) * DOOR_FB_WIDTH;
		uint8_t *      o   = g_idx_buf + (size_t)y * ew;
		int            x;
		for (x = 0; x < ew; x++)
			o[x] = row[x * DOOR_FB_WIDTH / ew];
	}
	return g_idx_buf;
}

/* Nearest-neighbor scale + palette-map the native framebuffer into g_rgb_buf
 * (RGB888) at ew x eh. */
static const uint8_t *door_pack_rgb(const uint8_t *fb, const uint8_t *pal, int ew, int eh)
{
	int      x, y;
	uint8_t *p;
	if (!ensure_cap(&g_rgb_buf, &g_rgb_cap, (size_t)ew * eh * 3))
		return NULL;
	p = g_rgb_buf;
	for (y = 0; y < eh; y++) {
		const uint8_t *row = fb + (size_t)(y * DOOR_FB_HEIGHT / eh) * DOOR_FB_WIDTH;
		for (x = 0; x < ew; x++) {
			const uint8_t *c = pal + (size_t)row[x * DOOR_FB_WIDTH / ew] * 3;
			*p++ = c[0]; *p++ = c[1]; *p++ = c[2];
		}
	}
	return g_rgb_buf;
}

#define DOOR_SCALE_MAX 2048

/* --- per-tier emit ---------------------------------------------------------- */
static uint8_t *g_sixel_buf; static size_t g_sixel_cap;
#ifdef WITH_JXL
static uint8_t *g_jxl_buf;   static size_t g_jxl_cap;
#endif
static uint8_t *g_apc_buf;   static size_t g_apc_cap;
static uint8_t *g_ppm_buf;   static size_t g_ppm_cap;

/* Full-res sixel only (pan=1;pad=1, DESIGN.md) -- clamp to whole 6-row sixel
 * bands: a partial final band garbles under SyncTERM's decoder (the proven
 * SyncDuke workaround, syncduke_io.c:1224-1232). */
static size_t door_emit_sixel(const uint8_t *fb, const uint8_t *pal8, int ew, int eh, int emit_pal)
{
	const uint8_t *idx;

	if (ew > DOOR_SCALE_MAX)
		ew = DOOR_SCALE_MAX;
	eh -= eh % 6;
	if (eh < 6)
		eh = 6;
	idx = door_scale_idx(fb, ew, eh);
	if (idx == NULL)
		return 0;
	return sixel_encode(&g_sixel_buf, &g_sixel_cap, idx, ew, eh, pal8, emit_pal);
}

#ifdef WITH_JXL
static size_t door_emit_jxl(const uint8_t *fb, const uint8_t *pal8, int ew, int eh, int dx, int dy)
{
	const uint8_t *rgb = door_pack_rgb(fb, pal8, ew, eh);
	size_t         n;

	if (rgb == NULL)
		return 0;
	n = termgfx_jxl_encode(&g_jxl_buf, &g_jxl_cap, rgb, ew, eh, 2.0f, 1);
	if (n == 0)
		return 0;
	/* Per-session cache name (SF syncterm #256): two SyncTERM windows on the same
	 * dialing-entry share one cache dir, so a fixed frame name would collide. */
	static char name[32];
	if (name[0] == '\0')
		snprintf(name, sizeof name, "syncalert_%08x.jxl", termgfx_session_salt());
	return termgfx_apc_image(&g_apc_buf, &g_apc_cap, name, "DrawJXL",
	                         g_jxl_buf, n, dx, dy, g_img_blob_ok);
}
#endif

/* SyncTERM-only fallback for a client with neither sixel nor JXL: the APC
 * cache Store+Draw wraps a plain uncompressed PPM (P6) payload. */
static size_t door_emit_ppm(const uint8_t *fb, const uint8_t *pal8, int ew, int eh, int dx, int dy)
{
	const uint8_t *rgb = door_pack_rgb(fb, pal8, ew, eh);
	char           hdr[32];
	int            hlen;
	size_t         need;

	if (rgb == NULL)
		return 0;
	hlen = snprintf(hdr, sizeof hdr, "P6\n%d %d\n255\n", ew, eh);
	need = (size_t)hlen + (size_t)ew * eh * 3;
	if (!ensure_cap(&g_ppm_buf, &g_ppm_cap, need))
		return 0;
	memcpy(g_ppm_buf, hdr, hlen);
	memcpy(g_ppm_buf + hlen, rgb, (size_t)ew * eh * 3);
	static char name[32];   /* per-session cache name (SF syncterm #256) */
	if (name[0] == '\0')
		snprintf(name, sizeof name, "syncalert_%08x.ppm", termgfx_session_salt());
	return termgfx_apc_image(&g_apc_buf, &g_apc_cap, name, "DrawPPM",
	                         g_ppm_buf, need, dx, dy, g_img_blob_ok);
}

static int g_rt_mode = -1, g_rt_cols, g_rt_rows;

/* Client terminal charset for the block tiers: the block glyph is UTF-8
 * (U+2580) on a UTF-8 client but a single 0xDF byte on CP437. Emitting UTF-8
 * to a CP437 client (the BBS default) shows mojibake -- the door runs with
 * character-set translation OFF, so whatever bytes the tier emits reach the
 * terminal untranslated. Synchronet writes the user's terminal settings to
 * $SBBSNODE/terminal.ini; read its root-section "chars" key and use UTF-8 only
 * when it says so, else CP437 (the safe default). Resolved once. Matches how
 * syncduke/syncdoom source it (they add a -charset arg / [video] charset
 * override too, for non-Synchronet installs -- room to add here later). */
static int door_term_is_utf8(void)
{
	static int  cached = -1;
	const char *node;
	char        path[600], line[256];
	FILE *      f;

	if (cached >= 0)
		return cached;
	cached = 0;                          /* default: CP437 (safe) */
	node   = getenv("SBBSNODE");
	if (node == NULL || node[0] == '\0')
		return cached;
	snprintf(path, sizeof path, "%s/terminal.ini", node);
	if ((f = fopen(path, "r")) == NULL)
		return cached;
	while (fgets(line, sizeof line, f) != NULL) {
		char *p = line, *v;

		while (*p == ' ' || *p == '\t')
			p++;
		if (*p == '[')                   /* past the root section -- "chars" lives there */
			break;
		if (strnicmp(p, "chars", 5) != 0)   /* xpdev genwrap.h: portable case-insensitive */
			continue;
		v = p + 5;
		while (*v == ' ' || *v == '\t')
			v++;
		if (*v != '=')
			continue;
		v++;
		while (*v == ' ' || *v == '\t')
			v++;
		if (strnicmp(v, "utf", 3) == 0)     /* utf8 / utf-8 / UTF-8 */
			cached = 1;
		break;
	}
	fclose(f);
	return cached;
}

/* Text/block tier: rt_render_frame scales+cell-diffs internally, so this
 * packs the NATIVE framebuffer to RGB (no pre-scale needed). The text grid
 * (cols x rows) uses the terminal's real cell size (door_cell_size(), not a
 * hardcoded 8x16 -- a scaled canvas / non-8x16 font otherwise mis-sizes it),
 * and the block charset follows the client (door_term_is_utf8()). */
static size_t door_emit_text(const uint8_t *fb, const uint8_t *pal8, rt_mode_t mode, int canvas_w, int canvas_h)
{
	int            cw, ch, cols, rows;
	const uint8_t *rgb;

	door_cell_size(&cw, &ch);
	cols = canvas_w / (cw > 0 ? cw : 8);
	rows = canvas_h / (ch > 0 ? ch : 16);
	const char * tb;
	size_t       tlen;

	if (cols < 1)
		cols = 1;
	if (rows < 1)
		rows = 1;
	if ((int)mode != g_rt_mode || cols != g_rt_cols || rows != g_rt_rows) {
		rt_config(cols, rows, mode, g_is_syncterm ? RT_24BIT : RT_8BIT,
		          door_term_is_utf8() ? RT_UTF8 : RT_CP437);
		g_rt_mode = (int)mode; g_rt_cols = cols; g_rt_rows = rows;
	}
	rgb = door_pack_rgb(fb, pal8, DOOR_FB_WIDTH, DOOR_FB_HEIGHT);
	if (rgb == NULL)
		return 0;
	tb = rt_render_frame(rgb, DOOR_FB_WIDTH, DOOR_FB_HEIGHT, &tlen);
	door_out_put(tb, tlen);
	return tlen;
}

/* --- DSR-ACK pacing + AIMD pipeline depth (SyncDuke/SyncDOOM model,
 * termgfx/pace.h) ------------------------------------------------------------
 * After each frame we emit a DSR (ESC[6n); the terminal's cursor-position
 * report comes back only once it has CONSUMED that frame, so the count of
 * un-acked DSRs (g_inflight) is how many frames the terminal hasn't drawn
 * yet. A stale-progress deadline reclaims the pipeline so a terminal that
 * never answers can't freeze the door (degrades to a slideshow). */
#define DOOR_PACE_DEADLINE_MS 750
#define DOOR_AUTO_DEPTH_MAX   8
#define DOOR_DSR_RING         16
#define DOOR_PROBE_GRACE_MS   500

static uint32_t g_dsr_ts[DOOR_DSR_RING];
static uint32_t g_pace_progress_ms;
static int      g_auto_depth = 3;
static uint32_t g_auto_adj_at;
static int      g_rt_high;
static uint32_t g_rtt_ms, g_rtt_min, g_rtt_min_at;

/* Frames-emitted-per-second and wire throughput, both sampled over the same
 * rolling ~1s window for the Ctrl-S stats overlay. g_fps holds the last
 * window's frame rate; g_bps its bits/second (from g_tx_bytes' delta over the
 * window -- total wire TX, so it includes palette/overlay/DSR/audio, not just
 * the frame payload the per-tier B/frame stat isolates). */
static int      g_fps;
static uint32_t g_fps_win_ms;
static int      g_fps_frames;
static uint32_t g_bps;
static uint64_t g_tx_bytes_at;

static int door_eff_depth(void) { return g_auto_depth; }

static void door_dsr_sent(uint32_t now)
{
	g_dsr_ts[g_dsr_h] = now;
	g_dsr_h = (g_dsr_h + 1) % DOOR_DSR_RING;
}

/* A DSR cursor-position report came back: one in-flight frame was consumed. */
static void door_pace_ack(void)
{
	uint32_t now = door_now_ms();

	if (g_inflight > 0)
		g_inflight--;
	if (g_dsr_h != g_dsr_t) {
		uint32_t rtt = now - g_dsr_ts[g_dsr_t];
		g_dsr_t = (g_dsr_t + 1) % DOOR_DSR_RING;
		if (termgfx_rtt_sample(&g_rtt_ms, &g_rtt_min, &g_rtt_min_at, &g_rt_high, rtt, now, 0, 0))
			termgfx_aimd_update(1, &g_auto_depth, &g_auto_adj_at,
			                    g_rtt_ms, g_rtt_min, g_rt_high, DOOR_AUTO_DEPTH_MAX, now);
	}
	g_pace_progress_ms = now;
}

/* --- present ---------------------------------------------------------------- */
static uint8_t g_last_fb[DOOR_FB_WIDTH * DOOR_FB_HEIGHT];
static uint8_t g_last_pal[768];
static int     g_last_tier = -1;
static int     g_last_fit;
static int     g_last_canvas_w, g_last_canvas_h;   /* geometry used to size/center the last
                                                     * sent frame -- a probe reply landing after
                                                     * that frame changes g_canvas_w/h without
                                                     * touching fb/pal/tier/fit, so it must be
                                                     * part of the de-dupe signature too
                                                     * (syncduke_io.c:1113-1119 does the same
                                                     * with last_w/last_h). */

static void scale_palette(const uint8_t *pal768, uint8_t *out768)
{
	int i;
	for (i = 0; i < 768; i++)
		out768[i] = (uint8_t)(pal768[i] << 2);
}

/* The terminal's real character-cell pixel size. Derived from the probed
 * canvas and text grid (canvas_w/cols, canvas_h/rows) rather than assumed 8x16
 * -- a scaled SyncTERM or a non-8x16 font (Windows Terminal) reports a canvas
 * whose cells aren't 8x16, and hardcoding 8x16 there misplaces both the image
 * centering AND the SGR-mouse->game-pixel mapping (live-test finding: cursor
 * pushed off toward a corner). Falls back to 8x16 only when the grid/canvas
 * isn't known yet. Shared by door_calc_rect() and door_io_cell_size(). */
static void door_cell_size(int *cw, int *ch)
{
	int w = (g_grid_cols > 0 && g_canvas_w > 0) ? g_canvas_w / g_grid_cols : 8;
	int h = (g_grid_rows > 0 && g_canvas_h > 0) ? g_canvas_h / g_grid_rows : 16;

	*cw = w > 0 ? w : 8;
	*ch = h > 0 ? h : 16;
}

/* Fit a DOOR_FB_WIDTH x DOOR_FB_HEIGHT frame into a vw x vh canvas (Aspect
 * true-ratio, or Fill/stretch per g_fit_fill), center it, and return the pixel
 * rect (ew/eh/dx/dy) + top-left text cell (icol/irow). Shared by
 * door_io_present() and door_io_get_rect() (door_input.c's SGR mouse mapper) so
 * both key off the SAME geometry.
 *
 * reserve_bottom: leave one text-cell row free at the bottom. ONLY the SIXEL
 * tier needs this -- a terminal that ignores DECSDM ?80l (Windows Terminal,
 * some SyncTERM builds) scrolls a bottom-touching sixel, making the image
 * "bounce" (the fix syncduke/syncdoom use, commit 07a345e987). JXL and PPM are
 * positioned by an absolute APC pixel offset, never a text cursor, so they do
 * NOT scroll and MUST use the full canvas -- reserving there just shrinks them
 * needlessly (live-test finding: JXL looked scaled/letterboxed instead of the
 * crisp full-screen 1:1 it should be at 640x400/80x25). */
static void door_calc_rect(int vw, int vh, int reserve_bottom, int *ew, int *eh,
                           int *dx, int *dy, int *icol, int *irow)
{
	int cellw, cellh, fitvh;

	door_cell_size(&cellw, &cellh);
	/* TODO (640x384 fractional-downscale / lost 1px detail): the engine is
	 * 640x400 (80x25), but SyncTERM DEFAULTS to an 80x24 / 640x384 canvas --
	 * its status line eats the 25th row -- so EVERY image tier downscales
	 * 0.96x to fit and RA's single-pixel vertical bars drop (text/gauges look
	 * degraded). Crisp only at a true 640x400. Fix: drop the status line with
	 * DECSSDT (CSI 0 $ ~ = "no status display"; cterm then grows to 25 rows via
	 * cterm_resize_rows -> 640x400), FIRST saving the client's current setting
	 * with DECRQSS (DCS $ q $ ~ ST) so door_term_restore() can put it back on
	 * exit. The sixel bottom-row reserve below is a SEPARATE, smaller loss (one
	 * row, sixel only -- JXL/PPM pass reserve_bottom=false and fill the canvas). */
	fitvh = reserve_bottom ? vh - cellh : vh;
	if (fitvh < cellh)   /* degenerate (sub-one-cell) canvas: don't reserve */
		fitvh = vh;
	if (g_fit_fill) {
		*ew = vw;
		*eh = fitvh;
	} else {
		termgfx_geom_fit_ex(vw, fitvh, DOOR_FB_WIDTH, DOOR_FB_HEIGHT,
		                    DOOR_SCALE_MAX, 0, ew, eh);
	}
	/* Sixel caps its emitted width at DOOR_SCALE_MAX (door_emit_sixel, a bandwidth
	 * guard). Fill sets *ew = vw uncapped, so on a canvas wider than the cap the
	 * center below would place a full-width image but the sixel is actually
	 * narrower -> it lands flush-left with a lopsided right margin. Apply the same
	 * cap HERE (sixel only -- reserve_bottom marks the sixel tier; JXL/PPM emit at
	 * full width) so the image is centered symmetrically. The aspect branch already
	 * caps inside termgfx_geom_fit_ex, so this only bites Fill. */
	if (reserve_bottom && *ew > DOOR_SCALE_MAX)
		*ew = DOOR_SCALE_MAX;
	termgfx_geom_center(vw, fitvh, *ew, *eh, cellw, cellh, dx, dy, icol, irow);
}

void door_io_get_rect(int *ew, int *eh, int *dx, int *dy)
{
	int vw   = g_canvas_w > 0 ? g_canvas_w : DOOR_FB_WIDTH;
	int vh   = g_canvas_h > 0 ? g_canvas_h : DOOR_FB_HEIGHT;
	int eff  = (g_tier_force >= 0) ? g_tier_force : sa_auto_tier();
	int icol, irow;

	/* Match the rect to the CURRENT image tier: only sixel reserves the bottom
	 * row, so the mouse maps against the same geometry the frame was drawn in. */
	door_calc_rect(vw, vh, eff == SA_SIXEL, ew, eh, dx, dy, &icol, &irow);
}

/* The terminal's real cell pixel size (canvas/grid derived) -- door_input.c's
 * SGR-mouse mapper needs it to turn a reported text cell into a canvas pixel in
 * the SAME space door_calc_rect() centered the image in. */
void door_io_cell_size(int *cw, int *ch)
{
	door_cell_size(cw, ch);
}

/* True once SGR-Pixels (DEC 1016) is confirmed active: door_input.c's mouse
 * mapper then treats a report's coords as canvas PIXELS, not text cells. */
int door_io_mouse_pixels(void)
{
	return g_mouse_pixels;
}

/* The text grid (cols x rows), for door_input.c's pixel-report auto-detect. */
void door_io_grid(int *cols, int *rows)
{
	*cols = g_grid_cols;
	*rows = g_grid_rows;
}

/* door_input.c calls this when a mouse report's coordinate exceeds the text
 * grid -- proof the terminal is reporting PIXELS (SGR-Pixels/1016 is live)
 * even though the DECRQM handshake didn't confirm it (e.g. a terminal that
 * honors ?1016h but doesn't answer ?1016$p). Latches pixel mode so the rest
 * of the session maps per-pixel. Cell reports never exceed the grid, so this
 * can't false-trigger on a genuinely cell-granular terminal. */
void door_io_note_pixel_report(void)
{
	if (!g_mouse_pixels) {
		g_mouse_pixels = 1;
		fprintf(stderr, DOOR_SHORT_NAME ": SGR-Pixels auto-detected (report exceeded text grid)\n");
	}
}

/* Advance the Ctrl-S fps/throughput window by `emitted` frames; when the
 * rolling ~1s window closes, recompute g_fps (frames/s) and g_bps (bits/s, from
 * g_tx_bytes' delta over the window). Called with 1 per emitted frame and with
 * 0 on a deduped (unchanged) frame, so a still scene decays toward ~0 fps
 * instead of freezing at its last value. */
static void door_fps_tick(int emitted)
{
	uint32_t fnow = door_now_ms();

	g_fps_frames += emitted;
	if (g_fps_win_ms == 0) {
		g_fps_win_ms  = fnow;
		g_tx_bytes_at = g_tx_bytes;
		return;
	}
	if (fnow - g_fps_win_ms >= 1000) {
		uint32_t el = fnow - g_fps_win_ms;
		g_fps         = (int)((uint64_t)g_fps_frames * 1000 / el);
		g_bps         = (uint32_t)((g_tx_bytes - g_tx_bytes_at) * 8000 / el);
		g_fps_frames  = 0;
		g_fps_win_ms  = fnow;
		g_tx_bytes_at = g_tx_bytes;
	}
}

/* Ctrl-S live stats overlay: one condensed line on the bottom text row, in
 * EVERY output mode. Reports the runtime facts a live tester needs to
 * characterise a session without server-side logs -- graphics tier, frame rate,
 * throughput, pacing depth, keyboard-event mode, mouse granularity, client
 * charset, RTT, canvas/grid/cell geometry, and the tier's mean bytes/frame.
 * Session-only; toggled by Ctrl-S in door_io_pump()'s byte parser.
 *
 * The bottom row is the one placement that works across every tier: SIXEL can't
 * paint over it (the fit RESERVES that row, door_calc_rect), and for a layered
 * APC image (JXL/PPM) or the text tiers a text overlay drawn after the frame
 * sits on top of it. (A top overlay was unreadable under sixel, which shares the
 * text plane and repaints with a slow progressive pass.)
 *
 * Built into one buffer and compared against the last emitted (g_stats_last):
 * with `force` false (the deduped/still-frame path) it's re-sent ONLY when the
 * readout changed, so a static screen doesn't re-emit every tick; with `force`
 * true (right after a fresh frame that repainted the row) it always redraws.
 * This is why the overlay does NOT bypass the frame-dedupe -- re-encoding the
 * whole image every tick just to refresh the text is what made it flicker.
 * Autowrap stays off all session (term_enter's DECAWM ?7l), so an over-wide
 * line clips at the right edge instead of wrapping/scrolling the page. */
static void door_stats_draw(int force)
{
	char               buf[256];
	char               t[160], rate[24];
	int                cw = 0, ch = 0, tier, off, brow;
	unsigned long long bpf;

	if (!g_stats_overlay)
		return;
	door_cell_size(&cw, &ch);
	tier = (g_tier_force >= 0) ? g_tier_force : sa_auto_tier();
	brow = g_grid_rows > 0 ? g_grid_rows : 25;

	/* Throughput: total wire TX over the last window, Mbps once it's >= 1.
	 * No space before the unit -- the overlay must fit one 80-col row even with
	 * the blob + fill tokens both present (see the format string below). */
	if (g_bps >= 1000000)
		snprintf(rate, sizeof rate, "%u.%uMbps", g_bps / 1000000, (g_bps % 1000000) / 100000);
	else
		snprintf(rate, sizeof rate, "%uKbps", g_bps / 1000);

	/* Bytes/frame rounded to KB (these frames are 10s-100s of KB) -- keeps the
	 * last field 2-3 digits so the row fits 80 cols; the node log keeps raw bytes. */
	bpf = g_tstat[tier].frames ? (g_tstat[tier].bytes / g_tstat[tier].frames + 512) / 1024 : 0;
	snprintf(t, sizeof t, " %s %dfps %s d%d %s/%s %s%s%s %ums %dx%d %dx%d %dx%d %lluKB/f",
	         sa_tier_name(tier), g_fps, rate, g_auto_depth,
	         door_io_evdev_active()      ? "evdev"
	         : door_input_kitty_active() ? "kitty" : "legacy",
	         g_mouse_pixels ? "pixel" : "cell",
	         door_term_is_utf8() ? "utf8" : "cp437",
	         (g_img_blob_ok && (tier == SA_JXL || tier == SA_PPM)) ? " blob" : "",   /* frames shipping inline (Draw*Blob), no cache */
	         g_fit_fill ? " fill" : "",   /* Ctrl-F Fill (stretch-to-canvas); absent = Aspect (default, true ratio) */
	         (unsigned)g_rtt_ms,
	         g_canvas_w, g_canvas_h, g_grid_cols, g_grid_rows, cw, ch, bpf);
	/* \x1b[K (erase to end of line) BEFORE the reset clears any leftover tail
	 * from a previous, longer readout (the line is variable length) and, since
	 * the cyan attribute is still active, paints the rest of the row as a clean
	 * full-width status bar via background-color erase. */
	off = snprintf(buf, sizeof buf, "\x1b[%d;1H\x1b[30;46m%s\x1b[K\x1b[0m", brow, t);
	if (off < 0 || off >= (int)sizeof buf)
		return;

	if (!force && strcmp(buf, g_stats_last) == 0)
		return;   /* unchanged readout on the still-frame path -- don't re-emit */
	snprintf(g_stats_last, sizeof g_stats_last, "%s", buf);
	door_io_send(buf, (size_t)off);
}

void door_io_present(const uint8_t *fb, const uint8_t *pal768)
{
	static int      cleared;
	static uint32_t cleared_ms;
	uint8_t         pal8[768];
	int             tier;
	int             fit_changed;
	int             geom_changed;
	int             pal_dirty;
	int             vw, vh;
	size_t          n = 0;

	if (!g_inited)
		door_io_init();

	door_check_shutdown();    /* Task 5: SIGTERM/SIGHUP -- exits (via atexit) if requested */
	door_check_time_limit();  /* Task 5: DOOR32.SYS time-left -- v1 warn-only */
	door_node_tick();         /* Task 5: node.exb status refresh + pending Ctrl-U/Ctrl-P */

	scale_palette(pal768, pal8);

	if (g_file_mode) {
		/* Capture mode: always a self-contained full-res frame (palette
		 * included), no pacing/de-dupe -- the file just holds the latest
		 * frame for offline decode (the SYNCALERT_SIXELOUT regression test). */
		n = sixel_encode(&g_sixel_buf, &g_sixel_cap, fb, DOOR_FB_WIDTH, DOOR_FB_HEIGHT, pal8, 1);
		door_out_put(g_sixel_buf, n);
		door_out_flush();
		return;
	}

	/* Once, on entry: clear+home, hide cursor, no autowrap, reset sixel
	 * scrolling, probe the terminal's pixel canvas, DA1+CTDA (sixel/
	 * SyncTERM detect), Q;JXL (JXL detect). door_io_pump()'s parser
	 * resolves the replies. */
	if (!cleared) {
		door_out_puts(termgfx_term_enter);
		/* Hide the client's status line (DECSSDT) BEFORE probing the canvas, so
		 * SyncTERM's default 80x24/640x384 grows back to a true 80x25/640x400
		 * and the probe reports the reclaimed size -- the game then renders 1:1
		 * with no fractional downscale (see door_calc_rect's TODO). The DECRQSS
		 * query it carries lets door_io_pump() capture the pre-door setting for
		 * restore in door_term_restore(). */
		door_out_puts(termgfx_term_status_off);
		door_out_puts(termgfx_term_probe);
		door_out_puts("\x1b[c\x1b[<c");
		door_out_puts(termgfx_query_jxl);
		door_out_puts(termgfx_keymode_query_kitty);   /* kitty keyboard-protocol query (Task 3, door_input.c) */
		door_out_puts("\x1b[?1003h\x1b[?1006h"); /* SGR mouse tracking, always on -- no steering
		                                           * toggle here (unlike SyncDuke's Ctrl-O), the
		                                           * RTS mouse cursor/clicks are always wanted */
		/* Ask for SGR-Pixels (DEC 1016): pixel-granular mouse instead of
		 * cell-granular, so a fine control (the in-game volume slider) is
		 * draggable per-pixel. Then DECRQM-query it; door_csi_final()'s 'y'
		 * case only flips g_mouse_pixels on if the terminal confirms the mode
		 * is set. Windows Terminal supports it; SyncTERM ignores both and
		 * stays cell-granular (its reply, if any, is Ps=0). Harmless on a
		 * terminal that supports neither. */
		door_out_puts("\x1b[?1016h\x1b[?1016$p");
		termgfx_audio_probe(g_audio);    /* Q;libsndfile: SyncTERM replies ESC[=7;100;{0,1}n -> digital tier */
		door_dsr_sent(door_now_ms());   /* the probe's ESC[6n is a DSR too */
		cleared    = 1;
		cleared_ms = door_now_ms();
	}

	/* Startup grace: when auto-selecting, hold the first frame(s) until the
	 * capability probe answers, so a no-sixel terminal never sees a sixel
	 * frame. Cap the wait so a silent terminal still gets frames. */
	if (g_tier_force < 0 && !g_probe_replied
	    && (int32_t)(door_now_ms() - cleared_ms) < DOOR_PROBE_GRACE_MS) {
		door_out_flush();
		return;
	}

	door_out_flush();   /* drain whatever's left of the previous frame first */

	/* DSR-ACK pacing gate: don't get ahead of what the terminal has drawn. */
	if (g_inflight >= door_eff_depth()) {
		if ((int32_t)(door_now_ms() - g_pace_progress_ms) > DOOR_PACE_DEADLINE_MS) {
			g_inflight = 0;
			g_dsr_t    = g_dsr_h;   /* drop the stale unacked DSR timestamps */
		} else
			return;
	}
	if (g_out_off < g_out_len)   /* prior frame's bytes not all out yet */
		return;

	tier = (g_tier_force >= 0) ? g_tier_force : sa_auto_tier();
#ifndef WITH_JXL
	if (tier == SA_JXL)
		tier = SA_SIXEL;
#endif

	vw = g_canvas_w > 0 ? g_canvas_w : DOOR_FB_WIDTH;
	vh = g_canvas_h > 0 ? g_canvas_h : DOOR_FB_HEIGHT;

	/* One-time geometry diagnostic: the resolved canvas/grid/cell + fitted
	 * image rect the mouse mapper and image tiers both key off. Logged once,
	 * after the probes have had a chance to answer, so a mispositioned cursor
	 * or bounced image can be diagnosed from a sysop's node log without a live
	 * capture. */
	{
		static int geom_logged;
		if (!geom_logged && g_probe_replied) {
			int cw, ch, ew, eh, dx, dy, icol, irow;
			int et = (g_tier_force >= 0) ? g_tier_force : sa_auto_tier();
			door_cell_size(&cw, &ch);
			door_calc_rect(vw, vh, et == SA_SIXEL, &ew, &eh, &dx, &dy, &icol, &irow);
			fprintf(stderr,
			        DOOR_SHORT_NAME ": geometry canvas=%dx%d%s grid=%dx%d cell=%dx%d "
			        "mouse=%s tier=%d fit=%s -> image=%dx%d @px(%d,%d) cell(%d,%d)\n",
			        g_canvas_w, g_canvas_h, g_px_exact ? "(exact)" : "(est)",
			        g_grid_cols, g_grid_rows, cw, ch,
			        g_mouse_pixels ? "pixels(1016)" : "cells",
			        (g_tier_force >= 0) ? g_tier_force : sa_auto_tier(),
			        g_fit_fill ? "fill" : "aspect", ew, eh, dx, dy, icol, irow);
			geom_logged = 1;
		}
	}

	fit_changed  = (g_last_fit != g_fit_fill);
	geom_changed = (g_last_canvas_w != vw || g_last_canvas_h != vh);
	pal_dirty    = !g_have_last || memcmp(g_last_pal, pal8, 768) != 0;

	/* Whole-frame de-dupe: fb + palette + tier + fit-mode + canvas geometry
	 * all unchanged since the last SENT frame -> nothing new to draw. Canvas
	 * geometry (vw/vh, which size/center every emitted frame) is included
	 * because an exact-pixel probe reply can land after the first frame
	 * already went out, changing g_canvas_w/h without touching fb/pal/tier/
	 * fit -- syncduke_io.c:1113-1119 folds in last_w/last_h the same way.
	 * Task 5: the who's-online/page-compose overlay is a SEPARATE set of
	 * cursor-positioned escapes appended after the frame (below), so a
	 * static game frame (e.g. sitting at the menu) would otherwise never
	 * repaint while the overlay is counting down or capturing typed text --
	 * bypass the dedupe for as long as it's showing. The Ctrl-S stats overlay
	 * does NOT bypass it (that re-encoded the whole image every tick and made
	 * the strip flicker under sixel/JXL/PPM); instead, on the deduped path it
	 * refreshes JUST the overlay text below, leaving the image untouched. */
	if (!fit_changed && !geom_changed && g_have_last && g_last_tier == tier
	    && !pal_dirty && !door_node_overlay_active()
	    && memcmp(g_last_fb, fb, DOOR_FB_BYTES) == 0) {
		door_fps_tick(0);   /* keep the window ticking so a still scene reads ~0 */
		if (g_stats_overlay) {
			door_stats_draw(0);
			door_out_flush();
		}
		return;
	}

	/* Tier change requested a screen clear (see door_tier_cycle()): wipe the
	 * old tier's content so a letterboxed image doesn't sit on a floor of
	 * stale text/pixels. Force the text renderer to re-init too (g_rt_cols=0)
	 * so its next frame is a FULL redraw, not a diff against the now-blank
	 * screen it can't see was cleared. */
	if (g_clear_pending) {
		door_out_puts("\x1b[2J\x1b[H");
		g_rt_cols = g_rt_rows = 0;
		g_clear_pending = 0;
	}

	if (sa_is_text_tier(tier)) {
		rt_mode_t m = (tier == SA_QUADRANT) ? RT_QUADRANT
		            : (tier == SA_SEXTANT)  ? RT_SEXTANT : RT_HALF;
		n = door_emit_text(fb, pal8, m, vw, vh);
	} else {
		/* Image tiers: fit the native frame into the canvas -- Aspect
		 * (default, true ratio) or Fill (stretch), DESIGN.md -- and center
		 * it (door_calc_rect(), shared with door_io_get_rect() for SGR
		 * mouse mapping). */
		int ew, eh, dx = 0, dy = 0, icol = 1, irow = 1;
		int emit_pal;

		/* Only sixel reserves the bottom row (see door_calc_rect); JXL/PPM
		 * are pixel-positioned and fill the full canvas. */
		door_calc_rect(vw, vh, tier == SA_SIXEL, &ew, &eh, &dx, &dy, &icol, &irow);

		/* SyncTERM persists sixel color registers across images -- (re)send
		 * them only on a real palette change, or when (re)entering the
		 * sixel tier from something else (which never defined them). Other
		 * terminals reset registers per image, so send every frame there. */
		emit_pal = pal_dirty || !g_is_syncterm || g_last_tier != SA_SIXEL;

		if (tier == SA_SIXEL) {
			door_cursor_save_to(irow, icol);
			n = door_emit_sixel(fb, pal8, ew, eh, emit_pal);
			door_out_put(g_sixel_buf, n);
			door_cursor_restore();
		}
#ifdef WITH_JXL
		else if (tier == SA_JXL) {
			n = door_emit_jxl(fb, pal8, ew, eh, dx, dy);
			if (n != 0)
				door_out_put(g_apc_buf, n);
			else {   /* encode failure this frame: fall back to sixel */
				door_cursor_save_to(irow, icol);
				n = door_emit_sixel(fb, pal8, ew, eh, 1);
				door_out_put(g_sixel_buf, n);
				door_cursor_restore();
				tier = SA_SIXEL;
			}
		}
#endif
		else {   /* SA_PPM */
			n = door_emit_ppm(fb, pal8, ew, eh, dx, dy);
			if (n != 0)
				door_out_put(g_apc_buf, n);
		}
	}

	door_stat_record(tier, n);   /* per-tier bandwidth accounting (JXL vs sixel) */

	door_fps_tick(1);   /* one emitted frame -> fps/throughput window (Ctrl-S overlay) */

	/* Task 5: who's-online / page-compose overlay, on top of the frame just
	 * staged above (its own cursor-positioned escapes -- door_node_draw()
	 * no-ops instantly when nothing is showing). Text-grid cols/rows, same
	 * estimate door_csi_final()'s 999;999 probe reply fills; 80x25 default
	 * before any reply lands. */
	door_node_draw(g_grid_cols > 0 ? g_grid_cols : 80, g_grid_rows > 0 ? g_grid_rows : 25);

	/* Ctrl-S debug stats, drawn AFTER the node overlay so it wins the top-right
	 * cells the node strip's ERASE-TO-EOL would otherwise blank. force=1: the
	 * fresh frame just repainted these cells, so always redraw. */
	door_stats_draw(1);

	door_out_put("\x1b[6n", 4);   /* DSR: paces both image and text tiers */
	g_inflight++;
	g_pace_progress_ms = door_now_ms();
	door_dsr_sent(g_pace_progress_ms);

	memcpy(g_last_fb, fb, DOOR_FB_BYTES);
	memcpy(g_last_pal, pal8, 768);
	g_last_tier      = tier;
	g_last_fit       = g_fit_fill;
	g_last_canvas_w  = vw;
	g_last_canvas_h  = vh;
	g_have_last      = 1;

	door_out_flush();
}

/* --- minimal input parser: probe replies + F4/Ctrl-F, else forward --------- */
static enum { P_NORMAL, P_ESC, P_CSI, P_APC, P_APC_ESC } g_pstate;
static char g_csi_intro;
static char g_csi_par[40];
static int  g_csi_len;
static int  g_apc_len;

/* Parse up to `max` ';'-separated decimal params from g_csi_par (a leading
 * non-digit marker byte like '<'/'='/'?' is simply skipped). */
static int door_csi_params(int *out, int max)
{
	int n = 0, i, have = 0, v = 0;
	for (i = 0; i <= g_csi_len && n < max; i++) {
		char c = (i < g_csi_len) ? g_csi_par[i] : ';';
		if (c >= '0' && c <= '9') { v = v * 10 + (c - '0'); have = 1; }
		else if (c == ';') {
			if (have)
				out[n++] = v;
			v = 0;
			have = 0;
		}
	}
	return n;
}

static void door_csi_final(char fin)
{
	int p[16], np;

	switch (fin) {
		case 'R':   /* ESC[rows;colsR: the 999;999 grid probe, and every DSR ack */
			np = door_csi_params(p, 2);
			if (np >= 2 && g_grid_rows == 0) {
				g_grid_rows = p[0];
				g_grid_cols = p[1];
				if (!g_px_exact) {
					g_canvas_w = g_grid_cols * 8;
					g_canvas_h = g_grid_rows * 16;
				}
			}
			door_pace_ack();
			return;
		case 't':   /* ESC[4;h;wt: exact text-area pixel size (authoritative) */
			np = door_csi_params(p, 4);
			if (np >= 3 && p[0] == 4) {
				g_canvas_h = p[1];
				g_canvas_w = p[2];
				g_px_exact = 1;
			}
			return;
		case 'y':   /* DECRPM: ESC[?1016;Ps$y -- reply to our SGR-Pixels DECRQM.
		             * Ps 1/3 = mode set (pixel mouse active); 0 = unrecognized
		             * (SyncTERM), 2/4 = reset -- all leave us in cell mode. */
			np = door_csi_params(p, 2);
			if (np >= 2 && p[0] == 1016)
				g_mouse_pixels = (p[1] == 1 || p[1] == 3);
			return;
		case 'c':   /* DA1 (ESC[c) / CTDA (ESC[<c) device-attributes reply */
			g_probe_replied = 1;
			if (g_csi_len > 0 && (g_csi_par[0] == '<' || g_csi_par[0] == '=')) {
				g_is_syncterm = 1;
				g_have_sixel  = 1;
			}
			np = door_csi_params(p, 16);
			{
				int k;
				for (k = 0; k < np; k++) {
					if (p[k] == 4)
						g_have_sixel = 1;
					/* CTDA cap 8 = physical key (evdev) reports. Prefer it over
					 * kitty (a SyncTERM that advertises it doesn't speak kitty
					 * anyway): enable reports (=1h) and suppress the translated
					 * byte stream (=2h), so keys arrive only as the CSI=<code>K/k
					 * edges door_input.c's evdev decoder handles. */
					if (p[k] == 8 && g_csi_par[0] == '<') {
						char   ks[TERMGFX_KEYMODE_SEQ_MAX];
						size_t kn = termgfx_keymode_enable_evdev(&g_km, ks, sizeof ks,
						                                         door_now_ms());

						if (kn > 0)
							door_out_put(ks, kn);   /* arms the held-key settle window */
					}
				}
			}
			/* CTerm >= 1.329 adds the APC blob media verbs. The CTDA reply has no
			 * version field, but the DA1 reply ("ESC[=67;84;101;114;109;MAJ;MIN;
			 * ...c") does -- enable inline A;LoadBlob audio streaming so cutscene
			 * chunks skip the C;S cache entirely (no-op on older SyncTERM). */
			if (termgfx_caps_cterm_version(p, np, (char)g_csi_par[0]) >= TERMGFX_CTERM_VER_BLOB) {
				termgfx_audio_set_blob_ok(g_audio, 1);
				g_img_blob_ok = 1;   /* DrawJXLBlob/DrawPPMBlob: inline video frames */
			}
			return;
		case 'n':   /* CTerm state report: our Q;JXL reply is ESC[=1;{0,1}n */
			if (g_csi_len > 0 && g_csi_par[0] == '=') {
				uint8_t seq[48];
				int     sl = 0, k, r;
				seq[sl++] = 0x1b;
				seq[sl++] = (uint8_t)g_csi_intro;
				for (k = 0; k < g_csi_len && sl < (int)sizeof(seq) - 1; k++)
					seq[sl++] = (uint8_t)g_csi_par[k];
				seq[sl++] = (uint8_t)fin;
				r = termgfx_caps_parse_jxl(seq, sl);
				if (r >= 0) {
					g_is_syncterm   = 1;
					g_jxl_supported = (r == 1);
				}
			}
			return;
		case 'S':
			/* termgfx_term_probe also sends XTSMGRAPHICS (ESC[?2;1S), which
			 * SyncTERM answers with ESC[?2;0;W;HS -- the real graphics-canvas
			 * pixels, preferred over the ESC[14t/grid estimate. Only a BARE
			 * CSI S (kitty F4, no params) or SS3 "ESC O S" means the F4 key;
			 * anything else starting with '?' is this probe reply. */
			if (g_csi_len > 0 && g_csi_par[0] == '?') {
				np = door_csi_params(p, 4);
				if (np >= 4 && p[0] == 2 && p[1] == 0) {
					g_canvas_w = p[2];
					g_canvas_h = p[3];
					g_px_exact = 1;
				}
				return;
			}
			door_tier_cycle();   /* SS3 "ESC O S" or bare CSI "ESC[S" -- F4 */
			return;
		default:
			/* Not one of our recognized probe replies -- forward the raw
			 * sequence, in order, to the Task-3 input hook. This parser
			 * only claims what it understands; real keys/mouse reports
			 * are preserved for the real input parser. */
			door_input_byte(0x1b);
			door_input_byte((uint8_t)g_csi_intro);
			{
				int k;
				for (k = 0; k < g_csi_len; k++)
					door_input_byte((uint8_t)g_csi_par[k]);
			}
			door_input_byte((uint8_t)fin);
			return;
	}
}

void door_io_pump(void)
{
	uint8_t buf[256];
	int     n, i;

	if (!g_inited)
		door_io_init();
	door_check_shutdown();   /* Task 5: SIGTERM/SIGHUP -- exits (via atexit) if requested */
	SoundImp_Music_Housekeep();   /* fire a pending "stop music" once no theme replaced it */
	if (g_file_mode)
		return;   /* offline capture: no client to read from */

#ifdef _WIN32
	if (!g_use_sock)
		return;
	n = recv(g_iosock, (char *)buf, (int)sizeof buf, 0);
	if (n == SOCKET_ERROR) {
		int e = WSAGetLastError();
		if (e == WSAEWOULDBLOCK || e == WSAENOBUFS || e == WSAEINTR)
			return;
		if (g_cli_sock >= 0) {
			char r[48];
			snprintf(r, sizeof r, "input recv error wsa=%d", e);
			door_hangup(r);
		}
		return;
	}
	if (n == 0) {
		if (g_cli_sock >= 0)
			door_hangup("client closed (input EOF)");
		return;
	}
#else
	n = (int)read(g_fd_in, buf, sizeof buf);
	if (n <= 0) {
		if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
			return;
		if (g_cli_sock >= 0)   /* only a real door socket hanging up ends the process */
			door_hangup(n == 0 ? "client closed (input EOF)" : "input read error");
		return;
	}
#endif

	termgfx_audio_feed(g_audio, buf, n);   /* resolve the audio cap probe + any C;L cache-list
	                                         * reply -- a raw-byte scan, tolerant of everything
	                                         * else (keys, DSR acks) interleaved; independent of
	                                         * the P_APC swallow below (audio_mgr.h). */

	/* Capture the DECRQSS reply to our DECSSDT status-line query (the pre-door
	 * status type, for restore on exit). A raw-byte scan like the audio one --
	 * the DCS reply is otherwise swallowed by the P_APC state below -- with a
	 * small rolling window to bridge a reply split across reads. */
	if (g_status_type < 0) {
		static uint8_t sacc[32];
		static int     sacclen;
		int            r = termgfx_term_parse_status(buf, n);

		if (r < 0) {
			if (n >= (int)sizeof sacc) {
				memcpy(sacc, buf + (n - (int)sizeof sacc), sizeof sacc);
				sacclen = (int)sizeof sacc;
			} else {
				int keep = (int)sizeof sacc - n;
				if (sacclen > keep) {
					memmove(sacc, sacc + (sacclen - keep), keep);
					sacclen = keep;
				}
				memcpy(sacc + sacclen, buf, n);
				sacclen += n;
			}
			r = termgfx_term_parse_status(sacc, sacclen);
		}
		if (r >= 0)
			g_status_type = r;
	}
	{
		static int sa_prev_tier = -2;   /* log the negotiated audio tier once it resolves */
		int        t = termgfx_audio_tier(g_audio);
		if (t != sa_prev_tier) {
			sa_prev_tier = t;
			fprintf(stderr, DOOR_SHORT_NAME ": audio tier=%d (%s)\n", t,
			        t == 1 ? "digital -- SFX/music should play" :
			        t == 0 ? "audio APC but no libsndfile -- silent" :
			        "no audio APC reply -- old SyncTERM or no audio");
		}
	}

	for (i = 0; i < n; i++) {
		uint8_t c = buf[i];
		switch (g_pstate) {
			case P_NORMAL:
				/* Task 5: Ctrl-P page-compose capture -- while composing, EVERY
				 * plain byte (this parser's raw-byte level, same as F4/Ctrl-F
				 * below -- a "minimal v1" scope per the task brief; a kitty-
				 * negotiated client's OWN letter/digit keys still arrive as
				 * plain bytes here too, only MODIFIED keys get CSI-u wrapped)
				 * goes to the compose line instead of normal dispatch. A bare
				 * Escape always cancels, even if the user meant to start an
				 * arrow-key CSI sequence -- accepted simplification (DEFERRED.md
				 * candidate, not a v1 promise). */
				if (door_node_composing()) {
					if (c == 0x1b || c == '\r' || c == '\n' || c == 0x08 || c == 0x7f
					    || (c >= 0x20 && c < 0x7f))
						door_node_compose_key((int)c);
					break;
				}
				if (c == 0x1b)
					g_pstate = P_ESC;
				else if (c >= 0x01 && c <= 0x1a && door_io_hotkey(c | 0x60, 1)) {
					/* Ctrl+letter door hotkey (Ctrl-D/F/U/P/S) consumed as a raw
					 * control byte -- the legacy (non-evdev/kitty) key path. The
					 * evdev/kitty paths reach door_io_hotkey() from door_input.c
					 * instead, since there these arrive as decoded key events. */
				} else if ((c == '+' || c == '=' || c == '-' || c == '_')
				           && door_io_hotkey(c, 1)) {
					/* +/- music-volume hotkey as a plain byte (legacy + kitty --
					 * evdev/SyncTERM reaches door_io_hotkey() via door_input.c). */
				} else
					door_input_byte(c);
				break;
			case P_ESC:
				if (c == '[' || c == 'O') {
					g_pstate    = P_CSI;
					g_csi_intro = (char)c;
					g_csi_len   = 0;
				} else if (c == '_' || c == 'P' || c == ']' || c == '^') {
					g_pstate = P_APC;   /* APC/DCS/OSC/PM string: swallow to ST/BEL */
					g_apc_len = 0;
				} else {
					door_input_byte(0x1b);
					g_pstate = P_NORMAL;
					i--;                /* reprocess c as a normal byte */
				}
				break;
			case P_APC:
				if (c == 0x1b)
					g_pstate = P_APC_ESC;
				else if (c == 0x07)
					g_pstate = P_NORMAL;
				else if (++g_apc_len > (1 << 20))
					g_pstate = P_NORMAL;   /* unterminated: bail rather than lock up */
				break;
			case P_APC_ESC:
				g_pstate = (c == '\\') ? P_NORMAL : P_APC;
				break;
			case P_CSI:
				if (c >= 0x40 && c <= 0x7e) {
					door_csi_final((char)c);
					g_pstate = P_NORMAL;
				} else if (g_csi_len < (int)sizeof g_csi_par) {
					g_csi_par[g_csi_len++] = (char)c;
				}
				break;
		}
	}
}

/* --- ring buffer feeding door_input.c's parser (door.h's Task-2 hook) ------ */
#define DOOR_INPUT_RING 256
static uint8_t g_input_ring[DOOR_INPUT_RING];
static int     g_input_head, g_input_tail;

void door_input_byte(uint8_t b)
{
	int next = (g_input_head + 1) % DOOR_INPUT_RING;
	if (next == g_input_tail)          /* full: drop the oldest byte rather than block */
		g_input_tail = (g_input_tail + 1) % DOOR_INPUT_RING;
	g_input_ring[g_input_head] = b;
	g_input_head = next;
}

/* door_input.c's drain side of the ring above. */
int door_io_input_pop(uint8_t *b)
{
	if (g_input_head == g_input_tail)
		return 0;
	*b = g_input_ring[g_input_tail];
	g_input_tail = (g_input_tail + 1) % DOOR_INPUT_RING;
	return 1;
}

/* door_input.c's only way to write to the wire (kitty push-flags reply) --
 * just stages the bytes into the same buffer door_io_present() flushes. */
void door_io_send(const void *buf, size_t len)
{
	door_out_put(buf, len);
}

/* User's alias/handle parsed from DOOR32.SYS line 7, or "" if no DOOR32.SYS
 * was given or the alias line was empty. Used by door_node.c for Ctrl-P page
 * "from" field. Mirrors syncduke_door_alias(). */
const char *door_io_alias(void)
{
	return g_alias;
}
