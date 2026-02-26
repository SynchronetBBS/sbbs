/* Execute a Synchronet JavaScript module from the command-line */

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

#ifndef JAVASCRIPT
#define JAVASCRIPT
#endif

#ifdef __unix__
#define _WITH_GETLINE
#include <signal.h>
#include <termios.h>
#endif

#define STARTUP_INI_JSOPT_BITDESC_TABLE
#include "sbbs.h"
#include "ciolib.h"
#if defined(main) && defined(_WIN32) && !defined(JSDOOR)
 #undef main    // Don't be a Windows program, be a Console one
#endif
#include "ini_file.h"
#include "js_rtpool.h"
#include "js_request.h"
#include "jsdebug.h"
#include "git_branch.h"
#include "git_hash.h"

#define DEFAULT_LOG_LEVEL   LOG_INFO
#define DEFAULT_ERR_LOG_LVL LOG_WARNING

static const char* strJavaScriptMaxBytes       = "JavaScriptMaxBytes";
static const char* strJavaScriptTimeLimit      = "JavaScriptTimeLimit";
static const char* strJavaScriptGcInterval     = "JavaScriptGcInterval";
static const char* strJavaScriptYieldInterval  = "JavaScriptYieldInterval";
static const char* strJavaScriptOptions        = "JavaScriptOptions";

js_startup_t       startup;
JSRuntime*         js_runtime;
JSContext*         js_cx;
JSObject*          js_glob;
js_callback_t      cb;
scfg_t             scfg;
char*              text[TOTAL_TEXT];
ulong              js_max_bytes = JAVASCRIPT_MAX_BYTES;
#ifndef JSDOOR
ulong              js_opts = JAVASCRIPT_OPTIONS;
#else
#ifdef __OpenBSD__
ulong              js_opts = JSOPTION_VAROBJFIX | JSOPTION_JIT | JSOPTION_COMPILE_N_GO | JSOPTION_PROFILING;
#else
ulong              js_opts = JSOPTION_VAROBJFIX | JSOPTION_JIT | JSOPTION_METHODJIT | JSOPTION_COMPILE_N_GO | JSOPTION_PROFILING;
#endif
#endif
FILE*              confp;
FILE*              errfp;
FILE*              nulfp;
FILE*              statfp;
char               compiler[32];
char*              host_name = NULL;
char               host_name_buf[128];
const char*        load_path_list = JAVASCRIPT_LOAD_PATH;
bool               pause_on_exit = false;
bool               pause_on_error = false;
bool               terminated = false;
bool               recycled;
bool               require_cfg = true;
int                log_level = DEFAULT_LOG_LEVEL;
int                err_level = DEFAULT_ERR_LOG_LVL;
long               umask_val = -1;
pthread_mutex_t    output_mutex;
#if defined(__unix__)
bool               daemonize = false;
struct termios     orig_term;
#endif
char               orig_cwd[MAX_PATH + 1];
bool               debugger = false;
#ifndef PROG_NAME
#define PROG_NAME "JSexec"
#endif
#ifndef PROG_NAME_LC
#define PROG_NAME_LC "jsexec"
#endif

void banner(FILE* fp)
{
	fprintf(fp, "\n" PROG_NAME " v%s%c-%s %s/%s%s - "
	        "Execute Synchronet JavaScript Module\n"
	        , VERSION, REVISION
	        , PLATFORM_DESC
	        , GIT_BRANCH, GIT_HASH
#ifdef _DEBUG
	        , " Debug"
#else
	        , ""
#endif
	        );

	fprintf(fp, "Compiled %s with %s\n"
	        , GIT_DATE, compiler);
}

void usage()
{
	banner(stdout);

	fprintf(stdout, "\nusage: " PROG_NAME_LC " [-opts] [[path/]module[.js] [args]\n"
	        "   or: " PROG_NAME_LC " [-opts] -r js-expression [args]\n"
	        "   or: " PROG_NAME_LC " -v\n"
	        "\navailable opts:\n\n"
	        "    -r<expression> run (compile and execute) JavaScript expression\n"
#ifdef JSDOOR
	        "    -c<ctrl_dir>   specify path to CTRL directory\n"
#else
	        "    -c<ctrl_dir>   specify path to Synchronet CTRL directory\n"
	        "    -U             do not require successful load of configuration files\n"
	        "    -C             do not change the current working directory (to CTRL dir)\n"
#endif
#if defined(__unix__)
	        "    -d             run in background (daemonize)\n"
#endif
	        "    -m<bytes>      set maximum heap size (default=%u bytes)\n"
	        "    -t<limit>      set time limit (default=%u, 0=unlimited)\n"
	        "    -y<interval>   set yield interval (default=%u, 0=never)\n"
	        "    -g<interval>   set garbage collection interval (default=%u, 0=never)\n"
#ifdef JSDOOR
	        "    -h[hostname]   use local or specified host name\n"
#else
	        "    -h[hostname]   use local or specified host name (instead of SCFG value)\n"
#endif
	        "    -u<mask>       set file creation permissions mask (in octal)\n"
	        "    -L<level>      set output log level by number or name (default=%u)\n"
	        "    -E<level>      set error log level threshold (default=%u)\n"
	        "    -i<path_list>  set load() comma-sep search path list (default=\"%s\")\n"
	        "    -f             use non-buffered stream for console messages\n"
	        "    -a             append instead of overwriting message output files\n"
	        "    -A             send all messages to stdout\n"
	        "    -A<filename>   send all messages to file instead of stdout/stderr\n"
	        "    -e<filename>   send error messages to file in addition to stderr\n"
	        "    -o<filename>   send console messages to file instead of stdout\n"
	        "    -S<filename>   send status messages to file instead of stderr\n"
	        "    -S             send status messages to stdout\n"
	        "    -n             send status messages to %s instead of stderr\n"
	        "    -q             send console messages to %s instead of stdout\n"
	        "    -v             display version details and exit\n"
	        "    -x             disable auto-termination on local abort signal\n"
	        "    -l             loop until intentionally terminated\n"
	        "    -p             wait for keypress (pause) on exit\n"
	        "    -!             wait for keypress (pause) on error\n"
	        "    -D             load the script into an interactive debugger\n"
	        , JAVASCRIPT_MAX_BYTES
	        , JAVASCRIPT_TIME_LIMIT
	        , JAVASCRIPT_YIELD_INTERVAL
	        , JAVASCRIPT_GC_INTERVAL
	        , DEFAULT_LOG_LEVEL
	        , DEFAULT_ERR_LOG_LVL
	        , load_path_list
	        , _PATH_DEVNULL
	        , _PATH_DEVNULL
	        );
}

