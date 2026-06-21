//
// Copyright(C) 2026 Rob Swindell / syncdoom
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// DESCRIPTION:
//     syncdoom multiplayer dedicated-server lifecycle.  See mp_server.h.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>

#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>        // _getpid
#endif

#include "sockwrap.h"       // xpdev: SOCKET, closesocket
#include "dirwrap.h"        // xpdev: mkpath

#include "i_timer.h"        // I_Sleep, I_GetTimeMS
#include "z_zone.h"         // Z_Init (the net layer allocates via the zone)
#include "m_argv.h"         // M_CheckParmWithArgs, myargv
#include "net_defs.h"       // net_module_t (used by net_server.h)
#include "net_server.h"
#include "net_udp.h"

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

// ---------------------------------------------------------------------------
// Game registry -- the dedicated server is the single writer of its own entry
// (<gamesdir>/<hostid>-<port>.ini), so the lobby on any node can discover and
// join it. See MULTIPLAYER.md.
// ---------------------------------------------------------------------------

static char mp_reg_path[PATH_MAX];   // the entry we own (empty = none)

// Fetch a string command-line arg's value, or a default.
static const char *mp_arg(const char *flag, const char *def)
{
	int p = M_CheckParmWithArgs((char *)flag, 1);

	return (p > 0) ? myargv[p + 1] : def;
}

// Short, filesystem-safe host identifier (gethostname up to the first '.',
// non-alphanumerics -> '_').
static void mp_host_id(char *buf, size_t n)
{
	char   h[256];
	char * dot;
	size_t i;

	if (n == 0)
		return;
	if (gethostname(h, sizeof(h)) != 0)
		strncpy(h, "host", sizeof(h));
	h[sizeof(h) - 1] = '\0';
	dot = strchr(h, '.');
	if (dot != NULL)
		*dot = '\0';
	for (i = 0; h[i] != '\0' && i + 1 < n; i++)
		buf[i] = isalnum((unsigned char)h[i]) ? h[i] : '_';
	buf[i] = '\0';
	if (buf[0] == '\0')
		strncpy(buf, "host", n);
}

// (Re)write our registry entry. Single-writer: only this server touches it.
static void mp_write_registry(const char *host, const char *wadset,
                              const char *mode, const char *addr, int port,
                              const char *hostid, int maxplayers)
{
	FILE *f;

	if (mp_reg_path[0] == '\0')
		return;
	f = fopen(mp_reg_path, "w");
	if (f == NULL)
		return;

	fprintf(f, "host = %s\n", host);
	fprintf(f, "wadset = %s\n", wadset);
	fprintf(f, "mode = %s\n", mode);
	fprintf(f, "addr = %s\n", addr);
	fprintf(f, "port = %d\n", port);
	fprintf(f, "hostid = %s\n", hostid);
	fprintf(f, "players = %d\n", NET_SV_ConnectedClients());
	fprintf(f, "maxplayers = %d\n", maxplayers);
	fprintf(f, "status = %s\n", NET_SV_GameInProgress() ? "playing" : "lobby");
	fprintf(f, "pid = %ld\n", (long)getpid());
	fprintf(f, "heartbeat = %ld\n", (long)time(NULL));
	fclose(f);
}

int mp_dedicated_main(int idle_timeout_secs)
{
	int         idle_ms = idle_timeout_secs * 1000;
	int         empty_ms = 8000;          // once a player has joined, an empty match
	                                      // quits fast (cancelled/finished -- it's
	                                      // hidden from Browse the moment it empties)
	int         had_client = 0;           // a client connected at least once
	int         last_nonempty = I_GetTimeMS();
	int         last_heartbeat = 0;
	int         reg_live = 0;             // registry entry currently exists (joinable)
	int         maxplayers, port;
	const char *gamesdir, *host, *wadset, *mode, *addr;
	char        hostid[64];

	// The headless path bypasses D_DoomMain, so initialize the zone allocator
	// the net layer relies on (normally done early in D_DoomMain).
	Z_Init();

	NET_SV_Init();
	NET_SV_AddModule(&net_udp_module);   // binds the -port UDP socket

	// The lobby tells us the match size, so the wait threshold doesn't depend
	// on which client wins the race to become the controller.
	maxplayers = atoi(mp_arg("-maxplayers", "0"));
	if (maxplayers > 0)
		NET_SV_SetMaxPlayers(maxplayers);

	// Registry metadata (passed by the lobby). Without -gamesdir we don't
	// register -- e.g. a hand-launched -dedicated for testing.
	port     = atoi(mp_arg("-port", "0"));
	gamesdir = mp_arg("-gamesdir", "");
	host     = mp_arg("-host", "BBS");
	wadset   = mp_arg("-wadset", "");
	mode     = mp_arg("-gamemode", "coop");
	addr     = mp_arg("-advertise", "127.0.0.1");
	mp_host_id(hostid, sizeof(hostid));

	if (gamesdir[0] != '\0' && port > 0) {
		char dir[PATH_MAX];

		strncpy(dir, gamesdir, sizeof(dir) - 1);
		dir[sizeof(dir) - 1] = '\0';
		mkpath(dir);
		snprintf(mp_reg_path, sizeof(mp_reg_path), "%s%s-%d.ini",
		         dir, hostid, port);
		mp_write_registry(host, wadset, mode, addr, port, hostid, maxplayers);
		reg_live = 1;
	}

	printf("syncdoom: dedicated server running"
	       " (idle timeout %ds)\n", idle_timeout_secs);
	fflush(stdout);

	for (;;) {
		NET_SV_Run();

		if (NET_SV_ConnectedClients() > 0) {
			last_nonempty = I_GetTimeMS();
			had_client = 1;
		} else {
			// Before anyone has joined, allow the full idle grace (the creator's
			// connect is in flight). Once a client has come and gone, the match is
			// cancelled/over -- quit promptly so it stops lingering.
			int grace = had_client ? empty_ms : idle_ms;
			if (grace > 0 && I_GetTimeMS() - last_nonempty > grace) {
				printf("syncdoom: dedicated server %s, shutting down\n",
				       had_client ? "match empty" : "idle (nobody joined)");
				break;
			}
		}

		// Once the match starts it's no longer joinable (vanilla Doom has no
		// mid-game join), so drop the registry entry right away -- Browse should
		// only list joinable (lobby) games. While still gathering players,
		// heartbeat the entry (players/status/timestamp) every ~3s.
		if (NET_SV_GameInProgress()) {
			if (reg_live) {
				remove(mp_reg_path);
				reg_live = 0;
			}
		} else if (I_GetTimeMS() - last_heartbeat > 3000) {
			mp_write_registry(host, wadset, mode, addr, port, hostid, maxplayers);
			last_heartbeat = I_GetTimeMS();
		}

		I_Sleep(10);
	}

	if (reg_live)
		remove(mp_reg_path);
	NET_SV_Shutdown();
	return 0;
}

