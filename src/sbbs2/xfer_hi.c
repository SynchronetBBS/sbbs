#line 1 "XFER_HI.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"

uint temp_dirnum;

/****************************************************************************/
/* Uploads files                                                            */
/****************************************************************************/
void upload(uint dirnum)
{
    static char lastdesc[59];
	uchar str[256],src[256]={""},descbeg[25]={""},descend[25]={""},path[256]
        ,fname[13],keys[256],ch,*p;
    time_t start,end;
    int i,j,file,destuser[MAX_USERXFER],destusers=0;
	uint k;
    file_t f;
    struct dfree d;
    user_t user;
    node_t node;

if(sys_status&SS_EVENT && online==ON_REMOTE && !dir_op(dirnum))
    bprintf(text[UploadBeforeEvent],timeleft/60);
if(altul)
    strcpy(path,altpath[altul-1]);
else
    strcpy(path,dir[dirnum]->path);
if(path[1]==':')
    i=path[0]-'A'+1;
else i=0;
getdfree(i,&d);
if(d.df_sclus==0xffff)
    errormsg(WHERE,ERR_CHK,path,0);
if((ulong)d.df_bsec*(ulong)d.df_sclus
    *(ulong)d.df_avail<(ulong)min_dspace*1024L) {
    bputs(text[LowDiskSpace]);
    sprintf(str,"Diskspace is low: %s",path);
    errorlog(str);
	if(!dir_op(dirnum))
        return; }
bprintf(text[DiskNBytesFree],ultoac((ulong)d.df_bsec
    *(ulong)d.df_sclus*(ulong)d.df_avail,tmp));
f.dir=curdirnum=dirnum;
f.misc=0;
f.altpath=altul;
bputs(text[Filename]);
if(!getstr(fname,12,K_UPPER) || strchr(fname,'?') || strchr(fname,'*')
	|| !checkfname(fname) || (trashcan(fname,"FILE") && !dir_op(dirnum))) {
	if(fname[0])
		bputs(text[BadFilename]);
	return; }
if(dirnum==sysop_dir)
    sprintf(str,text[UploadToSysopDirQ],fname);
else if(dirnum==user_dir)
    sprintf(str,text[UploadToUserDirQ],fname);
else
    sprintf(str,text[UploadToCurDirQ],fname,lib[dir[dirnum]->lib]->sname
        ,dir[dirnum]->sname);
if(!yesno(str)) return;
action=NODE_ULNG;
padfname(fname,f.name);
sprintf(str,"%s%s",path,fname);
if(fexist(str)) {   /* File is on disk */
	if(!dir_op(dirnum) && online!=ON_LOCAL) {		 /* local users or sysops */
        bprintf(text[FileAlreadyThere],fname);
        return; }
    if(!yesno(text[FileOnDiskAddQ]))
        return; }
else if(online==ON_LOCAL) {
    bputs(text[FileNotOnDisk]);
    bputs(text[EnterPath]);
    if(!getstr(tmp,60,K_LINE|K_UPPER))
        return;
    backslash(tmp);
    sprintf(src,"%s%s",tmp,fname); }
strcpy(str,dir[dirnum]->exts);
strcpy(tmp,f.name);
truncsp(tmp);
j=strlen(str);
for(i=0;i<j;i+=ch+1) { /* Check extension of upload with allowable exts */
    p=strchr(str+i,',');
    if(p!=NULL)
        *p=0;
    ch=strlen(str+i);
    if(!strcmp(tmp+9,str+i))
        break; }
if(j && i>=j) {
    bputs(text[TheseFileExtsOnly]);
    bputs(dir[dirnum]->exts);
    CRLF;
	if(!dir_op(dirnum)) return; }
bputs(text[SearchingForDupes]);
for(i=k=0;i<usrlibs;i++)
	for(j=0;j<usrdirs[i];j++,k++) {
		outchar('.');
		if(k && !(k%5))
			bputs("\b\b\b\b\b     \b\b\b\b\b");
		if((usrdir[i][j]==dirnum || dir[usrdir[i][j]]->misc&DIR_DUPES)
			&& findfile(usrdir[i][j],f.name)) {
            bputs(text[SearchedForDupes]);
			bprintf(text[FileAlreadyOnline],f.name);
			if(!dir_op(dirnum))
				return; 	 /* File is in database for another dir */
			if(usrdir[i][j]==dirnum)
				return; } } /* don't allow duplicates */
bputs(text[SearchedForDupes]);
if(dirnum==user_dir) {  /* User to User transfer */
    bputs(text[EnterAfterLastDestUser]);
	while((!dir_op(dirnum) && destusers<max_userxfer) || destusers<MAX_USERXFER) {
        bputs(text[SendFileToUser]);
		if(!getstr(str,LEN_ALIAS,K_UPRLWR))
            break;
        if((user.number=finduser(str))!=0) {
			if(!dir_op(dirnum) && user.number==useron.number) {
                bputs(text[CantSendYourselfFiles]);
                continue; }
            for(i=0;i<destusers;i++)
                if(user.number==destuser[i])
                    break;
            if(i<destusers) {
                bputs(text[DuplicateUser]);
                continue; }
            getuserdat(&user);
            if((user.rest&(FLAG('T')|FLAG('D')))
                || !chk_ar(lib[dir[user_dir]->lib]->ar,user)
                || !chk_ar(dir[user_dir]->dl_ar,user)) {
                bprintf(text[UserWontBeAbleToDl],user.alias); }
            else {
                bprintf(text[UserAddedToDestList],user.alias);
                destuser[destusers++]=user.number; } }
        else {
            CRLF; } }
    if(!destusers)
        return; }
if(dir[dirnum]->misc&DIR_RATE) {
    SYNC;
    bputs(text[RateThisFile]);
	ch=getkey(K_ALPHA);
	if(!isalpha(ch) || sys_status&SS_ABORT)
		return;
	CRLF;
	sprintf(descbeg,text[Rated],toupper(ch)); }
if(dir[dirnum]->misc&DIR_ULDATE) {
    now=time(NULL);
    if(descbeg[0])
        strcat(descbeg," ");
    sprintf(str,"%s  ",unixtodstr(now,tmp));
    strcat(descbeg,str); }
if(dir[dirnum]->misc&DIR_MULT) {
    SYNC;
    if(!noyes(text[MultipleDiskQ])) {
        bputs(text[HowManyDisksTotal]);
        if((i=getnum(99))<2)
            return;
        bputs(text[NumberOfFile]);
        if((j=getnum(i))<1)
            return;
        if(j==1)
            lastdesc[0]=0;
        if(i>9)
            sprintf(descend,text[FileOneOfTen],j,i);
        else
            sprintf(descend,text[FileOneOfTwo],j,i); }
    else
        lastdesc[0]=0; }
else
    lastdesc[0]=0;
bputs(text[EnterDescNow]);
i=LEN_FDESC-(strlen(descbeg)+strlen(descend));
getstr(lastdesc,i,K_LINE|K_EDIT|K_AUTODEL);
if(sys_status&SS_ABORT)
	return;
if(descend[0])      /* end of desc specified, so pad desc with spaces */
    sprintf(f.desc,"%s%-*s%s",descbeg,i,lastdesc,descend);
else                /* no end specified, so string ends at desc end */
    sprintf(f.desc,"%s%s",descbeg,lastdesc);

if(dir[dirnum]->misc&DIR_ANON && !(dir[dirnum]->misc&DIR_AONLY)
	&& (dir_op(dirnum) || useron.exempt&FLAG('A'))) {
    if(!noyes(text[AnonymousQ]))
        f.misc|=FM_ANON; }
sprintf(str,"%s%s",path,fname);
if(src[0]) {    /* being copied from another local dir */
    bprintf(text[RetrievingFile],fname);
    if(mv(src,str,1))
        return;
    CRLF; }
if(fexist(str)) {   /* File is on disk */
	if(!uploadfile(&f))
        return; }
else {
    menu("ULPROT");
    SYNC;
    strcpy(keys,"Q");
    if(dirnum==user_dir || !max_batup)  /* no batch user to user xfers */
        mnemonics(text[ProtocolOrQuit]);
    else {
        mnemonics(text[ProtocolBatchOrQuit]);
        strcat(keys,"B"); }
    for(i=0;i<total_prots;i++)
		if(prot[i]->ulcmd[0] && chk_ar(prot[i]->ar,useron)) {
            sprintf(tmp,"%c",prot[i]->mnemonic);
            strcat(keys,tmp); }
    ch=getkeys(keys,0);
	if(ch=='Q')
		return;
    if(ch=='B') {
        if(batup_total>=max_batup)
            bputs(text[BatchUlQueueIsFull]);
        else {
            for(i=0;i<batup_total;i++)
                if(!strcmp(batup_name[i],f.name)) {
					bprintf(text[FileAlreadyInQueue],f.name);
                    return; }
            strcpy(batup_name[batup_total],f.name);
            strcpy(batup_desc[batup_total],f.desc);
            batup_dir[batup_total]=dirnum;
            batup_misc[batup_total]=f.misc;
            batup_alt[batup_total]=altul;
            batup_total++;
            bprintf(text[FileAddedToUlQueue]
                ,f.name,batup_total,max_batup); } }
    else {
        for(i=0;i<total_prots;i++)
			if(prot[i]->ulcmd[0] && prot[i]->mnemonic==ch
				&& chk_ar(prot[i]->ar,useron))
                break;
        if(i<total_prots) {
            start=time(NULL);
			protocol(cmdstr(prot[i]->ulcmd,str,nulstr,NULL),0);
            end=time(NULL);
			if(!(dir[dirnum]->misc&DIR_ULTIME)) /* Don't deduct upload time */
				starttime+=end-start;
			ch=uploadfile(&f);
            autohangup();
			if(!ch)  /* upload failed, don't process user to user xfer */
				return; } } }
if(dirnum==user_dir) {  /* Add files to XFER.IXT in INDX dir */
	sprintf(str,"%sXFER.IXT",data_dir);
    if((file=nopen(str,O_WRONLY|O_CREAT|O_APPEND))==-1) {
        errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_APPEND);
        return; }
    for(j=0;j<destusers;j++) {
        for(i=1;i<=sys_nodes;i++) { /* Tell user, if online */
            getnodedat(i,&node,0);
            if(node.useron==destuser[j] && !(node.misc&NODE_POFF)
                && (node.status==NODE_INUSE || node.status==NODE_QUIET)) {
                sprintf(str,text[UserToUserXferNodeMsg],node_num,useron.alias);
                putnmsg(i,str);
                break; } }
        if(i>sys_nodes) {   /* User not online */
            sprintf(str,text[UserSentYouFile],useron.alias);
            putsmsg(destuser[j],str); }
        sprintf(str,"%4.4u %12.12s %4.4u\r\n"
            ,destuser[j],f.name,useron.number);
        write(file,str,strlen(str)); }
    close(file); }
}

