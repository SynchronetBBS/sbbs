/* Synchronet string input routines */

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
#include "utf8.h"

/****************************************************************************/
/* Returns the display width (in terminal columns) of the first 'len' bytes */
/* of 'str'. Every single-byte char counts as one column. When 'utf8' is    */
/* true, each multi-byte UTF-8 sequence counts as the width of its codepoint*/
/* (0, 1 or 2 columns), so byte-offsets can be converted to cursor columns. */
/****************************************************************************/
static size_t str_width(const char* str, size_t len, bool utf8, size_t zerowidth)
{
	if (!utf8)
		return len;
	size_t width = 0;
	for (size_t i = 0; i < len;) {
		if (!(str[i] & 0x80)) {
			++width;
			++i;
			continue;
		}
		enum unicode_codepoint codepoint = UNICODE_UNDEFINED;
		int                    clen = utf8_getc(str + i, len - i, &codepoint);
		if (clen < 1) { // Invalid or incomplete sequence
			++width;
			++i;
			continue;
		}
		width += unicode_width(codepoint, zerowidth);
		i += clen;
	}
	return width;
}

/****************************************************************************/
/* Returns the byte-offset of the beginning of the char preceding offset 'i'*/
/****************************************************************************/
static size_t prev_char(const char* str, size_t i, bool utf8)
{
	if (i == 0)
		return 0;
	--i;
	if (utf8) {
		while (i > 0 && (str[i] & 0xC0) == 0x80) // UTF-8 continuation byte
			--i;
	}
	return i;
}

/****************************************************************************/
/* Returns the byte-offset of the char following the char at offset 'i'    */
/****************************************************************************/
static size_t next_char(const char* str, size_t i, size_t len, bool utf8)
{
	if (i >= len)
		return len;
	++i;
	if (utf8) {
		while (i < len && (str[i] & 0xC0) == 0x80) // UTF-8 continuation byte
			++i;
	}
	return i;
}

/****************************************************************************/
/* Redraws a string (strin) being edited: 'l' is its length in bytes and    */
/* 'i' is the byte-offset of the cursor position within the string.         */
/* 'mode' is a bit-field of P_* (putmsg/bputs) flags.                       */
/****************************************************************************/
void sbbs_t::redrwstr(char *strin, int i, int l, int mode)
{
	if (i < 0)
		i = 0;
	if (l < 0)
		l = 0;
	if (i > l)
		i = l;
	auto_utf8(strin, mode);
	bool   utf8 = (mode & P_UTF8) != 0;
	size_t icol = str_width(strin, i, utf8, unicode_zerowidth);
	if (icol > 0)
		term->cursor_left(icol);
	if (mode)
		bprintf(mode, "%-*.*s", l, l, strin);
	else
		rprintf("%-*.*s", l, l, strin);
	term->cleartoeol();
	if (i < l)
		term->cursor_left(str_width(strin + i, l - i, utf8, unicode_zerowidth));
}

