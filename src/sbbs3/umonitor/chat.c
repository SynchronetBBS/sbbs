/* chat.c */

/* Synchronet for *nix sysop chat routines */

/* $Id: chat.c,v 1.21 2019/08/31 22:33:26 rswindell Exp $ */

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

#include <genwrap.h>
#include "ciolib.h"
#define __COLORS
#include "sbbs.h"
#include "chat.h"

#define PCHAT_LEN 1000		/* Size of Private chat file */

typedef struct {
	int	left;
	int	top;
	int	right;
	int	bottom;
	int	x;
	int	y;
	int colour;
} WINDOW;

WINDOW	uwin;
WINDOW	swin;

int togglechat(scfg_t *cfg, int node_num, node_t *node, int on)
{
    static int  org_act;

	int nodefile = -1;
	if(getnodedat(cfg,node_num,node,TRUE,&nodefile)) {
		return(FALSE);
	}
    if(on) {
        org_act=node->action;
        if(org_act==NODE_PCHT)
            org_act=NODE_MAIN;
        node->misc|=NODE_LCHAT;
    } else {
        node->action=org_act;
        node->misc&=~NODE_LCHAT;
    }
	putnodedat(cfg,node_num,node,TRUE,nodefile);
    return(TRUE);
}

void drawchatwin(box_t *boxch, char* topname, char* botname) {
	struct text_info ti;
	int	i;

	_wscroll=0;
	gettextinfo(&ti);
	textcolor(WHITE);
	textbackground(BLACK);
	clrscr();
	uwin.top=1;
	uwin.left=1;
	uwin.right=ti.screenwidth;
	uwin.bottom=ti.screenheight/2-1;
	uwin.colour=GREEN;
	swin.top=ti.screenheight/2+1;
	swin.left=1;
	swin.right=ti.screenwidth;
	swin.bottom=ti.screenheight;
	swin.colour=LIGHTGREEN;

	gotoxy(1,ti.screenheight/2);
	putch(196);
	putch(196);
	putch(196);
	putch(196);
	putch(180);
	cputs(topname);
	putch(195);
	for(i=wherex();i<=ti.screenwidth;i++)
		putch(196);

	_wscroll=1;
}

int chatchar(WINDOW *win, int ch) {
	window(win->left,win->top,win->right,win->bottom);
	gotoxy(win->x,win->y);
	textcolor(win->colour);
	putch(ch);
	if(ch=='\r')
		putch('\n');
	win->x=wherex();
	win->y=wherey();
	return(0);
}

int chat(scfg_t *cfg, int nodenum, node_t *node, box_t *boxch, void(*timecallback)(void)) {
	int		in,out;
	char	inpath[MAX_PATH];
	char	outpath[MAX_PATH];
	char	usrname[128];
	char	*p;
	char	ch;
	time_t	now;
	time_t	last_nodechk=0;
	struct text_info ti;
	char	*buf;

	gettextinfo(&ti);
	if((buf=(char *)alloca(ti.screenwidth*ti.screenheight*2))==NULL) {
		return(-1);
	}

	if(getnodedat(cfg,nodenum,node,FALSE,NULL))
		return(-1);

	username(cfg,node->useron,usrname);

	gettext(1,1,ti.screenwidth,ti.screenheight,buf);
	drawchatwin(boxch,usrname,cfg->sys_op);

	sprintf(outpath,"%slchat.dab",cfg->node_path[nodenum-1]);
	if((out=sopen(outpath,O_RDWR|O_CREAT|O_BINARY,O_DENYNONE
		,DEFFILEMODE))==-1)
		return(-1);

	sprintf(inpath,"%schat.dab",cfg->node_path[nodenum-1]);
	if((in=sopen(inpath,O_RDWR|O_CREAT|O_BINARY,O_DENYNONE
		,DEFFILEMODE))==-1) {
		close(out);
		return(-1);
    }

	if((p=(char *)alloca(PCHAT_LEN))==NULL) {
		close(in);
		close(out);
		return(-1);
    }
	memset(p,0,PCHAT_LEN);
	write(in,p,PCHAT_LEN);
	write(out,p,PCHAT_LEN);
	lseek(in,0,SEEK_SET);
	lseek(out,0,SEEK_SET);

	togglechat(cfg,nodenum,node,TRUE);

	while(in != -1) {
		
		now=time(NULL);
		if(now!=last_nodechk) {

			if(timecallback != NULL)
				timecallback();

			if(getnodedat(cfg,nodenum,node,FALSE,NULL)!=0)
				break;
			last_nodechk=now;
		}
		if(node->misc&NODE_LCHAT) {
			if(kbhit()) {
				ch=getch();
				if(ch==ESC || ch==3) {
					close(in);
					in=-1;
				}
				if(ch==0 || ch=='\xe0') {		/* Special keys... eat 'em. */
					getch();
				}
			}
			YIELD();
			continue;
		}

		if(node->status==NODE_WFC || node->status>NODE_QUIET || node->action!=NODE_PCHT) {
			close(in);
			in=-1;
		}

		utime(inpath,NULL);
		_setcursortype(_NORMALCURSOR);
		while(1) {
			switch(read(in,&ch,1)) {
				case -1:
					close(in);
					in=-1;
					break;

				case 0:
					lseek(in,0,SEEK_SET);	/* Wrapped */
					continue;

				case 1:
					lseek(in,-1L,SEEK_CUR);
					if(ch) {
						chatchar(&uwin,ch);
						ch=0;
						write(in,&ch,1);
						continue;
					}
					break;
			}
			break;
		}

		if(kbhit()) {
			ch=getch();
			switch(ch)  {
				case 0:			/* Special Chars */
				case '\xe0':
					ch=0;
					getch();
					break;

				case ESC:
				case 3:
					close(in);
					in=-1;
					continue;

				default:
					if(lseek(out,0,SEEK_CUR)>=PCHAT_LEN)
						lseek(out,0,SEEK_SET);
					chatchar(&swin,ch);
					switch(write(out,&ch,1)) {
						case -1:
							close(in);
							in=-1;
							break;
					}
					break;
			}
			utime(outpath,NULL);
		}
	}
	if(in != -1)
		close(in);
	if(out != -1)
		close(out);
	togglechat(cfg,nodenum,node,FALSE);
	puttext(1,1,ti.screenwidth,ti.screenheight,buf);
	window(ti.winleft,ti.wintop,ti.winright,ti.winbottom);
	gotoxy(ti.curx,ti.cury);
	textattr(ti.attribute);
	return(0);
}
