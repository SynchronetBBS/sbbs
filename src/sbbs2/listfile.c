#line 1 "LISTFILE.C"

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

#include "sbbs.h"

int listfile(char *fname, char HUGE16 *buf, uint dirnum
	, char *search, char letter, ulong datoffset);

void clearline(void)
{
	int i;

outchar(CR);
if(useron.misc&ANSI)
	bputs("\x1b[K");
else {
	for(i=0;i<79;i++)
		outchar(SP);
	outchar(CR); }
}

/****************************************************************************/
/* Remove credits from uploader of file 'f'                                 */
/****************************************************************************/
void removefcdt(file_t f)
{
	char str[128];
	int u;
	long cdt;

if((u=matchuser(f.uler))==0) {
   bputs(text[UnknownUser]);
   return; }
cdt=0L;
if(dir[f.dir]->misc&DIR_CDTMIN && cur_cps) {
	if(dir[f.dir]->misc&DIR_CDTUL)
		cdt=((ulong)(f.cdt*(dir[f.dir]->up_pct/100.0))/cur_cps)/60;
	if(dir[f.dir]->misc&DIR_CDTDL
		&& f.timesdled)  /* all downloads */
		cdt+=((ulong)((long)f.timesdled
			*f.cdt*(dir[f.dir]->dn_pct/100.0))/cur_cps)/60;
	adjustuserrec(u,U_MIN,10,-cdt);
	sprintf(str,"%lu minute",cdt);
	sprintf(tmp,text[FileRemovedUserMsg]
		,f.name,cdt ? str : text[No]);
	putsmsg(u,tmp); }
else {
	if(dir[f.dir]->misc&DIR_CDTUL)
		cdt=(ulong)(f.cdt*(dir[f.dir]->up_pct/100.0));
	if(dir[f.dir]->misc&DIR_CDTDL
		&& f.timesdled)  /* all downloads */
		cdt+=(ulong)((long)f.timesdled
			*f.cdt*(dir[f.dir]->dn_pct/100.0));
	adjustuserrec(u,U_CDT,10,-cdt);
	sprintf(tmp,text[FileRemovedUserMsg]
		,f.name,cdt ? ultoac(cdt,str) : text[No]);
    putsmsg(u,tmp); }

adjustuserrec(u,U_ULB,10,-f.size);
adjustuserrec(u,U_ULS,5,-1);
}

/****************************************************************************/
/* Move file 'f' from f.dir to newdir                                       */
/****************************************************************************/
void movefile(file_t f, int newdir)
{
	char str[256],path[256],fname[128],ext[1024];
	int olddir=f.dir;

if(findfile(newdir,f.name)) {
	bprintf(text[FileAlreadyThere],f.name);
	return; }
getextdesc(olddir,f.datoffset,ext);
if(dir[olddir]->misc&DIR_MOVENEW)
	f.dateuled=time(NULL);
unpadfname(f.name,fname);
removefiledat(f);
f.dir=newdir;
addfiledat(&f);
bprintf(text[MovedFile],f.name
	,lib[dir[f.dir]->lib]->sname,dir[f.dir]->sname);
sprintf(str,"Moved %s to %s %s",f.name
	,lib[dir[f.dir]->lib]->sname,dir[f.dir]->sname);
logline(nulstr,str);
if(!f.altpath) {	/* move actual file */
	sprintf(str,"%s%s",dir[olddir]->path,fname);
	if(fexist(str)) {
		sprintf(path,"%s%s",dir[f.dir]->path,fname);
		mv(str,path,0); } }
if(f.misc&FM_EXTDESC)
	putextdesc(f.dir,f.datoffset,ext);
}

