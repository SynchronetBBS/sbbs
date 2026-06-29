/*
 * syncduke_config.c -- syncduke.ini parsing, GRP directory resolution, and routing
 * the engine's diagnostics to stderr for a door.
 *
 * Kept separate from syncduke_door.c so the door/socket and input layers stay free of
 * the xpdev link dependency (the keymap unit test links those directly).
 *
 * A glibc .init_array constructor runs before the engine's main():
 *  1. Resolve the read-only GRP directory to an ABSOLUTE path (exported as
 *     syncduke_grpdir, which the engine's findGRPToUse() consults) and chdir into
 *     the user's per-user -home dir so Duke's config (duke3d.cfg) and savegames land
 *     THERE, not in the GRP dir shared by every player -- mirrors syncdoom's -home.
 *     GRP order: -grpdir <path> arg, then syncduke.ini [grp] dir (xpdev ini_file,
 *     the same parser syncdoom uses), else the launch dir.  No -home => keep the GRP
 *     dir as CWD (the old single-location behavior).
 *  2. When running as a door, point stdout at stderr. The Duke engine logs
 *     everything (banner, "Scanning directory ...", and Error()'s "Could not
 *     initialize ... GRP") via printf/vprintf to STDOUT, which Synchronet does
 *     not log -- but it DOES intercept the door's stderr and write it to syslog /
 *     sbbs.log. Routing stdout to stderr makes a GRP/permission error visible to
 *     the sysop in the BBS logs, no separate file needed. (For a socket-I/O door,
 *     fd 0/1/2 are not the client socket, so this never touches our sixel, which
 *     goes out the door socket fd in syncduke_io.c.) Dev/tty mode is left alone.
 */

#include <stdio.h>
#include <stdlib.h>     /* realpath / _fullpath */
#include <string.h>
#include <limits.h>     /* PATH_MAX */

#ifdef _WIN32
  #include <direct.h>   /* chdir / _fullpath */
  #include <io.h>       /* dup2 */
  #define strcasecmp _stricmp
  #ifndef STDOUT_FILENO
	#define STDOUT_FILENO 1
  #endif
  #ifndef STDERR_FILENO
	#define STDERR_FILENO 2
  #endif
#else
  #include <unistd.h>   /* chdir, dup2, realpath, STD*_FILENO */
#endif

#ifndef PATH_MAX
  #define PATH_MAX 1024
#endif

#ifdef _WIN32
/* realpath(path, resolved) -> _fullpath(resolved, path, max) (note arg order). */
  #define realpath(path, resolved) _fullpath((resolved), (path), PATH_MAX)
#endif

#include "ini_file.h"   /* xpdev: iniReadFile / iniGetString */
#include "dirwrap.h"    /* xpdev: mkpath() (recursive mkdir) */
#include "sbbs_node.h"  /* termgfx: sbbs_my_node() -- current node # from SBBSNNUM */

/* Shared, read-only GRP directory (absolute), consulted by the engine's findGRPToUse()
 * (Game/src/game.c).  Deliberately separate from the process CWD: when launched as a door
 * with -home, we chdir into the user's per-user storage dir so Duke's config (duke3d.cfg)
 * and savegames land THERE, not in the GRP dir shared by every player.  Empty => the engine
 * keeps its stock CWD scan (standalone/dev runs). */
char syncduke_grpdir[PATH_MAX] = "";

/* Net role/port/peer from the command line (the lobby's path), consumed by
 * syncduke_net.c (which falls back to the SYNCDUKE_NET* env vars when unset).
 * Defined in syncduke_net.c; we fill them here in the pre-main arg pass. */
extern char syncduke_net_role[16];
extern char syncduke_net_port[16];
extern char syncduke_net_peer[128];

/* Whether the in-game RECORD (demo) menu option is offered + actually records.
 * Default OFF: a demo file (demo1.dmo) is useless to a BBS user who can't download
 * it and just wastes disk. Enable with syncduke.ini [game] record = true. */
static int syncduke_allow_record;
int syncduke_record_enabled(void) { return syncduke_allow_record; }

/* Max emitted JXL image width in px (the tier scales Duke's frame up to fill the
 * canvas, capped here so a huge window can't produce an enormous frame).  0 =
 * uncapped.  syncduke.ini [video] scale_max; default matches SyncDOOM's 1280. */
