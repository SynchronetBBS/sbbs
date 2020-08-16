/* Synchronet for *nix user editor */

/* $Id: uedit.c,v 1.64 2020/04/12 20:24:43 rswindell Exp $ */
// vi: tabstop=4

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

#include <sys/types.h>
#include <time.h>
#ifdef __QNX__
#include <string.h>
#endif
#include <stdio.h>
#ifdef __unix__
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>
#endif

#include "ciolib.h"
#include "curs_cio.h"
#undef OK
#include "sbbs.h"

#include "genwrap.h"
#include "uifc.h"
#include "sbbsdefs.h"
#include "genwrap.h"	/* stricmp */
#include "dirwrap.h"	/* lock/unlock/sopen */
#include "filewrap.h"	/* lock/unlock/sopen */
#include "sbbs_ini.h"	/* INI parsing */
#include "scfglib.h"	/* SCFG files */
#include "ars_defs.h"	/* needed for SCFG files */
#include "userdat.h"	/* getnodedat() */

#define CTRL(x) (x&037)

struct user_list {
	char	info[MAX_OPLN];
	int		usernum;
};

/********************/
/* Global Variables */
/********************/
uifcapi_t uifc; /* User Interface (UIFC) Library API */
char YesStr[]="Yes";
char NoStr[]="No";

#define GETUSERDAT(cfg, user)	if (getuserdat(cfg, user) != 0) { uifc.msg("Error reading user database!"); return -1; }

/*
 * Find the first occurrence of find in s, ignore case.
 * From FreeBSD src/lib/libc/string/strcasestr.c
 */
