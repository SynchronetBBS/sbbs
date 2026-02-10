/* Synchronet class (sbbs_t) definition and exported function prototypes */

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
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
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
#elif !defined(__GCC__) || (__GCC__ > 4)
	extern
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
#include "unicode_defs.h"

#define SAFECOPY_UTF8(dst, src) do { if(utf8_str_is_valid(src)) utf8_strlcpy(dst, src, sizeof dst); else SAFECOPY(dst, src); } while(0)

/***********************/
/* Synchronet-specific */
/***********************/
#ifndef RINGBUF_EVENT
 #define RINGBUF_EVENT
#endif
#include "startup.h"
#ifdef __cplusplus
	#include "threadwrap.h"	/* pthread_mutex_t */
	#include "ansi_parser.h"
#endif

#include "smblib.h"
#include "ars_defs.h"
#include "scfglib.h"
#include "scfgsave.h"
#include "userdat.h"
#include "riodefs.h"
#include "cmdshell.h"
#include "ringbuf.h"    /* RingBuf definition */
#include "client.h"		/* client_t definition */
#include "crc16.h"
#include "crc32.h"
#include "telnet.h"
#include "nopen.h"
#include "logfile.h"
#include "trash.h"
#include "text.h"
#include "str_util.h"
#include "findstr.h"
#include "date_str.h"
#include "load_cfg.h"
#include "getstats.h"
#include "msgdate.h"
#include "getmail.h"
#include "msg_id.h"
#include "mqtt.h"
#if defined(__cplusplus)
extern "C" {
/*
 * MSVC (correctly) warns that flexible arrays are not part of C++
 * Since these are used in this header, disable the warning
 */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4200)
#endif
#include "sftp.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
}
#endif

#if defined(JAVASCRIPT)
enum js_event_type {
	JS_EVENT_SOCKET_READABLE_ONCE,
	JS_EVENT_SOCKET_READABLE,
	JS_EVENT_SOCKET_WRITABLE_ONCE,
	JS_EVENT_SOCKET_WRITABLE,
	JS_EVENT_SOCKET_CONNECT,
	JS_EVENT_INTERVAL,
	JS_EVENT_TIMEOUT,
	JS_EVENT_CONSOLE_INPUT_ONCE,
	JS_EVENT_CONSOLE_INPUT
};

struct js_event_interval {
	uint64_t last;	// The tick the last event should have triggered at
	uint64_t period;
};

struct js_event_timeout {
	uint64_t end;	// Time the timeout expires
};

struct js_event_connect {
	SOCKET sv[2];
	SOCKET sock;
};

struct js_event_list {
	struct js_event_list *prev;
	struct js_event_list *next;
	JSFunction *cb;
	JSObject *cx;
	union {
		SOCKET	sock;
		struct js_event_connect connect;
		struct js_event_interval interval;
		struct js_event_timeout timeout;
	} data;
	int32 id;
	enum js_event_type type;
};

struct js_runq_entry {
	JSFunction *func;
	JSObject *cx;
	struct js_runq_entry *next;
};

struct js_listener_entry {
	char *name;
	JSFunction *func;
	int32 id;
	struct js_listener_entry *next;
};

typedef struct js_callback {
	struct js_event_list	*events;
	struct js_runq_entry    *rq_head;
	struct js_runq_entry    *rq_tail;
	struct js_listener_entry *listeners;
	volatile bool*	terminated;
	struct js_callback	*parent_cb;
	uint32_t		counter;
	uint32_t		limit;
	uint32_t		yield_interval;
	uint32_t		gc_interval;
	uint32_t		gc_attempts;
	uint32_t		offline_counter;
	int32			next_eid;
	JSBool			auto_terminate;
	bool			auto_terminated;
	JSBool			keepGoing;
	bool			bg;
	bool			events_supported;
} js_callback_t;
#endif

/* Synchronet Node Instance class definition */
#if defined(__cplusplus) && defined(JAVASCRIPT)

#include <atomic>
#include <string>
#include <unordered_map>

class Terminal;

enum sftp_dir_tree {
	SFTP_DTREE_FULL,
	SFTP_DTREE_SHORT,
	SFTP_DTREE_VIRTUAL
};

typedef struct sftp_dirdes {
	bool is_static;
	enum sftp_dir_tree tree;
	union {
		struct {
			int lib;
			int dir;
			int32_t idx;
		} filebase;
		struct {
			int32_t idx;
			void *mapping;
		} rootdir;
	} info;
} *sftp_dirdescriptor_t;

typedef struct sftp_filedes {
	char *local_path;    // Needed to get size and record transfer
	uint32_t idx_offset; // TODO: Not needed?  idx_number is likely better
	uint32_t idx_number; // Used when recording transfer
	int fd;              // File descriptor
	int dir;             // Used to record the transfer and to indicate if it's a filebase file
	bool created;        // Basically indicates it's an "upload"
} *sftp_filedescriptor_t;

class cached_mail_count {
	scfg_t* cfg;
	user_t* user;
	bool sent;
	int attr;
	int count{};
	time_t last{};
public:
	cached_mail_count(scfg_t* cfg, user_t* user, bool sent, int attr)
		: cfg(cfg)
		, user(user)
		, sent(sent)
		, attr(attr)
	{}
	int get() {
		if (last == 0 || difftime(time(nullptr), last) >= cfg->stats_interval) {
			count = getmail(cfg, user->number, sent, attr);
			last = time(nullptr);
		}
		return count;
	}
	void reset() {
		last = 0;
	}
};

class sbbs_t
{

public:

	sbbs_t(ushort node_num, union xp_sockaddr *addr, size_t addr_len, const char* host_name, SOCKET
		,scfg_t*, char* text[], client_t* client_info, bool is_event_thread = false);
	~sbbs_t();

	bbs_startup_t*	startup;

	bool	init(void);
	bool	terminated = false;

	client_t client{};
	std::atomic<SOCKET> client_socket{INVALID_SOCKET};
	std::atomic<SOCKET> client_socket_dup{INVALID_SOCKET};
	union xp_sockaddr	client_addr{};
	char	client_name[128]{};
	char	client_ident[128]{};
	char	client_ipaddr[INET6_ADDRSTRLEN]{};
	char	local_addr[INET6_ADDRSTRLEN]{};
#ifdef USE_CRYPTLIB
	CRYPT_SESSION	ssh_session=-1;
#endif
	int session_channel=-1;
	int sftp_channel = -1;
	sftps_state_t sftp_state = nullptr;
	char *sftp_cwd = nullptr;
#define NUM_SFTP_FILEDES 10
	sftp_filedescriptor_t sftp_filedes[NUM_SFTP_FILEDES] {};
#define NUM_SFTP_DIRDES 4
	sftp_dirdescriptor_t sftp_dirdes[NUM_SFTP_DIRDES] {};

	std::atomic<bool> ssh_mode{false};
	std::atomic<bool> term_output_disabled{};
	std::atomic<SOCKET> passthru_socket{INVALID_SOCKET};
	std::atomic<bool> passthru_socket_active{false};
	void   passthru_socket_activate(bool);
	std::atomic<bool> passthru_thread_running{false};

	scfg_t	cfg{};
	struct mqtt* mqtt = nullptr;
	Terminal *term{nullptr};

	int 	rioctl(ushort action); // remote i/o control
	std::atomic<bool> rio_abortable{false};

	RingBuf	inbuf{};
	RingBuf	outbuf{};
	bool	WaitForOutbufEmpty(int timeout) { return WaitForEvent(outbuf.empty_event, timeout) == WAIT_OBJECT_0; }
	bool	flush_output(int timeout) { return online && WaitForOutbufEmpty(timeout); }
	HANDLE	input_thread=nullptr;
	pthread_mutex_t	input_thread_mutex;
	std::atomic<bool> input_thread_mutex_created{false};
	pthread_mutex_t	ssh_mutex;
	std::atomic<bool> ssh_mutex_created{false};
	xpevent_t ssh_active = nullptr;

	#define OUTCOM_RETRY_DELAY		80		// milliseconds
	int		outcom_max_attempts =	1000;	// 80 seconds
	int 	_outcom(uchar ch); 	   // send character, without retry (on buffer full condition)
	int		outcom(uchar ch);		// send character, with retry
	int 	incom(unsigned int timeout=0);		   // receive character
	int 	kbincom(unsigned int timeout=0);	   // " " or return keyboard buffer
	int		translate_input(int ch);
	void	translate_input(uchar* buf, size_t);

