/* Directory-related system-call wrappers */

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

#include <string.h>	/* strrchr */

#if defined(_WIN32)

	#include <windows.h>	/* WINAPI, etc */
	#include <io.h>			/* _findfirst */

#elif defined __unix__

	#include <unistd.h>		/* usleep */
	#include <fcntl.h>		/* O_NOCCTY */
	#include <ctype.h>		/* toupper */
	#include <sys/param.h>

	#if defined(BSD)
		#include <sys/mount.h>
	#endif
	#if defined(__FreeBSD__)
		#include <sys/kbio.h>
	#endif
	#if defined(__NetBSD_Version__) && (__NetBSD_Version__ >= 300000000 /* NetBSD 3.0 */)
		#include <sys/statvfs.h>
	#endif

	#include <sys/ioctl.h>	/* ioctl */

	#if defined(__GLIBC__)		/* actually, BSD, but will work for now */
		#include <sys/vfs.h>    /* statfs() */
	#endif

	#if defined(__solaris__)
		#include <sys/statvfs.h>
	#endif

#endif /* __unix__ */

#if defined(__WATCOMC__)
	#include <dos.h>
#endif

#include <sys/types.h>	/* _dev_t */

#include <stdio.h>		/* sprintf */
#include <stdlib.h>		/* rand */
#include <errno.h>		/* ENOENT definitions */

#include "genwrap.h"	/* strupr/strlwr */
#include "dirwrap.h"
#include "filewrap.h"	/* stat */

#if !defined(S_ISDIR)
	#define S_ISDIR(x)	((x)&S_IFDIR)
#endif

/****************************************************************************/
/* Return the filename portion of a full pathname							*/
/****************************************************************************/
char* getfname(const char* path)
{
	const char* fname;
	const char* bslash;

	fname=strrchr(path,'/');
	bslash=strrchr(path,'\\');
	if(bslash>fname)
		fname=bslash;
	if(fname!=NULL)
		fname++;
	else
		fname=(char*)path;
	return((char*)fname);
}

/****************************************************************************/
/* Return the filename or last directory portion of a full pathname			*/
/* A directory pathname is expected to end in a '/'							*/
/****************************************************************************/
char* getdirname(const char* path)
{
	char* last = lastchar(path);
	if(*last == '/') {
		if(last == path)
			return last;
		for(last--; last >= path; last--) {
			if(IS_PATH_DELIM(*last))
				return last + 1;
		}
		return (char*)path;
	}
	return getfname(path);
}

/****************************************************************************/
/* Return a pointer to a file's extension/suffix (beginning with '.')		*/
/****************************************************************************/
char* getfext(const char* path)
{
	char *fname;
	char *fext;

	fname=getfname(path);
	fext=strrchr(fname,'.');
	if(fext==NULL || fext==fname)
		return(NULL);
	return(fext);
}

/****************************************************************************/
/* Break a path name into components.										*/
/****************************************************************************/
#if defined(__unix__)
void _splitpath(const char *path, char *drive, char *dir, char *fname, char *ext)
{
	char*	p;

	ext[0]=0;
	drive[0]=0;			/* no drive letters on Unix */

	snprintf(dir, MAX_PATH+1, "%s", path);	/* Optional directory path, including trailing slash. */
	p=getfname(dir);
	snprintf(fname, MAX_PATH+1, "%s", p);	/* Base filename (no extension) */
	if(p==dir)
		dir[0]=0;		/* no directory specified in path */
	else
		*p=0;			/* truncate dir at filename */
	p=getfext(fname);
	if(p!=NULL) {
		snprintf(ext, MAX_PATH+1, "%s", p);	/* Optional filename extension, including leading period (.) */
		*p=0;
	}
}
#endif

/****************************************************************************/
/* Win32 (minimal) implementation of POSIX.2 glob() function				*/
/* This code _may_ work on other DOS-based platforms (e.g. OS/2)			*/
/****************************************************************************/
#if !defined(__unix__)
static int __cdecl glob_compare( const void *arg1, const void *arg2 )
{
   /* Compare all of both strings: */
   return strcmp( * ( char** ) arg1, * ( char** ) arg2 );
}

#if defined(__BORLANDC__)
	#pragma argsused
#endif

#if defined(__WATCOMC__)

