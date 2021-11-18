/* Synchronet single-key console functions */

/* $Id: getkey.cpp,v 1.69 2020/05/24 08:19:18 rswindell Exp $ */

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

#include "sbbs.h"
#include "telnet.h"	// TELNET_GA

/****************************************************************************/
/* Waits for remote or local user to hit a key. Inactivity timer is checked */
/* and hangs up if inactive for 4 minutes. Returns key hit, or uppercase of */
/* key hit if mode&K_UPPER or key out of KEY BUFFER. Does not print key.    */
/* Called from functions all over the place.                                */
/****************************************************************************/
char sbbs_t::getkey(long mode)
{
	uchar	ch, coldkey;
	ulong	c = sbbs_random(5);
	time_t	last_telnet_cmd=0;
	char* cursor = text[SpinningCursor0  + sbbs_random(10)];
	size_t cursors = strlen(cursor);

	if(online==ON_REMOTE && !input_thread_running)
		online=FALSE;
	if(!online) {
		YIELD();	// just in case someone is looping on getkey() when they shouldn't
		return(0);
	}
	sys_status&=~SS_ABORT;
	if((sys_status&SS_USERON || action==NODE_DFLT) && !(mode&(K_GETSTR|K_NOSPIN)))
		mode|=(useron.misc&SPIN);
	lncntr=0;
	timeout=time(NULL);
#if !defined SPINNING_CURSOR_OVER_HARDWARE_CURSOR
	if(mode&K_SPIN)
		outchar(' ');
#endif
	do {
		if(sys_status&SS_ABORT) {
			if(mode&K_SPIN) {
#if defined SPINNING_CURSOR_OVER_HARDWARE_CURSOR
				bputs(" \b");
#else
				backspace();
#endif
			}
			return(0); 
		}

		if(mode&K_SPIN) {
#if !defined SPINNING_CURSOR_OVER_HARDWARE_CURSOR
			outchar('\b');
#endif
			outchar(cursor[(c++) % cursors]);
#if defined SPINNING_CURSOR_OVER_HARDWARE_CURSOR
			outchar('\b');
#endif
		}
		ch=inkey(mode,mode&K_SPIN ? 200:1000);
		if(sys_status&SS_ABORT)
			return(0);
		now=time(NULL);
		if(ch) {
			if(mode&K_NUMBER && IS_PRINTABLE(ch) && !IS_DIGIT(ch))
				continue;
			if(mode&K_ALPHA && IS_PRINTABLE(ch) && !IS_ALPHA(ch))
				continue;
			if(mode&K_NOEXASC && ch&0x80)
				continue;
			if(mode&K_SPIN) {
#if defined SPINNING_CURSOR_OVER_HARDWARE_CURSOR
				bputs(" \b");
#else
				backspace();
#endif
			}
			if(mode&K_COLD && ch>' ' && useron.misc&COLDKEYS) {
				if(mode&K_UPPER)
					outchar(toupper(ch));
				else
					outchar(ch);
				while((coldkey=inkey(mode,1000))==0 && online && !(sys_status&SS_ABORT))
					;
				backspace();
				if(coldkey==BS || coldkey==DEL)
					continue;
				if(coldkey>' ')
					ungetkey(coldkey); 
			}
			if(mode&K_UPPER)
				return(toupper(ch));
			return(ch); 
		}
		if(sys_status&SS_USERON && !(sys_status&SS_LCHAT)) gettimeleft();
		else if(online && now-answertime>SEC_LOGON && !(sys_status&SS_LCHAT)) {
			console&=~(CON_R_ECHOX|CON_L_ECHOX);
			console|=(CON_R_ECHO|CON_L_ECHO);
			bputs(text[TakenTooLongToLogon]);
			hangup(); 
		}
		if(sys_status&SS_USERON && online && (timeleft/60)<(5-timeleft_warn)
			&& !SYSOP && !(sys_status&SS_LCHAT)) {
			timeleft_warn=5-(timeleft/60);
			SAVELINE;
			attr(LIGHTGRAY);
			bprintf(text[OnlyXminutesLeft]
				,((ushort)timeleft/60)+1,(timeleft/60) ? "s" : nulstr);
			RESTORELINE; 
		}

		if(!(startup->options&BBS_OPT_NO_TELNET_GA)
			&& now!=last_telnet_cmd && now-timeout>=60 && !((now-timeout)%60)) {
			// Let's make sure the socket is up
			// Sending will trigger a socket d/c detection
			send_telnet_cmd(TELNET_GA,0);
			last_telnet_cmd=now;
		}
			
		if(online==ON_REMOTE && !(console&CON_NO_INACT)
			&& (now-timeout >= cfg.sec_warn || now-timeout >= cfg.sec_hangup)) {
			if(sys_status&SS_USERON && cfg.sec_warn < cfg.sec_hangup) {
				SAVELINE;
				bputs(text[AreYouThere]); 
			}
			else
				bputs("\7\7");
			while(!inkey(K_NONE,100) && online && now-timeout < cfg.sec_hangup) {
				now=time(NULL);
			}
			if(now-timeout >= cfg.sec_hangup) {
				if(online==ON_REMOTE) {
					console|=CON_R_ECHO;
					console&=~CON_R_ECHOX; 
				}
				bputs(text[CallBackWhenYoureThere]);
				logline(LOG_NOTICE,nulstr,"Inactive");
				hangup();
				return(0); 
			}
			if(sys_status&SS_USERON) {
				bputs("\r\1n\1>");
				RESTORELINE; 
			}
			timeout=now; 
		}

	} while(online);

	return(0);
}


