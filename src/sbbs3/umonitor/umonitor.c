/* umonitor.c */

/* Synchronet for *nix node activity monitor */

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
#include "conwrap.h"	/* this has to go BEFORE curses.h so getkey() can be macroed around */
#include <curses.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#ifdef __QNX__
#include <string.h>
#endif
#include <stdio.h>
#include <unistd.h>
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
#include "spyon.h"
#include "chat.h"

#define CTRL(x) (x&037)

/********************/
/* Global Variables */
/********************/
uifcapi_t uifc; /* User Interface (UIFC) Library API */
char tmp[256];
const char *YesStr="Yes";
const char *NoStr="No";
box_t boxch;

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

void node_toggles(scfg_t *cfg,int nodenum)  {
	int nodefile;
	char**	opt;
	int		i,j;
	node_t	node;
	int		save=0;

	if((opt=(char **)MALLOC(sizeof(char *)*(MAX_OPTS+1)))==NULL)
		allocfail(sizeof(char *)*(MAX_OPTS+1));
	for(i=0;i<(MAX_OPTS+1);i++)
		if((opt[i]=(char *)MALLOC(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	i=0;
	uifc.helpbuf=	"`Node Toggles:`\n"
					"\nToDo: Add help (Mention that changes take effect immediately)";
	while(save==0) {
		if(getnodedat(cfg,nodenum,&node,&nodefile)) {
			uifc.msg("Error reading node data!");
			break;
		}
		j=0;
		sprintf(opt[j++],"%-30s%3s","Locked for SysOps only",node.misc&NODE_LOCK ? YesStr : NoStr);
		sprintf(opt[j++],"%-30s%3s","Interrupt (Hangup)",node.misc&NODE_INTR ? YesStr : NoStr);
		sprintf(opt[j++],"%-30s%3s","Page disabled",node.misc&NODE_POFF ? YesStr : NoStr);
		sprintf(opt[j++],"%-30s%3s","Activity alert disabled",node.misc&NODE_AOFF ? YesStr : NoStr);
		sprintf(opt[j++],"%-30s%3s","Re-run on logoff",node.misc&NODE_RRUN ? YesStr : NoStr);
		sprintf(opt[j++],"%-30s%3s","Down node after logoff",(node.misc&NODE_DOWN || (node.status==NODE_OFFLINE)) ? YesStr : NoStr);
		sprintf(opt[j++],"%-30s%3s","Reset private chat",node.misc&NODE_RPCHT ? YesStr : NoStr);
		opt[j][0]=0;

		switch(uifc.list(WIN_MID,0,0,0,&i,0,"Node Toggles",opt)) {
			case 0:	/* Locked */
				node.misc ^= NODE_LOCK;
				break;

			case 1:	/* Interrupt */
				node.misc ^= NODE_INTR;
				break;

			case 2:	/* Page disabled */
				node.misc ^= NODE_POFF;
				break;

			case 3:	/* Activity alert */
				node.misc ^= NODE_AOFF;
				break;

			case 4:	/* Re-run */
				node.misc ^= NODE_RRUN;
				break;

			case 5:	/* Down */
				if(node.status != NODE_WFC && node.status != NODE_OFFLINE)
					node.misc ^= NODE_DOWN;
				else {
					if(node.status!=NODE_OFFLINE)
						node.status=NODE_OFFLINE;
					else
						node.status=NODE_WFC;
				}
				break;

			case 6:	/* Reset chat */
				node.misc ^= NODE_RPCHT;
				break;
				
			case -1:
				save=1;
				break;
				
			default:
				uifc.msg("Option not implemented");
				continue;
		}
		putnodedat(cfg,nodenum,&node,nodefile);
	}
}

int dospy(int nodenum, bbs_startup_t *bbs_startup)  {
	char str[80],str2[80];
	int i;

	if(bbs_startup->temp_dir[0])
    	snprintf(str,sizeof(str),"%slocalspy%d.sock", bbs_startup->temp_dir, nodenum);
	else
		snprintf(str,sizeof(str),"%slocalspy%d.sock", bbs_startup->ctrl_dir, nodenum);
	endwin();
	i=spyon(str);
	refresh();
	switch(i) {
		case SPY_NOSOCKET:
			uifc.msg("Could not create socket");
			return(-1);
					
		case SPY_NOCONNECT:
			sprintf(str2,"Failed to connect to %s",str);
			uifc.msg(str2);
			return(-1);

		case SPY_SELECTFAILED:
			uifc.msg("select() failed, connection terminated.");
			return(-1);

		case SPY_SOCKETLOST:
			uifc.msg("Spy socket lost");
			return(-1);
							
		case SPY_STDINLOST:
			uifc.msg("STDIN has gone away... you probably can't close this window.  :-)");
			return(-1);
							
		case SPY_CLOSED:
			break;
			
		default:
			sprintf(str,"Unknown return code %d",i);
			uifc.msg(str);
			return(-1);
	}
	return(0);
}

int sendmessage(scfg_t *cfg, int nodenum,node_t *node)  {
	char str[80],str2[80];

	uifc.input(WIN_MID,0,0,"Telegram",str2,58,K_WRAP|K_MSG);
	sprintf(str,"\1n\1y\1hMessage From Sysop:\1w %s\r\n",str2);
	if(getnodedat(cfg,nodenum,node,NULL))
		return(-1);
	if(node->useron==0)
		return(-1);
	putsmsg(cfg, node->useron, str);
	return(0);
}

int clearerrors(scfg_t *cfg, int nodenum, node_t *node) {
	int nodefile;
	if(getnodedat(cfg,nodenum,node,&nodefile)) {
		uifc.msg("getnodedat() failed! (Nothing done)");
		return(-1);
	}
	node->errors=0;
	putnodedat(cfg,nodenum,node,nodefile);
	return(0);
}

int main(int argc, char** argv)  {
	char**	opt;
	char**	mopt;
	int		main_dflt=0;
	int		main_bar=0;
	char revision[16];
	char str[256],ctrl_dir[41],*p;
	char title[256];
	int i,j;
	node_t node;
	char	*buf;
	int		buffile;
	int		nodefile;
	box_t	boxch;
	scfg_t	cfg;
/******************/
/* Ini file stuff */
/******************/
	char	host_name[128]="";
	char	ini_file[MAX_PATH+1];
	FILE*				fp;
	bbs_startup_t		bbs_startup;

	sscanf("$Revision$", "%*s %s", revision);

    printf("\nSynchronet UNIX Monitor %s-%s  Copyright 2003 "
        "Rob Swindell\n",revision,PLATFORM_DESC);

	p=getenv("SBBSCTRL");
	if(p==NULL) {
		printf("\7\nSBBSCTRL environment variable is not set.\n");
		printf("This environment variable must be set to your CTRL directory.");
		printf("\nExample: SET SBBSCTRL=/sbbs/ctrl\n");
		exit(1); }

	sprintf(ctrl_dir,"%.40s",p);
	if(ctrl_dir[strlen(ctrl_dir)-1]!='\\'
		&& ctrl_dir[strlen(ctrl_dir)-1]!='/')
		strcat(ctrl_dir,"/");

	gethostname(host_name,sizeof(host_name)-1);
	sprintf(ini_file,"%s%c%s.ini",ctrl_dir,BACKSLASH,host_name);
#if defined(PREFIX)
	if(!fexistcase(ini_file))
		sprintf(ini_file,"%s/etc/sbbs.ini",PREFIX);
#endif
	if(!fexistcase(ini_file))
		sprintf(ini_file,"%s%csbbs.ini",ctrl_dir,BACKSLASH);

	/* Initialize BBS startup structure */
    memset(&bbs_startup,0,sizeof(bbs_startup));
    bbs_startup.size=sizeof(bbs_startup);
    strcpy(bbs_startup.ctrl_dir,ctrl_dir);

	/* Read .ini file here */
	if(ini_file[0]!=0 && (fp=fopen(ini_file,"r"))!=NULL) {
		printf("Reading %s\n",ini_file);
	}
	/* We call this function to set defaults, even if there's no .ini file */
	sbbs_read_ini(fp, 
		NULL, &bbs_startup, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

	/* close .ini file here */
	if(fp!=NULL)
		fclose(fp);

	chdir(bbs_startup.ctrl_dir);
	
	/* Read .cfg files here */
	cfg.size=sizeof(cfg);
	if(!read_main_cfg(&cfg, str)) {
		printf("ERROR! %s\n",str);
		exit(1);
	}

	if(!read_xtrn_cfg(&cfg, str)) {
		printf("ERROR! %s\n",str);
		exit(1);
	}

/*	if(!read_node_cfg(&cfg, str)) {
		printf("ERROR! %s\n",str);
		exit(1);
	} */

    memset(&uifc,0,sizeof(uifc));

	uifc.esc_delay=500;

	boxch.ls=ACS_VLINE; 
	boxch.rs=ACS_VLINE;
	boxch.ts=ACS_HLINE;
	boxch.bs=ACS_HLINE;
	boxch.tl=ACS_ULCORNER;
	boxch.tr=ACS_URCORNER;
	boxch.bl=ACS_LLCORNER;
	boxch.br=ACS_LRCORNER;
	for(i=1;i<argc;i++) {
        if(argv[i][0]=='-'
            )
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
					/* Set up ex-ascii codes */
					boxch.ls=186; 
					boxch.rs=186;
					boxch.ts=205;
					boxch.bs=205;
					boxch.tl=201;
					boxch.tr=187;
					boxch.bl=200;
					boxch.br=188;
					uifc.mode|=UIFC_IBM;
					break;
                default:
                    printf("\nusage: %s [ctrl_dir] [options]"
                        "\n\noptions:\n\n"
                        "-c  =  force color mode\n"
                        "-e# =  set escape delay to #msec\n"
						"-i  =  force IBM charset\n"
                        "-l# =  set screen lines to #\n"
						,argv[0]
                        );
        			exit(0);
           }
    }

	signal(SIGPIPE, SIG_IGN);   

	uifc.size=sizeof(uifc);
	i=uifcinic(&uifc);  /* curses */
	if(i!=0) {
		printf("uifc library init returned error %d\n",i);
		exit(1);
	}

	if((opt=(char **)MALLOC(sizeof(char *)*(MAX_OPTS+1)))==NULL)
		allocfail(sizeof(char *)*(MAX_OPTS+1));
	for(i=0;i<(MAX_OPTS+1);i++)
		if((opt[i]=(char *)MALLOC(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	if((mopt=(char **)MALLOC(sizeof(char *)*MAX_OPTS))==NULL)
		allocfail(sizeof(char *)*MAX_OPTS);
	for(i=0;i<MAX_OPTS;i++)
		if((mopt[i]=(char *)MALLOC(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	sprintf(title,"Synchronet UNIX Monitor %s-%s",revision,PLATFORM_DESC);
	if(uifc.scrn(title)) {
		printf(" USCRN (len=%d) failed!\n",uifc.scrn_len+1);
		bail(1);
	}

	while(1) {
		for(i=1;i<=cfg.sys_nodes;i++) {
			if((j=getnodedat(&cfg,i,&node,NULL)))
				sprintf(mopt[i-1],"Error reading node data (%d)!",j);
			else
				sprintf(mopt[i-1],"%3d: %s",i,nodestatus(&cfg,&node,str,71));
		}
		mopt[i-1][0]=0;

		uifc.helpbuf=	"`Synchronet Monitor:`\n"
						"\nCTRL-E displays the error log"
						"\nF12 spys on the currently selected node"
						"\nF11 send message to the currently selected node"
						"\nF10 Chats with the user on the currently selected node"
						"\nDEL Clear errors on currently selected node"
						"\nCTRL-L Lock node toggle"
						"\nCTRL-R Rerun node"
						"\nCTRL-D Down node toggle"
						"\nCTRL-I Interrupt node"
						"\nToDo: Add more help. (Explain what you're looking at)";
						
		j=uifc.list(WIN_ORG|WIN_MID|WIN_ESC|WIN_ACT|WIN_DYN,0,0,70,&main_dflt,&main_bar
			,title,mopt);

		if(j==-7) {	/* CTRL-E */
			sprintf(str,"%s/error.log",cfg.data_dir);
			if(fexist(str)) {
				if((buffile=sopen(str,O_RDONLY,SH_DENYWR))>=0) {
					j=filelength(buffile);
					if((buf=(char *)MALLOC(j+1))!=NULL) {
						read(buffile,buf,j);
						close(buffile);
						*(buf+j)=0;
						uifc.showbuf(buf,"Error Log",0);
						free(buf);
						continue;
					}
					close(buffile);
					uifc.msg("Error allocating memory for the error log");
					continue;
				}
				uifc.msg("Error opening error log");
			}
			else {
				uifc.msg("Error log does not exist");
			}
			continue;
		}
		
		if(j==-2-KEY_DC) {	/* Clear errors */
			clearerrors(&cfg, main_dflt+1,&node);
			continue;
		}

		if(j==-2-KEY_F(10)) {	/* Chat */
			if(getnodedat(&cfg,main_dflt+1,&node,NULL)) {
				uifc.msg("Error reading node data!");
				continue;
			}
			if((node.status==NODE_INUSE) && node.useron)
				chat(&cfg,main_dflt+1,&node,&boxch,uifc.timedisplay);
			continue;
		}

		if(j==-2-KEY_F(11)) {	/* Send message */
			sendmessage(&cfg, main_dflt+1,&node);
			continue;
		}
		
		if(j==-2-KEY_F(12)) {	/* Spy */
			dospy(main_dflt+1,&bbs_startup);
			continue;
		}
		
		if(j==-2-CTRL('l')) {	/* Lock node */
			if(getnodedat(&cfg,main_dflt+1,&node,&nodefile)) {
				uifc.msg("Error reading node data!");
				continue;
			}
			node.misc^=NODE_LOCK;
			putnodedat(&cfg,main_dflt+1,&node,nodefile);
			continue;
		}
		
		if(j==-2-CTRL('r')) {	/* Rerun node */
			if(getnodedat(&cfg,main_dflt+1,&node,&nodefile)) {
				uifc.msg("Error reading node data!");
				continue;
			}
			node.misc^=NODE_RRUN;
			putnodedat(&cfg,main_dflt+1,&node,nodefile);
			continue;
		}

		if(j==-2-CTRL('d')) {	/* Down node */
			if(getnodedat(&cfg,main_dflt+1,&node,&nodefile)) {
				uifc.msg("Error reading node data!");
				continue;
			}
			if(node.status != NODE_WFC && node.status != NODE_OFFLINE)
				node.misc ^= NODE_DOWN;
			else {
				if(node.status!=NODE_OFFLINE)
					node.status=NODE_OFFLINE;
				else
					node.status=NODE_WFC;
			}
			putnodedat(&cfg,main_dflt+1,&node,nodefile);
			continue;
		}

		if(j==-2-CTRL('i')) {	/* Interrupt node */
			if(getnodedat(&cfg,main_dflt+1,&node,&nodefile)) {
				uifc.msg("Error reading node data!");
				continue;
			}
			node.misc^=NODE_INTR;
			putnodedat(&cfg,main_dflt+1,&node,nodefile);
			continue;
		}
		
		if(j <= -2)
			continue;

		if(j==-1) {
			i=0;
			strcpy(opt[0],YesStr);
			strcpy(opt[1],NoStr);
			opt[2][0]=0;
			uifc.helpbuf=	"`Exit Synchronet UNIX Monitor:`\n"
							"\n"
							"\nIf you want to exit the Synchronet UNIX monitor utility,"
							"\nselect `Yes`. Otherwise, select `No` or hit ~ ESC ~.";
			i=uifc.list(WIN_MID,0,0,0,&i,0,"Exit Synchronet Monitor",opt);
			if(!i)
				bail(0);
			continue;
		}
		if(j<cfg.sys_nodes && j>=0) {
			i=0;
			strcpy(opt[i++],"Spy on node");
			strcpy(opt[i++],"Node toggles");
			strcpy(opt[i++],"Clear Errors");
			if(!getnodedat(&cfg,j+1,&node,NULL)) {
				if((node.status==NODE_INUSE) && node.useron) {
					strcpy(opt[i++],"Send message to user");
					strcpy(opt[i++],"Chat with user");
				}
			}
			opt[i][0]=0;
			i=0;
			uifc.helpbuf=	"`Node Options:`\n"
							"\nToDo: Add help";
			switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0,"Node Options",opt))  {
				case 0:	/* Spy */
					dospy(j+1,&bbs_startup);
					break;

				case 1: /* Node Toggles */
					node_toggles(&cfg, j+1);
					break;

				case 2:
					clearerrors(&cfg, j+1,&node);
					break;

				case 3:	/* Send message */
					sendmessage(&cfg, j+1,&node);
					break;

				case 4:
					chat(&cfg,main_dflt+1,&node,&boxch,uifc.timedisplay);
					break;
				
				case -1:
					break;
					
				default:
					uifc.msg("Option not implemented");
					break;
			}
		}
	}
}
