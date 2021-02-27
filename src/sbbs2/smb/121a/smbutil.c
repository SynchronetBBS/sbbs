/* SMBUTIL.C */

/* Synchronet Message Base Utility */

#define SMBUTIL_VER "1.24"

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
"       k[n] = kill (delete) n msgs\n"
"       i<f> = import from text file f\n"
"       s    = display msg base status\n"
"       c    = change msg base status\n"
"       m    = maintain msg base - delete old msgs and msgs over max\n"
"       p[k] = pack msg base (k specifies minimum packable Kbytes)\n"
"opts:\n"
"       a    = always pack msg base (disable compression analysis)\n"
"       z[n] = set time zone (n=min +/- from UT or 'EST','EDT','CST',etc)\n"
;

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


/****************************************************************************/
/* Adds a new message to the message base									*/
/****************************************************************************/
void postmsg(smbstatus_t status)
{
	char	str[128],buf[SDT_BLOCK_LEN];
	ushort	xlat;
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
if(!(status.attr&SMB_HYPERALLOC)) {
	i=smb_open_da(10);
	if(i) {
		printf("smb_open_da returned %d\n",i);
		exit(1); }
	offset=smb_allocdat(length,1);
	fclose(sda_fp); }
else
	offset=smb_hallocdat();

if((file=open(filein,O_RDONLY|O_BINARY))==-1
	|| (instream=fdopen(file,"rb"))==NULL) {
	printf("Error opening %s for read\n",filein);
	smb_freemsgdat(offset,length,1);
	exit(1); }
setvbuf(instream,NULL,_IOFBF,32*1024);
fseek(sdt_fp,offset,SEEK_SET);
xlat=XLAT_NONE;
fwrite(&xlat,2,1,sdt_fp);
k=SDT_BLOCK_LEN-2;
while(!feof(instream)) {
	memset(buf,0,k);
	j=fread(buf,1,k,instream);
	if(status.max_crcs)
		for(i=0;i<j;i++)
			crc=ucrc32(buf[i],crc);
	fwrite(buf,k,1,sdt_fp);
	k=SDT_BLOCK_LEN; }
fflush(sdt_fp);
fclose(instream);
crc=~crc;

memset(&msg,0,sizeof(smbmsg_t));
memcpy(msg.hdr.id,"SHD\x1a",4);
msg.hdr.version=SMB_VERSION;
msg.hdr.when_written.time=time(NULL);
msg.hdr.when_written.zone=tzone;
msg.hdr.when_imported=msg.hdr.when_written;

if(status.max_crcs) {
	i=smb_addcrc(status.max_crcs,crc,10);
	if(i) {
		printf("smb_addcrc returned %d\n",i);
		smb_freemsgdat(offset,length,1);
		exit(1); } }

msg.hdr.offset=offset;

printf("To User Name: ");
gets(str);
i=smb_hfield(&msg,RECIPIENT,strlen(str),str);
if(i) {
	printf("smb_hfield returned %d\n",i);
	smb_freemsgdat(offset,length,1);
	exit(1); }
if(status.attr&SMB_EMAIL) {
	printf("To User Number: ");
	gets(str);
	i=smb_hfield(&msg,RECIPIENTEXT,strlen(str),str);
	if(i) {
		printf("smb_hfield returned %d\n",i);
		smb_freemsgdat(offset,length,1);
		exit(1); }
	msg.idx.to=atoi(str); }
else {
	strlwr(str);
	msg.idx.to=crc16(str); }

printf("From User Name: ");
gets(str);
i=smb_hfield(&msg,SENDER,strlen(str),str);
if(i) {
	printf("smb_hfield returned %d\n",i);
	smb_freemsgdat(offset,length,1);
    exit(1); }
if(status.attr&SMB_EMAIL) {
	printf("From User Number: ");
	gets(str);
	i=smb_hfield(&msg,SENDEREXT,strlen(str),str);
	if(i) {
		printf("smb_hfield returned %d\n",i);
		smb_freemsgdat(offset,length,1);
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
	smb_freemsgdat(offset,length,1);
    exit(1); }
strlwr(str);
msg.idx.subj=crc16(str);

i=smb_dfield(&msg,TEXT_BODY,length);
if(i) {
	printf("smb_dfield returned %d\n",i);
	smb_freemsgdat(offset,length,1);
	exit(1); }

i=smb_addmsghdr(&msg,&status,status.attr&SMB_HYPERALLOC,10);

if(i) {
	printf("smb_addmsghdr returned %d\n",i);
	smb_freemsgdat(offset,length,1);
	exit(1); }
smb_freemsgmem(msg);

}

/****************************************************************************/
/* Shows the message base header											*/
/****************************************************************************/
void showstatus()
{
	int i;
	smbstatus_t status;

i=smb_locksmbhdr(10);
if(i) {
	printf("smb_locksmbhdr returned %d\n",i);
	return; }
i=smb_getstatus(&status);
smb_unlocksmbhdr();
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
	   ,status.last_msg
	   ,status.total_msgs
	   ,status.header_offset
	   ,status.max_crcs
	   ,status.max_msgs
	   ,status.max_age
	   ,status.attr
	   );
}

