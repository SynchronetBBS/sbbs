/* OUTPHOTO.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* Scans SMM database and posts any photographs into the an SMB base */

#define  uint unsigned int

#include <dos.h>
#include <share.h>
#include "smblib.h"
#include "smmdefs.h"
#include "crc32.h"

#define DAYS 15L			 // Hatch every 15 days

smb_t smb;
extern int daylight=0;
extern long timezone=0L;

unsigned _stklen=16000; 		  /* Set stack size in code, not header */

/****************************************************************************/
/* Checks the disk drive for the existence of a file. Returns 1 if it       */
/* exists, 0 if it doesn't.                                                 */
/****************************************************************************/
char fexist(char *filespec)
{
    struct ffblk f;

if(findfirst(filespec,&f,0)==0)
    return(1);
return(0);
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



time_t checktime()
{
	struct tm tm;

memset(&tm,0,sizeof(tm));
tm.tm_year=94;
tm.tm_mday=1;
return(mktime(&tm)^0x2D24BD00L);
}

#define BUF_LEN 8192

int main(int argc, char **argv)
{
	uchar	str[128],tmp[128],buf[BUF_LEN],fname[64],path[128],*p,ch;
	int 	i,j,file,in,linelen,all=0;
	ushort	xlat;
	long	length,l;
	ulong	offset,crc;
	time_t	now;
	struct	ffblk ff;
	user_t	user;
	smbmsg_t msg;
	FILE	*stream;

fprintf(stderr,"\nOUTPHOTO %s - Write SMM photos to SMB - Copyright 2002 "
	"Rob Swindell\n\n",__DATE__);

if(checktime()) {
    printf("Time problem!\n");
    return(-1); }

if(argc<3) {
	fprintf(stderr,"usage: outphoto <smm.dab> <smb_file>\n\n");
	fprintf(stderr,"example: outphoto c:\\sbbs\\xtrn\\smm\\smm.dab "
		"c:\\sbbs\\data\\subs\\syncdata\n");
	return(1); }

for(i=3;i<argc;i++)
	if(!stricmp(argv[i],"/ALL"))
		all=1;

strcpy(smb.file,argv[2]);
strupr(smb.file);

strcpy(str,argv[1]);
strupr(str);
if((file=open(str,O_RDWR|O_BINARY|O_DENYNONE))==-1) {
	printf("error opening %s\n",str);
	return(1); }
if((stream=fdopen(file,"w+b"))==NULL) {
	printf("error fdopening %s\n",str);
	return(1); }
setvbuf(stream,NULL,_IOFBF,4096);

sprintf(str,"%s.SHD",smb.file);
if(!fexist(str)) {
	printf("%s doesn't exist\n",smb.file);
	return(0); }
fprintf(stderr,"Opening %s\n",smb.file);
smb.retry_time=30;
if((i=smb_open(&smb))!=0) {
	printf("smb_open returned %d\n",i);
	return(1); }

now=time(NULL);
while(!feof(stream)) {
	if(!fread(&user,sizeof(user_t),1,stream))
		break;

	if(user.misc&(USER_DELETED|USER_FROMSMB) || !(user.misc&USER_PHOTO))
		continue;

	printf("Photo: %-25.25s  %.24s  "
		,user.name,ctime(&user.photo));

	if(user.photo && now-user.photo<DAYS*24L*60L*60L && !all) {
		printf("skipping\n");
		continue; }
	printf("hatching\n");

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
	strcpy(str,user.system);
	strupr(str);
	strcat(fname,base41(crc16(str),tmp));
	strcat(fname,base41(user.number,tmp));
	strcpy(path,argv[1]);
	p=strchr(path,'\\');
	if(p) *(p+1)=0;
	else path[0]=0;
	sprintf(str,"%sPHOTO\\%s.*",path,fname);
	if(findfirst(str,&ff,0)) {
		printf("%s doesn't exist!\n",str);
		continue; }
	strcpy(fname,ff.ff_name);
	strcpy(path,argv[1]);
	p=strchr(path,'\\');
	if(p) *(p+1)=0;
	else path[0]=0;
	sprintf(str,"%sPHOTO\\%s",path,fname);

	printf("Exporting %s",str);
	if((in=open(str,O_RDONLY|O_BINARY|SH_DENYWR))==-1) {
		printf("\nError opening %s\n",str);
		continue; }

	memset(buf,0,BUF_LEN);
	crc=0xffffffffUL;
	linelen=0;
	sprintf(buf,"%lx\r\n",filelength(in)^4096);
	l=strlen(buf);
	while(l<(BUF_LEN-128)) {
		if(!read(in,&ch,1))
			break;
		crc=ucrc32(ch,crc);
		if(ch=='`') {
			buf[l++]='`';
			buf[l++]='`';
			linelen++; }
		else if(ch==0xff)
			buf[l++]='\xee';
		else if(ch<0xf)
			buf[l++]=ch|0xf0;
		else if(ch<0x20) {
			buf[l++]='`';
			buf[l++]=ch+'@';
			linelen++; }
		else if(ch>=0xf0) {
			sprintf(str,"`%x",ch&0xf);
			buf[l++]=str[0];
			buf[l++]=str[1];
			linelen++; }
		else if(ch==0xe3) {
			buf[l++]='`';
			buf[l++]='g';
			linelen++; }
		else if(ch==0x8d) {
			buf[l++]='`';
			buf[l++]='h';
			linelen++; }
		else if(ch==0xee) {
			buf[l++]='`';
			buf[l++]='i';
            linelen++; }
		else
			buf[l++]=ch;
		linelen++;
		if(linelen>=70) {
			buf[l++]=CR;
			buf[l++]=LF;
			linelen=0; } }
	buf[l]=0;
	crc=~crc;
	sprintf(str,"%lx\r\n--- OUTPHOTO %s",time(NULL),__DATE__);
	strcat(buf,str);
	printf("\n");

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
	msg.hdr.when_written.time=time(NULL);
    
	msg.hdr.offset=offset;

	strcpy(str,"SMM PHOTO");
	i=smb_hfield(&msg,RECIPIENT,strlen(str),str);
	if(i) {
		printf("smb_hfield returned %d\n",i);
		smb_freemsgdat(&smb,offset,length,1);
		exit(1); }
	strlwr(str);
	msg.idx.to=crc16(str);

	strcpy(str,user.system);
	i=smb_hfield(&msg,SENDER,strlen(str),str);
	if(i) {
		printf("smb_hfield returned %d\n",i);
		smb_freemsgdat(&smb,offset,length,1);
		exit(1); }
	strlwr(str);
	msg.idx.from=crc16(str);

	sprintf(tmp,"%.25s",user.system);
	strupr(tmp);
	sprintf(str,"%04lx%08lx%08lx%.3s"
		,user.number^0x191L,crc32(tmp)^0x90120e71L,crc^0x05296328L,fname+9);
	i=smb_hfield(&msg,SUBJECT,strlen(str),str);
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
	smb_freemsgmem(&msg);
	fseek(stream,ftell(stream)-sizeof(user_t),SEEK_SET);
	user.photo=time(NULL);
	fwrite(&user,sizeof(user_t),1,stream);
	fseek(stream,ftell(stream),SEEK_SET); }
return(0);
}

