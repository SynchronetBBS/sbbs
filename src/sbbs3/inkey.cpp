/* Synchronet single key input function */

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
#include "petdefs.h"
#include "unicode.h"
#include "utf8.h"

int sbbs_t::kbincom(unsigned int timeout)
{
	int ch;

	if (keybuftop != keybufbot) {
		ch = keybuf[keybufbot++];
		if (keybufbot == KEY_BUFSIZE)
			keybufbot = 0;
#if 0
		char* p = c_escape_char(ch);
		if (p == NULL)
			p = (char*)&ch;
		lprintf(LOG_DEBUG, "kbincom read %02X '%s'", ch, p);
#endif
		return ch;
	}
	ch = incom(timeout);

	return ch;
}

int sbbs_t::translate_input(int ch)
{
	if (term->charset() == CHARSET_PETSCII) {
		switch (ch) {
			case PETSCII_HOME:
				return TERM_KEY_HOME;
			case PETSCII_CLEAR:
				return TERM_KEY_END;
			case PETSCII_INSERT:
				return TERM_KEY_INSERT;
			case PETSCII_DELETE:
				return '\b';
			case PETSCII_LEFT:
				return TERM_KEY_LEFT;
			case PETSCII_RIGHT:
				return TERM_KEY_RIGHT;
			case PETSCII_UP:
				return TERM_KEY_UP;
			case PETSCII_DOWN:
				return TERM_KEY_DOWN;
		}
		if ((ch & 0xe0) == 0xc0)   /* "Codes $60-$7F are, actually, copies of codes $C0-$DF" */
			ch = 0x60 | (ch & 0x1f);
		if (IS_ALPHA(ch))
			ch ^= 0x20; /* Swap upper/lower case */
	}
	else {
		bool lwe = last_inkey_was_esc;
		last_inkey_was_esc =  (ch == ESC);
		if (lwe && ch == '[') {
			autoterm |= ANSI;	// Receiving any CSI means the terminal is ANSI
			// update_terminal(this); causes later crash
		}
		if (term->supports(SWAP_DELETE)) {
			switch (ch) {
				case TERM_KEY_DELETE:
					ch = '\b';
					break;
				case '\b':
					ch = TERM_KEY_DELETE;
					break;
			}
		}
	}

	return ch;
}

void sbbs_t::translate_input(uchar* buf, size_t len)
{
	for (size_t i = 0; i < len; i++)
		buf[i] = translate_input(buf[i]);
}

/****************************************************************************/
/* Returns character if a key has been hit remotely and responds			*/
/* May return NOINP on timeout instead of '\0' when K_NUL mode is used.		*/
/****************************************************************************/
int sbbs_t::inkey(int mode, unsigned int timeout)
{
	int       ch = 0;
	const int no_input = (mode & K_NUL) ? NOINP : 0; // distinguish between timeout and '\0'

	ch = kbincom(timeout);

	if (sys_status & SS_SYSPAGE)
		sbbs_beep(400 + sbbs_random(800), ch == NOINP ? 100 : 10);

	if (ch == NOINP)
		return no_input;

	if (term->charset() == CHARSET_ASCII)
		ch &= 0x7f; // e.g. strip parity bit

	getkey_last_activity = time(NULL);

	/* Is this a control key */
	if (!(mode & (K_CTRLKEYS | K_EXTKEYS)) && ch < ' ') {
		if (cfg.ctrlkey_passthru & (1 << ch))    /*  flagged as passthru? */
			return ch;                    /* do not handle here */
		return handle_ctrlkey(ch, mode);
	} else if (ch == ESC && (mode & K_EXTKEYS)) {
		char key = ch;
		if (term->parse_input_sequence(key, mode))
			return key;
	}

	/* Translate (not control character) input into CP437 */
	if (!(mode & K_UTF8)) {
		if ((ch & 0x80) && (term->charset() == CHARSET_UTF8)) {
			char   utf8[UTF8_MAX_LEN] = { (char)ch };
			size_t len = utf8_decode_firstbyte(ch);
			if (len < 2 || len > sizeof(utf8))
				return no_input;
			for (size_t i = 1; i < len; ++i) {
				ch = kbincom(timeout);
				if (!(ch & 0x80) || (ch == NOINP))
					break;
				utf8[i] = ch;
			}
			if (ch & 0x80) {
				enum unicode_codepoint codepoint;
				len = utf8_getc(utf8, len, &codepoint);
				if (len < 1)
					return no_input;
				ch = unicode_to_cp437(codepoint);
				if (ch == 0)
					return no_input;
			}
		}
	}

	if (mode & K_UPPER)
		ch = toupper(ch);

	return ch;
}

