/* listfile.cpp */

/* Synchronet file database listing functions */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "sbbs.h"

#define BF_MAX	26	/* Batch Flag max: A-Z */	

int extdesclines(char *str);

/*****************************************************************************/
/* List files in directory 'dir' that match 'filespec'. Filespec must be 	 */
/* padded. ex: FILE*   .EXT, not FILE*.EXT. 'mode' determines other critiria */
/* the files must meet before they'll be listed. 'mode' bit FL_NOHDR doesn't */
/* list the directory header.                                                */
/* Returns -1 if the listing was aborted, otherwise total files listed		 */
/*****************************************************************************/
int sbbs_t::listfiles(uint dirnum, char *filespec, int tofile, long mode)
{
	char	str[256],hdr[256],c,d,letter='A',*p,*datbuf,ext[513];
	char 	tmp[512];
	uchar*	ixbbuf;
	uchar	flagprompt=0;
	uint	i,j;
	int		file,found=0,lastbat=0,disp;
	long	l,m=0,n,anchor=0,next,datbuflen;
	file_t	f,bf[26];	/* bf is batch flagged files */

	if(mode&FL_ULTIME) {
		last_ns_time=now;
		sprintf(str,"%s%s.dab",cfg.dir[dirnum]->data_dir,cfg.dir[dirnum]->code);
		if((file=nopen(str,O_RDONLY))!=-1) {
			read(file,&l,4);
			close(file);
			if(ns_time>(time_t)l)
				return(0); } }
	sprintf(str,"%s%s.ixb",cfg.dir[dirnum]->data_dir,cfg.dir[dirnum]->code);
	if((file=nopen(str,O_RDONLY))==-1)
		return(0);
	l=filelength(file);
	if(!l) {
		close(file);
		return(0); }
	if((ixbbuf=(uchar *)MALLOC(l))==NULL) {
		close(file);
		errormsg(WHERE,ERR_ALLOC,str,l);
		return(0); }
	if(lread(file,ixbbuf,l)!=l) {
		close(file);
		errormsg(WHERE,ERR_READ,str,l);
		FREE((char *)ixbbuf);
		return(0); }
	close(file);
	sprintf(str,"%s%s.dat",cfg.dir[dirnum]->data_dir,cfg.dir[dirnum]->code);
	if((file=nopen(str,O_RDONLY))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
		FREE((char *)ixbbuf);
		return(0); }
	datbuflen=filelength(file);
	if((datbuf=(char *)MALLOC(datbuflen))==NULL) {
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
		getnodedat(cfg.node_num,&thisnode,0);
		if(thisnode.action!=NODE_LFIL) {	/* was a sync */
			if(getnodedat(cfg.node_num,&thisnode,true)==0) {
				thisnode.action=NODE_LFIL;
				putnodedat(cfg.node_num,&thisnode); 
			} 
		} 
	}

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
				else if((int)i==-1) {
					FREE((char *)ixbbuf);
					FREE((char *)datbuf);
					return(-1); }
				else
					break;
				getnodedat(cfg.node_num,&thisnode,0);
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
				str[j]=ixbbuf[m]>' ' ? '.' : ' ';
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
				getextdesc(&cfg,dirnum,n,ext);
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
				if(usrlib[i]==cfg.dir[dirnum]->lib)
					break;
			for(j=0;j<usrdirs[i];j++)
				if(usrdir[i][j]==dirnum)
					break;						/* big header */
			if((!mode || !(useron.misc&EXPERT)) && !tofile && (!filespec[0]
				|| (strchr(filespec,'*') || strchr(filespec,'?')))) {
				sprintf(hdr,"%s%s.hdr",cfg.dir[dirnum]->data_dir,cfg.dir[dirnum]->code);
				if(fexistcase(hdr))
					printfile(hdr,0);	/* Use DATA\DIRS\<CODE>.HDR */
				else {
					if(useron.misc&BATCHFLAG)
						bputs(text[FileListBatchCommands]);
					else {
						CLS;
						d=strlen(cfg.lib[usrlib[i]]->lname)>strlen(cfg.dir[dirnum]->lname) ?
							strlen(cfg.lib[usrlib[i]]->lname)+17
							: strlen(cfg.dir[dirnum]->lname)+17;
						if(i>8 || j>8) d++;
						attr(cfg.color[clr_filelsthdrbox]);
						bputs("ÉÍ");            /* use to start with \r\n */
						for(c=0;c<d;c++)
							outchar('Í');
						bputs("»\r\nº ");
						sprintf(hdr,text[BoxHdrLib],i+1,cfg.lib[usrlib[i]]->lname);
						bputs(hdr);
						for(c=bstrlen(hdr);c<d;c++)
							outchar(SP);
						bputs("º\r\nº ");
						sprintf(hdr,text[BoxHdrDir],j+1,cfg.dir[dirnum]->lname);
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
					sprintf(hdr,"(%u) %s ",i+1,cfg.lib[usrlib[i]]->sname);
					write(tofile,crlf,2);
					write(tofile,hdr,strlen(hdr)); }
				else {
					sprintf(hdr,text[ShortHdrLib],i+1,cfg.lib[usrlib[i]]->sname);
					bputs("\r\1>\r\n");
					bputs(hdr); }
				c=bstrlen(hdr);
				if(tofile) {
					sprintf(hdr,"(%u) %s",j+1,cfg.dir[dirnum]->lname);
					write(tofile,hdr,strlen(hdr)); }
				else {
					sprintf(hdr,text[ShortHdrDir],j+1,cfg.dir[dirnum]->lname);
					bputs(hdr); }
				c+=bstrlen(hdr);
				if(tofile) {
					write(tofile,crlf,2);
					sprintf(hdr,"%*s",c,nulstr);
					memset(hdr,'Ä',c);
					strcat(hdr,crlf);
					write(tofile,hdr,strlen(hdr)); }
				else {
					CRLF;
					attr(cfg.color[clr_filelstline]);
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
			getfiledat(&cfg,&f);
			if(!found)
				bputs("\r\1>");
			if(mode&FL_EXFIND) {
				if(!viewfile(&f,1)) {
					FREE((char *)ixbbuf);
					FREE((char *)datbuf);
					return(-1); } }
			else {
				if(!viewfile(&f,0)) {
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
				if((int)(i=batchflagprompt(dirnum,bf,letter-'A'+1,l/F_IXBSIZE))<1) {
					FREE((char *)ixbbuf);
					FREE((char *)datbuf);
					if((int)i==-1)
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
				getnodedat(cfg.node_num,&thisnode,0);
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

/****************************************************************************/
/* Prints one file's information on a single line                           */
/* Return 1 if displayed, 0 otherwise										*/
/****************************************************************************/
bool sbbs_t::listfile(char *fname, char HUGE16 *buf, uint dirnum
	, char *search, char letter, ulong datoffset)
{
	char	str[256],ext[513]="",*ptr,*cr,*lf,exist=1;
	char	path[MAX_PATH+1];
	char 	tmp[512];
    uchar	alt;
    int		i,j;
    ulong	cdt;

	if(buf[F_MISC]!=ETX && (buf[F_MISC]-SP)&FM_EXTDESC && useron.misc&EXTDESC) {
		getextdesc(&cfg,dirnum,datoffset,ext);
		if(useron.misc&BATCHFLAG && lncntr+extdesclines(ext)>=rows-2 && letter!='A')
			return(false); }

	attr(cfg.color[clr_filename]);
	bputs(fname);

	getrec((char *)buf,F_ALTPATH,2,str);
	alt=(uchar)ahtoul(str);
	sprintf(path,"%s%s",alt>0 && alt<=cfg.altpaths ? cfg.altpath[alt-1]:cfg.dir[dirnum]->path
		,unpadfname(fname,tmp));

	if(buf[F_MISC]!=ETX && (buf[F_MISC]-SP)&FM_EXTDESC) {
		if(!(useron.misc&EXTDESC))
			outchar('+');
		else
			outchar(SP); }
	else
		outchar(SP);
	if(useron.misc&BATCHFLAG) {
		attr(cfg.color[clr_filedesc]);
		bprintf("%c",letter); }
	if(cfg.dir[dirnum]->misc&DIR_FCHK && !fexistcase(path)) {
		exist=0;
		attr(cfg.color[clr_err]); }
	else
		attr(cfg.color[clr_filecdt]);
	getrec((char *)buf,F_CDT,LEN_FCDT,str);
	cdt=atol(str);
	if(useron.misc&BATCHFLAG) {
		if(!cdt) {
			attr(curatr^(HIGH|BLINK));
			bputs("  FREE"); }
		else {
			if(cdt<1024)    /* 1k is smallest size */
				cdt=1024;
			if(cdt>(99999*1024))
				bprintf("%5luM",cdt/(1024*1024));
			else
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
	attr(cfg.color[clr_filedesc]);

#ifdef _WIN32
 
	if(exist && !(cfg.file_misc&FM_NO_LFN)) {

		WIN32_FIND_DATA finddata;
		HANDLE h=FindFirstFile(path,&finddata);

		if(h!=INVALID_HANDLE_VALUE) {

			if(stricmp(finddata.cFileName,fname) && stricmp(finddata.cFileName,str))
				bprintf("%.*s\r\n%21s",LEN_FDESC,finddata.cFileName,"");

			FindClose(h);
		}
	}

#endif
	if(!ext[0]) {
		if(search[0]) { /* high-light string in string */
			strcpy(tmp,str);
			strupr(tmp);
			ptr=strstr(tmp,search);
			i=strlen(search);
			j=ptr-tmp;
			bprintf("%.*s",j,str);
			attr(cfg.color[clr_filedesc]^HIGH);
			bprintf("%.*s",i,str+j);
			attr(cfg.color[clr_filedesc]);
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
	return(true);
}

/****************************************************************************/
/* Remove credits from uploader of file 'f'                                 */
/****************************************************************************/
bool sbbs_t::removefcdt(file_t* f)
{
	char	str[128];
	char 	tmp[512];
	int		u;
	long	cdt;

	if((u=matchuser(&cfg,f->uler,TRUE /*sysop_alias*/))==0) {
	   bputs(text[UnknownUser]);
	   return(false); }
	cdt=0L;
	if(cfg.dir[f->dir]->misc&DIR_CDTMIN && cur_cps) {
		if(cfg.dir[f->dir]->misc&DIR_CDTUL)
			cdt=((ulong)(f->cdt*(cfg.dir[f->dir]->up_pct/100.0))/cur_cps)/60;
		if(cfg.dir[f->dir]->misc&DIR_CDTDL
			&& f->timesdled)  /* all downloads */
			cdt+=((ulong)((long)f->timesdled
				*f->cdt*(cfg.dir[f->dir]->dn_pct/100.0))/cur_cps)/60;
		adjustuserrec(&cfg,u,U_MIN,10,-cdt);
		sprintf(str,"%lu minute",cdt);
		sprintf(tmp,text[FileRemovedUserMsg]
			,f->name,cdt ? str : text[No]);
		putsmsg(&cfg,u,tmp); }
	else {
		if(cfg.dir[f->dir]->misc&DIR_CDTUL)
			cdt=(ulong)(f->cdt*(cfg.dir[f->dir]->up_pct/100.0));
		if(cfg.dir[f->dir]->misc&DIR_CDTDL
			&& f->timesdled)  /* all downloads */
			cdt+=(ulong)((long)f->timesdled
				*f->cdt*(cfg.dir[f->dir]->dn_pct/100.0));
		adjustuserrec(&cfg,u,U_CDT,10,-cdt);
		sprintf(tmp,text[FileRemovedUserMsg]
			,f->name,cdt ? ultoac(cdt,str) : text[No]);
		putsmsg(&cfg,u,tmp); }

	adjustuserrec(&cfg,u,U_ULB,10,-f->size);
	adjustuserrec(&cfg,u,U_ULS,5,-1);
	return(true);
}

/****************************************************************************/
/* Move file 'f' from f.dir to newdir                                       */
/****************************************************************************/
bool sbbs_t::movefile(file_t* f, int newdir)
{
	char str[256],path[256],fname[128],ext[1024];
	int olddir=f->dir;

	if(findfile(&cfg,newdir,f->name)) {
		bprintf(text[FileAlreadyThere],f->name);
		return(false); }
	getextdesc(&cfg,olddir,f->datoffset,ext);
	if(cfg.dir[olddir]->misc&DIR_MOVENEW)
		f->dateuled=time(NULL);
	unpadfname(f->name,fname);
	removefiledat(&cfg,f);
	f->dir=newdir;
	addfiledat(&cfg,f);
	bprintf(text[MovedFile],f->name
		,cfg.lib[cfg.dir[f->dir]->lib]->sname,cfg.dir[f->dir]->sname);
	sprintf(str,"%s moved %s to %s %s",f->name
		,useron.alias
		,cfg.lib[cfg.dir[f->dir]->lib]->sname,cfg.dir[f->dir]->sname);
	logline(nulstr,str);
	if(!f->altpath) {	/* move actual file */
		sprintf(str,"%s%s",cfg.dir[olddir]->path,fname);
		if(fexistcase(str)) {
			sprintf(path,"%s%s",cfg.dir[f->dir]->path,fname);
			mv(str,path,0); } }
	if(f->misc&FM_EXTDESC)
		putextdesc(&cfg,f->dir,f->datoffset,ext);
	return(true);
}

/****************************************************************************/
/* Batch flagging prompt for download, extended info, and archive viewing	*/
/* Returns -1 if 'Q' or Ctrl-C, 0 if skip, 1 if [Enter], 2 otherwise        */
/* or 3, backwards. 														*/
/****************************************************************************/
int sbbs_t::batchflagprompt(uint dirnum, file_t* bf, uint total
							,long totalfiles)
{
	char	ch,c,d,str[256],fname[128],*p,remcdt=0,remfile=0;
	char 	tmp[512];
	uint	i,j,ml=0,md=0,udir,ulib;
	file_t	f;

	for(ulib=0;ulib<usrlibs;ulib++)
		if(usrlib[ulib]==cfg.dir[dirnum]->lib)
			break;
	for(udir=0;udir<usrdirs[ulib];udir++)
		if(usrdir[ulib][udir]==dirnum)
			break;

	CRLF;
	while(online) {
		bprintf(text[BatchFlagPrompt]
			,ulib+1
			,cfg.lib[cfg.dir[dirnum]->lib]->sname
			,udir+1
			,cfg.dir[dirnum]->sname
			,totalfiles);
		ch=getkey(K_UPPER);
		clearline();
		if(ch=='?') {
			menu("batflag");
			if(lncntr)
				pause();
			return(2); }
		if(ch=='Q' || sys_status&SS_ABORT)
			return(-1);
		if(ch=='S')
			return(0);
		if(ch=='P' || ch=='-')
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
				getfiledat(&cfg,&f);
				addtobatdl(&f);
				CRLF;
				return(2); }
			bputs(text[BatchDlFlags]);
			d=getstr(str,BF_MAX,K_UPPER|K_LOWPRIO|K_NOCRLF);
			lncntr=0;
			if(sys_status&SS_ABORT)
				return(-1);
			if(d) { 	/* d is string length */
				CRLF;
				lncntr=0;
				for(c=0;c<d;c++) {
					if(batdn_total>=cfg.max_batdn) {
						bprintf(text[BatchDlQueueIsFull],str+c);
						break; }
					if(strchr(str+c,'.')) {     /* filename or spec given */
						f.dir=dirnum;
						p=strchr(str+c,SP);
						if(!p) p=strchr(str+c,',');
						if(p) *p=0;
						for(i=0;i<total;i++) {
							if(batdn_total>=cfg.max_batdn) {
								bprintf(text[BatchDlQueueIsFull],str+c);
								break; }
							padfname(str+c,tmp);
							if(filematch(bf[i].name,tmp)) {
								strcpy(f.name,bf[i].name);
								f.datoffset=bf[i].datoffset;
								f.size=0;
								getfiledat(&cfg,&f);
								addtobatdl(&f); } } }
					if(strchr(str+c,'.'))
						c+=strlen(str+c);
					else if(str[c]<'A'+(char)total && str[c]>='A') {
						f.dir=dirnum;
						strcpy(f.name,bf[str[c]-'A'].name);
						f.datoffset=bf[str[c]-'A'].datoffset;
						f.size=0;
						getfiledat(&cfg,&f);
						addtobatdl(&f); } }
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
				getfiledat(&cfg,&f);
				if(!viewfile(&f,ch=='E'))
					return(-1);
				return(2); }
			bputs(text[BatchDlFlags]);
			d=getstr(str,BF_MAX,K_UPPER|K_LOWPRIO|K_NOCRLF);
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
								getfiledat(&cfg,&f);
								if(!viewfile(&f,ch=='E'))
									return(-1); } } }
					if(strchr(str+c,'.'))
						c+=strlen(str+c);
					else if(str[c]<'A'+(char)total && str[c]>='A') {
						f.dir=dirnum;
						strcpy(f.name,bf[str[c]-'A'].name);
						f.datoffset=bf[str[c]-'A'].datoffset;
						f.dateuled=bf[str[c]-'A'].dateuled;
						f.datedled=bf[str[c]-'A'].datedled;
						f.size=0;
						getfiledat(&cfg,&f);
						if(!viewfile(&f,ch=='E'))
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
				d=getstr(str,BF_MAX,K_UPPER|K_LOWPRIO|K_NOCRLF); }
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
						bprintf(text[MoveToLibLstFmt],i+1,cfg.lib[usrlib[i]]->lname);
					SYNC;
					bprintf(text[MoveToLibPrompt],cfg.dir[dirnum]->lib+1);
					if((int)(ml=getnum(usrlibs))==-1)
						return(2);
					if(!ml)
						ml=cfg.dir[dirnum]->lib;
					else
						ml--;
					CRLF;
					for(j=0;j<usrdirs[ml];j++)
						bprintf(text[MoveToDirLstFmt]
							,j+1,cfg.dir[usrdir[ml][j]]->lname);
					SYNC;
					bprintf(text[MoveToDirPrompt],usrdirs[ml]);
					if((int)(md=getnum(usrdirs[ml]))==-1)
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
								getfiledat(&cfg,&f);
								if(f.opencount) {
									bprintf(text[FileIsOpen]
										,f.opencount,f.opencount>1 ? "s":nulstr);
									continue; }
								if(ch=='D') {
									removefiledat(&cfg,&f);
									if(remfile) {
										sprintf(tmp,"%s%s",cfg.dir[f.dir]->path,fname);
										remove(tmp); }
									if(remcdt)
										removefcdt(&f); }
								else if(ch=='M')
									movefile(&f,usrdir[ml][md]); } } }
					if(strchr(str+c,'.'))
						c+=strlen(str+c);
					else if(str[c]<'A'+(char)total && str[c]>='A') {
						f.dir=dirnum;
						strcpy(f.name,bf[str[c]-'A'].name);
						unpadfname(f.name,fname);
						f.datoffset=bf[str[c]-'A'].datoffset;
						f.dateuled=bf[str[c]-'A'].dateuled;
						f.datedled=bf[str[c]-'A'].datedled;
						f.size=0;
						getfiledat(&cfg,&f);
						if(f.opencount) {
							bprintf(text[FileIsOpen]
								,f.opencount,f.opencount>1 ? "s":nulstr);
							continue; }
						if(ch=='D') {
							removefiledat(&cfg,&f);
							if(remfile) {
								sprintf(tmp,"%s%s",cfg.dir[f.dir]->path,fname);
								remove(tmp); }
							if(remcdt)
								removefcdt(&f); }
						else if(ch=='M')
							movefile(&f,usrdir[ml][md]); } }
				return(2); }
			clearline();
			continue; }

		return(1); }

	return(-1);
}

/****************************************************************************/
/* List detailed information about the files in 'filespec'. Prompts for     */
/* action depending on 'mode.'                                              */
/* Returns number of files matching filespec that were found                */
/****************************************************************************/
int sbbs_t::listfileinfo(uint dirnum, char *filespec, long mode)
{
	char	str[258],path[258],dirpath[256],done=0,ch,fname[13],ext[513];
	char 	tmp[512];
	uchar	*ixbbuf,*usrxfrbuf=NULL,*p;
	int		file;
	int		error;
	int		found=0;
    uint	i,j;
	long	usrxfrlen=0;
    long	m,l;
	long	usrcdt;
    time_t	start,end,t;
    file_t	f;
	struct	tm tm;

	sprintf(str,"%sxfer.ixt",cfg.data_dir);
	if(mode==FI_USERXFER) {
		if(flength(str)<1L)
			return(0);
		if((file=nopen(str,O_RDONLY))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
			return(0); }
		usrxfrlen=filelength(file);
		if((usrxfrbuf=(uchar *)MALLOC(usrxfrlen))==NULL) {
			close(file);
			errormsg(WHERE,ERR_ALLOC,str,usrxfrlen);
			return(0); }
		if(read(file,usrxfrbuf,usrxfrlen)!=usrxfrlen) {
			close(file);
			FREE(usrxfrbuf);
			errormsg(WHERE,ERR_READ,str,usrxfrlen);
			return(0); }
		close(file); }
	sprintf(str,"%s%s.ixb",cfg.dir[dirnum]->data_dir,cfg.dir[dirnum]->code);
	if((file=nopen(str,O_RDONLY))==-1)
		return(0);
	l=filelength(file);
	if(!l) {
		close(file);
		return(0); }
	if((ixbbuf=(uchar *)MALLOC(l))==NULL) {
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
	sprintf(str,"%s%s.dat",cfg.dir[dirnum]->data_dir,cfg.dir[dirnum]->code);
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
				str[i]=ixbbuf[m]>' ' ? '.' : ' ';
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
		getfiledat(&cfg,&f);
		if(mode==FI_OFFLINE && f.size>=0)
			continue;
		if(f.altpath>0 && f.altpath<=cfg.altpaths)
			strcpy(dirpath,cfg.altpath[f.altpath-1]);
		else
			strcpy(dirpath,cfg.dir[f.dir]->path);
		if(mode==FI_CLOSE && !f.opencount)
			continue;
		if(mode==FI_USERXFER) {
			for(p=usrxfrbuf;p<usrxfrbuf+usrxfrlen;p+=24) {
				sprintf(str,"%17.17s",p);   /* %4.4u %12.12s */
				if(!strcmp(str+5,f.name) && useron.number==atoi(str))
					break; }
			if(p>=usrxfrbuf+usrxfrlen) /* file wasn't found */
				continue; }
		if((mode==FI_REMOVE) && (!dir_op(dirnum) && stricmp(f.uler
			,useron.alias) && !(useron.exempt&FLAG('R'))))
			continue;
		found++;
		if(mode==FI_INFO) {
			if(!viewfile(&f,1)) {
				done=1;
				found=-1; } }
		else
			fileinfo(&f);
		if(mode==FI_CLOSE) {
			if(!noyes(text[CloseFileRecordQ])) {
				f.opencount=0;
				putfiledat(&cfg,&f); } }
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
					viewfilecontents(&f);
					CRLF;
					ASYNC;
					pause();
					m-=F_IXBSIZE;
					continue;
				case 'E':   /* edit file information */
					if(dir_op(dirnum)) {
						bputs(text[EditFilename]);
						strcpy(str,fname);
						if(!getstr(str,12,K_EDIT|K_AUTODEL))
							break;
						if(strcmp(str,fname)) { /* rename */
							padfname(str,path);
							if(stricmp(str,fname)
								&& findfile(&cfg,f.dir,path))
								bprintf(text[FileAlreadyThere],path);
							else {
								sprintf(path,"%s%s",dirpath,fname);
								sprintf(tmp,"%s%s",dirpath,str);
								if(rename(path,tmp))
									bprintf(text[CouldntRenameFile],path,tmp);
								else {
									bprintf(text[FileRenamed],path,tmp);
									strcpy(fname,str);
									removefiledat(&cfg,&f);
									strcpy(f.name,padfname(str,tmp));
									addfiledat(&cfg,&f); 
								} 
							} 
						} 
					}
					bputs(text[EditDescription]);
					getstr(f.desc,LEN_FDESC,K_LINE|K_EDIT|K_AUTODEL);
					if(sys_status&SS_ABORT)
						break;
					if(f.misc&FM_EXTDESC) {
						if(!noyes(text[DeleteExtDescriptionQ])) {
							remove(str);
							f.misc&=~FM_EXTDESC; } }
					if(!dir_op(dirnum)) {
						putfiledat(&cfg,&f);
						break; }
					bputs(text[EditUploader]);
					if(!getstr(f.uler,LEN_ALIAS,K_EDIT|K_AUTODEL))
						break;
					ultoa(f.cdt,str,10);
					bputs(text[EditCreditValue]);
					getstr(str,7,K_NUMBER|K_EDIT|K_AUTODEL);
					if(sys_status&SS_ABORT)
						break;
					f.cdt=atol(str);
					ultoa(f.timesdled,str,10);
					bputs(text[EditTimesDownloaded]);
					getstr(str,5,K_NUMBER|K_EDIT|K_AUTODEL);
					if(sys_status&SS_ABORT)
						break;
					f.timesdled=atoi(str);
					if(f.opencount) {
						ultoa(f.opencount,str,10);
						bputs(text[EditOpenCount]);
						getstr(str,3,K_NUMBER|K_EDIT|K_AUTODEL);
						f.opencount=atoi(str); }
					if(cfg.altpaths || f.altpath) {
						ultoa(f.altpath,str,10);
						bputs(text[EditAltPath]);
						getstr(str,3,K_NUMBER|K_EDIT|K_AUTODEL);
						f.altpath=atoi(str);
						if(f.altpath>cfg.altpaths)
							f.altpath=0; }
					if(sys_status&SS_ABORT)
						break;
					putfiledat(&cfg,&f);
					inputnstime(&f.dateuled);
					update_uldate(&cfg, &f);
					break;
				case 'F':   /* delete file only */
					sprintf(str,"%s%s",dirpath,fname);
					if(!fexistcase(str))
						bputs(text[FileNotThere]);
					else {
						if(!noyes(text[AreYouSureQ])) {
							if(remove(str))
								bprintf(text[CouldntRemoveFile],str);
							else {
								sprintf(tmp,"%s deleted %s"
									,useron.alias
									,str);
								logline(nulstr,tmp); 
							} 
						} 
					}
					break;
				case 'R':   /* remove file from database */
					if(noyes(text[AreYouSureQ]))
						break;
					removefiledat(&cfg,&f);
					sprintf(str,"%s removed %s from %s %s"
						,useron.alias
						,f.name
						,cfg.lib[cfg.dir[f.dir]->lib]->sname,cfg.dir[f.dir]->sname);
					logline("U-",str);
					sprintf(str,"%s%s",dirpath,fname);
					if(fexistcase(str)) {
						if(dir_op(dirnum)) {
							if(!noyes(text[DeleteFileQ])) {
								if(remove(str))
									bprintf(text[CouldntRemoveFile],str);
								else {
									sprintf(tmp,"%s deleted %s"
										,useron.alias
										,str);
									logline(nulstr,tmp); 
								} 
							} 
						}
						else if(remove(str))    /* always remove if not sysop */
							bprintf(text[CouldntRemoveFile],str); }
					if(dir_op(dirnum) || useron.exempt&FLAG('R')) {
						i=cfg.lib[cfg.dir[f.dir]->lib]->offline_dir;
						if(i!=dirnum && i!=INVALID_DIR
							&& !findfile(&cfg,i,f.name)) {
							sprintf(str,text[AddToOfflineDirQ]
								,fname,cfg.lib[cfg.dir[i]->lib]->sname,cfg.dir[i]->sname);
							if(yesno(str)) {
								getextdesc(&cfg,f.dir,f.datoffset,ext);
								f.dir=i;
								addfiledat(&cfg,&f);
								if(f.misc&FM_EXTDESC)
									putextdesc(&cfg,f.dir,f.datoffset,ext); } } }
					if(dir_op(dirnum) || stricmp(f.uler,useron.alias)) {
						if(noyes(text[RemoveCreditsQ]))
	/* Fall through */      break; }
				case 'C':   /* remove credits only */
					if((i=matchuser(&cfg,f.uler,TRUE /*sysop_alias*/))==0) {
						bputs(text[UnknownUser]);
						break; }
					if(dir_op(dirnum)) {
						usrcdt=(ulong)(f.cdt*(cfg.dir[f.dir]->up_pct/100.0));
						if(f.timesdled)     /* all downloads */
							usrcdt+=(ulong)((long)f.timesdled
								*f.cdt*(cfg.dir[f.dir]->dn_pct/100.0));
						ultoa(usrcdt,str,10);
						bputs(text[CreditsToRemove]);
						getstr(str,10,K_NUMBER|K_LINE|K_EDIT|K_AUTODEL);
						f.cdt=atol(str); }
					usrcdt=adjustuserrec(&cfg,i,U_CDT,10,-(long)f.cdt);
					if(i==useron.number)
						useron.cdt=usrcdt;
					sprintf(str,text[FileRemovedUserMsg]
						,f.name,f.cdt ? ultoac(f.cdt,tmp) : text[No]);
					putsmsg(&cfg,i,str);
					usrcdt=adjustuserrec(&cfg,i,U_ULB,10,-f.size);
					if(i==useron.number)
						useron.ulb=usrcdt;
					usrcdt=adjustuserrec(&cfg,i,U_ULS,5,-1);
					if(i==useron.number)
						useron.uls=(ushort)usrcdt;
					break;
				case 'M':   /* move the file to another dir */
					CRLF;
					for(i=0;i<usrlibs;i++)
						bprintf(text[MoveToLibLstFmt],i+1,cfg.lib[usrlib[i]]->lname);
					SYNC;
					bprintf(text[MoveToLibPrompt],cfg.dir[dirnum]->lib+1);
					if((int)(i=getnum(usrlibs))==-1)
						continue;
					if(!i)
						i=cfg.dir[dirnum]->lib;
					else
						i--;
					CRLF;
					for(j=0;j<usrdirs[i];j++)
						bprintf(text[MoveToDirLstFmt]
							,j+1,cfg.dir[usrdir[i][j]]->lname);
					SYNC;
					bprintf(text[MoveToDirPrompt],usrdirs[i]);
					if((int)(j=getnum(usrdirs[i]))==-1)
						continue;
					if(!j)
						j=usrdirs[i]-1;
					else j--;
					CRLF;
					if(findfile(&cfg,usrdir[i][j],f.name)) {
						bprintf(text[FileAlreadyThere],f.name);
						break; }
					getextdesc(&cfg,f.dir,f.datoffset,ext);
					removefiledat(&cfg,&f);
					if(f.dir==cfg.upload_dir || f.dir==cfg.sysop_dir)
						f.dateuled=time(NULL);
					f.dir=usrdir[i][j];
					addfiledat(&cfg,&f);
					bprintf(text[MovedFile],f.name
						,cfg.lib[cfg.dir[f.dir]->lib]->sname,cfg.dir[f.dir]->sname);
					sprintf(str,"%s moved %s to %s %s"
						,useron.alias
						,f.name
						,cfg.lib[cfg.dir[f.dir]->lib]->sname,cfg.dir[f.dir]->sname);
					logline(nulstr,str);
					if(!f.altpath) {    /* move actual file */
						sprintf(str,"%s%s",cfg.dir[dirnum]->path,fname);
						if(fexistcase(str)) {
							sprintf(path,"%s%s",cfg.dir[f.dir]->path,fname);
							mv(str,path,0); } }
					if(f.misc&FM_EXTDESC)
						putextdesc(&cfg,f.dir,f.datoffset,ext);
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
			if(!is_download_free(&cfg,f.dir,&useron)
				&& f.cdt>(useron.cdt+useron.freecdt)) {
				SYNC;
				bprintf(text[YouOnlyHaveNCredits]
					,ultoac(useron.cdt+useron.freecdt,tmp));
				mnemonics(text[QuitOrNext]);
				if(getkeys("\rQ",0)=='Q') {
					found=-1;
					break; }
				continue; }
			if(!chk_ar(cfg.dir[f.dir]->dl_ar,&useron)) {
				SYNC;
				bputs(text[CantDownloadFromDir]);
				mnemonics(text[QuitOrNext]);
				if(getkeys("\rQ",0)=='Q') {
					found=-1;
					break; }
				continue; }
			if(!(cfg.dir[f.dir]->misc&DIR_TFREE) && f.timetodl>timeleft && !dir_op(dirnum)
				&& !(useron.exempt&FLAG('T'))) {
				SYNC;
				bputs(text[NotEnoughTimeToDl]);
				mnemonics(text[QuitOrNext]);
				if(getkeys("\rQ",0)=='Q') {
					found=-1;
					break; }
				continue; }
			menu("dlprot");
			openfile(&f);
			SYNC;
			mnemonics(text[ProtocolBatchQuitOrNext]);
			strcpy(str,"BQ\r");
			for(i=0;i<cfg.total_prots;i++)
				if(cfg.prot[i]->dlcmd[0]
					&& chk_ar(cfg.prot[i]->ar,&useron)) {
					sprintf(tmp,"%c",cfg.prot[i]->mnemonic);
					strcat(str,tmp); }
	//		  ungetkey(useron.prot);
			ch=(char)getkeys(str,0);
			if(ch=='Q') {
				found=-1;
				done=1; }
			else if(ch=='B') {
				if(!addtobatdl(&f)) {
					closefile(&f);
					break; } }
			else if(ch!=CR) {
				for(i=0;i<cfg.total_prots;i++)
					if(cfg.prot[i]->dlcmd[0] && cfg.prot[i]->mnemonic==ch
						&& chk_ar(cfg.prot[i]->ar,&useron))
						break;
				if(i<cfg.total_prots) {
					if(online==ON_LOCAL) {
						bputs(text[EnterPath]);
						if(getstr(path,60,K_LINE)) {
							backslash(path);
							strcat(path,fname);
							sprintf(str,"%s%s",dirpath,fname);
							if(!mv(str,path,1))
								downloadfile(&f);
							for(j=0;j<cfg.total_dlevents;j++)
								if(!stricmp(cfg.dlevent[j]->ext,f.name+9)
									&& chk_ar(cfg.dlevent[j]->ar,&useron)) {
									bputs(cfg.dlevent[j]->workstr);
									external(cmdstr(cfg.dlevent[j]->cmd,path,nulstr
										,NULL)
										,EX_OUTL);
									CRLF; }
								} }
					else {
						delfiles(cfg.temp_dir,ALLFILES);
						if(cfg.dir[f.dir]->seqdev) {
							lncntr=0;
							seqwait(cfg.dir[f.dir]->seqdev);
							bprintf(text[RetrievingFile],fname);
							sprintf(str,"%s%s",dirpath,fname);
							sprintf(path,"%s%s",cfg.temp_dir,fname);
							mv(str,path,1); /* copy the file to temp dir */
							if(getnodedat(cfg.node_num,&thisnode,true)==0) {
								thisnode.aux=0xf0;
								putnodedat(cfg.node_num,&thisnode);
							}
							CRLF; 
						}
						for(j=0;j<cfg.total_dlevents;j++)
							if(!stricmp(cfg.dlevent[j]->ext,f.name+9)
								&& chk_ar(cfg.dlevent[j]->ar,&useron)) {
								bputs(cfg.dlevent[j]->workstr);
								external(cmdstr(cfg.dlevent[j]->cmd,path,nulstr,NULL)
									,EX_OUTL);
								CRLF; }
						getnodedat(cfg.node_num,&thisnode,1);
						action=NODE_DLNG;
						t=now+f.timetodl;
						localtime_r(&t,&tm);
						thisnode.aux=(tm.tm_hour*60)+tm.tm_min;
						putnodedat(cfg.node_num,&thisnode); /* calculate ETA */
						start=time(NULL);
						error=protocol(cmdstr(cfg.prot[i]->dlcmd,path,nulstr,NULL),false);
						end=time(NULL);
						if(cfg.dir[f.dir]->misc&DIR_TFREE)
							starttime+=end-start;
						if(checkprotresult(cfg.prot[i],error,&f))
							downloadfile(&f);
						else
							notdownloaded(f.size,start,end); 
						delfiles(cfg.temp_dir,ALLFILES);
						autohangup(); 
					} 
				} 
			}
			closefile(&f); }
		if(filespec[0] && !strchr(filespec,'*') && !strchr(filespec,'?')) 
			break; 
	}
	FREE((char *)ixbbuf);
	if(usrxfrbuf)
		FREE(usrxfrbuf);
	return(found);
}

/****************************************************************************/
/* Prints one file's information on a single line to a file 'file'          */
/****************************************************************************/
void sbbs_t::listfiletofile(char *fname, char HUGE16 *buf, uint dirnum, int file)
{
    char	str[256];
	char 	tmp[512];
    uchar	alt;
    ulong	cdt;
	bool	exist=true;

	strcpy(str,fname);
	if(buf[F_MISC]!=ETX && (buf[F_MISC]-SP)&FM_EXTDESC)
		strcat(str,"+");
	else
		strcat(str," ");
	write(file,str,13);
	getrec((char *)buf,F_ALTPATH,2,str);
	alt=(uchar)ahtoul(str);
	sprintf(str,"%s%s",alt>0 && alt<=cfg.altpaths ? cfg.altpath[alt-1]
		: cfg.dir[dirnum]->path,unpadfname(fname,tmp));
	if(cfg.dir[dirnum]->misc&DIR_FCHK && !fexistcase(str))
		exist=false;
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

int extdesclines(char *str)
{
	int i,lc,last;

	for(i=lc=last=0;str[i];i++)
		if(str[i]==LF || i-last>LEN_FDESC) {
			lc++;
			last=i; }
	return(lc);
}
