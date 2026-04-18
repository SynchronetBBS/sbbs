#ifdef _WIN32

#include <fileapi.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <io.h>

#include "win_wrappers.h"

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L
char *strdup(const char *s)
{
	return _strdup(s);
}
#endif

bool plat_CreateSemfile(const char *filename, const char *content)
{
	FILE *fp = fopen(filename, "w+x");
	if (!fp)
		return false;
	fputs(content, fp);
	fclose(fp);
	return true;
}

FILE *plat_fsopen(const char *pathname, const char *mode, int shflag)
{
	int winflag = 0;
	if (shflag & PLAT_SH_DENYWR)
		winflag |= _SH_DENYWR;
	if (shflag & PLAT_SH_DENYRW)
		winflag |= _SH_DENYRW;

	/* _fsopen with an exclusive share mode fails immediately with
	   EACCES on contention; the Unix side uses fcntl(F_SETLKW) which
	   blocks.  Approximate blocking behaviour with a bounded retry
	   so transient conflicts (peer writing a packet, log rotation,
	   etc.) don't surface as spurious open failures. */
	FILE *fp = _fsopen(pathname, mode, winflag);
	if (fp != NULL || winflag == 0)
		return fp;
	for (int i = 0; i < 20 && errno == EACCES; i++) {
		Sleep(50);
		fp = _fsopen(pathname, mode, winflag);
		if (fp != NULL)
			return fp;
	}
	return NULL;
}

