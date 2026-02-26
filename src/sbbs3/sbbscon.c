/* Synchronet vanilla/console-mode "front-end" */

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

#if defined USE_LINUX_CAPS && !defined _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* ANSI headers */
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#ifdef __QNX__
#include <locale.h>
#endif
#ifdef USE_SYSTEMD
#include <systemd/sd-daemon.h>
#endif

/* Synchronet-specific headers */
#undef SBBS /* this shouldn't be defined unless building sbbs.dll/libsbbs.so */
#include "sbbs.h"       /* load_cfg() */
#include "sbbs_ini.h"   /* sbbs_read_ini() */
#include "ftpsrvr.h"    /* ftp_startup_t, ftp_server */
#include "mailsrvr.h"   /* mail_startup_t, mail_server */
#include "services.h"   /* services_startup_t, services_thread */
#include "ver.h"

/* XPDEV headers */
#include "conwrap.h"    /* kbhit/getch */
#include "threadwrap.h" /* pthread_mutex_t */

#ifdef __unix__

#ifdef USE_LINUX_CAPS
#include <sys/capability.h>
#include <sys/prctl.h>
#endif

#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <stdarg.h>
#include <stdlib.h>  /* Is this included from somewhere else? */
#include <syslog.h>

#endif

/* Global variables */
bool               terminated = FALSE;

enum server_state  server_state[SERVER_COUNT];
bool               server_stopped[SERVER_COUNT];
bool               run_bbs = FALSE;
bbs_startup_t      bbs_startup;
bool               run_ftp = FALSE;
ftp_startup_t      ftp_startup;
bool               run_mail = FALSE;
mail_startup_t     mail_startup;
bool               run_services = FALSE;
services_startup_t services_startup;
bool               run_web = FALSE;
web_startup_t      web_startup;
ulong              thread_count = 1;
ulong              socket_count = 0;
ulong              error_count = 0;
int                prompt_len = 0;
static scfg_t      scfg;                    /* To allow rerun */
static ulong       served = 0;
char               ini_file[MAX_PATH + 1];
link_list_t        login_attempt_list;
link_list_t        client_list;

#ifdef __unix__
char               new_uid_name[32];
char               new_gid_name[32];
uid_t              new_uid;
uid_t              old_uid;
gid_t              new_gid;
gid_t              old_gid;
bool               is_daemon = FALSE;
char               log_facility[2];
char               log_ident[128];
bool               std_facilities = FALSE;
FILE *             pidf;
char               pid_fname[MAX_PATH + 1];
bool               capabilities_set = FALSE;
bool               syslog_always = FALSE;

#ifdef USE_LINUX_CAPS
/*
 * If the value of PR_SET_KEEPCAPS is not in <sys/prctl.h>, define it
 * here.  This allows setuid() to work on systems running a new enough
 * kernel but with /usr/include/linux pointing to "standard" kernel
 * headers.
 */
#ifndef PR_SET_KEEPCAPS
#define PR_SET_KEEPCAPS 8
#endif

#ifndef SYS_capset
#ifndef __NR_capset
#include <asm/unistd.h> /* Slackware 4.0 needs this. */
#endif
#define SYS_capset __NR_capset
#endif
#endif /* USE_LINUX_CAPS */

#endif

static const char* prompt;

static const char* usage  = "\nusage: %s [[cmd | setting] [...]] [ctrl_dir | path/sbbs.ini]\n"
                            "\n"
                            "Commands:\n"
                            "\n"
                            "\tversion    show version/revision details and exit\n"
                            "\n"
                            "Global settings:\n"
                            "\n"
                            "\thn[host]   set hostname for this instance\n"
                            "\t           if host not specified, uses gethostname\n"
#ifdef __unix__
                            "\tun<user>   set username for BBS to run as\n"
                            "\tug<group>  set group for BBS to run as\n"
                            "\td[x]       run as daemon, log using syslog\n"
                            "\t           x is the optional LOCALx facility to use\n"
                            "\t           if none is specified, uses USER\n"
                            "\t           if 'S' is specified, uses standard facilities\n"
                            "\tsyslog     log to syslog (even when not daemonized)\n"
#endif
                            "\tgi         get user identity (using IDENT protocol)\n"
                            "\tnh         disable hostname lookups\n"
                            "\tnj         disable JavaScript support\n"
                            "\tne         disable event thread\n"
                            "\tni         do not read settings from .ini file\n"
#ifdef __unix__
                            "\tnd         do not run as daemon - overrides .ini file\n"
#endif
                            "\tdefaults   show default settings and options\n"
                            "\n"
;
static const char* telnet_usage  = "Terminal server settings:\n\n"
                                   "\ttf<node>   set first node number\n"
                                   "\ttl<node>   set last node number\n"
                                   "\ttp<port>   set Telnet server port\n"
                                   "\trp<port>   set RLogin server port (and enable RLogin server)\n"
                                   "\tto<value>  set Terminal server options value (advanced)\n"
                                   "\tta         enable auto-logon via IP address\n"
                                   "\ttd         enable Telnet command debug output\n"
                                   "\ttq         disable QWK events\n"
                                   "\tt<+|->     enable or disable Terminal server\n"
                                   "\tt!         run Terminal server only\n"
;
static const char* ftp_usage  = "FTP server settings:\n"
                                "\n"
                                "\tfp<port>   set FTP server port\n"
                                "\tfo<value>  set FTP server options value (advanced)\n"
                                "\tf<+|->     enable or disable FTP server\n"
                                "\tf!         run FTP server only\n"
;
static const char* mail_usage  = "Mail server settings:\n"
                                 "\n"
                                 "\tms<port>   set SMTP server port\n"
                                 "\tmp<port>   set POP3 server port\n"
                                 "\tmr<addr>   set SMTP relay server (and enable SMTP relay)\n"
                                 "\tmd<addr>   set DNS server address for MX-record lookups\n"
                                 "\tmo<value>  set Mail server options value (advanced)\n"
                                 "\tma         allow SMTP relays from authenticated users\n"
                                 "\tm<+|->     enable or disable Mail server (entirely)\n"
                                 "\tmp-        disable POP3 server\n"
                                 "\tms-        disable SendMail thread\n"
                                 "\tm!         run Mail server only\n"
;
static const char* services_usage  = "Services settings:\n"
                                     "\n"
                                     "\tso<value>  set Services option value (advanced)\n"
                                     "\ts<+|->     enable or disable Services server\n"
                                     "\ts!         run Services server only\n"
;
static const char* web_usage  = "Web server settings:\n"
                                "\n"
                                "\twp<port>   set HTTP server port\n"
                                "\two<value>  set Web server option value (advanced)\n"
                                "\tw<+|->     enable or disable Web server\n"
                                "\tw!         run Web server only\n"
;

static bool server_running(enum server_type type)
{
	return server_state[type] != SERVER_STOPPED;
}

static bool server_ready(enum server_type type)
{
	return server_state[type] == SERVER_READY;
}

static bool any_server_running()
{
	for (int i = 0; i < SERVER_COUNT; i++)
		if (server_running(i))
			return true;
	return false;
}

#ifdef USE_SYSTEMD
static bool any_server_with_state(enum server_state state)
{
	for (int i = 0; i < SERVER_COUNT; i++)
		if (server_state[i] == state)
			return true;
	return false;
}

static void notify_systemd(const char* new_status)
{
	static char status[1024];
	if (new_status != NULL)
		SAFECOPY(status, new_status);
	char*       ready = "";
	if (any_server_with_state(SERVER_RELOADING))
		ready = "RELOADING=1";
	else if (any_server_with_state(SERVER_STOPPING))
		ready = "STOPPING=1";
	else if (any_server_with_state(SERVER_READY))
		ready = "READY=1";
	sd_notifyf(/* unset_environment: */ 0, "%s\nSTATUS=%s", ready, status);
}
#endif

static int lputs(int level, const char *str)
{
	static pthread_mutex_t mutex;
	static bool            mutex_initialized;
	const char *           p;

#ifdef __unix__
	if (is_daemon)  {
		if (str != NULL) {
			if (std_facilities)
				syslog(level | LOG_AUTH, "%s", str);
			else
				syslog(level, "%s", str);
#ifdef USE_SYSTEMD
			notify_systemd(str);
#endif
		}
		return 0;
	}
#endif
	if (!mutex_initialized) {
		pthread_mutex_init(&mutex, NULL);
		mutex_initialized = TRUE;
	}
	pthread_mutex_lock(&mutex);
	/* erase prompt */
	printf("\r%*s\r", prompt_len, "");
	if (str != NULL) {
		for (p = str; *p; p++) {
			if (iscntrl((unsigned char)*p))
				printf("^%c", '@' + *p);
			else
				printf("%c", *p);
		}
		puts("");
	}
	/* re-display prompt with current stats */
	if (prompt != NULL)
		prompt_len = printf(prompt, thread_count, socket_count, client_list.count, served, error_count);
	fflush(stdout);
	pthread_mutex_unlock(&mutex);

	return prompt_len;
}

