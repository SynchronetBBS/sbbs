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

#include <termios.h>
#include <unistd.h>
#include "sockwrap.h"	/* Must go before <sys/un.h> */
#include <sys/un.h>
#include <stdio.h>
#include <string.h>
#include "sockwrap.h"
#include "spyon.h"

int spyon(char *sockname)  {
	SOCKET		spy_sock=INVALID_SOCKET;
	struct sockaddr_un spy_name;
	socklen_t	spy_len;
	char		key;
	char		buf;
	int		i;
	struct	termios	tio;
	struct	termios	old_tio;
	fd_set	rd;
	BOOL	b;
	int		retval=0;
	char	ANSIbuf[32];
	int		parsing=0;
	int		telnet_strip=0;

	/* ToDo Test for it actually being a socket! */
	/* Well, it will fail to connect won't it?   */

	if((spy_sock=socket(PF_LOCAL,SOCK_STREAM,0))==INVALID_SOCKET)  {
		printf("ERROR %d creating local spy socket", errno);
		return(SPY_NOSOCKET);
	}
	
	spy_name.sun_family=AF_LOCAL;
	SAFECOPY(spy_name.sun_path,sockname);
	spy_len=SUN_LEN(&spy_name);
	if(connect(spy_sock,(struct sockaddr *)&spy_name,spy_len))  {
		return(SPY_NOCONNECT);
	}
	i=1;

	tcgetattr(STDIN_FILENO,&old_tio);
	tcgetattr(STDIN_FILENO,&tio);
	cfmakeraw(&tio);
	tcsetattr(STDIN_FILENO,TCSANOW,&tio);
	printf("\r\n\r\nLocal spy mode... press CTRL-C to return to monitor\r\n\r\n");
	while(spy_sock!=INVALID_SOCKET)  {
		FD_ZERO(&rd);
		FD_SET(STDIN_FILENO,&rd);
		FD_SET(spy_sock,&rd);
		if((select(spy_sock > STDIN_FILENO ? spy_sock+1 : STDIN_FILENO+1,&rd,NULL,NULL,NULL))<0)  {
			close(spy_sock);
			spy_sock=INVALID_SOCKET;
			break;
		}
		if(!socket_check(spy_sock,NULL,&b,0)) {
			close(spy_sock);
			spy_sock=INVALID_SOCKET;
			retval=SPY_SOCKETLOST;
			break;
		}
		if(FD_ISSET(STDIN_FILENO,&rd))  {
			if((i=read(STDIN_FILENO,&key,1))==1)  {
				/* Check for control keys */
				switch(key)  {
					case 3:	/* CTRL-C */
						close(spy_sock);
						spy_sock=INVALID_SOCKET;
						retval=SPY_CLOSED;
						break;
					default:
						if(parsing || key==27) { 	/* Escape */
							switch (key) {
								case 27:
									if(parsing) {
										ANSIbuf[parsing++]=key;
										write(spy_sock,ANSIbuf,parsing);
										parsing=0;
									}
									else
										ANSIbuf[parsing++]=key;
									break;
								case '[':
									if(parsing==1) {
										ANSIbuf[parsing++]=key;
										write(spy_sock,ANSIbuf,parsing);
										parsing=0;
									}
									else
										ANSIbuf[parsing++]=key;
									break;
								case '?':
								case ';':
								case '0'-'9':
									ANSIbuf[parsing++]=key;
									break;
								case 'R':			/* Cursor report... eat it. */
								case 'c':			/* VT-100 thing... eat it. */
								case 'n':			/* VT-100 thing... eat it. */
									parsing=0;
									break;
								default:
									ANSIbuf[parsing++]=key;
									write(spy_sock,ANSIbuf,parsing);
									parsing=0;
									break;
							}
						}
						else
							write(spy_sock,&key,1);
				}
			}
			else if(i<0) {
				close(spy_sock);
				spy_sock=INVALID_SOCKET;
				retval=SPY_STDINLOST;
				break;
			}
		}
		if(spy_sock != INVALID_SOCKET && FD_ISSET(spy_sock,&rd))  {
			if((i=read(spy_sock,&buf,1))==1)  {
				if(telnet_strip) {
					telnet_strip++;
					if(buf=='\xff' && telnet_strip==2) {
						telnet_strip=0;
						write(STDOUT_FILENO,&buf,1);
					}
					if(telnet_strip==3)
						telnet_strip=0;
				}
				else
					if(buf=='\xff')
						telnet_strip=1;
					else
						write(STDOUT_FILENO,&buf,1);
			}
			else if(i<0) {
				close(spy_sock);
				spy_sock=INVALID_SOCKET;
				retval=SPY_SOCKETLOST;
				break;
			}
		}
	}
	tcsetattr(STDIN_FILENO,TCSANOW,&old_tio);
	return(retval);
}

