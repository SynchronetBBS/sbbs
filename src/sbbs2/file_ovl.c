#line 1 "FILE_OVL.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"

/****************************************************************************/
/* Adds file data in 'f' to DIR#.DAT and DIR#.IXB   and updates user        */
/* info for uploader. Must have .name, .desc and .dir fields filled prior   */
/* to a call to this function.                                              */
/* Returns 1 if file uploaded sucessfully, 0 if not.                        */
/****************************************************************************/
char uploadfile(file_t *f)
{
	uchar path[256],str[256],fname[25],ext[513],desc[513],tmp[128],*p;
	static uchar sbbsfilename[128],sbbsfiledesc[128];
	int  file;
    uint i;
    long length;
	FILE *stream;

f->misc=0;
curdirnum=f->dir;
if(findfile(f->dir,f->name)) {
	errormsg(WHERE,ERR_CHK,f->name,f->dir);
    return(0); }
sprintf(path,"%s%s",f->altpath>0 && f->altpath<=altpaths
	? altpath[f->altpath-1]
	: dir[f->dir]->path,unpadfname(f->name,fname));
if(!fexist(path)) {
	bprintf(text[FileNotReceived],f->name);
	sprintf(str,"Attempted to upload %s to %s %s (Not received)",f->name
		,lib[dir[f->dir]->lib]->sname,dir[f->dir]->sname);
    logline("U!",str);
    return(0); }
strcpy(tmp,f->name);
truncsp(tmp);
for(i=0;i<total_ftests;i++)
	if(ftest[i]->ext[0]=='*' || !strcmp(tmp+9,ftest[i]->ext)) {
		if(!chk_ar(ftest[i]->ar,useron))
			continue;
		attr(LIGHTGRAY);
        bputs(ftest[i]->workstr);

		sprintf(sbbsfilename,"SBBSFILENAME=%.12s",unpadfname(f->name,fname));
		putenv(sbbsfilename);
		sprintf(sbbsfiledesc,"SBBSFILEDESC=%.*s",LEN_FDESC,f->desc);
		putenv(sbbsfiledesc);
		sprintf(str,"%sSBBSFILE.NAM",node_dir);
        if((stream=fopen(str,"w"))!=NULL) {
			fwrite(fname,1,strlen(fname),stream);
			fclose(stream); }
        sprintf(str,"%sSBBSFILE.DES",node_dir);
		if((stream=fopen(str,"w"))!=NULL) {
			fwrite(f->desc,1,strlen(f->desc),stream);
            fclose(stream); }
		if(external(cmdstr(ftest[i]->cmd,path,f->desc,NULL),0)) { /* EX_OUTL */
			bprintf(text[FileHadErrors],f->name,ftest[i]->ext);
            if(SYSOP) {
                if(!yesno(text[DeleteFileQ])) return(0); }
            remove(path);
			sprintf(str,"Attempted to upload %s to %s %s (%s Errors)",f->name
				,lib[dir[f->dir]->lib]->sname,dir[f->dir]->sname,ftest[i]->ext);
            logline("U!",str);
            return(0); }
        else {
			sprintf(str,"%sSBBSFILE.NAM",node_dir);
			if((stream=fopen(str,"r"))!=NULL) {
				if(fgets(str,128,stream)) {
					truncsp(str);
					strupr(str);
					padfname(str,f->name);
					strcpy(tmp,f->name);
					truncsp(tmp);
					sprintf(path,"%s%s",f->altpath>0 && f->altpath<=altpaths
						? altpath[f->altpath-1] : dir[f->dir]->path
						,unpadfname(f->name,fname)); }
                fclose(stream);
				}
			sprintf(str,"%sSBBSFILE.DES",node_dir);
			if((stream=fopen(str,"r"))!=NULL) {
				if(fgets(str,128,stream)) {
					truncsp(str);
					sprintf(f->desc,"%.*s",LEN_FDESC,str); }
				fclose(stream); }
            CRLF; } }

if((length=flength(path))<=0L) {
	bprintf(text[FileZeroLength],f->name);
    remove(path);
	sprintf(str,"Attempted to upload %s to %s %s (Zero length)",f->name
		,lib[dir[f->dir]->lib]->sname,dir[f->dir]->sname);
    logline("U!",str);
    return(0); }
if(dir[f->dir]->misc&DIR_DIZ) {
	for(i=0;i<total_fextrs;i++)
		if(!stricmp(fextr[i]->ext,tmp+9) && chk_ar(fextr[i]->ar,useron))
			break;
	if(i<total_fextrs) {
		sprintf(str,"%sFILE_ID.DIZ",temp_dir);
		remove(str);
		external(cmdstr(fextr[i]->cmd,path,"FILE_ID.DIZ",NULL),EX_OUTL);
		if(!fexist(str)) {
			sprintf(str,"%sDESC.SDI",temp_dir);
			remove(str);
			external(cmdstr(fextr[i]->cmd,path,"DESC.SDI",NULL),EX_OUTL); }
		if((file=nopen(str,O_RDONLY))!=-1) {
			memset(ext,0,513);
			read(file,ext,512);
			for(i=512;i;i--)
				if(ext[i-1]>SP)
					break;
			ext[i]=0;
			if(!f->desc[0]) {
				strcpy(desc,ext);
				strip_exascii(desc);
				strip_ctrl(desc);
				for(i=0;desc[i];i++)
					if(isalnum(desc[i]))
						break;
				sprintf(f->desc,"%.*s",LEN_FDESC,desc+i); }
			close(file);
			remove(str);
			f->misc|=FM_EXTDESC; } } }

logon_ulb+=length;  /* Update 'this call' stats */
logon_uls++;
if(dir[f->dir]->misc&DIR_AONLY)  /* Forced anonymous */
	f->misc|=FM_ANON;
f->cdt=length;
f->dateuled=time(NULL);
f->timesdled=0;
f->datedled=0L;
f->opencount=0;
strcpy(f->uler,useron.alias);
bprintf(text[FileNBytesReceived],f->name,ultoac(length,tmp));
if(!f->desc[0])
	sprintf(f->desc,"%.*s",LEN_FDESC,text[NoDescription]);
if(!addfiledat(f))
	return(0);

if(f->misc&FM_EXTDESC)
	putextdesc(f->dir,f->datoffset,ext);

sprintf(str,"Uploaded %s to %s %s",f->name,lib[dir[f->dir]->lib]->sname
	,dir[f->dir]->sname);
if(dir[f->dir]->upload_sem[0])
	if((file=nopen(dir[f->dir]->upload_sem,O_WRONLY|O_CREAT|O_TRUNC))!=-1)
        close(file);
logline("U+",str);
/**************************/
/* Update Uploader's Info */
/**************************/
useron.uls=adjustuserrec(useron.number,U_ULS,5,1);
useron.ulb=adjustuserrec(useron.number,U_ULB,10,length);
if(dir[f->dir]->up_pct && dir[f->dir]->misc&DIR_CDTUL) { /* credit for upload */
	if(dir[f->dir]->misc&DIR_CDTMIN && cur_cps)    /* Give min instead of cdt */
		useron.min=adjustuserrec(useron.number,U_MIN,10
			,((ulong)(length*(dir[f->dir]->up_pct/100.0))/cur_cps)/60);
	else
		useron.cdt=adjustuserrec(useron.number,U_CDT,10
			,(ulong)(f->cdt*(dir[f->dir]->up_pct/100.0))); }
return(1);
}


