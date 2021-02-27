#line 1 "XFER_MID.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"

/****************************************************************************/
/* This function is called when a file is unsuccessfully downloaded.        */
/* It logs the tranfer time and checks for possible leech protocol use.     */
/****************************************************************************/
void notdownloaded(ulong size, time_t start, time_t end)
{
    char str[256],tmp2[256];

sprintf(str,"Estimated Time: %s  Transfer Time: %s"
    ,sectostr(cur_cps ? size/cur_cps : 0,tmp)
    ,sectostr((uint)end-start,tmp2));
logline(nulstr,str);
if(leech_pct && cur_cps                 /* leech detection */
    && end-start>=leech_sec
    && end-start>=(double)(size/cur_cps)*(double)leech_pct/100.0) {
    sprintf(str,"Possible use of leech protocol (leech=%u  downloads=%u)"
        ,useron.leech+1,useron.dls);
    errorlog(str);
    useron.leech=adjustuserrec(useron.number,U_LEECH,2,1); }
}

/****************************************************************************/
/* List detailed information about the files in 'filespec'. Prompts for     */
/* action depending on 'mode.'                                              */
/* Returns number of files matching filespec that were found                */
/****************************************************************************/
int listfileinfo(uint dirnum, char *filespec, char mode)
{
	uchar str[258],path[258],dirpath[256],done=0,ch,fname[13],ext[513];
	uchar HUGE16 *ixbbuf,*usrxfrbuf=NULL,*p;
    int i,j,found=0,file;
    ulong l,m,usrcdt,usrxfrlen;
    time_t start,end;
    file_t f;

sprintf(str,"%sXFER.IXT",data_dir);
if(mode==FI_USERXFER && flength(str)>0L) {
    if((file=nopen(str,O_RDONLY))==-1) {
        errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
        return(0); }
    usrxfrlen=filelength(file);
	if((usrxfrbuf=MALLOC(usrxfrlen))==NULL) {
        close(file);
        errormsg(WHERE,ERR_ALLOC,str,usrxfrlen);
        return(0); }
    if(read(file,usrxfrbuf,usrxfrlen)!=usrxfrlen) {
        close(file);
		FREE(usrxfrbuf);
        errormsg(WHERE,ERR_READ,str,usrxfrlen);
        return(0); }
    close(file); }
sprintf(str,"%s%s.IXB",dir[dirnum]->data_dir,dir[dirnum]->code);
if((file=nopen(str,O_RDONLY))==-1)
    return(0);
l=filelength(file);
if(!l) {
    close(file);
    return(0); }
if((ixbbuf=(char *)MALLOC(l))==NULL) {
    close(file);
    errormsg(WHERE,ERR_ALLOC,str,l);
    return(0); }
if(lread(file,ixbbuf,l)!=l) {
    close(file);
    errormsg(WHERE,ERR_READ,str,l);
	FREE((char *)ixbbuf);
    if(usrxfrbuf)
		FREE(usrxfrbuf);
    return(0); }
close(file);
sprintf(str,"%s%s.DAT",dir[dirnum]->data_dir,dir[dirnum]->code);
if((file=nopen(str,O_RDONLY))==-1) {
    errormsg(WHERE,ERR_READ,str,O_RDONLY);
	FREE((char *)ixbbuf);
    if(usrxfrbuf)
		FREE(usrxfrbuf);
    return(0); }
close(file);
m=0;
while(online && !done && m<l) {
	if(mode==FI_REMOVE && dir_op(dirnum))
        action=NODE_SYSP;
    else action=NODE_LFIL;
    if(msgabort()) {
        found=-1;
        break; }
    for(i=0;i<12 && m<l;i++)
        if(i==8)
            str[i]='.';
        else
            str[i]=ixbbuf[m++];     /* Turns FILENAMEEXT into FILENAME.EXT */
    str[i]=0;
    unpadfname(str,fname);
    if(filespec[0] && !filematch(str,filespec)) {
        m+=11;
        continue; }
    f.datoffset=ixbbuf[m]|((long)ixbbuf[m+1]<<8)|((long)ixbbuf[m+2]<<16);
    f.dateuled=ixbbuf[m+3]|((long)ixbbuf[m+4]<<8)
        |((long)ixbbuf[m+5]<<16)|((long)ixbbuf[m+6]<<24);
    f.datedled=ixbbuf[m+7]|((long)ixbbuf[m+8]<<8)
        |((long)ixbbuf[m+9]<<16)|((long)ixbbuf[m+10]<<24);
    m+=11;
    if(mode==FI_OLD && f.datedled>ns_time)
        continue;
    if((mode==FI_OLDUL || mode==FI_OLD) && f.dateuled>ns_time)
        continue;
	f.dir=curdirnum=dirnum;
    strcpy(f.name,str);
	f.size=0;
    getfiledat(&f);
	if(mode==FI_OFFLINE && f.size>=0)
		continue;
    if(f.altpath>0 && f.altpath<=altpaths)
        strcpy(dirpath,altpath[f.altpath-1]);
    else
        strcpy(dirpath,dir[f.dir]->path);
    if(mode==FI_CLOSE && !f.opencount)
        continue;
    if(mode==FI_USERXFER) {
        for(p=usrxfrbuf;p<usrxfrbuf+usrxfrlen;p+=24) {
            sprintf(str,"%17.17s",p);   /* %4.4u %12.12s */
            if(!strcmp(str+5,f.name) && useron.number==atoi(str))
                break; }
        if(p>=usrxfrbuf+usrxfrlen) /* file wasn't found */
            continue; }
	if((mode==FI_REMOVE) && (!dir_op(dirnum) && strcmpi(f.uler
		,useron.alias) && !(useron.exempt&FLAG('R'))))
		continue;
    found++;
	if(mode==FI_INFO) {
		if(!viewfile(f,1)) {
			done=1;
			found=-1; } }
	else
		fileinfo(f);
    if(mode==FI_CLOSE) {
        if(!noyes(text[CloseFileRecordQ])) {
            f.opencount=0;
            putfiledat(f); } }
    else if(mode==FI_REMOVE || mode==FI_OLD || mode==FI_OLDUL
        || mode==FI_OFFLINE) {
        SYNC;
        CRLF;
        if(f.opencount) {
            mnemonics(text[QuitOrNext]);
            strcpy(str,"Q\r"); }
		else if(dir_op(dirnum)) {
            mnemonics(text[SysopRemoveFilePrompt]);
			strcpy(str,"VEFMCQR\r"); }
        else if(useron.exempt&FLAG('R')) {
            mnemonics(text[RExemptRemoveFilePrompt]);
			strcpy(str,"VEMQR\r"); }
        else {
            mnemonics(text[UserRemoveFilePrompt]);
			strcpy(str,"VEQR\r"); }
        switch(getkeys(str,0)) {
			case 'V':
				viewfilecontents(f);
				CRLF;
				ASYNC;
				pause();
				m-=F_IXBSIZE;
				continue;
            case 'E':   /* edit file information */
				if(dir_op(dirnum)) {
                    bputs(text[EditFilename]);
                    strcpy(str,fname);
					getstr(str,12,K_EDIT|K_AUTODEL|K_UPPER);
                    if(strcmp(str,fname)) { /* rename */
                        padfname(str,path);
                        if(findfile(f.dir,path))
                            bprintf(text[FileAlreadyThere],path);
                        else {
                            sprintf(path,"%s%s",dirpath,fname);
                            sprintf(tmp,"%s%s",dirpath,str);
                            if(rename(path,tmp))
                                bprintf(text[CouldntRenameFile],path,tmp);
                            else {
                                bprintf(text[FileRenamed],path,tmp);
                                strcpy(fname,str);
                                removefiledat(f);
                                strcpy(f.name,padfname(str,tmp));
                                addfiledat(&f); } } } }
                bputs(text[EditDescription]);
				getstr(f.desc,LEN_FDESC,K_LINE|K_EDIT|K_AUTODEL);
                if(f.misc&FM_EXTDESC) {
					if(!noyes(text[DeleteExtDescriptionQ])) {
                        remove(str);
                        f.misc&=~FM_EXTDESC; } }
				if(!dir_op(dirnum)) {
                    putfiledat(f);
                    break; }
                bputs(text[EditUploader]);
				getstr(f.uler,LEN_ALIAS,K_UPRLWR|K_EDIT|K_AUTODEL);
                ultoa(f.cdt,str,10);
                bputs(text[EditCreditValue]);
				getstr(str,7,K_NUMBER|K_EDIT|K_AUTODEL);
                f.cdt=atol(str);
                itoa(f.timesdled,str,10);
                bputs(text[EditTimesDownloaded]);
				getstr(str,5,K_NUMBER|K_EDIT|K_AUTODEL);
                f.timesdled=atoi(str);
                if(f.opencount) {
                    itoa(f.opencount,str,10);
                    bputs(text[EditOpenCount]);
					getstr(str,3,K_NUMBER|K_EDIT|K_AUTODEL);
                    f.opencount=atoi(str); }
                if(altpaths || f.altpath) {
                    itoa(f.altpath,str,10);
                    bputs(text[EditAltPath]);
					getstr(str,3,K_NUMBER|K_EDIT|K_AUTODEL);
                    f.altpath=atoi(str);
                    if(f.altpath>altpaths)
                        f.altpath=0; }
                putfiledat(f);
				inputnstime(&f.dateuled);
				update_uldate(f);
                break;
            case 'F':   /* delete file only */
                sprintf(str,"%s%s",dirpath,fname);
                if(!fexist(str))
                    bputs(text[FileNotThere]);
                else {
                    if(!noyes(text[AreYouSureQ])) {
                        if(remove(str))
                            bprintf(text[CouldntRemoveFile],str);
                        else {
                            sprintf(tmp,"Deleted %s",str);
                            logline(nulstr,tmp); } } }
                break;
            case 'R':   /* remove file from database */
                if(noyes(text[AreYouSureQ]))
                    break;
                removefiledat(f);
                sprintf(str,"Removed %s from %s %s",f.name
                    ,lib[dir[f.dir]->lib]->sname,dir[f.dir]->sname);
                logline("U-",str);
                sprintf(str,"%s%s",dirpath,fname);
                if(fexist(str)) {
					if(dir_op(dirnum)) {
                        if(!noyes(text[DeleteFileQ])) {
                            if(remove(str))
                                bprintf(text[CouldntRemoveFile],str);
                            else {
                                sprintf(tmp,"Deleted %s",str);
                                logline(nulstr,tmp); } } }
                    else if(remove(str))    /* always remove if not sysop */
                        bprintf(text[CouldntRemoveFile],str); }
				if(dir_op(dirnum) || useron.exempt&FLAG('R')) {
                    i=lib[dir[f.dir]->lib]->offline_dir;
                    if(i!=dirnum && i!=(int)INVALID_DIR
                        && !findfile(i,f.name)) {
                        sprintf(str,text[AddToOfflineDirQ]
                            ,fname,lib[dir[i]->lib]->sname,dir[i]->sname);
                        if(yesno(str)) {
							getextdesc(f.dir,f.datoffset,ext);
                            f.dir=i;
							addfiledat(&f);
							if(f.misc&FM_EXTDESC)
								putextdesc(f.dir,f.datoffset,ext); } } }
				if(dir_op(dirnum) || strcmpi(f.uler,useron.alias)) {
                    if(noyes(text[RemoveCreditsQ]))
/* Fall through */      break; }
            case 'C':   /* remove credits only */
                if((i=matchuser(f.uler))==0) {
					bputs(text[UnknownUser]);
                    break; }
				if(dir_op(dirnum)) {
					usrcdt=(ulong)(f.cdt*(dir[f.dir]->up_pct/100.0));
                    if(f.timesdled)     /* all downloads */
                        usrcdt+=(ulong)((long)f.timesdled
							*f.cdt*(dir[f.dir]->dn_pct/100.0));
                    ultoa(usrcdt,str,10);
                    bputs(text[CreditsToRemove]);
					getstr(str,10,K_NUMBER|K_LINE|K_EDIT|K_AUTODEL);
                    f.cdt=atol(str); }
                usrcdt=adjustuserrec(i,U_CDT,10,-f.cdt);
                if(i==useron.number)
                    useron.cdt=usrcdt;
                sprintf(str,text[FileRemovedUserMsg]
					,f.name,f.cdt ? ultoac(f.cdt,tmp) : text[No]);
                putsmsg(i,str);
                usrcdt=adjustuserrec(i,U_ULB,10,-f.size);
                if(i==useron.number)
                    useron.ulb=usrcdt;
                usrcdt=adjustuserrec(i,U_ULS,5,-1);
                if(i==useron.number)
                    useron.uls=usrcdt;
                break;
            case 'M':   /* move the file to another dir */
				CRLF;
                for(i=0;i<usrlibs;i++)
                    bprintf(text[MoveToLibLstFmt],i+1,lib[usrlib[i]]->lname);
                SYNC;
                bprintf(text[MoveToLibPrompt],dir[dirnum]->lib+1);
				if((i=getnum(usrlibs))==-1)
                    continue;
                if(!i)
                    i=dir[dirnum]->lib;
                else
                    i--;
                CRLF;
                for(j=0;j<usrdirs[i];j++)
                    bprintf(text[MoveToDirLstFmt]
                        ,j+1,dir[usrdir[i][j]]->lname);
                SYNC;
                bprintf(text[MoveToDirPrompt],usrdirs[i]);
                if((j=getnum(usrdirs[i]))==-1)
                    continue;
                if(!j)
                    j=usrdirs[i]-1;
                else j--;
                CRLF;
                if(findfile(usrdir[i][j],f.name)) {
                    bprintf(text[FileAlreadyThere],f.name);
                    break; }
				getextdesc(f.dir,f.datoffset,ext);
                removefiledat(f);
				if(f.dir==upload_dir || f.dir==sysop_dir)
					f.dateuled=time(NULL);
                f.dir=usrdir[i][j];
                addfiledat(&f);
                bprintf(text[MovedFile],f.name
                    ,lib[dir[f.dir]->lib]->sname,dir[f.dir]->sname);
                sprintf(str,"Moved %s to %s %s",f.name
                    ,lib[dir[f.dir]->lib]->sname,dir[f.dir]->sname);
                logline(nulstr,str);
                if(!f.altpath) {    /* move actual file */
                    sprintf(str,"%s%s",dir[dirnum]->path,fname);
                    if(fexist(str)) {
                        sprintf(path,"%s%s",dir[f.dir]->path,fname);
                        mv(str,path,0); } }
				if(f.misc&FM_EXTDESC)
					putextdesc(f.dir,f.datoffset,ext);
                break;
            case 'Q':   /* quit */
                found=-1;
                done=1;
                break; } }
    else if(mode==FI_DOWNLOAD || mode==FI_USERXFER) {
        sprintf(path,"%s%s",dirpath,fname);
        if(f.size<1L) { /* getfiledat will set this to -1 if non-existant */
            SYNC;       /* and 0 byte files shouldn't be d/led */
            mnemonics(text[QuitOrNext]);
            if(getkeys("\rQ",0)=='Q') {
                found=-1;
                break; }
            continue; }
        if(!(dir[f.dir]->misc&DIR_FREE) && !(useron.exempt&FLAG('D'))
			&& f.cdt>(useron.cdt+useron.freecdt)) {
            SYNC;
			bprintf(text[YouOnlyHaveNCredits]
				,ultoac(useron.cdt+useron.freecdt,tmp));
            mnemonics(text[QuitOrNext]);
            if(getkeys("\rQ",0)=='Q') {
                found=-1;
                break; }
            continue; }
		if(!chk_ar(dir[f.dir]->dl_ar,useron)) {
			SYNC;
			bputs(text[CantDownloadFromDir]);
			mnemonics(text[QuitOrNext]);
            if(getkeys("\rQ",0)=='Q') {
                found=-1;
                break; }
            continue; }
		if(!(dir[f.dir]->misc&DIR_TFREE) && f.timetodl>timeleft && !dir_op(dirnum)
			&& !(useron.exempt&FLAG('T'))) {
            SYNC;
            bputs(text[NotEnoughTimeToDl]);
            mnemonics(text[QuitOrNext]);
            if(getkeys("\rQ",0)=='Q') {
                found=-1;
                break; }
            continue; }
        menu("DLPROT");
        openfile(f);
        SYNC;
        mnemonics(text[ProtocolBatchQuitOrNext]);
        strcpy(str,"BQ\r");
        for(i=0;i<total_prots;i++)
			if(prot[i]->dlcmd[0]
				&& chk_ar(prot[i]->ar,useron)) {
                sprintf(tmp,"%c",prot[i]->mnemonic);
                strcat(str,tmp); }
//		  ungetkey(useron.prot);
		ch=getkeys(str,0);
        if(ch=='Q') {
            found=-1;
            done=1; }
        else if(ch=='B') {
            if(!addtobatdl(f)) {
                closefile(f);
                break; } }
        else if(ch!=CR) {
            for(i=0;i<total_prots;i++)
				if(prot[i]->dlcmd[0] && prot[i]->mnemonic==ch
					&& chk_ar(prot[i]->ar,useron))
                    break;
            if(i<total_prots) {
				if(online==ON_LOCAL) {
					bputs(text[EnterPath]);
					if(getstr(path,60,K_UPPER|K_LINE)) {
						backslash(path);
						strcat(path,fname);
						sprintf(str,"%s%s",dirpath,fname);
						if(!mv(str,path,1))
							downloadfile(f);
						for(j=0;j<total_dlevents;j++)
							if(!stricmp(dlevent[j]->ext,f.name+9)
								&& chk_ar(dlevent[j]->ar,useron)) {
								bputs(dlevent[j]->workstr);
								external(cmdstr(dlevent[j]->cmd,path,nulstr
									,NULL)
									,EX_OUTL);
								CRLF; }
							} }
				else {
					delfiles(temp_dir,"*.*");
					if(dir[f.dir]->seqdev) {
						lncntr=0;
						seqwait(dir[f.dir]->seqdev);
						bprintf(text[RetrievingFile],fname);
						sprintf(str,"%s%s",dirpath,fname);
						sprintf(path,"%s%s",temp_dir,fname);
						mv(str,path,1); /* copy the file to temp dir */
						getnodedat(node_num,&thisnode,1);
						thisnode.aux=0xf0;
						putnodedat(node_num,thisnode);
						CRLF; }
					for(j=0;j<total_dlevents;j++)
						if(!stricmp(dlevent[j]->ext,f.name+9)
							&& chk_ar(dlevent[j]->ar,useron)) {
							bputs(dlevent[j]->workstr);
							external(cmdstr(dlevent[j]->cmd,path,nulstr,NULL)
								,EX_OUTL);
							CRLF; }
					getnodedat(node_num,&thisnode,1);
					action=NODE_DLNG;
					unixtodos(now+f.timetodl,&date,&curtime);
					thisnode.aux=(curtime.ti_hour*60)+curtime.ti_min;
					putnodedat(node_num,thisnode); /* calculate ETA */
					start=time(NULL);
					j=protocol(cmdstr(prot[i]->dlcmd,path,nulstr,NULL),0);
					end=time(NULL);
					if(dir[f.dir]->misc&DIR_TFREE)
						starttime+=end-start;
					if(prot[i]->misc&PROT_DSZLOG) {
						if(checkprotlog(f))
							downloadfile(f);
						else
							notdownloaded(f.size,start,end); }
					else {
						if(!j)
							downloadfile(f);
						else {
							bprintf(text[FileNotSent],f.name);
							notdownloaded(f.size,start,end); } }
					delfiles(temp_dir,"*.*");
					autohangup(); } } }
        closefile(f); }
    if(filespec[0] && !strchr(filespec,'*') && !strchr(filespec,'?')) break; }
FREE((char *)ixbbuf);
if(usrxfrbuf)
	FREE(usrxfrbuf);
return(found);
}


