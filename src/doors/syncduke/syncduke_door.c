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
 * On *nix the socket is an inherited fd we read/write directly (plain
 * non-blocking I/O in syncduke_io.c / syncduke_input.c); xpdev sockwrap for Windows is a
 * later portability step.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "syncduke.h"

/*
 * The client socket has closed or broken (EOF on read / write error): the user
 * disconnected. Leave the door so the BBS reclaims the node instead of leaving a
 * ghost. Logged to stderr (-> BBS log). Defined here (linked by the keymap test)
 * so the engine-independent input layer can call it.
 */
void syncduke_hangup(const char *why)
{
	fprintf(stderr, "syncduke: client hangup (%s) -- exiting\n", why ? why : "");
	fflush(stderr);
	_exit(0);   /* immediate: skip the engine's atexit cleanup, which could block on the dead socket */
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
	int fd = syncduke_door_socket();                    /* resolves the door args too */

	if (fd < 0)
		fd = 1;                                         /* dev/tty fallback */
	signal(SIGPIPE, SIG_IGN);
	(void)write(fd, banner, sizeof(banner) - 1);
}

/* glibc passes (argc, argv, envp) to .init_array constructors. */
__attribute__((constructor))
static void syncduke_door_grab_args(int argc, char **argv, char **envp)
{
	(void)envp;
	g_argc = argc;
	g_argv = argv;
	syncduke_door_splash();   /* instant feedback while the engine spends ~2s loading */
}

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
			g_socket = atoi(a + 2);               /* -s<fd> */
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
