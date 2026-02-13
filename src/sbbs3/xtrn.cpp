/* Synchronet external program support routines */

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

#include "sbbs.h"
#include "cmdshell.h"
#include "telnet.h"
#include "unicode.h"
#include "utf8.h"

#include <signal.h>         // kill()

#ifdef __unix__
	#include <sys/wait.h>   // WEXITSTATUS

	#include <sys/ttydefaults.h>    // Linux - it's motherfucked.
#if defined(__FreeBSD__)
	#include <libutil.h>    // forkpty()
#elif defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DARWIN__)
	#include <util.h>
#elif defined(__linux__)
	#include <pty.h>
#elif defined(__QNX__)
#if 0
	#include <unix.h>
#else
	#define NEEDS_FORKPTY
#endif
#endif

	#ifdef NEEDS_FORKPTY
	#include <grp.h>
	#endif

	#include <termios.h>

/*
 * Control Character Defaults
 */
#ifdef _POSIX_VDISABLE
#define XTRN_VDISABLE _POSIX_VDISABLE
#else
#define XTRN_VDISABLE 0xff
#endif
#ifndef CTRL
	#define CTRL(x) (x & 037)
#endif
#ifndef CEOF
	#define CEOF        CTRL('d')
#endif
#ifndef CEOL
	#define CEOL        XTRN_VDISABLE
#endif
#ifndef CERASE
	#define CERASE      0177
#endif
#ifndef CERASE2
	#define CERASE2     CTRL('h')
#endif
#ifndef CINTR
	#define CINTR       CTRL('c')
#endif
#ifndef CSTATUS
	#define CSTATUS     CTRL('t')
#endif
#ifndef CKILL
	#define CKILL       CTRL('u')
#endif
#ifndef CMIN
	#define CMIN        1
#endif
#ifndef CQUIT
	#define CQUIT       034     /* FS, ^\ */
#endif
#ifndef CSUSP
	#define CSUSP       CTRL('z')
#endif
#ifndef CTIME
	#define CTIME       0
#endif
#ifndef CDSUSP
	#define CDSUSP      CTRL('y')
#endif
#ifndef CSTART
	#define CSTART      CTRL('q')
#endif
#ifndef CSTOP
	#define CSTOP       CTRL('s')
#endif
#ifndef CLNEXT
	#define CLNEXT      CTRL('v')
#endif
#ifndef CDISCARD
	#define CDISCARD    CTRL('o')
#endif
#ifndef CWERASE
	#define CWERASE     CTRL('w')
#endif
#ifndef CREPRINT
	#define CREPRINT    CTRL('r')
#endif
#ifndef CEOT
	#define CEOT        CEOF
#endif
/* compat */
#ifndef CBRK
	#define CBRK        CEOL
#endif
#ifndef CRPRNT
	#define CRPRNT      CREPRINT
#endif
#ifndef CFLUSH
	#define CFLUSH      CDISCARD
#endif

#ifndef TTYDEF_IFLAG
	#define TTYDEF_IFLAG    (BRKINT | ICRNL | IMAXBEL | IXON | IXANY)
#endif
#ifndef TTYDEF_OFLAG
	#define TTYDEF_OFLAG    (OPOST | ONLCR)
#endif
#ifndef TTYDEF_LFLAG
	#define TTYDEF_LFLAG    (ECHO | ICANON | ISIG | IEXTEN | ECHOE | ECHOKE | ECHOCTL)
#endif
#ifndef TTYDEF_CFLAG
	#define TTYDEF_CFLAG    (CREAD | CS8 | HUPCL)
#endif

#endif  /* __unix__ */

#define XTRN_IO_BUF_LEN 10000   /* 50% of IO_THREAD_BUF_SIZE */

/*****************************************************************************/
/* Interrupt routine to expand WWIV Ctrl-C# codes into ANSI escape sequences */
/*****************************************************************************/
BYTE* wwiv_expand(BYTE* buf, uint buflen, BYTE* outbuf, size_t& newlen
                  , uint user_misc, bool& ctrl_c)
{
	char ansi_seq[32];
	uint i, j, k;

	for (i = j = 0; i < buflen; i++) {
		if (buf[i] == CTRL_C) {    /* WWIV color escape char */
			ctrl_c = true;
			continue;
		}
		if (!ctrl_c) {
			outbuf[j++] = buf[i];
			continue;
		}
		ctrl_c = false;
		if (user_misc & ANSI) {
			switch (buf[i]) {
				default:
					strcpy(ansi_seq, "\x1b[0m");          /* low grey */
					break;
				case '1':
					strcpy(ansi_seq, "\x1b[0;1;36m");     /* high cyan */
					break;
				case '2':
					strcpy(ansi_seq, "\x1b[0;1;33m");     /* high yellow */
					break;
				case '3':
					strcpy(ansi_seq, "\x1b[0;35m");       /* low magenta */
					break;
				case '4':
					strcpy(ansi_seq, "\x1b[0;1;44m");     /* white on blue */
					break;
				case '5':
					strcpy(ansi_seq, "\x1b[0;32m");       /* low green */
					break;
				case '6':
					strcpy(ansi_seq, "\x1b[0;1;5;31m");   /* high blinking red */
					break;
				case '7':
					strcpy(ansi_seq, "\x1b[0;1;34m");     /* high blue */
					break;
				case '8':
					strcpy(ansi_seq, "\x1b[0;34m");       /* low blue */
					break;
				case '9':
					strcpy(ansi_seq, "\x1b[0;36m");       /* low cyan */
					break;
			}
			for (k = 0; ansi_seq[k]; k++)
				outbuf[j++] = ansi_seq[k];
		}
	}
	newlen = j;
	return outbuf;
}

static void petscii_convert(BYTE* buf, uint len)
{
	for (uint i = 0; i < len; i++) {
		buf[i] = cp437_to_petscii(buf[i]);
	}
}

static BYTE* cp437_to_utf8(BYTE* input, size_t& len, BYTE* outbuf, size_t maxlen)
{
	size_t outlen = 0;
	for (size_t i = 0; i < len; ++i) {
		if (outlen >= maxlen) {
			break;
		}
		BYTE                   ch = input[i];
		enum unicode_codepoint codepoint = UNICODE_UNDEFINED;
		switch (ch) {
			case '\a':
			case '\b':
			case ESC: // '\e' not supported by MSVC
			case '\f':
			case '\n':
			case '\r':
			case '\t':
				// Don't convert these control characters UNICODE
				break;
			default:
				codepoint = cp437_unicode_tbl[ch];
		}
		if (codepoint != UNICODE_UNDEFINED) {
			int ulen = utf8_putc((char*)outbuf + outlen, maxlen - outlen, codepoint);
			if (ulen < 1)
				break;
			outlen += ulen;
		} else {
			*(outbuf + outlen) = ch;
			outlen++;
		}
	}
	len = outlen;
	return outbuf;
}

bool native_executable(scfg_t* cfg, const char* cmdline, int mode)
{
	char* p;
	char  str[MAX_PATH + 1];
	char  name[64];
	char  base[64];
	int   i;

	if (mode & EX_NATIVE)
		return true;

	if (*cmdline == '?' || *cmdline == '*')
		return true;

	SAFECOPY(str, cmdline);              /* Set str to program name only */
	truncstr(str, " ");
	SAFECOPY(name, getfname(str));
	SAFECOPY(base, name);
	if ((p = getfext(base)) != NULL)
		*p = 0;

	for (i = 0; i < cfg->total_natvpgms; i++)
		if (stricmp(name, cfg->natvpgm[i]->name) == 0
		    || stricmp(base, cfg->natvpgm[i]->name) == 0)
			break;
	return i < cfg->total_natvpgms;
}

#define XTRN_LOADABLE_MODULE(cmdline, startup_dir)           \
		if (cmdline[0] == '*') /* Baja module or JavaScript */ \
		return exec_bin(cmdline + 1, &main_csi, startup_dir);
#ifdef JAVASCRIPT
	#define XTRN_LOADABLE_JS_MODULE(cmdline, mode, startup_dir)   \
			if (cmdline[0] == '?' && (mode & EX_SH))                     \
			return js_execxtrn(cmdline + 1, startup_dir);        \
			if (cmdline[0] == '?')                                     \
			return js_execfile(cmdline + 1, startup_dir);
#else
	#define XTRN_LOADABLE_JS_MODULE
#endif

#ifdef _WIN32

#include "vdd_func.h"   /* DOSXTRN.EXE API */

extern SOCKET node_socket[];

/*****************************************************************************/
// Expands Single CR to CRLF
/*****************************************************************************/
BYTE* cr_expand(BYTE* inbuf, size_t inlen, BYTE* outbuf, size_t& newlen)
{
	uint i, j;

	for (i = j = 0; i < inlen; i++) {
		outbuf[j++] = inbuf[i];
		if (inbuf[i] == '\r')
			outbuf[j++] = '\n';
	}
	newlen = j;
	return outbuf;
}

static void add_env_var(str_list_t* list, const char* var, const char* val)
{
	char str[MAX_PATH * 2];
	SetEnvironmentVariable(var, NULL);   /* Delete in current process env */
	SAFEPRINTF2(str, "%s=%s", var, val);
	strListPush(list, str);
}

/* Clean-up resources while preserving current LastError value */
#define XTRN_CLEANUP                                                \
		last_error = GetLastError();                                      \
		if (rdslot != INVALID_HANDLE_VALUE)    CloseHandle(rdslot);        \
		if (wrslot != INVALID_HANDLE_VALUE)    CloseHandle(wrslot);        \
		if (start_event != NULL)               CloseHandle(start_event);   \
		if (hungup_event != NULL)              CloseHandle(hungup_event);  \
		if (hangup_event != NULL)              CloseHandle(hangup_event);  \
		SetLastError(last_error)

