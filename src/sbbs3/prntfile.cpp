/* Synchronet file print/display routines */

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
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "sbbs.h"
#include "utf8.h"
#include "petdefs.h"

#ifndef PRINTFILE_MAX_LINE_LEN
#define PRINTFILE_MAX_LINE_LEN (8*1024)
#endif
#ifndef PRINTFILE_MAX_FILE_LEN
#define PRINTFILE_MAX_FILE_LEN (2*1024*1024)
#endif

/****************************************************************************/
/* Prints a file remotely and locally, interpreting ^A sequences, checks    */
/* for pauses, aborts and ANSI. 'str' is the path of the file to print      */
/* Called from functions menu and text_sec                                  */
/****************************************************************************/
bool sbbs_t::printfile(const char* fname, long mode, long org_cols, JSObject* obj)
{
	char* buf;
	char fpath[MAX_PATH+1];
	char* p;
	int file;
	BOOL rip=FALSE;
	long l,length,savcon=console;
	FILE *stream;

	SAFECOPY(fpath, fname);
	(void)fexistcase(fpath);
	p=getfext(fpath);
	if(p!=NULL) {
		if(stricmp(p,".rip")==0) {
			rip=TRUE;
			mode|=P_NOPAUSE;
		} else if(stricmp(p, ".seq") == 0) {
			mode |= P_PETSCII;
		} else if(stricmp(p, ".utf8") == 0) {
			mode |= P_UTF8;
		}
	}

	if(mode&P_NOABORT || rip) {
		if(online==ON_REMOTE && console&CON_R_ECHO) {
			rioctl(IOCM|ABORT);
			rioctl(IOCS|ABORT); 
		}
		sys_status&=~SS_ABORT; 
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
	if(length < 1) {
		fclose(stream);
		if(length < 0) {
			errormsg(WHERE,ERR_CHK,fpath,length);
			return false;
		}
		return true;
	}

	if(!(mode&P_NOCRLF) && row > 0 && !rip) {
		newline();
	}

	if((mode&P_OPENCLOSE) && length <= PRINTFILE_MAX_FILE_LEN) {
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
			if((mode&P_UTF8) && !term_supports(UTF8))
				utf8_normalize_str(buf);
			putmsg(buf,mode,org_cols, obj);
		}
		free(buf);
	} else {	// Line-at-a-time mode
		ulong sys_status_sav = sys_status;
		enum output_rate output_rate = cur_output_rate;
		uint tmpatr = curatr;
		ulong orgcon = console;
		attr_sp = 0;	/* clear any saved attributes */
		if(!(mode&P_SAVEATR))
			attr(LIGHTGRAY);
		if(mode&P_NOPAUSE)
			sys_status |= SS_PAUSEOFF;
		if(length > PRINTFILE_MAX_LINE_LEN)
			length = PRINTFILE_MAX_LINE_LEN;
		if((buf=(char*)malloc(length+1L))==NULL) {
			fclose(stream);
			errormsg(WHERE,ERR_ALLOC,fpath,length+1L);
			return false; 
		}
		while(!feof(stream) && !msgabort()) {
			if(fgets(buf, length + 1, stream) == NULL)
				break;
			if((mode&P_UTF8) && !term_supports(UTF8))
				utf8_normalize_str(buf);
			if(putmsgfrag(buf, mode, org_cols, obj) != '\0') // early-EOF?
				break;
		}
		free(buf);
		fclose(stream);
		if(!(mode&P_SAVEATR)) {
			console = orgcon;
			attr(tmpatr);
		}
		if(!(mode&P_NOATCODES) && cur_output_rate != output_rate)
			set_output_rate(output_rate);
		if(mode&P_PETSCII)
			outcom(PETSCII_UPPERLOWER);
		attr_sp=0;	/* clear any saved attributes */

		/* Restore original settings of Forced Pause On/Off */
		sys_status &= ~(SS_PAUSEOFF|SS_PAUSEON);
		sys_status |= (sys_status_sav&(SS_PAUSEOFF|SS_PAUSEON));
	}

	if((mode&P_NOABORT || rip) && online==ON_REMOTE) {
		SYNC;
		rioctl(IOSM|ABORT); 
	}
	if(rip)
		ansi_getlines();
	console=savcon;
	return true;
}

