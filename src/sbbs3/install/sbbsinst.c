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

#include <sys/utsname.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "conwrap.h"
#include "uifc.h"
#include "sbbs.h"
#include "ftpio.h"

/***************/
/* Definitions */
/***************/
#define DEFAULT_CVSROOT		":pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs"
#define RELEASE_LIST_URL1	"ftp://freebsd.synchro.net/main/misc/sbbs-rel.lst"
#define RELEASE_LIST_URL2	"ftp://freebsd.synchro.net/main/misc/sbbs-rel.lst"
#define RELEASE_LIST_URL3	"ftp://freebsd.synchro.net/main/misc/sbbs-rel.lst"
#define RELEASE_LIST_URL4	"ftp://freebsd.synchro.net/main/misc/sbbs-rel.lst"
#define MAX_RELEASES		50
#define	MAX_DIST_FILES			10
#define MAX_SERVERS			100
#define MAX_FILELEN			32

/*******************/
/* DistList Format */
/*******************/

struct server_ent_t {
	char	desc[78];
	char	addr[256];
};

struct dist_t {
	char					version[78];
	char					tag[20];
	struct server_ent_t		**servers;
	char					**files;
	int						type;
};

enum {
	 CVS_SERVER
	,DIST_SET
};

/********************/
/* Global Variables */
/********************/
uifcapi_t uifc; /* User Interface (UIFC) Library API */

struct {
	char install_path[256];
	BOOL usebcc;
	char cflags[256];
	BOOL release;
	BOOL symlink;
	BOOL cvs;
	char cvstag[256];
	char cvsroot[256];
} params; /* Build parameters */

#define MAKEFILE "/tmp/SBBSmakefile"

#define CVSCOMMAND "cvs "

char **opt;
char tmp[256];
char error[256];
char cflags[MAX_PATH+1];
char cvsroot[MAX_PATH+1];
int  backup_level=5;
BOOL keep_makefile=FALSE;

/*************/
/* Constants */
/*************/
char *ftp_user="anonymous";
char *ftp_pass="new@synchro.net";

/**************/
/* Prototypes */
/**************/
void install_sbbs(struct dist_t *, struct server_ent_t *);
struct dist_t **get_distlist(void);
int choose_release(char **opts);
int choose_server(char **opts);

void bail(int code)
{
    if(code) {
        puts("\nHit a key...");
        getch(); 
	}
    uifc.bail();

    exit(code);
}

void allocfail(uint size)
{
    printf("\7Error allocating %u bytes of memory.\r\n",size);
    bail(1);
}

int main(int argc, char **argv)
{
	char	**mopt;
	int 	i=0;
	int		main_dflt=0;
	char 	str[129];
	BOOL    door_mode=FALSE;
	struct dist_t 	**distlist;
	int		release;
	int		server;

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
	strcpy(params.cvsroot,DEFAULT_CVSROOT);

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

	distlist=get_distlist();

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

release=-2;
while(1) {
	if(release==-2)
		release=choose_release((char **)distlist);
	if(release==-1)  {
		i=0;
		strcpy(opt[0],"Yes");
		strcpy(opt[1],"No");
		opt[2][0]=0;
		uifc.helpbuf=	"`Exit SBBSINST:`\n"
						"\n"
						"\nIf you want to exit the Synchronet installation utility, select `Yes`."
						"\nOtherwise, select `No` or hit ~ ESC ~.";
		i=uifc.list(WIN_MID,0,0,0,&i,0,"Exit SBBSINST",opt);
		if(!i)
			bail(0);
		release=-2;
	}

	if(release>=0)  {
		server=choose_server((char **)(((struct dist_t **)distlist)[release]->servers));
		if(server==-1)
			release=-2;
	}

	while(release>=0 && server>=0) {
		i=0;
		sprintf(mopt[i++],"%-33.33s%s","Install Path",params.install_path);
		sprintf(mopt[i++],"%-33.33s%s","Compiler",params.usebcc?"BCC":"GCC");
		sprintf(mopt[i++],"%-33.33s%s","Compiler Flags",params.cflags);
		sprintf(mopt[i++],"%-33.33s%s","Release Version",params.release?"Yes":"No");
		sprintf(mopt[i++],"%-33.33s%s","Symlink Binaries",params.symlink?"Yes":"No");
		sprintf(mopt[i++],"%-33.33s","Start Installation");
		mopt[i][0]=0;

		uifc.helpbuf=	"`Main Installation Menu:`\n"
						"\nToDo: Add help.";
		switch(uifc.list(WIN_MID|WIN_ACT,0,0,60,&main_dflt,0
			,"Configure",mopt)) {
			case 0:
				uifc.helpbuf=	"`Install Path`\n"
								"\n"
								"\nPath to install the Synchronet BBS system into."
								"\nSome common paths:"
								"\n /sbbs"
								"\n	/usr/local/sbbs"
								"\n	/opt/sbbs"
								"\n	/home/bbs/sbbs";
				uifc.input(WIN_MID,0,0,"Install Path",params.install_path,40,K_EDIT);
				break;
			case 1:
				strcpy(opt[0],"BCC");
				strcpy(opt[1],"GCC");
				opt[2][0]=0;
				i=params.usebcc?0:1;
				uifc.helpbuf=	"`Build From CVS`\n"
								"\nToDo: Add help.";
				i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Compiler",opt);
				if(!i)
					params.usebcc=TRUE;
				else if(i==1)
					params.usebcc=FALSE;
				i=0;
				break;
			case 2:
				uifc.helpbuf=	"`Compiler Flags`\n"
								"\nToDo: Add help.";
				uifc.input(WIN_MID,0,0,"Additional Compiler Flags",params.cflags,40,K_EDIT);
				break;
			case 3:
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				i=params.release?0:1;
				uifc.helpbuf=	"`Build Release Version`\n"
								"\nToDo: Add help.";
				i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Build a release version",opt);
				if(!i)
					params.release=TRUE;
				else if(i==1)
					params.release=FALSE;
				i=0;
				break;
			case 4:
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				i=params.symlink?0:1;
				uifc.helpbuf=	"`Symlink Binaries:`\n"
								"\n"
								"\nShould the installer create symlinks to the binaries or copy them from"
								"\nthe compiled location?";
				i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Symlink Binaries",opt);
				if(!i)
					params.symlink=TRUE;
				else if(i==1)
					params.symlink=FALSE;
				i=0;
				break;
			case 5:
				install_sbbs(distlist[release],distlist[release]->servers[server]);
				bail(0);
				break;
			case -1:
				server=-2;
				break; 
		} 
	}
}
}

		/* Some jiggery-pokery here to avoid having to enter the CVS password */
