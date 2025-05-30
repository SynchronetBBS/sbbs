/* Synchronet message creation routines */

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
#include "wordwrap.h"
#include "utf8.h"
#include "git_branch.h"
#include "git_hash.h"

#define MAX_LINE_LEN    ((term->cols - 1) + 2)

const char *quote_fmt = " > %.*s\r\n";
void quotestr(char *str);

/****************************************************************************/
/* Returns temporary message text filename (for message/text editors)		*/
/****************************************************************************/
char* sbbs_t::msg_tmp_fname(int xedit, char* path, size_t len)
{
	safe_snprintf(path, len, "%sINPUT.MSG", cfg.temp_dir);

	if (xedit && chk_ar(cfg.xedit[xedit - 1]->ar, &useron, &client)) {
		if (cfg.xedit[xedit - 1]->misc & QUICKBBS)
			safe_snprintf(path, len, "%sMSGTMP", cfg.node_dir); /* QuickBBS editors are dumb */
		if (cfg.xedit[xedit - 1]->misc & XTRN_LWRCASE)
			strlwr(getfname(path));
	}

	return path;
}

/****************************************************************************/
/****************************************************************************/
char* sbbs_t::quotes_fname(int xedit, char *path, size_t len)
{
	safe_snprintf(path, len, "%sQUOTES.TXT", cfg.node_dir);
	if (xedit
	    && chk_ar(cfg.xedit[xedit - 1]->ar, &useron, &client)
	    && (cfg.xedit[xedit - 1]->misc & XTRN_LWRCASE))
		strlwr(getfname(path));
	return path;
}

/****************************************************************************/
/****************************************************************************/
bool sbbs_t::quotemsg(smb_t* smb, smbmsg_t* msg, bool tails)
{
	char    fname[MAX_PATH + 1];
	char*   buf;
	char*   wrapped = NULL;
	FILE*   fp;
	ushort  useron_xedit = useron.xedit;
	uint8_t org_cols = TERM_COLS_DEFAULT;

	if (msg->columns != 0)
		org_cols = msg->columns;

	if (useron_xedit && !chk_ar(cfg.xedit[useron_xedit - 1]->ar, &useron, &client))
		useron_xedit = 0;

	quotes_fname(useron_xedit, fname, sizeof(fname));
	(void)removecase(fname);

	if ((fp = fopen(fname, "wb")) == NULL) {
		errormsg(WHERE, ERR_OPEN, fname, 0);
		return false;
	}

	if (useron_xedit > 0 && cfg.xedit[useron_xedit - 1]->type == XTRN_WWIV)
		fputs("#\r\n\r\n", fp); // WWIV adds 2 lines of metadata, BRedit checks for the # symbol

	bool  result = false;
	ulong mode = GETMSGTXT_PLAIN;
	if (tails)
		mode |= GETMSGTXT_TAILS;
	if ((buf = smb_getmsgtxt(smb, msg, mode)) != NULL) {
		if (useron_xedit && (cfg.xedit[useron_xedit - 1]->misc & KEEP_CTRL_A))
			strip_invalid_attr(buf);
		else
			remove_ctrl_a(buf, buf);
		truncsp(buf);
		BOOL is_utf8 = FALSE;
		if (!str_is_ascii(buf)) {
			if (smb_msg_is_utf8(msg)) {
				if ((term->charset() == CHARSET_UTF8)
				    && (!useron_xedit || (cfg.xedit[useron_xedit - 1]->misc & XTRN_UTF8)))
					is_utf8 = TRUE;
				else {
					utf8_to_cp437_inplace(buf);
				}
			} else { // CP437
				char* orgtxt;
				if ((term->charset() == CHARSET_UTF8)
				    && (!useron_xedit || (cfg.xedit[useron_xedit - 1]->misc & XTRN_UTF8))
				    && (orgtxt = strdup(buf)) != NULL) {
					is_utf8 = TRUE;
					size_t max = strlen(buf) * 4;
					char*  newbuf = (char*)realloc(buf, max + 1);
					if (newbuf != NULL) {
						buf = newbuf;
						cp437_to_utf8_str(orgtxt, buf, max, /* minval: */ '\x80');
					}
					free(orgtxt);
				}
			}
		}
		if (!useron_xedit || (useron_xedit && (cfg.xedit[useron_xedit - 1]->misc & QUOTEWRAP))) {
			int wrap_cols = 0;
			if (useron_xedit > 0)
				wrap_cols = cfg.xedit[useron_xedit - 1]->quotewrap_cols;
			if (wrap_cols == 0)
				wrap_cols = term->cols - 1;
			wrapped = ::wordwrap(buf, wrap_cols, org_cols - 1, /* handle_quotes: */ TRUE, is_utf8 ? P_UTF8 : 0);
		}
		if (wrapped != NULL) {
			fputs(wrapped, fp);
			free(wrapped);
		} else
			fputs(buf, fp);
		smb_freemsgtxt(buf);
		result = true;
	} else if (smb_getmsgdatlen(msg) > 2)
		errormsg(WHERE, ERR_READ, smb->file, smb_getmsgdatlen(msg));

	fclose(fp);
	return result;
}

/****************************************************************************/
/* Insure ends in LF or CRLF												*/
/****************************************************************************/
int sbbs_t::process_edited_text(char* buf, FILE* stream, int mode, unsigned* lines, unsigned maxlines)
{
	unsigned i, l;
	int      len = 0;
	ushort   useron_xedit = useron.xedit;
	char     lastch = 0;

	if (useron_xedit && !chk_ar(cfg.xedit[useron_xedit - 1]->ar, &useron, &client))
		useron_xedit = 0;

	for (l = i = 0; buf[l] && i < maxlines; l++) {
		if (buf[l] == '\r' && (mode & WM_STRIP_CR))
			continue;
		if ((uchar)buf[l] == FIDO_SOFT_CR && useron_xedit
		    && !(cfg.xedit[useron_xedit - 1]->misc & XTRN_UTF8)) {
			i++;
			switch (cfg.xedit[useron_xedit - 1]->soft_cr) {
				case XEDIT_SOFT_CR_EXPAND:
					if (mode & WM_STRIP_CR)
						len += fwrite("\n", 1, 1, stream);
					else
						len += fwrite(crlf, 1, 2, stream);
					lastch = '\n';
					continue;
				case XEDIT_SOFT_CR_STRIP:
					continue;
				case XEDIT_SOFT_CR_RETAIN:
				case XEDIT_SOFT_CR_UNDEFINED:
					break;
			}
		}
		/* Expand LF to CRLF? */
		if (buf[l] == LF && (!l || buf[l - 1] != CR) && (mode & WM_EXPANDLF)) {
			len += fwrite(crlf, 1, 2, stream);
			lastch = '\n';
			i++;
			continue;
		}
		if (buf[l] == CTRL_A) {
			/* Strip FidoNet Kludge Lines? */
			if (useron_xedit
			    && cfg.xedit[useron_xedit - 1]->misc & STRIPKLUDGE) {
				l++;
				char* kludge = buf + l;
				if (editor_details[0] == 0 && strncmp(kludge, "NOTE:", 5) == 0) {
					kludge += 5;
					SKIP_WHITESPACE(kludge);
					SAFECOPY(editor_details, kludge);
					truncstr(editor_details, "\r\n");
				}
				while (buf[l] != 0 && buf[l] != '\r' && buf[l] != '\n')
					l++;
				if (buf[l] == 0)
					break;
				if (buf[l] == '\r' && buf[l + 1] == '\n')
					l++;
				continue;
			}
			/* Convert invalid or dangerous Ctrl-A codes */
			if (!valid_ctrl_a_attr(buf[l + 1]))
				buf[l] = '@';
		}

		if (!(mode & (WM_EMAIL | WM_NETMAIL | WM_EDIT))
		    && (!l || buf[l - 1] == LF)
		    && buf[l] == '-' && buf[l + 1] == '-' && buf[l + 2] == '-'
		    && (buf[l + 3] == ' ' || buf[l + 3] == TAB || buf[l + 3] == CR))
			buf[l + 1] = '+';
		if (buf[l] == LF)
			i++;
		fputc(buf[l], stream);
		lastch = buf[l];
		len++;
	}

	if (buf[l]) {
		bprintf(text[NoMoreLines], i);
		buf[l] = 0;
	}
	if (lastch != '\n') {
		if (mode & WM_STRIP_CR)
			len += fwrite("\n", 1, 1, stream);
		else
			len += fwrite(crlf, 1, 2, stream);
		++i;
	}

	if (lines != NULL)
		*lines = i;
	return len;
}

/****************************************************************************/
/****************************************************************************/
int sbbs_t::process_edited_file(const char* src, const char* dest, int mode, unsigned* lines, unsigned maxlines)
{
	char* buf;
	long  len;
	FILE* fp;

	if ((len = (long)flength(src)) < 1)
		return -1;

	if ((buf = (char*)malloc(len + 1)) == NULL)
		return -2;

	if ((fp = fopen(src, "rb")) == NULL) {
		free(buf);
		return -3;
	}

	memset(buf, 0, len + 1);
	int rd = fread(buf, len, 1, fp);
	fclose(fp);
	if (rd != 1) {
		free(buf);
		return -4;
	}

	len = -5;
	if ((fp = fopen(dest, "wb")) != NULL) {
		len = process_edited_text(buf, fp, mode, lines, maxlines);
		fclose(fp);
	}
	free(buf);

	return len;
}

