/* ADDFILES.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* Program to add files to a Synchronet file database */

#include "sbbs.h"

#define ADDFILES_VER "2.24"

char tmp[256];
char *crlf="\r\n";

#ifdef __TURBOC__
unsigned _stklen=16000;
#else
unsigned _stacksize=16000;
#endif

int cur_altpath=0;

long files=0,removed=0,mode=0;

#define DEL_LIST	(1L<<0)
#define NO_EXTEND	(1L<<1)
#define FILE_DATE	(1L<<2)
#define TODAYS_DATE (1L<<3)
#define FILE_ID 	(1L<<4)
#define NO_UPDATE	(1L<<5)
#define NO_NEWDATE	(1L<<6)
#define ASCII_ONLY	(1L<<7)
#define UL_STATS	(1L<<8)
#define ULDATE_ONLY (1L<<9)
#define KEEP_DESC	(1L<<10)
#define AUTO_ADD	(1L<<11)
#define SEARCH_DIR	(1L<<12)
#define SYNC_LIST	(1L<<13)
#define KEEP_SPACE	(1L<<14)

#ifndef __FLAT__
/****************************************************************************/
/* This function reads files that are potentially larger than 32k.  		*/
/* Up to one megabyte of data can be read with each call.                   */
/****************************************************************************/
long lread(int file, char HUGE16 *buf,long bytes)
{
	long count;

for(count=bytes;count>32767;count-=32767,buf+=32767)
	if(read(file,(char *)buf,32767)!=32767)
		return(-1L);
if(read(file,(char *)buf,(int)count)!=count)
	return(-1L);
return(bytes);
}

/****************************************************************************/
/* This function writes files that are potentially larger than 32767 bytes  */
/* Up to one megabytes of data can be written with each call.				*/
/****************************************************************************/
long lwrite(int file, char HUGE16 *buf, long bytes)
{

	long count;

for(count=bytes;count>32767;count-=32767,buf+=32767)
	if(write(file,(char *)buf,32767)!=32767)
		return(-1L);
if(write(file,(char *)buf,(int)count)!=count)
	return(-1L);
return(bytes);
}
#endif

long lputs(char FAR16 *str)
{
    char tmp[256];
    int i,j,k;

j=strlen(str);
for(i=k=0;i<j;i++)      /* remove CRs */
    if(str[i]==CR && str[i+1]==LF)
        continue;
    else
        tmp[k++]=str[i];
tmp[k]=0;
return(fputs(tmp,stdout));
}

/****************************************************************************/
/* Returns string for 2 digit hex+ numbers up to 575						*/
/****************************************************************************/
char *hexplus(uint num, char *str)
{
sprintf(str,"%03x",num);
str[0]=num/0x100 ? 'f'+(num/0x10)-0xf : str[1];
str[1]=str[2];
str[2]=0;
return(str);
}


/****************************************************************************/
/* Places into 'strout', 'strin' starting at 'start' and ending at			*/
/* 'start'+'length'   														*/
/****************************************************************************/
void putrec(char *strout,int start,int length,char *strin)
{
	int i=0,j;

j=strlen(strin);
while(i<j && i<length)
	strout[start++]=strin[i++];
while(i++<length)
	strout[start++]=ETX;
}

/****************************************************************************/
/* Performs printf() through local assembly routines                        */
/* Called from everywhere                                                   */
/****************************************************************************/
int lprintf(char *fmat, ...)
{
	va_list argptr;
	char sbuf[256];
	int chcount;

va_start(argptr,fmat);
chcount=vsprintf(sbuf,fmat,argptr);
va_end(argptr);
lputs(sbuf);
return(chcount);
}

/****************************************************************************/
/* Truncates white-space chars off end of 'str' and terminates at first tab */
/****************************************************************************/
void truncsp(char *str)
{
	int c;

str[strcspn(str,"\t")]=0;
c=strlen(str);
while(c && (uchar)str[c-1]<=SP) c--;
str[c]=0;
}

/****************************************************************************/
/* Puts a backslash on path strings 										*/
/****************************************************************************/
void backslash(char *str)
{
    int i;

i=strlen(str);
if(i && str[i-1]!='\\') {
    str[i]='\\'; str[i+1]=0; }
}

/****************************************************************************/
/* Checks the disk drive for the existence of a file. Returns 1 if it       */
/* exists, 0 if it doesn't.                                                 */
/****************************************************************************/
char fexist(char *filespec)
{
	struct find_t f;
	uint i;

i=_dos_findfirst(filespec,_A_NORMAL,&f);
#ifdef __WATCOMC__
_dos_findclose(&f);
#endif
if(!i)
    return(1);
return(0);
}

/****************************************************************************/
/* Returns the time/date of the file in 'filespec' in time_t (unix) format  */
/****************************************************************************/
long fdate(char *filespec)
{
	int 	file;
	ushort	fd,ft;
	struct	tm t;

if((file=nopen(filespec,O_RDONLY))==-1)
	return(0);
_dos_getftime(file,&fd,&ft);
close(file);
memset(&t,0,sizeof(t));
t.tm_year=((fd&0xfe00)>>9)+80;
t.tm_mon=((fd&0x01e0)>>5)-1;
t.tm_mday=fd&0x1f;
t.tm_hour=(ft&0xf800)>>11;
t.tm_min=(ft&0x07e0)>>5;
t.tm_sec=(ft&0x001f)<<1;
return(mktime(&t));
}

/*****************************************************************************/
/* Returns command line generated from instr with %c replacments             */
/*****************************************************************************/
char *cmdstr(char *instr, char *fpath, char *fspec, char *outstr)
{
    static char cmd[128];
    char str[256];
    int i,j,len;

len=strlen(instr);
for(i=j=0;i<len && j<128;i++) {
    if(instr[i]=='%') {
        i++;
        cmd[j]=0;
        switch(toupper(instr[i])) {
            case 'F':   /* File path */
                strcat(cmd,fpath);
                break;
            case 'G':   /* Temp directory */
                if(temp_dir[0]!='\\' && temp_dir[1]!=':')
                    strcat(cmd,node_dir);
                strcat(cmd,temp_dir);
                break;
            case 'N':   /* Node Directory (same as SBBSNODE environment var) */
                strcat(cmd,node_dir);
                break;
            case 'S':   /* File Spec */
                strcat(cmd,fspec);
                break;
            case '!':   /* EXEC Directory */
                if(exec_dir[0]!='\\' && exec_dir[1]!=':')
                    strcat(cmd,node_dir);
                strcat(cmd,exec_dir);
                break;
            case '#':   /* Node number (same as SBBSNNUM environment var) */
                sprintf(str,"%d",node_num);
                strcat(cmd,str);
                break;
            case '%':   /* %% for percent sign */
                strcat(cmd,"%");
                break;
            default:    /* unknown specification */
                break; }
        j=strlen(cmd); }
    else
        cmd[j++]=instr[i]; }
cmd[j]=0;

return(cmd);
}


/****************************************************************************/
/* Converts unix time format (long - time_t) into a char str MM/DD/YY		*/
/****************************************************************************/
char *unixtodstr(time_t unix, char *str)
{
	struct tm *t;

if(!unix)
	strcpy(str,"00/00/00");
else {
	t=gmtime(&unix);
	sprintf(str,"%02u/%02u/%02u",t->tm_mon+1,t->tm_mday
		,TM_YEAR(t->tm_year)); }
return(str);
}

