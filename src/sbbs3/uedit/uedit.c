/* umonitor.c */

/* Synchronet for *nix user editor */

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
#include <sys/types.h>
#include <time.h>
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

#define CTRL(x) (x&037)

/********************/
/* Global Variables */
/********************/
uifcapi_t uifc; /* User Interface (UIFC) Library API */
const char *YesStr="Yes";
const char *NoStr="No";

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

void freeopt(char** opt)
{
	int i;

	for(i=0;i<(MAX_OPTS+1);i++)
		free(opt[i]);

	free(opt);
}

int edit_user(scfg_t *cfg, int usernum)
{
	return(0);
}

int main(int argc, char** argv)  {
	char**	opt;
	char**	mopt;
	int		main_dflt=0;
	int		main_bar=0;
	char	revision[16];
	char	str[256],ctrl_dir[41],*p;
	char	title[256];
	int		i,j;
	scfg_t	cfg;
	int		done;
	int		last;
	user_t	user;
	/******************/
	/* Ini file stuff */
	/******************/
	char	ini_file[MAX_PATH+1];
	FILE*				fp;
	bbs_startup_t		bbs_startup;

	sscanf("$Revision$", "%*s %s", revision);

    printf("\nSynchronet User Editor %s-%s  Copyright 2003 "
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

	gethostname(str,sizeof(str)-1);

	sbbs_get_ini_fname(ini_file, ctrl_dir, str);

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
    memset(&cfg,0,sizeof(cfg));
	cfg.size=sizeof(cfg);
	SAFECOPY(cfg.ctrl_dir,bbs_startup.ctrl_dir);
	if(!load_cfg(&cfg, NULL, TRUE, str)) {
		printf("ERROR! %s\n",str);
		exit(1);
	}
	prep_dir(cfg.data_dir, cfg.temp_dir, sizeof(cfg.temp_dir));

    memset(&uifc,0,sizeof(uifc));

	uifc.esc_delay=500;

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
#ifdef USE_CURSES
	i=uifcinic(&uifc);  /* curses */
#else
	i=uifcini32(&uifc);  /* curses */
#endif
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

	sprintf(title,"Synchronet User Editor %s-%s",revision,PLATFORM_DESC);
	if(uifc.scrn(title)) {
		printf(" USCRN (len=%d) failed!\n",uifc.scrn_len+1);
		bail(1);
	}

	strcpy(mopt[0],"New User");
	strcpy(mopt[1],"Find User");
	strcpy(mopt[2],"User List");
	mopt[3][0]=0;

	uifc.helpbuf=	"`User Editor:`\n"
					"\nToDo: Add Help";

	while(1) {
		j=uifc.list(WIN_L2R|WIN_ESC|WIN_ACT|WIN_DYN,0,5,70,&main_dflt,&main_bar
			,title,mopt);

		if(j == -2)
			continue;

		if(j==-8) {	/* CTRL-F */
			/* Find User */
			continue;
		}
		
		if(j==-2-KEY_F(4)) {	/* Find? */
			continue;
		}

		if(j <= -2)
			continue;

		if(j==-1) {
			i=0;
			strcpy(opt[0],YesStr);
			strcpy(opt[1],NoStr);
			opt[2][0]=0;
			uifc.helpbuf=	"`Exit Synchronet User Editor:`\n"
							"\n"
							"\nIf you want to exit the Synchronet user editor,"
							"\nselect `Yes`. Otherwise, select `No` or hit ~ ESC ~.";
			i=uifc.list(WIN_MID,0,0,0,&i,0,"Exit Synchronet Monitor",opt);
			if(!i)
				bail(0);
			continue;
		}

		if(j==0) {
			/* New User */
		}
		if(j==1) {
			/* Find User */
		}
		if(j==2) {
			/* User List */
			done=0;
			while(!done) {
				last=lastuser(&cfg);
				for(i=1; i<=last; i++) {
					user.number=i;
					getuserdat(&cfg,&user);
					sprintf(opt[i-1],"%s (%s)",user.name,user.alias);
				}
				opt[i-1][0]=0;
				i=0;
				switch(uifc.list(WIN_MID,0,0,0,&i,0,"Select User",opt)) {
					case -1:
						done=1;
						break;
					default:
						edit_user(&cfg, i+1);
						break;
				}
			}
		}
	}
}
