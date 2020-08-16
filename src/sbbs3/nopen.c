/* nopen.c */

/* Network open functions (nopen and fnopen) and friends */

/* $Id: nopen.c,v 1.30 2018/11/23 17:08:42 rswindell Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
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
/* Network open function. Opens all files DENYALL, DENYWRITE, or DENYNONE	*/
/* depending on access, and retries LOOP_NOPEN number of times if the		*/
/* attempted file is already open or denying access  for some other reason. */
/* All files are opened in BINARY mode.										*/
/****************************************************************************/
int nopen(const char* str, int access)
{
	int file,share,count=0;

    if(access&O_DENYNONE) {
        share=SH_DENYNO;
        access&=~O_DENYNONE; 
	} 
	else if((access&~(O_TEXT|O_BINARY))==O_RDONLY) 
		share=SH_DENYWR;
    else 
		share=SH_DENYRW;

#if !defined(__unix__)	/* Basically, a no-op on Unix anyway */
	if(!(access&O_TEXT))
		access|=O_BINARY;
#endif
    while(((file=sopen(str,access,share,DEFFILEMODE))==-1)
        && (errno==EACCES || errno==EAGAIN || errno==EDEADLOCK) && count++<LOOP_NOPEN)
        if(count)
            mswait(100);
    return(file);
}

/****************************************************************************/
/* This function performs an nopen, but returns a file stream with a buffer */
/* allocated.																*/
/****************************************************************************/
FILE* fnopen(int* fd, const char* str, int access)
{
	char*	mode;
	int		file;
	FILE *	stream;

    if((file=nopen(str,access))==-1)
        return(NULL);

    if(fd!=NULL)
        *fd=file;

    if(access&O_APPEND) {
        if((access&O_RDWR)==O_RDWR)
            mode="a+";
        else
            mode="a"; 
	} else if(access&(O_TRUNC|O_WRONLY)) {
		if((access&O_RDWR)==O_RDWR)
			mode="w+";
		else
			mode="w";
	} else {
        if((access&O_RDWR)==O_RDWR)
            mode="r+";
        else
            mode="r"; 
	}
    stream=fdopen(file,mode);
    if(stream==NULL) {
        close(file);
        return(NULL); 
	}
    setvbuf(stream,NULL,_IOFBF,FNOPEN_BUF_SIZE);
    return(stream);
}

BOOL ftouch(const char* fname)
{
	int file;
	struct utimbuf ut;

	/* update the time stamp */
	ut.actime = ut.modtime = time(NULL);
	if(utime(fname, &ut)==0)
		return TRUE;

	/* create the file */
	if((file=nopen(fname,O_WRONLY|O_CREAT))<0)
		return FALSE;
	close(file);
	return TRUE;
}

BOOL fmutex(const char* fname, const char* text, long max_age)
{
	int file;
	time_t t;
#if !defined(NO_SOCKET_SUPPORT)
	char hostname[128];
	if(text==NULL && gethostname(hostname,sizeof(hostname))==0)
		text=hostname;
#endif

	if(max_age && (t=fdate(fname)) >= 0 && (time(NULL)-t) > max_age) {
		if(remove(fname)!=0)
			return FALSE;
	}
	if((file=open(fname,O_CREAT|O_WRONLY|O_EXCL,DEFFILEMODE))<0)
		return FALSE;
	if(text!=NULL)
		write(file,text,strlen(text));
	close(file);
	return TRUE;
}

BOOL fcopy(const char* src, const char* dest)
{
	int		ch;
	ulong	count=0;
	FILE*	in;
	FILE*	out;
	BOOL	success=TRUE;

	if((in=fopen(src,"rb"))==NULL)
		return FALSE;
	if((out=fopen(dest,"wb"))==NULL) {
		fclose(in);
		return FALSE;
	}

	while(!feof(in)) {
		if((ch=fgetc(in))==EOF)
			break;
		if(fputc(ch,out)==EOF) {
			success=FALSE;
			break;
		}
		if(((count++)%(32*1024))==0)
			MAYBE_YIELD();
	}

	fclose(in);
	fclose(out);

	return(success);
}

BOOL fcompare(const char* fn1, const char* fn2)
{
	FILE*	fp1;
	FILE*	fp2;
	BOOL	success=TRUE;

	if(flength(fn1) != flength(fn2))
		return FALSE;
	if((fp1=fopen(fn1,"rb"))==NULL)
		return FALSE;
	if((fp2=fopen(fn2,"rb"))==NULL) {
		fclose(fp1);
		return FALSE;
	}

	while(!feof(fp1) && success) {
		if(fgetc(fp1) != fgetc(fp2))
			success=FALSE;
	}

	fclose(fp1);
	fclose(fp2);

	return(success);
}


/****************************************************************************/
/****************************************************************************/
BOOL backup(const char *fname, int backup_level, BOOL ren)
{
	char	oldname[MAX_PATH+1];
	char	newname[MAX_PATH+1];
	char*	ext;
	int		i;
	int		len;

	if(flength(fname) < 1)	/* no need to backup a 0-byte (or non-existent) file */
		return FALSE;

	if((ext=strrchr(fname,'.'))==NULL)
		ext="";

	len=strlen(fname)-strlen(ext);

	for(i=backup_level;i;i--) {
		safe_snprintf(newname,sizeof(newname),"%.*s.%d%s",len,fname,i-1,ext);
		if(i==backup_level)
			if(fexist(newname) && remove(newname)!=0)
				return FALSE;
		if(i==1) {
			if(ren == TRUE) {
				if(rename(fname,newname)!=0)
					return FALSE;
			} else {
				struct utimbuf ut;

				/* preserve the original time stamp */
				ut.modtime = fdate(fname);

				if(!fcopy(fname,newname))
					return FALSE;

				ut.actime = time(NULL);
				utime(newname, &ut);
			}
			continue; 
		}
		safe_snprintf(oldname,sizeof(oldname),"%.*s.%d%s",len,fname,i-2,ext);
		if(fexist(oldname) && rename(oldname,newname)!=0)
			return FALSE;
	}

	return TRUE;
}
