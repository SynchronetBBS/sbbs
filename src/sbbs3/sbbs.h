/* Synchronet class (sbbs_t) definition and exported function prototypes */
// vi: tabstop=4
/* $Id$ */

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

#ifndef _SBBS_H
#define _SBBS_H

/****************************/
/* Standard library headers */
/****************************/

/***************/
/* OS-specific */
/***************/
#if defined(_WIN32)			/* Windows */

	#define NOCRYPT     /* Stop windows.h from loading wincrypt.h */
                    /* Is windows.h REALLY necessary?!?! */
	#define WIN32_LEAN_AND_MEAN
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
	HINSTANCE hK32;

#elif defined(__unix__)		/* Unix-variant */

	#include <unistd.h>		/* close */

#endif

#ifdef _THREAD_SUID_BROKEN
extern int	thread_suid_broken;			/* NPTL is no longer broken */
#else
#define thread_suid_broken FALSE
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

#ifdef __unix__
	#define XP_UNIX
#else
	#define XP_PC
	#define XP_WIN
#endif

#if defined(JAVASCRIPT)
#include "comio.h"			/* needed for COM_HANDLE definition only */
#if __GNUC__ > 5
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wmisleading-indentation"
	#pragma GCC diagnostic ignored "-Wignored-attributes"
#endif
#include <jsversion.h>
#include <jsapi.h>
#if __GNUC_ > 5
	#pragma GCC diagnostic pop
#endif
#define JS_DestroyScript(cx,script)

#define JSSTRING_TO_RASTRING(cx, str, ret, sizeptr, lenptr) \
{ \
	size_t			*JSSTSlenptr=(lenptr); \
	size_t			JSSTSlen; \
	size_t			JSSTSpos; \
	const jschar	*JSSTSstrval; \
	char			*JSSTStmpptr; \
\
	if(JSSTSlenptr==NULL) \
		JSSTSlenptr=&JSSTSlen; \
	if((str) != NULL) { \
		if((JSSTSstrval=JS_GetStringCharsAndLength((cx), (str), JSSTSlenptr))) { \
			if((*(sizeptr) < (*JSSTSlenptr+1 )) || (ret)==NULL) { \
				*(sizeptr) = *JSSTSlenptr+1; \
				if((JSSTStmpptr=(char *)realloc((ret), *(sizeptr)))==NULL) { \
					JS_ReportError(cx, "Error reallocating %lu bytes at %s:%d", (*JSSTSlenptr)+1, getfname(__FILE__), __LINE__); \
					if((ret) != NULL) free(ret); \
					(ret)=NULL; \
				} \
				else { \
					(ret)=JSSTStmpptr; \
				} \
			} \
			if(ret) { \
				for(JSSTSpos=0; JSSTSpos<*JSSTSlenptr; JSSTSpos++) \
					(ret)[JSSTSpos]=(char)JSSTSstrval[JSSTSpos]; \
				(ret)[*JSSTSlenptr]=0; \
			} \
		} \
	} \
	else { \
		if(ret) \
			*(ret)=0; \
	} \
}

#define JSVALUE_TO_RASTRING(cx, val, ret, sizeptr, lenptr) \
{ \
	JSString	*JSVTSstr=JS_ValueToString((cx), (val)); \
	JSSTRING_TO_RASTRING((cx), JSVTSstr, (ret), (sizeptr), (lenptr)); \
}

#define JSSTRING_TO_MSTRING(cx, str, ret, lenptr) \
{ \
	size_t			*JSSTSlenptr=(lenptr); \
	size_t			JSSTSlen; \
	size_t			JSSTSpos; \
	const jschar	*JSSTSstrval; \
\
	if(JSSTSlenptr==NULL) \
		JSSTSlenptr=&JSSTSlen; \
	(ret)=NULL; \
	if((str) != NULL) { \
		if((JSSTSstrval=JS_GetStringCharsAndLength((cx), (str), JSSTSlenptr))) { \
			if(((ret)=(char *)malloc(*JSSTSlenptr+1))) { \
				for(JSSTSpos=0; JSSTSpos<*JSSTSlenptr; JSSTSpos++) \
					(ret)[JSSTSpos]=(char)JSSTSstrval[JSSTSpos]; \
				(ret)[*JSSTSlenptr]=0; \
			} \
			else JS_ReportError((cx), "Error allocating %lu bytes at %s:%d", (*JSSTSlenptr)+1, getfname(__FILE__), __LINE__); \
		} \
	} \
}

#define JSVALUE_TO_MSTRING(cx, val, ret, lenptr) \
{ \
	JSString	*JSVTSstr=JS_ValueToString((cx), (val)); \
	JSSTRING_TO_MSTRING((cx), JSVTSstr, (ret), lenptr); \
}

#define JSSTRING_TO_STRBUF(cx, str, ret, bufsize, lenptr) \
{ \
	size_t			*JSSTSlenptr=(lenptr); \
	size_t			JSSTSlen; \
	size_t			JSSTSpos; \
	const jschar	*JSSTSstrval; \
\
	if(JSSTSlenptr==NULL) \
		JSSTSlenptr=&JSSTSlen; \
	if((bufsize) < 1 || (str)==NULL) \
		*JSSTSlenptr = 0; \
	else { \
		if((JSSTSstrval=JS_GetStringCharsAndLength((cx), (str), JSSTSlenptr))) { \
			if(*JSSTSlenptr >= (bufsize)) \
				*JSSTSlenptr = (bufsize)-1; \
			for(JSSTSpos=0; JSSTSpos<*JSSTSlenptr; JSSTSpos++) \
				(ret)[JSSTSpos]=(char)JSSTSstrval[JSSTSpos]; \
		} \
		else \
			*JSSTSlenptr=0; \
	} \
	(ret)[*JSSTSlenptr]=0; \
}

#define JSVALUE_TO_STRBUF(cx, val, ret, bufsize, lenptr) \
{ \
	JSString	*JSVTSstr=JS_ValueToString((cx), (val)); \
	JSSTRING_TO_STRBUF((cx), JSVTSstr, (ret), (bufsize), lenptr); \
}

#define HANDLE_PENDING(cx, p ) \
	if(JS_IsExceptionPending(cx)) { \
		if(p != NULL) \
			free(p); \
		return JS_FALSE; \
	}

#define JSSTRING_TO_ASTRING(cx, str, ret, maxsize, lenptr) \
{ \
	size_t			*JSSTSlenptr=(lenptr); \
	size_t			JSSTSlen; \
	size_t			JSSTSpos; \
	const jschar	*JSSTSstrval; \
\
	if(JSSTSlenptr==NULL) \
		JSSTSlenptr=&JSSTSlen; \
	(ret)=NULL; \
	if((str) != NULL) { \
		if((JSSTSstrval=JS_GetStringCharsAndLength((cx), (str), JSSTSlenptr))) { \
			if(*JSSTSlenptr >= (maxsize)) { \
				*JSSTSlenptr = (maxsize)-1; \
			} \
			if(((ret)=(char *)alloca(*JSSTSlenptr+1))) { \
				for(JSSTSpos=0; JSSTSpos<*JSSTSlenptr; JSSTSpos++) { \
					(ret)[JSSTSpos]=(char)JSSTSstrval[JSSTSpos]; \
				} \
				(ret)[*JSSTSlenptr]=0; \
			} \
			else JS_ReportError((cx), "Error allocating %lu bytes on stack at %s:%d", (*JSSTSlenptr)+1, getfname(__FILE__), __LINE__); \
		} \
	} \
}

