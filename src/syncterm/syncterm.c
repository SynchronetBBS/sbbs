/* $Id$ */

#include <sys/stat.h>
#ifdef _WIN32
#include <shlobj.h>
#endif

#include <gen_defs.h>
#include <stdlib.h>
#include <ciolib.h>

#include <ini_file.h>
#include <dirwrap.h>

#include "ciolib.h"
#include "cterm.h"
#include "allfonts.h"

#include "fonts.h"
#include "syncterm.h"
#include "bbslist.h"
#include "conn.h"
#include "term.h"
#include "uifcinit.h"
#include "window.h"

char* syncterm_version = "SyncTERM 0.7"
#ifdef _DEBUG
	" Debug ("__DATE__")"
#endif
#ifdef PCM
	" Clippy Edition"
#endif
	;

char *inpath=NULL;
int default_font=0;
struct syncterm_settings settings;
char *font_names[sizeof(conio_fontdata)/sizeof(struct conio_font_data_struct)];
unsigned char *scrollback_buf=NULL;
unsigned int  scrollback_lines=0;
int	safe_mode=0;
FILE* log_fp;
extern ini_style_t ini_style;

#ifdef _WINSOCKAPI_

static WSADATA WSAData;
#define SOCKLIB_DESC WSAData.szDescription
static BOOL WSAInitialized=FALSE;

static BOOL winsock_startup(void)
{
	int		status;             /* Status Code */

    if((status = WSAStartup(MAKEWORD(1,1), &WSAData))==0) {
		fprintf(stderr,"%s %s",WSAData.szDescription, WSAData.szSystemStatus);
		WSAInitialized=TRUE;
		return (TRUE);
	}

    fprintf(stderr,"!WinSock startup ERROR %d", status);
	return (FALSE);
}

#else /* No WINSOCK */

#define winsock_startup()	(TRUE)
#define SOCKLIB_DESC NULL

#endif

void parse_url(char *url, struct bbslist *bbs, int dflt_conn_type, int force_defaults)
{
	char *p1, *p2, *p3;
	struct	bbslist	*list[MAX_OPTS+1];
	char	listpath[MAX_PATH+1];
	int		listcount=0, i;

	/* User BBS list path */
	get_syncterm_filename(listpath, sizeof(listpath), SYNCTERM_PATH_LIST, FALSE);

	bbs->id=-1;
	bbs->added=time(NULL);
	bbs->calls=0;
	bbs->type=USER_BBSLIST;
	if(force_defaults) {
		bbs->user[0]=0;
		bbs->password[0]=0;
		bbs->reversed=FALSE;
		bbs->screen_mode=SCREEN_MODE_CURRENT;
		bbs->conn_type=dflt_conn_type;
		bbs->port=(dflt_conn_type==CONN_TYPE_TELNET)?23:513;
		bbs->xfer_loglevel=LOG_INFO;
		bbs->telnet_loglevel=LOG_INFO;
		bbs->music=CTERM_MUSIC_BANSI;
		strcpy(bbs->font,"Codepage 437 English");
	}
	p1=url;
	if(!strnicmp("rlogin://",url,9)) {
		bbs->conn_type=CONN_TYPE_RLOGIN;
		bbs->port=513;
		p1=url+9;
	}
	else if(!strnicmp("telnet://",url,9)) {
		bbs->conn_type=CONN_TYPE_TELNET;
		bbs->port=23;
		p1=url+9;
	}
	/* Remove trailing / (Win32 adds one 'cause it hates me) */
	p2=strchr(p1,'/');
	if(p2!=NULL)
		*p2=0;
	p3=strchr(p1,'@');
	if(p3!=NULL) {
		*p3=0;
		p2=strchr(p1,':');
		if(p2!=NULL) {
			*p2=0;
			p2++;
			SAFECOPY(bbs->password,p2);
		}
		SAFECOPY(bbs->user,p1);
		p1=p3+1;
	}
	SAFECOPY(bbs->name,p1);
	p2=strchr(p1,':');
	if(p2!=NULL) {
		*p2=0;
		p2++;
		bbs->port=atoi(p2);
	}
	SAFECOPY(bbs->addr,p1);

	/* Find BBS listing in users phone book */
	if(listpath != NULL) {
		read_list(listpath, &list[0], NULL, &listcount, USER_BBSLIST);
		for(i=0;i<listcount;i++) {
			if((stricmp(bbs->addr,list[i]->addr)==0)
					&& (bbs->port==list[i]->port)
					&& (bbs->conn_type==list[i]->conn_type)
					&& (bbs->user[0]==0 || (stricmp(bbs->name,list[i]->name)==0))
					&& (bbs->password[0]==0 || (stricmp(bbs->password,list[i]->password)==0))) {
				memcpy(bbs,list[i],sizeof(struct bbslist));
				break;
			}
		}
	}
	free_list(&list[0],listcount);
}

