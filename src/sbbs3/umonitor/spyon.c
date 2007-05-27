/* spyon.c */

/* Synchronet for *nix node spy */

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

#include <unistd.h>
#include <stdlib.h>
#include "sockwrap.h"	/* Must go before <sys/un.h> */
#include <sys/un.h>
#include <stdio.h>
#include <string.h>
#include "sockwrap.h"
#include "spyon.h"
#include "cterm.h"
#include "ciolib.h"

int spyon(char *sockname)  {
	SOCKET		spy_sock=INVALID_SOCKET;
	struct sockaddr_un spy_name;
	socklen_t	spy_len;
	unsigned char		key;
	unsigned char		buf;
	int		i;
	fd_set	rd;
	BOOL	b;
	int		retval=0;
	char	ANSIbuf[32];
	int		parsing=0;
	int		telnet_strip=0;
	struct text_info ti;
	char *scrn;

	/* ToDo Test for it actually being a socket! */
	/* Well, it will fail to connect won't it?   */

	if((spy_sock=socket(PF_UNIX,SOCK_STREAM,0))==INVALID_SOCKET)  {
		printf("ERROR %d creating local spy socket", errno);
		return(SPY_NOSOCKET);
	}
	
	spy_name.sun_family=AF_UNIX;
	SAFECOPY(spy_name.sun_path,sockname);
#ifdef SUN_LEN
	spy_len=SUN_LEN(&spy_name);
#else
	spy_len=sizeof(struct sockaddr_un);
#endif
	if(connect(spy_sock,(struct sockaddr *)&spy_name,spy_len))  {
		return(SPY_NOCONNECT);
	}
	i=1;

	gettextinfo(&ti);
	scrn=(char *)alloca(ti.screenwidth*ti.screenheight*2);
	gettext(1,1,ti.screenwidth,ti.screenheight,scrn);
	textcolor(YELLOW);
	textbackground(BLUE);
	clrscr();
	gotoxy(1,ti.screenheight);
	cputs("Local spy mode... press CTRL-C to return to monitor");
	clreol();
	cterm_init(ti.screenheight-1,ti.screenwidth,1,1,0,NULL,CTERM_EMULATION_ANSI_BBS);
	while(spy_sock!=INVALID_SOCKET)  {
		struct timeval tv;
		tv.tv_sec=0;
		tv.tv_usec=100;
		FD_ZERO(&rd);
		FD_SET(spy_sock,&rd);
		if((select(spy_sock+1,&rd,NULL,NULL,&tv))<0)  {
			close(spy_sock);
			spy_sock=INVALID_SOCKET;
			retval=SPY_SELECTFAILED;
			break;
		}
		if(!socket_check(spy_sock,NULL,&b,0)) {
			close(spy_sock);
			spy_sock=INVALID_SOCKET;
			retval=SPY_SOCKETLOST;
			break;
		}
		if(spy_sock != INVALID_SOCKET && FD_ISSET(spy_sock,&rd))  {
			if((i=read(spy_sock,&buf,1))==1)  {
				if(telnet_strip) {
					telnet_strip++;
					if(buf==255 && telnet_strip==2) {
						telnet_strip=0;
						cterm_write(&buf,1,NULL,0,NULL);
					}
					if(telnet_strip==3)
						telnet_strip=0;
				}
				else
					if(buf==255)
						telnet_strip=1;
					else
						cterm_write(&buf,1,NULL,0,NULL);
			}
			else if(i<0) {
				close(spy_sock);
				spy_sock=INVALID_SOCKET;
				retval=SPY_SOCKETLOST;
				break;
			}
		}
		if(kbhit())  {
			key=getch();
			/* Check for control keys */
			switch(key)  {
				case 0:		/* Extended keys */
				case 0xff:
					getch();
					break;
				case 3:	/* CTRL-C */
					close(spy_sock);
					spy_sock=INVALID_SOCKET;
					retval=SPY_CLOSED;
					break;
				default:
					write(spy_sock,&key,1);
			}
		}
	}
	puttext(1,1,ti.screenwidth,ti.screenheight,scrn);
	window(ti.winleft,ti.wintop,ti.winright,ti.winbottom);
	textattr(ti.attribute);
	gotoxy(ti.curx,ti.cury);
	return(retval);
}

