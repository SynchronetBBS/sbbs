#line 1 "FILE.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/*************************************************************************/
/* Add/delete/edit/view/retrieve the file information for struct file_t  */
/* Called only from functions within xfer.c								 */
/*************************************************************************/

#include "sbbs.h"

long fdate_dir(char *filespec);

void getextdesc(uint dirnum, ulong datoffset, char *ext)
{
	char str[256];
	int file;

memset(ext,0,513);
sprintf(str,"%s%s.EXB",dir[dirnum]->data_dir,dir[dirnum]->code);
if((file=nopen(str,O_RDONLY))==-1)
	return;
lseek(file,(datoffset/F_LEN)*512L,SEEK_SET);
read(file,ext,512);
close(file);
}

void putextdesc(uint dirnum, ulong datoffset, char *ext)
{
	char str[256],nulbuf[512];
	int file;

memset(nulbuf,0,512);
sprintf(str,"%s%s.EXB",dir[dirnum]->data_dir,dir[dirnum]->code);
if((file=nopen(str,O_WRONLY|O_CREAT))==-1)
	return;
lseek(file,0L,SEEK_END);
while(filelength(file)<(datoffset/F_LEN)*512L)
	write(file,nulbuf,512);
lseek(file,(datoffset/F_LEN)*512L,SEEK_SET);
write(file,ext,512);
close(file);
}

/****************************************************************************/
/* Prints all information of file in file_t structure 'f'					*/
/****************************************************************************/
void fileinfo(file_t f)
{
	char str[256],fname[13],ext[513];
	uint i,j;
	long t;

for(i=0;i<usrlibs;i++)
	if(usrlib[i]==dir[f.dir]->lib)
		break;
for(j=0;j<usrdirs[i];j++)
	if(usrdir[i][j]==f.dir)
		break;
unpadfname(f.name,fname);
bprintf(text[FiLib],i+1,lib[dir[f.dir]->lib]->lname);
bprintf(text[FiDir],j+1,dir[f.dir]->lname);
bprintf(text[FiFilename],fname);
if(f.size!=-1L)
	bprintf(text[FiFileSize],ultoac(f.size,tmp));
bprintf(text[FiCredits]
	,(dir[f.dir]->misc&DIR_FREE || !f.cdt) ? "FREE" : ultoac(f.cdt,tmp));
bprintf(text[FiDescription],f.desc);
bprintf(text[FiUploadedBy],f.misc&FM_ANON ? text[UNKNOWN_USER] : f.uler);
if(f.date)
	bprintf(text[FiFileDate],timestr(&f.date));
bprintf(text[FiDateUled],timestr(&f.dateuled));
bprintf(text[FiDateDled],f.datedled ? timestr(&f.datedled) : "Never");
bprintf(text[FiTimesDled],f.timesdled);
if(f.size!=-1L)
	bprintf(text[FiTransferTime],sectostr(f.timetodl,tmp));
if(f.altpath) {
	if(f.altpath<=altpaths) {
		if(SYSOP)
			bprintf(text[FiAlternatePath],altpath[f.altpath-1]); }
	else
		bprintf(text[InvalidAlternatePathN],f.altpath); }
CRLF;
if(f.misc&FM_EXTDESC) {
	getextdesc(f.dir,f.datoffset,ext);
	CRLF;
	putmsg(ext,P_NOATCODES);
	CRLF; }
if(f.size==-1L)
	bprintf(text[FileIsNotOnline],f.name);
if(f.opencount)
	bprintf(text[FileIsOpen],f.opencount,f.opencount>1 ? "s" : nulstr);

}

