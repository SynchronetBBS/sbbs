/* jsexec.c */

/* Execute a Synchronet JavaScript module from the command-line */

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

#ifndef JAVASCRIPT
#define JAVASCRIPT
#endif

#ifdef __unix__
#define _WITH_GETLINE
#include <signal.h>
#include <termios.h>
#endif

#include "sbbs.h"
#include "ciolib.h"
#include "ini_file.h"
#include "js_rtpool.h"
#include "js_request.h"
#include "jsdebug.h"

#define DEFAULT_LOG_LEVEL	LOG_DEBUG	/* Display all LOG levels */
#define DEFAULT_ERR_LOG_LVL	LOG_WARNING

js_startup_t	startup;
JSRuntime*	js_runtime;
JSContext*	js_cx;
JSObject*	js_glob;
js_callback_t	cb;
scfg_t		scfg;
ulong		js_max_bytes=JAVASCRIPT_MAX_BYTES;
ulong		js_cx_stack=JAVASCRIPT_CONTEXT_STACK;
FILE*		confp;
FILE*		errfp;
FILE*		nulfp;
FILE*		statfp;
char		revision[16];
char		compiler[32];
char*		host_name=NULL;
char		host_name_buf[128];
char*		load_path_list=JAVASCRIPT_LOAD_PATH;
BOOL		pause_on_exit=FALSE;
BOOL		pause_on_error=FALSE;
BOOL		terminated=FALSE;
BOOL		recycled;
int			log_level=DEFAULT_LOG_LEVEL;
int  		err_level=DEFAULT_ERR_LOG_LVL;
pthread_mutex_t output_mutex;
#if defined(__unix__)
BOOL		daemonize=FALSE;
#endif
char		orig_cwd[MAX_PATH+1];
BOOL		debugger=FALSE;
#ifndef PROG_NAME
#define PROG_NAME "JSexec"
#endif
#ifndef PROG_NAME_LC
#define PROG_NAME_LC "jsexec"
#endif

void banner(FILE* fp)
{
	fprintf(fp,"\n" PROG_NAME " v%s%c-%s (rev %s)%s - "
		"Execute Synchronet JavaScript Module\n"
		,VERSION,REVISION
		,PLATFORM_DESC
		,revision
#ifdef _DEBUG
		," Debug"
#else
		,""
#endif
		);

	fprintf(fp, "Compiled %s %s with %s\n"
		,__DATE__, __TIME__, compiler);
}

void usage(FILE* fp)
{
	banner(fp);

	fprintf(fp,"\nusage: " PROG_NAME_LC " [-opts] [path]module[.js] [args]\n"
		"\navailable opts:\n\n"
#ifdef JSDOOR
		"\t-c<ctrl_dir>   specify path to CTRL directory\n"
#else
		"\t-c<ctrl_dir>   specify path to Synchronet CTRL directory\n"
#endif
#if defined(__unix__)
		"\t-d             run in background (daemonize)\n"
#endif
		"\t-m<bytes>      set maximum heap size (default=%u bytes)\n"
		"\t-s<bytes>      set context stack size (default=%u bytes)\n"
		"\t-t<limit>      set time limit (default=%u, 0=unlimited)\n"
		"\t-y<interval>   set yield interval (default=%u, 0=never)\n"
		"\t-g<interval>   set garbage collection interval (default=%u, 0=never)\n"
#ifdef JSDOOR
		"\t-h[hostname]   use local or specified host name\n"
#else
		"\t-h[hostname]   use local or specified host name (instead of SCFG value)\n"
#endif
		"\t-u<mask>       set file creation permissions mask (in octal)\n"
		"\t-L<level>      set log level (default=%u)\n"
		"\t-E<level>      set error log level threshold (default=%u)\n"
		"\t-i<path_list>  set load() comma-sep search path list (default=\"%s\")\n"
		"\t-f             use non-buffered stream for console messages\n"
		"\t-a             append instead of overwriting message output files\n"
		"\t-e<filename>   send error messages to file in addition to stderr\n"
		"\t-o<filename>   send console messages to file instead of stdout\n"
		"\t-n             send status messages to %s instead of stderr\n"
		"\t-q             send console messages to %s instead of stdout\n"
		"\t-v             display version details and exit\n"
		"\t-x             disable auto-termination on local abort signal\n"
		"\t-l             loop until intentionally terminated\n"
		"\t-p             wait for keypress (pause) on exit\n"
		"\t-!             wait for keypress (pause) on error\n"
		"\t-D             debugs the script\n"
		,JAVASCRIPT_MAX_BYTES
		,JAVASCRIPT_CONTEXT_STACK
		,JAVASCRIPT_TIME_LIMIT
		,JAVASCRIPT_YIELD_INTERVAL
		,JAVASCRIPT_GC_INTERVAL
		,DEFAULT_LOG_LEVEL
		,DEFAULT_ERR_LOG_LVL
		,load_path_list
		,_PATH_DEVNULL
		,_PATH_DEVNULL
		);
}