#define JSVALUE_TO_ASTRING(cx, val, ret, maxsize, lenptr) \
{ \
	JSString	*JSVTSstr=JS_ValueToString((cx), (val)); \
	JSSTRING_TO_ASTRING((cx), JSVTSstr, (ret), (maxsize), (lenptr)); \
}

#endif

#ifdef USE_CRYPTLIB
#include <cryptlib.h>
#endif

/* xpdev */
#ifndef LINK_LIST_THREADSAFE
 #define LINK_LIST_THREADSAFE
#endif
#include "genwrap.h"
#include "semfile.h"
#include "netwrap.h"
#include "dirwrap.h"
#include "filewrap.h"
#include "datewrap.h"
#include "sockwrap.h"
#include "multisock.h"
#include "eventwrap.h"
#include "link_list.h"
#include "msg_queue.h"
#include "xpdatetime.h"

/***********************/
/* Synchronet-specific */
/***********************/
#include "startup.h"
#ifdef __cplusplus
	#include "threadwrap.h"	/* pthread_mutex_t */
#endif

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
#include "nopen.h"
#include "text.h"
#include "petdefs.h"

/* Synchronet Node Instance class definition */
#ifdef __cplusplus
class sbbs_t
{

public:

	sbbs_t(ushort node_num, union xp_sockaddr *addr, size_t addr_len, const char* host_name, SOCKET
		,scfg_t*, char* text[], client_t* client_info, bool is_event_thread = false);
	~sbbs_t();

	bbs_startup_t*	startup;

	bool	init(void);
	BOOL	terminated;

	client_t client;
	SOCKET	client_socket;
	SOCKET	client_socket_dup;
	union xp_sockaddr	client_addr;
	char	client_name[128];
	char	client_ident[128];
	char	client_ipaddr[INET6_ADDRSTRLEN];
	char	local_addr[INET6_ADDRSTRLEN];
#ifdef USE_CRYPTLIB
	CRYPT_SESSION	ssh_session;
	int		session_channel;
	bool	ssh_mode;
	SOCKET	passthru_socket;
    bool	passthru_output_thread_running;
    bool	passthru_input_thread_running;
#endif

	scfg_t	cfg;

	int		outchar_esc;		   // track ANSI escape seq output

	int 	rioctl(ushort action); // remote i/o control
	bool	rio_abortable;

    RingBuf	inbuf;
    RingBuf	outbuf;
	HANDLE	input_thread;
	pthread_mutex_t	input_thread_mutex;
	bool	input_thread_mutex_locked;	// by someone other than the input_thread
	bool	input_thread_mutex_created;
	pthread_mutex_t	ssh_mutex;
	bool	ssh_mutex_created;

	#define OUTCOM_RETRY_DELAY		80		// milliseconds
	#define OUTCOM_RETRY_ATTEMPTS	1000	// 80 seconds
	int 	_outcom(uchar ch); 	   // send character, without retry (on buffer flow condition)
	int		outcom(uchar ch, int max_attempts = OUTCOM_RETRY_ATTEMPTS);		// send character, with retry
	int 	incom(unsigned long timeout=0);		   // receive character

	void	spymsg(const char *msg);		// send message to active spies

	int		putcom(const char *str, size_t len=0);  // Send string
	void	hangup(void);		   // Hangup modem

	uchar	telnet_local_option[0x100];
	uchar	telnet_remote_option[0x100];
	void	send_telnet_cmd(uchar cmd, uchar opt);
	bool	request_telnet_opt(uchar cmd, uchar opt, unsigned waitforack=0);

    uchar	telnet_cmd[64];
    uint	telnet_cmdlen;
	ulong	telnet_mode;
	/* 	input_thread() writes to these variables: */
	uchar	telnet_last_rxch;
	char	telnet_location[128];
	char	telnet_terminal[TELNET_TERM_MAXLEN+1];
	long 	telnet_rows;	
	long	telnet_cols;
	long	telnet_speed;

	xpevent_t	telnet_ack_event;

	time_t	event_time;				// Time of next exclusive event
	const char*	event_code;				// Internal code of next exclusive event
	bool	is_event_thread;
	bool	event_thread_running;
    bool	output_thread_running;
    bool	input_thread_running;
	bool	terminate_output_thread;

#ifdef JAVASCRIPT

	JSRuntime*		js_runtime;
	JSContext*		js_cx;
	JSObject*		js_glob;
	js_callback_t	js_callback;
	long			js_execfile(const char *fname, const char* startup_dir, JSObject* scope=NULL);
	bool			js_init(ulong* stack_frame);
	void			js_cleanup(void);
	void			js_create_user_objects(void);

#endif

	char	syspage_semfile[MAX_PATH+1];	/* Sysop page semaphore file */
	char 	menu_dir[128];	/* Over-ride default menu dir */
	char 	menu_file[128]; /* Over-ride menu file */

	user_t	useron; 		/* User currently online */
	node_t	thisnode;		/* Node information */
	smb_t	smb;			/* Currently open message base */
	char	rlogin_name[LEN_ALIAS+1];
	char	rlogin_pass[LEN_PASS+1];
	char	rlogin_term[TELNET_TERM_MAXLEN+1];	/* RLogin passed terminal type/speed (e.g. "xterm/57600") */

	uint	temp_dirnum;

	FILE	*nodefile_fp,
			*node_ext_fp,
			*logfile_fp;

	int 	nodefile;		/* File handle for node.dab */
	pthread_mutex_t	nodefile_mutex;
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
    int     keybuftop,keybufbot;    /* Keyboard input buffer pointers (for ungetkey) */
	char    keybuf[KEY_BUFSIZE];    /* Keyboard input buffer */

	ushort	node_connection;
	char	connection[LEN_MODEM+1];	/* Connection Description */
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
	long	column;			/* Current column counter (for line counter) */
	long	tabstop;		/* Current symmetric-tabstop (size) */
	long	lastlinelen;	/* The previously displayed line length */
	long 	autoterm;		/* Auto-detected terminal type */
	char	terminal[TELNET_TERM_MAXLEN+1];	// <- answer() writes to this
	long	cterm_version;	/* (MajorVer*1000) + MinorVer */
	link_list_t savedlines;
	char 	lbuf[LINE_BUFSIZE+1];/* Temp storage for each line output */
	int		lbuflen;		/* Number of characters in line buffer */
	char 	latr;			/* Starting attribute of line buffer */
	ulong	console;		/* Defines current Console settings */
	char 	wordwrap[81];	/* Word wrap buffer */
	time_t	now,			/* Used to store current time in Unix format */
			last_sysop_auth,/* Time sysop was last authenticated */
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
	bool 	autohang;		/* Used for auto-hangup after transfer */
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