bool DirExists(const char *pszDirName)
{
	if (pszDirName == NULL || pszDirName[0] == '\0')
		return false;

	/* GetFileAttributesA handles drive roots ("C:\") and trailing
	   separators without the quirks stat() has around drive letters. */
	DWORD attr = GetFileAttributesA(pszDirName);
	if (attr == INVALID_FILE_ATTRIBUTES)
		return false;
	return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool plat_getmode(const char *filename, unsigned *mode)
{
	(void)filename;
	*mode = 0;
	return false;
}

bool plat_chmod(const char *filename, unsigned mode)
{
	(void)filename;
	(void)mode;
	return true;
}

bool plat_mkdir(const char *dir)
{
	return (_mkdir(dir) == 0);
}

void plat_GetExePath(const char *argv0, char *buf, size_t bufsz)
{
	(void)argv0;
	GetModuleFileName(NULL, buf, (DWORD)bufsz);
	// Some versions of GetModuleFileName() don't NUL terminate.
	buf[bufsz - 1] = 0;
	/* Strip to directory */
	size_t len = strlen(buf);
	while (len > 0 && buf[len] != '\\' && buf[len] != '/')
		len--;
	if (len > 0)
		len++;
	buf[len] = 0;
}

int plat_stricmp(const char *s1, const char *s2)
{
	return _stricmp(s1, s2);
}

void plat_Delay(unsigned msec)
{
	Sleep(msec);
}

bool plat_DeleteFile(const char *fname)
{
	return DeleteFileA((LPCSTR)fname);
}

void display_win32_error(void)
{
	LPTSTR message = NULL;
	TCHAR buffer[1000];
	DWORD rc;

	rc = FormatMessage(
		FORMAT_MESSAGE_IGNORE_INSERTS|
		FORMAT_MESSAGE_FROM_SYSTEM|
		FORMAT_MESSAGE_ALLOCATE_BUFFER,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&message,
		0,
		NULL);

	if (rc == 0 || message == NULL) {
		_sntprintf(buffer, sizeof(buffer) / sizeof(buffer[0]),
		    _T("Win32 System Error %lu (FormatMessage failed)"),
		    (unsigned long)GetLastError());
	}
	else {
		_sntprintf(buffer, sizeof(buffer) / sizeof(buffer[0]),
		    _T("Win32 System Error:\n%s\n"), message);
	}
	buffer[(sizeof(buffer) / sizeof(buffer[0])) - 1] = 0;
	MessageBox(NULL, buffer, _T("System Error"), MB_OK | MB_ICONERROR);

	if (message != NULL)
		LocalFree(message);
}

struct Sortable {
	char *p;
	uint64_t wt;	/* FILETIME packed as 100ns ticks since 1601 */
};

static int
scmp(const void *s1p, const void *s2p)
{
	struct Sortable const * const s1 = s1p;
	struct Sortable const * const s2 = s2p;

	if (s1->wt != s2->wt) {
		if (s1->wt < s2->wt)
			return -1;
		return 1;
	}
	return strcmp(s1->p, s2->p);
}

const char *FileName(const char *path)
{
	const char *LastSlash = strrchr(path, '/');
	const char *LastBSlash = strrchr(path, '\\');
	if (LastSlash && (LastBSlash == NULL || LastBSlash < LastSlash))
		return &LastSlash[1];
	if (LastBSlash && (LastSlash == NULL || LastSlash < LastBSlash))
		return &LastBSlash[1];
	// No slash or backslash, check for drive letter
	if (path[0] && path[1] == ':')
		return &path[2];
	return path;
}

void FreeFileList(char **fl)
{
	for (char **f = fl; *f; f++)
		free(*f);
	free(fl);
}

static void
FreePartialSortableArray(struct Sortable *sa, size_t found)
{
	if (sa == NULL)
		return;
	for (size_t i = 0; i < found; i++) {
		free(sa[i].p);
	}
	free(sa);
}

char **FilesOrderedByDate(const char *path, const char *match, bool *error)
{
	struct Sortable *sa = NULL;
	size_t found = 0;
	*error = false;

	char *Backslashed = malloc((path ? strlen(path) : 0) + strlen(match) + 2);
	if (Backslashed == NULL) {
		*error = true;
		return NULL;
	}
	char *out = Backslashed;
	if (path) {
		bool last_was_slash = false;
		for (const char *in = path; *in; in++) {
			if (*in == '/')
				*(out) = '\\';
			else
				*(out) = *(in);
			last_was_slash = (*out == '\\');
			out++;
		}
		if (!last_was_slash)
			*(out++) = '\\';
	}
	for (const char *in = match; *in; in++) {
		*(out++) = *in;
	}
	*out = 0;

	WIN32_FIND_DATAA fd;
	HANDLE sh = FindFirstFileA(Backslashed, &fd);
	char *last_bs = strrchr(Backslashed, '\\');
	size_t path_bytes = last_bs ? ((size_t)(last_bs - Backslashed) + 1) : 0;
	char *tmpstr = malloc(MAX_PATH + path_bytes);
	if (tmpstr == NULL) {
		free(Backslashed);
		*error = true;
		return NULL;
	}
	if (path_bytes)
		memcpy(tmpstr, Backslashed, path_bytes);
	free(Backslashed);
	if (sh == INVALID_HANDLE_VALUE) {
		DWORD err = GetLastError();
		/* ERROR_FILE_NOT_FOUND and ERROR_PATH_NOT_FOUND are the
		   benign "empty listing" cases. Anything else is a real
		   I/O error the caller should know about. */
		if (err != ERROR_FILE_NOT_FOUND && err != ERROR_PATH_NOT_FOUND
		    && err != ERROR_NO_MORE_FILES)
			*error = true;
		free(tmpstr);
		return NULL;
	}
	do {
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			continue;
		struct Sortable *nsa = realloc(sa, sizeof(struct Sortable) * (found + 1));
		if (nsa == NULL) {
			FreePartialSortableArray(sa, found);
			FindClose(sh);
			*error = true;
			free(tmpstr);
			return NULL;
		}
		sa = nsa;
		strlcpy(&tmpstr[path_bytes], fd.cFileName, MAX_PATH);
		sa[found].p = strdup(tmpstr);
		if (sa[found].p == NULL) {
			FreePartialSortableArray(sa, found);
			FindClose(sh);
			*error = true;
			free(tmpstr);
			return NULL;
		}
		sa[found].wt = ((uint64_t)fd.ftLastWriteTime.dwHighDateTime << 32)
		             | (uint64_t)fd.ftLastWriteTime.dwLowDateTime;
		found++;
	} while (FindNextFileA(sh, &fd));
	/* FindNextFile returns FALSE both on end-of-iteration and on real
	   errors; GetLastError() disambiguates. */
	if (GetLastError() != ERROR_NO_MORE_FILES) {
		FindClose(sh);
		FreePartialSortableArray(sa, found);
		*error = true;
		free(tmpstr);
		return NULL;
	}
	FindClose(sh);
	if (found == 0) {
		free(tmpstr);
		return NULL;
	}
	qsort(sa, found, sizeof(struct Sortable), scmp);
	char **ret = calloc(found + 1, sizeof(char *));
	if (ret == NULL) {
		*error = true;
		FreePartialSortableArray(sa, found);
		free(tmpstr);
		return NULL;
	}
	for (size_t match = 0; match < found; match++) {
		ret[match] = sa[match].p;
	}
	free(sa);
	free(tmpstr);
	return ret;
}

bool plat_getftime(FILE *fd, uint16_t *datep, uint16_t *timep)
{
	struct stat file_stats;
	struct tm *file_datetime;
	struct tm file_dt;
	int handle = _fileno(fd);

	if (fstat(handle, &file_stats) != 0) {
		*datep = (1 << 5) | 1;
		*timep = 0;
		return false;
	}

	file_datetime = localtime(&file_stats.st_mtime);
	if (!file_datetime)
		return false;
	memcpy(&file_dt, file_datetime, sizeof(struct tm));

	*datep = 0;
	*datep = ((file_dt.tm_mday) & 0x1f);
	*datep |= ((file_dt.tm_mon + 1) & 0x0f) << 5;
	*datep |= (uint16_t)(((file_dt.tm_year - 80) & 0x7f) << 9);

	/* DOS time: 5-bit sec/2 (valid 0-29), 6-bit min, 5-bit hour.
	   Truncate seconds (floor) -- rounding up can produce 30 for
	   tm_sec=59 which reads back as an invalid 60s. */
	*timep = 0;
	*timep = ((uint16_t)(file_dt.tm_sec / 2) & 0x1f);
	*timep |= ((file_dt.tm_min) & 0x3f) << 5;
	*timep |= (uint16_t)(((file_dt.tm_hour) & 0x1f) << 11);

	return true;
}

bool plat_setftime(FILE *fd, unsigned short date, unsigned short time)
{
	struct tm dos_dt;
	time_t file_dt;
	struct _utimbuf tm_buf;
	int handle = _fileno(fd);

	memset(&dos_dt, 0, sizeof(struct tm));
	dos_dt.tm_year = ((date & 0xfe00) >> 9) + 80;
	dos_dt.tm_mon  = ((date & 0x01e0) >> 5) + 1;
	dos_dt.tm_mday = (date & 0x001f);
	dos_dt.tm_hour = (time & 0xf800) >> 11;
	dos_dt.tm_min  = (time & 0x07e0) >> 5;
	dos_dt.tm_sec  = (time & 0x001f) * 2;

	file_dt = mktime(&dos_dt);

	tm_buf.actime = tm_buf.modtime = file_dt;
	return (_futime(handle, &tm_buf) == 0);
}
#endif