int mfprintf(FILE* fp, char *fmt, ...)
{
	va_list argptr;
	char sbuf[1024];
	int ret=0;

    va_start(argptr,fmt);
    ret=vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
    va_end(argptr);

	/* Mutex-protect stdout/stderr */
	pthread_mutex_lock(&output_mutex);

	ret = fprintf(fp, "%s\n", sbuf);

	pthread_mutex_unlock(&output_mutex);
    return(ret);
}

/* Log printf */
int lprintf(int level, const char *fmt, ...)
{
	va_list argptr;
	char sbuf[1024];
	int ret=0;

	if(level > log_level)
		return(0);

    va_start(argptr,fmt);
    ret=vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
    va_end(argptr);
#if defined(__unix__)
	if(daemonize) {
		syslog(level,"%s",sbuf);
		return(ret);
	}
#endif

	/* Mutex-protect stdout/stderr */
	pthread_mutex_lock(&output_mutex);

	if(level<=err_level) {
		ret=fprintf(errfp,"%s\n",sbuf);
		if(errfp!=stderr)
			ret=fprintf(statfp,"%s\n",sbuf);
	}
	if(level>err_level)
		ret=fprintf(statfp,"%s\n",sbuf);

	pthread_mutex_unlock(&output_mutex);
    return(ret);
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
        return (-1);
    case 0:
        break;
    default:
        _exit(0);
    }

    if (setsid() == -1)
        return (-1);

    if (!nochdir)
        (void)chdir("/");

    if (!noclose && (fd = open(_PATH_DEVNULL, O_RDWR, 0)) != -1) {
        (void)dup2(fd, STDIN_FILENO);
        (void)dup2(fd, STDOUT_FILENO);
        (void)dup2(fd, STDERR_FILENO);
        if (fd > 2)
            (void)close(fd);
    }
    return (0);
}
#endif

#if defined(_WINSOCKAPI_)

WSADATA WSAData;
#define SOCKLIB_DESC WSAData.szDescription
static BOOL WSAInitialized=FALSE;

static BOOL winsock_startup(void)
{
	int		status;             /* Status Code */

    if((status = WSAStartup(MAKEWORD(1,1), &WSAData))==0) {
/*		fprintf(statfp,"%s %s\n",WSAData.szDescription, WSAData.szSystemStatus); */
		WSAInitialized=TRUE;
		return(TRUE);
	}

    lprintf(LOG_CRIT,"!WinSock startup ERROR %d", status);
	return(FALSE);
}

#else /* No WINSOCK */

#define winsock_startup()	(TRUE)
#define SOCKLIB_DESC NULL

#endif

#ifdef __unix__
struct termios orig_term;
#endif

static int do_bail(int code)
{
#if defined(_WINSOCKAPI_)
	if(WSAInitialized && WSACleanup()!=0) 
		lprintf(LOG_ERR,"!WSACleanup ERROR %d",ERROR_VALUE);
#endif

	if(pause_on_exit || (code && pause_on_error)) {
		fprintf(statfp,"\nHit enter to continue...");
		getchar();
	}

	if(code)
		fprintf(statfp,"\nReturning error code: %d\n",code);
#ifdef __unix__
	if(isatty(fileno(stdin)))
		tcsetattr(STDIN_FILENO, TCSANOW, &orig_term);
#endif
	return(code);
}

void bail(int code)
{
	exit(do_bail(code));
}

static JSBool
js_log(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
    uintN		i=0;
	int32		level=LOG_INFO;
    JSString*	str=NULL;
	jsrefcount	rc;
	char		*logstr=NULL;
	size_t		logstr_sz=0;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if(argc > 1 && JSVAL_IS_NUMBER(argv[i])) {
		if(!JS_ValueToInt32(cx,argv[i++],&level))
			return JS_FALSE;
	}

	for(; i<argc; i++) {
		if((str=JS_ValueToString(cx, argv[i]))==NULL)
			return(JS_FALSE);
		JSSTRING_TO_RASTRING(cx, str, logstr, &logstr_sz, NULL);
		if(logstr==NULL)
			return(JS_FALSE);
		rc=JS_SUSPENDREQUEST(cx);
		lprintf(level,"%s",logstr);
		JS_RESUMEREQUEST(cx, rc);
		FREE_AND_NULL(logstr);
	}

	if(str==NULL)
		JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	else
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(str));

    return(JS_TRUE);
}

