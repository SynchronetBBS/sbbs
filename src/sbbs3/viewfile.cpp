/* viewfile.cpp */

/* Synchronet file contents display routines */

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
/* Views file with:                                                         */
/* (B)atch download, (V)iew file (E)xtended info, (Q)uit, or [Next]:        */
/* call with ext=1 for default to extended info, or 0 for file view         */
/* Returns -1 for Batch, 1 for Next, or 0 for Quit                          */
/****************************************************************************/
int sbbs_t::viewfile(file_t* f, int ext)
{
	char	ch,str[256];
	char 	tmp[512];

	curdirnum=f->dir;	/* for ARS */
	while(online) {
		if(ext)
			fileinfo(f);
		else
			viewfilecontents(f);
		ASYNC;
		CRLF;
		sprintf(str,text[FileInfoPrompt],unpadfname(f->name,tmp));
		mnemonics(str);
		ch=(char)getkeys("BEVQ\r",0);
		if(ch=='Q' || sys_status&SS_ABORT)
			return(0);
		switch(ch) {
			case 'B':
				addtobatdl(f);
				CRLF;
				return(-1);
			case 'E':
				ext=1;
				continue;
			case 'V':
				ext=0;
				continue;
			case CR:
				return(1); } }
	return(0);
}

/*****************************************************************************/
/* View viewable file types from dir 'dirnum'                                */
/* 'fspec' must be padded                                                    */
/*****************************************************************************/
void sbbs_t::viewfiles(uint dirnum, char *fspec)
{
    char	viewcmd[256];
	char 	tmp[512];
    int		i;

	curdirnum=dirnum;	/* for ARS */
	sprintf(viewcmd,"%s%s",cfg.dir[dirnum]->path,fspec);
	if(!fexist(viewcmd)) {
		bputs(text[FileNotFound]);
		return; }
	padfname(fspec,tmp);
	truncsp(tmp);
	for(i=0;i<cfg.total_fviews;i++)
		if(!stricmp(tmp+9,cfg.fview[i]->ext) && chk_ar(cfg.fview[i]->ar,&useron)) {
			strcpy(viewcmd,cfg.fview[i]->cmd);
			break; }
	if(i==cfg.total_fviews) {
		bprintf(text[NonviewableFile],tmp+9);
		return; }
	sprintf(tmp,"%s%s",cfg.dir[dirnum]->path,fspec);
	if((i=external(cmdstr(viewcmd,tmp,tmp,NULL),EX_OUTL|EX_OUTR|EX_INR|EX_SH))!=0)
		errormsg(WHERE,ERR_EXEC,viewcmd,i);    /* must of EX_SH to ^C */
}

/****************************************************************************/
/****************************************************************************/
void sbbs_t::viewfilecontents(file_t* f)
{
	char	str[128],cmd[128];
	char 	tmp[512];
	int		i;

	if(f->size<=0L) {
		bputs(text[FileNotThere]);
		return; }

	sprintf(str,"%s%s",f->altpath > 0 && f->altpath<=cfg.altpaths
		? cfg.altpath[f->altpath-1] : cfg.dir[f->dir]->path
		,unpadfname(f->name,tmp));
	strcpy(tmp,f->name);
	truncsp(tmp);
	for(i=0;i<cfg.total_fviews;i++) {
		if(!stricmp(tmp+9,cfg.fview[i]->ext)
			&& chk_ar(cfg.fview[i]->ar,&useron)) {
			strcpy(cmd,cfg.fview[i]->cmd);
			break; } }
	if(i==cfg.total_fviews)
		bprintf(text[NonviewableFile],tmp+9);
	else
		if((i=external(cmdstr(cmd,str,str,NULL)
			,EX_OUTL|EX_OUTR|EX_INR))!=0)
			errormsg(WHERE,ERR_EXEC,cmdstr(cmd,str,str,NULL),i);
}