int	glob(const char *pattern, int flags, void* unused, glob_t* glob)
{
    struct	find_t ff;
	size_t	found=0;
	char	path[MAX_PATH+1];
	char*	p;
	char**	new_pathv;

	if(!(flags&GLOB_APPEND)) {
		glob->gl_pathc=0;
		glob->gl_pathv=NULL;
	}

	if(_dos_findfirst((char*)pattern,(flags&GLOB_PERIOD) ? _A_HIDDEN : _A_NORMAL,&ff)!=0)
		return(GLOB_NOMATCH);

	do {
		if((flags&GLOB_PERIOD || ff.name[0]!='.') &&
			(!(flags&GLOB_ONLYDIR) || ff.attrib&_A_SUBDIR)) {
			if((new_pathv=realloc(glob->gl_pathv
				,(glob->gl_pathc+1)*sizeof(char*)))==NULL) {
				globfree(glob);
				return(GLOB_NOSPACE);
			}
			glob->gl_pathv=new_pathv;

			/* build the full pathname */
			SAFECOPY(path,pattern);
			p=getfname(path);
			*p=0;
			strcat(path,ff.name);

			if((glob->gl_pathv[glob->gl_pathc]=malloc(strlen(path)+2))==NULL) {
				globfree(glob);
				return(GLOB_NOSPACE);
			}
			strcpy(glob->gl_pathv[glob->gl_pathc],path);
			if(flags&GLOB_MARK && ff.attrib&_A_SUBDIR)
				strcat(glob->gl_pathv[glob->gl_pathc],"/");

			glob->gl_pathc++;
			found++;
		}
	} while(_dos_findnext(&ff)==0);
	_dos_findclose(&ff);

	if(found==0)
		return(GLOB_NOMATCH);

	if(!(flags&GLOB_NOSORT)) {
		qsort(glob->gl_pathv,found,sizeof(char*),glob_compare);
	}

	return(0);	/* success */
}

#else

int	glob(const char *pattern, int flags, void* unused, glob_t* glob)
{
    struct	_finddata_t ff;
	long	ff_handle;
	size_t	found=0;
	char	path[MAX_PATH+1];
	char*	p;
	char**	new_pathv;

	if(!(flags&GLOB_APPEND)) {
		glob->gl_pathc=0;
		glob->gl_pathv=NULL;
	}

	ff_handle=_findfirst((char*)pattern,&ff);
	while(ff_handle!=-1) {
		if((flags&GLOB_PERIOD || (ff.name[0]!='.' && !(ff.attrib&_A_HIDDEN))) &&
			(!(flags&GLOB_ONLYDIR) || ff.attrib&_A_SUBDIR)) {
			if((new_pathv=(char**)realloc(glob->gl_pathv
				,(glob->gl_pathc+1)*sizeof(char*)))==NULL) {
				globfree(glob);
				return(GLOB_NOSPACE);
			}
			glob->gl_pathv=new_pathv;

			/* build the full pathname */
			SAFECOPY(path,pattern);
			p=getfname(path);
			*p=0;
			SAFECAT(path,ff.name);

			if((glob->gl_pathv[glob->gl_pathc]=(char*)malloc(strlen(path)+2))==NULL) {
				globfree(glob);
				return(GLOB_NOSPACE);
			}
			strcpy(glob->gl_pathv[glob->gl_pathc],path);
			if(flags&GLOB_MARK && ff.attrib&_A_SUBDIR)
				strcat(glob->gl_pathv[glob->gl_pathc],"/");

			glob->gl_pathc++;
			found++;
		}
		if(_findnext(ff_handle, &ff)!=0) {
			_findclose(ff_handle);
			ff_handle=-1;
		}
	}

	if(found==0)
		return(GLOB_NOMATCH);

	if(!(flags&GLOB_NOSORT)) {
		qsort(glob->gl_pathv,found,sizeof(char*),glob_compare);
	}

	return(0);	/* success */
}

#endif

void globfree(glob_t* glob)
{
	size_t i;

	if(glob==NULL)
		return;

	if(glob->gl_pathv!=NULL) {
		for(i=0;i<glob->gl_pathc;i++)
			if(glob->gl_pathv[i]!=NULL)
				free(glob->gl_pathv[i]);

		free(glob->gl_pathv);
		glob->gl_pathv=NULL;
	}
	glob->gl_pathc=0;
}