/****************************************************************************/
/* Checks directory for 'dir' and prompts user to enter description for     */
/* the files that aren't in the database.                                   */
/* Returns 1 if the user aborted, 0 if not.                                 */
/****************************************************************************/
char bulkupload(uint dirnum)
{
    char done,str[256];
    struct ffblk ff;
    file_t f;

memset(&f,0,sizeof(file_t));
f.dir=dirnum;
f.altpath=altul;
bprintf(text[BulkUpload],lib[dir[dirnum]->lib]->sname,dir[dirnum]->sname);
sprintf(str,"%s*.*",altul>0 && altul<=altpaths ? altpath[altul-1]
    : dir[dirnum]->path);
done=findfirst(str,&ff,0);
action=NODE_ULNG;
SYNC;
while(!done && !msgabort()) {
    if(gettotalfiles(dirnum)>dir[dirnum]->maxfiles) {
        bputs(text[DirFull]);
        return(0); }
    strupr(ff.ff_name);
    padfname(ff.ff_name,str);
    if(findfile(f.dir,str)==0) {
        strcpy(f.name,str);
		f.cdt=ff.ff_fsize;
        bprintf(text[BulkUploadDescPrompt],f.name,f.cdt);
		getstr(f.desc,LEN_FDESC,K_LINE);
		if(sys_status&SS_ABORT)
			return(1);
		uploadfile(&f); }	/* used to abort here if the file failed upload */
    done=findnext(&ff); }
return(0);
}



