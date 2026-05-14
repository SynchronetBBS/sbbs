/* Synchronet file print/display routines */

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
#include "boolsrch.h"
#include "utf8.h"
#include "petdefs.h"
#include "sauce.h"

#ifndef PRINTFILE_MAX_LINE_LEN
#define PRINTFILE_MAX_LINE_LEN (8 * 1024)
#endif
#ifndef PRINTFILE_MAX_FILE_LEN
#define PRINTFILE_MAX_FILE_LEN (2 * 1024 * 1024)
#endif


/****************************************************************************/
/* like fgets(), excepts discards all carriage-returns						*/
/* and if cols is non-zero, stops reading when displayed width >= cols		*/
/****************************************************************************/
char* sbbs_t::fgetline(char* s, size_t size, int cols, FILE* stream, int mode)
{
	size_t len = 0;

	if (size == 0)
		return NULL;
	memset(s, 0, size);

	while (len + 1 < size) {
		int ch = fgetc(stream);
		if (ch == EOF)
			break;
		if (ch == '\r')
			continue;
		s[len++] = ch;
		if (ch == '\n')
			break;
		if (cols && len >= (size_t)cols && (int)term->bstrlen(s, mode) >= cols) {
			ch = fgetc(stream);
			if (ch == '\r')
				ch = fgetc(stream);
			if (ch == EOF || ch == '\n')
				break;
			if (fseek(stream, -1, SEEK_CUR) != 0)
				return NULL;
			break;
		}
	}
	return len ? s : NULL;
}