/****************************************************************************/
/* Updates downloader, uploader and downloaded file data                    */
/* Must have offset, dir and name fields filled prior to call.              */
/****************************************************************************/
void downloadfile(file_t f)
{
    char str[256],str2[256],fname[13];
    int i,file;
	long length,mod;
    ulong l;
	user_t uploader;

getfiledat(&f); /* Get current data - right after download */
if((length=f.size)<0L)
    length=0L;
logon_dlb+=length;  /* Update 'this call' stats */
logon_dls++;
bprintf(text[FileNBytesSent],f.name,ultoac(length,tmp));
sprintf(str,"Downloaded %s from %s %s",f.name,lib[dir[f.dir]->lib]->sname
    ,dir[f.dir]->sname);
logline("D-",str);
/****************************/
/* Update Downloader's Info */
/****************************/
useron.dls=adjustuserrec(useron.number,U_DLS,5,1);
useron.dlb=adjustuserrec(useron.number,U_DLB,10,length);
if(!(dir[f.dir]->misc&DIR_FREE) && !(useron.exempt&FLAG('D')))
	subtract_cdt(f.cdt);
/**************************/
/* Update Uploader's Info */
/**************************/
i=matchuser(f.uler);
uploader.number=i;
getuserdat(&uploader);
if(i && i!=useron.number && uploader.firston<f.dateuled) {
	l=f.cdt;
	if(!(dir[f.dir]->misc&DIR_CDTDL))	/* Don't give credits on d/l */
		l=0;
	if(dir[f.dir]->misc&DIR_CDTMIN && cur_cps) { /* Give min instead of cdt */
		mod=((ulong)(l*(dir[f.dir]->dn_pct/100.0))/cur_cps)/60;
		adjustuserrec(i,U_MIN,10,mod);
		sprintf(tmp,"%lu minute",mod);
		sprintf(str,text[DownloadUserMsg]
			,!strcmp(dir[f.dir]->code,"TEMP") ? temp_file : f.name
			,!strcmp(dir[f.dir]->code,"TEMP") ? text[Partially] : nulstr
			,useron.alias,tmp); }
	else {
		mod=(ulong)(l*(dir[f.dir]->dn_pct/100.0));
		adjustuserrec(i,U_CDT,10,mod);
		ultoac(mod,tmp);
		sprintf(str,text[DownloadUserMsg]
			,!strcmp(dir[f.dir]->code,"TEMP") ? temp_file : f.name
			,!strcmp(dir[f.dir]->code,"TEMP") ? text[Partially] : nulstr
			,useron.alias,tmp); }
    putsmsg(i,str); }
/*******************/
/* Update IXB File */
/*******************/
f.datedled=time(NULL);
sprintf(str,"%s%s.IXB",dir[f.dir]->data_dir,dir[f.dir]->code);
if((file=nopen(str,O_RDWR))==-1) {
    errormsg(WHERE,ERR_OPEN,str,O_RDWR);
    return; }
length=filelength(file);
if(length%F_IXBSIZE) {
    close(file);
    errormsg(WHERE,ERR_LEN,str,length);
    return; }
strcpy(fname,f.name);
for(i=8;i<12;i++)   /* Turn FILENAME.EXT into FILENAMEEXT */
    fname[i]=fname[i+1];
for(l=0;l<length;l+=F_IXBSIZE) {
    read(file,str,F_IXBSIZE);      /* Look for the filename in the IXB file */
    str[11]=0;
    if(!strcmp(fname,str)) break; }
if(l>=length) {
    close(file);
    errormsg(WHERE,ERR_CHK,f.name,0);
    return; }
lseek(file,l+18,SEEK_SET);
write(file,&f.datedled,4);  /* Write the current time stamp for datedled */
close(file);
/*******************/
/* Update DAT File */
/*******************/
f.timesdled++;
putfiledat(f);
/******************************************/
/* Update User to User index if necessary */
/******************************************/
if(f.dir==user_dir) {
    rmuserxfers(0,useron.number,f.name);
    if(!getuserxfers(0,0,f.name)) { /* check if any ixt entries left */
        sprintf(str,"%s%s",f.altpath>0 && f.altpath<=altpaths ?
            altpath[f.altpath-1] : dir[f.dir]->path,unpadfname(f.name,tmp));
        remove(str);
        removefiledat(f); } }
}

