/* Copyright (C), 2007 by Stephen Hurd */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <dirwrap.h>
#include <ini_file.h>
#include <uifc.h>
#include "filepick.h"

#include "syncterm.h"
#include "fonts.h"
#include "bbslist.h"
#include "uifcinit.h"
#include "conn.h"
#include "ciolib.h"
#include "cterm.h"
#include "window.h"
#include "term.h"
#include "menu.h"
#include "vidmodes.h"

struct sort_order_info {
	char		*name;
	int			flags;
	size_t		offset;
	int			length;
};

#define SORT_ORDER_REVERSED		(1<<0)
#define SORT_ORDER_STRING		(1<<1)

struct sort_order_info sort_order[] = {
	 {
		 NULL
		,0
		,0
		,0
	}
	,{
		 "Entry Name"
		,SORT_ORDER_STRING
		,offsetof(struct bbslist, name)
		,sizeof(((struct bbslist *)NULL)->name)
	}
	,{
		 "Date Added"
		,SORT_ORDER_REVERSED
		,offsetof(struct bbslist, added)
		,sizeof(((struct bbslist *)NULL)->added)
	}
	,{
		 "Date Last Connected"
		,SORT_ORDER_REVERSED
		,offsetof(struct bbslist, connected)
		,sizeof(((struct bbslist *)NULL)->connected)
	}
	,{
		 "Total Calls"
		,SORT_ORDER_REVERSED
		,offsetof(struct bbslist, calls)
		,sizeof(((struct bbslist *)NULL)->calls)
	}
	,{
		 "Dialing List"
		,0
		,offsetof(struct bbslist, type)
		,sizeof(((struct bbslist *)NULL)->type)
	}
	,{
		 "Address"
		,SORT_ORDER_STRING
		,offsetof(struct bbslist, addr)
		,sizeof(((struct bbslist *)NULL)->addr)
	}
	,{
		 "Port"
		,0
		,offsetof(struct bbslist, port)
		,sizeof(((struct bbslist *)NULL)->port)
	}
	,{
		 "Username"
		,SORT_ORDER_STRING
		,offsetof(struct bbslist, user)
		,sizeof(((struct bbslist *)NULL)->user)
	}
	,{
		 "Password"
		,SORT_ORDER_STRING
		,offsetof(struct bbslist, password)
		,sizeof(((struct bbslist *)NULL)->password)
	}
	,{
		 "System Password"
		,SORT_ORDER_STRING
		,offsetof(struct bbslist, syspass)
		,sizeof(((struct bbslist *)NULL)->syspass)
	}
	,{
		 "Connection Type"
		,0
		,offsetof(struct bbslist, conn_type)
		,sizeof(((struct bbslist *)NULL)->conn_type)
	}
	,{
		 "Screen Mode"
		,0
		,offsetof(struct bbslist, screen_mode)
		,sizeof(((struct bbslist *)NULL)->screen_mode)
	}
	,{
		 "Status Line Visibility"
		,0
		,offsetof(struct bbslist, nostatus)
		,sizeof(((struct bbslist *)NULL)->nostatus)
	}
	,{
		 "Download Directory"
		,SORT_ORDER_STRING
		,offsetof(struct bbslist, dldir)
		,sizeof(((struct bbslist *)NULL)->dldir)
	}
	,{
		 "Upload Directory"
		,SORT_ORDER_STRING
		,offsetof(struct bbslist, uldir)
		,sizeof(((struct bbslist *)NULL)->uldir)
	}
	,{
		 "Log File"
		,SORT_ORDER_STRING
		,offsetof(struct bbslist, logfile)
		,sizeof(((struct bbslist *)NULL)->logfile)
	}
	,{
		 "Transfer Log Level"
		,0
		,offsetof(struct bbslist, xfer_loglevel)
		,sizeof(((struct bbslist *)NULL)->xfer_loglevel)
	}
	,{
		 "BPS Rate"
		,0
		,offsetof(struct bbslist, bpsrate)
		,sizeof(((struct bbslist *)NULL)->bpsrate)
	}
	,{
		 "ANSI Music"
		,0
		,offsetof(struct bbslist, music)
		,sizeof(((struct bbslist *)NULL)->music)
	}
	,{
		 "Address Family"
		,0
		,offsetof(struct bbslist, address_family)
		,sizeof(((struct bbslist *)NULL)->address_family)
	}
	,{
		 "Font"
		,SORT_ORDER_STRING
		,offsetof(struct bbslist, font)
		,sizeof(((struct bbslist *)NULL)->font)
	}
	,{
		 NULL
		,0
		,0
		,0
	}
};

int sortorder[sizeof(sort_order)/sizeof(struct sort_order_info)];

char *sort_orders[]={"Entry Name","Address","Connection Type","Port","Date Added","Date Last Connected"};

char *screen_modes[]={     "Current", "80x25", "80x28", "80x30", "80x43", "80x50", "80x60", "132x37 (16:9)", "132x52 (5:4)", "132x25", "132x28", "132x30", "132x34", "132x43", "132x50", "132x60", "C64", "C128 (40col)", "C128 (80col)", "Atari", "Atari XEP80", "Custom", NULL};
char *screen_modes_enum[]={"Current", "80x25", "80x28", "80x30", "80x43", "80x50", "80x60", "132x37",        "132x52",       "132x25", "132x28", "132x30", "132x34", "132x43", "132x50", "132x60", "C64", "C128-40col",   "C128-80col",   "Atari", "Atari-XEP80", "Custom", NULL};
char *log_levels[]={"Emergency", "Alert", "Critical", "Error", "Warning", "Notice", "Info", "Debug", NULL};
char *log_level_desc[]={"None", "Alerts", "Critical Errors", "Errors", "Warnings", "Notices", "Normal", "All (Debug)", NULL};

char *rate_names[]={"300", "600", "1200", "2400", "4800", "9600", "19200", "38400", "57600", "76800", "115200", "Current", NULL};
int rates[]={300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 76800, 115200, 0};

char *music_names[]={"ESC [ | only", "BANSI Style", "All ANSI Music enabled", NULL};
char music_helpbuf[] = "`ANSI Music Setup`\n\n"
						"~ ESC [ | only ~            Only CSI | (SyncTERM) ANSI music is supported.\n"
						"                          Enables Delete Line\n"
						"~ BANSI Style ~             Also enables BANSI-Style (CSI N)\n"
						"                          Enables Delete Line\n"
						"~ All ANSI Music Enabled ~  Enables both CSI M and CSI N ANSI music.\n"
						"                          Delete Line is disabled.\n"
						"\n"
						"So-Called ANSI Music has a long and troubled history.  Although the\n"
						"original ANSI standard has well defined ways to provide private\n"
						"extensions to the spec, none of these methods were used.  Instead,\n"
						"so-called ANSI music replaced the Delete Line ANSI sequence.  Many\n"
						"full-screen editors use DL, and to this day, some programs (Such as\n"
						"BitchX) require it to run.\n\n"
						"To deal with this, BananaCom decided to use what *they* thought was an\n"
						"unspecified escape code ESC[N for ANSI music.  Unfortunately, this is\n"
						"broken also.  Although rarely implemented in BBS clients, ESC[N is\n"
						"the erase field sequence.\n\n"
						"SyncTERM has now defined a third ANSI music sequence which *IS* legal\n"
						"according to the ANSI spec.  Specifically ESC[|.";

char *address_families[]={"PerDNS", "IPv4", "IPv6", NULL};
char *address_family_names[]={"As per DNS", "IPv4 only", "IPv6 only", NULL};

char *address_family_help =	"`Address Family`\n\n"
							"Select the address family to resolve\n\n"
							"`As per DNS`..: Uses what is in the DNS system\n"
							"`IPv4 only`...: Only uses IPv4 addresses.\n"
							"`IPv6 only`...: Only uses IPv6 addresses.\n";

char *address_help=
					"`Address`, `Phone Number`, `Serial Port`, or `Command`\n\n"
					"Enter the hostname, IP address, phone number, or serial port device of\n"
					"the system to connect to. Example: `nix.synchro.net`\n\n"
					"In the case of the Shell type, enter the command to run.\n"
					"Shell types are only functional under *nix\n";
