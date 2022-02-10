/* Standard I/O Implementation of UIFC (user interface) library */

/* $Id: uifcx.c,v 1.41 2020/08/16 20:37:08 rswindell Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
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

#include "genwrap.h"
#include "gen_defs.h"
#include "xpprintf.h"
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
static int uscrn(const char *str);
static int ulist(int mode, int left, int top, int width, int *dflt, int *bar
	,const char *title, char **option);
static int uinput(int imode, int left, int top, const char *prompt, char *str
	,int len ,int kmode);
static int umsg(const char *str);
static int umsgf(char *str, ...);
static BOOL confirm(char *str, ...);
static BOOL deny(char *str, ...);
static void upop(const char *str);
static void sethelp(int line, char* file);

/****************************************************************************/
/****************************************************************************/
static int uprintf(int x, int y, unsigned attr, char *fmat, ...)
{
	va_list argptr;
	char str[MAX_COLS + 1];
	int i;

	va_start(argptr, fmat);
	vsprintf(str, fmat, argptr);
	va_end(argptr);
	i = printf("%s", str);
	return(i);
}


/****************************************************************************/
/* Initialization function, see uifc.h for details.							*/
/* Returns 0 on success.													*/
/****************************************************************************/
int uifcinix(uifcapi_t* uifcapi)
{
	static char* yesNoOpts[] = {"Yes", "No", NULL};

    if(uifcapi==NULL || uifcapi->size!=sizeof(uifcapi_t))
        return(-1);

    api=uifcapi;

	if (api->yesNoOpts == NULL)
		api->yesNoOpts = yesNoOpts; // Not currently used in this interface instance

    /* install function handlers */
    api->bail=uifcbail;
    api->scrn=uscrn;
    api->msg=umsg;
	api->msgf=umsgf;
	api->confirm=confirm;
	api->deny=deny;
    api->pop=upop;
    api->list=ulist;
    api->input=uinput;
    api->sethelp=sethelp;
    api->showhelp=help;
	api->showbuf=NULL;
	api->timedisplay=NULL;
	api->printf = uprintf;

    setvbuf(stdin,NULL,_IONBF,0);
    setvbuf(stdout,NULL,_IONBF,0);
    api->scrn_len=24;
	api->initialized=TRUE;

    return(0);
}

/****************************************************************************/
/* Exit/uninitialize UIFC implementation.									*/
/****************************************************************************/
void uifcbail(void)
{
	api->initialized=FALSE;
}

/****************************************************************************/
/* Clear screen, fill with background attribute, display application title.	*/
/* Returns 0 on success.													*/
/****************************************************************************/
int uscrn(const char *str)
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
		if(fread(&ch,1,1,stdin)!=1)
			break;
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
/* General menu function, see uifc.h for details.							*/
/****************************************************************************/
int ulist(int mode, int left, int top, int width, int *cur, int *bar
	, const char *title, char **option)
{
    char str[128];
	int i,opts;
    int optnumlen;
    int yesno=0;
    int lines;

    for(opts=0;opts<MAX_OPTS;opts++)
    	if(option[opts]==NULL || option[opts][0]==0)
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
            if(mode&WIN_COPY)
                strcat(str,", Copy");
			if(mode&WIN_CUT)
				strcat(str,", X-Cut");
            if(mode&WIN_PASTE)
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
				if(!(mode&WIN_INS))
					break;
				if(!opts)
    				return(MSK_INS);
                if(i>0 && i<=opts+1)
        			return((i-1)|MSK_INS);
                return(which("Add before",opts+1)|MSK_INS);
            case 'D':   /* Delete */
				if(!(mode&WIN_DEL))
					break;
				if(!opts)
    				break;
                if(i>0 && i<=opts)
        			return((i-1)|MSK_DEL);
                if(opts==1)
                    return(MSK_DEL);
                return(which("Delete",opts)|MSK_DEL);
            case 'C':   /* Copy */
				if(!(mode&WIN_COPY))
					break;
				if(!opts)
    				break;
                if(i>0 && i<=opts)
        			return((i-1)|MSK_COPY);
                if(opts==1)
                    return(MSK_COPY);
                return(which("Copy",opts)|MSK_COPY);
            case 'X':   /* Cut */
				if(!(mode&WIN_CUT))
					break;
				if(!opts)
    				break;
                if(i>0 && i<=opts)
        			return((i-1)|MSK_CUT);
                if(opts==1)
                    return(MSK_CUT);
                return(which("Cut",opts)|MSK_CUT);
            case 'P':   /* Paste */
				if(!(mode&WIN_PASTE))
					break;
				if(!opts)
    				break;
                if(i>0 && i<=opts+1)
        			return((i-1)|MSK_PASTE);
                if(opts==1)
                    return(MSK_PASTE);
                return(which("Insert pasted item before",opts+1)|MSK_PASTE);
        }
    }
}