/****************************************************************************/
/* Removes DAT and IXB entries for the file in the struct 'f'               */
/****************************************************************************/
void removefiledat(file_t f)
{
	char c,str[256],ixbname[12],HUGE16 *ixbbuf,fname[13];
    int file;
    ulong l,length;

strcpy(fname,f.name);
for(c=8;c<12;c++)   /* Turn FILENAME.EXT into FILENAMEEXT */
    fname[c]=fname[c+1];
sprintf(str,"%s%s.IXB",dir[f.dir]->data_dir,dir[f.dir]->code);
if((file=nopen(str,O_RDONLY))==-1) {
    errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
    return; }
length=filelength(file);
if(!length) {
	close(file);
	return; }
if((ixbbuf=(char *)MALLOC(length))==0) {
    close(file);
    errormsg(WHERE,ERR_ALLOC,str,length);
    return; }
if(lread(file,ixbbuf,length)!=length) {
    close(file);
    errormsg(WHERE,ERR_READ,str,length);
	FREE((char *)ixbbuf);
    return; }
close(file);
if((file=nopen(str,O_WRONLY|O_TRUNC))==-1) {
    errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_TRUNC);
    return; }
for(l=0;l<length;l+=F_IXBSIZE) {
    for(c=0;c<11;c++)
        ixbname[c]=ixbbuf[l+c];
    ixbname[c]=0;
    if(strcmp(ixbname,fname))
		if(lwrite(file,&ixbbuf[l],F_IXBSIZE)!=F_IXBSIZE) {
            close(file);
            errormsg(WHERE,ERR_WRITE,str,F_IXBSIZE);
			FREE((char *)ixbbuf);
            return; } }