/****************************************************************************/
/* This is the batch menu section                                           */
/****************************************************************************/
void batchmenu()
{
    char str[129],tmp2[250],done=0,ch;
	uint i,j,xfrprot,xfrdir;
    ulong totalcdt,totalsize,totaltime;
    time_t start,end;
    int file,n;
    file_t f;

if(!batdn_total && !batup_total && upload_dir==INVALID_DIR) {
    bputs(text[NoFilesInBatchQueue]);
    return; }
if(useron.misc&(RIP|WIP) && !(useron.misc&EXPERT))
    menu("BATCHXFR");
lncntr=0;
while(online && !done && (batdn_total || batup_total
	|| upload_dir!=INVALID_DIR)) {
	if(!(useron.misc&(EXPERT|RIP|WIP))) {
        sys_status&=~SS_ABORT;
        if(lncntr) {
            SYNC;
            CRLF;
            if(lncntr)          /* CRLF or SYNC can cause pause */
                pause(); }
        menu("BATCHXFR"); }
    ASYNC;
    bputs(text[BatchMenuPrompt]);
    ch=getkeys("BCDLQRU?\r",0);
    if(ch>SP)
        logch(ch,0);
    switch(ch) {
        case '?':
			if(useron.misc&(EXPERT|RIP|WIP))
                menu("BATCHXFR");
            break;
        case CR:
        case 'Q':
            lncntr=0;
            done=1;
            break;
        case 'B':   /* Bi-directional transfers */
            if(useron.rest&FLAG('D')) {
                bputs(text[R_Download]);
				break; }
            if(useron.rest&FLAG('U')) {
                bputs(text[R_Upload]);
				break; }
            if(!batdn_total) {
                bputs(text[DownloadQueueIsEmpty]);
                break; }
			if(!batup_total && upload_dir==INVALID_DIR) {
                bputs(text[UploadQueueIsEmpty]);
                break; }
            for(i=0,totalcdt=0;i<batdn_total;i++)
                    totalcdt+=batdn_cdt[i];
            if(!(useron.exempt&FLAG('D'))
                && totalcdt>useron.cdt+useron.freecdt) {
                bprintf(text[YouOnlyHaveNCredits]
                    ,ultoac(useron.cdt+useron.freecdt,tmp));
                break; }
            for(i=0,totalsize=totaltime=0;i<batdn_total;i++) {
                totalsize+=batdn_size[i];
                if(!(dir[batdn_dir[i]]->misc&DIR_TFREE) && cur_cps)
                    totaltime+=batdn_size[i]/(ulong)cur_cps; }
			if(!(useron.exempt&FLAG('T')) && !SYSOP && totaltime>timeleft) {
                bputs(text[NotEnoughTimeToDl]);
                break; }
            menu("BIPROT");
            if(!create_batchdn_lst())
                break;
            if(!create_batchup_lst())
                break;
            if(!create_bimodem_pth())
                break;
            SYNC;
            mnemonics(text[ProtocolOrQuit]);
            strcpy(tmp2,"Q");
            for(i=0;i<total_prots;i++)
				if(prot[i]->bicmd[0] && chk_ar(prot[i]->ar,useron)) {
                    sprintf(tmp,"%c",prot[i]->mnemonic);
                    strcat(tmp2,tmp); }
			ungetkey(useron.prot);
            ch=getkeys(tmp2,0);
            if(ch=='Q')
                break;
            for(i=0;i<total_prots;i++)
				if(prot[i]->bicmd[0] && prot[i]->mnemonic==ch
					&& chk_ar(prot[i]->ar,useron))
                    break;
            if(i<total_prots) {
                xfrprot=i;
                action=NODE_BXFR;
                SYNC;
				for(i=0;i<batdn_total;i++)
                    if(dir[batdn_dir[i]]->seqdev) {
                        lncntr=0;
                        unpadfname(batdn_name[i],tmp);
                        sprintf(tmp2,"%s%s",temp_dir,tmp);
                        if(!fexist(tmp2)) {
                            seqwait(dir[batdn_dir[i]]->seqdev);
                            bprintf(text[RetrievingFile],tmp);
                            sprintf(str,"%s%s"
                                ,batdn_alt[i]>0 && batdn_alt[i]<=altpaths
                                ? altpath[batdn_alt[i]-1]
                                : dir[batdn_dir[i]]->path
                                ,tmp);
                            mv(str,tmp2,1); /* copy the file to temp dir */
                            getnodedat(node_num,&thisnode,1);
							thisnode.aux=0xff;
                            putnodedat(node_num,thisnode);
                            CRLF; } }
                sprintf(str,"%sBATCHDN.LST",node_dir);
                sprintf(tmp2,"%sBATCHUP.LST",node_dir);
                start=time(NULL);
				protocol(cmdstr(prot[xfrprot]->bicmd,str,tmp2,NULL),0);
                end=time(NULL);
				for(i=0;i<batdn_total;i++)
					if(dir[batdn_dir[i]]->seqdev) {
						unpadfname(batdn_name[i],tmp);
						sprintf(tmp2,"%s%s",temp_dir,tmp);
						remove(tmp2); }
                batch_upload();
                batch_download(xfrprot);
                if(batdn_total)     /* files still in queue, not xfered */
                    notdownloaded(totalsize,start,end);
                autohangup(); }
            break;
        case 'C':
            if(batup_total) {
                if(!noyes(text[ClearUploadQueueQ])) {
                    batup_total=0;
                    bputs(text[UploadQueueCleared]); } }
            if(batdn_total) {
                if(!noyes(text[ClearDownloadQueueQ])) {
                    for(i=0;i<batdn_total;i++) {
                        f.dir=batdn_dir[i];
                        f.datoffset=batdn_offset[i];
                        f.size=batdn_size[i];
                        strcpy(f.name,batdn_name[i]);
                        closefile(f); }
                    batdn_total=0;
                    bputs(text[DownloadQueueCleared]); } }
            break;
        case 'D':
            if(useron.rest&FLAG('D')) {
                bputs(text[R_Download]);
				break; }
            if(!batdn_total) {
                bputs(text[DownloadQueueIsEmpty]);
                break; }
            start_batch_download();
            break;
        case 'L':
            if(batup_total) {
                bputs(text[UploadQueueLstHdr]);
                for(i=0;i<batup_total;i++)
                    bprintf(text[UploadQueueLstFmt],i+1,batup_name[i]
                        ,batup_desc[i]); }
            if(batdn_total) {
                bputs(text[DownloadQueueLstHdr]);
                for(i=0,totalcdt=0,totalsize=0;i<batdn_total;i++) {
                    bprintf(text[DownloadQueueLstFmt],i+1
                        ,batdn_name[i],ultoac(batdn_cdt[i],tmp)
                        ,ultoac(batdn_size[i],str)
                        ,cur_cps
                        ? sectostr(batdn_size[i]/(ulong)cur_cps,tmp2)
                        : "??:??:??");
                    totalsize+=batdn_size[i];
                    totalcdt+=batdn_cdt[i]; }
                bprintf(text[DownloadQueueTotals]
                    ,ultoac(totalcdt,tmp),ultoac(totalsize,str),cur_cps
                    ? sectostr(totalsize/(ulong)cur_cps,tmp2)
                    : "??:??:??"); }
            break;  /* Questionable line ^^^, see note above function */
        case 'R':
            if(batup_total) {
                bprintf(text[RemoveWhichFromUlQueue],batup_total);
                n=getnum(batup_total);
                if(n>=1) {
                    n--;
                    batup_total--;
                    while(n<batup_total) {
                        batup_dir[n]=batup_dir[n+1];
                        batup_misc[n]=batup_misc[n+1];
                        batup_alt[n]=batup_alt[n+1];
                        strcpy(batup_name[n],batup_name[n+1]);
                        strcpy(batup_desc[n],batup_desc[n+1]);
                        n++; }
                    if(!batup_total)
                        bputs(text[UploadQueueCleared]); } }
             if(batdn_total) {
                bprintf(text[RemoveWhichFromDlQueue],batdn_total);
                n=getnum(batdn_total);
                if(n>=1) {
                    n--;
                    f.dir=batdn_dir[n];
                    strcpy(f.name,batdn_name[n]);
                    f.datoffset=batdn_offset[n];
                    f.size=batdn_size[n];
                    closefile(f);
                    batdn_total--;
                    while(n<batdn_total) {
                        strcpy(batdn_name[n],batdn_name[n+1]);
                        batdn_dir[n]=batdn_dir[n+1];
                        batdn_cdt[n]=batdn_cdt[n+1];
                        batdn_alt[n]=batdn_alt[n+1];
                        batdn_size[n]=batdn_size[n+1];
                        batdn_offset[n]=batdn_offset[n+1];
                        n++; }
                    if(!batdn_total)
                        bputs(text[DownloadQueueCleared]); } }
            break;
       case 'U':
			if(useron.rest&FLAG('U')) {
                bputs(text[R_Upload]);
				break; }
			if(!batup_total && upload_dir==INVALID_DIR) {
                bputs(text[UploadQueueIsEmpty]);
                break; }
            menu("BATUPROT");
            if(!create_batchup_lst())
                break;
            if(!create_bimodem_pth())
                break;
            ASYNC;
            mnemonics(text[ProtocolOrQuit]);
            strcpy(str,"Q");
            for(i=0;i<total_prots;i++)
				if(prot[i]->batulcmd[0] && chk_ar(prot[i]->ar,useron)) {
                    sprintf(tmp,"%c",prot[i]->mnemonic);
                    strcat(str,tmp); }
            ch=getkeys(str,0);
            if(ch=='Q')
                break;
            for(i=0;i<total_prots;i++)
				if(prot[i]->batulcmd[0] && prot[i]->mnemonic==ch
					&& chk_ar(prot[i]->ar,useron))
                    break;
            if(i<total_prots) {
                sprintf(str,"%sBATCHUP.LST",node_dir);
                xfrprot=i;
				if(batup_total)
					xfrdir=batup_dir[0];
				else
					xfrdir=upload_dir;
                action=NODE_ULNG;
                SYNC;
				if(online==ON_REMOTE) {
					delfiles(temp_dir,"*.*");
					start=time(NULL);
					protocol(cmdstr(prot[xfrprot]->batulcmd,str,nulstr,NULL),1);
					end=time(NULL);
					if(!(dir[xfrdir]->misc&DIR_ULTIME))
						starttime+=end-start; }
                batch_upload();
                delfiles(temp_dir,"*.*");
                autohangup(); }
            break; } }
delfiles(temp_dir,"*.*");
}