	void	spymsg(const char *msg);		// send message to active spies

	int		putcom(const char *str, size_t len=0);  // Send string
	void	hangup(void);		   // Hangup modem

	uchar	telnet_local_option[0x100]{};
	uchar	telnet_remote_option[0x100]{};
	void	send_telnet_cmd(uchar cmd, uchar opt);
	bool	request_telnet_opt(uchar cmd, uchar opt, unsigned waitforack=0);

	uchar	telnet_cmd[64]{};
	uint	telnet_cmdlen = 0;
	uint	telnet_cmds_received = 0;
	std::atomic<uint> telnet_mode{0};
	/* 	input_thread() writes to these variables: */
	uchar	telnet_last_rxch = 0;
	char	telnet_location[128]{};
	char	telnet_terminal[TELNET_TERM_MAXLEN+1]{};
	std::atomic<int> telnet_rows{0};
	std::atomic<int> telnet_cols{0};
	std::atomic<int> telnet_speed{0};

	xpevent_t	telnet_ack_event;

	time_t	event_time = 0;				// Time of next exclusive event
	const char*	event_code = "";			// Internal code of next exclusive event
	bool	is_event_thread = false;
	std::atomic<bool> event_thread_running{false};
	std::atomic<bool> output_thread_running{false};
	std::atomic<bool> input_thread_running{false};
	std::atomic<bool> terminate_output_thread{false};
	char*	event_running_filename(char* str, size_t sz, int event) {
		snprintf(str, sz, "%sevent.%s.running", cfg.data_dir, cfg.event[event]->code);
		return str;
	}

	JSRuntime*		js_runtime = nullptr;
	JSContext*		js_cx = nullptr;
	JSObject*		js_glob = nullptr;
	JSRuntime*		js_hotkey_runtime = nullptr;
	JSContext*		js_hotkey_cx = nullptr;
	JSObject*		js_hotkey_glob = nullptr;
	js_callback_t	js_callback{};
	int				js_execfile(const char *fname, const char* startup_dir = NULL
						,JSObject* scope = NULL, JSContext* cx = NULL, JSObject* glob = NULL);
	int				js_execxtrn(const char *fname, const char* startup_dir);
	JSContext*		js_init(JSRuntime**, JSObject**, const char* desc);
	void			js_cleanup(void);
	bool			js_create_user_objects(JSContext*, JSObject* glob);

	char	syspage_semfile[MAX_PATH+1]{};	/* Sysop page semaphore file */
	char 	menu_dir[128]{};	/* Over-ride default menu dir */
	char 	menu_file[128]{};	/* Over-ride menu file */

	user_t	useron{}; 		/* User currently online */
	node_t	thisnode{};		/* Node information */
	smb_t	smb{};			/* Currently active message base */
	link_list_t smb_list{};
#define SMB_STACK_PUSH	true
#define SMB_STACK_POP	false
	int 	smb_stack(smb_t* smb, bool push);

	enum { user_not_logged_in, user_registering, user_logged_in, user_logged_on } user_login_state{};
	bool	useron_is_guest() { return user_is_guest(&useron); }
	bool	useron_is_sysop() { return user_is_sysop(&useron) || (sys_status & SS_TMPSYSOP); }

	char	rlogin_name[LEN_ALIAS+1]{};
	char	rlogin_pass[LEN_PASS+1]{};
	char	rlogin_term[TELNET_TERM_MAXLEN+1]{};	/* RLogin passed terminal type/speed (e.g. "xterm/57600") */

	FILE	*logfile_fp = nullptr;

	int 	nodefile = -1;	/* File handle for node.dab */
	pthread_mutex_t	nodefile_mutex;
	int		node_ext = -1;	/* File handle for node.exb */
	size_t	batup_total();
	size_t	batdn_total();
	int64_t	batdn_bytes();
	int64_t	batdn_cost();
	uint	batdn_time();

	/********************************/
	/* Text Configuration Variables */
	/********************************/
	char 	*text[TOTAL_TEXT]{};			/* Text from text.dat/text.ini */
	char 	*text_sav[TOTAL_TEXT]{};		/* Text from text.dat/text.ini */
	bool	text_replaced[TOTAL_TEXT]{};
	std::unordered_map<std::string, int> text_id_map{};
	char	yes_key(void) { return toupper(*text[Yes]); }
	char	no_key(void) { return toupper(*text[No]); }
	char	quit_key(void) { return toupper(*text[Quit]); }
	char*	quit_key(char* str) { str[0] = quit_key(); str[1] = '\0'; return str; }
	char	all_key(void) { return toupper(*text[All]); }
	char	list_key(void) { return toupper(*text[List]); }
	char	next_key(void) { return toupper(*text[Next]); }
	char	prev_key(void) { return toupper(*text[Previous]); }

	char 	dszlog[127]{};	/* DSZLOG environment variable */
	int     keybuftop=0, keybufbot=0;    /* Keyboard input buffer pointers (for ungetkey) */
	char    keybuf[KEY_BUFSIZE]{};    /* Keyboard input buffer */
	size_t	keybuf_space(void);
	size_t	keybuf_level(void);

	ushort	node_connection = NODE_CONNECTION_TELNET;
	char	connection[LEN_CONNECTION+1];	/* Connection Description */
	uint	cur_rate=0;		/* Current Connection (DCE) Rate */
	uint	cur_cps=10000;	/* Current Average Download CPS */
	uint	dte_rate=0;		/* Current COM Port (DTE) Rate */
	time_t 	getkey_last_activity=0;		/* User inactivity timeout reference */
	uint 	timeleft_warn=0;/* low timeleft warning flag */
	std::atomic<uint> socket_inactive{0}; // Socket inactivity counter (watchdog), in seconds, incremented by input_thread()
	std::atomic<uint> max_socket_inactivity{0};	// Socket inactivity limit (in seconds), enforced by input_thread()
	std::atomic<bool> socket_inactivity_warning_sent{false};
	uint	curatr = LIGHTGRAY;     /* Last Attributes requested by attr() */
	uint	attr_stack[64]{};	/* Saved attributes (stack) */
	int 	attr_sp = 0;	/* Attribute stack pointer */
	uint	saved_pcb_attr{LIGHTGRAY};
	uint	mneattr_low = LIGHTGRAY;
	uint	mneattr_high = LIGHTGRAY;
	uint	mneattr_cmd = LIGHTGRAY;
	uint	rainbow[LEN_RAINBOW + 1]{};
	bool	rainbow_wrap = true;
	int		rainbow_index = -1;
	int		rainbow_len() { int len = 0; for (len = 0; len < LEN_RAINBOW; ++len) if (rainbow[len] == 0) break; return len; }
	bool	msghdr_tos = false;	/* Message header was displayed at Top of Screen */
	int 	autoterm=0;		/* Auto-detected terminal type */
	size_t	unicode_zerowidth=0;
	char	terminal[TELNET_TERM_MAXLEN+1]{};	// <- answer() writes to this
	uint	line_delay=0;	/* Delay duration (ms) after each line sent */
	uint	console = 0;	/* Defines current Console settings */
	char 	wordwrap[TERM_COLS_MAX + 1]{};	/* Word wrap buffer */
	time_t	now=0,			/* Used to store current time in Unix format */
			last_sysop_auth=0,/* Time sysop was last authenticated */
			answertime=0, 	/* Time call was answered */
			logontime=0,	/* Time user logged on */
			starttime=0,	/* Time stamp to use for time left calcs */
			ns_time=0,		/* File new-scan time */
			last_ns_time=0;	/* Most recent new-file-scan this call */
	uint    timeon() { int result = (int)(time(&now) - logontime); if (result < 0) result = 0; return result; }
	uint    timeused() { int result = (int)(time(&now) - starttime); if (result < 0) result = 0; return result; }
	uint    useron_minutes_today() { return useron.ttoday + (timeon() / 60); }
	uint    useron_minutes_total() { return useron.timeon + (timeon() / 60); }
	uchar 	action = NODE_MAIN;		/* Current action of user */
	std::atomic<int> online{0}; 	/* Remote/Local or not online */
	std::atomic<int> sys_status{0};	/* System Status */
	subscan_t* subscan = nullptr;	/* User sub configuration/scan info */

