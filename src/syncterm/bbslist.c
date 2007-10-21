#include <stdio.h>
#include <stdlib.h>

#include <dirwrap.h>
#include <ini_file.h>
#include <uifc.h>
#include "filepick.h"
#include "allfonts.h"

#include "syncterm.h"
#include "fonts.h"
#include "bbslist.h"
#include "uifcinit.h"
#include "conn.h"
#include "ciolib.h"
#include "keys.h"
#include "mouse.h"
#include "cterm.h"
#include "window.h"
#include "term.h"

char *screen_modes[]={"Current", "80x25", "80x28", "80x43", "80x50", "80x60", "C64", "C128 (40col)", "C128 (80col)", "Atari", NULL};
char *log_levels[]={"Emergency", "Alert", "Critical", "Error", "Warning", "Notice", "Info", "Debug", NULL};
char *log_level_desc[]={"None", "Alerts", "Critical Errors", "Errors", "Warnings", "Notices", "Normal", "All (Debug)", NULL};

char *rate_names[]={"300bps", "600bps", "1200bps", "2400bps", "4800bps", "9600bps", "19.2Kbps", "38.4Kbps", "57.6Kbps", "76.8Kbps", "115.2Kbps", "Unlimited", NULL};
int rates[]={300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 76800, 115200, 0};

char *music_names[]={"ESC [ | only", "BANSI Style", "All ANSI Music enabled", NULL};

ini_style_t ini_style = {
	/* key_len */ 15, 
	/* key_prefix */ "\t", 
	/* section_separator */ "\n", 
	/* value_separarator */NULL, 
	/* bit_separator */ NULL };

void viewofflinescroll(void)
{
	int	top;
	int key;
	int i;
	char	*scrnbuf;
	struct	text_info txtinfo;
	int	x,y;
	struct mouse_event mevent;

	x=wherex();
	y=wherey();
    gettextinfo(&txtinfo);
	scrnbuf=(char *)alloca(txtinfo.screenheight*txtinfo.screenwidth*2);
	gettext(1,1,txtinfo.screenwidth,txtinfo.screenheight,scrnbuf);
	uifcbail();
	drawwin();
	top=scrollback_lines;
	gotoxy(1,1);
	textattr(uifc.hclr|(uifc.bclr<<4)|BLINK);
	for(i=0;!i;) {
		if(top<1)
			top=1;
		if(top>(int)scrollback_lines)
			top=scrollback_lines;
		puttext(((txtinfo.screenwidth-80)/2)+1,1,(txtinfo.screenwidth-80)/2+80,txtinfo.screenheight,scrollback_buf+(80*2*top));
		cputs("Scrollback");
		gotoxy(71,1);
		cputs("Scrollback");
		gotoxy(1,1);
		key=getch();
		switch(key) {
			case 0xff:
			case 0:
				switch(key|getch()<<8) {
					case CIO_KEY_MOUSE:
						getmouse(&mevent);
						switch(mevent.event) {
							case CIOLIB_BUTTON_1_DRAG_START:
								mousedrag(scrollback_buf);
								break;
						}
						break;
					case CIO_KEY_UP:
						top--;
						break;
					case CIO_KEY_DOWN:
						top++;
						break;
					case CIO_KEY_PPAGE:
						top-=txtinfo.screenheight;
						break;
					case CIO_KEY_NPAGE:
						top+=txtinfo.screenheight;
						break;
					case CIO_KEY_F(1):
						init_uifc(FALSE, FALSE);
						uifc.helpbuf=	"`Scrollback Buffer`\n\n"
										"~ J ~ or ~ Up Arrow ~   Scrolls up one line\n"
										"~ K ~ or ~ Down Arrow ~ Scrolls down one line\n"
										"~ H ~ or ~ Page Up ~    Scrolls up one screen\n"
										"~ L ~ or ~ Page Down ~  Scrolls down one screen\n";
						uifc.showhelp();
						uifcbail();
						drawwin();
						break;
				}
				break;
			case 'j':
			case 'J':
				top--;
				break;
			case 'k':
			case 'K':
				top++;
				break;
			case 'h':
			case 'H':
				top-=txtinfo.screenheight;
				break;
			case 'l':
			case 'L':
				top+=txtinfo.screenheight;
				break;
			case ESC:
				i=1;
				break;
		}
	}
	init_uifc(TRUE, TRUE);
	puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,scrnbuf);
	gotoxy(x,y);
	return;
}

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

