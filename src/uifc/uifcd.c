/* uifcd.c */

/* Unix libdialog implementation of UIFC library (by Deuce)	*/

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2002 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#include "uifc.h"
#include "dialog.h"

static char app_title[81]="";
static char *helpfile=0;
static uint helpline=0;
static uifcapi_t* api;

/* Prototypes */
static void help();

/* API routines */
static void uifcbail(void);
static int uscrn(char *str);
static int ulist(int mode, int left, int top, int width, int *dflt, int *bar
					,char *title, char **option);
static int uinput(int imode, int left, int top, char *prompt, char *str
					,int len ,int kmode);
static void umsg(char *str);
static void upop(char *str);
static void sethelp(int line, char* file);

/****************************************************************************/
/* Initialization function, see uifc.h for details.                         */
/* Returns 0 on success.                                                    */
/****************************************************************************/
int uifcinid(uifcapi_t* uifcapi)
{

    if(uifcapi==NULL || uifcapi->size!=sizeof(uifcapi_t))
        return(-1);

    api=uifcapi;

    /* install function handlers */
    api->bail=uifcbail;
    api->scrn=uscrn;
    api->msg=umsg;
    api->pop=upop;
    api->list=ulist;
    api->input=uinput;
    api->sethelp=sethelp;
    api->showhelp=help;
	api->showbuf=NULL;
	api->timedisplay=NULL;

    setvbuf(stdout,NULL,_IONBF,0);
    api->scrn_len=24;

    init_dialog();

    return(0);
}

/****************************************************************************/
/* Exit/uninitialize UIFC implementation.                                   */
/****************************************************************************/
void uifcbail(void)
{
    end_dialog();
}

/****************************************************************************/
/* Clear screen, fill with background attribute, display application title. */
/* Returns 0 on success.                                                    */
/****************************************************************************/
int uscrn(char *str)
{
	sprintf(app_title,"%.*s",(int)sizeof(app_title)-1,str);
	/**********************************************************************/
    /* ToDo - Does not display application title... mostly 'cause I clear */
	/* the screen so often                                                */
    /**********************************************************************/
	dialog_clear();
    return(0);
}