/****************************************************************************/
/* Checks  directory data file for 'filename' (must be padded). If found,   */
/* it returns the 1, else returns 0.                                        */
/* Called from upload and bulkupload                                        */
/****************************************************************************/
char findfile(uint dirnum, char *filename)
{
	char str[256],c,fname[13],HUGE16 *ixbbuf;
    int file;
    ulong length,l;

sprintf(fname,"%.12s",filename);
strupr(fname);
for(c=8;c<12;c++)   /* Turn FILENAME.EXT into FILENAMEEXT */
    fname[c]=fname[c+1];
sprintf(str,"%s%s.IXB",dir[dirnum]->data_dir,dir[dirnum]->code);
if((file=nopen(str,O_RDONLY))==-1) return(0);
length=filelength(file);
if(!length) {
    close(file);
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
close(file);
for(l=0;l<length;l+=F_IXBSIZE) {
    for(c=0;c<11;c++)
        if(fname[c]!=toupper(ixbbuf[l+c])) break;
    if(c==11) break; }
FREE((char *)ixbbuf);
if(l!=length)
    return(1);
return(0);
}

/****************************************************************************/
/* Prints one file's information on a single line to a file 'file'          */
/****************************************************************************/
void listfiletofile(char *fname, char HUGE16 *buf, uint dirnum, int file)
{
    char str[256],exist=1;
    uchar alt;
    ulong cdt;

strcpy(str,fname);
if(buf[F_MISC]!=ETX && (buf[F_MISC]-SP)&FM_EXTDESC)
    strcat(str,"+");
else
    strcat(str," ");
write(file,str,13);
getrec((char *)buf,F_ALTPATH,2,str);
alt=(uchar)ahtoul(str);
sprintf(str,"%s%s",alt>0 && alt<=altpaths ? altpath[alt-1]
    : dir[dirnum]->path,unpadfname(fname,tmp));
if(dir[dirnum]->misc&DIR_FCHK && !fexist(str))
    exist=0;
getrec((char *)buf,F_CDT,LEN_FCDT,str);
cdt=atol(str);
if(!cdt)
    strcpy(str,"   FREE");
else
    sprintf(str,"%7lu",cdt);
if(exist)
    strcat(str," ");
else
    strcat(str,"-");
write(file,str,8);
getrec((char *)buf,F_DESC,LEN_FDESC,str);
write(file,str,strlen(str));
write(file,crlf,2);
}



/****************************************************************************/
/* Handles start and stop routines for transfer protocols                   */
/****************************************************************************/
int protocol(char *cmdline, int cd)
{
	char tmp[128];
    int i;

sprintf(tmp,"%sPROTOCOL.LOG",node_dir);
remove(tmp);                            /* Deletes the protocol log */
if(useron.misc&AUTOHANG)
	autohang=1;
else
	autohang=yesno(text[HangUpAfterXferQ]);
if(sys_status&SS_ABORT) {				/* if ctrl-c */
	autohang=0;
	return(-1); }
bputs(text[StartXferNow]);
RIOSYNC(0);
lprintf("%s\r\n",cmdline);
if(cd) {
	if(temp_dir[1]==':')    /* fix for DSZ */
		setdisk(toupper(temp_dir[0])-'A');
	strcpy(tmp,temp_dir);
	tmp[strlen(tmp)-1]=0;	/* take off '\' */
	if(chdir(tmp))
		errormsg(WHERE,ERR_CHDIR,tmp,0); }
i=external(cmdline,EX_OUTL);	/* EX_CC removed because of error level prob */
if(online==ON_REMOTE)
    rioctl(IOFB);
CRLF;
if(autohang) sys_status|=SS_PAUSEOFF;	/* Pause off after download */
return(i);
}

/****************************************************************************/
/* Invokes the timed auto-hangup after transfer routine                     */
/****************************************************************************/
void autohangup()
{
    char a,c,k;

SYNC;
sys_status&=~SS_PAUSEOFF;		/* turn pause back on */
rioctl(IOFI);
if(!autohang) return;
lncntr=0;
bputs(text[Disconnecting]);
attr(GREEN);
outchar('[');
for(c=9,a=0;c>-1 && online && !a;c--) {
    checkline();
    attr(LIGHTGRAY|HIGH);
    bputs(itoa(c,tmp,10));
    attr(GREEN);
    outchar(']');
	while((k=inkey(0))!=0 && online) {
        if(toupper(k)=='H') {
            c=0;
            break; }
        if(toupper(k)=='A') {
            a=1;
            break; } }
	mswait(DELAY_AUTOHG);
	if(!a) {
		outchar(BS);
		outchar(BS); } }
if(c==-1) {
    bputs(text[Disconnected]);
    hangup(); }
else
    CRLF;
}

/****************************************************************************/
/* Checks dsz compatible log file for errors in transfer                    */
/* Returns 1 if the file in the struct file_t was successfuly transfered    */
/****************************************************************************/
char checkprotlog(file_t f)
{
	char str[256],size[128];
    int file;
    FILE *stream;

sprintf(str,"%sPROTOCOL.LOG",node_dir);
if((stream=fnopen(&file,str,O_RDONLY))==NULL) {
    bprintf(text[FileNotSent],f.name);
    if(f.dir<total_dirs)
		sprintf(str,"Attempted to download %s (%s) from %s %s"
			,f.name,ultoac(f.size,size)
            ,lib[dir[f.dir]->lib]->sname,dir[f.dir]->sname);
	else if(f.dir==total_dirs)
        strcpy(str,"Attempted to download QWK packet");
	else if(f.dir==total_dirs+1)
		sprintf(str,"Attempted to download attached file: %s",f.name);
    logline("D!",str);
    return(0); }
unpadfname(f.name,tmp);
if(tmp[strlen(tmp)-1]=='.')     /* DSZ log uses FILE instead of FILE. */
    tmp[strlen(tmp)-1]=0;       /* when there isn't an extension.     */
while(!ferror(stream)) {
    if(!fgets(str,256,stream))
        break;
    if(str[strlen(str)-2]==CR)
        str[strlen(str)-2]=0;       /* chop off CRLF */
    strupr(str);
    if(strstr(str,tmp)) {   /* Only check for name, Bimodem doesn't put path */
        logline(nulstr,str);
		if(str[0]=='E' || str[0]=='L' || (str[6]==SP && str[7]=='0'))
            break;          /* E for Error or L for Lost Carrier */
        fclose(stream);     /* or only sent 0 bytes! */
        return(1); } }
fclose(stream);
bprintf(text[FileNotSent],f.name);
if(f.dir<total_dirs)
    sprintf(str,"Attempted to download %s (%s) from %s %s",f.name
        ,ultoac(f.size,tmp),lib[dir[f.dir]->lib]->sname,dir[f.dir]->sname);
else
    strcpy(str,"Attempted to download QWK packet");
logline("D!",str);
return(0);
}

/*****************************************************************************/
/* View viewable file types from dir 'dirnum'                                */
/* 'fspec' must be padded                                                    */
/*****************************************************************************/
void viewfiles(uint dirnum, char *fspec)
{
    char viewcmd[256];
    int i;

curdirnum=dirnum;	/* for ARS */
sprintf(viewcmd,"%s%s",dir[dirnum]->path,fspec);
if(!fexist(viewcmd)) {
    bputs(text[FileNotFound]);
    return; }
padfname(fspec,tmp);
truncsp(tmp);
for(i=0;i<total_fviews;i++)
	if(!strcmp(tmp+9,fview[i]->ext) && chk_ar(fview[i]->ar,useron)) {
        strcpy(viewcmd,fview[i]->cmd);
        break; }
if(i==total_fviews) {
	bprintf(text[NonviewableFile],tmp+9);
    return; }
sprintf(tmp,"%s%s",dir[dirnum]->path,fspec);
if((i=external(cmdstr(viewcmd,tmp,tmp,NULL),EX_OUTL|EX_OUTR|EX_INR|EX_CC))!=0)
	errormsg(WHERE,ERR_EXEC,viewcmd,i);    /* must of EX_CC to ^C */
}

/****************************************************************************/
/* Compares filenames for ascending name sort								*/
/****************************************************************************/
int fnamecmp_a(char **str1, char **str2)
{
return(strncmp(*str1,*str2,11));
}

/****************************************************************************/
/* Compares filenames for descending name sort								*/
/****************************************************************************/
int fnamecmp_d(char **str1, char **str2)
{
return(strncmp(*str2,*str1,11));
}

/****************************************************************************/
/* Compares file upload dates for ascending date sort						*/
/****************************************************************************/
int fdatecmp_a(uchar **buf1, uchar **buf2)
{
	time_t date1,date2;

date1=((*buf1)[14]|((long)(*buf1)[15]<<8)|((long)(*buf1)[16]<<16)
	|((long)(*buf1)[17]<<24));
date2=((*buf2)[14]|((long)(*buf2)[15]<<8)|((long)(*buf2)[16]<<16)
	|((long)(*buf2)[17]<<24));
if(date1>date2)	return(1);
if(date1<date2)	return(-1);
return(0);
}

/****************************************************************************/
/* Compares file upload dates for descending date sort						*/
/****************************************************************************/
int fdatecmp_d(uchar **buf1, uchar **buf2)
{
	time_t date1,date2;

date1=((*buf1)[14]|((long)(*buf1)[15]<<8)|((long)(*buf1)[16]<<16)
	|((long)(*buf1)[17]<<24));
date2=((*buf2)[14]|((long)(*buf2)[15]<<8)|((long)(*buf2)[16]<<16)
	|((long)(*buf2)[17]<<24));
if(date1>date2)	return(-1);
if(date1<date2)	return(1);
return(0);
}

/************************************************************************/
/* Wait (for a limited period of time) for sequential dev to become 	*/
/* available for file retrieval 										*/
/************************************************************************/
void seqwait(uint devnum)
{
	char loop=0;
	int i;
	time_t start;
	node_t node;


if(!devnum)
	return;
for(start=now=time(NULL);online && now-start<90;now=time(NULL)) {
	if(msgabort())				/* max wait ^^^^ sec */
		break;
	getnodedat(node_num,&thisnode,1);	/* open and lock this record */
	for(i=1;i<=sys_nodes;i++) {
		if(i==node_num) continue;
		getnodedat(i,&node,1);
		if((node.status==NODE_INUSE || node.status==NODE_QUIET)
			&& node.action==NODE_RFSD && node.aux==devnum) {
			putnodedat(i,node);
			break; }
		putnodedat(i,node); }
	if(i>sys_nodes) {
		thisnode.action=NODE_RFSD;
		thisnode.aux=devnum;
		putnodedat(node_num,thisnode);	/* write devnum, unlock, and ret */
		return; }
	putnodedat(node_num,thisnode);
	if(!loop)
		bprintf(text[WaitingForDeviceN],devnum);
	loop=1;
	mswait(100); }

}
