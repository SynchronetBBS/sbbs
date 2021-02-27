/* FIDO2SMB.C */

/* Converts FidoNet FTSC-1 (*.MSG) message base to SMB format */

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
									/* Attribute bits for fido msg header */
#define FIDO_PRIVATE	(1<<0)		/* Private message */
#define FIDO_CRASH		(1<<1)		/* Crash-mail (send immediately) */
#define FIDO_RECV		(1<<2)		/* Received successfully */
#define FIDO_SENT		(1<<3)		/* Sent successfully */
#define FIDO_FILE		(1<<4)		/* File attached */
#define FIDO_INTRANS	(1<<5)		/* In transit */
#define FIDO_ORPHAN 	(1<<6)		/* Orphan */
#define FIDO_KILLSENT	(1<<7)		/* Kill it after sending it */
#define FIDO_LOCAL		(1<<8)		/* Created locally - on this system */
#define FIDO_HOLD		(1<<9)		/* Hold - don't send it yet */
#define FIDO_FREQ		(1<<11) 	/* File request */
#define FIDO_RRREQ		(1<<12) 	/* Return receipt request */
#define FIDO_RR 		(1<<13) 	/* This is a return receipt */
#define FIDO_AUDIT		(1<<14) 	/* Audit request */
#define FIDO_FUPREQ     (1<<15)     /* File update request */

typedef struct {						/* FidoNet msg header */
	uchar	from[36],					/* From user */
			to[36], 					/* To user */
			subj[72],					/* Message title */
			time[20];					/* Time in goof-ball ASCII format */
	short	read,						/* Times read */
			destnode,					/* Destination node */
			orignode,					/* Origin node */
			cost,						/* Cost in pennies */
			orignet,					/* Origin net */
			destnet,					/* Destination net */
			destzone,					/* Destination zone */
			origzone,					/* Origin zone */
			destpoint,					/* Destination point */
			origpoint,					/* Origin point */
			re, 						/* Message number regarding */
			attr,						/* Attributes - see FIDO_* */
			next;						/* Next message number in stream */
            } fmsghdr_t;

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

/****************************************************************************/
/* Converts goofy FidoNet time format into Unix format						*/
/****************************************************************************/
time_t fmsgtime(char *str)
{
	char month[4];
	struct date date;
	struct time t;

if(isdigit(str[1])) {	/* Regular format: "01 Jan 86  02:34:56" */
	date.da_day=atoi(str);
	sprintf(month,"%3.3s",str+3);
	if(!stricmp(month,"jan"))
		date.da_mon=1;
	else if(!stricmp(month,"feb"))
		date.da_mon=2;
	else if(!stricmp(month,"mar"))
		date.da_mon=3;
	else if(!stricmp(month,"apr"))
		date.da_mon=4;
	else if(!stricmp(month,"may"))
		date.da_mon=5;
	else if(!stricmp(month,"jun"))
		date.da_mon=6;
	else if(!stricmp(month,"jul"))
		date.da_mon=7;
	else if(!stricmp(month,"aug"))
		date.da_mon=8;
	else if(!stricmp(month,"sep"))
		date.da_mon=9;
	else if(!stricmp(month,"oct"))
		date.da_mon=10;
	else if(!stricmp(month,"nov"))
		date.da_mon=11;
	else
		date.da_mon=12;
	date.da_year=1900+atoi(str+7);
	t.ti_hour=atoi(str+11);
	t.ti_min=atoi(str+14);
	t.ti_sec=atoi(str+17); }

else {					/* SEAdog  format: "Mon  1 Jan 86 02:34" */
	date.da_day=atoi(str+4);
	sprintf(month,"%3.3s",str+7);
	if(!stricmp(month,"jan"))
		date.da_mon=1;
	else if(!stricmp(month,"feb"))
		date.da_mon=2;
	else if(!stricmp(month,"mar"))
		date.da_mon=3;
	else if(!stricmp(month,"apr"))
		date.da_mon=4;
	else if(!stricmp(month,"may"))
		date.da_mon=5;
	else if(!stricmp(month,"jun"))
		date.da_mon=6;
	else if(!stricmp(month,"jul"))
		date.da_mon=7;
	else if(!stricmp(month,"aug"))
		date.da_mon=8;
	else if(!stricmp(month,"sep"))
		date.da_mon=9;
	else if(!stricmp(month,"oct"))
		date.da_mon=10;
	else if(!stricmp(month,"nov"))
		date.da_mon=11;
	else
		date.da_mon=12;
	date.da_year=1900+atoi(str+11);
	t.ti_hour=atoi(str+14);
	t.ti_min=atoi(str+17);
	t.ti_sec=0; }
return(dostounix(&date,&t));
}