/****************************************************************************/
/* Batch flagging prompt for download, extended info, and archive viewing	*/
/* Returns -1 if 'Q' or Ctrl-C, 0 if skip, 1 if [Enter], 2 otherwise        */
/* or 3, backwards. 														*/
/****************************************************************************/
char batchflagprompt(int dirnum, file_t bf[26], char total, long totalfiles)
{
	char ch,c,d,str[256],fname[128],*p,remcdt,remfile;
	int i,j,u,ml,md,udir,ulib;
	long cdt;
	file_t f;

for(ulib=0;ulib<usrlibs;ulib++)
	if(usrlib[ulib]==dir[dirnum]->lib)
		break;
for(udir=0;udir<usrdirs[ulib];udir++)
	if(usrdir[ulib][udir]==dirnum)
		break;

CRLF;
while(online) {
	bprintf(text[BatchFlagPrompt]
		,ulib+1
		,lib[dir[dirnum]->lib]->sname
		,udir+1
		,dir[dirnum]->sname
		,totalfiles);
	ch=getkey(K_UPPER);
	clearline();
	if(ch=='?') {
		menu("BATFLAG");
		if(lncntr)
			pause();
		return(2); }
	if(ch=='Q' || sys_status&SS_ABORT)
		return(-1);
	if(ch=='S')
		return(0);
	if(ch=='P')
		return(3);
	if(ch=='B') {    /* Flag for batch download */
		if(useron.rest&FLAG('D')) {
			bputs(text[R_Download]);
			return(2); }
		if(total==1) {
			f.dir=dirnum;
			strcpy(f.name,bf[0].name);
			f.datoffset=bf[0].datoffset;
			f.size=0;
			getfiledat(&f);
			addtobatdl(f);
			CRLF;
			return(2); }
		bputs(text[BatchDlFlags]);
		d=getstr(str,26,K_UPPER|K_LOWPRIO|K_NOCRLF);
		lncntr=0;
		if(sys_status&SS_ABORT)
			return(-1);
		if(d) { 	/* d is string length */
			CRLF;
			lncntr=0;
			for(c=0;c<d;c++) {
				if(batdn_total>=max_batdn) {
					bprintf(text[BatchDlQueueIsFull],str+c);
					break; }
				if(strchr(str+c,'.')) {     /* filename or spec given */
					f.dir=dirnum;
					p=strchr(str+c,SP);
					if(!p) p=strchr(str+c,',');
					if(p) *p=0;
					for(i=0;i<total;i++) {
						if(batdn_total>=max_batdn) {
							bprintf(text[BatchDlQueueIsFull],str+c);
							break; }
						padfname(str+c,tmp);
						if(filematch(bf[i].name,tmp)) {
							strcpy(f.name,bf[i].name);
							f.datoffset=bf[i].datoffset;
							f.size=0;
							getfiledat(&f);
							addtobatdl(f); } } }
				if(strchr(str+c,'.'))
					c+=strlen(str+c);
				else if(str[c]<'A'+total && str[c]>='A') {
					f.dir=dirnum;
					strcpy(f.name,bf[str[c]-'A'].name);
					f.datoffset=bf[str[c]-'A'].datoffset;
					f.size=0;
					getfiledat(&f);
					addtobatdl(f); } }
			CRLF;
			return(2); }
		clearline();
		continue; }

	if(ch=='E' || ch=='V') {    /* Extended Info */
		if(total==1) {
			f.dir=dirnum;
			strcpy(f.name,bf[0].name);
			f.datoffset=bf[0].datoffset;
			f.dateuled=bf[0].dateuled;
			f.datedled=bf[0].datedled;
			f.size=0;
            getfiledat(&f);
			if(!viewfile(f,ch=='E'))
				return(-1);
			return(2); }
		bputs(text[BatchDlFlags]);
		d=getstr(str,26,K_UPPER|K_LOWPRIO|K_NOCRLF);
		lncntr=0;
		if(sys_status&SS_ABORT)
			return(-1);
		if(d) { 	/* d is string length */
			CRLF;
			lncntr=0;
			for(c=0;c<d;c++) {
				if(strchr(str+c,'.')) {     /* filename or spec given */
					f.dir=dirnum;
					p=strchr(str+c,SP);
					if(!p) p=strchr(str+c,',');
					if(p) *p=0;
					for(i=0;i<total;i++) {
						padfname(str+c,tmp);
						if(filematch(bf[i].name,tmp)) {
							strcpy(f.name,bf[i].name);
							f.datoffset=bf[i].datoffset;
							f.dateuled=bf[i].dateuled;
							f.datedled=bf[i].datedled;
							f.size=0;
							getfiledat(&f);
							if(!viewfile(f,ch=='E'))
								return(-1); } } }
				if(strchr(str+c,'.'))
					c+=strlen(str+c);
				else if(str[c]<'A'+total && str[c]>='A') {
					f.dir=dirnum;
					strcpy(f.name,bf[str[c]-'A'].name);
					f.datoffset=bf[str[c]-'A'].datoffset;
					f.dateuled=bf[str[c]-'A'].dateuled;
					f.datedled=bf[str[c]-'A'].datedled;
					f.size=0;
					getfiledat(&f);
					if(!viewfile(f,ch=='E'))
						return(-1); } }
			return(2); }
		clearline();
		continue; }

	if((ch=='D' || ch=='M')     /* Delete or Move */
		&& !(useron.rest&FLAG('R'))
		&& (dir_op(dirnum) || useron.exempt&FLAG('R'))) {
		if(total==1) {
			strcpy(str,"A");
			d=1; }
		else {
			bputs(text[BatchDlFlags]);
			d=getstr(str,26,K_UPPER|K_LOWPRIO|K_NOCRLF); }
		lncntr=0;
		if(sys_status&SS_ABORT)
			return(-1);
		if(d) { 	/* d is string length */
			CRLF;
			if(ch=='D') {
				if(noyes(text[AreYouSureQ]))
					return(2);
				remcdt=remfile=1;
				if(dir_op(dirnum)) {
					remcdt=!noyes(text[RemoveCreditsQ]);
					remfile=!noyes(text[DeleteFileQ]); } }
			else if(ch=='M') {
				CRLF;
				for(i=0;i<usrlibs;i++)
                    bprintf(text[MoveToLibLstFmt],i+1,lib[usrlib[i]]->lname);
                SYNC;
                bprintf(text[MoveToLibPrompt],dir[dirnum]->lib+1);
				if((ml=getnum(usrlibs))==-1)
					return(2);
				if(!ml)
					ml=dir[dirnum]->lib;
                else
					ml--;
                CRLF;
				for(j=0;j<usrdirs[ml];j++)
                    bprintf(text[MoveToDirLstFmt]
						,j+1,dir[usrdir[ml][j]]->lname);
                SYNC;
				bprintf(text[MoveToDirPrompt],usrdirs[ml]);
				if((md=getnum(usrdirs[ml]))==-1)
					return(2);
				if(!md)
					md=usrdirs[ml]-1;
				else md--;
				CRLF; }
			lncntr=0;
			for(c=0;c<d;c++) {
				if(strchr(str+c,'.')) {     /* filename or spec given */
					f.dir=dirnum;
					p=strchr(str+c,SP);
					if(!p) p=strchr(str+c,',');
					if(p) *p=0;
					for(i=0;i<total;i++) {
						padfname(str+c,tmp);
						if(filematch(bf[i].name,tmp)) {
							strcpy(f.name,bf[i].name);
							unpadfname(f.name,fname);
							f.datoffset=bf[i].datoffset;
							f.dateuled=bf[i].dateuled;
							f.datedled=bf[i].datedled;
							f.size=0;
							getfiledat(&f);
							if(f.opencount) {
								bprintf(text[FileIsOpen]
									,f.opencount,f.opencount>1 ? "s":nulstr);
								continue; }
							if(ch=='D') {
								removefiledat(f);
								if(remfile) {
									sprintf(tmp,"%s%s",dir[f.dir]->path,fname);
									remove(tmp); }
								if(remcdt)
									removefcdt(f); }
							else if(ch=='M')
								movefile(f,usrdir[ml][md]); } } }
				if(strchr(str+c,'.'))
					c+=strlen(str+c);
				else if(str[c]<'A'+total && str[c]>='A') {
					f.dir=dirnum;
					strcpy(f.name,bf[str[c]-'A'].name);
					unpadfname(f.name,fname);
					f.datoffset=bf[str[c]-'A'].datoffset;
					f.dateuled=bf[str[c]-'A'].dateuled;
					f.datedled=bf[str[c]-'A'].datedled;
					f.size=0;
					getfiledat(&f);
					if(f.opencount) {
						bprintf(text[FileIsOpen]
							,f.opencount,f.opencount>1 ? "s":nulstr);
						continue; }
					if(ch=='D') {
						removefiledat(f);
						if(remfile) {
							sprintf(tmp,"%s%s",dir[f.dir]->path,fname);
							remove(tmp); }
						if(remcdt)
							removefcdt(f); }
					else if(ch=='M')
						movefile(f,usrdir[ml][md]); } }
			return(2); }
		clearline();
        continue; }

	return(1); }

return(-1);
}

