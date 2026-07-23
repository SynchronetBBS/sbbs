/* syncmoo1_door.c -- DOOR32.SYS dropfile, socket resolution, splash, argv
 * sanitize, and hangup for the syncmoo1 Synchronet door.
 *
 * DESIGN.md §8. Ported from src/doors/syncduke/syncduke_door.c's
 * read_door32()/-s<fd>/-t<sec>/-name/bare-*door32.sys-path resolution and
 * sanitize_cmdline, with one deliberate structural change: syncduke's engine
 * owns main() (a DOS-era port), so it has to grab argv via a pre-main
 * .init_array constructor. hw_term.c (this door's 1oom "hw" backend) OWNS
 * main() directly -- so sm_door_setup() is just an ordinary function called
 * first thing in main(), no constructor dance needed (the resolved decision
 * for open question #3 in DESIGN.md §14).
 *
 * DOOR32.SYS (the portable door interface, Synchronet's %f): line 1 = comm
 * type (2 = telnet/socket), line 2 = comm/socket handle, line 7 = user
 * alias, line 9 = minutes left. The handle is an inherited descriptor: a
 * plain fd on *nix, an inherited Winsock SOCKET on Windows. Both are carried
 * as a decimal int and both are consumed through syncmoo1_plat.h, which is
 * where the whole platform split lives.
 *
 * Call order, wired in hw_term.c's main():
 *   1. sm_door_setup(argc, argv)     -- resolve socket/time/alias, configure
 *                                        the socket, paint the splash.
 *   2. sm_door_sanitize_argv(&argc, argv) -- strip door args so 1oom's own
 *                                        options_parse() (main.c) never sees
 *                                        an absolute dropfile path or -s<fd>
 *                                        as an unrecognized "-" option (which
 *                                        would abort main_1oom() -- see
 *                                        options.c's options_parse_do()).
 *   3. sm_io_init(sm_door_socket())  -- adopt the REAL socket fd, before the
 *                                        first present (fixes the Task 5/6
 *                                        "stdout fallback" carry-over).
 *   4. main_1oom(argc, argv)         -- engine drives from here.
 */
#include "syncmoo1_door.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "door32.h"    /* termgfx: the shared DOOR32.SYS parser */
#include <unistd.h>         /* _exit() -- on MSVC this is compat/unistd.h -> <process.h> */

#include "genwrap.h"        /* xpdev: stricmp() -- strcasecmp on POSIX, _stricmp on MSVC */
#include "dirwrap.h"        /* xpdev: mkpath() -- recursive mkdir for the log dir */
#include "sbbs_node.h"      /* termgfx: sbbs_my_node() -- node-tag the log file */
#include "syncmoo1_plat.h"  /* the door's platform seam: clock, socket setup, write */

#define SM_DOOR_ALIAS_MAX 64
#define SM_DOOR_HOME_MAX  4096  /* generous fixed buffer for a path; longer
                                 * values are silently truncated (mirrors
                                 * g_alias/SM_DOOR_ALIAS_MAX above) */

#ifndef SM_DOOR_PATH_MAX
#define SM_DOOR_PATH_MAX  4096
#endif

static int      g_socket = -1;         /* resolved comm descriptor, or -1 (none) */
/* DOOR32 comm type 0 / -stdio: the BBS redirected our stdin/stdout instead of
 * handing us a socket (Synchronet's XTRN_STDIO, Mystic on *nix). POSIX only. */
static int      g_stdio;
static uint32_t g_time_limit_ms;       /* 0 = no session time limit */
static uint32_t g_deadline_ms;         /* only meaningful if g_deadline_armed */
static int      g_deadline_armed;
static char     g_alias[SM_DOOR_ALIAS_MAX];
static int      g_alias_authoritative;   /* set only by read_door32(): the BBS named the user */
static char     g_home[SM_DOOR_HOME_MAX];  /* "" = no -home given */

/* Monotonic millisecond clock (syncmoo1_plat.h, xpdev's xp_timer64), used here
 * only for the session deadline so a wall-clock step (NTP, DST) can't mistime
 * it. */
#define sm_door_now_ms() sm_plat_now_ms()