/*************************************************************************/
/* This function is a windowed input string input routine.               */
/*************************************************************************/
int uinput(int mode, int left, int top, const char *prompt, char *outstr,
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
/* Displays the message 'str' and waits for the user to hit ENTER           */
/****************************************************************************/
int umsg(const char *str)
{
	int ch;
	printf("%s\nHit enter to continue:",str);
	ch = getchar();
	return ch == '\r' || ch == '\n';
}

/* Same as above, using printf-style varargs */
int umsgf(char* fmt, ...)
{
	int retval = -1;
	va_list va;
	char* buf = NULL;

	va_start(va, fmt);
	vasprintf(&buf, fmt, va);
	va_end(va);
	if(buf != NULL) {
		retval = umsg(buf);
		free(buf);
	}
	return retval;
}

BOOL confirm(char* fmt, ...)
{
	int ch;
	va_list va;

	va_start(va, fmt);
	vprintf(fmt, va);
	va_end(va);
	printf(" (Y/n)? ");
	ch = getchar();
	return tolower(ch) != 'n' && ch != EOF;
}

BOOL deny(char* fmt, ...)
{
	int ch;
	va_list va;

	va_start(va, fmt);
	vprintf(fmt, va);
	va_end(va);
	printf(" (N/y)? ");
	ch = getchar();
	return tolower(ch) != 'y';
}

/****************************************************************************/
/* Status popup/down function, see uifc.h for details.						*/
/****************************************************************************/
void upop(const char *str)
{
	static int len;

    if(str==NULL)
        printf("\r%*s\r", len, "");
    else
        len = printf("\r%s\r", str) - 2;
}

/****************************************************************************/
/* Sets the current help index by source code file and line number.			*/
/****************************************************************************/
void sethelp(int line, char* file)
{
    helpline=line;
    helpfile=file;
}

static void uputs(char* ptr)
{
	while(*ptr) {
		if(*ptr>2)
			putchar(*ptr);
		ptr++;
	}
	putchar('\n');
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
            SAFEPRINTF(hbuf,"ERROR: Cannot open help index: %.128s"
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
                if(fread(str,12,1,fp)!=1)
                    break;
                str[12]=0;
                if(fread(&line,2,1,fp)!=1)
					break;
                if(stricmp(str,p) || line!=helpline) {
                    if(fseek(fp,4,SEEK_CUR)==0)
						break;
                    continue;
                }
                if(fread(&l,4,1,fp)!=1)
					l=-1L;
                break;
            }
            fclose(fp);
            if(l==-1L)
                SAFEPRINTF3(hbuf,"ERROR: Cannot locate help key (%s:%u) in: %.128s"
                    ,p,helpline,api->helpixbfile);
            else {
                if((fp=fopen(api->helpdatfile,"rb"))==NULL)
                    SAFEPRINTF(hbuf,"ERROR: Cannot open help file: %.128s"
                        ,api->helpdatfile);
                else {
                    if(fseek(fp,l,SEEK_SET)!=0) {
						SAFEPRINTF4(hbuf,"ERROR: Cannot seek to help key (%s:%u) at %ld in: %.128s"
							,p,helpline,l,api->helpixbfile);
					}
					else {
						if(fread(hbuf,1,HELPBUF_SIZE,fp)<1) {
							SAFEPRINTF4(hbuf,"ERROR: Cannot read help key (%s:%u) at %ld in: %.128s"
								,p,helpline,l,api->helpixbfile);
						}
						hbuf[HELPBUF_SIZE-1] = 0;
					}
					fclose(fp);
				}
			}
		}
		uputs(hbuf);
		if(strlen(hbuf)>200) {
			printf("Hit enter");
			getstr(str,sizeof(str)-1);
		}
	}
    else {
		uputs(api->helpbuf);
		if(strlen(api->helpbuf)>200) {
			printf("Hit enter");
			getstr(str,sizeof(str)-1);
		}
	}
}