static JSBool
js_read(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char*	buf;
	int		rd;
	int32	len=128;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if(argc) {
		if(!JS_ValueToInt32(cx,argv[0],&len))
			return JS_FALSE;
	}
	if((buf=malloc(len))==NULL)
		return(JS_TRUE);

	rc=JS_SUSPENDREQUEST(cx);
	rd=fread(buf,sizeof(char),len,stdin);
	JS_RESUMEREQUEST(cx, rc);

	if(rd>=0)
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(JS_NewStringCopyN(cx,buf,rd)));
	free(buf);

    return(JS_TRUE);
}

static JSBool
js_readln(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char*	buf;
	char*	p;
	int32	len=128;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if(argc) {
		if(!JS_ValueToInt32(cx,argv[0],&len))
			return JS_FALSE;
	}
	if((buf=malloc(len+1))==NULL)
		return(JS_TRUE);

	rc=JS_SUSPENDREQUEST(cx);
	p=fgets(buf,len+1,stdin);
	JS_RESUMEREQUEST(cx, rc);

	if(p!=NULL)
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(JS_NewStringCopyZ(cx,truncnl(p))));
	free(buf);

    return(JS_TRUE);
}


static JSBool
js_write(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
    uintN		i;
    JSString*	str=NULL;
	jsrefcount	rc;
	char		*line=NULL;
	size_t		line_sz=0;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

    for (i = 0; i < argc; i++) {
		if((str=JS_ValueToString(cx, argv[i]))==NULL)
		    return(JS_FALSE);
		JSSTRING_TO_RASTRING(cx, str, line, &line_sz, NULL);
		if(line==NULL)
			return(JS_FALSE);
		rc=JS_SUSPENDREQUEST(cx);
		fprintf(confp,"%s",line);
		FREE_AND_NULL(line);
		JS_RESUMEREQUEST(cx, rc);
	}

	if(str==NULL)
		JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	else
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(str));
    return(JS_TRUE);
}

static JSBool
js_writeln(JSContext *cx, uintN argc, jsval *arglist)
{
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if(!js_write(cx,argc,arglist))
		return(JS_FALSE);

	rc=JS_SUSPENDREQUEST(cx);
	fprintf(confp,"\n");
	JS_RESUMEREQUEST(cx, rc);
    return(JS_TRUE);
}

static JSBool
jse_printf(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char* p;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p = js_sprintf(cx, 0, argc, argv))==NULL) {
		JS_ReportError(cx,"js_sprintf failed");
		return(JS_FALSE);
	}

	rc=JS_SUSPENDREQUEST(cx);
	fprintf(confp,"%s",p);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, p)));

	js_sprintf_free(p);

    return(JS_TRUE);
}

static JSBool
js_alert(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	jsrefcount	rc;
	char		*line;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	JSVALUE_TO_MSTRING(cx, argv[0], line, NULL);
	if(line==NULL)
	    return(JS_FALSE);

	rc=JS_SUSPENDREQUEST(cx);
	fprintf(confp,"!%s\n",line);
	free(line);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, argv[0]);

    return(JS_TRUE);
}

static JSBool
js_confirm(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
    JSString *	str;
	char	 *	cstr;
	char     *	p;
	jsrefcount	rc;
	char		instr[81]="y";

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((str=JS_ValueToString(cx, argv[0]))==NULL)
	    return(JS_FALSE);

	JSSTRING_TO_MSTRING(cx, str, cstr, NULL);
	HANDLE_PENDING(cx);
	if(cstr==NULL)
		return JS_TRUE;
	rc=JS_SUSPENDREQUEST(cx);
	printf("%s (Y/n)? ", cstr);
	free(cstr);
	fgets(instr,sizeof(instr),stdin);
	JS_RESUMEREQUEST(cx, rc);

	p=instr;
	SKIP_WHITESPACE(p);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(tolower(*p)!='n'));
	return(JS_TRUE);
}

static JSBool
js_deny(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
    JSString *	str;
	char	 *	cstr;
	char     *	p;
	jsrefcount	rc;
	char		instr[81];

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((str=JS_ValueToString(cx, argv[0]))==NULL)
	    return(JS_FALSE);

	JSSTRING_TO_MSTRING(cx, str, cstr, NULL);
	HANDLE_PENDING(cx);
	if(cstr==NULL)
		return JS_TRUE;
	rc=JS_SUSPENDREQUEST(cx);
	printf("%s (N/y)? ", cstr);
	free(cstr);
	fgets(instr,sizeof(instr),stdin);
	JS_RESUMEREQUEST(cx, rc);

	p=instr;
	SKIP_WHITESPACE(p);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(tolower(*p)!='y'));
	return(JS_TRUE);
}

