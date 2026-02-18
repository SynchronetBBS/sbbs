/* Synchronet single-key console functions */

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
#include "telnet.h" // TELNET_GA

/****************************************************************************/
/* Waits for remote or local user to hit a key. Inactivity timer is checked */
/* and hangs up if inactive for 4 minutes. Returns key hit, or uppercase of */
/* key hit if mode&K_UPPER or key out of KEY BUFFER. Does not print key.    */
/* Called from functions all over the place.                                */
/****************************************************************************/
char sbbs_t::getkey(int mode)
{
	uchar  ch, coldkey;
	uint   c = sbbs_random(5);
	time_t last_telnet_cmd = 0;
	char*  cursor = text[SpinningCursor0  + sbbs_random(10)];
	size_t cursors = strlen(cursor);

	if (online == ON_REMOTE && !input_thread_running)
		online = FALSE;
	if (!online) {
		YIELD();    // just in case someone is looping on getkey() when they shouldn't
		return 0;
	}
	clearabort();
	if ((sys_status & SS_USERON || action == NODE_DFLT) && !(mode & (K_GETSTR | K_NOSPIN)))
		mode |= (useron.misc & SPIN);
	term->lncntr = 0;
	getkey_last_activity = time(NULL);
#if !defined SPINNING_CURSOR_OVER_HARDWARE_CURSOR
	if (mode & K_SPIN)
		outchar(' ');
#endif
	do {
		if (sys_status & SS_ABORT) {
			if (mode & K_SPIN) {
#if defined SPINNING_CURSOR_OVER_HARDWARE_CURSOR
				bputs(" \b");
#else
				term->backspace();
#endif
			}
			return 0;
		}

		if (mode & K_SPIN) {
#if !defined SPINNING_CURSOR_OVER_HARDWARE_CURSOR
			outchar('\b');
#endif
			outchar(cursor[(c++) % cursors]);
#if defined SPINNING_CURSOR_OVER_HARDWARE_CURSOR
			outchar('\b');
#endif
		}
		ch = inkey(mode, mode & K_SPIN ? 200:1000);
		if (sys_status & SS_ABORT)
			return 0;
		now = time(NULL);
		if (ch) {
			if (mode & K_NUMBER && IS_PRINTABLE(ch) && !IS_DIGIT(ch))
				continue;
			if (mode & K_ALPHA && IS_PRINTABLE(ch) && !IS_ALPHA(ch))
				continue;
			if (mode & K_NOEXASC && ch & 0x80)
				continue;
			if (mode & K_SPIN) {
#if defined SPINNING_CURSOR_OVER_HARDWARE_CURSOR
				bputs(" \b");
#else
				term->backspace();
#endif
			}
			if (mode & K_COLD && ch > ' ' && useron.misc & COLDKEYS) {
				if (mode & K_UPPER)
					outchar(toupper(ch));
				else
					outchar(ch);
				while ((coldkey = inkey(mode, 1000)) == 0 && online && !(sys_status & SS_ABORT))
					;
				term->backspace();
				if (coldkey == BS || coldkey == DEL)
					continue;
				if (coldkey > ' ')
					ungetkey(coldkey);
			}
			if (mode & K_UPPER)
				return toupper(ch);
			return ch;
		}
		if (sys_status & SS_USERON && !(sys_status & SS_LCHAT))
			gettimeleft();
		else if (online && now - answertime > SEC_LOGON && !(sys_status & SS_LCHAT)) {
			console &= ~CON_PASSWORD;
			bputs(text[TakenTooLongToLogon]);
			hangup();
		}
		if (sys_status & SS_USERON && online && (timeleft / 60) < (5 - timeleft_warn)
		    && !useron_is_sysop() && !(sys_status & SS_LCHAT)) {
			timeleft_warn = 5 - (timeleft / 60);
			term->saveline();
			attr(LIGHTGRAY);
			bprintf(text[OnlyXminutesLeft]
			        , ((ushort)timeleft / 60) + 1, (timeleft / 60) ? "s" : nulstr);
			term->restoreline();
		}

		if (!(startup->options & BBS_OPT_NO_TELNET_GA)
		    && now != last_telnet_cmd && now - getkey_last_activity >= 60 && !((now - getkey_last_activity) % 60)) {
			// Let's make sure the socket is up
			// Sending will trigger a socket d/c detection
			send_telnet_cmd(TELNET_GA, 0);
			last_telnet_cmd = now;
		}

		time_t inactive = now - getkey_last_activity;
		if (online == ON_REMOTE && !(console & CON_NO_INACT)
		    && ((cfg.inactivity_warn && inactive >= cfg.max_getkey_inactivity * (cfg.inactivity_warn / 100.0))
		        || inactive >= cfg.max_getkey_inactivity)) {
			if ((sys_status & SS_USERON) && inactive < cfg.max_getkey_inactivity && *text[AreYouThere] != '\0') {
				term->saveline();
				bputs(text[AreYouThere]);
			}
			else
				bputs(text[InactivityAlert]);
			while (!inkey(K_NONE, 100) && online && now - getkey_last_activity < cfg.max_getkey_inactivity) {
				now = time(NULL);
			}
			if (now - getkey_last_activity >= cfg.max_getkey_inactivity) {
				if (online == ON_REMOTE) {
					console &= ~CON_PASSWORD;
				}
				bputs(text[CallBackWhenYoureThere]);
				logline(LOG_NOTICE, nulstr, "Maximum user input inactivity exceeded");
				hangup();
				return 0;
			}
			if ((sys_status & SS_USERON) && *text[AreYouThere] != '\0') {
				attr(LIGHTGRAY);
				term->carriage_return();
				term->cleartoeol();
				term->restoreline();
			}
			getkey_last_activity = now;
		}

	} while (online);

	return 0;
}


