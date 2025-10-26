#include <stdio.h>      // Only need for printf() debugging.
#include <stdlib.h>

#ifdef __unix__

#include <sys/file.h>
#include <fnmatch.h>

#include "unix_wrappers.h"
#include "win_wrappers.h"

#if defined(__linux__) || defined(__NetBSD__)
void srandomdev(void)
{
	/*  int f;
	    unsigned int r;
	    f = open("/dev/random",O_RDONLY);
	    read(f,&r,sizeof(r));
	    close(f);*/
	srand(time(0));
}
#endif

off_t
filelength(int file)
{
	struct stat file_stat;
	fstat(file,&file_stat);
	return file_stat.st_size;
}

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

int
clans_getdate(struct date *getme)
{
	time_t    currtime;
	struct tm     *thetime;

	currtime=time(NULL);
	thetime=localtime(&currtime);
	(*getme).da_year=(*thetime).tm_year+1900;
	(*getme).da_day=(*thetime).tm_mday;
	(*getme).da_mon=(*thetime).tm_mon+1;
	return 0;
}

int
gettime(struct tm *getme)
{
	time_t    currtime;
	struct tm *gottime;

	currtime=time(NULL);
	gottime=localtime(&currtime);
	(*getme).tm_sec=(*gottime).tm_sec;
	(*getme).tm_min=(*gottime).tm_min;
	(*getme).tm_hour=(*gottime).tm_hour;
	return 0;
}

int32_t
unix_random(int32_t maxnum)
{
	if (maxnum == 0) return 0;
	return (int32_t)random()%maxnum;
}

FILE *
_fsopen(char *pathname, char *mode, int flags)
{
	FILE *thefile;

	thefile = fopen(pathname, mode);
	if (thefile != NULL)  {
		if (flags&SH_DENYWR) {
			flock(fileno(thefile), LOCK_SH);
			return(thefile);
		}
		if (flags&SH_DENYRW) {
			flock(fileno(thefile), LOCK_EX);
			return(thefile);
		}
	}
	return(thefile);
}

void
delay(unsigned msec)
{
	struct timeval delaytime;

	delaytime.tv_sec=msec/1000;
	delaytime.tv_usec=msec % 1000;

	select(0,NULL,NULL,NULL,&delaytime);
}

void
GetSystemTime(SYSTEMTIME *t)
{
	time_t    currtime;
	struct tm *gottime;

	currtime=time(NULL);
	gottime=localtime(&currtime);
	t->wSecond = gottime->tm_sec;
	t->wMinute = gottime->tm_min;
	t->wHour = gottime->tm_hour;
	t->wDay = gottime->tm_mday;
	t->wMonth = gottime->tm_mon + 1;
	t->wYear = gottime->tm_year + 1900;
}

#endif
