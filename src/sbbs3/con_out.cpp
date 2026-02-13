/* Synchronet console output routines */

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
#include "unicode.h"
#include "petdefs.h"
#include "cp437defs.h"

char* sbbs_t::auto_utf8(const char* str, int& mode)
{
	// If the string starts with a UTF-8 BOM, set P_UTF8 mode and skip the BOM
	if (strncmp(str, "\xEF\xBB\xBF", 3) == 0) {
		mode |= P_UTF8;
		return (char*)str + 3;
	}
	if (mode & P_AUTO_UTF8) {
		if (!str_is_ascii(str) && utf8_str_is_valid(str))
			mode |= P_UTF8;
	}
	return (char*)str;
}

/****************************************************************************/
/* Outputs a NULL terminated string locally and remotely (if applicable)    */
/* Handles ctrl-a codes, Telnet-escaping, column & line count, auto-pausing */
/* Supported P_* mode flags:
   P_PETSCII
   P_UTF8
   P_AUTO_UTF8
   P_NOATCODES
   P_TRUNCATE
   P_REMOTE
 ****************************************************************************/
int sbbs_t::bputs(const char *str, int mode)
{
	int    i;
	size_t l = 0;

	if ((mode & P_REMOTE) && online != ON_REMOTE)
		return 0;

	if (online == ON_LOCAL)  /* script running as event */
		return lputs(LOG_INFO, str);

	uint cols = term->print_cols(mode);
	if (mode & P_CENTER) {
		size_t len = term->bstrlen(str);
		term->carriage_return();
		if (len < cols)
			term->cursor_right((cols - len) / 2);
	}
	str = auto_utf8(str, mode);
	size_t len = strlen(str);
	while (l < len && online) {
		switch (str[l]) {
			case '\b':
			case '\r':
			case '\n':
			case FF:
			case CTRL_A:
				break;
			default: // printing char
				if ((mode & P_TRUNCATE) && term->column >= (cols - 1)) {
					l++;
					continue;
				}
		}
		if (str[l] == CTRL_A && str[l + 1] != 0) {
			l++;
			if (str[l] == 'Z')   /* EOF (uppercase 'Z' only) */
				break;
			if (str[l] == '~') { // Mouse hot-spot (hungry)
				l++;
				if (str[l] >= ' ')
					term->add_hotspot(str[l], /* hungry */ true);
				else
					term->add_hotspot('\r', /* hungry */ true);
				continue;
			}
			if (str[l] == '`' && str[l + 1] >= ' ') { // Mouse hot-spot (strict)
				l++;
				term->add_hotspot(str[l], /* hungry */ false);
				continue;
			}
			ctrl_a(str[l++]);
			continue;
		}
		if (!(mode & P_NOATCODES) && str[l] == '@') {
			if (str == mnestr          /* Mnemonic string or */
			    || (mode & P_ATCODES) /* trusted parameters to formatted string */
				) {
				i = show_atcode(str + l);   /* return 0 if not valid @ code */
				l += i;                   /* i is length of code string */
				if (i)                   /* if valid string, go to top */
					continue;
			}
			for (i = 0; i < TOTAL_TEXT; i++)
				if (str == text[i])
					break;
			if (i < TOTAL_TEXT) {      /* Replacement text */
				i = show_atcode(str + l);
				l += i;
				if (i)
					continue;
			}
		}
		if (mode & P_PETSCII) {
			if (term->charset() == CHARSET_PETSCII)
				term_out(str[l++]);
			else
				petscii_to_ansibbs(str[l++]);
		} else if (str[l] == '\r' && str[l + 1] == '\n') {
			term->newline();
			l += 2;
		} else if ((str[l] & 0x80) && (mode & P_UTF8)) {
			if (term->charset() == CHARSET_UTF8)
				term_out(str[l++]);
			else
				l += print_utf8_as_cp437(str + l, len - l);
		} else
			outchar(str[l++]);
	}
	return l;
}

