/* sbbs.h */

/* Synchronet class (sbbs_t) definition and exported function prototypes */

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

#ifndef _SBBS_H
#define _SBBS_H

/****************************/
/* Standard library headers */
/****************************/

/***************/
/* OS-specific */
/***************/
#if defined(_WIN32)			/* Windows */

	#include <io.h>
	#include <share.h>
	#include <windows.h>
	#include <process.h>	/* _beginthread() prototype */
	#include <direct.h>		/* _mkdir() prototype */
	#include <mmsystem.h>	/* SND_ASYNC */

	#if defined(_DEBUG) && defined(_MSC_VER)
		#include <crtdbg.h> /* Windows debug macros and stuff */
	#endif

#if defined(__cplusplus)
	extern "C"
#endif
	extern HINSTANCE hK32;

#elif defined(__unix__)		/* Unix-variant */

	#include <unistd.h>		/* close */

#endif

/******************/
/* ANSI C Library */
/******************/
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>			/* open */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#ifndef __unix__

	#include <malloc.h>

#endif

#include <sys/stat.h>

#ifdef JAVASCRIPT
	#ifdef __unix__
		#define XP_UNIX
	#else
		#define XP_PC
		#define XP_WIN
	#endif
	#include <jsapi.h>
	#include <jsprf.h>		/* JS-safe sprintf functions */
#endif

/***********************/
/* Synchronet-specific */
/***********************/
#ifdef SBBS	
	#include "threadwrap.h"	/* must be before dirwrap.h for OpenBSD FULLPATH */
	#include "text.h"
#endif
#include "genwrap.h"
#include "dirwrap.h"
#include "filewrap.h"
#include "sockwrap.h"
#include "smblib.h"
#include "ars_defs.h"
#include "scfgdefs.h"
#include "scfglib.h"
#include "userdat.h"
#include "riodefs.h"
#include "cmdshell.h"
#include "ringbuf.h"    /* RingBuf definition */
#include "client.h"		/* client_t definition */
#include "crc16.h"
#include "crc32.h"
#include "telnet.h"

/* Synchronet Node Instance class definition */
#ifdef __cplusplus
class sbbs_t
{

public:

	sbbs_t(ushort node_num, DWORD addr, char* host_name, SOCKET
		,scfg_t*, char* text[], client_t* client_info);
	~sbbs_t();

	bbs_startup_t*	startup;

	bool	init(void);
	bool	terminated;

	client_t client;
	SOCKET	client_socket;
	SOCKET	client_socket_dup;
	DWORD	client_addr;
	char	client_name[128];
	char	client_ident[128];
	DWORD	local_addr;

	scfg_t	cfg;

	int		outchar_esc;		   // track ANSI escape seq output

	int 	rioctl(ushort action); // remote i/o control
	bool	rio_abortable;

    RingBuf	inbuf;
    RingBuf	outbuf;
	HANDLE	input_thread;
	pthread_mutex_t	input_thread_mutex;
	bool	input_thread_mutex_locked;	// by someone other than the input_thread

	int 	outcom(uchar ch); 	   // send character
	int 	incom(unsigned long timeout=0);		   // receive character

	void	spymsg(char *msg);		// send message to active spies

	void	putcom(char *str, int len=0);  // Send string
	void	hangup(void);		   // Hangup modem


	void	send_telnet_cmd(uchar cmd, uchar opt);
    uchar	telnet_cmd[64];
    uint	telnet_cmdlen;
	ulong	telnet_mode;
	uchar	telnet_last_rxch;
	char	terminal[TELNET_TERM_MAXLEN+1];

	time_t	event_time;				// Time of next exclusive event
	char*	event_code;				// Internal code of next exclusive event
	bool	event_thread_running;
    bool	output_thread_running;
    bool	input_thread_running;

#ifdef JAVASCRIPT

	JSRuntime*	js_runtime;
	JSContext*	js_cx;
	JSObject*	js_glob;
	js_branch_t	js_branch;
	long		js_execfile(const char *fname);
	bool		js_init(void);
	void		js_create_user_objects(void);

#endif

	char 	menu_dir[128];	/* Over-ride default menu dir */
	char 	menu_file[128]; /* Over-ride menu file */

	user_t	useron; 		/* User currently online */
	node_t	thisnode;		/* Node information */
	smb_t	smb;			/* Currently open message base */
	char	rlogin_name[LEN_ALIAS+1];

	uint	temp_dirnum;

	FILE	*nodefile_fp,
			*node_ext_fp,
			*logfile_fp;

	int 	nodefile;		/* File handle for node.dab */
	int		node_ext;		/* File handle for node.exb */
	int 	inputfile;		/* File handle to use for input */

							/* Batch download queue */
	char 	**batdn_name;	/* Filenames */
	ushort	*batdn_alt; 	/* Alternate path */
	uint 	*batdn_dir, 	/* Directory for each file */
			 batdn_total;	/* Total files */
	long 	*batdn_offset;	/* Offset for data */
	ulong	*batdn_size;	/* Size of file in bytes */
	ulong	*batdn_cdt; 	/* Credit value of file */

							/* Batch upload queue */
	char 	**batup_desc,	/* Description for each file */
			**batup_name;	/* Filenames */
	long	*batup_misc;	/* Miscellaneous bits */
	ushort	*batup_alt; 	/* Alternate path */
	uint 	*batup_dir, 	/* Directory for each file */
			batup_total;	/* Total files */

