/* sbbsinst.c */

/* Synchronet installation utility 										*/

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2003 Rob Swindell - http://www.synchro.net/copyright.html		*
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

/********************/
/* Global Variables */
/********************/

#include "sbbsinst.h"
uifcapi_t uifc; /* User Interface (UIFC) Library API */
params_t params; /* Build parameters */

char **opt;
char tmp[256];
char error[256];
int  backup_level=5;

void allocfail(uint size)
{
    printf("\7Error allocating %u bytes of memory.\r\n",size);
    bail(1);
}

int main(int argc, char **argv)
{
	char	**mopt;
	int 	i=0;
	int		main_dflt=1;
	char 	str[129];
	BOOL    door_mode=FALSE;

	/************/
	/* Defaults */
	/************/
	strcpy(params.install_path,"/usr/local/sbbs");
	params.usebcc=FALSE;
	strcpy(params.cflags,"");
	params.release=TRUE;
	params.symlink=TRUE;
	params.cvs=TRUE;
	strcpy(params.cvstag,"HEAD");

    printf("\r\nSynchronet Installation Utility (%s)  v%s  Copyright 2003 "
        "Rob Swindell\r\n",PLATFORM_DESC,VERSION);

    memset(&uifc,0,sizeof(uifc));

	uifc.esc_delay=25;

	for(i=1;i<argc;i++) {
        if(argv[i][0]=='-'
#ifndef __unix__
            || argv[i][0]=='/'
#endif
            )
            switch(toupper(argv[i][1])) {
                case 'C':
        			uifc.mode|=UIFC_COLOR;
                    break;
                case 'D':
                    door_mode=TRUE;
                    break;
                case 'L':
                    uifc.scrn_len=atoi(argv[i]+2);
                    break;
                case 'E':
                    uifc.esc_delay=atoi(argv[i]+2);
                    break;
				case 'I':
					uifc.mode|=UIFC_IBM;
					break;
                case 'V':
#if !defined(__unix__)
                    textmode(atoi(argv[i]+2));
#endif
                    break;
                default:
                    printf("\nusage: sbbsinst [ctrl_dir] [options]"
                        "\n\noptions:\n\n"
                        "-d  =  run in standard input/output/door mode\r\n"
                        "-c  =  force color mode\r\n"
#ifdef USE_CURSES
                        "-e# =  set escape delay to #msec\r\n"
						"-i  =  force IBM charset\r\n"
#endif
#if !defined(__unix__)
                        "-v# =  set video mode to #\r\n"
#endif
                        "-l# =  set screen lines to #\r\n"
                        );
        			exit(0);
           }
    }

uifc.size=sizeof(uifc);
#if defined(USE_FLTK)
if(!door_mode&&(getenv("DISPLAY")!=NULL))
    i=uifcinifltk(&uifc);  /* fltk */
else
#endif
#if defined(USE_DIALOG)
if(!door_mode)
    i=uifcinid(&uifc);  /* dialog */
else
#elif defined(USE_CURSES)
if(!door_mode)
    i=uifcinic(&uifc);  /* curses */
else
#elif !defined(__unix__)
if(!door_mode)
    i=uifcini(&uifc);   /* conio */
else
#endif
    i=uifcinix(&uifc);  /* stdio */
if(i!=0) {
    printf("uifc library init returned error %d\n",i);
    exit(1);
}

if((opt=(char **)MALLOC(sizeof(char *)*(MAX_OPTS+1)))==NULL)
	allocfail(sizeof(char *)*(MAX_OPTS+1));
for(i=0;i<(MAX_OPTS+1);i++)
	if((opt[i]=(char *)MALLOC(MAX_OPLN))==NULL)
		allocfail(MAX_OPLN);

if((mopt=(char **)MALLOC(sizeof(char *)*14))==NULL)
	allocfail(sizeof(char *)*14);
for(i=0;i<14;i++)
	if((mopt[i]=(char *)MALLOC(64))==NULL)
		allocfail(64);

sprintf(str,"Synchronet for %s v%s",PLATFORM_DESC,VERSION);
if(uifc.scrn(str)) {
	printf(" USCRN (len=%d) failed!\r\n",uifc.scrn_len+1);
	bail(1);
}

while(1) {
	i=0;
	sprintf(mopt[i++],"%-33.33s","Install");
	sprintf(mopt[i++],"%-33.33s%s","Install Path",params.install_path);
	sprintf(mopt[i++],"%-33.33s%s","Compiler",params.usebcc?"BCC":"GCC");
	sprintf(mopt[i++],"%-33.33s%s","Compiler Flags",params.cflags);
	sprintf(mopt[i++],"%-33.33s%s","Release Version",params.release?"Yes":"No");
	sprintf(mopt[i++],"%-33.33s%s","Symlink Binaries",params.symlink?"Yes":"No");
	sprintf(mopt[i++],"%-33.33s%s","Pull sources from CVS",params.cvs?"Yes":"No");
	if(params.cvs)
		sprintf(mopt[i++],"%-33.33s%s","CVS Tag",params.cvstag);
	mopt[i][0]=0;

	uifc.helpbuf=	"Main Installation Menu:\n"
					"\nToDo: Add help.";
	switch(uifc.list(WIN_ORG|WIN_MID|WIN_ESC|WIN_ACT,0,0,60,&main_dflt,0
		,"Configure",mopt)) {
		case 0:
			write_makefile();
			install_sbbs();
			bail(0);
			break;
		case 1:
			uifc.helpbuf=	"Install Path\n"
							"\n"
							"\nPath to install the Synchronet BBS system into."
							"\nSome common paths:"
							"\n /sbbs"
							"\n	/usr/local/sbbs"
							"\n	/opt/sbbs"
							"\n	/home/bbs/sbbs";
			uifc.input(WIN_MID,0,0,"Install Path",params.install_path,40,K_EDIT);
			break;
		case 2:
			strcpy(opt[0],"BCC");
			strcpy(opt[1],"GCC");
			opt[2][0]=0;
			i=params.usebcc?0:1;
			uifc.helpbuf=	"Build From CVS\n"
							"\nToDo: Add help.";
			i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
				,"Compiler",opt);
			if(!i)
				params.usebcc=TRUE;
			else if(i==1)
				params.usebcc=FALSE;
			i=0;
			break;
		case 3:
			uifc.helpbuf=	"Compiler Flags\n"
							"\nToDo: Add help.";
			uifc.input(WIN_MID,0,0,"Additional Compiler Flags",params.cflags,40,K_EDIT);
			break;
		case 4:
			strcpy(opt[0],"Yes");
			strcpy(opt[1],"No");
			opt[2][0]=0;
			i=params.release?0:1;
			uifc.helpbuf=	"Build Release Version\n"
							"\nToDo: Add help.";
			i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
				,"Build a release version?",opt);
			if(!i)
				params.release=TRUE;
			else if(i==1)
				params.release=FALSE;
			i=0;
			break;
		case 5:
			strcpy(opt[0],"Yes");
			strcpy(opt[1],"No");
			opt[2][0]=0;
			i=params.symlink?0:1;
			uifc.helpbuf=	"Symlink Binaries:\n"
							"\n"
							"\nShould the installer create symlinks to the binaries or copy them from"
							"\nthe compiled location?";
			i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
				,"Symlink Binaries?",opt);
			if(!i)
				params.symlink=TRUE;
			else if(i==1)
				params.symlink=FALSE;
			i=0;
			break;
		case 6:
			strcpy(opt[0],"Yes");
			strcpy(opt[1],"No");
			opt[2][0]=0;
			i=params.cvs?0:1;
			uifc.helpbuf=	"Pull sources from CVS:\n"
							"\n"
							"\nShould the installer do a CVS update before compiling the binaies?"
							"\n"
							"\nIf this is the first time you have ran SBBSINST, you MUST enable this.";
			i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
				,"Pull from CVS?",opt);
			if(!i)
				params.cvs=TRUE;
			else if(i==1)
				params.cvs=FALSE;
			i=0;
			break;
		case 7:
			uifc.helpbuf=	"CVS Tag:\n"
							"\n"
							"\nCVS tag to use when updating sources.  Enter \"HEAD\" to use current sources.";
			uifc.input(WIN_MID,0,0,"CVS Tag",params.cvstag,40,K_EDIT);
			break;
		case -1:
			i=0;
			strcpy(opt[0],"Yes");
			strcpy(opt[1],"No");
			opt[2][0]=0;
			uifc.helpbuf=	"Exit SBBSINST:\n"
							"\n"
							"\nIf you want to exit the Synchronet installation utility, select Yes."
							"\nOtherwise, select No or hit  ESC .";
			i=uifc.list(WIN_MID,0,0,0,&i,0,"Exit SBBSINST",opt);
			if(!i)
				bail(0);
			break; } }
}