/* Perform PETSCII terminal output translation (from ASCII/CP437) */
unsigned char cp437_to_petscii(unsigned char ch)
{
	if (IS_ALPHA(ch))
		return ch ^ 0x20;   /* swap upper/lower case */
	switch (ch) {
		case '\1':      return '@';
		case '\x10':    return '>';
		case '\x11':    return '<';
		case '\x18':
		case '\x1e':    return PETSCII_UPARROW;
		case '\x19':
		case '\x1f':    return 'V';
		case '\x1a':    return '>';
		case '|':       return PETSCII_VERTLINE;
		case '\\':      return PETSCII_BACKSLASH;
		case '`':       return PETSCII_BACKTICK;
		case '~':       return PETSCII_TILDE;
		case '_':       return PETSCII_UNDERSCORE;
		case '{':       return '(';
		case '}':       return ')';
		case '\b':      return PETSCII_LEFT;
		case 156:       return PETSCII_BRITPOUND;
		case 251:       return PETSCII_CHECKMARK;
		case 176:       return PETSCII_LIGHTHASH;
		case 177:       return PETSCII_MEDIUMHASH;
		case 178:       return PETSCII_HEAVYHASH;
		case 219:       return PETSCII_SOLID;
		case 220:       return PETSCII_BOTTOMHALF;
		case 221:       return PETSCII_LEFTHALF;
		case 222:       return PETSCII_RIGHTHALF;
		case 223:       return PETSCII_TOPHALF;
		case 254:       return PETSCII_UPRLFTBOX;
		/* Line drawing chars */
		case 186:
		case 179:       return PETSCII_VERTLINE;
		case 205:
		case 196:       return PETSCII_HORZLINE;
		case 206:
		case 215:
		case 216:
		case 197:       return PETSCII_CROSS;
		case 188:
		case 189:
		case 190:
		case 217:       return '\xBD';
		case 201:
		case 213:
		case 214:
		case 218:       return '\xB0';
		case 183:
		case 184:
		case 187:
		case 191:       return '\xAE';
		case 200:
		case 211:
		case 212:
		case 192:       return '\xAD';
		case 198:
		case 199:
		case 204:
		case 195:       return '\xAB';
		case 180:
		case 181:
		case 182:
		case 185:       return '\xB3';
		case 203:
		case 209:
		case 210:
		case 194:       return '\xB2';
		case 202:
		case 207:
		case 208:
		case 193:       return '\xB1';
	}
	if (ch & 0x80)
		return exascii_to_ascii_char(ch);
	return ch;
}

/* Perform PETSCII conversion to ANSI-BBS/CP437 */
int sbbs_t::petscii_to_ansibbs(unsigned char ch)
{
	if ((ch & 0xe0) == 0xc0)   /* "Codes $60-$7F are, actually, copies of codes $C0-$DF" */
		ch = 0x60 | (ch & 0x1f);
	if (IS_ALPHA(ch))
		return outchar(ch ^ 0x20);  /* swap upper/lower case */
	switch (ch) {
		case '\r':                  term->newline();      break;
		case PETSCII_HOME:          term->cursor_home();  break;
		case PETSCII_CLEAR:         return cls();
		case PETSCII_DELETE:        term->backspace();    break;
		case PETSCII_LEFT:          term->cursor_left();  break;
		case PETSCII_RIGHT:         term->cursor_right(); break;
		case PETSCII_UP:            term->cursor_up();    break;
		case PETSCII_DOWN:          term->cursor_down();  break;

		case PETSCII_BRITPOUND:     return outchar((char)156);
		case PETSCII_CHECKMARK:     return outchar((char)251);
		case PETSCII_CHECKERBRD:
		case PETSCII_LIGHTHASH:     return outchar((char)176);
		case 0x7e:
		case PETSCII_MEDIUMHASH:    return outchar((char)177);
		case PETSCII_HEAVYHASH:     return outchar((char)178);
		case PETSCII_SOLID:         return outchar((char)219);
		case PETSCII_BOTTOMHALF:    return outchar((char)220);
		case PETSCII_LEFTHALF:      return outchar((char)221);
		case PETSCII_RIGHTHALF:     return outchar((char)222);
		case PETSCII_TOPHALF:       return outchar((char)223);
		case PETSCII_LWRLFTBOX:
		case PETSCII_LWRRHTBOX:
		case PETSCII_UPRRHTBOX:
		case PETSCII_UPRLFTBOX:     return outchar((char)254);

		/* Line drawing chars */
		case 0x7D:
		case PETSCII_VERTLINE:      return outchar((char)179);
		case PETSCII_HORZLINE:      return outchar((char)196);
		case 0x7B:
		case PETSCII_CROSS:         return outchar((char)197);
		case (uchar)'\xBD':         return outchar((char)217);
		case (uchar)'\xB0':         return outchar((char)218);
		case (uchar)'\xAE':         return outchar((char)191);
		case (uchar)'\xAD':         return outchar((char)192);
		case (uchar)'\xAB':         return outchar((char)195);
		case (uchar)'\xB3':         return outchar((char)180);
		case (uchar)'\xB2':         return outchar((char)194);
		case (uchar)'\xB1':         return outchar((char)193);
		case PETSCII_BLACK:         return attr(BLACK);
		case PETSCII_WHITE:         return attr(WHITE);
		case PETSCII_RED:           return attr(RED);
		case PETSCII_GREEN:         return attr(GREEN);
		case PETSCII_BLUE:          return attr(BLUE);
		case PETSCII_ORANGE:        return attr(MAGENTA);
		case PETSCII_BROWN:         return attr(BROWN);
		case PETSCII_YELLOW:        return attr(YELLOW);
		case PETSCII_CYAN:          return attr(LIGHTCYAN);
		case PETSCII_LIGHTRED:      return attr(LIGHTRED);
		case PETSCII_DARKGRAY:      return attr(DARKGRAY);
		case PETSCII_MEDIUMGRAY:    return attr(CYAN);
		case PETSCII_LIGHTGREEN:    return attr(LIGHTGREEN);
		case PETSCII_LIGHTBLUE:     return attr(LIGHTBLUE);
		case PETSCII_LIGHTGRAY:     return attr(LIGHTGRAY);
		case PETSCII_PURPLE:        return attr(LIGHTMAGENTA);
		case PETSCII_REVERSE_ON:    return attr((curatr & 0x07) << 4);
		case PETSCII_REVERSE_OFF:   return attr(curatr >> 4);
		case PETSCII_FLASH_ON:      return attr(curatr | BLINK);
		case PETSCII_FLASH_OFF:     return attr(curatr & ~BLINK);
		default:
			if (ch & 0x80)
				return bprintf("#%3d", ch);
			return outchar(ch);
		case PETSCII_UPPERLOWER:
		case PETSCII_UPPERGRFX:
			/* Do nothing */
			return 0;
	}
	return 0;
}

