/* getstr.cpp */

/* Synchronet string input routines */

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

/****************************************************************************/
/* Waits for remote or local user to input a CR terminated string. 'length' */
/* is the maximum number of characters that getstr will allow the user to   */
/* input into the string. 'mode' specifies upper case characters are echoed */
/* or wordwrap or if in message input (^A sequences allowed). ^W backspaces */
/* a word, ^X backspaces a line, ^Gs, BSs, TABs are processed, LFs ignored. */
/* ^N non-destructive BS, ^V center line. Valid keys are echoed.            */
/****************************************************************************/
size_t sbbs_t::getstr(char *strout, size_t maxlen, long mode)
{
    size_t	i,l,x,z;    /* i=current position, l=length, j=printed chars */
                    /* x&z=misc */
	char	str1[256],str2[256];
    uchar	ch;
	uchar	ins=0,atr;

	console&=~CON_UPARROW;
	sys_status&=~SS_ABORT;
	if(mode&K_LINE && useron.misc&ANSI && !(mode&K_NOECHO)) {
		attr(cfg.color[clr_inputline]);
		for(i=0;i<maxlen;i++)
			outchar(SP);
		bprintf("\x1b[%dD",maxlen); }
	if(wordwrap[0]) {
		strcpy(str1,wordwrap);
		wordwrap[0]=0; }
	else str1[0]=0;
	if(mode&K_EDIT)
		strcat(str1,strout);
	else
		strout[0]=0;
	if(strlen(str1)>maxlen)
		str1[maxlen]=0;
	atr=curatr;
	if(!(mode&K_NOECHO)) {
		if(mode&K_AUTODEL && str1[0]) {
			i=(cfg.color[clr_inputline]&0x77)<<4;
			i|=(cfg.color[clr_inputline]&0x77)>>4;
			attr(i); }
		rputs(str1);
		if(mode&K_EDIT && !(mode&(K_LINE|K_AUTODEL)) && useron.misc&ANSI)
			bputs("\x1b[K");  /* destroy to eol */ }

	i=l=strlen(str1);
	if(mode&K_AUTODEL && str1[0] && !(mode&K_NOECHO)) {
		ch=getkey(mode|K_GETSTR);
		attr(atr);
		if(isprint(ch) || ch==DEL) {
			for(i=0;i<l;i++)
				bputs("\b \b");
			i=l=0; }
		else {
			for(i=0;i<l;i++)
				outchar(BS);
			rputs(str1);
			i=l; }
		if(ch!=SP && ch!=TAB)
			ungetkey(ch); }

	while(!(sys_status&SS_ABORT) && (ch=getkey(mode|K_GETSTR))!=CR && online) {
		if(!input_thread_running)
			break;
		if(sys_status&SS_ABORT)
			break;
		if(ch==LF && mode&K_MSG) /* Down-arrow same as CR */
			break;
		if(ch==TAB && (mode&K_TAB || !(mode&K_WRAP)))	/* TAB same as CR */
			break;
		if(!i && mode&K_UPRLWR && (ch==SP || ch==TAB))
			continue;	/* ignore beginning white space if upper/lower */
		if(mode&K_E71DETECT && (uchar)ch==(CR|0x80) && l>1) {
			if(strstr(str1,"")) {
				bputs("\r\n\r\nYou must set your terminal to NO PARITY, "
					"8 DATA BITS, and 1 STOP BIT (N-8-1).\7\r\n");
				return(0); } }
		switch(ch) {
			case CTRL_A: /* Ctrl-A for ANSI */
				if(!(mode&K_MSG) || useron.rest&FLAG('A') || i>maxlen-3)
					break;
				if(ins) {
					if(l<maxlen)
						l++;
					for(x=l;x>i;x--)
						str1[x]=str1[x-1];
					rprintf("%.*s",l-i,str1+i);
					rprintf("\x1b[%dD",l-i);
					if(i==maxlen-1)
						ins=0; }
				outchar(str1[i++]=1);
				break;
			case CTRL_B: /* Ctrl-B Beginning of Line */
				if(useron.misc&ANSI && i && !(mode&K_NOECHO)) {
					bprintf("\x1b[%dD",i);
					i=0; }
				break;
			case CTRL_D: /* Ctrl-D Delete word right */
				if(i<l) {
					x=i;
					while(x<l && str1[x]!=SP) {
						outchar(SP);
						x++; }
					while(x<l && str1[x]==SP) {
						outchar(SP);
						x++; }
					bprintf("\x1b[%dD",x-i);   /* move cursor back */
					z=i;
					while(z<l-(x-i))  {             /* move chars in string */
						outchar(str1[z]=str1[z+(x-i)]);
						z++; }
					while(z<l) {                    /* write over extra chars */
						outchar(SP);
						z++; }
					bprintf("\x1b[%dD",z-i);
					l-=x-i; }                       /* l=new length */
				break;
			case CTRL_E: /* Ctrl-E End of line */
				if(useron.misc&ANSI && i<l) {
					bprintf("\x1b[%dC",l-i);  /* move cursor to eol */
					i=l; }
				break;
			case CTRL_F: /* Ctrl-F move cursor forewards */
				if(i<l && (useron.misc&ANSI)) {
					bputs("\x1b[C");   /* move cursor right one */
					i++; }
				break;
			case CTRL_G: /* Bell */
				if(!(mode&K_MSG))
					break;
				if(useron.rest&FLAG('B')) {
					if (i+6<maxlen) {
						if(ins) {
							for(x=l+6;x>i;x--)
								str1[x]=str1[x-6];
							if(l+5<maxlen)
								l+=6;
							if(i==maxlen-1)
								ins=0; }
						str1[i++]='(';
						str1[i++]='b';
						str1[i++]='e';
						str1[i++]='e';
						str1[i++]='p';
						str1[i++]=')';
						if(!(mode&K_NOECHO))
							bputs("(beep)"); }
					if(ins)
						redrwstr(str1,i,l,0);
					break; }
				 if(ins) {
					if(l<maxlen)
						l++;
					for(x=l;x>i;x--)
						str1[x]=str1[x-1];
					if(i==maxlen-1)
						ins=0; }
				 if(i<maxlen) {
					str1[i++]=BEL;
					if(!(mode&K_NOECHO))
						outchar(BEL); }
				 break;
			case CTRL_H:	/* Ctrl-H/Backspace */
				if(!i)
					break;
				i--;
				l--;
				if(i!=l) {              /* Deleting char in middle of line */
					outchar(BS);
					z=i;
					while(z<l)  {       /* move the characters in the line */
						outchar(str1[z]=str1[z+1]);
						z++; }
					outchar(SP);        /* write over the last char */
					bprintf("\x1b[%dD",(l-i)+1); }
				else if(!(mode&K_NOECHO))
					bputs("\b \b");
				break;
			case CTRL_I:	/* Ctrl-I/TAB */
				if(!(i%EDIT_TABSIZE)) {
					if(ins) {
						if(l<maxlen)
							l++;
						for(x=l;x>i;x--)
							str1[x]=str1[x-1];
						if(i==maxlen-1)
							ins=0; }
					str1[i++]=SP;
					if(!(mode&K_NOECHO))
						outchar(SP); }
				while(i<maxlen && i%EDIT_TABSIZE) {
					if(ins) {
						if(l<maxlen)
							l++;
						for(x=l;x>i;x--)
							str1[x]=str1[x-1];
						if(i==maxlen-1)
							ins=0; }
					str1[i++]=SP;
					if(!(mode&K_NOECHO))
						outchar(SP); }
				if(ins && !(mode&K_NOECHO))
					redrwstr(str1,i,l,0);
				break;

			case CTRL_L:    /* Ctrl-L   Center line (used to be Ctrl-V) */
				str1[l]=0;
				l=bstrlen(str1);
				if(!l) break;
				for(x=0;x<(maxlen-l)/2;x++)
					str2[x]=SP;
				str2[x]=0;
				strcat(str2,str1);
				strcpy(strout,str2);
				l=strlen(strout);
				if(mode&K_NOECHO)
					return(l);
				if(mode&K_MSG)
					redrwstr(strout,i,l,K_MSG);
				else {
					while(i--)
						bputs("\b");
					bputs(strout);
					if(mode&K_LINE)
						attr(LIGHTGRAY); }
				CRLF;
				return(l);

			case CTRL_N:    /* Ctrl-N Next word */
				if(i<l && (useron.misc&ANSI)) {
					x=i;
					while(str1[i]!=SP && i<l)
						i++;
					while(str1[i]==SP && i<l)
						i++;
					bprintf("\x1b[%dC",i-x); }
				break;
			case CTRL_R:    /* Ctrl-R Redraw Line */
				if(!(mode&K_NOECHO))
					redrwstr(str1,i,l,0);
				break;
			case CTRL_V:	/* Ctrl-V			Toggles Insert/Overwrite */
				if(!(useron.misc&ANSI) || mode&K_NOECHO)
					break;
				if(ins) {
					ins=0;
					redrwstr(str1,i,l,0); }
				else if(i<l) {
					ins=1;
					bprintf("\x1b[s\x1b[79C");    	/* save pos  */
					z=curatr;                       /* and go to EOL */
					attr(z|BLINK|HIGH);
					outchar('°');
					attr(z);
					bputs("\x1b[u"); }  /* restore pos */
				break;
			case CTRL_W:    /* Ctrl-W   Delete word left */
				if(i<l) {
					x=i;                            /* x=original offset */
					while(i && str1[i-1]==SP) {
						outchar(BS);
						i--; }
					while(i && str1[i-1]!=SP) {
						outchar(BS);
						i--; }
					z=i;                            /* i=z=new offset */
					while(z<l-(x-i))  {             /* move chars in string */
						outchar(str1[z]=str1[z+(x-i)]);
						z++; }
					while(z<l) {                    /* write over extra chars */
						outchar(SP);
						z++; }
					bprintf("\x1b[%dD",z-i);        /* back to new x corridnant */
					l-=x-i; }                       /* l=new length */
				else {
					while(i && str1[i-1]==SP) {
						i--;
						l--;
						if(!(mode&K_NOECHO))
							bputs("\b \b"); }
					while(i && str1[i-1]!=SP) {
						i--;
						l--;
						if(!(mode&K_NOECHO))
							bputs("\b \b"); } }
				break;
			case CTRL_X:    /* Ctrl-X   Delete entire line */
				if(mode&K_NOECHO)
					l=0;
				else {
					while(i<l) {
						outchar(SP);
						i++; }
					while(l) {
						l--;
						bputs("\b \b"); } }
				i=0;
				break;
			case CTRL_Y:    /* Ctrl-Y   Delete to end of line */
				if(useron.misc&ANSI && !(mode&K_NOECHO)) {
					bputs("\x1b[K");
					l=i; }
				break;
			case 28:    /* Ctrl-\ Previous word */
				if(i && (useron.misc&ANSI) && !(mode&K_NOECHO)) {
					x=i;
					while(str1[i-1]==SP && i)
						i--;
					while(str1[i-1]!=SP && i)
						i--;
					bprintf("\x1b[%dD",x-i); }
				break;
			case 29:  /* Ctrl-]/Left Arrow  Reverse Cursor Movement */
				if(i && (useron.misc&ANSI) && !(mode&K_NOECHO)) {
					bputs("\x1b[D");   /* move cursor left one */
					i--; }
				break;
			case 30:  /* Ctrl-^/Up Arrow */
				if(!(mode&K_EDIT))
					break;
				if(i>l)
					l=i;
				str1[l]=0;
				strcpy(strout,str1);
				if((strip_invalid_attr(strout) || ins) && !(mode&K_NOECHO))
					redrwstr(strout,i,l,K_MSG);
				if(mode&K_LINE && !(mode&K_NOECHO))
					attr(LIGHTGRAY);
				console|=CON_UPARROW;
				return(l);
			case DEL:  /* Ctrl-BkSpc (DEL) Delete current char */
				if(i==l) {	/* Backspace if end of line */
					if(i) {
						i--;
						l--;
						if(!(mode&K_NOECHO))
							bputs("\b \b");
					}
					break;
				}
				l--;
				z=i;
				while(z<l)  {       /* move the characters in the line */
					outchar(str1[z]=str1[z+1]);
					z++; }
				outchar(SP);        /* write over the last char */
				bprintf("\x1b[%dD",(l-i)+1);
				break;
			default:
				if(mode&K_WRAP && i==maxlen && ch>=SP && !ins) {
					str1[i]=0;
					if(ch==SP && !(mode&K_CHAT)) { /* don't wrap a space */ 
						strcpy(strout,str1);	   /* as last char */
						if(strip_invalid_attr(strout) && !(mode&K_NOECHO))
							redrwstr(strout,i,l,K_MSG);
						if(!(mode&K_NOECHO))
							CRLF;
						return(i); }
					x=i-1;
					z=1;
					wordwrap[0]=ch;
					while(str1[x]!=SP && x)
						wordwrap[z++]=str1[x--];
					if(x<(maxlen/2)) {
						wordwrap[1]=0;  /* only wrap one character */
						strcpy(strout,str1);
						if(strip_invalid_attr(strout) && !(mode&K_NOECHO))
							redrwstr(strout,i,l,K_MSG);
						if(!(mode&K_NOECHO))
							CRLF;
						return(i); }
					wordwrap[z]=0;
					if(!(mode&K_NOECHO))
						while(z--) {
							bputs("\b \b");
							i--; }
					strrev(wordwrap);
					str1[x]=0;
					strcpy(strout,str1);
					if(strip_invalid_attr(strout) && !(mode&K_NOECHO))
						redrwstr(strout,i,x,(char)mode);
					if(!(mode&K_NOECHO))
						CRLF;
					return(x); }
				if(i<maxlen && ch>=SP) {
					if(mode&K_UPRLWR)
						if(!i || (i && (str1[i-1]==SP || str1[i-1]=='-'
							|| str1[i-1]=='.' || str1[i-1]=='_')))
							ch=toupper(ch);
						else
							ch=tolower(ch);
					if(ins) {
						if(l<maxlen)    /* l<maxlen */
							l++;
						for(x=l;x>i;x--)
							str1[x]=str1[x-1];
						rprintf("%.*s",l-i,str1+i);
						rprintf("\x1b[%dD",l-i);
						if(i==maxlen-1) {
							bputs("  \b\b");
							ins=0; } }
					str1[i++]=ch;
					if(!(mode&K_NOECHO))
						outchar(ch); } }
		if(i>l)
			l=i;
		if(mode&K_CHAT && !l)
			return(0); }
	if(!online)
		return(0);
	if(i>l)
		l=i;
	str1[l]=0;
	if(!(sys_status&SS_ABORT)) {
		strcpy(strout,str1);
		if((strip_invalid_attr(strout) || ins) && !(mode&K_NOECHO))
			redrwstr(strout,i,l,K_MSG); }
	else
		l=0;
	if(mode&K_LINE && !(mode&K_NOECHO)) attr(LIGHTGRAY);
	if(!(mode&(K_NOCRLF|K_NOECHO))) {
		outchar(CR);
		if(!(mode&K_MSG && sys_status&SS_ABORT))
			outchar(LF);
		lncntr=0; }
	return(l);
}