char *conn_type_help=			"`Connection Type`\n\n"
								"Select the type of connection you wish to make:\n\n"
								"`RLogin`...........: Auto-login with RLogin protocol\n"
								"`RLogin Reversed`..: RLogin using reversed username/password parameters\n"
								"`Telnet`...........: Use more common Telnet protocol\n"
								"`Raw`..............: Make a raw TCP socket connection\n"
								"`SSH`..............: Connect using the Secure Shell (SSH-2) protocol\n"
								"`Modem`............: Connect using a dial-up modem\n"
								"`Serial`...........: Connect directly to a serial communications port\n"
								"`Shell`............: Connect to a local PTY (*nix only)\n";
								;

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
	struct	text_info txtinfo;
	struct	text_info sbtxtinfo;
	struct mouse_event mevent;

	if(scrollback_buf==NULL)
		return;
	uifcbail();
    gettextinfo(&txtinfo);

	textmode(scrollback_mode);
	switch(ciolib_to_screen(scrollback_mode)) {
		case SCREEN_MODE_C64:
			setfont(33,TRUE,1);
			break;
		case SCREEN_MODE_C128_40:
		case SCREEN_MODE_C128_80:
			setfont(35,TRUE,1);
			break;
		case SCREEN_MODE_ATARI:
		case SCREEN_MODE_ATARI_XEP80:
			setfont(36,TRUE,1);
			break;
	}
	drawwin();
	top=scrollback_lines;
	gotoxy(1,1);
	textattr(uifc.hclr|(uifc.bclr<<4)|BLINK);
    gettextinfo(&sbtxtinfo);

	for(i=0;!i && !quitting;) {
		if(top<1)
			top=1;
		if(top>(int)scrollback_lines)
			top=scrollback_lines;
		vmem_puttext(((sbtxtinfo.screenwidth-scrollback_cols)/2)+1,1
				,(sbtxtinfo.screenwidth-scrollback_cols)/2+scrollback_cols
				,sbtxtinfo.screenheight
				,scrollback_buf+(scrollback_cols*top));
		cputs("Scrollback");
		gotoxy(scrollback_cols-9,1);
		cputs("Scrollback");
		gotoxy(1,1);
		key=getch();
		switch(key) {
			case 0xe0:
			case 0:
				switch(key|getch()<<8) {
					case CIO_KEY_QUIT:
						check_exit(TRUE);
						if (quitting)
							i=1;
						break;
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
						top-=sbtxtinfo.screenheight;
						break;
					case CIO_KEY_NPAGE:
						top+=sbtxtinfo.screenheight;
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
				top-=term.height;
				break;
			case 'l':
			case 'L':
				top+=term.height;
				break;
			case ESC:
				i=1;
				break;
		}
	}

	textmode(txtinfo.currmode);
	init_uifc(TRUE,TRUE);
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

int is_sorting(int chk)
{
	int i;

	for(i=0; i<sizeof(sort_order)/sizeof(struct sort_order_info); i++)
		if((abs(sortorder[i]))==chk)
			return(1);
	return(0);
}

int intbufcmp(const void *a, const void *b, size_t size)
{
#ifdef __BIG_ENDIAN__
	return(memcmp(a,b,size));
#else
	int i;
	const unsigned char *ac=(const unsigned char *)a;
	const unsigned char *bc=(const unsigned char *)b;

	for(i=size-1; i>=0; i--) {
		if(ac[i]!=bc[i])
			return(ac[i]-bc[i]);
	}
	return(0);
#endif
}

int listcmp(const void *aptr, const void *bptr)
{
	const char *a=*(void **)(aptr);
	const char *b=*(void **)(bptr);
	int i;
	int item;
	int reverse;
	int ret=0;

	for(i=0; i<sizeof(sort_order)/sizeof(struct sort_order_info); i++) {
		item=abs(sortorder[i]);
		reverse=(sortorder[i]<0?1:0)^((sort_order[item].flags&SORT_ORDER_REVERSED)?1:0);
		if(sort_order[item].name) {
			if(sort_order[item].flags & SORT_ORDER_STRING)
				ret=stricmp(a+sort_order[item].offset,b+sort_order[item].offset);
			else
				ret=intbufcmp(a+sort_order[item].offset,b+sort_order[item].offset,sort_order[item].length);
			if(ret) {
				if(reverse)
					ret=0-ret;
				return(ret);
			}
		}
		else {
			return(ret);
		}
	}
	return(0);
}

void sort_list(struct bbslist **list, int *listcount, int *cur, int *bar, char *current)  {
	int i;
	qsort(list, *listcount, sizeof(struct bbslist *), listcmp);
	if(cur && current) {
		for(i=0; i<*listcount; i++) {
			if(strcmp(list[i]->name,current)==0) {
				*cur=i;
				if(bar)
					*bar=i;
				break;
			}
		}
	}
}

void write_sortorder(void)
{
	char	inipath[MAX_PATH+1];
	FILE	*inifile;
	str_list_t	inicontents;
	str_list_t	sortorders;
	char	str[64];
	int		i;

	sortorders=strListInit();
	for(i=0;sort_order[abs(sortorder[i])].name!=NULL;i++) {
		sprintf(str,"%d",sortorder[i]);
		strListPush(&sortorders, str);
	}

	get_syncterm_filename(inipath, sizeof(inipath), SYNCTERM_PATH_INI, FALSE);
	if((inifile=fopen(inipath,"r"))!=NULL) {
		inicontents=iniReadFile(inifile);
		fclose(inifile);
	}
	else {
		inicontents=strListInit();
	}

	iniSetStringList(&inicontents, "SyncTERM", "SortOrder", ",", sortorders, &ini_style);
	if((inifile=fopen(inipath,"w"))!=NULL) {
		iniWriteFile(inifile,inicontents);
		fclose(inifile);
	}
	strListFree(&sortorders);
	strListFree(&inicontents);
}

void edit_sorting(struct bbslist **list, int *listcount, int *ocur, int *obar, char *current)
{
	char	opt[sizeof(sort_order)/sizeof(struct sort_order_info)][80];
	char	*opts[sizeof(sort_order)/sizeof(struct sort_order_info)+1];
	char	sopt[sizeof(sort_order)/sizeof(struct sort_order_info)][80];
	char	*sopts[sizeof(sort_order)/sizeof(struct sort_order_info)+1];
	int		curr=0,bar=0;
	int		scurr=0,sbar=0;
	int		ret,sret;
	int		i,j;

	for(i=0;i<sizeof(sort_order)/sizeof(struct sort_order_info);i++)
		opts[i]=opt[i];
	opts[i]=NULL;
	for(i=0;i<sizeof(sort_order)/sizeof(struct sort_order_info);i++)
		sopts[i]=sopt[i];
	sopts[i]=NULL;

	for(;!quitting;) {
		/* Build ordered list of sort order */
		for(i=0; i<sizeof(sort_order)/sizeof(struct sort_order_info); i++) {
			if(sort_order[abs(sortorder[i])].name) {
				SAFECOPY(opt[i], sort_order[abs(sortorder[i])].name);
				if((sortorder[i]<0?1:0) ^ ((sort_order[abs(sortorder[i])].flags&SORT_ORDER_REVERSED)?1:0))
					strcat(opt[i]," (reversed)");
			}
			else
				opt[i][0]=0;
		}
		uifc.helpbuf=	"`Sort Order`\n\n"
						"Move the highlight bar to the position you would like\n"
						"to add a new ordering before and press ~INSERT~.  Choose\n"
						"a field from the list and it will be inserted.\n\n"
						"To remove a sort order, use ~DELETE~.\n\n"
						"To reverse a sort order, highlight it and press enter";
		ret=uifc.list(WIN_XTR|WIN_DEL|WIN_INS|WIN_INSACT|WIN_ACT|WIN_SAV
					,0,0,0,&curr,&bar,"Sort Order",opts);
		if(ret==-1) {
			if (uifc.exit_flags & UIFC_XF_QUIT) {
				if (!check_exit(FALSE))
					continue;
			}
			break;
		}
		if(ret & MSK_INS) {		/* Insert sorting */
			j=0;
			for(i=0; i<sizeof(sort_order)/sizeof(struct sort_order_info); i++) {
				if(!is_sorting(i) && sort_order[i].name) {
					SAFECOPY(sopt[j], sort_order[i].name);
					j++;
				}
			}
			if(j==0) {
				uifc.helpbuf=	"All sort orders are present in the list.";
				uifc.msg("No more sort orders.");
				if(check_exit(FALSE))
					break;
			}
			else {
				sopt[j][0]=0;
				uifc.helpbuf=	"Select a sort order to add and press enter";
				sret=uifc.list(WIN_SAV|WIN_BOT|WIN_RHT
							,0,0,0,&scurr,&sbar,"Sort Field",sopts);
				if(sret>=0) {
					/* Insert into array */
					memmove(&(sortorder[ret&MSK_OFF])+1,&(sortorder[(ret&MSK_OFF)]),sizeof(sortorder)-sizeof(sortorder[0])*((ret&MSK_OFF)+1));
					j=0;
					for(i=0; i<sizeof(sort_order)/sizeof(struct sort_order_info); i++) {
						if(!is_sorting(i) && sort_order[i].name) {
							if(j==sret) {
								sortorder[ret&MSK_OFF]=i;
								break;
							}
							j++;
						}
					}
				}
				else {
					if(check_exit(FALSE))
						break;
				}
			}
		}
		else if(ret & MSK_DEL) {		/* Delete criteria */
			memmove(&(sortorder[ret&MSK_OFF]),&(sortorder[(ret&MSK_OFF)])+1,sizeof(sortorder)-sizeof(sortorder[0])*((ret&MSK_OFF)+1));
		}
		else {
			sortorder[ret&MSK_OFF]=0-sortorder[ret&MSK_OFF];
		}
	}

	/* Write back to the .ini file */
	write_sortorder();
	sort_list(list, listcount, ocur, obar, current);
}

void free_list(struct bbslist **list, int listcount)
{
	int i;

	for(i=0;i<listcount;i++) {
		FREE_AND_NULL(list[i]);
	}
}

void read_item(str_list_t listfile, struct bbslist *entry, char *bbsname, int id, int type)
{
	char		home[MAX_PATH+1];
	str_list_t	section;

	get_syncterm_filename(home, sizeof(home), SYNCTERM_DEFAULT_TRANSFER_PATH, FALSE);
	if(bbsname != NULL) {
#if 0
		switch(type) {
			case USER_BBSLIST:
				SAFECOPY(entry->name,bbsname);
				break;
			case SYSTEM_BBSLIST:
				sprintf(entry->name,"[%.*s]",sizeof(entry->name)-3,bbsname);
				break;
		}
#else
		SAFECOPY(entry->name,bbsname);
#endif
	}
	section=iniGetSection(listfile,bbsname);
	iniGetString(section,NULL,"Address","",entry->addr);
	entry->conn_type=iniGetEnum(section,NULL,"ConnectionType",conn_types_enum,CONN_TYPE_RLOGIN);
	entry->port=iniGetShortInt(section,NULL,"Port",conn_ports[entry->conn_type]);
	entry->added=iniGetDateTime(section,NULL,"Added",0);
	entry->connected=iniGetDateTime(section,NULL,"LastConnected",0);
	entry->calls=iniGetInteger(section,NULL,"TotalCalls",0);
	iniGetString(section,NULL,"UserName","",entry->user);
	iniGetString(section,NULL,"Password","",entry->password);
	iniGetString(section,NULL,"SystemPassword","",entry->syspass);
	if(iniGetBool(section,NULL,"BeDumb",FALSE))	/* Legacy */
		entry->conn_type=CONN_TYPE_RAW;
	entry->screen_mode=iniGetEnum(section,NULL,"ScreenMode",screen_modes_enum,SCREEN_MODE_CURRENT);
	entry->nostatus=iniGetBool(section,NULL,"NoStatus",FALSE);
	iniGetString(section,NULL,"DownloadPath",home,entry->dldir);
	iniGetString(section,NULL,"UploadPath",home,entry->uldir);

	/* Log Stuff */
	iniGetString(section,NULL,"LogFile","",entry->logfile);
	entry->append_logfile=iniGetBool(section,NULL,"AppendLogFile",TRUE);
	entry->xfer_loglevel=iniGetEnum(section,NULL,"TransferLogLevel",log_levels,LOG_INFO);
	entry->telnet_loglevel=iniGetEnum(section,NULL,"TelnetLogLevel",log_levels,LOG_INFO);

	entry->bpsrate=iniGetInteger(section,NULL,"BPSRate",0);
	entry->music=iniGetInteger(section,NULL,"ANSIMusic",CTERM_MUSIC_BANSI);
	entry->address_family=iniGetEnum(section,NULL,"AddressFamily",address_families, ADDRESS_FAMILY_UNSPEC);
	iniGetString(section,NULL,"Font","Codepage 437 English",entry->font);
	entry->type=type;
	entry->id=id;

	strListFree(&section);
}

/*
 * Checks if bbsname already is listed in list
 * setting *pos to the position if not NULL.
 * optionally only if the entry is a user list
 * entry
 */
int list_name_check(struct bbslist **list, char *bbsname, int *pos, int useronly)
{
	int i;

	if(list==NULL) {
		FILE	*listfile;
		str_list_t	inifile;

		if((listfile=fopen(settings.list_path,"r"))!=NULL) {
			inifile=iniReadFile(listfile);
			i=iniSectionExists(inifile, bbsname);
			strListFree(&inifile);
			fclose(listfile);
			return(i);
		}
		return(0);
	}
	for(i=0; list[i]!=NULL; i++) {
		if(useronly && list[i]->type != USER_BBSLIST)
			continue;
		if(stricmp(list[i]->name,bbsname)==0) {
			if(pos)
				*pos=i;
			return(1);
		}
	}
	return(0);
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
	str_list_t	inilines;

	if((listfile=fopen(listpath,"r"))!=NULL) {
		inilines=iniReadFile(listfile);
		fclose(listfile);
		if(defaults != NULL && type==USER_BBSLIST)
			read_item(inilines,defaults,NULL,-1,type);
		bbses=iniGetSectionList(inilines,NULL);
		while((bbsname=strListRemove(&bbses,0))!=NULL) {
			if(!list_name_check(list, bbsname, NULL, FALSE)) {
				if((list[*i]=(struct bbslist *)malloc(sizeof(struct bbslist)))==NULL) {
					free(bbsname);
					break;
				}
				read_item(inilines,list[*i],bbsname,*i,type);
				(*i)++;
			}
			free(bbsname);
		}
		strListFree(&bbses);
		strListFree(&inilines);
	}
	else {
		if(defaults != NULL && type==USER_BBSLIST)
			read_item(NULL,defaults,NULL,-1,type);
	}

#if 0	/* This isn't necessary (NULL is a sufficient) */
	/* Add terminator */
	list[*i]=(struct bbslist *)"";
#endif
}

int edit_list(struct bbslist **list, struct bbslist *item,char *listpath,int isdefault)
{
	char	opt[20][80];	/* <- Beware of magic number! */
	char	*opts[(sizeof(opt)/sizeof(opt[0]))+1];
	int		changed=0;
	int		copt=0,i,j;
	int		bar=0;
	char	str[64];
	FILE *listfile;
	str_list_t	inifile;
	char	tmp[LIST_NAME_MAX+1];
	char	*itemname;
	char	*YesNo[3]={"Yes","No",""};

	for(i=0;i<sizeof(opt)/sizeof(opt[0]);i++)
		opts[i]=opt[i];
	if(item->type==SYSTEM_BBSLIST) {
		uifc.helpbuf=	"`Copy from system directory`\n\n"
						"This entry was loaded from the system directory.  In order to edit it, it\n"
						"must be copied into your personal directory.\n";
		i=0;
		if(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,NULL,"Copy from system directory?",YesNo)!=0) {
			check_exit(FALSE);
			return(0);
		}
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
	for(;!quitting;) {
		i=0;
		if(!isdefault) {
			sprintf(opt[i++], "Name              %s",itemname);
			if(item->conn_type==CONN_TYPE_MODEM)
				sprintf(opt[i++], "Phone Number      %s",item->addr);
			else if(item->conn_type==CONN_TYPE_SERIAL)
				sprintf(opt[i++], "Device Name       %s",item->addr);
			else if(item->conn_type==CONN_TYPE_SHELL)
				sprintf(opt[i++], "Command           %s",item->addr);
			else
				sprintf(opt[i++], "Address           %s",item->addr);
		}
		sprintf(opt[i++], "Connection Type   %s",conn_types[item->conn_type]);
		if(item->conn_type!=CONN_TYPE_MODEM && item->conn_type!=CONN_TYPE_SERIAL
			&& item->conn_type!=CONN_TYPE_SHELL
			)
			sprintf(opt[i++], "TCP Port          %hu",item->port);
		sprintf(opt[i++], "Username          %s",item->user);
		sprintf(opt[i++], "Password          %s",item->password[0]?"********":"<none>");
		sprintf(opt[i++], "System Password   %s",item->syspass[0]?"********":"<none>");
		sprintf(opt[i++], "Screen Mode       %s",screen_modes[item->screen_mode]);
		sprintf(opt[i++], "Hide Status Line  %s",item->nostatus?"Yes":"No");
		sprintf(opt[i++], "Download Path     %s",item->dldir);
		sprintf(opt[i++], "Upload Path       %s",item->uldir);
		sprintf(opt[i++], "Log File          %s",item->logfile);
		sprintf(opt[i++], "Log Transfers     %s",log_level_desc[item->xfer_loglevel]);
		sprintf(opt[i++], "Log Telnet Cmds   %s",log_level_desc[item->telnet_loglevel]);
		sprintf(opt[i++], "Append Log File   %s",item->append_logfile ? "Yes":"No");
		if(item->bpsrate)
			sprintf(str,"%ubps", item->bpsrate);
		else
			strcpy(str,"Current");
		sprintf(opt[i++], "Comm Rate         %s",str);
		sprintf(opt[i++], "ANSI Music        %s",music_names[item->music]);
		sprintf(opt[i++], "Address Family    %s",address_family_names[item->address_family]);
		sprintf(opt[i++], "Font              %s",item->font);
		opt[i][0]=0;
		uifc.changes=0;

		uifc.helpbuf=	"`Edit Directory Entry`\n\n"
						"Select item to edit.\n\n"
						"~ Name ~\n"
						"        The name of the BBS entry\n\n"
						"~ Phone Number / Device Name / Command / Address ~\n"
						"        Required information to establish the connection (type specific)\n\n"
						"~ Conection Type ~\n"
						"        Type of connection\n\n"
						"~ TCP Port ~ (if applicable)\n"
						"        TCP port to connect to (applicable types only)\n\n"
						"~ Username ~\n"
						"        Username sent by the auto-login command\n\n"
						"~ Password ~\n"
						"        Password sent by auto-login command (not securely stored)\n\n"
						"~ System Password ~\n"
						"        System Password sent by auto-login command (not securely stored)\n\n"
						"~ Screen Mode ~\n"
						"        Display mode to use\n\n"
						"~ Hide Status Line ~\n"
						"        Selects if the status line should be hidden, giving an extra\n"
						"        display row\n\n"
						"~ Download Path ~\n"
						"        Default path to store downloaded files\n\n"
						"~ Upload Path ~\n"
						"        Default path for uploads\n\n"
						"~ Log File ~\n"
						"        Log file name when logging is enabled\n\n"
						"~ Log Transfers ~\n"
						"        Cycles through the various transfer log settings.\n\n"
						"~ Log Telnet Cmds ~\n"
						"        Cycles through the various telnet command log settings.\n\n"
						"~ Append Log File ~\n"
						"        Append log file (instead of overwrite) on each connection\n\n"
						"~ Comm Rate ~\n"
						"        Display speed\n\n"
						"~ ANSI Music ~\n"
						"        ANSI music type selection\n\n"
						"~ Address Family ~\n"
						"        IPv4 or IPv6\n\n"
						"~ Font ~\n"
						"        Select font to use for the entry\n\n"
						;
		i=uifc.list(WIN_MID|WIN_SAV|WIN_ACT,0,0,0,&copt,&bar
			,isdefault ? "Edit Default Connection":"Edit Directory Entry"
			,opts);
		if(i>=0 && isdefault)
			i+=2;
		if(i>=3 && (item->conn_type==CONN_TYPE_MODEM || item->conn_type==CONN_TYPE_SERIAL
				|| item->conn_type==CONN_TYPE_SHELL
				))
			i++;	/* no port number */
		switch(i) {
			case -1:
				check_exit(FALSE);
				if((!isdefault) && (itemname!=NULL) && (itemname[0]==0)) {
					uifc.helpbuf=	"`Cancel Save`\n\n"
									"This entry has no name and can therefore not be saved.\n"
									"Selecting `No` will return to editing mode.\n";
					i=0;
					if(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,NULL,"Cancel Save?",YesNo)!=0) {
						quitting=FALSE;
						check_exit(FALSE);
						break;
					}
					strListFree(&inifile);
					return(0);
				}
				if(!safe_mode) {
					if((listfile=fopen(listpath,"w"))!=NULL) {
						iniWriteFile(listfile,inifile);
						fclose(listfile);
					}
				}
				strListFree(&inifile);
				return(changed);
			case 0:
				do {
					uifc.helpbuf=	"`Directory Entry Name`\n\n"
									"Enter the name of the entry as it is to appear in the directory.";
					strcpy(tmp,itemname);
					uifc.input(WIN_MID|WIN_SAV,0,0,"Name",tmp,LIST_NAME_MAX,K_EDIT);
					check_exit(FALSE);
					if(quitting)
						break;
					if(stricmp(tmp,itemname) && list_name_check(list, tmp, NULL, FALSE)) {
						uifc.helpbuf=	"`Entry Name Already Exists`\n\n"
										"An entry with that name already exists in the directory.\n"
										"Please choose a unique name.\n";
						uifc.msg("Entry Name Already Exists!");
						check_exit(FALSE);
					}
					else {
						if(tmp[0]==0) {
							uifc.helpbuf=	"`Can Not Use and Empty Name`\n\n"
											"Entry names can not be empty.  Please enter an entry name.\n";
							uifc.msg("Can not use an empty name");
							check_exit(FALSE);
						}
						else {
							iniRenameSection(&inifile,itemname,tmp);
							strcpy(itemname, tmp);
						}
					}
				} while(tmp[0]==0 && !quitting);
				break;
			case 1:
				uifc.helpbuf=address_help;
				uifc.input(WIN_MID|WIN_SAV,0,0
					,item->conn_type==CONN_TYPE_MODEM ? "Phone Number"
					:item->conn_type==CONN_TYPE_SERIAL ? "Device Name"
					:item->conn_type==CONN_TYPE_SHELL ? "Command"
					: "Address"
					,item->addr,LIST_ADDR_MAX,K_EDIT);
				check_exit(FALSE);
				iniSetString(&inifile,itemname,"Address",item->addr,&ini_style);
				break;
			case 3:
				i=item->port;
				sprintf(str,"%hu",item->port);
				uifc.helpbuf=	"`TCP Port`\n\n"
								"Enter the TCP port number that the server is listening to on the remote system.\n"
								"Telnet is generally port 23, RLogin is generally 513 and SSH is\n"
								"generally 22\n";
				uifc.input(WIN_MID|WIN_SAV,0,0,"TCP Port",str,5,K_EDIT|K_NUMBER);
				check_exit(FALSE);
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
			case 4:
				uifc.helpbuf=	"`Username`\n\n"
								"Enter the username to attempt auto-login to the remote with.\n"
								"For SSH, this must be the SSH user name.";
				uifc.input(WIN_MID|WIN_SAV,0,0,"Username",item->user,MAX_USER_LEN,K_EDIT);
				check_exit(FALSE);
				iniSetString(&inifile,itemname,"UserName",item->user,&ini_style);
				break;
			case 5:
				uifc.helpbuf=	"`Password`\n\n"
								"Enter your password for auto-login.\n"
								"For SSH, this must be the SSH password if it exists.\n";
				uifc.input(WIN_MID|WIN_SAV,0,0,"Password",item->password,MAX_PASSWD_LEN,K_EDIT);
				check_exit(FALSE);
				iniSetString(&inifile,itemname,"Password",item->password,&ini_style);
				break;
			case 6:
				uifc.helpbuf=	"`System Password`\n\n"
								"Enter your System password for auto-login.\n"
								"This password is sent after the username and password, so for non-\n"
								"Synchronet, or non-sysop accounts, this can be used for simple\n"
								"scripting.";
				uifc.input(WIN_MID|WIN_SAV,0,0,"System Password",item->syspass,MAX_SYSPASS_LEN,K_EDIT);
				check_exit(FALSE);
				iniSetString(&inifile,itemname,"SystemPassword",item->syspass,&ini_style);
				break;
			case 2:
				i=item->conn_type;
				item->conn_type--;
				uifc.helpbuf=conn_type_help;
				switch(uifc.list(WIN_SAV,0,0,0,&(item->conn_type),NULL,"Connection Type",&(conn_types[1]))) {
					case -1:
						check_exit(FALSE);
						item->conn_type=i;
						break;
					default:
						item->conn_type++;
						iniSetEnum(&inifile,itemname,"ConnectionType",conn_types_enum,item->conn_type,&ini_style);

						if(item->conn_type!=CONN_TYPE_MODEM && item->conn_type!=CONN_TYPE_SERIAL
								&& item->conn_type!=CONN_TYPE_SHELL
								) {
							/* Set the port too */
							j=conn_ports[item->conn_type];
							if(j<1 || j>65535)
								j=item->port;
							item->port=j;
							iniSetShortInt(&inifile,itemname,"Port",item->port,&ini_style);
						}

						changed=1;
						break;
				}
				break;
			case 7:
				i=item->screen_mode;
				uifc.helpbuf=	"`Screen Mode`\n\n"
								"Select the screen size for this connection\n";
				j = i;
				switch(uifc.list(WIN_SAV,0,0,0,&(item->screen_mode),&j,"Screen Mode",screen_modes)) {
					case -1:
						check_exit(FALSE);
						item->screen_mode=i;
						break;
					default:
						iniSetEnum(&inifile,itemname,"ScreenMode",screen_modes_enum,item->screen_mode,&ini_style);
						if(item->screen_mode == SCREEN_MODE_C64) {
							SAFECOPY(item->font,font_names[33]);
							iniSetString(&inifile,itemname,"Font",item->font,&ini_style);
							item->nostatus = 1;
							iniSetBool(&inifile,itemname,"NoStatus",item->nostatus,&ini_style);
						}
						else if(item->screen_mode == SCREEN_MODE_C128_40
								|| item->screen_mode == SCREEN_MODE_C128_80) {
							SAFECOPY(item->font,font_names[35]);
							iniSetString(&inifile,itemname,"Font",item->font,&ini_style);
							item->nostatus = 1;
							iniSetBool(&inifile,itemname,"NoStatus",item->nostatus,&ini_style);
						}
						else if(item->screen_mode == SCREEN_MODE_ATARI) {
							SAFECOPY(item->font,font_names[36]);
							iniSetString(&inifile,itemname,"Font",item->font,&ini_style);
							item->nostatus = 1;
							iniSetBool(&inifile,itemname,"NoStatus",item->nostatus,&ini_style);
						}
						else if(item->screen_mode == SCREEN_MODE_ATARI_XEP80) {
							SAFECOPY(item->font,font_names[36]);
							iniSetString(&inifile,itemname,"Font",item->font,&ini_style);
							item->nostatus = 1;
							iniSetBool(&inifile,itemname,"NoStatus",item->nostatus,&ini_style);
						}
						else if (i == SCREEN_MODE_C64 || i == SCREEN_MODE_C128_40 ||
						    i == SCREEN_MODE_C128_80 || i == SCREEN_MODE_ATARI ||
						    i == SCREEN_MODE_ATARI_XEP80) {
							SAFECOPY(item->font,font_names[0]);
							iniSetString(&inifile,itemname,"Font",item->font,&ini_style);
							item->nostatus = 0;
							iniSetBool(&inifile,itemname,"NoStatus",item->nostatus,&ini_style);
						}
						changed=1;
						break;
				}
				break;
			case 8:
				item->nostatus=!item->nostatus;
				changed=1;
				iniSetBool(&inifile,itemname,"NoStatus",item->nostatus,&ini_style);
				break;
			case 9:
				uifc.helpbuf=	"`Download Path`\n\n"
								"Enter the path where downloads will be placed.";
				if(uifc.input(WIN_MID|WIN_SAV,0,0,"Download Path",item->dldir,MAX_PATH,K_EDIT)>=0)
					iniSetString(&inifile,itemname,"DownloadPath",item->dldir,&ini_style);
				else
					check_exit(FALSE);
				break;
			case 10:
				uifc.helpbuf=	"`Upload Path`\n\n"
								"Enter the path where uploads will be browsed from.";
				if(uifc.input(WIN_MID|WIN_SAV,0,0,"Upload Path",item->uldir,MAX_PATH,K_EDIT)>=0)
					iniSetString(&inifile,itemname,"UploadPath",item->uldir,&ini_style);
				else
					check_exit(FALSE);
				break;
			case 11:
				uifc.helpbuf=	"`Log Filename`\n\n"
								"Enter the path to the optional log file.";
				if(uifc.input(WIN_MID|WIN_SAV,0,0,"Log File",item->logfile,MAX_PATH,K_EDIT)>=0)
					iniSetString(&inifile,itemname,"LogFile",item->logfile,&ini_style);
				else
					check_exit(FALSE);
				break;
			case 12:
				item->xfer_loglevel--;
				if(item->xfer_loglevel<0)
					item->xfer_loglevel=LOG_DEBUG;
				else if(item->xfer_loglevel<LOG_ERR)
					item->xfer_loglevel=0;
				iniSetEnum(&inifile,itemname,"TransferLogLevel",log_levels,item->xfer_loglevel,&ini_style);
				changed=1;
				break;
			case 13:
				item->telnet_loglevel--;
				if(item->telnet_loglevel<0)
					item->telnet_loglevel=LOG_DEBUG;
				else if(item->telnet_loglevel<LOG_ERR)
					item->telnet_loglevel=0;
				iniSetEnum(&inifile,itemname,"TelnetLogLevel",log_levels,item->telnet_loglevel,&ini_style);
				changed=1;
				break;
			case 14:
				item->append_logfile=!item->append_logfile;
				changed=1;
				iniSetBool(&inifile,itemname,"AppendLogFile",item->append_logfile,&ini_style);
				break;
			case 15:
				uifc.helpbuf=	"`Comm Rate (in bits-per-second)`\n\n"
								"`For TCP connections:`\n"
								"Select the rate which received characters will be displayed.\n\n"
								"This allows animated ANSI and some games to work as intended.\n\n"
								"`For Modem/Direct COM port connections:`\n"
								"Select the `DTE Rate` to use."
								;
				i=get_rate_num(item->bpsrate);
				switch(uifc.list(WIN_SAV,0,0,0,&i,NULL,"Comm Rate (BPS)",rate_names)) {
					case -1:
						check_exit(FALSE);
						break;
					default:
						item->bpsrate=rates[i];
						iniSetInteger(&inifile,itemname,"BPSRate",item->bpsrate,&ini_style);
						changed=1;
				}
				break;
			case 16:
				uifc.helpbuf=music_helpbuf;				i=item->music;
				if(uifc.list(WIN_SAV,0,0,0,&i,NULL,"ANSI Music Setup",music_names)!=-1) {
					item->music=i;
					iniSetInteger(&inifile,itemname,"ANSIMusic",item->music,&ini_style);
					changed=1;
				}
				else
					check_exit(FALSE);
				break;
			case 17:
				uifc.helpbuf=address_family_help;
				i=item->address_family;
				if(uifc.list(WIN_SAV, 0, 0, 0, &i, NULL, "Address Family", address_family_names)!=-1) {
					item->address_family=i;
					iniSetEnum(&inifile, itemname, "AddressFamily", address_families, item->address_family, &ini_style);
					changed=1;
				}
				else
					check_exit(FALSE);
				break;
			case 18:
				uifc.helpbuf=	"`Font`\n\n"
								"Select the desired font for this connection.\n\n"
								"Some fonts do not allow some modes.  When this is the case, an\n"
								"appropriate mode will be forced.\n";
				i=j=find_font_id(item->font);
				switch(uifc.list(WIN_SAV,0,0,0,&i,&j,"Font",font_names)) {
					case -1:
						check_exit(FALSE);
						break;
					default:
					if(i!=find_font_id(item->font)) {
						SAFECOPY(item->font,font_names[i]);
						iniSetString(&inifile,itemname,"Font",item->font,&ini_style);
						changed=1;
					}
				}
				break;
		}
		if(uifc.changes)
			changed=1;
	}
	strListFree(&inifile);
	return (changed);
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
	iniSetEnum(&inifile,bbs->name,"ConnectionType",conn_types_enum,bbs->conn_type,&ini_style);
	iniSetEnum(&inifile,bbs->name,"ScreenMode",screen_modes_enum,bbs->screen_mode,&ini_style);
	iniSetBool(&inifile,bbs->name,"NoStatus",bbs->nostatus,&ini_style);
	iniSetString(&inifile,bbs->name,"DownloadPath",bbs->dldir,&ini_style);
	iniSetString(&inifile,bbs->name,"UploadPath",bbs->uldir,&ini_style);
	iniSetString(&inifile,bbs->name,"LogFile",bbs->logfile,&ini_style);
	iniSetEnum(&inifile,bbs->name,"TransferLogLevel",log_levels,bbs->xfer_loglevel,&ini_style);
	iniSetEnum(&inifile,bbs->name,"TelnetLogLevel",log_levels,bbs->telnet_loglevel,&ini_style);
	iniSetBool(&inifile,bbs->name,"AppendLogFile",bbs->append_logfile,&ini_style);
	iniSetInteger(&inifile,bbs->name,"BPSRate",bbs->bpsrate,&ini_style);
	iniSetInteger(&inifile,bbs->name,"ANSIMusic",bbs->music,&ini_style);
	iniSetEnum(&inifile, bbs->name, "AddressFamily", address_families, bbs->address_family, &ini_style);
	iniSetString(&inifile,bbs->name,"Font",bbs->font,&ini_style);
	if((listfile=fopen(listpath,"w"))!=NULL) {
		iniWriteFile(listfile,inifile);
		fclose(listfile);
	}
	strListFree(&inifile);
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
		strListFree(&inifile);
	}
}