/****************************************************************************/
/* Download files from batch queue                                          */
/****************************************************************************/
void start_batch_download()
{
	char ch,str[256],tmp2[256],tmp3[128],fname[64];
    int j;
    uint i,xfrprot;
    ulong totalcdt,totalsize,totaltime;
    time_t start,end;
    file_t f;

if(useron.rest&FLAG('D')) {     /* Download restriction */
    bputs(text[R_Download]);
    return; }
for(i=0,totalcdt=0;i<batdn_total;i++)
    totalcdt+=batdn_cdt[i];
if(!(useron.exempt&FLAG('D'))
    && totalcdt>useron.cdt+useron.freecdt) {
    bprintf(text[YouOnlyHaveNCredits]
        ,ultoac(useron.cdt+useron.freecdt,tmp));
    return; }

if(online==ON_LOCAL) {          /* Local download */
    bputs(text[EnterPath]);
    if(!getstr(str,60,K_LINE|K_UPPER))
        return;
    backslash(str);
    for(i=0;i<batdn_total;i++) {
		curdirnum=batdn_dir[i]; 		/* for ARS */
        lncntr=0;
        unpadfname(batdn_name[i],tmp);
        sprintf(tmp2,"%s%s",str,tmp);
		seqwait(dir[batdn_dir[i]]->seqdev);
        bprintf(text[RetrievingFile],tmp);
        sprintf(tmp3,"%s%s"
            ,batdn_alt[i]>0 && batdn_alt[i]<=altpaths
            ? altpath[batdn_alt[i]-1]
            : dir[batdn_dir[i]]->path
            ,tmp);
        j=mv(tmp3,tmp2,1);
        getnodedat(node_num,&thisnode,1);
		thisnode.aux=30; /* clear the seq dev # */
        putnodedat(node_num,thisnode);
        CRLF;
        if(j)   /* copy unsuccessful */
			return;
		for(j=0;j<total_dlevents;j++)
			if(!stricmp(dlevent[j]->ext,batdn_name[i]+9)
				&& chk_ar(dlevent[j]->ar,useron)) {
				bputs(dlevent[j]->workstr);
				external(cmdstr(dlevent[j]->cmd,tmp2,nulstr,NULL),EX_OUTL);
				CRLF; }
		}
    for(i=0;i<batdn_total;i++) {
		curdirnum=batdn_dir[i]; 		/* for ARS */
        f.dir=batdn_dir[i];
        strcpy(f.name,batdn_name[i]);
        f.datoffset=batdn_offset[i];
        f.size=batdn_size[i];
        f.altpath=batdn_alt[i];
        downloadfile(f);
        closefile(f); }
    batdn_total=0;
    return; }

for(i=0,totalsize=totaltime=0;i<batdn_total;i++) {
    totalsize+=batdn_size[i];
    if(!(dir[batdn_dir[i]]->misc&DIR_TFREE) && cur_cps)
        totaltime+=batdn_size[i]/(ulong)cur_cps; }
if(!(useron.exempt&FLAG('T')) && !SYSOP && totaltime>timeleft) {
    bputs(text[NotEnoughTimeToDl]);
    return; }
menu("BATDPROT");
if(!create_batchdn_lst())
    return;
if(!create_bimodem_pth())
    return;
ASYNC;
mnemonics(text[ProtocolOrQuit]);
strcpy(tmp2,"Q");
for(i=0;i<total_prots;i++)
	if(prot[i]->batdlcmd[0] && chk_ar(prot[i]->ar,useron)) {
        sprintf(tmp,"%c",prot[i]->mnemonic);
        strcat(tmp2,tmp); }
ungetkey(useron.prot);
ch=getkeys(tmp2,0);
if(ch=='Q' || sys_status&SS_ABORT)
    return;
for(i=0;i<total_prots;i++)
	if(prot[i]->batdlcmd[0] && prot[i]->mnemonic==ch
		&& chk_ar(prot[i]->ar,useron))
        break;
if(i<total_prots) {
    xfrprot=i;
    /* delfiles(temp_dir,"*.*"); fix for CD-ROM */
	for(i=0;i<batdn_total;i++) {
		curdirnum=batdn_dir[i]; 		/* for ARS */
		unpadfname(batdn_name[i],fname);
        if(dir[batdn_dir[i]]->seqdev) {
            lncntr=0;
			sprintf(tmp2,"%s%s",temp_dir,fname);
            if(!fexist(tmp2)) {
                seqwait(dir[batdn_dir[i]]->seqdev);
				bprintf(text[RetrievingFile],fname);
                sprintf(str,"%s%s"
                    ,batdn_alt[i]>0 && batdn_alt[i]<=altpaths
                    ? altpath[batdn_alt[i]-1]
                    : dir[batdn_dir[i]]->path
					,fname);
                mv(str,tmp2,1); /* copy the file to temp dir */
                getnodedat(node_num,&thisnode,1);
				thisnode.aux=40; /* clear the seq dev # */
                putnodedat(node_num,thisnode);
                CRLF; } }
		else
			sprintf(tmp2,"%s%s"
				,batdn_alt[i]>0 && batdn_alt[i]<=altpaths
				? altpath[batdn_alt[i]-1]
				: dir[batdn_dir[i]]->path
				,fname);
		sprintf(str,"total_dlevents=%d",total_dlevents);
//		  logline("dl",str);
		for(j=0;j<total_dlevents;j++) {
//			  logline("dl",dlevent[j]->ext);
  //		  logline("dl",dlevent[j]->cmd);
	//		  logline("dl",tmp2);
			if(stricmp(dlevent[j]->ext,batdn_name[i]+9))
				continue;
//			  logline("dl","chkar");
			if(!chk_ar(dlevent[j]->ar,useron))
				continue;
//			  logline("dl","bputs");
			bputs(dlevent[j]->workstr);
//			  logline("dl","external");
			external(cmdstr(dlevent[j]->cmd,tmp2,nulstr,NULL),EX_OUTL);
//			  logline("dl","crlf");
			CRLF; }
//		  logline("dl","after dlevents");
		}

    sprintf(str,"%sBATCHDN.LST",node_dir);
    getnodedat(node_num,&thisnode,1);
    action=NODE_DLNG;
    if(cur_cps)
        unixtodos(now+(totalsize/(ulong)cur_cps)
            ,&date,&curtime);
    thisnode.aux=(curtime.ti_hour*60)+curtime.ti_min;
	thisnode.action=action;
    putnodedat(node_num,thisnode); /* calculate ETA */
    start=time(NULL);
	protocol(cmdstr(prot[xfrprot]->batdlcmd,str,nulstr,NULL),0);
    end=time(NULL);
    batch_download(xfrprot);
    if(batdn_total)
        notdownloaded(totalsize,start,end);
    /* delfiles(temp_dir,"*.*"); fix for CD-ROM */
    autohangup(); }
}

