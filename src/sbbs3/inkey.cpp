/* inkey.cpp */

/* Synchronet single key input function (no wait) */

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

#define LAST_STAT_LINE 16

#define nosound()	

/****************************************************************************/
/* Returns character if a key has been hit remotely and responds			*/
/* Called from functions getkey, msgabort and main_sec						*/
/****************************************************************************/
char sbbs_t::inkey(long mode)
{
	char	str[512];
	char 	tmp[512];
	uchar	ch=0;
	uint	i,j;

    if(keybuftop!=keybufbot) {
        ch=keybuf[keybufbot++];
        if(keybufbot==KEY_BUFSIZE)
            keybufbot=0; 
	} else
		ch=incom();
	if(ch==0) {
		// moved here from getkey() on AUG-29-2001
		if(sys_status&SS_SYSPAGE) 
			sbbs_beep(sbbs_random(800),100);

		if(!(mode&K_GETSTR) || mode&K_LOWPRIO || cfg.node_misc&NM_LOWPRIO)
			mswait(1);

		return(0);
	}

	if(cfg.node_misc&NM_7BITONLY
		&& (!(sys_status&SS_USERON) || useron.misc&NO_EXASCII))
		ch&=0x7f; 

	timeout=time(NULL);
	if(ch==CTRL_C) {  /* Ctrl-C Abort */
		sys_status|=SS_ABORT;
		if(mode&K_SPIN) /* back space once if on spinning cursor */
			bputs("\b \b");
		return(0); 
	}
	if(ch==CTRL_Z && action!=NODE_PCHT) {	 /* Ctrl-Z toggle raw input mode */
		if(hotkey_inside>1)	/* only allow so much recursion */
			return(0);
		hotkey_inside++;
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
		hotkey_inside--;
		if(action!=NODE_MAIN && action!=NODE_XFER)
			return(CTRL_Z);
		return(0); 
	}

	if(mode&K_UPPER)
		ch=toupper(ch);

	if(console&CON_RAW_IN)	 /* ignore ctrl-key commands if in raw mode */
		return(ch);

	if(ch<SP) { 				/* Control chars */
		if(ch==LF)				/* ignore LF's if not in raw mode */
			return(0);
		for(i=0;i<cfg.total_hotkeys;i++)
			if(cfg.hotkey[i]->key==ch)
				break;
		if(i<cfg.total_hotkeys) {
			if(hotkey_inside>1)	/* only allow so much recursion */
				return(0);
			hotkey_inside++;
			if(mode&K_SPIN)
				bputs("\b ");
			if(!(sys_status&SS_SPLITP)) {
				SAVELINE;
				attr(LIGHTGRAY);
				CRLF; 
			}
			external(cmdstr(cfg.hotkey[i]->cmd,nulstr,nulstr,NULL),0);
			if(!(sys_status&SS_SPLITP)) {
				CRLF;
				RESTORELINE; 
			}
			lncntr=0;
			hotkey_inside--;
			return(0);
		}
		if(ch==CTRL_O) {	/* Ctrl-O toggles pause temporarily */
			useron.misc^=UPAUSE;
			return(0); 
		}
		if(ch==CTRL_P) {	/* Ctrl-P Private node-node comm */
			if(!(sys_status&SS_USERON))
				return(0);			 /* keep from being recursive */
			if(hotkey_inside>1)	/* only allow so much recursion */
				return(0);
			hotkey_inside++;
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
			hotkey_inside--;
			return(0); 
		}

		if(ch==CTRL_U) { /* Ctrl-U Users online */
			/* needs recursion checking */
			if(!(sys_status&SS_USERON))
				return(0);
			if(hotkey_inside>1)	/* only allow so much recursion */
				return(0);
			hotkey_inside++;
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
			hotkey_inside--;
			return(0); 
		}
		if(ch==CTRL_T && !(sys_status&SS_SPLITP)) { /* Ctrl-T Time information */
			if(!(sys_status&SS_USERON))
				return(0);
			if(hotkey_inside>1)	/* only allow so much recursion */
				return(0);
			hotkey_inside++;
			if(mode&K_SPIN)
				bputs("\b ");
			SAVELINE;
			attr(LIGHTGRAY);
			now=time(NULL);
			bprintf(text[TiLogon],timestr(&logontime));
			bprintf(text[TiNow],timestr(&now));
			bprintf(text[TiTimeon]
				,sectostr(now-logontime,tmp));
			bprintf(text[TiTimeLeft]
				,sectostr(timeleft,tmp));
			SYNC;
			RESTORELINE;
			lncntr=0;
			hotkey_inside--;
			return(0); 
		}
		if(ch==CTRL_K && !(sys_status&SS_SPLITP)) {  /*  Ctrl-k Control key menu */
			if(!(sys_status&SS_USERON))
				return(0);
			if(hotkey_inside>1)	/* only allow so much recursion */
				return(0);
			hotkey_inside++;
			if(mode&K_SPIN)
				bputs("\b ");
			SAVELINE;
			attr(LIGHTGRAY);
			lncntr=0;
			bputs(text[ControlKeyMenu]);
			ASYNC;
			RESTORELINE;
			lncntr=0;
			hotkey_inside--;
			return(0); 
		}

		if(ch==ESC && console&CON_R_INPUT) {
			if(mode&K_GETSTR)
				i=60;	// 3 seconds in GETSTR mode
			else
				i=20;	// 1 second otherwise
			for(;i && !rioctl(RXBC);i--)
				mswait(50);
			if(!i)		// timed-out waiting for '['
				return(ESC);
			ch=incom();
			if(ch!='[') {
				ungetkey(ESC);
				ungetkey(ch);
				return(0); 
			}
			i=j=0;
			autoterm|=ANSI; 			/* <ESC>[x means they have ANSI */
			if(!(useron.misc&ANSI) && useron.misc&AUTOTERM && sys_status&SS_USERON
				&& useron.number) {
				useron.misc|=ANSI;
				putuserrec(&cfg,useron.number,U_MISC,8,ultoa(useron.misc,str,16)); 
			}
			while(i<10 && j<30) {		/* up to 3 seconds */
				if(rioctl(RXBC)) {
					ch=incom();
					if(ch!=';' && !isdigit(ch) && ch!='R') {    /* other ANSI */
						switch(ch) {
							case 'A':
								return(0x1e);	/* ctrl-^ (up arrow) */
							case 'B':
								return(LF); 	/* ctrl-j (dn arrow) */
							case 'C':
								return(CTRL_F);	/* ctrl-f (rt arrow) */
							case 'D':
								return(0x1d);	/* ctrl-] (lf arrow) */
							case 'H':
								return(CTRL_B);	/* ctrl-b (beg line) */
							case 'K':
								return(CTRL_E);	/* ctrl-e (end line) */
						}
						ungetkey(ESC);
						ungetkey('[');
						for(j=0;j<i;j++)
							ungetkey(str[j]);
						ungetkey(ch);
						return(0); 
					}
					if(ch=='R') {       /* cursor position report */
						if(i && !(useron.rows)) {	/* auto-detect rows */
							str[i]=0;
							rows=atoi(str);
							lprintf("Node %d ANSI cursor position report: %u rows"
								,cfg.node_num, rows);
							if(rows<10 || rows>99) rows=24; 
						}
						return(0); 
					}
					str[i++]=ch; 
				} else {
					mswait(100);
					j++; 
				} 
			}

			ungetkey(ESC);		/* should only get here if time-out */
			ungetkey('[');
			for(j=0;j<i;j++)
				ungetkey(str[j]);
			return(0); 
		}

	}	/* end of control chars */

	return(ch);
}

