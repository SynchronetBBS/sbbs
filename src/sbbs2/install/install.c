/* install.c */

/* Developed 1990-1997 by Rob Swindell; PO Box 501, Yorba Linda, CA 92885 */

/* Synchronet BBS Installation program */

unsigned int _stklen=0x8000;

#include <io.h>
#include <dir.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <process.h>
#include <errno.h>
#include "uifc.h"

/* OS Specific */
#if defined(__FLAT__)	// 32-bit
	#define FAR16
	#define HUGE16
	#define lread(f,b,l) read(f,b,l)
	#define lfread(b,l,f) fread(b,l,f)
	#define lwrite(f,b,l) write(f,b,l)
	#define lfwrite(b,l,f) fwrite(b,l,f)
#else					// 16-bit
	#define FAR16 far
	#define HUGE16 huge
#endif

void bail(int code);

char **opt;

#define EXEC		(1<<1)
#define CFGS		(1<<2)
#define TEXT		(1<<3)
#define DOCS		(1<<4)

#define DISK1		(EXEC|CFGS|TEXT|DOCS)

#define UTIL		(1<<10)
#define XTRN		(1<<11)
#define XSDK		(1<<12)

#define DISK2		(UTIL|XTRN|XSDK)

#define UPGRADE 	(1<<13)

#define LOOP_NOPEN 100

int mode=(DISK1|DISK2);

enum {				/* Upgrade FROM version */
	 NONE
	,SBBS20
	,SBBS21
	,SBBS22
	,SBBS23
	};

char tmp[256];
char install_to[129],install_from[256];

/****************************************************************************/
/* Puts a backslash on path strings 										*/
/****************************************************************************/
void backslash(char *str)
{
    int i;

i=strlen(str);
if(i && str[i-1]!='\\') {
    str[i]='\\'; str[i+1]=0; }
}


/****************************************************************************/
/* If the directory 'path' doesn't exist, create it.                      	*/
/* returns 1 if successful, 0 otherwise 									*/
/****************************************************************************/
int md(char *path)
{
	char str[128];
	struct ffblk ff;

if(strlen(path)<2) {
	umsg("Invalid path");
	return(0); }
strcpy(str,path);
if(strcmp(str+1,":\\") && str[strlen(str)-1]=='\\')
	str[strlen(str)-1]=0;	/* Chop of \ if not root directory */
if(findfirst(str,&ff,FA_DIREC)) {
	if(mkdir(str)) {
		sprintf(str,"Unable to Create Directory '%s'",path);
		umsg(str);
		return(0); } }
return(1);
}

long fdate_dir(char *filespec)
{
    struct ffblk f;
    struct date fd;
    struct time ft;

if(findfirst(filespec,&f,FA_RDONLY|FA_HIDDEN|FA_SYSTEM|FA_DIREC)==0) {
	fd.da_day=f.ff_fdate&0x1f;
	fd.da_mon=(f.ff_fdate>>5)&0xf;
	fd.da_year=1980+((f.ff_fdate>>9)&0x7f);
	ft.ti_hour=0;
	ft.ti_min=0;
	ft.ti_sec=0;
    return(dostounix(&fd,&ft)); }
else return(0);
}

/****************************************************************************/
/* Returns the length of the file in 'filespec'                             */
/****************************************************************************/
long flength(char *filespec)
{
	struct ffblk f;

if(findfirst(filespec,&f,0)==0)
	return(f.ff_fsize);
return(-1L);
}

/****************************************************************************/
/* Checks the disk drive for the existence of a file. Returns 1 if it       */
/* exists, 0 if it doesn't.                                                 */
/****************************************************************************/
char fexist(char *filespec)
{
    struct ffblk f;

if(findfirst(filespec,&f,FA_RDONLY|FA_HIDDEN|FA_SYSTEM|FA_DIREC)==0)
    return(1);
return(0);
}

#ifndef __OS2__
/****************************************************************************/
/* This function reads files that are potentially larger than 32k.  		*/
/* Up to one megabyte of data can be read with each call.                   */
/****************************************************************************/
long lread(int file, char huge *buf,long bytes)
{
	long count;

for(count=bytes;count>32767;count-=32767,buf+=32767)
	if(read(file,(char *)buf,32767)!=32767)
		return(-1L);
if(read(file,(char *)buf,(int)count)!=count)
	return(-1L);
return(bytes);
}