static int syncduke_scale_max = 1280;
int syncduke_jxl_scale_max(void) { return syncduke_scale_max; }

/* Max width (px) of the SIXEL image.  The door fits Duke's frame into the terminal's
 * probed window up to this; a larger window fills more but costs more wire bytes (sixel
 * is RLE).  syncduke.ini [video] sixel_max_width; default 1024 (fills a large-font 80-col
 * window).  Clamped to [320, 1024] (the encoded buffer's hard ceiling). */
static int syncduke_sixel_w = 1024;
int syncduke_sixel_max_w(void) { return syncduke_sixel_w; }

/* [video] use_cell_size (default true): when the terminal didn't report exact pixels
 * (ESC[14t), estimate its window from the reported cell size (ESC[16t) instead of an
 * 8x16 font.  Defined in syncduke_input.c; we set it here from the ini. */
extern int syncduke_use_cell_size;

/* True if argv looks like a door launch: a DOOR32.SYS drop file (Synchronet %f)
 * or a -s<fd> socket arg. */
static int syncduke_is_door(int argc, char **argv)
{
	int i;

	for (i = 1; i < argc; i++) {
		const char *a = argv[i];
		const char *b;
		if (a[0] == '-' && a[1] == 's' && a[2] >= '0' && a[2] <= '9')
			return 1;
		b = a + strlen(a);
		while (b > a && b[-1] != '/' && b[-1] != '\\')
			b--;
		if (strcasecmp(b, "door32.sys") == 0)
			return 1;
	}
	return 0;
}

