/* sbbsecho.c */

/* Synchronet FidoNet EchoMail Scanning/Tossing and NetMail Tossing Utility */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2003 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#ifdef _WIN32
	#include <windows.h>
#endif

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef __WATCOMC__
	#include <mem.h>
#endif

#ifndef __unix__
	#include <malloc.h>
#endif

#include "conwrap.h"		/* getch() */
#include "sbbs.h"			/* load_cfg() */
#include "sbbsdefs.h"
#include "smblib.h"
#include "scfglib.h"
#include "lzh.h"
#include "sbbsecho.h"

smb_t *smb,*email;
long misc=(IMPORT_PACKETS|IMPORT_NETMAIL|IMPORT_ECHOMAIL|EXPORT_ECHOMAIL
			|DELETE_NETMAIL|DELETE_PACKETS);
ulong netmail=0;
char tmp[256],pkt_type=0;
int secure,cur_smb=0;
FILE *fidologfile=NULL;
two_two_t two_two;
two_plus_t two_plus;
BOOL twit_list;

faddr_t		sys_faddr;
config_t	cfg;
scfg_t		scfg;
char		revision[16];
char		compiler[32];

BOOL pause_on_exit=FALSE;

#ifdef __WATCOMC__
/******************************************************************************
 A DOS to Unix function - because WATCOM doesn't have one
******************************************************************************/
time_t dostounix(struct date *d,struct time *t)
{
	struct tm tm;

	tm.tm_sec=t->ti_sec;
	tm.tm_min=t->ti_min;
	tm.tm_hour=t->ti_hour;
	tm.tm_mday=d->da_day;
	tm.tm_mon=(d->da_mon-1);
	tm.tm_year=d->da_year-1900;
	tm.tm_isdst=0;

	return(mktime(&tm));
}
#endif

#ifndef __NT__
#define delfile(x) remove(x)
#else
int delfile(char *filename)
{
	int i=0;

	while(remove(filename) && i++<120)	/* Wait up to 60 seconds to delete file */
		delay(500); 					/* for Win95 bug fix */
	return(i);
}
#endif

#if defined(__unix__)	/* borrowed from MSVC */
unsigned _rotr (
        unsigned val,
        int shift
        )
{
    register unsigned lobit;        /* non-zero means lo bit set */
    register unsigned num = val;    /* number to rotate */

    shift &= 0x1f;                  /* modulo 32 -- this will also make
                                       negative shifts work */
    while (shift--) {
	    lobit = num & 1;        /* get high bit */
        num >>= 1;              /* shift right one bit */
        if (lobit)
			num |= 0x80000000;  /* set hi bit if lo bit was set */
	}

    return num;
}
#endif

/******************************************************************************
 Displays characters locally
******************************************************************************/
long lputs(char *str)
{
    char tmp[256];
    int i,j,k;


	j=strlen(str);
	for(i=k=0;i<j;i++)      /* remove CRs */
		if(str[i]==CR && str[i+1]==LF)
			continue;
		else
			tmp[k++]=str[i];
	tmp[k]=0;
	return(fputs(tmp,stdout));
}

/****************************************************************************/
/* Performs printf() through local assembly routines                        */
/* Called from everywhere                                                   */
/****************************************************************************/
int lprintf(char *fmat, ...)
{
	va_list argptr;
	char sbuf[256];
	int chcount;

	va_start(argptr,fmat);
	chcount=vsnprintf(sbuf,sizeof(sbuf),fmat,argptr);
	sbuf[sizeof(sbuf)-1]=0;
	va_end(argptr);
	lputs(sbuf);
	return(chcount);
}

/**********************/
/* Log print function */
/**********************/
void logprintf(char *str, ...)
{
    va_list argptr;
    char buf[256];
    time_t now;
    struct tm *gm;

	if(!(misc&LOGFILE) || fidologfile==NULL)
		return;
	va_start(argptr,str);
	vsnprintf(buf,sizeof(buf),str,argptr);
	buf[sizeof(buf)-1]=0;
	va_end(argptr);
	now=time(NULL);
	gm=localtime(&now);
	fseek(fidologfile,0L,SEEK_END);
	fprintf(fidologfile,"%02u/%02u/%02u %02u:%02u:%02u %s\r\n"
		,gm->tm_mon+1,gm->tm_mday,TM_YEAR(gm->tm_year),gm->tm_hour,gm->tm_min,gm->tm_sec
		,buf);
	fflush(fidologfile);
}

/*****************************************************************************/
/* Returns command line generated from instr with %c replacments             */
/*****************************************************************************/
char *cmdstr(scfg_t* cfg, char *instr, char *fpath, char *fspec)
{
    static char cmd[128];
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
						strcat(cmd,str); }
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
						strcat(cmd,str); }
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
					if(cfg->exec_dir[0]!='\\' 
						&& cfg->exec_dir[0]!='/' 
						&& cfg->exec_dir[1]!=':') {
						strcpy(str,cfg->node_dir);
						strcat(str,cfg->exec_dir);
						if(FULLPATH(str2,str,40))
							strcpy(str,str2);
						backslash(str);
						strcat(cmd,str); }
					else
						strcat(cmd,cfg->exec_dir);
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
				default:    /* unknown specification */
					printf("ERROR Checking Command Line '%s'\n",instr);
					logprintf("ERROR line %d Checking Command Line '%s'",__LINE__
						,instr);
					bail(1);
					break; }
			j=strlen(cmd); }
		else
			cmd[j++]=instr[i]; }
	cmd[j]=0;

	return(cmd);
}

/****************************************************************************/
/* Runs an external program directly using spawnvp							*/
/****************************************************************************/
int execute(char *cmdline)
{
#if 1
	return system(cmdline);
#else
	char c,d,e,cmdlen,*arg[30],str[256];
	int i;

	strcpy(str,cmdline);
	arg[0]=str;	/* point to the beginning of the string */
	cmdlen=strlen(str);
	for(c=0,d=1,e=0;c<cmdlen;c++,e++)	/* Break up command line */
		if(str[c]==SP) {
			str[c]=0;			/* insert nulls */
			arg[d++]=str+c+1;	/* point to the beginning of the next arg */
			e=0; }
	arg[d]=0;
	i=spawnvp(P_WAIT,arg[0],arg);
	return(i);
#endif
}

/******************************************************************************
 Returns the system address with the same zone as the address passed
******************************************************************************/
faddr_t getsysfaddr(short zone)
{
	int i;
	faddr_t sysfaddr;

	sysfaddr.zone=sysfaddr.net=sysfaddr.node=1;
	sysfaddr.point=0;
	if(!scfg.total_faddrs)
		return(sys_faddr);
	sysfaddr=scfg.faddr[0];
	if(scfg.total_faddrs==1)
		return(sysfaddr);
	for(i=0;i<scfg.total_faddrs;i++)
		if(scfg.faddr[i].zone==zone)
			return(scfg.faddr[i]);
	return(sysfaddr);
}
/******************************************************************************
 This function creates or appends on existing Binkley compatible .?LO file
 attach file.
 Returns 0 on success.
******************************************************************************/
int write_flofile(char *attachment, faddr_t dest)
{
	char fname[MAX_PATH+1];
	char outbound[MAX_PATH+1];
	char str[MAX_PATH+1];
	char ch;
	char searchstr[MAX_PATH+1];
	ushort attr=0;
	int i,file;
	FILE *stream;

	i=matchnode(dest,0);
	if(i<(int)cfg.nodecfgs)
		attr=cfg.nodecfg[i].attr;

	if(attr&ATTR_CRASH) ch='c';
	else if(attr&ATTR_HOLD) ch='h';
	else if(attr&ATTR_DIRECT) ch='d';
	else ch='f';
	if(dest.zone==scfg.faddr[0].zone)		/* Default zone, use default outbound */
		strcpy(outbound,cfg.outbound);
	else {								/* Inter-zone outbound is OUTBOUND.XXX */
		sprintf(outbound,"%.*s.%03x"
			,(int)strlen(cfg.outbound)-1,cfg.outbound,dest.zone);
		MKDIR(outbound);
		backslash(outbound);
	}
	if(dest.point) {					/* Point destination is OUTBOUND\*.PNT */
		sprintf(str,"%04x%04x.pnt"
			,dest.net,dest.node);
		strcat(outbound,str); }
	if(outbound[strlen(outbound)-1]=='\\'
		|| outbound[strlen(outbound)-1]=='/')
		outbound[strlen(outbound)-1]=0;
	MKDIR(outbound);
	backslash(outbound);
	if(dest.point)
		sprintf(fname,"%s%08x.%clo",outbound,dest.point,ch);
	else
		sprintf(fname,"%s%04x%04x.%clo",outbound,dest.net,dest.node,ch);
	sprintf(searchstr,"^%s",attachment);
	if(findstr(searchstr,fname))	/* file already in FLO file */
		return(0);
	if((stream=fnopen(&file,fname,O_WRONLY|O_APPEND|O_CREAT))==NULL) {
		printf("\7ERROR line %d opening %s %s\n",__LINE__,fname,strerror(errno));
		logprintf("ERROR line %d opening %s %s",__LINE__,fname,strerror(errno));
		return(-1); 
	}
	fprintf(stream,"^%s\r\n",attachment);
	fclose(stream);
	return(0);
}

/* Writes text buffer to file, expanding sole LFs to CRLFs */
size_t fwrite_crlf(char* buf, size_t len, FILE* fp)
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

/******************************************************************************
 This function will create a netmail message (.MSG format).
 If file is non-zero, will set file attachment bit (for bundles).
 Returns 0 on success.
******************************************************************************/
int create_netmail(char *to, char *subject, char *body, faddr_t dest, BOOL file_attached)
{
	FILE *fstream;
	char str[256],fname[256];
	ushort attr=0;
	int fmsg;
	uint i;
	static uint startmsg;
	time_t t;
	faddr_t	faddr;
	fmsghdr_t hdr;
	struct tm *tm;

	if(!startmsg) startmsg=1;
	i=matchnode(dest,0);
	if(i<cfg.nodecfgs) {
		attr=cfg.nodecfg[i].attr;
		if(!attr) {
			i=matchnode(dest,2);
			if(i<cfg.nodecfgs)
				attr=cfg.nodecfg[i].attr; } }

	do {
		for(i=startmsg;i;i++) {
			sprintf(fname,"%s%u.msg",scfg.netmail_dir,i);
			if(!fexistcase(fname))
				break; 
		}
		if(!i) {
			printf("\7%s directory full!\n",scfg.netmail_dir);
			logprintf("Directory full: %s",scfg.netmail_dir);
			return(-1); }
		startmsg=i+1;
		if((fstream=fnopen(&fmsg,fname,O_RDWR|O_CREAT))==NULL) {
			printf("\7ERROR line %d opening %s %s\n",__LINE__,fname,strerror(errno));
			logprintf("ERROR line %d opening %s %s",__LINE__,fname,strerror(errno));
			return(-1); }

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

		if(attr&ATTR_HOLD)
			hdr.attr|=FIDO_HOLD;
		if(attr&ATTR_CRASH)
			hdr.attr|=FIDO_CRASH;

		sprintf(hdr.from,"SBBSecho");

		t=time(NULL);
		tm=localtime(&t);
		sprintf(hdr.time,"%02u %3.3s %02u  %02u:%02u:%02u"
			,tm->tm_mday,mon[tm->tm_mon],TM_YEAR(tm->tm_year)
			,tm->tm_hour,tm->tm_min,tm->tm_sec);

		if(to)
			sprintf(hdr.to,"%s",to);
		else
			sprintf(hdr.to,"SYSOP");

		sprintf(hdr.subj,"%.71s",subject);

		fwrite(&hdr,sizeof(fmsghdr_t),1,fstream);
		sprintf(str,"\1INTL %hu:%hu/%hu %hu:%hu/%hu\r"
			,hdr.destzone,hdr.destnet,hdr.destnode
			,hdr.origzone,hdr.orignet,hdr.orignode);
		fwrite(str,strlen(str),1,fstream);

		/* Add FSC-53 FLAGS kludge */
		fprintf(fstream,"\1FLAGS");
		if(attr&ATTR_DIRECT)
			fprintf(fstream," DIR");
		if(file_attached) {
			if(misc&TRUNC_BUNDLES)
				fprintf(fstream," TFS");
			else
				fprintf(fstream," KFS");
		}
		fprintf(fstream,"\r");

		if(hdr.destpoint) {
			sprintf(str,"\1TOPT %hu\r",hdr.destpoint);
			fwrite(str,strlen(str),1,fstream); }
		if(hdr.origpoint) {
			sprintf(str,"\1FMPT %hu\r",hdr.origpoint);
			fwrite(str,strlen(str),1,fstream); }
		if(!file_attached || (!(attr&ATTR_DIRECT) && file_attached))
			fwrite_crlf(body,strlen(body)+1,fstream);	/* Write additional NULL */
		else
			fwrite("\0",1,1,fstream);               /* Write NULL */
		fclose(fstream);
	} while(!fexistcase(fname));
	return(0);
}

/******************************************************************************
 This function takes the contents of 'infile' and puts it into a netmail
 message bound for addr.
******************************************************************************/
void file_to_netmail(FILE *infile,char *title,faddr_t addr,char *to)
{
	char *buf,*p;
	long l,m,len;

	l=len=ftell(infile);
	if(len>8192L)
		len=8192L;
	rewind(infile);
	if((buf=(char *)MALLOC(len+1))==NULL) {
		printf("ERROR allocating %lu bytes for file to netmail buffer.\n",len);
		logprintf("ERROR line %d allocating %lu for file to netmail buf",__LINE__
			,len);
		return; }
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
					fseek(infile,-1L,SEEK_CUR); } } }
		if(ftell(infile)<l)
			strcat(buf,"\r\nContinued in next message...\r\n");
		create_netmail(to,title,buf,addr,FALSE); 
	}
	FREE(buf);
}
/******************************************************************************
 This function sends a notify list to applicable nodes, this list includes the
 settings configured for the node, as well as a list of areas the node is
 connected to.
******************************************************************************/
void notify_list(void)
{
	FILE *	tmpf;
	char	str[256];
	uint	i,j,k;

	for(k=0;k<cfg.nodecfgs;k++) {

		if(!(cfg.nodecfg[k].attr&SEND_NOTIFY))
			continue;

		if((tmpf=tmpfile())==NULL) {
			printf("\7ERROR couldn't open tmpfile.\n");
			logprintf("ERROR line %d couldn't open tmpfile",__LINE__);
			return; }

		fprintf(tmpf,"Following are the options set for your system and a list "
			"of areas\r\nyou are connected to.  Please make sure everything "
			"is correct.\r\n\r\n");
		fprintf(tmpf,"Packet Type       %s\r\n"
			,cfg.nodecfg[k].pkt_type==PKT_TWO ? "2"
			:cfg.nodecfg[k].pkt_type==PKT_TWO_TWO ? "2.2":"2+");
		fprintf(tmpf,"Archive Type      %s\r\n"
			,(cfg.nodecfg[k].arctype>cfg.arcdefs) ?
			 "None":cfg.arcdef[cfg.nodecfg[k].arctype].name);
		fprintf(tmpf,"Mail Status       %s\r\n"
			,cfg.nodecfg[k].attr&ATTR_CRASH ? "Crash"
			:cfg.nodecfg[k].attr&ATTR_HOLD ? "Hold" : "None");
		fprintf(tmpf,"Direct            %s\r\n"
			,cfg.nodecfg[k].attr&ATTR_DIRECT ? "Yes":"No");
		fprintf(tmpf,"Passive           %s\r\n"
			,cfg.nodecfg[k].attr&ATTR_PASSIVE ? "Yes":"No");
		fprintf(tmpf,"Remote AreaMgr    %s\r\n\r\n"
			,cfg.nodecfg[k].password[0] ? "Yes" : "No");

		fprintf(tmpf,"Connected Areas\r\n---------------\r\n");
		for(i=0;i<cfg.areas;i++) {
			sprintf(str,"%s\r\n",cfg.area[i].name);
			if(str[0]=='*')
				continue;
			for(j=0;j<cfg.area[i].uplinks;j++)
				if(!memcmp(&cfg.nodecfg[k].faddr,&cfg.area[i].uplink[j]
					,sizeof(faddr_t)))
					break;
			if(j<cfg.area[i].uplinks)
				fprintf(tmpf,"%s",str); }

		if(ftell(tmpf))
			file_to_netmail(tmpf,"SBBSecho Notify List",cfg.nodecfg[k].faddr,0);
		fclose(tmpf); }
}
/******************************************************************************
 This function creates a netmail to addr showing a list of available areas (0),
 a list of connected areas (1), or a list of removed areas (2).
******************************************************************************/
void netmail_arealist(char type,faddr_t addr)
{
	FILE *stream,*tmpf;
	char str[256],temp[256],title[81],match,*p;
	int file,i,j,k,x,y;

	if(!type)
		strcpy(title,"List of Available Areas");
	else if(type==1)
		strcpy(title,"List of Connected Areas");
	else
		strcpy(title,"List of Unlinked Areas");

	if((tmpf=tmpfile())==NULL) {
		printf("\7ERROR couldn't open tmpfile.\n");
		logprintf("ERROR line %d couldn't open tmpfile",__LINE__);
		return; }

	if(type==1 || (type!=1 && !(misc&ELIST_ONLY))) {
		for(i=0;i<cfg.areas;i++) {
			sprintf(str,"%s\r\n",cfg.area[i].name);
			if(type) {
				for(j=0;j<cfg.area[i].uplinks;j++)
					if(!memcmp(&addr,&cfg.area[i].uplink[j],sizeof(faddr_t)))
						break;
				if((type==1 && j<cfg.area[i].uplinks) ||
					(type==2 && j==cfg.area[i].uplinks))
						fprintf(tmpf,"%s",str); }
			else
				fprintf(tmpf,"%s",str); } }

	if(!type) {
		i=matchnode(addr,0);
		if(i<cfg.nodecfgs) {
			for(j=0;j<cfg.listcfgs;j++) {
				match=0;
				for(k=0;k<cfg.listcfg[j].numflags;k++) {
					if(match) break;
					for(x=0;x<cfg.nodecfg[i].numflags;x++)
						if(!stricmp(cfg.listcfg[j].flag[k].flag
							,cfg.nodecfg[i].flag[x].flag)) {
							if((stream=fnopen(&file
								,cfg.listcfg[j].listpath,O_RDONLY))==NULL) {
								printf("\7ERROR couldn't open %s.\n"
									,cfg.listcfg[j].listpath);
								logprintf("ERROR line %d couldn't open %s %s"
									,__LINE__,cfg.listcfg[j].listpath
									,strerror(errno));
								match=1;
								break; }
							while(!feof(stream)) {
								if(!fgets(str,255,stream))
									break;
								truncsp(str);
								strcat(str,"\r\n");
								p=str;
								while(*p && *p<=SP) p++;
								if(*p==';')     /* Ignore Comment Lines */
									continue;
								strcpy(temp,p);
								p=temp;
								while(*p && *p>SP) p++;
								*p=0;
								if(!(misc&ELIST_ONLY)) {
									for(y=0;y<cfg.areas;y++)
										if(!stricmp(cfg.area[y].name,temp))
											break;
									if(y==cfg.areas)
										fprintf(tmpf,"%s",str); }
								else
									fprintf(tmpf,"%s",str); }
							fclose(stream);
							match=1;
							break; } } } } }

	if(!ftell(tmpf))
		create_netmail(NULL,title,"None.",addr,FALSE);
	else
		file_to_netmail(tmpf,title,addr,0);
	fclose(tmpf);
}
/******************************************************************************
 Imitation of Borland's tempnam function because Watcom doesn't have it
******************************************************************************/
char *tempname(char *dir, char *prefix)
{
	char str[256],*p;
	int i;

	for(i=0;i<1000;i++) {
		sprintf(str,"%s%s%03u.$$$",dir,prefix,i);
		if(!fexist(str))
			break; }
	if(i>=1000) {
		logprintf("tempnam: too many files");
		return(NULL); }
	p=malloc(strlen(str)+1);
	if(!p) {
		logprintf("tempnam: couldn't malloc %u",strlen(str)+1);
		return(NULL); }
	strcpy(p,str);
	return(p);
}