// Return length of sequence
size_t sbbs_t::print_utf8_as_cp437(const char* str, size_t len)
{
	if (((*str) & 0x80) == 0) {
		outchar(*str);
		return sizeof(char);
	}
	enum unicode_codepoint codepoint = UNICODE_UNDEFINED;
	len = utf8_getc(str, len, &codepoint);
	if ((int)len < 2) {
		outchar(*str);  // Assume it's a CP437 character
		lprintf(LOG_DEBUG, "Invalid UTF-8 sequence: %02X (error = %d)", (uchar) * str, (int)len);
		return 1;
	}
	for (int i = 1; i < 0x100; i++) {
		if (cp437_unicode_tbl[i]
		    && cp437_unicode_tbl[i] == codepoint) {
			outchar(i);
			return len;
		}
	}
	size_t width = unicode_width(codepoint, unicode_zerowidth);
	char ch = unicode_to_cp437(codepoint);
	if (ch)
		outchar(ch);
	else {
		outchar(CP437_INVERTED_QUESTION_MARK);
		char seq[32] = "";
		for (size_t i = 0; i < len; i++)
			snprintf(seq + strlen(seq), 4, "%02X ", (uchar) * (str + i));
		lprintf(LOG_DEBUG, "Unsupported UTF-8 sequence: %s (U+%X) width=%u", seq, codepoint, (uint)width);
	}
	for (size_t i = 1; i < width; ++i)
		outchar(' ');
	return len;
}

/****************************************************************************/
/* Raw put string (remotely)												*/
/* Performs Telnet IAC escaping												*/
/* Performs charset translations											*/
/* Performs saveline buffering (for restoreline)							*/
/* DOES NOT expand ctrl-A codes, track columns, lines, auto-pause, etc.     */
/****************************************************************************/
size_t sbbs_t::rputs(const char *str, size_t len)
{
	unsigned oldlc = term->lncntr;
	size_t ret = cp437_out(str, len ? len : SIZE_MAX);
	term->lncntr = oldlc;
	return ret;
}

/*
 * Translates from CP437 to the current encoding and passes the result
 * to term_out()
 * Returns the number of bytes successfully consumed
 */
size_t sbbs_t::cp437_out(const char *str, size_t len)
{
	if (len == SIZE_MAX)
		len = strlen(str);
	for (size_t l = 0; l < len && online; l++) {
		if (cp437_out(str[l]) == 0)
			return l;
	}
	return len;
}

