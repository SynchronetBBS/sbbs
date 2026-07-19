/* sst_plat.c -- SyncSCUMM's platform seam, implemented over xpdev.
 *
 * See sst_plat.h for the contract. This is the ONLY translation unit in the
 * door that knows Windows from POSIX, and the only one that pulls in
 * <winsock2.h> (via xpdev's sockwrap.h) -- deliberately, because windows.h's
 * macros collide with the termgfx and ScummVM headers every other door file
 * includes. Mirrors src/doors/syncmoo1/syncmoo1_plat.c.
 *
 * xpdev-first, per src/doors/syncconquer/CLAUDE.md: sockwrap.h normalizes
 * WSAGetLastError() into the POSIX errno values, so classify() below is a
 * single un-#ifdef'd chain rather than one WSAE* ladder and one errno ladder.
 * genwrap.h supplies the monotonic clock and the sleep; dirwrap.h the
 * existence test.
 */
#include "sst_plat.h"

#include <limits.h>
#include <stdio.h>   /* freopen, setvbuf */

/* sockwrap.h FIRST: on Windows it pulls <winsock2.h>, which must precede the
 * <windows.h> that gen_defs.h (via genwrap.h) includes, or winsock2 and the
 * ancient winsock.h that windows.h drags in fight over the same symbols. */
#include "sockwrap.h"   /* xpdev: SOCKET, send/recv, ioctlsocket, SOCKET_ERRNO,
                         *        and the WSA->errno normalization (EWOULDBLOCK,
                         *        EINTR, ENOBUFS, ...) */
#include "genwrap.h"    /* xpdev: xp_timer64(), SLEEP() */
#include "dirwrap.h"    /* xpdev: fexist() */

#ifdef _WIN32
  #include <io.h>       /* _isatty */
  #include <process.h>  /* _getpid */
#else
  #include <fcntl.h>    /* fcntl, O_NONBLOCK */
  #include <signal.h>   /* signal, SIGPIPE */
  #include <unistd.h>   /* read, write, isatty, getpid */
#endif

/* --- clock / sleep --------------------------------------------------------- */

uint32_t sst_plat_now_ms(void)
{
	return (uint32_t)xp_timer64();
}

void sst_plat_sleep_ms(int ms)
{
	SLEEP(ms);
}

/* --- socket layer bring-up ------------------------------------------------- */

/*
 * xpdev's sockwrap does NOT self-initialize Winsock, and a door that skips
 * WSAStartup() dies on its first send()/recv() with WSANOTINITIALISED (10093)
 * -- which the input pump reads as a client hangup, silently dropping the user
 * back to the BBS. That exact bug cost a sibling door a release (see
 * syncmoo1_plat.c), so this is called unconditionally on every launch.
 * Idempotent.
 */
void sst_plat_net_init(void)
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

int sst_plat_sock_setup(int fd)
{
	int one = 1;
	int sndbuf = 96 * 1024;
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
	 * dev/tty fd 1, a capture pipe -- rejects these, and that is not an error
	 * worth failing a player's session over. Matches the pre-port inline
	 * setsockopt() calls in sst_io.c (TCP_NODELAY + a large SO_SNDBUF so a
	 * burst of sixel/JXL frames isn't throttled by a small kernel buffer). */
	(void)setsockopt((SOCKET)fd, IPPROTO_TCP, TCP_NODELAY, (char *)&one, sizeof one);
	(void)setsockopt((SOCKET)fd, SOL_SOCKET, SO_SNDBUF, (char *)&sndbuf, sizeof sndbuf);
	return rc;
}

/* --- descriptor I/O -------------------------------------------------------- */

/* Map the just-failed transfer onto the SST_IO_* codes. SOCKET_ERRNO is
 * xpdev's normalized error: plain errno on POSIX, WSAGetLastError() minus
 * WSABASEERR on Windows -- which is exactly what makes sockwrap's
 * EWOULDBLOCK/EINTR/ENOBUFS the right constants to test on BOTH platforms.
 *
 * ENOBUFS is transient, not fatal: a burst of large sixel/JXL frames can
 * exhaust the Winsock buffer pool, and treating that as a dead socket would
 * wrongly drop a player back to the BBS (the sibling doors say the same). */
static int sst_plat_classify(void)
{
	int e = SOCKET_ERRNO;

	if (e == EINTR)
		return SST_IO_INTR;
	if (e == EWOULDBLOCK || e == EAGAIN || e == ENOBUFS)
		return SST_IO_AGAIN;
	return SST_IO_ERROR;
}

/* send()/recv() rather than write()/read() on Windows: the DOOR32.SYS handle
 * is a Winsock SOCKET, not a CRT file descriptor -- the CRT's read/write cannot
 * touch it. On POSIX we keep write()/read(), which work on the client socket
 * AND on the fd-1 dev/tty fallback (send() on a tty is ENOTSOCK). */
int sst_plat_write(int fd, const void *buf, size_t len)
{
	int n;

	if (len > INT_MAX)
		len = INT_MAX;
#ifdef _WIN32
	n = send((SOCKET)fd, (const char *)buf, (int)len, 0);
#else
	n = (int)write(fd, buf, len);
#endif
	return (n >= 0) ? n : sst_plat_classify();
}

int sst_plat_read(int fd, void *buf, size_t len)
{
	int n;

	if (len > INT_MAX)
		len = INT_MAX;
#ifdef _WIN32
	n = recv((SOCKET)fd, (char *)buf, (int)len, 0);
#else
	n = (int)read(fd, buf, len);
#endif
	return (n >= 0) ? n : sst_plat_classify();
}

/* --- misc dev/diagnostic wrappers ------------------------------------------ */

int sst_plat_isatty(int fd)
{
#ifdef _WIN32
	return _isatty(fd);
#else
	return isatty(fd);
#endif
}

int sst_plat_file_exists(const char *path)
{
	return fexist(path) ? 1 : 0;   /* xpdev dirwrap.h */
}

long sst_plat_getpid(void)
{
#ifdef _WIN32
	return (long)_getpid();
#else
	return (long)getpid();
#endif
}

int sst_plat_redirect_stderr(const char *path)
{
#ifdef _WIN32
	if (path == NULL || path[0] == '\0')
		return -1;
	if (freopen(path, "w", stderr) == NULL)
		return -1;
	/* Unbuffered so an _exit()/abort path can't drop the buffered tail.
	 * _IONBF, not _IOLBF: MSVC's CRT __fastfail()s on _IOLBF with size 0. */
	setvbuf(stderr, NULL, _IONBF, 0);
	return 0;
#else
	(void)path;   /* inherited stderr is already durable on POSIX */
	return 0;
#endif
}

int sst_plat_captures_stderr(void)
{
#ifdef _WIN32
	return 1;
#else
	return 0;
#endif
}
