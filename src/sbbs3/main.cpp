/* main.cpp */

/* Synchronet main/telnet server thread and related functions */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#include "sbbs.h"
#include "ident.h"
#include "telnet.h" 

//---------------------------------------------------------------------------

/* Temporary */
int	mswtyp=0;
uint riobp;

#define TELNET_SERVER "Synchronet Telnet Server"
#define STATUS_WFC	"Listening"

#define TIMEOUT_THREAD_WAIT		60		// Seconds (was 15)
#define IO_THREAD_BUF_SIZE	   	10000   // Bytes

// Globals
#ifdef _WIN32
	HANDLE		exec_mutex=NULL;
	HINSTANCE	hK32=NULL;

	GetLongPathName_t Win98GetLongPathName=NULL;

	#if defined(_DEBUG) && defined(_MSC_VER)
			HANDLE	debug_log=INVALID_HANDLE_VALUE;
		   _CrtMemState mem_chkpoint;
	#endif // _DEBUG && _MSC_VER

#endif // _WIN32

time_t	uptime=0;
DWORD	served=0;

static	uint node_threads_running=0;
static	uint thread_count=0;
		
char 	lastuseron[LEN_ALIAS+1];  /* Name of user last online */
RingBuf* node_inbuf[MAX_NODES];
SOCKET	spy_socket[MAX_NODES];
SOCKET	node_socket[MAX_NODES];
static	SOCKET telnet_socket=INVALID_SOCKET;
static	SOCKET rlogin_socket=INVALID_SOCKET;
static	pthread_mutex_t event_mutex;
static	sbbs_t*	sbbs=NULL;
static	scfg_t	scfg;
static	bool	scfg_reloaded=true;
static	char *	text[TOTAL_TEXT];
static	WORD	first_node;
static	WORD	last_node;
static	bool	recycle_server=false;

extern "C" {

static bbs_startup_t* startup=NULL;

static void status(char* str)
{
	if(startup!=NULL && startup->status!=NULL)
	    startup->status(str);
}

static void update_clients()
{
	if(startup!=NULL && startup->clients!=NULL)
		startup->clients(node_threads_running);
}

void client_on(SOCKET sock, client_t* client, BOOL update)
{
	if(startup!=NULL && startup->client_on!=NULL)
		startup->client_on(TRUE,sock,client,update);
}

static void client_off(SOCKET sock)
{
	if(startup!=NULL && startup->client_on!=NULL)
		startup->client_on(FALSE,sock,NULL,FALSE);
}

static void thread_up(BOOL setuid)
{
	thread_count++;
	if(startup!=NULL && startup->thread_up!=NULL)
		startup->thread_up(TRUE,setuid);
}

static void thread_down()
{
	if(thread_count>0)
		thread_count--;
	if(startup!=NULL && startup->thread_up!=NULL)
		startup->thread_up(FALSE,FALSE);
}

int lputs(char* str)
{
	if(startup==NULL || startup->lputs==NULL)
    	return(0);

#if defined(_WIN32) && defined(_DEBUG)
	if(IsBadCodePtr((FARPROC)startup->lputs)) {
		DebugBreak();
		return(0);
	}
#endif

    return(startup->lputs(str));
}

int lprintf(char *fmt, ...)
{
	va_list argptr;
	char sbuf[1024];

    if(startup==NULL || startup->lputs==NULL)
        return(0);

    va_start(argptr,fmt);
    vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
    va_end(argptr);
    return(startup->lputs(sbuf));
}

int eprintf(char *fmt, ...)
{
	va_list argptr;
	char sbuf[1024];

    if(startup==NULL || startup->event_log==NULL)
        return(0);

    va_start(argptr,fmt);
    vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
    va_end(argptr);
	strip_ctrl(sbuf);
    return(startup->event_log(sbuf));
}

SOCKET open_socket(int type)
{
	SOCKET	sock;
	char	error[256];

	sock=socket(AF_INET, type, IPPROTO_IP);
	if(sock!=INVALID_SOCKET && startup!=NULL && startup->socket_open!=NULL) 
		startup->socket_open(TRUE);
	if(sock!=INVALID_SOCKET && set_socket_options(&scfg, sock, error))
		lprintf("%04d !ERROR %s",sock,error);

	return(sock);
}

SOCKET accept_socket(SOCKET s, SOCKADDR* addr, socklen_t* addrlen)
{
	SOCKET	sock;

	sock=accept(s,addr,addrlen);
	if(sock!=INVALID_SOCKET && startup!=NULL && startup->socket_open!=NULL) 
		startup->socket_open(TRUE);

	return(sock);
}

int close_socket(SOCKET sock)
{
	int		result;

	if(sock==INVALID_SOCKET || sock==0)
		return(0);

	shutdown(sock,SHUT_RDWR);	/* required on Unix */
	result=closesocket(sock);
	if(result==0 && startup!=NULL && startup->socket_open!=NULL) 
		startup->socket_open(FALSE);
	if(result!=0 && ERROR_VALUE!=ENOTSOCK)
		lprintf("!ERROR %d closing socket %d",ERROR_VALUE,sock);
	return(result);
}


u_long resolve_ip(char *addr)
{
	HOSTENT*	host;
	char*		p;

	if(*addr==0)
		return(INADDR_NONE);

	for(p=addr;*p;p++)
		if(*p!='.' && !isdigit(*p))
			break;
	if(!(*p))
		return(inet_addr(addr));
	if((host=gethostbyname(addr))==NULL) 
		return(INADDR_NONE);
	return(*((ulong*)host->h_addr_list[0]));
}

} /* extern "C" */

#ifdef JAVASCRIPT

JSBool	
DLLCALL js_CreateArrayOfStrings(JSContext* cx, JSObject* parent, const char* name, char* str[],uintN flags)
{
	JSObject*	array;
	JSString*	js_str;
	jsval		val;
	size_t		i;
	jsuint		len=0;
		
	if(JS_GetProperty(cx,parent,name,&val) && val!=JSVAL_VOID)
		array=JSVAL_TO_OBJECT(val);
	else
		if((array=JS_NewArrayObject(cx, 0, NULL))==NULL)
			return(JS_FALSE);

	if(!JS_GetArrayLength(cx, array, &len))
		return(JS_FALSE);

	for(i=0;str[i]!=NULL;i++) {
		if((js_str = JS_NewStringCopyZ(cx, str[i]))==NULL)
			break;
		val = STRING_TO_JSVAL(js_str);
		if(!JS_SetElement(cx, array, len+i, &val))
			break;
	}

	return(JS_DefineProperty(cx, parent, name, OBJECT_TO_JSVAL(array)
		,NULL,NULL,flags));
}

/* Convert from Synchronet-specific jsMethodSpec to JSAPI's JSFunctionSpec */

JSBool
DLLCALL js_DescribeObject(JSContext* cx, JSObject* obj, const char* str)
{
	JSString* js_str = JS_NewStringCopyZ(cx, str);

	if(js_str==NULL)
		return(JS_FALSE);

	return(JS_DefineProperty(cx,obj,"_description"
		,STRING_TO_JSVAL(js_str),NULL,NULL,JSPROP_READONLY));
}

JSBool
DLLCALL js_DescribeConstructor(JSContext* cx, JSObject* obj, const char* str)
{
	JSString* js_str = JS_NewStringCopyZ(cx, str);

	if(js_str==NULL)
		return(JS_FALSE);

	return(JS_DefineProperty(cx,obj,"_constructor"
		,STRING_TO_JSVAL(js_str),NULL,NULL,JSPROP_READONLY));
}

#ifdef _DEBUG

static const char* method_array_name = "_method_list";

/*
 * from jsatom.c:
 * Keep this in sync with jspubtd.h -- an assertion below will insist that
 * its length match the JSType enum's JSTYPE_LIMIT limit value.
 */
static const char *js_type_str[] = {
    "void",			// changed from "undefined"
    "object",
    "function",
    "string",
    "number",
    "boolean",
	"array",		// JSTYPE_LIMIT
};

JSBool 
DLLCALL js_DefineMethods(JSContext* cx, JSObject* obj, jsMethodSpec *funcs)
{
	int			i;
	jsuint		len=0;
	jsval		val;
	JSObject*	method;
	JSObject*	method_array;
	JSString*	js_str;

	/* Return existing method_list array if it's already been created */
	if(JS_GetProperty(cx,obj,method_array_name,&val) && val!=JSVAL_VOID)
		method_array=JSVAL_TO_OBJECT(val);
	else
		if((method_array=JS_NewArrayObject(cx, 0, NULL))==NULL) 
			return(JS_FALSE);

	if(!JS_GetArrayLength(cx, method_array, &len))
		return(JS_FALSE);

	for(i=0;funcs[i].name;i++) {

		JS_DefineFunction(cx, obj, funcs[i].name, funcs[i].call, funcs[i].nargs, 0);

		if(funcs[i].type==JSTYPE_ALIAS)
			continue;

		method = JS_NewObject(cx, NULL, NULL, method_array);

		if(method==NULL)
			return(JS_FALSE);

		if(funcs[i].name!=NULL) {
			if((js_str=JS_NewStringCopyZ(cx,funcs[i].name))==NULL)
				return(JS_FALSE);
			val = STRING_TO_JSVAL(js_str);
			JS_SetProperty(cx, method, "name", &val);
		}

		val = INT_TO_JSVAL(funcs[i].nargs);
		if(!JS_SetProperty(cx, method, "nargs", &val))
			return(JS_FALSE);

		if((js_str=JS_NewStringCopyZ(cx,js_type_str[funcs[i].type]))==NULL)
			return(JS_FALSE);
		val = STRING_TO_JSVAL(js_str);
		JS_SetProperty(cx, method, "type", &val);

		if(funcs[i].args!=NULL) {
			if((js_str=JS_NewStringCopyZ(cx,funcs[i].args))==NULL)
				return(JS_FALSE);
			val = STRING_TO_JSVAL(js_str);
			JS_SetProperty(cx, method, "args", &val);
		}

		if(funcs[i].desc!=NULL) { 
			if((js_str=JS_NewStringCopyZ(cx,funcs[i].desc))==NULL)
				return(JS_FALSE);
			val = STRING_TO_JSVAL(js_str);
			JS_SetProperty(cx, method, "desc", &val);
		}

		val=OBJECT_TO_JSVAL(method);
		if(!JS_SetElement(cx, method_array, len+i, &val))
			return(JS_FALSE);
	}

	if(!JS_DefineProperty(cx, obj, method_array_name, OBJECT_TO_JSVAL(method_array)
		, NULL, NULL, 0))
		return(JS_FALSE);

	return(JS_TRUE);
}

#else // NON-DEBUG

JSBool 
DLLCALL js_DefineMethods(JSContext* cx, JSObject* obj, jsMethodSpec *funcs)
{
	int			i;

	for(i=0;funcs[i].name;i++)
		JS_DefineFunction(cx, obj, funcs[i].name, funcs[i].call, funcs[i].nargs, 0);
	return(JS_TRUE);
}

#endif

/* 
 * @method: log
 * @syntax: log([value][,value][...])
 * @arg:	value Variable or constant of any type
 * @desc:	Print one or more values (typically Strings) in the local log window/display.
 */
static JSBool
js_log(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN		i;
    JSString *	str;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

    for (i = 0; i < argc; i++) {
		if((str=JS_ValueToString(cx, argv[i]))==NULL)
		    return(JS_FALSE);
		if(sbbs->online==ON_LOCAL) {
			if(startup!=NULL && startup->event_log!=NULL)
				startup->event_log(JS_GetStringBytes(str));
		} else
			lputs(JS_GetStringBytes(str));
		}

	*rval = JSVAL_VOID;
    return(JS_TRUE);
}

/*
 * @method: print
 * @usage: print([value][,value][...])
 */

static JSBool
js_print(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN		i;
    JSString *	str;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

    for (i = 0; i < argc; i++) {
		if((str=JS_ValueToString(cx, argv[i]))==NULL)
		    return(JS_FALSE);
		if(sbbs->online==ON_LOCAL)
			eprintf("%s",JS_GetStringBytes(str));
		else
			sbbs->bputs(JS_GetStringBytes(str));
		}
	if(sbbs->online==ON_REMOTE)
		sbbs->bputs(crlf);

	*rval = JSVAL_VOID;
    return(JS_TRUE);
}

static JSBool
js_printf(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
    uintN		i;
	JSString *	fmt;
    JSString *	str;
	sbbs_t*		sbbs;
	va_list		arglist[64];

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((fmt = JS_ValueToString(cx, argv[0]))==NULL)
		return(JS_FALSE);

	memset(arglist,0,sizeof(arglist));	// Initialize arglist to NULLs

    for (i = 1; i < argc && i<sizeof(arglist)/sizeof(arglist[0]); i++) {
		if(JSVAL_IS_STRING(argv[i])) {
			if((str=JS_ValueToString(cx, argv[i]))==NULL)
			    return(JS_FALSE);
			arglist[i-1]=JS_GetStringBytes(str);
		} 
		else if(JSVAL_IS_DOUBLE(argv[i]))
			arglist[i-1]=(char*)(unsigned long)*JSVAL_TO_DOUBLE(argv[i]);
		else if(JSVAL_IS_INT(argv[i]) || JSVAL_IS_BOOLEAN(argv[i]))
			arglist[i-1]=(char *)JSVAL_TO_INT(argv[i]);
		else
			arglist[i-1]=NULL;
	}
	
	if((p=JS_vsmprintf(JS_GetStringBytes(fmt),(char*)arglist))==NULL)
		return(JS_FALSE);

	if(sbbs->online==ON_LOCAL)
		eprintf("%s",p);
	else
		sbbs->bputs(p);
	JS_smprintf_free(p);

	*rval = JSVAL_VOID;
    return(JS_TRUE);
}

static JSBool
js_alert(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString *	str;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((str=JS_ValueToString(cx, argv[0]))==NULL)
	    return(JS_FALSE);

	sbbs->attr(sbbs->cfg.color[clr_err]);
	sbbs->bputs(JS_GetStringBytes(str));
	sbbs->attr(LIGHTGRAY);
	sbbs->bputs(crlf);

	*rval = JSVAL_VOID;
    return(JS_TRUE);
}

static JSBool
js_confirm(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString *	str;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((str=JS_ValueToString(cx, argv[0]))==NULL)
	    return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(sbbs->yesno(JS_GetStringBytes(str)));
	return(JS_TRUE);
}

static JSBool
js_prompt(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char		instr[81];
    JSString *	prompt;
    JSString *	str;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((prompt=JS_ValueToString(cx, argv[0]))==NULL)
	    return(JS_FALSE);

	if(argc>1) {
		if((str=JS_ValueToString(cx, argv[1]))==NULL)
		    return(JS_FALSE);
		SAFECOPY(instr,JS_GetStringBytes(str));
	} else
		instr[0]=0;

	sbbs->bprintf("\1n\1y\1h%s\1w: ",JS_GetStringBytes(prompt));

	if(!sbbs->getstr(instr,sizeof(instr)-1,K_EDIT)) {
		*rval = JSVAL_NULL;
		return(JS_TRUE);
	}

	if((str=JS_NewStringCopyZ(cx, instr))==NULL)
	    return(JS_FALSE);

	*rval = STRING_TO_JSVAL(str);
    return(JS_TRUE);
}

static jsMethodSpec js_global_functions[] = {
	{"log",				js_log,				1,	JSTYPE_VOID,	JSDOCSTR("string text [,text]")
	,JSDOCSTR("Log a string")
	},
    {"print",           js_print,           0,	JSTYPE_VOID,	JSDOCSTR("string text [,test]")
	,JSDOCSTR("Print a string, auto-crlf")
	},
    {"printf",          js_printf,          1,	JSTYPE_VOID,	JSDOCSTR("string format [,value][,value]")
	,JSDOCSTR("Print a formatted string")
	},	
	{"alert",			js_alert,			1,	JSTYPE_VOID,	JSDOCSTR("string text")
	,JSDOCSTR("Print an alert message (ala client-side)")
	},
	{"prompt",			js_prompt,			1,	JSTYPE_STRING,	JSDOCSTR("string text")
	,JSDOCSTR("Prompt for a user string  (ala clent-side)")
	},
	{"confirm",			js_confirm,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("string text")
	,JSDOCSTR("Confirm a question (ala client-side)")
	},
    {0}
};

static void
js_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
	char	line[64];
	char	file[MAX_PATH+1];
	sbbs_t*	sbbs;
	const char*	warning;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return;
	
	if(report==NULL) {
		lprintf("!JavaScript: %s", message);
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
	} else
		warning=nulstr;

	if(sbbs->online==ON_LOCAL) 
		eprintf("!JavaScript %s%s%s: %s",warning,file,line,message);
	else {
		lprintf("!JavaScript %s%s%s: %s",warning,file,line,message);
		sbbs->bprintf("!JavaScript %s%s%s: %s\r\n",warning,file,line,message);
	}
}

static JSClass js_server_class = {
        "TelnetServer",0, 
        JS_PropertyStub,JS_PropertyStub,JS_PropertyStub,JS_PropertyStub, 
        JS_EnumerateStub,JS_ResolveStub,JS_ConvertStub,JS_FinalizeStub 
}; 

