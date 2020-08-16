/* sbbsinst.c */

/* Synchronet installation utility 										*/

/* $Id: sbbsinst.c,v 1.100 2020/03/31 06:48:50 deuce Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
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

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* XPDEV */
#include "gen_defs.h"

#include "md5.h"
#include "ciolib.h"
#include "uifc.h"
#include "sbbs.h"
#include "httpio.h"

/***************/
/* Definitions */
/***************/
#define DEFAULT_CVSROOT		":pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs"
#define DEFAULT_DISTFILE	"sbbs_src.tgz"
#define DEFAULT_LIBFILE		"lib-%s.tgz"	/* MUST HAVE ONE %s for system type (os-machine or just os) */
#define DEFAULT_SYSTYPE		"unix"			/* If no other system type available, use this one */
#define MAX_DISTRIBUTIONS	50
#define	MAX_DIST_FILES		10
#define MAX_SERVERS			100
#define MAX_FILELEN			32
#define MAKE_ERROR			"make failure.\n"
#if defined(__linux__)
#define MAKE				"make"
#else
#define MAKE				"gmake"
#endif

char *distlists[]={
	 "http://cvs.synchro.net/cgi-bin/viewcvs.cgi/*checkout*/src/sbbs3/install/sbbsdist.lst?rev=HEAD&content-type=text/plain"
	 "http://cvs-mirror.synchro.net/cgi-bin/viewcvs.cgi/*checkout*/src/sbbs3/install/sbbsdist.lst?rev=HEAD&content-type=text/plain"
	,NULL	/* terminator */
};

/*******************/
/* DistList Format */
/*******************/

struct server_ent_t {
	char	desc[78];
	char	addr[256];
};

typedef struct {
	char					version[78];
	char					tag[20];
	struct server_ent_t		**servers;
	char					**files;
	int						type;
	char					make_opts[128];
} dist_t;

enum {
	 CVS_SERVER
	,DIST_SET
	,LOCAL_FILE
};

/********************/
/* Global Variables */
/********************/
uifcapi_t uifc; /* User Interface (UIFC) Library API */
uifcapi_t uifc_bak; /* User Interface (UIFC) Library API */

struct {
	char	install_path[256];
	BOOL	usebcc;
	char	cflags[256];
	BOOL	debug;
	BOOL	symlink;
	BOOL	cvs;
	char	cvstag[256];
	char	cvsroot[256];
	char	make_cmdline[128];
	char	sys_desc[1024];
	struct utsname	name;	
	char	sbbsuser[9];		/* Historical UName limit of 8 chars */
	char	sbbsgroup[17];		/* Can't find historical limit for group names */
#ifdef __linux__
	BOOL	use_dosemu;
#endif
} params; /* Build parameters */

#define MAKEFILE "/tmp/SBBSmakefile"

#define CVSCOMMAND "cvs "

char **opt;
char tmp[256];
char error[256];
char cflags[MAX_PATH+1];
char cvsroot[MAX_PATH+1];
char distlist_rev[128]="Unspecified";
char revision[16];

int  backup_level=5;
BOOL keep_makefile=FALSE;
BOOL http_distlist=TRUE;
BOOL http_verbose=FALSE;
BOOL uifcinit=FALSE;

/**************/
/* Prototypes */
/**************/
void install_sbbs(dist_t *, struct server_ent_t *);
dist_t **get_distlist(void);
int choose_dist(char **opts);
int choose_server(char **opts);

int filereadline(int sock, char *buf, size_t length, char *error)
{
	char    ch;
	int             i;

	for(i=0;1;) {
		if(read(sock, &ch,1)!=1)  {
			if(error != NULL)
				strcpy(error,"Error Reading File");
			return(-1);
		}

		if(ch=='\n')
			break;

		if(i<length)
			buf[i++]=ch;
	}

	/* Terminate at length if longer */
	if(i>length)
		i=length;

	if(i>0 && buf[i-1]=='\r')
		buf[--i]=0;
	else
		buf[i]=0;

	return(i);
}

