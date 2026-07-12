/* syncretro_door.c -- DOOR32.SYS / -s<fd> / -name / -home / -t / -core setup.
 *
 * Resolves the client socket + session limits + paths from the drop file and
 * the door's own flags, BEFORE the core is loaded. Same contract as the other
 * doors (../syncmoo1/syncmoo1_door.c, ../syncduke/syncduke_door.c).
 *
 * DOOR32.SYS (the portable door interface, Synchronet's %f): line 1 = comm type
 * (0 = local, 1 = serial, 2 = telnet/socket), line 2 = comm/socket handle,
 * line 7 = user alias, line 9 = minutes left.
 *
 * Comm type 0 means THERE IS NO SOCKET -- the BBS redirected our stdin/stdout
 * instead -- so the drop file alone selects the stdio door; -stdio is the manual
 * equivalent, for a BBS that writes no drop file at all. The handle is an inherited descriptor: a plain fd on *nix, a
 * Winsock SOCKET on Windows -- syncretro_plat.c hides that difference, so nothing
 * here is #ifdef'd.
 *
 * One thing here has no counterpart in the sibling doors: SyncRetro takes a
 * -core <path> (the libretro core to load) and a ROM path. Both are its own
 * arguments, resolved here and stripped by sr_door_sanitize_argv() -- the ROM is
 * also accepted positionally, as the one non-flag argument that isn't the
 * DOOR32.SYS drop file.
 */
#include "syncretro.h"
#include "syncretro_plat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "genwrap.h"           /* xpdev: stricmp() (POSIX strcasecmp) */
#include "dirwrap.h"           /* xpdev: mkpath() -- the diagnostics-log dir */
#include "door32.h"          /* termgfx: the shared DOOR32.SYS parser */
#include "sbbs_node.h"         /* termgfx: node number, who's-online status */
#include "syncretro_profile.h" /* sr_profile_name(): the console fallback */
#include "syncretro_binds.h"   /* sr_bind_help_line(): drives the usage key list */

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
/* Which console's key bindings to use ("pad", "intv"). The LOBBY knows which
 * console it launched, so it says so; a bare command-line run leaves this empty
 * and the door infers the profile from the core's library_name instead. */
static char     g_profile[32];         /* "" = infer from the core */
/* What the player is playing, for the who's-online line. The LOBBY supplies
 * both: it has already parsed "Astrosmash" out of
 * "Astrosmash (1981) (Mattel).int", and it knows which console it is. A bare
 * command-line run supplies neither, and falls back to the ROM's own filename
 * and the profile's name. */
static char     g_title[64];           /* -title:   "Astrosmash" */
static char     g_console[32];         /* -console: "Intellivision" / "NES" */
/* -stdio: the BBS redirected our stdin/stdout instead of handing us a socket.
 * Mystic on *nix forks the door and pipes it -- and does the telnet (and SSH)
 * itself, so what arrives is already a clean 8-bit stream, exactly as
 * Synchronet's passthru socket delivers. That is the whole reason a stdio door
 * is worth having: a door reading a RAW telnet socket would have to negotiate,
 * unescape IAC IAC, and survive a NAWS resize whose payload bytes look like
 * keystrokes -- one of which (0x11) is our quit key. */
static int      g_stdio;
static char     g_rom[SR_PATH_MAX];    /* "" = no ROM given */

/* Monotonic millisecond clock -- the session deadline uses it so a wall-clock
 * step (NTP, DST) can't mistime it. Same clock domain as syncretro_io.c. */
#define sr_door_now_ms()   sr_plat_now_ms()

/*
 * The single, canonical hangup path (../syncmoo1/syncmoo1_door.c's
 * sm_door_hangup() sets the precedent). Reached only from the WRITE side --
 * sr_io_out_flush() on a genuine write error -- which runs inside a libretro
 * video callback, where there is no failure to return. The READ side does NOT
 * come here: it latches sr_door_carrier_lost() and lets main.c's loop exit
 * normally at the top of the next frame, through the one teardown path.
 *
 * sr_io_leave() runs first (idempotent, with a BOUNDED drain) so the BBS's
 * terminal is restored rather than left wedged. Then _Exit(), not exit(), so no
 * atexit handler can block on the now-dead socket. _Exit() (C99, <stdlib.h>) not
 * POSIX _exit(): both skip the atexit(sr_io_leave) handler and the stdio flush,
 * but only _Exit() is declared portably without pulling <unistd.h> back into
 * this otherwise platform-neutral file (MSVC has no <unistd.h>).
 *
 * The `entered` guard makes it re-entrant-safe: sr_io_leave()'s drain calls
 * sr_io_out_flush(), which on a truly dead socket calls back in here.
 */