static JSBool
js_prompt(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char		instr[81];
    JSString *	str;
	jsrefcount	rc;
	char		*prstr;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if(argc>0 && !JSVAL_IS_VOID(argv[0])) {
		JSVALUE_TO_MSTRING(cx, argv[0], prstr, NULL);
		HANDLE_PENDING(cx);
		if(prstr==NULL)
			return(JS_FALSE);
		rc=JS_SUSPENDREQUEST(cx);
		fprintf(confp,"%s: ",prstr);
		free(prstr);
		JS_RESUMEREQUEST(cx, rc);
	}

	if(argc>1) {
		JSVALUE_TO_STRBUF(cx, argv[1], instr, sizeof(instr), NULL);
		HANDLE_PENDING(cx);
	} else
		instr[0]=0;

	rc=JS_SUSPENDREQUEST(cx);
	if(!fgets(instr,sizeof(instr),stdin)) {
		JS_RESUMEREQUEST(cx, rc);
		return(JS_TRUE);
	}
	JS_RESUMEREQUEST(cx, rc);

	if((str=JS_NewStringCopyZ(cx, truncnl(instr)))==NULL)
	    return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(str));
    return(JS_TRUE);
}

static JSBool
js_chdir(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char*		p;
	jsrefcount	rc;
	BOOL		ret;

	JSVALUE_TO_MSTRING(cx, argv[0], p, NULL);
	HANDLE_PENDING(cx);
	if(p==NULL) {
		JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(-1));
		return(JS_TRUE);
	}

	rc=JS_SUSPENDREQUEST(cx);
	ret=chdir(p)==0;
	free(p);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));
	return(JS_TRUE);
}

static JSBool
js_putenv(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char*		p=NULL;
	BOOL		ret;
	jsrefcount	rc;

	if(argc) {
		JSVALUE_TO_MSTRING(cx, argv[0], p, NULL);
		HANDLE_PENDING(cx);
	}
	if(p==NULL) {
		JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(-1));
		return(JS_TRUE);
	}

	rc=JS_SUSPENDREQUEST(cx);
	ret=putenv(strdup(p))==0;
	free(p);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));
	return(JS_TRUE);
}

static jsSyncMethodSpec js_global_functions[] = {
	{"log",				js_log,				1},
	{"read",			js_read,            1},
	{"readln",			js_readln,			0,	JSTYPE_STRING,	JSDOCSTR("[count]")
	,JSDOCSTR("read a single line, up to count characters, from input stream")
	,311
	},
    {"write",           js_write,           0},
    {"writeln",         js_writeln,         0},
    {"print",           js_writeln,         0},
    {"printf",          jse_printf,          1},	
	{"alert",			js_alert,			1},
	{"prompt",			js_prompt,			1},
	{"confirm",			js_confirm,			1},
	{"deny",			js_deny,			1},
	{"chdir",			js_chdir,			1},
	{"putenv",			js_putenv,			1},
    {0}
};

static void
js_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
	char	line[64];
	char	file[MAX_PATH+1];
	const char*	warning;
	jsrefcount	rc;
	int		log_level;

	rc=JS_SUSPENDREQUEST(cx);
	if(report==NULL) {
		lprintf(LOG_ERR,"!JavaScript: %s", message);
		JS_RESUMEREQUEST(cx, rc);
		return;
    }

	if(report->filename)
		sprintf(file," %s",report->filename);
	else
		file[0]=0;

	if(report->lineno)
		sprintf(line," line %d",report->lineno);
	else
		line[0]=0;

	if(JSREPORT_IS_WARNING(report->flags)) {
		if(JSREPORT_IS_STRICT(report->flags))
			warning="strict warning";
		else
			warning="warning";
		log_level=LOG_WARNING;
	} else {
		log_level=LOG_ERR;
		warning="";
	}

	lprintf(log_level,"!JavaScript %s%s%s: %s",warning,file,line,message);
	JS_RESUMEREQUEST(cx, rc);
}

static JSBool
js_OperationCallback(JSContext *cx)
{
	JSBool	ret;

	JS_SetOperationCallback(cx, NULL);
	ret=js_CommonOperationCallback(cx, &cb);
	JS_SetOperationCallback(cx, js_OperationCallback);
	return ret;
}

static BOOL js_CreateEnvObject(JSContext* cx, JSObject* glob, char** env)
{
	char		name[256];
	char*		val;
	uint		i;
	JSObject*	js_env;

	if((js_env=JS_NewObject(js_cx, NULL, NULL, glob))==NULL)
		return(FALSE);

	if(!JS_DefineProperty(cx, glob, "env", OBJECT_TO_JSVAL(js_env)
		,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE))
		return(FALSE);

	for(i=0;env[i]!=NULL;i++) {
		SAFECOPY(name,env[i]);
		truncstr(name,"=");
		val=strchr(env[i],'=');
		if(val==NULL)
			val="";
		else
			val++;
		if(!JS_DefineProperty(cx, js_env, name, STRING_TO_JSVAL(JS_NewStringCopyZ(cx,val))
			,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE))
			return(FALSE);
	}

	return(TRUE);
}

