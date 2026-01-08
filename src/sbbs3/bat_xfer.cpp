/* Synchronet batch file transfer functions */

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
/* This is the batch menu section                                           */
/****************************************************************************/
void sbbs_t::batchmenu()
{
	char       str[129], tmp2[250], ch;
	char       tmp[512];
	char       keys[32];
	int        sort = -1;
	int        n;
	int64_t    totalcdt, totalsize;
	str_list_t ini;
	str_list_t filenames;

	if (cfg.batxfer_mod[0] && !batchmenu_inside) {
		batchmenu_inside = true;
		exec_mod("batch transfer", cfg.batxfer_mod);
		batchmenu_inside = false;
		return;
	}

	if (batdn_total() < 1 && batup_total() < 1 && cfg.upload_dir == INVALID_DIR) {
		bputs(text[NoFilesInBatchQueue]);
		return;
	}
	if (useron.misc & (RIP) && !(useron.misc & EXPERT))
		menu("batchxfr");
	term->lncntr = 0;
	while (online && (cfg.upload_dir != INVALID_DIR || batdn_total() || batup_total())) {
		if (!(useron.misc & (EXPERT | RIP))) {
			clearabort();
			if (term->lncntr) {
				sync();
				term->newline();
				if (term->lncntr)          /* term->newline() or SYNC can cause pause */
					pause();
			}
			menu("batchxfr");
		}
		sync();
		bputs(text[BatchMenuPrompt]);
		SAFEPRINTF(keys, "CDLRU?\r%c", quit_key());
		ch = (char)getkeys(keys, 0);
		if (ch > ' ')
			logch(ch, 0);
		if (ch == quit_key() || ch == '\r') {    /* Quit */
			term->lncntr = 0;
			break;
		}
		switch (ch) {
			case '?':
				if (useron.misc & (EXPERT | RIP))
					menu("batchxfr");
				break;
			case 'C':
				if (batup_total() < 1) {
					bputs(text[UploadQueueIsEmpty]);
				} else {
					if (text[ClearUploadQueueQ][0] == 0 || !noyes(text[ClearUploadQueueQ])) {
						if (clearbatul())
							bputs(text[UploadQueueCleared]);
					}
				}
				if (batdn_total() < 1) {
					bputs(text[DownloadQueueIsEmpty]);
				} else {
					if (text[ClearDownloadQueueQ][0] == 0 || !noyes(text[ClearDownloadQueueQ])) {
						if (clearbatdl())
							bputs(text[DownloadQueueCleared]);
					}
				}
				break;
			case 'D':
				start_batch_download();
				break;
			case 'L':
			{
				ini = batch_list_read(&cfg, useron.number, XFER_BATCH_UPLOAD);
				filenames = iniGetSectionList(ini, NULL);
				if (strListCount(filenames)) {
					sort = !noyes(text[SortAlphaQ]);
					if (sort)
						strListSortAlphaCase(filenames);
					bputs(text[UploadQueueLstHdr]);
					for (size_t i = 0; filenames[i] != NULL && !msgabort(); ++i) {
						const char* filename = filenames[i];
						char        value[INI_MAX_VALUE_LEN];
						bprintf(text[UploadQueueLstFmt]
						        , i + 1, filename
						        , iniGetString(ini, filename, "desc", text[NoDescription], value));
					}
				} else
					bputs(text[UploadQueueIsEmpty]);

				iniFreeStringList(filenames);
				iniFreeStringList(ini);

				totalsize = 0;
				totalcdt = 0;
				ini = batch_list_read(&cfg, useron.number, XFER_BATCH_DOWNLOAD);
				filenames = iniGetSectionList(ini, NULL);
				if (strListCount(filenames)) {
					if (sort == -1)
						sort = !noyes(text[SortAlphaQ]);
					if (sort)
						strListSortAlphaCase(filenames);
					bputs(text[DownloadQueueLstHdr]);
					for (size_t i = 0; filenames[i] && !msgabort(); ++i) {
						const char* filename = filenames[i];
						file_t      f = {{}};
						if (!batch_file_load(&cfg, ini, filename, &f))
							continue;
						getfilesize(&cfg, &f);
						getfiletime(&cfg, &f);
						bprintf(text[DownloadQueueLstFmt], i + 1
						        , filename
						        , byte_estimate_to_str(f.cost, tmp, sizeof(tmp), 1, 1)
						        , byte_estimate_to_str(f.size, str, sizeof(str), 1, 1)
						        , cur_cps
						        ? sectostr((uint)(f.size / (uint64_t)cur_cps), tmp2)
						        : "??:??:??"
						        , datestr(f.time)
						        );
						totalsize += f.size;
						totalcdt += f.cost;
						smb_freefilemem(&f);
					}
					if (!msgabort())
						bprintf(text[DownloadQueueTotals]
						        , byte_estimate_to_str(totalcdt, tmp, sizeof(tmp), 1, 1)
						        , byte_estimate_to_str(totalsize, str, sizeof(tmp), 1, 1)
						        , cur_cps ? sectostr((uint)(totalsize / (uint64_t)cur_cps), tmp2) : "??:??:??");
				} else
					bputs(text[DownloadQueueIsEmpty]);
				iniFreeStringList(filenames);
				iniFreeStringList(ini);
				if (sort == TRUE) {
					batch_list_sort(&cfg, useron.number, XFER_BATCH_UPLOAD);
					batch_list_sort(&cfg, useron.number, XFER_BATCH_DOWNLOAD);
				}
				break;
			}
			case 'R':
				if ((n = batup_total()) > 0) {
					bprintf(text[RemoveWhichFromUlQueue], n);
					if (getstr(str, MAX_FILENAME_LEN, K_NONE) > 0)
						if (!batch_file_remove(&cfg, useron.number, XFER_BATCH_UPLOAD, str) && (n = atoi(str)) > 0)
							batch_file_remove_n(&cfg, useron.number, XFER_BATCH_UPLOAD, n - 1);
				}
				if ((n = batdn_total()) > 0) {
					bprintf(text[RemoveWhichFromDlQueue], n);
					if (getstr(str, MAX_FILENAME_LEN, K_NONE) > 0)
						if (!batch_file_remove(&cfg, useron.number, XFER_BATCH_DOWNLOAD, str) && (n = atoi(str)) > 0)
							batch_file_remove_n(&cfg, useron.number, XFER_BATCH_DOWNLOAD, n - 1);
				}
				break;
			case 'U':
				if (useron.rest & FLAG('U')) {
					bputs(text[R_Upload]);
					break;
				}
				batch_upload();
				break;
		}
	}
	delfiles(cfg.temp_dir, ALLFILES);
}

