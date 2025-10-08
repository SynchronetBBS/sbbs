/* Synchronet for *nix node spy */

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

#include <stdlib.h>
#include "sockwrap.h"	/* Must go before <sys/un.h> */
#ifndef _WIN32
#include <sys/un.h>
#endif
#include <stdio.h>
#include <string.h>

#include <genwrap.h>
#include <sockwrap.h>
#include "ini_file.h"
#include "spyon.h"
#include "ciolib.h"
#include "cterm.h"
#include "uifc.h"
#include "str_util.h"

struct cterminal *cterm;
extern uifcapi_t uifc; /* User Interface (UIFC) Library API */
cterm_emulation_t cterm_emu = CTERM_EMULATION_ANSI_BBS;
char term_type[INI_MAX_VALUE_LEN] = "unknown";
int term_cols = 80;
int term_rows = 25;
bool term_utf8 = false;

time_t read_term_ini(const char* path)
{
	FILE* fp = iniOpenFile(path, /* for_modify: */false);
	if(fp != NULL) {
		char chars[INI_MAX_VALUE_LEN];
		iniReadString(fp, ROOT_SECTION, "type", /* default: */NULL, term_type);
		iniReadString(fp, ROOT_SECTION, "chars", /* default: */NULL, chars);
		term_cols = iniReadInteger(fp, ROOT_SECTION, "cols", term_cols);
		term_rows = iniReadInteger(fp, ROOT_SECTION, "rows", term_rows);
		term_utf8 = false;
		if(stricmp(term_type, "PETSCII") == 0)
			cterm_emu = CTERM_EMULATION_PETASCII;
		else {
			cterm_emu = CTERM_EMULATION_ANSI_BBS;
			if(stricmp(chars, "UTF-8") == 0)
				term_utf8 = true;
		}
		fclose(fp);
	}
	return fdate(path);
}

int spyon(char *sockname, int nodenum, scfg_t* cfg)  {
#if defined _WIN32
	uifc.msg("Spying not supported on Win32 yet!");
	return SPY_SOCKETLOST;
#else
	SOCKET		spy_sock=INVALID_SOCKET;
	struct sockaddr_un spy_name;
	socklen_t	spy_len;
	unsigned char		key;
	char	buf[100000];
	char	term_ini_fname[MAX_PATH + 1];
	time_t	term_ini_ftime;
	int		idle_count = 0;
	int		i;
	fd_set	rd;
	bool	b;
	int		retval=0;
	struct text_info ti;
	char *scrn;

	/* ToDo Test for it actually being a socket! */
	/* Well, it will fail to connect won't it?   */

	if((spy_sock=socket(PF_UNIX,SOCK_STREAM,0))==INVALID_SOCKET)  {
		printf("ERROR %d creating local spy socket", errno);
		return(SPY_NOSOCKET);
	}
	
	snprintf(term_ini_fname, sizeof term_ini_fname, "%sterminal.ini"
		,cfg->node_path[nodenum - 1]);

	term_ini_ftime = read_term_ini(term_ini_fname);

	spy_name.sun_family=AF_UNIX;
	SAFECOPY(spy_name.sun_path,sockname);
#ifdef SUN_LEN
	spy_len=SUN_LEN(&spy_name);
#else
	spy_len=sizeof(struct sockaddr_un);
#endif
	if(connect(spy_sock,(struct sockaddr *)&spy_name,spy_len))  {
		closesocket(spy_sock);
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
	cprintf("Spying on node %d ... press CTRL-C to return to monitor", nodenum);
	clreol();
	cterm = cterm_init(ti.screenheight - 1, ti.screenwidth, 1, 1, 0, 0, NULL, cterm_emu);
	while(spy_sock!=INVALID_SOCKET && cterm != NULL)  {
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
			if((i=read(spy_sock, buf, sizeof(buf) - 1)) > 0)  {
				if(idle_count >= 1000 && fdate(term_ini_fname) > term_ini_ftime) {
					term_ini_ftime = read_term_ini(term_ini_fname);
					if(cterm_emu != cterm->emulation) {
						cterm_end(cterm, 1);
						cterm = cterm_init(ti.screenheight - 1, ti.screenwidth, 1, 1, 0, 0, NULL, cterm_emu);
					}
				}
				if(term_utf8) {
					buf[i] = '\0';
					utf8_to_cp437_inplace(buf);
					i = strlen(buf);
				}
				idle_count = 0;
				cterm_write(cterm, buf, i, NULL, 0, NULL);
			}
			else if(i<0) {
				close(spy_sock);
				spy_sock=INVALID_SOCKET;
				retval=SPY_SOCKETLOST;
				break;
			}
		} else {
			++idle_count;
		}
		if(kbhit())  {
			key=getch();
			/* Check for control keys */
			switch(key)  {
				case 3:	/* CTRL-C */
					close(spy_sock);
					spy_sock=INVALID_SOCKET;
					retval=SPY_CLOSED;
					break;
				case 0:		/* Extended keys */
				case 0xe0:
					key = getch();
					if (key != 0xe0)
						break;
					// Fall-through
				default:
					if(write(spy_sock,&key,1) != 1)
						perror("writing to spy socket");
			}
		}
	}
	cterm_end(cterm, 1);
	puttext(1,1,ti.screenwidth,ti.screenheight,scrn);
	window(ti.winleft,ti.wintop,ti.winright,ti.winbottom);
	textattr(ti.attribute);
	gotoxy(ti.curx,ti.cury);
	return(retval);
#endif
}