	cached_mail_count mail_waiting{&cfg, &useron, false, 0};
	cached_mail_count mail_read{&cfg, &useron, false, MSG_READ};
	cached_mail_count mail_unread{&cfg, &useron, false, ~MSG_READ};
	cached_mail_count mail_pending{&cfg, &useron, true, 0};
	cached_mail_count spam_waiting{&cfg, &useron, false, MSG_SPAM};

	int64_t	logon_ulb=0,	/* Upload Bytes This Call */
			logon_dlb=0;	/* Download Bytes This Call */
	uint	logon_uls=0,	/* Uploads This Call */
			logon_dls=0;	/* Downloads This Call */
	uint	logon_posts=0,	/* Posts This Call */
			logon_emails=0,	/* Emails This Call */
			logon_fbacks=0;	/* Feedbacks This Call */
	uchar	logon_ml = 0;	/* Security level of the user upon logon */

	uint 	main_cmds = 0;		/* Number of Main Commands this call */
	uint 	xfer_cmds = 0;		/* Number of Xfer Commands this call */
	uint	posts_read = 0; 	/* Number of Posts read this call */
	bool 	autohang = false;	/* Used for auto-hangup after transfer */
	size_t 	logcol = 1; 		/* Current column of log file */
	uint 	criterrs = 0;		/* Critical error counter */

	int 	curgrp = 0;				/* Current group */
	int*	cursub = nullptr;		/* Current sub-board for each group */
	int		curlib = 0; 			/* Current library */
	int*	curdir = nullptr;		/* Current directory for each library */
	int*	usrgrp = nullptr;		/* Real group numbers */
	int		usrgrps = 0;			/* Number groups this user has access to */
	int		usrgrp_total = 0;		/* Total number of groups */
	int	*	usrlib = nullptr;		/* Real library numbers */
	int		usrlibs = 0;			/* Number of libs this user can access */
	int		usrlib_total = 0;		/* Total number of libraries */
	int**	usrsub = nullptr;		/* Real sub numbers */
	int*	usrsubs = nullptr;		/* Num of subs with access for each grp */
	int**	usrdir = nullptr;		/* Real dir numbers */
	int*	usrdirs = nullptr;		/* Num of dirs with access for each lib */
	int		cursubnum = INVALID_SUB;	/* For ARS */
	int		curdirnum = INVALID_DIR;	/* For ARS */
	uint 	timeleft = 60 * 10;	/* Number of seconds user has left online */

	int current_subnum() {	if (SMB_IS_OPEN(&smb)) return smb.subnum; return usrgrps ? usrsub[curgrp][cursub[curgrp]] : INVALID_SUB; }

	char 	*comspec = nullptr;	/* Pointer to environment variable COMSPEC */
	char 	cid[LEN_CID+1]{}; /* Caller ID (IP Address) of current caller */
	char 	*noaccess_str = nullptr;	/* Why access was denied via ARS */
	int 	noaccess_val = 0;	/* Value of parameter not met in ARS */
	int		errorlevel = 0;	/* Error level of external program */

	csi_t	main_csi{};		/* Main Command Shell Image */
	char    optext[256]{}; // See bbs.optext property and OPTEXT @-code

	const smbmsg_t*	current_msg = nullptr;	/* For message header @-codes */
	const char*	current_msg_subj = nullptr;
	const char*	current_msg_from = nullptr;
	const char*	current_msg_to = nullptr;
	uint32_t current_msg_number = 0;
	file_t*	current_file = nullptr;

			/* Global command shell variables */
	int		global_str_vars = 0;
	char **	global_str_var = nullptr;
	uint32_t *	global_str_var_name = nullptr;
	int		global_int_vars = 0;
	int32_t *	global_int_var = nullptr;
	uint32_t *	global_int_var_name = nullptr;
	char *	sysvar_p[MAX_SYSVARS]{};
	uint	sysvar_pi = 0;
	long	sysvar_l[MAX_SYSVARS]{};
	uint	sysvar_li = 0;

			/* Command Shell Methods */
	int		exec(csi_t *csi);
	int		exec_function(csi_t *csi);
	int		exec_misc(csi_t *csi, const char *path);
	int		exec_net(csi_t *csi);
	int		exec_msg(csi_t *csi);
	int		exec_file(csi_t *csi);
	int		exec_bin(const char *mod, csi_t *csi, const char* startup_dir=NULL);
	int		exec_mod(const char* name, struct loadable_module, bool* invoked = nullptr, const char* fmt = nullptr, ...)
#if defined(__GNUC__)   // Catch printf-format errors
    __attribute__ ((format (printf, 5, 6)))
#endif
	;
	str_list_t mod_callstack{};
	void	clearvars(csi_t *bin);
	void	freevars(csi_t *bin);
	char**	getstrvar(csi_t *bin, uint32_t name);
	int32_t* getintvar(csi_t *bin, uint32_t name);
	char*	copystrvar(csi_t *csi, char *p, char *str);
	void	skipto(csi_t *csi, uchar inst);
	bool	ftp_cmd(csi_t* csi, SOCKET ctrl_sock, const char* cmdsrc, char* rsp);
	bool	ftp_put(csi_t* csi, SOCKET ctrl_sock, char* src, char* dest);
	bool	ftp_get(csi_t* csi, SOCKET ctrl_sock, char* src, char* dest, bool dir=false);
	SOCKET	ftp_data_sock(csi_t* csi, SOCKET ctrl_sock, SOCKADDR_IN*);

	bool	select_shell(user_t*);
	bool	select_editor(user_t*);

	bool	set_shell(int shell_index);
	bool	set_shell(const char* code);
	bool	set_editor(const char* code);

	void	sys_info(void);
	void	user_info(void);
	void	xfer_policy(void);

	char*	xfer_prot_menu(enum XFER_TYPE, user_t* user = nullptr, char* buf = nullptr, size_t size = 0);
	void	node_stats(uint node_num);
	void	sys_stats(void);
	void	logonlist(const char* args = "");
	bool	spy(uint node_num);

	void	reset_logon_vars(void);

	uint	finduser(const char* str, bool silent_failure = false);

	int 	sub_op(int subnum);
	bool	can_view_deleted_msgs(int subnum);
	int		dir_op(int dirnum);
	bool	subnum_is_valid(int subnum) { return ::subnum_is_valid(&cfg, subnum); }
	bool	dirnum_is_valid(int dirnum) { return ::dirnum_is_valid(&cfg, dirnum); }
	char*	dir_name(int dirnum) { return ::dir_name(&cfg, dirnum); }
	char*	lib_name(int dirnum) { return ::lib_name(&cfg, dirnum); }
	char*	sub_name(int subnum) { return ::sub_name(&cfg, subnum); }
	char*	grp_name(int subnum) { return ::grp_name(&cfg, subnum); }

	void	getmsgptrs(void);
	void	putmsgptrs(void);
	void	reinit_msg_ptrs(void);
	void	getusrsubs(void);
	void	getusrdirs(void);
	int		getusrsub(int subnum);
	int		getusrgrp(int subnum);
	int		getusrdir(int dirnum);
	int		getusrlib(int dirnum);
	bool	getuseron(int line, const char* function, const char *source, uint usernum = 0);

	bool	putuserstr(int usernumber, enum user_field, const char *str);
	bool	putuserdatetime(int usernumber, enum user_field, time_t t);
	bool	putuserflags(int usernumber, enum user_field, uint32_t flags);
	bool	putuserhex32(int usernumber, enum user_field, uint32_t value);
	bool	putuserdec32(int usernumber, enum user_field, uint32_t value);
	bool	putuserdec64(int usernumber, enum user_field, uint64_t value);
	bool	putusermisc(int usernumber, uint32_t value);
	bool	putuserchat(int usernumber, uint32_t value);
	bool	putuserqwk(int usernumber, uint32_t value);
	bool	putuserdat(user_t*);

	uint	finduserstr(uint usernumber, enum user_field, const char* str
				,bool del=false, bool next=false);
	uint	gettimeleft(bool handle_out_of_time=true);
	bool	gettimeleft_inside = false;

