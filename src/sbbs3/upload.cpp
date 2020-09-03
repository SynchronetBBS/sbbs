/* upload.cpp */

/* Synchronet file upload-related routines */

/* $Id: upload.cpp,v 1.63 2019/08/02 10:36:45 rswindell Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
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

/****************************************************************************/
/* Adds file data in 'f' to DIR#.DAT and DIR#.IXB   and updates user        */
/* info for uploader. Must have .name, .desc and .dir fields filled prior   */
/* to a call to this function.                                              */
/* Returns 1 if file uploaded sucessfully, 0 if not.                        */
/****************************************************************************/
bool sbbs_t::uploadfile(file_t *f)
{
	char	path[MAX_PATH+1];
	char	str[MAX_PATH+1],fname[25];
	char	ext[F_EXBSIZE+1];
	char	desc[F_EXBSIZE+1];
	char	tmp[MAX_PATH+1];
	int		file;
    uint	i;
    long	length;
	FILE*	stream;

	// f->misc=0; Removed AUG-22-2001 - broken anonymous uploads
	curdirnum=f->dir;
	if(findfile(&cfg,f->dir,f->name)) {
		errormsg(WHERE,ERR_CHK,f->name,f->dir);
		return(0); 
	}
	sprintf(path,"%s%s",f->altpath>0 && f->altpath<=cfg.altpaths
		? cfg.altpath[f->altpath-1]
		: cfg.dir[f->dir]->path,unpadfname(f->name,fname));
	sprintf(tmp,"%s%s",cfg.temp_dir,fname);
	if(!fexistcase(path) && fexistcase(tmp))
		mv(tmp,path,0);
	if(!fexistcase(path)) {
		bprintf(text[FileNotReceived],f->name);
		sprintf(str,"attempted to upload %s to %s %s (Not received)"
			,f->name
			,cfg.lib[cfg.dir[f->dir]->lib]->sname,cfg.dir[f->dir]->sname);
		logline(LOG_NOTICE,"U!",str);
		return(0); 
	}
	strcpy(tmp,f->name);
	truncsp(tmp);
	for(i=0;i<cfg.total_ftests;i++)
		if(cfg.ftest[i]->ext[0]=='*' || !stricmp(tmp+9,cfg.ftest[i]->ext)) {
			if(!chk_ar(cfg.ftest[i]->ar,&useron,&client))
				continue;
			attr(LIGHTGRAY);
			bputs(cfg.ftest[i]->workstr);

			sprintf(sbbsfilename,"SBBSFILENAME=%.64s",unpadfname(f->name,fname));
			putenv(sbbsfilename);
			sprintf(sbbsfiledesc,"SBBSFILEDESC=%.*s",LEN_FDESC,f->desc);
			putenv(sbbsfiledesc);
			sprintf(str,"%ssbbsfile.nam",cfg.node_dir);
			if((stream=fopen(str,"w"))!=NULL) {
				fwrite(fname,1,strlen(fname),stream);
				fclose(stream); 
			}
			sprintf(str,"%ssbbsfile.des",cfg.node_dir);
			if((stream=fopen(str,"w"))!=NULL) {
				fwrite(f->desc,1,strlen(f->desc),stream);
				fclose(stream); 
			}
			if(external(cmdstr(cfg.ftest[i]->cmd,path,f->desc,NULL),EX_OFFLINE)) {
				sprintf(str,"attempted to upload %s to %s %s (%s Errors)"
					,f->name
					,cfg.lib[cfg.dir[f->dir]->lib]->sname,cfg.dir[f->dir]->sname,cfg.ftest[i]->ext);
				logline(LOG_NOTICE,"U!",str);
#if 0
				sprintf(str,"Failed test: %s", cmdstr(cfg.ftest[i]->cmd,path,f->desc,NULL));
				logline("  ",str);
#endif
				bprintf(text[FileHadErrors],f->name,cfg.ftest[i]->ext);
				if(!SYSOP || yesno(text[DeleteFileQ]))
					remove(path);
				return(0); 
			} else {
				sprintf(str,"%ssbbsfile.nam",cfg.node_dir);
				if((stream=fopen(str,"r"))!=NULL) {
					if(fgets(str,128,stream)) {
						truncsp(str);
						padfname(str,f->name);
						strcpy(tmp,f->name);
						truncsp(tmp);
						sprintf(path,"%s%s",f->altpath>0 && f->altpath<=cfg.altpaths
							? cfg.altpath[f->altpath-1] : cfg.dir[f->dir]->path
							,unpadfname(f->name,fname)); 
					}
					fclose(stream);
					}
				sprintf(str,"%ssbbsfile.des",cfg.node_dir);
				if((stream=fopen(str,"r"))!=NULL) {
					if(fgets(str,128,stream)) {
						truncsp(str);
						sprintf(f->desc,"%.*s",LEN_FDESC,str); 
					}
					fclose(stream); 
				}
				CRLF; 
			} 
		}

	if((length=(long)flength(path))==0L) {
		bprintf(text[FileZeroLength],f->name);
		remove(path);
		sprintf(str,"attempted to upload %s to %s %s (Zero length)"
			,f->name
			,cfg.lib[cfg.dir[f->dir]->lib]->sname,cfg.dir[f->dir]->sname);
		logline(LOG_NOTICE,"U!",str);
		return(0); 
	}
	if(cfg.dir[f->dir]->misc&DIR_DIZ) {
		for(i=0;i<cfg.total_fextrs;i++)
			if(!stricmp(cfg.fextr[i]->ext,tmp+9) && chk_ar(cfg.fextr[i]->ar,&useron,&client))
				break;
		if(i<cfg.total_fextrs) {
			sprintf(str,"%sFILE_ID.DIZ",cfg.temp_dir);
			if(fexistcase(str))
				remove(str);
			external(cmdstr(cfg.fextr[i]->cmd,path,"FILE_ID.DIZ",NULL),EX_OFFLINE);
			if(!fexistcase(str)) {
				sprintf(str,"%sDESC.SDI",cfg.temp_dir);
				if(fexistcase(str))
					remove(str);
				external(cmdstr(cfg.fextr[i]->cmd,path,"DESC.SDI",NULL),EX_OFFLINE); 
				fexistcase(str);
			}
			if((file=nopen(str,O_RDONLY))!=-1) {
				memset(ext,0,F_EXBSIZE+1);
				read(file,ext,F_EXBSIZE);
				for(i=F_EXBSIZE;i;i--)
					if(ext[i-1]>' ')
						break;
				ext[i]=0;
				if(!f->desc[0]) {
					strcpy(desc,ext);
					strip_exascii(desc, desc);
					prep_file_desc(desc, desc);
					for(i=0;desc[i];i++)
						if(isalnum(desc[i]))
							break;
					sprintf(f->desc,"%.*s",LEN_FDESC,desc+i); 
				}
				close(file);
				remove(str);
				f->misc|=FM_EXTDESC; 
			} 
		} 
	}

	if(!(cfg.dir[f->dir]->misc&DIR_NOSTAT)) {
		logon_ulb+=length;  /* Update 'this call' stats */
		logon_uls++;
	}
	if(cfg.dir[f->dir]->misc&DIR_AONLY)  /* Forced anonymous */
		f->misc|=FM_ANON;
	f->cdt=length;
	f->dateuled=time32(NULL);
	f->timesdled=0;
	f->datedled=0L;
	f->opencount=0;
	strcpy(f->uler,useron.alias);
	bprintf(text[FileNBytesReceived],f->name,ultoac(length,tmp));
	if(!f->desc[0]) {
		if(stricmp(f->name,getfname(path)))
			SAFECOPY(f->desc,getfname(path));
		else
			sprintf(f->desc,"%.*s",LEN_FDESC,text[NoDescription]);
	}
	if(!addfiledat(&cfg,f))
		return(0);

	if(f->misc&FM_EXTDESC)
		putextdesc(&cfg,f->dir,f->datoffset,ext);

	sprintf(str,"uploaded %s to %s %s"
		,f->name,cfg.lib[cfg.dir[f->dir]->lib]->sname
		,cfg.dir[f->dir]->sname);
	if(cfg.dir[f->dir]->upload_sem[0])
		ftouch(cmdstr(cfg.dir[f->dir]->upload_sem,nulstr,nulstr,NULL));
	logline("U+",str);
	/**************************/
	/* Update Uploader's Info */
	/**************************/
	user_uploaded(&cfg, &useron, 1, length);
	if(cfg.dir[f->dir]->up_pct && cfg.dir[f->dir]->misc&DIR_CDTUL) { /* credit for upload */
		if(cfg.dir[f->dir]->misc&DIR_CDTMIN && cur_cps)    /* Give min instead of cdt */
			useron.min=adjustuserrec(&cfg,useron.number,U_MIN,10
				,((ulong)(length*(cfg.dir[f->dir]->up_pct/100.0))/cur_cps)/60);
		else
			useron.cdt=adjustuserrec(&cfg,useron.number,U_CDT,10
				,(ulong)(f->cdt*(cfg.dir[f->dir]->up_pct/100.0))); 
	}

	user_event(EVENT_UPLOAD);

	return(true);
}

