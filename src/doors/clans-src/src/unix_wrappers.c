#ifdef __unix__

#include <sys/file.h>
#include <sys/stat.h>
#include <errno.h>
#include <fnmatch.h>
#include <glob.h>
#include <stdio.h>      // Only need for printf() debugging.
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "unix_wrappers.h"
#include "win_wrappers.h"

FILE *
_fsopen(const char *pathname, const char *mode, int shflag)
{
	FILE *thefile;
	bool isRead = strchr(mode, 'r');
	bool isWrite = strchr(mode, 'w');
	if (!isRead)
		isRead = strchr(mode, '+');
	if (!isWrite)
		isWrite = strchr(mode, '+');
	if (!isWrite)
		isWrite = strchr(mode, 'a');

	thefile = fopen(pathname, mode);
	if (thefile != NULL) {
		// Fix up share type...
		if (shflag == _SH_DENYWR) {
			if (!isRead) {
				shflag = _SH_DENYRW;
			}
		}
		if (shflag == _SH_DENYRW) {
			if (!isWrite) {
				shflag = _SH_DENYWR;
			}
		}
		if ((shflag == _SH_DENYRW) || (shflag == _SH_DENYWR)) {
			struct flock f = {
				.l_type = shflag & _SH_DENYWR ? F_RDLCK : F_WRLCK,
				.l_whence = SEEK_SET
			};
			if (fcntl(fileno(thefile), F_SETLKW, &f) == -1) {
				fclose(thefile);
				return NULL;
			}
		}
	}
	return(thefile);
}

void
delay(unsigned msec)
{
	struct timespec ts = {
		.tv_sec = msec/1000,
		.tv_nsec = (long)(msec % 1000) * 1000000,
	};
	int ret;
	do {
		ret = nanosleep(&ts, &ts);
	} while (ret == -1 && errno == EINTR);
}

struct Sortable {
	const char *p;
	struct stat st;
};

static int
scmp(const void *s1p, const void *s2p)
{
	struct Sortable const * const s1 = s1p;
	struct Sortable const * const s2 = s2p;

	if (s1->st.st_mtime != s2->st.st_mtime) {
		if (s1->st.st_mtime < s2->st.st_mtime)
			return -1;
		return 1;
	}
	return strcmp(s1->p, s2->p);
}

const char *FileName(const char *path)
{
	const char *LastSlash = strrchr(path, '/');
	if (LastSlash)
		return &LastSlash[1];
	return path;
}

void FreeFileList(char **fl)
{
	for (char **f = fl; *f; f++)
		free(*f);
	free(fl);
}

char **FilesOrderedByDate(const char *path, const char *match, bool *error)
{
	*error = false;
	size_t pathlen = path ? strlen(path) : 0;
	bool add_slash = pathlen && path[pathlen - 1] != '/';
	char *Expanded = malloc(pathlen + add_slash + strlen(match) * 4 + 1);
	if (Expanded == NULL) {
		*error = true;
		return NULL;
	}
	char *out = Expanded;
	if (path) {
		for (const char *in = path; *in; in++) {
			*(out++) = *(in);
		}
		if (add_slash)
			*(out++) = '/';
	}
	// Expand to make case insentitive...
	for (const char *in = match; *in; in++) {
		if (*in >= 'a' && *in <= 'z') {
			*(out++) = '[';
			*(out++) = *in & 0x5F;
			*(out++) = *in;
			*(out++) = ']';
		}
		else if (*in >= 'A' && *in <= 'Z') {
			*(out++) = '[';
			*(out++) = *in;
			*(out++) = *in | 0x20;
			*(out++) = ']';
		}
		else
			*(out++) = *in;
	}
	*out = 0;

	glob_t gl = {0};
	int globret = glob(Expanded, GLOB_NOSORT | GLOB_NOCHECK, NULL, &gl);
	if (globret) {
		free(Expanded);
		if (globret != GLOB_NOMATCH)
			*error = true;
		return NULL;
	}
	free(Expanded);
	if (gl.gl_pathc == 0) {
		globfree(&gl);
		return NULL;
	}

	size_t sortlen = 0;
	struct Sortable *sa = malloc(sizeof(struct Sortable) * gl.gl_pathc);
	for (size_t match = 0; match < gl.gl_pathc; match++) {
		if (!stat(gl.gl_pathv[match], &sa[sortlen].st)) {
			if (S_ISDIR(sa[sortlen].st.st_mode))
				continue;
			sa[sortlen].p = gl.gl_pathv[match];
			sortlen++;
		}
	}
	if (sortlen == 0) {
		free(sa);
		globfree(&gl);
		return NULL;
	}
	qsort(sa, sortlen, sizeof(struct Sortable), scmp);
	char **ret = calloc(sortlen + 1, sizeof(char *));
	if (ret == NULL) {
		*error = true;
		free(sa);
		globfree(&gl);
		return NULL;
	}
	for (size_t match = 0; match < sortlen; match++) {
		ret[match] = strdup(sa[match].p);
		if (ret[match] == NULL) {
			globfree(&gl);
			free(sa);
			FreeFileList(ret);
			*error = true;
			return NULL;
		}
	}
	globfree(&gl);
	free(sa);
	return ret;
}

#else

static int Windows = 1;

#endif
