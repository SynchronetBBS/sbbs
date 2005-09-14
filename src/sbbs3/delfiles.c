#line 1 "delfiles.c"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"

#define DELFILES_VER "1.01"

char tmp[256];
char *crlf="\r\n";

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

int main(int argc, char **argv)
{
	char str[256],fname[MAX_PATH+1],not[MAX_NOTS][9],nots=0,*p;
	int i,j,dirnum,libnum,file;
	ulong l,m,n,length;
	long misc=0;
	time_t now;
	file_t workfile;
	scfg_t cfg;
	glob_t gl;
	uchar HUGE16 *ixbbuf;

putenv("TZ=UCT0");
setvbuf(stdout,NULL,_IONBF,0);

fprintf(stderr,"\nDELFILES Version %s (%s) - Removes files from Synchronet "
	"Filebase\n" ,DELFILES_VER, PLATFORM_DESC );

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
	return(0); }

p=getenv("SBBSCTRL");
if(p==NULL) {
	printf("\nSBBSCTRL environment variable not set.\n");
#ifdef __unix__
	printf("\nExample: export SBBSCTRL=/sbbs/ctrl\n");
#else
	printf("\nExample: SET SBBSCTRL=C:\\SBBS\\CTRL\n");
#endif
	return(1); }

putenv("TZ=UCT0");

memset(&cfg, 0, sizeof(cfg));
cfg.size=sizeof(cfg);
SAFECOPY(cfg.ctrl_dir, p);
backslash(cfg.ctrl_dir);

load_cfg(&cfg, NULL, TRUE, str);
chdir(cfg.ctrl_dir);

dirnum=libnum=-1;
if(argv[1][0]=='*')
	misc|=ALL;
else if(argv[1][0]!='/') {
	strupr(argv[1]);
	for(i=0;i<cfg.total_dirs;i++)
		if(!stricmp(argv[1],cfg.dir[i]->code))
			break;
	if(i>=cfg.total_dirs) {
		printf("\nDirectory code '%s' not found.\n",argv[1]);
		return(1); }
	dirnum=i; }
for(i=1;i<argc;i++) {
	if(!stricmp(argv[i],"/LIB")) {
		if(dirnum!=-1) {
			printf("\nBoth directory code and /LIB parameters were used.\n");
			return(1); }
		i++;
		if(i>=argc) {
			printf("\nLibrary short name must follow /LIB parameter.\n");
			return(1); }
		strupr(argv[i]);
		for(j=0;j<cfg.total_libs;j++)
			if(!stricmp(cfg.lib[j]->sname,argv[i]))
				break;
		if(j>=cfg.total_libs) {
			printf("\nLibrary short name '%s' not found.\n",argv[i]);
			return(1); }
		libnum=j; }
	else if(!stricmp(argv[i],"/NOT")) {
		if(nots>=MAX_NOTS) {
			printf("\nMaximum number of /NOT options (%u) exceeded.\n"
				,MAX_NOTS);
			return(1); }
		i++;
		if(i>=argc) {
			printf("\nDirectory internal code must follow /NOT parameter.\n");
            return(1); }
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
            return(1); }
		if(libnum!=-1) {
			printf("\nBoth library name and /ALL parameters were used.\n");
			return(1); }
		misc|=ALL; } }

for(i=0;i<cfg.total_dirs;i++) {
	if(!(misc&ALL) && i!=dirnum && cfg.dir[i]->lib!=libnum)
		continue;
	for(j=0;j<nots;j++)
		if(!stricmp(not[j],cfg.dir[i]->code))
			break;
	if(j<nots)
        continue;

	if(misc&NO_LINK && cfg.dir[i]->misc&DIR_FCHK) {
		strcpy(tmp,cfg.dir[i]->path);
		sprintf(str,"%s*.*",tmp);
		printf("\nSearching %s for unlinked files\n",str);
		if(!glob(str, GLOB_MARK, NULL, &gl)) {
			for(j=0; j<gl.gl_pathc; j++) {
				/* emulate _A_NORMAL */
				if(isdir(gl.gl_pathv[j]))
					continue;
				if(access(gl.gl_pathv[j], R_OK|W_OK))
					continue;
				padfname(gl.gl_pathv[j],str);
				/* strupr(str); */
				if(!findfile(&cfg, i,str)) {
					sprintf(str,"%s%s",tmp,gl.gl_pathv[j]);
					printf("Removing %s (not in database)\n",gl.gl_pathv);
					if(!(misc&REPORT) && remove(str))
						printf("Error removing %s\n",str); } } }
		globfree(&gl);
	}

	if(!cfg.dir[i]->maxage && !(misc&OFFLINE))
		continue;

	printf("\nScanning %s %s\n",cfg.lib[cfg.dir[i]->lib]->sname,cfg.dir[i]->lname);

	sprintf(str,"%s%s.ixb",cfg.dir[i]->data_dir,cfg.dir[i]->code);
	if((file=nopen(str,O_RDONLY|O_BINARY))==-1)
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
			,workfile.altpath>0 && workfile.altpath<=cfg.altpaths
				? cfg.altpath[workfile.altpath-1]
			: cfg.dir[workfile.dir]->path,fname);
		workfile.datoffset=ixbbuf[m]|((long)ixbbuf[m+1]<<8)
			|((long)ixbbuf[m+2]<<16);
		workfile.dateuled=(ixbbuf[m+3]|((long)ixbbuf[m+4]<<8)
			|((long)ixbbuf[m+5]<<16)|((long)ixbbuf[m+6]<<24));
		workfile.datedled=(ixbbuf[m+7]|((long)ixbbuf[m+8]<<8)
			|((long)ixbbuf[m+9]<<16)|((long)ixbbuf[m+10]<<24));
		m+=11;
		if(cfg.dir[i]->maxage && cfg.dir[i]->misc&DIR_SINCEDL && workfile.datedled
			&& (now-workfile.datedled)/86400L>cfg.dir[i]->maxage) {
				printf("Deleting %s (%ld days since last download)\n",fname
					,(now-workfile.datedled)/86400L);
				getfiledat(&cfg, &workfile);
				if(!(misc&REPORT)) {
					removefiledat(&cfg, &workfile);
					if(remove(str))
						printf("Error removing %s\n",str); } }
		else if(cfg.dir[i]->maxage
			&& !(workfile.datedled && cfg.dir[i]->misc&DIR_SINCEDL)
			&& (now-workfile.dateuled)/86400L>cfg.dir[i]->maxage) {
				printf("Deleting %s (uploaded %ld days ago)\n",fname
					,(now-workfile.dateuled)/86400L);
				getfiledat(&cfg, &workfile);
				if(!(misc&REPORT)) {
					removefiledat(&cfg, &workfile);
					if(remove(str))
						printf("Error removing %s\n",str); } }
		else if(misc&OFFLINE && cfg.dir[i]->misc&DIR_FCHK && !fexist(str)) {
				printf("Removing %s (doesn't exist)\n",fname);
				getfiledat(&cfg, &workfile);
				if(!(misc&REPORT))
					removefiledat(&cfg, &workfile); } }

	FREE((char *)ixbbuf); }
}