bool sbbs_t::batch_upload()
{
	char str[129];
	char keys[128];
	char ch;
	int  i;

	if (batup_total() < 1 && !okay_to_upload(cfg.upload_dir)) {
		bputs(text[UploadQueueIsEmpty]);
		return false;
	}
	xfer_prot_menu(XFER_BATCH_UPLOAD, &useron, keys, sizeof keys);
	SAFECAT(keys, quit_key(str));
	if (!create_batchup_lst())
		return false;
	sync();
	mnemonics(text[ProtocolOrQuit]);
	ch = (char)getkeys(keys, 0);
	if (ch == quit_key() || !online)
		return false;
	i = protnum(ch, XFER_BATCH_UPLOAD);
	if (i >= cfg.total_prots)
		return false;
	SAFEPRINTF(str, "%sBATCHUP.LST", cfg.node_dir);
	action = NODE_ULNG;
	sync();
	delfiles(cfg.temp_dir, ALLFILES);
	time_t elapsed = 0;
	protocol(cfg.prot[i], XFER_BATCH_UPLOAD, str, nulstr, /* cd: */ true, /* autohang: */ true, &elapsed);
	if (batup_total() < 1 && dirnum_is_valid(cfg.upload_dir) && !(cfg.dir[cfg.upload_dir]->misc & DIR_ULTIME))
		starttime += elapsed;
	bool   result = process_batch_upload_queue();
	delfiles(cfg.temp_dir, ALLFILES);
	autohangup();
	return result;
}

/****************************************************************************/
/****************************************************************************/
static const char* quoted_string(const char* str, char* buf, size_t maxlen)
{
	if (strchr(str, ' ') == NULL)
		return str;
	safe_snprintf(buf, maxlen, "\"%s\"", str);
	return buf;
}