	char 	*comspec;		/* Pointer to environment variable COMSPEC */
	ushort	altul;			/* Upload to alternate path flag */
	char 	cid[LEN_CID+1]; /* Caller ID (IP Address) of current caller */
	char 	*noaccess_str;	/* Why access was denied via ARS */
	long 	noaccess_val;	/* Value of parameter not met in ARS */
	int		errorlevel; 	/* Error level of external program */

	csi_t	main_csi;		/* Main Command Shell Image */

	smbmsg_t*	current_msg;	/* For message header @-codes */
	file_t*		current_file;

			/* Global command shell variables */
	uint	global_str_vars;
	char **	global_str_var;
	int32_t *	global_str_var_name;
	uint	global_int_vars;
	int32_t *	global_int_var;
	int32_t *	global_int_var_name;
	char *	sysvar_p[MAX_SYSVARS];
	uint	sysvar_pi;
	int32_t	sysvar_l[MAX_SYSVARS];
	uint	sysvar_li;

    /* ansi_term.cpp */
	const char*	ansi(int atr);			/* Returns ansi escape sequence for atr */
	char*	ansi(int atr, int curatr, char* str);
    bool	ansi_gotoxy(int x, int y);
	bool	ansi_getxy(int* x, int* y);
	bool	ansi_save(void);
	bool	ansi_restore(void);
	void	ansi_getlines(void);

			/* Command Shell Methods */
	int		exec(csi_t *csi);
	int		exec_function(csi_t *csi);
	int		exec_misc(csi_t *csi, char *path);
	int		exec_net(csi_t *csi);
	int		exec_msg(csi_t *csi);
	int		exec_file(csi_t *csi);
	long	exec_bin(const char *mod, csi_t *csi, const char* startup_dir=NULL);
	void	clearvars(csi_t *bin);
	void	freevars(csi_t *bin);
	char**	getstrvar(csi_t *bin, int32_t name);
	int32_t*	getintvar(csi_t *bin, int32_t name);
	char*	copystrvar(csi_t *csi, char *p, char *str);
	void	skipto(csi_t *csi, uchar inst);
	bool	ftp_cmd(csi_t* csi, SOCKET ctrl_sock, const char* cmdsrc, char* rsp);
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

	uint	finduser(char *str, bool silent_failure = false);

	int 	sub_op(uint subnum);

	int		dir_op(uint dirnum);
	int		getuserxfers(int fromuser, int destuser, char *fname);

	void	getmsgptrs(void);
	void	putmsgptrs(void);
	void	getusrsubs(void);
	void	getusrdirs(void);
	uint	getusrsub(uint subnum);
	uint	getusrgrp(uint subnum);
	uint	getusrdir(uint dirnum);
	uint	getusrlib(uint dirnum);

	uint	userdatdupe(uint usernumber, uint offset, uint datlen, char *dat
				,bool del=false, bool next=false);
	ulong	gettimeleft(bool handle_out_of_time=true);
	bool	gettimeleft_inside;

	/* str.cpp */
	char*	timestr(time_t);
	char*	datestr(time_t);
    char	timestr_output[60];
	char	datestr_output[60];
	char*	age_of_posted_item(char* buf, size_t max, time_t);
	void	userlist(long mode);
	size_t	gettmplt(char *outstr, const char *tmplt, long mode);
	void	sif(char *fname, char *answers, long len);	/* Synchronet Interface File */
	void	sof(char *fname, char *answers, long len);
	void	create_sif_dat(char *siffile, char *datfile);
	void	read_sif_dat(char *siffile, char *datfile);
	void	printnodedat(uint number, node_t* node);
	bool	inputnstime32(time32_t *dt);
	bool	inputnstime(time_t *dt);
	bool	chkpass(char *pass, user_t* user, bool unique);
	char *	cmdstr(const char *instr, const char *fpath, const char *fspec, char *outstr);
	char	cmdstr_output[512];

	void	subinfo(uint subnum);
	void	dirinfo(uint dirnum);
	bool	trashcan(const char *insearch, const char *name);
	void	time_bank(void);
	void	change_user(void);

	/* writemsg.cpp */
	void	automsg(void);
	bool	writemsg(const char *str, const char *top, char *subj, long mode, uint subnum
				,const char *to, const char* from, char** editor=NULL);
	char*	quotes_fname(int xedit, char* buf, size_t len);
	char*	msg_tmp_fname(int xedit, char* fname, size_t len);
	char	putmsg(const char *str, long mode);
	bool	msgabort(void);
	bool	email(int usernumber, const char *top, const char *title, long mode);
	void	forwardmail(smbmsg_t* msg, int usernum);
	void	removeline(char *str, char *str2, char num, char skip);
	ulong	msgeditor(char *buf, const char *top, char *title);
	bool	editfile(char *path, bool msg=false);
	ushort	chmsgattr(smbmsg_t);
	void	quotemsg(smbmsg_t* msg, int tails);
	void	editmsg(smbmsg_t* msg, uint subnum);
	void	editor_inf(int xeditnum, const char *to, const char* from, const char *subj, long mode
				,uint subnum, const char* tagfile);
	void	copyfattach(uint to, uint from, char *title);
	bool	movemsg(smbmsg_t* msg, uint subnum);
	int		process_edited_text(char* buf, FILE* stream, long mode, unsigned* lines, unsigned maxlines);
	int		process_edited_file(const char* src, const char* dest, long mode, unsigned* lines, unsigned maxlines);

	/* postmsg.cpp */
	bool	postmsg(uint subnum, smbmsg_t* msg, long wm_mode);

	/* mail.cpp */
	int		delmail(uint usernumber,int which);
	void	telluser(smbmsg_t* msg);
	void	delallmail(uint usernumber, int which, bool permanent=true, long lm_mode = 0);

	/* getmsg.cpp */
	int		loadmsg(smbmsg_t *msg, ulong number);
	void	show_msgattr(smbmsg_t*);
	void	show_msghdr(smbmsg_t* msg);
	void	show_msg(smbmsg_t* msg, long mode, post_t* post = NULL);
	void	msgtotxt(smbmsg_t* msg, char *str, bool header, ulong mode);
	ulong	getlastmsg(uint subnum, uint32_t *ptr, time_t *t);
	time_t	getmsgtime(uint subnum, ulong ptr);
	ulong	getmsgnum(uint subnum, time_t t);

	/* readmail.cpp */
	void	readmail(uint usernumber, int which, long lm_mode = 0);
	bool	readmail_inside;
	long	searchmail(mail_t*, long start, long msgss, int which, const char *search);

	/* bulkmail.cpp */
	bool	bulkmail(uchar *ar);
	int		bulkmailhdr(smb_t*, smbmsg_t*, uint usernum);