#endif /* !defined(__unix__) */

/****************************************************************************/
/* Returns number of files and/or sub-directories in directory (path)		*/
/* Similar, but not identical, to getfilecount()							*/
/****************************************************************************/
long getdirsize(const char* path, BOOL include_subdirs, BOOL subdir_only)
{
	char		match[MAX_PATH+1];
	glob_t		g;
	unsigned	gi;
	long		count=0;

	if(!isdir(path))
		return -1;

	SAFECOPY(match,path);
	backslash(match);
	SAFECAT(match,ALLFILES);
	if (glob(match,GLOB_MARK,NULL,&g) != 0)
		return 0;
	if(include_subdirs && !subdir_only)
		count=g.gl_pathc;
	else
		for(gi=0;gi<g.gl_pathc;gi++) {
			if(*lastchar(g.gl_pathv[gi])=='/') {
				if(!include_subdirs)
					continue;
			} else
				if(subdir_only)
					continue;
			count++;
		}
	globfree(&g);
	return(count);
}

/****************************************************************************/
/* POSIX directory operations using Microsoft _findfirst/next API.			*/
/****************************************************************************/
#if defined(_MSC_VER) || defined(__DMC__)
DIR* opendir(const char* dirname)
{
	DIR*	dir;

	if((dir=(DIR*)calloc(1,sizeof(DIR)))==NULL) {
		errno=ENOMEM;
		return(NULL);
	}
	sprintf(dir->filespec,"%.*s",sizeof(dir->filespec)-5,dirname);
	if(*dir->filespec && dir->filespec[strlen(dir->filespec)-1]!='\\')
		strcat(dir->filespec,"\\");
	strcat(dir->filespec,"*.*");
	dir->handle=_findfirst(dir->filespec,&dir->finddata);
	if(dir->handle==-1) {
		errno=ENOENT;
		free(dir);
		return(NULL);
	}
	return(dir);
}
struct dirent* readdir(DIR* dir)
{
	if(dir==NULL)
		return(NULL);
	if(dir->end==TRUE)
		return(NULL);
	if(dir->handle==-1)
		return(NULL);
	sprintf(dir->dirent.d_name,"%.*s",sizeof(struct dirent)-1,dir->finddata.name);
	if(_findnext(dir->handle,&dir->finddata)!=0)
		dir->end=TRUE;
	return(&dir->dirent);
}
int closedir (DIR* dir)
{
	if(dir==NULL)
		return(-1);
	_findclose(dir->handle);
	free(dir);
	return(0);
}
void rewinddir(DIR* dir)
{
	if(dir==NULL)
		return;
	_findclose(dir->handle);
	dir->end=FALSE;
	dir->handle=_findfirst(dir->filespec,&dir->finddata);
}
#endif /* defined(_MSC_VER) */

/****************************************************************************/
/* Returns the creation time of the file 'filename' in time_t format		*/
/****************************************************************************/
time_t fcdate(const char* filename)
{
	struct stat st;

	if(stat(filename, &st) != 0)
		return -1;

	return st.st_ctime;
}

/****************************************************************************/
/* Returns the time/date of the file in 'filename' in time_t (unix) format  */
/****************************************************************************/
time_t fdate(const char* filename)
{
	struct stat st;

	if(stat(filename, &st)!=0)
		return(-1);

	return(st.st_mtime);
}

/****************************************************************************/
/* Change the access and modification times for specified filename			*/
/****************************************************************************/
int setfdate(const char* filename, time_t t)
{
	struct utimbuf ut;

	memset(&ut,0,sizeof(ut));

	ut.actime=t;
	ut.modtime=t;

	return(utime(filename,&ut));
}

/****************************************************************************/
/* Returns the length of the file in 'filename'                             */
/* or -1 if the file doesn't exist											*/
/****************************************************************************/
off_t flength(const char *filename)
{
#if defined(__BORLANDC__) && !defined(__unix__)	/* stat() doesn't work right */

	long	handle;
	struct _finddata_t f;

	if((handle=_findfirst((char*)filename,&f))==-1)
		return(-1);

 	_findclose(handle);

	return(f.size);

#else

	struct stat st;

	if(stat(filename, &st)!=0)
		return(-1);

	return(st.st_size);

#endif
}


