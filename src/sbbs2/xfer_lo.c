#line 1 "XFER_LO.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"

/****************************************************************************/
/* Prompts user for file specification. <CR> is *.* and .* is assumed.      */
/* Returns padded file specification.                                       */
/* Returns NULL if input was aborted.                                       */
/****************************************************************************/
char *getfilespec(char *str)
{
bputs(text[FileSpecStarDotStar]);
if(!getstr(str,12,K_UPPER))
    strcpy(str,"*.*");
else if(!strchr(str,'.') && strlen(str)<=8)
    strcat(str,".*");
if(sys_status&SS_ABORT)
    return(0);
return(str);
}

/****************************************************************************/
/* Turns FILE.EXT into FILE    .EXT                                         */
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
/* Checks to see if filename matches filespec. Returns 1 if yes, 0 if no    */
/****************************************************************************/
char filematch(char *filename, char *filespec)
{
    char c;

for(c=0;c<8;c++) /* Handle Name */
    if(filespec[c]=='*') break;
    else if(filespec[c]=='?') continue;
    else if(filename[c]!=filespec[c]) return(0);
for(c=9;c<12;c++)
    if(filespec[c]=='*') break;
    else if(filespec[c]=='?') continue;
    else if(filename[c]!=filespec[c]) return(0);
return(1);
}

/****************************************************************************/
/* Deletes all files in dir 'path' that match file spec 'spec'              */
/****************************************************************************/
int delfiles(char *inpath, char *spec)
{
	char str[256],path[128],done;
    int files=0;
    struct ffblk ff;

strcpy(path,inpath);
backslash(path);
sprintf(str,"%s%s",path,spec);
done=findfirst(str,&ff,0);
while(!done) {
    sprintf(str,"%s%s",path,ff.ff_name);
	_chmod(str,1,FA_NORMAL);	// Incase it's been marked RDONLY
    if(remove(str))
        errormsg(WHERE,ERR_REMOVE,str,0);
    else
        files++;
    done=findnext(&ff); }
return(files);
}

/*****************************************************************************/
/* Checks the filename 'fname' for invalid symbol or character sequences     */
/*****************************************************************************/
char checkfname(char *fname)
{
	char str[256],c=0,d;

if(strcspn(fname,"\\/|<>+[]:=\";,%")!=strlen(fname)) {
	sprintf(str,"Suspicious filename attempt: '%s'",fname);
    errorlog(str);
	return(0); }
if(strstr(fname,".."))
	return(0);
if(strcspn(fname,".")>8)
	return(0);
d=strlen(fname);
while(c<d) {
	if(fname[c]<=SP || fname[c]&0x80)
		return(0);
	c++; }
return(1);
}

/**************************************************************************/
/* Add file 'f' to batch download queue. Return 1 if successful, 0 if not */
/**************************************************************************/
char addtobatdl(file_t f)
{
    char str[256],tmp2[256];
    uint i;
	ulong totalcdt, totalsize, totaltime;

if(useron.rest&FLAG('D')) {
	bputs(text[R_Download]);
	return(0); }
/***
sprintf(str,"%s%s",f.altpath>0 && f.altpath<=altpaths ? altpath[f.altpath-1]
    : dir[f.dir]->path,unpadfname(f.name,tmp));
***/
for(i=0;i<batdn_total;i++) {
    if(!strcmp(batdn_name[i],f.name) && f.dir==batdn_dir[i]) {
		bprintf(text[FileAlreadyInQueue],f.name);
        return(0); } }
if(f.size<=0 /* !fexist(str) */) {
	bprintf(text[CantAddToQueue],f.name);
    bputs(text[FileIsNotOnline]);
    return(0); }
if(batdn_total>=max_batdn) {
	bprintf(text[CantAddToQueue],f.name);
    bputs(text[BatchDlQueueIsFull]);
    return(0); }
for(i=0,totalcdt=0;i<batdn_total;i++)
    totalcdt+=batdn_cdt[i];
if(dir[f.dir]->misc&DIR_FREE) f.cdt=0L;
totalcdt+=f.cdt;
if(!(useron.exempt&FLAG('D')) && totalcdt>useron.cdt+useron.freecdt) {
	bprintf(text[CantAddToQueue],f.name);
	bprintf(text[YouOnlyHaveNCredits],ultoac(useron.cdt+useron.freecdt,tmp));
    return(0); }
if(!chk_ar(dir[f.dir]->dl_ar,useron)) {
	bprintf(text[CantAddToQueue],f.name);
	bputs(text[CantDownloadFromDir]);
	return(0); }
for(i=0,totalsize=totaltime=0;i<batdn_total;i++) {
    totalsize+=batdn_size[i];
	if(!(dir[batdn_dir[i]]->misc&DIR_TFREE) && cur_cps)
		totaltime+=batdn_size[i]/(ulong)cur_cps; }
totalsize+=f.size;
if(!(dir[f.dir]->misc&DIR_TFREE) && cur_cps)
	totaltime+=f.size/(ulong)cur_cps;
if(!(useron.exempt&FLAG('T')) && totaltime>timeleft) {
	bprintf(text[CantAddToQueue],f.name);
    bputs(text[NotEnoughTimeToDl]);
    return(0); }
strcpy(batdn_name[batdn_total],f.name);
batdn_dir[batdn_total]=f.dir;
batdn_cdt[batdn_total]=f.cdt;
batdn_offset[batdn_total]=f.datoffset;
batdn_size[batdn_total]=f.size;
batdn_alt[batdn_total]=f.altpath;
batdn_total++;
openfile(f);
bprintf(text[FileAddedToBatDlQueue]
    ,f.name,batdn_total,max_batdn,ultoac(totalcdt,tmp)
    ,ultoac(totalsize,tmp2)
    ,sectostr(totalsize/(ulong)cur_cps,str));
return(1);
}

/****************************************************************************/
/* This function returns the command line for the temp file extension for	*/
/* current user online. 													*/
/****************************************************************************/
char *temp_cmd(void)
{
	int i;

if(!total_fcomps) {
	errormsg(WHERE,ERR_CHK,"compressable file types",0);
	return(nulstr); }
for(i=0;i<total_fcomps;i++)
	if(!stricmp(useron.tmpext,fcomp[i]->ext)
		&& chk_ar(fcomp[i]->ar,useron))
		return(fcomp[i]->cmd);
return(fcomp[0]->cmd);
}