	/* con_out.cpp */
	int		bputs(const char *str);					/* BBS puts function */
	int		rputs(const char *str, size_t len=0);	/* BBS raw puts function */
	int		bprintf(const char *fmt, ...)			/* BBS printf function */
#if defined(__GNUC__)   // Catch printf-format errors
    __attribute__ ((format (printf, 2, 3)));		// 1 is 'this'
#endif
	;
	int		rprintf(const char *fmt, ...)			/* BBS raw printf function */
#if defined(__GNUC__)   // Catch printf-format errors
    __attribute__ ((format (printf, 2, 3)));		// 1 is 'this'
#endif
	;
	void	backspace(void);				/* Output a destructive backspace via outchar */
	void	outchar(char ch);				/* Output a char - check echo and emu.  */
	void	center(char *str);
	void	clearline(void);
	void	cleartoeol(void);
	void	cleartoeos(void);
	void	cursor_home(void);
	void	cursor_up(int count=1);
	void	cursor_down(int count=1);
	void	cursor_left(int count=1);
	void	cursor_right(int count=1);
	void	carriage_return(void);
	void	line_feed(void);
	void	newline(void);
	long	term_supports(long cmp_flags=0);
	int		backfill(const char* str, float pct, int full_attr, int empty_attr);
	void	progress(const char* str, int count, int total, int interval=1);
	bool	saveline(void);
	bool	restoreline(void);

	/* getstr.cpp */
	size_t	getstr_offset;
	size_t	getstr(char *str, size_t length, long mode, const str_list_t history = NULL);
	long	getnum(ulong max, ulong dflt=0);
	void	insert_indicator(void);

	/* getkey.cpp */
	char	getkey(long mode); 		/* Waits for a key hit local or remote  */
	long	getkeys(const char *str, ulong max);
	void	ungetkey(char ch);		/* Places 'ch' into the input buffer    */
	char	question[MAX_TEXTDAT_ITEM_LEN+1];
	bool	yesno(const char *str);
	bool	noyes(const char *str);
	void	pause(void);
	const char *	mnestr;
	void	mnemonics(const char *str);

	/* inkey.cpp */
	char	inkey(long mode, unsigned long timeout=0);
	char	handle_ctrlkey(char ch, long mode=0);

	/* prntfile.cpp */
	bool	printfile(char *str, long mode);
	bool	printtail(char *str, int lines, long mode);
	bool	menu(const char *code, long mode = 0);
	bool	menu_exists(const char *code, const char* ext=NULL, char* realpath=NULL);

	int		uselect(int add, uint n, const char *title, const char *item, const uchar *ar);
	uint	uselect_total, uselect_num[500];

	long	mselect(const char *title, str_list_t list, unsigned max_selections, const char* item_fmt, const char* selected_str, const char* unselected_str, const char* prompt_fmt);

	void	redrwstr(char *strin, int i, int l, long mode);
	void	attr(int atr);				/* Change local and remote text attributes */
	void	ctrl_a(char x);			/* Peforms the Ctrl-Ax attribute changes */

	/* atcodes.cpp */
	int		show_atcode(const char *code);
	const char*	atcode(char* sp, char* str, size_t maxlen);

	/* getnode.cpp */
	int		getsmsg(int usernumber, bool clearline = false);
	int		getnmsg(bool clearline = false);
	int		whos_online(bool listself);/* Lists active nodes, returns active nodes */
	void	nodelist(void);
	int		getnodeext(uint number, char * str);
	int		getnodedat(uint number, node_t * node, bool lock);
	void	nodesync(bool clearline = false);
	user_t	nodesync_user;
	bool	nodesync_inside;

	/* putnode.cpp */
	int		putnodedat(uint number, node_t * node);
	int		putnodeext(uint number, char * str);

	/* login.ccp */
	int		login(char *user_name, char *pw_prompt, const char* user_pw = NULL, const char* sys_pw = NULL);
	void	badlogin(char* user, char* passwd, const char* protocol=NULL, xp_sockaddr* addr=NULL, bool delay=true);

	/* answer.cpp */
	bool	answer();

	/* logon.ccp */
	bool	logon(void);

	/* logout.cpp */
	void	logout(void);
	void	backout(void);

	/* newuser.cpp */
	BOOL	newuser(void);					/* Get new user							*/

	/* text_sec.cpp */
	int		text_sec(void);						/* Text sections */

	/* readmsgs.cpp */
	post_t* loadposts(uint32_t *posts, uint subnum, ulong ptr, long mode, ulong *unvalidated_num, uint32_t* visible=NULL);
	int		scanposts(uint subnum, long mode, const char* find);	/* Scan sub-board */
	bool	scanposts_inside;
	long	listsub(uint subnum, long mode, long start, const char* search);
	long	listmsgs(uint subnum, long mode, post_t* post, long start, long posts);
	long	searchposts(uint subnum, post_t* post, long start, long msgs, const char* find);
	long	showposts_toyou(uint subnum, post_t* post, ulong start, long posts, long mode=0);
	void	show_thread(uint32_t msgnum, post_t* post, unsigned curmsg, int thread_depth = 0, uint64_t reply_mask = 0);
	void	msghdr(smbmsg_t* msg);
	uchar	msg_listing_flag(uint subnum, smbmsg_t*, post_t*);
	int64_t get_start_msgnum(smb_t*, int next=0);

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
	int		lputs(int level, const char* str);
	int		lprintf(int level, const char *fmt, ...)
#if defined(__GNUC__)   // Catch printf-format errors
    __attribute__ ((format (printf, 3, 4)));		// 1 is 'this'
#endif
	;
	void	printstatslog(uint node);
	ulong	logonstats(void);
	void	logoffstats(void);
	int		nopen(char *str, int access);
	int		mv(char *src, char *dest, char copy); /* fast file move/copy function */
	bool	chksyspass(const char* sys_pw = NULL);
	bool	chk_ar(const uchar * str, user_t* user, client_t* client); /* checks access requirements */
	bool	ar_exp(const uchar ** ptrptr, user_t*, client_t*);
	void	daily_maint(void);
	bool	backup(const char* fname, int backup_level, bool rename);

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
	const char*	protcmdline(prot_t* prot, enum XFER_TYPE type);
	void	seqwait(uint devnum);
	void	autohangup(void);
	bool	checkdszlog(const char*);
	bool	checkprotresult(prot_t*, int error, file_t*);
	bool	sendfile(char* fname, char prot=0, const char* description = NULL);
	bool	recvfile(char* fname, char prot=0);

	/* file.cpp */
	void	fileinfo(file_t* f);
	void	openfile(file_t* f);
	void	closefile(file_t* f);
	bool	removefcdt(file_t* f);
	bool	removefile(file_t* f);
	bool	movefile(file_t* f, int newdir);
	char *	getfilespec(char *str);
	bool	checkfname(char *fname);
	bool	addtobatdl(file_t* f);