/*
 * The single, canonical hangup path for the whole door (DESIGN.md §8 lists
 * hangup as a produced interface; syncduke sets the precedent with one
 * syncduke_hangup() called from both its read and write sides). Wired to
 * BOTH failure sites: the read side (hw_term.c's hw_event_handle(), when
 * sm_input_pump() returns < 0) and the write side (syncmoo1_io.c's
 * sm_io_out_flush(), on a genuine write error) -- replacing the two earlier
 * ad-hoc bare-_exit(0) paths so there is exactly one place the door dies on
 * a broken client.
 *
 * Runs sm_io_leave() first (idempotent + BOUNDED drain, SM_LEAVE_DRAIN_MS in
 * syncmoo1_io.c) so a read-side hangup, where the write direction may still
 * be open, still restores the BBS's terminal (mouse tracking off,
 * term_leave) rather than leaving it wedged -- an improvement over the two
 * prior bare-_exit paths, which skipped teardown entirely. Then _exit(0)
 * (not exit()) so the engine's other atexit handlers, which could block on
 * the now-dead socket, are skipped -- matching syncduke_hangup()'s policy.
 *
 * The `entered` guard makes it re-entrant-safe: sm_io_leave()'s drain calls
 * sm_io_out_flush(), which on a truly dead (write-side) socket calls back
 * into sm_door_hangup() -- the guard turns that recursion into an immediate
 * _exit() instead of looping (sm_io_leave()'s own g_left guard also stops
 * the teardown from running twice).
 */
void sm_door_hangup(const char *why)
{
    static int entered;

    if (entered)
        _exit(0);   /* re-entered via sm_io_leave()'s drain on a dead socket */
    entered = 1;

    fprintf(stderr, "syncmoo1: client hangup (%s) -- exiting\n", why ? why : "");
    fflush(stderr);
    sm_io_leave();   /* bounded drain + BBS terminal-mode restore (idempotent) */
    _exit(0);        /* skip the engine's other atexit handlers (dead socket) */
}