/****************************************************************************/
/* Waits for remote or local user to input a CR terminated string. 'length' */
/* is the maximum number of characters that getstr will allow the user to   */
/* input into the string. 'mode' specifies upper case characters are echoed */
/* or wordwrap or if in message input (^A sequences allowed). ^W backspaces */
/* a word, ^X backspaces a line, ^Gs, BSs, TABs are processed, LFs ignored. */
/* ^N non-destructive BS, ^V center line. Valid keys are echoed.            */
/* With K_UTF8 (and a UTF-8 terminal), multi-byte UTF-8 sequences are       */
/* accepted (not translated to CP437) and 'length' is a limit in bytes.     */
/****************************************************************************/
size_t sbbs_t::getstr(char *strout, size_t maxlen, int mode, const str_list_t history)
{
	size_t i, l, x, z;    /* i=current position, l=length (both byte-offsets) */
	/* x&z=misc */
	char   str1[256], str2[256], undo[256];
	uchar  ch;
	uint   atr;
	int    hidx = -1;
	int    org_lbuflen = term->lbuflen;
	bool   utf8 = (mode & K_UTF8) && term->charset() == CHARSET_UTF8; /* str1 may contain multi-byte chars */
	int    rmode = utf8 ? P_UTF8 : 0;    /* redraw (bputs/redrwstr) mode */
	size_t maxcols = SIZE_MAX;           /* display width limit (columns) */

	// Display width (in columns) of the bytes of str1 between byte-offsets 'from' and 'to'
	auto   width = [&](size_t from, size_t to) -> size_t {
		return str_width(str1 + from, to - from, utf8, unicode_zerowidth);
	};
	// Prints 'len' bytes of str1 beginning at byte-offset 'from'
	auto   outbytes = [&](size_t from, size_t len) {
		if (utf8) {
			char tmp[sizeof(str1)];
			memcpy(tmp, str1 + from, len);
			tmp[len] = 0;
			bputs(tmp, P_UTF8);
		} else {
			for (size_t n = 0; n < len; ++n)
				outchar(str1[from + n]);
		}
	};
	// Removes the bytes of str1 between byte-offsets 'from' and 'to', redrawing the remainder of the line
	// The cursor must be at the display column of 'from' (which becomes the current position)
	auto   delete_range = [&](size_t from, size_t to) {
		size_t wdel = width(from, to);
		memmove(str1 + from, str1 + to, l - to);
		l -= to - from;
		i = from;
		if (!(mode & K_NOECHO)) {
			outbytes(i, l - i);
			for (size_t n = 0; n < wdel; ++n) /* write over the vacated columns */
				outchar(' ');
			term->cursor_left(width(i, l) + wdel);
		}
	};
	// Inserts 'n' bytes (seq) into str1 at the current position (i), or over-writes the char there
	// (when 'may_overwrite' is true and not in insert mode). Returns false if there's no room.
	auto   insert_bytes = [&](const char* seq, size_t n, bool may_overwrite) -> bool {
		size_t old = 0; /* bytes over-written */
		if (may_overwrite && !(console & CON_INSERT) && i < l)
			old = next_char(str1, i, l, utf8) - i;
		size_t newcols = str_width(seq, n, utf8, unicode_zerowidth);
		size_t oldcols = width(i, i + old);
		/* Drop chars from the end of the line to make room, if necessary */
		while (l > i + old && (l - old + n > maxlen || width(0, l) - oldcols + newcols > maxcols))
			l = prev_char(str1, l, utf8);
		if (l - old + n > maxlen || width(0, l) - oldcols + newcols > maxcols)
			return false;
		memmove(str1 + i + n, str1 + i + old, l - (i + old));
		memcpy(str1 + i, seq, n);
		l += n - old;
		if (!(mode & K_NOECHO)) {
			outbytes(i, n);
			if (old != n || oldcols != newcols) { /* redraw the remainder of the line */
				outbytes(i + n, l - (i + n));
				size_t pad = oldcols > newcols ? oldcols - newcols : 0;
				for (size_t p = 0; p < pad; ++p)
					outchar(' ');
				term->cursor_left(width(i + n, l) + pad);
			}
		}
		i += n;
		return true;
	};

	console &= ~(CON_UPARROW | CON_DOWNARROW | CON_LEFTARROW | CON_RIGHTARROW | CON_BACKSPACE | CON_DELETELINE);
	if (!(mode & K_WORDWRAP))
		console &= ~CON_INSERT;
	clearabort();
	if (maxlen >= sizeof(str1))
		maxlen = sizeof(str1) - 1;
	if (!(mode & K_LINEWRAP) && term->cols >= TERM_COLS_MIN && !(mode & K_NOECHO) && !(console & CON_PASSWORD)
	    && term->column + (int)maxlen >= term->cols) {  /* Don't allow the terminal to auto line-wrap */
		maxcols = term->cols - term->column - 1;
		if (!utf8) /* one byte per column */
			maxlen = maxcols;
	}
	if (mode & K_LINE && (term->can_highlight()) && !(mode & K_NOECHO)) {
		attr(cfg.color[clr_inputline]);
		x = maxlen < maxcols ? maxlen : maxcols;
		for (i = 0; i < x; i++)
			term_out(' ');
		term->cursor_left(x);
	}
	if (wordwrap[0]) {
		SAFECOPY(str1, wordwrap);
		wordwrap[0] = 0;
	}
	else
		str1[0] = 0;
	if (mode & K_EDIT)
		SAFECAT(str1, strout);
	else
		strout[0] = 0;
	l = strlen(str1);
	if (l > maxlen) {
		l = maxlen;
		while (l > 0 && utf8 && (str1[l] & 0xC0) == 0x80) /* don't leave a partial UTF-8 sequence */
			--l;
		str1[l] = 0;
	}
	while (l > 0 && utf8 && width(0, l) > maxcols)
		l = prev_char(str1, l, utf8);
	str1[l] = 0;
	atr = curatr;
	if (!(mode & K_NOECHO)) {
		if (mode & K_AUTODEL && str1[0]) {
			i = (cfg.color[clr_inputline] & 0x07) << 4;
			i |= (cfg.color[clr_inputline] & 0x70) >> 4;
			attr(i);
		}
		bputs(str1, P_AUTO_UTF8 | rmode);
		if (mode & K_EDIT && !(mode & (K_LINE | K_AUTODEL)))
			term->cleartoeol();  /* destroy to eol */
	}

	SAFECOPY(undo, str1);
	i = l;
	if (mode & K_AUTODEL && str1[0] && !(mode & K_NOECHO)) {
		ch = getkey(mode | K_GETSTR);
		attr(atr);
		if (IS_PRINTABLE(ch) || ch == DEL || ch == BS) {
			term->backspace(width(0, l));
			i = l = 0;
		}
		else {
			for (x = width(0, l); x > 0; x--)
				outchar(BS);
			bputs(str1, P_AUTO_UTF8 | rmode);
			i = l;
		}
		if (ch != ' ' && ch != TAB)
			ungetkey(ch);
	}

	if (mode & K_USEOFFSET) {
		i = getstr_offset;
		if (i > l)
			i = l;
		while (i > 0 && utf8 && (str1[i] & 0xC0) == 0x80) /* don't land in the middle of a UTF-8 sequence */
			--i;
		if (l - i) {
			term->cursor_left(width(i, l));
		}
	}

	if (console & CON_INSERT && !(mode & K_NOECHO))
		term->insert_indicator();

	while (!(sys_status & SS_ABORT) && online && input_thread_running) {
		if (mode & K_LEFTEXIT
		    && console & (CON_LEFTARROW | CON_BACKSPACE | CON_DELETELINE))
			break;
		if (console & CON_UPARROW)
			break;
		if ((ch = getkey(mode | K_GETSTR)) == CR)
			break;
		if (sys_status & SS_ABORT || !online)
			break;
		if (ch == TERM_KEY_DOWN && (mode & K_MSG) && history == nullptr) { // Down-arrow same as CR
			console |= CON_DOWNARROW;
			break;
		}
		if (ch == TERM_KEY_RIGHT && (mode & K_RIGHTEXIT) && i == l) {
			console |= CON_RIGHTARROW;
			break;
		}
		if (ch == TAB && (mode & K_TAB || (!(mode & K_WORDWRAP) && history == NULL)))  /* TAB same as CR */
			break;
		if (!i && (mode & (K_UPRLWR | K_TRIM)) && (ch == ' ' || ch == TAB))
			continue;   /* ignore beginning white space if upper/lower */
		if (mode & K_E71DETECT && (uchar)ch == (CR | 0x80) && l > 1) {
			if (strstr(str1, "\x8d\x8d")) {
				bputs("\r\n\r\nYou must set your terminal to NO PARITY, "
				      "8 DATA BITS, and 1 STOP BIT (N-8-1).\7\r\n");
				return 0;
			}
		}
		getstr_offset = i;
		switch (ch) {
			case CTRL_A: /* Ctrl-A for ANSI */
				if (!(mode & K_MSG) || useron.rest & UREST_ANSI || i > maxlen - 3)
					break;
				insert_bytes("\x01", 1, /* may_overwrite: */ true);
				break;
			case TERM_KEY_HOME: /* Ctrl-B Beginning of Line */
				if (i && !(mode & K_NOECHO)) {
					term->cursor_left(width(0, i));
					i = 0;
				}
				break;
			case CTRL_D: /* Ctrl-D Delete word right */
				if (i < l) {
					x = i;
					while (x < l && str1[x] != ' ')
						x++;
					while (x < l && str1[x] == ' ')
						x++;
					delete_range(i, x);
				}
				break;
			case TERM_KEY_END: /* Ctrl-E End of line */
				if (term->can_move() && i < l) {
					term->cursor_right(width(i, l));  /* move cursor to eol */
					i = l;
				}
				break;
			case TERM_KEY_RIGHT: /* Ctrl-F move cursor forward */
				if (i < l && term->can_move()) {
					x = next_char(str1, i, l, utf8);
					term->cursor_right(width(i, x));   /* move cursor right one */
					i = x;
				}
				break;
			case CTRL_G: /* Bell */
				if (!(mode & K_MSG))
					break;
				if (useron.rest & UREST_BEEP) {
					if (i + 6 < maxlen)
						insert_bytes("(beep)", 6, /* may_overwrite: */ false);
					break;
				}
				insert_bytes("\a", 1, /* may_overwrite: */ true);
				break;
			case CTRL_H:    /* Ctrl-H/Backspace */
				if (i == 0) {
					console |= CON_BACKSPACE;
					break;
				}
				x = prev_char(str1, i, utf8);
				if (!(mode & K_NOECHO))
					term->cursor_left(width(x, i));
				delete_range(x, i);
				break;
			case CTRL_I:    /* Ctrl-I/TAB */
				if (history != NULL) {
					if (l < 1)
						break;
					int hi;
					for (hi = 0; history[hi] != NULL; hi++)
						if (strnicmp(history[hi], str1, l) == 0) {
							hidx = hi;
							SAFECOPY(str1, history[hi]);
							term->backspace(width(0, i));
							i = l = strlen(str1);
							outbytes(0, l);
							term->cleartoeol();
							break;
						}
					break;
				}
				if (!(width(0, i) % EDIT_TABSIZE))
					insert_bytes(" ", 1, /* may_overwrite: */ true);
				while (i < maxlen && width(0, i) % EDIT_TABSIZE) {
					if (!insert_bytes(" ", 1, /* may_overwrite: */ true))
						break;
				}
				break;

			case CTRL_L:    /* Ctrl-L   Center line (used to be Ctrl-V) */
				str1[l] = 0;
				z = term->bstrlen(str1, rmode);  /* display width */
				if (!z)
					break;
				x = maxlen < maxcols ? maxlen : maxcols; /* line width */
				x = z < x ? (x - z) / 2 : 0;             /* leading spaces */
				if (x + l > maxlen)
					x = maxlen - l;
				memset(str2, ' ', x);
				str2[x] = 0;
				SAFECAT(str2, str1);
				strcpy(strout, str2);
				l = strlen(strout);
				if (mode & K_NOECHO)
					return l;
				if (mode & K_MSG)
					redrwstr(strout, i, l, K_MSG | rmode);
				else {
					for (x = width(0, i); x > 0; x--)
						bputs("\b");
					bputs(strout, rmode);
					if (mode & K_LINE)
						attr(LIGHTGRAY);
				}
				if (!(mode & K_NOCRLF))
					term->newline();
				return l;

			case CTRL_N:    /* Ctrl-N Next word */
				if (i < l && term->can_move()) {
					x = i;
					while (str1[i] != ' ' && i < l)
						i++;
					while (str1[i] == ' ' && i < l)
						i++;
					term->cursor_right(width(x, i));
				}
				break;
			case CTRL_R:    /* Ctrl-R Redraw Line */
				if (!(mode & K_NOECHO))
					redrwstr(str1, i, l, P_AUTO_UTF8 | rmode);
				break;
			case TERM_KEY_INSERT:   /* Ctrl-V			Toggles Insert/Overwrite */
				if (mode & K_NOECHO)
					break;
				console ^= CON_INSERT;
				term->insert_indicator();
				break;
			case CTRL_W:    /* Ctrl-W   Delete word left */
				x = i;                            /* x=original offset */
				while (i && str1[i - 1] == ' ')
					i--;
				while (i && str1[i - 1] != ' ')
					i--;
				if (!(mode & K_NOECHO))
					term->cursor_left(width(i, x));
				delete_range(i, x);
				break;
			case CTRL_Y:    /* Ctrl-Y   Delete to end of line */
				if (i != l) {  /* if not at EOL */
					if (!(mode & K_NOECHO))
						term->cleartoeol();
					l = i;
					break;
				}
			/* fall-through */
			case CTRL_X:    /* Ctrl-X   Delete entire line */
				if (mode & K_NOECHO)
					l = 0;
				else {
					term->cursor_left(width(0, i));
					term->cleartoeol();
					l = 0;
					term->lbuflen = org_lbuflen;
				}
				i = 0;
				console |= CON_DELETELINE;
				break;
			case CTRL_Z:    /* Undo */
				if (!(mode & K_NOECHO))
					term->backspace(width(0, i));
				SAFECOPY(str1, undo);
				i = l = strlen(str1);
				outbytes(0, l);
				term->cleartoeol();
				break;
			case CTRL_BACKSLASH:    /* Ctrl-\ Previous word */
				if (i && !(mode & K_NOECHO)) {
					x = i;
					while (i && str1[i - 1] == ' ')
						i--;
					while (i && str1[i - 1] != ' ')
						i--;
					term->cursor_left(width(i, x));
				}
				break;
			case TERM_KEY_LEFT:  /* Ctrl-]/Left Arrow  Reverse Cursor Movement */
				if (i == 0) {
					if (mode & K_LEFTEXIT)
						console |= CON_LEFTARROW;
					break;
				}
				if (!(mode & K_NOECHO)) {
					x = prev_char(str1, i, utf8);
					term->cursor_left(width(x, i));   /* move cursor left one */
					i = x;
				}
				break;
			case TERM_KEY_DOWN:
				if (history != NULL) {
					if (hidx < 0) {
						outchar(BEL);
						break;
					}
					hidx--;
					if (hidx < 0)
						SAFECOPY(str1, undo);
					else
						SAFECOPY(str1, history[hidx]);
					term->backspace(width(0, i));
					i = l = strlen(str1);
					outbytes(0, l);
					term->cleartoeol();
					break;
				}
				break;
			case TERM_KEY_UP:  /* Ctrl-^/Up Arrow */
				if (history != NULL) {
					if (history[hidx + 1] == NULL) {
						outchar(BEL);
						break;
					}
					hidx++;
					term->backspace(width(0, i));
					SAFECOPY(str1, history[hidx]);
					i = l = strlen(str1);
					outbytes(0, l);
					term->cleartoeol();
					break;
				}
				if (!(mode & K_EDIT))
					break;
				if (i > l)
					l = i;
				str1[l] = 0;
				strcpy(strout, str1);
				if ((strip_invalid_attr(strout) || console & CON_INSERT) && !(mode & K_NOECHO))
					redrwstr(strout, i, l, K_MSG | rmode);
				if (mode & K_LINE && !(mode & K_NOECHO))
					attr(LIGHTGRAY);
				console |= CON_UPARROW;
				return l;
			case TERM_KEY_DELETE:  /* Ctrl-BkSpc (DEL) Delete current char */
				if (i == l) {  /* Backspace if end of line */
					if (i) {
						x = prev_char(str1, i, utf8);
						if (!(mode & K_NOECHO))
							term->cursor_left(width(x, i));
						delete_range(x, i);
					}
					break;
				}
				delete_range(i, next_char(str1, i, l, utf8));
				break;
			default: {
				char   seq[UTF8_MAX_LEN + 1]; /* the char (byte sequence) input */
				size_t seqlen = 1;
				seq[0] = ch;
				if (utf8 && (ch & 0x80)) { /* Lead byte of a multi-byte UTF-8 sequence: read the rest */
					int len = utf8_decode_firstbyte(ch);
					if (len < 2 || len > UTF8_MAX_LEN) /* Not a valid lead byte: ignore */
						break;
					while (seqlen < (size_t)len) {
						uchar c = getkey(mode | K_GETSTR);
						if ((c & 0xC0) != 0x80) { /* Not a continuation byte: discard the partial sequence */
							if (c != 0)
								ungetkey(c);
							seqlen = 0;
							break;
						}
						seq[seqlen++] = c;
					}
					if (seqlen < 2 || utf8_getc(seq, seqlen, NULL) < 1)
						break;
				}
				if (mode & K_WORDWRAP && i == l && ch >= ' ' && !(console & CON_INSERT)
				    && (l + seqlen > maxlen || width(0, l) + str_width(seq, seqlen, utf8, unicode_zerowidth) > maxcols)) {
					str1[i] = 0;
					if (ch == ' ' && !(mode & K_CHAT)) { /* don't wrap a space */
						strcpy(strout, str1);       /* as last char */
						if (strip_invalid_attr(strout) && !(mode & K_NOECHO))
							redrwstr(strout, i, l, K_MSG | rmode);
						if (!(mode & (K_NOECHO | K_NOCRLF)))
							term->newline();
						return i;
					}
					for (z = 0; z < seqlen; z++) /* reversed, for the strrev() below */
						wordwrap[z] = seq[seqlen - 1 - z];
					x = i - 1;
					while (str1[x] != ' ' && x > 0 && z < sizeof(wordwrap) - 1)
						wordwrap[z++] = str1[x--];
					size_t linecols = maxlen < maxcols ? maxlen : maxcols;
					if (width(0, x) < linecols / 2) {
						wordwrap[seqlen] = 0;  /* only wrap one character */
						strrev(wordwrap);
						strcpy(strout, str1);
						if (strip_invalid_attr(strout) && !(mode & K_NOECHO))
							redrwstr(strout, i, l, K_MSG | rmode);
						if (!(mode & (K_NOECHO | K_NOCRLF)))
							term->newline();
						return i;
					}
					wordwrap[z] = 0;
					if (!(mode & K_NOECHO))
						term->backspace(width(x, l));
					i = x;
					strrev(wordwrap);
					str1[x] = 0;
					strcpy(strout, str1);
					if (strip_invalid_attr(strout) && !(mode & K_NOECHO))
						redrwstr(strout, i, x, K_MSG | rmode);
					if (!(mode & (K_NOECHO | K_NOCRLF)))
						term->newline();
					return x;
				}
				if (ch < ' ') {
					outchar(BEL);
					break;
				}
				if (ch == ' ' && (mode & K_TRIM) && i && str1[i - 1] == ' ')
					continue;
				if ((mode & K_UPRLWR) && !(ch & 0x80)) {
					if (!i || (i && (str1[i - 1] == ' ' || str1[i - 1] == '-'
					                 || str1[i - 1] == '.' || str1[i - 1] == '_')))
						seq[0] = toupper(ch);
					else
						seq[0] = tolower(ch);
				}
				if (!insert_bytes(seq, seqlen, /* may_overwrite: */ true))
					outchar(BEL);   /* Added at Angus McLeod's request */
				break;
			}
		}
		if (i > l)
			l = i;
		if (mode & K_CHAT && !l)
			return 0;
	}
	getstr_offset = i;
	if (!online)
		return 0;
	if (i > l)
		l = i;
	str1[l] = 0;
	if (!(sys_status & SS_ABORT)) {
		strcpy(strout, str1);
		if (mode & K_TRIM)
			truncsp(strout);
		if ((strip_invalid_attr(strout) || (console & CON_INSERT)) && !(mode & K_NOECHO))
			redrwstr(strout, i, l, P_AUTO_UTF8 | rmode);
	}
	else
		l = 0;
	if (mode & K_LINE && !(mode & K_NOECHO))
		attr(LIGHTGRAY);
	if (!(mode & (K_NOCRLF | K_NOECHO))) {
		if (!(mode & K_MSG && sys_status & SS_ABORT)) {
			term->newline();
		} else
			term->carriage_return();
		term->lncntr = 0;
	}
	return l;
}

