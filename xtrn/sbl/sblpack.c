/* SBLPACK.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/***************************************/
/* Synchronet BBS List Database Packer */
/***************************************/

#include <stdio.h>
#include <stdlib.h>		/* exit() */
#include <share.h>
#include <time.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
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

	printf("\nSBLPACK v1.10/%s  Developed 1995-2001 Rob Swindell\n\n"
		,PLATFORM_DESC);

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
	fcloseall();
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