	/*********************************/
	/* Color Configuration Variables */
	/*********************************/
	char 	*text[TOTAL_TEXT];			/* Text from ctrl\text.dat */
	char 	*text_sav[TOTAL_TEXT];		/* Text from ctrl\text.dat */
	char 	dszlog[127];	/* DSZLOG enviornment variable */
	int		keybuftop,keybufbot;	/* Keyboard input buffer pointers */
	char 	keybuf[KEY_BUFSIZE];	/* Keyboard input buffer */
	char *	connection;		/* Connection Description */
	ulong	cur_rate;		/* Current Connection (DCE) Rate */
	ulong	cur_cps;		/* Current Average Transfer CPS */
	ulong	dte_rate;		/* Current COM Port (DTE) Rate */
	time_t 	timeout;		/* User inactivity timeout reference */
	ulong 	timeleft_warn;	/* low timeleft warning flag */
	uchar 	curatr; 		/* Current Text Attributes Always */
	uchar	attr_stack[64];	/* Saved attributes (stack) */
	int 	attr_sp;		/* Attribute stack pointer */
	long 	lncntr; 		/* Line Counter - for PAUSE */
	long 	tos;			/* Top of Screen */
	long 	rows;			/* Current number of Rows for User */
	long	cols;			/* Current number of Columns for User */
	long 	autoterm;		/* Autodetected terminal type */
	char 	slbuf[SAVE_LINES][LINE_BUFSIZE+1]; /* Saved for redisplay */
	char 	slatr[SAVE_LINES];	/* Starting attribute of each line */
	char 	slcuratr[SAVE_LINES];	/* Ending attribute of each line */
	int 	slcnt;			/* Number of lines currently saved */
	char 	lbuf[LINE_BUFSIZE+1];/* Temp storage for each line output */
	int		lbuflen;		/* Number of characters in line buffer */
	char 	latr;			/* Starting attribute of line buffer */
	ulong	console;		/* Defines current Console settings */
	char 	wordwrap[81];	/* Word wrap buffer */
	time_t	now,			/* Used to store current time in Unix format */
			answertime, 	/* Time call was answered */
			logontime,		/* Time user logged on */
			starttime,		/* Time stamp to use for time left calcs */
			ns_time,		/* File new-scan time */
			last_ns_time;	/* Most recent new-file-scan this call */
	uchar 	action;			/* Current action of user */
	long 	online; 		/* Remote/Local or not online */
	long 	sys_status; 	/* System Status */
	subscan_t	*subscan;	/* User sub configuration/scan info */

	ulong	logon_ulb,		/* Upload Bytes This Call */
			logon_dlb,		/* Download Bytes This Call */
			logon_uls,		/* Uploads This Call */
			logon_dls,		/* Downloads This Call */
			logon_posts,	/* Posts This Call */
			logon_emails,	/* Emails This Call */
			logon_fbacks;	/* Feedbacks This Call */
	uchar	logon_ml;		/* ML of the user upon logon */

	uint 	main_cmds;		/* Number of Main Commands this call */
	uint 	xfer_cmds;		/* Number of Xfer Commands this call */
	ulong	posts_read; 	/* Number of Posts read this call */
	char 	temp_uler[31];  /* User who uploaded the files to temp dir */
	char 	temp_file[41];	/* Origin of extracted temp files */
	long 	temp_cdt;		/* Credit value of file that was extracted */
	char 	autohang;		/* Used for auto-hangup after transfer */
	size_t 	logcol; 		/* Current column of log file */
	uint 	criterrs; 		/* Critical error counter */

	uint 	curgrp; 		/* Current group */
	uint	*cursub;		/* Current sub-board for each group */
	uint	curlib; 		/* Current library */
	uint	*curdir;		/* Current directory for each library */
	uint 	*usrgrp;		/* Real group numbers */
	uint	usrgrps;		/* Number groups this user has access to */
	uint	usrgrp_total;	/* Total number of groups */
	uint 	*usrlib;		/* Real library numbers */
	uint	usrlibs;		/* Number of libs this user can access */
	uint	usrlib_total;	/* Total number of libraries */
	uint 	**usrsub;		/* Real sub numbers */
	uint	*usrsubs;		/* Num of subs with access for each grp */
	uint 	**usrdir;		/* Real dir numbers */
	uint	*usrdirs;		/* Num of dirs with access for each lib */
	uint	cursubnum;		/* For ARS */
	uint	curdirnum;		/* For ARS */
	ulong 	timeleft;		/* Number of seconds user has left online */
	char	sbbsnnum[MAX_PATH+1];	/* Environment var to contain node num */
	char	sbbsnode[MAX_PATH+1];	/* Environment var to contain node dir path */
	char	sbbsctrl[MAX_PATH+1];	/* Environment var to contain ctrl dir path */
	char	sbbsdata[MAX_PATH+1];	/* Environment var to contain data dir path */
	char	sbbsexec[MAX_PATH+1];	/* Environment var to contain exec dir path */
	char	env_day[32];			/* Environment var for day of month */
	char	env_weekday[32];		/* Environment var for name of weekday */
	char	env_month[32];			/* Environment var for month number (1-based) */
	char	env_monthname[32];		/* Environment var for day of month abbreviation */
	char	env_year[32];			/* Environment var for the year */

