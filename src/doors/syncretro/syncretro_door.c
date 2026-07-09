/* syncretro_door.c -- DOOR32.SYS / -s<fd> / -name / -home / -t / -core setup.
 *
 * Resolves the client socket + session limits + paths from the drop file and
 * the door's own flags, BEFORE the core is loaded. Same contract as the other
 * doors (../syncmoo1/syncmoo1_door.c, ../syncduke/syncduke_door.c).
 *
 * DOOR32.SYS (the portable door interface, Synchronet's %f): line 1 = comm type
 * (2 = telnet/socket), line 2 = comm/socket handle, line 7 = user alias, line 9
 * = minutes left. The handle is an inherited descriptor: a plain fd on *nix
 * (Windows/Winsock is out of M1 scope, matching syncretro_io.c).
 *
 * One thing here has no counterpart in the sibling doors: SyncRetro takes a
 * -core <path> (the libretro .so to dlopen) and a ROM path. Both are its own
 * arguments, resolved here and stripped by sr_door_sanitize_argv() -- the ROM is
 * also accepted positionally, as the one non-flag argument that isn't the
 * DOOR32.SYS drop file.
 */
#include "syncretro.h"

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include <netinet/in.h>   /* IPPROTO_TCP */
#include <netinet/tcp.h>  /* TCP_NODELAY */
#include <sys/socket.h>   /* setsockopt */

#define SR_ALIAS_MAX 64
#define SR_PATH_MAX  4096

static int      g_socket = -1;         /* resolved comm descriptor, or -1 (none) */
static uint32_t g_time_limit_ms;       /* 0 = no session time limit */
static uint32_t g_deadline_ms;
static int      g_deadline_armed;
static int      g_carrier_lost;
static char     g_alias[SR_ALIAS_MAX];
static char     g_home[SR_PATH_MAX];   /* "" = no -home given */
static char     g_core[SR_PATH_MAX];   /* "" = no -core given */
static char     g_rom[SR_PATH_MAX];    /* "" = no ROM given */

/* Monotonic millisecond clock -- the session deadline uses it so a wall-clock
 * step (NTP, DST) can't mistime it. Same clock domain as syncretro_io.c. */
static uint32_t sr_door_now_ms(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint32_t)((uint64_t)t.tv_sec * 1000ULL + (uint64_t)t.tv_nsec / 1000000ULL);
}

/*
 * The single, canonical hangup path (../syncmoo1/syncmoo1_door.c's
 * sm_door_hangup() sets the precedent). Reached only from the WRITE side --
 * sr_io_out_flush() on a genuine write error -- which runs inside a libretro
 * video callback, where there is no failure to return. The READ side does NOT
 * come here: it latches sr_door_carrier_lost() and lets main.c's loop exit
 * normally at the top of the next frame, through the one teardown path.
 *
 * sr_io_leave() runs first (idempotent, with a BOUNDED drain) so the BBS's
 * terminal is restored rather than left wedged. Then _exit(), not exit(), so no
 * atexit handler can block on the now-dead socket.
 *
 * The `entered` guard makes it re-entrant-safe: sr_io_leave()'s drain calls
 * sr_io_out_flush(), which on a truly dead socket calls back in here.
 */
void sr_door_hangup(const char *why)
{
	static int entered;

	if (entered)
		_exit(0);   /* re-entered via sr_io_leave()'s drain on a dead socket */
	entered = 1;

	fprintf(stderr, "syncretro: client hangup (%s) -- exiting\n", why ? why : "");
	fflush(stderr);
	sr_io_leave();
	_exit(0);
}

void sr_door_carrier_lost(void)
{
	g_carrier_lost = 1;
}

int sr_door_should_exit(void)
{
	if (g_carrier_lost)
		return 1;
	if (sr_input_quit_requested())
		return 1;
	if (g_deadline_armed && (int32_t)(sr_door_now_ms() - g_deadline_ms) >= 0) {
		fprintf(stderr, "syncretro: DOOR32 time limit exceeded -- exiting\n");
		return 1;
	}
	return 0;
}

/* basename(path) == "door32.sys", case-insensitive (Synchronet's %f). */
static int is_door32_path(const char *s)
{
	const char *b = s + strlen(s);

	while (b > s && b[-1] != '/' && b[-1] != '\\')
		b--;
	return strcasecmp(b, "door32.sys") == 0;
}

/* True iff s is non-empty and every character is an ASCII digit. */
static int all_digits(const char *s)
{
	if (*s == '\0')
		return 0;
	for (; *s != '\0'; ++s)
		if (*s < '0' || *s > '9')
			return 0;
	return 1;
}

/* The attached socket/time args are "-s<digits>" / "-t<digits>": the suffix must
 * be ALL DIGITS. SyncRetro links no engine with its own option parser (the core
 * is a .so, not a main()), so nothing collides today -- but the digit-suffix
 * rule is what lets sr_door_resolve() (parse) and sr_door_sanitize_argv()
 * (strip) agree on what a door arg is, and it keeps the contract identical to
 * the sibling doors. */