/* basename(path) == "door32.sys", case-insensitive (Synchronet's %f). */
static int is_door32_path(const char *s)
{
    const char *b = s + strlen(s);
    while (b > s && b[-1] != '/' && b[-1] != '\\')
        b--;
    return stricmp(b, "door32.sys") == 0;
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

/* The door's attached socket/time args are "-s<digits>" / "-t<digits>" -- the
 * suffix after the flag letter must be ALL DIGITS. This is what keeps them
 * from colliding with 1oom's own word options that merely START with "-s"/
 * "-t": -sfx, -sfxinitpar, -sfxvol, -skipintro, -savequit (1oom/src/options.c
 * and game/game.c), none of which have an all-digit suffix. A bare "-s"/"-t"
 * (no digits) is NOT a door arg either. BOTH sm_door_resolve() (parse) and
 * sm_door_sanitize_argv() (strip) use these two predicates, so the two
 * functions' notion of "a door socket/time arg" can never drift apart. */
static int is_socket_arg(const char *a)
{
    return a[0] == '-' && a[1] == 's' && all_digits(a + 2);
}

static int is_time_arg(const char *a)
{
    return a[0] == '-' && a[1] == 't' && all_digits(a + 2);
}

/* DOOR32.SYS line numbers below are 0-based (fgets counter); the spec itself
 * is 1-based, so "line 1/2/7/9" in the file header above maps to ln 0/1/6/8
 * here -- matching syncduke_door.c's read_door32() exactly. */
static void read_door32(const char *path)
{
    termgfx_door32_t d;
    const char      *why;

    if (termgfx_door32_read(path, &d) != 0)
        return;

    if (d.socket >= 0)
        g_socket = d.socket;
    else if (d.stdio && sm_plat_stdio_ok()) {
        g_stdio = 1;   /* comm type 0: the BBS redirected our stdin/stdout */
        /* ...and dup2'd our stderr onto the same stream, so 1oom's config errors
         * would be painted over the player's screen. Send stderr to a file. */
        termgfx_stderr_capture("syncmoo1", d.node);
    }

    if (d.alias[0] != '\0') {
        snprintf(g_alias, sizeof(g_alias), "%s", d.alias);
        g_alias_authoritative = 1;   /* the BBS said so; -name cannot outrank it */
    }
    if (d.time_limit_ms > 0)
        g_time_limit_ms = d.time_limit_ms;

    /* Say why a drop file is no use to us: falling through to the dev sink gives
     * a door that starts, shows nothing, and ignores every key, with no clue. */
    if ((why = termgfx_door32_why_unusable(&d)) != NULL)
        fprintf(stderr, "syncmoo1: %s is no use to this door: %s\n", path, why);
}

/* Parse argv for the door's own arguments, falling back to SYNCMOO1_SOCK
 * (dev use -- e.g. point it at one end of a socketpair()) when nothing on
 * the command line resolved a socket. Idempotent-safe to call more than
 * once (later args win), but sm_door_setup() only calls it the once. */
static void sm_door_resolve(int argc, char **argv)
{
    int         i;
    const char *s;

    for (i = 1; i < argc; i++) {
        const char *a = argv[i];

        if (is_socket_arg(a)) {
            g_socket = atoi(a + 2);                             /* -s<fd>        */
        } else if (is_time_arg(a)) {
            g_time_limit_ms = (uint32_t)atoi(a + 2) * 1000u;    /* -t<seconds>   */
        } else if (strcmp(a, "-stdio") == 0) {
            if (sm_plat_stdio_ok())
                g_stdio = 1;
            else
                fprintf(stderr, "syncmoo1: -stdio is POSIX-only (a Windows BBS "
                        "hands a door a socket).\n");
        } else if (strcmp(a, "-name") == 0 && i + 1 < argc) {
            /* -name <alias>: the alias source for the DROPFILE-LESS dev path
             * (-s<fd>/SYNCMOO1_SOCK), where there is no DOOR32.SYS line 7 to
             * read. It never outranks a drop file: the BBS's own statement of
             * who the user is wins, whatever order argv happens to be in.
             * (Guarding on the flag rather than on g_alias[0] so a drop file
             * carrying an EMPTY alias still shuts -name out -- an empty line 7
             * is the BBS saying "no alias", not "help yourself".) */
            ++i;
            if (!g_alias_authoritative) {
                strncpy(g_alias, argv[i], sizeof(g_alias) - 1);
                g_alias[sizeof(g_alias) - 1] = '\0';   /* strncpy may not NUL-terminate */
            }
        } else if (strcmp(a, "-home") == 0 && i + 1 < argc) {
            /* -home <dir>: captured HERE (not just stripped) because
             * sm_door_sanitize_argv() below removes it from argv without
             * saving its value -- by the time syncmoo1_config.c's
             * sm_config_apply() runs (after sanitize, DESIGN.md §8) the value
             * would otherwise be gone. NUL-safe strncpy into a fixed buffer,
             * same pattern as g_alias just above. */
            strncpy(g_home, argv[++i], sizeof(g_home) - 1);
            g_home[sizeof(g_home) - 1] = '\0';
        } else if (is_door32_path(a)) {
            read_door32(a);
        }
    }
    if (g_socket < 0 && (s = getenv("SYNCMOO1_SOCK")) != NULL && *s != '\0')
        g_socket = atoi(s);   /* dev fallback: no DOOR32.SYS/-s<fd> on the cmdline */
}

/* Non-blocking + TCP_NODELAY (DESIGN.md §8), via syncmoo1_plat.h. Best-effort:
 * a failure (e.g. TCP_NODELAY on a non-socket -s<fd>, or a unix-domain test fd)
 * is silently ignored rather than treated as fatal -- matching the general "the
 * door degrades, it doesn't abort" posture here. The SIGPIPE-ignore that used
 * to live here moved into sm_plat_net_init(), which sm_door_setup() calls
 * unconditionally: it must apply even when nothing resolved a socket. */
static void sm_door_configure_socket(int fd)
{
    if (fd >= 0)
        (void)sm_plat_sock_setup(fd);
}

/* Paint an immediate "loading" banner to the client the instant the door
 * starts, before 1oom's own (slow, LBX-scanning) init runs -- without it the
 * user stares at a blank/stale screen for a couple seconds with no sign the
 * door launched (mirrors syncduke_door_splash()'s rationale exactly). Writes
 * directly to `fd` -- syncmoo1_io.c's out-buffer/sixel layer isn't up yet
 * this early (sm_io_init() runs strictly after sm_door_setup() in main()).
 *
 * `fd` is the resolved door socket. On POSIX an unresolved one falls back to
 * fd 1 (stdout), so a dev run on a tty still sees the banner -- the same
 * fallback sm_io_init() uses. On Windows there is no such fallback: the door's
 * descriptor is a Winsock SOCKET and fd 1 is not one, so with nothing resolved
 * there is simply nothing to paint to this early (a dev run there uses
 * SYNCMOO1_SIXELOUT capture mode instead). */
static void sm_door_splash(int fd)
{
    static const char banner[] =
        "\x1b[2J\x1b[H\x1b[?25l"                          /* clear, home, hide cursor */
        "\x1b[12;28HLoading Master of Orion...";           /* ~centered on an 80-col screen */

    if (fd < 0)
        fd = sm_plat_fallback_fd();
    if (fd >= 0)
        (void)sm_plat_write(fd, banner, sizeof(banner) - 1);
}

/* True if `a` is one of the help-request forms. */
static int sm_door_is_help_arg(const char *a)
{
    return strcmp(a, "-help") == 0 || strcmp(a, "--help") == 0 || strcmp(a, "-?") == 0;
}

/* Command-line usage: the door's own options only (1oom's engine options
 * pass through untouched -- see sm_door_sanitize_argv()). Printed on
 * -help/--help/-?, which is checked FIRST, before any resolution, so it wins
 * even over a well-formed DOOR32.SYS/-s<fd> launch. */
static void sm_door_usage(const char *argv0)
{
    const char *prog = (argv0 != NULL && argv0[0] != '\0') ? argv0 : "syncmoo1";

    printf(
        "syncmoo1 -- Master of Orion 1 over a terminal, as a Synchronet DOOR32.SYS door.\n"
        "\n"
        "usage: %s [door opts] [1oom engine opts]\n"
        "\n"
        "Session:\n"
        "  <path>/door32.sys  DOOR32.SYS drop file (Synchronet %%f): socket+time+alias\n"
        "  -s<fd>             client socket descriptor directly (no drop file)\n"
        "  -t<seconds>        session time limit; the door exits when it elapses\n"
        "  -name <handle>     player name\n"
        "\n"
        "  -help, --help, -?  show this help\n",
        prog);
    fflush(stdout);   /* main() returns after this -- flush since we don't _exit() */
}

/*
 * Capture the door's own stderr diagnostics -- the hangup reason, -home setup
 * failures, the time-limit exit -- to a durable file, so they survive a
 * console-less launch. This is the door-layer half of the diagnostics story;
 * 1oom's ENGINE messages (LBX-not-found, cfg errors) are already captured
 * separately, in <userpath>/1oom_log.txt, which options_parse_early() opens
 * unconditionally.
 *
 * Only meaningful on Windows, and only under a real BBS session (a door
 * socket): Synchronet spawns a native door with XTRN_NODISPLAY set using
 * CREATE_NO_WINDOW, and a console-less process's stderr is dead -- so without
 * this, hiding the console (install-xtrn.ini's XTRN_NODISPLAY) would silently
 * lose exactly these messages, the trap src/doors/syncdoom & syncduke's
 * install-xtrn.ini call out. sm_plat_redirect_stderr() is a no-op on POSIX
 * (where an inherited stderr already reaches the server log) and when there is
 * no socket (a dev/tty run, where the console is real and wanted).
 *
 * Path (mirrors the wire dump in syncmoo1_io.c, and the sibling doors'
 * data/<door>/<door>_n<node>.log convention -- generated runtime data belongs
 * in data/, node-tagged so concurrent nodes don't clobber each other):
 *     $SBBSDATA/syncmoo1/syncmoo1_n<node>.log
 * $SBBSDATA and $SBBSNNUM (read by sbbs_my_node) are both set by a native
 * door's launch (xtrn.cpp), so a BBS session always resolves this. With no
 * $SBBSDATA (an odd config) it degrades to a bare relative name; truncated per
 * session ("w"), like the wire dump, so the file is always the run you just
 * had.
 */
static void sm_door_capture_diagnostics(void)
{
    const char *data = getenv("SBBSDATA");
    char        path[SM_DOOR_PATH_MAX];
    int         node = sbbs_my_node();

    if (g_socket < 0)
        return;   /* no BBS session (dev/tty run): keep stderr where it is */
    if (!sm_plat_captures_stderr())
        return;   /* POSIX: stderr is already durable; don't even make the dir */

    if (data != NULL && data[0] != '\0') {
        size_t dl = strlen(data);
        char   dir[SM_DOOR_PATH_MAX];

        snprintf(dir, sizeof dir, "%s%ssyncmoo1", data,
                 (dl && (data[dl - 1] == '/' || data[dl - 1] == '\\')) ? "" : "/");
        mkpath(dir);                             /* xpdev: recursive mkdir */
        if (node > 0)
            snprintf(path, sizeof path, "%s/syncmoo1_n%d.log", dir, node);
        else
            snprintf(path, sizeof path, "%s/syncmoo1.log", dir);
    } else {
        snprintf(path, sizeof path, "syncmoo1.log");
    }

    /* Best-effort: a failure leaves stderr as-is (dead on a console-less
     * Windows launch, but no worse than not trying) -- never a reason to fail
     * a player's session. */
    (void)sm_plat_redirect_stderr(path);
}

int sm_door_setup(int argc, char **argv)
{
    int i, fd;

    /* -help/--help/-? anywhere wins before any resolution -- a real door
     * launch always carries a socket or DOOR32.SYS path, so there's no
     * conflict with a legitimate session. */
    for (i = 1; i < argc; i++) {
        if (sm_door_is_help_arg(argv[i])) {
            sm_door_usage(argv[0]);
            return 1;
        }
    }

    /* Before ANY socket call (Winsock's WSAStartup; SIGPIPE off on POSIX). The
     * first thing to touch the socket is sm_door_configure_socket() below, but
     * this is deliberately unconditional and this early: a door that reaches
     * its first send()/recv() with Winsock uninitialized fails with
     * WSANOTINITIALISED, which the input pump reads as a client hangup. */
    sm_plat_net_init();

    sm_door_resolve(argc, argv);

    /* Right after resolve (g_socket is now known) and before the first
     * fprintf(stderr) the rest of setup could produce, so nothing is lost on a
     * console-less Windows launch. */
    sm_door_capture_diagnostics();

    fd = g_socket;
    sm_door_configure_socket(fd);            /* no-op on the socket options if fd < 0 */
    sm_door_splash(fd);                       /* falls back to the tty on POSIX */

    if (g_time_limit_ms > 0) {
        g_deadline_ms = sm_door_now_ms() + g_time_limit_ms;
        g_deadline_armed = 1;
    }
    return 0;
}

int sm_door_stdio(void)  { return g_stdio; }

int sm_door_socket(void)
{
    return g_socket;
}

const char *sm_door_alias(void)
{
    return g_alias;
}

/* See syncmoo1.h. The name the new-game prompt will show is edited through
 * uiobj_read_str(..., 0xb, ...) (uinewgame.c), i.e. ELEVEN characters -- less
 * than EMPEROR_NAME_LEN's 15 -- so truncate to what the player can actually
 * see and edit rather than to what the struct can hold. A BBS alias is often
 * longer; better a clean 11 characters than a name the prompt clips. */
#define SM_EMPEROR_NAME_MAX 11

char **sm_door_argv_add_emperor_name(int *argc, char **argv)
{
    static char name[SM_EMPEROR_NAME_MAX + 1];
    char      **nv;
    int         n = *argc;

    if (g_alias[0] == '\0')
        return argv;   /* no dropfile and no -name: let 1oom invent one */

    strncpy(name, g_alias, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';

    /* +3 for "-ngn", "1", name; +1 for the NULL terminator argv[] carries. */
    nv = (char **)malloc(((size_t)n + 4) * sizeof(*nv));
    if (nv == NULL)
        return argv;   /* OOM: the name is a nicety, not worth failing over */

    memcpy(nv, argv, (size_t)n * sizeof(*nv));
    nv[n++] = (char *)"-ngn";
    nv[n++] = (char *)"1";       /* player 1 is the human (see 1oom's -new HUMANS) */
    nv[n++] = name;
    nv[n]   = NULL;
    *argc   = n;
    return nv;
}

/* NULL (not "") when unset -- unlike sm_door_alias() above -- so
 * syncmoo1_config.c's sm_config_apply() can test it directly with
 * "if (sm_door_home())" rather than also checking for an empty string. */
const char *sm_door_home(void)
{
    return g_home[0] ? g_home : NULL;
}

uint32_t sm_door_time_limit_ms(void)
{
    return g_time_limit_ms;
}

void sm_door_check_time(void)
{
    if (!g_deadline_armed)
        return;
    if ((int32_t)(sm_door_now_ms() - g_deadline_ms) < 0)
        return;   /* still within the session time limit */

    fprintf(stderr, "syncmoo1: DOOR32 time limit exceeded -- exiting\n");
    fflush(stderr);
    /* A clean exit(), not sm_door_hangup(): the socket is presumably still
     * live (the user just ran out of time, they didn't disconnect), so the
     * atexit-registered sm_io_leave() should run and restore the BBS
     * terminal (mouse tracking off, term_leave) the same as any other clean
     * quit -- DESIGN.md §9's "a clean quit runs the restore with blocking
     * writes so it isn't dropped". */
    exit(0);
}

/*
 * Strip the door's own arguments from argv before 1oom's own
 * options_parse() (1oom/src/main.c via options.c) sees it. options_parse_do()
 * (options.c) does an exact strcmp() against its known option table for any
 * argv element starting with '-' or '+'; on no match it logs "unknown
 * option", calls show_usage(), and returns -1, which makes main_1oom() bail
 * out at "if (options_parse(argc, argv)) return 3;" (main.c) -- so leaving
 * -s<fd>/-t<seconds>/-name/-home/the DOOR32.SYS path in argv would abort the
 * door before the engine even starts. The -s<fd>/-t<seconds> match is
 * digit-suffix-only (is_socket_arg()/is_time_arg()) precisely so it does NOT
 * strip 1oom's real word options that merely start with "-s"/"-t" --
 * -sfx/-sfxinitpar/-sfxvol/-skipintro/-savequit (options.c, game/game.c),
 * all of which reach the engine untouched. The SAME two predicates drive
 * sm_door_resolve() above, so parse and strip can't disagree about what
 * counts as a door arg. -home is matched by exact strcmp, so it can't
 * collide either.
 *
 * -home <dir>'s VALUE is captured separately, by sm_door_resolve() above
 * (into g_home, read back via sm_door_home()) -- this function only strips
 * the flag+value pair from argv so 1oom's options_parse() never sees the
 * unrecognized "-home" flag (this is the exact syncduke lesson referenced
 * above). syncmoo1_config.c's sm_config_apply() (DESIGN.md §8) reads
 * sm_door_home() to mkpath+chdir into the per-user sandbox; it must run
 * AFTER this strip (per hw_term.c's main() call order) since the value has
 * already been saved off by sm_door_resolve() before this function ever ran.
 *
 * Compacts in place, keeps argv[0], lowers *argc, and leaves any genuine
 * 1oom option untouched. Call after sm_door_setup(), before main_1oom().
 */
void sm_door_sanitize_argv(int *argc, char **argv)
{
    int n = *argc;
    int w = 1;                 /* always keep argv[0] (the program path) */
    int i;

    for (i = 1; i < n; i++) {
        const char *a = argv[i];

        /* -s<digits> socket descriptor / -t<digits> time limit (attached door
         * args) -- digit-suffix-only so -sfx/-skipintro/-savequit et al pass
         * through to 1oom untouched (see is_socket_arg()/is_time_arg()). */
        if (is_socket_arg(a) || is_time_arg(a))
            continue;
        /* -name <alias> / -home <dir>: drop flag + value */
        if (strcmp(a, "-name") == 0 || strcmp(a, "-home") == 0) {
            if (i + 1 < n)
                i++;
            continue;
        }
        /* a DOOR32.SYS drop-file path (Synchronet's %f) */
        if (is_door32_path(a))
            continue;

        argv[w++] = argv[i];   /* keep: a genuine 1oom engine option */
    }
    argv[w] = NULL;            /* keep argv NULL-terminated */
    *argc = w;
}