/****************************************************************************/
/* Outputs a string highlighting characters preceded by a tilde             */
/****************************************************************************/
void sbbs_t::mnemonics(const char *instr, int mode)
{
	size_t l;

	if (!strchr(instr, '~')) {
		mnestr = instr;
		bputs(instr, mode);
		return;
	}
	bool ctrl_a_codes = contains_ctrl_a_attr(instr);
	if (!ctrl_a_codes) {
		const char* last = lastchar(instr);
		if (instr[0] == '@' && *last == '@' && strchr(instr + 1, '@') == last && strchr(instr, ' ') == NULL) {
			mnestr = instr;
			bputs(instr, mode);
			return;
		}
	}

	mneattr_low = cfg.color[clr_mnelow];
	mneattr_high = cfg.color[clr_mnehigh];
	mneattr_cmd = cfg.color[clr_mnecmd];

	char str[256];
	expand_atcodes(instr, str, sizeof str);
	l = 0L;
	attr(mneattr_low);

	while (str[l]) {
		if (str[l] == '~' && str[l + 1] < ' ') {
			term->add_hotspot('\r', /* hungry: */ true);
			l += 2;
		}
		else if (str[l] == '~') {
			if (!(term->can_highlight()))
				outchar('(');
			l++;
			if (!ctrl_a_codes)
				attr(mneattr_high);
			term->add_hotspot(str[l], /* hungry: */ true);
			outchar(str[l]);
			l++;
			if (!(term->can_highlight()))
				outchar(')');
			if (!ctrl_a_codes)
				attr(mneattr_low);
		}
		else if (str[l] == '`' && str[l + 1] != 0) {
			if (!(term->can_highlight()))
				outchar('[');
			l++;
			if (!ctrl_a_codes)
				attr(mneattr_high);
			term->add_hotspot(str[l], /* hungry: */ false);
			outchar(str[l]);
			l++;
			if (!(term->can_highlight()))
				outchar(']');
			if (!ctrl_a_codes)
				attr(mneattr_low);
		}
		else {
			if (str[l] == CTRL_A && str[l + 1] != 0) {
				l++;
				if (str[l] == 'Z')   /* EOF (uppercase 'Z') */
					break;
				ctrl_a(str[l++]);
			} else {
				outchar(str[l++]);
			}
		}
	}
	if (!ctrl_a_codes)
		attr(mneattr_cmd);
}