/****************************************************************************/
/* Checks the file system for the existence of a file.						*/
/* Returns TRUE if it exists, FALSE if it doesn't.                          */
/* 'filename' may *NOT* contain wildcards!									*/
/****************************************************************************/
static BOOL fnameexist(const char *filename)
{
	struct stat st;

	if(stat(filename, &st) != 0)
		return FALSE;

	if(S_ISDIR(st.st_mode))
		return FALSE;

	return TRUE;
}

/****************************************************************************/
/* Checks the file system for the existence of one or more files.			*/
/* Returns TRUE if it exists, FALSE if it doesn't.                          */
/* 'filespec' may contain wildcards!										*/
/****************************************************************************/
BOOL fexist(const char *filespec)
{
#if defined(_WIN32)

	long	handle;
	struct _finddata_t f;
	BOOL	found;

	if(!strchr(filespec,'*') && !strchr(filespec,'?'))
		return(fnameexist(filespec));

	if((handle=_findfirst((char*)filespec,&f))==-1)
		return(FALSE);
	found=TRUE;
	while(f.attrib&_A_SUBDIR)
		if(_findnext(handle,&f)!=0) {
			found=FALSE;
			break;
		}

 	_findclose(handle);

	return(found);

#else /* Unix or OS/2 */

	/* portion by cmartin */

	glob_t g;
    int c;

	if(!strchr(filespec,'*') && !strchr(filespec,'?'))
		return(fnameexist(filespec));

    /* start the search */
    glob(filespec, GLOB_MARK | GLOB_NOSORT, NULL, &g);

    if (!g.gl_pathc) {
	    /* no results */
    	globfree(&g);
    	return FALSE;
    }

    /* make sure it's not a directory */
	c = g.gl_pathc;
    while (c--) {
    	if (*lastchar(g.gl_pathv[c]) != '/') {
        	globfree(&g);
            return TRUE;
        }
    }

    globfree(&g);
    return FALSE;

#endif
}

/****************************************************************************/
/* Fixes upper/lowercase filename for Unix file systems						*/
/****************************************************************************/
BOOL fexistcase(char *path)
{
#if defined(_WIN32)

	char*	fname;
	long	handle;
	struct _finddata_t f;

	if(access(path, F_OK)==-1 && !strchr(path,'*') && !strchr(path,'?'))
		return(FALSE);

	if((handle=_findfirst((char*)path,&f))==-1)
		return(FALSE);

 	_findclose(handle);

 	if(f.attrib&_A_SUBDIR)
		return(FALSE);

	fname=getfname(path);	/* Find filename in path */
	strcpy(fname,f.name);	/* Correct filename */

	return(TRUE);

#else /* Unix or OS/2 */

	char globme[MAX_PATH*4+1];
	char fname[MAX_PATH+1];
	char tmp[5];
	char *p;
	int  i;
	glob_t	glb;

	if(path[0]==0)		/* work around glibc bug 574274 */
		return FALSE;

	if(!strchr(path,'*') && !strchr(path,'?') && fnameexist(path))
		return(TRUE);

	SAFECOPY(globme,path);
	p=getfname(globme);
	SAFECOPY(fname,p);
	*p=0;
	for(i=0;fname[i];i++)  {
		if(IS_ALPHA(fname[i]))
			sprintf(tmp,"[%c%c]",toupper(fname[i]),tolower(fname[i]));
		else
			sprintf(tmp,"%c",fname[i]);
		strncat(globme,tmp,MAX_PATH*4);
	}
#if 0
	if(strcspn(path,"?*")!=strlen(path))  {
		sprintf(path,"%.*s",MAX_PATH,globme);
		return(fexist(path));
	}
#endif

	if(glob(globme,GLOB_MARK,NULL,&glb) != 0)
		return(FALSE);

	if(glb.gl_pathc>0)  {
		for(i=0;i<glb.gl_pathc;i++)  {
			if(*lastchar(glb.gl_pathv[i]) != '/')
				break;
		}
		if(i<glb.gl_pathc)  {
			sprintf(path,"%.*s",MAX_PATH,glb.gl_pathv[i]);
			globfree(&glb);
			return TRUE;
		}
	}

	globfree(&glb);
	return FALSE;

#endif
}

