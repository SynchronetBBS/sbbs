/* Synchronet message/menu display routine */

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
#include "zmodem.h"
#include "petdefs.h"

/****************************************************************************/
/* Outputs a NULL terminated string with @-code parsing,                    */
/* checking for message aborts, pauses, ANSI escape and ^A sequences.		*/
/* Changes local text attributes if necessary.                              */
/* Returns the last char of the buffer access.. 0 if not aborted.           */
/* If P_SAVEATR bit is set in mode, the attributes set by the message       */
/* will be the current attributes after the message is displayed, otherwise */
/* the attributes prior to displaying the message are always restored.      */
/* Stops parsing/displaying upon CTRL-Z (only in P_CPM_EOF mode).           */
/****************************************************************************/
char sbbs_t::putmsg(const char *buf, int mode, int org_cols, JSObject* obj)
{
	uint             tmpatr;
	uint             org_line_delay = line_delay;
	uint             orgcon = console;
	uint             sys_status_sav = sys_status;
	enum output_rate output_rate = term->cur_output_rate;

	attr_sp = 0;  /* clear any saved attributes */
	tmpatr = curatr;  /* was lclatr(-1) */
	if (!(mode & P_SAVEATR))
		attr(LIGHTGRAY);
	if (mode & P_NOPAUSE)
		sys_status |= SS_PAUSEOFF;

	ansiParser.reset();
	char ret = putmsgfrag(buf, mode, org_cols, obj);
	if (ansiParser.current_state() != ansiState_none)
		lprintf(LOG_DEBUG, "Incomplete ANSI stripped from end");
	memcpy(rainbow, cfg.rainbow, sizeof rainbow);
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

	return ret;
}

