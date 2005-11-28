#include <stdio.h>
#include <stdlib.h>

#include <dirwrap.h>
#include <ini_file.h>
#include <uifc.h>
#include "filepick.h"

#include "syncterm.h"
#include "bbslist.h"
#include "uifcinit.h"
#include "conn.h"
#include "ciolib.h"
#include "keys.h"
#include "mouse.h"
#include "cterm.h"

char *screen_modes[]={"Current", "80x25", "80x28", "80x43", "80x50", "80x60", NULL};
char *log_levels[]={"Emergency", "Alert", "Critical", "Error", "Warning", "Notice", "Info", "Debug", NULL};

char *rate_names[]={"300bps", "600bps", "1200bps", "2400bps", "4800bps", "9600bps", "19.2Kbps", "38.4Kbps", "57.6Kbps", "76.8Kbps", "115.2Kbps", "Unlimited", NULL};
int rates[]={300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 76800, 115200, 0};

char *music_names[]={"ESC [ | only", "BANSI Style", "All ANSI Music enabled", NULL};

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

void read_item(FILE *listfile, struct bbslist *entry, char *bbsname, int id, int type)
{
	BOOL	dumb;
	char	home[MAX_PATH];

	get_syncterm_filename(home, sizeof(home), SYNCTERM_DEFAULT_TRANSFER_PATH, FALSE);
	if(bbsname != NULL)
		strcpy(entry->name,bbsname);
	iniReadString(listfile,bbsname,"Address","",entry->addr);
	entry->port=iniReadShortInt(listfile,bbsname,"Port",513);
	entry->added=iniReadDateTime(listfile,bbsname,"Added",0);
	entry->connected=iniReadDateTime(listfile,bbsname,"LastConnected",0);
	entry->calls=iniReadInteger(listfile,bbsname,"TotalCalls",0);
	iniReadString(listfile,bbsname,"UserName","",entry->user);
	iniReadString(listfile,bbsname,"Password","",entry->password);
	iniReadString(listfile,bbsname,"SystemPassword","",entry->syspass);
	entry->conn_type=iniReadEnum(listfile,bbsname,"ConnectionType",conn_types,CONN_TYPE_RLOGIN);
	dumb=iniReadBool(listfile,bbsname,"BeDumb",0);
	if(dumb)
		entry->conn_type=CONN_TYPE_RAW;
	entry->reversed=iniReadBool(listfile,bbsname,"Reversed",0);
	entry->screen_mode=iniReadEnum(listfile,bbsname,"ScreenMode",screen_modes,SCREEN_MODE_CURRENT);
	entry->nostatus=iniReadBool(listfile,bbsname,"NoStatus",0);
	iniReadString(listfile,bbsname,"DownloadPath",home,entry->dldir);
	iniReadString(listfile,bbsname,"UploadPath",home,entry->uldir);
	entry->loglevel=iniReadInteger(listfile,bbsname,"LogLevel",LOG_INFO);
	entry->bpsrate=iniReadInteger(listfile,bbsname,"BPSRate",0);
	entry->music=iniReadInteger(listfile,bbsname,"ANSIMusic",CTERM_MUSIC_BANSI);
	entry->font=iniReadInteger(listfile,bbsname,"Font",default_font);
	entry->type=type;
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

	/* Add terminator */
	list[*i]=(struct bbslist *)"";
}