	/* str.cpp */
	char	format_text_buf[256]{};
	char*	format_text(int /* enum text */, ...);
	char*	format_text(enum text, smbmsg_t*, ...);
	int		get_text_num(const char* id);
	const char* get_text(const char* id);
	bool	load_user_text(void);
	bool	replace_text(const char* path);
	void	revert_text(void);
	char*	server_host_name(void);
	char*	timestr(time_t);
	char*	datestr(time_t, char* str = nullptr);
	char*	unixtodstr(time_t, char* str = nullptr);
	char	timestr_output[60]{};
	char	datestr_output[60]{};
	char*	age_of_posted_item(char* buf, size_t max, time_t);
	void	userlist(int mode);
	size_t	gettmplt(char *outstr, const char *tmplt, int mode);
	void	sif(char *fname, char *answers, int len);	/* Synchronet Interface File */
	void	sof(char *fname, char *answers, int len);
	void	create_sif_dat(char *siffile, char *datfile);
	void	read_sif_dat(char *siffile, char *datfile);
	void	printnodedat(uint number, node_t* node);
	bool	inputnstime32(time32_t *dt);
	bool	inputnstime(time_t *dt);
	bool	chkpass(char *pass, user_t* user, bool unique = false);
	char *	cmdstr(const char *instr, const char *fpath, const char *fspec, char *outstr, int mode = EX_UNSPECIFIED);
	char	cmdstr_output[512]{};
	char*	ultoac(uint32_t, char*, char sep=',');
	char*	u64toac(uint64_t, char*, char sep=',');
	int		protnum(char prot, enum XFER_TYPE xfer_type = XFER_DOWNLOAD);
	const char* protname(char prot, enum XFER_TYPE xfer_type = XFER_DOWNLOAD);

	void	subinfo(int subnum);
	void	dirinfo(int dirnum);
	bool	trashcan(const char *insearch, const char *name, struct trash* trash = NULL);
	void	trashcan_msg(const char* name);
	void	time_bank(void);
	bool	change_user(const char* username = nullptr);

	/* putmsg.cpp */
	char	putmsg(const char *str, int mode, int org_cols = 0, JSObject* obj = NULL);
	char	putmsgfrag(const char* str, int& mode, unsigned org_cols = 0, JSObject* obj = NULL);
	bool	putnmsg(int node_num, const char*);
	bool	putsmsg(int user_num, const char*);
	ANSI_Parser ansiParser{};

	/* writemsg.cpp */
	void	automsg(void);
	bool	writemsg(const char *str, const char *top, char *subj, int mode, int subnum
				,const char *to, const char* from, const char** editor=NULL, const char** charset=NULL);
	char*	quotes_fname(int xedit, char* buf, size_t len);
	char*	msg_tmp_fname(int xedit, char* fname, size_t len);
	bool	msgabort(bool clear = false);
	void	clearabort();
	bool	email(int usernumber, const char *top = NULL, const char *title = NULL
				, int mode = WM_NONE, smb_t* resmb = NULL, smbmsg_t* remsg = NULL);
	bool	forwardmsg(smb_t*, smbmsg_t*, const char* to, const char* subject = NULL, const char* comment = NULL);
	void	removeline(char *str, char *str2, char num, char skip);
	uint	msgeditor(char *buf, const char *top, char *title, uint max_lines, uint max_line_len);
	bool	editfile(char *path, uint max_lines = 10000, int wmode = WM_NONE, const char* to = NULL, const char* from = NULL, const char* subj = NULL, const char* msgarea = NULL, bool clean_quotes = true);
	ushort	chmsgattr(const smbmsg_t*);
	bool	quotemsg(smb_t*, smbmsg_t*, bool tails = false);
	bool	editmsg(smb_t*, smbmsg_t*);
	void	editor_inf(int xeditnum, const char *to, const char* from, const char *subj, int mode
				,const char* msgarea, const char* tagfile = NULL);
	bool	copyfattach(uint to, uint from, const char* subj);
	bool	movemsg(smbmsg_t* msg, int subnum);
	int		process_edited_text(char* buf, FILE* stream, int mode, unsigned* lines, unsigned maxlines);
	int		process_edited_file(const char* src, const char* dest, int mode, unsigned* lines, unsigned maxlines);
	void	editor_info_to_msg(smbmsg_t*, const char* editor, const char* charset);
	char	editor_details[128]{};

	/* postmsg.cpp */
	bool	postmsg(int subnum, int wm_mode = WM_NONE, smb_t* resmb = NULL, smbmsg_t* remsg = NULL);
	bool	notify(const char* subject, bool strip_ctrl = false, const char* text = NULL, const char* replyto = NULL);

	/* mail.cpp */
	int		delmail(uint usernumber,int which);
	void	telluser(smbmsg_t* msg);
	int		delallmail(uint usernumber, int which, bool permanent=true, int lm_mode = 0);

	/* getmsg.cpp */
	int		loadmsg(smbmsg_t *msg, uint number);
	void	show_msgattr(const smbmsg_t*);
	void	show_msghdr(smb_t*, const smbmsg_t*, const char *subj = NULL, const char* from = NULL, const char* to = NULL);
	bool	show_msg(smb_t*, smbmsg_t*, int p_mode = 0, post_t* post = NULL);
	bool	msgtotxt(smb_t*, smbmsg_t*, const char *fname, bool header = true, uint gettxt_mode = GETMSGTXT_ALL);
	const char* msghdr_text(const smbmsg_t*, uint index);
	char	msghdr_utf8_text[128]{};
	const char* msghdr_field(const smbmsg_t*, const char* str, char* buf = NULL, bool can_utf8 = false);
	char	msgghdr_field_cp437_str[128]{};
	uint	getlastmsg(int subnum, uint32_t *ptr, time_t *t);
	time_t	getmsgtime(int subnum, uint ptr);
	int		getmsgnum(int subnum, time_t t);
	void	download_msg_attachments(smb_t*, smbmsg_t*, bool del, bool use_default_prot = false);

	/* readmail.cpp */
	int		readmail(uint usernumber, int which, int lm_mode = 0, bool listmsgs = true);
	int		searchmail(mail_t*, int start, int msgss, int which, const char *search, const char* order);

	/* bulkmail.cpp */
	bool	bulkmail(uchar *ar);
	int		bulkmailhdr(smb_t*, smbmsg_t*, uint usernum);

	/* con_out.cpp */
	int		bputs(const char *str, int mode = 0);	/* BBS puts function */
	int		bputs(int mode, const char* str) { return bputs(str, mode); }
	size_t	rputs(const char *str, size_t len=0);	/* BBS raw puts function */
	int		rputs(int mode, const char* str) { return rputs(str, mode); }
	int		bprintf(const char *fmt, ...)			/* BBS printf function */
#if defined(__GNUC__)   // Catch printf-format errors
    __attribute__ ((format (printf, 2, 3)))			// 1 is 'this'
#endif
	;
	int		bprintf(int mode, const char *fmt, ...)
#if defined(__GNUC__)   // Catch printf-format errors
    __attribute__ ((format (printf, 3, 4)))			// 1 is 'this', 2 is 'mode'
#endif
	;
	int		rprintf(const char *fmt, ...)			/* BBS raw printf function */
#if defined(__GNUC__)   // Catch printf-format errors
    __attribute__ ((format (printf, 2, 3)))			// 1 is 'this'
#endif
	;
	int		term_printf(const char *fmt, ...)			/* BBS direct-comm printf function */
#if defined(__GNUC__)   // Catch printf-format errors
    __attribute__ ((format (printf, 2, 3)))			// 1 is 'this'
#endif
	;
	int		outchar(char ch);				/* Output a char - check echo and emu.  */
	int		cls() { return outchar(FF); }	// Clear the screen
	bool	pause_enabled();		// Check if screen pausing is enabled
	bool	check_pause();			// Check line counter (lncntr) and pause() if appropriate
	int		outcp(enum unicode_codepoint, char cp437_fallback);
	int		outcp(enum unicode_codepoint, const char* cp437_fallback = NULL);
	void	wide(const char*);
	// These are user settings, not terminal properties.
	char*	term_rows(user_t*, char* str, size_t);
	char*	term_cols(user_t*, char* str, size_t);
	char*	term_type(user_t*, int term, char* str, size_t);
	const char* term_type(int term_supports = -1);
	const char* term_charset(int term_supports = -1);
	bool	update_nodeterm(void);
	int		backfill(const char* str, float pct, int full_attr, int empty_attr);
	void	progress(const char* str, int count, int total, int interval = 500);
	double	last_progress = 0;
	int		petscii_to_ansibbs(unsigned char);
	size_t	print_utf8_as_cp437(const char*, size_t);
	int		attr(int);				/* Change text color/attributes */
	void	ctrl_a(char);			/* Performs Ctrl-Ax attribute changes */
	char*	auto_utf8(const char*, int& mode);
	void	getdimensions();
	size_t term_out(const char *str, size_t len = SIZE_MAX);
	size_t term_out(int ch);
	size_t cp437_out(const char *str, size_t len = SIZE_MAX);
	size_t cp437_out(int ch);

