/* logfile.cpp */

/* Synchronet log file routines */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2011 Rob Swindell - http://www.synchro.net/copyright.html		*
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
	char	hdr[1024];
	char	tstr[64];
	char	fname[MAX_PATH+1];
	int		file;
	time32_t now=time32(NULL);

	sprintf(fname,"%shack.log",cfg->logs_dir);

	if((file=sopen(fname,O_CREAT|O_RDWR|O_BINARY|O_APPEND,SH_DENYWR,DEFFILEMODE))==-1)
		return(FALSE);

	sprintf(hdr,"SUSPECTED %s HACK ATTEMPT for user '%s' on %.24s\r\nUsing port %u at %s [%s]\r\nDetails: "
		,prot
		,user
		,timestr(cfg,now,tstr)
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

BOOL sbbs_t::hacklog(char* prot, char* text)
{
	return ::hacklog(&cfg, prot, useron.alias, text, client_name, &client_addr);
}

extern "C" BOOL DLLCALL spamlog(scfg_t* cfg, char* prot, char* action
								,char* reason, char* host, char* ip_addr
								,char* to, char* from)
{
	char	hdr[1024];
	char	to_user[256];
	char	tstr[64];
	char	fname[MAX_PATH+1];
	int		file;
	time32_t now=time32(NULL);

	sprintf(fname,"%sspam.log",cfg->logs_dir);

	if((file=sopen(fname,O_CREAT|O_RDWR|O_BINARY|O_APPEND,SH_DENYWR,DEFFILEMODE))==-1)
		return(FALSE);

	if(to==NULL)
		to_user[0]=0;
	else
		sprintf(to_user,"to: %.128s",to);

	if(from==NULL)
		from=host;
		
	sprintf(hdr,"SUSPECTED %s SPAM %s on %.24s\r\nHost: %s [%s]\r\nFrom: %.128s %s\r\nReason: "
		,prot
		,action
		,timestr(cfg,now,tstr)
		,host
		,ip_addr
		,from
		,to_user
		);
	write(file,hdr,strlen(hdr));
	if(reason!=NULL)
		write(file,reason,strlen(reason));
	write(file,crlf,2);
	write(file,crlf,2);
	close(file);

	return(TRUE);
}

extern "C" int DLLCALL errorlog(scfg_t* cfg, const char* host, const char* text)
{
	FILE*	fp;
	char	buf[128];
	char	path[MAX_PATH+1];

	sprintf(path,"%serror.log",cfg->logs_dir);
	if((fp=fnopen(NULL,path,O_WRONLY|O_CREAT|O_APPEND))==NULL)
		return -1; 
	fprintf(fp,"%s %s\r\n%s\r\n\r\n", timestr(cfg,time32(NULL),buf), host==NULL ? "":host, text);
	fclose(fp);
	return 0;
}

void sbbs_t::logentry(const char *code, const char *entry)
{
	char str[512];

	now=time(NULL);
	sprintf(str,"Node %2d  %s\r\n   %s",cfg.node_num,timestr(now),entry);
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
		logcol=1; 
	}
	if(logcol==1) {
		fprintf(logfile_fp,"   ");
		logcol=4; 
	}
	fprintf(logfile_fp,str);
	if(*lastchar(str)==LF) {
		logcol=1;
		fflush(logfile_fp);
	}
	else
		logcol+=strlen(str);
}

bool sbbs_t::syslog(const char* code, const char *entry)
{		
	char	fname[MAX_PATH+1];
	char	str[128];
	char	tmp[64];
	int		file;
	struct tm tm;

	now=time(NULL);
	if(localtime_r(&now,&tm)==NULL)
		return(false);
	sprintf(fname,"%slogs/%2.2d%2.2d%2.2d.log",cfg.logs_dir,tm.tm_mon+1,tm.tm_mday
		,TM_YEAR(tm.tm_year));
	if((file=nopen(fname,O_WRONLY|O_APPEND|O_CREAT))==-1) {
		lprintf(LOG_ERR,"!ERRROR %d opening/creating %s",errno,fname); 
		return(false);
	}

	sprintf(str,"%-2.2s %s  %s\r\n\r\n",code, hhmmtostr(&cfg,&tm,tmp), entry);
	write(file,str,strlen(str));
	close(file);

	return(true);
}

/****************************************************************************/
/* Writes 'str' on it's own line in node.log (using LOG_INFO level)			*/
/****************************************************************************/
void sbbs_t::logline(const char *code, const char *str)
{
	logline(LOG_INFO, code, str);
}

/****************************************************************************/
/* Writes 'str' on it's own line in node.log								*/
/****************************************************************************/
void sbbs_t::logline(int level, const char *code, const char *str)
{
	if(strchr(str,'\n')==NULL) {	// Keep the console log pretty
		if(online==ON_LOCAL)
			eprintf(level,"%s",str);
		else
			lprintf(level,"Node %d %s", cfg.node_num, str);
	}
	if(logfile_fp==NULL || (online==ON_LOCAL && strcmp(code,"!!"))) return;
	if(logcol!=1)
		fprintf(logfile_fp,"\r\n");
	fprintf(logfile_fp,"%-2.2s %s\r\n",code,str);
	logcol=1;
	fflush(logfile_fp);
}

/****************************************************************************/
/* Writes a comma then 'ch' to log, tracking column.						*/
/****************************************************************************/
void sbbs_t::logch(char ch, bool comma)
{

	if(logfile_fp==NULL || (online==ON_LOCAL)) return;
	if((uchar)ch<' ')	/* Don't log control chars */
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
		if(ch<' ')
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
void sbbs_t::errormsg(int line, const char *source, const char* action, const char *object
					  ,ulong access, const char *extinfo)
{
	const char*	src;
    char	str[2048];

	/* prevent recursion */
	if(errormsg_inside)
		return;
	errormsg_inside=true;

	/* Don't log path to source code */
	src=getfname(source);
	safe_snprintf(str,sizeof(str),"ERROR %d (%s) "
#ifdef _WIN32
		"(WinError %u) "
#endif
		"in %s line %u %s \"%s\" access=%ld %s%s"
		,errno,STRERROR(errno)
#ifdef _WIN32
		,GetLastError()
#endif
		,src, line, action, object, access
		,extinfo==NULL ? "":"info="
		,extinfo==NULL ? "":extinfo);
	if(online==ON_LOCAL)
		eprintf(LOG_ERR,"%s",str);
	else {
		int savatr=curatr;
		lprintf(LOG_ERR,"Node %d !%s",cfg.node_num, str);
		attr(cfg.color[clr_err]);
		bprintf("\7\r\n!ERROR %s %s\r\n", action, object);   /* tell user about error */
		bputs("\r\nThe sysop has been notified.\r\n");
		pause();
		attr(savatr);
		CRLF;
	}
	safe_snprintf(str,sizeof(str),"ERROR %s %s", action, object);
	if(cfg.node_num>0) {
		getnodedat(cfg.node_num,&thisnode,1);
		if(thisnode.errors<UCHAR_MAX)
			thisnode.errors++;
		criterrs=thisnode.errors;
		putnodedat(cfg.node_num,&thisnode);
	}
	now=time(NULL);

	if(logfile_fp!=NULL) {
		if(logcol!=1)
			fprintf(logfile_fp,"\r\n");
		fprintf(logfile_fp,"!! %s\r\n",str);
		logcol=1;
		fflush(logfile_fp);
	}

	errormsg_inside=false;
}