/****************************************************************************/
/* Prompts user for Y or N (yes or no) and CR is interpreted as a Y         */
/* Returns true for Yes or false for No                                     */
/* Called from quite a few places                                           */
/****************************************************************************/
bool sbbs_t::yesno(const char *str, int mode)
{
	char ch;

	if (*str == 0)
		return true;
	SAFECOPY(question, str);
	sync();
	bprintf(mode, text[YesNoQuestion], str);
	while (online) {
		if (sys_status & SS_ABORT)
			ch = no_key();
		else
			ch = getkey(K_UPPER | K_COLD);
		if (ch == yes_key() || ch == CR) {
			if (bputs(text[Yes], mode) && !(mode & P_NOCRLF))
				term->newline();
			if (!(mode & P_SAVEATR))
				attr(LIGHTGRAY);
			term->lncntr = 0;
			return true;
		}
		if (ch == no_key()) {
			if (bputs(text[No], mode) && !(mode & P_NOCRLF))
				term->newline();
			if (!(mode & P_SAVEATR))
				attr(LIGHTGRAY);
			term->lncntr = 0;
			return false;
		}
	}
	return true;
}

/****************************************************************************/
/* Prompts user for N or Y (no or yes) and CR is interpreted as a N         */
/* Returns true for No or false for Yes                                     */
/****************************************************************************/
bool sbbs_t::noyes(const char *str, int mode)
{
	char ch;

	if (*str == 0)
		return true;
	SAFECOPY(question, str);
	sync();
	bprintf(mode, text[NoYesQuestion], str);
	while (online) {
		if (sys_status & SS_ABORT)
			ch = no_key();
		else
			ch = getkey(K_UPPER | K_COLD);
		if (ch == no_key() || ch == CR) {
			if (bputs(text[No], mode) && !(mode & P_NOCRLF))
				term->newline();
			if (!(mode & P_SAVEATR))
				attr(LIGHTGRAY);
			term->lncntr = 0;
			return true;
		}
		if (ch == yes_key()) {
			if (bputs(text[Yes], mode) && !(mode & P_NOCRLF))
				term->newline();
			if (!(mode & P_SAVEATR))
				attr(LIGHTGRAY);
			term->lncntr = 0;
			return false;
		}
	}
	return true;
}

/****************************************************************************/
/* Waits for remote or local user to hit a key among 'keys'.				*/
/* If 'keys' is NULL, *any* non-numeric key is valid input.					*/
/* 'max' is non-zero, allow that a decimal number input up to that size		*/
/* and return the value OR'd with 0x80000000.								*/
/* default mode value is K_UPPER											*/
/****************************************************************************/
int sbbs_t::getkeys(const char *keys, uint max, int mode)
{
	char  str[81]{};
	uchar ch, n = 0, c = 0;
	uint  i = 0;

	if (keys != NULL) {
		SAFECOPY(str, keys);
	}
	while (online) {
		ch = getkey(mode);
		if (max && ch > 0x7f)  /* extended ascii chars are digits to isdigit() */
			continue;
		if (sys_status & SS_ABORT) {   /* return -1 if Ctrl-C hit */
			if (!(mode & (K_NOECHO | K_NOCRLF))) {
				attr(LIGHTGRAY);
				term->newline();
			}
			term->lncntr = 0;
			return -1;
		}
		if (ch && !n && ((keys == NULL && !IS_DIGIT(ch)) || (strchr(str, ch)))) {  /* return character if in string */
			if (ch > ' ') {
				if (!(mode & K_NOECHO))
					outchar(ch);
				if (useron.misc & COLDKEYS) {
					while (online && !(sys_status & SS_ABORT)) {
						c = getkey(0);
						if (c == CR || c == BS || c == DEL)
							break;
					}
					if (sys_status & SS_ABORT) {
						if (!(mode & (K_NOECHO | K_NOCRLF))) {
							term->newline();
						}
						return -1;
					}
					if (c == BS || c == DEL) {
						if (!(mode & K_NOECHO))
							term->backspace();
						continue;
					}
				}
				if (!(mode & (K_NOECHO | K_NOCRLF))) {
					attr(LIGHTGRAY);
					term->newline();
				}
				term->lncntr = 0;
			}
			return ch;
		}
		if (ch == CR && max) {             /* return 0 if no number */
			if (!(mode & (K_NOECHO | K_NOCRLF))) {
				attr(LIGHTGRAY);
				term->newline();
			}
			term->lncntr = 0;
			if (n)
				return i | 0x80000000L;    /* return number plus high bit */
			return 0;
		}
		if ((ch == BS || ch == DEL) && n) {
			if (!(mode & K_NOECHO))
				term->backspace();
			i /= 10;
			n--;
		}
		else if (max && IS_DIGIT(ch) && (i * 10) + (ch & 0xf) <= max && (ch != '0' || n)) {
			i *= 10;
			n++;
			i += ch & 0xf;
			if (!(mode & K_NOECHO))
				outchar(ch);
			if (i * 10 > max && !(useron.misc & COLDKEYS) && keybuf_level() < 1) {
				if (!(mode & (K_NOECHO | K_NOCRLF))) {
					attr(LIGHTGRAY);
					term->newline();
				}
				term->lncntr = 0;
				return i | 0x80000000L;
			}
		}
	}
	return -1;
}

