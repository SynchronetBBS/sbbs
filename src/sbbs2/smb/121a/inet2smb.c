/* Converts RFC #822 Internet Text Messages to SMB format */

#include <dos.h>
#include "smblib.h"

/******************************************************************************
 Chops off the ":" and all spaces and tabs after it for the message headers
******************************************************************************/
char *header(char *instr)
{
	char str[256],*p;
	int i=0;

	p=strstr(instr,":");
	++p;
	while(*p==SP || *p==TAB)
		++p;
	while(i<strlen(p)) {
		if(p[i]==LF || p[i]==CR) {
			p[i]=0;
			break; }
		++i; }
	return p;
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

/******************************************************************************
 Converts ASCII time in RFC 822 or RFC 1036 format to SMB when_t format
******************************************************************************/
when_t imsgtime(char *str)
{
	char month[25],zone[25],*p;
	struct date date;
	struct time t;
	when_t when;

when.zone=0;	/* Default to UT */

if(isdigit(str[1])) {	/* Regular format: "01 Jan 86 0234 GMT" */
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
	t.ti_hour=atoi(str+10);
	t.ti_min=atoi(str+12);
	t.ti_sec=0;
	p=str+13; }

else {					/* USENET format: "Mon,  1 Jan 86 02:34:00 GMT" */
	date.da_day=atoi(str+5);
	sprintf(month,"%3.3s",str+8);
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
	date.da_year=atoi(str+12);
	if(date.da_year<100)
		date.da_year+=1900;
    p=str+12;
	while(*p!=SP) p++;	/* skip the year */
	while(*p==SP) p++;	/* and white space */
	t.ti_hour=atoi(p);
	t.ti_min=atoi(p+3);
	t.ti_sec=atoi(p+6);
	p=str+22; }

when.time=dostounix(&date,&t);

while(*p!=SP) p++;	/* skip the time */
while(*p==SP) p++;	/* and white space */

sprintf(zone,"%-.5s",p);

/* Get the zone */
if(!strcmpi(zone,"GMT") || !strcmpi(zone,"UT"))
	when.zone=0;
else if(!strcmpi(zone,"EST"))
	when.zone=EST;
else if(!strcmpi(zone,"EDT"))
	when.zone=EDT;
else if(!strcmpi(zone,"MST"))
	when.zone=MST;
else if(!strcmpi(zone,"MDT"))
	when.zone=MDT;
else if(!strcmpi(zone,"CST"))
	when.zone=CST;
else if(!strcmpi(zone,"CDT"))
	when.zone=CDT;
else if(!strcmpi(zone,"PST"))
	when.zone=PST;
else if(!strcmpi(zone,"PDT"))
	when.zone=PDT;

else if(isalpha(zone[0]) && !zone[1]) { /* Military single character zone */
	zone[0]&=0xdf;	/* convert to upper case */
	if(zone[0]>='A' && zone[0]<='I')
		when.zone='@'-zone[0];
	else if(zone[0]>='K' && zone[0]<='M')   /* J is not used */
		when.zone='A'-zone[0];
	else if(zone[0]>='N' && zone[0]<='Y')
		when.zone=zone[0]-'M'; }

else if((zone[0]=='+' || zone[0]=='-') && isdigit(zone[1])) { /* Literal */
	when.zone=(((zone[1]&0xf)*10)+(zone[2]&0xf))*60;	/* Hours */
	when.zone+=atoi(zone+3);							/* Minutes */
	if(zone[0]=='-')                                    /* Negative? */
		when.zone=-when.zone; }

return(when);
}

void main(int argc, char **argv)
{
	FILE *stream;
	char str[256]={NULL},filespec[256],file[256],*p,*abuf;
	ushort i,xlat,net;
	ulong l,length,datlen;
	struct ffblk ff;
	smbmsg_t	msg;
    smbstatus_t status;

	if(argc<2) {
		printf("Usage: INET2SMB <file_spec> <smb_name>\r\n");
		exit(1); }

	strcpy(filespec,argv[1]);
	strcpy(smb_file,argv[2]);
	strupr(smb_file);

	smb_open(10);
	if(!filelength(fileno(shd_fp)))
		smb_create(2000,2000,0,0,10);

	i=findfirst(filespec,&ff,0);
	while(!i) {
		sprintf(file,"%s",ff.ff_name);
		if((stream=fopen(file,"rb"))==NULL) {
			printf("Error opening %s\r\n",file);
			break; }

		memset(&msg,0,sizeof(smbmsg_t));
		memcpy(msg.hdr.id,"SHD\x1a",4);
		msg.hdr.version=SMB_VERSION;
		msg.hdr.when_imported.time=time(NULL);
		msg.hdr.when_imported.zone=PST;  /* set to local time zone */

		while(str[0]!=CR && str[0]!=LF) {
			fgets(str,81,stream);
			if(!strnicmp(str,"Resent-",6)) {
				p=strstr(str,"Resent-");
				sprintf(str,"%s",p); }
			if(!strnicmp(str,"Return-Path",11)
				|| !strnicmp(str,"Path",4)) {
				strcpy(str,header(str));
				smb_hfield(&msg,REPLYTO,strlen(str),str); }
			else if(!strnicmp(str,"Date",4)) {
				strcpy(str,header(str));
				msg.hdr.when_written=imsgtime(str); }
			else if(!strnicmp(str,"From",4) || !strnicmp(str,"Sender",6)) {
				strcpy(str,header(str));
				p=strstr(str," (");
				if(p) {
					*p=0;
					*(p+1)=0;
					p+=2;
					smb_hfield(&msg,SENDERNETADDR,strlen(str),str);
					sprintf(str,"%s",p);
					p=strstr(str,")");
					*p=0; }
				smb_hfield(&msg,SENDER,strlen(str),str);
				strlwr(str);
				msg.idx.from=crc16(str); }
			else if(!strnicmp(str,"Subject",7)) {
				strcpy(str,header(str));
				smb_hfield(&msg,SUBJECT,strlen(str),str);
				strlwr(str);
				msg.idx.subj=crc16(str); }
			else if(!strnicmp(str,"To",2)) {
				strcpy(str,header(str));
				smb_hfield(&msg,RECIPIENT,strlen(str),str);
				strlwr(str);
				msg.idx.to=crc16(str); }

			/* Following are optional fields */

			else if(!strnicmp(str,"Message-ID",10)) {
				strcpy(str,header(str));
				smb_hfield(&msg,RFC822MSGID,strlen(str),str); }
			else if(!strnicmp(str,"In-Reply-To",11)) {
				strcpy(str,header(str));
				smb_hfield(&msg,RFC822REPLYID,strlen(str),str); }

			/* User defined extension field */

			else if(!strnicmp(str,"X-",2)) {
				if(strstr(str,"To")) {
					strcpy(str,header(str));
					smb_hfield(&msg,RECIPIENT,strlen(str),str);
					strlwr(str);
					msg.idx.to=crc16(str); }
				else
					smb_hfield(&msg,RFC822HEADER,strlen(str),str); }
			else
				smb_hfield(&msg,RFC822HEADER,strlen(str),str); }

		l=ftell(stream);
		fseek(stream,0L,SEEK_END);
		length=ftell(stream)-l;
		fseek(stream,l,SEEK_SET);

		if((abuf=(char *)MALLOC(length))==NULL) {
			printf("alloc error\n");
			exit(1); }
		fread(abuf,length,1,stream);
		fclose(stream);

		net=NET_INTERNET;
		smb_hfield(&msg,SENDERNETTYPE,sizeof(ushort),&net);

		if(smb_open_da(10)) {
			printf("error opening %s.SDA\n",smb_file);
			exit(1); }
		msg.hdr.offset=smb_fallocdat(length+2,1);
		fclose(sda_fp);
		if(msg.hdr.offset && msg.hdr.offset<1L) {
			printf("error %ld allocating records\r\n",msg.hdr.offset);
			exit(1); }
		fseek(sdt_fp,msg.hdr.offset,SEEK_SET);
		xlat=XLAT_NONE;
		fwrite(&xlat,2,1,sdt_fp);
		fwrite(abuf,SDT_BLOCK_LEN,smb_datblocks(length),sdt_fp);
		FREE(abuf);
		fflush(sdt_fp);

		smb_dfield(&msg,TEXT_BODY,length+2);

		smb_addmsghdr(&msg,&status,1,10);
		smb_freemsgmem(msg);
		i=findnext(&ff); }
}