FREE((char *)ixbbuf);
close(file);
sprintf(str,"%s%s.DAT",dir[f.dir]->data_dir,dir[f.dir]->code);
if((file=nopen(str,O_WRONLY))==-1) {
    errormsg(WHERE,ERR_OPEN,str,O_WRONLY);
    return; }
lseek(file,f.datoffset,SEEK_SET);
c=ETX;          /* If first char of record is ETX, record is unused */
if(write(file,&c,1)!=1) { /* So write a D_T on the first byte of the record */
    close(file);
    errormsg(WHERE,ERR_WRITE,str,1);
    return; }
close(file);
if(f.dir==user_dir)  /* remove file from index */
    rmuserxfers(0,0,f.name);

}

/****************************************************************************/
/* Puts filedata into DIR_code.DAT file                                     */
/* Called from removefiles                                                  */
/****************************************************************************/
void putfiledat(file_t f)
{
    char buf[F_LEN+1],str[256];
    int file;
    long length;

putrec(buf,F_CDT,LEN_FCDT,ultoa(f.cdt,tmp,10));
putrec(buf,F_DESC,LEN_FDESC,f.desc);
putrec(buf,F_DESC+LEN_FDESC,2,crlf);
putrec(buf,F_ULER,LEN_ALIAS+5,f.uler);
putrec(buf,F_ULER+LEN_ALIAS+5,2,crlf);
putrec(buf,F_TIMESDLED,5,itoa(f.timesdled,tmp,10));
putrec(buf,F_TIMESDLED+5,2,crlf);
putrec(buf,F_OPENCOUNT,3,itoa(f.opencount,tmp,10));
putrec(buf,F_OPENCOUNT+3,2,crlf);
buf[F_MISC]=f.misc+SP;
putrec(buf,F_ALTPATH,2,hexplus(f.altpath,tmp));
putrec(buf,F_ALTPATH+2,2,crlf);
sprintf(str,"%s%s.DAT",dir[f.dir]->data_dir,dir[f.dir]->code);
if((file=nopen(str,O_WRONLY))==-1) {
    errormsg(WHERE,ERR_OPEN,str,O_WRONLY);
    return; }
length=filelength(file);
if(length%F_LEN) {
    close(file);
    errormsg(WHERE,ERR_LEN,str,length);
    return; }
if(f.datoffset>length) {
    close(file);
    errormsg(WHERE,ERR_LEN,str,length);
    return; }
lseek(file,f.datoffset,SEEK_SET);
if(write(file,buf,F_LEN)!=F_LEN) {
    close(file);
    errormsg(WHERE,ERR_WRITE,str,F_LEN);
    return; }
length=filelength(file);
close(file);
if(length%F_LEN)
    errormsg(WHERE,ERR_LEN,str,length);
}

