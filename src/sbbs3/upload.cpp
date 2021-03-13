/* Synchronet file upload-related routines */

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
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "sbbs.h"
#include "filedat.h"

/****************************************************************************/
/****************************************************************************/
bool sbbs_t::uploadfile(smbfile_t* f)
{
	char	path[MAX_PATH+1];
	char	str[MAX_PATH+1];
	char	ext[513] = "";
	char	tmp[MAX_PATH+1];
    uint	i;
    long	length;
	FILE*	stream;

	curdirnum=f->dir;
	if(findfile(&cfg, f->dir, f->name, NULL)) {
		errormsg(WHERE, ERR_CHK, f->name, f->dir);
		return false;
	}
	getfilepath(&cfg, f, path);
	SAFEPRINTF2(tmp, "%s%s", cfg.temp_dir, getfname(path));
	if(!fexistcase(path) && fexistcase(tmp))
		mv(tmp,path,0);
	if(!fexistcase(path)) {
		bprintf(text[FileNotReceived],f->name);
		safe_snprintf(str,sizeof(str),"attempted to upload %s to %s %s (Not received)"
			,f->name
			,cfg.lib[cfg.dir[f->dir]->lib]->sname,cfg.dir[f->dir]->sname);
		logline(LOG_NOTICE,"U!",str);
		return false;
	}
	f->hdr.when_written.time = (uint32_t)fdate(path);
	char* fext = getfext(f->name);
	for(i=0;i<cfg.total_ftests;i++)
		if(cfg.ftest[i]->ext[0]=='*' || (fext != NULL && stricmp(fext, cfg.ftest[i]->ext) == 0)) {
			if(!chk_ar(cfg.ftest[i]->ar,&useron,&client))
				continue;
			attr(LIGHTGRAY);
			bputs(cfg.ftest[i]->workstr);

			safe_snprintf(sbbsfilename,sizeof(sbbsfilename),"SBBSFILENAME=%s",f->name);
			putenv(sbbsfilename);
			safe_snprintf(sbbsfiledesc,sizeof(sbbsfiledesc),"SBBSFILEDESC=%s",f->desc);
			putenv(sbbsfiledesc);
			SAFEPRINTF(str,"%ssbbsfile.nam",cfg.node_dir);
			if((stream=fopen(str,"w"))!=NULL) {
				fprintf(stream, "%s", f->desc);
				fclose(stream); 
			}
			SAFEPRINTF(str,"%ssbbsfile.des",cfg.node_dir);
			if((stream=fopen(str,"w"))!=NULL) {
				fwrite(f->desc,1,strlen(f->desc),stream);
				fclose(stream); 
			}
			if(external(cmdstr(cfg.ftest[i]->cmd,path,f->desc,NULL),EX_OFFLINE)) {
				safe_snprintf(str,sizeof(str),"attempted to upload %s to %s %s (%s Errors)"
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
#if 0 // NFB-TODO - uploader tester changes filename or description
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
#endif
			}
		}

	if((length=(long)flength(path))==0L) {
		bprintf(text[FileZeroLength],f->name);
		remove(path);
		safe_snprintf(str,sizeof(str),"attempted to upload %s to %s %s (Zero length)"
			,f->name
			,cfg.lib[cfg.dir[f->dir]->lib]->sname,cfg.dir[f->dir]->sname);
		logline(LOG_NOTICE,"U!",str);
		return false; 
	}

	bputs(text[SearchingForDupes]);
	/* Note: Hashes file *after* running upload-testers (which could modify file) */
	if(hashfile(&cfg, f)) {
		for(uint i=0, k=0; i < usrlibs; i++) {
			for(uint j=0; j < usrdirs[i]; j++,k++) {
				outchar('.');
				if(k && (k%5) == 0)
					bputs("\b\b\b\b\b     \b\b\b\b\b");
				if(cfg.dir[usrdir[i][j]]->misc&DIR_DUPES
					&& findfile(&cfg, usrdir[i][j], /* filename: */NULL, f)) {
					bprintf(text[FileAlreadyOnline], f->name);
					if(!dir_op(f->dir)) {
						remove(path);
						safe_snprintf(str, sizeof(str), "attempted to upload %s to %s %s (duplicate hash)"
							,f->name
							,cfg.lib[cfg.dir[f->dir]->lib]->sname, cfg.dir[f->dir]->sname);
						logline(LOG_NOTICE, "U!", str);
						return(false); 	 /* File is in database for another dir */
					}
				}
			}
		}
	}
	bputs(text[SearchedForDupes]);

	if(cfg.dir[f->dir]->misc&DIR_DIZ) {
		lprintf(LOG_DEBUG, "Extracting DIZ from: %s", path);
		if(extract_diz(&cfg, f, /* diz_fnames: */NULL, str, sizeof(str))) {
			lprintf(LOG_DEBUG, "Parsing DIZ: %s", str);

			str_list_t lines = read_diz(str, /* max_line_len: */80);
			format_diz(lines, ext, sizeof(ext), /* allow_ansi: */false);
			strListFree(&lines);

			if(f->desc == NULL || f->desc[0] == 0) {
				char	desc[LEN_FDESC];
				SAFECOPY(desc, (char*)ext);
				strip_exascii(desc, desc);
				prep_file_desc(desc, desc);
				for(i=0;desc[i];i++)
					if(IS_ALPHANUMERIC(desc[i]))
						break;
				if(desc[i] == '\0')
					i = 0;
				smb_hfield_str(f, SMB_FILEDESC, desc + i);
			}
			remove(str);
		} else
			lprintf(LOG_DEBUG, "DIZ does not exist in: %s", path);
	}

	if(!(cfg.dir[f->dir]->misc&DIR_NOSTAT)) {
		logon_ulb+=length;  /* Update 'this call' stats */
		logon_uls++;
	}
	if(cfg.dir[f->dir]->misc&DIR_AONLY)  /* Forced anonymous */
		f->hdr.attr |= MSG_ANONYMOUS;
	uint32_t cdt = (uint32_t)length;
	smb_hfield_bin(f, SMB_COST, cdt);
	smb_hfield_str(f, SENDER, useron.alias);
	bprintf(text[FileNBytesReceived],f->name,ultoac(length,tmp));
	if(!addfile(&cfg, f->dir, f, ext))
		return false;

	safe_snprintf(str,sizeof(str),"uploaded %s to %s %s"
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
				,(ulong)(f->cost * (cfg.dir[f->dir]->up_pct/100.0))); 
	}

	user_event(EVENT_UPLOAD);

	return true;
}