/****************************************************************************/
/* Gets file data from dircode.IXB file										*/
/* Need fields .name and .dir filled.                                       */
/* only fills .offset, .dateuled, and .datedled                             */
/****************************************************************************/
void getfileixb(file_t *f)
{
	uchar HUGE16 *ixbbuf,str[256],fname[13];
	int file;
	ulong l,length;

sprintf(str,"%s%s.IXB",dir[f->dir]->data_dir,dir[f->dir]->code);
if((file=nopen(str,O_RDONLY))==-1) {
	errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
	return; }
length=filelength(file);
if(length%F_IXBSIZE) {
	close(file);
	errormsg(WHERE,ERR_LEN,str,length);
	return; }
if((ixbbuf=MALLOC(length))==NULL) {
	close(file);
	errormsg(WHERE,ERR_ALLOC,str,length);
	return; }
if(lread(file,ixbbuf,length)!=length) {
	close(file);
	FREE((char *)ixbbuf);
	errormsg(WHERE,ERR_READ,str,length);
	return; }
close(file);
strcpy(fname,f->name);
for(l=8;l<12;l++)	/* Turn FILENAME.EXT into FILENAMEEXT */
	fname[l]=fname[l+1];
for(l=0;l<length;l+=F_IXBSIZE) {
	sprintf(str,"%11.11s",ixbbuf+l);
	if(!strcmp(str,fname))
		break; }
if(l>=length) {
	errormsg(WHERE,ERR_CHK,str,0);
	FREE((char *)ixbbuf);
	return; }
l+=11;
f->datoffset=ixbbuf[l]|((long)ixbbuf[l+1]<<8)|((long)ixbbuf[l+2]<<16);
f->dateuled=ixbbuf[l+3]|((long)ixbbuf[l+4]<<8)
	|((long)ixbbuf[l+5]<<16)|((long)ixbbuf[l+6]<<24);
f->datedled=ixbbuf[l+7]|((long)ixbbuf[l+8]<<8)
	|((long)ixbbuf[l+9]<<16)|((long)ixbbuf[l+10]<<24);
FREE((char *)ixbbuf);
}