	/* listfile.cpp */
	bool	listfile(const char *fname, const char *buf, uint dirnum
				,const char *search, const char letter, ulong datoffset);
	int		listfiles(uint dirnum, const char *filespec, int tofile, long mode);
	int		listfileinfo(uint dirnum, char *filespec, long mode);
	void	listfiletofile(char *fname, char *buf, uint dirnum, int file);
	int		batchflagprompt(uint dirnum, file_t bf[], uint total, long totalfiles);

	/* bat_xfer.cpp */
	void	batchmenu(void);
	void	batch_create_list(void);
	void	batch_add_list(char *list);
	bool	create_batchup_lst(void);
	bool	create_batchdn_lst(bool native);
	bool	create_bimodem_pth(void);
	void	batch_upload(void);
	void	batch_download(int xfrprot);
	BOOL	start_batch_download(void);

	/* tmp_xfer.cpp */
	void	temp_xfer(void);
	void	extract(uint dirnum);
	char *	temp_cmd(void);					/* Returns temp file command line */
	ulong	create_filelist(const char *name, long mode);

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
	void	xtrndat(const char* name, const char* dropdir, uchar type, ulong tleft
				,ulong misc);
	bool	exec_xtrn(uint xtrnnum);			/* Executes online external program */
	bool	user_event(user_event_t);			/* Executes user event(s) */
	char	xtrn_access(uint xnum);			/* Does useron have access to xtrn? */
	void	moduserdat(uint xtrnnum);

	/* logfile.cpp */
	void	logentry(const char *code,const char *entry);
	void	log(char *str);				/* Writes 'str' to node log */
	void	logch(char ch, bool comma);	/* Writes 'ch' to node log */
	void	logline(const char *code,const char *str); /* Writes 'str' on it's own line in log (LOG_INFO level) */
	void	logline(int level, const char *code,const char *str);
	void	logofflist(void);              /* List of users logon activity */
	bool	errormsg_inside;
	void	errormsg(int line, const char* function, const char *source, const char* action, const char *object
				,long access, const char *extinfo=NULL);
	BOOL	hacklog(char* prot, char* text);

	/* qwk.cpp */
	bool	qwklogon;
	ulong	qwkmail_last;
	void	qwk_sec(void);
	uint	total_qwknodes;
	struct qwknode {
		char	id[LEN_QWKID+1];
		char	path[MAX_PATH+1];
		time_t	time;
	}* qwknode;
	void	update_qwkroute(char *via);
	void	qwk_success(ulong msgcnt, char bi, char prepack);
	void	qwksetptr(uint subnum, char *buf, int reset);
	void	qwkcfgline(char *buf,uint subnum);
	int		set_qwk_flag(ulong flag);
	uint	resolve_qwkconf(uint n, int hubnum=-1);
	bool	qwk_vote(str_list_t ini, const char* section, smb_net_type_t, const char* qnet_id, int hubnum);
	bool	qwk_voting(str_list_t* ini, long offset, smb_net_type_t, const char* qnet_id, int hubnum = -1);
	void	qwk_handle_remaining_votes(str_list_t* ini, smb_net_type_t, const char* qnet_id, int hubnum = -1);

	/* pack_qwk.cpp */
	bool	pack_qwk(char *packet, ulong *msgcnt, bool prepack);

	/* un_qwk.cpp */
	bool	unpack_qwk(char *packet,uint hubnum);

	/* pack_rep.cpp */
	bool	pack_rep(uint hubnum);

	/* un_rep.cpp */
	bool	unpack_rep(char* repfile=NULL);

	/* msgtoqwk.cpp */
	ulong	msgtoqwk(smbmsg_t* msg, FILE *qwk_fp, long mode, uint subnum, int conf, FILE* hdrs_dat, FILE* voting_dat = NULL);

	/* qwktomsg.cpp */
	void	qwk_new_msg(ulong confnum, smbmsg_t* msg, char* hdrblk, long offset, str_list_t headers, bool parse_sender_hfields);
	bool	qwk_import_msg(FILE *qwk_fp, char *hdrblk, ulong blocks, char fromhub, uint subnum
				,uint touser, smbmsg_t* msg);

	/* fido.cpp */
	bool	netmail(const char *into, const char *subj, long mode);
	void	qwktonetmail(FILE *rep, char *block, char *into, uchar fromhub);
	bool	lookup_netuser(char *into);

	bool	inetmail(const char *into, const char *subj, long mode);
	bool	qnetmail(const char *into, const char *subj, long mode);

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
	bool	scansubs_inside;
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
	void	telnet_gate(char* addr, ulong mode, char* client_user_name=NULL, char* server_user_name=NULL, char* term_type=NULL);	// See TG_* for mode bits

};

#endif /* __cplusplus */

#ifdef DLLEXPORT
#undef DLLEXPORT
#endif
#ifdef DLLCALL
#undef DLLCALL
#endif
#ifdef _WIN32
	#ifdef __MINGW32__
		#define DLLEXPORT
		#define DLLCALL
	#else
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
	#endif
#else	/* !_WIN32 */
	#define DLLEXPORT
	#define DLLCALL
#endif

