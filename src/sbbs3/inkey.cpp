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
		if(!(mode&K_GETSTR) || mode&K_LOWPRIO || cfg.node_misc&NM_LOWPRIO)
			mswait(1);
		return(0);
	}

	if(cfg.node_misc&NM_7BITONLY
		&& (!(sys_status&SS_USERON) || useron.misc&NO_EXASCII))
		ch&=0x7f; 

	timeout=time(NULL);
	if(ch==3) {  /* Ctrl-C Abort */
		sys_status|=SS_ABORT;
		if(mode&K_SPIN) /* back space once if on spinning cursor */
			bputs("\b \b");
		return(0); }
	if(ch==26 && action!=NODE_PCHT) {	 /* Ctrl-Z toggle raw input mode */
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
		if(action!=NODE_MAIN && action!=NODE_XFER)
			return(26);
		return(0); }
	if(console&CON_RAW_IN)	 /* ignore ctrl-key commands if in raw mode */
		return(ch);

	if(ch<SP) { 				/* Control chars */
		if(ch==LF)				/* ignore LF's in not in raw mode */
			return(0);
		if(ch==15) {	/* Ctrl-O toggles pause temporarily */
			useron.misc^=UPAUSE;
			return(0); }
		if(ch==0x10) {	/* Ctrl-P Private node-node comm */
			if(!(sys_status&SS_USERON))
				return(0);			 /* keep from being recursive */
			if(mode&K_SPIN)
				bputs("\b ");
			if(sys_status&SS_SPLITP) {
#if 0 /* screen size */
				if((scrnbuf=(uchar*)MALLOC((24L*80L)*2L))==NULL) {
					errormsg(WHERE,ERR_ALLOC,nulstr,(24L*80L)*2L);
					return(CR); }
				gettext(1,1,80,24,scrnbuf);
				x=lclwx();
				y=lclwy();
				CLS; 
#endif
			}
			else {
				SAVELINE;
				attr(LIGHTGRAY);
				CRLF; }
			nodesync(); 	/* read any waiting messages */
			nodemsg();		/* send a message */
			SYNC;
			if(sys_status&SS_SPLITP) {
#if 0 /* screen size */
				lncntr=0;
				CLS;
				for(i=0;i<((24*80)-1)*2;i+=2) {
					if(scrnbuf[i+1]!=curatr)
						attr(scrnbuf[i+1]);
					outchar(scrnbuf[i]); }
				FREE(scrnbuf);
				GOTOXY(x,y); 
#endif
			}
			else {
				CRLF;
				RESTORELINE; }
			lncntr=0;
			return(0); }

		if(ch==21) { /* Ctrl-U Users online */
			if(!(sys_status&SS_USERON))
				return(0);
			if(mode&K_SPIN)
				bputs("\b ");
			if(sys_status&SS_SPLITP) {
#if 0 /* screen size */
				if((scrnbuf=(uchar*)MALLOC((24L*80L)*2L))==NULL) {
					errormsg(WHERE,ERR_ALLOC,nulstr,(24L*80L)*2L);
					return(CR); }
				gettext(1,1,80,24,scrnbuf);
				x=lclwx();
				y=lclwy();
				CLS; 
#endif
			}
			else {
				SAVELINE;
				attr(LIGHTGRAY);
				CRLF; }
			whos_online(true); 	/* list users */
			ASYNC;
			if(sys_status&SS_SPLITP) {
#if 0 /* screen size */
				CRLF;
				nodesync();
				pause();
				CLS;
				for(i=0;i<((24*80)-1)*2;i+=2) {
					if(scrnbuf[i+1]!=curatr)
						attr(scrnbuf[i+1]);
					outchar(scrnbuf[i]); }
				FREE(scrnbuf);
				GOTOXY(x,y); 
#endif
			}
			else {
				CRLF;
				RESTORELINE; }
			lncntr=0;
			return(0); }
		if(ch==20 && !(sys_status&SS_SPLITP)) { /* Ctrl-T Time information */
			if(!(sys_status&SS_USERON))
				return(0);
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
			return(0); }
		if(ch==11 && !(sys_status&SS_SPLITP)) {  /*  Ctrl-k Control key menu */
			if(!(sys_status&SS_USERON))
				return(0);
			if(mode&K_SPIN)
				bputs("\b ");
			SAVELINE;
			attr(LIGHTGRAY);
			lncntr=0;
			bputs(text[ControlKeyMenu]);
			ASYNC;
			RESTORELINE;
			lncntr=0;
			return(0); }

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
				return(0); }
			i=j=0;
			autoterm|=ANSI; 			/* <ESC>[x means they have ANSI */
			if(!(useron.misc&ANSI) && useron.misc&AUTOTERM && sys_status&SS_USERON
				&& useron.number) {
				useron.misc|=ANSI;
				putuserrec(&cfg,useron.number,U_MISC,8,ultoa(useron.misc,str,16)); }
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
								return(0x6);	/* ctrl-f (rt arrow) */
							case 'D':
								return(0x1d);	/* ctrl-] (lf arrow) */
							case 'H':
								return(0x2);	/* ctrl-b (beg line) */
							case 'K':
								return(0x5);	/* ctrl-e (end line) */
							}
						ungetkey(ESC);
						ungetkey('[');
						for(j=0;j<i;j++)
							ungetkey(str[j]);
						ungetkey(ch);
						return(0); }
					if(ch=='R') {       /* cursor position report */
						if(i && !(useron.rows)) {	/* auto-detect rows */
							str[i]=0;
							rows=atoi(str);
							if(rows<5 || rows>99) rows=24; }
						return(0); }
					str[i++]=ch; }
				else {
					mswait(100);
					j++; } }

			ungetkey(ESC);		/* should only get here if time-out */
			ungetkey('[');
			for(j=0;j<i;j++)
				ungetkey(str[j]);
			return(0); }

			}	/* end of control chars */

	return(ch);
}