/****************************************************************************/
/* Creates a message (post or mail) using standard line editor. 'fname' is  */
/* is name of file to create, 'top' is a buffer to place at beginning of    */
/* message and 'title' is the title (70chars max) for the message.          */
/* 'dest' contains a text description of where the message is going.        */
/****************************************************************************/
bool sbbs_t::writemsg(const char *fname, const char *top, char *subj, int mode, int subnum
                      , const char *to, const char* from, const char** editor, const char** charset)
{
	char     str[256], quote[128], c, *buf, *p, *tp
	, useron_level;
	char     path[MAX_PATH + 1];
	char     msgtmp[MAX_PATH + 1];
	char     tagfile[MAX_PATH + 1];
	char     draft_desc[128];
	char     draft[MAX_PATH + 1];
	char     tmp[512];
	int      i, j, file, linesquoted = 0;
	int      length, qlen = 0, qtime = 0, ex_mode = 0;
	uint     l;
	FILE*    stream;
	FILE*    fp;
	unsigned lines;
	ushort   useron_xedit = useron.xedit;

	if (term->cols < TERM_COLS_MIN) {
		errormsg(WHERE, ERR_CHK, "columns (too narrow)", term->cols);
		return false;
	}

	if (top == NULL)
		top = "";

	if (useron_xedit && !chk_ar(cfg.xedit[useron_xedit - 1]->ar, &useron, &client))
		useron_xedit = 0;

	useron_level = useron.level;

	if (editor != NULL)
		*editor = NULL;

	if ((buf = (char*)malloc(strlen(top) + (cfg.level_linespermsg[useron_level] * MAX_LINE_LEN) + 1))
	    == NULL) {
		errormsg(WHERE, ERR_ALLOC, fname
		         , (cfg.level_linespermsg[useron_level] * MAX_LINE_LEN) + 1);
		return false;
	}

	if (mode & WM_NETMAIL ||
	    (!(mode & (WM_EMAIL | WM_NETMAIL)) && cfg.sub[subnum]->misc & SUB_PNET))
		mode |= WM_NOTOP;

	msg_tmp_fname(useron_xedit, msgtmp, sizeof(msgtmp));
	(void)removecase(msgtmp);
	SAFEPRINTF(tagfile, "%seditor.tag", cfg.temp_dir);
	(void)removecase(tagfile);
	SAFEPRINTF(draft_desc, "draft.%s.msg", subnum == INVALID_SUB ? "mail" : cfg.sub[subnum]->code);
	SAFEPRINTF3(draft, "%suser/%04u.%s", cfg.data_dir, useron.number, draft_desc);

	bool draft_restored = false;
	if (flength(draft) > 0 && (time(NULL) - fdate(draft)) < 48L * 60L * 60L && yesno("Unsaved draft message found. Use it")) {
		if (mv(draft, msgtmp, /* copy: */ true) == 0) {
			lprintf(LOG_NOTICE, "draft message restored: %s (%lu bytes)", draft, (ulong)flength(msgtmp));
			draft_restored = true;
			(void)removecase(quotes_fname(useron_xedit, str, sizeof(str)));
		} else
			lprintf(LOG_ERR, "ERROR %d (%s) restoring draft message: %s", errno, strerror(errno), draft);
	}
	else if (mode & WM_QUOTE && !(useron.rest & FLAG('J'))
	         && ((mode & (WM_EMAIL | WM_NETMAIL) && cfg.sys_misc & SM_QUOTE_EM)
	             || (!(mode & (WM_EMAIL | WM_NETMAIL)) && subnum != INVALID_SUB
	                 && cfg.sub[subnum]->misc & SUB_QUOTE))) {

		/* Quote entire message to MSGTMP or INPUT.MSG */

		if (useron_xedit && cfg.xedit[useron_xedit - 1]->misc & QUOTEALL) {
			if (!fexist(quotes_fname(useron_xedit, path, sizeof(path))))
				fexistcase(path);
			if ((stream = fnopen(NULL, path, O_RDONLY)) == NULL) {
				errormsg(WHERE, ERR_OPEN, path, O_RDONLY);
				free(buf);
				return false;
			}
			if (cfg.xedit[useron_xedit - 1]->type == XTRN_WWIV) { // 2 lines of metadata
				if (fgets(str, sizeof(str), stream) == NULL)
					errormsg(WHERE, ERR_READ, path, sizeof(str));
				if (fgets(str, sizeof(str), stream) == NULL)
					errormsg(WHERE, ERR_READ, path, sizeof(str));
			}
			if ((file = nopen(msgtmp, O_WRONLY | O_CREAT | O_TRUNC)) == -1) {
				errormsg(WHERE, ERR_OPEN, msgtmp, O_WRONLY | O_CREAT | O_TRUNC);
				free(buf);
				fclose(stream);
				return false;
			}

			while (!feof(stream) && !ferror(stream)) {
				if (!fgets(str, sizeof(str), stream))
					break;
				quotestr(str);
				SAFEPRINTF2(tmp, quote_fmt, term->cols - 4, str);
				if (write(file, tmp, strlen(tmp)) > 0)
					linesquoted++;
			}
			fclose(stream);
			close(file);
		}

		/* Quote nothing to MSGTMP or INPUT.MSG automatically */

		else if (useron_xedit && cfg.xedit[useron_xedit - 1]->misc & QUOTENONE)
			;

		else if (yesno(text[QuoteMessageQ])) {
			if (!fexist(quotes_fname(useron_xedit, path, sizeof(path))))
				fexistcase(path);
			if ((stream = fnopen(&file, path, O_RDONLY)) == NULL) {
				errormsg(WHERE, ERR_OPEN, path, O_RDONLY);
				free(buf);
				return false;
			}
			if (useron_xedit > 0 && cfg.xedit[useron_xedit - 1]->type == XTRN_WWIV) { // 2 lines of metadata
				if (fgets(str, sizeof(str), stream) == NULL)
					errormsg(WHERE, ERR_READ, path, sizeof(str));
				if (fgets(str, sizeof(str), stream) == NULL)
					errormsg(WHERE, ERR_READ, path, sizeof(str));
			}
			if ((file = nopen(msgtmp, O_WRONLY | O_CREAT | O_TRUNC)) == -1) {
				errormsg(WHERE, ERR_OPEN, msgtmp, O_WRONLY | O_CREAT | O_TRUNC);
				free(buf);
				fclose(stream);
				return false;
			}

			l = (long)ftell(stream);          /* l now points to start of message */

			while (online) {
				SAFEPRINTF(str, text[QuoteLinesPrompt], linesquoted ? text[Done] : text[All]);
				mnemonics(str);
				i = getstr(quote, 10, K_UPPER);
				if (sys_status & SS_ABORT) {
					fclose(stream);
					close(file);
					free(buf);
					return false;
				}
				if (!i && linesquoted)
					break;
				if (!i || quote[0] == all_key()) {                   /* Quote all */
					fseek(stream, l, SEEK_SET);
					while (!feof(stream) && !ferror(stream)) {
						if (!fgets(str, sizeof(str), stream))
							break;
						quotestr(str);
						SAFEPRINTF2(tmp, quote_fmt, term->cols - 4, str);
						if (write(file, tmp, strlen(tmp)) > 0)
							linesquoted++;
					}
					break;
				}
				if (quote[0] == list_key()) {
					fseek(stream, l, SEEK_SET);
					i = 1;
					term->newline();
					attr(LIGHTGRAY);
					while (!feof(stream) && !ferror(stream) && !msgabort()) {
						if (!fgets(str, sizeof(str), stream))
							break;
						quotestr(str);
						bprintf(P_AUTO_UTF8, "%4d: %.*s\r\n", i, (int)term->cols - 7, str);
						i++;
					}
					continue;
				}

				if (!IS_DIGIT(quote[0]))
					break;
				p = quote;
				while (p) {
					if (*p == ',' || *p == ' ')
						p++;
					i = atoi(p);
					if (!i)
						break;
					fseek(stream, l, SEEK_SET);
					j = 1;
					while (!feof(stream) && !ferror(stream) && j < i) {
						if (!fgets(tmp, sizeof(tmp), stream))
							break;
						j++; /* skip beginning */
					}
					tp = strchr(p, '-');   /* tp for temp pointer */
					if (tp) {         /* range */
						i = atoi(tp + 1);
						while (!feof(stream) && !ferror(stream) && j <= i) {
							if (!fgets(str, sizeof(str), stream))
								break;
							quotestr(str);
							SAFEPRINTF2(tmp, quote_fmt, term->cols - 4, str);
							if (write(file, tmp, strlen(tmp)) > 0)
								linesquoted++;
							j++;
						}
					}
					else {          /* one line */
						if (fgets(str, sizeof(str), stream)) {
							quotestr(str);
							SAFEPRINTF2(tmp, quote_fmt, term->cols - 4, str);
							if (write(file, tmp, strlen(tmp)) > 0)
								linesquoted++;
						}
					}
					p = strchr(p, ',');
					// if(!p) p=strchr(p,' ');  02/05/96 huh?
				}
			}

			fclose(stream);
			close(file);
		}
	}
	else {
		(void)removecase(quotes_fname(useron_xedit, str, sizeof(str)));
	}

	if (!online || sys_status & SS_ABORT) {
		free(buf);
		return false;
	}

	if (!(mode & (WM_EXTDESC | WM_SUBJ_RO))) {
		int max_title_len;

		if (mode & WM_FILE) {
			term->newline();
			bputs(text[Filename]);
		}
		else {
			bputs(text[SubjectPrompt]);
		}
		max_title_len = term->cols - term->column - 1;
		if (max_title_len > LEN_TITLE)
			max_title_len = LEN_TITLE;
		if (draft_restored)
			user_get_property(&cfg, useron.number, draft_desc, "subject", subj, max_title_len);
		if (!getstr(subj, max_title_len, mode & WM_FILE ? K_LINE | K_TRIM : K_LINE | K_EDIT | K_AUTODEL | K_TRIM | K_UTF8)
		    && useron_level && useron.logons) {
			free(buf);
			return false;
		}
		if ((mode & WM_FILE) && !checkfname(subj)) {
			free(buf);
			bprintf(text[BadFilename], subj);
			return false;
		}
		if (!(mode & (WM_EMAIL | WM_NETMAIL)) && cfg.sub[subnum]->misc & SUB_QNET
		    && !useron_is_sysop()
		    && (!stricmp(subj, "DROP") || !stricmp(subj, "ADD")
		        || !strnicmp(to, "SBBS", 4))) {
			free(buf);   /* Users can't post DROP or ADD in QWK netted subs */
			return false; /* or messages to "SBBS" */
		}
	}

	if (!online || sys_status & SS_ABORT) {
		free(buf);
		return false;
	}

	editor_details[0] = 0;

	if (console & CON_RAW_IN) {

		if (editor != NULL)
			*editor = "Synchronet writemsg " GIT_BRANCH "/" GIT_HASH;

		bprintf(text[EnterMsgNowRaw]
		        , (ulong)cfg.level_linespermsg[useron_level] * MAX_LINE_LEN);
		rioctl(IOFI);       // flush input buffer (e.g. stray LFs in input)
		if (top[0] && !(mode & WM_NOTOP)) {
			strcpy((char *)buf, top);
			l = strlen((char *)buf);
			if (l < 4 || strcmp(buf + l - 4, "\r\n\r\n") != 0) {
				strcat((char *)buf, crlf);
				l += 2;
			}
		}
		else
			l = 0;
		while (l < (ulong)(cfg.level_linespermsg[useron_level] * MAX_LINE_LEN)) {
			c = getkey(0);
			if (sys_status & SS_ABORT) {  /* Ctrl-C */
				free(buf);
				return false;
			}
			if ((c == ESC || c == CTRL_A) && useron.rest & FLAG('A')) /* ANSI restriction */
				continue;
			if (c == BEL && useron.rest & FLAG('B'))   /* Beep restriction */
				continue;
			if (!(console & CON_RAW_IN))   /* Ctrl-Z was hit */
				break;
			outchar(c);
			buf[l++] = c;
		}
		console &= ~CON_RAW_IN; // Turn off raw input mode in case the input exceeded length limit
		buf[l] = 0;
		if (l == (ulong)cfg.level_linespermsg[useron_level] * MAX_LINE_LEN)
			bputs(text[OutOfBytes]);
	}

	else if (useron_xedit) {

		if (editor != NULL)
			*editor = cfg.xedit[useron_xedit - 1]->name;
		if (!str_is_ascii(subj)) {
			if (utf8_str_is_valid(subj)) {
				if ((term->charset() != CHARSET_UTF8) || !(cfg.xedit[useron_xedit - 1]->misc & XTRN_UTF8)) {
					utf8_to_cp437_inplace(subj);
				}
			} else { // CP437
				if ((term->charset() == CHARSET_UTF8) && (cfg.xedit[useron_xedit - 1]->misc & XTRN_UTF8)) {
					cp437_to_utf8_str(subj, str, sizeof(str) - 1, /* minval: */ '\x80');
					safe_snprintf(subj, LEN_TITLE + 1, "%s", str);
				}
			}
		}
		editor_inf(useron_xedit, to, from, subj, mode, subnum_is_valid(subnum) ? cfg.sub[subnum]->sname : nulstr, tagfile);
		if (cfg.xedit[useron_xedit - 1]->type) {
			gettimeleft();
			xtrndat(mode & WM_ANON ? text[Anonymous]:from, cfg.node_dir, cfg.xedit[useron_xedit - 1]->type
			        , timeleft, cfg.xedit[useron_xedit - 1]->misc);
		}

		if (cfg.xedit[useron_xedit - 1]->misc & XTRN_STDIO) {
			ex_mode |= EX_STDIO;
			if (cfg.xedit[useron_xedit - 1]->misc & WWIVCOLOR)
				ex_mode |= EX_WWIV;
		}
		if (cfg.xedit[useron_xedit - 1]->misc & XTRN_NATIVE)
			ex_mode |= EX_NATIVE;
		if (cfg.xedit[useron_xedit - 1]->misc & XTRN_SH)
			ex_mode |= EX_SH;

		if (!draft_restored) {
			if (!linesquoted)
				(void)removecase(msgtmp);
			else {
				qlen = (long)flength(msgtmp);
				qtime = (long)fdate(msgtmp);
			}
		}

		cls();
		rioctl(IOCM | PAUSE | ABORT);
		auto        savsubnum = smb.subnum;
		smb.subnum = subnum;    /* Allow JS msgeditors to use bbs.smb_sub* */
		const char* cmd = cmdstr(cfg.xedit[useron_xedit - 1]->rcmd, msgtmp, nulstr, NULL, ex_mode);
		int         result = external(cmd, ex_mode, cfg.node_dir);
		smb.subnum = savsubnum;
		lprintf(LOG_DEBUG, "'%s' returned %d", cmd, result);
		rioctl(IOSM | PAUSE | ABORT);

		checkline();
		if (!online && (flength(msgtmp) > 0) && (useron.number > 0) && !(useron.rest & FLAG('G'))) { // save draft message due to disconnection
			if (mv(msgtmp, draft, /* copy: */ true) == 0) {
				user_set_property(&cfg, useron.number, draft_desc, "subject", subj);
				user_set_time_property(&cfg, useron.number, draft_desc, "created", time(NULL));
				lprintf(LOG_NOTICE, "draft message saved: %s (%lu bytes)", draft, (ulong)flength(draft));
			} else
				lprintf(LOG_ERR, "ERROR %d (%s) saving draft message: %s", errno, strerror(errno), draft);
		}

		if (result != EXIT_SUCCESS || !fexistcase(msgtmp) || !online
		    || (linesquoted && qlen == flength(msgtmp) && qtime == fdate(msgtmp))) {
			free(buf);
			return false;
		}
		SAFEPRINTF(str, "%sRESULT.ED", cfg.node_dir);
		if (!(mode & (WM_EXTDESC | WM_FILE))
		    && fexistcase(str)) {
			if ((fp = fopen(str, "r")) != NULL) {
				if (fgets(str, sizeof(str), fp) != NULL) {
					str[0] = 0;
					if (fgets(str, sizeof(str), fp) != NULL) {
						truncsp(str);
						if (str[0] && !(mode & WM_SUBJ_RO))
							safe_snprintf(subj, LEN_TITLE + 1, "%s", str);
						if (fgets(editor_details, sizeof(editor_details), fp) != NULL) {
							truncsp(editor_details);
						}
					}
				}
				fclose(fp);
			}
		}

		buf[0] = 0;
		if (!(mode & WM_NOTOP))
			strcpy((char *)buf, top);
		if ((file = nopen(msgtmp, O_RDONLY)) == -1) {
			errormsg(WHERE, ERR_OPEN, msgtmp, O_RDONLY);
			free(buf);
			return false;
		}
		length = (long)filelength(file);
		if (length < 0) {
			close(file);
			errormsg(WHERE, ERR_LEN, msgtmp, length);
			free(buf);
			return false;
		}
		l = strlen((char *)buf);    /* reserve space for top and terminating null */
		/* truncate if too big */
		if (length > (long)((cfg.level_linespermsg[useron_level] * MAX_LINE_LEN) - (l + 1))) {
			length = (cfg.level_linespermsg[useron_level] * MAX_LINE_LEN) - (l + 1);
			bputs(text[OutOfBytes]);
		}
		long rd = read(file, buf + l, length);
		close(file);
		if (rd != length) {
			errormsg(WHERE, ERR_READ, msgtmp, length);
			free(buf);
			return false;
		}
		// remove(msgtmp); 	   /* no need to save the temp input file */
		buf[l + length] = 0;
	}
	else {

		if (editor != NULL)
			*editor = "Synchronet msgeditor " GIT_BRANCH "/" GIT_HASH;

		buf[0] = 0;
		if (linesquoted || draft_restored) {
			if ((file = nopen(msgtmp, O_RDONLY)) != -1) {
				length = (long)filelength(file);
				l = length > (int)(cfg.level_linespermsg[useron_level] * MAX_LINE_LEN) - 1
				    ? (cfg.level_linespermsg[useron_level] * MAX_LINE_LEN) - 1 : length;
				if (read(file, buf, l) != (ssize_t)l)
					l = 0;
				buf[l] = 0;
				close(file);
				// remove(msgtmp);
			}
		}
		if (!(msgeditor((char *)buf, mode & WM_NOTOP ? nulstr : top, subj, cfg.level_linespermsg[useron_level], MAX_LINE_LEN))) {
			if (!online) {
				FILE* fp = fopen(draft, "wb");
				if (fp == NULL)
					errormsg(WHERE, ERR_CREATE, draft, O_WRONLY);
				else {
					fputs(buf, fp);
					fclose(fp);
					user_set_property(&cfg, useron.number, draft_desc, "subject", subj);
				}
			}
			free(buf);  /* Assertion here Dec-17-2003, think I fixed in block above (rev 1.52) */
			return false;
		}
	}

	now = time(NULL);
	bputs(text[Saving]);
	(void)removecase(fname);
	if ((stream = fnopen(NULL, fname, O_WRONLY | O_CREAT | O_TRUNC)) == NULL) {
		errormsg(WHERE, ERR_OPEN, fname, O_WRONLY | O_CREAT | O_TRUNC);
		free(buf);
		return false;
	}
	l = process_edited_text(buf, stream, mode | WM_EXPANDLF, &lines, cfg.level_linespermsg[useron_level]);
	if (editor_details[0] && editor != NULL)
		*editor = editor_details;
	bool utf8 = !str_is_ascii(buf) && utf8_str_is_valid(buf);
	if (charset != NULL) {
		if (utf8)
			*charset = FIDO_CHARSET_UTF8;
	}

	if (!(mode & (WM_EXTDESC | WM_ANON))) {
		/* Signature file */
		if ((subnum == INVALID_SUB && cfg.msg_misc & MM_EMAILSIG)
		    || (subnum != INVALID_SUB && !(cfg.sub[subnum]->misc & SUB_NOUSERSIG))) {
			bool got_sig = false;
			if (subnum_is_valid(subnum)) {
				snprintf(str, sizeof str, "%suser/%04u.%s.sig", cfg.data_dir, useron.number, cfg.sub[subnum]->code);
				got_sig = fexistcase(str);
			}
			if (!got_sig)
				SAFEPRINTF2(str, "%suser/%04u.sig", cfg.data_dir, useron.number);
			FILE* sig;
			if (fexistcase(str) && (sig = fopen(str, "r")) != NULL) {
				while (!feof(sig)) {
					if (!fgets(str, sizeof(str), sig))
						break;
					truncnl(str);
					if (utf8) {
						char buf[sizeof(str) * 4];
						cp437_to_utf8_str(str, buf, sizeof(buf) - 1, /* minval: */ '\x02');
						l += fprintf(stream, "%s\r\n", buf);
					} else
						l += fprintf(stream, "%s\r\n", str);
					lines++;        /* line counter */
				}
				fclose(sig);
			}
		}
		if (fexistcase(tagfile)) {
			FILE* tag;

			if ((tag = fopen(tagfile, "r")) != NULL) {
				while (!feof(tag)) {
					if (!fgets(str, sizeof(str), tag))
						break;
					truncsp(str);
					if (utf8) {
						char buf[sizeof(str) * 4];
						cp437_to_utf8_str(str, buf, sizeof(buf) - 1, /* minval: */ '\x02');
						l += fprintf(stream, "%s\r\n", buf);
					} else
						l += fprintf(stream, "%s\r\n", str);
					lines++;        /* line counter */
				}
				fclose(tag);
			}
		}
	}

	(void)remove(draft);
	fclose(stream);
	free((char *)buf);
	bprintf(text[SavedNBytes], l, lines);
	return true;
}

