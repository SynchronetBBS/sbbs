/* smb2sbl.c */

/* Scans SMB message base for messages to "SBL" and adds them to the SBL    */

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

#include <time.h>		/* struct tm */
#include <fcntl.h>		/* O_RDWR */
#include <sys/stat.h>	/* S_IWRITE */
#include <ctype.h>		/* isdigit */
#include <stdlib.h>		/* atoi */
#include <string.h>
#include "smblib.h"
#include "sbldefs.h"
#include "genwrap.h"	/* PLATFORM_DESC */
#include "dirwrap.h"	/* fexist/flength */
#include "filewrap.h"	/* O_BINARY */

smb_t		smb;
char		revision[16];

char *loadmsgtxt(smbmsg_t msg, int tails)
{
	char	*buf=NULL,*lzhbuf;
	ushort	xlat;
	int 	i,lzh;
	long	l=0,lzhlen,length;

	for(i=0;i<msg.hdr.total_dfields;i++) {
		if(!(msg.dfield[i].type==TEXT_BODY
			|| (tails && msg.dfield[i].type==TEXT_TAIL)))
			continue;
		fseek(smb.sdt_fp,msg.hdr.offset+msg.dfield[i].offset
			,SEEK_SET);
		fread(&xlat,2,1,smb.sdt_fp);
		lzh=0;
		if(xlat==XLAT_LZH) {
			lzh=1;
			fread(&xlat,2,1,smb.sdt_fp); }
		if(xlat!=XLAT_NONE) 		/* no other translations supported */
			continue;

		length=msg.dfield[i].length-2;
		if(lzh) {
			length-=2;
			if((lzhbuf=MALLOC(length))==NULL) {
				printf("ERR_ALLOC lzhbuf of %lu\n",length);
				return(buf); }
			fread(lzhbuf,1,length,smb.sdt_fp);
			lzhlen=*(long *)lzhbuf;
			if((buf=REALLOC(buf,l+lzhlen+3))==NULL) {
				FREE(lzhbuf);
				printf("ERR_ALLOC lzhoutbuf of %l\n",l+lzhlen+1);
				return(buf); }
			lzh_decode(lzhbuf,length,buf+l);
			FREE(lzhbuf);
			l+=lzhlen; }
		else {
			if((buf=REALLOC(buf,l+msg.dfield[i].length+3))==NULL) {
				printf("ERR_ALLOC of %lu\n",l+msg.dfield[i].length+1);
				return(buf); }
			l+=fread(buf+l,1,length,smb.sdt_fp); }
		buf[l]=CR;
		l++;
		buf[l]=LF;
		l++;
		buf[l]=0; }
	return(buf);
}