bool sbbs_t::js_init()
{
	char		node[128];
	char		ver[256];
	jsval		val;
	JSObject*	server;
	JSString*	js_str;

    if(cfg.node_num)
    	sprintf(node,"Node %d",cfg.node_num);
    else
    	strcpy(node,client_name);

	lprintf("%s JavaScript: Creating runtime: %lu bytes"
		,node,startup->js_max_bytes);

	if((js_runtime = JS_NewRuntime(startup->js_max_bytes))==NULL)
		return(false);

	lprintf("%s JavaScript: Initializing context (stack: %lu bytes)"
		,node,JAVASCRIPT_CONTEXT_STACK);

    if((js_cx = JS_NewContext(js_runtime, JAVASCRIPT_CONTEXT_STACK))==NULL)
		return(false);

	js_loop = 0;	/* loop counter */

	bool success=false;

	do {

		JS_SetErrorReporter(js_cx, js_ErrorReporter);

		JS_SetContextPrivate(js_cx, this);	/* Store a pointer to sbbs_t instance */

		/* Global Object */
		if((js_glob=js_CreateGlobalObject(js_cx, &cfg))==NULL)
			break;

		if(!js_DefineMethods(js_cx, js_glob, js_global_functions))
			break;

#ifdef _DEBUG
		JS_DefineProperty(js_cx, js_glob, "_global", OBJECT_TO_JSVAL(js_glob)
			,NULL,NULL,JSPROP_READONLY);
#endif

		/* System Object */
		if(js_CreateSystemObject(js_cx, js_glob, &cfg, uptime, startup->host_name)==NULL)
			break;

		/* Client Object */
		if(js_CreateClientObject(js_cx, js_glob, "client", &client, client_socket)==NULL)
			break;

		/* BBS Object */
		if(js_CreateBbsObject(js_cx, js_glob)==NULL)
			break;

		/* Console Object */
		if(js_CreateConsoleObject(js_cx, js_glob)==NULL)
			break;

		/* Socket Class */
		if(js_CreateSocketClass(js_cx, js_glob)==NULL)
			break;

		/* MsgBase Class */
		if(js_CreateMsgBaseClass(js_cx, js_glob, &scfg)==NULL)
			break;

		/* File Class */
		if(js_CreateFileClass(js_cx, js_glob)==NULL)
			break;

		/* User class */
		if(js_CreateUserClass(js_cx, js_glob, &cfg)==NULL) 
			break;

		/* Area Objects */
		if(!js_CreateUserObjects(js_cx, js_glob, &scfg, NULL, NULL, NULL)) 
			break;

		/* Server Object */
		if((server=JS_DefineObject(js_cx, js_glob, "server", &js_server_class
			,NULL,JSPROP_ENUMERATE))==NULL)
			break;

		js_DescribeObject(js_cx,server,"Server-specifc properties");

		sprintf(ver,"%s %s%c",TELNET_SERVER,VERSION,REVISION);
		if((js_str=JS_NewStringCopyZ(js_cx, ver))==NULL)
			break;
		val = STRING_TO_JSVAL(js_str);
		if(!JS_SetProperty(js_cx, server, "version", &val))
			break;

		if((js_str=JS_NewStringCopyZ(js_cx, bbs_ver()))==NULL)
			break;
		val = STRING_TO_JSVAL(js_str);
		if(!JS_SetProperty(js_cx, server, "version_detail", &val))
			break;


		success=true;

	} while(0);

	if(!success) {
		JS_DestroyContext(js_cx);
		js_cx=NULL;
		return(false);
	}

	return(true);
}

void sbbs_t::js_create_user_objects(void)
{
	if(js_cx==NULL)
		return;

	if(!js_CreateUserObjects(js_cx, js_glob, &cfg, &useron, NULL, subscan)) 
		lprintf("!JavaScript ERROR creating user objects");
}

#endif	/* JAVASCRIPT */

#ifdef _WINSOCKAPI_

WSADATA WSAData;
static BOOL WSAInitialized=FALSE;

static BOOL winsock_startup(void)
{
	int		status;             /* Status Code */

    if((status = WSAStartup(MAKEWORD(1,1), &WSAData))==0) {
		lprintf("%s %s",WSAData.szDescription, WSAData.szSystemStatus);
		WSAInitialized=TRUE;
		return(TRUE);
	}

    lprintf("!WinSock startup ERROR %d", status);
	return(FALSE);
}

#else /* No WINSOCK */

#define winsock_startup()	(TRUE)	

#endif

static BYTE* telnet_interpret(sbbs_t* sbbs, BYTE* inbuf, int inlen,
  									BYTE* outbuf, int& outlen)
{
	BYTE*   first_iac=NULL;
	BYTE*	first_cr=NULL;
	int 	i;

    first_iac=(BYTE*)memchr(inbuf, TELNET_IAC, inlen);

	if(!(sbbs->telnet_mode&(TELNET_MODE_BIN_RX|TELNET_MODE_GATE)) 
		&& !(sbbs->console&CON_RAW_IN)) {
		if(sbbs->telnet_last_rxch==CR)
			first_cr=inbuf;
		else
			first_cr=(BYTE*)memchr(inbuf, CR, inlen);
	}

    if(!sbbs->telnet_cmdlen	&& first_iac==NULL && first_cr==NULL) {
        outlen=inlen;
        return(inbuf);	// no interpretation needed
    }

    if(first_iac!=NULL || first_cr!=NULL) {
		if(first_iac!=NULL && (first_cr==NULL || first_iac<first_cr))
   			outlen=first_iac-inbuf;
		else
			outlen=first_cr-inbuf;
	    memcpy(outbuf, inbuf, outlen);
    } else
    	outlen=0;

    for(i=outlen;i<inlen;i++) {
		if(!(sbbs->telnet_mode&(TELNET_MODE_BIN_RX|TELNET_MODE_GATE)) 
			&& !(sbbs->console&CON_RAW_IN)) {
			if(sbbs->telnet_last_rxch==CR
				&& (inbuf[i]==LF || inbuf[i]==0)) { // CR/LF or CR/NUL, ignore 2nd char
#if 0 /* Debug CR/LF problems */
				lprintf("Node %d CR/%02Xh detected and ignored"
					,sbbs->cfg.node_num, inbuf[i]);
#endif
				sbbs->telnet_last_rxch=inbuf[i];
				continue;
			}
			sbbs->telnet_last_rxch=inbuf[i];
		}

        if(inbuf[i]==TELNET_IAC && sbbs->telnet_cmdlen==1) { /* escaped 255 */
            sbbs->telnet_cmdlen=0;
            outbuf[outlen++]=TELNET_IAC;
            continue;
        }
        if(inbuf[i]==TELNET_IAC || sbbs->telnet_cmdlen) {
            sbbs->telnet_cmd[sbbs->telnet_cmdlen++]=inbuf[i];
            if(sbbs->telnet_cmdlen==2 && inbuf[i]<TELNET_WILL) {
				if(startup->options&BBS_OPT_DEBUG_TELNET)
            		lprintf("Node %d %s telnet cmd: %s"
	                	,sbbs->cfg.node_num
						,sbbs->telnet_mode&TELNET_MODE_GATE ? "passed-through" : "received"
                		,telnet_cmd_desc(sbbs->telnet_cmd[2]));
                sbbs->telnet_cmdlen=0;
            }
            else if(sbbs->telnet_cmdlen>=3) {
				if(sbbs->telnet_cmd[2]==TELNET_BINARY) {
					if(sbbs->telnet_cmd[1]==TELNET_WILL)
						sbbs->telnet_mode|=TELNET_MODE_BIN_RX;
					else if(sbbs->telnet_cmd[1]==TELNET_WONT)
						sbbs->telnet_mode&=~TELNET_MODE_BIN_RX;
				}
				if(sbbs->telnet_cmd[2]==TELNET_ECHO) {
					if(sbbs->telnet_cmd[1]==TELNET_DO)
						sbbs->telnet_mode|=TELNET_MODE_ECHO;
					else if(sbbs->telnet_cmd[1]==TELNET_DONT) {
						sbbs->telnet_mode&=~TELNET_MODE_ECHO;
						if(!(sbbs->telnet_mode&TELNET_MODE_GATE))
							sbbs->send_telnet_cmd(TELNET_WILL,TELNET_ECHO);
					}
				}
				if(startup->options&BBS_OPT_DEBUG_TELNET)
					lprintf("Node %d %s telnet cmd: %s %s"
	                    ,sbbs->cfg.node_num
						,sbbs->telnet_mode&TELNET_MODE_GATE ? "passed-through" : "received"
						,telnet_cmd_desc(sbbs->telnet_cmd[1])
						,telnet_opt_desc(sbbs->telnet_cmd[2]));
                sbbs->telnet_cmdlen=0;
            }
			if(sbbs->telnet_mode&TELNET_MODE_GATE)	// Pass-through commads
				outbuf[outlen++]=inbuf[i];
        } else
        	outbuf[outlen++]=inbuf[i];
    }
    return(outbuf);
}

void sbbs_t::send_telnet_cmd(uchar cmd, uchar opt)
{
	char buf[16];

	if(cmd<TELNET_WILL) {
		if(startup->options&BBS_OPT_DEBUG_TELNET)
            lprintf("Node %d sending telnet cmd: %s"
	            ,cfg.node_num
                ,telnet_cmd_desc(cmd));
		sprintf(buf,"%c%c",TELNET_IAC,cmd);
		putcom(buf,2);
	} else {
		if(startup->options&BBS_OPT_DEBUG_TELNET)
			lprintf("Node %d sending telnet cmd: %s %s"
				,cfg.node_num
				,telnet_cmd_desc(cmd)
				,telnet_opt_desc(opt));
		sprintf(buf,"%c%c%c",TELNET_IAC,cmd,opt);
		putcom(buf,3);
	}
}

void input_thread(void *arg)
{
	BYTE		inbuf[4000];
   	BYTE		telbuf[sizeof(inbuf)];
    BYTE		*wrbuf;
    int			i,rd,wr,avail;
	ulong		total_recv=0;
	ulong		total_pkts=0;
	fd_set		socket_set;
	sbbs_t*		sbbs = (sbbs_t*) arg;
	struct timeval	tv;

	thread_up(TRUE /* setuid */);

#ifdef _DEBUG
	lprintf("Node %d input thread started",sbbs->cfg.node_num);
#endif

	pthread_mutex_init(&sbbs->input_thread_mutex,NULL);
    sbbs->input_thread_running = true;
	sbbs->console|=CON_R_INPUT;

	while(sbbs->online && sbbs->client_socket!=INVALID_SOCKET
		&& node_socket[sbbs->cfg.node_num-1]!=INVALID_SOCKET) {

		pthread_mutex_lock(&sbbs->input_thread_mutex);

		FD_ZERO(&socket_set);
		FD_SET(sbbs->client_socket,&socket_set);
	
		tv.tv_sec=1;
		tv.tv_usec=0;

		if((i=select(sbbs->client_socket+1,&socket_set,NULL,NULL,&tv))<1) {
			pthread_mutex_unlock(&sbbs->input_thread_mutex);
			if(i==0) {
				mswait(1);
				continue;
			}

        	if(ERROR_VALUE == ENOTSOCK)
                lprintf("Node %d socket closed by peer on input->select", sbbs->cfg.node_num);
			else if(ERROR_VALUE==ESHUTDOWN)
				lprintf("Node %d socket shutdown on input->select", sbbs->cfg.node_num);
			else if(ERROR_VALUE==EINTR)
				lprintf("Node %d input thread interrupted",sbbs->cfg.node_num);
            else if(ERROR_VALUE==ECONNRESET) 
				lprintf("Node %d connection reset by peer on input->select", sbbs->cfg.node_num);
            else if(ERROR_VALUE==ECONNABORTED) 
				lprintf("Node %d connection aborted by peer on input->select", sbbs->cfg.node_num);
			else
				lprintf("Node %d !ERROR %d input->select socket %d"
                	,sbbs->cfg.node_num, ERROR_VALUE, sbbs->client_socket);
			break;
		}

		if(sbbs->client_socket==INVALID_SOCKET) {
			pthread_mutex_unlock(&sbbs->input_thread_mutex);
			break;
		}

    	rd=RingBufFree(&sbbs->inbuf);

		if(!rd) { // input buffer full
        	// wait up to 5 seconds to empty (1 byte min)
			time_t start=time(NULL);
            while((rd=RingBufFree(&sbbs->inbuf))==0) {
            	if(time(NULL)-start>=5) {
                	rd=1;
                	break;
                }
                mswait(1);
            }
		}
		
	    if(rd > (int)sizeof(inbuf))
        	rd=sizeof(inbuf);

    	rd = recv(sbbs->client_socket, (char*)inbuf, rd, 0);

		pthread_mutex_unlock(&sbbs->input_thread_mutex);

		if(rd == SOCKET_ERROR)
		{
        	if(ERROR_VALUE == ENOTSOCK)
                lprintf("Node %d socket closed by peer on receive", sbbs->cfg.node_num);
            else if(ERROR_VALUE==ECONNRESET) 
				lprintf("Node %d connection reset by peer on receive", sbbs->cfg.node_num);
			else if(ERROR_VALUE==ESHUTDOWN)
				lprintf("Node %d socket shutdown on receive", sbbs->cfg.node_num);
            else if(ERROR_VALUE==ECONNABORTED) 
				lprintf("Node %d connection aborted by peer on receive", sbbs->cfg.node_num);
			else
				lprintf("Node %d !ERROR %d receiving from socket %d"
                	,sbbs->cfg.node_num, ERROR_VALUE, sbbs->client_socket);
			break;
		}

		if(rd == 0)
		{
			lprintf("Node %d disconnected", sbbs->cfg.node_num);
			break;
		}

		total_recv+=rd;
		total_pkts++;

        // telbuf and wr are modified to reflect telnet escaped data
		wrbuf=telnet_interpret(sbbs, inbuf, rd, telbuf, wr);
		if(wr > (int)sizeof(telbuf)) 
			lprintf("!TELBUF OVERFLOW (%d>%d)",wr,sizeof(telbuf));

		/* First level Ctrl-C checking */
		if(!(sbbs->cfg.ctrlkey_passthru&(1<<CTRL_C))
			&& sbbs->rio_abortable 
			&& !(sbbs->telnet_mode&(TELNET_MODE_BIN_RX|TELNET_MODE_GATE))
			&& memchr(wrbuf, CTRL_C, wr)) {	
			if(RingBufFull(&sbbs->inbuf))
    			lprintf("Node %d Ctrl-C hit with %lu bytes in input buffer"
					,sbbs->cfg.node_num,RingBufFull(&sbbs->inbuf));
			if(RingBufFull(&sbbs->outbuf))
    			lprintf("Node %d Ctrl-C hit with %lu bytes in output buffer"
					,sbbs->cfg.node_num,RingBufFull(&sbbs->outbuf));
			sbbs->sys_status|=SS_ABORT;
			RingBufReInit(&sbbs->inbuf);	/* Purge input buffer */
    		RingBufReInit(&sbbs->outbuf);	/* Purge output buffer */
			continue;	// Ignore the entire buffer
		}

		avail=RingBufFree(&sbbs->inbuf);

        if(avail<wr)
			lprintf("!INPUT BUFFER FULL (%d free)", avail);
        else
			RingBufWrite(&sbbs->inbuf, wrbuf, wr);
//		if(wr>100)
//			mswait(500);	// Throttle sender
	}
	sbbs->online=0;

    sbbs->input_thread_running = false;
	if(node_socket[sbbs->cfg.node_num-1]==INVALID_SOCKET)	// Shutdown locally
		sbbs->terminated = true;	// Signal JS to stop execution

	pthread_mutex_destroy(&sbbs->input_thread_mutex);

	thread_down();
	lprintf("Node %d input thread terminated (received %lu bytes in %lu packets)"
		,sbbs->cfg.node_num, total_recv, total_pkts);
}

void output_thread(void* arg)
{
	char		node[128];
	char		stats[128];
    BYTE		buf[IO_THREAD_BUF_SIZE];
	int			i;
    ulong		avail;
	ulong		total_sent=0;
	ulong		total_pkts=0;
    ulong		bufbot=0;
    ulong		buftop=0;
	sbbs_t*		sbbs = (sbbs_t*) arg;
	fd_set		socket_set;
	struct timeval tv;

	thread_up(TRUE /* setuid */);

    if(sbbs->cfg.node_num)
    	sprintf(node,"Node %d",sbbs->cfg.node_num);
    else
    	strcpy(node,sbbs->client_name);
#ifdef _DEBUG
	lprintf("%s output thread started",node);
#endif

    sbbs->output_thread_running = true;
	sbbs->console|=CON_R_ECHO;

	while(sbbs->client_socket!=INVALID_SOCKET && telnet_socket!=INVALID_SOCKET) {
    	if(bufbot==buftop)
	    	avail=RingBufFull(&sbbs->outbuf);
        else
        	avail=buftop-bufbot;

		if(!avail) {
/** 
    This caused occasional segfaults on Linux and *could* have been the cause
    of blocked output on Win32 (what was I thinking?)
			sem_init(&sbbs->output_sem,0,0);
**/
			sem_wait(&sbbs->output_sem);
			continue; 
		}

		/* Check socket for writability (using select) */
		tv.tv_sec=0;
		tv.tv_usec=0;

		FD_ZERO(&socket_set);
		FD_SET(sbbs->client_socket,&socket_set);

		i=select(sbbs->client_socket+1,NULL,&socket_set,NULL,&tv);
		if(i==SOCKET_ERROR) {
			lprintf("!%s: ERROR %d selecting socket %u for send"
				,node,ERROR_VALUE,sbbs->client_socket);
			if(sbbs->cfg.node_num)	/* Only break if node output (not server) */
				break;
			RingBufReInit(&sbbs->outbuf);	/* Flush output buffer */
			mswait(1);
			continue;
		}
		if(i<1) {
			mswait(1);
			continue;
		}

        if(bufbot==buftop) { // linear buf empty, read from ring buf
            if(avail>sizeof(buf)) {
                lprintf("Reducing output buffer");
                avail=sizeof(buf);
            }
            buftop=RingBufRead(&sbbs->outbuf, buf, avail);
            bufbot=0;
        }
		i=sendsocket(sbbs->client_socket, (char*)buf+bufbot, buftop-bufbot);
		if(i==SOCKET_ERROR) {
        	if(ERROR_VALUE == ENOTSOCK)
                lprintf("%s client socket closed on send", node);
            else if(ERROR_VALUE==ECONNRESET) 
				lprintf("%s connection reset by peer on send", node);
            else if(ERROR_VALUE==ECONNABORTED) 
				lprintf("%s connection aborted by peer on send", node);
			else
				lprintf("!%s: ERROR %d sending on socket %d"
                	,node, ERROR_VALUE, sbbs->client_socket);
			sbbs->online=0;
			/* was break; on 4/7/00 */
			i=buftop-bufbot;	// Pretend we sent it all
		}

		if(sbbs->cfg.node_num>0 && !(sbbs->sys_status&SS_FILEXFER)) {
			/* Spy on the user locally */
			if(startup->node_spybuf!=NULL 
				&& startup->node_spybuf[sbbs->cfg.node_num-1]!=NULL) {
				RingBufWrite(startup->node_spybuf[sbbs->cfg.node_num-1],buf+bufbot,i);
				/* Signal spy output semaphore? */
				if(startup->node_spysem!=NULL 
					&& startup->node_spysem[sbbs->cfg.node_num-1]!=NULL)
					sem_post(startup->node_spysem[sbbs->cfg.node_num-1]);
			}
			/* Spy on the user remotely */
			if(spy_socket[sbbs->cfg.node_num-1]!=INVALID_SOCKET) 
				sendsocket(spy_socket[sbbs->cfg.node_num-1],(char*)buf+bufbot,i);
		}

		bufbot+=i;
		total_sent+=i;
		total_pkts++;
    }

	sbbs->spymsg("Disconnected");

    sbbs->output_thread_running = false;

	if(total_sent)
		sprintf(stats,"(sent %lu bytes in %lu packets)"
			,total_sent, total_pkts);
	else
		stats[0]=0;

	thread_down();
	lprintf("%s output thread terminated %s", node, stats);
}