/****************************************************************************/
/* This function writes files that are potentially larger than 32767 bytes  */
/* Up to one megabytes of data can be written with each call.				*/
/****************************************************************************/
long lwrite(int file, char huge *buf, long bytes)
{

	long count;

for(count=bytes;count>32767;count-=32767,buf+=32767)
	if(write(file,(char *)buf,32767)!=32767)
		return(-1L);
if(write(file,(char *)buf,(int)count)!=count)
	return(-1L);
return(bytes);
}
#endif

/****************************************************************************/
/* Network open function. Opens all files DENYALL and retries LOOP_NOPEN    */
/* number of times if the attempted file is already open or denying access	*/
/* for some other reason.	All files are opened in BINARY mode.			*/
/****************************************************************************/
int nopen(char *str, int access)
{
	char logstr[256];
	int file,share,count=0;

if(access==O_RDONLY) share=O_DENYWRITE;
	else share=O_DENYALL;
while(((file=open(str,O_BINARY|share|access,S_IWRITE))==-1)
	&& errno==EACCES && count++<LOOP_NOPEN);
if(file==-1 && errno==EACCES)
	cputs("\7\r\nNOPEN: ACCESS DENIED\r\n\7");
return(file);
}

int copy(char *src, char *dest)
{
	char c,HUGE16 *buf,str[256],str2[128],*scrnsave;
	int in,out;
	long length,chunk,l;
	struct ftime ftime;

if(!strcmp(src,dest))	/* copy from and to are same, so return */
	return(0);

if((scrnsave=MALLOC(scrn_len*80*2))==NULL)
	return(-1);
hidemouse();
gettext(1,1,80,scrn_len,scrnsave);

#define width 72
#define top 10
#define left 0

sprintf(str2,"Copying %s to %s...",src,dest);
sprintf(str," %-64.64s",str2); /* Make sure it's no more than 60 */

gotoxy(SCRN_LEFT+left,SCRN_TOP+top);
textattr(hclr|(bclr<<4));
putch('É');
for(c=1;c<width-1;c++)
	putch('Í');
putch('»');
gotoxy(SCRN_LEFT+left,SCRN_TOP+top+1);
putch('º');
textattr(lclr|(bclr<<4));
cprintf("%-*.*s",width-2,width-2,str);
gotoxy(SCRN_LEFT+left+(width-1),SCRN_TOP+top+1);
textattr(hclr|(bclr<<4));
putch('º');
gotoxy(SCRN_LEFT+left,SCRN_TOP+top+2);
putch('È');
for(c=1;c<width-1;c++)
	putch('Í');
putch('¼');
gotoxy(SCRN_LEFT+left+(width-5),SCRN_TOP+top+1);
textattr(lclr|(bclr<<4));

if((in=nopen(src,O_RDONLY))==-1) {
	cprintf("\r\nERR_OPEN %s",src);
	getch();
	puttext(1,1,80,scrn_len,scrnsave);
	FREE(scrnsave);
	showmouse();
    return(-1); }
if((out=nopen(dest,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
	close(in);
	sprintf(str,"ERROR OPENING %s",dest);
	umsg(str);
	puttext(1,1,80,scrn_len,scrnsave);
	FREE(scrnsave);
	showmouse();
	return(-1); }
length=filelength(in);
if(!length) {
	close(in);
	close(out);
	sprintf(str,"ZERO LENGTH %s",src);
	umsg(str);
	puttext(1,1,80,scrn_len,scrnsave);
	showmouse();
	FREE(scrnsave);
	return(-1); }
#if __OS2__
chunk=1000000;
#else
chunk=0x8000; /* use 32k chunks */
#endif
if(chunk>length)			/* leave space for stack expansion */
	chunk=length;
if((buf=MALLOC(chunk))==NULL) {
	close(in);
	close(out);
	sprintf(str,"ERROR ALLOC %s %lu",src,chunk);
	umsg(str);
	puttext(1,1,80,scrn_len,scrnsave);
    FREE(scrnsave);
	showmouse();
    return(-1); }
l=0L;
while(l<length) {
	cprintf("%2lu%%",l ? (long)(100.0/((float)length/l)) : 0L);
	if(l+chunk>length)
		chunk=length-l;
	lread(in,buf,chunk);
	lwrite(out,buf,chunk);
	l+=chunk;
	cputs("\b\b\b");
	}
// cputs("   \b\b\b");  /* erase it */
FREE((char *)buf);
getftime(in,&ftime);
setftime(out,&ftime);
close(in);
close(out);
puttext(1,1,80,scrn_len,scrnsave);
FREE(scrnsave);
showmouse();
return(0);
}

int unarc(int timestamp, char *src, char *dir, char *arg)
{
	int i,atr;
	char str[128],cmd[128],*scrnsave;

if((scrnsave=MALLOC((scrn_len+1)*80*2))==NULL)
	return(-1);
hidemouse();
gettext(1,1,80,scrn_len+1,scrnsave);
//lclini(scrn_len+1);
textattr(LIGHTGRAY);
clrscr();
sprintf(cmd,"%sLHA.EXE",install_from);
sprintf(str,"/c%d",timestamp);
i=spawnlp(P_WAIT,cmd,cmd,"x",str,"/s","/m1",src,dir,arg,NULL);
if(i==-1) {
	printf("\r\nCannot execute %s.  Hit any key to continue.",cmd);
	getch(); }
else if(i) {
	printf("\r\n%s returned error level %d.  Hit any key to continue.",cmd,i);
	getch(); }
clrscr();
puttext(1,1,80,scrn_len+1,scrnsave);
showmouse();
FREE(scrnsave);
//lclini(scrn_len);
//textattr(atr);
return(i);
}


void main(int argc, char **argv)
{
	int i,j,k,ver;
	long l;
	char str[128],*p;
	char text_dir[64],docs_dir[64],exec_dir[64],ctrl_dir[64];
	struct text_info txt;
	struct ffblk ff;

timezone=0;
daylight=0;
uifcini();
if(argc>1) {	/* user specified install path */
	sprintf(install_to,"%.20s",argv[1]);
	strupr(install_to);
	if(strlen(install_to)==1) {
		if(!isalpha(install_to[0])) {
			cprintf("\7\r\nInvalid install path - '%s'\r\n",install_to);
			bail(1); }
		strcat(install_to,":"); }
	backslash(install_to);
	if(strlen(install_to)<4)	/* user says C, C:, or C:\, make C:\SBBS */
		strcat(install_to,"SBBS\\"); }
else
	strcpy(install_to,"C:\\SBBS\\");

strcpy(install_from,argv[0]);
strupr(install_from);
p=strrchr(install_from,'\\');
if(p)
	*(p+1)=0;
else {				/* incase of A:INSTALL */
	p=strchr(install_from,':');
	if(p)
		*(p+1)=0; }

if((opt=(char **)MALLOC(sizeof(char *)*MAX_OPTS))==NULL) {
	cputs("memory allocation error\r\n");
	bail(1); }
for(i=0;i<MAX_OPTS;i++)
	if((opt[i]=(char *)MALLOC(MAX_OPLN))==NULL) {
		cputs("memory allocation error\r\n");
		bail(1); }

/***
if(findfirst("MENU.ZIP",&ff,0)) {
	cprintf("\7\r\nCan't find MENU.ZIP\r\n");
	bail(1); }
if(findfirst("TEXT.ZIP",&ff,0)) {
	cprintf("\7\r\nCan't find TEXT.ZIP\r\n");
	bail(1); }
if(findfirst("DOCS.ZIP",&ff,0)) {
	cprintf("\7\r\nCan't find DOCS.ZIP\r\n");
	bail(1); }
if(findfirst("EXEC.ZIP",&ff,0)) {
	cprintf("\7\r\nCan't find EXEC.ZIP\r\n");
	bail(1); }
if(findfirst("PKUNZIP.EXE",&ff,0)) {
	cprintf("\7\r\nCan't find PKUNZIP.EXE\r\n");
	bail(1); }
***/
uscrn("Synchronet Installation Utility  Version 2.3");

i=0;
while(1) {

	helpbuf=
" Synchronet Installation \r\n\r\n"
"If you are installing Synchronet for evaluation or demonstration\r\n"
"purposes, be sure to set Install Registration Key to No.\r\n\r\n"
"If do not have the Synchronet Utilities Disk (Distribution Disk 2), be\r\n"
"sure to set Install Distribution Disk 2 to None.\r\n\r\n"
"When you are happy with the installation settings and the destination\r\n"
"path, select Start Installation to continue or hit  ESC  to abort.";

	j=0;
	str[0]=0;
	if((mode&DISK1)==DISK1)
		strcpy(str,"All");
	else {
		if(!(mode&DISK1))
			strcpy(str,"None");
		else {
			strcat(str,"Selected: ");
			if(mode&EXEC)
				strcat(str,"EXEC ");
			if(mode&CFGS)
				strcat(str,"CFGS ");
			if(mode&TEXT)
				strcat(str,"TEXT ");
			if(mode&DOCS)
				strcat(str,"DOCS "); } }
	sprintf(opt[j++],"%-30.30s%s","Install Distribution Disk 1",str);
	str[0]=0;
	if((mode&DISK2)==DISK2)
		strcpy(str,"All");
	else {
		if(!(mode&DISK2))
			strcpy(str,"None");
		else {
			strcat(str,"Selected: ");
			if(mode&UTIL)
				strcat(str,"UTIL ");
			if(mode&XTRN)
				strcat(str,"XTRN ");
			if(mode&XSDK)
				strcat(str,"XSDK "); } }
	sprintf(opt[j++],"%-30.30s%s","Install Distribution Disk 2",str);
	sprintf(opt[j++],"%-30.30s%s","Source Path",install_from);
	sprintf(opt[j++],"%-30.30s%s","Target Path",install_to);
	strcpy(opt[j++],"Start Installation");
	opt[j][0]=0;
	switch(ulist(WIN_ORG|WIN_MID|WIN_ACT|WIN_ESC,0,0,60,&i,0
		,"Synchronet Multinode BBS Installation",opt)) {
		case 0:
			j=0;
			while(1) {
				k=0;
				sprintf(opt[k++],"%-30.30s%3s","Main Executables"
					,mode&EXEC ? "Yes":"No");
				sprintf(opt[k++],"%-30.30s%3s","Default Configuration"
					,mode&CFGS ? "Yes":"No");
				sprintf(opt[k++],"%-30.30s%3s","Text and Menus"
					,mode&TEXT ? "Yes":"No");
				sprintf(opt[k++],"%-30.30s%3s","Documentation"
					,mode&DOCS ? "Yes":"No");
				strcpy(opt[k++],"All");
				strcpy(opt[k++],"None");
				opt[k][0]=0;
helpbuf=
" Distribution Disk 1 \r\n\r\n"
"If you are a installing Synchronet for the first time, it is suggested\r\n"
"that you install All of Disk 1.\r\n\r\n"
"If for some reason, you wish to partially install Synchronet or update\r\n"
"a previously installed version, you can toggle the installation of the\r\n"
"following file sets:\r\n\r\n"
"EXEC: Main executable files necessary for Synchronet to run\r\n\r\n"
"CFGS: Default configuration files\r\n\r\n"
"TEXT: Text and menu files\r\n\r\n"
"DOCS: Online ASCII documentation for the system operator";
				j=ulist(0,0,0,0,&j,0,"Distribution Disk 1",opt);
				if(j==-1)
					break;
				switch(j) {
					case 0:
						mode^=EXEC;
						break;
					case 1:
						mode^=CFGS;
						break;
					case 2:
						mode^=TEXT;
						break;
					case 3:
						mode^=DOCS;
						break;
					case 4:
						mode|=DISK1;
						break;
					case 5:
						mode&=~DISK1;
						break; } }
			break;
		case 1:
			j=0;
			while(1) {
				k=0;
				sprintf(opt[k++],"%-30.30s%3s","Utilities"
					,mode&UTIL ? "Yes":"No");
				sprintf(opt[k++],"%-30.30s%3s","Online External Programs"
					,mode&XTRN ? "Yes":"No");
				sprintf(opt[k++],"%-30.30s%3s","External Program SDK"
					,mode&XSDK ? "Yes":"No");
				strcpy(opt[k++],"All");
				strcpy(opt[k++],"None");
				opt[k][0]=0;
helpbuf=
" Distribution Disk 2 \r\n\r\n"
"This disk is optional for the execution of Synchronet. This disk is\r\n"
"also referred to as the Synchronet Utilities Disk.\r\n\r\n"
"If for some reason, you wish to partially install Synchronet or update\r\n"
"a previously installed version, you can toggle the installation of the\r\n"
"following file sets:\r\n\r\n"
"UTIL: Utilities to enhance the operation of Synchronet\r\n\r\n"
"XTRN: Online External Program Samples\r\n\r\n"
"XSDK: External Program Software Development Kit (for programmers)";
				j=ulist(WIN_RHT,0,0,0,&j,0,"Distribution Disk 2",opt);
				if(j==-1)
					break;
				switch(j) {
					case 0:
						mode^=UTIL;
						break;
					case 1:
						mode^=XTRN;
						break;
					case 2:
						mode^=XSDK;
						break;
					case 3:
						mode|=DISK2;
						break;
					case 4:
						mode&=~DISK2;
                        break; } }
			break;
		case 2:
helpbuf=
" Source Path \r\n\r\n"
"This is the complete path (drive and directory) to install Synchronet\r\n"
"from. The suggested path is A:\\, but any valid DOS drive and\r\n"
"directory may be used.";
			uinput(WIN_L2R,0,0,"Source Path",install_from
				,30,K_EDIT|K_UPPER);
			backslash(install_from);
            break;
		case 3:
helpbuf=
" Target Path \r\n\r\n"
"This is the complete path (drive and directory) to install Synchronet\r\n"
"to. The suggested path is C:\\SBBS\\, but any valid DOS drive and\r\n"
"directory may be used.";
			uinput(WIN_L2R|WIN_BOT,0,0,"Target Path",install_to
				,20,K_EDIT|K_UPPER);
			backslash(install_to);
			break;
		case 4:
			strcpy(opt[0],"Yes");
			strcpy(opt[1],"No");
			opt[2][0]=0;
			j=1;
			j=ulist(WIN_MID|WIN_SAV,0,0,0,&j,0
				,"Are you upgrading from an older version (already installed)"
				,opt);
			if(j==-1)
				break;
			if(j==0) {
				helpbuf=
" Upgrade Back-up \r\n"
"\r\n"
"INSTALL will take care to only overwrite files that have been changed\r\n"
"in this new version, but it is always a good idea to do a complete\r\n"
"system back-up before upgrading, just to be safe.\r\n"
"\r\n"
"If you have customized (with an editor) any of the following files, you\r\n"
"will want to back them up (copy to another disk or directory) before\r\n"
"you begin the upgrade procedure:\r\n"
"\r\n"
"TEXT.DAT (CTRL\\TEXT.DAT)\r\n"
"Command Shells and Loadable Modules (EXEC\\*.SRC)\r\n"
"Menus (TEXT\\MENU\\*.ASC)\r\n"
"Match Maker Questionnaires (XTRN\\SMM\\*.QUE)\r\n"
"External Program Menus (XTRN\\SMM\\*.ASC, XTRN\\SCB\\*.ASC, etc)\r\n";
				help();
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				j=0;
				j=ulist(WIN_MID|WIN_SAV,0,0,0,&j,0
					,"Continue with upgrade"
					,opt);
				if(j)
					break;
				mode|=UPGRADE;
				mode&=~CFGS; }
			else
				mode&=~UPGRADE;
			strcpy(str,install_to);
			if(str[1]==':')                /* drive letter specified */
				setdisk(toupper(str[0])-'A');
			if(!md(str))
				break;
			sprintf(exec_dir,"%sEXEC\\",install_to);
			if(mode&UPGRADE)
				if(uinput(WIN_L2R|WIN_SAV,0,0,"EXEC Directory",exec_dir
					,30,K_EDIT|K_UPPER)<1)
					break;
			backslash(exec_dir);
			if(!md(exec_dir))
				break;
			if(mode&UPGRADE) {
				sprintf(str,"%sSBBS.EXE",exec_dir);
				l=fdate_dir(str);
				ver=NONE;
				switch(l) {
					case 0x3044fb80:	/* 08/31/95 v2.20a */
						ver=SBBS22;
						break;
					case 0x2f6a2280:	/* 03/18/95 v2.11a */
					case 0x2f43e700:	/* 02/17/95 v2.10a */
						ver=SBBS21;
						break;
					case 0x2e6fa580:	/* 09/09/94 v2.00g */
					case 0x2e627680:	/* 08/30/94 v2.00f */
					case 0x2e5a8d80:	/* 08/24/94 v2.00e */
					case 0x2e569900:	/* 08/21/94 v2.00d */
					case 0x2e554780:	/* 08/20/94 v2.00c */
					case 0x2e0b7380:	/* 06/25/94 v2.00b */
					case 0x2ded2100:	/* 06/02/94 v2.00a */
						ver=SBBS20;
						break; }
				if(ver==NONE && flength(str)==642448UL)
					ver=SBBS22; /* v2.20b - possibly patched */
				if(ver==NONE) {
					strcpy(opt[0],"Version 1");
					strcpy(opt[1],"Version 2.0");
					strcpy(opt[2],"Version 2.1");
					strcpy(opt[3],"Version 2.2");
					strcpy(opt[4],"Version 2.3 beta");
					opt[5][0]=0;
					j=0;
					j=ulist(WIN_MID|WIN_SAV,0,0,0,&j,0
						,"Which version are you upgrading from?"
						,opt);
					if(j<0)
						break;
					ver=j;
					if(ver==NONE) { // Version 1
						umsg("INSTALL can only upgrade from Version 2.0 or later");
						umsg("See the file UPGRADE.DOC for UPGRADE instructions");
						break; } } }

			if(mode&EXEC) {
				strcpy(opt[0],"Skip This Disk");
				strcpy(opt[1],"Okay, Try Again");
                strcpy(opt[2],"Change Source Path...");
				opt[3][0]=0;
				j=1;
				while(1) {
					sprintf(str,"%sEXEC.LZH",install_from);
					if(!findfirst(str,&ff,0))
						break;
					j=ulist(WIN_MID|WIN_SAV,0,0,0,&j,0
						,"Insert Distribution Disk 1",opt);
					if(j==-1)
						break;
					if(j==0) {
						mode&=~DISK1;
						break; }
					if(j==2) {
						uinput(WIN_L2R|WIN_SAV,0,0,"Source Path",install_from
							,30,K_EDIT|K_UPPER);
						backslash(install_from); } }
				if(j==-1)
					break;

				/* If they already exist, flag as read/write */
				if(mode&DISK1) {
					sprintf(str,"%sSBBS.EXE",exec_dir);
					chmod(str,S_IREAD|S_IWRITE);
					sprintf(str,"%sSCFG.EXE",exec_dir);
					chmod(str,S_IREAD|S_IWRITE);

					sprintf(str,"%sEXEC.LZH",install_from);
					sprintf(tmp,"%sEXEC.LZH",exec_dir);
					if(copy(str,tmp))
						break;
					if(mode&UPGRADE) {
						sprintf(str,"@%sEXEC.UPG",install_from);
						if(fexist(str+1))
							if(unarc(1,tmp,exec_dir,str)) {
								remove(tmp);
								break; }
						if(unarc(0,tmp,exec_dir,NULL)) {
                            remove(tmp);
							break; } }
                    else
						if(unarc(1,tmp,exec_dir,NULL)) {
							remove(tmp);
							break; }
					remove(tmp);

					/* now set to READ ONLY */
					sprintf(str,"%sSBBS.EXE",exec_dir);
					if(chmod(str,S_IREAD))
						umsg("Error setting SBBS.EXE to READ ONLY");
					sprintf(str,"%sSCFG.EXE",exec_dir);
					if(chmod(str,S_IREAD))
						umsg("Error setting SCFG.EXE to READ ONLY"); } }

			if(mode&CFGS) {
				sprintf(str,"%sCTRL",install_to);
				if(!md(str))
					break;
				strcpy(opt[0],"Skip This Disk");
				strcpy(opt[1],"Okay, Try Again");
                strcpy(opt[2],"Change Source Path...");
                opt[3][0]=0;
				j=1;
				while(1) {
					sprintf(str,"%sCFGS.LZH",install_from);
					if(!findfirst(str,&ff,0))
						break;
					j=ulist(WIN_MID|WIN_SAV,0,0,0,&j,0
						,"Insert Distribution Disk 1",opt);
					if(j==-1)
						break;
					if(j==0) {
						mode&=~DISK1;
						break; }
					if(j==2) {
						uinput(WIN_L2R|WIN_SAV,0,0,"Source Path",install_from
							,30,K_EDIT|K_UPPER);
						backslash(install_from); } }
				if(j==-1)
					break;
				if(mode&DISK1) {	 /* not skip */
					sprintf(str,"%sCFGS.LZH",install_from);
					sprintf(tmp,"%sCFGS.LZH",install_to);
					if(copy(str,tmp))
						break;
					sprintf(str,"%sCTRL",install_to);
					if(unarc(1,tmp,install_to,NULL)) {
						remove(tmp);
						break; }
					remove(tmp); } }

			if(mode&TEXT) {
				sprintf(ctrl_dir,"%sCTRL\\",install_to);
				if(!md(ctrl_dir))
					break;
				sprintf(text_dir,"%sTEXT\\",install_to);
				if(!md(text_dir))
                    break;
				strcpy(opt[0],"Skip This Disk");
				strcpy(opt[1],"Okay, Try Again");
                strcpy(opt[2],"Change Source Path...");
                opt[3][0]=0;
				j=1;
				while(1) {
					sprintf(str,"%sTEXT.LZH",install_from);
					if(!findfirst(str,&ff,0))
						break;
					j=ulist(WIN_MID|WIN_SAV,0,0,0,&j,0
						,"Insert Distribution Disk 1",opt);
					if(j==-1)
						break;
					if(j==0) {
						mode&=~DISK1;
						break; }
					if(j==2) {
						uinput(WIN_L2R|WIN_SAV,0,0,"Source Path",install_from
							,30,K_EDIT|K_UPPER);
						backslash(install_from); } }
				if(j==-1)
					break;
				if(mode&DISK1) {
					sprintf(str,"%sTEXT.LZH",install_from);
					sprintf(tmp,"%sTEXT.LZH",install_to);
					if(copy(str,tmp))
						break;
					if(mode&UPGRADE) {
						sprintf(str,"@%sTEXT.UPG",install_from);
						if(fexist(str+1))
							if(unarc(1,tmp,install_to,str)) {
								remove(tmp);
								break; }
						if(unarc(0,tmp,install_to,NULL)) {
                            remove(tmp);
							break; } }
					else
						if(unarc(1,tmp,install_to,NULL)) {
							remove(tmp);
							break; }
					remove(tmp); } }

			if(mode&DOCS) {
				sprintf(docs_dir,"%sDOCS\\",install_to);
				if(mode&UPGRADE)
					if(uinput(WIN_L2R|WIN_SAV,0,0,"DOCS Directory",docs_dir
						,30,K_EDIT|K_UPPER)<1)
                        break;
				backslash(docs_dir);
				if(!md(docs_dir))
					break;
				strcpy(opt[0],"Skip This Disk");
				strcpy(opt[1],"Okay, Try Again");
                strcpy(opt[2],"Change Source Path...");
                opt[3][0]=0;
				j=1;
				while(1) {
					sprintf(str,"%sDOCS.LZH",install_from);
					if(!findfirst(str,&ff,0))
						break;
					j=ulist(WIN_MID|WIN_SAV,0,0,0,&j,0
						,"Insert Distribution Disk 1",opt);
					if(j==-1)
						break;
					if(j==0) {
						mode&=~DISK1;
						break; }
					if(j==2) {
						uinput(WIN_L2R|WIN_SAV,0,0,"Source Path",install_from
							,30,K_EDIT|K_UPPER);
						backslash(install_from); } }
				if(j==-1)
					break;
				if(mode&DISK1) {
					sprintf(str,"%sDOCS.LZH",install_from);
					sprintf(tmp,"%sDOCS.LZH",install_to);
					if(copy(str,tmp))
						break;
					if(unarc(1,tmp,docs_dir,NULL)) {
						remove(tmp);
						break; }
					remove(tmp); } }

			if(mode&UTIL) {
				strcpy(opt[0],"Skip This Disk");
				strcpy(opt[1],"Okay, Try Again");
                strcpy(opt[2],"Change Source Path...");
                opt[3][0]=0;
				j=1;
				while(1) {
					sprintf(str,"%sUTIL.LZH",install_from);
					if(!findfirst(str,&ff,0))
						break;
					j=ulist(WIN_MID|WIN_SAV,0,0,0,&j,0
						,"Insert Distribution Disk 2 (Utilities Disk)",opt);
					if(j==-1)
						break;
					if(j==0) {
						mode&=~DISK2;
						break; }
					if(j==2) {
						uinput(WIN_L2R|WIN_SAV,0,0,"Source Path",install_from
							,30,K_EDIT|K_UPPER);
						backslash(install_from); } }
				if(j==-1)
					break;

				if(mode&DISK2) {
					sprintf(str,"%sUTIL.LZH",install_from);
					sprintf(tmp,"%sUTIL.LZH",exec_dir);
					if(copy(str,tmp))
						break;
					if(mode&UPGRADE) {
						sprintf(str,"@%sUTIL.UPG",install_from);
						if(fexist(str+1))
							if(unarc(1,tmp,exec_dir,str)) {
								remove(tmp);
								break; }
						if(unarc(0,tmp,exec_dir,NULL)) {
                            remove(tmp);
							break; } }
                    else
						if(unarc(1,tmp,exec_dir,NULL)) {
							remove(tmp);
							break; }
					remove(tmp); } }

			if(mode&XTRN) {
				sprintf(str,"%sXTRN",install_to);
				if(!md(str))
					break;
				strcpy(opt[0],"Skip This Disk");
				strcpy(opt[1],"Okay, Try Again");
                strcpy(opt[2],"Change Source Path...");
                opt[3][0]=0;
				j=1;
				while(1) {
					sprintf(str,"%sXTRN.LZH",install_from);
					if(!findfirst(str,&ff,0))
						break;
					j=ulist(WIN_MID|WIN_SAV,0,0,0,&j,0
						,"Insert Distribution Disk 2 (Utilities Disk)",opt);
					if(j==-1)
						break;
					if(j==0) {
						mode&=~DISK2;
						break; }
					if(j==2) {
						uinput(WIN_L2R|WIN_SAV,0,0,"Source Path",install_from
							,30,K_EDIT|K_UPPER);
						backslash(install_from); } }
				if(j==-1)
					break;

				if(mode&DISK2) {
					sprintf(str,"%sXTRN.LZH",install_from);
					sprintf(tmp,"%sXTRN.LZH",install_to);
					if(copy(str,tmp))
						break;
					if(mode&UPGRADE) {
						sprintf(str,"@%sXTRN.UPG",install_from);
						if(fexist(str+1))
							if(unarc(1,tmp,install_to,str)) {
								remove(tmp);
								break; }
						if(unarc(0,tmp,install_to,NULL)) {
                            remove(tmp);
							break; } }
					else
						if(unarc(1,tmp,install_to,NULL)) {
							remove(tmp);
                            break; }
					remove(tmp); } }

			if(mode&XSDK) {
				sprintf(str,"%sXTRN",install_to);
				if(!md(str))
					break;
				sprintf(str,"%sXTRN\\SDK",install_to);
				if(!md(str))
                    break;
				strcpy(opt[0],"Skip This Disk");
				strcpy(opt[1],"Okay, Try Again");
                strcpy(opt[2],"Change Source Path...");
                opt[3][0]=0;
				j=1;
				while(1) {
					sprintf(str,"%sXSDK.LZH",install_from);
					if(!findfirst(str,&ff,0))
						break;
					j=ulist(WIN_MID|WIN_SAV,0,0,0,&j,0
						,"Insert Distribution Disk 2 (Utilities Disk)",opt);
					if(j==-1)
						break;
					if(j==0) {
						mode&=~DISK2;
						break; }
					if(j==2) {
						uinput(WIN_L2R|WIN_SAV,0,0,"Source Path",install_from
							,30,K_EDIT|K_UPPER);
						backslash(install_from); } }
				if(j==-1)
					break;

				if(mode&DISK2) {
					sprintf(str,"%sXSDK.LZH",install_from);
					sprintf(tmp,"%sXTRN\\XSDK.LZH",install_to);
					if(copy(str,tmp))
						break;
					sprintf(str,"%sXTRN\\SDK\\",install_to);
					if(unarc(1,tmp,str,NULL)) {
						remove(tmp);
						break; }
					remove(tmp); } }
			sprintf(str,"%sNODE1",install_to);
			chdir(str);
			if(mode&UPGRADE) {
				helpbuf=
" Additional Upgrade Instructions \r\n"
"\r\n"
"If you were previously running version 1.1 of SyncEdit, you'll need to\r\n"
"change your command lines and toggle options in SCFG for this new\r\n"
"version (2.0). See DOCS\\SYNCEDIT.DOC for details.\r\n"
"\r\n"
"Make sure you have SCFG->System->Loadable Modules->Login set to LOGIN\r\n"
"and SCFG->System->Loadable Modules->Logon set to LOGON.\r\n"
"\r\n"
"If you want your users to be able to use WIP compatible terminals with\r\n"
"your BBS, add WIPSHELL to SCFG->Command Shells with an Access\r\n"
"Requirement String of \"WIP\".";
				help(); }
			uifcbail();
			if(mode&UPGRADE) {
				cprintf("Synchronet Upgrade Complete.\r\n");
				exit(0); }
			if(mode&CFGS)
				p="/F";
			else
				p=NULL;
			spawnl(P_WAIT,"..\\EXEC\\SCFG.EXE"
				,"..\\EXEC\\SCFG","..\\CTRL",p,NULL);
			textattr(LIGHTGRAY);
			cprintf(
"Synchronet BBS and its utilities use file and record locking to maintain\r\n"
"data integrity in a multinode environment. File and record locking under DOS\r\n"
"requires the use of SHARE.\r\n"
"\r\n"
			);
			cprintf(
"SHARE is a program that is distributed with MS-DOS and PC-DOS v3.0 and higher\r\n"
"and must be executed prior to running SBBS. SHARE.EXE should be located in the\r\n"
"DOS directory of your hard disk.\r\n"
"\r\n"
			);
			cprintf(
"If you are running Microsoft Windows, you must exit Windows and load SHARE\r\n"
"before reloading Windows.\r\n"
"\r\n"
			);
			cprintf(
"It is not necessary to run SHARE if using a single node on a Novell NetWare\r\n"
"workstation or in an OS/2 DOS window.\r\n"
"\r\n"
			);
			cprintf(
"SHARE.EXE can be automatically loaded in your AUTOEXEC.BAT or CONFIG.SYS.\r\n"
"\r\n"
			);
			textattr(WHITE);
			cprintf(
"After you have loaded SHARE, type SBBS from THIS directory.\r\n"
			);
			textattr(LIGHTGRAY);
			exit(0);
		case -1:
			j=0;
			strcpy(opt[0],"Yes");
			strcpy(opt[1],"No");
			opt[2][0]=0;
			if(!(ulist(WIN_MID,0,0,20,&j,0,"Abort Installation",opt)))
				bail(0);
			break; } }
}

void bail(int code)
{

if(code)
	getch();
uifcbail();
exit(code);
}
