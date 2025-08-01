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
#include "sauce.h"
#include "filedat.h"

/****************************************************************************/
/****************************************************************************/
bool sbbs_t::uploadfile(file_t* f)
{
	char  path[MAX_PATH + 1];
	char  str[MAX_PATH + 1] = "";
	char  ext[LEN_EXTDESC + 1] = "";
	char  tmp[MAX_PATH + 1];
	int   i;
	off_t length;
	FILE* stream;

	curdirnum = f->dir;
	if (findfile(&cfg, f->dir, f->name, NULL)) {
		errormsg(WHERE, ERR_CHK, f->name, f->dir);
		return false;
	}
	getfilepath(&cfg, f, path);
	SAFEPRINTF2(tmp, "%s%s", cfg.temp_dir, getfname(path));
	if (!fexistcase(path) && fexistcase(tmp))
		mv(tmp, path, 0);
	if (!fexistcase(path)) {
		bprintf(text[FileNotReceived], f->name);
		safe_snprintf(str, sizeof(str), "attempted to upload %s to %s %s (Not received)"
		              , f->name
		              , cfg.lib[cfg.dir[f->dir]->lib]->sname, cfg.dir[f->dir]->sname);
		logline(LOG_NOTICE, "U!", str);
		return false;
	}
	f->hdr.when_written.time = (uint32_t)fdate(path);
	for (i = 0; i < cfg.total_ftests; i++)
		if (file_type_match(f->name, cfg.ftest[i]->ext)) {
			if (!chk_ar(cfg.ftest[i]->ar, &useron, &client))
				continue;
			attr(LIGHTGRAY);
			bputs(cfg.ftest[i]->workstr);

			SAFEPRINTF(str, "%ssbbsfile.nam", cfg.node_dir);
			if ((stream = fopen(str, "w")) != NULL) {
				fprintf(stream, "%s", f->name);
				fclose(stream);
			}
			SAFEPRINTF(str, "%ssbbsfile.des", cfg.node_dir);
			if ((stream = fopen(str, "w")) != NULL) {
				if (f->desc != NULL)
					fprintf(stream, "%s", f->desc);
				fclose(stream);
			}
			// Note: str (%s) is path/to/sbbsfile.des (used to be the description itself)
			int result = external(cmdstr(cfg.ftest[i]->cmd, path, str, NULL, cfg.ftest[i]->ex_mode), cfg.ftest[i]->ex_mode | EX_OFFLINE);
			term->clearline();
			if (result != 0) {
				safe_snprintf(str, sizeof(str), "attempted to upload %s to %s %s (%s error code %d)"
				              , f->name
				              , cfg.lib[cfg.dir[f->dir]->lib]->sname, cfg.dir[f->dir]->sname, cfg.ftest[i]->ext
				              , result);
				logline(LOG_NOTICE, "U!", str);
				bprintf(text[FileHadErrors], f->name, cfg.ftest[i]->ext);
				if (!useron_is_sysop() || yesno(text[DeleteFileQ]))
					remove(path);
				return false;
			}
			SAFEPRINTF(str, "%ssbbsfile.nam", cfg.node_dir);
			if ((stream = fopen(str, "r")) != NULL) {
				if (fgets(str, sizeof(str), stream)) {
					truncsp(str);
					smb_new_hfield_str(f, SMB_FILENAME, str);
					getfilepath(&cfg, f, path);
				}
				fclose(stream);
			}
			SAFEPRINTF(str, "%ssbbsfile.des", cfg.node_dir);
			if ((stream = fopen(str, "r")) != NULL) {
				if (fgets(str, sizeof(str), stream)) {
					truncsp(str);
					if (*str)
						smb_new_hfield_str(f, SMB_FILEDESC, str);
				}
				fclose(stream);
			}
		}

	if ((length = flength(path)) == 0L) {
		bprintf(text[FileZeroLength], f->name);
		remove(path);
		safe_snprintf(str, sizeof(str), "attempted to upload %s to %s %s (Zero length)"
		              , f->name
		              , cfg.lib[cfg.dir[f->dir]->lib]->sname, cfg.dir[f->dir]->sname);
		logline(LOG_NOTICE, "U!", str);
		return false;
	}

	bputs(text[HashingFile]);
	/* Note: Hashes file *after* running upload-testers (which could modify file) */
	bool hashed = hashfile(&cfg, f);
	bputs(text[HashedFile]);
	if (hashed) {
		for (int i = 0, k = 0; i < usrlibs; i++) {
			progress(text[Scanning], i, usrlibs);
			for (int j = 0; j < usrdirs[i]; j++, k++) {
				if (cfg.dir[usrdir[i][j]]->misc & DIR_DUPES
				    && findfile(&cfg, usrdir[i][j], /* filename: */ NULL, f)) {
					bprintf(text[FileAlreadyOnline], f->name, lib_name(usrdir[i][j]), dir_name(usrdir[i][j]));
					if (!dir_op(f->dir)) {
						remove(path);
						safe_snprintf(str, sizeof(str), "attempted to upload %s to %s %s (duplicate hash)"
						              , f->name
						              , cfg.lib[cfg.dir[f->dir]->lib]->sname, cfg.dir[f->dir]->sname);
						logline(LOG_NOTICE, "U!", str);
						return false;   /* File is in database for another dir */
					}
				}
			}
		}
		progress(text[Done], usrlibs, usrlibs);
	}

	if (cfg.dir[f->dir]->misc & DIR_DIZ) {
		lprintf(LOG_DEBUG, "Extracting DIZ from: %s", path);
		if (extract_diz(&cfg, f, /* diz_fnames: */ NULL, str, sizeof(str))) {
			struct sauce_charinfo sauce;
			lprintf(LOG_DEBUG, "Parsing DIZ: %s", str);

			char*                 lines = read_diz(str, &sauce);
			format_diz(lines, ext, sizeof(ext), sauce.width, sauce.ice_color);
			free_diz(lines);
			file_sauce_hfields(f, &sauce);

			if (f->desc == NULL || f->desc[0] == 0) {
				char desc[LEN_EXTDESC + 1];
				SAFECOPY(desc, (char*)ext);
				prep_file_desc(desc, desc);
				smb_new_hfield_str(f, SMB_FILEDESC, desc);
			}
			remove(str);
		} else
			lprintf(LOG_DEBUG, "DIZ does not exist in: %s", path);
	}

	if (!(cfg.dir[f->dir]->misc & DIR_NOSTAT)) {
		logon_ulb += length;  /* Update 'this call' stats */
		logon_uls++;
		inc_upload_stats(&cfg, 1, length);
	}
	if (cfg.dir[f->dir]->misc & DIR_AONLY)  /* Forced anonymous */
		f->hdr.attr |= MSG_ANONYMOUS;
	smb_hfield_bin(f, SMB_COST, length);
	smb_hfield_str(f, SENDER, useron.alias);
	bprintf(text[FileNBytesReceived], f->name, u64toac(length, tmp));
	if (!addfile(&cfg, f, ext, /* metadata: */ NULL, &client, NULL))
		return false;

	snprintf(str, sizeof(str), "uploaded %s (%" PRId64 " bytes) to %s %s"
	         , f->name
	         , length
	         , cfg.lib[cfg.dir[f->dir]->lib]->sname
	         , cfg.dir[f->dir]->sname);
	if (cfg.dir[f->dir]->upload_sem[0])
		ftouch(cmdstr(cfg.dir[f->dir]->upload_sem, nulstr, nulstr, NULL));
	logline("U+", str);
	/**************************/
	/* Update Uploader's Info */
	/**************************/
	user_uploaded(&cfg, &useron, 1, length);
	if (cfg.dir[f->dir]->up_pct && cfg.dir[f->dir]->misc & DIR_CDTUL) { /* credit for upload */
		if (cfg.dir[f->dir]->misc & DIR_CDTMIN && cur_cps)    /* Give min instead of cdt */
			useron.min = (uint32_t)adjustuserval(&cfg, useron.number, USER_MIN
			                                     , ((ulong)(length * (cfg.dir[f->dir]->up_pct / 100.0)) / cur_cps) / 60);
		else
			useron.cdt = adjustuserval(&cfg, useron.number, USER_CDT
			                           , (int64_t)(f->cost * (cfg.dir[f->dir]->up_pct / 100.0)));
	}
	mqtt_file_upload(mqtt, &useron, f->dir, f->name, length, &client);
	user_event(EVENT_UPLOAD);
	if (f->dir == cfg.sysop_dir) {
		snprintf(str, sizeof str, text[UserSentYouFile], useron.alias);
		notify(str, f->name);
	}
	return true;
}