char *
strcasestr(const char *s, const char *find)
{
	char c, sc;
	size_t len;

	if ((c = *find++) != 0) {
		c = tolower((unsigned char)c);
		len = strlen(find);
		do {
			do {
				if ((sc = *s++) == 0)
					return (NULL);
			} while ((char)tolower((unsigned char)sc) != c);
		} while (strnicmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}

int lprintf(char *fmt, ...)
{
	va_list argptr;
	char sbuf[1024];
	int	len;

	va_start(argptr,fmt);
	len=vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
	va_end(argptr);
	uifc.msg(sbuf);
	return(len);
}

void bail(int code)
{
    if(code) {
        puts("\nHit a key...");
        getch();
	}
    uifc.bail();

    exit(code);
}

void allocfail(uint size)
{
    printf("\7Error allocating %u bytes of memory.\n",size);
    bail(1);
}

int confirm(char *prompt)
{
	int i=0;
	char *opt[3]={
		 YesStr
		,NoStr
		,""
	};

	i=uifc.list(WIN_MID|WIN_ACT|WIN_SAV,0,0,0,&i,0,prompt,opt);
	if(i==0)
		return(1);
	if(i==-1)
		return(-1);
	return(0);
}

/****************************************************************************/
/* Takes a string in the format HH:MM:SS and returns in seconds             */
/****************************************************************************/
time_t DLLCALL strtosec(char *str)
{
	char *p1;
	char *p2;
	long int hour=0;
	long int min=0;
	long int sec2=0;
	time_t	sec=0;

	hour=strtol(str,&p1,10);
	if(hour<0 || hour > 24)
		return(-1);
	if(*p1==':') {
		p1++;
		min=strtol(p1,&p2,10);
		if(min<0 || min > 59)
			return(-1);
		if(*p2==':') {
			p2++;
			sec2=strtol(p2,&p1,10);
			if(sec2 < 0 || sec2 > 59)
				return(-1);
		}
	}

	sec=hour*60*60;
	sec+=min*60;
	sec+=sec2;
	return(sec);
}

char *geteditor(char *edit)
{
	if(getenv("EDITOR")==NULL && (getenv("VISUAL")==NULL || getenv("DISPLAY")==NULL))
#ifdef __unix__
		strcpy(edit,"vi");
#else
		strcpy(edit,"notepad");
#endif
	else {
		if(getenv("DISPLAY")!=NULL && getenv("VISUAL")!=NULL)
			strcpy(edit,getenv("VISUAL"));
		else
			strcpy(edit,getenv("EDITOR"));
	}
	return(edit);
}

int do_cmd(char *cmd)
{
	int i;

#ifdef HAS_CURSES
	if(cio_api.mode == CIOLIB_MODE_CURSES || cio_api.mode == CIOLIB_MODE_CURSES_IBM)
		endwin();
#endif
	i=system(cmd);
#ifdef HAS_CURSES
	if(cio_api.mode == CIOLIB_MODE_CURSES || cio_api.mode == CIOLIB_MODE_CURSES_IBM)
		refresh();
#endif
	return(i);
}

/* Edit terminal settings
 *       Auto-Detect
 *       Extended ASCII
 *       ANSI
 *       Color
 *       RIP
 *       WIP
 *       Pause
 *       Hot Keys
 *       Spinning Cursor
 *       Number of Rows
 */
int edit_terminal(scfg_t *cfg, user_t *user)
{
	int 	i,j;
	char 	**opt;
	char	str[256];

	if((opt=(char **)alloca(sizeof(char *)*(10+1)))==NULL)
		allocfail(sizeof(char *)*(10+1));
	for(i=0;i<(10+1);i++)
		if((opt[i]=(char *)alloca(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	j=0;
	while(1) {
		GETUSERDAT(cfg,user);
		i=0;
		sprintf(opt[i++],"Auto-detect      %s",user->misc & AUTOTERM?"Yes":"No");
		sprintf(opt[i++],"Extended ASCII   %s",user->misc & NO_EXASCII?"No":"Yes");
		sprintf(opt[i++],"ANSI             %s",user->misc & ANSI?"Yes":"No");
		sprintf(opt[i++],"Color            %s",user->misc & COLOR?"Yes":"No");
		sprintf(opt[i++],"RIP              %s",user->misc & RIP?"Yes":"No");
		sprintf(opt[i++],"WIP              %s",user->misc & WIP?"Yes":"No");
		sprintf(opt[i++],"Pause            %s",user->misc & UPAUSE?"Yes":"No");
		sprintf(opt[i++],"Hot Keys         %s",user->misc & COLDKEYS?"No":"Yes");
		sprintf(opt[i++],"Spinning Cursor  %s",user->misc & SPIN?"Yes":"No");
		sprintf(str,"%u",user->rows);
		sprintf(opt[i++],"Number of Rows   %s",user->rows?str:"Auto");
		opt[i][0]=0;
		switch(uifc.list(WIN_MID|WIN_ACT|WIN_SAV,0,0,0,&j,0,"Terminal Settings",opt)) {
			case -1:
				return(0);
				break;
			case 0:
				/* Auto-detect */
				user->misc ^= AUTOTERM;
				putuserrec(cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 1:
				/* EX-ASCII */
				user->misc ^= NO_EXASCII;
				putuserrec(cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 2:
				/* ANSI */
				user->misc ^= ANSI;
				putuserrec(cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 3:
				/* Colour */
				user->misc ^= COLOR;
				putuserrec(cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 4:
				/* RIP */
				user->misc ^= RIP;
				putuserrec(cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 5:
				/* WIP */
				user->misc ^= WIP;
				putuserrec(cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 6:
				/* Pause */
				user->misc ^= UPAUSE;
				putuserrec(cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 7:
				/* Hot Keys */
				user->misc ^= COLDKEYS;
				putuserrec(cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 8:
				/* Spinning Cursor */
				user->misc ^= SPIN;
				putuserrec(cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 9:
				/* Rows */
				sprintf(str,"%u",user->rows);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Rows",str,2,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					user->rows=strtoul(str,NULL,10);
					putuserrec(cfg,user->number,U_ROWS,2,ultoa(user->rows,str,10));
				}
				break;
		}
	}
	return(0);
}

/* Edit Logon settings
 *       Ask for New Message Scan
 *       Ask for Your Message Scan
 *       Remember Current Sub-board
 *       Quiet Mode (Q exempt)
 *       Auto-Login via IP (V exempt)
 */
int edit_logon(scfg_t *cfg, user_t *user)
{
	int 	i,j;
	char 	**opt;
	char	str[256];

	if((opt=(char **)alloca(sizeof(char *)*(5+1)))==NULL)
		allocfail(sizeof(char *)*(5+1));
	for(i=0;i<(5+1);i++)
		if((opt[i]=(char *)alloca(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	j=0;
	while(1) {
		i=0;
		GETUSERDAT(cfg,user);
		sprintf(opt[i++],"Ask for New Message Scan     %s",user->misc & ASK_NSCAN?"Yes":"No");
		sprintf(opt[i++],"Ask for Your Message Scan    %s",user->misc & ASK_SSCAN?"Yes":"No");
		sprintf(opt[i++],"Remember Current Sub         %s",user->misc & CURSUB?"Yes":"No");
		sprintf(opt[i++],"Quiet Mode  (Q exempt)       %s",user->misc & QUIET?"Yes":"No");
		sprintf(opt[i++],"Auto-Login via IP (V exempt) %s",user->misc & AUTOLOGON?"Yes":"No");
		opt[i][0]=0;
		switch(uifc.list(WIN_MID|WIN_ACT|WIN_SAV,0,0,0,&j,0,"Logon Settings",opt)) {
			case -1:
				return(0);
				break;
			case 0:
				/* Ask New MSG Scan */
				user->misc ^= ASK_NSCAN;
				putuserrec(cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 1:
				/* Ask YOUR MSG scan */
				user->misc ^= ASK_SSCAN;
				putuserrec(cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 2:
				/* Remember Curr Sub */
				user->misc ^= CURSUB;
				putuserrec(cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 3:
				/* Quiet Mode */
				user->misc ^= QUIET;
				putuserrec(cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 4:
				/* Auto-Login by IP */
				user->misc ^= AUTOLOGON;
				putuserrec(cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
		}
	}
	return(0);
}

/* Edit Chat Settings
 *       Multinode Chat Echo
 *       Multinode Chat Actions
 *       Available for Internode chat
 *       Multinode Activity Alerts
 *       Split-screen Private Chat
 */
int edit_chat(scfg_t *cfg, user_t *user)
{
	int 	i,j;
	char 	**opt;
	char	str[256];

	if((opt=(char **)alloca(sizeof(char *)*(5+1)))==NULL)
		allocfail(sizeof(char *)*(5+1));
	for(i=0;i<(5+1);i++)
		if((opt[i]=(char *)alloca(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	j=0;
	while(1) {
		i=0;
		GETUSERDAT(cfg,user);
		sprintf(opt[i++],"Chat Echo                  %s",user->chat & CHAT_ECHO?"Yes":"No");
		sprintf(opt[i++],"Chat Actions               %s",user->chat & CHAT_ACTION?"Yes":"No");
		sprintf(opt[i++],"Available for Chat         %s",user->chat & CHAT_NOPAGE?"No":"Yes");
		sprintf(opt[i++],"Activity Alerts            %s",user->chat & CHAT_NOACT?"No":"Yes");
		sprintf(opt[i++],"Split-Screen Private Chat  %s",user->chat & CHAT_SPLITP?"Yes":"No");
		opt[i][0]=0;
		switch(uifc.list(WIN_MID|WIN_ACT|WIN_SAV,0,0,0,&j,0,"Chat Settings",opt)) {
			case -1:
				return(0);
				break;
			case 0:
				/* Chat Echo */
				user->chat ^= CHAT_ECHO;
				putuserrec(cfg,user->number,U_CHAT,8,ultoa(user->chat,str,16));
				break;
			case 1:
				/* Chat Actions */
				user->chat ^= CHAT_ACTION;
				putuserrec(cfg,user->number,U_CHAT,8,ultoa(user->chat,str,16));
				break;
			case 2:
				/* Availabe for Chat */
				user->chat ^= CHAT_NOPAGE;
				putuserrec(cfg,user->number,U_CHAT,8,ultoa(user->chat,str,16));
				break;
			case 3:
				/* Activity Alerts */
				user->chat ^= CHAT_NOACT;
				putuserrec(cfg,user->number,U_CHAT,8,ultoa(user->chat,str,16));
				break;
			case 4:
				/* Split-Screen Priv Chat */
				user->chat ^= CHAT_SPLITP;
				putuserrec(cfg,user->number,U_CHAT,8,ultoa(user->chat,str,16));
				break;
		}
	}
	return(0);
}

/* Pick Command Shell */

int edit_shell(scfg_t *cfg, user_t *user)
{
	int 	i,j;
	char 	**opt;

	if((opt=(char **)alloca(sizeof(char *)*(cfg->total_shells+1)))==NULL)
		allocfail(sizeof(char *)*(cfg->total_shells+1));

	for(i=0;i<cfg->total_shells;i++) {
		opt[i]=cfg->shell[i]->name;
	}
	opt[i]="";
	j=user->shell;
	switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&j,0,"Command Shell",opt)) {
		case -1:
			break;
		default:
			if(user->shell != j) {
				user->shell=j;
				putuserrec(cfg,user->number,U_SHELL,8,cfg->shell[j]->code);
			}
			break;
	}

	return(0);
}

/* Edit Command Shell
 *       Name
 *       Expert Mode
 */
int edit_cmd(scfg_t *cfg, user_t *user)
{
	int 	i,j;
	char 	**opt;
	char	str[256];

	if((opt=(char **)alloca(sizeof(char *)*(2+1)))==NULL)
		allocfail(sizeof(char *)*(2+1));
	for(i=0;i<(2+1);i++)
		if((opt[i]=(char *)alloca(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	j=0;
	while(1) {
		i=0;
		GETUSERDAT(cfg,user);
		sprintf(opt[i++],"Command Shell  %s",cfg->shell[user->shell]->name);
		sprintf(opt[i++],"Expert Mode    %s",user->misc & EXPERT?"Yes":"No");
		opt[i][0]=0;
		switch(uifc.list(WIN_MID|WIN_ACT|WIN_SAV,0,0,0,&j,0,"Command Shell",opt)) {
			case -1:
				return(0);
				break;
			case 0:
				/* Command Shell */
				edit_shell(cfg,user);
				break;
			case 1:
				/* Expert Mode */
				user->misc ^= EXPERT;
				putuserrec(cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
		}
	}

	return(0);
}

/* Pick External Editor */
int edit_xedit(scfg_t *cfg, user_t *user)
{
	int 	i,j;
	char 	**opt;

	if((opt=(char **)alloca(sizeof(char *)*(cfg->total_xedits+1)))==NULL)
		allocfail(sizeof(char *)*(cfg->total_xedits+1));

	GETUSERDAT(cfg,user);
	opt[0]="None";
	for(i=1;i<=cfg->total_xedits;i++) {
		opt[i]=cfg->xedit[i-1]->name;
	}
	opt[i]="";
	j=user->xedit;
	switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&j,0,"External Editor",opt)) {
		case -1:
			break;
		default:
			if(user->xedit != j) {
				user->xedit=j;
				if(j > 0)
				    putuserrec(cfg,user->number,U_XEDIT,8,cfg->xedit[j-1]->code);
				else
				    putuserrec(cfg,user->number,U_XEDIT,8,nulstr);
			}
			break;
	}
	return(0);
}

/* Edit Message Options
 *       Forward Email to NetMail
 *       Clear Screen Between Messages
 *       External Editor
 */
int edit_msgopts(scfg_t *cfg, user_t *user)
{
	int 	i,j;
	char 	**opt;
	char	str[256];

	if((opt=(char **)alloca(sizeof(char *)*(3+1)))==NULL)
		allocfail(sizeof(char *)*(3+1));
	for(i=0;i<(3+1);i++)
		if((opt[i]=(char *)alloca(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	j=0;
	while(1) {
		GETUSERDAT(cfg,user);
		i=0;
		sprintf(opt[i++],"Forward Email to NetMail       %s",user->misc & NETMAIL?"Yes":"No");
		sprintf(opt[i++],"Clear Screen Between Messages  %s",user->misc & CLRSCRN?"Yes":"No");
		sprintf(opt[i++],"External Editor                %s",user->xedit?cfg->xedit[user->xedit-1]->name:"None");
		opt[i][0]=0;
		switch(uifc.list(WIN_MID|WIN_ACT|WIN_SAV,0,0,0,&j,0,"Message Options",opt)) {
			case -1:
				return(0);
				break;
			case 0:
				/* FWD Email */
				user->misc ^= NETMAIL;
				putuserrec(cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 1:
				/* Clear Between MSGS */
				user->misc ^=CLRSCRN;
				putuserrec(cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 2:
				/* External Editor */
				edit_xedit(cfg,user);
				break;
		}
	}
	return(0);
}

/* Pick Tmp/QWK File Type */
int edit_tmpqwktype(scfg_t *cfg, user_t *user)
{
	int 	i;
	int		j=0;
	char 	**opt;

	if((opt=(char **)alloca(sizeof(char *)*(cfg->total_fcomps+1)))==NULL)
		allocfail(sizeof(char *)*(cfg->total_fcomps+1));

	GETUSERDAT(cfg,user);
	for(i=0;i<cfg->total_fcomps;i++) {
		opt[i]=cfg->fcomp[i]->ext;
		if(!strcmp(cfg->fcomp[i]->ext,user->tmpext))
			j=i;
	}
	opt[i]="";
	switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&j,0,"Temp/QWK File Type",opt)) {
		case -1:
			break;
		default:
			if(strcmp(cfg->fcomp[j]->ext,user->tmpext)) {
				strcpy(user->tmpext,cfg->fcomp[j]->ext);
				putuserrec(cfg,user->number,U_TMPEXT,3,user->tmpext);
			}
			break;
	}
	return(0);
}

/* Edit QWK Packet Options
 *       Include New Files List
 *       Include Unread Email
 *       Delete Email After Download
 *       Include Messages From Self
 *       Expand CTRL-A Codes to ANSI
 *       Strip CTRL-A Codes
 *       Include File Attachments
 *       Include Index Files
 *       Include Time Zone (@TZ)
 *       Include Seen-Bys (@VIA)
 *       Extraneous Control Files
 */
int edit_qwk(scfg_t *cfg, user_t *user)
{
	int 	i,j;
	char 	**opt;
	char	str[256];

	if((opt=(char **)alloca(sizeof(char *)*(15+1)))==NULL)
		allocfail(sizeof(char *)*(15+1));
	for(i=0;i<(15+1);i++)
		if((opt[i]=(char *)alloca(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	j=0;
	while(1) {
		i=0;
		GETUSERDAT(cfg,user);
		sprintf(opt[i++],"Include New Files List        %s",user->qwk & QWK_FILES?"Yes":"No");
		sprintf(opt[i++],"Include Unread Email          %s",user->qwk & QWK_EMAIL?"Yes":"No");
		sprintf(opt[i++],"Include ALL Email             %s",user->qwk & QWK_ALLMAIL?"Yes":"No");
		sprintf(opt[i++],"Delete Email After Download   %s",user->qwk & QWK_DELMAIL?"Yes":"No");
		sprintf(opt[i++],"Include Messages from Self    %s",user->qwk & QWK_BYSELF?"Yes":"No");
		sprintf(opt[i++],"Expand CTRL-A to ANSI         %s",user->qwk & QWK_EXPCTLA?"Yes":"No");
		sprintf(opt[i++],"Strip CTRL-A Codes            %s",user->qwk & QWK_RETCTLA?"No":"Yes");
		sprintf(opt[i++],"Include File Attachments      %s",user->qwk & QWK_ATTACH?"Yes":"No");
		sprintf(opt[i++],"Include Index Files           %s",user->qwk & QWK_NOINDEX?"No":"Yes");
		sprintf(opt[i++],"Include Time Zone (@TZ)       %s",user->qwk & QWK_TZ?"Yes":"No");
		sprintf(opt[i++],"Include Seen-Bys (@VIA)       %s",user->qwk & QWK_VIA?"Yes":"No");
		sprintf(opt[i++],"Extraneous Control Files      %s",user->qwk & QWK_NOCTRL?"No":"Yes");
		sprintf(opt[i++],"Extended (QWKE) Format        %s",user->qwk & QWK_EXT?"Yes":"No");
		sprintf(opt[i++],"Include Message IDs (@MSGID)  %s",user->qwk & QWK_MSGID?"Yes":"No");
		sprintf(opt[i++],"Temp/QWK File Type            %s",user->tmpext);
		opt[i][0]=0;
		switch(uifc.list(WIN_MID|WIN_ACT|WIN_SAV,0,0,0,&j,0,"Command Shell",opt)) {
			case -1:
				return(0);
				break;
			case 0:
				/* New Files List */
				user->qwk ^= QWK_FILES;
				putuserrec(cfg,user->number,U_QWK,8,ultoa(user->qwk,str,16));
				break;
			case 1:
				/* Unread Email */
				user->qwk ^= QWK_EMAIL;
				putuserrec(cfg,user->number,U_QWK,8,ultoa(user->qwk,str,16));
				break;
			case 2:
				/* ALL Email */
				user->qwk ^= QWK_ALLMAIL;
				putuserrec(cfg,user->number,U_QWK,8,ultoa(user->qwk,str,16));
				break;
			case 3:
				/* Del Email after Download */
				user->qwk ^= QWK_DELMAIL;
				putuserrec(cfg,user->number,U_QWK,8,ultoa(user->qwk,str,16));
				break;
			case 4:
				/* Include From Self */
				user->qwk ^= QWK_BYSELF;
				putuserrec(cfg,user->number,U_QWK,8,ultoa(user->qwk,str,16));
				break;
			case 5:
				/* Expand CTRL-A */
				user->qwk ^= QWK_EXPCTLA;
				putuserrec(cfg,user->number,U_QWK,8,ultoa(user->qwk,str,16));
				break;
			case 6:
				/* Strip CTRL-A */
				user->qwk ^= QWK_RETCTLA;
				putuserrec(cfg,user->number,U_QWK,8,ultoa(user->qwk,str,16));
				break;
			case 7:
				/* Include Attach */
				user->qwk ^= QWK_ATTACH;
				putuserrec(cfg,user->number,U_QWK,8,ultoa(user->qwk,str,16));
				break;
			case 8:
				/* Include Indexes */
				user->qwk ^= QWK_NOINDEX;
				putuserrec(cfg,user->number,U_QWK,8,ultoa(user->qwk,str,16));
				break;
			case 9:
				/* Include TZ */
				user->qwk ^= QWK_TZ;
				putuserrec(cfg,user->number,U_QWK,8,ultoa(user->qwk,str,16));
				break;
			case 10:
				/* Include VIA */
				user->qwk ^= QWK_VIA;
				putuserrec(cfg,user->number,U_QWK,8,ultoa(user->qwk,str,16));
				break;
			case 11:
				/* Extra CTRL Files */
				user->qwk ^= QWK_NOCTRL;
				putuserrec(cfg,user->number,U_QWK,8,ultoa(user->qwk,str,16));
				break;
			case 12:
				/* Extended QWKE */
				user->qwk ^= QWK_EXT;
				putuserrec(cfg,user->number,U_QWK,8,ultoa(user->qwk,str,16));
				break;
			case 13:
				/* Include MSGID */
				user->qwk ^= QWK_MSGID;
				putuserrec(cfg,user->number,U_QWK,8,ultoa(user->qwk,str,16));
				break;
			case 14:
				/* Temp/QWK Type */
				edit_tmpqwktype(cfg,user);
				break;
		}
	}
	return(0);
}

/* Pick Protocol */

int edit_proto(scfg_t *cfg, user_t *user)
{
	int 	i;
	int		j=0;
	char 	**opt;

	if((opt=(char **)alloca(sizeof(char *)*(cfg->total_prots+1)))==NULL)
		allocfail(sizeof(char *)*(cfg->total_prots+1));

	GETUSERDAT(cfg,user);
	opt[0]="None";
	for(i=1;i<=cfg->total_prots;i++) {
		opt[i]=cfg->prot[i-1]->name;
		if(cfg->prot[i-1]->mnemonic == user->prot)
			j=i;
	}
	opt[i]="";
	switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&j,0,"Default Protcol",opt)) {
		case -1:
			break;
		case 0:
			if(user->prot != ' ')
				putuserrec(cfg,user->number,U_PROT,1," ");
			user->prot=' ';
			break;
		default:
			if(user->prot != cfg->prot[j-1]->mnemonic) {
				user->prot=cfg->prot[j-1]->mnemonic;
				putuserrec(cfg,user->number,U_PROT,1,(char*)&user->prot);
			}
			break;
	}
	return(0);
}

/* Edit File Options
 *       Auto-New Scan
 *       Extended Descriptions
 *       Batch Flagging
 *       Auto-Hangup After Transfer
 *       Default download Protocol
 *       Temp/QWK File Type
 */
int edit_fileopts(scfg_t *cfg, user_t *user)
{
	int 	i,j;
	int		k;
	char 	**opt;
	char 	str[256];

	if((opt=(char **)alloca(sizeof(char *)*(6+1)))==NULL)
		allocfail(sizeof(char *)*(6+1));
	for(i=0;i<(6+1);i++)
		if((opt[i]=(char *)alloca(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	j=0;
	while(1) {
		GETUSERDAT(cfg,user);
		i=0;
		sprintf(opt[i++],"Auto-New Scan              %s",user->misc & ANFSCAN?"Yes":"No");
		sprintf(opt[i++],"Extended Descriptions      %s",user->misc & EXTDESC?"Yes":"No");
		sprintf(opt[i++],"Batch Flagging             %s",user->misc & BATCHFLAG?"Yes":"No");
		sprintf(opt[i++],"Auto Transfer Hangup       %s",user->misc & AUTOHANG?"Yes":"No");
		strcpy(str,"None");
		for(k=0;k<cfg->total_prots;k++)
			if(cfg->prot[k]->mnemonic==user->prot)
				strcpy(str,cfg->prot[k]->name);
		sprintf(opt[i++],"Default Download Protocol  %s",str);
		sprintf(opt[i++],"Temp/QWK File Type         %s",user->tmpext);
		opt[i][0]=0;
		switch(uifc.list(WIN_MID|WIN_ACT|WIN_SAV,0,0,0,&j,0,"File Options",opt)) {
			case -1:
				return(0);
				break;
			case 0:
				/* Auto-New Scan */
				user->misc ^= ANFSCAN;
				putuserrec(cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 1:
				/* Extended Descs */
				user->misc ^= EXTDESC;
				putuserrec(cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 2:
				/* Batch Flagging */
				user->misc ^= BATCHFLAG;
				putuserrec(cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 3:
				/* Auto-Hangup */
				user->misc ^= AUTOHANG;
				putuserrec(cfg,user->number,U_MISC,8,ultoa(user->misc,str,16));
				break;
			case 4:
				/* Default Download Protocol */
				edit_proto(cfg,user);
				break;
			case 5:
				/* Temp/QWK File Type */
				edit_tmpqwktype(cfg,user);
				break;
		}
	}
	return(0);
}

/* Edit "Extended Comment" */

int edit_comment(scfg_t *cfg, user_t *user)
{
	char str[1024];
	char editor[1024];

	sprintf(str,"%s %suser/%04u.msg",geteditor(editor),cfg->data_dir,user->number);
	do_cmd(str);
	return(0);
}

/* MSG and File settings
 *      - Message Options
 *      - QWK Message Packet
 *      - File Options
 */
int edit_msgfile(scfg_t *cfg, user_t *user)
{
	char *opt[4]={
		 "Message Options"
		,"QWK Message Packet"
		,"File Options"
		,""};
	int i=0;
	while(1) {
		switch(uifc.list(WIN_BOT|WIN_RHT|WIN_ACT,0,0,0,&i,0,"Settings",opt)) {
			case -1:
				return(0);
				break;
			case 0:
				/* Message Options */
				edit_msgopts(cfg,user);
				break;
			case 1:
				/* QWK Message Packet */
				edit_qwk(cfg,user);
				break;
			case 2:
				/* File Options */
				edit_fileopts(cfg,user);
				break;
		}
	}
	return(0);
}

/* Settings
 *     - Terminal Settings
 *     - Logon Toggles
 *     - Chat Toggles
 *     - Command Shell
 */
int edit_settings(scfg_t *cfg, user_t *user)
{
	char *opt[5]={
		 "Terminal Settings"
		,"Logon Toggles"
		,"Chat Toggles"
		,"Command Shell"
		,""};
	int i=0;

	while(1) {
		switch(uifc.list(WIN_BOT|WIN_RHT|WIN_ACT,0,0,0,&i,0,"Settings",opt)) {
			case -1:
				return(0);
				break;
			case 0:
				/* Terminal Settings */
				edit_terminal(cfg,user);
				break;
			case 1:
				/* Logon Toggles */
				edit_logon(cfg,user);
				break;
			case 2:
				/* Chat Toggles */
				edit_chat(cfg,user);
				break;
			case 3:
				/* Command Shell */
				edit_cmd(cfg,user);
				break;
		}
	}
	return(0);
}

/* Statistics
 *     First On
 *     Last On
 *     Total Logons
 *     Todays Logons
 *     Total Posts
 *     Todays Posts
 *     Total Uploads
 *     Todays Uploads
 *     Total Time On
 *     Todays Time On
 *     Last Call Time On
 *     Extra
 *     Total Downloads
 *     Bytes
 *     Leech
 *     Total Email
 *     Todays Email
 *     Email to Sysop
 */
int edit_stats(scfg_t *cfg, user_t *user)
{
	int 	i,j;
	char 	**opt;
	char	str[256];
	time_t	temptime,temptime2;

	if((opt=(char **)alloca(sizeof(char *)*(20+1)))==NULL)
		allocfail(sizeof(char *)*(20+1));
	for(i=0;i<(20+1);i++)
		if((opt[i]=(char *)alloca(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	j=0;
	while(1) {
		GETUSERDAT(cfg,user);
		i=0;
		sprintf(opt[i++],"First On           %s",user->firston?timestr(cfg, user->firston, str):"Never");
		sprintf(opt[i++],"Last On            %s",user->laston?timestr(cfg, user->laston, str):"Never");
		sprintf(opt[i++],"Logon Time         %s",user->laston?timestr(cfg, user->logontime, str):"Not On");
		sprintf(opt[i++],"Total Logons       %hu",user->logons);
		sprintf(opt[i++],"Todays Logons      %hu",user->ltoday);
		sprintf(opt[i++],"Total Posts        %hu",user->posts);
		sprintf(opt[i++],"Todays Posts       %hu",user->ptoday);
		sprintf(opt[i++],"Total Email        %hu",user->emails);
		sprintf(opt[i++],"Todays Email       %hu",user->etoday);
		sprintf(opt[i++],"Email To Sysop     %hu",user->fbacks);
		sprintf(opt[i++],"Total Time On      %hu",user->timeon);
		sprintf(opt[i++],"Time On Today      %hu",user->ttoday);
		sprintf(opt[i++],"Time On Last Call  %hu",user->tlast);
		sprintf(opt[i++],"Extra Time Today   %hu",user->textra);
		sprintf(opt[i++],"Total Downloads    %hu",user->dls);
		sprintf(opt[i++],"Downloaded Bytes   %lu",user->dlb);
		sprintf(opt[i++],"Total Uploads      %hu",user->uls);
		sprintf(opt[i++],"Uploaded Bytes     %lu",user->ulb);
		sprintf(opt[i++],"Leech Downloads    %u",user->leech);
		sprintf(opt[i++],"Password Modified  %s",user->pwmod?timestr(cfg, user->pwmod, str):"Never");
		opt[i][0]=0;
		switch(uifc.list(WIN_MID|WIN_ACT,0,0,0,&j,0,"Statistics",opt)) {
			case -1:
				return(0);
				break;
			case 0:
				/* First On */
				GETUSERDAT(cfg,user);
				temptime=user->firston;
				unixtodstr(cfg,temptime,str);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"First On Date",str,8,K_EDIT);
				user->firston=dstrtounix(cfg, str);
				temptime2=temptime-user->firston;
				sectostr(temptime2,str);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"First On Time",str,8,K_EDIT);
				temptime2=strtosec(str);
				if(temptime2!=-1)
					user->firston += temptime2;
				if(temptime!=user->firston)
					putuserrec(cfg,user->number,U_FIRSTON,8,ultoa(user->firston,str,16));
				break;
			case 1:
				/* Last On */
				GETUSERDAT(cfg,user);
				temptime=user->laston;
				unixtodstr(cfg,temptime,str);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Last On Date",str,8,K_EDIT);
				user->laston=dstrtounix(cfg, str);
				temptime2=temptime-user->laston;
				sectostr(temptime2,str);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Last On Time",str,8,K_EDIT);
				temptime2=strtosec(str);
				if(temptime2!=-1)
					user->laston += temptime2;
				if(temptime!=user->laston)
					putuserrec(cfg,user->number,U_LASTON,8,ultoa(user->laston,str,16));
				break;
			case 2:
				/* Logon Time */
				GETUSERDAT(cfg,user);
				temptime=user->logontime;
				unixtodstr(cfg,temptime,str);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Logon Date",str,8,K_EDIT);
				user->logontime=dstrtounix(cfg, str);
				temptime2=temptime-user->logontime;
				sectostr(temptime2,str);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Logon Time",str,8,K_EDIT);
				temptime2=strtosec(str);
				if(temptime2!=-1)
					user->logontime += temptime2;
				if(temptime!=user->logontime)
					putuserrec(cfg,user->number,U_LOGONTIME,8,ultoa(user->logontime,str,16));
				break;
			case 3:
				/* Total Logons */
				GETUSERDAT(cfg,user);
				sprintf(str,"%hu",user->logons);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Total Logons",str,5,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					user->logons=strtoul(str,NULL,10);
					putuserrec(cfg,user->number,U_LOGONS,5,ultoa(user->logons,str,10));
				}
				break;
			case 4:
				/* Todays Logons */
				GETUSERDAT(cfg,user);
				sprintf(str,"%hu",user->ltoday);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Todays Logons",str,5,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					user->ltoday=strtoul(str,NULL,10);
					putuserrec(cfg,user->number,U_LTODAY,5,ultoa(user->ltoday,str,10));
				}
				break;
			case 5:
				/* Total Posts */
				GETUSERDAT(cfg,user);
				sprintf(str,"%hu",user->posts);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Total Posts",str,5,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					user->posts=strtoul(str,NULL,10);
					putuserrec(cfg,user->number,U_POSTS,5,ultoa(user->posts,str,10));
				}
				break;
			case 6:
				/* Todays Posts */
				GETUSERDAT(cfg,user);
				sprintf(str,"%hu",user->ptoday);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Todays Posts",str,5,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					user->ptoday=strtoul(str,NULL,10);
					putuserrec(cfg,user->number,U_PTODAY,5,ultoa(user->ptoday,str,10));
				}
				break;
			case 7:
				/* Total Emails */
				GETUSERDAT(cfg,user);
				sprintf(str,"%hu",user->emails);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Total Emails",str,5,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					user->emails=strtoul(str,NULL,10);
					putuserrec(cfg,user->number,U_EMAILS,5,ultoa(user->emails,str,10));
				}
				break;
			case 8:
				/* Todays Emails */
				GETUSERDAT(cfg,user);
				sprintf(str,"%hu",user->etoday);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Todays Emails",str,5,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					user->etoday=strtoul(str,NULL,10);
					putuserrec(cfg,user->number,U_ETODAY,5,ultoa(user->etoday,str,10));
				}
				break;
			case 9:
				/* Emails to Sysop */
				GETUSERDAT(cfg,user);
				sprintf(str,"%hu",user->fbacks);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Emails to Sysop",str,5,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					user->fbacks=strtoul(str,NULL,10);
					putuserrec(cfg,user->number,U_FBACKS,5,ultoa(user->fbacks,str,10));
				}
				break;
			case 10:
				/* Total Time On */
				GETUSERDAT(cfg,user);
				sprintf(str,"%hu",user->timeon);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Total Time On",str,5,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					user->timeon=strtoul(str,NULL,10);
					putuserrec(cfg,user->number,U_TIMEON,5,ultoa(user->timeon,str,10));
				}
				break;
			case 11:
				/* Time On Today */
				GETUSERDAT(cfg,user);
				sprintf(str,"%hu",user->ttoday);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Time On Today",str,5,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					user->ttoday=strtoul(str,NULL,10);
					putuserrec(cfg,user->number,U_TTODAY,5,ultoa(user->ttoday,str,10));
				}
				break;
			case 12:
				/* Time On Last Call */
				GETUSERDAT(cfg,user);
				sprintf(str,"%hu",user->tlast);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Time On Last Call",str,5,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					user->tlast=strtoul(str,NULL,10);
					putuserrec(cfg,user->number,U_TLAST,5,ultoa(user->tlast,str,10));
				}
				break;
			case 13:
				/* Extra Time Today */
				GETUSERDAT(cfg,user);
				sprintf(str,"%hu",user->textra);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Extra Time Today",str,5,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					user->textra=strtoul(str,NULL,10);
					putuserrec(cfg,user->number,U_TEXTRA,5,ultoa(user->textra,str,10));
				}
				break;
			case 14:
				/* Total Downloads */
				GETUSERDAT(cfg,user);
				sprintf(str,"%hu",user->dls);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Total Downloads",str,5,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					user->dls=strtoul(str,NULL,10);
					putuserrec(cfg,user->number,U_DLS,5,ultoa(user->dls,str,10));
				}
				break;
			case 15:
				/* Downloaded Bytes */
				GETUSERDAT(cfg,user);
				sprintf(str,"%lu",user->dlb);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Downloaded Bytes",str,10,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					user->dlb=strtoul(str,NULL,10);
					putuserrec(cfg,user->number,U_DLB,10,ultoa(user->dlb,str,10));
				}
				break;
			case 16:
				/* Total Uploads */
				GETUSERDAT(cfg,user);
				sprintf(str,"%hu",user->uls);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Total Uploads",str,5,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					user->uls=strtoul(str,NULL,10);
					putuserrec(cfg,user->number,U_ULS,5,ultoa(user->uls,str,10));
				}
				break;
			case 17:
				/* Uploaded Bytes */
				GETUSERDAT(cfg,user);
				sprintf(str,"%lu",user->ulb);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Uploaded Bytes",str,10,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					user->ulb=strtoul(str,NULL,10);
					putuserrec(cfg,user->number,U_ULB,10,ultoa(user->ulb,str,10));
				}
				break;
			case 18:
				/* Leech Counter */
				GETUSERDAT(cfg,user);
				sprintf(str,"%u",user->leech);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Leech Counter",str,3,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					user->leech=strtoul(str,NULL,10);
					putuserrec(cfg,user->number,U_LEECH,2,ultoa(user->leech,str,16));
				}
				break;
			case 19:
				/* Password Last Modified */
				GETUSERDAT(cfg,user);
				temptime=user->pwmod;
				unixtodstr(cfg,temptime,str);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Password Modified Date",str,8,K_EDIT);
				user->firston=dstrtounix(cfg, str);
				temptime2=temptime-user->pwmod;
				sectostr(temptime2,str);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Password Modified Time",str,8,K_EDIT);
				temptime2=strtosec(str);
				if(temptime2!=-1)
					user->pwmod += temptime2;
				if(temptime!=user->pwmod)
					putuserrec(cfg,user->number,U_PWMOD,8,ultoa(user->pwmod,str,16));
				break;
		}
	}
	return(0);
}

/* Security settings
 *     Level
 *     Expiration
 *     Flag Set 1
 *     Flag Set 2
 *     Flag Set 3
 *     Flag Set 4
 *     Exemptions
 *     Restrictions
 *     Credits
 *     Free Credits
 *     Minutes
 */
int edit_security(scfg_t *cfg, user_t *user)
{
	int 	i,j;
	char 	**opt;
	char	str[256];

	if((opt=(char **)alloca(sizeof(char *)*(11+1)))==NULL)
		allocfail(sizeof(char *)*(11+1));
	for(i=0;i<(11+1);i++)
		if((opt[i]=(char *)alloca(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	j=0;
	GETUSERDAT(cfg,user);
	while(1) {
		i=0;
		sprintf(opt[i++],"Level         %d",user->level);
		sprintf(opt[i++],"Expiration    %s",user->expire?unixtodstr(cfg, user->expire, str):"Never");
		sprintf(opt[i++],"Flag Set 1    %s",ltoaf(user->flags1,str));
		sprintf(opt[i++],"Flag Set 2    %s",ltoaf(user->flags2,str));
		sprintf(opt[i++],"Flag Set 3    %s",ltoaf(user->flags3,str));
		sprintf(opt[i++],"Flag Set 4    %s",ltoaf(user->flags4,str));
		sprintf(opt[i++],"Exemptions    %s",ltoaf(user->exempt,str));
		sprintf(opt[i++],"Restrictions  %s",ltoaf(user->rest,str));
		sprintf(opt[i++],"Credits       %lu",user->cdt);
		sprintf(opt[i++],"Free Credits  %lu",user->freecdt);
		sprintf(opt[i++],"Minutes       %lu",user->min);
		opt[i][0]=0;
		switch(uifc.list(WIN_MID|WIN_ACT,0,0,0,&j,0,"Security Settings",opt)) {
			case -1:
				return(0);
				break;
			case 0:
				/* Level */
				GETUSERDAT(cfg,user);
				sprintf(str,"%d",user->level);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Level",str,2,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					user->level=atoi(str);
					putuserrec(cfg,user->number,U_LEVEL,2,str);
				}
				break;
			case 1:
				/* Expiration */
				GETUSERDAT(cfg,user);
				unixtodstr(cfg,user->expire,str);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Expiration",str,8,K_EDIT);
				if(uifc.changes && dstrtounix(cfg, str)!=user->expire) {
					user->expire=dstrtounix(cfg, str);
					putuserrec(cfg,user->number,U_EXPIRE,8,ultoa(user->expire,str,16));
				}
				break;
			case 2:
				/* Flag Set 1 */
				GETUSERDAT(cfg,user);
				ltoaf(user->flags1,str);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Flag Set 1",str,26,K_EDIT|K_UPPER|K_ALPHA);
				if(uifc.changes) {
					user->flags1=aftol(str);
					putuserrec(cfg,user->number,U_FLAGS1,8,ultoa(user->flags1,str,16));
				}
				break;
			case 3:
				/* Flag Set 2 */
				GETUSERDAT(cfg,user);
				ltoaf(user->flags2,str);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Flag Set 2",str,26,K_EDIT|K_UPPER|K_ALPHA);
				if(uifc.changes) {
					user->flags2=aftol(str);
					putuserrec(cfg,user->number,U_FLAGS2,8,ultoa(user->flags2,str,16));
				}
				break;
			case 4:
				/* Flag Set 3 */
				GETUSERDAT(cfg,user);
				ltoaf(user->flags3,str);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Flag Set 3",str,26,K_EDIT|K_UPPER|K_ALPHA);
				if(uifc.changes) {
					user->flags3=aftol(str);
					putuserrec(cfg,user->number,U_FLAGS3,8,ultoa(user->flags3,str,16));
				}
				break;
			case 5:
				/* Flag Set 4 */
				GETUSERDAT(cfg,user);
				ltoaf(user->flags4,str);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Flag Set 4",str,26,K_EDIT|K_UPPER|K_ALPHA);
				if(uifc.changes) {
					user->flags4=aftol(str);
					putuserrec(cfg,user->number,U_FLAGS4,8,ultoa(user->flags4,str,16));
				}
				break;
			case 6:
				/* Exemptions */
				GETUSERDAT(cfg,user);
				ltoaf(user->exempt,str);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Exemptions",str,26,K_EDIT|K_UPPER|K_ALPHA);
				if(uifc.changes) {
					user->exempt=aftol(str);
					putuserrec(cfg,user->number,U_EXEMPT,8,ultoa(user->exempt,str,16));
				}
				break;
			case 7:
				/* Restrictions */
				GETUSERDAT(cfg,user);
				ltoaf(user->rest,str);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Restrictions",str,26,K_EDIT|K_UPPER|K_ALPHA);
				if(uifc.changes) {
					user->rest=aftol(str);
					putuserrec(cfg,user->number,U_REST,8,ultoa(user->rest,str,16));
				}
				break;
			case 8:
				/* Credits */
				GETUSERDAT(cfg,user);
				sprintf(str,"%lu",user->cdt);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Credits",str,10,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					user->cdt=strtoul(str,NULL,10);
					putuserrec(cfg,user->number,U_CDT,10,ultoa(user->cdt,str,10));
				}
				break;
			case 9:
				/* Free Credits */
				GETUSERDAT(cfg,user);
				sprintf(str,"%lu",user->freecdt);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Free Credits",str,10,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					user->freecdt=strtoul(str,NULL,10);
					putuserrec(cfg,user->number,U_FREECDT,10,ultoa(user->freecdt,str,10));
				}
				break;
			case 10:
				/* Minutes */
				GETUSERDAT(cfg,user);
				sprintf(str,"%lu",user->min);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Minutes",str,10,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					user->min=strtoul(str,NULL,10);
					putuserrec(cfg,user->number,U_MIN,10,ultoa(user->min,str,10));
				}
				break;
		}
	}

	return(0);
}

/*
 * Personal settings...
 *     Real Name
 *     Alias
 *     Chat Handle
 *     Computer
 *     NetMail
 *     Gender
 *     Birthdate
 *     Address 1
 *     Location
 *     Postal/ZIP
 *     Phone
 *     Computer
 *     Connection
 *     Password
 *     Note
 *     Comment
 */
int edit_personal(scfg_t *cfg, user_t *user)
{
	int 	i,j;
	char 	**opt;
	char	onech[2];
	char	str[256];

	if((opt=(char **)alloca(sizeof(char *)*(16+1)))==NULL)
		allocfail(sizeof(char *)*(16+1));
	for(i=0;i<(16+1);i++)
		if((opt[i]=(char *)alloca(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	j=0;
	while(1) {
		GETUSERDAT(cfg,user);
		i=0;
		sprintf(opt[i++],"Real Name   %s",user->name);
		sprintf(opt[i++],"Alias       %s",user->alias);
		sprintf(opt[i++],"Chat Handle %s",user->handle);
		sprintf(opt[i++],"NetMail     %s",user->netmail);
		sprintf(opt[i++],"Gender      %c",user->sex);
		sprintf(opt[i++],"D.O.B.      %s",user->birth);
		sprintf(opt[i++],"Address     %s",user->address);
		sprintf(opt[i++],"Location    %s",user->location);
		sprintf(opt[i++],"Postal/Zip  %s",user->zipcode);
		sprintf(opt[i++],"Phone       %s",user->phone);
		sprintf(opt[i++],"Computer    %s",user->comp);
		sprintf(opt[i++],"Connection  %s",user->modem);
		sprintf(opt[i++],"IP Address  %s",user->ipaddr);
		sprintf(opt[i++],"Password    %s",user->pass);
		sprintf(opt[i++],"Note        %s",user->note);
		sprintf(opt[i++],"Comment     %s",user->comment);
		opt[i][0]=0;
		uifc.changes=FALSE;
		switch(uifc.list(WIN_MID|WIN_ACT,0,0,0,&j,0,"Personal Settings",opt)) {
			case -1:
				return(0);
			case 0:
				/* Real Name */
				GETUSERDAT(cfg,user);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Real Name",user->name,LEN_NAME,K_EDIT);
				if(uifc.changes)
					putuserrec(cfg,user->number,U_NAME,LEN_NAME,user->name);
				break;
			case 1:
				/* Alias */
				GETUSERDAT(cfg,user);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Alias",user->alias,LEN_ALIAS,K_EDIT);
				if(uifc.changes) {
					putuserrec(cfg,user->number,U_ALIAS,LEN_ALIAS,user->alias);
					putusername(cfg,user->number,user->alias);
				}
				break;
			case 2:
				/* Handle */
				GETUSERDAT(cfg,user);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Chat Handle",user->handle,LEN_ALIAS,K_EDIT);
				if(uifc.changes)
					putuserrec(cfg,user->number,U_HANDLE,LEN_HANDLE,user->handle);
				break;
			case 3:
				/* NetMail */
				GETUSERDAT(cfg,user);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"NetMail Address",user->netmail,LEN_NETMAIL,K_EDIT);
				if(uifc.changes)
					putuserrec(cfg,user->number,U_NETMAIL,LEN_NETMAIL,user->netmail);
				break;
			case 4:
				/* Gender */
				GETUSERDAT(cfg,user);
				sprintf(onech,"%c",user->sex);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Gender",onech,1,K_UPPER|K_ALPHA|K_EDIT);
				if(uifc.changes) {
					user->sex=onech[0];
					putuserrec(cfg,user->number,U_SEX,1,onech);
				}
				break;
			case 5:
			    /* D.O.B */
				GETUSERDAT(cfg,user);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"D.O.B.",user->birth,LEN_BIRTH,K_EDIT);
				if(uifc.changes)
					putuserrec(cfg,user->number,U_BIRTH,LEN_BIRTH,user->birth);
				break;
			case 6:
				/* Address */
				GETUSERDAT(cfg,user);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Address",user->address,LEN_ADDRESS,K_EDIT);
				if(uifc.changes)
					putuserrec(cfg,user->number,U_ADDRESS,LEN_ADDRESS,user->address);
				break;
			case 7:
				/* Location */
				GETUSERDAT(cfg,user);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Location",user->location,LEN_LOCATION,K_EDIT);
				if(uifc.changes)
					putuserrec(cfg,user->number,U_LOCATION,LEN_LOCATION,user->location);
				break;
			case 8:
				/* Postal/Zip */
				GETUSERDAT(cfg,user);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Postal/Zip Code",user->zipcode,LEN_ZIPCODE,K_EDIT);
				if(uifc.changes)
					putuserrec(cfg,user->number,U_ZIPCODE,LEN_ZIPCODE,user->zipcode);
				break;
			case 9:
				/* Phone */
				GETUSERDAT(cfg,user);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Phone",user->phone,LEN_PHONE,K_EDIT);
				if(uifc.changes)
					putuserrec(cfg,user->number,U_PHONE,LEN_PHONE,user->phone);
				break;
			case 10:
				/* Computer */
				GETUSERDAT(cfg,user);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Computer",user->comp,LEN_COMP,K_EDIT);
				if(uifc.changes)
					putuserrec(cfg,user->number,U_COMP,LEN_COMP,user->comp);
				break;

            case 11:
				/* Connection */
				GETUSERDAT(cfg,user);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Connection",user->modem,LEN_MODEM,K_EDIT);
				if(uifc.changes)
					putuserrec(cfg,user->number,U_MODEM,LEN_MODEM,user->modem);
				break;
			case 12:
				/* IP Address */
				GETUSERDAT(cfg,user);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"IP Address",user->ipaddr,LEN_IPADDR,K_EDIT);
				if(uifc.changes)
					putuserrec(cfg,user->number,U_IPADDR,LEN_IPADDR,user->ipaddr);
				break;
			case 13:
				/* Password */
				GETUSERDAT(cfg,user);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Password",user->pass,LEN_PASS,K_EDIT);
				if(uifc.changes) {
					putuserrec(cfg,user->number,U_PASS,LEN_PASS,user->pass);
					user->pwmod=time(NULL);
					putuserrec(cfg,user->number,U_PWMOD,8,ultoa(user->pwmod,str,16));
				}
				break;
			case 14:
				/* Note */
				GETUSERDAT(cfg,user);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Note",user->note,LEN_NOTE,K_EDIT);
				if(uifc.changes)
					putuserrec(cfg,user->number,U_NOTE,LEN_NOTE,user->note);
				break;
			case 15:
			    /* Comment */
				GETUSERDAT(cfg,user);
				uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Comment",user->comment,LEN_COMMENT,K_EDIT);
				if(uifc.changes)
					putuserrec(cfg,user->number,U_COMMENT,60,user->comment);
				break;
		}
	}

	return(0);
}

/* This is where the good stuff happens */

int edit_user(scfg_t *cfg, int usernum)
{
	char**	opt;
	int 	i,j;
	user_t	user;
	char	str[256];

	if((opt=(char **)alloca(sizeof(char *)*(8+1)))==NULL)
		allocfail(sizeof(char *)*(8+1));
	for(i=0;i<(8+1);i++)
		if((opt[i]=(char *)alloca(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	user.number=usernum;

	j=0;
	while(1) {
		GETUSERDAT(cfg,&user);
		i=0;
		if (user.misc & DELETED)
			strcpy(opt[i++],"Undelete");
		else
			strcpy(opt[i++],"Delete");
		if (user.misc & INACTIVE)
			strcpy(opt[i++],"Activate");
		else
		strcpy(opt[i++],"Deactivate");
		strcpy(opt[i++],"Personal");
		strcpy(opt[i++],"Security");
		strcpy(opt[i++],"Statistics");
		strcpy(opt[i++],"Settings");
		strcpy(opt[i++],"MSG/File Settings");
		strcpy(opt[i++],"Extended Comment");
		opt[i][0]=0;

		sprintf(str,"Edit User: %d (%s)",user.number,user.name[0]?user.name:user.alias);
		switch(uifc.list(WIN_ORG|WIN_ACT,0,0,0,&j,0,str,opt)) {
			case -1:
				return(0);

			case 0:
				user.misc ^= DELETED;
				putuserrec(cfg,user.number,U_MISC,8,ultoa(user.misc,str,16));
				if(user.misc & DELETED)
					putusername(cfg,user.number,nulstr);
				else
					putusername(cfg,user.number,user.alias);
				break;

			case 1:
				user.misc ^= INACTIVE;
				putuserrec(cfg,user.number,U_MISC,8,ultoa(user.misc,str,16));
				break;

			case 2:
				edit_personal(cfg,&user);
				break;

			case 3:
				edit_security(cfg,&user);
				break;

			case 4:
				edit_stats(cfg,&user);
				break;

			case 5:
				edit_settings(cfg,&user);
				break;

			case 6:
				edit_msgfile(cfg,&user);
				break;

			case 7:
				edit_comment(cfg,&user);
				break;

			default:
				break;
		}
	}

	return(0);
}

void free_opts(char **opt)
{
	int i;
	for(i=0; i<(MAX_OPTS+1) && opt[i]!=NULL; i++)
		FREE_AND_NULL(opt[i]);
	free(opt);
}

int finduser(scfg_t *cfg, user_t *user)
{
	int i,j,last;
	ushort un;
	char str[256];
	struct user_list **opt;
	int done=0;

	last=lastuser(cfg);
	if((opt=(struct user_list **)malloc(sizeof(struct user_list *)*(last+2)))==NULL)
		allocfail(sizeof(struct user_list *)*(last+2));
	memset(opt, 0, sizeof(struct user_list *)*(last+2));

	str[0]=0;
	uifc.input(WIN_MID|WIN_ACT|WIN_SAV,0,0,"Search String",str,LEN_NAME,K_EDIT);
	un=atoi(str);
	/* User List */
	done=0;
	while(!done) {
		j=0;
		for(i=1; i<=last; i++) {
			user->number=i;
			GETUSERDAT(cfg,user);
			if(strcasestr(user->alias, str)!=NULL || strcasestr(user->name, str)!=NULL || strcasestr(user->handle, str)!=NULL
					|| user->number==un) {
				FREE_AND_NULL(opt[j]);
				if((opt[j]=(struct user_list *)malloc(sizeof(struct user_list)))==NULL)
					allocfail(sizeof(struct user_list));
				sprintf(opt[j]->info,"%1.1s³%1.1s³ %-25.25s ³ %-25.25s",user->misc&DELETED?"*":" ",user->misc&INACTIVE?"*":" ",user->name,user->alias);
				opt[j++]->usernum=i;
			}
		}
		FREE_AND_NULL(opt[j]);
		if((opt[j]=(struct user_list *)malloc(sizeof(struct user_list)))==NULL)
			allocfail(sizeof(struct user_list));
		opt[j]->info[0]=0;
		i=0;
		switch(uifc.list(WIN_ORG|WIN_MID|WIN_ACT,0,0,0,&i,0,"D³I³ Real Name                 ³ Alias                    ",(char **)opt)) {
			case -1:
				done=1;
				break;
			default:
				edit_user(cfg, opt[i]->usernum);
				break;
		}
	}

	free_opts((char **)opt);
	return(0);
}

/* Get newly created Default User "New User" and set for Editing */
/*               Adapted from finduser function                  */

int getuser(scfg_t *cfg, user_t *user, char* str)
{
	int i,j,last;
	struct user_list **opt;
	int done=0;

	if((opt=(struct user_list **)malloc(sizeof(struct user_list *)*(MAX_OPTS+1)))==NULL)
		allocfail(sizeof(struct user_list *)*(MAX_OPTS+1));
	for(i=0;i<(MAX_OPTS+1);i++)
		opt[i]=NULL;

	/* User List */
	done=0;
	while(!done) {
		last=lastuser(cfg);
		j=0;
		for(i=1; i<=last; i++) {
			user->number=i;
			GETUSERDAT(cfg,user);
			if(strcasestr(user->alias, str)!=NULL || strcasestr(user->name, str)!=NULL || strcasestr(user->handle, str)!=NULL) {
				FREE_AND_NULL(opt[j]);
				if((opt[j]=(struct user_list *)malloc(sizeof(struct user_list)))==NULL)
					allocfail(sizeof(struct user_list));
				sprintf(opt[j]->info,"%1.1s³%1.1s³ %-25.25s ³ %-25.25s",user->misc&DELETED?"*":" ",user->misc&INACTIVE?"*":" ",user->name,user->alias);
				opt[j++]->usernum=i;
			}
		}
		FREE_AND_NULL(opt[j]);
		if((opt[j]=(struct user_list *)malloc(sizeof(struct user_list)))==NULL)
			allocfail(sizeof(struct user_list));
		opt[j]->info[0]=0;
		i=0;
		switch(uifc.list(WIN_ORG|WIN_MID|WIN_ACT,0,0,0,&i,0,"D³I³ Real Name                 ³ Alias                    ",(char **)opt)) {
			case -1:
				done=1;
				break;
			default:
				edit_user(cfg, opt[i]->usernum);
				done=1;
				break;
		}
	}
	free_opts((char **)opt);
	return(0);
}

/* Create a Default User: "New User" */
/*      Adapted from makeuser.c      */

int createdefaults()

{
	int		i;
	time_t	now;
	scfg_t cfg;
	user_t	user;
	char	error[512];
	char* environ;

	environ=getenv("SBBSCTRL");

	memset(&cfg,0,sizeof(cfg));
	cfg.size=sizeof(cfg);
	SAFECOPY(cfg.ctrl_dir,environ);

	if(chdir(cfg.ctrl_dir)!=0)
		lprintf("!ERROR changing directory to: %s", cfg.ctrl_dir);

	if(!load_cfg(&cfg,NULL,TRUE,error)) {
		lprintf("!ERROR loading configuration files: %s\n",error);
		exit(1);
	}

	now=time(NULL);

	memset(&user,0,sizeof(user));

    SAFECOPY(user.alias,"New Alias");
    SAFECOPY(user.name,"New User");
    SAFECOPY(user.handle,"New Handle");
    SAFECOPY(user.pass,"PASSWORD");
    SAFECOPY(user.birth,"01/01/80");

    SAFECOPY(user.address,"123 My Street");
    SAFECOPY(user.location,"City, St");
    SAFECOPY(user.zipcode,"123456");

	SAFECOPY(user.phone,"123-456-7890");

    user.level=10;

    SAFECOPY(user.comment," ");

    SAFECOPY(user.netmail,"name@address.com");

	user.level=cfg.new_level;
	user.flags1=cfg.new_flags1;
	user.flags2=cfg.new_flags2;
	user.flags3=cfg.new_flags3;
	user.flags4=cfg.new_flags4;
	user.rest=cfg.new_rest;
	user.exempt=cfg.new_exempt;

	user.cdt=cfg.new_cdt;
	user.min=cfg.new_min;
	user.freecdt=cfg.level_freecdtperday[user.level];

	if(cfg.total_fcomps)
		strcpy(user.tmpext,cfg.fcomp[0]->ext);
	else
		strcpy(user.tmpext,"ZIP");
	for(i=0;i<cfg.total_xedits;i++)
		if(!stricmp(cfg.xedit[i]->code,cfg.new_xedit))
			break;
	if(i<cfg.total_xedits)
		user.xedit=i+1;

	user.shell=cfg.new_shell;
	user.misc=(cfg.new_misc&~(DELETED|INACTIVE|QUIET|NETMAIL));
    user.misc^=AUTOTERM;
    user.misc^=ANSI;
    user.misc^=COLOR;
	user.qwk=QWK_DEFAULT;
	user.firston=now;
	user.laston=now;
	user.pwmod=now;
	user.logontime=now;
	user.sex=' ';
	user.prot=cfg.new_prot;
	if(cfg.new_expire)
		user.expire=now+((long)cfg.new_expire*24L*60L*60L);

	if((i=matchuser(&cfg,user.alias,FALSE))!=0) {
	    lprintf("Error!  Default User already in Userfile");
		return(2);
	}

	if(user.handle[0]==0)
		SAFECOPY(user.handle,user.alias);
	if(user.name[0]==0)
		SAFECOPY(user.name,user.alias);

	if((i=newuserdat(&cfg, &user))!=0) {
	    lprintf("%s %d", "Error creating Default User.  Error # ",i);
		return(i);
	}
	return(i);
}

int main(int argc, char** argv)  {
	char**	opt;
	char**	mopt;
	int		main_dflt=0;
	int		main_bar=0;
	char	revision[16];
	char	str[256],ctrl_dir[MAX_PATH + 1];
	char	title[256];
	int		i,j;
	scfg_t	cfg;
	int		done;
	int		last;
	user_t	user;
	int		edtuser=0;
	int		ciolib_mode=CIOLIB_MODE_AUTO;

	/******************/
	/* Ini file stuff */
	/******************/
	char	ini_file[MAX_PATH+1];
	FILE*				fp;
	bbs_startup_t		bbs_startup;

	sscanf("$Revision: 1.64 $", "%*s %s", revision);

    printf("\nSynchronet User Editor %s-%s  Copyright %s "
        "Rob Swindell\n",revision,PLATFORM_DESC,&__DATE__[7]);

	SAFECOPY(ctrl_dir, get_ctrl_dir());

	gethostname(str,sizeof(str)-1);

	sbbs_get_ini_fname(ini_file, ctrl_dir, str);

	/* Initialize BBS startup structure */
    memset(&bbs_startup,0,sizeof(bbs_startup));
    bbs_startup.size=sizeof(bbs_startup);
    strcpy(bbs_startup.ctrl_dir,ctrl_dir);

	/* Read .ini file here */
	if(ini_file[0]!=0 && (fp=fopen(ini_file,"r"))!=NULL) {
		printf("Reading %s\n",ini_file);
		/* We call this function to set defaults, even if there's no .ini file */
		sbbs_read_ini(fp, ini_file,
			NULL,		/* global_startup */
			NULL, &bbs_startup,
			NULL, NULL, /* ftp_startup */
			NULL, NULL, /* web_startup */
			NULL, NULL, /* mail_startup */
			NULL, NULL  /* services_startup */
			);

		/* close .ini file here */
		if(fp!=NULL)
			fclose(fp);
	}
	chdir(bbs_startup.ctrl_dir);

	/* Read .cfg files here */
    memset(&cfg,0,sizeof(cfg));
	cfg.size=sizeof(cfg);
	SAFECOPY(cfg.ctrl_dir,bbs_startup.ctrl_dir);
	if(!load_cfg(&cfg, NULL, TRUE, str)) {
		printf("ERROR! %s\n",str);
		exit(1);
	}
	prep_dir(cfg.data_dir, cfg.temp_dir, sizeof(cfg.temp_dir));

    memset(&uifc,0,sizeof(uifc));
	uifc.mode|=UIFC_NOCTRL;

	uifc.esc_delay=500;

	for(i=1;i<argc;i++) {
        if(argv[i][0]=='-')
            switch(toupper(argv[i][1])) {
                case 'C':
        			uifc.mode|=UIFC_COLOR;
                    break;
                case 'L':
                    uifc.scrn_len=atoi(argv[i]+2);
                    break;
                case 'E':
                    uifc.esc_delay=atoi(argv[i]+2);
                    break;
				case 'I':
					switch(toupper(argv[i][2])) {
						case 'A':
							ciolib_mode=CIOLIB_MODE_ANSI;
							break;
						case 'C':
							ciolib_mode=CIOLIB_MODE_CURSES;
							break;
						case 0:
							printf("NOTICE: The -i option is deprecated, use -if instead\n");
							SLEEP(2000);
						case 'F':
							ciolib_mode=CIOLIB_MODE_CURSES_IBM;
							break;
						case 'I':
							ciolib_mode=CIOLIB_MODE_CURSES_ASCII;
							break;
						case 'X':
							ciolib_mode=CIOLIB_MODE_X;
							break;
						case 'W':
							ciolib_mode=CIOLIB_MODE_CONIO;
							break;
						default:
							goto USAGE;
					}
					break;
                default:
					USAGE:
                    printf("\nusage: %s [ctrl_dir] [options]"
                        "\n\noptions:\n\n"
                        "-c  =  force color mode\n"
                        "-e# =  set escape delay to #msec\n"
						"-iX =  set interface mode to X (default=auto) where X is one of:\n"
#ifdef __unix__
						"       X = X11 mode\n"
						"       C = Curses mode\n"
						"       F = Curses mode with forced IBM charset\n"
						"       I = Curses mode with forced ASCII charset\n"
#else
						"       W = Win32 native mode\n"
#endif
						"       A = ANSI mode\n"
                        "-l# =  set screen lines to #\n"
						,argv[0]
                        );
        			exit(0);
		}
		if(atoi(argv[i]))
			edtuser=atoi(argv[i]);
    }

#ifdef __unix__
	signal(SIGPIPE, SIG_IGN);
#endif

	uifc.size=sizeof(uifc);
	i=initciolib(ciolib_mode);
	if(i!=0) {
    	printf("ciolib library init returned error %d\n",i);
    	exit(1);
	}
	i=uifcini32(&uifc);  /* curses */
	if(i!=0) {
		printf("uifc library init returned error %d\n",i);
		exit(1);
	}

	if((opt=(char **)malloc(sizeof(char *)*(MAX_OPTS+1)))==NULL)
		allocfail(sizeof(char *)*(MAX_OPTS+1));
	for(i=0;i<(MAX_OPTS+1);i++)
		if((opt[i]=(char *)malloc(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	if((mopt=(char **)alloca(sizeof(char *)*5))==NULL)
		allocfail(sizeof(char *)*5);
	for(i=0;i<5;i++)
		if((mopt[i]=(char *)alloca(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	sprintf(title,"Synchronet User Editor %s-%s",revision,PLATFORM_DESC);
	if(uifc.scrn(title)) {
		printf(" USCRN (len=%d) failed!\n",uifc.scrn_len+1);
		bail(1);
	}

	if(edtuser) {
		edit_user(&cfg, edtuser);
		bail(0);
	}

	i=0;
	strcpy(mopt[i++],"New User");
	strcpy(mopt[i++],"Find User");
	strcpy(mopt[i++],"List All User Records");
	strcpy(mopt[i++],"List Active User Records");
	mopt[i][0]=0;

	uifc.helpbuf=	"`User Editor\n"
					"`-----------\n\n"
					"`New User  : `Add a new user.  This will created a default user using\n"
					"            some default entries that you can then edit.\n"
					"`Find User : `Find a user using full or partial search name\n"
					"`List All User Records: `Display all user records (including deleted/inactive)\n"
					"`List Active User Records: `Display active user records only\n"
					" Users can be edited from lists by highlighting a user and pressing ~Enter~";

	while(1) {
		j=uifc.list(WIN_L2R|WIN_ESC|WIN_ACT|WIN_DYN|WIN_ORG|WIN_EXTKEYS,0,5,0,&main_dflt,&main_bar
			,title,mopt);

		if(j == -2)
			continue;

		if(j==-8) {	/* CTRL-F */
			/* Find User */
			finduser(&cfg,&user);
		}

		if(j <= -2)
			continue;

		if(j==-1) {
			uifc.helpbuf=	"`Exit Synchronet User Editor\n"
							"`---------------------------\n\n"
							"If you want to exit the Synchronet user editor,\n"
							"select `Yes`. Otherwise, select `No` or hit ~ ESC ~.";
			if(confirm("Exit Synchronet User Editor")==1)
				bail(0);
			continue;
		}

		if(j==0) {
			/* New User */
			    createdefaults();
			    lprintf("Please edit defaults using next screen.");
			    getuser(&cfg,&user,"New User");
		}
		if(j==1) {
		    /* Find User */
			finduser(&cfg,&user);
		}
		if(j==2) {
			/* List All Users */
			done=0;
			while(!done) {
				last=lastuser(&cfg);
				for(i=1; i<=last; i++) {
					user.number=i;
					GETUSERDAT(&cfg,&user);
					sprintf(opt[i-1],"%1.1s³%1.1s³ %-25.25s ³ %-25.25s",user.misc&DELETED?"*":" ",user.misc&INACTIVE?"*":" ",user.name,user.alias);
				}
				opt[i-1][0]=0;
				i=0;
				switch(uifc.list(WIN_ORG|WIN_MID|WIN_ACT,0,0,0,&i,0,"D³I³ Real Name                 ³ Alias                    ",opt)) {
					case -1:
						done=1;
						break;
					default:
						edit_user(&cfg, i+1);
						break;
				}
			}
		}
		if(j==3) {
			/* List Active Users */
			done=0;
			while(!done) {
				last=lastuser(&cfg);
				for(i=1,j=0; i<=last; i++) {
					user.number = i;
					GETUSERDAT(&cfg, &user);
					sprintf(opt[j++], "%-4u ³ %-25.25s ³ %-25.25s", i, user.name, user.alias);
				}
				opt[j][0] = 0;
				i=0;
				switch(uifc.list(WIN_ORG|WIN_MID|WIN_ACT,0,0,0,&i,0,"Num  ³ Real Name                 ³ Alias                    ",opt)) {
					case -1:
						done = 1;
						break;
					default:
						edit_user(&cfg, atoi(opt[i]));
						break;
				}
			}
		}
	}
	free_opts(opt);
}



