/* Synchronet ANSI terminal functions */

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

#define TIMEOUT_ANSI_GETXY	5	// Seconds

/****************************************************************************/
/* Returns the ANSI code to obtain the value of atr. Mixed attributes		*/
/* high intensity colors, or background/foreground combinations don't work. */
/* A call to attr() is more appropriate, being it is intelligent			*/
/****************************************************************************/
const char *sbbs_t::ansi(int atr)
{

	switch(atr) {

		/* Special case */
		case ANSI_NORMAL:
			return("\x1b[0m");
		case BLINK:
		case BG_BRIGHT:
			return("\x1b[5m");

		/* Foreground */
		case HIGH:
			return("\x1b[1m");
		case BLACK:
			return("\x1b[30m");
		case RED:
			return("\x1b[31m");
		case GREEN:
			return("\x1b[32m");
		case BROWN:
			return("\x1b[33m");
		case BLUE:
			return("\x1b[34m");
		case MAGENTA:
			return("\x1b[35m");
		case CYAN:
			return("\x1b[36m");
		case LIGHTGRAY:
			return("\x1b[37m");

		/* Background */
		case BG_BLACK:
			return("\x1b[40m");
		case BG_RED:
			return("\x1b[41m");
		case BG_GREEN:
			return("\x1b[42m");
		case BG_BROWN:
			return("\x1b[43m");
		case BG_BLUE:
			return("\x1b[44m");
		case BG_MAGENTA:
			return("\x1b[45m");
		case BG_CYAN:
			return("\x1b[46m");
		case BG_LIGHTGRAY:
			return("\x1b[47m"); 
	}

	return("-Invalid use of ansi()-");
}

/* insure str is at least 14 bytes in size! */
extern "C" char* ansi_attr(int atr, int curatr, char* str, BOOL color)
{
	if(!color) {  /* eliminate colors if terminal doesn't support them */
		if(atr&LIGHTGRAY)       /* if any foreground bits set, set all */
			atr|=LIGHTGRAY;
		if(atr&BG_LIGHTGRAY)  /* if any background bits set, set all */
			atr|=BG_LIGHTGRAY;
		if((atr&LIGHTGRAY) && (atr&BG_LIGHTGRAY))
			atr&=~LIGHTGRAY;    /* if background is solid, foreground is black */
		if(!atr)
			atr|=LIGHTGRAY;		/* don't allow black on black */
	}
	if(curatr==atr) { /* text hasn't changed. no sequence needed */
		*str=0;
		return str;
	}

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
	if(strlen(str)==2)	/* Convert <ESC>[ to blank */
		*str=0;
	else
		str[strlen(str)-1]='m';
	return str;
}

char* sbbs_t::ansi(int atr, int curatr, char* str)
{
	long term = term_supports();
	if(term&ICE_COLOR) {
		switch(atr&(BG_BRIGHT|BLINK)) {
			case BG_BRIGHT:
			case BLINK:
				atr ^= BLINK;
				break;
		}
	}
	return ::ansi_attr(atr, curatr, str, (term&COLOR) ? TRUE:FALSE);
}

void sbbs_t::ansi_getlines()
{
	if(sys_status&SS_USERON && useron.misc&ANSI
		&& (useron.rows == TERM_ROWS_AUTO || useron.cols == TERM_COLS_AUTO)
		&& online==ON_REMOTE) {									/* Remote */
		SYNC;
		putcom("\x1b[s\x1b[255B\x1b[255C\x1b[6n\x1b[u");
		inkey(K_ANSI_CPR,TIMEOUT_ANSI_GETXY*1000); 
	}
}

bool sbbs_t::ansi_getxy(int* x, int* y)
{
	size_t	rsp=0;
	int		ch;
	char	str[128];

	if(x != NULL)
		*x=0;
	if(y != NULL)
		*y=0;

	putcom("\x1b[6n");	/* Request cursor position */

    time_t start=time(NULL);
    sys_status&=~SS_ABORT;
    while(online && !(sys_status&SS_ABORT) && rsp < sizeof(str)) {
		if((ch=incom(1000))!=NOINP) {
			str[rsp] = ch;
			if(ch==ESC && rsp==0) {
            	rsp++;
				start=time(NULL);
			}
            else if(ch=='[' && rsp==1) {
            	rsp++;
				start=time(NULL);
			}
            else if(IS_DIGIT(ch) && rsp==2) {
				if(y!=NULL) {
               		(*y)*=10;
					(*y)+=(ch&0xf);
				}
				start=time(NULL);
            }
            else if(ch==';' && rsp>=2) {
            	rsp++;
				start=time(NULL);
			}
            else if(IS_DIGIT(ch) && rsp==3) {
				if(x!=NULL) {
            		(*x)*=10;
					(*x)+=(ch&0xf);
				}
				start=time(NULL);
            }
            else if(ch=='R' && rsp)
            	break;
			else {
				str[rsp] = 0;
#ifdef _DEBUG
				char dbg[128];
				c_escape_str(str, dbg, sizeof(dbg), /* Ctrl-only? */true);
				lprintf(LOG_DEBUG, "Unexpected ansi_getxy response: '%s'", dbg);
#endif
				ungetstr(str, /* insert */false);
				rsp = 0;
			}
        }
    	if(time(NULL)-start>TIMEOUT_ANSI_GETXY) {
        	lprintf(LOG_NOTICE, "!TIMEOUT in ansi_getxy");
            return(false);
        }
    }

	return(true);
}

bool sbbs_t::ansi_gotoxy(int x, int y)
{
	if(term_supports(ANSI)) {
		comprintf("\x1b[%d;%dH",y,x);
		if(x>0)
			column=x-1;
		if(y>0)
			row = y - 1;
		lncntr=0;
		return true;
	}
	return false;
}

bool sbbs_t::ansi_save(void)
{
	if(term_supports(ANSI)) {
		putcom("\x1b[s");
		return true;
	}
	return false;
}

bool sbbs_t::ansi_restore(void)
{
	if(term_supports(ANSI)) {
		putcom("\x1b[u");
		return true;
	}
	return false;
}

int sbbs_t::ansi_mouse(enum ansi_mouse_mode mode, bool enable)
{
	char str[32] = "";
	SAFEPRINTF2(str, "\x1b[?%u%c", mode, enable ? 'h' : 'l');
	return putcom(str);
}