int edit_list(struct bbslist *item,char *listpath,int isdefault)
{
	char	opt[17][80];
	char	*opts[17];
	int		changed=0;
	int		copt=0,i,j;
	char	str[6];
	FILE *listfile;
	str_list_t	inifile;
	char	tmp[LIST_NAME_MAX+1];
	char	*itemname;

	for(i=0;i<17;i++)
		opts[i]=opt[i];
	if(item->type==SYSTEM_BBSLIST) {
		uifc.helpbuf=	"`Cannot edit system BBS list`\n\n"
						"SyncTERM supports system-wide and per-user lists.  You may only edit entries"
						"in your own personal list.\n";
		uifc.msg("Cannot edit system BBS list");
		return(0);
	}
	if((listfile=fopen(listpath,"r"))!=NULL) {
		inifile=iniReadFile(listfile);
		fclose(listfile);
	}
	else {
		if((listfile=fopen(listpath,"w"))==NULL)
			return(0);
		fclose(listfile);
		if((listfile=fopen(listpath,"r"))==NULL)
			return(0);
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
		sprintf(opt[i++],"Upload Path       %s",item->uldir);
		sprintf(opt[i++],"Log Level         %s",log_levels[item->loglevel]);
		sprintf(opt[i++],"Simulated BPS     %s",rate_names[get_rate_num(item->bpsrate)]);
		sprintf(opt[i++],"ANSI Music        %s",music_names[item->music]);
		sprintf(opt[i++],"Font              %s",font_names[item->font]);
		opt[i++][0]=0;
		uifc.changes=0;

		uifc.helpbuf=	"`Edit BBS`\n\n"
						"Select item to edit.";
		i=uifc.list(WIN_MID|WIN_SAV|WIN_ACT,0,0,0,&copt,NULL,"Edit Entry",opts);
		if(i>=0 && isdefault)
			i+=2;
		switch(i) {
			case -1:
#ifdef PCM
				if(!confirm("Quit editing?", NULL))
					continue;
#endif
				if((listfile=fopen(listpath,"w"))!=NULL) {
					iniWriteFile(listfile,inifile);
					fclose(listfile);
				}
				strListFreeStrings(inifile);
				return(changed);
			case 0:
#ifdef PCM
				if(!confirm("Edit BBS Name?",NULL))
					continue;
#endif
				uifc.helpbuf=	"`BBS Name`\n\n"
								"Enter the BBS name as it is to appear in the list.";
				strcpy(tmp,itemname);
				uifc.input(WIN_MID|WIN_SAV,0,0,"BBS Name",itemname,LIST_NAME_MAX,K_EDIT);
				iniRenameSection(&inifile,tmp,itemname);
				break;
			case 1:
#ifdef PCM
				if(!confirm("Edit BBS Address?",NULL))
					continue;
#endif
				uifc.helpbuf=	"`Address`\n\n"
								"Enter the domain name of the system to connect to ie:\n"
								"nix.synchro.net";
				uifc.input(WIN_MID|WIN_SAV,0,0,"Address",item->addr,LIST_ADDR_MAX,K_EDIT);
				iniSetString(&inifile,itemname,"Address",item->addr,&ini_style);
				break;
			case 2:
#ifdef PCM
				if(!confirm("Edit BBS Port?",NULL))
					continue;
#endif
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
				iniSetShortInt(&inifile,itemname,"Port",item->port,&ini_style);
				if(i!=j)
					uifc.changes=1;
				else
					uifc.changes=0;
				break;
			case 3:
#ifdef PCM
				if(!confirm("Edit BBS Username?",NULL))
					continue;
#endif
				uifc.helpbuf=	"`Username`\n\n"
								"Enter the username to attempt auto-login to the remote with.";
				uifc.input(WIN_MID|WIN_SAV,0,0,"Username",item->user,MAX_USER_LEN,K_EDIT);
				iniSetString(&inifile,itemname,"UserName",item->user,&ini_style);
				break;
			case 4:
#ifdef PCM
				if(!confirm("Edit BBS Password?",NULL))
					continue;
#endif
				uifc.helpbuf=	"`Password`\n\n"
								"Enter your password for auto-login.";
				uifc.input(WIN_MID|WIN_SAV,0,0,"Password",item->password,MAX_PASSWD_LEN,K_EDIT);
				iniSetString(&inifile,itemname,"Password",item->password,&ini_style);
				break;
			case 5:
#ifdef PCM
				if(!confirm("Edit BBS System Password?",NULL))
					continue;
#endif
				uifc.helpbuf=	"`System Password`\n\n"
								"Enter your System password for auto-login."
								"For non-Synchronet systems, or non-SysOp accounts,"
								"this can be used for simple scripting.";
				uifc.input(WIN_MID|WIN_SAV,0,0,"System Password",item->syspass,MAX_SYSPASS_LEN,K_EDIT);
				iniSetString(&inifile,itemname,"SystemPassword",item->syspass,&ini_style);
				break;
			case 6:
#ifdef PCM
				if(!confirm("Edit BBS Connection Type?",NULL))
					continue;
#endif
				item->conn_type--;
				uifc.helpbuf=	"`Connection Type`\n\n"
								"Select the type of connection you wish to make:\n"
								"~ RLogin:~ Auto-login with RLogin protocol\n"
								"~ Telnet:~ Use more common Telnet protocol\n"
								"~ Raw:   ~ Make a raw socket connection\n";
				uifc.list(WIN_SAV,0,0,0,&(item->conn_type),NULL,"Connection Type",&(conn_types[1]));
				item->conn_type++;
				iniSetEnum(&inifile,itemname,"ConnectionType",conn_types,item->conn_type,&ini_style);
				changed=1;
				break;
			case 7:
#ifdef PCM
				if(!confirm("Edit BBS Reversed RLogin Setting?",NULL))
					continue;
#endif
				item->reversed=!item->reversed;
				changed=1;
				iniSetBool(&inifile,itemname,"Reversed",item->reversed,&ini_style);
				break;
			case 8:
#ifdef PCM
				if(!confirm("Edit BBS Screen Mode?",NULL))
					continue;
#endif
				uifc.helpbuf=	"`Screen Mode`\n\n"
								"Select the screen size for this connection\n";
				uifc.list(WIN_SAV,0,0,0,&(item->screen_mode),NULL,"Screen Mode",screen_modes);
				iniSetEnum(&inifile,itemname,"ScreenMode",screen_modes,item->screen_mode,&ini_style);
				changed=1;
				break;
			case 9:
#ifdef PCM
				if(!confirm("Edit Status Line Visibility?",NULL))
					continue;
#endif
				item->nostatus=!item->nostatus;
				changed=1;
				iniSetBool(&inifile,itemname,"NoStatus",item->nostatus,&ini_style);
				break;
			case 10:
#ifdef PCM
				if(!confirm("Edit BBS Download Path?",NULL))
					continue;
#endif
				uifc.helpbuf=	"`Download Path`\n\n"
								"Enter the path where downloads will be placed.";
				uifc.input(WIN_MID|WIN_SAV,0,0,"Download Path",item->dldir,MAX_PATH,K_EDIT);
				iniSetString(&inifile,itemname,"DownloadPath",item->dldir,&ini_style);
				break;
			case 11:
#ifdef PCM
				if(!confirm("Edit BBS Upload Path?",NULL))
					continue;
#endif
				uifc.helpbuf=	"`Upload Path`\n\n"
								"Enter the path where uploads will be browsed for.";
				uifc.input(WIN_MID|WIN_SAV,0,0,"Upload Path",item->uldir,MAX_PATH,K_EDIT);
				iniSetString(&inifile,itemname,"UploadPath",item->uldir,&ini_style);
				break;
			case 12:
#ifdef PCM
				if(!confirm("Edit BBS Log Level?",NULL))
					continue;
#endif
				uifc.helpbuf=	"`Log Level`\n\n"
								"Set the level of verbosity for file transfer info.\n\n";
				uifc.list(WIN_SAV,0,0,0,&(item->loglevel),NULL,"Log Level",log_levels);
				iniSetInteger(&inifile,itemname,"LogLevel",item->loglevel,&ini_style);
				changed=1;
				break;
			case 13:
#ifdef PCM
				if(!confirm("Edit BBS Simulated BPS Rate?",NULL))
					continue;
#endif
				uifc.helpbuf=	"`Simulated BPS Rate`\n\n"
								"Select the rate which recieved characters will be displayed.\n\n"
								"This allows ANSImation to work as intended.";
				i=get_rate_num(item->bpsrate);
				uifc.list(WIN_SAV,0,0,0,&i,NULL,"Simulated BPS Rate",rate_names);
				item->bpsrate=rates[i];
				iniSetInteger(&inifile,itemname,"BPSRate",item->bpsrate,&ini_style);
				changed=1;
				break;
			case 14:
#ifdef PCM
				if(!confirm("Edit BBS ANSI Music Setting?",NULL))
					continue;
#endif
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
			case 15:
#ifdef PCM
				if(!confirm("Edit BBS Font?",NULL))
					continue;
#endif
				uifc.helpbuf=	"`Font`\n\n"
								"Select the desired font for this connection.\n\n"
								"Some fonts do not allow some modes.  When this is the case, 80x25 will be"
								"forced.\n";
				i=item->font;
				uifc.list(WIN_SAV,0,0,0,&i,NULL,"Font",font_names);
				if(i != item->font) {
					item->font=i;
					iniSetInteger(&inifile,itemname,"Font",item->font,&ini_style);
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
	iniSetInteger(&inifile,bbs->name,"Font",bbs->font,&ini_style);
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

struct font_files {
	char	*name;
	char	*path8x8;
	char	*path8x14;
	char	*path8x16;
};

void free_font_files(struct font_files *ff)
{
	int	i;

	if(ff==NULL)
		return;
	for(i=0; ff[i].name != NULL; i++) {
		FREE_AND_NULL(ff[i].name);
		FREE_AND_NULL(ff[i].path8x8);
		FREE_AND_NULL(ff[i].path8x14);
		FREE_AND_NULL(ff[i].path8x16);
	}
	FREE_AND_NULL(ff);
}

void save_font_files(struct font_files *fonts)
{
	FILE	*inifile;
	char	inipath[MAX_PATH];
	char	newfont[MAX_PATH];
	char	*fontid;
	str_list_t	ini_file;
	str_list_t	fontnames;
	int		i;

	get_syncterm_filename(inipath, sizeof(inipath), SYNCTERM_PATH_INI, FALSE);
	if((inifile=fopen(inipath,"r"))!=NULL) {
		ini_file=iniReadFile(inifile);
		fclose(inifile);
	}
	else {
		ini_file=strListInit();
	}

	fontnames=iniGetSectionList(ini_file, "Font:");

	/* TODO: Remove all sections... we don't *NEED* to do this */
	while((fontid=strListPop(&fontnames))!=NULL) {
		iniRemoveSection(&ini_file, fontid);
	}

	if(fonts != NULL) {
		for(i=0; fonts[i].name && fonts[i].name[0]; i++) {
			sprintf(newfont,"Font:%s",fonts[i].name);
			if(fonts[i].path8x8)
				iniSetString(&ini_file, newfont, "Path8x8", fonts[i].path8x8, &ini_style);
			if(fonts[i].path8x14)
				iniSetString(&ini_file, newfont, "Path8x14", fonts[i].path8x14, &ini_style);
			if(fonts[i].path8x16)
				iniSetString(&ini_file, newfont, "Path8x16", fonts[i].path8x16, &ini_style);
		}
	}
	if((inifile=fopen(inipath,"w"))!=NULL) {
		iniWriteFile(inifile,ini_file);
		fclose(inifile);
	}
	else {
		uifc.msg("Cannot write to the .ini file!");
	}

	strListFreeStrings(fontnames);
	strListFreeStrings(ini_file);
}

struct font_files *read_font_files(int *count)
{
	FILE	*inifile;
	char	inipath[MAX_PATH];
	char	fontpath[MAX_PATH];
	char	*fontid;
	str_list_t	fonts;
	struct font_files	*ret=NULL;
	struct font_files	*tmp=NULL;

	*count=0;
	get_syncterm_filename(inipath, sizeof(inipath), SYNCTERM_PATH_INI, FALSE);
	if((inifile=fopen(inipath, "r"))==NULL) {
		return(ret);
	}
	fonts=iniReadSectionList(inifile, "Font:");
	while((fontid=strListPop(&fonts))!=NULL) {
		if(!fontid[5])
			continue;
		(*count)++;
		tmp=(struct font_files *)realloc(ret, sizeof(struct font_files)*(*count+1));
		if(tmp==NULL) {
			count--;
			continue;
		}
		ret=tmp;
		ret[*count].name=NULL;
		ret[*count-1].name=strdup(fontid+5);
		if((ret[*count-1].path8x8=iniReadString(inifile,fontid,"Path8x8",NULL,fontpath))!=NULL)
			ret[*count-1].path8x8=strdup(fontpath);
		if((ret[*count-1].path8x14=iniReadString(inifile,fontid,"Path8x14","",fontpath))!=NULL)
			ret[*count-1].path8x14=strdup(fontpath);
		if((ret[*count-1].path8x16=iniReadString(inifile,fontid,"Path8x16",NULL,fontpath))!=NULL)
			ret[*count-1].path8x16=strdup(fontpath);
	}
	fclose(inifile);
	strListFreeStrings(fonts);
	return(ret);
}

void font_management(void)
{
	int i,j;
	int cur=0;
	int bar=0;
	int fcur=0;
	int fbar=0;
	int	count=0;
	int	size=0;
	struct font_files	*fonts;
	char	*opt[256];
	char	opts[5][80];
	struct font_files	*tmp=NULL;
	char	str[128];

	fonts=read_font_files(&count);
	opts[4][0]=0;

	for(;;) {
		uifc.helpbuf=	"`Font Management`\n\n"
						"Allows you to add and remove font files to/from the default font set.\n\n"
						"`INS` Adds a new font.\n"
						"`DEL` Removes an existing font.\n\n"
						"Selecting a font allows you to set the files for all three font sizes:\n"
						"8x8, 8x14, and 8x16.";
		if(fonts) {
			for(j=0;fonts[j].name && fonts[j].name[0]; j++)
				opt[j]=fonts[j].name;
			opt[j]="";
		}
		else {
			opts[0][0]=0;
			opt[0]=opts[0];
		}
		i=uifc.list(WIN_SAV|WIN_INS|WIN_INSACT|WIN_DEL|WIN_XTR,0,0,0,&cur,&bar,"Font",opt);
		if(i==-1) {
			save_font_files(fonts);
			free_font_files(fonts);
			return;
		}
		for(;;) {
			char 	*fontmask;
			int		show_filepick=0;
			char	**path;

			if(i&MSK_DEL) {
				FREE_AND_NULL(fonts[cur].name);
				FREE_AND_NULL(fonts[cur].path8x8);
				FREE_AND_NULL(fonts[cur].path8x14);
				FREE_AND_NULL(fonts[cur].path8x16);
				memmove(&(fonts[cur]),&(fonts[cur+1]),sizeof(struct font_files)*(count-cur-1));
				count--;
				break;
			}
			if(i&MSK_INS) {
				str[0]=0;
				if(uifc.input(WIN_SAV|WIN_MID,0,0,"Font Name",str,50,0)==-1)
					break;
				count++;
				tmp=(struct font_files *)realloc(fonts, sizeof(struct font_files)*(count+1));
				if(tmp==NULL) {
					uifc.msg("realloc() failure, cannot add font.");
					count--;
					break;
				}
				fonts=tmp;
				memmove(fonts+cur+1,fonts+cur,sizeof(struct font_files)*(count-cur));
				memset(&(fonts[count]),0,sizeof(fonts[count]));
				fonts[cur].name=strdup(str);
				fonts[cur].path8x8=NULL;
				fonts[cur].path8x14=NULL;
				fonts[cur].path8x16=NULL;
			}
			for(i=0; i<5; i++)
				opt[i]=opts[i];
			sprintf(opts[0],"Name: %.50s",fonts[cur].name?fonts[cur].name:"<undefined>");
			sprintf(opts[1],"8x8   %.50s",fonts[cur].path8x8?fonts[cur].path8x8:"<undefined>");
			sprintf(opts[2],"8x14  %.50s",fonts[cur].path8x14?fonts[cur].path8x14:"<undefined>");
			sprintf(opts[3],"8x16  %.50s",fonts[cur].path8x16?fonts[cur].path8x16:"<undefined>");
			opts[4][0]=0;
			i=uifc.list(WIN_SAV|WIN_INS|WIN_DEL,0,0,0,&fcur,&fbar,"Font",opt);
			if(i==-1)
				break;
			switch(i) {
				case 0:
					SAFECOPY(str,fonts[cur].name);
					free(fonts[cur].name);
					uifc.input(WIN_SAV|WIN_MID,0,0,"Font Name",str,50,K_EDIT);
					fonts[cur].name=strdup(str);
					show_filepick=0;
					break;
				case 1:
					sprintf(str,"8x8 %.50s",fonts[cur].name);
					path=&(fonts[cur].path8x8);
					fontmask="*.f8";
					show_filepick=1;
					break;
				case 2:
					sprintf(str,"8x14 %.50s",fonts[cur].name);
					path=&(fonts[cur].path8x14);
					fontmask="*.f14";
					show_filepick=1;
					break;
				case 3:
					sprintf(str,"8x16 %.50s",fonts[cur].name);
					path=&(fonts[cur].path8x16);
					fontmask="*.f16";
					show_filepick=1;
					break;
			}
			if(show_filepick) {
				int result;
				struct file_pick fpick;
				char	*savbuf;
				struct text_info	ti;

				gettextinfo(&ti);
				savbuf=(char *)malloc((ti.screenheight-2)*ti.screenwidth*2);
				if(savbuf==NULL) {
					uifc.msg("malloc() failure.");
					continue;
				}
				gettext(1,2,ti.screenwidth,ti.screenheight-1,savbuf);
				result=filepick(&uifc, str, &fpick, ".", fontmask, UIFC_FP_ALLOWENTRY);
				if(result!=-1 && fpick.files>0) {
					free(*path);
					*(path)=strdup(fpick.selected[0]);
				}
				filepick_free(&fpick);
				puttext(1,2,ti.screenwidth,ti.screenheight-1,savbuf);
				free(savbuf);
			}
		}
	}
}

/*
 * Displays the BBS list and allows edits to user BBS list
 * Mode is one of BBSLIST_SELECT or BBSLIST_EDIT
 */
struct bbslist *show_bbslist(int mode)
{
	struct	bbslist	*list[MAX_OPTS+1];
	int		i,j;
	int		opt=0,bar=0,oldopt=-1;
	int		sopt=0,sbar=0;
	static struct bbslist retlist;
	int		val;
	int		listcount=0;
	char	str[128];
	char	*YesNo[3]={"Yes","No",""};
	char	title[1024];
	char	currtitle[1024];
	char	*p;
	char	addy[LIST_ADDR_MAX+1];
	char	*settings_menu[]= {
					 "Default BBS Configuration"
					,"Mouse Actions"
					,"Screen Setup"
					,"Font Management"
					,"Program Settings"
					,""
				};
	int		settings=0;
	struct mouse_event mevent;
	struct bbslist defaults;
	char	shared_list[MAX_PATH];
	char	listpath[MAX_PATH];

	if(init_uifc(TRUE, TRUE))
		return(NULL);

	get_syncterm_filename(listpath, sizeof(listpath), SYNCTERM_PATH_LIST, FALSE);
	read_list(listpath, &list[0], &defaults, &listcount, USER_BBSLIST);

	/* System BBS List */
	get_syncterm_filename(shared_list, sizeof(shared_list), SYNCTERM_PATH_LIST, TRUE);
	read_list(shared_list, &list[0], &defaults, &listcount, SYSTEM_BBSLIST);

	sort_list(list);
	uifc.helpbuf=	"`SyncTERM Settings Menu`\n\n";
	uifc.list(WIN_T2B|WIN_RHT|WIN_IMM|WIN_INACT
		,0,0,0,&sopt,&sbar,"SyncTERM Settings",settings_menu);
	for(;;) {
		if (!settings) {
			for(;!settings;) {
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
						case -2-CIO_KEY_MOUSE:	/* Clicked outside of window... */
							getmouse(&mevent);
						case -11:		/* TAB */
							uifc.list((listcount<MAX_OPTS?WIN_XTR:0)
								|WIN_T2B|WIN_IMM|WIN_INACT
								,0,0,0,&opt,&bar,mode==BBSLIST_SELECT?"Directory":"Edit",(char **)list);
							settings=!settings;
							break;
						case -7:		/* CTRL-E */
		#ifdef PCM
							if(!confirm("Edit this entry?",NULL))
								continue;
		#endif
							i=list[opt]->id;
							if(edit_list(list[opt],listpath,FALSE)) {
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
		#ifdef PCM
							if(!confirm("Connect to This Address?",NULL))
								continue;
		#endif
							memcpy(&retlist, &defaults, sizeof(defaults));
							if(uifc.changes) {
								parse_url(addy,&retlist,defaults.conn_type,FALSE);
								free_list(&list[0],listcount);
								return(&retlist);
							}
							break;
						case -1:		/* ESC */
		#ifdef PCM
							if(!confirm("Are you sure you want to exit?",NULL))
								continue;
		#endif
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
		#ifdef PCM
							if(!confirm("Add new Entry?",NULL))
								continue;
		#endif
							listcount++;
							list[listcount]=list[listcount-1];
							list[listcount-1]=(struct bbslist *)malloc(sizeof(struct bbslist));
							memcpy(list[listcount-1],&defaults,sizeof(struct bbslist));
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
		#ifdef PCM
							if(!confirm("Edit this entry?",NULL))
								continue;
		#endif
							i=list[opt]->id;
							if(edit_list(list[opt],listpath,FALSE)) {
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
		#ifdef PCM
						if(!confirm("Edit this entry?",NULL))
							continue;
		#endif
						i=list[opt]->id;
						if(edit_list(list[opt],listpath,FALSE)) {
							sort_list(list);
							for(j=0;list[j]->name[0];j++) {
								if(list[j]->id==i)
									opt=j;
							}
							oldopt=-1;
						}
					}
					else {
		#ifdef PCM
						if(!confirm("Connect to this system?",NULL))
							continue;
		#endif
						memcpy(&retlist,list[val],sizeof(struct bbslist));
						free_list(&list[0],listcount);
						return(&retlist);
					}
				}
			}
		}
		else {
			for(;settings;) {
				uifc.helpbuf=	"`SyncTERM Settings Menu`\n\n";
				if(oldopt != -2)
					settitle(syncterm_version);
				oldopt=-2;
				val=uifc.list(WIN_ACT|WIN_T2B|WIN_RHT|WIN_EXTKEYS|WIN_DYN|WIN_UNGETMOUSE
					,0,0,0,&sopt,&sbar,"SyncTERM Settings",settings_menu);
				switch(val) {
					case -2-CIO_KEY_MOUSE:
						getmouse(&mevent);
					case -11:		/* TAB */
						uifc.list(WIN_T2B|WIN_RHT|WIN_IMM|WIN_INACT
							,0,0,0,&sopt,&sbar,"SyncTERM Settings",settings_menu);
						settings=!settings;
						break;
					case -1:		/* ESC */
		#ifdef PCM
						if(!confirm("Are you sure you want to exit?",NULL))
							continue;
		#endif
						free_list(&list[0],listcount);
						return(NULL);
					case 0:			/* Edit default connection settings */
						edit_list(&defaults,listpath,TRUE);
						break;
					case 1:			/* Mouse Actions setup */
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
								}
								init_uifc(TRUE, TRUE);
							}
							val=uifc.list((listcount<MAX_OPTS?WIN_XTR:0)
								|WIN_T2B|WIN_IMM|WIN_INACT
								,0,0,0,&opt,&bar,mode==BBSLIST_SELECT?"Directory":"Edit",(char **)list);
						}
						break;
					case 3:			/* Font management */
						font_management();
						break;
					case 4:			/* Program settings */
						uifc.msg("This section not yet functional");
						break;
				}
			}
		}
	}
}
