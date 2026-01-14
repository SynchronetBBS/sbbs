/* Synchronet temp directory file transfer routines */

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

/*****************************************************************************/
/* Temp directory section. Files must be extracted here and both temp_uler   */
/* and temp_uler fields should be filled before entrance.                    */
/*****************************************************************************/
void sbbs_t::temp_xfer()
{
	if (exec_mod("temp transfer", cfg.tempxfer_mod) != 0)
		bprintf(text[DirectoryDoesNotExist], "temp (module)");
}

/****************************************************************************/
/* Creates a text file named NEWFILES.DAT in the temp directory that        */
/* all new files since p-date. Returns number of files in list.             */
/****************************************************************************/
uint sbbs_t::create_filelist(const char *name, int mode)
{
	char  tmpfname[MAX_PATH + 1];
	FILE* fp;
	int   i, j, d;
	int   l, k;

	if (online == ON_REMOTE)
		bprintf(text[CreatingFileList], name);
	SAFEPRINTF2(tmpfname, "%s%s", cfg.temp_dir, name);
	if ((fp = fopen(tmpfname, "ab")) == NULL) {
		errormsg(WHERE, ERR_OPEN, tmpfname, O_CREAT | O_WRONLY | O_APPEND);
		return 0;
	}
	k = 0;
	if (mode & FL_ULTIME) {
		fprintf(fp, "New files since: %s\r\n", timestr(ns_time));
	}
	unsigned total_dirs = 0;
	for (i = 0; i < usrlibs ; i++)
		total_dirs += usrdirs[i];
	for (i = j = d = 0; i < usrlibs; i++) {
		for (j = 0; j < usrdirs[i] && !msgabort(); j++, d++) {
			progress(text[Scanning], d, total_dirs);
			if (mode & FL_ULTIME /* New-scan */
			    && (cfg.lib[usrlib[i]]->offline_dir == usrdir[i][j]
			        || cfg.dir[usrdir[i][j]]->misc & DIR_NOSCAN))
				continue;
			l = listfiles(usrdir[i][j], nulstr, fp, mode);
			if (l == -1)
				break;
			k += l;
		}
		if (j < usrdirs[i])
			break;
	}
	if (msgabort(/* clear */ true)) {
		fclose(fp);
		bputs(text[Aborted]);
		remove(tmpfname);
		return 0;
	}
	progress(text[Done], d, total_dirs);
	if (k > 1) {
		fprintf(fp, "\r\n%d Files Listed.\r\n", k);
	}
	fclose(fp);
	if (k)
		bprintf(text[CreatedFileList], name);
	else {
		if (online == ON_REMOTE)
			bputs(text[NoFiles]);
		remove(tmpfname);
	}
	return k;
}

/****************************************************************************/
/* This function returns the command line for the temp file extension for	*/
/* current user online. 													*/
/****************************************************************************/
const char* sbbs_t::temp_cmd(int& ex_mode)
{
	int i;

	for (i = 0; i < cfg.total_fcomps; i++) {
		if (!stricmp(useron.tmpext, cfg.fcomp[i]->ext)
		    && chk_ar(cfg.fcomp[i]->ar, &useron, &client)) {
			ex_mode |= cfg.fcomp[i]->ex_mode;
			return cfg.fcomp[i]->cmd;
		}
	}
	errormsg(WHERE, ERR_CHK, "Unsupported temp archive file type", 0, useron.tmpext);
	return nulstr;
}