static void
cooked_tty(void)
{
	if (isatty(STDIN_FILENO) && isatty(STDOUT_FILENO)) {
#ifdef __unix__
		tcsetattr(STDIN_FILENO, TCSANOW, &orig_term);
#elif defined _WIN32
		SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), ENABLE_ECHO_INPUT
		               | ENABLE_EXTENDED_FLAGS | ENABLE_INSERT_MODE | ENABLE_LINE_INPUT
		               | ENABLE_MOUSE_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_QUICK_EDIT_MODE);
#else
		#warning "Can't cook the tty on this platform"
#endif
	}
}

#ifdef __unix__
static void raw_input(struct termios *t)
{
#ifdef JSDOOR
	t->c_iflag &= ~(IMAXBEL | IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	t->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
#else
	t->c_iflag &= ~(IMAXBEL | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	t->c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN);
#endif
}
#endif

static void
raw_tty(void)
{
	if (isatty(STDIN_FILENO) && isatty(STDOUT_FILENO)) {
#ifdef __unix__
		struct termios term = orig_term;

		raw_input(&term);
		tcsetattr(STDIN_FILENO, TCSANOW, &term);
#elif defined _WIN32
		SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), 0);
#else
		#warning "Can't set the tty as raw on this platform"
#endif
	}
}

int mfprintf(FILE* fp, const char *fmt, ...)
{
	va_list argptr;
	char    sbuf[8192];
	int     ret = 0;

	va_start(argptr, fmt);
	ret = vsnprintf(sbuf, sizeof(sbuf), fmt, argptr);
	sbuf[sizeof(sbuf) - 1] = 0;
	va_end(argptr);

	/* Mutex-protect stdout/stderr */
	pthread_mutex_lock(&output_mutex);

	ret = fprintf(fp, "%s\n", sbuf);

	pthread_mutex_unlock(&output_mutex);
	return ret;
}

/* Log printf */
int lprintf(int level, const char *fmt, ...)
{
	va_list argptr;
	char    sbuf[8192];
	int     ret = 0;

	if (level > log_level)
		return 0;

	va_start(argptr, fmt);
	ret = vsnprintf(sbuf, sizeof(sbuf), fmt, argptr);
	sbuf[sizeof(sbuf) - 1] = 0;
	va_end(argptr);
#if defined(__unix__)
	if (daemonize) {
		syslog(level, "%s", sbuf);
		return ret;
	}
#endif

	/* Mutex-protect stdout/stderr */
	pthread_mutex_lock(&output_mutex);

	if (level <= err_level) {
		ret = fprintf(errfp, "%s\n", sbuf);
		if (errfp != stderr)
			ret = fprintf(statfp, "%s\n", sbuf);
	}
	if (level > err_level)
		ret = fprintf(statfp, "%s\n", sbuf);

	pthread_mutex_unlock(&output_mutex);
	return ret;
}

#if defined(__unix__) && defined(NEEDS_DAEMON)
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

#if defined(_WINSOCKAPI_)

WSADATA     WSAData;
#define SOCKLIB_DESC WSAData.szDescription
static bool WSAInitialized = false;

static bool winsock_startup(void)
{
	int status;                 /* Status Code */

	if ((status = WSAStartup(MAKEWORD(1, 1), &WSAData)) == 0) {
/*		fprintf(statfp,"%s %s\n",WSAData.szDescription, WSAData.szSystemStatus); */
		WSAInitialized = true;
		return true;
	}

	lprintf(LOG_CRIT, "!WinSock startup ERROR %d", status);
	return false;
}

#else /* No WINSOCK */

#define winsock_startup()   (true)
#define SOCKLIB_DESC NULL

#endif

static int do_bail(int code)
{
#if defined(_WINSOCKAPI_)
	if (WSAInitialized && WSACleanup() != 0)
		lprintf(LOG_ERR, "!WSACleanup ERROR %d", SOCKET_ERRNO);
#endif

	cooked_tty();
	if (pause_on_exit || (code && pause_on_error)) {
		fprintf(statfp, "\nHit enter to continue...");
		getchar();
	}

	if (code)
		fprintf(statfp, "\nReturning error code: %d\n", code);
	return code;
}

void bail(int code)
{
	exit(do_bail(code));
}

static JSBool
js_log(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	uintN      i = 0;
	int32      level = LOG_INFO;
	JSString*  str = NULL;
	jsrefcount rc;
	char *     logstr = NULL;
	size_t     logstr_sz = 0;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;

	if (argc > 1 && JSVAL_IS_NUMBER(argv[i])) {
		if (!JS_ValueToInt32(cx, argv[i++], &level))
			return JS_FALSE;
	}

	for (; i < argc; i++) {
		if ((str = JS_ValueToString(cx, argv[i])) == NULL)
			return JS_FALSE;
		JSSTRING_TO_RASTRING(cx, str, logstr, &logstr_sz, NULL);
		if (logstr == NULL)
			return JS_FALSE;
		rc = JS_SUSPENDREQUEST(cx);
		lprintf(level, "%s", logstr);
		JS_RESUMEREQUEST(cx, rc);
		FREE_AND_NULL(logstr);
	}

	if (str == NULL)
		JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	else
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(str));

	return JS_TRUE;
}

