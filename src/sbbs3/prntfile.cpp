/* prntfile.cpp */

/* Synchronet file print/display routines */

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
/* Prints a file remotely and locally, interpreting ^A sequences, checks    */
/* for pauses, aborts and ANSI. 'str' is the path of the file to print      */
/* Called from functions menu and text_sec                                  */
/****************************************************************************/
void sbbs_t::printfile(char *str, long mode)
{
	char* buf;
	char* p;
	int file,wip=0,rip=0;
	long length,savcon=console;
	FILE *stream;

	p=strrchr(str,'.');
	if(p!=NULL) {
		if(stricmp(p,".wip")==0) {
			wip=1;
			mode|=P_NOPAUSE;
		}
		else if(stricmp(p,".rip")==0) {
			rip=1;
			mode|=P_NOPAUSE;
		}
		else if(stricmp(p,".html")==0)  {
			html=1;
			mode|=(P_HTML|P_NOPAUSE);
		}
	}

	if(mode&P_NOABORT || wip || rip || html) {
		if(online==ON_REMOTE && console&CON_R_ECHO) {
			rioctl(IOCM|ABORT);
			rioctl(IOCS|ABORT); 
		}
		sys_status&=~SS_ABORT; 
	}

	if(!tos && !wip && !rip && !html)
		CRLF;

	if((stream=fnopen(&file,str,O_RDONLY))==NULL) {
		lprintf("File not found: %s",str);
		bputs(text[FileNotFound]);
		if(SYSOP) bputs(str);
		CRLF;
		return; 
	}

	if(mode&P_OPENCLOSE) {
		length=filelength(file);
		if((buf=(char*)MALLOC(length+1L))==NULL) {
			close(file);
			console=savcon;
			errormsg(WHERE,ERR_ALLOC,str,length+1L);
			return; 
		}
		buf[lread(file,buf,length)]=0;
		fclose(stream);
		putmsg(buf,mode);
		FREE((char *)buf); 
	}
	else {
		putmsg_fp(stream,filelength(file),mode);
		fclose(stream); 
	}
	if((mode&P_NOABORT || wip || rip || html) && online==ON_REMOTE) {
		SYNC;
		rioctl(IOSM|ABORT); 
	}
	if(rip)
		ansi_getlines();
	console=savcon;
}

void sbbs_t::printtail(char *str, int lines, long mode)
{
	char HUGE16 *buf,HUGE16 *p;
	int file,cur=0;
	ulong length,l;

	if(mode&P_NOABORT) {
		if(online==ON_REMOTE) {
			rioctl(IOCM|ABORT);
			rioctl(IOCS|ABORT); 
		}
		sys_status&=~SS_ABORT; 
	}
	if(!tos) {
		CRLF; 
	}
	if((file=nopen(str,O_RDONLY))==-1) {
		lprintf("File not found: %s",str);
		bputs(text[FileNotFound]);
		if(SYSOP) bputs(str);
		CRLF;
		return; 
	}
	length=filelength(file);
	if((buf=(char*)MALLOC(length+1L))==NULL) {
		close(file);
		errormsg(WHERE,ERR_ALLOC,str,length+1L);
		return; 
	}
	l=lread(file,buf,length);
	buf[l]=0;
	close(file);
	p=(buf+l)-1;
	if(*p==LF) p--;
	while(*p && p>buf) {
		if(*p==LF)
			cur++;
		if(cur>=lines) {
			p++;
			break; 
		}
		p--; 
	}
	putmsg(p,mode);
	if(mode&P_NOABORT && online==ON_REMOTE) {
		SYNC;
		rioctl(IOSM|ABORT); 
	}
	FREE((char *)buf);
}

/****************************************************************************/
/* Prints the menu number 'menunum' from the text directory. Checks for ^A  */
/* ,ANSI sequences, pauses and aborts. Usually accessed by user inputing '?'*/
/* Called from every function that has an available menu.                   */
/* The code definitions are as follows:                                     */
/****************************************************************************/
void sbbs_t::menu(char *code)
{
    char str[MAX_PATH+1],path[MAX_PATH+1];

	sys_status&=~SS_ABORT;
	if(menu_file[0])
		strcpy(path,menu_file);
	else {
		sprintf(str,"%smenu/",cfg.text_dir);
		if(menu_dir[0]) {
			strcat(str,menu_dir);
			strcat(str,"/"); 
		}
		strcat(str,code);
		strcat(str,".");
		sprintf(path,"%s%s",str,useron.misc&WIP ? "wip": useron.misc&RIP ? "rip" : "html");
		if(!(useron.misc&(RIP|WIP|HTML)) || !fexistcase(path)) {
			sprintf(path,"%smon",str);
			if((useron.misc&(COLOR|ANSI))!=ANSI || !fexistcase(path)) {
				sprintf(path,"%sans",str);
				if(!(useron.misc&ANSI) || !fexistcase(path))
					sprintf(path,"%sasc",str); 
			} 
		} 
	}

	printfile(path,P_OPENCLOSE);
}