static void errormsg(void *cbdata, int level, const char *msg)
{
	error_count++;
}

static int lprintf(int level, const char *fmt, ...)
{
	va_list argptr;
	char    sbuf[1024];

	va_start(argptr, fmt);
	vsnprintf(sbuf, sizeof(sbuf), fmt, argptr);
	sbuf[sizeof(sbuf) - 1] = 0;
	va_end(argptr);
	return lputs(level, sbuf);
}

static void recycle_all()
{
	bbs_startup.recycle_now = TRUE;
	ftp_startup.recycle_now = TRUE;
	web_startup.recycle_now = TRUE;
	mail_startup.recycle_now = TRUE;
	services_startup.recycle_now = TRUE;
}

#ifdef __unix__
pthread_once_t         setid_mutex_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t setid_mutex;

static void
init_setuid_mutex(void)
{
	pthread_mutex_init(&setid_mutex, NULL);
}

/**********************************************************
* Change uid of the calling process to the user if specified
* **********************************************************/
static bool do_seteuid(bool to_new)
{
	bool result = FALSE;

	if (capabilities_set)
		return TRUE;      /* do nothing */

	if (new_uid_name[0] == 0)  /* not set? */
		return TRUE;      /* do nothing */

	if (old_uid == new_uid && old_gid == new_gid)
		return TRUE;      /* do nothing */

	pthread_once(&setid_mutex_once, init_setuid_mutex);
	pthread_mutex_lock(&setid_mutex);

	if (to_new) {
		if ((new_gid == getegid() || setregid(-1, new_gid) == 0)
		    && (new_uid == geteuid() || setreuid(-1, new_uid) == 0))
			result = TRUE;
		else
			result = FALSE;
	}
	else {
		if ((old_gid == getegid() || setregid(-1, old_gid) == 0)
		    && (old_uid == geteuid() || setreuid(-1, old_uid) == 0))
			result = TRUE;
		else
			result = FALSE;
	}

	pthread_mutex_unlock(&setid_mutex);

	if (!result) {
		lprintf(LOG_ERR, "!seteuid FAILED with error %d (%s)", errno, strerror(errno));
	}
	return result;
}

/**********************************************************
* Change uid of the calling process to the user if specified
* **********************************************************/
bool do_setuid(bool force)
{
	bool result = TRUE;
#if defined(DONT_BLAME_SYNCHRONET)
	if (!force)
		return do_seteuid(TRUE);
#endif

#if defined(_THREAD_SUID_BROKEN)
	if (thread_suid_broken && (!force))
		return do_seteuid(TRUE);
#endif

	if (old_uid == new_uid && old_gid == new_gid)
		return TRUE;      /* do nothing */

	pthread_once(&setid_mutex_once, init_setuid_mutex);
	pthread_mutex_lock(&setid_mutex);

	if (getegid() != old_gid) {
		if (setregid(-1, old_gid) != 0)
			lprintf(LOG_ERR, "!setregid FAILED with error %d (%s)", errno, strerror(errno));
	}
	if (geteuid() != old_gid) {
		if (setreuid(-1, old_uid) != 0)
			lprintf(LOG_ERR, "!setreuid FAILED with error %d (%s)", errno, strerror(errno));
	}
	if (getgid() != new_gid || getegid() != new_gid) {
		if (setregid(new_gid, new_gid)) {
			lprintf(LOG_ERR, "!setgid FAILED with error %d (%s)", errno, strerror(errno));
			result = FALSE;
		}
	}

	if (getuid() != new_uid || geteuid() != new_uid) {
		if (initgroups(new_uid_name, new_gid)) {
			lprintf(LOG_ERR, "!initgroups FAILED with error %d (%s)", errno, strerror(errno));
			result = FALSE;
		}
		if (setreuid(new_uid, new_uid)) {
			lprintf(LOG_ERR, "!setuid FAILED with error %d (%s)", errno, strerror(errno));
			result = FALSE;
		}
	}

	pthread_mutex_unlock(&setid_mutex);

	if (force && (!result))
		exit(1);

	return result;
}

bool change_user(void)
{
	if (!do_setuid(FALSE)) {
		/* actually try to change the uid of this process */
		lputs(LOG_ERR, "!Setting new user-id failed!  (Does the user exist?)");
		return false;
	}
	struct passwd *pwent;

	pwent = getpwnam(new_uid_name);
	if (pwent == NULL)
		lprintf(LOG_WARNING, "No password name for: %s", new_uid_name);
	else {
		static char uenv[128];
		static char henv[MAX_PATH + 6];
		sprintf(uenv, "USER=%s", pwent->pw_name);
		putenv(uenv);
		sprintf(henv, "HOME=%s", pwent->pw_dir);
		putenv(henv);
	}
	if (new_gid_name[0]) {
		static char genv[128];
		sprintf(genv, "GROUP=%s", new_gid_name);
		putenv(genv);
	}
	lprintf(LOG_INFO, "Successfully changed user-id to %s", new_uid_name);
	return true;
}

#ifdef USE_LINUX_CAPS
/**********************************************************
* Set system capabilities on Linux.  Allows non root user
* to make calls to bind
* **********************************************************/
void whoami(void)
{
	uid_t a, b, c;
	getresuid(&a, &b, &c);
	lprintf(LOG_DEBUG, "Current usr ids: ruid - %d, euid - %d, suid - %d", a, b, c);
	getresgid(&a, &b, &c);
	lprintf(LOG_DEBUG, "Current grp ids: rgid - %d, egid - %d, sgid - %d", a, b, c);
}

bool list_caps(void)
{
	cap_t   caps = cap_get_proc();
	if (caps == NULL)
		return false;
	ssize_t y = 0;
	lprintf(LOG_DEBUG, "The process %d was given capabilities %s", (int) getpid(), cap_to_text(caps, &y));
	cap_free(caps);
	return true;
}

static int linux_keepcaps(void)
{
	/*
	 * Ask the kernel to allow us to keep our capabilities after we
	 * setuid().
	 */
	if (prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0) < 0) {
		if (errno != EINVAL) {
			lprintf(LOG_ERR, "linux_keepcaps FAILED with error %d (%s)", errno, strerror(errno));
		}
		return -1;
	}
	return 0;
}

static bool linux_setcaps(unsigned int caps)
{
	struct __user_cap_header_struct caphead;
	struct __user_cap_data_struct   cap;

	memset(&caphead, 0, sizeof(caphead));
	caphead.version = _LINUX_CAPABILITY_VERSION;
	caphead.pid = 0;
	memset(&cap, 0, sizeof(cap));
	cap.effective = caps;
	cap.permitted = caps;
	cap.inheritable = 0;
	int ret = syscall(SYS_capset, &caphead, &cap);
	if (ret == 0)
		return true;
	lprintf(LOG_ERR, "linux_setcaps(0x%x) failed (errno %d: %s)"
	        , caps, errno, strerror(errno));

	return false;
}

static bool linux_initialprivs(void)
{
	unsigned int caps;

	caps = 0;
	caps |= (1 << CAP_NET_BIND_SERVICE);
	caps |= (1 << CAP_SETUID);
	caps |= (1 << CAP_SETGID);
	caps |= (1 << CAP_DAC_READ_SEARCH);
	caps |= (1 << CAP_SYS_RESOURCE);
	printf("Setting initial privileges\n");
	return linux_setcaps(caps);
}

static bool linux_minprivs(void)
{
	unsigned int caps;

	caps = 0;
	caps |= (1 << CAP_NET_BIND_SERVICE);
	caps |= (1 << CAP_SYS_RESOURCE);
	printf("Setting minimum privileges\n");
	return linux_setcaps(caps);
}
/**********************************************************
* End capabilities section
* **********************************************************/
#endif /* USE_LINUX_CAPS */

#endif   /* __unix__ */

#ifdef _WINSOCKAPI_

static WSADATA WSAData;

static bool winsock_startup(void)
{
	int status;                 /* Status Code */

	if ((status = WSAStartup(MAKEWORD(1, 1), &WSAData)) == 0)
		return TRUE;

	lprintf(LOG_CRIT, "!WinSock startup ERROR %d", status);
	return FALSE;
}

static bool winsock_cleanup(void)
{
	if (WSACleanup() == 0)
		return TRUE;

	lprintf(LOG_ERR, "!WinSock cleanup ERROR %d", SOCKET_ERRNO);
	return FALSE;
}

#else /* No WINSOCK */

#define winsock_startup()   (TRUE)
#define winsock_cleanup()   (TRUE)

#endif

static void set_state(void* cbdata, enum server_state state)
{
	enum server_type server_type = ((struct startup*)cbdata)->type;
	server_state[server_type] = state;
#ifdef USE_SYSTEMD
	notify_systemd(NULL);
#endif

#ifdef _THREAD_SUID_BROKEN
	if (state == SERVER_READY) {
		if (thread_suid_broken) {
			do_seteuid(FALSE);
			do_setuid(FALSE);
		}
	}
#endif
	server_stopped[server_type] = (state == SERVER_STOPPED);
}

