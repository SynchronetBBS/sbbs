/* Synchronet FidoNet EchoMail Scanning/Tossing and NetMail Tossing Utility */

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

/* Portions written by Allen Christiansen 1994-1996 						*/

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#if defined(__unix__)
	#include <signal.h>
#endif

#include "conwrap.h"		/* getch() */
#include "load_cfg.h"		/* load_cfg() */
#include "smblib.h"
#include "scfglib.h"
#include "sbbsecho.h"
#include "genwrap.h"		/* PLATFORM_DESC */
#include "xpendian.h"
#include "utf8.h"
#include "date_str.h"
#include "link_list.h"
#include "str_list.h"
#include "str_util.h"
#include "datewrap.h"
#include "nopen.h"
#include "crc32.h"
#include "userdat.h"
#include "filedat.h"
#include "msg_id.h"
#include "scfgsave.h"
#include "getmail.h"
#include "git_branch.h"
#include "git_hash.h"

#define MAX_OPEN_SMBS	10

smb_t *smb,*email;
bool opt_import_packets		= true;
bool opt_import_netmail		= true;
bool opt_delete_netmail		= true;		/* delete after importing (no effect on exported netmail) */
bool opt_import_echomail	= true;
bool opt_export_echomail	= true;
bool opt_export_netmail		= true;
bool opt_pack_netmail		= true;
bool opt_gen_notify_list	= false;
bool opt_export_ftn_echomail= false;	/* Export msgs previously imported from FidoNet */
bool opt_update_msgptrs		= false;
bool opt_ignore_msgptrs		= false;
bool opt_leave_msgptrs		= false;
bool opt_dump_area_file		= false;
bool opt_retoss_badmail		= false;	/* Re-toss from the badecho/unknown msg sub */

/* statistics */
ulong netmail=0;	/* imported */
ulong echomail=0;	/* imported */
ulong exported_netmail=0;
ulong exported_echomail=0;
ulong packed_netmail=0;
ulong packets_sent=0;
ulong packets_imported=0;
ulong bundles_sent=0;
ulong bundles_unpacked=0;

int cur_smb=0;
FILE *fidologfile=NULL;
str_list_t twit_list;
str_list_t bad_areas;

fidoaddr_t		sys_faddr = {1,1,1,0};		/* Default system address: 1:1/1.0 */
sbbsecho_cfg_t	cfg;
scfg_t			scfg;
char			compiler[32];

bool pause_on_exit=false;
bool pause_on_abend=false;
bool mtxfile_locked=false;
bool terminated=false;

str_list_t	locked_bso_nodes;

int lprintf(int level, char *fmt, ...)
#if defined(__GNUC__)   // Catch printf-format errors
    __attribute__ ((format (printf, 2, 3)));
#endif
;
int mv(const char *insrc, const char *indest, bool copy);
time32_t fmsgtime(const char *str);
void export_echomail(const char *sub_code, const nodecfg_t*, bool rescan);
const char* area_desc(const char* areatag);

/* FTN-compliant "Program Identifier"/PID (also used as a "Tosser Identifier"/TID) */
const char* sbbsecho_pid(void)
{
	static char str[256];

	sprintf(str, "SBBSecho %u.%02u-%s %s/%s %s %s"
		,SBBSECHO_VERSION_MAJOR,SBBSECHO_VERSION_MINOR,PLATFORM_DESC,GIT_BRANCH,GIT_HASH,__DATE__,compiler);

	return str;
}

/* for *.bsy file contents: */
const char* program_id(void)
{
	static char str[256];

	SAFEPRINTF2(str, "%u %s", getpid(), sbbsecho_pid());

	return str;
}

const char* tear_line(char ch)
{
	static char str[256];

	sprintf(str,"%c%c%c SBBSecho %u.%02u-%s\r", ch, ch, ch
		,SBBSECHO_VERSION_MAJOR,SBBSECHO_VERSION_MINOR,PLATFORM_DESC);

	return str;
}

const char default_domain[] = "fidonet";

const char* zone_domain(uint16_t zone)
{
	for(unsigned i = 0; i < cfg.domain_count; i++)
		for(unsigned j = 0; j < cfg.domain_list[i].zone_count; j++)
			if(cfg.domain_list[i].zone_list[j] == zone)
				return cfg.domain_list[i].name;

	return default_domain;
}

const char* zone_root_outbound(uint16_t zone)
{
	for(unsigned i = 0; i < cfg.domain_count; i++)
		for(unsigned j = 0; j < cfg.domain_list[i].zone_count; j++)
			if(cfg.domain_list[i].zone_list[j] == zone)
				return cfg.domain_list[i].root;

	return cfg.outbound;
}

/* Allocates the buffer and returns the data (or NULL) */
char* parse_control_line(const char* fmsgbuf, const char* kludge)
{
	char*	p;
	char	str[128];

	if(fmsgbuf == NULL)
		return NULL;
	SAFEPRINTF(str, "\1%s", kludge);
	p = strstr(fmsgbuf, str);
	if(p == NULL)
		return NULL;
	if(p != fmsgbuf && *(p-1) != '\r' && *(p-1) != '\n')	/* Kludge must start new-line */
		return NULL;
	SAFECOPY(str, p);

	/* Terminate at the CR */
	p = strchr(str, '\r');
	if(p == NULL)
		return NULL;
	*p = 0;

	/* Only return the data portion */
	p = str + strlen(kludge) + 1;
	SKIP_WHITESPACE(p);
	return strdup(p);
}

/* Write an FTS-4009 compliant "Via" control line to the (netmail) file */
int fwrite_via_control_line(FILE* fp, fidoaddr_t* addr)
{
	time_t t = time(NULL);
	struct tm* tm = gmtime(&t);
	return fprintf(fp,"\1Via %s @%04u%02u%02u.%02u%02u%02u.UTC "
		"SBBSecho %u.%02u-%s %s/%s\r"
		,smb_faddrtoa(addr, NULL)
		,tm->tm_year+1900
		,tm->tm_mon+1
		,tm->tm_mday
		,tm->tm_hour
		,tm->tm_min
		,tm->tm_sec
		,SBBSECHO_VERSION_MAJOR,SBBSECHO_VERSION_MINOR,PLATFORM_DESC,GIT_BRANCH,GIT_HASH);
}

int fwrite_intl_control_line(FILE* fp, fmsghdr_t* hdr)
{
	return fprintf(fp,"\1INTL %hu:%hu/%hu %hu:%hu/%hu\r"
		,hdr->destzone,hdr->destnet,hdr->destnode
		,hdr->origzone,hdr->orignet,hdr->orignode);
}

typedef struct echostat_msg {
	char msg_id[128];
	char reply_id[128];
	char pid[128];
	char tid[128];
	char to[FIDO_NAME_LEN];
	char from[FIDO_NAME_LEN];
	char subj[FIDO_SUBJ_LEN];
	char msg_tz[128];
	time_t msg_time;
	time_t localtime;
	size_t length;
	fidoaddr_t origaddr;
	fidoaddr_t pkt_orig;
} echostat_msg_t;

enum echostat_msg_type {
	ECHOSTAT_MSG_RECEIVED,
	ECHOSTAT_MSG_IMPORTED,
	ECHOSTAT_MSG_EXPORTED,
	ECHOSTAT_MSG_DUPLICATE,
	ECHOSTAT_MSG_CIRCULAR,
	ECHOSTAT_MSG_TYPES
};

const char* echostat_msg_type[ECHOSTAT_MSG_TYPES] = {
	"Received",
	"Imported",
	"Exported",
	"Duplicate",
	"Circular",
};

typedef struct echostat {
	char tag[FIDO_AREATAG_LEN + 1];
	bool known;	// listed in Area File
	echostat_msg_t last[ECHOSTAT_MSG_TYPES];
	echostat_msg_t first[ECHOSTAT_MSG_TYPES];
	ulong total[ECHOSTAT_MSG_TYPES];
} echostat_t;

echostat_t* echostat;
size_t echostat_count;

int echostat_compare(const void* c1, const void* c2)
{
	echostat_t* stat1 = (echostat_t*)c1;
	echostat_t* stat2 = (echostat_t*)c2;

	return stricmp(stat1->tag, stat2->tag);
}

echostat_msg_t parse_echostat_msg(str_list_t ini, const char* section, const char* prefix)
{
	char str[128];
	char key[128];
	echostat_msg_t msg = {{0}};

	sprintf(key, "%s.to", prefix),			iniGetString(ini, section, key, NULL, msg.to);
	sprintf(key, "%s.from", prefix),		iniGetString(ini, section, key, NULL, msg.from);
	sprintf(key, "%s.subj", prefix),		iniGetString(ini, section, key, NULL, msg.subj);
	sprintf(key, "%s.msg_id", prefix),		iniGetString(ini, section, key, NULL, msg.msg_id);
	sprintf(key, "%s.reply_id", prefix),	iniGetString(ini, section, key, NULL, msg.reply_id);
	sprintf(key, "%s.pid", prefix),			iniGetString(ini, section, key, NULL, msg.pid);
	sprintf(key, "%s.tid", prefix),			iniGetString(ini, section, key, NULL, msg.tid);
	sprintf(key, "%s.msg_tz", prefix),		iniGetString(ini, section, key, NULL, msg.msg_tz);
	sprintf(key, "%s.msg_time", prefix),	msg.msg_time = iniGetDateTime(ini, section, key, 0);
	sprintf(key, "%s.localtime", prefix),	msg.localtime = iniGetDateTime(ini, section, key, 0);
	sprintf(key, "%s.length", prefix),		msg.length = (size_t)iniGetBytes(ini, section, key, 1, 0);
	sprintf(key, "%s.origaddr", prefix),	iniGetString(ini, section, key, NULL, str);
	if(str[0])	 msg.origaddr = atofaddr(str);
	sprintf(key, "%s.pkt_orig", prefix),	iniGetString(ini, section, key, NULL, str);
	if(str[0])	 msg.pkt_orig = atofaddr(str);

	return msg;
}

echostat_msg_t fidomsg_to_echostat_msg(fmsghdr_t* hdr, fidoaddr_t* pkt_orig, const char* fmsgbuf)
{
	char* p;
	echostat_msg_t msg = {{0}};

	SAFECOPY(msg.to		, hdr->to);
	SAFECOPY(msg.from	, hdr->from);
	SAFECOPY(msg.subj	, hdr->subj);
	msg.msg_time		= fmsgtime(hdr->time);
	msg.localtime		= time(NULL);
	msg.origaddr.zone	= hdr->origzone;
	msg.origaddr.net	= hdr->orignet;
	msg.origaddr.node	= hdr->orignode;
	msg.origaddr.point	= hdr->origpoint;
	if(pkt_orig != NULL)
		msg.pkt_orig	= *pkt_orig;
	if((p = parse_control_line(fmsgbuf, "MSGID:")) != NULL) {
		SAFECOPY(msg.msg_id, p);
		free(p);
	}
	if((p = parse_control_line(fmsgbuf, "REPLY:")) != NULL) {
		SAFECOPY(msg.reply_id, p);
		free(p);
	}
	if((p = parse_control_line(fmsgbuf, "PID:")) != NULL) {
		SAFECOPY(msg.pid, p);
		free(p);
	}
	if((p = parse_control_line(fmsgbuf, "TID:")) != NULL) {
		SAFECOPY(msg.tid, p);
		free(p);
	}
	if((p = parse_control_line(fmsgbuf, "TZUTC:")) != NULL
		|| (p = parse_control_line(fmsgbuf, "TZUTCINFO:")) != NULL) {
		SAFECOPY(msg.msg_tz, p);
		free(p);
	}
	if(fmsgbuf != NULL)
		msg.length = strlen(fmsgbuf);

	return msg;
}

echostat_msg_t smsg_to_echostat_msg(const smbmsg_t* smsg, size_t msglen, fidoaddr_t addr)
{
	char* p;
	echostat_msg_t emsg = {{0}};

	SAFECOPY(emsg.to	, smsg->to);
	SAFECOPY(emsg.from	, smsg->from);
	SAFECOPY(emsg.subj	, smsg->subj);
	emsg.msg_time		= smsg->hdr.when_written.time;
	emsg.localtime		= time(NULL);
	SAFECOPY(emsg.msg_tz, smb_zonestr(smsg->hdr.when_written.zone, NULL));
	if(smsg->from_net.type == NET_FIDO && smsg->from_net.addr != NULL)
		emsg.origaddr	= *(fidoaddr_t*)smsg->from_net.addr;
	if((p = smsg->ftn_msgid) != NULL)
		SAFECOPY(emsg.msg_id, p);
	if((p = smsg->ftn_reply) != NULL)
		SAFECOPY(emsg.reply_id, p);
	if((p = smsg->ftn_pid) != NULL)
		SAFECOPY(emsg.pid, p);
	if((p = smsg->ftn_tid) != NULL)
		SAFECOPY(emsg.tid, p);
	else
		SAFECOPY(emsg.tid, sbbsecho_pid());
	emsg.length = msglen;
	emsg.pkt_orig = addr;

	return emsg;
}

void new_echostat_msg(echostat_t* stat, enum echostat_msg_type type, echostat_msg_t msg)
{
	stat->last[type] = msg;
	if(stat->first[type].localtime == 0)
		stat->first[type] = msg;
	stat->total[type]++;
}

size_t read_echostats(const char* fname, echostat_t **echostat)
{
	str_list_t ini;
	FILE* fp = iniOpenFile(fname, FALSE);
	if(fp == NULL)
		return 0;

	ini = iniReadFile(fp);
	iniCloseFile(fp);

	str_list_t echoes = iniGetSectionList(ini, NULL);
	if(echoes == NULL) {
		iniFreeStringList(ini);
		return 0;
	}

	size_t echo_count = strListCount(echoes);
	*echostat = malloc(sizeof(echostat_t) * echo_count);
	if(*echostat == NULL) {
		iniFreeStringList(echoes);
		iniFreeStringList(ini);
		return 0;
	}
	for(unsigned i = 0; i < echo_count; i++) {
		echostat_t* stat = &(*echostat)[i];
		SAFECOPY(stat->tag, echoes[i]);
		str_list_t keys = iniGetSection(ini, stat->tag);
		if(keys == NULL)
			continue;
		for(int type = 0; type < ECHOSTAT_MSG_TYPES; type++) {
			char prefix[32];
			sprintf(prefix, "First%s", echostat_msg_type[type])
				,stat->first[type]	= parse_echostat_msg(keys, NULL, prefix);
			sprintf(prefix, "Last%s", echostat_msg_type[type])
				,stat->last[type]	= parse_echostat_msg(keys, NULL, prefix);
			sprintf(prefix, "Total%s", echostat_msg_type[type])
				,stat->total[type] = iniGetLongInt(keys, NULL, prefix, 0);
		}
		stat->known = iniGetBool(keys, NULL, "Known", false);
		iniFreeStringList(keys);
	}
	iniFreeStringList(echoes);
	iniFreeStringList(ini);

	lprintf(LOG_DEBUG, "Read %lu echo statistics from %s", (ulong)echo_count, fname);
	return echo_count;
}

/* Mimic iniSetDateTime() */
const char* iniTimeStr(time_t t)
{
	static char	tstr[32];
	char	tmp[32];
	char*	p;

	if(t == 0)
		return "Never";
	if((p = ctime_r(&t, tmp)) == NULL)
		sprintf(tstr, "0x%lx", (ulong)t);
	else
		sprintf(tstr, "%.3s %.2s %.4s %.8s", p+4, p+8, p+20, p+11);

	return tstr;
}

void fwrite_echostat_msg(FILE* fp, const echostat_msg_t* msg, const char* prefix)
{
	echostat_msg_t zero = {{0}};
	if(memcmp(msg, &zero, sizeof(*msg)) == 0)
		return;
	if(msg->to[0])			fprintf(fp, "%s.to = %s\n"		, prefix, msg->to);
	if(msg->from[0])		fprintf(fp, "%s.from = %s\n"	, prefix, msg->from);
	if(msg->subj[0])		fprintf(fp, "%s.subj = %s\n"	, prefix, msg->subj);
	if(msg->msg_id[0])		fprintf(fp, "%s.msg_id = %s\n"	, prefix, msg->msg_id);
	if(msg->reply_id[0])	fprintf(fp, "%s.reply_id = %s\n", prefix, msg->reply_id);
	if(msg->pid[0])			fprintf(fp, "%s.pid = %s\n"		, prefix, msg->pid);
	if(msg->tid[0])			fprintf(fp, "%s.tid = %s\n"		, prefix, msg->tid);
	fprintf(fp, "%s.length = %lu\n"							, prefix, (ulong)msg->length);
	fprintf(fp, "%s.msg_time = %s\n"						, prefix, iniTimeStr(msg->msg_time));
	if(msg->msg_tz[0])		fprintf(fp, "%s.msg_tz = %s\n"	, prefix, msg->msg_tz);
	fprintf(fp, "%s.localtime = %s\n"						, prefix, iniTimeStr(msg->localtime));
	if(msg->origaddr.zone)
		fprintf(fp, "%s.origaddr = %s\n"					, prefix, faddrtoa(&msg->origaddr));
	fprintf(fp, "%s.pkt_orig = %s\n"						, prefix, faddrtoa(&msg->pkt_orig));
}

void fwrite_echostat(FILE* fp, echostat_t* stat)
{
	const char* desc = area_desc(stat->tag);
	fprintf(fp, "[%s]\n"						, stat->tag);
	fprintf(fp,	"Known = %s\n"					, stat->known ? "true" : "false");
	if(desc != NULL)
		fprintf(fp, "Title = %s\n"				, desc);
	for(int type = 0; type < ECHOSTAT_MSG_TYPES; type++) {
		char prefix[32];
		sprintf(prefix, "First%s", echostat_msg_type[type])	, fwrite_echostat_msg(fp, &stat->first[type], prefix);
		sprintf(prefix, "Last%s", echostat_msg_type[type])	, fwrite_echostat_msg(fp, &stat->last[type], prefix);
		if(stat->total[type] != 0)
			fprintf(fp,	"Total%s = %lu\n"					, echostat_msg_type[type], stat->total[type]);
	}
}

bool write_echostats(const char* fname, echostat_t* echostat, size_t echo_count)
{
	FILE* fp;

	qsort(echostat, echo_count, sizeof(echostat_t), echostat_compare);

	if((fp=fopen(fname, "w"))==NULL)
		return false;

	for(unsigned i = 0; i < echo_count; i++) {
		fwrite_echostat(fp, &echostat[i]);
		fprintf(fp, "\n");
	}
	fclose(fp);
	return true;
}

echostat_t* get_echostat(const char* tag, bool create)
{
	for(unsigned int i = 0; i < echostat_count; i++) {
		if(stricmp(echostat[i].tag, tag) == 0)
			return &echostat[i];
	}
	if(!create)
		return NULL;
	echostat_t* new_echostat = realloc(echostat, sizeof(echostat_t) * (echostat_count + 1));
	if(new_echostat == NULL)
		return NULL;
	echostat = new_echostat;
	memset(&echostat[echostat_count], 0, sizeof(echostat_t));
	SAFECOPY(echostat[echostat_count].tag, tag);

	return &echostat[echostat_count++];
}

/**********************/
/* Log print function */
/**********************/
int lprintf(int level, char *fmt, ...)
{
	va_list argptr;
	char sbuf[1024];
	int chcount;

	va_start(argptr,fmt);
	chcount=vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
	va_end(argptr);
	truncsp(sbuf);
	printf("%s\n",sbuf);

	if(fidologfile!=NULL && level<=cfg.log_level) {
	    time_t now = time(NULL);
		struct tm *tm;
		struct tm tmbuf = {0};
		char timestamp[128];
		strip_ctrl(sbuf, sbuf);
		if((tm = localtime(&now)) == NULL)
			tm = &tmbuf;
		if(strftime(timestamp, sizeof(timestamp), cfg.logtime, tm) <= 0)
			snprintf(timestamp, sizeof(timestamp)-1, "%u-%02u-%02u %02u:%02u:%02u"
				,1900+tm->tm_year, tm->tm_mon+1, tm->tm_mday
				,tm->tm_hour, tm->tm_min, tm->tm_sec);
		fprintf(fidologfile, "%s %s\n", timestamp, sbuf);
		fflush(fidologfile);
	}
	return(chcount);
}

bool delfile(const char *filename, int line)
{
	lprintf(LOG_DEBUG, "Deleting %s (from line %u)", filename, line);
	if(remove(filename) != 0) {
		lprintf(LOG_ERR, "ERROR %u (%s) line %u removing file %s"
			,errno, strerror(errno), line, filename);
		return false;
	}
	return true;
}

/****************************************************************************/
/* Runs an external program directly using system()							*/
/****************************************************************************/
int execute(char *cmdline)
{
	int retval;

	lprintf(LOG_DEBUG, "Executing: %s", cmdline);
	if((retval = system(cmdline)) != 0)
		lprintf(LOG_ERR,"ERROR executing '%s' system returned: %d, errno: %d (%s)"
			,cmdline, retval, errno, strerror(errno));

	return retval;
}

/******************************************************************************
 Returns the local system address that best correlates with the passed address
******************************************************************************/
fidoaddr_t getsysfaddr(fidoaddr_t addr)
{
	nodecfg_t*	nodecfg;

	nodecfg = findnodecfg(&cfg, addr, /* exact */FALSE);
	if(nodecfg != NULL && nodecfg->local_addr.zone)
		return nodecfg->local_addr;

	int i;
	for(i=0; i<scfg.total_faddrs && addr.net != 0; i++)
		if(scfg.faddr[i].zone == addr.zone && scfg.faddr[i].net == addr.net)
			return scfg.faddr[i];
	for(i=0; i<scfg.total_faddrs; i++)
		if(scfg.faddr[i].zone == addr.zone)
			return scfg.faddr[i];
	return sys_faddr;
}

int get_outbound(fidoaddr_t dest, char* outbound, size_t maxlen, bool fileboxes)
{
	nodecfg_t*	nodecfg;

	strncpy(outbound,zone_root_outbound(dest.zone),maxlen);
	if(fileboxes &&
		(nodecfg = findnodecfg(&cfg, dest, /* exact */true)) != NULL
		&& nodecfg->outbox[0])
		strncpy(outbound, nodecfg->outbox, maxlen);
	else if(cfg.flo_mailer) {
		if(dest.zone != sys_faddr.zone)	{ /* Inter-zone outbound is "OUTBOUND.ZZZ/" */
			char* p = lastchar(outbound);
			if(IS_PATH_DELIM(*p))
				*p = 0;
			safe_snprintf(outbound+strlen(outbound), maxlen,".%03x", dest.zone);
		}
		if(dest.point != 0) {			/* Point destination is "OUTBOUND[.ZZZ]/NNNNnnnn.pnt/" */
			backslash(outbound);
			safe_snprintf(outbound+strlen(outbound), maxlen, "%04x%04x.pnt", dest.net, dest.node);
		}
	}
	backslash(outbound);
	if(isdir(outbound))
		return 0;
	lprintf(LOG_DEBUG, "Creating outbound directory for %s: %s", smb_faddrtoa(&dest, NULL), outbound);
	return mkpath(outbound);
}

const char* get_current_outbound(fidoaddr_t dest, bool fileboxes)
{
	static char outbound[MAX_PATH+1];
	if(get_outbound(dest, outbound, sizeof(outbound)-1, fileboxes) != 0) {
		lprintf(LOG_ERR, "Error creating outbound directory");
		return NULL;
	}
	return outbound;
}

bool bso_lock_node(fidoaddr_t dest)
{
	const char* outbound;
	char fname[MAX_PATH+1];

	if(!cfg.flo_mailer)
		return true;

	outbound = get_current_outbound(dest, /* fileboxes: */false);
	if(outbound == NULL)
		return false;

	if(dest.point)
		SAFEPRINTF2(fname,"%s%08x.bsy",outbound,dest.point);
	else
		SAFEPRINTF3(fname,"%s%04x%04x.bsy",outbound,dest.net,dest.node);

	if(strListFind(locked_bso_nodes, fname, /* case_sensitive: */true) >= 0)
		return true;
	for(unsigned attempt=0;;) {
		if(fmutex(fname, program_id(), cfg.bsy_timeout))
			break;
		lprintf(LOG_NOTICE, "Node (%s) externally locked via: %s", smb_faddrtoa(&dest, NULL), fname);
		if(++attempt >= cfg.bso_lock_attempts) {
			lprintf(LOG_WARNING, "Giving up after %u attempts to lock node %s", attempt, smb_faddrtoa(&dest, NULL));
			return false;
		}
		if(terminated)
			return false;
		SLEEP(cfg.bso_lock_delay*1000);
		(void)mkpath(outbound); // Just in case something (e.g. Argus) deleted the outbound directory
	}
	strListPush(&locked_bso_nodes, fname);
	lprintf(LOG_DEBUG, "Node (%s) successfully locked via: %s", smb_faddrtoa(&dest, NULL), fname);
	return true;
}

const char* bso_flo_filename(fidoaddr_t dest, uint16_t attr)
{
	nodecfg_t* nodecfg;
	char ch='f';
	const char* outbound;
	static char filename[MAX_PATH+1];

	if(attr&FIDO_CRASH)
		ch='c';
	else if(attr&FIDO_HOLD)
		ch='h';
	else if((nodecfg=findnodecfg(&cfg, dest, /* exact: */false)) != NULL) {
		switch(nodecfg->status) {
			case MAIL_STATUS_CRASH:	ch='c';	break;
			case MAIL_STATUS_HOLD:	ch='h';	break;
			default:
				if(nodecfg->direct)
					ch='d';
				break;
		}
	}
	outbound = get_current_outbound(dest, /* fileboxes: */false);
	if(outbound == NULL)
		return NULL;

	if(dest.point)
		SAFEPRINTF3(filename,"%s%08x.%clo",outbound,dest.point,ch);
	else
		SAFEPRINTF4(filename,"%s%04x%04x.%clo",outbound,dest.net,dest.node,ch);
	return filename;
}

/******************************************************************************
 This function creates or appends on existing Binkley compatible .?LO file
 attach file.
 Returns 0 on success.
******************************************************************************/
int write_flofile(const char *infile, fidoaddr_t dest, bool bundle, bool use_outbox
									, bool del_file, uint16_t attr)
{
	const char* flo_filename;
	char attachment[MAX_PATH+1];
	char searchstr[MAX_PATH+1];
	char* p;
	FILE *fp;
	nodecfg_t* nodecfg;

	if(use_outbox && (nodecfg=findnodecfg(&cfg, dest, /* exact: */false)) != NULL) {
		if(nodecfg->outbox[0])
			return 0;
	}

	if(!bso_lock_node(dest))
		return 1;

	flo_filename = bso_flo_filename(dest, attr);
	if(flo_filename == NULL)
		return -2;

	if(*infile == '^')  /* work-around for BRE/FE inter-BBS attachment bug */
		infile++;

#ifdef __unix__
	if(IS_ALPHA(infile[0]) && infile[1] == ':')	// Ignore "C:" prefix
		infile += 2;
#endif
	SAFECOPY(attachment, infile);
	REPLACE_CHARS(attachment, '\\', '/', p);
	if(!fexistcase(attachment))	{ /* just in-case it's the wrong case for a Unix file system */
		lprintf(LOG_ERR, "ERROR line %u, attachment file not found: %s", __LINE__, attachment);
		return -1;
	}
	char* prefix = "";
	if(bundle) {
		prefix = (cfg.trunc_bundles) ? "#" : "^";
	} else {
		if(del_file)
			prefix = "^";
	}
	SAFEPRINTF2(searchstr, "%s%s", prefix, attachment);
	if(findstr(searchstr,flo_filename))	/* file already in FLO file */
		return 0;
	if((fp=fopen(flo_filename,"a"))==NULL) {
		lprintf(LOG_ERR,"ERROR %u (%s) opening %s",errno, strerror(errno), flo_filename);
		return -1;
	}
	fprintf(fp,"%s\n",searchstr);
	fclose(fp);
	lprintf(LOG_INFO, "File (%s, %1.1fKB) for %s added to BSO/FLO file: %s"
		,attachment, flength(attachment)/1024.0, smb_faddrtoa(&dest,NULL), flo_filename);
	return 0;
}

/* Writes text buffer to file, expanding sole LFs to CRLFs or stripping LFs */
size_t fwrite_crlf(const char* buf, size_t len, FILE* fp)
{
	char	ch,last_ch=0;
	size_t	i;
	size_t	wr=0;	/* total chars written (may be > len) */

	for(i=0;i<len;i++) {
		ch=*buf++;
		if(ch=='\n') {
			if(last_ch!='\r') {
				if(fputc('\r', fp) == EOF)
					break;
				wr++;
			}
			if(cfg.strip_lf)
				continue;
		}
		if(fputc(ch,fp)==EOF)
			break;
		wr++;
		last_ch=ch;
	}

	return(wr);
}

bool fidoctrl_line_exists(const smbmsg_t* msg, const char* prefix)
{
	if(msg==NULL || prefix==NULL)
		return false;
	for(int i=0; i<msg->total_hfields; i++) {
		if(msg->hfield[i].type == FIDOCTRL
			&& strncmp((char*)msg->hfield_dat[i], prefix, strlen(prefix)) == 0)
			return true;
	}
	return false;
}

fidoaddr_t fmsghdr_srcaddr(const fmsghdr_t* hdr)
{
	fidoaddr_t addr;

	addr.zone	= hdr->origzone;
	addr.net	= hdr->orignet;
	addr.node	= hdr->orignode;
	addr.point	= hdr->origpoint;

	return addr;
}

const char* fmsghdr_srcaddr_str(const fmsghdr_t* hdr)
{
	static char buf[64];
	fidoaddr_t addr = fmsghdr_srcaddr(hdr);

	return smb_faddrtoa(&addr, buf);
}

fidoaddr_t fmsghdr_destaddr(const fmsghdr_t* hdr)
{
	fidoaddr_t addr;

	addr.zone	= hdr->destzone;
	addr.net	= hdr->destnet;
	addr.node	= hdr->destnode;
	addr.point	= hdr->destpoint;

	return addr;
}

const char* fmsghdr_destaddr_str(const fmsghdr_t* hdr)
{
	static char buf[64];
	fidoaddr_t addr = fmsghdr_destaddr(hdr);

	return smb_faddrtoa(&addr, buf);
}

bool parse_origin(const char* fmsgbuf, fmsghdr_t* hdr)
{
	char* p;
	fidoaddr_t origaddr;

	if((p = strstr(fmsgbuf, FIDO_ORIGIN_PREFIX_FORM_1)) == NULL)
		p = strstr(fmsgbuf, FIDO_ORIGIN_PREFIX_FORM_2);
	if(p == NULL)
		return false;

	p += FIDO_ORIGIN_PREFIX_LEN;
	p = strrchr(p, '(');
	if(p == NULL)
		return false;
	p++;
	origaddr = atofaddr(p);
	if(origaddr.zone == 0 || faddr_contains_wildcard(&origaddr))
		return false;
	hdr->origzone	= origaddr.zone;
	hdr->orignet	= origaddr.net;
	hdr->orignode	= origaddr.node;
	hdr->origpoint	= origaddr.point;
	return true;
}

bool parse_pkthdr(const fpkthdr_t* hdr, fidoaddr_t* orig_addr, fidoaddr_t* dest_addr, enum pkt_type* pkt_type)
{
	fidoaddr_t	orig;
	fidoaddr_t	dest;
	enum pkt_type type = PKT_TYPE_2;

	if(hdr->type2.pkttype != 2)
		return false;

	orig.zone	= hdr->type2.origzone;
	orig.net	= hdr->type2.orignet;
	orig.node	= hdr->type2.orignode;
	orig.point	= 0;

	dest.zone	= hdr->type2.destzone;
	dest.net	= hdr->type2.destnet;
	dest.node	= hdr->type2.destnode;
	dest.point	= 0;				/* No point info in the 2.0 hdr! */

	if(hdr->type2plus.cword == BYTE_SWAP_16(hdr->type2plus.cwcopy)  /* 2e Packet Header (FSC-39) */
		&& (hdr->type2plus.cword&1)) {	/* Some call this a Type-2+ packet, but it's not (yet) FSC-48 conforming */
		type = PKT_TYPE_2_EXT;
		orig.point = hdr->type2plus.origpoint;
		dest.point = hdr->type2plus.destpoint;
		if(orig.zone == 0) orig.zone = hdr->type2plus.origzone;
		if(dest.zone == 0) dest.zone = hdr->type2plus.destzone;
		if(hdr->type2plus.auxnet != 0) {	/* strictly speaking, auxnet may be 0 and a valid 2+ packet */
			type = PKT_TYPE_2_PLUS;
			if(orig.point != 0 && orig.net == 0xffff) 	/* see FSC-0048 for details */
				orig.net = hdr->type2plus.auxnet;
		}
	} else if(hdr->type2_2.subversion==2) {					/* Type 2.2 Packet Header (FSC-45) */
		type = PKT_TYPE_2_2;
		orig.point = hdr->type2_2.origpoint;
		dest.point = hdr->type2_2.destpoint;
	}

	if(pkt_type != NULL)
		*pkt_type = type;
	if(dest_addr != NULL)
		*dest_addr = dest;
	if(orig_addr != NULL)
		*orig_addr = orig;

	return true;
}