/****************************************************************************/
/* Prints a file remotely and locally, interpreting ^A sequences, checks    */
/* for pauses, aborts and ANSI. 'str' is the path of the file to print      */
/* Called from functions menu and text_sec                                  */
/****************************************************************************/
bool sbbs_t::printfile(const char* inpath, int mode, int org_cols, JSObject* obj)
{
	char* buf;
	char  fpath[MAX_PATH + 1];
	char* p;
	int   file;
	BOOL  rip = FALSE;
	off_t l, length;
	auto savcon = console;
	FILE *stream;

	if (*inpath == '\0')
		return false;
	if (FULLPATH(fpath, inpath, sizeof fpath) == NULL)
		SAFECOPY(fpath, inpath);
	if ((mode & P_MODS) && cfg.mods_dir[0] != '\0') {
		if (strncmp(fpath, cfg.text_dir, strlen(cfg.text_dir)) == 0) {
			char modpath[MAX_PATH + 1];
			snprintf(modpath, sizeof modpath, "%stext/%s", cfg.mods_dir, fpath + strlen(cfg.text_dir));
			if(fexistcase(modpath))
				SAFECOPY(fpath, modpath);
		}
	}
	(void)fexistcase(fpath);
	p = getfext(fpath);
	if (p != NULL) {
		if (stricmp(p, ".rip") == 0) {
			rip = TRUE;
			mode |= P_NOPAUSE;
		} else if (stricmp(p, ".seq") == 0) {
			mode |= P_PETSCII;
		} else if (stricmp(p, ".utf8") == 0) {
			mode |= P_UTF8;
		}
	}

	if (mode & P_NOABORT || rip) {
		if (online == ON_REMOTE) {
			rioctl(IOCM | ABORT);
			rioctl(IOCS | ABORT);
		}
		clearabort();
	}

	if (!(mode & P_NOXATTRS))
		mode |= (cfg.sys_misc & SM_XATTR_SUPPORT) << P_XATTR_SHIFT;

	if (rip) // Let's not eat the pipe codes used in RIP files
		mode &= ~(P_CELERITY);

	if ((stream = fnopen(&file, fpath, O_RDONLY | O_DENYNONE)) == NULL) {
		if (!(mode & P_NOERROR)) {
			lprintf(LOG_NOTICE, "!Error %d (%s) opening: %s"
			        , errno, strerror(errno), fpath);
			bputs(text[FileNotFound]);
			if (useron_is_sysop())
				bputs(fpath);
			term->newline();
		}
		return false;
	}

	length = filelength(file);
	if (length < 1) {
		fclose(stream);
		if (length < 0) {
			errormsg(WHERE, ERR_CHK, fpath, (int)length);
			return false;
		}
		return true;
	}

	struct sauce_charinfo sauce{};
	if (sauce_fread_charinfo(stream, /* type */nullptr, &sauce)) {
		mode |= P_CPM_EOF;
		if (org_cols == 0 && sauce.width >= TERM_COLS_MIN && sauce.width <= TERM_COLS_MAX) {
			org_cols = sauce.width;
			mode |= P_WRAP;
		}
	}

	lprintf(LOG_DEBUG, "Printing file: %s", fpath);
	if (!(mode & P_NOCRLF) && term->row > 0 && !rip) {
		term->newline();
	}

	if ((mode & P_OPENCLOSE) && length <= PRINTFILE_MAX_FILE_LEN) {
		if ((buf = (char*)malloc((size_t)length + 1L)) == NULL) {
			fclose(stream);
			errormsg(WHERE, ERR_ALLOC, fpath, (int)length + 1L);
			return false;
		}
		l = fread(buf, 1, (size_t)length, stream);
		fclose(stream);
		if (l != length)
			errormsg(WHERE, ERR_READ, fpath, (int)length);
		else {
			buf[l] = 0;
			if ((mode & P_UTF8) && (term->charset() != CHARSET_UTF8))
				utf8_normalize_str(buf);
			putmsg(buf, mode, org_cols, obj);
		}
		free(buf);
	} else {    // Line-at-a-time mode
		uint             sys_status_sav = sys_status;
		enum output_rate output_rate = term->cur_output_rate;
		uint             org_line_delay = line_delay;
		uint             tmpatr = curatr;
		uint             orgcon = console;

		attr_sp = 0;    /* clear any saved attributes */
		off_t* offset = nullptr;
		size_t line=0;
		size_t lines=0;
		size_t last_match = SIZE_MAX;    // line of most recent search match (for 'n'/'N' continuation)
		char key = 0;
		char find_str[128] = {0};
		bool_expr_t* find_expr = NULL;   // compiled boolean expression for '/' search
		int kmode = 0;    // case-sensitive: 'n' (next match) and 'N' (previous match)
		if ((sys_status & SS_USERON) && !(useron.misc & (NOPAUSESPIN)) && cfg.spinning_pause_prompt)
			kmode |= K_SPIN;

		if (!pause_enabled())
			mode &= ~P_SEEK;

		if (mode & P_SEEK)
			mode |= P_NOPAUSE;
		if (!(mode & P_SAVEATR))
			attr(LIGHTGRAY);
		if (mode & P_NOPAUSE)
			sys_status |= SS_PAUSEOFF;

		if (length > PRINTFILE_MAX_LINE_LEN)
			length = PRINTFILE_MAX_LINE_LEN;
		if ((buf = (char*)malloc((size_t)length + 1L)) == NULL) {
			fclose(stream);
			errormsg(WHERE, ERR_ALLOC, fpath, (int)length + 1L);
			return false;
		}

		uint lncntr = 0; // term->lncntr doesn't increment for initial blank lines
		ansiParser.reset();
		int cols = (mode & P_SEEK) ? term->cols : 0;
		while (!feof(stream) && !msgabort()) {
			off_t o = ftello(stream);
			bool  at_eof = false;
			if (fgetline(buf, (size_t)length + 1, cols, stream, mode) == NULL) {
				if (!(mode & P_SEEK))
					break;
				// P_SEEK at EOF: fall through to the prompt (don't exit silently and don't
				// reposition the screen — 'less'-style: leave content where it is).
				at_eof = true;
			}
			if (!at_eof) {
				truncnl(buf);
				if ((mode & P_SEEK) && line == lines) {
					++lines;
					if ((offset = static_cast<off_t *>(realloc_or_free(offset, lines * sizeof *offset))) == nullptr) {
						errormsg(WHERE, ERR_ALLOC, fpath, lines * sizeof *offset);
						break;
					}
					offset[line] = o;
				}
				if ((mode & P_UTF8) && (term->charset() != CHARSET_UTF8))
					utf8_normalize_str(buf);
				if (putmsgfrag(buf, mode, org_cols, obj) != '\0') // early-EOF?
					break;
				if (term->bstrlen(buf, mode) < 1 || term->column > 0)
					term->newline();
				++lncntr;
			}
			if ((mode & P_SEEK) && (at_eof || lncntr == term->rows - 1 || key == TERM_KEY_DOWN || key == '\r')) {
				size_t nextline = line;
				bool   reprompt;
				do {
					reprompt = false;
					lncntr = 0;
					nextline = line;
					int    curatr = term->curatr;
					double progress = (double)filelength(file) / ftell(stream);
					bprintf(P_ATCODES, text[SeekPrompt], (int)(progress ? (100.0 / progress) : 0), getfname(fpath));
					key = getkey(kmode);
					// 'N' is reserved here for backward search ('less'-style); only quit_key aborts.
					// toupper() lets users press 'q' or 'Q' to quit (no longer using K_UPPER on getkey()).
					if (toupper(key) == quit_key())
						sys_status |= SS_ABORT;
					attr(curatr);
					term->carriage_return();
					term->cleartoeol();
					switch (key) {
						case TERM_KEY_HOME:
							nextline = 0;
							break;
						case TERM_KEY_UP:
							if (line <= term->rows - 1)
								nextline = 0;
							else
								nextline = line - (term->rows - 1);
							break;
						case 'b':
						case 'B':
						case TERM_KEY_PAGEUP:
							if (line <= ((term->rows - 1) * 2) - 1)
								nextline = 0;
							else
								nextline = line - (((term->rows - 1) * 2) - 1);
							break;
						case TERM_KEY_END:
						{
							if (lines < 1)
								break;
							bputs(text[SeekingFile]);
							if (fseeko(stream, offset[lines - 1], SEEK_SET) != 0) {
								errormsg(WHERE, ERR_SEEK, fpath, static_cast<int>(offset[lines - 1]));
								break;
							}
							if (fgetline(buf, (size_t)length + 1, cols, stream, mode) == NULL)
								break;
							size_t lastline = lines - 1;
							while (!feof(stream) && !msgabort()) {
								o = ftello(stream);
								if (fgetline(buf, (size_t)length + 1, cols, stream, mode) == NULL)
									break;
								++lastline;
								if (lastline >= lines) {
									++lines;
									if ((offset = static_cast<off_t*>(realloc_or_free(offset, lines * sizeof *offset))) == nullptr) {
										errormsg(WHERE, ERR_ALLOC, fpath, lines * sizeof *offset);
										break;
									}
									offset[lastline] = o;
								}
							}
							bputs(text[SeekingFileDone]);
							if (lines <= term->rows - 1)
								nextline = 0;
							else
								nextline = lines - (term->rows - 1);
							break;
						}
						case '/':
						case 'n':    // 'less'-style: next match (search forward)
						{
							if (key == '/') {
								bputs(text[SearchStringPrompt]);
								size_t input_len = getstr(find_str, sizeof(find_str) - 1, K_LINE | K_NOCRLF);
								// K_NOCRLF leaves the cursor at end of input; clear the prompt line.
								term->carriage_return();
								term->cleartoeol();
								if (input_len == 0) {
									find_str[0] = '\0';
									bool_expr_free(find_expr);
									find_expr = NULL;
									reprompt = true;
									break;
								}
								char*        new_errmsg = NULL;
								bool_expr_t* new_expr   = bool_expr_compile(find_str, &new_errmsg);
								if (new_expr == NULL) {
									bprintf(text[InvalidSearchExpression],
									        new_errmsg != NULL ? new_errmsg : "(?)");
									free(new_errmsg);
									find_str[0] = '\0';
									reprompt = true;
									break;
								}
								bool_expr_free(find_expr);
								find_expr = new_expr;
							}
							if (find_expr == NULL) {
								reprompt = true;
								break;
							}
							// '/' starts a fresh search from the start of the file. 'n' continues
							// from the line right after the previous match (less-style), so matches
							// still visible on screen above the prompt are reachable. Falls back to
							// line+1 if there's no prior match (e.g., user navigated manually since).
							if (key == '/')
								last_match = SIZE_MAX;
							size_t scan_line = (key == '/') ? 0
							                 : (last_match != SIZE_MAX) ? last_match + 1
							                                            : line + 1;
							bputs(text[SeekingFile]);
							off_t saved_pos = ftello(stream);
							bool  found = false;
							if (scan_line < lines && fseeko(stream, offset[scan_line], SEEK_SET) != 0) {
								errormsg(WHERE, ERR_SEEK, fpath, static_cast<int>(offset[scan_line]));
								break;
							}
							while (!feof(stream) && !msgabort()) {
								off_t scan_o = ftello(stream);
								if (fgetline(buf, (size_t)length + 1, cols, stream, mode) == NULL)
									break;
								if (scan_line >= lines) {
									++lines;
									if ((offset = static_cast<off_t*>(realloc_or_free(offset, lines * sizeof *offset))) == nullptr) {
										errormsg(WHERE, ERR_ALLOC, fpath, lines * sizeof *offset);
										break;
									}
									offset[scan_line] = scan_o;
								}
								if (bool_expr_match(find_expr, buf)) {
									found = true;
									nextline = scan_line;
									break;
								}
								++scan_line;
							}
							bputs(text[SeekingFileDone]);
							if (offset == nullptr)
								break;
							if (found) {
								// Peek forward up to rows-1 lines past the match to see if it's
								// in the last page of the file. If so, position the display so
								// the screen shows the last full page (match near the bottom)
								// instead of just match + EOF (which leaves leftover content
								// above). Otherwise, match goes at the top of the screen ('less'
								// behavior).
								size_t target = scan_line;
								bool   near_eof = false;
								for (size_t i = 1; i < (size_t)(term->rows - 1); i++) {
									off_t peek_o = ftello(stream);
									if (fgetline(buf, (size_t)length + 1, cols, stream, mode) == NULL) {
										near_eof = true;
										break;
									}
									size_t peek_line = scan_line + i;
									if (peek_line >= lines) {
										++lines;
										if ((offset = static_cast<off_t*>(realloc_or_free(offset, lines * sizeof *offset))) == nullptr) {
											errormsg(WHERE, ERR_ALLOC, fpath, lines * sizeof *offset);
											break;
										}
										offset[peek_line] = peek_o;
									}
								}
								if (offset == nullptr)
									break;
								if (near_eof && lines > (size_t)(term->rows - 1))
									target = lines - (term->rows - 1);
								if (fseeko(stream, offset[target], SEEK_SET) != 0) {
									errormsg(WHERE, ERR_SEEK, fpath, static_cast<int>(offset[target]));
									break;
								}
								clearerr(stream);
								nextline = target;
								last_match = scan_line;
							} else {
								// Not found: restore file position, show message, and re-prompt
								// (don't advance the file display — 'less'-style).
								if (saved_pos >= 0)
									(void)fseeko(stream, saved_pos, SEEK_SET);
								clearerr(stream);
								bputs(text[FindStringNotFound]);
								reprompt = true;
							}
							break;
						}
						case 'N':    // 'less'-style: previous match (search backward)
						{
							if (find_str[0] == '\0') {
								reprompt = true;
								break;
							}
							bputs(text[SeekingFile]);
							off_t saved_pos = ftello(stream);
							bool  found = false;
							// Search backward starting from before the previous match (less-style),
							// so 'N' steps through visible matches above the prompt.
							// offset[0..line] is populated (we've scrolled through those lines).
							size_t scan_start = (last_match != SIZE_MAX && last_match <= line) ? last_match : line;
							for (size_t i = scan_start; i > 0; ) {
								--i;
								if (fseeko(stream, offset[i], SEEK_SET) != 0) {
									errormsg(WHERE, ERR_SEEK, fpath, static_cast<int>(offset[i]));
									break;
								}
								if (fgetline(buf, (size_t)length + 1, cols, stream, mode) == NULL)
									break;
								if (strcasestr(buf, find_str) != NULL) {
									found = true;
									nextline = i;
									last_match = i;
									break;
								}
							}
							bputs(text[SeekingFileDone]);
							if (!found) {
								if (saved_pos >= 0)
									(void)fseeko(stream, saved_pos, SEEK_SET);
								clearerr(stream);
								bputs(text[FindStringNotFound]);
								reprompt = true;
							}
							break;
						}
						case '?':    // Display help (key listing); re-prompt afterward
							bputs(text[SeekHelp]);
							reprompt = true;
							break;
						case TERM_KEY_PAGEDN:
							if (feof(stream)) {
								// Already at EOF: stay (re-prompt) instead of advancing
								reprompt = true;
								break;
							}
							// Fall-through
						default:
						case TERM_KEY_DOWN:
							nextline = line + 1;
							break;
					}
				} while (reprompt && offset != nullptr && !msgabort());
				if (offset == nullptr)
					break;
				if (key != '/' && key != 'n' && key != 'N' && key != '?')
					last_match = SIZE_MAX;  // user navigated manually; clear search anchor
				if ((key == TERM_KEY_END || nextline != line + 1) && nextline < lines) {
					if (fseeko(stream, offset[nextline], 0) != 0) {
						errormsg(WHERE, ERR_SEEK, fpath, static_cast<int>(offset[nextline]));
						break;
					}
				}
				line = nextline;
			}
			else if (!at_eof)
				++line;
		}
		if (ansiParser.current_state() != ansiState_none)
			lprintf(LOG_DEBUG, "Incomplete ANSI stripped from end");
		memcpy(rainbow, cfg.rainbow, sizeof rainbow);
		free(buf);
		fclose(stream);
		free(offset);
		bool_expr_free(find_expr);
		if (!(mode & P_SAVEATR)) {
			console = orgcon;
			attr(tmpatr);
		}
		if (!(mode & P_NOATCODES) && term->cur_output_rate != output_rate)
			term->set_output_rate(output_rate);
		if (mode & P_PETSCII)
			term_out(PETSCII_UPPERLOWER);
		attr_sp = 0;  /* clear any saved attributes */

		/* Restore original settings of Forced Pause On/Off */
		sys_status &= ~(SS_PAUSEOFF | SS_PAUSEON);
		sys_status |= (sys_status_sav & (SS_PAUSEOFF | SS_PAUSEON));

		line_delay = org_line_delay;
	}

	if ((mode & P_NOABORT || rip) && online == ON_REMOTE) {
		sync();
		rioctl(IOSM | ABORT);
	}
	if (rip)
		term->getdims();
	console = savcon;

	return true;
}