void event_thread(void* arg)
{
	char		str[MAX_PATH+1];
	char		bat_list[MAX_PATH+1];
	char		semfile[MAX_PATH+1];
	int			i,j,k;
	int			file;
	int			offset;
	bool		check_semaphores;
	ulong		l;
	time_t		now;
	time_t		start;
	time_t		lastsemchk=0;
	time_t		lastnodechk=0;
	time_t		lastprepack=0;
	node_t		node;
	glob_t		g;
	sbbs_t*		sbbs = (sbbs_t*) arg;
	struct tm	now_tm;
	struct tm	tm;

	eprintf("BBS Events thread started");

	sbbs->event_thread_running = true;

	srand(time(NULL));	/* Seed random number generator */
	sbbs_random(10);	/* Throw away first number */

	thread_up(TRUE /* setuid */);

#ifdef JAVASCRIPT
	if(!(startup->options&BBS_OPT_NO_JAVASCRIPT)) {
		if(!sbbs->js_init())	/* This must be done in the context of the node thread */
			lprintf("!JavaScript Initialization FAILURE");
	}
#endif

	while(!sbbs->terminated && telnet_socket!=INVALID_SOCKET) {

		now=time(NULL);
		localtime_r(&now,&now_tm);

		if(now-lastsemchk>=sbbs->cfg.node_sem_check) {
			check_semaphores=true;
			lastsemchk=now;
		} else
			check_semaphores=false;

		pthread_mutex_lock(&event_mutex);

		sbbs->online=0;	/* reset this from ON_LOCAL */

		if(scfg_reloaded==true) {

			for(i=0;i<TOTAL_TEXT;i++)
				sbbs->text[i]=sbbs->text_sav[i]=text[i];

			memcpy(&sbbs->cfg,&scfg,sizeof(scfg_t));

//			sprintf(sbbs->cfg.temp_dir, "temp/%08lX", sbbs_random(INT_MAX));
			prep_dir(sbbs->cfg.data_dir, sbbs->cfg.temp_dir);

			// Read TIME.DAB
			sprintf(str,"%stime.dab",sbbs->cfg.ctrl_dir);
			if((file=sbbs->nopen(str,O_RDWR|O_CREAT))==-1) {
				sbbs->errormsg(WHERE,ERR_OPEN,str,0);
				break; 
			}
			for(i=0;i<sbbs->cfg.total_events;i++) {
				sbbs->cfg.event[i]->last=0;
				if(filelength(file)<(long)(sizeof(time_t)*(i+1)))
					write(file,&sbbs->cfg.event[i]->last,sizeof(time_t));
				else
					read(file,&sbbs->cfg.event[i]->last,sizeof(time_t)); 
			}
			read(file,&lastprepack,sizeof(time_t));
			close(file);

			// Read QNET.DAB
			sprintf(str,"%sqnet.dab",sbbs->cfg.ctrl_dir);
			if((file=sbbs->nopen(str,O_RDWR|O_CREAT))==-1) {
				sbbs->errormsg(WHERE,ERR_OPEN,str,0);
				return;
			}
			for(i=0;i<sbbs->cfg.total_qhubs;i++) {
				sbbs->cfg.qhub[i]->last=0;
				if(filelength(file)<(long)(sizeof(time_t)*(i+1)))
					write(file,&sbbs->cfg.qhub[i]->last,sizeof(time_t));
				else
					read(file,&sbbs->cfg.qhub[i]->last,sizeof(time_t)); 
			}
			close(file);

			// Read PNET.DAB
			sprintf(str,"%spnet.dab",sbbs->cfg.ctrl_dir);
			if((file=sbbs->nopen(str,O_RDWR|O_CREAT))==-1) {
				sbbs->errormsg(WHERE,ERR_OPEN,str,0);
				break;
			}
			for(i=0;i<sbbs->cfg.total_phubs;i++) {
				sbbs->cfg.phub[i]->last=0;
				if(filelength(file)<(long)(sizeof(time_t)*(i+1)))
					write(file,&sbbs->cfg.phub[i]->last,sizeof(time_t));
				else
					read(file,&sbbs->cfg.phub[i]->last,sizeof(time_t)); 
			}
			close(file);

			scfg_reloaded=false;
		}

		/* QWK events */
		if(check_semaphores && !(startup->options&BBS_OPT_NO_QWK_EVENTS)) {
			/* Import any REP files that have magically appeared (via FTP perhaps) */
			sprintf(str,"%sfile/",sbbs->cfg.data_dir);
			offset=strlen(str);
			strcat(str,"*.rep");
			glob(str,0,NULL,&g);
			for(i=0;i<(int)g.gl_pathc;i++) {
				sbbs->useron.number=atoi(g.gl_pathv[i]+offset);
				getuserdat(&sbbs->cfg,&sbbs->useron);
				if(sbbs->useron.number && flength(g.gl_pathv[i])>0) {
					sbbs->online=ON_LOCAL;
					eprintf("Un-packing QWK Reply packet from %s",sbbs->useron.alias);
					sbbs->getusrsubs();
					sbbs->unpack_rep(g.gl_pathv[i]);
					sbbs->batch_create_list();	/* FREQs? */
					sbbs->batdn_total=0;
					
					/* putuserdat? */
					remove(g.gl_pathv[i]);
				}
			}
			globfree(&g);

			/* Create any QWK files that have magically appeared (via FTP perhaps) */
			sprintf(str,"%spack*.now",sbbs->cfg.data_dir);
			offset=strlen(sbbs->cfg.data_dir)+4;
			glob(str,0,NULL,&g);
			for(i=0;i<(int)g.gl_pathc;i++) {
				sbbs->useron.number=atoi(g.gl_pathv[i]+offset);
				getuserdat(&sbbs->cfg,&sbbs->useron);
				if(sbbs->useron.number && !(sbbs->useron.misc&(DELETED|INACTIVE))) {
					eprintf("Packing QWK Message Packet for %s",sbbs->useron.alias);
					sbbs->online=ON_LOCAL;
					delfiles(sbbs->cfg.temp_dir,ALLFILES);
					sbbs->getmsgptrs();
					sbbs->getusrsubs();
					sbbs->batdn_total=0;

					sbbs->last_ns_time=sbbs->ns_time=sbbs->useron.ns_time;
					sprintf(bat_list,"%sfile/%04u.dwn",sbbs->cfg.data_dir,sbbs->useron.number);
					sbbs->batch_add_list(bat_list);

					sprintf(str,"%sfile%c%04u.qwk"
						,sbbs->cfg.data_dir,BACKSLASH,sbbs->useron.number);
					if(sbbs->pack_qwk(str,&l,true /* pre-pack/off-line */)) {
						eprintf("Packing completed");
						sbbs->qwk_success(l,0,1);
						sbbs->putmsgptrs(); 
						remove(bat_list);
					} else
						eprintf("No packet created (no new messages)");
					delfiles(sbbs->cfg.temp_dir,ALLFILES);
					sbbs->online=0;
				}
				remove(g.gl_pathv[i]);
			}
			globfree(&g);

			/* Create (pre-pack) QWK files for users configured as such */
			sprintf(semfile,"%sprepack.now",sbbs->cfg.data_dir);
			if(sbbs->cfg.preqwk_ar[0] 
				&& (fexistcase(semfile) || (now-lastprepack)/60>(60*24))) {
				j=lastuser(&sbbs->cfg);
				eprintf("Pre-packing QWK Message packets...");
				for(i=1;i<=j;i++) {

					sprintf(str,"%5u of %-5u",i,j);
					//status(str);
					sbbs->useron.number=i;
					getuserdat(&sbbs->cfg,&sbbs->useron);

					if(sbbs->useron.number
						&& !(sbbs->useron.misc&(DELETED|INACTIVE))	 /* Pre-QWK */
						&& sbbs->chk_ar(sbbs->cfg.preqwk_ar,&sbbs->useron)) { 
						for(k=1;k<=sbbs->cfg.sys_nodes;k++) {
							if(sbbs->getnodedat(k,&node,0)!=0)
								continue;
							if((node.status==NODE_INUSE || node.status==NODE_QUIET
								|| node.status==NODE_LOGON) && node.useron==i)
								break; 
						}
						if(k<=sbbs->cfg.sys_nodes)	/* Don't pre-pack with user online */
							continue;
						eprintf("Pre-packing QWK for %s",sbbs->useron.alias);
						sbbs->online=ON_LOCAL;
						delfiles(sbbs->cfg.temp_dir,ALLFILES);
						sbbs->getmsgptrs();
						sbbs->getusrsubs();
						sbbs->batdn_total=0;
						sprintf(str,"%sfile%c%04u.qwk"
							,sbbs->cfg.data_dir,BACKSLASH,sbbs->useron.number);
						if(sbbs->pack_qwk(str,&l,true /* pre-pack */)) {
							sbbs->qwk_success(l,0,1);
							sbbs->putmsgptrs(); 
						}
						delfiles(sbbs->cfg.temp_dir,ALLFILES);
						sbbs->online=0;
					} 
				}
				lastprepack=now;
				sprintf(str,"%stime.dab",sbbs->cfg.ctrl_dir);
				if((file=sbbs->nopen(str,O_WRONLY))==-1) {
					sbbs->errormsg(WHERE,ERR_OPEN,str,O_WRONLY);
					break; 
				}
				lseek(file,(long)sbbs->cfg.total_events*4L,SEEK_SET);
				write(file,&lastprepack,sizeof(time_t));
				close(file);

				remove(semfile);
				//status(STATUS_WFC);
			}
		}

		if(check_semaphores) {
			/* Node Daily Events */
			for(i=first_node;i<=last_node;i++) {
				// Node Daily Event
				node.status=NODE_INVALID_STATUS;
				if(sbbs->getnodedat(i,&node,0)!=0)
					continue;
				if(node.misc&NODE_EVENT && node.status==NODE_WFC) {
					sbbs->getnodedat(i,&node,1);
					node.status=NODE_EVENT_RUNNING;
					sbbs->putnodedat(i,&node);
					if(sbbs->cfg.node_daily[0]) {
						sbbs->cfg.node_num=i;
						strcpy(sbbs->cfg.node_dir, sbbs->cfg.node_path[i-1]);

						eprintf("Running node %d daily event",i);
						sbbs->online=ON_LOCAL;
						sbbs->logentry("!:","Run node daily event");
						sbbs->external(
							 sbbs->cmdstr(sbbs->cfg.node_daily,nulstr,nulstr,NULL)
							,EX_OFFLINE);
					}
					sbbs->getnodedat(i,&node,1);
					node.misc&=~NODE_EVENT;
					node.status=NODE_WFC;
					node.useron=0;
					sbbs->putnodedat(i,&node); 
				}
			}

			/* QWK Networking Call-out sempahores */
			for(i=0;i<sbbs->cfg.total_qhubs;i++) {
				if(sbbs->cfg.qhub[i]->node<first_node 
					|| sbbs->cfg.qhub[i]->node>last_node)
					continue;
				if(sbbs->cfg.qhub[i]->last==-1) // already signaled
					continue;
				sprintf(str,"%sqnet/%s.now",sbbs->cfg.data_dir,sbbs->cfg.qhub[i]->id);
				if(fexistcase(str)) {
					strcpy(str,sbbs->cfg.qhub[i]->id);
					eprintf("Semaphore signaled for QWK Network Hub: %s",strupr(str));
					sbbs->cfg.qhub[i]->last=-1; 
				}
			}

			/* Timed Event sempahores */
			for(i=0;i<sbbs->cfg.total_events;i++) {
				if((sbbs->cfg.event[i]->node<first_node
					|| sbbs->cfg.event[i]->node>last_node)
					&& !(sbbs->cfg.event[i]->misc&EVENT_EXCL))
					continue;	// ignore non-exclusive events for other instances
				if(sbbs->cfg.event[i]->last==-1) // already signaled
					continue;
				sprintf(str,"%s%s.now",sbbs->cfg.data_dir,sbbs->cfg.event[i]->code);
				if(fexistcase(str)) {
					strcpy(str,sbbs->cfg.event[i]->code);
					eprintf("Semaphore signaled for Timed Event: %s",strupr(str));
					sbbs->cfg.event[i]->last=-1; 
				}
			}
		}

		/* QWK Networking Call-out Events */
		for(i=0;i<sbbs->cfg.total_qhubs;i++) {
			if(sbbs->cfg.qhub[i]->node<first_node ||
				sbbs->cfg.qhub[i]->node>last_node)
				continue;

			if(check_semaphores) {
				// See if any packets have come in
				for(j=0;j<101;j++) {
					sprintf(str,"%s%s.q%c%c",sbbs->cfg.data_dir,sbbs->cfg.qhub[i]->id
						,j>10 ? ((j-1)/10)+'0' : 'w'
						,j ? ((j-1)%10)+'0' : 'k');
					fexistcase(str);		/* fix wrong-case filenames on Unix */
					if(flength(str)>0) {	/* silently ignore 0-byte QWK packets */
						delfiles(sbbs->cfg.temp_dir,ALLFILES);
						sbbs->online=ON_LOCAL;
						if(sbbs->unpack_qwk(str,i)==false) {
							char newname[MAX_PATH+1];
							sprintf(newname,"%s.%lx.bad",str,(long)now);
							remove(newname);
							if(rename(str,newname)==0) {
								char logmsg[MAX_PATH*3];
								sprintf(logmsg,"%s renamed to %s",str,newname);
								sbbs->logline("Q!",logmsg);
							}
						}
						sbbs->online=0;
						remove(str);
					} 
				}
			}

			/* Qnet call out based on time */
			if(localtime_r(&sbbs->cfg.qhub[i]->last,&tm)==NULL)
				memset(&tm,0,sizeof(tm));
			if((sbbs->cfg.qhub[i]->last==-1L					/* or frequency */
				|| ((sbbs->cfg.qhub[i]->freq
					&& (now-sbbs->cfg.qhub[i]->last)/60>sbbs->cfg.qhub[i]->freq)
					|| (sbbs->cfg.qhub[i]->time
						&& (now_tm.tm_hour*60)+now_tm.tm_min>=sbbs->cfg.qhub[i]->time
						&& (now_tm.tm_mday!=tm.tm_mday || now_tm.tm_mon!=tm.tm_mon)))
						&& sbbs->cfg.qhub[i]->days&(1<<now_tm.tm_wday))) {
				sprintf(str,"%sqnet/%s.now"
					,sbbs->cfg.data_dir,sbbs->cfg.qhub[i]->id);
				if(fexistcase(str))
					remove(str);					/* Remove semaphore file */
				sprintf(str,"%sqnet/%s.ptr"
					,sbbs->cfg.data_dir,sbbs->cfg.qhub[i]->id);
				file=sbbs->nopen(str,O_RDONLY);
				for(j=0;j<sbbs->cfg.qhub[i]->subs;j++) {
					sbbs->subscan[sbbs->cfg.qhub[i]->sub[j]].ptr=0;
					if(file!=-1) {
						lseek(file,sbbs->cfg.sub[sbbs->cfg.qhub[i]->sub[j]]->ptridx*sizeof(long),SEEK_SET);
						read(file,&sbbs->subscan[sbbs->cfg.qhub[i]->sub[j]].ptr,sizeof(long)); 
					}
				}
				if(file!=-1)
					close(file);
				if(sbbs->pack_rep(i)) {
					if((file=sbbs->nopen(str,O_WRONLY|O_CREAT))==-1)
						sbbs->errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT);
					else {
						for(j=l=0;j<sbbs->cfg.qhub[i]->subs;j++) {
							while(filelength(file)<
								sbbs->cfg.sub[sbbs->cfg.qhub[i]->sub[j]]->ptridx*4L)
								write(file,&l,4);		/* initialize ptrs to null */
							lseek(file
								,sbbs->cfg.sub[sbbs->cfg.qhub[i]->sub[j]]->ptridx*sizeof(long)
								,SEEK_SET);
							write(file,&sbbs->subscan[sbbs->cfg.qhub[i]->sub[j]].ptr,sizeof(long)); 
						}
						close(file); 
					} 
				}
				delfiles(sbbs->cfg.temp_dir,ALLFILES);

				sbbs->cfg.qhub[i]->last=time(NULL);
				sprintf(str,"%sqnet.dab",sbbs->cfg.ctrl_dir);
				if((file=sbbs->nopen(str,O_WRONLY))==-1) {
					sbbs->errormsg(WHERE,ERR_OPEN,str,O_WRONLY);
					break; 
				}
				lseek(file,sizeof(time_t)*i,SEEK_SET);
				write(file,&sbbs->cfg.qhub[i]->last,sizeof(time_t));
				close(file);

				if(sbbs->cfg.qhub[i]->call[0]) {
					sbbs->cfg.node_num=sbbs->cfg.qhub[i]->node;
					if(sbbs->cfg.node_num<1) 
						sbbs->cfg.node_num=1;
					strcpy(sbbs->cfg.node_dir, sbbs->cfg.node_path[sbbs->cfg.node_num-1]);
					eprintf("QWK Network call-out: %s",sbbs->cfg.qhub[i]->id); 
					sbbs->online=ON_LOCAL;
					sbbs->external(
						 sbbs->cmdstr(sbbs->cfg.qhub[i]->call,nulstr,nulstr,NULL)
						,EX_OFFLINE|EX_SH);	/* sh for Unix perl scripts */
				}
			} 
		}

		/* PostLink Networking Call-out Events */
		for(i=0;i<sbbs->cfg.total_phubs;i++) {
			if(sbbs->cfg.phub[i]->node<first_node 
				|| sbbs->cfg.phub[i]->node>last_node)
				continue;
			/* PostLink call out based on time */
			if(localtime_r(&sbbs->cfg.phub[i]->last,&tm)==NULL)
				memset(&tm,0,sizeof(tm));
			if(sbbs->cfg.phub[i]->last==-1
				|| (((sbbs->cfg.phub[i]->freq								/* or frequency */
					&& (now-sbbs->cfg.phub[i]->last)/60>sbbs->cfg.phub[i]->freq)
				|| (sbbs->cfg.phub[i]->time
					&& (now_tm.tm_hour*60)+now_tm.tm_min>=sbbs->cfg.phub[i]->time
				&& (now_tm.tm_mday!=tm.tm_mday || now_tm.tm_mon!=tm.tm_mon)))
				&& sbbs->cfg.phub[i]->days&(1<<now_tm.tm_wday))) {

				sbbs->cfg.phub[i]->last=time(NULL);
				sprintf(str,"%spnet.dab",sbbs->cfg.ctrl_dir);
				if((file=sbbs->nopen(str,O_WRONLY))==-1) {
					sbbs->errormsg(WHERE,ERR_OPEN,str,O_WRONLY);
					break; 
				}
				lseek(file,sizeof(time_t)*i,SEEK_SET);
				write(file,&sbbs->cfg.phub[i]->last,sizeof(time_t));
				close(file);

				if(sbbs->cfg.phub[i]->call[0]) {
					sbbs->cfg.node_num=sbbs->cfg.phub[i]->node;
					if(sbbs->cfg.node_num<1) 
						sbbs->cfg.node_num=1;
					strcpy(sbbs->cfg.node_dir, sbbs->cfg.node_path[sbbs->cfg.node_num-1]);
					eprintf("PostLink Network call-out: %s",sbbs->cfg.phub[i]->name); 
					sbbs->online=ON_LOCAL;
					sbbs->external(
						 sbbs->cmdstr(sbbs->cfg.phub[i]->call,nulstr,nulstr,NULL)
						,EX_OFFLINE|EX_SH);	/* sh for Unix perl scripts */
				} 
			}
		}

		/* Timed Events */
		for(i=0;i<sbbs->cfg.total_events;i++) {
			if(!sbbs->cfg.event[i]->node 
				|| sbbs->cfg.event[i]->node>sbbs->cfg.sys_nodes)
				continue;	// ignore events for invalid nodes

			if((sbbs->cfg.event[i]->node<first_node
				|| sbbs->cfg.event[i]->node>last_node)
				&& !(sbbs->cfg.event[i]->misc&EVENT_EXCL))
				continue;	// ignore non-exclusive events for other instances

			if(localtime_r(&sbbs->cfg.event[i]->last,&tm)==NULL)
				memset(&tm,0,sizeof(tm));
			if(sbbs->cfg.event[i]->last==-1 ||
				(((sbbs->cfg.event[i]->freq 
					&& (now-sbbs->cfg.event[i]->last)/60>sbbs->cfg.event[i]->freq)
				|| 	(!sbbs->cfg.event[i]->freq 
					&& (now_tm.tm_hour*60)+now_tm.tm_min>=sbbs->cfg.event[i]->time
				&& (now_tm.tm_mday!=tm.tm_mday || now_tm.tm_mon!=tm.tm_mon)))
				&& sbbs->cfg.event[i]->days&(1<<now_tm.tm_wday)
				&& (sbbs->cfg.event[i]->mdays==0 
					|| sbbs->cfg.event[i]->mdays&(1<<now_tm.tm_mday)))) 
			{
				if(sbbs->cfg.event[i]->misc&EVENT_EXCL) { /* exclusive event */

					if(sbbs->cfg.event[i]->node<first_node
						|| sbbs->cfg.event[i]->node>last_node) {
						eprintf("Waiting for node %d to run timed event: %s"
							,sbbs->cfg.event[i]->node,sbbs->cfg.event[i]->code);
						lastnodechk=0;	 /* really last event time check */
						start=time(NULL);
						while(!sbbs->terminated) {
							mswait(1000);
							now=time(NULL);
							if(now-lastnodechk<10)
								continue;
							for(j=first_node;j<=last_node;j++) {
								if(sbbs->getnodedat(j,&node,1)!=0)
									continue;
								if(node.status==NODE_WFC)
									node.status=NODE_EVENT_LIMBO;
								node.aux=sbbs->cfg.event[i]->node;
								sbbs->putnodedat(j,&node);
							}

							lastnodechk=now;
							sprintf(str,"%stime.dab",sbbs->cfg.ctrl_dir);
							if((file=sbbs->nopen(str,O_RDONLY))==-1) {
								sbbs->errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
								sbbs->cfg.event[i]->last=now;
								continue; 
							}
							lseek(file,(long)i*4L,SEEK_SET);
							read(file,&sbbs->cfg.event[i]->last,sizeof(time_t));
							close(file);
							if(now-sbbs->cfg.event[i]->last<(60*60))	/* event is done */
								break; 
							if(now-start>(60*60)) {
								eprintf("!TIMEOUT waiting for event to complete");
								break;
							}
						}
						sprintf(str,"%s%s.now",sbbs->cfg.data_dir,sbbs->cfg.event[i]->code);
						if(fexistcase(str))
							remove(str);
						sbbs->cfg.event[i]->last=now;
					} else {	// Exclusive event to run on a node under our control
						eprintf("Waiting for all nodes to become inactive before "
							"running timed event: %s",sbbs->cfg.event[i]->code);
						lastnodechk=0;
						start=time(NULL);
						while(!sbbs->terminated) {
							mswait(1000);
							now=time(NULL);
							if(now-lastnodechk<10)
								continue;
							lastnodechk=now;
							// Check/change the status of the nodes that we're in control of
							for(j=first_node;j<=last_node;j++) {
								if(sbbs->getnodedat(j,&node,1)!=0)
									continue;
								if(node.status==NODE_WFC) {
									if(j==sbbs->cfg.event[i]->node)
										node.status=NODE_EVENT_WAITING;
									else
										node.status=NODE_EVENT_LIMBO;
									node.aux=sbbs->cfg.event[i]->node;
								}
								sbbs->putnodedat(j,&node);
							}

							for(j=1;j<=sbbs->cfg.sys_nodes;j++) {
								if(sbbs->getnodedat(j,&node,0)!=0)
									continue;
								if(j==sbbs->cfg.event[i]->node) {
									if(node.status!=NODE_EVENT_WAITING)
										break;
								} else {
									if(node.status!=NODE_OFFLINE
										&& node.status!=NODE_EVENT_LIMBO)
										break; 
								}
							}
							if(j>sbbs->cfg.sys_nodes) /* all nodes either offline or in limbo */
								break;
							eprintf("Waiting for node %d (status=%d)",j,node.status);
							if(now-start>(60*60)) {
								eprintf("!TIMEOUT waiting for node %d to become inactive",j);
								break;
							}
						} 
					} 
				}
#if 0 // removed Jun-23-2002
				else {	/* non-exclusive */
					sbbs->getnodedat(sbbs->cfg.event[i]->node,&node,0);
					if(node.status!=NODE_WFC)
						continue;
				}
#endif
				if(sbbs->cfg.event[i]->node<first_node 
					|| sbbs->cfg.event[i]->node>last_node) {
					eprintf("Changing node status for nodes %d through %d to WFC"
						,first_node,last_node);
					sbbs->cfg.event[i]->last=now;
					for(j=first_node;j<=last_node;j++) {
						node.status=NODE_INVALID_STATUS;
						if(sbbs->getnodedat(j,&node,1)!=0)
							continue;
						node.status=NODE_WFC;
						sbbs->putnodedat(j,&node);
					}
				}
				else {
					sbbs->cfg.node_num=sbbs->cfg.event[i]->node;
					if(sbbs->cfg.node_num<1) 
						sbbs->cfg.node_num=1;
					strcpy(sbbs->cfg.node_dir, sbbs->cfg.node_path[sbbs->cfg.node_num-1]);
				
					sprintf(str,"%s%s.now",sbbs->cfg.data_dir,sbbs->cfg.event[i]->code);
					if(fexistcase(str))
						remove(str);
					if(sbbs->cfg.event[i]->misc&EVENT_EXCL) {
						sbbs->getnodedat(sbbs->cfg.event[i]->node,&node,1);
						node.status=NODE_EVENT_RUNNING;
						sbbs->putnodedat(sbbs->cfg.event[i]->node,&node);
					}
					strcpy(str,sbbs->cfg.event[i]->code);
					eprintf("Running timed event: %s",strupr(str));
					int ex_mode = EX_OFFLINE;
					if(!(sbbs->cfg.event[i]->misc&EVENT_EXCL)
						&& sbbs->cfg.event[i]->misc&EX_BG)
						ex_mode |= EX_BG;
					if(sbbs->cfg.event[i]->misc&XTRN_SH)
						ex_mode |= EX_SH;
					ex_mode|=(sbbs->cfg.event[i]->misc&EX_NATIVE);
					sbbs->online=ON_LOCAL;
					sbbs->external(
						 sbbs->cmdstr(sbbs->cfg.event[i]->cmd,nulstr,nulstr,NULL)
						,ex_mode
						,sbbs->cfg.event[i]->dir);
					sbbs->cfg.event[i]->last=time(NULL);
					sprintf(str,"%stime.dab",sbbs->cfg.ctrl_dir);
					if((file=sbbs->nopen(str,O_WRONLY))==-1) {
						sbbs->errormsg(WHERE,ERR_OPEN,str,O_WRONLY);
						break; 
					}
					lseek(file,(long)i*4L,SEEK_SET);
					write(file,&sbbs->cfg.event[i]->last,sizeof(time_t));
					close(file);

					if(sbbs->cfg.event[i]->misc&EVENT_EXCL) { /* exclusive event */
						// Check/change the status of the nodes that we're in control of
						for(j=first_node;j<=last_node;j++) {
							node.status=NODE_INVALID_STATUS;
							if(sbbs->getnodedat(j,&node,1)!=0)
								continue;
							node.status=NODE_WFC;
							sbbs->putnodedat(j,&node);
						}
					}
				} 
			} 
		}
		pthread_mutex_unlock(&event_mutex);

		mswait(1000);
	}
	sbbs->cfg.node_num=0;
    sbbs->event_thread_running = false;

	thread_down();
	eprintf("BBS Event thread terminated (%u threads remain)", thread_count);
}