static JSBool
js_read(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      buf;
	int        rd;
	int32      len = 128;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if (argc) {
		if (js_argvIsNullOrVoid(cx, argv, 0))
			return JS_FALSE;
		if (!JS_ValueToInt32(cx, argv[0], &len))
			return JS_FALSE;
	}
	if ((buf = static_cast<char *>(malloc(len))) == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	rd = fread(buf, sizeof(char), len, stdin);
	JS_RESUMEREQUEST(cx, rc);

	if (rd > 0)
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(JS_NewStringCopyN(cx, buf, rd)));
	free(buf);

	return JS_TRUE;
}

static JSBool
js_readln(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      buf;
	char*      p;
	int32      len = 128;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if (argc) {
		if (js_argvIsNullOrVoid(cx, argv, 0))
			return JS_FALSE;
		if (!JS_ValueToInt32(cx, argv[0], &len))
			return JS_FALSE;
	}

	if (len > 0) {
		if ((buf = static_cast<char *>(malloc(len + 1))) == NULL)
			return JS_TRUE;

		rc = JS_SUSPENDREQUEST(cx);
		cooked_tty();
		p = fgets(buf, len + 1, stdin);
		raw_tty();
		JS_RESUMEREQUEST(cx, rc);

		if (p != NULL)
			JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, truncnl(p))));
		free(buf);
	}
	return JS_TRUE;
}


static JSBool
js_write(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	uintN      i;
	JSString*  str = NULL;
	jsrefcount rc;
	char *     line = NULL;
	size_t     line_sz = 0;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	for (i = 0; i < argc; i++) {
		if ((str = JS_ValueToString(cx, argv[i])) == NULL)
			return JS_FALSE;
		JSSTRING_TO_RASTRING(cx, str, line, &line_sz, NULL);
		if (line == NULL)
			return JS_FALSE;
		rc = JS_SUSPENDREQUEST(cx);
		fprintf(confp, "%s", line);
		FREE_AND_NULL(line);
		JS_RESUMEREQUEST(cx, rc);
	}

	return JS_TRUE;
}

static JSBool
js_writeln(JSContext *cx, uintN argc, jsval *arglist)
{
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if (!js_write(cx, argc, arglist))
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	fprintf(confp, "\n");
	JS_RESUMEREQUEST(cx, rc);
	return JS_TRUE;
}

static JSBool
jse_printf(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      p;
	jsrefcount rc;

	if (argc < 1) {
		JS_SET_RVAL(cx, arglist, JS_GetEmptyStringValue(cx));
		return JS_TRUE;
	}

	if ((p = js_sprintf(cx, 0, argc, argv)) == NULL) {
		JS_ReportError(cx, "js_sprintf failed");
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	fprintf(confp, "%s", p);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, p)));

	js_sprintf_free(p);

	return JS_TRUE;
}

static JSBool
js_alert(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	jsrefcount rc;
	char *     line;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	JSVALUE_TO_MSTRING(cx, argv[0], line, NULL);
	if (line == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	fprintf(confp, "!%s\n", line);
	free(line);
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_confirm(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	JSString * str;
	char *     cstr = NULL;
	char *     p;
	jsrefcount rc;
	char       instr[81] = "y";

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	if ((str = JS_ValueToString(cx, argv[0])) == NULL)
		return JS_FALSE;

	JSSTRING_TO_MSTRING(cx, str, cstr, NULL);
	HANDLE_PENDING(cx, cstr);
	if (cstr == NULL)
		return JS_TRUE;
	rc = JS_SUSPENDREQUEST(cx);
	printf("%s (Y/n)? ", cstr);
	free(cstr);
	cooked_tty();
	p = fgets(instr, sizeof(instr), stdin);
	raw_tty();
	JS_RESUMEREQUEST(cx, rc);

	if (p != NULL) {
		SKIP_WHITESPACE(p);
		JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(tolower(*p) != 'n'));
	}
	return JS_TRUE;
}

static JSBool
js_deny(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	JSString * str;
	char *     cstr = NULL;
	char *     p;
	jsrefcount rc;
	char       instr[81];

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	if ((str = JS_ValueToString(cx, argv[0])) == NULL)
		return JS_FALSE;

	JSSTRING_TO_MSTRING(cx, str, cstr, NULL);
	HANDLE_PENDING(cx, cstr);
	if (cstr == NULL)
		return JS_TRUE;
	rc = JS_SUSPENDREQUEST(cx);
	printf("%s (N/y)? ", cstr);
	free(cstr);
	cooked_tty();
	p = fgets(instr, sizeof(instr), stdin);
	raw_tty();
	JS_RESUMEREQUEST(cx, rc);

	if (p != NULL) {
		SKIP_WHITESPACE(p);
		JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(tolower(*p) != 'y'));
	}
	return JS_TRUE;
}

static JSBool
js_prompt(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char       instr[256];
	JSString * str;
	jsrefcount rc;
	char *     prstr = NULL;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	if (argc > 0 && !JSVAL_IS_VOID(argv[0])) {
		JSVALUE_TO_MSTRING(cx, argv[0], prstr, NULL);
		HANDLE_PENDING(cx, prstr);
		if (prstr == NULL)
			return JS_FALSE;
		rc = JS_SUSPENDREQUEST(cx);
		fprintf(confp, "%s: ", prstr);
		free(prstr);
		JS_RESUMEREQUEST(cx, rc);
	}

	if (argc > 1) {
		JSVALUE_TO_STRBUF(cx, argv[1], instr, sizeof(instr), NULL);
		HANDLE_PENDING(cx, NULL);
	} else
		instr[0] = 0;

	rc = JS_SUSPENDREQUEST(cx);
	cooked_tty();
	if (!fgets(instr, sizeof(instr), stdin)) {
		raw_tty();
		JS_RESUMEREQUEST(cx, rc);
		return JS_TRUE;
	}
	raw_tty();
	JS_RESUMEREQUEST(cx, rc);

	if ((str = JS_NewStringCopyZ(cx, truncnl(instr))) == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(str));
	return JS_TRUE;
}

