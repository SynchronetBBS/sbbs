/* SBLPACK.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/***************************************/
/* Synchronet BBS List Database Packer */
/***************************************/

#include <stdio.h>
#include <share.h>
#include <time.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "gen_defs.h"
#include "sbldefs.h"

int main(void)
{
	int file;
	FILE *in,*out;
	bbs_t bbs;

printf("\nSBLPACK v1.00  Developed 1995-1997 Rob Swindell\n\n");

if((file=open("SBL.DAB",O_RDWR|O_BINARY|O_DENYNONE|O_CREAT
	,S_IWRITE|S_IREAD))==-1) {
	printf("\n\7Error opening/creating SBL.DAB\n");
	exit(1); }
if((in=fdopen(file,"w+b"))==NULL) {
	printf("\n\7Error converting SBL.DAB file handle to stream\n");
	exit(1); }
setvbuf(in,0L,_IOFBF,2048);
if((out=fopen("SBL.TMP","wb"))==NULL) {
	printf("\n\7Error opening SBL.TMP file\n");
	exit(1); }

while(!feof(in)) {
	if(!fread(&bbs,sizeof(bbs_t),1,in))
		break;
	putchar('.');
	if(!bbs.name[0])
		continue;
	fwrite(&bbs,sizeof(bbs_t),1,out); }
fcloseall();
putchar('\n');
if(remove("SBL.DAB")) {
	printf("\n\7Data file in use, can't remove.\n");
	remove("SBL.TMP");
	exit(1); }
rename("SBL.TMP","SBL.DAB");
printf("\nDone.\n");
return(0);
}