/*
 * This is pretty sketchy...
 * These are pointers to automatic variables in show_bbslist() which are
 * used to redraw the entire menu set if the custom mode is current
 * and is changed.
 */
static int *glob_sopt;
static int *glob_sbar;
static char **glob_settings_menu;

static int *glob_listcount;
static int *glob_opt;
static int *glob_bar;
static char *glob_list_title;
static struct bbslist ***glob_list;

/*
 * This uses the above variables and therefore *must* be called from
 * show_bbslist().
 *
 * If show_bbslist() is not on the stack, this will do insane things.
 */
static void
custom_mode_adjusted(int *cur, char **opt)
{
	struct text_info ti;
	int cvmode;

	gettextinfo(&ti);
	if (ti.currmode != CIOLIB_MODE_CUSTOM) {
		cvmode = find_vmode(ti.currmode);
		if (cvmode >= 0) {
			vparams[cvmode].cols = settings.custom_cols;
			vparams[cvmode].rows = settings.custom_rows;
			vparams[cvmode].charheight = settings.custom_fontheight;
		}
		return;
	}

	uifcbail();
	textmode(0);
	cvmode = find_vmode(ti.currmode);
	if (cvmode >= 0) {
		vparams[cvmode].cols = settings.custom_cols;
		vparams[cvmode].rows = settings.custom_rows;
		vparams[cvmode].charheight = settings.custom_fontheight;
		textmode(ti.currmode);
	}
	init_uifc(TRUE, TRUE);

	// Draw BBS List
	uifc.list((*glob_listcount<MAX_OPTS?WIN_XTR:0)
		|WIN_ACT|WIN_INSACT|WIN_DELACT|WIN_SAV|WIN_ESC
		|WIN_T2B|WIN_INS|WIN_DEL|WIN_EDIT|WIN_EXTKEYS|WIN_DYN
		|WIN_SEL|WIN_INACT
		,0,0,0,glob_opt,glob_bar,glob_list_title,(char **)*glob_list);
	// Draw settings menu
	uifc.list(WIN_T2B|WIN_RHT|WIN_EXTKEYS|WIN_DYN|WIN_ACT|WIN_INACT
		,0,0,0,glob_sopt,glob_sbar,"SyncTERM Settings",glob_settings_menu);
	// Draw program settings
	uifc.list(WIN_MID|WIN_SAV|WIN_ACT|WIN_DYN|WIN_SEL|WIN_INACT,0,0,0,cur,NULL,"Program Settings",opt);
}