void sbbs_t::editor_info_to_msg(smbmsg_t* msg, const char* editor, const char* charset)
{
	smb_hfield_string(msg, SMB_EDITOR, editor);
	smb_hfield_string(msg, FIDOCHARSET, charset);

	ushort useron_xedit = useron.xedit;

	if (useron_xedit > 0 && !chk_ar(cfg.xedit[useron_xedit - 1]->ar, &useron, &client))
		useron_xedit = 0;

	if (editor == NULL || useron_xedit == 0 || (cfg.xedit[useron_xedit - 1]->misc & SAVECOLUMNS))
		smb_hfield_bin(msg, SMB_COLUMNS, term->cols);

	if (!str_is_ascii(msg->subj) && utf8_str_is_valid(msg->subj))
		msg->hdr.auxattr |= MSG_HFIELDS_UTF8;
}

/****************************************************************************/
/****************************************************************************/
/* Modify 'str' to for quoted format. Remove ^A codes, etc.                 */
/****************************************************************************/
void quotestr(char *str)
{
	if (str != NULL) {
		truncsp(str);
		remove_ctrl_a(str, str);
	}
}

/****************************************************************************/
/****************************************************************************/
void sbbs_t::editor_inf(int xeditnum, const char *to, const char* from, const char *subj, int mode
                        , const char* msgarea, const char* tagfile)
{
	char  path[MAX_PATH + 1];
	FILE* fp;

	xeditnum--;

	SAFEPRINTF(path, "%sresult.ed", cfg.node_dir);
	(void)removecase(path);
	if (cfg.xedit[xeditnum]->misc & QUICKBBS) {
		SAFEPRINTF2(path, "%s%s", cfg.node_dir, cfg.xedit[xeditnum]->misc & XTRN_LWRCASE ? "msginf":"MSGINF");
		(void)removecase(path);
		if ((fp = fopen(path, "wb")) == NULL) {
			errormsg(WHERE, ERR_OPEN, path, O_WRONLY | O_CREAT | O_TRUNC);
			return;
		}
		fprintf(fp, "%s\r\n%s\r\n%s\r\n%u\r\n%s\r\n%s\r\n"
		        , mode & WM_ANON ? text[Anonymous]:from, to, subj, 1
		        , mode & WM_NETMAIL ? "NetMail"
		        :mode & WM_EMAIL ? "Electronic Mail" : msgarea
		        , mode & WM_PRIVATE ? "YES":"NO");
		/* the 7th line (the tag-line file) is a Synchronet extension, for SlyEdit */
		if ((mode & WM_EXTDESC) == 0 && tagfile != NULL)
			fprintf(fp, "%s", tagfile);
		fprintf(fp, "\r\n");
		fprintf(fp, "%s\r\n", cfg.xedit[xeditnum]->misc & XTRN_UTF8 ? "UTF-8" : "CP437");
		fclose(fp);
	}
	else {
		SAFEPRINTF2(path, "%s%s", cfg.node_dir, cfg.xedit[xeditnum]->misc & XTRN_LWRCASE ? "editor.inf" : "EDITOR.INF");
		(void)removecase(path);
		if ((fp = fopen(path, "wb")) == NULL) {
			errormsg(WHERE, ERR_OPEN, path, O_WRONLY | O_CREAT | O_TRUNC);
			return;
		}
		fprintf(fp, "%s\r\n%s\r\n%u\r\n%s\r\n%s\r\n%u\r\n"
		        , subj
		        , to
		        , useron.number
		        , mode & WM_ANON ? text[Anonymous]:from
		        , useron.name
		        , useron.level);
		fclose(fp);
	}
}