/*		fprintf(makefile,"\tif(grep '%s' -q ~/.cvspass) then echo \"%s A\" >> ~/.cvspass; fi\n",
 *				params.cvsroot,params.cvsroot);
 *
 *		Actually, it looks like you don't NEED to login if the password is blank... huh.
 */

void install_sbbs(struct dist_t *release,struct server_ent_t *server)  {
	char	cmd[MAX_PATH+1];
	char	str[1024];
	char	sbbsdir[9+MAX_PATH];
	char	cvstag[7+MAX_PATH];

	if(params.release)
		putenv("RELEASE=1");
	else
		putenv("DEBUG=1");
	
	if(params.symlink)
		putenv("SYMLINK=1");
	
	sprintf(sbbsdir,"SBBSDIR=%s",params.install_path);
	putenv(sbbsdir);
	
	if(params.usebcc)
		putenv("bcc=1");
	
	if(mkdir(params.install_path,0777)&&errno!=EEXIST)  {
		sprintf(str,"Could not create install %s!",params.install_path);
		uifc.msg(str);
		bail(EXIT_FAILURE);
	}
	if(chdir(params.install_path))  {
		sprintf(str,"Could not change to %s (%d)!",params.install_path,errno);
		uifc.msg(str);
		bail(EXIT_FAILURE);
	}
	uifc.bail();
	switch (release->type)  {
		case CVS_SERVER:
			sprintf(cvstag,"CVSTAG=%s",release->tag);
			putenv(cvstag);
			sprintf(cmd,"cvs -d %s co -r %s install",server->addr,release->tag);
			if(system(cmd))  {
				printf("Could not checkout install makefile.\n");
				exit(EXIT_FAILURE);
			}
			sprintf(cmd,"gmake install -f install/GNUmakefile");
			if(system(cmd))  {
				printf("'Nuff said.\n");
				exit(EXIT_FAILURE);
			}
			exit(EXIT_SUCCESS);
		case DIST_SET:
			
			break;
	}

	uifc.bail();
	system("gmake -f " MAKEFILE " install");
	if(!keep_makefile)
		unlink(MAKEFILE);
}

