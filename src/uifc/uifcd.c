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
int ulist(int mode, char left, int top, char width, int *cur, int *bar
	, char *title, char **option)
{

    int cnt;
    int cnt2;
    int freecnt;
    int i;
	int	scrollpos;
    char **it;
    char str[128];
    int ret;
	int adj_width=0;

	/* Count number of menu options */
    for(cnt=0;cnt<MAX_OPTS;cnt++)
		if(option[cnt][0]==0)
	   		break;
    freecnt=cnt+4;

    // Allocate and fill **it
    it=(char **)MALLOC(sizeof(char *)*2*(cnt+4));
	if(it==NULL)
		return(-1);

    for(i=0;i<(cnt+4)*2;i++)  {
        it[i]=(char *)MALLOC(MAX_OPLN+1);
		if(it[i]==NULL)
			return(-1);
	}

    strcpy(str,"1");

    cnt2 = 0;
    for(i=0;i<cnt;i++)  {
		sprintf(it[cnt2++],"%d",i+1);
		strcpy(it[cnt2++],option[i]);

		/* Adjust width if it's too small */
		if(width<strlen(option[i])+12)  {
			width=strlen(option[i])+12;
			adj_width=TRUE;
		}
    }

	// Make up for expanding first column in width adjustment.
	i=cnt;
	while(i>9)  {
		i /= 10;
		width+=1;
	}

    if(mode&WIN_INS)  {
		strcpy(it[cnt2++],"I");
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
    if(width<strlen(title)+4) width=strlen(title)+4;

    if(*cur<0 || *cur>cnt)*cur=0;

    do {
        i=*cur;
        if(i<0) i=0;
		if(strcmp(option[0],"Yes")==0 && strcmp(option[1],"No")==0 && cnt==2)  {
		    if(i==0)
				ret=dialog_yesno((char *)NULL,title,5,width);
	    	else
				ret=dialog_noyes((char *)NULL,title,5,width);
	
	    	if(ret) ret=1; else ret=0;
		}
		else if(strcmp(option[0],"No")==0 && strcmp(option[1],"Yes")==0 && cnt==2)  {
	    	if(i==1)
				ret=dialog_yesno((char *)NULL,title,5,width);
	    	else
				ret=dialog_noyes((char *)NULL,title,5,width);
	    	if(ret) ret=0; else ret=1;
		}
		else  {
            dialog_clear_norefresh();
			scrollpos=0;
			if(i>14)
				scrollpos=i-14;
            ret=dialog_menu((char *)NULL, title, 22, width, 14, cnt, it, str, &i, &scrollpos);
            if(ret==1)  {
				ret = -1;
				*cur = -1;
			}
            if(ret==0)  {
				if(str[0]=='I') ret=-2;
				else if(str[0]=='D') ret=-3;
				else if(str[0]=='C') ret=-4;
				else if(str[0]=='P') ret=-5;
				else  {
					*cur=atoi(str)-1;
					ret=*cur;
				}
            }
		}
        if(ret==-2)  {
            dialog_clear_norefresh();
			if(freecnt-4>0)  {
	        	ret=dialog_menu(title, "Insert Where?", 22, width, 14, freecnt-4, it, str, 0, 0);
				if(ret==0)  {
					*cur=atoi(str)-1;
					ret=*cur|MSK_INS;
				}
	       	}
			else {
				*cur=0;
				ret=MSK_INS;
			}
	    }
        if(ret==-3)  {
            dialog_clear_norefresh();
			if(freecnt-4>0)  {
	        	ret=dialog_menu(title, "Delete Which?", 22, width, 14, freecnt-4, it, str, 0, 0);
				if(ret==0)  {
					*cur=atoi(str)-1;
					ret=*cur|MSK_DEL;
				}
	       	}
			else {
				*cur=0;
				ret=MSK_DEL;
			}
        }
        if(ret==-4)  {
            dialog_clear_norefresh();
			if(freecnt-4>0)  {
	        	ret=dialog_menu(title, "Copy Which?", 22, width, 14, freecnt-4, it, str, 0, 0);
				if(ret==0)  {
					*cur=atoi(str)-1;
					ret=*cur|MSK_GET;
				}
	       	}
			else {
				*cur=0;
				ret=MSK_GET;
			}
        }
        if(ret==-5)  {
            dialog_clear_norefresh();
			if(freecnt-4>0)  {
	        	ret=dialog_menu(title, "Paste Where?", 22, width, 14, freecnt-4, it, str, 0, 0);
				if(ret==0)  {
					*cur=atoi(str)-1;
					ret=*cur|MSK_PUT;
				}
	       	}
			else {
				*cur=0;
				ret=MSK_PUT;
			}
        }
    } while(ret<-1 && ret>-8);

    // free() the strings!
    for(i=0;i<(freecnt)*2;i++)
	free(it[i]);
    free(it);
    
    return(ret);
}


/*************************************************************************/
/* This function is a windowed input string input routine.               */
/*************************************************************************/
int uinput(int mode, char left, char top, char *prompt, char *outstr,
	char max, int kmode)
{
    dialog_inputbox((char *)NULL, prompt, 9, max+4, outstr);
    return strlen(outstr);
}

/****************************************************************************/
/* Displays the message 'str' and waits for the user to select "OK"         */
/****************************************************************************/
void umsg(char *str)
{
    dialog_mesgbox((char *)NULL, str, 7, strlen(str)+4);
}

/****************************************************************************/
/* Status popup/down function, see uifc.h for details.                      */
/****************************************************************************/
void upop(char *str)
{
	// Pop-down doesn't do much... the mext item should over-write this.
    dialog_gauge((char *)NULL,str,8,20,7,40,0);
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
	uint i,j,k,len;
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
                fread(&k,2,1,fp);
                if(stricmp(str,p) || k!=helpline) {
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
                    fclose(fp); } } } }
    else
        strcpy(hbuf,api->helpbuf);

    puts(hbuf);
    if(strlen(hbuf)>200) {
        printf("Hit enter");
        getc(stdin);
    }
}
