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
bool sbbs_t::printfile(const char* fname, long mode)
{
	char* buf;
	char fpath[MAX_PATH+1];
	char* p;
	int file;
	BOOL rip=FALSE;
	long l,length,savcon=console;
	FILE *stream;

	SAFECOPY(fpath, fname);
	fexistcase(fpath);
	p=getfext(fpath);
	if(p!=NULL) {
		if(stricmp(p,".rip")==0) {
			rip=TRUE;
			mode|=P_NOPAUSE;
		} else if(stricmp(p, ".seq") == 0) {
			mode |= P_PETSCII;
		}
	}

	if(mode&P_NOABORT || rip) {
		if(online==ON_REMOTE && console&CON_R_ECHO) {
			rioctl(IOCM|ABORT);
			rioctl(IOCS|ABORT); 
		}
		sys_status&=~SS_ABORT; 
	}

	if(!(mode&P_NOCRLF) && !tos && !rip) {
		CRLF;
	}

	if((stream=fnopen(&file,fpath,O_RDONLY|O_DENYNONE))==NULL) {
		if(!(mode&P_NOERROR)) {
			lprintf(LOG_NOTICE,"!Error %d (%s) opening: %s"
				,errno,strerror(errno),fpath);
			bputs(text[FileNotFound]);
			if(SYSOP) bputs(fpath);
			CRLF;
		}
		return false; 
	}

	length=(long)filelength(file);
	if(length<0) {
		fclose(stream);
		errormsg(WHERE,ERR_CHK,fpath,length);
		return false;
	}
	if((buf=(char*)malloc(length+1L))==NULL) {
		fclose(stream);
		errormsg(WHERE,ERR_ALLOC,fpath,length+1L);
		return false; 
	}
	l=lread(file,buf,length);
	fclose(stream);
	if(l!=length)
		errormsg(WHERE,ERR_READ,fpath,length);
	else {
		buf[l]=0;
		putmsg(buf,mode);
	}
	free(buf); 

	if((mode&P_NOABORT || rip) && online==ON_REMOTE) {
		SYNC;
		rioctl(IOSM|ABORT); 
	}
	if(rip)
		ansi_getlines();
	console=savcon;
	return true;
}

bool sbbs_t::printtail(const char* fname, int lines, long mode)
{
	char*	buf;
	char	fpath[MAX_PATH+1];
	char*	p;
	int		file,cur=0;
	long	length,l;

	SAFECOPY(fpath, fname);
	fexistcase(fpath);
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
	if((file=nopen(fpath,O_RDONLY|O_DENYNONE))==-1) {
		if(!(mode&P_NOERROR)) {
			lprintf(LOG_NOTICE,"!Error %d (%s) opening: %s"
				,errno,strerror(errno),fpath);
			bputs(text[FileNotFound]);
			if(SYSOP) bputs(fpath);
			CRLF;
		}
		return false; 
	}
	length=(long)filelength(file);
	if(length<0) {
		close(file);
		errormsg(WHERE,ERR_CHK,fpath,length);
		return false;
	}
	if((buf=(char*)malloc(length+1L))==NULL) {
		close(file);
		errormsg(WHERE,ERR_ALLOC,fpath,length+1L);
		return false; 
	}
	l=lread(file,buf,length);
	close(file);
	if(l!=length)
		errormsg(WHERE,ERR_READ,fpath,length);
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
	return true;
}

/****************************************************************************/
/* Displays a menu file (e.g. from the text/menu directory)                 */
/****************************************************************************/
bool sbbs_t::menu(const char *code, long mode)
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
	return printfile(path, mode);
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