/****************************************************************************/
/* Uploads files                                                            */
/****************************************************************************/
bool sbbs_t::upload(uint dirnum)
{
	char	descbeg[25]={""},descend[25]={""}
				,fname[MAX_FILENAME_LEN + 1],keys[256],ch,*p;
	char	str[MAX_PATH+1];
	char	path[MAX_PATH+1];
	char 	tmp[512];
    time_t	start,end;
    uint	i,j,k;
	ulong	space;
	smbfile_t	f = {{}};

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
		SAFECOPY(path,cfg.altpath[altul-1]);
	else
		SAFECOPY(path,cfg.dir[dirnum]->path);

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
	f.hdr.altpath=altul;
	bputs(text[Filename]);
	if(getstr(fname, sizeof(fname) - 1, 0) < 1 || strchr(fname,'?') || strchr(fname,'*')
		|| !checkfname(fname) || (trashcan(fname,"file") && !dir_op(dirnum))) {
		if(fname[0])
			bputs(text[BadFilename]);
		return(false); 
	}
	if(dirnum==cfg.sysop_dir)
		SAFEPRINTF(str,text[UploadToSysopDirQ],fname);
	else if(dirnum==cfg.user_dir)
		SAFEPRINTF(str,text[UploadToUserDirQ],fname);
	else
		SAFEPRINTF3(str,text[UploadToCurDirQ],fname,cfg.lib[cfg.dir[dirnum]->lib]->sname
			,cfg.dir[dirnum]->sname);
	if(!yesno(str)) return(false);
	action=NODE_ULNG;
	SAFECOPY(f.file_idx.name, fname);
	getfilepath(&cfg, &f, path);
	if(fexistcase(path)) {   /* File is on disk */
		if(!dir_op(dirnum) && online!=ON_LOCAL) {		 /* local users or sysops */
			bprintf(text[FileAlreadyThere],fname);
			return(false); 
		}
		if(!yesno(text[FileOnDiskAddQ]))
			return(false); 
	}
	char* ext = getfext(fname);
	SAFECOPY(str,cfg.dir[dirnum]->exts);
	j=strlen(str);
	for(i=0;i<j;i+=ch+1) { /* Check extension of upload with allowable exts */
		p=strchr(str+i,',');
		if(p!=NULL)
			*p=0;
		ch=(char)strlen(str+i);
		if(ext != NULL && stricmp(ext + 1, str + i) == 0)
			break;
	}
	if(j && i>=j) {
		bputs(text[TheseFileExtsOnly]);
		bputs(cfg.dir[dirnum]->exts);
		CRLF;
		if(!dir_op(dirnum)) return(false); 
	}
	bputs(text[SearchingForDupes]);
	if(findfile(&cfg, dirnum, f.name, NULL)) {
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
				&& findfile(&cfg, usrdir[i][j], f.name, NULL)) {
				bputs(text[SearchedForDupes]);
				bprintf(text[FileAlreadyOnline],fname);
				if(!dir_op(dirnum))
					return(false); 	 /* File is in database for another dir */
			}
		}
	}
	bputs(text[SearchedForDupes]);
	if(cfg.dir[dirnum]->misc&DIR_RATE) {
		SYNC;
		bputs(text[RateThisFile]);
		ch=getkey(K_ALPHA);
		if(!IS_ALPHA(ch) || sys_status&SS_ABORT)
			return(false);
		CRLF;
		SAFEPRINTF(descbeg,text[Rated],toupper(ch)); 
	}
	if(cfg.dir[dirnum]->misc&DIR_ULDATE) {
		now=time(NULL);
		if(descbeg[0])
			strcat(descbeg," ");
		SAFEPRINTF(str,"%s  ",unixtodstr(&cfg,(time32_t)now,tmp));
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
				SAFEPRINTF2(descend,text[FileOneOfTen],j,i);
			else
				SAFEPRINTF2(descend,text[FileOneOfTwo],j,i); 
		} else
			upload_lastdesc[0]=0; 
	}
	else
		upload_lastdesc[0]=0;

	char fdesc[LEN_FDESC + 1] = "";
	bputs(text[EnterDescNow]);
	i=LEN_FDESC-(strlen(descbeg)+strlen(descend));
	getstr(upload_lastdesc,i,K_LINE|K_EDIT|K_AUTODEL|K_TRIM);
	if(sys_status&SS_ABORT)
		return(false);
	if(descend[0])      /* end of desc specified, so pad desc with spaces */
		safe_snprintf(fdesc,sizeof(fdesc),"%s%-*s%s",descbeg,i,upload_lastdesc,descend);
	else                /* no end specified, so string ends at desc end */
		safe_snprintf(fdesc,sizeof(fdesc),"%s%s",descbeg,upload_lastdesc);

	char tags[64] = "";
	if((cfg.dir[dirnum]->misc&DIR_FILETAGS) && (text[TagFileQ][0] == 0 || !noyes(text[TagFileQ]))) {
		bputs(text[TagFilePrompt]);
		getstr(tags, sizeof(tags)-1, K_EDIT|K_LINE|K_TRIM);
	}

	if(cfg.dir[dirnum]->misc&DIR_ANON && !(cfg.dir[dirnum]->misc&DIR_AONLY)
		&& (dir_op(dirnum) || useron.exempt&FLAG('A'))) {
		if(!noyes(text[AnonymousQ]))
			f.hdr.attr |= MSG_ANONYMOUS;
	}

	bool result = false;
	smb_hfield_str(&f, SMB_FILENAME, fname);
	smb_hfield_str(&f, SMB_FILEDESC, fdesc);
	if(tags[0])
		smb_hfield_str(&f, SMB_TAGS, tags);
	if(fexistcase(path)) {   /* File is on disk */
		result = uploadfile(&f);
	} else {
		xfer_prot_menu(XFER_UPLOAD);
		SYNC;
		SAFEPRINTF(keys,"%c",text[YNQP][2]);
		if(dirnum==cfg.user_dir || !cfg.max_batup)  /* no batch user to user xfers */
			mnemonics(text[ProtocolOrQuit]);
		else {
			mnemonics(text[ProtocolBatchOrQuit]);
			strcat(keys,"B"); 
		}
		for(i=0;i<cfg.total_prots;i++)
			if(cfg.prot[i]->ulcmd[0] && chk_ar(cfg.prot[i]->ar,&useron,&client)) {
				SAFEPRINTF(tmp,"%c",cfg.prot[i]->mnemonic);
				strcat(keys,tmp); 
			}
		ch=(char)getkeys(keys,0);
		if(ch==text[YNQP][2] || (sys_status&SS_ABORT))
			result = false;
		else if(ch=='B') {
			if(batup_total() >= cfg.max_batup)
				bputs(text[BatchUlQueueIsFull]);
			else if(batch_file_exists(&cfg, useron.number, XFER_BATCH_UPLOAD, f.name))
				bprintf(text[FileAlreadyInQueue],fname);
			else if(batch_file_add(&cfg, useron.number, XFER_BATCH_UPLOAD, &f)) {
				bprintf(text[FileAddedToUlQueue]
					,fname, batup_total(), cfg.max_batup);
				result = true;
			}
		} else {
			for(i=0;i<cfg.total_prots;i++)
				if(cfg.prot[i]->ulcmd[0] && cfg.prot[i]->mnemonic==ch
					&& chk_ar(cfg.prot[i]->ar,&useron,&client))
					break;
			if(i<cfg.total_prots) {
				start=time(NULL);
				protocol(cfg.prot[i],XFER_UPLOAD,path,nulstr,true);
				end=time(NULL);
				if(!(cfg.dir[dirnum]->misc&DIR_ULTIME)) /* Don't deduct upload time */
					starttime+=end-start;
				result = uploadfile(&f);
				autohangup();
			} 
		} 
	}
	smb_freefilemem(&f);
	return result;
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
	char	desc[LEN_FDESC + 1];
	smb_t	smb;
    smbfile_t f;
	DIR*	dir;
	DIRENT*	dirent;

	memset(&f,0,sizeof(f));
	f.dir=dirnum;
	f.hdr.altpath=altul;
	bprintf(text[BulkUpload],cfg.lib[cfg.dir[dirnum]->lib]->sname,cfg.dir[dirnum]->sname);
	SAFECOPY(path, altul>0 && altul<=cfg.altpaths ? cfg.altpath[altul-1] : cfg.dir[dirnum]->path);

	int result = smb_open_dir(&cfg, &smb, dirnum);
	if(result != SMB_SUCCESS) {
		errormsg(WHERE, ERR_OPEN, smb.file, result, smb.last_error);
		return false;
	}
	action=NODE_ULNG;
	SYNC;
	str_list_t list = loadfilenames(&cfg, &smb, ALLFILES, /* time_t */0, /* sort */false, NULL);
	smb_close(&smb);
	dir=opendir(path);
	while(dir!=NULL && (dirent=readdir(dir))!=NULL && !msgabort()) {
		char fname[SMB_FILEIDX_NAMELEN + 1];
		SAFEPRINTF2(str,"%s%s",path,dirent->d_name);
		if(isdir(str))
			continue;
#ifdef _WIN32
		/* Skip hidden/system files on Win32 */
		if(getfattr(str)&(_A_HIDDEN|_A_SYSTEM))
			continue;
#endif
		smb_fileidxname(dirent->d_name, fname, sizeof(fname));
		if(strListFind(list, fname, /* case-sensitive: */FALSE) < 0) {
			smb_freemsgmem(&f);
			smb_hfield_str(&f, SMB_FILENAME, dirent->d_name);
			uint32_t cdt = (uint32_t)flength(str);
			smb_hfield_bin(&f, SMB_COST, cdt);
			bprintf(text[BulkUploadDescPrompt], format_filename(f.name, fname, 12, /* pad: */FALSE), cdt/1024);
			getstr(desc, LEN_FDESC, K_LINE);
			if(sys_status&SS_ABORT)
				break;
			if(strcmp(desc,"-")==0)	/* don't add this file */
				continue;
			smb_hfield_str(&f, SMB_FILEDESC, desc);
			uploadfile(&f);
		}
	}
	if(dir!=NULL)
		closedir(dir);
	strListFree(&list);
	smb_freemsgmem(&f);
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
		SAFEPRINTF(keys,"%c",text[YNQP][2]);
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
