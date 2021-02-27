/* SMB2SMM */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* Scans SMB message base for messages to "SMM" and adds them to the SMM    */
/* database. */

#define  uint unsigned int

#define LOOP_NOPEN 500

#include <dos.h>
#include "smblib.h"
#include "smmdefs.h"
#include "nodedefs.h"
#include "crc32.h"

#define VERSION "2.01"

extern int daylight=0;
extern long timezone=0L;

unsigned _stklen=16000; 		  /* Set stack size in code, not header */

char data_dir[128];

smb_t smb;
FILE *trashfile=NULL;

uchar cryptchar(uchar ch, ulong seed)
{
if(ch==0xfe)
	return(1);
if(ch<0x20 || ch&0x80)	/* Ctrl chars and ex-ASCII are not xlated */
	return(ch);
return(ch^(seed&0x1f));
}

void decrypt(char *str, ulong seed)
{
	char out[1024];
	int i,j;

j=strlen(str);
for(i=0;i<j;i++)
	out[i]=cryptchar(str[i],seed^(i&7));
out[i]=0;
strcpy(str,out);
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

/***************************************************************************/
/* Truncates white-space chars off end of 'str' and terminates at first CR  */
/****************************************************************************/
void truncsp(char *str)
{
	char c;

str[strcspn(str,"\r")]=0;
c=strlen(str);
while(c && (uchar)str[c-1]<=SP) c--;
str[c]=0;
}

/****************************************************************************/
/* Converts an ASCII Hex string into an ulong                       */
/****************************************************************************/
ulong ahtoul(char *str)
{
	ulong l,val=0;

while(*str>' ' && (l=(*str++)|0x20)!=0x20)
	val=(l&0xf)+(l>>6&1)*9+val*16;
return(val);
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
/* Returns the length of the file in 'filespec'                             */
/****************************************************************************/
long flength(char *filespec)
{
    struct ffblk f;

if(findfirst(filespec,&f,0)==0)
    return(f.ff_fsize);
return(-1L);
}


/****************************************************************************/
/* Converts a date string in format MM/DD/YY into unix time format			*/
/****************************************************************************/
time_t dstrtounix(char *str)
{
	struct date date;
	struct time curtime;

if(!strncmp(str,"00/00/00",8))
	return(0);
curtime.ti_hour=curtime.ti_min=curtime.ti_sec=0;
if(str[6]<'7')
	date.da_year=2000+((str[6]&0xf)*10)+(str[7]&0xf);
else
	date.da_year=1900+((str[6]&0xf)*10)+(str[7]&0xf);
date.da_mon=((str[0]&0xf)*10)+(str[1]&0xf);
date.da_day=((str[3]&0xf)*10)+(str[4]&0xf);
return(dostounix(&date,&curtime));
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
/* Returns the age derived from the string 'birth' in the format MM/DD/YY	*/
/****************************************************************************/
char getage(char *birth)
{
	char age;
	struct date date;

if(birth[0]<=SP)
	return(0);
getdate(&date);
age=(date.da_year-1900)-(((birth[6]&0xf)*10)+(birth[7]&0xf));
if(age>90)
	age-=90;
if(atoi(birth)>12 || atoi(birth+3)>31)
	return(0);
if(((birth[0]&0xf)*10)+(birth[1]&0xf)>date.da_mon ||
	(((birth[0]&0xf)*10)+(birth[1]&0xf)==date.da_mon &&
	((birth[3]&0xf)*10)+(birth[4]&0xf)>date.da_day))
	age--;
if(age<0)
	return(0);
return(age);
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
/* Network open function. Opens all files DENYALL and retries LOOP_NOPEN    */
/* number of times if the attempted file is already open or denying access  */
/* for some other reason.	All files are opened in BINARY mode.			*/
/****************************************************************************/
int nopen(char *str, int access)
{
	char logstr[256];
	int file,share,count=0;

if(access&O_DENYNONE) share=O_DENYNONE;
else if(access==O_RDONLY) share=O_DENYWRITE;
else share=O_DENYALL;
while(((file=open(str,O_BINARY|share|access,S_IWRITE))==-1)
	&& errno==EACCES && count++<LOOP_NOPEN)
	if(count>10)
	   delay(55);
if(file==-1 && errno==EACCES)
	puts("\7\nNOPEN: ACCESS DENIED\n\7");
return(file);
}


/****************************************************************************/
/* Creates a short message for 'usernumber' than contains 'strin'			*/
/****************************************************************************/
void putsmsg(int usernumber, char *strin)
{
	char str[256];
	int file,i;
	struct ffblk ff;
    node_t node;

sprintf(str,"%sMSGS\\%4.4u.MSG",data_dir,usernumber);
if((file=nopen(str,O_WRONLY|O_CREAT|O_APPEND))==-1) {
	printf("\7Error opening/creating %s for creat/append access\n",str);
	return; }
i=strlen(strin);
if(write(file,strin,i)!=i) {
	close(file);
	printf("\7Error writing %u bytes to %s\n",i,str);
	return; }
close(file);
}

void puttgram(int usernumber, char *strin)
{
	char str[256];
	int file,i;
	struct ffblk ff;
    node_t node;

sprintf(str,"%4.4u.MSG",usernumber);
if((file=nopen(str,O_WRONLY|O_CREAT|O_APPEND))==-1) {
	printf("\7Error opening/creating %s for creat/append access\n",str);
	return; }
i=strlen(strin);
if(write(file,strin,i)!=i) {
	close(file);
	printf("\7Error writing %u bytes to %s\n",i,str);
	return; }
close(file);
}

int trash(char *instr)
{
	char str[1024],word[128];

if(!trashfile)
	return(0);
strcpy(str,instr);
strupr(str);
rewind(trashfile);
while(!ferror(trashfile)) {
	if(!fgets(word,125,trashfile))
		break;
	truncsp(word);
	if(!word[0])
		continue;
	strupr(word);
	if(strstr(str,word))
		return(1); }
return(0);
}

int main(int argc, char **argv)
{
	uchar	str[256],system[128],telegram[1024],HUGE16 *buf,HUGE16 *hp, *p
			,min_age,fname[64],path[128],tmp[128],ch;
	char	sys[128];
	int 	i,j,file,out,wallfile,msgfile,tgramonly=0,wallonly=0,esc;
	ulong	l,m,last,high,system_crc,crc,length;
	ushort	smm,smm_photo,touser;
	user_t	user;
	wall_t	wall;
	time_t	now;
	smbmsg_t msg;
	FILE	*stream,*index,*tmpfile;
	ixb_t	ixb;
	struct tm* tm;

fprintf(stderr,"\nSMB2SMM v%s - Updates SMM via SMB - Developed 1995-1997 "
	"Rob Swindell\n\n",VERSION);

if(argc<3) {
	fprintf(stderr,"usage: smb2smm <smb_file> <smm.dab>\n\n");
	fprintf(stderr,"example: smb2smm c:\\sbbs\\data\\subs\\syncdata "
		"c:\\sbbs\\xtrn\\smm\\smm.dab\n");
	return(1); }

p=getenv("SBBSNODE");
if(p==NULL) {
	printf("\7\nSBBSNODE environment variable not set.\n");
	exit(1); }
strcpy(str,p);
if(str[strlen(str)-1]!='\\')
	strcat(str,"\\");
strcat(str,"XTRN.DAT");
if((file=nopen(str,O_RDONLY))==-1 || (stream=fdopen(file,"rb"))==NULL) {
	printf("\7\nCan't open %s\n",str);
	exit(1); }
fgets(str,81,stream);		/* user name */
fgets(system,81,stream);	/* system name */
truncsp(system);
fgets(str,81,stream);		/* sysop name */
fgets(str,81,stream);		/* guru name */
fgets(str,81,stream);		/* ctrl dir */
fgets(str,81,stream);		/* data dir */
truncsp(str);
if(str[0]=='.') {
	strcpy(data_dir,p); 	/* node dir */
	if(data_dir[strlen(data_dir)-1]!='\\')
		strcat(data_dir,"\\");
	strcat(data_dir,str); }
else
	strcpy(data_dir,str);
fclose(stream);

if(argc>3 && !stricmp(argv[3],"/t"))
	tgramonly=1;

if(argc>3 && !stricmp(argv[3],"/w"))
	wallonly=1;

strcpy(smb.file,argv[1]);
strupr(smb.file);

strcpy(str,argv[2]);
strupr(str);
if((file=open(str,O_RDWR|O_BINARY|O_DENYNONE|O_CREAT,S_IWRITE|S_IREAD))==-1) {
	printf("error opening %s\n",str);
	return(1); }
if((stream=fdopen(file,"r+b"))==NULL) {
	printf("error fdopening %s\n",str);
	return(1); }
setvbuf(stream,NULL,_IOFBF,4096);

p=strrchr(str,'.');
if(!p) p=str;
else p++;
strcpy(p,"IXB");
if((file=open(str,O_RDWR|O_BINARY|O_DENYNONE|O_CREAT,S_IWRITE|S_IREAD))==-1) {
	printf("error opening %s\n",str);
	return(1); }
if((index=fdopen(file,"r+b"))==NULL) {
	printf("error fdopening %s\n",str);
	return(1); }
setvbuf(index,NULL,_IOFBF,1024);

p=strrchr(str,'.');
if(!p) p=str;
else p++;
strcpy(p,"CAN");
trashfile=NULL;
if((file=open(str,O_RDONLY|O_DENYNONE))!=-1) {
	trashfile=fdopen(file,"rb");
	setvbuf(trashfile,NULL,_IOFBF,4096); }

p=strrchr(str,'.');
if(!p) p=str;
else p++;
strcpy(p,"CFG");
if((file=nopen(str,O_RDONLY))==-1) {
	printf("error opening %s\n",str);
    return(1); }
if((tmpfile=fdopen(file,"rb"))==NULL) {
	printf("error fdopening %s\n",str);
    return(1); }
fgets(str,128,tmpfile); /* Min purity age */
fgets(str,128,tmpfile); /* Min profile age */
truncsp(str);
min_age=atoi(str);
fclose(tmpfile);

sprintf(str,"%s.SMM",smb.file);
if((file=open(str,O_RDWR|O_BINARY|O_CREAT,S_IWRITE|S_IREAD))==-1) {
	printf("error opening %s\n",str);
	return(1); }
if(read(file,&last,4)!=4)
	last=0;
high=last;

sprintf(str,"%s.SHD",smb.file);
if(!fexist(str)) {
	printf("%s doesn't exist\n",smb.file);
	return(0); }
sprintf(str,"%s.SID",smb.file);
if(!flength(str)) {
	printf("%s is empty\n",smb.file);
    return(0); }
fprintf(stderr,"Opening %s\n",smb.file);
smb.retry_time=30;
if((i=smb_open(&smb))!=0) {
	printf("smb_open returned %d\n",i);
	return(1); }

smm=crc16("smm");
smm_photo=crc16("smm photo");

if((i=smb_locksmbhdr(&smb))!=0) {				/* Be sure noone deletes or */
	printf("Error locking %d\n",i);             /* adds while we're reading */
	return(1); }

now=time(NULL);
tm=localtime(&now);
rewind(smb.sid_fp);
while(!feof(smb.sid_fp) && !ferror(smb.sid_fp)) {
	if(!fread(&msg.idx,sizeof(idxrec_t),1,smb.sid_fp))
        break;
	fprintf(stderr,"\r#%-5lu  ",msg.idx.number);
	if(msg.idx.to!=smm && msg.idx.to!=smm_photo)
		continue;
	if(msg.idx.number<=last || msg.idx.attr&MSG_DELETE)
		continue;
	high=msg.idx.number;
	if((i=smb_lockmsghdr(&smb,&msg))!=0) {
		printf("\7Error %d locking msg #%lu\n",i,msg.idx.number);
		continue; }
	if((i=smb_getmsghdr(&smb,&msg))!=0) {
		smb_unlockmsghdr(&smb,&msg);
		printf("\7Error %d reading msg #%lu\n",i,msg.idx.number);
		continue; }

	smb_unlockmsghdr(&smb,&msg);
	if(!msg.from_net.type || !strnicmp(msg.from,system,25)) { // ignore local msg
		smb_freemsgmem(&msg);
		continue; }

	printf("%02u/%02u/%02u "
		,tm->tm_mon+1,tm->tm_mday,TM_YEAR(tm->tm_year));

	printf("From: %-25.25s  To: %-25.25s  Subj: %s\n"
		,msg.from,msg.to,msg.subj);

	if(msg.idx.to==smm_photo) {
		buf=smb_getmsgtxt(&smb,&msg,0);
		if(!buf) {
			smb_freemsgmem(&msg);
			continue; }
		sprintf(str,"%.4s",msg.subj);
		l=ahtoul(str);
		l^=0x0191;
		sprintf(sys,"%.8s",msg.subj+4);
		system_crc=ahtoul(sys);
		system_crc^=0x90120e71;
		fprintf(stderr,"Searching...");

		rewind(index);
		memset(&user,0,sizeof(user_t));
		while(1) {
			if(!fread(&ixb,sizeof(ixb_t),1,index)) {
				memset(&user,0,sizeof(user_t));
				break; }
			if(ixb.number==l && ixb.system==system_crc) {
				fseek(index,ftell(index)-sizeof(ixb_t),SEEK_SET);
				fseek(stream,(ftell(index)/sizeof(ixb_t))*sizeof(user_t)
					,SEEK_SET);
				fread(&user,sizeof(user_t),1,stream);
				break; } }
		fprintf(stderr,"\n");

		if(!user.number) {
			printf("Profile (%ld@%s) Not found\n",l,sys);
			smb_freemsgmem(&msg);
			continue; }

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
		sprintf(str,".%.3s",msg.subj+20);
		strcat(fname,str);
		strupr(fname);

		if((out=nopen(fname,O_CREAT|O_WRONLY|O_TRUNC))==-1) {
			printf("Error opening %s\n",fname);
			smb_freemsgmem(&msg);
			continue; }
		
		crc=0xffffffffUL;
		esc=0;
		for(l=0;buf[l]!=CR;l++)
			;
		buf[l]=0;
		length=ahtoul((char *)buf)^4096;
		l+=2;	/* Skip CRLF */
		for(m=0;buf[l] && m<length;l++) {
			ch=buf[l];
			if(ch<SP)
				continue;
			if(ch=='`') {
				if(esc) {
					write(out,&ch,1);
					m++;
					crc=ucrc32(ch,crc);
					esc=0; }
				else
					esc=1;
				continue; }
			if(esc) {
				if(isalpha(ch)) {
					if(isupper(ch))
						ch-='@';
					else if(ch=='g')
						ch=0xe3;
					else if(ch=='h')
						ch=0x8d;
					else if(ch=='i')
						ch=0xee;
					else
						ch=0xfa+(ch-'a'); }
				else
					ch=0xf0+(ch-'0');
				write(out,&ch,1);
				m++;
				crc=ucrc32(ch,crc);
				esc=0;
				continue; }
			if(ch>=0xf0)
				ch&=0xf;
			if(ch==0xee)
				ch=0xff;
			write(out,&ch,1);
			m++;
			crc=ucrc32(ch,crc); }
		close(out);
		crc=~crc;
		crc^=0x05296328L;
		sprintf(str,"%.8s",msg.subj+12);
		smb_freemsgmem(&msg);
		if(crc!=ahtoul(str)) {
			printf("CRC error (%lx != %lx) in %s!\n"
				,crc, ahtoul(str), fname);
			/* remove(fname); */
			continue; 
		}
		sprintf(path,"PHOTO\\%s",fname);
		remove(path);	/* remove existing photo with same name */
        mkdir("PHOTO");
		if(rename(fname,path)) {
			printf("Error renaming %s to %s!\n",fname,path);
			/* remove(fname); */
			continue; 
		}
		user.misc|=USER_PHOTO;
		fseek(stream,(ftell(index)/sizeof(ixb_t))*sizeof(user_t),SEEK_SET);
		fwrite(&user,sizeof(user_t),1,stream);
		if(!strnicmp(user.system,system,25))
			putsmsg(user.number
				,"\1n\1h\1mYour photo has been imported into "
					"\1wMatch Maker\1n\7\r\n");
		continue; }

	if(!stricmp(msg.subj,"->WALL<-")) {
		buf=smb_getmsgtxt(&smb,&msg,0);
		smb_freemsgmem(&msg);
		if(!buf)
			continue;
		memset(&wall,0,sizeof(wall_t));
		hp=buf;
		while(*hp && *hp<SP) hp++;
		for(i=0;*hp>=SP && i<25;i++,hp++)
			wall.name[i]=*hp;
		wall.name[i]=0;
		hp+=2;	 /* Skip CRLF */
		for(i=0;*hp>=SP && i<40;i++,hp++)
			wall.system[i]=*hp;
		wall.system[i]=0;
		hp+=2;	 /* Skip CRLF */
		for(i=0;i<5;i++) {
			for(j=0;*hp>=SP && j<70;j++,hp++)
				wall.text[i][j]=*hp;
			wall.text[i][j]=0;
			hp+=2; }

		wall.written=ahtoul((char *)hp);
		wall.imported=time(NULL);
		FREE((char *)buf);
		decrypt(wall.name,wall.written);
		decrypt(wall.system,wall.written);
		for(i=0;i<5;i++) {
			decrypt(wall.text[i],wall.written);
			if(trash(wall.text[i])) {
				printf("Rejected: Wall writing found in trash!\n");
				break; } }
		if(i<5)
			continue;
		if((wallfile=sopen("WALL.DAB",O_WRONLY|O_BINARY|O_CREAT,SH_DENYNO
			,S_IWRITE|S_IREAD))==-1) {
			printf("Couldn't open WALL.DAB!\n");
			continue; }
		lseek(wallfile,0L,SEEK_END);
		write(wallfile,&wall,sizeof(wall_t));
		close(wallfile);
		continue; }

	if(wallonly) {
		smb_freemsgmem(&msg);
		continue; }

	if(!stricmp(msg.subj,"->ALL<-")
		|| !stricmp(msg.subj,"->SYS<-")
		|| !stricmp(msg.subj,"->ONE<-")) {
		sprintf(str,"%.3s%05lu.MSG",msg.subj+2,msg.idx.number);
		buf=smb_getmsgtxt(&smb,&msg,0);
		smb_freemsgmem(&msg);
		if(!buf)
            continue;
		l=ahtoul((char *)buf);
		if(!l)
            continue;
		for(i=0;buf[i];i++)
            if(buf[i]==LF)
                break;
        if(buf[i]!=LF)
            continue;
		if((msgfile=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
			printf("error opening %s\n",str);
			continue; }
		if((tmpfile=fdopen(msgfile,"wb"))==NULL) {
			close(msgfile);
			printf("error fdopening %s\n",str);
			continue; }
		fprintf(tmpfile,"%08lx\r\n",l);
		l^=0x305F6C81UL;
		for(i++;buf[i];i++)
			fputc(cryptchar(buf[i],l^(i&7)),tmpfile);
		fclose(tmpfile);
		putsmsg(1,"\1n\1h\1mNew announcement in \1wMatch Maker\1n\7\r\n");
		continue; }


	j=strlen(msg.subj);
	for(i=0;i<j;i++)
		if(!isdigit(msg.subj[i]))
			break;
	if(i<j) {				/* at least one non-digit, must be telegram */
		if(strnicmp(msg.subj,system,25)) {
			smb_freemsgmem(&msg);
			continue; }
		buf=smb_getmsgtxt(&smb,&msg,0);
		smb_freemsgmem(&msg);
		if(!buf)
			continue;
		l=0;
		while(buf[l] && buf[l]!='}')         /* Find first text on line */
			l++;
		if(!buf[l] || !buf[++l]) {
			FREE(buf);
			continue; }
		touser=ahtoul((char *)buf+l);
		while(buf[l] && buf[l]>=SP) 	/* Go to end of line */
			l++;
		while(buf[l] && buf[l]<=SP) 	/* next line */
			l++;
		i=0;
		while(buf[l]) {
			if(buf[l]==LF && buf[l+1]==CR) { /* blank line */
				telegram[i++]=LF;
				break; }
			telegram[i++]=buf[l++]; }
		telegram[i]=0;
		decrypt(telegram,touser);
		printf("Telegram to %u\n",touser);
		if(trash(telegram))
			printf("Rejected: Contents in trash!\n");
		else {
			putsmsg(touser,TGRAM_NOTICE);
			puttgram(touser,telegram); }
		FREE(buf);
		continue;
		}

	if(tgramonly) {
		smb_freemsgmem(&msg);
		continue; }

	if(!msg.from[0]) {
		smb_freemsgmem(&msg);
		printf("Blank 'from' field.\n");
        continue; }

	l=atol(msg.subj);
	if(!l || l>65535L) {
		smb_freemsgmem(&msg);
		printf("Invalid profile.\n");
		continue; }
	fprintf(stderr,"Searching for %lu @ %s",l,msg.from);
	sprintf(str,"%.25s",msg.from);
	strupr(str);
	system_crc=crc32(str);

	rewind(index);
	memset(&user,0,sizeof(user_t));
	while(1) {
		if(!fread(&ixb,sizeof(ixb_t),1,index)) {
			memset(&user,0,sizeof(user_t));
			break; }
		if(ixb.number==l && ixb.system==system_crc) {
			fseek(index,ftell(index)-sizeof(ixb_t),SEEK_SET);
			fseek(stream,(ftell(index)/sizeof(ixb_t))*sizeof(user_t),SEEK_SET);
			fread(&user,sizeof(user_t),1,stream);
			if(!(user.misc&USER_FROMSMB)) {
				memset(&user,0,sizeof(user_t));
				fread(&ixb,sizeof(ixb_t),1,index);
				continue; }
			break; } }
	fprintf(stderr,"\n");

	if(!user.number) {
		fprintf(stderr,"Searching for unused record...");
		rewind(index);
		while(1) {					/* Find deleted record */
			if(!fread(&ixb,sizeof(ixb_t),1,index))
				break;
			if(ixb.number==0) {
				fseek(index,ftell(index)-sizeof(ixb_t),SEEK_SET);
				break; } }
		user.created=time(NULL);
		user.number=l;
		sprintf(user.system,"%-.40s",msg.from);
		fprintf(stderr,"\n"); }

	fseek(stream,(ftell(index)/sizeof(ixb_t))*sizeof(user_t),SEEK_SET);
	user.updated=time(NULL);
	user.misc|=USER_FROMSMB;
	buf=smb_getmsgtxt(&smb,&msg,0);
	l=0;
	while(buf[l]) {
		while(buf[l] && buf[l]<=SP) 		/* Find first text on line */
			l++;
		if(!strnicmp((char *)buf+l,"0:",2)) {
			l+=2;
			sprintf(user.name,"%-.25s",(char *)buf+l);
			truncsp(user.name);
			decrypt(user.name,user.number); }
		if(!strnicmp((char *)buf+l,"1:",2)) {
			l+=2;
			sprintf(user.realname,"%-.25s",(char *)buf+l);
			truncsp(user.realname);
			decrypt(user.realname,user.number); }
		if(!strnicmp((char *)buf+l,"2:",2)) {
			l+=2;
			sprintf(user.birth,"%-.8s",(char *)buf+l);
			decrypt(user.birth,user.number); }
		if(!strnicmp((char *)buf+l,"3:",2)) {
			l+=2;
			sprintf(user.location,"%-.30s",(char *)buf+l);
			truncsp(user.location);
			decrypt(user.location,user.number); }
		if(!strnicmp((char *)buf+l,"4:",2)) {
            l+=2;
			sprintf(user.zipcode,"%-.10s",(char *)buf+l);
            truncsp(user.zipcode);
			decrypt(user.zipcode,user.number); }
		if(!strnicmp((char *)buf+l,"5:",2)) {
			l+=2;
			sprintf(user.min_zipcode,"%-.10s",(char *)buf+l);
			truncsp(user.min_zipcode);
			decrypt(user.min_zipcode,user.number); }
		if(!strnicmp((char *)buf+l,"6:",2)) {
			l+=2;
			sprintf(user.max_zipcode,"%-.10s",(char *)buf+l);
			truncsp(user.max_zipcode);
			decrypt(user.max_zipcode,user.number); }
		if(!strnicmp((char *)buf+l,"7:",2)) {
			l+=2;
			sprintf(user.mbtype,"%-.4s",(char *)buf+l);
			truncsp(user.mbtype);
			decrypt(user.mbtype,user.number); }

		if(!strnicmp((char *)buf+l,"A:",2)) {
			l+=2;
			sprintf(user.note[0],"%-.50s",(char *)buf+l);
			truncsp(user.note[0]);
			decrypt(user.note[0],user.number); }
		if(!strnicmp((char *)buf+l,"B:",2)) {
			l+=2;
			sprintf(user.note[1],"%-.50s",(char *)buf+l);
			truncsp(user.note[1]);
			decrypt(user.note[1],user.number); }
		if(!strnicmp((char *)buf+l,"C:",2)) {
			l+=2;
			sprintf(user.note[2],"%-.50s",(char *)buf+l);
			truncsp(user.note[2]);
			decrypt(user.note[2],user.number); }
		if(!strnicmp((char *)buf+l,"D:",2)) {
			l+=2;
			sprintf(user.note[3],"%-.50s",(char *)buf+l);
			truncsp(user.note[3]);
			decrypt(user.note[3],user.number); }
		if(!strnicmp((char *)buf+l,"E:",2)) {
			l+=2;
			sprintf(user.note[4],"%-.50s",(char *)buf+l);
			truncsp(user.note[4]);
			decrypt(user.note[4],user.number); }

		if(!strnicmp((char *)buf+l,"F:",2)) {
			l+=2;
			user.sex=buf[l];
			user.pref_sex=buf[l+1]; }

		if(!strnicmp((char *)buf+l,"G:",2)) {
			l+=2;
			user.marital=ahtoul((char *)buf+l); }
		if(!strnicmp((char *)buf+l,"H:",2)) {
			l+=2;
			user.pref_marital=ahtoul((char *)buf+l); }

		if(!strnicmp((char *)buf+l,"I:",2)) {
			l+=2;
			user.race=ahtoul((char *)buf+l); }
		if(!strnicmp((char *)buf+l,"J:",2)) {
			l+=2;
			user.pref_race=ahtoul((char *)buf+l); }

		if(!strnicmp((char *)buf+l,"K:",2)) {
			l+=2;
			user.hair=ahtoul((char *)buf+l); }
		if(!strnicmp((char *)buf+l,"L:",2)) {
			l+=2;
			user.pref_hair=ahtoul((char *)buf+l); }

		if(!strnicmp((char *)buf+l,"M:",2)) {
			l+=2;
			user.eyes=ahtoul((char *)buf+l); }
		if(!strnicmp((char *)buf+l,"N:",2)) {
			l+=2;
			user.pref_eyes=ahtoul((char *)buf+l); }

		if(!strnicmp((char *)buf+l,"O:",2)) {
			l+=2;
			user.weight=ahtoul((char *)buf+l); }
		if(!strnicmp((char *)buf+l,"P:",2)) {
			l+=2;
			user.min_weight=ahtoul((char *)buf+l); }
		if(!strnicmp((char *)buf+l,"Q:",2)) {
			l+=2;
			user.max_weight=ahtoul((char *)buf+l); }

		if(!strnicmp((char *)buf+l,"R:",2)) {
			l+=2;
			user.height=ahtoul((char *)buf+l); }
		if(!strnicmp((char *)buf+l,"S:",2)) {
			l+=2;
			user.min_height=ahtoul((char *)buf+l); }
		if(!strnicmp((char *)buf+l,"T:",2)) {
			l+=2;
			user.max_height=ahtoul((char *)buf+l); }

		if(!strnicmp((char *)buf+l,"U:",2)) {
			l+=2;
			user.min_age=ahtoul((char *)buf+l); }
		if(!strnicmp((char *)buf+l,"V:",2)) {
			l+=2;
			user.max_age=ahtoul((char *)buf+l); }

		if(!strnicmp((char *)buf+l,"W:",2)) {
			l+=2;
			user.purity=ahtoul((char *)buf+l); }

		if(!strnicmp((char *)buf+l,"X:",2)) {
			l+=2;
			user.income=ahtoul((char *)buf+l); }
		if(!strnicmp((char *)buf+l,"Y:",2)) {
			l+=2;
			user.min_income=ahtoul((char *)buf+l); }
		if(!strnicmp((char *)buf+l,"Z:",2)) {
			l+=2;
			user.max_income=ahtoul((char *)buf+l); }

		if(toupper(buf[l])=='*' && isdigit(buf[l+1])) {  /* Questionnaires */
			i=buf[l+1]-'0';
			l+=2;
			sprintf(user.queans[i].name,"%-.8s",(char *)buf+l);
			truncsp(user.queans[i].name);
			decrypt(user.queans[i].name,user.number);
			while(buf[l] && buf[l]>=SP) l++;	/* Go to end of line */
			for(j=0;j<20;j++) {
				while(buf[l] && buf[l]<=SP) l++;
				user.queans[i].self[j]=ahtoul((char *)buf+l);
				l+=5;
				user.queans[i].pref[j]=ahtoul((char *)buf+l);
				l+=5; } }

		while(buf[l] && buf[l]>=SP) 	/* Go to end of line */
			l++; }

	if(getage(user.birth)<min_age)		/* Too young */
		printf("Rejected: User's age (%u) less than minimum age (%u)\n"
			,getage(user.birth),min_age);
	else if(user.name[0]<SP || user.realname[0]<SP || user.system[0]<SP
		|| user.location[0]<SP || user.zipcode[0]<SP || user.birth[0]<SP)
		printf("Rejected: Invalid user string\n");
	else if(trash(user.name))
		printf("Rejected: User's name (%s) in trash!\n",user.name);
	else if(trash(user.location))
		printf("Rejected: User's location (%s) in trash!\n",user.location);
	else if(trash(user.note[0]) || trash(user.note[1]) || trash(user.note[2])
		|| trash(user.note[3]) || trash(user.note[4]))
		printf("Rejected: User's personal text in trash!\n");
	else {
		fwrite(&user,sizeof(user_t),1,stream);
		strupr(user.name);
		ixb.name=crc32(user.name);
		strupr(user.system);
		user.system[25]=0;
		ixb.system=crc32(user.system);
		ixb.number=user.number;
		ixb.updated=user.updated;
		fwrite(&ixb,sizeof(ixb_t),1,index); }
	FREE((char *)buf);
	smb_freemsgmem(&msg);
	}

lseek(file,0L,SEEK_SET);
write(file,&high,4);
close(file);
return(0);
}