size_t sbbs_t::cp437_out(int ich)
{
	unsigned char ch {static_cast<unsigned char>(ich)};
	if (console & CON_ECHO_OFF)
		return 0;
	if (!online)
		return 0;

	// Many of the low CP437 characters will print on many terminals,
	// so we may want to translate some of them.
	// Synchronet specifically has definitions for the following:
	//
	// BEL (0x07) Make a beeping noise
	// BS  (0x08) Move left one character, but do not wrap to
	//            previous line
	// LF  (0x0A) Move down one line, scrolling if on bottom row
	// CR  (0x0D) Move to the first position in the current line
	//
	// Further, the following are expected to be translated
	// earlier and should only arrive here via rputs()
	// TAB (0x09) Expanded to spaces to the next tabstop (defined by
	//            sbbs::tabstop)
	// FF  (0x0C) Clear the screen
	//
	// It is unclear what "should" happen when these arrive here,
	// but it should be consistent, likely they should be expanded.
	switch (ch) {
		case '\b':	// BS
			term->cursor_left();
			return 1;
		case '\t':	// TAB
			if (term->column < (term->cols - 1)) {
                                term_out(' ');
                                while ((term->column < (term->cols - 1)) && (term->column % term->tabstop))
                                        term_out(' ');
                        }
                        return 1;

		case '\n':	// LF
			term->line_feed();
			return 1;
		case '\r':	// CR
			term->carriage_return();
			return 1;
		case FF:	// FF
			term->clearscreen();
			return 1;
	}

	// UTF-8 Note that CP437 0x7C maps to U+00A6 so we can't just do
	//       everything below 0x80 this way.
	if (term->charset() == CHARSET_UTF8) {
		if (ch != '\a') {
			char utf8[UTF8_MAX_LEN + 1];
			enum unicode_codepoint codepoint = cp437_unicode_tbl[ch];
			if (codepoint == 0)
				codepoint = static_cast<enum unicode_codepoint>(ch);
			int len = utf8_putc(utf8, sizeof(utf8), codepoint);

			if (len < 1)
				return 0;
			if (term_out(utf8, len) != (size_t)len)
				return 0;
			return 1;
		}
	}
	// PETSCII
	else if (term->charset() == CHARSET_PETSCII) {
		ch = cp437_to_petscii(ch);
		// TODO: This needs to be aware of the current state of reverse...
		//       It could cast sbbs->term to PETSCII_Terminal (ugh)
		if (ch == PETSCII_SOLID) {
			if (term_out(PETSCII_REVERSE_ON) != 1)
				return 0;
		}
		if (term_out(ch) != 1)
			return 0;
		if (ch == PETSCII_SOLID) {
			if (term_out(PETSCII_REVERSE_OFF) != 1)
				return 0;
		}
		return 1;
	}
	// CP437 or US-ASCII
	if ((term->charset() == CHARSET_ASCII) && (ch & 0x80))
		ch = exascii_to_ascii_char(ch);  /* seven bit table */
	if (term_out(ch) != 1)
		return 0;
	return 1;
}

/*
 * Update column, row, line buffer, and line counter
 * Returns the number of bytes consumed
 */
size_t sbbs_t::term_out(const char *str, size_t len)
{
	if (len == SIZE_MAX)
		len = strlen(str);
	for (size_t l = 0; l < len && online; l++) {
		uchar ch = str[l];
		if (!term_out(ch))
			return l;
	}
	return len;
}

size_t sbbs_t::term_out(int ich)
{
	unsigned char ch{static_cast<unsigned char>(ich)};
	if (console & CON_ECHO_OFF)
		return 0;
	if (!online)
		return 0;
	// We do this before parse_output() so parse_output() can
	// prevent \n from ending up at the start of the line buffer.
	if (term->lbuflen < LINE_BUFSIZE && !term->suspend_lbuf) {
		if (term->lbuflen == 0)
			term->latr = curatr;
		// Historically, beeps don't go into lbuf
		// hopefully nobody notices and cares, because this would mean
		// that BEL *must* be part of any charset we support... and it's
		// not part of C64 PETSCII.
		term->lbuf[term->lbuflen++] = ch;
	}
	if (!term->parse_output(ch))
		return 1;
	if (ch == TELNET_IAC && !(telnet_mode & TELNET_MODE_OFF)) {
		if (outcom(TELNET_IAC))
			return 0;
	}
	if (outcom(ch))
		return 0;
	return 1;
}

/****************************************************************************/
/* Performs printf() using bbs bputs function								*/
/****************************************************************************/
int sbbs_t::bprintf(const char *fmt, ...)
{
	va_list argptr;
	char    sbuf[4096];

	if (strchr(fmt, '%') == NULL)
		return bputs(fmt);
	va_start(argptr, fmt);
	vsnprintf(sbuf, sizeof(sbuf), fmt, argptr);
	sbuf[sizeof(sbuf) - 1] = 0; /* force termination */
	va_end(argptr);
	return bputs(sbuf);
}

/****************************************************************************/
/* Performs printf() using bbs bputs function (with mode)					*/
/****************************************************************************/
int sbbs_t::bprintf(int mode, const char *fmt, ...)
{
	va_list argptr;
	char    sbuf[4096];

	if (strchr(fmt, '%') == NULL)
		return bputs(fmt, mode);
	va_start(argptr, fmt);
	vsnprintf(sbuf, sizeof(sbuf), fmt, argptr);
	sbuf[sizeof(sbuf) - 1] = 0; /* force termination */
	va_end(argptr);
	return bputs(sbuf, mode);
}

/****************************************************************************/
/* Performs printf() using bbs rputs function								*/
/****************************************************************************/
int sbbs_t::rprintf(const char *fmt, ...)
{
	va_list argptr;
	char    sbuf[4096];

	va_start(argptr, fmt);
	vsnprintf(sbuf, sizeof(sbuf), fmt, argptr);
	sbuf[sizeof(sbuf) - 1] = 0; /* force termination */
	va_end(argptr);
	return rputs(sbuf);
}

/****************************************************************************/
/* Performs printf() using bbs term_out functions						*/
/****************************************************************************/
int sbbs_t::term_printf(const char *fmt, ...)
{
	va_list argptr;
	char    sbuf[4096];

	va_start(argptr, fmt);
	vsnprintf(sbuf, sizeof(sbuf), fmt, argptr);
	sbuf[sizeof(sbuf) - 1] = 0; /* force termination */
	va_end(argptr);
	return term_out(sbuf);
}