	char 	*comspec;		/* Pointer to environment variable COMSPEC */
	ushort	altul;			/* Upload to alternate path flag */
	char 	cid[LEN_CID+1]; /* Caller ID (IP Address) of current caller */
	char 	*noaccess_str;	/* Why access was denied via ARS */
	long 	noaccess_val;	/* Value of parameter not met in ARS */
	int		errorlevel; 	/* Error level of external program */

	csi_t	main_csi;		/* Main Command Shell Image */

	smbmsg_t*	current_msg;	/* For message header @-codes */

			/* Global command shell variables */
	uint	global_str_vars;
	char **	global_str_var;
	long *	global_str_var_name;
	uint	global_int_vars;
	long *	global_int_var;
	long *	global_int_var_name;
	char *	sysvar_p[MAX_SYSVARS];
	uint	sysvar_pi;
	long	sysvar_l[MAX_SYSVARS];
	uint	sysvar_li;

    /* ansi_term.cpp */
	char *	ansi(int atr);			/* Returns ansi escape sequence for atr */
    bool	ansi_getxy(int* x, int* y);
	void	ansi_getlines(void);

			/* Command Shell Methods */
	int		exec(csi_t *csi);
	int		exec_function(csi_t *csi);
	int		exec_misc(csi_t *csi, char *path);
	int		exec_net(csi_t *csi);
	int		exec_msg(csi_t *csi);
	int		exec_file(csi_t *csi);
	long	exec_bin(char *mod, csi_t *csi);
	void	clearvars(csi_t *bin);
	void	freevars(csi_t *bin);
	char**	getstrvar(csi_t *bin, long name);
	long*	getintvar(csi_t *bin, long name);
	char*	copystrvar(csi_t *csi, char *p, char *str);
	void	skipto(csi_t *csi, uchar inst);
	bool	ftp_cmd(csi_t* csi, SOCKET ctrl_sock, char* cmdsrc, char* rsp);
	bool	ftp_put(csi_t* csi, SOCKET ctrl_sock, char* src, char* dest);
	bool	ftp_get(csi_t* csi, SOCKET ctrl_sock, char* src, char* dest, bool dir=false);
	SOCKET	ftp_data_sock(csi_t* csi, SOCKET ctrl_sock, SOCKADDR_IN*);

	bool	select_shell(void);
	bool	select_editor(void);

	void	sys_info(void);
	void	user_info(void);
	void	xfer_policy(void);

	void	xfer_prot_menu(enum XFER_TYPE);
	void	node_stats(uint node_num);
	void	sys_stats(void);
	void	logonlist(void);
	bool	spy(uint node_num);

	void	reset_logon_vars(void);

	uint	finduser(char *str);

	int 	sub_op(uint subnum);
	ulong	getlastmsg(uint subnum, ulong *ptr, time_t *t);
	time_t	getmsgtime(uint subnum, ulong ptr);
	ulong	getmsgnum(uint subnum, time_t t);

	int		dir_op(uint dirnum);
	int		getuserxfers(int fromuser, int destuser, char *fname);

	void	getmsgptrs(void);
	void	putmsgptrs(void);
	void	getusrsubs(void);
	void	getusrdirs(void);
	uint	getusrsub(uint subnum);
	uint	getusrgrp(uint subnum);

	uint	userdatdupe(uint usernumber, uint offset, uint datlen, char *dat
				,bool del);
	void	gettimeleft(void);
	bool	gettimeleft_inside;

	/* str.cpp */
	char*	timestr(time_t *intime);
    char	timestr_output[60];
	void	userlist(long mode);
	size_t	gettmplt(char *outstr, char *tmplt, long mode);
	void	sif(char *fname, char *answers, long len);	/* Synchronet Interface File */
	void	sof(char *fname, char *answers, long len);
	void	create_sif_dat(char *siffile, char *datfile);
	void	read_sif_dat(char *siffile, char *datfile);
	void	printnodedat(uint number, node_t* node);
	bool	inputnstime(time_t *dt);
	bool	chkpass(char *pass, user_t* user, bool unique);
	char *	cmdstr(char *instr, char *fpath, char *fspec, char *outstr);
	char	cmdstr_output[512];

	void	subinfo(uint subnum);
	void	dirinfo(uint dirnum);
	bool	trashcan(char *insearch, char *name);
	void	time_bank(void);
	void	change_user(void);

	/* writemsg.cpp */
	void	automsg(void);
	bool	writemsg(char *str, char *top, char *title, long mode, int subnum
				,char *dest);
	char	putmsg(char HUGE16 *str, long mode);
	bool	msgabort(void);
	bool	email(int usernumber, char *top, char *title, long mode);
	void	forwardmail(smbmsg_t* msg, int usernum);
	void	removeline(char *str, char *str2, char num, char skip);
	ulong	msgeditor(char *buf, char *top, char *title);
	void	editfile(char *path);
	int		loadmsg(smbmsg_t *msg, ulong number);
	ushort	chmsgattr(ushort attr);
	void	show_msgattr(ushort attr);
	void	show_msghdr(smbmsg_t* msg);
	void	show_msg(smbmsg_t* msg, long mode);
	void	msgtotxt(smbmsg_t* msg, char *str, int header, int tails);
	void	quotemsg(smbmsg_t* msg, int tails);
	void	putmsg_fp(FILE *fp, long length, long mode);
	void	editmsg(smbmsg_t* msg, uint subnum);
	void	editor_inf(int xeditnum,char *dest, char *title, long mode
				,uint subnum);
	void	copyfattach(uint to, uint from, char *title);
	bool	movemsg(smbmsg_t* msg, uint subnum);