	/* getstr.cpp */
	size_t	getstr_offset = 0;
	size_t	getstr(char *str, size_t length, int mode, const str_list_t history = NULL);
	int		getnum(uint max, uint dflt=0);

	/* getkey.cpp */
	char	getkey(int mode = K_NONE);
	int		getkeys(const char *str, uint max, int mode = K_UPPER);
	bool	ungetkey(char ch, bool insert = false);		/* Places 'ch' into the input buffer    */
	bool	ungetkeys(const char* str, bool insert = false);
	char	question[MAX_TEXTDAT_ITEM_LEN+1]{};
	bool	yesno(const char *str, int mode = 0);
	bool	noyes(const char *str, int mode = 0);
	bool	confirm(const char* str, bool dflt, int mode = 0) { return dflt ? yesno(str, mode) : !noyes(str, mode); };
	bool	pause_inside = false;
	bool	pause(bool set_abort = true);
	const char*	mnestr = nullptr;
	void	mnemonics(const char *str);

	/* inkey.cpp */
	bool last_inkey_was_esc{false}; // Used by auto-ANSI detection
	int		inkey(int mode = K_NONE, unsigned int timeout=0);
	char	handle_ctrlkey(char ch, int mode=0);

									// Terminal mouse reporting mode (mouse_mode)
#define MOUSE_MODE_OFF		0		// No terminal mouse reporting enabled/expected
#define MOUSE_MODE_X10		(1<<0)	// X10 compatible mouse reporting enabled
#define MOUSE_MODE_NORM		(1<<1)	// Normal tracking mode mouse reporting
#define MOUSE_MODE_BTN		(1<<2)	// Button-event tracking mode mouse reporting
#define MOUSE_MODE_ANY		(1<<3)	// Any-event tracking mode mouse reporting
#define MOUSE_MODE_EXT		(1<<4)	// SGR-encoded extended coordinate mouse reporting
#define MOUSE_MODE_ON		(MOUSE_MODE_NORM | MOUSE_MODE_EXT) // Default mouse "enabled" mode flags

	int		mouse_mode = MOUSE_MODE_OFF;	// Mouse reporting mode flags
	uint	hot_attr = 0;		// Auto-Mouse hot-spot attribute (when non-zero)
	bool	hungry_hotspots = true;

	stats_t stats{}; // cached statistics

	// Thread-safe std/socket errno description getters
	char	strerror_buf[256]{};
	const char* strerror(int errnum) { return safe_strerror(errnum, strerror_buf, sizeof strerror_buf); }
	const char* socket_strerror(int errnum) { return ::socket_strerror(errnum, strerror_buf, sizeof strerror_buf); }

	/* prntfile.cpp */
	char*	fgetline(char* s, int size, int cols, FILE*);
	bool	printfile(const char* fname, int mode, int org_cols = 0, JSObject* obj = NULL);
	bool	printtail(const char* fname, int lines, int mode, int org_cols = 0, JSObject* obj = NULL);
	bool	menu(const char *code, int mode = 0, JSObject* obj = NULL);
	bool	random_menu(const char *code, int mode = 0, JSObject* obj = NULL);
	bool	menu_exists(const char *code, const char* ext=NULL, char* realpath=NULL);

	int		uselect(bool add, uint n, const char *title, const char *item, const uchar *ar);
	uint	uselect_count = 0, uselect_num[500]{};

	int		mselect(const char *title, str_list_t list, unsigned max_selections, const char* item_fmt, const char* selected_str, const char* unselected_str, const char* prompt_fmt);

	void	redrwstr(char *strin, int i, int l, int mode);

	/* atcodes.cpp */
	int		show_atcode(const char *code, uint cols = 0, JSObject* obj = NULL);
	const char*	atcode(const char* sp, char* str, size_t maxlen, int* pmode = NULL, bool centered = false, uint cols = 0, JSObject* obj = NULL);
	const char* formatted_atcode(const char* sp, char* str, size_t maxlen);
	char* expand_atcodes(const char* src, char* buf, size_t size, const smbmsg_t* msg = NULL);

	/* getnode.cpp */
	bool	getsmsg(int usernumber, bool clearline = false);
	bool	getnmsg(bool clearline = false);
	void	whos_online(bool listself); // Lists active nodes
	void	nodelist(void);
	bool	getnodeext(uint number, char * str);
	bool	getnodedat(uint number, node_t * node, bool lock = false);
	void	nodesync(bool clearline = false);
	user_t	nodesync_user{};
	bool	nodesync_inside = false;
	uint	count_nodes(bool self = true);
	void	sync(bool clearline = false) { getnodedat(cfg.node_num, &thisnode); nodesync(clearline); }

	/* putnode.cpp */
	bool	putnodedat(uint number, node_t * node);
	bool	putnodeext(uint number, char * str);
	bool	putnode_downloading(off_t size);
	bool	unlocknodedat(uint number);

	/* login.ccp */
	int		login(const char *user_name, const char *pw_prompt, const char* user_pw = NULL, const char* sys_pw = NULL);
	void	badlogin(const char* user, const char* passwd, const char* protocol=NULL, xp_sockaddr* addr=NULL, bool delay=true);
	const char*	parse_login(const char*);

	/* answer.cpp */
	bool    set_authresponse(bool activate_ssh);
	bool	answer();

	/* logon.ccp */
	bool	logon();
	bool	logon_process();

	/* logout.cpp */
	void	logout();
	bool	logoff(bool prompt = false);

	/* newuser.cpp */
	bool	newuser(void);					/* Get new user							*/

	/* text_sec.cpp */
	void	text_sec(void);						/* Text sections */

	/* readmsgs.cpp */
	post_t* loadposts(uint32_t *posts, int subnum, uint ptr, int mode, uint *unvalidated_num, uint32_t* visible=NULL);
	int		scanposts(int subnum, int mode, const char* find);	/* Scan sub-board */
	bool	scanposts_inside = false;
	int		listsub(int subnum, int mode, int start, const char* search);
	int		listmsgs(int subnum, int mode, post_t* post, int start, int posts, bool reading = true);
	int		searchposts(int subnum, post_t* post, int start, int msgs, const char* find);
	int		showposts_toyou(int subnum, post_t* post, uint start, int posts, int mode=0);
	void	show_thread(uint32_t msgnum, post_t* post, unsigned curmsg, int thread_depth = 0, uint64_t reply_mask = 0);
	void	dump_msghdr(smbmsg_t*);
	uchar	msg_listing_flag(int subnum, smbmsg_t*, post_t*);
	int64_t get_start_msgnum(smb_t*, int next=0);

	/* chat.cpp */
	void	chatsection(void);
	void	multinodechat(int channel=1);
	void	nodepage(void);
	void	nodemsg(void);
	uint	nodemsg_inside = 0;
	uint	hotkey_inside = 0;
	uchar	lastnodemsg = 0;	/* Number of node last message was sent to */
	char	lastnodemsguser[LEN_ALIAS+1]{};
	void	guruchat(char* line, char* guru, int gurunum, char* last_answer);
	bool	guruexp(char **ptrptr, char *line);
	void	localguru(char *guru, int gurunum);
	bool	sysop_page(void);
	bool	guru_page(void);
	void	privchat(bool forced=false, int node_num=0);
	bool	chan_access(int cnum);
	int		getnodetopage(int all, int telegram);