/****************************************************************************/
/* Network open function. Opens all files DENYALL and retries LOOP_NOPEN    */
/* number of times if the attempted file is already open or denying access	*/
/* for some other reason.	All files are opened in BINARY mode.			*/
/****************************************************************************/
int nopen(char *str, int access)
{
	char logstr[256];
	int file,share,count=0;

if(access==O_RDONLY) share=SH_DENYWR;
	else share=SH_DENYRW;
while(((file=sopen(str,O_BINARY|access,share,S_IWRITE))==-1)
	&& errno==EACCES && count++<LOOP_NOPEN);
if(file==-1 && errno==EACCES)
	lputs("\7\r\nNOPEN: ACCESS DENIED\r\n\7");
return(file);
}

/****************************************************************************/
/* This function performs an nopen, but returns a file stream with a buffer */
/* allocated.																*/
/****************************************************************************/
FILE *fnopen(int *file, char *str, int access)
{
	char mode[128];
	FILE *stream;

if(access&O_WRONLY) access|=O_RDWR; 	/* fdopen can't open WRONLY */

if(((*file)=nopen(str,access))==-1)
	return(NULL);

if(access&O_APPEND) {
	if(access&(O_RDONLY|O_RDWR))
		strcpy(mode,"a+");
	else
		strcpy(mode,"a"); }
else {
	if(access&(O_WRONLY|O_RDWR))
		strcpy(mode,"r+");
	else
		strcpy(mode,"r"); }
stream=fdopen((*file),mode);
if(stream==NULL) {
	close(*file);
	return(NULL); }
setvbuf(stream,NULL,_IOFBF,16*1024);
return(stream);
}

void allocfail(uint size)
{
lprintf("\7Error allocating %u bytes of memory.\r\n",size);
bail(1);
}

void bail(int code)
{
exit(code);
}

/****************************************************************************/
/* Returns the length of the file in 'filespec'                             */
/****************************************************************************/
long flength(char *filespec)
{
	struct find_t f;
	uint i;

i=_dos_findfirst(filespec,_A_NORMAL,&f);
#ifdef __WATCOMC__
_dos_findclose(&f);
#endif
if(!i)
	return(f.size);
return(-1L);
}

/****************************************************************************/
/* Turns FILE.EXT into FILE    .EXT                                         */
/* Called from upload                                                       */
/****************************************************************************/
char *padfname(char *filename, char *str)
{
    char c,d;

for(c=0;c<8;c++)
    if(filename[c]=='.' || !filename[c]) break;
    else str[c]=filename[c];
d=c;
if(filename[c]=='.') c++;
while(d<8)
    str[d++]=SP;
str[d++]='.';
while(d<12)
    if(!filename[c]) break;
    else str[d++]=filename[c++];
while(d<12)
    str[d++]=SP;
str[d]=0;
return(str);
}

/****************************************************************************/
/* Turns FILE    .EXT into FILE.EXT                                         */
/****************************************************************************/
char *unpadfname(char *filename, char *str)
{
    char c,d;

for(c=0,d=0;c<strlen(filename);c++)
    if(filename[c]!=SP) str[d++]=filename[c];
str[d]=0;
return(str);
}

/****************************************************************************/
/* Checks  directory data file for 'filename' (must be padded). If found,   */
/* it returns the 1, else returns 0.                                        */
/* Called from upload and bulkupload                                        */
/****************************************************************************/
char findfile(uint dirnum, char *filename)
{
	char str[256],c,fname[128],HUGE16 *ixbbuf;
    int file;
    ulong length,l;

strcpy(fname,filename);
for(c=8;c<12;c++)   /* Turn FILENAME.EXT into FILENAMEEXT */
    fname[c]=fname[c+1];
sprintf(str,"%s%s.IXB",dir[dirnum]->data_dir,dir[dirnum]->code);
if((file=nopen(str,O_RDONLY))==-1) return(0);
length=filelength(file);
if(!length) {
    close(file);
    return(0); }
if((ixbbuf=(char *)MALLOC(length))==NULL) {
    close(file);
	printf("ERR_ALLOC %s %lu\n",str,length);
    return(0); }
if(lread(file,ixbbuf,length)!=length) {
    close(file);
	FREE((char *)ixbbuf);
    return(0); }
close(file);
for(l=0;l<length;l+=F_IXBSIZE) {
    for(c=0;c<11;c++)
        if(fname[c]!=ixbbuf[l+c]) break;
    if(c==11) break; }
FREE((char *)ixbbuf);
if(l<length)
    return(1);
return(0);
}


