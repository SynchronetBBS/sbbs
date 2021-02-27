/* QWKNODES.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* Generates QWKnet node list or ROUTE.DAT file from Synchronet message base */

#include "sbbs.h"
#include "crc32.h"
#include "crc16.c"

unsigned _stklen=10000;
smb_t	 smb;

/****************************************************************************/
/* Returns 32-crc of string (not counting terminating NULL) 				*/
/****************************************************************************/
ulong crc32(char *str)
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
char *unixtodstr(time_t unix, char *str)
{
	struct date date;
	struct time curtime;

if(!unix)
	strcpy(str,"00/00/00");
else {
	unixtodos(unix,&date,&curtime);
	if((unsigned)date.da_mon>12) {	  /* DOS leap year bug */
		date.da_mon=1;
		date.da_year++; }
	if((unsigned)date.da_day>31)
		date.da_day=1;
	if(sys_misc&SM_EURODATE)
		sprintf(str,"%02u/%02u/%02u",date.da_day,date.da_mon
			,date.da_year>=2000 ? date.da_year-2000 : date.da_year-1900);
	else
		sprintf(str,"%02u/%02u/%02u",date.da_mon,date.da_day
			,date.da_year>=2000 ? date.da_year-2000 : date.da_year-1900); }
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
	char logstr[256];
	int file,share,count=0;

if(access==O_RDONLY) share=SH_DENYWR;
	else share=SH_DENYRW;
while(((file=sopen(str,O_BINARY|access,share,S_IWRITE))==-1)
	&& errno==EACCES && count++<LOOP_NOPEN);
if(file==-1 && errno==EACCES)
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
	if(str[i]==1)
		i++;
	else
		out[j++]=str[i]; }
out[j]=0;
strcpy(str,out);
}


long lputs(char FAR16 *str)
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
chcount=vsprintf(sbuf,fmat,argptr);
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
			"\n cmds: r  =  create ROUTE.DAT"
			"\n       u  =  create USERS.DAT"
			"\n       n  =  create NODES.DAT"
			"\n"
			"\n opts: f  =  format addresses for nodes that feed from this system"
			"\n       a  =  append existing output files"
			"\n       t  =  include tag lines in NODES.DAT"
			"\n       l  =  include local users in USERS.DAT"
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
	smbstatus_t 	status;
	smbmsg_t		msg;

txt.openerr="\7\r\nError opening %s for read.\r\n";
txt.reading="\r\nReading %s...";
txt.readit="\rRead %s       ";
txt.allocerr="\7\r\nError allocating %u bytes of memory\r\n";
txt.error="\7\r\nERROR: Offset %lu in %s\r\n\r\n";

fprintf(stderr,"\nSynchronet QWKnet Node/Route/User List  v1.20  "
	"Developed 1995-1997 Rob Swindell\n");


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
	if((nodes=fnopen(&i,"NODES.DAT",o_mode))==NULL) {
		printf("\7\nError opening NODES.DAT\n");
		exit(1); }

if(cmd&USERS)
	if((users=fnopen(&i,"USERS.DAT",o_mode))==NULL) {
		printf("\7\nError opening USERS.DAT\n");
        exit(1); }

if(cmd&ROUTE)
	if((route=fnopen(&i,"ROUTE.DAT",o_mode))==NULL) {
		printf("\7\nError opening ROUTE.DAT\n");
        exit(1); }

if(!node_dir[0]) {
	p=getenv("SBBSNODE");
	if(p==NULL) {
		printf("\7\nSBBSNODE environment variable not set.\n");
		exit(1); }
	strcpy(node_dir,p); }

strupr(node_dir);

if(node_dir[strlen(node_dir)-1]!='\\')
	strcat(node_dir,"\\");

read_node_cfg(txt);
if(ctrl_dir[0]=='.') {   /* Relative path */
	strcpy(str,ctrl_dir);
	sprintf(ctrl_dir,"%s%s",node_dir,str);
	if(_fullpath(str,ctrl_dir,40))
		strcpy(ctrl_dir,str); }
backslash(ctrl_dir);

read_main_cfg(txt);
if(data_dir[0]=='.') {   /* Relative path */
	strcpy(str,data_dir);
	sprintf(data_dir,"%s%s",node_dir,str);
	if(_fullpath(str,data_dir,40))
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
	msg.offset=status.total_msgs;
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

