/* syncretro_plat.h -- the door's whole platform seam: monotonic clock, sleep,
 * non-blocking descriptor I/O, and the console-less stderr capture, over xpdev.
 *
 * Everything Windows-vs-POSIX about the SyncRetro frontend lives behind these
 * functions, so no other syncretro_*.c / main.c file needs a single #ifdef
 * _WIN32 (nor <winsock2.h>, whose macro soup does not mix with libretro.h or the
 * termgfx headers). The one deliberate exception is retro_core.c, which owns the
 * dlopen/LoadLibrary seam for the core itself (CLAUDE.md keeps that isolated
 * there, because xpdev's xp_dlopen() mangles names and cannot load a core by an
 * explicit path). Modelled on ../syncmoo1/syncmoo1_plat.h, which set the pattern.
 *
 *   clock   xp_timer64() / xp_timer()  (genwrap.h) -- CLOCK_MONOTONIC on unix,
 *                                       QueryPerformanceCounter on Windows
 *   sleep   SLEEP()                    (genwrap.h)
 *   socket  sockwrap.h                 -- normalizes WSAGetLastError() into the
 *                                       POSIX errno values, so ONE errno test
 *                                       classifies a failed send()/recv() on
 *                                       both platforms
 *
 * The descriptor:  On POSIX the door's descriptor is a plain fd -- the client
 * socket, or fd 1 (stdout) in dev/tty use -- so sr_plat_read/write use
 * read()/write(), which work on both. On Windows the DOOR32.SYS handle is a
 * Winsock SOCKET, which the CRT's read()/write() cannot touch at all, so they
 * use recv()/send(); there is consequently NO stdout fallback on Windows (a dev
 * run with no socket has no live sink, and uses SYNCRETRO_SIXELOUT capture mode
 * instead). Callers pass the descriptor as an int either way -- Windows SOCKET
 * values are 32-bit-significant by design, which is why DOOR32.SYS can carry one
 * as a decimal integer in the first place.
 */
#ifndef SYNCRETRO_PLAT_H_
#define SYNCRETRO_PLAT_H_

#include <stddef.h>
#include <stdint.h>

/* sr_plat_read()/sr_plat_write() return a byte count >= 0, or one of these.
 * The split is the point: it is what lets the callers stop caring whether the
 * failure arrived as errno or as WSAGetLastError().
 *
 * A read() of 0 means EOF (the peer closed) -- a hangup, not one of these. */
#define SR_IO_AGAIN (-1)   /* transient: nothing to do now, retry next tick   */
#define SR_IO_INTR  (-2)   /* interrupted before transferring: retry at once  */
#define SR_IO_ERROR (-3)   /* the descriptor is dead: hang up                 */

/* Monotonic clock, milliseconds. Wraps every ~49 days; every caller compares
 * differences as (int32_t)(a - b), which is wrap-safe. Monotonic, so an NTP
 * step or a DST change can't mistime the session deadline or the frame pacer. */
uint32_t sr_plat_now_ms(void);

/* Monotonic clock, microseconds -- main.c's frame pacer accumulates absolute
 * deadlines in this domain (the drift-free design the POSIX build got from
 * clock_nanosleep(TIMER_ABSTIME), reframed as integer microseconds so it ports
 * to a platform with only a millisecond sleep). Only ever differenced. */
int64_t sr_plat_now_us(void);

void sr_plat_sleep_ms(int ms);

/* Bring up the socket layer (WSAStartup on Windows; nothing on POSIX) and make
 * a write to a closed socket report an error rather than kill the process
 * (SIGPIPE ignored on POSIX; Windows has no such signal). Idempotent. Call
 * once, before any other socket use -- sr_door_setup() and sr_io_init() do. */
void sr_plat_net_init(void);

/* Non-blocking + TCP_NODELAY on the door's descriptor, so a wedged client can
 * never stall the core loop and small frames aren't held by Nagle. Returns 0 on
 * success, -1 otherwise. Best-effort by contract: the TCP_NODELAY half is
 * expected to fail on a non-socket (a dev/tty fd 1, a -s<fd> pipe) and is not
 * reported. fd < 0 is a no-op returning -1. */
int sr_plat_sock_setup(int fd);

/* Non-blocking transfer of up to `len` bytes. Returns the count transferred
 * (0 from sr_plat_read means EOF), or SR_IO_AGAIN / SR_IO_INTR / SR_IO_ERROR. */
int sr_plat_write(int fd, const void *buf, size_t len);
int sr_plat_read(int fd, void *buf, size_t len);

/* The descriptor to use when nothing resolved a door socket (no DOOR32.SYS, no
 * -s<fd>, no $SYNCRETRO_SOCK): fd 1 (stdout) on POSIX, so a dev run still draws
 * to the tty; -1 on Windows, where the CRT's fd 1 is not a Winsock SOCKET and so
 * cannot be written by sr_plat_write() at all. A -1 here means "no live sink":
 * the caller discards output (or uses SYNCRETRO_SIXELOUT capture mode), and
 * sr_input_pump() already treats a negative descriptor as "no input". Keeping
 * this decision here is what spares every other file an #ifdef. */
int sr_plat_fallback_fd(void);

/* Redirect the process's stderr onto `path`, so the door's diagnostics survive
 * even when there is no console to print them to. Returns 0 on success, -1 on
 * failure (in which case stderr is left as it was).
 *
 * WHY it's a platform call: on POSIX this is a deliberate NO-OP returning 0 -- a
 * Synchronet native door inherits the server's stderr, which already goes to a
 * durable place (the terminal-server log / journal / console). Only Windows
 * needs it: Synchronet spawns a native door with CREATE_NO_WINDOW when the
 * program has XTRN_NODISPLAY set (to suppress the per-session console window),
 * and a console-less process has a dead stderr -- so a hangup reason or a -home
 * failure would be recorded nowhere. This freopen()s it to a file instead. See
 * sr_door_setup()'s caller for the path policy.
 *
 * The reopened stderr is set UNBUFFERED (_IONBF): sr_door_hangup() exits via
 * _exit(), which skips CRT buffer flushing, so a fully-buffered file would drop
 * the very hangup message this exists to capture. (_IONBF, not _IOLBF: MSVC's
 * CRT __fastfail()s on _IOLBF/_IOFBF with size 0 -- the exact bug that cost
 * syncalert a release.) */
int sr_plat_redirect_stderr(const char *path);

/* 1 iff this platform benefits from sr_plat_redirect_stderr() -- i.e. a
 * console-less launch would otherwise strand stderr (Windows). 0 on POSIX, where
 * an inherited stderr is already durable. The caller uses this to skip even
 * resolving/creating the capture path when it would go unused, so a POSIX launch
 * never makes an empty log directory. */
int sr_plat_captures_stderr(void);

#endif /* SYNCRETRO_PLAT_H_ */
