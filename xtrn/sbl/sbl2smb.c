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

#ifdef _WIN32
	#include <windows.h>
	#include <io.h>		/* access */
	#include <share.h>
#endif

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "genwrap.h"
#include "filewrap.h"
#include "dirwrap.h"
#include "sbbsdefs.h"
#include "smblib.h"
#include "sbldefs.h"

smb_t		smb;
char		revision[16];

/****************************************************************************/
/* Truncates white-space chars off end of 'str'								*/
/****************************************************************************/
static void truncsp(char *str)
{
	uint c;

	c=strlen(str);
	while(c && (uchar)str[c-1]<=SP) c--;
	str[c]=0;
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
char* unixtodstr(time_t unix_time, char *str)
{
	struct tm* tm;

	if(!unix_time)
		strcpy(str,"00/00/00");
	else {
		tm=gmtime(&unix_time);
		if(tm==NULL) {
			strcpy(str,"00/00/00");
			return(str);
		}
		if(tm->tm_mon>11) {	  /* DOS leap year bug */
			tm->tm_mon=0;
			tm->tm_year++; }
		if(tm->tm_mday>31)
			tm->tm_mday=1;
		sprintf(str,"%02u/%02u/%02u",tm->tm_mday,tm->tm_mon+1
			,TM_YEAR(tm->tm_year));
	}
	return(str);
}



#define BUF_LEN (32*1024)

int main(int argc, char **argv)
{
	uchar	str[128],tmp[128],*buf,*p,software[128];
	int 	i,file;
	ushort	xlat;
	long	length;
	ulong	offset;
	time_t	last,now;
	bbs_t	bbs;
	smbmsg_t msg;
	FILE	*stream;

	sscanf("$Revision$" + 11, "%s", revision);

	fprintf(stderr,"\nSBL2SMB v2.%s-%s - Write SBL to SMB - Coyright 2002 "
		"Rob Swindell\n\n",revision,PLATFORM_DESC);
	if(argc<3) {
		fprintf(stderr,"usage: sbl2smb <sbl.dab> <smb_file> [/s:software]\n\n");
		fprintf(stderr,"ex: sbl2smb /sbbs/xtrn/sbl/sbl.dab "
			"/sbbs/data/subs/syncdata\n");
		return(1); 
	}

	if((buf = malloc(BUF_LEN)) == NULL) {
		fprintf(stderr,"!malloc failure\n");
		return(2);
	}
	now=time(NULL);

	software[0]=0;
	if(argc>3 && !strnicmp(argv[3],"/S:",3))
		SAFECOPY(software,argv[3]+3);

	SAFECOPY(smb.file,argv[2]);

	SAFECOPY(str,argv[1]);
	if((file=sopen(str,O_RDWR|O_BINARY,SH_DENYNO))==-1) {
		printf("error opening %s\n",str);
		return(1); }
	if((stream=fdopen(file,"rb"))==NULL) {
		printf("error fdopening %s\n",str);
		return(1); }
	strcpy(tmp,str);
	p=strrchr(tmp,'.');
	if(p) {
		(*p)=0;
		strcat(tmp,"2smb.dab");
		if((file=open(tmp,O_RDWR|O_BINARY|O_CREAT,S_IWRITE|S_IREAD))==-1) {
			printf("error opening %s\n",str);
			return(1); }
		if(read(file,&last,sizeof(time_t))!=sizeof(time_t))
			last=0;
		lseek(file,0L,SEEK_SET);
		write(file,&now,sizeof(now));
		close(file); }

	sprintf(str,"%s.shd",smb.file);
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
		if(bbs.total_numbers<1		/* corrupted? */
			|| bbs.total_numbers>MAX_NUMBERS)
			continue;
		truncsp(bbs.name);
		if(bbs.name[0]<=' '			/* corrupted? */
			|| bbs.misc&FROM_SMB
			|| (bbs.updated<last && bbs.created<last 
				&& bbs.verified<last))
			continue;
		if(software[0] && strnicmp(software,bbs.software,strlen(software)))
			continue;

		truncsp(bbs.user);
		if(bbs.user[0]<=' ')		/* corrupted? */
			continue;
		truncsp(bbs.software);
		if(bbs.software[0]<=' ')	/* corrupted? */
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
			truncsp(bbs.sysop[i]);
			sprintf(str,"%-15.15s%s\r\n"
				,"Sysop:",bbs.sysop[i]);
			strcat(buf,str); }

		strcat(buf,"\r\n");

		truncsp(bbs.sysop_email);
		if(bbs.sysop_email[0]) {
			sprintf(str,"%-15.15s%s\r\n"
				,"E-mail:",bbs.sysop_email);
			strcat(buf,str);
		}

		truncsp(bbs.web_url);
		if(bbs.web_url[0]) {
			sprintf(str,"%-15.15s%s\r\n"
				,"Web-site:",bbs.web_url);
			strcat(buf,str);
		}

		strcat(buf,"\r\n");

		for(i=0;i<bbs.total_numbers;i++) {
			truncsp(bbs.number[i].modem.number);
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
			truncsp(bbs.number[i].modem.location);
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

		sprintf(buf+strlen(buf),"\r\n--- SBL2SMB 2.%s-%s",revision,PLATFORM_DESC);

		length=strlen(buf);   /* +2 for translation string */

		if(smb.status.attr&SMB_HYPERALLOC)
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
		msg.hdr.when_written.time=now;
		msg.hdr.when_imported.time=now;
    
		msg.hdr.offset=offset;

		strcpy(str,"SBL");
		i=smb_hfield(&msg,RECIPIENT,(ushort)strlen(str),str);
		if(i) {
			printf("smb_hfield returned %d\n",i);
			smb_freemsgdat(&smb,offset,length,1);
			exit(1); }
		strlwr(str);
		msg.idx.to=crc16(str);

		strcpy(str,bbs.user);
		i=smb_hfield(&msg,SENDER,(ushort)strlen(str),str);
		if(i) {
			printf("smb_hfield returned %d\n",i);
			smb_freemsgdat(&smb,offset,length,1);
			exit(1); }
		strlwr(str);
		msg.idx.from=crc16(str);

		strcpy(str,bbs.name);
		i=smb_hfield(&msg,SUBJECT,(ushort)strlen(str),str);
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

		i=smb_addmsghdr(&smb,&msg,smb.status.attr&SMB_HYPERALLOC);
		if(i) {
			printf("smb_addmsghdr returned %d\n",i);
			smb_freemsgdat(&smb,offset,length,1);
			exit(1); }
		smb_freemsgmem(&msg); }
	return(0);
}

/* End of SBL2SMB.C */
