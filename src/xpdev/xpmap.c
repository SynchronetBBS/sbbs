/* xpmap.c */

/* mmap() style cross-platform development wrappers */

/* $Id: xpmap.c,v 1.8 2018/07/24 01:13:10 rswindell Exp $ */

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

#include "xpmap.h"
#include <stdlib.h>	// malloc()

#if defined(__unix__)

#include <unistd.h>	// close()
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

struct xpmapping* DLLCALL xpmap(const char *filename, enum xpmap_type type)
{
	int					fd;
	void				*addr=NULL;
	int					oflags;
	int					mflags;
	int					mprot;
	struct stat			sb;
	struct xpmapping	*ret;

	switch(type) {
		case XPMAP_READ:
			oflags=O_RDONLY;
			mflags=0;
			mprot=PROT_READ;
			break;
		case XPMAP_WRITE:
			oflags=O_RDWR;
			mflags=MAP_SHARED;
			mprot=PROT_READ|PROT_WRITE;
			break;
		case XPMAP_COPY:
			oflags=O_RDWR;
			mflags=MAP_PRIVATE;
			mprot=PROT_READ|PROT_WRITE;
			break;
	}

	fd=open(filename, oflags);
	if(fd == -1)
		return NULL;
	if(fstat(fd, &sb)==-1) {
		close(fd);
		return NULL;
	}
	addr=mmap(NULL, sb.st_size, mprot, mflags, fd, 0);
	if(addr==MAP_FAILED) {
		close(fd);
		return NULL;
	}
	ret=(struct xpmapping *)malloc(sizeof(struct xpmapping));
	if(ret==NULL) {
		munmap(addr, sb.st_size);
		close(fd);
		return NULL;
	}
	ret->addr=addr;
	ret->fd=fd;
	ret->size=sb.st_size;
	return ret;
}

void DLLCALL xpunmap(struct xpmapping *map)
{
	munmap(map->addr, map->size);
	close(map->fd);
	free(map);
}

#elif defined(_WIN32)

struct xpmapping* DLLCALL xpmap(const char *filename, enum xpmap_type type)
{
	HFILE				fd;
	HANDLE				md;
	OFSTRUCT			of;
	UINT				oflags;
	DWORD				mprot;
	DWORD				maccess;
	DWORD				size;
	void				*addr;
	struct xpmapping	*ret;

	switch(type) {
		case XPMAP_READ:
			oflags=OF_READ|OF_SHARE_DENY_NONE;
			mprot=PAGE_READONLY;
			maccess=FILE_MAP_READ;
			break;
		case XPMAP_WRITE:
			oflags=OF_READWRITE|OF_SHARE_DENY_NONE;
			mprot=PAGE_READWRITE;
			maccess=FILE_MAP_WRITE;
			break;
		case XPMAP_COPY:
			oflags=OF_READ|OF_SHARE_DENY_NONE;
			mprot=PAGE_WRITECOPY;
			maccess=FILE_MAP_COPY;
			break;
	}

	fd=OpenFile(filename, &of, oflags);
	if(fd == HFILE_ERROR)
		return NULL;
	if((size=GetFileSize((HANDLE)fd, NULL))==INVALID_FILE_SIZE)
		return NULL;
	md=CreateFileMapping((HANDLE)fd, NULL, mprot, 0, size, NULL);
	if(md==NULL)
		return NULL;
	addr=MapViewOfFile(md, maccess, 0, 0, size);
	ret=(struct xpmapping *)malloc(sizeof(struct xpmapping));
	if(ret==NULL)
		return NULL;
	ret->addr=addr;
	ret->fd=(HANDLE)fd;
	ret->md=md;
	ret->size=size;
	return ret;
}

void DLLCALL xpunmap(struct xpmapping *map)
{
	UnmapViewOfFile(map->addr);
	CloseHandle(map->md);
	CloseHandle(map->fd);
	free(map);
}

#else

	#error "Need mmap wrappers."

#endif
