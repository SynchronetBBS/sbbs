/* QWK2SMB.C */

/* Converts QWK packet to to SMB formatted message bases */

/* The intention of this source code is an example of how to use the SMBLIB */
/* library functions to access an SMB format message base.					*/

/* This program and source code are freeware. May be used in part or whole	*/
/* for any purpose without consent or notification of Digital Dynamics. 	*/

/* Digital Dynamics does request that developers that release products that */
/* support the SMB format notify Digital Dynamics so the implementation 	*/
/* and contact chapters in the technical specification may be updated.		*/

#include "smblib.h"
#include "crc32.h"
#include <dos.h>

char sbbs[128];

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
/* Truncates white-space chars off end of 'str' and terminates at first tab */
/****************************************************************************/
void truncsp(char *str)
{
	char c;

str[strcspn(str,"\t")]=0;
c=strlen(str);
while(c && (uchar)str[c-1]<=' ') c--;
str[c]=0;
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
/* Returns the length of the file in 'filespec'                             */
/****************************************************************************/
long flength(char *filespec)
{
    struct ffblk f;

if(findfirst(filespec,&f,0)==0)
    return(f.ff_fsize);
return(-1L);
}

/****************************************************************************/
/* Returns the FidoNet address kept in str as ASCII.                        */
/****************************************************************************/
fidoaddr_t atofaddr(char *str)
{
    char *p;
	fidoaddr_t addr;

addr.zone=addr.net=addr.node=addr.point=0;
if((p=strchr(str,':'))!=NULL) {
	addr.zone=atoi(str);
	addr.net=atoi(p+1); }
else {
	addr.zone=1;
	addr.net=atoi(str); }
if(!addr.zone)				/* no such thing as zone 0 */
	addr.zone=1;
if((p=strchr(str,'/'))!=NULL)
    addr.node=atoi(p+1);
else {
	addr.net=1;
	addr.node=atoi(str); }
if((p=strchr(str,'.'))!=NULL)
    addr.point=atoi(p+1);
return(addr);
}


int main(int argc, char **argv)
{
	uchar	*p,str[128],from[128],to[128],subj[128],block[128],*buf;
	ushort	xlat;
	ushort	i,j,k,msgs,total;
	ulong	l,m,length,size,crc;
	FILE	*qwk;
	struct	date date;
	struct	time curtime;
	smbmsg_t	msg;
	smbstatus_t status;

if(argc<2) {
	printf("usage: qwk2smb <qwk_dir>\r\n");
	exit(1); }

sprintf(str,"%s\\MESSAGES.DAT",argv[1]);
if((qwk=fopen(str,"rb"))==NULL) {
	printf("error opening %s\n",str);
	exit(1); }

size=filelength(fileno(qwk));
smb_file[0]=0;

for(l=128,i=1,msgs=total=1;l<size;l+=i*128) {
	fseek(qwk,l,SEEK_SET);
	if(!fread(block,1,128,qwk))
		break;
	i=atoi(block+116);	/* i = number of 128 byte records */
	if(i<2) {
		i=1;
		continue; }

	j=(ushort)block[123]|(((ushort)block[124])<<8);  /* conference number */

	if(strcmp(smb_file,itoa(j,str,10))) {		 /* changed conference */
		fclose(sdt_fp);
		fclose(shd_fp);
		fclose(sid_fp);
		strcpy(smb_file,str);
		printf("\nConference #%s\n",str);
		msgs=1;
		k=smb_open(10);
		if(k) {
			printf("smb_open returned %d\n",k);
			exit(1); }
		if(!filelength(fileno(shd_fp))) 		/* new conference */
			smb_create(2000,2000,0,0,10); }

	smb_getstatus(&status); 	  // Initialized for first call to smb_addcrc()

	printf("%u (total=%u)\r",msgs,total);

	/*****************************/
	/* Initialize the SMB header */
	/*****************************/

	memset(&msg,0,sizeof(smbmsg_t));
	memcpy(msg.hdr.id,"SHD\x1a",4);
	msg.hdr.version=SMB_VERSION;

	/**************************/
	/* Convert the QWK header */
	/**************************/

	if(block[0]=='*' || block[0]=='+')
		msg.idx.attr|=MSG_PRIVATE;
	if(block[0]=='*' || block[0]=='-' || block[0]=='`')
		msg.idx.attr|=MSG_READ;
    msg.hdr.attr=msg.idx.attr;

	date.da_mon=((block[8]&0xf)*10)+(block[9]&0xf);
	date.da_day=((block[11]&0xf)*10)+(block[12]&0xf);
	date.da_year=((block[14]&0xf)*10)+(block[15]&0xf)+1900;
	curtime.ti_hour=((block[16]&0xf)*10)+(block[17]&0xf);
	curtime.ti_min=((block[19]&0xf)*10)+(block[20]&0xf);
	curtime.ti_sec=0;
	msg.hdr.when_written.time=dostounix(&date,&curtime);
	msg.hdr.when_written.zone=PST;		 /* set to local time zone */
	msg.hdr.when_imported.time=time(NULL);
	msg.hdr.when_imported.zone=PST; 	 /* set to local time zone */

	sprintf(to,"%25.25s",block+21);     /* To user */
    truncsp(to);
    smb_hfield(&msg,RECIPIENT,strlen(to),to);
    strlwr(to);
    msg.idx.to=crc16(to);

	sprintf(from,"%25.25s",block+46);   /* From user */
    truncsp(from);
    smb_hfield(&msg,SENDER,strlen(from),from);
    strlwr(from);
    msg.idx.from=crc16(from);

	sprintf(subj,"%25.25s",block+71);   /* Subject */
	truncsp(subj);
	smb_hfield(&msg,SUBJECT,strlen(subj),subj);
	strlwr(subj);
    msg.idx.subj=crc16(subj);

	/********************************/
	/* Convert the QWK message text */
	/********************************/

	length=0;
	if((buf=MALLOC((i-1)*128*2))==NULL) {
		printf("memory allocation error\n");
		exit(1); }
	for(j=1;j<i;j++) {
		if(!fread(block,1,128,qwk))
			break;
		for(k=0;k<128;k++) {
			if(block[k]==0)
				continue;
			if(block[k]==0xE3) {	   /* expand 0xe3 to crlf */
				buf[length++]=CR;
				buf[length++]=LF;
				continue; }
			buf[length++]=block[k]; } }

	while(length && buf[length-1]<=SP) length--; /* remove trailing garbage */

	/*****************/
	/* Calculate CRC */
	/*****************/

	crc=0xffffffffUL;
	for(m=0;m<length;m++)
		crc=ucrc32(buf[m],crc);
	crc=~crc;

	/*******************/
	/* Check for dupes */
	/*******************/

	j=smb_addcrc(status.max_crcs,crc,10);
    if(j) {
        if(j==1) {
            printf("\nDuplicate message\n");
            smb_freemsgmem(msg);
			FREE(buf);
            continue; }
        printf("smb_addcrc returned %d\n",j);
        exit(1); }

	/*************************************/
	/* Write SMB message header and text */
	/*************************************/

	length+=2;	/* for translation string */

	if(smb_open_da(10)) {
		printf("error opening %s.SDA\n",smb_file);
		exit(1); }
	msg.hdr.offset=smb_fallocdat(length,1);
	fclose(sda_fp);
	if(msg.hdr.offset && msg.hdr.offset<1L) {
		printf("error %ld allocating blocks\r\n",msg.hdr.offset);
		exit(1); }
	fseek(sdt_fp,msg.hdr.offset,SEEK_SET);
    xlat=XLAT_NONE;
    fwrite(&xlat,2,1,sdt_fp);
	fwrite(buf,length,1,sdt_fp);
	fflush(sdt_fp);

	smb_dfield(&msg,TEXT_BODY,length);

	k=smb_addmsghdr(&msg,&status,1,10);
	if(k) {
		printf("smb_addmsghdr returned %d\n",k);
		exit(1); }
	smb_freemsgmem(msg);
	FREE(buf);
	msgs++;
	total++; }

return(0);
}

