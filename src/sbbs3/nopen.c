/* nopen.c */

/* Network open functions (nopen and fnopen) */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
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

#include "sbbs.h"
#include "crc32.h"

/****************************************************************************/
/* Network open function. Opens all files DENYALL and retries LOOP_NOPEN    */
/* number of times if the attempted file is already open or denying access  */
/* for some other reason.	All files are opened in BINARY mode.			*/
/****************************************************************************/
int nopen(char *str, int access)
{
	int file,share,count=0;

    if(access&O_DENYNONE) {
        share=SH_DENYNO;
        access&=~O_DENYNONE; }
    else if(access==O_RDONLY) share=SH_DENYWR;
    else share=SH_DENYRW;
	if(!(access&O_TEXT))
		access|=O_BINARY;
    while(((file=sopen(str,access,share))==-1)
        && (errno==EACCES || errno==EAGAIN) && count++<LOOP_NOPEN)
        if(count)
            mswait(100);
    return(file);
}
/****************************************************************************/
/* This function performs an nopen, but returns a file stream with a buffer */
/* allocated.																*/
/****************************************************************************/
FILE* fnopen(int *fd, char *str, int access)
{
	char	mode[128];
	int		file;
	FILE *	stream;

    if((file=nopen(str,access))==-1)
        return(NULL);

    if(fd!=NULL)
        *fd=file;

    if(access&O_APPEND) {
        if(access&O_RDWR==O_RDWR)
            strcpy(mode,"a+");
        else
            strcpy(mode,"a"); 
	} else if(access&(O_TRUNC|O_WRONLY)) {
		if(access&O_RDWR==O_RDWR)
			strcpy(mode,"w+");
		else
			strcpy(mode,"w");
	} else {
        if(access&O_RDWR==O_RDWR)
            strcpy(mode,"r+");
        else
            strcpy(mode,"r"); 
	}
    stream=fdopen(file,mode);
    if(stream==NULL) {
        close(file);
        return(NULL); 
	}
    setvbuf(stream,NULL,_IOFBF,FNOPEN_BUF_SIZE);
    return(stream);
}