/****************************************************************************/
/* Download files from batch queue                                          */
/****************************************************************************/
bool sbbs_t::start_batch_download()
{
	char ch;
	char tmp[32];
	char str[MAX_PATH + 1];
	char path[MAX_PATH + 1];
	char list[1024] = "";
	int  error;
	int  i, xfrprot;

	if (useron.rest & FLAG('D')) {     /* Download restriction */
		bputs(text[R_Download]);
		return false;
	}

	str_list_t ini = batch_list_read(&cfg, useron.number, XFER_BATCH_DOWNLOAD);

	size_t     file_count = iniGetSectionCount(ini, NULL);
	if (file_count < 1) {
		bputs(text[DownloadQueueIsEmpty]);
		iniFreeStringList(ini);
		return false;
	}
	if (useron.dtoday + file_count > user_downloads_per_day(&cfg, &useron)) {
		bputs(text[NoMoreDownloads]);
		iniFreeStringList(ini);
		return false;
	}
	str_list_t filenames = iniGetSectionList(ini, NULL);

	if (file_count == 1) {   // Only one file in the queue? Perform a non-batch (e.g. XMODEM) download
		const char* filename = filenames[0];
		file_t      f = {{}};
		bool        result = false;
		if (!batch_file_load(&cfg, ini, filename, &f)) {
			bprintf(text[FileDoesNotExist], filename);
			batch_file_remove(&cfg, useron.number, XFER_BATCH_DOWNLOAD, filename);
		} else {
			uint reason = R_Download;
			if (!user_can_download(&cfg, f.dir, &useron, &client, &reason)) {
				bputs(text[reason]);
				batch_file_remove(&cfg, useron.number, XFER_BATCH_DOWNLOAD, filename);
			}
			else if (f.cost > user_available_credits(&useron))
				bprintf(text[YouOnlyHaveNCredits]
				        , u64toac(user_available_credits(&useron), tmp));
			else if (!(cfg.dir[f.dir]->misc & DIR_TFREE) && gettimetodl(&cfg, &f, cur_cps) > timeleft
			         && !dir_op(f.dir) && !(useron.exempt & FLAG('T')))
				bputs(text[NotEnoughTimeToDl]);
			else {
				putnode_downloading(getfilesize(&cfg, &f));
				result = sendfile(&f, useron.prot, /* autohang: */ true);
			}
			if (result == true)
				batch_file_remove(&cfg, useron.number, XFER_BATCH_DOWNLOAD, f.name);
		}
		iniFreeStringList(ini);
		iniFreeStringList(filenames);
		smb_freefilemem(&f);
		return result;
	}

	uint64_t totalcdt = 0;
	for (size_t i = 0; filenames[i] != NULL; ++i) {
		const char* filename = filenames[i];
		progress(text[Scanning], i, file_count);
		file_t      f = {{}};
		if (!batch_file_load(&cfg, ini, filename, &f)) {
			bprintf(text[FileDoesNotExist], filename);
			batch_file_remove(&cfg, useron.number, XFER_BATCH_DOWNLOAD, filename);
			continue;
		}
		uint reason = R_Download;
		if (!user_can_download(&cfg, f.dir, &useron, &client, &reason)) {
			bputs(text[reason]);
			batch_file_remove(&cfg, useron.number, XFER_BATCH_DOWNLOAD, filename);
		} else
			totalcdt += f.cost;
		smb_freefilemem(&f);
	}
	bputs(text[Scanned]);
	if (totalcdt > user_available_credits(&useron)) {
		bprintf(text[YouOnlyHaveNCredits]
		        , u64toac(user_available_credits(&useron), tmp));
		iniFreeStringList(ini);
		iniFreeStringList(filenames);
		return false;
	}

	int64_t totalsize = 0;
	int64_t totaltime = 0;
	for (size_t i = 0; filenames[i] != NULL; ++i) {
		file_t f = {{}};
		if (!batch_file_get(&cfg, ini, filenames[i], &f))
			continue;
		totalsize += getfilesize(&cfg, &f);
		if (!(cfg.dir[f.dir]->misc & DIR_TFREE))
			totaltime += gettimetodl(&cfg, &f, cur_cps);
		char qpath[MAX_PATH + 1];
		SAFECAT(list, quoted_string(getfilepath(&cfg, &f, path), qpath, sizeof qpath));
		SAFECAT(list, " ");
		smb_freefilemem(&f);
	}
	iniFreeStringList(ini);
	iniFreeStringList(filenames);
	truncsp(list);

	if (!(useron.exempt & FLAG('T')) && !useron_is_sysop() && totaltime > (int64_t)timeleft) {
		bputs(text[NotEnoughTimeToDl]);
		return false;
	}
	i = protnum(useron.prot, XFER_BATCH_DOWNLOAD);
	if (i >= cfg.total_prots) {
		char keys[128];
		xfer_prot_menu(XFER_BATCH_DOWNLOAD, &useron, keys, sizeof keys);
		SAFECAT(keys, quit_key(str));
		sync();
		mnemonics(text[ProtocolOrQuit]);
		ch = (char)getkeys(keys, 0);
		if (ch == quit_key() || sys_status & SS_ABORT)
			return false;
		i = protnum(ch, XFER_BATCH_DOWNLOAD);
	}
	if (i >= cfg.total_prots || !create_batchdn_lst((cfg.prot[i]->misc & PROT_NATIVE) ? true:false)) {
		return false;
	}
	xfrprot = i;
#if 0 // NFB-TODO: Download events
	list = NULL;
	for (i = 0; i < batdn_total; i++) {
		curdirnum = batdn_dir[i];         /* for ARS */
		unpadfname(batdn_name[i], fname);
		if (cfg.dir[batdn_dir[i]]->seqdev) {
			term->lncntr = 0;
			SAFEPRINTF2(path, "%s%s", cfg.temp_dir, fname);
			if (!fexistcase(path)) {
				seqwait(cfg.dir[batdn_dir[i]]->seqdev);
				bprintf(text[RetrievingFile], fname);
				SAFEPRINTF2(str, "%s%s"
				            , cfg.dir[batdn_dir[i]]->path
				            , fname);
				mv(str, path, 1); /* copy the file to temp dir */
				if (getnodedat(cfg.node_num, &thisnode, true)) {
					thisnode.aux = 40; /* clear the seq dev # */
					putnodedat(cfg.node_num, &thisnode);
				}
				CRLF;
			}
		}
		else
			SAFEPRINTF2(path, "%s%s"
			            , batdn_alt[i] > 0 && batdn_alt[i] <= cfg.altpaths
			    ? cfg.altpath[batdn_alt[i] - 1]
			    : cfg.dir[batdn_dir[i]]->path
			            , fname);
		if (list == NULL)
			list_len = 0;
		else
			list_len = strlen(list) + 1; /* add one for ' ' */
		char* np;
		if ((np = (char*)realloc(list, list_len + strlen(path) + 1 /* add one for '\0'*/)) == NULL) {
			free(list);
			errormsg(WHERE, ERR_ALLOC, "list", list_len + strlen(path));
			return false;
		}
		list = np;
		if (!list_len)
			strcpy(list, path);
		else {
			strcat(list, " ");
			strcat(list, path);
		}
		for (j = 0; j < cfg.total_dlevents; j++) {
			if (stricmp(cfg.dlevent[j]->ext, batdn_name[i] + 9))
				continue;
			if (!chk_ar(cfg.dlevent[j]->ar, &useron, &client))
				continue;
			bputs(cfg.dlevent[j]->workstr);
			external(cmdstr(cfg.dlevent[j]->cmd, path, nulstr, NULL, cfg.dlevent[j]->ex_mode), cfg.dlevent[j]->ex_mode);
			clearline();
		}
	}
#endif

	SAFEPRINTF(str, "%sBATCHDN.LST", cfg.node_dir);
	putnode_downloading(totalsize);
	time_t elapsed = 0;
	error = protocol(cfg.prot[xfrprot], XFER_BATCH_DOWNLOAD, str, list, /* cid: */ false, /* autohang: */ true, &elapsed);
	if (cfg.prot[xfrprot]->misc & PROT_DSZLOG || !error)
		batch_download(xfrprot);
	if (batdn_total())
		notdownloaded(totalsize, elapsed);
	else
		downloadedbytes(totalsize, elapsed);
	autohangup();

	return true;
}