char *get_syncterm_filename(char *fn, int fnlen, int type, int shared)
{
	char	oldlst[MAX_PATH+1];

#ifdef _WIN32
	char	*home;
	home=getenv("HOME");
	if(home==NULL)
		home=getenv("USERPROFILE");
	if(home==NULL) {
		strcpy(oldlst,"./syncterm.lst");
	}
	else {
		SAFECOPY(oldlst, home);
		strcat(oldlst, "/syncterm.lst");
	}
#ifdef CSIDL_FLAG_CREATE
	if(type==SYNCTERM_DEFAULT_TRANSFER_PATH) {
		switch(SHGetFolderPath(NULL, CSIDL_PERSONAL|CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, fn)) {
			case E_FAIL:
			case E_INVALIDARG:
				getcwd(fn, fnlen);
				backslash(fn);
				break;
			default:
				backslash(fn);
				strcat(fn,"SyncTERM");
				break;
		}
		if(!isdir(fn))
			MKDIR(fn);
		return(fn);
	}
	switch(SHGetFolderPath(NULL, (shared?CSIDL_COMMON_APPDATA:CSIDL_APPDATA)|CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, fn)) {
		case E_FAIL:
		case E_INVALIDARG:
			strcpy(fn,".");
			break;
		default:
			backslash(fn);
			strcat(fn,"SyncTERM");
			break;
	}
#else
	getcwd(fn, fnlen);
	backslash(fn);
#endif

	/* Create if it doesn't exist */
	if(!isdir(fn)) {
		if(MKDIR(fn))
			fn[0]=0;
	}

	switch(type) {
		case SYNCTERM_PATH_INI:
			backslash(fn);
			strncat(fn,"syncterm.ini",fnlen);
			break;
		case SYNCTERM_PATH_LIST:
			backslash(fn);
			strncat(fn,"syncterm.lst",fnlen);
			break;
	}
#else
	char	*home;
	int		created;

	if(inpath==NULL)
		home=getenv("HOME");
	if(home==NULL) {
		if(type==SYNCTERM_DEFAULT_TRANSFER_PATH) {
			getcwd(fn, fnlen);
			backslash(fn);
			return(fn);
		}
		SAFECOPY(oldlst,"syncterm.lst");
		strcpy(fn,"./");
	}
	else {
		if(type==SYNCTERM_DEFAULT_TRANSFER_PATH) {
			strcpy(fn, home);
			backslash(fn);
			if(!isdir(fn))
				MKDIR(fn);
			return(fn);
		}
		SAFECOPY(oldlst,home);
		backslash(oldlst);
		strcat(oldlst,"syncterm.lst");
		sprintf(fn,"%.*s",fnlen,home);
		strncat(fn, "/.syncterm", fnlen);
		backslash(fn);
	}

	if(shared) {
#ifdef PREFIX
		strcpy(fn,PREFIX);
		backslash(fn);
#else
		strcpy(fn,"/usr/local/etc/");
#endif
	}

	/* Create if it doesn't exist */
	if(!isdir(fn) && !shared) {
		if(MKDIR(fn))
			fn[0]=0;
	}

	switch(type) {
		case SYNCTERM_PATH_INI:
			strncat(fn,"syncterm.ini",fnlen);
			break;
		case SYNCTERM_PATH_LIST:
			strncat(fn,"syncterm.lst",fnlen);
			break;
	}
#endif

	/* Copy pre-0.7 version of the syncterm.lst file to new location */
	if(!shared && type == SYNCTERM_PATH_LIST && (!fexist(fn)) && fexist(oldlst))
		rename(oldlst, fn);
	return(fn);
}

void load_settings(struct syncterm_settings *set)
{
	FILE	*inifile;
	char	inipath[MAX_PATH+1];

	get_syncterm_filename(inipath, sizeof(inipath), SYNCTERM_PATH_INI, FALSE);
	inifile=fopen(inipath,"r");
	set->confirm_close=iniReadBool(inifile,"SyncTERM","ConfirmClose",FALSE);
	set->startup_mode=iniReadInteger(inifile,"SyncTERM","VideoMode",FALSE);
	set->backlines=iniReadInteger(inifile,"SyncTERM","ScrollBackLines",2000);
	if(inifile)
		fclose(inifile);
}