char* sbbs_t::term_rows(user_t* user, char* str, size_t size)
{
	if (user->rows >= TERM_ROWS_MIN && user->rows <= TERM_ROWS_MAX)
		term->rows = user->rows;
	safe_snprintf(str, size, "%s%d %s", user->rows ? nulstr:text[TerminalAutoDetect], term->rows, text[TerminalRows]);
	return str;
}

char* sbbs_t::term_cols(user_t* user, char* str, size_t size)
{
	if (user->cols >= TERM_COLS_MIN && user->cols <= TERM_COLS_MAX)
		term->cols = user->cols;
	safe_snprintf(str, size, "%s%d %s", user->cols ? nulstr:text[TerminalAutoDetect], term->cols, text[TerminalColumns]);
	return str;
}

/****************************************************************************/
/* Returns verbose description of the terminal type	configuration for user	*/
/****************************************************************************/
char* sbbs_t::term_type(user_t* user, int term, char* str, size_t size)
{
	if (term & PETSCII)
		safe_snprintf(str, size, "%sCBM/PETSCII"
		              , (user->misc & AUTOTERM) ? text[TerminalAutoDetect] : nulstr);
	else
		safe_snprintf(str, size, "%s%s / %s %s%s%s"
		              , (user->misc & AUTOTERM) ? text[TerminalAutoDetect] : nulstr
		              , term_charset(term)
		              , term_type(term)
		              , (term & COLOR) ? (term & ICE_COLOR ? text[TerminalIceColor] : text[TerminalColor]) : text[TerminalMonochrome]
		              , (term & MOUSE) ? text[TerminalMouse] : ""
		              , (term & SWAP_DELETE) ? "DEL=BS" : nulstr);
	truncsp(str);
	return str;
}

/****************************************************************************/
/* Returns short description of the terminal type							*/
/****************************************************************************/
const char* sbbs_t::term_type(int term)
{
	if (term == -1)
		term = this->term->flags();
	if (term & PETSCII)
		return "PETSCII";
	if (term & RIP)
		return "RIP";
	if (term & ANSI)
		return "ANSI";
	return "DUMB";
}

/****************************************************************************/
/* Returns description of the terminal supported character set (charset)	*/
/****************************************************************************/
const char* sbbs_t::term_charset(int term)
{
	if (term == -1)
		return this->term->charset_str();
	if (term & PETSCII)
		return "CBM-ASCII";
	if (term & UTF8)
		return "UTF-8";
	if (term & NO_EXASCII)
		return "US-ASCII";
	return "CP437";
}

/****************************************************************************/
/* For node spying purposes													*/
/****************************************************************************/
bool sbbs_t::update_nodeterm(void)
{
	update_terminal(this);
	str_list_t ini = strListInit();
	iniSetInteger(&ini, ROOT_SECTION, "cols", term->cols, NULL);
	iniSetInteger(&ini, ROOT_SECTION, "rows", term->rows, NULL);
	iniSetString(&ini, ROOT_SECTION, "desc", terminal, NULL);
	iniSetString(&ini, ROOT_SECTION, "type", term_type(), NULL);
	iniSetString(&ini, ROOT_SECTION, "chars", term->charset_str(), NULL);
	iniSetHexInt(&ini, ROOT_SECTION, "flags", term->flags(), NULL);
	iniSetHexInt(&ini, ROOT_SECTION, "mouse", mouse_mode, NULL);
	iniSetHexInt(&ini, ROOT_SECTION, "console", console, NULL);

	char  path[MAX_PATH + 1];
	SAFEPRINTF(path, "%sterminal.ini", cfg.node_dir);
	FILE* fp = iniOpenFile(path, /* for_modify: */ TRUE);
	bool  result = false;
	if (fp != NULL) {
		result = iniWriteFile(fp, ini);
		iniCloseFile(fp);
	}
	strListFree(&ini);

	if (mqtt->connected) {
		char str[256];
		char topic[128];
		SAFEPRINTF(topic, "node/%u/terminal", cfg.node_num);
		snprintf(str, sizeof(str), "%u\t%u\t%s\t%s\t%s\t%x\t%x\t%x"
		         , term->cols
		         , term->rows
		         , terminal
		         , term_type()
		         , term->charset_str()
		         , term->flags()
		         , mouse_mode
		         , console
		         );
		mqtt_pub_strval(mqtt, TOPIC_BBS, topic, str);
	}
	return result;
}

