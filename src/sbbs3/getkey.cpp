/* getkey.cpp */

/* Synchronet single-key console functions */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
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
	char	ch,coldkey,c=0,spin=sbbs_random(5);
	time_t	last_telnet_cmd=0;

	if(!online || !input_thread_running) {
		mswait(1);	// just in case someone is looping on getkey() when they shouldn't
		return(0);
	}
	sys_status&=~SS_ABORT;
	if((sys_status&SS_USERON || action==NODE_DFLT) && !(mode&K_GETSTR))
		mode|=(useron.misc&SPIN);
	lncntr=0;
	timeout=time(NULL);
	if(mode&K_SPIN)
		outchar(' ');
	do {
		checkline();    /* check to make sure remote user is still online */
		if(sys_status&SS_ABORT) {
			if(mode&K_SPIN) /* back space once if on spinning cursor */
				bputs("\b \b");
			return(0); 
		}

#if 0	// moved to inkey() on AUG-29-2001
		if(sys_status&SS_SYSPAGE) {
			sbbs_beep(sbbs_random(800),100);
		}
#endif

		if(mode&K_SPIN)
			switch(spin) {
				case 0:
					switch(c++) {
						case 0:
							outchar(BS);
							outchar('³');
							break;
						case 10:
							outchar(BS);
							outchar('/');
							break;
						case 20:
							outchar(BS);
							outchar('Ä');
							break;
						case 30:
							outchar(BS);
							outchar('\\');
							break;
						case 40:
							c=0;
							break;
						default:
							if(!(cfg.node_misc&NM_WINOS2))
								mswait(DELAY_SPIN);
							break;  }
					break;
				case 1:
					switch(c++) {
						case 0:
							outchar(BS);
							outchar('°');
							break;
						case 10:
							outchar(BS);
							outchar('±');
							break;
						case 20:
							outchar(BS);
							outchar('²');
							break;
						case 30:
							outchar(BS);
							outchar('Û');
							break;
						case 40:
							outchar(BS);
							outchar('²');
							break;
						case 50:
							outchar(BS);
							outchar('±');
							break;
						case 60:
							c=0;
							break;
						default:
							if(!(cfg.node_misc&NM_WINOS2))
								mswait(DELAY_SPIN);
							break;  }
					break;
				case 2:
					switch(c++) {
						case 0:
							outchar(BS);
							outchar('-');
							break;
						case 10:
							outchar(BS);
							outchar('=');
							break;
						case 20:
							outchar(BS);
							outchar('ð');
							break;
						case 30:
							outchar(BS);
							outchar('=');
							break;
						case 40:
							c=0;
							break;
						default:
							if(!(cfg.node_misc&NM_WINOS2))
								mswait(DELAY_SPIN);
							break;  }
					break;
				case 3:
					switch(c++) {
						case 0:
							outchar(BS);
							outchar('Ú');
							break;
						case 10:
							outchar(BS);
							outchar('À');
							break;
						case 20:
							outchar(BS);
							outchar('Ù');
							break;
						case 30:
							outchar(BS);
							outchar('¿');
							break;
						case 40:
							c=0;
							break;
						default:
							if(!(cfg.node_misc&NM_WINOS2))
								mswait(DELAY_SPIN);
							break;  }
					break;
				case 4:
					switch(c++) {
						case 0:
							outchar(BS);
							outchar('Ü');
							break;
						case 10:
							outchar(BS);
							outchar('Þ');
							break;
						case 20:
							outchar(BS);
							outchar('ß');
							break;
						case 30:
							outchar(BS);
							outchar('Ý');
							break;
						case 40:
							c=0;
							break;
						default:
							if(!(cfg.node_misc&NM_WINOS2))
								mswait(DELAY_SPIN);
							break;  
					}
					break; 
			}

		ch=inkey(mode);
		if(sys_status&SS_ABORT)
			return(0);
		now=time(NULL);
		if(ch) {
			if(mode&K_NUMBER && isprint(ch) && !isdigit(ch))
				continue;
			if(mode&K_ALPHA && isprint(ch) && !isalpha(ch))
				continue;
			if(mode&K_NOEXASC && ch&0x80)
				continue;
			if(mode&K_SPIN)
				bputs("\b \b");
			if(mode&K_COLD && ch>SP && useron.misc&COLDKEYS) {
				if(mode&K_UPPER)
					outchar(toupper(ch));
				else
					outchar(ch);
				while((coldkey=inkey(mode))==0 && online && !(sys_status&SS_ABORT))
					mswait(1);
				bputs("\b \b");
				if(coldkey==BS || coldkey==DEL)
					continue;
				if(coldkey>SP)
					ungetkey(coldkey); 
			}
			if(mode&K_UPPER)
				return(toupper(ch));
			return(ch); 
		}
		if(sys_status&SS_USERON && !(sys_status&SS_LCHAT)) gettimeleft();
		else if(online &&
			((cfg.node_dollars_per_call && now-answertime>SEC_BILLING)
			|| (now-answertime>SEC_LOGON && !(sys_status&SS_LCHAT)))) {
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

		if(!(startup->options&BBS_OPT_NO_TELNET_NOP)
			&& now!=last_telnet_cmd && now-timeout>=60 && !((now-timeout)%60)) {
			// Let's make sure the socket is up
			// Sending will trigger a socket d/c detection
			send_telnet_cmd(TELNET_NOP,0);
			last_telnet_cmd=now;
		}
			
		if(online==ON_REMOTE && !(console&CON_NO_INACT)
			&& now-timeout>=cfg.sec_warn) { 					/* warning */
			if(sys_status&SS_USERON && cfg.sec_warn!=cfg.sec_hangup) {
				SAVELINE;
				bputs(text[AreYouThere]); 
			}
			else
				bputs("\7\7");
			while(!inkey(0) && online && now-timeout>=cfg.sec_warn) {
				now=time(NULL);
				if(now-timeout>=cfg.sec_hangup) {
					if(online==ON_REMOTE) {
						console|=CON_R_ECHO;
						console&=~CON_R_ECHOX; 
					}
					bputs(text[CallBackWhenYoureThere]);
					logline(nulstr,"Inactive");
					hangup();
					return(0); 
				}
				mswait(100); 
			}
			if(sys_status&SS_USERON && cfg.sec_warn!=cfg.sec_hangup) {
				bputs("\r\1n\1>");
				RESTORELINE; 
			}
			timeout=now; 
		}

		} while(online);
	return(0);
}