// Print a message fragment, doesn't save/restore any console states (e.g. attributes, auto-pause)
char sbbs_t::putmsgfrag(const char* buf, int& mode, unsigned org_cols, JSObject* obj)
{
	char                 tmp[256];
	char                 tmp2[256];
	char                 path[MAX_PATH + 1];
	char*                str = (char*)buf;
	bool                 exatr = false; // attributes should be reset to default (lightgray) before newline
	char                 mark = '\0';
	int                  i;
	unsigned             col = term->column;
	uint                 l = 0;
	uint                 lines_printed = 0;
	struct mouse_hotspot hot_spot = {};

	hot_attr = 0;
	hungry_hotspots = true;
	str = auto_utf8(str, mode);
	size_t len = strlen(str);

	uint cols = term->print_cols(mode);

	if (!(mode & P_NOATCODES) && memcmp(str, "@WRAPOFF@", 9) == 0) {
		mode &= ~P_WORDWRAP;
		l += 9;
	}
	if (mode & P_WORDWRAP) {
		char* wrapoff = NULL;
		if (!(mode & P_NOATCODES)) {
			wrapoff = strstr((char*)str + l, "@WRAPOFF@");
			if (wrapoff != NULL)
				*wrapoff = 0;
		}
		char *wrapped;
		if (org_cols < TERM_COLS_MIN)
			org_cols = TERM_COLS_DEFAULT;
		if ((wrapped = ::wordwrap((char*)str + l, cols - 1, org_cols - 1, /* handle_quotes: */ TRUE, mode)) == NULL)
			errormsg(WHERE, ERR_ALLOC, "wordwrap buffer", 0);
		else {
			truncsp_lines(wrapped);
			mode &= ~P_WORDWRAP;
			putmsgfrag(wrapped, mode);
			free(wrapped);
			l = strlen(str);
			if (wrapoff != NULL)
				l += 9; // Skip "<NUL>WRAPOFF@"
		}
	}
	if (mode & P_CENTER) {
		size_t widest = widest_line(str + l);
		if (widest < cols && term->column == 0) {
			term->cursor_right((cols - widest) / 2);
			mode |= P_INDENT;
		}
	}

	while (l < len && (mode & P_NOABORT || !msgabort()) && online) {
		switch (str[l]) {
			case '\r':
			case '\n':
			case FF:
			case CTRL_A:
				break;
			case ZHEX:
				if (l && str[l - 1] == ZDLE) {
					l++;
					continue;
				}
			// fallthrough
			default: // printing char
				if ((mode & P_INDENT) && term->column < col)
					term->cursor_right(col - term->column);
				else if ((mode & P_TRUNCATE) && term->column >= (cols - 1)) {
					l++;
					continue;
				} else if (mode & P_WRAP) {
					if (org_cols) {
						if (term->column > (org_cols - 1)) {
							term->newline(1, /* no_bg_attr */true);
							++lines_printed;
						}
					} else {
						if (term->column >= (cols - 1)) {
							term->newline(1, /* no_bg_attr */true);
							++lines_printed;
						}
					}
				}
				break;
		}
		if (mode & P_MARKUP) {
			const char* marks = "*/_#";
			if (((mark == 0) && strchr(marks, str[l]) != NULL) || str[l] == mark) {
				char* next = NULL;
				char  following = '\0';
				if (mark == 0) {
					next = strchr(str + l + 1, str[l]);
					if (next != NULL)
						following = *(next + 1);
				}
				char *blank = strstr(str + l + 1, "\n\r");
				if (((l < 1 || ((IS_WHITESPACE(str[l - 1]) || IS_PUNCTUATION(str[l - 1])) && strchr(marks, str[l - 1]) == NULL))
				     && IS_ALPHANUMERIC(str[l + 1]) && mark == 0 && next != NULL
				     && (following == '\0' || IS_WHITESPACE(following) || (IS_PUNCTUATION(following) && strchr(marks, following) == NULL))
				     && (blank == NULL || next < blank))
				    || str[l] == mark) {
					if (mark == 0)
						mark = str[l];
					else {
						mark = 0;
						if (!(mode & P_HIDEMARKS))
							outchar(str[l]);
					}
					switch (str[l]) {
						case '*':
							attr(curatr ^ HIGH);
							break;
						case '/':
							attr(curatr ^ BLINK);
							break;
						case '_':
							attr(curatr ^ (HIGH | BLINK));
							break;
						case '#':
							attr(((curatr & 0x0f) << 4) | ((curatr & 0xf0) >> 4));
							break;
					}
					if (mark != 0 && !(mode & P_HIDEMARKS))
						outchar(str[l]);
					l++;
					continue;
				}
			}
		}
		if (str[l] == CTRL_A && str[l + 1] != 0) {
			if (str[l + 1] == '"' && !(sys_status & SS_NEST_PF) && !(mode & P_NOATCODES)) {  /* Quote a file */
				l += 2;
				i = 0;
				while (i < (int)sizeof(tmp2) - 1 && IS_PRINTABLE(str[l]) && str[l] != '\\' && str[l] != '/')
					tmp2[i++] = str[l++];
				if (i > 0) {
					tmp2[i] = 0;
					sys_status |= SS_NEST_PF;     /* keep it only one message deep! */
					SAFEPRINTF2(path, "%s%s", cfg.text_dir, tmp2);
					printfile(path, P_MODS);
					sys_status &= ~SS_NEST_PF;
				}
			}
			else if (str[l + 1] == 'Z')    /* Ctrl-AZ==EOF (uppercase 'Z' only) */
				break;
			else if (str[l + 1] == '~') {
				l += 2;
				if (str[l] >= ' ')
					term->add_hotspot(str[l], /* hungry: */ true);
				else
					term->add_hotspot('\r', /* hungry: */ true);
			}
			else if (str[l + 1] == '`' && str[l + 2] >= ' ') {
				term->add_hotspot(str[l + 2], /* hungry: */ false);
				l += 2;
			}
			else {
				bool was_tos = (term->row == 0);
				ctrl_a(str[l + 1]);
				if (term->row == 0 && !was_tos && (sys_status & SS_ABORT) && !lines_printed) /* Aborted at (auto) pause prompt (e.g. due to CLS)? */
					clearabort();                /* Clear the abort flag (keep displaying the msg/file) */
				l += 2;
			}
		}
		else if ((mode & P_PCBOARD)
		         && str[l] == '@' && str[l + 1] == 'X'
		         && IS_HEXDIGIT(str[l + 2]) && IS_HEXDIGIT(str[l + 3])
		         && !islower(str[l + 2]) && !islower(str[l + 3])) {
			uint val = (HEX_CHAR_TO_INT(str[l + 2]) << 4) + HEX_CHAR_TO_INT(str[l + 3]);
			// @X00 saves the current color and @XFF restores that saved color
			switch (val) {
				case 0x00:
					saved_pcb_attr = curatr;
					break;
				case 0xff:
					attr(saved_pcb_attr);
					break;
				default:
					attr(val);
					break;
			}
			l += 4;
		}
		else if ((mode & P_WILDCAT)
		         && str[l] == '@' && str[l + 3] == '@'
		         && IS_HEXDIGIT(str[l + 1]) && IS_HEXDIGIT(str[l + 2])
		         && !islower(str[l + 1]) && !islower(str[l + 2])) {
			attr((HEX_CHAR_TO_INT(str[l + 1]) << 4) + HEX_CHAR_TO_INT(str[l + 2]));
			l += 4;
		}
		else if ((mode & P_RENEGADE)
		         && str[l] == '|' && IS_DIGIT(str[l + 1])
		         && IS_DIGIT(str[l + 2])) {
			i = (DEC_CHAR_TO_INT(str[l + 1]) * 10) + DEC_CHAR_TO_INT(str[l + 2]);
			if (i >= 16) {                 /* setting background */
				i -= 16;
				i <<= 4;
				i |= (curatr & 0x0f);       /* leave foreground alone */
			}
			else
				i |= (curatr & 0xf0);   /* leave background alone */
			attr(i);
			exatr = true;
			l += 3;   /* Skip |xx */
		}
		else if ((mode & P_CELERITY)
		         && str[l] == '|' && IS_ALPHA(str[l + 1])) {
			switch (str[l + 1]) {
				case 'k':
					attr((curatr & 0xf0) | BLACK);
					break;
				case 'b':
					attr((curatr & 0xf0) | BLUE);
					break;
				case 'g':
					attr((curatr & 0xf0) | GREEN);
					break;
				case 'c':
					attr((curatr & 0xf0) | CYAN);
					break;
				case 'r':
					attr((curatr & 0xf0) | RED);
					break;
				case 'm':
					attr((curatr & 0xf0) | MAGENTA);
					break;
				case 'y':
					attr((curatr & 0xf0) | YELLOW);
					break;
				case 'w':
					attr((curatr & 0xf0) | LIGHTGRAY);
					break;
				case 'd':
					attr((curatr & 0xf0) | BLACK | HIGH);
					break;
				case 'B':
					attr((curatr & 0xf0) | BLUE | HIGH);
					break;
				case 'G':
					attr((curatr & 0xf0) | GREEN | HIGH);
					break;
				case 'C':
					attr((curatr & 0xf0) | CYAN | HIGH);
					break;
				case 'R':
					attr((curatr & 0xf0) | RED | HIGH);
					break;
				case 'M':
					attr((curatr & 0xf0) | MAGENTA | HIGH);
					break;
				case 'Y':   /* Yellow */
					attr((curatr & 0xf0) | YELLOW | HIGH);
					break;
				case 'W':
					attr((curatr & 0xf0) | LIGHTGRAY | HIGH);
					break;
				case 'S':   /* swap foreground and background - TODO: This sets foreground to BLACK! */
					attr((curatr & 0x07) << 4);
					break;
			}
			exatr = true;
			l += 2;   /* Skip |x */
		}  /* Skip second digit if it exists */
		else if ((mode & P_WWIV)
		         && str[l] == CTRL_C && IS_DIGIT(str[l + 1])) {
			exatr = true;
			switch (str[l + 1]) {
				default:
					attr(LIGHTGRAY);
					break;
				case '1':
					attr(CYAN | HIGH);
					break;
				case '2':
					attr(BROWN | HIGH);
					break;
				case '3':
					attr(MAGENTA);
					break;
				case '4':
					attr(LIGHTGRAY | HIGH | BG_BLUE);
					break;
				case '5':
					attr(GREEN);
					break;
				case '6':
					attr(RED | HIGH | BLINK);
					break;
				case '7':
					attr(BLUE | HIGH);
					break;
				case '8':
					attr(BLUE);
					break;
				case '9':
					attr(CYAN);
					break;
			}
			l += 2;
		}
		else {
			/*
			 * ansi escape sequence:
			 * Strip broken sequences
			 * Strip "dangerous" sequences
			 * Reset line counter on sequences that may change the row
			 * (The last is done that way for backward compatibility)
			 */
			if (term->supports(ANSI)) {
				switch (ansiParser.parse(str[l])) {
					case ansiState_broken:
						// TODO: Maybe just strip the CSI or something?
						lprintf(LOG_DEBUG, "Stripping broken ANSI sequence \"%s\"", ansiParser.ansi_sequence.c_str());
						ansiParser.reset();
						// break here prints the first non-valid character
						break;
					case ansiState_none:
						break;
					case ansiState_final:
						if ((!ansiParser.ansi_was_private) && ansiParser.ansi_final_byte == 'p')
							lprintf(LOG_DEBUG, "Stripping SKR sequence");
						else if (ansiParser.ansi_was_private && ansiParser.ansi_params[0] == '?' && ansiParser.ansi_final_byte == 'S')
							lprintf(LOG_DEBUG, "Stripping XTSRGA sequence");
						else if (ansiParser.ansi_final_byte == 'n')
							lprintf(LOG_DEBUG, "Stripping DSR sequence");
						else if (ansiParser.ansi_final_byte == 'c')
							lprintf(LOG_DEBUG, "Stripping DA sequence");
						else if (ansiParser.ansi_ibs == "," && ansiParser.ansi_final_byte == 'q')
							lprintf(LOG_DEBUG, "Stripping DECTID sequence");
						else if (ansiParser.ansi_ibs == "&" && ansiParser.ansi_final_byte == 'u')
							lprintf(LOG_DEBUG, "Stripping DECRQUPSS sequence");
						else if (ansiParser.ansi_ibs == "+" && ansiParser.ansi_final_byte == 'x')
							lprintf(LOG_DEBUG, "Stripping DECRQPKFM sequence");
						else if (ansiParser.ansi_ibs == "$" && ansiParser.ansi_final_byte == 'p')
							lprintf(LOG_DEBUG, "Stripping DECRQM sequence");
						else if (ansiParser.ansi_ibs == "$" && ansiParser.ansi_final_byte == 'u')
							lprintf(LOG_DEBUG, "Stripping DECRQTSR sequence");
						else if (ansiParser.ansi_ibs == "$" && ansiParser.ansi_final_byte == 'w')
							lprintf(LOG_DEBUG, "Stripping DECRQPSR sequence");
						else if (ansiParser.ansi_ibs == "*" && ansiParser.ansi_final_byte == 'y')
							lprintf(LOG_DEBUG, "Stripping DECRQCRA sequence");
						else if (ansiParser.ansi_sequence.substr(0, 4) == "\x1bP$q")
							lprintf(LOG_DEBUG, "Stripping DECRQSS sequence");
						else if (ansiParser.ansi_sequence.substr(0, 14) == "\x1b_SyncTERM:C;L")
							lprintf(LOG_DEBUG, "Stripping CTSFI sequence");
						else if (ansiParser.ansi_sequence.substr(0, 16) == "\x1b_SyncTERM:Q;JXL")
							lprintf(LOG_DEBUG, "Stripping CTQJS sequence");
						else {
							if ((!ansiParser.ansi_was_private) && ansiParser.ansi_ibs == "") {
								if (strchr("AFkBEeHfJdu", ansiParser.ansi_final_byte) != nullptr)    /* ANSI anim */
									term->lncntr = 0; /* so defeat pause */
							}
							term_out(ansiParser.ansi_sequence.c_str(), ansiParser.ansi_sequence.length());
						}
						ansiParser.reset();
						l++;
						continue;
					default:
						l++;
						continue;
				}
			}
			if (str[l] == '!' && str[l + 1] == '|' && useron.misc & RIP) /* RIP */
				term->lncntr = 0;             /* so defeat pause */
			if (str[l] == '@' && !(mode & P_NOATCODES)) {
				if (memcmp(str + l, "@EOF@", 5) == 0)
					break;
				if (memcmp(str + l, "@CLEAR@", 7) == 0) {
					cls();
					clearabort();
					l += 7;
					while (str[l] != 0 && (str[l] == '\r' || str[l] == '\n'))
						l++;
					continue;
				}
				if (memcmp(str + l, "@80COLS@", 8) == 0) {
					l += 8;
					if (cols > 80)
						cols = 80;
					mode |= P_80COLS;
					continue;
				}
				if (memcmp(str + l, "@CENTER@", 8) == 0) {
					l += 8;
					i = 0;
					while (i < (int)sizeof(tmp) - 1 && str[l] != 0 && str[l] != '\r' && str[l] != '\n')
						tmp[i++] = str[l++];
					tmp[i] = 0;
					truncsp(tmp);
					term->center(expand_atcodes(tmp, tmp2, sizeof tmp2), mode);
					if (str[l] == '\r')
						l++;
					if (str[l] == '\n')
						l++;
					continue;
				}
				if (memcmp(str + l, "@SYSONLY@", 9) == 0) {
					if (!useron_is_sysop())
						console ^= CON_ECHO_OFF;
					l += 9;
					continue;
				}
				if (memcmp(str + l, "@WRAP@", 6) == 0) {
					l += 6;
					mode |= P_WRAP;
					continue;
				}
				if (memcmp(str + l, "@WORDWRAP@", 10) == 0) {
					l += 10;
					mode |= P_WORDWRAP;
					return putmsgfrag(str + l, mode, org_cols);
				}
				if (memcmp(str + l, "@WRAPOFF@", 9) == 0) {
					l += 9;
					mode &= ~P_WRAP;
					continue;
				}
				if (memcmp(str + l, "@TRUNCATE@", 10) == 0) {
					l += 10;
					mode |= P_TRUNCATE;
					continue;
				}
				if (memcmp(str + l, "@TRUNCOFF@", 10) == 0) {
					l += 10;
					mode &= ~P_TRUNCATE;
					continue;
				}
				if (memcmp(str + l, "@QON@", 5) == 0) {    // Allow the file display to be aborted (PCBoard)
					l += 5;
					mode &= ~P_NOABORT;
					continue;
				}
				if (memcmp(str + l, "@STOP@", 6) == 0) {  // Wildcat!
					l += 6;
					mode &= ~P_NOABORT;
					continue;
				}
				if (memcmp(str + l, "@QOFF@", 6) == 0) {   // Do not allow the display of the file to be aborted (PCBoard)
					l += 6;
					mode |= P_NOABORT;
					continue;
				}
				if (memcmp(str + l, "@NOSTOP@", 8) == 0) { // Wildcat!
					l += 8;
					mode |= P_NOABORT;
					continue;
				}
				if (memcmp(str + l, "@NOCODE@", 8) == 0) { // Wildcat!
					l += 8;
					mode ^= P_NOATCODES;
					continue;
				}
				if (memcmp(str + l, "@XON@", 5) == 0) { // PCBoard "Enables the interpretation of @X color codes."
					l += 5;
					mode |= P_PCBOARD;
					continue;
				}
				if (memcmp(str + l, "@XOFF@", 6) == 0) { // PCBoard "Disables the interpretation of @X color codes."
					l += 6;
					mode &= ~P_PCBOARD;
					continue;
				}
				if (memcmp(str + l, "@LINEDELAY@", 11) == 0) {
					l += 11;
					line_delay = 100;
					continue;
				}
				if (memcmp(str + l, "@LINEDELAY:", 11) == 0) {
					char* p = str + l + 11;
					SKIP_DIGIT(p);
					if (*p == '@') {
						l += 11;
						line_delay = atoi(str + l) * 10;
						while (str[l] != '@')
							l++;
						l++;
						continue;
					}
				}
				bool was_tos = (term->row == 0);
				i = show_atcode((char *)str + l, cols, obj);  /* returns 0 if not valid @ code */
				l += i;                   /* i is length of code string */
				if (term->row > 0 && !was_tos && (sys_status & SS_ABORT) && !lines_printed)  /* Aborted at (auto) pause prompt (e.g. due to CLS)? */
					clearabort();                /* Clear the abort flag (keep displaying the msg/file) */
				if (i)                   /* if valid string, go to top */
					continue;
			}
			if (mode & P_CPM_EOF && str[l] == CTRL_Z)
				break;
			if (hot_attr) {
				if (curatr == hot_attr && str[l] > ' ') {
					hot_spot.y = term->row;
					if (!hot_spot.minx)
						hot_spot.minx = term->column;
					hot_spot.maxx = term->column;
					hot_spot.cmd[strlen(hot_spot.cmd)] = str[l];
				} else if (hot_spot.cmd[0]) {
					hot_spot.hungry = hungry_hotspots;
					term->add_hotspot(&hot_spot);
					memset(&hot_spot, 0, sizeof(hot_spot));
				}
			}
			size_t skip = sizeof(char);
			if (mode & P_PETSCII) {
				if (term->charset() == CHARSET_PETSCII) {
					term_out(str[l]);
				} else
					petscii_to_ansibbs(str[l]);
			} else if ((str[l] & 0x80) && (mode & P_UTF8)) {
				if (term->charset() == CHARSET_UTF8)
					term_out(str[l]);
				else
					skip = print_utf8_as_cp437(str + l, len - l);
			} else if (str[l] == '\r' && str[l + 1] == '\n') {
				if (exatr) {
					attr(LIGHTGRAY);
					exatr = false;
				}
				term->newline(1, /* no_bg_attr */(mode & P_WRAP));
				++lines_printed;
				skip++;
			} else if (str[l] == '\n') {
				if (exatr) {
					attr(LIGHTGRAY);
					exatr = false;
				}
				term->newline(1, /* no_bg_attr */(mode & P_WRAP));
				++lines_printed;
			} else {
				uint atr = curatr;
				outchar(str[l]);
				if (curatr != atr)   // We assume the attributes are retained between lines
					attr(atr);
			}
			l += skip;
		}
	}

	return str[l];
}

// Write short message (telegram) to user, supports @-codes
bool sbbs_t::putsmsg(int user_num, const char* str)
{
	return ::putsmsg(&cfg, user_num, (char*)str) == 0;
}

// Write short message to node in-use, supports @-codes
bool sbbs_t::putnmsg(int node_num, const char* str)
{
	return ::putnmsg(&cfg, node_num, (char*)str) == 0;
}
