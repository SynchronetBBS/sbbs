/* Synchronet high-level string i/o routines */

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
#include "dat_rec.h"

/****************************************************************************/
// Returns 0-based text string index
// Caches the result
/****************************************************************************/
int sbbs_t::get_text_num(const char* id)
{
	int i;
	if (IS_DIGIT(*id)) {
		i = atoi(id);
		if (i < 1)
			return TOTAL_TEXT;
		return i - 1;
	}
	auto index = text_id_map.find(id);
	if (index != text_id_map.end())
		i = index->second;
	else {
		for (i = 0; i < TOTAL_TEXT; ++i) {
			if (strcmp(text_id[i], id) == 0) {
				text_id_map[id] = i;
				break;
			}
		}
	}
	return i;
}

/****************************************************************************/
/****************************************************************************/
const char* sbbs_t::get_text(const char* id)
{
	int i = get_text_num(id);
	if (i >= 0 && i < TOTAL_TEXT)
		return text[i];
	return NULL;
}

/****************************************************************************/
/* Somewhat copied from load_cfg()											*/
/****************************************************************************/
bool sbbs_t::replace_text(const char* path)
{
	FILE* fp;

	if ((fp = fnopen(NULL, path, O_RDONLY)) != NULL) {
		char             buf[INI_MAX_VALUE_LEN];
		bool             success = true;
		str_list_t       ini = iniReadFile(fp);
		fclose(fp);

		named_string_t** list = iniGetNamedStringList(ini, "substr");
		for (size_t n = 0; list != NULL && n < TOTAL_TEXT; ++n) {
			replace_named_values(text[n], buf, sizeof buf, /* escape_seq */ NULL,list, /* str_list: */NULL, /* int_list : */ NULL, /* case_sensitive: */ true);
			if (strcmp(text[n], buf) == 0)
				continue;
			if (text[n] != text_sav[n] && text[n] != nulstr)
				free(text[n]);
			text[n] = strdup(buf);
			text_replaced[n] = true;
		}
		iniFreeNamedStringList(list);

		list = iniGetNamedStringList(ini, ROOT_SECTION);
		for (size_t i = 0; list != NULL && list[i] != NULL; ++i) {
			int n = get_text_num(list[i]->name);
			if (n >= TOTAL_TEXT) {
				lprintf(LOG_ERR, "%s text ID (%s) not recognized"
				        , path
				        , list[i]->name);
				success = false;
				break;
			}
			if (text[n] != text_sav[n] && text[n] != nulstr)
				free(text[n]);
			if (*list[i]->value == '\0')
				text[n] = (char *)nulstr;
			else
				text[n] = strdup(list[i]->value);
			text_replaced[n] = true;
		}
		iniFreeNamedStringList(list);
		iniFreeStringList(ini);
		return success;
	}
	return false;
}

/****************************************************************************/
/* Only reverts text that was replaced via replace_text()					*/
/****************************************************************************/
void sbbs_t::revert_text(void)
{
	for (size_t i = 0; i < TOTAL_TEXT; ++i) {
		if (text_replaced[i]) {
			if (text[i] != text_sav[i] && text[i] != nulstr)
				free(text[i]);
			text[i] = text_sav[i];
			text_replaced[i] = false;
		}
	}
}

/****************************************************************************/
/* Sysops can have custom text based on terminal character set (charset)	*/
/* Users can have their own language setting, make that current				*/
/****************************************************************************/
bool sbbs_t::load_user_text(void)
{
	char path[MAX_PATH + 1];

	revert_text();

	char connection[LEN_CONNECTION + 1];
	SAFECOPY(connection, this->connection);
	strlwr(connection);
	snprintf(path, sizeof path, "%s%s/text.ini", cfg.ctrl_dir, connection);
	if (fexist(path)) {
		if (!replace_text(path))
			return false;
	}
	char charset[16];
	SAFECOPY(charset, term->charset_str());
	strlwr(charset);
	snprintf(path, sizeof path, "%s%s/text.ini", cfg.ctrl_dir, charset);
	if (fexist(path)) {
		if (!replace_text(path))
			return false;
	}
	if (*useron.lang == '\0')
		return true;
	snprintf(path, sizeof path, "%s%s/text.%s.ini", cfg.ctrl_dir, charset, useron.lang);
	if (fexist(path)) {
		if (!replace_text(path))
			return false;
	}
	snprintf(path, sizeof path, "%stext.%s.ini", cfg.ctrl_dir, useron.lang);
	return replace_text(path);
}

char* sbbs_t::format_text(int /* enum text */ num, ...)
{
	expand_atcodes(text[num], format_text_buf, sizeof format_text_buf, /* remsg: */ NULL);
	if (strcmp(text[num], format_text_buf) == 0) { // No @-codes expanded
		va_list args;
		va_start(args, num);
		vsnprintf(format_text_buf, sizeof format_text_buf, text[num], args);
		va_end(args);
	}
	return format_text_buf;
}

char* sbbs_t::format_text(enum text num, smbmsg_t* remsg, ...)
{
	expand_atcodes(text[num], format_text_buf, sizeof format_text_buf, remsg);
	if (strcmp(text[num], format_text_buf) == 0) { // No @-codes expanded
		va_list args;
		va_start(args, remsg);
		vsnprintf(format_text_buf, sizeof format_text_buf, text[num], args);
		va_end(args);
	}
	return format_text_buf;
}

