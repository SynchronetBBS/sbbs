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

int kbincom(sbbs_t* sbbs, unsigned long timeout)
{
	int	ch;

	if(sbbs->keybuftop!=sbbs->keybufbot) { 
		ch=sbbs->keybuf[sbbs->keybufbot++]; 
		if(sbbs->keybufbot==KEY_BUFSIZE) 
			sbbs->keybufbot=0;
#if 0
		char* p = c_escape_char(ch);
		if(p == NULL)
			p = (char*)&ch;
		lprintf(LOG_DEBUG, "kbincom read %02X '%s'", ch, p);
#endif
	} else {
		ch=sbbs->incom(timeout);
		long term = sbbs->term_supports();
		if(term&PETSCII) {
			switch(ch) {
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
			if((ch&0xe0) == 0xc0)	/* "Codes $60-$7F are, actually, copies of codes $C0-$DF" */
				ch = 0x60 | (ch&0x1f);
			if(IS_ALPHA(ch))
				ch ^= 0x20;	/* Swap upper/lower case */
		}

		if(term&SWAP_DELETE) {
			switch(ch) {
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

/****************************************************************************/
/* Returns character if a key has been hit remotely and responds			*/
/* May return NOINP on timeout instead of '\0' when K_NUL mode is used.		*/
/****************************************************************************/
int sbbs_t::inkey(long mode, unsigned long timeout)
{
	int	ch=0;

	ch=kbincom(this,timeout);

	if(sys_status&SS_SYSPAGE) 
		sbbs_beep(400 + sbbs_random(800), ch == NOINP ? 100 : 10);

	if(ch == NOINP) {
		if(mode & K_NUL)	// distinguish between timeout and '\0'
			return NOINP;
		return 0;
	}

	if(cfg.node_misc&NM_7BITONLY
		&& (!(sys_status&SS_USERON) || term_supports(NO_EXASCII)))
		ch&=0x7f; 

	this->timeout=time(NULL);

	/* Is this a control key */
	if(!(mode & K_CTRLKEYS) && ch < ' ') {
		if(cfg.ctrlkey_passthru&(1<<ch))	/*  flagged as passthru? */
			return(ch);						/* do not handle here */
		return(handle_ctrlkey(ch,mode));
	}

	if(mode&K_UPPER)
		ch=toupper(ch);

	return(ch);
}

char sbbs_t::handle_ctrlkey(char ch, long mode)
{
	char	str[512];
	char 	tmp[512];
	uint	i,j;

	if(ch==TERM_KEY_ABORT) {  /* Ctrl-C Abort */
		sys_status|=SS_ABORT;
		if(mode&K_SPIN) /* back space once if on spinning cursor */
			backspace();
		return(0); 
	}
	if(ch==CTRL_Z && !(mode&(K_MSG|K_GETSTR))
		&& action!=NODE_PCHT) {	 /* Ctrl-Z toggle raw input mode */
		if(hotkey_inside&(1<<ch))
			return(0);
		hotkey_inside |= (1<<ch);
		if(mode&K_SPIN)
			bputs("\b ");
		SAVELINE;
		attr(LIGHTGRAY);
		CRLF;
		bputs(text[RawMsgInputModeIsNow]);
		if(console&CON_RAW_IN)
			bputs(text[OFF]);
		else
			bputs(text[ON]);
		console^=CON_RAW_IN;
		CRLF;
		CRLF;
		RESTORELINE;
		lncntr=0;
		hotkey_inside &= ~(1<<ch);
		if(action!=NODE_MAIN && action!=NODE_XFER)
			return(CTRL_Z);
		return(0); 
	}

	if(console&CON_RAW_IN)	 /* ignore ctrl-key commands if in raw mode */
		return(ch);

#if 0	/* experimental removal to fix Tracker1's pause module problem with down-arrow */
	if(ch==LF)				/* ignore LF's if not in raw mode */
		return(0);
#endif

	/* Global hot key event */
	if(sys_status&SS_USERON) {
		for(i=0;i<cfg.total_hotkeys;i++)
			if(cfg.hotkey[i]->key==ch)
				break;
		if(i<cfg.total_hotkeys) {
			if(hotkey_inside&(1<<ch))
				return(0);
			hotkey_inside |= (1<<ch);
			if(mode&K_SPIN)
				bputs("\b ");
			if(!(sys_status&SS_SPLITP)) {
				SAVELINE;
				attr(LIGHTGRAY);
				CRLF; 
			}
			if(cfg.hotkey[i]->cmd[0]=='?') {
				if(js_hotkey_cx == NULL) {
					js_hotkey_cx = js_init(&js_hotkey_runtime, &js_hotkey_glob, "HotKey");
					js_create_user_objects(js_hotkey_cx, js_hotkey_glob);
				}
				js_execfile(cmdstr(cfg.hotkey[i]->cmd+1,nulstr,nulstr,tmp), /* startup_dir: */NULL, /* scope: */js_hotkey_glob, js_hotkey_cx, js_hotkey_glob);
			} else
				external(cmdstr(cfg.hotkey[i]->cmd,nulstr,nulstr,tmp),0);
			if(!(sys_status&SS_SPLITP)) {
				CRLF;
				RESTORELINE; 
			}
			lncntr=0;
			hotkey_inside &= ~(1<<ch);
			return(0);
		}
	}

	switch(ch) {
		case CTRL_O:	/* Ctrl-O toggles pause temporarily */
			console^=CON_PAUSEOFF;
			return(0); 
		case CTRL_P:	/* Ctrl-P Private node-node comm */
			if(!(sys_status&SS_USERON))
				break;;
			if(hotkey_inside&(1<<ch))
				return(0);
			hotkey_inside |= (1<<ch);
			if(mode&K_SPIN)
				bputs("\b ");
			if(!(sys_status&SS_SPLITP)) {
				SAVELINE;
				attr(LIGHTGRAY);
				CRLF; 
			}
			nodesync(); 	/* read any waiting messages */
			nodemsg();		/* send a message */
			SYNC;
			if(!(sys_status&SS_SPLITP)) {
				CRLF;
				RESTORELINE; 
			}
			lncntr=0;
			hotkey_inside &= ~(1<<ch);
			return(0); 

		case CTRL_U:	/* Ctrl-U Users online */
			if(!(sys_status&SS_USERON))
				break;
			if(hotkey_inside&(1<<ch))
				return(0);
			hotkey_inside |= (1<<ch);
			if(mode&K_SPIN)
				bputs("\b ");
			if(!(sys_status&SS_SPLITP)) {
				SAVELINE;
				attr(LIGHTGRAY);
				CRLF; 
			}
			whos_online(true); 	/* list users */
			ASYNC;
			if(!(sys_status&SS_SPLITP)) {
				CRLF;
				RESTORELINE; 
			}
			lncntr=0;
			hotkey_inside &= ~(1<<ch);
			return(0); 
		case CTRL_T: /* Ctrl-T Time information */
			if(sys_status&SS_SPLITP)
				return(ch);
			if(!(sys_status&SS_USERON))
				break;
			if(hotkey_inside&(1<<ch))
				return(0);
			hotkey_inside |= (1<<ch);
			if(mode&K_SPIN)
				bputs("\b ");
			SAVELINE;
			attr(LIGHTGRAY);
			now=time(NULL);
			bprintf(text[TiLogon],timestr(logontime));
			bprintf(text[TiNow],timestr(now),smb_zonestr(sys_timezone(&cfg),NULL));
			bprintf(text[TiTimeon]
				,sectostr((uint)(now-logontime),tmp));
			bprintf(text[TiTimeLeft]
				,sectostr(timeleft,tmp));
			if(sys_status&SS_EVENT)
				bprintf(text[ReducedTime],timestr(event_time));
			SYNC;
			RESTORELINE;
			lncntr=0;
			hotkey_inside &= ~(1<<ch);
			return(0); 
		case CTRL_K:  /*  Ctrl-K Control key menu */
			if(sys_status&SS_SPLITP)
				return(ch);
			if(!(sys_status&SS_USERON))
				break;
			if(hotkey_inside&(1<<ch))
				return(0);
			hotkey_inside |= (1<<ch);
			if(mode&K_SPIN)
				bputs("\b ");
			SAVELINE;
			attr(LIGHTGRAY);
			lncntr=0;
			if(mode&K_GETSTR)
				bputs(text[GetStrMenu]);
			else
				bputs(text[ControlKeyMenu]);
			ASYNC;
			RESTORELINE;
			lncntr=0;
			hotkey_inside &= ~(1<<ch);
			return(0); 
		case ESC:
			i=kbincom(this, (mode&K_GETSTR) ? 3000:1000);
			if(i==NOINP)		// timed-out waiting for '['
				return(ESC);
			ch=i;
			if(ch!='[') {
				ungetkey(ch, /* insert: */true);
				return(ESC); 
			}
			i=j=0;
			autoterm|=ANSI; 			/* <ESC>[x means they have ANSI */
#if 0 // this seems like a "bad idea" {tm}
			if(sys_status&SS_USERON && useron.misc&AUTOTERM && !(useron.misc&ANSI)
				&& useron.number) {
				useron.misc|=ANSI;
				putuserrec(&cfg,useron.number,U_MISC,8,ultoa(useron.misc,str,16)); 
			}
#endif
			while(i<10 && j<30) {		/* up to 3 seconds */
				ch=kbincom(this, 100);
				if(ch==(NOINP&0xff)) {
					j++;
					continue;
				}
				if(i == 0 && ch == 'M' && mouse_mode != MOUSE_MODE_OFF) {
					str[i++] = ch;
					int button = kbincom(this, 100);
					if(button == NOINP) {
						lprintf(LOG_DEBUG, "Timeout waiting for mouse button value");
						continue;
					}
					str[i++] = button;
					ch = kbincom(this, 100);
					if(ch < '!') {
						lprintf(LOG_DEBUG, "Unexpected mouse-button (0x%02X) tracking char: 0x%02X < '!'"
							, button, ch);
						continue;
					}
					str[i++] = ch;
					int x = ch - '!';
					ch = kbincom(this, 100);
					if(ch < '!') {
						lprintf(LOG_DEBUG, "Unexpected mouse-button (0x%02X) tracking char: 0x%02X < '!'"
							, button, ch);
						continue;
					}
					str[i++] = ch;
					int y = ch - '!';
					lprintf(LOG_DEBUG, "X10 Mouse button-click (0x%02X) reported at: %u x %u", button, x, y);
					if(button == 0x20) { // Left-click
						list_node_t* node;
						for(node = mouse_hotspots.first; node != NULL; node = node->next) {
							struct mouse_hotspot* spot = (struct mouse_hotspot*)node->data;
							if(spot->y == y && x >= spot->minx && x <= spot->maxx)
								break;
						}
						if(node == NULL) {
							for(node = mouse_hotspots.first; node != NULL; node = node->next) {
								struct mouse_hotspot* spot = (struct mouse_hotspot*)node->data;
								if(spot->hungry && spot->y == y && x >= spot->minx)
									break;
							}
						}
						if(node == NULL) {
							for(node = mouse_hotspots.last; node != NULL; node = node->prev) {
								struct mouse_hotspot* spot = (struct mouse_hotspot*)node->data;
								if(spot->hungry && spot->y == y && x <= spot->minx)
									break;
							}
						}
						if(node != NULL) {
							struct mouse_hotspot* spot = (struct mouse_hotspot*)node->data;
	#ifdef _DEBUG
							{
								char dbg[128];
								c_escape_str(spot->cmd, dbg, sizeof(dbg), /* Ctrl-only? */true);
								lprintf(LOG_DEBUG, "Stuffing hot spot command into keybuf: '%s'", dbg);
							}
	#endif
							ungetstr(spot->cmd);
							if(pause_inside && pause_hotspot == NULL)
								return handle_ctrlkey(TERM_KEY_ABORT, mode);
							return 0;
						}
						if(pause_inside && y == rows - 1)
							return '\r';
					} else if(button == '`' && console&CON_MOUSE_SCROLL) {
						return TERM_KEY_UP;
					} else if(button == 'a' && console&CON_MOUSE_SCROLL) {
						return TERM_KEY_DOWN;
					}
					if((button != 0x23 && console&CON_MOUSE_CLK_PASSTHRU)
						|| (button == 0x23 && console&CON_MOUSE_REL_PASSTHRU)) {
						for(j = i; j > 0; j--)
							ungetkey(str[j - 1], /* insert: */true);
						ungetkey('[', /* insert: */true);
						return(ESC); 
					}
					if(button == 0x22)  // Right-click
						return handle_ctrlkey(TERM_KEY_ABORT, mode);
					return 0;
				}
				if(i == 0 && ch == '<' && mouse_mode != MOUSE_MODE_OFF) {
					while(i < sizeof(str) - 1) {
						int byte = kbincom(this, 100);
						if(byte == NOINP) {
							lprintf(LOG_DEBUG, "Timeout waiting for mouse report character (%d)", i);
							return 0;
						}
						str[i++] = byte;
						if(IS_ALPHA(byte))
							break;
					}
					str[i] = 0;
					int button = -1, x = 0, y = 0;
					if(sscanf(str, "%d;%d;%d%c", &button, &x, &y, &ch) != 4
						|| button < 0 || x < 1 || y < 1 || toupper(ch) != 'M') {
						lprintf(LOG_DEBUG, "Invalid SGR mouse report sequence: '%s'", str);
						return 0;
					}
					--x;
					--y;
					lprintf(LOG_DEBUG, "SGR Mouse button-click (0x%02X) reported at: %u x %u", button, x, y);
					if(button == 0 && ch == 'M') { // Left-button press
						list_node_t* node;
						for(node = mouse_hotspots.first; node != NULL; node = node->next) {
							struct mouse_hotspot* spot = (struct mouse_hotspot*)node->data;
							if(spot->y == y && x >= spot->minx && x <= spot->maxx)
								break;
						}
						if(node == NULL) {
							for(node = mouse_hotspots.first; node != NULL; node = node->next) {
								struct mouse_hotspot* spot = (struct mouse_hotspot*)node->data;
								if(spot->hungry && spot->y == y && x >= spot->minx)
									break;
							}
						}
						if(node == NULL) {
							for(node = mouse_hotspots.last; node != NULL; node = node->prev) {
								struct mouse_hotspot* spot = (struct mouse_hotspot*)node->data;
								if(spot->hungry && spot->y == y && x <= spot->minx)
									break;
							}
						}
						if(node != NULL) {
							struct mouse_hotspot* spot = (struct mouse_hotspot*)node->data;
	#ifdef _DEBUG
							{
								char dbg[128];
								c_escape_str(spot->cmd, dbg, sizeof(dbg), /* Ctrl-only? */true);
								lprintf(LOG_DEBUG, "Stuffing hot spot command into keybuf: '%s'", dbg);
							}
	#endif
							ungetstr(spot->cmd);
							if(pause_inside && pause_hotspot == NULL)
								return handle_ctrlkey(TERM_KEY_ABORT, mode);
							return 0;
						}
						if(pause_inside && y == rows - 1)
							return '\r';
					} else if(button == 0x40 && console&CON_MOUSE_SCROLL) {
						return TERM_KEY_UP;
					} else if(button == 0x41 && console&CON_MOUSE_SCROLL) {
						return TERM_KEY_DOWN;
					}
					if((ch == 'M' && console&CON_MOUSE_CLK_PASSTHRU)
						|| (ch == 'm' && console&CON_MOUSE_REL_PASSTHRU)) {
						lprintf(LOG_DEBUG, "Passing-through SGR mouse report: 'ESC[<%s'", str);
						for(j = i; j > 0; j--)
							ungetkey(str[j - 1], /* insert: */true);
						ungetkey('<', /* insert: */true);
						ungetkey('[', /* insert: */true);
						return(ESC); 
					}
					if(ch == 'M' && button == 2)  // Right-click
						return handle_ctrlkey(TERM_KEY_ABORT, mode);
	#ifdef _DEBUG
					lprintf(LOG_DEBUG, "Eating SGR mouse report: 'ESC[<%s'", str);
	#endif
					return 0;
				}
				if(ch!=';' && !IS_DIGIT(ch) && ch!='R') {    /* other ANSI */
					str[i]=0;
					switch(ch) {
						case 'A':
							return(TERM_KEY_UP);
						case 'B':
							return(TERM_KEY_DOWN);
						case 'C':
							return(TERM_KEY_RIGHT);
						case 'D':
							return(TERM_KEY_LEFT);
						case 'H':	/* ANSI:  home cursor */
							return(TERM_KEY_HOME);
						case 'V':
							return TERM_KEY_PAGEUP;
						case 'U':
							return TERM_KEY_PAGEDN;
						case 'F':	/* Xterm: cursor preceding line */
						case 'K':	/* ANSI:  clear-to-end-of-line */
							return(TERM_KEY_END);
						case '@':	/* ANSI/ECMA-048 INSERT */
							return(TERM_KEY_INSERT);
						case '~':	/* VT-220 (XP telnet.exe) */
							switch(atoi(str)) {
								case 1:
									return(TERM_KEY_HOME);
								case 2:
									return(TERM_KEY_INSERT);
								case 3:
									return(TERM_KEY_DELETE);
								case 4:
									return(TERM_KEY_END);
								case 5:
									return TERM_KEY_PAGEUP;
								case 6:
									return TERM_KEY_PAGEDN;
							}
							break;
					}
					ungetkey(ch, /* insert: */true);
					for(j = i; j > 0; j--)
						ungetkey(str[j - 1], /* insert: */true);
					ungetkey('[', /* insert: */true);
					return(ESC); 
				}
				if(ch=='R') {       /* cursor position report */
					if(mode&K_ANSI_CPR && i) {	/* auto-detect rows */
						int	x,y;
						str[i]=0;
						if(sscanf(str,"%u;%u",&y,&x)==2) {
							lprintf(LOG_DEBUG,"received ANSI cursor position report: %ux%u"
								,x, y);
							/* Sanity check the coordinates in the response: */
							if(useron.cols == TERM_COLS_AUTO && x >= TERM_COLS_MIN && x <= TERM_COLS_MAX) cols=x;
							if(useron.rows == TERM_ROWS_AUTO && y >= TERM_ROWS_MIN && y <= TERM_ROWS_MAX) rows=y;
							if(useron.cols == TERM_COLS_AUTO || useron.rows == TERM_ROWS_AUTO)
								update_nodeterm();
						}
					}
					return(0); 
				}
				str[i++]=ch; 
			}

			for(j = i; j > 0; j--)
				ungetkey(str[j - 1], /* insert: */true);
			ungetkey('[', /* insert: */true);
			return(ESC); 
	}
	return(ch);
}

void sbbs_t::set_mouse(long flags)
{
	long term = term_supports();
	if((term&ANSI) && ((term&MOUSE) || flags == MOUSE_MODE_OFF)) {
		long mode = mouse_mode & ~flags;
		if(mode & MOUSE_MODE_X10)	ansi_mouse(ANSI_MOUSE_X10, false);
		if(mode & MOUSE_MODE_NORM)	ansi_mouse(ANSI_MOUSE_NORM, false);
		if(mode & MOUSE_MODE_BTN)	ansi_mouse(ANSI_MOUSE_BTN, false);
		if(mode & MOUSE_MODE_ANY)	ansi_mouse(ANSI_MOUSE_ANY, false);
		if(mode & MOUSE_MODE_EXT)	ansi_mouse(ANSI_MOUSE_EXT, false);

		mode = flags & ~mouse_mode;
		if(mode & MOUSE_MODE_X10)	ansi_mouse(ANSI_MOUSE_X10, true);
		if(mode & MOUSE_MODE_NORM)	ansi_mouse(ANSI_MOUSE_NORM, true);
		if(mode & MOUSE_MODE_BTN)	ansi_mouse(ANSI_MOUSE_BTN, true);
		if(mode & MOUSE_MODE_ANY)	ansi_mouse(ANSI_MOUSE_ANY, true);
		if(mode & MOUSE_MODE_EXT)	ansi_mouse(ANSI_MOUSE_EXT, true);

		mouse_mode = flags;
	}
}

struct mouse_hotspot* sbbs_t::add_hotspot(struct mouse_hotspot* spot)
{
	if(spot->y < 0)
		spot->y = row;
	if(spot->minx < 0)
		spot->minx = column;
	if(spot->maxx < 0)
		spot->maxx = cols - 1;
#ifdef _DEBUG
	char dbg[128];
	lprintf(LOG_DEBUG, "Adding mouse hot spot %ld-%ld x %ld = '%s'"
		,spot->minx, spot->maxx, spot->y, c_escape_str(spot->cmd, dbg, sizeof(dbg), /* Ctrl-only? */true));
#endif
	list_node_t* node = listInsertNodeData(&mouse_hotspots, spot, sizeof(*spot));
	if(node == NULL)
		return NULL;
	set_mouse(MOUSE_MODE_NORM | MOUSE_MODE_EXT);
	return (struct mouse_hotspot*)node->data;
}

void sbbs_t::clear_hotspots(void)
{
	long spots = listCountNodes(&mouse_hotspots);
	if(spots) {
#ifdef _DEBUG
		lprintf(LOG_DEBUG, "Clearing %ld mouse hot spots", spots);
#endif
		listFreeNodes(&mouse_hotspots);
		if(!(console&CON_MOUSE_SCROLL))
			set_mouse(MOUSE_MODE_OFF);
	}
}

void sbbs_t::scroll_hotspots(long count)
{
	long spots = 0;
	long remain = 0;
	for(list_node_t* node = mouse_hotspots.first; node != NULL; node = node->next) {
		struct mouse_hotspot* spot = (struct mouse_hotspot*)node->data;
		spot->y -= count;
		spots++;
		if(spot->y >= 0)
			remain++;
	}
#ifdef _DEBUG
	if(spots)
		lprintf(LOG_DEBUG, "Scrolled %ld mouse hot-spots %ld rows (%ld remain)", spots, count, remain);
#endif
	if(remain < 1)
		clear_hotspots();
}

struct mouse_hotspot* sbbs_t::add_hotspot(char cmd, bool hungry, long minx, long maxx, long y)
{
	struct mouse_hotspot spot = {0};
	spot.cmd[0] = cmd;
	spot.minx = minx < 0 ? column : minx;
	spot.maxx = maxx < 0 ? column : maxx;
	spot.y = y;
	spot.hungry = hungry;
	return add_hotspot(&spot);
}

struct mouse_hotspot* sbbs_t::add_hotspot(ulong num, bool hungry, long minx, long maxx, long y)
{
	struct mouse_hotspot spot = {0};
	SAFEPRINTF(spot.cmd, "%lu\r", num);
	spot.minx = minx;
	spot.maxx = maxx;
	spot.y = y;
	spot.hungry = hungry;
	return add_hotspot(&spot);
}

struct mouse_hotspot* sbbs_t::add_hotspot(const char* cmd, bool hungry, long minx, long maxx, long y)
{
	struct mouse_hotspot spot = {0};
	SAFECOPY(spot.cmd, cmd);
	spot.minx = minx;
	spot.maxx = maxx;
	spot.y = y;
	spot.hungry = hungry;
	return add_hotspot(&spot);
}