/****************************************************************************/
/* Returns TRUE if the filename specified is a directory					*/
/****************************************************************************/
BOOL isdir(const char *filename)
{
	char	path[MAX_PATH+1];
	char*	p;
	struct stat st;

	SAFECOPY(path,filename);

	p=lastchar(path);
	if(p!=path && IS_PATH_DELIM(*p)) {	/* chop off trailing slash */
#if !defined(__unix__)
		if(*(p-1)!=':')		/* Don't change C:\ to C: */
#endif
			*p=0;
	}

#if defined(__BORLANDC__) && !defined(__unix__)	/* stat() doesn't work right */
	if(stat(path, &st)!=0 || strchr(path,'*')!=NULL || strchr(path,'?')!=NULL)
#else
	if(stat(path, &st)!=0)
#endif
		return(FALSE);

	return(S_ISDIR(st.st_mode) ? TRUE : FALSE);
}

/****************************************************************************/
/* Returns the attributes (mode) for specified 'filename' or -1 on failure.	*/
/* The return value on Windows is *not* compatible with chmod().			*/
/****************************************************************************/
int getfattr(const char* filename)
{
#if defined(_WIN32)
	long handle;
	struct _finddata_t	finddata;

	if((handle=_findfirst((char*)filename,&finddata))==-1) {
		errno=ENOENT;
		return(-1);
	}
	_findclose(handle);
	return(finddata.attrib);
#else
	struct stat st;

	if(stat(filename, &st)!=0) {
		errno=ENOENT;
		return(-1L);
	}

	return(st.st_mode);
#endif
}

/****************************************************************************/
/* Returns the mode / type flags for specified 'filename'					*/
/* The return value *is* compatible with chmod(), or -1 upon failure.		*/
/****************************************************************************/
int getfmode(const char* filename)
{
	struct stat st;

	if(stat(filename, &st) != 0)
		return -1;

	return st.st_mode;
}


#ifdef __unix__
int removecase(const char *path)
{
	char inpath[MAX_PATH+1];
	char fname[MAX_PATH*4+1];
	char tmp[5];
	char *p;
	int  i;

	if(strchr(path,'?') || strchr(path,'*'))
		return(-1);
	SAFECOPY(inpath,path);
	p=getfname(inpath);
	fname[0]=0;
	for(i=0;p[i];i++)  {
		if(IS_ALPHA(p[i]))
			sprintf(tmp,"[%c%c]",toupper(p[i]),tolower(p[i]));
		else
			sprintf(tmp,"%c",p[i]);
		strncat(fname,tmp,MAX_PATH*4);
	}
	*p=0;

	return(delfiles(inpath,fname,0) >=1 ? 0 : -1);
}
#endif

/****************************************************************************/
/* Deletes all files in dir 'path' that match file spec 'spec'              */
/* If spec matches a sub-directory, it is traversed and removed recursively */			
/* Optionally, keep the last so many files (sorted by name)                 */
/* Returns number of files deleted or negative on error						*/
/****************************************************************************/
long delfiles(const char *inpath, const char *spec, size_t keep)
{
	char*	path;
	char*	fpath;
	char*	fname;
	char	lastch;
	size_t	i;
    ulong	files = 0;
	long	errors = 0;
	long	recursed;
	glob_t	g;
	size_t	inpath_len=strlen(inpath);
	int		flags =
#ifdef GLOB_PERIOD
		GLOB_PERIOD
#else
		0
#endif
	;

	if(inpath_len==0)
		lastch=0;
	else
		lastch=inpath[inpath_len-1];
	path=(char *)malloc(inpath_len+1/*Delim*/+strlen(spec)+1/*Terminator*/);
	if(path==NULL)
		return -1;
	if(!IS_PATH_DELIM(lastch) && lastch)
		sprintf(path,"%s%c",inpath,PATH_DELIM);
	else
		strcpy(path,inpath);
	strcat(path,spec);
	glob(path, flags, NULL, &g);
	free(path);
	if(keep >= g.gl_pathc)
		return 0;
	for(i = 0; i < g.gl_pathc && files < g.gl_pathc - keep; i++) {
		fpath = g.gl_pathv[i];
		if(isdir(fpath)) {
			fname = getfname(fpath);
			if(strcmp(fname, ".") == 0 || strcmp(fname, "..") == 0)
				continue;
			recursed = delfiles(fpath, spec, keep);
			if(recursed >= 0)
				files += recursed;
			else
				errors += recursed;
			if(rmdir(fpath) != 0)
				errors++;
			continue;
		}
		(void)CHMOD(fpath, S_IWRITE);	/* In case it's been marked RDONLY */
		if(remove(fpath)==0)
			files++;
		else
			errors++;
	}
	globfree(&g);
	if(errors)
		return -errors;
	return files;
}