void change_settings(int connected)
{
	char	inipath[MAX_PATH+1];
	FILE	*inifile;
	str_list_t	inicontents;
	char	opts[12][80];
	char	*opt[13];
	char	*subopts[8];
	int		i,j,k,l;
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

	for(i=0; i<12; i++)
		opt[i]=opts[i];
	opt[i]=NULL;

	for(;!quitting;) {

		uifc.helpbuf=	"`Program Settings Menu`\n\n"
						"~ Confirm Program Exit ~\n"
						"        Prompt the user before exiting.\n\n"
						"~ Prompt to Save ~\n"
						"        Prompt to save new URIs on before exiting\n\n"
						"~ Startup Screen Mode ~\n"
						"        Set the initial screen screen mode/size.\n\n"
						"~ Video Output Mode ~\n"
						"        Set video output mode (used during startup).\n\n"
						"~ Scrollback Buffer Lines ~\n"
						"        The number of lines in the scrollback buffer.\n\n"
						"~ Modem/Comm Device ~\n"
						"        The device name of the modem's communications port.\n\n"
						"~ Modem/Comm Rate ~\n"
						"        The DTE rate of the modem's communications port.\n\n"
						"~ Modem Init String ~\n"
						"        The command string to use to initialize the modem.\n\n"
						"~ Modem Dial String ~\n"
						"        The command string to use to dial the modem.\n\n"
						"~ List Path ~\n"
						"        The complete path to the user's BBS list.\n\n"
						"~ TERM For Shell ~\n"
						"        The value to set the TERM envirnonment variable to goes here.\n\n"
						"~ Custom Screen Mode ~\n"
						"        Configure the Custom screen mode.\n\n";
		SAFEPRINTF(opts[0],"Confirm Program Exit    %s",settings.confirm_close?"Yes":"No");
		SAFEPRINTF(opts[1],"Prompt to Save          %s",settings.prompt_save?"Yes":"No");
		SAFEPRINTF(opts[2],"Startup Screen Mode     %s",screen_modes[settings.startup_mode]);
		SAFEPRINTF(opts[3],"Video Output Mode       %s",output_descrs[settings.output_mode]);
		SAFEPRINTF(opts[4],"Scrollback Buffer Lines %d",settings.backlines);
		SAFEPRINTF(opts[5],"Modem/Comm Device       %s",settings.mdm.device_name);
		if(settings.mdm.com_rate)
			sprintf(str,"%lubps",settings.mdm.com_rate);
		else
			strcpy(str,"Current");
		SAFEPRINTF(opts[6],"Modem/Comm Rate         %s",str);
		SAFEPRINTF(opts[7],"Modem Init String       %s",settings.mdm.init_string);
		SAFEPRINTF(opts[8],"Modem Dial String       %s",settings.mdm.dial_string);
		SAFEPRINTF(opts[9],"List Path               %s",settings.list_path);
		SAFEPRINTF(opts[10],"TERM For Shell          %s",settings.TERM);
		if (connected)
			opt[11] = NULL;
		else
			sprintf(opts[11],"Custom Screen Mode");
		switch(uifc.list(WIN_MID|WIN_SAV|WIN_ACT,0,0,0,&cur,NULL,"Program Settings",opt)) {
			case -1:
				check_exit(FALSE);
				goto write_ini;
			case 0:
				settings.confirm_close=!settings.confirm_close;
				iniSetBool(&inicontents,"SyncTERM","ConfirmClose",settings.confirm_close,&ini_style);
				break;
			case 1:
				settings.prompt_save=!settings.prompt_save;
				iniSetBool(&inicontents,"SyncTERM","PromptSave",settings.prompt_save,&ini_style);
				break;
			case 2:
				j=settings.startup_mode;
				uifc.helpbuf=	"`Startup Screen Mode`\n\n"
								"Select the screen mode/size for at startup\n";
				switch(i=uifc.list(WIN_SAV,0,0,0,&j,NULL,"Startup Screen Mode",screen_modes)) {
					case -1:
						check_exit(FALSE);
						continue;
					default:
						settings.startup_mode=j;
						iniSetEnum(&inicontents,"SyncTERM","ScreenMode",screen_modes_enum,settings.startup_mode,&ini_style);
						break;
				}
				break;
			case 3:
				for(j=0;output_types[j]!=NULL;j++)
					if(output_map[j]==settings.output_mode)
						break;
				if(output_types[j]==NULL)
					j=0;
				uifc.helpbuf=	"`Video Output Mode`\n\n"
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
								"        as a BBS door\n\n"
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
#endif
								;
				switch(i=uifc.list(WIN_SAV,0,0,0,&j,NULL,"Video Output Mode",output_types)) {
					case -1:
						check_exit(FALSE);
						continue;
					default:
						settings.output_mode=output_map[j];
						iniSetEnum(&inicontents,"SyncTERM","OutputMode",output_enum,settings.output_mode,&ini_style);
						break;
				}
				break;
			case 4:
				uifc.helpbuf="`Scrollback Buffer Lines`\n\n"
							 "        The number of lines in the scrollback buffer.\n"
							 "        This value MUST be greater than zero\n";
				sprintf(str,"%d",settings.backlines);
				if(uifc.input(WIN_SAV|WIN_MID,0,0,"Scrollback Lines",str,9,K_NUMBER|K_EDIT)!=-1) {
					struct vmem_cell *tmpscroll;

					j=atoi(str);
					if(j<1) {
						uifc.helpbuf=	"There must be at least one line in the scrollback buffer.";
						uifc.msg("Cannot set lines to less than one.");
						check_exit(FALSE);
					}
					else {
						tmpscroll=realloc(scrollback_buf,80*sizeof(*scrollback_buf)*j);
						scrollback_buf = tmpscroll ? tmpscroll : scrollback_buf;
						if(tmpscroll == NULL) {
							uifc.helpbuf="The selected scrollback size is too large.\n"
										 "Please reduce the number of lines.";
							uifc.msg("Cannot allocate space for scrollback.");
							check_exit(FALSE);
						}
						else {
							if(scrollback_lines > (unsigned)j)
								scrollback_lines=j;
							settings.backlines=j;
							iniSetInteger(&inicontents,"SyncTERM","ScrollBackLines",settings.backlines,&ini_style);
						}
					}
				}
				else
					check_exit(FALSE);
				break;
			case 5:
				uifc.helpbuf=	"`Modem/Comm Device`\n\n"
								"Enter the name of the device used to communicate with the modem.\n\n"
								"Example: \"`"
								DEFAULT_MODEM_DEV
								"`\"";
				if(uifc.input(WIN_MID|WIN_SAV,0,0,"Modem/Comm Device",settings.mdm.device_name,INI_MAX_VALUE_LEN,K_EDIT)>=0)
					iniSetString(&inicontents,"SyncTERM","ModemDevice",settings.mdm.device_name,&ini_style);
				else
					check_exit(FALSE);
				break;
			case 6:
				uifc.helpbuf=	"`Modem/Comm Rate`\n\n"
								"Enter the rate (in `bits-per-second`) used to communicate with the modem.\n"
								"Use the highest `DTE Rate` supported by your communication port and modem.\n\n"
								"Examples: `38400`, `57600`, `115200`\n\n"
								"This rate is sometimes (incorrectly) referred to as the `baud rate`.\n\n"
								"Enter `0` to use the current or default rate of the communication port";
				sprintf(str,"%lu",settings.mdm.com_rate);
				if(uifc.input(WIN_MID|WIN_SAV,0,0,"Modem/Comm Rate",str,12,K_EDIT)>=0) {
					settings.mdm.com_rate=strtol(str,NULL,10);
					iniSetLongInt(&inicontents,"SyncTERM","ModemComRate",settings.mdm.com_rate,&ini_style);
				}
				else
					check_exit(FALSE);
				break;

			case 7:
				uifc.helpbuf=	"`Modem Init String`\n\n"
								"Your modem initialization string goes here.\n\n"
								"Example:\n"
								"\"`AT&F`\" will load a Hayes compatible modem's factory default settings.\n\n"
								"~For reference, here are the expected Hayes-compatible settings:~\n\n"
								"State                      Command\n"
								"----------------------------------\n"
								"Echo on                    E1\n"
								"Verbal result codes        Q0V1\n"
								"Normal CD Handling         &C1\n"
								"Normal DTR                 &D2\n"
								"\n\n"
								"~For reference, here are the expected USRobotics-compatible settings:~\n\n"
								"State                      Command\n"
								"----------------------------------\n"
								"Include connection speed   &X4\n"
								"Locked speed               &B1\n"
								"CTS/RTS Flow Control       &H1&R2\n"
								"Disable Software Flow      &I0\n";
				if(uifc.input(WIN_MID|WIN_SAV,0,0,"Modem Init String",settings.mdm.init_string,INI_MAX_VALUE_LEN-1,K_EDIT)>=0)
					iniSetString(&inicontents,"SyncTERM","ModemInit",settings.mdm.init_string,&ini_style);
				else
					check_exit(FALSE);
				break;
			case 8:
				uifc.helpbuf=   "`Modem Dial String`\n\n"
								"The command string to dial the modem goes here.\n\n"
								"Example: \"`ATDT`\" will dial a Hayes-compatible modem in touch-tone mode.";
				if(uifc.input(WIN_MID|WIN_SAV,0,0,"Modem Dial String",settings.mdm.dial_string,INI_MAX_VALUE_LEN-1,K_EDIT)>=0)
					iniSetString(&inicontents,"SyncTERM","ModemDial",settings.mdm.dial_string,&ini_style);
				else
					check_exit(FALSE);
				break;
			case 9:
				uifc.helpbuf=   "`List Path`\n\n"
								"The complete path to the BBS list goes here.\n";
				if(uifc.input(WIN_MID|WIN_SAV,0,0,"List Path",settings.list_path,MAX_PATH,K_EDIT)>=0)
					iniSetString(&inicontents,"SyncTERM","ListPath",settings.list_path,&ini_style);
				else
					check_exit(FALSE);
				break;
			case 10:
				uifc.helpbuf=   "`TERM For Shell`\n\n"
								"The value to set the TERM envirnonment variable to goes here.\n\n"
								"Example: \"`ansi`\" will select a dumb ANSI mode.";
				if(uifc.input(WIN_MID|WIN_SAV,0,0,"TERM",settings.TERM,LIST_NAME_MAX,K_EDIT)>=0)
					iniSetString(&inicontents,"SyncTERM","TERM",settings.TERM,&ini_style);
				else
					check_exit(FALSE);
				break;
			case 11:
				uifc.helpbuf=	"`Custom Screen Mode`\n\n"
								"~ Rows ~\n"
								"        Sets the number of rows in the custom screen mode\n"
								"~ Columns ~\n"
								"        Sets the number of columns in the custom screen mode\n"
								"~ Font Size ~\n"
								"        Chooses the font size used by the custom screen mode\n";
				j = 0;
				for (k=0; k==0;) {
					// Beware case 2 below if adding things
					asprintf(&subopts[0], "Rows      (%d)", settings.custom_rows);
					asprintf(&subopts[1], "Columns   (%d)", settings.custom_cols);
					asprintf(&subopts[2], "Font Size (%s)", settings.custom_fontheight == 8 ? "8x8" : settings.custom_fontheight == 14 ? "8x14" : "8x16");
					subopts[3] = NULL;
					switch (uifc.list(WIN_SAV,0,0,0,&j,NULL,"Video Output Mode",subopts)) {
						case -1:
							check_exit(FALSE);
							k = 1;
							break;
						case 0:
							uifc.helpbuf=	"`Rows`\n\n"
											"Number of rows on the custom screen.  Must be between 14 and 255";
							sprintf(str,"%d",settings.custom_rows);
							if (uifc.input(WIN_SAV|WIN_MID, 0, 0, "Custom Rows", str, 3, K_NUMBER|K_EDIT)!=-1) {
								l = atoi(str);
								if (l < 14 || l > 255) {
									uifc.msg("Rows must be between 14 and 255.");
									check_exit(FALSE);
								}
								else {
									settings.custom_rows = l;
									iniSetInteger(&inicontents, "SyncTERM", "CustomRows", settings.custom_rows, &ini_style);
									custom_mode_adjusted(&cur, opt);
								}
							}
							break;
						case 1:
							uifc.helpbuf=	"`Columns`\n\n"
											"Number of columns on the custom screen.  Must be between 40 and 255\n"
											"Note that values other than 40, 80, and 132 are not supported.";
							sprintf(str,"%d",settings.custom_cols);
							if (uifc.input(WIN_SAV|WIN_MID, 0, 0, "Custom Columns", str, 3, K_NUMBER|K_EDIT)!=-1) {
								l = atoi(str);
								if (l < 40 || l > 255) {
									uifc.msg("Columns must be between 40 and 255.");
									check_exit(FALSE);
								}
								else {
									settings.custom_cols = l;
									iniSetInteger(&inicontents, "SyncTERM", "CustomColumns", settings.custom_cols, &ini_style);
									custom_mode_adjusted(&cur, opt);
								}
							}
							break;
						case 2:
							uifc.helpbuf=	"`Font Size`\n\n"
											"Choose the font size for the custom mode.";
							subopts[4] = "8x8";
							subopts[5] = "8x14";
							subopts[6] = "8x16";
							subopts[7] = NULL;
							switch(settings.custom_fontheight) {
								case 8:
									l = 0;
									break;
								case 14:
									l = 1;
									break;
								default:
									l = 2;
									break;
							}
							switch (uifc.list(WIN_SAV, 0, 0, 0, &l, NULL, "Font Size", &subopts[4])) {
								case -1:
									check_exit(FALSE);
									break;
								case 0:
									settings.custom_fontheight = 8;
									iniSetInteger(&inicontents, "SyncTERM", "CustomFontHeight", settings.custom_fontheight, &ini_style);
									custom_mode_adjusted(&cur, opt);
									break;
								case 1:
									settings.custom_fontheight = 14;
									iniSetInteger(&inicontents, "SyncTERM", "CustomFontHeight", settings.custom_fontheight, &ini_style);
									custom_mode_adjusted(&cur, opt);
									break;
								case 2:
									settings.custom_fontheight = 16;
									iniSetInteger(&inicontents, "SyncTERM", "CustomFontHeight", settings.custom_fontheight, &ini_style);
									custom_mode_adjusted(&cur, opt);
									break;
							}
					}
					free(subopts[0]);
					free(subopts[1]);
					free(subopts[2]);
				}
		}
	}
write_ini:
	if(!safe_mode) {
		if((inifile=fopen(inipath,"w"))!=NULL) {
			iniWriteFile(inifile,inicontents);
			fclose(inifile);
		}
	}
	strListFree(&inicontents);
}

