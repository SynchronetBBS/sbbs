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

#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#endif

#include "sockwrap.h"       // xpdev: SOCKET, closesocket

#include "i_timer.h"        // I_Sleep, I_GetTimeMS
#include "z_zone.h"         // Z_Init (the net layer allocates via the zone)
#include "net_defs.h"       // net_module_t (used by net_server.h)
#include "net_server.h"
#include "net_udp.h"

int mp_dedicated_main(int idle_timeout_secs)
{
	int idle_ms = idle_timeout_secs * 1000;
	int last_nonempty = I_GetTimeMS();

	// The headless path bypasses D_DoomMain, so initialize the zone allocator
	// the net layer relies on (normally done early in D_DoomMain).
	Z_Init();

	NET_SV_Init();
	NET_SV_AddModule(&net_udp_module);   // binds the -port UDP socket

	printf("syncdoom: dedicated server running"
	       " (idle timeout %ds)\n", idle_timeout_secs);
	fflush(stdout);

	for (;;) {
		NET_SV_Run();

		if (NET_SV_ConnectedClients() > 0) {
			last_nonempty = I_GetTimeMS();
		} else if (idle_ms > 0 && I_GetTimeMS() - last_nonempty > idle_ms) {
			printf("syncdoom: dedicated server idle, shutting down\n");
			break;
		}

		I_Sleep(10);
	}

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

long mp_spawn_server(const char *exe_path, int port)
{
#ifndef _WIN32
	char  portstr[16];
	pid_t pid;

	snprintf(portstr, sizeof(portstr), "%d", port);

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

		execl(exe_path, exe_path, "-dedicated", "-port", portstr,
		      (char *)NULL);
		_exit(127);     // exec failed
	}

	// Parent: pid is the detached server (now a session leader, reparented
	// to init when we exit).  Return it for registry liveness tracking.
	return (long)pid;
#else
	(void)exe_path;
	(void)port;
	return -1;          // TODO: Windows DETACHED_PROCESS spawn
#endif
}
