/* con_out.cpp */

/* Synchronet console output routines */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2003 Rob Swindell - http://www.synchro.net/copyright.html		*
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

/***************************************************/
/* Seven bit table for EXASCII to ASCII conversion */
/***************************************************/
static const char *sbtbl="CUeaaaaceeeiiiAAEaAooouuyOUcLYRfaiounNao?--24!<>"
			"###||||++||++++++--|-+||++--|-+----++++++++##[]#"
			"abrpEout*ono%0ENE+><rj%=o..+n2* ";

/****************************************************************************/
/* Convert string from IBM extended ASCII to just ASCII						*/
/****************************************************************************/
char* DLLCALL ascii_str(uchar* str)
{
	size_t i;

	for(i=0;str[i];i++)
		if(str[i]&0x80)
			str[i]=sbtbl[str[i]^0x80];  /* seven bit table */
		else if(str[i]==CTRL_A	        /* ctrl-a */
			&& str[i+1]!=0)				/* valid */
			i++;						/* skip the attribute code */

	return((char*)str);
}

/****************************************************************************/
/* Outputs a NULL terminated string locally and remotely (if applicable)    */
/* Handles ctrl-a characters                                                */
/****************************************************************************/
int sbbs_t::bputs(char *str)
{
	int i;
    ulong l=0;

	if(online==ON_LOCAL && console&CON_L_ECHO) 	/* script running as event */
		return(eprintf("%s",str));

	while(str[l]) {
		if(str[l]==CTRL_A	        /* ctrl-a */
			&& str[l+1]!=0) {		/* valid */
			ctrl_a(str[++l]);       /* skip the ctrl-a */
			l++;					/* skip the attribute code */
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

/****************************************************************************/
/* Outputs a NULL terminated string locally and remotely (if applicable)    */
/* Does not expand ctrl-a characters (raw)                                  */
/* Max length of str is 64 kbytes                                           */
/****************************************************************************/
int sbbs_t::rputs(char *str)
{
    ulong l=0;

	if(online==ON_LOCAL && console&CON_L_ECHO)	/* script running as event */
		return(eprintf("%s",str));

	while(str[l])
		outchar(str[l++]);
	return(l);
}

/****************************************************************************/
/* Performs printf() using bbs bputs function								*/
/****************************************************************************/
int sbbs_t::bprintf(char *fmt, ...)
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
int sbbs_t::rprintf(char *fmt, ...)
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
/* Outputs character locally and remotely (if applicable), preforming echo  */
/* translations (X's and r0dent emulation) if applicable.					*/
/****************************************************************************/
void sbbs_t::outchar(char ch)
{
	int		i;

	if(console&CON_ECHO_OFF)
		return;
	if(ch==ESC)
		outchar_esc=1;
	else if(outchar_esc==1) {
		if(ch=='[')
			outchar_esc++;
		else
			outchar_esc=0; 
	}
	else
		outchar_esc=0;
	if(useron.misc&NO_EXASCII && ch&0x80)
		ch=sbtbl[(uchar)ch^0x80];  /* seven bit table */
	if(ch==FF && lncntr>1 && !tos) {
		lncntr=0;
		CRLF;
		if(!(sys_status&SS_PAUSEOFF)) {
			pause();
			while(lncntr && online && !(sys_status&SS_ABORT))
				pause(); 
		}
	}
#if 0
	if(sys_status&SS_CAP	/* Writes to Capture File */
		&& (sys_status&SS_ANSCAP || (ch!=ESC /* && !lclaes() */)))
		fwrite(&ch,1,1,capfile);
#endif
#if 0 
	if(console&CON_L_ECHO) {
		if(console&CON_L_ECHOX && (uchar)ch>SP)
			putch(password_char);
		else if(cfg.node_misc&NM_NOBEEP && ch==BEL);	 /* Do nothing if beep */
		else if(ch==BEL) {
				sbbs_beep(2000,110);
				nosound(); 
		}
		else putch(ch); 
	}
#endif

	if(online==ON_REMOTE && console&CON_R_ECHO) {
		if(console&CON_R_ECHOX && (uchar)ch>SP) {
			ch=text[YN][3];
			if(text[YN][2]==0 || ch==0) ch='X';
		}
		if(ch==FF && useron.misc&ANSI) {
			putcom("\x1b[2J\x1b[H");	/* clear screen, home cursor */
		}
		else {
			i=0;
			while(outcom(ch)&TXBOF && i<1440) { /* 3 minute pause delay */
				if(!online)
					break;
				i++;
				if(sys_status&SS_SYSPAGE)
					sbbs_beep(i,80);
				else
					mswait(80); 
			}
			if(i==1440) {							/* timeout - beep flush outbuf */
				i=rioctl(TXBC);
				lprintf("timeout(outchar) %04X %04X\r\n",i,rioctl(IOFO));
				outcom(BEL);
				rioctl(IOCS|PAUSE); 
			} 
		} 
	}
	if(ch==LF) {
		lncntr++;
		lbuflen=0;
		tos=0;
	} else if(ch==FF) {
		lncntr=0;
		lbuflen=0;
		tos=1;
	} else {
		if(!lbuflen)
			latr=curatr;
		if(lbuflen<LINE_BUFSIZE)
			lbuf[lbuflen++]=ch; 
	}

	if(lncntr==rows-1 && ((useron.misc&UPAUSE) || sys_status&SS_PAUSEON) 
		&& !(sys_status&SS_PAUSEOFF)) {
		lncntr=0;
		pause(); 
	}
}

void sbbs_t::center(char *instr)
{
	char str[256];
	int i,j;

	SAFECOPY(str,instr);
	truncsp(str);
	j=bstrlen(str);
	for(i=0;i<(80-j)/2;i++)
		outchar(SP);
	bputs(str);
	CRLF;
}

void sbbs_t::clearline(void)
{
	int i;

	outchar(CR);
	if(useron.misc&ANSI)
		rputs("\x1b[K");
	else {
		for(i=0;i<cols-1;i++)
			outchar(SP);
		outchar(CR); 
	}
}

void sbbs_t::cursor_home(void)
{
	if(useron.misc&ANSI)
		rputs("\x1b[H");
	else
		outchar(FF);
}

void sbbs_t::cursor_up(int count)
{
	if(!(useron.misc&ANSI))
		return;
	if(count>1)
		rprintf("\x1b[%dA",count);
	else
		rputs("\x1b[A");
}

void sbbs_t::cursor_down(int count)
{
	if(!(useron.misc&ANSI))
		return;
	if(count>1)
		rprintf("\x1b[%dB",count);
	else
		rputs("\x1b[B");
}

void sbbs_t::cursor_right(int count)
{
	if(useron.misc&ANSI) {
		if(count>1)
			rprintf("\x1b[%dC",count);
		else
			rputs("\x1b[C");
	} else {
		for(int i=0;i<count;i++)
			outchar(SP);
	}
}

void sbbs_t::cursor_left(int count)
{
	if(useron.misc&ANSI) {
		if(count>1)
			rprintf("\x1b[%dD",count);
		else
			rputs("\x1b[D");
	} else {
		for(int i=0;i<count;i++)
			outchar('\b');
	}
}

void sbbs_t::cleartoeol(void)
{
	if(useron.misc&ANSI)
		rputs("\x1b[K");
#if 0
	else {
		i=j=lclwx();	/* commented out */
		while(i++<79)
			outchar(SP);
		while(j++<79)
			outchar(BS); 
	}
#endif                
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
		if(useron.misc&ANSI)
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
		case '+':	/* push current attribte */
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
			bprintf("%02d:%02d %s"
				,tm.tm_hour==0 ? 12
				: tm.tm_hour>12 ? tm.tm_hour-12
				: tm.tm_hour, tm.tm_min, tm.tm_hour>11 ? "pm":"am");
			break;
		case 'D':   /* Date */
			now=time(NULL);
			bputs(unixtodstr(&cfg,now,tmp1));
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
		case 'L':	/* CLS (form feed) */
			CLS;
			break;
		case '>':   /* CLREOL */
			cleartoeol();
			break;
		case '<':   /* Non-destructive backspace */
			outchar(BS);
			break;
		case '[':   /* Carriage return */
			outchar(CR);
			break;
		case ']':   /* Line feed */
			outchar(LF);
			break;
		case 'A':   /* Ctrl-A */
			outchar(CTRL_A);
			break;
		case 'H': 	/* High intensity */
			atr|=HIGH;
			attr(atr);
			break;
		case 'I':	/* Blink */
			atr|=BLINK;
			attr(atr);
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

/***************************************************************************/
/* Changes local and remote text attributes accounting for monochrome      */
/***************************************************************************/
/****************************************************************************/
/* Sends ansi codes to change remote ansi terminal's colors                 */
/* Only sends necessary codes - tracks remote terminal's current attributes */
/* through the 'curatr' variable                                            */
/****************************************************************************/
void sbbs_t::attr(int atr)
{

	if(!(useron.misc&ANSI))
		return;
	if(!(useron.misc&COLOR)) {  /* eliminate colors if user doesn't have them */
		if(atr&LIGHTGRAY)       /* if any foreground bits set, set all */
			atr|=LIGHTGRAY;
		if(atr&BG_LIGHTGRAY)  /* if any background bits set, set all */
			atr|=BG_LIGHTGRAY;
		if(atr&LIGHTGRAY && atr&BG_LIGHTGRAY)
			atr&=~LIGHTGRAY;    /* if background is solid, foreground is black */
		if(!atr)
			atr|=LIGHTGRAY;		/* don't allow black on black */
	}
	if(curatr==atr) /* text hasn't changed. don't send codes */
		return;

	if((!(atr&HIGH) && curatr&HIGH) || (!(atr&BLINK) && curatr&BLINK)
		|| atr==LIGHTGRAY) {
		rputs(ansi(ANSI_NORMAL));
		curatr=LIGHTGRAY; 
	}

	if(atr==LIGHTGRAY)                  /* no attributes */
		return;

	if(atr&BLINK) {                     /* special attributes */
		if(!(curatr&BLINK))
			rputs(ansi(BLINK)); 
	}
	if(atr&HIGH) {
		if(!(curatr&HIGH))
			rputs(ansi(HIGH)); 
	}

	if((atr&0x7)==BLACK) {              /* foreground colors */
		if((curatr&0x7)!=BLACK)
			rputs(ansi(BLACK)); 
	}
	else if((atr&0x7)==RED) {
		if((curatr&0x7)!=RED)
			rputs(ansi(RED)); 
	}
	else if((atr&0x7)==GREEN) {
		if((curatr&0x7)!=GREEN)
			rputs(ansi(GREEN)); 
	}
	else if((atr&0x7)==BROWN) {
		if((curatr&0x7)!=BROWN)
			rputs(ansi(BROWN)); 
	}
	else if((atr&0x7)==BLUE) {
		if((curatr&0x7)!=BLUE)
			rputs(ansi(BLUE)); 
	}
	else if((atr&0x7)==MAGENTA) {
		if((curatr&0x7)!=MAGENTA)
			rputs(ansi(MAGENTA)); 
	}
	else if((atr&0x7)==CYAN) {
		if((curatr&0x7)!=CYAN)
			rputs(ansi(CYAN)); 
	}
	else if((atr&0x7)==LIGHTGRAY) {
		if((curatr&0x7)!=LIGHTGRAY)
			rputs(ansi(LIGHTGRAY)); 
	}

	if((atr&0x70)==0) {        /* background colors */
		if((curatr&0x70)!=0)
			rputs(ansi(BG_BLACK)); 
	}
	else if((atr&0x70)==BG_RED) {
		if((curatr&0x70)!=BG_RED)
			rputs(ansi(BG_RED)); 
	}
	else if((atr&0x70)==BG_GREEN) {
		if((curatr&0x70)!=BG_GREEN)
			rputs(ansi(BG_GREEN)); 
	}
	else if((atr&0x70)==BG_BROWN) {
		if((curatr&0x70)!=BG_BROWN)
			rputs(ansi(BG_BROWN)); 
	}
	else if((atr&0x70)==BG_BLUE) {
		if((curatr&0x70)!=BG_BLUE)
			rputs(ansi(BG_BLUE)); 
	}
	else if((atr&0x70)==BG_MAGENTA) {
		if((curatr&0x70)!=BG_MAGENTA)
			rputs(ansi(BG_MAGENTA)); 
	}
	else if((atr&0x70)==BG_CYAN) {
		if((curatr&0x70)!=BG_CYAN)
			rputs(ansi(BG_CYAN)); 
	}
	else if((atr&0x70)==BG_LIGHTGRAY) {
		if((curatr&0x70)!=BG_LIGHTGRAY)
			rputs(ansi(BG_LIGHTGRAY)); 
	}

	curatr=atr;
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