/**************************************************************************/
/* General menu display function. see uifc.h for details                  */
/**************************************************************************/
int ulist(int mode, int left, int top, int width, int *cur, int *bar
	, char *title, char **option)
{

    int cnt;
    int cnt2;
	int options;
	int freecnt;
    int i;
	int	scrollpos;
	int height=14;	/* default menu height */
	size_t app_title_len;
    char **it;
    char str[128];
    char tmpstr[MAX_OPLN+1];	 //	Used for restoring the <-- At End --> bit
    char tmpstr2[MAX_OPLN+1];	//  for Add and Paste
    int ret;

	/* Count number of menu options */
    for(cnt=0;cnt<MAX_OPTS;cnt++)
		if(option[cnt][0]==0)
	   		break;
	options=cnt;
	freecnt=cnt+5;	/* Add, Delete, Copy, Paste, At End */

    // Allocate and fill **it
    it=(char **)MALLOC(sizeof(char *)*2*(freecnt));
	if(it==NULL)
		return(-1);

    for(i=0;i<freecnt*2;i++)  {
        it[i]=(char *)MALLOC(MAX_OPLN+1);
		if(it[i]==NULL)
			return(-1);
	}

    strcpy(str,"1");

    cnt2 = 0;
    for(i=0;i<cnt;i++)  {
		sprintf(it[cnt2++],"%u",i+1);
		strcpy(it[cnt2++],option[i]);

		/* Adjust width if it's too small */
		if(width<strlen(option[i])+12)  
			width=strlen(option[i])+12;
    }

	// Make up for expanding first column in width adjustment.
	i=cnt;
	while(i>9)  {
		i /= 10;
		width+=1;
	}

    if(mode&WIN_INS)  {
		strcpy(it[cnt2++],"A");	/* Changed from "I" */
        strcpy(it[cnt2++],"Add New");
		cnt++;
    }
    if(mode&WIN_DEL)  {
		strcpy(it[cnt2++],"D");
        strcpy(it[cnt2++],"Delete");
		cnt++;
    }
    if(mode&WIN_GET)  {
		strcpy(it[cnt2++],"C");
        strcpy(it[cnt2++],"Copy");
		cnt++;
    }
    if(mode&WIN_PUT)  {
		strcpy(it[cnt2++],"P");
        strcpy(it[cnt2++],"Paste");
		cnt++;
    }

    if(width<strlen(title)+4) 
		width=strlen(title)+4;
	app_title_len=strlen(app_title);

    if(*cur<0 || *cur>cnt)*cur=0;

	if(height>cnt)
		height=cnt; /* no reason to display overly "tall" menus */

    while(1) {
        i=*cur;
        if(i<0) i=0;
		if(cnt==2 && strcmp(option[0],"Yes")==0 && strcmp(option[1],"No")==0)  {
		    if(i==0)
				ret=dialog_yesno((char*)NULL,title,5,width);
	    	else
				ret=dialog_noyes((char*)NULL,title,5,width);
			break;
		}
        /* make sure we're wide enough to display the application title */
        if(width<app_title_len+4)
            width=app_title_len+4;
        dialog_clear_norefresh();
        scrollpos=0;

        if(i>=height)				//  Whenever advanced was returned from, it had something
            scrollpos=i-height+1;  //   Beyond the bottom selected
        i=i-scrollpos;			  //	Apparently, *ch is the current position ON THE SCREEN
        ret=dialog_menu(app_title, title, height+8, width, height, cnt, it, str, &i, &scrollpos);

        if(ret==1)  {	/* Cancel */
			ret = -1;
			*cur = -1;
			break;
		}
		if(ret==-1)		/* ESC */
			break;
		if(ret==-2)	{	/* Help */
			help();
			continue;
		}
        if(ret!=0)		/* ??? */
			continue;
		switch(str[0]) {
			case 'A':	/* Add */
				dialog_clear_norefresh();
				if(options>0)  {
					strncpy(tmpstr,it[options*2],MAX_OPLN);
					sprintf(it[options*2],"%u",options+1);
					strncpy(tmpstr2,it[options*2+1],MAX_OPLN);
			        strcpy(it[options*2+1],"<-- At End -->");
	        		ret=dialog_menu(title, "Insert Where?", height+8, width, height, options+1, it, str, 0, 0);
					strncpy(it[options*2],tmpstr,MAX_OPLN);
					strncpy(it[(options*2)+1],tmpstr2,MAX_OPLN);
					if(ret==0)  {
						*cur=atoi(str)-1;
						ret=*cur|MSK_INS;
					}
	       		}
				else {
					*cur=0;
					ret=MSK_INS;
				}
				break;
			case 'D':	/* Delete */
				dialog_clear_norefresh();
				if(options>0)  {
	        		ret=dialog_menu(title, "Delete Which?", height+8, width, height, options, it, str, 0, 0);
					if(ret==0)  {
						*cur=atoi(str)-1;
						ret=*cur|MSK_DEL;
					}
	       		}
				else {
					*cur=0;
					ret=MSK_DEL;
				}
				break;
			case 'C':	/* Copy */
				dialog_clear_norefresh();
				if(options>0)  {
	        		ret=dialog_menu(title, "Copy Which?", height+8, width, height, options, it, str, 0, 0);
					if(ret==0)  {
						*cur=atoi(str)-1;
						ret=*cur|MSK_GET;
					}
	       		}
				else {
					*cur=0;
					ret=MSK_GET;
				}
				break;
			case 'P':	/* Paste */
		       dialog_clear_norefresh();
				if(options>0)  {
					strncpy(tmpstr,it[options*2],MAX_OPLN);
					sprintf(it[options*2],"%u",options+1);
					strncpy(tmpstr2,it[options*2+1],MAX_OPLN);
			        strcpy(it[options*2+1],"<-- At End -->");
	        		ret=dialog_menu(title, "Paste Where?", height+8, width, height, options+1, it, str, 0, 0);
					strncpy(it[options*2],tmpstr,MAX_OPLN);
					strncpy(it[(options*2)+1],tmpstr2,MAX_OPLN);
					if(ret==0)  {
						*cur=atoi(str)-1;
						ret=*cur|MSK_PUT;
					}
	       		}
				else {
					*cur=0;
					ret=MSK_PUT;
				}
				break;
			default:
				*cur=atoi(str)-1;
				ret=*cur;
				break;
		}
		break;
    } 

    // free() the strings!
    for(i=0;i<freecnt*2;i++)
		free(it[i]);
    free(it);
    
    return(ret);
}