/****************************************************************************/
/* Removes from file 'str' every LF terminated line that starts with 'str2' */
/* That is divisable by num. Function skips first 'skip' number of lines    */
/****************************************************************************/
void sbbs_t::removeline(char *str, char *str2, char num, char skip)
{
	char*  buf;
	size_t slen;
	int    i, file;
	long   l = 0, flen;
	FILE * stream;

	if ((file = nopen(str, O_RDONLY)) == -1) {
		errormsg(WHERE, ERR_OPEN, str, O_RDONLY);
		return;
	}
	flen = (long)filelength(file);
	if (flen < 0) {
		close(file);
		errormsg(WHERE, ERR_CHK, str, flen);
		return;
	}
	slen = strlen(str2);
	if ((buf = (char *)malloc(flen)) == NULL) {
		close(file);
		errormsg(WHERE, ERR_ALLOC, str, flen);
		return;
	}
	if (read(file, buf, flen) != flen) {
		close(file);
		errormsg(WHERE, ERR_READ, str, flen);
		free(buf);
		return;
	}
	close(file);
	if ((stream = fnopen(&file, str, O_WRONLY | O_TRUNC)) == NULL) {
		errormsg(WHERE, ERR_OPEN, str, O_WRONLY | O_TRUNC);
		free(buf);
		return;
	}
	for (i = 0; l < flen && i < skip; l++) {
		fputc(buf[l], stream);
		if (buf[l] == LF)
			i++;
	}
	while (l < flen) {
		if (!strncmp((char *)buf + l, str2, slen)) {
			for (i = 0; i < num && l < flen; i++) {
				while (l < flen && buf[l] != LF) l++;
				l++;
			}
		}
		else {
			for (i = 0; i < num && l < flen; i++) {
				while (l < flen && buf[l] != LF) fputc(buf[l++], stream);
				fputc(buf[l++], stream);
			}
		}
	}
	fclose(stream);
	free((char *)buf);
}