//****************************************************************************
sbbs_t::sbbs_t(ushort node_num, DWORD addr, char* name, SOCKET sd,
			   scfg_t* global_cfg, char* global_text[], client_t* client_info)
{
	char	nodestr[32];
	uint	i;

    if(node_num)
    	sprintf(nodestr,"Node %d",node_num);
    else
    	strcpy(nodestr,name);

	lprintf("%s constructor using socket %d (settings=%lx)"
		, nodestr, sd, global_cfg->node_misc);

	startup = ::startup;	// Convert from global to class member

	memcpy(&cfg, global_cfg, sizeof(cfg));

	cfg.node_num=node_num;
	if(node_num>0) {
		strcpy(cfg.node_dir, cfg.node_path[node_num-1]);
		prep_dir(cfg.node_dir, cfg.temp_dir);
	} else {
//		sprintf(cfg.temp_dir, "temp/%08lX", sbbs_random(INT_MAX));
    	prep_dir(cfg.data_dir, cfg.temp_dir);
	}

	terminated = false;
	event_thread_running = false;
    input_thread_running = false;
    output_thread_running = false;
	input_thread_mutex_locked = false;

	if(client_info==NULL)
		memset(&client,0,sizeof(client));
	else
		memcpy(&client,client_info,sizeof(client));
	client_addr = addr;
	client_socket = sd;
	SAFECOPY(client_name, name);
	client_socket_dup=INVALID_SOCKET;
	client_ident[0]=0;

	/* Init some important variables */

	rio_abortable=false;

	console = 0;
	online = 0;
	outchar_esc = 0;
	nodemsg_inside = 0;	/* allows single nest */
	hotkey_inside = 0;	/* allows single nest */
	nodesync_inside = false;
	errorlog_inside = false;
	errormsg_inside = false;
	gettimeleft_inside = false;
	uselect_total = 0;
	lbuflen = 0;
	connection="Telnet";
    telnet_cmdlen=0;
	telnet_mode=0;
	telnet_last_rxch=0;
	sys_status=lncntr=tos=criterrs=keybufbot=keybuftop=lbuflen=slcnt=0L;
	curatr=LIGHTGRAY;
	errorlevel=0;
	logcol=1;
	logfile_fp=NULL;
	nodefile=-1;
	node_ext=-1;
	nodefile_fp=NULL;
	node_ext_fp=NULL;
	current_msg=NULL;

#ifdef JAVASCRIPT
	js_runtime=NULL;	/* runtime */
	js_cx=NULL;			/* context */
#endif

	for(i=0;i<TOTAL_TEXT;i++)
		text[i]=text_sav[i]=global_text[i];

	memset(&main_csi,0,sizeof(main_csi));
	memset(&thisnode,0,sizeof(thisnode));
	memset(&useron,0,sizeof(useron));
	memset(&inbuf,0,sizeof(inbuf));
	memset(&outbuf,0,sizeof(outbuf));
	memset(&smb,0,sizeof(smb));

	global_str_vars=0;
	global_str_var=NULL;
	global_str_var_name=NULL;
	global_int_vars=0;
	global_int_var=NULL;
	global_int_var_name=NULL;
	sysvar_li=0;
	sysvar_pi=0;

	cursub=NULL;
	usrgrp=NULL;
	usrsubs=NULL;
	usrsub=NULL;
	usrgrp_total=0;

	subscan=NULL;

	curdir=NULL;
	usrlib=NULL;
	usrdirs=NULL;
	usrdir=NULL;
	usrlib_total=0;

	batup_desc=NULL;
	batup_name=NULL;
	batup_misc=NULL;
	batup_dir=NULL;
	batup_alt=NULL;

	batdn_name=NULL;
	batdn_dir=NULL;
	batdn_offset=NULL;
	batdn_size=NULL;
	batdn_alt=NULL;
	batdn_cdt=NULL;

	spymsg("Connected");
}