static int is_socket_arg(const char *a)
{
	return a[0] == '-' && a[1] == 's' && all_digits(a + 2);
}

static int is_time_arg(const char *a)
{
	return a[0] == '-' && a[1] == 't' && all_digits(a + 2);
}

/* A flag taking a separate value argument (the pair is stripped together). */
static int is_valued_flag(const char *a)
{
	return strcmp(a, "-name") == 0 || strcmp(a, "-home") == 0
	       || strcmp(a, "-core") == 0 || strcmp(a, "-rom") == 0;
}

/* Bounded copy into a fixed buffer: NUL-terminates, truncates silently. Spelled
 * out with memcpy rather than snprintf("%s") so the deliberate truncation isn't
 * a -Wformat-truncation warning (which -Werror turns into a build failure on
 * some targets), and not strncpy, which may leave dst unterminated. */
static void copy_arg(char *dst, size_t sz, const char *src)
{
	size_t n = strlen(src);

	if (sz == 0)
		return;
	if (n >= sz)
		n = sz - 1;
	memcpy(dst, src, n);
	dst[n] = '\0';
}

/* DOOR32.SYS line numbers below are 0-based (fgets counter); the spec is
 * 1-based, so "line 1/2/7/9" in the file header maps to ln 0/1/6/8. */
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
			case 6:                                 /* user alias / handle       */
				line[strcspn(line, "\r\n")] = '\0';
				copy_arg(g_alias, sizeof(g_alias), line);
				break;
			case 8: tmin = atoi(line); break;       /* time left, minutes        */
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

/* Parse argv for the door's own arguments, falling back to the SYNCRETRO_SOCK /
 * SYNCRETRO_CORE / SYNCRETRO_ROM env vars (dev use -- e.g. point the socket at
 * one end of a socketpair()) for anything the command line didn't resolve. */
static void sr_door_resolve(int argc, char **argv)
{
	int         i;
	const char *s;

	for (i = 1; i < argc; i++) {
		const char *a = argv[i];

		if (is_socket_arg(a)) {
			g_socket = atoi(a + 2);                              /* -s<fd>      */
		} else if (is_time_arg(a)) {
			g_time_limit_ms = (uint32_t)atoi(a + 2) * 1000u;     /* -t<seconds> */
		} else if (strcmp(a, "-name") == 0 && i + 1 < argc) {
			copy_arg(g_alias, sizeof(g_alias), argv[++i]);
		} else if (strcmp(a, "-home") == 0 && i + 1 < argc) {
			/* Captured HERE, not just stripped: sr_door_sanitize_argv() removes
			 * the flag+value from argv without saving it, and sr_config_apply()
			 * runs after the strip. Same for -core/-rom below. */
			copy_arg(g_home, sizeof(g_home), argv[++i]);
		} else if (strcmp(a, "-core") == 0 && i + 1 < argc) {
			copy_arg(g_core, sizeof(g_core), argv[++i]);
		} else if (strcmp(a, "-rom") == 0 && i + 1 < argc) {
			copy_arg(g_rom, sizeof(g_rom), argv[++i]);
		} else if (is_door32_path(a)) {
			read_door32(a);
		} else if (a[0] != '-' && g_rom[0] == '\0') {
			copy_arg(g_rom, sizeof(g_rom), a);   /* bare positional: the ROM */
		}
	}

	if (g_socket < 0 && (s = getenv("SYNCRETRO_SOCK")) != NULL && *s != '\0')
		g_socket = atoi(s);
	if (g_core[0] == '\0' && (s = getenv("SYNCRETRO_CORE")) != NULL && *s != '\0')
		copy_arg(g_core, sizeof(g_core), s);
	if (g_rom[0] == '\0' && (s = getenv("SYNCRETRO_ROM")) != NULL && *s != '\0')
		copy_arg(g_rom, sizeof(g_rom), s);
}

/* Non-blocking + TCP_NODELAY + SIGPIPE-ignore. SIGPIPE is ignored
 * unconditionally (a write to a closed socket should return EPIPE, not kill the
 * process, even in the dev/tty fd-1 fallback); the socket-only options are
 * skipped when nothing resolved. Both are best-effort: a failure (TCP_NODELAY on
 * a non-socket -s<fd>, say) is ignored rather than treated as fatal. */
static void sr_door_configure_socket(int fd)
{
	signal(SIGPIPE, SIG_IGN);
	if (fd >= 0) {
		int fl = fcntl(fd, F_GETFL, 0);
		int one = 1;

		if (fl != -1)
			fcntl(fd, F_SETFL, fl | O_NONBLOCK);
		(void)setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
	}
}

/* Paint a "loading" banner the instant the door starts, before the core's own
 * init runs -- without it the user stares at a stale screen with no sign the
 * door launched (mirrors sm_door_splash()'s rationale). Writes directly to `fd`:
 * syncretro_io.c's out-buffer isn't up yet (sr_io_init() runs strictly after
 * sr_door_setup()). `fd` is the door socket, or 1 (stdout) in dev/tty use. */