int main(int argc, char **argv)
{
	struct bbslist *bbs=NULL;
	struct	text_info txtinfo;
	char	str[MAX_PATH+1];
	char	drive[MAX_PATH+1];
	char	path[MAX_PATH+1];
	char	fname[MAX_PATH+1];
	char	ext[MAX_PATH+1];
	/* Command-line parsing vars */
	char	url[MAX_PATH+1];
	int		i;
	int	ciolib_mode=CIOLIB_MODE_AUTO;
	str_list_t	inifile;
	FILE *listfile;
	char	listpath[MAX_PATH+1];
	char	*inpath=NULL;
	BOOL	exit_now=FALSE;
	int		conn_type=CONN_TYPE_TELNET;
	BOOL	dont_set_mode=FALSE;

	/* UIFC initialization */
    memset(&uifc,0,sizeof(uifc));
	uifc.mode=UIFC_NOCTRL;
	uifc.size=sizeof(uifc);
	uifc.esc_delay=25;
	url[0]=0;
	for(i=1;i<argc;i++) {
        if(argv[i][0]=='-'
#ifndef __unix__
            || argv[i][0]=='/'
#endif
            )
            switch(toupper(argv[i][1])) {
                case 'E':
                    uifc.esc_delay=atoi(argv[i]+2);
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
                case 'L':
                    uifc.scrn_len=atoi(argv[i]+2);
					dont_set_mode=TRUE;
                    break;
				case 'R':
					conn_type=CONN_TYPE_RLOGIN;
					break;
				case 'T':
					conn_type=CONN_TYPE_TELNET;
					break;
				case 'S':
					safe_mode=1;
					break;
                default:
					goto USAGE;
           }
        else
			SAFECOPY(url,argv[i]);
    }

	load_settings(&settings);

	initciolib(ciolib_mode);
	if(!dont_set_mode) {
		switch(settings.startup_mode) {
			case SCREEN_MODE_80X25:
				textmode(C80);
				break;
			case SCREEN_MODE_80X28:
				textmode(C80X28);
				break;
			case SCREEN_MODE_80X43:
				textmode(C80X43);
				break;
			case SCREEN_MODE_80X50:
				textmode(C80X50);
				break;
			case SCREEN_MODE_80X60:
				textmode(C80X60);
				break;
		}
	}

    gettextinfo(&txtinfo);
	if((txtinfo.screenwidth<40) || txtinfo.screenheight<24) {
		fputs("Window too small, must be at least 80x24\n",stderr);
		return(1);
	}

	if(inpath==NULL) {
		_splitpath(argv[0],drive,path,fname,ext);
		strcat(drive,path);
	}
	else
		FULLPATH(path,inpath,sizeof(path));
	atexit(uifcbail);

	scrollback_buf=malloc(80*2*settings.backlines);	/* Terminal width is *always* 80 cols */
	if(scrollback_buf==NULL) {
		uifc.msg("Cannot allocate space for scrollback buffer.\n");
	}

#ifdef __unix__
	umask(077);
#endif

	/* User BBS list path */
	get_syncterm_filename(listpath, sizeof(listpath), SYNCTERM_PATH_LIST, FALSE);

	/* Auto-connect URL */
	if(url[0]) {
		if((bbs=(struct bbslist *)malloc(sizeof(struct bbslist)))==NULL) {
			uifcmsg("Unable to allocate memory","The system was unable to allocate memory.");
			return(1);
		}
		if((listfile=fopen(listpath,"r"))==NULL)
			parse_url(url, bbs, conn_type, TRUE);
		else {
			read_item(listfile, bbs, NULL, 0, USER_BBSLIST);
			parse_url(url, bbs, conn_type, FALSE);
			fclose(listfile);
		}
		if(bbs->port==0)
			goto USAGE;
	}

	if(!winsock_startup())
		return(1);

	load_font_files();
	while(bbs!=NULL || (bbs=show_bbslist(BBSLIST_SELECT))!=NULL) {
    		gettextinfo(&txtinfo);	/* Current mode may have changed while in show_bbslist() */
		if(!conn_connect(bbs->addr,bbs->port,bbs->reversed?bbs->password:bbs->user,bbs->reversed?bbs->user:bbs->password,bbs->syspass,bbs->conn_type,bbs->bpsrate)) {
			/* ToDo: Update the entry with new lastconnected */
			/* ToDo: Disallow duplicate entries */
			bbs->connected=time(NULL);
			bbs->calls++;
			if(bbs->id != -1) {
				if((listfile=fopen(listpath,"r"))!=NULL) {
					inifile=iniReadFile(listfile);
					fclose(listfile);
					iniSetDateTime(&inifile,bbs->name,"LastConnected",TRUE,bbs->connected,&ini_style);
					iniSetInteger(&inifile,bbs->name,"TotalCalls",bbs->calls,&ini_style);
					if((listfile=fopen(listpath,"w"))!=NULL) {
						iniWriteFile(listfile,inifile);
						fclose(listfile);
					}
					strListFreeStrings(inifile);
				}
			}
			uifcbail();
			load_font_files();
			setfont(find_font_id(bbs->font),TRUE);
			switch(bbs->screen_mode) {
				case SCREEN_MODE_80X25:
					textmode(C80);
					break;
				case SCREEN_MODE_80X28:
					textmode(C80X28);
					break;
				case SCREEN_MODE_80X43:
					textmode(C80X43);
					break;
				case SCREEN_MODE_80X50:
					textmode(C80X50);
					break;
				case SCREEN_MODE_80X60:
					textmode(C80X60);
					break;
			}
			sprintf(str,"SyncTERM - %s",bbs->name);
			settitle(str);
			term.nostatus=bbs->nostatus;
			if(drawwin())
				return(1);
			if(log_fp==NULL && bbs->logfile[0])
				log_fp=fopen(bbs->logfile,"a");
			if(log_fp!=NULL) {
				time_t now=time(NULL);
				fprintf(log_fp,"%.15s Log opened\n", ctime(&now)+4);
			}

			exit_now=doterm(bbs);

			if(log_fp!=NULL) {
				time_t now=time(NULL);
				fprintf(log_fp,"%.15s Log closed\n", ctime(&now)+4);
				fclose(log_fp);
				log_fp=NULL;
			}
			setfont(default_font,TRUE);
			for(i=CONIO_FIRST_FREE_FONT; i<256; i++) {
				FREE_AND_NULL(conio_fontdata[i].eight_by_sixteen);
				FREE_AND_NULL(conio_fontdata[i].eight_by_fourteen);
				FREE_AND_NULL(conio_fontdata[i].eight_by_eight);
				FREE_AND_NULL(conio_fontdata[i].desc);
			}
			load_font_files();
			textmode(txtinfo.currmode);
			settitle("SyncTERM");
		}
		if(exit_now || url[0]) {
			if(bbs != NULL && bbs->id==-1) {
				char	*YesNo[3]={"Yes","No",""};
				/* Started from the command-line with a URL */
				init_uifc(TRUE, TRUE);
				switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,NULL,"Save this BBS in directory?",YesNo)) {
					case 0:	/* Yes */
						add_bbs(listpath,bbs);
						break;
					default: /* ESC/No */
						break;
				}
				free(bbs);
			}
			bbs=NULL;
		}
		else
			bbs=NULL;
	}
	uifcbail();
