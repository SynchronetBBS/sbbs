/* chksmb.c */

/* Synchronet message base (SMB) validity checker */

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
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

/* ANSI */
#include <stdio.h>	
#include <stdlib.h>		/* exit */
#include <string.h>		/* strrchr */
#include <time.h>		/* ctime */
#include <ctype.h>		/* toupper */

/* SMB-specific */
#include "genwrap.h"
#include "conwrap.h"	/* getch */
#include "dirwrap.h"	/* fexist */
#include "filewrap.h"	/* filelength */
#include "smblib.h"

/****************************************************************************/
/* Returns in 'string' a character representation of the number in l with   */
/* commas.																	*/
/****************************************************************************/
char *ultoac(ulong l, char *string)
{
	char str[256];
	signed char i,j,k;

	sprintf(str,"%lu",l);
	i=strlen(str)-1;
	j=i/3+1+i;
	string[j--]=0;
	for(k=1;i>-1;k++) {
		string[j--]=str[i--];
		if(j>0 && !(k%3))
			string[j--]=','; }
	return(string);
}

/****************************************************************************/
/* Returns an ASCII string for FidoNet address 'addr'                       */
/****************************************************************************/
char *faddrtoa(fidoaddr_t addr)
{
	static char str[25];
	char point[25];

	sprintf(str,"%hu:%hu/%hu",addr.zone,addr.net,addr.node);
	if(addr.point) {
		sprintf(point,".%u",addr.point);
		strcat(str,point); }
	return(str);
}

/****************************************************************************/
/* Truncates white-space chars off end of 'str'								*/
/****************************************************************************/
void truncsp(char *str)
{
	uint c;

	c=strlen(str);
	while(c && (uchar)str[c-1]<=SP) c--;
	str[c]=0;
}

char* DLLCALL strip_ctrl(char *str)
{
	char tmp[1024];
	int i,j;

	for(i=j=0;str[i] && j<sizeof(tmp)-1;i++)
		if(str[i]==CTRL_A && str[i+1]!=0)
			i++;
		else if((uchar)str[i]>=SP)
			tmp[j++]=str[i];
	tmp[j]=0;
	strcpy(str,tmp);
	return(str);
}

char *usage="\nusage: chksmb [-opts] <filespec.SHD>\n"
			"\n"
			" opts:\n"
			"       s - stop after errored message base\n"
			"       p - pause after errored messsage base\n"
			"       q - quiet mode (no beeps while checking)\n"
			"       a - don't check allocation files\n"
			"       t - don't check translation strings\n"
			"       e - display extended info on corrupted msgs\n";

