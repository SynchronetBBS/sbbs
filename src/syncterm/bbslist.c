#include <stdio.h>
#include <stdlib.h>

#include <dirwrap.h>
#include <ini_file.h>
#include <uifc.h>

#include "syncterm.h"
#include "bbslist.h"
#include "uifcinit.h"
#include "conn.h"
#include "ciolib.h"
#include "cterm.h"

char *screen_modes[]={"Current", "80x25", "80x28", "80x43", "80x50", "80x60", NULL};
char *log_levels[]={"Emergency", "Alert", "Critical", "Error", "Warning", "Notice", "Info", "Debug", NULL};

char *rate_names[]={"300bps", "600bps", "1200bps", "2400bps", "4800bps", "9600bps", "19.2Kbps", "38.4Kbps", "57.6Kbps", "76.8Kbps", "115.2Kbps", "Unlimited", NULL};
int rates[]={300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 76800, 115200, 0};

char *music_names[]={"ESC [ | only", "BANSI Style", "All ANSI Music enabled"};

ini_style_t ini_style = {
	/* key_len */ 15, 
	/* key_prefix */ "\t", 
	/* section_separator */ "\n", 
	/* value_separarator */NULL, 
	/* bit_separator */ NULL };

int get_rate_num(int rate)
{
	int i;

	for(i=0; rates[i] && (!rate || rate > rates[i]); i++);
	return(i);
}

int get_next_rate(int curr_rate) {
	int i;

	if(curr_rate == 0)
		i=0;
	else
		i=get_rate_num(curr_rate)+1;
	return(rates[i]);
}

void sort_list(struct bbslist **list)  {
	struct bbslist *tmp;
	unsigned int	i,swapped=1;

	while(swapped) {
		swapped=0;
		for(i=1;list[i-1]->name[0] && list[i]->name[0];i++) {
			int	cmp=stricmp(list[i-1]->name,list[i]->name);
			if(cmp>0) {
				tmp=list[i];
				list[i]=list[i-1];
				list[i-1]=tmp;
				swapped=1;
			}
		}
	}
}

void free_list(struct bbslist **list, int listcount)
{
	int i;

	for(i=0;i<listcount;i++) {
		free(list[i]);
	}
}

/*
 * Reads in a BBS list from listpath using *i as the counter into bbslist
 * first BBS read goes into list[i]
 */
void read_list(char *listpath, struct bbslist **list, int *i, int type, char* home)
{
	FILE	*listfile;
	char	*bbsname;
	str_list_t	bbses;
	BOOL	dumb;

	if((listfile=fopen(listpath,"r"))!=NULL) {
		bbses=iniReadSectionList(listfile,NULL);
		while((bbsname=strListPop(&bbses))!=NULL) {
			if((list[*i]=(struct bbslist *)malloc(sizeof(struct bbslist)))==NULL)
				break;
			strcpy(list[*i]->name,bbsname);
			iniReadString(listfile,bbsname,"Address","",list[*i]->addr);
			list[*i]->port=iniReadShortInt(listfile,bbsname,"Port",513);
			list[*i]->added=iniReadDateTime(listfile,bbsname,"Added",0);
			list[*i]->connected=iniReadDateTime(listfile,bbsname,"LastConnected",0);
			list[*i]->calls=iniReadInteger(listfile,bbsname,"TotalCalls",0);
			iniReadString(listfile,bbsname,"UserName","",list[*i]->user);
			iniReadString(listfile,bbsname,"Password","",list[*i]->password);
			iniReadString(listfile,bbsname,"SystemPassword","",list[*i]->syspass);
			list[*i]->conn_type=iniReadEnum(listfile,bbsname,"ConnectionType",conn_types,CONN_TYPE_RLOGIN);
			dumb=iniReadBool(listfile,bbsname,"BeDumb",0);
			if(dumb)
				list[*i]->conn_type=CONN_TYPE_RAW;
			list[*i]->reversed=iniReadBool(listfile,bbsname,"Reversed",0);
			list[*i]->screen_mode=iniReadEnum(listfile,bbsname,"ScreenMode",screen_modes,SCREEN_MODE_CURRENT);
			list[*i]->nostatus=iniReadBool(listfile,bbsname,"NoStatus",0);
			iniReadString(listfile,bbsname,"DownloadPath",home,list[*i]->dldir);
			iniReadString(listfile,bbsname,"UploadPath",home,list[*i]->uldir);
			list[*i]->loglevel=iniReadInteger(listfile,bbsname,"LogLevel",LOG_INFO);
			list[*i]->bpsrate=iniReadInteger(listfile,bbsname,"BPSRate",0);
			list[*i]->music=iniReadInteger(listfile,bbsname,"ANSIMusic",CTERM_MUSIC_BANSI);
			list[*i]->type=type;
			list[*i]->id=*i;
			(*i)++;
		}
		fclose(listfile);
		strListFreeStrings(bbses);
	}

	/* Add terminator */
	list[*i]=(struct bbslist *)"";
}