	/* postmsg.cpp */
	bool	postmsg(uint subnum, smbmsg_t* msg, long wm_mode);

	/* mail.cpp */
	int		delmail(uint usernumber,int which);
	void	telluser(smbmsg_t* msg);
	void	delallmail(uint usernumber);

	/* getmsg.cpp */
	post_t* loadposts(long *posts, uint subnum, ulong ptr, long mode);

	/* readmail.cpp */
	void	readmail(uint usernumber, int sent);

	/* bulkmail.cpp */
	bool	bulkmail(uchar *ar);
	int		bulkmailhdr(smb_t*, smbmsg_t*, uint usernum);

	/* con_out.cpp */
	int		bputs(char *str);				/* BBS puts function */
	int		rputs(char *str);				/* BBS raw puts function */
	int		bprintf(char *fmt, ...);		/* BBS printf function */
	int		rprintf(char *fmt, ...);		/* BBS raw printf function */
	void	outchar(char ch);				/* Output a char - check echo and emu.  */
	void	center(char *str);
	void	clearline(void);
	void	cleartoeol(void);
	void	cursor_home(void);
	void	cursor_up(int count=1);
	void	cursor_down(int count=1);
	void	cursor_left(int count=1);
	void	cursor_right(int count=1);

	/* getstr.cpp */
	size_t	getstr_offset;
	size_t	getstr(char *str, size_t length, long mode);
	long	getnum(ulong max);
	void	insert_indicator(void);

	/* getkey.cpp */
	char	getkey(long mode); 		/* Waits for a key hit local or remote  */
	long	getkeys(char *str, ulong max);
	void	ungetkey(char ch);		/* Places 'ch' into the input buffer    */
	char	question[128];
	bool	yesno(char *str);
	bool	noyes(char *str);
	void	pause(void);
	char *	mnestr;
	void	mnemonics(char *str);

	/* inkey.cpp */
	char	inkey(long mode, unsigned long timeout=0);
	char	handle_ctrlkey(char ch, long mode=0);

	/* prntfile.cpp */
	void	printfile(char *str, long mode);
	void	printtail(char *str, int lines, long mode);
	void	menu(const char *code);

	int		uselect(int add, uint n, char *title, char *item, uchar *ar);
	uint	uselect_total, uselect_num[500];

	void	riosync(char abortable);
	void	redrwstr(char *strin, int i, int l, long mode);
	void	attr(int atr);				/* Change local and remote text attributes */
	void	ctrl_a(char x);			/* Peforms the Ctrl-Ax attribute changes */

	/* atcodes.cpp */
	int		show_atcode(char *code);
	char*	atcode(char* sp, char* str);

	/* getnode.cpp */
	int		getsmsg(int usernumber);
	int		getnmsg(void);
	int		whos_online(bool listself);/* Lists active nodes, returns active nodes */
	void	nodelist(void);
	int		getnodeext(uint number, char * str);
	int		getnodedat(uint number, node_t * node, bool lock);
	void	nodesync(void);
	user_t	nodesync_user;
	bool	nodesync_inside;

	/* putnode.cpp */
	int		putnodedat(uint number, node_t * node);
	int		putnodeext(uint number, char * str);

	/* logonoff.cpp */
	bool	answer();
	int		login(char *str, char *pw);
	bool	logon(void);
	void	logout(void);
	void	logoff(void);
	BOOL	newuser(void);					/* Get new user							*/
	void	backout(void);

	/* readmsgs.cpp */
	int		scanposts(uint subnum, long mode, char *find);	/* Scan sub-board */
	int		searchsub(uint subnum, char *search);	/* Search for string on sub */
	int		searchsub_toyou(uint subnum);
	int		text_sec(void);						/* Text sections */
	void	listmsgs(int subnum, post_t * post, long i, long posts);
	void	msghdr(smbmsg_t* msg);
	int		searchposts(uint subnum, post_t * post, long start, long msgs
				,char *search);
	void	showposts_toyou(post_t * post, ulong start, long posts);

	/* chat.cpp */
	void	chatsection(void);
	void	multinodechat(int channel=1);
	void	nodepage(void);
	void	nodemsg(void);
	uint	nodemsg_inside;
	uint	hotkey_inside;
	uchar	lastnodemsg;	/* Number of node last message was sent to */
	char	lastnodemsguser[LEN_ALIAS+1];
	void	guruchat(char* line, char* guru, int gurunum, char* last_answer);
	bool	guruexp(char **ptrptr, char *line);
	void	localguru(char *guru, int gurunum);
	bool	sysop_page(void);
	bool	guru_page(void);
	void	privchat(bool local=false);
	bool	chan_access(uint cnum);
	int		getnodetopage(int all, int telegram);