int check_elists(char *areatag,faddr_t addr)
{
	FILE *stream;
	char str[1025],quit=0,*p;
	int i,j,k,x,file,match=0;

	i=matchnode(addr,0);
	if(i<cfg.nodecfgs) {
		for(j=0;j<cfg.listcfgs;j++) {
			quit=0;
			for(k=0;k<cfg.listcfg[j].numflags;k++) {
				if(quit) break;
				for(x=0;x<cfg.nodecfg[i].numflags;x++)
					if(!stricmp(cfg.listcfg[j].flag[k].flag
						,cfg.nodecfg[i].flag[x].flag)) {
						if((stream=fnopen(&file
							,cfg.listcfg[j].listpath,O_RDONLY))==NULL) {
							printf("\7ERROR couldn't open %s.\n"
								,cfg.listcfg[j].listpath);
							logprintf("ERROR line %d opening %s"
								,__LINE__,cfg.listcfg[j].listpath);
							quit=1;
							break; }
						while(!feof(stream)) {
							if(!fgets(str,255,stream))
								break;
							truncsp(str);
							strcat(str,"\r\n");
							p=str;
							while(*p && *p<=SP) p++;
							if(*p==';')     /* Ignore Comment Lines */
								continue;
							strcpy(str,p);
							p=str;
							while(*p && *p>SP) p++;
							*p=0;
							if(!stricmp(areatag,str)) {
								match=1;
								break; } }
						fclose(stream);
						quit=1;
						if(match)
							return(match);
						break; } } } }
	return(match);
}
/******************************************************************************
 Used by AREAFIX to add/remove/change areas in the areas file
******************************************************************************/
void alter_areas(area_t add_area,area_t del_area,faddr_t addr)
{
	FILE *nmfile,*afilein,*afileout,*fwdfile;
	char str[1025],fields[1025],field1[81],field2[81],field3[81]
		,drive[3],dir[MAX_PATH+1],name[MAX_PATH+1],ext[MAX_PATH+1],outpath[128]
		,*outname,*p,*tp,nomatch=0,match=0;
	int i,j,k,x,y,file;
	ulong tagcrc;

	_splitpath(cfg.areafile,drive,dir,name,ext);
	sprintf(outpath,"%s%s",drive,dir);
	if((outname=tempname(outpath,"AREAS"))==NULL) {
		printf("\7ERROR creating temp file name for %s.\n",outpath);
		logprintf("ERROR tempnam(%s,AREAS)",outpath);
		return; }
	if((nmfile=tmpfile())==NULL) {
		printf("\7ERROR couldn't open NetMail temp file.\n");
		logprintf("ERROR in tmpfile()");
		free(outname);
		return; }
	if((afileout=fopen(outname,"w+b"))==NULL) {
		printf("\7ERROR couldn't open %s.\n",outname);
		logprintf("ERROR line %d opening %s %s",__LINE__,outname
			,strerror(errno));
		fclose(nmfile);
		free(outname);
		return; }
	if((afilein=fnopen(&file,cfg.areafile,O_RDONLY))==NULL) {
		printf("\7ERROR couldn't open %s.\n",cfg.areafile);
		logprintf("ERROR line %d opening %s %s",__LINE__,cfg.areafile
			,strerror(errno));
		fclose(afileout);
		fclose(nmfile);
		free(outname);
		return; }
	while(!feof(afilein)) {
		if(!fgets(fields,1024,afilein))
			break;
		truncsp(fields);
		strcat(fields,"\r\n");
		p=fields;
		while(*p && *p<=SP) p++;
		if(*p==';') {    /* Skip Comment Lines */
			fprintf(afileout,"%s",fields);
			continue; }
		sprintf(field1,"%-.81s",p);         /* Internal Code Field */
		tp=field1;
		while(*tp && *tp>SP) tp++;
		*tp=0;
		while(*p && *p>SP) p++;
		while(*p && *p<=SP) p++;
		sprintf(field2,"%-.81s",p);         /* Areatag Field */
		tp=field2;
		while(*tp && *tp>SP) tp++;
		*tp=0;
		while(*p && *p>SP) p++;
		while(*p && *p<=SP) p++;
		if((tp=strchr(p,';'))!=NULL) {
			sprintf(field3,"%-.81s",p);     /* Comment Field (if any) */
			while(*tp && *tp>SP) tp++;
			*tp=0; }
		else
			field3[0]=0;
		if(del_area.tags) { 				/* Check for areas to remove */
			for(i=0;i<del_area.tags;i++) {
				if(!stricmp(del_area.tag[i],field2) ||
					!stricmp(del_area.tag[0],"-ALL"))     /* Match Found */
					break; }
			if(i<del_area.tags) {
				for(i=0;i<cfg.areas;i++) {
					if(!stricmp(field2,cfg.area[i].name)) {
						for(j=0;j<cfg.area[i].uplinks;j++)
							if(!memcmp(&cfg.area[i].uplink[j],&addr
								,sizeof(faddr_t)))
								break;
						if(j==cfg.area[i].uplinks &&
							stricmp(del_area.tag[0],"-ALL")) {
							fprintf(afileout,"%s",fields);
							fprintf(nmfile,"%s not connected.\r\n",field2);
							break; }

						/* Added 12/4/95 to remove uplink from connected uplinks */

						for(k=j;k<cfg.area[i].uplinks-1;k++)
							memcpy(&cfg.area[i].uplink[k],&cfg.area[i].uplink[k+1]
								,sizeof(faddr_t));
						--cfg.area[i].uplinks;
						if((cfg.area[i].uplink=(faddr_t *)
							REALLOC(cfg.area[i].uplink,sizeof(faddr_t)
							*(cfg.area[i].uplinks)))==NULL) {
							printf("ERROR allocating memory for area #%u "
								"uplinks.\n",i+1);
							logprintf("ERROR line %d allocating memory for area "
								"#%u uplinks.\n",__LINE__,i+1);
							bail(1); }

						fprintf(afileout,"%-16s%-23s ",field1,field2);
						for(j=0;j<cfg.area[i].uplinks;j++) {
							if(!memcmp(&cfg.area[i].uplink[j],&addr
								,sizeof(faddr_t)))
								continue;
							fprintf(afileout,"%s "
								,faddrtoa(&cfg.area[i].uplink[j],NULL)); }
						if(field3[0])
							fprintf(afileout,"%s",field3);
						fprintf(afileout,"\r\n");
						fprintf(nmfile,"%s removed.\r\n",field2);
						break; } }
				if(i==cfg.areas)			/* Something screwy going on */
					fprintf(afileout,"%s",fields);
				continue; } }				/* Area match so continue on */
		if(add_area.tags) { 				/* Check for areas to add */
			for(i=0;i<add_area.tags;i++)
				if(!stricmp(add_area.tag[i],field2) ||
					!stricmp(add_area.tag[0],"+ALL"))      /* Match Found */
					break;
			if(i<add_area.tags) {
				if(stricmp(add_area.tag[i],"+ALL"))
					add_area.tag[i][0]=0;  /* So we can check other lists */
				for(i=0;i<cfg.areas;i++) {
					if(!stricmp(field2,cfg.area[i].name)) {
						for(j=0;j<cfg.area[i].uplinks;j++)
							if(!memcmp(&cfg.area[i].uplink[j],&addr
								,sizeof(faddr_t)))
								break;
						if(j<cfg.area[i].uplinks) {
							fprintf(afileout,"%s",fields);
							fprintf(nmfile,"%s already connected.\r\n",field2);
							break; }
						if(misc&ELIST_ONLY && !check_elists(field2,addr)) {
							fprintf(afileout,"%s",fields);
							break; }

						/* Added 12/4/95 to add uplink to connected uplinks */

						++cfg.area[i].uplinks;
						if((cfg.area[i].uplink=(faddr_t *)
							REALLOC(cfg.area[i].uplink,sizeof(faddr_t)
							*(cfg.area[i].uplinks)))==NULL) {
							printf("ERROR allocating memory for area #%u "
								"uplinks.\n",i+1);
							logprintf("ERROR line %d allocating memory for area "
								"#%u uplinks.\n",__LINE__,i+1);
							bail(1); }
						memcpy(&cfg.area[i].uplink[j],&addr,sizeof(faddr_t));

						fprintf(afileout,"%-16s%-23s ",field1,field2);
						for(j=0;j<cfg.area[i].uplinks;j++)
							fprintf(afileout,"%s "
								,faddrtoa(&cfg.area[i].uplink[j],NULL));
						if(field3[0])
							fprintf(afileout,"%s",field3);
						fprintf(afileout,"\r\n");
						fprintf(nmfile,"%s added.\r\n",field2);
						break; } }
				if(i==cfg.areas)			/* Something screwy going on */
					fprintf(afileout,"%s",fields);
				continue; } 				/* Area match so continue on */
			nomatch=1; }					/* This area wasn't in there */
		fprintf(afileout,"%s",fields); }    /* No match so write back line */
	fclose(afilein);
	if(nomatch || (add_area.tags && !stricmp(add_area.tag[0],"+ALL"))) {
		i=matchnode(addr,0);
		if(i<cfg.nodecfgs) {
			for(j=0;j<cfg.listcfgs;j++) {
				match=0;
				for(k=0;k<cfg.listcfg[j].numflags;k++) {
					if(match) break;
					for(x=0;x<cfg.nodecfg[i].numflags;x++)
						if(!stricmp(cfg.listcfg[j].flag[k].flag
							,cfg.nodecfg[i].flag[x].flag)) {
							if((fwdfile=tmpfile())==NULL) {
								printf("\7ERROR couldn't open forwarding temp "
									"file.\n");
								logprintf("ERROR line %d opening forward temp "
									"file",__LINE__);
								match=1;
								break; }
							if((afilein=fnopen(&file
								,cfg.listcfg[j].listpath,O_RDONLY))==NULL) {
								printf("\7ERROR couldn't open %s.\n"
									,cfg.listcfg[j].listpath);
								logprintf("ERROR line %d opening %s"
									,__LINE__,cfg.listcfg[j].listpath);
								fclose(fwdfile);
								match=1;
								break; }
							while(!feof(afilein)) {
								if(!fgets(str,255,afilein))
									break;
								truncsp(str);
								strcat(str,"\r\n");
								p=str;
								while(*p && *p<=SP) p++;
								if(*p==';')     /* Ignore Comment Lines */
									continue;
								strcpy(str,p);
								p=str;
								while(*p && *p>SP) p++;
								*p=0;
								if(!stricmp(add_area.tag[0],"+ALL")) {
									sprintf(fields,"%.1024s",str);
									tagcrc=crc32(strupr(fields),0);
									for(y=0;y<cfg.areas;y++)
										if(tagcrc==cfg.area[y].tag)
											break;
									if(y<cfg.areas)
										continue; }
								for(y=0;y<add_area.tags;y++)
									if((!stricmp(add_area.tag[y],str) &&
										add_area.tag[y][0]) ||
										!stricmp(add_area.tag[0],"+ALL"))
										break;
								if(y<add_area.tags) {
									fprintf(afileout,"%-16s%-23s","P",str);
									if(cfg.listcfg[j].forward.zone)
										fprintf(afileout," %s"
											,faddrtoa(&cfg.listcfg[j].forward,NULL));
									fprintf(afileout," %s\r\n",faddrtoa(&addr,NULL));
									fprintf(nmfile,"%s added.\r\n",str);
									if(stricmp(add_area.tag[0],"+ALL"))
										add_area.tag[y][0]=0;
									if(!(cfg.listcfg[j].misc&NOFWD)
										&& cfg.listcfg[j].forward.zone)
										fprintf(fwdfile,"%s\r\n",str); } }
							fclose(afilein);
							if(!(cfg.listcfg[j].misc&NOFWD) && ftell(fwdfile)>0)
								file_to_netmail(fwdfile,cfg.listcfg[j].password
									,cfg.listcfg[j].forward,"Areafix");
							fclose(fwdfile);
							match=1;
							break; } } } } }
	if(add_area.tags && stricmp(add_area.tag[0],"+ALL")) {
		for(i=0;i<add_area.tags;i++)
			if(add_area.tag[i][0])
				fprintf(nmfile,"%s not found.\r\n",add_area.tag[i]); }
	if(!ftell(nmfile))
		create_netmail(NULL,"Area Change Request","No changes made.",addr,FALSE);
	else
		file_to_netmail(nmfile,"Area Change Request",addr,0);
	fclose(nmfile);
	fclose(afileout);
	if(delfile(cfg.areafile))					/* Delete AREAS.BBS */
		logprintf("ERROR line %d removing %s %s",__LINE__,cfg.areafile
			,strerror(errno));
	if(rename(outname,cfg.areafile))		   /* Rename new AREAS.BBS file */
		logprintf("ERROR line %d renaming %s to %s",__LINE__,outname,cfg.areafile);
	free(outname);
}
/******************************************************************************
 Used by AREAFIX to add/remove/change uplink info in the configuration file
 old = the old setting for this option, new = what the setting is changing to
 option = 0 for compression type change
		  1 for areafix password change
		  2 to set this node to passive
		  3 to set this node to active (remove passive)
******************************************************************************/
void alter_config(faddr_t addr, char *old, char *new, int option)
{
	FILE *outfile,*cfgfile;
	char str[257],outpath[128],tmp[257],tmp2[257],*outname,*p,*tp
		,drive[3],dir[MAX_PATH+1],name[MAX_PATH+1],ext[MAX_PATH+1],match=0;
	int i,j,k,file;
	faddr_t taddr;

	i=matchnode(addr,0);				  /* i = config number from here on */
	_splitpath(cfg.cfgfile,drive,dir,name,ext);
	sprintf(outpath,"%s%s",drive,dir);
	if((outname=tempname(outpath,"CFG"))==NULL) {
		printf("\7ERROR creating temporary file name for %s.\n",outpath);
		logprintf("ERROR tempnam(%s,CFG)",outpath);
		return; }
	if((outfile=fopen(outname,"w+b"))==NULL) {
		printf("\7ERROR couldn't open %s.\n",outname);
		logprintf("ERROR line %d opening %s %s",__LINE__,outname
			,strerror(errno));
		free(outname);
		return; }
	if((cfgfile=fnopen(&file,cfg.cfgfile,O_RDONLY))==NULL) {
		printf("\7ERROR couldn't open %s.\n",cfg.cfgfile);
		logprintf("ERROR line %d opening %s",__LINE__,cfg.cfgfile
			,strerror(errno));
		fclose(outfile);
		free(outname);
		return; }

	while(!feof(cfgfile)) {
		if(!fgets(str,256,cfgfile))
			break;
		truncsp(str);
		p=str;
		while(*p && *p<=SP) p++;
		if(*p==';') {
			fprintf(outfile,"%s\r\n",str);
			continue; }
		sprintf(tmp,"%-.25s",p);
		tp=strchr(tmp,SP);
		if(tp)
			*tp=0;								/* Chop off at space */
		strupr(tmp);							/* Convert code to uppercase */
		while(*p>SP) p++;						/* Skip code */
		while(*p && *p<=SP) p++;                /* Skip white space */

		if(option==0 && !strcmp(tmp,"USEPACKER")) {     /* Change Compression */
			if(!*p)
				continue;
			strcpy(tmp2,p);
			p=tmp2;
			while(*p && *p>SP) p++;
			*p=0;
			p++;
			if(!stricmp(new,tmp2)) {   /* Add to new definition */
				fprintf(outfile,"%-10s %s %s %s\r\n",tmp,tmp2
					,faddrtoa(&cfg.nodecfg[i].faddr,NULL)
					,(*p) ? p : "");
				match=1;
				continue; }
			else if(!stricmp(old,tmp2)) {	/* Remove from old def */
				for(j=k=0;j<cfg.nodecfgs;j++) {
					if(j==i)
						continue;
					if(!stricmp(cfg.arcdef[cfg.nodecfg[j].arctype].name,tmp2)) {
						if(!k) {
							fprintf(outfile,"%-10s %s",tmp,tmp2);
							k++; }
						fprintf(outfile," %s"
							,faddrtoa(&cfg.nodecfg[j].faddr,NULL)); } }
				fprintf(outfile,"\r\n");
				continue; } }

		if(option==1 && !strcmp(tmp,"AREAFIX")) {       /* Change Password */
			if(!*p)
				continue;
			taddr=atofaddr(p);
			if(!memcmp(&cfg.nodecfg[i].faddr,&taddr,sizeof(faddr_t))) {
				while(*p && *p>SP) p++; 	/* Skip over address */
				while(*p && *p<=SP) p++;	/* Skip over whitespace */
				while(*p && *p>SP) p++; 	/* Skip over password */
				while(*p && *p<=SP) p++;	/* Skip over whitespace */
				fprintf(outfile,"%-10s %s %s %s\r\n",tmp
					,faddrtoa(&cfg.nodecfg[i].faddr,NULL),new,p);
				continue; } }

		if(option>1 && !strcmp(tmp,"PASSIVE")) {        /* Toggle Passive Areas */
			match=1;
			for(j=k=0;j<cfg.nodecfgs;j++) {
				if(option==2 && j==i) {
					if(!k) fprintf(outfile,"%-10s",tmp);
					fprintf(outfile," %s",faddrtoa(&cfg.nodecfg[j].faddr,NULL));
					k++;
					continue; }
				if(option==3 && j==i)
					continue;
				if(cfg.nodecfg[j].attr&ATTR_PASSIVE) {
					if(!k) fprintf(outfile,"%-10s",tmp);
					fprintf(outfile," %s",faddrtoa(&cfg.nodecfg[j].faddr,NULL));
					k++; } }
			if(k) fprintf(outfile,"\r\n");
			continue; }
		fprintf(outfile,"%s\r\n",str); }

	if(!match) {
		if(option==0)
			fprintf(outfile,"%-10s %s %s\r\n","USEPACKER",new
				,faddrtoa(&cfg.nodecfg[i].faddr,NULL));
		if(option==2)
			fprintf(outfile,"%-10s %s\r\n","PASSIVE"
				,faddrtoa(&cfg.nodecfg[i].faddr,NULL)); }

	fclose(cfgfile);
	fclose(outfile);
	if(delfile(cfg.cfgfile))
		logprintf("ERROR line %d removing %s %s",__LINE__,cfg.cfgfile
			,strerror(errno));
	if(rename(outname,cfg.cfgfile))
		logprintf("ERROR line %d renaming %s to %s",__LINE__,outname,cfg.cfgfile);
	free(outname);
}
/******************************************************************************
 Used by AREAFIX to process any '%' commands that come in via netmail
******************************************************************************/
void command(char *instr,faddr_t addr)
{
	FILE *stream,*tmpf;
	char str[256],temp[256],*buf,*p;
	int  file,i,node;
	long l;
	area_t add_area,del_area;

	node=matchnode(addr,0);
	if(node>=cfg.nodecfgs)
		return;
	memset(&add_area,0,sizeof(area_t));
	memset(&del_area,0,sizeof(area_t));
	strupr(instr);
	if((p=strstr(instr,"HELP"))!=NULL) {
		sprintf(str,"%sAREAMGR.HLP",scfg.exec_dir);
		if(!fexistcase(str))
			return;
		if((stream=fnopen(&file,str,O_RDONLY))==NULL) {
			printf("\7ERROR couldn't open %s.\n",str);
			logprintf("ERROR line %d opening %s %s",__LINE__,str
				,strerror(errno));
			return; 
		}
		l=filelength(file);
		if((buf=(char *)LMALLOC(l+1L))==NULL) {
			printf("ERROR line %d allocating %lu bytes for %s\n",__LINE__,l,str);
			return; 
		}
		fread(buf,l,1,stream);
		fclose(stream);
		buf[l]=0;
		create_netmail(NULL,"Area Manager Help",buf,addr,FALSE);
		LFREE(buf);
		return; 
	}

	if((p=strstr(instr,"LIST"))!=NULL) {
		netmail_arealist(0,addr);
		return; 
	}

	if((p=strstr(instr,"QUERY"))!=NULL) {
		netmail_arealist(1,addr);
		return; 
	}

	if((p=strstr(instr,"UNLINKED"))!=NULL) {
		netmail_arealist(2,addr);
		return; 
	}

	if((p=strstr(instr,"COMPRESSION"))!=NULL) {
		while(*p && *p>SP) p++;
		while(*p && *p<=SP) p++;
		for(i=0;i<cfg.arcdefs;i++)
			if(!stricmp(p,cfg.arcdef[i].name))
				break;
		if(!stricmp(p,"NONE"))
			i=0xffff;
		if(i==cfg.arcdefs) {
			if((tmpf=tmpfile())==NULL) {
				printf("\7ERROR couldn't open tmpfile.\n");
				logprintf("ERROR line %d opening tmpfile()",__LINE__);
				return; }
			fprintf(tmpf,"Compression type unavailable.\r\n\r\n"
				"Available types are:\r\n");
			for(i=0;i<cfg.arcdefs;i++)
				fprintf(tmpf,"                     %s\r\n",cfg.arcdef[i].name);
			file_to_netmail(tmpf,"Compression Type Change",addr,0);
			fclose(tmpf);
			//LFREE(buf);
			return; 
		}
		alter_config(addr,cfg.arcdef[cfg.nodecfg[node].arctype].name
			,cfg.arcdef[i].name,0);
		cfg.nodecfg[node].arctype=i;
		sprintf(str,"Compression type changed to %s.",cfg.arcdef[i].name);
		create_netmail(NULL,"Compression Type Change",str,addr,FALSE);
		return; 
	}

	if((p=strstr(instr,"PASSWORD"))!=NULL) {
		while(*p && *p>SP) p++;
		while(*p && *p<=SP) p++;
		sprintf(temp,"%-.25s",p);
		p=temp;
		while(*p && *p>SP) p++;
		*p=0;
		if(node>=cfg.nodecfgs)		   /* Should never happen */
			return;
		if(!stricmp(temp,cfg.nodecfg[node].password)) {
			sprintf(str,"Your password was already set to %s."
				,cfg.nodecfg[node].password);
			create_netmail(NULL,"Password Change Request",str,addr,FALSE);
			return; }
		alter_config(addr,cfg.nodecfg[node].password,temp,1);
		sprintf(str,"Your password has been changed from %s to %.25s."
			,cfg.nodecfg[node].password,temp);
		sprintf(cfg.nodecfg[node].password,"%.25s",temp);
		create_netmail(NULL,"Password Change Request",str,addr,FALSE);
		return; 
	}

	if((p=strstr(instr,"RESCAN"))!=NULL) {
		export_echomail("",addr);
		create_netmail(NULL,"Rescan Areas"
			,"All connected areas carried by your hub have been rescanned."
			,addr,FALSE);
		return; 
	}

	if((p=strstr(instr,"ACTIVE"))!=NULL) {
		if(!(cfg.nodecfg[node].attr&ATTR_PASSIVE)) {
			create_netmail(NULL,"Reconnect Disconnected Areas"
				,"Your areas are already connected.",addr,FALSE);
			return; }
		alter_config(addr,0,0,3);
		create_netmail(NULL,"Reconnect Disconnected Areas"
			,"Temporarily disconnected areas have been reconnected.",addr,FALSE);
		return; 
	}

	if((p=strstr(instr,"PASSIVE"))!=NULL) {
		if(cfg.nodecfg[node].attr&ATTR_PASSIVE) {
			create_netmail(NULL,"Temporarily Disconnect Areas"
				,"Your areas are already temporarily disconnected.",addr,FALSE);
			return; 
		}
		alter_config(addr,0,0,2);
		create_netmail(NULL,"Temporarily Disconnect Areas"
			,"Your areas have been temporarily disconnected.",addr,FALSE);
		return; 
	}

	if((p=strstr(instr,"FROM"))!=NULL);

	if((p=strstr(instr,"+ALL"))!=NULL) {
		if((add_area.tag=(char **)REALLOC(add_area.tag
			,sizeof(char *)*add_area.tags+1))==NULL) {
			printf("ERROR allocating memory for add area tag #%u.\n"
				,add_area.tags+1);
			logprintf("ERROR line %d allocating memory for add area tag #%u"
				,__LINE__,add_area.tags+1);
			bail(1); }
		if((add_area.tag[add_area.tags]=(char *)LMALLOC(strlen(instr)+1))==NULL) {
			printf("ERROR allocating memory for add area tag #%u.\n"
				,add_area.tags+1);
			logprintf("ERROR line %d allocating memory for add area tag #%u"
				,__LINE__,add_area.tags+1);
			bail(1); }
		strcpy(add_area.tag[add_area.tags],instr);
		add_area.tags++;
		alter_areas(add_area,del_area,addr);
		for(i=0;i<add_area.tags;i++)
			LFREE(add_area.tag[i]);
		FREE(add_area.tag);
		return; 
	}

	if((p=strstr(instr,"-ALL"))!=NULL) {
		if((del_area.tag=(char **)REALLOC(del_area.tag
			,sizeof(char *)*del_area.tags+1))==NULL) {
			printf("ERROR allocating memory for del area tag #%u.\n"
				,del_area.tags+1);
			logprintf("ERROR line %d allocating memory for del area tag #%u"
				,__LINE__,del_area.tags+1);
			bail(1); }
		if((del_area.tag[del_area.tags]=(char *)LMALLOC(strlen(instr)+1))==NULL) {
			printf("ERROR allocating memory for del area tag #%u.\n"
				,del_area.tags+1);
			logprintf("ERROR line %d allocating memory for del area tag #%u"
				,__LINE__,del_area.tags+1);
			bail(1); }
		strcpy(del_area.tag[del_area.tags],instr);
		del_area.tags++;
		alter_areas(add_area,del_area,addr);
		for(i=0;i<del_area.tags;i++)
			LFREE(del_area.tag[i]);
		FREE(del_area.tag);
		return; 
	}
}
/******************************************************************************
 This is where we're gonna process any netmail that comes in for areafix.
 Returns text for message body for the local sysop if necessary.
******************************************************************************/
char *process_areafix(faddr_t addr,char HUGE16 *inbuf,char *password)
{
	static char body[512],str[81];
	char *p,*tp,action,percent=0;
	int i;
	ulong l,m;
	area_t add_area,del_area;

	p=(char *)inbuf;

	while(*p==1) {				/* Skip kludge lines 11/05/95 */
		while(*p && *p!=CR)
			p++;				/* Skip meat */
		if(*p)
			p++; }				/* Skip CR */

	if(((tp=strstr(p,"---\r"))!=NULL || (tp=strstr(p,"--- "))!=NULL) &&
		(*(tp-1)==10 || *(tp-1)==13))
		*tp=0;

	if(!strnicmp(p,"%FROM",5)) {    /* Remote Remote Maintenance (must be first) */
		sprintf(str,"%.80s",p+6);
		if((tp=strchr(str,CR))!=NULL)
			*tp=0;
		logprintf("Remote maintenance for %s requested via %s",str
			,faddrtoa(&addr,NULL));
		addr=atofaddr(str); }

	i=matchnode(addr,0);
	if(i>=cfg.nodecfgs) {
		sprintf(body,"Your node is not configured for Areafix, please "
			"contact your hub.\r\n");
		create_netmail(NULL,"Areafix Request",body,addr,FALSE);
		sprintf(body,"An areafix request was made by node %s.\r\nThis node "
			"is not currently configured for areafix.\r\n"
			,faddrtoa(&addr,NULL));
		return(body); }

	if(stricmp(cfg.nodecfg[i].password,password)) {
		create_netmail(NULL,"Areafix Request","Invalid Password.",addr,FALSE);
		sprintf(body,"Node %s attempted an areafix request using an invalid "
			"password.\r\nThe password attempted was %s.\r\nThe correct password "
			"for this node is %s.\r\n",faddrtoa(&addr,NULL),password
			,(cfg.nodecfg[i].password[0]) ? cfg.nodecfg[i].password
			 : "[None Defined]");
		return(body); }

	m=strlen(p);
	add_area.tags=0;
	add_area.tag=NULL;
	del_area.tags=0;
	del_area.tag=NULL;
	for(l=0;l<m;l++) {
		while(*(p+l) && *(p+l)<=SP) l++;
		if(!(*(p+l))) break;
		if(*(p+l)=='+' || *(p+l)=='-' || *(p+l)=='%') {
			action=*(p+l);
			l++; }
		else
			action='+';
		sprintf(str,"%.80s",p+l);
		if((tp=strchr(str,CR))!=NULL)
			*tp=0;
		switch(action) {
			case '+':                       /* Add Area */
				if((add_area.tag=(char **)REALLOC(add_area.tag
					,sizeof(char *)*add_area.tags+1))==NULL) {
					printf("ERROR allocating memory for add area tag #%u.\n"
						,add_area.tags+1);
					logprintf("ERROR line %d allocating memory for add area "
						"tag #%u",__LINE__,add_area.tags+1);
					bail(1); }
				if((add_area.tag[add_area.tags]=(char *)LMALLOC(strlen(str)+1))
					==NULL) {
					printf("ERROR allocating memory for add area tag #%u.\n"
						,add_area.tags+1);
					logprintf("ERROR line %d allocating memory for add area "
						"tag #%u",__LINE__,add_area.tags+1);
					bail(1); }
				strcpy(add_area.tag[add_area.tags],str);
				add_area.tags++;
				break;
			case '-':                       /* Remove Area */
				if((del_area.tag=(char **)REALLOC(del_area.tag
					,sizeof(char *)*del_area.tags+1))==NULL) {
					printf("ERROR allocating memory for del area tag #%u.\n"
						,del_area.tags+1);
					logprintf("ERROR line %d allocating memory for del area "
						"tag #%u",__LINE__,del_area.tags+1);
					bail(1); }
				if((del_area.tag[del_area.tags]=(char *)LMALLOC(strlen(str)+1))
					==NULL) {
					printf("ERROR allocating memory for del area tag #%u.\n"
						,del_area.tags+1);
					logprintf("ERROR line %d allocating memory for del area "
						"tag #%u",__LINE__,del_area.tags+1);
					bail(1); }
				strcpy(del_area.tag[del_area.tags],str);
				del_area.tags++;
				break;
			case '%':                       /* Process Command */
				command(str,addr);
				percent++;
				break; }

		while(*(p+l) && *(p+l)!=CR) l++; }

	if(!percent && !add_area.tags && !del_area.tags) {
		create_netmail(NULL,"Areafix Request","No commands to process.",addr,FALSE);
		sprintf(body,"Node %s attempted an areafix request with an empty message "
			"body or with no valid commands.\r\n",faddrtoa(&addr,NULL));
		return(body); }
	if(add_area.tags || del_area.tags)
		alter_areas(add_area,del_area,addr);
	if(add_area.tags) {
		for(i=0;i<add_area.tags;i++)
			LFREE(add_area.tag[i]);
		FREE(add_area.tag); }
	if(del_area.tags) {
		for(i=0;i<del_area.tags;i++)
			LFREE(del_area.tag[i]);
		FREE(del_area.tag); }
	return(0);
}
/******************************************************************************
 This function will compare the archive signatures defined in the CFG file and
 extract 'infile' using the appropriate de-archiver.
******************************************************************************/
int unpack(char *infile)
{
	FILE *stream;
	char str[256],tmp[3];
	int i,j,ch,file;

	if((stream=fnopen(&file,infile,O_RDONLY))==NULL) {
		printf("\7ERROR couldn't open %s.\n",infile);
		logprintf("ERROR line %d opening %s %s",__LINE__,infile
			,strerror(errno));
		bail(1); }
	for(i=0;i<cfg.arcdefs;i++) {
		str[0]=0;
		fseek(stream,cfg.arcdef[i].byteloc,SEEK_SET);
		for(j=0;j<strlen(cfg.arcdef[i].hexid)/2;j++) {
			ch=fgetc(stream);
			if(ch==EOF) {
				i=cfg.arcdefs;
				break; }
			sprintf(tmp,"%02X",ch);
			strcat(str,tmp); }
		if(!stricmp(str,cfg.arcdef[i].hexid))
			break; }
	fclose(stream);

	if(i==cfg.arcdefs) {
		printf("\7ERROR couldn't determine filetype of %s.\n",infile);
		logprintf("ERROR line %d determining filetype of %s",__LINE__,infile);
		return(1); }

	j=execute(cmdstr(&scfg,cfg.arcdef[i].unpack,infile
		,secure ? cfg.secure : cfg.inbound));
	if(j) {
		printf("\7ERROR %d (%d) executing %s\n"
			,j,errno,cmdstr(&scfg,cfg.arcdef[i].unpack,infile
				,secure ? cfg.secure : cfg.inbound));
		logprintf("ERROR %d (%d) line %d executing %s"
			,j,errno,__LINE__,cmdstr(&scfg,cfg.arcdef[i].unpack,infile
				,secure ? cfg.secure : cfg.inbound));
		return(j); }
	return(0);
}
/******************************************************************************
 This function will check the 'dest' for the type of archiver to use (as
 defined in the CFG file) and compress 'srcfile' into 'destfile' using the
 appropriate archive program.
******************************************************************************/
void pack(char *srcfile,char *destfile,faddr_t dest)
{
	int i,j;
	uint use=0;

	i=matchnode(dest,0);
	if(i<cfg.nodecfgs)
		use=cfg.nodecfg[i].arctype;

	j=execute(cmdstr(&scfg,cfg.arcdef[use].pack,destfile,srcfile));
	if(j) {
		printf("\7ERROR %d (%d) executing %s\n"
			,j,errno,cmdstr(&scfg,cfg.arcdef[use].pack,destfile,srcfile));
		logprintf("ERROR %d (%d) line %d executing %s"
			,j,errno,__LINE__,cmdstr(&scfg,cfg.arcdef[use].pack,destfile,srcfile)); }
}

