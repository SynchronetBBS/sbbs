/* SBBSFIDO.C */

/* Synchronet FidoNet EchoMail Scanning/Tossing and NetMail Tossing Utility */

#define VER "2.24"

#include "sbbs.h"
#include "crc32.h"
#include "lzh.h"
#include "post.h"
#include "scfglib.h"

#define IMPORT_NETMAIL	(1L<<0)
#define IMPORT_ECHOMAIL (1L<<1)
#define EXPORT_ECHOMAIL (1L<<2)
#define DELETE_NETMAIL	(1L<<3)
#define DELETE_ECHOMAIL (1L<<4)
#define IGNORE_POINT	(1L<<5)
#define IGNORE_ZONE 	(1L<<6)
#define IGNORE_MSGPTRS	(1L<<7)
#define UPDATE_MSGPTRS	(1L<<8)
#define LEAVE_MSGPTRS	(1L<<9)
#define KILL_ECHOMAIL	(1L<<10)
#define ASCII_ONLY		(1L<<11)
#define LOGFILE 		(1L<<12)
#define REPORT			(1L<<13)
#define EXPORT_ALL		(1L<<14)
#define PURGE_ECHOMAIL	(1L<<15)
#define UNKNOWN_NETMAIL (1L<<16)
#define IGNORE_ADDRESS	(1L<<17)
#define IMPORT_LOCAL	(1L<<18)
#define IMPORT_NEW_ONLY (1L<<19)
#define DONT_SET_RECV	(1L<<20)
#define IGNORE_RECV 	(1L<<21)
#define CONVERT_TEAR	(1L<<22)
#define IMPORT_PRIVATE	(1L<<23)
#define LOCAL_NETMAIL	(1L<<24)
#define NOTIFY_RECEIPT	(1L<<25)