/*****************************************************************************/
/* Temp directory section. Files must be extracted here and both temp_uler   */
/* and temp_uler fields should be filled before entrance.                    */
/*****************************************************************************/
void temp_xfer()
{
    char str[256],tmp2[256],done=0,ch;
    uint i,dirnum=total_dirs,j,files;
    ulong bytes;
    time_t start,end;
    struct ffblk ff;
    struct dfree d;
    file_t f;

if(!usrlibs)
	return;
if(useron.rest&FLAG('D')) {
    bputs(text[R_Download]);
    return; }
/*************************************/
/* Create TEMP directory information */
/*************************************/
if((dir[dirnum]=(dir_t *)MALLOC(sizeof(dir_t)))==0) {
    errormsg(WHERE,ERR_ALLOC,"temp_dir",sizeof(dir_t));
    return; }
memset(dir[dirnum],0,sizeof(dir_t));
dir[dirnum]->lname="Temporary";
dir[dirnum]->sname="Temp";
strcpy(dir[dirnum]->code,"TEMP");
dir[dirnum]->path=temp_dir;
dir[dirnum]->maxfiles=MAX_FILES;
dir[dirnum]->data_dir=dir[0]->data_dir;
dir[dirnum]->op_ar=nulstr;
temp_dirnum=curdirnum=usrdir[curlib][curdir[curlib]];
total_dirs++;

/****************************/
/* Fill filedat information */
/****************************/
sprintf(f.name,"TEMP_%3.3d.%s",node_num,useron.tmpext);
strcpy(f.desc,"Temp File");
f.dir=dirnum;
f.misc=f.timesdled=f.dateuled=f.datedled=0L;

if(useron.misc&(RIP|WIP) && !(useron.misc&EXPERT))
    menu("TEMPXFER");
lncntr=0;
while(online && !done) {
	if(!(useron.misc&(EXPERT|RIP|WIP))) {
        sys_status&=~SS_ABORT;
        if(lncntr) {
            SYNC;
            CRLF;
            if(lncntr)          /* CRLF or SYNC can cause pause */
                pause(); }
        menu("TEMPXFER"); }
    ASYNC;
    bputs(text[TempDirPrompt]);
    strcpy(f.uler,temp_uler);
    ch=getkeys("ADEFNILQRVX?\r",0);
    if(ch>SP)
        logch(ch,0);
    switch(ch) {
        case 'A':   /* add to temp file */
            if(temp_dir[1]==':')
                i=temp_dir[0]-'A'+1;
            else i=0;
            getdfree(i,&d);
            if(d.df_sclus==0xffff)
                errormsg(WHERE,ERR_CHK,temp_dir,0);
            if((ulong)d.df_bsec*(ulong)d.df_sclus
                *(ulong)d.df_avail<(ulong)min_dspace*1024L) {
                bputs(text[LowDiskSpace]);
                sprintf(str,"Diskspace is low: %s",temp_dir);
                errorlog(str);
				if(!dir_op(dirnum))
                    break; }
            bprintf(text[DiskNBytesFree],ultoac((ulong)d.df_bsec
                *(ulong)d.df_sclus*(ulong)d.df_avail,tmp));
			if(!getfilespec(str))
                break;
            if(!checkfname(str))
                break;
            sprintf(tmp2,"Added %s to %s",str,f.name);
            logline(nulstr,tmp2);
            sprintf(tmp2,"%s%s",temp_dir,str);
            sprintf(str,"%s%s",temp_dir,f.name);
			external(cmdstr(temp_cmd(),str,tmp2,NULL),EX_CC|EX_OUTL|EX_OUTR);
            break;
        case 'D':   /* download from temp dir */
            sprintf(str,"%s%s",temp_dir,f.name);
            if(!fexist(str)) {
                bprintf(text[TempFileNotCreatedYet],f.name);
                break; }
            f.size=f.cdt=flength(str);
            f.opencount=0;
            if(temp_cdt)    /* if file was not free */
				f.cdt=f.size;
            else
                f.cdt=0;
            if(!(useron.exempt&FLAG('D'))
                && f.cdt>useron.cdt+useron.freecdt) {
                bprintf(text[YouOnlyHaveNCredits]
                    ,ultoac(useron.cdt+useron.freecdt,tmp));
                break; }    /* f.cdt must equal size here */
			if(!(useron.exempt&FLAG('T')) && !dir_op(dirnum)
                && !(dir[temp_dirnum]->misc&DIR_TFREE) && cur_cps
                && f.size/(ulong)cur_cps>timeleft) {
                bputs(text[NotEnoughTimeToDl]);
                break; }
            if(!chk_ar(dir[temp_dirnum]->dl_ar,useron)) {
                bputs(text[CantDownloadFromDir]);
                break; }
            addfiledat(&f);
            menu("DLPROT");
            SYNC;
            mnemonics(text[ProtocolOrQuit]);
            strcpy(tmp2,"Q");
            for(i=0;i<total_prots;i++)
				if(prot[i]->dlcmd[0] && chk_ar(prot[i]->ar,useron)) {
                    sprintf(tmp,"%c",prot[i]->mnemonic);
                    strcat(tmp2,tmp); }
			ungetkey(useron.prot);
            ch=getkeys(tmp2,0);
            for(i=0;i<total_prots;i++)
				if(prot[i]->dlcmd[0] && prot[i]->mnemonic==ch
					&& chk_ar(prot[i]->ar,useron))
                    break;
            if(i<total_prots) {
                getnodedat(node_num,&thisnode,1);
                action=NODE_DLNG;
                if(cur_cps)
                    unixtodos(now+(f.size/(ulong)cur_cps)
                        ,&date,&curtime);
                thisnode.aux=(curtime.ti_hour*60)+curtime.ti_min;
                putnodedat(node_num,thisnode); /* calculate ETA */
                start=time(NULL);
				j=protocol(cmdstr(prot[i]->dlcmd,str,nulstr,NULL),0);
                end=time(NULL);
                if(dir[temp_dirnum]->misc&DIR_TFREE)
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
                autohangup(); }
            removefiledat(f);
            break;
        case 'E':
			extract(usrdir[curlib][curdir[curlib]]);
            sys_status&=~SS_ABORT;
            break;
        case 'F':   /* Create a file list */
            delfiles(temp_dir,"*.*");
            create_filelist("FILELIST.TXT",0);
            if(!(sys_status&SS_ABORT))
                logline(nulstr,"Created list of all files");
            CRLF;
            sys_status&=~SS_ABORT;
            break;
        case 'I':   /* information on what's here */
            bprintf(text[TempFileInfo],f.uler,temp_file);
            break;
        case 'L':   /* list files in dir */
            if(!getfilespec(str))
                break;
			if(!checkfname(str))
				break;
            bytes=files=0L;
            CRLF;
            sprintf(tmp2,"%s%s",temp_dir,str);
            i=findfirst(tmp2,&ff,0);
            while(!i && !msgabort()) {
                bprintf("%s %10s\r\n",padfname(ff.ff_name,str)
                    ,ultoac(ff.ff_fsize,tmp));
                files++;
                bytes+=ff.ff_fsize;
                i=findnext(&ff); }
            if(!files)
                bputs(text[EmptyDir]);
            else if(files>1)
                bprintf(text[TempDirTotal],ultoac(bytes,tmp),files);
            break;
        case 'N':   /* Create a list of new files */
            delfiles(temp_dir,"*.*");
            create_filelist("NEWFILES.TXT",FL_ULTIME);
            if(!(sys_status&SS_ABORT))
                logline(nulstr,"Created list of new files");
            CRLF;
            sys_status&=~SS_ABORT;
            break;
        case 'R':   /* Remove files from dir */
            if(!getfilespec(str))
                break;
			// padfname(str,tmp);  Removed 04/14/96
			bprintf(text[NFilesRemoved],delfiles(temp_dir,tmp));
            break;
        case 'V':   /* view files in dir */
            bputs(text[FileSpec]);
            if(!getstr(str,12,K_UPPER) || !checkfname(str))
                break;
            viewfiles(dirnum,str);
            break;
        case CR:
        case 'Q':   /* quit */
            done=1;
            break;
        case 'X':   /* extract from archive in temp dir */
            extract(dirnum);
            sys_status&=~SS_ABORT;
            break;
        case '?':   /* menu */
			if(useron.misc&(EXPERT|RIP|WIP))
                menu("TEMPXFER");
            break; }
    if(sys_status&SS_ABORT)
        break; }
FREE(dir[dirnum]);
total_dirs--;
}


