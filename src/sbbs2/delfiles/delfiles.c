#line 1 "DELFILES.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"

#define DELFILES_VER "1.01"

char tmp[256];

#define MAX_NOTS	25

#define ALL 	(1L<<0)
#define OFFLINE (1L<<1)
#define NO_LINK (1L<<2)
#define REPORT	(1L<<3)

void bail(int code)
{
exit(code);
}

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
/* Network open function. Opens all files DENYALL and retries LOOP_NOPEN    */
/* number of times if the attempted file is aREADy open or denying access  */
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

if(((*file)=nopen(str,access))==-1) {
	printf("nopen -1\n");
	return(NULL); }

if(access&O_APPEND) {
	if(access&O_RDONLY)
		strcpy(mode,"a+");
	else
		strcpy(mode,"a"); }
else {
	if(access&O_WRONLY)
		strcpy(mode,"r+");
	else
		strcpy(mode,"r"); }
stream=fdopen((*file),mode);
if(stream==NULL) {
	printf("fdopen NULL mode='%s'\n",mode);
	close(*file);
	return(NULL); }
setvbuf(stream,NULL,_IOFBF,16*1024);
return(stream);
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
	printf("ERROR opening %s\r\n",str);
    return; }
length=filelength(file);
if((ixtbuf=(char *)MALLOC(length))==NULL) {
    close(file);
	printf("ERROR allocating %lu bytes for %s\r\n",length,str);
    return; }
if(read(file,ixtbuf,length)!=length) {
    close(file);
	FREE(ixtbuf);
	printf("ERROR reading %lu bytes from %s\r\n",length,str);
    return; }
close(file);
if((file=nopen(str,O_WRONLY|O_TRUNC))==-1) {
	FREE(ixtbuf);
	printf("ERROR opening %s\r\n",str);
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
	printf("ERROR opening %s\r\n",str);
    return; }
length=filelength(file);
if(!length) {
	close(file);
	return; }
if((ixbbuf=(char *)MALLOC(length))==0) {
    close(file);
	printf("ERROR allocating %lu bytes for %s\r\n",length,str);
    return; }
if(lread(file,ixbbuf,length)!=length) {
    close(file);
	printf("ERROR reading %lu bytes from %s\r\n",length,str);
	FREE((char *)ixbbuf);
    return; }
close(file);
if((file=nopen(str,O_WRONLY|O_TRUNC))==-1) {
	printf("ERROR opening %s\r\n",str);
    return; }
for(l=0;l<length;l+=F_IXBSIZE) {
    for(c=0;c<11;c++)
        ixbname[c]=ixbbuf[l+c];
    ixbname[c]=0;
    if(strcmp(ixbname,fname))
		if(lwrite(file,&ixbbuf[l],F_IXBSIZE)!=F_IXBSIZE) {
            close(file);
			printf("ERROR writing %lu bytes to %s\r\n",F_IXBSIZE,str);
			FREE((char *)ixbbuf);
            return; } }
FREE((char *)ixbbuf);
close(file);
sprintf(str,"%s%s.DAT",dir[f.dir]->data_dir,dir[f.dir]->code);
if((file=nopen(str,O_WRONLY))==-1) {
	printf("ERROR opening %s\r\n",str);
    return; }
lseek(file,f.datoffset,SEEK_SET);
c=ETX;          /* If first char of record is ETX, record is unused */
if(write(file,&c,1)!=1) { /* So write a D_T on the first byte of the record */
    close(file);
	printf("ERROR writing to %s\r\n",str);
    return; }
