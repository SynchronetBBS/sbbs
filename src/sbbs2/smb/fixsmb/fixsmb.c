/* FIXSMB.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* Re-generates an SMB message base based on SHD and SDT files */

#include "smblib.h"

smb_t smb;

char *usage="usage: fixsmb [/opts] <smb_file>\n"
			"\n"
			" opts:\n"
			"       m - force mail format instead of sub-board format\n"
			"\n"
			"   ex: FIXSMB /M MAIL\n"
			"   or: FIXSMB DEBATE\n";

void remove_re(char *str)
{
while(!strnicmp(str,"RE:",3)) {
	strcpy(str,str+3);
	while(str[0]==SP)
		strcpy(str,str+1); }
}

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

#define MAIL (1<<0)

int main(int argc, char **argv)
{
	char		str[128],c;
	int 		i,w,mode=0;
	ulong		l,length,size,n,m;
	smbmsg_t	msg;
	smbstatus_t status;

printf("\nFIXSMB v1.22 ú Rebuild Synchronet Message Base ú Developed 1995-1997 "
	"Rob Swindell\n");

smb.file[0]=0;
for(i=1;i<argc;i++)
	if(argv[i][0]=='/')
		switch(toupper(argv[i][1])) {
			case 'M':
				mode|=MAIL;
				break;
			default:
				printf(usage);
				exit(1); }
	else
		strcpy(smb.file,argv[i]);

if(!smb.file[0]) {
	printf(usage);
	exit(1); }

strupr(smb.file);
smb.retry_time=30;

if((i=smb_open(&smb))!=0) {
	printf("smb_open returned %d\n",i);
	exit(1); }

if((i=smb_locksmbhdr(&smb))!=0) {
	smb_close(&smb);
	printf("smb_locksmbhdr returned %d\n",i);
	exit(1); }

if((i=smb_getstatus(&smb))!=0) {
	smb_unlocksmbhdr(&smb);
	smb_close(&smb);
	printf("smb_getstatus returned %d\n",i);
	exit(1); }

if(mode&MAIL && !(status.attr&SMB_EMAIL)) {
	status.attr|=SMB_EMAIL;
	if((i=smb_putstatus(&smb))!=0) {
		smb_unlocksmbhdr(&smb);
		smb_close(&smb);
		printf("smb_putstatus returned %d\n",i);
		exit(1); } }

if(!(status.attr&SMB_HYPERALLOC)) {

	if((i=smb_open_ha(&smb))!=0) {
		smb_close(&smb);
		printf("smb_open_ha returned %d\n",i);
		exit(1); }

	if((i=smb_open_da(&smb))!=0) {
		smb_close(&smb);
		printf("smb_open_da returned %d\n",i);
		exit(1); }

	rewind(smb.sha_fp);
	chsize(fileno(smb.sha_fp),0L);		/* Truncate the header allocation file */
	rewind(smb.sda_fp);
	chsize(fileno(smb.sda_fp),0L);		/* Truncate the data allocation file */
	}

rewind(smb.sid_fp);
chsize(fileno(smb.sid_fp),0L);			/* Truncate the index */


if(!(status.attr&SMB_HYPERALLOC)) {
	length=filelength(fileno(smb.sdt_fp));
	w=0;
	for(l=0;l<length;l+=SDT_BLOCK_LEN)	/* Init .SDA file to NULL */
		fwrite(&w,2,1,smb.sda_fp);

	length=filelength(fileno(smb.shd_fp));
	c=0;
	for(l=0;l<length;l+=SHD_BLOCK_LEN)	/* Init .SHD file to NULL */
		fwrite(&c,1,1,smb.sha_fp); }
else
	length=filelength(fileno(smb.shd_fp));

n=1;	/* messsage number */
for(l=status.header_offset;l<length;l+=size) {
	printf("\r%2u%%  ",(long)(100.0/((float)length/l)));
	msg.idx.offset=l;
	if((i=smb_lockmsghdr(&smb,&msg))!=0) {
		printf("\n(%06lX) smb_lockmsghdr returned %d\n",l,i);
		continue; }
	if((i=smb_getmsghdr(&smb,&msg))!=0) {
		smb_unlockmsghdr(&smb,&msg);
		printf("\n(%06lX) smb_getmsghdr returned %d\n",l,i);
		size=SHD_BLOCK_LEN;
		continue; }
	smb_unlockmsghdr(&smb,&msg);
	printf("#%-5lu (%06lX) %-25.25s ",msg.hdr.number,l,msg.from);
	if(!(msg.hdr.attr&MSG_DELETE)) {   /* Don't index deleted messages */
		msg.offset=n-1;
		msg.hdr.number=n;
		msg.idx.number=n;
		msg.idx.attr=msg.hdr.attr;
		msg.idx.time=msg.hdr.when_imported.time;
		strcpy(str,msg.subj);
		strlwr(str);
		remove_re(str);
		msg.idx.subj=crc16(str);
		if(status.attr&SMB_EMAIL) {
			if(msg.to_ext)
				msg.idx.to=atoi(msg.to_ext);
			else
				msg.idx.to=0;
			if(msg.from_ext)
				msg.idx.from=atoi(msg.from_ext);
			else
				msg.idx.from=0; }
		else {
			strcpy(str,msg.to);
			strlwr(str);
			msg.idx.to=crc16(str);
			strcpy(str,msg.from);
			strlwr(str);
			msg.idx.from=crc16(str); }
		if((i=smb_putmsg(&smb,&msg))!=0) {
			printf("\nsmb_putmsg returned %d\n",i);
			continue; }
		n++; }
	else
		printf("Not indexing deleted message\n");
	size=smb_getmsghdrlen(&msg);
	while(size%SHD_BLOCK_LEN)
		size++;

	if(!(status.attr&SMB_HYPERALLOC)) {
		/**************************/
		/* Allocate header blocks */
		/**************************/
		fseek(smb.sha_fp,(l-status.header_offset)/SHD_BLOCK_LEN,SEEK_SET);
		if(msg.hdr.attr&MSG_DELETE) c=0;		/* mark as free */
		else c=1;								/* or allocated */

		for(i=0;i<size/SHD_BLOCK_LEN;i++)
			fputc(c,smb.sha_fp);

		/************************/
		/* Allocate data blocks */
		/************************/

		if(!(msg.hdr.attr&MSG_DELETE))
			smb_incdat(&smb,msg.hdr.offset,smb_getmsgdatlen(&msg),1);
		}

	smb_freemsgmem(&msg); }
printf("\nDone.\n");
status.total_msgs=status.last_msg=n-1;
if((i=smb_putstatus(&smb))!=0)
	printf("\nsmb_putstatus returned %d\n",i);
smb_unlocksmbhdr(&smb);
smb_close(&smb);
return(0);
}
