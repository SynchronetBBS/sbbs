/* data_ovl.cpp */

/* Synchronet hi-level data access routines */

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
/* Puts 'name' into slot 'number' in user/name.dat							*/
/****************************************************************************/
void sbbs_t::putusername(int number, char *name)
{
	char str[256];
	int file;
	long length;

	if (number<1) {
		errormsg(WHERE,ERR_CHK,"user number",number);
		return; }

	sprintf(str,"%suser/name.dat", cfg.data_dir);
	if((file=nopen(str,O_RDWR|O_CREAT))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_RDWR|O_CREAT);
		return; }
	length=filelength(file);
	if(length && length%(LEN_ALIAS+2)) {
		close(file);
		errormsg(WHERE,ERR_LEN,str,length);
		return; }
	if(length<(((long)number-1)*(LEN_ALIAS+2))) {
		sprintf(str,"%*s",LEN_ALIAS,nulstr);
		memset(str,ETX,LEN_ALIAS);
		strcat(str,crlf);
		lseek(file,0L,SEEK_END);
		while(filelength(file)<((long)number*(LEN_ALIAS+2)))
			write(file,str,(LEN_ALIAS+2)); }
	lseek(file,(long)(((long)number-1)*(LEN_ALIAS+2)),SEEK_SET);
	putrec(str,0,LEN_ALIAS,name);
	putrec(str,LEN_ALIAS,2,crlf);
	write(file,str,LEN_ALIAS+2);
	close(file);
}

/****************************************************************************/
/* Fills the 'ptr' element of the each element of the cfg.sub[] array of sub_t  */
/* and the sub_cfg and sub_ptr global variables                            */
/* Called from function main                                                */
/****************************************************************************/
void sbbs_t::getmsgptrs()
{
	char	str[256];
	uint	i;
	int 	file;
	long	length;
	FILE	*stream;

	now=time(NULL);
	if(!useron.number)
		return;
	bputs(text[LoadingMsgPtrs]);
	sprintf(str,"%suser/ptrs/%4.4u.ixb", cfg.data_dir,useron.number);
	if((stream=fnopen(&file,str,O_RDONLY))==NULL) {
		for(i=0;i<cfg.total_subs;i++) {
			sub_ptr[i]=sav_sub_ptr[i]=0;
			sub_last[i]=sav_sub_last[i]=0;
			sub_cfg[i]=0;
			if(cfg.sub[i]->misc&SUB_NSDEF)
				sub_cfg[i]|=SUB_CFG_NSCAN;
			if(cfg.sub[i]->misc&SUB_SSDEF)
				sub_cfg[i]|=SUB_CFG_SSCAN;
			sav_sub_cfg[i]=sub_cfg[i]; 
		}
		bputs(text[LoadedMsgPtrs]);
		return; }
	length=filelength(file);
	for(i=0;i<cfg.total_subs;i++) {
		if(length<(cfg.sub[i]->ptridx+1)*10L) {
			sub_ptr[i]=sub_last[i]=0L;
			sub_cfg[i]=0;
			if(cfg.sub[i]->misc&SUB_NSDEF)
				sub_cfg[i]|=SUB_CFG_NSCAN;
			if(cfg.sub[i]->misc&SUB_SSDEF)
				sub_cfg[i]|=SUB_CFG_SSCAN; 
		}
		else {
			fseek(stream,(long)cfg.sub[i]->ptridx*10L,SEEK_SET);
			fread(&sub_ptr[i],4,1,stream);
			fread(&sub_last[i],4,1,stream);
			fread(&sub_cfg[i],2,1,stream);
		}
		sav_sub_ptr[i]=sub_ptr[i];
		sav_sub_last[i]=sub_last[i];
		sav_sub_cfg[i]=sub_cfg[i]; 
	}
	fclose(stream);
	bputs(text[LoadedMsgPtrs]);
}

/****************************************************************************/
/* Writes to DATA\USER\PTRS\xxxx.DAB the msgptr array for the current user	*/
/* Called from functions main and newuser                                   */
/****************************************************************************/
void sbbs_t::putmsgptrs()
{
	char	str[256];
	ushort	idx,ch;
	uint	i,j;
	int 	file;
	ulong	l=0L,length;

	if(!useron.number)
		return;
	sprintf(str,"%suser/ptrs/%4.4u.ixb", cfg.data_dir,useron.number);
	if((file=nopen(str,O_WRONLY|O_CREAT))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT);
		return; }
	length=filelength(file);
	for(i=0;i<cfg.total_subs;i++) {
		if(sav_sub_ptr[i]==sub_ptr[i] && sav_sub_last[i]==sub_last[i]
			&& length>=((cfg.sub[i]->ptridx+1)*10UL)
			&& sav_sub_cfg[i]==sub_cfg[i])
			continue;
		while(filelength(file)<(long)(cfg.sub[i]->ptridx)*10) {
			lseek(file,0L,SEEK_END);
			idx=tell(file)/10;
			for(j=0;j<cfg.total_subs;j++)
				if(cfg.sub[j]->ptridx==idx)
					break;
			write(file,&l,4);
			write(file,&l,4);
			ch=0xff;					/* default to scan ON for new sub */
			if(j<cfg.total_subs) {
				if(!(cfg.sub[j]->misc&SUB_NSDEF))
					ch&=~SUB_CFG_NSCAN;
				if(!(cfg.sub[j]->misc&SUB_SSDEF))
					ch&=~SUB_CFG_SSCAN; 
			}
			write(file,&ch,2); 
		}
		lseek(file,(long)((long)(cfg.sub[i]->ptridx)*10),SEEK_SET);
		write(file,&(sub_ptr[i]),4);
		write(file,&(sub_last[i]),4);
		write(file,&(sub_cfg[i]),2);
	}
	close(file);
	if(!flength(str))				/* Don't leave 0 byte files */
		remove(str);
}

/****************************************************************************/
/* Checks for a duplicate user field starting at user record offset         */
/* 'offset', reading in 'datlen' chars, comparing to 'str' for each user    */
/* except 'usernumber' if it is non-zero. Comparison is NOT case sensitive. */
/* 'del' is true if the search is to include deleted/inactive users			*/
/* Returns the usernumber of the dupe if found, 0 if not                    */
/****************************************************************************/
uint sbbs_t::userdatdupe(uint usernumber, uint offset, uint datlen, char *dat
    ,bool del)
{
	bputs(text[SearchingForDupes]);
	uint i=::userdatdupe(&cfg, usernumber, offset, datlen, dat, del);
	bputs(text[SearchedForDupes]);
	return(i);
}