/****************************************************************************/
/* Runs an external program (on Windows) 									*/
/****************************************************************************/
int sbbs_t::external(const char* cmdline, int mode, const char* startup_dir)
{
	char                str[MAX_PATH + 1];
	char*               env_block = NULL;
	char*               env_strings;
	const char*         p_startup_dir;
	char                path[MAX_PATH + 1];
	char                fullcmdline[MAX_PATH + 1];
	char                realcmdline[MAX_PATH + 1];
	char                comspec_str[MAX_PATH + 1];
	char                title[MAX_PATH + 1];
	BYTE                buf[XTRN_IO_BUF_LEN], *bp;
	BYTE                telnet_buf[XTRN_IO_BUF_LEN * 2];
	BYTE                output_buf[XTRN_IO_BUF_LEN * 2];
	BYTE                wwiv_buf[XTRN_IO_BUF_LEN * 2];
	bool                wwiv_flag = false;
	bool                native = false; // DOS program by default
	bool                was_online = true;
	bool                rio_abortable_save = rio_abortable;
	bool                use_pipes = false; // NT-compatible console redirection
	BOOL                success;
	BOOL                processTerminated = false;
	bool				input_thread_mutex_locked = false;
	uint                i;
	time_t              hungup = 0;
	HANDLE              rdslot = INVALID_HANDLE_VALUE;
	HANDLE              wrslot = INVALID_HANDLE_VALUE;
	HANDLE              start_event = NULL;
	HANDLE              hungup_event = NULL;
	HANDLE              hangup_event = NULL;
	HANDLE              rdoutpipe;
	HANDLE              wrinpipe;
	PROCESS_INFORMATION process_info;
	size_t              rd;
	DWORD               wr;
	DWORD               len;
	DWORD               avail;
	DWORD               msglen;
	DWORD               retval;
	DWORD               last_error;
	DWORD               loop_since_io = 0;
	struct  tm          tm;
	str_list_t          env_list;

	xtrn_mode = mode;
	lprintf(LOG_DEBUG, "Executing external: %s", cmdline);

	if (startup_dir != NULL && startup_dir[0] && !isdir(startup_dir)) {
		errormsg(WHERE, ERR_CHK, startup_dir, 0);
		return -1;
	}

	term->clear_hotspots();

	XTRN_LOADABLE_MODULE(cmdline, startup_dir);
	XTRN_LOADABLE_JS_MODULE(cmdline, mode, startup_dir);

	attr(cfg.color[clr_external]);      /* setup default attributes */

	native = native_executable(&cfg, cmdline, mode);

	if (!native && (startup->options & BBS_OPT_NO_DOS)) {
		lprintf((mode & EX_OFFLINE) ? LOG_ERR : LOG_WARNING, "DOS programs not supported: %s", cmdline);
		bputs(text[NoDOS]);
		return -1;
	}
	if (mode & EX_OFFLINE)
		native = true;  // We don't need to invoke our virtual UART/FOSSIL driver

	if (mode & EX_SH || strcspn(cmdline, "<>|") != strlen(cmdline))
		sprintf(comspec_str, "%s /C ", comspec);
	else
		comspec_str[0] = 0;

	if (startup_dir && cmdline[1] != ':' && cmdline[0] != '/'
	    && cmdline[0] != '\\' && cmdline[0] != '.')
		SAFEPRINTF3(fullcmdline, "%s%s%s", comspec_str, startup_dir, cmdline);
	else
		SAFEPRINTF2(fullcmdline, "%s%s", comspec_str, cmdline);

	SAFECOPY(realcmdline, fullcmdline); // for errormsg if failed to execute

	now = time(NULL);
	if (localtime_r(&now, &tm) == NULL)
		memset(&tm, 0, sizeof(tm));

	if (native && mode & EX_STDOUT && !(mode & EX_OFFLINE))
		use_pipes = true;

	if (native) { // Native (not MS-DOS) external

		if ((env_list = strListInit()) == NULL) {
			XTRN_CLEANUP;
			errormsg(WHERE, ERR_CREATE, "env_list", 0);
			return errno;
		}

		// Current environment passed to child process
		sprintf(str, "%sprotocol.log", cfg.node_dir);
		add_env_var(&env_list, "DSZLOG", str);
		add_env_var(&env_list, "SBBSNODE", cfg.node_dir);
		add_env_var(&env_list, "SBBSCTRL", cfg.ctrl_dir);
		add_env_var(&env_list, "SBBSDATA", cfg.data_dir);
		add_env_var(&env_list, "SBBSEXEC", cfg.exec_dir);
		sprintf(str, "%d", cfg.node_num);
		add_env_var(&env_list, "SBBSNNUM", str);
		/* date/time env vars */
		sprintf(str, "%02u", tm.tm_mday);
		add_env_var(&env_list, "DAY", str);
		add_env_var(&env_list, "WEEKDAY", wday[tm.tm_wday]);
		add_env_var(&env_list, "MONTHNAME", mon[tm.tm_mon]);
		sprintf(str, "%02u", tm.tm_mon + 1);
		add_env_var(&env_list, "MONTH", str);
		sprintf(str, "%u", 1900 + tm.tm_year);
		add_env_var(&env_list, "YEAR", str);

		env_strings = GetEnvironmentStrings();
		env_block = strListCopyBlock(env_strings);
		if (env_strings != NULL)
			FreeEnvironmentStrings(env_strings);
		env_block = strListAppendBlock(env_block, env_list);
		strListFree(&env_list);
		if (env_block == NULL) {
			XTRN_CLEANUP;
			errormsg(WHERE, ERR_CREATE, "env_block", 0);
			return errno;
		}

	} else { // DOS external

		// DOS-compatible (short) paths
		char node_dir[MAX_PATH + 1];
		char ctrl_dir[MAX_PATH + 1];
		char data_dir[MAX_PATH + 1];
		char exec_dir[MAX_PATH + 1];

		// in case GetShortPathName fails
		SAFECOPY(node_dir, cfg.node_dir);
		SAFECOPY(ctrl_dir, cfg.ctrl_dir);
		SAFECOPY(data_dir, cfg.data_dir);
		SAFECOPY(exec_dir, cfg.exec_dir);

		GetShortPathName(cfg.node_dir, node_dir, sizeof(node_dir));
		GetShortPathName(cfg.ctrl_dir, ctrl_dir, sizeof(node_dir));
		GetShortPathName(cfg.data_dir, data_dir, sizeof(data_dir));
		GetShortPathName(cfg.exec_dir, exec_dir, sizeof(exec_dir));

		SAFEPRINTF(path, "%sDOSXTRN.RET", cfg.node_dir);
		(void)remove(path);
		SAFEPRINTF(path, "%sDOSXTRN.ERR", cfg.node_dir);
		(void)remove(path);

		// Create temporary environment file
		SAFEPRINTF(path, "%sDOSXTRN.ENV", node_dir);
		FILE* fp = fopen(path, "w");
		if (fp == NULL) {
			XTRN_CLEANUP;
			errormsg(WHERE, ERR_CREATE, path, 0);
			return errno;
		}
		fprintf(fp, "%s\n", fullcmdline);
		fprintf(fp, "DSZLOG=%sPROTOCOL.LOG\n", node_dir);
		fprintf(fp, "SBBSNODE=%s\n", node_dir);
		fprintf(fp, "SBBSCTRL=%s\n", ctrl_dir);
		fprintf(fp, "SBBSDATA=%s\n", data_dir);
		fprintf(fp, "SBBSEXEC=%s\n", exec_dir);
		fprintf(fp, "SBBSNNUM=%d\n", cfg.node_num);
		fprintf(fp, "PCBNODE=%d\n", cfg.node_num);
		fprintf(fp, "PCBDRIVE=%.2s\n", node_dir);
		fprintf(fp, "PCBDIR=%s\n", node_dir + 2);
		/* date/time env vars */
		fprintf(fp, "DAY=%02u\n", tm.tm_mday);
		fprintf(fp, "WEEKDAY=%s\n", wday[tm.tm_wday]);
		fprintf(fp, "MONTHNAME=%s\n", mon[tm.tm_mon]);
		fprintf(fp, "MONTH=%02u\n", tm.tm_mon + 1);
		fprintf(fp, "YEAR=%u\n", 1900 + tm.tm_year);
		fclose(fp);

		SAFEPRINTF2(fullcmdline, "%sDOSXTRN.EXE %s", cfg.exec_dir, path);

		if (!(mode & EX_OFFLINE)) {
			i = SBBSEXEC_MODE_UNSPECIFIED;
			if (mode & EX_UART)
				i |= SBBSEXEC_MODE_UART;
			if (mode & EX_FOSSIL)
				i |= SBBSEXEC_MODE_FOSSIL;
			if (mode & EX_STDIN)
				i |= SBBSEXEC_MODE_DOS_IN;
			if (mode & EX_STDOUT)
				i |= SBBSEXEC_MODE_DOS_OUT;
			BOOL x64 = FALSE;
			IsWow64Process(GetCurrentProcess(), &x64);
			sprintf(str, " %s %u %u"
			        , x64 ? "x64" : "NT", cfg.node_num, i);
			strcat(fullcmdline, str);

			sprintf(str, "sbbsexec_hungup%d", cfg.node_num);
			if ((hungup_event = CreateEvent(
					 NULL // pointer to security attributes
					 , TRUE // flag for manual-reset event
					 , FALSE // flag for initial state
					 , str // pointer to event-object name
					 )) == NULL) {
				XTRN_CLEANUP;
				errormsg(WHERE, ERR_CREATE, str, 0);
				return GetLastError();
			}

			sprintf(str, "sbbsexec_hangup%d", cfg.node_num);
			if ((hangup_event = CreateEvent(
					 NULL // pointer to security attributes
					 , TRUE // flag for manual-reset event
					 , FALSE // flag for initial state
					 , str // pointer to event-object name
					 )) == NULL) {
				XTRN_CLEANUP;
				errormsg(WHERE, ERR_CREATE, str, 0);
				return GetLastError();
			}

			sprintf(str, "\\\\.\\mailslot\\sbbsexec\\rd%d"
			        , cfg.node_num);
			rdslot = CreateMailslot(str
			                        , sizeof(buf) / 2 // Maximum message size (0=unlimited)
			                        , 0 // Read time-out
			                        , NULL); // Security
			if (rdslot == INVALID_HANDLE_VALUE) {
				XTRN_CLEANUP;
				errormsg(WHERE, ERR_CREATE, str, 0);
				return GetLastError();
			}
		}
	}

	if (startup_dir != NULL && startup_dir[0])
		p_startup_dir = startup_dir;
	else
		p_startup_dir = NULL;
	STARTUPINFO startup_info = {0};
	startup_info.cb = sizeof(startup_info);
	if (mode & EX_OFFLINE)
		startup_info.lpTitle = NULL;
	else {
		SAFEPRINTF3(title, "%s running %s on node %d"
		            , useron.number ? useron.alias : "Event"
		            , realcmdline
		            , cfg.node_num);
		startup_info.lpTitle = title;
	}
	if (startup->options & BBS_OPT_XTRN_MINIMIZED) {
		startup_info.wShowWindow = SW_SHOWMINNOACTIVE;
		startup_info.dwFlags |= STARTF_USESHOWWINDOW;
	}
	if (use_pipes) {
		// Set up the security attributes struct.
		SECURITY_ATTRIBUTES sa;
		memset(&sa, 0, sizeof(sa));
		sa.nLength = sizeof(SECURITY_ATTRIBUTES);
		sa.lpSecurityDescriptor = NULL;
		sa.bInheritHandle = TRUE;

		// Create the child output pipe (override default 4K buffer size)
		if (!CreatePipe(&rdoutpipe, &startup_info.hStdOutput, &sa, sizeof(buf))) {
			errormsg(WHERE, ERR_CREATE, "stdout pipe", 0);
			strListFreeBlock(env_block);
			return GetLastError();
		}
		startup_info.hStdError = startup_info.hStdOutput;

		// Create the child input pipe.
		if (!CreatePipe(&startup_info.hStdInput, &wrinpipe, &sa, sizeof(buf))) {
			errormsg(WHERE, ERR_CREATE, "stdin pipe", 0);
			CloseHandle(rdoutpipe);
			strListFreeBlock(env_block);
			return GetLastError();
		}

		DuplicateHandle(
			GetCurrentProcess(), rdoutpipe,
			GetCurrentProcess(), &rdslot, 0, FALSE, DUPLICATE_SAME_ACCESS);

		DuplicateHandle(
			GetCurrentProcess(), wrinpipe,
			GetCurrentProcess(), &wrslot, 0, FALSE, DUPLICATE_SAME_ACCESS);

		CloseHandle(rdoutpipe);
		CloseHandle(wrinpipe);

		startup_info.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
		startup_info.wShowWindow = SW_HIDE;
	}
	if (native && !(mode & (EX_OFFLINE | EX_STDIN))) {
		if (passthru_thread_running)
			passthru_socket_activate(true);
		else
			input_thread_mutex_locked = (pthread_mutex_lock(&input_thread_mutex) == 0);
	}

	DWORD creation_flags = (mode & EX_NODISPLAY) ? CREATE_NO_WINDOW : CREATE_NEW_CONSOLE;
	success = CreateProcess(
		NULL,           // pointer to name of executable module
		fullcmdline,    // pointer to command line string
		NULL,           // process security attributes
		NULL,           // thread security attributes
		native && !(mode & EX_OFFLINE),               // handle inheritance flag
		creation_flags, // creation flags
		env_block,      // pointer to new environment block
		p_startup_dir,  // pointer to current directory name
		&startup_info,  // pointer to STARTUPINFO
		&process_info   // pointer to PROCESS_INFORMATION
		);

	strListFreeBlock(env_block);

	if (!success) {
		XTRN_CLEANUP;
		if (native && !(mode & (EX_OFFLINE | EX_STDIN))) {
			if (passthru_thread_running)
				passthru_socket_activate(false);
			else if (input_thread_mutex_locked)
				pthread_mutex_unlock(&input_thread_mutex);
		}
		SetLastError(last_error);   /* Restore LastError */
		errormsg(WHERE, ERR_EXEC, realcmdline, mode);
		SetLastError(last_error);   /* Restore LastError */
		return GetLastError();
	}

#if 0
	char dbgstr[256];
	sprintf(dbgstr, "Node %d created: hProcess %X hThread %X processID %X threadID %X\n"
	        , cfg.node_num
	        , process_info.hProcess
	        , process_info.hThread
	        , process_info.dwProcessId
	        , process_info.dwThreadId);
	OutputDebugString(dbgstr);
#endif

	CloseHandle(process_info.hThread);

	/* Disable Ctrl-C checking */
	if (!(mode & EX_OFFLINE))
		rio_abortable = false;

	// Executing app in foreground?, monitor
	retval = STILL_ACTIVE;
	while (!(mode & EX_BG)) {
		if (mode & EX_CHKTIME)
			gettimeleft();
		if (!online && !(mode & EX_OFFLINE)) { // Tell VDD and external that user hung-up
			if (was_online) {
				logline(LOG_NOTICE, "X!", "hung-up in external program");
				hungup = time(NULL);
				if (!native) {
					SetEvent(hungup_event);
				}
				was_online = false;
			}
			if (hungup && time(NULL) - hungup > 5 && !processTerminated) {
				lprintf(LOG_INFO, "Terminating process from line %d", __LINE__);
				processTerminated = TerminateProcess(process_info.hProcess, 2112);
			}
		}
		if ((native && !use_pipes) || mode & EX_OFFLINE) {
			/* Monitor for process termination only */
			if (WaitForSingleObject(process_info.hProcess, 1000) == WAIT_OBJECT_0)
				break;
		} else {

			/* Write to VDD */

			wr = RingBufPeek(&inbuf, buf, sizeof(buf));
			if (wr) {
				if (!use_pipes && wrslot == INVALID_HANDLE_VALUE) {
					sprintf(str, "\\\\.\\mailslot\\sbbsexec\\wr%d"
					        , cfg.node_num);
					wrslot = CreateFile(str
					                    , GENERIC_WRITE
					                    , FILE_SHARE_READ
					                    , NULL
					                    , OPEN_EXISTING
					                    , FILE_ATTRIBUTE_NORMAL
					                    , (HANDLE) NULL);
					if (wrslot == INVALID_HANDLE_VALUE)
						lprintf(LOG_DEBUG, "!ERROR %u (%s) opening %s", GetLastError(), strerror(errno), str);
					else
						lprintf(LOG_DEBUG, "CreateFile(%s)=0x%x", str, wrslot);
				}

				/* CR expansion */
				if (use_pipes) {
					size_t sz;
					bp = cr_expand(buf, wr, output_buf, sz);
					wr = sz;
				} else
					bp = buf;

				len = 0;
				if (wrslot == INVALID_HANDLE_VALUE) {
					if (WaitForSingleObject(process_info.hProcess, 0) != WAIT_OBJECT_0)  // Process still running?
						lprintf(LOG_WARNING, "VDD Open failed (not loaded yet?)");
				} else if (!WriteFile(wrslot, bp, wr, &len, NULL)) {
					if (WaitForSingleObject(process_info.hProcess, 0) != WAIT_OBJECT_0) { // Process still running?
						lprintf(LOG_ERR, "!VDD WriteFile(0x%x, %u) FAILURE (Error=%u)", wrslot, wr, GetLastError());
						if (GetMailslotInfo(wrslot, &wr, NULL, NULL, NULL))
							lprintf(LOG_DEBUG, "!VDD MailSlot max_msg_size=%u", wr);
						else
							lprintf(LOG_DEBUG, "!GetMailslotInfo(0x%x)=%u", wrslot, GetLastError());
					}
				} else {
					if (len != wr)
						lprintf(LOG_WARNING, "VDD short write (%u instead of %u)", len, wr);
					RingBufRead(&inbuf, NULL, len);
					if (use_pipes && !(mode & EX_NOECHO)) {
						/* echo */
						RingBufWrite(&outbuf, bp, len);
					}
				}
				wr = len;
			}

			/* Read from VDD */

			rd = 0;
			len = sizeof(buf);
			avail = RingBufFree(&outbuf) / 2;   // leave room for wwiv/telnet expansion
#if 0
			if (avail == 0)
				lprintf("Node %d !output buffer full (%u bytes)"
				        , cfg.node_num, RingBufFull(&outbuf));
#endif
			if (len > avail)
				len = avail;

			while (rd < len) {
				unsigned long waiting = 0;

				if (use_pipes)
					PeekNamedPipe(
						rdslot,             // handle to pipe to copy from
						NULL,               // pointer to data buffer
						0,                  // size, in bytes, of data buffer
						NULL,               // pointer to number of bytes read
						&waiting,           // pointer to total number of bytes available
						NULL                // pointer to unread bytes in this message
						);
				else
					GetMailslotInfo(
						rdslot,             // mailslot handle
						NULL,               // address of maximum message size
						NULL,               // address of size of next message
						&waiting,           // address of number of messages
						NULL                // address of read time-out
						);
				if (!waiting)
					break;
				if (ReadFile(rdslot, buf + rd, len - rd, &msglen, NULL) == FALSE || msglen < 1)
					break;
				rd += msglen;
			}

			if (rd) {
				if (mode & EX_WWIV) {
					bp = wwiv_expand(buf, rd, wwiv_buf, rd, useron.misc, wwiv_flag);
					if (rd > sizeof(wwiv_buf))
						lprintf(LOG_ERR, "WWIV_BUF OVERRUN");
				} else if (telnet_mode & TELNET_MODE_OFF) {
					bp = buf;
				} else {
					rd = telnet_expand(buf, rd, telnet_buf, sizeof(telnet_buf), /* expand_cr: */ false, &bp);
				}
				if (rd > RingBufFree(&outbuf)) {
					lprintf(LOG_ERR, "output buffer overflow");
					rd = RingBufFree(&outbuf);
				}
				if (mode & EX_BIN)
					RingBufWrite(&outbuf, bp, rd);
				else
					rputs((char*)bp, rd);
			}
#if defined(_DEBUG) && 0
			if (rd > 1) {
				sprintf(str, "Node %d read %5d bytes from xtrn", cfg.node_num, rd);
				OutputDebugString(str);
			}
#endif
			if ((!rd && !wr) || hungup) {

				loop_since_io++;    /* number of loop iterations with no I/O */

				/* only check process termination after 300 milliseconds of no I/O */
				/* to allow for last minute reception of output from DOS programs */
				if (loop_since_io >= 3) {

					if (online && hangup_event != NULL
					    && WaitForSingleObject(hangup_event, 0) == WAIT_OBJECT_0) {
						lputs(LOG_NOTICE, "External program requested hangup (dropped DTR)");
						hangup();
					}

					if (WaitForSingleObject(process_info.hProcess, 0) == WAIT_OBJECT_0)
						break;  /* Process Terminated */
				}

				/* only check node for interrupt flag every 3 seconds of no I/O */
				if ((loop_since_io % 30) == 0) {
					// Check if the node has been interrupted
					getnodedat(cfg.node_num, &thisnode);
					if (thisnode.misc & NODE_INTR)
						break;
				}

				/* only send telnet GA every 30 seconds of no I/O */
				if ((loop_since_io % 300) == 0) {
#if defined(_DEBUG)
					sprintf(str, "Node %d xtrn idle\n", cfg.node_num);
					OutputDebugString(str);
#endif
					// Let's make sure the socket is up
					// Sending will trigger a socket d/c detection
					if (!(startup->options & BBS_OPT_NO_TELNET_GA))
						send_telnet_cmd(TELNET_GA, 0);
				}
				WaitForEvent(inbuf.data_event, 100);
			} else
				loop_since_io = 0;
		}
	}

	if (!(mode & EX_BG)) {         /* !background execution */

		if (GetExitCodeProcess(process_info.hProcess, &retval) == FALSE)
			errormsg(WHERE, ERR_CHK, "ExitCodeProcess", (DWORD)process_info.hProcess);

		if (retval == STILL_ACTIVE) {
			lprintf(LOG_INFO, "Terminating process from line %d", __LINE__);
			processTerminated = TerminateProcess(process_info.hProcess, GetLastError());
		}

		// Get return value
		if (!native && !processTerminated) {
			SAFEPRINTF(path, "%sDOSXTRN.RET", cfg.node_dir);
			FILE* fp = fopen(path, "r");
			if (fp == NULL) {
				lprintf(LOG_ERR, "Error %d opening %s after running %s", errno, path, cmdline);
			} else {
				if (fscanf(fp, "%d", &retval) != 1) {
					lprintf(LOG_ERR, "Error reading return value from %s", path);
					retval = -2;
				}
				fclose(fp);
			}
			if (retval == -1) {
				SAFEPRINTF(path, "%sDOSXTRN.ERR", cfg.node_dir);
				fp = fopen(path, "r");
				if (fp == NULL) {
					lprintf(LOG_ERR, "Error %d opening %s after DOSXTRN.RET contained -1", errno, path);
				} else {
					char errstr[256] = "";
					int  errval = 0;
					if (fscanf(fp, "%d\n", &errval) == 1) {
						fgets(errstr, sizeof(errstr), fp);
						truncsp(errstr);
						lprintf(LOG_ERR, "DOSXTRN Error %d (%s) executing: %s", errval, errstr, cmdline);
					} else
						lprintf(LOG_ERR, "DOSXTRN.RET contained -1 and we failed to parse: %s", path);
					fclose(fp);
				}
			}
		}
	}

	XTRN_CLEANUP;
	CloseHandle(process_info.hProcess);

	if (!(mode & EX_OFFLINE)) {    /* !off-line execution */

		if (!WaitForOutbufEmpty(5000))
			lprintf(LOG_WARNING, "%s Timeout waiting for output buffer to empty", __FUNCTION__);

		if (native && !(mode & EX_STDIN)) {
			if (passthru_thread_running)
				passthru_socket_activate(false);
			else if (input_thread_mutex_locked)
				pthread_mutex_unlock(&input_thread_mutex);
		}

		curatr = ~0;          // Can't guarantee current attributes
		attr(LIGHTGRAY);    // Force to "normal"

		rio_abortable = rio_abortable_save;   // Restore abortable state
	}

//	lprintf("%s returned %d",realcmdline, retval);

	errorlevel = retval; // Baja or JS retrievable error value

	return retval;
}

