/* SMBACTIV.C */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include <share.h>
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
void bail(int code)
{
exit(code);
}

void main(int argc, char **argv)
{
	char str[256],*p;
	int i,j,file;
	ulong l,length,max_users=0xffffffff;
	sub_status_t *sub_status;
	read_cfg_text_t txt;
	struct find_t f;
	FILE *stream;

   _fmode=O_BINARY;
	txt.openerr="\7\r\nError opening %s for read.\r\n";
	txt.reading="\r\nReading %s...";
	txt.readit="\rRead %s       ";
	txt.allocerr="\7\r\nError allocating %u bytes of memory\r\n";
	txt.error="\7\r\nERROR: Offset %lu in %s\r\n\r\n";

fprintf(stderr,"\nSMBACTIV Version %s (%s) - Synchronet Message Base Activity "
	"Monitor\n"
	,SMBACTIV_VER
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

	if(argc>1 && (!stricmp(argv[1],"/?") || !stricmp(argv[1],"?"))) {
		lprintf("\nusage: SMBACTIV [max_users]\n\n");
		lprintf("max_users = limit output to subs read by this many users "
			"or less\n");
		exit(0); }

	if(argc>1)
		max_users=atol(argv[1]);

	if(!node_dir[0]) {
		p=getenv("SBBSNODE");
		if(p==NULL) {
			printf("\7\nSBBSNODE environment variable not set.\n");
			exit(1); }
		strcpy(node_dir,p); }

	strupr(node_dir);

	if(node_dir[strlen(node_dir)-1]!='\\')
		strcat(node_dir,"\\");

	read_node_cfg(txt);
	if(ctrl_dir[0]=='.') {   /* Relative path */
		strcpy(str,ctrl_dir);
		sprintf(ctrl_dir,"%s%s",node_dir,str);
		if(_fullpath(str,ctrl_dir,40))
			strcpy(ctrl_dir,str); }
	backslash(ctrl_dir);

	read_main_cfg(txt);
	if(data_dir[0]=='.') {   /* Relative path */
		strcpy(str,data_dir);
		sprintf(data_dir,"%s%s",node_dir,str);
		if(_fullpath(str,data_dir,40))
			strcpy(data_dir,str); }
	backslash(data_dir);
	if(text_dir[0]=='.') {   /* Relative path */
		strcpy(str,text_dir);
		sprintf(text_dir,"%s%s",node_dir,str);
		if(_fullpath(str,text_dir,40))
			strcpy(text_dir,str); }
	backslash(text_dir);
	read_msgs_cfg(txt);

	if((sub_status=(sub_status_t *)MALLOC
		(total_subs*sizeof(sub_status_t)))==NULL) {
		printf("ERROR Allocating memory for sub_status\r\n");
		exit(1); }

	lprintf("\nReading sub-board ");
	for(i=0;i<total_subs;i++) {
		lprintf("%5d of %-5d\b\b\b\b\b\b\b\b\b\b\b\b\b\b",i+1,total_subs);
		sprintf(smb.file,"%s%s",sub[i]->data_dir,sub[i]->code);
		if((j=smb_open(&smb))!=0) {
			lprintf("Error %d opening %s\r\n",j,smb.file);
			sub_status[i].read=0;
			sub_status[i].firstmsg=0L;
            continue; }
		sub_status[i].read=0;
		sub_status[i].firstmsg=first_msg();
		smb_close(&smb); }

	sprintf(str,"%sUSER\\PTRS\\*.IXB",data_dir);
	if(_dos_findfirst(str,0,&f)) {
		lprintf("Unable to find any user pointer files.\n");
		FREE(sub_status);
		exit(1); }

	j=0;
	lprintf("\nComparing user pointers ");
	while(1) {
		lprintf("%-5d\b\b\b\b\b",++j);
		sprintf(str,"%sUSER\\PTRS\\%s",data_dir,f.name);
		if((file=nopen(str,O_RDONLY))==-1) {
			if(_dos_findnext(&f))
				break;
			continue; }
		length=filelength(file);
		for(i=0;i<total_subs;i++) {
			if(sub_status[i].read>max_users)
				continue;
			if(length<(sub[i]->ptridx+1)*10L)
				continue;
			else {
				lseek(file,((long)sub[i]->ptridx*10L)+4L,SEEK_SET);
				read(file,&l,4); }
			if(l>sub_status[i].firstmsg)
				sub_status[i].read++; }
		close(file);
		if(_dos_findnext(&f))
			break; }

	printf("NumUsers    Sub-board\n");
	printf("--------    -------------------------------------------------"
		"-----------\n");
	for(i=0;i<total_subs;i++) {
		if(sub_status[i].read>max_users)
			continue;
		printf("%8lu    %-*s %-*s\n"
			,sub_status[i].read
			,LEN_GSNAME,grp[sub[i]->grp]->sname
			,LEN_SLNAME,sub[i]->lname); }

}