static JSBool
js_chdir(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      p = NULL;
	jsrefcount rc;
	bool       ret;

	JSVALUE_TO_MSTRING(cx, argv[0], p, NULL);
	HANDLE_PENDING(cx, p);
	if (p == NULL) {
		JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(-1));
		return JS_TRUE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	ret = chdir(p) == 0;
	free(p);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));
	return JS_TRUE;
}

static JSBool
js_putenv(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      p = NULL;
	bool       ret;
	jsrefcount rc;

	if (argc) {
		JSVALUE_TO_MSTRING(cx, argv[0], p, NULL);
		HANDLE_PENDING(cx, p);
	}
	if (p == NULL) {
		JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(-1));
		return JS_TRUE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	ret = putenv(strdup(p)) == 0;
	free(p);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));
	return JS_TRUE;
}

// Forked from mozilla/js/src/shell/js.cpp: AssertEq()
static JSBool
js_AssertEq(JSContext *cx, uintN argc, jsval *vp)
{
	if (!(argc == 2 || (argc == 3 && JSVAL_IS_STRING(JS_ARGV(cx, vp)[2])))) {
		JS_ReportError(cx, "assertEq: missing or invalid args");
		return JS_FALSE;
	}

	jsval *argv = JS_ARGV(cx, vp);
	JSBool same;
	if (!JS_SameValue(cx, argv[0], argv[1], &same))
		return JS_FALSE;
	if (!same) {
		JS_ReportError(cx, "not equal");
		return JS_FALSE;
	}
	JS_SET_RVAL(cx, vp, JSVAL_VOID);
	return JS_TRUE;
}


static jsSyncMethodSpec js_global_functions[] = {
	{"log",             js_log,             1},
	{"read",            js_read,            1},
	{"readln",          js_readln,          0,  JSTYPE_STRING,  JSDOCSTR("[count]")
	 , JSDOCSTR("Read a single line, up to count characters, from input stream")
	 , 311},
	{"write",           js_write,           0},
	{"write_raw",       js_write,           0}, // alias for compatibility with sbbs
	{"writeln",         js_writeln,         0},
	{"print",           js_writeln,         0},
	{"printf",          jse_printf,         1},
	{"alert",           js_alert,           1},
	{"prompt",          js_prompt,          1},
	{"confirm",         js_confirm,         1},
	{"deny",            js_deny,            1},
	{"chdir",           js_chdir,           1},
	{"putenv",          js_putenv,          1},
	{"assertEq",        js_AssertEq,        2},
	{0}
};

static void
js_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
	char        line[64];
	char        file[MAX_PATH + 1];
	const char* warning;
	jsrefcount  rc;
	int         log_level;

	rc = JS_SUSPENDREQUEST(cx);
	if (report == NULL) {
		lprintf(LOG_ERR, "!JavaScript: %s", message);
		JS_RESUMEREQUEST(cx, rc);
		return;
	}

	if (report->filename)
		sprintf(file, " %s", report->filename);
	else
		file[0] = 0;

	if (report->lineno)
		sprintf(line, " line %d", report->lineno);
	else
		line[0] = 0;

	if (JSREPORT_IS_WARNING(report->flags)) {
		if (JSREPORT_IS_STRICT(report->flags))
			warning = "strict warning";
		else
			warning = "warning";
		log_level = LOG_WARNING;
	} else {
		log_level = LOG_ERR;
		warning = "";
	}

	lprintf(log_level, "!JavaScript %s%s%s: %s", warning, file, line, message);
	JS_RESUMEREQUEST(cx, rc);
}

static JSBool
js_OperationCallback(JSContext *cx)
{
	JSBool ret;

	JS_SetOperationCallback(cx, NULL);
	ret = js_CommonOperationCallback(cx, &cb);
	JS_SetOperationCallback(cx, js_OperationCallback);
	return ret;
}

static bool js_CreateEnvObject(JSContext* cx, JSObject* glob, char** env)
{
	char      name[256];
	const char* val;
	uint      i;
	JSObject* js_env;

	if ((js_env = JS_NewObject(js_cx, NULL, NULL, glob)) == NULL)
		return false;

	if (!JS_DefineProperty(cx, glob, "env", OBJECT_TO_JSVAL(js_env)
	                       , NULL, NULL, JSPROP_READONLY | JSPROP_ENUMERATE))
		return false;

	if (env) {
		for (i = 0; env[i] != NULL; i++) {
			SAFECOPY(name, env[i]);
			truncstr(name, "=");
			val = strchr(env[i], '=');
			if (val == NULL)
				val = "";
			else
				val++;
			if (!JS_DefineProperty(cx, js_env, name, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, val))
			                       , NULL, NULL, JSPROP_READONLY | JSPROP_ENUMERATE))
				return false;
		}
	}

	return true;
}