/****************************************************************************/
/* Creates the file BATCHDN.LST in the node directory. Returns true if      */
/* everything goes okay, false if not.                                      */
/****************************************************************************/
bool sbbs_t::create_batchdn_lst(bool native)
{
	char  path[MAX_PATH + 1];
	int   errval;

	SAFEPRINTF(path, "%sBATCHDN.LST", cfg.node_dir);
	FILE* fp = fopen(path, "wb");
	if (fp == NULL) {
		errormsg(WHERE, ERR_OPEN, path);
		return false;
	}
	bool        result = false;
	uint64_t    totalcdt = 0;
	const char* list_desc = "Batch Download File List";
	bprintf(text[CreatingFileList], list_desc);
	str_list_t  ini = batch_list_read(&cfg, useron.number, XFER_BATCH_DOWNLOAD);
	str_list_t  filenames = iniGetSectionList(ini, /* prefix: */ NULL);
	for (size_t i = 0; filenames[i] != NULL; ++i) {
		const char* filename = filenames[i];
		file_t      f = {};
		f.dir = batch_file_dir(&cfg, ini, filename);
		uint        reason = R_Download;
		if (!user_can_download(&cfg, f.dir, &useron, &client, &reason)) {
			bputs(text[reason]);
			batch_file_remove(&cfg, useron.number, XFER_BATCH_DOWNLOAD, filename);
			continue;
		}
		if (!loadfile(&cfg, f.dir, filename, &f, file_detail_index, &errval)) {
			errormsg(WHERE, "loading file", filename, errval);
			batch_file_remove(&cfg, useron.number, XFER_BATCH_DOWNLOAD, filename);
			continue;
		}
		getfilepath(&cfg, &f, path);
		if (!fexistcase(path)) {
			bprintf(text[FileDoesNotExist], path);
			batch_file_remove(&cfg, useron.number, XFER_BATCH_DOWNLOAD, filename);
		}
		else if (totalcdt + f.cost > user_available_credits(&useron)) {
			char tmp[128];
			bprintf(text[YouOnlyHaveNCredits]
			        , u64toac(user_available_credits(&useron), tmp));
			batch_file_remove(&cfg, useron.number, XFER_BATCH_DOWNLOAD, filename);
		}
		else {
#ifdef _WIN32
			if (!native) {
				char tmp[MAX_PATH + 1];
				GetShortPathName(path, tmp, sizeof(tmp));
				SAFECOPY(path, tmp);
			}
#endif
			fprintf(fp, "%s\r\n", path);
			totalcdt += f.cost;
			result = true;
		}
		smb_freefilemem(&f);
	}
	fclose(fp);
	iniFreeStringList(ini);
	iniFreeStringList(filenames);
	bprintf(text[CreatedFileList], list_desc);
	return result;
}

