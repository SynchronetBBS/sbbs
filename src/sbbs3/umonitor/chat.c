/* chat.c */

/* Synchronet for *nix sysop chat routines */

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

#include <sys/types.h>
#include <utime.h>

#include "sbbs.h"
#include "chat.h"

#define PCHAT_LEN 1000		/* Size of Private chat file */

bool togglechat(scfg_t *cfg, int node_num, node_t *node, bool on)
{
    static int  org_act;

	int nodefile;
	if(getnodedat(cfg,node_num,node,&nodefile)) {
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
	putnodedat(cfg,node_num,node,nodefile);
    return(TRUE);
}

void wsetcolor(WINDOW *win, int fg, int bg)  {
	int   attrs=A_NORMAL;
	short	colour;
	int		lastfg=0;
	int		lastbg=0;

	if (lastfg==fg && lastbg==bg)
		return;

	lastfg=fg;
	lastbg=bg;

	if (fg & 8)  {
		attrs |= A_BOLD;
	}
	wattrset(win, attrs);
	colour = COLOR_PAIR( ((fg&7)|(bg<<3))+1 );
	#ifdef NCURSES_VERSION_MAJOR
	wcolor_set(win, colour,NULL);
	#endif
	wbkgdset(win, colour);
}

void drawchatwin(WINDOW **uwin, WINDOW **swin, box_t *boxch) {
	int maxy,maxx;

	endwin();
	getmaxyx(stdscr,maxy,maxx);
	*uwin=newwin(maxy/2,maxx,0,0);
	*swin=newwin(0,maxx,maxy/2,0);
	wsetcolor(*uwin,YELLOW,BLUE);
	wclear(*uwin);
	wborder(*uwin,  boxch->ls, boxch->rs, boxch->ts, boxch->bs, boxch->tl, boxch->tr, boxch->bl, boxch->br);
	wmove(*uwin,0,5);
	waddstr(*uwin,"Remote User");
	wmove(*uwin,1,2);
	scrollok(*uwin,TRUE);
	wrefresh(*uwin);
	wsetcolor(*swin,YELLOW,BLUE);
	wclear(*swin);
	wborder(*swin,  boxch->ls, boxch->rs, boxch->ts, boxch->bs, boxch->tl, boxch->tr, boxch->bl, boxch->br);
	wmove(*swin,0,5);
	waddstr(*swin,"Sysop");
	wmove(*swin,1,2);
	scrollok(*swin,TRUE);
	wrefresh(*swin);
}

int chatchar(WINDOW *win, int ch, box_t *boxch) {
	int	maxy,maxx;
	int	cury,curx;
	getmaxyx(win,maxy,maxx);
	getyx(win,cury,curx);
	switch(ch) {
		case 8:
			curx-=1;
			if(curx<2)
				curx+=1;
			mvwaddch(win,cury,curx,' ');
			wmove(win,cury,curx);
			wrefresh(win);
			break;
		
		case '\r':
		case '\n':
			curx=2;
			cury+=1;
			wmove(win,cury,curx);
			if(cury>=maxy-1) {
				wscrl(win,1);
				cury-=1;
				wmove(win,cury,curx-1);
				whline(win,' ',maxx-2);
				wborder(win, boxch->ls, boxch->rs, boxch->ts, boxch->bs, boxch->tl, boxch->tr, boxch->bl, boxch->br);
				wmove(win,0,5);
				waddstr(win,"Sysop");
				wmove(win,cury,curx);
			}
			wrefresh(win);
			break;
			
		default:
			waddch(win,ch);
			getyx(win,cury,curx);
			if(curx>=maxx-2) {
				curx=2;
				cury+=1;
				if(cury>=maxy-1) {
					wscrl(win,1);
					cury-=1;
					wmove(win,cury,curx-1);
					whline(win,' ',maxx-2);
					wborder(win, boxch->ls, boxch->rs, boxch->ts, boxch->bs, boxch->tl, boxch->tr, boxch->bl, boxch->br);
					wmove(win,0,5);
					waddstr(win,"Sysop");
				}
				wmove(win,cury,curx);
			}
			wrefresh(win);
	}
	return(0);
}

int chat(scfg_t *cfg, int nodenum, node_t *node, box_t *boxch) {
	WINDOW	*uwin;
	WINDOW	*swin;
	int		in,out;
	char	inpath[MAX_PATH];
	char	outpath[MAX_PATH];
	char	*p;
	char	ch;

	drawchatwin(&uwin,&swin,boxch);

	if(getnodedat(cfg,nodenum,node,NULL)) {
		return(-1);
	}

	sprintf(outpath,"%slchat.dab",cfg->node_path[nodenum-1]);
	if((out=sopen(outpath,O_RDWR|O_CREAT|O_BINARY,O_DENYNONE
		,S_IREAD|S_IWRITE))==-1) {
		return(-1);
	}

	sprintf(inpath,"%schat.dab",cfg->node_path[nodenum-1]);
	if((in=sopen(inpath,O_RDWR|O_CREAT|O_BINARY,O_DENYNONE
		,S_IREAD|S_IWRITE))==-1) {
		close(out);
		return(-1);
    }

	if((p=(char *)MALLOC(PCHAT_LEN))==NULL) {
		close(in);
		close(out);
		return(-1);
    }
	memset(p,0,PCHAT_LEN);
	write(in,p,PCHAT_LEN);
	write(out,p,PCHAT_LEN);
	FREE(p);
	lseek(in,0,SEEK_SET);
	lseek(out,0,SEEK_SET);
	halfdelay(1);

	togglechat(cfg,nodenum,node,TRUE);

	while(in != -1) {
		utime(outpath,NULL);
		utime(inpath,NULL);
		
		if(getnodedat(cfg,nodenum,node,NULL)) {
			break;
		}
		if(node->misc&NODE_LCHAT) {
			if((ch=wgetch(swin))) {
				if(ch==ESC || ch==3) {
					close(in);
					in=-1;
				}
			}
			YIELD();
			continue;
		}
		
		if(!node->status || node->status>NODE_QUIET || node->action!=NODE_PCHT) {
			close(in);
			in=-1;
		}
		switch (read(in,&ch,1)) {
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
					chatchar(uwin,ch,boxch);
					ch=0;
					write(in,&ch,1);
				}
		}
		if((ch=wgetch(swin))) {
			switch(ch)  {
				case 0:
				case ERR:
					ch=0;
					write(out,&ch,1);
					lseek(out,-1,SEEK_CUR);
					break;
					
				case ESC:
				case 3:
					close(in);
					in=-1;
					continue;
					
				default:
					if(lseek(out,0,SEEK_CUR)>=PCHAT_LEN)
						lseek(out,0,SEEK_SET);
					chatchar(swin,ch,boxch);
					switch(write(out,&ch,1)) {
						case -1:
							close(in);
							in=-1;
							break;
					}
			}
		}
	}
	if(in != -1)
		close(in);
	if(out != -1)
		close(out);
	nocbreak();
	cbreak();
	togglechat(cfg,nodenum,node,FALSE);
	delwin(uwin);
	delwin(swin);
	touchwin(stdscr);
	refresh();
	return(0);
}