static bool js_init(char** env)
{
	memset(&startup, 0, sizeof(startup));
	SAFECOPY(startup.load_path, load_path_list);

	if ((js_cx = JS_NewContext(js_runtime, JAVASCRIPT_CONTEXT_STACK)) == NULL)
		return false;
	JS_SetOptions(js_cx, js_opts);
	JS_BEGINREQUEST(js_cx);

	JS_SetErrorReporter(js_cx, js_ErrorReporter);

	/* Global Object */
	if (!js_CreateCommonObjects(js_cx, &scfg, NULL, js_global_functions
	                            , time(NULL), host_name, SOCKLIB_DESC /* system */
	                            , &cb, &startup /* js */
	                            , NULL /* user */
	                            , NULL, INVALID_SOCKET, -1 /* client */
	                            , NULL          /* server */
	                            , &js_glob
	                            , (struct mqtt*)NULL
	                            )) {
		JS_ENDREQUEST(js_cx);
		return false;
	}

	/* Environment Object (associative array) */
	if (!js_CreateEnvObject(js_cx, js_glob, env)) {
		JS_ENDREQUEST(js_cx);
		return false;
	}

	if (js_CreateUifcObject(js_cx, js_glob) == NULL) {
		JS_ENDREQUEST(js_cx);
		return false;
	}

	if (js_CreateConioObject(js_cx, js_glob) == NULL) {
		JS_ENDREQUEST(js_cx);
		return false;
	}

	/* STDIO objects */
	if (!js_CreateFileObject(js_cx, js_glob, "stdout", STDOUT_FILENO, "w")) {
		JS_ENDREQUEST(js_cx);
		return false;
	}

	if (!js_CreateFileObject(js_cx, js_glob, "stdin", STDIN_FILENO, "r")) {
		JS_ENDREQUEST(js_cx);
		return false;
	}

	if (!js_CreateFileObject(js_cx, js_glob, "stderr", STDERR_FILENO, "w")) {
		JS_ENDREQUEST(js_cx);
		return false;
	}

	return true;
}

static const char* js_ext(const char* fname)
{
	if (strchr(fname, '.') == NULL)
		return ".js";
	return "";
}

void dbg_puts(const char *msg)
{
	fputs(msg, stderr);
}

char *dbg_getline(void)
{
#ifdef __unix__
	char * line = NULL;
	size_t linecap = 0;

	cooked_tty();
	if (getline(&line, &linecap, stdin) >= 0) {
		raw_tty();
		return line;
	}
	raw_tty();
	if (line)
		free(line);
	return NULL;
#else
	// I assume Windows sucks.
	char buf[1025];

	cooked_tty();
	if (fgets(buf, sizeof(buf), stdin)) {
		raw_tty();
		return strdup(buf);
	}
	raw_tty();
	return NULL;
#endif
}

long js_exec(const char *fname, const char* buf, char** args)
{
	int         argc = 0;
	uint        line_no;
	char        path[MAX_PATH + 1];
	char        line[1024];
	char        rev_detail[256];
	size_t      len;
	char*       js_buf = NULL;
	size_t      js_buflen;
	JSObject*   js_script = NULL;
	JSString*   arg;
	JSObject*   argv;
	FILE*       fp = stdin;
	jsval       val;
	jsval       rval = JSVAL_VOID;
	int32       result = 0;
	long double start;
	long double diff;
	JSBool      exec_result;
	bool        abort = false;

	if (fname != NULL) {
		if (isfullpath(fname)) {
			SAFECOPY(path, fname);
		}
		else {
			SAFEPRINTF3(path, "%s%s%s", orig_cwd, fname, js_ext(fname));
			if (!fexistcase(path)) {
				SAFEPRINTF3(path, "%s%s%s", scfg.mods_dir, fname, js_ext(fname));
				if (scfg.mods_dir[0] == 0 || !fexistcase(path))
					SAFEPRINTF3(path, "%s%s%s", scfg.exec_dir, fname, js_ext(fname));
			}
		}

		if (!fexistcase(path)) {
			lprintf(LOG_ERR, "!Module file (%s) doesn't exist", path);
			return -1;
		}

		if ((fp = fopen(path, "r")) == NULL) {
			lprintf(LOG_ERR, "!Error %d (%s) opening %s", errno, strerror(errno), path);
			return -1;
		}
	}
	JS_ClearPendingException(js_cx);

	argv = JS_NewArrayObject(js_cx, 0, NULL);
	JS_DefineProperty(js_cx, js_glob, "argv", OBJECT_TO_JSVAL(argv)
	                  , NULL, NULL, JSPROP_READONLY | JSPROP_ENUMERATE);

	for (argc = 0; args[argc]; argc++) {
		arg = JS_NewStringCopyZ(js_cx, args[argc]);
		if (arg == NULL)
			break;
		val = STRING_TO_JSVAL(arg);
		if (!JS_SetElement(js_cx, argv, argc, &val))
			break;
	}
	JS_DefineProperty(js_cx, js_glob, "argc", INT_TO_JSVAL(argc)
	                  , NULL, NULL, JSPROP_READONLY | JSPROP_ENUMERATE);

	JS_DefineProperty(js_cx, js_glob, PROG_NAME_LC "_revision"
	                  , STRING_TO_JSVAL(JS_NewStringCopyZ(js_cx, GIT_BRANCH "/" GIT_HASH))
	                  , NULL, NULL, JSPROP_READONLY | JSPROP_ENUMERATE);

	safe_snprintf(rev_detail, sizeof(rev_detail), PROG_NAME " " GIT_BRANCH "/" GIT_HASH "%s  "
	              "Compiled %s with %s"
#ifdef _DEBUG
	              , " Debug"
#else
	              , ""
#endif
	              , GIT_DATE, compiler
	              );

	JS_DefineProperty(js_cx, js_glob, PROG_NAME_LC "_revision_detail"
	                  , STRING_TO_JSVAL(JS_NewStringCopyZ(js_cx, rev_detail))
	                  , NULL, NULL, JSPROP_READONLY | JSPROP_ENUMERATE);

	cb.terminated = &terminated;

	JS_SetOperationCallback(js_cx, js_OperationCallback);

	if (fname == NULL && buf != NULL) {
		SAFECOPY(path, "cmdline");
		fp = NULL;
		js_buf = (char*)buf;
		js_buflen = strlen(buf);
	} else {
		if (fp == stdin)    /* Using stdin for script source */
			SAFECOPY(path, "stdin");

		fprintf(statfp, "Reading script from %s\n", path);
		line_no = 0;
		js_buflen = 0;
		while (!feof(fp)) {
			if (!fgets(line, sizeof(line), fp))
				break;
			line_no++;
			/* Support Unix Shell Scripts that start with #!/path/to/jsexec */
			if (line_no == 1 && strncmp(line, "#!", 2) == 0)
				strcpy(line, "\n"); /* To keep line count correct */
			len = strlen(line);
			if ((js_buf = static_cast<char *>(realloc_or_free(js_buf, js_buflen + len))) == NULL) {
				lprintf(LOG_ERR, "!Error allocating %lu bytes of memory"
				        , (ulong)(js_buflen + len));
				if (fp != stdin)
					fclose(fp);
				return -1;
			}
			memcpy(js_buf + js_buflen, line, len);
			js_buflen += len;
		}
		if (fp != stdin)
			fclose(fp);
	}
	start = xp_timer();
	if (debugger)
		init_debugger(js_runtime, js_cx, dbg_puts, dbg_getline);
	if ((js_script = JS_CompileScript(js_cx, js_glob, js_buf, js_buflen, fname == NULL ? NULL : path, 1)) == NULL) {
		lprintf(LOG_ERR, "!Error compiling script from %s", path);
		if (js_buf != buf)
			free(js_buf);
		return -1;
	}
	if ((diff = xp_timer() - start) > 0)
		mfprintf(statfp, "%s compiled in %.2Lf seconds"
		         , path
		         , diff);

	js_PrepareToExecute(js_cx, js_glob, fname == NULL ? NULL : path, orig_cwd, js_glob);
	start = xp_timer();
	if (debugger)
		abort = debug_prompt(js_cx, js_script) == DEBUG_EXIT;
	if (abort) {
		result = EXIT_FAILURE;
	} else {
		cb.keepGoing = false;
		cb.events_supported = true;
		exec_result = JS_ExecuteScript(js_cx, js_glob, js_script, &rval);
		js_handle_events(js_cx, &cb, &terminated);
		fflush(confp);

		char *p;
		if (buf != NULL) {
			JSVALUE_TO_MSTRING(js_cx, rval, p, NULL);
			mfprintf(statfp, "Result (%s): %s", JS_GetTypeName(js_cx, JS_TypeOfValue(js_cx, rval)), p);
			free(p);
		}
		JS_GetProperty(js_cx, js_glob, "exit_code", &rval);
		if (rval != JSVAL_VOID && JSVAL_IS_NUMBER(rval)) {
			JSVALUE_TO_MSTRING(js_cx, rval, p, NULL);
			mfprintf(statfp, "Using JavaScript exit_code: %s", p);
			free(p);
			JS_ValueToInt32(js_cx, rval, &result);
		} else if (!exec_result)
			result = EXIT_FAILURE;
		js_EvalOnExit(js_cx, js_glob, &cb);

		JS_ReportPendingException(js_cx);
	}

	if ((diff = xp_timer() - start) > 0)
		mfprintf(statfp, "%s executed in %.2Lf seconds"
		         , path
		         , diff);

	if (js_buf != NULL && js_buf != buf)
		free(js_buf);

	return result;
}