/****************************************************************************/
/* Re-sorts file directory 'dirnum' according to dir[dirnum]->sort type     */
/****************************************************************************/
void resort(uint dirnum)
{
	char str[25],ixbfname[128],datfname[128],exbfname[128],txbfname[128]
		,ext[512],nulbuf[512];
	uchar HUGE16 *ixbbuf, HUGE16 *datbuf;
	uchar HUGE16 *ixbptr[MAX_FILES];
	int ixbfile,datfile,exbfile,txbfile,i,j;
	ulong ixblen,datlen,offset,newoffset,l;

memset(nulbuf,0,512);
bprintf(text[ResortLineFmt],lib[dir[dirnum]->lib]->sname,dir[dirnum]->sname);
sprintf(ixbfname,"%s%s.IXB",dir[dirnum]->data_dir,dir[dirnum]->code);
sprintf(datfname,"%s%s.DAT",dir[dirnum]->data_dir,dir[dirnum]->code);
sprintf(exbfname,"%s%s.EXB",dir[dirnum]->data_dir,dir[dirnum]->code);
sprintf(txbfname,"%s%s.TXB",dir[dirnum]->data_dir,dir[dirnum]->code);

if(flength(ixbfname)<1L || flength(datfname)<1L) {
	remove(exbfname);
	remove(txbfname);
    remove(ixbfname);
    remove(datfname);
    bputs(text[ResortEmptyDir]);
    return; }
bputs(text[Sorting]);
if((ixbfile=nopen(ixbfname,O_RDONLY))==-1) {
    errormsg(WHERE,ERR_OPEN,ixbfname,O_RDONLY);
    return; }
if((datfile=nopen(datfname,O_RDONLY))==-1) {
    close(ixbfile);
    errormsg(WHERE,ERR_OPEN,datfname,O_RDONLY);
    return; }
ixblen=filelength(ixbfile);
datlen=filelength(datfile);
if((ixbbuf=MALLOC(ixblen))==NULL) {
    close(ixbfile);
    close(datfile);
    errormsg(WHERE,ERR_ALLOC,ixbfname,ixblen);
    return; }
if((datbuf=MALLOC(datlen))==NULL) {
    close(ixbfile);
    close(datfile);
    FREE((char *)ixbbuf);
    errormsg(WHERE,ERR_ALLOC,datfname,datlen);
    return; }
if(lread(ixbfile,ixbbuf,ixblen)!=ixblen) {
    close(ixbfile);
    close(datfile);
    FREE((char *)ixbbuf);
    FREE((char *)datbuf);
    errormsg(WHERE,ERR_READ,ixbfname,ixblen);
    return; }
if(lread(datfile,datbuf,datlen)!=datlen) {
    close(ixbfile);
    close(datfile);
    FREE((char *)ixbbuf);
    FREE((char *)datbuf);
    errormsg(WHERE,ERR_READ,datfname,datlen);
    return; }
close(ixbfile);
close(datfile);
if((ixbfile=nopen(ixbfname,O_WRONLY|O_TRUNC))==-1) {
    FREE((char *)ixbbuf);
    FREE((char *)datbuf);
    errormsg(WHERE,ERR_OPEN,ixbfname,O_WRONLY|O_TRUNC);
    return; }
if((datfile=nopen(datfname,O_WRONLY|O_TRUNC))==-1) {
    close(ixbfile);
    FREE((char *)ixbbuf);
    FREE((char *)datbuf);
    errormsg(WHERE,ERR_OPEN,datfname,O_WRONLY|O_TRUNC);
    return; }
for(l=0,i=0;l<ixblen && i<MAX_FILES;l+=F_IXBSIZE,i++)
    ixbptr[i]=ixbbuf+l;
switch(dir[dirnum]->sort) {
    case SORT_NAME_A:
        qsort((void *)ixbptr,ixblen/F_IXBSIZE,sizeof(ixbptr[0])
            ,(int(*)(const void*, const void*))fnamecmp_a);
        break;
    case SORT_NAME_D:
        qsort((void *)ixbptr,ixblen/F_IXBSIZE,sizeof(ixbptr[0])
            ,(int(*)(const void*, const void*))fnamecmp_d);
        break;
    case SORT_DATE_A:
        qsort((void *)ixbptr,ixblen/F_IXBSIZE,sizeof(ixbptr[0])
            ,(int(*)(const void*, const void*))fdatecmp_a);
        break;
    case SORT_DATE_D:
        qsort((void *)ixbptr,ixblen/F_IXBSIZE,sizeof(ixbptr[0])
            ,(int(*)(const void*, const void*))fdatecmp_d);
        break; }

if((exbfile=nopen(exbfname,O_RDWR|O_CREAT))==-1) {
	close(ixbfile);
	close(datfile);
	FREE((char *)ixbbuf);
    FREE((char *)datbuf);
    errormsg(WHERE,ERR_OPEN,exbfname,O_RDWR|O_CREAT);
	return; }
if((txbfile=nopen(txbfname,O_RDWR|O_CREAT))==-1) {
    close(exbfile);
	close(datfile);
	close(exbfile);
	FREE((char *)ixbbuf);
    FREE((char *)datbuf);
	errormsg(WHERE,ERR_OPEN,txbfname,O_RDWR|O_CREAT);
	return; }

for(i=0;i<ixblen/F_IXBSIZE;i++) {
    offset=ixbptr[i][11]|((long)ixbptr[i][12]<<8)|((long)ixbptr[i][13]<<16);
    lwrite(datfile,&datbuf[offset],F_LEN);

	newoffset=(ulong)i*(ulong)F_LEN;

	j=datbuf[offset+F_MISC];  /* misc bits */
	if(j!=ETX) j-=SP;
	if(j&FM_EXTDESC) { /* extended description */
		lseek(exbfile,(offset/F_LEN)*512L,SEEK_SET);
		memset(ext,0,512);
		read(exbfile,ext,512);
		while(filelength(txbfile)<(newoffset/F_LEN)*512L) {
//			  lseek(txbfile,0L,SEEK_END);
			write(txbfile,nulbuf,512); }
		lseek(txbfile,(newoffset/F_LEN)*512L,SEEK_SET);
		write(txbfile,ext,512); }

	str[0]=newoffset&0xff;	   /* Get offset within DAT file for IXB file */
	str[1]=(newoffset>>8)&0xff;
	str[2]=(newoffset>>16)&0xff;
    lwrite(ixbfile,ixbptr[i],11);       /* filename */
    lwrite(ixbfile,str,3);              /* offset */
    lwrite(ixbfile,ixbptr[i]+14,8); }   /* upload and download times */
close(exbfile);
close(txbfile);
close(ixbfile);
close(datfile);
remove(exbfname);
rename(txbfname,exbfname);
if(!flength(exbfname))
	remove(exbfname);
FREE((char *)ixbbuf);
FREE((char *)datbuf);
if(ixblen/F_IXBSIZE==datlen/F_LEN)
    bputs(text[Sorted]);
else
    bprintf(text[Compressed]
        ,(uint)((datlen/F_LEN)-(ixblen/F_IXBSIZE))
        ,ultoac(((datlen/F_LEN)-(ixblen/F_IXBSIZE))*F_LEN,tmp));
}

