/* sortdir.cpp */

/* Synchronet file database sorting routines */

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
/* Re-sorts file directory 'dirnum' according to dir[dirnum]->sort type     */
/****************************************************************************/
void sbbs_t::resort(uint dirnum)
{
	char	str[25],ixbfname[128],datfname[128],exbfname[128],txbfname[128]
			,ext[512],nulbuf[512];
	char 	tmp[512];
	uchar*	ixbbuf, *datbuf;
	uchar*	ixbptr[MAX_FILES];
	int		ixbfile,datfile,exbfile,txbfile,i,j;
	long	ixblen,datlen,offset,newoffset,l;

	memset(nulbuf,0,512);
	bprintf(text[ResortLineFmt],cfg.lib[cfg.dir[dirnum]->lib]->sname,cfg.dir[dirnum]->sname);
	sprintf(ixbfname,"%s%s.ixb",cfg.dir[dirnum]->data_dir,cfg.dir[dirnum]->code);
	sprintf(datfname,"%s%s.dat",cfg.dir[dirnum]->data_dir,cfg.dir[dirnum]->code);
	sprintf(exbfname,"%s%s.exb",cfg.dir[dirnum]->data_dir,cfg.dir[dirnum]->code);
	sprintf(txbfname,"%s%s.txb",cfg.dir[dirnum]->data_dir,cfg.dir[dirnum]->code);

	if(flength(ixbfname)<1L || flength(datfname)<1L) {
		remove(exbfname);
		remove(txbfname);
		remove(ixbfname);
		remove(datfname);
		bputs(text[ResortEmptyDir]);
		return; }
	bputs(text[Sorting]);
	if((ixbfile=nopen(ixbfname,O_RDONLY))==-1) {
		errormsg(WHERE,ERR_OPEN,ixbfname,O_RDONLY);
		return; }
	if((datfile=nopen(datfname,O_RDONLY))==-1) {
		close(ixbfile);
		errormsg(WHERE,ERR_OPEN,datfname,O_RDONLY);
		return; }
	ixblen=filelength(ixbfile);
	datlen=filelength(datfile);
	if((ixbbuf=(uchar *)MALLOC(ixblen))==NULL) {
		close(ixbfile);
		close(datfile);
		errormsg(WHERE,ERR_ALLOC,ixbfname,ixblen);
		return; }
	if((datbuf=(uchar *)MALLOC(datlen))==NULL) {
		close(ixbfile);
		close(datfile);
		FREE((char *)ixbbuf);
		errormsg(WHERE,ERR_ALLOC,datfname,datlen);
		return; }
	if(lread(ixbfile,ixbbuf,ixblen)!=ixblen) {
		close(ixbfile);
		close(datfile);
		FREE((char *)ixbbuf);
		FREE((char *)datbuf);
		errormsg(WHERE,ERR_READ,ixbfname,ixblen);
		return; }
	if(lread(datfile,datbuf,datlen)!=datlen) {
		close(ixbfile);
		close(datfile);
		FREE((char *)ixbbuf);
		FREE((char *)datbuf);
		errormsg(WHERE,ERR_READ,datfname,datlen);
		return; }
	close(ixbfile);
	close(datfile);
	if((ixbfile=nopen(ixbfname,O_WRONLY|O_TRUNC))==-1) {
		FREE((char *)ixbbuf);
		FREE((char *)datbuf);
		errormsg(WHERE,ERR_OPEN,ixbfname,O_WRONLY|O_TRUNC);
		return; }
	if((datfile=nopen(datfname,O_WRONLY|O_TRUNC))==-1) {
		close(ixbfile);
		FREE((char *)ixbbuf);
		FREE((char *)datbuf);
		errormsg(WHERE,ERR_OPEN,datfname,O_WRONLY|O_TRUNC);
		return; }
	for(l=0,i=0;l<ixblen && i<MAX_FILES;l+=F_IXBSIZE,i++)
		ixbptr[i]=ixbbuf+l;
	switch(cfg.dir[dirnum]->sort) {
		case SORT_NAME_A:
			qsort((void *)ixbptr,ixblen/F_IXBSIZE,sizeof(ixbptr[0])
				,(int(*)(const void*, const void*))fnamecmp_a);
			break;
		case SORT_NAME_D:
			qsort((void *)ixbptr,ixblen/F_IXBSIZE,sizeof(ixbptr[0])
				,(int(*)(const void*, const void*))fnamecmp_d);
			break;
		case SORT_DATE_A:
			qsort((void *)ixbptr,ixblen/F_IXBSIZE,sizeof(ixbptr[0])
				,(int(*)(const void*, const void*))fdatecmp_a);
			break;
		case SORT_DATE_D:
			qsort((void *)ixbptr,ixblen/F_IXBSIZE,sizeof(ixbptr[0])
				,(int(*)(const void*, const void*))fdatecmp_d);
			break; }

	if((exbfile=nopen(exbfname,O_RDWR|O_CREAT))==-1) {
		close(ixbfile);
		close(datfile);
		FREE((char *)ixbbuf);
		FREE((char *)datbuf);
		errormsg(WHERE,ERR_OPEN,exbfname,O_RDWR|O_CREAT);
		return; }
	if((txbfile=nopen(txbfname,O_RDWR|O_CREAT))==-1) {
		close(exbfile);
		close(datfile);
		close(exbfile);
		FREE((char *)ixbbuf);
		FREE((char *)datbuf);
		errormsg(WHERE,ERR_OPEN,txbfname,O_RDWR|O_CREAT);
		return; }

	for(i=0;i<ixblen/F_IXBSIZE;i++) {
		offset=ixbptr[i][11]|((long)ixbptr[i][12]<<8)|((long)ixbptr[i][13]<<16);
		lwrite(datfile,&datbuf[offset],F_LEN);

		newoffset=(ulong)i*(ulong)F_LEN;

		j=datbuf[offset+F_MISC];  /* misc bits */
		if(j!=ETX) j-=SP;
		if(j&FM_EXTDESC) { /* extended description */
			lseek(exbfile,(offset/F_LEN)*512L,SEEK_SET);
			memset(ext,0,512);
			read(exbfile,ext,512);
			while(filelength(txbfile)<(newoffset/F_LEN)*512L) {
	//			  lseek(txbfile,0L,SEEK_END);
				write(txbfile,nulbuf,512); }
			lseek(txbfile,(newoffset/F_LEN)*512L,SEEK_SET);
			write(txbfile,ext,512); }

		str[0]=newoffset&0xff;	   /* Get offset within DAT file for IXB file */
		str[1]=(newoffset>>8)&0xff;
		str[2]=(newoffset>>16)&0xff;
		lwrite(ixbfile,ixbptr[i],11);       /* filename */
		lwrite(ixbfile,str,3);              /* offset */
		lwrite(ixbfile,ixbptr[i]+14,8); }   /* upload and download times */
	close(exbfile);
	close(txbfile);
	close(ixbfile);
	close(datfile);
	remove(exbfname);
	rename(txbfname,exbfname);
	if(!flength(exbfname))
		remove(exbfname);
	FREE((char *)ixbbuf);
	FREE((char *)datbuf);
	if(ixblen/F_IXBSIZE==datlen/F_LEN)
		bputs(text[Sorted]);
	else
		bprintf(text[Compressed]
			,(uint)((datlen/F_LEN)-(ixblen/F_IXBSIZE))
			,ultoac(((datlen/F_LEN)-(ixblen/F_IXBSIZE))*F_LEN,tmp));
}

