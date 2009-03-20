/* con_hi.cpp */

/* Synchronet hi-level console routines */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2009 Rob Swindell - http://www.synchro.net/copyright.html		*
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
/* Redraws str using i as current cursor position and l as length           */
/* Currently only used by getstr() - so should be moved to getstr.cpp?		*/
/****************************************************************************/
void sbbs_t::redrwstr(char *strin, int i, int l, long mode)
{
	cursor_left(i);
	if(mode&K_MSG)
		bprintf("%-*.*s",l,l,strin);
	else
		column+=rprintf("%-*.*s",l,l,strin);
	cleartoeol();
	if(i<l)
		cursor_left(l-i); 
}

int sbbs_t::uselect(int add, uint n, const char *title, const char *item, const uchar *ar)
{
	char	str[128];
	int		i;
	uint	t,u;

	if(uselect_total>=sizeof(uselect_num)/sizeof(uselect_num[0]))	/* out of bounds */
		uselect_total=0;

	if(add) {
		if(ar && !chk_ar(ar,&useron,&client))
			return(0);
		if(!uselect_total)
			bprintf(text[SelectItemHdr],title);
		uselect_num[uselect_total++]=n;
		bprintf(text[SelectItemFmt],uselect_total,item);
		return(0); }

	if(!uselect_total)
		return(-1);

	for(u=0;u<uselect_total;u++)
		if(uselect_num[u]==n)
			break;
	if(u==uselect_total)
		u=0;
	sprintf(str,text[SelectItemWhich],u+1);
	mnemonics(str);
	i=getnum(uselect_total);
	t=uselect_total;
	uselect_total=0;
	if(i<0)
		return(-1);
	if(!i) {					/* User hit ENTER, use default */
		for(u=0;u<t;u++)
			if(uselect_num[u]==n)
				return(uselect_num[u]);
		if(n<t)
			return(uselect_num[n]);
		return(-1); }
	return(uselect_num[i-1]);
}

/****************************************************************************/
/* Prompts user for System Password. Returns 1 if user entered correct PW	*/
/****************************************************************************/
bool sbbs_t::chksyspass()
{
	char	str[256],str2[256];
	int 	orgcon=console;

	if(online==ON_REMOTE && !(cfg.sys_misc&SM_R_SYSOP)) {
		logline("S!","Remote sysop access disabled");
		return(false);
	}
	bputs(text[SystemPassword]);
	getstr(str,40,K_UPPER|K_NOECHO);
	CRLF;
	if(strcmp(cfg.sys_pass,str)) {
		if(cfg.sys_misc&SM_ECHO_PW) 
			SAFEPRINTF3(str2,"%s #%u System password attempt: '%s'"
				,useron.alias,useron.number,str);
		else
			SAFEPRINTF2(str2,"%s #%u System password verification failure"
				,useron.alias,useron.number);
		logline("S!",str2);
		return(false); 
	}
	return(true);
}