/****************************************************************************/
/* Lists all users who have access to the current sub.                      */
/****************************************************************************/
void sbbs_t::userlist(int mode)
{
	char   name[256], sort = 0;
	char   tmp[512];
	int    i, j, k, users = 0;
	char * line[2500];
	user_t user;

	bool invoked;
	exec_mod("user list", cfg.userlist_mod, &invoked, "%d", mode);
	if (invoked)
		return;

	if (lastuser(&cfg) <= (int)(sizeof(line) / sizeof(line[0])))
		sort = yesno(text[SortAlphaQ]);
	if (sort) {
		bputs(text[CheckingSlots]);
	}
	else {
		term->newline();
	}
	j = 0;
	k = lastuser(&cfg);
	int userfile = openuserdat(&cfg, /* for_modify: */ FALSE);
	for (i = 1; i <= k && !msgabort(); i++) {
		if (sort && (online == ON_LOCAL || !rioctl(TXBC)))
			bprintf("%-4d\b\b\b\b", i);
		user.number = i;
		if (fgetuserdat(&cfg, &user, userfile) != 0)
			continue;
		if (user.alias[0] <= ' ')
			continue;
		if (user.misc & (DELETED | INACTIVE))
			continue;
		users++;
		if (mode == UL_SUB) {
			if (!usrgrps)
				continue;
			if (!chk_ar(cfg.grp[usrgrp[curgrp]]->ar, &user, /* client: */ NULL))
				continue;
			if (!chk_ar(cfg.sub[usrsub[curgrp][cursub[curgrp]]]->ar, &user, /* client: */ NULL)
			    || (cfg.sub[usrsub[curgrp][cursub[curgrp]]]->read_ar[0]
			        && !chk_ar(cfg.sub[usrsub[curgrp][cursub[curgrp]]]->read_ar, &user, /* client: */ NULL)))
				continue;
		}
		else if (mode == UL_DIR) {
			if (user.rest & FLAG('T'))
				continue;
			if (!usrlibs)
				continue;
			if (!chk_ar(cfg.lib[usrlib[curlib]]->ar, &user, /* client: */ NULL))
				continue;
			if (!chk_ar(cfg.dir[usrdir[curlib][curdir[curlib]]]->ar, &user, /* client: */ NULL))
				continue;
		}
		if (sort) {
			if ((line[j] = (char *)malloc(128)) == 0) {
				closeuserdat(userfile);
				errormsg(WHERE, ERR_ALLOC, nulstr, 83);
				for (i = 0; i < j; i++)
					free(line[i]);
				return;
			}
			snprintf(name, sizeof name, "%s #%d", user.alias, i);
			snprintf(line[j], 128, text[UserListFmt], name
			         , cfg.sys_misc & SM_LISTLOC ? user.location : user.note
			         , datestr(user.laston, tmp)
			         , user.connection);
		}
		else {
			snprintf(name, sizeof name, "%s #%u", user.alias, i);
			bprintf(text[UserListFmt], name
			        , cfg.sys_misc & SM_LISTLOC ? user.location : user.note
			        , datestr(user.laston, tmp)
			        , user.connection);
		}
		j++;
	}
	closeuserdat(userfile);
	if (i <= k) {  /* aborted */
		if (sort)
			for (i = 0; i < j; i++)
				free(line[i]);
		return;
	}
	if (!sort) {
		term->newline();
	}
	bprintf(text[NTotalUsers], users);
	if (mode == UL_SUB)
		bprintf(text[NUsersOnCurSub], j);
	else if (mode == UL_DIR)
		bprintf(text[NUsersOnCurDir], j);
	if (!sort)
		return;
	term->newline();
	qsort((void *)line, j, sizeof(line[0])
	      , (int (*)(const void*, const void*)) pstrcmp);
	for (i = 0; i < j && !msgabort(); i++)
		bputs(line[i]);
	for (i = 0; i < j; i++)
		free(line[i]);
}

/****************************************************************************/
/* SIF input function. See SIF.DOC for more info        					*/
/****************************************************************************/
void sbbs_t::sif(char *fname, char *answers, int len)
{
	char str[256], tmplt[256], *buf;
	uint t, max, min, mode, cr;
	int  file;
	int  length, l = 0, m, top, a = 0;

	*answers = 0;
	snprintf(str, sizeof str, "%s%s.sif", cfg.text_dir, fname);
	if ((file = nopen(str, O_RDONLY)) == -1) {
		errormsg(WHERE, ERR_OPEN, str, O_RDONLY);
		return;
	}
	length = (int)filelength(file);
	if (length < 0) {
		close(file);
		errormsg(WHERE, ERR_CHK, str, length);
		return;
	}
	if ((buf = (char *)calloc(length + 1, 1)) == 0) {
		close(file);
		errormsg(WHERE, ERR_ALLOC, str, length);
		return;
	}
	if (read(file, buf, length) != length) {
		close(file);
		free(buf);
		errormsg(WHERE, ERR_READ, str, length);
		return;
	}
	close(file);
	while (l < length && online) {
		mode = min = max = t = cr = 0;
		top = l;
		while (l < length && buf[l++] != STX);
		for (m = l; m < length; m++)
			if (buf[m] == ETX || !buf[m]) {
				buf[m] = 0;
				break;
			}
		if (l >= length)
			break;
		if (online == ON_REMOTE) {
			rioctl(IOCM | ABORT);
			rioctl(IOCS | ABORT);
		}
		putmsg(buf + l, P_SAVEATR);
		m++;
		if (toupper(buf[m]) != 'C' && toupper(buf[m]) != 'S')
			continue;
		sync();
		if (online == ON_REMOTE)
			rioctl(IOSM | ABORT);
		if (a >= len) {
			errormsg(WHERE, ERR_LEN, fname, len);
			break;
		}
		if ((buf[m] & 0xdf) == 'C') {
			if ((buf[m + 1] & 0xdf) == 'U') {      /* Uppercase only */
				mode |= K_UPPER;
				m++;
			}
			else if ((buf[m + 1] & 0xdf) == 'N') { /* Numbers only */
				mode |= K_NUMBER;
				m++;
			}
			if ((buf[m + 1] & 0xdf) == 'L') {      /* Draw line */
				if (term->supports(COLOR))
					attr(cfg.color[clr_inputline]);
				else
					attr(BLACK | BG_LIGHTGRAY);
				bputs(" \b");
				m++;
			}
			if ((buf[m + 1] & 0xdf) == 'R') {      /* Add term->newline() */
				cr = 1;
				m++;
			}
			if (buf[m + 1] == '"') {
				m += 2;
				for (l = m; l < length; l++)
					if (buf[l] == '"') {
						buf[l] = 0;
						break;
					}
				answers[a++] = (char)getkeys((char *)buf + m, 0);
			}
			else {
				answers[a] = getkey(mode);
				outchar(answers[a++]);
				attr(LIGHTGRAY);
				term->newline();
			}
			if (cr) {
				answers[a++] = CR;
				answers[a++] = LF;
			}
		}
		else if ((buf[m] & 0xdf) == 'S') {       /* String */
			if ((buf[m + 1] & 0xdf) == 'U') {      /* Uppercase only */
				mode |= K_UPPER;
				m++;
			}
			else if ((buf[m + 1] & 0xdf) == 'F') { /* Force Upper/Lowr case */
				mode |= K_UPRLWR;
				m++;
			}
			else if ((buf[m + 1] & 0xdf) == 'N') { /* Numbers only */
				mode |= K_NUMBER;
				m++;
			}
			if ((buf[m + 1] & 0xdf) == 'L') {      /* Draw line */
				mode |= K_LINE;
				m++;
			}
			if ((buf[m + 1] & 0xdf) == 'R') {      /* Add term->newline() */
				cr = 1;
				m++;
			}
			if (IS_DIGIT(buf[m + 1])) {
				max = buf[++m] & 0xf;
				if (IS_DIGIT(buf[m + 1]))
					max = max * 10 + (buf[++m] & 0xf);
			}
			if (buf[m + 1] == '.' && IS_DIGIT(buf[m + 2])) {
				m++;
				min = buf[++m] & 0xf;
				if (IS_DIGIT(buf[m + 1]))
					min = min * 10 + (buf[++m] & 0xf);
			}
			if (buf[m + 1] == '"') {
				m++;
				mode &= ~K_NUMBER;
				while (buf[++m] != '"' && t < 80)
					tmplt[t++] = buf[m];
				tmplt[t] = 0;
				max = strlen(tmplt);
			}
			if (t) {
				if (gettmplt(str, tmplt, mode) < min) {
					l = top;
					continue;
				}
			}
			else {
				if (!max)
					continue;
				if (getstr(str, max, mode) < min) {
					l = top;
					continue;
				}
			}
			if (!cr) {
				for (cr = 0; str[cr]; cr++)
					answers[a + cr] = str[cr];
				while (cr < max)
					answers[a + cr++] = ETX;
				a += max;
			}
			else {
				putrec(answers, a, max, str);
				putrec(answers, a + max, 2, crlf);
				a += max + 2;
			}
		}
	}
	answers[a] = 0;
	free((char *)buf);
}