/*****************************************************************************/
/* List files in directory 'dir' that match 'filespec'. Filespec must be 	 */
/* padded. ex: FILE*   .EXT, not FILE*.EXT. 'mode' determines other critiria */
/* the files must meet before they'll be listed. 'mode' bit FL_NOHDR doesn't */
/* list the directory header.                                                */
/* Returns -1 if the listing was aborted, otherwise total files listed		 */
/*****************************************************************************/
int listfiles(uint dirnum, char *filespec, int tofile, char mode)
{
	uchar str[256],hdr[256],cmd[129],c,d,letter='A',*buf,*p,ext[513];
	uchar HUGE16 *datbuf,HUGE16 *ixbbuf,flagprompt=0;
	int i,j,file,found=0,lastbat=0,disp;
	ulong l,m=0,n,length,anchor,next,datbuflen;
	file_t f,bf[26];	/* bf is batch flagged files */

if(mode&FL_ULTIME) {
	last_ns_time=now;
    sprintf(str,"%s%s.DAB",dir[dirnum]->data_dir,dir[dirnum]->code);
	if((file=nopen(str,O_RDONLY))!=-1) {
		read(file,&l,4);
		close(file);
		if(ns_time>l)
			return(0); } }
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
	return(0); }
close(file);
sprintf(str,"%s%s.DAT",dir[dirnum]->data_dir,dir[dirnum]->code);
if((file=nopen(str,O_RDONLY))==-1) {
	errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
	FREE((char *)ixbbuf);
	return(0); }
