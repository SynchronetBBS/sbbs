/* sbbsecho.c */

/* Synchronet FidoNet EchoMail Scanning/Tossing and NetMail Tossing Utility */

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

#include "conwrap.h"		/* getch() */
#include "sbbs.h"			/* load_cfg() */
#include "sbbsdefs.h"
#include "smblib.h"
#include "scfglib.h"
#include "lzh.h"
#include "sbbsecho.h"
#include "genwrap.h"		/* PLATFORM_DESC */
#include "xpendian.h"

#define MAX_OPEN_SMBS	10

smb_t *smb,*email;
bool opt_import_packets		= true;
bool opt_import_netmail		= true;
bool opt_import_echomail	= true;
bool opt_export_echomail	= true;
bool opt_export_netmail		= true;
bool opt_pack_netmail		= true;
bool opt_gen_notify_list	= false;
bool opt_export_ftn_echomail= false;	/* Export msgs previously imported from FidoNet */
bool opt_update_msgptrs		= false;
bool opt_ignore_msgptrs		= false;
bool opt_leave_msgptrs		= false;

/* statistics */
ulong netmail=0;	/* imported */
ulong echomail=0;	/* imported */
ulong exported_netmail=0;
ulong exported_echomail=0;
ulong packed_netmail=0;

char tmp[256];
int cur_smb=0;
FILE *fidologfile=NULL;
bool twit_list;

fidoaddr_t		sys_faddr = {1,1,1,0};		/* Default system address: 1:1/1.0 */
sbbsecho_cfg_t	cfg;
scfg_t			scfg;
char			revision[16];
char			compiler[32];

bool pause_on_exit=false;
bool pause_on_abend=false;
bool mtxfile_locked=false;
bool terminated=false;

str_list_t	locked_bso_nodes;

int mv(const char *insrc, const char *indest, bool copy);
void export_echomail(const char *sub_code, const nodecfg_t*, bool rescan);

const char default_domain[] = "fidonet";

const char* zone_domain(uint16_t zone)
{
	struct zone_mapping *i;

	if (!cfg.use_ftn_domains)
		return default_domain;

	for (i=cfg.zone_map; i; i=i->next)
		if (i->zone == zone)
			return i->domain;

	return default_domain;
}

const char* zone_root_outbound(uint16_t zone)
{
	struct zone_mapping *i;

	if (!cfg.use_ftn_domains)
		return cfg.outbound;

	for (i=cfg.zone_map; i; i=i->next)
		if (i->zone == zone)
			return i->root;

	return cfg.outbound;
}

/* FTN-compliant "Program Identifier"/PID (also used as a "Tosser Identifier"/TID) */
const char* sbbsecho_pid(void)
{
	static char str[256];
	
	sprintf(str, "SBBSecho %u.%02u-%s r%s %s %s"
		,SBBSECHO_VERSION_MAJOR,SBBSECHO_VERSION_MINOR,PLATFORM_DESC,revision,__DATE__,compiler);

	return str;
}

/* for *.bsy file contents: */
const char* program_id(void)
{
	static char str[256];
	
	SAFEPRINTF2(str, "%u %s", getpid(), sbbsecho_pid());

	return str;
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
		strip_ctrl(sbuf, sbuf);
		if((tm = localtime(&now)) == NULL)
			tm = &tmbuf;
		fprintf(fidologfile,"%u-%02u-%02u %02u:%02u:%02u %s\n"
			,1900+tm->tm_year, tm->tm_mon+1, tm->tm_mday
			,tm->tm_hour, tm->tm_min, tm->tm_sec
			,sbuf);
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

/*****************************************************************************/
/* Returns command line generated from instr with %c replacments             */
/*****************************************************************************/
char *mycmdstr(scfg_t* cfg, const char *instr, const char *fpath, const char *fspec)
{
    static char cmd[MAX_PATH+1];
    char str[256],str2[128];
    int i,j,len;

	len=strlen(instr);
	for(i=j=0;i<len && j<128;i++) {
		if(instr[i]=='%') {
			i++;
			cmd[j]=0;
			switch(toupper(instr[i])) {
				case 'F':   /* File path */
					strcat(cmd,fpath);
					break;
				case 'G':   /* Temp directory */
					if(cfg->temp_dir[0]!='\\' 
						&& cfg->temp_dir[0]!='/' 
						&& cfg->temp_dir[1]!=':') {
						strcpy(str,cfg->node_dir);
						strcat(str,cfg->temp_dir);
						if(FULLPATH(str2,str,40))
							strcpy(str,str2);
						backslash(str);
						strcat(cmd,str);}
					else
						strcat(cmd,cfg->temp_dir);
					break;
				case 'J':
					if(cfg->data_dir[0]!='\\' 
						&& cfg->data_dir[0]!='/' 
						&& cfg->data_dir[1]!=':') {
						strcpy(str,cfg->node_dir);
						strcat(str,cfg->data_dir);
						if(FULLPATH(str2,str,40))
							strcpy(str,str2);
						backslash(str);
						strcat(cmd,str); 
					}
					else
						strcat(cmd,cfg->data_dir);
					break;
				case 'K':
					if(cfg->ctrl_dir[0]!='\\' 
						&& cfg->ctrl_dir[0]!='/' 
						&& cfg->ctrl_dir[1]!=':') {
						strcpy(str,cfg->node_dir);
						strcat(str,cfg->ctrl_dir);
						if(FULLPATH(str2,str,40))
							strcpy(str,str2);
						backslash(str);
						strcat(cmd,str); 
					}
					else
						strcat(cmd,cfg->ctrl_dir);
					break;
				case 'N':   /* Node Directory (same as SBBSNODE environment var) */
					strcat(cmd,cfg->node_dir);
					break;
				case 'O':   /* SysOp */
					strcat(cmd,cfg->sys_op);
					break;
				case 'Q':   /* QWK ID */
					strcat(cmd,cfg->sys_id);
					break;
				case 'S':   /* File Spec */
					strcat(cmd,fspec);
					break;
				case '!':   /* EXEC Directory */
					strcat(cmd,cfg->exec_dir);
					break;
                case '@':   /* EXEC Directory for DOS/OS2/Win32, blank for Unix */
#ifndef __unix__
                    strcat(cmd,cfg->exec_dir);
#endif
                    break;
				case '#':   /* Node number (same as SBBSNNUM environment var) */
					sprintf(str,"%d",cfg->node_num);
					strcat(cmd,str);
					break;
				case '*':
					sprintf(str,"%03d",cfg->node_num);
					strcat(cmd,str);
					break;
				case '%':   /* %% for percent sign */
					strcat(cmd,"%");
					break;
				case '.':	/* .exe for DOS/OS2/Win32, blank for Unix */
#ifndef __unix__
					strcat(cmd,".exe");
#endif
					break;
				case '?':	/* Platform */
					strcpy(str,PLATFORM_DESC);
					strlwr(str);
					strcat(cmd,str);
					break;
				default:    /* unknown specification */
					lprintf(LOG_ERR,"ERROR Checking Command Line '%s'",instr);
					bail(1);
					break; 
			}
			j=strlen(cmd); 
		}
		else
			cmd[j++]=instr[i]; 
}
	cmd[j]=0;

	return(cmd);
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
 Returns the system address with the same zone as the address passed
******************************************************************************/
fidoaddr_t getsysfaddr(short zone)
{
	int i;

	for(i=0;i<scfg.total_faddrs;i++)
		if(scfg.faddr[i].zone==zone)
			return(scfg.faddr[i]);
	return(sys_faddr);
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
			*lastchar(outbound) = 0;
			safe_snprintf(outbound+strlen(outbound), maxlen,".%03x", dest.zone);
		}
		if(dest.point != 0) {			/* Point destination is "OUTBOUND[.ZZZ]/NNNNnnnn.pnt/" */
			backslash(outbound);
			safe_snprintf(outbound+strlen(outbound), maxlen, "%04x%04x.pnt", dest.net, dest.node);
		}
	}
	backslash(outbound);
	if(!isdir(outbound))
		lprintf(LOG_DEBUG, "Creating outbound directory for %s: %s", smb_faddrtoa(&dest, NULL), outbound);
	return mkpath(outbound);
}

const char* get_current_outbound(fidoaddr_t dest, bool fileboxes)
{
	static char outbound[MAX_PATH+1];
	get_outbound(dest, outbound, sizeof(outbound)-1, fileboxes);
	return outbound;
}

bool bso_lock_node(fidoaddr_t dest)
{
	const char* outbound;
	char fname[MAX_PATH+1];

	if(!cfg.flo_mailer)
		return true;

	outbound = get_current_outbound(dest, /* fileboxes: */false);

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
	}
	strListPush(&locked_bso_nodes, fname);
	lprintf(LOG_DEBUG, "Node (%s) successfully locked via: %s", smb_faddrtoa(&dest, NULL), fname);
	return true;
}