int edit_list(struct bbslist *item,char *listpath)
{
	char	opt[15][80];
	char	*opts[15];
	int		changed=0;
	int		copt=0,i,j;
	char	str[6];
	FILE *listfile;
	str_list_t	inifile;
	char	tmp[LIST_NAME_MAX+1];

	for(i=0;i<15;i++)
		opts[i]=opt[i];
	if(item->type==SYSTEM_BBSLIST) {
		uifc.helpbuf=	"`Cannot edit system BBS list`\n\n"
						"SyncTERM supports system-wide and per-user lists.  You may only edit entries"
						"in your own personal list.\n";
		uifc.msg("Cannot edit system BBS list");
		return(0);
	}
	opt[15][0]=0;
	if((listfile=fopen(listpath,"r"))!=NULL) {
		inifile=iniReadFile(listfile);
		fclose(listfile);
	}
	else
		return(0);
	for(;;) {
		sprintf(opt[0], "BBS Name          %s",item->name);
		sprintf(opt[1], "Address           %s",item->addr);
		sprintf(opt[2], "Port              %hu",item->port);
		sprintf(opt[3], "Username          %s",item->user);
		sprintf(opt[4], "Password          ********");
		sprintf(opt[5], "System Password   %s",item->syspass[0]?"********":"<none>");
		sprintf(opt[6], "Connection        %s",conn_types[item->conn_type]);
		sprintf(opt[7], "Reversed          %s",item->reversed?"Yes":"No");
		sprintf(opt[8], "Screen Mode       %s",screen_modes[item->screen_mode]);
		sprintf(opt[9], "Hide Status Line  %s",item->nostatus?"Yes":"No");
		sprintf(opt[10], "Download Path     %s",item->dldir);
		sprintf(opt[11],"Upload Path       %s",item->uldir);
		sprintf(opt[12],"Log Level         %s",log_levels[item->loglevel]);
		sprintf(opt[13],"Simulated BPS     %s",rate_names[get_rate_num(item->bpsrate)]);
		sprintf(opt[14],"ANSI Music        %s",music_names[item->music]);
		uifc.changes=0;

		uifc.helpbuf=	"`Edit BBS`\n\n"
						"Select item to edit.";
		switch(uifc.list(WIN_MID|WIN_ACT,0,0,0,&copt,NULL,"Edit Entry",opts)) {
			case -1:
				if((listfile=fopen(listpath,"w"))!=NULL) {
					iniWriteFile(listfile,inifile);
					fclose(listfile);
				}
				strListFreeStrings(inifile);
				return(changed);
			case 0:
				uifc.helpbuf=	"`BBS Name`\n\n"
								"Enter the BBS name as it is to appear in the list.";
				strcpy(tmp,item->name);
				uifc.input(WIN_MID|WIN_SAV,0,0,"BBS Name",item->name,LIST_NAME_MAX,K_EDIT);
				iniRenameSection(&inifile,tmp,item->name);
				break;
			case 1:
				uifc.helpbuf=	"`Address`\n\n"
								"Enter the domain name of the system to connect to ie:\n"
								"nix.synchro.net";
				uifc.input(WIN_MID|WIN_SAV,0,0,"Address",item->addr,LIST_ADDR_MAX,K_EDIT);
				iniSetString(&inifile,item->name,"Address",item->addr,&ini_style);
				break;
			case 2:
				i=item->port;
				sprintf(str,"%hu",item->port?item->port:513);
				uifc.helpbuf=	"`Port`\n\n"
								"Enter the port which the BBS is listening to on the remote system\n"
								"Telnet is generally port 23 and RLogin is generally 513\n";
				uifc.input(WIN_MID|WIN_SAV,0,0,"Port",str,5,K_EDIT|K_NUMBER);
				j=atoi(str);
				if(j<1 || j>65535)
					j=513;
				item->port=j;
				iniSetShortInt(&inifile,item->name,"Port",item->port,&ini_style);
				if(i!=j)
					uifc.changes=1;
				else
					uifc.changes=0;
				break;
			case 3:
				uifc.helpbuf=	"`Username`\n\n"
								"Enter the username to attempt auto-login to the remote with.";
				uifc.input(WIN_MID|WIN_SAV,0,0,"Username",item->user,MAX_USER_LEN,K_EDIT);
				iniSetString(&inifile,item->name,"UserName",item->user,&ini_style);
				break;
			case 4:
				uifc.helpbuf=	"`Password`\n\n"
								"Enter your password for auto-login.";
				uifc.input(WIN_MID|WIN_SAV,0,0,"Password",item->password,MAX_PASSWD_LEN,K_EDIT);
				iniSetString(&inifile,item->name,"Password",item->password,&ini_style);
				break;
			case 5:
				uifc.helpbuf=	"`System Password`\n\n"
								"Enter your System password for auto-login."
								"For non-Synchronet systems, or non-SysOp accounts,"
								"this can be used for simple scripting.";
				uifc.input(WIN_MID|WIN_SAV,0,0,"System Password",item->syspass,MAX_SYSPASS_LEN,K_EDIT);
				iniSetString(&inifile,item->name,"SystemPassword",item->syspass,&ini_style);
				break;
			case 6:
				item->conn_type--;
				uifc.helpbuf=	"`Connection Type`\n\n"
								"Select the type of connection you wish to make:\n"
								"~ RLogin:~ Auto-login with RLogin protocol\n"
								"~ Telnet:~ Use more common Telnet protocol (experimental)\n"
								"~ Raw:   ~ Make a raw socket connection (experimental)\n";
				uifc.list(WIN_SAV,0,0,0,&(item->conn_type),NULL,"Connection Type",&(conn_types[1]));
				item->conn_type++;
				iniSetEnum(&inifile,item->name,"ConnectionType",conn_types,item->conn_type,&ini_style);
				changed=1;
				break;
			case 7:
				item->reversed=!item->reversed;
				changed=1;
				iniSetBool(&inifile,item->name,"Reversed",item->reversed,&ini_style);
				break;
			case 8:
				uifc.helpbuf=	"`Screen Mode`\n\n"
								"Select the screen size for this connection\n";
				uifc.list(WIN_SAV,0,0,0,&(item->screen_mode),NULL,"Screen Mode",screen_modes);
				iniSetEnum(&inifile,item->name,"ScreenMode",screen_modes,item->screen_mode,&ini_style);
				changed=1;
				break;
			case 9:
				item->nostatus=!item->nostatus;
				changed=1;
				iniSetBool(&inifile,item->name,"NoStatus",item->nostatus,&ini_style);
				break;
			case 10:
				uifc.helpbuf=	"`Download Path`\n\n"
								"Enter the path where downloads will be placed.";
				uifc.input(WIN_MID|WIN_SAV,0,0,"Download Path",item->dldir,MAX_PATH,K_EDIT);
				iniSetString(&inifile,item->name,"DownloadPath",item->dldir,&ini_style);
				break;
			case 11:
				uifc.helpbuf=	"`Upload Path`\n\n"
								"Enter the path where uploads will be browsed for.";
				uifc.input(WIN_MID|WIN_SAV,0,0,"Upload Path",item->uldir,MAX_PATH,K_EDIT);
				iniSetString(&inifile,item->name,"UploadPath",item->uldir,&ini_style);
				break;
			case 12:
				uifc.helpbuf=	"`Log Level`\n\n"
								"Set the level of verbosity for file transfer info.\n\n";
				uifc.list(WIN_SAV,0,0,0,&(item->loglevel),NULL,"Log Level",log_levels);
				iniSetInteger(&inifile,item->name,"LogLevel",item->loglevel,&ini_style);
				changed=1;
				break;
			case 13:
				uifc.helpbuf=	"`Simulated BPS Rate`\n\n"
								"Select the rate which recieved characters will be displayed.\n\n"
								"This allows ANSImation to work as intended.";
				i=get_rate_num(item->bpsrate);
				uifc.list(WIN_SAV,0,0,0,&i,NULL,"Simulated BPS Rate",rate_names);
				item->bpsrate=rates[i];
				iniSetInteger(&inifile,item->name,"BPSRate",item->bpsrate,&ini_style);
				changed=1;
				break;
			case 14:
				uifc.helpbuf="`ANSI Music Setup`\n\n"
						"~ ANSI Music Disabled ~ Completely disables ANSI music\n"
						"                      Enables Delete Line\n"
						"~ ESC[N ~               Enables BANSI-Style ANSI music\n"
						"                      Enables Delete Line\n"
						"~ ANSI Music Enabled ~  Enables both ESC[M and ESC[N ANSI music.\n"
						"                      Delete Line is disabled.\n"
						"\n"
						"So-Called ANSI Music has a long and troubled history.  Although the\n"
						"original ANSI standard has well defined ways to provide private\n"
						"extensions to the spec, none of these methods were used.  Instead,\n"
						"so-called ANSI music replaced the Delete Line ANSI sequence.  Many\n"
						"full-screen editors use DL, and to this day, some programs (Such as\n"
						"BitchX) require it to run.\n\n"
						"To deal with this, BananaCom decided to use what *they* though was an\n"
						"unspecified escape code ESC[N for ANSI music.  Unfortunately, this is\n"
						"broken also.  Although rarely implemented in BBS clients, ESC[N is\n"
						"the erase field sequence.\n\n"
						"SyncTERM has now defined a third ANSI music sequence which *IS* legal\n"
						"according to the ANSI spec.  Specifically ESC[|.";
				i=item->music;
				if(uifc.list(WIN_SAV,0,0,0,&i,NULL,"ANSI Music Setup",music_names)!=-1) {
					item->music=i;
					iniSetInteger(&inifile,item->name,"ANSIMusic",item->music,&ini_style);
					changed=1;
				}
				break;
		}
		if(uifc.changes)
			changed=1;
	}
}