/*****************************************************************************/
/* Handles extraction from a normal transfer file to the temp directory      */
/*****************************************************************************/
void extract(uint dirnum)
{
    char fname[13],str[256],excmd[256],path[256],done
		,files[256],tmp[256],intmp=0;
    int i,j;
    struct ffblk ff;
    file_t f;
    struct dfree d;

temp_dirnum=curdirnum=dirnum;
if(!strcmp(dir[dirnum]->code,"TEMP"))
    intmp=1;
if(temp_dir[1]==':')
    i=temp_dir[0]-'A'+1;
else i=0;
getdfree(i,&d);
if(d.df_sclus==0xffff)
    errormsg(WHERE,ERR_CHK,temp_dir,0);
if((ulong)d.df_bsec*(ulong)d.df_sclus
    *(ulong)d.df_avail<(ulong)min_dspace*1024L) {
    bputs(text[LowDiskSpace]);
    sprintf(str,"Diskspace is low: %s",temp_dir);
    errorlog(str);
	if(!dir_op(dirnum))
        return; }
else if(!intmp) {   /* not in temp dir */
    CRLF; }
bprintf(text[DiskNBytesFree],ultoac((ulong)d.df_bsec
    *(ulong)d.df_sclus*(ulong)d.df_avail,tmp));
if(!intmp) {    /* not extracting FROM temp directory */
    sprintf(str,"%s*.*",temp_dir);
    if(fexist(str)) {
        bputs(text[RemovingTempFiles]);
        done=findfirst(str,&ff,0);
        while(!done) {
            sprintf(str,"%s%s",temp_dir,ff.ff_name);
            remove(str);
            done=findnext(&ff); }
        CRLF; } }
bputs(text[ExtractFrom]);
if(!getstr(fname,12,K_UPPER) || !checkfname(fname) || strchr(fname,'*')
    || strchr(fname,'?'))
    return;
padfname(fname,f.name);
strcpy(str,f.name);
truncsp(str);
for(i=0;i<total_fextrs;i++)
	if(!strcmp(str+9,fextr[i]->ext) && chk_ar(fextr[i]->ar,useron)) {
        strcpy(excmd,fextr[i]->cmd);
        break; }
if(i==total_fextrs) {
    bputs(text[UnextractableFile]);
    return; }
if(!intmp && !findfile(dirnum,f.name)) {    /* not temp dir */
    bputs(text[SearchingAllDirs]);
    for(i=0;i<usrdirs[curlib] && !msgabort();i++) {
        if(i==dirnum) continue;
        if(findfile(usrdir[curlib][i],f.name))
            break; }
    if(i==usrdirs[curlib]) { /* not found in cur lib */
        bputs(text[SearchingAllLibs]);
        for(i=0;i<usrlibs;i++) {
            if(i==curlib) continue;
            for(j=0;j<usrdirs[i] && !msgabort();j++)
                if(findfile(usrdir[i][j],f.name))
                    break;
            if(j<usrdirs[i])
                break; }
        if(i==usrlibs) {
            bputs(text[FileNotFound]);  /* not in database */
            return; }
		dirnum=usrdir[i][j]; }
	else
        dirnum=usrdir[curlib][i]; }
if(sys_status&SS_ABORT)
    return;
sprintf(path,"%s%s",dir[dirnum]->path,fname);
if(!intmp) {    /* not temp dir, so get temp_file info */
    f.datoffset=f.dateuled=f.datedled=0L;
    f.dir=dirnum;
    getfileixb(&f);
    if(!f.datoffset && !f.dateuled && !f.datedled)  /* error reading ixb */
        return;
    f.size=0;
    getfiledat(&f);
    fileinfo(f);
    if(f.altpath>0 && f.altpath<=altpaths)
        sprintf(path,"%s%s",altpath[f.altpath-1],fname);
    temp_dirnum=dirnum;
    if(dir[f.dir]->misc&DIR_FREE)
        temp_cdt=0L;
    else
        temp_cdt=f.cdt;
    strcpy(temp_uler,f.uler);
    strcpy(temp_file,f.name); }     /* padded filename */
if(!fexist(path)) {
    bputs(text[FileNotThere]);  /* not on disk */
    return; }
done=0;
while(online && !done) {
    mnemonics(text[ExtractFilesPrompt]);
    switch(getkeys("EVQ",0)) {
        case 'E':
			if(!getfilespec(str))
                break;
            if(!checkfname(str))
                break;
			if((i=external(cmdstr(excmd,path,str,NULL)
                ,EX_INR|EX_OUTL|EX_OUTR|EX_CC))!=0) {
				errormsg(WHERE,ERR_EXEC,cmdstr(excmd,path,str,NULL),i);
                return; }
			sprintf(tmp,"Extracted %s from %s",str,path);
			logline(nulstr,tmp);
            CRLF;
            break;
        case 'V':
            viewfiles(dirnum,fname);
            break;
        default:
            done=1;
            break; } }
}

/****************************************************************************/
/* Creates the file BATCHDN.LST in the node directory. Returns 1 if         */
/* everything goes okay. 0 if not.                                          */
/****************************************************************************/
int create_batchdn_lst()
{
    char str[256];
    int i,file;

sprintf(str,"%sBATCHDN.LST",node_dir);
if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
    errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
    return(0); }
for(i=0;i<batdn_total;i++) {
    if(batdn_dir[i]>=total_dirs || dir[batdn_dir[i]]->seqdev)
        strcpy(str,temp_dir);
    else
        strcpy(str,batdn_alt[i]>0 && batdn_alt[i]<=altpaths
            ? altpath[batdn_alt[i]-1] : dir[batdn_dir[i]]->path);
    write(file,str,strlen(str));
    unpadfname(batdn_name[i],str);
    strcat(str,crlf);
    write(file,str,strlen(str)); }
close(file);
return(1);
}

/****************************************************************************/
/* Creates the file BATCHUP.LST in the node directory. Returns 1 if         */
/* everything goes okay. 0 if not.                                          */
/* This list is not used by any protocols to date.                          */
/****************************************************************************/
int create_batchup_lst()
{
    char str[256];
    int i,file;

sprintf(str,"%sBATCHUP.LST",node_dir);
if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
    errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
    return(0); }
for(i=0;i<batup_total;i++) {
    if(batup_dir[i]>=total_dirs)
        strcpy(str,temp_dir);
    else
        strcpy(str,batup_alt[i]>0 && batup_alt[i]<=altpaths
            ? altpath[batup_alt[i]-1] : dir[batup_dir[i]]->path);
    write(file,str,strlen(str));
    unpadfname(batup_name[i],str);
    strcat(str,crlf);
    write(file,str,strlen(str)); }
close(file);
return(1);
}