/****************************************************************************/
/* Returns number of files matching 'inpath'								*/
/* Similar, but not identical, to getdirsize(), e.g. subdirs never counted	*/
/****************************************************************************/
ulong getfilecount(const char *inpath)
{
	char path[MAX_PATH+1];
	glob_t	g;
	uint	gi;
	ulong	count = 0;

	SAFECOPY(path, inpath);
	if(isdir(path))
		backslash(path);
	if(IS_PATH_DELIM(*lastchar(path)))
		SAFECAT(path, ALLFILES);
	if(glob(path, GLOB_MARK, NULL, &g))
		return 0;
	for(gi = 0; gi < g.gl_pathc; ++gi) {
		if(*lastchar(g.gl_pathv[gi]) == '/')
			continue;
		count++;
	}
	globfree(&g);
	return count;
}

/****************************************************************************/
/* Returns number of bytes used by file(s) matching 'inpath'				*/
/****************************************************************************/
uint64_t getfilesizetotal(const char *inpath)
{
	char path[MAX_PATH+1];
	glob_t	g;
	uint	gi;
	off_t	size;
	uint64_t total = 0;

	SAFECOPY(path, inpath);
	if(isdir(path))
		backslash(path);
	if(IS_PATH_DELIM(*lastchar(path)))
		SAFECAT(path, ALLFILES);
	if(glob(path, GLOB_MARK, NULL, &g))
		return 0;
	for(gi = 0; gi < g.gl_pathc; ++gi) {
		if(*lastchar(g.gl_pathv[gi]) == '/')
			continue;
		size = flength(g.gl_pathv[gi]);
		if(size >= 1)
			total += size;
	}
	globfree(&g);
	return total;
}

/****************************************************************************/
/* Return free disk space in bytes (up to a maximum of 4GB)					*/
/****************************************************************************/
#if defined(_WIN32)
typedef BOOL(WINAPI * GetDiskFreeSpaceEx_t)
	(LPCTSTR,PULARGE_INTEGER,PULARGE_INTEGER,PULARGE_INTEGER);

static int bit_num(ulong val)
{
	int i;

	for(i=31;i>=0;i--)
		if(val&(1<<i))
			return(i);

	return(-1);
}
#endif

/* Unit should be a power-of-2 (e.g. 1024 to report kilobytes) or 1 (to report bytes) */
static ulong getdiskspace(const char* path, ulong unit, BOOL freespace)
{
#if defined(_WIN32)
	char			root[16];
	DWORD			TotalNumberOfClusters;
	DWORD			NumberOfFreeClusters;
	DWORD			BytesPerSector;
	DWORD			SectorsPerCluster;
	ULARGE_INTEGER	avail;
	ULARGE_INTEGER	size;
	static HINSTANCE hK32;
	GetDiskFreeSpaceEx_t GetDiskFreeSpaceEx;

	if(hK32 == NULL)
		hK32 = LoadLibrary("KERNEL32");
	GetDiskFreeSpaceEx
		= (GetDiskFreeSpaceEx_t)GetProcAddress(hK32,"GetDiskFreeSpaceExA");

	if (GetDiskFreeSpaceEx!=NULL) {	/* Windows 95-OSR2 or later */
		if(!GetDiskFreeSpaceEx(
			path,		/* pointer to the directory name */
			&avail,		/* receives the number of bytes on disk avail to the caller */
			&size,		/* receives the number of bytes on disk */
			NULL))		/* receives the free bytes on disk */
			return(0);

		if(freespace)
			size=avail;

		if(unit>1)
			size.QuadPart=Int64ShrlMod32(size.QuadPart,bit_num(unit));

#if defined(_ANONYMOUS_STRUCT)
		if(size.HighPart)
#else
		if(size.u.HighPart)
#endif
			return(0xffffffff);	/* 4GB max */

#if defined(_ANONYMOUS_STRUCT)
		return(size.LowPart);
#else
		return(size.u.LowPart);
#endif
	}

	/* Windows 95 (old way), limited to 2GB */
	sprintf(root,"%.3s",path);
	if(!GetDiskFreeSpace(
		root,					/* pointer to root path */
		(PDWORD)&SectorsPerCluster,		/* pointer to sectors per cluster */
		(PDWORD)&BytesPerSector,		/* pointer to bytes per sector */
		(PDWORD)&NumberOfFreeClusters,	/* pointer to number of free clusters */
		(PDWORD)&TotalNumberOfClusters  /* pointer to total number of clusters */
		))
		return(0);

	if(freespace)
		TotalNumberOfClusters = NumberOfFreeClusters;
	if(unit>1)
		TotalNumberOfClusters/=unit;
	return(TotalNumberOfClusters*SectorsPerCluster*BytesPerSector);


#elif defined(__solaris__) || (defined(__NetBSD_Version__) && (__NetBSD_Version__ >= 300000000 /* NetBSD 3.0 */))

	struct statvfs fs;
	unsigned long blocks;

    if (statvfs(path, &fs) < 0)
    	return 0;

	if(freespace)
		blocks=fs.f_bavail;
	else
		blocks=fs.f_blocks;

	if(unit>1)
		blocks/=unit;
    return fs.f_bsize * blocks;

/* statfs is also used under FreeBSD (Though it *supports* statvfs() now too) */
#elif defined(__GLIBC__) || defined(BSD)

	struct statfs fs;
	unsigned long blocks;

	if(statfs(path, &fs) < 0)
    	return 0;

	if(freespace)
		blocks=fs.f_bavail;
	else
		blocks=fs.f_blocks;

	if(unit>1)
		blocks/=unit;
    return fs.f_bsize * blocks;

#else

	fprintf(stderr,"\n*** !Missing getfreediskspace implementation ***\n");
	return(0);

#endif
}