void add_bbs(char *listpath, struct bbslist *bbs)
{
	FILE *listfile;
	str_list_t	inifile;

	if((listfile=fopen(listpath,"r"))!=NULL) {
		inifile=iniReadFile(listfile);
		fclose(listfile);
	}
	else {
		inifile=strListInit();
	}
	/* 
	 * Redundant:
	 * iniAddSection(&inifile,bbs->name,NULL);
	 */
	iniSetString(&inifile,bbs->name,"Address",bbs->addr,&ini_style);
	iniSetShortInt(&inifile,bbs->name,"Port",bbs->port,&ini_style);
	iniSetDateTime(&inifile,bbs->name,"Added",/* include time */TRUE,time(NULL),&ini_style);
	iniSetDateTime(&inifile,bbs->name,"LastConnected",/* include time */TRUE,bbs->connected,&ini_style);
	iniSetInteger(&inifile,bbs->name,"TotalCalls",bbs->calls,&ini_style);
	iniSetString(&inifile,bbs->name,"UserName",bbs->user,&ini_style);
	iniSetString(&inifile,bbs->name,"Password",bbs->password,&ini_style);
	iniSetString(&inifile,bbs->name,"SystemPassword",bbs->syspass,&ini_style);
	iniSetEnum(&inifile,bbs->name,"ConnectionType",conn_types,bbs->conn_type,&ini_style);
	iniSetBool(&inifile,bbs->name,"Reversed",bbs->reversed,&ini_style);
	iniSetEnum(&inifile,bbs->name,"ScreenMode",screen_modes,bbs->screen_mode,&ini_style);
	iniSetString(&inifile,bbs->name,"DownloadPath",bbs->dldir,&ini_style);
	iniSetString(&inifile,bbs->name,"UploadPath",bbs->uldir,&ini_style);
	iniSetInteger(&inifile,bbs->name,"LogLevel",bbs->loglevel,&ini_style);
	iniSetInteger(&inifile,bbs->name,"BPSRate",bbs->bpsrate,&ini_style);
	iniSetInteger(&inifile,bbs->name,"ANSIMusic",bbs->music,&ini_style);
	if((listfile=fopen(listpath,"w"))!=NULL) {
		iniWriteFile(listfile,inifile);
		fclose(listfile);
	}
	strListFreeStrings(inifile);
}