/****************************************************************************/
/* Creates the file BATCHUP.LST in the node directory. Returns true if      */
/* everything goes okay, false if not.                                      */
/* This list is not used by any protocols to date.                          */
/****************************************************************************/
bool sbbs_t::create_batchup_lst()
{
	char  path[MAX_PATH + 1];

	SAFEPRINTF(path, "%sBATCHUP.LST", cfg.node_dir);
	FILE* fp = fopen(path, "wb");
	if (fp == NULL) {
		errormsg(WHERE, ERR_OPEN, path);
		return false;
	}
	str_list_t ini = batch_list_read(&cfg, useron.number, XFER_BATCH_UPLOAD);
	str_list_t filenames = iniGetSectionList(ini, /* prefix: */ NULL);
	for (size_t i = 0; filenames[i] != NULL; ++i) {
		const char* filename = filenames[i];
		file_t      f = {{}};
		if (!batch_file_get(&cfg, ini, filename, &f))
			continue;
		fprintf(fp, "%s%s\r\n", cfg.dir[f.dir]->path, filename);
		smb_freefilemem(&f);
	}
	fclose(fp);
	iniFreeStringList(ini);
	iniFreeStringList(filenames);
	return true;
}

/****************************************************************************/
/* Processes files that were supposed to be received in the batch queue     */
/****************************************************************************/
bool sbbs_t::process_batch_upload_queue()
{
	char       src[MAX_PATH + 1];
	char       dest[MAX_PATH + 1];
	int        uploaded = 0;

	str_list_t ini = batch_list_read(&cfg, useron.number, XFER_BATCH_UPLOAD);
	str_list_t filenames = iniGetSectionList(ini, /* prefix: */ NULL);
	for (size_t i = 0; filenames[i] != NULL; ++i) {
		const char* filename = filenames[i];
		int         dir = batch_file_dir(&cfg, ini, filename);
		curdirnum = dir; /* for ARS */
		term->lncntr = 0; /* defeat pause */

		SAFEPRINTF2(src, "%s%s", cfg.temp_dir, filename);
		SAFEPRINTF2(dest, "%s%s", cfg.dir[dir]->path, filename);
		if (fexistcase(src) && fexistcase(dest)) { /* file's in two places */
			bprintf(text[FileAlreadyOnline], filename, lib_name(dir), dir_name(dir));
			remove(src);    /* this is the one received */
			continue;
		}

		if (fexist(src))
			mv(src, dest, /* copy: */ false);

		file_t f = {{}};
		if (!batch_file_get(&cfg, ini, filename, &f))
			continue;
		if (uploadfile(&f)) {
			batch_file_remove(&cfg, useron.number, XFER_BATCH_DOWNLOAD, filename);
			++uploaded;
		}
		smb_freefilemem(&f);
	}
	iniFreeStringList(filenames);
	iniFreeStringList(ini);

	if (cfg.upload_dir == INVALID_DIR) // no blind upload dir specified
		return uploaded > 0 && batup_total() == 0;

	DIR*    dir = opendir(cfg.temp_dir);
	DIRENT* dirent;
	while (dir != NULL && (dirent = readdir(dir)) != NULL) {
		SAFEPRINTF2(src, "%s%s", cfg.temp_dir, dirent->d_name);
		if (isdir(src))
			continue;
		if (!checkfname(dirent->d_name)) {
			bprintf(text[BadFilename], dirent->d_name);
			continue;
		}
		SAFEPRINTF2(dest, "%s%s", cfg.dir[cfg.upload_dir]->path, dirent->d_name);
		if (fexistcase(dest)) {
			bprintf(text[FileAlreadyOnline], dirent->d_name
			        , lib_name(cfg.upload_dir)
			        , dir_name(cfg.upload_dir));
			continue;
		}
		if (mv(src, dest, /* copy: */ false))
			continue;

		int x, y = 0;
		for (x = 0; x < usrlibs; x++) {
			progress(text[SearchingForDupes], x, usrlibs);
			for (y = 0; y < usrdirs[x]; y++)
				if (cfg.dir[usrdir[x][y]]->misc & DIR_DUPES
				    && findfile(&cfg, usrdir[x][y], dirent->d_name, NULL))
					break;
			if (y < usrdirs[x])
				break;
		}
		bputs(text[SearchedForDupes]);
		if (x < usrlibs) {
			bprintf(text[FileAlreadyOnline], dirent->d_name
			        , lib_name(usrdir[x][y]), dir_name(usrdir[x][y]));
		} else {
			file_t f = {{}};
			f.dir = cfg.upload_dir;
			smb_hfield_str(&f, SMB_FILENAME, dirent->d_name);
			if (uploadfile(&f))
				++uploaded;
			smb_freefilemem(&f);
		}
	}
	if (dir != NULL)
		closedir(dir);

	return uploaded > 0 && batup_total() == 0;
}

