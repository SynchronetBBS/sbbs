/* FILELIST.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* Utility to create list of files from Synchronet file directories */
/* Default list format is FILES.BBS, but file size, uploader, upload date */
/* and other information can be included. */

#include "sbbs.h"

#define FILELIST_VER "2.12"

#define MAX_NOTS 25

#ifdef lputs
#undef lputs
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
/* Turns FILE    .EXT into FILE.EXT                                         */
/* Called from upload                                                       */
/****************************************************************************/
char *unpadfname(char *filename, char *str)
{
    char c,d;

for(c=0,d=0;c<strlen(filename);c++)
    if(filename[c]!=SP) str[d++]=filename[c];
str[d]=0;
return(str);
}

#ifndef __FLAT__
/****************************************************************************/
/* This function reads files that are potentially larger than 32k.  		*/
/* Up to one megabyte of data can be read with each call.                   */
/****************************************************************************/
long lread(int file, char huge *buf,long bytes)
{
	long count;

for(count=bytes;count>32767;count-=32767,buf+=32767)
	if(read(file,(char *)buf,32767)!=32767)
		return(-1L);
if(read(file,(char *)buf,(int)count)!=count)
	return(-1L);
return(bytes);
}
#endif
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

/****************************************************************************/
/* Converts an ASCII Hex string into an ulong                       */
/****************************************************************************/
ulong ahtoul(char *str)
{
	ulong l,val=0;

while((l=(*str++)|0x20)!=0x20)
	val=(l&0xf)+(l>>6&1)*9+val*16;
return(val);
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

void stripctrlz(char *str)
{
	char tmp[1024];
	int i,j,k;

k=strlen(str);
for(i=j=0;i<k;i++)
	if(str[i]!=0x1a)
		tmp[j++]=str[i];
tmp[j]=0;
strcpy(str,tmp);
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
/* Places into 'strout' CR or ETX terminated string starting at 			*/
/* 'start' and ending at 'start'+'length' or terminator from 'strin'		*/
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
		,t->tm_year%100); }
return(str);
}

#define ALL 	(1L<<0)
#define PAD 	(1L<<1)
#define HDR 	(1L<<2)
#define CDT_	(1L<<3)
#define EXT 	(1L<<4)
#define ULN 	(1L<<5)
#define ULD 	(1L<<6)
#define DLS 	(1L<<7)
#define DLD 	(1L<<8)
#define NOD 	(1L<<9)
#define PLUS	(1L<<10)
#define MINUS	(1L<<11)
#define JST 	(1L<<12)
#define NOE 	(1L<<13)
#define DFD 	(1L<<14)
#define TOT 	(1L<<15)
#define AUTO	(1L<<16)

/*********************/
/* Entry point (duh) */
/*********************/
int main(int argc, char **argv)
{
	char	*p,str[256],fname[256],ext,not[MAX_NOTS][9],nots=0;
	uchar	HUGE16 *datbuf,HUGE16 *ixbbuf;
	int 	i,j,file,dirnum,libnum,desc_off,lines
			,omode=O_WRONLY|O_CREAT|O_TRUNC;
	ulong	l,m,n,cdt,misc=0,total_cdt=0,total_files=0,datbuflen;
	time_t	uld,dld;
	read_cfg_text_t txt;
	FILE	*in,*out=NULL;

putenv("TZ=UCT0");
_fmode=O_BINARY;
setvbuf(stdout,NULL,_IONBF,0);

fprintf(stderr,"\nFILELIST Version %s (%s) - Generate Synchronet File "
	"Directory Lists\n"
	,FILELIST_VER
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
	printf("\n   usage: FILELIST <dir_code or * for ALL> [switches] [outfile]\n");
	printf("\nswitches: /LIB name All directories of specified library\n");
	printf("          /NOT code Exclude specific directory\n");
	printf("          /CAT      Concatenate to existing outfile\n");
	printf("          /PAD      Pad filename with spaces\n");
	printf("          /HDR      Include directory headers\n");
	printf("          /CDT      Include credit value\n");
	printf("          /TOT      Include credit totals\n");
	printf("          /ULN      Include uploader's name\n");
	printf("          /ULD      Include upload date\n");
	printf("          /DFD      Include DOS file date\n");
	printf("          /DLD      Include download date\n");
	printf("          /DLS      Include total downloads\n");
	printf("          /NOD      Exclude normal descriptions\n");
	printf("          /NOE      Exclude normal descriptions, if extended "
		"exists\n");
	printf("          /EXT      Include extended descriptions\n");
	printf("          /JST      Justify extended descriptions under normal\n");
	printf("          /+        Include extended description indicator (+)\n");
	printf("          /-        Include offline file indicator (-)\n");
	printf("          /*        Short-hand for /PAD /HDR /CDT /+ /-\n");
	exit(0); }

p=getenv("SBBSNODE");
if(p==NULL) {
	printf("\nSBBSNODE environment variable not set.\n");
	printf("\nExample: SET SBBSNODE=C:\\SBBS\\NODE1\n");
	exit(1); }

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
read_file_cfg(txt);
printf("\n");


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
	else if(!stricmp(argv[i],"/ALL")) {
		if(dirnum!=-1) {
			printf("\nBoth directory code and /ALL parameters were used.\n");
            exit(1); }
		if(libnum!=-1) {
			printf("\nBoth library name and /ALL parameters were used.\n");
			exit(1); }
		misc|=ALL; }
	else if(!stricmp(argv[i],"/PAD"))
		misc|=PAD;
	else if(!stricmp(argv[i],"/CAT"))
		omode=O_WRONLY|O_CREAT|O_APPEND;
	else if(!stricmp(argv[i],"/HDR"))
		misc|=HDR;
	else if(!stricmp(argv[i],"/CDT"))
		misc|=CDT_;
	else if(!stricmp(argv[i],"/TOT"))
		misc|=TOT;
	else if(!stricmp(argv[i],"/EXT"))
		misc|=EXT;
	else if(!stricmp(argv[i],"/ULN"))
		misc|=ULN;
	else if(!stricmp(argv[i],"/ULD"))
		misc|=ULD;
	else if(!stricmp(argv[i],"/DLD"))
		misc|=DLD;
	else if(!stricmp(argv[i],"/DFD"))
		misc|=DFD;
	else if(!stricmp(argv[i],"/DLS"))
		misc|=DLS;
	else if(!stricmp(argv[i],"/NOD"))
		misc|=NOD;
	else if(!stricmp(argv[i],"/JST"))
		misc|=(EXT|JST);
	else if(!stricmp(argv[i],"/NOE"))
		misc|=(EXT|NOE);
	else if(!stricmp(argv[i],"/+"))
		misc|=PLUS;
	else if(!stricmp(argv[i],"/-"))
		misc|=MINUS;
	else if(!stricmp(argv[i],"/*"))
		misc|=(HDR|PAD|CDT_|PLUS|MINUS);

	else if(i!=1) {
		if(argv[i][0]=='*') {
			misc|=AUTO;
			continue; }
		if((j=nopen(argv[i],omode))==-1) {
			printf("\nError opening/creating %s for output.\n",argv[i]);
			exit(1); }
		out=fdopen(j,"wb"); } }