/*****************************************************************************/
/* The Synchronet editor.                                                    */
/* Returns the number of lines edited.                                       */
/*****************************************************************************/
uint sbbs_t::msgeditor(char *buf, const char *top, char *title, uint maxlines, uint max_line_len)
{
	int        i, j;
	uint       line, lines = 0;
	char       strin[TERM_COLS_MAX + 1];
	char       tmp[512];
	str_list_t str;
	long       pmode = P_SAVEATR | P_NOATCODES | P_AUTO_UTF8;

	if (term->cols < TERM_COLS_MIN) {
		errormsg(WHERE, ERR_CHK, "columns (too narrow)", term->cols);
		return 0;
	}

	rioctl(IOCM | ABORT);
	rioctl(IOCS | ABORT);

	if ((str = strListDivide(NULL, buf, "\n")) == NULL) {
		errormsg(WHERE, ERR_ALLOC, "msgeditor", strlen(buf));
		return 0;
	}
	lines = strListCount(str);
	while (lines > maxlines)
		free(str[--lines]);
	str[lines] = NULL;
	strListTruncateTrailingLineEndings(str);
	if (lines)
		bprintf("\r\nMessage editor: Read in %d lines\r\n", lines);
	bprintf(text[EnterMsgNow], maxlines);

	if (!menu("msgtabs", P_NOERROR)) {
		for (unsigned u = 0; u < (term->cols - 1); u++) {
			if (u % EDIT_TABSIZE || !u)
				outchar('-');
			else
				outchar('+');
		}
		term->newline();
	}
	putmsg(top, pmode);
	for (line = 0; line < lines && !msgabort(); line++) { /* display lines in buf */
		putmsg(str[line], pmode);
		term->cleartoeol();  /* delete to end of line */
		term->newline();
	}
	sync();
	rioctl(IOSM | ABORT);
	while (online) {
		if ((int)line < 0)
			line = 0;
		if ((int)line > (int)maxlines - 10) {
			if (line >= maxlines)
				bprintf(text[NoMoreLines], line);
			else
				bprintf(text[OnlyNLinesLeft], maxlines - line);
		}
		char prot = 0;
		do {
			if (str[line] != NULL)
				SAFECOPY(strin, str[line]);
			else
				strin[0] = 0;
			if (line < 1)
				term->carriage_return();
			ulong prev_con = console;
			int   kmode = K_WORDWRAP | K_MSG | K_EDIT | K_NOCRLF | K_USEOFFSET;
			if (line)
				kmode |= K_LEFTEXIT;
			if (str[line] != NULL)
				kmode |= K_RIGHTEXIT;
			getstr(strin, term->cols - 1, kmode);
			if ((prev_con & CON_DELETELINE) /* Ctrl-X/ZDLE */ && strncmp(strin, "B00", 3) == 0) {
				strin[0] = 0;
				prot = 'Z';
				goto upload;
			}
		} while (console & CON_UPARROW && !line && online);

		if (sys_status & SS_ABORT)
			continue;

		if (console & (CON_UPARROW | CON_LEFTARROW | CON_BACKSPACE)) {
			if (console & CON_BACKSPACE && strin[0] == 0) {
				strListRemove(&str, line);
				for (i = line; str[i]; i++) {
					putmsg(str[i], pmode);
					term->cleartoeol();
					term->newline();
				}
				term->clearline();
				if (line)
					--line;
				term->cursor_up(i - line);
				continue;
			} else if (str[line] == NULL) {
				if (strin[0] != 0)
					strListAppend(&str, strin, line);
			} else
				strListReplace(str, line, strin);
			if (line < 1)
				continue;
			term->carriage_return();
			term->cursor_up();
			term->cleartoeol();
			line--;
			continue;
		}
		if (console & CON_DELETELINE) {
			strListDelete(&str, line);
			continue;
		}
		term->newline();
		if (console & (CON_DOWNARROW | CON_RIGHTARROW)) {
			if (str[line] != NULL) {
				strListReplace(str, line, strin);
				line++;
				continue;
			}
		}
		if (strin[0] == '/' && strlen(strin) < 16) {
			if (!stricmp(strin, "/DEBUG") && useron_is_sysop()) {
				bprintf("\r\nline=%d lines=%d (%d), rows=%d\r\n", line, lines, (int)strListCount(str), term->rows);
				continue;
			}
			else if (!stricmp(strin, "/ABT")) {
				strListFree(&str);
				return 0;
			}
			else if (toupper(strin[1]) == 'D') {   // delete line(s)
				lines = strListCount(str);
				char* p = strin + 2;
				int   first = atoi(p) - 1;
				if (first < 0 || (uint)first >= lines) {
					if (*p) {
						bputs(text[InvalidLineNumber]);
						continue;
					}
					first = lines - 1; // /D means delete last line
					if (first < 0)
						continue;
				}
				int last = first;
				SKIP_DIGIT(p);
				FIND_CHAR(p, '-');
				if (*p == '-') {
					++p;
					last = atoi(p) - 1;
					if (last < first || (uint)last >= lines) {
						bputs(text[InvalidLineNumber]);
						continue;
					}
				}
				int count = (last - first) + 1;
				strListFastDelete(str, first, count);
				lines -= count;
				if (line > lines)
					line = lines;
				continue;
			}
			else if (toupper(strin[1]) == 'I') {   /* insert a line before number x */
				lines = strListCount(str);
				if (line >= maxlines || !lines)
					continue;
				i = atoi(strin + 2) - 1;
				if (i < 0)
					i = lines - 1;
				if (i >= (int)lines || i < 0)
					bputs(text[InvalidLineNumber]);
				else {
					strListInsert(&str, "", i);
					line = ++lines;
				}
				continue;
			}
			else if (toupper(strin[1]) == 'E') {   /* edit a line */
				if (str[0] == NULL)
					continue;
				lines = strListCount(str);
				i = atoi(strin + 2) - 1;
				j = K_MSG | K_EDIT; /* use j for the getstr mode */
				if (i == -1) { /* /E means edit last line */
					i = lines - 1;
					j |= K_WORDWRAP;  /* wrap when editing last line */
				}
				if (i >= (int)lines || i < 0)
					bputs(text[InvalidLineNumber]);
				else {
					SAFECOPY(strin, str[i]);
					getstr(strin, term->cols - 1, j);
					strListReplace(str, i, strin);
				}
				continue;
			}
			else if (!stricmp(strin, "/CLR")) {
				bputs(text[MsgCleared]);
				strListFreeStrings(str);
				line = 0;
				lines = 0;
				putmsg(top, pmode);
				continue;
			}
			else if (toupper(strin[1]) == 'L') {   /* list message */
				if (str[0] == NULL) {
					continue;
				}
				lines = strListCount(str);
				j = atoi(strin + 2) - 1;
				if (j < 0)
					j = 0;
				if (j >= (int)lines) {
					bputs(text[InvalidLineNumber]);
					continue;
				}
				bool linenums = !noyes(text[WithLineNumbersQ]);
				term->newline();
				attr(LIGHTGRAY);
				putmsg(top, pmode);
				int  digits = DEC_DIGITS(lines);
				while (str[j] != NULL && !msgabort()) {
					if (linenums) { /* line numbers */
						snprintf(tmp, sizeof tmp, "%*d: %-.*s", digits, j + 1, (int)(term->cols - (digits + 3)), str[j]);
						putmsg(tmp, pmode);
					}
					else
						putmsg(str[j], pmode);
					term->cleartoeol();  /* delete to end of line */
					term->newline();
					j++;
				}
				line = j;
				sync();
				continue;
			}
			else if (!stricmp(strin, "/S")) { /* Save */
				break;
			}
			else if (!stricmp(strin, "/T")) { /* Edit title/subject */
				if (title != nulstr) { // hack
					bputs(text[SubjectPrompt]);
					getstr(title, LEN_TITLE, K_LINE | K_EDIT | K_AUTODEL | K_TRIM | K_UTF8);
					sync();
					term->newline();
				}
				continue;
			}
			else if (!stricmp(strin, "/?")) {
				menu("editor"); /* User Editor Commands */
				sync();
				continue;
			}
			else if (!stricmp(strin, "/ATTR"))    {
				menu("attr");   /* User ANSI Commands */
				sync();
				continue;
			}
			else if (!stricmp(strin, "/UPLOAD")) {
upload:
				char fname[MAX_PATH + 1];
				SAFEPRINTF(fname, "%sUPLOAD.MSG", cfg.temp_dir);
				(void)removecase(fname);
				if (!recvfile(fname, prot, /* autohang: */ false)) {
					bprintf(text[FileNotReceived], "File");
					continue;
				}
				FILE* fp = fopen(fname, "r");
				if (fp == NULL) {
					errormsg(WHERE, ERR_OPEN, fname, 0);
					continue;
				}
				strListFreeStrings(str);
				strListReadFile(fp, &str, /* max line len */ 0);
				strListTruncateTrailingLineEndings(str);
				char rx_lines[128];
				SAFEPRINTF(rx_lines, "%u lines", lines = strListCount(str));
				bprintf(text[FileNBytesReceived], rx_lines, ultoac(ftell(fp), tmp));
				while (lines > maxlines)
					free(str[--lines]);
				line = lines;
				str[line] = NULL;
				fclose(fp);
				continue;
			}
		}
		if (str[line] != NULL) {
			strListReplace(str, line, strin);
			line++;
			strListInsert(&str, "", line);
			for (i = line; str[i]; i++) {
				putmsg(str[i], pmode);
				term->cleartoeol();
				term->newline();
			}
			term->clearline();
			term->cursor_up(i - line);
			continue;
		}

		if (line < maxlines) {
			strListAppend(&str, strin, line);
			line++;
		}
	}
	if (online)
		strcpy(buf, top);
	else
		buf[0] = 0;
	lines = strListCount(str);
	if (lines > maxlines) {
		SAFEPRINTF(tmp, "max lines (%u) exceeded", maxlines);
		errormsg(WHERE, ERR_CHK, tmp, lines);
		lines = maxlines;
	}
	for (i = 0; i < (int)lines; i++)
		snprintf(buf + strlen(buf), max_line_len, "%s\r\n", str[i]);
	strListFree(&str);
	if (!online)
		return 0;
	return lines;
}