/****************************************************************************/
/* Processes files that were supposed to be sent from the batch queue       */
/* xfrprot is -1 if downloading files from within QWK (no DSZLOG)           */
/****************************************************************************/
void sbbs_t::batch_download(int xfrprot)
{
	FILE*      fp = batch_list_open(&cfg, useron.number, XFER_BATCH_DOWNLOAD, /* for_modify: */ true);
	if (fp == NULL)
		return;
	str_list_t ini = iniReadFile(fp);
	str_list_t filenames = iniGetSectionList(ini, /* prefix: */ NULL);

	for (size_t i = 0; filenames[i] != NULL; ++i) {
		char* filename = filenames[i];
		term->lncntr = 0;                               /* defeat pause */
		if (xfrprot == -1 || checkprotresult(cfg.prot[xfrprot], 0, filename)) {
			file_t f = {{}};
			if (!batch_file_load(&cfg, ini, filename, &f)) {
				errormsg(WHERE, "loading file", filename, i);
				continue;
			}
			iniRemoveSection(&ini, filename);
			if ((cfg.dir[f.dir]->misc & DIR_TFREE) && cur_cps)
				starttime += f.size / (ulong)cur_cps;
			downloadedfile(&f);
			smb_freefilemem(&f);
		}
	}
	iniWriteFile(fp, ini);
	iniCloseFile(fp);
	iniFreeStringList(ini);
	iniFreeStringList(filenames);
}