/****************************************************************************/
/* Hot keyed number input routine.                                          */
/* Returns a valid number between 1 and max, 0 if no number entered, or -1  */
/* if the user hit 'Q' or ctrl-c                                            */
/****************************************************************************/
long sbbs_t::getnum(ulong max)
{
    uchar ch,n=0;
	long i=0;

	while(online) {
		ch=getkey(K_UPPER);
		if(ch>0x7f)
			continue;
		if(ch=='Q') {
			outchar('Q');
			if(useron.misc&COLDKEYS)
				ch=getkey(K_UPPER);
			if(ch==BS || ch==DEL) {
				bputs("\b \b");
				continue; }
			CRLF;
			lncntr=0;
			return(-1); }
		else if(sys_status&SS_ABORT) {
			CRLF;
			lncntr=0;
			return(-1); }
		else if(ch==CR) {
			CRLF;
			lncntr=0;
			return(i); }
		else if((ch==BS || ch==DEL) && n) {
			bputs("\b \b");
			i/=10;
			n--; }
		else if(isdigit(ch) && (i*10UL)+(ch&0xf)<=max && (ch!='0' || n)) {
			i*=10L;
			n++;
			i+=ch&0xf;
			outchar(ch);
			if(i*10UL>max && !(useron.misc&COLDKEYS)) {
				CRLF;
				lncntr=0;
				return(i); } } }
	return(0);
}
