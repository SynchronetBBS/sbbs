#line 1 "VIEWFILE.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"

void viewfilecontents(file_t f)
{
	char str[128],cmd[128];
	int i;

if(f.size<=0L) {
	bputs(text[FileNotThere]);
	return; }

sprintf(str,"%s%s",f.altpath > 0 && f.altpath<=altpaths
	? altpath[f.altpath-1] : dir[f.dir]->path
	,unpadfname(f.name,tmp));
strcpy(tmp,f.name);
truncsp(tmp);
for(i=0;i<total_fviews;i++) {
	if(!stricmp(tmp+9,fview[i]->ext)
		&& chk_ar(fview[i]->ar,useron)) {
		strcpy(cmd,fview[i]->cmd);
		break; } }
if(i==total_fviews)
	bprintf(text[NonviewableFile],tmp+9);
else
	if((i=external(cmdstr(cmd,str,str,NULL)
		,EX_OUTL|EX_OUTR|EX_INR|EX_CC))!=0)
		errormsg(WHERE,ERR_EXEC,cmdstr(cmd,str,str,NULL),i);
}

/****************************************************************************/
/* Views file with:                                                         */
/* (B)atch download, (V)iew file (E)xtended info, (Q)uit, or [Next]:        */
/* call with ext=1 for default to extended info, or 0 for file view         */
/* Returns -1 for Batch, 1 for Next, or 0 for Quit                          */
/****************************************************************************/
int viewfile(file_t f, int ext)
{
	char ch,str[256];
    int i;

curdirnum=f.dir;	/* for ARS */
while(online) {
    if(ext)
        fileinfo(f);
	else
		viewfilecontents(f);
    ASYNC;
    CRLF;
    sprintf(str,text[FileInfoPrompt],unpadfname(f.name,tmp));
    mnemonics(str);
    ch=getkeys("BEVQ\r",0);
    if(ch=='Q' || sys_status&SS_ABORT)
        return(0);
    switch(ch) {
        case 'B':
            addtobatdl(f);
            CRLF;
            return(-1);
        case 'E':
            ext=1;
            continue;
        case 'V':
            ext=0;
            continue;
        case CR:
            return(1); } }
return(0);
}

