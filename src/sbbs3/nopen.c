/* Network open functions (nopen and fnopen) and friends */

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
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "genwrap.h"
#include "dirwrap.h"
#include "filewrap.h"
#include "sockwrap.h"
#include "nopen.h"

/****************************************************************************/
/* Network open function. Opens all files DENYALL, DENYWRITE, or DENYNONE	*/
/* depending on access, and retries LOOP_NOPEN number of times if the		*/
/* attempted file is already open or denying access  for some other reason. */
/* All files are opened in BINARY mode.										*/
/****************************************************************************/
int nopen(const char* str, uint access)
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
            SLEEP(100);
    return(file);
}

/****************************************************************************/
/* This function performs an nopen, but returns a file stream with a buffer */
/* allocated.																*/
/****************************************************************************/
FILE* fnopen(int* fd, const char* str, uint access)
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

bool ftouch(const char* fname)
{
	int file;

	/* update the time stamp */
	if(utime(fname, NULL)==0)
		return true;

	/* create the file */
	if((file=nopen(fname,O_WRONLY|O_CREAT))<0)
		return false;
	close(file);
	return true;
}

bool fmutex(const char* fname, const char* text, long max_age)
{
	int file;
	time_t t;
	bool result = true;
#if !defined(NO_SOCKET_SUPPORT)
	char hostname[128];
	if(text==NULL && gethostname(hostname,sizeof(hostname))==0)
		text=hostname;
#endif

	if(max_age && (t=fdate(fname)) >= 0 && (time(NULL)-t) > max_age) {
		if(remove(fname)!=0)
			return false;
	}
	if((file=open(fname,O_CREAT|O_WRONLY|O_EXCL,DEFFILEMODE))<0)
		return false;
	if(text!=NULL)
		result = write(file,text,strlen(text)) >= 0;
	close(file);
	return result;
}

bool fcompare(const char* fn1, const char* fn2)
{
	FILE*	fp1;
	FILE*	fp2;
	bool	success=true;

	if(flength(fn1) != flength(fn2))
		return false;
	if((fp1=fopen(fn1,"rb"))==NULL)
		return false;
	if((fp2=fopen(fn2,"rb"))==NULL) {
		fclose(fp1);
		return false;
	}

	while(!feof(fp1) && success) {
		if(fgetc(fp1) != fgetc(fp2))
			success=false;
	}

	fclose(fp1);
	fclose(fp2);

	return(success);
}


/****************************************************************************/
/****************************************************************************/
bool backup(const char *fname, int backup_level, bool ren)
{
	char	oldname[MAX_PATH+1];
	char	newname[MAX_PATH+1];
	char*	ext;
	int		i;
	int		len;

	if(flength(fname) < 1)	/* no need to backup a 0-byte (or non-existent) file */
		return false;

	if((ext=strrchr(fname,'.'))==NULL)
		ext="";

	len=strlen(fname)-strlen(ext);

	for(i=backup_level;i;i--) {
		safe_snprintf(newname,sizeof(newname),"%.*s.%d%s",len,fname,i-1,ext);
		if(i==backup_level)
			if(fexist(newname) && remove(newname)!=0)
				return false;
		if(i==1) {
			if(ren == true) {
				if(rename(fname,newname)!=0)
					return false;
			} else {
				struct utimbuf ut;

				/* preserve the original time stamp */
				ut.modtime = fdate(fname);

				if(!CopyFile(fname, newname, /* failIfExists: */false))
					return false;

				ut.actime = time(NULL);
				utime(newname, &ut);
			}
			continue; 
		}
		safe_snprintf(oldname,sizeof(oldname),"%.*s.%d%s",len,fname,i-2,ext);
		if(fexist(oldname) && rename(oldname,newname)!=0)
			return false;
	}

	return true;
}
