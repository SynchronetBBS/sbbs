/* File-related system-call wrappers */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

/* OS-specific */
#if defined(__unix__)

#include <stdarg.h>     /* va_list */
#include <string.h>     /* strlen() */
#include <unistd.h>     /* getpid() */
#include <fcntl.h>      /* fcntl() file/record locking */
#include <sys/file.h>   /* L_SET for Solaris */
#include <errno.h>
#include <sys/param.h>  /* BSD */

#endif

/* ANSI */
#include <sys/types.h>  /* _dev_t */
#include <sys/stat.h>   /* struct stat */
#include <limits.h> /* struct stat */
#include <stdlib.h>     /* realloc() */

#include "filewrap.h"   /* Verify prototypes */

/****************************************************************************/
/* Returns the modification time of the file in 'fd'						*/
/* or -1 if file doesn't exist.												*/
/****************************************************************************/
time_t filetime(int fd)
{
	struct stat st;

	if (fstat(fd, &st) != 0)
		return -1;

	return st.st_mtime;
}

#if defined(__unix__) && !defined(__BORLANDC__)

/****************************************************************************/
/* Returns the length of the file in 'fd'									*/
/* or -1 if file doesn't exist.												*/
/****************************************************************************/
off_t filelength(int fd)
{
	struct stat st;

	if (fstat(fd, &st) != 0)
		return -1L;

	return st.st_size;
}

/*************************************/
/* Use OFD fcntl() locks when we can */
/*************************************/
#if defined __linux__
	#define USE_FCNTL_LOCKS
// See https://patchwork.kernel.org/patch/9289177/
	#if defined F_OFD_SETLK && _FILE_OFFSET_BITS != 64
		#undef F_OFD_SETLK
	#endif

	#if defined F_OFD_SETLK
		#undef F_SETLK
		#define F_SETLK F_OFD_SETLK
		#undef F_SETLKW
		#define F_SETLKW F_OFD_SETLKW
	#else
		#warning Linux OFD locks not enabled!
	#endif
#endif

/* Sets a lock on a portion of a file */
int xp_lockfile(int fd, off_t pos, off_t len, bool block)
{
#if defined USE_FCNTL_LOCKS
	struct flock alock = {0};

	// fcntl() will return EBADF if we try to set a write lock a file opened O_RDONLY
	int          flags;
	if ((flags = fcntl(fd, F_GETFL)) == -1)
		return -1;
	if ((flags & (O_RDONLY | O_RDWR | O_WRONLY)) == O_RDONLY)
		alock.l_type = F_RDLCK; /* set read lock to prevent writes */
	else
		alock.l_type = F_WRLCK; /* set write lock to prevent all access */
	alock.l_whence = L_SET;     /* SEEK_SET */
	alock.l_start = pos;
	alock.l_len = (int)len;

	int result = fcntl(fd, block ? F_SETLKW : F_SETLK, &alock);
	if (result == -1 && errno != EINVAL)
		return -1;
#elif !defined(__QNX__) && !defined(__solaris__)
	int op = LOCK_EX;
	if (!block)
		op |= LOCK_NB;
	/* use flock (doesn't work over NFS) */
	if (flock(fd, op) != 0  && errno != EOPNOTSUPP)
		return -1;
#endif
	return 0;
}

/* Removes a lock from a file record */
int unlock(int fd, off_t pos, off_t len)
{

#if defined USE_FCNTL_LOCKS
	struct flock alock = {0};

	alock.l_type = F_UNLCK;   /* remove the lock */
	alock.l_whence = L_SET;
	alock.l_start = pos;
	alock.l_len = (int)len;
	int result = fcntl(fd, F_SETLK, &alock);
	if (result == -1 && errno != EINVAL)
		return -1;
#elif !defined(__QNX__) && !defined(__solaris__)
	/* use flock (doesn't work over NFS) */
	if (flock(fd, LOCK_UN | LOCK_NB) != 0 && errno != EOPNOTSUPP)
		return -1;
#endif

	return 0;
}

