/* uifcx.c */

/* Standard I/O Implementation of UIFC (user interface) library */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2003 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
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

#include <sys/types.h>

#ifdef __unix__
/*	#include <sys/time.h> why? */
	#include <unistd.h>
#endif

static char *helpfile=0;
static uint helpline=0;
static uifcapi_t* api;

/* Prototypes */
static void help(void);

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
/* Initialization function, see uifc.h for details.							*/
/* Returns 0 on success.													*/
/****************************************************************************/
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
    api->showhelp=help;
	api->showbuf=NULL;
	api->timedisplay=NULL;

    setvbuf(stdin,NULL,_IONBF,0);
    setvbuf(stdout,NULL,_IONBF,0);
    api->scrn_len=24;

    return(0);
}

/****************************************************************************/
/* Exit/uninitialize UIFC implementation.									*/
/****************************************************************************/
void uifcbail(void)
{
}

/****************************************************************************/
/* Clear screen, fill with background attribute, display application title.	*/
/* Returns 0 on success.													*/
/****************************************************************************/
int uscrn(char *str)
{
    return(0);
}

static int getstr(char* str, int maxlen)
{
	char 	ch;
    int		len=0;
#ifdef __unix__
	int		istty;

	istty=isatty(fileno(stdin));
#endif
    while(1) {
		fread(&ch,1,1,stdin);
#ifdef __unix__
		if(!istty)  {
			printf("%c",ch);
			fflush(stdout);
		}
#endif
        if(ch=='\r' || ch=='\n')	/* enter */
        	break;
        if(ch=='\b' || ch==DEL) {	/* backspace */
        	if(len) len--;
            continue;
    	}
        if(len<maxlen)
			str[len++]=ch;
	}
    str[len]=0;	/* we need The Terminator */
    
	return(len);
}
	

/****************************************************************************/
/* Local utility function.													*/
/****************************************************************************/
static int which(char* prompt, int max)
{
    char    str[41];
    int     i;

    while(1) {
        printf("%s which (1-%d): ",prompt,max);
        str[0]=0;
		getstr(str,sizeof(str)-1);
        i=atoi(str);
        if(i>0 && i<=max)
            return(i-1);
    }
}

/****************************************************************************/
/* Truncates white-space chars off end of 'str'								*/
/****************************************************************************/
static void truncsp(char *str)
{
	uint c;

	c=strlen(str);
	while(c && (uchar)str[c-1]<=SP) c--;
	str[c]=0;
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

/****************************************************************************/
/* General menu function, see uifc.h for details.							*/
/****************************************************************************/
int ulist(int mode, int left, int top, int width, int *cur, int *bar
	, char *title, char **option)
{
    char str[128];
	int i,opts;
    int optnumlen;
    int yesno=0;
    int lines;

    for(opts=0;opts<MAX_OPTS;opts++)
    	if(option[opts][0]==0)
    		break;

    if((*cur)>=opts)
        (*cur)=opts-1;			/* returned after scrolled */

    if((*cur)<0)
        (*cur)=0;

    if(opts>999)
        optnumlen=4;
    else if(opts>99)
        optnumlen=3;
    else if(opts>9)
        optnumlen=2;
    else
        optnumlen=1;
    while(1) {
        if(opts==2 && !stricmp(option[0],"Yes") && !stricmp(option[1],"No")) {
            yesno=1;
            printf("%s? ",title);
        } else {
            printf("\n[%s]\n",title);
            lines=2;
            for(i=0;i<opts;i++) {
                printf("%*d: %s\n",optnumlen,i+1,option[i]);
                lines++;
                if(!(lines%api->scrn_len)) {
                    printf("More? ");
                    str[0]=0;
                    getstr(str,sizeof(str)-1);
                    if(toupper(*str)=='N')
                        break;
                }
            }
            str[0]=0;
            if(mode&WIN_GET)
                strcat(str,", Copy");
            if(mode&WIN_PUT)
                strcat(str,", Paste");
            if(mode&WIN_INS)
                strcat(str,", Add");
            if(mode&WIN_DEL)
                strcat(str,", Delete");
            printf("\nWhich (Help%s or Quit): ",str);
        }
        str[0]=0;
        getstr(str,sizeof(str)-1);
        
        truncsp(str);
        i=atoi(str);
        if(i>0 && i<=opts) {
            *cur=--i;
            return(*cur);
        }
        i=atoi(str+1);
        switch(toupper(*str)) {
            case 0:
            case ESC:
            case 'Q':
                printf("Quit\n");
                return(-1);
            case 'Y':
                if(!yesno)
                    break;
                printf("Yes\n");
                return(0);
            case 'N':
                if(!yesno)
                    break;
                printf("No\n");
                return(1);
            case 'H':
            case '?':
                printf("Help\n");
                help();
                break;
            case 'A':   /* Add/Insert */
				if(!opts)
    				return(MSK_INS);
                if(i>0 && i<=opts+1)
        			return((i-1)|MSK_INS);
                return(which("Add before",opts+1)|MSK_INS);
            case 'D':   /* Delete */
				if(!opts)
    				break;
                if(i>0 && i<=opts)
        			return((i-1)|MSK_DEL);
                if(opts==1)
                    return(MSK_DEL);
                return(which("Delete",opts)|MSK_DEL);
            case 'C':   /* Copy/Get */
				if(!opts)
    				break;
                if(i>0 && i<=opts)
        			return((i-1)|MSK_GET);
                if(opts==1)
                    return(MSK_GET);
                return(which("Copy",opts)|MSK_GET);
            case 'P':   /* Paste/Put */
				if(!opts)
    				break;
                if(i>0 && i<=opts)
        			return((i-1)|MSK_PUT);
                if(opts==1)
                    return(MSK_PUT);
                return(which("Paste",opts)|MSK_PUT);
        }
    }
}


/*************************************************************************/
/* This function is a windowed input string input routine.               */
/*************************************************************************/
int uinput(int mode, int left, int top, char *prompt, char *outstr,
	int max, int kmode)
{
    char str[256];
    
    while(1) {
        printf("%s (maxlen=%u): ",prompt,max);

        getstr(str,max);
        truncsp(str);
        if(strcmp(str,"?"))
            break;
        help();
    }
	if(strcmp(outstr,str))
		api->changes=1;
    if(kmode&K_UPPER)	/* convert to uppercase? */
    	strupr(str);
    strcpy(outstr,str);    
    return(strlen(outstr));
}

/****************************************************************************/
/* Displays the message 'str' and waits for the user to select "OK"         */
/****************************************************************************/
void umsg(char *str)
{
    printf("%s\n",str);
}

/****************************************************************************/
/* Status popup/down function, see uifc.h for details.						*/
/****************************************************************************/
void upop(char *str)
{
    if(str==NULL)
        printf("\n");
    else
        printf("\r%-79s",str);
}

/****************************************************************************/
/* Sets the current help index by source code file and line number.			*/
/****************************************************************************/
void sethelp(int line, char* file)
{
    helpline=line;
    helpfile=file;
}

/****************************************************************************/
/* Help function.															*/
/****************************************************************************/
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

    puts(hbuf);
    if(strlen(hbuf)>200) {
        printf("Hit enter");
        getstr(str,sizeof(str)-1);
    }
}


