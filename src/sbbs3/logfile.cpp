/* logfile.cpp */

/* Synchronet log file routines */

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

extern "C" BOOL DLLCALL hacklog(scfg_t* cfg, char* prot, char* user, char* text, char* host, SOCKADDR_IN* addr)
{
	char	hdr[512];
	char	tstr[64];
	char	fname[MAX_PATH+1];
	int		file;
	time_t	now=time(NULL);

	sprintf(fname,"%shack.log",cfg->data_dir);

	if((file=sopen(fname,O_CREAT|O_RDWR|O_BINARY|O_APPEND,SH_DENYWR))==-1)
		return(FALSE);

	sprintf(hdr,"SUSPECTED %s HACK ATTEMPT from %s on %.24s\r\nUsing port %u at %s [%s]\r\nDetails: "
		,prot
		,user
		,timestr(cfg,&now,tstr)
		,addr->sin_port
		,host
		,inet_ntoa(addr->sin_addr)
		);
	write(file,hdr,strlen(hdr));
	write(file,text,strlen(text));
	write(file,crlf,2);
	write(file,crlf,2);
	close(file);

	return(TRUE);
}

extern "C" BOOL DLLCALL spamlog(scfg_t* cfg, char* prot, char* action
								,char* reason, char* host, char* ip_addr
								,char* to, char* from)
{
	char	hdr[512];
	char	to_user[128];
	char	tstr[64];
	char	fname[MAX_PATH+1];
	int		file;
	time_t	now=time(NULL);

	sprintf(fname,"%sspam.log",cfg->data_dir);

	if((file=sopen(fname,O_CREAT|O_RDWR|O_BINARY|O_APPEND,SH_DENYWR))==-1)
		return(FALSE);

	if(to==NULL)
		to_user[0]=0;
	else
		sprintf(to_user,"to: %s",to);

	if(from==NULL)
		from=host;
		
	sprintf(hdr,"SUSPECTED %s SPAM %s on %.24s\r\nHost: %s [%s]\r\nFrom: %s %s\r\nReason: "
		,prot
		,action
		,timestr(cfg,&now,tstr)
		,host
		,ip_addr
		,from
		,to_user
		);
	write(file,hdr,strlen(hdr));
	write(file,reason,strlen(reason));
	write(file,crlf,2);
	write(file,crlf,2);
	close(file);

	return(TRUE);
}

void sbbs_t::logentry(char *code, char *entry)
{
	char str[512];

	now=time(NULL);
	sprintf(str,"Node %2d  %s\r\n   %s",cfg.node_num,timestr(&now),entry);
	logline(code,str);
}

/****************************************************************************/
/* Writes 'str' verbatim into node.log 										*/
/****************************************************************************/
void sbbs_t::log(char *str)
{
	if(logfile_fp==NULL || online==ON_LOCAL) return;
	if(logcol>=78 || (78-logcol)<strlen(str)) {
		fprintf(logfile_fp,"\r\n");
		logcol=1; }
	if(logcol==1) {
		fprintf(logfile_fp,"   ");
		logcol=4; }
	fprintf(logfile_fp,str);
	if(str[strlen(str)-1]==LF)
		logcol=1;
	else
		logcol+=strlen(str);
}

bool sbbs_t::syslog(char* code, char *entry)
{		
	char	fname[MAX_PATH+1];
	char	str[128];
	char	tmp[64];
	int		file;
	struct tm tm;

	now=time(NULL);
	if(localtime_r(&now,&tm)==NULL)
		return(false);
	sprintf(fname,"%slogs/%2.2d%2.2d%2.2d.log",cfg.data_dir,tm.tm_mon+1,tm.tm_mday
		,TM_YEAR(tm.tm_year));
	if((file=nopen(fname,O_WRONLY|O_APPEND|O_CREAT))==-1) {
		lprintf("!ERRROR %d opening/creating %s",errno,fname); 
		return(false);
	}

	sprintf(str,"%-2.2s %s  %s\r\n\r\n",code, hhmmtostr(&cfg,&tm,tmp), entry);
	write(file,str,strlen(str));
	close(file);

	return(true);
}

/****************************************************************************/
/* Writes 'str' on it's own line in node.log								*/
/****************************************************************************/
void sbbs_t::logline(char *code, char *str)
{
	if(strchr(str,'\n')==NULL) {	// Keep the console log pretty
		if(online==ON_LOCAL)
			eprintf("%s",str);
		else
			lprintf("Node %d %s", cfg.node_num, str);
	}
	if(logfile_fp==NULL || (online==ON_LOCAL && strcmp(code,"!!"))) return;
	if(logcol!=1)
		fprintf(logfile_fp,"\r\n");
	fprintf(logfile_fp,"%-2.2s %s\r\n",code,str);
	logcol=1;
}

/****************************************************************************/
/* Writes a comma then 'ch' to log, tracking column.						*/
/****************************************************************************/
void sbbs_t::logch(char ch, bool comma)
{

	if(logfile_fp==NULL || (online==ON_LOCAL)) return;
	if((uchar)ch<SP)	/* Don't log control chars */
		return;
	if(logcol==1) {
		logcol=4;
		fprintf(logfile_fp,"   "); 
	}
	else if(logcol>=78) {
		logcol=4;
		fprintf(logfile_fp,"\r\n   "); 
	}
	if(comma && logcol!=4) {
		fprintf(logfile_fp,",");
		logcol++; 
	}
	if(ch&0x80) {
		ch&=0x7f;
		if(ch<SP)
			return;
		fprintf(logfile_fp,"/"); 
	}
	fprintf(logfile_fp,"%c",ch);
	logcol++;
}