char sbbs_t::handle_ctrlkey(char ch, int mode)
{
	char tmp[512];
	int  i;

	if (ch == TERM_KEY_ABORT) {  /* Ctrl-C Abort */
		sys_status |= SS_ABORT;
		if (mode & K_SPIN) /* back space once if on spinning cursor */
			term->backspace();
		return 0;
	}

	if (ch == CTRL_Z && !(mode & (K_MSG | K_GETSTR))
	    && action != NODE_PCHT) {  /* Ctrl-Z toggle raw input mode */
		if (hotkey_inside & (1 << ch))
			return 0;
		hotkey_inside |= (1 << ch);
		if (mode & K_SPIN)
			bputs("\b ");
		term->saveline();
		attr(LIGHTGRAY);
		term->newline();
		console ^= CON_RAW_IN;
		bputs(text[RawMsgInputModeIsNow]);
		term->newline(2);
		term->restoreline();
		term->lncntr = 0;
		hotkey_inside &= ~(1 << ch);
		if (action != NODE_MAIN && action != NODE_XFER)
			return CTRL_Z;
		return 0;
	}

	if (console & CON_RAW_IN)   /* ignore ctrl-key commands if in raw mode */
		return ch;

	/* Global hot key event */
	if (sys_status & SS_USERON) {
		for (i = 0; i < cfg.total_hotkeys; i++)
			if (cfg.hotkey[i]->key == ch)
				break;
		if (i < cfg.total_hotkeys) {
			if (hotkey_inside & (1 << ch))
				return 0;
			hotkey_inside |= (1 << ch);
			if (mode & K_SPIN)
				bputs("\b ");
			if (!(sys_status & SS_SPLITP)) {
				term->saveline();
				attr(LIGHTGRAY);
				term->newline();
			}
			if (cfg.hotkey[i]->cmd[0] == '?') {
				if (js_hotkey_cx == NULL) {
					js_hotkey_cx = js_init(&js_hotkey_runtime, &js_hotkey_glob, "HotKey");
					js_create_user_objects(js_hotkey_cx, js_hotkey_glob);
				}
				js_execfile(cmdstr(cfg.hotkey[i]->cmd + 1, nulstr, nulstr, tmp), /* startup_dir: */ NULL, /* scope: */ js_hotkey_glob, js_hotkey_cx, js_hotkey_glob);
			} else
				external(cmdstr(cfg.hotkey[i]->cmd, nulstr, nulstr, tmp), 0);
			if (!(sys_status & SS_SPLITP)) {
				term->newline();
				term->restoreline();
			}
			term->lncntr = 0;
			hotkey_inside &= ~(1 << ch);
			return 0;
		}
	}

	if (term->parse_input_sequence(ch, mode))
		return ch;

	switch (ch) {
		case CTRL_O:    /* Ctrl-O toggles pause temporarily */
			console ^= CON_PAUSE;
			return 0;
		case CTRL_P:    /* Ctrl-P Private node-node comm */
			if (!(sys_status & SS_USERON))
				break; ;
			if (hotkey_inside & (1 << ch))
				return 0;
			hotkey_inside |= (1 << ch);
			if (mode & K_SPIN)
				bputs("\b ");
			if (!(sys_status & SS_SPLITP)) {
				term->saveline();
				attr(LIGHTGRAY);
				term->newline();
			}
			nodesync();     /* read any waiting messages */
			nodemsg();      /* send a message */
			sync();
			if (!(sys_status & SS_SPLITP)) {
				term->newline();
				term->restoreline();
			}
			term->lncntr = 0;
			hotkey_inside &= ~(1 << ch);
			return 0;

		case CTRL_U:    /* Ctrl-U Users online */
			if (!(sys_status & SS_USERON))
				break;
			if (hotkey_inside & (1 << ch))
				return 0;
			hotkey_inside |= (1 << ch);
			if (mode & K_SPIN)
				bputs("\b ");
			if (!(sys_status & SS_SPLITP)) {
				term->saveline();
				attr(LIGHTGRAY);
				term->newline();
			}
			whos_online(true);  /* list users */
			sync();
			if (!(sys_status & SS_SPLITP)) {
				term->newline();
				term->restoreline();
			}
			term->lncntr = 0;
			hotkey_inside &= ~(1 << ch);
			return 0;
		case CTRL_T: /* Ctrl-T Time information */
			if (sys_status & SS_SPLITP)
				return ch;
			if (!(sys_status & SS_USERON))
				break;
			if (hotkey_inside & (1 << ch))
				return 0;
			hotkey_inside |= (1 << ch);
			if (mode & K_SPIN)
				bputs("\b ");
			term->saveline();
			attr(LIGHTGRAY);
			now = time(NULL);
			bprintf(text[TiLogon], timestr(logontime));
			bprintf(text[TiNow], timestr(now), smb_zonestr(sys_timezone(&cfg), NULL));
			bprintf(text[TiTimeon]
			        , sectostr(timeon(), tmp));
			bprintf(text[TiTimeLeft]
			        , sectostr(timeleft, tmp));
			if (sys_status & SS_EVENT)
				bprintf(text[ReducedTime], timestr(event_time));
			sync();
			term->restoreline();
			term->lncntr = 0;
			hotkey_inside &= ~(1 << ch);
			return 0;
		case CTRL_K:  /*  Ctrl-K Control key menu */
			if (sys_status & SS_SPLITP)
				return ch;
			if (!(sys_status & SS_USERON))
				break;
			if (hotkey_inside & (1 << ch))
				return 0;
			hotkey_inside |= (1 << ch);
			if (mode & K_SPIN)
				bputs("\b ");
			term->saveline();
			attr(LIGHTGRAY);
			term->lncntr = 0;
			if (mode & K_GETSTR)
				bputs(text[GetStrMenu]);
			else
				bputs(text[ControlKeyMenu]);
			sync();
			term->restoreline();
			term->lncntr = 0;
			hotkey_inside &= ~(1 << ch);
			return 0;
	}
	return ch;
}