/****************************************************************************/
/* Adds the data for struct filedat to the directory's data base.           */
/* changes the .datoffset field only                                        */
/****************************************************************************/
char addfiledat(file_t *f)
{
	char str[256],fdat[F_LEN+1],fname[128],idx[128],c,HUGE16 *ixbbuf;
    int i,file;
    ulong length,l,uldate;

/************************/
/* Add data to DAT File */
/************************/
sprintf(str,"%s%s.DAT",dir[f->dir]->data_dir,dir[f->dir]->code);
if((file=nopen(str,O_RDWR|O_CREAT))==-1) {
	printf("ERROR opening %s\n",str);
	return(0); }
length=filelength(file);
if(length==0L)
    l=0L;
else {
    if(length%F_LEN) {
        close(file);
		printf("ERR_LEN %s %lu\n",str,length);
		return(0); }
    for(l=0;l<length;l+=F_LEN) {    /* Find empty slot */
        lseek(file,l,SEEK_SET);
        read(file,&c,1);
        if(c==ETX) break; } }
putrec(fdat,F_CDT,7,ultoa(f->cdt,tmp,10));
putrec(fdat,F_CDT+7,2,crlf);
putrec(fdat,F_DESC,LEN_FDESC,f->desc);
putrec(fdat,F_DESC+LEN_FDESC,2,crlf);
putrec(fdat,F_ULER,LEN_ALIAS+5,f->uler);
putrec(fdat,F_ULER+LEN_ALIAS+5,2,crlf);
putrec(fdat,F_TIMESDLED,5,ultoa(f->timesdled,tmp,10));
putrec(fdat,F_TIMESDLED+5,2,crlf);
putrec(fdat,F_OPENCOUNT,3,itoa(f->opencount,tmp,10));
putrec(fdat,F_OPENCOUNT+3,2,crlf);
fdat[F_MISC]=f->misc+SP;
putrec(fdat,F_ALTPATH,2,hexplus(f->altpath,tmp));
putrec(fdat,F_ALTPATH+2,2,crlf);
f->datoffset=l;
idx[0]=l&0xff;          /* Get offset within DAT file for IXB file */
idx[1]=(l>>8)&0xff;
idx[2]=(l>>16)&0xff;
lseek(file,l,SEEK_SET);
if(write(file,fdat,F_LEN)!=F_LEN) {
    close(file);
	return(0); }
length=filelength(file);
close(file);
if(length%F_LEN)
	printf("ERR_LEN %s %lu\n",str,length);

/*******************************************/
/* Update last upload date/time stamp file */
/*******************************************/
sprintf(str,"%s%s.DAB",dir[f->dir]->data_dir,dir[f->dir]->code);
if((file=nopen(str,O_WRONLY|O_CREAT))==-1)
	printf("ERR_OPEN %s\r\n",str);
else {
	uldate=time(NULL);
	write(file,&uldate,4);
    close(file); }

/************************/
/* Add data to IXB File */
/************************/
strcpy(fname,f->name);
for(i=8;i<12;i++)   /* Turn FILENAME.EXT into FILENAMEEXT */
    fname[i]=fname[i+1];
sprintf(str,"%s%s.IXB",dir[f->dir]->data_dir,dir[f->dir]->code);
if((file=nopen(str,O_RDWR|O_CREAT))==-1) {
	printf("ERR_OPEN %s\n",str);
	return(0); }
length=filelength(file);
if(length) {    /* IXB file isn't empty */
    if(length%F_IXBSIZE) {
        close(file);
		printf("ERR_LEN %s %lu\n",str,length);
		return(0); }
	if((ixbbuf=(char *)MALLOC(length))==NULL) {
        close(file);
		printf("ERR_ALLOC %s %lu\n",str,length);
		return(0); }
	if(lread(file,ixbbuf,length)!=length) {
        close(file);
		printf("ERR_READ %s %lu\n",str,length);
		FREE((char *)ixbbuf);
		return(0); }
/************************************************/
/* Sort by Name or Date, Assending or Decending */
/************************************************/
    if(dir[f->dir]->sort==SORT_NAME_A || dir[f->dir]->sort==SORT_NAME_D) {
        for(l=0;l<length;l+=F_IXBSIZE) {
            for(i=0;i<12 && fname[i]==ixbbuf[l+i];i++);
			if(i==12) { 	/* file already in directory index */
                close(file);
				printf("ERR_CHK %s\n",fname);
				FREE((char *)ixbbuf);
				return(0); }
            if(dir[f->dir]->sort==SORT_NAME_A && fname[i]<ixbbuf[l+i])
                break;
            if(dir[f->dir]->sort==SORT_NAME_D && fname[i]>ixbbuf[l+i])
                break; } }
    else {  /* sort by date */
        for(l=0;l<length;l+=F_IXBSIZE) {
            uldate=(ixbbuf[l+15]|((long)ixbbuf[l+16]<<8)
                |((long)ixbbuf[l+17]<<16)|((long)ixbbuf[l+18]<<24));
            if(dir[f->dir]->sort==SORT_DATE_A && f->dateuled>uldate)
                break;
            if(dir[f->dir]->sort==SORT_DATE_D && f->dateuled<uldate)
                break; } }
    lseek(file,l,SEEK_SET);
    if(write(file,fname,11)!=11) {  /* Write filename to IXB file */
        close(file);
		FREE((char *)ixbbuf);
		return(0); }
    if(write(file,idx,3)!=3) {  /* Write DAT offset into IXB file */
        close(file);
		FREE((char *)ixbbuf);
		return(0); }
    write(file,&f->dateuled,sizeof(time_t));
    write(file,&f->datedled,4);              /* Write 0 for datedled */
	if(lwrite(file,&ixbbuf[l],length-l)!=length-l) { /* Write rest of IXB */
        close(file);
		FREE((char *)ixbbuf);
		return(0); }
	FREE((char *)ixbbuf); }
else {              /* IXB file is empty... No files */
    if(write(file,fname,11)!=11) {  /* Write filename it IXB file */
        close(file);
		return(0); }
    if(write(file,idx,3)!=3) {  /* Write DAT offset into IXB file */
        close(file);
		return(0); }
    write(file,&f->dateuled,sizeof(time_t));
    write(file,&f->datedled,4); }
length=filelength(file);
close(file);
if(length%F_IXBSIZE)
	printf("ERR_LEN %s %lu\n",str,length);
return(1);
}

void putextdesc(uint dirnum, ulong datoffset, char *ext)
{
	char str[256],nulbuf[512];
	int file;

sprintf(str,"%s%s.EXB",dir[dirnum]->data_dir,dir[dirnum]->code);
if((file=nopen(str,O_WRONLY|O_CREAT))==-1)
	return;
lseek(file,0L,SEEK_END);
while(filelength(file)<(datoffset/F_LEN)*512L)
    write(file,nulbuf,512);
lseek(file,(datoffset/F_LEN)*512L,SEEK_SET);
write(file,ext,512);
close(file);
}

/****************************************************************************/
/* Gets file data from dircode.IXB file										*/
/* Need fields .name and .dir filled.                                       */
/* only fills .offset, .dateuled, and .datedled                             */
/****************************************************************************/
void getfileixb(file_t *f)
{
	uchar HUGE16 *ixbbuf,str[256],fname[128];
	int file;
	ulong l,length;

sprintf(str,"%s%s.IXB",dir[f->dir]->data_dir,dir[f->dir]->code);
if((file=nopen(str,O_RDONLY))==-1) {
	printf("ERR_OPEN %s\n",str);
	return; }
length=filelength(file);
if(length%F_IXBSIZE) {
	close(file);
	printf("ERR_LEN %s\n");
	return; }
if((ixbbuf=(uchar HUGE16 *)MALLOC(length))==NULL) {
	close(file);
	printf("ERR_ALLOC %s\n",str);
	return; }
if(lread(file,ixbbuf,length)!=length) {
	close(file);
	FREE((char *)ixbbuf);
	printf("ERR_READ %s\n",str);
	return; }
close(file);
strcpy(fname,f->name);
for(l=8;l<12;l++)	/* Turn FILENAME.EXT into FILENAMEEXT */
	fname[l]=fname[l+1];
for(l=0;l<length;l+=F_IXBSIZE) {
	sprintf(str,"%11.11s",ixbbuf+l);
	if(!strcmp(str,fname))
		break; }
if(l>=length) {
	printf("getfileixb: ERR_CHK %s\n",fname);
	FREE((char *)ixbbuf);
	return; }
l+=11;
f->datoffset=ixbbuf[l]|((long)ixbbuf[l+1]<<8)|((long)ixbbuf[l+2]<<16);
f->dateuled=ixbbuf[l+3]|((long)ixbbuf[l+4]<<8)
	|((long)ixbbuf[l+5]<<16)|((long)ixbbuf[l+6]<<24);
f->datedled=ixbbuf[l+7]|((long)ixbbuf[l+8]<<8)
	|((long)ixbbuf[l+9]<<16)|((long)ixbbuf[l+10]<<24);
FREE((char *)ixbbuf);
}