/****************************************************************************/
/* Edits an existing file or creates a new one in MSG format                */
/****************************************************************************/
bool sbbs_t::editfile(char *fname, uint maxlines, int wmode, const char* to, const char* from, const char* subj, const char* msgarea, bool clean_quotes)
{
	char *   buf, path[MAX_PATH + 1];
	char     msgtmp[MAX_PATH + 1];
	char     str[MAX_PATH + 1];
	int      file;
	long     length, l, ex_mode = 0;
	FILE*    stream;
	unsigned lines;
	ushort   useron_xedit = useron.xedit;

	if (term->cols < TERM_COLS_MIN) {
		errormsg(WHERE, ERR_CHK, "columns (too narrow)", term->cols);
		return false;
	}

	if (useron_xedit && !chk_ar(cfg.xedit[useron_xedit - 1]->ar, &useron, &client))
		useron_xedit = 0;

	if (clean_quotes)
		(void)removecase(quotes_fname(useron_xedit, path, sizeof(path)));

	if (to == NULL)
		to = fname;
	if (from == NULL)
		from = nulstr;
	if (subj == NULL)
		subj = nulstr;
	if (msgarea == NULL)
		msgarea = nulstr;

	if (useron_xedit) {

		SAFECOPY(path, fname);

		msg_tmp_fname(useron_xedit, msgtmp, sizeof(msgtmp));
		if (stricmp(msgtmp, path)) {
			(void)removecase(msgtmp);
			if (fexistcase(path))
				CopyFile(path, msgtmp, /* failIfExists: */ FALSE);
		}

		editor_inf(useron_xedit, to, from, subj, /* mode: */ 0, msgarea);
		if (cfg.xedit[useron_xedit - 1]->misc & XTRN_NATIVE)
			ex_mode |= EX_NATIVE;
		if (cfg.xedit[useron_xedit - 1]->misc & XTRN_SH)
			ex_mode |= EX_SH;
		if (cfg.xedit[useron_xedit - 1]->misc & XTRN_STDIO) {
			ex_mode |= EX_STDIO;
			if (cfg.xedit[useron_xedit - 1]->misc & WWIVCOLOR)
				ex_mode |= EX_WWIV;
		}
		cls();
		rioctl(IOCM | PAUSE | ABORT);
		if (external(cmdstr(cfg.xedit[useron_xedit - 1]->rcmd, msgtmp, nulstr, NULL, ex_mode), ex_mode, cfg.node_dir) != 0)
			return false;
		if (!fexist(msgtmp))
			return false;
		l = process_edited_file(msgtmp, path, wmode | WM_EDIT, &lines, maxlines);
		if (l > 0) {
			SAFEPRINTF3(str, "created or edited file: %s (%ld bytes, %u lines)"
			            , path, l, lines);
			logline(LOG_NOTICE, nulstr, str);
		} else if (l < 0)
			errormsg(WHERE, ERR_CREATE, path, l);
		rioctl(IOSM | PAUSE | ABORT);
		return true;
	}
	if ((buf = (char *)malloc((maxlines * MAX_LINE_LEN) + 1)) == NULL) {
		errormsg(WHERE, ERR_ALLOC, nulstr, (maxlines * MAX_LINE_LEN) + 1);
		return false;
	}
	if ((file = nopen(fname, O_RDONLY)) != -1) {
		length = (long)filelength(file);
		if (length < 0 || length > (long)(maxlines * MAX_LINE_LEN)) {
			close(file);
			free(buf);
			attr(cfg.color[clr_err]);
			bprintf("\7\r\nFile size (%lu bytes) is larger than %lu (maxlines: %u).\r\n"
			        , length, (ulong)maxlines * MAX_LINE_LEN, maxlines);
			return false;
		}
		if (read(file, buf, length) != length) {
			close(file);
			free(buf);
			errormsg(WHERE, ERR_READ, fname, length);
			return false;
		}
		buf[length] = 0;
		close(file);
	}
	else {
		buf[0] = 0;
		bputs(text[NewFile]);
	}
	if (!msgeditor(buf, nulstr, /* title: */ (char*)nulstr, maxlines, MAX_LINE_LEN)) {
		free(buf);
		return false;
	}
	bputs(text[Saving]);
	if ((stream = fnopen(NULL, fname, O_CREAT | O_WRONLY | O_TRUNC)) == NULL) {
		errormsg(WHERE, ERR_OPEN, fname, O_CREAT | O_WRONLY | O_TRUNC);
		free(buf);
		return false;
	}
	l = process_edited_text(buf, stream, wmode | WM_EDIT, &lines, maxlines);
	bprintf(text[SavedNBytes], l, lines);
	fclose(stream);
	free(buf);
	SAFEPRINTF3(str, "created or edited file: %s (%ld bytes, %u lines)"
	            , fname, l, lines);
	logline(nulstr, str);
	return true;
}

/*************************/
/* Copy file attachments */
/* TODO: Quoted filename support */
/*************************/
bool sbbs_t::copyfattach(uint to, uint from, const char* subj)
{
	char str[128], dest[MAX_PATH + 1], src[MAX_PATH + 1], *tp, *sp, *p;
	bool result = false;
	char dir[MAX_PATH + 1];

	if (to == 0)
		SAFEPRINTF2(dir, "%sfile/%04u.out", cfg.data_dir, from);
	else
		SAFEPRINTF2(dir, "%sfile/%04u.in", cfg.data_dir, to);
	if (mkpath(dir) != 0) {
		errormsg(WHERE, ERR_MKDIR, dir, 0);
		return false;
	}

	SAFECOPY(str, subj);
	tp = str;
	while (1) {
		p = strchr(tp, ' ');
		if (p)
			*p = 0;
		sp = strrchr(tp, '/');              /* sp is slash pointer */
		if (!sp)
			sp = strrchr(tp, '\\');
		if (sp)
			tp = sp + 1;
		if (strcspn(tp, ILLEGAL_FILENAME_CHARS) == strlen(tp)) {
			SAFEPRINTF2(dest, "%s/%s", dir, tp);
			SAFEPRINTF3(src, "%sfile/%04u.in/%s", cfg.data_dir, from, tp);
			if (mv(src, dest, /* copy */ true) != 0)
				return false;
			result = true;
		}
		if (!p)
			break;
		tp = p + 1;
	}
	return result;
}