/****************************************************************************/
/* Entry point - if you didn't know that, maybe you shouldn't be reading :) */
/****************************************************************************/
int main(int argc, char **argv)
{
	uchar	*p,str[128],*fbuf,*sbody,*stail,ch,done;
	ushort	xlat,net;
	int 	i,j,file,col,cr,esc,orig,last,msgs,found;
	ulong	l,m,length,bodylen,taillen,crc;
	struct	ffblk ff;
	smbmsg_t	msg;
	smbstatus_t status;
	fmsghdr_t	fmsghdr;
	fidoaddr_t	destaddr,origaddr,faddr;

if(argc<3) {
	printf("usage: fido2smb <fido_dir> <smb_name>\r\n");
	exit(1); }

strcpy(smb_file,argv[2]);
strupr(smb_file);

smb_open(10);
if(!filelength(fileno(shd_fp)))
	smb_create(2000,2000,0,0,10);

smb_getstatus(&status); 	  // Initialized for first call to smb_addcrc()

/*********************************************/
/* Get the total number of .MSG files in dir */
/*********************************************/
printf("\nCounting messages...");
sprintf(str,"%s\\*.MSG",argv[1]);
last=findfirst(str,&ff,0);
for(msgs=0;!last;msgs++)
	last=findnext(&ff);
printf("\n%u messages.\n",msgs);

/******************************************/
/* Convert in sequence, starting at 1.MSG */
/******************************************/
for(i=1,found=0;found<msgs && i<2000;i++) {
	sprintf(str,"%s\\%u.MSG",argv[1],i);
	if((file=open(str,O_RDONLY|O_BINARY))==-1)
		continue;
	found++;
	strupr(str);
	printf("%s\n",str);
	read(file,&fmsghdr,sizeof(fmsghdr_t));
	memset(&msg,0,sizeof(smbmsg_t));
	memcpy(msg.hdr.id,"SHD\x1a",4);
	msg.hdr.version=SMB_VERSION;
	if(fmsghdr.attr&FIDO_PRIVATE)
		msg.idx.attr|=MSG_PRIVATE;
	msg.hdr.attr=msg.idx.attr;

	msg.hdr.when_imported.time=time(NULL);
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

	smb_hfield(&msg,SUBJECT,strlen(fmsghdr.subj),fmsghdr.subj);
	strlwr(fmsghdr.subj);
	msg.idx.subj=crc16(fmsghdr.subj);

	length=filelength(file)-sizeof(fmsghdr_t);
	if((fbuf=(char *)MALLOC(length))==NULL) {
		printf("alloc error\n");
		exit(1); }
	if((sbody=(char *)MALLOC(length*2L))==NULL) {
		printf("alloc error\n");
        exit(1); }
	if((stail=(char *)MALLOC(length))==NULL) {
		printf("alloc error\n");
        exit(1); }
	read(file,fbuf,length);
	close(file);

	for(col=l=esc=done=bodylen=taillen=orig=0,cr=1;l<length;l++) {
		ch=fbuf[l];
		if(ch==1 && cr) {	/* kludge line */

			if(!strncmp(fbuf+l+1,"TOPT ",5))
				destaddr.point=atoi(fbuf+l+6);

			else if(!strncmp(fbuf+l+1,"FMPT ",5))
				origaddr.point=atoi(fbuf+l+6);

			else if(!strncmp(fbuf+l+1,"INTL ",5)) {
				faddr=atofaddr(fbuf+l+6);
				destaddr.zone=faddr.zone;
				destaddr.net=faddr.net;
				destaddr.node=faddr.node;
				l+=6;
				while(l<length && fbuf[l]!=SP) l++;
				faddr=atofaddr(fbuf+l+1);
				origaddr.zone=faddr.zone;
				origaddr.net=faddr.net;
				origaddr.node=faddr.node; }

			else if(!strncmp(fbuf+l+1,"MSGID:",6)) {
				l+=7;
				while(l<length && fbuf[l]<=SP) l++;
				m=l;
				while(m<length && fbuf[m]!=CR) m++;
				while(m && fbuf[m-1]<=SP) m--;
				if(m>l)
					smb_hfield(&msg,FIDOMSGID,m-l,fbuf+l); }

			else if(!strncmp(fbuf+l+1,"REPLY:",6)) {
				l+=7;
				while(l<length && fbuf[l]<=SP) l++;
				m=l;
				while(m<length && fbuf[m]!=CR) m++;
				while(m && fbuf[m-1]<=SP) m--;
				if(m>l)
					smb_hfield(&msg,FIDOREPLYID,m-l,fbuf+l); }

			else if(!strncmp(fbuf+l+1,"FLAGS:",6)) {
				l+=7;
				while(l<length && fbuf[l]<=SP) l++;
				m=l;
				while(m<length && fbuf[m]!=CR) m++;
				while(m && fbuf[m-1]<=SP) m--;
				if(m>l)
					smb_hfield(&msg,FIDOFLAGS,m-l,fbuf+l); }

			else if(!strncmp(fbuf+l+1,"PATH:",5)) {
				l+=6;
				while(l<length && fbuf[l]<=SP) l++;
				m=l;
				while(m<length && fbuf[m]!=CR) m++;
				while(m && fbuf[m-1]<=SP) m--;
				if(m>l)
					smb_hfield(&msg,FIDOPATH,m-l,fbuf+l); }

			else if(!strncmp(fbuf+l+1,"PID:",4)) {
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
			if(done && cr && !strncmp(fbuf+l,"SEEN-BY:",8)) {
				l+=8;
				while(l<length && fbuf[l]<=SP) l++;
				m=l;
				while(m<length && fbuf[m]!=CR) m++;
				while(m && fbuf[m-1]<=SP) m--;
				if(m>l)
					smb_hfield(&msg,FIDOSEENBY,m-l,fbuf+l);
				while(l<length && fbuf[l]!=CR) l++;
				continue;  }  /* skip the CR */
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
					p=strchr(fbuf+l+11,CR); 	 /* find carriage return */
					while(p && *p!='(') p--;     /* rewind to '(' */
					if(p)
						origaddr=atofaddr(p+1); 	/* get orig address */
					orig=done=1; }
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

	for(l=0,crc=0xffffffff;l<bodylen;l++)
		crc=ucrc32(sbody[l],crc);
	crc=~crc;

	j=smb_addcrc(status.max_crcs,crc,10);
	if(j) {
		if(j==1) {
			printf("\nDuplicate message\n");
			smb_freemsgmem(msg);
			FREE(fbuf);
			FREE(sbody);
			FREE(stail);
			continue; }
		printf("smb_addcrc returned %d\n",j);
        exit(1); }

	while(taillen && stail[taillen-1]<=SP)	/* trim all garbage off the tail */
		taillen--;

	net=NET_FIDO;	/* Record origin address */
	smb_hfield(&msg,SENDERNETTYPE,sizeof(ushort),&net);
	smb_hfield(&msg,SENDERNETADDR,sizeof(fidoaddr_t),&origaddr);

	if(!orig) { 	/* No origin line means NetMail, so add dest addr */
		smb_hfield(&msg,RECIPIENTNETTYPE,sizeof(ushort),&net);
		smb_hfield(&msg,RECIPIENTNETADDR,sizeof(fidoaddr_t),&destaddr); }

	if(smb_open_da(10)) {
		printf("error opening %s.SDA\n",smb_file);
        exit(1); }
	l=bodylen+2;
	if(taillen)
		l+=(taillen+2);
	msg.hdr.offset=smb_fallocdat(l,1);
	fclose(sda_fp);
	if(msg.hdr.offset && msg.hdr.offset<1L) {
		printf("error %ld allocating records\r\n",msg.hdr.offset);
        exit(1); }
	fseek(sdt_fp,msg.hdr.offset,SEEK_SET);
    xlat=XLAT_NONE;
	fwrite(&xlat,2,1,sdt_fp);
	l=ftell(sdt_fp);
	fwrite(sbody,SDT_BLOCK_LEN,smb_datblocks(bodylen),sdt_fp);
	if(taillen) {
		fseek(sdt_fp,l+bodylen,SEEK_SET);
		fwrite(&xlat,2,1,sdt_fp);
		fwrite(stail,SDT_BLOCK_LEN,smb_datblocks(taillen),sdt_fp); }
	fflush(sdt_fp);
	FREE(fbuf);
	FREE(sbody);
	FREE(stail);

	smb_dfield(&msg,TEXT_BODY,bodylen+2);
	if(taillen)
		smb_dfield(&msg,TEXT_TAIL,taillen+2);

	smb_addmsghdr(&msg,&status,1,10);
	smb_freemsgmem(msg); }

printf("\n%u messages converted.\n",found);
return(0);
}