/****************************************************************************/
/* Creates the file BIMODEM.PTH in the node directory. Returns 1 if         */
/* everything goes okay. 0 if not.                                          */
/****************************************************************************/
int create_bimodem_pth()
{
    char str[256],tmp2[512];
    int i,file;

sprintf(str,"%sBIMODEM.PTH",node_dir);  /* Create bimodem file */
if((file=nopen(str,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
    errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_TRUNC);
    return(0); }
for(i=0;i<batup_total;i++) {
    sprintf(str,"%s%s",batup_dir[i]>=total_dirs ? temp_dir
        : batup_alt[i]>0 && batup_alt[i]<=altpaths
        ? altpath[batup_alt[i]-1] : dir[batup_dir[i]]->path
        ,unpadfname(batup_name[i],tmp));
    sprintf(tmp2,"D       %-80.80s%-160.160s"
        ,unpadfname(batup_name[i],tmp),str);
    write(file,tmp2,248); }
for(i=0;i<batdn_total;i++) {
    sprintf(str,"%s%s"
        ,(batdn_dir[i]>=total_dirs || dir[batdn_dir[i]]->seqdev)
        ? temp_dir : batdn_alt[i]>0 && batdn_alt[i]<=altpaths
            ? altpath[batdn_alt[i]-1] : dir[batdn_dir[i]]->path
            ,unpadfname(batdn_name[i],tmp));
    sprintf(tmp2,"U       %-240.240s",str);
    write(file,tmp2,248); }
close(file);
return(1);
}

/****************************************************************************/
/* Processes files that were supposed to be received in the batch queue     */
/****************************************************************************/
void batch_upload()
{
    char str1[256],str2[256];
	int i,j,x,y;
    file_t f;
	struct ffblk ff;

for(i=0;i<batup_total;) {
	curdirnum=batup_dir[i]; 			/* for ARS */
    lncntr=0;                               /* defeat pause */
    unpadfname(batup_name[i],tmp);
    sprintf(str1,"%s%s",temp_dir,tmp);
    sprintf(str2,"%s%s",dir[batup_dir[i]]->path,tmp);
    if(fexist(str1) && fexist(str2)) { /* file's in two places */
        bprintf(text[FileAlreadyThere],batup_name[i]);
        remove(str1);    /* this is the one received */
        i++;
        continue; }
    if(fexist(str1))
        mv(str1,str2,0);
    strcpy(f.name,batup_name[i]);
    strcpy(f.desc,batup_desc[i]);
    f.dir=batup_dir[i];
    f.misc=batup_misc[i];
    f.altpath=batup_alt[i];
	if(uploadfile(&f)) {
        batup_total--;
        for(j=i;j<batup_total;j++) {
            batup_dir[j]=batup_dir[j+1];
            batup_alt[j]=batup_alt[j+1];
            batup_misc[j]=batup_misc[j+1];
            strcpy(batup_name[j],batup_name[j+1]);
            strcpy(batup_desc[j],batup_desc[j+1]); } }
    else i++; }
if(upload_dir==INVALID_DIR)
	return;
sprintf(str1,"%s*.*",temp_dir);
i=findfirst(str1,&ff,0);
while(!i) {
	memset(&f,0,sizeof(file_t));
	f.dir=upload_dir;
	padfname(ff.ff_name,f.name);
	strupr(f.name);
	sprintf(str1,"%s%s",temp_dir,ff.ff_name);
	for(x=0;x<usrlibs;x++) {
		for(y=0;y<usrdirs[x];y++)
			if(dir[usrdir[x][y]]->misc&DIR_DUPES
				&& findfile(usrdir[x][y],f.name))
				break;
		if(y<usrdirs[x])
            break; }
	sprintf(str2,"%s%s",dir[f.dir]->path,ff.ff_name);
	if(x<usrlibs || fexist(str2)) {
		bprintf(text[FileAlreadyOnline],f.name);
		remove(str1); }
	else {
		mv(str1,str2,0);
		uploadfile(&f); }
	i=findnext(&ff); }
}

/****************************************************************************/
/* Processes files that were supposed to be sent from the batch queue       */
/* xfrprot is -1 if downloading files from within QWK (no DSZLOG)           */
/****************************************************************************/
void batch_download(int xfrprot)
{
    int i,j;
    file_t f;

for(i=0;i<batdn_total;) {
    lncntr=0;                               /* defeat pause */
	f.dir=curdirnum=batdn_dir[i];
    strcpy(f.name,batdn_name[i]);
    f.datoffset=batdn_offset[i];
    f.size=batdn_size[i];
/*											   Removed 05/18/95
	if(dir[f.dir]->misc&DIR_TFREE && cur_cps)  Looks like it gave back double
        starttime+=f.size/(ulong)cur_cps;
*/
    f.altpath=batdn_alt[i];
    if(xfrprot==-1 || (prot[xfrprot]->misc&PROT_DSZLOG && checkprotlog(f))
        || !(prot[xfrprot]->misc&PROT_DSZLOG)) {
        if(dir[f.dir]->misc&DIR_TFREE && cur_cps)
            starttime+=f.size/(ulong)cur_cps;
        downloadfile(f);
        closefile(f);
        batdn_total--;
        for(j=i;j<batdn_total;j++) {
            strcpy(batdn_name[j],batdn_name[j+1]);
            batdn_dir[j]=batdn_dir[j+1];
            batdn_cdt[j]=batdn_cdt[j+1];
            batdn_alt[j]=batdn_alt[j+1];
            batdn_size[j]=batdn_size[j+1];
            batdn_offset[j]=batdn_offset[j+1]; } }
    else i++; }
}

/****************************************************************************/
/* Adds a list of files to the batch download queue 						*/
/****************************************************************************/
void batch_add_list(char *list)
{
    char str[128];
	int file,i,j,k;
    FILE *stream;
	file_t	f;

if((stream=fnopen(&file,list,O_RDONLY))!=NULL) {
	bputs(text[SearchingAllLibs]);
	while(!feof(stream)) {
		checkline();
		if(!online)
			break;
        if(!fgets(str,127,stream))
            break;
        truncsp(str);
        sprintf(f.name,"%.12s",str);
		strupr(f.name);
		lncntr=0;
        for(i=k=0;i<usrlibs;i++) {
            for(j=0;j<usrdirs[i];j++,k++) {
                outchar('.');
                if(k && !(k%5))
                    bputs("\b\b\b\b\b     \b\b\b\b\b");
                if(findfile(usrdir[i][j],f.name))
                    break; }
            if(j<usrdirs[i])
                break; }
        if(i<usrlibs) {
            f.dir=usrdir[i][j];
            getfileixb(&f);
            f.size=0;
            getfiledat(&f);
            if(f.size==-1L)
				bprintf(text[FileIsNotOnline],f.name);
            else
                addtobatdl(f); } }
    fclose(stream);
	remove(list);
	CRLF; }
}

/****************************************************************************/
/* Creates a text file named NEWFILES.DAT in the temp directory that        */
/* all new files since p-date. Returns number of files in list.             */
/****************************************************************************/
ulong create_filelist(char *name, char mode)
{
    char str[256];
	int i,j,d,file;
	ulong	l,k;

bprintf(text[CreatingFileList],name);
sprintf(str,"%s%s",temp_dir,name);
if((file=nopen(str,O_CREAT|O_WRONLY|O_APPEND))==-1) {
	errormsg(WHERE,ERR_OPEN,str,O_CREAT|O_WRONLY|O_APPEND);
    return(0); }
k=0;
if(mode&FL_ULTIME) {
    sprintf(str,"New files since: %s\r\n",timestr(&ns_time));
    write(file,str,strlen(str)); }
for(i=d=0;i<usrlibs;i++) {
	for(j=0;j<usrdirs[i];j++,d++) {
		outchar('.');
		if(d && !(d%5))
			bputs("\b\b\b\b\b     \b\b\b\b\b");
		if(mode&FL_ULTIME /* New-scan */
			&& (lib[usrlib[i]]->offline_dir==usrdir[i][j]
			|| dir[usrdir[i][j]]->misc&DIR_NOSCAN))
            continue;
        l=listfiles(usrdir[i][j],nulstr,file,mode);
		if((long)l==-1)
            break;
        k+=l; }
    if(j<usrdirs[i])
        break; }
if(k>1) {
    sprintf(str,"\r\n%d Files Listed.\r\n",k);
    write(file,str,strlen(str)); }
close(file);
if(k)
    bprintf(text[CreatedFileList],name);
else {
    bputs(text[NoFiles]);
    sprintf(str,"%s%s",temp_dir,name);
    remove(str); }
strcpy(temp_file,name);
strcpy(temp_uler,"File List");
return(k);
}