//****************************************************************************
bool sbbs_t::init()
{
	char		str[MAX_PATH+1];
	char		tmp[128];
	int			result;
	uint		i,j,k,l;
	node_t		node;
	socklen_t	addr_len;
	SOCKADDR_IN	addr;

	if(cfg.node_num>0) {
		RingBufInit(&inbuf, IO_THREAD_BUF_SIZE);
		node_inbuf[cfg.node_num-1]=&inbuf;
	}

    RingBufInit(&outbuf, IO_THREAD_BUF_SIZE);

	if(cfg.node_num && client_socket!=INVALID_SOCKET) {

#ifdef _WIN32
		if(!DuplicateHandle(GetCurrentProcess(),
			(HANDLE)client_socket,
			GetCurrentProcess(),
			(HANDLE*)&client_socket_dup,
			0,
			TRUE, // Inheritable
			DUPLICATE_SAME_ACCESS)) {
			errormsg(WHERE,ERR_CREATE,"duplicate socket handle",client_socket);
			return(false);
		}
#endif

		addr_len=sizeof(addr);
		if((result=getsockname(client_socket, (struct sockaddr *)&addr,&addr_len))!=0) {
			lprintf("Node %d !ERROR %d (%d) getting address/port"
				,cfg.node_num, result, ERROR_VALUE);
			return(false);
		} 
		lprintf("Node %d attached to local interface %s port %d"
			,cfg.node_num, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

		local_addr=addr.sin_addr.s_addr;
	}

	comspec=getenv(
#ifdef __unix__
		"SHELL"
#else
		"COMSPEC"
#endif
		);
	if(comspec==NULL) {
		errormsg(WHERE, ERR_CHK, "shell/comspec", 0);
		return(false);
	}

#ifdef _WIN32
	output_sem=CreateEvent(
					 NULL	// pointer to security attributes
					,false	// flag for manual-reset event
					,false	// flag for initial state
					,NULL	// pointer to event-object name
					);
	if(output_sem==NULL) {
		errormsg(WHERE, ERR_CREATE, "output_sem", 0);
		return(false);
	}
#endif
	sem_init(&output_sem,0,0);

	strcpy(str,cfg.temp_dir);
	if(strcmp(str+1,":\\") && strcmp(str+1,":/"))     /* not root directory */
	    str[strlen(str)-1]=0;   /* chop off '\' */
	md(str);

	/* Shared NODE files */
	sprintf(str,"%s%s",cfg.ctrl_dir,"node.dab");
	if((nodefile=nopen(str,O_DENYNONE|O_RDWR|O_CREAT))==-1) {
		errormsg(WHERE, ERR_OPEN, str, cfg.node_num);
		return(false); 
	}
	memset(&node,0,sizeof(node_t));  /* write NULL to node struct */
	node.status=NODE_OFFLINE;
	while(filelength(nodefile)<(long)(cfg.sys_nodes*sizeof(node_t))) {
		lseek(nodefile,0L,SEEK_END);
		if(write(nodefile,&node,sizeof(node_t))!=sizeof(node_t)) {
			errormsg(WHERE,ERR_WRITE,str,sizeof(node_t));
			break; 
		}
	}
	for(i=0; cfg.node_num>0 && i<LOOP_NODEDAB; i++) {
		if(lock(nodefile,(cfg.node_num-1)*sizeof(node_t),sizeof(node_t))==0) {
			unlock(nodefile,(cfg.node_num-1)*sizeof(node_t),sizeof(node_t));
			break;
		}
		mswait(100);
	}
	if(cfg.node_misc&NM_CLOSENODEDAB) {
		close(nodefile);
		nodefile=-1;
	}

	if(i>=LOOP_NODEDAB) {
		errormsg(WHERE, ERR_LOCK, str, cfg.node_num);
		return(false); 
	}

	if(cfg.node_num) {
		sprintf(str,"%snode.log",cfg.node_dir);
		if((logfile_fp=fopen(str,"a+b"))==NULL) {
			errormsg(WHERE, ERR_OPEN, str, 0);
			lprintf("Perhaps this node is already running");
			return(false); 
		}

		if(filelength(fileno(logfile_fp))) {
			log(crlf);
			now=time(NULL);
			struct tm tm;
			localtime_r(&now,&tm);
			sprintf(str,"%s  %s %s %02d %u  "
				"End of preexisting log entry (possible crash)"
				,hhmmtostr(&cfg,&tm,tmp)
				,wday[tm.tm_wday]
				,mon[tm.tm_mon],tm.tm_mday,tm.tm_year+1900);
			logline("L!",str);
			log(crlf);
			catsyslog(1); 
		}

		getnodedat(cfg.node_num,&thisnode,1);
		/* thisnode.status=0; */
		thisnode.action=0;
		thisnode.useron=0;
		thisnode.aux=0;
		thisnode.misc&=(NODE_EVENT|NODE_LOCK|NODE_RRUN);
		criterrs=thisnode.errors;
		putnodedat(cfg.node_num,&thisnode);
	}

/** Put in if(cfg.node_num) ? (not needed for server and event threads) */
	backout();

	/* Reset COMMAND SHELL */

	main_csi.str=(char *)MALLOC(1024);
	if(main_csi.str==NULL) {
		errormsg(WHERE,ERR_ALLOC,"main_csi.str",1024);
		return(false); 
	}
	memset(main_csi.str,0,1024);
/***/

	if(cfg.total_grps) {

		usrgrp_total = cfg.total_grps;

		if((cursub=(uint *)MALLOC(sizeof(uint)*usrgrp_total))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "cursub", sizeof(uint)*usrgrp_total);
			return(false);
		}

		if((usrgrp=(uint *)MALLOC(sizeof(uint)*usrgrp_total))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "usrgrp", sizeof(uint)*usrgrp_total);
			return(false);
		}

		if((usrsubs=(uint *)MALLOC(sizeof(uint)*usrgrp_total))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "usrsubs", sizeof(uint)*usrgrp_total);
			return(false);
		}

		if((usrsub=(uint **)calloc(usrgrp_total,sizeof(uint *)))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "usrsub", sizeof(uint)*usrgrp_total);
			return(false);
		}
 
		if((subscan=(subscan_t *)MALLOC(sizeof(subscan_t)*cfg.total_subs))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "subscan", sizeof(subscan_t)*cfg.total_subs);
			return(false);
		}
	}

	for(i=l=0;i<(uint)cfg.total_grps;i++) {
		for(j=k=0;j<cfg.total_subs;j++)
			if(cfg.sub[j]->grp==i)
				k++;	/* k = number of subs per grp[i] */
		if(k>l) l=k;  	/* l = the largest number of subs per grp */
	}
	if(l)
		for(i=0;i<cfg.total_grps;i++)
			if((usrsub[i]=(uint *)MALLOC(sizeof(uint)*l))==NULL) {
				errormsg(WHERE, ERR_ALLOC, "usrsub[x]", sizeof(uint)*l);
				return(false);
			}

	if(cfg.total_libs) {

		usrlib_total = cfg.total_libs;

		if((curdir=(uint *)MALLOC(sizeof(uint)*usrlib_total))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "curdir", sizeof(uint)*usrlib_total);
			return(false);
		}

		if((usrlib=(uint *)MALLOC(sizeof(uint)*usrlib_total))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "usrlib", sizeof(uint)*usrlib_total);
			return(false);
		}

		if((usrdirs=(uint *)MALLOC(sizeof(uint)*usrlib_total))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "usrdirs", sizeof(uint)*usrlib_total);
			return(false);
		}

		if((usrdir=(uint **)calloc(usrlib_total,sizeof(uint *)))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "usrdir", sizeof(uint)*usrlib_total);
			return(false);
		}
	}

	for(i=l=0;i<cfg.total_libs;i++) {
		for(j=k=0;j<cfg.total_dirs;j++)
			if(cfg.dir[j]->lib==i)
				k++;
		if(k>l) l=k; 	/* l = largest number of dirs in a lib */
	}
	if(l) {
		l++;	/* for temp dir */
		for(i=0;i<cfg.total_libs;i++)
			if((usrdir[i]=(uint *)MALLOC(sizeof(uint)*l))==NULL) {
				errormsg(WHERE, ERR_ALLOC, "usrdir[x]", sizeof(uint)*l);
				return(false);
			}
	}
 
	if(cfg.max_batup) {

		if((batup_desc=(char **)MALLOC(sizeof(char *)*cfg.max_batup))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "batup_desc", sizeof(char *)*cfg.max_batup);
			return(false);
		}
		if((batup_name=(char **)MALLOC(sizeof(char *)*cfg.max_batup))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "batup_name", sizeof(char *)*cfg.max_batup);
			return(false);
		}
		if((batup_misc=(long *)MALLOC(sizeof(long)*cfg.max_batup))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "batup_misc", sizeof(char *)*cfg.max_batup);
			return(false);
		}
		if((batup_dir=(uint *)MALLOC(sizeof(uint)*cfg.max_batup))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "batup_dir", sizeof(char *)*cfg.max_batup);
			return(false);
		}
		if((batup_alt=(ushort *)MALLOC(sizeof(ushort)*cfg.max_batup))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "batup_alt", sizeof(char *)*cfg.max_batup);
			return(false);
		}
		for(i=0;i<cfg.max_batup;i++) {
			if((batup_desc[i]=(char *)MALLOC(59))==NULL) {
				errormsg(WHERE, ERR_ALLOC, "batup_desc[x]", 59);
				return(false);
			}
			if((batup_name[i]=(char *)MALLOC(13))==NULL) {
				errormsg(WHERE, ERR_ALLOC, "batup_name[x]", 13);
				return(false);
			} 
		} 
	}

	if(cfg.max_batdn) {

		if((batdn_name=(char **)MALLOC(sizeof(char *)*cfg.max_batdn))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "batdn_name", sizeof(char *)*cfg.max_batdn);
			return(false);
		}
		if((batdn_dir=(uint *)MALLOC(sizeof(uint)*cfg.max_batdn))==NULL)  {
			errormsg(WHERE, ERR_ALLOC, "batdn_dir", sizeof(uint)*cfg.max_batdn);
			return(false);
		}
		if((batdn_offset=(long *)MALLOC(sizeof(long)*cfg.max_batdn))==NULL)  {
			errormsg(WHERE, ERR_ALLOC, "batdn_offset", sizeof(long)*cfg.max_batdn);
			return(false);
		}
		if((batdn_size=(ulong *)MALLOC(sizeof(ulong)*cfg.max_batdn))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "batdn_size", sizeof(ulong)*cfg.max_batdn);
			return(false);
		}
		if((batdn_cdt=(ulong *)MALLOC(sizeof(ulong)*cfg.max_batdn))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "batdn_cdt", sizeof(long)*cfg.max_batdn);
			return(false);
		}
		if((batdn_alt=(ushort *)MALLOC(sizeof(ushort)*cfg.max_batdn))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "batdn_alt", sizeof(ushort)*cfg.max_batdn);
			return(false);
		}
		for(i=0;i<cfg.max_batdn;i++)
			if((batdn_name[i]=(char *)MALLOC(13))==NULL) {
				errormsg(WHERE, ERR_ALLOC, "batdn_name[x]", 13);
				return(false);
			} 
	}

	reset_logon_vars();

	online=ON_REMOTE;

	return(true);
}

//****************************************************************************
sbbs_t::~sbbs_t()
{
	uint i;
	char node[32];

    if(cfg.node_num)
    	sprintf(node,"Node %d", cfg.node_num);
    else
    	strcpy(node,client_name);
#ifdef _DEBUG
	lprintf("%s destructor begin", node);
#endif

//	if(!cfg.node_num)
//		rmdir(cfg.temp_dir);

	if(client_socket_dup!=INVALID_SOCKET)
		closesocket(client_socket_dup);	/* close duplicate handle */

	sem_post(&output_sem);		/* just incase someone's waiting */
	sem_destroy(&output_sem);

	if(cfg.node_num>0)
		node_inbuf[cfg.node_num-1]=NULL;
	if(!input_thread_running)
		RingBufDispose(&inbuf);
	if(!output_thread_running)
		RingBufDispose(&outbuf);

	/* Close all open files */
	if(nodefile!=-1) {
		close(nodefile);
		nodefile=-1;
	}
	if(node_ext!=-1) {
		close(node_ext);
		node_ext=-1;
	}
	if(logfile_fp!=NULL) {
		fclose(logfile_fp);
		logfile_fp=NULL;
	}

	/********************************/
	/* Free allocated class members */
	/********************************/

#ifdef JAVASCRIPT
	/* Free Context */
	if(js_cx!=NULL) {	
		lprintf("%s JavaScript: Destroying context",node);
		JS_DestroyContext(js_cx);
		js_cx=NULL;
	}

	if(js_runtime!=NULL) {
		lprintf("%s JavaScript: Destroying runtime",node);
		JS_DestroyRuntime(js_runtime);
		js_runtime=NULL;
	}
#endif

	/* Reset text.dat */

	for(i=0;i<TOTAL_TEXT && text!=NULL;i++)
		if(text[i]!=text_sav[i]) {
			if(text[i]!=nulstr)
				FREE(text[i]); 
		}

	/* Global command shell vars */

	freevars(&main_csi);
	clearvars(&main_csi);
	FREE_AND_NULL(main_csi.str);	/* crash */
	FREE_AND_NULL(main_csi.cs);

	for(i=0;i<global_str_vars && global_str_var!=NULL;i++)
		FREE_AND_NULL(global_str_var[i]);

	FREE_AND_NULL(global_str_var);
	FREE_AND_NULL(global_str_var_name);
	global_str_vars=0;

	FREE_AND_NULL(global_int_var);
	FREE_AND_NULL(global_int_var_name);
	global_int_vars=0;

	/* Sub-board variables */
	for(i=0;i<usrgrp_total && usrsub!=NULL;i++)
		FREE_AND_NULL(usrsub[i]);	/* exception here (ptr=0xfdfdfdfd) on exit July-10-2002 */

	FREE_AND_NULL(cursub);
	FREE_AND_NULL(usrgrp);
	FREE_AND_NULL(usrsubs);
	FREE_AND_NULL(usrsub);
	FREE_AND_NULL(subscan);

	/* File Directory variables */
	for(i=0;i<usrlib_total && usrdir!=NULL;i++)
		FREE_AND_NULL(usrdir[i]);

	FREE_AND_NULL(curdir);
	FREE_AND_NULL(usrlib);
	FREE_AND_NULL(usrdirs);
	FREE_AND_NULL(usrdir);

	/* Batch upload vars */
	for(i=0;i<cfg.max_batup && batup_desc!=NULL && batup_name!=NULL;i++) {
		FREE_AND_NULL(batup_desc[i]);
		FREE_AND_NULL(batup_name[i]);
	}

	FREE_AND_NULL(batup_desc);
	FREE_AND_NULL(batup_name);
	FREE_AND_NULL(batup_misc);
	FREE_AND_NULL(batup_dir);
	FREE_AND_NULL(batup_alt);

	/* Batch download vars */
	for(i=0;i<cfg.max_batdn && batdn_name!=NULL;i++)
		FREE_AND_NULL(batdn_name[i]); 

	FREE_AND_NULL(batdn_name);
	FREE_AND_NULL(batdn_dir);
	FREE_AND_NULL(batdn_offset);
	FREE_AND_NULL(batdn_size);
	FREE_AND_NULL(batdn_cdt);
	FREE_AND_NULL(batdn_alt);

#if 0 && defined(_WIN32) && defined(_DEBUG) && defined(_MSC_VER)
	if(!_CrtCheckMemory())
		lprintf("!MEMORY ERRORS REPORTED IN DATA/DEBUG.LOG!");
#endif

#ifdef _DEBUG
	lprintf("%s destructor end", node);
#endif
}

/****************************************************************************/
/* Network open function. Opens all files DENYALL and retries LOOP_NOPEN    */
/* number of times if the attempted file is already open or denying access  */
/* for some other reason.													*/
/* All files are opened in BINARY mode, unless O_TEXT access bit is set.	*/
/****************************************************************************/
int sbbs_t::nopen(char *str, int access)
{
	char logstr[256];
	int file,share,count=0;

    if(access&O_DENYNONE) {
        share=SH_DENYNO;
        access&=~O_DENYNONE; 
	}
    else if(access==O_RDONLY) share=SH_DENYWR;
    else share=SH_DENYRW;
	if(!(access&O_TEXT))
		access|=O_BINARY;
    while(((file=sopen(str,access,share))==-1)
        && (errno==EACCES || errno==EAGAIN) && count++<LOOP_NOPEN)
	    mswait(100);
    if(count>(LOOP_NOPEN/2) && count<=LOOP_NOPEN) {
        sprintf(logstr,"NOPEN COLLISION - File: \"%s\" Count: %d"
            ,str,count);
        logline("!!",logstr); 
	}
    if(file==-1 && (errno==EACCES || errno==EAGAIN)) {
        sprintf(logstr,"NOPEN ACCESS DENIED - File: \"%s\" errno: %d"
			,str,errno);
		logline("!!",logstr);
		bputs("\7\r\nNOPEN: ACCESS DENIED\r\n\7");
	}
    return(file);
}

void sbbs_t::spymsg(char* msg)
{
	char str[512];
	struct in_addr addr;

	if(cfg.node_num<1)
		return;

	addr.s_addr=client_addr;
	sprintf(str,"\r\n\r\n*** Spy Message ***\r\nNode %d: %s [%s]\r\n*** %s ***\r\n\r\n"
		,cfg.node_num,client_name,inet_ntoa(addr),msg);
	if(startup->node_spybuf!=NULL 
		&& startup->node_spybuf[cfg.node_num-1]!=NULL) {
		RingBufWrite(startup->node_spybuf[cfg.node_num-1],(uchar*)str,strlen(str));
		/* Signal spy output semaphore? */
		if(startup->node_spysem!=NULL 
			&& startup->node_spysem[sbbs->cfg.node_num-1]!=NULL)
			sem_post(startup->node_spysem[sbbs->cfg.node_num-1]);
	}

	if(cfg.node_num && spy_socket[cfg.node_num-1]!=INVALID_SOCKET) 
		sendsocket(spy_socket[cfg.node_num-1],str,strlen(str));
}

#define MV_BUFLEN	4096

/****************************************************************************/
/* Moves or copies a file from one dir to another                           */
/* both 'src' and 'dest' must contain full path and filename                */
/* returns 0 if successful, -1 if error                                     */
/****************************************************************************/
int sbbs_t::mv(char *src, char *dest, char copy)
{
	char	str[MAX_PATH+1],*buf,atr=curatr;
	int		ind,outd;
	uint	chunk=MV_BUFLEN;
	ulong	length,l;
	/* struct ftime ftime; */
	FILE *inp,*outp;

    if(!stricmp(src,dest))	 /* source and destination are the same! */
        return(0);
    if(!fexist(src)) {
        bprintf("\r\n\7MV ERROR: Source doesn't exist\r\n'%s'\r\n"
            ,src);
        return(-1); 
	}
    if(!copy && fexist(dest)) {
        bprintf("\r\n\7MV ERROR: Destination already exists\r\n'%s'\r\n"
            ,dest);
        return(-1); 
	}
#ifndef __unix__	/* need to determine if on same mount device */
    if(!copy && ((src[1]!=':' && dest[1]!=':')
        || (src[1]==':' && dest[1]==':' && toupper(src[0])==toupper(dest[0])))) {
        if(rename(src,dest)) {						/* same drive, so move */
            bprintf("\r\nMV ERROR: Error renaming '%s'"
                    "\r\n                      to '%s'\r\n\7",src,dest);
            return(-1); 
		}
        return(0); 
	}
#endif
    attr(WHITE);
    if((ind=nopen(src,O_RDONLY))==-1) {
        errormsg(WHERE,ERR_OPEN,src,O_RDONLY);
        return(-1); 
	}
    if((inp=fdopen(ind,"rb"))==NULL) {
        close(ind);
        errormsg(WHERE,ERR_FDOPEN,str,O_RDONLY);
        return(-1); 
	}
    setvbuf(inp,NULL,_IOFBF,32*1024);
    if((outd=nopen(dest,O_WRONLY|O_CREAT|O_TRUNC))==-1) {
        fclose(inp);
        errormsg(WHERE,ERR_OPEN,dest,O_WRONLY|O_CREAT|O_TRUNC);
        return(-1); 
	}
    if((outp=fdopen(outd,"wb"))==NULL) {
        close(outd);
        fclose(inp);
        errormsg(WHERE,ERR_FDOPEN,dest,O_WRONLY|O_CREAT|O_TRUNC);
        return(-1); 
	}
    setvbuf(outp,NULL,_IOFBF,8*1024);
    length=filelength(ind);
    if(!length) {
        fclose(inp);
        fclose(outp);
        errormsg(WHERE,ERR_LEN,src,0);
        return(-1); 
	}
    if((buf=(char *)MALLOC(MV_BUFLEN))==NULL) {
        fclose(inp);
        fclose(outp);
        errormsg(WHERE,ERR_ALLOC,nulstr,MV_BUFLEN);
        return(-1); 
	}
    l=0L;
    while(l<length) {
        bprintf("%2lu%%",l ? (long)(100.0/((float)length/l)) : 0L);
        if(l+chunk>length)
            chunk=length-l;
        if(fread(buf,1,chunk,inp)!=chunk) {
            FREE(buf);
            fclose(inp);
            fclose(outp);
            errormsg(WHERE,ERR_READ,src,chunk);
            return(-1); 
		}
        if(fwrite(buf,1,chunk,outp)!=chunk) {
            FREE(buf);
            fclose(inp);
            fclose(outp);
            errormsg(WHERE,ERR_WRITE,dest,chunk);
            return(-1); 
		}
        l+=chunk;
        bputs("\b\b\b"); 
	}
    bputs("   \b\b\b");  /* erase it */
    attr(atr);
    /* getftime(ind,&ftime);
    setftime(outd,&ftime); */
    FREE(buf);
    fclose(inp);
    fclose(outp);
    if(!copy && remove(src)) {
        errormsg(WHERE,ERR_REMOVE,src,0);
        return(-1); 
	}
    return(0);
}