	/* main.cpp */
	void    log_crypt_error_status_sock(int status, const char *action);
	int		lputs(int level, const char* str);
	int		lprintf(int level, const char *fmt, ...)
#if defined(__GNUC__)   // Catch printf-format errors
    __attribute__ ((format (printf, 3, 4)))			// 1 is 'this'
#endif
	;
	int		errprintf(int level, int line, const char* function, const char* file, const char *fmt, ...)
#if defined(__GNUC__)   // Catch printf-format errors
    __attribute__ ((format (printf, 6, 7)))			// 1 is 'this'
#endif
	;
	uint	logonstats(void);
	void	logoffstats(void);
	void    register_login(void);
	int		nopen(char *str, int access);
	int		mv(const char *src, const char *dest, bool copy); /* fast file move/copy function */
	bool	chksyspass(const char* sys_pw = NULL);
	bool	chk_ar(const uchar * str, user_t* user, client_t* client); /* checks access requirements */
	bool	chk_ars(const char * str, user_t* user, client_t* client);
	bool	ar_exp(const uchar ** ptrptr, user_t*, client_t*);
	void	daily_maint(void);
	int64_t	backup(const char* fname, int backup_level, bool rename);

	/* upload.cpp */
	bool	uploadfile(file_t* f);
	bool	okay_to_upload(int dirnum);
	bool	upload(int dirnum, const char* fname = nullptr);
	char	upload_lastdesc[LEN_FDESC+1]{};
	bool	bulkupload(int dirnum);

	/* download.cpp */
	void	data_transfer_begin(uchar& local_binary_tx, uchar& remote_binary_tx);
	void	data_transfer_end(uchar local_binary_tx, uchar remote_binary_tx);
	void	downloadedfile(file_t* f);
	void	notdownloaded(off_t size, time_t elapsed);
	void	downloadedbytes(off_t size, time_t elapsed);
	int		protocol(prot_t* prot, enum XFER_TYPE, const char *fpath, const char *fspec, bool cd, bool autohangup=true, time_t* elapsed=NULL);
	const char*	protcmdline(prot_t* prot, enum XFER_TYPE type);
	void	seqwait(uint devnum);
	void	autohangup(void);
	bool	checkdszlog(const char*);
	bool	checkprotresult(prot_t*, int error, const char* fpath);
	bool	checkprotresult(prot_t*, int error, file_t*);
	bool	sendfile(file_t*, char prot, bool autohang);
	bool	sendfile(char* fname, char prot=0, const char* description = NULL, bool autohang=true);
	bool	recvfile(char* fname, char prot=0, bool autohang=true);

	/* file.cpp */
	void	showfileinfo(file_t*, bool show_extdesc = true);
	bool	removefcdt(file_t*);
	bool	removefile(smb_t*, file_t*);
	bool	movefile(smb_t*, file_t*, int newdir);
	char *	getfilespec(char *str);
	bool	checkfname(const char *fname);
	bool	addtobatdl(file_t*);
	bool	clearbatdl(void);
	bool	clearbatul(void);
	bool	editfilename(file_t*);
	bool	editfiledesc(file_t*);
	bool	editfileinfo(file_t*);
	int		delfiles(const char *inpath, const char *spec, size_t keep = 0);

	/* listfile.cpp */
	bool	listfile(file_t*, int dirnum, const char *search, const char letter, size_t namelen);
	int		listfiles(int dirnum, const char *filespec, FILE* tofile, int mode);
	int		listfileinfo(int dirnum, const char *filespec, int mode);
	void	listfiletofile(file_t*, FILE*);
	int		batchflagprompt(smb_t*, file_t* bf[], uint row[], int total, int totalfiles);

	/* bat_xfer.cpp */
	void	batchmenu(void);
	void	batch_add_list(char *list);
	bool	create_batchup_lst(void);
	bool	create_batchdn_lst(bool native);
	bool	batch_upload(void);
	bool	process_batch_upload_queue(void);
	void	batch_download(int xfrprot);
	bool	start_batch_download(void);

	/* tmp_xfer.cpp */
	void	temp_xfer(void);
	const char*	temp_cmd(int& ex_mode);					/* Returns temp file command line */
	uint	create_filelist(const char *name, int mode);

	/* viewfile.cpp */
	int		viewfile(file_t* f, bool extdesc);
	bool	viewfile(const char* path);
	bool	viewfilecontents(file_t* f);

	/* xtrn.cpp */
	int		external(const char* cmdline, int mode, const char* startup_dir=NULL);
	int		xtrn_mode = 0;
	char	term_env[256]{};

	/* xtrn_sec.cpp */
	void	xtrn_sec(const char* section = "");	/* The external program section  */
	void	xtrndat(const char* name, const char* dropdir, uchar type, uint tleft
				,uint misc);
	bool	exec_xtrn(uint xtrnnum, bool user_event = false);	/* Executes online external program */
	bool	user_event(user_event_t);			/* Executes user event(s) */
	void	moduserdat(uint xtrnnum);
	const char* xtrn_dropdir(const xtrn_t*, char* buf, size_t);

	/* logfile.cpp */
	void	logentry(const char *code,const char *entry);
	void	log(const char *str);				/* Writes 'str' to node log */
	void	logch(char ch, bool comma);	/* Writes 'ch' to node log */
	void	logline(const char *code,const char *str); /* Writes 'str' on it's own line in log (LOG_INFO level) */
	void	logline(int level, const char *code,const char *str);
	void	llprintf(int level, const char* code, const char *fmt, ...)
#if defined(__GNUC__)   // Catch printf-format errors
    __attribute__ ((format (printf, 4, 5)))			// 1 is 'this', 2 is 'level', 3 is 'code'
#endif
	;
	void	llprintf(const char* code, const char *fmt, ...)
#if defined(__GNUC__)   // Catch printf-format errors
    __attribute__ ((format (printf, 3, 4)))			// 1 is 'this', 2 is 'code'
#endif
	;

	bool	logofflist(void);              /* List of users logon activity */
	bool	errormsg_inside = false;
	void	errormsg(int line, const char* function, const char *source, const char* action, const char *object
				,int access=0, const char *extinfo=NULL);
	bool	hacklog(const char* prot, const char* text);
	void	fremove(int line, const char* function, const char *source, const char* path, bool log_all_errors = false) {
		int result = remove(path);
		if(result != 0 && (log_all_errors == true || errno != ENOENT))
			errormsg(line, function, source, ERR_REMOVE, path);
	}

	/* qwk.cpp */
	uint	qwkmail_last = 0;
	void	qwk_sec(void);
	uint	total_qwknodes = 0;
	struct qwknode {
		char	id[LEN_QWKID+1];
		char	path[MAX_PATH+1];
		time_t	time;
	}* qwknode = nullptr;
	void	update_qwkroute(char *via);
	void	qwk_success(uint msgcnt, char bi, char prepack);
	void	qwksetptr(int subnum, char *buf, int reset);
	void	qwkcfgline(char *buf,int subnum);
	bool	set_qwk_flag(uint flag);
	uint	resolve_qwkconf(uint confnum, int hubnum=-1);
	struct msg_filters {
		str_list_t ip_can;
		str_list_t host_can;
		str_list_t subject_can;
		str_list_t twit_list;
	};
	bool	qwk_msg_filtered(smbmsg_t* msg, msg_filters);
	bool	qwk_vote(str_list_t ini, const char* section, smb_net_type_t, const char* qnet_id, uint confnum, msg_filters, int hubnum);
	bool	qwk_voting(str_list_t* ini, int offset, smb_net_type_t, const char* qnet_id, uint confnum, msg_filters, int hubnum = -1);
	void	qwk_handle_remaining_votes(str_list_t* ini, smb_net_type_t, const char* qnet_id, int hubnum = -1);

	/* pack_qwk.cpp */
	bool	pack_qwk(char *packet, uint *msgcnt, bool prepack);

	/* un_qwk.cpp */
	bool	unpack_qwk(char *packet,uint hubnum);

	/* pack_rep.cpp */
	bool	pack_rep(uint hubnum);

	/* un_rep.cpp */
	bool	unpack_rep(char* repfile=NULL);

	/* msgtoqwk.cpp */
	int		msgtoqwk(smbmsg_t* msg, FILE *qwk_fp, int mode, smb_t*, int conf, FILE* hdrs_dat, FILE* voting_dat = NULL);