/*
 * bputs  putmsg
 *   \      /
 *   outchar    rputs
 *      +---------|
 *            cp437_out     outcp
 *                +-----------+
 * putcom      term_out
 *    +-----------+
 *              outcom
 *                |
 *           RingBufWrite
 * 
 * In this model:
 * bputs() and putmsg() call term_out to bypass charset translations
 * (ie: PETSCII and UTF-8 output)
 * 
 * outchar() does tab, FF, rainbow, and auto-pause
 * 
 * rputs() effectively just calls cp437_out() and keeps the line counter
 *         unchanged.  It's used by console.print(), so keeping the
 *         behaviour unchanged may be important.
 * 
 * cp437_out() translates from cp437 to the terminal charset this
 *             specifically includes the control characters
 *             BEL, BS, TAB, LF, FF, CR, and DEL
 * 
 * outcp() calls term_out() if UTF-8 is supported, or bputs() if not
 * 
 * putcom() is used not only for text, but also to send telnet commands.
 *          The use for text could be replaced, but the use for IAC
 *          needs to be unchanged.
 * 
 * term_out() updates column and row, and maintains the line buffer
 * 
 * outcom() and RingBufWrite() are post-IAC expansion
 */

/****************************************************************************/
/* Outputs character														*/
/* Performs charset translations (e.g. EXASCII-to-ASCII, CP437-to-PETSCII)	*/
/* Performs terminal expansions and state parsing (e.g. FF to ESC[2JESC[H)	*/
/* Performs Telnet IAC escaping												*/
/* Performs tab expansion													*/
/* Performs column counting, line counting, and auto-pausing				*/
/* Performs saveline buffering (for restoreline)							*/
/****************************************************************************/
int sbbs_t::outchar(char ch)
{
	if (console & CON_ECHO_OFF)
		return 0;

	if (rainbow_index >= 0) {
		attr(rainbow[rainbow_index]);
		if (rainbow[rainbow_index + 1] == 0) {
			if (rainbow_wrap)
				rainbow_index = 0;
		} else
			++rainbow_index;
	}
	if ((console & CON_PASSWORD) && (uchar)ch >= ' ')
		ch = *text[PasswordChar];

	if (ch == '\n' && line_delay)
		SLEEP(line_delay);

	/*
	 * When line counter overflows, pause on the next pause-eligible line
	 * and log a debug message
	 */
	if (term->lncntr >= term->rows - 1 && term->column == 0) {
		unsigned lost = term->lncntr - (term->rows - 1);
		term->lncntr = term->rows -1;
		if (check_pause()) {
			if (lost)
				lprintf(LOG_DEBUG, "line counter overflowed, %u lines scrolled", lost);
		}
	}

	if (ch == FF) {
		if (term->lncntr > 0 && term->row > 0) {
			term->lncntr = 0;
			term->newline(); // Legacy behavior: FF does LF first, conditional-LF would have made more sense (but would be a behavior change)
			if (!(sys_status & SS_PAUSEOFF)) { // Intentionally ignore UPAUSE here
				pause();
				while (term->lncntr && online && !(sys_status & SS_ABORT))
					pause();
			}
		}
	}

	cp437_out(ch);

	check_pause();
	return 0;
}

bool sbbs_t::pause_enabled() {
	return (((useron.misc ^ (console & CON_PAUSE)) & UPAUSE) || (sys_status & SS_PAUSEON))
		&& !(sys_status & (SS_PAUSEOFF | SS_ABORT));
}

bool sbbs_t::check_pause() {
	if (term->lncntr == term->rows - 1 && pause_enabled()) {
		term->lncntr = 0;
		pause();
		return true;
	}
	return false;
}

int sbbs_t::outcp(enum unicode_codepoint codepoint, const char* cp437_fallback)
{
	if (term->charset() == CHARSET_UTF8) {
		char str[UTF8_MAX_LEN];
		int  len = utf8_putc(str, sizeof(str), codepoint);
		if (len < 1)
			return len;
		term_out(str, len);
		term->inc_column(unicode_width(codepoint, unicode_zerowidth));
		return 0;
	}
	if (cp437_fallback == NULL)
		return 0;
	return bputs(cp437_fallback);
}

int sbbs_t::outcp(enum unicode_codepoint codepoint, char cp437_fallback)
{
	char str[2] = { cp437_fallback, '\0' };
	return outcp(codepoint, str);
}

void sbbs_t::wide(const char* str)
{
	while (*str != '\0') {
		if ((term->charset() == CHARSET_UTF8) && *str >= '!' && *str <= '~')
			outcp((enum unicode_codepoint)(UNICODE_FULLWIDTH_EXCLAMATION_MARK + (*str - '!')));
		else {
			outchar(*str);
			outchar(' ');
		}
		str++;
	}
}

/****************************************************************************/
/* Get the dimensions of the current user console, place into row and cols	*/
/****************************************************************************/
void sbbs_t::getdimensions()
{
	if (sys_status & SS_USERON) {
		term->getdims();
		if (useron.rows >= TERM_ROWS_MIN && useron.rows <= TERM_ROWS_MAX)
			term->rows = useron.rows;
		if (useron.cols >= TERM_COLS_MIN && useron.cols <= TERM_COLS_MAX)
			term->cols = useron.cols;
	}
}

