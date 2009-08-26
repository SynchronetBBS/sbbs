#include <stdio.h>      // Only need for printf() debugging.
#include <stdlib.h>
#include <sys/file.h>
#include <fnmatch.h>
#include "unix_wrappers.h"

#if defined(__linux__) || defined(__NetBSD__)
void srandomdev()
{
	/*  int f;
	    unsigned int r;
	    f=open("/dev/random",O_RDONLY);
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
strrev(char *str)
{
	char str2[MAX_STRING_LEN];
	size_t i,j;
	j=0;
	for (i=strlen(str)-1; i>-1; i--) {
		str2[j++]=str[i];
	}
	str2[strlen(str)]=0;
	strcpy(str,str2);
	return 0;
}

int
strlwr(char *str)
{
	size_t i;
	for (i=0; i<strlen(str); i++) {
		if (str[i]>64 && str[i]<=91) str[i]=str[i]|32;
	}
	return 0;
}

int
strupr(char *str)
{
	size_t i;
	for (i=0; i<strlen(str); i++) {
		if (str[i]>96 && str[i]<123) str[i] &= 223;
	}
	return 0;
}

int
findfirst(char *pathname, struct ffblk *fblk, int attrib)
{
	char  path[MAXPATH+3];
	struct dirent *dp;
	struct stat   file_stat;

	strcpy(&path[2],pathname);
	if (path[2] != '/' && path[2] != '~')  {
		path[0]='.';
		path[1]='/';
	}
	else  {
		memmove(path, &path[2], strlen(&path[2])+1);
	}
	*(strrchr(path,'/')+1)=0;
	fblk->dir_handle = opendir(path);
	strcpy(fblk->pathname,strrchr(pathname,'/')+1);

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

			if (file_stat.st_mode&S_IFDIR)(*fblk).ff_attrib=FA_DIREC;
			if (strrchr(dp->d_name,'/')!=NULL)
				strcpy((*fblk).ff_name,strrchr(dp->d_name,'/')+1);
			else
				strcpy((*fblk).ff_name,dp->d_name);
			if ((*fblk).ff_name!=NULL)  {
				return(0);
			}
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

			if (file_stat.st_mode&S_IFDIR)(*fblk).ff_attrib=FA_DIREC;
			if (strrchr(dp->d_name,'/')!=NULL)
				strcpy((*fblk).ff_name,strrchr(dp->d_name,'/')+1);
			else
				strcpy((*fblk).ff_name,dp->d_name);
			if ((*fblk).ff_name!=NULL)  {
				return(0);
			}
		}

	closedir(fblk->dir_handle);
	return(1);
}

int
getdate(struct date *getme)
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

int
strset(char *str, char ch)
{
	int x;
	for (x=0; x<sizeof(str); x++) {
		str[x]=ch;
	}
	str[sizeof(str)]=0;
	return 0;
}

int
lock(int file, off_t offset, off_t length)
{
	struct flock mylock;
	mylock.l_start=offset;
	mylock.l_len=length;
	mylock.l_pid=0;
	mylock.l_type=F_WRLCK;
	mylock.l_whence=SEEK_SET;
	return fcntl(file,F_SETLKW,mylock);
}

int
unlock(int file, off_t offset, off_t length)
{
	struct flock mylock;
	mylock.l_start=offset;
	mylock.l_len=length;
	mylock.l_pid=0;
	mylock.l_type=F_UNLCK;
	mylock.l_whence=SEEK_SET;
	return fcntl(file,F_SETLKW,mylock);
}

long
unix_random(long maxnum)
{
	if (maxnum == 0) return 0;
	return (long)random()%maxnum;
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

#ifdef NEED_GETCH
char
getch()
{
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(fileno(stdin),&fds);

	select(1, &fds, NULL, NULL, NULL);
	return (char)getchar();
}
#endif
