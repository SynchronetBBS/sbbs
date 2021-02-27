/* DUPEFIND.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"
#include "crc32.h"

#define DUPEFIND_VER "1.01"

void bail(int code)
{
exit(code);
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
return(fputs(tmp,stderr));
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
char *display_filename(ushort dir_num,ushort fil_off)
{
	static char str[256];
	char fname[13];
    int file;

    sprintf(str,"%s%s.IXB",dir[dir_num]->data_dir,dir[dir_num]->code);
    if((file=nopen(str,O_RDONLY))==-1)
        return("UNKNOWN");
	lseek(file,(long)(22*(fil_off-1)),SEEK_SET);
    read(file,fname,11);
    close(file);

    sprintf(str,"%-8.8s.%c%c%c",fname,fname[8],fname[9],fname[10]);
    return(str);
}

void main(int argc,char **argv)
{
	char str[256],*ixbbuf,*p;
	ulong **fcrc,*foundcrc,total_found=0L;
	ushort i,j,k,h,g,start_lib=0,end_lib=0,found=-1;
	int file;
    long l,m;
    read_cfg_text_t txt;

putenv("TZ=UCT0");
_fmode=O_BINARY;
setvbuf(stdout,NULL,_IONBF,0);

fprintf(stderr,"\nDUPEFIND Version %s (%s) - Synchronet Duplicate File "
	"Finder\n"
	,DUPEFIND_VER
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

    p=getenv("SBBSNODE");
    if(p==NULL) {
		fprintf(stderr,"\nSBBSNODE environment variable must be set.\n");
		fprintf(stderr,"\nExample: SET SBBSNODE=C:\\SBBS\\NODE1\n");
        exit(1); }

	if(!stricmp(argv[1],"/?") || !stricmp(argv[1],"?")) {
		fprintf(stderr,"\n");
		fprintf(stderr,"usage: DUPEFIND [start] [end]\n");
		fprintf(stderr,"where: [start] is the starting library number to check\n");
		fprintf(stderr,"       [end]   is the final library number to check\n");
		return; }


    strcpy(node_dir,p);
    if(node_dir[strlen(node_dir)-1]!='\\')
        strcat(node_dir,"\\");

    txt.openerr="\7\nError opening %s for read.\n";
    txt.reading="\nReading %s...";
    txt.readit="\rRead %s       ";
    txt.allocerr="\7\nError allocating %u bytes of memory\n";
    txt.error="\7\nERROR: Offset %lu in %s\r\n\n";

    read_node_cfg(txt);
    if(ctrl_dir[0]=='.') {
        strcpy(str,ctrl_dir);
		sprintf(ctrl_dir,"%s%s",node_dir,str);
		if(_fullpath(str,ctrl_dir,40))
            strcpy(ctrl_dir,str); }
	backslash(ctrl_dir);
    read_main_cfg(txt);
    if(data_dir[0]=='.') {
        strcpy(str,data_dir);
		sprintf(data_dir,"%s%s",node_dir,str);
		if(_fullpath(str,data_dir,40))
			strcpy(data_dir,str); }
    backslash(data_dir);
    read_file_cfg(txt);
	lputs("\n");

	start_lib=0;
	end_lib=total_libs-1;
	if(argc>1)
		start_lib=end_lib=atoi(argv[1])-1;
	if(argc>2)
        end_lib=atoi(argv[2])-1;

	if((fcrc=(ulong **)MALLOC(total_dirs*sizeof(ulong *)))==NULL) {
		printf("Not enough memory for CRCs.\r\n");
		exit(1); }

	for(i=0;i<total_dirs;i++) {
		fprintf(stderr,"Reading directory index %u of %u\r",i+1,total_dirs);
        sprintf(str,"%s%s.IXB",dir[i]->data_dir,dir[i]->code);
		if((file=nopen(str,O_RDONLY))==-1) {
			fcrc[i]=(ulong *)MALLOC(1*sizeof(ulong));
            fcrc[i][0]=0;
			continue; }
        l=filelength(file);
		if(!l || (dir[i]->lib<start_lib || dir[i]->lib>end_lib)) {
            close(file);
			fcrc[i]=(ulong *)MALLOC(1*sizeof(ulong));
			fcrc[i][0]=0;
            continue; }
		if((fcrc[i]=(ulong *)MALLOC((l/22+2)*sizeof(ulong)))==NULL) {
            printf("Not enough memory for CRCs.\r\n");
            exit(1); }
		fcrc[i][0]=(ulong)(l/22);
        if((ixbbuf=(char *)MALLOC(l))==NULL) {
            close(file);
            printf("\7Error allocating memory for index %s.\r\n",str);
            continue; }
        if(read(file,ixbbuf,l)!=l) {
            close(file);
            printf("\7Error reading %s.\r\n",str);
			FREE(ixbbuf);
            continue; }
        close(file);
		j=1;
		m=0L;
		while(m<l) {
			sprintf(str,"%-11.11s",(ixbbuf+m));
			strupr(str);
			fcrc[i][j++]=crc32(str);
			m+=22; }
		FREE(ixbbuf); }
	lputs("\n");

	foundcrc=0L;
	for(i=0;i<total_dirs;i++) {
		if(dir[i]->lib<start_lib || dir[i]->lib>end_lib)
			continue;
		lprintf("Scanning %s %s\n",lib[dir[i]->lib]->sname,dir[i]->sname);
		for(k=1;k<fcrc[i][0];k++) {
			for(j=i+1;j<total_dirs;j++) {
				if(dir[j]->lib<start_lib || dir[j]->lib>end_lib)
					continue;
				for(h=1;h<fcrc[j][0];h++) {
					if(fcrc[i][k]==fcrc[j][h]) {
						if(found!=k) {
							found=k;
							for(g=0;g<total_found;g++) {
								if(foundcrc[g]==fcrc[i][k])
									g=total_found+1; }
							if(g==total_found) {
								++total_found;
								if((foundcrc=(ulong *)REALLOC(foundcrc
									,total_found*sizeof(ulong)))==NULL) {
								printf("Out of memory reallocating\r\n");
								exit(1); } }
							else
								found=0;
							printf("\n%-12s is located in : %-*s  %s\n"
								   "%-12s           and : %-*s  %s\n"
								,display_filename(i,k)
								,LEN_GSNAME
								,lib[dir[i]->lib]->sname
								,dir[i]->sname
								,""
								,LEN_GSNAME
								,lib[dir[j]->lib]->sname
								,dir[j]->sname
								); }
						else
							printf("%-12s           and : %-*s  %s\n"
								,""
								,LEN_GSNAME
								,lib[dir[j]->lib]->sname
								,dir[j]->sname
								); } } } } }
}