int mp_alloc_port(int lo, int hi)
{
	int p;

	for (p = lo; p <= hi; p++) {
		SOCKET             s;
		struct sockaddr_in addr;

		s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (s == INVALID_SOCKET)
			continue;

		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
		addr.sin_port = htons((unsigned short)p);

		if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
			closesocket(s);     // free it; the server will rebind
			return p;
		}
		closesocket(s);
	}

	return -1;
}

long mp_spawn_server(const char *exe_path, int port, char *const extra_argv[])
{
#ifndef _WIN32
	char  portstr[16];
	char *argv[64];
	int   n = 0, i;
	pid_t pid;

	// Build the child command: exe -dedicated -port <port> <extra...>. The
	// extra args (e.g. -maxplayers, and later the registry metadata) are
	// forwarded from the -spawnserver invocation.
	snprintf(portstr, sizeof(portstr), "%d", port);
	argv[n++] = (char *)exe_path;
	argv[n++] = "-dedicated";
	argv[n++] = "-port";
	argv[n++] = portstr;
	if (extra_argv != NULL) {
		for (i = 0; extra_argv[i] != NULL && n < 62; i++)
			argv[n++] = extra_argv[i];
	}
	argv[n] = NULL;

	pid = fork();
	if (pid < 0)
		return -1;

	if (pid == 0) {
		// Child: detach into its own session so it survives the launching
		// door exiting (no controlling terminal -> no SIGHUP), then exec
		// the headless server.
		int fd;

		setsid();
		signal(SIGHUP, SIG_IGN);

		fd = open("/dev/null", O_RDWR);
		if (fd >= 0) {
			dup2(fd, STDIN_FILENO);
			dup2(fd, STDOUT_FILENO);
			dup2(fd, STDERR_FILENO);
			if (fd > STDERR_FILENO)
				close(fd);
		}

		execv(exe_path, argv);
		_exit(127);     // exec failed
	}

	// Parent: pid is the detached server (now a session leader, reparented
	// to init when we exit).  Return it for registry liveness tracking.
	return (long)pid;
#else
	// Build a single quoted command line: "exe" -dedicated -port <port> <extra...>
	// CreateProcess takes one string, not an argv vector.
	char                cmd[4096];
	size_t              len = 0;
	int                 i;
	STARTUPINFO         si;
	PROCESS_INFORMATION pi;

	#define MP_APPEND(s) do { \
				int _need = snprintf(cmd + len, sizeof(cmd) - len, "%s", (s)); \
				if (_need < 0 || (size_t)_need >= sizeof(cmd) - len) return -1; \
				len += (size_t)_need; \
} while (0)

	// A token is quoted if it contains a space (or is empty); paths with spaces
	// are the common case (e.g. the exe path or -gamesdir).
	#define MP_APPEND_ARG(s) do { \
				const char *_a = (s); \
				if (_a[0] == '\0' || strchr(_a, ' ') != NULL) { \
					MP_APPEND("\""); MP_APPEND(_a); MP_APPEND("\""); \
				} else { \
					MP_APPEND(_a); \
				} \
} while (0)

	cmd[0] = '\0';
	MP_APPEND_ARG(exe_path);
	MP_APPEND(" -dedicated -port ");
	{
		char portstr[16];
		snprintf(portstr, sizeof(portstr), "%d", port);
		MP_APPEND(portstr);
	}
	if (extra_argv != NULL) {
		for (i = 0; extra_argv[i] != NULL; i++) {
			MP_APPEND(" ");
			MP_APPEND_ARG(extra_argv[i]);
		}
	}

	#undef MP_APPEND
	#undef MP_APPEND_ARG

	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	memset(&pi, 0, sizeof(pi));

	// DETACHED_PROCESS: no console, survives the launching door exiting (the
	// Windows analogue of the *nix setsid()/SIGHUP-ignore dance above).
	if (!CreateProcess(NULL, cmd, NULL, NULL, FALSE,
	                   DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP,
	                   NULL, NULL, &si, &pi))
		return -1;

	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);   // we track liveness by pid via the registry, not the handle
	return (long)pi.dwProcessId;
#endif
}
