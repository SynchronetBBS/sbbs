/* uedit.c */

/* Synchronet for *nix user editor */

/* $Id$ */

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

#include <signal.h>
#include <stdio.h>
/* #include "curs_fix.h" */
#include "filepick.h"
#include "uifc.h"

/********************/
/* Global Variables */
/********************/
uifcapi_t uifc; /* User Interface (UIFC) Library API */
char YesStr[]="Yes";
char NoStr[]="No";
char *ok[2]={"OK",""};

int main(int argc, char** argv)  {
	char**	opt;
	char**	mopt;
	int		main_dflt=0;
	int		main_bar=0;
	char	revision[16];
	char	str[256],ctrl_dir[41],*p;
	char	title[256];
	int		i,j;
	int		done;
	int		last;
	int		edtuser=0;
	char	longtitle[1024];
	struct file_pick fper;
	/******************/
	/* Ini file stuff */
	/******************/
	char	ini_file[MAX_PATH+1];
	FILE*				fp;

	sscanf("$Revision$", "%*s %s", revision);

    printf("\nSynchronet UIFC Test Suite Copyright "
        "Rob Swindell\n");

    memset(&uifc,0,sizeof(uifc));

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
		if(atoi(argv[i]))
			edtuser=atoi(argv[i]);
    }

#ifdef __unix__
	signal(SIGPIPE, SIG_IGN);
#endif

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

	opt=(char **)malloc(sizeof(char *)*(MAX_OPTS+1));
	for(i=0;i<(MAX_OPTS+1);i++)
		opt[i]=(char *)malloc(MAX_OPLN);

	mopt=(char **)malloc(sizeof(char *)*MAX_OPTS);
	for(i=0;i<MAX_OPTS;i++)
		mopt[i]=(char *)malloc(MAX_OPLN);

	sprintf(title,"Synchronet Test Suite");
	if(uifc.scrn(title)) {
		printf(" USCRN (len=%d) failed!\n",uifc.scrn_len+1);
		return(1);
	}

	strcpy(mopt[0],"Long Title");
	strcpy(mopt[1],"String Input");
	strcpy(mopt[2],"File picker");
	mopt[3][0]=0;

	uifc.helpbuf=	"`Test Suite:`\n"
					"\nToDo: Add Help";

	while(1) {
		j=uifc.list(WIN_L2R|WIN_ESC|WIN_ACT|WIN_DYN|WIN_ORG,0,5,70,&main_dflt,&main_bar
			,title,mopt);

		if(j <= -2)
			continue;

		if(j==-1) {
			uifc.bail();
			return(0);
		}

		if(j==0) {
			/* Long Title */
			strcpy(longtitle,"This is a long title...");
			for(p=strchr(longtitle,0);p<longtitle+sizeof(longtitle)-1;p++)
				*p='.';
			*p=0;
			done=0;
			uifc.list(WIN_ORG|WIN_MID|WIN_ACT,0,0,0,&i,0,longtitle,ok);
		}
		if(j==1) {
			/* String input */
			strcpy(longtitle,"This is a test if string input... enter/edit this field");
			uifc.input(WIN_MID|WIN_NOBRDR,0,0,"Input",longtitle,sizeof(longtitle),K_EDIT);
			uifc.showbuf(WIN_MID, 0, 0, uifc.scrn_width-4, uifc.scrn_len-4, "Result:", longtitle, NULL, NULL);
		}
		if(j==2) {
			/* File picker */
			if(filepick(&uifc, "Bob", &fper, NULL, NULL, UIFC_FP_ALLOWENTRY)!=-1) {
				if(fper.files==1) {
					sprintf(str,"File selected: %-.200s",fper.selected[0]);
					uifc.msg(str);
				}
				filepick_free(&fper);
			}
		}
	}
}