/****************************************************************************/
/* SIF output function. See SIF.DOC for more info        					*/
/****************************************************************************/
void sbbs_t::sof(char *fname, char *answers, int len)
{
	char str[256], *buf, max, min, cr;
	int  file;
	int  length, l = 0, m, a = 0;

	*answers = '\0';
	snprintf(str, sizeof str, "%s%s.sif", cfg.text_dir, fname);
	if ((file = nopen(str, O_RDONLY)) == -1) {
		errormsg(WHERE, ERR_OPEN, str, O_RDONLY);
		return;
	}
	length = (int)filelength(file);
	if (length < 0) {
		close(file);
		errormsg(WHERE, ERR_LEN, str, length);
		return;
	}
	if ((buf = (char *)calloc(length + 1, 1)) == 0) {
		close(file);
		errormsg(WHERE, ERR_ALLOC, str, length);
		return;
	}
	if (read(file, buf, length) != length) {
		close(file);
		errormsg(WHERE, ERR_READ, str, length);
		free(buf);
		return;
	}
	close(file);
	while (l < length && online) {
		min = max = cr = 0;
		while (l < length && buf[l++] != STX);
		for (m = l; m < length; m++)
			if (buf[m] == ETX || !buf[m]) {
				buf[m] = 0;
				break;
			}
		if (l >= length)
			break;
		if (online == ON_REMOTE) {
			rioctl(IOCM | ABORT);
			rioctl(IOCS | ABORT);
		}
		putmsg(buf + l, P_SAVEATR);
		m++;
		if (toupper(buf[m]) != 'C' && toupper(buf[m]) != 'S')
			continue;
		sync();
		if (online == ON_REMOTE)
			rioctl(IOSM | ABORT);
		if (a >= len) {
			bprintf("\r\nSOF: %s defined more data than buffer size "
			        "(%u bytes)\r\n", fname, len);
			break;
		}
		if ((buf[m] & 0xdf) == 'C') {
			if ((buf[m + 1] & 0xdf) == 'U')        /* Uppercase only */
				m++;
			else if ((buf[m + 1] & 0xdf) == 'N')   /* Numbers only */
				m++;
			if ((buf[m + 1] & 0xdf) == 'L') {      /* Draw line */
				if (term->supports(COLOR))
					attr(cfg.color[clr_inputline]);
				else
					attr(BLACK | BG_LIGHTGRAY);
				bputs(" \b");
				m++;
			}
			if ((buf[m + 1] & 0xdf) == 'R') {      /* Add term->newline() */
				cr = 1;
				m++;
			}
			outchar(answers[a++]);
			attr(LIGHTGRAY);
			term->newline();
			if (cr)
				a += 2;
		}
		else if ((buf[m] & 0xdf) == 'S') {       /* String */
			if ((buf[m + 1] & 0xdf) == 'U')
				m++;
			else if ((buf[m + 1] & 0xdf) == 'F')
				m++;
			else if ((buf[m + 1] & 0xdf) == 'N')   /* Numbers only */
				m++;
			if ((buf[m + 1] & 0xdf) == 'L') {
				if (term->supports(COLOR))
					attr(cfg.color[clr_inputline]);
				else
					attr(BLACK | BG_LIGHTGRAY);
				m++;
			}
			if ((buf[m + 1] & 0xdf) == 'R') {
				cr = 1;
				m++;
			}
			if (IS_DIGIT(buf[m + 1])) {
				max = buf[++m] & 0xf;
				if (IS_DIGIT(buf[m + 1]))
					max = max * 10 + (buf[++m] & 0xf);
			}
			if (buf[m + 1] == '.' && IS_DIGIT(buf[m + 2])) {
				m++;
				min = buf[++m] & 0xf;
				if (IS_DIGIT(buf[m + 1]))
					min = min * 10 + (buf[++m] & 0xf);
			}
			if (buf[m + 1] == '"') {
				max = 0;
				m++;
				while (buf[++m] != '"' && max < 80)
					max++;
			}
			if (!max)
				continue;
			getrec(answers, a, max, str);
			bputs(str);
			attr(LIGHTGRAY);
			term->newline();
			if (!cr)
				a += max;
			else
				a += max + 2;
		}
	}
	free((char *)buf);
}

/****************************************************************************/
/* Creates data file 'datfile' from input via sif file 'siffile'            */
/****************************************************************************/
void sbbs_t::create_sif_dat(char *siffile, char *datfile)
{
	char *buf;
	int   file;

	if ((buf = (char *)malloc(SIF_MAXBUF)) == NULL) {
		errormsg(WHERE, ERR_ALLOC, siffile, SIF_MAXBUF);
		return;
	}
	memset(buf, 0, SIF_MAXBUF);    /* initialize to null */
	sif(siffile, buf, SIF_MAXBUF);
	if ((file = nopen(datfile, O_WRONLY | O_TRUNC | O_CREAT)) == -1) {
		free(buf);
		errormsg(WHERE, ERR_OPEN, datfile, O_WRONLY | O_TRUNC | O_CREAT);
		return;
	}
	int wr = write(file, buf, strlen(buf));
	close(file);
	if (wr < 0)
		errormsg(WHERE, ERR_WRITE, datfile, strlen(buf));
	free(buf);
}

