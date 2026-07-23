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

#include "door32.h"    /* termgfx: the shared DOOR32.SYS parser */
#include "idle.h"      /* termgfx: the shared idle-USER clock */

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

/* Idle-USER detection (../termgfx/idle.h). Distinct from the time limit above:
 * that one bounds how long the visit may last, this one notices that nobody is
 * driving it. The BBS's own "Maximum Inactivity" cannot -- it is reset by any
 * socket read, and DSR pacing makes the terminal answer ~10x/second on its own.
 *
 * SD_IDLE_DEFAULT is the shipped policy, not merely a fallback: unset is NOT
 * off. `timeout = 0` disables. Matches the sibling doors; keep them in step. */
#define SYNCDUKE_IDLE_DEFAULT 600      /* 10 minutes */
static termgfx_idle_t g_idle;
static int            g_idle_arg = -1; /* -i<seconds>; -1 = not given */
static int            g_idle_showing;  /* the countdown owns the overlay row */
static char           g_idle_msg[96];  /* ...and this is what it says */
static int            g_idle_clear;    /* it just went: wipe the row once */
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
	termgfx_door32_t d;
	const char      *why;

	if (termgfx_door32_read(path, &d) != 0)
		return;

	if (d.socket >= 0)
		g_socket = d.socket;
	else if (d.stdio)
		/* A STDIO door: the BBS redirected our stdin/stdout, and dup2'd our stderr
		 * onto the same stream -- so the engine chatter this door deliberately
		 * routes to stderr (syncduke_config.c) would be painted over the player's
		 * screen. Send stderr to a file. */
		termgfx_stderr_capture("syncduke", d.node);
	/* comm type 0 (local) needs no action here: with no socket, syncduke_io.c
	 * writes fd 1 and syncduke_input_fd() reads fd 0 -- which IS a stdio door.
	 * SyncDuke has had that path all along as the *nix dev/tty fallback; the drop
	 * file now says when the BBS means it (Synchronet's XTRN_STDIO, Mystic on
	 * *nix). */

	if (d.alias[0] != '\0')
		snprintf(g_alias, sizeof g_alias, "%s", d.alias);
	if (d.time_limit_ms > 0)
		g_time_limit_ms = d.time_limit_ms;

	/* A drop file we cannot use is worth saying out loud: silently falling back to
	 * stdout gives a door that starts, shows the player nothing, and ignores every
	 * key, with no clue anywhere as to why. */
	if ((why = termgfx_door32_why_unusable(&d)) != NULL)
		fprintf(stderr, "syncduke: %s is no use to this door: %s\n", path, why);
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
		else if (a[0] == '-' && a[1] == 'i' && syncduke_all_digits(a + 2))
			g_idle_arg = atoi(a + 2);                          /* -i<seconds>   */
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

/* True iff s is non-empty and every character is an ASCII digit. */
int syncduke_all_digits(const char *s)
{
	if (*s == '\0')
		return 0;
	for (; *s != '\0'; ++s)
		if (*s < '0' || *s > '9')
			return 0;
	return 1;
}

/* Arm the idle clock. Called once the arguments AND syncduke.ini are both
 * known: -i wins when given (a launcher passes -i0 to excuse an exempt user),
 * otherwise the door's own [idle] section, which on a non-Synchronet BBS is the
 * only mechanism there is. */
void syncduke_idle_arm(void)
{
	unsigned secs = (g_idle_arg >= 0) ? (unsigned)g_idle_arg
	                                  : syncduke_config_idle_timeout();

	termgfx_idle_init(&g_idle, secs, syncduke_config_idle_warn(),
	                  syncduke_clock_ms());
}

/* Arm on FIRST USE, never from syncduke_door_resolve(). The resolve is reached
 * from syncduke_door_splash() inside the pre-main constructor, and the ini is
 * read by syncduke_config_init() -- a constructor in a DIFFERENT translation
 * unit, whose relative order is undefined. Arming there therefore raced the ini
 * and lost, silently falling back to the compile-time default and ignoring
 * [idle] entirely. Both callers below run at main time, after every
 * constructor, so by then -i and the ini are both known. */
static void idle_ensure_armed(void)
{
	static int armed;

	if (!armed) {
		armed = 1;
		syncduke_idle_arm();
	}
}

/* Feed the clock from REAL input; returns 1 when that input answered an
 * on-screen countdown, in which case the caller must CONSUME it rather than
 * queue it for the engine. "Press any key" must not also press that key at
 * Duke, where every key does something. */
int syncduke_idle_wake(void)
{
	int woke;

	idle_ensure_armed();
	woke = g_idle_showing;

	termgfx_idle_activity(&g_idle, syncduke_clock_ms());
	if (woke) {
		g_idle_showing = 0;
		g_idle_clear   = 1;
	}
	return woke;
}

int         syncduke_idle_showing(void)  { return g_idle_showing; }
int         syncduke_idle_clear_due(void){ return g_idle_clear; }
void        syncduke_idle_clear_done(void){ g_idle_clear = 0; }
const char *syncduke_idle_text(void)     { return g_idle_msg; }

/* Per frame, from _nextpage() beside the session time limit. Returns 1 when the
 * threshold has passed and the caller should leave. */
int syncduke_idle_check(void)
{
	static unsigned last_shown = 0;
	unsigned        left = 0;

	idle_ensure_armed();
	switch (termgfx_idle_poll(&g_idle, syncduke_clock_ms(), &left)) {
		case TERMGFX_IDLE_WARN:
			if (left != last_shown) {
				/* "terminating", not "disconnecting": this ends the GAME and
				 * hands the player back to the BBS. Same wording as the
				 * sibling doors, deliberately. */
				snprintf(g_idle_msg, sizeof(g_idle_msg),
				         " Still there? Press any key -- terminating in %u second%s ",
				         left, (left == 1) ? "" : "s");
				last_shown = left;
			}
			g_idle_showing = 1;
			return 0;
		case TERMGFX_IDLE_EXPIRED:
			return 1;
		default:
			last_shown     = 0;
			g_idle_showing = 0;
			return 0;
	}
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
		/* -i<seconds>: ours. Matched on an ALL-DIGIT suffix rather than by the
		 * looser rule above, so a Duke word option beginning with -i could never
		 * be swallowed by it. */
		if (a[0] == '-' && a[1] == 'i' && syncduke_all_digits(a + 2))
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
