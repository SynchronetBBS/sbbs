/* syncretro_plat.c -- the door's platform seam, implemented over xpdev.
 *
 * See syncretro_plat.h for the contract. This is the only translation unit in
 * the frontend that knows Windows from POSIX for the clock/sleep/socket layer,
 * and the only one that pulls in <winsock2.h> (via xpdev's sockwrap.h) --
 * deliberately, because windows.h's macros collide badly with the termgfx and
 * libretro headers. (retro_core.c owns the one other seam, dlopen/LoadLibrary,
 * isolated there per CLAUDE.md.)
 *
 * xpdev-first, per src/doors/syncconquer/CLAUDE.md: sockwrap.h normalizes
 * WSAGetLastError() into the POSIX errno values, so sr_plat_classify() below is
 * a single un-#ifdef'd chain rather than one WSAE* ladder and one errno ladder.
 * genwrap.h supplies the monotonic clock and the sleep.
 */
#include "syncretro_plat.h"

#include <limits.h>
#include <stdio.h>

/* sockwrap.h FIRST: on Windows it pulls <winsock2.h>, which must precede the
 * <windows.h> that gen_defs.h (via genwrap.h) includes, or winsock2 and the
 * ancient winsock.h that windows.h drags in fight over the same symbols. */
#include "sockwrap.h"   /* xpdev: SOCKET, send/recv, ioctlsocket, SOCKET_ERRNO,
                         *        and the WSA->errno normalization (EWOULDBLOCK,
                         *        EINTR, ENOBUFS, ...) */
#include "genwrap.h"    /* xpdev: xp_timer64(), xp_timer(), SLEEP() */

#ifndef _WIN32
  #include <fcntl.h>    /* fcntl, O_NONBLOCK */
  #include <signal.h>   /* signal, SIGPIPE */
  #include <unistd.h>   /* read, write */
#endif

/* --- clock / sleep --------------------------------------------------------- */

uint32_t sr_plat_now_ms(void)
{
	return (uint32_t)xp_timer64();
}

int64_t sr_plat_now_us(void)
{
	return (int64_t)(xp_timer() * 1000000.0L);
}

void sr_plat_sleep_ms(int ms)
{
	SLEEP(ms);
}

/* --- socket layer bring-up -------------------------------------------------- */

/*
 * xpdev's sockwrap does NOT self-initialize Winsock, and a door that skips
 * WSAStartup() dies on its first send()/recv() with WSANOTINITIALISED (10093)
 * -- which the input pump reads as a client hangup, silently dropping the user
 * back to the BBS. That exact bug cost syncalert a release (see the commit
 * "syncalert: fix four Windows-only defects that broke the door end to end"),
 * so this is called unconditionally from sr_door_setup() on EVERY launch, not
 * lazily from some failure path. Idempotent, and also called defensively from
 * sr_io_init() in case a caller reaches the I/O layer first.
 */
void sr_plat_net_init(void)
{
	static int done;

	if (done)
		return;
	done = 1;

#ifdef _WIN32
	{
		WSADATA wsa;
		(void)WSAStartup(MAKEWORD(2, 2), &wsa);
	}
#else
	/* A write to a closed socket should return EPIPE, not kill the process.
	 * Windows has no SIGPIPE -- a failed send() just reports the error. */
	signal(SIGPIPE, SIG_IGN);
#endif
}