bool sbbs_t::printtail(const char* fname, int lines, long mode, long org_cols, JSObject* obj)
{
	char*	buf;
	char	fpath[MAX_PATH+1];
	char*	p;
	FILE*	fp;
	int		file,cur=0;
	long	length,l;

	SAFECOPY(fpath, fname);
	(void)fexistcase(fpath);
	if(mode&P_NOABORT) {
		if(online==ON_REMOTE) {
			rioctl(IOCM|ABORT);
			rioctl(IOCS|ABORT); 
		}
		sys_status&=~SS_ABORT; 
	}
	if((fp=fnopen(&file,fpath,O_RDONLY|O_DENYNONE))==NULL) {
		if(!(mode&P_NOERROR)) {
			lprintf(LOG_NOTICE,"!Error %d (%s) opening: %s"
				,errno,strerror(errno),fpath);
			bputs(text[FileNotFound]);
			if(SYSOP) bputs(fpath);
			CRLF;
		}
		return false; 
	}
	if(!(mode&P_NOCRLF) && row > 0) {
		newline();
	}
	length=(long)filelength(file);
	if(length<0) {
		fclose(fp);
		errormsg(WHERE,ERR_CHK,fpath,length);
		return false;
	}
	if(length > lines * PRINTFILE_MAX_LINE_LEN) {
		length = lines * PRINTFILE_MAX_LINE_LEN; 
		(void)fseek(fp, -length, SEEK_END);
	}
	if((buf=(char*)malloc(length+1L))==NULL) {
		fclose(fp);
		errormsg(WHERE,ERR_ALLOC,fpath,length+1L);
		return false; 
	}
	l=fread(buf, sizeof(char), length, fp);
	fclose(fp);
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
		putmsg(p,mode,org_cols, obj);
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
/* Pass a code including wildcards (* or ?) to display a randomly-chosen	*/
/* file matching the pattern in 'code'										*/
/****************************************************************************/
bool sbbs_t::menu(const char *code, long mode, JSObject* obj)
{
    char path[MAX_PATH+1];
	const char *next= "msg";
	const char *last = "asc";

	if(strcspn(code, "*?") != strlen(code))
		return random_menu(code, mode, obj);

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
			if(!menu_exists(code, last, path))
				return false;
		} while(0);
	}

	mode |= P_OPENCLOSE | P_CPM_EOF;
	if(column == 0)
		mode |= P_NOCRLF;
	return printfile(path, mode, /* org_cols: */0, obj);
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

	char prefix[MAX_PATH];
	if(isfullpath(code))
		SAFECOPY(prefix, code);
	else {
		backslash(menu_dir);
		SAFEPRINTF3(prefix, "%smenu/%s%s", cfg.text_dir, menu_dir, code);
		FULLPATH(path, prefix, MAX_PATH);
		SAFECOPY(prefix, path);
	}
	safe_snprintf(path, MAX_PATH, "%s.%lucol.%s", prefix, cols, ext);
	if(fexistcase(path))
		return true;
	safe_snprintf(path, MAX_PATH, "%s.%s", prefix, ext);
	return fexistcase(path) ? true : false;
}

/****************************************************************************/
/* Displays a random menu file (e.g. from the text/menu directory)          */
/****************************************************************************/
bool sbbs_t::random_menu(const char *name, long mode, JSObject* obj)
{
	char path[MAX_PATH + 1];
	glob_t g = {0};
	str_list_t names = NULL;

	SAFEPRINTF2(path, "%smenu/%s", cfg.text_dir, name);
	if(glob(path, GLOB_NOESCAPE|GLOB_MARK, NULL, &g) != 0) {
		return false;
	}
	for(size_t i = 0; i < g.gl_pathc; i++) {
		char* ext = getfext(g.gl_pathv[i]);
		if(ext == NULL)
			continue;
		*ext = 0;
		strListPush(&names, g.gl_pathv[i]);
	}
	globfree(&g);
	strListDedupe(&names, /* case_sensitive: */true);
	bool result = false;
	size_t i = sbbs_random(strListCount(names));
	if(menu_exists(names[i], NULL, path)) {
		result = menu(names[i], mode, obj);
	}
	strListFree(&names);
	return result;
}