void load_bbslist(struct bbslist **list, size_t listsize, struct bbslist *defaults, char *listpath, size_t listpathsize, char *shared_list, size_t shared_listsize, int *listcount, int *cur, int *bar, char *current)
{
	free_list(&list[0],*listcount);
	*listcount=0;

	memset(list,0,listsize);
	memset(defaults,0,sizeof(struct bbslist));

	read_list(listpath, list, defaults, listcount, USER_BBSLIST);

	/* System BBS List */
	if(stricmp(shared_list, listpath)) /* don't read the same list twice */
		read_list(shared_list, list, defaults, listcount, SYSTEM_BBSLIST);
	sort_list(list, listcount, cur, bar, current);
	if(current)
		free(current);
}

/*
 * Displays the BBS list and allows edits to user BBS list
 * Mode is one of BBSLIST_SELECT or BBSLIST_EDIT
 */
struct bbslist *show_bbslist(char *current, int connected)
{
#define BBSLIST_SIZE ((MAX_OPTS+1)*sizeof(struct bbslist *))
	struct	bbslist	**list;
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
					 "Default Connection Settings"
#ifdef CONFIGURABLE_MOUSE_ACTIONS
					,"Mouse Actions"
#endif
					,"Current Screen Mode"
					,"Font Management"
					,"Program Settings"
					,NULL
				};
	char	*connected_settings_menu[]= {
					 "Default Connection Settings"
#ifdef CONFIGURABLE_MOUSE_ACTIONS
					,"Mouse Actions"
#endif
					,"Font Management"
					,"Program Settings"
					,NULL
				};
	int		at_settings=0;
	struct mouse_event mevent;
	struct bbslist defaults;
	char	shared_list[MAX_PATH+1];
	char list_title[30];

	glob_sbar = &sbar;
	glob_sopt = &sopt;
	glob_settings_menu = settings_menu;
	glob_listcount = &listcount;
	glob_opt = &opt;
	glob_bar = &bar;
	glob_list_title = list_title;
	glob_list = &list;

	if(init_uifc(connected?FALSE:TRUE, TRUE))
		return(NULL);

	get_syncterm_filename(shared_list, sizeof(shared_list), SYNCTERM_PATH_LIST, TRUE);
	list = malloc(BBSLIST_SIZE);
	if (list == NULL)
		return (NULL);
	load_bbslist(list, BBSLIST_SIZE, &defaults, settings.list_path, sizeof(settings.list_path), shared_list, sizeof(shared_list), &listcount, &opt, &bar, current?strdup(current):NULL);

	uifc.helpbuf="Help Button Hack";
	uifc.list(WIN_T2B|WIN_RHT|WIN_EXTKEYS|WIN_DYN|WIN_ACT|WIN_INACT
		,0,0,0,&sopt,&sbar,"SyncTERM Settings",connected?connected_settings_menu:settings_menu);
	for(;;) {
		if (quitting) {
			free(list);
			return NULL;
		}
		if (!at_settings) {
			for(;!at_settings;) {
				sprintf(list_title, "Directory (%d items)", listcount);
				if (quitting) {
					free(list);
					return NULL;
				}
				if(connected)
					uifc.helpbuf=	"`SyncTERM Directory`\n\n"
									"Commands:\n\n"
									"~ CTRL-E ~ to edit the selected entry\n"
									"~ CTRL-S ~ to modify the sort order\n"
									" ~ ENTER ~ to connect to the selected entry";
				else
					uifc.helpbuf=	"`SyncTERM Directory`\n\n"
									"Commands:\n\n"
									"~ CTRL-D ~ Quick-connect to a URL\n"
									"~ CTRL-E ~ to edit the selected entry\n"
									"~ CTRL-S ~ to modify the sort order\n"
									" ~ ENTER ~ to connect to the selected entry";
				if(opt != oldopt) {
					if(list[opt]!=NULL && list[opt]->name[0]) {
						sprintf(title, "%s - %s (%d calls / Last: %s", syncterm_version, (char *)(list[opt]), list[opt]->calls, list[opt]->connected?ctime(&list[opt]->connected):"Never\n");
						p=strrchr(title, '\n');
						if(p!=NULL)
							*p=')';
					}
					else
						SAFECOPY(title, syncterm_version);
					settitle(title);
				}
				oldopt=opt;
				val=uifc.list((listcount<MAX_OPTS?WIN_XTR:0)
					|WIN_ACT|WIN_INSACT|WIN_DELACT|WIN_UNGETMOUSE|WIN_SAV|WIN_ESC
					|WIN_T2B|WIN_INS|WIN_DEL|WIN_EDIT|WIN_EXTKEYS|WIN_DYN
					,0,0,0,&opt,&bar,list_title,(char **)list);
				if(val==listcount)
					val=listcount|MSK_INS;
				if(val==-7)	{ /* CTRL-E */
					uifc.list((listcount<MAX_OPTS?WIN_XTR:0)
						|WIN_ACT|WIN_INSACT|WIN_DELACT|WIN_SAV|WIN_ESC
						|WIN_T2B|WIN_INS|WIN_DEL|WIN_EDIT|WIN_EXTKEYS|WIN_DYN
						|WIN_SEL
						,0,0,0,&opt,&bar,list_title,(char **)list);
					val=opt|MSK_EDIT;
				}
				if(val<0) {
					switch(val) {
						case -2-0x13:	/* CTRL-S - Sort */
							uifc.list((listcount<MAX_OPTS?WIN_XTR:0)
								|WIN_ACT|WIN_INSACT|WIN_DELACT|WIN_SAV|WIN_ESC
								|WIN_T2B|WIN_INS|WIN_DEL|WIN_EDIT|WIN_EXTKEYS|WIN_DYN
								|WIN_SEL
								,0,0,0,&opt,&bar,list_title,(char **)list);
							edit_sorting(list,&listcount, &opt, &bar, list[opt]?list[opt]->name:NULL);
							break;
						case -2-0x3000:	/* ALT-B - Scrollback */
							if(!connected) {
								viewofflinescroll();
								uifc.list(WIN_T2B|WIN_RHT|WIN_EXTKEYS|WIN_DYN|WIN_ACT|WIN_INACT
									,0,0,0,&sopt,&sbar,"SyncTERM Settings",settings_menu);
							}
							break;
						case -2-CIO_KEY_MOUSE:	/* Clicked outside of window... */
							getmouse(&mevent);
							/* Fall-through */
						case -2-0x0f00:	/* Backtab */
						case -2-0x4b00:	/* Left Arrow */
						case -2-0x4d00:	/* Right Arrow */
						case -11:		/* TAB */
							uifc.list((listcount<MAX_OPTS?WIN_XTR:0)
								|WIN_ACT|WIN_INSACT|WIN_DELACT|WIN_SAV|WIN_ESC
								|WIN_T2B|WIN_INS|WIN_DEL|WIN_EDIT|WIN_EXTKEYS|WIN_DYN
								|WIN_SEL
								,0,0,0,&opt,&bar,list_title,(char **)list);
							at_settings=!at_settings;
							break;
						case -6:		/* CTRL-D */
							if(!connected) {
								if(safe_mode) {
									uifc.helpbuf=	"`Cannot Quick-Connect in safe mode`\n\n"
													"SyncTERM is currently running in safe mode.  This means you cannot use the\n"
													"Quick-Connect feature.";
									uifc.msg("Cannot edit list in safe mode");
									break;
								}
								uifc.changes=0;
								uifc.helpbuf=	"`SyncTERM Quick-Connect`\n\n"
												"Enter a URL in the format:\n"
												"[(rlogin|telnet|ssh)://][user[:password]@]domainname[:port]\n";
								uifc.list((listcount<MAX_OPTS?WIN_XTR:0)
									|WIN_ACT|WIN_INSACT|WIN_DELACT|WIN_SAV|WIN_ESC
									|WIN_T2B|WIN_INS|WIN_DEL|WIN_EDIT|WIN_EXTKEYS|WIN_DYN
									|WIN_SEL
									,0,0,0,&opt,&bar,list_title,(char **)list);
								uifc.input(WIN_MID|WIN_SAV,0,0,"Address",addy,LIST_ADDR_MAX,0);
								memcpy(&retlist, &defaults, sizeof(defaults));
								if(uifc.changes) {
									parse_url(addy,&retlist,defaults.conn_type,FALSE);
									free_list(&list[0],listcount);
									free(list);
									return(&retlist);
								}
							}
							break;
						case -1:		/* ESC */
							if(!connected)
								if (!check_exit(TRUE))
									continue;
							free_list(&list[0],listcount);
							free(list);
							return(NULL);
					}
				}
				else if(val&MSK_ON) {
					char tmp[LIST_NAME_MAX+1];

					switch(val&MSK_ON) {
						case MSK_INS:
							if(listcount>=MAX_OPTS) {
								uifc.helpbuf=	"`Max List size reached`\n\n"
												"The total combined size of loaded entries is currently the highest\n"
												"supported size.  You must delete entries before adding more.";
								uifc.msg("Max List size reached!");
								check_exit(FALSE);
								break;
							}
							if(safe_mode) {
								uifc.helpbuf=	"`Cannot edit list in safe mode`\n\n"
												"SyncTERM is currently running in safe mode.  This means you cannot add to the\n"
												"directory.";
								uifc.msg("Cannot edit list in safe mode");
								check_exit(FALSE);
								break;
							}
							tmp[0]=0;
							uifc.changes=0;
							uifc.helpbuf=	"`Name`\n\n"
											"Enter the name of the entry as it is to appear in the directory.";
							if(uifc.input(WIN_MID|WIN_SAV,0,0,"Name",tmp,LIST_NAME_MAX,K_EDIT)==-1) {
								if (check_exit(FALSE))
									break;
							}
							if(!uifc.changes)
								break;
							if(list_name_check(list, tmp, NULL, FALSE)) {
								uifc.helpbuf=	"`Entry Name Already Exists`\n\n"
												"An entry with that name already exists in the directory.\n"
												"Please choose a unique name.\n";
								uifc.msg("Entry Name Already Exists!");
								check_exit(FALSE);
								break;
							}
							listcount++;
							list[listcount]=list[listcount-1];
							list[listcount-1]=(struct bbslist *)malloc(sizeof(struct bbslist));
							memcpy(list[listcount-1],&defaults,sizeof(struct bbslist));
							list[listcount-1]->id=listcount-1;
							strcpy(list[listcount-1]->name,tmp);

							uifc.changes=0;
							list[listcount-1]->conn_type--;
							uifc.helpbuf=conn_type_help;
							if(uifc.list(WIN_SAV,0,0,0,&(list[listcount-1]->conn_type),NULL,"Connection Type",&(conn_types[1]))>=0) {
								list[listcount-1]->conn_type++;
								if(list[listcount-1]->conn_type!=CONN_TYPE_MODEM
									&& list[listcount-1]->conn_type!=CONN_TYPE_SERIAL
									&& list[listcount-1]->conn_type!=CONN_TYPE_SHELL
									) {
									/* Set the port too */
									j=conn_ports[list[listcount-1]->conn_type];
									if(j<1 || j>65535)
										j=list[listcount-1]->port;
									list[listcount-1]->port=j;
								}
								uifc.changes=1;
							}
							else {
								if (check_exit(FALSE))
									break;
							}

							if(uifc.changes) {
								uifc.changes=0;
								uifc.helpbuf=address_help;
								uifc.input(WIN_MID|WIN_SAV,0,0
									,list[listcount-1]->conn_type==CONN_TYPE_MODEM ? "Phone Number"
									:list[listcount-1]->conn_type==CONN_TYPE_SERIAL ? "Device Name"
									:list[listcount-1]->conn_type==CONN_TYPE_SHELL ? "Command"
									:"Address"
									,list[listcount-1]->addr,LIST_ADDR_MAX,K_EDIT);
								check_exit(FALSE);
							}
							if(quitting || !uifc.changes) {
								FREE_AND_NULL(list[listcount-1]);
								list[listcount-1]=list[listcount];
								listcount--;
							}
							else {
								add_bbs(settings.list_path,list[listcount-1]);
								load_bbslist(list, BBSLIST_SIZE, &defaults, settings.list_path, sizeof(settings.list_path), shared_list, sizeof(shared_list), &listcount, &opt, &bar, strdup(list[listcount-1]->name));
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
								check_exit(FALSE);
								break;
							}
							if(safe_mode) {
								uifc.helpbuf=	"`Cannot edit list in safe mode`\n\n"
												"SyncTERM is currently running in safe mode.  This means you cannot remove from the\n"
												"directory.";
								uifc.msg("Cannot edit list in safe mode");
								check_exit(FALSE);
								break;
							}
							if(list[opt]->type==SYSTEM_BBSLIST) {
								uifc.helpbuf=	"`Cannot delete from system list`\n\n"
												"This entry was loaded from the system-wide list and cannot be deleted.";
								uifc.msg("Cannot delete system list entries");
								check_exit(FALSE);
								break;
							}
							sprintf(str,"Delete %s?",list[opt]->name);
							i=0;
							if(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,NULL,str,YesNo)!=0)
								break;
							del_bbs(settings.list_path,list[opt]);
							load_bbslist(list, BBSLIST_SIZE, &defaults, settings.list_path, sizeof(settings.list_path), shared_list, sizeof(shared_list), &listcount, NULL, NULL, NULL);
							oldopt=-1;
							break;
						case MSK_EDIT:
							if(safe_mode) {
								uifc.helpbuf=	"`Cannot edit list in safe mode`\n\n"
												"SyncTERM is currently running in safe mode.  This means you cannot edit the\n"
												"directory.";
								uifc.msg("Cannot edit list in safe mode");
								check_exit(FALSE);
								break;
							}
							if(edit_list(list, list[opt],settings.list_path,FALSE)) {
								load_bbslist(list, BBSLIST_SIZE, &defaults, settings.list_path, sizeof(settings.list_path), shared_list, sizeof(shared_list), &listcount, &opt, &bar, strdup(list[opt]->name));
								oldopt=-1;
							}
							break;
					}
				}
				else {
					if(connected) {
						if(safe_mode) {
							uifc.helpbuf=	"`Cannot edit list in safe mode`\n\n"
											"SyncTERM is currently running in safe mode.  This means you cannot edit the\n"
											"directory.";
							uifc.msg("Cannot edit list in safe mode");
							check_exit(FALSE);
						}
						else if(edit_list(list, list[opt],settings.list_path,FALSE)) {
							load_bbslist(list, BBSLIST_SIZE, &defaults, settings.list_path, sizeof(settings.list_path), shared_list, sizeof(shared_list), &listcount, &opt, &bar, strdup(list[opt]->name));
							oldopt=-1;
						}
					}
					else {
						memcpy(&retlist,list[val],sizeof(struct bbslist));
						free_list(&list[0],listcount);
						free(list);
						return(&retlist);
					}
				}
			}
		}
		else {
			for(;at_settings && !quitting;) {
				uifc.helpbuf=	"`SyncTERM Settings Menu`\n\n"
								"~ Default Connection Settings ~\n"
								"        Modify the settings that are used by default for new entries\n\n"
#ifdef CONFIGURABLE_MOUSE_ACTIONS
								"~ Mouse Actions ~\n"
								"        This feature is not yet functional\n\n"
#endif
								"~ Current Screen Mode ~\n"
								"        Set the current screen size/mode\n\n"
								"~ Font Management ~\n"
								"        Configure additional font files\n\n"
								"~ Program Settings ~\n"
								"        Modify hardware and screen/video settings\n\n";
				if(oldopt != -2)
					settitle(syncterm_version);
				oldopt=-2;
				val=uifc.list(WIN_T2B|WIN_RHT|WIN_EXTKEYS|WIN_DYN|WIN_UNGETMOUSE|WIN_ACT|WIN_ESC
					,0,0,0,&sopt,&sbar,"SyncTERM Settings",connected?connected_settings_menu:settings_menu);
				if(connected && val >= 1)
					val++;
				switch(val) {
					case -2-0x3000:	/* ALT-B - Scrollback */
						if(!connected) {
							viewofflinescroll();
							uifc.list((listcount<MAX_OPTS?WIN_XTR:0)
								|WIN_ACT|WIN_INSACT|WIN_DELACT|WIN_SAV|WIN_ESC
								|WIN_T2B|WIN_INS|WIN_DEL|WIN_EDIT|WIN_EXTKEYS|WIN_DYN
								|WIN_SEL|WIN_INACT
								,0,0,0,&opt,&bar,list_title,(char **)list);
						}
						break;
					case -2-CIO_KEY_MOUSE:
						getmouse(&mevent);
						/* Fall-through */
					case -2-0x0f00:	/* Backtab */
					case -2-0x4b00:	/* Left Arrow */
					case -2-0x4d00:	/* Right Arrow */
					case -11:		/* TAB */
						uifc.list(WIN_T2B|WIN_RHT|WIN_EXTKEYS|WIN_DYN|WIN_ACT|WIN_SEL
							,0,0,0,&sopt,&sbar,"SyncTERM Settings",connected?connected_settings_menu:settings_menu);
						at_settings=!at_settings;
						break;
					case -1:		/* ESC */
						if (!connected)
							if (!check_exit(TRUE))
								continue;
						free_list(&list[0],listcount);
						free(list);
						return(NULL);
					case 0:			/* Edit default connection settings */
						edit_list(NULL, &defaults,settings.list_path,TRUE);
						break;
#ifdef CONFIGURABLE_MOUSE_ACTIONS
					case 1:			/* Mouse Actions setup */
						uifc.helpbuf=	"Mouse actions are not yet user conifurable."
										"This item is here to remind me to implement it.";
						uifc.msg("This section not yet functional");
						check_exit(FALSE);
						break;
#endif
					case 1: {		/* Screen Mode */
							struct text_info ti;
							gettextinfo(&ti);

							uifc.helpbuf=	"`Current Screen Mode`\n\n"
									"Change the current screen size/mode.\n";
							i=ti.currmode;
							i=ciolib_to_screen(ti.currmode);
							i--;
							j=i;
							i=uifc.list(WIN_SAV,0,0,0,&i,&j,"Screen Mode",screen_modes+1);
							if(i>=0) {
								i++;
								uifcbail();
								textmode(screen_to_ciolib(i));
								init_uifc(TRUE, TRUE);
								uifc.list((listcount<MAX_OPTS?WIN_XTR:0)
									|WIN_ACT|WIN_INSACT|WIN_DELACT|WIN_SAV|WIN_ESC
									|WIN_T2B|WIN_INS|WIN_DEL|WIN_EDIT|WIN_EXTKEYS|WIN_DYN
									|WIN_SEL|WIN_INACT
									,0,0,0,&opt,&bar,list_title,(char **)list);
							}
							else if (check_exit(FALSE)) {
								free_list(&list[0],listcount);
								free(list);
								return(NULL);
							}
						}
						break;
					case 2:			/* Font management */
						if(!safe_mode) {
							font_management();
							load_font_files();
						}
						break;
					case 3:			/* Program settings */
						change_settings(connected);
						load_bbslist(list, BBSLIST_SIZE, &defaults, settings.list_path, sizeof(settings.list_path), shared_list, sizeof(shared_list), &listcount, &opt, &bar, list[opt]?strdup(list[opt]->name):NULL);
						oldopt=-1;
						break;
				}
			}
		}
	}
}