/****************************************************************************/
/* Reads data from dsts.dab into stats structure                            */
/* If node is zero, reads from ctrl\dsts.dab, otherwise from each node		*/
/****************************************************************************/
BOOL DLLCALL getstats(scfg_t* cfg, char node, stats_t* stats)
{
    char str[MAX_PATH+1];
    int file;

    sprintf(str,"%sdsts.dab",node ? cfg->node_path[node-1] : cfg->ctrl_dir);
    if((file=nopen(str,O_RDONLY))==-1) {
//        errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
        return(FALSE); 
	}
    lseek(file,4L,SEEK_SET);    /* Skip update time/date */
    read(file,stats,sizeof(stats_t));
    close(file);
	return(TRUE);
}


void sbbs_t::hangup(void)
{
	if(client_socket!=INVALID_SOCKET) {
		mswait(1000);	/* Give socket output buffer time to flush */
		riosync(0);
		client_off(client_socket);
		close_socket(client_socket);
		client_socket=INVALID_SOCKET;
	}
	if(client_socket_dup!=INVALID_SOCKET) {
		closesocket(client_socket_dup);
		client_socket_dup=INVALID_SOCKET;
	}
	sem_post(&output_sem);
	online=0;
}

int sbbs_t::incom(void)
{
	uchar	ch;

	if(!RingBufRead(&inbuf, &ch, 1))
		return(NOINP);
#if 0 // removed Jan-2003
	if(!(cfg.ctrlkey_passthru&(1<<CTRL_C))	
		&& rio_abortable && ch==CTRL_C)
		return(NOINP); /* this should never happen since input_thread eats Ctrl-C chars */
#endif
	return(ch);
}

int sbbs_t::outcom(uchar ch)
{
	if(!RingBufFree(&outbuf))
		return(TXBOF);
    if(!RingBufWrite(&outbuf, &ch, 1))
		return(TXBOF);
	sem_post(&output_sem);
	return(0);
}

void sbbs_t::putcom(char *str, int len)
{
	int i;

    if(!len)
    	len=strlen(str);
    for(i=0;i<len && online; i++)
        outcom(str[i]);
}

void sbbs_t::riosync(char abortable)
{
	if(useron.misc&(RIP|WIP|HTML))	/* don't allow abort with RIP or WIP */
		abortable=0;			/* mainly because of ANSI cursor position response */
	if(sys_status&SS_ABORT)		/* no need to sync if already aborting */
		return;
	time_t start=time(NULL);
	while(online && rioctl(TXBC)) {				/* wait up to three minutes for tx buf empty */
		if(abortable && rioctl(RXBC)) { 		/* incoming characer */
			rioctl(IOFO);						/* flush output */
			sys_status|=SS_ABORT;				/* set abort flag so no pause */
			break;								/* abort sync */
		}
		if(time(NULL)-start>180) {				/* timeout */
			logline("!!","riosync timeout"); 
			break;
		}
		mswait(100);
	}
}

/* Legacy Remote I/O Control Interface */
int sbbs_t::rioctl(ushort action)
{
	int		mode;
	int		state;

	switch(action) {
		case GVERS: 	/* Get version */
			return(0x200);
		case GUART: 	/* Get UART I/O address, not available */
			return(0xffff);
		case GIRQN: 	/* Get IRQ number, not available */
			return((int)client_socket);
		case GBAUD: 	/* Get current bit rate */
			return(0xffff);
		case RXBC:		/* Get receive buffer count */
			// ulong	cnt;
			// ioctlsocket (client_socket,FIONREAD,&cnt);
 			return(/* cnt+ */RingBufFull(&inbuf));
		case RXBS:		/* Get receive buffer size */
			return(inbuf.size);
		case TXBC:		/* Get transmit buffer count */
			return(RingBufFull(&outbuf));
		case TXBS:		/* Get transmit buffer size */
			return(outbuf.size);
		case TXBF:		/* Get transmit buffer free space */
 			return(RingBufFree(&outbuf));
		case IOMODE:
			mode=0;
			if(rio_abortable)
				mode|=ABORT;
			return(mode);
		case IOSTATE:
			state=0;
			if(sys_status&SS_ABORT)
				state|=ABORT;
			return(state);
		case IOFI:		/* Flush input buffer */
			RingBufReInit(&inbuf);
			break;
		case IOFO:		/* Flush output buffer */
    		RingBufReInit(&outbuf);
			break;
		case IOFB:		/* Flush both buffers */
			RingBufReInit(&inbuf);
			RingBufReInit(&outbuf);
			break;
		case LFN81:
		case LFE71:
		case FIFOCTL:
			return(0);
		}

	if((action&0xff)==IOSM) {	/* Get/Set/Clear mode */
		if(action&ABORT)
			rio_abortable=true;
		return(0); 
	}

	if((action&0xff)==IOCM) {	/* Get/Set/Clear mode */
		if(action&ABORT)
			rio_abortable=false;
		return(0); 
	}

	if((action&0xff)==IOSS) {	/* Set state */
		if(action&ABORT)
			sys_status|=SS_ABORT;
		return(0); 
	}

	if((action&0xff)==IOCS) {	/* Clear state */
		if(action&ABORT)
			sys_status&=~SS_ABORT;
		return(0); 
	}

	return(0);
}

void sbbs_t::reset_logon_vars(void)
{
	int i;

    /* bools */
    qwklogon=false;

    sys_status&=~(SS_USERON|SS_TMPSYSOP|SS_LCHAT|SS_ABORT
        |SS_PAUSEON|SS_PAUSEOFF|SS_EVENT|SS_NEWUSER|SS_NEWDAY);
    cid[0]=0;
    wordwrap[0]=0;
    question[0]=0;
    menu_dir[0]=0;
    menu_file[0]=0;
    rows=0;
    lncntr=0;
    autoterm=0;
    keybufbot=keybuftop=lbuflen=0;
    slcnt=0;
    altul=0;
    timeleft_warn=0;
    logon_uls=logon_ulb=logon_dls=logon_dlb=0;
    logon_posts=logon_emails=logon_fbacks=0;
    batdn_total=batup_total=0;
    usrgrps=usrlibs=0;
    curgrp=curlib=0;
    for(i=0;i<cfg.total_libs;i++)
        curdir[i]=0;
    for(i=0;i<cfg.total_grps;i++)
        cursub[i]=0;
	cur_cps=3000;
    cur_rate=30000;
    dte_rate=38400;
	main_cmds=xfer_cmds=posts_read=0;
	lastnodemsg=0;
	lastnodemsguser[0]=0;
}

/****************************************************************************/
/* Writes NODE.LOG at end of SYSTEM.LOG										*/
/****************************************************************************/
void sbbs_t::catsyslog(int crash)
{
	char str[MAX_PATH+1];
	char HUGE16 *buf;
	int  i,file;
	long length;
	struct tm tm;

	if(logfile_fp==NULL) {
		sprintf(str,"%snode.log",cfg.node_dir);
		if((logfile_fp=fopen(str,"rb"))==NULL) {
			errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
			return; 
		}
	}
	length=ftell(logfile_fp);
	if(length) {
		if((buf=(char HUGE16 *)LMALLOC(length))==NULL) {
			errormsg(WHERE,ERR_ALLOC,str,length);
			return; 
		}
		rewind(logfile_fp);
		if(fread(buf,1,length,logfile_fp)!=(size_t)length) {
			errormsg(WHERE,ERR_READ,"log file",length);
			FREE((char *)buf);
			return; 
		}
		now=time(NULL);
		localtime_r(&now,&tm);
		sprintf(str,"%slogs/%2.2d%2.2d%2.2d.log",cfg.data_dir,tm.tm_mon+1,tm.tm_mday
			,TM_YEAR(tm.tm_year));
		if((file=nopen(str,O_WRONLY|O_APPEND|O_CREAT))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_APPEND|O_CREAT);
			FREE((char *)buf);
			return; 
		}
		if(lwrite(file,buf,length)!=length) {
			close(file);
			errormsg(WHERE,ERR_WRITE,str,length);
			FREE((char *)buf);
			return; 
		}
		close(file);
		if(crash) {
			for(i=0;i<2;i++) {
				sprintf(str,"%scrash.log",i ? cfg.data_dir : cfg.node_dir);
				if((file=nopen(str,O_WRONLY|O_APPEND|O_CREAT))==-1) {
					errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_APPEND|O_CREAT);
					FREE((char *)buf);
					return; 
				}
				if(lwrite(file,buf,length)!=length) {
					close(file);
					errormsg(WHERE,ERR_WRITE,str,length);
					FREE((char *)buf);
					return; 
				}
				close(file); 
			} 
		}
		FREE((char *)buf); 
	}

	fclose(logfile_fp);

	sprintf(str,"%snode.log",cfg.node_dir);
	if((logfile_fp=fopen(str,"w+b"))==NULL) /* Truncate NODE.LOG */
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_TRUNC);
}


void sbbs_t::logoffstats()
{
    char str[MAX_PATH+1];
    int i,file;
    stats_t stats;

	if(REALSYSOP && !(cfg.sys_misc&SM_SYSSTAT))
		return;
	
	for(i=0;i<2;i++) {
		sprintf(str,"%sdsts.dab",i ? cfg.ctrl_dir : cfg.node_dir);
		if((file=nopen(str,O_RDWR))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_RDWR);
			return; 
		}
		lseek(file,12L,SEEK_SET);   /* Skip timestamp, logons and logons today */
		read(file,&stats.timeon,4);   /* Total time on system  */
		read(file,&stats.ttoday,4); /* Time today on system  */
		read(file,&stats.uls,4);        /* Uploads today         */
		read(file,&stats.ulb,4);        /* Upload bytes today    */
		read(file,&stats.dls,4);        /* Downloads today       */
		read(file,&stats.dlb,4);        /* Download bytes today  */
		read(file,&stats.ptoday,4); 	/* Posts today			 */
		read(file,&stats.etoday,4); /* Emails today          */
		read(file,&stats.ftoday,4); /* Feedback sent today  */
		read(file,&stats.nusers,2); /* New users today		*/

		if(!(useron.rest&FLAG('Q'))) {	/* Don't count QWKnet nodes */
			stats.timeon+=(now-logontime)/60;
			stats.ttoday+=(now-logontime)/60;
			stats.ptoday+=logon_posts;
		}
		stats.uls+=logon_uls;
		stats.ulb+=logon_ulb;
		stats.dls+=logon_dls;
		stats.dlb+=logon_dlb;
		stats.etoday+=logon_emails;
		stats.ftoday+=logon_fbacks;
		if(sys_status&SS_NEWUSER)
			stats.nusers++;

		lseek(file,12L,SEEK_SET);
		write(file,&stats.timeon,4);	/* Total time on system  */
		write(file,&stats.ttoday,4);    /* Time today on system  */
		write(file,&stats.uls,4);       /* Uploads today         */
		write(file,&stats.ulb,4);       /* Upload bytes today    */
		write(file,&stats.dls,4);       /* Downloads today       */
		write(file,&stats.dlb,4);       /* Download bytes today  */
		write(file,&stats.ptoday,4);    /* Posts today           */
		write(file,&stats.etoday,4);    /* Emails today          */
		write(file,&stats.ftoday,4);	/* Feedback sent today	 */
		write(file,&stats.nusers,2);	/* New users today		 */
		close(file); 
	}
}