/****************************************************************************/
/* Outputs a string highlighting characters preceded by a tilde             */
/****************************************************************************/
void sbbs_t::mnemonics(const char *str)
{
    const char *ctrl_a_codes;
    long l;

	if(!strchr(str,'~')) {
		mnestr=str;
		bputs(str);
		return; 
	}
	ctrl_a_codes=strchr(str,1);
	if(!ctrl_a_codes) {
		if(str[0]=='@' && str[strlen(str)-1]=='@' && !strchr(str,' ')) {
			mnestr=str;
			bputs(str);
			return; 
		}
		attr(cfg.color[clr_mnelow]); 
	}
	l=0L;
	long term = term_supports();

	while(str[l]) {
		if(str[l]=='~' && str[l+1] < ' ') {
			add_hotspot('\r', /* hungry: */true);
			l+=2;
		}
		else if(str[l]=='~') {
			if(!(term&(ANSI|PETSCII)))
				outchar('(');
			l++;
			if(!ctrl_a_codes)
				attr(cfg.color[clr_mnehigh]);
			add_hotspot(str[l], /* hungry: */true);
			outchar(str[l]);
			l++;
			if(!(term&(ANSI|PETSCII)))
				outchar(')');
			if(!ctrl_a_codes)
				attr(cfg.color[clr_mnelow]); 
		}
		else if(str[l]=='`' && str[l+1]!=0) {
			if(!(term&(ANSI|PETSCII)))
				outchar('[');
			l++;
			if(!ctrl_a_codes)
				attr(cfg.color[clr_mnehigh]);
			add_hotspot(str[l], /* hungry: */false);
			outchar(str[l]);
			l++;
			if(!(term&(ANSI|PETSCII)))
				outchar(']');
			if(!ctrl_a_codes)
				attr(cfg.color[clr_mnelow]); 
		}
		else {
			if(str[l]==CTRL_A && str[l+1]!=0) {
				l++;
				if(str[l] == 'Z')	/* EOF (uppercase 'Z') */
					break;
				ctrl_a(str[l++]);
			} else {
				if(str[l] == '@') {
					int i = show_atcode(str + l);
					if(i) {
						l += i;
						continue;
					}
				}
				outchar(str[l++]);
			}
		} 
	}
	if(!ctrl_a_codes)
		attr(cfg.color[clr_mnecmd]);
}

