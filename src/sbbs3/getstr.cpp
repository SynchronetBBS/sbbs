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
/* Waits for remote or local user to input a CR terminated string. 'length' */
/* is the maximum number of characters that getstr will allow the user to   */
/* input into the string. 'mode' specifies upper case characters are echoed */
/* or wordwrap or if in message input (^A sequences allowed). ^W backspaces */
/* a word, ^X backspaces a line, ^Gs, BSs, TABs are processed, LFs ignored. */
/* ^N non-destructive BS, ^V center line. Valid keys are echoed.            */
/****************************************************************************/
size_t sbbs_t::getstr(char *strout, size_t maxlen, int mode, const str_list_t history)
{
	size_t i, l, x, z;    /* i=current position, l=length, j=printed chars */
	/* x&z=misc */
	char   str1[256], str2[256], undo[256];
	uchar  ch;
	uint   atr;
	int    hidx = -1;
	int    org_column = term->column;
	int    org_lbuflen = term->lbuflen;

	console &= ~(CON_UPARROW | CON_DOWNARROW | CON_LEFTARROW | CON_RIGHTARROW | CON_BACKSPACE | CON_DELETELINE);
	if (!(mode & K_WORDWRAP))
		console &= ~CON_INSERT;
	clearabort();
	if (!(mode & K_LINEWRAP) && term->cols >= TERM_COLS_MIN && !(mode & K_NOECHO) && !(console & CON_R_ECHOX)
	    && term->column + (int)maxlen >= term->cols)    /* Don't allow the terminal to auto line-wrap */
		maxlen = term->cols - term->column - 1;
	if (mode & K_LINE && (term->can_highlight()) && !(mode & K_NOECHO)) {
		attr(cfg.color[clr_inputline]);
		for (i = 0; i < maxlen; i++)
			term_out(' ');
		term->cursor_left(maxlen);
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
	if (strlen(str1) > maxlen)
		str1[maxlen] = 0;
	atr = curatr;
	if (!(mode & K_NOECHO)) {
		if (mode & K_AUTODEL && str1[0]) {
			i = (cfg.color[clr_inputline] & 0x07) << 4;
			i |= (cfg.color[clr_inputline] & 0x70) >> 4;
			attr(i);
		}
		bputs(str1, P_AUTO_UTF8);
		if (mode & K_EDIT && !(mode & (K_LINE | K_AUTODEL)))
			term->cleartoeol();  /* destroy to eol */
	}

	SAFECOPY(undo, str1);
	i = l = term->bstrlen(str1, P_AUTO_UTF8);
	if (mode & K_AUTODEL && str1[0] && !(mode & K_NOECHO)) {
		ch = getkey(mode | K_GETSTR);
		attr(atr);
		if (IS_PRINTABLE(ch) || ch == DEL) {
			for (i = 0; i < l; i++)
				term->backspace();
			i = l = 0;
		}
		else {
			for (i = 0; i < l; i++)
				outchar(BS);
			bputs(str1, P_AUTO_UTF8);
			i = l;
		}
		if (ch != ' ' && ch != TAB)
			ungetkey(ch);
	}

	if (mode & K_USEOFFSET) {
		i = getstr_offset;
		if (i > l)
			i = l;
		if (l - i) {
			term->cursor_left(l - i);
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
		if (ch == LF && mode & K_MSG) { /* Down-arrow same as CR */
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
				if (!(mode & K_MSG) || useron.rest & FLAG('A') || i > maxlen - 3)
					break;
				if (console & CON_INSERT) {
					if (l < maxlen)
						l++;
					for (x = l; x > i; x--)
						str1[x] = str1[x - 1];
					rprintf("%.*s", (int)(l - i), str1 + i);
					term->cursor_left(l - i);
#if 0
					if (i == maxlen - 1)
						console &= ~CON_INSERT;
#endif
				}
				outchar(str1[i++] = 1);
				break;
			case TERM_KEY_HOME: /* Ctrl-B Beginning of Line */
				if (i && !(mode & K_NOECHO)) {
					term->cursor_left(i);
					i = 0;
				}
				break;
			case CTRL_D: /* Ctrl-D Delete word right */
				if (i < l) {
					x = i;
					while (x < l && str1[x] != ' ') {
						outchar(' ');
						x++;
					}
					while (x < l && str1[x] == ' ') {
						outchar(' ');
						x++;
					}
					term->cursor_left(x - i);   /* move cursor back */
					z = i;
					while (z < l - (x - i))  {             /* move chars in string */
						outchar(str1[z] = str1[z + (x - i)]);
						z++;
					}
					while (z < l) {                    /* write over extra chars */
						outchar(' ');
						z++;
					}
					term->cursor_left(z - i);
					l -= x - i;                         /* l=new length */
				}
				break;
			case TERM_KEY_END: /* Ctrl-E End of line */
				if (term->can_move() && i < l) {
					term->cursor_right(l - i);  /* move cursor to eol */
					i = l;
				}
				break;
			case TERM_KEY_RIGHT: /* Ctrl-F move cursor forward */
				if (i < l && term->can_move()) {
					term->cursor_right();   /* move cursor right one */
					i++;
				}
				break;
			case CTRL_G: /* Bell */
				if (!(mode & K_MSG))
					break;
				if (useron.rest & FLAG('B')) {
					if (i + 6 < maxlen) {
						if (console & CON_INSERT) {
							for (x = l + 6; x > i; x--)
								str1[x] = str1[x - 6];
							if (l + 5 < maxlen)
								l += 6;
#if 0
							if (i == maxlen - 1)
								console &= ~CON_INSERT;
#endif
						}
						str1[i++] = '(';
						str1[i++] = 'b';
						str1[i++] = 'e';
						str1[i++] = 'e';
						str1[i++] = 'p';
						str1[i++] = ')';
						if (!(mode & K_NOECHO))
							bputs("(beep)");
					}
					if (console & CON_INSERT)
						redrwstr(str1, i, l, 0);
					break;
				}
				if (console & CON_INSERT) {
					if (l < maxlen)
						l++;
					for (x = l; x > i; x--)
						str1[x] = str1[x - 1];
#if 0
					if (i == maxlen - 1)
						console &= ~CON_INSERT;
#endif
				}
				if (i < maxlen) {
					str1[i++] = BEL;
					if (!(mode & K_NOECHO))
						outchar(BEL);
				}
				break;
			case CTRL_H:    /* Ctrl-H/Backspace */
				if (i == 0) {
					console |= CON_BACKSPACE;
					break;
				}
				do {
					i--;
					l--;
				} while ((term->charset() == CHARSET_UTF8) && (i > 0) && (str1[i] & 0x80) && (str1[i - 1] & 0x80));
				if (i != l) {              /* Deleting char in middle of line */
					outchar(BS);
					z = i;
					while (z < l)  {       /* move the characters in the line */
						outchar(str1[z] = str1[z + 1]);
						z++;
					}
					outchar(' ');        /* write over the last char */
					term->cursor_left((l - i) + 1);
				}
				else if (!(mode & K_NOECHO))
					term->backspace();
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
							while (i--)
								term->backspace();
							i = l = strlen(str1);
							rputs(str1);
							term->cleartoeol();
							break;
						}
					break;
				}
				if (!(i % EDIT_TABSIZE)) {
					if (console & CON_INSERT) {
						if (l < maxlen)
							l++;
						for (x = l; x > i; x--)
							str1[x] = str1[x - 1];
#if 0
						if (i == maxlen - 1)
							console &= ~CON_INSERT;
#endif
					}
					str1[i++] = ' ';
					if (!(mode & K_NOECHO))
						outchar(' ');
				}
				while (i < maxlen && i % EDIT_TABSIZE) {
					if (console & CON_INSERT) {
						if (l < maxlen)
							l++;
						for (x = l; x > i; x--)
							str1[x] = str1[x - 1];
#if 0
						if (i == maxlen - 1)
							console &= ~CON_INSERT;
#endif
					}
					str1[i++] = ' ';
					if (!(mode & K_NOECHO))
						outchar(' ');
				}
				if (console & CON_INSERT && !(mode & K_NOECHO))
					redrwstr(str1, i, l, 0);
				break;

			case CTRL_L:    /* Ctrl-L   Center line (used to be Ctrl-V) */
				str1[l] = 0;
				l = term->bstrlen(str1);
				if (!l)
					break;
				for (x = 0; x < (maxlen - l) / 2; x++)
					str2[x] = ' ';
				str2[x] = 0;
				SAFECAT(str2, str1);
				strcpy(strout, str2);
				l = strlen(strout);
				if (mode & K_NOECHO)
					return l;
				if (mode & K_MSG)
					redrwstr(strout, i, l, K_MSG);
				else {
					while (i--)
						bputs("\b");
					bputs(strout);
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
					term->cursor_right(i - x);
				}
				break;
			case CTRL_R:    /* Ctrl-R Redraw Line */
				if (!(mode & K_NOECHO))
					redrwstr(str1, i, l, P_AUTO_UTF8);
				break;
			case TERM_KEY_INSERT:   /* Ctrl-V			Toggles Insert/Overwrite */
				if (mode & K_NOECHO)
					break;
				console ^= CON_INSERT;
				term->insert_indicator();
				break;
			case CTRL_W:    /* Ctrl-W   Delete word left */
				if (i < l) {
					x = i;                            /* x=original offset */
					while (i && str1[i - 1] == ' ') {
						outchar(BS);
						i--;
					}
					while (i && str1[i - 1] != ' ') {
						outchar(BS);
						i--;
					}
					z = i;                            /* i=z=new offset */
					while (z < l - (x - i))  {             /* move chars in string */
						outchar(str1[z] = str1[z + (x - i)]);
						z++;
					}
					while (z < l) {                    /* write over extra chars */
						outchar(' ');
						z++;
					}
					term->cursor_left(z - i);               /* back to new x corridnant */
					l -= x - i;                         /* l=new length */
				} else {
					while (i && str1[i - 1] == ' ') {
						i--;
						l--;
						if (!(mode & K_NOECHO))
							term->backspace();
					}
					while (i && str1[i - 1] != ' ') {
						i--;
						l--;
						if (!(mode & K_NOECHO))
							term->backspace();
					}
				}
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
					term->cursor_left(i);
					term->cleartoeol();
					l = 0;
					term->lbuflen = org_lbuflen;
				}
				i = 0;
				console |= CON_DELETELINE;
				break;
			case CTRL_Z:    /* Undo */
				if (!(mode & K_NOECHO)) {
					while (i--)
						term->backspace();
				}
				SAFECOPY(str1, undo);
				i = l = strlen(str1);
				rputs(str1);
				term->cleartoeol();
				break;
			case CTRL_BACKSLASH:    /* Ctrl-\ Previous word */
				if (i && !(mode & K_NOECHO)) {
					x = i;
					while (str1[i - 1] == ' ' && i)
						i--;
					while (str1[i - 1] != ' ' && i)
						i--;
					term->cursor_left(x - i);
				}
				break;
			case TERM_KEY_LEFT:  /* Ctrl-]/Left Arrow  Reverse Cursor Movement */
				if (i == 0) {
					if (mode & K_LEFTEXIT)
						console |= CON_LEFTARROW;
					break;
				}
				if (!(mode & K_NOECHO)) {
					term->cursor_left();   /* move cursor left one */
					i--;
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
					while (i--)
						term->backspace();
					i = l = strlen(str1);
					rputs(str1);
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
					while (i--)
						term->backspace();
					SAFECOPY(str1, history[hidx]);
					i = l = strlen(str1);
					rputs(str1);
					term->cleartoeol();
					break;
				}
				if (!(mode & K_EDIT))
					break;
#if 1
				if (i > l)
					l = i;
				str1[l] = 0;
				strcpy(strout, str1);
				if ((strip_invalid_attr(strout) || console & CON_INSERT) && !(mode & K_NOECHO))
					redrwstr(strout, i, l, K_MSG);
				if (mode & K_LINE && !(mode & K_NOECHO))
					attr(LIGHTGRAY);
				console |= CON_UPARROW;
				return l;
#else
				console |= CON_UPARROW;
				break;
#endif
			case TERM_KEY_DELETE:  /* Ctrl-BkSpc (DEL) Delete current char */
				if (i == l) {  /* Backspace if end of line */
					if (i) {
						do {
							i--;
							l--;
							if (!(mode & K_NOECHO))
								term->backspace();
						} while ((term->charset() == CHARSET_UTF8) && (i > 0) && (str1[i] & 0x80) && (str1[i - 1] & 0x80));
					}
					break;
				}
				l--;
				z = i;
				while (z < l)  {       /* move the characters in the line */
					outchar(str1[z] = str1[z + 1]);
					z++;
				}
				outchar(' ');        /* write over the last char */
				term->cursor_left((l - i) + 1);
				break;
			default:
				if (mode & K_WORDWRAP && i == maxlen && ch >= ' ' && !(console & CON_INSERT)) {
					str1[i] = 0;
					if (ch == ' ' && !(mode & K_CHAT)) { /* don't wrap a space */
						strcpy(strout, str1);       /* as last char */
						if (strip_invalid_attr(strout) && !(mode & K_NOECHO))
							redrwstr(strout, i, l, K_MSG);
						if (!(mode & (K_NOECHO | K_NOCRLF)))
							term->newline();
						return i;
					}
					x = i - 1;
					z = 1;
					wordwrap[0] = ch;
					while (str1[x] != ' ' && x > 0 && z < sizeof(wordwrap) - 1)
						wordwrap[z++] = str1[x--];
					if (x < (maxlen / 2)) {
						wordwrap[1] = 0;  /* only wrap one character */
						strcpy(strout, str1);
						if (strip_invalid_attr(strout) && !(mode & K_NOECHO))
							redrwstr(strout, i, l, K_MSG);
						if (!(mode & (K_NOECHO | K_NOCRLF)))
							term->newline();
						return i;
					}
					wordwrap[z] = 0;
					if (!(mode & K_NOECHO))
						while (z--) {
							term->backspace();
							i--;
						}
					strrev(wordwrap);
					str1[x] = 0;
					strcpy(strout, str1);
					if (strip_invalid_attr(strout) && !(mode & K_NOECHO))
						redrwstr(strout, i, x, mode);
					if (!(mode & (K_NOECHO | K_NOCRLF)))
						term->newline();
					return x;
				}
				if (i < maxlen && ch >= ' ') {
					if (ch == ' ' && (mode & K_TRIM) && i && str1[i - 1] == ' ')
						continue;
					if ((mode & K_UPRLWR) && !(ch & 0x80)) {
						if (!i || (i && (str1[i - 1] == ' ' || str1[i - 1] == '-'
						                 || str1[i - 1] == '.' || str1[i - 1] == '_')))
							ch = toupper(ch);
						else
							ch = tolower(ch);
					}
					if (console & CON_INSERT && i != l) {
						if (l < maxlen)    /* l<maxlen */
							l++;
						for (x = l; x > i; x--)
							str1[x] = str1[x - 1];
						rprintf("%.*s", (int)(l - i), str1 + i);
						term->cursor_left(l - i);
#if 0
						if (i == maxlen - 1) {
							bputs("  \b\b");
							console &= ~CON_INSERT;
						}
#endif
					}
					str1[i++] = ch;
					if (!(mode & K_NOECHO)) {
						if ((term->charset() == CHARSET_UTF8) && (ch & 0x80)) {
							if (i > l)
								l = i;
							str1[l] = 0;
							if (utf8_str_is_valid(str1))
								redrwstr(str1, term->column - org_column, l, P_UTF8);
						} else {
							outchar(ch);
						}
					}
				} else
					outchar(BEL);   /* Added at Angus McLeod's request */
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
			redrwstr(strout, i, l, P_AUTO_UTF8);
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
/****************************************************************************/
int sbbs_t::getnum(uint max, uint dflt)
{
	uchar ch, n = 0;
	int   i = 0;

	while (online) {
		ch = getkey(K_UPPER);
		if (ch > 0x7f)
			continue;
		if (ch == quit_key()) {
			outchar(quit_key());
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