enum {
	 ATTACHMENT_ADD
	,ATTACHMENT_NETMAIL
	,ATTACHMENT_CHECK
};

int attachment(char *bundlename,faddr_t dest, int mode)
{
	FILE *fidomsg,*stream;
	char str[1025],*path,fname[129],*p;
	int fmsg,file,error=0L;
	long fncrc,*mfncrc=0L,num_mfncrc=0L,crcidx;
    attach_t attach;
	fmsghdr_t hdr;
	size_t		f;
	glob_t		g;

	if(bundlename==NULL && mode!=ATTACHMENT_NETMAIL) {
		logprintf("ERROR line %d NULL bundlename",__LINE__);
		return(1);
	}
	sprintf(fname,"%sBUNDLES.SBE",cfg.outbound);
	if((stream=fnopen(&file,fname,O_RDWR|O_CREAT))==NULL) {
		printf("\7ERROR line %d opening %s %s\n",__LINE__,fname,strerror(errno));
		logprintf("ERROR line %d opening %s %s",__LINE__,fname,strerror(errno));
		return(1); 
	}

	if(mode==ATTACHMENT_CHECK) {				/* Check for existance in BUNDLES.SBE */
		while(!feof(stream)) {
			if(!fread(&attach,1,sizeof(attach_t),stream))
				break;
			if(!stricmp(attach.fname,bundlename)) {
				fclose(stream);
				return(1); 
			} 
		}
		fclose(stream);
		if(!flength(fname))
			remove(fname);
		return(0); 
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
		for(f=0;f<g.gl_pathc;f++) {

			path=g.gl_pathv[f];

			if((fidomsg=fnopen(&fmsg,path,O_RDWR))==NULL) {
				printf("\7ERROR line %d opening %s\n",__LINE__,path);
				logprintf("ERROR line %d opening %s %s",__LINE__,path
					,strerror(errno));
				continue; 
			}
			if(filelength(fmsg)<sizeof(fmsghdr_t)) {
				printf("\7ERROR %s has invalid length of %lu bytes\n"
					,path
					,filelength(fmsg));
				logprintf("ERROR line %d %s has invalid length of %lu bytes"
					,__LINE__
					,path
					,filelength(fmsg));
				fclose(fidomsg);
				continue; 
			}
			if(fread(&hdr,sizeof(fmsghdr_t),1,fidomsg)!=1) {
				fclose(fidomsg);
				printf("\7ERROR reading %u bytes from %s"
					,sizeof(fmsghdr_t),path);
				logprintf("ERROR line %d reading %u bytes from %s"
					,__LINE__,sizeof(fmsghdr_t),path);
				continue; 
			}
			fclose(fidomsg);
			if(!(hdr.attr&FIDO_FILE))		/* Not a file attach */
				continue;
			num_mfncrc++;
			p=getfname(hdr.subj);
			if((mfncrc=(long *)REALLOC(mfncrc,num_mfncrc*sizeof(long)))==NULL) {
				printf("ERROR allocating %lu bytes for bundle name crc.\n"
					,num_mfncrc*sizeof(long));
				logprintf("ERROR line %d allocating %lu for bundle name crc"
					,__LINE__,num_mfncrc*sizeof(long));
				continue; 
			}
			mfncrc[num_mfncrc-1]=crc32(strupr(p),0); 
		}
		globfree(&g);

		while(!feof(stream)) {
			if(!fread(&attach,1,sizeof(attach_t),stream))
				break;
			sprintf(str,"%s%s",cfg.outbound,attach.fname);
			if(!fexistcase(str))
				continue;
			fncrc=crc32(strupr(attach.fname),0);
			for(crcidx=0;crcidx<num_mfncrc;crcidx++)
				if(mfncrc[crcidx]==fncrc)
					break;
			if(crcidx==num_mfncrc)
				if(create_netmail(NULL,str
					,misc&TRUNC_BUNDLES ? "\1FLAGS TFS\r" : "\1FLAGS KFS\r"
					,attach.dest,TRUE))
					error=1; 
		}
		fclose(stream);
		if(!error)			/* remove bundles.sbe if no error occurred */		
			remove(fname);	/* used to truncate here, August-20-2002 */
		if(num_mfncrc)
			FREE(mfncrc);
		return(0); 
	}

	while(!feof(stream)) {
		if(!fread(&attach,1,sizeof(attach_t),stream))
			break;
		if(!stricmp(attach.fname,bundlename)) {
			fclose(stream);
			return(0); 
		} 
	}

	memcpy(&attach.dest,&dest,sizeof(faddr_t));
	strcpy(attach.fname,bundlename);
	fwrite(&attach,sizeof(attach_t),1,stream);
	fclose(stream);
	return(0);
}

/******************************************************************************
 This function is called when a message packet has reached it's maximum size.
 It places packets into a bundle until that bundle is full, at which time the
 last character of the extension increments (1 thru 0 and then A thru Z).  If
 all bundles have reached their maximum size remaining packets are placed into
 the Z bundle.
******************************************************************************/
void pack_bundle(char *infile,faddr_t dest)
{
	char str[256],fname[256],outbound[128],day[3],*p;
	int i,j,file,node;
	time_t now;

	if(infile==NULL || infile[0]==0) {
		logprintf("ERROR line %d invalid filename",__LINE__);
		bail(1);
	}

	node=matchnode(dest,0);
	strcpy(str,infile);
	str[strlen(str)-1]='t';
	if(rename(infile,str))				   /* Change .PK_ file to .PKT file */
		logprintf("ERROR line %d renaming %s to %s",__LINE__,infile,str);
	infile[strlen(infile)-1]='t';
	time(&now);
	sprintf(day,"%-.2s",ctime(&now));
	strupr(day);
	if(misc&FLO_MAILER) {
		if(node<cfg.nodecfgs && cfg.nodecfg[node].route.zone) {
			dest=cfg.nodecfg[node].route;
			if(cfg.log&LOG_ROUTING)
				logprintf("Routing %s to %s",infile,faddrtoa(&dest,NULL));
		}

		if(dest.zone==scfg.faddr[0].zone)	/* Default zone, use default outbound */
			strcpy(outbound,cfg.outbound);
		else {							/* Inter-zone outbound is OUTBOUND.XXX */
			sprintf(outbound,"%.*s.%03x"
				,(int)strlen(cfg.outbound)-1,cfg.outbound,dest.zone);
			MKDIR(outbound);
			backslash(outbound);
		}
		if(dest.point) {				/* Point destination is OUTBOUND\*.PNT */
			sprintf(str,"%04x%04x.pnt"
				,dest.net,dest.node);
			strcat(outbound,str); }
		}
	else
		strcpy(outbound,cfg.outbound);
	if(outbound[strlen(outbound)-1]=='\\'
		|| outbound[strlen(outbound)-1]=='/')
		outbound[strlen(outbound)-1]=0;
	MKDIR(outbound);
	backslash(outbound);

	if(node<cfg.nodecfgs)
		if(cfg.nodecfg[node].arctype==0xffff) {    /* Uncompressed! */
			if(misc&FLO_MAILER)
				i=write_flofile(infile,dest);
			else
				i=create_netmail(NULL,infile
					,misc&TRUNC_BUNDLES ? "\1FLAGS TFS\r" : "\1FLAGS KFS\r"
					,dest,TRUE);
			if(i) bail(1);
			return; }

	if(dest.point && !(misc&FLO_MAILER))
		sprintf(fname,"%s%04hxp%03hx.%s",outbound,0,(short)dest.point,day);
	else
		sprintf(fname,"%s%04hx%04hx.%s",outbound,(short)(scfg.faddr[0].net-dest.net)
			,(short)(scfg.faddr[0].node-dest.node),day);
	for(i='0';i<='Z';i++) {
		if(i==':')
			i='A';
		sprintf(str,"%s%c",fname,i);
		if(flength(str)==0) {
			/* Feb-10-2003: Don't overwrite or delete 0-byte file less than 24hrs old */
			if((time(NULL)-fdate(str))<24L*60L*60L)
				continue;	
			if(delfile(str))
				logprintf("ERROR line %d removing %s %s",__LINE__,str
					,strerror(errno));
		}
		if(fexistcase(str)) {
			if(flength(str)>=cfg.maxbdlsize)
				continue;
			file=sopen(str,O_WRONLY,SH_DENYRW);
			if(file==-1)		/* Can't open?!? Probably being sent */
				continue;
			close(file);
			p=getfname(str);
			if(!attachment(p,dest,ATTACHMENT_CHECK))
				attachment(p,dest,ATTACHMENT_ADD);
			pack(infile,str,dest);
			if(delfile(infile))
				logprintf("ERROR line %d removing %s %s",__LINE__,infile
					,strerror(errno));
			return; }
		else {
			if(misc&FLO_MAILER)
				j=write_flofile(str,dest);
			else {
				p=getfname(str);
				j=attachment(p,dest,ATTACHMENT_ADD); }
			if(j)
				bail(1);
			pack(infile,str,dest);
			if(delfile(infile))
				logprintf("ERROR line %d removing %s %s",__LINE__,infile
					,strerror(errno));
			return; } }

	pack(infile,str,dest);	/* Won't get here unless all bundles are full */
}
/******************************************************************************
 This function checks the inbound directory for the first bundle it finds, it
 will then unpack and delete the bundle.  If no bundles exist this function
 returns a FALSE, otherwise a TRUE is returned.
 ******************************************************************************/
BOOL unpack_bundle(void)
{
	char*				p;
	char				str[MAX_PATH+1];
	char				fname[MAX_PATH+1];
	int				i;
	static glob_t	g;
	static int		gi;

	for(i=0;i<7;i++) {
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
		sprintf(str,"%s*.%s?",secure ? cfg.secure : cfg.inbound,p);
		if(gi>=g.gl_pathc) {
			gi=0;
			globfree(&g);
			glob(str,0,NULL,&g);
		}
		if(gi<g.gl_pathc) {
			SAFECOPY(fname,g.gl_pathv[gi]);
			if(unpack(fname)) {	/* failure */
				if(fdate(fname)+(48L*60L*60L)>time(NULL)) {
					SAFECOPY(str,fname);
					str[strlen(str)-2]='_';
					if(fexistcase(str))
						str[strlen(str)-2]='-';
					if(fexistcase(str))
						delfile(str);
					if(rename(fname,str))
						logprintf("ERROR line %d renaming %s to %s"
							,__LINE__,fname,str); 
				} 
			}
			else if(delfile(fname))	/* successful, so delete bundle */
				logprintf("ERROR line %d removing %s %s",__LINE__,fname
					,strerror(errno));
			gi++;
			return(TRUE); 
		} 
	}

	return(FALSE);
}