/****************************************************************************/
/* Hot keyed number input routine.                                          */
/* Returns a valid number between 1 and max, 0 if no number entered, or -1  */
/* if the user hit the quit key (e.g. 'Q') or ctrl-c                        */
/* If navkey is non-NULL, a navigation key (up/down/home/end/pageup/pagedn) */
/* hit before any digits is stored in *navkey and GETNUM_NAVKEY is returned */
/****************************************************************************/
int sbbs_t::getnum(uint max, uint dflt, char* navkey)
{
	uchar ch, n = 0;
	int   i = 0;

	while (online) {
		ch = getkey(K_UPPER);
		if (ch > 0x7f)
			continue;
		if (is_quit_key(ch)) {
			outchar(ch);
			if (useron.misc & COLDKEYS)
				ch = getkey(K_UPPER);
			if (ch == BS || ch == DEL) {
				term->backspace();
				continue;
			}
			term->newline();
			term->lncntr = 0;
			return -1;
		}
		else if (sys_status & SS_ABORT) {
			term->newline();
			term->lncntr = 0;
			return -1;
		}
		else if (ch == CR) {
			term->newline();
			term->lncntr = 0;
			if (!n)
				return dflt;
			return i;
		}
		else if (navkey != nullptr && !n
		         && (ch == TERM_KEY_UP || ch == TERM_KEY_DOWN
		             || ch == TERM_KEY_HOME || ch == TERM_KEY_END
		             || ch == TERM_KEY_PAGEUP || ch == TERM_KEY_PAGEDN)) {
			*navkey = ch;
			return GETNUM_NAVKEY;
		}
		else if ((ch == BS || ch == DEL) && n) {
			term->backspace();
			i /= 10;
			n--;
		}
		else if (IS_DIGIT(ch) && (i * 10UL) + (ch & 0xf) <= max && (dflt || ch != '0' || n)) {
			i *= 10L;
			n++;
			i += ch & 0xf;
			outchar(ch);
			if (i * 10UL > max && !(useron.misc & COLDKEYS) && keybuf_level() < 1) {
				term->newline();
				term->lncntr = 0;
				return i;
			}
		}
	}
	return 0;
}