close(file);
if(f.dir==user_dir)  /* remove file from index */
    rmuserxfers(0,0,f.name);
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
/* Returns the time/date of the file in 'filespec' in time_t (unix) format  */
/****************************************************************************/
long fdate(char *filespec)
{
	int 	file;
#ifdef __WATCOMC__
	unsigned short ft,fd;
#else
	unsigned ft,fd;
#endif
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

/****************************************************************************/
/* Converts an ASCII Hex string into an ulong                               */
/****************************************************************************/
ulong ahtoul(char *str)
{
    ulong l,val=0;

while((l=(*str++)|0x20)!=0x20)
    val=(l&0xf)+(l>>6&1)*9+val*16;
return(val);
}

uint hptoi(char *str)
{
	char tmp[128];
	uint i;

if(!str[1] || toupper(str[0])<='F')
	return(ahtoul(str));
strcpy(tmp,str);
tmp[0]='F';
i=ahtoul(tmp)+((toupper(str[0])-'F')*0x10);
return(i);
}

/****************************************************************************/
/* Gets filedata from dircode.DAT file										*/
/* Need fields .name ,.dir and .offset to get other info    				*/
/* Does not fill .dateuled or .datedled fields.                             */
/****************************************************************************/
void getfiledat(file_t *f)
{
	char buf[F_LEN+1],str[256],tmp[256];
	int file;
	long length;

sprintf(str,"%s%s.DAT",dir[f->dir]->data_dir,dir[f->dir]->code);
if((file=nopen(str,O_RDONLY))==-1) {
	printf("ERROR opening %s\r\n",str);
	return; }
length=filelength(file);
if(f->datoffset>length) {
	close(file);
	printf("ERROR %s filelength %ld is less than offset\r\n",str,length);
	return; }
if(length%F_LEN) {
	close(file);
	printf("ERROR %s filelength %ld is not evenly divisible\r\n",str,length);
	return; }
lseek(file,f->datoffset,SEEK_SET);
if(read(file,buf,F_LEN)!=F_LEN) {
	close(file);
	printf("ERROR reading %ld bytes from %s\r\n",F_LEN,str);
	return; }
close(file);
getrec(buf,F_ALTPATH,2,str);
f->altpath=hptoi(str);
getrec(buf,F_CDT,7,str);
f->cdt=atol(str);

if(!f->size) {					/* only read disk if this is null */
	if(dir[f->dir]->misc&DIR_FCHK) {
		sprintf(str,"%s%s"
			,f->altpath>0 && f->altpath<=altpaths ? altpath[f->altpath-1]
			: dir[f->dir]->path,unpadfname(f->name,tmp));
		f->size=flength(str);
		f->date=fdate(str); }
	else {
		f->size=f->cdt;
		f->date=0; } }
//if((f->size>0L) && cur_cps)
//	  f->timetodl=(f->size/(ulong)cur_cps);
//else
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

void main(int argc, char **argv)
{
	char str[256],fname[256],not[MAX_NOTS][9],nots=0,*p;
	uchar HUGE16 *ixbbuf;
	int i,j,dirnum,libnum,file;
	ulong l,m,n,length;
	long misc=0;
	time_t now;
	read_cfg_text_t txt;
	file_t workfile;
	struct find_t ff;

putenv("TZ=UCT0");
_fmode=O_BINARY;
setvbuf(stdout,NULL,_IONBF,0);

fprintf(stderr,"\nDELFILES Version %s (%s) - Removes files from Synchronet "
	"Filebase\n"
	,DELFILES_VER
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
	printf("\n   usage: DELFILES <dir_code or * for ALL> [switches]\n");
	printf("\nswitches: /LIB name All directories of specified library\n");
    printf("          /NOT code Exclude specific directory\n");
	printf("          /OFF      Remove files that are offline "
		"(don't exist on disk)\n");
	printf("          /NOL      Remove files with no link "
		"(don't exist in database)\n");
	printf("          /RPT      Report findings only "
		"(don't delete any files)\n");
	exit(0); }

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

dirnum=libnum=-1;
if(argv[1][0]=='*')
	misc|=ALL;
else if(argv[1][0]!='/') {
	strupr(argv[1]);
	for(i=0;i<total_dirs;i++)
		if(!stricmp(argv[1],dir[i]->code))
			break;
	if(i>=total_dirs) {
		printf("\nDirectory code '%s' not found.\n",argv[1]);
		exit(1); }
	dirnum=i; }
for(i=1;i<argc;i++) {
	if(!stricmp(argv[i],"/LIB")) {
		if(dirnum!=-1) {
			printf("\nBoth directory code and /LIB parameters were used.\n");
			exit(1); }
		i++;
		if(i>=argc) {
			printf("\nLibrary short name must follow /LIB parameter.\n");
			exit(1); }
		strupr(argv[i]);
		for(j=0;j<total_libs;j++)
			if(!stricmp(lib[j]->sname,argv[i]))
				break;
		if(j>=total_libs) {
			printf("\nLibrary short name '%s' not found.\n",argv[i]);
			exit(1); }
		libnum=j; }
	else if(!stricmp(argv[i],"/NOT")) {
		if(nots>=MAX_NOTS) {
			printf("\nMaximum number of /NOT options (%u) exceeded.\n"
				,MAX_NOTS);
			exit(1); }
		i++;
		if(i>=argc) {
			printf("\nDirectory internal code must follow /NOT parameter.\n");
            exit(1); }
		sprintf(not[nots++],"%.8s",argv[i]); }
	else if(!stricmp(argv[i],"/OFF"))
		misc|=OFFLINE;
	else if(!stricmp(argv[i],"/NOL"))
		misc|=NO_LINK;
	else if(!stricmp(argv[i],"/RPT"))
		misc|=REPORT;
	else if(!stricmp(argv[i],"/ALL")) {
		if(dirnum!=-1) {
			printf("\nBoth directory code and /ALL parameters were used.\n");
            exit(1); }
		if(libnum!=-1) {
			printf("\nBoth library name and /ALL parameters were used.\n");
			exit(1); }
		misc|=ALL; } }

for(i=0;i<total_dirs;i++) {
	if(!(misc&ALL) && i!=dirnum && dir[i]->lib!=libnum)
		continue;
	for(j=0;j<nots;j++)
		if(!stricmp(not[j],dir[i]->code))
			break;
	if(j<nots)
        continue;

	if(misc&NO_LINK && dir[i]->misc&DIR_FCHK) {
		strcpy(tmp,dir[i]->path);
		if(tmp[0]=='.') {
			sprintf(str,"%s%s",node_dir,tmp);
			strcpy(tmp,str); }
		sprintf(str,"%s*.*",tmp);
		printf("\nSearching %s for unlinked files\n",str);
		for(j=_dos_findfirst(str,_A_NORMAL,&ff);!j
			;j=_dos_findnext(&ff)) {
			strupr(ff.name);
			padfname(ff.name,str);
			if(!findfile(i,str)) {
				sprintf(str,"%s%s",tmp,ff.name);
				printf("Removing %s (not in database)\n",ff.name);
				if(!(misc&REPORT) && remove(str))
					printf("Error removing %s\n",str); } }
#ifdef __WATCOMC__
		_dos_findclose(&ff);
#endif
	}

	if(!dir[i]->maxage && !(misc&OFFLINE))
		continue;

	printf("\nScanning %s %s\n",lib[dir[i]->lib]->sname,dir[i]->lname);

	sprintf(str,"%s%s.IXB",dir[i]->data_dir,dir[i]->code);
	if((file=nopen(str,O_RDONLY))==-1)
		continue;
    l=filelength(file);
	if(!l) {
		close(file);
		continue; }
	if((ixbbuf=(char *)MALLOC(l))==NULL) {
		close(file);
		printf("\7ERR_ALLOC %s %lu\n",str,l);
        continue; }
	if(read(file,ixbbuf,l)!=l) {
		close(file);
		printf("\7ERR_READ %s %lu\n",str,l);
		FREE((char *)ixbbuf);
		continue; }
    close(file);

	m=0L;
	now=time(NULL);
	while(m<l) {
		memset(&workfile,0,sizeof(file_t));
		for(j=0;j<12 && m<l;j++)
			if(j==8)
				fname[j]='.';
			else
				fname[j]=ixbbuf[m++];
		fname[j]=0;
		strcpy(workfile.name,fname);
		unpadfname(workfile.name,fname);
		workfile.dir=i;
		sprintf(str,"%s%s"
			,workfile.altpath>0 && workfile.altpath<=altpaths
				? altpath[workfile.altpath-1]
			: dir[workfile.dir]->path,fname);
		if(str[0]=='.') {
			sprintf(tmp,"%s%s",node_dir,str);
			strcpy(str,tmp); }
		workfile.datoffset=ixbbuf[m]|((long)ixbbuf[m+1]<<8)
			|((long)ixbbuf[m+2]<<16);
		workfile.dateuled=(ixbbuf[m+3]|((long)ixbbuf[m+4]<<8)
			|((long)ixbbuf[m+5]<<16)|((long)ixbbuf[m+6]<<24));
		workfile.datedled=(ixbbuf[m+7]|((long)ixbbuf[m+8]<<8)
			|((long)ixbbuf[m+9]<<16)|((long)ixbbuf[m+10]<<24));
		m+=11;
		if(dir[i]->maxage && dir[i]->misc&DIR_SINCEDL && workfile.datedled
			&& (now-workfile.datedled)/86400L>dir[i]->maxage) {
				printf("Deleting %s (%ld days since last download)\n",fname
					,(now-workfile.datedled)/86400L);
				getfiledat(&workfile);
				if(!(misc&REPORT)) {
					removefiledat(workfile);
					if(remove(str))
						printf("Error removing %s\n",str); } }
		else if(dir[i]->maxage
			&& !(workfile.datedled && dir[i]->misc&DIR_SINCEDL)
			&& (now-workfile.dateuled)/86400L>dir[i]->maxage) {
				printf("Deleting %s (uploaded %ld days ago)\n",fname
					,(now-workfile.dateuled)/86400L);
				getfiledat(&workfile);
				if(!(misc&REPORT)) {
					removefiledat(workfile);
					if(remove(str))
						printf("Error removing %s\n",str); } }
		else if(misc&OFFLINE && dir[i]->misc&DIR_FCHK && !fexist(str)) {
				printf("Removing %s (doesn't exist)\n",fname);
				getfiledat(&workfile);
				if(!(misc&REPORT))
					removefiledat(workfile); } }

	FREE((char *)ixbbuf); }
}
