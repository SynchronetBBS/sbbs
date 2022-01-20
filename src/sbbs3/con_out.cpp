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

char* sbbs_t::auto_utf8(const char* str, long& mode)
{
	if(strncmp(str, "\xEF\xBB\xBF", 3) == 0) {
		mode |= P_UTF8;
		return (char*)(str + 3);
	}
	if(mode & P_AUTO_UTF8) {
		if(!str_is_ascii(str) && utf8_str_is_valid(str))
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
int sbbs_t::bputs(const char *str, long mode)
{
	int i;
    ulong l=0;
	long term = term_supports();

	if((mode & P_REMOTE) && online != ON_REMOTE)
		return 0;

	if(online==ON_LOCAL && console&CON_L_ECHO) 	/* script running as event */
		return(lputs(LOG_INFO, str));

	str = auto_utf8(str, mode);
	size_t len = strlen(str);
	while(l < len && online) {
		switch(str[l]) {
			case '\b':
			case '\r':
			case '\n':
			case FF:
			case CTRL_A:
				break;
			default: // printing char
				if((mode&P_TRUNCATE) && column >= (cols - 1)) {
					l++;
					continue;
				}
		}
		if(str[l]==CTRL_A && str[l+1]!=0) {
			l++;
			if(str[l] == 'Z')	/* EOF (uppercase 'Z' only) */
				break;
			if(str[l] == '~') { // Mouse hot-spot (hungry)
				l++;
				if(str[l] >= ' ')
					add_hotspot(str[l], /* hungry */true);
				else
					add_hotspot('\r', /* hungry */true);
				continue;
			}
			if(str[l] == '`' && str[l + 1] >= ' ') { // Mouse hot-spot (strict)
				l++;
				add_hotspot(str[l], /* hungry */false);
				continue;
			}
			ctrl_a(str[l++]);
			continue;
		}
		if(!(mode&P_NOATCODES) && str[l]=='@') {
			if(str==mnestr			/* Mnemonic string or */
				|| (str>=text[0]	/* Straight out of TEXT.DAT */
					&& str<=text[TOTAL_TEXT-1])) {
				i=show_atcode(str+l);	/* return 0 if not valid @ code */
				l+=i;					/* i is length of code string */
				if(i)					/* if valid string, go to top */
					continue;
			}
			for(i=0;i<TOTAL_TEXT;i++)
				if(str==text[i])
					break;
			if(i<TOTAL_TEXT) {		/* Replacement text */
				i=show_atcode(str+l);
				l+=i;
				if(i)
					continue;
			}
		}
		if(mode&P_PETSCII) {
			if(term&PETSCII)
				outcom(str[l++]);
			else
				petscii_to_ansibbs(str[l++]);
		} else if((str[l]&0x80) && (mode&P_UTF8)) {
			if(term&UTF8)
				outcom(str[l++]);
			else
				l += print_utf8_as_cp437(str + l, len - l);
		} else
			outchar(str[l++]);
	}
	return(l);
}

/****************************************************************************/
/* Returns the printed columns from 'str' accounting for Ctrl-A codes		*/
/****************************************************************************/
size_t sbbs_t::bstrlen(const char *str, long mode)
{
	str = auto_utf8(str, mode);
	size_t count = 0;
	const char* end = str + strlen(str);
	while (str < end) {
		int len = 1;
		if(*str == CTRL_A) {
			str++;
			if(*str == 0 || *str == 'Z')	// EOF
				break;
			if(*str == '[') // CR
				count = 0;
			else if(*str == '<' && count) // ND-Backspace
				count--;
		} else if(((*str) & 0x80) && (mode&P_UTF8)) {
			enum unicode_codepoint codepoint = UNICODE_UNDEFINED;
			len = utf8_getc(str, end - str, &codepoint);
			if(len < 1)
				break;
			count += unicode_width(codepoint);;
		} else
			count++;
		str += len;
	}
	return count;
}


/* Perform PETSCII terminal output translation (from ASCII/CP437) */
unsigned char cp437_to_petscii(unsigned char ch)
{
	if(IS_ALPHA(ch))
		return ch ^ 0x20;	/* swap upper/lower case */
	switch(ch) {
		case '\1':		return '@';
		case '\x10':	return '>';
		case '\x11':	return '<';
		case '\x18':	
		case '\x1e':	return PETSCII_UPARROW;
		case '\x19':
		case '\x1f':	return 'V';
		case '\x1a':	return '>';
		case '|':		return PETSCII_VERTLINE;
		case '\\':		return PETSCII_BACKSLASH;
		case '`':		return PETSCII_BACKTICK;
		case '~':		return PETSCII_TILDE;
		case '_':		return PETSCII_UNDERSCORE;
		case '{':		return '(';
		case '}':		return ')';
		case '\b':		return PETSCII_LEFT;
		case 156:		return PETSCII_BRITPOUND;
		case 251:		return PETSCII_CHECKMARK;
		case 176:		return PETSCII_LIGHTHASH;
		case 177:		return PETSCII_MEDIUMHASH;
		case 178:		return PETSCII_HEAVYHASH;
		case 219:		return PETSCII_SOLID;
		case 220:		return PETSCII_BOTTOMHALF;
		case 221:		return PETSCII_LEFTHALF;
		case 222:		return PETSCII_RIGHTHALF;
		case 223:		return PETSCII_TOPHALF;
		case 254:		return PETSCII_UPRLFTBOX;
		/* Line drawing chars */
		case 186:
		case 179:		return PETSCII_VERTLINE;
		case 205:
		case 196:		return PETSCII_HORZLINE;
		case 206:
		case 215:
		case 216:
		case 197:		return PETSCII_CROSS;
		case 188:
		case 189:
		case 190:
		case 217:		return '\xBD';
		case 201:
		case 213:
		case 214:
		case 218:		return '\xB0';
		case 183:
		case 184:
		case 187:
		case 191:		return '\xAE';
		case 200:
		case 211:
		case 212:
		case 192:		return '\xAD';
		case 198:
		case 199:
		case 204:
		case 195:		return '\xAB';
		case 180:
		case 181:
		case 182:
		case 185:		return '\xB3';
		case 203:
		case 209:
		case 210:
		case 194:		return '\xB2';
		case 202:
		case 207:
		case 208:
		case 193:		return '\xB1';
	}
	if(ch&0x80)
		return exascii_to_ascii_char(ch);
	return ch;
}

/* Perform PETSCII conversion to ANSI-BBS/CP437 */
int sbbs_t::petscii_to_ansibbs(unsigned char ch)
{
	if((ch&0xe0) == 0xc0)	/* "Codes $60-$7F are, actually, copies of codes $C0-$DF" */
		ch = 0x60 | (ch&0x1f);
	if(IS_ALPHA(ch))
		return outchar(ch ^ 0x20);	/* swap upper/lower case */
	switch(ch) {
		case '\r':					newline();		break;
		case PETSCII_HOME:			cursor_home();	break;
		case PETSCII_CLEAR:			return CLS;
		case PETSCII_DELETE:		backspace();	break;
		case PETSCII_LEFT:			cursor_left();	break;
		case PETSCII_RIGHT:			cursor_right();	break;
		case PETSCII_UP:			cursor_up();	break;
		case PETSCII_DOWN:			cursor_down();	break;

		case PETSCII_BRITPOUND:		return outchar((char)156);
		case PETSCII_CHECKMARK:		return outchar((char)251);
		case PETSCII_CHECKERBRD:
		case PETSCII_LIGHTHASH:		return outchar((char)176);
		case 0x7e:
		case PETSCII_MEDIUMHASH:	return outchar((char)177);
		case PETSCII_HEAVYHASH:		return outchar((char)178);
		case PETSCII_SOLID:			return outchar((char)219);
		case PETSCII_BOTTOMHALF:	return outchar((char)220);
		case PETSCII_LEFTHALF:		return outchar((char)221);
		case PETSCII_RIGHTHALF:		return outchar((char)222);
		case PETSCII_TOPHALF:		return outchar((char)223);
		case PETSCII_LWRLFTBOX:
		case PETSCII_LWRRHTBOX:
		case PETSCII_UPRRHTBOX:
		case PETSCII_UPRLFTBOX:		return outchar((char)254);

		/* Line drawing chars */
		case 0x7D:
		case PETSCII_VERTLINE:		return outchar((char)179);
		case PETSCII_HORZLINE:		return outchar((char)196);
		case 0x7B:
		case PETSCII_CROSS:			return outchar((char)197);
		case (uchar)'\xBD':			return outchar((char)217);
		case (uchar)'\xB0':			return outchar((char)218);
		case (uchar)'\xAE':			return outchar((char)191);
		case (uchar)'\xAD':			return outchar((char)192);
		case (uchar)'\xAB':			return outchar((char)195);
		case (uchar)'\xB3':			return outchar((char)180);
		case (uchar)'\xB2':			return outchar((char)194);
		case (uchar)'\xB1':			return outchar((char)193);
		case PETSCII_BLACK:			return attr(BLACK);
		case PETSCII_WHITE:			return attr(WHITE);
		case PETSCII_RED:			return attr(RED);
		case PETSCII_GREEN:			return attr(GREEN);
		case PETSCII_BLUE:			return attr(BLUE);
		case PETSCII_ORANGE:		return attr(MAGENTA);
		case PETSCII_BROWN:			return attr(BROWN);
		case PETSCII_YELLOW:		return attr(YELLOW);
		case PETSCII_CYAN:			return attr(LIGHTCYAN);
		case PETSCII_LIGHTRED:		return attr(LIGHTRED);
		case PETSCII_DARKGRAY:		return attr(DARKGRAY);
		case PETSCII_MEDIUMGRAY:	return attr(CYAN);
		case PETSCII_LIGHTGREEN:	return attr(LIGHTGREEN);
		case PETSCII_LIGHTBLUE:		return attr(LIGHTBLUE);
		case PETSCII_LIGHTGRAY:		return attr(LIGHTGRAY);
		case PETSCII_PURPLE:		return attr(LIGHTMAGENTA);
		case PETSCII_REVERSE_ON:	return attr((curatr&0x07) << 4);
		case PETSCII_REVERSE_OFF:	return attr(curatr >> 4);
		case PETSCII_FLASH_ON:		return attr(curatr | BLINK);
		case PETSCII_FLASH_OFF:		return attr(curatr & ~BLINK);
		default:					
			if(ch&0x80)				return bprintf("#%3d", ch);
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
	if(((*str)&0x80) == 0) {
		outchar(*str);
		return sizeof(char);
	}
	enum unicode_codepoint codepoint = UNICODE_UNDEFINED;
	len = utf8_getc(str, len, &codepoint);
	if((int)len < 2) {
		outchar(*str);	// Assume it's a CP437 character
		lprintf(LOG_DEBUG, "Invalid UTF-8 sequence: %02X (error = %d)", (uchar)*str, (int)len);
		return 1;
	}
	for(int i = 1; i < 0x100; i++) {
		if(cp437_unicode_tbl[i]
			&& cp437_unicode_tbl[i] == codepoint) {
			outchar(i);
			return len;
		}
	}
	char ch = unicode_to_cp437(codepoint);
	if(ch)
		outchar(ch);
	else if(unicode_width(codepoint) > 0) {
		outchar(CP437_INVERTED_QUESTION_MARK);
		char seq[32] = "";
		for(size_t i = 0; i < len; i++)
			sprintf(seq + strlen(seq), "%02X ", (uchar)*(str + i));
		lprintf(LOG_DEBUG, "Unsupported UTF-8 sequence: %s (U+%X)", seq, codepoint);
	}
	return len;
}

/****************************************************************************/
/* Raw put string (remotely)												*/
/* Performs Telnet IAC escaping												*/
/* Performs charset translations											*/
/* Performs saveline buffering (for restoreline)							*/
/* DOES NOT expand ctrl-A codes, track columns, lines, auto-pause, etc.     */
/****************************************************************************/
int sbbs_t::rputs(const char *str, size_t len)
{
    size_t	l;

	if(console&CON_ECHO_OFF)
		return 0;
	if(len==0)
		len=strlen(str);
	long term = term_supports();
	char utf8[UTF8_MAX_LEN + 1] = "";
	for(l=0;l<len && online;l++) {
		uchar ch = str[l];
		utf8[0] = 0;
		if(term&PETSCII) {
			ch = cp437_to_petscii(ch);
			if(ch == PETSCII_SOLID)
				outcom(PETSCII_REVERSE_ON);
		}
		else if((term&NO_EXASCII) && (ch&0x80))
			ch = exascii_to_ascii_char(ch);  /* seven bit table */
		else if(term&UTF8) {
			enum unicode_codepoint codepoint = cp437_unicode_tbl[(uchar)ch];
			if(codepoint != 0)
				utf8_putc(utf8, sizeof(utf8) - 1, codepoint);
		}
		if(utf8[0])
			putcom(utf8);
		else {
			if(outcom(ch)!=0)
				break;
			if((char)ch == (char)TELNET_IAC && !(telnet_mode&TELNET_MODE_OFF))
				outcom(TELNET_IAC);	/* Must escape Telnet IAC char (255) */
			if((term&PETSCII) && ch == PETSCII_SOLID)
				outcom(PETSCII_REVERSE_OFF);
		}
		if(ch == '\n')
			lbuflen=0;
		else if(lbuflen<LINE_BUFSIZE) {
			if(lbuflen == 0)
				latr = curatr;
			lbuf[lbuflen++] = ch;
		}
	}
	return(l);
}

/****************************************************************************/
/* Performs printf() using bbs bputs function								*/
/****************************************************************************/
int sbbs_t::bprintf(const char *fmt, ...)
{
	va_list argptr;
	char sbuf[4096];

	if(strchr(fmt,'%')==NULL)
		return(bputs(fmt));
	va_start(argptr,fmt);
	vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;	/* force termination */
	va_end(argptr);
	return(bputs(sbuf));
}

/****************************************************************************/
/* Performs printf() using bbs bputs function (with mode)					*/
/****************************************************************************/
int sbbs_t::bprintf(long mode, const char *fmt, ...)
{
	va_list argptr;
	char sbuf[4096];

	if(strchr(fmt,'%')==NULL)
		return(bputs(fmt, mode));
	va_start(argptr,fmt);
	vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;	/* force termination */
	va_end(argptr);
	return(bputs(sbuf, mode));
}

/****************************************************************************/
/* Performs printf() using bbs rputs function								*/
/****************************************************************************/
int sbbs_t::rprintf(const char *fmt, ...)
{
	va_list argptr;
	char sbuf[4096];

	va_start(argptr,fmt);
	vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;	/* force termination */
	va_end(argptr);
	return(rputs(sbuf));
}

/****************************************************************************/
/* Performs printf() using bbs putcom/outcom functions						*/
/****************************************************************************/
int sbbs_t::comprintf(const char *fmt, ...)
{
	va_list argptr;
	char sbuf[4096];

	va_start(argptr,fmt);
	vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;	/* force termination */
	va_end(argptr);
	return(putcom(sbuf));
}

/****************************************************************************/
/* Outputs destructive backspace 											*/
/****************************************************************************/
void sbbs_t::backspace(int count)
{
	if(count < 1)
		return;
	if(!(console&CON_ECHO_OFF)) {
		for(int i = 0; i < count; i++) {
			if(term_supports(PETSCII))
				outcom(PETSCII_DELETE);
			else {
				outcom('\b');
				outcom(' ');
				outcom('\b');
			}
			if(column > 0)
				column--;
			if(lbuflen > 0)
				lbuflen--;
		}
	}
}

/****************************************************************************/
/* Returns true if the user (or the yet-to-be-logged-in client) supports	*/
/* all of the specified terminal 'cmp_flags' (e.g. ANSI, COLOR, RIP).		*/
/* If no flags specified, returns all terminal flag bits supported			*/
/****************************************************************************/
long sbbs_t::term_supports(long cmp_flags)
{
	long flags = ((sys_status&SS_USERON) && !(useron.misc&AUTOTERM)) ? useron.misc : autoterm;

	if((sys_status&SS_USERON) && (useron.misc&AUTOTERM))
		flags |= useron.misc & (NO_EXASCII | SWAP_DELETE | COLOR | ICE_COLOR | MOUSE);

	return(cmp_flags ? ((flags&cmp_flags)==cmp_flags) : (flags&TERM_FLAGS));
}

/****************************************************************************/
/* Returns description of the terminal type									*/
/****************************************************************************/
const char* sbbs_t::term_type(long term)
{
	if(term == -1)
		term = term_supports();
	if(term&PETSCII)
		return "PETSCII";
	if(term&RIP)
		return "RIP";
	if(term&ANSI)
		return "ANSI";
	return "DUMB";
}

/****************************************************************************/
/* Returns description of the terminal supported character set (charset)	*/
/****************************************************************************/
const char* sbbs_t::term_charset(long term)
{
	if(term == -1)
		term = term_supports();
	if(term&PETSCII)
		return "CBM-ASCII";
	if(term&UTF8)
		return "UTF-8";
	if(term&NO_EXASCII)
		return "US-ASCII";
	return "CP437";
}

/****************************************************************************/
/* For node spying purposes													*/
/****************************************************************************/
bool sbbs_t::update_nodeterm(void)
{
	str_list_t	ini = strListInit();
	iniSetInteger(&ini, ROOT_SECTION, "cols", cols, NULL);
	iniSetInteger(&ini, ROOT_SECTION, "rows", rows, NULL);
	iniSetString(&ini, ROOT_SECTION, "type", term_type(), NULL);
	iniSetString(&ini, ROOT_SECTION, "chars", term_charset(), NULL);
	iniSetHexInt(&ini, ROOT_SECTION, "flags", term_supports(), NULL);
	iniSetHexInt(&ini, ROOT_SECTION, "mouse", mouse_mode, NULL);
	iniSetHexInt(&ini, ROOT_SECTION, "console", console, NULL);

	char path[MAX_PATH + 1];
	SAFEPRINTF(path, "%sterminal.ini", cfg.node_dir);
	FILE* fp = iniOpenFile(path, /* create: */TRUE);
	bool result = false;
	if(fp != NULL) {
		result = iniWriteFile(fp, ini);
		iniCloseFile(fp);
	}
	strListFree(&ini);
	return result;
}

/****************************************************************************/
/* Outputs character														*/
/* Performs terminal translations (e.g. EXASCII-to-ASCII, FF->ESC[2J)		*/
/* Performs Telnet IAC escaping												*/
/* Performs tab expansion													*/
/* Performs column counting, line counting, and auto-pausing				*/
/* Performs saveline buffering (for restoreline)							*/
/****************************************************************************/
int sbbs_t::outchar(char ch)
{
	if(console&CON_ECHO_OFF)
		return 0;
	if(ch == ESC && outchar_esc < ansiState_string)
		outchar_esc = ansiState_esc;
	else if(outchar_esc == ansiState_esc) {
		if(ch == '[')
			outchar_esc = ansiState_csi;
		else if(ch == '_' || ch == 'P' || ch == '^' || ch == ']')
			outchar_esc = ansiState_string;
		else if(ch=='X')
			outchar_esc = ansiState_sos;
		else if(ch >= '@' && ch <= '_')
			outchar_esc = ansiState_final;
		else
			outchar_esc = ansiState_none;
	}
	else if(outchar_esc == ansiState_csi) {
		if(ch >= '@' && ch <= '~')
			outchar_esc = ansiState_final;
	}
	else if(outchar_esc == ansiState_string) {	// APS, DCS, PM, or OSC
		if (ch == ESC)
			outchar_esc = ansiState_esc;
		if (!((ch >= '\b' && ch <= '\r') || (ch >= ' ' && ch <= '~')))
			outchar_esc = ansiState_none;
	}
	else if(outchar_esc == ansiState_sos) {	// SOS
		if (ch == ESC)
			outchar_esc = ansiState_sos_esc;
	}
	else if(outchar_esc == ansiState_sos_esc) {	// ESC inside SOS
		if (ch == '\\')
			outchar_esc = ansiState_esc;
		else if (ch == 'X')
			outchar_esc = ansiState_none;
		else
			outchar_esc = ansiState_sos;
	}
	else
		outchar_esc = ansiState_none;
	long term = term_supports();
	char utf8[UTF8_MAX_LEN + 1] = "";
	if(!(term&PETSCII)) {
		if((term&NO_EXASCII) && (ch&0x80))
			ch = exascii_to_ascii_char(ch);  /* seven bit table */
		else if(term&UTF8) {
			enum unicode_codepoint codepoint = cp437_unicode_tbl[(uchar)ch];
			if(codepoint != 0)
				utf8_putc(utf8, sizeof(utf8) - 1, codepoint);
		}
	}

	if(ch==FF && lncntr > 0 && row > 0) {
		lncntr=0;
		newline();
		if(!(sys_status&SS_PAUSEOFF)) {
			pause();
			while(lncntr && online && !(sys_status&SS_ABORT))
				pause();
		}
	}

	if(!(console&CON_R_ECHO))
		return 0;

	if((console&CON_R_ECHOX) && (uchar)ch>=' ' && outchar_esc == ansiState_none) {
		ch=text[YNQP][3];
		if(text[YNQP][2]==0 || ch==0) ch='X';
	}
	if(ch==FF)
		clearscreen(term);
	else if(ch == '\t') {
		outcom(' ');
		column++;
		while(column%tabstop) {
			outcom(' ');
			column++;
		}
	}
	else {
		if(ch==(char)TELNET_IAC && !(telnet_mode&TELNET_MODE_OFF))
			outcom(TELNET_IAC);	/* Must escape Telnet IAC char (255) */
		if(ch == '\r' && (console&CON_CR_CLREOL))
			cleartoeol();
		if(term&PETSCII) {
			uchar pet = cp437_to_petscii(ch);
			if(pet == PETSCII_SOLID)
				outcom(PETSCII_REVERSE_ON);
			outcom(pet);
			if(pet == PETSCII_SOLID)
				outcom(PETSCII_REVERSE_OFF);
			if(ch == '\r' && (curatr&0xf0) != 0) // reverse video is disabled upon CR
				curatr >>= 4;
		} else {
			if(utf8[0] != 0)
				putcom(utf8);
			else
				outcom(ch);
		}
	}
	if(outchar_esc == ansiState_none) {
		/* Track cursor position locally */
		switch(ch) {
			case '\a':	// 7
			case '\t':	// 9
				/* Non-printing or handled elsewhere */
				break;
			case '\b':	// 8
				if(column > 0)
					column--;
				if(lbuflen<LINE_BUFSIZE) {
					if(lbuflen == 0)
						latr = curatr;
					lbuf[lbuflen++] = ch;
				}
				break;
			case '\n':	// 10
				inc_row(1);
				if(lncntr || lastlinelen)
					lncntr++;
				lbuflen=0;
				break;
			case FF:	// 12
				lncntr=0;
				lbuflen=0;
				row=0;
				column=0;
			case '\r':	// 13
				lastlinelen = column;
				column=0;
				break;
			default:
				inc_column(1);
				if(!lbuflen)
					latr=curatr;
				if(lbuflen<LINE_BUFSIZE)
					lbuf[lbuflen++]=ch;
				break;
		}
	}
	if(outchar_esc == ansiState_final)
		outchar_esc = ansiState_none;

	if(lncntr==rows-1 && ((useron.misc&(UPAUSE^(console&CON_PAUSEOFF))) || sys_status&SS_PAUSEON)
		&& !(sys_status&(SS_PAUSEOFF|SS_ABORT))) {
		lncntr=0;
		pause();
	}
	return 0;
}

int sbbs_t::outchar(enum unicode_codepoint codepoint, const char* cp437_fallback)
{
	if(term_supports(UTF8)) {
		char str[UTF8_MAX_LEN];
		int len = utf8_putc(str, sizeof(str), codepoint);
		if(len < 1)
			return len;
		putcom(str, len);
		inc_column(unicode_width(codepoint));
		return 0;
	}
	if(cp437_fallback == NULL)
		return 0;
	return bputs(cp437_fallback);
}

int sbbs_t::outchar(enum unicode_codepoint codepoint, char cp437_fallback)
{
	char str[2] = { cp437_fallback, '\0' };
	return outchar(codepoint, str);
}

void sbbs_t::inc_column(int count)
{
	column += count;
	if(column >= cols) {	// assume terminal has/will auto-line-wrap
		lncntr++;
		lbuflen = 0;
		lastlinelen = column;
		column = 0;
		inc_row(1);
	}
}

void sbbs_t::inc_row(int count)
{
	row += count;
	if(row >= rows) {
		scroll_hotspots((row - rows) + 1);
		row = rows - 1;
	}
}

void sbbs_t::center(const char *instr, unsigned int columns)
{
	char str[256];
	size_t len;

	if(columns < 1)
		columns = cols;

	SAFECOPY(str,instr);
	truncsp(str);
	len = bstrlen(str);
	carriage_return();
	if(len < columns)
		cursor_right((columns - len) / 2);
	bputs(str);
	newline();
}

void sbbs_t::wide(const char* str)
{
	long term = term_supports();
	while(*str != '\0') {
		if((term&UTF8) && *str >= '!' && *str <= '~')
			outchar((enum unicode_codepoint)(UNICODE_FULLWIDTH_EXCLAMATION_MARK + (*str - '!')));
		else {
			outchar(*str);
			outchar(' ');
		}
		str++;
	}
}


// Send a bare carriage return, hopefully moving the cursor to the far left, current row
void sbbs_t::carriage_return(int count)
{
	if(count < 1)
		return;
	for(int i = 0; i < count; i++) {
		if(term_supports(PETSCII))
			cursor_left(column);
		else
			outcom('\r');
		column = 0;
	}
}

// Send a bare line_feed, hopefully moving the cursor down one row, current column
void sbbs_t::line_feed(int count)
{
	if(count < 1)
		return;
	for(int i = 0; i < count; i++) {
		if(term_supports(PETSCII))
			outcom(PETSCII_DOWN);
		else 
			outcom('\n');
	}
	inc_row(count);
}

void sbbs_t::newline(int count)
{
	if(count < 1)
		return;
	for(int i = 0; i < count; i++) { 
		outchar('\r');
		outchar('\n');
	}
}

void sbbs_t::clearscreen(long term)
{
	clear_hotspots();
	if(term&ANSI)
		putcom("\x1b[2J\x1b[H");	/* clear screen, home cursor */
	else if(term&PETSCII)
		outcom(PETSCII_CLEAR);
	else
		outcom(FF);
	row=0;
	column=0;
	lncntr=0;
}

void sbbs_t::clearline(void)
{
	carriage_return();
	cleartoeol();
}

void sbbs_t::cursor_home(void)
{
	long term = term_supports();
	if(term&ANSI)
		putcom("\x1b[H");
	else if(term&PETSCII)
		outcom(PETSCII_HOME);
	else
		outchar(FF);	/* this will clear some terminals, do nothing with others */
	row=0;
	column=0;
}

void sbbs_t::cursor_up(int count)
{
	if(count<1)
		return;
	long term = term_supports();
	if(term&ANSI) {
		if(count>1)
			comprintf("\x1b[%dA",count);
		else
			putcom("\x1b[A");
	} else {
		if(term&PETSCII) {
			for(int i=0;i<count;i++)
				outcom(PETSCII_UP);
		}
	}
}

void sbbs_t::cursor_down(int count)
{
	if(count<1)
		return;
	if(term_supports(ANSI)) {
		if(count>1)
			comprintf("\x1b[%dB",count);
		else
			putcom("\x1b[B");
		inc_row(count);
	} else {
		for(int i=0;i<count;i++)
			line_feed();
	}
}

void sbbs_t::cursor_right(int count)
{
	if(count<1)
		return;
	long term = term_supports();
	if(term&ANSI) {
		if(count>1)
			comprintf("\x1b[%dC",count);
		else
			putcom("\x1b[C");
	} else {
		for(int i=0;i<count;i++) {
			if(term&PETSCII)
				outcom(PETSCII_RIGHT);
			else
				outcom(' ');
		}
	}
	column+=count;
}

void sbbs_t::cursor_left(int count)
{
	if(count<1)
		return;
	long term = term_supports();
	if(term&ANSI) {
		if(count>1)
			comprintf("\x1b[%dD",count);
		else
			putcom("\x1b[D");
	} else {
		for(int i=0;i<count;i++) {
			if(term&PETSCII)
				outcom(PETSCII_LEFT);
			else
				outcom('\b');
		}
	}
	if(column > count)
		column-=count;
	else
		column=0;
}

bool sbbs_t::cursor_xy(int x, int y)
{
	long term = term_supports();
	if(term&ANSI)
		return ansi_gotoxy(x, y);
	if(term&PETSCII) {
		outcom(PETSCII_HOME);
		cursor_down(y - 1);
		cursor_right(x - 1);
		return true;
	}
	return false;
}

bool sbbs_t::cursor_getxy(int* x, int* y)
{
	if(term_supports(ANSI))
		return ansi_getxy(x, y);
	*x = column + 1;
	*y = row + 1;
	return true;
}

void sbbs_t::cleartoeol(void)
{
	int i,j;

	long term = term_supports();
	if(term&ANSI)
		putcom("\x1b[K");
	else {
		i=j=column;
		while(++i<cols)
			outcom(' ');
		while(++j<cols) {
			if(term&PETSCII)
				outcom(PETSCII_LEFT);
			else
				outcom('\b');
		}
	}
}

void sbbs_t::cleartoeos(void)
{
	if(term_supports(ANSI))
		putcom("\x1b[J");
}

void sbbs_t::set_output_rate(enum output_rate speed)
{
	if(term_supports(ANSI)) {
		unsigned int val = speed;
		switch(val) {
			case 0:		val = 0; break;
			case 600:	val = 2; break;
			case 1200:	val = 3; break;
			case 2400:	val = 4; break;
			case 4800:	val = 5; break;
			case 9600:	val = 6; break;
			case 19200:	val = 7; break;
			case 38400: val = 8; break;
			case 57600: val = 9; break;
			case 76800: val = 10; break;
			default:
				if(val <= 300)
					val = 1;
				else if(val > 76800)
					val = 11;
				break;
		}
		comprintf("\x1b[;%u*r", val);
		cur_output_rate = speed;
	}
}

/****************************************************************************/
/* performs the correct attribute modifications for the Ctrl-A code			*/
/****************************************************************************/
void sbbs_t::ctrl_a(char x)
{
	char	tmp1[128];
	uint	atr = curatr;
	struct	tm tm;

	if(x && (uchar)x<=CTRL_Z) {    /* Ctrl-A through Ctrl-Z for users with MF only */
		if(!(useron.flags1&FLAG(x+64)))
			console^=(CON_ECHO_OFF);
		return;
	}
	switch(toupper(x)) {
		case '!':   /* level 10 or higher */
			if(useron.level<10)
				console^=CON_ECHO_OFF;
			break;
		case '@':   /* level 20 or higher */
			if(useron.level<20)
				console^=CON_ECHO_OFF;
			break;
		case '#':   /* level 30 or higher */
			if(useron.level<30)
				console^=CON_ECHO_OFF;
			break;
		case '$':   /* level 40 or higher */
			if(useron.level<40)
				console^=CON_ECHO_OFF;
			break;
		case '%':   /* level 50 or higher */
			if(useron.level<50)
				console^=CON_ECHO_OFF;
			break;
		case '^':   /* level 60 or higher */
			if(useron.level<60)
				console^=CON_ECHO_OFF;
			break;
		case '&':   /* level 70 or higher */
			if(useron.level<70)
				console^=CON_ECHO_OFF;
			break;
		case '*':   /* level 80 or higher */
			if(useron.level<80)
				console^=CON_ECHO_OFF;
			break;
		case '(':   /* level 90 or higher */
			if(useron.level<90)
				console^=CON_ECHO_OFF;
			break;
		case ')':   /* turn echo back on */
			console&=~CON_ECHO_OFF;
			break;
	}
	if(console & CON_ECHO_OFF)
		return;

	if((uchar)x>0x7f) {
		cursor_right((uchar)x-0x7f);
		return;
	}
	if(IS_DIGIT(x)) {	/* background color */
		atr &= (BG_BRIGHT|BLINK|0x0f);
	}
	switch(toupper(x)) {
		case '+':	/* push current attribute */
			if(attr_sp<(int)(sizeof(attr_stack) / sizeof(attr_stack[0])))
				attr_stack[attr_sp++]=curatr;
			break;
		case '-':	/* pop current attribute OR optimized "normal" */
			if(attr_sp>0)
				attr(attr_stack[--attr_sp]);
			else									/* turn off all attributes if */
				if(atr&(HIGH|BLINK|BG_LIGHTGRAY))	/* high intensity, blink or */
					attr(LIGHTGRAY);				/* background bits are set */
			break;
		case '_':								/* turn off all attributes if */
			if(atr&(BLINK|BG_LIGHTGRAY))		/* blink or background is set */
				attr(LIGHTGRAY);
			break;
		case 'P':	/* Pause */
			pause();
			break;
		case 'Q':   /* Pause reset */
			lncntr=0;
			break;
		case 'T':   /* Time */
			now=time(NULL);
			localtime_r(&now,&tm);
			if(cfg.sys_misc&SM_MILITARY)
				bprintf("%02u:%02u:%02u"
					,tm.tm_hour, tm.tm_min, tm.tm_sec);
			else
				bprintf("%02d:%02d %s"
					,tm.tm_hour==0 ? 12
					: tm.tm_hour>12 ? tm.tm_hour-12
					: tm.tm_hour, tm.tm_min, tm.tm_hour>11 ? "pm":"am");
			break;
		case 'D':   /* Date */
			now=time(NULL);
			bputs(unixtodstr(&cfg,(time32_t)now,tmp1));
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
			ASYNC;
			break;
		case 'J':	/* clear to end-of-screen */
			cleartoeos();
			break;
		case 'L':	/* CLS (form feed) */
			CLS;
			break;
		case '\'':	/* Home cursor */
		case '`':	// usurped by strict hot-spot
			cursor_home();
			break;
		case '>':   /* CLREOL */
			cleartoeol();
			break;
		case '<':   /* Non-destructive backspace */
			cursor_left();
			break;
		case '/':	/* Conditional new-line */
			cond_newline();
			break;
		case '\\':	/* Conditional New-line / Continuation prefix (if cols < 80) */
			cond_contline();
			break;
		case '?':	/* Conditional blank-line */
			cond_blankline();
			break;
		case '[':   /* Carriage return */
			carriage_return();
			break;
		case ']':   /* Line feed */
			line_feed();
			break;
		case 'A':   /* Ctrl-A */
			outchar(CTRL_A);
			break;
		case 'Z':	/* Ctrl-Z */
			outchar(CTRL_Z);
			break;
		case 'H': 	/* High intensity */
			atr|=HIGH;
			attr(atr);
			break;
		case 'I':
 			if((term_supports()&(ICE_COLOR|PETSCII)) != ICE_COLOR)
				attr(atr|BLINK);
			break;
		case 'E': /* Bright Background */
 			if(term_supports()&(ICE_COLOR|PETSCII))
				attr(atr|BG_BRIGHT);
			break;
		case 'F':	/* Blink, only if alt Blink Font is loaded */
			if(((atr&HIGH) && (console&CON_HBLINK_FONT)) || (!(atr&HIGH) && (console&CON_BLINK_FONT)))
				attr(atr|BLINK);
			else if(x == 'F' && !(atr&HIGH))	/* otherwise, set HIGH attribute (only if capital 'F') */
				attr(atr|HIGH);
			break;
		case 'N': 	/* Normal */
			attr(LIGHTGRAY);
			break;
		case 'R':
			atr=(atr&0xf8)|RED;
			attr(atr);
			break;
		case 'G':
			atr=(atr&0xf8)|GREEN;
			attr(atr);
			break;
		case 'B':
			atr=(atr&0xf8)|BLUE;
			attr(atr);
			break;
		case 'W':	/* White */
			atr=(atr&0xf8)|LIGHTGRAY;
			attr(atr);
			break;
		case 'C':
			atr=(atr&0xf8)|CYAN;
			attr(atr);
			break;
		case 'M':
			atr=(atr&0xf8)|MAGENTA;
			attr(atr);
			break;
		case 'Y':   /* Yellow */
			atr=(atr&0xf8)|BROWN;
			attr(atr);
			break;
		case 'K':	/* Black */
			atr=(atr&0xf8)|BLACK;
			attr(atr);
			break;
		case '0':	/* Black Background */
			attr(atr);
			break;
		case '1':	/* Red Background */
			attr(atr | BG_RED);
			break;
		case '2':	/* Green Background */
			attr(atr | BG_GREEN);
			break;
		case '3':	/* Yellow Background */
			attr(atr | BG_BROWN);
			break;
		case '4':	/* Blue Background */
			attr(atr | BG_BLUE);
			break;
		case '5':	/* Magenta Background */
			attr(atr |BG_MAGENTA);
			break;
		case '6':	/* Cyan Background */
			attr(atr | BG_CYAN);
			break;
		case '7':	/* White Background */
			attr(atr | BG_LIGHTGRAY);
			break;
	}
}

/****************************************************************************/
/* Sends terminal control codes to change remote terminal colors/attributes */
/****************************************************************************/
int sbbs_t::attr(int atr)
{
	char	str[16];
	int		newatr = atr;

	long term = term_supports();
	if(term&PETSCII) {
		if(atr&(0x70|BG_BRIGHT)) {	// background color (reverse video for PETSCII)
			if(atr&BG_BRIGHT)
				atr |= HIGH;
			else
				atr &= ~HIGH;
			atr = (atr&(BLINK|HIGH)) | ((atr&0x70)>>4);
			outcom(PETSCII_REVERSE_ON);
		} else
			outcom(PETSCII_REVERSE_OFF);
		if(atr&BLINK)
			outcom(PETSCII_FLASH_ON);
		else
			outcom(PETSCII_FLASH_OFF);
		switch(atr&0x0f) {
			case BLACK:
				outcom(PETSCII_BLACK);
				break;
			case WHITE:
				outcom(PETSCII_WHITE);
				break;
			case DARKGRAY:
				outcom(PETSCII_DARKGRAY);
				break;
			case LIGHTGRAY:
				outcom(PETSCII_LIGHTGRAY);
				break;
			case BLUE:
				outcom(PETSCII_BLUE);
				break;
			case LIGHTBLUE:
				outcom(PETSCII_LIGHTBLUE);
				break;
			case CYAN:
				outcom(PETSCII_MEDIUMGRAY);
				break;
			case LIGHTCYAN:
				outcom(PETSCII_CYAN);
				break;
			case YELLOW:
				outcom(PETSCII_YELLOW);
				break;
			case BROWN:
				outcom(PETSCII_BROWN);
				break;
			case RED:
				outcom(PETSCII_RED);
				break;
			case LIGHTRED:
				outcom(PETSCII_LIGHTRED);
				break;
			case GREEN:
				outcom(PETSCII_GREEN);
				break;
			case LIGHTGREEN:
				outcom(PETSCII_LIGHTGREEN);
				break;
			case MAGENTA:
				outcom(PETSCII_ORANGE);
				break;
			case LIGHTMAGENTA:
				outcom(PETSCII_PURPLE);
				break;
		}
	}
	else if(term&ANSI)
		rputs(ansi(newatr,curatr,str));
	curatr=newatr;
	return 0;
}

/****************************************************************************/
/* Checks to see if user has hit abort (Ctrl-C). Returns 1 if user aborted. */
/* When 'clear' is false (the default), preserves SS_ABORT flag state.		*/
/****************************************************************************/
bool sbbs_t::msgabort(bool clear)
{
	static ulong counter;

	if(sys_status&SS_SYSPAGE && !(++counter%100))
		sbbs_beep(400 + sbbs_random(800), 10);

	if(sys_status&SS_ABORT) {
		if(clear)
			sys_status &= ~SS_ABORT;
		return(true);
	}
	if(!online)
		return(true);
	return(false);
}

int sbbs_t::backfill(const char* instr, float pct, int full_attr, int empty_attr)
{
	uint atr;
	uint save_atr = curatr;
	int len;
	char* str = strip_ctrl(instr, NULL);

	len = strlen(str);
	if(!(term_supports()&(ANSI|PETSCII)))
		bputs(str);
	else {
		for(int i=0; i<len; i++) {
			if(((float)(i+1) / len)*100.0 <= pct)
				atr = full_attr;
			else
				atr = empty_attr;
			if(curatr != atr) attr(atr);
			outchar(str[i]);
		}
		attr(save_atr);
	}
	free(str);
	return len;
}

void sbbs_t::progress(const char* text, int count, int total, int interval)
{
	char str[128];

	if(cfg.node_num == 0)
		return;	// Don't output this for events
	clock_t now = msclock();
	if(now - last_progress < interval)
		return;
	if(text == NULL) text = "";
	float pct = total ? ((float)count/total)*100.0F : 100.0F;
	SAFEPRINTF2(str, "[ %-8s  %4.1f%% ]", text, pct);
	cond_newline();
	cursor_left(backfill(str, pct, cfg.color[clr_progress_full], cfg.color[clr_progress_empty]));
	last_progress = now;
}

struct savedline {
	char 	buf[LINE_BUFSIZE+1];	/* Line buffer (i.e. ANSI-encoded) */
	uint	beg_attr;				/* Starting attribute of each line */
	uint	end_attr;				/* Ending attribute of each line */
	long	column;					/* Current column number */
};

bool sbbs_t::saveline(void)
{
	struct savedline line;
#ifdef _DEBUG
	lprintf(LOG_DEBUG, "Saving %d chars, cursor at col %ld: '%.*s'", lbuflen, column, lbuflen, lbuf);
#endif
	line.beg_attr = latr;
	line.end_attr = curatr;
	line.column = column;
	snprintf(line.buf, sizeof(line.buf), "%.*s", lbuflen, lbuf);
	TERMINATE(line.buf);
	lbuflen=0;
	return listPushNodeData(&savedlines, &line, sizeof(line)) != NULL;
}

bool sbbs_t::restoreline(void)
{
	struct savedline* line = (struct savedline*)listPopNode(&savedlines);
	if(line == NULL)
		return false;
#ifdef _DEBUG
	lprintf(LOG_DEBUG, "Restoring %d chars, cursor at col %ld: '%s'", (int)strlen(line->buf), line->column, line->buf);
#endif
	lbuflen=0;
	attr(line->beg_attr);
	rputs(line->buf);
	curatr = line->end_attr;
	carriage_return();
	cursor_right(line->column);
	free(line);
	insert_indicator();
	return true;
}