bool sbbs_t::okay_to_upload(int dirnum)
{
	char str[MAX_PATH + 1];
	char path[MAX_PATH + 1];

	if (!dirnum_is_valid(dirnum))
		return false;

	SAFECOPY(path, cfg.dir[dirnum]->path);

	if (!isdir(path)) {
		bprintf(text[DirectoryDoesNotExist], path);
		lprintf(LOG_ERR, "File directory does not exist: %s", path);
		return false;
	}

	/* get free disk space */
	int64_t space = getfreediskspace(path, 1);
	byte_estimate_to_str(space, str, sizeof(str), /* units: */ 1024, /* precision: */ 1);
	if (space < cfg.min_dspace) {
		bputs(text[LowDiskSpace]);
		lprintf(LOG_ERR, "Disk space is low: %s (%s bytes)", path, str);
		if (!dir_op(dirnum))
			return false;
	}
	bprintf(text[DiskNBytesFree], str);
	return true;
}

/****************************************************************************/
/* Uploads files                                                            */
/****************************************************************************/
bool sbbs_t::upload(int dirnum, const char* fname)
{
	char       descbeg[25] = {""}, descend[25] = {""};
	char       filename[MAX_FILENAME_LEN + 1]{};
	char       keys[256], ch, *p;
	char       str[MAX_PATH + 1];
	char       path[MAX_PATH + 1];
	char       tmp[512];
	int        i, j, k;
	file_t     f = {{}};
	str_list_t dest_user_list = NULL;

	/* Security Checks */
	if (useron.rest & FLAG('U')) {
		bputs(text[R_Upload]);
		return false;
	}
	if (dirnum == INVALID_DIR) {
		bputs(text[CantUploadHere]);
		return false;
	}
	if (!(useron.exempt & FLAG('U')) && !dir_op(dirnum)) {
		if (!chk_ar(cfg.dir[dirnum]->ul_ar, &useron, &client) || !chk_ar(cfg.lib[cfg.dir[dirnum]->lib]->ul_ar, &useron, &client)) {
			bputs(dirnum == cfg.user_dir ? text[CantUploadToUser] :
			      dirnum == cfg.sysop_dir ? text[CantUploadToSysop] : text[CantUploadHere]);
			return false;
		}
		if (cfg.dir[dirnum]->maxfiles && getfiles(&cfg, dirnum) >= cfg.dir[dirnum]->maxfiles) {
			bputs(dirnum == cfg.user_dir ? text[UserDirFull] : text[DirFull]);
			return false;
		}
	}

	if (sys_status & SS_EVENT && online == ON_REMOTE && !dir_op(dirnum))
		bprintf(text[UploadBeforeEvent], timeleft / 60);

	if (!okay_to_upload(dirnum))
		return false;

	f.dir = curdirnum = dirnum;
	if (fname == nullptr) {
		bputs(text[Filename]);
		if (getstr(filename, sizeof(filename) - 1, K_TRIM) < 1)
			return false;
		fname = filename;
	}
	if (!checkfname(fname)) {
		bprintf(text[BadFilename], fname);
		return false;
	}
	if (dirnum == cfg.sysop_dir)
		SAFEPRINTF(str, text[UploadToSysopDirQ], fname);
	else if (dirnum == cfg.user_dir)
		SAFEPRINTF(str, text[UploadToUserDirQ], fname);
	else
		SAFEPRINTF3(str, text[UploadToCurDirQ], fname, cfg.lib[cfg.dir[dirnum]->lib]->sname
		            , cfg.dir[dirnum]->sname);
	if (!yesno(str))
		return false;
	action = NODE_ULNG;
	SAFECOPY(f.file_idx.name, fname);
	getfilepath(&cfg, &f, path);
	if (fexistcase(path)) {   /* File is on disk */
		if (!dir_op(dirnum) && online != ON_LOCAL) {        /* local users or sysops */
			bprintf(text[FileAlreadyThere], fname);
			return false;
		}
		if (!yesno(text[FileOnDiskAddQ]))
			return false;
	}
	char* ext = getfext(fname);
	SAFECOPY(str, cfg.dir[dirnum]->exts);
	j = strlen(str);
	for (i = 0; i < j; i += ch + 1) { /* Check extension of upload with allowable exts */
		p = strchr(str + i, ',');
		if (p != NULL)
			*p = 0;
		ch = (char)strlen(str + i);
		if (ext != NULL && stricmp(ext + 1, str + i) == 0)
			break;
	}
	if (j && i >= j) {
		bputs(text[TheseFileExtsOnly]);
		bputs(cfg.dir[dirnum]->exts);
		term->newline();
		if (!dir_op(dirnum))
			return false;
	}
	bputs(text[SearchingForDupes]);
	bool found = findfile(&cfg, dirnum, fname, NULL);
	bputs(text[SearchedForDupes]);
	if (found) {
		bprintf(text[FileAlreadyOnline], fname, lib_name(dirnum), dir_name(dirnum));
		return false;   /* File is already in database */
	}
	for (i = k = 0; i < usrlibs; i++) {
		progress(text[SearchingForDupes], i, usrlibs);
		for (j = 0; j < usrdirs[i]; j++, k++) {
			if (usrdir[i][j] == dirnum)
				continue;   /* we already checked this dir */
			if (cfg.dir[usrdir[i][j]]->misc & DIR_DUPES
			    && findfile(&cfg, usrdir[i][j], fname, NULL)) {
				bputs(text[SearchedForDupes]);
				bprintf(text[FileAlreadyOnline], fname, lib_name(usrdir[i][j]), dir_name(usrdir[i][j]));
				if (!dir_op(dirnum))
					return false;  /* File is in database for another dir */
			}
			if (msgabort(true)) {
				bputs(text[SearchedForDupes]);
				return false;
			}
		}
	}
	bputs(text[SearchedForDupes]);
	if (cfg.dir[dirnum]->misc & DIR_RATE) {
		sync();
		bputs(text[RateThisFile]);
		ch = getkey(K_ALPHA);
		if (!IS_ALPHA(ch) || sys_status & SS_ABORT)
			return false;
		term->newline();
		SAFEPRINTF(descbeg, text[Rated], toupper(ch));
	}
	if (cfg.dir[dirnum]->misc & DIR_ULDATE) {
		now = time(NULL);
		if (descbeg[0])
			strcat(descbeg, " ");
		SAFEPRINTF(str, "%s  ", datestr(now, tmp));
		strcat(descbeg, str);
	}
	if (cfg.dir[dirnum]->misc & DIR_MULT) {
		sync();
		if (!noyes(text[MultipleDiskQ])) {
			bputs(text[HowManyDisksTotal]);
			if ((int)(i = getnum(99)) < 2)
				return false;
			bputs(text[NumberOfFile]);
			if ((int)(j = getnum(i)) < 1)
				return false;
			if (j == 1)
				upload_lastdesc[0] = 0;
			if (i > 9)
				SAFEPRINTF2(descend, text[FileOneOfTen], j, i);
			else
				SAFEPRINTF2(descend, text[FileOneOfTwo], j, i);
		} else
			upload_lastdesc[0] = 0;
	}
	else
		upload_lastdesc[0] = 0;
	if (dirnum == cfg.user_dir) {  /* User to User transfer */
		bputs(text[EnterAfterLastDestUser]);
		while (dir_op(dirnum) || strListCount(dest_user_list) < cfg.max_userxfer) {
			bputs(text[SendFileToUser]);
			if (!getstr(str, LEN_ALIAS, cfg.uq & UQ_NOUPRLWR ? K_NONE:K_UPRLWR))
				break;
			user_t user;
			if ((user.number = finduser(str)) != 0) {
				if (!dir_op(dirnum) && user.number == useron.number) {
					bputs(text[CantSendYourselfFiles]);
					continue;
				}
				char usernum[16];
				SAFEPRINTF(usernum, "%u", user.number);
				if (strListFind(dest_user_list, usernum, /* case-sensitive: */ true) >= 0) {
					bputs(text[DuplicateUser]);
					continue;
				}
				getuserdat(&cfg, &user);
				if (!user_can_download(&cfg, cfg.user_dir, &user, /* client: */ NULL, /* reason: */ NULL)) {
					bprintf(text[UserWontBeAbleToDl], user.alias);
				} else {
					bprintf(text[UserAddedToDestList], user.alias, usernum);
					strListPush(&dest_user_list, usernum);
				}
			}
			else {
				term->newline();
			}
		}
		if (strListCount(dest_user_list) < 1)
			return false;
	}

	char fdesc[LEN_FDESC + 1] = "";
	bputs(text[EnterDescNow]);
	i = LEN_FDESC - (strlen(descbeg) + strlen(descend));
	getstr(upload_lastdesc, i, K_LINE | K_EDIT | K_AUTODEL | K_TRIM);
	if (sys_status & SS_ABORT) {
		strListFree(&dest_user_list);
		return false;
	}
	if (descend[0])      /* end of desc specified, so pad desc with spaces */
		safe_snprintf(fdesc, sizeof(fdesc), "%s%-*s%s", descbeg, i, upload_lastdesc, descend);
	else                /* no end specified, so string ends at desc end */
		safe_snprintf(fdesc, sizeof(fdesc), "%s%s", descbeg, upload_lastdesc);

	char tags[64] = "";
	if ((cfg.dir[dirnum]->misc & DIR_FILETAGS) && (text[TagFileQ][0] == 0 || !noyes(text[TagFileQ]))) {
		bputs(text[TagFilePrompt]);
		getstr(tags, sizeof(tags) - 1, K_EDIT | K_LINE | K_TRIM);
	}

	if (cfg.dir[dirnum]->misc & DIR_ANON && !(cfg.dir[dirnum]->misc & DIR_AONLY)
	    && (dir_op(dirnum) || useron.exempt & FLAG('A'))) {
		if (!noyes(text[AnonymousQ]))
			f.hdr.attr |= MSG_ANONYMOUS;
	}

	bool result = false;
	smb_hfield_str(&f, SMB_FILENAME, fname);
	smb_hfield_str(&f, SMB_FILEDESC, fdesc);
	if (strListCount(dest_user_list) > 0)
		smb_hfield_str(&f, RECIPIENTLIST, strListCombine(dest_user_list, tmp, sizeof(tmp), ","));
	if (tags[0])
		smb_hfield_str(&f, SMB_TAGS, tags);
	if (fexistcase(path)) {   /* File is on disk */
		result = uploadfile(&f);
	} else {
		xfer_prot_menu(XFER_UPLOAD, &useron, keys, sizeof keys);
		SAFECAT(keys, quit_key(str));
		sync();
		if (dirnum == cfg.user_dir || !cfg.max_batup)  /* no batch user to user xfers */
			mnemonics(text[ProtocolOrQuit]);
		else {
			mnemonics(text[ProtocolBatchOrQuit]);
			SAFECAT(keys, "B");
		}
		ch = (char)getkeys(keys, 0);
		if (ch == quit_key() || (sys_status & SS_ABORT))
			result = false;
		else if (ch == 'B') {
			if (batup_total() >= cfg.max_batup)
				bputs(text[BatchUlQueueIsFull]);
			else if (batch_file_exists(&cfg, useron.number, XFER_BATCH_UPLOAD, f.name))
				bprintf(text[FileAlreadyInQueue], fname);
			else if (batch_file_add(&cfg, useron.number, XFER_BATCH_UPLOAD, &f)) {
				bprintf(text[FileAddedToUlQueue]
				        , fname, batup_total(), cfg.max_batup);
				result = true;
			}
		} else {
			i = protnum(ch, XFER_UPLOAD);
			if (i < cfg.total_prots) {
				time_t elapsed = 0;
				protocol(cfg.prot[i], XFER_UPLOAD, path, nulstr, /* cd: */ true, /* autohang: */ true, &elapsed);
				if (!(cfg.dir[dirnum]->misc & DIR_ULTIME)) /* Don't deduct upload time */
					starttime += elapsed;
				result = uploadfile(&f);
				autohangup();
			}
		}
	}
	if (result == true && dirnum == cfg.user_dir) {
		snprintf(str, sizeof str, text[UserSentYouFile], useron.alias);
		for (i = 0; dest_user_list != NULL && dest_user_list[i] != NULL; ++i)
			putsmsg(matchuser(&cfg, dest_user_list[i], /* sysop_alias: */false), str);
	}
	smb_freefilemem(&f);
	strListFree(&dest_user_list);
	return result;
}

