//
// Copyright(C) 2026 Rob Swindell / syncdoom
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// DESCRIPTION:
//     syncdoom multiplayer dedicated-server lifecycle: the headless server
//     loop, a detached spawn helper (for the lobby to launch a match server),
//     and a free-UDP-port allocator.
//

#ifndef MP_SERVER_H_
#define MP_SERVER_H_

// Run the headless dedicated server: bind the -port UDP socket, relay tics,
// and self-terminate after idle_timeout_secs with no connected clients
// (0 = never).  Returns a process exit code; normally does not return until
// the match empties.  Called from main() in -dedicated mode.
int mp_dedicated_main(int idle_timeout_secs);

// Find a UDP port in [lo, hi] that can currently be bound, or -1 if none.
int mp_alloc_port(int lo, int hi);

// Spawn a detached dedicated server (exe -dedicated -port <port>) that
// survives this process exiting.  Returns the server's pid, or -1 on error.
long mp_spawn_server(const char *exe_path, int port);

#endif /* #ifndef MP_SERVER_H_ */
