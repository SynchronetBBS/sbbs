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

bool plat_DeleteFile(const char *fname)
{
	const int rc = unlink(fname);
	return (rc == 0);
}

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
plat_Delay(unsigned msec)
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
	if (!sa) {
		globfree(&gl);
		return NULL;
	}
	for (size_t matchp = 0; matchp < gl.gl_pathc; matchp++) {
		if (!stat(gl.gl_pathv[matchp], &sa[sortlen].st)) {
			if (S_ISDIR(sa[sortlen].st.st_mode))
				continue;
			sa[sortlen].p = gl.gl_pathv[matchp];
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
	for (size_t matchp = 0; matchp < sortlen; matchp++) {
		ret[matchp] = strdup(sa[matchp].p);
		if (ret[matchp] == NULL) {
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

char * fullpath(char *target, const char *path, size_t size)
{
	char    *out;
	char    *p;
	bool alloced = false;

	if (target==NULL)  {
		if ((target=malloc(PATH_MAX+1))==NULL) {
			return(NULL);
		}
		alloced = true;
	}
	out=target;
	*out=0;

	if (*path != '/')  {
		p=getcwd(target,size);
		if (p==NULL || strlen(p)+strlen(path)>=size) {
			if (alloced)
				free(target);
			return(NULL);
		}
		out=strrchr(target,'\0');
		*(out++)='/';
		*out=0;
		out--;
	}
	strlcat(target, path, size);

	for (; *out; out++)  {
		while (*out=='/')  {
			if (*(out+1)=='/')
				memmove(out,out+1,strlen(out));
			else if (*(out+1)=='.' && (*(out+2)=='/' || *(out+2)==0))
				memmove(out,out+2,strlen(out)-1);
			else if (*(out+1)=='.' && *(out+2)=='.' && (*(out+3)=='/' || *(out+3)==0))  {
				*out=0;
				p=strrchr(target,'/');
				memmove(p,out+3,strlen(out+3)+1);
				out=p;
			}
			else  {
				out++;
			}
		}
		if (!*out)
			break;
	}
	return(target);
}

bool plat_getftime(FILE *fd, uint16_t *datep, uint16_t *timep)
{
	struct stat file_stats;
	struct tm *file_datetime;
	struct tm file_dt;
	int handle = fileno(fd);

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

	return true;
}

bool plat_setftime(FILE *fd, unsigned short date, unsigned short time)
{
	struct tm dos_dt;
	time_t file_dt;
	struct timespec tmv_buf[2];
	int handle = fileno(fd);

	memset(&dos_dt, 0, sizeof(struct tm));
	dos_dt.tm_year = ((date & 0xfe00) >> 9) + 80;
	dos_dt.tm_mon  = ((date & 0x01e0) >> 5) + 1;
	dos_dt.tm_mday = (date & 0x001f);
	dos_dt.tm_hour = (time & 0xf800) >> 11;
	dos_dt.tm_min  = (time & 0x07e0) >> 5;
	dos_dt.tm_sec  = (time & 0x001f) * 2;

	file_dt = mktime(&dos_dt);

	tmv_buf[0].tv_sec = file_dt;
	tmv_buf[0].tv_nsec = file_dt * 1000000;
	tmv_buf[1].tv_sec = file_dt;
	tmv_buf[1].tv_nsec = file_dt * 1000000;
	return (futimens(handle, tmv_buf) == 0);
}
#endif