/****************************************************************************/
/* Moves or copies a file from one dir to another                           */
/* both 'src' and 'dest' must contain full path and filename                */
/* returns 0 if successful, -1 if error                                     */
/****************************************************************************/
int mv(char *src, char *dest, BOOL copy)
{
	char buf[4096],str[256];
	int  ind,outd;
	long length,chunk=4096,l;
    FILE *inp,*outp;

	if(!strcmp(src,dest))	/* source and destination are the same! */
		return(0);
	if(!fexistcase(src)) {
		logprintf("MV ERROR: Source doesn't exist '%s",src);
		return(-1); }
	if(!copy && fexistcase(dest)) {
		logprintf("MV ERROR: Destination already exists '%s'",dest);
		return(-1); }
	if(!copy && ((src[1]!=':' && dest[1]!=':')
		|| (src[1]==':' && dest[1]==':' && toupper(src[0])==toupper(dest[0])))) {
		if(rename(src,dest)) {						/* same drive, so move */
			logprintf("MV ERROR: Error renaming %s to %s",src,dest);
			return(-1); }
		return(0); }
	if((ind=nopen(src,O_RDONLY))==-1) {
		logprintf("MV ERROR: ERR_OPEN %s",src);
		return(-1); }
	if((inp=fdopen(ind,"rb"))==NULL) {
		close(ind);
		logprintf("MV ERROR: ERR_FDOPEN %s",str);
		return(-1); }
	setvbuf(inp,NULL,_IOFBF,8*1024);
	if((outd=nopen(dest,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
		fclose(inp);
		logprintf("MV ERROR: ERR_OPEN %s",dest);
		return(-1); }
	if((outp=fdopen(outd,"wb"))==NULL) {
		close(outd);
		fclose(inp);
		logprintf("MV ERROR: ERR_FDOPEN %s",str);
		return(-1); }
	setvbuf(outp,NULL,_IOFBF,8*1024);
	length=filelength(ind);
	l=0L;
	while(l<length) {
		if(l+chunk>length)
			chunk=length-l;
		fread(buf,chunk,1,inp);
		fwrite(buf,chunk,1,outp);
		l+=chunk; }
	fclose(inp);
	fclose(outp);
	if(!copy && delfile(src)) {
		logprintf("ERROR line %d removing %s %s",__LINE__,src,strerror(errno));
		return(-1); }
	return(0);
}

/****************************************************************************/
/* Returns the total number of msgs in the sub-board and sets 'ptr' to the  */
/* date of the last message in the sub (0) if no messages.					*/
/****************************************************************************/
ulong getlastmsg(uint subnum, ulong *ptr, time_t *t)
{
	int i;
	smb_t smbfile;

	if(subnum>=scfg.total_subs) {
		printf("\nERROR getlastmsg, subnum=%d\n",subnum);
		logprintf("ERROR line %d getlastmsg %d",__LINE__,subnum);
		bail(1); }
	sprintf(smbfile.file,"%s%s",scfg.sub[subnum]->data_dir,scfg.sub[subnum]->code);
	smbfile.retry_time=scfg.smb_retry_time;
	if((i=smb_open(&smbfile))!=0) {
		printf("ERROR %d opening %s\n",i,smbfile.file);
		logprintf("ERROR %d line %d opening %s",i,__LINE__,smbfile.file);
		return(0); }

	if(!filelength(fileno(smbfile.shd_fp))) {			/* Empty base */
		if(ptr) (*ptr)=0;
		smb_close(&smbfile);
		return(0); }
	smb_close(&smbfile);
	if(ptr) (*ptr)=smbfile.status.last_msg;
	return(smbfile.status.total_msgs);
}


ulong loadmsgs(post_t HUGE16 **post, ulong ptr)
{
	int i;
	long l=0,total;
	idxrec_t idx;


	if((i=smb_locksmbhdr(&smb[cur_smb]))!=0) {
		printf("ERROR %d locking %s\n",i,smb[cur_smb].file);
		logprintf("ERROR %d line %d locking %s",i,__LINE__,smb[cur_smb].file);
		return(0L); }

	/* total msgs in sub */
	total=filelength(fileno(smb[cur_smb].sid_fp))/sizeof(idxrec_t);

	if(!total) {			/* empty */
		smb_unlocksmbhdr(&smb[cur_smb]);
		return(0); }

	if(((*post)=(post_t HUGE16 *)LMALLOC(sizeof(post_t)*total))    /* alloc for max */
		==NULL) {
		smb_unlocksmbhdr(&smb[cur_smb]);
		printf("ERROR allocating %lu bytes for %s\n",sizeof(post_t *)*total
			,smb[cur_smb].file);
		logprintf("ERROR line %d allocating %lu bytes for %s",__LINE__
			,sizeof(post_t *)*total,smb[cur_smb].file);
		return(0); }

	fseek(smb[cur_smb].sid_fp,0L,SEEK_SET);
	while(!feof(smb[cur_smb].sid_fp)) {
		if(smb_fread(&idx,sizeof(idx),smb[cur_smb].sid_fp) != sizeof(idx))
			break;

		if(idx.number==0)	/* invalid message number, ignore */
			continue;

		if(idx.number<=ptr || idx.attr&MSG_DELETE)
			continue;

		if(idx.attr&MSG_MODERATED && !(idx.attr&MSG_VALIDATED))
			break;

		(*post)[l++]=idx;
	}
	smb_unlocksmbhdr(&smb[cur_smb]);
	if(!l)
		LFREE(*post);
	return(l);
}

void bail(int code)
{
	if(code || pause_on_exit) {
		fprintf(stderr,"\nHit any key...");
		getch();
		fprintf(stderr,"\n");
	}
	exit(code);
}

typedef struct {
	ulong	alias,
			real;
			} username_t;

/****************************************************************************/
/* Note: Wrote another version of this function that read all userdata into */
/****************************************************************************/
/* Looks for a perfect match amoung all usernames (not deleted users)		*/
/* Returns the number of the perfect matched username or 0 if no match		*/
/* Called from functions waitforcall and newuser							*/
/* memory then scanned it from memory... took longer - always.              */
/****************************************************************************/
ulong matchname(char *inname)
{
	static ulong total_users;
	static username_t *username;
	ulong last_user;
	int userdat,i;
	char str[256],name[LEN_NAME+1],alias[LEN_ALIAS+1];
	ulong l,crc;

	if(!total_users) {		/* Load CRCs */
		fprintf(stderr,"\n%-25s","Loading user names...");
		sprintf(str,"%suser/user.dat",scfg.data_dir);
		if((userdat=nopen(str,O_RDONLY|O_DENYNONE))==-1)
			return(0);
		last_user=filelength(userdat)/U_LEN;
		for(total_users=0;total_users<last_user;total_users++) {
			printf("%5ld\b\b\b\b\b",total_users);
			if((username=(username_t *)REALLOC(username
				,(total_users+1L)*sizeof(username_t)))==NULL)
				break;
			username[total_users].alias=0;
			username[total_users].real=0;
			i=0;
			while(i<LOOP_NODEDAB
				&& lock(userdat,(long)((long)(total_users)*U_LEN)+U_ALIAS
					,LEN_ALIAS+LEN_NAME)==-1)
				i++;
			if(i>=LOOP_NODEDAB) {	   /* Couldn't lock USER.DAT record */
				logprintf("ERROR locking USER.DAT record #%ld",total_users);
				continue; }
			lseek(userdat,(long)((long)(total_users)*U_LEN)+U_ALIAS,SEEK_SET);
			read(userdat,alias,LEN_ALIAS);
			read(userdat,name,LEN_NAME);
			lseek(userdat,(long)(((long)total_users)*U_LEN)+U_MISC,SEEK_SET);
			read(userdat,tmp,8);
			for(i=0;i<8;i++)
				if(tmp[i]==ETX || tmp[i]==CR) break;
			tmp[i]=0;
			unlock(userdat,(long)((long)(total_users)*U_LEN)+U_ALIAS
				,LEN_ALIAS+LEN_NAME);
			if(ahtoul(tmp)&DELETED)
				continue;
			for(i=0;i<LEN_ALIAS;i++)
				if(alias[i]==ETX || alias[i]==CR) break;
			alias[i]=0;
			strupr(alias);
			for(i=0;i<LEN_NAME;i++)
				if(name[i]==ETX || name[i]==CR) break;
			name[i]=0;
			strupr(name);
			username[total_users].alias=crc32(alias,0);
			username[total_users].real=crc32(name,0); }
		close(userdat);
		fprintf(stderr,"     \b\b\b\b\b");  // Clear counter
		fprintf(stderr,
			"\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
			"%25s"
			"\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
			,""); }

	strcpy(str,inname);
	strupr(str);
	crc=crc32(str,0);
	for(l=0;l<total_users;l++)
		if((crc==username[l].alias || crc==username[l].real))
			return(l+1);
	return(0);
}

/****************************************************************************/
/* Converts goofy FidoNet time format into Unix format						*/
/****************************************************************************/
time_t fmsgtime(char *str)
{
	char month[4];
	struct tm tm;

	memset(&tm,0,sizeof(tm));
	if(isdigit(str[1])) {	/* Regular format: "01 Jan 86  02:34:56" */
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
		tm.tm_sec=atoi(str+17); }

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
		tm.tm_sec=0; }
	return(mktime(&tm));
}

static short fmsgzone(char* p)
{
	char	hr[4]="";
	char	min[4]="";
	short	val;
	BOOL	west=TRUE;

	if(*p=='-')
		p++;
	else
		west=FALSE;

	if(strlen(p)>=2)
		sprintf(hr,"%.2s",p);
	if(strlen(p+2)>=2)
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

#if 1		/* Old way */

char HUGE16 *getfmsg(FILE *stream, ulong *outlen)
{
	uchar HUGE16 *fbuf;
	int ch;
	ulong l,length,start;

	length=0L;
	start=ftell(stream);						/* Beginning of Message */
	while(1) {
		ch=fgetc(stream);						/* Look for Terminating NULL */
		if(!ch || ch==EOF)						/* Found end of message */
			break;
		length++; } 							/* Increment the Length */

	if((fbuf=(char *)LMALLOC(length+1))==NULL) {
		printf("Unable to allocate %lu bytes for message.\n",length+1);
		logprintf("ERROR line %d allocating %lu bytes of memory",__LINE__,length+1);
		bail(1); }

	fseek(stream,start,SEEK_SET);
	for(l=0;l<length;l++)
		fbuf[l]=fgetc(stream);
	fbuf[length]=0;
	if(!ch)
		fgetc(stream);		/* Read NULL */
	if(outlen)
		*outlen=length;
	return(fbuf);
}

#else

#define FBUF_BLOCK 4096

char *getfmsg(FILE *stream)
{
	uchar *fbuf,*p;
	ulong l,n,length,start;

	length=0L;
	start=ftell(stream);						/* Beginning of Message */
	if((fbuf=LMALLOC(FBUF_BLOCK))==NULL)
		return(fbuf);
	while(!feof(stream)) {
		l=fread(fbuf+length,1,FBUF_BLOCK,stream);
		if(l<1)
			break;
		*(fbuf+length+l)=0;
		n=strlen(fbuf+length);
		if(n<l) {
			length+=(n+1);
			break; }
		printf(",");
		length+=l;
		if(l<FBUF_BLOCK)
			break;
		printf("<");
		if((p=REALLOC(fbuf,length+FBUF_BLOCK+1))==NULL) {
			LFREE(fbuf);
			printf("!");
			fseek(stream,-l,SEEK_CUR);
			return(NULL); }
		fbuf=p;
		printf(">");
		}
	printf(".");

	fseek(stream,start+length,SEEK_SET);
	return(fbuf);
}

#endif

#define MAX_TAILLEN 1024

/****************************************************************************/
/* Coverts a FidoNet message into a Synchronet message						*/
/* Returns 1 on success, 0 on failure, -1 on dupe.							*/
/****************************************************************************/
int fmsgtosmsg(uchar HUGE16 *fbuf, fmsghdr_t fmsghdr, uint user, uint subnum)
{
	uchar	ch,HUGE16 *sbody,HUGE16 *stail,HUGE16 *outbuf
				,*p,str[128];
	BOOL	done,esc,cr;
	int 	i,chunk,lzh=0,storage;
	uint	col;
	ushort	xlat,net;
	ulong	l,m,length,lzhlen,bodylen,taillen,crc;
	ulong	save;
	faddr_t faddr,origaddr,destaddr;
	smbmsg_t	msg;
	smb_t	*smbfile;
	char	fname[MAX_PATH+1];

	if(twit_list) {
		sprintf(fname,"%stwitlist.cfg",scfg.ctrl_dir);
		if(findstr(fmsghdr.from,fname)) {
			printf("Filtering message from twit: %s",fmsghdr.from);
			return(0);
		}
	}

	memset(&msg,0,sizeof(smbmsg_t));
	msg.hdr.version=smb_ver();
	if(fmsghdr.attr&FIDO_PRIVATE)
		msg.idx.attr|=MSG_PRIVATE;
	msg.hdr.attr=msg.idx.attr;

	if(fmsghdr.attr&FIDO_FILE)
		msg.hdr.auxattr|=MSG_FILEATTACH;

	msg.hdr.when_imported.time=time(NULL);
	msg.hdr.when_imported.zone=scfg.sys_timezone;
	msg.hdr.when_written.time=fmsgtime(fmsghdr.time);

	origaddr.zone=fmsghdr.origzone; 	/* only valid if NetMail */
	origaddr.net=fmsghdr.orignet;
	origaddr.node=fmsghdr.orignode;
	origaddr.point=fmsghdr.origpoint;

	destaddr.zone=fmsghdr.destzone; 	/* only valid if NetMail */
	destaddr.net=fmsghdr.destnet;
	destaddr.node=fmsghdr.destnode;
	destaddr.point=fmsghdr.destpoint;

	smb_hfield(&msg,SENDER,(ushort)strlen(fmsghdr.from),fmsghdr.from);
	strlwr(fmsghdr.from);
	if(subnum==INVALID_SUB)
		msg.idx.from=0;
	else
		msg.idx.from=crc16(fmsghdr.from,0);

	smb_hfield(&msg,RECIPIENT,(ushort)strlen(fmsghdr.to),fmsghdr.to);
	strlwr(fmsghdr.to);
	msg.idx.to=crc16(fmsghdr.to,0);

	if(user) {
		sprintf(str,"%u",user);
		smb_hfield(&msg,RECIPIENTEXT,(ushort)strlen(str),str);
		msg.idx.to=user; }

	smb_hfield(&msg,SUBJECT,(ushort)strlen(fmsghdr.subj),fmsghdr.subj);
	msg.idx.subj=subject_crc(fmsghdr.subj);

	if(fbuf==NULL) {
		printf("ERROR allocating fbuf\n");
		logprintf("ERROR line %d allocating fbuf",__LINE__);
		smb_freemsgmem(&msg);
		return(0); }
	length=strlen((char *)fbuf);
	if((sbody=(char HUGE16 *)LMALLOC((length+1)*2))==NULL) {
		printf("ERROR allocating %lu bytes for body",(length+1)*2L);
		logprintf("ERROR line %d allocating %lu bytes for body",__LINE__
			,(length+1)*2L);
		smb_freemsgmem(&msg);
		return(0); }
	if((stail=(char HUGE16 *)LMALLOC(MAX_TAILLEN))==NULL) {
		printf("ERROR allocating %u bytes\n",MAX_TAILLEN);
		logprintf("ERROR line %d allocating %u bytes for tail",__LINE__
			,MAX_TAILLEN);
		LFREE(sbody);
		smb_freemsgmem(&msg);
		return(0); }

	for(col=l=esc=done=bodylen=taillen=0,cr=1;l<length;l++) {

		if(!l && !strncmp((char *)fbuf,"AREA:",5)) {
			save=l;
			l+=5;
			while(l<length && fbuf[l]<=SP) l++;
			m=l;
			while(m<length && fbuf[m]!=CR) m++;
			while(m && fbuf[m-1]<=SP) m--;
			if(m>l)
				smb_hfield(&msg,FIDOAREA,(ushort)(m-l),fbuf+l);
			while(l<length && fbuf[l]!=CR) l++;
			/* If unknown echo, keep AREA: line in message body */
			if(cfg.badecho>=0 && subnum==cfg.area[cfg.badecho].sub)
				l=save;
			else
				continue; 
		}

		ch=fbuf[l];
		if(ch==1 && cr) {	/* kludge line */

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
				while(l<length && fbuf[l]!=SP) l++;
				faddr=atofaddr((char *)fbuf+l+1);
				origaddr.zone=faddr.zone;
				origaddr.net=faddr.net;
				origaddr.node=faddr.node; }

			else if(!strncmp((char *)fbuf+l+1,"MSGID:",6)) {
				l+=7;
				while(l<length && fbuf[l]<=SP) l++;
				m=l;
				while(m<length && fbuf[m]!=CR) m++;
				while(m && fbuf[m-1]<=SP) m--;
				if(m>l)
					smb_hfield(&msg,FIDOMSGID,(ushort)(m-l),fbuf+l); }

			else if(!strncmp((char *)fbuf+l+1,"REPLY:",6)) {
				l+=7;
				while(l<length && fbuf[l]<=SP) l++;
				m=l;
				while(m<length && fbuf[m]!=CR) m++;
				while(m && fbuf[m-1]<=SP) m--;
				if(m>l)
					smb_hfield(&msg,FIDOREPLYID,(ushort)(m-l),fbuf+l); }

			else if(!strncmp((char *)fbuf+l+1,"FLAGS ",6)		/* correct */
				||  !strncmp((char *)fbuf+l+1,"FLAGS:",6)) {	/* incorrect */
				l+=7;
				while(l<length && fbuf[l]<=SP) l++;
				m=l;
				while(m<length && fbuf[m]!=CR) m++;
				while(m && fbuf[m-1]<=SP) m--;
				if(m>l)
					smb_hfield(&msg,FIDOFLAGS,(ushort)(m-l),fbuf+l); }

			else if(!strncmp((char *)fbuf+l+1,"PATH:",5)) {
				l+=6;
				while(l<length && fbuf[l]<=SP) l++;
				m=l;
				while(m<length && fbuf[m]!=CR) m++;
				while(m && fbuf[m-1]<=SP) m--;
				if(m>l && misc&STORE_PATH)
					smb_hfield(&msg,FIDOPATH,(ushort)(m-l),fbuf+l); }

			else if(!strncmp((char *)fbuf+l+1,"PID:",4)) {
				l+=5;
				while(l<length && fbuf[l]<=SP) l++;
				m=l;
				while(m<length && fbuf[m]!=CR) m++;
				while(m && fbuf[m-1]<=SP) m--;
				if(m>l)
					smb_hfield(&msg,FIDOPID,(ushort)(m-l),fbuf+l); }

			else if(!strncmp((char *)fbuf+l+1,"TID:",4)) {
				l+=5;
				while(l<length && fbuf[l]<=SP) l++;
				m=l;
				while(m<length && fbuf[m]!=CR) m++;
				while(m && fbuf[m-1]<=SP) m--;
				if(m>l)
					smb_hfield(&msg,FIDOTID,(ushort)(m-l),fbuf+l); }

			else if(!strncmp((char *)fbuf+l+1,"TZUTC:",6)) {		/* FSP-1001 */
				l+=7;
				while(l<length && fbuf[l]<=SP) l++;
				msg.hdr.when_written.zone = fmsgzone(fbuf+l);
			}

			else if(!strncmp((char *)fbuf+l+1,"TZUTCINFO:",10)) {	/* non-standard */
				l+=11;
				while(l<length && fbuf[l]<=SP) l++;
				msg.hdr.when_written.zone = fmsgzone(fbuf+l);
			}

			else {		/* Unknown kludge line */
				while(l<length && fbuf[l]<=SP) l++;
				m=l;
				while(m<length && fbuf[m]!=CR) m++;
				while(m && fbuf[m-1]<=SP) m--;
				if(m>l && misc&STORE_KLUDGE)
					smb_hfield(&msg,FIDOCTRL,(ushort)(m-l),fbuf+l); }

			while(l<length && fbuf[l]!=CR) l++;
			continue; }

		if(ch!=LF && ch!=0x8d) {	/* ignore LF and soft CRs */
			if(cr && (!strncmp((char *)fbuf+l,"--- ",4)
				|| !strncmp((char *)fbuf+l,"---\r",4)))
				done=1; 			/* tear line and down go into tail */
			if(done && cr && !strncmp((char *)fbuf+l,"SEEN-BY:",8)) {
				l+=8;
				while(l<length && fbuf[l]<=SP) l++;
				m=l;
				while(m<length && fbuf[m]!=CR) m++;
				while(m && fbuf[m-1]<=SP) m--;
				if(m>l && misc&STORE_SEENBY)
					smb_hfield(&msg,FIDOSEENBY,(ushort)(m-l),fbuf+l);
				while(l<length && fbuf[l]!=CR) l++;
				continue; }
			if(done) {
				if(taillen<MAX_TAILLEN)
					stail[taillen++]=ch; }
			else
				sbody[bodylen++]=ch;
			col++;
			if(ch==CR) {
				cr=1;
				col=0;
				if(done) {
					if(taillen<MAX_TAILLEN)
						stail[taillen++]=LF; }
				else
					sbody[bodylen++]=LF; }
			else {
				cr=0;
				if(col==1 && !strncmp((char *)fbuf+l," * Origin: ",11)) {
					p=(char *)fbuf+l+11;
					while(*p && *p!=CR) p++;	 /* Find CR */
					while(p && *p!='(') p--;     /* rewind to '(' */
					if(p)
						origaddr=atofaddr(p+1); 	/* get orig address */
					done=1; }
				if(done)
					continue;

				if(ch==ESC) esc=1;		/* ANSI codes */
				if(ch==SP && col>40 && !esc) {	/* word wrap */
					for(m=l+1;m<length;m++) 	/* find next space */
						if(fbuf[m]<=SP)
							break;
					if(m<length && m-l>80-col) {  /* if it's beyond the eol */
						sbody[bodylen++]=CR;
						sbody[bodylen++]=LF;
						col=0; } }
				} } }

	if(bodylen>=2 && sbody[bodylen-2]==CR && sbody[bodylen-1]==LF)
		bodylen-=2; 						/* remove last CRLF if present */

	if(smb[cur_smb].status.max_crcs) {
		for(l=0,crc=0xffffffff;l<bodylen;l++)
			crc=ucrc32(sbody[l],crc);
		crc=~crc;
		i=smb_addcrc(&smb[cur_smb],crc);
		if(i) {
			if(i==1)
				printf("Duplicate ");
			else
				printf("smb_addcrc returned %d ",i);
			smb_freemsgmem(&msg);
			LFREE(sbody);
			LFREE(stail);
			if(i==1)
				return(-1);
			return(0); } }

	while(taillen && stail[taillen-1]<=SP)	/* trim all garbage off the tail */
		taillen--;

	if(!origaddr.zone && subnum==INVALID_SUB)
		net=NET_NONE;						/* Message from SBBSecho */
	else
		net=NET_FIDO;						/* Record origin address */

	if(net) {
		smb_hfield(&msg,SENDERNETTYPE,sizeof(ushort),&net);
		smb_hfield(&msg,SENDERNETADDR,sizeof(fidoaddr_t),&origaddr); }

	if(subnum==INVALID_SUB) {
		smbfile=email;
		if(net) {
			smb_hfield(&msg,RECIPIENTNETTYPE,sizeof(ushort),&net);
			smb_hfield(&msg,RECIPIENTNETADDR,sizeof(fidoaddr_t),&destaddr); } }
	else
		smbfile=&smb[cur_smb];

	if(subnum!=INVALID_SUB && scfg.sub[subnum]->misc&SUB_LZH
		&& bodylen+2L+taillen+2L>=SDT_BLOCK_LEN && bodylen) {
		if((outbuf=(char *)LMALLOC(bodylen*2L))==NULL) {
			printf("ERROR allocating %lu bytes for lzh\n",bodylen*2);
			logprintf("ERROR line %d allocating %lu bytes for lzh",__LINE__
				,bodylen*2);
			smb_freemsgmem(&msg);
			LFREE(sbody);
			LFREE(stail);
			return(0); }
		lzhlen=lzh_encode((uchar *)sbody,bodylen,(uchar *)outbuf);
		if(lzhlen>1 &&
			smb_datblocks(lzhlen+4L+taillen+2L)<
			smb_datblocks(bodylen+2L+taillen+2L)) {
			bodylen=lzhlen; 	/* Compressable */
			l=bodylen+4;
			LFREE(sbody);
			lzh=1;
			sbody=outbuf; }
		else {					/* Uncompressable */
			l=bodylen+2;
			LFREE(outbuf); } }
	else
		l=bodylen+2;

	if(taillen)
		l+=(taillen+2);

	if(l&0xfff00000) {
		printf("ERROR checking msg len %lu\n",l);
		logprintf("ERROR line %d checking msg len %lu",__LINE__,l);
		smb_freemsgmem(&msg);
		LFREE(sbody);
		LFREE(stail);
		return(0); }

	if(smbfile->status.attr&SMB_HYPERALLOC) {
		if((i=smb_locksmbhdr(smbfile))!=0) {
			printf("ERROR %d locking %s\n",i,smbfile->file);
			logprintf("ERROR %d line %d locking %s",i,__LINE__,smbfile->file);
			smb_freemsgmem(&msg);
			LFREE(sbody);
			LFREE(stail);
			return(0); }
		msg.hdr.offset=smb_hallocdat(smbfile);
		storage=SMB_HYPERALLOC; }
	else {
		if((i=smb_open_da(smbfile))!=0) {
			smb_freemsgmem(&msg);
			printf("ERROR %d opening %s.sda\n",i,smbfile->file);
			logprintf("ERROR %d line %d opening %s.sda",i,__LINE__
				,smbfile->file);
			LFREE(sbody);
			LFREE(stail);
			return(0); }
		if(subnum!=INVALID_SUB && scfg.sub[subnum]->misc&SUB_FAST) {
			msg.hdr.offset=smb_fallocdat(smbfile,l,1);
			storage=SMB_FASTALLOC; }
		else {
			msg.hdr.offset=smb_allocdat(smbfile,l,1);
			storage=SMB_SELFPACK; }
		smb_close_da(smbfile); }

	if(msg.hdr.offset && msg.hdr.offset<1L) {
		if(smbfile->status.attr&SMB_HYPERALLOC)
			smb_unlocksmbhdr(smbfile);
		smb_freemsgmem(&msg);
		LFREE(sbody);
		LFREE(stail);
		printf("ERROR %ld allocating records\n",msg.hdr.offset);
		logprintf("ERROR line %d %ld allocating records",__LINE__,msg.hdr.offset);
		return(0); }
	fseek(smbfile->sdt_fp,msg.hdr.offset,SEEK_SET);
	if(lzh) {
		xlat=XLAT_LZH;
		fwrite(&xlat,2,1,smbfile->sdt_fp); }
	xlat=XLAT_NONE;
	fwrite(&xlat,2,1,smbfile->sdt_fp);
	chunk=30000;
	for(l=0;l<bodylen;l+=chunk) {
		if(l+chunk>bodylen)
			chunk=bodylen-l;
		fwrite(sbody+l,1,chunk,smbfile->sdt_fp); }
	if(taillen) {
		fwrite(&xlat,2,1,smbfile->sdt_fp);
		fwrite(stail,1,taillen,smbfile->sdt_fp); }
	LFREE(sbody);
	LFREE(stail);
	fflush(smbfile->sdt_fp);
	if(smbfile->status.attr&SMB_HYPERALLOC)
		smb_unlocksmbhdr(smbfile);

	if(lzh)
		bodylen+=2;
	bodylen+=2;
	smb_dfield(&msg,TEXT_BODY,bodylen);
	if(taillen)
		smb_dfield(&msg,TEXT_TAIL,taillen+2);

	i=smb_addmsghdr(smbfile,&msg,storage);
	smb_freemsgmem(&msg);
	if(i) {
		printf("ERROR smb_addmsghdr returned %d\n",i);
		logprintf("ERROR line %d smb_addmsghdr returned %d"
			,__LINE__,i);
		return(0); }
	return(1);
}

/***********************************************************************/
/* Get zone and point from kludge lines from the stream  if they exist */
/***********************************************************************/
void getzpt(FILE *stream, fmsghdr_t *hdr)
{
	char buf[0x1000];
	int i,len,cr=0;
	long pos;
	faddr_t faddr;

	pos=ftell(stream);
	len=fread(buf,1,0x1000,stream);
	for(i=0;i<len;i++) {
		if((!i || cr) && buf[i]==1) {	/* kludge */
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
				while(buf[i] && buf[i]!=SP) i++;
				faddr=atofaddr(buf+i+1);
				hdr->origzone=faddr.zone;
				hdr->orignet=faddr.net;
				hdr->orignode=faddr.node; }
			while(i<len && buf[i]!=CR) i++;
			cr=1;
			continue; }
		if(buf[i]==CR)
			cr=1;
		else
			cr=0; }
	fseek(stream,pos,SEEK_SET);
}
/******************************************************************************
 This function will seek to the next NULL found in stream
******************************************************************************/
void seektonull(FILE *stream)
{
	char ch;

	while(!feof(stream)) {
		if(!fread(&ch,1,1,stream))
			break;
		if(!ch)
			break; 
	}
}