/****************************************************************************/
/* Configure message base header											*/
/****************************************************************************/
void config()
{
	char max_msgs[128],max_crcs[128],max_age[128],header_offset[128],attr[128];
	int i;
	smbstatus_t status;

i=smb_locksmbhdr(10);
if(i) {
	printf("smb_locksmbhdr returned %d\n",i);
	return; }
i=smb_getstatus(&status);
smb_unlocksmbhdr();
if(i) {
	printf("smb_getstatus returned %d\n",i);
	return; }
printf("Header offset =%-5lu  New value (CR=No Change): ",status.header_offset);
gets(header_offset);
printf("Max msgs      =%-5lu  New value (CR=No Change): ",status.max_msgs);
gets(max_msgs);
printf("Max crcs      =%-5lu  New value (CR=No Change): ",status.max_crcs);
gets(max_crcs);
printf("Max age       =%-5u  New value (CR=No Change): ",status.max_age);
gets(max_age);
printf("Attributes    =%-5u  New value (CR=No Change): ",status.attr);
gets(attr);
i=smb_locksmbhdr(10);
if(i) {
	printf("smb_locksmbhdr returned %d\n",i);
	return; }
i=smb_getstatus(&status);
if(i) {
	printf("smb_getstatus returned %d\n",i);
	smb_unlocksmbhdr();
    return; }
if(isdigit(max_msgs[0]))
	status.max_msgs=atol(max_msgs);
if(isdigit(max_crcs[0]))
	status.max_crcs=atol(max_crcs);
if(isdigit(max_age[0]))
	status.max_age=atoi(max_age);
if(isdigit(header_offset[0]))
	status.header_offset=atol(header_offset);
if(isdigit(attr[0]))
	status.attr=atoi(attr);
i=smb_putstatus(status);
smb_unlocksmbhdr();
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
fseek(sid_fp,(start-1L)*sizeof(idxrec_t),SEEK_SET);
while(l<count) {
	if(!fread(&msg.idx,1,sizeof(idxrec_t),sid_fp))
		break;
	i=smb_lockmsghdr(msg,10);
	if(i) {
		printf("smb_lockmsghdr returned %d\n",i);
		break; }
	i=smb_getmsghdr(&msg);
	smb_unlockmsghdr(msg);
	if(i) {
		printf("smb_getmsghdr returned %d\n",i);
		break; }
	printf("%4lu %-25.25s %-25.25s %.20s\n"
		,msg.hdr.number,msg.from,msg.to,msg.subj);
	smb_freemsgmem(msg);
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
/* Converts when_t.zone into ASCII format									*/
/****************************************************************************/
char *zonestr(short zone)
{
	static char str[32];

switch(zone) {
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

sprintf(str,"%02d:%02d",zone/60,zone<0 ? (-zone)%60 : zone%60);
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
fseek(sid_fp,(start-1L)*sizeof(idxrec_t),SEEK_SET);
while(l<count) {
	if(!fread(&msg.idx,1,sizeof(idxrec_t),sid_fp))
		break;
	i=smb_lockmsghdr(msg,10);
	if(i) {
		printf("smb_lockmsghdr returned %d\n",i);
		break; }
	i=smb_getmsghdr(&msg);
	smb_unlockmsghdr(msg);
	if(i) {
		printf("smb_getmsghdr returned %d\n",i);
		break; }

	sprintf(when_written,"%.24s %s"
		,ctime((time_t *)&msg.hdr.when_written.time)
		,zonestr(msg.hdr.when_written.zone));
	sprintf(when_imported,"%.24s %s"
		,ctime((time_t *)&msg.hdr.when_imported.time)
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
		ftell(sid_fp)/sizeof(idxrec_t),

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
	smb_freemsgmem(msg);
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
	smbstatus_t status;
	smbmsg_t msg;
	idxrec_t HUGE16 *idx;

printf("Maintaining %s\r\n",smb_file);
now=time(NULL);
i=smb_locksmbhdr(10);
if(i) {
	printf("smb_locksmbhdr returned %d\n",i);
	return; }
i=smb_getstatus(&status);
if(i) {
	smb_unlocksmbhdr();
	printf("smb_getstatus returned %d\n",i);
	return; }
if(!status.total_msgs) {
	smb_unlocksmbhdr();
	printf("Empty\n");
	return; }
printf("Loading index...\n");
if((idx=(idxrec_t *)LMALLOC(sizeof(idxrec_t)*status.total_msgs))
	==NULL) {
	smb_unlocksmbhdr();
	printf("can't allocate %lu bytes of memory\n"
		,sizeof(idxrec_t)*status.total_msgs);
	return; }
fseek(sid_fp,0L,SEEK_SET);
for(l=0;l<status.total_msgs;l++) {
	printf("%lu of %lu\r"
		,l+1,status.total_msgs);
	if(!fread(&idx[l],1,sizeof(idxrec_t),sid_fp))
		break; }
printf("\nDone.\n\n");

printf("Scanning for pre-flagged messages...\n");
for(m=0;m<l;m++) {
	printf("\r%2u%%",m ? (long)(100.0/((float)l/m)) : 0);
	if(idx[m].attr&MSG_DELETE)
		flagged++; }
printf("\r100%% (%lu pre-flagged for deletion)\n",flagged);

if(status.max_age) {
	printf("Scanning for messages more than %u days old...\n"
		,status.max_age);
	for(m=f=0;m<l;m++) {
		printf("\r%2u%%",m ? (long)(100.0/((float)l/m)) : 0);
		if(idx[m].attr&(MSG_PERMANENT|MSG_DELETE))
			continue;
		if(now>idx[m].time && (now-idx[m].time)/(24L*60L*60L)>status.max_age) {
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

if(l-flagged>status.max_msgs) {
	printf("Flagging excess messages for deletion...\n");
	for(m=n=0,f=flagged;l-flagged>status.max_msgs && m<l;m++) {
		if(idx[m].attr&(MSG_PERMANENT|MSG_DELETE))
			continue;
		printf("%lu of %lu\r",++n,(l-f)-status.max_msgs);
		flagged++;
		idx[m].attr|=MSG_DELETE; }			/* mark for deletion */
	printf("\nDone.\n\n"); }

if(!flagged) {				/* No messages to delete */
	LFREE(idx);
	smb_unlocksmbhdr();
	return; }

if(!(mode&NOANALYSIS)) {

	printf("Freeing allocated header and data blocks for deleted messages...\n");
	if(!(status.attr&SMB_HYPERALLOC)) {
		i=smb_open_da(10);
		if(i) {
			smb_unlocksmbhdr();
			printf("smb_open_da returned %d\n",i);
			exit(1); }
		i=smb_open_ha(10);
		if(i) {
			smb_unlocksmbhdr();
			printf("smb_open_ha returned %d\n",i);
			exit(1); } }

	for(m=n=0;m<l;m++) {
		if(idx[m].attr&MSG_DELETE) {
			printf("%lu of %lu\r",++n,flagged);
			msg.idx=idx[m];
			msg.hdr.number=msg.idx.number;
			if((i=smb_getmsgidx(&msg))!=0) {
				printf("\nsmb_getmsgidx returned %d\n",i);
				continue; }
			i=smb_lockmsghdr(msg,10);
			if(i) {
				printf("\nsmb_lockmsghdr returned %d\n",i);
				break; }
			if((i=smb_getmsghdr(&msg))!=0) {
				smb_unlockmsghdr(msg);
				printf("\nsmb_getmsghdr returned %d\n",i);
				break; }
			msg.hdr.attr|=MSG_DELETE;			/* mark header as deleted */
			if((i=smb_putmsg(msg))!=0) {
				smb_freemsgmem(msg);
				smb_unlockmsghdr(msg);
				printf("\nsmb_putmsg returned %d\n",i);
				break; }
			smb_unlockmsghdr(msg);
			if((i=smb_freemsg(msg,status))!=0) {
				smb_freemsgmem(msg);
				printf("\nsmb_freemsg returned %d\n",i);
				break; }
			smb_freemsgmem(msg); } }
	if(!(status.attr&SMB_HYPERALLOC)) {
		fclose(sha_fp);
		fclose(sda_fp); }
	printf("\nDone.\n\n"); }

printf("Re-writing index...\n");
rewind(sid_fp);
if(chsize(fileno(sid_fp),0L))
	printf("chsize failed!\n");
for(m=n=0;m<l;m++) {
	if(idx[m].attr&MSG_DELETE)
		continue;
	printf("%lu of %lu\r",++n,l-flagged);
	fwrite(&idx[m],sizeof(idxrec_t),1,sid_fp); }
printf("\nDone.\n\n");
fflush(sid_fp);

LFREE(idx);
status.total_msgs-=flagged;
smb_putstatus(status);
smb_unlocksmbhdr();
}

/****************************************************************************/
/* Kills 'msgs' number of messags                                           */
/* Returns actual number of messages killed.								*/
/****************************************************************************/
ulong kill(ulong msgs)
{
    int i;
	ulong l,m,n,flagged=0;
	smbstatus_t status;
	smbmsg_t msg;
	idxrec_t *idx;

i=smb_locksmbhdr(10);
if(i) {
	printf("smb_locksmbhdr returned %d\n",i);
	return(0); }
i=smb_getstatus(&status);
if(i) {
	smb_unlocksmbhdr();
	printf("smb_getstatus returned %d\n",i);
	return(0); }
printf("Loading index...\n");
if((idx=(idxrec_t *)LMALLOC(sizeof(idxrec_t)*status.total_msgs))
	==NULL) {
	smb_unlocksmbhdr();
	printf("can't allocate %lu bytes of memory\n"
		,sizeof(idxrec_t)*status.total_msgs);
	return(0); }
fseek(sid_fp,0L,SEEK_SET);
for(l=0;l<status.total_msgs;l++) {
	printf("%lu of %lu\r"
		,l+1,status.total_msgs);
	if(!fread(&idx[l],1,sizeof(idxrec_t),sid_fp))
		break; }
printf("\nDone.\n\n");

printf("Flagging messages for deletion...\n");
for(m=0;m<l && flagged<msgs;m++) {
	if(idx[m].attr&(MSG_PERMANENT))
		continue;
	printf("%lu of %lu\r",++flagged,msgs);
	idx[m].attr|=MSG_DELETE; }			/* mark for deletion */
printf("\nDone.\n\n");

printf("Freeing allocated header and data blocks for deleted messages...\n");
if(!(status.attr&SMB_HYPERALLOC)) {
	i=smb_open_da(10);
	if(i) {
		smb_unlocksmbhdr();
		printf("smb_open_da returned %d\n",i);
		exit(1); }
	i=smb_open_ha(10);
	if(i) {
		smb_unlocksmbhdr();
		printf("smb_open_ha returned %d\n",i);
		exit(1); } }

for(m=n=0;m<l;m++) {
	if(idx[m].attr&MSG_DELETE) {
		printf("%lu of %lu\r",++n,flagged);
		msg.idx=idx[m];
		i=smb_lockmsghdr(msg,10);
		if(i) {
			printf("\nsmb_lockmsghdr returned %d\n",i);
			break; }
		msg.hdr.number=msg.idx.number;
		if((i=smb_getmsgidx(&msg))!=0) {
			smb_unlockmsghdr(msg);
			printf("\nsmb_getmsgidx returned %d\n",i);
            break; }
		if((i=smb_getmsghdr(&msg))!=0) {
			smb_unlockmsghdr(msg);
			printf("\nsmb_getmsghdr returned %d\n",i);
			break; }
		msg.hdr.attr|=MSG_DELETE;			/* mark header as deleted */
		if((i=smb_putmsg(msg))!=0) {
			smb_unlockmsghdr(msg);
			printf("\nsmb_putmsg returned %d\n",i);
            break; }
		smb_unlockmsghdr(msg);
		smb_freemsg(msg,status);
		smb_freemsgmem(msg); } }

if(!(status.attr&SMB_HYPERALLOC)) {
	fclose(sha_fp);
	fclose(sda_fp); }

printf("\nDone.\n\n");

printf("Re-writing index...\n");
rewind(sid_fp);
chsize(fileno(sid_fp),0L);
for(m=n=0;m<l;m++) {
    if(idx[m].attr&MSG_DELETE)
        continue;
    printf("%lu of %lu\r",++n,l-flagged);
    fwrite(&idx[m],1,sizeof(idxrec_t),sid_fp); }
printf("\nDone.\n\n");

fflush(sid_fp);

LFREE(idx);
status.total_msgs-=flagged;
smb_putstatus(status);
smb_unlocksmbhdr();
return(flagged);
}

typedef struct {
	ulong old,new;
	} datoffset_t;

/****************************************************************************/
/* Removes all unused blocks from SDT and SHD files 						*/
/****************************************************************************/
void packmsgs(ulong packable)
{
	uchar buf[SDT_BLOCK_LEN],ch,fname[128],tmpfname[128];
	int i,file,size;
	ulong l,m,n,datoffsets=0,length,total;
	FILE *tmp_sdt,*tmp_shd,*tmp_sid;
	smbhdr_t	hdr;
	smbstatus_t status;
	smbmsg_t	msg;
	datoffset_t *datoffset=NULL;

printf("Packing %s\n",smb_file);
i=smb_locksmbhdr(10);
if(i) {
	printf("smb_locksmbhdr returned %d\n",i);
	return; }
i=smb_getstatus(&status);
if(i) {
	smb_unlocksmbhdr();
	printf("smb_getstatus returned %d\n",i);
    return; }

if(!(status.attr&SMB_HYPERALLOC)) {
	i=smb_open_ha(10);
	if(i) {
		smb_unlocksmbhdr();
		printf("smb_open_ha returned %d\n",i);
		return; }
	i=smb_open_da(10);
	if(i) {
		smb_unlocksmbhdr();
		fclose(sha_fp);
		printf("smb_open_da returned %d\n",i);
		return; } }

if(!status.total_msgs) {
    printf("Empty\n");
	rewind(shd_fp);
    chsize(fileno(shd_fp),status.header_offset);
	rewind(sdt_fp);
    chsize(fileno(sdt_fp),0L);
	rewind(sid_fp);
    chsize(fileno(sid_fp),0L);
	if(!(status.attr&SMB_HYPERALLOC)) {
		rewind(sha_fp);
		chsize(fileno(sha_fp),0L);
		rewind(sda_fp);
		chsize(fileno(sda_fp),0L);
		fclose(sha_fp);
		fclose(sda_fp); }
    smb_unlocksmbhdr();
    return; }


if(!(status.attr&SMB_HYPERALLOC) && !(mode&NOANALYSIS)) {
    printf("Analyzing data blocks...\n");

    length=filelength(fileno(sda_fp));

    fseek(sda_fp,0L,SEEK_SET);
    for(l=m=0;l<length;l+=2) {
        printf("\r%2u%%  ",l ? (long)(100.0/((float)length/l)) : 0);
        i=0;
        if(!fread(&i,2,1,sda_fp))
            break;
        if(!i)
            m++; }

    printf("\rAnalyzing header blocks...\n");

    length=filelength(fileno(sha_fp));

    fseek(sha_fp,0L,SEEK_SET);
    for(l=n=0;l<length;l++) {
        printf("\r%2u%%  ",l ? (long)(100.0/((float)length/l)) : 0);
		ch=0;
		if(!fread(&ch,1,1,sha_fp))
            break;
		if(!ch)
            n++; }

    if(!m && !n) {
		printf("\rAlready compressed.\n\n");
        fclose(sha_fp);
        fclose(sda_fp);
        smb_unlocksmbhdr();
        return; }

	if(packable && (m*SDT_BLOCK_LEN)+(n*SHD_BLOCK_LEN)<packable*1024L) {
		printf("\rLess than %luk compressable bytes.\n\n",packable);
		fclose(sha_fp);
		fclose(sda_fp);
		smb_unlocksmbhdr();
		return; }

	printf("\rCompressing %lu data blocks (%lu bytes)\n"
			 "        and %lu header blocks (%lu bytes)\n"
			  ,m,m*SDT_BLOCK_LEN,n,n*SHD_BLOCK_LEN); }

if(!(status.attr&SMB_HYPERALLOC)) {
	rewind(sha_fp);
	chsize(fileno(sha_fp),0L);		/* Reset both allocation tables */
	rewind(sda_fp);
	chsize(fileno(sda_fp),0L); }

if(status.attr&SMB_HYPERALLOC && !(mode&NOANALYSIS)) {
	printf("Analyzing %s\n",smb_file);

	length=filelength(fileno(shd_fp));
	m=n=0;
	for(l=status.header_offset;l<length;l+=size) {
		printf("\r%2u%%  ",(long)(100.0/((float)length/l)));
		msg.idx.offset=l;
		if((i=smb_lockmsghdr(msg,10))!=0) {
			printf("\n(%06lX) smb_lockmsghdr returned %d\n",l,i);
			size=SHD_BLOCK_LEN;
			continue; }
		if((i=smb_getmsghdr(&msg))!=0) {
			smb_unlockmsghdr(msg);
			m++;
			size=SHD_BLOCK_LEN;
			continue; }
		smb_unlockmsghdr(msg);
		if(msg.hdr.attr&MSG_DELETE) {
			m+=smb_hdrblocks(msg.hdr.length);
			total=0;
			for(i=0;i<msg.hdr.total_dfields;i++)
				total+=msg.dfield[i].length;
			n+=smb_datblocks(total); }
		size=smb_getmsghdrlen(msg);
		if(size<1) size=SHD_BLOCK_LEN;
		while(size%SHD_BLOCK_LEN)
			size++;
		smb_freemsgmem(msg); }


    if(!m && !n) {
		printf("\rAlready compressed.\n\n");
        smb_unlocksmbhdr();
        return; }

	if(packable && (n*SDT_BLOCK_LEN)+(m*SHD_BLOCK_LEN)<packable*1024L) {
		printf("\rLess than %luk compressable bytes.\n\n",packable);
		smb_unlocksmbhdr();
		return; }

	printf("\rCompressing %lu data blocks (%lu bytes)\n"
			 "        and %lu header blocks (%lu bytes)\n"
			  ,n,n*SDT_BLOCK_LEN,m,m*SHD_BLOCK_LEN); }

sprintf(fname,"%s.SD$",smb_file);
tmp_sdt=fopen(fname,"wb");
sprintf(fname,"%s.SH$",smb_file);
tmp_shd=fopen(fname,"wb");
sprintf(fname,"%s.SI$",smb_file);
tmp_sid=fopen(fname,"wb");
if(!tmp_sdt || !tmp_shd || !tmp_sid) {
	smb_unlocksmbhdr();
	if(!(status.attr&SMB_HYPERALLOC)) {
		fclose(sha_fp);
		fclose(sda_fp); }
	printf("error opening temp file\n");
	return; }
setvbuf(tmp_sdt,NULL,_IOFBF,2*1024);
setvbuf(tmp_shd,NULL,_IOFBF,2*1024);
setvbuf(tmp_sid,NULL,_IOFBF,2*1024);
if(!(status.attr&SMB_HYPERALLOC)
	&& (datoffset=(datoffset_t *)LMALLOC(sizeof(datoffset_t)*status.total_msgs))
	==NULL) {
	smb_unlocksmbhdr();
	fclose(sha_fp);
	fclose(sda_fp);
	fclose(tmp_sdt);
	fclose(tmp_shd);
	fclose(tmp_sid);
	printf("error allocating mem\n");
    return; }
fseek(shd_fp,0L,SEEK_SET);
fread(&hdr,1,sizeof(smbhdr_t),shd_fp);
fwrite(&hdr,1,sizeof(smbhdr_t),tmp_shd);
fwrite(&status,1,sizeof(smbstatus_t),tmp_shd);
for(l=sizeof(smbhdr_t)+sizeof(smbstatus_t);l<status.header_offset;l++) {
	fread(&ch,1,1,shd_fp);			/* copy additional base header records */
	fwrite(&ch,1,1,tmp_shd); }
fseek(sid_fp,0L,SEEK_SET);
total=0;
for(l=0;l<status.total_msgs;l++) {
	printf("%lu of %lu\r",l+1,status.total_msgs);
	if(!fread(&msg.idx,1,sizeof(idxrec_t),sid_fp))
		break;
	if(msg.idx.attr&MSG_DELETE) {
		printf("\nDeleted index.\n");
		continue; }
	i=smb_lockmsghdr(msg,10);
	if(i) {
		printf("\nsmb_lockmsghdr returned %d\n",i);
		continue; }
	i=smb_getmsghdr(&msg);
	smb_unlockmsghdr(msg);
	if(i) {
		printf("\nsmb_getmsghdr returned %d\n",i);
		continue; }
	if(msg.hdr.attr&MSG_DELETE) {
		printf("\nDeleted header.\n");
		smb_freemsgmem(msg);
		continue; }
	for(m=0;m<datoffsets;m++)
		if(msg.hdr.offset==datoffset[m].old)
			break;
	if(m<datoffsets) {				/* another index pointed to this data */
		printf("duplicate index\n");
		msg.hdr.offset=datoffset[m].new;
		smb_incdat(datoffset[m].new,smb_getmsgdatlen(msg),1); }
	else {

		if(!(status.attr&SMB_HYPERALLOC))
			datoffset[datoffsets].old=msg.hdr.offset;

		fseek(sdt_fp,msg.hdr.offset,SEEK_SET);

		m=smb_getmsgdatlen(msg);
        if(m>16L*1024L*1024L) {
            printf("\nInvalid data length (%lu)\n",m);
            continue; }

		if(!(status.attr&SMB_HYPERALLOC)) {
			datoffset[datoffsets].new=msg.hdr.offset
				=smb_fallocdat(m,1);
			datoffsets++;
			fseek(tmp_sdt,msg.hdr.offset,SEEK_SET); }
		else {
			fseek(tmp_sdt,0L,SEEK_END);
			msg.hdr.offset=ftell(tmp_sdt); }

		/* Actually copy the data */

		n=smb_datblocks(m);
		for(m=0;m<n;m++) {
			fread(buf,1,SDT_BLOCK_LEN,sdt_fp);
			if(!m && *(ushort *)buf!=XLAT_NONE && *(ushort *)buf!=XLAT_LZH) {
				printf("\nUnsupported translation type (%04X)\n"
					,*(ushort *)buf);
				break; }
			fwrite(buf,1,SDT_BLOCK_LEN,tmp_sdt); }
		if(m<n)
			continue; }

	/* Write the new index entry */
	length=smb_getmsghdrlen(msg);
	if(status.attr&SMB_HYPERALLOC)
		msg.idx.offset=ftell(tmp_shd);
	else
		msg.idx.offset=smb_fallochdr(length)+status.header_offset;
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
	smb_freemsgmem(msg); }

if(datoffset)
	LFREE(datoffset);
if(!(status.attr&SMB_HYPERALLOC)) {
	fclose(sha_fp);
	fclose(sda_fp); }

/* Change *.SH$ into *.SHD */
fclose(shd_fp);
fclose(tmp_shd);
sprintf(fname,"%s.SHD",smb_file);
remove(fname);
sprintf(tmpfname,"%s.SH$",smb_file);
rename(tmpfname,fname);

/* Change *.SD$ into *.SDT */
fclose(sdt_fp);
fclose(tmp_sdt);
sprintf(fname,"%s.SDT",smb_file);
remove(fname);
sprintf(tmpfname,"%s.SD$",smb_file);
rename(tmpfname,fname);

/* Change *.SI$ into *.SID */
fclose(sid_fp);
fclose(tmp_sid);
sprintf(fname,"%s.SID",smb_file);
remove(fname);
sprintf(tmpfname,"%s.SI$",smb_file);
rename(tmpfname,fname);

if((i=smb_open(10))!=0) {
	printf("Error %d reopening %s\n",i,smb_file);
	return; }

status.total_msgs=total;
if((i=smb_putstatus(status))!=0)
	printf("\nsmb_putstatus returned %d\n",i);
printf("\nDone.\n\n");
}


/****************************************************************************/
/* Read messages in message base											*/
/****************************************************************************/
void readmsgs(ulong start)
{
	char	str[128],*inbuf,*outbuf;
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
		fseek(sid_fp,msg.offset*sizeof(idxrec_t),SEEK_SET);
		if(!fread(&msg.idx,1,sizeof(idxrec_t),sid_fp))
			break;
		i=smb_lockmsghdr(msg,10);
		if(i) {
			printf("smb_lockmsghdr returned %d\n",i);
			break; }
		i=smb_getmsghdr(&msg);
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
		printf("\nDate : %.24s %s",ctime((time_t *)&msg.hdr.when_written.time)
			,zonestr(msg.hdr.when_written.zone));
		printf("\n\n");
		for(i=0;i<msg.hdr.total_dfields;i++)
			switch(msg.dfield[i].type) {
				case TEXT_BODY:
				case TEXT_TAIL:
					fseek(sdt_fp,msg.hdr.offset+msg.dfield[i].offset
						,SEEK_SET);
					fread(&xlat,2,1,sdt_fp);
					l=2;
					lzh=0;
					while(xlat!=XLAT_NONE) {
						if(xlat==XLAT_LZH)
							lzh=1;
						fread(&xlat,2,1,sdt_fp);
						l+=2; }
					if(lzh) {
						if((inbuf=(char *)LMALLOC(msg.dfield[i].length))
							==NULL) {
							printf("Malloc error of %lu\n"
								,msg.dfield[i].length);
							exit(1); }
						fread(inbuf,msg.dfield[i].length-l,1,sdt_fp);
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
							ch=fgetc(sdt_fp);
							if(ch)
								putchar(ch); } }
					printf("\n");
					break; }
		i=smb_unlockmsghdr(msg);
        if(i) {
			printf("smb_unlockmsghdr returned %d\n",i);
			break; }
		smb_freemsgmem(msg); }
	domsg=1;
	printf("\nReading %s (?=Menu): ",smb_file);
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
	smbstatus_t status;

#ifdef __TURBOC__
	timezone=0; 		/* Fix for Borland C++ EST default */
	daylight=0; 		/* Fix for Borland C++ EDT default */
#elif defined(__WATCOMC__)
	putenv("TZ=UCT0");  /* Fix for Watcom C++ EDT default */
#endif
setvbuf(stdout,0,_IONBF,0);

smb_file[0]=0;
printf("\nSynchronet Message Base Utility v%s  "\
	"Copyright 1995 Digital Dynamics\n\n"
	,SMBUTIL_VER);
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
			sprintf(smb_file,"%.64s",argv[x]);
			p=strrchr(smb_file,'.');
			s=strrchr(smb_file,'\\');
			if(p>s) *p=0;
			strupr(smb_file);
			printf("Opening %s\r\n",smb_file);
			if((i=smb_open(10))!=0) {
				printf("error %d opening %s message base\n",i,smb_file);
				exit(1); }
			if(!filelength(fileno(shd_fp))) {
				printf("Empty\n");
				smb_close();
				continue; }
			for(y=0;cmd[y];y++)
				switch(toupper(cmd[y])) {
					case 'I':
						strcpy(filein,cmd+1);
						i=smb_locksmbhdr(10);
						if(i) {
							printf("smb_locksmbhdr returned %d\n",i);
							return(1); }
						i=smb_getstatus(&status);
						if(i) {
							printf("smb_getstatus returned %d\n",i);
							return(1); }
							smb_unlocksmbhdr();
						postmsg(status);
						y=strlen(cmd)-1;
						break;
					case 'K':
						printf("Killing %lu messages...\n",atol(cmd+1));
						l=kill(atol(cmd+1));
						printf("%lu messages killed.\n",l);
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
			smb_close(); } } }
if(!cmd[0])
	printf("%s",usage);
return(0);
}
