/* file.cpp */

/* Synchronet file transfer-related functions */

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

/****************************************************************************/
/* Prints all information of file in file_t structure 'f'					*/
/****************************************************************************/
void sbbs_t::fileinfo(file_t* f)
{
	char	fname[13],ext[513];
	char 	tmp[512];
	char	path[MAX_PATH+1];
	uint	i,j;
#ifdef _WIN32
	char*	p;
	char	lfn[MAX_PATH+1];
	DWORD	lfn_len;
#endif

	for(i=0;i<usrlibs;i++)
		if(usrlib[i]==cfg.dir[f->dir]->lib)
			break;
	for(j=0;j<usrdirs[i];j++)
		if(usrdir[i][j]==f->dir)
			break;

	unpadfname(f->name,fname);

	sprintf(path,"%s%s",f->altpath>0 && f->altpath<=cfg.altpaths 
			? cfg.altpath[f->altpath-1]:cfg.dir[f->dir]->path
			,fname);
	bprintf(text[FiLib],i+1,cfg.lib[cfg.dir[f->dir]->lib]->lname);
	bprintf(text[FiDir],j+1,cfg.dir[f->dir]->lname);
	bprintf(text[FiFilename],fname);
#ifdef _WIN32
	if(f->size!=-1 && Win98GetLongPathName!=NULL) {	/* Windows 98/2K or later */

		lfn_len=Win98GetLongPathName(path,lfn,sizeof(lfn));

		if(lfn_len!=0 && lfn_len!=strlen(path)) {
			p=strrchr(lfn,'\\');
			if(p!=NULL)
				bprintf(text[FiFilename],p+1);
		}
	}
#endif

	if(f->size!=-1L)
		bprintf(text[FiFileSize],ultoac(f->size,tmp));
	bprintf(text[FiCredits]
		,(cfg.dir[f->dir]->misc&DIR_FREE || !f->cdt) ? "FREE" : ultoac(f->cdt,tmp));
	bprintf(text[FiDescription],f->desc);
	bprintf(text[FiUploadedBy],f->misc&FM_ANON ? text[UNKNOWN_USER] : f->uler);
	if(f->date)
		bprintf(text[FiFileDate],timestr(&f->date));
	bprintf(text[FiDateUled],timestr(&f->dateuled));
	bprintf(text[FiDateDled],f->datedled ? timestr(&f->datedled) : "Never");
	bprintf(text[FiTimesDled],f->timesdled);
	if(f->size!=-1L)
		bprintf(text[FiTransferTime],sectostr(f->timetodl,tmp));
	if(f->altpath) {
		if(f->altpath<=cfg.altpaths) {
			if(SYSOP)
				bprintf(text[FiAlternatePath],cfg.altpath[f->altpath-1]); }
		else
			bprintf(text[InvalidAlternatePathN],f->altpath); }
	CRLF;
	if(f->misc&FM_EXTDESC) {
		getextdesc(&cfg,f->dir,f->datoffset,ext);
		CRLF;
		putmsg(ext,P_NOATCODES);
		CRLF; }
	if(f->size==-1L)
		bprintf(text[FileIsNotOnline],f->name);
	if(f->opencount)
		bprintf(text[FileIsOpen],f->opencount,f->opencount>1 ? "s" : nulstr);

}


/****************************************************************************/
/* Increments the opencount on the file data 'f' and adds the transaction 	*/
/* to the backout.dab														*/
/****************************************************************************/
void sbbs_t::openfile(file_t* f)
{
	char str1[256],str2[4],str3[4],ch;
	int file;

	/************************************/
	/* Increment open count in dat file */
	/************************************/
	sprintf(str1,"%s%s.dat",cfg.dir[f->dir]->data_dir,cfg.dir[f->dir]->code);
	if((file=nopen(str1,O_RDWR))==-1) {
		errormsg(WHERE,ERR_OPEN,str1,O_RDWR);
		return; }
	lseek(file,f->datoffset+F_OPENCOUNT,SEEK_SET);
	if(read(file,str2,3)!=3) {
		close(file);
		errormsg(WHERE,ERR_READ,str1,3);
		return; }
	str2[3]=0;
	ultoa(atoi(str2)+1,str3,10);
	putrec(str2,0,3,str3);
	lseek(file,f->datoffset+F_OPENCOUNT,SEEK_SET);
	if(write(file,str2,3)!=3) {
		close(file);
		errormsg(WHERE,ERR_WRITE,str1,3);
		return; }
	close(file);
	/**********************************/
	/* Add transaction to BACKOUT.DAB */
	/**********************************/
	sprintf(str1,"%sbackout.dab",cfg.node_dir);
	if((file=nopen(str1,O_WRONLY|O_APPEND|O_CREAT))==-1) {
		errormsg(WHERE,ERR_OPEN,str1,O_WRONLY|O_APPEND|O_CREAT);
		return; }
	ch=BO_OPENFILE;
	write(file,&ch,1);				/* backout type */
	write(file,cfg.dir[f->dir]->code,8); /* directory code */
	write(file,&f->datoffset,4);		/* offset into .dat file */
	write(file,&ch,BO_LEN-(1+8+4)); /* pad it */
	close(file);
}