bool sbbs_t::printtail(const char* fname, int lines, int mode, int org_cols, JSObject* obj)
{
	char* buf;
	char  fpath[MAX_PATH + 1];
	char* p;
	FILE* fp;
	int   file, cur = 0;
	int   length, l;

	SAFECOPY(fpath, fname);
	(void)fexistcase(fpath);
	if (mode & P_NOABORT) {
		if (online == ON_REMOTE) {
			rioctl(IOCM | ABORT);
			rioctl(IOCS | ABORT);
		}
		clearabort();
	}
	if ((fp = fnopen(&file, fpath, O_RDONLY | O_DENYNONE)) == NULL) {
		if (!(mode & P_NOERROR)) {
			lprintf(LOG_NOTICE, "!Error %d (%s) opening: %s"
			        , errno, strerror(errno), fpath);
			bputs(text[FileNotFound]);
			if (useron_is_sysop())
				bputs(fpath);
			term->newline();
		}
		return false;
	}
	length = (int)filelength(file);
	if (length < 0) {
		fclose(fp);
		errormsg(WHERE, ERR_CHK, fpath, length);
		return false;
	}

	lprintf(LOG_DEBUG, "Printing tail: %s", fpath);
	if (!(mode & P_NOCRLF) && term->row > 0) {
		term->newline();
	}

	if (length > lines * PRINTFILE_MAX_LINE_LEN) {
		length = lines * PRINTFILE_MAX_LINE_LEN;
		(void)fseek(fp, -length, SEEK_END);
	}
	if ((buf = (char*)malloc(length + 1L)) == NULL) {
		fclose(fp);
		errormsg(WHERE, ERR_ALLOC, fpath, length + 1L);
		return false;
	}
	l = fread(buf, sizeof(char), length, fp);
	fclose(fp);
	if (l != length)
		errormsg(WHERE, ERR_READ, fpath, length);
	else {
		buf[l] = 0;
		p = (buf + l) - 1;
		if (*p == LF)
			p--;
		while (*p && p > buf) {
			if (*p == LF)
				cur++;
			if (cur >= lines) {
				p++;
				break;
			}
			p--;
		}
		putmsg(p, mode, org_cols, obj);
	}
	if (mode & P_NOABORT && online == ON_REMOTE) {
		sync();
		rioctl(IOSM | ABORT);
	}
	free(buf);
	return true;
}