/****************************************************************************/
/* Error handling routine. Prints to local and remote screens the error     */
/* information, function, action, object and access and then attempts to    */
/* write the error information into the file ERROR.LOG and NODE.LOG         */
/****************************************************************************/
void sbbs_t::errormsg(int line, char *source, char action, char *object
					  ,ulong access, char *extinfo)
{
	char*	src;
    char	str[2048];
	char 	tmp[512];
    char*	actstr;

	/* prevent recursion */
	if(errormsg_inside)
		return;
	errormsg_inside=true;

	/* Don't log path to source code */
	src=strrchr(source,BACKSLASH);
	if(src==NULL) 
		src=source;
	else
		src++;

	switch(action) {
		case ERR_OPEN:
			actstr="opening";
			break;
		case ERR_CLOSE:
			actstr="closing";
			break;
		case ERR_FDOPEN:
			actstr="fdopen";
			break;
		case ERR_READ:
			actstr="reading";
			break;
		case ERR_WRITE:
			actstr="writing";
			break;
		case ERR_REMOVE:
			actstr="removing";
			break;
		case ERR_ALLOC:
			actstr="allocating memory";
			break;
		case ERR_CHK:
			actstr="checking";
			break;
		case ERR_LEN:
			actstr="checking length";
			break;
		case ERR_EXEC:
			actstr="executing";
			break;
		case ERR_CHDIR:
			actstr="changing directory";
			break;
		case ERR_CREATE:
			actstr="creating";
			break;
		case ERR_LOCK:
			actstr="locking";
			break;
		case ERR_UNLOCK:
			actstr="unlocking";
			break;
		case ERR_TIMEOUT:
    		actstr="time-out waiting for resource";
			break;
		case ERR_IOCTL:
    		actstr="sending IOCTL";
			break;
		default:
			actstr="UNKNOWN"; 
			break;
	}
	sprintf(str,"Node %d !ERROR %d in %s line %d %s \"%s\" access=%ld"
		,cfg.node_num, errno, src, line, actstr, object, access);
	if(online==ON_LOCAL)
		eprintf("%s",str);
	else {
		lprintf("%s",str);
		bprintf("\7\r\nERROR -   action: %s",actstr);   /* tell user about error */
		bprintf("\7\r\n          object: %s",object);
		bprintf("\7\r\n          access: %ld",access);
		if(access>9 && (long)access!=-1 && (short)access!=-1 && (char)access!=-1)
			bprintf(" (0x%lX)",access);
		if(cfg.sys_misc&SM_ERRALARM) {
			sbbs_beep(500,220); sbbs_beep(250,220);
			sbbs_beep(500,220); sbbs_beep(250,220);
			sbbs_beep(500,220); sbbs_beep(250,220);
			nosound(); }
		bputs("\r\n\r\nThe sysop has been notified. <Hit a key>");
		getkey(0);
		CRLF;
	}
	sprintf(str,"\r\n    source: %s\r\n      line: %d\r\n    action: %s\r\n"
		"    object: %s\r\n    access: %ld"
		,src,line,actstr,object,access);
	if(access>9 && (long)access!=-1 && (short)access!=-1 && (char)access!=-1) {
		sprintf(tmp," (0x%lX)",access);
		strcat(str,tmp); }
	if(extinfo!=NULL) {
		sprintf(tmp,"\r\n      info: %s",extinfo);
		strcat(str,tmp);
	}
	if(errno) {
		sprintf(tmp,"\r\n     errno: %d (%s)",errno,strerror(errno));
		strcat(str,tmp); 
		errno=0;
	}
#if defined(__MSDOS__)
	if(_doserrno && _doserrno!=(ulong)errno) {
		sprintf(tmp,"\r\n  doserrno: %d",_doserrno);
		strcat(str,tmp); 
	}
	_doserrno=0;
#endif
#if defined(_WIN32)
	if(GetLastError()!=0) {
		sprintf(tmp,"\r\n  winerrno: %d (0x%X)",GetLastError(), GetLastError());
		strcat(str,tmp);
	}
#endif
	errorlog(str);
	errormsg_inside=false;
}

/*****************************************************************************/
/* Error logging to NODE.LOG and DATA\ERROR.LOG function                     */
/*****************************************************************************/
void sbbs_t::errorlog(char *text)
{
    char hdr[256],str[256],tmp2[256];
    int file;

	if(errorlog_inside)		/* let's not go recursive on this puppy */
		return;
	errorlog_inside=1;
	if(cfg.node_num>0) {
		getnodedat(cfg.node_num,&thisnode,1);
		criterrs=++thisnode.errors;
		putnodedat(cfg.node_num,&thisnode);
	}
	now=time(NULL);
	logline("!!",text);
	sprintf(str,"%serror.log",cfg.data_dir);
	if((file=nopen(str,O_WRONLY|O_CREAT|O_APPEND))==-1) {
		sprintf(tmp2,"!ERROR %d opening/creating %s",errno,str);
		logline("!!",tmp2);
		errorlog_inside=0;
		return; }
	sprintf(hdr,"%s Node %2d: %s #%d"
		,timestr(&now),cfg.node_num,useron.alias,useron.number);
	write(file,hdr,strlen(hdr));
	write(file,crlf,2);
	write(file,text,strlen(text));
	write(file,"\r\n\r\n",4);
	close(file);
	errorlog_inside=0;
}