cterm_emulation_t
get_emulation(struct bbslist *bbs)
{
	if (bbs == NULL)
		return CTERM_EMULATION_ANSI_BBS;

	switch(bbs->screen_mode) {
		case SCREEN_MODE_C64:
		case SCREEN_MODE_C128_40:
		case SCREEN_MODE_C128_80:
			return CTERM_EMULATION_PETASCII;
		case SCREEN_MODE_ATARI:
		case SCREEN_MODE_ATARI_XEP80:
			return CTERM_EMULATION_ATASCII;
		default:
			return CTERM_EMULATION_ANSI_BBS;
	}
}

const char *
get_emulation_str(cterm_emulation_t emu)
{
	switch (emu) {
		case CTERM_EMULATION_ANSI_BBS:
			return "syncterm";
		case CTERM_EMULATION_PETASCII:
			return "PETSCII";
		case CTERM_EMULATION_ATASCII:
			return "ATASCII";
	}
	return "none";
}

void
get_term_size(struct bbslist *bbs, int *cols, int *rows)
{
	int cmode = find_vmode(screen_to_ciolib(bbs->screen_mode));

	if (cmode < 0) {
		// This shouldn't happen, but if it does, make something up.
		*cols = 80;
		*rows = 24;
		return;
	}
	if(vparams[cmode].cols < 80) {
		if (cols)
			*cols=40;
	}
	else {
		if(vparams[cmode].cols < 132) {
			if (cols)
				*cols=80;
		}
		else {
			if (cols)
				*cols=132;
		}
	}
	if (rows) {
		*rows=vparams[cmode].rows;
		if(!bbs->nostatus)
			(*rows)--;
		if(*rows<24)
			*rows=24;
	}
}