void sort_list(struct bbslist **list, int *listcount)  {
	struct bbslist *tmp;
	unsigned int	i,j,swapped=1;

	while(swapped) {
		swapped=0;
		for(i=1;list[i]!=NULL && list[i-1]->name[0] && list[i]->name[0];i++) {
			int	cmp=stricmp(list[i-1]->name,list[i]->name);
			if(cmp>0 || (cmp==0 && list[i-1]->type==SYSTEM_BBSLIST && list[i]->type==USER_BBSLIST)) {
				tmp=list[i];
				list[i]=list[i-1];
				list[i-1]=tmp;
				swapped=1;
			}
			if(cmp==0) {
				/* Duplicate.  Remove the one from system BBS list */
				tmp=list[i];
				for(j=i;list[j]!=NULL && list[j]->name[0];j++) {
					list[j]=list[j+1];
				}
				free(tmp);
				for(j=0;list[j]!=NULL && list[j]->name[0];j++) {
					list[j]->id=j;
				}
				(*listcount)--;
				swapped=1;
				break;
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

void read_item(FILE *listfile, struct bbslist *entry, char *bbsname, int id, int type)
{
	BOOL	dumb;
	char	home[MAX_PATH+1];

	get_syncterm_filename(home, sizeof(home), SYNCTERM_DEFAULT_TRANSFER_PATH, FALSE);
	if(bbsname != NULL)
		SAFECOPY(entry->name,bbsname);
	iniReadString(listfile,bbsname,"Address","",entry->addr);
	entry->conn_type=iniReadEnum(listfile,bbsname,"ConnectionType",conn_types,CONN_TYPE_RLOGIN);
	entry->port=iniReadShortInt(listfile,bbsname,"Port",conn_ports[entry->conn_type]);
	entry->added=iniReadDateTime(listfile,bbsname,"Added",0);
	entry->connected=iniReadDateTime(listfile,bbsname,"LastConnected",0);
	entry->calls=iniReadInteger(listfile,bbsname,"TotalCalls",0);
	iniReadString(listfile,bbsname,"UserName","",entry->user);
	iniReadString(listfile,bbsname,"Password","",entry->password);
	iniReadString(listfile,bbsname,"SystemPassword","",entry->syspass);
	dumb=iniReadBool(listfile,bbsname,"BeDumb",0);
	if(dumb)
		entry->conn_type=CONN_TYPE_RAW;
	entry->reversed=iniReadBool(listfile,bbsname,"Reversed",0);
	entry->screen_mode=iniReadEnum(listfile,bbsname,"ScreenMode",screen_modes,SCREEN_MODE_CURRENT);
	entry->nostatus=iniReadBool(listfile,bbsname,"NoStatus",0);
	iniReadString(listfile,bbsname,"DownloadPath",home,entry->dldir);
	iniReadString(listfile,bbsname,"UploadPath",home,entry->uldir);

	/* Log Stuff */
	iniReadString(listfile,bbsname,"LogFile","",entry->logfile);
	entry->xfer_loglevel=iniReadEnum(listfile,bbsname,"TransferLogLevel",log_levels,LOG_INFO);
	entry->telnet_loglevel=iniReadEnum(listfile,bbsname,"TelnetLogLevel",log_levels,LOG_INFO);

	entry->bpsrate=iniReadInteger(listfile,bbsname,"BPSRate",0);
	entry->music=iniReadInteger(listfile,bbsname,"ANSIMusic",CTERM_MUSIC_BANSI);
	iniReadString(listfile,bbsname,"Font","Codepage 437 English",entry->font);
	entry->type=type;
	entry->id=id;
}

/*
 * Reads in a BBS list from listpath using *i as the counter into bbslist
 * first BBS read goes into list[i]
 */
void read_list(char *listpath, struct bbslist **list, struct bbslist *defaults, int *i, int type)
{
	FILE	*listfile;
	char	*bbsname;
	str_list_t	bbses;

	if((listfile=fopen(listpath,"r"))!=NULL) {
		if(defaults != NULL)
			read_item(listfile,defaults,NULL,-1,type);
		bbses=iniReadSectionList(listfile,NULL);
		while((bbsname=strListPop(&bbses))!=NULL) {
			if((list[*i]=(struct bbslist *)malloc(sizeof(struct bbslist)))==NULL)
				break;
			read_item(listfile,list[*i],bbsname,*i,type);
			(*i)++;
		}
		fclose(listfile);
		strListFreeStrings(bbses);
	}
	else {
		if(defaults != NULL && type==USER_BBSLIST)
			read_item(listfile,defaults,NULL,-1,type);
	}

#if 0	/* This isn't necessary (NULL is a sufficient) */
	/* Add terminator */
	list[*i]=(struct bbslist *)"";
#endif
}

int edit_list(struct bbslist *item,char *listpath,int isdefault)
{
	char	opt[18][80];	/* <- Beware of magic number! */
	char	*opts[19];		/* <- Beware of magic number! */
	int		changed=0;
	int		copt=0,i,j;
	int		bar=0;
	char	str[6];
	FILE *listfile;
	str_list_t	inifile;
	char	tmp[LIST_NAME_MAX+1];
	char	*itemname;

	for(i=0;i<18;i++)		/* <- Beware of magic number! */
		opts[i]=opt[i];
	if(item->type==SYSTEM_BBSLIST) {
		char	*YesNo[3]={"Yes","No",""};
		uifc.helpbuf=	"`Copy from system BBS list`\n\n"
						"This BBS was loaded from the system BBS list.  In order to edit it, it\n"
						"must be copied into your personal BBS list.\n";
		if(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,NULL,"Copy from system BBS list?",YesNo)!=0)
			return(0);
		item->type=USER_BBSLIST;
		add_bbs(listpath, item);
	}
	if((listfile=fopen(listpath,"r"))!=NULL) {
		inifile=iniReadFile(listfile);
		fclose(listfile);
	}
	else {
		inifile=strListInit();
	}

	if(isdefault)
		itemname=NULL;
	else
		itemname=item->name;
	for(;;) {
		i=0;
		if(!isdefault) {
			sprintf(opt[i++], "BBS Name          %s",itemname);
			sprintf(opt[i++], "Address           %s",item->addr);
		}
		sprintf(opt[i++], "Port              %hu",item->port);
		sprintf(opt[i++], "Username          %s",item->user);
		sprintf(opt[i++], "Password          ********");
		sprintf(opt[i++], "System Password   %s",item->syspass[0]?"********":"<none>");
		sprintf(opt[i++], "Connection        %s",conn_types[item->conn_type]);
		sprintf(opt[i++], "Reversed          %s",item->reversed?"Yes":"No");
		sprintf(opt[i++], "Screen Mode       %s",screen_modes[item->screen_mode]);
		sprintf(opt[i++], "Hide Status Line  %s",item->nostatus?"Yes":"No");
		sprintf(opt[i++], "Download Path     %s",item->dldir);
		sprintf(opt[i++], "Upload Path       %s",item->uldir);
		sprintf(opt[i++], "Log File          %s",item->logfile);
		sprintf(opt[i++], "Log Transfers     %s",log_level_desc[item->xfer_loglevel]);
		sprintf(opt[i++], "Log Telnet Cmds   %s",log_level_desc[item->telnet_loglevel]);
		sprintf(opt[i++], "Simulated BPS     %s",rate_names[get_rate_num(item->bpsrate)]);
		sprintf(opt[i++], "ANSI Music        %s",music_names[item->music]);
		sprintf(opt[i++], "Font              %s",item->font);
		opts[i]=NULL;
		uifc.changes=0;

		uifc.helpbuf=	"`Edit BBS`\n\n"
						"Select item to edit.\n\n"
						"~ Reversed ~\n"
						"        For RLogin connections, send the username and password in\n"
						"        reverse order. Some Synchronet systems are set up this way.\n\n"
						"~ Hide Status Line ~\n"
						"        Selects if the status line should be hidden, giving an extra\n"
						"        display row\n\n"
						"~ Log Transfers ~\n"
						"        Cycles through the various transfer log settings.\n\n"
						"~ Log Telnet Cmds ~\n"
						"        Cycles through the various telnet command log settings.\n\n"
						;
		i=uifc.list(WIN_MID|WIN_SAV|WIN_ACT,0,0,0,&copt,&bar,"Edit Entry",opts);
		if(i>=0 && isdefault)
			i+=2;
		switch(i) {
			case -1:
				if(!safe_mode) {
					if((listfile=fopen(listpath,"w"))!=NULL) {
						iniWriteFile(listfile,inifile);
						fclose(listfile);
					}
				}
				strListFreeStrings(inifile);
				return(changed);
			case 0:
				uifc.helpbuf=	"`BBS Name`\n\n"
								"Enter the BBS name as it is to appear in the list.";
				strcpy(tmp,itemname);
				uifc.input(WIN_MID|WIN_SAV,0,0,"BBS Name",itemname,LIST_NAME_MAX,K_EDIT);
				iniRenameSection(&inifile,tmp,itemname);
				break;
			case 1:
				uifc.helpbuf=	"`Address`\n\n"
								"Enter the domain name of the system to connect to ie:\n"
								"nix.synchro.net";
				uifc.input(WIN_MID|WIN_SAV,0,0,"Address",item->addr,LIST_ADDR_MAX,K_EDIT);
				iniSetString(&inifile,itemname,"Address",item->addr,&ini_style);
				break;
			case 2:
				i=item->port;
				sprintf(str,"%hu",item->port);
				uifc.helpbuf=	"`Port`\n\n"
								"Enter the port which the BBS is listening to on the remote system\n"
								"Telnet is generally port 23, RLogin is generally 513 and SSH is\n"
								"generally 22\n";
				uifc.input(WIN_MID|WIN_SAV,0,0,"Port",str,5,K_EDIT|K_NUMBER);
				j=atoi(str);
				if(j<1 || j>65535)
					j=conn_ports[item->conn_type];
				item->port=j;
				iniSetShortInt(&inifile,itemname,"Port",item->port,&ini_style);
				if(i!=j)
					uifc.changes=1;
				else
					uifc.changes=0;
				break;
			case 3:
				uifc.helpbuf=	"`Username`\n\n"
								"Enter the username to attempt auto-login to the remote with.\n"
								"For SSH, this must be the SSH user name.";
				uifc.input(WIN_MID|WIN_SAV,0,0,"Username",item->user,MAX_USER_LEN,K_EDIT);
				iniSetString(&inifile,itemname,"UserName",item->user,&ini_style);
				break;
			case 4:
				uifc.helpbuf=	"`Password`\n\n"
								"Enter your password for auto-login.\n"
								"For SSH, this must be the SSH password if it exists.\n";
				uifc.input(WIN_MID|WIN_SAV,0,0,"Password",item->password,MAX_PASSWD_LEN,K_EDIT);
				iniSetString(&inifile,itemname,"Password",item->password,&ini_style);
				break;
			case 5:
				uifc.helpbuf=	"`System Password`\n\n"
								"Enter your System password for auto-login.\n"
								"This password is sent after the username and password, so for non-\n"
								"Synchronet, or non-sysop accounts, this can be used for simple\n"
								"scripting.";
				uifc.input(WIN_MID|WIN_SAV,0,0,"System Password",item->syspass,MAX_SYSPASS_LEN,K_EDIT);
				iniSetString(&inifile,itemname,"SystemPassword",item->syspass,&ini_style);
				break;
			case 6:
				i=item->conn_type;
				item->conn_type--;
				uifc.helpbuf=	"`Connection Type`\n\n"
								"Select the type of connection you wish to make:\n"
								"~ RLogin:~ Auto-login with RLogin protocol\n"
								"~ Telnet:~ Use more common Telnet protocol\n"
								"~ Raw:   ~ Make a raw socket connection\n"
								"~ SSH:   ~ Connect using the SSH protocol\n"
								"~ Modem: ~ Connect using a modem\n"
#ifdef __unix__
								"~ Shell: ~ Connect to a local PTY\n";
#else
								;
#endif
				switch(uifc.list(WIN_SAV,0,0,0,&(item->conn_type),NULL,"Connection Type",&(conn_types[1]))) {
					case -1:
						item->conn_type=i;
						break;
					default:
						item->conn_type++;
						iniSetEnum(&inifile,itemname,"ConnectionType",conn_types,item->conn_type,&ini_style);

						/* Set the port too */
						j=conn_ports[item->conn_type];
						if(j<1 || j>65535)
							j=item->port;
						item->port=j;
						iniSetShortInt(&inifile,itemname,"Port",item->port,&ini_style);

						changed=1;
						break;
				}
				break;
			case 7:
				item->reversed=!item->reversed;
				changed=1;
				iniSetBool(&inifile,itemname,"Reversed",item->reversed,&ini_style);
				break;
			case 8:
				i=item->screen_mode;
				uifc.helpbuf=	"`Screen Mode`\n\n"
								"Select the screen size for this connection\n";
				switch(uifc.list(WIN_SAV,0,0,0,&(item->screen_mode),NULL,"Screen Mode",screen_modes)) {
					case -1:
						item->screen_mode=i;
						break;
					default:
						iniSetEnum(&inifile,itemname,"ScreenMode",screen_modes,item->screen_mode,&ini_style);
						if(item->screen_mode == SCREEN_MODE_C64) {
							strcpy(item->font,font_names[33]);
							iniSetString(&inifile,itemname,"Font",item->font,&ini_style);
							item->nostatus = 1;
							iniSetBool(&inifile,itemname,"NoStatus",item->nostatus,&ini_style);
						}
						if(item->screen_mode == SCREEN_MODE_C128_40
								|| item->screen_mode == SCREEN_MODE_C128_80) {
							strcpy(item->font,font_names[35]);
							iniSetString(&inifile,itemname,"Font",item->font,&ini_style);
							item->nostatus = 1;
							iniSetBool(&inifile,itemname,"NoStatus",item->nostatus,&ini_style);
						}
						if(item->screen_mode == SCREEN_MODE_ATARI) {
							strcpy(item->font,font_names[36]);
							iniSetString(&inifile,itemname,"Font",item->font,&ini_style);
							item->nostatus = 1;
							iniSetBool(&inifile,itemname,"NoStatus",item->nostatus,&ini_style);
						}
						changed=1;
						break;
				}
				break;
			case 9:
				item->nostatus=!item->nostatus;
				changed=1;
				iniSetBool(&inifile,itemname,"NoStatus",item->nostatus,&ini_style);
				break;
			case 10:
				uifc.helpbuf=	"`Download Path`\n\n"
								"Enter the path where downloads will be placed.";
				if(uifc.input(WIN_MID|WIN_SAV,0,0,"Download Path",item->dldir,MAX_PATH,K_EDIT)>=0)
					iniSetString(&inifile,itemname,"DownloadPath",item->dldir,&ini_style);
				break;
			case 11:
				uifc.helpbuf=	"`Upload Path`\n\n"
								"Enter the path where uploads will be browsed from.";
				if(uifc.input(WIN_MID|WIN_SAV,0,0,"Upload Path",item->uldir,MAX_PATH,K_EDIT)>=0)
					iniSetString(&inifile,itemname,"UploadPath",item->uldir,&ini_style);
				break;
			case 12:
				uifc.helpbuf=	"`Log Filename`\n\n"
								"Enter the path to the optional log file.";
				if(uifc.input(WIN_MID|WIN_SAV,0,0,"Log File",item->logfile,MAX_PATH,K_EDIT)>=0)
					iniSetString(&inifile,itemname,"LogFile",item->logfile,&ini_style);
				break;
			case 13:
				item->xfer_loglevel--;
				if(item->xfer_loglevel<0)
					item->xfer_loglevel=LOG_DEBUG;
				else if(item->xfer_loglevel<LOG_ERR)
					item->xfer_loglevel=0;
				iniSetEnum(&inifile,itemname,"TransferLogLevel",log_levels,item->xfer_loglevel,&ini_style);
				changed=1;
				break;
			case 14:
				item->telnet_loglevel--;
				if(item->telnet_loglevel<0)
					item->telnet_loglevel=LOG_DEBUG;
				else if(item->telnet_loglevel<LOG_ERR)
					item->telnet_loglevel=0;
				iniSetEnum(&inifile,itemname,"TelnetLogLevel",log_levels,item->telnet_loglevel,&ini_style);
				changed=1;
				break;
			case 15:
				uifc.helpbuf=	"`Simulated BPS Rate`\n\n"
								"Select the rate which recieved characters will be displayed.\n\n"
								"This allows ANSImation to work as intended.";
				i=get_rate_num(item->bpsrate);
				switch(uifc.list(WIN_SAV,0,0,0,&i,NULL,"Simulated BPS Rate",rate_names)) {
					case -1:
						break;
					default:
						item->bpsrate=rates[i];
						iniSetInteger(&inifile,itemname,"BPSRate",item->bpsrate,&ini_style);
						changed=1;
				}
				break;
			case 16:
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
					iniSetInteger(&inifile,itemname,"ANSIMusic",item->music,&ini_style);
					changed=1;
				}
				break;
			case 17:
				uifc.helpbuf=	"`Font`\n\n"
								"Select the desired font for this connection.\n\n"
								"Some fonts do not allow some modes.  When this is the case, an\n"
								"appropriate mode will be forced.\n";
				i=j=find_font_id(item->font);
				switch(uifc.list(WIN_SAV,0,0,0,&i,&j,"Font",font_names)) {
					case -1:
						break;
					default:
					if(i!=find_font_id(item->font)) {
						strcpy(item->font,font_names[i]);
						iniSetString(&inifile,itemname,"Font",item->font,&ini_style);
						changed=1;
					}
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

	if(safe_mode)
		return;
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
	iniSetString(&inifile,bbs->name,"LogFile",bbs->logfile,&ini_style);
	iniSetEnum(&inifile,bbs->name,"TransferLogLevel",log_levels,bbs->xfer_loglevel,&ini_style);
	iniSetEnum(&inifile,bbs->name,"TelnetLogLevel",log_levels,bbs->telnet_loglevel,&ini_style);
	iniSetInteger(&inifile,bbs->name,"BPSRate",bbs->bpsrate,&ini_style);
	iniSetInteger(&inifile,bbs->name,"ANSIMusic",bbs->music,&ini_style);
	iniSetString(&inifile,bbs->name,"Font",bbs->font,&ini_style);
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

	if(safe_mode)
		return;
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

void change_settings(void)
{
	char	inipath[MAX_PATH+1];
	FILE	*inifile;
	str_list_t	inicontents;
	char	opts[7][80];
	char	*opt[7];
	int		i,j;
	char	str[64];
	int	cur=0;

	get_syncterm_filename(inipath, sizeof(inipath), SYNCTERM_PATH_INI, FALSE);
	if((inifile=fopen(inipath,"r"))!=NULL) {
		inicontents=iniReadFile(inifile);
		fclose(inifile);
	}
	else {
		inicontents=strListInit();
	}

	for(i=0; i<7; i++)
		opt[i]=opts[i];

	opts[6][0]=0;
	for(;;) {
		uifc.helpbuf=	"`Program Settings Menu`\n\n"
						"~ Confirm Program Exit ~\n"
						"        Prompt the user before exiting.\n\n"
						"~ Startup Video Mode ~\n"
						"        Set the initial video screen size.\n\n"
						"~ Output Mode ~\n"
						"        Set video output mode.\n\n"
						"~ Scrollback Buffer Lines ~\n"
						"        The number of lines in the scrollback buffer.\n\n"
						"~ Modem Device ~\n"
						"        The device name of the modem.\n\n";
						"~ Modem Init String ~\n"
						"        An init string to use for the modem.\n\n";
		sprintf(opts[0],"Confirm Program Exit    %s",settings.confirm_close?"Yes":"No");
		sprintf(opts[1],"Startup Video Mode      %s",screen_modes[settings.startup_mode]);
		sprintf(opts[2],"Output Mode             %s",output_descrs[settings.output_mode]);
		sprintf(opts[3],"Scrollback Buffer Lines %d",settings.backlines);
		sprintf(opts[4],"Modem Device            %s",settings.mdm.device_name);
		sprintf(opts[5],"Modem Init String       %s",settings.mdm.init_string);
		switch(uifc.list(WIN_ACT|WIN_MID|WIN_SAV,0,0,0,&cur,NULL,"Program Settings",opt)) {
			case -1:
				goto write_ini;
			case 0:
				settings.confirm_close=!settings.confirm_close;
				iniSetBool(&inicontents,"SyncTERM","ConfirmClose",settings.confirm_close,&ini_style);
				break;
			case 1:
				j=settings.startup_mode;
				uifc.helpbuf=	"`Startup Video Mode`\n\n"
								"Select the screen size for at startup\n";
				switch(i=uifc.list(WIN_SAV,0,0,0,&j,NULL,"Startup Video Mode",screen_modes)) {
					case -1:
						continue;
					default:
						settings.startup_mode=j;
						iniSetInteger(&inicontents,"SyncTERM","VideoMode",settings.startup_mode,&ini_style);
						break;
				}
				break;
			case 2:
				for(j=0;output_types[j]!=NULL;j++)
					if(output_map[j]==settings.output_mode)
						break;
				if(output_types[j]==NULL)
					j=0;
				uifc.helpbuf=	"`Output Mode`\n\n"
								"~ Autodetect ~\n"
								"        Attempt to use the \"best\" display mode possible.  The order\n"
								"        these are attempted is:"
#ifdef __unix__
#ifdef NO_X
								" SDL, then Curses\n\n"
#else
								" SDL, X11 then Curses\n\n"
#endif
#else
								" SDL, then Windows Console\n\n"
#endif
#ifdef __unix__
								"~ Curses ~\n"
								"        Use text output using the Curses library.  This mode should work\n"
								"        from any terminal, however, high and low ASCII will not work\n"
								"        correctly.\n\n"
								"~ Curses on cp437 Device ~\n"
								"        As above, but assumes that the current terminal is configured to\n"
								"        display CodePage 437 correctly\n\n"
#endif
								"~ ANSI ~\n"
								"        Writes ANSI on CodePage 437 on stdout and reads input from\n"
								"        stdin. ANSI must be supported on the current terminal for this\n"
								"        mode to work.  This mode is intended to be used to run SyncTERM\n"
								"        as a door\n\n"
#if defined(__unix__) && !defined(NO_X)
								"~ X11 ~\n"
								"        Uses the Xlib library directly for graphical output.  This is\n"
								"        the graphical mode most likely to work when using X11.  This\n"
								"        mode supports font changes.\n\n"
#endif
#ifdef _WIN32
								"~ Win32 Console ~\n"
								"        Uses the windows console for display.  The font setting will\n"
								"        affect the look of the output and some low ASCII characters are\n"
								"        not displayable.  When in a window, blinking text is displayed\n"
								"        with a high-intensity background rather than blinking.  In\n"
								"        full-screen mode, blinking works correctly.\n\n"
#endif
#if defined(WITH_SDL) || defined(WITH_SDL_AUDIO)
								"~ SDL ~\n"
								"        Makes use of the SDL graphics library for graphical output.\n"
								"        This output mode allows switching to full-screen mode but is\n"
								"        otherwise identical to X11 mode.\n\n"
								"~ SDL Fullscreen ~\n"
								"        As above, but starts in full-screen mode rather than a window\n\n"
								"~ SDL Overlay ~\n"
								"        The most resource intensive mode.  However, unlike the other\n"
								"        graphical modes, this window can be scaled to any size and,\n"
								"        when switched to full screen, will always use the entire\n"
								"        display.\n\n"
								"~ SDL Overlay Fullscreen ~\n"
								"        As above, but starts in full-screen mode rather than a window\n\n"
#endif
								;
				switch(i=uifc.list(WIN_SAV,0,0,0,&j,NULL,"Output Mode",output_types)) {
					case -1:
						continue;
					default:
						settings.output_mode=output_map[j];
						iniSetEnum(&inicontents,"SyncTERM","OutputMode",output_enum,settings.output_mode,&ini_style);
						break;
				}
				break;
			case 3:
				uifc.helpbuf="`Scrollback Buffer Lines`\n\n"
							 "        The number of lines in the scrollback buffer.\n";
							 "        This value MUST be greater than zero\n";
				sprintf(str,"%d",settings.backlines);
				if(uifc.input(WIN_SAV|WIN_MID,0,0,"Scrollback Lines",str,9,K_NUMBER|K_EDIT)!=-1) {
					unsigned char *tmpscroll;

					j=atoi(str);
					if(j<1) {
						uifc.helpbuf=	"There must be at least one line in the scrollback buffer.";
						uifc.msg("Cannot set lines to less than one.");
					}
					else {
						tmpscroll=(unsigned char *)realloc(scrollback_buf,80*2*j);
						if(tmpscroll == NULL) {
							uifc.helpbuf="The selected scrollback size is too large.\n"
										 "Please reduce the number of lines.";
							uifc.msg("Cannot allocate space for scrollback.");
						}
						else {
							if(scrollback_lines > (unsigned)j)
								scrollback_lines=j;
							scrollback_buf=tmpscroll;
							settings.backlines=j;
							iniSetInteger(&inicontents,"SyncTERM","ScrollBackLines",settings.backlines,&ini_style);
						}
					}
				}
				break;
			case 4:
				uifc.helpbuf=	"`Modem Device`\n\n"
#ifdef _WIN32
								"Enter the modem device name (ie: COM1).";
#else
								"Enter the modem device name (ie: /dev/ttyd0).";
#endif
				if(uifc.input(WIN_MID|WIN_SAV,0,0,"Modem Device",settings.mdm.device_name,LIST_NAME_MAX,K_EDIT)>=0)
					iniSetString(&inicontents,"SyncTERM","ModemDevice",settings.mdm.device_name,&ini_style);
				break;
			case 5:
				uifc.helpbuf=	"`Modem Init String`\n\n"
								"Your modem init string goes here.\n"
								"For reference, here are the expected settings and USR inits\n\n"
								"State                      USR Init\n"
								"------------------------------------\n"
								"Echo on                    E1\n"
								"Verbal result codes        Q0V1\n"
								"Include connection speed   &X4\n"
								"Normal CD Handling         &C1\n"
								"Locked speed               &B1\n"
								"Normal DTR                 &D2\n"
								"CTS/RTS Flow Control       &H1&R2\n"
								"Disable Software Flow      &I0\n";
				if(uifc.input(WIN_MID|WIN_SAV,0,0,"Modem Init String",settings.mdm.init_string,LIST_NAME_MAX,K_EDIT)>=0)
					iniSetString(&inicontents,"SyncTERM","ModemInit",settings.mdm.init_string,&ini_style);
				break;
		}
	}
write_ini:
	if(!safe_mode) {
		if((inifile=fopen(inipath,"w"))!=NULL) {
			iniWriteFile(inifile,inicontents);
			fclose(inifile);
		}
	}
}

void load_bbslist(struct bbslist **list, size_t listsize, struct bbslist *defaults, char *listpath, size_t listpathsize, char *shared_list, size_t shared_listsize, int *listcount)
{
	free_list(&list[0],*listcount);
	*listcount=0;

	memset(list,0,listsize);
	memset(defaults,0,sizeof(struct bbslist));

	read_list(listpath, list, defaults, listcount, USER_BBSLIST);

	/* System BBS List */
	if(stricmp(shared_list, listpath)) /* don't read the same list twice */
		read_list(shared_list, list, defaults, listcount, SYSTEM_BBSLIST);
	sort_list(list, listcount);
}

/*
 * Displays the BBS list and allows edits to user BBS list
 * Mode is one of BBSLIST_SELECT or BBSLIST_EDIT
 */
struct bbslist *show_bbslist(int mode)
{
	struct	bbslist	*list[MAX_OPTS+1];
	int		i,j;
	static int		opt=0,bar=0;
	int		oldopt=-1;
	int		sopt=0,sbar=0;
	static struct bbslist retlist;
	int		val;
	int		listcount=0;
	char	str[128];
	char	*YesNo[3]={"Yes","No",""};
	char	title[1024];
	char	*p;
	char	addy[LIST_ADDR_MAX+1];
	char	*settings_menu[]= {
					 "Default BBS Configuration"
					,"Mouse Actions"
					,"Screen Setup"
					,"Font Management"
					,"Program Settings"
					,NULL
				};
	int		at_settings=0;
	struct mouse_event mevent;
	struct bbslist defaults;
	char	shared_list[MAX_PATH+1];
	char	listpath[MAX_PATH+1];

	if(init_uifc(TRUE, TRUE))
		return(NULL);

	get_syncterm_filename(listpath, sizeof(listpath), SYNCTERM_PATH_LIST, FALSE);
	get_syncterm_filename(shared_list, sizeof(shared_list), SYNCTERM_PATH_LIST, TRUE);
	load_bbslist(list, sizeof(list), &defaults, listpath, sizeof(listpath), shared_list, sizeof(shared_list), &listcount);

	uifc.list(WIN_T2B|WIN_RHT|WIN_IMM|WIN_INACT
		,0,0,0,&sopt,&sbar,"SyncTERM Settings",settings_menu);
	for(;;) {
		if (!at_settings) {
			for(;!at_settings;) {
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
					|WIN_ACT|WIN_INSACT|WIN_DELACT|WIN_UNGETMOUSE
					|WIN_T2B|WIN_INS|WIN_DEL|WIN_EDIT|WIN_EXTKEYS|WIN_DYN
					,0,0,0,&opt,&bar,mode==BBSLIST_SELECT?"Directory":"Edit",(char **)list);
				if(val==listcount)
					val=listcount|MSK_INS;
				if(val<0) {
					switch(val) {
						case -2-0x3000:	/* ALT-B - Scrollback */
							viewofflinescroll();
							break;
						case -2-CIO_KEY_MOUSE:	/* Clicked outside of window... */
							getmouse(&mevent);
						case -2-0x0f00:	/* Backtab */
						case -2-0x4b00:	/* Left Arrow */
						case -2-0x4d00:	/* Right Arrow */
						case -11:		/* TAB */
							uifc.list((listcount<MAX_OPTS?WIN_XTR:0)
								|WIN_T2B|WIN_IMM|WIN_INACT
								,0,0,0,&opt,&bar,mode==BBSLIST_SELECT?"Directory":"Edit",(char **)list);
							at_settings=!at_settings;
							break;
						case -7:		/* CTRL-E */
							if(list[opt]) {
								i=list[opt]->id;
								if(edit_list(list[opt],listpath,FALSE)) {
									load_bbslist(list, sizeof(list), &defaults, listpath, sizeof(listpath), shared_list, sizeof(shared_list), &listcount);
									for(j=0;list[j]!=NULL && list[j]->name[0];j++) {
										if(list[j]->id==i)
											opt=j;
									}
									oldopt=-1;
								}
							}
							break;
						case -6:		/* CTRL-D */
							uifc.changes=0;
							uifc.helpbuf=	"`SyncTERM QuickDial`\n\n"
											"Enter a URL in the format [(rlogin|telnet)://][user[:password]@]domainname[:port]\n";
							uifc.input(WIN_MID|WIN_SAV,0,0,"BBS Address",addy,LIST_ADDR_MAX,0);
							memcpy(&retlist, &defaults, sizeof(defaults));
							if(uifc.changes) {
								parse_url(addy,&retlist,defaults.conn_type,FALSE);
								free_list(&list[0],listcount);
								return(&retlist);
							}
							break;
						case -1:		/* ESC */
							if(settings.confirm_close && !confirm("Are you sure you want to exit?",NULL))
								continue;
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
							if(safe_mode) {
								uifc.helpbuf=	"`Cannot edit list in safe mode`\n\n"
												"SyncTERM is currently running in safe mode.  This means you cannot add to the\n"
												"BBS list.";
								uifc.msg("Cannot edit list in safe mode");
								break;
							}
							listcount++;
							list[listcount]=list[listcount-1];
							list[listcount-1]=(struct bbslist *)malloc(sizeof(struct bbslist));
							memcpy(list[listcount-1],&defaults,sizeof(struct bbslist));
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
								add_bbs(listpath,list[listcount-1]);
								load_bbslist(list, sizeof(list), &defaults, listpath, sizeof(listpath), shared_list, sizeof(shared_list), &listcount);
								for(j=0;list[j]!=NULL && list[j]->name[0];j++) {
									if(list[j]->id==listcount-1)
										opt=j;
								}
								oldopt=-1;
							}
							break;
						case MSK_DEL:
							if(list[opt]==NULL || !list[opt]->name[0]) {
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
							if(safe_mode) {
								uifc.helpbuf=	"`Cannot edit list in safe mode`\n\n"
												"SyncTERM is currently running in safe mode.  This means you cannot remove from the\n"
												"BBS list.";
								uifc.msg("Cannot edit list in safe mode");
								break;
							}
							if(list[opt]->type==SYSTEM_BBSLIST) {
								uifc.helpbuf=	"`Cannot delete from system list`\n\n"
												"This BBS was loaded from the system-wide list and cannot be deleted.";
								uifc.msg("Cannot delete system list entries");
								break;
							}
							sprintf(str,"Delete %s?",list[opt]->name);
							i=1;
							if(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,NULL,str,YesNo)!=0)
								break;
							del_bbs(listpath,list[opt]);
							load_bbslist(list, sizeof(list), &defaults, listpath, sizeof(listpath), shared_list, sizeof(shared_list), &listcount);
							break;
						case MSK_EDIT:
							if(safe_mode) {
								uifc.helpbuf=	"`Cannot edit list in safe mode`\n\n"
												"SyncTERM is currently running in safe mode.  This means you cannot edit the\n"
												"BBS list.";
								uifc.msg("Cannot edit list in safe mode");
								break;
							}
							i=list[opt]->id;
							if(edit_list(list[opt],listpath,FALSE)) {
								load_bbslist(list, sizeof(list), &defaults, listpath, sizeof(listpath), shared_list, sizeof(shared_list), &listcount);
								for(j=0;list[j]!=NULL && list[j]->name[0];j++) {
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
						if(safe_mode) {
							uifc.helpbuf=	"`Cannot edit list in safe mode`\n\n"
											"SyncTERM is currently running in safe mode.  This means you cannot edit the\n"
											"BBS list.";
							uifc.msg("Cannot edit list in safe mode");
							break;
						}
						i=list[opt]->id;
						if(edit_list(list[opt],listpath,FALSE)) {
							load_bbslist(list, sizeof(list), &defaults, listpath, sizeof(listpath), shared_list, sizeof(shared_list), &listcount);
							for(j=0;list[j]!=NULL && list[j]->name[0];j++) {
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
		else {
			for(;at_settings;) {
				uifc.helpbuf=	"`SyncTERM Settings Menu`\n\n"
								"~ Default BBS Configuration ~\n"
								"        Modify the settings that are used by default for new entries\n\n"
								"~ Mouse Actions ~\n"
								"        This feature is not yet functional\n\n"
								"~ Screen Setup ~\n"
								"        Set the initial screen size and emulation\n\n"
								"~ Font Management ~\n"
								"        Configure additional font files\n\n"
								"~ Program Settings ~\n"
								"        Modify hardware and video settings\n\n";
				if(oldopt != -2)
					settitle(syncterm_version);
				oldopt=-2;
				val=uifc.list(WIN_ACT|WIN_T2B|WIN_RHT|WIN_EXTKEYS|WIN_DYN|WIN_UNGETMOUSE
					,0,0,0,&sopt,&sbar,"SyncTERM Settings",settings_menu);
				switch(val) {
					case -2-0x3000:	/* ALT-B - Scrollback */
						viewofflinescroll();
						break;
					case -2-CIO_KEY_MOUSE:
						getmouse(&mevent);
					case -2-0x0f00:	/* Backtab */
					case -2-0x4b00:	/* Left Arrow */
					case -2-0x4d00:	/* Right Arrow */
					case -11:		/* TAB */
						uifc.list(WIN_T2B|WIN_RHT|WIN_IMM|WIN_INACT
							,0,0,0,&sopt,&sbar,"SyncTERM Settings",settings_menu);
						at_settings=!at_settings;
						break;
					case -1:		/* ESC */
						if(settings.confirm_close && !confirm("Are you sure you want to exit?",NULL))
							continue;
						free_list(&list[0],listcount);
						return(NULL);
					case 0:			/* Edit default connection settings */
						edit_list(&defaults,listpath,TRUE);
						break;
					case 1:			/* Mouse Actions setup */
						uifc.helpbuf=	"Mouse actions are not yet user conifurable."
										"This item is here to remind me to implement it.";
						uifc.msg("This section not yet functional");
						break;
					case 2: {		/* Screen Setup */
							struct text_info ti;
							gettextinfo(&ti);

							uifc.helpbuf=	"`Screen Setup`\n\n"
									"Select the new screen size.\n";
							i=ti.currmode;
							switch(i) {
								case C80:
									i=SCREEN_MODE_80X25;
									break;
								case C80X28:
									i=SCREEN_MODE_80X28;
									break;
								case C80X43:
									i=SCREEN_MODE_80X43;
									break;
								case C80X50:
									i=SCREEN_MODE_80X50;
									break;
								case C80X60:
									i=SCREEN_MODE_80X60;
									break;
							}
							i=uifc.list(WIN_SAV,0,0,0,&i,NULL,"Screen Setup",screen_modes);
							if(i>=0) {
								uifcbail();
								switch(i) {
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
									case SCREEN_MODE_C64:
										textmode(C64_40X25);
										break;
									case SCREEN_MODE_C128_40:
										textmode(C128_40X25);
										break;
									case SCREEN_MODE_C128_80:
										textmode(C128_80X25);
										break;
									case SCREEN_MODE_ATARI:
										textmode(ATARI_40X24);
										break;
								}
								init_uifc(TRUE, TRUE);
							}
							uifc.list((listcount<MAX_OPTS?WIN_XTR:0)
								|WIN_T2B|WIN_IMM|WIN_INACT
								,0,0,0,&opt,&bar,mode==BBSLIST_SELECT?"Directory":"Edit",(char **)list);
						}
						break;
					case 3:			/* Font management */
						if(!safe_mode) font_management();
						break;
					case 4:			/* Program settings */
						change_settings();
						break;
				}
			}
		}
	}
}