/****************************************************************************/
/* Outputs a string highlighting characters preceeded by a tilde            */
/****************************************************************************/
void sbbs_t::mnemonics(char *str)
{
    char *ctrl_a_codes;
    long l;

	if(!strchr(str,'~')) {
		mnestr=str;
		bputs(str);
		return; 
	}
	ctrl_a_codes=strchr(str,1);
	if(!ctrl_a_codes) {
		if(str[0]=='@' && str[strlen(str)-1]=='@' && !strchr(str,SP)) {
			mnestr=str;
			bputs(str);
			return; 
		}
		attr(cfg.color[clr_mnelow]); 
	}
	l=0L;
	while(str[l]) {
		if(str[l]=='~' && str[l+1]!=0) {
			if(!(useron.misc&ANSI))
				outchar('(');
			l++;
			if(!ctrl_a_codes)
				attr(cfg.color[clr_mnehigh]);
			outchar(str[l]);
			l++;
			if(!(useron.misc&ANSI))
				outchar(')');
			if(!ctrl_a_codes)
				attr(cfg.color[clr_mnelow]); 
		}
		else {
			if(str[l]==CTRL_A           /* ctrl-a */
				&& str[l+1]!=0)	{		/* valid */
				ctrl_a(str[++l]);       /* skip the ctrl-a */
				l++;                    /* skip the attribute code */
			} else
				outchar(str[l++]); 
		} 
	}
	if(!ctrl_a_codes)
		attr(cfg.color[clr_mnecmd]);
}

/****************************************************************************/
/* Prompts user for Y or N (yes or no) and CR is interpreted as a Y         */
/* Returns 1 for Y or 0 for N                                               */
/* Called from quite a few places                                           */
/****************************************************************************/
bool sbbs_t::yesno(char *str)
{
    char ch;

	strcpy(question,str);
	SYNC;
	if(useron.misc&WIP) {
		strip_ctrl(question);
		menu("yesno"); 
	}
	else
		bprintf(text[YesNoQuestion],str);
	while(online) {
		if(sys_status&SS_ABORT)
			ch=text[YN][1];
		else
			ch=getkey(K_UPPER|K_COLD);
		if(ch==text[YN][0] || ch==CR) {
			if(bputs(text[Yes]))
				CRLF;
			lncntr=0;
			return(true); 
		}
		if(ch==text[YN][1]) {
			if(bputs(text[No]))
				CRLF;
			lncntr=0;
			return(false); 
		} 
	}
	return(true);
}

