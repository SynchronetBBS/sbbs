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
	return _fsopen(pathname, mode, winflag);
}

bool DirExists(const char *pszDirName)
{
	struct stat file_stats;

	if (pszDirName == NULL)
		return false;

	char *copy = _strdup(pszDirName);
	size_t len = strlen(copy);

	/* Remove trailing slash */
	if (len > 0 && (copy[len - 1] == '/' || copy[len - 1] == '\\'))
		copy[len - 1] = '\0';

	bool result = false;
	if (stat(copy, &file_stats) == 0)
		result = S_ISDIR(file_stats.st_mode);

	free(copy);
	return result;
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
	LPVOID message;
	TCHAR buffer[1000];

	FormatMessage(
		FORMAT_MESSAGE_IGNORE_INSERTS|
		FORMAT_MESSAGE_FROM_SYSTEM|
		FORMAT_MESSAGE_ALLOCATE_BUFFER,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&message,
		0,
		NULL);

	_stprintf(buffer, _T("Win32 System Error:\n%s\n"), (char*)message);
	MessageBox(NULL, buffer, _T("System Error"), MB_OK | MB_ICONERROR);

	LocalFree(message);
}

struct Sortable {
	char *p;
	time_t wt;
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

	struct _finddata_t fd;
	intptr_t sh = _findfirst(Backslashed, &fd);
	char *last_bs = strrchr(Backslashed, '\\');
	size_t path_bytes = last_bs ? ((size_t)(last_bs - Backslashed) + 1) : 0;
	char *tmpstr = malloc(_MAX_PATH + path_bytes);
	if (tmpstr == NULL) {
		free(Backslashed);
		*error = true;
		return NULL;
	}
	if (path_bytes)
		memcpy(tmpstr, Backslashed, path_bytes);
	free(Backslashed);
	if (sh == -1) {
		if (errno != ENOENT)
			*error = true;
		free(tmpstr);
		return NULL;
	}
	int rval;
	do {
		if (fd.attrib & _A_SUBDIR)
			continue;
		struct Sortable *nsa = realloc(sa, sizeof(struct Sortable) * (found + 1));
		if (nsa == NULL) {
			FreePartialSortableArray(sa, found);
			_findclose(sh);
			*error = true;
			free(tmpstr);
			return NULL;
		}
		sa = nsa;
		strlcpy(&tmpstr[path_bytes], fd.name, _MAX_PATH);
		sa[found].p = strdup(tmpstr);
		if (sa[found].p == NULL) {
			FreePartialSortableArray(sa, found);
			_findclose(sh);
			*error = true;
			free(tmpstr);
			return NULL;
		}
		sa[found].wt = fd.time_write;
		found++;
	} while (!(rval = _findnext(sh, &fd)));
	if (errno != ENOENT) {
		_findclose(sh);
		FreePartialSortableArray(sa, found);
		*error = true;
		free(tmpstr);
		return NULL;
	}
	_findclose(sh);
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

	*timep = 0;
	*timep = (((file_dt.tm_sec + 2) / 2) & 0x1f);
	*timep |= ((file_dt.tm_min) & 0x3f) << 5;
	*timep |= (uint16_t)(((file_dt.tm_hour) & 0x1f) << 11);

	return 0;
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