int main(int argc, char **argv)
{
	char		str[128],*p,*s,*beep="\7";
	int 		i,j,x,y,lzh,errors,errlast,stop_on_error=0,pause_on_error=0
				,chkxlat=1,chkalloc=1,lzhmsg,extinfo=0,msgerr;
	ushort		xlat;
	ulong		l,m,n,length,size,total=0,orphan=0,deleted=0,headers=0
				,*offset,*number,xlaterr
				,delidx
				,delhdrblocks,deldatblocks,hdrerr=0,lockerr=0,hdrnumerr=0,hdrlenerr=0
				,acthdrblocks,actdatblocks
				,dfieldlength=0,dfieldoffset=0
				,dupenum=0,dupenumhdr=0,dupeoff=0,attr=0,actalloc=0
				,datactalloc=0,misnumbered=0,timeerr=0,idxofferr=0,idxerr
				,zeronum,idxzeronum,idxnumerr,packable=0L,totallzhsaved=0L
				,totalmsgs=0,totallzhmsgs=0,totaldelmsgs=0,totalmsgbytes=0L
				,lzhblocks,lzhsaved;
	smb_t		smb;
	idxrec_t	idx;
	smbmsg_t	msg;

	fprintf(stderr,"\nCHKSMB v2.12 - Check Synchronet Message Base - "
		"Copyright 2003 Rob Swindell\n");

	if(argc<2) {
		printf("%s",usage);
		exit(1); }

	errlast=errors=0;
	for(x=1;x<argc;x++) {
		if(stop_on_error && errors)
			break;
		if(pause_on_error && errlast!=errors) {
			fprintf(stderr,"\7\nHit any key to continue...");
			if(!getch())
				getch();
			printf("\n"); }
		errlast=errors;
		if(argv[x][0]=='-'
#if !defined(__unix__)	/* just for backwards compatibility */
			|| argv[x][0]=='/'
#endif
			) {
			for(y=1;argv[x][y];y++)
				switch(toupper(argv[x][y])) {
					case 'Q':
						beep="";
						break;
					case 'P':
						pause_on_error=1;
						break;
					case 'S':
						stop_on_error=1;
						break;
					case 'T':
						chkxlat=0;
						break;
					case 'A':
						chkalloc=0;
						break;
					case 'E':
						extinfo=1;
						break;
					default:
						printf("%s",usage);
						exit(1); }
			continue; }

	SAFECOPY(smb.file,argv[x]);
	p=strrchr(smb.file,'.');
	s=strrchr(smb.file,'\\');
	if(p>s) *p=0;

	sprintf(str,"%s.shd",smb.file);
	if(!fexist(str)) {
		printf("\n%s doesn't exist.\n",smb.file);
		continue; }

	fprintf(stderr,"\nChecking %s Headers\n\n",smb.file);

	smb.retry_time=30;
	if((i=smb_open(&smb))!=0) {
		printf("smb_open returned %d: %s\n",i,smb.last_error);
		errors++;
		continue; }

	length=filelength(fileno(smb.shd_fp));
	if(length<sizeof(smbhdr_t)) {
		printf("Empty\n");
		smb_close(&smb);
		continue; }

	if((i=smb_locksmbhdr(&smb))!=0) {
		smb_close(&smb);
		printf("smb_locksmbhdr returned %d: %s\n",i,smb.last_error);
		errors++;
		continue; }

	if((length/SHD_BLOCK_LEN)*sizeof(ulong)) {
		if((number=(ulong *)MALLOC(((length/SHD_BLOCK_LEN)+2)*sizeof(ulong)))
			==NULL) {
			printf("Error allocating %lu bytes of memory\n"
				,(length/SHD_BLOCK_LEN)*sizeof(ulong));
			return(++errors); } }
	else
		number=NULL;

	if(chkalloc && !(smb.status.attr&SMB_HYPERALLOC)) {
		if((i=smb_open_ha(&smb))!=0) {
			printf("smb_open_ha returned %d: %s\n",i,smb.last_error);
			return(++errors); }

		if((i=smb_open_da(&smb))!=0) {
			printf("smb_open_da returned %d: %s\n",i,smb.last_error);
			return(++errors); } }

	headers=deleted=orphan=dupenumhdr=attr=zeronum=timeerr=lockerr=hdrerr=0;
	actalloc=datactalloc=deldatblocks=delhdrblocks=xlaterr=0;
	lzhblocks=lzhsaved=acthdrblocks=actdatblocks=0;

	for(l=smb.status.header_offset;l<length;l+=size) {
		fprintf(stderr,"\r%2lu%%  ",(long)(100.0/((float)length/l)));
		msg.idx.offset=l;
		msgerr=0;
		if((i=smb_lockmsghdr(&smb,&msg))!=0) {
			printf("\n(%06lX) smb_lockmsghdr returned %d: %s\n",l,i,smb.last_error);
			lockerr++;
			headers++;
			size=SHD_BLOCK_LEN;
			continue; }
		if((i=smb_getmsghdr(&smb,&msg))!=0) {
			smb_unlockmsghdr(&smb,&msg);
			if(chkalloc && !(smb.status.attr&SMB_HYPERALLOC)) {
				fseek(smb.sha_fp
					,(l-smb.status.header_offset)/SHD_BLOCK_LEN,SEEK_SET);
				j=fgetc(smb.sha_fp);
				if(j) { 			/* Allocated block or at EOF */
					printf("%s\n(%06lX) smb_getmsghdr returned %d: %s\n",beep,l,i,smb.last_error);
					hdrerr++; }
				else
					delhdrblocks++; }
			else {
				/* printf("%s\n(%06lX) smb_getmsghdr returned %d\n",beep,l,i); */
				delhdrblocks++; }
			size=SHD_BLOCK_LEN;
			continue; }
		smb_unlockmsghdr(&smb,&msg);
		truncsp(msg.from);
		strip_ctrl(msg.from);
		fprintf(stderr,"#%-5lu (%06lX) %-25.25s ",msg.hdr.number,l,msg.from);

		if(msg.hdr.length!=smb_getmsghdrlen(&msg)) {
			fprintf(stderr,"%sHeader length mismatch\n",beep);
			msgerr=1;
			if(extinfo)
				printf("MSGERR: Header length (%hu) does not match calculcated length (%lu)\n"
					,msg.hdr.length,smb_getmsghdrlen(&msg));
			hdrlenerr++; 
		}

		lzhmsg=0;
		if(msg.hdr.attr&MSG_DELETE) {
			deleted++;
			if(number)
				number[headers]=0;
			if(smb.status.attr&SMB_HYPERALLOC)
				deldatblocks+=smb_datblocks(smb_getmsgdatlen(&msg)); }
		else {
			actdatblocks+=smb_datblocks(smb_getmsgdatlen(&msg));
			if(msg.hdr.number>smb.status.last_msg) {
				fprintf(stderr,"%sOut-Of-Range message number\n",beep);
				msgerr=1;
				if(extinfo)
					printf("MSGERR: Header number (%lu) greater than last (%lu)\n"
						,msg.hdr.number,smb.status.last_msg);
				hdrnumerr++; }

			if(smb_getmsgidx(&smb,&msg)) {
				fprintf(stderr,"%sNot found in index\n",beep);
				msgerr=1;
				if(extinfo)
					printf("MSGERR: Header number (%lu) not found in index\n"
						,msg.hdr.number);
				orphan++; }
			else if(msg.hdr.attr!=msg.idx.attr) {
				fprintf(stderr,"%sAttributes mismatch index\n",beep);
				msgerr=1;
				if(extinfo)
					printf("MSGERR: Header attributes (%04X) do not match index "
						"attributes (%04X)\n"
						,msg.hdr.attr,msg.idx.attr);
				attr++; }
			else if(msg.hdr.when_imported.time!=msg.idx.time) {
				fprintf(stderr,"%sImport date/time mismatch index\n",beep);
				msgerr=1;
				if(extinfo)
					printf("MSGERR: Header import date/time does not match "
						"index import date/time\n");
				timeerr++; }

			if(msg.hdr.number==0) {
				fprintf(stderr,"%sZero message number\n",beep);
				msgerr=1;
				if(extinfo)
					printf("MSGERR: Header number is zero (invalid)\n");
				zeronum++; }
			if(number) {
				for(m=0;m<headers;m++)
					if(number[m] && msg.hdr.number==number[m]) {
						fprintf(stderr,"%sDuplicate message number\n",beep);
						msgerr=1;
						if(extinfo)
							printf("MSGERR: Header number (%lu) duplicated\n"
								,msg.hdr.number);
						dupenumhdr++;
						break; }
				number[headers]=msg.hdr.number; }
			if(chkxlat) {		/* Check translation strings */
				for(i=0;i<msg.hdr.total_dfields;i++) {
					fseek(smb.sdt_fp,msg.hdr.offset+msg.dfield[i].offset,SEEK_SET);
					if(!fread(&xlat,2,1,smb.sdt_fp))
						xlat=0xffff;
					lzh=0;
					if(xlat==XLAT_LZH) {
						lzh=1;
						if(!fread(&xlat,2,1,smb.sdt_fp))
							xlat=0xffff; }
					if(xlat!=XLAT_NONE) {
						fprintf(stderr,"%sUnsupported Xlat %04X dfield[%u]\n"
							,beep,xlat,i);
						msgerr=1;
						if(extinfo)
							printf("MSGERR: Unsupported translation type (%04X) "
								"in dfield[%u] (offset %ld)\n"
								,xlat,i,msg.dfield[i].offset);
						xlaterr++; }
					else {
						if(lzh) {
							lzhmsg=1;
							if(fread(&m,4,1,smb.sdt_fp)) { /* Get uncompressed len */
								lzhsaved+=(smb_datblocks(m+2)
									-smb_datblocks(msg.dfield[i].length))
									*SDT_BLOCK_LEN;
								lzhblocks+=smb_datblocks(msg.dfield[i].length);
							} } } } } }

		size=smb_getmsghdrlen(&msg);
		while(size%SHD_BLOCK_LEN) size++;

		if(chkalloc && !(smb.status.attr&SMB_HYPERALLOC)) {
			fseek(smb.sha_fp,(l-smb.status.header_offset)/SHD_BLOCK_LEN,SEEK_SET);
			for(m=0;m<size;m+=SHD_BLOCK_LEN) {
	/***
				if(msg.hdr.attr&MSG_DELETE && (i=fgetc(smb.sha_fp))!=0) {
					fprintf(stderr,"%sDeleted Header Block %lu marked %02X\n"
						,beep,m/SHD_BLOCK_LEN,i);
					msgerr=1;
					delalloc++; }
	***/
				if(!(msg.hdr.attr&MSG_DELETE) && (i=fgetc(smb.sha_fp))!=1) {
					fprintf(stderr,"%sActive Header Block %lu marked %02X\n"
						,beep,m/SHD_BLOCK_LEN,i);
					msgerr=1;
					if(extinfo)
						printf("MSGERR: Active header block %lu marked %02X "
							"instead of 01\n"
							,m/SHD_BLOCK_LEN,i);
					actalloc++; } }

			if(!(msg.hdr.attr&MSG_DELETE)) {
				acthdrblocks+=(size/SHD_BLOCK_LEN);
				for(n=0;n<msg.hdr.total_dfields;n++) {
					if(msg.dfield[n].offset&0x80000000UL) {
						msgerr=1;
						if(extinfo)
							printf("MSGERR: Invalid Data Field [%lu] Offset: %lu\n"
								,n,msg.dfield[n].offset);
						dfieldoffset++; }
					if(msg.dfield[n].length&0x80000000UL) {
						msgerr=1;
						if(extinfo)
							printf("MSGERR: Invalid Data Field [%lu] Length: %lu\n"
								,n,msg.dfield[n].length);
						dfieldlength++; }
					fseek(smb.sda_fp
						,((msg.hdr.offset+msg.dfield[n].offset)/SDT_BLOCK_LEN)*2
						,SEEK_SET);
					for(m=0;m<msg.dfield[n].length;m+=SDT_BLOCK_LEN) {
						if(!fread(&i,2,1,smb.sda_fp) || !i) {
							fprintf(stderr
								,"%sActive Data Block %lu.%lu marked free\n"
								,beep,n,m/SHD_BLOCK_LEN);
							msgerr=1;
							if(extinfo)
								printf("MSGERR: Active Data Block %lu.%lu "
									"marked free\n"
									,n,m/SHD_BLOCK_LEN);
							datactalloc++; } } } }
			else
				delhdrblocks+=(size/SHD_BLOCK_LEN); }

		else {	 /* Hyper Alloc */
			if(msg.hdr.attr&MSG_DELETE)
				delhdrblocks+=(size/SHD_BLOCK_LEN);
			else
				acthdrblocks+=(size/SHD_BLOCK_LEN); }

		totallzhmsgs+=lzhmsg;
		headers++;
		if(msgerr && extinfo) {
			printf("\n");
			printf("%-20s: %s\n","Message Base",smb.file);
			printf("%-20s: %lu (%lu)\n","Message Number"
				,msg.hdr.number,msg.offset+1);
			printf("%-20s: %s\n","Subject",msg.subj);
			printf("%-20s: %s","To",msg.to);
			if(msg.to_net.type && msg.to_net.addr)
				printf(" (%s)",msg.to_net.type==NET_FIDO
					? faddrtoa(*(fidoaddr_t *)msg.to_net.addr) : (char*)msg.to_net.addr);
			printf("\n%-20s: %s","From",msg.from);
			if(msg.from_net.type && msg.from_net.addr)
				printf(" (%s)",msg.from_net.type==NET_FIDO
					? faddrtoa(*(fidoaddr_t *)msg.from_net.addr)
						: (char*)msg.from_net.addr);
			printf("\n");
			printf("%-20s: %.24s\n","When Written"
				,ctime((time_t *)&msg.hdr.when_written.time));
			printf("%-20s: %.24s\n","When Imported"
				,ctime((time_t *)&msg.hdr.when_imported.time));
			printf("%-20s: %04hXh\n","Type"
				,msg.hdr.type);
			printf("%-20s: %04hXh\n","Version"
				,msg.hdr.version);
			printf("%-20s: %u\n","Length"
				,msg.hdr.length);
			printf("%-20s: %lu\n","Calculated Length"
				,smb_getmsghdrlen(&msg));
			printf("%-20s: %04hXh\n","Attributes"
				,msg.hdr.attr);
			printf("%-20s: %08lXh\n","Auxilary Attributes"
				,msg.hdr.auxattr);
			printf("%-20s: %08lXh\n","Network Attributes"
				,msg.hdr.netattr);
			printf("%-20s: %06lXh\n","Header Offset"
				,msg.idx.offset);
			printf("%-20s: %06lXh\n","Data Offset"
				,msg.hdr.offset);
			printf("%-20s: %u\n","Total Data Fields"
				,msg.hdr.total_dfields);
			for(i=0;i<msg.hdr.total_dfields;i++)
				printf("dfield[%u].type      : %02Xh\n"
					   "dfield[%u].offset    : %lu (%lXh)\n"
					   "dfield[%u].length    : %lu\n"
					   ,i,msg.dfield[i].type
					   ,i,msg.dfield[i].offset, msg.dfield[i].offset
					   ,i,msg.dfield[i].length);
			printf("\n"); }

		smb_freemsgmem(&msg); }

	if(number)
		FREE(number);

	fprintf(stderr,"\r%79s\r100%%\n","");


	if(chkalloc && !(smb.status.attr&SMB_HYPERALLOC)) {

		fprintf(stderr,"\nChecking %s Data Blocks\n\n",smb.file);

		length=filelength(fileno(smb.sda_fp));

		fseek(smb.sda_fp,0L,SEEK_SET);
		for(l=0;l<length;l+=2) {
			fprintf(stderr,"\r%2lu%%  ",l ? (long)(100.0/((float)length/l)) : 0);
			i=0;
			if(!fread(&i,2,1,smb.sda_fp))
				break;
			if(!i)
				deldatblocks++; }

		fclose(smb.sha_fp);
		fclose(smb.sda_fp);

		fprintf(stderr,"\r%79s\r100%%\n",""); }

	total=filelength(fileno(smb.sid_fp))/sizeof(idxrec_t);

	dupenum=dupeoff=misnumbered=idxzeronum=idxnumerr=idxofferr=idxerr=delidx=0;

	if(total) {

	fprintf(stderr,"\nChecking %s Index\n\n",smb.file);

	if((offset=(ulong *)MALLOC(total*sizeof(ulong)))==NULL) {
		printf("Error allocating %lu bytes of memory\n",total*sizeof(ulong));
		return(++errors); }
	if((number=(ulong *)MALLOC(total*sizeof(ulong)))==NULL) {
		printf("Error allocating %lu bytes of memory\n",total*sizeof(ulong));
		return(++errors); }
	fseek(smb.sid_fp,0L,SEEK_SET);

	for(l=0;l<total;l++) {
		fprintf(stderr,"\r%2lu%%  %5lu ",l ? (long)(100.0/((float)total/l)) : 0,l);
		if(!fread(&idx,sizeof(idxrec_t),1,smb.sid_fp))
			break;
		fprintf(stderr,"#%-5lu (%06lX) 1st Pass ",idx.number,idx.offset);
		if(idx.attr&MSG_DELETE) {
	//		fprintf(stderr,"%sMarked for deletion\n",beep);
			delidx++; }
		for(m=0;m<l;m++)
			if(number[m]==idx.number) {
				fprintf(stderr,"%sDuplicate message number\n",beep);
				dupenum++;
				break; }
		for(m=0;m<l;m++)
			if(offset[m]==idx.offset) {
				fprintf(stderr,"%sDuplicate offset: %lu\n",beep,idx.offset);
				dupeoff++;
				break; }
		if(idx.offset<smb.status.header_offset) {
			fprintf(stderr,"%sInvalid offset\n",beep);
			idxofferr++;
			break; }
		if(idx.number==0) {
			fprintf(stderr,"%sZero message number\n",beep);
			idxzeronum++;
			break; }
		if(idx.number>smb.status.last_msg) {
			fprintf(stderr,"%sOut-Of-Range message number\n",beep);
			idxnumerr++;
			break; }
		number[l]=idx.number;
		offset[l]=idx.offset; }

	if(l<total) {
		fprintf(stderr,"%sError reading index record\n",beep);
		idxerr=1; }
	else {
		fprintf(stderr,"\r%79s\r","");
		for(m=0;m<total;m++) {
			fprintf(stderr,"\r%2lu%%  %5lu ",m ? (long)(100.0/((float)total/m)) : 0,m);
			fprintf(stderr,"#%-5lu (%06lX) 2nd Pass ",number[m],offset[m]);
			for(n=0;n<m;n++)
				if(number[m] && number[n] && number[m]<number[n]) {
					fprintf(stderr,"%sMisordered message number\n",beep);
					misnumbered++;
					number[n]=0;
					break; } }
		fprintf(stderr,"\r%79s\r100%%\n",""); }
	FREE(number);
	FREE(offset);

	}	/* if(total) */

	totalmsgs+=smb.status.total_msgs;
	totalmsgbytes+=(acthdrblocks*SHD_BLOCK_LEN)+(actdatblocks*SDT_BLOCK_LEN);
	totaldelmsgs+=deleted;
	totallzhsaved+=lzhsaved;
	printf("\n");
	printf("%-35.35s (=): %lu\n"
		,"Status Total"
		,smb.status.total_msgs);
	printf("%-35.35s (=): %lu\n"
		,"Total Indexes"
		,total);
	printf("%-35.35s (=): %lu\n"
		,"Active Indexes"
		,total-delidx);
	printf("%-35.35s (=): %lu\n"
		,"Active Headers"
		,headers-deleted);
	printf("%-35.35s ( ): %-8lu %13s bytes used\n"
		,"Active Header Blocks"
		,acthdrblocks,ultoac(acthdrblocks*SHD_BLOCK_LEN,str));
	printf("%-35.35s ( ): %-8lu %13s bytes used\n"
		,"Active Data Blocks"
		,actdatblocks,ultoac(actdatblocks*SDT_BLOCK_LEN,str));
	if(lzhblocks)
		printf("%-35.35s ( ): %-8lu %13s bytes saved\n"
			,"Active LZH Compressed Data Blocks"
			,lzhblocks,ultoac(lzhsaved,str));
	printf("%-35.35s ( ): %lu\n"
		,"Header Records"
		,headers);
	printf("%-35.35s ( ): %lu\n"
		,"Deleted Indexes"
		,delidx);
	printf("%-35.35s ( ): %lu\n"
		,"Deleted Headers"
		,deleted);
	printf("%-35.35s ( ): %-8lu %13s bytes used\n"
		,"Deleted Header Blocks"
		,delhdrblocks,ultoac(delhdrblocks*SHD_BLOCK_LEN,str));
	packable+=(delhdrblocks*SHD_BLOCK_LEN);
	printf("%-35.35s ( ): %-8lu %13s bytes used\n"
		,"Deleted Data Blocks"
		,deldatblocks,ultoac(deldatblocks*SDT_BLOCK_LEN,str));
	packable+=(deldatblocks*SDT_BLOCK_LEN);

	if(orphan)
		printf("%-35.35s (!): %lu\n"
			,"Orphaned Headers"
			,orphan);
	if(idxzeronum)
		printf("%-35.35s (!): %lu\n"
			,"Zeroed Index Numbers"
			,idxzeronum);
	if(zeronum)
		printf("%-35.35s (!): %lu\n"
			,"Zeroed Header Numbers"
			,zeronum);
	if(idxofferr)
		printf("%-35.35s (!): %lu\n"
			,"Invalid Index Offsets"
			,idxofferr);
	if(dupenum)
		printf("%-35.35s (!): %lu\n"
			,"Duplicate Index Numbers"
			,dupenum);
	if(dupeoff)
		printf("%-35.35s (!): %lu\n"
			,"Duplicate Index Offsets"
			,dupeoff);
	if(dupenumhdr)
		printf("%-35.35s (!): %lu\n"
			,"Duplicate Header Numbers"
			,dupenumhdr);
	if(misnumbered)
		printf("%-35.35s (!): %lu\n"
			,"Misordered Index Numbers"
			,misnumbered);
	if(lockerr)
		printf("%-35.35s (!): %lu\n"
			,"Unlockable Header Records"
			,lockerr);
	if(hdrerr)
		printf("%-35.35s (!): %lu\n"
			,"Unreadable Header Records"
			,hdrerr);
	if(idxnumerr)
		printf("%-35.35s (!): %lu\n"
			,"Out-Of-Range Index Numbers"
			,idxnumerr);
	if(hdrnumerr)
		printf("%-35.35s (!): %lu\n"
			,"Out-Of-Range Header Numbers"
			,hdrnumerr);
	if(hdrlenerr)
		printf("%-35.35s (!): %lu\n"
			,"Mismatched Header Lengths"
			,hdrlenerr);
	if(attr)
		printf("%-35.35s (!): %lu\n"
			,"Mismatched Header Attributes"
			,attr);
	if(timeerr)
		printf("%-35.35s (!): %lu\n"
			,"Mismatched Header Import Time"
			,timeerr);
	if(xlaterr)
		printf("%-35.35s (!): %lu\n"
			,"Unsupported Translation Types"
			,xlaterr);
	if(datactalloc)
		printf("%-35.35s (!): %lu\n"
			,"Misallocated Active Data Blocks"
			,datactalloc);
	if(actalloc)
		printf("%-35.35s (!): %lu\n"
			,"Misallocated Active Header Blocks"
			,actalloc);
	/***
	if(delalloc)
		printf("%-35.35s (!): %lu\n"
			,"Misallocated Deleted Header Blocks"
			,delalloc);
	***/

	if(dfieldoffset)
		printf("%-35.35s (!): %lu\n"
			,"Invalid Data Field Offsets"
			,dfieldoffset);

	if(dfieldlength)
		printf("%-35.35s (!): %lu\n"
			,"Invalid Data Field Lengths"
			,dfieldlength);


	printf("\n%s Message Base ",smb.file);
	if(/* (headers-deleted)!=smb.status.total_msgs || */
		total!=smb.status.total_msgs
		|| (headers-deleted)!=total-delidx
		|| idxzeronum || zeronum
		|| orphan || dupenumhdr || dupenum || dupeoff || attr
		|| lockerr || hdrerr || hdrnumerr || idxnumerr || idxofferr
		|| actalloc || datactalloc || misnumbered || timeerr
		|| dfieldoffset || dfieldlength || xlaterr || idxerr) {
		printf("%shas Errors!\n",beep);
		errors++; }
	else
		printf("is OK\n");

	smb_unlocksmbhdr(&smb);
	smb_close(&smb);
	}

	if((totalmsgs && (totalmsgs!=smb.status.total_msgs || totallzhmsgs))
		|| packable)
		printf("\n");
	if(totalmsgs && totalmsgs!=smb.status.total_msgs)
		printf("%-39.39s: %-8lu %13s bytes used\n"
			,"Total Active Messages"
			,totalmsgs,ultoac(totalmsgbytes,str));
	if(totallzhmsgs && totalmsgs!=smb.status.total_msgs)
		printf("%-39.39s: %-8lu %13s bytes saved\n"
			,"Total LZH Compressed Messages"
			,totallzhmsgs,ultoac(totallzhsaved,str));
	if(packable)
		printf("%-39.39s: %-8lu %13s bytes used\n"
			,"Total Deleted Messages"
			,totaldelmsgs,ultoac(packable,str));

	if(pause_on_error && errlast!=errors) {
		fprintf(stderr,"\7\nHit any key to continue...");
		if(!getch())
			getch();
		fprintf(stderr,"\n"); }


	return(errors);
}
