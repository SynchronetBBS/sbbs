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
#include "mqtt.h"
#include "git_branch.h"
#include "git_hash.h"

const char* log_line_ending = "\r\n";

extern "C" bool hacklog(scfg_t* cfg, struct mqtt* mqtt, const char* prot, const char* user, const char* text, const char* host, union xp_sockaddr* addr)
{
	char   tstr[64];
	char   fname[MAX_PATH + 1];
	FILE*  fp;
	char   ip[INET6_ADDRSTRLEN];
	time_t now = time(NULL);

	SAFEPRINTF(fname, "%shack.log", cfg->logs_dir);

	if ((fp = fopenlog(cfg, fname)) == NULL)
		return false;

	inet_addrtop(addr, ip, sizeof(ip));
	fprintf(fp, "SUSPECTED %s HACK ATTEMPT for user '%s' on %.24s%sUsing port %u at %s [%s]%s"
	        , prot
	        , user
	        , timestr(cfg, (time32_t)now, tstr)
	        , log_line_ending
	        , inet_addrport(addr)
	        , host
	        , ip
	        , log_line_ending
	        );
	if (text != NULL)
		fprintf(fp, "Details: %s%s", text, log_line_ending);
	fputs(log_line_ending, fp);
	fcloselog(fp);

	if (mqtt != NULL) {
		char topic[128];
		char str[1024];
		if (text == NULL)
			text = "";
		snprintf(str, sizeof(str), "%s\t%u\t%s\t%s\t%s", user, inet_addrport(addr), host, ip, text);
		snprintf(topic, sizeof(topic), "hack/%s", prot);
		strlwr(topic);
		mqtt_pub_timestamped_msg(mqtt, TOPIC_BBS_ACTION, topic, now, str);
	}

	return true;
}

bool sbbs_t::hacklog(const char* prot, const char* text)
{
	return ::hacklog(&cfg, mqtt, prot, useron.alias, text, client_name, &client_addr);
}

extern "C" bool spamlog(scfg_t* cfg, struct mqtt* mqtt, char* prot, char* action
                        , char* reason, char* host, char* ip_addr
                        , char* to, char* from)
{
	char   to_user[256];
	char   tstr[64];
	char   fname[MAX_PATH + 1];
	FILE*  fp;
	time_t now = time(NULL);

	SAFEPRINTF(fname, "%sspam.log", cfg->logs_dir);

	if ((fp = fopenlog(cfg, fname)) == NULL)
		return false;

	if (to == NULL)
		to_user[0] = 0;
	else
		SAFEPRINTF(to_user, "to: %.128s", to);

	if (from == NULL)
		from = host;

	fprintf(fp, "SUSPECTED %s SPAM %s on %.24s%sHost: %s [%s]%sFrom: %.128s %s%s"
	        , prot
	        , action
	        , timestr(cfg, (time32_t)now, tstr)
	        , log_line_ending
	        , host
	        , ip_addr
	        , log_line_ending
	        , from
	        , to_user
	        , log_line_ending
	        );
	if (reason != NULL)
		fprintf(fp, "Reason: %s%s", reason, log_line_ending);
	fputs(log_line_ending, fp);
	fcloselog(fp);

	if (mqtt != NULL) {
		char str[1024];
		char topic[128];
		if (reason == NULL)
			reason = (char*)"";
		snprintf(str, sizeof(str), "%s\t%s\t%s\t%s\t%s\t%s", prot, host, ip_addr, from, to_user, reason);
		snprintf(topic, sizeof(topic), "spam/%s", action);
		strlwr(topic);
		mqtt_pub_timestamped_msg(mqtt, TOPIC_BBS_ACTION, topic, now, str);
	}

	return true;
}