int sr_plat_sock_setup(int fd)
{
	int one = 1;
	int rc  = 0;

	if (fd < 0)
		return -1;

#ifdef _WIN32
	{   /* The door's descriptor is a Winsock SOCKET; FIONBIO is how it goes
	     * non-blocking (there is no fcntl). */
		u_long nb = 1;
		if (ioctlsocket((SOCKET)fd, FIONBIO, &nb) != 0)
			rc = -1;
	}
#else
	{   /* fcntl, not ioctl(FIONBIO): on POSIX this fd may be a tty (fd 1, the
	     * dev/tty fallback) as well as a socket, and O_NONBLOCK is the one
	     * spelling that works on both. */
		int fl = fcntl(fd, F_GETFL, 0);
		if (fl == -1 || fcntl(fd, F_SETFL, fl | O_NONBLOCK) == -1)
			rc = -1;
	}
#endif

	/* Best-effort by contract (see the header): a non-socket descriptor -- the
	 * dev/tty fd 1, a -s<fd> pipe -- rejects this, and that is not an error
	 * worth failing a player's session over. */
	(void)setsockopt((SOCKET)fd, IPPROTO_TCP, TCP_NODELAY, (char *)&one, sizeof one);
	return rc;
}

/* --- descriptor I/O --------------------------------------------------------- */

/* Map the just-failed transfer onto the SR_IO_* codes. SOCKET_ERRNO is xpdev's
 * normalized error: plain errno on POSIX, WSAGetLastError() minus WSABASEERR on
 * Windows -- which is exactly what makes sockwrap's EWOULDBLOCK/EINTR/ENOBUFS
 * the right constants to test on BOTH platforms.
 *
 * ENOBUFS is transient, not fatal: a burst of large sixel frames can exhaust the
 * Winsock buffer pool, and treating that as a dead socket wrongly dropped
 * SyncDuke players back to the BBS (syncduke_io.c says the same). */
static int sr_plat_classify(void)
{
	int e = SOCKET_ERRNO;

	if (e == EINTR)
		return SR_IO_INTR;
	if (e == EWOULDBLOCK || e == EAGAIN || e == ENOBUFS)
		return SR_IO_AGAIN;
	return SR_IO_ERROR;
}

/* send()/recv() rather than write()/read() on Windows: the DOOR32.SYS handle is
 * a Winsock SOCKET, which is not a CRT file descriptor -- the CRT's read/write
 * cannot touch it. On POSIX we keep write()/read(), which work on the client
 * socket AND on the fd-1 dev/tty fallback (send() on a tty is ENOTSOCK). */
int sr_plat_write(int fd, const void *buf, size_t len)
{
	int n;

	if (len > INT_MAX)
		len = INT_MAX;
#ifdef _WIN32
	n = send((SOCKET)fd, (const char *)buf, (int)len, 0);
#else
	n = (int)write(fd, buf, len);
#endif
	return (n >= 0) ? n : sr_plat_classify();
}

int sr_plat_read(int fd, void *buf, size_t len)
{
	int n;

	if (len > INT_MAX)
		len = INT_MAX;
#ifdef _WIN32
	n = recv((SOCKET)fd, (char *)buf, (int)len, 0);
#else
	n = (int)read(fd, buf, len);
#endif
	return (n >= 0) ? n : sr_plat_classify();
}

int sr_plat_fallback_fd(void)
{
#ifdef _WIN32
	return -1;   /* fd 1 is not a SOCKET here; there is no live sink */
#else
	return 1;    /* stdout: a dev run on a tty still draws */
#endif
}

int sr_plat_stdio_ok(void)
{
#ifdef _WIN32
	return 0;
#else
	return 1;
#endif
}

/* Works on BOTH platforms now. WHEN to call it is the door's policy, not the
 * platform's -- see sr_plat_stderr_needs_capture() and the stdio door. */
int sr_plat_redirect_stderr(const char *path)
{
	if (path == NULL || path[0] == '\0')
		return -1;
	if (freopen(path, "w", stderr) == NULL)
		return -1;
	/* Unbuffered so a _exit() (sr_door_hangup) can't drop the buffered tail --
	 * see the header for why _IONBF specifically and not _IOLBF. */
	setvbuf(stderr, NULL, _IONBF, 0);
	return 0;
}

int sr_plat_captures_stderr(void)
{
#ifdef _WIN32
	return 1;
#else
	return 0;
#endif
}