/****************************************************************************/
/* Puts filedata into DIR_code.DAT file                                     */
/* Called from removefiles                                                  */
/****************************************************************************/
void putfiledat(file_t f)
{
    char buf[F_LEN+1],str[256];
    int file;
    long length;

putrec(buf,F_CDT,7,ultoa(f.cdt,tmp,10));
putrec(buf,F_CDT+7,2,crlf);
putrec(buf,F_DESC,LEN_FDESC,f.desc);
putrec(buf,F_DESC+LEN_FDESC,2,crlf);
putrec(buf,F_ULER,LEN_ALIAS+5,f.uler);
putrec(buf,F_ULER+LEN_ALIAS+5,2,crlf);
putrec(buf,F_TIMESDLED,5,itoa(f.timesdled,tmp,10));
putrec(buf,F_TIMESDLED+5,2,crlf);
putrec(buf,F_OPENCOUNT,3,itoa(f.opencount,tmp,10));
putrec(buf,F_OPENCOUNT+3,2,crlf);
buf[F_MISC]=f.misc+SP;
putrec(buf,F_ALTPATH,2,hexplus(f.altpath,tmp));
putrec(buf,F_ALTPATH+2,2,crlf);
sprintf(str,"%s%s.DAT",dir[f.dir]->data_dir,dir[f.dir]->code);
if((file=nopen(str,O_WRONLY))==-1) {
	printf("ERR_OPEN %s\n",str);
    return; }
length=filelength(file);
if(length%F_LEN) {
    close(file);
	printf("ERR_LEN %s\n",str);
    return; }
if(f.datoffset>length) {
    close(file);
	printf("ERR_LEN %s\n",str);
    return; }
lseek(file,f.datoffset,SEEK_SET);
if(write(file,buf,F_LEN)!=F_LEN) {
    close(file);
	printf("ERR_WRITE %s\n",str);
    return; }
length=filelength(file);
close(file);
if(length%F_LEN)
	printf("ERR_LEN %s\n",str);
}

/****************************************************************************/
/* Update the upload date for the file 'f'                                  */
/****************************************************************************/
void update_uldate(file_t f)
{
	char str[256],fname[128];
	int i,file;
	long l,length;

/*******************/
/* Update IXB File */
/*******************/
sprintf(str,"%s%s.IXB",dir[f.dir]->data_dir,dir[f.dir]->code);
if((file=nopen(str,O_RDWR))==-1) {
	printf("ERR_OPEN %s\n",str);
    return; }
length=filelength(file);
if(length%F_IXBSIZE) {
    close(file);
	printf("ERR_LEN %s\n",str);
    return; }
strcpy(fname,f.name);
for(i=8;i<12;i++)   /* Turn FILENAME.EXT into FILENAMEEXT */
    fname[i]=fname[i+1];
for(l=0;l<length;l+=F_IXBSIZE) {
    read(file,str,F_IXBSIZE);      /* Look for the filename in the IXB file */
    str[11]=0;
    if(!strcmp(fname,str)) break; }
if(l>=length) {
    close(file);
	printf("ERR_CHK %s\n",f.name);
    return; }
lseek(file,l+14,SEEK_SET);
write(file,&f.dateuled,4);	/* Write the current time stamp for datedled */
close(file);

/*******************************************/
/* Update last upload date/time stamp file */
/*******************************************/
sprintf(str,"%s%s.DAB",dir[f.dir]->data_dir,dir[f.dir]->code);
if((file=nopen(str,O_WRONLY|O_CREAT))==-1)
	printf("ERR_OPEN %s\r\n",str);
else {
	write(file,&f.dateuled,4);
    close(file); }

}


void strip_ctrl(char *str)
{
	char tmp[1024];
	int i,j,k;

k=strlen(str);
for(i=j=0;i<k;i++)
	if(j && str[i]==SP && tmp[j-1]==SP && (mode&KEEP_SPACE))
		tmp[j++]=str[i];
	else if(j && str[i]<=SP && tmp[j-1]==SP)
		continue;
	else if(i && !isalnum(str[i]) && str[i]==str[i-1])
		continue;
	else if((uchar)str[i]>=SP)
		tmp[j++]=str[i];
	else if(str[i]==TAB || (str[i]==CR && str[i+1]==LF))
		tmp[j++]=SP;
tmp[j]=0;
strcpy(str,tmp);
}


void strip_exascii(char *str)
{
	char tmp[1024];
	int i,j,k;

k=strlen(str);
for(i=j=0;i<k;i++)
	if(!(str[i]&0x80))
		tmp[j++]=str[i];
tmp[j]=0;
strcpy(str,tmp);
}

/****************************************************************************/
/* Updates dstst.dab file													*/
/****************************************************************************/
void updatestats(ulong size)
{
    char str[256];
    int i,file;
	ulong l;

sprintf(str,"%sDSTS.DAB",ctrl_dir);
if((file=nopen(str,O_RDWR))==-1) {
	printf("ERR_OPEN %s\n",str);
	return; }
lseek(file,20L,SEEK_SET);	/* Skip timestamp, logons and logons today */
read(file,&l,4);			/* Uploads today		 */
l++;
lseek(file,-4L,SEEK_CUR);
write(file,&l,4);
read(file,&l,4);			/* Upload bytes today	 */
l+=size;
lseek(file,-4L,SEEK_CUR);
write(file,&l,4);
close(file);
}