/****************************************************************************/
/* Forwards msg 'orgmsg' to 'to' with optional 'comment'					*/
/* If comment is NULL, comment lines will be prompted for.					*/
/* If comment is a zero-length string, no comments will be included.		*/
/****************************************************************************/
bool sbbs_t::forwardmsg(smb_t* smb, smbmsg_t* orgmsg, const char* to, const char* subject, const char* comment)
{
	char     str[256], touser[128];
	char     tmp[512];
	char     subj[LEN_TITLE + 1];
	int      result;
	smbmsg_t msg;
	node_t   node;
	uint     usernumber = 0;

	if (to == NULL)
		return false;

	uint16_t net_type = NET_NONE;
	if (strchr(to, '@') != NULL)
		net_type = smb_netaddr_type(to);
	if (net_type == NET_NONE) {
		usernumber = finduser(to);
		if (usernumber < 1)
			return false;
	} else if (!is_supported_netmail_addr(&cfg, to)) {
		bprintf(text[InvalidNetMailAddr], to);
		return false;
	}

	if (useron.etoday >= cfg.level_emailperday[useron.level] && !useron_is_sysop() && !(useron.exempt & FLAG('M'))) {
		bputs(text[TooManyEmailsToday]);
		return false;
	}
	if (useron.rest & FLAG('F')) {
		bputs(text[R_Forward]);
		return false;
	}
	if (usernumber == 1 && useron.rest & FLAG('S')) {
		bprintf(text[R_Feedback], cfg.sys_op);
		return false;
	}
	if (usernumber != 1 && useron.rest & FLAG('E')) {
		bputs(text[R_Email]);
		return false;
	}

	if (subject == NULL) {
		subject = subj;
		if (orgmsg->hdr.auxattr & MSG_FILEATTACH)
			SAFECOPY(subj, orgmsg->subj);
		else {
			SAFEPRINTF(subj, "Fwd: %s", orgmsg->subj);
			bputs(text[SubjectPrompt]);
			if (!getstr(subj, sizeof(subj) - 1, K_LINE | K_EDIT | K_AUTODEL | K_TRIM | K_UTF8))
				return false;
		}
	}

	memset(&msg, 0, sizeof(msg));
	msg.hdr.auxattr = orgmsg->hdr.auxattr & (MSG_HFIELDS_UTF8 | MSG_MIMEATTACH);
	msg.hdr.when_imported.time = time32(NULL);
	msg.hdr.when_imported.zone = sys_timezone(&cfg);
	msg.hdr.when_written = smb_when(msg.hdr.when_imported.time, msg.hdr.when_imported.zone);

	smb_hfield_str(&msg, SUBJECT, subject);
	add_msg_ids(&cfg, smb, &msg, orgmsg);

	smb_hfield_str(&msg, SENDER, useron.alias);
	SAFEPRINTF(str, "%u", useron.number);
	smb_hfield_str(&msg, SENDEREXT, str);

	/* Security logging */
	msg_client_hfields(&msg, &client);
	smb_hfield_str(&msg, SENDERSERVER, server_host_name());

	if (usernumber > 0) {
		username(&cfg, usernumber, touser);
		smb_hfield_str(&msg, RECIPIENT, touser);
		SAFEPRINTF(str, "%u", usernumber);
		smb_hfield_str(&msg, RECIPIENTEXT, str);
	} else {
		SAFECOPY(touser, to);
		char* p;
		if ((p = strchr(touser, '@')) != NULL)
			*p = '\0';
		smb_hfield_str(&msg, RECIPIENT, touser);
		SAFECOPY(touser, to);
		const char* addr = touser;
		if (net_type != NET_INTERNET && p != NULL)
			addr = p + 1;
		char        fulladdr[128];
		if (net_type == NET_QWK) {
			usernumber = qwk_route(&cfg, addr, fulladdr, sizeof(fulladdr) - 1);
			if (*fulladdr == '\0') {
				bprintf(text[InvalidNetMailAddr], addr);
				smb_freemsgmem(&msg);
				return false;
			}
			addr = fulladdr;
			SAFEPRINTF(str, "%u", usernumber);
			smb_hfield_str(&msg, RECIPIENTEXT, str);
			usernumber = 0;
		}
		smb_hfield_bin(&msg, RECIPIENTNETTYPE, net_type);
		smb_hfield_netaddr(&msg, RECIPIENTNETADDR, addr, &net_type);
	}
	if (orgmsg->mime_version != NULL) {
		safe_snprintf(str, sizeof(str), "MIME-Version: %s", orgmsg->mime_version);
		smb_hfield_str(&msg, RFC822HEADER, str);
	}
	if (orgmsg->content_type != NULL) {
		safe_snprintf(str, sizeof(str), "Content-type: %s", orgmsg->content_type);
		smb_hfield_str(&msg, RFC822HEADER, str);
	}
	// This header field not strictly required any more:
	time32_t    now32 = time32(NULL);
	smb_hfield(&msg, FORWARDED, sizeof(now32), &now32);

	const char* br = NULL;
	const char* pg = nulstr;
	const char* lt = "<";
	const char* gt = ">";
	if (orgmsg->text_subtype != NULL && stricmp(orgmsg->text_subtype, "html") == 0) {
		lt = "&lt;";
		gt = "&gt;";
		br = "<br>";
		pg = "<p>";
	}

	if (comment == NULL) {
		while (online && !msgabort()) {
			bputs(text[UeditComment]);
			if (!getstr(str, 70, K_WORDWRAP))
				break;
			smb_hfield_string(&msg, SMB_COMMENT, str);
			smb_hfield_string(&msg, SMB_COMMENT, br);
		}
		if (!online || msgabort()) {
			smb_freemsgmem(&msg);
			return false;
		}
	} else {
		if (*comment)
			smb_hfield_string(&msg, SMB_COMMENT, comment);
	}
	if (smb_get_hfield(&msg, SMB_COMMENT, NULL) != NULL)
		smb_hfield_string(&msg, SMB_COMMENT, pg);
	smb_hfield_string(&msg, SMB_COMMENT, "-----Forwarded Message-----");
	smb_hfield_string(&msg, SMB_COMMENT, br);
	if (orgmsg->from_net.addr != NULL)
		safe_snprintf(str, sizeof(str), "From: %s %s%s%s"
		              , orgmsg->from, lt, smb_netaddrstr(&orgmsg->from_net, tmp), gt);
	else
		safe_snprintf(str, sizeof(str), "From: %s", orgmsg->from);
	smb_hfield_string(&msg, SMB_COMMENT, str);
	smb_hfield_string(&msg, SMB_COMMENT, br);
	safe_snprintf(str, sizeof(str), "Date: %s", msgdate(orgmsg->hdr.when_written, tmp));
	smb_hfield_string(&msg, SMB_COMMENT, str);
	smb_hfield_string(&msg, SMB_COMMENT, br);
	if (orgmsg->to_net.addr != NULL)
		safe_snprintf(str, sizeof(str), "To: %s %s%s%s"
		              , orgmsg->to, lt, smb_netaddrstr(&orgmsg->to_net, tmp), gt);
	else
		safe_snprintf(str, sizeof(str), "To: %s", orgmsg->to);
	smb_hfield_string(&msg, SMB_COMMENT, str);
	smb_hfield_string(&msg, SMB_COMMENT, br);
	safe_snprintf(str, sizeof(str), "Subject: %s", orgmsg->subj);
	smb_hfield_string(&msg, SMB_COMMENT, str);
	smb_hfield_string(&msg, SMB_COMMENT, pg);

	// Re-use the original message's data
	if ((result = smb_open_da(smb)) != SMB_SUCCESS) {
		smb_freemsgmem(&msg);
		errormsg(WHERE, ERR_OPEN, smb->file, result, smb->last_error);
		return false;
	}
	if ((result = smb_incmsg_dfields(smb, orgmsg, 1)) != SMB_SUCCESS) {
		smb_freemsgmem(&msg);
		errormsg(WHERE, ERR_WRITE, smb->file, result, smb->last_error);
		return false;
	}
	smb_close_da(smb);

	msg.dfield = orgmsg->dfield;
	msg.hdr.offset = orgmsg->hdr.offset;
	msg.hdr.total_dfields = orgmsg->hdr.total_dfields;

	if (orgmsg->hdr.auxattr & MSG_FILEATTACH) {
		copyfattach(usernumber, useron.number, orgmsg->subj);
		msg.hdr.auxattr |= MSG_FILEATTACH;
	}

	result = smb_addmsghdr(smb, &msg, smb_storage_mode(&cfg, smb));
	msg.dfield = NULL;
	smb_freemsgmem(&msg);
	if (result != SMB_SUCCESS) {
		errormsg(WHERE, ERR_WRITE, smb->file, result, smb->last_error);
		smb_freemsg_dfields(smb, orgmsg, 1);
		return false;
	}

	bprintf(text[Forwarded], touser, usernumber);
	SAFEPRINTF(str, "forwarded mail to %s", touser);
	logline("E+", str);

	if (usernumber == 1) {
		useron.fbacks = (ushort)adjustuserval(&cfg, useron.number, USER_FBACKS, 1);
		logon_fbacks++;
	}
	else {
		useron.emails = (ushort)adjustuserval(&cfg, useron.number, USER_EMAILS, 1);
		logon_emails++;
	}
	useron.etoday = (ushort)adjustuserval(&cfg, useron.number, USER_ETODAY, 1);

	if (usernumber > 0) {
		int i;
		for (i = 1; i <= cfg.sys_nodes; i++) { /* Tell user, if online */
			getnodedat(i, &node);
			if (node.useron == usernumber && !(node.misc & NODE_POFF)
			    && (node.status == NODE_INUSE || node.status == NODE_QUIET)) {
				SAFEPRINTF2(str, text[EmailNodeMsg], cfg.node_num, useron.alias);
				putnmsg(i, str);
				break;
			}
		}
		if (i > cfg.sys_nodes) {   /* User wasn't online, so leave short msg */
			SAFEPRINTF(str, text[UserSentYouMail], useron.alias);
			putsmsg(usernumber, str);
		}
	} else {
		if (net_type == NET_FIDO && cfg.netmail_sem[0])
			ftouch(cmdstr(cfg.netmail_sem, nulstr, nulstr, NULL));
	}
	return true;
}

/****************************************************************************/
/* Auto-Message Routine ('A' from the main menu)                            */
/****************************************************************************/
void sbbs_t::automsg()
{
	if (cfg.automsg_mod[0])
		exec_bin(cfg.automsg_mod, &main_csi);
	else
		bputs(text[R_AutoMsg]);
}