/****************************************************************************/
/* Adds a list of files to the batch download queue 						*/
/****************************************************************************/
void sbbs_t::batch_add_list(char *list)
{
	char   str[1024];
	char   path[MAX_PATH + 1];
	int    file;
	int    i, j, k;
	FILE * stream;
	file_t f;

	if ((stream = fnopen(&file, list, O_RDONLY)) != NULL) {
		bputs(text[SearchingAllLibs]);
		while (!feof(stream)) {
			checkline();
			if (!online)
				break;
			if (!fgets(str, sizeof(str) - 1, stream))
				break;
			truncnl(str);
			term->lncntr = 0;
			for (i = j = k = 0; i < usrlibs; i++) {
				for (j = 0; j < usrdirs[i]; j++, k++) {
					outchar('.');
					if (k && !(k % 5))
						bputs("\b\b\b\b\b     \b\b\b\b\b");
					if (loadfile(&cfg, usrdir[i][j], str, &f, file_detail_normal, NULL)) {
						if (fexist(getfilepath(&cfg, &f, path)))
							addtobatdl(&f);
						else
							bprintf(text[FileIsNotOnline], f.name);
						smb_freefilemem(&f);
						break;
					}
				}
				if (j < usrdirs[i])
					break;
			}
		}
		fclose(stream);
		remove(list);
		term->newline();
	}
}

/**************************************************************************/
/* Add file 'f' to batch download queue. Return 1 if successful, 0 if not */
/**************************************************************************/
bool sbbs_t::addtobatdl(file_t* f)
{
	char     str[256], tmp2[256];
	char     tmp[512];
	uint     i;
	uint     reason = R_Download;
	uint64_t totalcost, totalsize;
	uint64_t totaltime;

	if (!user_can_download(&cfg, f->dir, &useron, &client, &reason)) {
		bprintf(text[CantAddToQueue], f->name);
		bputs(text[reason]);
		return false;
	}
	if (getfilesize(&cfg, f) < 1) {
		bprintf(text[CantAddToQueue], f->name);
		bprintf(text[FileIsNotOnline], f->name);
		return false;
	}

	str_list_t ini = batch_list_read(&cfg, useron.number, XFER_BATCH_DOWNLOAD);
	if (iniSectionExists(ini, f->name)) {
		bprintf(text[FileAlreadyInQueue], f->name);
		iniFreeStringList(ini);
		return false;
	}

	bool       result = false;
	str_list_t filenames = iniGetSectionList(ini, /* prefix: */ NULL);
	size_t file_count = strListCount(filenames);
	if (useron.dtoday + file_count >= user_downloads_per_day(&cfg, &useron)) {
		bprintf(text[CantAddToQueue], f->name);
		bputs(text[NoMoreDownloads]);
	} else if (file_count >= cfg.max_batdn) {
		bprintf(text[CantAddToQueue], f->name);
		bputs(text[BatchDlQueueIsFull]);
	} else {
		totalcost = 0;
		totaltime = 0;
		totalsize = 0;
		for (i = 0; filenames[i] != NULL; ++i) {
			const char* filename = filenames[i];
			file_t      bf = {{}};
			if (!batch_file_load(&cfg, ini, filename, &bf))
				continue;
			totalcost += bf.cost;
			totalsize += getfilesize(&cfg, &bf);
			if (!(cfg.dir[bf.dir]->misc & DIR_TFREE) && cur_cps)
				totaltime += bf.size / (ulong)cur_cps;
			smb_freefilemem(&bf);
		}
		if (cfg.dir[f->dir]->misc & DIR_FREE)
			f->cost = 0L;
		if (!download_is_free(&cfg, f->dir, &useron, &client))
			totalcost += f->cost;
		if (totalcost > user_available_credits(&useron)) {
			bprintf(text[CantAddToQueue], f->name);
			bprintf(text[YouOnlyHaveNCredits], u64toac(user_available_credits(&useron), tmp));
		} else {
			totalsize += f->size;
			if (!(cfg.dir[f->dir]->misc & DIR_TFREE) && cur_cps)
				totaltime += f->size / (ulong)cur_cps;
			if (!(useron.exempt & FLAG('T')) && totaltime > timeleft) {
				bprintf(text[CantAddToQueue], f->name);
				bputs(text[NotEnoughTimeToDl]);
			} else {
				if (batch_file_add(&cfg, useron.number, XFER_BATCH_DOWNLOAD, f)) {
					bprintf(text[FileAddedToBatDlQueue]
					        , f->name, strListCount(filenames) + 1, cfg.max_batdn
					        , byte_estimate_to_str(totalcost, tmp, sizeof(tmp), 1, 1)
					        , byte_estimate_to_str(totalsize, tmp2, sizeof(tmp2), 1, 1)
					        , sectostr((uint)(totalsize / MAX((uint64_t)cur_cps, 1)), str));
					result = true;
				}
			}
		}
	}
	iniFreeStringList(ini);
	iniFreeStringList(filenames);
	return result;
}

