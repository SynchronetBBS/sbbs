/* prntfile.cpp */
// vi: tabstop=4

/* Synchronet file print/display routines */

/* $Id$ */

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

/****************************************************************************/
/* Prints a file remotely and locally, interpreting ^A sequences, checks    */
/* for pauses, aborts and ANSI. 'str' is the path of the file to print      */
/* Called from functions menu and text_sec                                  */
/****************************************************************************/
void sbbs_t::printfile(char *str, long mode)
{
	char* buf;
	char* p;
	int file;
	BOOL wip=FALSE,rip=FALSE,html=FALSE;
	long l,length,savcon=console;
	FILE *stream;

	p=strrchr(str,'.');
	if(p!=NULL) {
		if(stricmp(p,".wip")==0) {
			wip=TRUE;
			mode|=P_NOPAUSE;
		}
		else if(stricmp(p,".rip")==0) {
			rip=TRUE;
			mode|=P_NOPAUSE;
		}
		else if(stricmp(p,".html")==0)  {
			html=TRUE;
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

	if(!(mode&P_NOCRLF) && !tos && !wip && !rip && !html) {
		CRLF;
	}

	fexistcase(str);
	if((stream=fnopen(&file,str,O_RDONLY|O_DENYNONE))==NULL) {
		lprintf(LOG_NOTICE,"Node %d !Error %d (%s) opening: %s"
			,cfg.node_num,errno,strerror(errno),str);
		bputs(text[FileNotFound]);
		if(SYSOP) bputs(str);
		CRLF;
		return; 
	}

	length=(long)filelength(file);
	if(length<0) {
		fclose(stream);
		errormsg(WHERE,ERR_CHK,str,length);
		return;
	}
	if((buf=(char*)malloc(length+1L))==NULL) {
		fclose(stream);
		errormsg(WHERE,ERR_ALLOC,str,length+1L);
		return; 
	}
	l=lread(file,buf,length);
	fclose(stream);
	if(l!=length)
		errormsg(WHERE,ERR_READ,str,length);
	else {
		buf[l]=0;
		putmsg(buf,mode);
	}
	free(buf); 

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
	char*	buf;
	char*	p;
	int		file,cur=0;
	long	length,l;

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
	fexistcase(str);
	if((file=nopen(str,O_RDONLY|O_DENYNONE))==-1) {
		lprintf(LOG_NOTICE,"Node %d !Error %d (%s) opening: %s"
			,cfg.node_num,errno,strerror(errno),str);
		bputs(text[FileNotFound]);
		if(SYSOP) bputs(str);
		CRLF;
		return; 
	}
	length=(long)filelength(file);
	if(length<0) {
		close(file);
		errormsg(WHERE,ERR_CHK,str,length);
		return;
	}
	if((buf=(char*)malloc(length+1L))==NULL) {
		close(file);
		errormsg(WHERE,ERR_ALLOC,str,length+1L);
		return; 
	}
	l=lread(file,buf,length);
	close(file);
	if(l!=length)
		errormsg(WHERE,ERR_READ,str,length);
	else {
		buf[l]=0;
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
	}
	if(mode&P_NOABORT && online==ON_REMOTE) {
		SYNC;
		rioctl(IOSM|ABORT); 
	}
	free(buf);
}

/****************************************************************************/
/* Displays a menu file (e.g. from the text/menu directory)                 */
/****************************************************************************/
void sbbs_t::menu(const char *code, long mode)
{
    char path[MAX_PATH+1];
	const char *next= "msg";
	const char *last = "asc";

	sys_status&=~SS_ABORT;
	if(menu_file[0])
		SAFECOPY(path,menu_file);
	else {
		long term = term_supports();
		do {
			if((term&RIP) && menu_exists(code, "rip", path))
				break;
			if((term&(ANSI|COLOR)) == ANSI && menu_exists(code, "mon", path))
				break;
			if((term&ANSI) && menu_exists(code, "ans", path))
				break;
			if((term&PETSCII) && menu_exists(code, "seq", path))
				break;
			if(term&NO_EXASCII) {
				next = "asc";
				last = "msg";
			}
			if(menu_exists(code, next, path))
				break;
			menu_exists(code, last, path);
		} while(0);
	}

	mode |= P_OPENCLOSE | P_CPM_EOF;
	if(column == 0)
		mode |= P_NOCRLF;
	printfile(path, mode);
}

bool sbbs_t::menu_exists(const char *code, const char* ext, char* path)
{
	char pathbuf[MAX_PATH+1];
	if(path == NULL)
		path = pathbuf;

	if(menu_file[0]) {
		strncpy(path, menu_file, MAX_PATH);
		return fexistcase(path) ? true : false;
	}

	/* Either <menu>.asc or <menu>.msg is required */
	if(ext == NULL)
		return menu_exists(code, "asc", path)
			|| menu_exists(code, "msg", path);

	backslash(menu_dir);
	safe_snprintf(path, MAX_PATH, "%smenu/%s%s.%ucol.%s"
		,cfg.text_dir, menu_dir, code, cols, ext);
	if(fexistcase(path))
		return true;
	safe_snprintf(path, MAX_PATH, "%smenu/%s%s.%s"
		,cfg.text_dir, menu_dir, code, ext);
	return fexistcase(path) ? true : false;
}