/****************************************************************************/
/* Prompts user for N or Y (no or yes) and CR is interpreted as a N         */
/* Returns 1 for N or 0 for Y                                               */
/* Called from quite a few places                                           */
/****************************************************************************/
bool sbbs_t::noyes(char *str)
{
    char ch;

	strcpy(question,str);
	SYNC;
	if(useron.misc&WIP) {
		strip_ctrl(question);
		menu("noyes"); 
	}
	else
		bprintf(text[NoYesQuestion],str);
	while(online) {
		if(sys_status&SS_ABORT)
			ch=text[YN][1];
		else
			ch=getkey(K_UPPER|K_COLD);
		if(ch==text[YN][1] || ch==CR) {
			if(bputs(text[No]))
				CRLF;
			lncntr=0;
			return(true); 
		}
		if(ch==text[YN][0]) {
			if(bputs(text[Yes]))
				CRLF;
			lncntr=0;
			return(false); 
		} 
	}
	return(true);
}

/****************************************************************************/
/* Waits for remote or local user to hit a key that is contained inside str.*/
/* 'str' should contain uppercase characters only. When a valid key is hit, */
/* it is echoed (upper case) and is the return value.                       */
/* Called from quite a few functions                                        */
/****************************************************************************/
long sbbs_t::getkeys(char *keys, ulong max)
{
	char	str[81];
	uchar	ch,n=0,c=0;
	ulong	i=0;

	SAFECOPY(str,keys);
	strupr(str);
	while(online) {
		ch=getkey(K_UPPER);
		if(max && ch>0x7f)  /* extended ascii chars are digits to isdigit() */
			continue;
		if(sys_status&SS_ABORT) {   /* return -1 if Ctrl-C hit */
			attr(LIGHTGRAY);
			CRLF;
			lncntr=0;
			return(-1); 
		}
		if(ch && !n && (strchr(str,ch))) {  /* return character if in string */
			outchar(ch);
			if(useron.misc&COLDKEYS && ch>SP) {
				while(online && !(sys_status&SS_ABORT)) {
					c=getkey(0);
					if(c==CR || c==BS || c==DEL)
						break; 
				}
				if(sys_status&SS_ABORT) {
					CRLF;
					return(-1); 
				}
				if(c==BS || c==DEL) {
					bputs("\b \b");
					continue; 
				} 
			}
			attr(LIGHTGRAY);
			CRLF;
			lncntr=0;
			return(ch); 
		}
		if(ch==CR && max) {             /* return 0 if no number */
			attr(LIGHTGRAY);
			CRLF;
			lncntr=0;
			if(n)
				return(i|0x80000000L);		 /* return number plus high bit */
			return(0); 
		}
		if((ch==BS || ch==DEL) && n) {
			bputs("\b \b");
			i/=10;
			n--; 
		}
		else if(max && isdigit(ch) && (i*10)+(ch&0xf)<=max && (ch!='0' || n)) {
			i*=10;
			n++;
			i+=ch&0xf;
			outchar(ch);
			if(i*10>max && !(useron.misc&COLDKEYS)) {
				attr(LIGHTGRAY);
				CRLF;
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
	uchar	tempattrs=curatr; /* was lclatr(-1) */
    int		i,j;
	long	l=K_UPPER;

	RIOSYNC(0);
	if(sys_status&SS_ABORT)
		return;
	lncntr=0;
	if(online==ON_REMOTE)
		rioctl(IOFI);
	bputs(text[Pause]);
	j=bstrlen(text[Pause]);
	if(sys_status&SS_USERON && !(useron.misc&NO_EXASCII) && !(useron.misc&WIP)
		&& !(cfg.node_misc&NM_NOPAUSESPIN))
		l|=K_SPIN;
	ch=getkey(l);
	if(ch==text[YN][1] || ch=='Q')
		sys_status|=SS_ABORT;
	else if(ch==LF)	// down arrow == display one more line
		lncntr=rows-2;
	if(text[Pause][0]!='@')
		for(i=0;i<j;i++)
			bputs("\b \b");
	getnodedat(cfg.node_num,&thisnode,0);
	nodesync();
	attr(tempattrs);
}

/****************************************************************************/
/* Puts a character into the input buffer                                   */
/****************************************************************************/
void sbbs_t::ungetkey(char ch)
{

	keybuf[keybuftop++]=ch;
	if(keybuftop==KEY_BUFSIZE)
		keybuftop=0;
}