/******************************************************************************
 This function returns a packet name - used for outgoing packets
******************************************************************************/
char *pktname(void)
{
	static char str[128];
	int i;
    time_t now;
    struct tm *tm;

	now=time(NULL);
	for(i=0;i<MAX_TOTAL_PKTS*2;i++) {
		now+=i;
		tm=localtime(&now);
		sprintf(str,"%s%02u%02u%02u%02u.pk_",cfg.outbound,tm->tm_mday,tm->tm_hour
			,tm->tm_min,tm->tm_sec);
		if(!fexist(str))				/* Add 1 second if name exists */
			break; }
	return(str);
}
/******************************************************************************
 This function puts a message into a Fido packet, writing both the header
 information and the message body
******************************************************************************/
void putfmsg(FILE *stream,uchar HUGE16 *fbuf,fmsghdr_t fmsghdr,areasbbs_t area
	,addrlist_t seenbys,addrlist_t paths)
{
	char str[256],seenby[256];
	short i,j,lastlen=0,net_exists=0;
	faddr_t addr,sysaddr;
	time_t t;
	struct tm* tm;

	addr=getsysfaddr(fmsghdr.destzone);

	i=0x0002;
	fwrite(&i,2,1,stream);
	fwrite(&addr.node,2,1,stream);
	fwrite(&fmsghdr.destnode,2,1,stream);
	fwrite(&addr.net,2,1,stream);
	fwrite(&fmsghdr.destnet,2,1,stream);
	fwrite(&fmsghdr.attr,2,1,stream);
	fwrite(&fmsghdr.cost,2,1,stream);
	fwrite(fmsghdr.time,strlen(fmsghdr.time)+1,1,stream);
	fwrite(fmsghdr.to,strlen(fmsghdr.to)+1,1,stream);
	fwrite(fmsghdr.from,strlen(fmsghdr.from)+1,1,stream);
	fwrite(fmsghdr.subj,strlen(fmsghdr.subj)+1,1,stream);
	if(area.name)
		if(strncmp((char *)fbuf,"AREA:",5))                     /* No AREA: Line */
			fprintf(stream,"AREA:%s\r",area.name);              /* So add one */
	fwrite(fbuf,strlen((char *)fbuf),1,stream);
	lastlen=9;
	if(fbuf[strlen((char *)fbuf)-1]!=CR)
		fputc(CR,stream);

	if(area.name==NULL)	{ /* NetMail, so add FSP-1010 Via kludge line */
		t=time(NULL);
		tm=gmtime(&t);
		fprintf(stream,"\1Via %s @%04u%02u%02u.%02u%02u%02u.UTC "
			"SBBSecho %s-%s r%s\r"
			,faddrtoa(&addr,NULL)
			,tm->tm_year+1900
			,tm->tm_mon+1
			,tm->tm_mday
			,tm->tm_hour
			,tm->tm_min
			,tm->tm_sec
			,SBBSECHO_VER,PLATFORM_DESC,revision);
	}
			

	if(area.name && addr.zone!=fmsghdr.destzone)	/* Zone Gate */
		fprintf(stream,"SEEN-BY: %d/%d\r",fmsghdr.destnet,fmsghdr.destnode);

	if(area.name && addr.zone==fmsghdr.destzone) {	/* Not NetMail */
		fprintf(stream,"SEEN-BY:");
		for(i=0;i<seenbys.addrs;i++) {			  /* Put back original SEEN-BYs */
			strcpy(seenby," ");
			if(seenbys.addr[i].zone!=addr.zone)
				continue;
			if(seenbys.addr[i].net!=addr.net || !net_exists) {
				net_exists=1;
				addr.net=seenbys.addr[i].net;
				sprintf(str,"%d/",addr.net);
				strcat(seenby,str); }
			sprintf(str,"%d",seenbys.addr[i].node);
			strcat(seenby,str);
			if(lastlen+strlen(seenby)<80) {
				fwrite(seenby,strlen(seenby),1,stream);
				lastlen+=strlen(seenby); }
			else {
				--i;
				lastlen=9; /* +strlen(seenby); */
				net_exists=0;
				fprintf(stream,"\rSEEN-BY:"); } }

		for(i=0;i<area.uplinks;i++) {			/* Add all uplinks to SEEN-BYs */
			strcpy(seenby," ");
			if(area.uplink[i].zone!=addr.zone || area.uplink[i].point)
				continue;
			for(j=0;j<seenbys.addrs;j++)
				if(!memcmp(&area.uplink[i],&seenbys.addr[j],sizeof(faddr_t)))
					break;
			if(j==seenbys.addrs) {
				if(area.uplink[i].net!=addr.net || !net_exists) {
					net_exists=1;
					addr.net=area.uplink[i].net;
					sprintf(str,"%d/",addr.net);
					strcat(seenby,str); }
				sprintf(str,"%d",area.uplink[i].node);
				strcat(seenby,str);
				if(lastlen+strlen(seenby)<80) {
					fwrite(seenby,strlen(seenby),1,stream);
					lastlen+=strlen(seenby); }
				else {
					--i;
					lastlen=9; /* +strlen(seenby); */
					net_exists=0;
					fprintf(stream,"\rSEEN-BY:"); } } }

		for(i=0;i<scfg.total_faddrs;i++) {				/* Add AKAs to SEEN-BYs */
			strcpy(seenby," ");
			if(scfg.faddr[i].zone!=addr.zone || scfg.faddr[i].point)
				continue;
			for(j=0;j<seenbys.addrs;j++)
				if(!memcmp(&scfg.faddr[i],&seenbys.addr[j],sizeof(faddr_t)))
					break;
			if(j==seenbys.addrs) {
				if(scfg.faddr[i].net!=addr.net || !net_exists) {
					net_exists=1;
					addr.net=scfg.faddr[i].net;
					sprintf(str,"%d/",addr.net);
					strcat(seenby,str); }
				sprintf(str,"%d",scfg.faddr[i].node);
				strcat(seenby,str);
				if(lastlen+strlen(seenby)<80) {
					fwrite(seenby,strlen(seenby),1,stream);
					lastlen+=strlen(seenby); }
				else {
					--i;
					lastlen=9; /* +strlen(seenby); */
					net_exists=0;
					fprintf(stream,"\rSEEN-BY:"); } } }

		lastlen=7;
		net_exists=0;
		fprintf(stream,"\r\1PATH:");
		addr=getsysfaddr(fmsghdr.destzone);
		for(i=0;i<paths.addrs;i++) {			  /* Put back the original PATH */
			strcpy(seenby," ");
			if(paths.addr[i].zone!=addr.zone || paths.addr[i].point)
				continue;
			if(paths.addr[i].net!=addr.net || !net_exists) {
				net_exists=1;
				addr.net=paths.addr[i].net;
				sprintf(str,"%d/",addr.net);
				strcat(seenby,str); }
			sprintf(str,"%d",paths.addr[i].node);
			strcat(seenby,str);
			if(lastlen+strlen(seenby)<80) {
				fwrite(seenby,strlen(seenby),1,stream);
				lastlen+=strlen(seenby); }
			else {
				--i;
				lastlen=7; /* +strlen(seenby); */
				net_exists=0;
				fprintf(stream,"\r\1PATH:"); } }

		strcpy(seenby," ");         /* Add first address with same zone to PATH */
		sysaddr=getsysfaddr(fmsghdr.destzone);
		if(!sysaddr.point) {
			if(sysaddr.net!=addr.net || !net_exists) {
				net_exists=1;
				addr.net=sysaddr.net;
				sprintf(str,"%d/",addr.net);
				strcat(seenby,str); }
			sprintf(str,"%d",sysaddr.node);
			strcat(seenby,str);
			if(lastlen+strlen(seenby)<80)
				fwrite(seenby,strlen(seenby),1,stream);
			else {
				fprintf(stream,"\r\1PATH:");
				fwrite(seenby,strlen(seenby),1,stream); } }

		fputc(CR,stream); }

	fputc(0,stream);
}

/******************************************************************************
 This function creates a binary list of the message seen-bys and path from
 inbuf.
******************************************************************************/
void gen_psb(addrlist_t *seenbys,addrlist_t *paths,char HUGE16 *inbuf
	,ushort zone)
{
	char str[128],seenby[256],*p,*p1,*p2,HUGE16 *fbuf;
	int i,j,len;
	faddr_t addr;

	if(!inbuf)
		return;
	fbuf=strstr((char *)inbuf,"\r * Origin: ");
	if(!fbuf)
	fbuf=strstr((char *)inbuf,"\n * Origin: ");
	if(!fbuf)
		fbuf=inbuf;
	if(seenbys->addr) {
		FREE(seenbys->addr);
		seenbys->addr=0;
		seenbys->addrs=0; }
	addr.zone=addr.net=addr.node=addr.point=seenbys->addrs=0;
	p=strstr((char *)fbuf,"\rSEEN-BY:");
	if(!p) p=strstr((char *)fbuf,"\nSEEN-BY:");
	if(p) {
		while(1) {
			sprintf(str,"%-.100s",p+10);
			if((p1=strchr(str,CR))!=NULL)
				*p1=0;
			p1=str;
			i=j=0;
			len=strlen(str);
			while(i<len) {
				j=i;
				while(i<len && *(p1+i)!=SP)
					++i;
				if(j>len)
					break;
				sprintf(seenby,"%-.*s",(i-j),p1+j);
				if((p2=strchr(seenby,':'))!=NULL) {
					addr.zone=atoi(seenby);
					addr.net=atoi(p2+1); }
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
				if((seenbys->addr=(faddr_t *)REALLOC(seenbys->addr
					,sizeof(faddr_t)*(seenbys->addrs+1)))==NULL) {
					printf("ERROR allocating memory for seenbys\n");
					logprintf("ERROR line %d allocating memory for message "
						"seenbys.",__LINE__);
					bail(1); }
				memcpy(&seenbys->addr[seenbys->addrs],&addr,sizeof(faddr_t));
				seenbys->addrs++;
				++i; }
			p1=strstr(p+10,"\rSEEN-BY:");
			if(!p1)
				p1=strstr(p+10,"\nSEEN-BY:");
			if(!p1)
				break;
			p=p1; } }
	else {
		if((seenbys->addr=(faddr_t *)REALLOC(seenbys->addr
			,sizeof(faddr_t)))==NULL) {
			printf("ERROR allocating memory for seenbys\n");
			logprintf("ERROR line %d allocating memory for message seenbys."
				,__LINE__);
			bail(1); }
		memset(&seenbys->addr[0],0,sizeof(faddr_t)); }

	if(paths->addr) {
		FREE(paths->addr);
		paths->addr=0;
		paths->addrs=0; }
	addr.zone=addr.net=addr.node=addr.point=paths->addrs=0;
	if((p=strstr((char *)fbuf,"\1PATH:"))!=NULL) {
		while(1) {
			sprintf(str,"%-.100s",p+7);
			if((p1=strchr(str,CR))!=NULL)
				*p1=0;
			p1=str;
			i=j=0;
			len=strlen(str);
			while(i<len) {
				j=i;
				while(i<len && *(p1+i)!=SP)
					++i;
				if(j>len)
					break;
				sprintf(seenby,"%-.*s",(i-j),p1+j);
				if((p2=strchr(seenby,':'))!=NULL) {
					addr.zone=atoi(seenby);
					addr.net=atoi(p2+1); }
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
				if((paths->addr=(faddr_t *)REALLOC(paths->addr
					,sizeof(faddr_t)*(paths->addrs+1)))==NULL) {
					printf("ERROR allocating memory for paths\n");
					logprintf("ERROR line %d allocating memory for message "
						"paths.",__LINE__);
					bail(1); }
				memcpy(&paths->addr[paths->addrs],&addr,sizeof(faddr_t));
				paths->addrs++;
				++i; }
			if((p1=strstr(p+7,"\1PATH:"))==NULL)
				break;
			p=p1; } }
	else {
		if((paths->addr=(faddr_t *)REALLOC(paths->addr
			,sizeof(faddr_t)))==NULL) {
			printf("ERROR allocating memory for paths\n");
			logprintf("ERROR line %d allocating memory for message paths."
				,__LINE__);
			bail(1); }
		memset(&paths->addr[0],0,sizeof(faddr_t)); }
}

