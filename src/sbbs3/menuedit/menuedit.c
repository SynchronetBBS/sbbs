/* menuedit.c */

/* Synchronet Menu Editor		 										*/

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2004 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#include <stdlib.h>
#include <malloc.h>		/* alloca */
#include "uifc.h"
#include "genwrap.h"
#include "dirwrap.h"
#include "ini_file.h"

uifcapi_t uifc; /* User Interface (UIFC) Library API */

static int yesno(char *prompt, int dflt)
{
	char*	opt[]={"Yes","No",NULL};

	return(uifc.list(WIN_MID|WIN_SAV,0,0,0,&dflt,0,prompt,opt));
}

static void edit_menu(char* path)
{
	char	str[MAX_PATH*2];
	FILE*	fp;
	int		i;
	static	dflt;
	char	value[INI_MAX_VALUE_LEN];
	char*	opt[MAX_OPTS+1];
	char	prompt[256];
	const char* opt_fmt = "%-25.25s %s";
	str_list_t	list;
	ini_style_t style;

	/* Open menu file */
	if((fp=fopen(path,"r"))==NULL) {
		sprintf(str,"ERROR %u opening %s",errno,path);
		uifc.msg(str);
		return;
	}

	/* Read menu file */
	SAFECOPY(prompt,iniGetString(fp, ROOT_SECTION, "prompt", "'Command: '", value));
	fclose(fp);


	for(i=0;i<MAX_OPTS+1;i++)
		opt[i]=(char *)alloca(MAX_OPLN+1);

	uifc.changes=0;
	while(1) {

		/* Create option list */
		i=0;
		safe_snprintf(opt[i++],MAX_OPLN,opt_fmt,"Prompt",prompt);
		opt[i][0]=0;

		i=uifc.list(WIN_ACT|WIN_CHE|WIN_RHT|WIN_BOT,0,0,10,&dflt,0
			,getfname(path),opt);
		if(i==-1) {
			if(uifc.changes) {
				i=yesno("Save Changes",TRUE);
				if(i==-1)
					continue;
				if(i==1)
					uifc.changes=0;
			}
			break;
		}
		switch(i) {
			case 0:
				uifc.input(WIN_MID|WIN_SAV,0,0,"Prompt",prompt,sizeof(prompt)-1,K_EDIT);
				break;
		}
	}

	if(uifc.changes) {	/* Saving changes? */

		/* Open menu file */
		if((fp=fopen(path,"r+"))==NULL) {
			sprintf(str,"ERROR %u opening %s",errno,path);
			uifc.msg(str);
			return;
		}

		/* Set .ini style */
		memset(&style, 0, sizeof(style));
		style.value_separator=": ";

		/* Read menu file */
		if((list=iniReadFile(fp))!=NULL) {

			/* Update options */
			iniSetString(&list,ROOT_SECTION,"prompt",prompt,&style);

			/* Write menu file */
			iniWriteFile(fp,list);
			strListFree(&list);
		}

		fclose(fp);
	}
}

int main(int argc, char **argv)
{
	char*	p;
	char	str[128];
	char	path[MAX_PATH+1];
	char	exec_dir[MAX_PATH/2];
	char	revision[16];
	char*	opt[MAX_OPTS];
	int		i;
	int		dflt=0;
	BOOL	door_mode=FALSE;
	glob_t	g;
	size_t	gi;

	sscanf("$Revision$", "%*s %s", revision);

    printf("\r\nSynchronet Menu Editor (%s)  %s  Copyright 2004 "
        "Rob Swindell\r\n",PLATFORM_DESC,revision);

	exec_dir[0]=0;
    if((p=getenv("SBBSEXEC"))!=NULL)
		SAFECOPY(exec_dir,p);

	for(i=1;i<argc;i++) {
		if(argv[i][0] != '-')
			SAFECOPY(exec_dir,argv[i]);
	}
	if(exec_dir[0]==0) {
		printf("!ERROR: SBBSEXEC environment variable must be set or specified on command-line.\n");
		return(1);
	}

	backslash(exec_dir);

	uifc.esc_delay=25;

	uifc.size=sizeof(uifc);
#if defined(USE_UIFC32)
	if(!door_mode)
		i=uifcini32(&uifc);  /* curses/conio */
	else
#endif
		i=uifcinix(&uifc);  /* stdio */
	if(i!=0) {
		printf("!ERROR: UIFC library init returned: %d\n",i);
		exit(1);
	}

	atexit(uifc.bail);

	sprintf(str,"Synchronet Menu Editor %s",revision);
	if(uifc.scrn(str)) {
		printf(" USCRN (len=%d) failed!\r\n",uifc.scrn_len+1);
		return(2);
	}

	while(1) {
		sprintf(path,"%s*.mnu",exec_dir);
		glob(path,0,NULL,&g);
		for(gi=0;gi<g.gl_pathc && gi<MAX_OPTS;gi++)
			opt[gi]=getfname(g.gl_pathv[gi]);
		opt[gi]=NULL;
		uifc.helpbuf="Sorry, no help available yet";
		i=uifc.list(WIN_ORG|WIN_ACT|WIN_INS|WIN_DEL,0,0,10,&dflt,0
			,"Edit Menu",opt);
		if(i==-1)
			break;
		if(i>=0 && i<(int)g.gl_pathc)
			edit_menu(g.gl_pathv[i]);
		globfree(&g);
	}

	return(0);
}