/****************************************************************************/
/* Gets filedata from dircode.DAT file										*/
/* Need fields .name ,.dir and .offset to get other info    				*/
/* Does not fill .dateuled or .datedled fields.                             */
/****************************************************************************/
void getfiledat(file_t *f)
{
	char buf[F_LEN+1],str[256];
	int file;
	long length;

sprintf(str,"%s%s.DAT",dir[f->dir]->data_dir,dir[f->dir]->code);
if((file=nopen(str,O_RDONLY))==-1) {
	errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
	return; }
length=filelength(file);
if(f->datoffset>length) {
	close(file);
	errormsg(WHERE,ERR_LEN,str,length);
	return; }
if(length%F_LEN) {
	close(file);
	errormsg(WHERE,ERR_LEN,str,length);
	return; }
lseek(file,f->datoffset,SEEK_SET);
if(read(file,buf,F_LEN)!=F_LEN) {
	close(file);
	errormsg(WHERE,ERR_READ,str,F_LEN);
	return; }
close(file);
getrec(buf,F_ALTPATH,2,str);
f->altpath=hptoi(str);
getrec(buf,F_CDT,LEN_FCDT,str);
f->cdt=atol(str);

if(!f->size) {					/* only read disk if this is null */
//	  if(dir[f->dir]->misc&DIR_FCHK) {
		sprintf(str,"%s%s"
			,f->altpath>0 && f->altpath<=altpaths ? altpath[f->altpath-1]
			: dir[f->dir]->path,unpadfname(f->name,tmp));
		f->size=flength(str);
		f->date=fdate_dir(str);
/*
		}
	else {
		f->size=f->cdt;
		f->date=0; }
*/
		}
if((f->size>0L) && cur_cps)
	f->timetodl=(f->size/(ulong)cur_cps);
else
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
/* Increments the opencount on the file data 'f' and adds the transaction 	*/
/* to the backout.dab														*/
/****************************************************************************/
void openfile(file_t f)
{
	char str1[256],str2[4],str3[4],ch;
	int file;

/************************************/
/* Increment open count in dat file */
/************************************/
sprintf(str1,"%s%s.DAT",dir[f.dir]->data_dir,dir[f.dir]->code);
if((file=nopen(str1,O_RDWR))==-1) {
	errormsg(WHERE,ERR_OPEN,str1,O_RDWR);
	return; }
lseek(file,f.datoffset+F_OPENCOUNT,SEEK_SET);
if(read(file,str2,3)!=3) {
	close(file);
	errormsg(WHERE,ERR_READ,str1,3);
	return; }
str2[3]=0;
itoa(atoi(str2)+1,str3,10);
putrec(str2,0,3,str3);
lseek(file,f.datoffset+F_OPENCOUNT,SEEK_SET);
if(write(file,str2,3)!=3) {
	close(file);
	errormsg(WHERE,ERR_WRITE,str1,3);
	return; }
close(file);
/**********************************/
/* Add transaction to BACKOUT.DAB */
/**********************************/
sprintf(str1,"%sBACKOUT.DAB",node_dir);
if((file=nopen(str1,O_WRONLY|O_APPEND|O_CREAT))==-1) {
	errormsg(WHERE,ERR_OPEN,str1,O_WRONLY|O_APPEND|O_CREAT);
	return; }
ch=BO_OPENFILE;
write(file,&ch,1);				/* backout type */
write(file,dir[f.dir]->code,8); /* directory code */
write(file,&f.datoffset,4);		/* offset into .dat file */
write(file,&ch,BO_LEN-(1+8+4)); /* pad it */
close(file);
}

/****************************************************************************/
/* Decrements the opencount on the file data 'f' and removes the backout  	*/
/* from the backout.dab														*/
/****************************************************************************/
void closefile(file_t f)
{
	uchar str1[256],str2[4],str3[4],ch,*buf;
	int file;
	long length,l,offset;

/************************************/
/* Decrement open count in dat file */
/************************************/
sprintf(str1,"%s%s.DAT",dir[f.dir]->data_dir,dir[f.dir]->code);
if((file=nopen(str1,O_RDWR))==-1) {
	errormsg(WHERE,ERR_OPEN,str1,O_RDWR);
	return; }
lseek(file,f.datoffset+F_OPENCOUNT,SEEK_SET);
if(read(file,str2,3)!=3) {
	close(file);
	errormsg(WHERE,ERR_READ,str1,3);
	return; }
str2[3]=0;
ch=atoi(str2);
if(ch) ch--;
itoa(ch,str3,10);
putrec(str2,0,3,str3);
lseek(file,f.datoffset+F_OPENCOUNT,SEEK_SET);
if(write(file,str2,3)!=3) {
	close(file);
	errormsg(WHERE,ERR_WRITE,str1,3);
	return; }
close(file);
/*****************************************/
/* Removing transaction from BACKOUT.DAB */
/*****************************************/
sprintf(str1,"%sBACKOUT.DAB",node_dir);
if(flength(str1)<1L)	/* file is not there or empty */
	return;
if((file=nopen(str1,O_RDONLY))==-1) {
	errormsg(WHERE,ERR_OPEN,str1,O_RDONLY);
	return; }
length=filelength(file);
if((buf=MALLOC(length))==NULL) {
	close(file);
	errormsg(WHERE,ERR_ALLOC,str1,length);
	return; }
if(read(file,buf,length)!=length) {
	close(file);
	FREE(buf);
	errormsg(WHERE,ERR_READ,str1,length);
	return; }
close(file);
if((file=nopen(str1,O_WRONLY|O_TRUNC))==-1) {
	errormsg(WHERE,ERR_OPEN,str1,O_WRONLY|O_TRUNC);
	return; }
ch=0;								/* 'ch' is a 'file already removed' flag */
for(l=0;l<length;l+=BO_LEN) {       /* in case file is in backout.dab > 1 */
	if(!ch && buf[l]==BO_OPENFILE) {
		memcpy(str1,buf+l+1,8);
		str1[8]=0;
		memcpy(&offset,buf+l+9,4);
		if(!stricmp(str1,dir[f.dir]->code) && offset==f.datoffset) {
			ch=1;
			continue; } }
	write(file,buf+l,BO_LEN); }
FREE(buf);
close(file);
}
