/* dirwrap.c */

/* Directory-related system-call wrappers */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2002 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
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
	#if defined(__FreeBSD__)
		#include <sys/kbio.h>
	#endif
	#endif

	#include <sys/ioctl.h>	/* ioctl */

	#if defined(__GLIBC__)		/* actually, BSD, but will work for now */
		#include <sys/vfs.h>    /* statfs() */
	#endif

#endif /* __unix__ */

#if defined(__WATCOMC__)
	#include <dos.h>
#endif
	
#include <sys/types.h>	/* _dev_t */
#include <sys/stat.h>	/* struct stat */

#include <stdio.h>		/* sprintf */
#include <stdlib.h>		/* rand */
#include <errno.h>		/* ENOENT definitions */

#include "genwrap.h"	/* strupr/strlwr */
#include "dirwrap.h"	/* DLLCALL */

/****************************************************************************/
/* Return the filename portion of a full pathname							*/
/****************************************************************************/
char* DLLCALL getfname(const char* path)
{
	char *fname;

	fname=strrchr(path,'/');
	if(fname==NULL) 
		fname=strrchr(path,'\\');
	if(fname!=NULL) 
		fname++;
	else 
		fname=(char*)path;
	return(fname);
}


/****************************************************************************/
/* Break a path name into components.										*/
/****************************************************************************/
#if defined(__unix__)
void DLLCALL _splitpath(const char *path, char *drive, char *dir, char *fname, char *ext)
{
	char*	p;

	ext[0]=0;
	drive[0]=0;	/* no drive letters on Unix */

	strcpy(dir,path);
	p=strrchr(dir,'/');
	if(p==NULL)
		p=strrchr(dir,'\\');
	if(p==NULL) {
		p=(char*)path;
		dir[0]=0;
	} else {
		*p=0;	/* truncate dir */
		p++;
	}
	strcpy(fname,p);
	p=strrchr(fname,'.');
	if(p!=NULL) {
		*p=0;
		strcpy(ext,p+1);
	}
}
#endif

/****************************************************************************/
/* Win32 (minimal) implementation of POSIX.2 glob() function				*/
/* This code _may_ work on other DOS-based platforms (e.g. OS/2)			*/
/****************************************************************************/
#if !defined(__unix__)
static int glob_compare( const void *arg1, const void *arg2 )
{
   /* Compare all of both strings: */
   return stricmp( * ( char** ) arg1, * ( char** ) arg2 );
}

#if defined(__BORLANDC__)
	#pragma argsused
#endif

#if defined(__WATCOMC__)