/* Opens a file in specified sharing (file-locking) mode */
/*
 * This is how it *SHOULD* work:
 * Values of DOS 2-6.22 file sharing behavior:
 *          | Second and subsequent Opens
 * First    |Compat Deny   Deny   Deny   Deny
 * Open     |       All    Write  Read   None
 *          |R W RW R W RW R W RW R W RW R W RW
 * - - - - -| - - - - - - - - - - - - - - - - -
 * Compat R |Y Y Y  N N N  1 N N  N N N  1 N N
 *        W |Y Y Y  N N N  N N N  N N N  N N N
 *        RW|Y Y Y  N N N  N N N  N N N  N N N
 * - - - - -|
 * Deny   R |C C C  N N N  N N N  N N N  N N N
 * All    W |C C C  N N N  N N N  N N N  N N N
 *        RW|C C C  N N N  N N N  N N N  N N N
 * - - - - -|
 * Deny   R |2 C C  N N N  Y N N  N N N  Y N N
 * Write  W |C C C  N N N  N N N  Y N N  Y N N
 *        RW|C C C  N N N  N N N  N N N  Y N N
 * - - - - -|
 * Deny   R |C C C  N N N  N Y N  N N N  N Y N
 * Read   W |C C C  N N N  N N N  N Y N  N Y N
 *        RW|C C C  N N N  N N N  N N N  N Y N
 * - - - - -|
 * Deny   R |2 C C  N N N  Y Y Y  N N N  Y Y Y
 * None   W |C C C  N N N  N N N  Y Y Y  Y Y Y
 *        RW|C C C  N N N  N N N  N N N  Y Y Y
 *
 * Legend:
 * Y = open succeeds,
 * N = open fails with error code 05h.
 * C = open fails, INT 24 generated.
 * 1 = open succeeds if file read-only, else fails with error code.
 * 2 = open succeeds if file read-only, else fails with INT 24
 */
#if !defined(__QNX__)
int sopen(const char *fn, int sh_access, int share, ...)
{
	int     fd;
#ifdef S_IREAD
	int     pmode = S_IREAD;
#else
	int     pmode = 0;
#endif
	va_list ap;

	if (sh_access & O_CREAT) {
		va_start(ap, share);
		pmode = va_arg(ap, unsigned int);
		va_end(ap);
	}

	if ((fd = open(fn, sh_access, pmode)) < 0)
		return -1;

	if (share == SH_DENYNO || share == SH_COMPAT) /* no lock needed */
		return fd;

#if defined USE_FCNTL_LOCKS

	struct flock alock = {0}; // lock entire file from offset 0

	if (share == SH_DENYWR)
		alock.l_type = F_RDLCK; /* set read lock to prevent writes */
	else
		alock.l_type = F_WRLCK; /* set write lock to prevent all access */

	if (fcntl(fd, F_SETLK, &alock) != 0) {
		close(fd);
		return -1;
	}

#elif !defined(__solaris__)

	int flock_op = LOCK_NB;   /* non-blocking */

	/* use flock (doesn't work over NFS) */
	if (share == SH_DENYRW)
		flock_op |= LOCK_EX;
	else   /* SH_DENYWR */
		flock_op |= LOCK_SH;
	if (flock(fd, flock_op) != 0 && errno != EOPNOTSUPP) { /* That object doesn't do locks */
		if (errno == EWOULDBLOCK)
			errno = EAGAIN;
		close(fd);
		return -1;
	}
#endif
	return fd;
}
#endif /* !QNX */

#elif defined(_MSC_VER) || defined(__MINGW32__) || defined(__DMC__)

#include <io.h>             /* tell */
#include <stdio.h>          /* SEEK_SET */
#include <sys/locking.h>    /* _locking */

/* Fix MinGW locking.h typo */
#if defined LK_UNLOCK && !defined LK_UNLCK
	#define LK_UNLCK LK_UNLOCK
#endif

int unlock(int file, off_t offset, off_t size)
{
	int   i;
	off_t pos;

	pos = tell(file);
	if (offset != pos)
		(void)lseek(file, offset, SEEK_SET);
	i = _locking(file, LK_UNLCK, (long)size);
	if (offset != pos)
		(void)lseek(file, pos, SEEK_SET);
	return i;
}

