/* uifcx.c */

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

#include <dialog.h>

#ifndef __unix__
#include <share.h>
#endif

static char *helpfile=0;
static uint helpline=0;
static uifcapi_t* api;

/* Prototypes */
static void help();

/* API routines */
static void uifcbail(void);
static int uscrn(char *str);
static int ulist(int mode, char left, int top, char width, int *dflt, int *bar
	,char *title, char **option);
static int uinput(int imode, char left, char top, char *prompt, char *str
	,char len ,int kmode);
static void umsg(char *str);
static void upop(char *str);
static void sethelp(int line, char* file);

int uifcinix(uifcapi_t* uifcapi)
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

    setvbuf(stdout,NULL,_IONBF,0);
    api->scrn_len=24;

    init_dialog();

    return(0);
}


void uifcbail(void)
{
    end_dialog();
}

int uscrn(char *str)
{
    // ToDo
    return(0);
}

/**************************************************************************/
/* General menu display function. SCRN_* macros define virtual screen     */
/* limits. *cur is a pointer to the current option. Returns option number */
/* positive value for selected option or -1 for ESC, -2 for INS or -3 for */
/* DEL. Menus can centered left to right and top to bottom automatically. */
/* mode bits are set with macros WIN_*									  */
/* option is an array of char arrays, first element of last char array    */
/* must be NULL.                                                          */
/**************************************************************************/
int ulist(int mode, char left, int top, char width, int *cur, int *bar
	, char *title, char **option)
{

    // Ignoring the mode bits...
    int cnt;
    int cnt2;
    int freecnt;
    int i;
    char **it;
    char *str;
    char key=49;
    int ret;

    for(cnt=0;cnt<MAX_OPTS;cnt++)
	if(option[cnt][0]==0)
	    break;
    freecnt=cnt+4;

    // Allocate and fill **it
    it=(char **)MALLOC(sizeof(char *)*2*(cnt+4));

    for(i=0;i<(cnt+4)*2;i++)
        it[i]=(char *)MALLOC(64);

    str=(char *)MALLOC(64);
    strcpy(str,"1");

    cnt2 = 0;
    for(i=0;i<cnt;i++)  {
	str[0]=key;
	strcpy(it[cnt2++],str);
	key++;
	if(key==58) key=65;
	strcpy(it[cnt2++],option[i]);
	if(width<strlen(option[i]+12)) width=strlen(option[i])+12;
    }
    if(mode&WIN_INS)  {
	key=33;
	str[0]=key;
	strcpy(it[cnt2++],str);
        strcpy(it[cnt2++],"Add New");
	cnt++;
    }
    if(mode&WIN_DEL)  {
	key=64;
	str[0]=key;
	strcpy(it[cnt2++],str);
        strcpy(it[cnt2++],"Delete");
	cnt++;
    }
    if(width<strlen(title)+4) width=strlen(title)+4;

    do {
        dialog_clear_norefresh();
        i=*cur;
        if(i<0) i=0;
        ret=dialog_menu(title, "SCFG Menu", 22, width, 15, cnt, it, str, &i, 0);

        if(ret==1) ret = -1;
        if(ret==0)  {
	    ret = str[0];
	    ret -= 49;
	    if(ret==-16) ret = -2;
	    if(ret==15) ret = -3;
	    if(ret>10) ret -= 7;
        }
        if(ret==-2)  {
            dialog_clear_norefresh();
	    if(freecnt-4>0)  {
	        ret=dialog_menu(title, "Insert Where?", 22, width, 15, freecnt-4, it, str, 0, 0);
	        if(ret==1) ret=-2;
	        if(ret==-1) ret=-2;
	        if(ret==0)  {
		    ret=str[0];
		    ret -= 49;
		    if(ret>10) ret -= 7;
	        }
	        ret |= MSK_INS;
	    }
	    else ret=MSK_INS;
        }
        if(ret==-3)  {
            dialog_clear_norefresh();
	    if(freecnt-4>0)  {
	        ret=dialog_menu(title, "Delete Which?", 22, width, 15, freecnt-4, it, str, 0, 0);
	        if(ret==1) ret=-3;
	        if(ret==-1) ret=-3;
	        if(ret==0)  {
		    ret=str[0];
		    ret -= 49;
		    if(ret>10) ret -= 7;
	        }
	        ret |= MSK_DEL;
	    }
	    else ret=MSK_DEL;
        }
    } while(ret<-1);

    // free() the strings!

    for(i=0;i<(freecnt)*2;i++)
	free(it[i]);
    free(str);
    free(it);
    
    return(ret);
}


/*************************************************************************/
/* This function is a windowed input string input routine.               */
/*************************************************************************/
int uinput(int mode, char left, char top, char *prompt, char *outstr,
	char max, int kmode)
{
    dialog_inputbox(prompt, "", 9, max+4, outstr);
}

/****************************************************************************/
/* Displays the message 'str' and waits for the user to select "OK"         */
/****************************************************************************/
void umsg(char *str)
{
    dialog_mesgbox("Message:", str, 7, strlen(str)+4);
}

/****************************************************************************/
/* Performs printf() through puttext() routine								*/
/****************************************************************************/
int uprintf(char x, char y, char attr, char *fmat, ...)
{
    int     i;
	va_list argptr;

    va_start(argptr,fmat);
    i=vprintf(fmat,argptr);
    va_end(argptr);

    return(i);
}

void upop(char *str)
{
    dialog_gauge("",str,8,20,7,40,0);
}

void sethelp(int line, char* file)
{
    helpline=line;
    helpfile=file;
}

/************************************************************/
/* Help (F1) key function. Uses helpbuf as the help input.	*/
/************************************************************/
void help()
{
	char hbuf[HELPBUF_SIZE],str[256];
    char *p;
	uint i,j,k,len;
	long l;
	FILE *fp;

    printf("\n");
    if(!api->helpbuf) {
#ifdef __unix__
        if((fp=fopen(api->helpixbfile,"rb"))==NULL)
#else
        if((fp=_fsopen(api->helpixbfile,"rb",SH_DENYWR))==NULL)
#endif
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
                fread(&k,2,1,fp);
                if(strcasecmp(str,p) || k!=helpline) {
                    fseek(fp,4,SEEK_CUR);
                    continue; }
                fread(&l,4,1,fp);
                break; }
            fclose(fp);
            if(l==-1L)
                sprintf(hbuf,"ERROR: Cannot locate help key (%s:%u) in: %s"
                    ,p,helpline,api->helpixbfile);
            else {
#ifdef __unix__
                if((fp=fopen(api->helpdatfile,"rb"))==NULL)
#else
                if((fp=_fsopen(api->helpdatfile,"rb",SH_DENYWR))==NULL)
#endif
                    sprintf(hbuf,"ERROR: Cannot open help file: %s"
                        ,api->helpdatfile);
                else {
                    fseek(fp,l,SEEK_SET);
                    fread(hbuf,HELPBUF_SIZE,1,fp);
                    fclose(fp); } } } }
    else
        strcpy(hbuf,api->helpbuf);

    puts(hbuf);
    if(strlen(hbuf)>200) {
        printf("Hit enter");
        getc(stdin);
    }
}