/****************************************************************************/
/* performs the correct attribute modifications for the Ctrl-A code			*/
/****************************************************************************/
void sbbs_t::ctrl_a(char x)
{
	uint       atr = curatr;
	char	   tmp[64];

	if (x && (uchar)x <= CTRL_Z) {    /* Ctrl-A through Ctrl-Z for users with MF only */
		if (!(useron.flags1 & FLAG(x + 64)))
			console ^= (CON_ECHO_OFF);
		return;
	}
	switch (toupper(x)) {
		case '!':   /* level 10 or higher */
			if (useron.level < 10)
				console ^= CON_ECHO_OFF;
			break;
		case '@':   /* level 20 or higher */
			if (useron.level < 20)
				console ^= CON_ECHO_OFF;
			break;
		case '#':   /* level 30 or higher */
			if (useron.level < 30)
				console ^= CON_ECHO_OFF;
			break;
		case '$':   /* level 40 or higher */
			if (useron.level < 40)
				console ^= CON_ECHO_OFF;
			break;
		case '%':   /* level 50 or higher */
			if (useron.level < 50)
				console ^= CON_ECHO_OFF;
			break;
		case '^':   /* level 60 or higher */
			if (useron.level < 60)
				console ^= CON_ECHO_OFF;
			break;
		case '&':   /* level 70 or higher */
			if (useron.level < 70)
				console ^= CON_ECHO_OFF;
			break;
		case '*':   /* level 80 or higher */
			if (useron.level < 80)
				console ^= CON_ECHO_OFF;
			break;
		case '(':   /* level 90 or higher */
			if (useron.level < 90)
				console ^= CON_ECHO_OFF;
			break;
		case ')':   /* turn echo back on */
			console &= ~CON_ECHO_OFF;
			break;
	}
	if (console & CON_ECHO_OFF)
		return;

	if ((uchar)x > 0x7f) {
		term->cursor_right((uchar)x - 0x7f);
		return;
	}
	if (valid_ctrl_a_attr(x))
		rainbow_index = -1;
	if (IS_DIGIT(x)) {   /* background color */
		atr &= (BG_BRIGHT | BLINK | 0x0f);
	}
	switch (toupper(x)) {
		case '+':   /* push current attribute */
			if (attr_sp < (int)(sizeof(attr_stack) / sizeof(attr_stack[0])))
				attr_stack[attr_sp++] = curatr;
			break;
		case '-':   /* pop current attribute OR optimized "normal" */
			if (attr_sp > 0)
				attr(attr_stack[--attr_sp]);
			else                                    /* turn off all attributes if */
			if (atr & (HIGH | BLINK | BG_LIGHTGRAY)) /* high intensity, blink or */
				attr(LIGHTGRAY);                    /* background bits are set */
			break;
		case '_':                               /* turn off all attributes if */
			if (atr & (BLINK | BG_LIGHTGRAY))        /* blink or background is set */
				attr(LIGHTGRAY);
			break;
		case 'P':   /* Pause */
			pause();
			break;
		case 'Q':   /* Pause reset */
			term->lncntr = 0;
			break;
		case 'T':   /* Time */
			bputs(time_as_hhmmss(&cfg, time(nullptr), tmp, sizeof tmp));
			break;
		case 'D':   /* Date */
			now = time(NULL);
			bputs(datestr(now));
			break;
		case ',':   /* Delay 1/10 sec */
			mswait(100);
			break;
		case ';':   /* Delay 1/2 sec */
			mswait(500);
			break;
		case '.':   /* Delay 2 secs */
			mswait(2000);
			break;
		case 'S':   /* Synchronize */
			sync();
			break;
		case 'J':   /* clear to end-of-screen */
			term->cleartoeos();
			break;
		case 'L':   /* CLS (form feed) */
			cls();
			break;
		case '\'':  /* Home cursor */
		case '`':   // usurped by strict hot-spot
			term->cursor_home();
			break;
		case '>':   /* CLREOL */
			term->cleartoeol();
			break;
		case '<':   /* Non-destructive backspace */
			term->cursor_left();
			break;
		case '/':   /* Conditional new-line */
			term->cond_newline();
			break;
		case '\\':  /* Conditional New-line / Continuation prefix (if cols < 80) */
			term->cond_contline();
			break;
		case '?':   /* Conditional blank-line */
			term->cond_blankline();
			break;
		case '[':   /* Carriage return */
			term->carriage_return();
			break;
		case ']':   /* Line feed */
			term->line_feed();
			break;
		case 'A':   /* Ctrl-A */
			outchar(CTRL_A);
			break;
		case 'Z':   /* Ctrl-Z */
			outchar(CTRL_Z);
			break;
		case 'H':   /* High intensity */
			atr |= HIGH;
			attr(atr);
			break;
		case 'I':
			attr(atr | BLINK);
			break;
		case 'E': /* Bright Background */
			attr(atr | BG_BRIGHT);
			break;
		case 'F':   /* Blink, only if alt Blink Font is loaded */
			if (((atr & HIGH) && (console & CON_HBLINK_FONT)) || (!(atr & HIGH) && (console & CON_BLINK_FONT)))
				attr(atr | BLINK);
			else if (x == 'F' && !(atr & HIGH))    /* otherwise, set HIGH attribute (only if capital 'F') */
				attr(atr | HIGH);
			break;
		case 'N':   /* Normal */
			attr(LIGHTGRAY);
			break;
		case 'R':
			atr = (atr & 0xf8) | RED;
			attr(atr);
			break;
		case 'G':
			atr = (atr & 0xf8) | GREEN;
			attr(atr);
			break;
		case 'B':
			atr = (atr & 0xf8) | BLUE;
			attr(atr);
			break;
		case 'W':   /* White */
			atr = (atr & 0xf8) | LIGHTGRAY;
			attr(atr);
			break;
		case 'C':
			atr = (atr & 0xf8) | CYAN;
			attr(atr);
			break;
		case 'M':
			atr = (atr & 0xf8) | MAGENTA;
			attr(atr);
			break;
		case 'Y':   /* Yellow */
			atr = (atr & 0xf8) | BROWN;
			attr(atr);
			break;
		case 'K':   /* Black */
			atr = (atr & 0xf8) | BLACK;
			attr(atr);
			break;
		case '0':   /* Black Background */
			attr(atr);
			break;
		case '1':   /* Red Background */
			attr(atr | BG_RED);
			break;
		case '2':   /* Green Background */
			attr(atr | BG_GREEN);
			break;
		case '3':   /* Yellow Background */
			attr(atr | BG_BROWN);
			break;
		case '4':   /* Blue Background */
			attr(atr | BG_BLUE);
			break;
		case '5':   /* Magenta Background */
			attr(atr | BG_MAGENTA);
			break;
		case '6':   /* Cyan Background */
			attr(atr | BG_CYAN);
			break;
		case '7':   /* White Background */
			attr(atr | BG_LIGHTGRAY);
			break;
		case 'U':   /* User Theme */
			attr(cfg.color[x == 'u' ? clr_userlow : clr_userhigh]);
			break;
		case 'X':   // Rainbow
			if (rainbow[rainbow_index + 1] != 0)
				++rainbow_index;
			rainbow_wrap = (x == 'X');
			break;
	}
}

