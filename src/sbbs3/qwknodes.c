/* QWKNODES.C */

/* Synchronet QWKnet node list or route.dat file generator */

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
#include "crc32.h"

unsigned _stklen=10000;
smb_t		smb;
scfg_t		cfg;

/****************************************************************************/
/* Updates 16-bit "rcrc" with character 'ch'                                */
/****************************************************************************/
void ucrc16(uchar ch, ushort *rcrc) {
	ushort i, cy;
    uchar nch=ch;
 
for (i=0; i<8; i++) {
    cy=*rcrc & 0x8000;
    *rcrc<<=1;
    if (nch & 0x80) *rcrc |= 1;
    nch<<=1;
    if (cy) *rcrc ^= 0x1021; }
}

/****************************************************************************/
/* Returns 16-crc of string (not counting terminating NULL) 				*/
/****************************************************************************/
ushort crc16(char *str)
{
	int 	i=0;
	ushort	crc=0;

ucrc16(0,&crc);
while(str[i])
	ucrc16(str[i++],&crc);
ucrc16(0,&crc);
ucrc16(0,&crc);
return(crc);
}

/****************************************************************************/
/* Returns 32-crc of string (not counting terminating NULL) 				*/
/****************************************************************************/
ulong _crc32(char *str)
{
	int i=0;
	ulong crc=0xffffffffUL;

	while(str[i])
		crc=ucrc32(str[i++],crc);
	crc=~crc;
	return(crc);
}

/****************************************************************************/
/* Converts unix time format (long - time_t) into a char str MM/DD/YY		*/
/****************************************************************************/
char * unixtodstr(time_t unix, char *str)
{
	struct tm* tm;

if(!unix)
	strcpy(str,"00/00/00");
else {
	tm=localtime(&unix);
	if(tm==NULL) {
		strcpy(str,"00/00/00");
		return(str);
	}
	if(tm->tm_mon>11) {	  /* DOS leap year bug */
		tm->tm_mon=0;
		tm->tm_year++; }
	if(tm->tm_mday>31)
		tm->tm_mday=1;
	if(cfg.sys_misc&SM_EURODATE)
		sprintf(str,"%02u/%02u/%02u",tm->tm_mday,tm->tm_mon+1
			,TM_YEAR(tm->tm_year));
	else
		sprintf(str,"%02u/%02u/%02u",tm->tm_mon+1,tm->tm_mday
			,TM_YEAR(tm->tm_year)); }
return(str);
}

/****************************************************************************/
/* Puts a backslash on path strings 										*/
/****************************************************************************/
void backslash(char *str)
{
    int i;

i=strlen(str);
if(i && str[i-1]!='\\') {
    str[i]='\\'; str[i+1]=0; }
}
/****************************************************************************/
/* Network open function. Opens all files DENYALL and retries LOOP_NOPEN    */
/* number of times if the attempted file is already open or denying access	*/
/* for some other reason.	All files are opened in BINARY mode.			*/
/****************************************************************************/
int nopen(char *str, int access)
{
	int file,share,count=0;

if(access==O_RDONLY) share=SH_DENYWR;
	else share=SH_DENYRW;
while(((file=sopen(str,O_BINARY|access,share,S_IWRITE))==-1)
	&& (errno==EACCES errno==EAGAIN) && count++<LOOP_NOPEN);
if(file==-1 && (errno==EACCES || errno==EAGAIN))
	lputs("\7\r\nNOPEN: ACCESS DENIED\r\n\7");
return(file);
}
/****************************************************************************/
/* This function performs an nopen, but returns a file stream with a buffer */
/* allocated.																*/
/****************************************************************************/
FILE *fnopen(int *file, char *str, int access)
{
	char mode[128];
	FILE *stream;

if(((*file)=nopen(str,access))==-1)
	return(NULL);

if(access&O_APPEND) {
	if(access&O_RDONLY)
		strcpy(mode,"a+");
	else
		strcpy(mode,"a"); }
else {
	if(access&O_WRONLY)
		strcpy(mode,"r+");
	else
		strcpy(mode,"r"); }
stream=fdopen((*file),mode);
if(stream==NULL) {
	close(*file);
	return(NULL); }
setvbuf(stream,NULL,_IOFBF,16*1024);
return(stream);
}