/****************************************************************************/
/* Displays a menu file (e.g. from the text/menu directory)                 */
/* Pass a code including wildcards (* or ?) to display a randomly-chosen	*/
/* file matching the pattern in 'code'										*/
/****************************************************************************/
bool sbbs_t::menu(const char *code, int mode, JSObject* obj)
{
	char        path[MAX_PATH + 1];
	const char *next = "msg";
	const char *last = "asc";

	if (*useron.lang != '\0' && strchr(code, '/') == NULL) {
		snprintf(path, sizeof path, "%s/%s", useron.lang, code);
		if (menu(path, mode | P_NOERROR, obj))
			return true;
	}

	if (strcspn(code, "*?") != strlen(code))
		return random_menu(code, mode, obj);

	clearabort();
	if (menu_file[0])
		SAFECOPY(path, menu_file);
	else {
		do {
			if ((term->supports(RIP)) && menu_exists(code, "rip", path))
				break;
			if ((term->supports(ANSI) && (!term->supports(COLOR))) && menu_exists(code, "mon", path))
				break;
			if ((term->supports(ANSI)) && menu_exists(code, "ans", path))
				break;
			if ((term->charset() == CHARSET_PETSCII) && menu_exists(code, "seq", path))
				break;
			if (term->charset() == CHARSET_ASCII) {
				next = "asc";
				last = "msg";
			}
			if (menu_exists(code, next, path))
				break;
			if (!menu_exists(code, last, path)) {
				if (!(mode & P_NOERROR))
					errormsg(WHERE, ERR_CHK, path);
				return false;
			}
		} while (0);
	}

	mode |= P_OPENCLOSE | P_CPM_EOF;
	if (term->column == 0)
		mode |= P_NOCRLF;
	return printfile(path, mode, /* org_cols: */ 0, obj);
}