/****************************************************************************/
/* Adds the data for struct filedat to the directory's data base.           */
/* changes the .datoffset field only                                        */
/* returns 1 if added successfully, 0 if not.								*/
/****************************************************************************/
char addfiledat(file_t *f)
{
	uchar str[256],fdat[F_LEN+1],fname[13],idx[3],c,HUGE16 *ixbbuf;
    int i,file;
	ulong length,l;
	time_t uldate;

/************************/
/* Add data to DAT File */
/************************/
sprintf(str,"%s%s.DAT",dir[f->dir]->data_dir,dir[f->dir]->code);
if((file=nopen(str,O_RDWR|O_CREAT))==-1) {
    errormsg(WHERE,ERR_OPEN,str,O_RDWR|O_CREAT);
	return(0); }
length=filelength(file);
if(length==0L)
    l=0L;
else {
    if(length%F_LEN) {
        close(file);
        errormsg(WHERE,ERR_LEN,str,length);
		return(0); }
    for(l=0;l<length;l+=F_LEN) {    /* Find empty slot */
        lseek(file,l,SEEK_SET);
        read(file,&c,1);
		if(c==ETX) break; }
	if(l/F_LEN>=MAX_FILES || l/F_LEN>=dir[f->dir]->maxfiles) {
		bputs(text[DirFull]);
		close(file);
		sprintf(str,"Directory Full: %s %s"
			,lib[dir[f->dir]->lib]->sname,dir[f->dir]->sname);
		logline("U!",str);
		return(0); } }
putrec(fdat,F_CDT,LEN_FCDT,ultoa(f->cdt,tmp,10));
putrec(fdat,F_DESC,LEN_FDESC,f->desc);
putrec(fdat,F_DESC+LEN_FDESC,2,crlf);
putrec(fdat,F_ULER,LEN_ALIAS+5,f->uler);
putrec(fdat,F_ULER+LEN_ALIAS+5,2,crlf);
putrec(fdat,F_TIMESDLED,5,ultoa(f->timesdled,tmp,10));
putrec(fdat,F_TIMESDLED+5,2,crlf);
putrec(fdat,F_OPENCOUNT,3,itoa(f->opencount,tmp,10));
putrec(fdat,F_OPENCOUNT+3,2,crlf);
fdat[F_MISC]=f->misc+SP;
putrec(fdat,F_ALTPATH,2,hexplus(f->altpath,tmp));
putrec(fdat,F_ALTPATH+2,2,crlf);
f->datoffset=l;
idx[0]=l&0xff;          /* Get offset within DAT file for IXB file */
idx[1]=(l>>8)&0xff;
idx[2]=(l>>16)&0xff;
lseek(file,l,SEEK_SET);
if(write(file,fdat,F_LEN)!=F_LEN) {
    close(file);
    errormsg(WHERE,ERR_WRITE,str,F_LEN);
	return(0); }
length=filelength(file);
close(file);
if(length%F_LEN)
    errormsg(WHERE,ERR_LEN,str,length);

/*******************************************/
/* Update last upload date/time stamp file */
/*******************************************/
sprintf(str,"%s%s.DAB",dir[f->dir]->data_dir,dir[f->dir]->code);
if((file=nopen(str,O_WRONLY|O_CREAT))==-1)
    errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT);
else {
    now=time(NULL);
    write(file,&now,4);
    close(file); }

/************************/
/* Add data to IXB File */
/************************/
strcpy(fname,f->name);
for(i=8;i<12;i++)   /* Turn FILENAME.EXT into FILENAMEEXT */
    fname[i]=fname[i+1];
sprintf(str,"%s%s.IXB",dir[f->dir]->data_dir,dir[f->dir]->code);
if((file=nopen(str,O_RDWR|O_CREAT))==-1) {
    errormsg(WHERE,ERR_OPEN,str,O_RDWR|O_CREAT);
	return(0); }
