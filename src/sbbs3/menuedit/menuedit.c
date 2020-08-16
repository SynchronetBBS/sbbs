/* menuedit.c */

/* Synchronet Menu Editor		 										*/

/* $Id: menuedit.c,v 1.6 2018/07/24 01:12:16 rswindell Exp $ */

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

#include <stdlib.h>
#include "genwrap.h"
#include "ciolib.h"
#include "uifc.h"
#include "dirwrap.h"
#include "ini_file.h"

uifcapi_t uifc; /* User Interface (UIFC) Library API */

static int yesno(char *prompt, BOOL dflt)
{
	char*	opt[]={"Yes","No",NULL};
	int		i=!dflt;	/* reverse logic */

	i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0,prompt,opt);
	if(i>=0) {
		i=!i;	/* reverse logic */
		if(i!=dflt)
			uifc.changes=TRUE;
	}
	return(i);
}

static const char* boolstr(BOOL val)
{
	return(val?"Yes":"No");
}

static void edit_menu(char* path)
{
	char	str[MAX_PATH*2];
	FILE*	fp;
	int		i;
	static	dflt;
	char	title[MAX_PATH+1];
	char	value[INI_MAX_VALUE_LEN];
	char*	p;
	char*	opt[MAX_OPTS+1];
	char	exec[INI_MAX_VALUE_LEN];
	char	prompt[INI_MAX_VALUE_LEN];
	char	menu_file[MAX_PATH+1];
	char	menu_format[INI_MAX_VALUE_LEN];
	size_t	menu_column_width;
	BOOL	menu_reverse;
	BOOL	hotkeys;
	BOOL	expert;
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
	SAFECOPY(prompt,iniReadString(fp,ROOT_SECTION, "prompt", "'Command: '", value));
	SAFECOPY(exec,iniReadString(fp,ROOT_SECTION, "exec", "", value));
	SAFECOPY(menu_file,iniReadString(fp,ROOT_SECTION, "menu_file", "", value));
	SAFECOPY(menu_format,iniReadString(fp,ROOT_SECTION, "menu_format", "\1n\1h\1w%s \1b%s", value));
	menu_column_width=iniReadInteger(fp,ROOT_SECTION,"menu_column_width", 39);
	menu_reverse=iniReadBool(fp,ROOT_SECTION,"menu_reverse", FALSE);

	hotkeys=iniReadBool(fp,ROOT_SECTION,"hotkeys",TRUE);
	expert=iniReadBool(fp,ROOT_SECTION,"expert",TRUE);

	fclose(fp);


	for(i=0;i<MAX_OPTS+1;i++)
		opt[i]=(char *)alloca(MAX_OPLN+1);

	SAFECOPY(str,getfname(path));
	if((p=getfext(str))!=NULL)
		*p=0;
	SAFEPRINTF(title, "Menu: %s", str);
	uifc.changes=0;
	while(1) {

		/* Create option list */
		i=0;
		safe_snprintf(opt[i++],MAX_OPLN,opt_fmt,"Menu File", menu_file[0] ? menu_file : "<Dynamic>");
		safe_snprintf(opt[i++],MAX_OPLN,opt_fmt,"Execute (JS Expression)",exec[0] ? exec : "<nothing>");
		safe_snprintf(opt[i++],MAX_OPLN,opt_fmt,"Prompt (JS String)",prompt[0] ? prompt : "<none>");
		safe_snprintf(opt[i++],MAX_OPLN,opt_fmt,"Hot Key Input",boolstr(hotkeys));
		safe_snprintf(opt[i++],MAX_OPLN,opt_fmt,"Display Menu"
			,expert ? "Based on User \"Expert\" Setting"
			: "Always");
		safe_snprintf(opt[i++],MAX_OPLN,"Commands...");

		opt[i][0]=0;

		i=uifc.list(WIN_ACT|WIN_CHE|WIN_RHT|WIN_BOT,0,0,10,&dflt,0
			,title,opt);
		if(i==-1) {
			if(uifc.changes) {
				i=yesno("Save Changes",TRUE);
				if(i==-1)
					continue;
				if(i==FALSE)
					uifc.changes=0;
			}
			break;
		}
		switch(i) {
			case 0:
				i=yesno("Use Static Menu File", menu_file[0]);
				if(i==-1)
					break;
				if(i==TRUE)
					uifc.input(WIN_MID|WIN_SAV,0,0,"Base Filename"
						,menu_file,sizeof(menu_file)-1,K_EDIT);
				else {
					menu_file[0]=0;
				}
				break;
			case 1:
				uifc.input(WIN_MID|WIN_SAV,0,0,"Execute (JS Expression)",exec,sizeof(exec)-1,K_EDIT);
				break;
			case 2:
				uifc.input(WIN_MID|WIN_SAV,0,0,"Prompt (JS String)",prompt,sizeof(prompt)-1,K_EDIT);
				break;
			case 3:
				i=yesno("Use Hot Key Input (ENTER key not used)",hotkeys);
				if(i>=0) hotkeys=i;
				break;
			case 4:
				i=yesno("Display Menu Always (Ignore \"Expert\" User Setting)",!expert);
				if(i>=0) expert=!i;
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
			iniSetString(&list,ROOT_SECTION,"menu_file",menu_file,&style);
			iniSetString(&list,ROOT_SECTION,"menu_format",menu_format,&style);
			iniSetInteger(&list,ROOT_SECTION,"menu_column_width",menu_column_width,&style);
			iniSetBool(&list,ROOT_SECTION,"menu_reverse",menu_reverse,&style);

			iniSetBool(&list,ROOT_SECTION,"hotkeys",hotkeys,&style);
			iniSetBool(&list,ROOT_SECTION,"expert",expert,&style);

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

	sscanf("$Revision: 1.6 $", "%*s %s", revision);

    printf("\r\nSynchronet Menu Editor (%s)  %s  Copyright "
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
	if(!door_mode)
		i=uifcini32(&uifc);  /* curses/conio */
	else
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