void bail(int code)
{
    if(code) {
        cputs("\nHit a key...");
        getch(); 
	}
	if(uifcinit)
	    uifc.bail();

    exit(code);
}

void allocfail(uint size)
{
    cprintf("\7Error allocating %u bytes of memory.\n",size);
    bail(1);
}

int main(int argc, char **argv)
{
	char**	mopt;
	char*	p;
	int 	i=0;
	int		main_dflt=0;
	char 	str[129];
	dist_t 	**distlist;
	int		dist=0;
	int		server=0;
	int		ciolib_mode=CIOLIB_MODE_AUTO;
	int		tmode=C80;

	/************/
	/* Defaults */
	/************/
	SAFECOPY(params.install_path,"/usr/local/sbbs");
	SAFECOPY(params.make_cmdline,MAKE " install -f install/GNUmakefile");
	params.usebcc=FALSE;
	SAFECOPY(params.cflags,"");
	params.debug=FALSE;
	params.symlink=TRUE;
	params.cvs=TRUE;
	SAFECOPY(params.cvstag,"HEAD");
	SAFECOPY(params.cvsroot,DEFAULT_CVSROOT);
	if((p=getenv("USER"))!=NULL)
		SAFECOPY(params.sbbsuser,p);
	if((p=getenv("GROUP"))!=NULL)
		SAFECOPY(params.sbbsgroup,p);
#ifdef __linux__
	params.use_dosemu=FALSE;
#endif

	sscanf("$Revision: 1.100 $", "%*s %s", revision);
	umask(077);

    printf("\nSynchronet Installation %s-%s\n",revision,PLATFORM_DESC);

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
                case 'L':
                    uifc.scrn_len=atoi(argv[i]+2);
                    break;
                case 'E':
                    uifc.esc_delay=atoi(argv[i]+2);
                    break;
				case 'F':
				case 'H':
					http_verbose=TRUE;
					break;
				case 'N':
					http_distlist=FALSE;
					break;
				case 'I':
					switch(toupper(argv[i][2])) {
						case 'A':
							ciolib_mode=CIOLIB_MODE_ANSI;
							break;
						case 'C':
							ciolib_mode=CIOLIB_MODE_CURSES;
							break;
						case 0:
							printf("NOTICE: The -i option is depreciated, use -if instead\r\n");
							SLEEP(2000);
						case 'F':
							ciolib_mode=CIOLIB_MODE_CURSES_IBM;
							break;
						case 'X':
							ciolib_mode=CIOLIB_MODE_X;
							break;
						case 'W':
							ciolib_mode=CIOLIB_MODE_CONIO;
							break;
						default:
							goto USAGE;
					}
					break;
                case 'V':
					tmode=atoi(argv[i]+2);
                    break;
                default:
					USAGE:
                    printf("\nusage: %s [ctrl_dir] [options]"
                        "\n\noptions:\n\n"
						"-n  =  do not HTTP-download distribution list\n"
						"-h  =  run in HTTP-verbose (debug) mode\n"
                        "-c  =  force color mode\n"
#ifdef USE_CURSES
                        "-e# =  set escape delay to #msec\n"
#endif
						"-iX =  set interface mode to X (default=auto) where X is one of:\r\n"
#ifdef __unix__
						"       X = X11 mode\r\n"
						"       C = Curses mode\r\n"
						"       F = Curses mode with forced IBM charset\r\n"
#else
						"       W = Win32 native mode\r\n"
#endif
						"       A = ANSI mode\r\n"
                        "-v# =  set video mode to #\n"
                        "-l# =  set screen lines to #\n"
						,argv[0]
                        );
        			exit(0);
           }
    }

	i=initciolib(ciolib_mode);
	if(i!=0) {
    	printf("ciolib library init returned error %d\n",i);
    	exit(1);
	}
	textmode(tmode);
	uifc.size=sizeof(uifc);
	memcpy(&uifc_bak,&uifc,sizeof(uifc));
    i=uifcini32(&uifc);  /* curses/conio/X/ANSI */
	if(i!=0) {
		cprintf("uifc library init returned error %d\n",i);
		bail(1);
	}
	uifcinit=TRUE;

	if(uname(&params.name)<0)  {
		SAFECOPY(params.name.machine,"Unknown");
		SAFECOPY(params.name.sysname,params.name.machine);
	}
	sprintf(params.sys_desc,"%s-%s",params.name.sysname,params.name.machine);

	distlist=get_distlist();

	if(distlist==NULL) {
		cprintf("No installation files or distribution list present!\n");
		bail(1);
	}

	if((opt=(char **)malloc(sizeof(char *)*(MAX_OPTS+1)))==NULL)
		allocfail(sizeof(char *)*(MAX_OPTS+1));
	for(i=0;i<(MAX_OPTS+1);i++)
		if((opt[i]=(char *)malloc(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	if((mopt=(char **)malloc(sizeof(char *)*MAX_OPTS))==NULL)
		allocfail(sizeof(char *)*MAX_OPTS);
	for(i=0;i<MAX_OPTS;i++)
		if((mopt[i]=(char *)malloc(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	sprintf(str,"Synchronet Installation %s-%s",revision,PLATFORM_DESC);
	if(uifc.scrn(str)) {
		cprintf(" USCRN (len=%d) failed!\n",uifc.scrn_len+1);
		bail(1);
	}

	while(1) {
		i=0;
		sprintf(mopt[i++],"%-27.27s%s","Distribution",distlist[dist]->version);
		sprintf(mopt[i++],"%-27.27s%s","Server"
			,(distlist[dist]->type==LOCAL_FILE?"Local Files":distlist[dist]->servers[server]->desc));
		sprintf(mopt[i++],"%-27.27s%s","System Type",params.sys_desc);
		sprintf(mopt[i++],"%-27.27s%s","Install Path",params.install_path);
		sprintf(mopt[i++],"%-27.27s%s","Compiler",params.usebcc?"Borland":"GNU");
		sprintf(mopt[i++],"%-27.27s%s","Compiler Flags",params.cflags);
		sprintf(mopt[i++],"%-27.27s%s","Debug Build",params.debug?"Yes":"No");
		sprintf(mopt[i++],"%-27.27s%s","Symlink Binaries",params.symlink?"Yes":"No");
		sprintf(mopt[i++],"%-27.27s%s","Make Command-line",params.make_cmdline);
		sprintf(mopt[i++],"%-27.27s%s","File Owner",params.sbbsuser);
		sprintf(mopt[i++],"%-27.27s%s","File Group",params.sbbsgroup);
#ifdef __linux__
		sprintf(mopt[i++],"%-27.27s%s","Integrate DOSEmu support",params.use_dosemu?"Yes":"No");
#endif
		sprintf(mopt[i++],"%-27.27s","Start Installation...");
		mopt[i][0]=0;

		sprintf(str,"Synchronet Installation - Distribution List %s",distlist_rev);
		uifc.helpbuf=	"`Synchronet Installation:`\n"
						"\nToDo: Add help.";
		switch(uifc.list(WIN_ESC|WIN_MID|WIN_ACT|WIN_ORG,0,0,70,&main_dflt,0
			,str,mopt)) {
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
						server=i;
				}
				break;
			case 2:
				uifc.helpbuf=	"`System Type`\n"
								"\nToDo: Add help.";
				uifc.input(WIN_MID,0,0,"System Type",params.sys_desc,40,K_EDIT);
				break;
			case 3:
				uifc.helpbuf=	"`Install Path`\n"
								"\n"
								"\nPath to install the Synchronet BBS system into."
								"\nSome common paths:"
								"\n /sbbs"
								"\n	/usr/local/sbbs"
								"\n	/opt/sbbs"
								"\n	/home/bbs/sbbs";
				uifc.input(WIN_MID,0,0,"",params.install_path,50,K_EDIT);
				break;
			case 4:
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
			case 5:
				uifc.helpbuf=	"`Compiler Flags`\n"
								"\nToDo: Add help.";
				uifc.input(WIN_MID,0,0,"Additional Compiler Flags",params.cflags,40,K_EDIT);
				break;
			case 6:
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
			case 7:
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
			case 8:
				uifc.helpbuf=	"`Make Command-line`\n"
								"\n";
				uifc.input(WIN_MID,0,0,"",params.make_cmdline,65,K_EDIT);
				break;
			case 9:
				uifc.helpbuf=	"`File Owner`\n"
								"\n";
				uifc.input(WIN_MID,0,0,"",params.sbbsuser,8,K_EDIT);
				break;
			case 10:
				uifc.helpbuf=	"`File Group`\n"
								"\n";
				uifc.input(WIN_MID,0,0,"",params.sbbsgroup,32,K_EDIT);
				break;
#ifdef __linux__
			case 11:
				strcpy(opt[0],"Yes");
				strcpy(opt[1],"No");
				opt[2][0]=0;
				i=params.use_dosemu?0:1;
				uifc.helpbuf=	"`Include DOSEmu Support`\n"
								"\nToDo: Add help.";
				i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0
					,"Integrate DOSEmu support into Synchronet?",opt);
				if(!i)
					params.use_dosemu=TRUE;
				else if(i==1)
					params.use_dosemu=FALSE;
				i=0;
				break;
			case 12:
#else
			case 11:
#endif
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

/* Little wrapper for system calls */
int exec(char* cmd)
{
	int		in_pipe[2];
	int		out_pipe[2];
	int		err_pipe[2];
	int		wait_done;
	pid_t	pid;
	struct	text_info	err_window;
	struct	text_info	out_window;
	int	avail;
	struct timeval timeout;
	char	*buf;
	int		i,retval;

	if(pipe(out_pipe)!=0 || pipe(in_pipe)!=0 || pipe(err_pipe)!=0) {
		cputs("\nError creating I/O pipes");
		bail(1);
	}

	if((pid=FORK())==-1) {
		return(-1);
	}

	if(pid==0) {
		char	*argv[4];
		/* Child Process */
		close(in_pipe[1]);		/* close write-end of pipe */
		dup2(in_pipe[0],0);		/* redirect stdin */
		close(in_pipe[0]);		/* close excess file descriptor */

		close(out_pipe[0]);		/* close read-end of pipe */
		dup2(out_pipe[1],1);	/* stdout */
		close(out_pipe[1]);		/* close excess file descriptor */
		setvbuf(stdout,NULL,_IONBF,0);	/* Make stdout unbuffered */

		close(err_pipe[0]);		/* close read-end of pipe */
		dup2(err_pipe[1],2);	/* stderr */
		close(err_pipe[1]);		/* close excess file descriptor */
		setvbuf(stderr,NULL,_IONBF,0);	/* Make stderr unbuffered */

		argv[0]="/bin/sh";
		argv[1]="-c";
		argv[2]=cmd;
		argv[3]=NULL;
		execvp(argv[0],argv);
		_exit(1);
	}

	close(in_pipe[0]);		/* close excess file descriptor */
	close(out_pipe[1]);		/* close excess file descriptor */
	close(err_pipe[1]);		/* close excess file descriptor */

	i=waitpid(pid, &retval, WNOHANG)!=0;
	wait_done=(i!=0);
	if(i==-1)
		retval=-1;
	gettextinfo(&err_window);
	clrscr();
	gotoxy(1,1);
	cprintf("Running command: %s",cmd);
	gotoxy(1,2);
	clreol();
	textattr(RED<<4|WHITE);
	cputs("Error Output:");
	window(1,3,err_window.screenwidth,7);
	clrscr();
	gotoxy(1,1);
	gettextinfo(&err_window);
	window(1,1,err_window.screenwidth,err_window.screenheight);
	gotoxy(1,8);
	textattr(LIGHTGRAY);
	cputs("Output:");
	window(1,9,err_window.screenwidth,err_window.screenheight);
	clrscr();
	gotoxy(1,1);
	gettextinfo(&out_window);
	_wscroll=1;
	while(!wait_done) {
		fd_set ibits;
		int	high_fd;

		i=waitpid(pid, &retval, WNOHANG)!=0;
		wait_done=(i!=0);
		if(i==-1)
			retval=-1;
		FD_ZERO(&ibits);
		FD_SET(err_pipe[0],&ibits);
		high_fd=err_pipe[0];
		FD_SET(out_pipe[0],&ibits);
		if(out_pipe[0]>err_pipe[0])
			high_fd=out_pipe[0];
		timeout.tv_sec=0;
		timeout.tv_usec=1000;
		if(select(high_fd+1,&ibits,NULL,NULL,&timeout)>0) {
			if(FD_ISSET(err_pipe[0],&ibits)) {
				/* Got some stderr input */
				window(err_window.winleft,err_window.wintop,err_window.winright,err_window.winbottom);
				gotoxy(err_window.curx,err_window.cury);
				textattr(err_window.attribute);
				avail=0;
				if(ioctl(err_pipe[0],FIONREAD,(void *)&avail)!=-1 && avail > 0) {
					int	got=0;
					buf=(char *)malloc(avail+1);
					while(got<avail) {
						i=read(err_pipe[0], buf+got, avail-got);
						if(i>0) {
							got+=i;
						}
						if(i==-1)
							break;
					}
					if(got==avail) {
						buf[avail]=0;
						cputs(buf);
					}
					free(buf);
				}
				gettextinfo(&err_window);
			}
			if(FD_ISSET(out_pipe[0],&ibits)) {
				/* Got some stdout input */
				window(out_window.winleft,out_window.wintop,out_window.winright,out_window.winbottom);
				gotoxy(out_window.curx,out_window.cury);
				textattr(out_window.attribute);
				avail=0;
				if(ioctl(out_pipe[0],FIONREAD,(void *)&avail)!=-1 && avail > 0) {
					int	got=0;
					buf=(char *)malloc(avail+1);
					while(got<avail) {
						i=read(out_pipe[0], buf+got, avail-got);
						if(i>0) {
							got+=i;
						}
						if(i==-1)
							break;
					}
					if(got==avail) {
						buf[avail]=0;
						cputs(buf);
					}
					free(buf);
				}
				gettextinfo(&out_window);
			}
		}
	}

	return(retval);
}

int view_file(char *filename, char *title)
{
	char str[1024];
	int buffile;
	int j;
	char *buf;

	if(fexist(filename)) {
		if((buffile=sopen(filename,O_RDONLY,SH_DENYWR))>=0) {
			j=filelength(buffile);
			if((buf=(char *)malloc(j+1))!=NULL) {
				read(buffile,buf,j);
				close(buffile);
				*(buf+j)=0;
				uifc.showbuf(WIN_MID|WIN_PACK|WIN_FAT,1,1,80,uifc.scrn_len,title,buf,NULL,NULL);
				free(buf);
				return(0);
			}
			close(buffile);
			uifc.msg("Error allocating memory for the file");
			return(3);
		}
		sprintf(str,"Error opening %s",title);
		uifc.msg(str);
		return(1);
	}
	sprintf(str,"%s does not exists",title);
	uifc.msg(str);
	return(2);
}

		/* Some jiggery-pokery here to avoid having to enter the CVS password */
/*		fprintf(makefile,"\tif(grep '%s' -q ~/.cvspass) then echo \"%s A\" >> ~/.cvspass; fi\n",
 *				params.cvsroot,params.cvsroot);
 *
 *		Actually, it looks like you don't NEED to login if the password is blank... huh.
 */

void install_sbbs(dist_t *dist,struct server_ent_t *server)  {
	char	cmd[MAX_PATH+1];
	char	str[1024];
	char	fname[MAX_PATH+1];
	char	dstfname[MAX_PATH+1];
	char	sbbsdir[9+MAX_PATH];
	char	cvstag[7+MAX_PATH];
	char	buf[4096];
	char	url[MAX_PATH+1];
	char	path[MAX_PATH+1];
	char	sbbsuser[128];
	char	sbbsgroup[128];
	int		i;
	int		fout,ret1,ret2;
	size_t	flen;
	long	offset;
	int		remote;
	char	http_error[128];

	if(params.debug)
		putenv("DEBUG=1");
	else
		putenv("RELEASE=1");
	
	if(params.symlink)
		putenv("SYMLINK=1");

	sprintf(sbbsdir,"SBBSDIR=%s",params.install_path);
	putenv(sbbsdir);
	if(params.sbbsuser[0]) {
		sprintf(sbbsuser,"SBBSUSER=%s",params.sbbsuser);
		putenv(sbbsuser);
	}
	if(params.sbbsgroup[0]) {
		sprintf(sbbsgroup,"SBBSGROUP=%s",params.sbbsgroup);
		putenv(sbbsgroup);
	}
#ifdef __linux__
	if(params.use_dosemu==TRUE) {
		putenv("USE_DOSEMU=1");
	}
#endif

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
	uifcinit=FALSE;
	switch (dist->type)  {
		case CVS_SERVER:
			sprintf(cvstag,"CVSTAG=%s",dist->tag);
			putenv(cvstag);
			sprintf(cmd,"cvs -d %s co -r %s install",server->addr,dist->tag);
			if(exec(cmd))  {
				cprintf("Could not checkout install makefile.\n");
				bail(EXIT_FAILURE);
			}
			sprintf(cmd,"%s %s",params.make_cmdline,dist->make_opts);
			if(exec(cmd))  {
				cprintf(MAKE_ERROR);
				bail(EXIT_FAILURE);
			}
			break;
		case DIST_SET:
			for(i=0;dist->files[i][0];i++)  {
				sprintf(fname,dist->files[i],params.sys_desc);
				SAFECOPY(dstfname,fname);
				if((fout=open(fname,O_WRONLY|O_TRUNC|O_CREAT,S_IRUSR|S_IWUSR))<0)  {
					cprintf("Could not download distfile to %s (%d)\n",fname,errno);
					bail(EXIT_FAILURE);
				}
				sprintf(url,"%s%s",server->addr,fname);
				if((remote=http_get_fd(url,&flen,NULL))<0)  {
					/* retry without machine type in name */
					SAFECOPY(str,fname);
					sprintf(fname,dist->files[i],params.name.sysname);
					sprintf(url,"%s%s",server->addr,fname);
					if(stricmp(str,fname)==0	/* no change in name? */
						|| (remote=http_get_fd(url,&flen,NULL))<0)  {
						/* retry using default system-type for system name */
						sprintf(fname,dist->files[i],DEFAULT_SYSTYPE);
						if((remote=http_get_fd(url,&flen,http_error))<0)  {
							cprintf("Cannot get distribution file %s!\n",fname);
							cprintf("%s\n- %s\n",url,http_error);
							close(fout);
							unlink(dstfname);
							bail(EXIT_FAILURE);
						}
					}
				}
				cprintf("Downloading %s           ",url);
				offset=0;
				while((ret1=read(remote,buf,sizeof(buf)))>0)  {
					ret2=write(fout,buf,ret1);
					if(ret2!=ret1)  {
						cprintf("\n!ERROR %d writing to %s\n",errno,dstfname);
						close(fout);
						unlink(dstfname);
						bail(EXIT_FAILURE);
					}
					offset+=ret2;
					if(flen)
						cprintf("\b\b\b\b\b\b\b\b\b\b%3lu%%      ",(long)(((float)offset/(float)flen)*100.0));
					else
						cprintf("\b\b\b\b\b\b\b\b\b\b%10lu",offset);
				}
				cprintf("\n");
				if(ret1<0)  {
					cprintf("!ERROR downloading %s\n",fname);
					close(fout);
					unlink(dstfname);
					bail(EXIT_FAILURE);
				}
				close(fout);
				sprintf(cmd,"gzip -dc %s | tar -xvf -",dstfname);
				if(exec(cmd))  {
					cprintf("Error extracting %s\n",dstfname);
					unlink(dstfname);
					bail(EXIT_FAILURE);
				}
				unlink(dstfname);
			}
			sprintf(cmd,"%s %s",params.make_cmdline,dist->make_opts);
			if(exec(cmd))  {
				cprintf(MAKE_ERROR);
				bail(EXIT_FAILURE);
			}
			break;
		case LOCAL_FILE:
			for(i=0;dist->files[i][0];i++)  {
				sprintf(cmd,"gzip -dc %s/%s | tar -xvf -",path,dist->files[i]);
				if(exec(cmd))  {
					cprintf("Error extracting %s/%s\n",path,dist->files[i]);
					bail(EXIT_FAILURE);
				}
			}
			sprintf(cmd,"%s %s",params.make_cmdline,dist->make_opts);
			if(exec(cmd))  {
				cprintf(MAKE_ERROR);
				bail(EXIT_FAILURE);
			}
			break;
	}

	memcpy(&uifc,&uifc_bak,sizeof(uifc));
    i=uifcini32(&uifc);  /* curses/conio/X/ANSI */
	if(i!=0) {
		cprintf("uifc library init returned error %d\n",i);
		bail(EXIT_FAILURE);
	}
	uifcinit=TRUE;
	sprintf(str,"Synchronet Installation %s-%s",revision,PLATFORM_DESC);
	if(uifc.scrn(str)) {
		cprintf(" USCRN (len=%d) failed!\n",uifc.scrn_len+1);
		bail(EXIT_FAILURE);
	}
	sprintf(cmd,"%s/docs/sbbscon.txt",params.install_path);
	view_file(cmd,"*nix console documentation");
	uifc.bail();
	uifcinit=FALSE;
	cprintf("Synchronet has been successfully installed to:\n\t%s\n",params.install_path);
	cprintf("Documentation files in:\n\t%s/docs\n",params.install_path);
	cprintf("Configuration files in:\n\t%s/ctrl\n",params.install_path);
	cprintf("Executable program files in:\n\t%s/exec\n",params.install_path);
	bail(EXIT_SUCCESS);
}

dist_t **
get_distlist(void)
{
	int i;
	char	in_line[256];
	dist_t	**dist;
	char	**file=NULL;
	struct server_ent_t	**server=NULL;
	int		r=0;
	int		f=0;
	int		s=0;
	char*	p;
	char*	tp;
	int		list=-1;
	char	sep[2]={'\t',0};
	char	str[1024];
	char	errors[sizeof(distlists)/sizeof(char*)][128];
	int     (*readline) (int sock, char *buf, size_t length, char *error)=NULL;

	memset(errors,0,sizeof(errors));
	if((dist=(dist_t **)malloc(sizeof(void *)*MAX_DISTRIBUTIONS))==NULL)
		allocfail(sizeof(void *)*MAX_DISTRIBUTIONS);
	for(i=0;i<MAX_DISTRIBUTIONS;i++)
		if((dist[i]=(void *)malloc(sizeof(dist_t)))==NULL)
			allocfail(sizeof(dist_t));

	sprintf(str,DEFAULT_LIBFILE,params.sys_desc);
	if(!fexistcase(str))	/* use lib-linux.tgz if lib-linux-i686.tgz doesn't exist */
		sprintf(str,DEFAULT_LIBFILE,params.name.sysname);
	if(!fexistcase(str))	/* use lib-unix.tgz if all else fails */
		sprintf(str,DEFAULT_LIBFILE,DEFAULT_SYSTYPE);
	if(fexist(DEFAULT_DISTFILE) && fexistcase(str))  {
		if((file=(char **)malloc(sizeof(char *)*MAX_DIST_FILES))==NULL)
			allocfail(sizeof(char *)*MAX_DIST_FILES);
		for(i=0;i<MAX_DIST_FILES;i++)
			if((file[i]=(char *)malloc(MAX_FILELEN))==NULL)
				allocfail(MAX_FILELEN);
		server=NULL;
		f=0;
		s=0;

		memset(dist[r],0,sizeof(dist_t));
		sprintf(dist[r]->version,"%s (Local)",VERSION);
		dist[r]->type=LOCAL_FILE;
		dist[r]->servers=server;
		dist[r]->files=file;
		strcpy(dist[r]->make_opts,"NOCVS=1");
		r++;
		strcpy(file[f++],DEFAULT_DISTFILE);
		strcpy(file[f++],str);
	}

	if(http_distlist) {
		uifc.pop("Getting distributions");
		for(i=0;distlists[i]!=NULL;i++)  {
			if((list=http_get_fd(distlists[i],NULL,errors[i]))>=0)  {
				readline=sockreadline;
				break;
			}
		}
	}
	if(list<0)  {
		if(http_distlist)
			uifc.pop(NULL);
		uifc.pop("Loading distlist");
		if((list=open("./sbbsdist.lst",O_RDONLY))<0)
			list=open("../sbbsdist.lst",O_RDONLY);
		if(list>=0)
			readline=filereadline;
	}
	if(list<0)  {
		cprintf("Cannot get distribution list!\n");
		for(i=0;distlists[i]!=NULL;i++)
			cprintf("%s\n- %s\n",distlists[i],errors[i]);
		bail(EXIT_FAILURE);
	}

	while(readline != NULL && list>=0 && (readline(list,in_line,sizeof(in_line),NULL)>=0))  {
		i=strlen(in_line);
		while(i>0 && in_line[i]<=' ')
			in_line[i--]=0;

		if(in_line[0]=='D')  {
			strcpy(str,in_line);
			sep[0]=' ';
			p=strtok(str,sep);
			p=strtok(NULL,sep);
			if(strstr(p,params.sys_desc)==NULL)
				break;
			sep[0]='\n';
			p=strtok(NULL,sep);
			strcpy(in_line,p);
		}

		switch(in_line[0])  {
			case 'R':	/* revision */
				sscanf(in_line+2, "%*s %s", distlist_rev);
				break;
			case 'C':
				if(file!=NULL)
					file[f]="";
				if(server!=NULL)
					memset(server[s],0,sizeof(struct server_ent_t));

				file=NULL;

				if((server=(struct server_ent_t **)malloc(sizeof(void *)*MAX_SERVERS))==NULL)
					allocfail(sizeof(struct server_ent_t *)*MAX_SERVERS);
				for(i=0;i<MAX_SERVERS;i++)
					if((server[i]=(struct server_ent_t *)malloc(sizeof(struct server_ent_t)))==NULL)
						allocfail(64);
				f=0;
				s=0;

				memset(dist[r],0,sizeof(dist_t));
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

				if((file=(char **)malloc(sizeof(char *)*MAX_DIST_FILES))==NULL)
					allocfail(sizeof(char *)*MAX_DIST_FILES);
				for(i=0;i<MAX_DIST_FILES;i++)
					if((file[i]=(char *)malloc(MAX_FILELEN))==NULL)
						allocfail(MAX_FILELEN);

				if((server=(struct server_ent_t **)malloc(sizeof(struct server_ent_t *)*MAX_SERVERS))==NULL)
					allocfail(sizeof(struct server_ent_t *)*MAX_SERVERS);
				for(i=0;i<MAX_SERVERS;i++)
					if((server[i]=(struct server_ent_t *)malloc(sizeof(struct server_ent_t)))==NULL)
						allocfail(64);
				f=0;
				s=0;

				memset(dist[r],0,sizeof(dist_t));
				strcpy(dist[r]->version,in_line+2);
				dist[r]->type=DIST_SET;
				dist[r]->servers=server;
				dist[r]->files=file;
				r++;
				break;
			case 'f':
				strcpy(file[f++],in_line+2);
				break;
			case 't':
				SAFECOPY(dist[r-1]->tag,in_line+2);
				break;
			case 'm':
				SAFECOPY(dist[r-1]->make_opts,in_line+2);
				break;
			case 's':
				p=in_line+2;
				tp=p;
				while(*tp && *tp>' ') tp++;
					*tp=0;	/* truncate address at first whitespace */
				if(!strncasecmp(p,"ftp://",6))
					break;
				SAFECOPY(server[s]->addr,p);
				p=tp+1;
				while(*p && *p<=' ') p++;	/* desc follows whitepsace */
				SAFECOPY(server[s]->desc,p);
				s++;
				break;
		}
	}
	memset(dist[r],0,sizeof(dist_t));
	uifc.pop(NULL);
	if(list>=0)
		close(list);
	if(r<1)
		return(NULL);
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
