#line 1 "UTI.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* Shared routines for most of the UTI driver programs */

#include "sbbs.h"
#include "uti.h"

#define bputs	lputs
#define bprintf lprintf

int logfile;
char scrnbuf[4000],tmp[256];
struct text_info txtinfo;

/****************************************************************************/
/* Performs printf() through local assembly routines                        */
/* Called from everywhere                                                   */
/****************************************************************************/
int lprintf(char *fmat, ...) {
	char sbuf[256];
	int chcount;

chcount=vsprintf(sbuf,fmat,_va_ptr);
lputs(sbuf);
return(chcount);
}

long lputs(char far *str)
{
	char tmp[256];
	int i,j,k;

j=strlen(str);
for(i=k=0;i<j;i++)		/* remove CRs */
	if(str[i]==CR && str[i+1]==LF)
		continue;
	else
		tmp[k++]=str[i];
tmp[k]=0;
return(fputs(tmp,stdout));
}

/****************************************************************************/
/* Truncates white-space chars off end of 'str' and terminates at first tab */
/****************************************************************************/
void truncsp(char *str)
{
	char c;

str[strcspn(str,"\t")]=0;
c=strlen(str);
while(c && str[c-1]<=SP) c--;
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
/* Network open function. Opens all files DENYALL and retries LOOP_NOPEN    */
/* number of times if the attempted file is already open or denying access  */
/* for some other reason.	All files are opened in BINARY mode.			*/
/****************************************************************************/
int nopen(char *str, int access)
{
	char logstr[256];
	int file,share,count=0;

if(access==O_RDONLY) share=O_DENYWRITE;
	else share=O_DENYALL;
while(((file=open(str,O_BINARY|share|access,S_IWRITE))==-1)
	&& errno==EACCES && count++<LOOP_NOPEN);
if(count>(LOOP_NOPEN/2))
	lprintf("NOPEN COLLISION - File: %s Count: %d"
		,str,count);
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

if(((*file)=nopen(str,access))==-1)
	return(NULL);

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
	close(*file);
	return(NULL); }
setvbuf(stream,NULL,_IOFBF,16*1024);
return(stream);
}


/****************************************************************************/
/* Returns the length of the file in 'filespec'                             */
/****************************************************************************/
long flength(char *filespec)
{
    struct ffblk f;

if(findfirst(filespec,&f,0)==NULL)
    return(f.ff_fsize);
return(-1L);
}


/****************************************************************************/
/* Error handling routine. Prints to local and remote screens the error     */
/* information, function, action, object and access and then attempts to    */
/* write the error information into the file ERROR.LOG and NODE.LOG         */
/****************************************************************************/
void errormsg(int line, char *source, char action, char *object, ulong access)
{
    char str[512];
    char actstr[256];

switch(action) {
    case ERR_OPEN:
        strcpy(actstr,"opening");
        break;
    case ERR_CLOSE:
        strcpy(actstr,"closeing");
        break;
    case ERR_FDOPEN:
        strcpy(actstr,"fdopen");
        break;
    case ERR_READ:
        strcpy(actstr,"reading");
        break;
    case ERR_WRITE:
        strcpy(actstr,"writing");
        break;
    case ERR_REMOVE:
        strcpy(actstr,"removing");
        break;
    case ERR_ALLOC:
        strcpy(actstr,"allocating memory");
        break;
    case ERR_CHK:
        strcpy(actstr,"checking");
        break;
    case ERR_LEN:
        strcpy(actstr,"checking length");
        break;
    case ERR_EXEC:
        strcpy(actstr,"executing");
        break;
    default:
        strcpy(actstr,"UNKNOWN"); }
lprintf("\7\r\nERROR -     file: %s",source);
lprintf("\7\r\n            line: %u",line);
lprintf("\7\r\n          action: %s",actstr);   /* tell user about error */
lprintf("\7\r\n          object: %s",object);
lprintf("\7\r\n          access: %lu (%lxh)",access,access);
lputs("\r\n\r\n<Hit any key>");
getch();
lputs("\r\n");
}


void allocfail(uint size)
{
lprintf("\7Error allocating %u bytes of memory.\r\n",size);
exit(1);
}

void bail(int code)
{
	char str[256];
	time_t t;
	struct time curtime;
	struct date date;

if(!code) {
	puttext(1,1,80,25,scrnbuf); /* restore screen if no error */
	textattr(txtinfo.attribute);
	gotoxy(txtinfo.curx,txtinfo.cury); }
t=time(NULL);
unixtodos(t,&date,&curtime);
sprintf(str,"%02u/%02u/%u %02u:%02u:%02u    Exiting (%d)\r\n\r\n"
	,date.da_mon,date.da_day,date.da_year
	,curtime.ti_hour,curtime.ti_min,curtime.ti_sec
	,code);
write(logfile,str,strlen(str));
exit(code);
}

int getsubnum(char *code)
{
	int i;

for(i=0;i<total_subs;i++) {
//	  printf("%s vs %s\n",code,sub[i]->code);
	if(!stricmp(code,sub[i]->code))
		return(i); }
return(-1);
}

void uti_init(char *name, int argc, char **argv)
{
	char str[256],*p;
	int i;
	read_cfg_text_t txt;
	time_t t;
	struct tm *tm;

setvbuf(stdout,NULL,_IONBF,0);
putenv("TZ=UTC0");

txt.openerr="\7\r\nError opening %s for read.\r\n";
txt.reading="\r\nReading %s...";
txt.readit="\rRead %s       ";
txt.allocerr="\7\r\nError allocating %u bytes of memory\r\n";
txt.error="\7\r\nERROR: Offset %lu in %s\r\n\r\n";

p=getenv("SBBSNODE");
if(p==NULL) {
	printf("\7\nSBBSNODE environment variable not set.\n");
	exit(1); }
strcpy(node_dir,p);

strupr(node_dir);

if(node_dir[strlen(node_dir)-1]!='\\')
	strcat(node_dir,"\\");

read_node_cfg(txt);
if(ctrl_dir[0]=='.') {   /* Relative path */
	strcpy(str,ctrl_dir);
	sprintf(ctrl_dir,"%s%s",node_dir,str); }
read_main_cfg(txt);
if(data_dir[0]=='.') {   /* Relative path */
	strcpy(str,data_dir);
	sprintf(data_dir,"%s%s",node_dir,str); }
read_msgs_cfg(txt);
sprintf(str,"%sUTI.LOG",data_dir);
if((logfile=nopen(str,O_WRONLY|O_CREAT|O_APPEND))==-1) {
	printf("\7\nCan't open %s\n",str);
	exit(-1); }
t=time(NULL);
tm=gmtime(&t);
sprintf(str,"%02u/%02u/%02u %02u:%02u:%02u    %-8s %s \""
	,tm->tm_mon+1,tm->tm_mday,TM_YEAR(tm->tm_year)
	,tm->tm_hour,tm->tm_min,tm->tm_sec
	,name,VER);
printf("\n\n");
for(i=1;i<argc;i++) {
	if(i>1)
		strcat(str," ");
	strcat(str,argv[i]);
	printf("%s ",argv[i]); }
strcat(str,"\"");
write(logfile,str,strlen(str));
write(logfile,"\r\n",2);
printf("\n\nWorking...");
}