	/* qwktomsg.cpp */
	bool	qwk_new_msg(uint confnum, smbmsg_t* msg, char* hdrblk, int offset, str_list_t headers, bool parse_sender_hfields);
	bool	qwk_import_msg(FILE *qwk_fp, char *hdrblk, uint blocks, char fromhub, smb_t*
				,uint touser, smbmsg_t* msg, bool* dupe);

	/* fido.cpp */
	bool	netmail(const char *into, const char *subj = NULL, int mode = WM_NONE, smb_t* resmb = NULL, smbmsg_t* remsg = NULL, str_list_t cc = NULL);
	void	qwktonetmail(FILE *rep, char *block, char *into, uchar fromhub = 0);
	bool	lookup_netuser(char *into);

	bool	inetmail(const char *into, const char *subj = NULL, int mode = WM_NONE, smb_t* resmb = NULL, smbmsg_t* remsg = NULL, str_list_t cc = NULL);
	bool	qnetmail(const char *into, const char *subj = NULL, int mode = WM_NONE, smb_t* resmb = NULL, smbmsg_t* remsg = NULL);

	/* useredit.cpp */
	void	useredit(int usernumber);
	int		searchup(char *search,int usernum);
	int		searchdn(char *search,int usernum);
	void	user_config(user_t* user);
	bool	purgeuser(int usernumber);

	/* ver.cpp */
	void	ver(int p_mode = P_CENTER | P_80COLS, bool verbose = false);

	/* scansubs.cpp */
	void	scansubs(int mode);
	void	scanallsubs(int mode);
	void	new_scan_cfg(uint misc);
	void	new_scan_ptr_cfg(void);

	/* scandirs.cpp */
	void	scanalldirs(int mode);
	void	scandirs(int mode);

	#define nosound()
	#define checkline()

	void	catsyslog(int crash);

	/* telgate.cpp */
	bool	telnet_gate(char* addr, uint mode, unsigned timeout=10 	// See TG_* for mode bits
		,str_list_t send_strings=NULL, char* client_user_name=NULL, char* server_user_name=NULL, char* term_type=NULL);

	/* sftp.cpp */
	bool init_sftp(int channel_id);
	bool sftp_end(void);

};

#include "terminal.h"

#endif /* __cplusplus */

#ifdef DLLEXPORT
#undef DLLEXPORT
#endif
#ifdef _WIN32
	#ifdef __MINGW32__
		#define DLLEXPORT
	#else
		#ifdef SBBS_EXPORTS
			#define DLLEXPORT	__declspec(dllexport)
		#else
			#define DLLEXPORT	__declspec(dllimport)
		#endif
	#endif
#else	/* !_WIN32 */
	#define DLLEXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

	/* main.cpp */
	extern const char* nulstr;
	extern const char* crlf;
	DLLEXPORT int		sbbs_random(int);
	DLLEXPORT void		sbbs_srand(void);
	DLLEXPORT uint 		repeated_error(int line, const char* function);

	/* postmsg.cpp */
	DLLEXPORT int		savemsg(scfg_t*, smb_t*, smbmsg_t*, client_t*, const char* server, char* msgbuf, smbmsg_t* remsg);
	DLLEXPORT int		votemsg(scfg_t*, smb_t*, smbmsg_t*, const char* msgfmt, const char* votefmt);
	DLLEXPORT int		postpoll(scfg_t*, smb_t*, smbmsg_t*);
	DLLEXPORT int		closepoll(scfg_t*, smb_t*, uint32_t msgnum, const char* username);
	DLLEXPORT void		signal_sub_sem(scfg_t*, int subnum);
	DLLEXPORT int		msg_client_hfields(smbmsg_t*, client_t*);
	DLLEXPORT int		notify(scfg_t*, uint usernumber, const char* subject, bool strip_ctrl, const char* msg, const char* replyto);
	DLLEXPORT void		normalize_msg_hfield_encoding(const char* charset, char* str, size_t size);

	/* data.cpp */
	DLLEXPORT time_t	getnextevent(scfg_t* cfg, event_t**);
	DLLEXPORT time_t	getnexteventtime(const event_t*);

	/* sockopt.c */
	DLLEXPORT int		set_socket_options(scfg_t* cfg, SOCKET sock, const char* section
		,char* error, size_t errlen);

	/* qwk.cpp */
	DLLEXPORT int		qwk_route(scfg_t*, const char *inaddr, char *fulladdr, size_t maxlen);

	/* netmail.cpp */
	DLLEXPORT bool		netmail_addr_is_supported(scfg_t*, const char* addr);

	/* con_out.cpp */
	unsigned char		cp437_to_petscii(unsigned char);

	/* xtrn.cpp */
	bool				native_executable(scfg_t*, const char* cmdline, int mode);

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
		int             tinyid;
		uint8_t         flags;
		int				ver;		/* version added/modified */
		const char*		desc;		/* description */
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
		protected_uint32_t*	clients;
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
	DLLEXPORT JSBool	js_DescribeSyncObject(JSContext* cx, JSObject* obj, const char*, int ver);
	DLLEXPORT JSBool	js_DescribeSyncConstructor(JSContext* cx, JSObject* obj, const char*);
	DLLEXPORT JSBool	js_DefineSyncMethods(JSContext* cx, JSObject* obj, jsSyncMethodSpec*);
	DLLEXPORT JSBool	js_DefineSyncProperties(JSContext* cx, JSObject* obj, jsSyncPropertySpec*);
	DLLEXPORT JSBool	js_SyncResolve(JSContext* cx, JSObject* obj, char *name, jsSyncPropertySpec* props, jsSyncMethodSpec* funcs, jsConstIntSpec* consts, int flags);
	DLLEXPORT JSBool	js_DefineConstIntegers(JSContext* cx, JSObject* obj, jsConstIntSpec*, int flags);
	DLLEXPORT void*		js_GetClassPrivate(JSContext*, JSObject*, JSClass*);

	DLLEXPORT bool	js_CreateCommonObjects(JSContext* cx
													,scfg_t* cfg				/* common */
													,scfg_t* node_cfg			/* node-specific */
													,jsSyncMethodSpec* methods	/* global */
													,time_t uptime				/* system */
													,const char* host_name		/* system */
													,const char* socklib_desc	/* system */
													,js_callback_t*				/* js */
													,js_startup_t*				/* js */
													,user_t* user				/* user */
													,client_t* client			/* client */
													,SOCKET client_socket		/* client */
#ifdef USE_CRYPTLIB
													,CRYPT_CONTEXT session		/* client */
#else
													,int unused
#endif
													,js_server_props_t* props	/* server */
													,JSObject** glob
													,struct mqtt*
													);

	/* js_server.c */
	DLLEXPORT JSObject* js_CreateServerObject(JSContext* cx, JSObject* parent
										,js_server_props_t* props);

	/* js_global.c */
	struct js_onexit_scope {
		JSObject *scope;
		str_list_t onexit;
		struct js_onexit_scope *next;
	};

	typedef struct {
		scfg_t*				cfg;
		jsSyncMethodSpec*	methods;
		js_startup_t*		startup;
		unsigned			bg_count;
		sem_t				bg_sem;
		str_list_t			exit_func;
		struct js_onexit_scope	*onexit;
	} global_private_t;
	DLLEXPORT bool js_argcIsInsufficient(JSContext *cx, unsigned argc, unsigned min);
	DLLEXPORT bool js_argvIsNullOrVoid(JSContext *cx, jsval* argv, unsigned index);
	DLLEXPORT bool js_CreateGlobalObject(JSContext* cx, scfg_t* cfg, jsSyncMethodSpec* methods, js_startup_t*, JSObject**);

	/* js_internal.c */
	DLLEXPORT JSObject* js_CreateInternalJsObject(JSContext*, JSObject* parent, js_callback_t*, js_startup_t*);
	DLLEXPORT JSBool	js_CommonOperationCallback(JSContext*, js_callback_t*);
	DLLEXPORT void		js_EvalOnExit(JSContext*, JSObject*, js_callback_t*);
	DLLEXPORT void		js_PrepareToExecute(JSContext*, JSObject*, const char *filename, const char* startup_dir, JSObject *);
	DLLEXPORT JSBool	js_IsTerminated(JSContext*, JSObject*);
	DLLEXPORT char*		js_getstring(JSContext *cx, JSString *str);
	DLLEXPORT JSBool	js_handle_events(JSContext *cx, js_callback_t *cb, volatile bool *terminated);
	DLLEXPORT JSBool	js_clear_event(JSContext *cx, jsval *arglist, js_callback_t *cb, enum js_event_type et, int ididx);
	DLLEXPORT JSBool	js_CreateArrayOfStrings(JSContext* cx, JSObject* parent
														,const char* name, const char* str[], unsigned flags);

	/* js_system.c */
	DLLEXPORT JSObject* js_CreateSystemObject(JSContext* cx, JSObject* parent
													,scfg_t* cfg, time_t uptime
													,const char* host_name
													,const char* socklib_desc
													,struct mqtt*);
	DLLEXPORT JSBool	js_CreateTextProperties(JSContext* cx, JSObject* parent);

	/* js_client.c */