struct dist_t **
get_distlist(void)
{
	int ret1,ret2,ret3,ret4;
	int i;
	char	in_line[256];
	struct dist_t	**release;
	char	**file=NULL;
	struct server_ent_t	**server=NULL;
	int		r=0;
	int		f=0;
	int		s=0;
	char	*p;
	ftp_FILE	*list;
	char	sep[2]={'\t',0};
	char	str[1024];
	struct utsname	name;	
	char	sys_desc[1024];

	if(uname(&name))  {
		strcpy(name.machine,"Unknown");
		strcpy(name.sysname,name.machine);
	}
	sprintf(sys_desc,"%s-%s",name.sysname,name.machine);
	printf("Sys: %s\n",sys_desc);

	uifc.pop("Fetching distribution list");
	if((list=ftpGetURL(RELEASE_LIST_URL1,ftp_user,ftp_pass,&ret1))==NULL
			&& (list=ftpGetURL(RELEASE_LIST_URL2,ftp_user,ftp_pass,&ret2))==NULL
			&& (list=ftpGetURL(RELEASE_LIST_URL3,ftp_user,ftp_pass,&ret3))==NULL
			&& (list=ftpGetURL(RELEASE_LIST_URL4,ftp_user,ftp_pass,&ret4))==NULL)  {
		printf("Cannot get distribution list!\n");
		sprintf(str,"%s - %s\n%s - %s\n%s - %s\n%s - %s\n",
				RELEASE_LIST_URL1,ftpErrString(ret1),
				RELEASE_LIST_URL2,ftpErrString(ret1),
				RELEASE_LIST_URL3,ftpErrString(ret1),
				RELEASE_LIST_URL4,ftpErrString(ret1));
		uifc.pop(NULL);
		uifc.msg(str);
		bail(EXIT_FAILURE);
	}

	if((release=(struct dist_t **)MALLOC(sizeof(void *)*MAX_RELEASES))==NULL)
		allocfail(sizeof(void *)*MAX_RELEASES);
	for(i=0;i<MAX_RELEASES;i++)
		if((release[i]=(void *)MALLOC(sizeof(struct dist_t)))==NULL)
			allocfail(sizeof(struct dist_t));

	while((list->gets(in_line,sizeof(in_line),list))!=NULL)  {
		i=strlen(in_line);
		while(i>0 && in_line[i]<=' ')
			in_line[i--]=0;

		if(in_line[0]=='D')  {
			strcpy(str,in_line);
			sep[0]=' ';
			p=strtok(str,sep);
			p=strtok(NULL,sep);
			if(strstr(p,sys_desc)==NULL)
				break;
			sep[0]='\n';
			p=strtok(NULL,sep);
			strcpy(in_line,p);
		}

		switch(in_line[0])  {
			case 'C':
				if(file!=NULL)
					file[f]="";
				if(server!=NULL)
					memset(server[s],0,sizeof(struct server_ent_t));

				file=NULL;

				if((server=(struct server_ent_t **)MALLOC(sizeof(void *)*MAX_SERVERS))==NULL)
					allocfail(sizeof(struct server_ent_t *)*MAX_SERVERS);
				for(i=0;i<MAX_SERVERS;i++)
					if((server[i]=(struct server_ent_t *)MALLOC(sizeof(struct server_ent_t)))==NULL)
						allocfail(64);
				f=0;
				s=0;

				memset(release[r],0,sizeof(struct dist_t));
				strcpy(release[r]->version,in_line+2);
				release[r]->type=CVS_SERVER;
				release[r]->servers=server;
				r++;
				break;
			case 'T':
				if(file!=NULL)
					file[f]="";
				if(server!=NULL)
					memset(server[s],0,sizeof(struct server_ent_t));

				if((file=(char **)MALLOC(sizeof(char *)*MAX_DIST_FILES))==NULL)
					allocfail(sizeof(char *)*MAX_DIST_FILES);
				for(i=0;i<MAX_DIST_FILES;i++)
					if((file[i]=(char *)MALLOC(MAX_FILELEN))==NULL)
						allocfail(MAX_FILELEN);

				if((server=(struct server_ent_t **)MALLOC(sizeof(struct server_ent_t *)*MAX_SERVERS))==NULL)
					allocfail(sizeof(struct server_ent_t *)*MAX_SERVERS);
				for(i=0;i<MAX_SERVERS;i++)
					if((server[i]=(struct server_ent_t *)MALLOC(sizeof(struct server_ent_t)))==NULL)
						allocfail(64);
				f=0;
				s=0;

				memset(release[r],0,sizeof(struct dist_t));
				strcpy(release[r]->version,in_line+2);
				release[r]->type=DIST_SET;
				release[r]->servers=server;
				release[r]->files=file;
				r++;
				break;
			case 'f':
				strcpy(file[f++],in_line+2);
				break;
			case 't':
				strcpy(release[r-1]->tag,in_line+2);
				break;
			case 's':
				p=in_line+2;
				sep[0]='\t';
				strcpy(server[s]->addr,strtok(p,sep));
				strcpy(server[s]->desc,strtok(NULL,sep));
				s++;
				break;
		}
	}
	memset(release[r],0,sizeof(struct dist_t));
	uifc.pop(NULL);
	return(release);
}

int choose_release(char **opts)
{
	static int		dist_dflt=0;

	uifc.helpbuf=	"`Distribution List:`\n"
			"\nToDo: Add help.";
	return(uifc.list(WIN_ESC|WIN_ORG|WIN_MID|WIN_ACT|WIN_SAV,0,0,0,&dist_dflt,0
			,"Select Distribution",opts));
}

int choose_server(char **opts)
{
	static int		srvr_dflt=0;

	uifc.helpbuf=	"`Server List:`\n"
				"\nToDo: Add help.";
	return(uifc.list(WIN_MID|WIN_ACT|WIN_SAV,0,0,0,&srvr_dflt,0
		,"Select Server",opts));

}
/* End of SBBSINST.C */