/****************************************************************************/
/* Reads data file 'datfile' and displays output via sif file 'siffile'     */
/****************************************************************************/
void sbbs_t::read_sif_dat(char *siffile, char *datfile)
{
	char *buf;
	int   file;
	int   length;

	if ((file = nopen(datfile, O_RDONLY)) == -1) {
		errormsg(WHERE, ERR_OPEN, datfile, O_RDONLY);
		return;
	}
	length = (int)filelength(file);
	if (length <= 0) {
		close(file);
		return;
	}
	if ((buf = (char *)malloc(length)) == NULL) {
		close(file);
		errormsg(WHERE, ERR_ALLOC, datfile, length);
		return;
	}
	length = read(file, buf, length);
	if (length < 0)
		length = 0;
	close(file);
	sof(siffile, buf, length);
	free(buf);
}

static bool is_template_char(char ch)
{
	return ch == 'N' || ch == 'A' || ch == '!';
}

/****************************************************************************/
/* Get string by template. A=Alpha, N=Number, !=Anything                    */
/* First character MUST be an A,N or !.                                     */
/* Modes - K_EDIT, K_LINE and K_UPPER are supported.						*/
/****************************************************************************/
size_t sbbs_t::gettmplt(char *strout, const char *templt, int mode)
{
	char   ch;
	char   str[256]{};
	char   tmplt[128];
	size_t t = strlen(templt), c = 0;

	clearabort();
	SAFECOPY(tmplt, templt);
	strupr(tmplt);
	// MODE7: This was ANSI-only, added support for PETSCII, 
	//        but we may not want it for Mode7.
	if (mode & K_LINE) {
		if (term->supports(COLOR))
			attr(cfg.color[clr_inputline]);
		else
			attr(BLACK | BG_LIGHTGRAY);
	}
	while (c < t) {
		if (is_template_char(tmplt[c]))
			outchar(' ');
		else
			outchar(tmplt[c]);
		c++;
	}
	term->cursor_left(t);
	c = 0;
	if (mode & K_EDIT) {
		SAFECOPY(str, strout);
		term->cursor_left(bputs(str));
	}
	while ((ch = getkey(mode)) != CR && online && !(sys_status & SS_ABORT)) {
		if (ch == TAB)  // TAB same as CR (implicitly K_TAB mode)
			break;
		if (ch == BS || ch == DEL) {
			while (c > 0) {
				c--;
				term->cursor_left();
				if (is_template_char(tmplt[c])) {
					str[c] = ' ';
					bputs(" \b");
					break;
				}
			}
			continue;
		}
		if (ch == CTRL_X) {
			for (; c; c--) {
				outchar(BS);
				if (is_template_char(tmplt[c - 1]))
					bputs(" \b");
			}
			str[0] = '\0';
		}
		else if (ch == TERM_KEY_LEFT) {
			while (c > 0) {
				c--;
				term->cursor_left();
				if(is_template_char(tmplt[c]))
					break;
			}
		}
		else if (ch == TERM_KEY_RIGHT) {
			while (c < t && str[c] != '\0') {
				c++;
				term->cursor_right();
				if(is_template_char(tmplt[c]))
					break;
			}
		}
		else if (c < t) {
			if (tmplt[c] == 'N' && !IS_DIGIT(ch))
				continue;
			if (tmplt[c] == 'A' && !IS_ALPHA(ch))
				continue;
			outchar(ch);
			str[c++] = ch;
			while (c < t && !is_template_char(tmplt[c])) {
				str[c] = tmplt[c];
				outchar(tmplt[c++]);
			}
		}
	}
	attr(LIGHTGRAY);
	term->newline();
	if (!(sys_status & SS_ABORT))
		strcpy(strout, str); // Not SAFECOPY()able
	return c;
}

/*****************************************************************************/
/* Accepts a user's input to change a new-scan time pointer                  */
/* Returns 0 if input was aborted or invalid, 1 if complete					 */
/*****************************************************************************/
bool sbbs_t::inputnstime32(time32_t *dt)
{
	bool   retval;
	time_t tmptime = *dt;

	retval = inputnstime(&tmptime);
	*dt = (time32_t)tmptime;
	return retval;
}

bool sbbs_t::inputnstime(time_t *dt)
{
	int       hour;
	struct tm tm;
	bool      pm = false;
	char      str[256];

	bputs(text[NScanDate]);
	bputs(timestr(*dt));
	term->newline();
	if (localtime_r(dt, &tm) == NULL) {
		errormsg(WHERE, ERR_CHK, "time ptr", 0);
		return FALSE;
	}

	bputs(text[NScanYear]);
	ultoa(tm.tm_year + 1900, str, 10);
	if (!getstr(str, 4, K_EDIT | K_AUTODEL | K_NUMBER | K_NOCRLF) || sys_status & SS_ABORT) {
		term->newline();
		return false;
	}
	tm.tm_year = atoi(str);
	if (tm.tm_year < 1970) {       /* unix time is seconds since 1/1/1970 */
		term->newline();
		return false;
	}
	tm.tm_year -= 1900;   /* tm_year is years since 1900 */

	bputs(text[NScanMonth]);
	ultoa(tm.tm_mon + 1, str, 10);
	if (!getstr(str, 2, K_EDIT | K_AUTODEL | K_NUMBER | K_NOCRLF) || sys_status & SS_ABORT) {
		term->newline();
		return false;
	}
	tm.tm_mon = atoi(str);
	if (tm.tm_mon < 1 || tm.tm_mon > 12) {
		term->newline();
		return false;
	}
	tm.tm_mon--;        /* tm_mon is zero-based */

	bputs(text[NScanDay]);
	ultoa(tm.tm_mday, str, 10);
	if (!getstr(str, 2, K_EDIT | K_AUTODEL | K_NUMBER | K_NOCRLF) || sys_status & SS_ABORT) {
		term->newline();
		return false;
	}
	tm.tm_mday = atoi(str);
	if (tm.tm_mday < 1 || tm.tm_mday > 31) {
		term->newline();
		return false;
	}
	bputs(text[NScanHour]);
	if (cfg.sys_misc & SM_MILITARY)
		hour = tm.tm_hour;
	else {
		if (tm.tm_hour == 0) { /* 12 midnite */
			pm = false;
			hour = 12;
		}
		else if (tm.tm_hour > 12) {
			hour = tm.tm_hour - 12;
			pm = true;
		}
		else {
			hour = tm.tm_hour;
			pm = false;
		}
	}
	ultoa(hour, str, 10);
	if (!getstr(str, 2, K_EDIT | K_AUTODEL | K_NUMBER | K_NOCRLF) || sys_status & SS_ABORT) {
		term->newline();
		return false;
	}
	tm.tm_hour = atoi(str);
	if (tm.tm_hour > 24) {
		term->newline();
		return false;
	}

	bputs(text[NScanMinute]);
	ultoa(tm.tm_min, str, 10);
	if (!getstr(str, 2, K_EDIT | K_AUTODEL | K_NUMBER | K_NOCRLF) || sys_status & SS_ABORT) {
		term->newline();
		return false;
	}

	tm.tm_min = atoi(str);
	if (tm.tm_min > 59) {
		term->newline();
		return false;
	}
	tm.tm_sec = 0;
	if (!(cfg.sys_misc & SM_MILITARY) && tm.tm_hour && tm.tm_hour < 13) {
		if (pm && yesno(text[NScanPmQ])) {
			if (tm.tm_hour < 12)
				tm.tm_hour += 12;
		}
		else if (!pm && !yesno(text[NScanAmQ])) {
			if (tm.tm_hour < 12)
				tm.tm_hour += 12;
		}
		else if (tm.tm_hour == 12)
			tm.tm_hour = 0;
	}
	else {
		term->newline();
	}
	tm.tm_isdst = -1; /* Do not adjust for DST */
	*dt = mktime(&tm);
	return true;
}