static BOOL js_init(char** environ)
{
	memset(&startup,0,sizeof(startup));
	SAFECOPY(startup.load_path, load_path_list);

	fprintf(statfp,"%s\n",(char *)JS_GetImplementationVersion());

	fprintf(statfp,"JavaScript: Creating runtime: %lu bytes\n"
		,js_max_bytes);

	if((js_runtime = jsrt_GetNew(js_max_bytes, 5000, __FILE__, __LINE__))==NULL)
		return(FALSE);

	fprintf(statfp,"JavaScript: Initializing context (stack: %lu bytes)\n"
		,js_cx_stack);

    if((js_cx = JS_NewContext(js_runtime, js_cx_stack))==NULL)
		return(FALSE);
	JS_BEGINREQUEST(js_cx);

	JS_SetErrorReporter(js_cx, js_ErrorReporter);

	/* Global Object */
	if(!js_CreateCommonObjects(js_cx, &scfg, NULL, js_global_functions
		,time(NULL), host_name, SOCKLIB_DESC	/* system */
		,&cb,&startup						/* js */
		,NULL,INVALID_SOCKET,-1					/* client */
		,NULL									/* server */
		,&js_glob
		)) {
		JS_ENDREQUEST(js_cx);
		return(FALSE);
	}

	/* Environment Object (associative array) */
	if(!js_CreateEnvObject(js_cx, js_glob, environ)) {
		JS_ENDREQUEST(js_cx);
		return(FALSE);
	}

	if(js_CreateUifcObject(js_cx, js_glob)==NULL) {
		JS_ENDREQUEST(js_cx);
		return(FALSE);
	}

	if(js_CreateConioObject(js_cx, js_glob)==NULL) {
		JS_ENDREQUEST(js_cx);
		return(FALSE);
	}

	/* STDIO objects */
	if(!js_CreateFileObject(js_cx, js_glob, "stdout", stdout)) {
		JS_ENDREQUEST(js_cx);
		return(FALSE);
	}

	if(!js_CreateFileObject(js_cx, js_glob, "stdin", stdin)) {
		JS_ENDREQUEST(js_cx);
		return(FALSE);
	}

	if(!js_CreateFileObject(js_cx, js_glob, "stderr", stderr)) {
		JS_ENDREQUEST(js_cx);
		return(FALSE);
	}

	return(TRUE);
}

static const char* js_ext(const char* fname)
{
	if(strchr(fname,'.')==NULL)
		return(".js");
	return("");
}

void dbg_puts(const char *msg)
{
	fputs(msg, stderr);
}

char *dbg_getline(void)
{
#ifdef __unix__
	char	*line=NULL;
	size_t	linecap=0;

	if(getline(&line, &linecap, stdin)>=0)
		return line;
	if(line)
		free(line);
	return NULL;
#else
	// I assume Windows sucks.
	char	buf[1025];

	if(fgets(buf,sizeof(buf),stdin))
		return strdup(buf);
	return NULL;
#endif
}