/****************************************************************************/
/* Uploads files                                                            */
/****************************************************************************/
bool sbbs_t::upload(uint dirnum)
{
	char	descbeg[25]={""},descend[25]={""}
				,fname[13],keys[256],ch,*p;
	char	str[MAX_PATH+1];
	char	path[MAX_PATH+1];
	char	spath[MAX_PATH+1];
	char 	tmp[512];
    time_t	start,end;
    uint	i,j,k,destuser[MAX_USERXFER],destusers=0;
	int		file;
	ulong	space;
    file_t	f;
    user_t	user;
    node_t	node;

	/* Security Checks */
	if(useron.rest&FLAG('U')) {
		bputs(text[R_Upload]);
		return(false); 
	}
	if(dirnum==INVALID_DIR) {
		bputs(text[CantUploadHere]);
		return(false);
	}
	if(!(useron.exempt&FLAG('U')) && !dir_op(dirnum)) {
		if(!chk_ar(cfg.dir[dirnum]->ul_ar,&useron,&client)) {
			bputs(dirnum==cfg.user_dir ? text[CantUploadToUser] : 
				dirnum==cfg.sysop_dir ? text[CantUploadToSysop] : text[CantUploadHere]);
			return(false); 
		}
		if(cfg.dir[dirnum]->maxfiles && getfiles(&cfg,dirnum)>=cfg.dir[dirnum]->maxfiles) {
			bputs(dirnum==cfg.user_dir ? text[UserDirFull] : text[DirFull]);
			return(false);
		}
	}

	if(sys_status&SS_EVENT && online==ON_REMOTE && !dir_op(dirnum))
		bprintf(text[UploadBeforeEvent],timeleft/60);
	if(altul)
		strcpy(path,cfg.altpath[altul-1]);
	else
		strcpy(path,cfg.dir[dirnum]->path);

	if(!isdir(path)) {
		bprintf(text[DirectoryDoesNotExist], path);
		lprintf(LOG_ERR,"File directory does not exist: %s", path);
		return(false);
	}

	/* get free disk space */
	space=getfreediskspace(path,1024);
	if(space<(ulong)cfg.min_dspace) {
		bputs(text[LowDiskSpace]);
		lprintf(LOG_ERR,"Diskspace is low: %s (%lu kilobytes)",path,space);
		if(!dir_op(dirnum))
			return(false); 
	}
	bprintf(text[DiskNBytesFree],ultoac(space,tmp));

	f.dir=curdirnum=dirnum;
	f.misc=0;
	f.altpath=altul;
	bputs(text[Filename]);
	if(!getstr(fname,12,0) || strchr(fname,'?') || strchr(fname,'*')
		|| !checkfname(fname) || (trashcan(fname,"file") && !dir_op(dirnum))) {
		if(fname[0])
			bputs(text[BadFilename]);
		return(false); 
	}
	if(dirnum==cfg.sysop_dir)
		sprintf(str,text[UploadToSysopDirQ],fname);
	else if(dirnum==cfg.user_dir)
		sprintf(str,text[UploadToUserDirQ],fname);
	else
		sprintf(str,text[UploadToCurDirQ],fname,cfg.lib[cfg.dir[dirnum]->lib]->sname
			,cfg.dir[dirnum]->sname);
	if(!yesno(str)) return(false);
	action=NODE_ULNG;
	sprintf(str,"%s%s",path,fname);
	if(fexistcase(str)) {   /* File is on disk */
#ifdef _WIN32
		GetShortPathName(str, spath, sizeof(spath));
#else
		SAFECOPY(spath,str);
#endif
		SAFECOPY(fname,getfname(spath));
		if(!dir_op(dirnum) && online!=ON_LOCAL) {		 /* local users or sysops */
			bprintf(text[FileAlreadyThere],fname);
			return(false); 
		}
		if(!yesno(text[FileOnDiskAddQ]))
			return(false); 
	}
	padfname(fname,f.name);
	strcpy(str,cfg.dir[dirnum]->exts);
	strcpy(tmp,f.name);
	truncsp(tmp);
	j=strlen(str);
	for(i=0;i<j;i+=ch+1) { /* Check extension of upload with allowable exts */
		p=strchr(str+i,',');
		if(p!=NULL)
			*p=0;
		ch=(char)strlen(str+i);
		if(!stricmp(tmp+9,str+i))
			break; 
	}
	if(j && i>=j) {
		bputs(text[TheseFileExtsOnly]);
		bputs(cfg.dir[dirnum]->exts);
		CRLF;
		if(!dir_op(dirnum)) return(false); 
	}
	bputs(text[SearchingForDupes]);
	if(findfile(&cfg,dirnum,f.name)) {
		bputs(text[SearchedForDupes]);
		bprintf(text[FileAlreadyOnline],fname);
		return(false); 	 /* File is already in database */
	}
	for(i=k=0;i<usrlibs;i++) {
		for(j=0;j<usrdirs[i];j++,k++) {
			outchar('.');
			if(k && !(k%5))
				bputs("\b\b\b\b\b     \b\b\b\b\b");
			if(usrdir[i][j]==dirnum)
				continue;	/* we already checked this dir */
			if(cfg.dir[usrdir[i][j]]->misc&DIR_DUPES
				&& findfile(&cfg,usrdir[i][j],f.name)) {
				bputs(text[SearchedForDupes]);
				bprintf(text[FileAlreadyOnline],fname);
				if(!dir_op(dirnum))
					return(false); 	 /* File is in database for another dir */
			}
		}
	}
	bputs(text[SearchedForDupes]);
	if(dirnum==cfg.user_dir) {  /* User to User transfer */
		bputs(text[EnterAfterLastDestUser]);
		while((!dir_op(dirnum) && destusers<cfg.max_userxfer) || destusers<MAX_USERXFER) {
			bputs(text[SendFileToUser]);
			if(!getstr(str,LEN_ALIAS,cfg.uq&UQ_NOUPRLWR ? K_NONE:K_UPRLWR))
				break;
			if((user.number=finduser(str))!=0) {
				if(!dir_op(dirnum) && user.number==useron.number) {
					bputs(text[CantSendYourselfFiles]);
					continue; 
				}
				for(i=0;i<destusers;i++)
					if(user.number==destuser[i])
						break;
				if(i<destusers) {
					bputs(text[DuplicateUser]);
					continue; 
				}
				getuserdat(&cfg,&user);
				if((user.rest&(FLAG('T')|FLAG('D')))
					|| !chk_ar(cfg.lib[cfg.dir[cfg.user_dir]->lib]->ar,&user,/* client: */NULL)
					|| !chk_ar(cfg.dir[cfg.user_dir]->dl_ar,&user,/* client: */NULL)) {
					bprintf(text[UserWontBeAbleToDl],user.alias); 
				} else {
					bprintf(text[UserAddedToDestList],user.alias);
					destuser[destusers++]=user.number; 
				} 
			}
			else {
				CRLF; 
			} 
		}
		if(!destusers)
			return(false); 
	}
	if(cfg.dir[dirnum]->misc&DIR_RATE) {
		SYNC;
		bputs(text[RateThisFile]);
		ch=getkey(K_ALPHA);
		if(!isalpha(ch) || sys_status&SS_ABORT)
			return(false);
		CRLF;
		sprintf(descbeg,text[Rated],toupper(ch)); 
	}
	if(cfg.dir[dirnum]->misc&DIR_ULDATE) {
		now=time(NULL);
		if(descbeg[0])
			strcat(descbeg," ");
		sprintf(str,"%s  ",unixtodstr(&cfg,(time32_t)now,tmp));
		strcat(descbeg,str); 
	}
	if(cfg.dir[dirnum]->misc&DIR_MULT) {
		SYNC;
		if(!noyes(text[MultipleDiskQ])) {
			bputs(text[HowManyDisksTotal]);
			if((int)(i=getnum(99))<2)
				return(false);
			bputs(text[NumberOfFile]);
			if((int)(j=getnum(i))<1)
				return(false);
			if(j==1)
				upload_lastdesc[0]=0;
			if(i>9)
				sprintf(descend,text[FileOneOfTen],j,i);
			else
				sprintf(descend,text[FileOneOfTwo],j,i); 
		} else
			upload_lastdesc[0]=0; 
	}
	else
		upload_lastdesc[0]=0;
	bputs(text[EnterDescNow]);
	i=LEN_FDESC-(strlen(descbeg)+strlen(descend));
	getstr(upload_lastdesc,i,K_LINE|K_EDIT|K_AUTODEL);
	if(sys_status&SS_ABORT)
		return(false);
	if(descend[0])      /* end of desc specified, so pad desc with spaces */
		sprintf(f.desc,"%s%-*s%s",descbeg,i,upload_lastdesc,descend);
	else                /* no end specified, so string ends at desc end */
		sprintf(f.desc,"%s%s",descbeg,upload_lastdesc);

	if(cfg.dir[dirnum]->misc&DIR_ANON && !(cfg.dir[dirnum]->misc&DIR_AONLY)
		&& (dir_op(dirnum) || useron.exempt&FLAG('A'))) {
		if(!noyes(text[AnonymousQ]))
			f.misc|=FM_ANON; 
	}
	sprintf(str,"%s%s",path,fname);
	if(fexistcase(str)) {   /* File is on disk */
		if(!uploadfile(&f))
			return(false); 
	} else {
		xfer_prot_menu(XFER_UPLOAD);
		SYNC;
		sprintf(keys,"%c",text[YNQP][2]);
		if(dirnum==cfg.user_dir || !cfg.max_batup)  /* no batch user to user xfers */
			mnemonics(text[ProtocolOrQuit]);
		else {
			mnemonics(text[ProtocolBatchOrQuit]);
			strcat(keys,"B"); 
		}
		for(i=0;i<cfg.total_prots;i++)
			if(cfg.prot[i]->ulcmd[0] && chk_ar(cfg.prot[i]->ar,&useron,&client)) {
				sprintf(tmp,"%c",cfg.prot[i]->mnemonic);
				strcat(keys,tmp); 
			}
		ch=(char)getkeys(keys,0);
		if(ch==text[YNQP][2] || (sys_status&SS_ABORT))
			return(false);
		if(ch=='B') {
			if(batup_total>=cfg.max_batup)
				bputs(text[BatchUlQueueIsFull]);
			else {
				for(i=0;i<batup_total;i++)
					if(!strcmp(batup_name[i],f.name)) {
						bprintf(text[FileAlreadyInQueue],fname);
						return(false); 
					}
				strcpy(batup_name[batup_total],f.name);
				strcpy(batup_desc[batup_total],f.desc);
				batup_dir[batup_total]=dirnum;
				batup_misc[batup_total]=f.misc;
				batup_alt[batup_total]=altul;
				batup_total++;
				bprintf(text[FileAddedToUlQueue]
					,fname,batup_total,cfg.max_batup); 
			} 
		} else {
			for(i=0;i<cfg.total_prots;i++)
				if(cfg.prot[i]->ulcmd[0] && cfg.prot[i]->mnemonic==ch
					&& chk_ar(cfg.prot[i]->ar,&useron,&client))
					break;
			if(i<cfg.total_prots) {
				start=time(NULL);
				protocol(cfg.prot[i],XFER_UPLOAD,str,nulstr,true);
				end=time(NULL);
				if(!(cfg.dir[dirnum]->misc&DIR_ULTIME)) /* Don't deduct upload time */
					starttime+=end-start;
				ch=uploadfile(&f);
				autohangup();
				if(!ch)  /* upload failed, don't process user to user xfer */
					return(false); 
			} 
		} 
	}
	if(dirnum==cfg.user_dir) {  /* Add files to XFER.IXT in INDX dir */
		sprintf(str,"%sxfer.ixt",cfg.data_dir);
		if((file=nopen(str,O_WRONLY|O_CREAT|O_APPEND))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_APPEND);
			return(false); 
		}
		for(j=0;j<destusers;j++) {
			for(i=1;i<=cfg.sys_nodes;i++) { /* Tell user, if online */
				getnodedat(i,&node,0);
				if(node.useron==destuser[j] && !(node.misc&NODE_POFF)
					&& (node.status==NODE_INUSE || node.status==NODE_QUIET)) {
					sprintf(str,text[UserToUserXferNodeMsg],cfg.node_num,useron.alias);
					putnmsg(&cfg,i,str);
					break; 
				} 
			}
			if(i>cfg.sys_nodes) {   /* User not online */
				sprintf(str,text[UserSentYouFile],useron.alias);
				putsmsg(&cfg,destuser[j],str); 
			}
			sprintf(str,"%4.4u %12.12s %4.4u\r\n"
				,destuser[j],f.name,useron.number);
			write(file,str,strlen(str)); 
		}
		close(file); 
	}
	return(true);
}