void del_bbs(char *listpath, struct bbslist *bbs)
{
	FILE *listfile;
	str_list_t	inifile;

	if((listfile=fopen(listpath,"r"))!=NULL) {
		inifile=iniReadFile(listfile);
		fclose(listfile);
		iniRemoveSection(&inifile,bbs->name);
		if((listfile=fopen(listpath,"w"))!=NULL) {
			iniWriteFile(listfile,inifile);
			fclose(listfile);
		}
		strListFreeStrings(inifile);
	}
}

/*
 * Displays the BBS list and allows edits to user BBS list
 * Mode is one of BBSLIST_SELECT or BBSLIST_EDIT
 */
struct bbslist *show_bbslist(char* listpath, int mode, char *home)
{
	struct	bbslist	*list[MAX_OPTS+1];
	int		i,j;
	int		opt=0,bar=0,oldopt=-1;
	static struct bbslist retlist;
	int		val;
	int		listcount=0;
	char	str[128];
	char	*YesNo[3]={"Yes","No",""};
	char	title[1024];
	char	currtitle[1024];
	char	*p;
	char	addy[LIST_ADDR_MAX+1];

	if(init_uifc(TRUE, TRUE))
		return(NULL);

	read_list(listpath, &list[0], &listcount, USER_BBSLIST, home);