/****************************************************************************/
/* Prints PAUSE message and waits for a key stoke                           */
/* Returns false if aborted by user											*/
/****************************************************************************/
bool sbbs_t::pause(bool set_abort)
{
	char   ch;
	uint   tempattrs = term->curatr; /* was lclatr(-1) */
	int    l = K_UPPER;
	size_t len;

	if ((sys_status & SS_ABORT) || pause_inside)
		return false;
	pause_inside = true;
	term->lncntr = 0;
	if (online == ON_REMOTE)
		rioctl(IOFI);
	term->pause_hotspot = term->add_pause_hotspot('\r');
	bputs(text[Pause]);
	len = term->bstrlen(text[Pause]);
	if (sys_status & SS_USERON && !(useron.misc & (NOPAUSESPIN))
	    && cfg.spinning_pause_prompt)
		l |= K_SPIN;
	ch = getkey(l);
	if (term->pause_hotspot) {
		term->clear_hotspots();
		term->pause_hotspot = false;
	}
	bool aborted = (ch == no_key() || ch == quit_key() || (sys_status & SS_ABORT));
	if (set_abort && aborted)
		sys_status |= SS_ABORT;
	if (text[Pause][0] != '@')
		term->backspace(len);
	getnodedat(cfg.node_num, &thisnode);
	nodesync();
	attr(tempattrs);
	if (ch == TERM_KEY_DOWN) // down arrow == display one more line
		term->lncntr = term->rows - 2;
	pause_inside = false;
	return !aborted;
}

/****************************************************************************/
/* Puts a character into the input buffer                                   */
/****************************************************************************/
bool sbbs_t::ungetkey(char ch, bool insert)
{
	char dbg[2] = {};
#if 0   /* this way breaks ansi_getxy() */
	RingBufWrite(&inbuf, (uchar*)&ch, sizeof(uchar));
#else
	if (keybuf_space()) {
		char* p = c_escape_char(ch);
		if (p == NULL) {
			dbg[0] = ch;
			p = dbg;
		}
		lprintf(LOG_DEBUG, "%s key into keybuf: %02X (%s)", insert ? "insert" : "append", ch, p);
		if (insert) {
			if (keybufbot == 0)
				keybufbot = KEY_BUFSIZE - 1;
			else
				keybufbot--;
			keybuf[keybufbot] = ch;
		} else {
			keybuf[keybuftop++] = ch;
			if (keybuftop == KEY_BUFSIZE)
				keybuftop = 0;
		}
		return true;
	}
	lprintf(LOG_WARNING, "No space in keyboard input buffer");
	return false;
#endif
}

/****************************************************************************/
/* Puts a string into the input buffer										*/
/****************************************************************************/
bool sbbs_t::ungetkeys(const char* str, bool insert)
{
	size_t i;

	for (i = 0; str[i] != '\0'; i++) {
		if (!ungetkey(str[i], insert))
			return false;
	}
	return true;
}

size_t sbbs_t::keybuf_space(void)
{
	return sizeof(keybuf) - (keybuf_level() + 1);
}

size_t sbbs_t::keybuf_level(void)
{
	if (keybufbot > keybuftop)
		return (sizeof(keybuf) - keybufbot) + keybuftop;
	return keybuftop - keybufbot;
}