void node_thread(void* arg)
{
	char			str[128];
	char			uname[LEN_ALIAS+1];
	int				file;
	uint			i,j;
	uint			curshell=0;
	time_t			now;
	node_t			node;
	user_t			user;
	sbbs_t*			sbbs = (sbbs_t*) arg;

	update_clients();
	thread_up(TRUE /* setuid */);

#ifdef _DEBUG
	lprintf("Node %d thread started",sbbs->cfg.node_num);
#endif

	srand(time(NULL));	/* Seed random number generator */
	sbbs_random(10);	/* Throw away first number */

#ifdef JAVASCRIPT
	if(!(startup->options&BBS_OPT_NO_JAVASCRIPT)) {
		if(!sbbs->js_init())	/* This must be done in the context of the node thread */
			lprintf("Node %d !JavaScript Initialization FAILURE",sbbs->cfg.node_num);
	}
#endif

	if(sbbs->answer()) {

		if(sbbs->qwklogon) {
			sbbs->getsmsg(sbbs->useron.number);
			sbbs->qwk_sec();
		} else while(sbbs->useron.number 
			&& (sbbs->main_csi.misc&CS_OFFLINE_EXEC || sbbs->online)) {

			if(!sbbs->main_csi.cs || curshell!=sbbs->useron.shell) {
				if(sbbs->useron.shell>=sbbs->cfg.total_shells)
					sbbs->useron.shell=0;
				sprintf(str,"%s%s.bin",sbbs->cfg.exec_dir
					,sbbs->cfg.shell[sbbs->useron.shell]->code);
				if((file=sbbs->nopen(str,O_RDONLY))==-1) {
					sbbs->errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
					sbbs->hangup();
					break; 
				}
				FREE_AND_NULL(sbbs->main_csi.cs);
				sbbs->freevars(&sbbs->main_csi);
				sbbs->clearvars(&sbbs->main_csi);

				sbbs->main_csi.length=filelength(file);
				if((sbbs->main_csi.cs=(uchar *)MALLOC(sbbs->main_csi.length))==NULL) {
					close(file);
					sbbs->errormsg(WHERE,ERR_ALLOC,str,sbbs->main_csi.length);
					sbbs->hangup();
					break; 
				}

				if(lread(file,sbbs->main_csi.cs,sbbs->main_csi.length)
					!=(int)sbbs->main_csi.length) {
					sbbs->errormsg(WHERE,ERR_READ,str,sbbs->main_csi.length);
					close(file);
					FREE(sbbs->main_csi.cs);
					sbbs->main_csi.cs=NULL;
					sbbs->hangup();
					break; 
				}
				close(file);

				curshell=sbbs->useron.shell;
				sbbs->main_csi.ip=sbbs->main_csi.cs;
				sbbs->menu_dir[0]=0;
				sbbs->menu_file[0]=0;
				}
			if(sbbs->exec(&sbbs->main_csi))
				break;

		}
	}

#ifdef _WIN32
	if(startup->hangup_sound[0] && !(startup->options&BBS_OPT_MUTE)) 
		PlaySound(startup->hangup_sound, NULL, SND_ASYNC|SND_FILENAME);
#endif

	sbbs->hangup();	/* closes sockets, calls client_off, and shuts down the output_thread */
    node_socket[sbbs->cfg.node_num-1]=INVALID_SOCKET;

	sbbs->logout();
	sbbs->logoffstats();	/* Updates both system and node dsts.dab files */

	if(sbbs->sys_status&SS_DAILY) {	// New day, run daily events/maintenance

		now=time(NULL);

		sbbs->getnodedat(sbbs->cfg.node_num,&node,1);
		node.status=NODE_EVENT_RUNNING;
		sbbs->putnodedat(sbbs->cfg.node_num,&node);

		sbbs->logentry("!:","Ran system daily maintenance");

		if(sbbs->cfg.user_backup_level) {
			lprintf("Node %d Backing-up user data..."
				,sbbs->cfg.node_num);
			sprintf(str,"%suser/user.dat",sbbs->cfg.data_dir);
			backup(str,sbbs->cfg.user_backup_level,FALSE);
			sprintf(str,"%suser/name.dat",sbbs->cfg.data_dir);
			backup(str,sbbs->cfg.user_backup_level,FALSE);
		}

		if(sbbs->cfg.mail_backup_level) {
			lprintf("Node %d Backing-up mail data..."
				,sbbs->cfg.node_num);
			sprintf(str,"%smail.shd",sbbs->cfg.data_dir);
			backup(str,sbbs->cfg.mail_backup_level,FALSE);
			sprintf(str,"%smail.sha",sbbs->cfg.data_dir);
			backup(str,sbbs->cfg.mail_backup_level,FALSE);
			sprintf(str,"%smail.sdt",sbbs->cfg.data_dir);
			backup(str,sbbs->cfg.mail_backup_level,FALSE);
			sprintf(str,"%smail.sda",sbbs->cfg.data_dir);
			backup(str,sbbs->cfg.mail_backup_level,FALSE);
			sprintf(str,"%smail.sid",sbbs->cfg.data_dir);
			backup(str,sbbs->cfg.mail_backup_level,FALSE);
			sprintf(str,"%smail.sch",sbbs->cfg.data_dir);
			backup(str,sbbs->cfg.mail_backup_level,FALSE);
		}

		lprintf("Node %d Checking for inactive/expired user records..."
			,sbbs->cfg.node_num);
		j=lastuser(&sbbs->cfg);
		for(i=1;i<=j;i++) {

			sprintf(str,"%5u of %-5u",i,j);
			status(str);
			user.number=i;
			getuserdat(&sbbs->cfg,&user);

			/***********************************************/
			/* Fix name (name.dat and user.dat) mismatches */
			/***********************************************/
			username(&sbbs->cfg,i,uname);
			if(user.misc&DELETED) {
				if(strcmp(uname,"DELETED USER"))
					putusername(&sbbs->cfg,i,nulstr);
				continue; 
			}

			if(strcmp(user.alias,uname))
				putusername(&sbbs->cfg,i,user.alias);

			if(!(user.misc&(DELETED|INACTIVE))
				&& user.expire && (ulong)user.expire<=(ulong)now) {
				putsmsg(&sbbs->cfg,i,sbbs->text[AccountHasExpired]);
				sprintf(str,"%s #%u Expired",user.alias,user.number);
				sbbs->logentry("!%",str);
				if(sbbs->cfg.level_misc[user.level]&LEVEL_EXPTOVAL
					&& sbbs->cfg.level_expireto[user.level]<10) {
					user.flags1=sbbs->cfg.val_flags1[sbbs->cfg.level_expireto[user.level]];
					user.flags2=sbbs->cfg.val_flags2[sbbs->cfg.level_expireto[user.level]];
					user.flags3=sbbs->cfg.val_flags3[sbbs->cfg.level_expireto[user.level]];
					user.flags4=sbbs->cfg.val_flags4[sbbs->cfg.level_expireto[user.level]];
					user.exempt=sbbs->cfg.val_exempt[sbbs->cfg.level_expireto[user.level]];
					user.rest=sbbs->cfg.val_rest[sbbs->cfg.level_expireto[user.level]];
					if(sbbs->cfg.val_expire[sbbs->cfg.level_expireto[user.level]])
						user.expire=now
							+(sbbs->cfg.val_expire[sbbs->cfg.level_expireto[user.level]]*24*60*60);
					else
						user.expire=0;
					user.level=sbbs->cfg.val_level[sbbs->cfg.level_expireto[user.level]]; 
				}
				else {
					if(sbbs->cfg.level_misc[user.level]&LEVEL_EXPTOLVL)
						user.level=sbbs->cfg.level_expireto[user.level];
					else
						user.level=sbbs->cfg.expired_level;
					user.flags1&=~sbbs->cfg.expired_flags1; /* expired status */
					user.flags2&=~sbbs->cfg.expired_flags2; /* expired status */
					user.flags3&=~sbbs->cfg.expired_flags3; /* expired status */
					user.flags4&=~sbbs->cfg.expired_flags4; /* expired status */
					user.exempt&=~sbbs->cfg.expired_exempt;
					user.rest|=sbbs->cfg.expired_rest;
					user.expire=0; 
				}
				putuserrec(&sbbs->cfg,i,U_LEVEL,2,ultoa(user.level,str,10));
				putuserrec(&sbbs->cfg,i,U_FLAGS1,8,ultoa(user.flags1,str,16));
				putuserrec(&sbbs->cfg,i,U_FLAGS2,8,ultoa(user.flags2,str,16));
				putuserrec(&sbbs->cfg,i,U_FLAGS3,8,ultoa(user.flags3,str,16));
				putuserrec(&sbbs->cfg,i,U_FLAGS4,8,ultoa(user.flags4,str,16));
				putuserrec(&sbbs->cfg,i,U_EXPIRE,8,ultoa(user.expire,str,16));
				putuserrec(&sbbs->cfg,i,U_EXEMPT,8,ultoa(user.exempt,str,16));
				putuserrec(&sbbs->cfg,i,U_REST,8,ultoa(user.rest,str,16));
				if(sbbs->cfg.expire_mod[0]) {
					sbbs->useron=user;
					sbbs->online=ON_LOCAL;
					sbbs->exec_bin(sbbs->cfg.expire_mod,&sbbs->main_csi);
					sbbs->online=0; 
				}
			}

			/***********************************************************/
			/* Auto deletion based on expiration date or days inactive */
			/***********************************************************/
			if(!(user.exempt&FLAG('P'))     /* Not a permanent account */
				&& !(user.misc&(DELETED|INACTIVE))	 /* alive */
				&& (sbbs->cfg.sys_autodel && (now-user.laston)/(long)(24L*60L*60L)
				> sbbs->cfg.sys_autodel)) {			/* Inactive too long */
				sprintf(str,"Auto-Deleted %s #%u",user.alias,user.number);
				sbbs->logentry("!*",str);
				sbbs->delallmail(i);
				putusername(&sbbs->cfg,i,nulstr);
				putuserrec(&sbbs->cfg,i,U_MISC,8,ultoa(user.misc|DELETED,str,16)); 
			}
		}

		lprintf("Node %d Purging deleted/expired e-mail",sbbs->cfg.node_num);
		sprintf(sbbs->smb.file,"%smail",sbbs->cfg.data_dir);
		sbbs->smb.retry_time=sbbs->cfg.smb_retry_time;
		sbbs->smb.subnum=INVALID_SUB;
		if((i=smb_open(&sbbs->smb))!=0)
			sbbs->errormsg(WHERE,ERR_OPEN,sbbs->smb.file,i,sbbs->smb.last_error);
		else {
			if(filelength(fileno(sbbs->smb.shd_fp))>0) {
				if((i=smb_locksmbhdr(&sbbs->smb))!=0)
					sbbs->errormsg(WHERE,ERR_LOCK,sbbs->smb.file,i,sbbs->smb.last_error);
				else
					sbbs->delmail(0,MAIL_ALL);
			}
			smb_close(&sbbs->smb); 
		}

		sbbs->sys_status&=~SS_DAILY;
		if(sbbs->cfg.sys_daily[0]) {
//			status("Running system daily event");
			sbbs->logentry("!:","Ran system daily event");
			sbbs->external(sbbs->cmdstr(sbbs->cfg.sys_daily,nulstr,nulstr,NULL)
				,EX_OFFLINE); 
		}
	}

#if 0	/* this is handled in the event_thread now */
	// Node Daily Event
	sbbs->getnodedat(sbbs->cfg.node_num,&node,0);
	if(node.misc&NODE_EVENT) {
		sbbs->getnodedat(sbbs->cfg.node_num,&node,1);
		node.status=NODE_EVENT_RUNNING;
		sbbs->putnodedat(sbbs->cfg.node_num,&node);
		if(sbbs->cfg.node_daily[0]) {
			sbbs->logentry("!:","Run node daily event");
			sbbs->external(
				 sbbs->cmdstr(sbbs->cfg.node_daily,nulstr,nulstr,NULL)
				,EX_OFFLINE);
		}
		sbbs->getnodedat(sbbs->cfg.node_num,&node,1);
		node.misc&=~NODE_EVENT;
		sbbs->putnodedat(sbbs->cfg.node_num,&node); 
	}
#endif

    // Wait for all node threads to terminate
	if(sbbs->input_thread_running || sbbs->output_thread_running) {
		lprintf("Node %d Waiting for %s to terminate..."
			,sbbs->cfg.node_num
			,(sbbs->input_thread_running && sbbs->output_thread_running) ?
               	"I/O threads" : sbbs->input_thread_running
				? "input thread" : "output thread");
		time_t start=time(NULL);
		while(sbbs->input_thread_running
    		|| sbbs->output_thread_running) {
			if(time(NULL)-start>TIMEOUT_THREAD_WAIT) {
				lprintf("Node %d !TIMEOUT waiting for %s to terminate"
					, sbbs->cfg.node_num
					,(sbbs->input_thread_running && sbbs->output_thread_running) ?
                  		"I/O threads"
					: sbbs->input_thread_running
                		? "input thread" : "output thread");
				break;
			}
			mswait(100);
		}
	}

	sbbs->catsyslog(0);

	status(STATUS_WFC);

	sbbs->getnodedat(sbbs->cfg.node_num,&node,1);
	if(node.misc&NODE_DOWN)
		node.status=NODE_OFFLINE;
	else
		node.status=NODE_WFC;
	node.misc&=~(NODE_DOWN|NODE_INTR|NODE_MSGW|NODE_NMSG
				|NODE_UDAT|NODE_POFF|NODE_AOFF|NODE_EXT);
/*	node.useron=0; needed for hang-ups while in multinode chat */
	sbbs->putnodedat(sbbs->cfg.node_num,&node);

	if(node_threads_running>0)
		node_threads_running--;
	lprintf("Node %d thread terminated (%u node threads remain, %lu clients served)"
		,sbbs->cfg.node_num, node_threads_running, served);
    if(!sbbs->input_thread_running && !sbbs->output_thread_running)
		delete sbbs;
    else
		lprintf("Node %d !ORPHANED I/O THREAD(s)",sbbs->cfg.node_num);

	update_clients();
	thread_down();
}

time_t checktime(void)
{
	struct tm tm;

    memset(&tm,0,sizeof(tm));
    tm.tm_year=94;
    tm.tm_mday=1;
    return(mktime(&tm)-0x2D24BD00L);
}

const char* DLLCALL js_ver(void)
{
#ifdef JAVASCRIPT
	return(JS_GetImplementationVersion());
#else
	return("");
#endif
}

/* Returns char string of version and revision */
const char* DLLCALL bbs_ver(void)
{
	static char ver[256];
	char compiler[32];

	DESCRIBE_COMPILER(compiler);

	sprintf(ver,"%s %s%c%s  SMBLIB %s  Compiled %s %s with %s"
		,TELNET_SERVER
		,VERSION, REVISION
#ifdef _DEBUG
		," Debug"
#else
		,""
#endif
		,smb_lib_ver()
		,__DATE__, __TIME__, compiler
		);

	return(ver);
}

/* Returns binary-coded version and revision (e.g. 0x31000 == 3.10a) */
long DLLCALL bbs_ver_num(void)
{
	char*	minor;

	if((minor=strchr(VERSION,'.'))==NULL)
		return(0);
	minor++;

	return((strtoul(VERSION,NULL,16)<<16)|(strtoul(minor,NULL,16)<<8)|(REVISION-'A'));
}

void DLLCALL bbs_terminate(void)
{
	recycle_server=false;
	if(telnet_socket!=INVALID_SOCKET) {
    	lprintf("BBS Terminate: closing telnet socket %d",telnet_socket);
		close_socket(telnet_socket);
		telnet_socket=INVALID_SOCKET;
    }
}

static void cleanup(int code)
{
    lputs("BBS System thread terminating");

	if(telnet_socket!=INVALID_SOCKET) {
		close_socket(telnet_socket);
		telnet_socket=INVALID_SOCKET;
	}
	if(rlogin_socket!=INVALID_SOCKET) {
		close_socket(rlogin_socket);
		rlogin_socket=INVALID_SOCKET;
	}


#ifdef _WINSOCKAPI_
	if(WSAInitialized && WSACleanup()!=0) 
		lprintf("!WSACleanup ERROR %d",ERROR_VALUE);
#endif

	free_cfg(&scfg);
	free_text(text);

#ifdef _WIN32
	if(exec_mutex!=NULL) {
		CloseHandle(exec_mutex);
		exec_mutex=NULL;
	}

	if(hK32!=NULL) {
		FreeLibrary(hK32);
		hK32=NULL;
	}

#if 0 && defined(_DEBUG) && defined(_MSC_VER)
	_CrtMemDumpAllObjectsSince(&mem_chkpoint);

	if(debug_log!=INVALID_HANDLE_VALUE) {
		CloseHandle(debug_log);
		debug_log=INVALID_HANDLE_VALUE;
	}
#endif // _DEBUG && _MSC_VER
#endif // _WIN32

	pthread_mutex_destroy(&event_mutex);

	status("Down");
	thread_down();
    lprintf("BBS System thread terminated (%u threads remain, %lu clients served)"
		,thread_count, served);
	if(startup->terminated!=NULL)
		startup->terminated(code);
}

void DLLCALL bbs_thread(void* arg)
{
	char *			host_name;
	char *			identity;
    char			str[MAX_PATH+1];
	char			logstr[256];
	SOCKADDR_IN		server_addr={0};
	SOCKADDR_IN		client_addr;
	socklen_t		client_addr_len;
	SOCKET			client_socket;
	fd_set			socket_set;
	SOCKET			high_socket_set;
	int				i;
    int				file;
	int				result;
	BOOL			option;
	time_t			t;
	time_t			start;
	time_t			initialized=0;
	node_t			node;
	sbbs_t*			events;
	client_t		client;
	startup=(bbs_startup_t*)arg;

    if(startup==NULL) {
    	sbbs_beep(100,500);
    	fprintf(stderr, "No startup structure passed!\n");
    	return;
    }

	if(startup->size!=sizeof(bbs_startup_t)) {	// verify size
		sbbs_beep(100,500);
		sbbs_beep(300,500);
		sbbs_beep(100,500);
		fprintf(stderr, "Invalid startup structure!\n");
		return;
	}

	/* Setup intelligent defaults */
	if(startup->telnet_port==0)				startup->telnet_port=IPPORT_TELNET;
	if(startup->rlogin_port==0)				startup->rlogin_port=513;
	if(startup->xtrn_polls_before_yield==0)	startup->xtrn_polls_before_yield=10;
#ifdef JAVASCRIPT
	if(startup->js_max_bytes==0)			startup->js_max_bytes=JAVASCRIPT_MAX_BYTES;
#endif

	uptime=0;
	served=0;
	startup->recycle_now=FALSE;
	recycle_server=true;
	do {

	thread_up(FALSE /* setuid */);

	status("Initializing");

	/* Defeat the lameo hex0rs - the name and copyright must remain intact */
	if(crc32(COPYRIGHT_NOTICE,0)!=COPYRIGHT_CRC 
		|| crc32(VERSION_NOTICE,10)!=SYNCHRONET_CRC) {
		lprintf("!CORRUPTED LIBRARY FILE");
		cleanup(1);
		return;
	}

	memset(text, 0, sizeof(text));
    memset(&scfg, 0, sizeof(scfg));

	node_threads_running=0;
	lastuseron[0]=0;

	char compiler[32];
	DESCRIBE_COMPILER(compiler);

	lprintf("%s Version %s Revision %c%s"
		,TELNET_SERVER
		,VERSION
		,toupper(REVISION)
#ifdef _DEBUG
		," Debug"
#else
		,""
#endif
		);
	lprintf("Compiled %s %s with %s", __DATE__, __TIME__, compiler);
	lprintf("SMBLIB %s (format %x.%02x)",smb_lib_ver(),smb_ver()>>8,smb_ver()&0xff);

    if(startup->first_node<1 || startup->first_node>startup->last_node) {
    	lprintf("!ILLEGAL node configuration (first: %d, last: %d)"
        	,startup->first_node, startup->last_node);
		cleanup(1);
        return;
    }

	if(sizeof(node_t)!=SIZEOF_NODE_T) {
		lprintf("!COMPILER ERROR: sizeof(node_t)=%d instead of %d"
			,sizeof(node_t),SIZEOF_NODE_T);
		cleanup(1);
		return;
	}

#ifdef _WIN32
    if((exec_mutex=CreateMutex(NULL,false,NULL))==NULL) {
    	lprintf("!ERROR %d creating exec_mutex", GetLastError());
		cleanup(1);
        return;
    }
	hK32 = LoadLibrary("KERNEL32");

	if(hK32!=NULL)
		Win98GetLongPathName
			= (GetLongPathName_t)GetProcAddress(hK32,"GetLongPathNameA");
#endif // _WIN32

	pthread_mutex_init(&event_mutex,NULL);

	if(!winsock_startup()) {
		cleanup(1);
		return;
	}

	t=time(NULL);
	lprintf("Initializing on %.24s with options: %lx"
		,CTIME_R(&t,str),startup->options);

	if(chdir(startup->ctrl_dir)!=0)
		lprintf("!ERROR %d changing directory to: %s", errno, startup->ctrl_dir);

	/* Initial configuration and load from CNF files */
    SAFECOPY(scfg.ctrl_dir,startup->ctrl_dir);
    lprintf("Loading configuration files from %s", scfg.ctrl_dir);
	scfg.size=sizeof(scfg);
	scfg.node_num=startup->first_node;
	SAFECOPY(logstr,UNKNOWN_LOAD_ERROR);
	if(!load_cfg(&scfg, text, TRUE, logstr)) {
		lprintf("!ERROR %s",logstr);
		lprintf("!FAILED to load configuration files");
		cleanup(1);
		return;
	}
	scfg_reloaded=true;

	if(startup->host_name[0]==0)
		SAFECOPY(startup->host_name,scfg.sys_inetaddr);

	if(!(scfg.sys_misc&SM_LOCAL_TZ) && !(startup->options&BBS_OPT_LOCAL_TIMEZONE)) {
		if(putenv("TZ=UTC0"))
			lprintf("!putenv() FAILED");
		tzset();

		if((t=checktime())!=0) {   /* Check binary time */
			lprintf("!TIME PROBLEM (%ld)",t);
			cleanup(1);
			return;
		}
	}

	if(uptime==0)
		uptime=time(NULL);	/* this must be done *after* setting the timezone */

    if(startup->last_node>scfg.sys_nodes) {
    	lprintf("Specified last_node (%d) > sys_nodes (%d), auto-corrected"
        	,startup->last_node, scfg.sys_nodes);
        startup->last_node=scfg.sys_nodes;
    }

	/* Create missing directories */
	lprintf("Verifying/creating data directories");
	make_data_dirs(&scfg);

	/* Create missing node directories and dsts.dab files */
	lprintf("Verifying/creating node directories");
	for(i=0;i<=scfg.sys_nodes;i++) {
		if(i)
			md(scfg.node_path[i-1]);
		sprintf(str,"%sdsts.dab",i ? scfg.node_path[i-1] : scfg.ctrl_dir);
		if(flength(str)<DSTSDABLEN) {
			if((file=sopen(str,O_WRONLY|O_CREAT|O_APPEND, SH_DENYNO))==-1) {
				lprintf("!ERROR %d creating %s",errno, str);
				cleanup(1);
				return; 
			}
			while(filelength(file)<DSTSDABLEN)
				if(write(file,"\0",1)!=1)
					break;				/* Create NULL system dsts.dab */
			close(file); 
		} 
	}

	/* Initial global node variables */
	for(i=0;i<MAX_NODES;i++) {
		node_inbuf[i]=NULL;
    	node_socket[i]=INVALID_SOCKET;
		spy_socket[i]=INVALID_SOCKET;
	}

	startup->node_inbuf=node_inbuf;

    /* open a socket and wait for a client */

    telnet_socket = open_socket(SOCK_STREAM);

	if(telnet_socket == INVALID_SOCKET) {
		lprintf("!ERROR %d creating Telnet socket", ERROR_VALUE);
		cleanup(1);
		return;
	}

    lprintf("Telnet socket %d opened",telnet_socket);

	if(startup->options&BBS_OPT_KEEP_ALIVE) {
		lprintf("Enabling WinSock Keep Alives");
		option = TRUE;

		result = setsockopt(telnet_socket, SOL_SOCKET, SO_KEEPALIVE
    		,(char *)&option, sizeof(option));

		if(result != 0) {
			lprintf("!ERROR %d (%d) setting Telnet socket option", result, ERROR_VALUE);
			cleanup(1);
			return;
		}

	}

	/*****************************/
	/* Listen for incoming calls */
	/*****************************/
    memset(&server_addr, 0, sizeof(server_addr));

	server_addr.sin_addr.s_addr = htonl(startup->telnet_interface);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(startup->telnet_port);

	if(startup->seteuid!=NULL)
		startup->seteuid(FALSE);
    result = bind(telnet_socket,(struct sockaddr *)&server_addr,sizeof(server_addr));
	if(startup->seteuid!=NULL)
		startup->seteuid(TRUE);
	if(result != 0) {
		lprintf("!ERROR %d (%d) binding Telnet socket to port %d"
			,result, ERROR_VALUE,startup->telnet_port);
		lprintf(BIND_FAILURE_HELP);
		cleanup(1);
		return;
	}

    result = listen(telnet_socket, 1);

	if(result != 0) {
		lprintf("!ERROR %d (%d) listening on Telnet socket", result, ERROR_VALUE);
		cleanup(1);
		return;
	}
	lprintf("Telnet server listening on port %d",startup->telnet_port);

	if(startup->options&BBS_OPT_ALLOW_RLOGIN) {

		/* open a socket and wait for a client */

		rlogin_socket = open_socket(SOCK_STREAM);

		if(rlogin_socket == INVALID_SOCKET) {
			lprintf("!ERROR %d creating RLogin socket", ERROR_VALUE);
			cleanup(1);
			return;
		}

		lprintf("RLogin socket %d opened",rlogin_socket);

		/*****************************/
		/* Listen for incoming calls */
		/*****************************/
		memset(&server_addr, 0, sizeof(server_addr));

		server_addr.sin_addr.s_addr = htonl(startup->rlogin_interface);
		server_addr.sin_family = AF_INET;
		server_addr.sin_port   = htons(startup->rlogin_port);

		if(startup->seteuid!=NULL)
			startup->seteuid(FALSE);
		result = bind(rlogin_socket,(struct sockaddr *)&server_addr,sizeof(server_addr));
		if(startup->seteuid!=NULL)
			startup->seteuid(TRUE);
		if(result != 0) {
			lprintf("!ERROR %d (%d) binding RLogin socket to port %d"
				,result, ERROR_VALUE,startup->rlogin_port);
			lprintf(BIND_FAILURE_HELP);
			cleanup(1);
			return;
		}

		result = listen(rlogin_socket, 1);

		if(result != 0) {
			lprintf("!ERROR %d (%d) listening on RLogin socket", result, ERROR_VALUE);
			cleanup(1);
			return;
		}
		lprintf("RLogin server listening on port %d",startup->rlogin_port);
	}

	/* signal caller that we've started up successfully */
    if(startup->started!=NULL)
    	startup->started();

	sbbs = new sbbs_t(0, server_addr.sin_addr.s_addr
		,"BBS System", telnet_socket, &scfg, text, NULL);
    sbbs->online = 0;
	if(sbbs->init()==false) {
		lputs("!BBS initialization failed");
		cleanup(1);
		return;
	}
	_beginthread(output_thread, 0, sbbs);

	events = new sbbs_t(0, server_addr.sin_addr.s_addr
		,"BBS Events", INVALID_SOCKET, &scfg, text, NULL);
    events->online = 0;
	if(events->init()==false) {
		lputs("!Events initialization failed");
		cleanup(1);
		return;
	}
	_beginthread(event_thread, 0, events);

	/* Save these values incase they're changed dynamically */
	first_node=startup->first_node;
	last_node=startup->last_node;

	for(i=first_node;i<=last_node;i++) {
		sbbs->getnodedat(i,&node,1);
		node.status=NODE_WFC;
		node.misc&=NODE_EVENT;
		node.action=0;
		sbbs->putnodedat(i,&node);
	}

	lprintf("BBS System thread started for nodes %d through %d", first_node, last_node);
	status(STATUS_WFC);

#if defined(_WIN32) && defined(_DEBUG) && defined(_MSC_VER)
	
	sprintf(str,"%sDEBUG.LOG",scfg.data_dir);
	if((debug_log=CreateFile(
		str,				// pointer to name of the file
		GENERIC_READ|GENERIC_WRITE,
		FILE_SHARE_READ|FILE_SHARE_WRITE,
		NULL,               // pointer to security attributes
		OPEN_ALWAYS,		// how to create
		FILE_ATTRIBUTE_NORMAL, // file attributes
		NULL				// handle to file with attributes to 
		))==INVALID_HANDLE_VALUE) {
		lprintf("!ERROR %ld creating %s",GetLastError(),str);
		cleanup(1);
		return;
	}

	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_WARN, debug_log);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE|_CRTDBG_MODE_WNDW);
	_CrtSetReportFile(_CRT_ERROR, debug_log);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE|_CRTDBG_MODE_WNDW);
	_CrtSetReportFile(_CRT_ASSERT, debug_log);

	/* Turns on memory leak checking during program termination */
