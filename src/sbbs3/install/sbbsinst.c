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
#define DIST_LIST_URL1		"ftp://vert.synchro.net/Synchronet/sbbsdist.lst"
#define DIST_LIST_URL2		"ftp://rob.synchro.net/Synchronet/sbbsdist.lst"
#define DIST_LIST_URL3		"ftp://cvs.synchro.net/Synchronet/sbbsdist.lst"
#define DIST_LIST_URL4		"ftp://freebsd.synchro.net/Synchronet/sbbsdist.lst"
#define DEFAULT_DISTFILE	"sbbs-src.tgz"
#define DEFAULT_LIBFILE		"libs-%s.tgz"	/* MUST HAVE ONE %s */
#define MAX_DISTRIBUTIONS	50
#define	MAX_DIST_FILES		10
#define MAX_SERVERS			100
#define MAX_FILELEN			32
#define MAKE_ERROR			"make failure.\n"

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
	,LOCAL_FILE
};

/********************/
/* Global Variables */
/********************/
uifcapi_t uifc; /* User Interface (UIFC) Library API */

struct {
	char install_path[256];
	BOOL usebcc;
	char cflags[256];
	BOOL debug;
	BOOL symlink;
	BOOL cvs;
	char cvstag[256];
	char cvsroot[256];
	char make_cmdline[128];
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
int choose_dist(char **opts);
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
	int		dist=0;
	int		server=0;