static void thread_up(void* p, bool up, bool setuid)
{
	static pthread_mutex_t mutex;
	static bool            mutex_initialized;

#ifdef _THREAD_SUID_BROKEN
	if (thread_suid_broken && up && setuid) {
		do_seteuid(FALSE);
		do_setuid(FALSE);
	}
#endif

	if (!mutex_initialized) {
		pthread_mutex_init(&mutex, NULL);
		mutex_initialized = TRUE;
	}

	pthread_mutex_lock(&mutex);

	if (up)
		thread_count++;
	else if (thread_count > 0)
		thread_count--;
	pthread_mutex_unlock(&mutex);
	lputs(LOG_INFO, NULL); /* update displayed stats */
}

static void socket_open(void* p, bool open)
{
	static pthread_mutex_t mutex;
	static bool            mutex_initialized;

	if (!mutex_initialized) {
		pthread_mutex_init(&mutex, NULL);
		mutex_initialized = TRUE;
	}

	pthread_mutex_lock(&mutex);
	if (open)
		socket_count++;
	else if (socket_count > 0)
		socket_count--;
	pthread_mutex_unlock(&mutex);
	lputs(LOG_INFO, NULL); /* update displayed stats */
}

static void client_on(void* p, bool on, int sock, client_t* client, bool update)
{
	if (on) {
		if (update) {
			list_node_t* node;

			listLock(&client_list);
			if ((node = listFindTaggedNode(&client_list, sock)) != NULL)
				memcpy(node->data, client, sizeof(client_t));
			listUnlock(&client_list);
		} else {
			served++;
			listAddNodeData(&client_list, client, sizeof(client_t), sock, LAST_NODE);
		}
	} else
		listRemoveTaggedNode(&client_list, sock, /* free_data: */ TRUE);

	lputs(LOG_INFO, NULL); /* update displayed stats */
}

/****************************************************************************/
/* BBS local/log print routine												*/
/****************************************************************************/
static int bbs_lputs(void* p, int level, const char *str)
{
	char      logline[512];
	char      tstr[64];
	time_t    t;
	struct tm tm;

	if (level > bbs_startup.log_level)
		return 0;

#ifdef __unix__
	if (is_daemon || syslog_always)  {
		if (str == NULL)
			return 0;
		if (std_facilities)
			syslog(level | LOG_AUTH, "%s", str);
		else
			syslog(level, "term %s", str);
		if (is_daemon)
			return strlen(str);
	}
#endif

	t = time(NULL);
	if (localtime_r(&t, &tm) == NULL)
		tstr[0] = 0;
	else
		sprintf(tstr, "%d/%d %02d:%02d:%02d "
		        , tm.tm_mon + 1, tm.tm_mday
		        , tm.tm_hour, tm.tm_min, tm.tm_sec);

	sprintf(logline, "%sterm %.*s", tstr, (int)sizeof(logline) - 70, str);
	truncsp(logline);
	lputs(level, logline);

	return strlen(logline) + 1;
}


/****************************************************************************/
/* FTP local/log print routine												*/
/****************************************************************************/
static int ftp_lputs(void* p, int level, const char *str)
{
	char      logline[512];
	char      tstr[64];
	time_t    t;
	struct tm tm;

	if (level > ftp_startup.log_level)
		return 0;

#ifdef __unix__
	if (is_daemon || syslog_always)  {
		if (str == NULL)
			return 0;
		if (std_facilities)
#ifdef __solaris__
			syslog(level | LOG_DAEMON, "%s", str);
#else
			syslog(level | LOG_FTP, "%s", str);
#endif
		else
			syslog(level, "ftp  %s", str);
		if (is_daemon)
			return strlen(str);
	}
#endif

	t = time(NULL);
	if (localtime_r(&t, &tm) == NULL)
		tstr[0] = 0;
	else
		sprintf(tstr, "%d/%d %02d:%02d:%02d "
		        , tm.tm_mon + 1, tm.tm_mday
		        , tm.tm_hour, tm.tm_min, tm.tm_sec);

	sprintf(logline, "%sftp  %.*s", tstr, (int)sizeof(logline) - 70, str);
	truncsp(logline);
	lputs(level, logline);

	return strlen(logline) + 1;
}

/****************************************************************************/
/* Mail Server local/log print routine										*/
/****************************************************************************/
static int mail_lputs(void* p, int level, const char *str)
{
	char      logline[512];
	char      tstr[64];
	time_t    t;
	struct tm tm;

	if (level > mail_startup.log_level)
		return 0;

#ifdef __unix__
	if (is_daemon || syslog_always)  {
		if (str == NULL)
			return 0;
		if (std_facilities)
			syslog(level | LOG_MAIL, "%s", str);
		else
			syslog(level, "mail %s", str);
		if (is_daemon)
			return strlen(str);
	}
#endif

	t = time(NULL);
	if (localtime_r(&t, &tm) == NULL)
		tstr[0] = 0;
	else
		sprintf(tstr, "%d/%d %02d:%02d:%02d "
		        , tm.tm_mon + 1, tm.tm_mday
		        , tm.tm_hour, tm.tm_min, tm.tm_sec);

	sprintf(logline, "%smail %.*s", tstr, (int)sizeof(logline) - 70, str);
	truncsp(logline);
	lputs(level, logline);

	return strlen(logline) + 1;
}

/****************************************************************************/
/* Services local/log print routine											*/
/****************************************************************************/
static int services_lputs(void* p, int level, const char *str)
{
	char      logline[512];
	char      tstr[64];
	time_t    t;
	struct tm tm;

	if (level > services_startup.log_level)
		return 0;

#ifdef __unix__
	if (is_daemon || syslog_always)  {
		if (str == NULL)
			return 0;
		if (std_facilities)
			syslog(level | LOG_DAEMON, "%s", str);
		else
			syslog(level, "srvc %s", str);
		if (is_daemon)
			return strlen(str);
	}
#endif

	t = time(NULL);
	if (localtime_r(&t, &tm) == NULL)
		tstr[0] = 0;
	else
		sprintf(tstr, "%d/%d %02d:%02d:%02d "
		        , tm.tm_mon + 1, tm.tm_mday
		        , tm.tm_hour, tm.tm_min, tm.tm_sec);

	sprintf(logline, "%ssrvc %.*s", tstr, (int)sizeof(logline) - 70, str);
	truncsp(logline);
	lputs(level, logline);

	return strlen(logline) + 1;
}

/****************************************************************************/
/* Event thread local/log print routine										*/
/****************************************************************************/
static int event_lputs(void* p, int level, const char *str)
{
	char      logline[512];
	char      tstr[64];
	time_t    t;
	struct tm tm;

	if (level > bbs_startup.log_level)
		return 0;

#ifdef __unix__
	if (is_daemon || syslog_always)  {
		if (str == NULL)
			return 0;
		if (std_facilities)
			syslog(level | LOG_CRON, "%s", str);
		else
			syslog(level, "evnt %s", str);
		if (is_daemon)
			return strlen(str);
	}
#endif

	t = time(NULL);
	if (localtime_r(&t, &tm) == NULL)
		tstr[0] = 0;
	else
		sprintf(tstr, "%d/%d %02d:%02d:%02d "
		        , tm.tm_mon + 1, tm.tm_mday
		        , tm.tm_hour, tm.tm_min, tm.tm_sec);

	sprintf(logline, "%sevnt %.*s", tstr, (int)sizeof(logline) - 70, str);
	truncsp(logline);
	lputs(level, logline);

	return strlen(logline) + 1;
}

/****************************************************************************/
/* web local/log print routine											*/
/****************************************************************************/
static int web_lputs(void* p, int level, const char *str)
{
	char      logline[512];
	char      tstr[64];
	time_t    t;
	struct tm tm;

	if (level > web_startup.log_level)
		return 0;

#ifdef __unix__
	if (is_daemon || syslog_always)  {
		if (str == NULL)
			return 0;
		if (std_facilities)
			syslog(level | LOG_DAEMON, "%s", str);
		else
			syslog(level, "web  %s", str);
		if (is_daemon)
			return strlen(str);
	}
#endif

	t = time(NULL);
	if (localtime_r(&t, &tm) == NULL)
		tstr[0] = 0;
	else
		sprintf(tstr, "%d/%d %02d:%02d:%02d "
		        , tm.tm_mon + 1, tm.tm_mday
		        , tm.tm_hour, tm.tm_min, tm.tm_sec);

	sprintf(logline, "%sweb  %.*s", tstr, (int)sizeof(logline) - 70, str);
	truncsp(logline);
	lputs(level, logline);

	return strlen(logline) + 1;
}