bool new_pkthdr(fpkthdr_t* hdr, fidoaddr_t orig, fidoaddr_t dest, const nodecfg_t* nodecfg)
{
	enum pkt_type pkt_type = PKT_TYPE_2;
	struct tm* tm;
	time_t now = time(NULL);

	if(nodecfg != NULL)
		pkt_type = nodecfg->pkt_type;

	memset(hdr, 0, sizeof(fpkthdr_t));

	if((tm = localtime(&now)) != NULL) {
		hdr->type2.year	= tm->tm_year+1900;
		hdr->type2.month= tm->tm_mon;
		hdr->type2.day	= tm->tm_mday;
		hdr->type2.hour	= tm->tm_hour;
		hdr->type2.min	= tm->tm_min;
		hdr->type2.sec	= tm->tm_sec;
	}

	hdr->type2.origzone = orig.zone;
	hdr->type2.orignet	= orig.net;
	hdr->type2.orignode	= orig.node;
	hdr->type2.destzone	= dest.zone;
	hdr->type2.destnet	= dest.net;
	hdr->type2.destnode	= dest.node;

	hdr->type2.pkttype	= 2;
	hdr->type2.prodcode	= SBBSECHO_PRODUCT_CODE&0xff;
	hdr->type2.sernum	= SBBSECHO_VERSION_MAJOR;

	if(nodecfg != NULL && nodecfg->pktpwd[0] != 0)
		strncpy((char*)hdr->type2.password, nodecfg->pktpwd, sizeof(hdr->type2.password));

	if(pkt_type == PKT_TYPE_2)
		return true;

	if(pkt_type == PKT_TYPE_2_2) {
		hdr->type2_2.subversion = 2;	/* 2.2 */
		strncpy((char*)hdr->type2_2.origdomn,zone_domain(orig.zone),sizeof(hdr->type2_2.origdomn));
		strncpy((char*)hdr->type2_2.destdomn,zone_domain(dest.zone),sizeof(hdr->type2_2.destdomn));
		return true;
	}

	/* 2e and 2+ */
	if(pkt_type != PKT_TYPE_2_EXT && pkt_type != PKT_TYPE_2_PLUS) {
		lprintf(LOG_ERR, "UNSUPPORTED PACKET TYPE: %u", pkt_type);
		return false;
	}

	if(pkt_type == PKT_TYPE_2_PLUS) {
		if(orig.point != 0)
			hdr->type2plus.orignet	= 0xffff;
		hdr->type2plus.auxnet	= orig.net; /* Squish always copies the orignet here */
	}
	hdr->type2plus.cword		= 0x0001;
	hdr->type2plus.cwcopy		= 0x0100;
	hdr->type2plus.prodcodeHi	= SBBSECHO_PRODUCT_CODE>>8;
	hdr->type2plus.prodrevMinor	= SBBSECHO_VERSION_MINOR;
	hdr->type2plus.origzone		= orig.zone;
	hdr->type2plus.destzone		= dest.zone;
	hdr->type2plus.origpoint	= orig.point;
	hdr->type2plus.destpoint	= dest.point;

	return true;
}

/******************************************************************************
 This function will create a netmail message (FTS-1 "stored message" format).
 If file is non-zero, will set file attachment bit (for bundles).
 Returns 0 on success.
******************************************************************************/
int create_netmail(const char *to, const smbmsg_t* msg, const char *subject, const char *body, fidoaddr_t dest
	, fidoaddr_t* src)
{
	FILE *fp;
	char tmp[256];
	char fname[MAX_PATH+1];
	char* from=NULL;
	uint i;
	static uint startmsg;
	fidoaddr_t	faddr;
	fmsghdr_t hdr;
	time_t t;
	struct tm *tm;
	when_t when_written;
	nodecfg_t* nodecfg;
	bool	direct=false;

	if(msg==NULL) {
		when_written.time = time32(NULL);
		when_written.zone = sys_timezone(&scfg);
	} else {
		from = msg->from;
		when_written = msg->hdr.when_written;
	}
	if(from==NULL || *from==0) {
		from="SBBSecho";
		if(to != NULL && stricmp(to, "SBBSecho") == 0) {
			lprintf(LOG_NOTICE, "Refusing to create netmail-loop msg from %s to %s", from, to);
			return -42;	// Refuse to create mail-loop between 2 SBBSecho-bots
		}
	}

	if(to==NULL || *to==0)
		to="Sysop";
	if(!startmsg) startmsg=1;
	if((nodecfg=findnodecfg(&cfg, dest, 0)) != NULL) {
		if(nodecfg->status == MAIL_STATUS_NORMAL && !nodecfg->direct)
			nodecfg=findnodecfg(&cfg, dest, /* skip exact match: */2);
	}

	if(!isdir(scfg.netmail_dir) && mkpath(scfg.netmail_dir) != 0) {
		lprintf(LOG_ERR, "Error %u (%s) line %d creating directory: %s", errno, strerror(errno), __LINE__, scfg.netmail_dir);
		return -2;
	}
	for(i=startmsg;i;i++) {
		sprintf(fname,"%s%u.msg",scfg.netmail_dir,i);
		if(!fexistcase(fname))
			break;
	}
	if(!i) {
		lprintf(LOG_WARNING,"Directory full: %s",scfg.netmail_dir);
		return(-1);
	}
	startmsg=i+1;
	if((fp=fnopen(NULL,fname,O_RDWR|O_CREAT))==NULL) {
		lprintf(LOG_ERR,"ERROR %u (%s) line %d opening %s",errno,strerror(errno),__LINE__,fname);
		return(-1);
	}

	if(src != NULL)
		faddr = *src;
	else if(msg != NULL && msg->from_net.type == NET_FIDO && msg->from_net.addr != NULL)
		faddr = *(fidoaddr_t*)msg->from_net.addr;
	else
		faddr = getsysfaddr(dest);
	memset(&hdr,0,sizeof(fmsghdr_t));
	hdr.origzone=faddr.zone;
	hdr.orignet=faddr.net;
	hdr.orignode=faddr.node;
	hdr.origpoint=faddr.point;
	hdr.destzone=dest.zone;
	hdr.destnet=dest.net;
	hdr.destnode=dest.node;
	hdr.destpoint=dest.point;

	hdr.attr=(FIDO_PRIVATE|FIDO_KILLSENT|FIDO_LOCAL);
	if(msg != NULL) {
		if(msg->hdr.netattr&MSG_CRASH)
			hdr.attr|=FIDO_CRASH;
		if(msg->hdr.auxattr&MSG_FILEATTACH)
			hdr.attr|=FIDO_FILE;
		if(msg->hdr.auxattr&MSG_FILEREQUEST)
			hdr.attr|=FIDO_FREQ;
		if(msg->hdr.auxattr&MSG_RECEIPTREQ)
			hdr.attr|=FIDO_RRREQ;
	}

	if(nodecfg != NULL) {
		switch(nodecfg->status) {
			case MAIL_STATUS_HOLD:	hdr.attr|=FIDO_HOLD;	break;
			case MAIL_STATUS_CRASH:	hdr.attr|=FIDO_CRASH;	break;
			case MAIL_STATUS_NORMAL:						break;
		}
		direct = nodecfg->direct;
	}

	t = when_written.time;
	tm = localtime(&t);
	sprintf(hdr.time,"%02u %3.3s %02u  %02u:%02u:%02u"
		,tm->tm_mday,mon[tm->tm_mon],TM_YEAR(tm->tm_year)
		,tm->tm_hour,tm->tm_min,tm->tm_sec);

	SAFECOPY(hdr.to,to);
	SAFECOPY(hdr.from,from);
	SAFECOPY(hdr.subj,subject);

	(void)fwrite(&hdr,sizeof(fmsghdr_t),1,fp);
	fwrite_intl_control_line(fp, &hdr);

	if(!fidoctrl_line_exists(msg, "TZUTC:")) {
		/* TZUTC (FSP-1001) */
		int tzone=smb_tzutc(when_written.zone);
		char* minus="";
		if(tzone<0) {
			minus="-";
			tzone=-tzone;
		}
		fprintf(fp,"\1TZUTC: %s%02d%02u\r", minus, tzone/60, tzone%60);
	}
	if(msg == NULL) {
		if(!cfg.flo_mailer) {
			/* Add FSC-53 FLAGS kludge */
			fprintf(fp,"\1FLAGS");
			if(direct)
				fprintf(fp," DIR");
			if(hdr.attr&FIDO_FILE) {
				if(cfg.trunc_bundles)
					fprintf(fp," TFS");
				else
					fprintf(fp," KFS");
			}
			fprintf(fp,"\r");
		}
		fprintf(fp, "\1MSGID: %s %08lx\r", smb_faddrtoa(&faddr, NULL), time32(NULL));
	} else {
		if(msg->ftn_msgid != NULL)
			fprintf(fp, "\1MSGID: %.256s\r", msg->ftn_msgid);
		if(msg->ftn_reply != NULL)
			fprintf(fp, "\1REPLY: %.256s\r", msg->ftn_reply);
		if(msg->ftn_bbsid != NULL)
			fprintf(fp, "\1BBSID: %.256s\r", msg->ftn_bbsid);
		if(msg->ftn_flags != NULL)
			fprintf(fp, "\1FLAGS %s\r", msg->ftn_flags);
		else if(msg->hdr.auxattr&MSG_KILLFILE)
			fprintf(fp, "\1FLAGS %s\r", "KFS");
	}

	if(hdr.destpoint)
		fprintf(fp,"\1TOPT %hu\r",hdr.destpoint);
	if(hdr.origpoint)
		fprintf(fp,"\1FMPT %hu\r",hdr.origpoint);
	fprintf(fp,"\1PID: %s\r", (msg==NULL || msg->ftn_pid==NULL) ? sbbsecho_pid() : msg->ftn_pid);
	if(msg != NULL) {
		if(msg->columns)
			fprintf(fp,"\1COLS: %u\r", (unsigned int)msg->columns);
		/* Unknown kludge lines are added here */
		for(int i=0; i<msg->total_hfields; i++)
			if(msg->hfield[i].type == FIDOCTRL)
				fprintf(fp,"\1%.512s\r",(char*)msg->hfield_dat[i]);
		const char* charset = msg->ftn_charset;
		if(charset == NULL) {
			if(smb_msg_is_utf8(msg) || (msg->hdr.auxattr & MSG_HFIELDS_UTF8))
				charset = FIDO_CHARSET_UTF8;
			else if(str_is_ascii(body))
				charset = FIDO_CHARSET_ASCII;
			else
				charset = FIDO_CHARSET_CP437;
		}
		fprintf(fp, "\1CHRS: %s\r", charset);
		if(msg->editor != NULL)
			fprintf(fp, "\1NOTE: %s\r", msg->editor);
		if(subject != msg->subj)
			fprintf(fp, "Subject: %s\r\r", msg->subj);
	}
	/* Write the body text */
	if(body != NULL) {
		int bodylen = strlen(body);
		fwrite_crlf(body, bodylen, fp);
		/* Write the tear line */
		if(bodylen > 0 && body[bodylen-1] != '\r' && body[bodylen-1] != '\n')
			fputc('\r', fp);
		fprintf(fp, "%s", tear_line(strstr(body, "\n--- ") == NULL ? '-' : ' '));
	}
	fputc(FIDO_STORED_MSG_TERMINATOR, fp);
	lprintf(LOG_INFO, "Created NetMail (%s)%s from %s (%s) to %s (%s), attr: %04hX, subject: %s"
		,getfname(fname), (hdr.attr&FIDO_FILE) ? " with attachment" : ""
		,from, smb_faddrtoa(&faddr, tmp), to, smb_faddrtoa(&dest, NULL), hdr.attr, subject);
	return fclose(fp);
}

/******************************************************************************
 This function takes the contents of 'infile' and puts it into netmail
 message(s) bound for addr.
******************************************************************************/
int file_to_netmail(FILE* infile, const char* title, fidoaddr_t dest, const char* to)
{
	char *buf,*p;
	long l,m,len;
	int netmails_created = 0;

	if(fseek(infile, 0, SEEK_END) != 0)
		return 0;

	l=len=ftell(infile);
	if(len>8192L)
		len=8192L;
	else if(len < 1)
		return 0;
	rewind(infile);
	if((buf=(char *)malloc(len+1))==NULL) {
		lprintf(LOG_ERR,"ERROR line %d allocating %lu for file to netmail buf",__LINE__,len);
		return 0;
	}
	while((m=fread(buf,1,(len>8064L) ? 8064L:len,infile))>0) {
		buf[m]=0;
		if(l>8064L && (p=strrchr(buf,'\n'))!=NULL) {
			p++;
			if(*p) {
				*p=0;
				p++;
				(void)fseek(infile,-1L,SEEK_CUR);
				while(*p) { 			/* Seek back to end of last line */
					p++;
					(void)fseek(infile,-1L,SEEK_CUR);
				}
			}
		}
		if(ftell(infile)<l)
			strcat(buf,"\r\nContinued in next message...\r\n");
		if(create_netmail(to, /* msg: */NULL, title, buf, dest, /* src: */NULL) == 0)
			netmails_created++;
	}
	free(buf);
	return netmails_created;
}

bool new_area(const char* tag, uint subnum, fidoaddr_t* link)
{
	area_t* area_list;
	if((area_list = realloc(cfg.area, sizeof(area_t) * (cfg.areas+1))) == NULL)
		return false;
	cfg.area = area_list;
	memset(&cfg.area[cfg.areas], 0 ,sizeof(area_t));
	cfg.area[cfg.areas].sub = subnum;
	if(tag != NULL) {
		if((cfg.area[cfg.areas].tag = strdup(tag)) == NULL)
			return false;
	}
	if(link != NULL) {
		if((cfg.area[cfg.areas].link = malloc(sizeof(fidoaddr_t))) == NULL)
			return false;
		cfg.area[cfg.areas].link[0] = *link;
		cfg.area[cfg.areas].links++;
	}
	cfg.areas++;

	return true;
}

/* Returns true if area is linked with specified node address */
bool area_is_linked(unsigned area_num, const fidoaddr_t* addr)
{
	unsigned i;
	for(i=0;i<cfg.area[area_num].links;i++)
		if(!memcmp(addr,&cfg.area[area_num].link[i],sizeof(fidoaddr_t)))
			return true;
	return false;
}

/* Returns area index */
uint find_area(const char* echotag)
{
	unsigned u;

	for(u=0; u < cfg.areas; u++)
		if(cfg.area[u].tag != NULL && stricmp(cfg.area[u].tag, echotag) == 0)
			break;

	return u;
}

bool area_is_valid(uint areanum)
{
	return areanum < cfg.areas;
}

// if full is true, compare full address, otherwise, exclude point in comparison
uint find_sysfaddr(faddr_t addr, bool full)
{
	unsigned u;

	for(u=0; u < scfg.total_faddrs; u++) {
		if(full && memcmp(&scfg.faddr[u], &addr, sizeof(addr)) == 0)
			break;
		if(!full
			&& scfg.faddr[u].zone == addr.zone
			&& scfg.faddr[u].net == addr.net
			&& scfg.faddr[u].node == addr.node)
			break;
	}

	return u;
}

bool sysfaddr_is_valid(uint faddr_num)
{
	return faddr_num < scfg.total_faddrs;
}

/* Returns subnum (INVALID_SUB if pass-through) or SUB_NOT_FOUND */
#define SUB_NOT_FOUND ((uint)-2)
uint find_linked_area(const char* echotag, fidoaddr_t addr)
{
	unsigned area;

	if(area_is_valid(area = find_area(echotag)) && area_is_linked(area, &addr))
		return cfg.area[area].sub;

	return SUB_NOT_FOUND;
}

/******************************************************************************
 This function sends a notify list to applicable nodes, this list includes the
 settings configured for the node, as well as a list of areas the node is
 connected to.
******************************************************************************/
void gen_notify_list(nodecfg_t* nodecfg)
{
	FILE *	tmpf;
	char	str[256];
	uint	i,k;

	for(k=0;k<cfg.nodecfgs && !terminated;k++) {

		if(nodecfg != NULL && &cfg.nodecfg[k] != nodecfg)
			continue;

		if(!cfg.nodecfg[k].send_notify || cfg.nodecfg[k].passive)
			continue;

		if((tmpf=tmpfile())==NULL) {
			lprintf(LOG_ERR,"ERROR line %d couldn't open tmpfile",__LINE__);
			return;
		}

		fprintf(tmpf,"Following are the options set for your system and a list "
			"of areas\r\nyou are connected to.  Please make sure everything "
			"is correct.\r\n\r\n");
		fprintf(tmpf,"Name              %s\r\n", cfg.nodecfg[k].name);
		fprintf(tmpf,"Packet Type       %s\r\n", pktTypeStringList[cfg.nodecfg[k].pkt_type]);
		fprintf(tmpf,"Archive Type      %s\r\n"
			,cfg.nodecfg[k].archive == SBBSECHO_ARCHIVE_NONE ? "None" : cfg.nodecfg[k].archive->name);
		fprintf(tmpf,"Mail Status       %s\r\n", mailStatusStringList[cfg.nodecfg[k].status]);
		fprintf(tmpf,"Direct            %s\r\n", cfg.nodecfg[k].direct ? "Yes":"No");
		fprintf(tmpf,"Passive (paused)  %s\r\n", cfg.nodecfg[k].passive ? "Yes":"No");
		fprintf(tmpf,"Notifications     %s\r\n", cfg.nodecfg[k].send_notify ? "Yes":"No");
		fprintf(tmpf,"Remote AreaMgr    %s\r\n\r\n"
			,cfg.nodecfg[k].areafix ? "Yes" : "No");

		fprintf(tmpf,"Connected Areas\r\n---------------\r\n");
		for(i=0;i<cfg.areas;i++) {
			sprintf(str,"%s\r\n",cfg.area[i].tag);
			if(str[0]=='*')
				continue;
			if(area_is_linked(i,&cfg.nodecfg[k].addr))
				fprintf(tmpf,"%s",str);
		}

		if(ftell(tmpf))
			file_to_netmail(tmpf,"SBBSecho Notify List",cfg.nodecfg[k].addr, /* To: */cfg.nodecfg[k].name);
		fclose(tmpf);
	}
}

/******************************************************************************
 This function creates a netmail to addr showing a list of available areas (0),
 a list of connected areas (1), or a list of removed areas (2).
******************************************************************************/
enum arealist_type {
	 AREALIST_ALL			// %LIST
	,AREALIST_CONNECTED		// %QUERY
	,AREALIST_UNLINKED		// %UNLINKED
};
void netmail_arealist(enum arealist_type type, fidoaddr_t addr, const char* to)
{
	char str[256],title[128],match,*p,*tp;
	unsigned k,x;
	unsigned u;
	str_list_t	area_list;

	if(type == AREALIST_ALL)
		SAFECOPY(title,"List of Available Areas");
	else if(type == AREALIST_CONNECTED)
		SAFECOPY(title,"List of Connected Areas");
	else
		SAFECOPY(title,"List of Unlinked Areas");

	if((area_list=strListInit()) == NULL) {
		lprintf(LOG_ERR,"ERROR line %d couldn't allocate string list",__LINE__);
		return;
	}

	/* Include relevant areas from the area file (e.g. areas.bbs): */
	for(u=0;u<cfg.areas;u++) {
		if(u == cfg.badecho)
			continue;
		if((type == AREALIST_CONNECTED || cfg.add_from_echolists_only) && !area_is_linked(u,&addr))
			continue;
		if(type == AREALIST_UNLINKED && area_is_linked(u,&addr))
			continue;
		strListPush(&area_list, cfg.area[u].tag);
	}

	if(type != AREALIST_CONNECTED) {
		nodecfg_t* nodecfg=findnodecfg(&cfg, addr,0);
		if(nodecfg != NULL) {
			for(u=0;u<cfg.listcfgs;u++) {
				match=0;
				for(k=0; cfg.listcfg[u].keys[k]; k++) {
					if(match) break;
					for(x=0; nodecfg->keys[x]; x++) {
						if(!stricmp(cfg.listcfg[u].keys[k]
							,nodecfg->keys[x])) {
							FILE* fp;
							if((fp=fopen(cfg.listcfg[u].listpath,"r"))==NULL) {
								lprintf(LOG_ERR,"ERROR %u (%s) line %d opening %s"
									,errno,strerror(errno),__LINE__,cfg.listcfg[u].listpath);
								match=1;
								break;
							}
							while(!feof(fp)) {
								memset(str,0,sizeof(str));
								if(!fgets(str,sizeof(str),fp))
									break;
								truncsp(str);
								p=str;
								SKIP_WHITESPACE(p);
								if(*p==0 || *p==';')     /* Ignore Blank and Comment Lines */
									continue;
								tp=p;
								FIND_WHITESPACE(tp);
								*tp=0;
								if(find_linked_area(p, addr) == SUB_NOT_FOUND) {
									if(strListFind(area_list, p, /* case_sensitive */false) < 0)
										strListPush(&area_list, p);
								}
							}
							fclose(fp);
							match=1;
							break;
						}
					}
				}
			}
		}
	}
	strListSortAlpha(area_list);
	if(strListIsEmpty(area_list))
		create_netmail(to, /* msg: */NULL, title, "None.", /* dest: */addr, /* src: */NULL);
	else {
		FILE* fp;
		if((fp=tmpfile())==NULL) {
			lprintf(LOG_ERR,"ERROR line %d couldn't open tmpfile",__LINE__);
		} else {
			int longest = 0;
			for(u = 0; area_list[u] != NULL; u++) {
				int len = strlen(area_list[u]);
				if(len > longest) longest = len;
			}
			for(u = 0; area_list[u] != NULL; u++)
				fprintf(fp, "%-*s %s\r\n", longest, area_list[u], area_desc(area_list[u]));
			file_to_netmail(fp,title,addr,to);
			fclose(fp);
		}
	}
	lprintf(LOG_INFO,"AreaFix (for %s) Created response netmail with %s (%lu areas)"
		, smb_faddrtoa(&addr, NULL), title, (ulong)strListCount(area_list));
	strListFree(&area_list);
}

int check_elists(const char *areatag, fidoaddr_t addr)
{
	FILE *stream;
	char str[1025],quit=0,*p,*tp;
	unsigned k,x,match=0;
	unsigned u;

	nodecfg_t* nodecfg=findnodecfg(&cfg, addr,0);
	if(nodecfg!=NULL) {
		for(u=0;u<cfg.listcfgs;u++) {
			quit=0;
			for(k=0; cfg.listcfg[u].keys[k]; k++) {
				if(quit) break;
				for(x=0; nodecfg->keys[x] ;x++)
					if(!stricmp(cfg.listcfg[u].keys[k]
						,nodecfg->keys[x])) {
						if((stream=fopen(cfg.listcfg[u].listpath,"r"))==NULL) {
							lprintf(LOG_ERR,"ERROR %u (%s) line %d opening %s"
								,errno,strerror(errno),__LINE__,cfg.listcfg[u].listpath);
							quit=1;
							break;
						}
						while(!feof(stream)) {
							if(!fgets(str,sizeof(str),stream))
								break;
							p=str;
							SKIP_WHITESPACE(p);
							if(*p==';')     /* Ignore Comment Lines */
								continue;
							tp=p;
							FIND_WHITESPACE(tp);
							*tp=0;
							if(!stricmp(areatag,p)) {
								match=1;
								break;
							}
						}
						fclose(stream);
						quit=1;
						if(match)
							return(match);
						break;
					}
			}
		}
	}
	return(match);
}
/******************************************************************************
 Used by AREAFIX to add/remove/change areas in the areas file
******************************************************************************/
void alter_areas(str_list_t add_area, str_list_t del_area, fidoaddr_t addr, const char* to)
{
	FILE *nmfile,*afilein,*afileout,*fwdfile;
	char str[1024],fields[1024],code[LEN_EXTCODE+1],echotag[FIDO_AREATAG_LEN+1],comment[256]
		,outpath[MAX_PATH+1]
		,*p,*tp,nomatch=0,match=0;
	int	file;
	unsigned j,k,x,y;
	unsigned u;
	size_t add_count, added = 0;
	size_t del_count, deleted = 0;
	struct stat st = {0};

	add_count = strListCount(add_area);
	del_count = strListCount(del_area);

	if((nmfile=tmpfile())==NULL) {
		lprintf(LOG_ERR,"ERROR in tmpfile()");
		return;
	}
	SAFECOPY(outpath,cfg.areafile);
	*getfname(outpath)=0;
	SAFECAT(outpath, "AREASXXXXXX");
	if((file = mkstemp(outpath)) == -1) {
		lprintf(LOG_ERR, "ERROR %u (%s) line %d opening %s", errno, strerror(errno), __LINE__, outpath);
		fclose(nmfile);
		return;
	}
	if((afileout=fdopen(file, "w+"))==NULL) {
		lprintf(LOG_ERR,"ERROR %u (%s) line %d fdopening %s",errno,strerror(errno),__LINE__,outpath);
		fclose(nmfile);
		return;
	}
	if((afilein=fopen(cfg.areafile,"r"))==NULL) {
		lprintf(LOG_ERR,"ERROR %u (%s) line %d opening %s",errno,strerror(errno),__LINE__,cfg.areafile);
		fclose(afileout);
		fclose(nmfile);
		return;
	}
	while(!feof(afilein)) {
		if(!fgets(fields,sizeof(fields),afilein))
			break;
		truncsp(fields);
		p=fields;
		SKIP_WHITESPACE(p);
		if(*p==';') {    /* Skip Comment Lines */
			fprintf(afileout,"%s\n",fields);
			continue;
		}
		SAFECOPY(code,p);         /* Internal Code Field */
		truncstr(code," \t\r\n");
		FIND_WHITESPACE(p);
		SKIP_WHITESPACE(p);
		SAFECOPY(echotag,p);         /* Areatag Field */
		truncstr(echotag," \t\r\n");
		if(echotag[0] == '*') {
			fprintf(afileout, "%s\n", fields);	/* Don't allow down-links to our "Unknown area" */
			continue;
		}
		FIND_WHITESPACE(p);
		SKIP_WHITESPACE(p);
		if((tp=strchr(p,';'))!=NULL) {
			SAFECOPY(comment,tp);     /* Comment Field (if any), follows links */
		}
		else
			comment[0]=0;
		if(del_count) { 				/* Check for areas to remove */
			for(u=0;del_area[u]!=NULL;u++) {
				if(!stricmp(del_area[u],echotag) ||
					!stricmp(del_area[0],"-ALL"))     /* Match Found */
					break;
			}
			if(del_area[u]!=NULL) {
				for(u=0;u<cfg.areas;u++) {
					if(!stricmp(echotag,cfg.area[u].tag)) {
						if(!area_is_linked(u,&addr)) {
							fprintf(afileout,"%s\n",fields);
							/* bugfix here Mar-25-2004 (wasn't breaking for "-ALL") */
							if(stricmp(del_area[0],"-ALL")) {
								fprintf(nmfile,"%s not connected.\r\n",echotag);
								lprintf(LOG_NOTICE, "AreaFix (for %s) Unlink-requested area already unlinked: %s"
									,smb_faddrtoa(&addr,NULL), echotag);
							}
							break;
						}
						lprintf(LOG_INFO,"AreaFix (for %s) Unlinking area: %s", smb_faddrtoa(&addr,NULL), echotag);

						fprintf(afileout,"%-*s %-*s "
							,LEN_EXTCODE, code
							,FIDO_AREATAG_LEN, echotag);
						for(j=0;j<cfg.area[u].links;j++) {
							if(!memcmp(&cfg.area[u].link[j],&addr
								,sizeof(fidoaddr_t)))
								continue;
							fprintf(afileout,"%s "
								,smb_faddrtoa(&cfg.area[u].link[j],NULL));
						}
						if(comment[0])
							fprintf(afileout,"%s",comment);
						fprintf(afileout,"\n");
						fprintf(nmfile,"%s removed.\r\n",echotag);
						deleted++;
						break;
					}
				}
				if(u==cfg.areas)			/* Something screwy going on */
					fprintf(afileout,"%s\n",fields);
				continue;
			} 				/* Area match so continue on */
		}
		if(add_count) { 				/* Check for areas to add */
			for(u=0;add_area[u]!=NULL;u++)
				if(!stricmp(add_area[u],echotag) ||
					!stricmp(add_area[0],"+ALL"))      /* Match Found */
					break;
			if(add_area[u]!=NULL) {
				if(stricmp(add_area[u],"+ALL"))
					add_area[u][0]=0;  /* So we can check other lists */
				for(u=0;u<cfg.areas;u++) {
					if(!stricmp(echotag,cfg.area[u].tag)) {
						if(area_is_linked(u,&addr)) {
							fprintf(afileout,"%s\n",fields);
							fprintf(nmfile,"%s already connected.\r\n",echotag);
							lprintf(LOG_NOTICE, "AreaFix (for %s) Link-requested area is already connected: %s"
								,smb_faddrtoa(&addr,NULL), echotag);
							break;
						}
						if(cfg.add_from_echolists_only && !check_elists(echotag,addr)) {
							fprintf(afileout,"%s\n",fields);
							lprintf(LOG_NOTICE, "AreaFix (for %s) Link-requested area not found in EchoLists: %s"
								,smb_faddrtoa(&addr,NULL), echotag);
							break;
						}

						lprintf(LOG_INFO,"AreaFix (for %s) Linking area: %s", smb_faddrtoa(&addr,NULL), echotag);

						/* Added 12/4/95 to add link to connected links */

						++cfg.area[u].links;
						if((cfg.area[u].link=(fidoaddr_t *)
							realloc(cfg.area[u].link,sizeof(fidoaddr_t)
							*(cfg.area[u].links)))==NULL) {
							lprintf(LOG_ERR,"ERROR line %d allocating memory for area "
								"#%u links.",__LINE__,u+1);
							bail(1);
							return;
						}
						memcpy(&cfg.area[u].link[cfg.area[u].links-1],&addr,sizeof(fidoaddr_t));

						fprintf(afileout,"%-*s %-*s "
							,LEN_EXTCODE, code
							,FIDO_AREATAG_LEN, echotag);
						for(j=0;j<cfg.area[u].links;j++)
							fprintf(afileout,"%s "
								,smb_faddrtoa(&cfg.area[u].link[j],NULL));
						if(comment[0])
							fprintf(afileout,"%s",comment);
						fprintf(afileout,"\n");
						fprintf(nmfile,"%s added.\r\n",echotag);
						added++;
						break;
					}
				}
				if(u==cfg.areas)			/* Something screwy going on */
					fprintf(afileout,"%s\n",fields);
				continue;  					/* Area match so continue on */
			}
			nomatch=1; 						/* This area wasn't in there */
		}
		fprintf(afileout,"%s\n",fields);	/* No match so write back line */
	}
	fclose(afilein);
	if(nomatch || (add_count && stricmp(add_area[0],"+ALL") == 0)) {
		nodecfg_t* nodecfg=findnodecfg(&cfg, addr,0);
		if(nodecfg != NULL) {
			for(j=0;j<cfg.listcfgs;j++) {
				match=0;
				for(k=0; cfg.listcfg[j].keys[k] ;k++) {
					if(match) break;
					for(x=0; nodecfg->keys[x] ;x++) {
						if(!stricmp(cfg.listcfg[j].keys[k]
							,nodecfg->keys[x])) {
							if((fwdfile=tmpfile())==NULL) {
								lprintf(LOG_ERR,"ERROR line %d opening forward temp "
									"file",__LINE__);
								match=1;
								break;
							}
							if((afilein=fopen(cfg.listcfg[j].listpath,"r"))==NULL) {
								lprintf(LOG_ERR,"ERROR %u (%s) line %d opening %s"
									,errno,strerror(errno),__LINE__,cfg.listcfg[j].listpath);
								fclose(fwdfile);
								match=1;
								break;
							}
							while(!feof(afilein)) {
								if(!fgets(str,sizeof(str),afilein))
									break;
								p=str;
								SKIP_WHITESPACE(p);
								if(*p==';')     /* Ignore Comment Lines */
									continue;
								tp=p;
								FIND_WHITESPACE(tp);
								*tp=0;
								SAFECOPY(echotag, p);
								if(add_area[0]!=NULL && stricmp(add_area[0],"+ALL")==0) {
									if(area_is_valid(find_area(p)))
										continue;
								}
								for(y=0;add_area[y]!=NULL;y++)
									if((!stricmp(add_area[y], echotag) &&
										add_area[y][0]) ||
										!stricmp(add_area[0],"+ALL"))
										break;
								if(add_area[y]!=NULL) {
									fprintf(afileout,"%-*s %-*s"
										,LEN_EXTCODE, "P"
										,FIDO_AREATAG_LEN, echotag);
									if(cfg.listcfg[j].hub.zone)
										fprintf(afileout," %s"
											,smb_faddrtoa(&cfg.listcfg[j].hub,NULL));
									fprintf(afileout," %s\n",smb_faddrtoa(&addr,NULL));
									fprintf(nmfile,"%s added.\r\n", echotag);
									if(stricmp(add_area[0],"+ALL"))
										add_area[y][0]=0;
									if(cfg.listcfg[j].forward
										&& cfg.listcfg[j].hub.zone)
										fprintf(fwdfile,"%s\r\n", echotag);
									added++;
									lprintf(LOG_NOTICE, "AreaFix (for %s) Adding area from EchoList (%s): %s"
										, smb_faddrtoa(&addr,NULL), cfg.listcfg[j].listpath, echotag);
								}
							}
							fclose(afilein);
							if(cfg.listcfg[j].forward && ftell(fwdfile)>0)
								file_to_netmail(fwdfile,cfg.listcfg[j].password
									,cfg.listcfg[j].hub,/* To: */cfg.listcfg[j].areamgr);
							fclose(fwdfile);
							match=1;
							break;
						}
					}
				}
			}
		}
	}
	if(add_count && stricmp(add_area[0],"+ALL")) {
		for(u=0;add_area[u]!=NULL;u++)
			if(add_area[u][0]) {
				fprintf(nmfile,"%s not found.\r\n",add_area[u]);
				lprintf(LOG_NOTICE, "AreaFix (for %s) Link-requested echo not found: %s", smb_faddrtoa(&addr,NULL), add_area[u]);
			}
	}
	if (to != NULL) {
		if (!ftell(nmfile))
			create_netmail(to,/* msg: */NULL, "Area Management Request", "No changes made."
				,/* dest: */addr, /* src: */NULL);
		else
			file_to_netmail(nmfile, "Area Management Request", addr, to);
	}
	fclose(nmfile);
	fclose(afileout);
	if(added)
		lprintf(LOG_DEBUG, "AreaFix (for %s) Added links to %lu areas in %s"
			,smb_faddrtoa(&addr,NULL), (ulong)added, cfg.areafile);
	if(deleted)
		lprintf(LOG_DEBUG, "AreaFix (for %s) Removed links to %lu areas in %s"
			,smb_faddrtoa(&addr,NULL), (ulong)deleted, cfg.areafile);
	if(added || deleted) {
		if(stat(cfg.areafile, &st) == 0)
			chmod(outpath, st.st_mode);
		if(cfg.areafile_backups == 0 || !backup(cfg.areafile, cfg.areafile_backups, /* ren: */TRUE))
			delfile(cfg.areafile, __LINE__);					/* Delete AREAS.BBS */
		if(rename(outpath,cfg.areafile))		   /* Rename new AREAS.BBS file */
			lprintf(LOG_ERR,"ERROR line %d renaming %s to %s",__LINE__,outpath,cfg.areafile);
	}
}