#ifdef _WINSOCKAPI_
	if(WSAInitialized && WSACleanup()!=0) 
		fprintf(stderr,"!WSACleanup ERROR %d",ERROR_VALUE);
#endif
	return(0);

	USAGE:
	uifcbail();
	clrscr();
    cprintf("\nusage: syncterm [options] [URL]"
        "\n\noptions:\n\n"
        "-e# =  set escape delay to #msec\n"
		"-iX =  set interface mode to X (default=auto) where X is one of:\n"
#ifdef __unix__
		"       X = X11 mode\n"
		"       C = Curses mode\n"
		"       F = Curses mode with forced IBM charset\n"
#else
		"       W = Win32 native mode\n"
#endif
		"       A = ANSI mode\n"
        "-l# =  set screen lines to # (default=auto-detect)\n"
		"-t  =  use telnet mode if URL does not include the scheme\n"
		"-r  =  use rlogin mode if URL does not include the scheme\n"
		"-s  =  enable \"Safe Mode\" which prevents writing/browsing local files\n"
		"\n"
		"URL format is: [(rlogin|telnet)://][user[:password]@]domainname[:port]\n"
		"examples: rlogin://deuce:password@nix.synchro.net:5885\n"
		"          telnet://deuce@nix.synchro.net\n"
		"          nix.synchro.net\n"
		"          telnet://nix.synchro.net\n\nPress any key to exit..."
        );
	getch();
	return(0);
}
