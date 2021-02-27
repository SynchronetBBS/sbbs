/* DELPHOTO.C */

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

time_t checktime()
{
	struct tm tm;

memset(&tm,0,sizeof(tm));
tm.tm_year=94;
tm.tm_mday=1;
return(mktime(&tm)^0x2D24BD00L);
}


int main(int argc, char **argv)
{
	char str[128],fname[128],path[128],tmp[128];
	int i,file;
	FILE *index, *stream;
	ulong number,crc;
	user_t user;
	ixb_t ixb;
	struct ffblk ff;

printf("\nDELPHOTO v1.00 - Synchronet Match Maker Photograph Deletion\n\n");

if(checktime()) {
    printf("Time problem!\n");
    exit(1); }

if(argc<3) {
	printf("usage: delphoto user_number system_name\n");
	exit(1); }

if((file=open("SMM.DAB",O_RDWR|O_BINARY|SH_DENYNO|O_CREAT
    ,S_IWRITE|S_IREAD))==-1) {
	printf("\n\7Error opening/creating SMM.DAB\n");
    exit(1); }
if((stream=fdopen(file,"w+b"))==NULL) {
	printf("\n\7Error converting SMM.DAB file handle to stream\n");
    exit(1); }
setvbuf(stream,0L,_IOFBF,4096);

if((file=open("SMM.IXB",O_RDWR|O_BINARY|SH_DENYNO|O_CREAT
    ,S_IWRITE|S_IREAD))==-1) {
	printf("\n\7Error opening/creating SMM.IXB\n");
    exit(1); }
if((index=fdopen(file,"w+b"))==NULL) {
	printf("\n\7Error converting SMM.IXB file handle to stream\n");
    exit(1); }
setvbuf(stream,0L,_IOFBF,1024);

number=atol(argv[1]);
str[0]=0;
for(i=2;i<argc;i++) {
	if(str[0])
		strcat(str," ");
	strcat(str,argv[i]); }

strupr(str);
str[25]=0;

crc=crc32(str);
rewind(index);
i=0;
while(!feof(index)) {
	if(!fread(&ixb,sizeof(ixb_t),1,index))
		break;
	if(!ixb.number)    /* DELETED */
		continue;
	if(ixb.system!=crc || ixb.number!=number)
		continue;
	fseek(stream
		,((ftell(index)-sizeof(ixb_t))/sizeof(ixb_t))
		*sizeof(user_t),SEEK_SET);
	if(!fread(&user,sizeof(user_t),1,stream))
		continue;
	i=1;
	break; }
if(!i) {
	printf("\7User #%lu @ %s not found!\n",number,str);
	exit(2); }

if(!(user.misc&USER_PHOTO)) {
	printf("\7User #%lu @ %s doesn't have a photo attached to their profile.\n"
		,number,str);
	exit(3); }

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
strupr(user.system);
strcat(fname,base41(crc16(user.system),tmp));
strcat(fname,base41(user.number,tmp));
strcat(fname,".*");
strupr(fname);
sprintf(path,"PHOTO\\%s",fname);
i=findfirst(path,&ff,0);
if(i)
	printf("\7%s doesn't exist!\n",path);
else {
	sprintf(path,"PHOTO\\%s",ff.ff_name);
	if(remove(path))
		printf("\7%s couldn't be removed!\n",path); }

user.misc&=~USER_PHOTO;
user.updated=time(NULL);
user.photo=0;
fseek(stream
	,((ftell(index)-sizeof(ixb_t))/sizeof(ixb_t))
	*sizeof(user_t),SEEK_SET);
fwrite(&user,sizeof(user_t),1,stream);
ixb.updated=user.updated;
fseek(index,ftell(index)-sizeof(ixb_t),SEEK_SET);
fwrite(&ixb,sizeof(ixb_t),1,index);

printf("Photo removed successfully from database.\n");
return(0);
}

