/* sst_plat.h -- SyncSCUMM's platform seam: the monotonic clock, the sleep, the
 * socket bring-up, and non-blocking descriptor I/O, all over xpdev.
 *
 * Everything Windows-vs-POSIX about this door lives behind these functions, so
 * no other door file (sst_io.c, syncscumm.cpp, ...) needs a single #ifdef
 * _WIN32 -- nor <winsock2.h>, whose macro soup does not mix with the termgfx
 * or ScummVM headers those files pull in. Mirrors the seam the sibling termgfx
 * doors already ship (src/doors/syncmoo1/syncmoo1_plat.h,
 * src/doors/syncretro/syncretro_plat.h); the implementation is thin because
 * xpdev already solved each of these portably and the door already links it.
 *
 *   clock   xp_timer64() / xp_timer()  (genwrap.h) -- CLOCK_MONOTONIC on POSIX,
 *                                       QueryPerformanceCounter on Windows
 *   sleep   SLEEP()                    (genwrap.h)
 *   socket  sockwrap.h                 -- normalizes WSAGetLastError() into the
 *                                       POSIX errno values, so ONE classify()
 *                                       chain covers a failed send()/recv() on
 *                                       both platforms
 *
 * The descriptor:  On POSIX the door's descriptor is a plain fd (the client
 * socket, or fd 1 in a dev/tty run) so sst_plat_read/write are read()/write(),
 * which work on both. On Windows the DOOR32.SYS handle is a Winsock SOCKET,
 * which the CRT's read()/write() cannot touch, so they are recv()/send();
 * there is consequently no stdout door on Windows (the packages launch the
 * door as XTRN_DOOR32 with a socket, so this never bites). Callers pass the
 * descriptor as an int either way -- a Windows SOCKET value is 32-bit
 * significant, which is why DOOR32.SYS can carry one as a decimal integer.
 */
#ifndef SST_PLAT_H_
#define SST_PLAT_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* sst_plat_read()/sst_plat_write() return a byte count >= 0, or one of these.
 * The split is the point: it lets the callers stop caring whether the failure
 * arrived as errno or as WSAGetLastError().  A read() of 0 means EOF (the peer
 * closed) -- a hangup, not one of these. */
#define SST_IO_AGAIN (-1)   /* transient: nothing to do now, retry next tick   */
#define SST_IO_INTR  (-2)   /* interrupted before transferring: retry at once  */
#define SST_IO_ERROR (-3)   /* the descriptor is dead: hang up                 */

/* Monotonic clock, milliseconds. Wraps every ~49 days; callers compare
 * differences as (int32_t)(a - b), which is wrap-safe. Monotonic, so an NTP
 * step or DST change can't mistime the frame pacer or a settle deadline. */
uint32_t sst_plat_now_ms(void);

/* Sleep for `ms` milliseconds (0 yields). */
void sst_plat_sleep_ms(int ms);

/* Bring up the socket layer (WSAStartup on Windows; ignore SIGPIPE on POSIX)
 * and make a write to a closed socket report an error rather than kill the
 * process. Idempotent. Call once, before any other socket use. */
void sst_plat_net_init(void);

/* Non-blocking + TCP_NODELAY on the door's descriptor, so a wedged client can
 * never stall the frame loop and small frames aren't held by Nagle. Also sets
 * a large send buffer. Returns 0 on success, -1 otherwise. Best-effort: the
 * TCP_NODELAY/SNDBUF half is expected to fail on a non-socket and is not
 * reported. fd < 0 is a no-op returning -1. */
int sst_plat_sock_setup(int fd);

/* Non-blocking transfer of up to `len` bytes. Returns the count transferred
 * (0 from sst_plat_read means EOF), or SST_IO_AGAIN / SST_IO_INTR /
 * SST_IO_ERROR. */
int sst_plat_write(int fd, const void *buf, size_t len);
int sst_plat_read(int fd, void *buf, size_t len);

/* Thin cross-platform wrappers for the handful of remaining POSIX calls the
 * door makes outside the hot path (dev/diagnostic code): isatty, F_OK-style
 * existence test, and the pid used to name a per-process trace/wirecap dump. */
int sst_plat_isatty(int fd);
int sst_plat_file_exists(const char *path);
long sst_plat_getpid(void);

/* Redirect the process's stderr onto `path` (returns 0 ok, -1 on failure).
 * On Windows a native XTRN_NODISPLAY door is spawned with CREATE_NO_WINDOW, so
 * its stderr is dead and the ScummVM engine's diagnostics would be recorded
 * nowhere -- this freopen()s them to a file (set unbuffered so an _exit() path
 * can't drop the tail). On POSIX this is a deliberate no-op returning 0: an
 * inherited stderr already goes somewhere durable (the terminal-server log). */
int sst_plat_redirect_stderr(const char *path);

/* 1 iff this platform strands stderr on a console-less launch (Windows), so
 * the caller knows whether resolving a capture path is even worth it. 0 on
 * POSIX. */
int sst_plat_captures_stderr(void);

#ifdef __cplusplus
}
#endif

#endif /* SST_PLAT_H_ */
