/*
 * syncduke_door.c -- SyncDuke BBS door interface (DOOR32.SYS / -s<fd>).
 *
 * The vendored engine owns main() and its own command-line parser, so we can't
 * add a door-arg pass there cleanly. Instead a glibc .init_array constructor
 * grabs argc/argv BEFORE main() runs and resolves the client socket; the engine
 * then ignores the door args (its parser silently skips unknown "-" flags).
 *
 * Recognized args (set on the BBS door command line, e.g. Synchronet SCFG):
 *   -s<fd>            explicit client comm socket descriptor (e.g. -s%h)
 *   <path/door32.sys> a DOOR32.SYS drop file (Synchronet's %f) -- a bare path
 *                     whose basename is "door32.sys" self-triggers the read.
 *
 * DOOR32.SYS (the portable door interface): line 1 = comm type (2=telnet),
 * line 2 = comm/socket handle, line 7 = user alias, line 9 = time left (minutes).
 * The handle is an inherited descriptor: on *nix a plain fd we read/write
 * directly, on Windows a Winsock SOCKET used via send/recv (Winsock is brought
 * up here, in the pre-main constructor, before any I/O). The non-blocking I/O
 * lives in syncduke_io.c (writes) and syncduke_input.c (reads).
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <winsock2.h>          /* the DOOR32.SYS handle is a Winsock SOCKET on Windows */
  #include <windows.h>
  #include <process.h>           /* _exit */
  #define strcasecmp _stricmp
#else
  #include <unistd.h>
  #include <signal.h>
#endif

#include "syncduke.h"

/*
 * The client socket has closed or broken (EOF on read / write error): the user
 * disconnected. Leave the door so the BBS reclaims the node instead of leaving a
 * ghost. Logged to stderr (-> BBS log). Defined here (linked by the keymap test)
 * so the engine-independent input layer can call it.
 *
 * The terminal restore runs FIRST. "The socket is already dead" holds for a
 * write-side hangup, but a read-side one (EOF on read) leaves the WRITE direction
 * open, and there we owe the BBS its terminal back -- physical key reports off,
 * translated keys restored, mouse tracking off, cursor and autowrap back. Without
 * this, a user who simply closed their client left the next thing the BBS ran in
 * SyncTERM physical-key mode. syncduke_term_restore() is idempotent, so a truly
 * dead socket just costs a failed write. Matches syncretro's sr_door_hangup() and
 * syncdoom (which reaches its restore via exit()/atexit).
 *
 * The `entered` guard makes it re-entrant-safe: the restore flushes the staged
 * output, and on a dead socket that flush calls back in here.
 */
void syncduke_hangup(const char *why)
{
	static int entered;

	if (entered)
		_exit(0);   /* re-entered via the restore's flush on a dead socket */
	entered = 1;

	syncduke_log("HANGUP: %s -- exiting", why ? why : "");
	fprintf(stderr, "syncduke: client hangup (%s) -- exiting\n", why ? why : "");
	fflush(stderr);
	syncduke_term_restore();
	_exit(0);   /* skip the engine's atexit cleanup, which could block on the dead socket */
}

static int      g_argc;
static char **  g_argv;
static int      g_resolved;
static int      g_socket = -1;
static uint32_t g_time_limit_ms;
static char     g_alias[64];

/*
 * Paint an immediate "loading" banner to the client the instant the door starts,
 * before the engine's ~2s init (GRP scan, CON compile, art load) runs. Without it
 * the user stares at a blank/stale screen for a couple seconds with no sign the
 * door launched; the first sixel frame (the 3D Realms splash) only appears once
 * init finishes. The first present()'s clear (termgfx_term_enter) wipes this. We
 * write directly to the inherited client fd -- the out-buffer/sixel layer isn't up
 * yet this early. SIGPIPE is ignored so a stillborn socket can't kill us here. */
static void syncduke_door_splash(void)
{
	static const char banner[] =
		"\x1b[2J\x1b[H\x1b[?25l"                       /* clear, home, hide cursor */
		"\x1b[12;30HLoading Duke Nukem 3D...";          /* ~centered on an 80-col screen */
	int               fd = syncduke_door_socket();      /* resolves the door args too */

#ifdef _WIN32
	/* On Windows the door handle is a Winsock SOCKET. Bring Winsock up here, in the
	 * first code that runs (before main), so every later send/recv in the door works.
	 * No inherited socket (a dev run) -> nothing useful to paint to this early. */
	if (fd >= 0) {
		WSADATA wsa;
		WSAStartup(MAKEWORD(2, 2), &wsa);
		(void)send((SOCKET)fd, banner, (int)(sizeof(banner) - 1), 0);
	}
#else
	if (fd < 0)
		fd = 1;                                         /* dev/tty fallback */
	signal(SIGPIPE, SIG_IGN);
	(void)write(fd, banner, sizeof(banner) - 1);
#endif
}