/****************************************************************************/
/* Compares filenames for ascending name sort								*/
/****************************************************************************/
int fnamecmp_a(char **str1, char **str2)
{
	return(strnicmp(*str1,*str2,11));
}

/****************************************************************************/
/* Compares filenames for descending name sort								*/
/****************************************************************************/
int fnamecmp_d(char **str1, char **str2)
{
	return(strnicmp(*str2,*str1,11));
}

/****************************************************************************/
/* Compares file upload dates for ascending date sort						*/
/****************************************************************************/
int fdatecmp_a(uchar **buf1, uchar **buf2)
{
	time_t date1,date2;

	date1=((*buf1)[14]|((long)(*buf1)[15]<<8)|((long)(*buf1)[16]<<16)
		|((long)(*buf1)[17]<<24));
	date2=((*buf2)[14]|((long)(*buf2)[15]<<8)|((long)(*buf2)[16]<<16)
		|((long)(*buf2)[17]<<24));
	if(date1>date2)	return(1);
	if(date1<date2)	return(-1);
	return(0);
}

/****************************************************************************/
/* Compares file upload dates for descending date sort						*/
/****************************************************************************/
int fdatecmp_d(uchar **buf1, uchar **buf2)
{
	time_t date1,date2;

	date1=((*buf1)[14]|((long)(*buf1)[15]<<8)|((long)(*buf1)[16]<<16)
		|((long)(*buf1)[17]<<24));
	date2=((*buf2)[14]|((long)(*buf2)[15]<<8)|((long)(*buf2)[16]<<16)
		|((long)(*buf2)[17]<<24));
	if(date1>date2)	return(-1);
	if(date1<date2)	return(1);
	return(0);
}