	/* main.cpp */
	void	printstatslog(uint node);
	ulong	logonstats(void);
	void	logoffstats(void);
	int		nopen(char *str, int access);
	int		mv(char *src, char *dest, char copy); /* fast file move/copy function */
	bool	chksyspass(void);
	bool	chk_ar(uchar * str, user_t * user); /* checks access requirements */
	bool	ar_exp(uchar ** ptrptr, user_t * user);

	/* upload.cpp */
	bool	uploadfile(file_t* f);
	char	sbbsfilename[128],sbbsfiledesc[128]; /* env vars */
	bool	upload(uint dirnum);
    char	upload_lastdesc[LEN_FDESC+1];
	bool	bulkupload(uint dirnum);

	/* download.cpp */
	void	downloadfile(file_t* f);
	void	notdownloaded(ulong size, time_t start, time_t end);
	int		protocol(prot_t* prot, enum XFER_TYPE, char *fpath, char *fspec, bool cd);
	char*	protcmdline(prot_t* prot, enum XFER_TYPE type);
	void	seqwait(uint devnum);
	void	autohangup(void);
	bool	checkdszlog(file_t*);
	bool	checkprotresult(prot_t*, int error, file_t*);

	/* file.cpp */
	void	fileinfo(file_t* f);
	void	openfile(file_t* f);
	void	closefile(file_t* f);
	bool	removefcdt(file_t* f);
	bool	movefile(file_t* f, int newdir);
	char *	getfilespec(char *str);
	bool	checkfname(char *fname);
	bool	addtobatdl(file_t* f);

	/* listfile.cpp */
	bool	listfile(char *fname, char HUGE16 *buf, uint dirnum
				,char *search, char letter, ulong datoffset);
	int		listfiles(uint dirnum, char *filespec, int tofile, long mode);
	int		listfileinfo(uint dirnum, char *filespec, long mode);
	void	listfiletofile(char *fname, char HUGE16 *buf, uint dirnum, int file);
	int		batchflagprompt(uint dirnum, file_t bf[], uint total, long totalfiles);

	/* bat_xfer.cpp */
	void	batchmenu(void);
	void	batch_create_list(void);
	void	batch_add_list(char *list);
	bool	create_batchup_lst(void);
	bool	create_batchdn_lst(void);
	bool	create_bimodem_pth(void);
	void	batch_upload(void);
	void	batch_download(int xfrprot);
	BOOL	start_batch_download(void);

	/* tmp_xfer.cpp */
	void	temp_xfer(void);
	void	extract(uint dirnum);
	char *	temp_cmd(void);					/* Returns temp file command line */
	ulong	create_filelist(char *name, long mode);

	/* viewfile.cpp */
	int		viewfile(file_t* f, int ext);
	void	viewfiles(uint dirnum, char *fspec);
	void	viewfilecontents(file_t* f);

	/* sortdir.cpp */
	void	resort(uint dirnum);

	/* xtrn.cpp */
	int		external(const char* cmdline, long mode, const char* startup_dir=NULL);

	/* xtrn_sec.cpp */
	int		xtrn_sec(void);					/* The external program section  */
	void	xtrndat(char* name, char* dropdir, uchar type, ulong tleft
				,ulong misc);
	bool	exec_xtrn(uint xtrnnum);			/* Executes online external program */
	bool	user_event(user_event_t);			/* Executes user event(s) */
	char	xtrn_access(uint xnum);			/* Does useron have access to xtrn? */
	void	moduserdat(uint xtrnnum);

	/* logfile.cpp */
	void	logentry(char *code,char *entry);
	void	log(char *str);				/* Writes 'str' to node log */
	void	logch(char ch, bool comma);	/* Writes 'ch' to node log */
	void	logline(char *code,char *str); /* Writes 'str' on it's own line in log */
	void	logofflist(void);              /* List of users logon activity */
	bool	syslog(char* code, char *entry);
	void	errorlog(char *text);			/* Logs errors to ERROR.LOG and NODE.LOG */
	bool	errorlog_inside;
	bool	errormsg_inside;
	void	errormsg(int line, const char *file, char action, const char *object
				,ulong access, const char *extinfo=NULL);
	
	/* qwk.cpp */
	bool	qwklogon;
	ulong	qwkmail_last;
	void	qwk_sec(void);
	int		qwk_route(char *inaddr, char *fulladdr);
	void	update_qwkroute(char *via);
	void	qwk_success(ulong msgcnt, char bi, char prepack);
	void	qwksetptr(uint subnum, char *buf, int reset);
	void	qwkcfgline(char *buf,uint subnum);
	int		set_qwk_flag(ulong flag);

	/* pack_qwk.cpp */
	bool	pack_qwk(char *packet, ulong *msgcnt, bool prepack);

	/* un_qwk.cpp */
	bool	unpack_qwk(char *packet,uint hubnum);

	/* pack_rep.cpp */
	bool	pack_rep(uint hubnum);

	/* un_rep.cpp */
	bool	unpack_rep(char* repfile=NULL);

	/* msgtoqwk.cpp */
	ulong	msgtoqwk(smbmsg_t* msg, FILE *qwk_fp, long mode, int subnum, int conf);

	/* qwktomsg.cpp */
	bool	qwktomsg(FILE *qwk_fp, char *hdrblk, char fromhub, uint subnum
				,uint touser);

	/* fido.cpp */
	bool	netmail(char *into, char *subj, long mode);
	void	qwktonetmail(FILE *rep, char *block, char *into, uchar fromhub);
	bool	lookup_netuser(char *into);