/****************************************************************************/
/* Check a password for validity and print reason upon failure				*/
/****************************************************************************/
bool sbbs_t::chkpass(char *passwd, user_t* user, bool unique)
{
	int reason = -1;
	if (!check_pass(&cfg, passwd, user, unique, &reason)) {
		if (reason >= 0)
			bputs(text[reason]);
		return false;
	}
	return !trashcan(passwd, "password");
}

/****************************************************************************/
/* Displays information about sub-board subnum								*/
/****************************************************************************/
void sbbs_t::subinfo(int subnum)
{
	char str[MAX_PATH + 1];

	if (!menu("subinfo", P_NOERROR)) {
		bputs(text[SubInfoHdr]);
		bprintf(text[SubInfoLongName], cfg.sub[subnum]->lname);
		bprintf(text[SubInfoShortName], cfg.sub[subnum]->sname);
		bprintf(text[SubInfoQWKName], cfg.sub[subnum]->qwkname);
		if (cfg.sub[subnum]->maxmsgs)
			bprintf(text[SubInfoMaxMsgs], cfg.sub[subnum]->maxmsgs);
		if (cfg.sub[subnum]->misc & SUB_QNET)
			bprintf(text[SubInfoTagLine], cfg.sub[subnum]->tagline);
		if (cfg.sub[subnum]->misc & SUB_FIDO)
			bprintf(text[SubInfoFidoNet]
					, cfg.sub[subnum]->origline
					, smb_faddrtoa(&cfg.sub[subnum]->faddr, str));
	}
	SAFEPRINTF2(str, "%s%s", cfg.sub[subnum]->data_dir, cfg.sub[subnum]->code);
	if (menu_exists(str) && yesno(text[SubInfoViewFileQ]))
		menu(str);
}

/****************************************************************************/
/* Displays information about transfer directory dirnum 					*/
/****************************************************************************/
void sbbs_t::dirinfo(int dirnum)
{
	char str[MAX_PATH + 1];

	if (!menu("dirinfo", P_NOERROR)) {
		bputs(text[DirInfoHdr]);
		bprintf(text[DirInfoLongName], cfg.dir[dirnum]->lname);
		bprintf(text[DirInfoShortName], cfg.dir[dirnum]->sname);
		if (cfg.dir[dirnum]->exts[0])
			bprintf(text[DirInfoAllowedExts], cfg.dir[dirnum]->exts);
		if (cfg.dir[dirnum]->maxfiles)
			bprintf(text[DirInfoMaxFiles], cfg.dir[dirnum]->maxfiles);
	}
	SAFEPRINTF2(str, "%s%s", cfg.dir[dirnum]->data_dir, cfg.dir[dirnum]->code);
	if (menu_exists(str) && yesno(text[DirInfoViewFileQ]))
		menu(str);
}

/****************************************************************************/
/* Searches the file <name>.can in the TEXT directory for matches			*/
/* Returns TRUE if found in list, FALSE if not.								*/
/****************************************************************************/
bool sbbs_t::trashcan(const char *insearchof, const char *name, struct trash* trash)
{
	if (!::trashcan2(&cfg, insearchof, NULL, name, trash))
		return false;
	if (trash != nullptr && trash->quiet)
		return true;
	trashcan_msg(name);
	return true;
}

/****************************************************************************/
/* Displays bad<name>.msg in text directory if found.						*/
/****************************************************************************/
void sbbs_t::trashcan_msg(const char* name)
{
	char str[MAX_PATH + 1];

	snprintf(str, sizeof str, "%sbad%s.msg", cfg.text_dir, name);
	if (cfg.mods_dir[0] != '\0') {
		char modpath[MAX_PATH + 1];
		snprintf(modpath, sizeof modpath, "%stext/bad%s.msg", cfg.mods_dir, name);
		if (fexistcase(modpath))
			SAFECOPY(str, modpath);
	}
	if (fexistcase(str)) {
		printfile(str, 0);
		flush_output(500); // give time for tx buffer to clear before disconnect
	}
}

char* sbbs_t::timestr(time_t intime)
{
	return ::timestr(&cfg, (time32_t)intime, timestr_output);
}

char* sbbs_t::datestr(time_t t, char* str)
{
	if (str == nullptr)
		str = datestr_output;
	return ::datestr(&cfg, t, str);
}

char* sbbs_t::unixtodstr(time_t t, char* str)
{
	if (str == nullptr)
		str = datestr_output;
	return ::unixtodstr(&cfg, (time32_t)t, str);
}