datbuflen=filelength(file);
if((datbuf=MALLOC(datbuflen))==NULL) {
	close(file);
	errormsg(WHERE,ERR_ALLOC,str,datbuflen);
	FREE((char *)ixbbuf);
	return(0); }
if(lread(file,datbuf,datbuflen)!=datbuflen) {
	close(file);
	errormsg(WHERE,ERR_READ,str,datbuflen);
	FREE((char *)datbuf);
	FREE((char *)ixbbuf);
	return(0); }
close(file);
if(!tofile) {
	action=NODE_LFIL;
	getnodedat(node_num,&thisnode,0);
	if(thisnode.action!=NODE_LFIL) {	/* was a sync */
		getnodedat(node_num,&thisnode,1);
		thisnode.action=NODE_LFIL;
		putnodedat(node_num,thisnode); } }
while(online && found<MAX_FILES) {
	if(found<0)
		found=0;
	if(m>=l || flagprompt) {		  /* End of list */
		if(useron.misc&BATCHFLAG && !tofile && found && found!=lastbat
			&& !(mode&(FL_EXFIND|FL_VIEW))) {
			flagprompt=0;
			lncntr=0;
			if((i=batchflagprompt(dirnum,bf,letter-'A',l/F_IXBSIZE))==2) {
				m=anchor;
				found-=letter-'A';
				letter='A'; }
			else if(i==3) {
				if((long)anchor-((letter-'A')*F_IXBSIZE)<0) {
					m=0;
					found=0; }
				else {
					m=anchor-((letter-'A')*F_IXBSIZE);
					found-=letter-'A'; }
				letter='A'; }
			else if(i==-1) {
				FREE((char *)ixbbuf);
				FREE((char *)datbuf);
				return(-1); }
			else
				break;
			getnodedat(node_num,&thisnode,0);
			nodesync(); }
		else
			break; }

	if(letter>'Z')
		letter='A';
	if(letter=='A')
		anchor=m;

	if(msgabort()) {		 /* used to be !tofile && msgabort() */
		FREE((char *)ixbbuf);
		FREE((char *)datbuf);
		return(-1); }
	for(j=0;j<12 && m<l;j++)
		if(j==8)
			str[j]='.';
		else
			str[j]=ixbbuf[m++];		/* Turns FILENAMEEXT into FILENAME.EXT */
	str[j]=0;
	if(!(mode&(FL_FINDDESC|FL_EXFIND)) && filespec[0]
		&& !filematch(str,filespec)) {
		m+=11;
		continue; }
	n=ixbbuf[m]|((long)ixbbuf[m+1]<<8)|((long)ixbbuf[m+2]<<16);
	if(n>=datbuflen) {	/* out of bounds */
		m+=11;
		continue; }
	if(mode&(FL_FINDDESC|FL_EXFIND)) {
		getrec((char *)&datbuf[n],F_DESC,LEN_FDESC,tmp);
		strupr(tmp);
		p=strstr(tmp,filespec);
		if(!(mode&FL_EXFIND) && p==NULL) {
			m+=11;
			continue; }
		getrec((char *)&datbuf[n],F_MISC,1,tmp);
		j=tmp[0];  /* misc bits */
		if(j) j-=SP;
		if(mode&FL_EXFIND && j&FM_EXTDESC) { /* search extended description */
			getextdesc(dirnum,n,ext);
			strupr(ext);
			if(!strstr(ext,filespec) && !p) {	/* not in description or */
				m+=11;						 /* extended description */
				continue; } }
		else if(!p) {			 /* no extended description and not in desc */
			m+=11;
			continue; } }
	if(mode&FL_ULTIME) {
		if(ns_time>(ixbbuf[m+3]|((long)ixbbuf[m+4]<<8)|((long)ixbbuf[m+5]<<16)
			|((long)ixbbuf[m+6]<<24))) {
			m+=11;
			continue; } }
	if(useron.misc&BATCHFLAG && letter=='A' && found && !tofile
		&& !(mode&(FL_EXFIND|FL_VIEW))
		&& (!mode || !(useron.misc&EXPERT)))
        bputs(text[FileListBatchCommands]);
	m+=11;
	if(!found && !(mode&(FL_EXFIND|FL_VIEW))) {
		for(i=0;i<usrlibs;i++)
			if(usrlib[i]==dir[dirnum]->lib)
				break;
		for(j=0;j<usrdirs[i];j++)
			if(usrdir[i][j]==dirnum)
				break;						/* big header */
		if((!mode || !(useron.misc&EXPERT)) && !tofile && (!filespec[0]
			|| (strchr(filespec,'*') || strchr(filespec,'?')))) {
			sprintf(hdr,"%s%s.HDR",dir[dirnum]->data_dir,dir[dirnum]->code);
			if(fexist(hdr))
				printfile(hdr,0);	/* Use DATA\DIRS\<CODE>.HDR */
			else {
				if(useron.misc&BATCHFLAG)
					bputs(text[FileListBatchCommands]);
				else {
					CLS;
					d=strlen(lib[usrlib[i]]->lname)>strlen(dir[dirnum]->lname) ?
						strlen(lib[usrlib[i]]->lname)+17
						: strlen(dir[dirnum]->lname)+17;
					if(i>8 || j>8) d++;
					attr(color[clr_filelsthdrbox]);
					bputs("ÉÍ");            /* use to start with \r\n */
					for(c=0;c<d;c++)
						outchar('Í');
					bputs("»\r\nº ");
					sprintf(hdr,text[BoxHdrLib],i+1,lib[usrlib[i]]->lname);
					bputs(hdr);
					for(c=bstrlen(hdr);c<d;c++)
						outchar(SP);
					bputs("º\r\nº ");
					sprintf(hdr,text[BoxHdrDir],j+1,dir[dirnum]->lname);
					bputs(hdr);
					for(c=bstrlen(hdr);c<d;c++)
						outchar(SP);
					bputs("º\r\nº ");
					sprintf(hdr,text[BoxHdrFiles],l/F_IXBSIZE);
					bputs(hdr);
					for(c=bstrlen(hdr);c<d;c++)
						outchar(SP);
					bputs("º\r\nÈÍ");
					for(c=0;c<d;c++)
						outchar('Í');
					bputs("¼\r\n"); } } }
		else {					/* short header */
			if(tofile) {
				sprintf(hdr,"(%u) %s ",i+1,lib[usrlib[i]]->sname);
				write(tofile,crlf,2);
				write(tofile,hdr,strlen(hdr)); }
			else {
				sprintf(hdr,text[ShortHdrLib],i+1,lib[usrlib[i]]->sname);
				bputs("\r\1>\r\n");
				bputs(hdr); }
			c=bstrlen(hdr);
			if(tofile) {
				sprintf(hdr,"(%u) %s",j+1,dir[dirnum]->lname);
				write(tofile,hdr,strlen(hdr)); }
			else {
				sprintf(hdr,text[ShortHdrDir],j+1,dir[dirnum]->lname);
				bputs(hdr); }
			c+=bstrlen(hdr);
			if(tofile) {
				write(tofile,crlf,2);
				sprintf(hdr,"%*s",c,nulstr);
				strset(hdr,'Ä');
				strcat(hdr,crlf);
				write(tofile,hdr,strlen(hdr)); }
			else {
				CRLF;
				attr(color[clr_filelstline]);
				while(c--)
					outchar('Ä');
				CRLF; } } }
	next=m;
	disp=1;
	if(mode&(FL_EXFIND|FL_VIEW)) {
		f.dir=dirnum;
		strcpy(f.name,str);
		m-=11;
		f.datoffset=n;
		f.dateuled=ixbbuf[m+3]|((long)ixbbuf[m+4]<<8)
			|((long)ixbbuf[m+5]<<16)|((long)ixbbuf[m+6]<<24);
		f.datedled=ixbbuf[m+7]|((long)ixbbuf[m+8]<<8)
			|((long)ixbbuf[m+9]<<16)|((long)ixbbuf[m+10]<<24);
		m+=11;
		f.size=0;
		getfiledat(&f);
		if(!found)
			bputs("\r\1>");
		if(mode&FL_EXFIND) {
			if(!viewfile(f,1)) {
				FREE((char *)ixbbuf);
				FREE((char *)datbuf);
				return(-1); } }
		else {
			if(!viewfile(f,0)) {
				FREE((char *)ixbbuf);
				FREE((char *)datbuf);
				return(-1); } } }

	else if(tofile)
		listfiletofile(str,&datbuf[n],dirnum,tofile);
	else if(mode&FL_FINDDESC)
		disp=listfile(str,&datbuf[n],dirnum,filespec,letter,n);
	else
		disp=listfile(str,&datbuf[n],dirnum,nulstr,letter,n);
	if(!disp && letter>'A') {
		next=m-F_IXBSIZE;
		letter--; }
	else {
		disp=1;
		found++; }
	if(sys_status&SS_ABORT) {
		FREE((char *)ixbbuf);
		FREE((char *)datbuf);
		return(-1); }
	if(mode&(FL_EXFIND|FL_VIEW))
		continue;
	if(useron.misc&BATCHFLAG && !tofile) {
		if(disp) {
			strcpy(bf[letter-'A'].name,str);
			m-=11;
			bf[letter-'A'].datoffset=n;
			bf[letter-'A'].dateuled=ixbbuf[m+3]|((long)ixbbuf[m+4]<<8)
				|((long)ixbbuf[m+5]<<16)|((long)ixbbuf[m+6]<<24);
			bf[letter-'A'].datedled=ixbbuf[m+7]|((long)ixbbuf[m+8]<<8)
				|((long)ixbbuf[m+9]<<16)|((long)ixbbuf[m+10]<<24); }
        m+=11;
		if(flagprompt || letter=='Z' || !disp ||
			(filespec[0] && !strchr(filespec,'*') && !strchr(filespec,'?')
			&& !(mode&FL_FINDDESC))
			|| (useron.misc&BATCHFLAG && !tofile && lncntr>=rows-2)
			) {
			flagprompt=0;
			lncntr=0;
			lastbat=found;
			if((i=batchflagprompt(dirnum,bf,letter-'A'+1,l/F_IXBSIZE))<1) {
				FREE((char *)ixbbuf);
				FREE((char *)datbuf);
				if(i==-1)
					return(-1);
				else
					return(found); }
			if(i==2) {
				next=anchor;
				found-=(letter-'A')+1; }
			else if(i==3) {
				if((long)anchor-((letter-'A'+1)*F_IXBSIZE)<0) {
					next=0;
					found=0; }
				else {
					next=anchor-((letter-'A'+1)*F_IXBSIZE);
					found-=letter-'A'+1; } }
			getnodedat(node_num,&thisnode,0);
			nodesync();
			letter='A';	}
		else letter++; }
	if(useron.misc&BATCHFLAG && !tofile
		&& lncntr>=rows-2) {
		lncntr=0;		/* defeat pause() */
		flagprompt=1; }
	m=next;
	if(mode&FL_FINDDESC) continue;
	if(filespec[0] && !strchr(filespec,'*') && !strchr(filespec,'?') && m)
		break; }

