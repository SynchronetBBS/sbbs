/* sbl2smb.c */

/* Scans SBL database and posts any additions/updates into the an SMB base */

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
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout xtrn	*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#define  uint unsigned int

#include <dos.h>
#include "smblib.h"
#include "sbldefs.h"

smb_t		smb;
extern int	daylight=0;
extern long timezone=0L;

unsigned	_stklen=16000;

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
/* Converts unix time format (long - time_t) into a char str MM/DD/YY		*/
/****************************************************************************/
char *unixtodstr(time_t unix, char *str)
{
	struct time curtime;
	struct date date;

if(!unix)
	strcpy(str,"00/00/00");
else {
	unixtodos(unix,&date,&curtime);
	if((unsigned)date.da_mon>12) {	  /* DOS leap year bug */
		date.da_mon=1;
		date.da_year++; }
	if((unsigned)date.da_day>31)
		date.da_day=1;
	sprintf(str,"%02u/%02u/%02u",date.da_mon,date.da_day
		,date.da_year>=2000 ? date.da_year-2000 : date.da_year-1900); }
return(str);
}

#define BUF_LEN 8192

int main(int argc, char **argv)
{
	uchar	str[128],tmp[128],buf[BUF_LEN],*p,software[128];
	int 	i,file;
	ushort	xlat;
	long	length;
	ulong	offset;
	time_t	last,t;
	bbs_t	bbs;
	smbmsg_t msg;
	smbstatus_t status;
	FILE	*stream;

fprintf(stderr,"\nSBL2SMB v2.10 - Write SBL to SMB - Developed 1994-2000 "
	"Rob Swindell\n\n");
if(argc<3) {
	fprintf(stderr,"usage: sbl2smb <sbl.dab> <smb_file> [/s:software]\n\n");
	fprintf(stderr,"ex: sbl2smb c:\\sbbs\\xtrn\\sbl\\sbl.dab "
		"c:\\sbbs\\data\\subs\\syncdata /s:syn\n");
	return(1); }

software[0]=0;
if(argc>3 && !strnicmp(argv[3],"/S:",3))
	strcpy(software,argv[3]+3);

strcpy(smb.file,argv[2]);
strupr(smb.file);

strcpy(str,argv[1]);
strupr(str);
if((file=open(str,O_RDWR|O_BINARY|O_DENYNONE))==-1) {
	printf("error opening %s\n",str);
	return(1); }
if((stream=fdopen(file,"rb"))==NULL) {
	printf("error fdopening %s\n",str);
	return(1); }
strcpy(tmp,str);
p=strrchr(tmp,'.');
if(p) {
	(*p)=0;
	strcat(tmp,"2SMB.DAB");
	if((file=open(tmp,O_RDWR|O_BINARY|O_CREAT,S_IWRITE|S_IREAD))==-1) {
		printf("error opening %s\n",str);
		return(1); }
	t=time(NULL);
	if(read(file,&last,sizeof(time_t))!=sizeof(time_t))
		last=t;
	lseek(file,0L,SEEK_SET);
	write(file,&t,sizeof(time_t));
	close(file); }

sprintf(str,"%s.SHD",smb.file);
if(!fexist(str)) {
	printf("%s doesn't exist\n",smb.file);
	return(0); }
fprintf(stderr,"Opening %s\n",smb.file);
smb.retry_time=30;
if((i=smb_open(&smb))!=0) {
	printf("smb_open returned %d\n",i);
	return(1); }
i=smb_locksmbhdr(&smb);
if(i) {
	printf("smb_locksmbhdr returned %d\n",i);
	return(1); }
i=smb_getstatus(&smb);
smb_unlocksmbhdr(&smb);
if(i) {
	printf("smb_getstatus returned %d\n",i);
	return(1); }

while(!feof(stream)) {
	if(!fread(&bbs,sizeof(bbs_t),1,stream))
		break;
	if(!bbs.name[0] || bbs.misc&FROM_SMB
		|| (bbs.updated<last && bbs.created<last 
			&& bbs.verified<last))
		continue;
	if(software[0] && strnicmp(software,bbs.software,strlen(software)))
		continue;

	printf("%s\r\n",bbs.name);
	memset(buf,0,BUF_LEN);

	sprintf(str,"%-15.15s%s\r\n"
		,"Name:",bbs.name);
	strcat(buf,str);

	sprintf(str,"%-15.15s%s\r\n"
		,"Birth:",unixtodstr(bbs.birth,tmp));
    strcat(buf,str);

	sprintf(str,"%-15.15s%s\r\n"
		,"Software:",bbs.software);
    strcat(buf,str);

	for(i=0;i<bbs.total_sysops;i++) {
		sprintf(str,"%-15.15s%s\r\n"
			,"Sysop:",bbs.sysop[i]);
		strcat(buf,str); }

	strcat(buf,"\r\n");

	if(bbs.sysop_email[0]) {
		sprintf(str,"%-15.15s%s\r\n"
			,"E-mail:",bbs.sysop_email);
		strcat(buf,str);
	}

	if(bbs.web_url[0]) {
		sprintf(str,"%-15.15s%s\r\n"
			,"Web-site:",bbs.web_url);
		strcat(buf,str);
	}

	strcat(buf,"\r\n");

	for(i=0;i<bbs.total_numbers;i++) {
		sprintf(str,"%-15.15s%s\r\n"
			,"Number:",bbs.number[i].modem.number);
		strcat(buf,str);

		sprintf(str,"%-15.15s%u\r\n"
			,"MinRate:",bbs.number[i].modem.min_rate);
		strcat(buf,str);

		sprintf(str,"%-15.15s%u\r\n"
			,"MaxRate:",bbs.number[i].modem.max_rate);
		strcat(buf,str);

		if(bbs.number[i].modem.min_rate!=0xffff) {

			sprintf(str,"%-15.15s%s\r\n"
				,"Modem:",bbs.number[i].modem.desc);
            strcat(buf,str);
		}
		sprintf(str,"%-15.15s%s\r\n"
		   ,"Location:",bbs.number[i].modem.location);
		strcat(buf,str);
		if(i+1<bbs.total_numbers)
			strcat(buf,"\r\n"); }

	if(bbs.total_networks)
		strcat(buf,"\r\n");
	for(i=0;i<bbs.total_networks;i++) {
		sprintf(str,"%-15.15s%s\r\n"
			,"Network:",bbs.network[i]);
        strcat(buf,str);
		sprintf(str,"%-15.15s%s\r\n"
			,"Address:",bbs.address[i]);
		strcat(buf,str);
		if(i+1<bbs.total_networks)
			strcat(buf,"\r\n"); }

	strcat(buf,"\r\n");
	for(i=0;i<bbs.total_terminals;i++) {
		sprintf(str,"%-15.15s%s\r\n"
			,"Terminal:",bbs.terminal[i]);
		strcat(buf,str); }

	strcat(buf,"\r\n");
	sprintf(str,"%-15.15s%lu\r\n"
		,"Megs:",bbs.megs);
	strcat(buf,str);
	sprintf(str,"%-15.15s%lu\r\n"
		,"Msgs:",bbs.msgs);
    strcat(buf,str);
	sprintf(str,"%-15.15s%lu\r\n"
		,"Files:",bbs.files);
    strcat(buf,str);
	sprintf(str,"%-15.15s%u\r\n"
		,"Nodes:",bbs.nodes);
    strcat(buf,str);
	sprintf(str,"%-15.15s%u\r\n"
		,"Users:",bbs.users);
    strcat(buf,str);
	sprintf(str,"%-15.15s%u\r\n"
		,"Subs:",bbs.subs);
    strcat(buf,str);
	sprintf(str,"%-15.15s%u\r\n"
		,"Dirs:",bbs.dirs);
    strcat(buf,str);
	sprintf(str,"%-15.15s%u\r\n"
		,"Xtrns:",bbs.xtrns);
    strcat(buf,str);

	if(bbs.desc[0][0])
		strcat(buf,"\r\n");
	for(i=0;i<5;i++) {
		if(!bbs.desc[i][0])
			break;
		sprintf(str,"%-15.15s%s\r\n"
			,"Desc:",bbs.desc[i]);
		strcat(buf,str); }

	strcat(buf,"\r\n--- SBL2SMB v2.10");

	length=strlen(buf);   /* +2 for translation string */

	if(status.attr&SMB_HYPERALLOC)
		offset=smb_hallocdat(&smb);
	else {
		i=smb_open_da(&smb);
		if(i) {
			printf("smb_open_da returned %d\n",i);
			exit(1); }
		offset=smb_allocdat(&smb,length+2,1);
		fclose(smb.sda_fp); }

	fseek(smb.sdt_fp,offset,SEEK_SET);
	xlat=XLAT_NONE;
	fwrite(&xlat,2,1,smb.sdt_fp);
	fwrite(buf,length,1,smb.sdt_fp);
	length+=2;

	memset(&msg,0,sizeof(smbmsg_t));
	memcpy(msg.hdr.id,"SHD\x1a",4);
	msg.hdr.version=smb_ver();
	msg.hdr.when_written.time=time(NULL);
    
	msg.hdr.offset=offset;

	strcpy(str,"SBL");
	i=smb_hfield(&msg,RECIPIENT,strlen(str),str);
	if(i) {
		printf("smb_hfield returned %d\n",i);
		smb_freemsgdat(&smb,offset,length,1);
		exit(1); }
	strlwr(str);
	msg.idx.to=crc16(str);

	strcpy(str,bbs.user);
	i=smb_hfield(&msg,SENDER,strlen(str),str);
	if(i) {
		printf("smb_hfield returned %d\n",i);
		smb_freemsgdat(&smb,offset,length,1);
		exit(1); }
	strlwr(str);
	msg.idx.from=crc16(str);

	strcpy(str,bbs.name);
	i=smb_hfield(&msg,SUBJECT,strlen(str),str);
	if(i) {
		printf("smb_hfield returned %d\n",i);
		smb_freemsgdat(&smb,offset,length,1);
		exit(1); }
	strlwr(str);
	msg.idx.subj=crc16(str);

	i=smb_dfield(&msg,TEXT_BODY,length);
	if(i) {
		printf("smb_dfield returned %d\n",i);
		smb_freemsgdat(&smb,offset,length,1);
		exit(1); }

	i=smb_addmsghdr(&smb,&msg,status.attr&SMB_HYPERALLOC);
	if(i) {
		printf("smb_addmsghdr returned %d\n",i);
		smb_freemsgdat(&smb,offset,length,1);
		exit(1); }
	smb_freemsgmem(&msg); }
return(0);
}

/* End of SBL2SMB.C */
