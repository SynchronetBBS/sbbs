/* Network open functions (nopen and fnopen) and friends */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "genwrap.h"
#include "dirwrap.h"
#include "filewrap.h"
#include "sockwrap.h"
#include "nopen.h"
#ifdef _WIN32
	#include <io.h>
#endif

/****************************************************************************/
/* Network open function. Opens all files DENYALL, DENYWRITE, or DENYNONE	*/
/* depending on access, and retries LOOP_NOPEN number of times if the		*/
/* attempted file is already open or denying access  for some other reason. */
/****************************************************************************/
int nopen(const char* str, uint access)
{
	int file, share, count = 0;

	if (access & O_DENYNONE) {
		share = SH_DENYNO;
		access &= ~O_DENYNONE;
	}
	else if ((access & ~(O_TEXT | O_BINARY)) == O_RDONLY)
		share = SH_DENYWR;
	else
		share = SH_DENYRW;

#if !defined(__unix__)  /* Basically, a no-op on Unix anyway */
	if (!(access & O_TEXT))
		access |= O_BINARY;
#endif
	while (((file = sopen(str, access, share, DEFFILEMODE)) == -1)
	       && FILE_RETRY_ERRNO(errno) && count++ < LOOP_NOPEN)
		FILE_RETRY_DELAY(count);
	return file;
}

/****************************************************************************/
/* This function performs an nopen, but returns a file stream with a buffer */
/* allocated.																*/
/****************************************************************************/
FILE* fnopen(int* fd, const char* str, uint access)
{
	char*  mode;
	int    file;
	FILE * stream;

	if ((file = nopen(str, access)) == -1)
		return NULL;

	if (fd != NULL)
		*fd = file;

	if (access & O_APPEND) {
		if ((access & O_RDWR) == O_RDWR)
			mode = "a+";
		else
			mode = "a";
	} else if (access & (O_TRUNC | O_WRONLY)) {
		if ((access & O_RDWR) == O_RDWR)
			mode = "w+";
		else
			mode = "w";
	} else {
		if ((access & O_RDWR) == O_RDWR)
			mode = "r+";
		else
			mode = "r";
	}
	stream = fdopen(file, mode);
	if (stream == NULL) {
		close(file);
		return NULL;
	}
	setvbuf(stream, NULL, _IOFBF, FNOPEN_BUF_SIZE);
	return stream;
}

bool ftouch(const char* fname)
{
	int file;

	/* update the time stamp */
	if (utime(fname, NULL) == 0)
		return true;

	/* create the file */
	if ((file = nopen(fname, O_WRONLY | O_CREAT)) < 0)
		return false;
	close(file);
	return true;
}

fmutex_t fmutex_init(void)
{
	fmutex_t fm = { 0 };
	fm.fd = -1;
	fm.time = -1;
	return fm;
}

// Opens a mutex file (implementation)
static
bool _fmutex_open(fmutex_t* fm, const char* text, long max_age, bool auto_remove)
{
	size_t len;
	time_t now;
#if !defined(NO_SOCKET_SUPPORT)
	char   hostname[128];
#endif
#if defined _WIN32
	DWORD  attributes = FILE_ATTRIBUTE_NORMAL;
	HANDLE h;
#endif

	if (fm == NULL)
		return false;
	now = time(NULL);
	fm->fd = -1;
	fm->time = fdate(fm->name);
	if (max_age > 0 && fm->time != -1 && now > fm->time && (now - fm->time) > max_age) {
		if (remove(fm->name) != 0)
			return false;
	}
#if defined _WIN32
	if (auto_remove)
		attributes |= FILE_FLAG_DELETE_ON_CLOSE;
	h = CreateFileA(fm->name,
	                GENERIC_WRITE, // dwDesiredAccess
	                0,  // dwShareMode (deny all)
	                NULL, // lpSecurityAttributes,
	                CREATE_NEW, // dwCreationDisposition
	                attributes, // dwFlagsAndAttributes,
	                NULL // hTemplateFile
	                );
	if (h == INVALID_HANDLE_VALUE)
		return false;
	if (!LockFile(h,
	              0, // dwFileOffsetLow
	              0, // dwFileOffsetHigh
	              1, // nNumberOfBytesToLockLow
	              0 // nNumberOfBytesToLockHigh
	              )) {
		CloseHandle(h);
		return false;
	}
	if ((fm->fd = _open_osfhandle((intptr_t)h, O_WRONLY)) == -1) {
		CloseHandle(h);
		return false;
	}
#else
	if ((fm->fd = sopen(fm->name, O_CREAT | O_WRONLY | O_EXCL, SH_DENYRW, DEFFILEMODE)) < 0)
		return false;
#endif
#if !defined(NO_SOCKET_SUPPORT)
	if (text == NULL && gethostname(hostname, sizeof(hostname)) == 0)
		text = hostname;
#endif
	if (text != NULL) {
		len = strlen(text);
		if (write(fm->fd, text, len) != len) {
			close(fm->fd);
			return false;
		}
	}
	return true;
}

