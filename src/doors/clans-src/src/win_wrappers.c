#ifdef _WIN32

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <io.h>

#include "win_wrappers.h"

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

	char *Backslashed = malloc(strlen(path) + strlen(match) + 2);
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
	memcpy(tmpstr, Backslashed, path_bytes);
	free(Backslashed);
	if (sh == -1) {
		if (errno != ENOENT)
			*error = true;
		return NULL;
	}
	int rval;
	while (!(rval = _findnext(sh, &fd))) {
		if (fd.attrib & _A_SUBDIR)
			continue;
		struct Sortable *nsa = realloc(sa, sizeof(struct Sortable) * (found + 1));
		if (nsa == NULL) {
			FreePartialSortableArray(sa, found);
			_findclose(sh);
			*error = true;
			return NULL;
		}
		sa = nsa;
		strlcpy(&tmpstr[path_bytes], fd.name, _MAX_PATH);
		sa[found].p = strdup(tmpstr);
		if (sa[found].p == NULL) {
			FreePartialSortableArray(sa, found);
			_findclose(sh);
			*error = true;
			return NULL;
		}
		sa[found].wt = fd.time_write;
		found++;
	}
	if (errno != ENOENT) {
		_findclose(sh);
		FreePartialSortableArray(sa, found);
		*error = true;
		return NULL;
	}
	_findclose(sh);
	if (found == 0) {
		return NULL;
	}
	qsort(sa, found, sizeof(struct Sortable), scmp);
	char **ret = calloc(found + 1, sizeof(char *));
	if (ret == NULL) {
		*error = true;
		FreePartialSortableArray(sa, found);
		return NULL;
	}
	for (size_t match = 0; match < found; match++) {
		ret[match] = sa[match].p;
	}
	free(sa);
	return ret;
}

#endif