long js_exec(const char *fname, char** args)
{
	int			argc=0;
	uint		line_no;
	char		path[MAX_PATH+1];
	char		line[1024];
	char		rev_detail[256];
	size_t		len;
	char*		js_buf=NULL;
	size_t		js_buflen;
	JSObject*	js_script=NULL;
	JSString*	arg;
	JSObject*	argv;
	FILE*		fp=stdin;
	jsval		val;
	jsval		rval=JSVAL_VOID;
	int32		result=0;
	long double	start;
	long double	diff;

	if(fname!=NULL) {
		if(isfullpath(fname)) {
			SAFECOPY(path,fname);
		}
		else {
			SAFEPRINTF3(path,"%s%s%s",orig_cwd,fname,js_ext(fname));
			if(!fexistcase(path)) {
				SAFEPRINTF3(path,"%s%s%s",scfg.mods_dir,fname,js_ext(fname));
				if(scfg.mods_dir[0]==0 || !fexistcase(path))
					SAFEPRINTF3(path,"%s%s%s",scfg.exec_dir,fname,js_ext(fname));
			}
		}

		if(!fexistcase(path)) {
			lprintf(LOG_ERR,"!Module file (%s) doesn't exist",path);
			return(-1); 
		}

		if((fp=fopen(path,"r"))==NULL) {
			lprintf(LOG_ERR,"!Error %d (%s) opening %s",errno,STRERROR(errno),path);
			return(-1);
		}
	}
	JS_ClearPendingException(js_cx);

	argv=JS_NewArrayObject(js_cx, 0, NULL);
	JS_DefineProperty(js_cx, js_glob, "argv", OBJECT_TO_JSVAL(argv)
		,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);

	for(argc=0;args[argc];argc++) {
		arg = JS_NewStringCopyZ(js_cx, args[argc]);
		if(arg==NULL)
			break;
		val=STRING_TO_JSVAL(arg);
		if(!JS_SetElement(js_cx, argv, argc, &val))
			break;
	}
	JS_DefineProperty(js_cx, js_glob, "argc", INT_TO_JSVAL(argc)
		,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);

	JS_DefineProperty(js_cx, js_glob, PROG_NAME_LC "_revision"
		,STRING_TO_JSVAL(JS_NewStringCopyZ(js_cx,revision))
		,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);

	sprintf(rev_detail,PROG_NAME " %s%s  "
		"Compiled %s %s with %s"
		,revision
#ifdef _DEBUG
		," Debug"
#else
		,""
#endif
		,__DATE__, __TIME__, compiler
		);

	JS_DefineProperty(js_cx, js_glob, PROG_NAME_LC "_revision_detail"
		,STRING_TO_JSVAL(JS_NewStringCopyZ(js_cx,rev_detail))
		,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);

	cb.terminated=&terminated;

	JS_SetOperationCallback(js_cx, js_OperationCallback);

	if(fp==stdin) 	 /* Using stdin for script source */
		SAFECOPY(path,"stdin");

	fprintf(statfp,"Reading script from %s\n",path);
	line_no=0;
	js_buflen=0;
	while(!feof(fp)) {
		if(!fgets(line,sizeof(line),fp))
			break;
		line_no++;
		/* Support Unix Shell Scripts that start with #!/path/to/jsexec */
		if(line_no==1 && strncmp(line,"#!",2)==0)
			strcpy(line,"\n");	/* To keep line count correct */
		len=strlen(line);
		if((js_buf=realloc(js_buf,js_buflen+len))==NULL) {
			lprintf(LOG_ERR,"!Error allocating %u bytes of memory"
				,js_buflen+len);
			return(-1);
		}
		memcpy(js_buf+js_buflen,line,len);
		js_buflen+=len;
	}
	if(fp!=NULL && fp!=stdin)
		fclose(fp);

	start=xp_timer();
	if(debugger)
		init_debugger(js_runtime, js_cx, dbg_puts, dbg_getline);
	if((js_script=JS_CompileScript(js_cx, js_glob, js_buf, js_buflen, fname==NULL ? NULL : path, 1))==NULL) {
		lprintf(LOG_ERR,"!Error compiling script from %s",path);
		return(-1);
	}
	if((diff=xp_timer()-start) > 0)
		mfprintf(statfp,"%s compiled in %.2Lf seconds"
			,path
			,diff);

	js_PrepareToExecute(js_cx, js_glob, fname==NULL ? NULL : path, orig_cwd, js_glob);
	start=xp_timer();
	if(debugger)
		debug_prompt(js_cx, js_script);
	JS_ExecuteScript(js_cx, js_glob, js_script, &rval);
	JS_GetProperty(js_cx, js_glob, "exit_code", &rval);
	if(rval!=JSVAL_VOID && JSVAL_IS_NUMBER(rval)) {
		char	*p;

		JSVALUE_TO_MSTRING(js_cx, rval, p, NULL);
		mfprintf(statfp,"Using JavaScript exit_code: %s",p);
		free(p);
		JS_ValueToInt32(js_cx,rval,&result);
	}
	js_EvalOnExit(js_cx, js_glob, &cb);

	JS_ReportPendingException(js_cx);

	if((diff=xp_timer()-start) > 0)
		mfprintf(statfp,"%s executed in %.2Lf seconds"
			,path
			,diff);

	if(js_buf!=NULL)
		free(js_buf);

	return(result);
}

void break_handler(int type)
{
	lprintf(LOG_NOTICE,"\n-> Terminated Locally (signal: %d)",type);
	terminated=TRUE;
}

void recycle_handler(int type)
{
	lprintf(LOG_NOTICE,"\n-> Recycled Locally (signal: %d)",type);
	recycled=TRUE;
	cb.terminated=&recycled;
}


#if defined(_WIN32)
BOOL WINAPI ControlHandler(unsigned long CtrlType)
{
	break_handler((int)CtrlType);
	return TRUE;
}
#endif