//	_CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);

	/* Save this allocation point for comparison */
	_CrtMemCheckpoint(&mem_chkpoint);

#endif // _WIN32 && _DEBUG && _MSC_VER

	if(!initialized) {
		initialized=time(NULL);
		sprintf(str,"%stelnet.rec",scfg.ctrl_dir);
		t=fdate(str);
		if(t!=-1 && t>initialized)
			initialized=t;
	}

	while(telnet_socket!=INVALID_SOCKET) {

		if(node_threads_running==0) {	/* check for re-run flags */
			bool rerun=false;
			for(i=first_node;i<=last_node;i++) {
				if(sbbs->getnodedat(i,&node,0)!=0)
					continue;
				if(node.misc&NODE_RRUN) {
					sbbs->getnodedat(i,&node,1);
					if(!rerun)
						lprintf("Node %d flagged for re-run",i);
					rerun=true;
					node.misc&=~NODE_RRUN;
					sbbs->putnodedat(i,&node);
				}
			}
			if(rerun) {
				lprintf("Loading configuration files from %s", scfg.ctrl_dir);
				scfg.node_num=first_node;
				pthread_mutex_lock(&event_mutex);
				SAFECOPY(logstr,UNKNOWN_LOAD_ERROR);
				if(!load_cfg(&scfg, text, TRUE, logstr)) {
					lprintf("!ERROR %s",logstr);
					lprintf("!FAILED to load configuration files");
					break;
				}
				scfg_reloaded=true;
				pthread_mutex_unlock(&event_mutex);
			}
			if(!(startup->options&BBS_OPT_NO_RECYCLE)) {
				sprintf(str,"%stelnet.rec",scfg.ctrl_dir);
				t=fdate(str);
				if(t!=-1 && t>initialized) {
					lprintf("0000 Recycle semaphore file (%s) detected",str);
					initialized=t;
					break;
				}
				if(startup->recycle_now==TRUE) {
					lprintf("0000 Recycle semaphore signaled");
					startup->recycle_now=FALSE;
					break;
				}
			}
		}


    	sbbs->online=0;

		/* now wait for connection */

		FD_ZERO(&socket_set);
		FD_SET(telnet_socket,&socket_set);
		high_socket_set=telnet_socket+1;
		if(startup->options&BBS_OPT_ALLOW_RLOGIN 
			&& rlogin_socket!=INVALID_SOCKET) {
			FD_SET(rlogin_socket,&socket_set);
			if(rlogin_socket+1>high_socket_set)
				high_socket_set=rlogin_socket+1;
		}

		struct timeval tv;
		tv.tv_sec=2;
		tv.tv_usec=0;

		if((i=select(high_socket_set,&socket_set,NULL,NULL,&tv))<1) {
			if(i==0) {
				mswait(1);
				continue;
			}
			if(ERROR_VALUE==EINTR)
				lprintf("Telnet Server listening interrupted");
			else if(ERROR_VALUE == ENOTSOCK)
            	lprintf("Telnet Server sockets closed");
			else
				lprintf("!ERROR %d selecting sockets",ERROR_VALUE);
			break;
		}

		if(telnet_socket==INVALID_SOCKET)	/* terminated */
			break;

		client_addr_len = sizeof(client_addr);

		bool rlogin = false;

		if(telnet_socket!=INVALID_SOCKET 
			&& FD_ISSET(telnet_socket,&socket_set)) 
			client_socket = accept_socket(telnet_socket, (struct sockaddr *)&client_addr
	        	,&client_addr_len);
		else if(rlogin_socket!=INVALID_SOCKET 
			&& FD_ISSET(rlogin_socket,&socket_set)) {
			client_socket = accept_socket(rlogin_socket, (struct sockaddr *)&client_addr
	        	,&client_addr_len);
			rlogin = true;
		} else {
			lprintf("!NO SOCKETS set by select");
			continue;
		}

		if(client_socket == INVALID_SOCKET)	{
			if(ERROR_VALUE == ENOTSOCK || ERROR_VALUE == EINTR) {
            	lputs("BBS socket closed");
				break;
			}
			lprintf("!ERROR %d accepting connection", ERROR_VALUE);
			break;	// was continue, July-01-2002
		}
		char host_ip[32];

		strcpy(host_ip,inet_ntoa(client_addr.sin_addr));

		if(trashcan(&scfg,host_ip,"ip-silent")) {
			close_socket(client_socket);
			continue;
		}

		lprintf("%04d %s connection accepted from: %s port %u"
			,client_socket
			,rlogin ? "RLogin" : "Telnet", host_ip, ntohs(client_addr.sin_port));

#ifdef _WIN32
		if(startup->answer_sound[0] && !(startup->options&BBS_OPT_MUTE)) 
			PlaySound(startup->answer_sound, NULL, SND_ASYNC|SND_FILENAME);
#endif

   		sbbs->client_socket=client_socket;	// required for output to the user
        sbbs->online=ON_REMOTE;

		if(sbbs->trashcan(host_ip,"ip")) {
			close_socket(client_socket);
			lprintf("%04d !CLIENT BLOCKED in ip.can"
				,client_socket);
			sprintf(logstr, "Blocked IP: %s",host_ip);
			sbbs->syslog("@!",logstr);
			continue;
		}

		if(rlogin) {
			if(!trashcan(&scfg,host_ip,"rlogin")) {
				close_socket(client_socket);
				lprintf("%04d !CLIENT IP NOT LISTED in rlogin.can",client_socket);
				sprintf(logstr, "Invalid RLogin from: %s",host_ip);
				sbbs->syslog("@!",logstr);
				continue;
			}
			sbbs->outcom(0); /* acknowledge RLogin per RFC 1282 */
		}

		sbbs->putcom(crlf);
		sbbs->putcom(VERSION_NOTICE);
		sbbs->putcom(crlf);

		sbbs->bprintf("Connection from: %s\r\n", host_ip);

		struct hostent* h;
		if(startup->options&BBS_OPT_NO_HOST_LOOKUP)
			h=NULL;
		else {
			sbbs->bprintf("Resolving hostname...");
			h=gethostbyaddr((char *)&client_addr.sin_addr
				,sizeof(client_addr.sin_addr),AF_INET);
			sbbs->putcom(crlf);
		}
		if(h!=NULL && h->h_name!=NULL)
			host_name=h->h_name;
		else
			host_name="<no name>";

		if(!(startup->options&BBS_OPT_NO_HOST_LOOKUP))
			lprintf("%04d Hostname: %s", client_socket, host_name);

		if(sbbs->trashcan(host_name,"host")) {
			close_socket(client_socket);
			lprintf("%04d !CLIENT BLOCKED in host.can",client_socket);
			sprintf(logstr, "Blocked Hostname: %s",host_name);
			sbbs->syslog("@!",logstr);
			continue;
		}

		identity=NULL;
		if(startup->options&BBS_OPT_GET_IDENT) {
			sbbs->bprintf("Resolving identity...");
			identify(&client_addr, startup->telnet_port, str, sizeof(str)-1);
			identity=strrchr(str,':');
			if(identity!=NULL) {
				identity++;	/* skip colon */
				while(*identity && *identity<=SP) /* point to user name */
					identity++;
				lprintf("%04d Identity: %s",client_socket, identity);
			}
		}
		/* Initialize client display */
		client.size=sizeof(client);
		client.time=time(NULL);
		SAFECOPY(client.addr,host_ip);
		SAFECOPY(client.host,host_name);
		client.port=ntohs(client_addr.sin_port);
		client.protocol=rlogin ? "RLogin":"Telnet";
		client.user="<unknown>";
		client_on(client_socket,&client,FALSE /* update */);

		for(i=first_node;i<=last_node;i++) {
			/* paranoia: make sure node.status!=NODE_WFC by default */
			node.status=NODE_INVALID_STATUS;	
			if(sbbs->getnodedat(i,&node,1)!=0)
				continue;
			if(node.status==NODE_WFC) {
				node.status=NODE_LOGON;
				sbbs->putnodedat(i,&node);
				break;
			}
			sbbs->putnodedat(i,&node);
		}

		if(i>last_node) {
			lprintf("%04d !No nodes available for login.",client_socket);
			sprintf(str,"%snonodes.txt",scfg.text_dir);
			if(fexist(str))
				sbbs->printfile(str,P_NOABORT);
			else {
				sbbs->putcom("\r\nSorry, all telnet nodes are in use or otherwise unavailable.\r\n");
				sbbs->putcom("Please try again later.\r\n");
			}
			mswait(3000);
			client_off(client_socket);
			close_socket(client_socket);
			continue;
		}

        node_socket[i-1]=client_socket;

		sbbs_t* new_node = new sbbs_t(i, client_addr.sin_addr.s_addr, host_name
        	,client_socket, &scfg, text, &client);

		new_node->client=client;

		/* copy the IDENT response, if any */
		if(identity!=NULL)
			SAFECOPY(new_node->client_ident,identity);

		if(new_node->init()==false) {
			lprintf("%04d !Node %d Initialization failure"
				,client_socket,new_node->cfg.node_num);
			sprintf(str,"%snonodes.txt",scfg.text_dir);
			if(fexist(str))
				sbbs->printfile(str,P_NOABORT);
			else 
				sbbs->putcom("\r\nSorry, initialization failed. Try again later.\r\n");
			mswait(3000);
			sbbs->getnodedat(new_node->cfg.node_num,&node,1);
			node.status=NODE_WFC;
			sbbs->putnodedat(new_node->cfg.node_num,&node);
			delete new_node;
			node_socket[i-1]=INVALID_SOCKET;
			client_off(client_socket);
			close_socket(client_socket);
			continue;
		}

		if(rlogin==true) {
			new_node->connection="RLogin";
			new_node->sys_status|=SS_RLOGIN;
		}

	    node_threads_running++;
		new_node->input_thread=(HANDLE)_beginthread(input_thread,0, new_node);
		_beginthread(output_thread, 0, new_node);
		_beginthread(node_thread, 0, new_node);
		served++;
	}

    // Close all open sockets
    for(i=0;i<MAX_NODES;i++)
    	if(node_socket[i]!=INVALID_SOCKET) {
        	lprintf("Closing node %d socket %d", i+1, node_socket[i]);
        	close_socket(node_socket[i]);
			node_socket[i]=INVALID_SOCKET;
        }

	sbbs->client_socket=INVALID_SOCKET;
	events->terminated=true;
    // Wake-up BBS output thread so it can terminate
    sem_post(&sbbs->output_sem);

    // Wait for all node threads to terminate
	if(node_threads_running) {
		lprintf("Waiting for %d node threads to terminate...", node_threads_running);
		start=time(NULL);
		while(node_threads_running) {
			if(time(NULL)-start>TIMEOUT_THREAD_WAIT) {
				lprintf("!TIMEOUT waiting for %d node thread(s) to "
            		"terminate", node_threads_running);
				break;
			}
			mswait(100);
		}
	}

	// Wait for Events thread to terminate
	if(events->event_thread_running) {
		pthread_mutex_unlock(&event_mutex);
		lprintf("Waiting for event thread to terminate...");
		start=time(NULL);
		while(events->event_thread_running) {
			if(time(NULL)-start>TIMEOUT_THREAD_WAIT) {
				lprintf("!TIMEOUT waiting for BBS event thread to "
            		"terminate");
				break;
			}
			mswait(100);
		}
	}

    // Wait for BBS output thread to terminate
	if(sbbs->output_thread_running) {
		lprintf("Waiting for system output thread to terminate...");
		start=time(NULL);
		while(sbbs->output_thread_running) {
			if(time(NULL)-start>TIMEOUT_THREAD_WAIT) {
				lprintf("!TIMEOUT waiting for BBS output thread to "
            		"terminate");
				break;
			}
			mswait(100);
		}
	}

    // Set all nodes' status to OFFLINE
    for(i=first_node;i<=last_node;i++) {
        sbbs->getnodedat(i,&node,1);
        node.status=NODE_OFFLINE;
        sbbs->putnodedat(i,&node);
    }

	if(!events->event_thread_running)
		delete events;

    if(!sbbs->output_thread_running)
	    delete sbbs;

	cleanup(0);

	if(recycle_server) {
		lprintf("Recycling server...");
		mswait(2000);
	}

	} while(recycle_server);

}



