#ifdef __unix__

#include <sys/file.h>
#include <errno.h>
#include <fnmatch.h>
#include <stdio.h>      // Only need for printf() debugging.
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "unix_wrappers.h"
#include "win_wrappers.h"

int
findfirst(char *pathname, struct ffblk *fblk, int attrib)
{
	char  path[MAXPATH+3];
	struct dirent *dp;
	struct stat   file_stat;

	strlcpy(&path[2], pathname, sizeof(path) - 2);
	if (path[2] != '/' && path[2] != '~')  {
		path[0]='.';
		path[1]='/';
	}
	else  {
		memmove(path, &path[2], strlen(&path[2])+1);
	}
	*(strrchr(path,'/')+1)=0;
	fblk->dir_handle = opendir(path);
	if (fblk->dir_handle == NULL)
		return 1;
	strlcpy(fblk->pathname, strrchr(pathname,'/')+1, sizeof(fblk->pathname));

	while ((dp = readdir(fblk->dir_handle)) != NULL)
		if (!fnmatch(fblk->pathname,dp->d_name,0)) {
			stat(dp->d_name,&file_stat);
			// Do this if it's ever needed (Not yet)
			(*fblk).lfn_ctime=0;
			(*fblk).lfn_cdate=0;
			(*fblk).lfn_atime=0;
			(*fblk).lfn_adate=0;
			(*fblk).ff_ftime=0;
			(*fblk).ff_fdate=0;
			(*fblk).ff_fsize=file_stat.st_size;

			if (S_ISDIR(file_stat.st_mode))
				(*fblk).ff_attrib=FA_DIREC;
			if (strrchr(dp->d_name,'/') != NULL)
				strlcpy((*fblk).ff_name, strrchr(dp->d_name,'/')+1, sizeof((*fblk).ff_name));
			else
				strlcpy((*fblk).ff_name, dp->d_name, sizeof((*fblk).ff_name));
			return(0);
		}

	closedir(fblk->dir_handle);
	return(1);
}

int
findnext(struct ffblk *fblk)
{
	struct dirent *dp;
	struct stat   file_stat;

	while ((dp = readdir(fblk->dir_handle)) != NULL)
		if (!fnmatch(fblk->pathname,dp->d_name,0)) {
			stat(dp->d_name,&file_stat);
			// Do this if it's ever needed (Not yet)
			(*fblk).lfn_ctime=0;
			(*fblk).lfn_cdate=0;
			(*fblk).lfn_atime=0;
			(*fblk).lfn_adate=0;
			(*fblk).ff_ftime=0;
			(*fblk).ff_fdate=0;
			(*fblk).ff_fsize=file_stat.st_size;

			if (S_ISDIR(file_stat.st_mode))
				(*fblk).ff_attrib=FA_DIREC;
			if (strrchr(dp->d_name,'/')!=NULL)
				strlcpy((*fblk).ff_name, strrchr(dp->d_name,'/')+1, sizeof((*fblk).ff_name));
			else
				strlcpy((*fblk).ff_name,dp->d_name, sizeof((*fblk).ff_name));
			return(0);
		}

	closedir(fblk->dir_handle);
	return(1);
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

void
GetSystemTime(SYSTEMTIME *t)
{
	time_t    currtime;
	struct tm *gottime;

	currtime=time(NULL);
	gottime=localtime(&currtime);
	t->wSecond = (short)gottime->tm_sec;
	t->wMinute = (short)gottime->tm_min;
	t->wHour = (short)gottime->tm_hour;
	t->wDay = (short)gottime->tm_mday;
	t->wMonth = (short)gottime->tm_mon + 1;
	t->wYear = (short)gottime->tm_year + 1900;
}

#else

static int Windows = 1;

#endif