void sbbs_t::sys_info()
{
	if (!menu("sysinfo", P_NOERROR)) {
		char    tmp[128];
		int     i;

		bputs(text[SiHdr]);
		getstats_cached(&cfg, 0, &stats);
		bprintf(text[SiSysName], cfg.sys_name);
		bprintf(text[SiSysID], cfg.sys_id);  /* QWK ID */
		for (i = 0; i < cfg.total_faddrs; i++)
			bprintf(text[SiSysFaddr], smb_faddrtoa(&cfg.faddr[i], tmp));
		if (cfg.sys_location[0])
			bprintf(text[SiSysLocation], cfg.sys_location);
		bprintf(text[TiNow], timestr(now), smb_zonestr(sys_timezone(&cfg), NULL));
		if (cfg.sys_op[0])
			bprintf(text[SiSysop], cfg.sys_op);
		bprintf(text[SiSysNodes], cfg.sys_nodes);
		if (cfg.node_phone[0])
			bprintf(text[SiNodePhone], cfg.node_phone);
		bprintf(text[SiTotalLogons], ultoac(stats.logons, tmp));
		bprintf(text[SiLogonsToday], ultoac(stats.ltoday, tmp));
		bprintf(text[SiTotalTime], ultoac(stats.timeon, tmp));
		bprintf(text[SiTimeToday], ultoac(stats.ttoday, tmp));
		ver(P_80COLS);
		term->newline();
	}
	const char* fname = "../system";
	if (menu_exists(fname) && text[ViewSysInfoFileQ][0] && yesno(text[ViewSysInfoFileQ])) {
		cls();
		menu(fname);
	}
	fname = "logon";
	if (menu_exists(fname) && text[ViewLogonMsgQ][0] && yesno(text[ViewLogonMsgQ])) {
		cls();
		menu(fname);
	}
}

void sbbs_t::user_info()
{
	if (!menu("userinfo", P_NOERROR)) {
		float      f;
		char       str[128];
		char       tmp[128];
		char       tmp2[128];
		struct  tm tm;

		bprintf(text[UserStats], useron.alias, useron.number);

		if (localtime32(&useron.laston, &tm) != NULL)
			bprintf(text[UserDates]
					, datestr(useron.firston, str)
					, datestr(useron.expire, tmp)
					, datestr(useron.laston, tmp2)
					, tm.tm_hour, tm.tm_min);

		bprintf(text[UserTimes]
				, useron.timeon, useron.ttoday
				, cfg.level_timeperday[useron.level]
				, useron.tlast
				, cfg.level_timepercall[useron.level]
				, useron.textra);
		if (useron.posts)
			f = (float)useron.logons / useron.posts;
		else
			f = 0;
		bprintf(text[UserLogons]
				, useron.logons, useron.ltoday
				, cfg.level_callsperday[useron.level], useron.posts
				, f ? (uint)(100 / f) : useron.posts > useron.logons ? 100 : 0
				, useron.ptoday);
		bprintf(text[UserEmails]
				, useron.emails, useron.fbacks
				, mail_waiting.get(), useron.etoday);
		term->newline();
		bprintf(text[UserUploads]
				, byte_estimate_to_str(useron.ulb, tmp, sizeof(tmp), 1, 1), useron.uls);
		if (useron.dtoday)
			snprintf(str, sizeof str, text[UserDownloadsToday]
				, byte_estimate_to_str(useron.btoday, tmp, sizeof tmp, 1, 1)
				, useron.dtoday);
		else
			str[0] = '\0';
		bprintf(text[UserDownloads]
				, byte_estimate_to_str(useron.dlb, tmp, sizeof(tmp), 1, 1), useron.dls, str);
		bprintf(text[UserCredits], byte_estimate_to_str(useron.cdt, tmp, sizeof(tmp), 1, 1)
				, byte_estimate_to_str(useron.freecdt, tmp2, sizeof(tmp2), 1, 1)
				, byte_estimate_to_str(cfg.level_freecdtperday[useron.level], str, sizeof(str), 1, 1));
		bprintf(text[UserMinutes], ultoac(useron.min, tmp));
	}
}

void sbbs_t::xfer_policy()
{
	if (!usrlibs)
		return;
	if (!menu("tpolicy", P_NOERROR)) {
		bprintf(text[TransferPolicyHdr], cfg.sys_name);
		bprintf(text[TpUpload]
		        , cfg.dir[usrdir[curlib][curdir[curlib]]]->up_pct);
		bprintf(text[TpDownload]
		        , cfg.dir[usrdir[curlib][curdir[curlib]]]->dn_pct);
	}
}

const char* prot_menu_file[] = {
	"ulprot"
	, "dlprot"
	, "batuprot"
	, "batdprot"
	, "biprot"
};

char* sbbs_t::xfer_prot_menu(enum XFER_TYPE type, user_t* user, char* keys, size_t size)
{
	if (type == XFER_DOWNLOAD && current_file != nullptr)
		menu("download", P_NOERROR);
	size_t count = 0;
	bool   menu_used = menu(prot_menu_file[type], P_NOERROR);
	if (user == nullptr)
		user = &useron;
	term->cond_blankline();
	int    printed = 0;
	for (int i = 0; i < cfg.total_prots; i++) {
		if (!chk_ar(cfg.prot[i]->ar, user, &client))
			continue;
		if (type == XFER_UPLOAD && cfg.prot[i]->ulcmd[0] == 0)
			continue;
		if (type == XFER_DOWNLOAD && cfg.prot[i]->dlcmd[0] == 0)
			continue;
		if (type == XFER_BATCH_UPLOAD && cfg.prot[i]->batulcmd[0] == 0)
			continue;
		if (type == XFER_BATCH_DOWNLOAD && cfg.prot[i]->batdlcmd[0] == 0)
			continue;
		if (keys != nullptr && count + 1 < size)
			keys[count++] = cfg.prot[i]->mnemonic;
		if (menu_used)
			continue;
		if (printed && (term->cols < 80 || (printed % 2) == 0))
			term->newline();
		bprintf(text[TransferProtLstFmt], cfg.prot[i]->mnemonic, cfg.prot[i]->name);
		printed++;
	}
	if (keys != nullptr)
		keys[count] = '\0';
	if (!menu_used)
		term->newline();
	return keys;
}

void sbbs_t::node_stats(uint node_num)
{
	char    tmp[128];
	stats_t stats;

	if (node_num > cfg.sys_nodes) {
		bputs(text[InvalidNode]);
		return;
	}
	if (!node_num)
		node_num = cfg.node_num;
	bprintf(text[NodeStatsHdr], node_num);
	getstats(&cfg, node_num, &stats); // Cannot use cached stats for specific node
	bprintf(text[StatsTotalLogons], ultoac(stats.logons, tmp));
	bprintf(text[StatsLogonsToday], ultoac(stats.ltoday, tmp));
	bprintf(text[StatsTotalTime], ultoac(stats.timeon, tmp));
	bprintf(text[StatsTimeToday], ultoac(stats.ttoday, tmp));
	bprintf(text[StatsUploadsToday], u64toac(stats.ulb, tmp)
	        , stats.uls);
	bprintf(text[StatsDownloadsToday], u64toac(stats.dlb, tmp)
	        , stats.dls);
	bprintf(text[StatsPostsToday], ultoac(stats.ptoday, tmp));
	bprintf(text[StatsEmailsToday], ultoac(stats.etoday, tmp));
	bprintf(text[StatsFeedbacksToday], ultoac(stats.ftoday, tmp));
}