/* The pre-main initializer body; registered as a constructor below (portably). */
static void syncduke_config_init(int argc, char **argv)
{
	char  dir[INI_MAX_VALUE_LEN] = "";
	char  home[PATH_MAX] = "";
	char  logpath[INI_MAX_VALUE_LEN] = "";
	char  abs[PATH_MAX];
	int   i;
	FILE *f;

	/* --- GRP dir (-grpdir <path> / [grp] dir), per-user -home <path>, debug -log <path> --- */
	for (i = 1; i + 1 < argc; i++) {
		if (strcmp(argv[i], "-grpdir") == 0)
			strncpy(dir, argv[i + 1], sizeof(dir) - 1);
		else if (strcmp(argv[i], "-home") == 0)
			strncpy(home, argv[i + 1], sizeof(home) - 1);
		else if (strcmp(argv[i], "-log") == 0)
			strncpy(logpath, argv[i + 1], sizeof(logpath) - 1);
		else if (strcmp(argv[i], "-netrole") == 0)
			strncpy(syncduke_net_role, argv[i + 1], sizeof(syncduke_net_role) - 1);
		else if (strcmp(argv[i], "-netport") == 0)
			strncpy(syncduke_net_port, argv[i + 1], sizeof(syncduke_net_port) - 1);
		else if (strcmp(argv[i], "-netpeer") == 0)
			strncpy(syncduke_net_peer, argv[i + 1], sizeof(syncduke_net_peer) - 1);
	}
	if ((f = fopen("syncduke.ini", "r")) != NULL) {
		str_list_t ini = iniReadFile(f);
		fclose(f);
		if (!dir[0])
			iniGetString(ini, "grp", "dir", "", dir);
		if (!logpath[0])
			iniGetString(ini, "debug", "log", "", logpath);   /* blank => logging off */
		syncduke_allow_record = iniGetBool(ini, "game", "record", FALSE);   /* demos off by default */
		syncduke_scale_max = iniGetInteger(ini, "video", "scale_max", syncduke_scale_max);
		syncduke_sixel_w = iniGetInteger(ini, "video", "sixel_max_width", syncduke_sixel_w);
		if (syncduke_sixel_w < 320)
			syncduke_sixel_w = 320;                             /* never below native-ish width */
		if (syncduke_sixel_w > 1024)
			syncduke_sixel_w = 1024;                            /* the encoded buffer's hard ceiling */
		syncduke_use_cell_size = iniGetBool(ini, "video", "use_cell_size", TRUE);
		strListFree(&ini);
	}

	/* Resolve the GRP dir to an ABSOLUTE path NOW, while CWD is still the launch dir, so it
	 * keeps resolving after we chdir into the per-user home below.  Blank => the launch dir. */
	if (realpath(dir[0] ? dir : ".", abs) != NULL)
		strncpy(syncduke_grpdir, abs, sizeof(syncduke_grpdir) - 1);
	else if (dir[0])
		strncpy(syncduke_grpdir, dir, sizeof(syncduke_grpdir) - 1);

	/* Per-user storage: chdir into -home so Duke's config/saves land there (mirrors syncdoom;
	 * the BBS passes e.g. data/user/####/duke/).  No -home => keep the GRP dir as CWD, the old
	 * single-location behavior. */
	if (home[0]) {
		mkpath(home);
		if (chdir(home) != 0)
			fprintf(stderr, "syncduke: cannot enter per-user dir '%s'\n", home);
	}
	else if (syncduke_grpdir[0] && chdir(syncduke_grpdir) != 0)
		fprintf(stderr, "syncduke: cannot chdir to GRP dir '%s'\n", syncduke_grpdir);

	/* --- optional file debug log (off unless env SYNCDUKE_LOG / -log / [debug] log is set).
	 * Set the path + install the crash handler now, before the engine runs, so a fault or
	 * hangup is recorded to a plain file (we get neither stderr capture nor WER dumps here).
	 * A relative path lands in CWD -- i.e. the per-user -home dir just entered above.
	 * Tag the filename with the node number so two co-op doors the same user runs on
	 * different nodes -- which share one per-user dir -- don't clobber each other's log.
	 * sbbs_my_node() reads SBBSNNUM (SBBS sets it for every external program), falling
	 * back to SBBSNODE's trailing digits; <=0 (dev/standalone) -> leave it as configured. */
	if (logpath[0]) {
		int node = sbbs_my_node();
		if (node > 0) {
			char  tagged[INI_MAX_VALUE_LEN];
			char *dot = strrchr(logpath, '.');
			char *sep = strrchr(logpath, '/');
#ifdef _WIN32
			char *bs = strrchr(logpath, '\\');
			if (bs > sep)
				sep = bs;
#endif
			if (dot != NULL && dot > sep)   /* insert ".<node>" before the extension */
				snprintf(tagged, sizeof tagged, "%.*s.%d%s",
				         (int)(dot - logpath), logpath, node, dot);
			else                            /* no extension -> append ".<node>" */
				snprintf(tagged, sizeof tagged, "%s.%d", logpath, node);
			strncpy(logpath, tagged, sizeof(logpath) - 1);
			logpath[sizeof(logpath) - 1] = '\0';
		}
	}
	syncduke_log_set_path(logpath);
	syncduke_log_init();
	syncduke_log("start: argc=%d socket=%d grpdir='%s' home='%s'",
	             argc, syncduke_door_socket(), syncduke_grpdir, home[0] ? home : ".");

	/* --- route the engine's stdout diagnostics to stderr (Synchronet logs it) --- */
	if (syncduke_is_door(argc, argv)) {
		fflush(stdout);
		dup2(STDERR_FILENO, STDOUT_FILENO);   /* engine printf/Error() -> stderr -> BBS log */
#ifdef _WIN32
		/* MSVC's CRT rejects setvbuf(_IOLBF) with size 0 (it requires size >= 2 for
		 * line/full buffering) -> invalid-parameter fast-fail (c0000409). Use unbuffered:
		 * valid with size 0, and each write flushes immediately -- the same goal. */
		setvbuf(stdout, NULL, _IONBF, 0);
#else
		setvbuf(stdout, NULL, _IOLBF, 0);     /* line-buffered so lines flush promptly */
#endif
	}
}

/* Run syncduke_config_init() before the engine's main(). glibc passes
 * (argc, argv, envp) to .init_array constructors; MSVC has no such attribute, so
 * register a CRT initializer in the .CRT$XCU table (run during CRT startup, with
 * __argc/__argv already populated). See syncduke_door.c for the same pattern. */
#ifdef _MSC_VER
static void syncduke_config_ctor(void) { syncduke_config_init(__argc, __argv); }
#pragma section(".CRT$XCU", read)
__declspec(allocate(".CRT$XCU")) void (*syncduke_config_ctor_p)(void) = syncduke_config_ctor;
#if defined(_WIN64)
#pragma comment(linker, "/include:syncduke_config_ctor_p")
#else
#pragma comment(linker, "/include:_syncduke_config_ctor_p")
#endif
#else
__attribute__((constructor))
static void syncduke_config_ctor(int argc, char **argv, char **envp)
{
	(void)envp;
	syncduke_config_init(argc, argv);
}
#endif