/****************************************************************************/
/* Checks directory for 'dir' and prompts user to enter description for     */
/* the files that aren't in the database.                                   */
/* Returns 1 if the user aborted, 0 if not.                                 */
/****************************************************************************/
bool sbbs_t::bulkupload(uint dirnum)
{
    char	str[MAX_PATH+1];
	char	path[MAX_PATH+1];
	char	spath[MAX_PATH+1];
    file_t	f;
	DIR*	dir;
	DIRENT*	dirent;

	memset(&f,0,sizeof(file_t));
	f.dir=dirnum;
	f.altpath=altul;
	bprintf(text[BulkUpload],cfg.lib[cfg.dir[dirnum]->lib]->sname,cfg.dir[dirnum]->sname);
	strcpy(path,altul>0 && altul<=cfg.altpaths ? cfg.altpath[altul-1]
		: cfg.dir[dirnum]->path);
	action=NODE_ULNG;
	SYNC;
	dir=opendir(path);
	while(dir!=NULL && (dirent=readdir(dir))!=NULL && !msgabort()) {
		sprintf(str,"%s%s",path,dirent->d_name);
		if(isdir(str))
			continue;
#ifdef _WIN32
		/* Skip hidden/system files on Win32 */
		if(getfattr(str)&(_A_HIDDEN|_A_SYSTEM))
			continue;
#endif
#ifdef _WIN32
		GetShortPathName(str,spath,sizeof(spath));
#else
		strcpy(spath,str);
#endif
		padfname(getfname(spath),str);

		if(findfile(&cfg,f.dir,str)==0) {
			f.misc=0;
			strcpy(f.name,str);
			f.cdt=(long)flength(spath);
			bprintf(text[BulkUploadDescPrompt],f.name,f.cdt/1024);
			getstr(f.desc,LEN_FDESC,K_LINE);
			if(sys_status&SS_ABORT)
				break;
			if(strcmp(f.desc,"-")==0)	/* don't add this file */
				continue;
			uploadfile(&f); 
		}
	}
	if(dir!=NULL)
		closedir(dir);
	if(sys_status&SS_ABORT)
		return(true);
	return(false);
}

bool sbbs_t::recvfile(char *fname, char prot, bool autohang)
{
	char	keys[32];
	char	ch;
	size_t	i;
	bool	result=false;

	if(prot)
		ch=toupper(prot);
	else {
		xfer_prot_menu(XFER_UPLOAD);
		mnemonics(text[ProtocolOrQuit]);
		sprintf(keys,"%c",text[YNQP][2]);
		for(i=0;i<cfg.total_prots;i++)
			if(cfg.prot[i]->ulcmd[0] && chk_ar(cfg.prot[i]->ar,&useron,&client))
				sprintf(keys+strlen(keys),"%c",cfg.prot[i]->mnemonic);

		ch=(char)getkeys(keys,0);

		if(ch==text[YNQP][2] || sys_status&SS_ABORT)
			return(false); 
	}
	for(i=0;i<cfg.total_prots;i++)
		if(cfg.prot[i]->mnemonic==ch && chk_ar(cfg.prot[i]->ar,&useron,&client))
			break;
	if(i<cfg.total_prots) {
		if(protocol(cfg.prot[i], XFER_UPLOAD, fname, fname, true, autohang)==0)
			result=true;
		autohangup(); 
	}

	return(result);
}