int	DLLCALL	glob(const char *pattern, int flags, void* unused, glob_t* glob)
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

	if(_dos_findfirst((char*)pattern,_A_NORMAL,&ff)!=0)
		return(GLOB_NOMATCH);

	do {
		if(!(flags&GLOB_ONLYDIR) || ff.attrib&_A_SUBDIR) {
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

int	DLLCALL	glob(const char *pattern, int flags, void* unused, glob_t* glob)
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
		if(!(flags&GLOB_ONLYDIR) || ff.attrib&_A_SUBDIR) {
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

void DLLCALL globfree(glob_t* glob)
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
/* POSIX directory operations using Microsoft _findfirst/next API.			*/
/****************************************************************************/
#if defined(_MSC_VER)
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
/* Returns the time/date of the file in 'filename' in time_t (unix) format  */
/****************************************************************************/
time_t DLLCALL fdate(const char *filename)
{
	struct stat st;

	if(access(filename,0)==-1)
		return(-1);

	if(stat(filename, &st)!=0)
		return(-1);

	return(st.st_mtime);
}

/****************************************************************************/
/* Returns the length of the file in 'filename'                             */
/****************************************************************************/
long DLLCALL flength(const char *filename)
{
#if defined(__BORLANDC__) && !defined(__unix__)	/* stat() doesn't work right */

	long	handle;
	struct _finddata_t f;

	if(access((char*)filename,0)==-1)
		return(-1L);

	if((handle=_findfirst((char*)filename,&f))==-1)
		return(-1);

 	_findclose(handle);

	return(f.size);

#else 

	struct stat st;

	if(access(filename,0)==-1)
		return(-1L);

	if(stat(filename, &st)!=0)
		return(-1L);

	return(st.st_size);

#endif
}

/****************************************************************************/
/* Checks the file system for the existence of one or more files.			*/
/* Returns TRUE if it exists, FALSE if it doesn't.                          */
/* 'filespec' may contain wildcards!										*/
/****************************************************************************/
BOOL DLLCALL fexist(const char *filespec)
{
#if defined(_WIN32)

	long	handle;
	struct _finddata_t f;

	if(access(filespec,0)==-1 && !strchr(filespec,'*') && !strchr(filespec,'?'))
		return(FALSE);

	if((handle=_findfirst((char*)filespec,&f))==-1)
		return(FALSE);

 	_findclose(handle);

 	if(f.attrib&_A_SUBDIR)
		return(FALSE);

	return(TRUE);

#else /* Unix or OS/2 */
	
	/* portion by cmartin */

	glob_t g;
    int c;

	if(access(filespec,0)==-1 && !strchr(filespec,'*') && !strchr(filespec,'?'))
		return(FALSE);

    // start the search
    glob(filespec, GLOB_MARK | GLOB_NOSORT, NULL, &g);

    if (!g.gl_pathc) {
	    // no results
    	globfree(&g);
    	return FALSE;
    }

    // make sure it's not a directory
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
BOOL DLLCALL fexistcase(char *path)
{
#if defined(__unix__)
	char globme[MAX_PATH*4+1];
	char fname[MAX_PATH+1];
	char tmp[5];
	char *p;
	int  i;
	glob_t	glb;
	
	SAFECOPY(globme,path);
	p=getfname(globme);
	SAFECOPY(fname,p);
	*p=0;
	for(i=0;fname[i];i++)  {
		if(isalpha(fname[i]))
			sprintf(tmp,"[%c%c]",toupper(fname[i]),tolower(fname[i]));
		else
			sprintf(tmp,"%c",fname[i]);
		strncat(globme,tmp,MAX_PATH*4);
	}
	if(strcspn(path,"?*")!=strlen(path))  {
		sprintf(path,"%.*s",MAX_PATH,globme);
		return(fexist(path));
	}

	if(glob(globme,GLOB_MARK,NULL,&glb) != 0)
		return(FALSE);
	
	if(glb.gl_pathc>0)  {
		/**********************************************************
		 * If multiple matches are found, return TRUE only if one *
		 * EXACTLY matches path or all but one are directories	  *
		 **********************************************************/
		for(i=0;i<glb.gl_pathc;i++)  {
			if(strcmp(path,glb.gl_pathv[i])==0)  {
				globfree(&glb);
				return TRUE;
			}
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
	
#else
	return(fexist(path));
#endif
}

/****************************************************************************/
/* Returns TRUE if the filename specified is a directory					*/
/****************************************************************************/
BOOL DLLCALL isdir(const char *filename)
{
	struct stat st;

	if(stat(filename, &st)!=0)
		return(FALSE);

	return((st.st_mode&S_IFDIR) ? TRUE : FALSE);
}

/****************************************************************************/
/* Returns the attributes (mode) for specified 'filename'					*/
/****************************************************************************/
int DLLCALL getfattr(const char* filename)
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
/* Deletes all files in dir 'path' that match file spec 'spec'              */
/****************************************************************************/
ulong DLLCALL delfiles(char *inpath, char *spec)
{
	char	path[MAX_PATH+1];
	char	lastch;
    uint	i,files=0;
	glob_t	g;

	lastch=*lastchar(inpath);
	if(lastch!='/' && lastch!='\\')
		sprintf(path,"%s%c",inpath,BACKSLASH);
	else
		strcpy(path,inpath);
	strcat(path,spec);
	glob(path,0,NULL,&g);
	for(i=0;i<g.gl_pathc;i++) {
		if(isdir(g.gl_pathv[i]))
			continue;
		CHMOD(g.gl_pathv[i],S_IWRITE);	// Incase it's been marked RDONLY
		if(remove(g.gl_pathv[i])==0)
			files++;
	}
	globfree(&g);
	return(files);
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

/* Unit should be a power-of-2 (e.g. 1024 to report kilobytes) */
ulong DLLCALL getfreediskspace(const char* path, ulong unit)
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
			path,		// pointer to the directory name
			&avail,		// receives the number of bytes on disk avail to the caller
			&size,		// receives the number of bytes on disk
			NULL))		// receives the free bytes on disk
			return(0);

		if(unit>1)
			avail.QuadPart=Int64ShrlMod32(avail.QuadPart,bit_num(unit));

#if defined(_ANONYMOUS_STRUCT)
		if(avail.HighPart)
#else
		if(avail.u.HighPart)
#endif
			return(0xffffffff);	/* 4GB max */

#if defined(_ANONYMOUS_STRUCT)
		return(avail.LowPart);
#else
		return(avail.u.LowPart);
#endif
	}

	/* Windows 95 (old way), limited to 2GB */
	sprintf(root,"%.3s",path);
	if(!GetDiskFreeSpace(
		root,					// pointer to root path
		&SectorsPerCluster,		// pointer to sectors per cluster
		&BytesPerSector,		// pointer to bytes per sector
		&NumberOfFreeClusters,	// pointer to number of free clusters
		&TotalNumberOfClusters  // pointer to total number of clusters
		))
		return(0);

	if(unit>1)
		NumberOfFreeClusters/=unit;
	return(NumberOfFreeClusters*SectorsPerCluster*BytesPerSector);


/* statfs is also used under FreeBSD */
#elif defined(__GLIBC__) || defined(BSD)

	struct statfs fs;

    if (statfs(path, &fs) < 0)
    	return 0;

	if(unit>1)
		fs.f_bavail/=unit;
    return fs.f_bsize * fs.f_bavail;
    
#else

	fprintf(stderr,"\n*** !Missing getfreediskspace implementation ***\n");
	return(0);

#endif
}
