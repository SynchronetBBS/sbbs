/* SMBUTIL.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* Synchronet Message Base Utility */

#define SMBUTIL_VER "2.01"

#include "smblib.h"
#include "smbutil.h"
#include "crc32.h"
#include "crc16.c"

#ifdef __WATCOMC__
	#include <dos.h>
#endif

/********************/
/* Global variables */
/********************/

smb_t smb;
ulong mode=0L;
ushort tzone=PST;
char filein[128];
char attach[128];

/************************/
/* Program usage/syntax */
/************************/

char *usage=
"usage: smbutil [/opts] cmd <filespec.SHD>\n"
"\n"
"cmd:\n"
"       l[n] = list msgs starting at number n\n"
"       r[n] = read msgs starting at number n\n"
"       v[n] = view msg headers starting at number n\n"
"       i<f> = import msg from text file f\n"
"       e<f> = import e-mail from text file f\n"
"       n<f> = import netmail from text file f\n"
"       s    = display msg base status\n"
"       c    = change msg base status\n"
"       m    = maintain msg base - delete old msgs and msgs over max\n"
"       p[k] = pack msg base (k specifies minimum packable Kbytes)\n"
"opts:\n"
"       a    = always pack msg base (disable compression analysis)\n"
"       z[n] = set time zone (n=min +/- from UT or 'EST','EDT','CST',etc)\n"
;

