/* fixsmb.c */

/* Synchronet message base (SMB) index re-generator */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2003 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#include <stdio.h>	
#include <stdlib.h>	/* atoi */
#include <string.h>	/* strnicmp */
#include <ctype.h>	/* toupper */

#include "sbbs.h"

char *usage="usage: fixsmb [/opts] <smb_file>\n"
			"\n"
			" opts:\n"
			"       m - force mail format instead of sub-board format\n"
			"\n"
			"   ex: FIXSMB /M MAIL\n"
			"   or: FIXSMB DEBATE\n";

#define MAIL (1<<0)

int main(int argc, char **argv)
{
	char		str[512],c;
	int 		i,w,mode=0;
	ulong		l,length,size,n;
	smb_t		smb;
	smbmsg_t	msg;

	printf("\nFIXSMB v1.24 - Rebuild Synchronet Message Base Index - by Rob Swindell\n");

	smb.file[0]=0;
	for(i=1;i<argc;i++)
		if(argv[i][0]=='-')
			switch(toupper(argv[i][1])) {
				case 'M':
					mode|=MAIL;
					break;
				default:
					printf(usage);
					exit(1); }
		else
			SAFECOPY(smb.file,argv[i]);

	if(!smb.file[0]) {
		printf(usage);
		exit(1); }

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

	if(mode&MAIL && !(smb.status.attr&SMB_EMAIL)) {
		smb.status.attr|=SMB_EMAIL;
		if((i=smb_putstatus(&smb))!=0) {
			smb_unlocksmbhdr(&smb);
			smb_close(&smb);
			printf("smb_putstatus returned %d\n",i);
			exit(1); } }

	if(!(smb.status.attr&SMB_HYPERALLOC)) {

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


	if(!(smb.status.attr&SMB_HYPERALLOC)) {
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
	for(l=smb.status.header_offset;l<length;l+=size) {
		size=SHD_BLOCK_LEN;
		printf("\r%2lu%%  ",(long)(100.0/((float)length/l)));
		msg.idx.offset=l;
		if((i=smb_lockmsghdr(&smb,&msg))!=0) {
			printf("\n(%06lX) smb_lockmsghdr returned %d\n",l,i);
			continue; }
		if((i=smb_getmsghdr(&smb,&msg))!=0) {
			smb_unlockmsghdr(&smb,&msg);
			printf("\n(%06lX) smb_getmsghdr returned %d\n",l,i);
			continue; }
		smb_unlockmsghdr(&smb,&msg);
		printf("#%-5lu (%06lX) %-25.25s ",msg.hdr.number,l,msg.from);
		if(!(msg.hdr.attr&MSG_DELETE)) {   /* Don't index deleted messages */
			msg.offset=n-1;
			msg.hdr.number=n;
			msg.idx.number=n;
			msg.idx.attr=msg.hdr.attr;
			msg.idx.time=msg.hdr.when_imported.time;
			msg.idx.subj=subject_crc(msg.subj);
			if(smb.status.attr&SMB_EMAIL) {
				if(msg.to_ext)
					msg.idx.to=atoi(msg.to_ext);
				else
					msg.idx.to=0;
				if(msg.from_ext)
					msg.idx.from=atoi(msg.from_ext);
				else
					msg.idx.from=0; }
			else {
				SAFECOPY(str,msg.to);
				strlwr(str);
				msg.idx.to=crc16(str,0);
				SAFECOPY(str,msg.from);
				strlwr(str);
				msg.idx.from=crc16(str,0); }
			if((i=smb_putmsg(&smb,&msg))!=0) {
				printf("\nsmb_putmsg returned %d\n",i);
				continue; }
			n++; }
		else
			printf("Not indexing deleted message\n");
		size=smb_getmsghdrlen(&msg);
		while(size%SHD_BLOCK_LEN)
			size++;

		if(!(smb.status.attr&SMB_HYPERALLOC)) {
			/**************************/
			/* Allocate header blocks */
			/**************************/
			fseek(smb.sha_fp,(l-smb.status.header_offset)/SHD_BLOCK_LEN,SEEK_SET);
			if(msg.hdr.attr&MSG_DELETE) c=0;		/* mark as free */
			else c=1;								/* or allocated */

			for(i=0;i<(int)(size/SHD_BLOCK_LEN);i++)
				fputc(c,smb.sha_fp);

			/************************/
			/* Allocate data blocks */
			/************************/

			if(!(msg.hdr.attr&MSG_DELETE))
				smb_incdat(&smb,msg.hdr.offset,smb_getmsgdatlen(&msg),1);
			}

		smb_freemsgmem(&msg); }
	printf("\r%79s\r100%%\n","");
	smb.status.total_msgs=smb.status.last_msg=n-1;
	printf("Saving message base status.\n");
	if((i=smb_putstatus(&smb))!=0)
		printf("\nsmb_putstatus returned %d\n",i);
	smb_unlocksmbhdr(&smb);
	printf("Closing message base.\n");
	smb_close(&smb);
	printf("Done.\n");
	return(0);
}