/* Command-line usage. Lists the door's own options; DOS-style /engine options
 * pass through to the Duke3D engine. Printed on -help/--help/-?//? or a bare
 * launch (no args -> nothing to do but explain how to run it). */
static void syncduke_usage(const char *argv0)
{
	const char *prog = (argv0 != NULL && argv0[0] != '\0') ? argv0 : "syncduke";

	printf(
		"SyncDuke -- Duke Nukem 3D over a terminal, as a Synchronet or DOOR32.SYS door.\n"
		"\n"
		"usage: %s [session opts] [config opts] [multiplayer opts] [engine opts]\n"
		"\n"
		"The door consumes its own options below; DOS-style /engine options pass\n"
		"through to the Duke3D engine (e.g. /v1 /l1 = volume 1, level 1).\n"
		"\n"
		"Session / terminal:\n"
		"  <path>/door32.sys  DOOR32.SYS drop file (Synchronet %%f): socket+time+alias\n"
		"  -s<fd>             client socket descriptor directly (no drop file)\n"
		"  -t<seconds>        session time limit; the door exits when it elapses\n"
		"  -name <handle>     player name (who's-online, multiplayer)\n"
		"  -home <dir>        per-user dir for duke3d.cfg + savegames\n"
		"\n"
		"Config:\n"
		"  -grpdir <dir>      where DUKE3D.GRP lives (else beside the binary, or\n"
		"                     syncduke.ini [grp] dir)\n"
		"  -charset <c>       client charset: utf8|cp437|auto (auto reads terminal.ini;\n"
		"                     set it explicitly on non-Synchronet BBSes)\n"
		"  -log <path>        door debug log (else syncduke.ini [debug] log, else off)\n"
		"  -eventlog <path>   append game events (JSONL) for the lobby activity feed\n"
		"\n"
		"Multiplayer (co-op / dukematch -- the lobby sets these):\n"
		"  -netrole <role>       master | join\n"
		"  -netport <n>          the master's listen port (role master)\n"
		"  -netpeer <host:port>  the address the joiner dials (role join)\n"
		"\n"
		"Engine (DOS-style slash options pass through):\n"
		"  /v<n>              volume / episode\n"
		"  /l<n>              level\n"
		"  /c1 | /c2 | /c3    dukematch (spawn) | co-op | dukematch (no item respawn)\n"
		"  /m                 monsters off\n"
		"  /t                 respawn items\n"
		"\n"
		"  -help, --help, -?, /?  show this help\n",
		prog);
	fflush(stdout);   /* the callers _exit() -- which does NOT flush stdio buffers */
}

/* True if `a` is one of the help-request forms. */
static int syncduke_is_help_arg(const char *a)
{
	return strcmp(a, "-help") == 0 || strcmp(a, "--help") == 0
	       || strcmp(a, "-?") == 0 || strcmp(a, "/?") == 0;
}

/* Capture argv and paint the splash BEFORE the engine's main() runs. */
static void syncduke_door_init(int argc, char **argv)
{
	int i;

	/* -help/--help/-?//? anywhere, or a bare launch, prints usage and exits
	 * (a real door launch always carries a socket or DOOR32.SYS path). Done here,
	 * in the pre-main constructor, so it wins before the engine parses argv. */
	for (i = 1; i < argc; i++) {
		if (syncduke_is_help_arg(argv[i])) {
			syncduke_usage(argv[0]);
			_exit(0);
		}
	}
	if (argc < 2) {
		syncduke_usage(argv[0]);
		_exit(1);
	}

	g_argc = argc;
	g_argv = argv;
	syncduke_door_splash();   /* instant feedback while the engine spends ~2s loading */
}

#ifdef _MSC_VER
/* MSVC has no __attribute__((constructor)). Register a CRT initializer in the
 * .CRT$XCU table (the CRT walks it during startup, before main); __argc/__argv
 * are already populated by then. The /include keeps the linker from dead-
 * stripping the otherwise-unreferenced table pointer (x86 prefixes C symbols
 * with a leading underscore; x64 does not). */
static void syncduke_door_ctor(void) { syncduke_door_init(__argc, __argv); }
#pragma section(".CRT$XCU", read)
__declspec(allocate(".CRT$XCU")) void (*syncduke_door_ctor_p)(void) = syncduke_door_ctor;
#if defined(_WIN64)
#pragma comment(linker, "/include:syncduke_door_ctor_p")
#else
#pragma comment(linker, "/include:_syncduke_door_ctor_p")
#endif
#else
/* glibc passes (argc, argv, envp) to .init_array constructors. */
__attribute__((constructor))
static void syncduke_door_grab_args(int argc, char **argv, char **envp)
{
	(void)envp;
	syncduke_door_init(argc, argv);
}
#endif

/* basename(path) == "door32.sys", case-insensitive. */
static int is_door32_path(const char *s)
{
	const char *b = s + strlen(s);
	while (b > s && b[-1] != '/' && b[-1] != '\\')
		b--;
	return strcasecmp(b, "door32.sys") == 0;
}