char *wday[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
char *mon[]={"Jan","Feb","Mar","Apr","May","Jun"
            ,"Jul","Aug","Sep","Oct","Nov","Dec"};

long misc=(IMPORT_NETMAIL|IMPORT_ECHOMAIL|EXPORT_ECHOMAIL
			|DELETE_NETMAIL|DELETE_ECHOMAIL|KILL_ECHOMAIL);
char tmp[256];

FILE *fidologfile=NULL;
#ifdef __TURBOC__
	unsigned _stklen=20000;
#endif
int startmsg=1;
int nodefile;

#ifdef __WATCOMC__
	#define O_DENYNONE SH_DENYNO
#endif

long lputs(char FAR16 *str)
{
    char tmp[256];
    int i,j,k;

if(misc&LOGFILE && fidologfile!=NULL)
    fputs(str,fidologfile);
j=strlen(str);
for(i=k=0;i<j;i++)      /* remove CRs */
    if(str[i]==CR && str[i+1]==LF)
        continue;
    else
        tmp[k++]=str[i];
tmp[k]=0;
return(fputs(tmp,stdout));
}

/******************************************/
/* CRC-16 routines required for SMB index */
/******************************************/

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

/****************************************************************************/
/* Reads the data for node number 'number' into the structure 'node'        */
/* from NODE.DAB															*/
/* if lockit is non-zero, locks this node's record. putnodedat() unlocks it */
/****************************************************************************/
void getnodedat(uint number, node_t *node, char lockit)
{
	char str[256];
	int count=0;

number--;	/* make zero based */
while(count<LOOP_NODEDAB) {
	lseek(nodefile,(long)number*sizeof(node_t),SEEK_SET);
	if(lockit
		&& lock(nodefile,(long)number*sizeof(node_t),sizeof(node_t))==-1) {
		count++;
		continue; }
	if(read(nodefile,node,sizeof(node_t))==sizeof(node_t))
		break;
	count++; }
if(count==LOOP_NODEDAB)
	lprintf("\7Error unlocking and reading NODE.DAB\r\n");
}

/****************************************************************************/
/* Write the data from the structure 'node' into NODE.DAB  					*/
/* getnodedat(num,&node,1); must have been called before calling this func  */
/*          NOTE: ------^   the indicates the node record has been locked   */
/****************************************************************************/
void putnodedat(uint number, node_t node)
{
	char str[256];
	int count;

number--;	/* make zero based */
lseek(nodefile,(long)number*sizeof(node_t),SEEK_SET);
if(write(nodefile,&node,sizeof(node_t))!=sizeof(node_t)) {
	unlock(nodefile,(long)number*sizeof(node_t),sizeof(node_t));
	lprintf("\7Error writing NODE.DAB for node %u\r\n",number+1);
	return; }
unlock(nodefile,(long)number*sizeof(node_t),sizeof(node_t));
}


/****************************************************************************/
/* Creates a short message for 'usernumber' than contains 'strin'			*/
/****************************************************************************/
void putsmsg(int usernumber, char *strin)
{
	char str[256];
	int file,i;
    node_t node;

sprintf(str,"%sMSGS\\%4.4u.MSG",data_dir,usernumber);
if((file=nopen(str,O_WRONLY|O_CREAT|O_APPEND))==-1) {
	lprintf("\7Error opening/creating %s for creat/append access\r\n",str);
	return; }
i=strlen(strin);
if(write(file,strin,i)!=i) {
	close(file);
	lprintf("\7Error writing %u bytes to %s\r\n",i,str);
	return; }
close(file);
for(i=1;i<=sys_nodes;i++) {		/* flag node if user on that msg waiting */
	getnodedat(i,&node,0);
	if(node.useron==usernumber
		&& (node.status==NODE_INUSE || node.status==NODE_QUIET)
		&& !(node.misc&NODE_MSGW)) {
		getnodedat(i,&node,1);
		node.misc|=NODE_MSGW;
        putnodedat(i,node); } }
}


/****************************************************************************/
/* Converts an ASCII Hex string into an ulong                       */
/****************************************************************************/
ulong ahtoul(char *str)
{
	ulong l,val=0;

while((l=(*str++)|0x20)!=0x20)
	val=(l&0xf)+(l>>6&1)*9+val*16;
return(val);
}

/****************************************************************************/
/* Truncates white-space chars off end of 'str' and terminates at first tab */
/****************************************************************************/
void truncsp(char *str)
{
	char c;

str[strcspn(str,"\t")]=0;
c=strlen(str);
while(c && str[c-1]<=SP) c--;
str[c]=0;
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

void remove_re(char *str)
{
while(!strnicmp(str,"RE:",3)) {
	strcpy(str,str+3);
	while(str[0]==SP)
		strcpy(str,str+1); }
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

if(access&O_DENYNONE) {
	share=SH_DENYNO;
	access&=~O_DENYNONE; }
else if(access==O_RDONLY) share=SH_DENYWR;
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
/* Moves or copies a file from one dir to another                           */
/* both 'src' and 'dest' must contain full path and filename                */
/* returns 0 if successful, -1 if error                                     */
/****************************************************************************/
int mv(char *src, char *dest, char copy)
{
	char buf[4096],str[256];
	int  ind,outd;
	long length,chunk=4096,l;
	ushort ftime,fdate;
    FILE *inp,*outp;

if(!strcmp(src,dest))	/* source and destination are the same! */
	return(0);
if(!fexist(src)) {
	lprintf("\r\nMV ERROR: Source doesn't exist\r\n'%s'\r\n"
		,src);
	return(-1); }
if(!copy && fexist(dest)) {
	lprintf("\r\nMV ERROR: Destination already exists\r\n'%s'\r\n"
		,dest);
	return(-1); }
if(!copy && ((src[1]!=':' && dest[1]!=':')
	|| (src[1]==':' && dest[1]==':' && toupper(src[0])==toupper(dest[0])))) {
	if(rename(src,dest)) {						/* same drive, so move */
		lprintf("\r\nMV ERROR: Error renaming '%s'"
				"\r\n                      to '%s'\r\n",src,dest);
		return(-1); }
	return(0); }
if((ind=nopen(src,O_RDONLY))==-1) {
	lprintf("\r\nMV ERROR: ERR_OPEN %s\r\n",src);
	return(-1); }
if((inp=fdopen(ind,"rb"))==NULL) {
	close(ind);
	lprintf("\r\nMV ERROR: ERR_FDOPEN %s\r\n",str);
	return(-1); }
setvbuf(inp,NULL,_IOFBF,8*1024);
if((outd=nopen(dest,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
	fclose(inp);
	lprintf("\r\nMV ERROR: ERR_OPEN %s\r\n",dest);
	return(-1); }
if((outp=fdopen(outd,"wb"))==NULL) {
	close(outd);
	fclose(inp);
	lprintf("\r\nMV ERROR: ERR_FDOPEN %s\r\n",str);
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
_dos_getftime(ind,&fdate,&ftime);
_dos_setftime(outd,fdate,ftime);
fclose(inp);
fclose(outp);
if(!copy && remove(src)) {
	lprintf("MV ERROR: ERR_REMOVE %s\r\n",src);
	return(-1); }
return(0);
}

/****************************************************************************/
/* Returns the total number of msgs in the sub-board and sets 'ptr' to the  */
/* date of the last message in the sub (0) if no messages.					*/
/****************************************************************************/
ulong getlastmsg(uint subnum, ulong *ptr, time_t *t)
{
	char str[256];
	int i;
	smbstatus_t status;

sprintf(smb_file,"%s%s",sub[subnum]->data_dir,sub[subnum]->code);
if((i=smb_open(10))!=0) {
	lprintf("ERR_OPEN %s %d\r\n",smb_file,i);
	return(0); }

if(!filelength(fileno(shd_fp))) {			/* Empty base */
	if(ptr) (*ptr)=0;
	smb_close();
	return(0); }
if((i=smb_locksmbhdr(10))!=0) {
	smb_close();
	lprintf("ERR_LOCK %s %d\r\n",smb_file,i);
	return(0); }
if((i=smb_getstatus(&status))!=0) {
	smb_unlocksmbhdr();
	smb_close();
	lprintf("ERR_READ %s %d\r\n",smb_file,i);
	return(0); }
smb_unlocksmbhdr();
smb_close();
if(ptr) (*ptr)=status.last_msg;
return(status.total_msgs);
}


ulong loadmsgs(post_t HUGE16 **post, ulong ptr)
{
	int i;
	long l=0;
	idxrec_t idx;


if((i=smb_locksmbhdr(10))!=0) { 				/* Be sure noone deletes or */
	lprintf("ERR_LOCK %s %d\r\n",smb_file,i);   /* adds while we're reading */
	return(0L); }

fseek(sid_fp,0L,SEEK_SET);
while(!feof(sid_fp)) {
    if(!fread(&idx,sizeof(idxrec_t),1,sid_fp))
        break;

	if(idx.number<=ptr || idx.attr&MSG_DELETE)
		continue;

	if(idx.attr&MSG_MODERATED && !(idx.attr&MSG_VALIDATED))
		break;

	if(((*post)=(post_t HUGE16 *)REALLOC((*post),sizeof(post_t)*(l+1)))
        ==NULL) {
		smb_unlocksmbhdr();
		lprintf("ERR_ALLOC %s %lu\r\n",smb_file,sizeof(post_t *)*(l+1));
		return(l); }
	(*post)[l].offset=idx.offset;
	(*post)[l].number=idx.number;
	l++; }
smb_unlocksmbhdr();
return(l);
}


void allocfail(uint size)
{
lprintf("\7Error allocating %u bytes of memory.\r\n",size);
bail(1);
}

void bail(int code)
{
exit(code);
}

/****************************************************************************/
/* Returns the length of the file in 'filespec'                             */
/****************************************************************************/
long flength(char *filespec)
{
	struct find_t f;

if(!findfirst(filespec,&f,0))
	return(f.size);
return(-1L);
}

/****************************************************************************/
/* Checks the disk drive for the existence of a file. Returns 1 if it       */
/* exists, 0 if it doesn't.                                                 */
/* Called from upload                                                       */
/****************************************************************************/
char fexist(char *filespec)
{
	struct find_t f;

if(!findfirst(filespec,&f,0))
    return(1);
return(0);
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
	int userdat,file,i;
	char str[256],c;
	ulong l,crc;
	FILE *namedat;

if(!total_users) {		/* Load CRCs */
	fprintf(stderr,"%-25s","Loading user names...");
	sprintf(str,"%sUSER\\NAME.DAT",data_dir);
	if((namedat=fnopen(&file,str,O_RDONLY))==NULL)
		return(0);
	sprintf(str,"%sUSER\\USER.DAT",data_dir);
	if((userdat=nopen(str,O_RDONLY|O_DENYNONE))==-1) {
		fclose(namedat);
		return(0); }
	while(!feof(namedat) && !eof(userdat)) {
		if(!fread(str,LEN_ALIAS+2,1,namedat))
			break;
		if((username=(username_t *)REALLOC(username
			,(total_users+1)*sizeof(username_t)))==NULL)
			break;

		for(c=0;c<LEN_ALIAS;c++)
			if(str[c]==ETX) break;
		str[c]=0;
		strlwr(str);
		username[total_users].alias=crc32(str);
		i=0;
		while(i<LOOP_NODEDAB
			&& lock(userdat,(long)((long)(total_users)*U_LEN)+U_NAME
				,LEN_NAME)==-1) {
			i++; }
		if(i>=LOOP_NODEDAB) 	   /* Couldn't lock USER.DAT record */
			continue;
		lseek(userdat,(long)((long)(total_users)*U_LEN)+U_NAME,SEEK_SET);
		read(userdat,str,LEN_NAME);
		unlock(userdat,(long)((long)(total_users)*U_LEN)+U_NAME,LEN_NAME);
		for(c=0;c<LEN_NAME;c++)
			if(str[c]==ETX || str[c]==CR) break;
		str[c]=0;
		strlwr(str);
		username[total_users].real=crc32(str);
		total_users++; }
	fclose(namedat);
	close(userdat);
	fprintf(stderr,
		"\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
		"%25s"
		"\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
		,""); }

strcpy(str,inname);
strlwr(str);
crc=crc32(str);
for(l=0;l<total_users;l++)
	if(crc==username[l].alias || crc==username[l].real)
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
	tm.tm_hour=atoi(str+14);
	tm.tm_min=atoi(str+17);
	tm.tm_sec=0; }

if(tm.tm_year<70)
	tm.tm_year+=100;

return(mktime(&tm));
}

/****************************************************************************/
/* Returns the FidoNet address kept in str as ASCII.                        */
/****************************************************************************/
faddr_t atofaddr(char *str)
{
    char *p;
    faddr_t addr;

addr.zone=addr.net=addr.node=addr.point=0;
if((p=strchr(str,':'))!=NULL) {
    addr.zone=atoi(str);
    addr.net=atoi(p+1); }
else {
    if(total_faddrs)
		addr.zone=faddr[0].zone;
    else
        addr.zone=1;
    addr.net=atoi(str); }
if(!addr.zone)              /* no such thing as zone 0 */
    addr.zone=1;
if((p=strchr(str,'/'))!=NULL)
    addr.node=atoi(p+1);
else {
    if(total_faddrs)
		addr.net=faddr[0].net;
    else
        addr.net=1;
    addr.node=atoi(str); }
if((p=strchr(str,'.'))!=NULL)
    addr.point=atoi(p+1);
return(addr);
}

/****************************************************************************/
/* Returns an ASCII string for FidoNet address 'addr'                       */
/****************************************************************************/
char *faddrtoa(faddr_t addr)
{
    static char str[25];
    char point[25];

sprintf(str,"%u:%u/%u",addr.zone,addr.net,addr.node);
if(addr.point) {
    sprintf(point,".%u",addr.point);
    strcat(str,point); }
return(str);
}

#ifndef __OS2__
/****************************************************************************/
/* This function reads files that are potentially larger than 32k.  		*/
/* Up to one megabyte of data can be read with each call.                   */
/****************************************************************************/
long lread(int file, char HUGE16 *buf,long bytes)
{
	long count;

for(count=bytes;count>32767;count-=32767,buf+=32767)
	if(read(file,(char *)buf,32767)!=32767)
		return(-1L);
if(read(file,(char *)buf,(int)count)!=count)
	return(-1L);
return(bytes);
}
#endif

/****************************************************************************/
/* Coverts a FidoNet message into a Synchronet message						*/
/****************************************************************************/
void fmsgtosmsg(int file, fmsghdr_t fmsghdr, smbstatus_t status, uint user
	,uint subnum)
{
	uchar	ch,HUGE16 *fbuf,HUGE16 *sbody,HUGE16 *stail,HUGE16 *outbuf
			,done,col,esc,cr,*p,str[128];
	int 	i,chunk,lzh=0,storage;
	ushort	xlat,net;
	ulong	l,m,length,lzhlen,bodylen,taillen,crc;
	faddr_t faddr,origaddr,destaddr;
	smbmsg_t	msg;

memset(&msg,0,sizeof(smbmsg_t));
memcpy(msg.hdr.id,"SHD\x1a",4);
msg.hdr.version=SMB_VERSION;
if(fmsghdr.attr&FIDO_PRIVATE)
	msg.idx.attr|=MSG_PRIVATE;
msg.hdr.attr=msg.idx.attr;

if(fmsghdr.attr&FIDO_FILE)
	msg.hdr.auxattr|=MSG_FILEATTACH;

msg.hdr.when_imported.time=time(NULL);
msg.hdr.when_imported.zone=sys_timezone;
msg.hdr.when_written.time=fmsgtime(fmsghdr.time);

origaddr.zone=fmsghdr.origzone; 	/* only valid if NetMail */
origaddr.net=fmsghdr.orignet;
origaddr.node=fmsghdr.orignode;
origaddr.point=fmsghdr.origpoint;

destaddr.zone=fmsghdr.destzone; 	/* only valid if NetMail */
destaddr.net=fmsghdr.destnet;
destaddr.node=fmsghdr.destnode;
destaddr.point=fmsghdr.destpoint;

smb_hfield(&msg,SENDER,strlen(fmsghdr.from),fmsghdr.from);
strlwr(fmsghdr.from);
msg.idx.from=crc16(fmsghdr.from);

smb_hfield(&msg,RECIPIENT,strlen(fmsghdr.to),fmsghdr.to);
strlwr(fmsghdr.to);
msg.idx.to=crc16(fmsghdr.to);

if(user) {
	sprintf(str,"%u",user);
	smb_hfield(&msg,RECIPIENTEXT,strlen(str),str);
	msg.idx.to=user;
	msg.idx.from=0; }

smb_hfield(&msg,SUBJECT,strlen(fmsghdr.subj),fmsghdr.subj);
remove_re(fmsghdr.subj);
strlwr(fmsghdr.subj);
msg.idx.subj=crc16(fmsghdr.subj);

length=filelength(file)-sizeof(fmsghdr_t);
if((fbuf=(char *)LMALLOC(length+1))==NULL) {
	printf("alloc error\r\n");
	smb_freemsgmem(msg);
	return; }
if((sbody=(char *)LMALLOC((length+1)*2L))==NULL) {
	printf("alloc error\n");
	LFREE((char *)fbuf);
	smb_freemsgmem(msg);
	return; }
if((stail=(char *)LMALLOC((length+1)*2L))==NULL) {
	printf("alloc error\n");
	LFREE((char *)fbuf);
	LFREE((char *)sbody);
	smb_freemsgmem(msg);
	return; }
lread(file,fbuf,length);

for(col=l=esc=done=bodylen=taillen=0,cr=1;l<length;l++) {
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
				smb_hfield(&msg,FIDOMSGID,m-l,fbuf+l); }

		else if(!strncmp((char *)fbuf+l+1,"REPLY:",6)) {
			l+=7;
			while(l<length && fbuf[l]<=SP) l++;
			m=l;
			while(m<length && fbuf[m]!=CR) m++;
			while(m && fbuf[m-1]<=SP) m--;
			if(m>l)
				smb_hfield(&msg,FIDOREPLYID,m-l,fbuf+l); }

		else if(!strncmp((char *)fbuf+l+1,"FLAGS:",6)) {
			l+=7;
			while(l<length && fbuf[l]<=SP) l++;
			m=l;
			while(m<length && fbuf[m]!=CR) m++;
			while(m && fbuf[m-1]<=SP) m--;
			if(m>l)
				smb_hfield(&msg,FIDOFLAGS,m-l,fbuf+l); }

		else if(!strncmp((char *)fbuf+l+1,"PATH:",5)) {
			l+=6;
			while(l<length && fbuf[l]<=SP) l++;
			m=l;
			while(m<length && fbuf[m]!=CR) m++;
			while(m && fbuf[m-1]<=SP) m--;
			if(m>l)
				smb_hfield(&msg,FIDOPATH,m-l,fbuf+l); }

		else if(!strncmp((char *)fbuf+l+1,"PID:",4)) {
			l+=5;
			while(l<length && fbuf[l]<=SP) l++;
			m=l;
			while(m<length && fbuf[m]!=CR) m++;
			while(m && fbuf[m-1]<=SP) m--;
			if(m>l)
				smb_hfield(&msg,FIDOPID,m-l,fbuf+l); }

		else {		/* Unknown kludge line */
			while(l<length && fbuf[l]<=SP) l++;
			m=l;
			while(m<length && fbuf[m]!=CR) m++;
			while(m && fbuf[m-1]<=SP) m--;
			if(m>l)
				smb_hfield(&msg,FIDOCTRL,m-l,fbuf+l); }

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
			if(m>l)
				smb_hfield(&msg,FIDOSEENBY,m-l,fbuf+l);
			while(l<length && fbuf[l]!=CR) l++;
			continue; }
		if(done)
			stail[taillen++]=ch;
		else
			sbody[bodylen++]=ch;
		col++;
		if(ch==CR) {
			cr=1;
			col=0;
			if(done)
				stail[taillen++]=LF;
			else
				sbody[bodylen++]=LF; }
		else {
			cr=0;
			if(col==1 && !strncmp((char *)fbuf+l," * Origin: ",11)) {
				p=strchr((char *)fbuf+l+11,CR); 	 /* find carriage return */
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

LFREE(fbuf);

if(bodylen>=2 && sbody[bodylen-2]==CR && sbody[bodylen-1]==LF)
	bodylen-=2; 						/* remove last CRLF if present */

if(status.max_crcs) {
	for(l=0,crc=0xffffffff;l<bodylen;l++)
		crc=ucrc32(sbody[l],crc);
	crc=~crc;

	i=smb_addcrc(status.max_crcs,crc,10);
	if(i) {
		if(i==1)
			lprintf("Duplicate message\r\n");
		else
			lprintf("smb_addcrc returned %d\r\n",i);
		smb_freemsgmem(msg);
		LFREE(sbody);
		LFREE(stail);
		return; } }

while(taillen && stail[taillen-1]<=SP)	/* trim all garbage off the tail */
	taillen--;

net=NET_FIDO;	/* Record origin address */
smb_hfield(&msg,SENDERNETTYPE,sizeof(ushort),&net);
smb_hfield(&msg,SENDERNETADDR,sizeof(fidoaddr_t),&origaddr);

if(subnum==INVALID_SUB) { /* No origin line means NetMail, so add dest addr */
	smb_hfield(&msg,RECIPIENTNETTYPE,sizeof(ushort),&net);
	smb_hfield(&msg,RECIPIENTNETADDR,sizeof(fidoaddr_t),&destaddr); }

if(subnum!=INVALID_SUB && sub[subnum]->misc&SUB_LZH
	&& bodylen+2+taillen+2>=SDT_BLOCK_LEN && bodylen) {
	if((outbuf=(char *)LMALLOC(bodylen*2))==NULL) {
		printf("alloc error for lzh: %lu\n",bodylen*2);
		smb_freemsgmem(msg);
		LFREE(sbody);
		LFREE(stail);
		return; }
	lzhlen=lzh_encode((uchar *)sbody,bodylen,(uchar *)outbuf);
	if(lzhlen>1 &&
		smb_datblocks(lzhlen+4+taillen+2)<smb_datblocks(bodylen+2+taillen+2)) {
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


if(status.attr&SMB_HYPERALLOC) {
	if((i=smb_locksmbhdr(10))!=0) {
		printf("smb_locksmbhdr returned %d\n",i);
		smb_freemsgmem(msg);
		LFREE(sbody);
		LFREE(stail);
		return; }
	msg.hdr.offset=smb_hallocdat();
	storage=SMB_HYPERALLOC; }
else {
	if(smb_open_da(10)) {
		smb_freemsgmem(msg);
		printf("error opening %s.SDA\r\n",smb_file);
		LFREE(sbody);
		LFREE(stail);
		return; }
	if(subnum!=INVALID_SUB && sub[subnum]->misc&SUB_FAST) {
		msg.hdr.offset=smb_fallocdat(l,1);
		storage=SMB_FASTALLOC; }
	else {
		msg.hdr.offset=smb_allocdat(l,1);
		storage=SMB_SELFPACK; }
	fclose(sda_fp); }

if(msg.hdr.offset && msg.hdr.offset<1L) {
	smb_unlocksmbhdr();
	smb_freemsgmem(msg);
	LFREE(sbody);
	LFREE(stail);
	printf("error %ld allocating records\r\n",msg.hdr.offset);
	return; }
fseek(sdt_fp,msg.hdr.offset,SEEK_SET);
if(lzh) {
	xlat=XLAT_LZH;
	fwrite(&xlat,2,1,sdt_fp); }
xlat=XLAT_NONE;
fwrite(&xlat,2,1,sdt_fp);
chunk=30000;
for(l=0;l<bodylen;l+=chunk) {
	if(l+chunk>bodylen)
		chunk=bodylen-l;
	fwrite(sbody+l,1,chunk,sdt_fp); }
if(taillen) {
	fwrite(&xlat,2,1,sdt_fp);
	fwrite(stail,1,taillen,sdt_fp); }
fflush(sdt_fp);
LFREE(sbody);
LFREE(stail);

if(status.attr&SMB_HYPERALLOC)
	smb_unlocksmbhdr();

if(lzh)
	bodylen+=2;
bodylen+=2;
smb_dfield(&msg,TEXT_BODY,bodylen);
if(taillen)
	smb_dfield(&msg,TEXT_TAIL,taillen+2);

smb_addmsghdr(&msg,&status,storage,10);
smb_freemsgmem(msg);
}

/****************************************************************/
/* Get zone and point from kludge lines in 'file' if they exist */
/****************************************************************/
void getzpt(int file, fmsghdr_t *hdr)
{
	char buf[0x1000];
	int i,len,cr=0;
	faddr_t faddr;

len=read(file,buf,0x1000);
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
lseek(file,sizeof(fmsghdr_t),SEEK_SET);
}

/***********************************/
/* Synchronet/FidoNet Message util */
/***********************************/
int main(int argc, char **argv)
{
	char	ch,str[512],fname[256],touser[512],subj[512],path[512],sub_code[9]
			,*p,*tp,*sp,*buf,*outbuf,cr,tear,lzh;
	ushort	xlat;
	int 	i,j,k,n,x,last,file,fmsg,nextmsg,g;
	ulong	files,msgfiles,echomail=0,netmail=0,exported=0,crc,
			l,m,length,lastmsg,posts,msgs,exp;
	time_t	ptr,now,start,lastimport;
	read_cfg_text_t txt;
	struct	find_t ff;
	struct	tm tm,*tm_p;
	fmsghdr_t hdr;
	faddr_t addr,sys_faddr;
	post_t	HUGE16 *post;
	FILE	*stream,*fstream;
	smbstatus_t status;
	smbmsg_t msg;

lprintf("\nSynchronet <=> FidoNet Utility  Version %s  "
	"Developed by Rob Swindell\n",VER);

putenv("TZ=UCT0");
_fmode=O_BINARY;
setvbuf(stdout,NULL,_IONBF,0);

txt.openerr="\7\r\nError opening %s for read.\r\n";
txt.reading="\r\nReading %s...";
txt.readit="\rRead %s       ";
txt.allocerr="\7\r\nError allocating %u bytes of memory\r\n";
txt.error="\7\r\nERROR: Offset %lu in %s\r\n\r\n";

node_dir[0]=sub_code[0]=0;
for(i=1;i<argc;i++) {
	if(argv[i][0]=='/') {
		j=1;
		while(argv[i][j]) {
			switch(toupper(argv[i][j])) {
				case 'A':
                    misc|=ASCII_ONLY;
                    break;
				case 'B':
					misc|=LOCAL_NETMAIL;
					break;
				case 'C':
                    misc|=PURGE_ECHOMAIL;
                    break;
				case 'D':
                    misc&=~DELETE_NETMAIL;
                    break;
				case 'E':
                    misc&=~EXPORT_ECHOMAIL;
                    break;
				case 'F':
                    misc|=IMPORT_LOCAL;
                    break;
				case 'G':
                    misc|=IMPORT_NEW_ONLY;
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
				case 'K':
                    misc&=~KILL_ECHOMAIL;
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
					misc|=IGNORE_POINT;
					break;
				case 'Q':
					misc|=DONT_SET_RECV;
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
                    misc&=~DELETE_ECHOMAIL;
                    break;
                case 'Y':
                    misc|=UNKNOWN_NETMAIL;
                    break;
				case 'Z':
                    misc|=IGNORE_ZONE;
                    break;

				case '=':
					misc|=CONVERT_TEAR;
					break;
				case '!':
					misc|=NOTIFY_RECEIPT;
					break;
				case '2':
					startmsg=2;
					break;
				default:
					printf("\nusage: sbbsfido [sbbsnode] [/switches] "
						"[sub_code]");
					printf("\nwhere: sbbsnode is the path for your "
						"NODE1 directory (example: c:\\sbbs\\node1)\n");
					printf("       sub_code is the internal code for a "
						"sub-board (default is ALL subs)\n");
					printf("\nvalid switches:\n\n");
					printf("n:do not import netmail               "
						   "i:do not import echomail\n");
					printf("p:ignore point in netmail address     "
						   "e:do not export echomail\n");
					printf("z:ignore zone in netmail address      "
						   "h:export all echomail (hub rescan)\n");
					printf("o:ignore entire netmail address       "
						   "m:ignore message pointers (export all)\n");
					printf("y:import netmail for unknown users    "
						   "u:update message pointers (export none)\n");
					printf("d:do not delete netmail after import  "
						   "x:do not delete echomail after import\n");
					printf("l:output to SBBSFIDO.LOG (verbose)    "
						   "k:do not kill echomail after export\n");
					printf("r:create report of import totals      "
						   "t:do not update message pointers\n");
					printf("a:export ASCII characters only        "
						   "c:delete all messages (echomail purge)\n");
					printf("j:ignore recieved bit on import       "
						   "s:import private override (strip pvt)\n");
					printf("q:do not set received bit on import   "
                           "g:import new echomail only\n");
					printf("b:import locally created netmail too  "
                           "f:import locally created echomail too\n");
					printf("=:change existing tear lines to ===   "
                           "2:export/import/delete starting at 2.MSG\n");
					printf("!:notify users of received echomail\n");
					exit(0); }
			j++; } }
	else {
		if(strchr(argv[i],'\\') || argv[i][1]==':')
			sprintf(node_dir,"%.40s",argv[i]);
		else
			sprintf(sub_code,"%.8s",argv[i]); }  }

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
if(text_dir[0]=='.') {   /* Relative path */
	strcpy(str,text_dir);
	sprintf(text_dir,"%s%s",node_dir,str);
	if(_fullpath(str,text_dir,40))
		strcpy(text_dir,str); }
backslash(text_dir);
read_msgs_cfg(txt);

if(total_faddrs<1) {
	sys_faddr.zone=sys_faddr.net=sys_faddr.node=1;
	sys_faddr.point=0; }
else
	sys_faddr=faddr[0];


if(misc&LOGFILE)
	if((fidologfile=fnopen(&i,"SBBSFIDO.LOG"
		,O_WRONLY|O_APPEND|O_CREAT))==NULL) {
		lprintf("\7ERROR opening SBBSFIDO.LOG\r\n");
		exit(1); }

sprintf(str,"%s%s",ctrl_dir,"NODE.DAB");
if((nodefile=sopen(str,O_BINARY|O_RDWR,SH_DENYNO))==-1) {
	lprintf("\r\n\7Error opening %s\r\n",str);
    exit(1); }

if(misc&IMPORT_NETMAIL) {

lputs("\r\n\r\nScanning for Inbound NetMail...\r\n");

sprintf(smb_file,"%sMAIL",data_dir);
if((i=smb_open(10))!=0) {
	lprintf("Error %d opening %s\r\n",i,smb_file);
	exit(1); }

if(!filelength(fileno(shd_fp)))
	if((i=smb_create(mail_maxcrcs,MAX_SYSMAIL,mail_maxage,SMB_EMAIL,10))!=0) {
		lprintf("Error %d creating %s\r\n",i,smb_file);
		exit(1); }

if((i=smb_locksmbhdr(10))!=0) {
	lprintf("Error %d locking %s\r\n",i,smb_file);
	exit(1); }
if((i=smb_getstatus(&status))!=0) {
	lprintf("Error %d reading %s status header\r\n",i,smb_file);
	exit(1); }
smb_unlocksmbhdr();

sprintf(str,"%s*.MSG",netmail_dir);

for(last=findfirst(str,&ff,0);!last;last=findnext(&ff)) {
	sprintf(path,"%s%s",netmail_dir,ff.name);
	strupr(path);
	lprintf("\r%s    ",path);
	if((fmsg=nopen(path,O_RDWR))==-1) {
		lprintf("\7ERROR opening");
		continue; }
	if(filelength(fmsg)<sizeof(fmsghdr_t)) {
		lprintf("\7Invalid length of %u bytes\r\n",filelength(fmsg));
		close(fmsg);
		continue; }
	if(read(fmsg,&hdr,sizeof(fmsghdr_t))!=sizeof(fmsghdr_t)) {
		close(fmsg);
		lprintf("\7ERROR reading %u bytes"
			,sizeof(fmsghdr_t));
		continue; }
	if(hdr.attr&FIDO_ORPHAN) {
		close(fmsg);
		lprintf("Orphan (%s).\r\n",hdr.to);
		continue; }
	if(misc&IGNORE_ZONE)				/* default to system's zone */
		hdr.destzone=hdr.origzone=sys_faddr.zone;
	if(misc&IGNORE_POINT)				/* default to no point */
		hdr.destpoint=hdr.origpoint=0;
	getzpt(fmsg,&hdr);					/* use kludge if found */
	for(i=0;i<total_faddrs;i++)
		if(hdr.destzone==faddr[i].zone
			&& hdr.destnet==faddr[i].net
			&& hdr.destnode==faddr[i].node
			&& hdr.destpoint==faddr[i].point)
			break;
	lprintf("%u:%u/%u.%u  "
		,hdr.destzone,hdr.destnet,hdr.destnode,hdr.destpoint);
	if(misc&IGNORE_ADDRESS || i<total_faddrs) {
		if(!(misc&IGNORE_RECV) && hdr.attr&FIDO_RECV) {
            close(fmsg);
			lputs("Already received.\r\n");
            continue; }
		if(hdr.attr&FIDO_LOCAL && !(misc&LOCAL_NETMAIL)) {
			close(fmsg);
			lputs("Created locally.\r\n");
			continue; }
		i=atoi(hdr.to);
		if(!stricmp(hdr.to,"SYSOP"))  /* NetMail to "sysop" goes to #1 */
			i=1;
		if(!i)
			i=matchname(hdr.to);
		if(!i) {
			if(misc&UNKNOWN_NETMAIL)	/* receive unknown user mail to 1 */
				i=1;
			else {
				lprintf("\7ERROR unknown user '%s'\r\n",hdr.to);
				hdr.attr|=FIDO_ORPHAN;
				lseek(fmsg,0L,SEEK_SET);
				write(fmsg,&hdr,sizeof(fmsghdr_t));
				close(fmsg);
				continue; } }
		lprintf("%s\r\n",hdr.to);

		/*********************/
		/* Importing NetMail */
		/*********************/

		fmsgtosmsg(fmsg,hdr,status,i,INVALID_SUB);

		addr.zone=hdr.origzone;
		addr.net=hdr.orignet;
		addr.node=hdr.orignode;
		addr.point=hdr.origpoint;
		sprintf(str,"\7\1n\1hSBBSFIDO: \1m%.36s \1n\1msent you NetMail from "
			"\1h%s\1n\r\n"
			,hdr.from,faddrtoa(addr));
		putsmsg(i,str);

		if(hdr.attr&FIDO_FILE) {	/* File attachment */
			strcpy(subj,hdr.subj);
			tp=subj;
			while(1) {
				p=strchr(tp,SP);
				if(p) *p=0;
				sp=strrchr(tp,'/');              /* sp is slash pointer */
				if(!sp) sp=strrchr(tp,'\\');
				if(sp) tp=sp+1;
				sprintf(str,"%s%s",fidofile_dir,tp);
				sprintf(tmp,"%sFILE\\%04u.IN",data_dir,i);
				mkdir(tmp);
				strcat(tmp,"\\");
				strcat(tmp,tp);
				mv(str,tmp,0);
				if(!p)
					break;
				tp=p+1; } }
		netmail++;

		/***************************/
		/* Updating message header */
		/***************************/
		if(!(misc&DONT_SET_RECV)) {
			hdr.attr|=FIDO_RECV;
			lseek(fmsg,0L,SEEK_SET);
			write(fmsg,&hdr,sizeof(fmsghdr_t)); }

		/**************************************/
		/* Delete source netmail if specified */
		/**************************************/
		close(fmsg);
		if(misc&DELETE_NETMAIL)
			remove(path); }
	else
		close(fmsg); }
smb_close(); }


if(misc&IMPORT_ECHOMAIL) {

start=time(NULL);

lputs("\r\n\r\nScanning for Inbound EchoMail...\r\n");

sprintf(path,"%sSBBSFIDO.DAB",data_dir);
if((file=nopen(path,O_RDWR|O_CREAT))==-1)
	lastimport=0L;
else {
	read(file,&lastimport,4);
	now=time(NULL);
	lseek(file,0L,SEEK_SET);
	write(file,&now,4);
	close(file); }

for(g=files=0;g<total_grps;g++)
for(i=0;i<total_subs;i++)
	if(sub[i]->misc&SUB_FIDO && sub[i]->grp==g) {
		if(sub_code[0] && stricmp(sub_code,sub[i]->code))
			continue;
		if(!sub[i]->echopath[0])
			sprintf(sub[i]->echopath,"%s%s\\",echomail_dir,sub[i]->code);
		if(files) {
			lputs("\r\n");
			files=0; }
		lprintf("\r\n%-15.15s %s\r\n"
			,grp[sub[i]->grp]->sname,sub[i]->lname);

		sprintf(path,"%s*.MSG",sub[i]->echopath);
		l=findfirst(path,&ff,0);
		if(startmsg==2 && !strcmp(ff.name,"1.MSG"))
			l=findnext(&ff);
		if(l)
			continue;
		lprintf("Counting %s",path);
		msgfiles=0;
		while(!l) {
			memset(&tm,0,sizeof(tm));
			tm.tm_mday=ff.wr_date&31;
			tm.tm_mon=(ff.wr_date>>5)&15;
			tm.tm_year=80+((ff.wr_date>>9)&127);
			tm.tm_hour=(ff.wr_time>>11)&31;
			tm.tm_min=(ff.wr_time>>5)&63;
			tm.tm_sec=(ff.wr_time&0x1f)<<1;
			if(isdigit(ff.name[0])
				&& !(startmsg==2 && !strcmp(ff.name,"1.MSG"))
				&& !(misc&IMPORT_NEW_ONLY && mktime(&tm)<=lastimport))
				msgfiles++; 		/* msgfiles= messages to import */
			l=findnext(&ff); }

		lprintf("\r\n%u messages.\r\n",msgfiles);
		if(!msgfiles)				/* no messages, so continue. */
			continue;

		sprintf(smb_file,"%s%s",sub[i]->data_dir,sub[i]->code);
		if((j=smb_open(10))!=0) {
			lprintf("Error %d opening %s\r\n",j,smb_file);
			continue; }
		if(!filelength(fileno(shd_fp)))
			if((j=smb_create(sub[i]->maxcrcs,sub[i]->maxmsgs
				,sub[i]->maxage
				,sub[i]->misc&SUB_HYPER ? SMB_HYPERALLOC:0
				,10))!=0) {
				lprintf("Error %d creating %s\r\n",j,smb_file);
				smb_close();
				continue; }

		if((j=smb_locksmbhdr(10))!=0) {
			lprintf("Error %d locking SMB header\r\n",j);
			smb_close();
			continue; }
		if((j=smb_getstatus(&status))!=0) {
			lprintf("Error %d reading SMB header\r\n",j);
			smb_close();
			continue; }
		smb_unlocksmbhdr();

		for(l=startmsg;l<0x8000 && msgfiles;l++) {
			sprintf(path,"%s%lu.MSG",sub[i]->echopath,l);
			if(findfirst(path,&ff,0))	/* doesn't exist */
				continue;				/* was break */
			tm.tm_mday=ff.wr_date&31;
			tm.tm_mon=(ff.wr_date>>5)&15;
			tm.tm_year=80+((ff.wr_date>>9)&127);
			tm.tm_hour=(ff.wr_time>>11)&31;
			tm.tm_min=(ff.wr_time>>5)&63;
			tm.tm_sec=(ff.wr_time&0x1f)<<1;
			if(misc&IMPORT_NEW_ONLY && mktime(&tm)<=lastimport)
				continue;
			if(startmsg==2 && !strcmp(ff.name,"1.MSG"))
				continue;
			msgfiles--; 		/* so we only look for as many as are there */
			strupr(path);
			lprintf("\r%s    ",path);
			files++;
			if((fmsg=nopen(path,O_RDWR))==-1) {
				lprintf("\7ERROR opening\r\n");
				continue; }
			if(filelength(fmsg)<sizeof(fmsghdr_t)) {
				lprintf("\7Invalid length of %u bytes\r\n",filelength(fmsg));
				close(fmsg);
				continue; }
			if(read(fmsg,&hdr,sizeof(fmsghdr_t))!=sizeof(fmsghdr_t)) {
				close(fmsg);
				lprintf("\7ERROR reading %u bytes\r\n"
					,sizeof(fmsghdr_t));
				continue; }
			if(misc&IMPORT_LOCAL || !(hdr.attr&FIDO_LOCAL)) {

				if(!(misc&IGNORE_RECV) && hdr.attr&FIDO_RECV) {
					close(fmsg);
					lputs("Already received.\r\n");
					continue; }

				if(hdr.attr&FIDO_SENT) {
					close(fmsg);
					lputs("Sent.");
					if(hdr.attr&FIDO_KILLSENT)
						if(!remove(path))
							lputs(" Killed.");
					lputs("\r\n");
					continue; }

				if(hdr.attr&FIDO_PRIVATE && !(sub[i]->misc&SUB_PRIV)) {
					if(misc&IMPORT_PRIVATE)
						hdr.attr&=~FIDO_PRIVATE;
					else {
						close(fmsg);
						lputs("Private posts disallowed.\r\n");
						continue; } }

				if(!(hdr.attr&FIDO_PRIVATE) && sub[i]->misc&SUB_PONLY)
					hdr.attr|=MSG_PRIVATE;

				/**********************/
				/* Importing EchoMail */
				/**********************/

				fmsgtosmsg(fmsg,hdr,status,0,i);

				echomail++;

				/* Updating message header */
				if(!(misc&DONT_SET_RECV)) {
					hdr.attr|=FIDO_RECV;
					lseek(fmsg,0L,SEEK_SET);
					write(fmsg,&hdr,sizeof(fmsghdr_t)); }

				close(fmsg);
				if(misc&NOTIFY_RECEIPT && (m=matchname(hdr.to))!=0) {
					sprintf(str
					,"\7\1n\1hSBBSFIDO: \1m%.36s \1n\1msent you EchoMail on "
						"\1h%s \1n\1m%s\1n\r\n"
						,hdr.from,grp[sub[i]->grp]->sname,sub[i]->sname);
					putsmsg(m,str); }

				/* Delete source EchoMail if specified */
				if(misc&DELETE_ECHOMAIL)
					remove(path); }
			else
				close(fmsg); }
		smb_close();
		}

now=time(NULL);
if(now-start)
	lprintf("\r\nImported %lu EchoMail messages in %lu seconds "
		"(%lu messages/second).\r\n"
		,echomail,now-start,echomail/(now-start));
}

if(misc&EXPORT_ECHOMAIL) {

start=time(NULL);

lputs("\r\n\r\nScanning for Outbound EchoMail...\r\n");

for(g=files=0;g<total_grps;g++)
for(i=0;i<total_subs;i++)
	if(sub[i]->misc&SUB_FIDO && sub[i]->grp==g) {
		if(sub_code[0] && stricmp(sub_code,sub[i]->code))
            continue;
		if(!sub[i]->echopath[0])
            sprintf(sub[i]->echopath,"%s%s\\",echomail_dir,sub[i]->code);
		if(files) {
			lputs("\r\n");
			files=0; }
		lprintf("\r\n%-15.15s %s\r\n"
			,grp[sub[i]->grp]->sname,sub[i]->lname);
		ptr=0;
		if(!(misc&IGNORE_MSGPTRS)) {
			sprintf(str,"%s%s.SFP",sub[i]->data_dir,sub[i]->code);
			if((file=nopen(str,O_RDONLY))!=-1) {
				read(file,&ptr,sizeof(time_t));
				close(file); } }

		msgs=getlastmsg(i,&lastmsg,0);
		if(!msgs || (!(misc&IGNORE_MSGPTRS) && ptr>=lastmsg)) {
			if(ptr>lastmsg && !(misc&LEAVE_MSGPTRS)) {	/* fix ptr */
                sprintf(str,"%s%s.SFP",sub[i]->data_dir,sub[i]->code);
				if((file=nopen(str,O_WRONLY|O_CREAT))==-1)
					lprintf("\7ERROR opening/creating %s",str);
				else {
					write(file,&lastmsg,4);
					close(file); } }
			continue; }
		nextmsg=startmsg;

		sprintf(smb_file,"%s%s"
			,sub[i]->data_dir,sub[i]->code);
		if((j=smb_open(10))!=0) {
			lprintf("Error %d opening %s\r\n",j,smb_file);
			continue; }

		post=NULL;
		posts=loadmsgs(&post,ptr);

		if(!posts)	{ /* no new messages */
			smb_close();
			if(post)
				FREE(post);
			continue; }

		for(m=exp=0;m<posts;m++) {
			printf("\rScanning: %lu of %lu     "
				,m+1,posts);

			msg.idx.offset=post[m].offset;
			if((k=smb_lockmsghdr(msg,10))!=0) {
				lprintf("ERR_LOCK %s %d\r\n",smb_file,k);
				continue; }
			k=smb_getmsghdr(&msg);
			if(k || msg.hdr.number!=post[m].number) {
				smb_unlockmsghdr(msg);
				smb_freemsgmem(msg);

				msg.hdr.number=post[m].number;
				if((k=smb_getmsgidx(&msg))!=0) {
					lprintf("ERR_READ %s %d\r\n",smb_file,k);
					continue; }
				if((k=smb_lockmsghdr(msg,10))!=0) {
					lprintf("ERR_LOCK %s %d\r\n",smb_file,k);
					continue; }
				if((k=smb_getmsghdr(&msg))!=0) {
					smb_unlockmsghdr(msg);
					lprintf("ERR_READ %s %d\r\n",smb_file,k);
					continue; } }

			if((!(misc&EXPORT_ALL) && msg.from_net.type==NET_FIDO)
				|| !strnicmp(msg.subj,"NE:",3)) {   /* no echo */
				smb_unlockmsghdr(msg);
				smb_freemsgmem(msg);
				continue; }  /* From a Fido node, ignore it */

			if(msg.from_net.type && msg.from_net.type!=NET_FIDO
				&& !(sub[i]->misc&SUB_GATE)) {
				smb_unlockmsghdr(msg);
				smb_freemsgmem(msg);
				continue; }

			for(j=nextmsg;j;j++) {
				sprintf(fname,"%s%u.MSG",sub[i]->echopath,j);
				if(!fexist(fname))
					break; }
			if(!j) {
				lputs("\7EchoMail dir full!");
				smb_unlockmsghdr(msg);
				smb_freemsgmem(msg);
				continue; }
			nextmsg=j+1;
			strupr(fname);
			if((fmsg=nopen(fname,O_WRONLY|O_CREAT))==-1) {
				smb_unlockmsghdr(msg);
				smb_freemsgmem(msg);
				lprintf("\7ERROR creating %s\r\n",fname);
				continue; }
			if((fstream=fdopen(fmsg,"wb"))==NULL) {
				close(fmsg);
				smb_unlockmsghdr(msg);
                smb_freemsgmem(msg);
				lprintf("\7ERROR fdopen %s\r\n",fname);
				continue; }
			setvbuf(fstream,NULL,_IOFBF,2048);

			files++;

			memset(&hdr,0,sizeof(fmsghdr_t));	 /* Zero the header */
			hdr.origzone=sub[i]->faddr.zone;
			hdr.orignet=sub[i]->faddr.net;
			hdr.orignode=sub[i]->faddr.node;
			hdr.origpoint=sub[i]->faddr.point;

			hdr.attr=FIDO_LOCAL;
			if(misc&KILL_ECHOMAIL)
				hdr.attr|=FIDO_KILLSENT;
			if(msg.hdr.attr&MSG_PRIVATE)
				hdr.attr|=FIDO_PRIVATE;

			sprintf(hdr.from,"%.35s",msg.from);

			tm_p=gmtime((time_t *)&msg.hdr.when_written.time);
			sprintf(hdr.time,"%02u %3.3s %02u  %02u:%02u:%02u"
				,tm_p->tm_mday,mon[tm_p->tm_mon],tm_p->tm_year%100
				,tm_p->tm_hour,tm_p->tm_min,tm_p->tm_sec);

			sprintf(hdr.to,"%.35s",msg.to);

			sprintf(hdr.subj,"%.71s",msg.subj);

			fwrite(&hdr,sizeof(fmsghdr_t),1,fstream);

			for(j=0;j<msg.hdr.total_dfields;j++) {

				if(msg.dfield[j].type!=TEXT_BODY
					&& msg.dfield[j].type!=TEXT_TAIL)
					continue;					/* skip non-text data fields */

				if(msg.dfield[j].length<3)		/* need at least 3 bytes */
					continue;

				fseek(sdt_fp,msg.hdr.offset+msg.dfield[j].offset,SEEK_SET);

				lzh=0;
				fread(&xlat,2,1,sdt_fp);
				if(xlat==XLAT_LZH) {
					lzh=1;
					fread(&xlat,2,1,sdt_fp); }
				if(xlat!=XLAT_NONE) 	/* no other translations supported */
					continue;

				length=msg.dfield[j].length-2;
				if(lzh)
					length-=2;

				if((buf=LMALLOC(length))==NULL) {
					lprintf("Error allocating %lu bytes\r\n",length);
					continue; }

				fread(buf,length,1,sdt_fp);

				if(lzh) {
					l=*(long *)buf;
					if((outbuf=LMALLOC(l))==NULL) {
						lprintf("Error allocationg %lu for lzh\r\n",l);
						LFREE(buf);
						continue; }
					length=lzh_decode(buf,length,outbuf);
					LFREE(buf);
					buf=outbuf; }

				tear=0;
				for(l=0,cr=1;l<length;l++) {
					if(buf[l]==1) { /* Ctrl-A, so skip it and the next char */
                        l++;
						continue; }
					if(buf[l]==LF || buf[l]==0)  /* Ignore line feeds */
						continue;
					if(cr) {
						if(l+3<length && buf[l]=='-' && buf[l+1]=='-'
							&& buf[l+2]=='-'
							&& (buf[l+3]==SP || buf[l+3]==CR)) {
							if(misc&CONVERT_TEAR)	/* Convert to === */
								buf[l]=buf[l+1]=buf[l+2]='=';
							else
								tear=1; }
						else if(l+10<length
							&& !strncmp(buf+l," * Origin: ",11))
							buf[l+1]='#'; } /* Convert * Origin into # Origin */

					if(buf[l]==CR)
						cr=1;
					else
						cr=0;
					if(sub[i]->misc&SUB_ASCII || misc&ASCII_ONLY) {
						if(buf[l]<SP && buf[l]!=CR) /* Ctrl ascii */
							buf[l]='.';             /* converted to '.' */
						if((uchar)buf[l]>0x7f)		/* extended ASCII */
							buf[l]='*'; }           /* converted to '*' */
					fputc(buf[l],fstream); }
				fprintf(fstream,"\r\n");
				LFREE(buf); }

			if(!(sub[i]->misc&SUB_NOTAG)) {
				if(!tear)	/* No previous tear line */
					fprintf(fstream,"--- Synchronet+SBBSfido v%s\r\n"
						,VER);	/* so add ours */
				fprintf(fstream," * Origin: %s (%s)\r\n"
					,sub[i]->origline[0] ? sub[i]->origline : origline
					,faddrtoa(sub[i]->faddr)); }

			fputc(0,fstream);	/* Null terminator */
			fclose(fstream);
			exported++;
			exp++;
			printf("Exported: %lu of %lu",exp,exported);
			smb_unlockmsghdr(msg);
			smb_freemsgmem(msg); }

		smb_close();
		FREE(post);

		/***********************/
		/* Update FIDO_PTR.DAB */
		/***********************/
		if(!(misc&LEAVE_MSGPTRS) && lastmsg>ptr) {
			sprintf(str,"%s%s.SFP",sub[i]->data_dir,sub[i]->code);
			if((file=nopen(str,O_WRONLY|O_CREAT))==-1)
				lprintf("\7ERROR opening/creating %s",str);
			else {
				write(file,&lastmsg,4);
				close(file); } } }

now=time(NULL);
if(now-start)
	lprintf("\r\nExported %lu EchoMail messages in %lu seconds "
		"(%lu messages/second).\r\n"
		,exported,now-start,exported/(now-start));

}

if(misc&UPDATE_MSGPTRS) {

lputs("\r\n\r\nUpdating Message Pointers to Last Posted Message...\r\n");

for(g=0;g<total_grps;g++)
for(i=0;i<total_subs;i++)
	if(sub[i]->misc&SUB_FIDO && sub[i]->grp==g) {
		lprintf("\r\n%-15.15s %s\r\n"
			,grp[sub[i]->grp]->sname,sub[i]->lname);
		getlastmsg(i,&l,0);
		sprintf(str,"%s%s.SFP",sub[i]->data_dir,sub[i]->code);
		if((file=nopen(str,O_WRONLY|O_CREAT))==-1)
			lprintf("\7ERROR opening/creating %s",str);
		else {
			write(file,&l,sizeof(time_t));
			close(file); } } }

if(misc&PURGE_ECHOMAIL) {

lputs("\r\n\r\nPurging EchoMail...\r\n");

for(g=0;g<total_grps;g++)
for(i=0;i<total_subs;i++)
	if(sub[i]->misc&SUB_FIDO && sub[i]->grp==g) {
		if(sub_code[0] && stricmp(sub_code,sub[i]->code))
            continue;
		if(!sub[i]->echopath[0])
            sprintf(sub[i]->echopath,"%s%s\\",echomail_dir,sub[i]->code);
		sprintf(str,"%s*.MSG",sub[i]->echopath);
		last=findfirst(str,&ff,0);
		while(!last) {
			sprintf(str,"%s%s",sub[i]->echopath,ff.name);
			if(startmsg!=2 || strcmp(ff.name,"1.MSG")) {
				lprintf("\r\nDeleting %s",str);
				remove(str); }
			last=findnext(&ff); } } }


if(misc&(IMPORT_NETMAIL|IMPORT_ECHOMAIL) && misc&REPORT) {
	sprintf(str,"%sSBBSFIDO.MSG",text_dir);
	if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
		lprintf("Error opening %s\r\n",str);
		exit(1); }
	sprintf(fname,"\1c\1h               "
		"‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹‹\r\n");
	sprintf(path,"\1c\1h               "
		"ﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂﬂ\r\n");
	write(file,fname,strlen(fname));
	sprintf(str,"               \1n\1k\0016"
		" Last FidoNet Transfer on %.24s \1n\r\n",ctime(&start));
	write(file,str,strlen(str));
	write(file,path,strlen(path));
	write(file,fname,strlen(fname));
	sprintf(subj,"Imported %lu EchoMail and %lu NetMail Messages"
		,echomail,netmail);
	sprintf(str,"               \1n\1k\0016 %-50.50s\1n\r\n",subj);
	write(file,str,strlen(str));
	write(file,path,strlen(path));
	close(file); }

return(0);
}