/****************************************************************************/
/* Converts a date string in format MM/DD/YY into unix time format			*/
/****************************************************************************/
time_t dstrtounix(char *instr)
{
	char*	p;
	char*	day;
	char	str[16];
	struct tm tm;

	if(!instr[0] || !strncmp(instr,"00/00/00",8))
		return(0);

	if(isdigit(instr[0]) && isdigit(instr[1])
		&& isdigit(instr[3]) && isdigit(instr[4])
		&& isdigit(instr[6]) && isdigit(instr[7]))
		p=instr;	/* correctly formatted */
	else {
		p=instr;	/* incorrectly formatted */
		while(*p && isdigit(*p)) p++;
		if(*p==0)
			return(0);
		p++;
		day=p;
		while(*p && isdigit(*p)) p++;
		if(*p==0)
			return(0);
		p++;
		sprintf(str,"%02u/%02u/%02u"
			,atoi(instr)%100,atoi(day)%100,atoi(p)%100);
		p=str;
	}

	memset(&tm,0,sizeof(tm));
	tm.tm_year=((p[6]&0xf)*10)+(p[7]&0xf);
	if (tm.tm_year<Y2K_2DIGIT_WINDOW)
		tm.tm_year+=100;
	tm.tm_mon=((p[0]&0xf)*10)+(p[1]&0xf);
	tm.tm_mday=((p[3]&0xf)*10)+(p[4]&0xf); 
	if (tm.tm_mon)
		tm.tm_mon--;	/* zero-based month field */
	return(mktime(&tm));
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
/* Truncates white-space chars off end of 'str'								*/
/****************************************************************************/
static void truncsp(char *str)
{
	uint c;

	c=strlen(str);
	while(c && (uchar)str[c-1]<=SP) c--;
	str[c]=0;
}

int main(int argc, char **argv)
{
	uchar	str[128],*buf;
	int 	i,file,sysop,number,network,terminal,desc;
	ulong	l,last,high;
	ushort	sbl;
	bbs_t	bbs;
	smbmsg_t msg;
	FILE	*stream;

	sscanf("$Revision$" + 11, "%s", revision);

	fprintf(stderr,"\nSMB2SBL v2.%s-%s - Updates SBL via SMB - Copyright 2002 "
		"Rob Swindell\n\n",revision,PLATFORM_DESC);

#if 0
	if(putenv("TZ=UCT0"))
		fprintf(stderr,"!putenv() FAILED\n");
	tzset();
#endif

	if(argc<3) {
		fprintf(stderr,"usage: smb2sbl <smb_file> <sbl.dab>\n\n");
		fprintf(stderr,"ex: smb2sbl /sbbs/data/subs/syncdata "
			"/sbbs/xtrn/sbl/sbl.dab\n");
		return(1); 
	}

	SAFECOPY(smb.file,argv[1]);

	SAFECOPY(str,argv[2]);
	if((file=sopen(str,O_RDWR|O_BINARY|O_CREAT,SH_DENYNO))==-1) {
		printf("error opening %s\n",str);
		return(1); }
	if((stream=fdopen(file,"r+b"))==NULL) {
		printf("error fdopening %s\n",str);
		return(1); }
	setvbuf(stream,NULL,_IOFBF,4096);

	sprintf(str,"%s.SBL",smb.file);
	if((file=open(str,O_RDWR|O_BINARY|O_CREAT,S_IWRITE|S_IREAD))==-1) {
		printf("error opening %s\n",str);
		return(1); }
	if(read(file,&last,4)!=4)
		last=0;
	high=last;

	sprintf(str,"%s.SHD",smb.file);
	if(!fexist(str)) {
		printf("%s doesn't exist\n",smb.file);
		return(0); }
	sprintf(str,"%s.SID",smb.file);
	if(!flength(str)) {
		printf("%s is empty\n",smb.file);
		return(0); }
	fprintf(stderr,"Opening %s\n",smb.file);
	smb.retry_time=30;
	if((i=smb_open(&smb))!=0) {
		printf("smb_open returned %d\n",i);
		return(1); }

	sbl=crc16("sbl");

	if((i=smb_locksmbhdr(&smb))!=0) {				/* Be sure noone deletes or */
		printf("Error locking %d\n",i);             /* adds while we're reading */
		return(1); }

	while(!feof(smb.sid_fp)) {
		if(!fread(&msg.idx,sizeof(idxrec_t),1,smb.sid_fp))
			break;
		fprintf(stderr,"\r%lu  ",msg.idx.number);
		if(msg.idx.number<=last || msg.idx.to!=sbl)
			continue;
		high=msg.idx.number;
		if((i=smb_lockmsghdr(&smb,&msg))!=0) {
			printf("\7Error %d locking msg #%lu\n",i,msg.idx.number);
			continue; }
		if((i=smb_getmsghdr(&smb,&msg))!=0) {
			smb_unlockmsghdr(&smb,&msg);
			printf("\7Error %d reading msg #%lu\n",i,msg.idx.number);
			continue; }
		smb_unlockmsghdr(&smb,&msg);
		if(!msg.from_net.type			/* ignore local message */
			|| msg.from[0]<=' '			/* corrupted? */
			|| msg.subj[0]<=' '			/* corrupted */
			) {
			smb_freemsgmem(&msg);
			continue; 
		}

		printf("\nMessage #%lu by %s on %.24s\n"
			,msg.hdr.number,msg.from,ctime((time_t*)&msg.hdr.when_written.time));

		truncsp(msg.subj);
		if(!msg.subj[0]) {
			smb_freemsgmem(&msg);
			continue; }
		fprintf(stderr,"Searching for %s...",msg.subj);
		fseek(stream,0L,SEEK_SET);
		memset(&bbs,0,sizeof(bbs_t));
		while(1) {
			l=ftell(stream);
			if(!fread(&bbs,sizeof(bbs_t),1,stream)) {
				memset(&bbs,0,sizeof(bbs_t));
				break; }
			if(msg.subj[0] && !stricmp(bbs.name,msg.subj)) {
				fseek(stream,l,SEEK_SET);
				break; } }
		fprintf(stderr,"\n");
		if(bbs.name[0] && strnicmp(bbs.user,msg.from,25)) {
			printf("%s didn't create the entry for %s\n",msg.from,msg.subj);
			smb_freemsgmem(&msg);
			continue; }
		if(!bbs.name[0]) {
			fprintf(stderr,"Searching for unused record...");
			fseek(stream,0L,SEEK_SET);
			while(1) {					/* Find deleted record */
				l=ftell(stream);
				if(!fread(&bbs,sizeof(bbs_t),1,stream))
					break;
				if(!bbs.name[0]) {
					fseek(stream,l,SEEK_SET);
					break; } }
			fprintf(stderr,"\n");
			memset(&bbs,0,sizeof(bbs_t));
			bbs.created=time(NULL);
			if(!bbs.birth)
				bbs.birth=bbs.created;
			sprintf(bbs.user,"%-.25s",msg.from); }
		sprintf(bbs.name,"%-.25s",msg.subj);
		bbs.updated=time(NULL);
		bbs.misc|=FROM_SMB;
		sprintf(bbs.userupdated,"%-.25s",msg.from);
		buf=loadmsgtxt(msg,0);
		sysop=number=network=terminal=desc=0;
		l=0;
		while(buf[l]) {
			while(buf[l] && buf[l]<=SP) 		/* Find first text on line */
				l++;
			if(!strnicmp(buf+l,"NAME:",5)) {
				l+=5;
				while(buf[l] && buf[l]<=SP && buf[l]!=CR)
					l++;
				sprintf(bbs.name,"%-.25s",buf+l);
				truncsp(bbs.name); }
			if(!strnicmp(buf+l,"BIRTH:",6)) {
				l+=6;
				while(buf[l] && buf[l]<=SP && buf[l]!=CR)
					l++;
				bbs.birth=dstrtounix(buf+l); }
			if(!strnicmp(buf+l,"SOFTWARE:",9)) {
				l+=9;
				while(buf[l] && buf[l]<=SP && buf[l]!=CR)
					l++;
				sprintf(bbs.software,"%-.15s",buf+l);
				truncsp(bbs.software); }
			if(!strnicmp(buf+l,"WEB-SITE:",9)) {
				l+=9;
				while(buf[l] && buf[l]<=SP && buf[l]!=CR)
					l++;
				sprintf(bbs.web_url,"%-.60s",buf+l);
				truncsp(bbs.web_url); }
			if(!strnicmp(buf+l,"E-MAIL:",7)) {
				l+=7;
				while(buf[l] && buf[l]<=SP && buf[l]!=CR)
					l++;
				sprintf(bbs.sysop_email,"%-.60s",buf+l);
				truncsp(bbs.sysop_email); }

			if(!strnicmp(buf+l,"SYSOP:",6)) {
				l+=6;
				while(buf[l] && buf[l]<=SP && buf[l]!=CR)
					l++;
				sprintf(bbs.sysop[sysop],"%-.25s",buf+l);
				truncsp(bbs.sysop[sysop]);
				if(sysop<MAX_SYSOPS-1)
					sysop++; }
			if(!strnicmp(buf+l,"NUMBER:",7)) {
				l+=7;
				while(buf[l] && buf[l]<=SP && buf[l]!=CR)
					l++;
				sprintf(bbs.number[number].modem.number,"%-.28s",buf+l);
				truncsp(bbs.number[number].modem.number);
				if(number<MAX_NUMBERS-1)
					number++; }
			if(!strnicmp(buf+l,"MODEM:",6)) {
				l+=6;
				while(buf[l] && buf[l]<=SP && buf[l]!=CR)
					l++;
				i=number;
				if(i) i--;
				sprintf(bbs.number[i].modem.desc,"%-.15s",buf+l);
				truncsp(bbs.number[i].modem.desc); }
			if(!strnicmp(buf+l,"LOCATION:",9)) {
				l+=9;
				while(buf[l] && buf[l]<=SP && buf[l]!=CR)
					l++;
				i=number;
				if(i) i--;
				sprintf(bbs.number[i].modem.location,"%-.30s",buf+l);
				truncsp(bbs.number[i].modem.location); }
			if(!strnicmp(buf+l,"MINRATE:",8)) {
				l+=8;
				while(buf[l] && buf[l]<=SP && buf[l]!=CR)
					l++;
				i=number;
				if(i) i--;
				bbs.number[i].modem.min_rate=atoi(buf+l); }
			if(!strnicmp(buf+l,"MAXRATE:",8)) {
				l+=8;
				while(buf[l] && buf[l]<=SP && buf[l]!=CR)
					l++;
				i=number;
				if(i) i--;
				bbs.number[i].modem.max_rate=atoi(buf+l); }
			if(!strnicmp(buf+l,"NETWORK:",8)) {
				l+=8;
				while(buf[l] && buf[l]<=SP && buf[l]!=CR)
					l++;
				sprintf(bbs.network[network],"%-.15s",buf+l);
				truncsp(bbs.network[network]);
				if(network<MAX_NETS-1)
					network++; }
			if(!strnicmp(buf+l,"ADDRESS:",8)) {
				l+=8;
				while(buf[l] && buf[l]<=SP && buf[l]!=CR)
					l++;
				i=network;
				if(i) i--;
				sprintf(bbs.address[i],"%-.25s",buf+l);
				truncsp(bbs.address[i]); }
			if(!strnicmp(buf+l,"TERMINAL:",9)) {
				l+=9;
				while(buf[l] && buf[l]<=SP && buf[l]!=CR)
					l++;
				sprintf(bbs.terminal[terminal],"%-.15s",buf+l);
				truncsp(bbs.terminal[terminal]);
				if(terminal<MAX_TERMS-1)
					terminal++; }
			if(!strnicmp(buf+l,"DESC:",5)) {
				l+=5;
				while(buf[l] && buf[l]<=SP && buf[l]!=CR)
					l++;
				sprintf(bbs.desc[desc],"%-.50s",buf+l);
				truncsp(bbs.desc[desc]);
				if(desc<4)
					desc++; }

			if(!strnicmp(buf+l,"MEGS:",5)) {
				l+=5;
				while(buf[l] && buf[l]<=SP && buf[l]!=CR)
					l++;
				bbs.megs=atol(buf+l); }
			if(!strnicmp(buf+l,"MSGS:",5)) {
				l+=5;
				while(buf[l] && buf[l]<=SP && buf[l]!=CR)
					l++;
				bbs.msgs=atol(buf+l); }
			if(!strnicmp(buf+l,"FILES:",6)) {
				l+=6;
				while(buf[l] && buf[l]<=SP && buf[l]!=CR)
					l++;
				bbs.files=atol(buf+l); }
			if(!strnicmp(buf+l,"NODES:",6)) {
				l+=6;
				while(buf[l] && buf[l]<=SP && buf[l]!=CR)
					l++;
				bbs.nodes=atoi(buf+l); }
			if(!strnicmp(buf+l,"USERS:",6)) {
				l+=6;
				while(buf[l] && buf[l]<=SP && buf[l]!=CR)
					l++;
				bbs.users=atoi(buf+l); }
			if(!strnicmp(buf+l,"SUBS:",5)) {
				l+=5;
				while(buf[l] && buf[l]<=SP && buf[l]!=CR)
					l++;
				bbs.subs=atoi(buf+l); }
			if(!strnicmp(buf+l,"DIRS:",5)) {
				l+=5;
				while(buf[l] && buf[l]<=SP && buf[l]!=CR)
					l++;
				bbs.dirs=atoi(buf+l); }
			if(!strnicmp(buf+l,"XTRNS:",6)) {
				l+=6;
				while(buf[l] && buf[l]<=SP && buf[l]!=CR)
					l++;
				bbs.xtrns=atoi(buf+l); }
			while(buf[l] && buf[l]>=SP) {	 /* Go to end of line */
				putchar(buf[l]);
				l++; }
			printf("\n"); }
	//	if(bbs.total_sysops<sysop)
			bbs.total_sysops=sysop;
	//	if(bbs.total_networks<network)
			bbs.total_networks=network;
	//	if(bbs.total_terminals<terminal)
			bbs.total_terminals=terminal;
	//	if(bbs.total_numbers<number)
			bbs.total_numbers=number;
		fwrite(&bbs,sizeof(bbs_t),1,stream);
		FREE(buf);
		smb_freemsgmem(&msg);
		}
	lseek(file,0L,SEEK_SET);
	write(file,&high,4);
	close(file);
	return(0);
}