/****************************************************************************/
/* Decrements the opencount on the file data 'f' and removes the backout  	*/
/* from the backout.dab														*/
/****************************************************************************/
void sbbs_t::closefile(file_t* f)
{
	char	str1[256],str2[4],str3[4],ch,*buf;
	int		file;
	long	length,l,offset;

	/************************************/
	/* Decrement open count in dat file */
	/************************************/
	sprintf(str1,"%s%s.dat",cfg.dir[f->dir]->data_dir,cfg.dir[f->dir]->code);
	if((file=nopen(str1,O_RDWR))==-1) {
		errormsg(WHERE,ERR_OPEN,str1,O_RDWR);
		return; }
	lseek(file,f->datoffset+F_OPENCOUNT,SEEK_SET);
	if(read(file,str2,3)!=3) {
		close(file);
		errormsg(WHERE,ERR_READ,str1,3);
		return; }
	str2[3]=0;
	ch=atoi(str2);
	if(ch) ch--;
	ultoa(ch,str3,10);
	putrec(str2,0,3,str3);
	lseek(file,f->datoffset+F_OPENCOUNT,SEEK_SET);
	if(write(file,str2,3)!=3) {
		close(file);
		errormsg(WHERE,ERR_WRITE,str1,3);
		return; }
	close(file);
	/*****************************************/
	/* Removing transaction from BACKOUT.DAB */
	/*****************************************/
	sprintf(str1,"%sbackout.dab",cfg.node_dir);
	if(flength(str1)<1L)	/* file is not there or empty */
		return;
	if((file=nopen(str1,O_RDONLY))==-1) {
		errormsg(WHERE,ERR_OPEN,str1,O_RDONLY);
		return; }
	length=filelength(file);
	if((buf=(char *)MALLOC(length))==NULL) {
		close(file);
		errormsg(WHERE,ERR_ALLOC,str1,length);
		return; }
	if(read(file,buf,length)!=length) {
		close(file);
		FREE(buf);
		errormsg(WHERE,ERR_READ,str1,length);
		return; }
	close(file);
	if((file=nopen(str1,O_WRONLY|O_TRUNC))==-1) {
		errormsg(WHERE,ERR_OPEN,str1,O_WRONLY|O_TRUNC);
		return; }
	ch=0;								/* 'ch' is a 'file already removed' flag */
	for(l=0;l<length;l+=BO_LEN) {       /* in case file is in backout.dab > 1 */
		if(!ch && buf[l]==BO_OPENFILE) {
			memcpy(str1,buf+l+1,8);
			str1[8]=0;
			memcpy(&offset,buf+l+9,4);
			if(!stricmp(str1,cfg.dir[f->dir]->code) && offset==f->datoffset) {
				ch=1;
				continue; } }
		write(file,buf+l,BO_LEN); }
	FREE(buf);
	close(file);
}

/****************************************************************************/
/* Prompts user for file specification. <CR> is *.* and .* is assumed.      */
/* Returns padded file specification.                                       */
/* Returns NULL if input was aborted.                                       */
/****************************************************************************/
char * sbbs_t::getfilespec(char *str)
{
	bputs(text[FileSpecStarDotStar]);
	if(!getstr(str,12,K_UPPER))
		strcpy(str,ALLFILES);
#if 0
	else if(!strchr(str,'.') && strlen(str)<=8)
		strcat(str,".*");
#endif
	if(sys_status&SS_ABORT)
		return(0);
	return(str);
}

/****************************************************************************/
/* Checks to see if filename matches filespec. Returns 1 if yes, 0 if no    */
/****************************************************************************/
extern "C" BOOL filematch(char *filename, char *filespec)
{
    char c;

	for(c=0;c<8;c++) /* Handle Name */
		if(filespec[c]=='*') break;
		else if(filespec[c]=='?') continue;
		else if(toupper(filename[c])!=toupper(filespec[c])) return(FALSE);
	if(filespec[8]==' ')	/* no extension specified */
		return(TRUE);
	for(c=9;c<12;c++)
		if(filespec[c]=='*') break;
		else if(filespec[c]=='?') continue;
		else if(toupper(filename[c])!=toupper(filespec[c])) return(FALSE);
	return(TRUE);
}

/*****************************************************************************/
/* Checks the filename 'fname' for invalid symbol or character sequences     */
/*****************************************************************************/
bool sbbs_t::checkfname(char *fname)
{
	char 	str[256];
    int		c=0,d;

	if(strcspn(fname,ILLEGAL_FILENAME_CHARS)!=strlen(fname)) {
		sprintf(str,"Suspicious filename attempt: '%s'",fname);
		errorlog(str);
		return(false); }
	if(strstr(fname,".."))
		return(false);
#if 0	/* long file name support */
	if(strcspn(fname,".")>8)
		return(false);
#endif
	d=strlen(fname);
	while(c<d) {
		if(fname[c]<=SP || fname[c]&0x80)
			return(false);
		c++; }
	return(true);
}