// Opens a mutex file (public API: always auto-removes upon close)
bool fmutex_open(fmutex_t* fm, const char* text, long max_age)
{
	return _fmutex_open(fm, text, max_age, /* auto-remove: */ true);
}

bool fmutex_close(fmutex_t* fm)
{
	if (fm == NULL)
		return false;
	if (fm->fd < 0) // already closed (or never opened)
		return true;
#if !defined _WIN32 // should only be necessary (and possible) on *nix
	// We remove before close to insure the file we remove is the file we opened
	if (unlink(fm->name) != 0)
		return false;
#endif
	if (close(fm->fd) != 0)
		return false;
	fm->fd = -1;
	return true;
}

// Opens and immediately closes a mutex file
bool fmutex(const char* fname, const char* text, long max_age, time_t* tp)
{
	fmutex_t fm;

	SAFECOPY(fm.name, fname);
	if (!_fmutex_open(&fm, text, max_age, /* auto_remove: */ false)) {
		if (tp != NULL)
			*tp = fm.time;
		return false;
	}
	return close(fm.fd) == 0;
}

bool fcompare(const char* fn1, const char* fn2)
{
	FILE* fp1;
	FILE* fp2;
	bool  success = true;

	if (flength(fn1) != flength(fn2))
		return false;
	if ((fp1 = fopen(fn1, "rb")) == NULL)
		return false;
	if ((fp2 = fopen(fn2, "rb")) == NULL) {
		fclose(fp1);
		return false;
	}

	while (!feof(fp1) && success) {
		if (fgetc(fp1) != fgetc(fp2))
			success = false;
	}

	fclose(fp1);
	fclose(fp2);

	return success;
}


/****************************************************************************/
/****************************************************************************/
bool backup(const char *fname, int backup_level, bool ren)
{
	char  oldname[MAX_PATH + 1];
	char  newname[MAX_PATH + 1];
	char* ext;
	int   i;
	int   len;

	if (flength(fname) < 1)  /* no need to backup a 0-byte (or non-existent) file */
		return false;

	if ((ext = strrchr(fname, '.')) == NULL)
		ext = "";

	len = strlen(fname) - strlen(ext);

	for (i = backup_level; i; i--) {
		safe_snprintf(newname, sizeof(newname), "%.*s.%d%s", len, fname, i - 1, ext);
		if (i == backup_level)
			if (fexist(newname) && remove(newname) != 0)
				return false;
		if (i == 1) {
			if (ren == true) {
				if (rename(fname, newname) != 0)
					return false;
			} else {
				struct utimbuf ut;

				/* preserve the original time stamp */
				ut.modtime = fdate(fname);

				if (!CopyFile(fname, newname, /* failIfExists: */ false))
					return false;

				ut.actime = time(NULL);
				utime(newname, &ut);
			}
			continue;
		}
		safe_snprintf(oldname, sizeof(oldname), "%.*s.%d%s", len, fname, i - 2, ext);
		if (fexist(oldname) && rename(oldname, newname) != 0)
			return false;
	}

	return true;
}