#ifdef __cplusplus
extern "C" {
#endif
	/* ansiterm.cpp */
	DLLEXPORT char*		DLLCALL ansi_attr(int attr, int curattr, char* str, BOOL color);

	/* main.cpp */
	DLLEXPORT int		DLLCALL sbbs_random(int);
	DLLEXPORT void		DLLCALL sbbs_srand(void);

	/* getstats.c */
	DLLEXPORT BOOL		DLLCALL getstats(scfg_t* cfg, char node, stats_t* stats);
	DLLEXPORT ulong		DLLCALL	getposts(scfg_t* cfg, uint subnum);
	DLLEXPORT long		DLLCALL getfiles(scfg_t* cfg, uint dirnum);

	/* getmail.c */
	DLLEXPORT int		DLLCALL getmail(scfg_t* cfg, int usernumber, BOOL sent, uint16_t attr);
	DLLEXPORT mail_t *	DLLCALL loadmail(smb_t* smb, uint32_t* msgs, uint usernumber
										,int which, long mode);
	DLLEXPORT void		DLLCALL freemail(mail_t* mail);
	DLLEXPORT BOOL		DLLCALL delfattach(scfg_t*, smbmsg_t*);

	/* postmsg.cpp */
	DLLEXPORT int		DLLCALL savemsg(scfg_t*, smb_t*, smbmsg_t*, client_t*, const char* server, char* msgbuf);
	DLLEXPORT int		DLLCALL votemsg(scfg_t*, smb_t*, smbmsg_t*, const char* msgfmt, const char* votefmt);
	DLLEXPORT int		DLLCALL postpoll(scfg_t*, smb_t*, smbmsg_t*);
	DLLEXPORT int		DLLCALL closepoll(scfg_t*, smb_t*, uint32_t msgnum, const char* username);
	DLLEXPORT void		DLLCALL signal_sub_sem(scfg_t*, uint subnum);
	DLLEXPORT int		DLLCALL msg_client_hfields(smbmsg_t*, client_t*);
	DLLEXPORT char*		DLLCALL msg_program_id(char* pid);

	/* filedat.c */
	DLLEXPORT BOOL		DLLCALL getfileixb(scfg_t* cfg, file_t* f);
	DLLEXPORT BOOL		DLLCALL putfileixb(scfg_t* cfg, file_t* f);
	DLLEXPORT BOOL		DLLCALL getfiledat(scfg_t* cfg, file_t* f);
	DLLEXPORT BOOL		DLLCALL putfiledat(scfg_t* cfg, file_t* f);
	DLLEXPORT void		DLLCALL putextdesc(scfg_t* cfg, uint dirnum, ulong datoffset, char *ext);
	DLLEXPORT void		DLLCALL getextdesc(scfg_t* cfg, uint dirnum, ulong datoffset, char *ext);
	DLLEXPORT char*		DLLCALL getfilepath(scfg_t* cfg, file_t* f, char* path);

	DLLEXPORT BOOL		DLLCALL removefiledat(scfg_t* cfg, file_t* f);
	DLLEXPORT BOOL		DLLCALL addfiledat(scfg_t* cfg, file_t* f);
	DLLEXPORT BOOL		DLLCALL findfile(scfg_t* cfg, uint dirnum, char *filename);
	DLLEXPORT char *	DLLCALL padfname(const char *filename, char *str);
	DLLEXPORT char *	DLLCALL unpadfname(const char *filename, char *str);
	DLLEXPORT BOOL		DLLCALL rmuserxfers(scfg_t* cfg, int fromuser, int destuser, char *fname);

	DLLEXPORT int		DLLCALL update_uldate(scfg_t* cfg, file_t* f);

	/* str_util.c */
	DLLEXPORT char *	DLLCALL remove_ctrl_a(const char* instr, char* outstr);
	DLLEXPORT char 		DLLCALL ctrl_a_to_ascii_char(char code);
	DLLEXPORT char *	DLLCALL truncstr(char* str, const char* set);
	DLLEXPORT char *	DLLCALL ascii_str(uchar* str);
	DLLEXPORT char		DLLCALL exascii_to_ascii_char(uchar ch);
	DLLEXPORT BOOL		DLLCALL findstr(const char *insearch, const char *fname);
	DLLEXPORT BOOL		DLLCALL findstr_in_string(const char* insearchof, char* string);
	DLLEXPORT BOOL		DLLCALL findstr_in_list(const char* insearchof, str_list_t list);
	DLLEXPORT BOOL		DLLCALL trashcan(scfg_t* cfg, const char *insearch, const char *name);
	DLLEXPORT char *	DLLCALL trashcan_fname(scfg_t* cfg, const char *name, char* fname, size_t);
	DLLEXPORT str_list_t DLLCALL trashcan_list(scfg_t* cfg, const char* name);
	DLLEXPORT char *	DLLCALL strip_exascii(const char *str, char* dest);
	DLLEXPORT char *	DLLCALL strip_space(const char *str, char* dest);
	DLLEXPORT char *	DLLCALL prep_file_desc(const char *str, char* dest);
	DLLEXPORT char *	DLLCALL strip_ctrl(const char *str, char* dest);
	DLLEXPORT char *	DLLCALL net_addr(net_t* net);
	DLLEXPORT BOOL		DLLCALL valid_ctrl_a_attr(char a);
	DLLEXPORT BOOL		DLLCALL valid_ctrl_a_code(char a);
	DLLEXPORT size_t	DLLCALL strip_invalid_attr(char *str);
	DLLEXPORT char *	DLLCALL ultoac(ulong l,char *str);
	DLLEXPORT char *	DLLCALL rot13(char* str);
	DLLEXPORT uint32_t	DLLCALL str_to_bits(uint32_t currval, const char *str);

	/* msg_id.c */
	DLLEXPORT char *	DLLCALL ftn_msgid(sub_t* sub, smbmsg_t* msg, char* msgid, size_t);
	DLLEXPORT char *	DLLCALL get_msgid(scfg_t* cfg, uint subnum, smbmsg_t* msg, char* msgid, size_t);
	DLLEXPORT char *	DLLCALL get_replyid(scfg_t* cfg, smb_t* smb, smbmsg_t* msg, char* msgid, size_t maxlen);
	DLLEXPORT uint32_t	DLLCALL get_new_msg_number(smb_t* smb);

	/* date_str.c */
	DLLEXPORT char *	DLLCALL zonestr(short zone);
	DLLEXPORT time32_t	DLLCALL dstrtounix(scfg_t*, const char *str);
	DLLEXPORT char *	DLLCALL unixtodstr(scfg_t*, time32_t, char *str);
	DLLEXPORT char *	DLLCALL sectostr(uint sec, char *str);
	DLLEXPORT char *	DLLCALL seconds_to_str(uint, char*);
	DLLEXPORT char *	DLLCALL hhmmtostr(scfg_t* cfg, struct tm* tm, char* str);
	DLLEXPORT char *	DLLCALL timestr(scfg_t* cfg, time32_t intime, char* str);

	/* msgdate.c */
	DLLEXPORT when_t	DLLCALL rfc822date(char* p);
	DLLEXPORT char *	DLLCALL msgdate(when_t when, char* buf);
	DLLEXPORT BOOL		DLLCALL newmsgs(smb_t*, time_t);

	/* load_cfg.c */
	DLLEXPORT BOOL		DLLCALL load_cfg(scfg_t* cfg, char* text[], BOOL prep, char* error);
	DLLEXPORT void		DLLCALL free_cfg(scfg_t* cfg);
	DLLEXPORT void		DLLCALL free_text(char* text[]);
	DLLEXPORT ushort	DLLCALL sys_timezone(scfg_t* cfg);
	DLLEXPORT char *	DLLCALL prep_dir(const char* base, char* dir, size_t buflen);

	/* scfgsave.c */
	DLLEXPORT BOOL		DLLCALL save_cfg(scfg_t* cfg, int backup_level);
	DLLEXPORT BOOL		DLLCALL write_node_cfg(scfg_t* cfg, int backup_level);
	DLLEXPORT BOOL		DLLCALL write_main_cfg(scfg_t* cfg, int backup_level);
	DLLEXPORT BOOL		DLLCALL write_msgs_cfg(scfg_t* cfg, int backup_level);
	DLLEXPORT BOOL		DLLCALL write_file_cfg(scfg_t* cfg, int backup_level);
	DLLEXPORT BOOL		DLLCALL write_chat_cfg(scfg_t* cfg, int backup_level);
	DLLEXPORT BOOL		DLLCALL write_xtrn_cfg(scfg_t* cfg, int backup_level);
	DLLEXPORT void		DLLCALL refresh_cfg(scfg_t* cfg);
	DLLEXPORT int		DLLCALL smb_storage_mode(scfg_t*, smb_t*);
	DLLEXPORT int		DLLCALL smb_open_sub(scfg_t*, smb_t*, unsigned int subnum);

	/* logfile.cpp */
	DLLEXPORT int		DLLCALL errorlog(scfg_t* cfg, const char* host, const char* text);

	DLLEXPORT BOOL		DLLCALL hacklog(scfg_t* cfg, const char* prot, const char* user, const char* text
										,const char* host, union xp_sockaddr* addr);
	DLLEXPORT BOOL		DLLCALL spamlog(scfg_t* cfg, char* prot, char* action, char* reason
										,char* host, char* ip_addr, char* to, char* from);

	/* data.cpp */
	DLLEXPORT time_t	DLLCALL getnextevent(scfg_t* cfg, event_t* event);

	/* sockopt.c */
	DLLEXPORT int		DLLCALL set_socket_options(scfg_t* cfg, SOCKET sock, const char* section
		,char* error, size_t errlen);

	/* xtrn.cpp */
	DLLEXPORT char*		DLLCALL cmdstr(scfg_t* cfg, user_t* user, const char* instr
									,const char* fpath, const char* fspec, char* cmd);

	/* qwk.cpp */
	DLLEXPORT int		qwk_route(scfg_t*, const char *inaddr, char *fulladdr, size_t maxlen);

#ifdef JAVASCRIPT

	typedef struct {
		const char*		name;
		JSNative        call;
		uint8_t         nargs;
		int				type;		/* return type */
		const char*		args;		/* arguments */
		const char*		desc;		/* description */
		int				ver;		/* version added/modified */
	} jsSyncMethodSpec;

	typedef struct {
		const char      *name;
		int8_t          tinyid;
		uint8_t         flags;
		int				ver;		/* version added/modified */
	} jsSyncPropertySpec;

	typedef struct {
		const char*		name;
		int				val;
	} jsConstIntSpec;

	typedef struct {
		char		version[128];
		const char*	version_detail;
		str_list_t*	interfaces;
		uint32_t*	options;
		uint32_t*	clients;
	} js_server_props_t;

	enum {
		 JSTYPE_ARRAY=JSTYPE_LIMIT
		,JSTYPE_ALIAS
		,JSTYPE_UNDEF
#if JS_VERSION < JSVERSION_1_6	/* JSTYPE_NULL was removed after 1.5 rc 6a (?) */
		,JSTYPE_NULL
#endif
	};

	#ifdef BUILD_JSDOCS	/* String compiled into debug build only, for JS documentation generation */
		#define	JSDOCSTR(s)	s
	#else
		#define JSDOCSTR(s)	NULL
	#endif

	/* main.cpp */
	DLLEXPORT JSBool	DLLCALL js_DescribeSyncObject(JSContext* cx, JSObject* obj, const char*, int ver);
	DLLEXPORT JSBool	DLLCALL js_DescribeSyncConstructor(JSContext* cx, JSObject* obj, const char*);
	DLLEXPORT JSBool	DLLCALL js_DefineSyncMethods(JSContext* cx, JSObject* obj, jsSyncMethodSpec*);
	DLLEXPORT JSBool	DLLCALL js_DefineSyncProperties(JSContext* cx, JSObject* obj, jsSyncPropertySpec*);
	DLLEXPORT JSBool	DLLCALL js_SyncResolve(JSContext* cx, JSObject* obj, char *name, jsSyncPropertySpec* props, jsSyncMethodSpec* funcs, jsConstIntSpec* consts, int flags);
	DLLEXPORT JSBool	DLLCALL js_DefineConstIntegers(JSContext* cx, JSObject* obj, jsConstIntSpec*, int flags);
	DLLEXPORT JSBool	DLLCALL js_CreateArrayOfStrings(JSContext* cx, JSObject* parent
														,const char* name, char* str[], unsigned flags);
	DLLEXPORT BOOL	DLLCALL js_CreateCommonObjects(JSContext* cx
													,scfg_t* cfg				/* common */
													,scfg_t* node_cfg			/* node-specific */
													,jsSyncMethodSpec* methods	/* global */
													,time_t uptime				/* system */
													,char* host_name			/* system */
													,char* socklib_desc			/* system */
													,js_callback_t*				/* js */
													,js_startup_t*				/* js */
													,client_t* client			/* client */
													,SOCKET client_socket		/* client */
#ifdef USE_CRYPTLIB
													,CRYPT_CONTEXT session		/* client */
#else
													,int unused
#endif
													,js_server_props_t* props	/* server */
													,JSObject** glob
													);

	/* js_server.c */
	DLLEXPORT JSObject* DLLCALL js_CreateServerObject(JSContext* cx, JSObject* parent
										,js_server_props_t* props);

	/* js_global.c */
	typedef struct {
		scfg_t*				cfg;
		jsSyncMethodSpec*	methods;
		js_startup_t*		startup;
		unsigned			bg_count;
		sem_t				bg_sem;
		str_list_t			exit_func;
	} global_private_t;
	DLLEXPORT BOOL DLLCALL js_argc(JSContext *cx, unsigned argc, unsigned min);
	DLLEXPORT BOOL DLLCALL js_CreateGlobalObject(JSContext* cx, scfg_t* cfg, jsSyncMethodSpec* methods, js_startup_t*, JSObject**);

	/* js_internal.c */
	DLLEXPORT JSObject* DLLCALL js_CreateInternalJsObject(JSContext*, JSObject* parent, js_callback_t*, js_startup_t*);
	DLLEXPORT JSBool	DLLCALL js_CommonOperationCallback(JSContext*, js_callback_t*);
	DLLEXPORT void		DLLCALL js_EvalOnExit(JSContext*, JSObject*, js_callback_t*);
	DLLEXPORT void		DLLCALL	js_PrepareToExecute(JSContext*, JSObject*, const char *filename, const char* startup_dir, JSObject *);
	DLLEXPORT char*		DLLCALL js_getstring(JSContext *cx, JSString *str);

	/* js_system.c */
	DLLEXPORT JSObject* DLLCALL js_CreateSystemObject(JSContext* cx, JSObject* parent
													,scfg_t* cfg, time_t uptime
													,char* host_name
													,char* socklib_desc);

	/* js_client.c */
#ifdef USE_CRYPTLIB
	DLLEXPORT JSObject* DLLCALL js_CreateClientObject(JSContext* cx, JSObject* parent
													,const char* name, client_t* client, SOCKET sock, CRYPT_CONTEXT session);
#endif
	/* js_user.c */
	DLLEXPORT JSObject*	DLLCALL js_CreateUserClass(JSContext* cx, JSObject* parent, scfg_t* cfg);
	DLLEXPORT JSObject* DLLCALL js_CreateUserObject(JSContext* cx, JSObject* parent, scfg_t* cfg
													,char* name, user_t* user, client_t* client, BOOL global_user);
	DLLEXPORT JSBool	DLLCALL js_CreateUserObjects(JSContext* cx, JSObject* parent, scfg_t* cfg
													,user_t* user, client_t* client, char* html_index_file
													,subscan_t* subscan);
	/* js_file_area.c */
	DLLEXPORT JSObject* DLLCALL js_CreateFileAreaObject(JSContext* cx, JSObject* parent, scfg_t* cfg
													,user_t* user, client_t* client, char* html_index_file);

	/* js_msg_area.c */
	DLLEXPORT JSObject* DLLCALL js_CreateMsgAreaObject(JSContext* cx, JSObject* parent, scfg_t* cfg
													,user_t* user, client_t* client, subscan_t* subscan);
	DLLEXPORT BOOL		DLLCALL js_CreateMsgAreaProperties(JSContext* cx, scfg_t* cfg
													,JSObject* subobj, uint subnum);

	/* js_xtrn_area.c */
	DLLEXPORT JSObject* DLLCALL js_CreateXtrnAreaObject(JSContext* cx, JSObject* parent, scfg_t* cfg
													,user_t* user, client_t* client);

	/* js_msgbase.c */
	DLLEXPORT JSObject* DLLCALL js_CreateMsgBaseClass(JSContext* cx, JSObject* parent, scfg_t* cfg);
	DLLEXPORT BOOL		DLLCALL js_ParseMsgHeaderObject(JSContext* cx, JSObject* hdrobj, smbmsg_t*);

	/* js_socket.c */
	DLLEXPORT JSObject* DLLCALL js_CreateSocketClass(JSContext* cx, JSObject* parent);
#ifdef USE_CRYPTLIB
	DLLEXPORT JSObject* DLLCALL js_CreateSocketObject(JSContext* cx, JSObject* parent
													,char *name, SOCKET sock, CRYPT_CONTEXT session);
#endif
	DLLEXPORT JSObject* DLLCALL js_CreateSocketObjectFromSet(JSContext* cx, JSObject* parent
													,char *name, struct xpms_set *set);

	DLLEXPORT void		DLLCALL js_timeval(JSContext* cx, jsval val, struct timeval* tv);
	DLLEXPORT SOCKET	DLLCALL js_socket(JSContext *cx, jsval val);
    DLLEXPORT SOCKET	DLLCALL js_socket_add(JSContext *cx, jsval val, fd_set *fds);
	DLLEXPORT BOOL		DLLCALL js_socket_isset(JSContext *cx, jsval val, fd_set *fds);

	/* js_queue.c */
	DLLEXPORT JSObject* DLLCALL js_CreateQueueClass(JSContext* cx, JSObject* parent);
	DLLEXPORT JSObject* DLLCALL js_CreateQueueObject(JSContext* cx, JSObject* parent
													,char *name, msg_queue_t* q);
	BOOL js_enqueue_value(JSContext *cx, msg_queue_t* q, jsval val, char* name);

	/* js_file.c */
	DLLEXPORT JSObject* DLLCALL js_CreateFileClass(JSContext* cx, JSObject* parent);
	DLLEXPORT JSObject* DLLCALL js_CreateFileObject(JSContext* cx, JSObject* parent, char *name, FILE* fp);

	/* js_sprintf.c */
	DLLEXPORT char*		DLLCALL js_sprintf(JSContext* cx, uint argn, unsigned argc, jsval *argv);
	DLLEXPORT void		DLLCALL js_sprintf_free(char *);

	/* js_console.cpp */
	JSObject* js_CreateConsoleObject(JSContext* cx, JSObject* parent);

	/* js_bbs.cpp */
	JSObject* js_CreateBbsObject(JSContext* cx, JSObject* parent);

	/* js_uifc.c */
	JSObject* js_CreateUifcObject(JSContext* cx, JSObject* parent);

	/* js_conio.c */
	JSObject* js_CreateConioObject(JSContext* cx, JSObject* parent);

	/* js_com.c */
	DLLEXPORT JSObject* DLLCALL js_CreateCOMClass(JSContext* cx, JSObject* parent);
	DLLEXPORT JSObject* DLLCALL js_CreateCOMObject(JSContext* cx, JSObject* parent, const char *name, COM_HANDLE sock);

	/* js_cryptcon.c */
	DLLEXPORT JSObject* DLLCALL js_CreateCryptContextClass(JSContext* cx, JSObject* parent);

	/* js_cryptkeyset.c */
	DLLEXPORT JSObject* DLLCALL js_CreateCryptKeysetClass(JSContext* cx, JSObject* parent);

	/* js_cryptcert.c */
	DLLEXPORT JSObject* DLLCALL js_CreateCryptCertClass(JSContext* cx, JSObject* parent);

#endif

/* str_util.c */
size_t	bstrlen(const char *str);
char*	backslashcolon(char *str);
ulong	ahtoul(const char *str);	/* Converts ASCII hex to ulong */
char *	hexplus(uint num, char *str); 	/* Hex plus for 3 digits up to 9000 */
uint	hptoi(const char *str);
int		pstrcmp(const char **str1, const char **str2);  /* Compares pointers to pointers */
int		strsame(const char *str1, const char *str2);	/* Compares number of same chars */

/* load_cfg.c */
BOOL 	md(char *path);
char*	prep_code(char *str, const char* prefix);

#ifdef SBBS /* These aren't exported */

	/* main.c */
	int 	lputs(int level, const char *);			/* log output */
	int 	lprintf(int level, const char *fmt, ...)	/* log output */
#if defined(__GNUC__)   // Catch printf-format errors
    __attribute__ ((format (printf, 2, 3)));
#endif
	;
	int 	eprintf(int level, const char *fmt, ...)	/* event log */
#if defined(__GNUC__)   // Catch printf-format errors
    __attribute__ ((format (printf, 2, 3)));
#endif
	;
	void	call_socket_open_callback(BOOL open);
	SOCKET	open_socket(int type, const char* protocol);
	SOCKET	accept_socket(SOCKET s, union xp_sockaddr* addr, socklen_t* addrlen);
	int		close_socket(SOCKET);
	u_long	resolve_ip(char *addr);

	char *	readtext(long *line, FILE *stream, long dflt);

	/* ver.cpp */
	char*	socklib_version(char* str, char* winsock_ver);

	/* sortdir.cpp */
	int		fnamecmp_a(char **str1, char **str2);	 /* for use with resort() */
	int		fnamecmp_d(char **str1, char **str2);
	int		fdatecmp_a(uchar **buf1, uchar **buf2);
	int		fdatecmp_d(uchar **buf1, uchar **buf2);

	/* file.cpp */
	BOOL	filematch(const char *filename, const char *filespec);

	/* sbbscon.c */
	#if defined(__unix__) && defined(NEEDS_DAEMON)
	int daemon(int nochdir, int noclose);
	#endif

#endif /* SBBS */

extern const char* wday[];	/* abbreviated weekday names */
extern const char* mon[];	/* abbreviated month names */
extern char lastuseron[LEN_ALIAS+1];  /* Name of user last online */

#ifdef __cplusplus
}
#endif

extern
#ifdef __cplusplus
 "C"
#endif
	const char* beta_version;

/* Global data */

/* ToDo: These should be hunted down and killed */
#define lread(f,b,l) read(f,b,l)
#define lfread(b,l,f) fread(b,l,f)
#define lwrite(f,b,l) write(f,b,l)
#define lfwrite(b,l,f) fwrite(b,l,f)
#define lkbrd(x)	0

#endif	/* Don't add anything after this line */