extern "C" int errorlog(scfg_t* cfg, struct mqtt* mqtt, int level, const char* host, const char* text)
{
	FILE*  fp;
	char   buf[128];
	char   path[MAX_PATH + 1];
	time_t now = time(NULL);

	if (host == NULL)
		host = "";

	SAFEPRINTF(path, "%serror.log", cfg->logs_dir);
	if ((fp = fopenlog(cfg, path)) == NULL)
		return -1;
	fprintf(fp, "%.24s %s/%s %s%s%s%s%s"
	        , ctime_r(&now, buf)
	        , GIT_BRANCH
	        , GIT_HASH
	        , host
	        , log_line_ending
	        , text
	        , log_line_ending
	        , log_line_ending
	        );
	fcloselog(fp);
	if (cfg->erruser && level <= cfg->errlevel) {
		char subject[128];
		SAFEPRINTF2(subject, "%s %sERROR occurred", host, level <= LOG_CRIT ? "CRITICAL " : "");
		notify(cfg, cfg->erruser, subject, text);
	}
	mqtt_errormsg(mqtt, level, text);

	return 0;
}

void sbbs_t::logentry(const char *code, const char *entry)
{
	char str[512];

	now = time(NULL);
	SAFEPRINTF4(str, "Node %2d  %s%s   %s", cfg.node_num, timestr(now), log_line_ending, entry);
	logline(code, str);
}

/****************************************************************************/
/* Writes 'str' verbatim into node.log 										*/
/****************************************************************************/
void sbbs_t::log(const char *str)
{
	if (logfile_fp == NULL || online == ON_LOCAL)
		return;
	if (logcol >= 78 || (logcol > 1 && (78 - logcol) < strlen(str))) {
		fputs(log_line_ending, logfile_fp);
		logcol = 1;
	}
	if (logcol == 1) {
		fputs("   ", logfile_fp);
		logcol = 4;
	}
	fputs(str, logfile_fp);
	if (*lastchar(str) == LF) {
		logcol = 1;
		fflush(logfile_fp);
	}
	else
		logcol += strlen(str);
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
	if (strchr(str, '\n') == NULL)  // Keep the console log pretty
		lputs(level, str);
	if (logfile_fp == NULL || (online == ON_LOCAL && strcmp(code, "!!")))
		return;
	if (logcol != 1)
		fputs(log_line_ending, logfile_fp);
	fprintf(logfile_fp, "%-2.2s %s%s", code, str, log_line_ending);
	logcol = 1;
	fflush(logfile_fp);
}

/****************************************************************************/
/* Writes formatted string on it's own line in node.log						*/
/****************************************************************************/
void sbbs_t::llprintf(int level, const char* code, const char *fmt, ...)
{
	va_list argptr;
	char    sbuf[1024];

	va_start(argptr, fmt);
	vsnprintf(sbuf, sizeof sbuf, fmt, argptr);
	TERMINATE(sbuf);
	va_end(argptr);
	logline(level, code, sbuf);
}

/****************************************************************************/
/* Writes a comma then 'ch' to log, tracking column.						*/
/****************************************************************************/
void sbbs_t::logch(char ch, bool comma)
{

	if (logfile_fp == NULL || (online == ON_LOCAL))
		return;
	if ((uchar)ch < ' ')   /* Don't log control chars */
		return;
	if (logcol == 1) {
		logcol = 4;
		fprintf(logfile_fp, "   ");
	}
	else if (logcol >= 78) {
		logcol = 4;
		fprintf(logfile_fp, "%s   ", log_line_ending);
	}
	if (comma && logcol != 4) {
		fprintf(logfile_fp, ",");
		logcol++;
	}
	if (ch & 0x80) {
		ch &= 0x7f;
		if (ch < ' ')
			return;
		fprintf(logfile_fp, "/");
	}
	fprintf(logfile_fp, "%c", ch);
	logcol++;
}