//****************************************************************************
// Prompt the user for a boolean-search query string. If the user enters a
// lone '?', display the "boolsrch" help menu and re-prompt. Returns true
// with the input in str on a real entry; false if the user aborted (empty
// input or SS_ABORT). kmode is the getstr() flag set the caller wants.
//****************************************************************************
bool sbbs_t::get_search_string(char* str, size_t maxlen, int kmode)
{
	while (online && !(sys_status & SS_ABORT)) {
		bputs(text[SearchStringPrompt]);
		if (!getstr(str, maxlen, kmode))
			return false;
		if (strcmp(str, "?") != 0)
			return true;
		menu("boolsrch");
	}
	return false;
}

//****************************************************************************
// Check (return true) if a menu file exists with specified type/extension
// 'path' buffer must be at least (MAX_PATH + 1) bytes in size
//****************************************************************************
bool sbbs_t::menu_exists(const char *code, const char* ext, char* path)
{
	char pathbuf[MAX_PATH + 1];
	if (path == NULL)
		path = pathbuf;

	if (menu_file[0]) {
		strncpy(path, menu_file, MAX_PATH);
		return fexistcase(path) ? true : false;
	}

	/* Either <menu>.asc or <menu>.msg or <menu>.ans is required */
	if (ext == NULL)
		return menu_exists(code, "asc", path)
		       || menu_exists(code, "msg", path)
		       || menu_exists(code, "ans", path);

	char prefix[MAX_PATH];
	if (isfullpath(code))
		SAFECOPY(prefix, code);
	else {
		char subdir[MAX_PATH + 1];
		SAFECOPY(subdir, menu_dir);
		backslash(subdir);
		if (*code == '.')
			*subdir = '\0';
		SAFEPRINTF3(prefix, "%smenu/%s%s", cfg.text_dir, subdir, code);
		FULLPATH(path, prefix, MAX_PATH);
		SAFECOPY(prefix, path);
		if (cfg.mods_dir[0] != '\0') {
			char modprefix[MAX_PATH + 1];
			char modpath[MAX_PATH + 1];
			snprintf(modprefix, sizeof modprefix, "%stext/menu/%s%s", cfg.mods_dir, subdir, code);
			snprintf(modpath, sizeof modpath, "%s.%s", modprefix, ext);
			FULLPATH(path, modpath, MAX_PATH);
			SAFECOPY(modpath, path);
			if (fexist(modpath)) {
				FULLPATH(path, modprefix, MAX_PATH);
				SAFECOPY(prefix, path);
			}
		}
	}
	// Display specified EXACT width file
	safe_snprintf(path, MAX_PATH, "%s.%ucol.%s", prefix, term->cols, ext);
	if (fexistcase(path))
		return true;
	// Display specified MINIMUM width file
	glob_t g = {0};
	safe_snprintf(path, MAX_PATH, "%s.c*.%s", prefix, ext);
	if (globi(path, GLOB_NOESCAPE | GLOB_MARK, NULL, &g) == 0) {
		char*  p;
		char   term[MAX_PATH + 1];
		safe_snprintf(term, sizeof(term), ".%s", ext);
		size_t skip = safe_snprintf(path, MAX_PATH, "%s.c", prefix);
		unsigned max = 0;
		for (size_t i = 0; i < g.gl_pathc; i++) {
			unsigned long c = strtoul(g.gl_pathv[i] + skip, &p, 10);
			if (stricmp(p, term) != 0) // Some other weird pattern ending in c*.<ext>
				continue;
			if (c <= this->term->cols && c > max) {
				max = c;
				safe_snprintf(path, MAX_PATH, "%s", g.gl_pathv[i]);
			}
		}
		globfree(&g);
		if (max > 0)
			return true;
	}

	safe_snprintf(path, MAX_PATH, "%s.%s", prefix, ext);
	return fexistcase(path) ? true : false;
}