	bool	inetmail(char *into, char *subj, long mode);
	bool	qnetmail(char *into, char *subj, long mode);

	/* useredit.cpp */
	void	useredit(int usernumber);
	int		searchup(char *search,int usernum);
	int		searchdn(char *search,int usernum);
	void	maindflts(user_t* user);
	void	purgeuser(int usernumber);

	/* ver.cpp */
	void	ver(void);

	/* scansubs.cpp */
	void	scansubs(long mode);
	void	scanallsubs(long mode);
	void	new_scan_cfg(ulong misc);
	void	new_scan_ptr_cfg(void);

	/* scandirs.cpp */
	void	scanalldirs(long mode);
	void	scandirs(long mode);

	#define nosound()
	#define checkline()

	void	catsyslog(int crash);

	/* telgate.cpp */
	void	telnet_gate(char* addr, ulong mode);	// See TG_* for mode bits

};

#endif /* __cplusplus */

#ifdef DLLEXPORT
#undef DLLEXPORT
#endif
#ifdef DLLCALL
#undef DLLCALL
#endif
#ifdef _WIN32
	#ifdef SBBS_EXPORTS
		#define DLLEXPORT	__declspec(dllexport)
	#else
		#define DLLEXPORT	__declspec(dllimport)
	#endif
	#ifdef __BORLANDC__
		#define DLLCALL __stdcall
	#else
		#define DLLCALL
	#endif
