/* Synchronet log file routines */

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
#include "git_branch.h"
#include "git_hash.h"

const char* log_line_ending = "\r\n";

extern "C" BOOL hacklog(scfg_t* cfg, const char* prot, const char* user, const char* text, const char* host, union xp_sockaddr* addr)
{
	char	tstr[64];
	char	fname[MAX_PATH+1];
	FILE*	fp;
	char	ip[INET6_ADDRSTRLEN];
	time32_t now=time32(NULL);

	SAFEPRINTF(fname, "%shack.log", cfg->logs_dir);

	if((fp = fnopen(NULL, fname, O_WRONLY|O_CREAT|O_APPEND)) == NULL)
		return false;

	inet_addrtop(addr, ip, sizeof(ip));
	fprintf(fp,"SUSPECTED %s HACK ATTEMPT for user '%s' on %.24s%sUsing port %u at %s [%s]%s"
		,prot
		,user
		,timestr(cfg,now,tstr)
		,log_line_ending
		,inet_addrport(addr)
		,host
		,ip
		,log_line_ending
		);
	if(text != NULL)
		fprintf(fp, "Details: %s%s", text, log_line_ending);
	fputs(log_line_ending, fp);
	fclose(fp);

	return true;
}

BOOL sbbs_t::hacklog(const char* prot, const char* text)
{
	return ::hacklog(&cfg, prot, useron.alias, text, client_name, &client_addr);
}

extern "C" BOOL spamlog(scfg_t* cfg, char* prot, char* action
								,char* reason, char* host, char* ip_addr
								,char* to, char* from)
{
	char	to_user[256];
	char	tstr[64];
	char	fname[MAX_PATH+1];
	FILE*	fp;
	time32_t now=time32(NULL);

	SAFEPRINTF(fname, "%sspam.log", cfg->logs_dir);

	if((fp = fnopen(NULL, fname, O_WRONLY|O_CREAT|O_APPEND)) == NULL)
		return false;

	if(to==NULL)
		to_user[0]=0;
	else
		SAFEPRINTF(to_user,"to: %.128s",to);

	if(from==NULL)
		from=host;
		
	fprintf(fp, "SUSPECTED %s SPAM %s on %.24s%sHost: %s [%s]%sFrom: %.128s %s%s"
		,prot
		,action
		,timestr(cfg,now,tstr)
		,log_line_ending
		,host
		,ip_addr
		,log_line_ending
		,from
		,to_user
		,log_line_ending
		);
	if(reason != NULL)
		fprintf(fp, "Reason: %s%s", reason, log_line_ending);
	fputs(log_line_ending, fp);
	fclose(fp);

	return true;
}

extern "C" int errorlog(scfg_t* cfg, int level, const char* host, const char* text)
{
	FILE*	fp;
	char	buf[128];
	char	path[MAX_PATH+1];
	time_t	now = time(NULL);

	SAFEPRINTF(path, "%serror.log", cfg->logs_dir);
	if((fp = fnopen(NULL,path,O_WRONLY|O_CREAT|O_APPEND))==NULL)
		return -1; 
	fprintf(fp,"%.24s %s/%s %s%s%s%s%s"
		,ctime_r(&now, buf)
		,GIT_BRANCH
		,GIT_HASH
		,host==NULL ? "":host
		,log_line_ending
		,text
		,log_line_ending
		,log_line_ending
		);
	fclose(fp);
	if(cfg->node_erruser && level <= cfg->node_errlevel) {
		char subject[128];
		SAFEPRINTF2(subject, "%s %sERROR occurred", host == NULL ? "" : host, level <= LOG_CRIT ? "CRITICAL " : "");
		notify(cfg, cfg->node_erruser, subject, text);
	}
	return 0;
}

void sbbs_t::logentry(const char *code, const char *entry)
{
	char str[512];

	now=time(NULL);
	SAFEPRINTF4(str, "Node %2d  %s%s   %s", cfg.node_num, timestr(now), log_line_ending, entry);
	logline(code,str);
}

/****************************************************************************/
/* Writes 'str' verbatim into node.log 										*/
/****************************************************************************/
void sbbs_t::log(const char *str)
{
	if(logfile_fp==NULL || online==ON_LOCAL) return;
	if(logcol>=78 || (78-logcol)<strlen(str)) {
		fputs(log_line_ending, logfile_fp);
		logcol=1; 
	}
	if(logcol==1) {
		fputs("   ",logfile_fp);
		logcol=4; 
	}
	fputs(str,logfile_fp);
	if(*lastchar(str)==LF) {
		logcol=1;
		fflush(logfile_fp);
	}
	else
		logcol+=strlen(str);
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
	if(strchr(str,'\n')==NULL) 	// Keep the console log pretty
		lputs(level, str);
	if(logfile_fp==NULL || (online==ON_LOCAL && strcmp(code,"!!"))) return;
	if(logcol!=1)
		fputs(log_line_ending, logfile_fp);
	fprintf(logfile_fp,"%-2.2s %s%s", code, str, log_line_ending);
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
		fprintf(logfile_fp, "%s   ", log_line_ending); 
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
void sbbs_t::errormsg(int line, const char* function, const char *src, const char* action, const char *object
					  ,long access, const char *extinfo)
{
    char	str[2048];
	char	errno_str[128];
	char	errno_info[256] = "";

	/* prevent recursion */
	if(errormsg_inside)
		return;
	errormsg_inside=true;

	if(strcmp(action, ERR_CHK) != 0)
		safe_snprintf(errno_info, sizeof(errno_info), "%d (%s) "
	#ifdef _WIN32
			"(WinError %u) "
	#endif
			,errno, safe_strerror(errno, errno_str, sizeof(errno_str))
	#ifdef _WIN32
			,GetLastError()
	#endif
		);

	safe_snprintf(str,sizeof(str),"ERROR %s"
		"in %s line %u (%s) %s \"%s\" access=%ld %s%s"
		,errno_info
		,src, line, function, action, object, access
		,extinfo==NULL ? "":"info="
		,extinfo==NULL ? "":extinfo);

	lprintf(LOG_ERR, "!%s", str);
	if(online == ON_REMOTE) {
		int savatr=curatr;
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
			fputs(log_line_ending, logfile_fp);
		fprintf(logfile_fp,"!! %s%s", str, log_line_ending);
		logcol=1;
		fflush(logfile_fp);
	}

	errormsg_inside=false;
}
