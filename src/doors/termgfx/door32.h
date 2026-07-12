#ifndef TERMGFX_DOOR32_H_
#define TERMGFX_DOOR32_H_

#include <stdint.h>

// DOOR32.SYS -- the portable drop file every BBS writes, and the one thing that
// tells a door HOW TO REACH THE PLAYER.
//
// This lived, copy-pasted, in five doors (syncdoom.c, syncduke_door.c,
// syncmoo1_door.c, syncconquer/door_io.c, syncretro_door.c) -- and had already
// drifted: four different ways to copy the alias, one door that never read it,
// and only one that knew what a comm type of 0 means. That is what a shared
// library is for.
//
// The file (Synchronet writes it in xtrn_sec.cpp; Mystic's D3 and most modern
// BBSs write the same):
//
//   line 1   comm type: 0 = LOCAL, 1 = serial, 2 = telnet/socket   [0-based ln 0]
//   line 2   comm/socket handle (an fd on *nix, a Winsock SOCKET on Windows)
//   line 3   baud (meaningless; kept for DOS-era doors)
//   line 4   BBS software name+version
//   line 5   user number
//   line 6   user's real name
//   line 7   user's alias/handle
//   line 8   security level
//   line 9   time left, in MINUTES
//   line 10  emulation (1 = ANSI)
//   line 11  node number
//
// COMM TYPE 0 IS NOT "NOTHING". It means there is no socket because the BBS
// redirected the door's stdin/stdout instead -- Synchronet writes exactly this
// (comm type 0, handle INVALID_SOCKET) when an external program is registered
// XTRN_STDIO, and Mystic does the same on *nix. A door that treats it as "no
// connection" and falls back to writing fd 1 while ALSO reading fd 1 appears to
// work at a dev console (a tty descriptor is bidirectional) and silently ignores
// every keystroke down a pipe. So it is reported here, explicitly, as `stdio`.
//
// Whether a door CAN honor it is the door's business: on Windows a CRT pipe is
// not a Winsock SOCKET and the doors' I/O seams use send()/recv(), so the stdio
// door is POSIX-only. This module reports the file's contents; it does not decide
// policy.

typedef struct {
	int commtype;             // raw line 1: 0 = local, 1 = serial, 2 = telnet
	int socket;               // the handle, or -1 when there is no usable socket
	int stdio;                // 1 = comm type 0: the BBS redirected our stdin/stdout
	uint32_t time_limit_ms;   // 0 = no limit given
	int node;                 // 0 = not given
	char alias[64];           // "" = not given
} termgfx_door32_t;

// Is `arg` a path whose basename is "door32.sys" (case-insensitively)? This is
// how a door recognizes Synchronet's %f among its other arguments.
int termgfx_door32_is_path(const char *arg);

// Parse the drop file. Returns 0 on success, -1 if it cannot be read (in which
// case *d is still zeroed and safe to use: socket = -1, stdio = 0, alias = "").
//
// A missing or malformed line leaves that field at its default rather than
// failing the whole parse -- a door that got a usable socket should not refuse to
// run because the BBS wrote no alias.
int termgfx_door32_read(const char *path, termgfx_door32_t *d);

// Why is this drop file no use to us? NULL when it IS usable (a socket, or the
// BBS's stdio). Otherwise a short reason a door can put in front of the sysop --
// serial, an unknown comm type, a telnet line with no usable handle, or a file so
// truncated it has no comm type at all. A door that just falls back to its dev
// sink here draws into the void and looks broken; say why instead.
const char *termgfx_door32_why_unusable(const termgfx_door32_t *d);

// A STDIO door MUST NOT write to stderr.
//
// The BBS dup2()s a door's stderr onto the same stream as its stdout (Synchronet
// does it in xtrn.cpp's EX_STDIO path), so on a stdio door stderr IS the player's
// terminal: every diagnostic line, and every line the hosted GAME ENGINE prints,
// is painted over the picture. That is not hypothetical -- it is what these doors
// do. SyncDOOM stamps timestamped lines to stderr; SyncDuke deliberately routes
// Chocolate Duke's printf/Error() there (correct when stderr is the BBS log, and
// exactly wrong when it is the screen); 1oom prints its config errors there.
//
// So a stdio door redirects stderr to a file, on every platform:
//     $SBBSDATA/<door>/<door>_n<node>.log     (node-tagged; truncated per session)
// falling back to "<door>.log" in the cwd when the BBS exported no data dir.
//
// Call once, at startup, as soon as the door knows it is a stdio door. Returns 0
// on success, -1 if the redirect failed (in which case stderr is left alone --
// better a messy screen than a door that cannot report why it died).
int termgfx_stderr_capture(const char *doorname, int node);

#endif // TERMGFX_DOOR32_H_