bool sbbs_t::clearbatdl(void)
{
	return batch_list_clear(&cfg, useron.number, XFER_BATCH_DOWNLOAD);
}

bool sbbs_t::clearbatul(void)
{
	return batch_list_clear(&cfg, useron.number, XFER_BATCH_UPLOAD);
}

int64_t sbbs_t::batdn_bytes(void)
{
	int64_t total = 0;
	str_list_t ini = batch_list_read(&cfg, useron.number, XFER_BATCH_DOWNLOAD);
	str_list_t filenames = iniGetSectionList(ini, NULL);

	for (size_t i = 0; filenames[i]; ++i) {
		const char* filename = filenames[i];
		file_t      f{};
		if (batch_file_load(&cfg, ini, filename, &f))
			total += getfilesize(&cfg, &f);
	}
	iniFreeStringList(filenames);
	iniFreeStringList(ini);
	return total;
}

int64_t sbbs_t::batdn_cost(void)
{
	int64_t total = 0;
	str_list_t ini = batch_list_read(&cfg, useron.number, XFER_BATCH_DOWNLOAD);
	str_list_t filenames = iniGetSectionList(ini, NULL);

	for (size_t i = 0; filenames[i]; ++i) {
		const char* filename = filenames[i];
		file_t      f{};
		if (!batch_file_load(&cfg, ini, filename, &f))
			continue;
		if (!download_is_free(&cfg, f.dir, &useron, &client))
			total += f.cost;
	}
	iniFreeStringList(filenames);
	iniFreeStringList(ini);
	return total;
}

uint sbbs_t::batdn_time(void)
{
	int64_t total = 0;

	if (cur_cps) {
		str_list_t ini = batch_list_read(&cfg, useron.number, XFER_BATCH_DOWNLOAD);
		str_list_t filenames = iniGetSectionList(ini, NULL);

		for (size_t i = 0; filenames[i]; ++i) {
			const char* filename = filenames[i];
			file_t      f{};
			if (batch_file_load(&cfg, ini, filename, &f))
				total += getfilesize(&cfg, &f) / cur_cps;
		}
		iniFreeStringList(filenames);
		iniFreeStringList(ini);
		if (total > UINT_MAX)
			total = UINT_MAX;
	}
	return (uint)total;
}

size_t sbbs_t::batdn_total(void)
{
	return batch_file_count(&cfg, useron.number, XFER_BATCH_DOWNLOAD);
}

size_t sbbs_t::batup_total(void)
{
	return batch_file_count(&cfg, useron.number, XFER_BATCH_UPLOAD);
}