/****************************************************************************/
/* Error handling routine. Prints to local and remote screens the error     */
/* information, function, action, object and access and then attempts to    */
/* write the error information into the file ERROR.LOG and NODE.LOG         */
/****************************************************************************/
void sbbs_t::errormsg(int line, const char* function, const char *src, const char* action, const char *object
                      , int access, const char *extinfo)
{
	char          repeat[128] = "";
	char          errno_str[128];
	char          errno_info[256] = "";
	static time_t lasttime;
	uint          repeat_count;

	/* prevent recursion */
	if (errormsg_inside)
		return;
	errormsg_inside = true;
	now = time(NULL);

	if (errno != 0 && strcmp(action, ERR_CHK) != 0)
		safe_snprintf(errno_info, sizeof(errno_info), "%d (%s) "
	#ifdef _WIN32
		              "(WinError %u) "
	#endif
		              , errno, safe_strerror(errno, errno_str, sizeof(errno_str))
	#ifdef _WIN32
		              , GetLastError()
	#endif
		              );

	int level = LOG_ERR;
	if ((repeat_count = repeated_error(line, function)) > 0) {
		snprintf(repeat, sizeof repeat, "[x%u]", repeat_count + 1);
		// De-duplicate by reducing severity of log messages
		if ((now - lasttime) < 12 * 60 * 60)
			level = LOG_WARNING;
	}
	lasttime = now;
	lprintf(level, "!ERROR%s %s"
	        "in %s line %u (%s) %s \"%s\" access=%d %s%s"
	        , repeat
	        , errno_info
	        , src, line, function, action, object, access
	        , extinfo == NULL ? "":"info="
	        , extinfo == NULL ? "":extinfo);

	if (online == ON_REMOTE) {
		int savatr = curatr;
		attr(cfg.color[clr_err]);
		bprintf("\7\r\n!ERROR%s %s %s\r\n", repeat, action, object);   /* tell user about error */
		bputs("\r\nThe sysop has been notified.\r\n");
		pause();
		attr(savatr);
		CRLF;
	}
	if (repeat_count == 0 && cfg.node_num > 0) {
		if (getnodedat(cfg.node_num, &thisnode, true)) {
			if (thisnode.errors < UCHAR_MAX)
				thisnode.errors++;
			criterrs = thisnode.errors;
			putnodedat(cfg.node_num, &thisnode);
		}
	}

	if (logfile_fp != NULL) {
		fprintf(logfile_fp, "%s!! ERROR%s %s %s%s"
		        , logcol == 1 ? "" : log_line_ending
		        , repeat
		        , action
		        , object
		        , log_line_ending);
		logcol = 1;
		fflush(logfile_fp);
	}

	errormsg_inside = false;
}

/****************************************************************************/
/* Open a log file for append, supporting log rotation based on size		*/
/****************************************************************************/
extern "C" FILE* fopenlog(scfg_t* cfg, const char* path)
{
	const int mode = O_WRONLY | O_CREAT | O_APPEND | O_DENYNONE;
	int       file;
	FILE*     fp;

	if ((fp = fnopen(&file, path, mode)) == NULL)
		return NULL;

	if (cfg->max_log_size && cfg->max_logs_kept && filelength(file) >= (off_t)cfg->max_log_size) {
#ifdef _WIN32 // Can't rename an open file on Windows
		fclose(fp);
#endif
		backup(path, cfg->max_logs_kept, /* rename: */ TRUE);
#ifndef _WIN32
		fclose(fp);
#endif
		if ((fp = fnopen(NULL, path, mode)) == NULL)
			return NULL;
	}
	return fp;
}

/****************************************************************************/
// Write a line to a log file, with date/timestamp and newline terminator
// May close the file if reached max size
// Returns number of characters written
/****************************************************************************/
extern "C" size_t fprintlog(scfg_t* cfg, FILE** fp, const char* str)
{
	struct tm tm{};
	time_t now = time(NULL);
	localtime_r(&now, &tm);
	size_t result = fprintf(*fp, "%u-%02u-%02u %02u:%02u:%02u  %s\n"
		, 1900 + tm.tm_year, tm.tm_mon + 1, tm.tm_mday
		, tm.tm_hour, tm.tm_min, tm.tm_sec
		, str);
	fflush(*fp);
	if (cfg->max_log_size && ftell(*fp) >= (off_t)cfg->max_log_size) {
		fclose(*fp);
		*fp = NULL;
	}
	return result;
}

extern "C" void fcloselog(FILE* fp)
{
	fclose(fp);
}