/****************************************************************************/
/* Truncates white-space chars off end of 'str' and terminates at first tab */
/****************************************************************************/
void truncsp(char *str)
{
	uchar c;

c=strlen(str);
while(c && (uchar)str[c-1]<=SP) c--;
str[c]=0;
}

void stripctrla(uchar *str)
{
	uchar out[256];
	int i,j;

for(i=j=0;str[i];i++) {
	if(str[i]==CTRL_A && str[i+1]!=0)
		i++;
	else
		out[j++]=str[i]; }
out[j]=0;
strcpy(str,out);
}


int lputs(char* str)
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
return(fputs(tmp,stderr));
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
void bail(int code)
{
	exit(code);
}


char *loadmsgtail(smbmsg_t msg)
{
	char	*buf=NULL;
	ushort	xlat;
	int 	i;
	long	l=0,length;

for(i=0;i<msg.hdr.total_dfields;i++) {
	if(msg.dfield[i].type!=TEXT_TAIL)
		continue;
	fseek(smb.sdt_fp,msg.hdr.offset+msg.dfield[i].offset
		,SEEK_SET);
	fread(&xlat,2,1,smb.sdt_fp);
	if(xlat!=XLAT_NONE) 		/* no translations supported */
		continue;
	length=msg.dfield[i].length-2;
	if((buf=REALLOC(buf,l+msg.dfield[i].length+1))==NULL)
		return(buf);
	l+=fread(buf+l,1,length,smb.sdt_fp);
	buf[l]=0; }
return(buf);
}


void gettag(smbmsg_t msg, char *tag)
{
	char *buf,*p;

tag[0]=0;
buf=loadmsgtail(msg);
if(buf==NULL)
	return;
truncsp(buf);
stripctrla(buf);
p=strrchr(buf,LF);
if(!p) p=buf;
else p++;
if(!strnicmp(p," þ Synchronet þ ",16))
	p+=16;
if(!strnicmp(p," * Synchronet * ",16))
    p+=16;
while(*p && *p<=SP) p++;
strcpy(tag,p);
FREE(buf);
}


#define FEED   (1<<0)
#define LOCAL  (1<<1)
#define APPEND (1<<2)
#define TAGS   (1<<3)

#define ROUTE  (1<<1)
#define NODES  (1<<2)
#define USERS  (1<<3)

char *usage="\nusage: qwknodes [/opts] cmds"
			"\n"
			"\n cmds: r  =  create route.dat"
			"\n       u  =  create users.dat"
			"\n       n  =  create nodes.dat
			"\n"
			"\n opts: f  =  format addresses for nodes that feed from this system"
			"\n       a  =  append existing output files"
			"\n       t  =  include tag lines in nodes.dat
			"\n       l  =  include local users in users.dat
			"\n       m# =  maximum message age set to # days"
			"\n";