int parseLogLevel(const char* p)
{
	str_list_t logLevelStringList=iniLogLevelStringList();
	int i;

	if(isdigit(*p))
		return strtol(p,NULL,0);

	/* Exact match */
	for(i=0;logLevelStringList[i]!=NULL;i++) {
		if(stricmp(logLevelStringList[i],p)==0)
			return i;
	}
	/* Partial match */
	for(i=0;logLevelStringList[i]!=NULL;i++) {
		if(strnicmp(logLevelStringList[i],p,strlen(p))==0)
			return i;
	}
	return DEFAULT_LOG_LEVEL;
}

#ifdef __unix__
void raw_input(struct termios *t)
{
#ifdef JSDOOR
	t->c_iflag &= ~(IMAXBEL|IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
	t->c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
#else
	t->c_iflag &= ~(IMAXBEL|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
	t->c_lflag &= ~(ECHO|ECHONL|ICANON|IEXTEN);
#endif
}
#endif

/*********************/
/* Entry point (duh) */
/*********************/
int main(int argc, char **argv, char** environ)
{
#ifndef JSDOOR
	char	error[512];
#endif
	char*	module=NULL;
	char*	p;
	char*	omode="w";
	int		argn;
	long	result;
	ulong	exec_count=0;
	BOOL	loop=FALSE;
	BOOL	nonbuffered_con=FALSE;

	confp=stdout;
	errfp=stderr;
	if((nulfp=fopen(_PATH_DEVNULL,"w+"))==NULL) {
		perror(_PATH_DEVNULL);
		return(do_bail(-1));
	}
	if(isatty(fileno(stdin))) {
#ifdef __unix__
		struct termios term;

		tcgetattr(fileno(stdin), &orig_term);
		term = orig_term;
		raw_input(&term);
		tcsetattr(fileno(stdin), TCSANOW, &term);
#else
	//	This completely disabled console input on Windows:
	//	SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), 0);
#endif
		statfp=stderr;
	}
	else	/* if redirected, don't send status messages to stderr */
		statfp=nulfp;

	cb.limit=JAVASCRIPT_TIME_LIMIT;
	cb.yield_interval=JAVASCRIPT_YIELD_INTERVAL;
	cb.gc_interval=JAVASCRIPT_GC_INTERVAL;
	cb.auto_terminate=TRUE;

	sscanf("$Revision$", "%*s %s", revision);
	DESCRIBE_COMPILER(compiler);

	memset(&scfg,0,sizeof(scfg));
	scfg.size=sizeof(scfg);

	if(!winsock_startup())
		return(do_bail(2));

	getcwd(orig_cwd, sizeof(orig_cwd));
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
	gethostname(host_name=host_name_buf,sizeof(host_name_buf));
	statfp = nulfp;
	errfp = fopen("error.log", "a");
#endif

	for(argn=1;argn<argc && module==NULL;argn++) {
		if(argv[argn][0]=='-') {
			p=argv[argn]+2;
			switch(argv[argn][1]) {
				case 'a':
					omode="a";
					break;
				case 'c':
					if(*p==0) p=argv[++argn];
					SAFECOPY(scfg.ctrl_dir,p);
					break;
#if defined(__unix__)
				case 'd':
					daemonize=TRUE;
					break;
#endif
				case 'D':
					debugger=TRUE;
					break;
				case 'E':
					if(*p==0) p=argv[++argn];
					err_level=parseLogLevel(p);
					break;
				case 'e':
					if(*p==0) p=argv[++argn];
					if (errfp != stderr)
						fclose(errfp);
					if((errfp=fopen(p,omode))==NULL) {
						perror(p);
						return(do_bail(1));
					}
					break;
				case 'f':
					nonbuffered_con=TRUE;
					break;
				case 'g':
					if(*p==0) p=argv[++argn];
					cb.gc_interval=strtoul(p,NULL,0);
					break;
				case 'h':
					if(*p==0)
						gethostname(host_name=host_name_buf,sizeof(host_name_buf));
					else
						host_name=p;
					break;
				case 'i':
					if(*p==0) p=argv[++argn];
					load_path_list=p;
					break;
				case 'L':
					if(*p==0) p=argv[++argn];
					log_level=parseLogLevel(p);
					break;
				case 'l':
					loop=TRUE;
					break;
				case 'm':
					if(*p==0) p=argv[++argn];
					js_max_bytes=(ulong)parse_byte_count(p, /* units: */1);
					break;
				case 'n':
					statfp=nulfp;
					break;
				case 'o':
					if(*p==0) p=argv[++argn];
					if((confp=fopen(p,omode))==NULL) {
						perror(p);
						return(do_bail(1));
					}
					break;
				case 'p':
					pause_on_exit=TRUE;
					break;
				case 'q':
					confp=nulfp;
					break;
				case 's':
					if(*p==0) p=argv[++argn];
					js_cx_stack=(ulong)parse_byte_count(p, /* units: */1);
					break;
				case 't':
					if(*p==0) p=argv[++argn];
					cb.limit=strtoul(p,NULL,0);
					break;
				case 'u':
					if(*p==0) p=argv[++argn];
					umask(strtol(p,NULL,8));
					break;
				case 'v':
					banner(statfp);
					fprintf(statfp,"%s\n",(char *)JS_GetImplementationVersion());
					return(do_bail(0));
				case 'x':
					cb.auto_terminate=FALSE;
					break;
				case 'y':
					if(*p==0) p=argv[++argn];
					cb.yield_interval=strtoul(p,NULL,0);
					break;
				case '!':
					pause_on_error=TRUE;
					break;
				default:
					fprintf(errfp,"\n!Unsupported option: %s\n",argv[argn]);
				case '?':
					usage(errfp);
					return(do_bail(1));
			}
			continue;
		}
		module=argv[argn];
	}

#ifndef JSDOOR
	if(scfg.ctrl_dir[0]==0) {
		if((p=getenv("SBBSCTRL"))==NULL) {
			fprintf(errfp,"\nSBBSCTRL environment variable not set and -c option not specified.\n");
			fprintf(errfp,"\nExample: SET SBBSCTRL=/sbbs/ctrl\n");
			fprintf(errfp,"\n     or: %s -c /sbbs/ctrl [module]\n",argv[0]);
			return(do_bail(1)); 
		}
		SAFECOPY(scfg.ctrl_dir,p);
	}	
#endif

	if(module==NULL && isatty(fileno(stdin))) {
		fprintf(errfp,"\n!Module name not specified\n");
		usage(errfp);
		return(do_bail(1)); 
	}

	banner(statfp);

#ifdef JSDOOR
	SAFECOPY(scfg.temp_dir,"./temp");
#else
	if(chdir(scfg.ctrl_dir)!=0)
		fprintf(errfp,"!ERROR changing directory to: %s\n", scfg.ctrl_dir);

	fprintf(statfp,"\nLoading configuration files from %s\n",scfg.ctrl_dir);
	if(!load_cfg(&scfg,NULL,TRUE,error)) {
		fprintf(errfp,"!ERROR loading configuration files: %s\n",error);
		return(do_bail(1));
	}
	SAFECOPY(scfg.temp_dir,"../temp");
#endif
	prep_dir(scfg.ctrl_dir, scfg.temp_dir, sizeof(scfg.temp_dir));

	if(host_name==NULL)
		host_name=scfg.sys_inetaddr;

#if defined(__unix__)
	if(daemonize) {
		fprintf(statfp,"\nRunning as daemon\n");
		if(daemon(TRUE,FALSE))  { /* Daemonize, DON'T switch to / and DO close descriptors */
			fprintf(statfp,"!ERROR %d running as daemon\n",errno);
			daemonize=FALSE;
		}
	}
#endif

	/* Don't cache error log */
	setvbuf(errfp,NULL,_IONBF,0);

	if(nonbuffered_con)
		setvbuf(confp,NULL,_IONBF,0);

	/* Seed random number generator */
	sbbs_srand();

	/* Install Ctrl-C/Break signal handler here */
#if defined(_WIN32)
	SetConsoleCtrlHandler(ControlHandler, TRUE /* Add */);
#elif defined(__unix__)
	signal(SIGQUIT,break_handler);
	signal(SIGINT,break_handler);
	signal(SIGTERM,break_handler);

	signal(SIGHUP,recycle_handler);

	/* Don't die on SIGPIPE  */
	signal(SIGPIPE,SIG_IGN);
#endif

	pthread_mutex_init(&output_mutex,NULL);

	setup_debugger();

	do {

		if(exec_count++)
			lprintf(LOG_INFO,"\nRe-running: %s", module);

		recycled=FALSE;

		if(!js_init(environ)) {
			lprintf(LOG_ERR,"!JavaScript initialization failure");
			return(do_bail(1));
		}
		fprintf(statfp,"\n");

		result=js_exec(module,&argv[argn]);
		JS_RemoveObjectRoot(js_cx, &js_glob);
		JS_ENDREQUEST(js_cx);
		YIELD();

		if(result)
			lprintf(LOG_ERR,"!Module set exit_code: %ld", result);

		fprintf(statfp,"\n");
		fprintf(statfp,"JavaScript: Destroying context\n");
		JS_DestroyContext(js_cx);
		fprintf(statfp,"JavaScript: Destroying runtime\n");
		jsrt_Release(js_runtime);	

	} while((recycled || loop) && !terminated);

	return(do_bail(result));
}