void sbbs_t::sys_stats(void)
{
	char    tmp[128];

	bputs(text[SystemStatsHdr]);
	getstats_cached(&cfg, 0, &stats);
	bprintf(text[StatsTotalLogons], ultoac(stats.logons, tmp));
	bprintf(text[StatsLogonsToday], ultoac(stats.ltoday, tmp));
	bprintf(text[StatsTotalTime], ultoac(stats.timeon, tmp));
	bprintf(text[StatsTimeToday], ultoac(stats.ttoday, tmp));
	bprintf(text[StatsUploadsToday], u64toac(stats.ulb, tmp)
	        , stats.uls);
	bprintf(text[StatsDownloadsToday], u64toac(stats.dlb, tmp)
	        , stats.dls);
	bprintf(text[StatsPostsToday], ultoac(stats.ptoday, tmp));
	bprintf(text[StatsEmailsToday], ultoac(stats.etoday, tmp));
	bprintf(text[StatsFeedbacksToday], ultoac(stats.ftoday, tmp));
}

void sbbs_t::logonlist(const char* args)
{
	char str[MAX_PATH + 1];

	bool invoked;
	exec_mod("list logons", cfg.logonlist_mod, &invoked, "%s", args);
	if (invoked)
		return;
	SAFEPRINTF(str, "%slogon.lst", cfg.data_dir);
	if (flength(str) < 1) {
		bputs("\r\n\r\n");
		bputs(text[NoOneHasLoggedOnToday]);
	} else {
		bputs(text[CallersToday]);
		printfile(str, P_NOATCODES | P_OPENCLOSE | P_TRUNCATE);
		term->newline();
	}
}

extern SOCKET   spy_socket[];
extern RingBuf* node_inbuf[];

bool sbbs_t::spy(uint i /* node_num */)
{
	char   ch;
	char   ansi_seq[256];
	size_t ansi_len;
	int    in;

	if (!i || i > MAX_NODES) {
		bprintf("Invalid node number: %d\r\n", i);
		return false;
	}
	if (i == cfg.node_num) {
		bprintf("Can't spy on yourself.\r\n");
		return false;
	}
	if (spy_socket[i - 1] != INVALID_SOCKET) {
		bprintf("Node %d already being spied (%x)\r\n", i, spy_socket[i - 1]);
		return false;
	}
	bprintf("*** Synchronet Remote Spy on Node %d: Ctrl-C to Abort ***"
	        "\r\n\r\n", i);
	if (passthru_thread_running)
		spy_socket[i - 1] = client_socket_dup;
	else
		spy_socket[i - 1] = client_socket;
	ansi_len = 0;
	while (online
	       && client_socket != INVALID_SOCKET
	       && spy_socket[i - 1] != INVALID_SOCKET
	       && !msgabort()) {
		in = incom(1000);
		if (in == NOINP) {
			gettimeleft();
			continue;
		}
		ch = in;
		if (ch == ESC) {
			if (ansi_len)
				ansi_len = 0;
			else {
				if ((in = incom(500)) != NOINP) {
					if (in == '[') {
						ansi_seq[ansi_len++] = ESC;
						ansi_seq[ansi_len++] = '[';
						continue;
					} else {
						if (node_inbuf[i - 1] != NULL) {
							RingBufWrite(node_inbuf[i - 1], (uchar*)&ch, sizeof(ch));
							ch = in;
						}
					}
				}
			}
		}
		if (ansi_len) {
			if (ansi_len < sizeof(ansi_seq))
				ansi_seq[ansi_len++] = ch;
			if (ch >= '@' && ch <= '~') {
				switch (ch) {
					case 'A':   // Up
					case 'B':   // Down
					case 'C':   // Right
					case 'D':   // Left
					case 'F':   // Preceding line
					case 'H':   // Home
					case 'K':   // End
					case 'V':   // PageUp
					case 'U':   // PageDn
					case '@':   // Insert
					case '~':   // Various VT-220
						// Pass-through these sequences to spied-upon node (eat all others)
						if (node_inbuf[i - 1] != NULL)
							RingBufWrite(node_inbuf[i - 1], (uchar*)ansi_seq, ansi_len);
						break;
				}
				ansi_len = 0;
			}
			continue;
		}
		if (ch < ' ') {
			term->lncntr = 0;                       /* defeat pause */
			spy_socket[i - 1] = INVALID_SOCKET; /* disable spy output */
			ch = handle_ctrlkey(ch, K_NONE);
			spy_socket[i - 1] = passthru_thread_running ? client_socket_dup : client_socket;  /* enable spy output */
			if (ch == 0)
				continue;
		}
		if (node_inbuf[i - 1] != NULL)
			RingBufWrite(node_inbuf[i - 1], (uchar*)&ch, 1);
	}
	spy_socket[i - 1] = INVALID_SOCKET;

	return true;
}