void sr_door_hangup(const char *why)
{
	static int entered;

	if (entered)
		_Exit(0);   /* re-entered via sr_io_leave()'s drain on a dead socket */
	entered = 1;

	fprintf(stderr, "syncretro: client hangup (%s) -- exiting\n", why ? why : "");
	fflush(stderr);
	sr_io_leave();
	_Exit(0);
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

/* Recognizing the drop file among our arguments is termgfx's rule too. */
#define is_door32_path(s)   termgfx_door32_is_path(s)

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
	       || strcmp(a, "-core") == 0 || strcmp(a, "-rom") == 0
	       || strcmp(a, "-profile") == 0 || strcmp(a, "-title") == 0
	       || strcmp(a, "-console") == 0;
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

/* The drop file is parsed by termgfx (door32.c), shared with the sibling doors:
 * five copies of these twenty lines had already drifted apart -- four different
 * ways to copy the alias, one door that never read it, and only one that knew
 * that a comm type of 0 means "no socket; I redirected your stdio". */
static void read_door32(const char *path)
{
	termgfx_door32_t d;

	const char *why;

	if (termgfx_door32_read(path, &d) != 0) {
		fprintf(stderr, "syncretro: cannot read %s\n", path);
		return;
	}

	if (d.socket >= 0)
		g_socket = d.socket;
	else if (d.stdio && sr_plat_stdio_ok())
		g_stdio = 1;            /* comm type 0: the BBS redirected our stdin/stdout */

	/* A drop file we cannot use is worth SAYING so. Falling back to the dev sink
	 * silently -- drawing to fd 1 and reading a descriptor nobody is typing into
	 * -- gives the sysop a door that starts, paints nothing he can see, and
	 * ignores every key, with no clue anywhere as to why. */
	if ((why = termgfx_door32_why_unusable(&d)) != NULL)
		fprintf(stderr, "syncretro: %s is no use to this door: %s\n", path, why);
	else if (d.stdio && !sr_plat_stdio_ok())
		fprintf(stderr, "syncretro: %s says stdio (comm type 0), which is POSIX-only.\n",
		        path);
	if (d.alias[0] != '\0')
		copy_arg(g_alias, sizeof(g_alias), d.alias);
	if (d.time_limit_ms > 0)
		g_time_limit_ms = d.time_limit_ms;
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
		} else if (strcmp(a, "-profile") == 0 && i + 1 < argc) {
			copy_arg(g_profile, sizeof(g_profile), argv[++i]);
		} else if (strcmp(a, "-title") == 0 && i + 1 < argc) {
			copy_arg(g_title, sizeof(g_title), argv[++i]);
		} else if (strcmp(a, "-console") == 0 && i + 1 < argc) {
			copy_arg(g_console, sizeof(g_console), argv[++i]);
		} else if (strcmp(a, "-stdio") == 0) {
			g_stdio = 1;
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

/* Bring up the socket layer (WSAStartup / SIGPIPE-ignore) and, when a descriptor
 * resolved, make it non-blocking + TCP_NODELAY. Both are best-effort via the
 * plat seam: a failure (TCP_NODELAY on a non-socket -s<fd>, say) is ignored
 * rather than treated as fatal. sr_plat_net_init() runs even with no descriptor,
 * because the audio/probe paths may still create sockets and Winsock must be up
 * first. */
static void sr_door_configure_socket(int fd)
{
	sr_plat_net_init();
	if (fd >= 0)
		(void)sr_plat_sock_setup(fd);
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

	(void)sr_plat_write(fd, banner, sizeof(banner) - 1);
}

static int sr_door_is_help_arg(const char *a)
{
	return strcmp(a, "-help") == 0 || strcmp(a, "--help") == 0 || strcmp(a, "-?") == 0;
}

static void sr_door_usage(const char *argv0)
{
	const char *prog = (argv0 != NULL && argv0[0] != '\0') ? argv0 : "syncretro";

	const char *key, *desc;
	int         i;

	printf(
		"syncretro -- legacy game consoles over a terminal, as a Synchronet DOOR32.SYS\n"
		"door, by hosting a libretro core and rendering it as sixel graphics.\n"
		"\n"
		"usage: %s [options] [<rom>]\n"
		"\n"
		"Content:\n"
		"  -profile <name>    controller profile: pad (default) | intv\n"
		"                     omitted: inferred from the core's own name\n"
		"  -title <name>      cartridge title, for the who's-online status\n"
		"  -console <name>    console name, for the who's-online status\n"
		"  -stdio             the BBS redirected our stdin/stdout instead of\n"
		"                     giving us a socket (Mystic on *nix). POSIX only.\n"
		"  -core <path>       libretro core to load: .so (*nix) / .dll (Windows)"
		" (or $SYNCRETRO_CORE)\n"
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
		"\n",
		prog);

	/* Drive the in-game key list from syncretro_binds.c's single source of truth
	 * (the same table the in-game '?' screen renders), so the two can't drift.
	 * sr_bind_help_line() reads a static const table, safe before any core load
	 * or terminal setup. */
	printf("In-game keys:\n");
	for (i = 0; sr_bind_help_line(i, &key, &desc); i++)
		printf("  %-16s %s\n", key, desc);
	printf("  ('?' shows this same list in-game)\n");

	printf(
		"\n"
		"Environment (dev overrides):\n"
		"  SYNCRETRO_SOCK=<fd>       client socket, when no drop file/-s is given\n"
		"  SYNCRETRO_SYSTEM=<dir>    BIOS/system dir handed to the core\n"
		"  SYNCRETRO_SIXELOUT=<path> capture mode: write each frame to <path>, no socket\n");
	fflush(stdout);
}

/*
 * Capture the door's own stderr diagnostics -- the hangup reason, -home/config
 * setup failures, the time-limit exit -- to a durable file, so they survive a
 * console-less launch. This is the door-layer half of the diagnostics story;
 * the keytrace (syncretro_input.c) is the opt-in wire log, separate.
 *
 * Only meaningful on Windows, and only under a real BBS session (a door socket):
 * Synchronet spawns a native door with XTRN_NODISPLAY set using CREATE_NO_WINDOW,
 * and a console-less process's stderr is dead -- so without this, hiding the
 * console (install-xtrn.ini's XTRN_NODISPLAY) would silently lose exactly these
 * messages, the trap the sibling doors' install-xtrn.ini call out.
 * sr_plat_redirect_stderr() is a no-op on POSIX (where an inherited stderr
 * already reaches the server log) and this skips it entirely when there is no
 * socket (a dev/tty run, where the console is real and wanted).
 *
 * Path (the sibling doors' data/<door>/<door>_n<node>.log convention --
 * generated runtime data belongs in data/, node-tagged so concurrent nodes don't
 * clobber each other):
 *     $SBBSDATA/syncretro/syncretro_n<node>.log
 * $SBBSDATA and $SBBSNNUM (read by sbbs_my_node) are both set by a native door's
 * launch (xtrn.cpp), so a BBS session always resolves this. With no $SBBSDATA (an
 * odd config) it degrades to a bare relative name; truncated per session ("w"),
 * so the file is always the run you just had.
 */
static void sr_door_capture_diagnostics(void)
{
	const char *data = getenv("SBBSDATA");
	char        path[SR_PATH_MAX];
	int         node = sbbs_my_node();

	/* A STDIO door MUST capture on EVERY platform, sr_plat_captures_stderr() or
	 * not: the BBS dup2()s our stderr onto the same stream as stdout (xtrn.cpp's
	 * EX_STDIO path), so an un-redirected diagnostic line is painted straight onto
	 * the player's screen, over the game. POSIX's "an inherited stderr is already
	 * durable" is true of a SOCKET door and false of this one -- there, stderr IS
	 * the terminal. */
	if (!g_stdio) {
		if (g_socket < 0)
			return;   /* no BBS session (dev/tty run): keep stderr where it is */
		if (!sr_plat_captures_stderr())
			return;   /* POSIX socket door: the server log already has it */
	}

	if (data != NULL && data[0] != '\0') {
		size_t dl = strlen(data);
		char   dir[SR_PATH_MAX];

		snprintf(dir, sizeof dir, "%s%ssyncretro", data,
		         (dl && (data[dl - 1] == '/' || data[dl - 1] == '\\')) ? "" : "/");
		mkpath(dir);                             /* xpdev: recursive mkdir */
		if (node > 0)
			snprintf(path, sizeof path, "%s/syncretro_n%d.log", dir, node);
		else
			snprintf(path, sizeof path, "%s/syncretro.log", dir);
	} else {
		snprintf(path, sizeof path, "syncretro.log");
	}

	/* Best-effort: a failure leaves stderr as-is (dead on a console-less Windows
	 * launch, but no worse than not trying) -- never a reason to fail a player's
	 * session. */
	(void)sr_plat_redirect_stderr(path);
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

	if (g_stdio && !sr_plat_stdio_ok()) {
		fprintf(stderr, "syncretro: -stdio is POSIX-only -- on Windows a BBS hands a\n"
		        "door a socket (DOOR32.SYS / -s<handle>), and this door's I/O seam\n"
		        "cannot read a CRT pipe. Use the socket instead.\n");
		return 1;
	}
	if (g_stdio && g_socket >= 0) {
		fprintf(stderr, "syncretro: -stdio and a socket were BOTH given; using the socket.\n");
		g_stdio = 0;
	}

	fd = g_socket;
	sr_door_configure_socket(fd);         /* WSAStartup + non-blocking; no-op opts if fd < 0 */
	sr_door_capture_diagnostics();        /* Windows: freopen stderr to the node log */
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
const char *sr_door_profile(void)   { return g_profile[0] ? g_profile : NULL; }
int sr_door_stdio(void)            { return g_stdio; }

/* --- who's-online status -----------------------------------------------------
 *
 * The sibling doors publish what the player is actually doing -- SyncDOOM names
 * the WAD and the map, SyncDuke the E#L# -- so the who's-online list reads
 * "playing SyncDOOM: DOOM2.WAD (MAP01)" instead of "running external program".
 * SyncRetro had no such line at all. It should name the CARTRIDGE, which is the
 * only thing a player would want to know.
 *
 * Unlike those two, there is nothing to update: a cartridge does not change
 * mid-session the way a map does. So this is set once, after the ROM is loaded,
 * and cleared on the way out. */

static int g_node_ok;

static void sr_door_node_atexit(void)
{
	if (g_node_ok)
		sbbs_node_set_ext("");    /* clear our NODE_EXT flag on exit */
}

/* "<rom>/Astrosmash (1981) (Mattel).int" -> "Astrosmash (1981) (Mattel)". The
 * fallback for a bare command-line run: the lobby's -title is already the parsed
 * title, which is nicer, but a door run by hand should still say something. */
static void sr_door_rom_basename(char *out, size_t cap, const char *path)
{
	const char *base = path;
	const char *p, *dot;
	size_t      n;

	if (path == NULL || *path == '\0') {
		snprintf(out, cap, "%s", "a cartridge");
		return;
	}
	for (p = path; *p != '\0'; p++)
		if (*p == '/' || *p == '\\')
			base = p + 1;

	dot = strrchr(base, '.');
	n   = (dot != NULL && dot != base) ? (size_t)(dot - base) : strlen(base);
	if (n >= cap)
		n = cap - 1;
	memcpy(out, base, n);
	out[n] = '\0';
}

/* Publish "playing <title> (<console>)". Call once, after the core has loaded the
 * ROM and the profile is resolved (the profile's name is the console fallback). */
void sr_door_node_playing(const char *rom_path)
{
	char status[128];
	char fallback[64];
	const char *title   = g_title;
	const char *console = g_console[0] ? g_console : sr_profile_name();

	/* $SBBSCTRL locates node.dab; the data dir comes from $SBBSDATA, or -- when a
	 * door is run by hand without it -- is derived by popping user/####/<console>
	 * off our -home, which is exactly the shape sr_config_apply() built. Passing
	 * g_home (rather than "") is what makes that fallback work. */
	sbbs_node_init(g_home);
	g_node_ok = sbbs_node_available();
	if (!g_node_ok)
		return;                   /* not launched by a BBS: nothing to publish */
	atexit(sr_door_node_atexit);

	if (title[0] == '\0') {
		sr_door_rom_basename(fallback, sizeof fallback, rom_path);
		title = fallback;
	}
	snprintf(status, sizeof status, "playing %s (%s)", title, console);
	sbbs_node_set_ext(status);
}
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

		if (is_socket_arg(a) || is_time_arg(a) || strcmp(a, "-stdio") == 0)
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