void break_handler(int type)
{
	lprintf(LOG_NOTICE, "\n-> Terminated Locally (signal: %d)", type);
	terminated = true;
}

void recycle_handler(int type)
{
	lprintf(LOG_NOTICE, "\n-> Recycled Locally (signal: %d)", type);
	recycled = true;
	cb.terminated = &recycled;
}


#if defined(_WIN32)
BOOL WINAPI ControlHandler(unsigned long CtrlType)
{
	break_handler((int)CtrlType);
	return true;
}
#endif

int parseLogLevel(const char* p)
{
	str_list_t logLevelStringList = iniLogLevelStringList();
	int        i;

	if (IS_DIGIT(*p))
		return strtol(p, NULL, 0);

	/* Exact match */
	for (i = 0; logLevelStringList[i] != NULL; i++) {
		if (stricmp(logLevelStringList[i], p) == 0)
			return i;
	}
	/* Partial match */
	for (i = 0; logLevelStringList[i] != NULL; i++) {
		if (strnicmp(logLevelStringList[i], p, strlen(p)) == 0)
			return i;
	}
	return DEFAULT_LOG_LEVEL;
}

#ifndef JSDOOR
void get_ini_values(str_list_t ini, const char* section, js_callback_t* cb)
{
	log_level = iniGetLogLevel(ini, section, "LogLevel", log_level);
	err_level = iniGetLogLevel(ini, section, "ErrorLevel", err_level);
	debugger = iniGetBool(ini, section, "Debugger", debugger);
	pause_on_exit = iniGetBool(ini, section, "PauseOnExit", pause_on_exit);
	pause_on_error = iniGetBool(ini, section, "PauseOnError", pause_on_error);
	umask_val = iniGetInteger(ini, section, "umask", umask_val);

	js_max_bytes        = (ulong)iniGetBytes(ini, section, strJavaScriptMaxBytes, /* unit: */ 1, js_max_bytes);
	cb->limit           = iniGetInteger(ini, section, strJavaScriptTimeLimit, cb->limit);
	cb->gc_interval     = iniGetInteger(ini, section, strJavaScriptGcInterval, cb->gc_interval);
	cb->yield_interval  = iniGetInteger(ini, section, strJavaScriptYieldInterval, cb->yield_interval);
	cb->auto_terminate  = iniGetBool(ini, section, "AutoTerminate", cb->auto_terminate);
	js_opts             = iniGetBitField(ini, section, strJavaScriptOptions, js_options, js_opts);
}
#endif