char *wday[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
char *mon[]={"Jan","Feb","Mar","Apr","May","Jun"
            ,"Jul","Aug","Sep","Oct","Nov","Dec"};


/****************************************************************************/
/* Checks the disk drive for the existence of a file. Returns 1 if it       */
/* exists, 0 if it doesn't.                                                 */
/****************************************************************************/
char fexist(char *filespec)
{
	struct ffblk f;

if(findfirst(filespec,&f,0)==0)
    return(1);
return(0);
}

/****************************************************************************/
/* Returns the length of the file in 'filespec'                             */
/****************************************************************************/
long flength(char *filespec)
{
	struct ffblk f;

if(findfirst(filespec,&f,0)==0)
#ifdef __WATCOMC__
	return(f.size);
#else
	return(f.ff_fsize);
#endif
return(-1L);
}

void remove_re(char *str)
{
while(!strnicmp(str,"RE:",3)) {
	strcpy(str,str+3);
	while(str[0]==SP)
		strcpy(str,str+1); }
}

/****************************************************************************/
/* Adds a new message to the message base									*/
/****************************************************************************/
void postmsg(char type)
{
	char	str[128],buf[SDT_BLOCK_LEN];
	ushort	xlat,net;
	int 	i,j,k,file;
	long	length;
	ulong	offset,crc=0xffffffffUL;
	FILE	*instream;
	smbmsg_t	msg;

length=flength(filein);
if(length<1L) {
	printf("Invalid file size for '%s'\n",filein);
	exit(1); }
length+=2;	/* for translation string */
if(!(smb.status.attr&SMB_HYPERALLOC)) {
	i=smb_open_da(&smb);
	if(i) {
		printf("smb_open_da returned %d\n",i);
		exit(1); }
	offset=smb_allocdat(&smb,length,1);
	smb_close_da(&smb); }
else
	offset=smb_hallocdat(&smb);

if((file=open(filein,O_RDONLY|O_BINARY))==-1
	|| (instream=fdopen(file,"rb"))==NULL) {
	printf("Error opening %s for read\n",filein);
	smb_freemsgdat(&smb,offset,length,1);
	exit(1); }
setvbuf(instream,NULL,_IOFBF,32*1024);
fseek(smb.sdt_fp,offset,SEEK_SET);
xlat=XLAT_NONE;
fwrite(&xlat,2,1,smb.sdt_fp);
k=SDT_BLOCK_LEN-2;
while(!feof(instream)) {
	memset(buf,0,k);
	j=fread(buf,1,k,instream);
	if(smb.status.max_crcs)
		for(i=0;i<j;i++)
			crc=ucrc32(buf[i],crc);
	fwrite(buf,k,1,smb.sdt_fp);
	k=SDT_BLOCK_LEN; }
fflush(smb.sdt_fp);
fclose(instream);
crc=~crc;

memset(&msg,0,sizeof(smbmsg_t));
memcpy(msg.hdr.id,"SHD\x1a",4);
msg.hdr.version=SMB_VERSION;
msg.hdr.when_written.time=time(NULL);
msg.hdr.when_written.zone=tzone;
msg.hdr.when_imported=msg.hdr.when_written;

if(smb.status.max_crcs) {
	i=smb_addcrc(&smb,crc);
	if(i) {
		printf("smb_addcrc returned %d\n",i);
		smb_freemsgdat(&smb,offset,length,1);
		exit(1); } }

msg.hdr.offset=offset;

printf("To User Name: ");
gets(str);
i=smb_hfield(&msg,RECIPIENT,strlen(str),str);
if(i) {
	printf("smb_hfield returned %d\n",i);
	smb_freemsgdat(&smb,offset,length,1);
	exit(1); }
if(type=='E' || type=='N')
	smb.status.attr|=SMB_EMAIL;
if(smb.status.attr&SMB_EMAIL) {
	printf("To User Number (0=QWKnet or Internet): ");
	gets(str);
	i=smb_hfield(&msg,RECIPIENTEXT,strlen(str),str);
	if(i) {
		printf("smb_hfield returned %d\n",i);
		smb_freemsgdat(&smb,offset,length,1);
		exit(1); }
	msg.idx.to=atoi(str); }
else {
	strlwr(str);
	msg.idx.to=crc16(str); }

if(type=='N') {
	printf("To Address: ");
	gets(str);
	if(*str) {
		if(strchr(str,'.'))
			net=NET_INTERNET;
		else
			net=NET_QWK;
		i=smb_hfield(&msg,RECIPIENTNETTYPE,sizeof(net),&net);
		if(i) {
			printf("smb_hfield returned %d\n",i);
			smb_freemsgdat(&smb,offset,length,1);
			exit(1); }
		i=smb_hfield(&msg,RECIPIENTNETADDR,strlen(str),str);
		if(i) {
			printf("smb_hfield returned %d\n",i);
			smb_freemsgdat(&smb,offset,length,1);
			exit(1); } } }

printf("From User Name: ");
gets(str);
i=smb_hfield(&msg,SENDER,strlen(str),str);
if(i) {
	printf("smb_hfield returned %d\n",i);
	smb_freemsgdat(&smb,offset,length,1);
    exit(1); }
if(smb.status.attr&SMB_EMAIL) {
	printf("From User Number: ");
	gets(str);
	i=smb_hfield(&msg,SENDEREXT,strlen(str),str);
	if(i) {
		printf("smb_hfield returned %d\n",i);
		smb_freemsgdat(&smb,offset,length,1);
		exit(1); }
	msg.idx.from=atoi(str); }
else {
	strlwr(str);
	msg.idx.from=crc16(str); }

printf("Subject: ");
gets(str);
i=smb_hfield(&msg,SUBJECT,strlen(str),str);
if(i) {
	printf("smb_hfield returned %d\n",i);
	smb_freemsgdat(&smb,offset,length,1);
    exit(1); }
remove_re(str);
strlwr(str);
msg.idx.subj=crc16(str);

i=smb_dfield(&msg,TEXT_BODY,length);
if(i) {
	printf("smb_dfield returned %d\n",i);
	smb_freemsgdat(&smb,offset,length,1);
	exit(1); }

i=smb_addmsghdr(&smb,&msg,smb.status.attr&SMB_HYPERALLOC);

if(i) {
	printf("smb_addmsghdr returned %d\n",i);
	smb_freemsgdat(&smb,offset,length,1);
	exit(1); }
smb_freemsgmem(&msg);

}

/****************************************************************************/
/* Shows the message base header											*/
/****************************************************************************/
void showstatus()
{
	int i;

i=smb_locksmbhdr(&smb);
if(i) {
	printf("smb_locksmbhdr returned %d\n",i);
	return; }
i=smb_getstatus(&smb);
smb_unlocksmbhdr(&smb);
if(i) {
	printf("smb_getstatus returned %d\n",i);
	return; }
printf("last_msg        =%lu\n"
	   "total_msgs      =%lu\n"
	   "header_offset   =%lu\n"
	   "max_crcs        =%lu\n"
	   "max_msgs        =%lu\n"
	   "max_age         =%u\n"
	   "attr            =%04Xh\n"
	   ,smb.status.last_msg
	   ,smb.status.total_msgs
	   ,smb.status.header_offset
	   ,smb.status.max_crcs
	   ,smb.status.max_msgs
	   ,smb.status.max_age
	   ,smb.status.attr
	   );
}

/****************************************************************************/
/* Configure message base header											*/
/****************************************************************************/
void config()
{
	char max_msgs[128],max_crcs[128],max_age[128],header_offset[128],attr[128];
	int i;

i=smb_locksmbhdr(&smb);
if(i) {
	printf("smb_locksmbhdr returned %d\n",i);
	return; }
i=smb_getstatus(&smb);
smb_unlocksmbhdr(&smb);
if(i) {
	printf("smb_getstatus returned %d\n",i);
	return; }
printf("Header offset =%-5lu  New value (CR=No Change): "
	,smb.status.header_offset);
gets(header_offset);
printf("Max msgs      =%-5lu  New value (CR=No Change): "
	,smb.status.max_msgs);
gets(max_msgs);
printf("Max crcs      =%-5lu  New value (CR=No Change): "
	,smb.status.max_crcs);
gets(max_crcs);
printf("Max age       =%-5u  New value (CR=No Change): "
	,smb.status.max_age);
gets(max_age);
printf("Attributes    =%-5u  New value (CR=No Change): "
	,smb.status.attr);
gets(attr);
i=smb_locksmbhdr(&smb);
if(i) {
	printf("smb_locksmbhdr returned %d\n",i);
	return; }
i=smb_getstatus(&smb);
if(i) {
	printf("smb_getstatus returned %d\n",i);
	smb_unlocksmbhdr(&smb);
    return; }
if(isdigit(max_msgs[0]))
	smb.status.max_msgs=atol(max_msgs);
if(isdigit(max_crcs[0]))
	smb.status.max_crcs=atol(max_crcs);
if(isdigit(max_age[0]))
	smb.status.max_age=atoi(max_age);
if(isdigit(header_offset[0]))
	smb.status.header_offset=atol(header_offset);
if(isdigit(attr[0]))
	smb.status.attr=atoi(attr);
i=smb_putstatus(&smb);
smb_unlocksmbhdr(&smb);
if(i)
	printf("smb_putstatus returned %d\n",i);
}

/****************************************************************************/
/* Lists messages' to, from, and subject                                    */
/****************************************************************************/
void listmsgs(ulong start, ulong count)
{
	int i;
	ulong l=0;
	smbmsg_t msg;
	idxrec_t idxrec;

if(!start)
	start=1;
fseek(smb.sid_fp,(start-1L)*sizeof(idxrec_t),SEEK_SET);
while(l<count) {
	if(!fread(&msg.idx,1,sizeof(idxrec_t),smb.sid_fp))
		break;
	i=smb_lockmsghdr(&smb,&msg);
	if(i) {
		printf("smb_lockmsghdr returned %d\n",i);
		break; }
	i=smb_getmsghdr(&smb,&msg);
	smb_unlockmsghdr(&smb,&msg);
	if(i) {
		printf("smb_getmsghdr returned %d\n",i);
		break; }
	printf("%4lu %-25.25s %-25.25s %.20s\n"
		,msg.hdr.number,msg.from,msg.to,msg.subj);
	smb_freemsgmem(&msg);
	l++; }
}

/****************************************************************************/
/* Returns an ASCII string for FidoNet address 'addr'                       */
/****************************************************************************/
char *faddrtoa(fidoaddr_t addr)
{
	static char str[25];
	char point[25];

sprintf(str,"%u:%u/%u",addr.zone,addr.net,addr.node);
if(addr.point) {
	sprintf(point,".%u",addr.point);
	strcat(str,point); }
return(str);
}

char *binstr(uchar *buf, ushort length)
{
	static char str[128];
	char tmp[128];
	int i;

str[0]=0;
for(i=0;i<length;i++)
	if(buf[i] && (buf[i]<SP || buf[i]>=0x7f))
		break;
if(i==length)		/* not binary */
	return(buf);
for(i=0;i<length;i++) {
	sprintf(tmp,"%02X ",buf[i]);
	strcat(str,tmp); }
return(str);
}

/****************************************************************************/
/* Generates a 24 character ASCII string that represents the time_t pointer */
/* Used as a replacement for ctime()                                        */
/****************************************************************************/
char *timestr(time_t *intime)
{
    static char str[256];
    char mer[3],hour;
    struct tm *gm;

printf("before localtime\n");
gm=localtime(intime);
printf("after\n");
if(gm==NULL) {
	strcpy(str,"Invalid Time");
	return(str); }
if(gm->tm_hour>=12) {
    if(gm->tm_hour==12)
        hour=12;
    else
        hour=gm->tm_hour-12;
    strcpy(mer,"pm"); }
else {
    if(gm->tm_hour==0)
        hour=12;
    else
        hour=gm->tm_hour;
    strcpy(mer,"am"); }
sprintf(str,"%s %s %02d %4d %02d:%02d %s"
    ,wday[gm->tm_wday],mon[gm->tm_mon],gm->tm_mday,1900+gm->tm_year
    ,hour,gm->tm_min,mer);
return(str);
}


/****************************************************************************/
/* Converts when_t.zone into ASCII format									*/
/****************************************************************************/
char *zonestr(short zone)
{
	static char str[32];

switch((ushort)zone) {
	case 0: 	return("UT");
	case AST:	return("AST");
	case EST:	return("EST");
	case CST:	return("CST");
	case MST:	return("MST");
	case PST:	return("PST");
	case YST:	return("YST");
	case HST:	return("HST");
	case BST:	return("BST");
	case ADT:	return("ADT");
	case EDT:	return("EDT");
	case CDT:	return("CDT");
	case MDT:	return("MDT");
	case PDT:	return("PDT");
	case YDT:	return("YDT");
	case HDT:	return("HDT");
	case BDT:	return("BDT");
	case MID:	return("MID");
	case VAN:	return("VAN");
	case EDM:	return("EDM");
	case WIN:	return("WIN");
	case BOG:	return("BOG");
	case CAR:	return("CAR");
	case RIO:	return("RIO");
	case FER:	return("FER");
	case AZO:	return("AZO");
	case LON:	return("LON");
	case BER:	return("BER");
	case ATH:	return("ATH");
	case MOS:	return("MOS");
	case DUB:	return("DUB");
	case KAB:	return("KAB");
	case KAR:	return("KAR");
	case BOM:	return("BOM");
	case KAT:	return("KAT");
	case DHA:	return("DHA");
	case BAN:	return("BAN");
	case HON:	return("HON");
	case TOK:	return("TOK");
	case SYD:	return("SYD");
	case NOU:	return("NOU");
	case WEL:	return("WEL");
	}

sprintf(str,"%02d:%02u",zone/60,zone<0 ? (-zone)%60 : zone%60);
return(str);
}
			 

/****************************************************************************/
/* Displays message header information										*/
/****************************************************************************/
void viewmsgs(ulong start, ulong count)
{
	char when_written[128]
		,when_imported[128];
	int i;
	ulong l=0;
	smbmsg_t msg;
	idxrec_t idxrec;

if(!start)
	start=1;
fseek(smb.sid_fp,(start-1L)*sizeof(idxrec_t),SEEK_SET);
while(l<count) {
	if(!fread(&msg.idx,1,sizeof(idxrec_t),smb.sid_fp))
		break;
	i=smb_lockmsghdr(&smb,&msg);
	if(i) {
		printf("smb_lockmsghdr returned %d\n",i);
		break; }
	i=smb_getmsghdr(&smb,&msg);
	smb_unlockmsghdr(&smb,&msg);
	if(i) {
		printf("smb_getmsghdr returned %d\n",i);
		break; }

	sprintf(when_written,"%.24s %s"
		,timestr((time_t *)msg.hdr.when_written.time)
		,zonestr(msg.hdr.when_written.zone));
	sprintf(when_imported,"%.24s %s"
		,timestr((time_t *)msg.hdr.when_imported.time)
		,zonestr(msg.hdr.when_imported.zone));

	printf( "%-20.20s %s\n"
			"%-20.20s %s\n"
            "%-20.20s %s\n"
			"%-20.20s %04Xh\n"
			"%-20.20s %04Xh\n"
			"%-20.20s %u\n"
			"%-20.20s %04Xh\n"
			"%-20.20s %08lXh\n"
			"%-20.20s %08lXh\n"
			"%-20.20s %s\n"
			"%-20.20s %s\n"
			"%-20.20s %ld (%ld)\n"
			"%-20.20s %ld\n"
			"%-20.20s %ld\n"
			"%-20.20s %ld\n"
			"%-20.20s %s\n"
			"%-20.20s %06lXh\n"
			"%-20.20s %u\n",

		"subj",
		msg.subj,

		"from",
		msg.from,

		"to",
		msg.to,

		"type",
		msg.hdr.type,

		"version",
		msg.hdr.version,

		"length",
		msg.hdr.length,

		"attr",
		msg.hdr.attr,

		"auxattr",
		msg.hdr.auxattr,

		"netattr",
		msg.hdr.netattr,

		"when_written",
		when_written,

		"when_imported",
		when_imported,

		"number",
		msg.hdr.number,
		ftell(smb.sid_fp)/sizeof(idxrec_t),

		"thread_orig",
		msg.hdr.thread_orig,

		"thread_next",
		msg.hdr.thread_next,

		"thread_first",
		msg.hdr.thread_first,

		"reserved[16]",
		binstr(msg.hdr.reserved,16),

		"offset",
		msg.hdr.offset,

		"total_dfields",
		msg.hdr.total_dfields
		);
	for(i=0;i<msg.hdr.total_dfields;i++)
		printf("dfield[%02u].type      %02Xh\n"
			   "dfield[%02u].offset    %lu\n"
			   "dfield[%02u].length    %lu\n"
			   ,i,msg.dfield[i].type
			   ,i,msg.dfield[i].offset
			   ,i,msg.dfield[i].length);

	for(i=0;i<msg.total_hfields;i++)
		printf("hfield[%02u].type      %02Xh\n"
			   "hfield[%02u].length    %d\n"
			   "hfield[%02u]_dat       %s\n"
			   ,i,msg.hfield[i].type
			   ,i,msg.hfield[i].length
			   ,i,binstr(msg.hfield_dat[i],msg.hfield[i].length));

	if(msg.from_net.type)
		printf("from_net.type        %02Xh\n"
			   "from_net.addr        %s\n"
			,msg.from_net.type
			,msg.from_net.type==NET_FIDO
			? faddrtoa(*(fidoaddr_t *)msg.from_net.addr) : msg.from_net.addr);

	if(msg.to_net.type)
		printf("to_net.type          %02Xh\n"
			   "to_net.addr          %s\n"
			,msg.to_net.type
			,msg.to_net.type==NET_FIDO
			? faddrtoa(*(fidoaddr_t *)msg.to_net.addr) : msg.to_net.addr);

	if(msg.replyto_net.type)
		printf("replyto_net.type     %02Xh\n"
			   "replyto_net.addr     %s\n"
			,msg.replyto_net.type
			,msg.replyto_net.type==NET_FIDO
			? faddrtoa(*(fidoaddr_t *)msg.replyto_net.addr)
				: msg.replyto_net.addr);

	printf("from_agent           %02Xh\n"
		   "to_agent             %02Xh\n"
		   "replyto_agent        %02Xh\n"
		   ,msg.from_agent
		   ,msg.to_agent
		   ,msg.replyto_agent);

	printf("\n");
	smb_freemsgmem(&msg);
	l++; }
}

/****************************************************************************/
/* Maintain message base - deletes messages older than max age (in days)	*/
/* or messages that exceed maximum											*/
/****************************************************************************/
void maint(void)
{
	int i;
	ulong l,m,n,f,flagged=0;
	time_t now;
	smbmsg_t msg;
	idxrec_t HUGE16 *idx;

printf("Maintaining %s\r\n",smb.file);
now=time(NULL);
i=smb_locksmbhdr(&smb);
if(i) {
	printf("smb_locksmbhdr returned %d\n",i);
	return; }
i=smb_getstatus(&smb);
if(i) {
	smb_unlocksmbhdr(&smb);
	printf("smb_getstatus returned %d\n",i);
	return; }
if(!smb.status.total_msgs) {
	smb_unlocksmbhdr(&smb);
	printf("Empty\n");
	return; }
printf("Loading index...\n");
if((idx=(idxrec_t *)LMALLOC(sizeof(idxrec_t)*smb.status.total_msgs))
	==NULL) {
	smb_unlocksmbhdr(&smb);
	printf("can't allocate %lu bytes of memory\n"
		,sizeof(idxrec_t)*smb.status.total_msgs);
	return; }
fseek(smb.sid_fp,0L,SEEK_SET);
for(l=0;l<smb.status.total_msgs;l++) {
	printf("%lu of %lu\r"
		,l+1,smb.status.total_msgs);
	if(!fread(&idx[l],1,sizeof(idxrec_t),smb.sid_fp))
		break; }
printf("\nDone.\n\n");

printf("Scanning for pre-flagged messages...\n");
for(m=0;m<l;m++) {
	printf("\r%2u%%",m ? (long)(100.0/((float)l/m)) : 0);
	if(idx[m].attr&MSG_DELETE)
		flagged++; }
printf("\r100%% (%lu pre-flagged for deletion)\n",flagged);

if(smb.status.max_age) {
	printf("Scanning for messages more than %u days old...\n"
		,smb.status.max_age);
	for(m=f=0;m<l;m++) {
		printf("\r%2u%%",m ? (long)(100.0/((float)l/m)) : 0);
		if(idx[m].attr&(MSG_PERMANENT|MSG_DELETE))
			continue;
		if(now>idx[m].time && (now-idx[m].time)/(24L*60L*60L)
			>smb.status.max_age) {
			f++;
			flagged++;
			idx[m].attr|=MSG_DELETE; } }  /* mark for deletion */
	printf("\r100%% (%lu flagged for deletion)\n",f); }

printf("Scanning for read messages to be killed...\n");
for(m=f=0;m<l;m++) {
	printf("\r%2u%%",m ? (long)(100.0/((float)l/m)) : 0);
	if(idx[m].attr&(MSG_PERMANENT|MSG_DELETE))
		continue;
	if((idx[m].attr&(MSG_READ|MSG_KILLREAD))==(MSG_READ|MSG_KILLREAD)) {
		f++;
		flagged++;
		idx[m].attr|=MSG_DELETE; } }
printf("\r100%% (%lu flagged for deletion)\n",f);

if(l-flagged>smb.status.max_msgs) {
	printf("Flagging excess messages for deletion...\n");
	for(m=n=0,f=flagged;l-flagged>smb.status.max_msgs && m<l;m++) {
		if(idx[m].attr&(MSG_PERMANENT|MSG_DELETE))
			continue;
		printf("%lu of %lu\r",++n,(l-f)-smb.status.max_msgs);
		flagged++;
		idx[m].attr|=MSG_DELETE; }			/* mark for deletion */
	printf("\nDone.\n\n"); }

if(!flagged) {				/* No messages to delete */
	LFREE(idx);
	smb_unlocksmbhdr(&smb);
	return; }

if(!(mode&NOANALYSIS)) {

	printf("Freeing allocated header and data blocks for deleted messages...\n");
	if(!(smb.status.attr&SMB_HYPERALLOC)) {
		i=smb_open_da(&smb);
		if(i) {
			smb_unlocksmbhdr(&smb);
			printf("smb_open_da returned %d\n",i);
			exit(1); }
		i=smb_open_ha(&smb);
		if(i) {
			smb_unlocksmbhdr(&smb);
			printf("smb_open_ha returned %d\n",i);
			exit(1); } }

	for(m=n=0;m<l;m++) {
		if(idx[m].attr&MSG_DELETE) {
			printf("%lu of %lu\r",++n,flagged);
			msg.idx=idx[m];
			msg.hdr.number=msg.idx.number;
			if((i=smb_getmsgidx(&smb,&msg))!=0) {
				printf("\nsmb_getmsgidx returned %d\n",i);
				continue; }
			i=smb_lockmsghdr(&smb,&msg);
			if(i) {
				printf("\nsmb_lockmsghdr returned %d\n",i);
				break; }
			if((i=smb_getmsghdr(&smb,&msg))!=0) {
				smb_unlockmsghdr(&smb,&msg);
				printf("\nsmb_getmsghdr returned %d\n",i);
				break; }
			msg.hdr.attr|=MSG_DELETE;			/* mark header as deleted */
			if((i=smb_putmsg(&smb,&msg))!=0) {
				smb_freemsgmem(&msg);
				smb_unlockmsghdr(&smb,&msg);
				printf("\nsmb_putmsg returned %d\n",i);
				break; }
			smb_unlockmsghdr(&smb,&msg);
			if((i=smb_freemsg(&smb,&msg))!=0) {
				smb_freemsgmem(&msg);
				printf("\nsmb_freemsg returned %d\n",i);
				break; }
			smb_freemsgmem(&msg); } }
	if(!(smb.status.attr&SMB_HYPERALLOC)) {
		smb_close_ha(&smb);
		smb_close_da(&smb); }
	printf("\nDone.\n\n"); }

printf("Re-writing index...\n");
rewind(smb.sid_fp);
if(chsize(fileno(smb.sid_fp),0L))
	printf("chsize failed!\n");
for(m=n=0;m<l;m++) {
	if(idx[m].attr&MSG_DELETE)
		continue;
	printf("%lu of %lu\r",++n,l-flagged);
	fwrite(&idx[m],sizeof(idxrec_t),1,smb.sid_fp); }
printf("\nDone.\n\n");
fflush(smb.sid_fp);

LFREE(idx);
smb.status.total_msgs-=flagged;
smb_putstatus(&smb);
smb_unlocksmbhdr(&smb);
}


typedef struct {
	ulong old,new;
	} datoffset_t;

/****************************************************************************/
/* Removes all unused blocks from SDT and SHD files 						*/
/****************************************************************************/
void packmsgs(ulong packable)
{
	uchar str[128],buf[SDT_BLOCK_LEN],ch,fname[128],tmpfname[128];
	int i,file,size;
	ulong l,m,n,datoffsets=0,length,total,now;
	FILE *tmp_sdt,*tmp_shd,*tmp_sid;
	smbhdr_t	hdr;
	smbmsg_t	msg;
	datoffset_t *datoffset=NULL;

now=time(NULL);
printf("Packing %s\n",smb.file);
i=smb_locksmbhdr(&smb);
if(i) {
	printf("smb_locksmbhdr returned %d\n",i);
	return; }
i=smb_getstatus(&smb);
if(i) {
	smb_unlocksmbhdr(&smb);
	printf("smb_getstatus returned %d\n",i);
    return; }

if(!(smb.status.attr&SMB_HYPERALLOC)) {
	i=smb_open_ha(&smb);
	if(i) {
		smb_unlocksmbhdr(&smb);
		printf("smb_open_ha returned %d\n",i);
		return; }
	i=smb_open_da(&smb);
	if(i) {
		smb_unlocksmbhdr(&smb);
		smb_close_ha(&smb);
		printf("smb_open_da returned %d\n",i);
		return; } }

if(!smb.status.total_msgs) {
    printf("Empty\n");
	rewind(smb.shd_fp);
	chsize(fileno(smb.shd_fp),smb.status.header_offset);
	rewind(smb.sdt_fp);
	chsize(fileno(smb.sdt_fp),0L);
	rewind(smb.sid_fp);
	chsize(fileno(smb.sid_fp),0L);
	if(!(smb.status.attr&SMB_HYPERALLOC)) {
		rewind(smb.sha_fp);
		chsize(fileno(smb.sha_fp),0L);
		rewind(smb.sda_fp);
		chsize(fileno(smb.sda_fp),0L);
		smb_close_ha(&smb);
		smb_close_da(&smb); }
	smb_unlocksmbhdr(&smb);
    return; }


if(!(smb.status.attr&SMB_HYPERALLOC) && !(mode&NOANALYSIS)) {
    printf("Analyzing data blocks...\n");

	length=filelength(fileno(smb.sda_fp));

	fseek(smb.sda_fp,0L,SEEK_SET);
    for(l=m=0;l<length;l+=2) {
        printf("\r%2u%%  ",l ? (long)(100.0/((float)length/l)) : 0);
        i=0;
		if(!fread(&i,2,1,smb.sda_fp))
            break;
        if(!i)
            m++; }

    printf("\rAnalyzing header blocks...\n");

	length=filelength(fileno(smb.sha_fp));

	fseek(smb.sha_fp,0L,SEEK_SET);
    for(l=n=0;l<length;l++) {
        printf("\r%2u%%  ",l ? (long)(100.0/((float)length/l)) : 0);
		ch=0;
		if(!fread(&ch,1,1,smb.sha_fp))
            break;
		if(!ch)
            n++; }

    if(!m && !n) {
		printf("\rAlready compressed.\n\n");
		smb_close_ha(&smb);
		smb_close_da(&smb);
		smb_unlocksmbhdr(&smb);
        return; }

	if(packable && (m*SDT_BLOCK_LEN)+(n*SHD_BLOCK_LEN)<packable*1024L) {
		printf("\rLess than %luk compressable bytes.\n\n",packable);
		smb_close_ha(&smb);
		smb_close_da(&smb);
		smb_unlocksmbhdr(&smb);
		return; }

	printf("\rCompressing %lu data blocks (%lu bytes)\n"
			 "        and %lu header blocks (%lu bytes)\n"
			  ,m,m*SDT_BLOCK_LEN,n,n*SHD_BLOCK_LEN); }

if(!(smb.status.attr&SMB_HYPERALLOC)) {
	rewind(smb.sha_fp);
	chsize(fileno(smb.sha_fp),0L);		/* Reset both allocation tables */
	rewind(smb.sda_fp);
	chsize(fileno(smb.sda_fp),0L); }

if(smb.status.attr&SMB_HYPERALLOC && !(mode&NOANALYSIS)) {
	printf("Analyzing %s\n",smb.file);

	length=filelength(fileno(smb.shd_fp));
	m=n=0;
	for(l=smb.status.header_offset;l<length;l+=size) {
		printf("\r%2u%%  ",(long)(100.0/((float)length/l)));
		msg.idx.offset=l;
		if((i=smb_lockmsghdr(&smb,&msg))!=0) {
			printf("\n(%06lX) smb_lockmsghdr returned %d\n",l,i);
			size=SHD_BLOCK_LEN;
			continue; }
		if((i=smb_getmsghdr(&smb,&msg))!=0) {
			smb_unlockmsghdr(&smb,&msg);
			m++;
			size=SHD_BLOCK_LEN;
			continue; }
		smb_unlockmsghdr(&smb,&msg);
		if(msg.hdr.attr&MSG_DELETE) {
			m+=smb_hdrblocks(msg.hdr.length);
			total=0;
			for(i=0;i<msg.hdr.total_dfields;i++)
				total+=msg.dfield[i].length;
			n+=smb_datblocks(total); }
		size=smb_getmsghdrlen(&msg);
		if(size<1) size=SHD_BLOCK_LEN;
		while(size%SHD_BLOCK_LEN)
			size++;
		smb_freemsgmem(&msg); }


    if(!m && !n) {
		printf("\rAlready compressed.\n\n");
		smb_unlocksmbhdr(&smb);
        return; }

	if(packable && (n*SDT_BLOCK_LEN)+(m*SHD_BLOCK_LEN)<packable*1024L) {
		printf("\rLess than %luk compressable bytes.\n\n",packable);
		smb_unlocksmbhdr(&smb);
		return; }

	printf("\rCompressing %lu data blocks (%lu bytes)\n"
			 "        and %lu header blocks (%lu bytes)\n"
			  ,n,n*SDT_BLOCK_LEN,m,m*SHD_BLOCK_LEN); }

sprintf(fname,"%s.SD$",smb.file);
tmp_sdt=fopen(fname,"wb");
sprintf(fname,"%s.SH$",smb.file);
tmp_shd=fopen(fname,"wb");
sprintf(fname,"%s.SI$",smb.file);
tmp_sid=fopen(fname,"wb");
if(!tmp_sdt || !tmp_shd || !tmp_sid) {
	smb_unlocksmbhdr(&smb);
	if(!(smb.status.attr&SMB_HYPERALLOC)) {
		smb_close_ha(&smb);
		smb_close_da(&smb); }
	printf("error opening temp file\n");
	return; }
setvbuf(tmp_sdt,NULL,_IOFBF,2*1024);
setvbuf(tmp_shd,NULL,_IOFBF,2*1024);
setvbuf(tmp_sid,NULL,_IOFBF,2*1024);
if(!(smb.status.attr&SMB_HYPERALLOC)
	&& (datoffset=(datoffset_t *)LMALLOC(sizeof(datoffset_t)*smb.status.total_msgs))
	==NULL) {
	smb_unlocksmbhdr(&smb);
	smb_close_ha(&smb);
	smb_close_da(&smb);
	fclose(tmp_sdt);
	fclose(tmp_shd);
	fclose(tmp_sid);
	printf("error allocating mem\n");
    return; }
fseek(smb.shd_fp,0L,SEEK_SET);
fread(&hdr,1,sizeof(smbhdr_t),smb.shd_fp);
fwrite(&hdr,1,sizeof(smbhdr_t),tmp_shd);
fwrite(&(smb.status),1,sizeof(smbstatus_t),tmp_shd);
for(l=sizeof(smbhdr_t)+sizeof(smbstatus_t);l<smb.status.header_offset;l++) {
	fread(&ch,1,1,smb.shd_fp);			/* copy additional base header records */
	fwrite(&ch,1,1,tmp_shd); }
fseek(smb.sid_fp,0L,SEEK_SET);
total=0;
for(l=0;l<smb.status.total_msgs;l++) {
	printf("%lu of %lu\r",l+1,smb.status.total_msgs);
	if(!fread(&msg.idx,1,sizeof(idxrec_t),smb.sid_fp))
		break;
	if(msg.idx.attr&MSG_DELETE) {
		printf("\nDeleted index.\n");
		continue; }
	i=smb_lockmsghdr(&smb,&msg);
	if(i) {
		printf("\nsmb_lockmsghdr returned %d\n",i);
		continue; }
	i=smb_getmsghdr(&smb,&msg);
	smb_unlockmsghdr(&smb,&msg);
	if(i) {
		printf("\nsmb_getmsghdr returned %d\n",i);
		continue; }
	if(msg.hdr.attr&MSG_DELETE) {
		printf("\nDeleted header.\n");
		smb_freemsgmem(&msg);
		continue; }
	if(msg.expiration.time && msg.expiration.time<=now) {
		printf("\nExpired message.\n");
		smb_freemsgmem(&msg);
		continue; }
	for(m=0;m<datoffsets;m++)
		if(msg.hdr.offset==datoffset[m].old)
			break;
	if(m<datoffsets) {				/* another index pointed to this data */
		printf("duplicate index\n");
		msg.hdr.offset=datoffset[m].new;
		smb_incdat(&smb,datoffset[m].new,smb_getmsgdatlen(&msg),1); }
	else {

		if(!(smb.status.attr&SMB_HYPERALLOC))
			datoffset[datoffsets].old=msg.hdr.offset;

		fseek(smb.sdt_fp,msg.hdr.offset,SEEK_SET);

		m=smb_getmsgdatlen(&msg);
        if(m>16L*1024L*1024L) {
            printf("\nInvalid data length (%lu)\n",m);
            continue; }

		if(!(smb.status.attr&SMB_HYPERALLOC)) {
			datoffset[datoffsets].new=msg.hdr.offset
				=smb_fallocdat(&smb,m,1);
			datoffsets++;
			fseek(tmp_sdt,msg.hdr.offset,SEEK_SET); }
		else {
			fseek(tmp_sdt,0L,SEEK_END);
			msg.hdr.offset=ftell(tmp_sdt); }

		/* Actually copy the data */

		n=smb_datblocks(m);
		for(m=0;m<n;m++) {
			fread(buf,1,SDT_BLOCK_LEN,smb.sdt_fp);
			if(!m && *(ushort *)buf!=XLAT_NONE && *(ushort *)buf!=XLAT_LZH) {
				printf("\nUnsupported translation type (%04X)\n"
					,*(ushort *)buf);
				break; }
			fwrite(buf,1,SDT_BLOCK_LEN,tmp_sdt); }
		if(m<n)
			continue; }

	/* Write the new index entry */
	length=smb_getmsghdrlen(&msg);
	if(smb.status.attr&SMB_HYPERALLOC)
		msg.idx.offset=ftell(tmp_shd);
	else
		msg.idx.offset=smb_fallochdr(&smb,length)+smb.status.header_offset;
	msg.idx.number=msg.hdr.number;
	msg.idx.attr=msg.hdr.attr;
	msg.idx.time=msg.hdr.when_imported.time;
	sprintf(str,"%.128s",msg.subj);
	strlwr(str);
	remove_re(str);
	msg.idx.subj=crc16(str);
	if(smb.status.attr&SMB_EMAIL) {
		if(msg.to_ext)
			msg.idx.to=atoi(msg.to_ext);
		else
			msg.idx.to=0;
		if(msg.from_ext)
			msg.idx.from=atoi(msg.from_ext);
		else
			msg.idx.from=0; }
	else {
		sprintf(str,"%.128s",msg.to);
		strlwr(str);
		msg.idx.to=crc16(str);
		sprintf(str,"%.128s",msg.from);
		strlwr(str);
		msg.idx.from=crc16(str); }
	fwrite(&msg.idx,1,sizeof(idxrec_t),tmp_sid);

	/* Write the new header entry */
	fseek(tmp_shd,msg.idx.offset,SEEK_SET);
	fwrite(&msg.hdr,1,sizeof(msghdr_t),tmp_shd);
	for(n=0;n<msg.hdr.total_dfields;n++)
		fwrite(&msg.dfield[n],1,sizeof(dfield_t),tmp_shd);
	for(n=0;n<msg.total_hfields;n++) {
		fwrite(&msg.hfield[n],1,sizeof(hfield_t),tmp_shd);
		fwrite(msg.hfield_dat[n],1,msg.hfield[n].length,tmp_shd); }
	while(length%SHD_BLOCK_LEN) {	/* pad with NULLs */
		fputc(0,tmp_shd);
		length++; }
	total++;
	smb_freemsgmem(&msg); }

if(datoffset)
	LFREE(datoffset);
if(!(smb.status.attr&SMB_HYPERALLOC)) {
	smb_close_ha(&smb);
	smb_close_da(&smb); }

/* Change *.SH$ into *.SHD */
fclose(smb.shd_fp);
fclose(tmp_shd);
sprintf(fname,"%s.SHD",smb.file);
remove(fname);
sprintf(tmpfname,"%s.SH$",smb.file);
rename(tmpfname,fname);

/* Change *.SD$ into *.SDT */
fclose(smb.sdt_fp);
fclose(tmp_sdt);
sprintf(fname,"%s.SDT",smb.file);
remove(fname);
sprintf(tmpfname,"%s.SD$",smb.file);
rename(tmpfname,fname);

/* Change *.SI$ into *.SID */
fclose(smb.sid_fp);
fclose(tmp_sid);
sprintf(fname,"%s.SID",smb.file);
remove(fname);
sprintf(tmpfname,"%s.SI$",smb.file);
rename(tmpfname,fname);

if((i=smb_open(&smb))!=0) {
	printf("Error %d reopening %s\n",i,smb.file);
	return; }

smb.status.total_msgs=total;
if((i=smb_putstatus(&smb))!=0)
	printf("\nsmb_putstatus returned %d\n",i);
printf("\nDone.\n\n");
}


/****************************************************************************/
/* Read messages in message base											*/
/****************************************************************************/
void readmsgs(ulong start)
{
	char	str[128],HUGE16 *inbuf,*outbuf;
	int 	i,ch,done=0,domsg=1,lzh;
	ushort	xlat;
	ulong	l,count,outlen;
	smbmsg_t msg;

if(start)
	msg.offset=start-1;
else
	msg.offset=0;
while(!done) {
	if(domsg) {
		fseek(smb.sid_fp,msg.offset*sizeof(idxrec_t),SEEK_SET);
		if(!fread(&msg.idx,1,sizeof(idxrec_t),smb.sid_fp))
			break;
		i=smb_lockmsghdr(&smb,&msg);
		if(i) {
			printf("smb_lockmsghdr returned %d\n",i);
			break; }
		i=smb_getmsghdr(&smb,&msg);
		if(i) {
			printf("smb_getmsghdr returned %d\n",i);
			break; }

		printf("\n%lu (%lu)\n",msg.hdr.number,msg.offset+1);
		printf("Subj : %s\n",msg.subj);
		printf("To   : %s",msg.to);
		if(msg.to_net.type)
			printf(" (%s)",msg.to_net.type==NET_FIDO
				? faddrtoa(*(fidoaddr_t *)msg.to_net.addr) : msg.to_net.addr);
		printf("\nFrom : %s",msg.from);
		if(msg.from_net.type)
			printf(" (%s)",msg.from_net.type==NET_FIDO
				? faddrtoa(*(fidoaddr_t *)msg.from_net.addr)
					: msg.from_net.addr);
#if 1
		printf("\nDate : %.24s %s"
			,timestr((time_t *)msg.hdr.when_written.time)
			,zonestr(msg.hdr.when_written.zone));
#endif
		printf("\n\n");
#if 0
		for(i=0;i<msg.hdr.total_dfields;i++)
			switch(msg.dfield[i].type) {
				case TEXT_BODY:
				case TEXT_TAIL:
					fseek(smb.sdt_fp,msg.hdr.offset+msg.dfield[i].offset
						,SEEK_SET);
					fread(&xlat,2,1,smb.sdt_fp);
					l=2;
					lzh=0;
					while(xlat!=XLAT_NONE) {
						if(xlat==XLAT_LZH)
							lzh=1;
						fread(&xlat,2,1,smb.sdt_fp);
						l+=2; }
					if(lzh) {
						if((inbuf=(char *)LMALLOC(msg.dfield[i].length))
							==NULL) {
							printf("Malloc error of %lu\n"
								,msg.dfield[i].length);
							exit(1); }
						fread(inbuf,msg.dfield[i].length-l,1,smb.sdt_fp);
						outlen=*(long *)inbuf;
						if((outbuf=(char *)LMALLOC(outlen))==NULL) {
							printf("Malloc error of lzh %lu\n"
								,outlen);
							exit(1); }
						lzh_decode(inbuf,msg.dfield[i].length-l,outbuf);
						LFREE(inbuf);
						for(l=0;l<outlen;l++)
							putchar(outbuf[l]);
						LFREE(outbuf); }
					else {
						for(;l<msg.dfield[i].length;l++) {
							ch=fgetc(smb.sdt_fp);
							if(ch)
								putchar(ch); } }
					printf("\n");
					break; }
#else
		if((inbuf=smb_getmsgtxt(&smb,&msg,GETMSGTXT_TAILS))!=NULL) {
			printf("%s",inbuf);
			FREE(inbuf); }
#endif
		i=smb_unlockmsghdr(&smb,&msg);
        if(i) {
			printf("smb_unlockmsghdr returned %d\n",i);
			break; }
		smb_freemsgmem(&msg); }
	domsg=1;
	printf("\nReading %s (?=Menu): ",smb.file);
	switch(toupper(getch())) {
		case '?':
			printf("\n"
				   "\n"
				   "(R)e-read current message\n"
				   "(L)ist messages\n"
				   "(T)en more titles\n"
				   "(V)iew message headers\n"
				   "(Q)uit\n"
				   "(+/-) Forward/Backward\n"
				   "\n");
			domsg=0;
			break;
		case 'Q':
			printf("Quit\n");
			done=1;
			break;
		case 'R':
			printf("Re-read\n");
			break;
		case '-':
			printf("Backwards\n");
			if(msg.offset)
				msg.offset--;
			break;
		case 'T':
			printf("Ten titles\n");
			listmsgs(msg.offset+2,10);
			msg.offset+=10;
			domsg=0;
			break;
		case 'L':
			printf("List messages\n");
			listmsgs(1,-1);
			domsg=0;
			break;
		case 'V':
			printf("View message headers\n");
			viewmsgs(1,-1);
			domsg=0;
			break;
		case CR:
		case '+':
			printf("Next\n");
			msg.offset++;
			break; } }
}

/***************/
/* Entry point */
/***************/
int main(int argc, char **argv)
{
	char cmd[128]="",*p,*s;
	int i,j,x,y;
	ulong l;

#ifdef __TURBOC__
	timezone=0; 		/* Fix for Borland C++ EST default */
	daylight=0; 		/* Fix for Borland C++ EDT default */
#elif defined(__WATCOMC__)
	putenv("TZ=UCT0");  /* Fix for Watcom C++ EDT default */
#endif
setvbuf(stdout,0,_IONBF,0);

smb.file[0]=0;
printf("\nSMBUTIL Version %s (%s) SMBLIB %s ú Synchronet Message Base "\
	"Utility\n\n"
	,SMBUTIL_VER
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
	,SMBLIB_VERSION
    );
for(x=1;x<argc;x++) {
	if(argv[x][0]=='/') {
		for(j=1;argv[x][j];j++)
			switch(toupper(argv[x][j])) {
				case 'A':
					mode|=NOANALYSIS;
					break;
				case 'Z':
					if(isdigit(argv[x][j+1]))
						tzone=atoi(argv[x]+j+1);
					else if(!stricmp(argv[x]+j+1,"EST"))
						tzone=EST;
					else if(!stricmp(argv[x]+j+1,"EDT"))
						tzone=EDT;
					else if(!stricmp(argv[x]+j+1,"CST"))
						tzone=CST;
					else if(!stricmp(argv[x]+j+1,"CDT"))
						tzone=CDT;
					else if(!stricmp(argv[x]+j+1,"MST"))
						tzone=MST;
					else if(!stricmp(argv[x]+j+1,"MDT"))
						tzone=MDT;
					else if(!stricmp(argv[x]+j+1,"PST"))
						tzone=PST;
					else if(!stricmp(argv[x]+j+1,"PDT"))
						tzone=PDT;
					j=strlen(argv[x])-1;
					break;
				default:
					printf("\nUnknown opt '%c'\n",argv[x][j]);
				case '?':
					printf("%s",usage);
					exit(1);
					break; } }
	else {
		if(!cmd[0])
			strcpy(cmd,argv[x]);
		else {
			sprintf(smb.file,"%.64s",argv[x]);
			p=strrchr(smb.file,'.');
			s=strrchr(smb.file,'\\');
			if(p>s) *p=0;
			strupr(smb.file);
			smb.retry_time=30;
			printf("Opening %s\r\n",smb.file);
			if((i=smb_open(&smb))!=0) {
				printf("error %d opening %s message base\n",i,smb.file);
				exit(1); }
			if(!filelength(fileno(smb.shd_fp))) {
				printf("Empty\n");
				smb_close(&smb);
				continue; }
			for(y=0;cmd[y];y++)
				switch(toupper(cmd[y])) {
					case 'I':
					case 'E':
					case 'N':
						strcpy(filein,cmd+1);
						i=smb_locksmbhdr(&smb);
						if(i) {
							printf("smb_locksmbhdr returned %d\n",i);
							return(1); }
						postmsg(toupper(cmd[y]));
						y=strlen(cmd)-1;
						break;
					case 'S':
						showstatus();
						break;
					case 'C':
						config();
						break;
					case 'L':
						listmsgs(atol(cmd+1),-1L);
						y=strlen(cmd)-1;
						break;
					case 'P':
						packmsgs(atol(cmd+y+1));
						y=strlen(cmd)-1;
						break;
					case 'R':
						readmsgs(atol(cmd+1));
						y=strlen(cmd)-1;
						break;
					case 'V':
						viewmsgs(atol(cmd+1),-1L);
						y=strlen(cmd)-1;
						break;
					case 'M':
						maint();
						break;
					default:
						printf("%s",usage);
						break; }
			smb_close(&smb); } } }
if(!cmd[0])
	printf("%s",usage);
return(0);
}