void sbbs_t::time_bank(void)
{
	char str[128];
	char tmp[128];
	char tmp2[128];
	int  s;

	if (cfg.sys_misc & SM_TIMEBANK) {  /* Allow users to deposit free time */
		s = (cfg.level_timeperday[useron.level] - useron.ttoday) + useron.textra;
		if (s < 0)
			s = 0;
		if (s > cfg.level_timepercall[useron.level])
			s = cfg.level_timepercall[useron.level];
		s -= (int)(now - starttime) / 60;
		if (s < 0)
			s = 0;
		bprintf(text[FreeMinLeft], s);
		bprintf(text[UserMinutes], ultoac(useron.min, tmp));
		if (cfg.max_minutes && useron.min >= cfg.max_minutes) {
			bputs(text[YouHaveTooManyMinutes]);
			return;
		}
		if (cfg.max_minutes)
			while (s > 0 && s + useron.min > cfg.max_minutes) s--;
		bprintf(text[FreeMinToDeposit], s);
		s = getnum(s);
		if (s > 0) {
			logline("  ", "Minute Bank Deposit");
			useron.min = (uint32_t)adjustuserval(&cfg, &useron, USER_MIN, s);
			useron.ttoday = (uint)adjustuserval(&cfg, &useron, USER_TTODAY, s);
			snprintf(str, sizeof str, "Minute Adjustment: %u", s * cfg.cdt_min_value);
			logline("*+", str);
		}
	}

	if (!(cfg.sys_misc & SM_NOCDTCVT)) {
		bprintf(text[ConversionRate], cfg.cdt_min_value);
		bprintf(text[UserCredits]
		        , u64toac(useron.cdt, tmp)
		        , u64toac(useron.freecdt, tmp2)
		        , u64toac(cfg.level_freecdtperday[useron.level], str));
		bprintf(text[UserMinutes], ultoac(useron.min, tmp));
		if (useron.cdt / 102400L < 1L) {
			bprintf(text[YouOnlyHaveNCredits], u64toac(useron.cdt, tmp));
			return;
		}
		if (cfg.max_minutes && useron.min >= cfg.max_minutes) {
			bputs(text[YouHaveTooManyMinutes]);
			return;
		}
		s = (uint32_t)(useron.cdt / 102400L);
		if (cfg.max_minutes)
			while (s > 0 && (s * cfg.cdt_min_value) + useron.min > cfg.max_minutes) s--;
		bprintf(text[CreditsToMin], s);
		s = getnum(s);
		if (s > 0) {
			logline("  ", "Credit to Minute Conversion");
			useron.cdt = adjustuserval(&cfg, &useron, USER_CDT, -(s * 102400L));
			useron.min = (uint32_t)adjustuserval(&cfg, &useron, USER_MIN, s * (int)cfg.cdt_min_value);
			snprintf(str, sizeof str, "Credit Adjustment: %ld", -(s * 102400L));
			logline("$-", str);
			snprintf(str, sizeof str, "Minute Adjustment: %u", s * cfg.cdt_min_value);
			logline("*+", str);
		}
	}
}

bool sbbs_t::change_user(const char* username)
{
	uint i;
	char str[256];
	char passwd[LEN_PASS + 1];

	if (!chksyspass())
		return false;
	if (username == nullptr || *username == '\0') {
		bputs(text[ChUserPrompt]);
		if (!getstr(str, LEN_ALIAS, K_UPPER))
			return false;
		username = str;
	}
	if ((i = finduser(username)) == 0)
		return false;
	if (getuserdec32(&cfg, i, USER_LEVEL) > logon_ml) {
		if (getuserstr(&cfg, i, USER_PASS, passwd, sizeof passwd) != NULL) {
			bputs(text[ChUserPwPrompt]);
			console |= CON_PASSWORD;
			getstr(str, sizeof str - 1, K_UPPER);
			console &= ~CON_PASSWORD;
			if (strcmp(str, passwd) != 0)
				return false;
		}
	}
	putmsgptrs();
	putuserstr(useron.number, USER_CURSUB
	           , cfg.sub[usrsub[curgrp][cursub[curgrp]]]->code);
	putuserstr(useron.number, USER_CURDIR
	           , cfg.dir[usrdir[curlib][curdir[curlib]]]->code);
	user_t org_user = useron;
	useron.number = i;
	if (getuserdat(&cfg, &useron) != USER_SUCCESS) {
		useron = org_user;
		return false;
	}
	if (getnodedat(cfg.node_num, &thisnode, true)) {
		thisnode.useron = useron.number;
		putnodedat(cfg.node_num, &thisnode);
	}
	getmsgptrs();
	if (user_is_sysop(&useron))
		sys_status &= ~SS_TMPSYSOP;
	else
		sys_status |= SS_TMPSYSOP;
	snprintf(str, sizeof str, "Changed into %s #%u", useron.alias, useron.number);
	logline("S+", str);
	return true;
}

/* 't' value must be adjusted for timezone offset */
char* sbbs_t::age_of_posted_item(char* buf, size_t max, time_t t)
{
	time_t now = time(NULL) - (xpTimeZone_local() * 60);
	char*  past = text[InThePast];
	char*  units = text[Years];
	char   value[128];

	double diff = difftime(now, t);
	if (diff < 0) {
		past = text[InTheFuture];
		diff = -diff;
	}

	if (diff < 60) {
		snprintf(value, sizeof value, "%.0f", diff);
		units = text[Seconds];
	} else if (diff < 60 * 60) {
		snprintf(value, sizeof value, "%.0f", diff / 60.0);
		units = text[Minutes];
	} else if (diff < 60 * 60 * 24) {
		snprintf(value, sizeof value, "%.1f", diff / (60.0 * 60.0));
		units = text[Hours];
	} else if (diff < 60 * 60 * 24 * 30) {
		snprintf(value, sizeof value, "%.1f", diff / (60.0 * 60.0 * 24.0));
		units = text[Days];
	} else if (diff < 60 * 60 * 24 * 365) {
		snprintf(value, sizeof value, "%.1f", diff / (60.0 * 60.0 * 24.0 * 30.0));
		units = text[Months];
	} else
		snprintf(value, sizeof value, "%.1f", diff / (60.0 * 60.0 * 24.0 * 365.25));
	remove_end_substr(value, ".0");
	safe_snprintf(buf, max, text[AgeOfPostedItem], value, units, past);
	return buf;
}

char* sbbs_t::server_host_name(void)
{
	return startup->host_name[0] ? startup->host_name : cfg.sys_inetaddr;
}

char* sbbs_t::ultoac(uint32_t val, char* str, char sep)
{
	return ::u32toac(val, str, sep);
}

char* sbbs_t::u64toac(uint64_t val, char* str, char sep)
{
	return ::u64toac(val, str, sep);
}

int sbbs_t::protnum(char prot, enum XFER_TYPE type)
{
	int i;

	for (i = 0; i < cfg.total_prots; ++i) {
		if (prot != cfg.prot[i]->mnemonic || !chk_ar(cfg.prot[i]->ar, &useron, &client))
			continue;
		switch (type) {
			case XFER_UPLOAD:
				if (cfg.prot[i]->ulcmd[0])
					return i;
				break;
			case XFER_DOWNLOAD:
				if (cfg.prot[i]->dlcmd[0])
					return i;
				break;
			case XFER_BATCH_UPLOAD:
				if (cfg.prot[i]->batulcmd[0])
					return i;
				break;
			case XFER_BATCH_DOWNLOAD:
				if (cfg.prot[i]->batdlcmd[0])
					return i;
				break;
		}
	}
	return i;
}

const char* sbbs_t::protname(char prot, enum XFER_TYPE type)
{
	int i = protnum(prot, type);
	if (i < cfg.total_prots)
		return cfg.prot[i]->name;
	return text[None];
}