	/* System BBS List */
#ifdef PREFIX
	strcpy(listpath,PREFIX"/etc/syncterm.lst");

	read_list(listpath, list, &listcount, SYSTEM_BBSLIST, home);
#endif
	sort_list(list);

	for(;;) {
		uifc.helpbuf=	"`SyncTERM Dialing Directory`\n\n"
						"Commands:\n\n"
						"~ CTRL-D ~ Quick-dial a URL\n"
						"~ CTRL-E ~ to edit the selected entry\n"
						" ~ ENTER ~ to dial the selected entry";
		if(opt != oldopt) {
			if(list[opt]!=NULL && list[opt]->name[0]) {
				sprintf(title, "%s - %s (%d calls / Last: %s", syncterm_version, (char *)(list[opt]), list[opt]->calls, list[opt]->connected?ctime(&list[opt]->connected):"Never\n");
				p=strrchr(title, '\n');
				if(p!=NULL)
					*p=')';
			}
			else
				strcpy(title,syncterm_version);
			settitle(title);
		}
		oldopt=opt;
		val=uifc.list((listcount<MAX_OPTS?WIN_XTR:0)
			|WIN_ORG|WIN_ACT|WIN_INSACT|WIN_DELACT
			|WIN_MID|WIN_INS|WIN_DEL|WIN_EDIT|WIN_EXTKEYS|WIN_DYN
			,0,0,0,&opt,&bar,mode==BBSLIST_SELECT?"Directory":"Edit",(char **)list);
		if(val==listcount)
			val=listcount|MSK_INS;
		if(val<0) {
			switch(val) {
				case -7:		/* CTRL-E */
					i=list[opt]->id;
					if(edit_list(list[opt],listpath)) {
						sort_list(list);
						for(j=0;list[j]->name[0];j++) {
							if(list[j]->id==i)
								opt=j;
						}
						oldopt=-1;
					}
					break;
				case -6:		/* CTRL-D */
					uifc.changes=0;
					uifc.helpbuf=	"`SyncTERM QuickDial`\n\n"
									"Enter a URL in the format [(rlogin|telnet)://][user[:password]@]domainname[:port]\n";
					uifc.input(WIN_MID|WIN_SAV,0,0,"BBS Address",addy,LIST_ADDR_MAX,0);
					if(uifc.changes) {
						parse_url(addy,&retlist);
						free_list(&list[0],listcount);
						return(&retlist);
					}
					break;
				case -1:		/* ESC */
					free_list(&list[0],listcount);
					return(NULL);
			}
		}
		else if(val&MSK_ON) {
			switch(val&MSK_ON) {
				case MSK_INS:
					if(listcount>=MAX_OPTS) {
						uifc.helpbuf=	"`Max List size reached`\n\n"
										"The total combined size of loaded BBS lists is currently the highest\n"
										"Supported size.  You must delete entries before adding more.";
						uifc.msg("Max List size reached!");
						break;
					}
					listcount++;
					list[listcount]=list[listcount-1];
					list[listcount-1]=(struct bbslist *)malloc(sizeof(struct bbslist));
					memset(list[listcount-1],0,sizeof(struct bbslist));
					list[listcount-1]->loglevel=LOG_INFO;
					list[listcount-1]->id=listcount-1;
					uifc.changes=0;
					uifc.helpbuf=	"`BBS Name`\n\n"
									"Enter the BBS name as it is to appear in the list.";
					uifc.input(WIN_MID|WIN_SAV,0,0,"BBS Name",list[listcount-1]->name,LIST_NAME_MAX,K_EDIT);
					if(uifc.changes) {
						uifc.changes=0;
						uifc.helpbuf=	"`Address`\n\n"
										"Enter the domain name of the system to connect to ie:\n"
										"nix.synchro.net";
						uifc.input(WIN_MID|WIN_SAV,0,0,"Address",list[listcount-1]->addr,LIST_ADDR_MAX,K_EDIT);
					}
					if(!uifc.changes) {
						free(list[listcount-1]);
						list[listcount-1]=list[listcount];
						listcount--;
					}
					else {
						while(!list[listcount-1]->port) {
							list[listcount-1]->port=513;
							sprintf(str,"%hu",list[listcount-1]->port);
							uifc.helpbuf=	"`Port`\n\n"
											"Enter the port which the BBS is listening to on the remote system\n"
											"Telnet is generally port 23 and RLogin is generally 513\n";
							uifc.input(WIN_MID|WIN_SAV,0,0,"Port",str,5,K_EDIT|K_NUMBER);
							j=atoi(str);
							if(j<1 || j>65535)
								j=0;
							list[listcount-1]->port=j;
						}
						uifc.helpbuf=	"`Connection Type`\n\n"
										"Select the type of connection you wish to make:\n"
										"~ RLogin:~ Auto-login with RLogin protocol\n"
										"~ Telnet:~ Use more common Telnet protocol (experimental)\n"
										"~ Raw:   ~ Make a raw socket connection (experimental)\n";
						list[listcount-1]->conn_type=list[listcount-1]->port==513
												?CONN_TYPE_RLOGIN-1
												:(list[listcount-1]->port==23
													?CONN_TYPE_TELNET-1
													:CONN_TYPE_RAW-1);
						uifc.list(WIN_MID|WIN_SAV,0,0,0,&list[listcount-1]->conn_type,NULL,"Connection Type",&conn_types[1]);
						list[listcount-1]->conn_type++;
						uifc.helpbuf=	"`Username`\n\n"
										"Enter the username to attempt auto-login to the remote with.";
						uifc.input(WIN_MID|WIN_SAV,0,0,"User Name",list[listcount-1]->user,MAX_USER_LEN,K_EDIT);
						uifc.helpbuf=	"`Password`\n\n"
										"Enter your password for auto-login.";
						uifc.input(WIN_MID|WIN_SAV,0,0,"Password",list[listcount-1]->password,MAX_PASSWD_LEN,K_EDIT);
						uifc.helpbuf=	"`System Password`\n\n"
										"Enter your System password for auto-login."
										"For non-Synchronet systems, or non-SysOp accounts,"
										"this can be used for simple scripting.";
						uifc.input(WIN_MID|WIN_SAV,0,0,"System Password",list[listcount-1]->syspass,MAX_SYSPASS_LEN,K_EDIT);
						if(list[listcount-1]->conn_type==CONN_TYPE_RLOGIN) {
							uifc.helpbuf=	"`Reversed`\n\n"
											"Select this option if you wish to send the username and password in the wrong\n"
											"order (useful for connecting to v3.11 and lower systems with the default"
											"config)";
							list[listcount-1]->reversed=1;
							uifc.list(WIN_MID|WIN_SAV,0,0,0,&list[listcount-1]->reversed,NULL,"Reversed",YesNo);
							list[listcount-1]->reversed=!list[listcount-1]->reversed;
						}
						uifc.helpbuf=	"`Screen Mode`\n\n"
										"Select the screen size for this connection\n";
						list[listcount-1]->screen_mode=SCREEN_MODE_CURRENT;
						uifc.list(WIN_MID|WIN_SAV,0,0,0,&list[listcount-1]->screen_mode,NULL,"Screen Mode",screen_modes);
						uifc.helpbuf=	"`Hide Status Line`\n\n"
										"Select this option if you wish to hide the status line, effectively adding\n"
										"an extra line to the display (May cause problems with some BBS software)\n";
						list[listcount-1]->nostatus=1;
						uifc.list(WIN_MID|WIN_SAV,0,0,0,&list[listcount-1]->nostatus,NULL,"Hide Status Lines",YesNo);
						list[listcount-1]->nostatus=!list[listcount-1]->nostatus;
						add_bbs(listpath,list[listcount-1]);
						sort_list(list);
						for(j=0;list[j]->name[0];j++) {
							if(list[j]->id==listcount-1)
								opt=j;
						}
						oldopt=-1;
					}
					break;
				case MSK_DEL:
					if(!list[opt]->name[0]) {
						uifc.helpbuf=	"`Calming down`\n\n"
										"~ Some handy tips on calming down ~\n"
										"Close your eyes, imagine yourself alone on a brilliant white beach...\n"
										"Picture the palm trees up towards the small town...\n"
										"Glory in the deep blue of the perfectly clean ocean...\n"
										"Feel the plush comfort of your beach towel...\n"
										"Enjoy the shade of your satellite internet feed which envelops\n"
										"your head, keeping you cool...\n"
										"Set your TEMPEST rated laptop aside on the beach, knowing it's\n"
										"completely impervious to anything on the beach...\n"
										"Reach over to your fridge, grab a cold one...\n"
										"Watch the seagulls in their dance...\n";
						uifc.msg("It's gone, calm down man!");
						break;
					}
					sprintf(str,"Delete %s?",list[opt]->name);
					i=1;
					if(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,NULL,str,YesNo)!=0)
						break;
					del_bbs(listpath,list[opt]);
					free(list[opt]);
					for(i=opt;list[i]->name[0];i++) {
						list[i]=list[i+1];
					}
					for(i=0;list[i]->name[0];i++) {
						list[i]->id=i;
					}
					listcount--;
					oldopt=-1;
					break;
				case MSK_EDIT:
					i=list[opt]->id;
					if(edit_list(list[opt],listpath)) {
						sort_list(list);
						for(j=0;list[j]->name[0];j++) {
							if(list[j]->id==i)
								opt=j;
						}
						oldopt=-1;
					}
					break;
			}
		}
		else {
			if(mode==BBSLIST_EDIT) {
				i=list[opt]->id;
				if(edit_list(list[opt],listpath)) {
					sort_list(list);
					for(j=0;list[j]->name[0];j++) {
						if(list[j]->id==i)
							opt=j;
					}
					oldopt=-1;
				}
			}
			else {
				memcpy(&retlist,list[val],sizeof(struct bbslist));
				free_list(&list[0],listcount);
				return(&retlist);
			}
		}
	}
}