/****************************************************************************/
/* Edits messages															*/
/****************************************************************************/
bool sbbs_t::editmsg(smb_t* smb, smbmsg_t *msg)
{
	char     buf[SDT_BLOCK_LEN];
	char     msgtmp[MAX_PATH + 1];
	uint16_t xlat;
	int      file, i, j, x;
	long     length;
	off_t    offset;
	FILE *   instream;
	bool     is_msg = (msg->hdr.type == SMB_MSG_TYPE_NORMAL);

	if (!msg->hdr.total_dfields) {
		if (!is_msg)
			smb_dfield(msg, TEXT_BODY, 0);
		else
			return false;
	}

	msg_tmp_fname(useron.xedit, msgtmp, sizeof(msgtmp));
	(void)removecase(msgtmp);
	msgtotxt(smb, msg, msgtmp, /* header: */ false, /* mode: */ is_msg ? GETMSGTXT_ALL : GETMSGTXT_BODY_ONLY);
	if (!editfile(msgtmp, cfg.level_linespermsg[useron.level], WM_EXPANDLF, msg->to, msg->from, msg->subj, subnum_is_valid(smb->subnum) ? cfg.sub[smb->subnum]->sname : nulstr))
		return false;
	length = (long)flength(msgtmp);
	if (length < 1L)
		return false;

	length += 2;   /* +2 for translation string */

	if ((i = smb_locksmbhdr(smb)) != SMB_SUCCESS) {
		errormsg(WHERE, ERR_LOCK, smb->file, i, smb->last_error);
		return false;
	}

	if ((i = smb_getstatus(smb)) != SMB_SUCCESS) {
		errormsg(WHERE, ERR_READ, smb->file, i, smb->last_error);
		return false;
	}

	if (!(smb->status.attr & SMB_HYPERALLOC)) {
		if ((i = smb_open_da(smb)) != SMB_SUCCESS) {
			errormsg(WHERE, ERR_OPEN, smb->file, i, smb->last_error);
			return false;
		}
		if ((i = smb_freemsg_dfields(smb, msg, 1)) != SMB_SUCCESS)
			errormsg(WHERE, ERR_WRITE, smb->file, i, smb->last_error);
	}

	msg->dfield[0].type = TEXT_BODY;              /* Make one single data field */
	msg->dfield[0].length = length;
	msg->dfield[0].offset = 0;
	if (is_msg) {
		for (x = 1; x < msg->hdr.total_dfields; x++) {     /* Clear the other data fields */
			msg->dfield[x].type = UNUSED;             /* so we leave the header length */
			msg->dfield[x].length = 0;                /* unchanged */
			msg->dfield[x].offset = 0;
		}
	}

	if (smb->status.attr & SMB_HYPERALLOC)
		offset = smb_hallocdat(smb);
	else {
		if (is_msg
		    && ((smb->subnum != INVALID_SUB && cfg.sub[smb->subnum]->misc & SUB_FAST)
		        || (smb->subnum == INVALID_SUB && cfg.sys_misc & SM_FASTMAIL)))
			offset = smb_fallocdat(smb, length, 1);
		else
			offset = smb_allocdat(smb, length, 1);
		smb_close_da(smb);
	}

	if (offset < 0) {
		smb_unlocksmbhdr(smb);
		errormsg(WHERE, ERR_ALLOC, msgtmp, length);
		return false;
	}

	msg->hdr.offset = (uint32_t)offset;
	if ((file = open(msgtmp, O_RDONLY | O_BINARY)) == -1
	    || (instream = fdopen(file, "rb")) == NULL) {
		smb_unlocksmbhdr(smb);
		smb_freemsgdat(smb, offset, length, 1);
		errormsg(WHERE, ERR_OPEN, msgtmp, O_RDONLY | O_BINARY);
		return false;
	}

	setvbuf(instream, NULL, _IOFBF, FNOPEN_BUF_SIZE);
	fseeko(smb->sdt_fp, offset, SEEK_SET);
	xlat = XLAT_NONE;
	if (fwrite(&xlat, 2, 1, smb->sdt_fp) != 1) {
		errormsg(WHERE, ERR_WRITE, smb->file, 2);
		smb_unlocksmbhdr(smb);
		smb_freemsgdat(smb, offset, length, 1);
		return false;
	}
	x = SDT_BLOCK_LEN - 2;              /* Don't read/write more than 255 */
	while (!feof(instream)) {
		memset(buf, 0, x);
		j = fread(buf, 1, x, instream);
		if (j < 1)
			break;
		if (j > 1 && (j != x || feof(instream)) && buf[j - 1] == LF && buf[j - 2] == CR)
			buf[j - 1] = buf[j - 2] = 0; /* Convert to NULL */
		if (fwrite(buf, j, 1, smb->sdt_fp) != 1) {
			fclose(instream);
			errormsg(WHERE, ERR_WRITE, smb->file, j);
			smb_unlocksmbhdr(smb);
			smb_freemsgdat(smb, offset, length, 1);
			return false;
		}
		x = SDT_BLOCK_LEN;
	}
	fflush(smb->sdt_fp);
	fclose(instream);

	smb_unlocksmbhdr(smb);
	msg->hdr.length = (ushort)smb_getmsghdrlen(msg);
	if ((i = smb_putmsghdr(smb, msg)) != SMB_SUCCESS)
		errormsg(WHERE, ERR_WRITE, smb->file, i, smb->last_error);
	return i == SMB_SUCCESS;
}

/****************************************************************************/
/* Moves a message from one message base to another 						*/
/****************************************************************************/
bool sbbs_t::movemsg(smbmsg_t* msg, int subnum)
{
	char     str[256], *buf;
	int      i;
	int      newgrp, newsub, storage;
	off_t    offset;
	uint     length;
	smbmsg_t newmsg = *msg;
	smb_t    newsmb;

	for (i = 0; i < usrgrps; i++)       /* Select New Group */
		uselect(1, i, "Message Group", cfg.grp[usrgrp[i]]->lname, 0);
	if ((newgrp = uselect(0, 0, 0, 0, 0)) < 0)
		return false;

	for (i = 0; i < usrsubs[newgrp]; i++)       /* Select New Sub-Board */
		uselect(1, i, "Sub-Board", cfg.sub[usrsub[newgrp][i]]->lname, 0);
	if ((newsub = uselect(0, 0, 0, 0, 0)) < 0)
		return false;
	newsub = usrsub[newgrp][newsub];

	length = smb_getmsgdatlen(msg);
	if ((buf = (char *)malloc(length)) == NULL) {
		errormsg(WHERE, ERR_ALLOC, smb.file, length);
		return false;
	}

	fseek(smb.sdt_fp, msg->hdr.offset, SEEK_SET);
	if (fread(buf, length, 1, smb.sdt_fp) != 1) {
		free(buf);
		errormsg(WHERE, ERR_READ, smb.file, length);
		return false;
	}

	SAFEPRINTF2(newsmb.file, "%s%s", cfg.sub[newsub]->data_dir, cfg.sub[newsub]->code);
	newsmb.retry_time = cfg.smb_retry_time;
	newsmb.subnum = newsub;
	if ((i = smb_open(&newsmb)) != SMB_SUCCESS) {
		free(buf);
		errormsg(WHERE, ERR_OPEN, newsmb.file, i, newsmb.last_error);
		return false;
	}

	if (filelength(fileno(newsmb.shd_fp)) < 1) {    /* Create it if it doesn't exist */
		newsmb.status.max_crcs = cfg.sub[newsub]->maxcrcs;
		newsmb.status.max_msgs = cfg.sub[newsub]->maxmsgs;
		newsmb.status.max_age = cfg.sub[newsub]->maxage;
		newsmb.status.attr = cfg.sub[newsub]->misc & SUB_HYPER ? SMB_HYPERALLOC :0;
		if ((i = smb_create(&newsmb)) != SMB_SUCCESS) {
			free(buf);
			smb_close(&newsmb);
			errormsg(WHERE, ERR_CREATE, newsmb.file, i, newsmb.last_error);
			return false;
		}
	}

	if ((i = smb_locksmbhdr(&newsmb)) != SMB_SUCCESS) {
		free(buf);
		smb_close(&newsmb);
		errormsg(WHERE, ERR_LOCK, newsmb.file, i, newsmb.last_error);
		return false;
	}

	if ((i = smb_getstatus(&newsmb)) != SMB_SUCCESS) {
		free(buf);
		smb_close(&newsmb);
		errormsg(WHERE, ERR_READ, newsmb.file, i, newsmb.last_error);
		return false;
	}

	if (newsmb.status.attr & SMB_HYPERALLOC) {
		offset = smb_hallocdat(&newsmb);
		storage = SMB_HYPERALLOC;
	}
	else {
		if ((i = smb_open_da(&newsmb)) != SMB_SUCCESS) {
			free(buf);
			smb_close(&newsmb);
			errormsg(WHERE, ERR_OPEN, newsmb.file, i, newsmb.last_error);
			return false;
		}
		if (cfg.sub[newsub]->misc & SUB_FAST) {
			offset = smb_fallocdat(&newsmb, length, 1);
			storage = SMB_FASTALLOC;
		}
		else {
			offset = smb_allocdat(&newsmb, length, 1);
			storage = SMB_SELFPACK;
		}
		smb_close_da(&newsmb);
	}

	newmsg.hdr.offset = (uint32_t)offset;
	newmsg.hdr.version = smb_ver();

	fseeko(newsmb.sdt_fp, offset, SEEK_SET);
	int wr = fwrite(buf, length, 1, newsmb.sdt_fp);
	fflush(newsmb.sdt_fp);
	free(buf);

	if (wr != 1) {
		errormsg(WHERE, ERR_WRITE, newsmb.file, length);
		smb_close(&newsmb);
		smb_freemsg_dfields(&newsmb, &newmsg, 1);
		return false;
	}

	i = smb_addmsghdr(&newsmb, &newmsg, storage);   // calls smb_unlocksmbhdr()
	smb_close(&newsmb);

	if (i) {
		errormsg(WHERE, ERR_WRITE, newsmb.file, i, newsmb.last_error);
		smb_freemsg_dfields(&newsmb, &newmsg, 1);
		return false;
	}

	bprintf("\r\nMoved to %s %s\r\n\r\n"
	        , cfg.grp[usrgrp[newgrp]]->sname, cfg.sub[newsub]->lname);
	safe_snprintf(str, sizeof(str), "moved message from %s %s to %s %s"
	              , cfg.grp[cfg.sub[subnum]->grp]->sname, cfg.sub[subnum]->sname
	              , cfg.grp[newgrp]->sname, cfg.sub[newsub]->sname);
	logline("M+", str);
	signal_sub_sem(&cfg, newsub);

	return true;
}

ushort sbbs_t::chmsgattr(const smbmsg_t* msg)
{
	int      ch;
	uint16_t attr = msg->hdr.attr;

	while (online && !(sys_status & SS_ABORT)) {
		term->newline();
		show_msgattr(msg);
		menu("msgattr");
		ch = getkey(K_UPPER);
		if (ch)
			bprintf("%c\r\n", ch);
		switch (ch) {
			case 'P':
				attr ^= MSG_PRIVATE;
				break;
			case 'S':
				attr ^= MSG_SPAM;
				break;
			case 'R':
				attr ^= MSG_READ;
				break;
			case 'K':
				attr ^= MSG_KILLREAD;
				break;
			case 'A':
				attr ^= MSG_ANONYMOUS;
				break;
			case 'N':   /* Non-purgeable */
				attr ^= MSG_PERMANENT;
				break;
			case 'M':
				attr ^= MSG_MODERATED;
				break;
			case 'V':
				attr ^= MSG_VALIDATED;
				break;
			case 'D':
				attr ^= MSG_DELETE;
				break;
			case 'L':
				attr ^= MSG_LOCKED;
				break;
			case 'C':
				attr ^= MSG_NOREPLY;
				break;
			case 'E':
				attr ^= MSG_REPLIED;
				break;
			default:
				return attr;
		}
	}
	return attr;
}