bool add_sub_to_arealist(sub_t* sub, fidoaddr_t uplink)
{
	FILE* fp = NULL;

	char echotag[FIDO_AREATAG_LEN+1];
	sub_area_tag(&scfg, sub, echotag, sizeof(echotag));

	for(unsigned u=0; u < cfg.areas; u++) {
		if(stricmp(cfg.area[u].tag, echotag) == 0)
			return false;
	}
	lprintf(LOG_INFO, "Adding sub-board (%-*s) to Area %s with uplink %s, tag: %s"
		,LEN_EXTCODE, sub->code, cfg.auto_add_to_areafile ? "File" : "List", smb_faddrtoa(&uplink, NULL), echotag);

	if(cfg.auto_add_to_areafile) {
		/* Back-up Area File (at most, once per invocation) */
		static ulong added;
		if(added++ == 0)
			backup(cfg.areafile, cfg.areafile_backups, /* ren: */FALSE);

		fp = fopen(cfg.areafile, fexist(cfg.areafile) ? "r+" : "w+");
		if(fp == NULL) {
			lprintf(LOG_ERR, "Error %d opening %s", errno, cfg.areafile);
			return false;
		}
		/* Make sure the line we add is on a line of its own: */
		int ch = EOF;
		int last_ch = '\n';
		while(!feof(fp)) {
			if((ch = fgetc(fp)) != EOF)
				last_ch = ch;
		}
		if(last_ch != '\n')
			fputc('\n', fp);

		fprintf(fp, "%-*s %-*s %s\n"
			,LEN_EXTCODE, sub->code, FIDO_AREATAG_LEN, echotag, smb_faddrtoa(&uplink, NULL));
		fclose(fp);
	}

	return new_area(echotag, sub->subnum, &uplink);
}

/******************************************************************************
 Used by AREAFIX to add/remove/change link info in the configuration file
******************************************************************************/
bool alter_config(nodecfg_t* nodecfg, const char* key, const char* value)
{
	FILE* fp;

	static ulong alterations;
	if(alterations++ == 0)
		backup(cfg.cfgfile, cfg.cfgfile_backups, /* ren: */false);

	if((fp=iniOpenFile(cfg.cfgfile, false)) == NULL) {
		lprintf(LOG_ERR, "ERROR %d (%s) opening %s for altering configuration of node %s"
			,errno, strerror(errno), cfg.cfgfile, smb_faddrtoa(&nodecfg->addr, NULL));
		return false;
	}
	str_list_t ini = iniReadFile(fp);
	if(ini == NULL) {
		lprintf(LOG_ERR, "ERROR reading %s for altering configuration of node %s"
			,cfg.cfgfile, smb_faddrtoa(&nodecfg->addr,NULL));
		iniCloseFile(fp);
		return false;
	}
	char section[128];
	SAFEPRINTF2(section, "node:%s@%s", smb_faddrtoa(&nodecfg->addr,NULL), nodecfg->domain);
	if(!iniSectionExists(ini, section))
		SAFEPRINTF(section, "node:%s", smb_faddrtoa(&nodecfg->addr,NULL));
	iniSetString(&ini, section, key, value, &sbbsecho_ini_style);
	iniWriteFile(fp, ini);
	iniCloseFile(fp);
	iniFreeStringList(ini);
	return true;
}

/******************************************************************************
 Used by AREAFIX to process any '%' commands that come in via netmail
******************************************************************************/
bool areafix_command(char* instr, nodecfg_t* nodecfg, const char* to)
{
	FILE *stream,*tmpf;
	char str[MAX_PATH+1];
	int  file;
	unsigned u;
	faddr_t addr = nodecfg->addr;

	lprintf(LOG_DEBUG, "AreaFix (for %s) Received command: %s", faddrtoa(&addr), instr);

	if(stricmp(instr, "HELP") == 0) {
		SAFEPRINTF(str,"%sAREAMGR.HLP",scfg.exec_dir);
		if(!fexistcase(str)) {
			lprintf(LOG_ERR, "AreaFix (for %s) Help file not found: %s", faddrtoa(&addr), str);
			return true;
		}
		if((stream=fnopen(&file,str,O_RDONLY))==NULL) {
			lprintf(LOG_ERR,"ERROR %u (%s) line %d opening %s",errno,strerror(errno),__LINE__,str);
			return false;
		}
		bool result = file_to_netmail(stream, "Area Manager Help", addr, to) > 0;
		if(!result)
			lprintf(LOG_ERR,"ERROR converting file to netmail(s)");
		fclose(stream);
		return result;
	}

	if(stricmp(instr, "LIST") == 0) {
		netmail_arealist(AREALIST_ALL,addr,to);
		return true;
	}

	if(stricmp(instr, "QUERY") == 0) {
		netmail_arealist(AREALIST_CONNECTED,addr,to);
		return true;
	}

	if(stricmp(instr, "UNLINKED") == 0) {
		netmail_arealist(AREALIST_UNLINKED,addr,to);
		return true;
	}

	if(stricmp(nodecfg->name, to) != 0) {
		lprintf(LOG_INFO, "AreaFix (for %s) Changing name to: %s", faddrtoa(&addr), to);
		alter_config(nodecfg, "Name", to);
	}

	if(strnicmp(instr, "COMPRESSION ", 12) == 0 || strnicmp(instr, "COMPRESS ", 9) == 0) {
		char* p = instr;
		FIND_WHITESPACE(p);
		SKIP_WHITESPACE(p);
		if(!stricmp(p,"NONE"))
			nodecfg->archive = SBBSECHO_ARCHIVE_NONE;
		else {
			for(u=0;u<cfg.arcdefs;u++)
				if(stricmp(p,cfg.arcdef[u].name) == 0)
					break;
			if(u==cfg.arcdefs) {
				if((tmpf=tmpfile())==NULL) {
					lprintf(LOG_ERR,"ERROR line %d opening tmpfile()",__LINE__);
					return false;
				}
				SAFEPRINTF(str, "Compression type unavailable: %s", p);
				lprintf(LOG_INFO, "AreaFix (for %s) %s", faddrtoa(&addr), str);
				fprintf(tmpf,"Compression type unavailable: %s\r\n\r\n"
					"Available types are:\r\n", p);
				for(u=0;u<cfg.arcdefs;u++)
					fprintf(tmpf,"                     %s\r\n",cfg.arcdef[u].name);
				if(!file_to_netmail(tmpf,"Compression Type Change",addr,to))
					lprintf(LOG_ERR,"ERROR converting file to netmail(s)");
				fclose(tmpf);
				return true;
			}
			nodecfg->archive = &cfg.arcdef[u];
		}
		alter_config(nodecfg,"archive",p);
		SAFEPRINTF(str, "Compression type changed to: %s", p);
		lprintf(LOG_INFO, "AreaFix (for %s) %s", faddrtoa(&addr), str);
		create_netmail(to, /* msg: */NULL, "Compression Type Change", str, /* dest: */addr, /* src: */NULL);
		return true;
	}

	if(strnicmp(instr, "PASSWORD ", 9) == 0 || strnicmp(instr, "PWD ", 4) == 0) {
		char password[FIDO_SUBJ_LEN];	/* AreaMgr password for this node */
		char* p = instr;
		FIND_WHITESPACE(p);
		SKIP_WHITESPACE(p);
		SAFECOPY(password, p);
		if(!stricmp(password, nodecfg->password)) {
			sprintf(str,"Your AreaMgr password was already set to '%s'."
				,nodecfg->password);
			lprintf(LOG_INFO, "AreaFix (for %s) %s", faddrtoa(&addr), str);
			create_netmail(to, /* msg: */NULL, "AreaMgr Password Change Request", str
				,/* dest: */addr, /* src: */NULL);
			return true;
		}
		if(alter_config(nodecfg,"AreaFixPwd", password)) {
			SAFEPRINTF2(str,"Your AreaMgr password has been changed from '%s' to '%s'."
				,nodecfg->password,password);
			SAFECOPY(nodecfg->password, password);
		} else {
			SAFECOPY(str,"Error changing AreaMgr password");
		}
		lprintf(LOG_INFO, "AreaFix (for %s) %s", faddrtoa(&addr), str);
		create_netmail(to, /* msg: */NULL, "AreaMgr Password Change Request", str, /* dest: */addr, /* src: */NULL);
		return true;
	}

	if(strnicmp(instr, "PKTPWD ", 7) == 0) {
		char pktpwd[FIDO_PASS_LEN + 1];	/* Packet password for this node */
		char* p = instr;
		FIND_WHITESPACE(p);
		SKIP_WHITESPACE(p);
		SAFECOPY(pktpwd, p);
		if(!stricmp(pktpwd, nodecfg->pktpwd)) {
			sprintf(str,"Your packet password was already set to '%s'."
				,nodecfg->pktpwd);
			lprintf(LOG_INFO, "AreaFix (for %s) %s", faddrtoa(&addr), str);
			create_netmail(to, /* msg: */NULL, "Packet Password Change Request", str, /* dest: */addr, /* src: */NULL);
			return true;
		}
		if(alter_config(nodecfg,"PacketPwd", pktpwd)) {
			SAFEPRINTF2(str,"Your packet password has been changed from '%s' to '%s'."
				,nodecfg->pktpwd, pktpwd);
			SAFECOPY(nodecfg->pktpwd, pktpwd);
		} else {
			SAFECOPY(str,"Error changing packet password");
		}
		lprintf(LOG_INFO, "AreaFix (for %s) %s", faddrtoa(&addr), str);
		create_netmail(to, /* msg: */NULL, "Packet Password Change Request", str, /* dest: */addr, /* src: */NULL);
		return true;
	}

	if(strnicmp(instr, "TICPWD ", 7) == 0) {
		char ticpwd[SBBSECHO_MAX_TICPWD_LEN + 1]; /* TIC File password for this node */
		char* p = instr;
		FIND_WHITESPACE(p);
		SKIP_WHITESPACE(p);
		SAFECOPY(ticpwd, p);
		if(!stricmp(ticpwd, nodecfg->ticpwd)) {
			sprintf(str,"Your TIC File password was already set to '%s'."
				,nodecfg->ticpwd);
			lprintf(LOG_INFO, "AreaFix (for %s) %s", faddrtoa(&addr), str);
			create_netmail(to, /* msg: */NULL, "TIC File Password Change Request", str, /* dest: */addr, /* src: */NULL);
			return true;
		}
		if(alter_config(nodecfg,"TicFilePwd", ticpwd)) {
			SAFEPRINTF2(str,"Your TIC File password has been changed from '%s' to '%s'."
				,nodecfg->ticpwd, ticpwd);
			SAFECOPY(nodecfg->ticpwd, ticpwd);
		} else {
			SAFECOPY(str,"Error changing TIC File password");
		}
		lprintf(LOG_INFO, "AreaFix (for %s) %s", faddrtoa(&addr), str);
		create_netmail(to, /* msg: */NULL, "TIC File Password Change Request", str, /* dest: */addr, /* src: */NULL);
		return true;
	}

	if(strnicmp(instr, "NOTIFY ", 7) == 0) {
		char* p = instr;
		FIND_WHITESPACE(p);
		SKIP_WHITESPACE(p);
		if(alter_config(nodecfg,"Notify", p)) {
			SAFEPRINTF2(str,"Your Notification Messages have been changed from '%s' to '%s'."
				,nodecfg->send_notify ? "ON" : "OFF", p);
		} else {
			SAFECOPY(str,"Error changing Notify Setting");
		}
		lprintf(LOG_INFO, "AreaFix (for %s) %s", faddrtoa(&addr), str);
		create_netmail(to, /* msg: */NULL, "Notification Messages Change Request", str, /* dest: */addr, /* src: */NULL);
		return true;
	}

	if(stricmp(instr, "RESCAN") == 0) {
		lprintf(LOG_INFO, "AreaFix (for %s) Rescanning all areas", faddrtoa(&addr));
		export_echomail(NULL, nodecfg, true);
		create_netmail(to,/* msg: */NULL, "Rescan Areas"
			,"All connected areas carried by your hub have been rescanned."
			,/* dest: */addr, /* src: */NULL);
		return true;
	}

	if(strnicmp(instr, "RESCAN ", 7) == 0) {
		char* p = instr;
		FIND_WHITESPACE(p);
		SKIP_WHITESPACE(p);
		int subnum = find_linked_area(p, addr);
		if(subnum == SUB_NOT_FOUND)
			SAFEPRINTF(str, "Echo-tag '%s' not linked or not found.", p);
		else if(subnum == INVALID_SUB)
			SAFEPRINTF(str, "Connected area '%s' is pass-through: cannot be rescanned", p);
		else {
			export_echomail(scfg.sub[subnum]->code, nodecfg, true);
			SAFEPRINTF(str, "Connected area '%s' has been rescanned.", p);
		}
		lprintf(LOG_INFO, "AreaFix (for %s) %s", faddrtoa(&addr), str);
		create_netmail(to,/* msg: */NULL, "Rescan Area"
			,str, /* dest: */addr, /* src: */NULL);
		return true;
	}

	if(strnicmp(instr, "ECHOSTATS ", 10) == 0) {
		char* p = instr;
		FIND_WHITESPACE(p);
		SKIP_WHITESPACE(p);
		echostat_t* stat = get_echostat(p, /* create: */false);
		if(stat == NULL) {
			lprintf(LOG_INFO, "AreaFix (for %s) EchoStats request for unknown echo: %s", faddrtoa(&addr), p);
		} else {
			FILE* fp;
			if((fp=tmpfile())==NULL) {
				lprintf(LOG_ERR,"ERROR line %d couldn't open tmpfile",__LINE__);
			} else {
				fwrite_echostat(fp, stat);
				if(!file_to_netmail(fp,"Echo Statistics",addr,to))
					lprintf(LOG_ERR,"ERROR converting file to netmail(s)");
				fclose(fp);
			}
		}
		return true;
	}

	if(stricmp(instr, "ACTIVE") == 0 || stricmp(instr, "RESUME") == 0) {
		if(!nodecfg->passive) {
			create_netmail(to,/* msg: */NULL,"Reconnect Disconnected (paused) Areas"
				,"Your areas are already connected.", /* dest: */addr, /* src: */NULL);
			return true;
		}
		nodecfg->passive = false;
		alter_config(nodecfg,"passive","false");
		create_netmail(to,/* msg: */NULL,"Reconnect Disconnected (paused) Areas"
			,"Temporarily disconnected areas have been reconnected.", /* dest: */addr, /* src: */NULL);
		return true;
	}

	if(stricmp(instr, "PASSIVE") == 0 || stricmp(instr, "PAUSE") == 0) {
		if(nodecfg->passive) {
			create_netmail(to,/* msg: */NULL,"Temporarily Disconnect (pause) Areas"
				,"Your areas are already temporarily disconnected.", /* dest: */addr, /* src: */NULL);
			return true;
		}
		nodecfg->passive = true;
		alter_config(nodecfg,"passive","true");
		create_netmail(to,/* msg: */NULL,"Temporarily Disconnect (pause) Areas"
			,"Your areas have been temporarily disconnected.", /* dest: */addr, /* src: */NULL);
		return true;
	}

	if(stricmp(instr, "+ALL") == 0) {
		str_list_t add_area=strListInit();
		strListPush(&add_area, instr);
		alter_areas(add_area,NULL,addr,to);
		strListFree(&add_area);
		return true;
	}

	if(stricmp(instr, "-ALL") == 0) {
		str_list_t del_area=strListInit();
		strListPush(&del_area, instr);
		alter_areas(NULL,del_area,addr,to);
		strListFree(&del_area);
		return true;
	}

	return false;
}
/******************************************************************************
 This is where we're gonna process any netmail that comes in for AreaFix.
 Returns text for message body for the local Area Mgr (sysop) upon failure.
******************************************************************************/
char* process_areafix(fidoaddr_t addr, char* inbuf, const char* password, const char* name)
{
	static char body[1024];
	char str[128];
	char *p,*tp,action,cmds=0;
	ulong l,m;
	str_list_t add_area,del_area;

	lprintf(LOG_INFO,"AreaFix (for %s) Request received from %s"
			,smb_faddrtoa(&addr,NULL), name);

	sprintf(body,"%s (%s) attempted an Area Management (AreaFix) request.\r\n", name, faddrtoa(&addr));

	p=(char *)inbuf;

	while(*p==CTRL_A) {			/* Skip kludge lines 11/05/95 */
		FIND_CHAR(p,'\r');
		if(*p) {
			p++;				/* Skip CR (required) */
			if(*p=='\n')
				p++;			/* Skip LF (optional) */
		}
	}

	if(((tp=strstr(p,"---\r"))!=NULL || (tp=strstr(p,"--- "))!=NULL || (tp=strstr(p,"-- \r"))!=NULL) &&
		(*(tp-1)=='\r' || *(tp-1)=='\n'))
		*tp=0;

	if(!strnicmp(p,"%FROM",5)) {    /* Remote Remote Maintenance (must be first) */
		SAFECOPY(str,p+6);
		truncstr(str,"\r\n");
		lprintf(LOG_NOTICE,"AreaFix (for %s) Remote/proxy management requested via %s",str
			,smb_faddrtoa(&addr,NULL));
		addr=atofaddr(str);
	}

	nodecfg_t* nodecfg = findnodecfg(&cfg, addr,0);
	if(nodecfg==NULL || !nodecfg->areafix) {
		lprintf(LOG_NOTICE,"AreaFix (for %s) Request received, but AreaFix not enabled for this node!", smb_faddrtoa(&addr,NULL));
		create_netmail(name,/* msg: */NULL,"Area Management Request"
			,"Your node is not configured for AreaFix, please contact your hub sysop.\r\n"
			,/* dest: */addr, /* src: */NULL);
		strcat(body,"This node is not configured for AreaFix operations.\r\n");
		return(body);
	}

	if(stricmp(nodecfg->password,password)) {
		create_netmail(name,/* msg: */NULL,"Area Management Request","Invalid Password."
			,/* dest: */addr, /* src: */NULL);
		sprintf(body+strlen(body), "An invalid password (%s) was supplied in the message subject.\r\n"
			"The correct Area Manager password for this node is: %s\r\n"
			,password
			,(nodecfg->password[0]) ? nodecfg->password
			 : "[None Defined]");
		return(body);
	}

	m=strlen(p);
	add_area=strListInit();
	del_area=strListInit();
	for(l=0;l<m;l++) {
		while(*(p+l) && IS_WHITESPACE(*(p+l))) l++;
		while(*(p+l)==CTRL_A) {				/* Ignore kludge lines June-13-2004 */
			while(*(p+l) && *(p+l)!='\r') l++;
			continue;
		}
		if(!(*(p+l))) break;
		if(*(p+l)=='+' || *(p+l)=='-' || *(p+l)=='%') {
			action=*(p+l);
			l++;
		}
		else
			action='+';
		SAFECOPY(str,p+l);
		truncstr(str,"\r\n");
		truncsp(str);	/* Remove trailing white-space, April-4-2014 */
		switch(action) {
			case '+':                       /* Add Area */
				strListPush(&add_area, str);
				break;
			case '-':                       /* Remove Area */
				strListPush(&del_area, str);
				break;
			case '%':                       /* Process Command */
				if(areafix_command(str, nodecfg, name))
					cmds++;
				break;
		}

		while(*(p+l) && *(p+l)!='\r') l++;
	}

	if(!cmds && strListIsEmpty(add_area) && strListIsEmpty(del_area)) {
		create_netmail(name,/* msg: */NULL,"Area Management Request"
			,"No commands to process.\r\nSend %HELP for help.\r\n"
			,/* dest: */addr, /* src: */NULL);
		strcat(body, "No valid AreaFix commands were detected.\r\n");
		strListFree(&add_area);
		strListFree(&del_area);
		return(body);
	}
	if(!strListIsEmpty(add_area) || !strListIsEmpty(del_area))
		alter_areas(add_area,del_area,addr,name);
	strListFree(&add_area);
	strListFree(&del_area);

	return(NULL);
}
/******************************************************************************
 This function will compare the archive signatures defined in the CFG file and
 extract 'infile' using the appropriate de-archiver.
******************************************************************************/
int unpack(const char *infile, const char* outdir)
{
	FILE *stream;
	char str[256],tmp[512];
	char error[256];
	int ch,file;
	unsigned u,j;
	long file_count;

	file_count = extract_files_from_archive(infile, outdir
		,/* allowed_filename_chars: */SAFEST_FILENAME_CHARS, /* with_path */false
		,/* max_files: */0, /* file_list = ALL */NULL, error, sizeof(error));
	if(file_count > 0) {
		lprintf(LOG_DEBUG, "libarchive extracted %lu files from %s", file_count, infile);
		return 0;
	}
	if(*error)
		lprintf(LOG_NOTICE, "libarchive error (%s) extracting %s", error, infile);

	if((stream=fnopen(&file,infile,O_RDONLY))==NULL) {
		lprintf(LOG_ERR,"ERROR %u (%s) opening archive: %s",errno,strerror(errno),infile);
		bail(1);
		return -1;
	}
	for(u=0;u<cfg.arcdefs;u++) {
		str[0]=0;
		if(fseek(stream,cfg.arcdef[u].byteloc,SEEK_SET) != 0)
			continue;
		for(j=0;j<strlen(cfg.arcdef[u].hexid)/2;j++) {
			ch=fgetc(stream);
			if(ch==EOF) {
				u=cfg.arcdefs;
				break;
			}
			sprintf(tmp,"%02X",ch);
			SAFECAT(str,tmp);
		}
		if(!stricmp(str,cfg.arcdef[u].hexid))
			break;
	}
	fclose(stream);

	if(u==cfg.arcdefs) {
		lprintf(LOG_ERR, "ERROR determining type of archive: %s", infile);
		return(1);
	}

	return execute(cmdstr(&scfg, /* user: */NULL, cfg.arcdef[u].unpack,infile, outdir, tmp, sizeof(tmp)));
}

/******************************************************************************
 This function will check the 'dest' for the type of archiver to use (as
 defined in the CFG file) and compress 'srcfile' into 'destfile' using the
 appropriate archive program.
******************************************************************************/
int pack(const char *srcfile, const char *destfile, fidoaddr_t dest)
{
	nodecfg_t*	nodecfg;
	arcdef_t*	archive;
	char tmp[512];

	if(!cfg.arcdefs) {
		lprintf(LOG_DEBUG, "ERROR: pack() called with no archive types configured!");
		return -1;
	}
	if((nodecfg=findnodecfg(&cfg, dest, 0)) != NULL)
		archive = nodecfg->archive;
	else
		archive = &cfg.arcdef[0];

	lprintf(LOG_DEBUG,"Packing packet (%s) into bundle (%s) for %s using %s"
		,srcfile, destfile, smb_faddrtoa(&dest, NULL), archive->name);
	if(strListFind((str_list_t)supported_archive_formats, archive->name, /* case_sensitive */FALSE) >= 0) {
		const char* file_list[] = { srcfile, NULL };
		if(create_archive(destfile, archive->name, /* with_path: */false, (str_list_t)file_list, tmp, sizeof(tmp)) == 1)
			return 0;
		lprintf(LOG_ERR, "libarchive error (%s) creating %s", tmp, destfile);
	}
	return execute(cmdstr(&scfg, /* user: */NULL, archive->pack, destfile, srcfile, tmp, sizeof(tmp)));
}

/* Reads a single FTS-1 stored message header from the specified file stream and terminates C-strings */
bool fread_fmsghdr(fmsghdr_t* hdr, FILE* fp)
{
	if(fread(hdr, sizeof(fmsghdr_t), 1, fp) != 1)
		return false;
	TERMINATE(hdr->from);
	TERMINATE(hdr->to);
	TERMINATE(hdr->subj);
	TERMINATE(hdr->time);
	return true;
}

enum attachment_mode {
	 ATTACHMENT_ADD
	,ATTACHMENT_NETMAIL
};

/* bundlename is the full path to the attached bundle file */
/* Returns 0 on success */
int attachment(const char *bundlename, fidoaddr_t dest, enum attachment_mode mode)
{
#if 1
	FILE *fidomsg,*stream;
	char str[MAX_PATH+1],*path,*p;
	char bundle_list_filename[MAX_PATH+1];
	int fmsg,file,error=0L;
	long fncrc,*mfncrc=0L,num_mfncrc=0L,crcidx;
    attach_t attach;
	fmsghdr_t hdr;
	size_t		f;
	glob_t		g;
#endif
	if(cfg.flo_mailer) {
		switch(mode) {
			case ATTACHMENT_ADD:
				return write_flofile(bundlename,dest,/* bundle: */true, cfg.use_outboxes
					,/* del_file: */true, /* attr: */0);
			case ATTACHMENT_NETMAIL:
				return 0; /* Do nothing */
		}
	}
#if 1
	if(bundlename==NULL && mode!=ATTACHMENT_NETMAIL) {
		lprintf(LOG_ERR,"ERROR line %d NULL bundlename",__LINE__);
		return(1);
	}
	SAFEPRINTF(bundle_list_filename,"%sbbsecho.bundles",cfg.outbound);
	if((stream=fnopen(&file,bundle_list_filename,O_RDWR|O_CREAT))==NULL) {
		lprintf(LOG_ERR,"ERROR %u (%s) opening %s",errno,strerror(errno),bundle_list_filename);
		return(1);
	}

	if(mode==ATTACHMENT_NETMAIL) {				/* Create netmail attaches */

		if(!filelength(file)) {
			fclose(stream);
			return(0);
		}
										/* Get attach names from existing MSGs */
#ifdef __unix__
		sprintf(str,"%s*.[Mm][Ss][Gg]",scfg.netmail_dir);
#else
		sprintf(str,"%s*.msg",scfg.netmail_dir);
#endif
		glob(str,0,NULL,&g);
		for(f=0;f<g.gl_pathc && !terminated;f++) {

			path=g.gl_pathv[f];

			if((fidomsg=fnopen(&fmsg,path,O_RDWR))==NULL) {
				lprintf(LOG_ERR,"ERROR %u (%s) line %d opening %s",errno,strerror(errno),__LINE__,path);
				continue;
			}
			if(filelength(fmsg)<sizeof(fmsghdr_t)) {
				lprintf(LOG_ERR,"ERROR line %d %s has invalid length of %lu bytes"
					,__LINE__
					,path
					,filelength(fmsg));
				fclose(fidomsg);
				continue;
			}
			if(!fread_fmsghdr(&hdr,fidomsg)) {
				fclose(fidomsg);
				lprintf(LOG_ERR,"ERROR line %d reading fido msghdr from %s",__LINE__,path);
				continue;
			}
			fclose(fidomsg);
			if(!(hdr.attr&FIDO_FILE))		/* Not a file attach */
				continue;
			num_mfncrc++;
			p=getfname(hdr.subj);
			if((mfncrc=(long *)realloc(mfncrc,num_mfncrc*sizeof(long)))==NULL) {
				lprintf(LOG_ERR,"ERROR line %d allocating %lu for bundle name crc"
					,__LINE__,num_mfncrc*sizeof(long));
				continue;
			}
			mfncrc[num_mfncrc-1]=crc32(strupr(p),0);
		}
		globfree(&g);

		while(!feof(stream)) {
			if(fread(&attach,1,sizeof(attach_t),stream) != sizeof(attach_t))
				break;
			TERMINATE(attach.filename);
			if(!fexistcase(attach.filename))
				continue;
			fncrc=crc32(attach.filename,0);
			for(crcidx=0;crcidx<num_mfncrc;crcidx++)
				if(mfncrc[crcidx]==fncrc)
					break;
			if(crcidx==num_mfncrc)
				if(create_netmail(/* To: */NULL
					,/* msg: */NULL
					,/* subj: */attach.filename
					,(cfg.trunc_bundles) ? "\1FLAGS TFS\r" : "\1FLAGS KFS\r"
					,attach.dest
					,/* src: */NULL))
					error=1;
		}
		fclose(stream);
		if(!error)			/* remove bundles.sbe if no error occurred */
			delfile(bundle_list_filename, __LINE__);	/* used to truncate here, August-20-2002 */
		if(num_mfncrc)
			free(mfncrc);
		return(0);
	}

	while(!feof(stream)) {
		if(!fread(&attach,sizeof(attach_t),1,stream))
			break;
		TERMINATE(attach.filename);
		if(stricmp(attach.filename,bundlename) == 0) {
			fclose(stream);
			return(0);
		}
	}

	memcpy(&attach.dest,&dest,sizeof(fidoaddr_t));
	SAFECOPY(attach.filename,bundlename);
	/* TODO: Write of unpacked struct */
	(void)fwrite(&attach,sizeof(attach_t),1,stream);
	fclose(stream);
#endif
	return(0);
}

/******************************************************************************
 This function returns a packet name - used for outgoing packets
******************************************************************************/
char *pktname(const char* outbound)
{
	static char path[MAX_PATH+1];
    time_t t;

	for(t = time(NULL); t != 0; t++) {
		SAFEPRINTF2(path,"%s%lx.pkt",outbound, t);
		if(!fexist(path))				/* Add 1 second if name exists */
			return path;
	}
	return NULL;	/* This should never happen */
}

/******************************************************************************
 This function is called when a message packet has reached it's maximum size.
 It places packets into a bundle until that bundle is full, at which time the
 last character of the extension increments (1 thru 0 and then A thru Z).  If
 all bundles have reached their maximum size remaining packets are placed into
 the Z bundle.
******************************************************************************/
bool pack_bundle(const char *tmp_pkt, fidoaddr_t orig, fidoaddr_t dest)
{
	const char* outbound;
	char*		packet;
	char		bundle[MAX_PATH+1];
	char		fname[MAX_PATH+1];
	char		new_pkt[MAX_PATH+1];
	char		str[128];
	char		day[3];
	int			i,file;
	time_t		now;
	nodecfg_t*	nodecfg;

	if(!bso_lock_node(dest))
		return false;

	nodecfg=findnodecfg(&cfg, dest,0);
	lprintf(LOG_INFO,"Sending packet (%s, %1.1fKB) from %s to %s"
		, tmp_pkt, flength(tmp_pkt)/1024.0, smb_faddrtoa(&orig, str), smb_faddrtoa(&dest,NULL));

	if(cfg.flo_mailer) {
		if(nodecfg!=NULL && !nodecfg->direct && nodecfg->route.zone) {
			dest=nodecfg->route;
			lprintf(LOG_NOTICE,"Routing packet (%s) to %s",tmp_pkt, smb_faddrtoa(&dest,NULL));
		}
		if(nodecfg == NULL && dest.point != 0) {
			fidoaddr_t	boss = dest;
			boss.point = 0;
			if((nodecfg = findnodecfg(&cfg, boss, /* exact: */true)) != NULL) {
				dest = boss;
				lprintf(LOG_INFO, "Routing packet (%s) to boss-node %s", tmp_pkt, smb_faddrtoa(&dest, NULL));
			}
		}
	}
	outbound = get_current_outbound(dest, cfg.use_outboxes);
	if(outbound == NULL)
		return false;

	/* Try to use the same filename as the temp packet, if we can */
	SAFEPRINTF2(new_pkt, "%s%s", outbound, getfname(tmp_pkt));
	if(!fexist(new_pkt))
		packet = new_pkt;
	else
		packet = pktname(outbound);
	lprintf(LOG_DEBUG, "Moving packet for %s: %s to %s", smb_faddrtoa(&dest,NULL), tmp_pkt, packet);
	if(mv(tmp_pkt,packet, /* copy: */false)) {
		lprintf(LOG_ERR,"ERROR moving %s to %s", tmp_pkt, packet);
		return false;
	}
	time(&now);
	sprintf(day,"%-.2s",ctime(&now));
	strupr(day);

	if(nodecfg != NULL)
		if(nodecfg->archive==SBBSECHO_ARCHIVE_NONE) {    /* Uncompressed! */
			if(cfg.flo_mailer)
				i=write_flofile(packet,dest,/* bundle: */true, cfg.use_outboxes, /* del_file: */true, /* attr: */0);
			else
				i=create_netmail(/* To: */NULL,/* msg: */NULL,packet
					,(cfg.trunc_bundles) ? "\1FLAGS TFS\r" : "\1FLAGS KFS\r"
					,dest, /* src: */NULL);
			return i==0;
		}

	if(dest.point)
		SAFEPRINTF3(fname,"%s0000p%03hx.%s",outbound,(uint16_t)dest.point,day);
	else
		SAFEPRINTF4(fname,"%s%04hx%04hx.%s",outbound,(uint16_t)(orig.net-dest.net), (uint16_t)(orig.node-dest.node), day);
	for(i='0';i<='Z';i++) {
		if(i==':')
			i='A';
		SAFEPRINTF2(bundle,"%s%c",fname,i);
		if(flength(bundle)==0) {
			/* Feb-10-2003: Don't overwrite or delete 0-byte file less than 24hrs old */
			/* This is used in combination with "Truncate Bundles" option so that bundle names are not reused
			 * after being sent (and deleted) by the mailer which can confuse some links.
			 */
			if((time(NULL)-fdate(bundle))<24L*60L*60L)
				continue;
			delfile(bundle, __LINE__);
		}
		if(fexistcase(bundle)) {
			if(i!='Z' && flength(bundle)>=(off_t)cfg.maxbdlsize) {
				if(attachment(bundle,dest,ATTACHMENT_ADD) == 0)
					bundles_sent++;
				continue;
			}
			file=sopen(bundle,O_WRONLY,SH_DENYRW);
			if(file==-1)		/* Can't open?!? Probably being sent */
				continue;
			close(file);
		}
		break;
	}
	if(i > 'Z')
		lprintf(LOG_WARNING,"All bundle files for %s already exist, adding to: %s"
			,smb_faddrtoa(&dest,NULL), bundle);
	if(pack(packet,bundle,dest))
		return false;
	if(attachment(bundle,dest,ATTACHMENT_ADD))
		return false;
	bundles_sent++;
	return delfile(packet, __LINE__);
}

