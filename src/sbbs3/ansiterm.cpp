/* ansiterm.cpp */

/* Synchronet ANSI terminal functions */

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

#include "sbbs.h"

#define TIMEOUT_ANSI_GETXY	5	// Seconds

/****************************************************************************/
/* Returns the ANSI code to obtain the value of atr. Mixed attributes		*/
/* high intensity colors, or background/forground cobinations don't work.   */
/* A call to attr is more appropriate, being it is intelligent				*/
/****************************************************************************/
char *sbbs_t::ansi(int atr)
{

	switch(atr) {

		/* Special case */
		case ANSI_NORMAL:
			return("\x1b[0m");
		case BLINK:
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

void sbbs_t::ansi_getlines()
{
	if(useron.misc&ANSI && !useron.rows         /* Auto-detect rows */
		&& online==ON_REMOTE) {                 /* Remote */
		SYNC;
		putcom("\x1b[s\x1b[99B\x1b[6n\x1b[u");
		inkey(K_NONE,TIMEOUT_ANSI_GETXY*1000); 
	}
}

bool sbbs_t::ansi_getxy(int* x, int* y)
{
	int 	rsp=0, ch;

    *x=0;
    *y=0;

	putcom("\x1b[6n");	/* Request cusor position */

    time_t start=time(NULL);
    sys_status&=~SS_ABORT;
    while(online && !(sys_status&SS_ABORT)) {
		if((ch=incom(1000))!=NOINP) {
			if(ch==ESC && rsp==0) {
            	rsp++;
				start=time(NULL);
			}
            else if(ch=='[' && rsp==1) {
            	rsp++;
				start=time(NULL);
			}
            else if(isdigit(ch) && rsp==2) {
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
            else if(isdigit(ch) && rsp==3) {
				if(x!=NULL) {
            		(*x)*=10;
					(*x)+=(ch&0xf);
				}
				start=time(NULL);
            }
            else if(ch=='R' && rsp)
            	break;
			else
				ungetkey(ch);
        }
    	if(time(NULL)-start>TIMEOUT_ANSI_GETXY) {
        	lprintf("Node %d !TIMEOUT in ansi_getxy", cfg.node_num);
            return(false);
        }
    }

	return(true);
}