	/************/
	/* Defaults */
	/************/
	SAFECOPY(params.install_path,"/usr/local/sbbs");
	strcpy(params.make_cmdline,
#if defined(__linux__)
		"make"
#else
		"gmake"
#endif
		" install -f install/GNUmakefile");
	params.usebcc=FALSE;
	SAFECOPY(params.cflags,"");
	params.debug=FALSE;
	params.symlink=TRUE;
	params.cvs=TRUE;
	SAFECOPY(params.cvstag,"HEAD");
	SAFECOPY(params.cvsroot,DEFAULT_CVSROOT);

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
                    printf("\nusage: %s [ctrl_dir] [options]"
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
						,argv[0]
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
		if((mopt[i]=(char *)MALLOC(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	sprintf(str,"Synchronet for %s v%s",PLATFORM_DESC,VERSION);
	if(uifc.scrn(str)) {
		printf(" USCRN (len=%d) failed!\r\n",uifc.scrn_len+1);
		bail(1);
	}

	while(1) {
		i=0;
		sprintf(mopt[i++],"%-27.27s%s","Distribution",distlist[dist]->version);
		sprintf(mopt[i++],"%-27.27s%s","Server"
			,(distlist[dist]->type==LOCAL_FILE?"Local Files":distlist[dist]->servers[server]->desc));
		sprintf(mopt[i++],"%-27.27s%s","Install Path",params.install_path);
		sprintf(mopt[i++],"%-27.27s%s","Compiler",params.usebcc?"Borland":"GNU");
		sprintf(mopt[i++],"%-27.27s%s","Compiler Flags",params.cflags);
		sprintf(mopt[i++],"%-27.27s%s","Debug Build",params.debug?"Yes":"No");
		sprintf(mopt[i++],"%-27.27s%s","Symlink Binaries",params.symlink?"Yes":"No");
		sprintf(mopt[i++],"%-27.27s%s","Make Command-line",params.make_cmdline);
		sprintf(mopt[i++],"%-27.27s","Start Installation...");
		mopt[i][0]=0;

		uifc.helpbuf=	"`Synchronet Installation:`\n"
						"\nToDo: Add help.";
		switch(uifc.list(WIN_ESC|WIN_MID|WIN_ACT,0,0,70,&main_dflt,0
			,"Synchronet Installation",mopt)) {
			case 0:
				i=choose_dist((char **)distlist);
				if(i>=0)  {
					server=0;
					dist=i;
				}
				break;
			case 1:
				if(distlist[dist]->type != LOCAL_FILE)  {
					i=choose_server((char **)distlist[dist]->servers);
					if(i>=0)
						server=0;
				}
				break;
			case 2:
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
			case 3:
				strcpy(opt[0],"Borland");
				strcpy(opt[1],"GNU");
				opt[2][0]=0;
				i=params.usebcc?0:1;
				uifc.helpbuf=	"`Which Compiler`\n"
								"\nToDo: Add help.";
				i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Compiler",opt);
				if(!i)
					params.usebcc=TRUE;
				else if(i==1)
					params.usebcc=FALSE;
				i=0;
				break;
			case 4:
				uifc.helpbuf=	"`Compiler Flags`\n"
								"\nToDo: Add help.";
				uifc.input(WIN_MID,0,0,"Additional Compiler Flags",params.cflags,40,K_EDIT);
				break;
			case 5:
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				i=params.debug?0:1;
				uifc.helpbuf=	"`Debug Build`\n"
								"\nToDo: Add help.";
				i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Build a Debug Version",opt);
				if(!i)
					params.debug=TRUE;
				else if(i==1)
					params.debug=FALSE;
				i=0;
				break;
			case 6:
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
			case 7:
				uifc.helpbuf=	"`Make Command-line`\n"
								"\n";
				uifc.input(WIN_MID,0,0,"Command",params.make_cmdline,60,K_EDIT);
				break;
			case 8:
				install_sbbs(distlist[dist],distlist[dist]->type==LOCAL_FILE?NULL:distlist[dist]->servers[server]);
				bail(0);
				break;
			case -1:
				i=0;
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				uifc.helpbuf=	"`Exit Synchronet Install:`\n"
								"\n"
								"\nIf you want to exit the Synchronet installation utility, select `Yes`."
								"\nOtherwise, select `No` or hit ~ ESC ~.";
				i=uifc.list(WIN_MID,0,0,0,&i,0,"Exit Synchronet Install",opt);
				if(!i)
					bail(0);
				break; 
		} 
	}
}

		/* Some jiggery-pokery here to avoid having to enter the CVS password */
/*		fprintf(makefile,"\tif(grep '%s' -q ~/.cvspass) then echo \"%s A\" >> ~/.cvspass; fi\n",
 *				params.cvsroot,params.cvsroot);
 *
 *		Actually, it looks like you don't NEED to login if the password is blank... huh.
 */

void install_sbbs(struct dist_t *dist,struct server_ent_t *server)  {
	char	cmd[MAX_PATH+1];
	char	str[1024];
	char	sbbsdir[9+MAX_PATH];
	char	cvstag[7+MAX_PATH];
	char	buf[1024];
	char	url[MAX_PATH+1];
	char	path[MAX_PATH+1];
	int		i;
	int		fout,ret1,ret2;
	ftp_FILE	*remote;

	if(params.debug)
		putenv("DEBUG=1");
	else
		putenv("RELEASE=1");
	
	if(params.symlink)
		putenv("SYMLINK=1");
	
	sprintf(sbbsdir,"SBBSDIR=%s",params.install_path);
	putenv(sbbsdir);

	if(params.usebcc)
		putenv("bcc=1");

	sprintf(path,"%s",FULLPATH(str,"./",sizeof(str)));
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
	switch (dist->type)  {
		case CVS_SERVER:
			sprintf(cvstag,"CVSTAG=%s",dist->tag);
			putenv(cvstag);
			sprintf(cmd,"cvs -d %s co -r %s install",server->addr,dist->tag);
			if(system(cmd))  {
				printf("Could not checkout install makefile.\n");
				exit(EXIT_FAILURE);
			}
			if(system(params.make_cmdline))  {
				printf(MAKE_ERROR);
				exit(EXIT_FAILURE);
			}
			exit(EXIT_SUCCESS);
			break;
		case DIST_SET:
			for(i=0;dist->files[i][0];i++)  {
				if((fout=open(dist->files[i],O_WRONLY|O_TRUNC|O_CREAT,S_IRUSR|S_IWUSR))<0)  {
					printf("Could not download distfile to %s (%d)\n",dist->files[i],errno);
					exit(EXIT_FAILURE);
				}
				sprintf(url,"%s%s",server->addr,dist->files[i]);
				if((remote=ftpGetURL(url,ftp_user,ftp_pass,&ret1))==NULL)  {
					printf("Cannot get distribution file %s!\n",dist->files[i]);
					printf("%s\n- %s\n",url,ftpErrString(ret1));
					unlink(dist->files[i]);
					exit(EXIT_FAILURE);
				}
				while((ret1=remote->read(remote,buf,sizeof(buf)))>0)  {
					ret2=write(fout,buf,ret1);
					if(ret2!=ret1)  {
						printf("Error writing to %s\n",dist->files[i]);
						unlink(dist->files[i]);
						exit(EXIT_FAILURE);
					}
				}
				if(ret1<0)  {
					printf("Error downloading %s\n",dist->files[i]);
					unlink(dist->files[i]);
					exit(EXIT_FAILURE);
				}
				sprintf(cmd,"gzip -dc %s | tar -xvf -",dist->files[i]);
				if(system(cmd))  {
					printf("Error extracting %s\n",dist->files[i]);
					unlink(dist->files[i]);
					exit(EXIT_FAILURE);
				}
				unlink(dist->files[i]);
			}
			if(system(params.make_cmdline))  {
				printf(MAKE_ERROR);
				exit(EXIT_FAILURE);
			}
			exit(EXIT_SUCCESS);
			break;
		case LOCAL_FILE:
			for(i=0;dist->files[i][0];i++)  {
				sprintf(cmd,"gzip -dc %s/%s | tar -xvf -",path,dist->files[i]);
				if(system(cmd))  {
					printf("Error extracting %s/%s\n",path,dist->files[i]);
					exit(EXIT_FAILURE);
				}
			}
			if(system(params.make_cmdline))  {
				printf(MAKE_ERROR);
				exit(EXIT_FAILURE);
			}
			exit(EXIT_SUCCESS);
			break;
	}
}

struct dist_t **
get_distlist(void)
{
	int ret1,ret2,ret3,ret4;
	int i,j;
	char	in_line[256];
	struct dist_t	**dist;
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

	if((dist=(struct dist_t **)MALLOC(sizeof(void *)*MAX_DISTRIBUTIONS))==NULL)
		allocfail(sizeof(void *)*MAX_DISTRIBUTIONS);
	for(i=0;i<MAX_DISTRIBUTIONS;i++)
		if((dist[i]=(void *)MALLOC(sizeof(struct dist_t)))==NULL)
			allocfail(sizeof(struct dist_t));

	sprintf(str,DEFAULT_LIBFILE,sys_desc);
	if(fexist(DEFAULT_DISTFILE) && fexist(str))  {
		if((file=(char **)MALLOC(sizeof(char *)*MAX_DIST_FILES))==NULL)
			allocfail(sizeof(char *)*MAX_DIST_FILES);
		for(i=0;i<MAX_DIST_FILES;i++)
			if((file[i]=(char *)MALLOC(MAX_FILELEN))==NULL)
				allocfail(MAX_FILELEN);
		server=NULL;
		f=0;
		s=0;

		memset(dist[r],0,sizeof(struct dist_t));
		sprintf(dist[r]->version,"%s (Local)",VERSION);
		dist[r]->type=LOCAL_FILE;
		dist[r]->servers=server;
		dist[r]->files=file;
		r++;
		strcpy(file[f++],DEFAULT_DISTFILE);
		strcpy(file[f++],str);
	}

	uifc.pop("Getting distributions");
	if((list=ftpGetURL(DIST_LIST_URL1,ftp_user,ftp_pass,&ret1))==NULL
			&& (list=ftpGetURL(DIST_LIST_URL2,ftp_user,ftp_pass,&ret2))==NULL
			&& (list=ftpGetURL(DIST_LIST_URL3,ftp_user,ftp_pass,&ret3))==NULL
			&& (list=ftpGetURL(DIST_LIST_URL4,ftp_user,ftp_pass,&ret4))==NULL
			&& r==0)  {
		uifc.pop(NULL);
		uifc.bail();
		printf("Cannot get distribution list!\n%s\n- %s\n%s\n- %s\n%s\n- %s\n%s\n- %s\n",
				DIST_LIST_URL1,ftpErrString(ret1),
				DIST_LIST_URL2,ftpErrString(ret1),
				DIST_LIST_URL3,ftpErrString(ret1),
				DIST_LIST_URL4,ftpErrString(ret1));
		exit(EXIT_FAILURE);
	}

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

				memset(dist[r],0,sizeof(struct dist_t));
				strcpy(dist[r]->version,in_line+2);
				dist[r]->type=CVS_SERVER;
				dist[r]->servers=server;
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

				memset(dist[r],0,sizeof(struct dist_t));
				strcpy(dist[r]->version,in_line+2);
				dist[r]->type=DIST_SET;
				dist[r]->servers=server;
				dist[r]->files=file;
				r++;
				break;
			case 'f':
				for(j=0,i=2;in_line[i];i++)  {
					if(in_line[i]=='%' && in_line[i+1]=='s')  {
						file[f][j]=0;
						strcat(file[f],sys_desc);
						j=strlen(file[f]);
						i++;
					}
					else
						file[f][j++]=in_line[i];
				}
				f++;
				break;
			case 't':
				strcpy(dist[r-1]->tag,in_line+2);
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
	memset(dist[r],0,sizeof(struct dist_t));
	uifc.pop(NULL);
	return(dist);
}

int choose_dist(char **opts)
{
	int	i;
	static int		dist_dflt=0;

	uifc.helpbuf=	"`Distribution List:`\n"
			"\nToDo: Add help.";

	i=uifc.list(WIN_MID|WIN_ACT,0,0,0,&dist_dflt,0
			,"Select Distribution",opts);
	return(i);
}

int choose_server(char **opts)
{
	int i;
	static int		srvr_dflt=0;

	uifc.helpbuf=	"`Server List:`\n"
				"\nToDo: Add help.";
	i=uifc.list(WIN_MID|WIN_ACT,0,0,0,&srvr_dflt,0
		,"Select Server",opts);
	return(i);

}
/* End of SBBSINST.C */