/******************************************************************************
 This function takes the addrs passed to it and compares them to the address
 passed in inaddr.	1 is returned if inaddr matches any of the addrs
 otherwise a 0 is returned.
******************************************************************************/
int check_psb(addrlist_t addrlist,faddr_t inaddr)
{
	int i;

	for(i=0;i<addrlist.addrs;i++) {
		if(!memcmp(&addrlist.addr[i],&inaddr,sizeof(faddr_t)))
			return(1); 
	}
	return(0);
}
/******************************************************************************
 This function strips the message seen-bys and path from inbuf.
******************************************************************************/
void strip_psb(char HUGE16 *inbuf)
{
	char *p,HUGE16 *fbuf;

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
void attach_bundles(void)
{
	FILE *fidomsg;
	char str[1025],path[512],*packet;
	int fmsg;
	faddr_t pkt_faddr;
	pkthdr_t pkthdr;
	size_t	f;
	glob_t	g;

	sprintf(path,"%s*.pk_",cfg.outbound);
	glob(path,0,NULL,&g);
	for(f=0;f<g.gl_pathc && !kbhit();f++) {

		packet=g.gl_pathv[f];

		printf("%21s: %s ","Outbound Packet",packet);
		if((fmsg=sopen(packet,O_RDWR,SH_DENYRW))==-1) {
			printf("ERROR line %d opening.\n",__LINE__);
			logprintf("ERROR line %d opening %s",__LINE__,packet);
			continue; }
		if((fidomsg=fdopen(fmsg,"r+b"))==NULL) {
			close(fmsg);
			printf("\7ERROR fdopening.\n");
			logprintf("ERROR line %d fdopening %s",__LINE__,packet);
			continue; }
		if(filelength(fmsg)<sizeof(pkthdr_t)) {
			printf("ERROR invalid length of %lu bytes for %s\n",filelength(fmsg)
				,packet);
			logprintf("ERROR line %d invalid length of %lu bytes for %s"
				,__LINE__,filelength(fmsg),packet);
			fclose(fidomsg);
			if(delfile(packet))
				logprintf("ERROR line %d removing %s %s",__LINE__,packet
					,strerror(errno));
			continue; }
		if(fread(&pkthdr,sizeof(pkthdr_t),1,fidomsg)!=1) {
			fclose(fidomsg);
			printf("\7ERROR reading %u bytes from %s\n",sizeof(pkthdr_t),packet);
			logprintf("ERROR line %d reading %u bytes from %s",__LINE__
				,sizeof(pkthdr_t),packet);
			continue; }
		fseek(fidomsg,-2L,SEEK_END);
		fread(str,2,1,fidomsg);
		fclose(fidomsg);
		if(!str[0] && !str[1]) {	/* Check for two nulls at end of packet */
			pkt_faddr.zone=pkthdr.destzone;
			pkt_faddr.net=pkthdr.destnet;
			pkt_faddr.node=pkthdr.destnode;
			pkt_faddr.point=0;				/* No point info in the 2.0 hdr! */
			memcpy(&two_plus,&pkthdr.empty,20);
			if(two_plus.cword==_rotr(two_plus.cwcopy,8)  /* 2+ Packet Header */
				&& two_plus.cword && two_plus.cword&1)
				pkt_faddr.point=two_plus.destpoint;
			else if(pkthdr.baud==2) {				/* Type 2.2 Packet Header */
				memcpy(&two_two,&pkthdr.empty,20);
				pkt_faddr.point=pkthdr.month; }
			printf("Sending to %s\n",faddrtoa(&pkt_faddr,NULL));
			pack_bundle(packet,pkt_faddr); 
		} else
			printf("Possibly still in use\n"); 
	}
	globfree(&g);
}
/******************************************************************************
 This is where we put outgoing messages into packets.  Set the 'cleanup'
 parameter to 1 to force all the remaining packets closed and stuff them into
 a bundle.
******************************************************************************/
void pkt_to_pkt(uchar HUGE16 *fbuf,areasbbs_t area,faddr_t faddr
	,fmsghdr_t fmsghdr,addrlist_t seenbys,addrlist_t paths, int cleanup)
{
	int i,j,k,file;
	short node;
	time_t now;
	struct tm *tm;
	pkthdr_t pkthdr;
	static ushort openpkts,totalpkts;
	static outpkt_t outpkt[MAX_TOTAL_PKTS];
	faddr_t sysaddr;
	two_two_t two;
	two_plus_t two_p;


	if(cleanup==1) {
		for(i=0;i<totalpkts;i++) {
			if(i>=MAX_TOTAL_PKTS) {
				printf("MAX_TOTAL_PKTS (%d) REACHED!\n",MAX_TOTAL_PKTS);
				logprintf("MAX_TOTAL_PKTS (%d) REACHED!\n",MAX_TOTAL_PKTS);
				break;
			}
			if(outpkt[i].curopen) {
				fputc(0,outpkt[i].stream);
				fputc(0,outpkt[i].stream);
				fclose(outpkt[i].stream); }
			else {
				if((outpkt[i].stream=fnopen(&file,outpkt[i].filename
					,O_WRONLY|O_APPEND))==NULL) {
					printf("ERROR line %d opening %s for write.\n",__LINE__,outpkt[i].filename);
					logprintf("ERROR line %d opening %s %s",__LINE__
						,outpkt[i].filename,strerror(errno));
					continue; }
				fputc(0,outpkt[i].stream);
				fputc(0,outpkt[i].stream);
				fclose(outpkt[i].stream); }
	//		  pack_bundle(outpkt[i].filename,outpkt[i].uplink);
			memset(&outpkt[i],0,sizeof(outpkt_t)); }
		totalpkts=openpkts=0;
		attach_bundles();
		attachment(0,faddr,ATTACHMENT_NETMAIL);
		return; }

	if(fbuf==NULL) {
		printf("ERROR allocating fbuf\n");
		logprintf("ERROR line %d allocating fbuf",__LINE__);
		return; }
	/* We want to see if there's already a packet open for this area.   */
	/* If not, we'll open a new one.  Once we have a packet, we'll add  */
	/* messages to it as they come in.	If necessary, we'll close an    */
	/* open packet to open a new one.									*/

	for(j=0;j<area.uplinks;j++) {
		if((cleanup==2 && memcmp(&faddr,&area.uplink[j],sizeof(faddr_t))) ||
			(!cleanup && (!memcmp(&faddr,&area.uplink[j],sizeof(faddr_t)) ||
			check_psb(seenbys,area.uplink[j]))))
			continue;
		node=matchnode(area.uplink[j],0);
		if(node<cfg.nodecfgs && cfg.nodecfg[node].attr&ATTR_PASSIVE)
			continue;
		sysaddr=getsysfaddr(area.uplink[j].zone);
		printf("%s ",faddrtoa(&area.uplink[j],NULL));
		for(i=0;i<totalpkts;i++) {
			if(i>=MAX_TOTAL_PKTS) {
				printf("MAX_TOTAL_PKTS (%d) REACHED!\n",MAX_TOTAL_PKTS);
				logprintf("MAX_TOTAL_PKTS (%d) REACHED!\n",MAX_TOTAL_PKTS);
				break;
			}
			if(!memcmp(&area.uplink[j],&outpkt[i].uplink,sizeof(faddr_t))) {
				if(!outpkt[i].curopen) {
					if(openpkts==DFLT_OPEN_PKTS)
						for(k=0;k<totalpkts;k++) {
							if(outpkt[k].curopen) {
								fclose(outpkt[k].stream);
								outpkt[k].curopen=0;
								break; } }
					if((outpkt[i].stream=fnopen(&file,outpkt[i].filename
						,O_WRONLY|O_APPEND))==NULL) {
						printf("Unable to open %s for write.\n"
							,outpkt[i].filename);
						logprintf("ERROR line %d opening %s %s",__LINE__
							,outpkt[i].filename,strerror(errno));
						bail(1); }
					outpkt[i].curopen=1; }
				if((strlen((char *)fbuf)+1+ftell(outpkt[i].stream))
					<=cfg.maxpktsize) {
					fmsghdr.destnode=area.uplink[j].node;
					fmsghdr.destnet=area.uplink[j].net;
					fmsghdr.destzone=area.uplink[j].zone;
					putfmsg(outpkt[i].stream,fbuf,fmsghdr,area,seenbys,paths); }
				else {
					fputc(0,outpkt[i].stream);
					fputc(0,outpkt[i].stream);
					fclose(outpkt[i].stream);
	//				  pack_bundle(outpkt[i].filename,outpkt[i].uplink);
					outpkt[i].stream=outpkt[totalpkts-1].stream;
					memcpy(&outpkt[i],&outpkt[totalpkts-1],sizeof(outpkt_t));
					memset(&outpkt[totalpkts-1],0,sizeof(outpkt_t));
					--totalpkts;
					--openpkts;
					i=totalpkts; 
				}
				break; 
			} 
		}
		if(i==totalpkts) {
			if(openpkts==DFLT_OPEN_PKTS)
				for(k=0;k<totalpkts;k++) {
					if(outpkt[k].curopen) {
						fclose(outpkt[k].stream);
						outpkt[k].curopen=0;
						--openpkts;
						break; } }
			strcpy(outpkt[i].filename,pktname());
			now=time(NULL);
			tm=localtime(&now);
			if((outpkt[i].stream=fnopen(&file,outpkt[i].filename
				,O_WRONLY|O_CREAT))==NULL) {
				printf("Unable to open %s for write.\n"
					,outpkt[i].filename);
				logprintf("ERROR line %d opening %s %s"
					,__LINE__,outpkt[i].filename,strerror(errno));
				bail(1); }
			pkthdr.orignode=sysaddr.node;
			fmsghdr.destnode=pkthdr.destnode=area.uplink[j].node;
			if(node<cfg.nodecfgs && cfg.nodecfg[node].pkt_type==PKT_TWO_TWO) {
				pkthdr.year=sysaddr.point;
				pkthdr.month=area.uplink[j].point;
				pkthdr.day=0;
				pkthdr.hour=0;
				pkthdr.min=0;
				pkthdr.sec=0;
				pkthdr.baud=0x0002; }
			else {
				pkthdr.year=tm->tm_year+1900;
				pkthdr.month=tm->tm_mon;
				pkthdr.day=tm->tm_mday;
				pkthdr.hour=tm->tm_hour;
				pkthdr.min=tm->tm_min;
				pkthdr.sec=tm->tm_sec;
				pkthdr.baud=0; }
			pkthdr.pkttype=0x0002;
			pkthdr.orignet=sysaddr.net;
			fmsghdr.destnet=pkthdr.destnet=area.uplink[j].net;
			pkthdr.prodcode=0;
			pkthdr.sernum=0;
			if(node<cfg.nodecfgs)
				memcpy(pkthdr.password,cfg.nodecfg[node].pktpwd,8);
			else
				memset(pkthdr.password,0,8);
			pkthdr.origzone=sysaddr.zone;
			fmsghdr.destzone=pkthdr.destzone=area.uplink[j].zone;
			memset(pkthdr.empty,0,sizeof(two_two_t));

			if(node<cfg.nodecfgs) {
				if(cfg.nodecfg[node].pkt_type==PKT_TWO_TWO) {
					memset(&two,0,20);
					strcpy(two.origdomn,"fidonet");
					strcpy(two.destdomn,"fidonet");
					memcpy(&pkthdr.empty,&two,20); }
				else if(cfg.nodecfg[node].pkt_type==PKT_TWO_PLUS) {
					memset(&two_p,0,20);
					if(sysaddr.point) {
						pkthdr.orignet=-1;
						two_p.auxnet=sysaddr.net; }
					two_p.cwcopy=0x0100;
					two_p.prodcode=pkthdr.prodcode;
					two_p.revision=pkthdr.sernum;
					two_p.cword=0x0001;
					two_p.origzone=pkthdr.origzone;
					two_p.destzone=pkthdr.destzone;
					two_p.origpoint=sysaddr.point;
					two_p.destpoint=area.uplink[j].point;
					memcpy(&pkthdr.empty,&two_p,sizeof(two_plus_t)); }
			}
			fwrite(&pkthdr,sizeof(pkthdr_t),1,outpkt[totalpkts].stream);
			putfmsg(outpkt[totalpkts].stream,fbuf,fmsghdr,area,seenbys,paths);
			outpkt[totalpkts].curopen=1;
			memcpy(&outpkt[totalpkts].uplink,&area.uplink[j]
				,sizeof(faddr_t));
			++openpkts;
			++totalpkts;
			if(totalpkts>=MAX_TOTAL_PKTS) {
				fclose(outpkt[totalpkts-1].stream);
//				  pack_bundle(outpkt[totalpkts-1].filename
//					  ,outpkt[totalpkts-1].uplink);
				--totalpkts;
				--openpkts; 
			}
		}
	}
}

/**************************************/
/* Send netmail, returns 0 on success */
/**************************************/
int import_netmail(char *path,fmsghdr_t hdr, FILE *fidomsg)
{
	uchar info[512],str[256],tmp[256],subj[256]
		,HUGE16 *fmsgbuf=NULL,*p,*tp,*sp;
	int i,match,usernumber;
	ulong l;
	faddr_t addr;

	hdr.destzone=hdr.origzone=sys_faddr.zone;
	hdr.destpoint=hdr.origpoint=0;
	getzpt(fidomsg,&hdr);				/* use kludge if found */
	for(match=0;match<scfg.total_faddrs;match++)
		if((hdr.destzone==scfg.faddr[match].zone || misc&FUZZY_ZONE)
			&& hdr.destnet==scfg.faddr[match].net
			&& hdr.destnode==scfg.faddr[match].node
			&& hdr.destpoint==scfg.faddr[match].point)
			break;
	if(match<scfg.total_faddrs && misc&FUZZY_ZONE)
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

	if(!(misc&IMPORT_NETMAIL)) {
		if(!path[0]) {
			fmsgbuf=getfmsg(fidomsg,&l);
			if(!fmsgbuf) {
				printf("ERROR Netmail allocation");
				logprintf("ERROR line %d netmail allocation",__LINE__);
				return(2); }

			if(!l && misc&KILL_EMPTY_MAIL)
				printf("Empty NetMail - ");
			else {
				for(i=1;i;i++) {
					sprintf(str,"%s%u.msg",scfg.netmail_dir,i);
					if(!fexistcase(str))
						break; }
				if(!i) {
					printf("Too many netmail messages");
					logprintf("Too many netmail messages");
					return(2); }
				if((i=nopen(str,O_WRONLY|O_CREAT))==-1) {
					printf("ERROR line %d opening %s",__LINE__,str);
					logprintf("ERROR line %d opening %s",__LINE__,str);
					return(2); }
				write(i,&hdr,sizeof(hdr));
				write(i,fmsgbuf,strlen((char *)fmsgbuf)+1); /* Write the NULL too */
				close(i); }
			FREE(fmsgbuf); }
		printf("Ignored");
		if(cfg.log&LOG_IGNORED)
			logprintf("%s Ignored",info);
		return(-1); }

	if(hdr.attr&FIDO_ORPHAN) {
		printf("Orphaned");
		return(1); }
	if(!(misc&IGNORE_ADDRESS) && match==scfg.total_faddrs && path[0]) {
		printf("Skipped");
		return(2); }
	if(!(misc&IGNORE_RECV) && hdr.attr&FIDO_RECV) {
		printf("Already received");
		return(3); }
	if(hdr.attr&FIDO_LOCAL && !(misc&LOCAL_NETMAIL)) {
		printf("Created locally");
		return(4); }

	if(email->shd_fp==NULL) {
		sprintf(email->file,"%smail",scfg.data_dir);
		email->retry_time=scfg.smb_retry_time;
		if((i=smb_open(email))!=0) {
			printf("ERROR %d opening %s\n",i,email->file);
			logprintf("ERROR %d line %d opening %s",i,__LINE__,email->file);
			bail(1); } }

	if(!filelength(fileno(email->shd_fp))) {
		email->status.max_crcs=scfg.mail_maxcrcs;
		email->status.max_msgs=MAX_SYSMAIL;
		email->status.max_age=scfg.mail_maxage;
		email->status.attr=SMB_EMAIL;
		if((i=smb_create(email))!=0) {
			sprintf(str,"ERROR %d creating %s",i,email->file);
			printf("%s\n",str);
			logprintf(str);
			bail(1); } }

	if(!stricmp(hdr.to,"AREAFIX") || !stricmp(hdr.to,"SBBSECHO")) {
		fmsgbuf=getfmsg(fidomsg,NULL);
		if(path[0]) {
			if(misc&DELETE_NETMAIL) {
				fclose(fidomsg);
				if(delfile(path))
					logprintf("ERROR line %d removing %s %s",__LINE__,path
						,strerror(errno)); }
			else {
				hdr.attr|=FIDO_RECV;
				fseek(fidomsg,0L,SEEK_SET);
				fwrite(&hdr,sizeof(fmsghdr_t),1,fidomsg);
				fclose(fidomsg); } }	/* Gotta close it here for areafix stuff */
		addr.zone=hdr.origzone;
		addr.net=hdr.orignet;
		addr.node=hdr.orignode;
		addr.point=hdr.origpoint;
		SAFECOPY(hdr.to,scfg.sys_op);
		SAFECOPY(hdr.from,"SBBSecho");
		SAFECOPY(str,hdr.subj);
		SAFECOPY(hdr.subj,"Areafix Request");
		hdr.origzone=hdr.orignet=hdr.orignode=hdr.origpoint=0;
		p=process_areafix(addr,fmsgbuf,str);
		if(p && cfg.notify)
			if(fmsgtosmsg(p,hdr,cfg.notify,INVALID_SUB)==1) {
				sprintf(str,"\7\1n\1hSBBSecho \1n\1msent you mail\r\n");
				putsmsg(&scfg,cfg.notify,str); }
		if(fmsgbuf)
			FREE(fmsgbuf);
		if(cfg.log&LOG_AREAFIX)
			logprintf(info);
		return(-2); }

	usernumber=atoi(hdr.to);
	if(!stricmp(hdr.to,"SYSOP"))  /* NetMail to "sysop" goes to #1 */
		usernumber=1;
	if(!usernumber && match<scfg.total_faddrs)
		usernumber=matchname(hdr.to);
	if(!usernumber) {
		if(misc&UNKNOWN_NETMAIL && match<scfg.total_faddrs)	/* receive unknown user */
			usernumber=1;								/* mail to 1 */
		else {
			if(match<scfg.total_faddrs) {
				printf("Unknown user ");
				if(cfg.log&LOG_UNKNOWN)
					logprintf("%s Unknown user",info); }
	/***
			hdr.attr|=FIDO_ORPHAN;
			fseek(fidomsg,0L,SEEK_SET);
			fwrite(&hdr,sizeof(fmsghdr_t),1,fidomsg);
	***/
			if(!path[0]) {
				fmsgbuf=getfmsg(fidomsg,&l);
				if(!fmsgbuf) {
					printf("ERROR Netmail allocation");
					logprintf("ERROR line %d netmail allocation",__LINE__);
					return(2); }
				if(!l && misc&KILL_EMPTY_MAIL) {
					printf("Empty NetMail - Ignored");
					if(cfg.log&LOG_IGNORED)
						logprintf("%s Empty - Ignored",info); }
				else {
					for(i=1;i;i++) {
						sprintf(str,"%s%u.msg",scfg.netmail_dir,i);
						if(!fexistcase(str))
							break; }
					if(!i) {
						printf("Too many netmail messages");
						logprintf("Too many netmail messages");
						return(2); }
					if((i=nopen(str,O_WRONLY|O_CREAT))==-1) {
						printf("ERROR line %d opening %s",__LINE__,str);
						logprintf("ERROR line %d opening %s",__LINE__,str);
						return(2); }
					write(i,&hdr,sizeof(hdr));
					write(i,fmsgbuf,strlen((char *)fmsgbuf)+1); /* Write the NULL too */
					close(i); }
				FREE(fmsgbuf); }
			return(2); } }

	/*********************/
	/* Importing NetMail */
	/*********************/

	fmsgbuf=getfmsg(fidomsg,&l);

	if(!l && misc&KILL_EMPTY_MAIL) {
		printf("Empty NetMail - Ignored");
		if(cfg.log&LOG_IGNORED)
			logprintf("%s Empty - Ignored",info);
		if(fmsgbuf)
			FREE(fmsgbuf);
		return(0); }

	i=fmsgtosmsg(fmsgbuf,hdr,usernumber,INVALID_SUB);
	if(i!=1) {
		printf("ERROR (%d) Importing",i);
		logprintf("ERROR (%d) Importing %s",i,info);
		if(fmsgbuf)
			FREE(fmsgbuf);
		return(10); }

	addr.zone=hdr.origzone;
	addr.net=hdr.orignet;
	addr.node=hdr.orignode;
	addr.point=hdr.origpoint;
	sprintf(str,"\7\1n\1hSBBSecho: \1m%.36s \1n\1msent you NetMail from "
		"\1h%s\1n\r\n",hdr.from,faddrtoa(&addr,NULL));
	putsmsg(&scfg,usernumber,str);

	if(hdr.attr&FIDO_FILE) {	/* File attachment */
		strcpy(subj,hdr.subj);
		tp=subj;
		while(1) {
			p=strchr(tp,SP);
			if(p) *p=0;
			sp=strrchr(tp,'/');              /* sp is slash pointer */
			if(!sp) sp=strrchr(tp,'\\');
			if(sp) tp=sp+1;
			sprintf(str,"%s%s",scfg.fidofile_dir,tp);
			sprintf(tmp,"%sfile/%04u.in",scfg.data_dir,usernumber);
			MKDIR(tmp);
			backslash(tmp);
			strcat(tmp,tp);
			mv(str,tmp,0);
			if(!p)
				break;
			tp=p+1; } }
	netmail++;

	if(fmsgbuf)
		FREE(fmsgbuf);

	/***************************/
	/* Updating message header */
	/***************************/
	/***	NOT packet compatible
	if(!(misc&DONT_SET_RECV)) {
		hdr.attr|=FIDO_RECV;
		fseek(fidomsg,0L,SEEK_SET);
		fwrite(&hdr,sizeof(fmsghdr_t),1,fidomsg); }
	***/
	if(cfg.log&LOG_IMPORTED)
		logprintf("%s Imported",info);
	return(0);
}

/******************************************************************************
 This is where we export echomail.	This was separated from function main so
 it could be used for the remote rescan function.  Passing anything but an
 empty address as 'addr' designates that a rescan should be done for that
 address.
******************************************************************************/
void export_echomail(char *sub_code,faddr_t addr)
{
	char	str[1025],tear,cr;
	char*	buf=NULL;
	char*	minus;
	uchar*	fmsgbuf=NULL;
	ulong	fmsgbuflen;
	int		tzone;
	int		g,i,j,k=0,file;
	ulong	f,l,m,exp,ptr,msgs,lastmsg,posts,exported=0;
	float	export_time;
	smbmsg_t msg;
	smbmsg_t orig_msg;
	fmsghdr_t hdr;
	struct	tm *tm;
	faddr_t pkt_faddr;
	post_t HUGE16 *post;
	areasbbs_t fakearea;
	addrlist_t msg_seen,msg_path;
    clock_t start_tick=0,export_ticks=0;

	memset(&msg_seen,0,sizeof(addrlist_t));
	memset(&msg_path,0,sizeof(addrlist_t));
	memset(&fakearea,0,sizeof(areasbbs_t));
	memset(&pkt_faddr,0,sizeof(faddr_t));
	start_tick=0;

	printf("\nScanning for Outbound EchoMail...\n");

	for(g=0;g<scfg.total_grps;g++)
	for(i=0;i<scfg.total_subs;i++)
		if(scfg.sub[i]->misc&SUB_FIDO && scfg.sub[i]->grp==g) {
			for(j=0;j<cfg.areas;j++)	/* Skip areas with no uplinks */
				if(cfg.area[j].sub==i)
					break;
			if(j==cfg.areas || (j<cfg.areas && !cfg.area[j].uplinks))
				continue;
			if(addr.zone) { 		/* Skip areas not meant for this address */
				if(j<cfg.areas)
					for(k=0;k<cfg.area[j].uplinks;k++)
						if(!memcmp(&cfg.area[j].uplink[k],&addr,sizeof(faddr_t)))
							break;
				if(k==cfg.area[j].uplinks)
					continue; }
			if(sub_code[0] && stricmp(sub_code,scfg.sub[i]->code))
				continue;
			printf("\nScanning %-15.15s %s\n"
				,scfg.grp[scfg.sub[i]->grp]->sname,scfg.sub[i]->lname);
			ptr=0;
			if(!addr.zone && !(misc&IGNORE_MSGPTRS)) {
				sprintf(str,"%s%s.sfp",scfg.sub[i]->data_dir,scfg.sub[i]->code);
				if((file=nopen(str,O_RDONLY))!=-1) {
					read(file,&ptr,sizeof(time_t));
					close(file); } }

			msgs=getlastmsg(i,&lastmsg,0);
			if(!msgs || (!addr.zone && !(misc&IGNORE_MSGPTRS) && ptr>=lastmsg)) {
				printf("No new messages.");
				if(ptr>lastmsg && !addr.zone && !(misc&LEAVE_MSGPTRS)) {
					printf("Fixing new-scan pointer.");
					sprintf(str,"%s%s.sfp",scfg.sub[i]->data_dir,scfg.sub[i]->code);
					if((file=nopen(str,O_WRONLY|O_CREAT))==-1) {
						printf("\7ERROR %d line %d opening/creating %s"
							,errno,__LINE__,str);
						logprintf("ERROR %d line %d opening/creating %s"
							,errno,__LINE__,str); }
					else {
						write(file,&lastmsg,4);
						close(file); } }
				continue; }

			sprintf(smb[cur_smb].file,"%s%s"
				,scfg.sub[i]->data_dir,scfg.sub[i]->code);
			smb[cur_smb].retry_time=scfg.smb_retry_time;
			if((j=smb_open(&smb[cur_smb]))!=0) {
				printf("ERROR %d opening %s\n",j,smb[cur_smb].file);
				logprintf("ERROR %d line %d opening %s",j,__LINE__
					,smb[cur_smb].file);
				continue; }

			post=NULL;
			posts=loadmsgs(&post,ptr);

			if(!posts)	{ /* no new messages */
				smb_close(&smb[cur_smb]);
				if(post)
					LFREE(post);
				continue; }

			if(start_tick)
				export_ticks+=clock()-start_tick;
			start_tick=clock();

			for(m=exp=0;m<posts;m++) {
				printf("\r%8s %5lu of %-5lu  "
					,scfg.sub[i]->code,m+1,posts);
				msg.idx=post[m];
				if((k=smb_lockmsghdr(&smb[cur_smb],&msg))!=0) {
					printf("ERROR %d locking %s msghdr\n",k,smb[cur_smb].file);
					logprintf("ERROR %d line %d locking %s msghdr\n"
						,k,__LINE__,smb[cur_smb].file);
					continue; }
				k=smb_getmsghdr(&smb[cur_smb],&msg);
				if(k || msg.hdr.number!=post[m].number) {
					smb_unlockmsghdr(&smb[cur_smb],&msg);
					smb_freemsgmem(&msg);

					msg.hdr.number=post[m].number;
					if((k=smb_getmsgidx(&smb[cur_smb],&msg))!=0) {
						printf("ERROR %d reading %s index\n",k,smb[cur_smb].file);
						logprintf("ERROR %d line %d reading %s index",k,__LINE__
							,smb[cur_smb].file);
						continue; }
					if((k=smb_lockmsghdr(&smb[cur_smb],&msg))!=0) {
						printf("ERROR %d locking %s msghdr\n",k,smb[cur_smb].file);
						logprintf("ERROR %d line %d locking %s msghdr",k,__LINE__
							,smb[cur_smb].file);
						continue; }
					if((k=smb_getmsghdr(&smb[cur_smb],&msg))!=0) {
						smb_unlockmsghdr(&smb[cur_smb],&msg);
						printf("ERROR %d reading %s msghdr\n",k,smb[cur_smb].file);
						logprintf("ERROR %d line %d reading %s msghdr",k,__LINE__
							,smb[cur_smb].file);
						continue; } }

				if((!addr.zone && !(misc&EXPORT_ALL)
					&& msg.from_net.type==NET_FIDO)
					|| !strnicmp(msg.subj,"NE:",3)) {   /* no echo */
					smb_unlockmsghdr(&smb[cur_smb],&msg);
					smb_freemsgmem(&msg);
					continue; }  /* From a Fido node, ignore it */

				if(msg.from_net.type && msg.from_net.type!=NET_FIDO
					&& !(scfg.sub[i]->misc&SUB_GATE)) {
					smb_unlockmsghdr(&smb[cur_smb],&msg);
					smb_freemsgmem(&msg);
					continue; }

				memset(&hdr,0,sizeof(fmsghdr_t));	 /* Zero the header */
				hdr.origzone=scfg.sub[i]->faddr.zone;
				hdr.orignet=scfg.sub[i]->faddr.net;
				hdr.orignode=scfg.sub[i]->faddr.node;
				hdr.origpoint=scfg.sub[i]->faddr.point;

				hdr.attr=FIDO_LOCAL;
				if(msg.hdr.attr&MSG_PRIVATE)
					hdr.attr|=FIDO_PRIVATE;

				sprintf(hdr.from,"%.35s",msg.from);

				tm=localtime((time_t *)&msg.hdr.when_written.time);
				sprintf(hdr.time,"%02u %3.3s %02u  %02u:%02u:%02u"
					,tm->tm_mday,mon[tm->tm_mon],TM_YEAR(tm->tm_year)
					,tm->tm_hour,tm->tm_min,tm->tm_sec);

				sprintf(hdr.to,"%.35s",msg.to);

				sprintf(hdr.subj,"%.71s",msg.subj);

				buf=smb_getmsgtxt(&smb[cur_smb],&msg,GETMSGTXT_TAILS);
				if(!buf) {
					smb_unlockmsghdr(&smb[cur_smb],&msg);
					smb_freemsgmem(&msg);
					continue; 
				}
				fmsgbuflen=strlen((char *)buf)+4096; /* over alloc for kludge lines */
				fmsgbuf=MALLOC(fmsgbuflen);
				if(!fmsgbuf) {
					printf("ERROR allocating %lu bytes for fmsgbuf\n",fmsgbuflen);
					logprintf("ERROR line %d allocating %lu bytes for fmsgbuf"
						,__LINE__,fmsgbuflen);
					smb_unlockmsghdr(&smb[cur_smb],&msg);
					smb_freemsgmem(&msg);
					continue; 
				}
				fmsgbuflen-=1024;	/* give us a bit of a guard band here */

				tear=0;
				f=0;

				tzone=smb_tzutc(msg.hdr.when_written.zone);
				if(tzone<0) {
					minus="-";
					tzone=-tzone;
				} else
					minus="";
				f+=sprintf(fmsgbuf+f,"\1TZUTC: %s%02d%02u\r"		/* TZUTC (FSP-1001) */
					,minus,tzone/60,tzone%60);

				if(msg.ftn_flags!=NULL)
					f+=sprintf(fmsgbuf+f,"\1FLAGS %.256s\r", msg.ftn_flags);

				f+=sprintf(fmsgbuf+f,"\1MSGID: %.256s\r"
					,ftn_msgid(scfg.sub[i],&msg));

				if(msg.ftn_reply!=NULL)			/* use original REPLYID */
					f+=sprintf(fmsgbuf+f,"\1REPLY: %.256s\r", msg.ftn_reply);
				else if(msg.hdr.thread_orig) {	/* generate REPLYID */
					memset(&orig_msg,0,sizeof(orig_msg));
					orig_msg.hdr.number=msg.hdr.thread_orig;
					if(smb_getmsgidx(smb, &orig_msg))
						f+=sprintf(fmsgbuf+f,"\1REPLY: <%s>\r",smb->last_error);
					else {
						smb_lockmsghdr(&smb[cur_smb],&orig_msg);
						smb_getmsghdr(&smb[cur_smb],&orig_msg);
						smb_unlockmsghdr(&smb[cur_smb],&orig_msg);
						f+=sprintf(fmsgbuf+f,"\1REPLY: %.256s\r"
							,ftn_msgid(scfg.sub[i],&orig_msg));	
					}
				}
				if(msg.ftn_pid!=NULL)	/* use original PID */
					f+=sprintf(fmsgbuf+f,"\1PID: %.256s\r", msg.ftn_pid);
				if(msg.ftn_tid!=NULL)	/* use original TID */
					f+=sprintf(fmsgbuf+f,"\1TID: %.256s\r", msg.ftn_tid);
				else					/* generate TID */
					f+=sprintf(fmsgbuf+f,"\1TID: SBBSecho %s-%s r%s %s %s\r"
						,SBBSECHO_VER,PLATFORM_DESC,revision,__DATE__,compiler);

				/* Unknown kludge lines are added here */
				for(l=0;l<msg.total_hfields && f<fmsgbuflen;l++)
					if(msg.hfield[l].type == FIDOCTRL)
						f+=sprintf(fmsgbuf+f,"\1%.512s\r",(char*)msg.hfield_dat[l]);

				for(l=0,cr=1;buf[l] && f<fmsgbuflen;l++) {
					if(buf[l]==1) { /* Ctrl-A, so skip it and the next char */
						l++;
						if(!buf[l])
							break;
						continue; }
					
					if(misc&STRIP_LF && buf[l]==LF)	/* Ignore line feeds */
						continue;

					if(cr) {
						if(buf[l]=='-' && buf[l+1]=='-'
							&& buf[l+2]=='-'
							&& (buf[l+3]==SP || buf[l+3]==CR)) {
							if(misc&CONVERT_TEAR)	/* Convert to === */
								buf[l]=buf[l+1]=buf[l+2]='=';
							else
								tear=1; }
						else if(!strncmp((char *)buf+l," * Origin: ",11))
							buf[l+1]='#'; } /* Convert * Origin into # Origin */

					if(buf[l]==CR)
						cr=1;
					else
						cr=0;
					if(scfg.sub[i]->misc&SUB_ASCII || misc&ASCII_ONLY) {
						if(buf[l]<SP && buf[l]!=CR
							&& buf[l]!=LF)			/* Ctrl ascii */
							buf[l]='.';             /* converted to '.' */
						if((uchar)buf[l]>0x7f)		/* extended ASCII */
							buf[l]='*'; }           /* converted to '*' */

					fmsgbuf[f++]=buf[l]; }

				FREE(buf);
				fmsgbuf[f]=0;

				if(!(scfg.sub[i]->misc&SUB_NOTAG)) {
					if(!tear) {  /* No previous tear line */
						sprintf(str,"--- SBBSecho %s-%s\r"
							,SBBSECHO_VER,PLATFORM_DESC);
						strcat((char *)fmsgbuf,str); }

					sprintf(str," * Origin: %s (%s)\r"
						,scfg.sub[i]->origline[0] ? scfg.sub[i]->origline : scfg.origline
						,faddrtoa(&scfg.sub[i]->faddr,NULL));
					strcat((char *)fmsgbuf,str); }

				for(k=0;k<cfg.areas;k++)
					if(cfg.area[k].sub==i) {
						cfg.area[k].exported++;
						pkt_to_pkt(fmsgbuf,cfg.area[k]
							,(addr.zone) ? addr:pkt_faddr,hdr,msg_seen
							,msg_path,(addr.zone) ? 2:0);
						break; }
				FREE(fmsgbuf);
				exported++;
				exp++;
				printf("Exp: %lu ",exp);
				smb_unlockmsghdr(&smb[cur_smb],&msg);
				smb_freemsgmem(&msg); }

			smb_close(&smb[cur_smb]);
			LFREE(post);

			/***********************/
			/* Update FIDO_PTR.DAB */
			/***********************/
			if(!addr.zone && !(misc&LEAVE_MSGPTRS) && lastmsg>ptr) {
				sprintf(str,"%s%s.sfp",scfg.sub[i]->data_dir,scfg.sub[i]->code);
				if((file=nopen(str,O_WRONLY|O_CREAT))==-1) {
					printf("\7ERROR %d line %d opening/creating %s"
						,errno,__LINE__,str);
					logprintf("ERROR %d line %d opening/creating %s"
						,errno,__LINE__,str); }
				else {
					write(file,&lastmsg,4);
					close(file); } } }

	printf("\n");
	if(start_tick)	/* Last possible increment of export_ticks */
		export_ticks+=clock()-start_tick;

	pkt_to_pkt(buf,fakearea,pkt_faddr,hdr,msg_seen,msg_path,1);

	if(!addr.zone && cfg.log&LOG_AREA_TOTALS && exported)
		for(i=0;i<cfg.areas;i++)
			if(cfg.area[i].exported)
				logprintf("Exported: %5u msgs %8s -> %s"
					,cfg.area[i].exported,scfg.sub[cfg.area[i].sub]->code
					,cfg.area[i].name);

	export_time=((float)export_ticks)/(float)CLK_TCK;
	if(cfg.log&LOG_TOTALS && exported && export_time) {
		printf("\nExported %lu EchoMail messages in %.1f seconds "
			,exported,export_time);
		logprintf("Exported: %5lu msgs in %.1f sec (%.1f/min %.1f/sec)"
			,exported,export_time
			,export_time/60.0 ? (float)exported/(export_time/60.0) :(float)exported
			,(float)exported/export_time);
		if(export_time/60.0)
			printf("(%.1f/min) ",(float)exported/(export_time/60.0));
		printf("(%.1f/sec)\n",(float)exported/export_time); }

}
/***********************************/
/* Synchronet/FidoNet Message util */
/***********************************/
int main(int argc, char **argv)
{
	FILE*	fidomsg;
	char	packet[MAX_PATH+1];
	char	ch,str[1025],fname[256],path[512],sub_code[9]
			,*p,*tp
			,areatagstr[128],outbound[128]
			,password[16];
	uchar	HUGE16 *fmsgbuf=NULL;
	ushort	attr;
	int 	i,j,k,file,fmsg, grp, grunged;
	uint	subnum[MAX_OPEN_SMBS]={INVALID_SUB};
	ulong	echomail=0,l,m/* f, */,areatag;
	time_t	now;
	float	import_time;
	clock_t start_tick=0,import_ticks=0;
	struct	tm *tm;
	size_t	f;
	glob_t	g;
	size_t	offset;
	fmsghdr_t hdr;
	faddr_t addr,pkt_faddr;
	FILE	*stream;
	pkthdr_t pkthdr;
	addrlist_t msg_seen,msg_path;
	areasbbs_t fakearea,curarea;
	char *usage="\n"
	"usage: sbbsecho [cfg_file] [-switches] [sub_code]\n"
	"\n"
	"where: cfg_file is the filename of config file (default is ctrl/sbbsecho.cfg)\n"
	"       sub_code is the internal code for a sub-board (default is ALL subs)\n"
	"\n"
	"valid switches:\n"
	"\n"
	"p: do not import packets                 x: do not delete packets after import\n"
	"n: do not import netmail                 d: do not delete netmail after import\n"
	"i: do not import echomail                e: do not export echomail\n"
	"m: ignore echomail ptrs (export all)     u: update echomail ptrs (export none)\n"
	"j: ignore recieved bit on netmail        t: do not update echomail ptrs\n"
	"l: create log file (data/sbbsecho.log)   r: create report of import totals\n"
	"h: export all echomail (hub rescan)      b: import locally created netmail too\n"
	"a: export ASCII characters only          f: create packets for outbound netmail\n"
	"g: generate notify lists                 =: change existing tear lines to ===\n"
	"y: import netmail for unknown users to sysop\n"
	"o: import all netmail regardless of destination address\n"
	"s: import private echomail override (strip private status)\n"
	"!: notify users of received echomail     @: prompt for key upon exiting (debug)\n";

	if((email=(smb_t *)MALLOC(sizeof(smb_t)))==NULL) {
		printf("ERROR allocating memory for email.\n");
		bail(1); }
	memset(email,0,sizeof(smb_t));
	if((smb=(smb_t *)MALLOC(MAX_OPEN_SMBS*sizeof(smb_t)))==NULL) {
		printf("ERROR allocating memory for smbs.\n");
		bail(1); }
	for(i=0;i<MAX_OPEN_SMBS;i++)
		memset(&smb[i],0,sizeof(smb_t));
	memset(&cfg,0,sizeof(config_t));
	memset(&msg_seen,0,sizeof(addrlist_t));
	memset(&msg_path,0,sizeof(addrlist_t));
	memset(&fakearea,0,sizeof(areasbbs_t));

	sscanf("$Revision$", "%*s %s", revision);

	DESCRIBE_COMPILER(compiler);

	printf("\nSBBSecho v%s-%s (rev %s) - Synchronet FidoNet Packet "
		"Tosser\n"
		,SBBSECHO_VER
#ifdef PLATFORM_DESC
		,PLATFORM_DESC
#else
	#if defined(__OS2__)
		,"OS/2"
	#elif defined(__NT__)
		,"Win32"
	#elif defined(__DOS4G__)
		,"DOS4G"
	#elif defined(__FLAT__)
		,"DOS32"
	#else
		,"DOS16"
	#endif
#endif
		,revision
		);
#if 0
	putenv("TZ=UTC0");
#endif

	putenv("TMP=");
#if !defined(__unix__)
	_fmode=O_BINARY;
#endif
	setvbuf(stdout,NULL,_IONBF,0);

	sub_code[0]=0;

	for(i=1;i<argc;i++) {
		if(argv[i][0]=='-'
#if !defined(__unix__)
			|| argv[i][0]=='/'
#endif
			) {
			j=1;
			while(argv[i][j]) {
				switch(toupper(argv[i][j])) {
					case 'A':
						misc|=ASCII_ONLY;
						break;
					case 'B':
						misc|=LOCAL_NETMAIL;
						break;
					case 'D':
						misc&=~DELETE_NETMAIL;
						break;
					case 'E':
						misc&=~EXPORT_ECHOMAIL;
						break;
					case 'F':
						misc|=PACK_NETMAIL;
						break;
					case 'G':
						misc|=GEN_NOTIFY_LIST;
						break;
					case 'H':
						misc|=EXPORT_ALL;
						break;
					case 'I':
						misc&=~IMPORT_ECHOMAIL;
						break;
					case 'J':
						misc|=IGNORE_RECV;
						break;
					case 'L':
						misc|=LOGFILE;
						break;
					case 'M':
						misc|=IGNORE_MSGPTRS;
						break;
					case 'N':
						misc&=~IMPORT_NETMAIL;
						break;
					case 'O':
						misc|=IGNORE_ADDRESS;
						break;
					case 'P':
						misc&=~IMPORT_PACKETS;
						break;
					case 'R':
						misc|=REPORT;
						break;
					case 'S':
						misc|=IMPORT_PRIVATE;
						break;
					case 'T':
						misc|=LEAVE_MSGPTRS;
						break;
					case 'U':
						misc|=UPDATE_MSGPTRS;
						misc&=~EXPORT_ECHOMAIL;
						break;
					case 'X':
						misc&=~DELETE_PACKETS;
						break;
					case 'Y':
						misc|=UNKNOWN_NETMAIL;
						break;
					case '=':
						misc|=CONVERT_TEAR;
						break;
					case '!':
						misc|=NOTIFY_RECEIPT;
						break;
					case '@':
						pause_on_exit=TRUE;
						break;
					case 'Q':
						bail(0);
					default:
						printf(usage);
						bail(0); }
				j++; } }
		else {
			if(strchr(argv[i],'\\') || strchr(argv[i],'/') 
				|| argv[i][1]==':' || strchr(argv[i],'.'))
				sprintf(cfg.cfgfile,"%.100s",argv[i]);
			else
				sprintf(sub_code,"%.8s",argv[i]); }  }

	if(!(misc&(IMPORT_NETMAIL|IMPORT_ECHOMAIL)))
		misc&=~IMPORT_PACKETS;

	p=getenv("SBBSCTRL");
	if(p==NULL) {
		printf("\7\nSBBSCTRL environment variable not set.\n");
		bail(1); }
	SAFECOPY(scfg.ctrl_dir,p); 

	if(chdir(scfg.ctrl_dir)!=0)
		printf("!ERROR changing directory to: %s", scfg.ctrl_dir);

    printf("\nLoading configuration files from %s\n", scfg.ctrl_dir);
	scfg.size=sizeof(scfg);
	SAFECOPY(str,UNKNOWN_LOAD_ERROR);
	if(!load_cfg(&scfg, NULL, TRUE, str)) {
		printf("!ERROR %s\n",str);
		printf("!Failed to load configuration files\n");
		bail(1);
	}

	sprintf(str,"%stwitlist.cfg",scfg.ctrl_dir);
	twit_list=fexist(str);

	if(scfg.total_faddrs<1) {
		sys_faddr.zone=sys_faddr.net=sys_faddr.node=1;
		sys_faddr.point=0; }
	else
		sys_faddr=scfg.faddr[0];

	if(!cfg.cfgfile[0])
		sprintf(cfg.cfgfile,"%ssbbsecho.cfg",scfg.ctrl_dir);
	strcpy(cfg.inbound,scfg.fidofile_dir);
	sprintf(cfg.areafile,"%sareas.bbs",scfg.data_dir);
	sprintf(cfg.logfile,"%ssbbsecho.log",scfg.logs_dir);

	read_echo_cfg();

	if(misc&LOGFILE)
		if((fidologfile=fnopen(NULL,cfg.logfile,O_RDWR|O_CREAT|O_DENYNONE))==NULL) {
			printf("\7ERROR line %d opening %s\n",__LINE__,cfg.logfile);
			bail(1); 
		}

	/******* READ IN AREAS.BBS FILE *********/

	printf("Reading %s",cfg.areafile);
	if((stream=fnopen(&file,cfg.areafile,O_RDONLY))==NULL) {
		printf("\nError opening %s for read: %s\n"
			,cfg.areafile,strerror(errno));
		bail(1); }
	cfg.areas=0;		/* Total number of areas in AREAS.BBS */
	cfg.area=NULL;
	while(1) {
		if(!fgets(str,1024,stream))
			break;
		truncsp(str);
		p=str;
		while(*p && *p<=SP) p++;	/* Find first printable char */
		if(*p==';' || !*p)          /* Ignore blank lines or start with ; */
			continue;
		if((cfg.area=(areasbbs_t *)REALLOC(cfg.area,sizeof(areasbbs_t)*
			(cfg.areas+1)))==NULL) {
			printf("ERROR allocating memory for area #%u.\n",cfg.areas+1);
			bail(1); }
		memset(&cfg.area[cfg.areas],0,sizeof(areasbbs_t));

		cfg.area[cfg.areas].sub=INVALID_SUB;	/* Default to passthru */

		sprintf(tmp,"%-.8s",p);
		tp=tmp;
		while(*tp>SP) tp++;
		*tp=0;
		for(i=0;i<scfg.total_subs;i++)
			if(!stricmp(tmp,scfg.sub[i]->code))
				break;
		if(i<scfg.total_subs)
			cfg.area[cfg.areas].sub=i;
		else if(stricmp(tmp,"P")) {
			printf("\n%s: Unrecongized internal code, assumed passthru",tmp);
			logprintf("%s: Unrecognized internal code, assumed passthru",tmp); }

		while(*p>SP) p++;				/* Skip code */
		while(*p && *p<=SP) p++;		/* Skip white space */
		sprintf(tmp,"%-.50s",p);        /* Area tag */
		if((tp=strchr(tmp,TAB))!=NULL)	/* Chop off any TABs */
			*tp=0;
		if((tp=strchr(tmp,SP))!=NULL)	/* Chop off any spaces */
			*tp=0;
		strupr(tmp);
		if(tmp[0]=='*')         /* UNKNOWN-ECHO area */
			cfg.badecho=cfg.areas;
		if((cfg.area[cfg.areas].name=(char *)MALLOC(strlen(tmp)+1))==NULL) {
			printf("ERROR allocating memory for area #%u tag name.\n"
				,cfg.areas+1);
			bail(1); }
		strcpy(cfg.area[cfg.areas].name,tmp);
		strupr(tmp);
		cfg.area[cfg.areas].tag=crc32(tmp,0);

		while(*p>SP) p++;				/* Skip tag */
		while(*p && *p<=SP) p++;		/* Skip white space */

		while(*p && *p!=';') {
			if((cfg.area[cfg.areas].uplink=(faddr_t *)
				REALLOC(cfg.area[cfg.areas].uplink
				,sizeof(faddr_t)*(cfg.area[cfg.areas].uplinks+1)))==NULL) {
				printf("ERROR allocating memory for area #%u uplinks.\n"
					,cfg.areas+1);
				bail(1); }
			cfg.area[cfg.areas].uplink[cfg.area[cfg.areas].uplinks]=atofaddr(p);
			while(*p>SP) p++;			/* Skip address */
			while(*p && *p<=SP) p++;	/* Skip white space */
			cfg.area[cfg.areas].uplinks++; }

		if(cfg.area[cfg.areas].sub!=INVALID_SUB || cfg.area[cfg.areas].uplinks)
			cfg.areas++;		/* Don't allocate if no tossing */
		}
	fclose(stream);

	printf("\n");

	if(!cfg.areas) {
		printf("No areas defined!\n");
		bail(1); 
	}

	#if 0	/* AREAS.BBS DEBUG */
		for(i=0;i<cfg.areas;i++) {
			printf("%4u: %-8s"
				,i+1
				,cfg.area[i].sub==INVALID_SUB ? "Passthru" :
				scfg.sub[cfg.area[i].sub]->code);
			for(j=0;j<cfg.area[i].uplinks;j++)
				printf(" %s",faddrtoa(&cfg.area[i].uplink[j],NULL));
			printf("\n"); }
	#endif

	if(misc&GEN_NOTIFY_LIST) {
		printf("\nGenerating Notify Lists...\n");
		notify_list(); 
	}

	/* Find any packets that have been left behind in the OUTBOUND directory */
	printf("\nScanning for Stray Outbound Packets...\n");
	sprintf(path,"%s*.pk_",cfg.outbound);
	glob(path,0,NULL,&g);
	for(f=0;f<g.gl_pathc && !kbhit();f++) {

		strcpy(packet,(char*)g.gl_pathv[f]);

		printf("%21s: %s ","Outbound Packet",packet);
		if((fmsg=sopen(packet,O_RDWR,SH_DENYRW))==-1) {
			printf("ERROR line %d opening.\n",__LINE__);
			logprintf("ERROR line %d opening %s",__LINE__,packet);
			continue; }
		if((fidomsg=fdopen(fmsg,"r+b"))==NULL) {
			close(fmsg);
			printf("\7ERROR fdopening.\n");
			logprintf("ERROR line %d fdopening %s",__LINE__,packet);
			continue; }
		if(filelength(fmsg)<sizeof(pkthdr_t)) {
			printf("ERROR invalid length of %lu bytes for %s\n",filelength(fmsg)
				,packet);
			logprintf("ERROR line %d invalid length of %lu bytes for %s"
				,__LINE__,filelength(fmsg),packet);
			fclose(fidomsg);
			if(delfile(packet))
				logprintf("ERROR line %d removing %s %s",__LINE__,packet
					,strerror(errno));
			continue; }
		if(fread(&pkthdr,sizeof(pkthdr_t),1,fidomsg)!=1) {
			fclose(fidomsg);
			printf("\7ERROR reading %u bytes from %s\n",sizeof(pkthdr_t),packet);
			logprintf("ERROR line %d reading %u bytes from %s",__LINE__
				,sizeof(pkthdr_t),packet);
			continue; }
		if((fdate(packet)+(60L*60L))<=time(NULL)) {
			fseek(fidomsg,-3L,SEEK_END);
			fread(str,3,1,fidomsg);
			if(str[2])						/* No ending NULL, probably junk */
				fputc(0,fidomsg);
			if(str[1])
				fputc(0,fidomsg);
			if(str[0])
				fputc(0,fidomsg);
			fclose(fidomsg);
			pkt_faddr.zone=pkthdr.destzone;
			pkt_faddr.net=pkthdr.destnet;
			pkt_faddr.node=pkthdr.destnode;
			pkt_faddr.point=0;				/* No point info in the 2.0 hdr! */
			memcpy(&two_plus,&pkthdr.empty,20);
			if(two_plus.cword==_rotr(two_plus.cwcopy,8)  /* 2+ Packet Header */
				&& two_plus.cword && two_plus.cword&1)
				pkt_faddr.point=two_plus.destpoint;
			else if(pkthdr.baud==2) {				/* Type 2.2 Packet Header */
				memcpy(&two_two,&pkthdr.empty,20);
				pkt_faddr.point=pkthdr.month; }
			printf("Sending to %s\n",faddrtoa(&pkt_faddr,NULL));
			pack_bundle(packet,pkt_faddr); }
		else {
			fclose(fidomsg);
			printf("Possibly still in use\n"); 
		} 
	}
	globfree(&g);

	if(misc&IMPORT_PACKETS) {

		printf("\nScanning for Inbound Packets...\n");

		/* We want to loop while there are bundles waiting for us, but first we want */
		/* to take care of any packets that may already be hanging around for some	 */
		/* reason or another (thus the do/while loop) */

		echomail=0;
		for(secure=0;secure<2;secure++) {
			if(secure && !cfg.secure[0])
				break;
		do {
		/****** START OF IMPORT PKT ROUTINE ******/

		offset=strlen(secure ? cfg.secure : cfg.inbound);
#ifdef __unix__
		sprintf(path,"%s*.[Pp][Kk][Tt]",secure ? cfg.secure : cfg.inbound);
#else
		sprintf(path,"%s*.pkt",secure ? cfg.secure : cfg.inbound);
#endif
		glob(path,0,NULL,&g);
		for(f=0;f<g.gl_pathc && !kbhit();f++) {

			strcpy(packet,g.gl_pathv[f]);

			if((fidomsg=fnopen(&fmsg,packet,O_RDWR))==NULL) {
				printf("\7ERROR line %d opening %s\n",__LINE__,packet);
				logprintf("ERROR line %d opening %s %s",__LINE__,packet
					,strerror(errno));
				continue; 
			}
			if(filelength(fmsg)<sizeof(pkthdr_t)) {
				printf("\7Invalid length of %lu bytes\n",filelength(fmsg));
				fclose(fidomsg);
				continue; 
			}

			fseek(fidomsg,-2L,SEEK_END);
			fread(str,2,1,fidomsg);
			if((str[0] || str[1]) &&
				(fdate(packet)+(48L*60L*60L))<=time(NULL)) {
				fclose(fidomsg);
				printf("\7ERROR packet %s not terminated correctly\n",packet);
				logprintf("ERROR line %d packet %s not terminated correctly",__LINE__
					,packet);
				continue; 
			}
			fseek(fidomsg,0L,SEEK_SET);
			if(fread(&pkthdr,sizeof(pkthdr_t),1,fidomsg)!=1) {
				fclose(fidomsg);
				printf("\7ERROR reading %u bytes\n",sizeof(pkthdr_t));
				logprintf("ERROR line %d reading %u bytes from %s",__LINE__
					,sizeof(pkthdr_t),packet);
				continue; 
			}

			pkt_faddr.zone=pkthdr.origzone ? pkthdr.origzone:sys_faddr.zone;
			pkt_faddr.net=pkthdr.orignet;
			pkt_faddr.node=pkthdr.orignode;
			pkt_faddr.point=0;

			printf("%21s: %s "
				,secure ? "Importing Secure Pkt" : "Importing Packet",packet+offset);
			memcpy(&two_plus,&pkthdr.empty,20);
			if(two_plus.cword==_rotr(two_plus.cwcopy,8)  /* 2+ Packet Header */
				&& two_plus.cword && two_plus.cword&1) {
				pkt_type=PKT_TWO_PLUS;
				pkt_faddr.point=two_plus.origpoint ? two_plus.origpoint:0;
				if(pkt_faddr.point && pkthdr.orignet==-1)
					pkt_faddr.net=two_plus.auxnet ? two_plus.auxnet:sys_faddr.zone;
				printf("(Type 2+)");
				if(cfg.log&LOG_PACKETS)
					logprintf("Importing %s%s (Type 2+) from %s"
						,secure ? "(secure) ":"",packet+offset,faddrtoa(&pkt_faddr,NULL)); 
			}
			else if(pkthdr.baud==2) {				/* Type 2.2 Packet Header */
				pkt_type=PKT_TWO_TWO;
				memcpy(&two_two,&pkthdr.empty,20);
				pkt_faddr.point=pkthdr.year ? pkthdr.year:0;
				printf("(Type 2.2)");
				if(cfg.log&LOG_PACKETS)
					logprintf("Importing %s%s (Type 2.2) from %s"
						,secure ? "(secure) ":"",packet+offset,faddrtoa(&pkt_faddr,NULL)); 
			}
			else {
				pkt_type=PKT_TWO;
				printf("(Type 2)");
				if(cfg.log&LOG_PACKETS)
					logprintf("Importing %s%s (Type 2) from %s"
						,secure ? "(secure) ":"",packet+offset,faddrtoa(&pkt_faddr,NULL)); 
			}

			printf(" from %s\n",faddrtoa(&pkt_faddr,NULL));

			if(misc&SECURE) {
				k=matchnode(pkt_faddr,1);
				sprintf(password,"%.8s",pkthdr.password);
				if(k<cfg.nodecfgs && cfg.nodecfg[k].pktpwd[0] &&
					stricmp(password,cfg.nodecfg[k].pktpwd)) {
					sprintf(str,"Packet %s from %s - "
						"Incorrect password ('%s' instead of '%s')"
						,packet+offset,faddrtoa(&pkt_faddr,NULL)
						,password,cfg.nodecfg[k].pktpwd);
					printf("Security Violation (Incorrect Password)\n");
					if(cfg.log&LOG_SECURITY)
						logprintf(str);
					fclose(fidomsg);
					continue; 
				} 
			}

			while(!feof(fidomsg)) {

				memset(&hdr,0,sizeof(fmsghdr_t));

				if(start_tick)
					import_ticks+=clock()-start_tick;
				start_tick=clock();

				if(fmsgbuf) {
					FREE(fmsgbuf);
					fmsgbuf=0; 
				}
				if(!fread(&ch,1,1,fidomsg)) 		 /* Message type (0200h) */
					break;
				if(ch!=02)
					continue;
				if(!fread(&ch,1,1,fidomsg))
					break;
				if(ch!=00)
					continue;
				fread(&hdr.orignode,2,1,fidomsg);
				fread(&hdr.destnode,2,1,fidomsg);
				fread(&hdr.orignet,2,1,fidomsg);
				fread(&hdr.destnet,2,1,fidomsg);
				fread(&hdr.attr,2,1,fidomsg);
				fread(&hdr.cost,2,1,fidomsg);

				grunged=0;

				for(i=0;i<sizeof(hdr.time);i++) 		/* Read in the Date/Time */
					if(!fread(hdr.time+i,1,1,fidomsg) || !hdr.time[i])
						break;
				if(i==sizeof(hdr.time)) grunged=1;

				for(i=0;!grunged && i<sizeof(hdr.to);i++) /* Read in the 'To' Field */
					if(!fread(hdr.to+i,1,1,fidomsg) || !hdr.to[i])
						break;
				if(i==sizeof(hdr.to)) grunged=1;

				for(i=0;!grunged && i<sizeof(hdr.from);i++) /* Read in 'From' Field */
					if(!fread(hdr.from+i,1,1,fidomsg) || !hdr.from[i])
						break;
				if(i==sizeof(hdr.from)) grunged=1;

				for(i=0;!grunged && i<sizeof(hdr.subj);i++) /* Read in 'Subj' Field */
					if(!fread(hdr.subj+i,1,1,fidomsg) || !hdr.subj[i])
						break;
				if(i==sizeof(hdr.subj)) grunged=1;

				str[0]=0;
				for(i=0;!grunged && i<sizeof(str);i++)	/* Read in the 'AREA' Field */
					if(!fread(str+i,1,1,fidomsg) || str[i]==CR)
						break;
				if(i<sizeof(str))
					str[i]=0;
				else
					grunged=1;

				if(!str[0] || grunged) {
					start_tick=0;
					if(cfg.log&LOG_GRUNGED)
						logprintf("Grunged message");
					seektonull(fidomsg);
					printf("Grunged message!\n");
					continue; 
				}

				if(i)
					fseek(fidomsg,(long)-(i+1),SEEK_CUR);

				truncsp(str);
				strupr(str);
				p=strstr(str,"AREA:");
				if(!p) {					/* Netmail */
					printf("AREA tag not found, calling import_netmail\n");
					start_tick=0;
					if(import_netmail("",hdr,fidomsg))
						seektonull(fidomsg);
					printf("\n");
					continue; 
				}

				if(!(misc&IMPORT_ECHOMAIL)) {
					start_tick=0;
					printf("EchoMail Ignored");
					seektonull(fidomsg);
					printf("\n");
					continue; 
				}

				p+=5;								/* Skip "AREA:" */
				while(*p && *p<=SP) p++;			/* Skip any white space */
				printf("%21s: ",p);                 /* Show areaname: */
				SAFECOPY(areatagstr,p);
				strupr(p);
				areatag=crc32(p,0);

				for(i=0;i<cfg.areas;i++)				/* Do we carry this area? */
					if(cfg.area[i].tag==areatag) {
						if(cfg.area[i].sub!=INVALID_SUB)
							printf("%s ",scfg.sub[cfg.area[i].sub]->code);
						else
							printf("(Passthru) ");
						fmsgbuf=getfmsg(fidomsg,NULL);
						gen_psb(&msg_seen,&msg_path,fmsgbuf,pkthdr.destzone);
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
						gen_psb(&msg_seen,&msg_path,fmsgbuf,pkthdr.destzone); 
					}
					else {
						start_tick=0;
						printf("Skipped\n");
						seektonull(fidomsg);
						continue; 
					} 
				}

				if(misc&SECURE && cfg.area[i].sub!=INVALID_SUB) {
					for(j=0;j<cfg.area[i].uplinks;j++)
						if(!memcmp(&cfg.area[i].uplink[j],&pkt_faddr,sizeof(faddr_t)))
							break;
					if(j==cfg.area[i].uplinks) {
						if(cfg.log&LOG_SECURITY)
							logprintf("%s: Security violation - %s not in AREAS.BBS"
								,areatagstr,faddrtoa(&pkt_faddr,NULL));
						printf("Security Violation (Not in AREAS.BBS)\n");
						seektonull(fidomsg);
						continue; 
					} 
				}

				/* From here on out, i = area number and area[i].sub = sub number */

				memcpy(&curarea,&cfg.area[i],sizeof(areasbbs_t));
				curarea.name=areatagstr;

				if(cfg.area[i].sub==INVALID_SUB) {			/* Passthru */
					start_tick=0;
					strip_psb(fmsgbuf);
					pkt_to_pkt(fmsgbuf,curarea,pkt_faddr,hdr,msg_seen,msg_path,0);
					printf("\n");
					continue; 
				} 						/* On to the next message */


				for(j=0;j<scfg.total_faddrs;j++)
					if(check_psb(msg_path,scfg.faddr[j]))
						break;
				if(j<scfg.total_faddrs) {
					start_tick=0;
					printf("Circular path (%s) ",faddrtoa(&scfg.faddr[j],NULL));
					cfg.area[i].circular++;
					if(cfg.log&LOG_CIRCULAR)
						logprintf("%s: Circular path detected for %s"
							,areatagstr,faddrtoa(&scfg.faddr[j],NULL));
					strip_psb(fmsgbuf);
					pkt_to_pkt(fmsgbuf,curarea,pkt_faddr,hdr,msg_seen,msg_path,0);
					printf("\n");
					continue; 
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
					if((j=smb_open(&smb[cur_smb]))!=0) {
						sprintf(str,"ERROR %d opening %s area #%d, sub #%d)"
							,j,smb[cur_smb].file,i+1,cfg.area[i].sub+1);
						printf(str);
						logprintf(str);
						strip_psb(fmsgbuf);
						pkt_to_pkt(fmsgbuf,curarea,pkt_faddr,hdr,msg_seen
							,msg_path,0);
						printf("\n");
						continue; 
					}
					if(!filelength(fileno(smb[cur_smb].shd_fp))) {
						smb[cur_smb].status.max_crcs=scfg.sub[cfg.area[i].sub]->maxcrcs;
						smb[cur_smb].status.max_msgs=scfg.sub[cfg.area[i].sub]->maxmsgs;
						smb[cur_smb].status.max_age=scfg.sub[cfg.area[i].sub]->maxage;
						smb[cur_smb].status.attr=scfg.sub[cfg.area[i].sub]->misc&SUB_HYPER
								? SMB_HYPERALLOC:0;
						if((j=smb_create(&smb[cur_smb]))!=0) {
							sprintf(str,"ERROR %d creating %s",j,smb[cur_smb].file);
							printf(str);
							logprintf(str);
							smb_close(&smb[cur_smb]);
							strip_psb(fmsgbuf);
							pkt_to_pkt(fmsgbuf,curarea,pkt_faddr,hdr,msg_seen
								,msg_path,0);
							printf("\n");
							continue; 
						} 
					}

					subnum[cur_smb]=cfg.area[i].sub;
				}

				if(hdr.attr&FIDO_PRIVATE && !(scfg.sub[cfg.area[i].sub]->misc&SUB_PRIV)) {
					if(misc&IMPORT_PRIVATE)
						hdr.attr&=~FIDO_PRIVATE;
					else {
						start_tick=0;
						printf("Private posts disallowed.");
						if(cfg.log&LOG_PRIVATE)
							logprintf("%s: Private posts disallowed"
								,areatagstr);
						strip_psb(fmsgbuf);
						pkt_to_pkt(fmsgbuf,curarea,pkt_faddr,hdr,msg_seen
							,msg_path,0);
						printf("\n");
						continue; 
					} 
				}

				if(!(hdr.attr&FIDO_PRIVATE) && scfg.sub[cfg.area[i].sub]->misc&SUB_PONLY)
					hdr.attr|=MSG_PRIVATE;

				/**********************/
				/* Importing EchoMail */
				/**********************/
				j=fmsgtosmsg(fmsgbuf,hdr,0,cfg.area[i].sub);

				if(start_tick) {
					import_ticks+=clock()-start_tick;
					start_tick=0; 
				}

				if(j==-1) {
					if(cfg.log&LOG_DUPES)
						logprintf("%s Duplicate message",areatagstr);
					cfg.area[i].dupes++; 
				}
				else {	   /* Not a dupe */
					strip_psb(fmsgbuf);
					pkt_to_pkt(fmsgbuf,curarea,pkt_faddr
						,hdr,msg_seen,msg_path,0); 
				}

				if(j==1) {		/* Successful import */
					echomail++;
					cfg.area[i].imported++;
					if(misc&NOTIFY_RECEIPT && (m=matchname(hdr.to))!=0) {
						sprintf(str
						,"\7\1n\1hSBBSecho: \1m%.36s \1n\1msent you EchoMail on "
							"\1h%s \1n\1m%s\1n\r\n"
							,hdr.from
							,scfg.grp[scfg.sub[cfg.area[i].sub]->grp]->sname
							,scfg.sub[cfg.area[i].sub]->sname);
						putsmsg(&scfg,m,str); 
					} 
				}
				printf("\n");
			}
			fclose(fidomsg);

			if(misc&DELETE_PACKETS)
				if(delfile(packet))
					logprintf("ERROR line %d removing %s %s",__LINE__,packet
						,strerror(errno)); 
		}
		globfree(&g);

		if(start_tick) {
			import_ticks+=clock()-start_tick;
			start_tick=0; 
		}

		} while(!kbhit() && unpack_bundle());

		if(kbhit()) printf("\nKey pressed - premature termination\n");
		while(kbhit()) getch();

		}	/* End of Secure : Inbound loop */

		if(start_tick)	/* Last possible increment of import_ticks */
			import_ticks+=clock()-start_tick;

		for(j=MAX_OPEN_SMBS-1;(int)j>=0;j--)		/* Close open bases */
			if(smb[j].shd_fp)
				smb_close(&smb[j]);

		pkt_to_pkt(fmsgbuf,fakearea,pkt_faddr,hdr,msg_seen,msg_path,1);

		/******* END OF IMPORT PKT ROUTINE *******/

		if(cfg.log&LOG_AREA_TOTALS) {
			for(i=0;i<cfg.areas;i++) {
				if(cfg.area[i].imported)
					logprintf("Imported: %5u msgs %8s <- %s"
						,cfg.area[i].imported,scfg.sub[cfg.area[i].sub]->code
						,cfg.area[i].name); }
			for(i=0;i<cfg.areas;i++) {
				if(cfg.area[i].circular)
					logprintf("Circular: %5u detected in %s"
						,cfg.area[i].circular,cfg.area[i].name); }
			for(i=0;i<cfg.areas;i++) {
				if(cfg.area[i].dupes)
					logprintf("Duplicate: %5u detected in %s"
						,cfg.area[i].dupes,cfg.area[i].name); } }

		import_time=((float)import_ticks)/(float)CLK_TCK;
		if(cfg.log&LOG_TOTALS && import_time && echomail) {
			printf("\nImported %lu EchoMail messages in %.1f seconds "
				,echomail,import_time);
			logprintf("Imported: %5lu msgs in %.1f sec (%.1f/min %.1f/sec)"
				,echomail,import_time
				,import_time/60.0 ? (float)echomail/(import_time/60.0) :(float)echomail
				,(float)echomail/import_time);
			if(import_time/60.0)
				printf("(%.1f/min) ",(float)echomail/(import_time/60.0));
			printf("(%.1f/sec)\n",(float)echomail/import_time); }
		if(fmsgbuf) {
			FREE(fmsgbuf);
			fmsgbuf=0; }

		}

		if(misc&IMPORT_NETMAIL) {

		printf("\nScanning for Inbound NetMail Messages...\n");

#ifdef __unix__
		sprintf(str,"%s*.[Mm][Ss][Gg]",scfg.netmail_dir);
#else
		sprintf(str,"%s*.msg",scfg.netmail_dir);
#endif
		glob(str,0,NULL,&g);
		for(f=0;f<g.gl_pathc && !kbhit();f++) {

			strcpy(path,g.gl_pathv[f]);

			if((fidomsg=fnopen(&fmsg,path,O_RDWR))==NULL) {
				printf("\7ERROR line %d opening %s\n",__LINE__,path);
				logprintf("ERROR line %d opening %s %s",__LINE__,path
					,strerror(errno));
				continue; }
			if(filelength(fmsg)<sizeof(fmsghdr_t)) {
				printf("\7ERROR invalid length of %lu bytes for %s\n",filelength(fmsg)
					,path);
				logprintf("ERROR line %d invalid length of %lu bytes for %s",__LINE__
					,filelength(fmsg),path);
				fclose(fidomsg);
				continue; }
			if(fread(&hdr,sizeof(fmsghdr_t),1,fidomsg)!=1) {
				fclose(fidomsg);
				printf("\7ERROR reading %u bytes from %s"
					,sizeof(fmsghdr_t),path);
				logprintf("ERROR line %d reading %u bytes from %s",__LINE__
					,sizeof(fmsghdr_t),path);
				continue; }
			i=import_netmail(path,hdr,fidomsg);
			/**************************************/
			/* Delete source netmail if specified */
			/**************************************/
			if(i==0) {
				if(misc&DELETE_NETMAIL) {
					fclose(fidomsg);
					if(delfile(path))
						logprintf("ERROR line %d removing %s %s",__LINE__,path
							,strerror(errno)); }
				else {
					hdr.attr|=FIDO_RECV;
					fseek(fidomsg,0L,SEEK_SET);
					fwrite(&hdr,sizeof(fmsghdr_t),1,fidomsg);
					fclose(fidomsg); } }
			else if(i!=-2)
				fclose(fidomsg);
			printf("\n"); 
			}
		globfree(&g);
	}


	if(misc&EXPORT_ECHOMAIL) {
		memset(&addr,0,sizeof(faddr_t));
		export_echomail(sub_code,addr);
	}


	if(misc&PACK_NETMAIL) {

		memset(&msg_seen,0,sizeof(addrlist_t));
		memset(&msg_path,0,sizeof(addrlist_t));
		memset(&fakearea,0,sizeof(areasbbs_t));

		printf("\nPacking Outbound NetMail...\n");

#ifdef __unix__
		sprintf(str,"%s*.[Mm][Ss][Gg]",scfg.netmail_dir);
#else
		sprintf(str,"%s*.msg",scfg.netmail_dir);
#endif
		glob(str,0,NULL,&g);
		for(f=0;f<g.gl_pathc && !kbhit();f++) {

			strcpy(path,g.gl_pathv[f]);

			if((fidomsg=fnopen(&fmsg,path,O_RDWR))==NULL) {
				printf("\7ERROR line %d opening %s\n",__LINE__,path);
				logprintf("ERROR line %d opening %s %s",__LINE__,path
					,strerror(errno));
				continue; }
			if(filelength(fmsg)<sizeof(fmsghdr_t)) {
				printf("\7%s Invalid length of %lu bytes\n",path,filelength(fmsg));
				fclose(fidomsg);
				continue; }
			if(fread(&hdr,sizeof(fmsghdr_t),1,fidomsg)!=1) {
				fclose(fidomsg);
				printf("\7ERROR reading %u bytes from %s"
					,sizeof(fmsghdr_t),path);
				logprintf("ERROR line %d reading %u bytes from %s",__LINE__
					,sizeof(fmsghdr_t),path);
				continue; }
			hdr.destzone=hdr.origzone=sys_faddr.zone;
			hdr.destpoint=hdr.origpoint=0;
			getzpt(fidomsg,&hdr);				/* use kludge if found */
			addr.zone=hdr.destzone;
			addr.net=hdr.destnet;
			addr.node=hdr.destnode;
			addr.point=hdr.destpoint;
			for(i=0;i<scfg.total_faddrs;i++)
				if(!memcmp(&addr,&scfg.faddr[i],sizeof(faddr_t)))
					break;
			if(i<scfg.total_faddrs)	{				  /* In-bound, so ignore */
				fclose(fidomsg);
				continue;
			}
			printf("\n%s to %s ",path,faddrtoa(&addr,NULL));
			if(cfg.log&LOG_PACKING)
				logprintf("Packing %s (%s)",path,faddrtoa(&addr,NULL));
			fmsgbuf=getfmsg(fidomsg,NULL);
			if(!fmsgbuf) {
				printf("ERROR allocating memory for NetMail fmsgbuf\n");
				logprintf("ERROR line %d allocating memory for NetMail fmsgbuf"
					,__LINE__);
				bail(1); }
			fclose(fidomsg);

			if(misc&FLO_MAILER) {
				attr=0;
				i=matchnode(addr,0);
				if(i<cfg.nodecfgs)
					if(cfg.nodecfg[i].route.zone
						&& !(hdr.attr&(FIDO_CRASH|FIDO_HOLD))) {
						addr=cfg.nodecfg[i].route;		/* Routed */
						if(cfg.log&LOG_ROUTING)
							logprintf("Routing %s to %s",path,faddrtoa(&addr,NULL));
						i=matchnode(addr,0); }
				if(i<cfg.nodecfgs)
					attr=cfg.nodecfg[i].attr;
				if(hdr.attr&FIDO_CRASH)
					attr|=ATTR_CRASH;
				else if(hdr.attr&FIDO_HOLD)
					attr|=ATTR_HOLD;
				if(attr&ATTR_CRASH) ch='c';
				else if(attr&ATTR_HOLD) ch='h';
				else if(attr&ATTR_DIRECT) ch='d';
				else ch='o';
				if(addr.zone==scfg.faddr[0].zone) /* Default zone, use default outbound */
					strcpy(outbound,cfg.outbound);
				else {						 /* Inter-zone outbound is OUTBOUND.XXX */
					sprintf(outbound,"%.*s.%03x"
						,(int)strlen(cfg.outbound)-1,cfg.outbound,addr.zone);
					MKDIR(outbound);
					backslash(outbound);
				}
				if(addr.point) {			/* Point destination is OUTBOUND.PNT */
					sprintf(str,"%04x%04x.pnt"
						,addr.net,addr.node);
					strcat(outbound,str); }
				if(outbound[strlen(outbound)-1]=='\\'
					|| outbound[strlen(outbound)-1]=='/')
					outbound[strlen(outbound)-1]=0;
				MKDIR(outbound);
				backslash(outbound);
				if(addr.point)
					sprintf(packet,"%s%08x.%cut",outbound,addr.point,ch);
				else
					sprintf(packet,"%s%04x%04x.%cut",outbound,addr.net,addr.node,ch);
				if(hdr.attr&FIDO_FILE)
					if(write_flofile(hdr.subj,addr))
						bail(1); }
			else
				strcpy(packet,pktname());

			now=time(NULL);
			tm=localtime(&now);
			if((stream=fnopen(&file,packet,O_WRONLY|O_APPEND|O_CREAT))==NULL) {
				printf("Unable to open %s for write.\n"
					,packet);
				logprintf("ERROR line %d opening %s %s",__LINE__,packet
					,strerror(errno));
				bail(1); }

			if(!filelength(file)) {
				pkthdr.orignode=hdr.orignode;
				pkthdr.destnode=hdr.destnode;
				pkthdr.year=tm->tm_year+1900;
				pkthdr.month=tm->tm_mon;
				pkthdr.day=tm->tm_mday;
				pkthdr.hour=tm->tm_hour;
				pkthdr.min=tm->tm_min;
				pkthdr.sec=tm->tm_sec;
				pkthdr.baud=0x00;
				pkthdr.pkttype=0x0002;
				pkthdr.orignet=hdr.orignet;
				pkthdr.destnet=hdr.destnet;
				pkthdr.prodcode=0x00;
				pkthdr.sernum=0;
				i=matchnode(addr,0);
				if(i<cfg.nodecfgs)
					memcpy(pkthdr.password,cfg.nodecfg[i].pktpwd,8);
				else
					memset(pkthdr.password,0,8);
				pkthdr.origzone=hdr.origzone;
				pkthdr.destzone=hdr.destzone;
				memset(pkthdr.empty,0,20);
				fwrite(&pkthdr,sizeof(pkthdr_t),1,stream); }

			putfmsg(stream,fmsgbuf,hdr,fakearea,msg_seen,msg_path);

			FREE(fmsgbuf);
			fclose(stream);
			/**************************************/
			/* Delete source netmail if specified */
			/**************************************/
			if(misc&DELETE_NETMAIL)
				if(delfile(path))
					logprintf("ERROR line %d removing %s %s",__LINE__,path
						,strerror(errno));
			printf("\n"); 
		}
		globfree(&g);
	}

	if(misc&UPDATE_MSGPTRS) {

		printf("\nUpdating Message Pointers to Last Posted Message...\n");

		for(grp=0;grp<scfg.total_grps;grp++) {
			for(i=0;i<scfg.total_subs;i++) {
				if(scfg.sub[i]->misc&SUB_FIDO && scfg.sub[i]->grp==grp) {
					printf("\n%-15.15s %s\n"
						,scfg.grp[scfg.sub[i]->grp]->sname,scfg.sub[i]->lname);
					getlastmsg(i,&l,0);
					sprintf(str,"%s%s.sfp",scfg.sub[i]->data_dir,scfg.sub[i]->code);
					if((file=nopen(str,O_WRONLY|O_CREAT))==-1) {
						printf("\7ERROR %d line %d opening/creating %s"
							,errno,__LINE__,str);
						logprintf("ERROR %d line %d opening/creating %s"
							,errno,__LINE__,str); }
					else {
						write(file,&l,sizeof(time_t));
						close(file); 
					} 
				}
			}
		}
	}

	if(misc&(IMPORT_NETMAIL|IMPORT_ECHOMAIL) && misc&REPORT) {
		now=time(NULL);
		sprintf(str,"%ssbbsecho.msg",scfg.text_dir);
		if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
			printf("ERROR line %d opening %s\n",__LINE__,str);
			logprintf("ERROR line %d opening %s",__LINE__,str);
			bail(1); }
		sprintf(fname,"\1c\1h               "
			"ÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜÜ\r\n");
		sprintf(path,"\1c\1h               "
			"ßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßßß\r\n");
		write(file,fname,strlen(fname));
		sprintf(str,"               \1n\1k\0016"
			" Last FidoNet Transfer on %.24s \1n\r\n",ctime(&now));
		write(file,str,strlen(str));
		write(file,path,strlen(path));
		write(file,fname,strlen(fname));
		sprintf(tmp,"Imported %lu EchoMail and %lu NetMail Messages"
			,echomail,netmail);
		sprintf(str,"               \1n\1k\0016 %-50.50s\1n\r\n",tmp);
		write(file,str,strlen(str));
		write(file,path,strlen(path));
		close(file); 
	}

	pkt_to_pkt(NULL,fakearea,pkt_faddr,hdr,msg_seen,msg_path,1);
	if(email->shd_fp)
		smb_close(email);

	FREE(smb);
	FREE(email);

	bail(0);
	return(0);
}
