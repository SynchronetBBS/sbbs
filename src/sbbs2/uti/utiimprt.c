/* UTIIMPRT.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"
#include "uti.h"
#include "crc32.h"

smb_t smb;

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


#define PRIVATE 0
#define PUBLIC	1

#define DEBUG 0


char *wday[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
char *mon[]={"Jan","Feb","Mar","Apr","May","Jun"
            ,"Jul","Aug","Sep","Oct","Nov","Dec"};

void remove_re(char *str)
{
while(!strnicmp(str,"RE:",3)) {
	strcpy(str,str+3);
	while(str[0]==SP)
		strcpy(str,str+1); }
}

/****************************************************************************/
/* Converts a date string in format MM/DD/YY into unix time format			*/
/****************************************************************************/
time_t dstrtounix(char *str)
{
	struct time curtime;
	struct date date;

#if DEBUG
printf("\rdstrtounix           ");
#endif

curtime.ti_hour=curtime.ti_min=curtime.ti_sec=0;
date.da_year=((str[6]&0xf)*10)+(str[7]&0xf);
if(date.da_year<Y2K_2DIGIT_WINDOW)
	date.da_year+=100;
date.da_year+=1900;
date.da_mon=((str[0]&0xf)*10)+(str[1]&0xf);
date.da_day=((str[3]&0xf)*10)+(str[4]&0xf);
return(dostounix(&date,&curtime));
}

/****************************************************************************/
/* Checks the disk drive for the existence of a file. Returns 1 if it       */
/* exists, 0 if it doesn't.                                                 */
/* Called from upload                                                       */
/****************************************************************************/
char fexist(char *filespec)
{
    struct ffblk f;

if(findfirst(filespec,&f,0)==NULL)
    return(1);
return(0);
}

/****************************************************************************/
/* This function reads files that are potentially larger than 32k.  		*/
/* Up to one megabyte of data can be read with each call.                   */
/****************************************************************************/
long lread(int file, char huge *buf,long bytes)
{
	long count;

for(count=bytes;count>32767;count-=32767,buf+=32767)
	if(read(file,(char *)buf,32767)!=32767)
		return(-1L);
if(read(file,(char *)buf,(int)count)!=count)
	return(-1L);
return(bytes);
}