static void terminate(void)
{
	ulong count = 0;

	prompt = "[Threads: %d  Sockets: %d  Clients: %d  Served: %lu  Errors: %lu] Terminating... ";
	if (server_running(SERVER_TERM))
		bbs_terminate();
	if (server_running(SERVER_FTP))
		ftp_terminate();
	if (server_running(SERVER_WEB))
		web_terminate();
	if (server_running(SERVER_MAIL))
		mail_terminate();
	if (server_running(SERVER_SERVICES))
		services_terminate();

	while (any_server_running()) {
		if (count && (count % 10) == 0) {
			if (server_running(SERVER_TERM))
				lputs(LOG_INFO, "Terminal Server thread still running");
			if (server_running(SERVER_FTP))
				lprintf(LOG_INFO, "FTP Server thread still running (inactivity timeout: %u seconds)", ftp_startup.max_inactivity);
			if (server_running(SERVER_WEB))
				lprintf(LOG_INFO, "Web Server thread still running (inactivity timeout: %u seconds)", web_startup.max_inactivity);
			if (server_running(SERVER_MAIL))
				lprintf(LOG_INFO, "Mail Server thread still running (inactivity timeout: %u seconds)", mail_startup.max_inactivity);
			if (server_running(SERVER_SERVICES))
				lputs(LOG_INFO, "Services thread still running");
		}
		count++;
		SLEEP(1000);
	}
}

static void read_startup_ini(bool recycle
                             , bbs_startup_t* bbs, ftp_startup_t* ftp, web_startup_t* web
                             , mail_startup_t* mail, services_startup_t* services)
{
	FILE* fp = NULL;

	/* Read .ini file here */
	if (ini_file[0] != 0) {
		if (isdir(ini_file))
			sbbs_get_ini_fname(ini_file, ini_file);
		if ((fp = fopen(ini_file, "r")) == NULL) {
			lprintf(LOG_ERR, "!ERROR %d (%s) opening %s", errno, strerror(errno), ini_file);
		} else {
			lprintf(LOG_INFO, "Reading %s", ini_file);
		}
	}
	if (fp == NULL)
		lputs(LOG_WARNING, "Using default initialization values");

	/* We call this function to set defaults, even if there's no .ini file */
	if (!sbbs_read_ini(fp,
	              ini_file,
	              NULL, /* global_startup */
	              &run_bbs,       bbs,
	              &run_ftp,       ftp,
	              &run_web,       web,
	              &run_mail,      mail,
	              &run_services,  services))
		lprintf(LOG_CRIT, "Internal error reading or initializing startup structures from %s", ini_file);

	/* read/default any sbbscon-specific .ini keys here */
#if defined(__unix__)
	{
		char  value[INI_MAX_VALUE_LEN];
		char* section = "UNIX";
		SAFECOPY(new_uid_name, iniReadString(fp, section, "User", "", value));
		SAFECOPY(new_gid_name, iniReadString(fp, section, "Group", "", value));
		if (!recycle)
			is_daemon = iniReadBool(fp, section, "Daemonize", FALSE);
		SAFECOPY(log_facility, iniReadString(fp, section, "LogFacility", "U", value));
		SAFECOPY(log_ident, iniReadString(fp, section, "LogIdent", "synchronet", value));
		SAFECOPY(pid_fname, iniReadString(fp, section, "PidFile", "sbbs.pid", value));
		umask(iniReadInteger(fp, section, "umask", 077));
	}
#endif
	/* close .ini file here */
	if (fp != NULL)
		fclose(fp);
}

/* Server recycle callback (read relevant startup .ini file section)		*/
void recycle(void* cbdata)
{
	bbs_startup_t*      bbs = NULL;
	ftp_startup_t*      ftp = NULL;
	web_startup_t*      web = NULL;
	mail_startup_t*     mail = NULL;
	services_startup_t* services = NULL;

	switch (((struct startup*)cbdata)->type) {
		case SERVER_TERM:
			bbs = &bbs_startup;
			break;
		case SERVER_MAIL:
			mail = &mail_startup;
			break;
		case SERVER_FTP:
			ftp = &ftp_startup;
			break;
		case SERVER_WEB:
			web = &web_startup;
			break;
		case SERVER_SERVICES:
			services  = &services_startup;
			break;
		default:
			return;
	}

	read_startup_ini(/* recycle? */ TRUE, bbs, ftp, web, mail, services);
}

void cleanup(void)
{
#ifdef __unix__
	FILE* pf;
	if ((pf = fopen(pid_fname, "r")) != NULL) {
		int fpid = -1;
		if (fscanf(pf, "%d", &fpid) == 1) {
			fclose(pf);
			if (fpid == getpid())
				unlink(pid_fname);
		}
		else
			fclose(pf);
	}
#endif
}

#if defined(_WIN32)
BOOL WINAPI ControlHandler(unsigned long CtrlType)
{
	terminated = TRUE;
	return TRUE;
}
#endif


#ifdef __unix__
void _sighandler_quit(int sig)
{
	static pthread_mutex_t mutex;
	static bool            mutex_initialized;

	if (!mutex_initialized) {
		pthread_mutex_init(&mutex, NULL);
		mutex_initialized = TRUE;
	}
	pthread_mutex_lock(&mutex);
	/* Can I get away with leaving this locked till exit? */

	lprintf(LOG_NOTICE, "     Got quit signal (%d)", sig);
	terminate();

	exit(0);
}

void _sighandler_rerun(int sig)
{

	lputs(LOG_NOTICE, "     Got HUP (rerun) signal");

	recycle_all();
}

static void handle_sigs(void)
{
	int      i;
	int      sig = 0;
	sigset_t sigs;

	SetThreadName("sbbs/sigHandler");
	thread_up(NULL, TRUE, TRUE);

	/* Write the standard .pid file if created/open */
	/* Must be here so signals are sent to the correct thread */

	if (pidf != NULL) {
		fprintf(pidf, "%d", getpid());
		fclose(pidf);
		pidf = NULL;
	}

	/* Set up blocked signals */
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGINT);
	sigaddset(&sigs, SIGQUIT);
	sigaddset(&sigs, SIGABRT);
	sigaddset(&sigs, SIGTERM);
	sigaddset(&sigs, SIGHUP);
	sigaddset(&sigs, SIGALRM);
	/* sigaddset(&sigs,SIGPIPE); */
	pthread_sigmask(SIG_BLOCK, &sigs, NULL);
	while (1)  {
		if ((i = sigwait(&sigs, &sig)) != 0) {   /* wait here until signaled */
			lprintf(LOG_ERR, "     !sigwait FAILURE (%d)", i);
			continue;
		}
		lprintf(LOG_NOTICE, "     Got signal (%d)", sig);
		switch (sig)  {
			/* QUIT-type signals */
			case SIGINT:
			case SIGQUIT:
			case SIGABRT:
			case SIGTERM:
				_sighandler_quit(sig);
				break;
			case SIGHUP:    /* rerun */
				_sighandler_rerun(sig);
				break;
			default:
				lputs(LOG_NOTICE, "     Signal has no handler (unexpected)");
		}
	}
}
#endif  /* __unix__ */

/****************************************************************************/
/* Displays appropriate usage info											*/
/****************************************************************************/
static void show_usage(char *cmd)
{
	printf(usage, cmd);
	puts(telnet_usage);
	puts(ftp_usage);
	puts(mail_usage);
	puts(services_usage);
	puts(web_usage);
}

static const char* sbbscon_ver()
{
	static char str[256];

	if (*str == '\0') {
		char compiler[32];
		DESCRIBE_COMPILER(compiler);
		snprintf(str, sizeof(str), "Synchronet Console %s%c%s  Compiled %s/%s %s with %s"
		         , VERSION, REVISION
#ifdef _DEBUG
		         , " Debug"
#else
		         , ""
#endif
		         , git_branch, git_hash
		         , git_date, compiler);
	}
	return str;
}

