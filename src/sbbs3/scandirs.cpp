/* Synchronet file database scanning routines */

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

/****************************************************************************/
/* Used to scan single or multiple directories. 'mode' is the scan type.    */
/****************************************************************************/
void sbbs_t::scandirs(int mode)
{
	char keys[32];
	char ch, str[256] = "";
	int  s;
	int  i, k;

	if (cfg.scandirs_mod[0] && !scandirs_inside) {
		char cmdline[256];

		scandirs_inside = true;
		snprintf(cmdline, sizeof(cmdline), "%s 0 %u", cfg.scandirs_mod, mode);
		exec_bin(cmdline, &main_csi);
		scandirs_inside = false;
		return;
	}

	if (!usrlibs)
		return;
	mnemonics(text[DirLibOrAll]);
	SAFEPRINTF2(keys, "%s%c\r", text[DirLibKeys], all_key());
	ch = (char)getkeys(keys, 0);
	if (sys_status & SS_ABORT || ch == CR) {
		term->lncntr = 0;
		return;
	}
	if (ch != all_key()) {
		if (mode & FL_ULTIME) {            /* New file scan */
			bprintf(text[NScanHdr], timestr(ns_time));
		}
		else if (mode == FL_NO_HDR) {      /* Search for a string */
			if (!getfilespec(str))
				return;
		}
		else if (mode == FL_FIND) {    /* Find text in description */
			if (text[DisplayExtendedFileInfoQ][0] && !noyes(text[DisplayExtendedFileInfoQ]))
				mode |= FL_EXT;
			if (sys_status & SS_ABORT) {
				term->lncntr = 0;
				return;
			}
			bputs(text[SearchStringPrompt]);
			if (!getstr(str, 40, K_LINE | K_UPPER)) {
				term->lncntr = 0;
				return;
			}
		}
	}
	if (ch == text[DirLibKeys][0]) {
		if ((s = listfiles(usrdir[curlib][curdir[curlib]], str, 0, mode)) == -1)
			return;
		bputs(text[Scanned]);
		if (s > 1)
			bprintf(text[NFilesListed], s);
		else if (!s && !(mode & FL_ULTIME))
			bputs(text[FileNotFound]);
		return;
	}
	if (ch == text[DirLibKeys][1]) {
		k = 0;
		for (i = 0; i < usrdirs[curlib] && !msgabort(); i++) {
			progress(text[Scanning], i, usrdirs[curlib]);
			if (mode & FL_ULTIME   /* New-scan */
			    && (cfg.lib[usrlib[curlib]]->offline_dir == usrdir[curlib][i]
			        || cfg.dir[usrdir[curlib][i]]->misc & DIR_NOSCAN))
				continue;
			else if ((s = listfiles(usrdir[curlib][i], str, 0, mode)) == -1)
				return;
			else
				k += s;
		}
		progress(text[Done], i, usrdirs[curlib]);
		bputs(text[Scanned]);
		if (k > 1)
			bprintf(text[NFilesListed], k);
		else if (!k && !(mode & FL_ULTIME))
			bputs(text[FileNotFound]);
		return;
	}

	scanalldirs(mode);
}

/****************************************************************************/
/* Scan all directories in all libraries for files							*/
/****************************************************************************/
void sbbs_t::scanalldirs(int mode)
{
	char str[256] = "";
	int  s;
	int  i, j, k, d;

	if (cfg.scandirs_mod[0] && !scandirs_inside) {
		char cmdline[256];

		scandirs_inside = true;
		snprintf(cmdline, sizeof(cmdline), "%s 1 %u", cfg.scandirs_mod, mode);
		exec_bin(cmdline, &main_csi);
		scandirs_inside = false;
		return;
	}

	if (!usrlibs)
		return;
	k = 0;
	if (mode & FL_ULTIME) {            /* New file scan */
		bprintf(text[NScanHdr], timestr(ns_time));
	}
	else if (mode == FL_NO_HDR) {      /* Search for a string */
		if (!getfilespec(str))
			return;
	}
	else if (mode == FL_FIND) {    /* Find text in description */
		if (text[DisplayExtendedFileInfoQ][0] && !noyes(text[DisplayExtendedFileInfoQ]))
			mode |= FL_EXT;
		if (sys_status & SS_ABORT) {
			term->lncntr = 0;
			return;
		}
		bputs(text[SearchStringPrompt]);
		if (!getstr(str, 40, K_LINE | K_UPPER)) {
			term->lncntr = 0;
			return;
		}
	}
	unsigned total_dirs = 0;
	for (i = 0; i < usrlibs ; i++)
		total_dirs += usrdirs[i];
	for (i = d = 0; i < usrlibs; i++) {
		for (j = 0; j < usrdirs[i] && !msgabort(); j++, d++) {
			progress(text[Scanning], d, total_dirs);
			if (mode & FL_ULTIME /* New-scan */
			    && (cfg.lib[usrlib[i]]->offline_dir == usrdir[i][j]
			        || cfg.dir[usrdir[i][j]]->misc & DIR_NOSCAN))
				continue;
			else if ((s = listfiles(usrdir[i][j], str, 0, mode)) == -1) {
				bputs(text[Scanned]);
				return;
			}
			else
				k += s;
		}
		if (j < usrdirs[i])   /* aborted */
			break;
	}
	progress(text[Done], d, total_dirs);
	bputs(text[Scanned]);
	if (k > 1)
		bprintf(text[NFilesListed], k);
	else if (!k && !(mode & FL_ULTIME))
		bputs(text[FileNotFound]);
}

