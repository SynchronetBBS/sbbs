#include <gen_defs.h>
#include <stdlib.h>
#include <ciolib.h>

#include <ini_file.h>
#include <dirwrap.h>

#include "bbslist.h"
#include "conn.h"
#include "uifcinit.h"

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

int main(int argc, char **argv)
{
	struct bbslist *bbs;
	struct	text_info txtinfo;
	char	str[MAX_PATH];
	char	drive[MAX_PATH];
	char	path[MAX_PATH];
	char	fname[MAX_PATH];
	char	ext[MAX_PATH];

	if(!winsock_startup())
		return(1);

    gettextinfo(&txtinfo);
	if((txtinfo.screenwidth<40) || txtinfo.screenheight<24) {
		fputs("Window too small, must be at least 80x24\n",stderr);
		return(1);
	}
	_splitpath(argv[0],drive,path,fname,ext);
	strcat(drive,path);
	FULLPATH(path,drive,sizeof(path));
	atexit(uifcbail);
	while((bbs=show_bbslist(BBSLIST_SELECT,path))!=NULL) {
		if(!conn_connect(bbs->addr,bbs->port,bbs->reversed?bbs->password:bbs->user,bbs->reversed?bbs->user:bbs->password,bbs->conn_type)) {
			/* ToDo: Update the entry with new lastconnected */
			/* ToDo: Disallow duplicate entries */
			str_list_t	inifile;
			FILE *listfile;
			char	listpath[MAX_PATH+1];
			char	*home;

			/* User BBS list */
			home=getenv("HOME");
			if(home==NULL)
				home=getenv("USERPROFILE");
			if(home==NULL)
				strcpy(listpath,path);
			else
				strcpy(listpath,home);
			strncat(listpath,"/syncterm.lst",sizeof(listpath));
			if(strlen(listpath)>MAX_PATH) {
				fprintf(stderr,"Path to syncterm.lst too long");
				return(0);
			}
			bbs->connected=time(NULL);
			bbs->calls++;
			if((listfile=fopen(listpath,"r"))!=NULL) {
				inifile=iniReadFile(listfile);
				fclose(listfile);
				iniSetInteger(&inifile,bbs->name,"LastConnected",bbs->connected,NULL);
				iniSetInteger(&inifile,bbs->name,"TotalCalls",bbs->calls,NULL);
				if((listfile=fopen(listpath,"w"))!=NULL) {
					iniWriteFile(listfile,inifile);
					fclose(listfile);
				}
				strListFreeStrings(inifile);
			}
			uifcbail();
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
			if(drawwin())
				return(1);
			doterm();
			textmode(txtinfo.currmode);
			settitle("SyncTERM");
		}
	}
	uifcbail();
#ifdef _WINSOCKAPI_
	if(WSAInitialized && WSACleanup()!=0) 
		fprintf(stderr,"!WSACleanup ERROR %d",ERROR_VALUE);
#endif
	return(0);
}