/****************************************************************************/
/* Displays a random menu file (e.g. from the text/menu directory)          */
/****************************************************************************/
bool sbbs_t::random_menu(const char *name, int mode, JSObject* obj)
{
	char       path[MAX_PATH + 1];
	glob_t     g = {0};
	str_list_t names = NULL;

	SAFEPRINTF2(path, "%smenu/%s", cfg.text_dir, name);
	if (cfg.mods_dir[0] != '\0') {
		char modpath[MAX_PATH + 1];
		char tmppath[MAX_PATH + 1];
		SAFEPRINTF2(modpath, "%stext/menu/%s", cfg.mods_dir, name);
		FULLPATH(tmppath, modpath, sizeof tmppath);
		SAFECOPY(modpath, tmppath);
		if (fexist(modpath))
			SAFECOPY(path, modpath);
	}
	if (glob(path, GLOB_NOESCAPE | GLOB_MARK, NULL, &g) != 0) {
		return false;
	}
	for (size_t i = 0; i < g.gl_pathc; i++) {
		char* ext = strchr(getfname(g.gl_pathv[i]), '.'); // intentionally not using getfext() - issue #380
		if (ext == NULL)
			continue;
		*ext = 0;
		strListPush(&names, g.gl_pathv[i]);
	}
	globfree(&g);
	strListDedupe(&names, /* case_sensitive: */ true);
	bool   result = false;
	size_t count = strListCount(names);
	while (count > 0) {
		size_t i = sbbs_random(count);
		if (menu_exists(names[i], NULL, path)) {
			result = menu(names[i], mode, obj);
			if (result == true)
				break;
		}
		strListFastDelete(names, i, 1);
		--count;
	}
	strListFree(&names);
	return result;
}