/****************************************************************************/
/* Prompts user for Y or N (yes or no) and CR is interpreted as a Y         */
/* Returns true for Yes or false for No                                     */
/* Called from quite a few places                                           */
/****************************************************************************/
bool sbbs_t::yesno(const char *str, long mode)
{
    char ch;

	if(*str == 0)
		return true;
	SAFECOPY(question,str);
	SYNC;
	bprintf(mode, text[YesNoQuestion], str);
	while(online) {
		if(sys_status&SS_ABORT)
			ch=no_key();
		else
			ch=getkey(K_UPPER|K_COLD);
		if(ch==yes_key() || ch==CR) {
			if(bputs(text[Yes], mode) && !(mode&P_NOCRLF))
				CRLF;
			if(!(mode&P_SAVEATR))
				attr(LIGHTGRAY);
			lncntr=0;
			return(true); 
		}
		if(ch==no_key()) {
			if(bputs(text[No], mode) && !(mode&P_NOCRLF))
				CRLF;
			if(!(mode&P_SAVEATR))
				attr(LIGHTGRAY);
			lncntr=0;
			return(false); 
		} 
	}
	return(true);
}

/****************************************************************************/
/* Prompts user for N or Y (no or yes) and CR is interpreted as a N         */
/* Returns true for No or false for Yes                                     */
/****************************************************************************/
bool sbbs_t::noyes(const char *str, long mode)
{
    char ch;

	if(*str == 0)
		return true;
	SAFECOPY(question,str);
	SYNC;
	bprintf(mode, text[NoYesQuestion], str);
	while(online) {
		if(sys_status&SS_ABORT)
			ch=no_key();
		else
			ch=getkey(K_UPPER|K_COLD);
		if(ch==no_key() || ch==CR) {
			if(bputs(text[No], mode) && !(mode&P_NOCRLF))
				CRLF;
			if(!(mode&P_SAVEATR))
				attr(LIGHTGRAY);
			lncntr=0;
			return(true); 
		}
		if(ch==yes_key()) {
			if(bputs(text[Yes], mode) && !(mode&P_NOCRLF))
				CRLF;
			if(!(mode&P_SAVEATR))
				attr(LIGHTGRAY);
			lncntr=0;
			return(false); 
		} 
	}
	return(true);
}

/****************************************************************************/
/* Waits for remote or local user to hit a key among 'keys'.				*/
/* If 'keys' is NULL, *any* non-numeric key is valid input.					*/
/* 'max' is non-zero, allow that a decimal number input up to that size		*/
/* and return the value OR'd with 0x80000000.								*/
/* default mode value is K_UPPER											*/
/****************************************************************************/
long sbbs_t::getkeys(const char *keys, ulong max, long mode)
{
	char	str[81];
	uchar	ch,n=0,c=0;
	ulong	i=0;

	if(keys != NULL) {
		SAFECOPY(str,keys);
	}
	while(online) {
		ch=getkey(mode);
		if(max && ch>0x7f)  /* extended ascii chars are digits to isdigit() */
			continue;
		if(sys_status&SS_ABORT) {   /* return -1 if Ctrl-C hit */
			if(!(mode&(K_NOECHO|K_NOCRLF))) {
				attr(LIGHTGRAY);
				CRLF;
			}
			lncntr=0;
			return(-1); 
		}
		if(ch && !n && ((keys == NULL && !IS_DIGIT(ch)) || (strchr(str,ch)))) {  /* return character if in string */
			if(ch > ' ') {
				if(!(mode&K_NOECHO))
					outchar(ch);
				if(useron.misc&COLDKEYS) {
					while(online && !(sys_status&SS_ABORT)) {
						c=getkey(0);
						if(c==CR || c==BS || c==DEL)
							break; 
					}
					if(sys_status&SS_ABORT) {
						if(!(mode&(K_NOECHO|K_NOCRLF))) {
							CRLF;
						}
						return(-1); 
					}
					if(c==BS || c==DEL) {
						if(!(mode&K_NOECHO))
							backspace();
						continue; 
					} 
				}
				if(!(mode&(K_NOECHO|K_NOCRLF))) {
					attr(LIGHTGRAY);
					CRLF;
				}
				lncntr=0;
			}
			return(ch); 
		}
		if(ch==CR && max) {             /* return 0 if no number */
			if(!(mode&(K_NOECHO|K_NOCRLF))) {
				attr(LIGHTGRAY);
				CRLF;
			}
			lncntr=0;
			if(n)
				return(i|0x80000000L);		 /* return number plus high bit */
			return(0); 
		}
		if((ch==BS || ch==DEL) && n) {
			if(!(mode&K_NOECHO))
				backspace();
			i/=10;
			n--; 
		}
		else if(max && IS_DIGIT(ch) && (i*10)+(ch&0xf)<=max && (ch!='0' || n)) {
			i*=10;
			n++;
			i+=ch&0xf;
			if(!(mode&K_NOECHO))	
				outchar(ch);
			if(i*10>max && !(useron.misc&COLDKEYS) && keybuf_level() < 1) {
				if(!(mode&(K_NOECHO|K_NOCRLF))) {
					attr(LIGHTGRAY);
					CRLF;
				}
				lncntr=0;
				return(i|0x80000000L); 
			} 
		} 
	}
	return(-1);
}

