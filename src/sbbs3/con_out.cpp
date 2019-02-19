/* Synchronet console output routines */
// vi: tabstop=4

/* $Id$ */

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
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/


/**********************************************************************/
/* Functions that pertain to console i/o - color, strings, chars etc. */
/* Called from functions everywhere                                   */
/**********************************************************************/

#include "sbbs.h"

/****************************************************************************/
/* Outputs a NULL terminated string locally and remotely (if applicable)    */
/* Handles ctrl-a codes, Telnet-escaping, column & line count, auto-pausing */
/****************************************************************************/
int sbbs_t::bputs(const char *str)
{
	int i;
    ulong l=0;

	if(online==ON_LOCAL && console&CON_L_ECHO) 	/* script running as event */
		return(lputs(LOG_INFO, str));

	while(str[l] && online) {
		if(str[l]==CTRL_A && str[l+1]!=0) {
			l++;
			if(str[l] == 'Z')	/* EOF (uppercase 'Z' only) */
				break;
			ctrl_a(str[l++]);
			continue;
		}
		if(str[l]=='@') {           /* '@' */
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
		outchar(str[l++]);
	}
	return(l);
}

/* Perform PETSCII terminal output translation (from ASCII/CP437) */
unsigned char cp437_to_petscii(unsigned char ch)
{
	if(isalpha(ch))
		return ch ^ 0x20;	/* swap upper/lower case */
	switch(ch) {
		case '\1':		return '@';
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
	if(isalpha(ch))
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


/****************************************************************************/
/* Raw put string (remotely)												*/
/* Performs Telnet IAC escaping												*/
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
	for(l=0;l<len && online;l++) {
		if(str[l]==(char)TELNET_IAC && !(telnet_mode&TELNET_MODE_OFF))
			outcom(TELNET_IAC);	/* Must escape Telnet IAC char (255) */
		uchar ch = str[l];
		if(term&PETSCII)
			ch = cp437_to_petscii(ch);
		if(outcom(ch)!=0)
			break;
		if(lbuflen<LINE_BUFSIZE)
			lbuf[lbuflen++] = ch;
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
/* Outputs destructive backspace 											*/
/****************************************************************************/
void sbbs_t::backspace(void)
{
	if(!(console&CON_ECHO_OFF)) {
		if(term_supports(PETSCII))
			outcom(PETSCII_DELETE);
		else {
			outcom('\b');
			outcom(' ');
			outcom('\b');
		}
		if(column)
			column--;
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

	return(cmp_flags ? ((flags&cmp_flags)==cmp_flags) : (flags&TERM_FLAGS));
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
	/*
	 * outchar_esc values:
	 * 0: No sequence
	 * 1: ESC
	 * 2: CSI
	 * 3: Final byte
     * 4: APS, DCS, PM, or OSC
     * 5: SOS
     * 6: ESC inside of SOS
     */

	if(console&CON_ECHO_OFF)
		return 0;
	if(ch==ESC && outchar_esc < 4)
		outchar_esc=1;
	else if(outchar_esc==1) {
		if(ch=='[')
			outchar_esc++;
		else if(ch=='_' || ch=='P' || ch == '^' || ch == ']')
			outchar_esc=4;
		else if(ch=='X')
			outchar_esc=5;
		else if(ch >= 0x40 && ch <= 0x5f)
			outchar_esc=3;
		else
			outchar_esc=0;
	}
	else if(outchar_esc==2) {
		if(ch>='@' && ch<='~')
			outchar_esc++;
	}
	else if(outchar_esc==4) {	// APS, DCS, PM, or OSC
		if (ch == ESC)
			outchar_esc = 1;
		if (!((ch >= 0x08 && ch <= 0x0d) || (ch >= 0x20 && ch <= 0x7e)))
			outchar_esc = 0;
	}
	else if(outchar_esc==5) {	// SOS
		if (ch == ESC)
			outchar_esc++;
	}
	else if(outchar_esc==6) {	// ESC inside SOS
		if (ch == '\\')
			outchar_esc = 1;
		else if (ch == 'X')
			outchar_esc = 0;
		else
			outchar_esc = 5;
	}
	else
		outchar_esc=0;
	long term = term_supports();
	if((term&(PETSCII|NO_EXASCII)) == NO_EXASCII && ch&0x80)
		ch=exascii_to_ascii_char(ch);  /* seven bit table */

	if(ch==FF && lncntr > 0 && !tos) {
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

	if((console&CON_R_ECHOX) && (uchar)ch>=' ' && !outchar_esc) {
		ch=text[YNQP][3];
		if(text[YNQP][2]==0 || ch==0) ch='X';
	}
	if(ch==FF) {
		if(term&ANSI)
			putcom("\x1b[2J\x1b[H");	/* clear screen, home cursor */
		else if(term&PETSCII)
			outcom(PETSCII_CLEAR);
		else
			outcom(FF);
	}
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
		if(term&PETSCII) {
			uchar pet = cp437_to_petscii(ch);
			if(pet == PETSCII_SOLID)
				outcom(PETSCII_REVERSE_ON);
			outcom(pet);
			if(pet == PETSCII_SOLID)
				outcom(PETSCII_REVERSE_OFF);
		} else
			outcom(ch);
	}
	if(!outchar_esc) {
		/* Track cursor position locally */
		switch(ch) {
			case '\a':	// 7
			case '\t':	// 9
				/* Non-printing or handled elsewhere */
				break;
			case '\b':	// 8
				if(column > 0)
					column--;
				if(lbuflen > 0)
					lbuflen--;
				break;
			case '\n':	// 10
				if(lncntr || lastlinelen)
					lncntr++;
				lbuflen=0;
				tos=0;
				column=0;
				break;
			case FF:	// 12
				lncntr=0;
				lbuflen=0;
				tos=1;
				column=0;
			case '\r':	// 13
				lastlinelen = column;
				column=0;
				break;
			default:
				column++;
				if(column >= cols) {	// assume terminal has/will auto-line-wrap
					lncntr++;
					lbuflen = 0;
					tos = 0;
					lastlinelen = column;
					column = 0;
				}
				if(!lbuflen)
					latr=curatr;
				if(lbuflen<LINE_BUFSIZE)
					lbuf[lbuflen++]=ch;
				break;
		}
	}
	if(outchar_esc==3)
		outchar_esc=0;

	if(lncntr==rows-1 && ((useron.misc&UPAUSE) || sys_status&SS_PAUSEON)
		&& !(sys_status&(SS_PAUSEOFF|SS_ABORT))) {
		lncntr=0;
		pause();
	}
	return 0;
}

void sbbs_t::center(char *instr)
{
	char str[256];
	int i,j;

	SAFECOPY(str,instr);
	truncsp(str);
	j=bstrlen(str);
	if(j < cols)
		for(i=0;i<(cols-j)/2;i++)
			outchar(' ');
	bputs(str);
	newline();
}

// Send a bare carriage return, hopefully moving the cursor to the far left, current row
void sbbs_t::carriage_return(void)
{
	if(term_supports(PETSCII))
		cursor_left(column);
	else
		outcom('\r');
	column = 0;
}

// Send a bare line_feed, hopefully moving the cursor down one row, current column
void sbbs_t::line_feed(void)
{
	if(term_supports(PETSCII))
		outcom(PETSCII_DOWN);
	else 
		outcom('\n');
}

void sbbs_t::newline(void)
{
	outchar('\r');
	outchar('\n');
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
		rputs("\x1b[H");
	else if(term&PETSCII)
		outcom(PETSCII_HOME);
	else
		outchar(FF);	/* this will clear some terminals, do nothing with others */
	tos=1;
	column=0;
}

void sbbs_t::cursor_up(int count)
{
	if(count<1)
		return;
	long term = term_supports();
	if(term&ANSI) {
		if(count>1)
			rprintf("\x1b[%dA",count);
		else
			rputs("\x1b[A");
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
			rprintf("\x1b[%dB",count);
		else
			rputs("\x1b[B");
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
			rprintf("\x1b[%dC",count);
		else
			rputs("\x1b[C");
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
			rprintf("\x1b[%dD",count);
		else
			rputs("\x1b[D");
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

void sbbs_t::cleartoeol(void)
{
	int i,j;

	long term = term_supports();
	if(term&ANSI)
		rputs("\x1b[K");
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
		rputs("\x1b[J");
}

/****************************************************************************/
/* performs the correct attribute modifications for the Ctrl-A code			*/
/****************************************************************************/
void sbbs_t::ctrl_a(char x)
{
	char	tmp1[128],atr=curatr;
	struct	tm tm;

	if(x && (uchar)x<=CTRL_Z) {    /* Ctrl-A through Ctrl-Z for users with MF only */
		if(!(useron.flags1&FLAG(x+64)))
			console^=(CON_ECHO_OFF);
		return;
	}
	if((uchar)x>0x7f) {
		cursor_right((uchar)x-0x7f);
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
		case '+':	/* push current attribute */
			if(attr_sp<(int)sizeof(attr_stack))
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
		case '`':	/* Home cursor */
			cursor_home();
			break;
		case '>':   /* CLREOL */
			cleartoeol();
			break;
		case '<':   /* Non-destructive backspace */
			cursor_left();
			break;
		case '/':	/* Conditional new-line */
			if(column > 0)
				newline();
			break;
		case '\\':	/* Conditional New-line / Continuation prefix (if cols < 80) */
			if(column > 0 && cols < TERM_COLS_DEFAULT)
				bputs(text[LongLineContinuationPrefix]);
			break;
		case '?':	/* Conditional blank-line */
			if(column > 0)
				newline();
			if(lastlinelen)
				newline();
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
		case 'I':	/* Blink */
			atr|=BLINK;
			attr(atr);
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
			atr=(atr&0x8f);
			attr(atr);
			break;
		case '1':	/* Red Background */
			atr=(atr&0x8f)|(uchar)BG_RED;
			attr(atr);
			break;
		case '2':	/* Green Background */
			atr=(atr&0x8f)|(uchar)BG_GREEN;
			attr(atr);
			break;
		case '3':	/* Yellow Background */
			atr=(atr&0x8f)|(uchar)BG_BROWN;
			attr(atr);
			break;
		case '4':	/* Blue Background */
			atr=(atr&0x8f)|(uchar)BG_BLUE;
			attr(atr);
			break;
		case '5':	/* Magenta Background */
			atr=(atr&0x8f)|(uchar)BG_MAGENTA;
			attr(atr);
			break;
		case '6':	/* Cyan Background */
			atr=(atr&0x8f)|(uchar)BG_CYAN;
			attr(atr);
			break;
		case '7':	/* White Background */
			atr=(atr&0x8f)|(uchar)BG_LIGHTGRAY;
			attr(atr);
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
		if(atr&0x70) {
			atr >>= 4;
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
/* Checks to see if user has hit Pause or Abort. Returns 1 if user aborted. */
/* If the user hit Pause, waits for a key to be hit.                        */
/* Emulates remote XON/XOFF flow control on local console                   */
/* Preserves SS_ABORT flag state, if already set.                           */
/* Called from various listing procedures that wish to check for abort      */
/****************************************************************************/
bool sbbs_t::msgabort()
{
	static ulong counter;

	if(sys_status&SS_SYSPAGE && !(++counter%100))
		sbbs_beep(sbbs_random(800),1);

	checkline();
	if(sys_status&SS_ABORT)
		return(true);
	if(!online)
		return(true);
	return(false);
}

int sbbs_t::backfill(const char* instr, float pct, int full_attr, int empty_attr)
{
	int	atr;
	int save_atr = curatr;
	int len;
	char* str = strip_ctrl(instr, NULL);

	len = strlen(str);
	if(!term_supports(ANSI))
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
	if((count%interval) != 0)
		return;
	if(text == NULL) text = "";
	float pct = ((float)count/total)*100.0F;
	SAFEPRINTF2(str, "[ %-8s  %4.1f%% ]", text, pct);
	cursor_left(backfill(str, pct, cfg.color[clr_progress_full], cfg.color[clr_progress_empty]));
}

struct savedline {
	char 	buf[LINE_BUFSIZE+1];	/* Line buffer (i.e. ANSI-encoded) */
	char 	beg_attr;				/* Starting attribute of each line */
	char 	end_attr;				/* Ending attribute of each line */
	long	column;					/* Current column number */
};

bool sbbs_t::saveline(void)
{
	struct savedline line;
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
	lbuflen=0;
	attr(line->beg_attr);
	rputs(line->buf);
	curatr = line->end_attr;
	column = line->column;
	free(line);
	return true;
}