static void read_door32(const char *path)
{
	FILE *f = fopen(path, "r");
	char  line[256];
	int   ln = 0, commtype = -1, handle = -1, tmin = -1;

	if (f == NULL)
		return;
	while (fgets(line, sizeof(line), f) != NULL) {
		switch (ln) {
			case 0: commtype = atoi(line); break;   /* 0=local 1=serial 2=telnet */
			case 1: handle   = atoi(line); break;   /* comm/socket descriptor    */
			case 6:                                  /* user alias / handle       */
				line[strcspn(line, "\r\n")] = '\0';
				strncpy(g_alias, line, sizeof(g_alias) - 1);
				break;
			case 8: tmin = atoi(line); break;        /* time left, minutes        */
		}
		if (++ln > 8)
			break;
	}
	fclose(f);

	if (commtype == 2 && handle >= 0)
		g_socket = handle;
	if (tmin > 0)
		g_time_limit_ms = (uint32_t)tmin * 60000u;
}

static void syncduke_door_resolve(void)
{
	int i;

	g_resolved = 1;
	for (i = 1; i < g_argc; i++) {
		const char *a = g_argv[i];
		if (a[0] == '-' && a[1] == 's' && a[2] != '\0')
			g_socket = atoi(a + 2);                            /* -s<fd>        */
		else if (a[0] == '-' && a[1] == 't' && a[2] != '\0')
			g_time_limit_ms = (uint32_t)atoi(a + 2) * 1000u;   /* -t<seconds>   */
		else if (strcmp(a, "-name") == 0 && i + 1 < g_argc)
			strncpy(g_alias, g_argv[++i], sizeof(g_alias) - 1); /* -name <alias> */
		else if (is_door32_path(a))
			read_door32(a);
	}
}

int syncduke_door_socket(void)
{
	if (!g_resolved)
		syncduke_door_resolve();
	return g_socket;
}

uint32_t syncduke_door_time_limit_ms(void)
{
	if (!g_resolved)
		syncduke_door_resolve();
	return g_time_limit_ms;
}

const char *syncduke_door_alias(void)
{
	if (!g_resolved)
		syncduke_door_resolve();
	return g_alias;
}

/*
 * Strip the door's own arguments from argv before the engine's checkcommandline()
 * (Game/src/game.c) parses it.  The vendored engine is a DOS-era port whose parser
 * treats any argv element beginning with '/' as a "/option" and dumps command-line
 * help (then exits) on an unrecognized one.  On a Unix host the DOOR32.SYS drop-file
 * path (Synchronet's %f) and the -home/-grpdir values are ABSOLUTE paths beginning
 * with '/', so the engine misreads them: e.g. "/home/.../door32.sys" parses as option
 * '/h' -> unknown -> help+exit, and the door never starts.  (It only "worked" when the
 * install path's first letter happened to be a handled option letter, e.g. /sbbs -> 's'.)
 *
 * Every door argument is already consumed by the pre-main constructors -- the socket,
 * time limit and alias from -s<fd>/DOOR32.SYS (syncduke_door_resolve, above) and the
 * GRP/-home dirs in syncduke_config.c -- so removing them here is safe and hands the
 * engine a clean argv of engine-only options.  Compacts in place, keeps argv[0], lowers
 * *argc, and leaves any genuine engine option (e.g. a future -map) untouched.
 */
void syncduke_sanitize_cmdline(int *argc, char **argv)
{
	int n = *argc;
	int w = 1;                 /* always keep argv[0] (the program path) */
	int i;

	for (i = 1; i < n; i++) {
		const char *a = argv[i];

		/* -home/-grpdir/-log/-eventlog <path>, -netrole/-netport/-netpeer <v>: drop
		 * flag + value.  (-log/-eventlog values are absolute paths too -- unstripped
		 * they parse as /options; "/sbbs/..." only survived as option 's' = skill.) */
		if (strcmp(a, "-home") == 0 || strcmp(a, "-grpdir") == 0
		    || strcmp(a, "-log") == 0 || strcmp(a, "-eventlog") == 0
		    || strcmp(a, "-netrole") == 0 || strcmp(a, "-netport") == 0
		    || strcmp(a, "-netpeer") == 0) {
			if (i + 1 < n)
				i++;           /* also skip the value */
			continue;
		}
		/* -s<fd> socket descriptor / -t<seconds> time limit (attached door args) */
		if (a[0] == '-' && (a[1] == 's' || a[1] == 't') && a[2] != '\0')
			continue;
		/* -name <alias>: drop flag + value (a "/"-leading alias would misparse) */
		if (strcmp(a, "-name") == 0) {
			if (i + 1 < n)
				i++;
			continue;
		}
		/* a DOOR32.SYS drop-file path (Synchronet's %f) */
		if (is_door32_path(a))
			continue;

		argv[w++] = argv[i];   /* keep: a genuine engine option */
	}
	argv[w] = NULL;            /* keep argv NULL-terminated */
	*argc = w;
}