const char* bso_flo_filename(fidoaddr_t dest)
{
	nodecfg_t* nodecfg;
	char ch='f';
	const char* outbound;
	static char filename[MAX_PATH+1];

	if((nodecfg=findnodecfg(&cfg, dest, /* exact: */false)) != NULL) {
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

	if(dest.point)
		SAFEPRINTF3(filename,"%s%08x.%clo",outbound,dest.point,ch);
	else
		SAFEPRINTF4(filename,"%s%04x%04x.%clo",outbound,dest.net,dest.node,ch);
	return filename;
}

bool bso_file_attached_to_flo(const char* attachment, fidoaddr_t dest, bool bundle)
{
	char searchstr[MAX_PATH+1];
	SAFEPRINTF2(searchstr,"%c%s",(bundle && cfg.trunc_bundles) ? '#':'^', attachment);
	return findstr(searchstr, bso_flo_filename(dest));
}

/******************************************************************************
 This function creates or appends on existing Binkley compatible .?LO file
 attach file.
 Returns 0 on success.
******************************************************************************/
int write_flofile(const char *infile, fidoaddr_t dest, bool bundle, bool use_outbox)
{
	const char* flo_filename;
	char attachment[MAX_PATH+1];
	char searchstr[MAX_PATH+1];
	FILE *fp;
	nodecfg_t* nodecfg;

	if(use_outbox && (nodecfg=findnodecfg(&cfg, dest, /* exact: */false)) != NULL) {
		if(nodecfg->outbox[0])
			return 0;
	}

	if(!bso_lock_node(dest))
		return 1;

	flo_filename = bso_flo_filename(dest);
	SAFECOPY(attachment, infile);
	if(!fexistcase(attachment))	{ /* just in-case it's the wrong case for a Unix file system */
		lprintf(LOG_ERR, "ERROR line %u, attachment file not found: %s", __LINE__, attachment);
		return -1;
	}
	SAFEPRINTF2(searchstr,"%c%s",bundle && (cfg.trunc_bundles) ? '#':'^', attachment);
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

/* Writes text buffer to file, expanding sole LFs to CRLFs */
size_t fwrite_crlf(const char* buf, size_t len, FILE* fp)
{
	char	ch,last_ch=0;
	size_t	i;
	size_t	wr=0;	/* total chars written (may be > len) */

	for(i=0;i<len;i++) {
		ch=*buf++;
		if(ch=='\n' && last_ch!='\r') {
			if(fputc('\r',fp)==EOF)
				return(wr);
			wr++;
		}
		if(fputc(ch,fp)==EOF)
			return(wr);
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

bool parse_pkthdr(const fpkthdr_t* hdr, fidoaddr_t* orig_addr, fidoaddr_t* dest_addr, enum pkt_type* pkt_type)
{
	fidoaddr_t	orig;
	fidoaddr_t	dest;
	enum pkt_type type = PKT_TYPE_2_0;

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

	if(hdr->type2plus.cword == BYTE_SWAP_16(hdr->type2plus.cwcopy)  /* 2+ Packet Header (FSC-39) */
		&& (hdr->type2plus.cword&1)) {
		type = PKT_TYPE_2_PLUS;
		dest.point = hdr->type2plus.destpoint;
		if(hdr->type2plus.origpoint!=0 && orig.net == 0xffff) {	/* see FSC-0048 for details */
			orig.net = hdr->type2plus.auxnet;
			orig.point = hdr->type2plus.origpoint;
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
	enum pkt_type pkt_type = PKT_TYPE_2_0;
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

	if(pkt_type == PKT_TYPE_2_0)
		return true;

	if(pkt_type == PKT_TYPE_2_2) {
		hdr->type2_2.subversion = 2;	/* 2.2 */
		strncpy((char*)hdr->type2_2.origdomn,zone_domain(orig.zone),sizeof(hdr->type2_2.origdomn));
		strncpy((char*)hdr->type2_2.destdomn,zone_domain(dest.zone),sizeof(hdr->type2_2.destdomn));
		return true;
	}
	
	/* 2+ */
	if(pkt_type != PKT_TYPE_2_PLUS) {
		lprintf(LOG_ERR, "UNSUPPORTED PACKET TYPE: %u", pkt_type);
		return false;
	}

	if(orig.point != 0) {
		hdr->type2plus.orignet	= 0xffff;
		hdr->type2plus.auxnet	= orig.net; 
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
 This function will create a netmail message (.MSG format).
 If file is non-zero, will set file attachment bit (for bundles).
 Returns 0 on success.
******************************************************************************/
int create_netmail(const char *to, const smbmsg_t* msg, const char *subject, const char *body, fidoaddr_t dest
					,bool file_attached)
{
	FILE *fp;
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
	if(from==NULL)
		from="SBBSecho";
	if(to==NULL)
		to="Sysop";
	if(!startmsg) startmsg=1;
	if((nodecfg=findnodecfg(&cfg, dest, 0)) != NULL) {
		if(nodecfg->status == MAIL_STATUS_NORMAL && !nodecfg->direct)
			nodecfg=findnodecfg(&cfg, dest, /* skip exact match: */2);
	}

	MKDIR(scfg.netmail_dir);
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

	faddr=getsysfaddr(dest.zone);
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
	if(file_attached)
		hdr.attr|=FIDO_FILE;

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

	fwrite(&hdr,sizeof(fmsghdr_t),1,fp);
	fprintf(fp,"\1INTL %hu:%hu/%hu %hu:%hu/%hu\r"
		,hdr.destzone,hdr.destnet,hdr.destnode
		,hdr.origzone,hdr.orignet,hdr.orignode);

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
	/* Add FSC-53 FLAGS kludge */
	fprintf(fp,"\1FLAGS");
	if(direct)
		fprintf(fp," DIR");
	if(file_attached) {
		if(cfg.trunc_bundles)
			fprintf(fp," TFS");
		else
			fprintf(fp," KFS");
	}
	fprintf(fp,"\r");

	if(hdr.destpoint)
		fprintf(fp,"\1TOPT %hu\r",hdr.destpoint);
	if(hdr.origpoint)
		fprintf(fp,"\1FMPT %hu\r",hdr.origpoint);
	fprintf(fp,"\1PID: %s\r", (msg==NULL || msg->ftn_pid==NULL) ? sbbsecho_pid() : msg->ftn_pid);
	if(msg != NULL) {
		/* Unknown kludge lines are added here */
		for(int i=0; i<msg->total_hfields; i++)
			if(msg->hfield[i].type == FIDOCTRL)
				fprintf(fp,"\1%.512s\r",(char*)msg->hfield_dat[i]);
	}
	if(!file_attached || (!direct && file_attached))
		fwrite_crlf(body,strlen(body)+1,fp);	/* Write additional NULL */
	else
		fwrite("\0",1,1,fp);               /* Write NULL */
	lprintf(LOG_INFO, "Created NetMail (%s)%s from %s (%s) to %s (%s), attr: %04hX, subject: %s"
		,getfname(fname), file_attached ? " with attachment" : ""
		,from, smb_faddrtoa(&faddr, tmp), to, smb_faddrtoa(&dest, NULL), hdr.attr, subject);
	return fclose(fp);
}

/******************************************************************************
 This function takes the contents of 'infile' and puts it into a netmail
 message bound for addr.
******************************************************************************/
void file_to_netmail(FILE* infile, const char* title, fidoaddr_t dest, const char* to)
{
	char *buf,*p;
	long l,m,len;

	l=len=ftell(infile);
	if(len>8192L)
		len=8192L;
	rewind(infile);
	if((buf=(char *)malloc(len+1))==NULL) {
		lprintf(LOG_ERR,"ERROR line %d allocating %lu for file to netmail buf",__LINE__,len);
		return; 
	}
	while((m=fread(buf,1,(len>8064L) ? 8064L:len,infile))>0) {
		buf[m]=0;
		if(l>8064L && (p=strrchr(buf,'\n'))!=NULL) {
			p++;
			if(*p) {
				*p=0;
				p++;
				fseek(infile,-1L,SEEK_CUR);
				while(*p) { 			/* Seek back to end of last line */
					p++;
					fseek(infile,-1L,SEEK_CUR); 
				} 
			} 
		}
		if(ftell(infile)<l)
			strcat(buf,"\r\nContinued in next message...\r\n");
		create_netmail(to, /* msg: */NULL, title, buf, dest, /* attachment: */false); 
	}
	free(buf);
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

/******************************************************************************
 This function sends a notify list to applicable nodes, this list includes the
 settings configured for the node, as well as a list of areas the node is
 connected to.
******************************************************************************/
void gen_notify_list(void)
{
	FILE *	tmpf;
	char	str[256];
	uint	i,k;

	for(k=0;k<cfg.nodecfgs && !terminated;k++) {

		if(!cfg.nodecfg[k].send_notify)
			continue;

		if((tmpf=tmpfile())==NULL) {
			lprintf(LOG_ERR,"ERROR line %d couldn't open tmpfile",__LINE__);
			return; 
		}

		fprintf(tmpf,"Following are the options set for your system and a list "
			"of areas\r\nyou are connected to.  Please make sure everything "
			"is correct.\r\n\r\n");
		fprintf(tmpf,"Packet Type       %s\r\n", pktTypeStringList[cfg.nodecfg[k].pkt_type]);
		fprintf(tmpf,"Archive Type      %s\r\n"
			,cfg.nodecfg[k].archive == SBBSECHO_ARCHIVE_NONE ? "None" : cfg.nodecfg[k].archive->name);
		fprintf(tmpf,"Mail Status       %s\r\n", mailStatusStringList[cfg.nodecfg[k].status]);
		fprintf(tmpf,"Direct            %s\r\n", cfg.nodecfg[k].direct ? "Yes":"No");
		fprintf(tmpf,"Passive           %s\r\n", cfg.nodecfg[k].passive ? "Yes":"No");
		fprintf(tmpf,"Remote AreaMgr    %s\r\n\r\n"
			,cfg.nodecfg[k].password[0] ? "Yes" : "No");

		fprintf(tmpf,"Connected Areas\r\n---------------\r\n");
		for(i=0;i<cfg.areas;i++) {
			sprintf(str,"%s\r\n",cfg.area[i].name);
			if(str[0]=='*')
				continue;
			if(area_is_linked(i,&cfg.nodecfg[k].addr))
				fprintf(tmpf,"%s",str); 
		}

		if(ftell(tmpf))
			file_to_netmail(tmpf,"SBBSecho Notify List",cfg.nodecfg[k].addr, /* To: */NULL);
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
	unsigned k,x,y;
	unsigned u;
	str_list_t	area_list;

	if(type == AREALIST_ALL)
		strcpy(title,"List of Available Areas");
	else if(type == AREALIST_CONNECTED)
		strcpy(title,"List of Connected Areas");
	else
		strcpy(title,"List of Unlinked Areas");

	if((area_list=strListInit()) == NULL) {
		lprintf(LOG_ERR,"ERROR line %d couldn't allocate string list",__LINE__);
		return; 
	}

	/* Include relevant areas from the area file (e.g. areas.bbs): */
	for(u=0;u<cfg.areas;u++) {
		if((type == AREALIST_CONNECTED || cfg.add_from_echolists_only) && !area_is_linked(u,&addr))
			continue;
		if(type == AREALIST_UNLINKED && area_is_linked(u,&addr))
			continue;
		strListPush(&area_list, cfg.area[u].name); 
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
								for(y=0;y<cfg.areas;y++)
									if(!stricmp(cfg.area[y].name,p))
										break;
								if(y>=cfg.areas || !area_is_linked(y,&addr)) {
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
	if(!strListCount(area_list))
		create_netmail(to,/* msg: */NULL,title,"None.",addr,/* attachment: */false);
	else {
		FILE* fp;
		if((fp=tmpfile())==NULL) {
			lprintf(LOG_ERR,"ERROR line %d couldn't open tmpfile",__LINE__);
		} else {
			strListWriteFile(fp, area_list, "\r\n");
			file_to_netmail(fp,title,addr,to);
			fclose(fp);
		}
	}
	lprintf(LOG_INFO,"Created AreaFix response netmail with %s (%u areas)", title, strListCount(area_list));
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
	char str[1024],fields[1024],field1[256],field2[256],field3[256]
		,outpath[MAX_PATH+1]
		,*outname,*p,*tp,nomatch=0,match=0;
	unsigned j,k,x,y;
	unsigned u;
	ulong tagcrc;

	SAFECOPY(outpath,cfg.areafile);
	*getfname(outpath)=0;
	if((outname=tempnam(outpath,"AREAS"))==NULL) {
		lprintf(LOG_ERR,"ERROR tempnam(%s,AREAS)",outpath);
		return; 
	}
	if((nmfile=tmpfile())==NULL) {
		lprintf(LOG_ERR,"ERROR in tmpfile()");
		free(outname);
		return; 
	}
	if((afileout=fopen(outname,"w+"))==NULL) {
		lprintf(LOG_ERR,"ERROR %u (%s) line %d opening %s",errno,strerror(errno),__LINE__,outname);
		fclose(nmfile);
		free(outname);
		return; 
	}
	if((afilein=fopen(cfg.areafile,"r"))==NULL) {
		lprintf(LOG_ERR,"ERROR %u (%s) line %d opening %s",errno,strerror(errno),__LINE__,cfg.areafile);
		fclose(afileout);
		fclose(nmfile);
		free(outname);
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
		SAFECOPY(field1,p);         /* Internal Code Field */
		truncstr(field1," \t\r\n");
		FIND_WHITESPACE(p);
		SKIP_WHITESPACE(p);
		SAFECOPY(field2,p);         /* Areatag Field */
		truncstr(field2," \t\r\n");
		FIND_WHITESPACE(p);
		SKIP_WHITESPACE(p);
		if((tp=strchr(p,';'))!=NULL) {
			SAFECOPY(field3,p);     /* Comment Field (if any) */
			FIND_WHITESPACE(tp);
			*tp=0; 
		}
		else
			field3[0]=0;
		if(strListCount(del_area)) { 				/* Check for areas to remove */
			lprintf(LOG_DEBUG,"Removing areas for %s from %s", smb_faddrtoa(&addr,NULL), cfg.areafile);
			for(u=0;del_area[u]!=NULL;u++) {
				if(!stricmp(del_area[u],field2) ||
					!stricmp(del_area[0],"-ALL"))     /* Match Found */
					break; 
			}
			if(del_area[u]!=NULL) {
				for(u=0;u<cfg.areas;u++) {
					if(!stricmp(field2,cfg.area[u].name)) {
						lprintf(LOG_DEBUG,"Unlinking area (%s) for %s in %s", field2, smb_faddrtoa(&addr,NULL), cfg.areafile);
						if(!area_is_linked(u,&addr)) {
							fprintf(afileout,"%s\n",fields);
							/* bugfix here Mar-25-2004 (wasn't breaking for "-ALL") */
							if(stricmp(del_area[0],"-ALL"))
								fprintf(nmfile,"%s not connected.\r\n",field2);
							break; 
						}

						/* Added 12/4/95 to remove link from connected link */

						for(k=u;k<cfg.area[u].links-1;k++)
							memcpy(&cfg.area[u].link[k],&cfg.area[u].link[k+1]
								,sizeof(fidoaddr_t));
						--cfg.area[u].links;
						if(cfg.area[u].links==0) {
							FREE_AND_NULL(cfg.area[u].link);
						} else {
							if((cfg.area[u].link=(fidoaddr_t *)
								realloc(cfg.area[u].link,sizeof(fidoaddr_t)
								*(cfg.area[u].links)))==NULL) {
								lprintf(LOG_ERR,"ERROR line %d allocating memory for area "
									"#%u links.",__LINE__,u+1);
								bail(1); 
								return;
							}
						}

						fprintf(afileout,"%-16s%-23s ",field1,field2);
						for(j=0;j<cfg.area[u].links;j++) {
							if(!memcmp(&cfg.area[u].link[u],&addr
								,sizeof(fidoaddr_t)))
								continue;
							fprintf(afileout,"%s "
								,smb_faddrtoa(&cfg.area[u].link[u],NULL)); 
						}
						if(field3[0])
							fprintf(afileout,"%s",field3);
						fprintf(afileout,"\n");
						fprintf(nmfile,"%s removed.\r\n",field2);
						break; 
					} 
				}
				if(u==cfg.areas)			/* Something screwy going on */
					fprintf(afileout,"%s\n",fields);
				continue; 
			} 				/* Area match so continue on */
		}
		if(strListCount(add_area)) { 				/* Check for areas to add */
			lprintf(LOG_DEBUG,"Adding areas for %s to %s", smb_faddrtoa(&addr,NULL), cfg.areafile);
			for(u=0;add_area[u]!=NULL;u++)
				if(!stricmp(add_area[u],field2) ||
					!stricmp(add_area[0],"+ALL"))      /* Match Found */
					break;
			if(add_area[u]!=NULL) {
				if(stricmp(add_area[u],"+ALL"))
					add_area[u][0]=0;  /* So we can check other lists */
				for(u=0;u<cfg.areas;u++) {
					if(!stricmp(field2,cfg.area[u].name)) {
						lprintf(LOG_DEBUG,"Linking area (%s) for %s in %s", field2, smb_faddrtoa(&addr,NULL), cfg.areafile);
						if(area_is_linked(u,&addr)) {
							fprintf(afileout,"%s\n",fields);
							fprintf(nmfile,"%s already connected.\r\n",field2);
							break; 
						}
						if(cfg.add_from_echolists_only && !check_elists(field2,addr)) {
							fprintf(afileout,"%s\n",fields);
							break; 
						}

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

						fprintf(afileout,"%-16s%-23s ",field1,field2);
						for(j=0;j<cfg.area[u].links;j++)
							fprintf(afileout,"%s "
								,smb_faddrtoa(&cfg.area[u].link[u],NULL));
						if(field3[0])
							fprintf(afileout,"%s",field3);
						fprintf(afileout,"\n");
						fprintf(nmfile,"%s added.\r\n",field2);
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
	if(nomatch || (strListCount(add_area) && !stricmp(add_area[0],"+ALL"))) {
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
								if(add_area[0]!=NULL && stricmp(add_area[0],"+ALL")==0) {
									SAFECOPY(tmp,p);
									tagcrc=crc32(strupr(tmp),0);
									for(y=0;y<cfg.areas;y++)
										if(tagcrc==cfg.area[y].tag)
											break;
									if(y<cfg.areas)
										continue; 
								}
								for(y=0;add_area[y]!=NULL;y++)
									if((!stricmp(add_area[y],str) &&
										add_area[y][0]) ||
										!stricmp(add_area[0],"+ALL"))
										break;
								if(add_area[y]!=NULL) {
									fprintf(afileout,"%-16s%-23s","P",str);
									if(cfg.listcfg[j].hub.zone)
										fprintf(afileout," %s"
											,smb_faddrtoa(&cfg.listcfg[j].hub,NULL));
									fprintf(afileout," %s\n",smb_faddrtoa(&addr,NULL));
									fprintf(nmfile,"%s added.\r\n",str);
									if(stricmp(add_area[0],"+ALL"))
										add_area[y][0]=0;
									if(cfg.listcfg[j].forward
										&& cfg.listcfg[j].hub.zone)
										fprintf(fwdfile,"%s\r\n",str); 
								} 
							}
							fclose(afilein);
							if(cfg.listcfg[j].forward && ftell(fwdfile)>0)
								file_to_netmail(fwdfile,cfg.listcfg[j].password
									,cfg.listcfg[j].hub,/* To: */"Areafix");
							fclose(fwdfile);
							match=1;
							break; 
						}
					}
				} 
			} 
		} 
	}
	if(strListCount(add_area) && stricmp(add_area[0],"+ALL")) {
		for(u=0;add_area[u]!=NULL;u++)
			if(add_area[u][0])
				fprintf(nmfile,"%s not found.\r\n",add_area[u]); 
	}
	if(!ftell(nmfile))
		create_netmail(to,/* msg: */NULL,"Area Change Request","No changes made.",addr,/* attachment: */false);
	else
		file_to_netmail(nmfile,"Area Change Request",addr,to);
	fclose(nmfile);
	fclose(afileout);
	delfile(cfg.areafile, __LINE__);					/* Delete AREAS.BBS */
	if(rename(outname,cfg.areafile))		   /* Rename new AREAS.BBS file */
		lprintf(LOG_ERR,"ERROR line %d renaming %s to %s",__LINE__,outname,cfg.areafile);
	free(outname);
}
/******************************************************************************
 Used by AREAFIX to add/remove/change link info in the configuration file
******************************************************************************/
bool alter_config(fidoaddr_t addr, const char* key, const char* value)
{
	FILE* fp;

	if((fp=iniOpenFile(cfg.cfgfile, false)) == NULL) {
		lprintf(LOG_ERR, "ERROR %d (%s) opening %s for altering configuration of node %s"
			,errno, strerror(errno), cfg.cfgfile, smb_faddrtoa(&addr, NULL));
		return false;
	}
	str_list_t ini = iniReadFile(fp);
	if(ini == NULL) {
		lprintf(LOG_ERR, "ERROR reading %s for altering configuration of node %s"
			,cfg.cfgfile, smb_faddrtoa(&addr,NULL));
		iniCloseFile(fp);
		return false;
	}
	char section[128];
	SAFEPRINTF(section, "node:%s", smb_faddrtoa(&addr,NULL));
	ini_style_t style = { 0, "\t" };
	iniSetString(&ini, section, key, value, &style);
	iniWriteFile(fp, ini);
	iniCloseFile(fp);
	iniFreeStringList(ini);
	return true;
}

/******************************************************************************
 Used by AREAFIX to process any '%' commands that come in via netmail
******************************************************************************/
void command(char* instr, fidoaddr_t addr, const char* to)
{
	FILE *stream,*tmpf;
	char str[MAX_PATH+1],temp[256],*buf,*p;
	int  file;
	long l;
	unsigned u;

	nodecfg_t* nodecfg=findnodecfg(&cfg, addr,0);
	if(nodecfg == NULL)
		return;
	strupr(instr);
	if((p=strstr(instr,"HELP"))!=NULL) {
		sprintf(str,"%sAREAMGR.HLP",scfg.exec_dir);
		if(!fexistcase(str))
			return;
		if((stream=fnopen(&file,str,O_RDONLY))==NULL) {
			lprintf(LOG_ERR,"ERROR %u (%s) line %d opening %s",errno,strerror(errno),__LINE__,str);
			return; 
		}
		l=filelength(file);
		if((buf=(char *)malloc(l+1L))==NULL) {
			lprintf(LOG_CRIT,"ERROR line %d allocating %lu bytes for %s",__LINE__,l,str);
			return; 
		}
		fread(buf,l,1,stream);
		fclose(stream);
		buf[l]=0;
		create_netmail(to,/* msg: */NULL,"Area Manager Help",buf,addr,/* attachment: */false);
		free(buf);
		return; 
	}

	if((p=strstr(instr,"LIST"))!=NULL) {
		netmail_arealist(AREALIST_ALL,addr,to);
		return; 
	}

	if((p=strstr(instr,"QUERY"))!=NULL) {
		netmail_arealist(AREALIST_CONNECTED,addr,to);
		return; 
	}

	if((p=strstr(instr,"UNLINKED"))!=NULL) {
		netmail_arealist(AREALIST_UNLINKED,addr,to);
		return; 
	}

	if((p=strstr(instr,"COMPRESSION"))!=NULL) {
		FIND_WHITESPACE(p);
		SKIP_WHITESPACE(p);
		if(!stricmp(p,"NONE"))
			nodecfg->archive = SBBSECHO_ARCHIVE_NONE;
		else {
			for(u=0;u<cfg.arcdefs;u++)
				if(!stricmp(p,cfg.arcdef[u].name))
					break;
			if(u==cfg.arcdefs) {
				if((tmpf=tmpfile())==NULL) {
					lprintf(LOG_ERR,"ERROR line %d opening tmpfile()",__LINE__);
					return; 
				}
				fprintf(tmpf,"Compression type unavailable.\r\n\r\n"
					"Available types are:\r\n");
				for(u=0;u<cfg.arcdefs;u++)
					fprintf(tmpf,"                     %s\r\n",cfg.arcdef[u].name);
				file_to_netmail(tmpf,"Compression Type Change",addr,to);
				fclose(tmpf);
				return; 
			}
			nodecfg->archive = &cfg.arcdef[u];
		}
		alter_config(addr,"archive",p);
		SAFEPRINTF(str, "Compression type changed to %s.", p);
		create_netmail(to,/* msg: */NULL,"Compression Type Change",str,addr,/* attachment: */false);
		return; 
	}

	if((p=strstr(instr,"PASSWORD"))!=NULL) {
		FIND_WHITESPACE(p);
		SKIP_WHITESPACE(p);
		sprintf(temp,"%-.25s",p);
		p=temp;
		FIND_WHITESPACE(p);
		*p=0;
		if(!stricmp(temp,nodecfg->password)) {
			sprintf(str,"Your password was already set to %s."
				,nodecfg->password);
			create_netmail(to,/* msg: */NULL,"Password Change Request",str,addr,/* attachment: */false);
			return; 
		}
		if(alter_config(addr,"areafix_pwd", temp)) {
			SAFEPRINTF2(str,"Your password has been changed from %s to %.25s."
				,nodecfg->password,temp);
			SAFECOPY(nodecfg->password,temp);
		} else {
			SAFECOPY(str,"Error changing password");
		}
		create_netmail(to,/* msg: */NULL,"Password Change Request",str,addr,/* attachment: */false);
		return; 
	}

	if((p=strstr(instr,"RESCAN"))!=NULL) {
		export_echomail("", nodecfg, true);
		create_netmail(to,/* msg: */NULL,"Rescan Areas"
			,"All connected areas carried by your hub have been rescanned."
			,addr,/* attachment: */false);
		return; 
	}

	if((p=strstr(instr,"ACTIVE"))!=NULL) {
		if(!nodecfg->passive) {
			create_netmail(to,/* msg: */NULL,"Reconnect Disconnected Areas"
				,"Your areas are already connected.",addr,/* attachment: */false);
			return; 
		}
		nodecfg->passive = false;
		alter_config(addr,"passive","false");
		create_netmail(to,/* msg: */NULL,"Reconnect Disconnected Areas"
			,"Temporarily disconnected areas have been reconnected.",addr,/* attachment: */false);
		return; 
	}

	if((p=strstr(instr,"PASSIVE"))!=NULL) {
		if(nodecfg->passive) {
			create_netmail(to,/* msg: */NULL,"Temporarily Disconnect Areas"
				,"Your areas are already temporarily disconnected.",addr,/* attachment: */false);
			return; 
		}
		nodecfg->passive = true;
		alter_config(addr,"passive","true");
		create_netmail(to,/* msg: */NULL,"Temporarily Disconnect Areas"
			,"Your areas have been temporarily disconnected.",addr,/* attachment: */false);
		return; 
	}

	//if((p=strstr(instr,"FROM"))!=NULL);

	if((p=strstr(instr,"+ALL"))!=NULL) {
		str_list_t add_area=strListInit();
		strListPush(&add_area, instr);
		alter_areas(add_area,NULL,addr,to);
		strListFree(&add_area);
		return; 
	}

	if((p=strstr(instr,"-ALL"))!=NULL) {
		str_list_t del_area=strListInit();
		strListPush(&del_area, instr);
		alter_areas(NULL,del_area,addr,to);
		strListFree(&del_area);
		return; 
	}
}
/******************************************************************************
 This is where we're gonna process any netmail that comes in for areafix.
 Returns text for message body for the local sysop if necessary.
******************************************************************************/
char* process_areafix(fidoaddr_t addr, char* inbuf, const char* password, const char* to)
{
	static char body[512];
	char str[128];
	char *p,*tp,action,percent=0;
	ulong l,m;
	str_list_t add_area,del_area;

	lprintf(LOG_INFO,"Areafix Request received from %s"
			,smb_faddrtoa(&addr,NULL));
	
	p=(char *)inbuf;

	while(*p==CTRL_A) {			/* Skip kludge lines 11/05/95 */
		FIND_CHAR(p,'\r');
		if(*p) {
			p++;				/* Skip CR (required) */
			if(*p=='\n')
				p++;			/* Skip LF (optional) */
		}
	}

	if(((tp=strstr(p,"---\r"))!=NULL || (tp=strstr(p,"--- "))!=NULL) &&
		(*(tp-1)=='\r' || *(tp-1)=='\n'))
		*tp=0;

	if(!strnicmp(p,"%FROM",5)) {    /* Remote Remote Maintenance (must be first) */
		SAFECOPY(str,p+6);
		truncstr(str,"\r\n");
		lprintf(LOG_NOTICE,"Remote maintenance for %s requested via %s",str
			,smb_faddrtoa(&addr,NULL));
		addr=atofaddr(str); 
	}

	nodecfg_t* nodecfg = findnodecfg(&cfg, addr,0);
	if(nodecfg==NULL) {
		lprintf(LOG_NOTICE,"Areafix not configured for %s", smb_faddrtoa(&addr,NULL));
		create_netmail(to,/* msg: */NULL,"Areafix Request"
			,"Your node is not configured for Areafix, please contact your hub.\r\n",addr,/* attachment: */false);
		SAFEPRINTF(body,"An areafix request was made by node %s.\r\nThis node "
			"is not currently configured for areafix.\r\n"
			,smb_faddrtoa(&addr,NULL));
		lprintf(LOG_DEBUG,"areafix debug, nodes=%u",cfg.nodecfgs);
		{
			unsigned u;
			for(u=0;u<cfg.nodecfgs;u++)
				lprintf(LOG_DEBUG,smb_faddrtoa(&cfg.nodecfg[u].addr,NULL));
		}
		return(body); 
	}

	if(stricmp(nodecfg->password,password)) {
		create_netmail(to,/* msg: */NULL,"Areafix Request","Invalid Password.",addr,/* attachment: */false);
		sprintf(body,"Node %s attempted an areafix request using an invalid "
			"password.\r\nThe password attempted was %s.\r\nThe correct password "
			"for this node is %s.\r\n",smb_faddrtoa(&addr,NULL),password
			,(nodecfg->password[0]) ? nodecfg->password
			 : "[None Defined]");
		return(body); 
	}

	m=strlen(p);
	add_area=strListInit();
	del_area=strListInit();
	for(l=0;l<m;l++) { 
		while(*(p+l) && isspace((uchar)*(p+l))) l++;
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
				command(str,addr,to);
				percent++;
				break; 
		}

		while(*(p+l) && *(p+l)!='\r') l++; 
	}

	if(!percent && !strListCount(add_area) && !strListCount(del_area)) {
		create_netmail(to,/* msg: */NULL,"Areafix Request","No commands to process.",addr,/* attachment: */false);
		sprintf(body,"Node %s attempted an areafix request with an empty message "
			"body or with no valid commands.\r\n",smb_faddrtoa(&addr,NULL));
		strListFree(&add_area);
		strListFree(&del_area);
		return(body); 
	}
	if(strListCount(add_area) || strListCount(del_area))
		alter_areas(add_area,del_area,addr,to);
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
	char str[256],tmp[128];
	int ch,file;
	unsigned u,j;

	if((stream=fnopen(&file,infile,O_RDONLY))==NULL) {
		lprintf(LOG_ERR,"ERROR %u (%s) opening archive: %s",errno,strerror(errno),infile);
		bail(1); 
		return -1;
	}
	for(u=0;u<cfg.arcdefs;u++) {
		str[0]=0;
		fseek(stream,cfg.arcdef[u].byteloc,SEEK_SET);
		for(j=0;j<strlen(cfg.arcdef[u].hexid)/2;j++) {
			ch=fgetc(stream);
			if(ch==EOF) {
				u=cfg.arcdefs;
				break; 
			}
			sprintf(tmp,"%02X",ch);
			strcat(str,tmp); 
		}
		if(!stricmp(str,cfg.arcdef[u].hexid))
			break; 
	}
	fclose(stream);

	if(u==cfg.arcdefs) {
		lprintf(LOG_ERR, "ERROR determining type of archive: %s", infile);
		return(1); 
	}

	return execute(mycmdstr(&scfg,cfg.arcdef[u].unpack,infile, outdir));
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
	return execute(mycmdstr(&scfg, archive->pack, destfile, srcfile));
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
				return write_flofile(bundlename,dest,/* bundle: */true,/* use_outbox: */true);
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
			if(!fread(&attach,sizeof(attach_t),1,stream))
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
					,attach.dest,/* attachment: */false))
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
	fwrite(&attach,sizeof(attach_t),1,stream);
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
	}
	outbound = get_current_outbound(dest, /* fileboxes: */true);

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
				i=write_flofile(packet,dest,/* bundle: */true,/* use_outbox: */true);
			else
				i=create_netmail(/* To: */NULL,/* msg: */NULL,packet
					,(cfg.trunc_bundles) ? "\1FLAGS TFS\r" : "\1FLAGS KFS\r"
					,dest,/* attachment: */false);
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
				attachment(bundle,dest,ATTACHMENT_ADD);
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
	if(pack(packet,bundle,dest))	/* Won't get here unless all bundles are full */
		return false;
	if(attachment(bundle,dest,ATTACHMENT_ADD))
		return false;
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
			lprintf(LOG_INFO,"Unpacking bundle: %s (%1.1fKB)", fname, flength(fname)/1024.0);
			if(unpack(fname, inbound)) {	/* failure */
				lprintf(LOG_ERR,"!Unpack failure");
				if(fdate(fname)+(48L*60L*60L)<time(NULL)) {	
					/* If bundle file older than 48 hours, give up and rename
					   to "*.?_?" or (if it exists) "*.?-?" */
					SAFECOPY(str,fname);
					str[strlen(str)-2]='_';
					if(fexistcase(str))
						str[strlen(str)-2]='-';
					if(fexistcase(str))
						delfile(str, __LINE__);
					if(rename(fname,str))
						lprintf(LOG_ERR,"ERROR line %d renaming %s to %s"
							,__LINE__,fname,str); 
				}
				continue;
			}
			delfile(fname, __LINE__);	/* successful, so delete bundle */
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
	char buf[4096],str[256];
	char src[MAX_PATH+1];
	char dest[MAX_PATH+1];
	int  ind,outd;
	long length,chunk=4096,l;
    FILE *inp,*outp;

	SAFECOPY(src, insrc);
	SAFECOPY(dest, indest);
	if(!strcmp(src,dest))	/* source and destination are the same! */
		return(0);
//	lprintf(LOG_DEBUG, "%s file from %s to %s", copy ? "Copying":"Moving", src, dest);
	if(!fexistcase(src)) {
		lprintf(LOG_WARNING,"MV ERROR: Source doesn't exist '%s",src);
		return(-1); 
	}
	if(isdir(dest)) {
		backslash(dest);
		strcat(dest, getfname(src));
	}
	if(!copy && fexistcase(dest)) {
		lprintf(LOG_WARNING,"MV ERROR: Destination already exists '%s'",dest);
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
		lprintf(LOG_ERR,"MV ERROR %u (%s) opening %s",errno,strerror(errno),src);
		return(-1); 
	}
	if((inp=fdopen(ind,"rb"))==NULL) {
		close(ind);
		lprintf(LOG_ERR,"MV ERROR %u (%s) fdopening %s",errno,strerror(errno),str);
		return(-1); 
	}
	setvbuf(inp,NULL,_IOFBF,8*1024);
	if((outd=nopen(dest,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
		fclose(inp);
		lprintf(LOG_ERR,"MV ERROR %u (%s) opening %s",errno,strerror(errno),dest);
		return(-1); 
	}
	if((outp=fdopen(outd,"wb"))==NULL) {
		close(outd);
		fclose(inp);
		lprintf(LOG_ERR,"MV ERROR %u (%s) fdopening %s",errno,strerror(errno),str);
		return(-1); 
	}
	setvbuf(outp,NULL,_IOFBF,8*1024);
	length=filelength(ind);
	l=0L;
	while(l<length) {
		if(l+chunk>length)
			chunk=length-l;
		fread(buf,chunk,1,inp);
		fwrite(buf,chunk,1,outp);
		l+=chunk; 
	}
	fclose(inp);
	fclose(outp);
	if(!copy)
		delfile(src, __LINE__);
	return(0);
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


ulong loadmsgs(post_t** post, ulong ptr)
{
	int i;
	long l,total;
	idxrec_t idx;


	if((i=smb_locksmbhdr(&smb[cur_smb]))!=SMB_SUCCESS) {
		lprintf(LOG_ERR,"ERROR %d (%s) line %d locking %s",i,smb[cur_smb].last_error,__LINE__,smb[cur_smb].file);
		return(0); 
	}

	/* total msgs in sub */
	total=filelength(fileno(smb[cur_smb].sid_fp))/sizeof(idxrec_t);

	if(!total) {			/* empty */
		smb_unlocksmbhdr(&smb[cur_smb]);
		return(0); 
	}

	if(((*post)=(post_t*)malloc(sizeof(post_t)*total))    /* alloc for max */
		==NULL) {
		smb_unlocksmbhdr(&smb[cur_smb]);
		lprintf(LOG_ERR,"ERROR line %d allocating %lu bytes for %s",__LINE__
			,sizeof(post_t *)*total,smb[cur_smb].file);
		return(0); 
	}

	fseek(smb[cur_smb].sid_fp,0L,SEEK_SET);
	for(l=0;l<total && !feof(smb[cur_smb].sid_fp); ) {
		if(smb_fread(&smb[cur_smb], &idx,sizeof(idx),smb[cur_smb].sid_fp) != sizeof(idx))
			break;

		if(idx.number==0)	/* invalid message number, ignore */
			continue;

		if(idx.number<=ptr || (idx.attr&MSG_DELETE))
			continue;

		if((idx.attr&MSG_MODERATED) && !(idx.attr&MSG_VALIDATED))
			break;

		(*post)[l++].idx=idx;
	}
	smb_unlocksmbhdr(&smb[cur_smb]);
	if(!l)
		FREE_AND_NULL(*post);
	return(l);
}

void cleanup(void)
{
	char*		p;
	char		path[MAX_PATH+1];

	while((p=strListPop(&locked_bso_nodes)) != NULL)
		delfile(p, __LINE__);

	if(mtxfile_locked) {
		SAFEPRINTF(path,"%ssbbsecho.bsy", scfg.ctrl_dir);
		if(delfile(path, __LINE__))
			mtxfile_locked = false;
	}
}

void bail(int code)
{
	cleanup();

	if(code || netmail || exported_netmail || packed_netmail || echomail || exported_echomail)
		lprintf(LOG_INFO
			,"SBBSecho exiting with error level %d, "
			"NetMail(%u imported, %u exported, %u packed), EchoMail(%u imported, %u exported)"
			,code, netmail, exported_netmail, packed_netmail, echomail, exported_echomail);
	if((code && pause_on_abend) || pause_on_exit) {
		fprintf(stderr,"\nHit any key...");
		getch();
		fprintf(stderr,"\n");
	}
	exit(code);
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


typedef struct {
	char		alias[LEN_ALIAS+1];
	char		real[LEN_NAME+1];
} username_t;

/****************************************************************************/
/* Note: Wrote another version of this function that read all userdata into */
/****************************************************************************/
/* Looks for a perfect match amoung all usernames (not deleted users)		*/
/* Returns the number of the perfect matched username or 0 if no match		*/
/* Called from functions waitforcall and newuser							*/
/* memory then scanned it from memory... took longer - always.              */
/****************************************************************************/
ushort matchname(const char *inname)
{
	static ulong total_users;
	static username_t *username;
	ulong last_user;
	int userdat,i;
	char str[256],name[LEN_NAME+1],alias[LEN_ALIAS+1];
	ulong l;

	if(!total_users) {		/* Load CRCs */
		fprintf(stderr,"\n%-25s","Loading user names...");
		sprintf(str,"%suser/user.dat",scfg.data_dir);
		if((userdat=nopen(str,O_RDONLY|O_DENYNONE))==-1)
			return(0);
		last_user=filelength(userdat)/U_LEN;
		for(total_users=0;total_users<last_user;total_users++) {
			printf("%5ld\b\b\b\b\b",total_users);
			if((username=(username_t *)realloc(username
				,(total_users+1L)*sizeof(username_t)))==NULL)
				break;
			memset(&username[total_users], 0, sizeof(username[total_users]));
			i=0;
			while(i<LOOP_NODEDAB
				&& lock(userdat,(long)((long)(total_users)*U_LEN)+U_ALIAS
					,LEN_ALIAS+LEN_NAME)==-1)
				i++;
			if(i>=LOOP_NODEDAB) {	   /* Couldn't lock USER.DAT record */
				lprintf(LOG_ERR,"ERROR locking USER.DAT record #%ld",total_users);
				continue; 
			}
			lseek(userdat,(long)((long)(total_users)*U_LEN)+U_ALIAS,SEEK_SET);
			read(userdat,alias,LEN_ALIAS);
			read(userdat,name,LEN_NAME);
			lseek(userdat,(long)(((long)total_users)*U_LEN)+U_MISC,SEEK_SET);
			read(userdat,tmp,8);
			for(i=0;i<8;i++)
				if(tmp[i]==ETX || tmp[i]=='\r') break;
			tmp[i]=0;
			unlock(userdat,(long)((long)(total_users)*U_LEN)+U_ALIAS
				,LEN_ALIAS+LEN_NAME);
			if(ahtoul(tmp)&DELETED)
				continue;
			for(i=0;i<LEN_ALIAS;i++)
				if(alias[i]==ETX || alias[i]=='\r') break;
			alias[i]=0;
			for(i=0;i<LEN_NAME;i++)
				if(name[i]==ETX || name[i]=='\r') break;
			name[i]=0;
			SAFECOPY(username[total_users].alias, alias);
			SAFECOPY(username[total_users].real, name);
		}
		close(userdat);
		fprintf(stderr,"     \b\b\b\b\b");  /* Clear counter */
		fprintf(stderr,
			"\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
			"%25s"
			"\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
			,""); 
	}

	for(l=0;l<total_users;l++)
		if(stricmp(username[l].alias, inname) == 0)
			return((ushort)l+1);
	for(l=0;l<total_users;l++)
		if(stricmp(username[l].real, inname) == 0)
			return((ushort)l+1);
	return(0);
}

/****************************************************************************/
/* Converts goofy FidoNet time format into Unix format						*/
/****************************************************************************/
time32_t fmsgtime(const char *str)
{
	char month[4];
	struct tm tm;

	memset(&tm,0,sizeof(tm));
	tm.tm_isdst=-1;	/* Do not adjust for DST */

	if(isdigit((uchar)str[1])) {	/* Regular format: "01 Jan 86  02:34:56" */
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
		switch(val|US_ZONE) {
			case AST:
			case EST:
			case CST:
			case MST:
			case PST:
			case YST:
			case HST:
			case BST:
				/* standard US timezone */
				return(val|US_ZONE);
			default: 
				/* non-standard timezone */
				return(-val);
		}
	return(val);
}

char* getfmsg(FILE* stream, ulong* outlen)
{
	char* fbuf;
	int ch;
	ulong l,length,start;

	length=0L;
	start=ftell(stream);						/* Beginning of Message */
	while(1) {
		ch=fgetc(stream);						/* Look for Terminating NULL */
		if(ch==0 || ch==EOF)					/* Found end of message */
			break;
		length++;	 							/* Increment the Length */
	}

	if((fbuf=(char *)malloc(length+1))==NULL) {
		lprintf(LOG_ERR,"ERROR line %d allocating %lu bytes of memory",__LINE__,length+1);
		bail(1); 
		return(NULL);
	}

	fseek(stream,start,SEEK_SET);
	for(l=0;l<length;l++)
		fbuf[l]=fgetc(stream);
	if(ch==0)
		fgetc(stream);		/* Read NULL */

	while(length && fbuf[length-1]<=' ')	/* truncate white-space */
		length--;
	fbuf[length]=0;

	if(outlen)
		*outlen=length;
	return((char*)fbuf);
}

#define MAX_TAILLEN 1024

/****************************************************************************/
/* Coverts a FidoNet message into a Synchronet message						*/
/* Returns 0 on success, 1 dupe, 2 filtered, 3 empty, 4 too-old				*/
/* or other SMB error														*/
/****************************************************************************/
int fmsgtosmsg(char* fbuf, fmsghdr_t fmsghdr, uint user, uint subnum)
{
	uchar	ch,stail[MAX_TAILLEN+1],*sbody;
	char	msg_id[256],str[128],*p;
	bool	done,esc,cr;
	int 	i,storage=SMB_SELFPACK;
	uint	col;
	ushort	xlat=XLAT_NONE,net;
	ulong	l,m,length,bodylen,taillen;
	ulong	save;
	long	dupechk_hashes=SMB_HASH_SOURCE_DUPE;
	fidoaddr_t faddr,origaddr,destaddr;
	smb_t*	smbfile;
	char	fname[MAX_PATH+1];
	smbmsg_t	msg;
	time32_t now=time32(NULL);
	ulong	max_msg_age = (subnum == INVALID_SUB) ? cfg.max_netmail_age : cfg.max_echomail_age;

	if(twit_list) {
		sprintf(fname,"%stwitlist.cfg",scfg.ctrl_dir);
		if(findstr(fmsghdr.from,fname) || findstr(fmsghdr.to,fname)) {
			lprintf(LOG_INFO,"Filtering message from %s to %s",fmsghdr.from,fmsghdr.to);
			return(2);
		}
	}

	memset(&msg,0,sizeof(smbmsg_t));
	if(fmsghdr.attr&FIDO_PRIVATE)
		msg.idx.attr|=MSG_PRIVATE;
	msg.hdr.attr=msg.idx.attr;

	if(fmsghdr.attr&FIDO_FILE)
		msg.hdr.auxattr|=MSG_FILEATTACH;

	msg.hdr.when_imported.time=now;
	msg.hdr.when_imported.zone=sys_timezone(&scfg);
	msg.hdr.when_written.time=fmsgtime(fmsghdr.time);
	if(max_msg_age && (time32_t)msg.hdr.when_written.time < now
		&& (now - msg.hdr.when_written.time) > max_msg_age) {
		lprintf(LOG_INFO, "Filtering message from %s due to age: %1.1f days"
			,fmsghdr.from
			,(now - msg.hdr.when_written.time) / (24.0*60.0*60.0));
		return 4;
	}

	origaddr.zone=fmsghdr.origzone; 	/* only valid if NetMail */
	origaddr.net=fmsghdr.orignet;
	origaddr.node=fmsghdr.orignode;
	origaddr.point=fmsghdr.origpoint;

	destaddr.zone=fmsghdr.destzone; 	/* only valid if NetMail */
	destaddr.net=fmsghdr.destnet;
	destaddr.node=fmsghdr.destnode;
	destaddr.point=fmsghdr.destpoint;

	smb_hfield_str(&msg,SENDER,fmsghdr.from);
	smb_hfield_str(&msg,RECIPIENT,fmsghdr.to);

	if(user) {
		sprintf(str,"%u",user);
		smb_hfield_str(&msg,RECIPIENTEXT,str);
	}

	smb_hfield_str(&msg,SUBJECT,fmsghdr.subj);

	if(fbuf==NULL) {
		lprintf(LOG_ERR,"ERROR line %d allocating fbuf",__LINE__);
		smb_freemsgmem(&msg);
		return(-1); 
	}
	length=strlen((char *)fbuf);
	if((sbody=(uchar*)malloc((length+1)*2))==NULL) {
		lprintf(LOG_ERR,"ERROR line %d allocating %lu bytes for body",__LINE__
			,(length+1)*2L);
		smb_freemsgmem(&msg);
		return(-1); 
	}

	for(col=l=esc=done=bodylen=taillen=0,cr=1;l<length;l++) {

		if(!l && !strncmp((char *)fbuf,"AREA:",5)) {
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
		if(ch==CTRL_A && cr) {	/* kludge line */

			if(!strncmp((char *)fbuf+l+1,"TOPT ",5))
				destaddr.point=atoi((char *)fbuf+l+6);

			else if(!strncmp((char *)fbuf+l+1,"FMPT ",5))
				origaddr.point=atoi((char *)fbuf+l+6);

			else if(!strncmp((char *)fbuf+l+1,"INTL ",5)) {
				faddr=atofaddr((char *)fbuf+l+6);
				destaddr.zone=faddr.zone;
				destaddr.net=faddr.net;
				destaddr.node=faddr.node;
				l+=6;
				while(l<length && fbuf[l]!=' ') l++;
				faddr=atofaddr((char *)fbuf+l+1);
				origaddr.zone=faddr.zone;
				origaddr.net=faddr.net;
				origaddr.node=faddr.node; 
			}

			else if(!strncmp((char *)fbuf+l+1,"MSGID:",6)) {
				l+=7;
				while(l<length && fbuf[l]<=' ' && fbuf[l]>=0) l++;
				m=l;
				while(m<length && fbuf[m]!='\r') m++;
				while(m && fbuf[m-1]<=' ' && fbuf[m-1]>=0) m--;
				if(m>l)
					smb_hfield(&msg,FIDOMSGID,(ushort)(m-l),fbuf+l); 
			}

			else if(!strncmp((char *)fbuf+l+1,"REPLY:",6)) {
				l+=7;
				while(l<length && fbuf[l]<=' ' && fbuf[l]>=0) l++;
				m=l;
				while(m<length && fbuf[m]!='\r') m++;
				while(m && fbuf[m-1]<=' ' && fbuf[m-1]>=0) m--;
				if(m>l)
					smb_hfield(&msg,FIDOREPLYID,(ushort)(m-l),fbuf+l); 
			}

			else if(!strncmp((char *)fbuf+l+1,"FLAGS ",6)		/* correct */
				||  !strncmp((char *)fbuf+l+1,"FLAGS:",6)) {	/* incorrect */
				l+=7;
				while(l<length && fbuf[l]<=' ' && fbuf[l]>=0) l++;
				m=l;
				while(m<length && fbuf[m]!='\r') m++;
				while(m && fbuf[m-1]<=' ' && fbuf[m-1]>=0) m--;
				if(m>l)
					smb_hfield(&msg,FIDOFLAGS,(ushort)(m-l),fbuf+l); 
			}

			else if(!strncmp((char *)fbuf+l+1,"PATH:",5)) {
				l+=6;
				while(l<length && fbuf[l]<=' ' && fbuf[l]>=0) l++;
				m=l;
				while(m<length && fbuf[m]!='\r') m++;
				while(m && fbuf[m-1]<=' ' && fbuf[m-1]>=0) m--;
				if(m>l)
					smb_hfield(&msg,FIDOPATH,(ushort)(m-l),fbuf+l); 
			}

			else if(!strncmp((char *)fbuf+l+1,"PID:",4)) {
				l+=5;
				while(l<length && fbuf[l]<=' ' && fbuf[l]>=0) l++;
				m=l;
				while(m<length && fbuf[m]!='\r') m++;
				while(m && fbuf[m-1]<=' ' && fbuf[m-1]>=0) m--;
				if(m>l)
					smb_hfield(&msg,FIDOPID,(ushort)(m-l),fbuf+l); 
			}

			else if(!strncmp((char *)fbuf+l+1,"TID:",4)) {
				l+=5;
				while(l<length && fbuf[l]<=' ' && fbuf[l]>=0) l++;
				m=l;
				while(m<length && fbuf[m]!='\r') m++;
				while(m && fbuf[m-1]<=' ' && fbuf[m-1]>=0) m--;
				if(m>l)
					smb_hfield(&msg,FIDOTID,(ushort)(m-l),fbuf+l); 
			}

			else if(!strncmp((char *)fbuf+l+1,"TZUTC:",6)) {		/* FSP-1001 */
				l+=7;
				while(l<length && fbuf[l]<=' ' && fbuf[l]>=0) l++;
				msg.hdr.when_written.zone = fmsgzone(fbuf+l);
			}

			else if(!strncmp((char *)fbuf+l+1,"TZUTCINFO:",10)) {	/* non-standard */
				l+=11;
				while(l<length && fbuf[l]<=' ' && fbuf[l]>=0) l++;
				msg.hdr.when_written.zone = fmsgzone(fbuf+l);
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

		if(ch!='\n' && ch!=0x8d) {	/* ignore LF and soft CRs */
			if(cr && (!strncmp((char *)fbuf+l,"--- ",4)
				|| !strncmp((char *)fbuf+l,"---\r",4)))
				done=1; 			/* tear line and down go into tail */
			if(done && cr && !strncmp((char *)fbuf+l,"SEEN-BY:",8)) {
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
			col++;
			if(ch=='\r') {
				cr=1;
				col=0;
				if(done) {
					if(taillen<MAX_TAILLEN)
						stail[taillen++]='\n'; 
				}
				else
					sbody[bodylen++]='\n'; 
			}
			else {
				cr=0;
				if(col==1 && !strncmp((char *)fbuf+l," * Origin: ",11)) {
					p=(char*)fbuf+l+11;
					while(*p && *p!='\r') p++;	 /* Find CR */
					while(p && *p!='(') p--;     /* rewind to '(' */
					if(p)
						origaddr=atofaddr(p+1); 	/* get orig address */
					done=1; 
				}
				if(done)
					continue;

				if(ch==ESC) esc=1;		/* ANSI codes */
				if(ch==' ' && col>40 && !esc) {	/* word wrap */
					for(m=l+1;m<length;m++) 	/* find next space */
						if(fbuf[m]<=' ' && fbuf[m]>=0)
							break;
					if(m<length && m-l>80-col) {  /* if it's beyond the eol */
						sbody[bodylen++]='\r';
						sbody[bodylen++]='\n';
						col=0; 
					} 
				}
			} 
		} 
	}

	if(bodylen>=2 && sbody[bodylen-2]=='\r' && sbody[bodylen-1]=='\n')
		bodylen-=2; 						/* remove last CRLF if present */
	sbody[bodylen]=0;

	while(taillen && stail[taillen-1]<=' ')	/* trim all garbage off the tail */
		taillen--;
	stail[taillen]=0;

	if(subnum==INVALID_SUB && !bodylen && !taillen && cfg.kill_empty_netmail) {
		lprintf(LOG_INFO,"Empty NetMail - Ignored ");
		smb_freemsgmem(&msg);
		free(sbody);
		return(3);
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
		if(net) {
			smb_hfield(&msg,RECIPIENTNETTYPE,sizeof(ushort),&net);
			smb_hfield(&msg,RECIPIENTNETADDR,sizeof(fidoaddr_t),&destaddr); 
		}
		smbfile->status.attr = SMB_EMAIL;
		smbfile->status.max_age = scfg.mail_maxage;
		smbfile->status.max_crcs = scfg.mail_maxcrcs;
		if(scfg.sys_misc&SM_FASTMAIL)
			storage= SMB_FASTALLOC;
	} else {
		smbfile=&smb[cur_smb];
		smbfile->status.max_age	 = scfg.sub[subnum]->maxage;
		smbfile->status.max_crcs = scfg.sub[subnum]->maxcrcs;
		smbfile->status.max_msgs = scfg.sub[subnum]->maxmsgs;
		if(scfg.sub[subnum]->misc&SUB_HYPER)
			storage = smbfile->status.attr = SMB_HYPERALLOC;
		else if(scfg.sub[subnum]->misc&SUB_FAST)
			storage = SMB_FASTALLOC;
		if(scfg.sub[subnum]->misc&SUB_LZH)
			xlat=XLAT_LZH;

		msg.idx.time=msg.hdr.when_imported.time;	/* needed for MSG-ID generation */
		msg.idx.number=smbfile->status.last_msg+1;		/* needed for MSG-ID generation */

		/* Generate default (RFC822) message-id (always) */
		get_msgid(&scfg,subnum,&msg,msg_id,sizeof(msg_id));
		smb_hfield_str(&msg,RFC822MSGID,msg_id);
	}
	if(smbfile->status.max_crcs==0)
		dupechk_hashes&=~(1<<SMB_HASH_SOURCE_BODY);
	/* Bad echo area collects a *lot* of messages, and thus, hashes - so no dupe checking */
	if(cfg.badecho>=0 && subnum==cfg.area[cfg.badecho].sub)
		dupechk_hashes=SMB_HASH_SOURCE_NONE;

	i=smb_addmsg(smbfile, &msg, storage, dupechk_hashes, xlat, sbody, stail);

	if(i!=SMB_SUCCESS) {
		lprintf(LOG_ERR,"ERROR smb_addmsg(%s) returned %d: %s"
			,subnum==INVALID_SUB ? "mail":scfg.sub[subnum]->code, i, smbfile->last_error);
	}
	smb_freemsgmem(&msg);

	return(i);
}

/***********************************************************************/
/* Get zone and point from kludge lines from the stream  if they exist */
/***********************************************************************/
void getzpt(FILE* stream, fmsghdr_t* hdr)
{
	char buf[0x1000];
	int i,len,cr=0;
	long pos;
	fidoaddr_t faddr;

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
	fseek(stream,pos,SEEK_SET);
}
/******************************************************************************
 This function will seek to the next NULL found in stream
******************************************************************************/
void seektonull(FILE* stream)
{
	char ch;

	while(!feof(stream)) {
		if(!fread(&ch,1,1,stream))
			break;
		if(!ch)
			break; 
	}
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
void putfmsg(FILE* stream, const char* fbuf, fmsghdr_t fmsghdr, area_t area
	,addrlist_t seenbys, addrlist_t paths)
{
	char str[256],seenby[256];
	int lastlen=0,net_exists=0;
	unsigned u,j;
	fidoaddr_t addr,sysaddr,lasthop={0,0,0,0};
	fpkdmsg_t pkdmsg;
	time_t t;
	size_t len;
	struct tm* tm;

	addr=getsysfaddr(fmsghdr.destzone);

	/* Write fixed-length header fields */
	memset(&pkdmsg,0,sizeof(pkdmsg));
	pkdmsg.type		= 2;
	pkdmsg.orignet	= addr.net;
	pkdmsg.orignode	= addr.node;
	pkdmsg.destnet	= fmsghdr.destnet;
	pkdmsg.destnode	= fmsghdr.destnode;
	pkdmsg.attr		= fmsghdr.attr;
	pkdmsg.cost		= fmsghdr.cost;
	SAFECOPY(pkdmsg.time,fmsghdr.time);
	fwrite(&pkdmsg		,sizeof(pkdmsg)			,1,stream);

	/* Write variable-length (ASCIIZ) header fields */
	fwrite(fmsghdr.to	,strlen(fmsghdr.to)+1	,1,stream);
	fwrite(fmsghdr.from	,strlen(fmsghdr.from)+1	,1,stream);
	fwrite(fmsghdr.subj	,strlen(fmsghdr.subj)+1	,1,stream);

	len = strlen((char *)fbuf);

	/* Write message body */
	if(area.name)
		if(strncmp((char *)fbuf,"AREA:",5))                     /* No AREA: Line */
			fprintf(stream,"AREA:%s\r",area.name);              /* So add one */
	fwrite(fbuf,len,1,stream);
	lastlen=9;
	if(len && fbuf[len-1]!='\r')
		fputc('\r',stream);

	if(area.name==NULL)	{ /* NetMail, so add FSP-1010 Via kludge line */
		t = time(NULL);
		tm = gmtime(&t);
		fprintf(stream,"\1Via %s @%04u%02u%02u.%02u%02u%02u.UTC "
			"SBBSecho %u.%02u-%s r%s\r"
			,smb_faddrtoa(&addr,NULL)
			,tm->tm_year+1900
			,tm->tm_mon+1
			,tm->tm_mday
			,tm->tm_hour
			,tm->tm_min
			,tm->tm_sec
			,SBBSECHO_VERSION_MAJOR,SBBSECHO_VERSION_MINOR,PLATFORM_DESC,revision);

	} else { /* EchoMail, Not NetMail */

		if(foreign_zone(addr.zone, fmsghdr.destzone))	/* Zone Gate */
			fprintf(stream,"SEEN-BY: %d/%d\r",fmsghdr.destnet,fmsghdr.destnode);
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
					strcat(seenby,str); 
				}
				sprintf(str,"%d",seenbys.addr[u].node);
				strcat(seenby,str);
				if(lastlen+strlen(seenby)<80) {
					fwrite(seenby,strlen(seenby),1,stream);
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
						strcat(seenby,str); 
					}
					sprintf(str,"%d",area.link[u].node);
					strcat(seenby,str);
					if(lastlen+strlen(seenby)<80) {
						fwrite(seenby,strlen(seenby),1,stream);
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
						strcat(seenby,str); 
					}
					sprintf(str,"%d",scfg.faddr[u].node);
					strcat(seenby,str);
					if(lastlen+strlen(seenby)<80) {
						fwrite(seenby,strlen(seenby),1,stream);
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
			addr=getsysfaddr(fmsghdr.destzone);
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
					strcat(seenby,str); 
				}
				sprintf(str,"%d",paths.addr[u].node);
				strcat(seenby,str);
				if(lastlen+strlen(seenby)<80) {
					fwrite(seenby,strlen(seenby),1,stream);
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
			sysaddr=getsysfaddr(fmsghdr.destzone);
			if(sysaddr.net!=0 && sysaddr.point==0 
				&& (paths.addrs==0 || lasthop.net!=sysaddr.net || lasthop.node!=sysaddr.node)) {
				if(sysaddr.net!=addr.net || !net_exists) {
					net_exists=1;
					addr.net=sysaddr.net;
					sprintf(str,"%d/",addr.net);
					strcat(seenby,str); 
				}
				sprintf(str,"%d",sysaddr.node);
				strcat(seenby,str);
				if(lastlen+strlen(seenby)<80)
					fwrite(seenby,strlen(seenby),1,stream);
				else {
					fprintf(stream,"\r\1PATH:");
					fwrite(seenby,strlen(seenby),1,stream); 
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

	fseek(stream, 0, SEEK_END);
	offset = ftell(stream);
	if(offset >= sizeof(fpkthdr_t)+sizeof(terminator)) {
		fseek(stream, offset-sizeof(terminator), SEEK_SET);
		if(fread(&terminator, sizeof(terminator), 1, stream)
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
	fbuf=strstr((char *)inbuf,"\r * Origin: ");
	if(!fbuf)
		fbuf=strstr((char *)inbuf,"\n * Origin: ");
	if(!fbuf)
		fbuf=inbuf;
	FREE_AND_NULL(seenbys->addr);
	addr.zone=addr.net=addr.node=addr.point=seenbys->addrs=0;
	p=strstr((char *)fbuf,"\rSEEN-BY:");
	if(!p) p=strstr((char *)fbuf,"\nSEEN-BY:");
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
	if((p=strstr((char *)fbuf,"\1PATH:"))!=NULL) {
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
	fbuf=strstr((char *)inbuf,"\r * Origin: ");
	if(!fbuf)
		fbuf=inbuf;
	if((p=strstr((char *)fbuf,"\rSEEN-BY:"))!=NULL)
		*(p)=0;
	if((p=strstr((char *)fbuf,"\r\1PATH:"))!=NULL)
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

		pack_bundle(pkt->filename, pkt->orig, pkt->dest);

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
		return NULL;
	}
	if((pkt->fp = fopen(pkt->filename, "wb")) == NULL) {
		lprintf(LOG_ERR, "ERROR %u (%s) opening/creating %s", errno, strerror(errno), pkt->filename);
		return NULL;
	}

	pkt->orig = orig;
	pkt->dest = dest;

	lprintf(LOG_DEBUG, "Creating outbound packet from %s to %s: %s"
		,smb_faddrtoa(&orig, str), smb_faddrtoa(&dest, NULL), pkt->filename);
	fpkthdr_t pkthdr;
	new_pkthdr(&pkthdr, orig, dest, nodecfg);
	fwrite(&pkthdr, sizeof(pkthdr), 1, pkt->fp);
	listAddNode(&outpkt_list, pkt, 0, LAST_NODE);
	return pkt;
}

/******************************************************************************
 This is where we put outgoing messages into packets.
******************************************************************************/
void pkt_to_pkt(const char *fbuf, area_t area, const fidoaddr_t* faddr
	,fmsghdr_t fmsghdr, addrlist_t seenbys, addrlist_t paths)
{
	unsigned u;
	fidoaddr_t sysaddr;

	for(u=0; u<area.links; u++) {
		if(faddr != NULL && memcmp(faddr,&area.link[u], sizeof(fidoaddr_t)) != 0)
			continue;
		if(check_psb(&seenbys, area.link[u]))
			continue;
		nodecfg_t* nodecfg = findnodecfg(&cfg, area.link[u],0);
		if(nodecfg != NULL && nodecfg->passive)
			continue;
		sysaddr = getsysfaddr(area.link[u].zone);
		printf("%s ",smb_faddrtoa(&area.link[u],NULL));
		outpkt_t* pkt = get_outpkt(sysaddr, area.link[u], nodecfg);
		if(pkt == NULL) {
			lprintf(LOG_ERR, "ERROR Creating/opening outbound packet for %s", smb_faddrtoa(&area.link[u], NULL));
			bail(1);
		}
		fmsghdr.destnode	= area.link[u].node;
		fmsghdr.destnet		= area.link[u].net;
		fmsghdr.destzone	= area.link[u].zone;
		lprintf(LOG_DEBUG, "Adding %s message from %s (%s) to packet for %s: %s"
			,area.name, fmsghdr.from, fmsghdr_srcaddr_str(&fmsghdr), smb_faddrtoa(&area.link[u], NULL), pkt->filename);
		putfmsg(pkt->fp, fbuf, fmsghdr, area, seenbys, paths); 
	}
}

int pkt_to_msg(FILE* fidomsg, const fmsghdr_t* hdr, const char* info)
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
		MKDIR(scfg.netmail_dir);
		for(i=1;i;i++) {
			sprintf(path,"%s%u.msg",scfg.netmail_dir,i);
			if(!fexistcase(path))
				break; 
			if(terminated)
				return 1;
		}
		if(!i) {
			lprintf(LOG_WARNING,"Too many netmail messages");
			return(-1); 
		}
		if((file=nopen(path,O_WRONLY|O_CREAT))==-1) {
			lprintf(LOG_ERR,"ERROR %u (%s) line %d creating %s",errno,strerror(errno),__LINE__,path);
			return(-1);
		}
		write(file,hdr,sizeof(fmsghdr_t));
		write(file,fmsgbuf,l+1); /* Write the '\0' terminator too */
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
int import_netmail(const char* path, fmsghdr_t hdr, FILE* fidomsg, const char* inbound)
{
	char info[512],str[256],tmp[256],subj[256]
		,*fmsgbuf=NULL,*p,*tp,*sp;
	int i,match,usernumber;
	ulong length;
	fidoaddr_t addr;

	hdr.destzone=sys_faddr.zone;
	hdr.destpoint=hdr.origpoint=0;
	getzpt(fidomsg,&hdr);				/* use kludge if found */
	for(match=0;match<scfg.total_faddrs;match++)
		if((hdr.destzone==scfg.faddr[match].zone || (cfg.fuzzy_zone))
			&& hdr.destnet==scfg.faddr[match].net
			&& hdr.destnode==scfg.faddr[match].node
			&& hdr.destpoint==scfg.faddr[match].point)
			break;
	if(match<scfg.total_faddrs && (cfg.fuzzy_zone))
		hdr.origzone=hdr.destzone=scfg.faddr[match].zone;
	if(hdr.origpoint)
		sprintf(tmp,".%hu",hdr.origpoint);
	else
		tmp[0]=0;
	if(hdr.destpoint)
		sprintf(str,".%hu",hdr.destpoint);
	else
		str[0]=0;
	sprintf(info,"%s%s%s (%hu:%hu/%hu%s) To: %s (%hu:%hu/%hu%s)"
		,path,path[0] ? " ":""
		,hdr.from,hdr.origzone,hdr.orignet,hdr.orignode,tmp
		,hdr.to,hdr.destzone,hdr.destnet,hdr.destnode,str);
	printf("%s ",info);

	if(!opt_import_netmail) {
		printf("Ignored");
		if(!path[0]) {
			printf(" - ");
			pkt_to_msg(fidomsg,&hdr,info);
		} else
			lprintf(LOG_INFO,"%s Ignored",info);

		return(-1); 
	}

	if(match>=scfg.total_faddrs && !cfg.ignore_netmail_dest_addr) {
		printf("Foreign address");
		if(!path[0]) {
			printf(" - ");
			pkt_to_msg(fidomsg,&hdr,info);
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
		sprintf(email->file,"%smail",scfg.data_dir);
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
			lprintf(LOG_ERR,"ERROR %d (%s) creating %s",i,email->last_error,email->file);
			bail(1); 
			return -1;
		} 
	}

	if(!stricmp(hdr.to,"AREAFIX") || !stricmp(hdr.to,"SBBSECHO")) {
		fmsgbuf=getfmsg(fidomsg,NULL);
		if(path[0]) {
			if(cfg.delete_netmail) {
				fclose(fidomsg);
				delfile(path, __LINE__);
			}
			else {
				hdr.attr|=FIDO_RECV;
				fseek(fidomsg,0L,SEEK_SET);
				fwrite(&hdr,sizeof(fmsghdr_t),1,fidomsg);
				fclose(fidomsg); /* Gotta close it here for areafix stuff */
			}
		}
		addr.zone=hdr.origzone;
		addr.net=hdr.orignet;
		addr.node=hdr.orignode;
		addr.point=hdr.origpoint;
		lprintf(LOG_INFO, info);
		p=process_areafix(addr,fmsgbuf,/* Password: */hdr.subj, /* To: */hdr.from);
		if(p) {
			uint notify = matchuser(&scfg, cfg.areamgr, TRUE);
			if(notify) {
				SAFECOPY(hdr.to,scfg.sys_op);
				SAFECOPY(hdr.from,"SBBSecho");
				SAFECOPY(hdr.subj,"Areafix Request");
				hdr.origzone=hdr.orignet=hdr.orignode=hdr.origpoint=0;
				if(fmsgtosmsg(p,hdr,notify,INVALID_SUB)==0) {
					sprintf(str,"\7\1n\1hSBBSecho \1n\1msent you mail\r\n");
					putsmsg(&scfg,notify,str); 
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
		usernumber = matchname(hdr.to);
	if(!usernumber && cfg.default_recipient[0])
		usernumber = matchuser(&scfg, cfg.default_recipient, TRUE);
	if(!usernumber) {
		lprintf(LOG_WARNING,"%s Unknown user",info); 

		if(!path[0]) {
			printf(" - ");
			pkt_to_msg(fidomsg,&hdr,info);
		}
		return(2); 
	}

	/*********************/
	/* Importing NetMail */
	/*********************/

	fmsgbuf=getfmsg(fidomsg,&length);

	switch(i=fmsgtosmsg(fmsgbuf,hdr,usernumber,INVALID_SUB)) {
		case 0:			/* success */
			break;
		case 2:			/* filtered */
			lprintf(LOG_WARNING,"%s Filtered - Ignored",info);
			break;
		case 3:			/* empty */
			lprintf(LOG_WARNING,"%s Empty - Ignored",info);
			break;
		case 4:			/* too-old */
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

	addr.zone=hdr.origzone;
	addr.net=hdr.orignet;
	addr.node=hdr.orignode;
	addr.point=hdr.origpoint;
	sprintf(str,"\7\1n\1hSBBSecho: \1m%.*s \1n\1msent you NetMail from \1h%s\1n\r\n"
		,FIDO_NAME_LEN-1
		,hdr.from
		,smb_faddrtoa(&addr,NULL));
	putsmsg(&scfg,usernumber,str);

	if(hdr.attr&FIDO_FILE) {	/* File attachment */
		SAFECOPY(subj,hdr.subj);
		tp=subj;
		while(1) {
			p=strchr(tp,' ');
			if(p) *p=0;
			sp=strrchr(tp,'/');              /* sp is slash pointer */
			if(!sp) sp=strrchr(tp,'\\');
			if(sp) tp=sp+1;
			SAFEPRINTF2(str,"%s%s", inbound, tp);
			if(!fexistcase(str)) {
				if(inbound == cfg.inbound)
					inbound = cfg.secure_inbound;
				else
					inbound = cfg.inbound;
				SAFEPRINTF2(str,"%s%s", inbound, tp);
			}
			SAFEPRINTF2(tmp,"%sfile/%04u.in",scfg.data_dir,usernumber);
			MKDIR(tmp);
			backslash(tmp);
			strcat(tmp,tp);
			mv(str,tmp,0);
			if(!p)
				break;
			tp=p+1; 
		} 
	}
	netmail++;

	FREE_AND_NULL(fmsgbuf);

	/***************************/
	/* Updating message header */
	/***************************/
	/***	NOT packet compatible
	if(!(misc&DONT_SET_RECV)) {
		hdr.attr|=FIDO_RECV;
		fseek(fidomsg,0L,SEEK_SET);
		fwrite(&hdr,sizeof(fmsghdr_t),1,fidomsg); }
	***/
	lprintf(LOG_INFO,"%s Imported",info);
	return(0);
}

static uint32_t read_export_ptr(int subnum, const char* tag)
{
	char		path[MAX_PATH+1];
	char		key[INI_MAX_VALUE_LEN];
	int			file;
	FILE*		fp;
	uint32_t	ptr=0;

	/* New way (July-21-2012): */
	safe_snprintf(path,sizeof(path),"%s%s.ini",scfg.sub[subnum]->data_dir,scfg.sub[subnum]->code);
	if((fp=iniOpenFile(path, /* create: */false)) != NULL) {
		safe_snprintf(key, sizeof(key), "%s.export_ptr", tag);
		if((ptr=iniReadLongInt(fp, "SBBSecho", key, 0)) == 0)
			ptr=iniReadLongInt(fp, "SBBSecho", "export_ptr", 0);	/* the previous .ini method (did not support gating) */
		iniCloseFile(fp);
	}
	if(ptr)	return ptr;
	/* Old way: */
	safe_snprintf(path,sizeof(path),"%s%s.sfp",scfg.sub[subnum]->data_dir,scfg.sub[subnum]->code);
	if((file=nopen(path,O_RDONLY)) != -1) {
		read(file,&ptr,sizeof(ptr));
		close(file); 
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
	char	msgid[256];
	char*	buf=NULL;
	char*	minus;
	char*	fmsgbuf=NULL;
	ulong	fmsgbuflen;
	int		tzone;
	unsigned area;
	int		i,j,k=0;
	ulong	f,l,m,exp,exported=0;
	uint32_t ptr,lastmsg,posts;
	long	msgs;
	smbmsg_t msg;
	smbmsg_t orig_msg;
	fmsghdr_t hdr;
	struct	tm *tm;
	post_t *post;
	area_t fakearea;
	addrlist_t msg_seen,msg_path;
	time_t	tt;

	memset(&msg_seen,0,sizeof(addrlist_t));
	memset(&msg_path,0,sizeof(addrlist_t));
	memset(&fakearea,0,sizeof(area_t));
	memset(&hdr,0,sizeof(hdr));

	printf("\nScanning for Outbound EchoMail...");

	for(area=0; area<cfg.areas && !terminated; area++) {
		const char* tag=cfg.area[area].name;
		if(area==cfg.badecho)		/* Don't scan the bad-echo area */
			continue;
		if(!cfg.area[area].links)
			continue;
		i=cfg.area[area].sub;
		if(i<0 || i>=scfg.total_subs)	/* Don't scan pass-through areas */
			continue;
		if(nodecfg != NULL ) { 		/* Skip areas not meant for this address */
			if(!area_is_linked(area,&nodecfg->addr))
				continue; 
		}
		if(sub_code[0] && stricmp(sub_code,scfg.sub[i]->code))
			continue;
		printf("\rScanning %-*.*s -> %-*s "
			,LEN_EXTCODE,LEN_EXTCODE,scfg.sub[i]->code
			,FIDO_AREATAG_LEN, tag);
		ptr=0;
		if(!rescan)
			ptr=read_export_ptr(i, tag);

		msgs=getlastmsg(i,&lastmsg,0);
		if(msgs<1 || (!rescan && ptr>=lastmsg)) {
			if(msgs>=0 && ptr>lastmsg && !rescan && !opt_leave_msgptrs) {
				lprintf(LOG_DEBUG,"Fixing new-scan pointer (%u, lastmsg=%u).", ptr, lastmsg);
				write_export_ptr(i, lastmsg, tag);
			}
			continue; 
		}

		sprintf(smb[cur_smb].file,"%s%s"
			,scfg.sub[i]->data_dir,scfg.sub[i]->code);
		smb[cur_smb].retry_time=scfg.smb_retry_time;
		if((j=smb_open(&smb[cur_smb]))!=SMB_SUCCESS) {
			lprintf(LOG_ERR,"ERROR %d (%s) line %d opening %s",j,smb[cur_smb].last_error,__LINE__
				,smb[cur_smb].file);
			continue; 
		}

		post=NULL;
		posts=loadmsgs(&post,ptr);

		if(!posts)	{ /* no new messages */
			smb_close(&smb[cur_smb]);
			FREE_AND_NULL(post);
			continue; 
		}

		for(m=exp=0;m<posts && !terminated;m++) {
			printf("\r%*s %5lu of %-5"PRIu32"  "
				,LEN_EXTCODE,scfg.sub[i]->code,m+1,posts);
			memset(&msg,0,sizeof(msg));
			msg.idx=post[m].idx;
			if((k=smb_lockmsghdr(&smb[cur_smb],&msg))!=SMB_SUCCESS) {
				lprintf(LOG_ERR,"ERROR %d (%s) line %d locking %s msghdr"
					,k,smb[cur_smb].last_error,__LINE__,smb[cur_smb].file);
				continue; 
			}
			k=smb_getmsghdr(&smb[cur_smb],&msg);
			if(k || msg.hdr.number!=post[m].idx.number) {
				smb_unlockmsghdr(&smb[cur_smb],&msg);
				smb_freemsgmem(&msg);

				msg.hdr.number=post[m].idx.number;
				if((k=smb_getmsgidx(&smb[cur_smb],&msg))!=SMB_SUCCESS) {
					lprintf(LOG_ERR,"ERROR %d line %d reading %s index",k,__LINE__
						,smb[cur_smb].file);
					continue; 
				}
				if((k=smb_lockmsghdr(&smb[cur_smb],&msg))!=SMB_SUCCESS) {
					lprintf(LOG_ERR,"ERROR %d line %d locking %s msghdr",k,__LINE__
						,smb[cur_smb].file);
					continue; 
				}
				if((k=smb_getmsghdr(&smb[cur_smb],&msg))!=SMB_SUCCESS) {
					smb_unlockmsghdr(&smb[cur_smb],&msg);
					lprintf(LOG_ERR,"ERROR %d line %d reading %s msghdr",k,__LINE__
						,smb[cur_smb].file);
					continue; 
				} 
			}

			if((!rescan && !(opt_export_ftn_echomail) && (msg.from_net.type==NET_FIDO))	
				|| !strnicmp(msg.subj,"NE:",3)) {   /* no echo */
				smb_unlockmsghdr(&smb[cur_smb],&msg);
				smb_freemsgmem(&msg);
				continue;   /* From a Fido node, ignore it */
			}

			if(msg.from_net.type!=NET_NONE 
				&& msg.from_net.type!=NET_FIDO
				&& msg.from_net.type!=NET_FIDO_ASCII
				&& !(scfg.sub[i]->misc&SUB_GATE)) {
				smb_unlockmsghdr(&smb[cur_smb],&msg);
				smb_freemsgmem(&msg);
				continue; 
			}

			memset(&hdr,0,sizeof(fmsghdr_t));	 /* Zero the header */
			hdr.origzone=scfg.sub[i]->faddr.zone;
			hdr.orignet=scfg.sub[i]->faddr.net;
			hdr.orignode=scfg.sub[i]->faddr.node;
			hdr.origpoint=scfg.sub[i]->faddr.point;

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

			buf=smb_getmsgtxt(&smb[cur_smb],&msg,GETMSGTXT_ALL);
			if(!buf) {
				smb_unlockmsghdr(&smb[cur_smb],&msg);
				smb_freemsgmem(&msg);
				continue; 
			}
			lprintf(LOG_DEBUG,"Exporting %s message #%u from %s to %s in area: %s"
				,scfg.sub[i]->code, msg.hdr.number, msg.from, msg.to, tag);
			fmsgbuflen=strlen((char *)buf)+4096; /* over alloc for kludge lines */
			fmsgbuf=malloc(fmsgbuflen);
			if(!fmsgbuf) {
				lprintf(LOG_ERR,"ERROR line %d allocating %lu bytes for fmsgbuf"
					,__LINE__,fmsgbuflen);
				smb_unlockmsghdr(&smb[cur_smb],&msg);
				smb_freemsgmem(&msg);
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

			f+=sprintf(fmsgbuf+f,"\1MSGID: %.256s\r"
				,ftn_msgid(scfg.sub[i],&msg,msgid,sizeof(msgid)));

			if(msg.ftn_reply!=NULL)			/* use original REPLYID */
				f+=sprintf(fmsgbuf+f,"\1REPLY: %.256s\r", msg.ftn_reply);
			else if(msg.hdr.thread_back) {	/* generate REPLYID (from original message's MSG-ID, if it had one) */
				memset(&orig_msg,0,sizeof(orig_msg));
				orig_msg.hdr.number=msg.hdr.thread_back;
				if(smb_getmsgidx(&smb[cur_smb], &orig_msg))
					f+=sprintf(fmsgbuf+f,"\1REPLY: <%s>\r",smb[cur_smb].last_error);
				else {
					smb_lockmsghdr(&smb[cur_smb],&orig_msg);
					smb_getmsghdr(&smb[cur_smb],&orig_msg);
					smb_unlockmsghdr(&smb[cur_smb],&orig_msg);
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

			/* Unknown kludge lines are added here */
			for(l=0;l<msg.total_hfields && f<fmsgbuflen;l++)
				if(msg.hfield[l].type == FIDOCTRL)
					f+=sprintf(fmsgbuf+f,"\1%.512s\r",(char*)msg.hfield_dat[l]);

			for(l=0,cr=1;buf[l] && f<fmsgbuflen;l++) {
				if(buf[l]==CTRL_A) { /* Ctrl-A, so skip it and the next char */
					char ch;
					l++;
					if(buf[l]==0 || toupper(buf[l])=='Z')	/* EOF */
						break;
					if((ch=ctrl_a_to_ascii_char(buf[l])) != 0)
						fmsgbuf[f++]=ch;
					continue; 
				}
					
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
					else if(!(scfg.sub[i]->misc&SUB_NOTAG) && !strncmp(tp," * Origin: ",11))
						*(tp+1)='#'; 
				} /* Convert * Origin into # Origin */

				if(buf[l]=='\r')
					cr=1;
				else
					cr=0;
				if((scfg.sub[i]->misc&SUB_ASCII)) {
					if(buf[l]<' ' && buf[l]>=0 && buf[l]!='\r'
						&& buf[l]!='\n')			/* Ctrl ascii */
						buf[l]='.';             /* converted to '.' */
					if((uchar)buf[l]&0x80)		/* extended ASCII */
						buf[l]=exascii_to_ascii_char(buf[l]);
				}

				fmsgbuf[f++]=buf[l]; 
			}

			FREE_AND_NULL(buf);
			fmsgbuf[f]=0;

			if(!(scfg.sub[i]->misc&SUB_NOTAG)) {
				if(!tear) {  /* No previous tear line */
					sprintf(str,"--- SBBSecho %u.%02u-%s\r"
						,SBBSECHO_VERSION_MAJOR,SBBSECHO_VERSION_MINOR,PLATFORM_DESC);
					strcat((char *)fmsgbuf,str); 
				}

				sprintf(str," * Origin: %s (%s)\r"
					,scfg.sub[i]->origline[0] ? scfg.sub[i]->origline : scfg.origline
					,smb_faddrtoa(&scfg.sub[i]->faddr,NULL));
				strcat((char *)fmsgbuf,str); 
			}

			for(k=0;k<cfg.areas;k++)
				if(cfg.area[k].sub==i) {
					cfg.area[k].exported++;
					pkt_to_pkt(fmsgbuf,cfg.area[k]
						,nodecfg ? &nodecfg->addr : NULL,hdr,msg_seen
						,msg_path);
					break; 
				}
			FREE_AND_NULL(fmsgbuf);
			exported++;
			exp++;
			printf("Exp: %lu ",exp);
			smb_unlockmsghdr(&smb[cur_smb],&msg);
			smb_freemsgmem(&msg); 
		}

		smb_close(&smb[cur_smb]);
		FREE_AND_NULL(post);

		if(!rescan && !opt_leave_msgptrs && lastmsg>ptr)
			write_export_ptr(i, lastmsg, tag);
	}

	printf("\r%*s\r", 70, "");

	for(i=0;i<cfg.areas;i++)
		if(cfg.area[i].exported)
			lprintf(LOG_INFO,"Exported: %5u msgs %*s -> %s"
				,cfg.area[i].exported,LEN_EXTCODE,scfg.sub[cfg.area[i].sub]->code
				,cfg.area[i].name);

	if(exported)
		lprintf(LOG_INFO,"Exported: %5lu msgs total", exported);
	exported_echomail += exported;
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
	for(;!feof(email->sid_fp);msg.offset++) {

		smb_freemsgmem(&msg);
		if(fread(&msg.idx, sizeof(msg.idx), 1, email->sid_fp) != 1)
			break;

		if((msg.idx.attr&MSG_DELETE) || msg.idx.to != 0)
			continue;
				
		if(smb_getmsghdr(email, &msg) != SMB_SUCCESS)
			continue;

		if(msg.to_ext != 0 || msg.to_net.type != NET_FIDO)
			continue;

		printf("NetMail msg #%u from %s to %s (%s): "
			,msg.hdr.number, msg.from, msg.to, smb_faddrtoa(msg.to_net.addr,NULL));
		if(msg.hdr.netattr&MSG_SENT) {
			printf("already sent\n");
			continue;
		}

		if((txt=smb_getmsgtxt(email,&msg,GETMSGTXT_ALL)) == NULL) {
			lprintf(LOG_ERR,"!ERROR %d getting message text for mail msg #%u"
				, email->last_error, msg.hdr.number);
			continue;
		}

		create_netmail(msg.to, &msg, msg.subj, txt, *(fidoaddr_t*)msg.to_net.addr,/* file_attached */false);
		FREE_AND_NULL(txt);

		if(cfg.delete_netmail || (msg.hdr.netattr&MSG_KILLSENT)) {
			/* Delete exported netmail */
			msg.hdr.attr |= MSG_DELETE;
			if(smb_updatemsg(email, &msg) != SMB_SUCCESS)
				lprintf(LOG_ERR,"!ERROR %d deleting mail msg #%u"
					,email->last_error, msg.hdr.number);
			if(msg.hdr.auxattr&MSG_FILEATTACH)
				delfattach(&scfg,&msg);
			fseek(email->sid_fp, (msg.offset+1)*sizeof(msg.idx), SEEK_SET);
		} else {
			/* Just mark as "sent" */
			msg.hdr.netattr |= MSG_SENT;
			if(smb_putmsghdr(email, &msg) != SMB_SUCCESS)
				lprintf(LOG_ERR,"!ERROR %d updating msg header for mail msg #%u"
					, email->last_error, msg.hdr.number);
			if(msg.hdr.auxattr&MSG_KILLFILE)
				delfattach(&scfg, &msg);
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
	fwrite(&hdr, sizeof(hdr), 1, fp);
	fclose(fp);
	if(hdr.attr&FIDO_KILLSENT)
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
	int		i;
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
		hdr.destzone=hdr.origzone=sys_faddr.zone;
		hdr.destpoint=hdr.origpoint=0;
		getzpt(fidomsg,&hdr);				/* use kludge if found */
		addr.zone		= hdr.destzone;
		addr.net		= hdr.destnet;
		addr.node		= hdr.destnode;
		addr.point		= hdr.destpoint;
		for(i=0;i<scfg.total_faddrs;i++)
			if(!memcmp(&addr,&scfg.faddr[i],sizeof(fidoaddr_t)))
				break;
		if(i<scfg.total_faddrs)	{				  /* In-bound, so ignore */
			fclose(fidomsg);
			continue;
		}
		printf("NetMail msg %s from %s (%s) to %s (%s): "
			,getfname(path), hdr.from, fmsghdr_srcaddr_str(&hdr), hdr.to, smb_faddrtoa(&addr,NULL));
		if(hdr.attr&FIDO_SENT) {
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
			outbound = get_current_outbound(addr, /* fileboxes: */true);
			if(addr.point)
				SAFEPRINTF2(req,"%s%08x.req",outbound,addr.point);
			else
				SAFEPRINTF3(req,"%s%04x%04x.req",outbound,addr.net,addr.node);
			if((fp=fopen(req,"a")) == NULL)
				lprintf(LOG_ERR,"ERROR %d creating/opening %s", errno, req);
			else {
				fprintf(fp,"%s\n",getfname(hdr.subj));
				fclose(fp);
				if(write_flofile(req, addr,/* bundle: */false,/* use_outbox: */true))
					bail(1);
				netmail_sent(path);
			}
			continue;
		}

		lprintf(LOG_INFO, "Packing NetMail (%s) from %s (%s) to %s (%s), attr: %04hX, subject: %s"
			,getfname(path)
			,hdr.from, fmsghdr_srcaddr_str(&hdr)
			,hdr.to, smb_faddrtoa(&addr,NULL)
			,hdr.attr, hdr.subj);
		fmsgbuf=getfmsg(fidomsg,NULL);
		fclose(fidomsg);
		if(!fmsgbuf) {
			lprintf(LOG_ERR,"ERROR line %d allocating memory for NetMail fmsgbuf"
				,__LINE__);
			bail(1); 
			return;
		}

		nodecfg=findnodecfg(&cfg, addr, 0);
		if(nodecfg!=NULL && nodecfg->route.zone	&& nodecfg->status==MAIL_STATUS_NORMAL) {
			addr=nodecfg->route;		/* Routed */
			lprintf(LOG_INFO, "Routing NetMail (%s) to %s",getfname(path),smb_faddrtoa(&addr,NULL));
			nodecfg=findnodecfg(&cfg, addr,0); 
		}
		if(!bso_lock_node(addr))
			continue;

		if(cfg.flo_mailer) {
			outbound = get_current_outbound(addr, /* fileboxes: */true);
			if(nodecfg!=NULL && nodecfg->outbox[0])
				SAFECOPY(packet, pktname(outbound));
			else {
				ch='o';
				if(nodecfg!=NULL) {
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
				if(write_flofile(hdr.subj,addr,/* bundle: */false,/* use_outbox: */false))
					bail(1);
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
			new_pkthdr(&pkthdr, fmsghdr_srcaddr(&hdr), addr, nodecfg);
			fwrite(&pkthdr,sizeof(pkthdr),1,stream); 
		} else
			fseek(stream,find_packet_terminator(stream),SEEK_SET);

		lprintf(LOG_DEBUG, "Adding NetMail (%s) to %spacket for %s: %s"
			,getfname(path)
			,newpkt, smb_faddrtoa(&addr, NULL), packet);
		putfmsg(stream,fmsgbuf,hdr,fakearea,msg_seen,msg_path);

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
	long	flen;
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

		if((fp=fopen(path,"rb"))==NULL) {
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
			lprintf(LOG_ERR,"ERROR reading header (%u bytes) from stray packet: %s"
				,sizeof(pkthdr), packet);
			fclose(fp);
			delfile(packet, __LINE__);
			continue; 
		}
		terminator = ~FIDO_PACKET_TERMINATOR;
		fseek(fp, flen-sizeof(terminator), SEEK_SET);
		fread(&terminator, sizeof(terminator), 1, fp);
		fclose(fp);
		if(!parse_pkthdr(&pkthdr, &pkt_orig, &pkt_dest, &pkt_type)) {
			lprintf(LOG_WARNING,"Failure to parse packet (%s) header, type: %u"
				,packet, pkthdr.type2.pkttype);
			delfile(packet, __LINE__);
			continue;
		}
		lprintf(LOG_WARNING,"Stray Outbound Packet (type %s) found, from %s to %s: %s"
			,pktTypeStringList[pkt_type], smb_faddrtoa(&pkt_orig, NULL), smb_faddrtoa(&pkt_dest, str), packet); 
		outpkt_t* pkt;
		if((pkt = calloc(1, sizeof(outpkt_t))) == NULL)
			continue;
		if((pkt->filename = strdup(packet)) == NULL)
			continue;
		if(terminator == FIDO_PACKET_TERMINATOR)
			lprintf(LOG_DEBUG, "Stray packet already finalized: %s", packet);
		else
			if((pkt->fp = fopen(pkt->filename, "ab")) == NULL)
				continue;
		pkt->orig = pkt_orig;
		pkt->dest = pkt_dest;
		listAddNode(&outpkt_list, pkt, 0, LAST_NODE);
	}
	if(g.gl_pathc)
		lprintf(LOG_DEBUG, "%u stray outbound packets (%u total pkts) found in %s"
			,listCountNodes(&outpkt_list), g.gl_pathc, cfg.temp_dir);
	globfree(&g);
}

bool rename_bad_packet(const char* packet)
{
	lprintf(LOG_WARNING, "Bad packet detected: %s", packet);
	char badpkt[MAX_PATH+1];
	SAFECOPY(badpkt, packet);
	char* ext = getfext(badpkt);
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
		fseek(fidomsg, -(int)sizeof(terminator), SEEK_END);
		fread(&terminator,sizeof(terminator),1,fidomsg);
		if(terminator != FIDO_PACKET_TERMINATOR) {
			fclose(fidomsg);
			lprintf(LOG_WARNING,"WARNING: packet %s not terminated correctly (0x%04hX)", packet, terminator);
			rename_bad_packet(packet);
			continue; 
		}
		fseek(fidomsg,0L,SEEK_SET);
		if(fread(&pkthdr,sizeof(pkthdr),1,fidomsg)!=1) {
			fclose(fidomsg);
			lprintf(LOG_ERR,"ERROR line %d reading %u bytes from %s",__LINE__
				,sizeof(pkthdr),packet);
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

			bool grunged=false;
			off_t msg_offset = ftell(fidomsg);
			/* Read fixed-length header fields */
			if(fread(&pkdmsg,sizeof(BYTE),sizeof(pkdmsg),fidomsg)!=sizeof(pkdmsg))
				continue;
				
			if(pkdmsg.type == 2) { /* Recognized type, copy fields */
				hdr.orignode	= pkdmsg.orignode;
				hdr.destnode	= pkdmsg.destnode;
				hdr.orignet		= pkdmsg.orignet;
				hdr.destnet		= pkdmsg.destnet;
				hdr.attr		= pkdmsg.attr;
				hdr.cost		= pkdmsg.cost;
				SAFECOPY(hdr.time,pkdmsg.time);
			} else
				grunged=true;

			/* Read variable-length header fields */
			if(!grunged) {
				freadstr(fidomsg,hdr.to,sizeof(hdr.to));
				freadstr(fidomsg,hdr.from,sizeof(hdr.from));
				freadstr(fidomsg,hdr.subj,sizeof(hdr.subj));
			}
			hdr.attr&=~FIDO_LOCAL;	/* Strip local bit, obviously not created locally */

			str[0]=0;
			for(i=0;!grunged && i<sizeof(str);i++)	/* Read in the 'AREA' Field */
				if(!fread(str+i,1,1,fidomsg) || str[i]=='\r')
					break;
			if(i<sizeof(str))
				str[i]=0;
			else
				grunged=1;

			if(grunged) {
				lprintf(LOG_NOTICE, "Grunged message (type %d) from %s at offset %u in packet: %s"
					,pkdmsg.type, smb_faddrtoa(&pkt_orig,NULL), msg_offset, packet);
				seektonull(fidomsg);
				printf("Grunged message!\n");
				bad_packet = true;
				continue; 
			}

			if(i)
				fseek(fidomsg,(long)-(i+1),SEEK_CUR);

			truncsp(str);
			p=strstr(str,"AREA:");
			if(p!=str) {					/* Netmail */
				long pos = ftell(fidomsg);
				import_netmail("", hdr, fidomsg, inbound);
				fseek(fidomsg, pos, SEEK_SET);
				seektonull(fidomsg);
				printf("\n");
				continue; 
			}

			if(!opt_import_echomail) {
				printf("EchoMail Ignored");
				seektonull(fidomsg);
				printf("\n");
				continue; 
			}

			p+=5;								/* Skip "AREA:" */
			SKIP_WHITESPACE(p);					/* Skip any white space */
			printf("%21s: ",p);                 /* Show areaname: */
			SAFECOPY(areatag,p);

			if(!secure && (nodecfg == NULL || nodecfg->pktpwd[0] == 0)) {
				lprintf(LOG_WARNING, "Unauthenticated %s EchoMail from %s ignored"
					,areatag, smb_faddrtoa(&pkt_orig, NULL));
				seektonull(fidomsg);
				printf("\n");
				bad_packet = true;
				continue; 
			}

			for(i=0;i<cfg.areas;i++)				/* Do we carry this area? */
				if(stricmp(cfg.area[i].name, areatag) == 0) {
					if(cfg.area[i].sub!=INVALID_SUB)
						printf("%s ",scfg.sub[cfg.area[i].sub]->code);
					else
						printf("(Passthru) ");
					fmsgbuf=getfmsg(fidomsg,NULL);
					gen_psb(&msg_seen,&msg_path,fmsgbuf,pkt_orig.zone);	/* was destzone */
					break; 
				}

			if(i==cfg.areas) {
				printf("(Unknown) ");
				if(cfg.badecho>=0) {
					i=cfg.badecho;
					if(cfg.area[i].sub!=INVALID_SUB)
						printf("%s ",scfg.sub[cfg.area[i].sub]->code);
					else
						printf("(Passthru) ");
					fmsgbuf=getfmsg(fidomsg,NULL);
					gen_psb(&msg_seen,&msg_path,fmsgbuf,pkt_orig.zone);	/* was destzone */
				}
				else {
					printf("Skipped\n");
					seektonull(fidomsg);
					continue; 
				} 
			}

			if(cfg.secure_echomail && cfg.area[i].sub!=INVALID_SUB) {
				if(!area_is_linked(i,&pkt_orig)) {
					lprintf(LOG_WARNING, "%s: Security violation - %s not in AREAS.BBS"
						,areatag,smb_faddrtoa(&pkt_orig,NULL));
					printf("Security Violation (Not in AREAS.BBS)\n");
					seektonull(fidomsg);
					bad_packet = true;
					continue; 
				} 
			}

			/* From here on out, i = area number and area[i].sub = sub number */

			memcpy(&curarea,&cfg.area[i],sizeof(area_t));
			curarea.name=areatag;

			if(cfg.area[i].sub==INVALID_SUB) {			/* Passthru */
				strip_psb(fmsgbuf);
				pkt_to_pkt(fmsgbuf,curarea,NULL,hdr,msg_seen,msg_path);
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
				if((j=smb_open(&smb[cur_smb]))!=SMB_SUCCESS) {
					sprintf(str,"ERROR %d opening %s area #%d, sub #%d)"
						,j,smb[cur_smb].file,i+1,cfg.area[i].sub+1);
					fputs(str,stdout);
					lprintf(LOG_ERR, "%s", str);
					strip_psb(fmsgbuf);
					pkt_to_pkt(fmsgbuf,curarea,NULL,hdr,msg_seen,msg_path);
					printf("\n");
					continue; 
				}
				if(!filelength(fileno(smb[cur_smb].shd_fp))) {
					smb[cur_smb].status.max_crcs=scfg.sub[cfg.area[i].sub]->maxcrcs;
					smb[cur_smb].status.max_msgs=scfg.sub[cfg.area[i].sub]->maxmsgs;
					smb[cur_smb].status.max_age=scfg.sub[cfg.area[i].sub]->maxage;
					smb[cur_smb].status.attr=scfg.sub[cfg.area[i].sub]->misc&SUB_HYPER
							? SMB_HYPERALLOC:0;
					if((j=smb_create(&smb[cur_smb]))!=SMB_SUCCESS) {
						sprintf(str,"ERROR %d creating %s",j,smb[cur_smb].file);
						fputs(str,stdout);
						lprintf(LOG_ERR, "%s", str);
						smb_close(&smb[cur_smb]);
						strip_psb(fmsgbuf);
						pkt_to_pkt(fmsgbuf,curarea,NULL,hdr,msg_seen,msg_path);
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
				pkt_to_pkt(fmsgbuf,curarea,NULL,hdr,msg_seen,msg_path);
				printf("\n");
				continue; 
			}

			if(!(hdr.attr&FIDO_PRIVATE) && (scfg.sub[cfg.area[i].sub]->misc&SUB_PONLY))
				hdr.attr|=MSG_PRIVATE;

			/**********************/
			/* Importing EchoMail */
			/**********************/
			j=fmsgtosmsg(fmsgbuf,hdr,0,cfg.area[i].sub);

			if(j==SMB_DUPE_MSG) {
				lprintf(LOG_NOTICE, "%s Duplicate message from %s (%s) to %s, subject: %s"
					,areatag, hdr.from, fmsghdr_srcaddr_str(&hdr), hdr.to, hdr.subj);
				cfg.area[i].dupes++; 
			}
			else {	   /* Not a dupe */
				strip_psb(fmsgbuf);
				pkt_to_pkt(fmsgbuf,curarea,NULL,hdr,msg_seen,msg_path); 
			}

			if(j==0) {		/* Successful import */
				user_t user;
				if(i!=cfg.badecho && cfg.echomail_notify && (user.number=matchname(hdr.to))!=0
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
			}
			printf("\n");
		}
		fclose(fidomsg);

		if(bad_packet)
			rename_bad_packet(packet);
		else if(cfg.delete_packets)
			delfile(packet, __LINE__);
	}
	globfree(&g);
}

/***********************************/
/* Synchronet/FidoNet Message util */
/***********************************/
int main(int argc, char **argv)
{
	FILE*	fidomsg;
	char	str[1025],path[MAX_PATH+1]
			,sub_code[LEN_EXTCODE+1]
			,*p,*tp;
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
	" * Generate AreaFix netmail reports/notifications for links (-g to enable)\n"
	" * Prompt for key-press upon normal exit (-@ to enable)\n"
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

	sscanf("$Revision$", "%*s %s", revision);

	DESCRIBE_COMPILER(compiler);

	printf("\nSBBSecho v%u.%02u-%s (rev %s) - Synchronet FidoNet EchoMail Tosser\n"
		,SBBSECHO_VERSION_MAJOR, SBBSECHO_VERSION_MINOR
		,PLATFORM_DESC
		,revision
		);

	sub_code[0]=0;

	cmdline[0]=0;
	for(i=1;i<argc;i++) {
		sprintf(cmdline+strlen(cmdline), "%s ", argv[i]);
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
					default:
						fprintf(stderr, "Unsupported option: %c\n", argv[i][j]);
					case '?':
						printf("%s", usage);
						bail(0); 
					case 'A':
					case 'B':
					case 'D':
					case 'F':
					case 'J':
					case 'L':
					case 'O':
					case 'R':
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
			else if(isdigit((uchar)argv[i][0]))
				nodeaddr = atofaddr(argv[i]);
			else
				SAFECOPY(sub_code,argv[i]); 
		}  
	}

	if(!opt_import_echomail && !opt_import_netmail)
		opt_import_packets = false;

	p=getenv("SBBSCTRL");
	if(p==NULL) {
		printf("\7\nSBBSCTRL environment variable not set.\n");
		bail(1); 
		return -1;
	}
	SAFECOPY(scfg.ctrl_dir,p);

	backslash(scfg.ctrl_dir);
	SAFEPRINTF(path,"%ssbbsecho.bsy", scfg.ctrl_dir);
	if(!fmutex(path, program_id(), cfg.bsy_timeout)) {
		lprintf(LOG_WARNING, "Mutex file exists (%s): SBBSecho appears to be already running", path);
		bail(1);
	}
	mtxfile_locked = true;
	atexit(cleanup);

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
	if(!load_cfg(&scfg, NULL, true, str)) {
		fprintf(stderr,"!ERROR %s\n",str);
		fprintf(stderr,"!Failed to load configuration files\n");
		bail(1);
		return -1;
	}

	SAFEPRINTF(str,"%stwitlist.cfg",scfg.ctrl_dir);
	twit_list=fexist(str);

	if(scfg.total_faddrs)
		sys_faddr=scfg.faddr[0];

	if(!cfg.cfgfile[0])
		SAFEPRINTF(cfg.cfgfile,"%ssbbsecho.ini",scfg.ctrl_dir);

	if(!sbbsecho_read_ini(&cfg)) {
		fprintf(stderr, "ERROR %d (%s) reading %s\n", errno, strerror(errno), cfg.cfgfile);
		bail(1);
	}
	if(!sbbsecho_read_ftn_domains(&cfg, scfg.ctrl_dir)) {
		fprintf(stderr, "ERROR %d (%s) reading %sftn_domains.ini\n", errno, strerror(errno), scfg.ctrl_dir);
		bail(1);
	}

	if(nodeaddr.zone && (nodecfg = findnodecfg(&cfg, nodeaddr, /* exact: */true)) == NULL) {
		fprintf(stderr, "Invalid node address: %s\n", argv[i]);
		bail(1);
	}

	if((fidologfile=fopen(cfg.logfile,"a"))==NULL) {
		fprintf(stderr,"ERROR %u (%s) line %d opening %s\n",errno,strerror(errno),__LINE__,cfg.logfile);
		bail(1); 
		return -1;
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
	backslash(cfg.inbound);
	if(cfg.secure_inbound[0])
		backslash(cfg.secure_inbound);

	truncsp(cmdline);
	lprintf(LOG_DEBUG,"%s invoked with options: %s", sbbsecho_pid(), cmdline);

	/******* READ IN AREAS.BBS FILE *********/

	printf("Reading %s",cfg.areafile);
	if((stream=fopen(cfg.areafile,"r"))==NULL) {
		lprintf(LOG_ERR,"ERROR %u (%s) line %d opening %s",errno,strerror(errno),__LINE__,cfg.areafile);
		bail(1); 
		return -1;
	}
	cfg.areas=0;		/* Total number of areas in AREAS.BBS */
	cfg.area=NULL;
	while(!terminated) {
		if(!fgets(str,sizeof(str),stream))
			break;
		truncsp(str);
		p=str;
		SKIP_WHITESPACE(p);	/* Find first printable char */
		if(*p==';' || !*p)          /* Ignore blank lines or start with ; */
			continue;
		if((cfg.area=(area_t *)realloc(cfg.area,sizeof(area_t)*(cfg.areas+1)))==NULL) {
			lprintf(LOG_ERR,"ERROR allocating memory for area #%u.",cfg.areas+1);
			bail(1); 
			return -1;
		}
		memset(&cfg.area[cfg.areas],0,sizeof(area_t));

		cfg.area[cfg.areas].sub=INVALID_SUB;	/* Default to passthru */

		sprintf(tmp,"%-.*s",LEN_EXTCODE,p);
		tp=tmp;
		FIND_WHITESPACE(tp);
		*tp=0;
		for(i=0;i<scfg.total_subs;i++)
			if(!stricmp(tmp,scfg.sub[i]->code))
				break;
		if(i<scfg.total_subs)
			cfg.area[cfg.areas].sub=i;
		else if(stricmp(tmp,"P")) {
			lprintf(LOG_WARNING,"%s: Unrecognized internal code, assumed passthru",tmp); 
		}

		FIND_WHITESPACE(p);				/* Skip code */
		SKIP_WHITESPACE(p);				/* Skip white space */
		SAFECOPY(tmp,p);		       /* Area tag */
		truncstr(tmp,"\t ");
		strupr(tmp);
		if(tmp[0]=='*')         /* UNKNOWN-ECHO area */
			cfg.badecho=cfg.areas;
		if((cfg.area[cfg.areas].name=strdup(tmp))==NULL) {
			lprintf(LOG_ERR,"ERROR allocating memory for area #%u tag name."
				,cfg.areas+1);
			bail(1); 
			return -1;
		}
		cfg.area[cfg.areas].tag=crc32(tmp,0);

		FIND_WHITESPACE(p);		/* Skip tag */
		SKIP_WHITESPACE(p);		/* Skip white space */

		while(*p && *p!=';') {
			if(!isdigit(*p)) {
				lprintf(LOG_WARNING, "Invalid Area File line, expected link address(es) after echo-tag: '%s'", str);
				break;
			}
			if((cfg.area[cfg.areas].link=(fidoaddr_t *)
				realloc(cfg.area[cfg.areas].link
				,sizeof(fidoaddr_t)*(cfg.area[cfg.areas].links+1)))==NULL) {
				lprintf(LOG_ERR,"ERROR allocating memory for area #%u links."
					,cfg.areas+1);
				bail(1); 
				return -1;
			}
			fidoaddr_t link = atofaddr(p);
			cfg.area[cfg.areas].link[cfg.area[cfg.areas].links] = link;
			if(findnodecfg(&cfg, link, /* exact: */false) == NULL)
				lprintf(LOG_WARNING, "Configuration for %s-linked-node (%s) not found in %s"
					,cfg.area[cfg.areas].name, faddrtoa(&link), cfg.cfgfile);
			else
				cfg.area[cfg.areas].links++; 
			FIND_WHITESPACE(p);	/* Skip address */
			SKIP_WHITESPACE(p);	/* Skip white space */
		}

		if(cfg.area[cfg.areas].sub!=INVALID_SUB || cfg.area[cfg.areas].links)
			cfg.areas++;		/* Don't allocate if no tossing */
	}
	fclose(stream);

	printf("\n");

	if(opt_gen_notify_list && !terminated) {
		lprintf(LOG_DEBUG,"Generating AreaFix Notifications...");
		gen_notify_list(); 
	}

	find_stray_packets();

	if(opt_import_packets) {

		printf("\nScanning for Inbound Packets...\n");

		/* We want to loop while there are bundles waiting for us, but first we want */
		/* to take care of any packets that may already be hanging around for some	 */
		/* reason or another (thus the do/while loop) */

		for(i=0; i<cfg.nodecfgs && !terminated; i++) {
			if(cfg.nodecfg[i].inbox[0] == 0)
				continue;
			printf("Scanning %s\n", cfg.nodecfg[i].inbox);
			do {
				import_packets(cfg.nodecfg[i].inbox, &cfg.nodecfg[i], /* secure: */true);
			} while(unpack_bundle(cfg.nodecfg[i].inbox));
		}
		if(cfg.secure_inbound[0] && !terminated) {
			printf("Scanning %s\n", cfg.secure_inbound);
			do { 
				import_packets(cfg.secure_inbound, NULL, /* secure: */true);
			} while(unpack_bundle(cfg.secure_inbound));
		}
		if(cfg.inbound[0] && !terminated) {
			printf("Scanning %s\n", cfg.inbound);
			do {
				import_packets(cfg.inbound, NULL, /* secure: */false);
			} while(unpack_bundle(cfg.inbound));
		}

		for(j=MAX_OPEN_SMBS-1;(int)j>=0;j--)		/* Close open bases */
			if(smb[j].shd_fp)
				smb_close(&smb[j]);

		/******* END OF IMPORT PKT ROUTINE *******/

		for(i=0;i<cfg.areas;i++) {
			if(cfg.area[i].imported)
				lprintf(LOG_INFO,"Imported: %5u msgs %*s <- %s"
					,cfg.area[i].imported,LEN_EXTCODE,scfg.sub[cfg.area[i].sub]->code
					,cfg.area[i].name); 
		}
		for(i=0;i<cfg.areas;i++) {
			if(cfg.area[i].circular)
				lprintf(LOG_INFO,"Circular: %5u detected in %s"
					,cfg.area[i].circular,cfg.area[i].name); 
		}
		for(i=0;i<cfg.areas;i++) {
			if(cfg.area[i].dupes)
				lprintf(LOG_INFO,"Duplicate: %4u detected in %s"
					,cfg.area[i].dupes,cfg.area[i].name); 
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
			i=import_netmail(path,hdr,fidomsg,cfg.inbound);
			/**************************************/
			/* Delete source netmail if specified */
			/**************************************/
			if(i==0) {
				if(cfg.delete_netmail) {
					fclose(fidomsg);
					delfile(path, __LINE__);
				}
				else {
					hdr.attr|=FIDO_RECV;
					fseek(fidomsg,0L,SEEK_SET);
					fwrite(&hdr,sizeof(fmsghdr_t),1,fidomsg);
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

		for(j=0; j<cfg.areas; j++) {
			uint32_t lastmsg;
			if(j==cfg.badecho)	/* Don't scan the bad-echo area */
				continue;
			i=cfg.area[j].sub;
			if(i<0 || i>=scfg.total_subs)	/* Don't scan pass-through areas */
				continue;
			lprintf(LOG_DEBUG,"\n%-*.*s -> %s"
				,LEN_EXTCODE, LEN_EXTCODE, scfg.sub[i]->code, cfg.area[j].name);
			if(getlastmsg(i,&lastmsg,0) >= 0)
				write_export_ptr(i, lastmsg, cfg.area[j].name);
		}
	}

	move_echomail_packets();
	if(nodecfg != NULL)
		attachment(NULL,nodecfg->addr,ATTACHMENT_NETMAIL);

	if(email->shd_fp)
		smb_close(email);

	free(smb);
	free(email);

	if(cfg.outgoing_sem[0]) {
		if (exported_netmail || exported_echomail || packed_netmail)
			ftouch(cfg.outgoing_sem);
	}
	bail(0);
	return(0);
}