/****************************************************************************/
/* Prints PAUSE message and waits for a key stoke                           */
/****************************************************************************/
void sbbs_t::pause()
{
	char	ch;
	uint	tempattrs=curatr; /* was lclatr(-1) */
	long	l=K_UPPER;
	size_t	len;

 	if((sys_status&SS_ABORT) || pause_inside)
		return;
	pause_inside = true;
	lncntr=0;
	if(online==ON_REMOTE)
		rioctl(IOFI);
	if(mouse_hotspots.first == NULL)
		pause_hotspot = add_hotspot('\r');
	bputs(text[Pause]);
	len = bstrlen(text[Pause]);
	if(sys_status&SS_USERON && !(useron.misc&(HTML|WIP|NOPAUSESPIN))
		&& !(cfg.node_misc&NM_NOPAUSESPIN))
		l|=K_SPIN;
	ch=getkey(l);
	if(pause_hotspot) {
		clear_hotspots();
		pause_hotspot = NULL;
	}
	if(ch==no_key() || ch==quit_key())
		sys_status|=SS_ABORT;
	else if(ch==LF)	// down arrow == display one more line
		lncntr=rows-2;
	if(text[Pause][0]!='@')
		backspace(len);
	getnodedat(cfg.node_num,&thisnode,0);
	nodesync();
	attr(tempattrs);
	pause_inside = false;
}

/****************************************************************************/
/* Puts a character into the input buffer                                   */
/****************************************************************************/
void sbbs_t::ungetkey(char ch, bool insert)
{
	char dbg[2] = {};
#if 0	/* this way breaks ansi_getxy() */
	RingBufWrite(&inbuf,(uchar*)&ch,sizeof(uchar));
#else
	if(keybuf_space()) {
		char* p = c_escape_char(ch);
		if(p == NULL) {
			dbg[0] = ch;
			p = dbg;
		}
		lprintf(LOG_DEBUG, "%s key into keybuf: %02X (%s)", insert ? "insert" : "append", ch, p);
		if(insert) {
			if(keybufbot == 0)
				keybufbot = KEY_BUFSIZE - 1;
			else
				keybufbot--;
			keybuf[keybufbot] = ch;
		} else {
			keybuf[keybuftop++]=ch;
			if(keybuftop==KEY_BUFSIZE)
				keybuftop=0;
		}
	} else
		lprintf(LOG_WARNING, "No space in keyboard input buffer");
#endif
}

/****************************************************************************/
/* Puts a string into the input buffer										*/
/****************************************************************************/
void sbbs_t::ungetstr(const char* str, bool insert)
{
	size_t i;

	for(i = 0; str[i] != '\0'; i++)
		ungetkey(str[i], insert);
}

size_t sbbs_t::keybuf_space(void)
{
	return sizeof(keybuf) - (keybuf_level() + 1);
}

size_t sbbs_t::keybuf_level(void)
{
	if(keybufbot > keybuftop)
		return (sizeof(keybuf) - keybufbot) + keybuftop;
	return keybuftop - keybufbot;
}