#else	/* !_WIN32 */
	#define DLLEXPORT
	#define DLLCALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

	/* getstats.c */
	DLLEXPORT BOOL		DLLCALL getstats(scfg_t* cfg, char node, stats_t* stats);
	DLLEXPORT ulong		DLLCALL	getposts(scfg_t* cfg, uint subnum);
	DLLEXPORT long		DLLCALL getfiles(scfg_t* cfg, uint dirnum);

	/* getmail.c */
	DLLEXPORT int		DLLCALL getmail(scfg_t* cfg, int usernumber, BOOL sent);
	DLLEXPORT mail_t *	DLLCALL loadmail(smb_t* smb, ulong* msgs, uint usernumber
										,int which, long mode);
	DLLEXPORT void		DLLCALL freemail(mail_t* mail);
	DLLEXPORT void		DLLCALL delfattach(scfg_t*, smbmsg_t*);

	/* postmsg.cpp */
	DLLEXPORT int		DLLCALL savemsg(scfg_t* cfg, smb_t* smb, smbmsg_t* msg, char* msgbuf);
	DLLEXPORT void		DLLCALL signal_sub_sem(scfg_t* cfg, uint subnum);

	/* filedat.c */
	DLLEXPORT BOOL		DLLCALL getfileixb(scfg_t* cfg, file_t* f);
	DLLEXPORT BOOL		DLLCALL getfiledat(scfg_t* cfg, file_t* f);
	DLLEXPORT BOOL		DLLCALL putfiledat(scfg_t* cfg, file_t* f);
	DLLEXPORT void		DLLCALL putextdesc(scfg_t* cfg, uint dirnum, ulong datoffset, char *ext);
	DLLEXPORT void		DLLCALL getextdesc(scfg_t* cfg, uint dirnum, ulong datoffset, char *ext);
	DLLEXPORT char*		DLLCALL getfilepath(scfg_t* cfg, file_t* f, char* path);

	DLLEXPORT BOOL		DLLCALL removefiledat(scfg_t* cfg, file_t* f);
	DLLEXPORT BOOL		DLLCALL addfiledat(scfg_t* cfg, file_t* f);
	DLLEXPORT BOOL		DLLCALL findfile(scfg_t* cfg, uint dirnum, char *filename);
	DLLEXPORT char *	DLLCALL padfname(char *filename, char *str);
	DLLEXPORT char *	DLLCALL unpadfname(char *filename, char *str);
	DLLEXPORT BOOL		DLLCALL rmuserxfers(scfg_t* cfg, int fromuser, int destuser, char *fname);

	DLLEXPORT int		DLLCALL update_uldate(scfg_t* cfg, file_t* f);

	/* str_util.c */
	DLLEXPORT char *	DLLCALL truncsp(char* str);
	DLLEXPORT char *	DLLCALL truncstr(char* str, const char* set);
	DLLEXPORT char *	DLLCALL ascii_str(uchar* str);
	DLLEXPORT BOOL		DLLCALL findstr(char *insearch, char *fname);
	DLLEXPORT BOOL		DLLCALL trashcan(scfg_t* cfg, char *insearch, char *name);
	DLLEXPORT char *	DLLCALL strip_exascii(char *str);
	DLLEXPORT char *	DLLCALL prep_file_desc(char *str);
	DLLEXPORT char *	DLLCALL strip_ctrl(char *str);
	DLLEXPORT char *	DLLCALL net_addr(net_t* net);
	DLLEXPORT BOOL		DLLCALL validattr(char a);
	DLLEXPORT size_t	DLLCALL strip_invalid_attr(char *str);
	DLLEXPORT ushort	DLLCALL subject_crc(char *subj);
	DLLEXPORT char *	DLLCALL ftn_msgid(sub_t* sub, smbmsg_t* msg);
	DLLEXPORT char *	DLLCALL get_msgid(scfg_t* cfg, uint subnum, smbmsg_t* msg);
	DLLEXPORT BOOL		DLLCALL get_msg_by_id(scfg_t* scfg, smb_t* smb, char* id, smbmsg_t* msg);
	DLLEXPORT char *	DLLCALL ultoac(ulong l,char *str);
	DLLEXPORT char *	DLLCALL rot13(char* str);


	/* date_str.c */
	DLLEXPORT char *	DLLCALL zonestr(short zone);
	DLLEXPORT time_t	DLLCALL dstrtounix(scfg_t*, char *str);	
	DLLEXPORT char *	DLLCALL unixtodstr(scfg_t*, time_t, char *str);
	DLLEXPORT char *	DLLCALL sectostr(uint sec, char *str);		
	DLLEXPORT char *	DLLCALL hhmmtostr(scfg_t* cfg, struct tm* tm, char* str);
	DLLEXPORT char *	DLLCALL timestr(scfg_t* cfg, time_t *intime, char* str);
	DLLEXPORT when_t	DLLCALL rfc822date(char* p);
	DLLEXPORT char *	DLLCALL msgdate(when_t when, char* buf);

	/* load_cfg.c */
	DLLEXPORT BOOL		DLLCALL load_cfg(scfg_t* cfg, char* text[], BOOL prep, char* error);
	DLLEXPORT void		DLLCALL free_cfg(scfg_t* cfg);
	DLLEXPORT void		DLLCALL free_text(char* text[]);

	/* scfgsave.c */
	DLLEXPORT BOOL		DLLCALL save_cfg(scfg_t* cfg, int backup_level);
	DLLEXPORT BOOL		DLLCALL write_node_cfg(scfg_t* cfg, int backup_level);
	DLLEXPORT BOOL		DLLCALL write_main_cfg(scfg_t* cfg, int backup_level);
	DLLEXPORT BOOL		DLLCALL write_msgs_cfg(scfg_t* cfg, int backup_level);
	DLLEXPORT BOOL		DLLCALL write_file_cfg(scfg_t* cfg, int backup_level);
	DLLEXPORT BOOL		DLLCALL write_chat_cfg(scfg_t* cfg, int backup_level);
	DLLEXPORT BOOL		DLLCALL write_xtrn_cfg(scfg_t* cfg, int backup_level);
	DLLEXPORT BOOL		DLLCALL fcopy(char* src, char* dest);
	DLLEXPORT BOOL		DLLCALL backup(char *org, int backup_level, BOOL ren);
	DLLEXPORT void		DLLCALL refresh_cfg(scfg_t* cfg);
	

	/* scfglib1.c */
	DLLEXPORT char *	DLLCALL prep_dir(char* base, char* dir, size_t buflen);

	/* logfile.cpp */
	DLLEXPORT BOOL		DLLCALL hacklog(scfg_t* cfg, char* prot, char* user, char* text 
										,char* host, SOCKADDR_IN* addr);
	DLLEXPORT BOOL		DLLCALL spamlog(scfg_t* cfg, char* prot, char* action, char* reason
										,char* host, char* ip_addr, char* to, char* from);

	DLLEXPORT char *	DLLCALL remove_ctrl_a(char* instr, char* outstr);

	/* data_ovl.cpp */
	DLLEXPORT BOOL		DLLCALL getmsgptrs(scfg_t* cfg, uint usernumber, subscan_t* subscan);
	DLLEXPORT BOOL		DLLCALL putmsgptrs(scfg_t* cfg, uint usernumber, subscan_t* subscan);

	/* sockopt.c */
	DLLEXPORT int		DLLCALL sockopt(char* str, int* level);
	DLLEXPORT int		DLLCALL set_socket_options(scfg_t* cfg, SOCKET sock, char* error);

	/* xtrn.cpp */
	DLLEXPORT char*		DLLCALL cmdstr(scfg_t* cfg, user_t* user, const char* instr
									,const char* fpath, const char* fspec, char* cmd);