/*********************/
/* Entry point (duh) */
/*********************/
extern char **environ;
extern "C" int main(int argc, char **argv)
{
#ifndef JSDOOR
	char             error[512];
#endif
	char*            module = NULL;
	char*            js_buf = NULL;
	char*            p;
	const char*      omode = "w";
	int              argn;
	long             result;
	ulong            exec_count = 0;
	bool             loop = false;
	bool             nonbuffered_con = false;
#ifndef JSDOOR
	bool             change_cwd = true;
	FILE*            fp;
	char             ini_fname[MAX_PATH + 1];
#endif
	str_list_t       ini = NULL;
#ifdef __unix__
	struct sigaction sa {};
#endif

	confp = stdout;
	errfp = stderr;
	if ((nulfp = fopen(_PATH_DEVNULL, "w+")) == NULL) {
		perror(_PATH_DEVNULL);
		return do_bail(-1);
	}
	if (isatty(STDIN_FILENO)) {
#ifdef __unix__
		tcgetattr(STDIN_FILENO, &orig_term);
#endif
		raw_tty();
		statfp = stderr;
	}
	else    /* if redirected, don't send status messages to stderr */
		statfp = nulfp;

	cb.limit = JAVASCRIPT_TIME_LIMIT;
	cb.yield_interval = JAVASCRIPT_YIELD_INTERVAL;
	cb.gc_interval = JAVASCRIPT_GC_INTERVAL;
	cb.auto_terminate = true;
	cb.events = NULL;

	DESCRIBE_COMPILER(compiler);

	memset(&scfg, 0, sizeof(scfg));
	scfg.size = sizeof(scfg);

	if (!winsock_startup())
		return do_bail(2);

#ifndef JSDOOR
	SAFECOPY(scfg.ctrl_dir, get_ctrl_dir(/* warn: */ false));
	iniFileName(ini_fname, sizeof(ini_fname), scfg.ctrl_dir, "jsexec.ini");
	if ((fp = iniOpenFile(ini_fname, /* for_modify: */ false)) != NULL) {
		ini = iniReadFile(fp);
		iniCloseFile(fp);
	} else if (fexist(ini_fname)) {
		fprintf(stderr, "Error %d (%s) opening %s\n", errno, strerror(errno), ini_fname);
	}
	get_ini_values(ini, /* section (global): */ NULL, &cb);
#endif

	if (getcwd(orig_cwd, sizeof(orig_cwd)) == NULL) {
		fprintf(stderr, "Error %d (%s) getting cwd\n", errno, strerror(errno));
		return do_bail(1);
	}
	backslash(orig_cwd);
#ifdef JSDOOR
	SAFECOPY(scfg.ctrl_dir, orig_cwd);
	prep_dir("", scfg.ctrl_dir, sizeof(scfg.ctrl_dir));
	SAFECOPY(scfg.exec_dir, orig_cwd);
	prep_dir(scfg.ctrl_dir, scfg.exec_dir, sizeof(scfg.exec_dir));
	SAFECOPY(scfg.mods_dir, orig_cwd);
	prep_dir(scfg.ctrl_dir, scfg.mods_dir, sizeof(scfg.mods_dir));
	SAFECOPY(scfg.data_dir, orig_cwd);
	prep_dir(scfg.ctrl_dir, scfg.data_dir, sizeof(scfg.data_dir));
	SAFECOPY(scfg.text_dir, orig_cwd);
	prep_dir(scfg.ctrl_dir, scfg.text_dir, sizeof(scfg.text_dir));
	SAFECOPY(scfg.logs_dir, orig_cwd);
	prep_dir(scfg.ctrl_dir, scfg.text_dir, sizeof(scfg.text_dir));
	scfg.sys_misc = 0; /* SM_EURODATE and SM_MILITARY are used */
	gethostname(host_name = host_name_buf, sizeof(host_name_buf));
	statfp = nulfp;
	errfp = fopen("error.log", "a");
#endif

	for (argn = 1; argn < argc && module == NULL && js_buf == NULL; argn++) {
		if (argv[argn][0] == '-') {
			p = argv[argn] + 2;
			switch (argv[argn][1]) {
				/* These options require a parameter value */
				case 'c':
				case 'e':
				case 'E':
				case 'g':
				case 'i':
				case 'r':
				case 'L':
				case 'm':
				case 'o':
				case 's':
				case 't':
				case 'u':
				case 'y':
				{
					char opt = argv[argn][1];
					if (*p == 0) {
						if (argn + 1 >= argc) {
							fprintf(stderr, "\n!Option requires a parameter value: %s\n", argv[argn]);
							usage();
							return do_bail(1);
						}
						p = argv[++argn];
					}
					switch (opt) {
						case 'c':
							SAFECOPY(scfg.ctrl_dir, p);
							break;
						case 'E':
							err_level = parseLogLevel(p);
							break;
						case 'e':
							if (errfp != NULL && errfp != stderr)
								fclose(errfp);
							if ((errfp = fopen(p, omode)) == NULL) {
								perror(p);
								return do_bail(1);
							}
							break;
						case 'g':
							cb.gc_interval = strtoul(p, NULL, 0);
							break;
						case 'i':
							load_path_list = p;
							break;
						case 'r':
							js_buf = p;
							break;
						case 'L':
							log_level = parseLogLevel(p);
							break;
						case 'm':
							js_max_bytes = (ulong)parse_byte_count(p, /* units: */ 1);
							break;
						case 'o':
							if ((confp = fopen(p, omode)) == NULL) {
								perror(p);
								return do_bail(1);
							}
							break;
						case 't':
							cb.limit = strtoul(p, NULL, 0);
							break;
						case 'u':
							umask_val = strtol(p, NULL, 8);
							break;
						case 'y':
							cb.yield_interval = strtoul(p, NULL, 0);
							break;
					}
					break;
				}
				case 'a':
					omode = "a";
					break;
				case 'A':
					if (errfp != NULL && errfp != stderr)
						fclose(errfp);
					if (*p == '\0') {
						errfp = stdout;
						confp = stdout;
						statfp = stdout;
					} else {
						if ((errfp = fopen(p, omode)) == NULL) {
							perror(p);
							return do_bail(1);
						}
						statfp = errfp;
						confp = errfp;
					}
					break;
#ifndef JSDOOR
				case 'C':
					change_cwd = false;
					break;
#endif
#if defined(__unix__)
				case 'd':
					daemonize = true;
					break;
#endif
				case 'D':
					debugger = true;
					break;
				case 'f':
					nonbuffered_con = true;
					break;
				case 'h':
					if (*p == 0)
						gethostname(host_name = host_name_buf, sizeof(host_name_buf));
					else
						host_name = p;
					break;
				case 'l':
					loop = true;
					break;
				case 'S':
					if (*p == '\0')
						statfp = stdout;
					else {
						if ((statfp = fopen(p, omode)) == NULL) {
							perror(p);
							return do_bail(1);
						}
					}
					break;
				case 'n':
					statfp = nulfp;
					break;
				case 'p':
					pause_on_exit = true;
					break;
				case 'q':
					confp = nulfp;
					break;
				case 'R':
					require_cfg = true;
					break;
				case 'U':
					require_cfg = false;
					break;
				case 'v':
					banner(statfp);
					fprintf(statfp, "%s\n", (char *)JS_GetImplementationVersion());
					return do_bail(0);
				case 'x':
					cb.auto_terminate = false;
					break;
				case '!':
					pause_on_error = true;
					break;
				default:
					fprintf(stderr, "\n!Unsupported option: %s\n", argv[argn]);
				// fall-through
				case '?':
					usage();
					iniFreeStringList(ini);
					return do_bail(1);
			}
			continue;
		}
		module = argv[argn];
#ifndef JSDOOR
		char ini_section[MAX_PATH + 1];
		SAFECOPY(ini_section, module);
		if (!iniSectionExists(ini, ini_section)) {
			SAFECOPY(ini_section, getfname(module));
			if ((p = getfext(ini_section)) != NULL)
				*p = 0;
		}
		get_ini_values(ini, ini_section, &cb);
#endif
	}

	if (umask_val >= 0)
		umask(umask_val);

	if (module == NULL && js_buf == NULL && isatty(STDIN_FILENO)) {
		fprintf(stderr, "\n!No JavaScript module-name or expression specified\n");
		usage();
		iniFreeStringList(ini);
		return do_bail(1);
	}

	banner(statfp);

#ifdef JSDOOR
	SAFECOPY(scfg.temp_dir, "./temp");
	strcpy(scfg.sys_pass, "ThisIsNotHowToDoSecurity");
	strcpy(scfg.sys_name, "JSDoor");
	strcpy(scfg.sys_inetaddr, "example.com");
	scfg.prepped = true;
#else
	char relpath[MAX_PATH + 1];
	SAFECOPY(relpath, scfg.ctrl_dir);
	FULLPATH(scfg.ctrl_dir, relpath, sizeof scfg.ctrl_dir);
	if (change_cwd && chdir(scfg.ctrl_dir) != 0)
		fprintf(errfp, "!ERROR changing directory to: %s\n", scfg.ctrl_dir);

	fprintf(statfp, "\nLoading configuration files from %s\n", scfg.ctrl_dir);
	if (!load_cfg(&scfg, text, TOTAL_TEXT, /* prep: */ true, require_cfg, error, sizeof(error))) {
		fprintf(errfp, "!ERROR loading configuration files: %s\n", error);
		return do_bail(1);
	} else if (error[0] != '\0' && require_cfg)
		fprintf(errfp, "!WARNING loading configuration files: %s\n", error);

	SAFECOPY(scfg.temp_dir, "../temp");
#endif
	prep_dir(scfg.ctrl_dir, scfg.temp_dir, sizeof(scfg.temp_dir));

	if (host_name == NULL)
		host_name = scfg.sys_inetaddr;

#if defined(__unix__)
	if (daemonize) {
		fprintf(statfp, "\nRunning as daemon\n");
		cooked_tty();
		if (daemon(true, false))  { /* Daemonize, DON'T switch to / and DO close descriptors */
			fprintf(statfp, "!ERROR %d (%s) running as daemon\n", errno, strerror(errno));
			daemonize = false;
		}
	}
#endif

	/* Don't cache error log */
	setvbuf(errfp, NULL, _IONBF, 0);

	if (nonbuffered_con)
		setvbuf(confp, NULL, _IONBF, 0);

	/* Seed random number generator */
	sbbs_srand();

	/* Install Ctrl-C/Break signal handler here */
#if defined(_WIN32)
	SetConsoleCtrlHandler(ControlHandler, true /* Add */);
#elif defined(__unix__)
	sa.sa_handler = break_handler;
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	signal(SIGHUP, recycle_handler);

	/* Don't die on SIGPIPE  */
	signal(SIGPIPE, SIG_IGN);
#endif

	pthread_mutex_init(&output_mutex, NULL);

	setup_debugger();

	fprintf(statfp, "%s\n", (char *)JS_GetImplementationVersion());

	fprintf(statfp, "JavaScript: Creating runtime: %lu bytes\n"
	        , js_max_bytes);

	if ((js_runtime = jsrt_GetNew(js_max_bytes, 5000, __FILE__, __LINE__)) == NULL) {
		lprintf(LOG_ERR, "!JavaScript runtime creation failure");
		return do_bail(1);
	}
	do {

		if (exec_count++)
			lprintf(LOG_INFO, "\nRe-running: %s", module);

		recycled = false;

		if (!js_init(environ)) {
			lprintf(LOG_ERR, "!JavaScript initialization failure");
			return do_bail(1);
		}
		fprintf(statfp, "\n");

		result = js_exec(module, js_buf, &argv[argn]);
		JS_RemoveObjectRoot(js_cx, &js_glob);
		JS_ENDREQUEST(js_cx);
		YIELD();

		if (result)
			lprintf(LOG_ERR, "!Module (%s) set exit_code: %ld", module == NULL ? "cmdline" : module, result);

		fprintf(statfp, "\n");
		fprintf(statfp, "JavaScript: Destroying context\n");
		JS_DestroyContext(js_cx);

	} while ((recycled || loop) && !terminated);

	fprintf(statfp, "JavaScript: Destroying runtime\n");
	jsrt_Release(js_runtime);

	iniFreeStringList(ini);
	return do_bail(result);
}