#endif  /* !__unix__ && (_MSC_VER || __MINGW32__ || __DMC__) */

int lock(int fd, off_t pos, off_t len)
{
	return xp_lockfile(fd, pos, len, /* block */ false);
}

#if defined(_WIN32)
static size_t
p2roundup(size_t n)
{
	if (n & (n - 1)) { // If n isn't a power of two already...
		n--;
		n |= n >> 1;
		n |= n >> 2;
		n |= n >> 4;
		n |= n >> 8;
		n |= n >> 16;
#if SIZE_T_MAX > 0xffffffffU
		n |= n >> 32;
#endif
		n++;
	}
	return n;
}

static int expandtofit(char **linep, size_t len, size_t *linecapp)
{
	char * newline;
	size_t newcap;

	if (len + 1 >= LONG_MAX)
		return -1;
	if (len > *linecapp) {
		if (len == LONG_MAX)
			newcap = LONG_MAX;
		else
			newcap = p2roundup(len);
		newline = (char *)realloc(*linep, newcap);
		if (newline == NULL)
			return -1;
		*linecapp = newcap;
		*linep = newline;
	}
	return 0;
}

ssize_t getdelim(char **linep, size_t *linecapp, int delimiter, FILE *stream)
{
	size_t linelen;
	int    ch;

	if (linep == NULL || linecapp == NULL)
		return -1;
	if (*linep == NULL)
		*linecapp = 0;
	if (feof(stream)) {
		if (expandtofit(linep, 1, linecapp))
			return -1;
		(*linep)[0] = 0;
		return -1;
	}

	linelen = 0;
	for (;;) {
		ch = fgetc(stream);
		if (ch == EOF)
			break;
		if (expandtofit(linep, linelen + 2, linecapp))
			return -1;
		(*linep)[linelen++] = ch;
		if (ch == delimiter)
			break;
	}
	(*linep)[linelen] = 0;
	if (linelen == 0)
		return -1;
	return linelen;
}
#endif

#ifdef __unix__
FILE *_fsopen(const char *pszFilename, const char *pszMode, int shmode)
{
#ifdef __EMSCRIPTEN__
	fprintf(stderr, "%s not implemented\n", __func__);
	return NULL;
#else
	int         file;
	int         Mode = 0;
	const char *p;

	for (p = pszMode; *p; p++)  {
		switch (*p)  {
			case 'r':
				Mode |= 1;
				break;
			case 'w':
				Mode |= 2;
				break;
			case 'a':
				Mode |= 4;
				break;
			case '+':
				Mode |= 8;
				break;
			case 'b':
			case 't':
				break;
			default:
				errno = EINVAL;
				return NULL;
		}
	}
	switch (Mode)  {
		case 1:
			Mode = O_RDONLY;
			break;
		case 2:
			Mode = O_WRONLY | O_CREAT | O_TRUNC;
			break;
		case 4:
			Mode = O_APPEND | O_WRONLY | O_CREAT;
			break;
		case 9:
			Mode = O_RDWR | O_CREAT;
			break;
		case 10:
			Mode = O_RDWR | O_CREAT | O_TRUNC;
			break;
		case 12:
			Mode = O_RDWR | O_APPEND | O_CREAT;
			break;
		default:
			errno = EINVAL;
			return NULL;
	}
	if (Mode & O_CREAT)
		file = sopen(pszFilename, Mode, shmode, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	else
		file = sopen(pszFilename, Mode, shmode);
	if (file == -1)
		return NULL;
	return fdopen(file, pszMode);
#endif
}
#endif

#ifdef _WIN32
#include <sys/locking.h>    /* LK_LOCK */
#if defined __BORLANDC__
	#define _locking locking
#endif
int xp_lockfile(int file, off_t offset, off_t size, bool block)
{
	int   i;
	off_t pos;

	pos = tell(file);
	if (offset != pos)
		(void)lseek(file, offset, SEEK_SET);
	do {
		i = _locking(file, block ? LK_LOCK : LK_NBLCK, (long)size);
	} while (block && i != 0 && errno == EDEADLOCK);
	if (offset != pos)
		(void)lseek(file, pos, SEEK_SET);
	return i;
}
#endif // _WIN32