static void sr_door_splash(int fd)
{
	static const char banner[] =
		"\x1b[2J\x1b[H\x1b[?25l"                /* clear, home, hide cursor */
		"\x1b[12;33HLoading SyncRetro...";      /* ~centered on an 80-col screen */

	(void)write(fd, banner, sizeof(banner) - 1);
}

static int sr_door_is_help_arg(const char *a)
{
	return strcmp(a, "-help") == 0 || strcmp(a, "--help") == 0 || strcmp(a, "-?") == 0;
}

static void sr_door_usage(const char *argv0)
{
	const char *prog = (argv0 != NULL && argv0[0] != '\0') ? argv0 : "syncretro";

	printf(
		"syncretro -- legacy game consoles over a terminal, as a Synchronet DOOR32.SYS\n"
		"door, by hosting a libretro core and rendering it as sixel graphics.\n"
		"\n"
		"usage: %s [options] [<rom>]\n"
		"\n"
		"Content:\n"
		"  -core <path>       libretro core .so to load (or $SYNCRETRO_CORE)\n"
		"  -rom <path>        game ROM; also taken as a bare argument (or $SYNCRETRO_ROM)\n"
		"\n"
		"Session:\n"
		"  <path>/door32.sys  DOOR32.SYS drop file (Synchronet %%f): socket+time+alias\n"
		"  -s<fd>             client socket descriptor directly (no drop file)\n"
		"  -t<seconds>        session time limit; the door exits when it elapses\n"
		"  -name <handle>     player name\n"
		"  -home <dir>        per-user sandbox: save states + SRAM land here\n"
		"\n"
		"  -help, --help, -?  show this help\n"
		"\n"
		"In-game: arrows/WASD = d-pad, Space/Z = A, X = B, C = X, V = Y, Q/E = L/R,\n"
		"Enter = Start, Tab = Select, Ctrl-Q = quit.\n"
		"\n"
		"Environment (dev overrides):\n"
		"  SYNCRETRO_SOCK=<fd>       client socket, when no drop file/-s is given\n"
		"  SYNCRETRO_SYSTEM=<dir>    BIOS/system dir handed to the core\n"
		"  SYNCRETRO_SIXELOUT=<path> capture mode: write each frame to <path>, no socket\n",
		prog);
	fflush(stdout);
}

int sr_door_setup(int argc, char **argv)
{
	int i, fd;

	/* -help anywhere wins before any resolution -- a real door launch always
	 * carries a socket or a DOOR32.SYS path, so there's no conflict. */
	for (i = 1; i < argc; i++) {
		if (sr_door_is_help_arg(argv[i])) {
			sr_door_usage(argv[0]);
			return 1;
		}
	}

	sr_door_resolve(argc, argv);

	fd = g_socket;
	sr_door_configure_socket(fd);         /* no-op on the socket options if fd < 0 */
	/* Capture mode has no terminal on the other end, and on the fd-1 dev
	 * fallback the banner would land in whatever stdout is redirected to. */
	if (getenv("SYNCRETRO_SIXELOUT") == NULL)
		sr_door_splash(fd >= 0 ? fd : 1);

	if (g_time_limit_ms > 0) {
		g_deadline_ms = sr_door_now_ms() + g_time_limit_ms;
		g_deadline_armed = 1;
	}
	return 0;
}

int         sr_door_socket(void)    { return g_socket; }
const char *sr_door_name(void)      { return g_alias[0] ? g_alias : NULL; }
const char *sr_door_home(void)      { return g_home[0] ? g_home : NULL; }
const char *sr_door_core_path(void) { return g_core[0] ? g_core : NULL; }
const char *sr_door_rom_path(void)  { return g_rom[0] ? g_rom : NULL; }

/*
 * Strip the door's own arguments from argv. SyncRetro hosts the core as a .so,
 * so there is no engine main() to protect from them today -- but the strip keeps
 * the door's contract identical to every sibling, and a future core launcher
 * would see this argv. The same predicates drive sr_door_resolve() above, so
 * parse and strip cannot disagree about what counts as a door arg.
 *
 * Compacts in place, keeps argv[0], lowers *argc.
 */
void sr_door_sanitize_argv(int *argc, char **argv)
{
	int n = *argc;
	int w = 1;                 /* always keep argv[0] (the program path) */
	int i;
	int rom_dropped = 0;

	for (i = 1; i < n; i++) {
		const char *a = argv[i];

		if (is_socket_arg(a) || is_time_arg(a))
			continue;
		if (is_valued_flag(a)) {
			if (i + 1 < n)
				i++;           /* drop the flag AND its value */
			continue;
		}
		if (is_door32_path(a))
			continue;
		/* The bare positional ROM path: only the FIRST such argument was
		 * consumed by sr_door_resolve(), so only the first is dropped here. */
		if (a[0] != '-' && !rom_dropped && g_rom[0] != '\0' && strcmp(a, g_rom) == 0) {
			rom_dropped = 1;
			continue;
		}
		argv[w++] = argv[i];
	}
	argv[w] = NULL;            /* keep argv NULL-terminated */
	*argc = w;
}