length=filelength(file);
if(length) {    /* IXB file isn't empty */
    if(length%F_IXBSIZE) {
        close(file);
        errormsg(WHERE,ERR_LEN,str,length);
		return(0); }
	if((ixbbuf=(char *)MALLOC(length))==NULL) {
        close(file);
        errormsg(WHERE,ERR_ALLOC,str,length);
		return(0); }
	if(lread(file,ixbbuf,length)!=length) {
        close(file);
        errormsg(WHERE,ERR_READ,str,length);
		FREE((char *)ixbbuf);
		return(0); }
/************************************************/
/* Sort by Name or Date, Assending or Decending */
/************************************************/
    if(dir[f->dir]->sort==SORT_NAME_A || dir[f->dir]->sort==SORT_NAME_D) {
        for(l=0;l<length;l+=F_IXBSIZE) {
            for(i=0;i<12 && fname[i]==ixbbuf[l+i];i++);
            if(i==12) {     /* file already in directory index */
                close(file);
                errormsg(WHERE,ERR_CHK,str,0);
				FREE((char *)ixbbuf);
				return(0); }
            if(dir[f->dir]->sort==SORT_NAME_A && fname[i]<ixbbuf[l+i])
                break;
            if(dir[f->dir]->sort==SORT_NAME_D && fname[i]>ixbbuf[l+i])
                break; } }
    else {  /* sort by date */
        for(l=0;l<length;l+=F_IXBSIZE) {
			uldate=(ixbbuf[l+14]|((long)ixbbuf[l+15]<<8)
				|((long)ixbbuf[l+16]<<16)|((long)ixbbuf[l+17]<<24));
			if(dir[f->dir]->sort==SORT_DATE_A && f->dateuled<uldate)
                break;
			if(dir[f->dir]->sort==SORT_DATE_D && f->dateuled>uldate)
                break; } }
    lseek(file,l,SEEK_SET);
    if(write(file,fname,11)!=11) {  /* Write filename to IXB file */
        close(file);
        errormsg(WHERE,ERR_WRITE,str,11);
		FREE((char *)ixbbuf);
		return(0); }
    if(write(file,idx,3)!=3) {  /* Write DAT offset into IXB file */
        close(file);
        errormsg(WHERE,ERR_WRITE,str,3);
		FREE((char *)ixbbuf);
		return(0); }
    write(file,&f->dateuled,sizeof(time_t));
    write(file,&f->datedled,4);              /* Write 0 for datedled */
	if(lwrite(file,&ixbbuf[l],length-l)!=length-l) { /* Write rest of IXB */
        close(file);
        errormsg(WHERE,ERR_WRITE,str,length-l);
		FREE((char *)ixbbuf);
		return(0); }
	FREE((char *)ixbbuf); }
else {              /* IXB file is empty... No files */
    if(write(file,fname,11)!=11) {  /* Write filename it IXB file */
        close(file);
        errormsg(WHERE,ERR_WRITE,str,11);
		return(0); }
    if(write(file,idx,3)!=3) {  /* Write DAT offset into IXB file */
        close(file);
        errormsg(WHERE,ERR_WRITE,str,3);
		return(0); }
    write(file,&f->dateuled,sizeof(time_t));
    write(file,&f->datedled,4); }
length=filelength(file);
close(file);
if(length%F_IXBSIZE)
    errormsg(WHERE,ERR_LEN,str,length);
return(1);
}

/****************************************************************************/
/* Update the upload date for the file 'f'                                  */
/****************************************************************************/
void update_uldate(file_t f)
{
	char str[256],fname[13];
	int i,file;
	long l,length;

/*******************/
/* Update IXB File */
/*******************/
sprintf(str,"%s%s.IXB",dir[f.dir]->data_dir,dir[f.dir]->code);
if((file=nopen(str,O_RDWR))==-1) {
	errormsg(WHERE,ERR_OPEN,str,O_RDWR);
    return; }
length=filelength(file);
if(length%F_IXBSIZE) {
    close(file);
	errormsg(WHERE,ERR_LEN,str,length);
    return; }
strcpy(fname,f.name);
for(i=8;i<12;i++)   /* Turn FILENAME.EXT into FILENAMEEXT */
    fname[i]=fname[i+1];
for(l=0;l<length;l+=F_IXBSIZE) {
    read(file,str,F_IXBSIZE);      /* Look for the filename in the IXB file */
    str[11]=0;
    if(!strcmp(fname,str)) break; }
if(l>=length) {
    close(file);
	errormsg(WHERE,ERR_CHK,f.name,length);
    return; }
lseek(file,l+14,SEEK_SET);
write(file,&f.dateuled,4);
close(file);

/*******************************************/
/* Update last upload date/time stamp file */
/*******************************************/
sprintf(str,"%s%s.DAB",dir[f.dir]->data_dir,dir[f.dir]->code);
if((file=nopen(str,O_WRONLY|O_CREAT))==-1)
	errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT);
else {
	write(file,&f.dateuled,4);
    close(file); }

}