#ifdef JAVASCRIPT

	typedef struct {
		const char*		name;
		JSNative        call;
		uint8           nargs;
		int				type;		/* return type */
		const char*		args;		/* arguments */
		const char*		desc;		/* description */
	} jsMethodSpec;

	#define JSTYPE_ARRAY JSTYPE_LIMIT
	#define JSTYPE_ALIAS JSTYPE_LIMIT+1

	#ifdef _DEBUG	/* String compiled into debug build only, for JS documentation generation */
		#define	JSDOCSTR(s)	s
	#else
		#define JSDOCSTR(s)	""
	#endif

	/* main.cpp */
	DLLEXPORT JSBool	DLLCALL js_DescribeObject(JSContext* cx, JSObject* obj, const char*);
	DLLEXPORT JSBool	DLLCALL js_DescribeConstructor(JSContext* cx, JSObject* obj, const char*);
	DLLEXPORT JSBool	DLLCALL js_DefineMethods(JSContext* cx, JSObject* obj, jsMethodSpec *fs, BOOL append);
	DLLEXPORT JSBool	DLLCALL js_CreateArrayOfStrings(JSContext* cx, JSObject* parent
														,const char* name, char* str[], uintN flags);

	/* js_global.c */
	DLLEXPORT JSObject* DLLCALL js_CreateGlobalObject(JSContext* cx, scfg_t* cfg, jsMethodSpec* methods);

	/* js_internal.c */
	DLLEXPORT JSObject* DLLCALL js_CreateInternalJsObject(JSContext* cx, JSObject* parent, js_branch_t* branch);

	/* js_system.c */
	DLLEXPORT JSObject* DLLCALL js_CreateSystemObject(JSContext* cx, JSObject* parent
													,scfg_t* cfg, time_t uptime, char* host_name);

	/* js_client.c */
	DLLEXPORT JSObject* DLLCALL js_CreateClientObject(JSContext* cx, JSObject* parent
													,char* name, client_t* client, SOCKET sock);
	/* js_user.c */
	DLLEXPORT JSObject*	DLLCALL js_CreateUserClass(JSContext* cx, JSObject* parent, scfg_t* cfg);
	DLLEXPORT JSObject* DLLCALL js_CreateUserObject(JSContext* cx, JSObject* parent, scfg_t* cfg
													,char* name, uint usernumber);
	DLLEXPORT JSBool	DLLCALL js_CreateUserObjects(JSContext* cx, JSObject* parent, scfg_t* cfg
													,user_t* user, char* html_index_file
													,subscan_t* subscan);
	/* js_file_area.c */
	DLLEXPORT JSObject* DLLCALL js_CreateFileAreaObject(JSContext* cx, JSObject* parent, scfg_t* cfg 
													,user_t* user, char* html_index_file);

	/* js_msg_area.c */
	DLLEXPORT JSObject* DLLCALL js_CreateMsgAreaObject(JSContext* cx, JSObject* parent, scfg_t* cfg
													,user_t* user, subscan_t* subscan);
	DLLEXPORT BOOL		DLLCALL js_CreateMsgAreaProperties(JSContext* cx, scfg_t* cfg
													,JSObject* subobj, uint subnum);

	/* js_xtrn_area.c */
	DLLEXPORT JSObject* DLLCALL js_CreateXtrnAreaObject(JSContext* cx, JSObject* parent, scfg_t* cfg
													,user_t* user);

	/* js_msgbase.c */
	DLLEXPORT JSObject* DLLCALL js_CreateMsgBaseClass(JSContext* cx, JSObject* parent, scfg_t* cfg);

	/* js_socket.c */
	DLLEXPORT JSObject* DLLCALL js_CreateSocketClass(JSContext* cx, JSObject* parent);
	DLLEXPORT JSObject* DLLCALL js_CreateSocketObject(JSContext* cx, JSObject* parent
													,char *name, SOCKET sock);

	/* js_file.c */
	DLLEXPORT JSObject* DLLCALL js_CreateFileClass(JSContext* cx, JSObject* parent);

	/* js_console.cpp */
	JSObject* js_CreateConsoleObject(JSContext* cx, JSObject* parent);

	/* js_bbs.cpp */
	JSObject* js_CreateBbsObject(JSContext* cx, JSObject* parent);

#endif

/* str_util.c */
int		bstrlen(char *str);
void	backslashcolon(char *str);
ulong	ahtoul(char *str);	/* Converts ASCII hex to ulong */
char *	hexplus(uint num, char *str); 	/* Hex plus for 3 digits up to 9000 */
uint	hptoi(char *str);
int		pstrcmp(char **str1, char **str2);  /* Compares pointers to pointers */
int		strsame(char *str1, char *str2);	/* Compares number of same chars */

/* nopen.c */
int		nopen(const char* str, int access);
FILE *	fnopen(int* file, const char* str, int access);
BOOL	ftouch(const char* fname);

/* load_cfg.c */
BOOL 	md(char *path);

#ifdef SBBS /* These aren't exported */

	/* main.c */
	int 	lprintf(char *fmt, ...);	/* telnet log */
	SOCKET	open_socket(int type);
	SOCKET	accept_socket(SOCKET s, SOCKADDR* addr, socklen_t* addrlen);
	int		close_socket(SOCKET);
	u_long	resolve_ip(char *addr);

	char *	readtext(long *line, FILE *stream);
	int 	eprintf(char *fmt, ...);	/* event log */
	int 	lputs(char *);				/* telnet log */

	/* ver.cpp */
	char*	socklib_version(char* str);

	/* sortdir.cpp */
	int		fnamecmp_a(char **str1, char **str2);	 /* for use with resort() */
	int		fnamecmp_d(char **str1, char **str2);
	int		fdatecmp_a(uchar **buf1, uchar **buf2);
	int		fdatecmp_d(uchar **buf1, uchar **buf2);

	/* file.cpp */
	BOOL	filematch(char *filename, char *filespec);

#endif /* SBBS */

extern const char* wday[];	/* abbreviated weekday names */
extern const char* mon[];	/* abbreviated month names */
extern char lastuseron[LEN_ALIAS+1];  /* Name of user last online */

#ifdef __cplusplus
}
#endif

/* Global data */

#if defined(__FLAT__) || defined(_WIN32)

#define lread(f,b,l) read(f,b,l)
#define lfread(b,l,f) fread(b,l,f)
#define lwrite(f,b,l) write(f,b,l)
#define lfwrite(b,l,f) fwrite(b,l,f)

#define lkbrd(x)	0

#endif

#endif	/* Don't add anything after this line */
