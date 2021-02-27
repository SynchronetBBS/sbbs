/* MAKEMSG.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include <dos.h>
#include <stdio.h>
#include <time.h>
#include "gen_defs.h"

/****************************************************************************/
/* Converts a date string in format MM/DD/YY into unix time format			*/
/****************************************************************************/
time_t dstrtounix(char *str)
{
	struct date date;
	struct time curtime;

if(!strcmp(str,"00/00/00") || !str[0])
	return(NULL);
curtime.ti_hour=curtime.ti_min=curtime.ti_sec=0;
if(str[6]<'7')
	date.da_year=2000+((str[6]&0xf)*10)+(str[7]&0xf);
else
	date.da_year=1900+((str[6]&0xf)*10)+(str[7]&0xf);
date.da_mon=((str[0]&0xf)*10)+(str[1]&0xf);
date.da_day=((str[3]&0xf)*10)+(str[4]&0xf);
return(dostounix(&date,&curtime));
}

uchar cryptchar(uchar ch, ulong seed)
{
if(ch==1)
	return(0xfe);
if(ch<0x20 || ch&0x80)	/* Ctrl chars and ex-ASCII are not xlated */
	return(ch);
return(ch^(seed&0x1f));
}

int main(int argc, char **argv)
{
	char str[256];
	FILE *in,*out;
	int i,j;
	long l;

if(argc<4) {
	printf("usage: makemsg infile outfile mm/dd/yy\n");
	exit(1); }

if((in=fopen(argv[1],"rb"))==NULL) {
	printf("error opening %s\n",argv[1]);
	exit(1); }

if((out=fopen(argv[2],"wb"))==NULL) {
	printf("error opening %s\n",argv[2]);
	exit(1); }

l=dstrtounix(argv[3]);
if(!l) {
	printf("Invalid date %s\n",argv[3]);
	exit(1); }
fprintf(out,"%lx\r\n",l^0x305F6C81UL);
i=ftell(out);
while(!feof(in)) {
	if(!fgets(str,128,in))
		break;
	for(j=0;str[j];j++,i++)
		fputc(cryptchar(str[j],l^(i&7)),out); }
return(0);
}