/****************************************************************************/
/* Main Entry Point															*/
/****************************************************************************/
int main(int argc, char** argv)
{
	int            i;
	int            n;
	int            nodefile = -1;
	char           ch;
	char*          p;
	char*          arg;
	const char*    ctrl_dir;
	char           str[MAX_PATH + 1];
	char           error[256];
	char           host_name[128] = "";
	node_t         node;
	unsigned       count;
#ifdef __unix__
	struct passwd* pw_entry;
	struct group*  gr_entry;
	sigset_t       sigs;
#endif
#ifdef _THREAD_SUID_BROKEN
	size_t         conflen;
#endif

#ifdef __QNX__
	setlocale( LC_ALL, "C-TRADITIONAL" );
#endif
#ifdef __unix__
	setsid();   /* Disassociate from controlling terminal */
	umask(077);
#elif defined(_WIN32)
	CreateMutex(NULL, FALSE, "sbbs_running");   /* For use by Inno Setup */
#endif
	printf("\nSynchronet Console for %s-%s  Version %s%c  %s\n\n"
	       , PLATFORM_DESC, ARCHITECTURE_DESC, VERSION, REVISION, COPYRIGHT_NOTICE);

	SetThreadName("sbbs");
	listInit(&client_list, LINK_LIST_MUTEX);
	loginAttemptListInit(&login_attempt_list);
	atexit(cleanup);

	ctrl_dir = get_ctrl_dir(/* warn: */ true);

	if (!winsock_startup())
		return -1;

	gethostname(host_name, sizeof(host_name) - 1);

	if (!winsock_cleanup())
		return -1;

	sbbs_get_ini_fname(ini_file, ctrl_dir);
	/* Initialize BBS startup structure */
	memset(&bbs_startup, 0, sizeof(bbs_startup));
	bbs_startup.size = sizeof(bbs_startup);
	bbs_startup.type = SERVER_TERM;
	bbs_startup.cbdata = &bbs_startup;
	bbs_startup.log_level = LOG_DEBUG;
	bbs_startup.lputs = bbs_lputs;
	bbs_startup.event_lputs = event_lputs;
	bbs_startup.errormsg = errormsg;
	bbs_startup.set_state = set_state;
	bbs_startup.recycle = recycle;
	bbs_startup.thread_up = thread_up;
	bbs_startup.socket_open = socket_open;
	bbs_startup.client_on = client_on;
#ifdef __unix__
	bbs_startup.seteuid = do_seteuid;
	bbs_startup.setuid = do_setuid;
#endif
	bbs_startup.login_attempt_list = &login_attempt_list;
	SAFECOPY(bbs_startup.ctrl_dir, ctrl_dir);

	/* Initialize FTP startup structure */
	memset(&ftp_startup, 0, sizeof(ftp_startup));
	ftp_startup.size = sizeof(ftp_startup);
	ftp_startup.type = SERVER_FTP;
	ftp_startup.cbdata = &ftp_startup;
	ftp_startup.log_level = LOG_DEBUG;
	ftp_startup.lputs = ftp_lputs;
	ftp_startup.errormsg = errormsg;
	ftp_startup.set_state = set_state;
	ftp_startup.recycle = recycle;
	ftp_startup.thread_up = thread_up;
	ftp_startup.socket_open = socket_open;
	ftp_startup.client_on = client_on;
#ifdef __unix__
	ftp_startup.seteuid = do_seteuid;
	ftp_startup.setuid = do_setuid;
#endif
	ftp_startup.login_attempt_list = &login_attempt_list;
	SAFECOPY(ftp_startup.index_file_name, "00index");
	SAFECOPY(ftp_startup.ctrl_dir, ctrl_dir);

	/* Initialize Web Server startup structure */
	memset(&web_startup, 0, sizeof(web_startup));
	web_startup.size = sizeof(web_startup);
	web_startup.type = SERVER_WEB;
	web_startup.cbdata = &web_startup;
	web_startup.log_level = LOG_DEBUG;
	web_startup.lputs = web_lputs;
	web_startup.errormsg = errormsg;
	web_startup.set_state = set_state;
	web_startup.recycle = recycle;
	web_startup.thread_up = thread_up;
	web_startup.socket_open = socket_open;
	web_startup.client_on = client_on;
#ifdef __unix__
	web_startup.seteuid = do_seteuid;
	web_startup.setuid = do_setuid;
#endif
	web_startup.login_attempt_list = &login_attempt_list;
	SAFECOPY(web_startup.ctrl_dir, ctrl_dir);

	/* Initialize Mail Server startup structure */
	memset(&mail_startup, 0, sizeof(mail_startup));
	mail_startup.size = sizeof(mail_startup);
	mail_startup.type = SERVER_MAIL;
	mail_startup.cbdata = &mail_startup;
	mail_startup.log_level = LOG_DEBUG;
	mail_startup.lputs = mail_lputs;
	mail_startup.errormsg = errormsg;
	mail_startup.set_state = set_state;
	mail_startup.recycle = recycle;
	mail_startup.thread_up = thread_up;
	mail_startup.socket_open = socket_open;
	mail_startup.client_on = client_on;
#ifdef __unix__
	mail_startup.seteuid = do_seteuid;
	mail_startup.setuid = do_setuid;
#endif
	mail_startup.login_attempt_list = &login_attempt_list;
	SAFECOPY(mail_startup.ctrl_dir, ctrl_dir);

	/* Initialize Services startup structure */
	memset(&services_startup, 0, sizeof(services_startup));
	services_startup.size = sizeof(services_startup);
	services_startup.type = SERVER_SERVICES;
	services_startup.cbdata = &services_startup;
	services_startup.log_level = LOG_DEBUG;
	services_startup.lputs = services_lputs;
	services_startup.errormsg = errormsg;
	services_startup.set_state = set_state;
	services_startup.recycle = recycle;
	services_startup.thread_up = thread_up;
	services_startup.socket_open = socket_open;
	services_startup.client_on = client_on;
#ifdef __unix__
	services_startup.seteuid = do_seteuid;
	services_startup.setuid = do_setuid;
#endif
	services_startup.login_attempt_list = &login_attempt_list;
	SAFECOPY(services_startup.ctrl_dir, ctrl_dir);

	/* Pre-INI command-line switches */
	for (i = 1; i < argc; i++) {
		arg = argv[i];
		while (*arg == '-')
			arg++;
		if (strcspn(arg, "\\/") != strlen(arg)) {
			SAFECOPY(ini_file, arg);
			continue;
		}
		if (stricmp(arg, "version") == 0) {
			printf("%s\n", bbs_ver());
			printf("%s\n", mail_ver());
			printf("%s\n", ftp_ver());
			printf("%s\n", web_ver());
			printf("%s\n", services_ver());
			printf("%s\n", sbbscon_ver());
			return EXIT_SUCCESS;
		}
		if (!stricmp(arg, "ni")) {
			ini_file[0] = 0;
			break;
		}
	}

	read_startup_ini(/* recycle? */ FALSE
	                 , &bbs_startup, &ftp_startup, &web_startup, &mail_startup, &services_startup);

	/* Post-INI command-line switches */
	for (i = 1; i < argc; i++) {
		arg = argv[i];
		while (*arg == '-')
			arg++;
		if (strcspn(arg, "\\/") != strlen(arg)) /* ini_file name */
			continue;
		if (!stricmp(arg, "defaults")) {
			printf("Default settings:\n");
			printf("\n");
			printf("Telnet server port:\t%u\n", bbs_startup.telnet_port);
			printf("Terminal first node:\t%u\n", bbs_startup.first_node);
			printf("Terminal last node:\t%u\n", bbs_startup.last_node);
			printf("Terminal server options:\t0x%08" PRIX32 "\n", bbs_startup.options);
			printf("FTP server port:\t%u\n", ftp_startup.port);
			printf("FTP server options:\t0x%08" PRIX32 "\n", ftp_startup.options);
			printf("Mail SMTP server port:\t%u\n", mail_startup.smtp_port);
			printf("Mail SMTP relay port:\t%u\n", mail_startup.relay_port);
			printf("Mail POP3 server port:\t%u\n", mail_startup.pop3_port);
			printf("Mail server options:\t0x%08" PRIX32 "\n", mail_startup.options);
			printf("Services options:\t0x%08" PRIX32 "\n", services_startup.options);
			printf("Web server port:\t%u\n", web_startup.port);
			printf("Web server options:\t0x%08" PRIX32 "\n", web_startup.options);
			return 0;
		}
#ifdef __unix__
		if (!stricmp(arg, "syslog")) {
			syslog_always = TRUE;
			continue;
		}
#endif
		switch (toupper(*(arg++))) {
#ifdef __unix__
			case 'D':     /* Run as daemon */
				is_daemon = TRUE;
				if (*arg)
					SAFECOPY(log_facility, arg++);
				break;
#endif
			case 'T':   /* Terminal server settings */
				switch (toupper(*(arg++))) {
					case '!':
						run_ftp = FALSE;
						run_mail = FALSE;
						run_web = FALSE;
						run_services = FALSE;
					// Fall-through
					case '+':
						run_bbs = TRUE;
						break;
					case '-':
						run_bbs = FALSE;
						break;
					case 'D': /* debug output */
						bbs_startup.options |= BBS_OPT_DEBUG_TELNET;
						break;
					case 'A': /* Auto-logon via IP */
						bbs_startup.options |= BBS_OPT_AUTO_LOGON;
						break;
					case 'Q': /* No QWK events */
						bbs_startup.options |= BBS_OPT_NO_QWK_EVENTS;
						break;
					case 'O': /* Set options */
						bbs_startup.options = strtoul(arg, NULL, 0);
						break;
					case 'P':
						bbs_startup.telnet_port = atoi(arg);
						break;
					case 'F':
						bbs_startup.first_node = atoi(arg);
						break;
					case 'L':
						bbs_startup.last_node = atoi(arg);
						break;
					default:
						show_usage(argv[0]);
						return 1;
				}
				break;
			case 'R':   /* RLogin */
				bbs_startup.options |= BBS_OPT_ALLOW_RLOGIN;
				switch (toupper(*(arg++))) {
					case 'P':
						bbs_startup.rlogin_port = atoi(arg);
						break;
					default:
						show_usage(argv[0]);
						return 1;
				}
				break;
			case 'F':   /* FTP */
				switch (toupper(*(arg++))) {
					case '!':
						run_bbs = FALSE;
						run_mail = FALSE;
						run_services = FALSE;
						run_web = FALSE;
					// fall-through
					case '+':
						run_ftp = TRUE;
						break;
					case '-':
						run_ftp = FALSE;
						break;
					case 'P':
						ftp_startup.port = atoi(arg);
						break;
					case 'O': /* Set options */
						ftp_startup.options = strtoul(arg, NULL, 0);
						break;
					default:
						show_usage(argv[0]);
						return 1;
				}
				break;
			case 'M':   /* Mail */
				switch (toupper(*(arg++))) {
					case '!':
						run_bbs = FALSE;
						run_ftp = FALSE;
						run_services = FALSE;
						run_web = FALSE;
					// fall-through
					case '+':
						run_mail = TRUE;
						break;
					case '-':
						run_mail = FALSE;
						break;
					case 'O': /* Set options */
						mail_startup.options = strtoul(arg, NULL, 0);
						break;
					case 'S':   /* SMTP/SendMail */
						if (IS_DIGIT(*arg)) {
							mail_startup.smtp_port = atoi(arg);
							break;
						}
						switch (toupper(*(arg++))) {
							case '-':
								mail_startup.options |= MAIL_OPT_NO_SENDMAIL;
								break;
							default:
								show_usage(argv[0]);
								return 1;
						}
						break;
					case 'P':   /* POP3 */
						if (IS_DIGIT(*arg)) {
							mail_startup.pop3_port = atoi(arg);
							break;
						}
						switch (toupper(*(arg++))) {
							case '-':
								mail_startup.options &= ~MAIL_OPT_ALLOW_POP3;
								break;
							default:
								show_usage(argv[0]);
								return 1;
						}
						break;
					case 'R':   /* Relay */
						mail_startup.options |= MAIL_OPT_RELAY_TX;
						p = strchr(arg, ':');  /* port specified */
						if (p != NULL) {
							*p = 0;
							mail_startup.relay_port = atoi(p + 1);
						}
						SAFECOPY(mail_startup.relay_server, arg);
						break;
					case 'D':   /* DNS Server */
						SAFECOPY(mail_startup.dns_server, arg);
						break;
					case 'A':
						mail_startup.options |= MAIL_OPT_ALLOW_RELAY;
						break;
					default:
						show_usage(argv[0]);
						return 1;
				}
				break;
			case 'S':   /* Services */
				switch (toupper(*(arg++))) {
					case '!':
						run_bbs = FALSE;
						run_ftp = FALSE;
						run_mail = FALSE;
						run_web = FALSE;
					// fall-through
					case '+':
						run_services = TRUE;
						break;
					case '-':
						run_services = FALSE;
						break;
					case 'O': /* Set options */
						services_startup.options = strtoul(arg, NULL, 0);
						break;
					default:
						show_usage(argv[0]);
						return 1;
				}
				break;
			case 'W':   /* Web server */
				switch (toupper(*(arg++))) {
					case '!':
						run_bbs = FALSE;
						run_ftp = FALSE;
						run_mail = FALSE;
						run_services = FALSE;
					// fall-through
					case '+':
						run_web = TRUE;
						break;
					case '-':
						run_web = FALSE;
						break;
					case 'P':
						web_startup.port = atoi(arg);
						break;
					case 'O': /* Set options */
						web_startup.options = strtoul(arg, NULL, 0);
						break;
					default:
						show_usage(argv[0]);
						return 1;
				}
				break;
			case 'G':   /* GET */
				switch (toupper(*(arg++))) {
					case 'I': /* Identity */
						bbs_startup.options |= BBS_OPT_GET_IDENT;
						ftp_startup.options |= BBS_OPT_GET_IDENT;
						mail_startup.options |= BBS_OPT_GET_IDENT;
						services_startup.options |= BBS_OPT_GET_IDENT;
						web_startup.options |= BBS_OPT_GET_IDENT;
						break;
					default:
						show_usage(argv[0]);
						return 1;
				}
				break;
			case 'H':   /* Host */
				switch (toupper(*(arg++))) {
					case 'N':   /* Name */
						if (*arg) {
							SAFECOPY(bbs_startup.host_name, arg);
							SAFECOPY(ftp_startup.host_name, arg);
							SAFECOPY(mail_startup.host_name, arg);
							SAFECOPY(services_startup.host_name, arg);
							SAFECOPY(web_startup.host_name, arg);
						} else {
							SAFECOPY(bbs_startup.host_name, host_name);
							SAFECOPY(ftp_startup.host_name, host_name);
							SAFECOPY(mail_startup.host_name, host_name);
							SAFECOPY(services_startup.host_name, host_name);
							SAFECOPY(web_startup.host_name, host_name);
						}
						printf("Setting hostname: %s\n", bbs_startup.host_name);
						break;
					default:
						show_usage(argv[0]);
						return 1;
				}
				break;
			case 'U':   /* runtime UID */
				switch (toupper(*(arg++))) {
					case 'N': /* username */
#ifdef __unix__
						if (strlen(arg) > 1)
						{
							SAFECOPY(new_uid_name, arg);
						}
#endif
						break;
					case 'G': /* groupname */
#ifdef __unix__
						if (strlen(arg) > 1)
						{
							SAFECOPY(new_gid_name, arg);
						}
#endif
						break;
					default:
						show_usage(argv[0]);
						return 1;
				}
				break;
			case 'N':   /* No */
				switch (toupper(*(arg++))) {
					case 'F':   /* FTP Server */
						run_ftp = FALSE;
						break;
					case 'M':   /* Mail Server */
						run_mail = FALSE;
						break;
					case 'S':   /* Services */
						run_services = FALSE;
						break;
					case 'T':   /* Terminal Server */
						run_bbs = FALSE;
						break;
					case 'E': /* No Events */
						bbs_startup.options     |= BBS_OPT_NO_EVENTS;
						break;
					case 'Q': /* No QWK events */
						bbs_startup.options     |= BBS_OPT_NO_QWK_EVENTS;
						break;
					case 'H':   /* Hostname lookup */
						bbs_startup.options     |= BBS_OPT_NO_HOST_LOOKUP;
						ftp_startup.options     |= BBS_OPT_NO_HOST_LOOKUP;
						mail_startup.options    |= BBS_OPT_NO_HOST_LOOKUP;
						services_startup.options |= BBS_OPT_NO_HOST_LOOKUP;
						break;
					case 'J':   /* JavaScript */
						bbs_startup.options     |= BBS_OPT_NO_JAVASCRIPT;
						ftp_startup.options     |= BBS_OPT_NO_JAVASCRIPT;
						mail_startup.options    |= BBS_OPT_NO_JAVASCRIPT;
						services_startup.options |= BBS_OPT_NO_JAVASCRIPT;
						break;
					case 'I':   /* no .ini file */
						break;
					case 'D':
#if defined(__unix__)
						is_daemon = FALSE;
#endif
						break;
					case 'W':   /* no web server */
						run_web = FALSE;
						break;
					default:
						show_usage(argv[0]);
						return 1;
				}
				break;

			default:
				show_usage(argv[0]);
				return 1;
		}
	}

	/* Read in configuration files */
	memset(&scfg, 0, sizeof(scfg));
	SAFECOPY(scfg.ctrl_dir, bbs_startup.ctrl_dir);

	if (chdir(scfg.ctrl_dir) != 0)
		lprintf(LOG_ERR, "!ERROR %d (%s) changing directory to: %s", errno, strerror(errno), scfg.ctrl_dir);

	scfg.size = sizeof(scfg);
	SAFECOPY(error, UNKNOWN_LOAD_ERROR);
	lprintf(LOG_INFO, "Loading configuration files from %s", scfg.ctrl_dir);
	if (!load_cfg(&scfg, /* text: */ NULL, 0, /* prep: */ TRUE, /* node: */ FALSE, error, sizeof(error))) {
		lprintf(LOG_CRIT, "!ERROR loading configuration files: %s", error);
		return -1;
	}
	if (error[0] != '\0')
		lprintf(LOG_WARNING, "!WARNING loading configuration files: %s", error);

/* Daemonize / Set uid/gid */
#ifdef __unix__

	switch (toupper(log_facility[0])) {
		case '0':
			openlog(log_ident, LOG_CONS, LOG_LOCAL0);
			break;
		case '1':
			openlog(log_ident, LOG_CONS, LOG_LOCAL1);
			break;
		case '2':
			openlog(log_ident, LOG_CONS, LOG_LOCAL2);
			break;
		case '3':
			openlog(log_ident, LOG_CONS, LOG_LOCAL3);
			break;
		case '4':
			openlog(log_ident, LOG_CONS, LOG_LOCAL4);
			break;
		case '5':
			openlog(log_ident, LOG_CONS, LOG_LOCAL5);
			break;
		case '6':
			openlog(log_ident, LOG_CONS, LOG_LOCAL6);
			break;
		case '7':
			openlog(log_ident, LOG_CONS, LOG_LOCAL7);
			break;
		case 'F':   /* this is legacy */
		case 'S':
			/* Use standard facilities */
			openlog(log_ident, LOG_CONS, LOG_USER);
			std_facilities = TRUE;
			break;
		default:
			openlog(log_ident, LOG_CONS, LOG_USER);
	}

	if (is_daemon) {
		lprintf(LOG_INFO, "Running as daemon");
		if (daemon(TRUE, FALSE))  { /* Daemonize, DON'T switch to / and DO close descriptors */
			lprintf(LOG_ERR, "!ERROR %d (%s) running as daemon", errno, strerror(errno));
			is_daemon = FALSE;
		}
	}
	/* Open here to use startup permissions to create the file */
	pidf = fopen(pid_fname, "w");
	if (pidf == NULL)
		lprintf(LOG_ERR, "!ERROR %d (%s) creating/opening %s", errno, strerror(errno), pid_fname);

	old_uid = getuid();
	if ((pw_entry = getpwnam(new_uid_name)) != 0)
	{
		new_uid = pw_entry->pw_uid;
		new_gid = pw_entry->pw_gid;
	}
	else  {
		new_uid = getuid();
		new_gid = getgid();
	}
	old_gid = getgid();
	if ((gr_entry = getgrnam(new_gid_name)) != 0)
		new_gid = gr_entry->gr_gid;

	do_seteuid(TRUE);

	/* Set up blocked signals */
	sigemptyset(&sigs);
	sigaddset(&sigs, SIGINT);
	sigaddset(&sigs, SIGQUIT);
	sigaddset(&sigs, SIGABRT);
	sigaddset(&sigs, SIGTERM);
	sigaddset(&sigs, SIGHUP);
	sigaddset(&sigs, SIGALRM);
	/* sigaddset(&sigs,SIGPIPE); */
	pthread_sigmask(SIG_BLOCK, &sigs, NULL);
	signal(SIGPIPE, SIG_IGN);       /* Ignore "Broken Pipe" signal (Also used for broken socket etc.) */
	signal(SIGALRM, SIG_IGN);       /* Ignore "Alarm" signal */

#endif // __unix__

#ifdef _THREAD_SUID_BROKEN
	/* check if we're using NPTL */
/* Old (2.2) systems don't have this. */
#ifdef _CS_GNU_LIBPTHREAD_VERSION
	conflen = confstr (_CS_GNU_LIBPTHREAD_VERSION, NULL, 0);
	if (conflen > 0) {
		char *buf = alloca (conflen);
		confstr (_CS_GNU_LIBPTHREAD_VERSION, buf, conflen);
		if (strstr (buf, "NPTL"))
			thread_suid_broken = FALSE;
	}
#endif
#endif

	/* Install Ctrl-C/Break signal handler here */
#if defined(_WIN32)
	SetConsoleCtrlHandler(ControlHandler, TRUE /* Add */);
#elif defined(__unix__)

#ifdef USE_LINUX_CAPS /* set capabilities and change user before we start threads */
	if (getuid() == 0) {
		whoami();
		if (list_caps() && linux_initialprivs()) {
			if (linux_keepcaps() < 0) {
				lprintf(LOG_ERR, "linux_keepcaps() FAILED with error %d (%s)", errno, strerror(errno));
			}
			else {
				if (!change_user()) {
					lputs(LOG_ERR, "change_user() FAILED");
				}
				else {
					if (!linux_minprivs()) {
						lprintf(LOG_ERR, "linux_minprivs() FAILED with error %d (%s)", errno, strerror(errno));
					}
					else {
						capabilities_set = TRUE;
					}
				}
			}
		}
		whoami();
		list_caps();
	}
#endif /* USE_LINUX_CAPS */

	_beginthread((void (*)(void*)) handle_sigs, 0, NULL);
	if (!capabilities_set) { /* capabilities were NOT set, fallback to original handling of thread options */
		if (new_uid_name[0] != 0) {        /*  check the user arg, if we have uid 0 */
			/* Can't recycle servers (re-bind ports) as non-root user */
			/* If DONT_BLAME_SYNCHRONET is set, keeps root credentials laying around */
#if !defined(DONT_BLAME_SYNCHRONET)
			if (!thread_suid_broken) {
				if (((bbs_startup.options & BBS_OPT_NO_TELNET) == 0
				     && bbs_startup.telnet_port < IPPORT_RESERVED)
				    || ((bbs_startup.options & BBS_OPT_ALLOW_RLOGIN)
				        && bbs_startup.rlogin_port < IPPORT_RESERVED)
#ifdef USE_CRYPTLIB
				    || ((bbs_startup.options & BBS_OPT_ALLOW_SSH)
				        && bbs_startup.ssh_port < IPPORT_RESERVED)
#endif
				    ) {
					lputs(LOG_WARNING, "Disabling Terminal Server recycle support");
					bbs_startup.options |= BBS_OPT_NO_RECYCLE;
				}
				if (ftp_startup.port < IPPORT_RESERVED) {
					lputs(LOG_WARNING, "Disabling FTP Server recycle support");
					ftp_startup.options |= FTP_OPT_NO_RECYCLE;
				}
				if (web_startup.port < IPPORT_RESERVED) {
					lputs(LOG_WARNING, "Disabling Web Server recycle support");
					web_startup.options |= BBS_OPT_NO_RECYCLE;
				}
				if (((mail_startup.options & MAIL_OPT_ALLOW_POP3)
				     && mail_startup.pop3_port < IPPORT_RESERVED)
				    || mail_startup.smtp_port < IPPORT_RESERVED) {
					lputs(LOG_WARNING, "Disabling Mail Server recycle support");
					mail_startup.options |= MAIL_OPT_NO_RECYCLE;
				}
				/* Perhaps a BBS_OPT_NO_RECYCLE_LOW option? */
				lputs(LOG_WARNING, "Disabling Services recycle support");
				services_startup.options |= BBS_OPT_NO_RECYCLE;
			}
#endif /* !defined(DONT_BLAME_SYNCHRONET) */
		}
	} /* end if(!capabilities_set) */
#endif /* defined(__unix__) */

	if (run_bbs)
		_beginthread((void (*)(void*)) bbs_thread, 0, &bbs_startup);
	if (run_ftp)
		_beginthread((void (*)(void*)) ftp_server, 0, &ftp_startup);
	if (run_mail)
		_beginthread((void (*)(void*)) mail_server, 0, &mail_startup);
	if (run_services)
		_beginthread((void (*)(void*)) services_thread, 0, &services_startup);
	if (run_web)
		_beginthread((void (*)(void*)) web_server, 0, &web_startup);

#ifdef __unix__
	uid_t uid = getuid();
	if (uid != 0 && !capabilities_set)  { /*  are we running as a normal user?  */
		lprintf(LOG_WARNING
		        , "!Started as non-root user (id %d): May fail to bind TCP/UDP ports below %u", uid, IPPORT_RESERVED);
	}
	else if (new_uid_name[0] == 0)   /*  check the user arg, if we have uid 0 */
		lputs(LOG_WARNING, "WARNING: No user account specified, running as root!");

	else
	{
		lputs(LOG_INFO, "Waiting for child threads to bind ports...");
		while ((run_bbs && !(server_ready(SERVER_TERM) || server_stopped[SERVER_TERM]))
		       || (run_ftp && !(server_ready(SERVER_FTP) || server_stopped[SERVER_FTP]))
		       || (run_web && !(server_ready(SERVER_WEB) || server_stopped[SERVER_WEB]))
		       || (run_mail && !(server_ready(SERVER_MAIL) || server_stopped[SERVER_MAIL]))
		       || (run_services && !(server_ready(SERVER_SERVICES) || server_stopped[SERVER_SERVICES])))  {
			mswait(1000);
			if (run_bbs && !(server_ready(SERVER_TERM) || server_stopped[SERVER_TERM]))
				lputs(LOG_INFO, "Waiting for BBS thread");
			if (run_web && !(server_ready(SERVER_WEB) || server_stopped[SERVER_WEB]))
				lputs(LOG_INFO, "Waiting for Web thread");
			if (run_ftp && !(server_ready(SERVER_FTP) || server_stopped[SERVER_FTP]))
				lputs(LOG_INFO, "Waiting for FTP thread");
			if (run_mail && !(server_ready(SERVER_MAIL) || server_stopped[SERVER_MAIL]))
				lputs(LOG_INFO, "Waiting for Mail thread");
			if (run_services && !(server_ready(SERVER_SERVICES) || server_stopped[SERVER_SERVICES]))
				lputs(LOG_INFO, "Waiting for Services thread");
		}

		if (!capabilities_set) { /* if using capabilities user should already have changed */
			if (!change_user())
				lputs(LOG_ERR, "change_user FAILED");
		}
	}

	if (!isatty(fileno(stdin))) {            /* redirected */
		while (1) {
			select(0, NULL, NULL, NULL, NULL);  /* Sleep forever - Should this just exit the thread? */
			lputs(LOG_WARNING, "select(NULL) returned!");
		}
	}
	else                                /* interactive */
#endif
	{
		prompt = "[Threads: %d  Sockets: %d  Clients: %d  Served: %lu  Errors: %lu] (?=Help): ";
		lputs(LOG_INFO, NULL);   /* display prompt */

		while (!terminated) {
#ifdef __unix__
			if (!isatty(STDIN_FILENO))  {        /* Controlling terminal has left us *sniff* */
				int fd;
				openlog(log_ident, LOG_CONS, LOG_USER);
				if ((fd = open(_PATH_DEVNULL, O_RDWR, 0)) != -1) {
					(void)dup2(fd, STDIN_FILENO);
					(void)dup2(fd, STDOUT_FILENO);
					(void)dup2(fd, STDERR_FILENO);
					if (fd > 2)
						(void)close(fd);
				}
				is_daemon = TRUE;
				lputs(LOG_WARNING, "STDIN is not a tty anymore... switching to syslog logging");
				select(0, NULL, NULL, NULL, NULL);  /* Sleep forever - Should this just exit the thread? */
			}
#endif

			if (!kbhit()) {
				YIELD();
				continue;
			}
			ch = getch();
			printf("%c\n", ch);
			switch (ch) {
				case 'q':
#ifdef SBBSCON_PROMPT_ON_QUIT   /* This part of Quicksilver's mod doesn't work right on an active BBS (the prompt is usually quickly erased) */
					/* default to no, prevent accidental quit */
					printf("Confirm quit [y/N]: ");
					fflush(stdout);
					switch (toupper(getch())) {
						case 'Y':
							terminated = TRUE;
							break;
						default:
							break;
					}
#else
					terminated = TRUE;
#endif
					break;
				case 'w':   /* who's online */
					printf("\nNodes in use:\n");
				// fall-through
				case 'n':   /* nodelist */
					printf("\n");
					count = 0;
					for (i = 1; i <= scfg.sys_nodes; i++) {
						getnodedat(&scfg, i, &node, /* lockit: */ FALSE, &nodefile);
						if (ch == 'w' && node.status != NODE_INUSE && node.status != NODE_QUIET)
							continue;
						printnodedat(&scfg, i, &node);
						count++;
					}
					if (ch == 'w')
						printf("%u nodes in use\n", count);
					break;
				case 'l':   /* lock node */
				case 'd':   /* down node */
				case 'i':   /* interrupt node */
#ifdef __unix__
					_echo_on(); /* turn on echoing so user can see what they type */
#endif
					printf("\nNode number: ");
					if ((n = atoi(fgets(str, sizeof(str), stdin))) < 1)
						break;
					fflush(stdin);
					printf("\n");
					if ((i = getnodedat(&scfg, n, &node, /* lockit: */ TRUE, &nodefile)) != 0) {
						printf("!Error %d getting node %d data\n", i, n);
						break;
					}
					switch (ch) {
						case 'l':
							node.misc ^= NODE_LOCK;
							break;
						case 'd':
							node.misc ^= NODE_DOWN;
							break;
						case 'i':
							node.misc ^= NODE_INTR;
							break;
					}
					putnodedat(&scfg, n, &node, /* closeit: */ FALSE, nodefile);
					printnodedat(&scfg, n, &node);
#ifdef __unix__
					_echo_off(); /* turn off echoing - failsafe */
#endif
					break;
				case 'r':   /* recycle */
				case 's':   /* shutdown */
				case 't':   /* terminate */
					printf("BBS, FTP, Web, Mail, Services, All, or [Cancel] ? ");
					fflush(stdout);
					switch (toupper(getch())) {
						case 'B':
							printf("BBS\n");
							if (ch == 't')
								bbs_terminate();
							else if (ch == 's')
								bbs_startup.shutdown_now = TRUE;
							else
								bbs_startup.recycle_now = TRUE;
							break;
						case 'F':
							printf("FTP\n");
							if (ch == 't')
								ftp_terminate();
							else if (ch == 's')
								ftp_startup.shutdown_now = TRUE;
							else
								ftp_startup.recycle_now = TRUE;
							break;
						case 'W':
							printf("Web\n");
							if (ch == 't')
								web_terminate();
							else if (ch == 's')
								web_startup.shutdown_now = TRUE;
							else
								web_startup.recycle_now = TRUE;
							break;
						case 'M':
							printf("Mail\n");
							if (ch == 't')
								mail_terminate();
							else if (ch == 's')
								mail_startup.shutdown_now = TRUE;
							else
								mail_startup.recycle_now = TRUE;
							break;
						case 'S':
							printf("Services\n");
							if (ch == 't')
								services_terminate();
							else if (ch == 's')
								services_startup.shutdown_now = TRUE;
							else
								services_startup.recycle_now = TRUE;
							break;
						case 'A':
							printf("All\n");
							if (ch == 't')
								terminate();
							else if (ch == 's') {
								bbs_startup.shutdown_now = TRUE;
								ftp_startup.shutdown_now = TRUE;
								web_startup.shutdown_now = TRUE;
								mail_startup.shutdown_now = TRUE;
								services_startup.shutdown_now = TRUE;
							}
							else {
								recycle_all();
							}
							break;
						case 'C':
						default:
							break;
					}
					break;
				case '!':   /* execute */
#ifdef __unix__
					_echo_on(); /* turn on echoing so user can see what they type */
#endif
					printf("Command line: ");
					if (fgets(str, sizeof(str), stdin) != NULL)
						printf("Result: %d\n", system(str));
#ifdef __unix__
					_echo_off(); /* turn off echoing - failsafe */
#endif
					break;
				case 'a':   /* Show failed login attempts: */
					printf("\nFailed login attempts:\n\n");
					{
						unsigned long    total = 0;
						struct tm        tm;
						list_node_t*     node;
						login_attempt_t* login_attempt;
						char             ip_addr[INET6_ADDRSTRLEN];

						listLock(&login_attempt_list);
						count = 0;
						for (node = login_attempt_list.first; node != NULL; node = node->next) {
							login_attempt = node->data;
							localtime32(&login_attempt->time, &tm);
							if (inet_addrtop(&login_attempt->addr, ip_addr, sizeof(ip_addr)) == NULL)
								SAFECOPY(ip_addr, "<invalid address>");
							printf("%lu attempts (%lu duplicate) from %s, last via %s on %u/%u %02u:%02u:%02u (user: %s, password: %s)\n"
							       , login_attempt->count
							       , login_attempt->dupes
							       , ip_addr
							       , login_attempt->prot
							       , tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec
							       , login_attempt->user
							       , login_attempt->pass
							       );
							count++;
							total += (login_attempt->count - login_attempt->dupes);
						}
						listUnlock(&login_attempt_list);
						if (count)
							printf("==\n");
						printf("%u failed login attempters (potential password hackers)\n", count);
						printf("%lu total unique failed login attempts (potential password hack attempts)\n", total);
					}
					break;
				case 'A':
					printf("\n%lu login attempts cleared\n", loginAttemptListClear(&login_attempt_list));
					break;
				case 'c':   /* Show connected clients: */
					printf("\nConnected clients:\n\n");
					{
						struct tm    tm;
						list_node_t* node;
						client_t*    client;

						listLock(&client_list);
						count = 0;
						for (node = client_list.first; node != NULL; node = node->next) {
							client = node->data;
							localtime32(&client->time, &tm);
							printf("%04d %s %s [%s] %s port %u since %u/%u %02u:%02u:%02u\n"
							       , node->tag
							       , client->protocol
							       , client->user
							       , client->addr
							       , client->host
							       , client->port
							       , tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec
							       );
							count++;
						}
						listUnlock(&client_list);
					}
					break;
				case '?': /* only print help if user requests it */
					printf("\nSynchronet Console Version %s%c Help\n\n", VERSION, REVISION);
					printf("q   = quit\n");
					printf("n   = node list\n");
					printf("w   = who's online (node's in-use)\n");
					printf("l   = lock node (toggle)\n");
					printf("d   = down node (toggle)\n");
					printf("i   = interrupt node (toggle)\n");
					printf("a   = show failed login attempts\n");
					printf("c   = show connected clients\n");
					printf("r   = recycle servers (when not in use)\n");
					printf("s   = shutdown servers (when not in use)\n");
					printf("t   = terminate servers (immediately)\n");
					printf("!   = execute external command\n");
					printf("?   = print this help information\n");
#if 0   /* to do */
					printf("c#  = chat with node #\n");
					printf("s#  = spy on node #\n");
#endif
					break;
				default:
					break;
			}
			lputs(LOG_INFO, ""); /* redisplay prompt */
		}
	}

	terminate();

	/* erase the prompt */
	printf("\r%*s\r", prompt_len, "");

	free_cfg(&scfg);
	sbbs_free_ini(/* global_startup: */ NULL, &bbs_startup, &ftp_startup, &web_startup, &mail_startup, &services_startup);
	return 0;
}