void main(int argc, char **argv)
{
	char			str[256],tmp[128],tag[256],addr[256],*p;
	int 			i,j,mode=0,cmd=0,o_mode,max_age=0;
	ushort			smm,sbl;
	ulong			*crc=NULL,curcrc,total_crcs=0,l;
	FILE			*route,*users,*nodes;
	time_t			now;
	read_cfg_text_t txt;
	smbmsg_t		msg;

txt.openerr="\7\r\nError opening %s for read.\r\n";
txt.reading="\r\nReading %s...";
txt.readit="\rRead %s       ";
txt.allocerr="\7\r\nError allocating %u bytes of memory\r\n";
txt.error="\7\r\nERROR: Offset %lu in %s\r\n\r\n";

fprintf(stderr,"\nSynchronet QWKnet Node/Route/User List  v1.20  "
	"Copyright 2000 Rob Swindell\n");


for(i=1;i<argc;i++)
	for(j=0;argv[i][j];j++)
		switch(toupper(argv[i][j])) {
			case '/':
			case '-':
				while(argv[i][++j])
					switch(toupper(argv[i][j])) {
						case 'F':
							mode|=FEED;
							break;
						case 'L':
							mode|=LOCAL;
							break;
						case 'A':
							mode|=APPEND;
							break;
						case 'T':
							mode|=TAGS;
							break;
						case 'M':
							j++;
							max_age=atoi(argv[i]+j);
							while(isdigit(argv[i][j+1])) j++;
							break;
						default:
							printf(usage);
							exit(1); }
				j--;
				break;
			case 'R':
				cmd|=ROUTE;
				break;
			case 'U':
				cmd|=USERS;
				break;
			case 'N':
				cmd|=NODES;
				break;
			default:
				printf(usage);
				exit(1); }

if(!cmd) {
	printf(usage);
	exit(1); }

if(mode&APPEND)
	o_mode=O_WRONLY|O_CREAT|O_APPEND;
else
	o_mode=O_WRONLY|O_CREAT|O_TRUNC;

if(cmd&NODES)
	if((nodes=fnopen(&i,"nodes.dat",o_mode))==NULL) {
		printf("\7\nError opening nodes.dat\n");
		exit(1); }

if(cmd&USERS)
	if((users=fnopen(&i,"users.dat",o_mode))==NULL) {
		printf("\7\nError opening users.dat\n");
        exit(1); }

if(cmd&ROUTE)
	if((route=fnopen(&i,"route.dat",o_mode))==NULL) {
		printf("\7\nError opening route.dat\n");
        exit(1); }

if(!node_dir[0]) {
	p=getenv("SBBSNODE");
	if(p==NULL) {
		printf("\7\nSBBSNODE environment variable not set.\n");
		exit(1); }
	strcpy(node_dir,p); }

if(node_dir[strlen(node_dir)-1]!='\\')
	strcat(node_dir,"\\");

read_node_cfg(&cfg,txt);
if(ctrl_dir[0]=='.') {   /* Relative path */
	strcpy(str,ctrl_dir);
	sprintf(ctrl_dir,"%s%s",node_dir,str);
	if(FULLPATH(str,ctrl_dir,40))
		strcpy(ctrl_dir,str); }
backslash(ctrl_dir);

read_main_cfg(&cfg,txt);
if(data_dir[0]=='.') {   /* Relative path */
	strcpy(str,data_dir);
	sprintf(data_dir,"%s%s",node_dir,str);
	if(FULLPATH(str,data_dir,40))
		strcpy(data_dir,str); }
backslash(data_dir);
read_msgs_cfg(txt);

now=time(NULL);
smm=crc16("smm");
sbl=crc16("sbl");
fprintf(stderr,"\n\n");
for(i=0;i<total_subs;i++) {
	if(!(sub[i]->misc&SUB_QNET))
		continue;
	fprintf(stderr,"%-*s  %s\n"
		,LEN_GSNAME,grp[sub[i]->grp]->sname,sub[i]->lname);
	sprintf(smb.file,"%s%s",sub[i]->data_dir,sub[i]->code);
	smb.retry_time=30;
	smb.sunum=i;
	if((j=smb_open(&smb))!=0) {
		printf("smb_open returned %d\n",j);
		continue; }
	if((j=smb_locksmbhdr(&smb))!=0) {
		printf("smb_locksmbhdr returned %d\n",j);
		smb_close(&smb);
		continue; }
	if((j=smb_getstatus(&smb))!=0) {
		printf("smb_getstatus returned %d\n",j);
		smb_close(&smb);
		continue; }
	smb_unlocksmbhdr(&smb);
	msg.offset=smb.status.total_msgs;
	if(!msg.offset) {
		smb_close(&smb);
		printf("Empty.\n");
		continue; }
	while(!kbhit() && !ferror(smb.sid_fp) && msg.offset) {
		msg.offset--;
		fseek(smb.sid_fp,msg.offset*sizeof(idxrec_t),SEEK_SET);
		if(!fread(&msg.idx,1,sizeof(idxrec_t),smb.sid_fp))
			break;
		fprintf(stderr,"%-5lu\r",msg.offset+1);
		if(msg.idx.to==smm || msg.idx.to==sbl)
			continue;
		if(max_age && now-msg.idx.time>((ulong)max_age*24UL*60UL*60UL))
			continue;
		if((j=smb_lockmsghdr(&smb,&msg))!=0) {
			printf("smb_lockmsghdr returned %d\n",j);
			break; }
		if((j=smb_getmsghdr(&smb,&msg))!=0) {
			printf("smb_getmsghdr returned %d\n",j);
            break; }
		smb_unlockmsghdr(&smb,&msg);
		if((mode&LOCAL && msg.from_net.type==NET_NONE)
			|| msg.from_net.type==NET_QWK) {
			if(msg.from_net.type!=NET_QWK)
				msg.from_net.addr="";
			if(cmd&USERS) {
				sprintf(str,"%s%s",msg.from_net.addr,msg.from);
				curcrc=crc32(str); }
			else
				curcrc=crc32(msg.from_net.addr);
			for(l=0;l<total_crcs;l++)
				if(curcrc==crc[l])
					break;
			if(l==total_crcs) {
				total_crcs++;
				if((crc=(ulong *)REALLOC(crc
					,sizeof(ulong)*total_crcs))==NULL) {
					printf("Error allocating %lu bytes\n"
						,sizeof(ulong)*total_crcs);
					break; }
				crc[l]=curcrc;
				if(cmd&ROUTE && msg.from_net.type==NET_QWK) {
					strcpy(addr,msg.from_net.addr);
					if(mode&FEED) {
						p=strrchr(addr,'/');
						if(!p)
							p=addr;
						else
							*(p++)=0;
						sprintf(str,"%s %s:%s%c%s"
							,unixtodstr(msg.hdr.when_written.time,tmp)
							,p,sys_id,p==addr ? 0 : '/'
							,addr);
						fprintf(route,"%s\r\n",str); }
					else {
						p=strrchr(addr,'/');
						if(p) {
							*(p++)=0;
						fprintf(route,"%s %s:%.*s\r\n"
							,unixtodstr(msg.hdr.when_written.time,str)
							,p
							,(uint)(p-addr)
							,addr); } } }
				if(cmd&USERS) {
					if(msg.from_net.type!=NET_QWK)
						strcpy(str,sys_id);
					else if(mode&FEED)
						sprintf(str,"%s/%s",sys_id,msg.from_net.addr);
					else
						strcpy(str,msg.from_net.addr);
					p=strrchr(str,'/');
                    if(p)
						fprintf(users,"%-25.25s  %-8.8s  %s  (%s)\r\n"
							,msg.from,p+1
							,unixtodstr(msg.hdr.when_written.time,tmp)
							,str);
					else
						fprintf(users,"%-25.25s  %-8.8s  %s\r\n"
							,msg.from,str
							,unixtodstr(msg.hdr.when_written.time,tmp)); }
				if(cmd&NODES && msg.from_net.type==NET_QWK) {
					if(mode&TAGS)
						gettag(msg,tag);
					if(mode&FEED)
						sprintf(str,"%s/%s",sys_id,msg.from_net.addr);
					else
						strcpy(str,msg.from_net.addr);
					p=strrchr(str,'/');
					if(p) {
						if(mode&TAGS)
							fprintf(nodes,"%-8.8s  %s\r\n"
								,p+1
								,tag);
						else
							fprintf(nodes,"%-8.8s  %s  (%s)\r\n"
								,p+1
								,unixtodstr(msg.hdr.when_written.time,tmp)
								,str); }
					else
						fprintf(nodes,"%-8.8s  %s\r\n"
							,str
							,mode&TAGS
							? tag
							: unixtodstr(msg.hdr.when_written.time,tmp)); }
				} }
		smb_freemsgmem(&msg); }

	smb_close(&smb);
	if(kbhit()) {
		getch();
		fprintf(stderr,"Key pressed.\n");
		break; } }
fprintf(stderr,"Done.\n");
}