/******************************************************************************
 This function checks the inbound directory for the first bundle it finds, it
 will then unpack and delete the bundle.  If no bundles exist this function
 returns a false, otherwise a true is returned.
 ******************************************************************************/
bool unpack_bundle(const char* inbound)
{
	char*			p;
	char			str[MAX_PATH+1];
	char			fname[MAX_PATH+1];
	int				i;
	static glob_t	g;
	static size_t	gi;

	for(i=0;i<7 && !terminated;i++) {
#if defined(__unix__)	/* support upper or lower case */
		switch(i) {
			case 0:
				p="[Ss][Uu]";
				break;
			case 1:
				p="[Mm][Oo]";
				break;
			case 2:
				p="[Tt][Uu]";
				break;
			case 3:
				p="[Ww][Ee]";
				break;
			case 4:
				p="[Tt][Hh]";
				break;
			case 5:
				p="[Ff][Rr]";
				break;
			default:
				p="[Ss][Aa]";
				break;
		}
#else
		switch(i) {
			case 0:
				p="su";
				break;
			case 1:
				p="mo";
				break;
			case 2:
				p="tu";
				break;
			case 3:
				p="we";
				break;
			case 4:
				p="th";
				break;
			case 5:
				p="fr";
				break;
			default:
				p="sa";
				break;
		}
#endif
		SAFEPRINTF2(str,"%s*.%s?",inbound,p);
		if(gi>=g.gl_pathc) {
			gi=0;
			globfree(&g);
			glob(str,0,NULL,&g);
		}
		if(gi<g.gl_pathc) {
			SAFECOPY(fname,g.gl_pathv[gi]);
			gi++;
			off_t length = flength(fname);
			if(length < 1) {
				if(fdate(fname) < time(NULL) + (24*60*60))
					lprintf(LOG_DEBUG, "Ignoring %ld-byte file (less than 24-hours old): %s", (long)length, fname);
				else {
					lprintf(LOG_INFO, "Deleting %ld-byte file (more than 24-hours old): %s", (long)length, fname);
					delfile(fname, __LINE__);	/* Delete it if it's a 0-byte file */
				}
				continue;
			}
			lprintf(LOG_INFO,"Unpacking bundle: %s (%1.1fKB)", fname, length/1024.0);
			if(unpack(fname, inbound) != 0) {	/* failure */
				lprintf(LOG_ERR, "!Unpack failure: %s", fname);
				/* rename to "*.?_?" or (if it exists) "*.?-?" */
				SAFECOPY(str,fname);
				str[strlen(str)-2]='_';
				if(fexistcase(str))
					str[strlen(str)-2]='-';
				if(fexistcase(str))
					delfile(str, __LINE__);
				if(rename(fname,str))
					lprintf(LOG_ERR,"ERROR line %d renaming %s to %s"
						,__LINE__,fname,str);
				continue;
			}
			delfile(fname, __LINE__);	/* successful, so delete bundle */
			bundles_unpacked++;
			return(true);
		}
	}

	return(false);
}

