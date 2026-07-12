/* syncmoo1_plat.h -- the door's whole platform seam: monotonic clock, sleep,
 * and non-blocking descriptor I/O, over xpdev.
 *
 * Everything Windows-vs-POSIX about this door lives behind these six
 * functions, so no other syncmoo1_*.c / hw_sbbs.c file needs a single #ifdef
 * _WIN32 (nor <winsock2.h>, whose macro soup does not mix well with the
 * vendored 1oom headers). The implementation is thin: xpdev already solved
 * each of these portably, and this door already links xpdev for ini_file and
 * mkpath.
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
 * socket, or fd 1 (stdout) in dev/tty use -- so sm_plat_read/write use
 * read()/write(), which work on both. On Windows the DOOR32.SYS handle is a
 * Winsock SOCKET, which the CRT's read()/write() cannot touch at all, so they
 * use recv()/send(); there is consequently NO stdout fallback on Windows (a
 * dev run with no socket has no live sink, and uses SYNCMOO1_SIXELOUT capture
 * mode instead). Callers pass the descriptor as an int either way -- Windows
 * SOCKET values are 32-bit-significant by design, which is why DOOR32.SYS can
 * carry one as a decimal integer in the first place.
 */
#ifndef SYNCMOO1_PLAT_H_
#define SYNCMOO1_PLAT_H_

#include <stddef.h>
#include <stdint.h>

/* sm_plat_read()/sm_plat_write() return a byte count >= 0, or one of these.
 * The split is the point: it is what lets the callers stop caring whether the
 * failure arrived as errno or as WSAGetLastError().
 *
 * A read() of 0 means EOF (the peer closed) -- a hangup, not one of these. */
#define SM_IO_AGAIN (-1)   /* transient: nothing to do now, retry next tick   */
#define SM_IO_INTR  (-2)   /* interrupted before transferring: retry at once  */
#define SM_IO_ERROR (-3)   /* the descriptor is dead: hang up                 */

/* Monotonic clock, milliseconds. Wraps every ~49 days; every caller compares
 * differences as (int32_t)(a - b), which is wrap-safe. Monotonic, so an NTP
 * step or a DST change can't mistime the session deadline or the frame pacer. */
uint32_t sm_plat_now_ms(void);

/* Monotonic clock, microseconds -- 1oom's hw_get_time_us() contract. Only ever
 * differenced by the engine (ui/classic/uidelay.c, uiclassic.c) and used once
 * as an RNG seed (rnd.c), so a since-boot origin is fine. */
int64_t sm_plat_now_us(void);

void sm_plat_sleep_ms(int ms);

/* Bring up the socket layer (WSAStartup on Windows; nothing on POSIX) and make
 * a write to a closed socket report an error rather than kill the process
 * (SIGPIPE ignored on POSIX; Windows has no such signal). Idempotent. Call
 * once, before any other socket use -- sm_door_setup() does. */
void sm_plat_net_init(void);

/* Non-blocking + TCP_NODELAY on the door's descriptor, so a wedged client can
 * never stall the game loop and small frames aren't held by Nagle. Returns 0
 * on success, -1 otherwise. Best-effort by contract: the TCP_NODELAY half is
 * expected to fail on a non-socket (a dev/tty fd 1, a -s<fd> pipe) and is not
 * reported. fd < 0 is a no-op returning -1. */
int sm_plat_sock_setup(int fd);

/* Non-blocking transfer of up to `len` bytes. Returns the count transferred
 * (0 from sm_plat_read means EOF), or SM_IO_AGAIN / SM_IO_INTR / SM_IO_ERROR. */
int sm_plat_write(int fd, const void *buf, size_t len);
int sm_plat_read(int fd, void *buf, size_t len);

/* The descriptor to use when nothing resolved a door socket (no DOOR32.SYS, no
 * -s<fd>, no $SYNCMOO1_SOCK): fd 1 (stdout) on POSIX, so a dev run still draws
 * to the tty; -1 on Windows, where the CRT's fd 1 is not a Winsock SOCKET and
 * so cannot be written by sm_plat_write() at all. A -1 here means "no live
 * sink": the caller should discard output (or use SYNCMOO1_SIXELOUT capture
 * mode), and sm_input_pump() already treats a negative descriptor as "no
 * input". Keeping this decision here is what spares every other file an
 * #ifdef. */
int sm_plat_fallback_fd(void);

/* Can this platform be a STDIO door (POSIX yes, Windows no)? */
int sm_plat_stdio_ok(void);

/* Redirect the process's stderr onto `path`, so the door's diagnostics survive
 * even when there is no console to print them to. Returns 0 on success, -1 on
 * failure (in which case stderr is left as it was).
 *
 * WHY it's a platform call: on POSIX this is a deliberate NO-OP returning 0 --
 * a Synchronet native door inherits the server's stderr, which already goes to
 * a durable place (the terminal-server log / journal / console). Only Windows
 * needs it: Synchronet spawns a native door with CREATE_NO_WINDOW when the
 * program has XTRN_NODISPLAY set (to suppress the per-session console window),
 * and a console-less process has a dead stderr -- so a hangup reason or a
 * -home failure would be recorded nowhere. This freopen()s it to a file
 * instead. See sm_door_setup()'s caller for the path policy.
 *
 * The reopened stderr is set UNBUFFERED (_IONBF): sm_door_hangup() exits via
 * _exit(), which skips CRT buffer flushing, so a fully-buffered file would
 * drop the very hangup message this exists to capture. (_IONBF, not _IOLBF:
 * MSVC's CRT __fastfail()s on _IOLBF/_IOFBF with size 0 -- the exact bug that
 * cost syncalert a release.) */
int sm_plat_redirect_stderr(const char *path);

/* 1 iff this platform benefits from sm_plat_redirect_stderr() -- i.e. a
 * console-less launch would otherwise strand stderr (Windows). 0 on POSIX,
 * where an inherited stderr is already durable. The caller uses this to skip
 * even resolving/creating the capture path when it would go unused, so a POSIX
 * launch never makes an empty log directory. */
int sm_plat_captures_stderr(void);

#endif /* SYNCMOO1_PLAT_H_ */
