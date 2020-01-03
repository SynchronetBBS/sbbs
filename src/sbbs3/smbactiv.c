/* SMBACTIV.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"

#define SMBACTIV_VER "1.01"

typedef struct {
	ulong read;
	ulong firstmsg;
} sub_status_t;

smb_t smb;

ulong first_msg()
{
	smbmsg_t msg;

	msg.offset=0;
	msg.hdr.number=0;
	if(smb_getmsgidx(&smb,&msg))			/* Get first message index */
		return(0);
	return(msg.idx.number);
}

long lputs(char *str)
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
int lprintf(const char *fmat, ...)
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

void bail(int code)
{
	exit(code);
}

int main(int argc, char **argv)
{
	char str[256],*p;
	int i,j,file;
	ulong length,max_users=0xffffffff;
	uint32_t l;
	sub_status_t *sub_status;
	scfg_t	cfg;
	glob_t	gl;
	size_t	glp;

	fprintf(stderr,"\nSMBACTIV Version %s (%s) - Synchronet Message Base Activity "
		"Monitor\n", SMBACTIV_VER, PLATFORM_DESC);

	if(argc>1)
		max_users=atol(argv[1]);

	if(!max_users) {
		lprintf("\nusage: SMBACTIV [max_users]\n\n");
		lprintf("max_users = limit output to subs read by this many users "
			"or less\n");
		return(0); }

	p = get_ctrl_dir();

	memset(&cfg,0,sizeof(cfg));
	cfg.size=sizeof(cfg);
	SAFECOPY(cfg.ctrl_dir,p);

	if(!load_cfg(&cfg,NULL,TRUE,str)) {
		fprintf(stderr,"!ERROR loading configuration files: %s\n",str);
		return(1);
	}

	chdir(cfg.ctrl_dir);

	if((sub_status=(sub_status_t *)MALLOC
		(cfg.total_subs*sizeof(sub_status_t)))==NULL) {
		printf("ERROR Allocating memory for sub_status\r\n");
		return(1); }

	lprintf("\nReading sub-board ");
	for(i=0;i<cfg.total_subs;i++) {
		lprintf("%5d of %-5d\b\b\b\b\b\b\b\b\b\b\b\b\b\b",i+1,cfg.total_subs);
		sprintf(smb.file,"%s%s",cfg.sub[i]->data_dir,cfg.sub[i]->code);
		if((j=smb_open(&smb))!=0) {
			lprintf("Error %d opening %s\r\n",j,smb.file);
			sub_status[i].read=0;
			sub_status[i].firstmsg=0L;
            continue; }
		sub_status[i].read=0;
		sub_status[i].firstmsg=first_msg();
		smb_close(&smb); }

	sprintf(str,"%suser/ptrs/*.ixb",cfg.data_dir);
	if(glob(str, GLOB_MARK, NULL, &gl)) {
		lprintf("Unable to find any user pointer files.\n");
		globfree(&gl);
		free(sub_status);
		return(1); }

	lprintf("\nComparing user pointers ");
	for(glp=0; glp<gl.gl_pathc; glp++) {
		lprintf("%-5d\b\b\b\b\b",glp);
		SAFECOPY(str,gl.gl_pathv[glp]);
		if((file=nopen(str,O_RDONLY|O_BINARY))==-1) {
			continue; }
		length=filelength(file);
		for(i=0;i<cfg.total_subs;i++) {
			if(sub_status[i].read>max_users)
				continue;
			if(length<(cfg.sub[i]->ptridx+1)*10UL)
				continue;
			else {
				lseek(file,((long)cfg.sub[i]->ptridx*10L)+4L,SEEK_SET);
				read(file,&l,4); }
			if(l>sub_status[i].firstmsg)
				sub_status[i].read++; }
		close(file); }
	globfree(&gl);

	printf("NumUsers    Sub-board\n");
	printf("--------    -------------------------------------------------"
		"-----------\n");
	for(i=0;i<cfg.total_subs;i++) {
		if(sub_status[i].read>max_users)
			continue;
		printf("%8lu    %-*s %-*s\n"
			,sub_status[i].read
			,LEN_GSNAME,cfg.grp[cfg.sub[i]->grp]->sname
			,LEN_SLNAME,cfg.sub[i]->lname); }

	return(0);
}
