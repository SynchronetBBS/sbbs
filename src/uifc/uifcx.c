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
#include <share.h>

char helpdatfile[256]=""
	,helpixbfile[256]=""
	,*helpfile=0,*helpbuf=0
	,changes=0,uifc_status=0;
char savnum, savdepth;   /* unused */    
uint scrn_len;
uint cursor,helpline=0,max_opts=MAX_OPTS;

int uifcini()
{
    scrn_len=24;
    
    return(scrn_len);
}


void uifcbail(void)
{
}

int uscrn(char *str)
{
    return(0);
}

static int which(char* prompt, int max)
{
    char    str[41];
    int     i;

    while(1) {
        printf("%s which (1-%d): ",prompt,max);
        str[0]=0;
        fgets(str,sizeof(str)-1,stdin);
        i=atoi(str);
        if(i>0 && i<=max)
            return(i-1);
    }
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
    char str[128];
	int i,j,opts;
    int optnumlen;
    int yesno=0;

    for(opts=0;opts<max_opts && opts<MAX_OPTS;opts++)
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
            for(i=0;i<opts;i++) {
                printf("%*d: %s\n",optnumlen,i+1,option[i]);
                if(i && !(i%(scrn_len-2))) {
                    printf("More? ");
                    str[0]=0;
                    fgets(str,sizeof(str)-1,stdin);
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
            printf("\nWhich (Help%s): ",str);
        }
        str[0]=0;
        fgets(str,sizeof(str)-1,stdin);
        
        truncsp(str);
        i=atoi(str);
        if(i>0) {
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
            case 'A':
				if(!opts)
    				return(MSK_INS);
                if(i>0 && i<=opts+1)
        			return((i-1)|MSK_INS);
                return(which("Add before",opts+1)|MSK_INS);
                break;
            case 'D':
				if(!opts)
    				break;
                if(i>0 && i<=opts)
        			return((i-1)|MSK_DEL);
                return(which("Delete",opts)|MSK_INS);
                break;
            case 'C':
				if(!opts)
    				break;
                if(i>0 && i<=opts)
        			return((i-1)|MSK_GET);
                return(which("Copy",opts)|MSK_GET);
                break;
            case 'P':
				if(!opts)
    				break;
                if(i>0 && i<=opts)
        			return((i-1)|MSK_PUT);
                return(which("Paste",opts)|MSK_PUT);
                break;

        }
    }
}


/*************************************************************************/
/* This function is a windowed input string input routine.               */
/*************************************************************************/
int uinput(int mode, char left, char top, char *prompt, char *str,
	char max, int kmode)
{
    printf("%s (maxlen=%u): ",prompt,max);
    fgets(str,max,stdin);
    truncsp(str);
    return(strlen(str));
}

/****************************************************************************/
/* Displays the message 'str' and waits for the user to select "OK"         */
/****************************************************************************/
void umsg(char *str)
{
    printf("%s\n",str);
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
    if(str==NULL)
        printf("\n");
    else
        printf("\r%-79s",str);
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
    if(!helpbuf) {
        if((fp=_fsopen(helpixbfile,"rb",SH_DENYWR))==NULL)
            sprintf(hbuf,"ERROR: Cannot open help index: %s"
                ,helpixbfile);
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
                    ,p,helpline,helpixbfile);
            else {
                if((fp=_fsopen(helpdatfile,"rb",SH_DENYWR))==NULL)
                    sprintf(hbuf,"ERROR: Cannot open help file: %s"
                        ,helpdatfile);
                else {
                    fseek(fp,l,SEEK_SET);
                    fread(hbuf,HELPBUF_SIZE,1,fp);
                    fclose(fp); } } } }
    else
        strcpy(hbuf,helpbuf);

    puts(hbuf);
}