void addlist(char *inpath, file_t f, char dskip, char sskip)
{
	uchar str[256],fname[256],listpath[256],filepath[256]
		,curline[256],nextline[256],*p,exist,ext[1024],tmpext[513];
	int i,file;
	long l;
	FILE *stream;
	struct find_t ff;

if(mode&SEARCH_DIR) {
	strcpy(str,cur_altpath ? altpath[cur_altpath-1] : dir[f.dir]->path);
	if(str[0]=='.') {
		sprintf(tmp,"%s%s",node_dir,str);
		strcpy(str,tmp); }
	strcat(str,"*.*");
	printf("Searching %s\n\n",str);
	for(i=_dos_findfirst(str,0,&ff);!i;i=_dos_findnext(&ff)) {
		strupr(ff.name);
		sprintf(filepath,"%s%s",cur_altpath ? altpath[cur_altpath-1]
            : dir[f.dir]->path,ff.name);
        if(filepath[0]=='.') { /* relative path */
            sprintf(tmp,"%s%s",node_dir,filepath);
            strcpy(filepath,tmp); }
		f.misc=0;
		f.desc[0]=0;
		f.cdt=flength(filepath);
		padfname(ff.name,f.name);
		printf("%s  %10lu  %s\n"
			,f.name,f.cdt,unixtodstr(fdate(filepath),str));
		exist=findfile(f.dir,f.name);
		if(exist) {
			if(mode&NO_UPDATE)
				continue;
			getfileixb(&f);
			if(mode&ULDATE_ONLY) {
				f.dateuled=time(NULL);
				update_uldate(f);
				continue; } }

		if(mode&FILE_DATE) {		/* get the file date and put into desc */
			unixtodstr(fdate(filepath),f.desc);
			strcat(f.desc,"  "); }

		if(mode&TODAYS_DATE) {		/* put today's date in desc */
			unixtodstr(time(NULL),f.desc);
			strcat(f.desc,"  "); }

		if(mode&FILE_ID) {
			for(i=0;i<total_fextrs;i++)
				if(!stricmp(fextr[i]->ext,f.name+9))
					break;
			if(i<total_fextrs) {
				sprintf(tmp,"%sFILE_ID.DIZ",temp_dir);
				remove(tmp);
				system(cmdstr(fextr[i]->cmd,filepath,"FILE_ID.DIZ",NULL));
				if(!fexist(tmp)) {
					sprintf(tmp,"%sDESC.SDI",temp_dir);
					remove(tmp);
					system(cmdstr(fextr[i]->cmd,filepath,"DESC.SDI",NULL)); }
				if((file=nopen(tmp,O_RDONLY))!=-1) {
					memset(ext,0,513);
					read(file,ext,512);
					for(i=512;i;i--)
						if(ext[i-1]>SP)
							break;
					ext[i]=0;
					if(mode&ASCII_ONLY)
						strip_exascii(ext);
					if(!(mode&KEEP_DESC)) {
						sprintf(tmpext,"%.256s",ext);
						strip_ctrl(tmpext);
						for(i=0;tmpext[i];i++)
							if(isalpha(tmpext[i]))
								break;
						sprintf(f.desc,"%.*s",LEN_FDESC,tmpext+i);
						for(i=0;f.desc[i]>=SP && i<LEN_FDESC;i++)
							;
						f.desc[i]=0; }
					close(file);
					f.misc|=FM_EXTDESC; } } }

		f.dateuled=time(NULL);
		f.altpath=cur_altpath;
		strip_ctrl(f.desc);
		if(mode&ASCII_ONLY)
			strip_exascii(f.desc);
		if(exist) {
			putfiledat(f);
			if(!(mode&NO_NEWDATE))
				update_uldate(f); }
		else
			addfiledat(&f);
		if(f.misc&FM_EXTDESC) {
			truncsp(ext);
			putextdesc(f.dir,f.datoffset,ext); }

		if(mode&UL_STATS)
			updatestats(f.cdt);
		files++; }
#ifdef __WATCOMC__
   _dos_findclose(&ff);
#endif
	return; }


strcpy(listpath,inpath);
if((stream=fnopen(&file,listpath,O_RDONLY))==NULL) {
	sprintf(listpath,"%s%s",cur_altpath ? altpath[cur_altpath-1]
			: dir[f.dir]->path,inpath);
	if(listpath[0]=='.') { /* relative path */
		sprintf(str,"%s%s",node_dir,listpath);
		strcpy(listpath,str); }
	if((stream=fnopen(&file,listpath,O_RDONLY))==NULL) {
		printf("Can't open: %s\n"
			   "        or: %s\n",inpath,listpath);
		return; } }

printf("Adding %s to %s %s\n\n"
	,listpath,lib[dir[f.dir]->lib]->sname,dir[f.dir]->sname);

fgets(nextline,255,stream);
while(!feof(stream) && !ferror(stream)) {
	f.misc=0;
	f.desc[0]=0;
	strcpy(curline,nextline);
	nextline[0]=0;
	fgets(nextline,255,stream);
	truncsp(curline);
	if(curline[0]<=SP || (uchar)curline[0]>=0x7e)
		continue;
	printf("%s\n",curline);
	strcpy(fname,curline);

	p=strchr(fname,'.');
	if(!p || p==fname || p>fname+8)    /* no dot or invalid dot location */
		continue;
	p=strchr(p,SP);
	if(p) *p=0;
	else				   /* no space after filename? */
		continue;
	strupr(fname);
	strcpy(fname,unpadfname(fname,tmp));

	padfname(fname,f.name);
	if(strcspn(f.name,"\\/|<>+[]:=\";,")!=strlen(f.name))
		continue;

    for(i=0;i<12;i++)
        if(f.name[i]<SP || (uchar)f.name[i]>0x7e)
            break;

	if(i<12)					/* Ctrl chars or EX-ASCII in filename? */
		continue;
	exist=findfile(f.dir,f.name);
    if(exist) {
        if(mode&NO_UPDATE)
            continue;
		getfileixb(&f);
		if(mode&ULDATE_ONLY) {
			f.dateuled=time(NULL);
			update_uldate(f);
			continue; } }

	sprintf(filepath,"%s%s",cur_altpath ? altpath[cur_altpath-1]
		: dir[f.dir]->path,fname);
	if(filepath[0]=='.') { /* relative path */
		sprintf(tmp,"%s%s",node_dir,filepath);
		strcpy(filepath,tmp); }

	if(mode&FILE_DATE) {		/* get the file date and put into desc */
		l=fdate(filepath);
		unixtodstr(l,f.desc);
		strcat(f.desc,"  "); }

	if(mode&TODAYS_DATE) {		/* put today's date in desc */
		l=time(NULL);
		unixtodstr(l,f.desc);
		strcat(f.desc,"  "); }

	if(dskip && strlen(curline)>=dskip) p=curline+dskip;
	else {
		p++;
		while(*p==SP) p++; }
	sprintf(tmp,"%.*s",LEN_FDESC-strlen(f.desc),p);
	strcat(f.desc,tmp);

	if(nextline[0]==SP || strlen(p)>LEN_FDESC) {	/* ext desc */
		if(!(mode&NO_EXTEND)) {
			memset(ext,0,513);
			f.misc|=FM_EXTDESC;
			sprintf(ext,"%s\r\n",p); }

		if(nextline[0]==SP) {
			strcpy(str,nextline);				   /* tack on to end of desc */
			p=str+dskip;
			i=LEN_FDESC-strlen(f.desc);
			if(i>1) {
				p[i-1]=0;
				truncsp(p);
				if(p[0]) {
					strcat(f.desc," ");
					strcat(f.desc,p); } } }

		while(!feof(stream) && !ferror(stream) && strlen(ext)<512) {
			if(nextline[0]!=SP)
				break;
			truncsp(nextline);
			printf("%s\n",nextline);
			if(!(mode&NO_EXTEND)) {
				f.misc|=FM_EXTDESC;
				p=nextline+dskip;
				while(*p==SP) p++;
				strcat(ext,p);
				strcat(ext,"\r\n"); }
			nextline[0]=0;
			fgets(nextline,255,stream); } }


	if(sskip) l=atol(fname+sskip);
	else {
		l=flength(filepath);
		if(l<1L) {
			printf("%s not found.\n",filepath);
			continue; } }

	if(mode&FILE_ID) {
        for(i=0;i<total_fextrs;i++)
            if(!stricmp(fextr[i]->ext,f.name+9))
                break;
        if(i<total_fextrs) {
            sprintf(tmp,"%sFILE_ID.DIZ",temp_dir);
            remove(tmp);
			system(cmdstr(fextr[i]->cmd,filepath,"FILE_ID.DIZ",NULL));
            if(!fexist(tmp)) {
                sprintf(tmp,"%sDESC.SDI",temp_dir);
                remove(tmp);
				system(cmdstr(fextr[i]->cmd,filepath,"DESC.SDI",NULL)); }
			if((file=nopen(tmp,O_RDONLY))!=-1) {
				memset(ext,0,513);
				read(file,ext,512);
				for(i=512;i;i--)
					if(ext[i-1]>SP)
						break;
				ext[i]=0;
				if(mode&ASCII_ONLY)
					strip_exascii(ext);
				if(!(mode&KEEP_DESC)) {
					sprintf(tmpext,"%.256s",ext);
					strip_ctrl(tmpext);
					for(i=0;tmpext[i];i++)
						if(isalpha(tmpext[i]))
							break;
					sprintf(f.desc,"%.*s",LEN_FDESC,tmpext+i);
					for(i=0;f.desc[i]>=SP && i<LEN_FDESC;i++)
						;
					f.desc[i]=0; }
				close(file);
				f.misc|=FM_EXTDESC; } } }

	f.cdt=l;
	f.dateuled=time(NULL);
	f.altpath=cur_altpath;
	strip_ctrl(f.desc);
	if(mode&ASCII_ONLY)
		strip_exascii(f.desc);
	if(exist) {
		putfiledat(f);
		if(!(mode&NO_NEWDATE))
			update_uldate(f); }
	else
		addfiledat(&f);
	if(f.misc&FM_EXTDESC) {
		truncsp(ext);
		putextdesc(f.dir,f.datoffset,ext); }

	if(mode&UL_STATS)
		updatestats(l);
	files++; }
fclose(stream);
if(mode&DEL_LIST && !(mode&SYNC_LIST)) {
	printf("\nDeleting %s\n",listpath);
	remove(listpath); }

}

