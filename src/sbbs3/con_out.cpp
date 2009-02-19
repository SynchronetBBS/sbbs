/* con_out.cpp */

/* Synchronet console output routines */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2009 Rob Swindell - http://www.synchro.net/copyright.html		*
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
/* Handles ctrl-a characters                                                */
/****************************************************************************/
int sbbs_t::bputs(const char *str)
{
	int i;
    ulong l=0;

	if(online==ON_LOCAL && console&CON_L_ECHO) 	/* script running as event */
		return(eprintf(LOG_INFO,"%s",str));

	while(str[l]) {
		if(str[l]==CTRL_A && str[l+1]!=0) {
			l++;
			if(toupper(str[l])=='Z')	/* EOF */
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

/****************************************************************************/
/* Outputs a NULL terminated string remotely (if applicable)				*/
/* Does not expand ctrl-A codes or track line counter, etc. (raw)           */
/****************************************************************************/
int sbbs_t::rputs(const char *str)
{
    ulong l=0;

	if(online==ON_LOCAL && console&CON_L_ECHO)	/* script running as event */
		return(eprintf(LOG_INFO,"%s",str));
	
	while(str[l])
		outcom(str[l++]);
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
/* Outputs destructive backspace locally and remotely (if applicable),		*/
/****************************************************************************/
void sbbs_t::backspace(void)
{
	outcom('\b');
	outcom(' ');
	outcom('\b');
	if(column)
		column--;
}

/****************************************************************************/
/* Returns true if the user (or the yet-to-be-logged-in client) supports	*/
/* all of the specified terminal 'cmp_flags' (e.g. ANSI, COLOR, RIP).		*/
/* If no flags specified, returns all terminal flag bits supported			*/
/****************************************************************************/
long sbbs_t::term_supports(long cmp_flags)
{
	long flags = sys_status&SS_USERON ? useron.misc : autoterm;

	return(cmp_flags ? ((flags&cmp_flags)==cmp_flags) : (flags&TERM_FLAGS));
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
	else if(outchar_esc==2) {
		if(ch>='@' && ch<='~')
			outchar_esc++;
	}
	else
		outchar_esc=0;
	if(term_supports(NO_EXASCII) && ch&0x80)
		ch=exascii_to_ascii_char(ch);  /* seven bit table */
	if(ch==FF && lncntr>1 && !tos) {
		lncntr=0;
		CRLF;
		if(!(sys_status&SS_PAUSEOFF)) {
			pause();
			while(lncntr && online && !(sys_status&SS_ABORT))
				pause(); 
		}
	}

	if(online==ON_REMOTE && console&CON_R_ECHO) {
		if(console&CON_R_ECHOX && (uchar)ch>=' ' && !outchar_esc) {
			ch=text[YN][3];
			if(text[YN][2]==0 || ch==0) ch='X';
		}
		if(ch==FF && term_supports(ANSI)) {
			putcom("\x1b[2J\x1b[H");	/* clear screen, home cursor */
		}
		else {
			if(ch==(char)TELNET_IAC && !(telnet_mode&TELNET_MODE_OFF))
				outcom(TELNET_IAC);	/* Must escape Telnet IAC char (255) */
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
				lprintf(LOG_NOTICE,"timeout(outchar) %04X %04X\r\n",i,rioctl(IOFO));
				outcom(BEL);
				rioctl(IOCS|PAUSE); 
			} 
		} 
	}
	if(!outchar_esc) {
		if((uchar)ch>=' ')
			column++;
		else if(ch=='\r')
			column=0;
		else if(ch=='\b') {
			if(column)
				column--;
		}
	}
	if(ch==LF || column>=cols) {
		lncntr++;
		lbuflen=0;
		tos=0;
		column=0;
	} else if(ch==FF) {
		lncntr=0;
		lbuflen=0;
		tos=1;
		column=0;
	} else {
		if(!lbuflen)
			latr=curatr;
		if(lbuflen<LINE_BUFSIZE)
			lbuf[lbuflen++]=ch; 
	}
	if(outchar_esc==3)
		outchar_esc=0;

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
	for(i=0;i<(cols-j)/2;i++)
		outchar(' ');
	bputs(str);
	CRLF;
}

void sbbs_t::clearline(void)
{
	outcom(CR);
	column=0;
	cleartoeol();
}

void sbbs_t::cursor_home(void)
{
	if(term_supports(ANSI))
		rputs("\x1b[H");
	else
		outchar(FF);	/* this will clear some terminals, do nothing with others */
}

void sbbs_t::cursor_up(int count)
{
	if(count<1)
		return;
	if(!term_supports(ANSI))
		return;
	if(count>1)
		rprintf("\x1b[%dA",count);
	else
		rputs("\x1b[A");
}

void sbbs_t::cursor_down(int count)
{
	if(count<1)
		return;
	if(!term_supports(ANSI))
		return;
	if(count>1)
		rprintf("\x1b[%dB",count);
	else
		rputs("\x1b[B");
}

void sbbs_t::cursor_right(int count)
{
	if(count<1)
		return;
	if(term_supports(ANSI)) {
		if(count>1)
			rprintf("\x1b[%dC",count);
		else
			rputs("\x1b[C");
	} else {
		for(int i=0;i<count;i++)
			outcom(' ');
	}
	column+=count;
}

void sbbs_t::cursor_left(int count)
{
	if(count<1)
		return;
	if(term_supports(ANSI)) {
		if(count>1)
			rprintf("\x1b[%dD",count);
		else
			rputs("\x1b[D");
	} else {
		for(int i=0;i<count;i++)
			outcom('\b');
	}
	if(column > count)
		column-=count;
	else
		column=0;
}

void sbbs_t::cleartoeol(void)
{
	int i,j;

	if(term_supports(ANSI))
		rputs("\x1b[K");
	else {
		i=j=column;
		while(++i<cols)
			outcom(' ');
		while(++j<cols)
			outcom(BS); 
	}
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
	char	str[16];

	if(!term_supports(ANSI))
		return;
	if(!term_supports(COLOR)) {  /* eliminate colors if user doesn't have them */
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

#if 1
	strcpy(str,"\033[");
	if((!(atr&HIGH) && curatr&HIGH) || (!(atr&BLINK) && curatr&BLINK)
		|| atr==LIGHTGRAY) {
		strcat(str,"0;");
		curatr=LIGHTGRAY;
	}
	if(atr&BLINK) {                     /* special attributes */
		if(!(curatr&BLINK))
			strcat(str,"5;");
	}
	if(atr&HIGH) {
		if(!(curatr&HIGH))
			strcat(str,"1;"); 
	}
	if((atr&0x07) != (curatr&0x07)) {
		switch(atr&0x07) {
			case BLACK:
				strcat(str,"30;");
				break;
			case RED:
				strcat(str,"31;");
				break;
			case GREEN:
				strcat(str,"32;");
				break;
			case BROWN:
				strcat(str,"33;");
				break;
			case BLUE:
				strcat(str,"34;");
				break;
			case MAGENTA:
				strcat(str,"35;");
				break;
			case CYAN:
				strcat(str,"36;");
				break;
			case LIGHTGRAY:
				strcat(str,"37;");
				break;
		}
	}
	if((atr&0x70) != (curatr&0x70)) {
		switch(atr&0x70) {
			/* The BG_BLACK macro is 0x200, so isn't in the mask */
			case 0 /* BG_BLACK */:	
				strcat(str,"40;");
				break;
			case BG_RED:
				strcat(str,"41;");
				break;
			case BG_GREEN:
				strcat(str,"42;");
				break;
			case BG_BROWN:
				strcat(str,"43;");
				break;
			case BG_BLUE:
				strcat(str,"44;");
				break;
			case BG_MAGENTA:
				strcat(str,"45;");
				break;
			case BG_CYAN:
				strcat(str,"46;");
				break;
			case BG_LIGHTGRAY:
				strcat(str,"47;");
				break;
		}
	}
	if(strlen(str)==2)
		return;
	str[strlen(str)-1]='m';
	rputs(str);
#else
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
#endif

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