/****************************************************************************/
/* Moves or copies a file from one dir to another                           */
/* both 'src' and 'dest' must contain full path and filename                */
/* returns 0 if successful, -1 if error                                     */
/****************************************************************************/
int mv(const char *insrc, const char *indest, bool copy)
{
	char buf[4096];
	char src[MAX_PATH+1];
	char dest[MAX_PATH+1];
	int  ind,outd;
	off_t length;
	long chunk=4096,l;
    FILE *inp,*outp;

	SAFECOPY(src, insrc);
	SAFECOPY(dest, indest);
	if(!strcmp(src,dest))	/* source and destination are the same! */
		return(0);
//	lprintf(LOG_DEBUG, "%s file from %s to %s", copy ? "Copying":"Moving", src, dest);
	if(!fexistcase(src)) {
		lprintf(LOG_WARNING,"MV ERROR: Source file doesn't exist '%s'",src);
		return(-1);
	}
	if(isdir(dest)) {
		backslash(dest);
		SAFECAT(dest, getfname(src));
	}
	if(!copy && fexistcase(dest)) {
		lprintf(LOG_WARNING,"MV ERROR: Destination file already exists '%s'",dest);
		return(-1);
	}
	if(!copy
#ifndef __unix__
		&& ((src[1]!=':' && dest[1]!=':')
		|| (src[1]==':' && dest[1]==':' && toupper(src[0])==toupper(dest[0])))
#endif
		) {
		if(rename(src,dest)==0)		/* same drive, so move */
			return(0);
		/* rename failed, so attempt copy */
	}
	if((ind=nopen(src,O_RDONLY))==-1) {
		lprintf(LOG_ERR,"MV ERROR %u (%s) opening '%s'",errno,strerror(errno),src);
		return(-1);
	}
	if((inp=fdopen(ind,"rb"))==NULL) {
		close(ind);
		lprintf(LOG_ERR,"MV ERROR %u (%s) fdopening '%s'",errno,strerror(errno),src);
		return(-1);
	}
	setvbuf(inp,NULL,_IOFBF,8*1024);
	if((outd=nopen(dest,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
		fclose(inp);
		lprintf(LOG_ERR,"MV ERROR %u (%s) opening '%s'",errno,strerror(errno),dest);
		return(-1);
	}
	if((outp=fdopen(outd,"wb"))==NULL) {
		close(outd);
		fclose(inp);
		lprintf(LOG_ERR,"MV ERROR %u (%s) fdopening '%s'",errno,strerror(errno),dest);
		return(-1);
	}
	setvbuf(outp,NULL,_IOFBF,8*1024);
	length=filelength(ind);
	l=0L;
	int result = 0;
	while(l<length) {
		if(l+chunk>length)
			chunk=(long)(length-l);
		if(fread(buf,1,chunk,inp) != chunk) {
			result = -2;
			break;
		}
		(void)fwrite(buf,chunk,1,outp);
		l+=chunk;
	}
	fclose(inp);
	fclose(outp);
	if(result == 0 && !copy)
		delfile(src, __LINE__);
	return result;
}

/****************************************************************************/
/* Returns negative value on error											*/
/****************************************************************************/
long getlastmsg(uint subnum, uint32_t *ptr, /* unused: */time_t *t)
{
	int i;
	smb_t smbfile;

	if(ptr) (*ptr)=0;
	ZERO_VAR(smbfile);
	if(subnum>=scfg.total_subs) {
		lprintf(LOG_ERR,"ERROR line %d getlastmsg %d",__LINE__,subnum);
		bail(1);
		return -1;
	}
	sprintf(smbfile.file,"%s%s",scfg.sub[subnum]->data_dir,scfg.sub[subnum]->code);
	smbfile.retry_time=scfg.smb_retry_time;
	if((i=smb_open(&smbfile))!=SMB_SUCCESS) {
		lprintf(LOG_ERR,"ERROR %d (%s) line %d opening %s",i,smbfile.last_error,__LINE__,smbfile.file);
		return -1;
	}

	if(!filelength(fileno(smbfile.shd_fp))) {			/* Empty base */
		if(ptr) (*ptr)=0;
		smb_close(&smbfile);
		return(0);
	}
	smb_close(&smbfile);
	if(ptr) (*ptr)=smbfile.status.last_msg;
	return(smbfile.status.total_msgs);
}


ulong loadmsgs(smb_t* smb, post_t** post, ulong ptr)
{
	int i;
	ulong l,total;
	idxrec_t idx;

	if((i=smb_locksmbhdr(smb))!=SMB_SUCCESS) {
		lprintf(LOG_ERR,"ERROR %d (%s) line %d locking %s",i,smb->last_error,__LINE__,smb->file);
		return(0);
	}

	/* total msgs in sub */
	total=(ulong)filelength(fileno(smb->sid_fp))/sizeof(idxrec_t);

	if(!total) {			/* empty */
		smb_unlocksmbhdr(smb);
		return(0);
	}

	if(((*post)=(post_t*)malloc((size_t)(sizeof(post_t)*total)))    /* alloc for max */
		==NULL) {
		smb_unlocksmbhdr(smb);
		lprintf(LOG_ERR,"ERROR line %d allocating %lu bytes for %s",__LINE__
			,sizeof(post_t *)*total,smb->file);
		return(0);
	}

	if(fseek(smb->sid_fp,0L,SEEK_SET) != 0) {
		lprintf(LOG_ERR, "ERROR %u (%s) line %d rewinding index of %s"
			,errno, strerror(errno), __LINE__, smb->file);
		return 0;
	}
	for(l=0;l<total && !feof(smb->sid_fp); ) {
		if(smb_fread(smb, &idx,sizeof(idx),smb->sid_fp) != sizeof(idx))
			break;

		if(idx.number==0)	/* invalid message number, ignore */
			continue;

		if(idx.attr&MSG_POLL_VOTE_MASK)
			continue;

		if(idx.number<=ptr || (idx.attr&MSG_DELETE))
			continue;

		if((idx.attr&MSG_MODERATED) && !(idx.attr&MSG_VALIDATED))
			break;

		(*post)[l++].idx=idx;
	}
	smb_unlocksmbhdr(smb);
	if(!l)
		FREE_AND_NULL(*post);
	return(l);
}

const char* area_desc(const char* areatag)
{
	char tag[FIDO_AREATAG_LEN+1];
	static char desc[128];

	for(uint i=0; i<cfg.listcfgs; i++) {
		FILE* fp = fopen(cfg.listcfg[i].listpath, "r");
		if(fp == NULL) {
			lprintf(LOG_ERR, "ERROR %d (%s) opening %s", errno, strerror(errno), cfg.listcfg[i].listpath);
			continue;
		}
		str_list_t list = strListReadFile(fp, NULL, 0);
		fclose(fp);
		if(list == NULL)
			continue;
		strListTruncateTrailingWhitespaces(list);
		for(int l=0; list[l] != NULL; l++) {
			SAFECOPY(tag, list[l]);
			truncstr(tag, " \t");
			if(stricmp(tag, areatag))
				continue;
			char* p = list[l];
			FIND_WHITESPACE(p);	// Skip the tag
			if(*p == 0)
				break;
			SKIP_WHITESPACE(p);	// Find the desc
			if(*p == 0)
				break;
			SAFECOPY(desc, p);
			return desc;
		}
	}

	return "";
}

void cleanup(void)
{
	char*		p;
	char		path[MAX_PATH+1];

	if(bad_areas != NULL) {
		lprintf(LOG_DEBUG, "Writing %lu areas to %s", (ulong)strListCount(bad_areas), cfg.badareafile);
		FILE* fp = fopen(cfg.badareafile, "wt");
		if(fp == NULL) {
			lprintf(LOG_ERR, "ERROR %d (%s) opening %s", errno, strerror(errno), cfg.badareafile);
		} else {
			int longest = 0;
			for(int i=0; bad_areas[i] != NULL; i++) {
				int len = strlen(bad_areas[i]);
				if(len > longest) longest = len;
			}
			strListSortAlpha(bad_areas);
			for(int i=0; bad_areas[i] != NULL; i++) {
				p = bad_areas[i];
//				lprintf(LOG_DEBUG, "Writing '%s' (%p) to %s", p, p, cfg.badareafile);
				fprintf(fp, "%-*s %s\n", longest, p, area_desc(p));
			}
			fclose(fp);
		}
		strListFree(&bad_areas);
	}
	while((p=strListPop(&locked_bso_nodes)) != NULL) {
		delfile(p, __LINE__);
		free(p);
	}

	if(mtxfile_locked) {
		SAFEPRINTF(path,"%ssbbsecho.bsy", scfg.ctrl_dir);
		if(delfile(path, __LINE__))
			mtxfile_locked = false;
	}
}

void bail(int error_level)
{
	cleanup();

	if(cfg.log_level == LOG_DEBUG
		|| error_level
		|| netmail || exported_netmail || packed_netmail
		|| echomail || exported_echomail
		|| packets_imported || packets_sent
		|| bundles_unpacked || bundles_sent) {
		char signoff[1024];
		sprintf(signoff, "SBBSecho (PID %u) exiting with error level %d", getpid(), error_level);
		if(bundles_unpacked || bundles_sent)
			sprintf(signoff+strlen(signoff), ", Bundles(%lu unpacked, %lu sent)", bundles_unpacked, bundles_sent);
		if(packets_imported || packets_sent)
			sprintf(signoff+strlen(signoff), ", Packets(%lu imported, %lu sent)", packets_imported, packets_sent);
		if(netmail || exported_netmail || packed_netmail)
			sprintf(signoff+strlen(signoff), ", NetMail(%lu imported, %lu exported, %lu packed)"
				,netmail, exported_netmail, packed_netmail);
		if(echomail || exported_echomail)
			sprintf(signoff+strlen(signoff), ", EchoMail(%lu imported, %lu exported)"
				,echomail, exported_echomail);
		lprintf(LOG_INFO, "%s", signoff);
	}
	if((error_level && pause_on_abend) || pause_on_exit) {
		fprintf(stderr,"\nHit any key...");
		getch();
		fprintf(stderr,"\n");
	}
	exit(error_level);
}

void break_handler(int type)
{
	lprintf(LOG_NOTICE,"\n-> Terminated Locally (signal: %d)",type);
	terminated = true;
}

#if defined(_WIN32)
BOOL WINAPI ControlHandler(unsigned long CtrlType)
{
	break_handler((int)CtrlType);
	return TRUE;
}
#endif

link_list_t user_list;

/****************************************************************************/
/* Converts goofy FidoNet time format into Unix format						*/
/****************************************************************************/
time32_t fmsgtime(const char *str)
{
	char month[4];
	struct tm tm;

	memset(&tm,0,sizeof(tm));
	tm.tm_isdst=-1;	/* Do not adjust for DST */

	if(IS_DIGIT(str[1])) {	/* Regular format: "01 Jan 86  02:34:56" */
		tm.tm_mday=atoi(str);
		sprintf(month,"%3.3s",str+3);
		if(!stricmp(month,"jan"))
			tm.tm_mon=0;
		else if(!stricmp(month,"feb"))
			tm.tm_mon=1;
		else if(!stricmp(month,"mar"))
			tm.tm_mon=2;
		else if(!stricmp(month,"apr"))
			tm.tm_mon=3;
		else if(!stricmp(month,"may"))
			tm.tm_mon=4;
		else if(!stricmp(month,"jun"))
			tm.tm_mon=5;
		else if(!stricmp(month,"jul"))
			tm.tm_mon=6;
		else if(!stricmp(month,"aug"))
			tm.tm_mon=7;
		else if(!stricmp(month,"sep"))
			tm.tm_mon=8;
		else if(!stricmp(month,"oct"))
			tm.tm_mon=9;
		else if(!stricmp(month,"nov"))
			tm.tm_mon=10;
		else
			tm.tm_mon=11;
		tm.tm_year=atoi(str+7);
		if(tm.tm_year<Y2K_2DIGIT_WINDOW)
			tm.tm_year+=100;
		tm.tm_hour=atoi(str+11);
		tm.tm_min=atoi(str+14);
		tm.tm_sec=atoi(str+17);
	}

	else {					/* SEAdog  format: "Mon  1 Jan 86 02:34" */
		tm.tm_mday=atoi(str+4);
		sprintf(month,"%3.3s",str+7);
		if(!stricmp(month,"jan"))
			tm.tm_mon=0;
		else if(!stricmp(month,"feb"))
			tm.tm_mon=1;
		else if(!stricmp(month,"mar"))
			tm.tm_mon=2;
		else if(!stricmp(month,"apr"))
			tm.tm_mon=3;
		else if(!stricmp(month,"may"))
			tm.tm_mon=4;
		else if(!stricmp(month,"jun"))
			tm.tm_mon=5;
		else if(!stricmp(month,"jul"))
			tm.tm_mon=6;
		else if(!stricmp(month,"aug"))
			tm.tm_mon=7;
		else if(!stricmp(month,"sep"))
			tm.tm_mon=8;
		else if(!stricmp(month,"oct"))
			tm.tm_mon=9;
		else if(!stricmp(month,"nov"))
			tm.tm_mon=10;
		else
			tm.tm_mon=11;
		tm.tm_year=atoi(str+11);
		if(tm.tm_year<Y2K_2DIGIT_WINDOW)
			tm.tm_year+=100;
		tm.tm_hour=atoi(str+14);
		tm.tm_min=atoi(str+17);
		tm.tm_sec=0;
	}
	return(mktime32(&tm));
}

static short fmsgzone(const char* p)
{
	char	hr[4]="";
	char	min[4]="";
	short	val;
	bool	west=true;

	if(*p=='-')
		p++;
	else
		west=false;

	if(strlen((char*)p)>=2)
		sprintf(hr,"%.2s",p);
	if(strlen((char*)p+2)>=2)
		sprintf(min,"%.2s",p+2);

	val=atoi(hr)*60;
	val+=atoi(min);

	if(west)
		return(-val);
	return(val);
}

char* getfmsg(FILE* stream, ulong* outlen)
{
	uchar* fbuf;
	int ch;
	off_t start;
	ulong l,length;

	length=0L;
	start=ftello(stream);						/* Beginning of Message */
	if(start < 0) {
		lprintf(LOG_ERR, "ERROR %d line %d getting file offset", errno, __LINE__);
		return NULL;
	}
	while(1) {
		ch=fgetc(stream);						/* Look for Terminating NULL */
		if(ch==0 || ch==EOF)					/* Found end of message */
			break;
		length++;	 							/* Increment the Length */
	}

	if((fbuf = malloc(length+1)) == NULL) {
		lprintf(LOG_ERR,"ERROR line %d allocating %lu bytes of memory",__LINE__,length+1);
		bail(1);
		return(NULL);
	}

	(void)fseeko(stream,start,SEEK_SET);
	for(l=0;l<length;l++)
		fbuf[l]=(uchar)fgetc(stream);
	if(ch==0)
		(void)fgetc(stream);		/* Read NULL */

	while(length && fbuf[length-1]<=' ')	/* truncate white-space */
		length--;
	fbuf[length]=0;

	if(outlen)
		*outlen=length;
	return((char*)fbuf);
}

#define MAX_TAILLEN 1024

enum {
	 IMPORT_FAILURE			= -1
	,IMPORT_SUCCESS			= SMB_SUCCESS
	,IMPORT_FILTERED_DUPE	= SMB_DUPE_MSG
	,IMPORT_FILTERED_TWIT	= 2
	,IMPORT_FILTERED_EMPTY	= 3
	,IMPORT_FILTERED_AGE	= 4
};

/****************************************************************************/
/* Coverts a FidoNet message into a Synchronet message						*/
/* Returns 0 on success, 1 dupe, 2 filtered, 3 empty, 4 too-old				*/
/* or other SMB error														*/
/****************************************************************************/
int fmsgtosmsg(char* fbuf, fmsghdr_t* hdr, uint usernumber, uint subnum)
{
	uchar	ch,stail[MAX_TAILLEN+1],*sbody;
	char	msg_id[256],str[128],*p;
	char	cmd[512];
	bool	done,cr;
	int 	i;
	ushort	xlat=XLAT_NONE,net;
	ulong	l,m,length,bodylen,taillen;
	ulong	save;
	long	dupechk_hashes=SMB_HASH_SOURCE_DUPE;
	fidoaddr_t faddr,origaddr,destaddr;
	smb_t*	smbfile;
	smbmsg_t	msg;
	time32_t now=time32(NULL);
	ulong	max_msg_age = (subnum == INVALID_SUB) ? cfg.max_netmail_age : cfg.max_echomail_age;

	if(findstr_in_list(hdr->from, twit_list) || findstr_in_list(hdr->to, twit_list)) {
		lprintf(LOG_INFO,"Filtering message from %s to %s",hdr->from,hdr->to);
		return IMPORT_FILTERED_TWIT;
	}

	memset(&msg,0,sizeof(smbmsg_t));
	if(hdr->attr&FIDO_PRIVATE)
		msg.hdr.attr|=MSG_PRIVATE;
	if(scfg.sys_misc&SM_DELREADM)
		msg.hdr.attr|=MSG_KILLREAD;
	if(hdr->attr&FIDO_FILE)
		msg.hdr.auxattr|=MSG_FILEATTACH;
	if(hdr->attr&FIDO_INTRANS)
		msg.hdr.netattr|=MSG_INTRANSIT;

	msg.hdr.when_imported.time=now;
	msg.hdr.when_imported.zone=sys_timezone(&scfg);
	if(hdr->time[0]) {
		msg.hdr.when_written.time=fmsgtime(hdr->time);
		if(max_msg_age && (time32_t)msg.hdr.when_written.time < now
			&& (now - msg.hdr.when_written.time) > max_msg_age) {
			lprintf(LOG_INFO, "Filtering message from %s due to age: %1.1f days"
				,hdr->from
				,(now - msg.hdr.when_written.time) / (24.0*60.0*60.0));
			return IMPORT_FILTERED_AGE;
		}
	} else
		msg.hdr.when_written = msg.hdr.when_imported;

	origaddr.zone=hdr->origzone; 	/* only valid if NetMail */
	origaddr.net=hdr->orignet;
	origaddr.node=hdr->orignode;
	origaddr.point=hdr->origpoint;

	destaddr.zone=hdr->destzone; 	/* only valid if NetMail */
	destaddr.net=hdr->destnet;
	destaddr.node=hdr->destnode;
	destaddr.point=hdr->destpoint;

	smb_hfield_str(&msg,SENDER,hdr->from);
	smb_hfield_str(&msg,RECIPIENT,hdr->to);

	if(usernumber) {
		user_t user = { .number = usernumber };
		i = getuserdat(&scfg, &user);
		if(i != 0) {
			lprintf(LOG_ERR, "Error %d reading user #%u", i, usernumber);
			return SMB_FAILURE;
		}
		uint16_t nettype;
		if((scfg.sys_misc&SM_FWDTONET) && (user.misc&NETMAIL)
			&& (nettype = smb_netaddr_type(user.netmail)) >= NET_UNKNOWN) {
			lprintf(LOG_INFO, "Forwarding message from %s to %s", hdr->from, user.netmail);
			smb_hfield_netaddr(&msg, RECIPIENTNETADDR, user.netmail, &nettype);
			smb_hfield_bin(&msg, RECIPIENTNETTYPE, nettype);
		} else {
			sprintf(str,"%u",usernumber);
			smb_hfield_str(&msg,RECIPIENTEXT,str);
		}
	}

	smb_hfield_str(&msg,SUBJECT,hdr->subj);

	if(fbuf==NULL) {
		lprintf(LOG_ERR,"ERROR line %d allocating fbuf",__LINE__);
		smb_freemsgmem(&msg);
		return IMPORT_FAILURE;
	}
	length=strlen(fbuf);
	if((sbody=(uchar*)malloc((length+1)*2))==NULL) {
		lprintf(LOG_ERR,"ERROR line %d allocating %lu bytes for body",__LINE__
			,(length+1)*2L);
		smb_freemsgmem(&msg);
		return IMPORT_FAILURE;
	}

	for(l=0,done=0,bodylen=0,taillen=0,cr=1;l<length;l++) {

		if(!l && !strncmp(fbuf,"AREA:",5)) {
			save=l;
			l+=5;
			while(l<length && fbuf[l]<=' ' && fbuf[l]>=0) l++;
			m=l;
			while(m<length && fbuf[m]!='\r') m++;
			while(m && fbuf[m-1]<=' ' && fbuf[m-1]>=0) m--;
			if(m>l)
				smb_hfield(&msg,FIDOAREA,(ushort)(m-l),fbuf+l);
			while(l<length && fbuf[l]!='\r') l++;
			/* If unknown echo, keep AREA: line in message body */
			if(cfg.badecho>=0 && subnum==cfg.area[cfg.badecho].sub)
				l=save;
			else
				continue;
		}

		ch=fbuf[l];
		if(ch==CTRL_A && !cr)
			ch = '@';
		else if(ch==CTRL_A) {	/* kludge line */

			if(!strncmp(fbuf+l+1,"TOPT ",5))
				destaddr.point=atoi(fbuf+l+6);

			else if(!strncmp(fbuf+l+1,"FMPT ",5))
				origaddr.point=atoi(fbuf+l+6);

			else if(!strncmp(fbuf+l+1,"INTL ",5)) {
				faddr=atofaddr(fbuf+l+6);
				destaddr.zone=faddr.zone;
				destaddr.net=faddr.net;
				destaddr.node=faddr.node;
				l+=6;
				while(l<length && fbuf[l]!=' ') l++;
				faddr=atofaddr(fbuf+l+1);
				origaddr.zone=faddr.zone;
				origaddr.net=faddr.net;
				origaddr.node=faddr.node;
			}

			else if(!strncmp(fbuf+l+1,"MSGID:",6)) {
				l+=7;
				while(l<length && fbuf[l]<=' ' && fbuf[l]>=0) l++;
				m=l;
				while(m<length && fbuf[m]!='\r') m++;
				if(m>l)
					smb_hfield(&msg,FIDOMSGID,(ushort)(m-l),fbuf+l);
			}

			else if(!strncmp(fbuf+l+1,"REPLY:",6)) {
				l+=7;
				while(l<length && fbuf[l]<=' ' && fbuf[l]>=0) l++;
				m=l;
				while(m<length && fbuf[m]!='\r') m++;
				if(m>l)
					smb_hfield(&msg,FIDOREPLYID,(ushort)(m-l),fbuf+l);
			}

			else if(!strncmp(fbuf+l+1,"FLAGS ",6)		/* correct */
				||  !strncmp(fbuf+l+1,"FLAGS:",6)) {	/* incorrect */
				l+=7;
				while(l<length && fbuf[l]<=' ' && fbuf[l]>=0) l++;
				m=l;
				while(m<length && fbuf[m]!='\r') m++;
				while(m && fbuf[m-1]<=' ' && fbuf[m-1]>=0) m--;
				if(m>l)
					smb_hfield(&msg,FIDOFLAGS,(ushort)(m-l),fbuf+l);
			}

			else if(!strncmp(fbuf+l+1,"PATH:",5)) {
				l+=6;
				while(l<length && fbuf[l]<=' ' && fbuf[l]>=0) l++;
				m=l;
				while(m<length && fbuf[m]!='\r') m++;
				while(m && fbuf[m-1]<=' ' && fbuf[m-1]>=0) m--;
				if(m>l)
					smb_hfield(&msg,FIDOPATH,(ushort)(m-l),fbuf+l);
			}

			else if(!strncmp(fbuf+l+1,"PID:",4)) {
				l+=5;
				while(l<length && fbuf[l]<=' ' && fbuf[l]>=0) l++;
				m=l;
				while(m<length && fbuf[m]!='\r') m++;
				while(m && fbuf[m-1]<=' ' && fbuf[m-1]>=0) m--;
				if(m>l)
					smb_hfield(&msg,FIDOPID,(ushort)(m-l),fbuf+l);
			}

			else if(!strncmp(fbuf+l+1,"TID:",4)) {
				l+=5;
				while(l<length && fbuf[l]<=' ' && fbuf[l]>=0) l++;
				m=l;
				while(m<length && fbuf[m]!='\r') m++;
				while(m && fbuf[m-1]<=' ' && fbuf[m-1]>=0) m--;
				if(m>l)
					smb_hfield(&msg,FIDOTID,(ushort)(m-l),fbuf+l);
			}

			else if(!strncmp(fbuf+l+1, "NOTE:", 5)) {
				l+=6;
				while(l<length && fbuf[l]<=' ' && fbuf[l]>=0) l++;
				m=l;
				while(m<length && fbuf[m]!='\r') m++;
				while(m && fbuf[m-1]<=' ' && fbuf[m-1]>=0) m--;
				if(m>l)
					smb_hfield(&msg, SMB_EDITOR, (ushort)(m-l), fbuf+l);
			}

			else if(!strncmp(fbuf+l+1, "CHRS:", 5)) {
				l+=6;
				while(l<length && fbuf[l]<=' ' && fbuf[l]>=0) l++;
				m=l;
				while(m<length && fbuf[m]!='\r') m++;
				while(m && fbuf[m-1]<=' ' && fbuf[m-1]>=0) m--;
				if(m>l) {
					smb_hfield(&msg, FIDOCHARSET, (ushort)(m-l), fbuf+l);
				}
			}

			else if(!strncmp(fbuf+l+1, "CHARSET:", 8)) {
				l+=9;
				while(l<length && fbuf[l]<=' ' && fbuf[l]>=0) l++;
				m=l;
				while(m<length && fbuf[m]!='\r') m++;
				while(m && fbuf[m-1]<=' ' && fbuf[m-1]>=0) m--;
				if(m>l)
					smb_hfield(&msg, FIDOCHARSET, (ushort)(m-l), fbuf+l);
			}

			else if(!strncmp(fbuf+l+1,"TZUTC:",6)) {		/* FSP-1001 */
				l+=7;
				while(l<length && fbuf[l]<=' ' && fbuf[l]>=0) l++;
				msg.hdr.when_written.zone = fmsgzone(fbuf+l);
			}

			else if(!strncmp(fbuf+l+1,"TZUTCINFO:",10)) {	/* non-standard */
				l+=11;
				while(l<length && fbuf[l]<=' ' && fbuf[l]>=0) l++;
				msg.hdr.when_written.zone = fmsgzone(fbuf+l);
			}

			else if(!strncmp(fbuf + l + 1, "COLS:", 5)) {	/* SBBSecho */
				l += 6;
				while(l<length && fbuf[l] <= ' ' && fbuf[l] >= 0) l++;
				uint8_t columns = atoi(fbuf + l);
				if(columns > 0)
					smb_hfield_bin(&msg, SMB_COLUMNS, columns);
			}

			else if(!strncmp(fbuf+l+1,"BBSID:",6)) {
				l+=7;
				while(l<length && fbuf[l]<=' ' && fbuf[l]>=0) l++;
				m=l;
				while(m<length && fbuf[m]!='\r') m++;
				if(m>l)
					smb_hfield(&msg,FIDOBBSID,(ushort)(m-l),fbuf+l);
			}

			else {		/* Unknown kludge line */
				while(l<length && fbuf[l]<=' ' && fbuf[l]>=0) l++;
				m=l;
				while(m<length && fbuf[m]!='\r') m++;
				while(m && fbuf[m-1]<=' ' && fbuf[m-1]>=0) m--;
				if(m>l)
					smb_hfield(&msg,FIDOCTRL,(ushort)(m-l),fbuf+l);
			}

			while(l<length && fbuf[l]!='\r') l++;
			continue;
		}
		if(ch == '\n')
			continue;
		if(cr && (!strncmp(fbuf+l,"--- ",4)
			|| !strncmp(fbuf+l,"---\r",4)
			|| !strncmp(fbuf+l,"-- \r",4)))
			done=1; 			/* tear line and down go into tail */
		else if(cr && !strncmp(fbuf+l," * Origin: ",11) && subnum != INVALID_SUB) {
			p=(char*)fbuf+l+11;
			while(*p && *p!='\r') p++;	 /* Find CR */
			while(p >= fbuf+l && *p!='(') p--;     /* rewind to '(' */
			if(*p == '(')
				origaddr=atofaddr(p+1); 	/* get orig address */
			done=1;
		}
		else if(done && cr && !strncmp(fbuf+l,"SEEN-BY:",8)) {
			l+=8;
			while(l<length && fbuf[l]<=' ' && fbuf[l]>=0) l++;
			m=l;
			while(m<length && fbuf[m]!='\r') m++;
			while(m && fbuf[m-1]<=' ' && fbuf[m-1]>=0) m--;
			if(m>l)
				smb_hfield(&msg,FIDOSEENBY,(ushort)(m-l),fbuf+l);
			while(l<length && fbuf[l]!='\r') l++;
			continue;
		}
		if(done) {
			if(taillen<MAX_TAILLEN)
				stail[taillen++]=ch;
		}
		else
			sbody[bodylen++]=ch;
		if(ch=='\r') {
			cr=1;
			if(done) {
				if(taillen<MAX_TAILLEN)
					stail[taillen++]='\n';
			}
			else
				sbody[bodylen++]='\n';
		}
		else
			cr=0;
	}

	if(bodylen>=2 && sbody[bodylen-2]=='\r' && sbody[bodylen-1]=='\n')
		bodylen-=2; 						/* remove last CRLF if present */
	sbody[bodylen]=0;

	while(taillen && stail[taillen-1]<=' ')	/* trim all garbage off the tail */
		taillen--;
	stail[taillen]=0;

	if(cfg.auto_utf8 && msg.ftn_charset == NULL && !str_is_ascii(fbuf) && utf8_str_is_valid(fbuf))
		smb_hfield_str(&msg, FIDOCHARSET, FIDO_CHARSET_UTF8);

	if(smb_msg_is_utf8(&msg))
		msg.hdr.auxattr |= MSG_HFIELDS_UTF8;
	else {
		if(cfg.strip_soft_cr)
			strip_char((const char*)sbody, (char*)sbody, FIDO_SOFT_CR);
	}

	if(subnum==INVALID_SUB && !bodylen && !taillen && cfg.kill_empty_netmail) {
		lprintf(LOG_INFO,"Empty NetMail - Ignored ");
		smb_freemsgmem(&msg);
		free(sbody);
		return IMPORT_FILTERED_EMPTY;
	}

	if(!origaddr.zone && subnum==INVALID_SUB)
		net=NET_NONE;						/* Message from SBBSecho */
	else
		net=NET_FIDO;						/* Record origin address */

	if(net) {
		if(origaddr.zone==0)
			origaddr.zone = sys_faddr.zone;
		smb_hfield(&msg,SENDERNETTYPE,sizeof(ushort),&net);
		smb_hfield(&msg,SENDERNETADDR,sizeof(fidoaddr_t),&origaddr);
	}

	if(subnum==INVALID_SUB) {
		smbfile=email;
		if(net && msg.to_net.type == NET_NONE) {
			smb_hfield(&msg,RECIPIENTNETTYPE,sizeof(ushort),&net);
			smb_hfield(&msg,RECIPIENTNETADDR,sizeof(fidoaddr_t),&destaddr);
		}
		smbfile->status.attr = SMB_EMAIL;
		smbfile->status.max_age = scfg.mail_maxage;
		smbfile->status.max_crcs = scfg.mail_maxcrcs;
	} else {
		smbfile=&smb[cur_smb];
		smbfile->status.max_age	 = scfg.sub[subnum]->maxage;
		smbfile->status.max_crcs = scfg.sub[subnum]->maxcrcs;
		smbfile->status.max_msgs = scfg.sub[subnum]->maxmsgs;
		if(scfg.sub[subnum]->misc&SUB_LZH)
			xlat=XLAT_LZH;

		msg.idx.time=msg.hdr.when_imported.time;	/* needed for MSG-ID generation */
		msg.idx.number=smbfile->status.last_msg+1;		/* needed for MSG-ID generation */

		/* Generate default (RFC822) message-id (always) */
		get_msgid(&scfg,subnum,&msg,msg_id,sizeof(msg_id));
		smb_hfield_str(&msg,RFC822MSGID,msg_id);
	}
	if(smbfile->status.max_crcs==0 || (subnum == INVALID_SUB && usernumber == 0))
		dupechk_hashes&=~(1<<SMB_HASH_SOURCE_BODY);
	/* Bad echo area collects a *lot* of messages, and thus, hashes - so no dupe checking */
	if(cfg.badecho>=0 && subnum==cfg.area[cfg.badecho].sub)
		dupechk_hashes=SMB_HASH_SOURCE_NONE;

	i=smb_addmsg(smbfile, &msg, smb_storage_mode(&scfg, smbfile), dupechk_hashes, xlat, sbody, stail);
	if(i == SMB_SUCCESS) {
		if(subnum != INVALID_SUB && scfg.sub[subnum]->post_sem[0])
			ftouch(cmdstr(&scfg, /* user: */NULL, scfg.sub[subnum]->post_sem, "", "", cmd, sizeof(cmd)));
	} else {
		lprintf(LOG_ERR,"ERROR %d (%s) line %d adding message to %s"
			,i, smbfile->last_error, __LINE__, subnum==INVALID_SUB ? "mail":scfg.sub[subnum]->code);
	}
	smb_freemsgmem(&msg);

	free(sbody);
	return(i);
}

/***********************************************************************/
/* Get zone and point from kludge lines from the stream  if they exist */
/* Returns true if an INTL klude line was found/parsed (with zone)     */
/***********************************************************************/
bool getzpt(FILE* stream, fmsghdr_t* hdr)
{
	char buf[0x1000];
	int i,len,cr=0;
	long pos;
	fidoaddr_t faddr;
	bool intl_found = false;

	pos=ftell(stream);
	len=fread(buf,1,0x1000,stream);
	for(i=0;i<len;i++) {
		if(buf[i]=='\n')	/* ignore line-feeds */
			continue;
		if((!i || cr) && buf[i]==CTRL_A) {	/* kludge */
			if(!strncmp(buf+i+1,"TOPT ",5))
				hdr->destpoint=atoi(buf+i+6);
			else if(!strncmp(buf+i+1,"FMPT ",5))
				hdr->origpoint=atoi(buf+i+6);
			else if(!strncmp(buf+i+1,"INTL ",5)) {
				faddr=atofaddr(buf+i+6);
				hdr->destzone=faddr.zone;
				hdr->destnet=faddr.net;
				hdr->destnode=faddr.node;
				i+=6;
				while(buf[i] && buf[i]!=' ') i++;
				faddr=atofaddr(buf+i+1);
				hdr->origzone=faddr.zone;
				hdr->orignet=faddr.net;
				hdr->orignode=faddr.node;
				intl_found = true;
			}
			while(i<len && buf[i]!='\r') i++;
			cr=1;
			continue;
		}
		if(buf[i]=='\r')
			cr=1;
		else
			cr=0;
	}
	(void)fseek(stream,pos,SEEK_SET);
	return intl_found;
}

bool foreign_zone(uint16_t zone1, uint16_t zone2)
{
	if(cfg.zone_blind && zone1 <= cfg.zone_blind_threshold && zone2 <= cfg.zone_blind_threshold)
		return false;
	return zone1!=zone2;
}

/******************************************************************************
 This function puts a message into a Fido packet, writing both the header
 information and the message body
******************************************************************************/
void putfmsg(FILE* stream, const char* fbuf, fmsghdr_t* hdr, area_t area
	,addrlist_t seenbys, addrlist_t paths)
{
	char str[256],seenby[256];
	int lastlen=0,net_exists=0;
	unsigned u,j;
	fidoaddr_t addr,sysaddr,lasthop={0,0,0,0};
	fidoaddr_t dest = { hdr->destzone, hdr->destnet, hdr->destnode, hdr->destpoint };
	fpkdmsg_t pkdmsg;
	size_t len;

	addr=getsysfaddr(dest);

	/* Write fixed-length header fields */
	memset(&pkdmsg,0,sizeof(pkdmsg));
	pkdmsg.type		= 2;
	if(area.tag == NULL)	{ /* NetMail, so use original origin address */
		pkdmsg.orignet	= hdr->orignet;
		pkdmsg.orignode = hdr->orignode;
	} else {
		pkdmsg.orignet	= addr.net;
		pkdmsg.orignode	= addr.node;
	}
	pkdmsg.destnet	= hdr->destnet;
	pkdmsg.destnode	= hdr->destnode;
	pkdmsg.attr		= hdr->attr;
	pkdmsg.cost		= hdr->cost;
	SAFECOPY(pkdmsg.time,hdr->time);
	(void)fwrite(&pkdmsg		,sizeof(pkdmsg)			,1,stream);

	/* Write variable-length (ASCIIZ) header fields */
	(void)fwrite(hdr->to	,strlen(hdr->to)+1	,1,stream);
	(void)fwrite(hdr->from	,strlen(hdr->from)+1	,1,stream);
	(void)fwrite(hdr->subj	,strlen(hdr->subj)+1	,1,stream);

	if(area.tag == NULL && strstr(fbuf, "\1INTL ") == NULL)	/* NetMail, so add FSC-0004 INTL kludge */
		fwrite_intl_control_line(stream, hdr);			/* If not already present */

	len = strlen(fbuf);

	/* Write message body */
	if(area.tag)
		if(strncmp(fbuf,"AREA:",5))                     /* No AREA: Line */
			fprintf(stream,"AREA:%s\r",area.tag);				/* So add one */
	(void)fwrite(fbuf,len,1,stream);
	lastlen=9;
	if(len && fbuf[len-1]!='\r')
		fputc('\r',stream);

	if(area.tag==NULL)	{ /* NetMail, so add FTS-4009 Via kludge line */
		fwrite_via_control_line(stream, &addr);
	} else { /* EchoMail, Not NetMail */

		if(foreign_zone(addr.zone, hdr->destzone))	/* Zone Gate */
			fprintf(stream,"SEEN-BY: %d/%d\r",hdr->destnet,hdr->destnode);
		else {
			fprintf(stream,"SEEN-BY:");
			for(u=0;u<seenbys.addrs;u++) {			  /* Put back original SEEN-BYs */
				strcpy(seenby," ");
				if(foreign_zone(addr.zone, seenbys.addr[u].zone))
					continue;
				if(seenbys.addr[u].net!=addr.net || !net_exists) {
					net_exists=1;
					addr.net=seenbys.addr[u].net;
					sprintf(str,"%d/",addr.net);
					SAFECAT(seenby,str);
				}
				sprintf(str,"%d",seenbys.addr[u].node);
				SAFECAT(seenby,str);
				if(lastlen+strlen(seenby)<80) {
					(void)fwrite(seenby,strlen(seenby),1,stream);
					lastlen+=strlen(seenby);
				}
				else {
					--u;
					lastlen=9; /* +strlen(seenby); */
					net_exists=0;
					fprintf(stream,"\rSEEN-BY:");
				}
			}

			for(u=0;u<area.links;u++) {			/* Add all links to SEEN-BYs */
				nodecfg_t* nodecfg=findnodecfg(&cfg, area.link[u],0);
				if(nodecfg!=NULL && nodecfg->passive)
					continue;
				strcpy(seenby," ");
				if(foreign_zone(addr.zone, area.link[u].zone) || area.link[u].point)
					continue;
				for(j=0;j<seenbys.addrs;j++)
					if(!memcmp(&area.link[u],&seenbys.addr[j],sizeof(fidoaddr_t)))
						break;
				if(j==seenbys.addrs) {
					if(area.link[u].net!=addr.net || !net_exists) {
						net_exists=1;
						addr.net=area.link[u].net;
						sprintf(str,"%d/",addr.net);
						SAFECAT(seenby,str);
					}
					sprintf(str,"%d",area.link[u].node);
					SAFECAT(seenby,str);
					if(lastlen+strlen(seenby)<80) {
						(void)fwrite(seenby,strlen(seenby),1,stream);
						lastlen+=strlen(seenby);
					}
					else {
						--u;
						lastlen=9; /* +strlen(seenby); */
						net_exists=0;
						fprintf(stream,"\rSEEN-BY:");
					}
				}
			}

			for(u=0;u<scfg.total_faddrs;u++) {				/* Add AKAs to SEEN-BYs */
				strcpy(seenby," ");
				if(foreign_zone(addr.zone, scfg.faddr[u].zone) || scfg.faddr[u].point)
					continue;
				for(j=0;j<seenbys.addrs;j++)
					if(!memcmp(&scfg.faddr[u],&seenbys.addr[j],sizeof(fidoaddr_t)))
						break;
				if(j==seenbys.addrs) {
					if(scfg.faddr[u].net!=addr.net || !net_exists) {
						net_exists=1;
						addr.net=scfg.faddr[u].net;
						sprintf(str,"%d/",addr.net);
						SAFECAT(seenby,str);
					}
					sprintf(str,"%d",scfg.faddr[u].node);
					SAFECAT(seenby,str);
					if(lastlen+strlen(seenby)<80) {
						(void)fwrite(seenby,strlen(seenby),1,stream);
						lastlen+=strlen(seenby);
					}
					else {
						--u;
						lastlen=9; /* +strlen(seenby); */
						net_exists=0;
						fprintf(stream,"\rSEEN-BY:");
					}
				}
			}

			lastlen=7;
			net_exists=0;
			fprintf(stream,"\r\1PATH:");
			addr=getsysfaddr(dest);
			for(u=0;u<paths.addrs;u++) {			  /* Put back the original PATH */
				if(paths.addr[u].net == 0)
					continue;	// Invalid node number/address, don't include "0/0" in PATH
				strcpy(seenby," ");
				if(foreign_zone(addr.zone, paths.addr[u].zone) || paths.addr[u].point)
					continue;
				lasthop=paths.addr[u];
				if(paths.addr[u].net!=addr.net || !net_exists) {
					net_exists=1;
					addr.net=paths.addr[u].net;
					sprintf(str,"%d/",addr.net);
					SAFECAT(seenby,str);
				}
				sprintf(str,"%d",paths.addr[u].node);
				SAFECAT(seenby,str);
				if(lastlen+strlen(seenby)<80) {
					(void)fwrite(seenby,strlen(seenby),1,stream);
					lastlen+=strlen(seenby);
				}
				else {
					--u;
					lastlen=7; /* +strlen(seenby); */
					net_exists=0;
					fprintf(stream,"\r\1PATH:");
				}
			}

			strcpy(seenby," ");         /* Add first address with same zone to PATH */
			sysaddr=getsysfaddr(dest);
			if(sysaddr.net!=0 && sysaddr.point==0
				&& (paths.addrs==0 || lasthop.net!=sysaddr.net || lasthop.node!=sysaddr.node)) {
				if(sysaddr.net!=addr.net || !net_exists) {
					net_exists=1;
					addr.net=sysaddr.net;
					sprintf(str,"%d/",addr.net);
					SAFECAT(seenby,str);
				}
				sprintf(str,"%d",sysaddr.node);
				SAFECAT(seenby,str);
				if(lastlen+strlen(seenby)<80)
					(void)fwrite(seenby,strlen(seenby),1,stream);
				else {
					fprintf(stream,"\r\1PATH:");
					(void)fwrite(seenby,strlen(seenby),1,stream);
				}
			}
			fputc('\r',stream);
		}
	}

	fputc(FIDO_PACKED_MSG_TERMINATOR, stream);
}

size_t terminate_packet(FILE* stream)
{
	uint16_t terminator = FIDO_PACKET_TERMINATOR;

	return fwrite(&terminator, sizeof(terminator), 1, stream);
}

long find_packet_terminator(FILE* stream)
{
	uint16_t	terminator = ~FIDO_PACKET_TERMINATOR;
	long		offset;

	if(fseek(stream, 0, SEEK_END) != 0)
		return 0;
	offset = ftell(stream);
	if(offset >= sizeof(fpkthdr_t)+sizeof(terminator)) {
		(void)fseek(stream, offset-sizeof(terminator), SEEK_SET);
		if(fread(&terminator, 1, sizeof(terminator), stream) == sizeof(terminator)
			&& terminator==FIDO_PACKET_TERMINATOR)
			offset -= sizeof(terminator);
	}
	return(offset);
}

/******************************************************************************
 This function creates a binary list of the message seen-bys and path from
 inbuf.
******************************************************************************/
void gen_psb(addrlist_t *seenbys, addrlist_t *paths, const char *inbuf, uint16_t zone)
{
	char str[128],seenby[256],*p,*p1,*p2;
	int i,j,len;
	fidoaddr_t addr;
	const char* fbuf;

	if(!inbuf)
		return;
	fbuf=strstr(inbuf, FIDO_ORIGIN_PREFIX_FORM_1);
	if(!fbuf)
		fbuf=strstr(inbuf, FIDO_ORIGIN_PREFIX_FORM_2);
	if(!fbuf)
		fbuf=inbuf;
	FREE_AND_NULL(seenbys->addr);
	addr.zone=addr.net=addr.node=addr.point=seenbys->addrs=0;
	p=strstr(fbuf,"\rSEEN-BY:");
	if(!p) p=strstr(fbuf,"\nSEEN-BY:");
	if(p) {
		while(1) {
			sprintf(str,"%-.100s",p+10);
			if((p1=strchr(str,'\r'))!=NULL)
				*p1=0;
			p1=str;
			i=j=0;
			len=strlen(str);
			while(i<len) {
				j=i;
				while(i<len && *(p1+i)!=' ')
					++i;
				if(j>len)
					break;
				sprintf(seenby,"%-.*s",(i-j),p1+j);
				if((p2=strchr(seenby,':'))!=NULL) {
					addr.zone=atoi(seenby);
					addr.net=atoi(p2+1);
				}
				else if((p2=strchr(seenby,'/'))!=NULL)
					addr.net=atoi(seenby);
				if((p2=strchr(seenby,'/'))!=NULL)
					addr.node=atoi(p2+1);
				else
					addr.node=atoi(seenby);
				if((p2=strchr(seenby,'.'))!=NULL)
					addr.point=atoi(p2+1);
				if(!addr.zone)
					addr.zone=zone; 		/* Was 1 */
				if((seenbys->addr=(fidoaddr_t *)realloc(seenbys->addr
					,sizeof(fidoaddr_t)*(seenbys->addrs+1)))==NULL) {
					lprintf(LOG_ERR,"ERROR line %d allocating memory for message "
						"seenbys.",__LINE__);
					bail(1);
					return;
				}
				memcpy(&seenbys->addr[seenbys->addrs],&addr,sizeof(fidoaddr_t));
				seenbys->addrs++;
				++i;
			}
			p1=strstr(p+10,"\rSEEN-BY:");
			if(!p1)
				p1=strstr(p+10,"\nSEEN-BY:");
			if(!p1)
				break;
			p=p1;
		}
	}
	else {
		if((seenbys->addr=(fidoaddr_t *)realloc(seenbys->addr
			,sizeof(fidoaddr_t)))==NULL) {
			lprintf(LOG_ERR,"ERROR line %d allocating memory for message seenbys."
				,__LINE__);
			bail(1);
			return;
		}
		memset(&seenbys->addr[0],0,sizeof(fidoaddr_t));
	}

	FREE_AND_NULL(paths->addr);
	addr.zone=addr.net=addr.node=addr.point=paths->addrs=0;
	if((p=strstr(fbuf,"\1PATH:"))!=NULL) {
		while(1) {
			sprintf(str,"%-.100s",p+7);
			if((p1=strchr(str,'\r'))!=NULL)
				*p1=0;
			p1=str;
			i=j=0;
			len=strlen(str);
			while(i<len) {
				j=i;
				while(i<len && *(p1+i)!=' ')
					++i;
				if(j>len)
					break;
				sprintf(seenby,"%-.*s",(i-j),p1+j);
				if((p2=strchr(seenby,':'))!=NULL) {
					addr.zone=atoi(seenby);
					addr.net=atoi(p2+1);
				}
				else if((p2=strchr(seenby,'/'))!=NULL)
					addr.net=atoi(seenby);
				if((p2=strchr(seenby,'/'))!=NULL)
					addr.node=atoi(p2+1);
				else
					addr.node=atoi(seenby);
				if((p2=strchr(seenby,'.'))!=NULL)
					addr.point=atoi(p2+1);
				if(!addr.zone)
					addr.zone=zone; 		/* Was 1 */
				if((paths->addr=(fidoaddr_t *)realloc(paths->addr
					,sizeof(fidoaddr_t)*(paths->addrs+1)))==NULL) {
					lprintf(LOG_ERR,"ERROR line %d allocating memory for message "
						"paths.",__LINE__);
					bail(1);
					return;
				}
				memcpy(&paths->addr[paths->addrs],&addr,sizeof(fidoaddr_t));
				paths->addrs++;
				++i;
			}
			if((p1=strstr(p+7,"\1PATH:"))==NULL)
				break;
			p=p1;
		}
	}
	else {
		if((paths->addr=(fidoaddr_t *)realloc(paths->addr
			,sizeof(fidoaddr_t)))==NULL) {
			lprintf(LOG_ERR,"ERROR line %d allocating memory for message paths."
				,__LINE__);
			bail(1);
			return;
		}
		memset(&paths->addr[0],0,sizeof(fidoaddr_t));
	}
}

/******************************************************************************
 This function takes the addrs passed to it and compares them to the address
 passed in compaddr.	true is returned if inaddr matches any of the addrs
 otherwise false is returned.
******************************************************************************/
bool check_psb(const addrlist_t* addrlist, fidoaddr_t compaddr)
{
	unsigned u;

	for(u=0;u<addrlist->addrs;u++) {
		if(foreign_zone(compaddr.zone, addrlist->addr[u].zone))
			continue;
		if(compaddr.net != addrlist->addr[u].net)
			continue;
		if(compaddr.node != addrlist->addr[u].node)
			continue;
		if(compaddr.point != addrlist->addr[u].point)
			continue;
		return(true); /* match found */
	}
	return(false);	/* match not found */
}

/******************************************************************************
 This function strips the message seen-bys and path from inbuf.
******************************************************************************/
void strip_psb(char *inbuf)
{
	char *p,*fbuf;

	if(!inbuf)
		return;
	fbuf=strstr(inbuf, FIDO_ORIGIN_PREFIX_FORM_1);
	if(!fbuf)
		fbuf=strstr(inbuf, FIDO_ORIGIN_PREFIX_FORM_2);
	if(!fbuf)
		fbuf=inbuf;
	if((p=strstr(fbuf,"\rSEEN-BY:"))!=NULL)
		*(p)=0;
	if((p=strstr(fbuf,"\r\1PATH:"))!=NULL)
		*(p)=0;
}

typedef struct {
	FILE*		fp;				/* The stream associated with this packet (NULL if finalized already) */
	fidoaddr_t	orig;			/* The source address of this packet */
	fidoaddr_t	dest;			/* The current link for this packet */
	char*		filename;		/* Name of the packet file */
} outpkt_t;

link_list_t outpkt_list;

void finalize_outpkt(outpkt_t* pkt)
{
	char str[128];

	lprintf(LOG_DEBUG, "Finalizing outbound packet from %s to %s: %s"
		,smb_faddrtoa(&pkt->orig, str), smb_faddrtoa(&pkt->dest, NULL), pkt->filename);
	terminate_packet(pkt->fp);
	fclose(pkt->fp);
	pkt->fp = NULL;
}

void move_echomail_packets(void)
{
	outpkt_t* pkt;

	while((pkt = listPopNode(&outpkt_list)) != NULL) {
		printf("%21s: %s ","Outbound Packet", pkt->filename);

		if(pkt->fp != NULL)
			finalize_outpkt(pkt);

		if(pack_bundle(pkt->filename, pkt->orig, pkt->dest))
			packets_sent++;

		free(pkt->filename);
		free(pkt);
	}
}

outpkt_t* get_outpkt(fidoaddr_t orig, fidoaddr_t dest, nodecfg_t* nodecfg)
{
	char str[128];
	outpkt_t* pkt = NULL;
	list_node_t* node;

	for(node=listFirstNode(&outpkt_list); node!=NULL; node=listNextNode(node)) {
		pkt = node->data;
		if(memcmp(&pkt->orig, &orig, sizeof(orig)) != 0)
			continue;
		if(memcmp(&pkt->dest, &dest, sizeof(dest)) != 0)
			continue;
		if(pkt->fp == NULL)
			continue;
		if(ftell(pkt->fp) >= (long)cfg.maxpktsize) {
			finalize_outpkt(pkt);
			continue;
		}
		lprintf(LOG_DEBUG, "Appending outbound packet from %s to %s: %s"
			,smb_faddrtoa(&orig, str), smb_faddrtoa(&dest, NULL), pkt->filename);
		return pkt;
	}

	if((pkt = calloc(1, sizeof(outpkt_t))) == NULL) {
		lprintf(LOG_CRIT, "Memory allocation error, line %u", __LINE__);
		return NULL;
	}
	if((pkt->filename = strdup(pktname(cfg.temp_dir))) == NULL) {
		lprintf(LOG_CRIT, "Memory allocation error, line %u", __LINE__);
		free(pkt);
		return NULL;
	}
	if((pkt->fp = fopen(pkt->filename, "wb")) == NULL) {
		lprintf(LOG_ERR, "ERROR %u (%s) opening/creating %s", errno, strerror(errno), pkt->filename);
		free(pkt->filename);
		free(pkt);
		return NULL;
	}

	pkt->orig = orig;
	pkt->dest = dest;

	lprintf(LOG_DEBUG, "Creating outbound packet from %s to %s: %s"
		,smb_faddrtoa(&orig, str), smb_faddrtoa(&dest, NULL), pkt->filename);
	fpkthdr_t pkthdr;
	new_pkthdr(&pkthdr, orig, dest, nodecfg);
	(void)fwrite(&pkthdr, sizeof(pkthdr), 1, pkt->fp);
	listAddNode(&outpkt_list, pkt, 0, LAST_NODE);
	return pkt;
}

/******************************************************************************
 This is where we put outgoing messages into packets.
******************************************************************************/
bool write_to_pkts(const char *fbuf, area_t area, const fidoaddr_t* faddr, const fidoaddr_t* pkt_orig
	,fmsghdr_t* hdr, addrlist_t seenbys, addrlist_t paths, bool rescan)
{
	unsigned u;
	fidoaddr_t sysaddr;
	unsigned pkts_written = 0;
	char* rescanned_from = NULL;
	char exceptions[128];
	unsigned msg_seen = 0;
	unsigned msg_path = 0;
	unsigned msg_origin = 0;
	unsigned pkt_origin = 0;
	unsigned passive_node = 0;

	if(!rescan)
		rescanned_from = parse_control_line(fbuf, "RESCANNED ");

	for(u=0; u<area.links; u++) {
		if(faddr != NULL && memcmp(faddr,&area.link[u], sizeof(fidoaddr_t)) != 0)
			continue;
		if(check_psb(&seenbys, area.link[u])) {
			msg_seen++;
			continue;
		}
		if(check_psb(&paths, area.link[u])) {
			msg_path++;
			continue;
		}
		if(hdr->origzone == area.link[u].zone
			&& hdr->orignet == area.link[u].net
			&& hdr->orignode == area.link[u].node
			&& hdr->origpoint == area.link[u].point) {
			msg_origin++;
			continue;	// Don't loop messages back to message originator
		}
		if(pkt_orig != NULL	&& memcmp(pkt_orig, &area.link[u], sizeof(*pkt_orig)) == 0) {
			pkt_origin++;
			continue;	// Don't loop message back to packet originator
		}
		nodecfg_t* nodecfg = findnodecfg(&cfg, area.link[u],0);
		if(nodecfg != NULL && nodecfg->passive) {
			passive_node++;
			continue;
		}
		if(rescanned_from != NULL) {
			lprintf(LOG_DEBUG, "NOT EXPORTING (to %s) previously-rescanned message from: %s"
				,smb_faddrtoa(&area.link[u], NULL), rescanned_from);
			continue;
		}
		sysaddr = getsysfaddr(area.link[u]);
		printf("%s ",smb_faddrtoa(&area.link[u],NULL));
		outpkt_t* pkt = get_outpkt(sysaddr, area.link[u], nodecfg);
		if(pkt == NULL) {
			lprintf(LOG_ERR, "ERROR Creating/opening outbound packet for %s", smb_faddrtoa(&area.link[u], NULL));
			bail(1);
		}
		hdr->destnode	= area.link[u].node;
		hdr->destnet	= area.link[u].net;
		hdr->destzone	= area.link[u].zone;
		lprintf(LOG_DEBUG, "Adding %s message from %s (%s) to packet for %s: %s"
			,area.tag, hdr->from, fmsghdr_srcaddr_str(hdr), smb_faddrtoa(&area.link[u], NULL), pkt->filename);
		putfmsg(pkt->fp, fbuf, hdr, area, seenbys, paths);
		pkts_written++;
	}
	free(rescanned_from);
	str_list_t details = strListInit();
	if(msg_seen)
		strListAppendFormat(&details, "%u seen", msg_seen);
	if(msg_path)
		strListAppendFormat(&details, "%u path", msg_path);
	if(msg_origin)
		strListAppendFormat(&details, "%u origin", msg_origin);
	if(pkt_origin)
		strListAppendFormat(&details, "%u pkt-origin", pkt_origin);
	if(passive_node)
		strListAppendFormat(&details, "%u passive", passive_node);
	strListJoin(details, exceptions, sizeof(exceptions), ", ");
	if(*exceptions == '\0')
		SAFECOPY(exceptions, "none");
	lprintf(LOG_DEBUG, "Added %s message from %s (%s) to packets for %u links (exceptions: %s)"
		,area.tag
		,hdr->from
		,fmsghdr_srcaddr_str(hdr)
		,pkts_written
		,exceptions);

	return pkts_written > 0;
}

int pkt_to_msg(FILE* fidomsg, fmsghdr_t* hdr, const char* info, const char* inbound)
{
	char path[MAX_PATH+1];
	char* fmsgbuf;
	int i,file;
	ulong l;

	if((fmsgbuf=getfmsg(fidomsg,&l))==NULL) {
		lprintf(LOG_ERR,"ERROR line %d netmail allocation",__LINE__);
		return(-1);
	}

	if(!l && cfg.kill_empty_netmail)
		printf("Empty NetMail");
	else {
		printf("Exporting: ");
		if(!isdir(scfg.netmail_dir) && mkpath(scfg.netmail_dir) != 0) {
			lprintf(LOG_ERR, "Error %u (%s) line %d creating directory: %s"
				,errno, strerror(errno), __LINE__, scfg.netmail_dir);
			free(fmsgbuf);
			return -2;
		}
		for(i=1;i;i++) {
			SAFEPRINTF2(path, "%s%u.msg", scfg.netmail_dir, i);
			if(!fexistcase(path))
				break;
			if(terminated) {
				free(fmsgbuf);
				return 1;
			}
		}
		if(!i) {
			lprintf(LOG_WARNING,"Too many netmail messages");
			free(fmsgbuf);
			return(-1);
		}
		if((file=nopen(path,O_WRONLY|O_CREAT))==-1) {
			lprintf(LOG_ERR,"ERROR %u (%s) line %d creating %s",errno,strerror(errno),__LINE__,path);
			free(fmsgbuf);
			return(-1);
		}
		if(hdr->attr&FIDO_FILE) {	/* File attachment (only a single file supported) */
			char fname[FIDO_SUBJ_LEN];
			SAFECOPY(fname, getfname(hdr->subj));
			SAFEPRINTF2(hdr->subj, "%s%s", inbound, fname);	/* Fix the file path in the subject */
		}
		const uint16_t remove_attrs = FIDO_CRASH | FIDO_LOCAL | FIDO_HOLD;
		if(hdr->attr&remove_attrs) {
			lprintf(LOG_DEBUG, "%s Removing attributes: %04hX", info, (uint16_t)(hdr->attr&remove_attrs));
			hdr->attr &= ~remove_attrs;
		}
		(void)write(file,hdr,sizeof(fmsghdr_t));
		(void)write(file,fmsgbuf,l+1); /* Write the '\0' terminator too */
		close(file);
		printf("%s", path);
		lprintf(LOG_INFO,"%s Exported to %s",info,path);
	}
	free(fmsgbuf);

	return(0);
}


/**************************************/
/* Send netmail, returns 0 on success */
/**************************************/
int import_netmail(const char* path, const fmsghdr_t* inhdr, FILE* fp, const char* inbound)
{
	char info[512],str[256],tmp[256],subj[256]
		,*fmsgbuf=NULL,*p,*tp,*sp;
	int i,match,usernumber = 0;
	ulong length;
	fidoaddr_t addr;
	fmsghdr_t hdr = *inhdr;

	hdr.destzone=sys_faddr.zone;
	hdr.destpoint=hdr.origpoint=0;
	bool got_zones = getzpt(fp, &hdr);				/* use kludge if found */
	for(match=0;match<scfg.total_faddrs;match++)
		if((hdr.destzone==scfg.faddr[match].zone || (cfg.fuzzy_zone && !got_zones))
			&& hdr.destnet==scfg.faddr[match].net
			&& hdr.destnode==scfg.faddr[match].node
			&& hdr.destpoint==scfg.faddr[match].point)
			break;
	if(match<scfg.total_faddrs && (cfg.fuzzy_zone && !got_zones))
		hdr.origzone=hdr.destzone=scfg.faddr[match].zone;
	if(hdr.origpoint)
		SAFEPRINTF(tmp,".%hu",hdr.origpoint);
	else
		tmp[0]=0;
	if(hdr.destpoint)
		SAFEPRINTF(str,".%hu",hdr.destpoint);
	else
		str[0]=0;
	safe_snprintf(info, sizeof(info), "%s%s%s (%hu:%hu/%hu%s) To: %s (%hu:%hu/%hu%s)"
		,path,path[0] ? " ":""
		,hdr.from,hdr.origzone,hdr.orignet,hdr.orignode,tmp
		,hdr.to,hdr.destzone,hdr.destnet,hdr.destnode,str);
	printf("%s ",info);

	if(!opt_import_netmail) {
		printf("Ignored");
		if(!path[0]) {
			printf(" - ");
			pkt_to_msg(fp,&hdr,info,inbound);
		} else
			lprintf(LOG_INFO,"%s Ignored",info);

		return(-1);
	}

	if(!sysfaddr_is_valid(match) && !cfg.ignore_netmail_dest_addr) {
		printf("Foreign address");
		if(!path[0]) {
			printf(" - ");
			pkt_to_msg(fp,&hdr,info,inbound);
		}
		return(2);
	}

	if(path[0]) {	/* .msg file, not .pkt */
		if(hdr.attr&FIDO_ORPHAN) {
			printf("Orphaned");
			return(1);
		}
		if((hdr.attr&FIDO_RECV) && !cfg.ignore_netmail_recv_attr) {
			printf("Already received");
			return(3);
		}
		if((hdr.attr&FIDO_LOCAL) && !cfg.ignore_netmail_local_attr) {
			printf("Created locally");
			return(4);
		}
		if(hdr.attr&FIDO_INTRANS) {
			printf("In-transit");
			return(5);
		}
	}

	if(email->shd_fp==NULL) {
		SAFEPRINTF(email->file,"%smail",scfg.data_dir);
		email->retry_time=scfg.smb_retry_time;
		if((i=smb_open(email))!=SMB_SUCCESS) {
			lprintf(LOG_ERR,"ERROR %d (%s) line %d opening %s",i,email->last_error,__LINE__,email->file);
			bail(1);
			return -1;
		}
	}

	if(!filelength(fileno(email->shd_fp))) {
		email->status.max_crcs=scfg.mail_maxcrcs;
		email->status.max_msgs=0;
		email->status.max_age=scfg.mail_maxage;
		email->status.attr=SMB_EMAIL;
		if((i=smb_create(email))!=SMB_SUCCESS) {
			lprintf(LOG_ERR,"ERROR %d (%s) line %d creating %s", i, email->last_error, __LINE__, email->file);
			bail(1);
			return -1;
		}
	}

	struct robot* robot = NULL;
	for(unsigned u = 0; u < cfg.robot_count; u++) {
		if(stricmp(hdr.to, cfg.robot_list[u].name) == 0) {
			robot = &cfg.robot_list[u];
			hdr.attr |= robot->attr;
			lprintf(LOG_DEBUG, "%s NetMail received for robot: %s", info, robot->name);
			break;
		}
	}

	if(robot == NULL) {
		if(stricmp(hdr.to, FIDO_AREAMGR_NAME) == 0
			|| stricmp(hdr.to, "SBBSecho") == 0
			|| stricmp(hdr.to, FIDO_PING_NAME) == 0) {
			fmsgbuf=getfmsg(fp,NULL);
			if(path[0]) {
				if(cfg.delete_netmail && opt_delete_netmail) {
					fclose(fp);
					delfile(path, __LINE__);
				}
				else {
					hdr.attr|=FIDO_RECV;
					(void)fseek(fp,0L,SEEK_SET);
					(void)fwrite(&hdr,sizeof(fmsghdr_t),1,fp);
					fclose(fp); /* Gotta close it here for areafix stuff */
				}
			}
			addr.zone=hdr.origzone;
			addr.net=hdr.orignet;
			addr.node=hdr.orignode;
			addr.point=hdr.origpoint;
			fidoaddr_t dest = {
				.zone = hdr.destzone,
				.net = hdr.destnet,
				.node = hdr.destnode,
				.point = hdr.destpoint
			};
			lprintf(LOG_INFO, "%s", info);
			if(stricmp(hdr.from, hdr.to) == 0)
				lprintf(LOG_NOTICE, "Refusing to auto-reply to NetMail from %s", hdr.from);
			else {
				if(stricmp(hdr.to, FIDO_PING_NAME) == 0) {

					lprintf(LOG_INFO, "PING (for %s) Request received from %s", faddrtoa(&addr), hdr.from);

					char subj[FIDO_SUBJ_LEN];

					REPLACE_CHARS(fmsgbuf, '\1', '@', tp);

					int bodylen = 0;
					char* body = malloc(strlen(fmsgbuf) + 4000);
					if(body == NULL)
						body = fmsgbuf;
					else {
						bodylen += sprintf(body, "Your PING request was received at: %s %s\r"
							,timestr(&scfg, time32(NULL), tmp), smb_zonestr(sys_timezone(&scfg),NULL));
						bodylen += sprintf(body+bodylen, "by: %s (sysop: %s) @ %s\r"
							,scfg.sys_name, scfg.sys_op, smb_faddrtoa(&scfg.faddr[match], NULL));
						time_t t = (time_t)fmsgtime(hdr.time);
						bodylen += sprintf(body+bodylen, "\rThe received message header contained:\r\r"
							"Subj: %s\r"
							"Attr: %04hX\r"
							"To  : %s (%s)\r"
							"From: %s (%s)\r"
							"Date: %s (parsed: 0x%08lX, %.24s)\r"
							,hdr.subj
							,hdr.attr
							,hdr.to, smb_faddrtoa(&scfg.faddr[match], NULL)
							,hdr.from, faddrtoa(&addr)
							,hdr.time, (ulong)t, ctime(&t));
						bodylen += sprintf(body+bodylen,"\r======[ Received message text ]======\r%s"
														"\r======[ End received msg text ]======\r", fmsgbuf);
					}
					SAFEPRINTF(subj, "Pong: %s", hdr.subj);
					create_netmail(/* to: */hdr.from, /* msg: */NULL, subj, body, /* dest: */addr, /* src: */&dest);
					if(body != fmsgbuf) {
						FREE_AND_NULL(body);
					}
				} else {	/* AreaFix */
					p=process_areafix(addr,fmsgbuf,/* Password: */hdr.subj, /* To: */hdr.from);
					if(p != NULL && cfg.areamgr[0] != 0) {
						uint notify = matchuser(&scfg, cfg.areamgr, TRUE);
						if(notify) {
							SAFEPRINTF(hdr.subj, "FAILED Area Management Request from %s", faddrtoa(&addr));
							SAFECOPY(hdr.to, cfg.areamgr);
							SAFECOPY(hdr.from, "SBBSecho");
							hdr.origzone=hdr.orignet=hdr.orignode=hdr.origpoint=0;
							hdr.time[0] = 0;	/* Generate a new timestamp */
							if(fmsgtosmsg(p, &hdr, notify, INVALID_SUB) == IMPORT_SUCCESS) {
								SAFECOPY(str,"\7\1n\1hSBBSecho \1n\1msent you mail\r\n");
								putsmsg(&scfg,notify,str);
							}
						} else
							lprintf(LOG_ERR, "Configured Area Manager, user not found: %s", cfg.areamgr);
					}
				}
			}
			FREE_AND_NULL(fmsgbuf);
			return(-2);
		}

		usernumber=atoi(hdr.to);
		if(!usernumber && strListFind(cfg.sysop_alias_list, hdr.to, /* case sensitive: */false) >= 0)
			usernumber = 1;
		if(!usernumber)
			usernumber = lookup_user(&scfg, &user_list, hdr.to);
		if(!usernumber && cfg.default_recipient[0])
			usernumber = matchuser(&scfg, cfg.default_recipient, TRUE);
		if(!usernumber) {
			lprintf(LOG_WARNING,"%s Unknown user",info);

			if(!path[0]) {
				printf(" - ");
				pkt_to_msg(fp,&hdr,info,inbound);
			}
			return(2);
		}
	}

	/*********************/
	/* Importing NetMail */
	/*********************/

	fmsgbuf=getfmsg(fp,&length);

	switch(i=fmsgtosmsg(fmsgbuf, &hdr, usernumber, INVALID_SUB)) {
		case IMPORT_SUCCESS:			/* success */
			break;
		case IMPORT_FILTERED_DUPE:		/* filtered */
			lprintf(LOG_WARNING,"%s Filtered - Ignored",info);
			break;
		case IMPORT_FILTERED_EMPTY:		/* empty */
			lprintf(LOG_WARNING,"%s Empty - Ignored",info);
			break;
		case IMPORT_FILTERED_AGE:		/* too-old */
			lprintf(LOG_WARNING,"%s Filtered - Too Old", info);
			break;
		default:
			lprintf(LOG_ERR,"ERROR (%d) Importing %s",i,info);
			break;
	}
	if(i) {
		FREE_AND_NULL(fmsgbuf);
		return(0);
	}

	if(usernumber) {
		addr.zone=hdr.origzone;
		addr.net=hdr.orignet;
		addr.node=hdr.orignode;
		addr.point=hdr.origpoint;
		safe_snprintf(str, sizeof(str), "\7\1n\1hSBBSecho: \1m%.*s \1n\1msent you NetMail%s from \1h%s\1n\r\n"
			,FIDO_NAME_LEN-1
			,hdr.from
			,hdr.attr&FIDO_FILE ? " with attachment" : ""
			,smb_faddrtoa(&addr,NULL));
		putsmsg(&scfg,usernumber,str);
	}
	if(hdr.attr&FIDO_FILE) {	/* File attachment */
		SAFECOPY(subj,hdr.subj);
		tp=subj;
		while(1) {
			p=strchr(tp,' ');
			if(p) *p=0;
			sp=strrchr(tp,'/');              /* sp is slash pointer */
			if(!sp) sp=strrchr(tp,'\\');
			if(sp) tp=sp+1;
			lprintf(LOG_INFO, "Processing attached file: %s", tp);
			SAFEPRINTF2(str,"%s%s", inbound, tp);
			if(!fexistcase(str)) {
				lprintf(LOG_WARNING, "Attached file not found in expected inbound: %s", str);
				if(inbound == cfg.inbound)
					inbound = cfg.secure_inbound;
				else
					inbound = cfg.inbound;
				SAFEPRINTF2(str,"%s%s", inbound, tp);
			}
			SAFEPRINTF2(tmp,"%sfile/%04u.in",scfg.data_dir,usernumber);
			(void)mkpath(tmp);
			backslash(tmp);
			SAFECAT(tmp,tp);
			lprintf(LOG_DEBUG, "Moving attachment from %s to %s", str, tmp);
			if((i = mv(str,tmp,0)) != 0)
				lprintf(LOG_ERR, "ERROR %d moving attached file from %s to %s for NetMail %s", i, str, tmp, info);
			if(!p)
				break;
			tp=p+1;
		}
	}
	netmail++;
	if(robot != NULL)
		robot->recv_count++;

	FREE_AND_NULL(fmsgbuf);

	/***************************/
	/* Updating message header */
	/***************************/
	/***	NOT packet compatible
	if(!(misc&DONT_SET_RECV)) {
		hdr.attr|=FIDO_RECV;
		fseek(fp,0L,SEEK_SET);
		fwrite(&hdr,sizeof(fmsghdr_t),1,fp); }
	***/
	lprintf(LOG_INFO,"%s Imported",info);
	return(0);
}

static uint32_t read_export_ptr(int subnum, const char* tag)
{
	char		path[MAX_PATH+1];
	char		key[INI_MAX_VALUE_LEN];
	FILE*		fp;
	uint32_t	ptr=0;

	safe_snprintf(path,sizeof(path),"%s%s.ini",scfg.sub[subnum]->data_dir,scfg.sub[subnum]->code);
	if((fp=iniOpenFile(path, /* create: */false)) != NULL) {
		safe_snprintf(key, sizeof(key), "%s.export_ptr", tag);
		if((ptr=iniReadLongInt(fp, "SBBSecho", key, 0)) == 0)
			ptr=iniReadLongInt(fp, "SBBSecho", "export_ptr", 0);	/* the previous .ini method (did not support gating) */
		iniCloseFile(fp);
	}
	return ptr;
}

static void write_export_ptr(int subnum, uint32_t ptr, const char* tag)
{
	char		path[MAX_PATH+1];
	char		key[INI_MAX_VALUE_LEN];
	FILE*		fp;
	str_list_t	ini_file;

	/* New way (July-21-2012): */
	safe_snprintf(path,sizeof(path),"%s%s.ini",scfg.sub[subnum]->data_dir,scfg.sub[subnum]->code);
	if((fp=iniOpenFile(path, /* create: */true)) != NULL) {
		safe_snprintf(key, sizeof(key), "%s.export_ptr", tag);
		ini_file = iniReadFile(fp);
		iniSetLongInt(&ini_file, "SBBSecho", key, ptr, /* style (default): */NULL);
		iniSetLongInt(&ini_file, "SBBSecho", "export_ptr", ptr, /* style (default): */NULL);
		iniWriteFile(fp, ini_file);
		iniCloseFile(fp);
		strListFree(&ini_file);
	}
}

/******************************************************************************
 This is where we export echomail.	This was separated from function main so
 it could be used for the remote rescan function.  Passing anything but an
 empty address as 'addr' designates that a rescan should be done for that
 address.
******************************************************************************/
void export_echomail(const char* sub_code, const nodecfg_t* nodecfg, bool rescan)
{
	char	str[256],tear,cr;
	char*	buf=NULL;
	char*	minus;
	char*	fmsgbuf=NULL;
	ulong	fmsgbuflen;
	int		tzone;
	unsigned area;
	ulong	f,l,m,exp,exported=0;
	uint32_t ptr,lastmsg,posts;
	long	msgs;
	smbmsg_t msg;
	smbmsg_t orig_msg;
	fmsghdr_t hdr;
	struct	tm *tm;
	post_t *post;
	addrlist_t msg_seen,msg_path;
	time_t	tt;
	time_t now = time(NULL);

	memset(&msg_seen,0,sizeof(addrlist_t));
	memset(&msg_path,0,sizeof(addrlist_t));
	memset(&hdr,0,sizeof(hdr));

	printf("\nScanning for Outbound EchoMail...");

	for(area=0; area<cfg.areas && !terminated; area++) {
		const char* tag=cfg.area[area].tag;
		if(area==cfg.badecho)		/* Don't scan the bad-echo area */
			continue;
		if(!cfg.area[area].links)
			continue;
		int subnum = cfg.area[area].sub;
		if(subnum < 0 || subnum >= scfg.total_subs)	/* Don't scan pass-through areas */
			continue;
		if(nodecfg != NULL ) { 		/* Skip areas not meant for this address */
			if(!area_is_linked(area,&nodecfg->addr))
				continue;
		}
		if(sub_code!=NULL && *sub_code && stricmp(sub_code,scfg.sub[subnum]->code))
			continue;
		printf("\rScanning %-*.*s -> %-*s "
			,LEN_EXTCODE,LEN_EXTCODE,scfg.sub[subnum]->code
			,FIDO_AREATAG_LEN, tag);
		ptr=0;
		if(!rescan)
			ptr=read_export_ptr(subnum, tag);

		msgs=getlastmsg(subnum, &lastmsg,0);
		if(msgs<1 || (!rescan && ptr>=lastmsg)) {
			if(msgs>=0 && ptr>lastmsg && !rescan && !opt_leave_msgptrs) {
				lprintf(LOG_DEBUG,"Fixing new-scan pointer (%u, lastmsg=%u).", ptr, lastmsg);
				write_export_ptr(subnum, lastmsg, tag);
			}
			continue;
		}

		int retval;
		smb_t smb;
		if((retval = smb_open_sub(&scfg, &smb, subnum)) != SMB_SUCCESS) {
			lprintf(LOG_ERR,"ERROR %d (%s) line %d opening sub: %s", retval, smb.last_error, __LINE__, smb.file);
			continue;
		}

		post=NULL;
		posts=loadmsgs(&smb, &post, ptr);

		if(!posts)	{ /* no new messages */
			smb_close(&smb);
			FREE_AND_NULL(post);
			continue;
		}

		echostat_t* stat = get_echostat(tag, /* create: */true);
		if(stat == NULL) {
			lprintf(LOG_CRIT, "Failed to allocated memory for echostats!");
			break;
		}
		stat->known = true;

		for(m=exp=0;m<posts && !terminated;m++) {
			printf("\r%*s %5lu of %-5"PRIu32"  "
				,LEN_EXTCODE,scfg.sub[subnum]->code,m+1,posts);
			memset(&msg,0,sizeof(msg));
			msg.idx=post[m].idx;
			if((retval = smb_lockmsghdr(&smb, &msg))!=SMB_SUCCESS) {
				lprintf(LOG_ERR,"ERROR %d (%s) line %d locking %s msghdr"
					,retval, smb.last_error,__LINE__, smb.file);
				continue;
			}
			retval = smb_getmsghdr(&smb, &msg);
			if(retval != SMB_SUCCESS || msg.hdr.number!=post[m].idx.number) {
				smb_unlockmsghdr(&smb, &msg);
				smb_freemsgmem(&msg);

				msg.hdr.number=post[m].idx.number;
				if((retval=smb_getmsgidx(&smb, &msg)) != SMB_SUCCESS) {
					lprintf(LOG_ERR,"ERROR %d (%s) line %d reading %s index", retval, smb.last_error, __LINE__
						,smb.file);
					continue;
				}
				if((retval = smb_lockmsghdr(&smb, &msg))!=SMB_SUCCESS) {
					lprintf(LOG_ERR,"ERROR %d (%s) line %d locking %s msghdr", retval, smb.last_error, __LINE__
						,smb.file);
					continue;
				}
				if((retval = smb_getmsghdr(&smb, &msg))!=SMB_SUCCESS) {
					smb_unlockmsghdr(&smb, &msg);
					lprintf(LOG_ERR,"ERROR %d (%s) line %d reading %s msghdr", retval, smb.last_error, __LINE__
						,smb.file);
					continue;
				}
			}

			if((!rescan && !(opt_export_ftn_echomail) && (msg.from_net.type==NET_FIDO))
				|| !strnicmp(msg.subj,"NE:",3)) {   /* no echo */
				smb_unlockmsghdr(&smb, &msg);
				smb_freemsgmem(&msg);
				continue;   /* From a Fido node, ignore it */
			}

			if(msg.from_net.type!=NET_NONE
				&& msg.from_net.type!=NET_FIDO
				&& !(scfg.sub[subnum]->misc&SUB_GATE)) {
				smb_unlockmsghdr(&smb, &msg);
				smb_freemsgmem(&msg);
				continue;
			}

			if(msg.hdr.type != SMB_MSG_TYPE_NORMAL) {
				smb_unlockmsghdr(&smb, &msg);
				smb_freemsgmem(&msg);
				continue;
			}

			if(!rescan && cfg.max_echomail_age != 0
				&& ((now > msg.hdr.when_written.time && (now - msg.hdr.when_written.time) > cfg.max_echomail_age)
				|| (now > msg.hdr.when_imported.time && (now - msg.hdr.when_imported.time) > cfg.max_echomail_age))) {
				smb_unlockmsghdr(&smb, &msg);
				smb_freemsgmem(&msg);
				continue;
			}

			memset(&hdr,0,sizeof(fmsghdr_t));	 /* Zero the header */
			hdr.origzone=scfg.sub[subnum]->faddr.zone;
			hdr.orignet=scfg.sub[subnum]->faddr.net;
			hdr.orignode=scfg.sub[subnum]->faddr.node;
			hdr.origpoint=scfg.sub[subnum]->faddr.point;

			hdr.attr=FIDO_LOCAL;
			if(msg.hdr.attr&MSG_PRIVATE)
				hdr.attr|=FIDO_PRIVATE;

			SAFECOPY(hdr.from,msg.from);

			tt = msg.hdr.when_written.time;
			if((tm = localtime(&tt)) != NULL)
				sprintf(hdr.time,"%02u %3.3s %02u  %02u:%02u:%02u"
					,tm->tm_mday,mon[tm->tm_mon],TM_YEAR(tm->tm_year)
					,tm->tm_hour,tm->tm_min,tm->tm_sec);

			SAFECOPY(hdr.to,msg.to);

			SAFECOPY(hdr.subj,msg.subj);

			buf=smb_getmsgtxt(&smb, &msg,GETMSGTXT_ALL);
			if(!buf) {
				smb_unlockmsghdr(&smb, &msg);
				smb_freemsgmem(&msg);
				continue;
			}
			lprintf(LOG_DEBUG,"Exporting %s message #%u from %s to %s in area: %s"
				,scfg.sub[subnum]->code, msg.hdr.number, msg.from, msg.to, tag);
			fmsgbuflen=strlen((char *)buf)+4096; /* over alloc for kludge lines */
			fmsgbuf=malloc(fmsgbuflen);
			if(!fmsgbuf) {
				lprintf(LOG_ERR,"ERROR line %d allocating %lu bytes for fmsgbuf"
					,__LINE__,fmsgbuflen);
				smb_unlockmsghdr(&smb, &msg);
				smb_freemsgmem(&msg);
				free(buf);
				continue;
			}
			fmsgbuflen-=1024;	/* give us a bit of a guard band here */

			tear=0;
			f=0;

			if(!fidoctrl_line_exists(&msg, "TZUTC:")) {
				tzone=smb_tzutc(msg.hdr.when_written.zone);
				if(tzone<0) {
					minus="-";
					tzone=-tzone;
				} else
					minus="";
				f+=sprintf(fmsgbuf+f,"\1TZUTC: %s%02d%02u\r"		/* TZUTC (FSP-1001) */
					,minus,tzone/60,tzone%60);
			}
			if(msg.ftn_flags!=NULL)
				f+=sprintf(fmsgbuf+f,"\1FLAGS %.256s\r", msg.ftn_flags);

			char* p = ftn_msgid(scfg.sub[subnum], &msg, NULL, 0);
			if(p != NULL)
				f += sprintf(fmsgbuf+f,"\1MSGID: %.256s\r", p);

			if(msg.ftn_reply!=NULL)			/* use original REPLYID */
				f+=sprintf(fmsgbuf+f,"\1REPLY: %.256s\r", msg.ftn_reply);
			else if(msg.hdr.thread_back) {	/* generate REPLYID (from original message's MSG-ID, if it had one) */
				memset(&orig_msg,0,sizeof(orig_msg));
				orig_msg.hdr.number=msg.hdr.thread_back;
				if(smb_getmsgidx(&smb, &orig_msg))
					f+=sprintf(fmsgbuf+f,"\1REPLY: <%s>\r",smb.last_error);
				else {
					smb_lockmsghdr(&smb, &orig_msg);
					smb_getmsghdr(&smb, &orig_msg);
					smb_unlockmsghdr(&smb, &orig_msg);
					if(orig_msg.ftn_msgid != NULL && orig_msg.ftn_msgid[0])
						f+=sprintf(fmsgbuf+f,"\1REPLY: %.256s\r",orig_msg.ftn_msgid);
				}
			}
			if(msg.ftn_pid!=NULL)	/* use original PID */
				f+=sprintf(fmsgbuf+f,"\1PID: %.256s\r", msg.ftn_pid);
			if(msg.ftn_tid!=NULL)	/* use original TID */
				f+=sprintf(fmsgbuf+f,"\1TID: %.256s\r", msg.ftn_tid);
			else					/* generate TID */
				f+=sprintf(fmsgbuf+f,"\1TID: %s\r", sbbsecho_pid());

			if(msg.columns)
				f += sprintf(fmsgbuf+f, "\1COLS: %u\r", (unsigned int)msg.columns);

			if(msg.ftn_bbsid != NULL)	/* use original BBSID */
				f += sprintf(fmsgbuf + f, "\1BBSID: %.256s\r", msg.ftn_bbsid);
			else if(msg.from_net.type == NET_QWK)
				f += sprintf(fmsgbuf + f, "\1BBSID: %.256s\r", (char*)msg.from_net.addr);
			else if(msg.from_net.type != NET_FIDO)
				f += sprintf(fmsgbuf + f, "\1BBSID: %.256s\r", scfg.sys_id);

			if(rescan)
				f+=sprintf(fmsgbuf+f,"\1RESCANNED %s\r", smb_faddrtoa(&scfg.sub[subnum]->faddr,NULL));

			/* Unknown kludge lines are added here */
			for(l=0;l<msg.total_hfields && f<fmsgbuflen;l++)
				if(msg.hfield[l].type == FIDOCTRL)
					f+=sprintf(fmsgbuf+f,"\1%.512s\r",(char*)msg.hfield_dat[l]);

			const char* charset = msg.ftn_charset;
			if(scfg.sub[subnum]->misc&SUB_ASCII)
				charset = FIDO_CHARSET_ASCII;
			if(charset == NULL) {
				if(smb_msg_is_utf8(&msg) || (msg.hdr.auxattr & MSG_HFIELDS_UTF8))
					charset = FIDO_CHARSET_UTF8;
				else if(str_is_ascii(buf))
					charset = FIDO_CHARSET_ASCII;
				else
					charset = FIDO_CHARSET_CP437;
			}
			f += sprintf(fmsgbuf + f, "\1CHRS: %s\r", charset);
			if(msg.editor != NULL)
				f += sprintf(fmsgbuf + f, "\1NOTE: %s\r", msg.editor);

			for(l=0,cr=1;buf[l] && f<fmsgbuflen;l++) {
				if(buf[l]==CTRL_A) { /* Ctrl-A, so skip it and the next char */
					l++;
					if(buf[l]==0 || buf[l]=='Z')	/* EOF */
						break;
					continue;
				}

				if(msg.from_net.type != NET_FIDO) {	/* Don't convert tear-lines or origin-lines for FTN-originated messages */
					/* Need to support converting sole-LFs to Hard-CR and soft-CR (0x8D) as well */
					if(cfg.strip_lf && buf[l]=='\n')	/* Ignore line feeds */
						continue;

					if(cr) {
						char *tp = (char*)buf+l;
						/* Bugfixed: handle tear line detection/conversion and origin line detection/conversion even when line-feeds exist and aren't stripped */
						if(*tp == '\n')
							tp++;
						if(*tp=='-' && *(tp+1)=='-'
							&& *(tp+2)=='-'
							&& (*(tp+3)==' ' || *(tp+3)=='\r')) {
							if(cfg.convert_tear)	/* Convert to === */
								*tp=*(tp+1)=*(tp+2)='=';
							else
								tear=1;
						}
						else if(!(scfg.sub[subnum]->misc&SUB_NOTAG) && !strncmp(tp," * Origin: ",11))
							*(tp+1)='#';
					} /* Convert * Origin into # Origin */

					if(buf[l]=='\r')
						cr=1;
					else
						cr=0;
					if((scfg.sub[subnum]->misc&SUB_ASCII)) {
						if(buf[l]<' ' && buf[l]>=0 && buf[l]!='\r'
							&& buf[l]!='\n')			/* Ctrl ascii */
							buf[l]='.';             /* converted to '.' */
						if((uchar)buf[l]&0x80)		/* extended ASCII */
							buf[l]=exascii_to_ascii_char(buf[l]);
					}
				}
				fmsgbuf[f++]=buf[l];
			}

			FREE_AND_NULL(buf);
			fmsgbuf[f]=0;

			if(msg.from_net.type != NET_FIDO && !(scfg.sub[subnum]->misc&SUB_NOTAG)) {
				if(!tear) {  /* No previous tear line */
					strcat((char *)fmsgbuf,tear_line('-'));
				}

				sprintf(str," * Origin: %s (%s)\r"
					,scfg.sub[subnum]->origline[0] ? scfg.sub[subnum]->origline : scfg.origline
					,smb_faddrtoa(&scfg.sub[subnum]->faddr,NULL));
				strcat((char *)fmsgbuf,str);
			}

			for(uint u=0; u < cfg.areas; u++) {
				if(cfg.area[u].sub == subnum) {
					if(cfg.area[u].links == 0) {
						lprintf(LOG_ERR, "No links for sub: %s", scfg.sub[subnum]->code);
					} else {
						if(write_to_pkts(fmsgbuf, cfg.area[u]
							,nodecfg ? &nodecfg->addr : NULL, /* pkt_orig: */NULL, &hdr, msg_seen, msg_path, rescan))
							cfg.area[u].exported++;
					}
					break;
				}
			}
			exported++;
			exp++;
			new_echostat_msg(stat, ECHOSTAT_MSG_EXPORTED
				,smsg_to_echostat_msg(&msg, strlen(fmsgbuf), scfg.sub[subnum]->faddr));
			FREE_AND_NULL(fmsgbuf);
			printf("Exp: %lu ",exp);
			smb_unlockmsghdr(&smb, &msg);
			smb_freemsgmem(&msg);
		}

		smb_close(&smb);
		FREE_AND_NULL(post);

		if(!rescan && !opt_leave_msgptrs && lastmsg>ptr)
			write_export_ptr(subnum, lastmsg, tag);
	}

	printf("\r%*s\r", 70, "");

	for(unsigned u=0; u<cfg.areas; u++)
		if(cfg.area[u].exported)
			lprintf(LOG_INFO,"Exported: %5u msgs %*s -> %s"
				,cfg.area[u].exported,LEN_EXTCODE,scfg.sub[cfg.area[u].sub]->code
				,cfg.area[u].tag);

	if(exported)
		lprintf(LOG_INFO,"Exported: %5lu msgs total", exported);
	exported_echomail += exported;
}

void del_file_attachments(smbmsg_t* msg)
{
	if(!delfattach(&scfg, msg))
		lprintf(LOG_ERR, "ERROR %d (%s) removing file attachment for message %u (%s)"
			,errno, strerror(errno), msg->hdr.number, msg->subj);
}


bool retoss_bad_echomail(void)
{
	area_t* badarea;

	if(cfg.badecho < 0 || (uint)cfg.badecho >= cfg.areas)
		return false;

	badarea = &cfg.area[cfg.badecho];

	int badsub = badarea->sub;
	if(badsub < 0 || badsub >= scfg.total_subs)
		return false;

	printf("\nRe-tossing Bad EchoMail from %s...\n", scfg.sub[badsub]->code);

	smb_t badsmb;
	int retval = smb_open_sub(&scfg, &badsmb, badsub);
	if(retval != SMB_SUCCESS) {
		lprintf(LOG_ERR,"ERROR %d (%s) line %d opening %s", retval, badsmb.last_error,__LINE__
			,badsmb.file);
		return false;
	}

	post_t *post = NULL;
	ulong posts	= loadmsgs(&badsmb, &post, /* ptr: */0);

	if(posts < 1) { /* no messages */
		smb_close(&badsmb);
		FREE_AND_NULL(post);
		return false;
	}

	ulong retossed = 0;
	for(ulong m=0; m<posts && !terminated; m++) {
		printf("\r%*s %5lu of %-5lu  "
			,LEN_EXTCODE,scfg.sub[badsub]->code,m+1,posts);
		smbmsg_t badmsg;
		memset(&badmsg, 0, sizeof(badmsg));
		badmsg.idx = post[m].idx;
		if((retval=smb_lockmsghdr(&badsmb, &badmsg))!=SMB_SUCCESS) {
			lprintf(LOG_ERR,"ERROR %d (%s) line %d locking %s msghdr"
				,retval,badsmb.last_error,__LINE__,badsmb.file);
			continue;
		}
		retval = smb_getmsghdr(&badsmb, &badmsg);
		if(retval || badmsg.hdr.number != post[m].idx.number) {
			smb_unlockmsghdr(&badsmb,&badmsg);
			smb_freemsgmem(&badmsg);

			badmsg.hdr.number=post[m].idx.number;
			if((retval=smb_getmsgidx(&badsmb, &badmsg))!=SMB_SUCCESS) {
				lprintf(LOG_ERR,"ERROR %d (%s) line %d reading %s index", retval, badsmb.last_error, __LINE__
					,badsmb.file);
				continue;
			}
			if((retval=smb_lockmsghdr(&badsmb,&badmsg))!=SMB_SUCCESS) {
				lprintf(LOG_ERR,"ERROR %d (%s) line %d locking %s msghdr", retval, badsmb.last_error, __LINE__
					,badsmb.file);
				continue;
			}
			if((retval=smb_getmsghdr(&badsmb,&badmsg))!=SMB_SUCCESS) {
				smb_unlockmsghdr(&badsmb,&badmsg);
				lprintf(LOG_ERR,"ERROR %d (%s) line %d reading %s msghdr", retval, badsmb.last_error, __LINE__
					,badsmb.file);
				continue;
			}
		}

		if(badmsg.from_net.type != NET_FIDO
			|| badmsg.ftn_area == NULL
			|| badmsg.hdr.type != SMB_MSG_TYPE_NORMAL) {
			smb_unlockmsghdr(&badsmb,&badmsg);
			smb_freemsgmem(&badmsg);
			continue;
		}

		uint areanum = find_area(badmsg.ftn_area);
		if(!area_is_valid(areanum) || cfg.area[areanum].sub >= scfg.total_subs) {
			smb_unlockmsghdr(&badsmb,&badmsg);
			smb_freemsgmem(&badmsg);
			continue;
		}
		int subnum = cfg.area[areanum].sub;
		printf("-> %s\n", scfg.sub[subnum]->code);
		lprintf(LOG_DEBUG,"Moving %s message #%u to sub-board %s"
			,scfg.sub[badsub]->code, badmsg.hdr.number, scfg.sub[subnum]->code);

		smbmsg_t newmsg;
		if((retval = smb_copymsgmem(NULL, &newmsg, &badmsg)) != SMB_SUCCESS) {
			smb_unlockmsghdr(&badsmb,&badmsg);
			smb_freemsgmem(&badmsg);
			continue;
		}

		char* body = smb_getmsgtxt(&badsmb, &badmsg, GETMSGTXT_NO_TAILS);
		if(body == NULL) {
			smb_unlockmsghdr(&badsmb,&badmsg);
			smb_freemsgmem(&badmsg);
			smb_freemsgmem(&newmsg);
			continue;
		}
		truncsp(body);
		char* tail = smb_getmsgtxt(&badsmb, &badmsg, GETMSGTXT_TAIL_ONLY);
		if(tail != NULL)
			truncsp(tail);

		/* Target sub-board: */
		{
			smb_t smb;
			retval = smb_open_sub(&scfg, &smb, subnum);
			if(retval != SMB_SUCCESS) {
				lprintf(LOG_ERR,"ERROR %d (%s) line %d opening %s", retval, smb.last_error,__LINE__
					,smb.file);
				smb_unlockmsghdr(&badsmb,&badmsg);
				smb_freemsgmem(&badmsg);
				smb_freemsgmem(&newmsg);
				FREE_AND_NULL(body);
				FREE_AND_NULL(tail);
				continue;
			}

			long	dupechk_hashes=SMB_HASH_SOURCE_DUPE;
			if(smb.status.max_crcs == 0)
				dupechk_hashes&=~(1<<SMB_HASH_SOURCE_BODY);
			char* body_start = body;
			if(strncmp(body_start, "AREA:", 5) == 0) {
				FIND_CHAR(body_start, '\r');
				SKIP_CHARSET(body_start, "\r\n");
			}
			retval = smb_addmsg(&smb, &newmsg, smb.status.attr&SMB_HYPERALLOC, dupechk_hashes, XLAT_NONE
				,(uchar*)body_start, (uchar*)tail);
			FREE_AND_NULL(body);
			FREE_AND_NULL(tail);

			if(retval != SMB_SUCCESS)
				lprintf(LOG_ERR, "ERROR %d (%s) line %d adding message to sub: %s"
					,retval, smb.last_error, __LINE__, scfg.sub[subnum]->code);
			if(retval == SMB_SUCCESS || retval == SMB_DUPE_MSG) {
				badmsg.hdr.attr |= MSG_DELETE;
				if((retval = smb_updatemsg(&badsmb, &badmsg)) != SMB_SUCCESS)
					lprintf(LOG_ERR,"!ERROR %d (%s) deleting msg #%u in bad echo sub"
						,retval, badsmb.last_error, badmsg.hdr.number);
				if(badmsg.hdr.auxattr&MSG_FILEATTACH)
					del_file_attachments(&badmsg);
				retossed++;
			}
			smb_close(&smb);
		}
		smb_unlockmsghdr(&badsmb,&badmsg);
		smb_freemsgmem(&badmsg);
		smb_freemsgmem(&newmsg);
	}

	smb_close(&badsmb);
	FREE_AND_NULL(post);

	printf("\r%*s\r", 70, "");
	if(retossed)
		lprintf(LOG_INFO, "Re-tossed (moved) %lu messages successfully from the bad-echo area", retossed);
	return true;
}


/* New Feature (as of Nov-22-2015):
   Export NetMail from SMB (data/mail) to .msg format
*/
int export_netmail(void)
{
	int			i;
	char*		txt;
	smbmsg_t	msg;
	ulong		exported = 0;

	printf("\nExporting Outbound NetMail from %smail to %s*.msg ...\n", scfg.data_dir, scfg.netmail_dir);

	if(email->shd_fp==NULL) {
		sprintf(email->file,"%smail",scfg.data_dir);
		email->retry_time=scfg.smb_retry_time;
		if((i=smb_open(email))!=SMB_SUCCESS) {
			lprintf(LOG_ERR,"ERROR %d (%s) line %d opening %s",i,email->last_error,__LINE__,email->file);
			return i;
		}
	}
	if((i=smb_locksmbhdr(email)) != SMB_SUCCESS) {
		lprintf(LOG_ERR,"ERROR %d (%s) line %d locking %s",i,email->last_error,__LINE__,email->file);
		return i;
	}

	memset(&msg, 0, sizeof(msg));
	rewind(email->sid_fp);
	for(;!feof(email->sid_fp);msg.idx_offset++) {

		smb_freemsgmem(&msg);
		if(fread(&msg.idx, sizeof(msg.idx), 1, email->sid_fp) != 1)
			break;

		if((msg.idx.attr&MSG_DELETE) || msg.idx.to != 0)
			continue;

		if((i = smb_getmsghdr(email, &msg)) != SMB_SUCCESS) {
			lprintf(LOG_ERR,"ERROR %d (%s) line %d reading msg header #%u from %s"
				,i, email->last_error, __LINE__, msg.idx.number, email->file);
			continue;
		}

		if(msg.to_ext != 0 || msg.to_net.type != NET_FIDO)
			continue;

		printf("NetMail msg #%u from %s to %s (%s): "
			,msg.hdr.number, msg.from, msg.to, smb_faddrtoa(msg.to_net.addr,NULL));
		if(msg.hdr.netattr&MSG_INTRANSIT) {
			printf("in-transit\n");
			continue;
		}
		if((msg.hdr.netattr&MSG_SENT) && !cfg.ignore_netmail_sent_attr) {
			printf("already sent\n");
			continue;
		}

		lprintf(LOG_DEBUG, "Exporting NetMail message #%u from %s to %s (%s)"
			,msg.hdr.number, msg.from, msg.to, smb_faddrtoa(msg.to_net.addr,NULL));
		char* msg_subj = msg.subj;
		if((txt=smb_getmsgtxt(email, &msg, GETMSGTXT_BODY_ONLY)) != NULL) {
			char filename[MAX_PATH+1] = {0};
			uint32_t filelen = 0;
			uint8_t* filedata;
			if((filedata = smb_getattachment(&msg, txt, filename, sizeof(filename), &filelen, /* attachment_index */0)) != NULL
				&& filename[0] != 0 && filelen > 0) {
				lprintf(LOG_DEBUG, "MIME attachment decoded: %s (%lu bytes)", filename, (ulong)filelen);
				char outdir[MAX_PATH+1];
				SAFEPRINTF2(outdir, "%sfile/%04u.out", scfg.data_dir, msg.idx.from);
				(void)mkpath(outdir);
				char fpath[MAX_PATH+1];
				SAFEPRINTF2(fpath, "%s/%s", outdir, filename);
				FILE* fp = fopen(fpath, "wb");
				if(fp == NULL)
					lprintf(LOG_ERR, "!ERROR %d (%s) opening/creating %s", errno, strerror(errno), fpath);
				else {
					int result = fwrite(filedata, filelen, 1, fp);
					fclose(fp);
					if(!result)
						lprintf(LOG_ERR, "!ERROR %d (%s) writing %u bytes to %s", errno, strerror(errno), filelen, fpath);
					else {
						lprintf(LOG_DEBUG, "Decoded MIME attachment stored as: %s", fpath);
						msg.hdr.auxattr |= (MSG_FILEATTACH | MSG_KILLFILE);
						msg_subj = fpath;
					}
				}
			}
			smb_freemsgtxt(txt);
		}

		if((txt=smb_getmsgtxt(email,&msg,GETMSGTXT_ALL|GETMSGTXT_PLAIN)) == NULL) {
			lprintf(LOG_ERR,"!ERROR %s getting message text for mail msg #%u"
				, email->last_error, msg.hdr.number);
			continue;
		}

		create_netmail(msg.to, &msg, msg_subj, txt, /* dest: */*(fidoaddr_t*)msg.to_net.addr, /* src: */NULL);
		FREE_AND_NULL(txt);

		msg.hdr.netattr |= MSG_SENT;
		msg.hdr.netattr &= ~MSG_INTRANSIT;
		if(cfg.delete_netmail || (msg.hdr.netattr&MSG_KILLSENT) || msg.from_ext == NULL) {
			/* Delete exported netmail */
			msg.hdr.attr |= MSG_DELETE;
			if((i = smb_updatemsg(email, &msg)) != SMB_SUCCESS)
				lprintf(LOG_ERR,"!ERROR %d (%s) line %d deleting mail msg #%u"
					,i, email->last_error, __LINE__, msg.hdr.number);
			(void)fseek(email->sid_fp, (msg.idx_offset+1)*sizeof(msg.idx), SEEK_SET);
		} else {
			if((i = smb_putmsghdr(email, &msg)) != SMB_SUCCESS)
				lprintf(LOG_ERR,"!ERROR %d (%s) line %d updating msg header for mail msg #%u"
					,i, email->last_error, __LINE__, msg.hdr.number);
		}
		exported++;
	}
	smb_freemsgmem(&msg);
	exported_netmail += exported;
	return smb_unlocksmbhdr(email);
}

char* freadstr(FILE* fp, char* str, size_t maxlen)
{
	int		ch;
	size_t	rd=0;
	size_t	len=0;

	memset(str,0,maxlen);	/* pre-terminate */

	while(rd<maxlen && (ch=fgetc(fp))!=EOF) {
		if(ch==0)
			break;
		if((uchar)ch>=' ')	/* not a ctrl char (garbage?) */
			str[len++]=ch;
		rd++;
	}
	if(ch != 0)	/* Must be NUL-terminated */
		return NULL;

	str[maxlen-1]=0;	/* Force terminator */

	return(str);
}

bool netmail_sent(const char* fname)
{
	FILE*		fp;
	fmsghdr_t	hdr;

	if(cfg.delete_netmail)
		return delfile(fname, __LINE__);
	if((fp=fopen(fname, "r+b")) == NULL) {
		lprintf(LOG_ERR, "ERROR %d (%s) opening %s", errno, strerror(errno), fname);
		return false;
	}
	if(!fread_fmsghdr(&hdr,fp)) {
		fclose(fp);
		lprintf(LOG_ERR, "ERROR line %u reading header from %s", __LINE__, fname);
		return false;
	}
	hdr.attr|=FIDO_SENT;
	rewind(fp);
	(void)fwrite(&hdr, sizeof(hdr), 1, fp);
	fclose(fp);
	if(!cfg.ignore_netmail_kill_attr && (hdr.attr&FIDO_KILLSENT))
		return delfile(fname, __LINE__);
	return true;
}

void pack_netmail(void)
{
	char	str[MAX_PATH+1];
	char	path[MAX_PATH+1];
	char	packet[MAX_PATH+1];
	const char*	outbound;
	char*	fmsgbuf=NULL;
	char	ch;
	int		file;
	int		fmsg;
	size_t	f;
	glob_t	g;
	FILE*	stream;
	FILE*	fidomsg;
	fpkthdr_t	pkthdr;
	fmsghdr_t	hdr;
	fidoaddr_t	addr;
	addrlist_t msg_seen,msg_path;
	area_t fakearea;
	nodecfg_t*	nodecfg = NULL;

	memset(&msg_seen,0,sizeof(msg_seen));
	memset(&msg_path,0,sizeof(msg_path));
	memset(&fakearea,0,sizeof(fakearea));

	printf("\nPacking Outbound NetMail from %s*.msg ...\n",scfg.netmail_dir);

#ifdef __unix__
	sprintf(str,"%s*.[Mm][Ss][Gg]",scfg.netmail_dir);
#else
	sprintf(str,"%s*.msg",scfg.netmail_dir);
#endif
	glob(str,0,NULL,&g);
	for(f=0;f<g.gl_pathc && !terminated;f++) {
		SAFECOPY(path,g.gl_pathv[f]);

		if((fidomsg=fnopen(&fmsg,path,O_RDWR))==NULL) {
			lprintf(LOG_ERR,"ERROR %u (%s) line %d opening %s",errno,strerror(errno),__LINE__,path);
			continue;
		}
		if(filelength(fmsg)<sizeof(fmsghdr_t)) {
			lprintf(LOG_WARNING,"%s Invalid length of %lu bytes",path,filelength(fmsg));
			fclose(fidomsg);
			continue;
		}
		if(!fread_fmsghdr(&hdr,fidomsg)) {
			fclose(fidomsg);
			lprintf(LOG_ERR,"ERROR line %d reading fido msghdr from %s",__LINE__,path);
			continue;
		}
		if(hdr.destzone == 0)
			hdr.destzone = sys_faddr.zone;
		if(hdr.origzone == 0)
			hdr.origzone = sys_faddr.zone;
		getzpt(fidomsg,&hdr);				/* use kludge if found */
		addr.zone		= hdr.destzone;
		addr.net		= hdr.destnet;
		addr.node		= hdr.destnode;
		addr.point		= hdr.destpoint;
		printf("NetMail msg %s from %s (%s) to %s (%s): "
			,getfname(path), hdr.from, fmsghdr_srcaddr_str(&hdr), hdr.to, smb_faddrtoa(&addr,NULL));
		if(sysfaddr_is_valid(find_sysfaddr(addr, true)))	{				  /* In-bound, so ignore */
			printf("in-bound\n");
			fclose(fidomsg);
			continue;
		}
		if((hdr.attr&FIDO_SENT) && !cfg.ignore_netmail_recv_attr) {
			printf("already sent\n");
			fclose(fidomsg);
			continue;
		}
		if((cfg.flo_mailer) && (hdr.attr&FIDO_FREQ)) {
			char req[MAX_PATH+1];
			FILE* fp;

			fclose(fidomsg);
			lprintf(LOG_INFO, "BSO file request from %s (%s) to %s (%s): %s"
				,hdr.from, fmsghdr_srcaddr_str(&hdr), hdr.to, smb_faddrtoa(&addr, NULL), hdr.subj);
			if(!bso_lock_node(addr))
				continue;
			outbound = get_current_outbound(addr, cfg.use_outboxes);
			if(outbound == NULL)
				continue;
			if(addr.point)
				SAFEPRINTF2(req,"%s%08x.req",outbound,addr.point);
			else
				SAFEPRINTF3(req,"%s%04x%04x.req",outbound,addr.net,addr.node);
			if((fp=fopen(req,"a")) == NULL)
				lprintf(LOG_ERR,"ERROR %d (%s) creating/opening %s", errno, strerror(errno), req);
			else {
				fprintf(fp,"%s\n",getfname(hdr.subj));
				fclose(fp);
				if(write_flofile(req, addr,/* bundle: */false, cfg.use_outboxes, /* del_file: */true, hdr.attr))
					continue;
				netmail_sent(path);
			}
			continue;
		}

		lprintf(LOG_INFO, "Packing NetMail (%s) from %s (%s) to %s (%s), attr: %04hX, subject: %s"
			,getfname(path)
			,hdr.from, fmsghdr_srcaddr_str(&hdr)
			,hdr.to, smb_faddrtoa(&addr,NULL)
			,hdr.attr, hdr.subj);
		FREE_AND_NULL(fmsgbuf);
		fmsgbuf=getfmsg(fidomsg,NULL);
		fclose(fidomsg);
		if(fmsgbuf == NULL) {
			lprintf(LOG_ERR,"ERROR line %d allocating memory for NetMail fmsgbuf"
				,__LINE__);
			bail(1);
			return;
		}

		nodecfg=findnodecfg(&cfg, addr, 0);
		if(!sysfaddr_is_valid(find_sysfaddr(addr, false))) {
			if(nodecfg!=NULL && nodecfg->route.zone	&& nodecfg->status==MAIL_STATUS_NORMAL
				&& !(hdr.attr&(FIDO_CRASH|FIDO_HOLD))) {
				addr=nodecfg->route;		/* Routed */
				lprintf(LOG_INFO, "Routing NetMail (%s) to %s",getfname(path),smb_faddrtoa(&addr,NULL));
				nodecfg=findnodecfg(&cfg, addr,0);
			}
			if(nodecfg == NULL && addr.point != 0) {
				fidoaddr_t	boss = addr;
				boss.point = 0;
				if((nodecfg = findnodecfg(&cfg, boss, /* exact: */true)) != NULL) {
					addr = boss;
					lprintf(LOG_INFO, "Routing NetMail (%s) to boss-node %s"
						,getfname(path), smb_faddrtoa(&addr, NULL));
				}
			}
		}
		if(!bso_lock_node(addr))
			continue;

		if(cfg.flo_mailer) {
			outbound = get_current_outbound(addr, cfg.use_outboxes);
			if(outbound == NULL)
				continue;
			if(cfg.use_outboxes && nodecfg!=NULL && nodecfg->outbox[0])
				SAFECOPY(packet, pktname(outbound));
			else {
				ch='o';
				if(hdr.attr&FIDO_CRASH)
					ch='c';
				else if(hdr.attr&FIDO_HOLD)
					ch='h';
				else if(nodecfg!=NULL) {
					switch(nodecfg->status) {
						case MAIL_STATUS_CRASH:	ch='c';	break;
						case MAIL_STATUS_HOLD:	ch='h'; break;
						default:
							if(nodecfg->direct)
								ch='d';
							break;
					}
				}
				if(addr.point)
					SAFEPRINTF3(packet,"%s%08x.%cut",outbound,addr.point,ch);
				else
					SAFEPRINTF4(packet,"%s%04x%04x.%cut",outbound,addr.net,addr.node,ch);
			}
			if(hdr.attr&FIDO_FILE) {
				// Parse Kill-File-Sent (KFS) from FLAGS from control paragraph (kludge line) within msg body
				const char* flags = strstr(fmsgbuf, "\1FLAGS ");
				if(flags != NULL && flags != fmsgbuf && *(flags-1) != '\r' && *(flags-1) != '\n') flags = NULL;
				const char* kfs = NULL;
				if(flags != NULL) {
					flags += 7;
					char* cr = strchr(flags, '\r');
					kfs = strstr(flags, "KFS");
					if(kfs > cr)
						kfs = NULL;
				}
				if(write_flofile(hdr.subj,addr,/* bundle: */false,/* use_outbox: */false, /* del_file: */kfs != NULL, hdr.attr))
					bail(1);
				SAFECOPY(hdr.subj, getfname(hdr.subj));	/* Don't include the file path in the subject */
			}
		}
		else {
			SAFECOPY(packet,pktname(cfg.outbound));
		}

		if((stream=fnopen(&file,packet,O_RDWR|O_CREAT))==NULL) {
			lprintf(LOG_ERR,"ERROR %u (%s) line %d opening %s",errno,strerror(errno),__LINE__,packet);
			bail(1);
			return;
		}
		const char* newpkt="";
		if(filelength(file) < sizeof(fpkthdr_t)) {
			newpkt="new ";
			chsize(file,0);
			rewind(stream);
			new_pkthdr(&pkthdr, getsysfaddr(addr), addr, nodecfg);
			(void)fwrite(&pkthdr,sizeof(pkthdr),1,stream);
		} else
			(void)fseek(stream,find_packet_terminator(stream),SEEK_SET);

		lprintf(LOG_DEBUG, "Adding NetMail (%s) to %spacket for %s: %s"
			,getfname(path)
			,newpkt, smb_faddrtoa(&addr, NULL), packet);
		putfmsg(stream, fmsgbuf, &hdr, fakearea, msg_seen, msg_path);

		/* Write packet terminator */
		terminate_packet(stream);

		FREE_AND_NULL(fmsgbuf);
		fclose(stream);
		/**************************************/
		/* Delete source netmail if specified */
		/**************************************/
		netmail_sent(path);
		printf("\n");
		packed_netmail++;
	}
	globfree(&g);
	FREE_AND_NULL(fmsgbuf);
}

/* Find any packets that have been left behind in the temp directory */
void find_stray_packets(void)
{
	char	path[MAX_PATH+1];
	char	packet[MAX_PATH+1];
	char	str[128];
	FILE*	fp;
	size_t	f;
	glob_t	g;
	off_t	flen;
	uint16_t	terminator;
	fidoaddr_t	pkt_orig;
	fidoaddr_t	pkt_dest;
	fpkthdr_t	pkthdr;
	enum pkt_type pkt_type;

	printf("\nScanning %s for Stray Outbound Packets...\n", cfg.temp_dir);
	SAFEPRINTF(path,"%s*.pkt",cfg.temp_dir);
	glob(path,0,NULL,&g);
	for(f=0;f<g.gl_pathc && !terminated;f++) {

		SAFECOPY(packet, (char*)g.gl_pathv[f]);

		printf("%21s: %s ","Stray Outbound Packet", packet);

		if((fp=fopen(packet,"rb"))==NULL) {
			lprintf(LOG_ERR,"ERROR %u (%s) opening stray packet: %s"
				,errno, strerror(errno), packet);
			continue;
		}
		flen = filelength(fileno(fp));
		if(flen < sizeof(pkthdr) + sizeof(terminator)) {
			lprintf(LOG_ERR,"ERROR invalid length of %lu bytes for stray packet: %s", flen, packet);
			fclose(fp);
			delfile(packet, __LINE__);
			continue;
		}
		if(fread(&pkthdr,sizeof(pkthdr),1,fp)!=1) {
			lprintf(LOG_ERR,"ERROR reading header (%lu bytes) from stray packet: %s"
				,(ulong)sizeof(pkthdr), packet);
			fclose(fp);
			delfile(packet, __LINE__);
			continue;
		}
		terminator = ~FIDO_PACKET_TERMINATOR;
		(void)fseek(fp, (long)(flen-sizeof(terminator)), SEEK_SET);
		if(fread(&terminator, sizeof(terminator), 1, fp) != 1)
			lprintf(LOG_WARNING, "Failure reading last byte from %s", packet);
		fclose(fp);
		if(!parse_pkthdr(&pkthdr, &pkt_orig, &pkt_dest, &pkt_type)) {
			lprintf(LOG_WARNING,"Failure to parse packet (%s) header, type: %u"
				,packet, pkthdr.type2.pkttype);
			delfile(packet, __LINE__);
			continue;
		}
		uint sysfaddr = find_sysfaddr(pkt_orig, true);
		lprintf(LOG_WARNING,"Stray Outbound Packet (type %s) found, from %s address %s to %s: %s"
			,pktTypeStringList[pkt_type], sysfaddr_is_valid(sysfaddr) ? "local" : "FOREIGN"
			,smb_faddrtoa(&pkt_orig, NULL), smb_faddrtoa(&pkt_dest, str), packet);
		if(!sysfaddr_is_valid(sysfaddr))
			continue;
		outpkt_t* pkt;
		if((pkt = calloc(1, sizeof(outpkt_t))) == NULL)
			continue;
		if((pkt->filename = strdup(packet)) == NULL) {
			free(pkt);
			continue;
		}
		if(terminator == FIDO_PACKET_TERMINATOR)
			lprintf(LOG_DEBUG, "Stray packet already finalized: %s", packet);
		else {
			if((pkt->fp = fopen(pkt->filename, "ab")) == NULL) {
				lprintf(LOG_ERR, "ERROR %d (%s) opening %s", errno, strerror(errno), pkt->filename);
				free(pkt);
				continue;
			}
		}
		pkt->orig = pkt_orig;
		pkt->dest = pkt_dest;
		listAddNode(&outpkt_list, pkt, 0, LAST_NODE);
	}
	if(g.gl_pathc)
		lprintf(LOG_DEBUG, "%lu stray outbound packets (%lu total pkts) found in %s"
			,listCountNodes(&outpkt_list), (ulong)g.gl_pathc, cfg.temp_dir);
	globfree(&g);
}

bool rename_bad_packet(const char* packet)
{
	lprintf(LOG_WARNING, "Bad packet detected: %s", packet);
	char badpkt[MAX_PATH+1];
	SAFECOPY(badpkt, packet);
	char* ext = getfext(badpkt);
	if(ext == NULL)
		return false;
	strcpy(ext, ".bad");
	if(mv(packet, badpkt, /* copy: */false) == 0)
		return true;
	if(cfg.delete_packets)
		delfile(packet, __LINE__);
	return false;
}

void import_packets(const char* inbound, nodecfg_t* inbox, bool secure)
{
	char	str[MAX_PATH+1];
	char	path[MAX_PATH+1];
	char	packet[MAX_PATH+1];
	char	areatag[128];
	char	password[FIDO_PASS_LEN+1];
	char*	fmsgbuf=NULL;
	char*	p;
	int		i,j;
	int		fmsg;
	int		result;
	FILE*	fidomsg;
	size_t	f;
	uint16_t	terminator;
	fidoaddr_t	pkt_orig;
	fidoaddr_t	pkt_dest;
	fpkthdr_t	pkthdr;
	fpkdmsg_t	pkdmsg;
	enum pkt_type pkt_type;
	glob_t	g;
	fmsghdr_t hdr;
	uint	subnum[MAX_OPEN_SMBS]={INVALID_SUB};
	addrlist_t msg_seen,msg_path;
	area_t curarea;

	memset(&msg_seen,0,sizeof(addrlist_t));
	memset(&msg_path,0,sizeof(addrlist_t));

#ifdef __unix__
	sprintf(path,"%s*.[Pp][Kk][Tt]", inbound);
#else
	sprintf(path,"%s*.pkt", inbound);
#endif
	glob(path,0,NULL,&g);
	for(f=0;f<g.gl_pathc && !terminated;f++) {

		SAFECOPY(packet,g.gl_pathv[f]);

		if((fidomsg=fnopen(&fmsg,packet,O_RDWR))==NULL) {
			lprintf(LOG_ERR,"ERROR %u (%s) line %d opening %s",errno,strerror(errno),__LINE__,packet);
			continue;
		}
		if(filelength(fmsg) < sizeof(pkthdr) + sizeof(terminator)) {
			lprintf(LOG_WARNING,"Invalid length of %s: %lu bytes"
				,packet,filelength(fmsg));
			fclose(fidomsg);
			rename_bad_packet(packet);
			continue;
		}

		terminator = ~FIDO_PACKET_TERMINATOR;
		(void)fseek(fidomsg, -(int)sizeof(terminator), SEEK_END);
		if(fread(&terminator, 1, sizeof(terminator), fidomsg) != sizeof(terminator)
			|| terminator != FIDO_PACKET_TERMINATOR) {
			fclose(fidomsg);
			lprintf(LOG_WARNING,"WARNING: packet %s not terminated correctly (0x%04hX)", packet, terminator);
			rename_bad_packet(packet);
			continue;
		}
		(void)fseek(fidomsg,0L,SEEK_SET);
		if(fread(&pkthdr,sizeof(pkthdr),1,fidomsg)!=1) {
			fclose(fidomsg);
			lprintf(LOG_ERR,"ERROR line %d reading %lu bytes from %s",__LINE__
				,(ulong)sizeof(pkthdr),packet);
			rename_bad_packet(packet);
			continue;
		}

		if(!parse_pkthdr(&pkthdr, &pkt_orig, &pkt_dest, &pkt_type)) {
			fclose(fidomsg);
			lprintf(LOG_WARNING,"%s is not a type 2 packet (type=%u)"
				,packet, pkthdr.type2.pkttype);
			rename_bad_packet(packet);
			continue;
		}
		if(inbox != NULL && memcmp(&pkt_orig, &inbox->addr, sizeof(pkt_orig)) != 0) {
			fclose(fidomsg);
			lprintf(LOG_WARNING, "Inbox packet from %s (not from %s): %s"
				,smb_faddrtoa(&pkt_orig, NULL), smb_faddrtoa(&inbox->addr, str), packet);
			rename_bad_packet(packet);
			continue;
		}

		nodecfg_t* nodecfg = findnodecfg(&cfg, pkt_orig, 1);
		SAFECOPY(password,(char*)pkthdr.type2.password);
		if(nodecfg !=NULL && (cfg.strict_packet_passwords || nodecfg->pktpwd[0])
			&& stricmp(password,nodecfg->pktpwd)) {
			lprintf(LOG_WARNING,"Packet %s from %s - "
				"Incorrect password ('%s' instead of '%s')"
				,packet,smb_faddrtoa(&pkt_orig,NULL)
				,password, nodecfg->pktpwd);
			fclose(fidomsg);
			rename_bad_packet(packet);
			continue;
		}

		lprintf(LOG_INFO, "Importing %s (Type %s, %1.1fKB) from %s to %s"
			,packet, pktTypeStringList[pkt_type], flength(packet)/1024.0
			,smb_faddrtoa(&pkt_orig,NULL), smb_faddrtoa(&pkt_dest, str));

		bool bad_packet = false;
		while(!feof(fidomsg)) {

			memset(&hdr,0,sizeof(fmsghdr_t));

			/* Sept-16-2013: copy the origin zone from the packet header
				as packed message headers don't have the zone information */
			hdr.origzone=pkt_orig.zone;

			FREE_AND_NULL(fmsgbuf);

			off_t msg_offset = ftell(fidomsg);
			/* Read fixed-length header fields */
			if(fread(&pkdmsg,sizeof(BYTE),sizeof(pkdmsg),fidomsg) != sizeof(pkdmsg))
				continue;

			if(pkdmsg.type == 2 /* Recognized type */
				&& pkdmsg.time[0] != 0
				&& pkdmsg.time[sizeof(pkdmsg.time)-1] == 0
				&& freadstr(fidomsg, hdr.to, sizeof(hdr.to)) != NULL
				&& freadstr(fidomsg, hdr.from, sizeof(hdr.from)) != NULL
				&& freadstr(fidomsg, hdr.subj, sizeof(hdr.subj)) != NULL
				) { /* Recognized type, copy fields */
				hdr.orignode	= pkdmsg.orignode;
				hdr.destnode	= pkdmsg.destnode;
				hdr.orignet		= pkdmsg.orignet;
				hdr.destnet		= pkdmsg.destnet;
				hdr.attr		= pkdmsg.attr;
				hdr.cost		= pkdmsg.cost;
				SAFECOPY(hdr.time,pkdmsg.time);
			} else {
				lprintf(LOG_NOTICE, "Grunged message (type %d) from %s at offset %ld in packet: %s"
					, pkdmsg.type, smb_faddrtoa(&pkt_orig, NULL), msg_offset, packet);
				printf("Grunged message!\n");
				bad_packet = true;
				break;
			}
			msg_offset = ftell(fidomsg);	// Save offset to msg body
			fmsgbuf = getfmsg(fidomsg, NULL);
			off_t next_msg = ftell(fidomsg);

			hdr.attr&=~FIDO_LOCAL;	/* Strip local bit, obviously not created locally */

			if(strncmp(fmsgbuf, "AREA:", 5) != 0) {					/* Netmail */
				(void)fseeko(fidomsg, msg_offset, SEEK_SET);
				import_netmail("", &hdr, fidomsg, inbound);
				(void)fseeko(fidomsg, next_msg, SEEK_SET);
				printf("\n");
				continue;
			}

			p = fmsgbuf + 5;					/* Skip "AREA:" */
			SKIP_WHITESPACE(p);					/* Skip any white space */
			SAFECOPY(areatag, p);
			truncstr(areatag, " \t\r\n");
			printf("%21s: ", areatag);          /* Show areatag/echotag: */

			echostat_t* stat = get_echostat(areatag, /* create: */true);
			if(stat == NULL) {
				lprintf(LOG_CRIT, "Failed to allocate memory for echostats!");
				break;
			}

			if(!parse_origin(fmsgbuf, &hdr))
				lprintf(LOG_WARNING, "%s: Failed to parse Origin Line in message from %s (%s) in packet from %s: %s"
					, areatag, hdr.from, fmsghdr_srcaddr_str(&hdr), smb_faddrtoa(&pkt_orig, NULL), packet);

			new_echostat_msg(stat, ECHOSTAT_MSG_RECEIVED, fidomsg_to_echostat_msg(&hdr, &pkt_orig, fmsgbuf));

			if(!opt_import_echomail) {
				printf("EchoMail Ignored");
				printf("\n");
				continue;
			}

			if(!secure && (nodecfg == NULL || nodecfg->pktpwd[0] == 0)) {
				lprintf(LOG_WARNING, "Unauthenticated %s EchoMail from %s (in the non-secure inbound directory) ignored"
					,areatag, smb_faddrtoa(&pkt_orig, NULL));
				printf("\n");
				bad_packet = true;
				continue;
			}

			if(area_is_valid(i = find_area(areatag))) {				/* Do we carry this area? */
				stat->known = true;
				if(cfg.area[i].sub!=INVALID_SUB)
					printf("%s ",scfg.sub[cfg.area[i].sub]->code);
				else
					printf("(Passthru) ");
				gen_psb(&msg_seen,&msg_path,fmsgbuf,pkt_orig.zone);	/* was destzone */
			} else {
				stat->known = false;
				printf("(Unknown) ");
				if(bad_areas != NULL && strListFind(bad_areas, areatag, /* case_sensitive: */false) < 0) {
					lprintf(LOG_NOTICE, "Adding unknown area (%s) to bad area list: %s", areatag, cfg.badareafile);
					strListPush(&bad_areas, areatag);
				}
				if(cfg.badecho>=0) {
					i=cfg.badecho;
					if(cfg.area[i].sub!=INVALID_SUB)
						printf("%s ",scfg.sub[cfg.area[i].sub]->code);
					else
						printf("(Passthru) ");
					gen_psb(&msg_seen,&msg_path,fmsgbuf,pkt_orig.zone);	/* was destzone */
				}
				else {
					printf("Skipped\n");
					continue;
				}
			}

			if(cfg.secure_echomail && cfg.area[i].sub!=INVALID_SUB) {
				if(!area_is_linked(i,&pkt_orig)) {
					lprintf(LOG_WARNING, "%s: Security violation - %s not in Area File"
						,areatag, smb_faddrtoa(&pkt_orig,NULL));
					printf("Security Violation (Not in %s)\n", cfg.areafile);
					bad_packet = true;
					continue;
				}
			}

			/* From here on out, i = area number and area[i].sub = sub number */

			memcpy(&curarea,&cfg.area[i],sizeof(area_t));
			curarea.tag=areatag;

			if(cfg.area[i].sub==INVALID_SUB) {			/* Passthru */
				strip_psb(fmsgbuf);
				write_to_pkts(fmsgbuf, curarea, NULL, &pkt_orig, &hdr, msg_seen, msg_path, /* rescan: */false);
				printf("\n");
				continue;
			} 						/* On to the next message */

			/* TODO: Should circular path detection occur before processing pass-through areas? */
			if(cfg.check_path && msg_path.addrs > 1) {
				for(j=0;j<scfg.total_faddrs;j++)
					if(check_psb(&msg_path,scfg.faddr[j]))
						break;
				if(j<scfg.total_faddrs) {
					printf("Circular path (%s) ",smb_faddrtoa(&scfg.faddr[j],NULL));
					cfg.area[i].circular++;
					lprintf(LOG_NOTICE, "%s: Circular path detected for %s in message from %s (%s)"
						,areatag,smb_faddrtoa(&scfg.faddr[j],NULL), hdr.from, fmsghdr_srcaddr_str(&hdr));
					printf("\n");
					new_echostat_msg(stat, ECHOSTAT_MSG_CIRCULAR
						,fidomsg_to_echostat_msg(&hdr, &pkt_orig, fmsgbuf));
					continue;
				}
			}

			for(j=0;j<MAX_OPEN_SMBS;j++)
				if(subnum[j]==cfg.area[i].sub)
					break;
			if(j<MAX_OPEN_SMBS) 				/* already open */
				cur_smb=j;
			else {
				if(smb[cur_smb].shd_fp) 		/* If open */
					cur_smb=!cur_smb;			/* toggle between 0 and 1 */
				smb_close(&smb[cur_smb]);		/* close, if open */
				subnum[cur_smb]=INVALID_SUB; 	/* reset subnum (just incase) */
			}

			if(smb[cur_smb].shd_fp==NULL) { 	/* Currently closed */
				sprintf(smb[cur_smb].file,"%s%s",scfg.sub[cfg.area[i].sub]->data_dir
					,scfg.sub[cfg.area[i].sub]->code);
				smb[cur_smb].retry_time=scfg.smb_retry_time;
				if((result=smb_open(&smb[cur_smb]))!=SMB_SUCCESS) {
					lprintf(LOG_ERR, "ERROR %d (%s) line %d opening area #%d, sub #%d"
						,result, smb[cur_smb].last_error, __LINE__, i+1, cfg.area[i].sub+1);
					strip_psb(fmsgbuf);
					write_to_pkts(fmsgbuf, curarea, NULL, &pkt_orig, &hdr, msg_seen, msg_path, /* rescan: */false);
					printf("\n");
					continue;
				}
				if(!filelength(fileno(smb[cur_smb].shd_fp))) {
					smb[cur_smb].status.max_crcs=scfg.sub[cfg.area[i].sub]->maxcrcs;
					smb[cur_smb].status.max_msgs=scfg.sub[cfg.area[i].sub]->maxmsgs;
					smb[cur_smb].status.max_age=scfg.sub[cfg.area[i].sub]->maxage;
					smb[cur_smb].status.attr=scfg.sub[cfg.area[i].sub]->misc&SUB_HYPER
							? SMB_HYPERALLOC:0;
					if((result=smb_create(&smb[cur_smb]))!=SMB_SUCCESS) {
						lprintf(LOG_ERR,"ERROR %d (%s) line %d creating %s"
							,result, smb[cur_smb].last_error, __LINE__, smb[cur_smb].file);
						smb_close(&smb[cur_smb]);
						strip_psb(fmsgbuf);
						write_to_pkts(fmsgbuf, curarea, NULL, &pkt_orig, &hdr, msg_seen, msg_path, /* rescan: */false);
						printf("\n");
						continue;
					}
				}

				subnum[cur_smb]=cfg.area[i].sub;
			}

			if((hdr.attr&FIDO_PRIVATE) && !(scfg.sub[cfg.area[i].sub]->misc&SUB_PRIV)) {
				lprintf(LOG_NOTICE, "%s: Private posts disallowed from %s (%s) to %s, subject: %s"
					,areatag, hdr.from, fmsghdr_srcaddr_str(&hdr), hdr.to, hdr.subj);
				strip_psb(fmsgbuf);
				write_to_pkts(fmsgbuf, curarea, NULL, &pkt_orig, &hdr, msg_seen, msg_path, /* rescan: */false);
				printf("\n");
				continue;
			}

			if(!(hdr.attr&FIDO_PRIVATE) && (scfg.sub[cfg.area[i].sub]->misc&SUB_PONLY))
				hdr.attr|=MSG_PRIVATE;

			/**********************/
			/* Importing EchoMail */
			/**********************/
			result=fmsgtosmsg(fmsgbuf, &hdr, 0, cfg.area[i].sub);

			if(result==IMPORT_FILTERED_DUPE) {
				lprintf(LOG_NOTICE, "%s Duplicate message from %s (%s) to %s, subject: %s"
					,areatag, hdr.from, fmsghdr_srcaddr_str(&hdr), hdr.to, hdr.subj);
				cfg.area[i].dupes++;
				new_echostat_msg(stat, ECHOSTAT_MSG_DUPLICATE
					,fidomsg_to_echostat_msg(&hdr, &pkt_orig, fmsgbuf));
			}
			else if(result==IMPORT_SUCCESS || (result==IMPORT_FILTERED_AGE && cfg.relay_filtered_msgs)) {	   /* Not a dupe */
				strip_psb(fmsgbuf);
				write_to_pkts(fmsgbuf, curarea, NULL, &pkt_orig, &hdr, msg_seen, msg_path, /* rescan: */false);
			}

			if(result==SMB_SUCCESS) {		/* Successful import */
				user_t user;
				if(i!=cfg.badecho && cfg.echomail_notify && (user.number=lookup_user(&scfg, &user_list, hdr.to))!=0
					&& getuserdat(&scfg, &user)==0
					&& can_user_read_sub(&scfg, cfg.area[i].sub, &user, NULL)) {
					sprintf(str
						,"\7\1n\1hSBBSecho: \1m%.*s \1n\1msent you EchoMail on \1h%s \1n\1m%s\1n\r\n"
						,FIDO_NAME_LEN-1
						,hdr.from
						,scfg.grp[scfg.sub[cfg.area[i].sub]->grp]->sname
						,scfg.sub[cfg.area[i].sub]->sname);
					putsmsg(&scfg, user.number, str);
				}
				echomail++;
				cfg.area[i].imported++;
				new_echostat_msg(stat, ECHOSTAT_MSG_IMPORTED
					,fidomsg_to_echostat_msg(&hdr, &pkt_orig, fmsgbuf));
			}
			printf("\n");
		}
		fclose(fidomsg);
		FREE_AND_NULL(fmsgbuf);

		if(bad_packet)
			rename_bad_packet(packet);
		else {
			packets_imported++;
			if(cfg.delete_packets)
				delfile(packet, __LINE__);
		}
	}
	globfree(&g);
}

void check_free_diskspace(const char* path)
{
	if(cfg.min_free_diskspace && isdir(path)) {
		ulong freek = getfreediskspace(path, 1024);

		if(freek < cfg.min_free_diskspace / 1024) {
			lprintf(LOG_ERR, "!Insufficient free disk space (%luK < %"PRId64"K bytes) in %s\n"
				, freek, cfg.min_free_diskspace / 1024, path);
			bail(1);
		}
	}
}

/***********************************/
/* Synchronet/FidoNet Message util */
/***********************************/
int main(int argc, char **argv)
{
	FILE*	fidomsg;
	char	str[1025],path[MAX_PATH+1]
			,*p,*tp;
	char*	sub_code = NULL;
	char	tmp_code[LEN_EXTCODE+1];
	char	area_tag[FIDO_AREATAG_LEN+1];
	char	cmdline[256];
	int 	i,j,fmsg;
	size_t	f;
	glob_t	g;
	FILE	*stream;
	fidoaddr_t	nodeaddr = {0};
	nodecfg_t* nodecfg = NULL;
	char *usage="\n"
	"usage: sbbsecho [cfg_file] [-options] [sub_code] [address]\n"
	"\n"
	"where: cfg_file is the filename of config file (default is ctrl/sbbsecho.ini)\n"
	"       sub_code is the internal code for a sub-board (default is ALL subs)\n"
	"       address is the FTN node/link to export for (default is ALL links)\n"
	"\n"
	"sbbsecho, by default, will:\n\n"
	" * Process packets (*.pkt) from all inbound directories (-p to disable)\n"
	" * Process netmail (*.msg) files and import netmail messages (-n to disable)\n"
	" * Delete netmail messages/files after importing them (-d to disable)\n"
	" * Import and forward packetized echomail messages (-i to disable)\n"
	" * Export local netmail messages from SMB to *.msg (-c to disable)\n"
	" * Export echomail messages from selected and linked sub(s) (-e to disable)\n"
	" * Load echomail export pointers before exporting (-m to disable)\n"
	" * Store echomail export pointers after exporting (-t to disable)\n"
	" * Packetize outbound netmail as req'd for BSO/FLO operation (-q to disable)\n"
	"\n"
	"sbbsecho, by default, will NOT:\n\n"
	" * Export echomail previously imported from FTN (-h to enable)\n"
	" * Update echomail export pointers without exporting messages (-u to enable)\n"
	" * Import echomail (re-toss) from the 'bad echo' sub-board (-r to enable)\n"
	" * Generate AreaFix netmail reports/notifications for links (-g to enable)\n"
	" * Display the parsed area file (e.g. AREAS.BBS) for debugging (-a to enable)\n"
	" * Prompt for key-press upon exit (-@ to enable)\n"
	" * Prompt for key-press upon abnormal exit (-w to enable)\n"
	;

	locked_bso_nodes = strListInit();

	if((email=(smb_t *)malloc(sizeof(smb_t)))==NULL) {
		printf("ERROR allocating memory for email.\n");
		bail(1);
		return -1;
	}
	memset(email,0,sizeof(smb_t));
	if((smb=(smb_t *)malloc(MAX_OPEN_SMBS*sizeof(smb_t)))==NULL) {
		printf("ERROR allocating memory for smbs.\n");
		bail(1);
		return -1;
	}
	for(i=0;i<MAX_OPEN_SMBS;i++)
		memset(&smb[i],0,sizeof(smb_t));
	memset(&cfg,0,sizeof(cfg));

	DESCRIBE_COMPILER(compiler);

	printf("\nSBBSecho v%u.%02u-%s (%s/%s) - Synchronet FidoNet EchoMail Tosser\n"
		,SBBSECHO_VERSION_MAJOR, SBBSECHO_VERSION_MINOR
		,PLATFORM_DESC
		,GIT_BRANCH, GIT_HASH
		);

	cmdline[0]=0;
	for(i=1;i<argc;i++) {
		SAFECAT(cmdline, argv[i]);
		SAFECAT(cmdline, " ");
		if(argv[i][0]=='-'
#if !defined(__unix__)
			|| argv[i][0]=='/'
#endif
			) {
			j=1;
			while(argv[i][j]) {
				switch(toupper(argv[i][j])) {
					case 'C':
						opt_export_netmail = false;
						break;
					case 'D':
						opt_delete_netmail = false;
						break;
					case 'E':
						opt_export_echomail = false;
						break;
					case 'Q':
						opt_pack_netmail = false;
						break;
					case 'G':
						opt_gen_notify_list = true;
						break;
					case 'H':
						opt_export_ftn_echomail = true;
						break;
					case 'I':
						opt_import_echomail = false;
						break;
					case 'M':
						opt_ignore_msgptrs = true;
						break;
					case 'N':
						opt_import_netmail = false;
						break;
					case 'P':
						opt_import_packets = false;
						break;
					case 'T':
						opt_leave_msgptrs = true;
						break;
					case 'U':
						opt_update_msgptrs = true;
						opt_export_echomail = false;
						break;
					case '@':
						pause_on_exit=true;
						break;
					case 'W':
						pause_on_abend=true;
						break;
					case 'A':
						opt_dump_area_file=true;
						break;
					case 'R':
						opt_retoss_badmail=true;
						break;
					default:
						fprintf(stderr, "Unsupported option: %c\n", argv[i][j]);
						/* Fall-through */
					case '?':
						printf("%s", usage);
						bail(0);
					case 'B':
					case 'F':
					case 'J':
					case 'L':
					case 'O':
					case 'S':
					case 'X':
					case 'Y':
					case '!':
						/* ignored (legacy) */
						break;
				}
				j++;
			}
		}
		else {
			if(strchr(argv[i],'.')!=NULL && fexist(argv[i]))
				SAFECOPY(cfg.cfgfile,argv[i]);
			else if(IS_DIGIT(argv[i][0]))
				nodeaddr = atofaddr(argv[i]);
			else
				sub_code = argv[i];
		}
	}

	if(!opt_import_echomail && !opt_import_netmail)
		opt_import_packets = false;

	SAFECOPY(scfg.ctrl_dir, get_ctrl_dir(/* warn: */true));

	backslash(scfg.ctrl_dir);

	/* Install Ctrl-C/Break signal handler here */
#if defined(_WIN32)
	SetConsoleCtrlHandler(ControlHandler, TRUE /* Add */);
#elif defined(__unix__)
	signal(SIGQUIT,break_handler);
	signal(SIGINT,break_handler);
	signal(SIGTERM,break_handler);
#endif


	if(chdir(scfg.ctrl_dir)!=0)
		printf("!ERROR changing directory to: %s", scfg.ctrl_dir);

    printf("\nLoading configuration files from %s\n", scfg.ctrl_dir);
	scfg.size=sizeof(scfg);
	SAFECOPY(str,UNKNOWN_LOAD_ERROR);
	if(!load_cfg(&scfg, /* text: */NULL, /* prep: */true, /* node: */false, str, sizeof(str))) {
		fprintf(stderr,"!ERROR %s\n",str);
		fprintf(stderr,"!Failed to load configuration files\n");
		bail(1);
		return -1;
	}

	SAFEPRINTF(str,"%stwitlist.cfg",scfg.ctrl_dir);
	twit_list=findstr_list(str);

	if(scfg.total_faddrs)
		sys_faddr=scfg.faddr[0];

	if(!cfg.cfgfile[0])
		SAFEPRINTF(cfg.cfgfile,"%ssbbsecho.ini",scfg.ctrl_dir);

	if(!sbbsecho_read_ini(&cfg)) {
		fprintf(stderr, "ERROR %d (%s) reading %s\n", errno, strerror(errno), cfg.cfgfile);
		bail(1);
	}

	if(nodeaddr.zone && (nodecfg = findnodecfg(&cfg, nodeaddr, /* exact: */true)) == NULL) {
		fprintf(stderr, "Invalid node address: %s\n", argv[i]);
		bail(1);
	}

#if defined(__unix__)
	umask(cfg.umask);
#endif

	if((fidologfile=fopen(cfg.logfile,"a"))==NULL) {
		fprintf(stderr,"ERROR %u (%s) line %d opening %s\n",errno,strerror(errno),__LINE__,cfg.logfile);
		bail(1);
		return -1;
	}

	for(uint u = 0; u < cfg.nodecfgs; u++) {
		if(sysfaddr_is_valid(find_sysfaddr(cfg.nodecfg[u].addr, true))) {
			lprintf(LOG_ERR, "Configuration ERROR: Linked node #%u is your own address: %s"
				,u + 1, faddrtoa(&cfg.nodecfg[u].addr));
			bail(1);
		}
	}

	char* tmpdir = FULLPATH(NULL, cfg.temp_dir, sizeof(cfg.temp_dir)-1);
	if(tmpdir != NULL) {
		SAFECOPY(cfg.temp_dir, tmpdir);
		free(tmpdir);
	}
	if(mkpath(cfg.temp_dir) != 0) {
		lprintf(LOG_ERR, "ERROR %d (%s) creating temp directory: %s", errno, strerror(errno), cfg.temp_dir);
		bail(1);
	}
	backslash(cfg.temp_dir);

	char* inbound = FULLPATH(NULL, cfg.inbound, sizeof(cfg.inbound)-1);
	if(inbound != NULL) {
		SAFECOPY(cfg.inbound, inbound);
		free(inbound);
	}
	backslash(cfg.inbound);

	if(cfg.secure_inbound[0]) {
		char* secure_inbound = FULLPATH(NULL, cfg.secure_inbound, sizeof(cfg.secure_inbound)-1);
		if(secure_inbound != NULL) {
			SAFECOPY(cfg.secure_inbound, secure_inbound);
			free(secure_inbound);
		}
		backslash(cfg.secure_inbound);
	}

	char* outbound = FULLPATH(NULL, cfg.outbound, sizeof(cfg.outbound)-1);
	if(outbound != NULL) {
		SAFECOPY(cfg.outbound, outbound);
		free(outbound);
	}

	check_free_diskspace(scfg.data_dir);
	check_free_diskspace(scfg.logs_dir);
	check_free_diskspace(scfg.netmail_dir);
	check_free_diskspace(cfg.outbound);
	check_free_diskspace(cfg.temp_dir);

	for(uint u=0; u<cfg.nodecfgs; u++) {
		if(cfg.nodecfg[u].inbox[0])
			backslash(cfg.nodecfg[u].inbox);
		if(cfg.use_outboxes && cfg.nodecfg[u].outbox[0])
			check_free_diskspace(cfg.nodecfg[u].outbox);
	}

	SAFEPRINTF(path,"%ssbbsecho.bsy", scfg.ctrl_dir);
	if(!fmutex(path, program_id(), cfg.bsy_timeout)) {
		lprintf(LOG_WARNING, "Mutex file exists (%s): SBBSecho appears to be already running", path);
		bail(1);
	}
	mtxfile_locked = true;
	atexit(cleanup);

	if(cfg.max_log_size && ftello(fidologfile) >= cfg.max_log_size) {
		lprintf(LOG_INFO, "Maximum log file size reached: %"PRId64" bytes", cfg.max_log_size);
		fclose(fidologfile);

		backup(cfg.logfile, cfg.max_logs_kept, /* rename: */true);
		if((fidologfile = fopen(cfg.logfile,"a")) == NULL) {
			fprintf(stderr,"ERROR %u (%s) line %d opening %s\n", errno, strerror(errno), __LINE__, cfg.logfile);
			bail(1);
			return -1;
		}
	}

	truncsp(cmdline);
	lprintf(LOG_DEBUG,"%s (PID %u) invoked with options: %s", sbbsecho_pid(), getpid(), cmdline);
	lprintf(LOG_DEBUG,"Configured: %u archivers, %u linked-nodes, %u echolists", cfg.arcdefs, cfg.nodecfgs, cfg.listcfgs);
	lprintf(LOG_DEBUG,"NetMail directory: %s", scfg.netmail_dir);
	lprintf(LOG_DEBUG,"Secure Inbound directory: %s", cfg.secure_inbound);
	lprintf(LOG_DEBUG,"Non-secure Inbound directory: %s", cfg.inbound);
	lprintf(LOG_DEBUG,"Outbound (BSO root) directory: %s", cfg.outbound);
	if(cfg.ignore_netmail_sent_attr && !cfg.delete_netmail)
		lprintf(LOG_WARNING, "Ignore NetMail 'Sent' Attribute is enabled with Delete NetMail disabled: Duplicate NetMail msgs may be sent!");

	/******* READ IN AREAS.BBS FILE *********/
	cfg.areas=0;		/* Total number of areas in AREAS.BBS */
	cfg.area=NULL;

	(void)fexistcase(cfg.areafile);
	if((stream = fopen(cfg.areafile,"r")) == NULL) {
		lprintf(LOG_NOTICE, "Could not open Area File (%s): %s", cfg.areafile, strerror(errno));
	} else {
		printf("Reading %s",cfg.areafile);
		while(!terminated) {
			if(!fgets(str,sizeof(str),stream))
				break;
			truncsp(str);
			p=str;
			SKIP_WHITESPACE(p);	/* Find first printable char */
			if(*p==';' || !*p)          /* Ignore blank lines or start with ; */
				continue;
			int areanum = cfg.areas;
			if(!new_area(/* tag: */NULL, /* Pass-through by default: */INVALID_SUB, /* link: */NULL)) {
				lprintf(LOG_ERR,"ERROR allocating memory for area #%u.",cfg.areas + 1);
				bail(1);
				return -1;
			}
			sprintf(tmp_code,"%-.*s",LEN_EXTCODE,p);
			tp=tmp_code;
			FIND_WHITESPACE(tp);
			*tp=0;
			for(i=0;i<scfg.total_subs;i++)
				if(!stricmp(tmp_code,scfg.sub[i]->code))
					break;
			if(i<scfg.total_subs)
				cfg.area[areanum].sub = i;
			else if(stricmp(tmp_code,"P")) {
				printf("\n");
				lprintf(LOG_WARNING,"%s: Unrecognized internal code, assumed passthru",tmp_code);
			}

			FIND_WHITESPACE(p);				/* Skip code */
			SKIP_WHITESPACE(p);				/* Skip white space */
			SAFECOPY(area_tag,p);		       /* Area tag */
			truncstr(area_tag,"\t ");
			strupr(area_tag);
			if(area_tag[0]=='*')         /* UNKNOWN-ECHO area */
				cfg.badecho=areanum;
			if(areanum > 0 && area_is_valid(find_area(area_tag))) {
				printf("\n");
				lprintf(LOG_WARNING, "DUPLICATE AREA (%s) in area file (%s), IGNORED!", area_tag, cfg.areafile);
				cfg.areas--;
				continue;
			}
			if((cfg.area[areanum].tag=strdup(area_tag))==NULL) {
				printf("\n");
				lprintf(LOG_ERR,"ERROR allocating memory for area #%u tag."
					,areanum+1);
				bail(1);
				return -1;
			}

			FIND_WHITESPACE(p);		/* Skip tag */
			SKIP_WHITESPACE(p);		/* Skip white space */

			while(*p && *p!=';') {
				if(!IS_DIGIT(*p)) {
					printf("\n");
					lprintf(LOG_WARNING, "Invalid Area File line, expected link address(es) after echo-tag: '%s'", str);
					break;
				}
				if((cfg.area[areanum].link=(fidoaddr_t *)
					realloc(cfg.area[areanum].link
					,sizeof(fidoaddr_t)*(cfg.area[areanum].links+1)))==NULL) {
					printf("\n");
					lprintf(LOG_ERR,"ERROR allocating memory for area #%u links."
						,areanum+1);
					bail(1);
					return -1;
				}
				fidoaddr_t link = atofaddr(p);
				cfg.area[areanum].link[cfg.area[areanum].links] = link;
				if(findnodecfg(&cfg, link, /* exact: */cfg.require_linked_node_cfg) == NULL) {
					printf("\n");
					lprintf(LOG_WARNING, "Configuration for %s-linked-node (%s) not found in %s"
						,cfg.area[areanum].tag, faddrtoa(&link), cfg.cfgfile);
				} else
					cfg.area[areanum].links++;
				FIND_WHITESPACE(p);	/* Skip address */
				SKIP_WHITESPACE(p);	/* Skip white space */
			}

	#if 0
			if(cfg.area[areanum].sub!=INVALID_SUB || cfg.area[areanum].links)
				cfg.areas++;		/* Don't allocate if no tossing */
	#endif
		}
		fclose(stream);
		printf("\n");
		lprintf(LOG_DEBUG, "Read %u areas from %s", cfg.areas, cfg.areafile);
	}

	if(cfg.badecho >= 0)
		lprintf(LOG_DEBUG, "Bad-echo area: %s", scfg.sub[cfg.area[cfg.badecho].sub]->code);

	if(cfg.auto_add_subs) {
		for(unsigned subnum = 0; subnum < scfg.total_subs; subnum++) {
			if(cfg.badecho >=0 && cfg.area[cfg.badecho].sub == subnum)
				continue;	/* No need to auto-add the badecho sub */
			if(!(scfg.sub[subnum]->misc&SUB_FIDO))
				continue;
			unsigned area;
			for(area = 0; area < cfg.areas; area++)
				if(cfg.area[area].sub == subnum)
					break;
			if(area < cfg.areas) /* Sub-board already listed in area file */
				continue;
			unsigned hub;
			for(hub = 0; hub < cfg.nodecfgs; hub++)
				if(strListFind(cfg.nodecfg[hub].grphub, scfg.grp[scfg.sub[subnum]->grp]->sname
					,/* Case-sensitive: */false) >= 0)
					break;
			if(hub < cfg.nodecfgs)
				add_sub_to_arealist(scfg.sub[subnum], cfg.nodecfg[hub].addr);
		}
	}

	if(opt_dump_area_file) {
		printf("Area file dump (%u areas):\n", cfg.areas);
		for(unsigned u=0; u < cfg.areas && !terminated; u++) {
			printf("%-*s %-*s "
				,LEN_EXTCODE, cfg.area[u].sub == INVALID_SUB ? "P" : scfg.sub[cfg.area[u].sub]->code
				,FIDO_AREATAG_LEN, cfg.area[u].tag);
			for(unsigned l=0; l < cfg.area[u].links && !terminated; l++)
				printf("%s ", faddrtoa(&cfg.area[u].link[l]));
			printf("\n");
		}
	}

	if(cfg.badareafile[0]) {
		int i;
		int before, after;
		FILE* fp;
		printf("Reading bad area file: %s\n", cfg.badareafile);

		fp = fopen(cfg.badareafile,"r");
		bad_areas = strListReadFile(fp, NULL, 0);
		before = strListCount(bad_areas);
		lprintf(LOG_DEBUG, "Read %u areas from %s", before, cfg.badareafile);
		if(fp!=NULL)
			fclose(fp);
//		lprintf(LOG_DEBUG, "Checking bad areas for areas we now carry - begin");
		strListTruncateStrings(bad_areas, " \t\r\n");
		for(i=0; bad_areas[i] != NULL; i++) {
			if(area_is_valid(find_area(bad_areas[i]))) {			/* Do we carry this area? */
				lprintf(LOG_DEBUG, "Removing area '%s' from bad areas list (since it is now carried locally)", bad_areas[i]);
				free(strListRemove(&bad_areas, i));
				i--;
			}
		}
//		lprintf(LOG_DEBUG, "Checking bad areas for areas we now carry - end");
		after = strListCount(bad_areas);
		if(before != after)
			lprintf(LOG_NOTICE, "Removed %d areas from bad area file: %s", before-after, cfg.badareafile);
	}

	if(opt_gen_notify_list && !terminated) {
		lprintf(LOG_DEBUG,"Generating AreaFix Notifications...");
		gen_notify_list(nodecfg);
	}

	if(cfg.echostats[0])
		echostat_count = read_echostats(cfg.echostats, &echostat);

	if(opt_retoss_badmail)
		retoss_bad_echomail();

	find_stray_packets();

	if(opt_import_packets) {

		printf("\nScanning for Inbound Packets...\n");

		/* We want to loop while there are bundles waiting for us, but first we want */
		/* to take care of any packets that may already be hanging around for some	 */
		/* reason or another (thus the do/while loop) */

		for(uint u=0; u<cfg.nodecfgs && !terminated; u++) {
			if(cfg.nodecfg[u].inbox[0] == 0)
				continue;
			printf("Scanning %s inbox: %s\n", faddrtoa(&cfg.nodecfg[u].addr), cfg.nodecfg[u].inbox);
			do {
				import_packets(cfg.nodecfg[u].inbox, &cfg.nodecfg[u], /* secure: */true);
			} while(unpack_bundle(cfg.nodecfg[u].inbox));
		}
		if(cfg.secure_inbound[0] && !terminated) {
			printf("Scanning secure inbound: %s\n", cfg.secure_inbound);
			do {
				import_packets(cfg.secure_inbound, NULL, /* secure: */true);
			} while(unpack_bundle(cfg.secure_inbound));
		}
		if(cfg.inbound[0] && !terminated) {
			printf("Scanning non-secure inbound: %s\n", cfg.inbound);
			do {
				import_packets(cfg.inbound, NULL, /* secure: */false);
			} while(unpack_bundle(cfg.inbound));
		}

		for(j=MAX_OPEN_SMBS-1;(int)j>=0;j--)		/* Close open bases */
			if(smb[j].shd_fp)
				smb_close(&smb[j]);

		/******* END OF IMPORT PKT ROUTINE *******/

		for(uint u=0; u<cfg.areas; u++) {
			if(cfg.area[u].imported)
				lprintf(LOG_INFO,"Imported: %5u msgs %*s <- %s"
					,cfg.area[u].imported,LEN_EXTCODE,scfg.sub[cfg.area[u].sub]->code
					,cfg.area[u].tag);
		}
		for(uint u=0; u<cfg.areas; u++) {
			if(cfg.area[u].circular)
				lprintf(LOG_INFO,"Circular: %5u detected in %s"
					,cfg.area[u].circular, cfg.area[u].tag);
		}
		for(uint u=0; u<cfg.areas; u++) {
			if(cfg.area[u].dupes)
				lprintf(LOG_INFO,"Duplicate: %4u detected in %s"
					,cfg.area[u].dupes, cfg.area[u].tag);
		}

		if(echomail)
			lprintf(LOG_INFO,"Imported: %5lu msgs total", echomail);
	}

	if(opt_import_netmail && !terminated) {

		printf("\nScanning for Inbound NetMail Messages...\n");

#ifdef __unix__
		sprintf(str,"%s*.[Mm][Ss][Gg]",scfg.netmail_dir);
#else
		sprintf(str,"%s*.msg",scfg.netmail_dir);
#endif
		glob(str,0,NULL,&g);
		for(f=0; f<g.gl_pathc && !terminated; f++) {
			fmsghdr_t hdr;

			SAFECOPY(path,g.gl_pathv[f]);

			if((fidomsg=fnopen(&fmsg,path,O_RDWR))==NULL) {
				lprintf(LOG_ERR,"ERROR %u (%s) line %d opening %s",errno,strerror(errno),__LINE__,path);
				continue;
			}
			if(filelength(fmsg)<sizeof(fmsghdr_t)) {
				lprintf(LOG_ERR,"ERROR line %d invalid length of %lu bytes for %s",__LINE__
					,filelength(fmsg),path);
				fclose(fidomsg);
				continue;
			}
			if(!fread_fmsghdr(&hdr,fidomsg)) {
				fclose(fidomsg);
				lprintf(LOG_ERR,"ERROR line %d reading fido msghdr from %s",__LINE__,path);
				continue;
			}
			i=import_netmail(path, &hdr, fidomsg, cfg.inbound);
			/**************************************/
			/* Delete source netmail if specified */
			/**************************************/
			if(i==0) {
				if(cfg.delete_netmail && opt_delete_netmail) {
					fclose(fidomsg);
					delfile(path, __LINE__);
				}
				else {
					hdr.attr|=FIDO_RECV;
					(void)fseek(fidomsg,0L,SEEK_SET);
					(void)fwrite(&hdr,sizeof(fmsghdr_t),1,fidomsg);
					fclose(fidomsg);
				}
			}
			else if(i!=-2)
				fclose(fidomsg);
			printf("\n");
		}
		globfree(&g);
	}

	if(opt_export_echomail && !terminated)
		export_echomail(sub_code, nodecfg, /* rescan: */opt_ignore_msgptrs);

	if(opt_export_netmail && !terminated)
		export_netmail();

	if(opt_pack_netmail && !terminated)
		pack_netmail();

	if(opt_update_msgptrs) {

		lprintf(LOG_DEBUG,"Updating Message Pointers to Last Posted Message...");

		for(uint u=0; u<cfg.areas; u++) {
			uint32_t lastmsg;
			if(u==cfg.badecho)	/* Don't scan the bad-echo area */
				continue;
			i=cfg.area[u].sub;
			if(i<0 || i>=scfg.total_subs)	/* Don't scan pass-through areas */
				continue;
			lprintf(LOG_DEBUG,"\n%-*.*s -> %s"
				,LEN_EXTCODE, LEN_EXTCODE, scfg.sub[i]->code, cfg.area[u].tag);
			if(getlastmsg(i,&lastmsg,0) >= 0)
				write_export_ptr(i, lastmsg, cfg.area[u].tag);
		}
	}

	move_echomail_packets();
	if(nodecfg != NULL)
		attachment(NULL,nodecfg->addr,ATTACHMENT_NETMAIL);

	if(cfg.echostats[0])
		write_echostats(cfg.echostats, echostat, echostat_count);

	if(email->shd_fp)
		smb_close(email);

	free(smb);
	free(email);

	if(cfg.outgoing_sem[0]) {
		if (exported_netmail || exported_echomail || packed_netmail || packets_sent || bundles_sent) {
			lprintf(LOG_DEBUG, "Touching outgoing semfile: %s", cfg.outgoing_sem);
			if(!ftouch(cfg.outgoing_sem))
				lprintf(LOG_ERR, "Error %d (%s) touching outgoing semfile: %s", errno, strerror(errno), cfg.outgoing_sem);
		}
	}
	for(unsigned u = 0; u < cfg.robot_count; u++) {
		if(cfg.robot_list[u].semfile[0] && cfg.robot_list[u].recv_count) {
			lprintf(LOG_DEBUG, "Touching robot semfile: %s", cfg.robot_list[u].semfile);
			if(!ftouch(cfg.robot_list[u].semfile))
				lprintf(LOG_ERR, "Error %d (%s) touching robot semfile: %s", errno, strerror(errno), cfg.robot_list[u].semfile);
		}
	}
	bail(0);
	return(0);
}