FREE((char *)ixbbuf);
FREE((char *)datbuf);
return(found);
}

int cntlines(char *str)
{
	int i,lc,last;

for(i=lc=last=0;str[i];i++)
	if(str[i]==LF || i-last>LEN_FDESC) {
		lc++;
		last=i; }
return(lc);
}

/****************************************************************************/
/* Prints one file's information on a single line                           */
/* Return 1 if displayed, 0 otherwise										*/
/****************************************************************************/
int listfile(char *fname, char HUGE16 *buf, uint dirnum
	, char *search, char letter, ulong datoffset)
{
	char str[256],ext[513]="",*ptr,*cr,*lf,exist=1;
    uchar alt;
    int i,j;
    ulong cdt;

if(buf[F_MISC]!=ETX && (buf[F_MISC]-SP)&FM_EXTDESC && useron.misc&EXTDESC) {
	getextdesc(dirnum,datoffset,ext);
	if(useron.misc&BATCHFLAG && lncntr+cntlines(ext)>=rows-2 && letter!='A')
		return(0); }
attr(color[clr_filename]);
bputs(fname);
if(buf[F_MISC]!=ETX && (buf[F_MISC]-SP)&FM_EXTDESC) {
	if(!(useron.misc&EXTDESC))
		outchar('+');
	else
		outchar(SP); }
else
    outchar(SP);
if(useron.misc&BATCHFLAG) {
    attr(color[clr_filedesc]);
    bprintf("%c",letter); }
getrec((char *)buf,F_ALTPATH,2,str);
alt=(uchar)ahtoul(str);
sprintf(str,"%s%s",alt>0 && alt<=altpaths ? altpath[alt-1]:dir[dirnum]->path
    ,unpadfname(fname,tmp));
if(dir[dirnum]->misc&DIR_FCHK && !fexist(str)) {
    exist=0;
    attr(color[clr_err]); }
else
    attr(color[clr_filecdt]);
getrec((char *)buf,F_CDT,LEN_FCDT,str);
cdt=atol(str);
if(useron.misc&BATCHFLAG) {
    if(!cdt) {
        attr(curatr^(HIGH|BLINK));
        bputs("  FREE"); }
    else {
        if(cdt<1024)    /* 1k is smallest size */
            cdt=1024;
		bprintf("%5luk",cdt/1024L); } }
else {
    if(!cdt) {  /* FREE file */
        attr(curatr^(HIGH|BLINK));
        bputs("   FREE"); }
	else if(cdt>9999999L)
		bprintf("%6luk",cdt/1024L);
	else
        bprintf("%7lu",cdt); }
if(exist)
    outchar(SP);
else
    outchar('-');
getrec((char *)buf,F_DESC,LEN_FDESC,str);
attr(color[clr_filedesc]);
if(!ext[0]) {
	if(search[0]) { /* high-light string in string */
		strcpy(tmp,str);
		strupr(tmp);
		ptr=strstr(tmp,search);
		i=strlen(search);
		j=ptr-tmp;
		bprintf("%.*s",j,str);
		attr(color[clr_filedesc]^HIGH);
		bprintf("%.*s",i,str+j);
		attr(color[clr_filedesc]);
		bprintf("%.*s",strlen(str)-(j+i),str+j+i); }
	else
		bputs(str);
	CRLF; }
ptr=ext;
while(*ptr && ptr<ext+512 && !msgabort()) {
	cr=strchr(ptr,CR);
	lf=strchr(ptr,LF);
	if(lf && (lf<cr || !cr)) cr=lf;
	if(cr>ptr+LEN_FDESC)
		cr=ptr+LEN_FDESC;
	else if(cr)
		*cr=0;
//	  bprintf("%.*s\r\n",LEN_FDESC,ptr);
	sprintf(str,"%.*s\r\n",LEN_FDESC,ptr);
	putmsg(str,P_NOATCODES|P_SAVEATR);
	if(!cr) {
		if(strlen(ptr)>LEN_FDESC)
			cr=ptr+LEN_FDESC;
		else
			break; }
	if(!(*(cr+1)) || !(*(cr+2)))
		break;
	bprintf("%21s",nulstr);
	ptr=cr;
	if(!(*ptr)) ptr++;
	while(*ptr==LF || *ptr==CR) ptr++; }
return(1);
}