/***********************/
/* Hex-plus to integer */
/***********************/
uint hptoi(char *str)
{
	char tmp[128];
	uint i;

if(!str[1] || toupper(str[0])<='F')
	return(strtol(str,0,16));
strcpy(tmp,str);
tmp[0]='F';
i=strtol(tmp,0,16)+((toupper(str[0])-'F')*0x10);
return(i);
}

/****************************************************************************/
/* Places into 'strout' CR or ETX terminated string starting at             */
/* 'start' and ending at 'start'+'length' or terminator from 'strin'        */
/****************************************************************************/
void getrec(char *strin,int start,int length,char *strout)
{
    int i=0,stop;

stop=start+length;
while(start<stop) {
    if(strin[start]==ETX)
        break;
    strout[i++]=strin[start++]; }
strout[i]=0;
}


/****************************************************************************/
/* Gets filedata from dircode.DAT file										*/
/* Need fields .name ,.dir and .offset to get other info    				*/
/* Does not fill .dateuled or .datedled fields.                             */
/****************************************************************************/
void getfiledat(file_t *f)
{
	char buf[F_LEN+1],str[256];
	int file;
	long length;

sprintf(str,"%s%s.DAT",dir[f->dir]->data_dir,dir[f->dir]->code);
if((file=nopen(str,O_RDONLY))==-1) {
	printf("ERR_OPEN %s\n",str);
	return; }
length=filelength(file);
if(f->datoffset>length) {
	close(file);
	printf("ERR_LEN %s %lu\n",str,length);
	return; }
if(length%F_LEN) {
	close(file);
	printf("ERR_LEN %s %lu\n",str,length);
	return; }
lseek(file,f->datoffset,SEEK_SET);
if(read(file,buf,F_LEN)!=F_LEN) {
    close(file);
	printf("ERR_READ %s %lu\n",str,F_LEN);
	return; }
close(file);
getrec(buf,F_ALTPATH,2,str);
f->altpath=hptoi(str);
getrec(buf,F_CDT,7,str);
f->cdt=atol(str);
f->timetodl=0;

getrec(buf,F_DESC,LEN_FDESC,f->desc);
getrec(buf,F_ULER,LEN_ALIAS,f->uler);
getrec(buf,F_TIMESDLED,5,str);
f->timesdled=atoi(str);
getrec(buf,F_OPENCOUNT,3,str);
f->opencount=atoi(str);
if(buf[F_MISC]!=ETX)
	f->misc=buf[F_MISC]-SP;
else
	f->misc=0;
}

/****************************************************************************/
/* Removes any files in the user transfer index (XFER.IXT) that match the   */
/* specifications of dest, or source user, or filename or any combination.  */
/****************************************************************************/
void rmuserxfers(int fromuser, int destuser, char *fname)
{
    char str[256],*ixtbuf;
    int file;
    long l,length;

sprintf(str,"%sXFER.IXT",data_dir);
if(!fexist(str))
    return;
if(!flength(str)) {
    remove(str);
    return; }
if((file=nopen(str,O_RDONLY))==-1) {
	printf("ERR_OPEN %s\n",str);
    return; }
length=filelength(file);
if((ixtbuf=(char *)MALLOC(length))==NULL) {
    close(file);
	printf("ERR_ALLOC %s\n",str,length);
    return; }
if(read(file,ixtbuf,length)!=length) {
    close(file);
	FREE(ixtbuf);
	printf("ERR_READ %s %lu\n",str,length);
    return; }
close(file);
if((file=nopen(str,O_WRONLY|O_TRUNC))==-1) {
	FREE(ixtbuf);
	printf("ERR_OPEN %s\n",str);
    return; }
for(l=0;l<length;l+=24) {
    if(fname!=NULL && fname[0]) {               /* fname specified */
        if(!strncmp(ixtbuf+l+5,fname,12)) {     /* this is the file */
            if(destuser && fromuser) {          /* both dest and from user */
                if(atoi(ixtbuf+l)==destuser && atoi(ixtbuf+l+18)==fromuser)
                    continue; }                 /* both match */
            else if(fromuser) {                 /* from user */
                if(atoi(ixtbuf+l+18)==fromuser) /* matches */
                    continue; }
            else if(destuser) {                 /* dest user */
                if(atoi(ixtbuf+l)==destuser)    /* matches */
                    continue; }
            else continue; } }                  /* no users, so match */
    else if(destuser && fromuser) {
        if(atoi(ixtbuf+l+18)==fromuser && atoi(ixtbuf+l)==destuser)
            continue; }
    else if(destuser && atoi(ixtbuf+l)==destuser)
        continue;
    else if(fromuser && atoi(ixtbuf+l+18)==fromuser)
        continue;
    write(file,ixtbuf+l,24); }
close(file);
FREE(ixtbuf);
}



/****************************************************************************/
/* Removes DAT and IXB entries for the file in the struct 'f'               */
/****************************************************************************/
void removefiledat(file_t f)
{
	char c,str[256],ixbname[12],HUGE16 *ixbbuf,fname[13];
    int file;
    ulong l,length;

strcpy(fname,f.name);
for(c=8;c<12;c++)   /* Turn FILENAME.EXT into FILENAMEEXT */
    fname[c]=fname[c+1];
sprintf(str,"%s%s.IXB",dir[f.dir]->data_dir,dir[f.dir]->code);
if((file=nopen(str,O_RDONLY))==-1) {
	printf("ERR_OPEN %s\n",str);
    return; }
length=filelength(file);
if(!length) {
	close(file);
	return; }
if((ixbbuf=(char *)MALLOC(length))==0) {
    close(file);
	printf("ERR_ALLOC %s %lu\n",str,length);
    return; }
if(lread(file,ixbbuf,length)!=length) {
    close(file);
	printf("ERR_READ %s %lu\n",str,length);
	FREE((char *)ixbbuf);
    return; }
close(file);
if((file=nopen(str,O_WRONLY|O_TRUNC))==-1) {
	printf("ERR_OPEN %s\n",str);
    return; }
for(l=0;l<length;l+=F_IXBSIZE) {
    for(c=0;c<11;c++)
        ixbname[c]=ixbbuf[l+c];
    ixbname[c]=0;
    if(strcmp(ixbname,fname))
		if(lwrite(file,&ixbbuf[l],F_IXBSIZE)!=F_IXBSIZE) {
            close(file);
			printf("ERR_WRITE %s %lu\n",str,F_IXBSIZE);
			FREE((char *)ixbbuf);
            return; } }
FREE((char *)ixbbuf);
close(file);
sprintf(str,"%s%s.DAT",dir[f.dir]->data_dir,dir[f.dir]->code);
if((file=nopen(str,O_WRONLY))==-1) {
	printf("ERR_OPEN %s\n",str);
    return; }
lseek(file,f.datoffset,SEEK_SET);
c=ETX;          /* If first char of record is ETX, record is unused */
if(write(file,&c,1)!=1) { /* So write a D_T on the first byte of the record */
    close(file);
	printf("ERR_WRITE %s 1",str);
    return; }
close(file);
if(f.dir==user_dir)  /* remove file from index */
    rmuserxfers(0,0,f.name);

}