if(!out && !(misc&AUTO)) {
	printf("\nOutput file not specified, using FILES.BBS in each "
		"directory.\n");
	misc|=AUTO; }

for(i=0;i<total_dirs;i++) {
	if(!(misc&ALL) && i!=dirnum && dir[i]->lib!=libnum)
		continue;
	for(j=0;j<nots;j++)
		if(!stricmp(not[j],dir[i]->code))
			break;
	if(j<nots)
		continue;
	if(misc&AUTO && dir[i]->seqdev) 	/* CD-ROM */
		continue;
	printf("\n%-*s %s",LEN_GSNAME,lib[dir[i]->lib]->sname,dir[i]->lname);
	sprintf(str,"%s%s.IXB",dir[i]->data_dir,dir[i]->code);
	if((file=nopen(str,O_RDONLY))==-1)
		continue;
	l=filelength(file);
	if(misc&AUTO) {
		sprintf(str,"%sFILES.BBS",dir[i]->path);
		if((j=nopen(str,omode))==-1) {
			printf("\nError opening/creating %s for output.\n",str);
			exit(1); }
		out=fdopen(j,"wb"); }
	if(misc&HDR) {
		sprintf(fname,"%-*s      %-*s       Files: %4u"
			,LEN_GSNAME,lib[dir[i]->lib]->sname
			,LEN_SLNAME,dir[i]->lname,l/F_IXBSIZE);
		fprintf(out,"%s\r\n",fname);
		strset(fname,'-');
		fprintf(out,"%s\r\n",fname); }
	if(!l) {
		close(file);
		if(misc&AUTO) fclose(out);
		continue; }
	if((ixbbuf=(char *)MALLOC(l))==NULL) {
		close(file);
		if(misc&AUTO) fclose(out);
		printf("\7ERR_ALLOC %s %lu\n",str,l);
		continue; }
	if(read(file,ixbbuf,l)!=l) {
		close(file);
		if(misc&AUTO) fclose(out);
		printf("\7ERR_READ %s %lu\n",str,l);
		FREE((char *)ixbbuf);
		continue; }
	close(file);
	sprintf(str,"%s%s.DAT",dir[i]->data_dir,dir[i]->code);
	if((file=nopen(str,O_RDONLY))==-1) {
		printf("\7ERR_OPEN %s %lu\n",str,O_RDONLY);
		FREE((char *)ixbbuf);
		if(misc&AUTO) fclose(out);
		continue; }
	datbuflen=filelength(file);
	if((datbuf=MALLOC(datbuflen))==NULL) {
		close(file);
		printf("\7ERR_ALLOC %s %lu\n",str,datbuflen);
		FREE((char *)ixbbuf);
		if(misc&AUTO) fclose(out);
		continue; }
	if(lread(file,datbuf,datbuflen)!=datbuflen) {
		close(file);
		printf("\7ERR_READ %s %lu\n",str,datbuflen);
		FREE((char *)datbuf);
		FREE((char *)ixbbuf);
		if(misc&AUTO) fclose(out);
		continue; }
	close(file);
	m=0L;
	while(m<l && !ferror(out)) {
		for(j=0;j<12 && m<l;j++)
			if(j==8)
				str[j]='.';
			else
				str[j]=ixbbuf[m++]; /* Turns FILENAMEEXT into FILENAME.EXT */
		str[j]=0;
		unpadfname(str,fname);
		fprintf(out,"%-12.12s",misc&PAD ? str : fname);
		total_files++;
		n=ixbbuf[m]|((long)ixbbuf[m+1]<<8)|((long)ixbbuf[m+2]<<16);
		uld=(ixbbuf[m+3]|((long)ixbbuf[m+4]<<8)|((long)ixbbuf[m+5]<<16)
			|((long)ixbbuf[m+6]<<24));
		dld=(ixbbuf[m+7]|((long)ixbbuf[m+8]<<8)|((long)ixbbuf[m+9]<<16)
			|((long)ixbbuf[m+10]<<24));
		m+=11;

		if(n>=datbuflen 							/* index out of bounds */
			|| datbuf[n+F_DESC+LEN_FDESC]!=CR) {	/* corrupted data */
			fprintf(stderr,"\n\7%s%s is corrupted!\n"
				,dir[i]->data_dir,dir[i]->code);
			exit(-1); }


		if(misc&PLUS && datbuf[n+F_MISC]!=ETX
			&& (datbuf[n+F_MISC]-SP)&FM_EXTDESC)
			fputc('+',out);
		else
			fputc(SP,out);

		desc_off=12;
		if(misc&(CDT_|TOT)) {
			getrec((char *)&datbuf[n],F_CDT,LEN_FCDT,str);
			cdt=atol(str);
			total_cdt+=cdt;
			if(misc&CDT_) {
				fprintf(out,"%7lu",cdt);
				desc_off+=7; } }

		if(misc&MINUS) {
			sprintf(str,"%s%s",dir[i]->path,fname);
			if(!fexist(str))
				fputc('-',out);
			else
				fputc(SP,out); }
		else
			fputc(SP,out);
		desc_off++;

		if(misc&DFD) {
			sprintf(str,"%s%s",dir[i]->path,fname);
			fprintf(out,"%s ",unixtodstr(fdate(str),str));
			desc_off+=9; }

		if(misc&ULD) {
			fprintf(out,"%s ",unixtodstr(uld,str));
            desc_off+=9; }

		if(misc&ULN) {
			getrec((char *)&datbuf[n],F_ULER,25,str);
			fprintf(out,"%-25s ",str);
			desc_off+=26; }

		if(misc&DLD) {
			fprintf(out,"%s ",unixtodstr(dld,str));
			desc_off+=9; }

		if(misc&DLS) {
			getrec((char *)&datbuf[n],F_TIMESDLED,5,str);
			j=atoi(str);
			fprintf(out,"%5u ",j);
			desc_off+=6; }

		if(datbuf[n+F_MISC]!=ETX && (datbuf[n+F_MISC]-SP)&FM_EXTDESC)
			ext=1;	/* extended description exists */
		else
			ext=0;	/* it doesn't */

		if(!(misc&NOD) && !(misc&NOE && ext)) {
			getrec((char *)&datbuf[n],F_DESC,LEN_FDESC,str);
			fprintf(out,"%s",str); }

		if(misc&EXT && ext) {							/* Print ext desc */

			sprintf(str,"%s%s.EXB",dir[i]->data_dir,dir[i]->code);
			if(!fexist(str))
				continue;
			if((j=nopen(str,O_RDONLY))==-1) {
				printf("\7ERR_OPEN %s %lu\n",str,O_RDONLY);
				continue; }
			if((in=fdopen(j,"rb"))==NULL) {
				close(j);
				continue; }
			fseek(in,(n/F_LEN)*512L,SEEK_SET);
			lines=0;
			if(!(misc&NOE)) {
				fprintf(out,"\r\n");
				lines++; }
			while(!feof(in) && !ferror(in)
				&& ftell(in)<((n/F_LEN)+1)*512L) {
				if(!fgets(str,128,in) || !str[0])
					break;
				stripctrlz(str);
				if(lines) {
					if(misc&JST)
						fprintf(out,"%*s",desc_off,"");
					fputc(SP,out);				/* indent one character */ }
				fprintf(out,"%s",str);
				lines++; }
			fclose(in); }
		fprintf(out,"\r\n"); }
	FREE((char *)datbuf);
	FREE((char *)ixbbuf);
	fprintf(out,"\r\n"); /* blank line at end of dir */
	if(misc&AUTO) fclose(out); }

if(misc&TOT && !(misc&AUTO))
	fprintf(out,"TOTALS\n------\n%lu credits/bytes in %lu files.\r\n"
		,total_cdt,total_files);
printf("\nDone.\n");
return(0);
}