/****************************************************************************/
/* Convert ASCIIZ string to upper case										*/
/****************************************************************************/
#if defined(__unix__)
static char* strupr(char* str)
{
	char*	p=str;

	while(*p) {
		*p=toupper(*p);
		p++;
	}
	return(str);
}
#endif

/*************************************************************************/
/* This function is a windowed input string input routine.               */
/*************************************************************************/
int uinput(int mode, int left, int top, char *prompt, char *outstr,
	int max, int kmode)
{
	char str[256];
	if(!(kmode&K_EDIT))
		outstr[0]=0;
	sprintf(str,"%.*s",(int)sizeof(str)-1,outstr);
    while(dialog_inputbox((char*)NULL, prompt, 9, max+4, str)==-2)
		help();
    if(kmode&K_UPPER)	/* convert to uppercase? */
    	strupr(str);
	if(strcmp(str,outstr)) {	/* changed? */
		api->changes=TRUE;
		sprintf(outstr,"%.*s",max,str);
	}
    return strlen(outstr);
}

/****************************************************************************/
/* Displays the message 'str' and waits for the user to select "OK"         */
/****************************************************************************/
void umsg(char *str)
{
    dialog_mesgbox((char*)NULL, str, 7, strlen(str)+4);
}

/****************************************************************************/
/* Status popup/down function, see uifc.h for details.                      */
/****************************************************************************/
void upop(char *str)
{
	// Pop-down doesn't do much... the mext item should over-write this.
	if(str!=NULL)
    	dialog_gauge((char*)NULL,str,8,20,7,40,0);
}

/****************************************************************************/
/* Sets the current help index by source code file and line number.         */
/****************************************************************************/
void sethelp(int line, char* file)
{
    helpfile=file;
    helpline=line;
}

/************************************************************/
/* Help function. 											*/
/************************************************************/
void help()
{
	char hbuf[HELPBUF_SIZE],str[256];
    char *p;
	unsigned short line;
	long l;
	FILE *fp;

    printf("\n");
    if(!api->helpbuf) {
        if((fp=fopen(api->helpixbfile,"rb"))==NULL)
            sprintf(hbuf,"ERROR: Cannot open help index: %s"
                ,api->helpixbfile);
        else {
            p=strrchr(helpfile,'/');
            if(p==NULL)
                p=strrchr(helpfile,'\\');
            if(p==NULL)
                p=helpfile;
            else
                p++;
            l=-1L;
            while(!feof(fp)) {
                if(!fread(str,12,1,fp))
                    break;
                str[12]=0;
                fread(&line,2,1,fp);
                if(stricmp(str,p) || line!=helpline) {
                    fseek(fp,4,SEEK_CUR);
                    continue; }
                fread(&l,4,1,fp);
                break; }
            fclose(fp);
            if(l==-1L)
                sprintf(hbuf,"ERROR: Cannot locate help key (%s:%u) in: %s"
                    ,p,helpline,api->helpixbfile);
            else {
                if((fp=fopen(api->helpdatfile,"rb"))==NULL)
                    sprintf(hbuf,"ERROR: Cannot open help file: %s"
                        ,api->helpdatfile);
                else {
                    fseek(fp,l,SEEK_SET);
                    fread(hbuf,HELPBUF_SIZE,1,fp);
                    fclose(fp); 
				} 
			} 
		} 
	}
    else
        strcpy(hbuf,api->helpbuf);

	dialog_mesgbox((char*)NULL, hbuf, 24, 80);
}
