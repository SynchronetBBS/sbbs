/* LSTPHOTO.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include <io.h>
#include <dos.h>
#include <dir.h>
#include <bios.h>
#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <share.h>
#include <conio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "gen_defs.h"
#include "crc32.h"
#include "smmdefs.h"

extern int daylight=0;
extern long timezone=0L;



char *base41(unsigned int i, char *str)
{
	char c;
	unsigned int j=41*41,k;

for(c=0;c<3;c++) {
	k=i/j;
	str[c]='0'+k;
	i-=(k*j);
	j/=41;
	if(str[c]>=':')
		str[c]='A'+(str[c]-':');
	if(str[c]>='[')
		str[c]='#'+(str[c]-'['); }
str[c]=0;
return(str);
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
/* Returns 16-crc of string (not counting terminating NULL)                 */
/****************************************************************************/
ushort crc16(char *str)
{
    int     i=0;
    ushort  crc=0;

ucrc16(0,&crc);
while(str[i])
    ucrc16(str[i++],&crc);
ucrc16(0,&crc);
ucrc16(0,&crc);
return(crc);
}

/****************************************************************************/
/* Returns 32-crc of string (not counting terminating NULL) 				*/
/****************************************************************************/
ulong crc32(char *str)
{
	int i=0;
	ulong crc=0xffffffffUL;

	while(str[i])
		crc=ucrc32(str[i++],crc);
	crc=~crc;
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


time_t checktime()
{
	struct tm tm;

memset(&tm,0,sizeof(tm));
tm.tm_year=94;
tm.tm_mday=1;
return(mktime(&tm)^0x2D24BD00L);
}


int main()
{
	char str[128],fname[128],tmp[128];
	int i,file;
	FILE *stream;
	user_t user;

printf("\nLSTPHOTO v1.00 - Synchronet Match Maker Photograph List\n\n");

if(checktime()) {
    printf("Time problem!\n");
    exit(1); }

if((file=open("SMM.DAB",O_RDWR|O_BINARY|SH_DENYNO|O_CREAT
    ,S_IWRITE|S_IREAD))==-1) {
	printf("\n\7Error opening/creating SMM.DAB\n");
    exit(1); }
if((stream=fdopen(file,"w+b"))==NULL) {
	printf("\n\7Error converting SMM.DAB file handle to stream\n");
    exit(1); }
setvbuf(stream,0L,_IOFBF,4096);

while(!feof(stream)) {
	if(!fread(&user,sizeof(user_t),1,stream))
		break;
	if(!(user.misc&USER_PHOTO) || user.misc&USER_DELETED || !user.number)
		continue;

	printf("%-25.25s %5lu  %-25.25s %s  ",user.system,user.number,user.name
		,unixtodstr(user.photo,tmp));
	for(i=0;user.system[i];i++)
		if(isalnum(user.system[i]))
			break;
	if(!user.system[i])
		fname[0]='~';
	else
		fname[0]=user.system[i];
	for(i=strlen(user.system)-1;i>0;i--)
		if(isalnum(user.system[i]))
			break;
	if(i<=0)
		fname[1]='~';
	else
		fname[1]=user.system[i];
	fname[2]=0;
	strupr(fname);
	strupr(user.system);
	strcat(fname,base41(crc16(user.system),tmp));
	strcat(fname,base41(user.number,tmp));
	printf("%s\n",fname); }

return(0);
}