ulong getfreediskspace(const char* path, ulong unit)
{
	return getdiskspace(path, unit, /* freespace? */TRUE);
}

ulong getdisksize(const char* path, ulong unit)
{
	return getdiskspace(path, unit, /* freespace? */FALSE);
}

/****************************************************************************/
/* Resolves //, /./, and /../ in a path. Should work identically to Windows */
/****************************************************************************/
#if defined(__unix__)
char * _fullpath(char *target, const char *path, size_t size)  {
	char	*out;
	char	*p;
	BOOL	target_alloced=FALSE;

	if(target==NULL)  {
		if((target=malloc(MAX_PATH+1))==NULL) {
			return(NULL);
		}
		target_alloced=TRUE;
	}
	out=target;
	*out=0;

	if(*path != '/')  {
		if(*path == '~') {
			p=getenv("HOME");
			if(p==NULL || strlen(p)+strlen(path)>=size) {
				if(target_alloced)
					free(target);
				return(NULL);
			}
			strcpy(target,p);
			out=strrchr(target,'\0');
			path++;
		}
		else {
			p=getcwd(NULL,size);
			if(p==NULL || strlen(p)+strlen(path)>=size) {
				if(target_alloced)
					free(target);
				return(NULL);
			}
			strcpy(target,p);
			free(p);
			out=strrchr(target,'\0');
			*(out++)='/';
			*out=0;
			out--;
		}
	}
	strncat(target,path,size-1);

/*	if(stat(target,&sb))
		return(NULL);
	if(sb.st_mode&S_IFDIR)
		strcat(target,"/"); */

	for(;*out;out++) {
		while(*out=='/') {
			if(*(out+1)=='/')
				memmove(out,out+1,strlen(out));
			else if(*(out+1)=='.' && (*(out+2)=='/' || *(out+2)==0))
				memmove(out,out+2,strlen(out)-1);
			else if(*(out+1)=='.' && *(out+2)=='.' && (*(out+3)=='/' || *(out+3)==0))  {
				*out=0;
				p=strrchr(target,'/');
				if(p==NULL)
					p=target;
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
#endif

/****************************************************************************/
/* Adds a trailing slash/backslash (path delimiter) on path strings 		*/
/****************************************************************************/
char* backslash(char* path)
{
	char* p;

	p=lastchar(path);

	if(*p && !IS_PATH_DELIM(*p)) {
#if defined(__unix__)
		/* Convert trailing backslash to forwardslash on *nix */
		if(*p!='\\')
#endif
			p++;
		*p=PATH_DELIM;
		*(++p)=0;
	}
	return(path);
}

/****************************************************************************/
/* Returns true if the specified filename an absolute pathname				*/
/****************************************************************************/
BOOL isabspath(const char *filename)
{
	char path[MAX_PATH+1];

	return(stricmp(filename,FULLPATH(path,filename,sizeof(path)))==0);
}

/****************************************************************************/
/* Returns true if the specified filename is a full ("rooted") path			*/
/****************************************************************************/
BOOL isfullpath(const char* filename)
{
	return(filename[0]=='/'
#ifdef WIN32
		|| filename[0]=='\\' || (IS_ALPHA(filename[0]) && filename[1]==':')
#endif
		);
}

/****************************************************************************/
/* Matches file name against filespec										*/
/* Optionally not allowing * to match PATH_DELIM (for paths)				*/
/****************************************************************************/

BOOL wildmatch(const char *fname, const char *spec, BOOL path, BOOL case_sensitive)
{
	char *specp;
	char *fnamep;
	char *wildend;

	specp=(char *)spec;
	fnamep=(char *)fname;
	for(;;specp++, fnamep++) {
		switch(*specp) {
			case '?':
				if(!(*fnamep))
					return(FALSE);
				break;
			case 0:
				if(!*fnamep)
					return(TRUE);
				break;
			case '*':
				while(*specp=='*')
					specp++;
				if(path) {
					for(wildend=fnamep; *wildend; wildend++) {
						if(IS_PATH_DELIM(*wildend)) {
							wildend--;
							break;
						}
					}
				}
				else
					wildend=strchr(fnamep, 0);
				for(;wildend >= fnamep;wildend--) {
					if(wildmatch(wildend, specp, path, case_sensitive))
						return(TRUE);
				}
				return(FALSE);
			default:
				if(case_sensitive && *specp != *fnamep)
					return(FALSE);
				if((!case_sensitive) && toupper(*specp) != toupper(*fnamep))
					return(FALSE);
		}
		if(!(*specp && *fnamep))
			break;
	}
	while(*specp=='*')
		specp++;
	if(*specp==*fnamep)
		return(TRUE);
	if((!case_sensitive) && toupper(*specp) == toupper(*fnamep))
		return(TRUE);
	return(FALSE);
}

/****************************************************************************/
/* Matches file name against filespec, ignoring case						*/
/****************************************************************************/
BOOL wildmatchi(const char *fname, const char *spec, BOOL path)
{
	return wildmatch(fname, spec, path, /* case_sensitive: */FALSE);
}

/****************************************************************************/
/* Creates all the necessary directories in the specified path				*/
/****************************************************************************/
int mkpath(const char* path)
{
	const char*	p=path;
	const char*	tp;
	const char*	sep=
#ifdef _WIN32
		"\\"
#endif
		"/";
	char	dir[MAX_PATH+1];
	int		result=0;

#ifdef _WIN32
	if(p[1]==':')	/* Skip drive letter, if specified */
		p+=2;
#endif

	while(*p) {
		SKIP_CHARSET(p,sep);
		if(*p==0)
			break;
		tp=p;
		FIND_CHARSET(tp,sep);
		safe_snprintf(dir,sizeof(dir),"%.*s", (int)(tp-path), path);
		if(!isdir(dir)) {
			if((result=MKDIR(dir))!=0)
				break;
		}
		p=tp;
	}

	return(result);
}

#if !defined _WIN32
BOOL CopyFile(const char* src, const char* dest, BOOL failIfExists)
{
	uint8_t	buf[256 * 1024];
	FILE*	in;
	FILE*	out;
	BOOL	success=TRUE;

	if(failIfExists && fexist(dest))
		return FALSE;
	if((in=fopen(src,"rb"))==NULL)
		return FALSE;
	if((out=fopen(dest,"wb"))==NULL) {
		fclose(in);
		return FALSE;
	}

	time_t	ftime = filetime(fileno(in));
	while(!feof(in)) {
		size_t rd = fread(buf, sizeof(uint8_t), sizeof(buf), in);
		if(rd < 1)
			break;
		if(fwrite(buf, sizeof(uint8_t), rd, out) != rd) {
			success = FALSE;
			break;
		}
		MAYBE_YIELD();
	}

	fclose(in);
	fclose(out);
	setfdate(dest,ftime);

	return success;
}
#endif