/****************************************************************************/
/* Sends terminal control codes to change remote terminal colors/attributes */
/****************************************************************************/
int sbbs_t::attr(int atr)
{
	char str[128];

	term->attrstr(atr, str, sizeof(str));
	term_out(str);
	curatr = atr;
	return 0;
}

/****************************************************************************/
/* Checks to see if user has hit abort (Ctrl-C). Returns 1 if user aborted. */
/* When 'clear' is false (the default), preserves SS_ABORT flag state.		*/
/****************************************************************************/
bool sbbs_t::msgabort(bool clear)
{
	static uint counter;

	if (sys_status & SS_SYSPAGE && !(++counter % 100))
		sbbs_beep(400 + sbbs_random(800), 10);

	if (sys_status & SS_ABORT) {
		if (clear)
			clearabort();
		return true;
	}
	if (!online)
		return true;
	return false;
}

void sbbs_t::clearabort()
{
	if (sys_status & SS_ABORT) {
		term->lncntr = 0;
		sys_status &= ~SS_ABORT;
	}
}

int sbbs_t::backfill(const char* instr, float pct, int full_attr, int empty_attr)
{
	uint  atr;
	uint  save_atr = curatr;
	int   len;
	char* str = strip_ctrl(instr, NULL);

	len = strlen(str);
	if (!(term->can_highlight()))
		bputs(str, P_REMOTE);
	else {
		for (int i = 0; i < len; i++) {
			if (((float)(i + 1) / len) * 100.0 <= pct)
				atr = full_attr;
			else
				atr = empty_attr;
			if (curatr != atr)
				attr(atr);
			outchar(str[i]);
		}
		attr(save_atr);
	}
	free(str);
	return len;
}

void sbbs_t::progress(const char* text, int count, int total, int interval)
{
	char   str[128];

	if (cfg.node_num == 0)
		return; // Don't output this for events
	double now = xp_timer();
	if (count > 0 && count < total && (now - last_progress) * 1000 < interval)
		return;
	if (text == NULL)
		text = "";
	float pct = total ? ((float)count / total) * 100.0F : 100.0F;
	SAFEPRINTF2(str, "[ %-8s %5.1f%% ]", text, pct);
	term->cond_newline();
	term->cursor_left(backfill(str, pct, cfg.color[clr_progress_full], cfg.color[clr_progress_empty]));
	last_progress = now;
}