void synclist(char *inpath, int dirnum)
{
	uchar str[1024],fname[256],listpath[256], HUGE16 *ixbbuf,*p;
	int i,file,found;
	long l,m,length;
	FILE *stream;
	file_t f;

sprintf(str,"%s%s.IXB",dir[dirnum]->data_dir,dir[dirnum]->code);
if((file=nopen(str,O_RDONLY))==-1) {
	printf("ERR_OPEN %s\n",str);
	return; }
length=filelength(file);
if(length%F_IXBSIZE) {
	close(file);
	printf("ERR_LEN %s\n");
	return; }
if((ixbbuf=(uchar HUGE16 *)MALLOC(length))==NULL) {
	close(file);
	printf("ERR_ALLOC %s\n",str);
	return; }
if(lread(file,ixbbuf,length)!=length) {
	close(file);
	FREE((char *)ixbbuf);
	printf("ERR_READ %s\n",str);
	return; }
close(file);

strcpy(listpath,inpath);
if((stream=fnopen(&file,listpath,O_RDONLY))==NULL) {
	sprintf(listpath,"%s%s",cur_altpath ? altpath[cur_altpath-1]
			: dir[dirnum]->path,inpath);
	if(listpath[0]=='.') { /* relative path */
		sprintf(str,"%s%s",node_dir,listpath);
		strcpy(listpath,str); }
	if((stream=fnopen(&file,listpath,O_RDONLY))==NULL) {
		printf("Can't open: %s\n"
			   "        or: %s\n",inpath,listpath);
		return; } }

printf("\nSynchronizing %s with %s %s\n\n"
	,listpath,lib[dir[dirnum]->lib]->sname,dir[dirnum]->sname);

for(l=0;l<length;l+=F_IXBSIZE) {
	m=l;
	for(i=0;i<12 && l<length;i++)
		if(i==8)
			str[i]='.';
		else
			str[i]=ixbbuf[m++]; 	/* Turns FILENAMEEXT into FILENAME.EXT */
	str[i]=0;
	unpadfname(str,fname);
	rewind(stream);
	found=0;
	while(!found) {
		if(!fgets(str,1000,stream))
			break;
		truncsp(str);
		p=strchr(str,SP);
		if(p) *p=0;
		if(!stricmp(str,fname))
			found=1; }
	if(found)
		continue;
	padfname(fname,f.name);
	printf("%s not found in list - ",f.name);
	f.dir=dirnum;
	f.datoffset=ixbbuf[m]|((long)ixbbuf[m+1]<<8)|((long)ixbbuf[m+2]<<16);
	getfiledat(&f);
	if(f.opencount) {
		printf("currently OPEN by %u users\n",f.opencount);
		continue; }
	removefiledat(f);
	sprintf(str,"%s%s"
		,f.altpath>0 && f.altpath<=altpaths ? altpath[f.altpath-1]
		: dir[f.dir]->path,fname);
	if(str[0]=='.') {
		sprintf(tmp,"%s%s",node_dir,str);
        strcpy(str,tmp); }
	if(remove(str))
		printf("Error removing %s\n",str);
	removed++;
	printf("Removed from database\n"); }

if(mode&DEL_LIST) {
	printf("\nDeleting %s\n",listpath);
	remove(listpath); }
}

char *usage="\nusage: addfiles code [.alt_path] [/opts] [\"*user\"] +list "
			 "[desc_off] [size_off]"
	"\n   or: addfiles code [.alt_path] [/opts] [\"*user\"]  file "
		"\"description\"\n"
	"\navailable opts:"
	"\n       a    import ASCII only (no extended ASCII)"
	"\n       b    synchronize database with file list (use with caution)"
	"\n       c    do not remove extra spaces from file description"
	"\n       d    delete list after import"
	"\n       e    do not import extended descriptions"
	"\n       f    include file date in descriptions"
	"\n       t    include today's date in descriptions"
	"\n       i    include added files in upload statistics"
	"\n       n    do not update information for existing files"
	"\n       o    update upload date only for existing files"
	"\n       u    do not update upload date for existing files"
	"\n       z    check for and import FILE_ID.DIZ and DESC.SDI"
	"\n       k    keep original short description (not DIZ)"
	"\n       s    search directory for files (no file list)"
	"\n"
	"\nAuto-ADD:   use * in place of code for Auto-ADD of FILES.BBS"
	"\n            use *filename to Auto-ADD a different filename"
	"\n"
	;

