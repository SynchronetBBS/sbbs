/* SMM2MSG.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* Scans SMM database and posts any additions/updates into an MSG file */

#define  uint unsigned int

#include <dos.h>
#include "smblib.h"
#include "smmdefs.h"

#define VERSION "1.00á"

smb_t smb;
extern int daylight=0;
extern long timezone=0L;

unsigned _stklen=16000; 		  /* Set stack size in code, not header */

uchar cryptchar(uchar ch, ulong seed)
{
if(ch==1)
	return(0xfe);
if(ch<0x20 || ch&0x80)	/* Ctrl chars and ex-ASCII are not xlated */
	return(ch);
return(ch^(seed&0x1f));
}

char *encrypt(uchar *str, ulong seed)
{
	static uchar out[1024];
	int i,j;

j=strlen(str);
for(i=0;i<j;i++)
	out[i]=cryptchar(str[i],seed^(i&7));
out[i]=0;
return(out);
}

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

#define BUF_LEN 8192

int main(int argc, char **argv)
{
	uchar	str[128],tmp[128],msg_dir[128],buf[BUF_LEN],telegram[513],*p;
	int 	i,j,file;
	ushort	xlat;
	long	length;
	ulong	offset;
	time_t	last,t;
	user_t	user,from;
	wall_t	wall;
    smbmsg_t msg;
	FILE	*stream;

fprintf(stderr,"\nSMM2MSG v%s - Write SMM data to MSG - Developed 1995-1997 "
	"Rob Swindell\n\n",VERSION);

if(checktime()) {
    printf("Time problem!\n");
    return(-1); }

if(argc<3) {
	fprintf(stderr,"usage: smm2msg <smm.dab> <msg_dir>\n\n");
	fprintf(stderr,"example: smm2msg c:\\sbbs\\xtrn\\smm\\smm.dab "
		"c:\\im\\mail\n");
	return(1); }


strcpy(msg_dir,argv[2]);
strupr(msg_dir);

strcpy(str,argv[1]);
strupr(str);
if((file=open(str,O_RDONLY|O_BINARY|O_DENYNONE))==-1) {
	printf("error opening %s\n",str);
	return(1); }
if((stream=fdopen(file,"rb"))==NULL) {
	printf("error fdopening %s\n",str);
	return(1); }
setvbuf(stream,NULL,_IOFBF,4096);

strcpy(tmp,str);
p=strrchr(tmp,'.');
if(p) {
	(*p)=0;
	strcat(tmp,"2MSG.DAB");
	if((file=open(tmp,O_RDWR|O_BINARY|O_CREAT,S_IWRITE|S_IREAD))==-1) {
		printf("error opening %s\n",str);
		return(1); }
	t=time(NULL);
	if(read(file,&last,sizeof(time_t))!=sizeof(time_t))
		last=0;
	lseek(file,0L,SEEK_SET);
	write(file,&t,sizeof(time_t));
	close(file); }

//sprintf(str,"%s.SHD",smb.file);
//if(!fexist(str)) {
//	  printf("%s doesn't exist\n",smb.file);
//	  return(0); }
//fprintf(stderr,"Opening %s\n",smb.file);
//smb.retry_time=30;
//if((i=smb_open(&smb))!=0) {
//	  printf("smb_open returned %d\n",i);
//	  return(1); }

strcpy(str,argv[1]);
strupr(str);
p=strrchr(str,'\\');
if(p) p++;
else p=str;
strcpy(p,"TELEGRAM.DAB");
if((file=open(str,O_RDWR|O_DENYNONE|O_BINARY))!=-1) {
	while(!eof(file)) {
		read(file,from.system,sizeof(user.system));
		read(file,user.system,sizeof(user.system));
        read(file,&user.number,sizeof(user.number));
		printf("Telegram to: %lu@%s\n",user.number,user.system);
		read(file,telegram,512);
		telegram[512]=0;
		sprintf(buf,"}%lx\r\n%s",user.number,encrypt(telegram,user.number));
		strcat(buf,"\r\n"); /* blank line terminates telegram */
		strcat(buf,"\r\n--- SMM2MSG v");
		strcat(buf,VERSION);

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
		msg.hdr.version=SMB_VERSION;
		msg.hdr.when_written.time=time(NULL);

		msg.hdr.offset=offset;

		strcpy(str,"SMM");
		i=smb_hfield(&msg,RECIPIENT,strlen(str),str);
		if(i) {
			printf("smb_hfield returned %d\n",i);
			smb_freemsgdat(&smb,offset,length,1);
			exit(1); }
		strlwr(str);
		msg.idx.to=crc16(str);

		strcpy(str,from.system);
		i=smb_hfield(&msg,SENDER,strlen(str),str);
		if(i) {
			printf("smb_hfield returned %d\n",i);
			smb_freemsgdat(&smb,offset,length,1);
			exit(1); }
		strlwr(str);
		msg.idx.from=crc16(str);

		strcpy(str,user.system);
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
		smb_freemsgmem(&msg); }
	chsize(file,0L);
	close(file); }

strcpy(str,argv[1]);
strupr(str);
p=strrchr(str,'\\');
if(p) p++;
else p=str;
strcpy(p,"WALL.DAB");
if((file=sopen(str,O_RDONLY|O_BINARY,SH_DENYNO))!=-1) {
	while(!eof(file)) {
		if(!read(file,&wall,sizeof(wall_t)))
			break;
		if(last && wall.imported<last)
			continue;

		if(wall.imported!=wall.written) 	/* Imported from SMB */
			continue;

		printf("Wall writing from: %s\n",wall.name);
		sprintf(buf,"%s\r\n",encrypt(wall.name,wall.written));
		sprintf(str,"%s\r\n",encrypt(wall.system,wall.written));
		strcat(buf,str);
		for(i=0;i<5;i++) {
			sprintf(str,"%s\r\n",encrypt(wall.text[i],wall.written));
			strcat(buf,str); }
		sprintf(str,"%lx\r\n",wall.written);
		strcat(buf,str);
		strcat(buf,"\r\n--- SMM2SMB v");
		strcat(buf,VERSION);

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
		msg.hdr.version=SMB_VERSION;
		msg.hdr.when_written.time=time(NULL);

		msg.hdr.offset=offset;

		strcpy(str,"SMM");
		i=smb_hfield(&msg,RECIPIENT,strlen(str),str);
		if(i) {
			printf("smb_hfield returned %d\n",i);
			smb_freemsgdat(&smb,offset,length,1);
			exit(1); }
		strlwr(str);
		msg.idx.to=crc16(str);

		strcpy(str,wall.system);
		i=smb_hfield(&msg,SENDER,strlen(str),str);
		if(i) {
			printf("smb_hfield returned %d\n",i);
			smb_freemsgdat(&smb,offset,length,1);
			exit(1); }
		strlwr(str);
		msg.idx.from=crc16(str);

		strcpy(str,"->WALL<-");
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
		smb_freemsgmem(&msg); }
    close(file); }


while(!feof(stream)) {
	if(!fread(&user,sizeof(user_t),1,stream))
		break;
	if(user.misc&(USER_FROMSMB|USER_DELETED)
		|| (last && user.updated<last && user.created<last)
		|| user.number&0xffff0000L
		|| user.number==0
		|| user.system[0]<SP
		|| user.name[0]<SP
		|| user.realname[0]<SP
		|| user.location[0]<SP
		|| user.zipcode[0]<SP
		|| user.birth[0]<SP)
		continue;

	printf("Profile: %s\n",user.name);
	memset(buf,0,BUF_LEN);

	sprintf(str,"0:%.25s\r\n",encrypt(user.name,user.number));
	strcat(buf,str);
	sprintf(str,"1:%.25s\r\n",encrypt(user.realname,user.number));
    strcat(buf,str);

	sprintf(str,"2:%.8s\r\n",encrypt(user.birth,user.number));
    strcat(buf,str);

	sprintf(str,"3:%.30s\r\n",encrypt(user.location,user.number));
    strcat(buf,str);

	sprintf(str,"4:%.10s\r\n",encrypt(user.zipcode,user.number));
    strcat(buf,str);
	sprintf(str,"5:%.10s\r\n",encrypt(user.min_zipcode,user.number));
    strcat(buf,str);
	sprintf(str,"6:%.10s\r\n",encrypt(user.max_zipcode,user.number));
    strcat(buf,str);
	sprintf(str,"7:%.4s\r\n",encrypt(user.mbtype,user.number));
	strcat(buf,str);

	sprintf(str,"A:%.50s\r\n",encrypt(user.note[0],user.number));
	strcat(buf,str);
	sprintf(str,"B:%.50s\r\n",encrypt(user.note[1],user.number));
    strcat(buf,str);
	sprintf(str,"C:%.50s\r\n",encrypt(user.note[2],user.number));
    strcat(buf,str);
	sprintf(str,"D:%.50s\r\n",encrypt(user.note[3],user.number));
    strcat(buf,str);
	sprintf(str,"E:%.50s\r\n",encrypt(user.note[4],user.number));
    strcat(buf,str);

	sprintf(str,"F:%c%c\r\n",user.sex,user.pref_sex);
	strcat(buf,str);

	sprintf(str,"G:%x\r\n",user.marital);
	strcat(buf,str);
	sprintf(str,"H:%x\r\n",user.pref_marital);
	strcat(buf,str);

	sprintf(str,"I:%x\r\n",user.race);
	strcat(buf,str);
	sprintf(str,"J:%x\r\n",user.pref_race);
    strcat(buf,str);

	sprintf(str,"K:%x\r\n",user.hair);
	strcat(buf,str);
	sprintf(str,"L:%x\r\n",user.pref_hair);
    strcat(buf,str);

	sprintf(str,"M:%x\r\n",user.eyes);
	strcat(buf,str);
	sprintf(str,"N:%x\r\n",user.pref_eyes);
    strcat(buf,str);

	sprintf(str,"O:%x\r\nP:%x\r\nQ:%x\r\n"
		,user.weight,user.min_weight,user.max_weight);
	strcat(buf,str);

	sprintf(str,"R:%x\r\nS:%x\r\nT:%x\r\n"
		,user.height,user.min_height,user.max_height);
    strcat(buf,str);

	sprintf(str,"U:%x\r\nV:%x\r\nW:%x\r\n"
		,user.min_age,user.max_age,user.purity);
	strcat(buf,str);

	sprintf(str,"X:%lx\r\nY:%lx\r\nZ:%lx\r\n!:%lx\r\n"
		,user.income,user.min_income,user.max_income,time(NULL));
	strcat(buf,str);

	for(i=0;i<5;i++) {
		if(!user.queans[i].name[0])
			continue;
		sprintf(str,"*%d%.8s\r\n",i,encrypt(user.queans[i].name,user.number));
		strcat(buf,str);
		for(j=0;j<20;j++) {
			sprintf(str," %04x %04x"
				,user.queans[i].self[j]
				,user.queans[i].pref[j]);
			strcat(buf,str);
			if(!((j+1)%7))
				strcat(buf,"\r\n"); }
		strcat(buf,"\r\n"); }

	strcat(buf,"\r\n--- SMM2MSG v");
	strcat(buf,VERSION);

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
	msg.hdr.version=SMB_VERSION;
	msg.hdr.when_written.time=time(NULL);
    
	msg.hdr.offset=offset;

	strcpy(str,"SMM");
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

	sprintf(str,"%lu",user.number);
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
	smb_freemsgmem(&msg); }
return(0);
}

