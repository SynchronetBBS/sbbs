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
/* Fills the 'ptr' element of the each element of the cfg.sub[] array of sub_t  */
/* and the sub_cfg and sub_ptr global variables                            */
/* Called from function main                                                */
/****************************************************************************/
void sbbs_t::getmsgptrs()
{
	if(!useron.number)
		return;
	bputs(text[LoadingMsgPtrs]);
	::getmsgptrs(&cfg,useron.number,subscan);
	bputs(text[LoadedMsgPtrs]);
}

extern "C" BOOL DLLCALL getmsgptrs(scfg_t* cfg, uint usernumber, subscan_t* subscan)
{
	char	str[256];
	uint	i;
	int 	file;
	long	length;
	FILE	*stream;

	if(!usernumber)
		return(FALSE);
	sprintf(str,"%suser/ptrs/%4.4u.ixb", cfg->data_dir,usernumber);
	if((stream=fnopen(&file,str,O_RDONLY))==NULL) {
		for(i=0;i<cfg->total_subs;i++) {
			subscan[i].ptr=subscan[i].sav_ptr=0;
			subscan[i].last=subscan[i].sav_last=0;
			subscan[i].cfg=0;
			if(cfg->sub[i]->misc&SUB_NSDEF)
				subscan[i].cfg|=SUB_CFG_NSCAN;
			if(cfg->sub[i]->misc&SUB_SSDEF)
				subscan[i].cfg|=SUB_CFG_SSCAN;
			subscan[i].sav_cfg=subscan[i].cfg; 
		}
		return(TRUE); 
	}
	length=filelength(file);
	for(i=0;i<cfg->total_subs;i++) {
		if(length<(cfg->sub[i]->ptridx+1)*10L) {
			subscan[i].ptr=subscan[i].last=0L;
			subscan[i].cfg=0;
			if(cfg->sub[i]->misc&SUB_NSDEF)
				subscan[i].cfg|=SUB_CFG_NSCAN;
			if(cfg->sub[i]->misc&SUB_SSDEF)
				subscan[i].cfg|=SUB_CFG_SSCAN; 
		}
		else {
			fseek(stream,(long)cfg->sub[i]->ptridx*10L,SEEK_SET);
			fread(&subscan[i].ptr,sizeof(long),1,stream);
			fread(&subscan[i].last,sizeof(long),1,stream);
			fread(&subscan[i].cfg,sizeof(short),1,stream);
		}
		subscan[i].sav_ptr=subscan[i].ptr;
		subscan[i].sav_last=subscan[i].last;
		subscan[i].sav_cfg=subscan[i].cfg; 
	}
	fclose(stream);
	return(TRUE);
}

void sbbs_t::putmsgptrs()
{
	::putmsgptrs(&cfg,useron.number,subscan);
}

/****************************************************************************/
/* Writes to DATA\USER\PTRS\xxxx.DAB the msgptr array for the current user	*/
/* Called from functions main and newuser                                   */
/****************************************************************************/
extern "C" BOOL DLLCALL putmsgptrs(scfg_t* cfg, uint usernumber, subscan_t* subscan)
{
	char	str[256];
	ushort	idx,ch;
	uint	i,j;
	int 	file;
	ulong	l=0L,length;

	if(!usernumber)
		return(FALSE);
	sprintf(str,"%suser/ptrs/%4.4u.ixb", cfg->data_dir,usernumber);
	if((file=nopen(str,O_WRONLY|O_CREAT))==-1) {
		return(FALSE); 
	}
	length=filelength(file);
	for(i=0;i<cfg->total_subs;i++) {
		if(subscan[i].sav_ptr==subscan[i].ptr 
			&& subscan[i].sav_last==subscan[i].last
			&& length>=((cfg->sub[i]->ptridx+1)*10UL)
			&& subscan[i].sav_cfg==subscan[i].cfg)
			continue;
		while(filelength(file)<(long)(cfg->sub[i]->ptridx)*10) {
			lseek(file,0L,SEEK_END);
			idx=(ushort)(tell(file)/10);
			for(j=0;j<cfg->total_subs;j++)
				if(cfg->sub[j]->ptridx==idx)
					break;
			write(file,&l,sizeof(long));
			write(file,&l,sizeof(long));
			ch=0xff;					/* default to scan ON for new sub */
			if(j<cfg->total_subs) {
				if(!(cfg->sub[j]->misc&SUB_NSDEF))
					ch&=~SUB_CFG_NSCAN;
				if(!(cfg->sub[j]->misc&SUB_SSDEF))
					ch&=~SUB_CFG_SSCAN; 
			}
			write(file,&ch,sizeof(short)); 
		}
		lseek(file,(long)((long)(cfg->sub[i]->ptridx)*10),SEEK_SET);
		write(file,&(subscan[i].ptr),sizeof(long));
		write(file,&(subscan[i].last),sizeof(long));
		write(file,&(subscan[i].cfg),sizeof(short));
	}
	close(file);
	if(!flength(str))				/* Don't leave 0 byte files */
		remove(str);

	return(TRUE);
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