#ifdef USE_CRYPTLIB
	DLLEXPORT JSObject* js_CreateClientObject(JSContext* cx, JSObject* parent
													,const char* name, client_t* client, SOCKET sock, CRYPT_CONTEXT session);
#endif
	/* js_user.c */
	DLLEXPORT JSObject*	js_CreateUserClass(JSContext* cx, JSObject* parent);
	DLLEXPORT JSObject* js_CreateUserObject(JSContext* cx, JSObject* parent
													,const char* name, user_t* user, client_t* client, bool global_user, struct mqtt*);
	DLLEXPORT JSBool	js_CreateUserObjects(JSContext* cx, JSObject* parent, scfg_t* cfg
													,user_t* user, client_t* client, const char* web_file_vpath_prefix
													,subscan_t* subscan, struct mqtt*);
	/* js_file_area.c */
	DLLEXPORT JSObject* js_CreateFileAreaObject(JSContext* cx, JSObject* parent, scfg_t* cfg
													,user_t* user, client_t* client, const char* web_file_vpath_prefix);

	/* js_msg_area.c */
	DLLEXPORT JSObject* js_CreateMsgAreaObject(JSContext* cx, JSObject* parent, scfg_t* cfg
													,user_t* user, client_t* client, subscan_t* subscan);
	DLLEXPORT bool		js_CreateMsgAreaProperties(JSContext* cx, scfg_t* cfg
													,JSObject* subobj, int subnum);

	/* js_xtrn_area.c */
	DLLEXPORT JSObject* js_CreateXtrnAreaObject(JSContext* cx, JSObject* parent, scfg_t* cfg
													,user_t* user, client_t* client);

	/* js_msgbase.c */
	DLLEXPORT JSObject* js_CreateMsgBaseClass(JSContext* cx, JSObject* parent);
	DLLEXPORT bool		js_ParseMsgHeaderObject(JSContext* cx, JSObject* hdrobj, smbmsg_t*);
	DLLEXPORT bool		js_GetMsgHeaderObjectPrivates(JSContext* cx, JSObject* hdrobj, smb_t**, smbmsg_t**, post_t**);

	/* js_filebase.c */
	DLLEXPORT JSObject* js_CreateFileBaseClass(JSContext*, JSObject* parent);

	/* js_socket.c */
	DLLEXPORT JSObject* js_CreateSocketClass(JSContext* cx, JSObject* parent);
#ifdef USE_CRYPTLIB
	DLLEXPORT JSObject* js_CreateSocketObject(JSContext* cx, JSObject* parent
													,const char *name, SOCKET sock, CRYPT_CONTEXT session);
#endif
	DLLEXPORT JSObject* js_CreateSocketObjectFromSet(JSContext* cx, JSObject* parent
													,const char *name, struct xpms_set *set);

	DLLEXPORT SOCKET	js_socket(JSContext *cx, jsval val);
	DLLEXPORT int		js_polltimeout(JSContext* cx, jsval val);
#ifdef PREFER_POLL
	DLLEXPORT size_t js_socket_numsocks(JSContext *cx, jsval val);
	DLLEXPORT size_t js_socket_add(JSContext *cx, jsval val, struct pollfd *fds, short events);
#else
	DLLEXPORT void		js_timeval(JSContext* cx, jsval val, struct timeval* tv);
    DLLEXPORT SOCKET	js_socket_add(JSContext *cx, jsval val, fd_set *fds);
	DLLEXPORT bool		js_socket_isset(JSContext *cx, jsval val, fd_set *fds);
#endif

	/* js_queue.c */
	DLLEXPORT JSObject* js_CreateQueueClass(JSContext* cx, JSObject* parent);
	DLLEXPORT JSObject* js_CreateQueueObject(JSContext* cx, JSObject* parent
													,const char *name, msg_queue_t* q);
	bool js_enqueue_value(JSContext *cx, msg_queue_t* q, jsval val, char* name);

	/* js_file.c */
	DLLEXPORT JSObject* js_CreateFileClass(JSContext* cx, JSObject* parent);
	DLLEXPORT JSObject* js_CreateFileObject(JSContext* cx, JSObject* parent, const char *name, int fd, const char* mode);

	/* js_archive.c */
	DLLEXPORT JSObject* js_CreateArchiveClass(JSContext* cx, JSObject* parent, const str_list_t supported_formats);

	/* js_sprintf.c */
	DLLEXPORT char*		js_sprintf(JSContext* cx, uint argn, unsigned argc, jsval *argv);
	DLLEXPORT void		js_sprintf_free(char *);

	/* js_console.cpp */
	JSObject* js_CreateConsoleObject(JSContext* cx, JSObject* parent);
	DLLEXPORT size_t js_cx_input_pending(JSContext *cx);
	DLLEXPORT void js_do_lock_input(JSContext *cx, JSBool lock);

	/* js_bbs.cpp */
	JSObject* js_CreateBbsObject(JSContext* cx, JSObject* parent);

	/* js_uifc.c */
	JSObject* js_CreateUifcObject(JSContext* cx, JSObject* parent);

	/* js_conio.c */
	JSObject* js_CreateConioObject(JSContext* cx, JSObject* parent);

	/* js_com.c */
	DLLEXPORT JSObject* js_CreateCOMClass(JSContext* cx, JSObject* parent);
	DLLEXPORT JSObject* js_CreateCOMObject(JSContext* cx, JSObject* parent, const char *name, COM_HANDLE sock);

	/* js_cryptcon.c */
	DLLEXPORT JSObject* js_CreateCryptContextClass(JSContext* cx, JSObject* parent);

	/* js_cryptkeyset.c */
	DLLEXPORT JSObject* js_CreateCryptKeysetClass(JSContext* cx, JSObject* parent);

	/* js_cryptcert.c */
	DLLEXPORT JSObject* js_CreateCryptCertClass(JSContext* cx, JSObject* parent);

	DLLEXPORT JSObject* js_CreateMQTTClass(JSContext*, JSObject* parent);
#endif

#ifdef SBBS /* These aren't exported */

	/* main.c */
	int 	lputs(int level, const char *);			/* log output */
	int 	lprintf(int level, const char *fmt, ...)	/* log output */
#if defined(__GNUC__)   // Catch printf-format errors
    __attribute__ ((format (printf, 2, 3)))
#endif
	;
	void	call_socket_open_callback(bool open);
	SOCKET	open_socket(int domain, int type, const char* protocol);
	SOCKET	accept_socket(SOCKET s, union xp_sockaddr* addr, socklen_t* addrlen);
	int		close_socket(SOCKET);
	in_addr_t resolve_ip(char *addr);

	char *	readtext(int *line, FILE *stream, int dflt);

	/* ver.cpp */
	char*	socklib_version(char* str, size_t, const char* winsock_ver);

	/* sortdir.cpp */
	int		fnamecmp_a(char **str1, char **str2);	 /* for use with resort() */
	int		fnamecmp_d(char **str1, char **str2);
	int		fdatecmp_a(uchar **buf1, uchar **buf2);
	int		fdatecmp_d(uchar **buf1, uchar **buf2);

	/* file.cpp */
	bool	filematch(const char *filename, const char *filespec);

	/* sbbscon.c */
	#if defined(__unix__) && defined(NEEDS_DAEMON)
	int daemon(int nochdir, int noclose);
	#endif

#endif /* SBBS */

extern char lastuseron[LEN_ALIAS+1];  /* Name of user last online */
extern time_t laston_time;

#ifdef __cplusplus
}
#endif

#endif	/* Don't add anything after this line */