/****************************************************************************/
/* Checks directory for 'dir' and prompts user to enter description for     */
/* the files that aren't in the database.                                   */
/* Returns 1 if the user aborted, 0 if not.                                 */
/****************************************************************************/
bool sbbs_t::bulkupload(int dirnum)
{
	char    str[MAX_PATH + 1];
	char    path[MAX_PATH + 1];
	char    desc[LEN_FDESC + 1];
	smb_t   smb;
	file_t  f;
	DIR*    dir;
	DIRENT* dirent;

	memset(&f, 0, sizeof(f));
	f.dir = dirnum;
	bprintf(text[BulkUpload], cfg.lib[cfg.dir[dirnum]->lib]->sname, cfg.dir[dirnum]->sname);
	SAFECOPY(path, cfg.dir[dirnum]->path);

	int result = smb_open_dir(&cfg, &smb, dirnum);
	if (result != SMB_SUCCESS) {
		errormsg(WHERE, ERR_OPEN, smb.file, result, smb.last_error);
		return false;
	}
	action = NODE_ULNG;
	sync();
	str_list_t list = loadfilenames(&smb, ALLFILES, /* time_t */ 0, FILE_SORT_NATURAL, NULL);
	smb_close(&smb);
	dir = opendir(path);
	while (dir != NULL && (dirent = readdir(dir)) != NULL && !msgabort()) {
		char fname[SMB_FILEIDX_NAMELEN + 1];
		SAFEPRINTF2(str, "%s%s", path, dirent->d_name);
		if (isdir(str))
			continue;
#ifdef _WIN32
		/* Skip hidden/system files on Win32 */
		if (getfattr(str) & (_A_HIDDEN | _A_SYSTEM))
			continue;
#endif
		smb_fileidxname(dirent->d_name, fname, sizeof(fname));
		if (strListFind(list, fname, /* case-sensitive: */ FALSE) < 0) {
			smb_freemsgmem(&f);
			smb_hfield_str(&f, SMB_FILENAME, dirent->d_name);
			char tmp[64];
			bprintf(text[BulkUploadDescPrompt], format_filename(f.name, fname, 12, /* pad: */ FALSE), byte_estimate_to_str(flength(str), tmp, sizeof tmp, 1, 1));
			if (strcmp(f.name, fname) != 0)
				SAFECOPY(desc, f.name);
			else
				desc[0] = 0;
			getstr(desc, LEN_FDESC, K_LINE | K_EDIT | K_AUTODEL);
			if (sys_status & SS_ABORT)
				break;
			if (strcmp(desc, "-") == 0) /* don't add this file */
				continue;
			smb_hfield_str(&f, SMB_FILEDESC, desc);
			uploadfile(&f);
		}
	}
	if (dir != NULL)
		closedir(dir);
	strListFree(&list);
	smb_freemsgmem(&f);
	if (sys_status & SS_ABORT)
		return true;
	return false;
}

bool sbbs_t::recvfile(char *fname, char prot, bool autohang)
{
	char keys[128];
	char str[128];
	char ch;
	int  i;
	bool result = false;

	if (prot)
		ch = toupper(prot);
	else {
		xfer_prot_menu(XFER_UPLOAD, &useron, keys, sizeof keys);
		SAFECAT(keys, quit_key(str));
		mnemonics(text[ProtocolOrQuit]);
		ch = (char)getkeys(keys, 0);

		if (ch == quit_key() || sys_status & SS_ABORT)
			return false;
	}
	i = protnum(ch, XFER_UPLOAD);
	if (i < cfg.total_prots) {
		if (protocol(cfg.prot[i], XFER_UPLOAD, fname, fname, true, autohang) == 0)
			result = true;
		autohangup();
	}

	return result;
}