#else   /* !WIN32 */

/*****************************************************************************/
// Expands Unix LF to CRLF
/*****************************************************************************/
BYTE* lf_expand(BYTE* inbuf, uint inlen, BYTE* outbuf, size_t& newlen)
{
	uint i, j;

	for (i = j = 0; i < inlen; i++) {
		if (inbuf[i] == '\n' && (!i || inbuf[i - 1] != '\r'))
			outbuf[j++] = '\r';
		outbuf[j++] = inbuf[i];
	}
	newlen = j;
	return outbuf;
}

#define MAX_ARGS 1000

#ifdef NEEDS_SETENV
static int setenv(const char *name, const char *value, int overwrite)
{
	char *envstr;
	char *oldenv;
	if (overwrite || getenv(name) == NULL) {
		envstr = (char *)malloc(strlen(name) + strlen(value) + 2);
		if (envstr == NULL) {
			errno = ENOMEM;
			return -1;
		}
		/* Note, on some platforms, this can be free()d... */
		sprintf(envstr, "%s=%s", name, value);
		putenv(envstr);
	}
	return 0;
}
#endif

#ifdef NEEDS_CFMAKERAW
void
cfmakeraw(struct termios *t)
{
	t->c_iflag &= ~(IMAXBEL | IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	t->c_oflag &= ~OPOST;
	t->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	t->c_cflag &= ~(CSIZE | PARENB);
	t->c_cflag |= CS8;
}
#endif

#ifdef NEEDS_FORKPTY
static int login_tty(int fd)
{
	(void) setsid();
	if (!isatty(fd))
		return -1;
	(void) dup2(fd, 0);
	(void) dup2(fd, 1);
	(void) dup2(fd, 2);
	if (fd > 2)
		(void) close(fd);
	return 0;
}

#ifdef NEEDS_DAEMON
/****************************************************************************/
/* Daemonizes the process                                                   */
/****************************************************************************/
int
daemon(int nochdir, int noclose)
{
	int fd;

	switch (fork()) {
		case -1:
			return -1;
		case 0:
			break;
		default:
			_exit(0);
	}

	if (setsid() == -1)
		return -1;

	if (!nochdir)
		(void)chdir("/");

	if (!noclose && (fd = open(_PATH_DEVNULL, O_RDWR, 0)) != -1) {
		(void)dup2(fd, STDIN_FILENO);
		(void)dup2(fd, STDOUT_FILENO);
		(void)dup2(fd, STDERR_FILENO);
		if (fd > 2)
			(void)close(fd);
	}
	return 0;
}
#endif

static int openpty(int *amaster, int *aslave, char *name, struct termios *termp, winsize *winp)
{
	char          line[] = "/dev/ptyXX";
	const char *  cp1, *cp2;
	int           master, slave, ttygid;
	struct group *gr;

	if ((gr = getgrnam("tty")) != NULL)
		ttygid = gr->gr_gid;
	else
		ttygid = -1;

	for (cp1 = "pqrsPQRS"; *cp1; cp1++) {
		line[8] = *cp1;
		for (cp2 = "0123456789abcdefghijklmnopqrstuv"; *cp2; cp2++) {
			line[5] = 'p';
			line[9] = *cp2;
			if ((master = open(line, O_RDWR, 0)) == -1) {
				if (errno == ENOENT)
					break; /* try the next pty group */
			} else {
				line[5] = 't';
				(void) chown(line, getuid(), ttygid);
				(void) chmod(line, S_IRUSR | S_IWUSR | S_IWGRP);
				/* Hrm... SunOS doesn't seem to have revoke
				(void) revoke(line); */
				if ((slave = open(line, O_RDWR, 0)) != -1) {
					*amaster = master;
					*aslave = slave;
					if (name)
						strcpy(name, line);
					if (termp)
						(void) tcsetattr(slave,
						                 TCSAFLUSH, termp);
					if (winp)
						(void) ioctl(slave, TIOCSWINSZ,
						             (char *)winp);
					return 0;
				}
				(void) close(master);
			}
		}
	}
	errno = ENOENT; /* out of ptys */
	return -1;
}

static int forkpty(int *amaster, char *name, termios *termp, winsize *winp)
{
	int master, slave, pid;

	if (openpty(&master, &slave, name, termp, winp) == -1)
		return -1;
	switch (pid = FORK()) {
		case -1:
			return -1;
		case 0:
			/*
			 * child
			 */
			(void) close(master);
			login_tty(slave);
			return 0;
	}
	/*
	 * parent
	 */
	*amaster = master;
	(void) close(slave);
	return pid;
}
#endif /* NEED_FORKPTY */

static int
xtrn_waitpid(pid_t wpid, int *status, int options)
{
	int ret;

	do {
		ret = waitpid(wpid, status, options);
	} while(ret == -1 && errno == EINTR);

	return ret;
}

/****************************************************************************/
/* Runs an external program (on *nix) 										*/
/****************************************************************************/
int sbbs_t::external(const char* cmdline, int mode, const char* startup_dir)
{
	char          str[MAX_PATH + 1];
	char          fname[MAX_PATH + 1];
	char          fullpath[MAX_PATH + 1];
	char          fullcmdline[MAX_PATH + 1];
	char*         argv[MAX_ARGS + 1];
	BYTE*         bp;
	BYTE          buf[XTRN_IO_BUF_LEN];
	BYTE          output_buf[XTRN_IO_BUF_LEN * 2];
	uint          avail;
	size_t        output_len;
	bool          native = false;   // DOS program by default
	bool          rio_abortable_save = rio_abortable;
	int           i;
	bool          data_waiting;
	int           rd;
	int           wr;
	int           argc;
	pid_t         pid;
	int           in_pipe[2];
	int           out_pipe[2];
	int           err_pipe[2];
	BYTE          wwiv_buf[XTRN_IO_BUF_LEN * 2];
	BYTE          utf8_buf[XTRN_IO_BUF_LEN * 4];
	bool          wwiv_flag = false;
	bool          input_thread_mutex_locked = false;
	char*         p;
#ifdef PREFER_POLL
	struct pollfd fds[2];
#else
#error select() implementation was removed in 3971ef4d
#endif

	xtrn_mode = mode;
	lprintf(LOG_DEBUG, "Executing external: %s", cmdline);

	term->clear_hotspots();

	if (startup_dir == NULL)
		startup_dir = nulstr;

	XTRN_LOADABLE_MODULE(cmdline, startup_dir);
	XTRN_LOADABLE_JS_MODULE(cmdline, mode, startup_dir);

	attr(cfg.color[clr_external]);  /* setup default attributes */

	native = native_executable(&cfg, cmdline, mode);

	SAFECOPY(str, cmdline);          /* Set fname to program name only */
	truncstr(str, " ");
	SAFECOPY(fname, getfname(str));

	snprintf(fullpath, sizeof fullpath, "%s%s", startup_dir, fname);
	if (cmdline[0] != '/' && cmdline[0] != '.' && fexist(fullpath))
		snprintf(fullcmdline, sizeof fullcmdline, "%s%s", startup_dir, cmdline);
	else
		SAFECOPY(fullcmdline, cmdline);

	if (native) { // Native (not MS-DOS) external

		if (startup_dir[0] && !isdir(startup_dir)) {
			errormsg(WHERE, ERR_CHK, startup_dir, 0);
			return -1;
		}

		// Current environment passed to child process
		snprintf(dszlog, sizeof dszlog, "%sPROTOCOL.LOG", cfg.node_dir);
		setenv("DSZLOG", dszlog, 1);      /* Makes the DSZ LOG active */
		setenv("SBBSNODE", cfg.node_dir, 1);
		setenv("SBBSCTRL", cfg.ctrl_dir, 1);
		setenv("SBBSDATA", cfg.data_dir, 1);
		setenv("SBBSEXEC", cfg.exec_dir, 1);
		snprintf(str, sizeof str, "%u", cfg.node_num);
		setenv("SBBSNNUM", str, 1);

		/* date/time env vars */
		now = time(NULL);
		struct  tm tm;
		if (localtime_r(&now, &tm) == NULL)
			memset(&tm, 0, sizeof(tm));
		snprintf(str, sizeof str, " %02u", tm.tm_mday);
		setenv("DAY", str, /* overwrite */ TRUE);
		setenv("WEEKDAY", wday[tm.tm_wday], /* overwrite */ TRUE);
		setenv("MONTHNAME", mon[tm.tm_mon], /* overwrite */ TRUE);
		snprintf(str, sizeof str, "%02u", tm.tm_mon + 1);
		setenv("MONTH", str, /* overwrite */ TRUE);
		snprintf(str, sizeof str, "%u", 1900 + tm.tm_year);
		if (setenv("YEAR", str, /* overwrite */ TRUE) != 0)
			errormsg(WHERE, ERR_WRITE, "environment", 0);

	} else {
		if (startup->options & BBS_OPT_NO_DOS) {
			lprintf((mode & EX_OFFLINE) ? LOG_ERR : LOG_WARNING, "DOS programs not supported: %s", cmdline);
			bputs(text[NoDOS]);
			return -1;
		}
#if defined(__FreeBSD__)
		/* ToDo: This seems to work for every door except Iron Ox
		   ToDo: Iron Ox is unique in that it runs perfectly from
		   ToDo: tcsh but not at all from anywhere else, complaining
		   ToDo: about corrupt files.  I've ruled out the possibilty
		   ToDo: of it being a terminal mode issue... no other ideas
		   ToDo: come to mind. */

		FILE * doscmdrc;

		snprintf(str, sizeof str, "%s.doscmdrc", cfg.node_dir);
		if ((doscmdrc = fopen(str, "w+")) == NULL)  {
			errormsg(WHERE, ERR_CREATE, str, 0);
			return -1;
		}
		if (startup_dir[0])
			fprintf(doscmdrc, "assign C: %s\n", startup_dir);
		else
			fprintf(doscmdrc, "assign C: .\n");

		fprintf(doscmdrc, "assign D: %s\n", cfg.node_dir);
		SAFECOPY(str, cfg.exec_dir);
		if ((p = strrchr(str, '/')) != NULL)
			*p = 0;
		if ((p = strrchr(str, '/')) != NULL)
			*p = 0;
		fprintf(doscmdrc, "assign E: %s\n", str);

		/* setup doscmd env here */
		/* ToDo Note, this assumes that the BBS uses standard dir names */
		fprintf(doscmdrc, "DSZLOG=E:\\node%d\\PROTOCOL.LOG\n", cfg.node_num);
		fprintf(doscmdrc, "SBBSNODE=D:\\\n");
		fprintf(doscmdrc, "SBBSCTRL=E:\\ctrl\\\n");
		fprintf(doscmdrc, "SBBSDATA=E:\\data\\\n");
		fprintf(doscmdrc, "SBBSEXEC=E:\\exec\\\n");
		fprintf(doscmdrc, "SBBSNNUM=%d\n", cfg.node_num);
		fprintf(doscmdrc, "PCBNODE=%d\n", cfg.node_num);
		fprintf(doscmdrc, "PCBDRIVE=D:\n");
		fprintf(doscmdrc, "PCBDIR=\\\n");

		fclose(doscmdrc);
		SAFECOPY(str, fullcmdline);
		snprintf(fullcmdline, sizeof fullcmdline, "%s -F %s", startup->dosemu_path, str);

#elif defined(__linux__)

		/* dosemu integration  --  originally by Ryan Underwood, <nemesis @ icequake.net> */

		FILE *      dosemubatfp;
		FILE *      externalbatfp;
		FILE *      de_launch_inifp;
		char        tok[MAX_PATH + 1];
		char        buf[1024];
		char        bufout[1024];

		char        cmdlinebatch[MAX_PATH + 1];
		char        externalbatsrc[MAX_PATH + 1];
		char        externalbat[MAX_PATH + 1];
		char        dosemuconf[MAX_PATH + 1];
		char        de_launch_cmd[INI_MAX_VALUE_LEN];
		char        dosemubinloc[MAX_PATH + 1];
		char        virtualconf[75];
		char        dosterm[15];
		char        log_external[MAX_PATH + 1];
		const char* runtype;
		str_list_t  de_launch_ini;

		/*  on the Unix side. xtrndir is the parent of the door's startup dir. */
		char        xtrndir[MAX_PATH + 1];

		/*  on the DOS side.  */
		char        xtrndir_dos[MAX_PATH + 1];
		char        ctrldir_dos[MAX_PATH + 1];
		char        datadir_dos[MAX_PATH + 1];
		char        execdir_dos[MAX_PATH + 1];
		char        nodedir_dos[MAX_PATH + 1];

		/* Default locations that can be overridden by
		 * the sysop in emusetup.bat */

		const char ctrldrive[] = DOSEMU_CTRL_DRIVE;
		const char datadrive[] = DOSEMU_DATA_DRIVE;
		const char execdrive[] = DOSEMU_EXEC_DRIVE;
		const char nodedrive[] = DOSEMU_NODE_DRIVE;

		const char external_bat_fn[] = "external.bat";
		const char dosemu_cnf_fn[] = "dosemu.conf";

		SAFECOPY(str, startup_dir);
		if (*(p = lastchar(str)) == '/')     /* kill trailing slash */
			*p = 0;
		if ((p = strrchr(str, '/')) != NULL)  /* kill the last element of the path */
			*p = 0;

		SAFECOPY(xtrndir, str);

		SAFECOPY(xtrndir_dos, xtrndir);
		REPLACE_CHARS(xtrndir_dos, '/', '\\', p);

		SAFECOPY(ctrldir_dos, cfg.ctrl_dir);
		REPLACE_CHARS(ctrldir_dos, '/', '\\', p);

		p = lastchar(ctrldir_dos);
		if (*p == '\\')
			*p = 0;

		SAFECOPY(datadir_dos, cfg.data_dir);
		REPLACE_CHARS(datadir_dos, '/', '\\', p);

		p = lastchar(datadir_dos);
		if (*p == '\\')
			*p = 0;

		SAFECOPY(execdir_dos, cfg.exec_dir);
		REPLACE_CHARS(execdir_dos, '/', '\\', p);

		p = lastchar(execdir_dos);
		if (*p == '\\')
			*p = 0;

		SAFECOPY(nodedir_dos, cfg.node_dir);
		REPLACE_CHARS(nodedir_dos, '/', '\\', p);

		p = lastchar(nodedir_dos);
		if (*p == '\\')
			*p = 0;

		/* must have sbbs.ini bbs useDOSemu=1 (or empty), cannot be =0 */
		if (!startup->usedosemu) {
			lprintf((mode & EX_OFFLINE) ? LOG_ERR : LOG_WARNING, "DOSEMU disabled, program not run");
			bputs(text[NoDOS]);
			return -1;
		}

		/* must have sbbs.ini bbs DOSemuPath set to valid path */
		SAFECOPY(dosemubinloc, (cmdstr(startup->dosemu_path, nulstr, nulstr, tok)));
		if (dosemubinloc[0] == '\0') {
			lprintf((mode & EX_OFFLINE) ? LOG_ERR : LOG_WARNING, "DOSEMU invalid DOSEmuPath, program not run");
			bputs(text[NoDOS]);
			return -1;
		}

		if (!fexist(dosemubinloc)) {
			lprintf((mode & EX_OFFLINE) ? LOG_ERR : LOG_WARNING, "DOSEMU not found: %s", dosemubinloc);
			bputs(text[NoDOS]);
			return -1;
		}

		/* check for existence of a dosemu.conf in the door directory.
		 * It is a good idea to be able to use separate configs for each
		 * door.
		 *
		 * First check startup_dir, then check cfg.ctrl_dir
		 */
		SAFEPRINTF2(str, "%s%s", startup_dir, dosemu_cnf_fn);
		if (!fexist(str)) {
			/* If we can't find it in the door dir, look for the configured one */
			SAFECOPY(str, startup->dosemuconf_path);
			if (!isabspath(str)) {
				SAFEPRINTF2(str, "%s%s", cfg.ctrl_dir, startup->dosemuconf_path);
			}
			if (!fexist(str)) {
				/* If we couldn't find either, try for the system one, then
				 * error out. */
				SAFEPRINTF(str, "/etc/dosemu/%s", dosemu_cnf_fn);
				if (!fexist(str)) {
					SAFEPRINTF(str, "/etc/%s", dosemu_cnf_fn);
					if (!fexist(str)) {
						errormsg(WHERE, ERR_READ, str, 0);
						return -1;
					}
					else
						SAFECOPY(dosemuconf, str);  /* using system conf */
				}
				else
					SAFECOPY(dosemuconf, str);  /* using system conf */
			}
			else
				SAFECOPY(dosemuconf, str);   /* using global conf */
		}
		else
			SAFECOPY(dosemuconf, str);  /* using door-specific conf */

		/* Create the external bat here to be placed in the node dir. */
		SAFEPRINTF2(str, "%s%s", cfg.node_dir, external_bat_fn);
		if (!(dosemubatfp = fopen(str, "w+"))) {
			errormsg(WHERE, ERR_CREATE, str, 0);
			return -1;
		}

		fprintf(dosemubatfp, "@ECHO OFF\r\n");
		fprintf(dosemubatfp, "SET DSZLOG=%s\\PROTOCOL.LOG\r\n", nodedrive);
		fprintf(dosemubatfp, "SET SBBSNODE=%s\r\n", nodedrive);
		fprintf(dosemubatfp, "SET SBBSNNUM=%d\r\n", cfg.node_num);
		fprintf(dosemubatfp, "SET SBBSCTRL=%s\r\n", ctrldrive);
		fprintf(dosemubatfp, "SET SBBSDATA=%s\r\n", datadrive);
		fprintf(dosemubatfp, "SET SBBSEXEC=%s\r\n", execdrive);
		fprintf(dosemubatfp, "SET PCBNODE=%d\r\n", cfg.node_num);
		fprintf(dosemubatfp, "SET PCBDRIVE=%s\r\n", nodedrive);
		fprintf(dosemubatfp, "SET PCBDIR=\\\r\n");

		char gamedir[MAX_PATH + 1]{};
		if (startup_dir[0]) {
			SAFECOPY(str, startup_dir);
			*lastchar(str) = 0;
			SAFECOPY(gamedir, getfname(str));
		}

		if (*gamedir == 0) {
			lprintf(LOG_ERR, "No startup directory configured for DOS command-line: %s", cmdline);
			fclose(dosemubatfp);
			return -1;
		}

		/* external editors use node dir so unset this */
		if (startup_dir == cfg.node_dir) {
			*gamedir = '\0';
		}

		fprintf(dosemubatfp, "SET STARTDIR=%s\r\n", gamedir);

		/* Check the "Stdio Interception" flag from scfg for this door.  If it's
		 * enabled, we enable doorway mode.  Else, it's vmodem for us, unless
		 * it's a timed event.
		 */

		if (!(mode & (EX_STDIO)) && online != ON_LOCAL) {
			SAFECOPY(virtualconf, "-I\"serial { virtual com 1 }\"");
			runtype = "FOSSIL";
		} else {
			virtualconf[0] = '\0';
			runtype = "STDIO";
		}

		/* now append exec/external.bat (which is editable) to this
		 generated file */
		SAFEPRINTF2(str, "%s%s", startup_dir, external_bat_fn);

		if ((startup_dir == cfg.node_dir) || !fexist(str)) {
			SAFEPRINTF2(str, "%s%s", cfg.exec_dir, external_bat_fn);
			if (!fexist(str)) {
				errormsg(WHERE, ERR_READ, str, 0);
				fclose(dosemubatfp);
				return -1;
			}
		}

		SAFECOPY(externalbatsrc, str);

		if (!(externalbatfp = fopen(externalbatsrc, "r"))) {
			errormsg(WHERE, ERR_OPEN, externalbatsrc, 0);
			fclose(dosemubatfp);
			return -1;
		}

		/* append the command line to the batch file */
		SAFECOPY(tok, cmdline);
		truncstr(tok, " ");
		p = getfext(tok);
		/*  check if it's a bat file  */
		if (p != NULL && stricmp(p, ".bat") == 0) {
			SAFEPRINTF(cmdlinebatch, "CALL %s", cmdline);
		} else {
			SAFECOPY(cmdlinebatch, cmdline);
		}

		named_string_t externalbat_replacements[] = {
			{(char*)"CMDLINE", cmdlinebatch},
			{(char*)"DSZLOG", (char*)nodedrive},
			{(char*)"SBBSNODE", (char*)nodedrive},
			{(char*)"SBBSCTRL", (char*)ctrldrive},
			{(char*)"SBBSDATA", (char*)datadrive},
			{(char*)"SBBSEXEC", (char*)execdrive},
			{(char*)"XTRNDIR", xtrndir_dos},
			{(char*)"CTRLDIR", ctrldir_dos},
			{(char*)"DATADIR", datadir_dos},
			{(char*)"EXECDIR", execdir_dos},
			{(char*)"NODEDIR", nodedir_dos},
			{(char*)"STARTDIR", (char*)gamedir},
			{(char*)"RUNTYPE", (char *)runtype},
			{NULL, NULL}
		};

		named_long_t    externalbat_int_replacements[] = {
			{(char*)"SBBSNNUM", cfg.node_num },
			{NULL}
		};

		while (!feof(externalbatfp)) {
			if (fgets(buf, sizeof(buf), externalbatfp) != NULL) {
				replace_named_values(buf, bufout, sizeof(bufout), "$", NULL, externalbat_replacements,
				                     externalbat_int_replacements, FALSE);
				fprintf(dosemubatfp, "%s", bufout);
			}
		}

		fclose(externalbatfp);

		/* Set the interception bits, since we are always going to want Synchronet
		 * to intercept dos programs under Unix.
		 */

		mode |= EX_STDIO;

		/* Attempt to keep dosemu from prompting for a disclaimer. */

		snprintf(str, sizeof str, "%s/.dosemu", cfg.ctrl_dir);
		if (!isdir(str)) {
			if (mkdir(str, 0755) != 0) {
				errormsg(WHERE, ERR_MKDIR, str, 0755);
				fclose(dosemubatfp);
				return -1;
			}
		}
		strcat(str, "/disclaimer");
		ftouch(str);

		/* Set up the command for dosemu to execute with 'unix -e'. */
		SAFEPRINTF2(externalbat, "%s%s", nodedrive, external_bat_fn);

		/* need TERM=linux for maintenance programs to work
		 * (dosemu won't start with no controlling terminal)
		 * Also, redirect stdout to a log if it's a timed event.
		 */

		if (online == ON_LOCAL) {
			SAFECOPY(dosterm, "TERM=linux");
			safe_snprintf(log_external, sizeof(log_external), ">> %sdosevent_%s.log", cfg.logs_dir, fname);
		}
		else {
			dosterm[0] = '\0';
			log_external[0] = '\0';
		}

		/*
		 * Get the global emu launch command
		 */
		/* look for file in startup dir */
		SAFEPRINTF(str, "%sdosemu.ini", startup_dir);
		if (!fexist(str)) {
			/* look for file in exec dir */
			SAFEPRINTF(str, "%sdosemu.ini", cfg.exec_dir);
			if (!fexist(str)) {
				errormsg(WHERE, ERR_OPEN, "dosemu.ini", 0);
				fclose(dosemubatfp);
				return -1;
			}
		}

		/* if file found, then open and process it */
		if ((de_launch_inifp = iniOpenFile(str, false)) == NULL) {
			errormsg(WHERE, ERR_OPEN, str, 0);
			fclose(dosemubatfp);
			return -1;
		}
		de_launch_ini = iniReadFile(de_launch_inifp);
		iniCloseFile(de_launch_inifp);
		SAFECOPY(de_launch_cmd, "");
		iniGetString(de_launch_ini, ROOT_SECTION, "cmd", nulstr, de_launch_cmd);
		if (virtualconf[0] == '\0') {
			iniGetString(de_launch_ini, "stdio", "cmd", de_launch_cmd, de_launch_cmd);
		}
		iniFreeStringList(de_launch_ini);

		named_string_t de_ini_replacements[] =
		{
			{(char*)"TERM", dosterm},
			{(char*)"CTRLDIR", cfg.ctrl_dir},
			{(char*)"NODEDIR", cfg.node_dir},
			{(char*)"EXECDIR", cfg.exec_dir},
			{(char*)"DATADIR", cfg.data_dir},
			{(char*)"XTRNDIR", xtrndir},
			{(char*)"DOSEMUBIN", dosemubinloc},
			{(char*)"VIRTUALCONF", virtualconf},
			{(char*)"DOSEMUCONF", dosemuconf},
			{(char*)"EXTBAT", externalbat},
			{(char*)"EXTLOG", log_external},
			{(char*)"RUNTYPE", (char *)runtype},
			{NULL, NULL}
		};
		named_long_t    de_ini_int_replacements[] = {
			{(char*)"NNUM", cfg.node_num },
			{NULL}
		};
		replace_named_values(de_launch_cmd, fullcmdline, sizeof(fullcmdline), (char*)"$", NULL,
		                     de_ini_replacements, de_ini_int_replacements, FALSE);

		/* Drum roll. */
		fprintf(dosemubatfp, "REM For debugging: %s\r\n", fullcmdline);
		fclose(dosemubatfp);

#else
		lprintf((mode & EX_OFFLINE) ? LOG_ERR : LOG_WARNING, "DOS programs not supported: %s", cmdline);
		bputs(text[NoDOS]);

		return -1;
#endif
	}

	if (!(mode & (EX_STDIN | EX_OFFLINE))) {
		if (passthru_thread_running)
			passthru_socket_activate(true);
		else
			input_thread_mutex_locked = (pthread_mutex_lock(&input_thread_mutex) == 0);
	}

	if (!(mode & EX_NOLOG) && pipe(err_pipe) != 0) {
		errormsg(WHERE, ERR_CREATE, "err_pipe", 0);
		return -1;
	}

	if ((mode & EX_STDIO) == EX_STDIO)  {
		struct winsize winsize;
		struct termios termio;
		memset(&termio, 0, sizeof(termio));
		cfsetispeed(&termio, B19200);
		cfsetospeed(&termio, B19200);
		if (mode & EX_BIN)
			cfmakeraw(&termio);
		else {
			termio.c_iflag = TTYDEF_IFLAG;
			termio.c_oflag = TTYDEF_OFLAG;
			termio.c_lflag = TTYDEF_LFLAG;
			termio.c_cflag = TTYDEF_CFLAG;
			/*
			 * On Linux, ttydefchars is in the wrong order, so
			 * it's completely useless for anything.
			 * Instead, set any value we've ever heard of
			 * to a value we may have made up.
			 * TODO: We can set stuff from the user term here...
			 */
			for (unsigned noti = 0; noti < (sizeof(termio.c_cc) / sizeof(termio.c_cc[0])); noti++)
				termio.c_cc[noti] = _POSIX_VDISABLE;
#ifdef VEOF
			termio.c_cc[VEOF] = CEOF;
#endif
#ifdef VEOL
			termio.c_cc[VEOL] = CEOL;
#endif
#ifdef VEOL2
#ifdef CEOL2
			termio.c_cc[VEOL2] = CEOL2;
#else
			termio.c_cc[VEOL2] = CEOL;
#endif
#endif
#ifdef VERASE
			termio.c_cc[VERASE] = CERASE;
#endif
#ifdef VKILL
			termio.c_cc[VKILL] = CKILL;
#endif
#ifdef VREPRINT
			termio.c_cc[VREPRINT] = CREPRINT;
#endif
#ifdef VINTR
			termio.c_cc[VINTR] = CINTR;
#endif
#ifdef VERASE2
#ifdef CERASE2
			termio.c_cc[VERASE2] = CERASE2;
#else
			termio.c_cc[VERASE2] = CERASE;
#endif
#endif
#ifdef VQUIT
			termio.c_cc[VQUIT] = CQUIT;
#endif
#ifdef VSUSP
			termio.c_cc[VSUSP] = CSUSP;
#endif
#ifdef VDSUSP
			termio.c_cc[VDSUSP] = CDSUSP;
#endif
#ifdef VSTART
			termio.c_cc[VSTART] = CSTART;
#endif
#ifdef VSTOP
			termio.c_cc[VSTOP] = CSTOP;
#endif
#ifdef VLNEXT
			termio.c_cc[VLNEXT] = CLNEXT;
#endif
#ifdef VDISCARD
			termio.c_cc[VDISCARD] = CDISCARD;
#endif
#ifdef VMIN
			termio.c_cc[VMIN] = CMIN;
#endif
#ifdef VTIME
			termio.c_cc[VTIME] = CTIME;
#endif
#ifdef VSTATUS
			termio.c_cc[VSTATUS] = CSTATUS;
#endif
#ifdef VWERASE
			termio.c_cc[VWERASE] = CWERASE;
#endif
#ifdef VEOT
			termio.c_cc[VEOT] = CEOT;
#endif
#ifdef VBRK
			termio.c_cc[VBRK] = CBRK;
#endif
#ifdef VRPRNT
			termio.c_cc[VRPRNT] = CRPRNT;
#endif
#ifdef VFLUSH
			termio.c_cc[VFLUSH] = CFLUSH
#endif
		}
		winsize.ws_row = term->rows;
		winsize.ws_col = term->cols;
		if ((pid = forkpty(&in_pipe[1], NULL, &termio, &winsize)) == -1) {
			if (!(mode & (EX_STDIN | EX_OFFLINE))) {
				if (passthru_thread_running)
					passthru_socket_activate(false);
				else if (input_thread_mutex_locked)
					pthread_mutex_unlock(&input_thread_mutex);
			}
			errormsg(WHERE, ERR_EXEC, fullcmdline, 0);
			return -1;
		}
		out_pipe[0] = in_pipe[1];
	}
	else  {
		if (mode & EX_STDIN)
			if (pipe(in_pipe) != 0) {
				errormsg(WHERE, ERR_CREATE, "in_pipe", 0);
				return -1;
			}
		if (mode & EX_STDOUT)
			if (pipe(out_pipe) != 0) {
				errormsg(WHERE, ERR_CREATE, "out_pipe", 0);
				return -1;
			}


		if ((pid = FORK()) == -1) {
			if (!(mode & (EX_STDIN | EX_OFFLINE))) {
				if (passthru_thread_running)
					passthru_socket_activate(false);
				else if (input_thread_mutex_locked)
					pthread_mutex_unlock(&input_thread_mutex);
			}
			errormsg(WHERE, ERR_EXEC, fullcmdline, 0);
			return -1;
		}
	}
	if (pid == 0) {    /* child process */
		/* Give away all privs for good now */
		if (startup->setuid != NULL)
			startup->setuid(TRUE);

		sigset_t sigs;
		sigfillset(&sigs);
		sigprocmask(SIG_UNBLOCK, &sigs, NULL);
		if (!(mode & EX_BIN))  {
			if (term->supports(ANSI))
				SAFEPRINTF(term_env, "TERM=%s", startup->xtrn_term_ansi);
			else
				SAFEPRINTF(term_env, "TERM=%s", startup->xtrn_term_dumb);
			putenv(term_env);
		}
#ifdef __FreeBSD__
		if (!native)
			chdir(cfg.node_dir);
		else
#endif
		if (startup_dir[0])
			if (chdir(startup_dir) != 0) {
				errormsg(WHERE, ERR_CHDIR, startup_dir, 0);
				return -1;
			}

		if (mode & EX_SH || strcspn(fullcmdline, "<>|;\"") != strlen(fullcmdline)) {
			argv[0] = comspec;
			argv[1] = (char*)"-c";
			argv[2] = fullcmdline;
			argv[3] = NULL;
		} else {
			argv[0] = fullcmdline;    /* point to the beginning of the string */
			argc = 1;
			for (i = 0; fullcmdline[i] && argc < MAX_ARGS; i++)    /* Break up command line */
				if (fullcmdline[i] == ' ') {
					fullcmdline[i] = 0;           /* insert nulls */
					argv[argc++] = fullcmdline + i + 1; /* point to the beginning of the next arg */
				}
			argv[argc] = NULL;
		}

		if (mode & EX_STDIN && !(mode & EX_STDOUT))  {
			close(in_pipe[1]);      /* close write-end of pipe */
			dup2(in_pipe[0], 0);     /* redirect stdin */
			close(in_pipe[0]);      /* close excess file descriptor */
		}

		if (mode & EX_STDOUT && !(mode & EX_STDIN)) {
			close(out_pipe[0]);     /* close read-end of pipe */
			dup2(out_pipe[1], 1);    /* stdout */
			if (!(mode & EX_NOLOG))
				dup2(out_pipe[1], 2);   /* stderr */
			close(out_pipe[1]);     /* close excess file descriptor */
		}

		if (!(mode & EX_STDIO)) {
			int fd;

			/* Redirect stdio to /dev/null */
			if ((fd = open(_PATH_DEVNULL, O_RDWR, 0)) != -1) {
				dup2(fd, STDIN_FILENO);
				dup2(fd, STDOUT_FILENO);
				if (!(mode & EX_NOLOG))
					dup2(fd, STDERR_FILENO);
				if (fd > 2)
					close(fd);
			}
		}

		if (mode & EX_BG)  /* background execution, detach child */
		{
			if (daemon(TRUE, FALSE) != 0)
				lprintf(LOG_ERR, "!ERROR %d (%s) daemonizing: %s", errno, strerror(errno), argv[0]);
		}

		if (!(mode & EX_NOLOG)) {
			close(err_pipe[0]);     /* close read-end of pipe */
			dup2(err_pipe[1], 2);    /* stderr */
		}

		execvp(argv[0], argv);
		lprintf(LOG_ERR, "!ERROR %d (%s) executing: %s", errno, strerror(errno), argv[0]);
		_exit(-1);  /* should never get here */
	}

	if (strcmp(cmdline, fullcmdline) != 0)
		lprintf(LOG_DEBUG, "Executing cmd-line: %s", fullcmdline);

	/* Disable Ctrl-C checking */
	if (!(mode & EX_OFFLINE))
		rio_abortable = false;

	if (!(mode & EX_NOLOG))
		close(err_pipe[1]); /* close write-end of pipe */

	if (mode & EX_STDOUT) {
		if (!(mode & EX_STDIN))
			close(out_pipe[1]); /* close write-end of pipe */
		fds[0].fd = out_pipe[0];
		fds[0].events = POLLIN;
		if (!(mode & EX_NOLOG)) {
			fds[1].fd = err_pipe[0];
			fds[1].events = POLLIN;
		}
		time_t lastnodechk = 0;
		while (!terminated) {
			if (xtrn_waitpid(pid, &i, WNOHANG) != 0)    /* child exited */
				break;

			if (mode & EX_CHKTIME)
				gettimeleft();

			if (!online && !(mode & EX_OFFLINE)) {
				logline(LOG_NOTICE, "X!", "hung-up in external program");
				break;
			}

			/* Input */
			if (mode & EX_STDIN && RingBufFull(&inbuf)) {
				if ((wr = RingBufRead(&inbuf, buf, sizeof(buf))) != 0) {
					if (write(in_pipe[1], buf, wr) != wr)
						lprintf(LOG_ERR, "ERROR %d writing to pipe", errno);
				}
			}

			/* Output */
			bp = buf;
			i = 0;
			if (mode & EX_NOLOG)
				poll(fds, 1, 1);
			else {
				while (poll(fds, 2, 1) > 0 && (fds[1].revents)
				       && (i < (int)sizeof(buf) - 1))  {
					if ((rd = read(err_pipe[0], bp, 1)) > 0)  {
						i += rd;
						bp++;
						if (*(bp - 1) == '\n')
							break;
					}
					else
						break;
				}
				if (i > 0) {
					buf[i] = '\0';
					p = (char*)buf;
					truncsp(p);
					SKIP_WHITESPACE(p);
					if (*p)
						lprintf(LOG_NOTICE, "%s: %s", fname, p);
				}

				/* Eat stderr if mode is EX_BIN */
				if (mode & EX_BIN)  {
					bp = buf;
					i = 0;
				}
			}

			data_waiting = fds[0].revents;
			if (i == 0 && data_waiting == 0) {
				// only check node for interrupt flag every 3 seconds of no I/O
				if (difftime(time(NULL), lastnodechk) >= 3) {
					if (getnodedat(cfg.node_num, &thisnode)) {
						if (thisnode.misc & NODE_INTR)
							break;
						lastnodechk = time(NULL);
					}
				}
				continue;
			}

			avail = (RingBufFree(&outbuf) - i) / 2;   // Leave room for wwiv/telnet expansion
			if (avail == 0) {
#if 0
				lprintf("Node %d !output buffer full (%u bytes)"
				        , cfg.node_num, RingBufFull(&outbuf));
#endif
				YIELD();
				continue;
			}

			rd = avail;

			if (rd > ((int)sizeof(buf) - i))
				rd = sizeof(buf) - i;

			if (data_waiting)  {
				rd = read(out_pipe[0], bp, rd);
				if (rd < 1 && i == 0)
					continue;
				if (rd < 0)
					rd = 0;
			}
			else
				rd = 0;

			rd += i;

			if (mode & EX_BIN) {
				if (telnet_mode & TELNET_MODE_OFF) {
					bp = buf;
					output_len = rd;
				}
				else
					output_len = telnet_expand(buf, rd, output_buf, sizeof(output_buf), /* expand_cr: */ false, &bp);
			} else {
				if ((mode & EX_STDIO) != EX_STDIO) {
					/* LF to CRLF expansion */
					bp = lf_expand(buf, rd, output_buf, output_len);
				} else if (mode & EX_WWIV) {
					bp = wwiv_expand(buf, rd, wwiv_buf, output_len, useron.misc, wwiv_flag);
					if (output_len > sizeof(wwiv_buf))
						lprintf(LOG_ERR, "WWIV_BUF OVERRUN");
				} else {
					bp = buf;
					output_len = rd;
				}
				if (term->charset() == CHARSET_PETSCII)
					petscii_convert(bp, output_len);
				else if (term->charset() == CHARSET_UTF8)
					bp = cp437_to_utf8(bp, output_len, utf8_buf, sizeof utf8_buf);
			}
			/* Did expansion overrun the output buffer? */
			if (output_len > sizeof(output_buf)) {
				lprintf(LOG_ERR, "OUTPUT_BUF OVERRUN");
				output_len = sizeof(output_buf);
			}

			/* Does expanded size fit in the ring buffer? */
			if (output_len > RingBufFree(&outbuf)) {
				lprintf(LOG_ERR, "output buffer overflow");
				output_len = RingBufFree(&outbuf);
			}

			RingBufWrite(&outbuf, bp, output_len);

		}

		if (xtrn_waitpid(pid, &i, WNOHANG) == 0)  {     // Child still running?
			kill(pid, SIGHUP);                  // Tell child user has hung up
			time_t start = time(NULL);            // Wait up to 5 seconds
			while (time(NULL) - start < 5) {        // for child to terminate
				if (xtrn_waitpid(pid, &i, WNOHANG) != 0)
					break;
				mswait(500);
			}
			if (xtrn_waitpid(pid, &i, WNOHANG) == 0) {  // Child still running?
				kill(pid, SIGTERM);             // terminate child process (gracefully)
				start = time(NULL);               // Wait up to 5 (more) seconds
				while (time(NULL) - start < 5) {    // for child to terminate
					if (xtrn_waitpid(pid, &i, WNOHANG) != 0)
						break;
					mswait(500);
				}
				if (xtrn_waitpid(pid, &i, WNOHANG) == 0) // Child still running?
					kill(pid, SIGKILL);         // terminate child process (ungracefully)
			}
		}
		/* close unneeded descriptors */
		if (mode & EX_STDIN)
			close(in_pipe[1]);
		close(out_pipe[0]);
	}
	if (mode & EX_NOLOG)
		xtrn_waitpid(pid, &i, 0);
	else {
		while (xtrn_waitpid(pid, &i, WNOHANG) == 0)  {
			bp = buf;
			i = 0;
			while (socket_readable(err_pipe[0], 1000) && (i < XTRN_IO_BUF_LEN - 1))  {
				if ((rd = read(err_pipe[0], bp, 1)) > 0)  {
					i += rd;
					if (*bp == '\n') {
						buf[i] = '\0';
						p = (char*)buf;
						truncsp(p);
						SKIP_WHITESPACE(p);
						if (*p)
							lprintf(LOG_NOTICE, "%s: %s", fname, p);
						i = 0;
						bp = buf;
					}
					else
						bp++;
				}
				else
					break;
			}
			if (i > 0) {
				buf[i] = '\0';
				p = (char*)buf;
				truncsp(p);
				SKIP_WHITESPACE(p);
				if (*p)
					lprintf(LOG_NOTICE, "%s: %s", fname, p);
			}
		}
	}
	if (!(mode & EX_OFFLINE)) {    /* !off-line execution */

		if (!WaitForOutbufEmpty(5000))
			lprintf(LOG_WARNING, "%s Timeout waiting for output buffer to empty", __FUNCTION__);

		if (!(mode & EX_STDIN)) {
			if (passthru_thread_running)
				passthru_socket_activate(false);
			else if (input_thread_mutex_locked)
				pthread_mutex_unlock(&input_thread_mutex);
		}

		curatr = ~0;          // Can't guarantee current attributes
		attr(LIGHTGRAY);    // Force to "normal"

		rio_abortable = rio_abortable_save;   // Restore abortable state
	}

	if (!(mode & EX_NOLOG))
		close(err_pipe[0]);

	return errorlevel = WEXITSTATUS(i);
}

#endif  /* !WIN32 */

static const char* quoted_string(const char* str, char* buf, size_t maxlen)
{
	if (strchr(str, ' ') == NULL)
		return str;
	safe_snprintf(buf, maxlen, "\"%s\"", str);
	return buf;
}

#define QUOTED_STRING(ch, str, buf, maxlen) \
		((IS_ALPHA(ch) && IS_UPPERCASE(ch)) ? str : quoted_string(str, buf, maxlen))

/*****************************************************************************/
/* Returns command line generated from instr with %c replacements            */
/*****************************************************************************/
char* sbbs_t::cmdstr(const char *instr, const char *fpath, const char *fspec, char *outstr, int mode)
{
	char str[MAX_PATH + 1], *cmd;
	int  i, j, len;
	bool native = (mode == EX_UNSPECIFIED) || native_executable(&cfg, instr, mode);
	(void) native;

	if (outstr == NULL)
		cmd = cmdstr_output;
	else
		cmd = outstr;
	len = strlen(instr);
	int maxlen = (int)sizeof(cmdstr_output) - 1;
	for (i = j = 0; i < len && j < maxlen; i++) {
		if (instr[i] == '%') {
			i++;
			cmd[j] = 0;
			int  avail = maxlen - j;
			char ch = instr[i];
			if (IS_ALPHA(ch))
				ch = toupper(ch);
			switch (ch) {
				case 'A':   /* User alias */
					strncat(cmd, QUOTED_STRING(instr[i], useron.alias, str, sizeof(str)), avail);
					break;
				case 'B':   /* Baud (DTE) Rate */
					strncat(cmd, ultoa(dte_rate, str, 10), avail);
					break;
				case 'C':   /* Connect Description */
					strncat(cmd, connection, avail);
					break;
				case 'D':   /* Connect (DCE) Rate */
					strncat(cmd, ultoa((uint)cur_rate, str, 10), avail);
					break;
				case 'E':   /* Estimated Rate */
					strncat(cmd, ultoa((uint)cur_cps * 10, str, 10), avail);
					break;
				case 'F':   /* File path */
#if defined(__linux__)
					if (!native && startup->usedosemu && strncmp(fpath, cfg.node_dir, strlen(cfg.node_dir)) == 0) {
						strncat(cmd, DOSEMU_NODE_DIR, avail);
						strncat(cmd, fpath + strlen(cfg.node_dir), avail);
					}
					else
#endif
					strncat(cmd, QUOTED_STRING(instr[i], fpath, str, sizeof(str)), avail);
					break;
				case 'G':   /* Temp directory */
#if defined(__linux__)
					if (!native && startup->usedosemu)
						strncat(cmd, DOSEMU_TEMP_DIR, avail);
					else
#endif
					strncat(cmd, cfg.temp_dir, avail);
					break;
				case 'H':   /* Socket Handle */
					strncat(cmd, ultoa(client_socket_dup, str, 10), avail);
					break;
				case 'I':   /* IP address */
					strncat(cmd, cid, avail);
					break;
				case 'J':
#if defined(__linux__)
					if (!native && startup->usedosemu)
						strncat(cmd, DOSEMU_DATA_DIR, avail);
					else
#endif
					strncat(cmd, cfg.data_dir, avail);
					break;
				case 'K':
#if defined(__linux__)
					if (!native && startup->usedosemu)
						strncat(cmd, DOSEMU_CTRL_DIR, avail);
					else
#endif
					strncat(cmd, cfg.ctrl_dir, avail);
					break;
				case 'L':   /* Lines per message */
					strncat(cmd, ultoa(cfg.level_linespermsg[useron.level], str, 10), avail);
					break;
				case 'M':   /* Minutes (credits) for user */
					strncat(cmd, ultoa(useron.min, str, 10), avail);
					break;
				case 'N':   /* Node Directory (same as SBBSNODE environment var) */
#if defined(__linux__)
					if (!native && startup->usedosemu)
						strncat(cmd, DOSEMU_NODE_DIR, avail);
					else
#endif
					strncat(cmd, cfg.node_dir, avail);
					break;
				case 'O':   /* SysOp */
					strncat(cmd, QUOTED_STRING(instr[i], cfg.sys_op, str, sizeof(str)), avail);
					break;
				case 'P':   /* Client protocol */
					strncat(cmd, passthru_thread_running ? "raw" : client.protocol, avail);
					break;
				case 'Q':   /* QWK ID */
					strncat(cmd, cfg.sys_id, avail);
					break;
				case 'R':   /* Rows */
					strncat(cmd, ultoa(term->rows, str, 10), avail);
					break;
				case 'S':   /* File Spec (or Baja command str) or startup-directory */
					strncat(cmd, fspec, avail);
					break;
				case 'T':   /* Time left in seconds */
					gettimeleft();
					strncat(cmd, ultoa(timeleft, str, 10), avail);
					break;
				case 'U':   /* UART I/O Address (in hex) */
					strncat(cmd, ultoa(cfg.com_base, str, 16), avail);
					break;
				case 'V':   /* Synchronet Version */
					snprintf(str, sizeof str, "%s%c", VERSION, REVISION);
					strncat(cmd, str, avail);
					break;
				case 'W':   /* Columns (width) */
					strncat(cmd, ultoa(term->cols, str, 10), avail);
					break;
				case 'X':
					strncat(cmd, cfg.shell[useron.shell]->code, avail);
					break;
				case '&':   /* Address of msr */
					break;
				case 'Y':
					strncat(cmd, comspec, avail);
					break;
				case 'Z':
#if defined(__linux__)
					if (!native && startup->usedosemu)
						strncat(cmd, DOSEMU_TEXT_DIR, avail);
					else
#endif
					strncat(cmd, cfg.text_dir, avail);
					break;
				case '~':   /* DOS-compatible (8.3) filename */
#ifdef _WIN32
					char sfpath[MAX_PATH + 1];
					SAFECOPY(sfpath, fpath);
					GetShortPathName(fpath, sfpath, sizeof(sfpath));
					strncat(cmd, sfpath, avail);
#else
					strncat(cmd, QUOTED_STRING(instr[i], fpath, str, sizeof(str)), avail);
#endif
					break;
				case '!':   /* EXEC Directory */
#if defined(__linux__)
					if (!native && startup->usedosemu)
						strncat(cmd, DOSEMU_EXEC_DIR, avail);
					else
#endif
					strncat(cmd, cfg.exec_dir, avail);
					break;
				case '@':   /* EXEC Directory for DOS/OS2/Win32, blank for Unix */
#ifndef __unix__
					strncat(cmd, cfg.exec_dir, avail);
#endif
					break;

				case '#':   /* Node number (same as SBBSNNUM environment var) */
					snprintf(str, sizeof str, "%d", cfg.node_num);
					strncat(cmd, str, avail);
					break;
				case '*':
					snprintf(str, sizeof str, "%03d", cfg.node_num);
					strncat(cmd, str, avail);
					break;
				case '$':   /* Credits */
					strncat(cmd, _ui64toa(user_available_credits(&useron), str, 10), avail);
					break;
				case '%':   /* %% for percent sign */
					strncat(cmd, "%", avail);
					break;
				case '.':   /* .exe for DOS/OS2/Win32, blank for Unix */
#ifndef __unix__
					strncat(cmd, ".exe", avail);
#endif
					break;
				case '?':   /* Platform */
#ifdef __OS2__
					strcpy(str, "OS2");
#else
					strcpy(str, PLATFORM_DESC);
#endif
					strlwr(str);
					strncat(cmd, str, avail);
					break;
				case '^':   /* Architecture */
					strncat(cmd, ARCHITECTURE_DESC, avail);
					break;
				case '+':   /* Real name */
					strncat(cmd, quoted_string(useron.name, str, sizeof(str)), avail);
					break;
				case '-':   /* Chat handle */
					strncat(cmd, quoted_string(useron.handle, str, sizeof(str)), avail);
					break;
				default:    /* unknown specification */
					if (IS_DIGIT(instr[i])) {
						snprintf(str, sizeof str, "%0*d", instr[i] & 0xf, useron.number);
						strncat(cmd, str, avail);
					}
					break;
			}
			j = strlen(cmd);
		}
		else
			cmd[j++] = instr[i];
	}
	cmd[j] = 0;

	return cmd;
}