/*********************/
/* Entry point (duh) */
/*********************/
int main(int argc, char **argv)
{
	uchar str[256],str2[256],*p,exist,listgiven=0,namegiven=0,ext[513]
		,auto_name[13]="FILES.BBS";
	int i,j,k,file;
	long l;
	read_cfg_text_t txt;
	file_t f;

putenv("TZ=UCT0");
_fmode=O_BINARY;
setvbuf(stdout,NULL,_IONBF,0);

fprintf(stderr,"\nADDFILES Version %s (%s) - Adds files to Synchronet "
	"Filebase\n"
	,ADDFILES_VER
#if defined(__OS2__)
	,"OS/2"
#elif defined(__NT__)
    ,"Win32"
#elif defined(__DOS4G__)
	,"DOS4G"
#elif defined(__FLAT__)
	,"DOS32"
#else
	,"DOS16"
#endif
	);

if(argc<2) {
	printf(usage);
	return(1); }

p=getenv("SBBSNODE");
if(p==NULL) {
	printf("\nSBBSNODE environment variable not set.\n");
	printf("\nExample: SET SBBSNODE=C:\\SBBS\\NODE1\n");
	exit(1); }

putenv("TZ=UCT0");

strcpy(node_dir,p);
if(node_dir[strlen(node_dir)-1]!='\\')
	strcat(node_dir,"\\");

txt.openerr="\7\nError opening %s for read.\n";
txt.reading="\nReading %s...";
txt.readit="\rRead %s       ";
txt.allocerr="\7\nError allocating %u bytes of memory\n";
txt.error="\7\nERROR: Offset %lu in %s\r\n\n";

read_node_cfg(txt);
if(ctrl_dir[0]=='.') {   /* Relative path */
	strcpy(str,ctrl_dir);
	sprintf(ctrl_dir,"%s%s",node_dir,str); }
read_main_cfg(txt);
if(data_dir[0]=='.') {   /* Relative path */
	strcpy(str,data_dir);
	sprintf(data_dir,"%s%s",node_dir,str); }
if(temp_dir[1]!=':' && temp_dir[0]!='\\') {
	strcpy(str,temp_dir);
	sprintf(temp_dir,"%s%s",node_dir,str); }
read_file_cfg(txt);
printf("\n\n");

if(argv[1][0]=='*') {
	if(argv[1][1])
		sprintf(auto_name,"%.12s",argv[1]+1);
	mode|=AUTO_ADD;
	i=0; }
else {
	for(i=0;i<total_dirs;i++)
		if(!stricmp(dir[i]->code,argv[1]))
			break;

	if(i>=total_dirs) {
		printf("Directory code '%s' not found.\n",argv[1]);
		exit(1); } }

memset(&f,0,sizeof(file_t));
f.dir=i;
strcpy(f.uler,"-> ADDFILES <-");

for(j=2;j<argc;j++) {
	if(argv[j][0]=='*')     /* set the uploader name */
		sprintf(f.uler,"%.25s",argv[j]+1);
	else if(argv[j][0]=='/') {      /* options */
		for(i=1;i<strlen(argv[j]);i++)
			switch(toupper(argv[j][i])) {
				case 'A':
					mode|=ASCII_ONLY;
					break;
				case 'B':
					mode|=(SYNC_LIST|NO_UPDATE);
					break;
				case 'C':
					mode|=KEEP_SPACE;
					break;
				case 'D':
					mode|=DEL_LIST;
					break;
				case 'E':
					mode|=NO_EXTEND;
					break;
				case 'I':
					mode|=UL_STATS;
					break;
				case 'Z':
					mode|=FILE_ID;
					break;
				case 'K':
					mode|=KEEP_DESC;	 /* Don't use DIZ for short desc */
					break;
				case 'N':
					mode|=NO_UPDATE;
					break;
				case 'O':
					mode|=ULDATE_ONLY;
					break;
				case 'U':
					mode|=NO_NEWDATE;
					break;
				case 'F':
					mode|=FILE_DATE;
					break;
				case 'T':
					mode|=TODAYS_DATE;
					break;
				case 'S':
					mode|=SEARCH_DIR;
					break;
				default:
					printf(usage);
					return(1); } }
	else if(argv[j][0]=='+') {      /* filelist - FILES.BBS */
		listgiven=1;
		if(isdigit(argv[j+1][0])) { /* skip x characters before description */
			if(isdigit(argv[j+2][0])) { /* skip x characters before size */
				addlist(argv[j]+1,f,atoi(argv[j+1]),atoi(argv[j+2]));
				j+=2; }
			else {
				addlist(argv[j]+1,f,atoi(argv[j+1]),0);
				j++; } }
		else
			addlist(argv[j]+1,f,0,0);
		if(mode&SYNC_LIST)
			synclist(argv[j]+1,f.dir); }
	else if(argv[j][0]=='.') {      /* alternate file path */
		cur_altpath=atoi(argv[j]+1);
		if(cur_altpath>altpaths) {
			printf("Invalid alternate path.\n");
			exit(1); } }
	else {
		namegiven=1;
		padfname(argv[j],f.name);
		f.desc[0]=0;
		strupr(f.name);
		if(j+1==argc) {
			printf("%s no description given.\n",f.name);
			continue; }
		sprintf(str,"%s%s",cur_altpath ? altpath[cur_altpath-1]
			: dir[f.dir]->path,argv[j]);
		if(str[0]=='.') {
			sprintf(tmp,"%s%s",node_dir,str);
			strcpy(str,tmp); }
		if(mode&FILE_DATE)
			sprintf(f.desc,"%s  ",unixtodstr(fdate(str),tmp));
		if(mode&TODAYS_DATE)
			sprintf(f.desc,"%s  ",unixtodstr(time(NULL),tmp));
		sprintf(tmp,"%.*s",LEN_FDESC-strlen(f.desc),argv[++j]);
		strcpy(f.desc,tmp);
		l=flength(str);
		if(l==-1) {
			printf("%s not found.\n",str);
			continue; }
		exist=findfile(f.dir,f.name);
		if(exist) {
			if(mode&NO_UPDATE)
				continue;
			getfileixb(&f);
			if(mode&ULDATE_ONLY) {
				f.dateuled=time(NULL);
				update_uldate(f);
				continue; } }
		f.cdt=l;
		f.dateuled=time(NULL);
		f.altpath=cur_altpath;
		strip_ctrl(f.desc);
		if(mode&ASCII_ONLY)
			strip_exascii(f.desc);
		printf("%s %7lu %s\n",f.name,f.cdt,f.desc);
		if(mode&FILE_ID) {
			for(i=0;i<total_fextrs;i++)
				if(!stricmp(fextr[i]->ext,f.name+9))
					break;
			if(i<total_fextrs) {
				sprintf(tmp,"%sFILE_ID.DIZ",temp_dir);
				remove(tmp);
				system(cmdstr(fextr[i]->cmd,str,"FILE_ID.DIZ",NULL));
				if(!fexist(tmp)) {
					sprintf(tmp,"%sDESC.SDI",temp_dir);
					remove(tmp);
					system(cmdstr(fextr[i]->cmd,str,"DESC.SDI",NULL)); }
				if((file=nopen(tmp,O_RDONLY))!=-1) {
					memset(ext,0,513);
					read(file,ext,512);
					if(!(mode&KEEP_DESC)) {
						sprintf(f.desc,"%.*s",LEN_FDESC,ext);
						for(i=0;f.desc[i]>=SP && i<LEN_FDESC;i++)
							;
						f.desc[i]=0; }
					close(file);
					f.misc|=FM_EXTDESC; } } }
		if(exist) {
			putfiledat(f);
			if(!(mode&NO_NEWDATE))
				update_uldate(f); }
		else
			addfiledat(&f);

		if(f.misc&FM_EXTDESC)
			putextdesc(f.dir,f.datoffset,ext);

		if(mode&UL_STATS)
			updatestats(l);
		files++; } }

if(mode&AUTO_ADD) {
	for(i=0;i<total_dirs;i++) {
		if(dir[i]->misc&DIR_NOAUTO)
			continue;
		f.dir=i;
		if(mode&SEARCH_DIR) {
			addlist("",f,0,0);
			continue; }
        sprintf(str,"%s.LST",dir[f.dir]->code);
        if(flength(str)>0L) {
			printf("Auto-adding %s\n",str);
            addlist(str,f,0,0);
			if(mode&SYNC_LIST)
				synclist(str,i);
            continue; }
		sprintf(str,"%s%s",dir[f.dir]->path,auto_name);
		if(str[0]=='.') {
			sprintf(tmp,"%s%s",node_dir,str);
			strcpy(str,tmp); }
		if(flength(str)>0L) {
			printf("Auto-adding %s\n",str);
			addlist(str,f,0,0);
			if(mode&SYNC_LIST)
				synclist(str,i);
			continue; } } }

else {
	if(!listgiven && !namegiven) {
		sprintf(str,"%s.LST",dir[f.dir]->code);
		if(flength(str)<=0L)
			strcpy(str,"FILES.BBS");
		addlist(str,f,0,0);
		if(mode&SYNC_LIST)
			synclist(str,f.dir); } }

printf("\n%lu file(s) added.",files);
if(removed)
	printf("\n%lu files(s) removed.",removed);
printf("\n");
return(0);
}

