/* Synchronet file contents display routines */

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
/* Views file with:                                                         */
/* (B)atch download, (V)iew file (E)xtended info, (Q)uit, or [Next]:        */
/* call with ext=1 for default to extended info, or 0 for file view         */
/* Returns -1 for Batch, 1 for Next, -2 for Previous, or 0 for Quit         */
/****************************************************************************/
int sbbs_t::viewfile(file_t* f, bool ext)
{
	char ch, str[256];
	char fname[13];     /* This is one of the only 8.3 filename formats left! (used for display purposes only) */
	format_filename(f->name, fname, sizeof(fname) - 1, /* pad: */ FALSE);

	curdirnum = f->dir;   /* for ARS */
	bool can_edit = dir_op(f->dir) || useron.exempt & FLAG('R') || stricmp(f->from, useron.alias) == 0;
	while (online) {
		clearabort();
		SAFEPRINTF(str, text[FileInfoPrompt], fname);
		if (ext) {
			showfileinfo(f);
			if (can_edit)
				SAFEPRINTF(str, text[FileInfoEditPrompt], fname);
		} else
			viewfilecontents(f);
		sync();
		mnemonics(str);
		SAFECOPY(str, "BEVQPN\b-\r");
		if (can_edit)
			SAFECAT(str, "D");
		ch = (char)getkeys(str, 0);
		if (ch == 'Q' || sys_status & SS_ABORT)
			return 0;
		switch (ch) {
			case 'B':
				addtobatdl(f);
				term->newline();
				return -1;
			case 'D':
				editfiledesc(f);
				continue;
			case 'E':
				if (ext && can_edit) {
					if (!editfiledesc(f))
						continue;
					editfileinfo(f);
				} else
					ext = true;
				continue;
			case 'V':
				ext = false;
				continue;
			case 'P':
			case '\b':
			case '-':
				return -2;
			case 'N':
			case CR:
				return 1;
		}
	}
	return 0;
}

/*****************************************************************************/
/*****************************************************************************/
bool sbbs_t::viewfile(const char* inpath)
{
	char path[MAX_PATH + 1];
	int  i;

	SAFECOPY(path, inpath);
	if (!fexistcase(path)) {
		bputs(text[FileNotFound]);
		return false;
	}
	for (i = 0; i < cfg.total_fviews; i++)
		if (file_type_match(path, cfg.fview[i]->ext) && chk_ar(cfg.fview[i]->ar, &useron, &client)) {
			break;
		}
	if (i >= cfg.total_fviews) {
		bprintf(text[NonviewableFile], getfname(path));
		return false;
	}
	fview_t* fview = cfg.fview[i];
	if ((i = external(cmdstr(fview->cmd, path, path, NULL, fview->ex_mode), fview->ex_mode)) != 0) {
		errormsg(WHERE, ERR_EXEC, cmdstr_output, i);    /* must have EX_SH to ^C */
		return false;
	}
	return true;
}

/****************************************************************************/
/****************************************************************************/
bool sbbs_t::viewfilecontents(file_t* f)
{
	char path[MAX_PATH + 1];

	getfilepath(&cfg, f, path);
	uint savedir = curdirnum;
	curdirnum = f->dir; /* for ARS */
	bool result = viewfile(path);
	curdirnum = savedir;
	return result;
}
