/* con_hi.cpp */

/* Synchronet hi-level console routines */

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
/* Returns 1 if a is a valid ctrl-a code, 0 if it isn't.                    */
/****************************************************************************/
bool sbbs_t::validattr(char a)
{

	switch(toupper(a)) {
		case '-':   /* clear        */
		case '_':   /* clear        */
		case 'B':   /* blue     fg  */
		case 'C':   /* cyan     fg  */
		case 'G':   /* green    fg  */
		case 'H':   /* high     fg  */
		case 'I':   /* blink        */
		case 'K':   /* black    fg  */
		case 'L':   /* cls          */
		case 'M':   /* magenta  fg  */
		case 'N':   /* normal       */
		case 'P':   /* pause        */
		case 'R':   /* red      fg  */
		case 'W':   /* white    fg  */
		case 'Y':   /* yellow   fg  */
		case '0':   /* black    bg  */
		case '1':   /* red      bg  */
		case '2':   /* green    bg  */
		case '3':   /* brown    bg  */
		case '4':   /* blue     bg  */
		case '5':   /* magenta  bg  */
		case '6':   /* cyan     bg  */
		case '7':   /* white    bg  */
			return(true); }
	return(false);
}

/****************************************************************************/
/* Strips invalid Ctrl-Ax sequences from str                                */
/* Returns number of ^A's in line                                           */
/****************************************************************************/
int sbbs_t::stripattr(char *strin)
{
    char str[256];
    uint a,c,d,e;

	e=strlen(strin);
	for(a=c=d=0;c<e && d<sizeof(str)-1;c++) {
		if(strin[c]==CTRL_A) {
			a++;
			if(!validattr(strin[c+1])) {
				c++;
				continue; } }
		str[d++]=strin[c]; }
	str[d]=0;
	strcpy(strin,str);
	return(a);
}

/****************************************************************************/
/* Redraws str using i as current cursor position and l as length           */
/****************************************************************************/
void sbbs_t::redrwstr(char *strin, int i, int l, long mode)
{
    char str[256],c;

	sprintf(str,"%-*.*s",l,l,strin);
	c=i;
	while(c--)
		outchar(BS);
	if(mode&K_MSG)
		bputs(str);
	else
		rputs(str);
	if(useron.misc&ANSI) {
		bputs("\x1b[K");
		if(i<l)
			bprintf("\x1b[%dD",l-i); }
	else {
		while(c<79) { /* clear to end of line */
			outchar(SP);
			c++; }
		while(c>l) { /* back space to end of string */
			outchar(BS);
			c--; } }
}


int sbbs_t::uselect(int add, uint n, char *title, char *item, uchar *ar)
{
	char	str[128];
	int		i;
	uint	t,u;

	if(uselect_total>=sizeof(uselect_num)/sizeof(uselect_num[0]))	/* out of bounds */
		uselect_total=0;

	if(add) {
		if(ar && !chk_ar(ar,&useron))
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
	if(online==ON_LOCAL) {
		if(!(cfg.sys_misc&SM_L_SYSOP))
			return(false);
		if(!(cfg.node_misc&NM_SYSPW) && !(cfg.sys_misc&SM_REQ_PW))
			return(false); 
	}
	bputs("SY: ");
	console&=~(CON_R_ECHO|CON_L_ECHO);
	getstr(str,40,K_UPPER);
	console=orgcon;
	CRLF;
	if(strcmp(cfg.sys_pass,str)) {
		if(cfg.sys_misc&SM_ECHO_PW) 
			sprintf(str2,"%s #%u System password attempt: '%s'"
				,useron.alias,useron.number,str);
		else
			sprintf(str2,"%s #%u System password verification failure"
				,useron.alias,useron.number);
		logline("S!",str2);
		return(false); 
	}
	return(true);
}