int main(int argc, char **argv)
{
	char	str[256],to[256],from[256],title[256],*p,*buf,*outbuf;
	ushort	xlat,net=0;
	int 	i,j,file,lzh,storage;
	uint	subnum,imported=0;
	ulong	l,length,lzhlen,offset,crc;
	FILE	*stream;
	smbmsg_t	msg;
	smbstatus_t status;

PREPSCREEN;

printf("Synchronet UTIIMPRT v%s\n",VER);

if(argc<3)
	exit(1);

if((argc>3 && !stricmp(argv[3],"/NETWORK"))
	|| (argc>4 && !stricmp(argv[4],"/NETWORK")))
	net=NET_POSTLINK;

uti_init("UTIIMPRT",argc,argv);

if((file=nopen(argv[2],O_RDONLY))==-1)
	bail(2);
if((stream=fdopen(file,"rb"))==NULL)
	bail(2);

subnum=getsubnum(argv[1]);
if((int)subnum==-1)
	bail(7);

sprintf(smb.file,"%s%s",sub[subnum]->data_dir,sub[subnum]->code);
smb.retry_time=30;
if((i=smb_open(&smb))!=0) {
	errormsg(WHERE,ERR_OPEN,smb.file,i);
	bail(5); }

if(filelength(fileno(smb.shd_fp))<1) {	 /* Create it if it doesn't exist */
	smb.status.max_crcs=sub[subnum]->maxcrcs;
	smb.status.max_msgs=sub[subnum]->maxmsgs;
	smb.status.max_age=sub[subnum]->maxage;
	smb.status.attr=sub[subnum]->misc&SUB_HYPER ? SMB_HYPERALLOC : 0;
	if((i=smb_create(&smb))!=0) {
		errormsg(WHERE,ERR_CREATE,smb.file,i);
		bail(5); } }

printf("\r\nImporting ");

while(!feof(stream) && !ferror(stream)) {
	memset(&msg,0,sizeof(smbmsg_t));
	memcpy(msg.hdr.id,"SHD\x1a",4);
	msg.hdr.version=SMB_VERSION;
	msg.hdr.when_imported.time=time(NULL);
	msg.hdr.when_imported.zone=sys_timezone;
	if(sub[subnum]->misc&SUB_AONLY)
		msg.hdr.attr|=MSG_ANONYMOUS;

	if(!fgets(to,250,stream))
		break;
	if(!fgets(from,250,stream))
		break;
	if(!fgets(title,250,stream))
		break;
	imported++;
	printf("%-5u\b\b\b\b\b",imported);
	truncsp(to);
	truncsp(from);
	truncsp(title);

	smb_hfield(&msg,RECIPIENT,strlen(to),to);
	strlwr(to);
	msg.idx.to=crc16(to);

	smb_hfield(&msg,SENDER,strlen(from),from);
	strlwr(from);
	msg.idx.from=crc16(from);

	if(net)
		i=smb_hfield(&msg,SENDERNETTYPE,2,&net);

	i=smb_hfield(&msg,SUBJECT,strlen(title),title);
	strlwr(title);
	remove_re(title);
	msg.idx.subj=crc16(title);

	fgets(str,128,stream);	 /* skip msg # */
	fgets(str,128,stream);	 /* ref # */
	msg.hdr.thread_orig=atol(str);
	fgets(str,128,stream);	 /* date */
	msg.hdr.when_written.time=dstrtounix(str);
	fgets(str,128,stream);	 /* time */
	msg.hdr.when_written.time+=atoi(str)*60*60;  /* hours */
	p=strchr(str,':');
	if(p)
		msg.hdr.when_written.time+=atoi(p+1)*60; /* mins */
	fgets(str,128,stream);	 /* private/public */
	if(!stricmp(str,"PRIVATE"))
		msg.hdr.attr|=MSG_PRIVATE;
	fgets(str,128,stream);	 /* Read? Y/N */
	if(toupper(str[0])=='Y')
		msg.hdr.attr|=MSG_READ;
	fgets(str,128,stream);	 /* Net? Y/N - ignore */
	if(toupper(str[0])=='Y')
		msg.hdr.netattr|=MSG_TYPELOCAL;
	while(!feof(stream) && !ferror(stream)) {
		fgets(str,128,stream);
		if(!strcmp(str,"TEXT:\r\n"))
			break; }

	buf=NULL;
	length=0;
	crc=0xffffffff;
	while(!feof(stream) && !ferror(stream)) {
		fgets(str,128,stream);
		if(!strcmp(str,"\xff\r\n"))     /* end of text */
			break;
		j=strlen(str);
		if((buf=REALLOC(buf,length+j+1))==NULL) {
			errormsg(WHERE,ERR_ALLOC,argv[1],length+j+1);
			bail(3); }
		if(sub[subnum]->maxcrcs) {
			for(i=0;i<j;i++)
				crc=ucrc32(str[i],crc); }
		strcpy(buf+length,str);
		length+=strlen(str); }
	crc=~crc;

	if((i=smb_locksmbhdr(&smb))!=0) {
		errormsg(WHERE,ERR_LOCK,smb.file,i);
		bail(11); }

	if((i=smb_getstatus(&smb))!=0) {
		errormsg(WHERE,ERR_READ,smb.file,i);
		bail(12); }

	if(sub[subnum]->maxcrcs) {
		i=smb_addcrc(&smb,crc);
		if(i) {
			printf("\nDuplicate message!\n");
			FREE(buf);
			smb_unlocksmbhdr(&smb);
			smb_freemsgmem(&msg);
			continue; } }

	if(length>=2 && buf[length-1]==LF && buf[length-2]==CR)
		length-=2;
	if(length>=2 && buf[length-1]==LF && buf[length-2]==CR)
        length-=2;

	lzh=0;
	if(sub[subnum]->misc&SUB_LZH && length+2>=SDT_BLOCK_LEN) {
		if((outbuf=(char *)MALLOC(length*2))==NULL) {
			errormsg(WHERE,ERR_ALLOC,"lzh",length*2);
			smb_unlocksmbhdr(&smb);
			smb_freemsgmem(&msg);
			bail(3); }
		lzhlen=lzh_encode(buf,length,outbuf);
		if(lzhlen>1
			&& smb_datblocks(lzhlen+4)
				<smb_datblocks(length+2)) { /* Compressable */
			length=lzhlen+2;
			FREE(buf);
			lzh=1;
			buf=outbuf; }
		else				/* Uncompressable */
			FREE(outbuf); }

	length+=2;						/* for translation string */

	if(status.attr&SMB_HYPERALLOC) {
		offset=smb_hallocdat(&smb);
		storage=SMB_HYPERALLOC; }
	else {
		if((i=smb_open_da(&smb))!=0) {
			errormsg(WHERE,ERR_OPEN,smb.file,i);
			bail(5); }
		if(sub[subnum]->misc&SUB_FAST) {
			offset=smb_fallocdat(&smb,length,1);
			storage=SMB_FASTALLOC; }
		else {
			offset=smb_allocdat(&smb,length,1);
			storage=SMB_SELFPACK; }
		fclose(smb.sda_fp); }

	msg.hdr.offset=offset;

	smb_dfield(&msg,TEXT_BODY,length);

	fseek(smb.sdt_fp,offset,SEEK_SET);
	if(lzh) {
		xlat=XLAT_LZH;
		fwrite(&xlat,2,1,smb.sdt_fp); }
	xlat=XLAT_NONE;
	fwrite(&xlat,2,1,smb.sdt_fp);
	j=SDT_BLOCK_LEN-2;				/* Don't read/write more than 255 */
	if(lzh)
		j-=2;
	l=0;
	length-=2;
	if(lzh)
		length-=2;
	while(l<length) {
		if(l+j>length)
			j=length-l;
		fwrite(buf+l,j,1,smb.sdt_fp);
		l+=j;
		j=SDT_BLOCK_LEN; }
	fflush(smb.sdt_fp);
	FREE(buf);
	smb_unlocksmbhdr(&smb);
	smb_addmsghdr(&smb,&msg,storage);
	smb_freemsgmem(&msg); }

sprintf(str,"%20s Imported %u\r\n","",imported);
write(logfile,str,strlen(str));
printf("\nDone.\n");
bail(0);
return(0);
}

