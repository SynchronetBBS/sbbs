/* DUPEFIND.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"
#include "crc32.h"

#define DUPEFIND_VER "1.01"

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
return(fputs(tmp,stderr));
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

char *display_filename(scfg_t *cfg, ushort dir_num,ushort fil_off)
{
	static char str[256];
	char fname[13];
	int file;

	sprintf(str,"%s%s.IXB",cfg->dir[dir_num]->data_dir,cfg->dir[dir_num]->code);
	if((file=nopen(str,O_RDONLY))==-1)
		return("UNKNOWN");
	lseek(file,(long)(22*(fil_off-1)),SEEK_SET);
	read(file,fname,11);
	close(file);

	sprintf(str,"%-8.8s.%c%c%c",fname,fname[8],fname[9],fname[10]);
	return(str);
}

int main(int argc,char **argv)
{
	char str[256],*ixbbuf,*p;
	ulong **fcrc,*foundcrc,total_found=0L;
	ushort i,j,k,h,g,start_lib=0,end_lib=0,found=-1;
	int file;
    long l,m;
	scfg_t cfg;

putenv("TZ=UCT0");
setvbuf(stdout,NULL,_IONBF,0);

fprintf(stderr,"\nDUPEFIND Version %s (%s) - Synchronet Duplicate File "
	"Finder\n", DUPEFIND_VER, PLATFORM_DESC);

    p=getenv("SBBSCTRL");
    if(p==NULL) {
		fprintf(stderr,"\nSBBSCTRL environment variable must be set.\n");
#ifdef __unix__
		fprintf(stderr,"\nExample: export SBBSCTRL=/sbbs/ctrl\n");
#else
		fprintf(stderr,"\nExample: SET SBBSCTRL=C:\\SBBS\\CTRL\n");
#endif
        return(1); }

	if(argc>1 && (!stricmp(argv[1],"/?") || !stricmp(argv[1],"?") || !stricmp(argv[1],"-?"))) {
		fprintf(stderr,"\n");
		fprintf(stderr,"usage: DUPEFIND [start] [end]\n");
		fprintf(stderr,"where: [start] is the starting library number to check\n");
		fprintf(stderr,"       [end]   is the final library number to check\n");
		return(0); }

	memset(&cfg,0,sizeof(cfg));
	cfg.size=sizeof(cfg);
	SAFECOPY(cfg.ctrl_dir,p);

	if(!load_cfg(&cfg,NULL,TRUE,str)) {
		fprintf(stderr,"!ERROR loading configuration files: %s\n",str);
		return(1);
	}

	chdir(cfg.ctrl_dir);

	lputs("\n");

	start_lib=0;
	end_lib=cfg.total_libs-1;
	if(argc>1)
		start_lib=end_lib=atoi(argv[1])-1;
	if(argc>2)
        end_lib=atoi(argv[2])-1;

	if((fcrc=(ulong **)MALLOC(cfg.total_dirs*sizeof(ulong *)))==NULL) {
		printf("Not enough memory for CRCs.\r\n");
		return(1); }

	for(i=0;i<cfg.total_dirs;i++) {
		fprintf(stderr,"Reading directory index %u of %u\r",i+1,cfg.total_dirs);
        sprintf(str,"%s%s.ixb",cfg.dir[i]->data_dir,cfg.dir[i]->code);
		if((file=nopen(str,O_RDONLY|O_BINARY))==-1) {
			fcrc[i]=(ulong *)MALLOC(1*sizeof(ulong));
            fcrc[i][0]=0;
			continue; }
        l=filelength(file);
		if(!l || (cfg.dir[i]->lib<start_lib || cfg.dir[i]->lib>end_lib)) {
            close(file);
			fcrc[i]=(ulong *)MALLOC(1*sizeof(ulong));
			fcrc[i][0]=0;
            continue; }
		if((fcrc[i]=(ulong *)MALLOC((l/22+2)*sizeof(ulong)))==NULL) {
            printf("Not enough memory for CRCs.\r\n");
            return(1); }
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
			fcrc[i][j++]=crc32(str,0);
			m+=22; }
		FREE(ixbbuf); }
	lputs("\n");

	foundcrc=0L;
	for(i=0;i<cfg.total_dirs;i++) {
		if(cfg.dir[i]->lib<start_lib || cfg.dir[i]->lib>end_lib)
			continue;
		lprintf("Scanning %s %s\n",cfg.lib[cfg.dir[i]->lib]->sname,cfg.dir[i]->sname);
		for(k=1;k<fcrc[i][0];k++) {
			for(j=i+1;j<cfg.total_dirs;j++) {
				if(cfg.dir[j]->lib<start_lib || cfg.dir[j]->lib>end_lib)
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
								return(1); } }
							else
								found=0;
							printf("\n%-12s is located in : %-*s  %s\n"
								   "%-12s           and : %-*s  %s\n"
								,display_filename(&cfg,i,k)
								,LEN_GSNAME
								,cfg.lib[cfg.dir[i]->lib]->sname
								,cfg.dir[i]->sname
								,""
								,LEN_GSNAME
								,cfg.lib[cfg.dir[j]->lib]->sname
								,cfg.dir[j]->sname
								); }
						else
							printf("%-12s           and : %-*s  %s\n"
								,""
								,LEN_GSNAME
								,cfg.lib[cfg.dir[j]->lib]->sname
								,cfg.dir[j]->sname
								); } } } } }
	return(0);
}

