/* sblpack.c */

/* Synchronet BBS List Database Packer */

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

#include <stdio.h>
#include <stdlib.h>		/* exit() */
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "filewrap.h"	/* sopen() */
#include "xsdkwrap.h"	/* PLATFORM_DESC */
#include "xsdkdefs.h"
#include "sbldefs.h"

int main(void)
{
	int		file;
	FILE *	in;
	FILE *	out;
	bbs_t	bbs;
	ulong	records=0;
	ulong	deleted=0;
	char	revision[16];

	sscanf("$Revision$", "%*s %s", revision);

	printf("\nSBLPACK %s-%s  Copyright 2003 Rob Swindell\n\n"
		,revision,PLATFORM_DESC);

	if((file=sopen("sbl.dab",O_RDWR|O_BINARY,SH_DENYNO))==-1) {
		printf("\n\7Error opening/creating sbl.dab\n");
		exit(1); }
	if((in=fdopen(file,"w+b"))==NULL) {
		printf("\n\7Error converting sbl.dab file handle to stream\n");
		exit(1); }
	setvbuf(in,0L,_IOFBF,2048);
	if((out=fopen("sbl.tmp","wb"))==NULL) {
		printf("\n\7Error opening sbl.tmp file\n");
		exit(1); }

	while(!feof(in)) {
		if(!fread(&bbs,sizeof(bbs_t),1,in))
			break;
		records++;
		putchar('.');
		if(bbs.name[0]==0 
			|| bbs.user[0]==0
			|| bbs.total_numbers<1) {
			deleted++;
			continue;
		}
		fwrite(&bbs,sizeof(bbs_t),1,out); 
	}
	fclose(in);
	fclose(out);
	putchar('\n');
	if(remove("sbl.dab")) {
		printf("\n\7Data file in use, can't remove.\n");
		remove("sbl.tmp");
		exit(1); 
	}
	rename("sbl.tmp","sbl.dab");
	printf("\nDone.\n");
	printf("\n%lu records in original file, %lu deleted (packed), new total = %lu.\n"
		,records,deleted,records-deleted);
	return(0);
}