void bail(int code)
{
    if(code) {
        puts("\nHit a key...");
        getch(); }

    uifc.bail();

    exit(code);
}

void write_makefile(void)  {
	FILE *makefile;

	makefile=fopen("/tmp/SBBSmakefile","w");
	if(makefile==NULL)  {
	    uifc.msg("Cannot create /tmp/SBBSmakefile!");
		return;
	}
	if(!params.release)
		  fputs("DEBUG	:=	1\n",makefile);

	if(params.symlink)
		fputs("INSBIN	:=	ln -s\n",makefile);
	else
		fputs("INSBIN	:=	install -c -s\n",makefile);

	if(params.usebcc)  {
		fputs("CCPRE	:=	bcc\n",makefile);
		fputs("MKFLAGS	+=	bcc=1\n",makefile);
	}
	else 
		fputs("CCPRE	:=	gcc\n",makefile);

/* Not supported
ifndef SBBSOWNER
 SBBSCHOWN	:= $(USER)
endif

ifdef SBBSGROUP
 SBBSCHOWN	:= $(SBBSCHOWN):$(SBBSGROUP)
endif

ifdef UNIX_INSTALL
 ifndef PREFIX
  PREFIX	:=	/usr/local
 endif
 MKFLAGS	+=	PREFIX=$(PREFIX)
else	# Classic Install
 ifndef SBBSDIR
  SBBSDIR	:=	$(shell pwd)
 endif
endif

*/
	fputs("os      :=   $(shell uname)\n",makefile);
	fputs("os      :=	$(shell echo $(os) | awk '/.*/ { print tolower($$1)}')\n",makefile);
	fputs("MKFLAGS	+=	os=$(os)\n",makefile);
	if(params.release)  {
		fputs("SUFFIX  :=  release\n",makefile);
		fputs("MKFLAGS	+=	RELEASE=1\n",makefile);
	}
	else  {
		fputs("SUFFIX  :=  debug\n",makefile);
		fputs("MKFLAGS	+=	DEBUG=1\n",makefile);
	}
	if(param.cflags[0])
		fprintf(makefile,"MKFLAGS	+=	CFLAGS=%s\n",params.cflags);
	fputs("SBBSDIR := ",makefile);
	fputs(params.install_path,makefile);
	fputs("\n",makefile);

/* Not supported
ifdef JSLIB
 MKFLAGS	+=	JSLIB=$(JSLIB)
endif
*/

	if(params.cvs)
		fputs("CVSCOMMAND	:=	cvs -z3 -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs\n",makefile);

	fputs("all: externals binaries baja\n\n",makefile);
	fputs("externals:	sbj sbl\n\n",makefile);
	fputs("fbinaries:	sbbs3 scfg\n\n",makefile);

	fputs("sbbs3:	",makefile);
	if(params.cvs)
	fputs("$(SBBSDIR)/src/sbbs3 $(SBBSDIR)/src/uifc $(SBBSDIR)/src/xpdev $(SBBSDIR)/src/mozilla",makefile);
	fputs("\n",makefile);	
	fputs("	gmake -C $(SBBSDIR)/src/sbbs3 $(MKFLAGS)\n",makefile);
	fputs("	MKFLAGS	+=	BAJAPATH=../src/sbbs3/$(CCPRE).$(os).exe.$(SUFFIX)/baja\n\n",makefile);

	fputs("scfg:",makefile);
	if(params.cvs)
		fputs("	$(SBBSDIR)/src/sbbs3 $(SBBSDIR)/src/uifc $(SBBSDIR)/src/xpdev",makefile);
	fputs("\n",makefile);
	fputs("	gmake -C $(SBBSDIR)/src/sbbs3/scfg $(MKFLAGS)\n\n",makefile);

	fputs("baja:",makefile);
	if(params.cvs)
		fputs("	$(SBBSDIR)/exec",makefile);
	fputs(" binaries\n",makefile);
	fputs("	gmake -C $(SBBSDIR)/exec $(MKFLAGS)\n\n",makefile);

	fputs("sbj:",makefile);
	if(params.cvs)
		fputs("	$(SBBSDIR)/xtrn",makefile);
	fputs("\n",makefile);
	fputs("	gmake -C $(SBBSDIR)/xtrn/sbj $(MKFLAGS)\n\n",makefile);

	fputs("sbl:",makefile);
	if(params.cvs)
		fputs("	$(SBBSDIR)/xtrn",makefile);
	fputs("\n",makefile);
	fputs("	gmake -C $(SBBSDIR)/xtrn/sbl $(MKFLAGS)\n\n",makefile);

	fputs("install: all",makefile);
	if(params.cvs)
		fputs(" ctrl text node1",makefile);
	fputs("\n",makefile);
	fputs("	echo Installing to $(SBBSDIR)\n",makefile);
	if(params.release)  {
		fputs("	strip $(SBBSDIR)/src/sbbs3/$(CCPRE).$(os).exe.$(SUFFIX)/baja\n",makefile);
		fputs("	strip $(SBBSDIR)/src/sbbs3/$(CCPRE).$(os).exe.$(SUFFIX)/node\n",makefile);
		fputs("	strip $(SBBSDIR)/src/sbbs3/$(CCPRE).$(os).exe.$(SUFFIX)/chksmb\n",makefile);
		fputs("	strip $(SBBSDIR)/src/sbbs3/$(CCPRE).$(os).exe.$(SUFFIX)/fixsmb\n",makefile);
		fputs("	strip $(SBBSDIR)/src/sbbs3/$(CCPRE).$(os).exe.$(SUFFIX)/addfiles\n",makefile);
		fputs("	strip $(SBBSDIR)/src/sbbs3/$(CCPRE).$(os).exe.$(SUFFIX)/smbutil\n",makefile);
		fputs("	strip $(SBBSDIR)/src/sbbs3/$(CCPRE).$(os).exe.$(SUFFIX)/sbbs\n",makefile);
		fputs("	strip $(SBBSDIR)/src/sbbs3/$(CCPRE).$(os).exe.$(SUFFIX)/sbbsecho\n",makefile);
		fputs("	strip $(SBBSDIR)/src/sbbs3/$(CCPRE).$(os).exe.$(SUFFIX)/filelist\n",makefile);
		fputs("	strip $(SBBSDIR)/src/sbbs3/scfg/$(CCPRE).$(os).$(SUFFIX)/scfg\n",makefile);
	}

	fputs("	strip $(SBBSDIR)/src/sbbs3/$(CCPRE).$(os).exe.$(SUFFIX)/baja $(SBBSDIR)/exec/baja\n",makefile);
	fputs("	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(os).exe.$(SUFFIX)/node $(SBBSDIR)/exec/node\n",makefile);
	fputs("	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(os).exe.$(SUFFIX)/chksmb $(SBBSDIR)/exec/chksmb\n",makefile);
	fputs("	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(os).exe.$(SUFFIX)/fixsmb $(SBBSDIR)/exec/fixsmb\n",makefile);
	fputs("	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(os).exe.$(SUFFIX)/addfiles $(SBBSDIR)/exec/addfiles\n",makefile);
	fputs("	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(os).exe.$(SUFFIX)/smbutil $(SBBSDIR)/exec/smbutil\n",makefile);
	fputs("	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(os).exe.$(SUFFIX)/sbbs $(SBBSDIR)/exec/sbbs\n",makefile);
	fputs("	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(os).exe.$(SUFFIX)/sbbsecho $(SBBSDIR)/exec/sbbsecho\n",makefile);
	fputs("	$(INSBIN) $(SBBSDIR)/src/sbbs3/$(CCPRE).$(os).exe.$(SUFFIX)/filelist $(SBBSDIR)/exec/filelist\n",makefile);
	fputs("	$(INSBIN) $(SBBSDIR)/src/sbbs3/scfg/$(CCPRE).$(os).$(SUFFIX)/scfg $(SBBSDIR)/exec/scfg\n",makefile);
	fputs("	$(INSBIN) $(SBBSDIR)/src/sbbs3/scfg/$(CCPRE).$(os).$(SUFFIX)/scfghelp.ixb $(SBBSDIR)/exec/scfghelp.ixb\n",makefile);
	fputs("	$(INSBIN) $(SBBSDIR)/src/sbbs3/scfg/$(CCPRE).$(os).$(SUFFIX)/scfghelp.dat $(SBBSDIR)/exec/scfghelp.dat\n\n",makefile);
/* Not implemented
fputs("	chown -R $(SBBSCHOWN) $(SBBSDIR)",makefile);
*/

	if(params.cvs)  {
		fputs("$(SBBSDIR)/ctrl: cvslogin\n",makefile);
		fprintf(makefile,"	cd $(SBBSDIR); $(CVSCOMMAND) co -r %s ctrl\n\n",params.cvstag);

		fputs("$(SBBSDIR)/text: cvslogin\n",makefile);
		fprintf(makefile,"	cd $(SBBSDIR); $(CVSCOMMAND) co -r %s text\n\n",params.cvstag);

		fputs("$(SBBSDIR)/exec: cvslogin\n",makefile);
		fprintf(makefile,"	cd $(SBBSDIR); $(CVSCOMMAND) co -r %s exec\n\n",params.cvstag);

		fputs("$(SBBSDIR)/node1: cvslogin\n",makefile);
		fprintf(makefile,"	cd $(SBBSDIR); $(CVSCOMMAND) co -r %s node1\n\n",params.cvstag);

		fputs("$(SBBSDIR)/xtrn: cvslogin\n",makefile);
		fprintf(makefile,"	cd $(SBBSDIR); $(CVSCOMMAND) co -r %s xtrn\n\n",params.cvstag);

		fputs("$(SBBSDIR)/src/sbbs3: cvslogin\n",makefile);
		fprintf(makefile,"	cd $(SBBSDIR); $(CVSCOMMAND) co -r %s src/sbbs3\n\n",params.cvstag);

		fputs("$(SBBSDIR)/src/uifc: cvslogin\n",makefile);
		fprintf(makefile,"	cd $(SBBSDIR); $(CVSCOMMAND) co -r %s src/uifc\n\n",params.cvstag);

		fputs("$(SBBSDIR)/src/xpdev: cvslogin\n",makefile);
		fprintf(makefile,"	cd $(SBBSDIR); $(CVSCOMMAND) co -r %s src/xpdev\n\n",params.cvstag);

		fputs("$(SBBSDIR)/src/mozilla: cvslogin\n",makefile);
		fprintf(makefile,"	cd $(SBBSDIR); $(CVSCOMMAND) co -r %s src/mozilla\n\n",params.cvstag);

		fputs("cvslogin: $(SBBSDIR)\n",makefile);
		fputs("	@echo Press \\<ENTER\\> when prompted for password\n",makefile);
		fputs("	@$(CVSCOMMAND) login\n\n",makefile);
	}
	fputs("$(SBBSDIR):\n",makefile);
	fputs("	@[ ! -e $(SBBSDIR) ] && mkdir $(SBBSDIR);\n",makefile);
	fclose(makefile);
}

void install_sbbs(void)  {
	uifc.bail();
	system("gmake -f /tmp/SBBSmakefile all");
	unlink("/tmp/SBBSmakefile");
}
/* End of SBBSINST.C */
