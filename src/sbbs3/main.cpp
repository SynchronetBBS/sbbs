/* main.cpp */

/* Synchronet main/telnet server thread and related functions */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2008 Rob Swindell - http://www.synchro.net/copyright.html		*
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
#include "netwrap.h"

#ifdef __unix__
	#include <sys/un.h>
	#ifndef SUN_LEN
		#define SUN_LEN(su) \
	        (sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))
	#endif
#endif

//---------------------------------------------------------------------------

#define TELNET_SERVER "Synchronet Telnet Server"
#define STATUS_WFC	"Listening"

#define TIMEOUT_THREAD_WAIT		60			// Seconds (was 15)
#define IO_THREAD_BUF_SIZE	   	20000		// Bytes

// Globals
#ifdef _WIN32
	HANDLE		exec_mutex=NULL;
	HINSTANCE	hK32=NULL;

	#if defined(_DEBUG) && defined(_MSC_VER)
			HANDLE	debug_log=INVALID_HANDLE_VALUE;
		   _CrtMemState mem_chkpoint;
	#endif // _DEBUG && _MSC_VER

#endif // _WIN32

#ifdef USE_CRYPTLIB
	#define SSH_END()	if(ssh)	cryptDestroySession(sbbs->ssh_session);
#else
	#define	SSH_END()
#endif

time_t	uptime=0;
DWORD	served=0;

static	DWORD node_threads_running=0;
static	ulong thread_count=0;
		
char 	lastuseron[LEN_ALIAS+1];  /* Name of user last online */
RingBuf* node_inbuf[MAX_NODES];
SOCKET	spy_socket[MAX_NODES];
#ifdef __unix__
SOCKET	uspy_socket[MAX_NODES];	  /* UNIX domain spy sockets */
#endif
SOCKET	node_socket[MAX_NODES];
static	SOCKET telnet_socket=INVALID_SOCKET;
static	SOCKET rlogin_socket=INVALID_SOCKET;
#ifdef USE_CRYPTLIB
static	SOCKET ssh_socket=INVALID_SOCKET;
#endif
static	sbbs_t*	sbbs=NULL;
static	scfg_t	scfg;
static	char *	text[TOTAL_TEXT];
static	WORD	first_node;
static	WORD	last_node;
static	bool	terminate_server=false;
static	str_list_t recycle_semfiles;
static	str_list_t shutdown_semfiles;
#ifdef _THREAD_SUID_BROKEN
int	thread_suid_broken=TRUE;			/* NPTL is no longer broken */
#endif

extern "C" {

static bbs_startup_t* startup=NULL;

static void status(char* str)
{
	if(startup!=NULL && startup->status!=NULL)
	    startup->status(startup->cbdata,str);
}

static void update_clients()
{
	if(startup!=NULL && startup->clients!=NULL)
		startup->clients(startup->cbdata,node_threads_running);
}

void client_on(SOCKET sock, client_t* client, BOOL update)
{
	if(startup!=NULL && startup->client_on!=NULL)
		startup->client_on(startup->cbdata,TRUE,sock,client,update);
}

static void client_off(SOCKET sock)
{
	if(startup!=NULL && startup->client_on!=NULL)
		startup->client_on(startup->cbdata,FALSE,sock,NULL,FALSE);
}

static void thread_up(BOOL setuid)
{
	thread_count++;
	if(startup!=NULL && startup->thread_up!=NULL)
		startup->thread_up(startup->cbdata,TRUE,setuid);
}

static void thread_down()
{
	if(thread_count>0)
		thread_count--;
	if(startup!=NULL && startup->thread_up!=NULL)
		startup->thread_up(startup->cbdata,FALSE,FALSE);
}

int lputs(int level, char* str)
{
	if(startup==NULL || startup->lputs==NULL || str==NULL)
    	return(0);

    return(startup->lputs(startup->cbdata,level,str));
}

int lprintf(int level, char *fmt, ...)
{
	va_list argptr;
	char sbuf[1024];

    va_start(argptr,fmt);
    vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
    va_end(argptr);
    return(lputs(level,sbuf));
}

int eprintf(int level, char *fmt, ...)
{
	va_list argptr;
	char sbuf[1024];

    if(startup==NULL || startup->event_lputs==NULL)
        return(0);

    va_start(argptr,fmt);
    vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
    va_end(argptr);
	strip_ctrl(sbuf);
    return(startup->event_lputs(level,sbuf));
}

SOCKET open_socket(int type, const char* protocol)
{
	SOCKET	sock;
	char	error[256];

	sock=socket(AF_INET, type, IPPROTO_IP);
	if(sock!=INVALID_SOCKET && startup!=NULL && startup->socket_open!=NULL) 
		startup->socket_open(startup->cbdata,TRUE);
	if(sock!=INVALID_SOCKET && set_socket_options(&scfg, sock, protocol, error, sizeof(error)))
		lprintf(LOG_ERR,"%04d !ERROR %s",sock,error);

	return(sock);
}

SOCKET accept_socket(SOCKET s, SOCKADDR* addr, socklen_t* addrlen)
{
	SOCKET	sock;

	sock=accept(s,addr,addrlen);
	if(sock!=INVALID_SOCKET && startup!=NULL && startup->socket_open!=NULL) 
		startup->socket_open(startup->cbdata,TRUE);

	return(sock);
}

int close_socket(SOCKET sock)
{
	int		result;

	if(sock==INVALID_SOCKET || sock==0)
		return(0);

	shutdown(sock,SHUT_RDWR);	/* required on Unix */
	result=closesocket(sock);
	if(startup!=NULL && startup->socket_open!=NULL)
		startup->socket_open(startup->cbdata,FALSE);
	if(result!=0 && ERROR_VALUE!=ENOTSOCK)
		lprintf(LOG_ERR,"!ERROR %d closing socket %d",ERROR_VALUE,sock);
	return(result);
}


u_long resolve_ip(char *addr)
{
	HOSTENT*	host;
	char*		p;

	if(*addr==0)
		return((u_long)INADDR_NONE);

	for(p=addr;*p;p++)
		if(*p!='.' && !isdigit(*p))
			break;
	if(!(*p))
		return(inet_addr(addr));
	if((host=gethostbyname(addr))==NULL) 
		return((u_long)INADDR_NONE);
	return(*((ulong*)host->h_addr_list[0]));
}

} /* extern "C" */

#ifdef _WINSOCKAPI_

WSADATA WSAData;
#define SOCKLIB_DESC WSAData.szDescription
static BOOL WSAInitialized=FALSE;

static BOOL winsock_startup(void)
{
	int		status;             /* Status Code */

    if((status = WSAStartup(MAKEWORD(1,1), &WSAData))==0) {
		lprintf(LOG_INFO,"%s %s",WSAData.szDescription, WSAData.szSystemStatus);
		WSAInitialized=TRUE;
		return(TRUE);
	}

    lprintf(LOG_ERR,"!WinSock startup ERROR %d", status);
	return(FALSE);
}

#else /* No WINSOCK */

#define winsock_startup()	(TRUE)
#define SOCKLIB_DESC NULL

#endif

DLLEXPORT void DLLCALL sbbs_srand()
{
	DWORD seed = time(NULL) ^ (DWORD)GetCurrentThreadId();

#if defined(HAS_DEV_RANDOM) && defined(RANDOM_DEV)
	int     rf;

	if((rf=open(RANDOM_DEV, O_RDONLY))!=-1) {
		read(rf, &seed, sizeof(seed));
		close(rf);
	}
#endif

 	srand(seed);
	sbbs_random(10);	/* Throw away first number */
}

int DLLCALL sbbs_random(int n)
{
	return(xp_random(n));
}

#ifdef JAVASCRIPT

static js_server_props_t js_server_props;

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
		if((array=JS_NewArrayObject(cx, 0, NULL))==NULL)	/* Assertion here, in _heap_alloc_dbg, June-21-2004 */
			return(JS_FALSE);								/* Caused by nntpservice.js? */

	if(!JS_DefineProperty(cx, parent, name, OBJECT_TO_JSVAL(array)
		,NULL,NULL,flags))
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

	return(JS_TRUE);
}

/* Convert from Synchronet-specific jsSyncMethodSpec to JSAPI's JSFunctionSpec */

JSBool
DLLCALL js_DescribeSyncObject(JSContext* cx, JSObject* obj, const char* str, int ver)
{
	JSString* js_str = JS_NewStringCopyZ(cx, str);

	if(js_str==NULL)
		return(JS_FALSE);

	if(ver < 10000)		/* auto convert 313 to 31300 */
		ver*=100;

	return(JS_DefineProperty(cx,obj,"_description"
			,STRING_TO_JSVAL(js_str),NULL,NULL,JSPROP_READONLY)
		&& JS_DefineProperty(cx,obj,"_ver"
			,INT_TO_JSVAL(ver),NULL,NULL,JSPROP_READONLY));
}

JSBool
DLLCALL js_DescribeSyncConstructor(JSContext* cx, JSObject* obj, const char* str)
{
	JSString* js_str = JS_NewStringCopyZ(cx, str);

	if(js_str==NULL)
		return(JS_FALSE);

	return(JS_DefineProperty(cx,obj,"_constructor"
		,STRING_TO_JSVAL(js_str),NULL,NULL,JSPROP_READONLY));
}

#ifdef BUILD_JSDOCS

static const char* method_array_name = "_method_list";
static const char* propver_array_name = "_property_ver_list";

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
	"array",
	"alias",
	"undefined",
	"null"
};

JSBool
DLLCALL js_DefineSyncProperties(JSContext *cx, JSObject *obj, jsSyncPropertySpec* props)
{
	uint		i;
	long		ver;
	jsval		val;
	jsuint		len=0;
	JSObject*	array;

	if((array=JS_NewArrayObject(cx, 0, NULL))==NULL)
		return(JS_FALSE);

	if(!JS_DefineProperty(cx, obj, propver_array_name, OBJECT_TO_JSVAL(array)
		,NULL,NULL,JSPROP_READONLY))
		return(JS_FALSE);

	for(i=0;props[i].name;i++) {
		if(!JS_DefinePropertyWithTinyId(cx, obj, /* Never reserve any "slots" for properties */
			props[i].name,props[i].tinyid, JSVAL_VOID, NULL, NULL, props[i].flags|JSPROP_SHARED))
			return(JS_FALSE);
		if(props[i].flags&JSPROP_ENUMERATE) {	/* No need to version invisible props */
			if((ver=props[i].ver) < 10000)		/* auto convert 313 to 31300 */
				ver*=100;
			val = INT_TO_JSVAL(ver);
			if(!JS_SetElement(cx, array, len++, &val))
				return(JS_FALSE);
		}
	}

	return(JS_TRUE);
}

JSBool 
DLLCALL js_DefineSyncMethods(JSContext* cx, JSObject* obj, jsSyncMethodSpec *funcs, BOOL append)
{
	int			i;
	jsuint		len=0;
	long		ver;
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

	if(!JS_DefineProperty(cx, obj, method_array_name, OBJECT_TO_JSVAL(method_array)
		, NULL, NULL, 0))
		return(JS_FALSE);

	if(append)
		if(!JS_GetArrayLength(cx, method_array, &len))
			return(JS_FALSE);

	for(i=0;funcs[i].name;i++) {

		if(!JS_DefineFunction(cx, obj, funcs[i].name, funcs[i].call, funcs[i].nargs, 0))
			return(JS_FALSE);

		if(funcs[i].type==JSTYPE_ALIAS)
			continue;

		method = JS_NewObject(cx, NULL, NULL, method_array);	/* exception here June-7-2003 */

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

		if(funcs[i].ver) {
			if((ver=funcs[i].ver) < 10000)		/* auto convert 313 to 31300 */
				ver*=100;
			val = INT_TO_JSVAL(ver);
			JS_SetProperty(cx,method, "ver", &val);
		}

		val=OBJECT_TO_JSVAL(method);
		if(!JS_SetElement(cx, method_array, len+i, &val))
			return(JS_FALSE);
	}

	return(JS_TRUE);
}

/*
 * Always resolve all here since
 * 1) We'll always be enumerating anyways
 * 2) The speed penalty won't be seen in production code anyways
 */
JSBool
DLLCALL js_SyncResolve(JSContext* cx, JSObject* obj, char *name, jsSyncPropertySpec* props, jsSyncMethodSpec* funcs, jsConstIntSpec* consts, int flags)
{
	JSBool	ret=JS_TRUE;

	if(props) {
		if(!js_DefineSyncProperties(cx, obj, props))
			ret=JS_FALSE;
	}
		
	if(funcs) {
		if(!js_DefineSyncMethods(cx, obj, funcs, 0))
			ret=JS_FALSE;
	}

	if(consts) {
		if(!js_DefineConstIntegers(cx, obj, consts, flags))
			ret=JS_FALSE;
	}

	return(ret);
}

#else // NON-JSDOCS

JSBool
DLLCALL js_DefineSyncProperties(JSContext *cx, JSObject *obj, jsSyncPropertySpec* props)
{
	uint i;

	for(i=0;props[i].name;i++) 
		if(!JS_DefinePropertyWithTinyId(cx, obj, 
			props[i].name,props[i].tinyid, JSVAL_VOID, NULL, NULL, props[i].flags|JSPROP_SHARED))
			return(JS_FALSE);

	return(JS_TRUE);
}


JSBool 
DLLCALL js_DefineSyncMethods(JSContext* cx, JSObject* obj, jsSyncMethodSpec *funcs, BOOL append)
{
	uint i;

	for(i=0;funcs[i].name;i++)
		if(!JS_DefineFunction(cx, obj, funcs[i].name, funcs[i].call, funcs[i].nargs, 0))
			return(JS_FALSE);
	return(JS_TRUE);
}

JSBool
DLLCALL js_SyncResolve(JSContext* cx, JSObject* obj, char *name, jsSyncPropertySpec* props, jsSyncMethodSpec* funcs, jsConstIntSpec* consts, int flags)
{
	uint i;
	jsval	val;

	if(props) {
		for(i=0;props[i].name;i++) {
			if(name==NULL || strcmp(name, props[i].name)==0) {
				if(!JS_DefinePropertyWithTinyId(cx, obj, 
						props[i].name,props[i].tinyid, JSVAL_VOID, NULL, NULL, props[i].flags|JSPROP_SHARED))
					return(JS_FALSE);
				if(name)
					return(JS_TRUE);
			}
		}
	}
	if(funcs) {
		for(i=0;funcs[i].name;i++) {
			if(name==NULL || strcmp(name, funcs[i].name)==0) {
				if(!JS_DefineFunction(cx, obj, funcs[i].name, funcs[i].call, funcs[i].nargs, 0))
					return(JS_FALSE);
				if(name)
					return(JS_TRUE);
			}
		}
	}
	if(consts) {
		for(i=0;consts[i].name;i++) {
			if(name==NULL || strcmp(name, consts[i].name)==0) {
	        	if(!JS_NewNumberValue(cx, consts[i].val, &val))
					return(JS_FALSE);

				if(!JS_DefineProperty(cx, obj, consts[i].name, val ,NULL, NULL, flags))
					return(JS_FALSE);

				if(name)
					return(JS_TRUE);
			}
		}
	}

	return(JS_TRUE);
}

#endif

/* This is a stream-lined version of JS_DefineConstDoubles */
JSBool
DLLCALL js_DefineConstIntegers(JSContext* cx, JSObject* obj, jsConstIntSpec* ints, int flags)
{
	uint	i;
	jsval	val;

	for(i=0;ints[i].name;i++) {
        if(!JS_NewNumberValue(cx, ints[i].val, &val))
			return(JS_FALSE);

		if(!JS_DefineProperty(cx, obj, ints[i].name, val ,NULL, NULL, flags))
			return(JS_FALSE);
	}

	return(JS_TRUE);
}

char*
DLLCALL js_ValueToStringBytes(JSContext* cx, jsval val, size_t* len)
{
	JSString* str;
	
	if((str=JS_ValueToString(cx, val))==NULL)
		return(NULL);

	if(len!=NULL)
		*len = JS_GetStringLength(str);

	return(JS_GetStringBytes(str));
}

static JSBool
js_log(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN		i=0;
	int32		level=LOG_INFO;
    JSString*	str=NULL;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc > 1 && JSVAL_IS_NUMBER(argv[i]))
		JS_ValueToInt32(cx,argv[i++],&level);

    for(; i<argc; i++) {
		if((str=JS_ValueToString(cx, argv[i]))==NULL)
		    return(JS_FALSE);
		if(sbbs->online==ON_LOCAL) {
			if(startup!=NULL && startup->event_lputs!=NULL)
				startup->event_lputs(level,JS_GetStringBytes(str));
		} else
			lputs(level,JS_GetStringBytes(str));
	}

	if(str==NULL)
		*rval = JSVAL_VOID;
	else
		*rval = STRING_TO_JSVAL(str);
    return(JS_TRUE);
}

static JSBool
js_read(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	uchar*		buf;
	int32		len=128;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc)
		JS_ValueToInt32(cx,argv[0],&len);

	if((buf=(uchar*)malloc(len))==NULL)
		return(JS_TRUE);

	len=RingBufRead(&sbbs->inbuf,buf,len);

	if(len>0)
		*rval = STRING_TO_JSVAL(JS_NewStringCopyN(cx,(char*)buf,len));

	free(buf);
	return(JS_TRUE);
}

static JSBool
js_readln(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		buf;
	int32		len=128;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc)
		JS_ValueToInt32(cx,argv[0],&len);

	if((buf=(char*)malloc(len))==NULL)
		return(JS_TRUE);

	len=sbbs->getstr(buf,len,K_NONE);

	if(len>0)
		*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,buf));

	free(buf);
	return(JS_TRUE);
}

static JSBool
js_write(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN		i;
    JSString*	str=NULL;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

    for (i = 0; i < argc; i++) {
		if((str=JS_ValueToString(cx, argv[i]))==NULL)
		    return(JS_FALSE);
		if(sbbs->online==ON_LOCAL)
			eprintf(LOG_INFO,"%s",JS_GetStringBytes(str));
		else
			sbbs->bputs(JS_GetStringBytes(str));
	}

	if(str==NULL)
		*rval = JSVAL_VOID;
	else
		*rval = STRING_TO_JSVAL(str);
    return(JS_TRUE);
}

static JSBool
js_write_raw(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    uintN		i;
    char*	str=NULL;
	size_t		len;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

    for (i = 0; i < argc; i++) {
		if((str=js_ValueToStringBytes(cx, argv[i], &len))==NULL)
		    return(JS_FALSE);
		sbbs->putcom(str, len);
	}

    return(JS_TRUE);
}

static JSBool
js_writeln(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	js_write(cx,obj,argc,argv,rval);
	if(sbbs->online==ON_REMOTE)
		sbbs->bputs(crlf);

    return(JS_TRUE);
}

static JSBool
js_printf(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	sbbs_t*		sbbs;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((p = js_sprintf(cx, 0, argc, argv))==NULL) {
		JS_ReportError(cx,"js_sprintf failed");
		return(JS_FALSE);
	}

	if(sbbs->online==ON_LOCAL)
		eprintf(LOG_INFO,"%s",p);
	else
		sbbs->bputs(p);

	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, p));

	js_sprintf_free(p);

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

static jsSyncMethodSpec js_global_functions[] = {
	{"log",				js_log,				1,	JSTYPE_STRING,	JSDOCSTR("[level,] value [,value]")
	,JSDOCSTR("add a line of text to the server and/or system log, "
		"<i>values</i> are typically string constants or variables, "
		"<i>level</i> is the debug level/priority (default: <tt>LOG_INFO</tt>)")
	,311
	},
	{"read",			js_read,			0,	JSTYPE_STRING,	JSDOCSTR("[count]")
	,JSDOCSTR("read up to count characters from input stream")
	,311
	},
	{"readln",			js_readln,			0,	JSTYPE_STRING,	JSDOCSTR("[count]")
	,JSDOCSTR("read a single line, up to count characters, from input stream")
	,311
	},
	{"write",			js_write,			0,	JSTYPE_VOID,	JSDOCSTR("value [,value]")
	,JSDOCSTR("send one or more values (typically strings) to the server output")
	,311
	},
	{"write_raw",			js_write_raw,			0,	JSTYPE_VOID,	JSDOCSTR("value [,value]")
	,JSDOCSTR("send a stream of bytes (possibly containing NULLs or special control code sequences) to the server output")
	,314
	},
	{"print",			js_writeln,			0,	JSTYPE_ALIAS },
    {"writeln",         js_writeln,         0,	JSTYPE_VOID,	JSDOCSTR("value [,value]")
	,JSDOCSTR("send a line of text to the console or event log with automatic line termination (CRLF), "
		"<i>values</i> are typically string constants or variables (AKA print)")
	,311
	},
    {"printf",          js_printf,          1,	JSTYPE_STRING,	JSDOCSTR("string format [,value][,value]")
	,JSDOCSTR("print a formatted string - <small>CAUTION: for experienced C programmers ONLY</small>")
	,310
	},	
	{"alert",			js_alert,			1,	JSTYPE_VOID,	JSDOCSTR("value")
	,JSDOCSTR("print an alert message (ala client-side JS)")
	,310
	},
	{"prompt",			js_prompt,			1,	JSTYPE_STRING,	JSDOCSTR("[value]")
	,JSDOCSTR("displays a prompt (<i>value</i>) and returns a string of user input (ala clent-side JS)")
	,310
	},
	{"confirm",			js_confirm,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("value")
	,JSDOCSTR("displays a Yes/No prompt and returns <i>true</i> or <i>false</i> "
		"based on users confirmation (ala client-side JS)")
	,310
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
		lprintf(LOG_ERR,"!JavaScript: %s", message);
		return;
    }

	if(report->filename)
		SAFEPRINTF(file," %s",report->filename);
	else
		file[0]=0;

	if(report->lineno)
		SAFEPRINTF(line," line %d",report->lineno);
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
		eprintf(LOG_ERR,"!JavaScript %s%s%s: %s",warning,file,line,message);
	else {
		lprintf(LOG_ERR,"!JavaScript %s%s%s: %s",warning,file,line,message);
		sbbs->bprintf("!JavaScript %s%s%s: %s\r\n",warning,file,line,message);
	}
}

bool sbbs_t::js_init(ulong* stack_frame)
{
	char		node[128];

    if(cfg.node_num)
    	SAFEPRINTF(node,"Node %d",cfg.node_num);
    else
    	SAFECOPY(node,client_name);

	if(startup->js.max_bytes==0)			startup->js.max_bytes=JAVASCRIPT_MAX_BYTES;
	if(startup->js.cx_stack==0)				startup->js.cx_stack=JAVASCRIPT_CONTEXT_STACK;

	lprintf(LOG_DEBUG,"%s JavaScript: Creating runtime: %lu bytes"
		,node,startup->js.max_bytes);

	if((js_runtime = JS_NewRuntime(startup->js.max_bytes))==NULL)
		return(false);

	lprintf(LOG_DEBUG,"%s JavaScript: Initializing context (stack: %lu bytes)"
		,node,startup->js.cx_stack);

    if((js_cx = JS_NewContext(js_runtime, startup->js.cx_stack))==NULL)
		return(false);
	
	memset(&js_branch,0,sizeof(js_branch));
	js_branch.limit = startup->js.branch_limit;
	js_branch.gc_interval = startup->js.gc_interval;
	js_branch.yield_interval = startup->js.yield_interval;
	js_branch.terminated = &terminated;
	js_branch.auto_terminate = TRUE;

	bool success=false;

	do {

		JS_SetErrorReporter(js_cx, js_ErrorReporter);

		JS_SetContextPrivate(js_cx, this);	/* Store a pointer to sbbs_t instance */

		/* Global Objects (including system, js, client, Socket, MsgBase, File, User, etc. */
		if((js_glob=js_CreateCommonObjects(js_cx, &scfg, &cfg, js_global_functions
					,uptime, startup->host_name, SOCKLIB_DESC	/* system */
					,&js_branch									/* js */
					,&client, client_socket						/* client */
					,&js_server_props							/* server */
			))==NULL)
			break;

		/* BBS Object */
		if(js_CreateBbsObject(js_cx, js_glob)==NULL)
			break;

		/* Console Object */
		if(js_CreateConsoleObject(js_cx, js_glob)==NULL)
			break;

		if(startup->js.thread_stack) {
			ulong stack_limit;

#if JS_STACK_GROWTH_DIRECTION > 0
			stack_limit=((ulong)stack_frame)+startup->js.thread_stack;
#else
			stack_limit=((ulong)stack_frame)-startup->js.thread_stack;
#endif
			JS_SetThreadStackLimit(js_cx, stack_limit);

			lprintf(LOG_DEBUG,"%s JavaScript: Thread stack limit: %lu bytes"
				,node, startup->js.thread_stack);
		}

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
		lprintf(LOG_ERR,"!JavaScript ERROR creating user objects");
}

#endif	/* JAVASCRIPT */

static BYTE* telnet_interpret(sbbs_t* sbbs, BYTE* inbuf, int inlen,
  									BYTE* outbuf, int& outlen)
{
	BYTE*   first_iac=NULL;
	BYTE*	first_cr=NULL;
	int 	i;

	if(inlen<1) {
		outlen=0;
		return(inbuf);	// no length? No interpretation
	}

    first_iac=(BYTE*)memchr(inbuf, TELNET_IAC, inlen);

	if(!(sbbs->telnet_mode&TELNET_MODE_GATE) 
		&& sbbs->telnet_remote_option[TELNET_BINARY_TX]!=TELNET_WILL
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
		if(!(sbbs->telnet_mode&TELNET_MODE_GATE) 
			&& sbbs->telnet_remote_option[TELNET_BINARY_TX]!=TELNET_WILL
			&& !(sbbs->console&CON_RAW_IN)) {
			if(sbbs->telnet_last_rxch==CR
				&& (inbuf[i]==LF || inbuf[i]==0)) { // CR/LF or CR/NUL, ignore 2nd char
#if 0 /* Debug CR/LF problems */
				lprintf(LOG_INFO,"Node %d CR/%02Xh detected and ignored"
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

			if(sbbs->telnet_cmdlen<sizeof(sbbs->telnet_cmd))
				sbbs->telnet_cmd[sbbs->telnet_cmdlen++]=inbuf[i];

			uchar command	= sbbs->telnet_cmd[1];
			uchar option	= sbbs->telnet_cmd[2];

			if(sbbs->telnet_cmdlen>=2 && command==TELNET_SB) {
				if(inbuf[i]==TELNET_SE 
					&& sbbs->telnet_cmd[sbbs->telnet_cmdlen-2]==TELNET_IAC) {

					if(startup->options&BBS_OPT_DEBUG_TELNET)
						lprintf(LOG_DEBUG,"Node %d %s Telnet sub-negotiation command: %s"
	                		,sbbs->cfg.node_num
							,sbbs->telnet_mode&TELNET_MODE_GATE ? "passed-through" : "received"
							,telnet_opt_desc(option));

					/* sub-option terminated */
					if(option==TELNET_TERM_TYPE
						&& sbbs->telnet_cmd[3]==TELNET_TERM_IS) {
						sprintf(sbbs->terminal,"%.*s",(int)sbbs->telnet_cmdlen-6,sbbs->telnet_cmd+4);
						lprintf(LOG_DEBUG,"Node %d %s telnet terminal type: %s"
	                		,sbbs->cfg.node_num
							,sbbs->telnet_mode&TELNET_MODE_GATE ? "passed-through" : "received"
							,sbbs->terminal);

					} else if(option==TELNET_TERM_SPEED
						&& sbbs->telnet_cmd[3]==TELNET_TERM_IS) {
						char speed[128];
						sprintf(speed,"%.*s",(int)sbbs->telnet_cmdlen-6,sbbs->telnet_cmd+4);
						lprintf(LOG_DEBUG,"Node %d %s telnet terminal speed: %s"
	                		,sbbs->cfg.node_num
							,sbbs->telnet_mode&TELNET_MODE_GATE ? "passed-through" : "received"
							,speed);
						sbbs->cur_rate=atoi(speed);
						sbbs->cur_cps=sbbs->cur_rate/10;

					} else if(option==TELNET_SEND_LOCATION) {
						safe_snprintf(sbbs->telnet_location
							,sizeof(sbbs->telnet_location)
							,"%.*s",(int)sbbs->telnet_cmdlen-5,sbbs->telnet_cmd+3);
						lprintf(LOG_DEBUG,"Node %d %s telnet location: %s"
	                		,sbbs->cfg.node_num
							,sbbs->telnet_mode&TELNET_MODE_GATE ? "passed-through" : "received"
							,sbbs->telnet_location);

					} else if(option==TELNET_NEGOTIATE_WINDOW_SIZE) {
						long cols = (sbbs->telnet_cmd[3]<<8) | sbbs->telnet_cmd[4];
						long rows = (sbbs->telnet_cmd[5]<<8) | sbbs->telnet_cmd[6];
						lprintf(LOG_DEBUG,"Node %d %s telnet window size: %ux%u"
	                		,sbbs->cfg.node_num
							,sbbs->telnet_mode&TELNET_MODE_GATE ? "passed-through" : "received"
							,cols
							,rows);
						if(rows && !sbbs->useron.rows)	/* auto-detect rows */
							sbbs->rows=rows;
						if(cols)
							sbbs->cols=cols;

					} else if(startup->options&BBS_OPT_DEBUG_TELNET)
            			lprintf(LOG_DEBUG,"Node %d %s unsupported telnet sub-negotiation cmd: %s"
	                		,sbbs->cfg.node_num
							,sbbs->telnet_mode&TELNET_MODE_GATE ? "passed-through" : "received"
                			,telnet_opt_desc(option));
					sbbs->telnet_cmdlen=0;
				}
			}
            else if(sbbs->telnet_cmdlen==2 && inbuf[i]<TELNET_WILL) {
				if(startup->options&BBS_OPT_DEBUG_TELNET)
            		lprintf(LOG_DEBUG,"Node %d %s telnet cmd: %s"
	                	,sbbs->cfg.node_num
						,sbbs->telnet_mode&TELNET_MODE_GATE ? "passed-through" : "received"
                		,telnet_cmd_desc(option));
                sbbs->telnet_cmdlen=0;
            }
            else if(sbbs->telnet_cmdlen>=3) {	/* telnet option negotiation */

				if(startup->options&BBS_OPT_DEBUG_TELNET)
					lprintf(LOG_DEBUG,"Node %d %s telnet cmd: %s %s"
						,sbbs->cfg.node_num
						,sbbs->telnet_mode&TELNET_MODE_GATE ? "passed-through" : "received"
						,telnet_cmd_desc(command)
						,telnet_opt_desc(option));

				if(!(sbbs->telnet_mode&TELNET_MODE_GATE)) {
					if(command==TELNET_DO || command==TELNET_DONT) {	/* local options */
						if(sbbs->telnet_local_option[option]!=command) {
							sbbs->telnet_local_option[option]=command;
							sbbs->send_telnet_cmd(telnet_opt_ack(command),option);
						}
					} else { /* WILL/WONT (remote options) */ 
						if(sbbs->telnet_remote_option[option]!=command) {	
						
							switch(option) {
								case TELNET_BINARY_TX:
								case TELNET_ECHO:
								case TELNET_TERM_TYPE:
								case TELNET_TERM_SPEED:
								case TELNET_SUP_GA:
								case TELNET_NEGOTIATE_WINDOW_SIZE:
								case TELNET_SEND_LOCATION:
									sbbs->telnet_remote_option[option]=command;
									sbbs->send_telnet_cmd(telnet_opt_ack(command),option);
									break;
								default: /* unsupported remote options */
									if(command==TELNET_WILL) /* NAK */
										sbbs->send_telnet_cmd(telnet_opt_nak(command),option);
									break;
							}
						}

						if(command==TELNET_WILL && option==TELNET_TERM_TYPE) {
							if(startup->options&BBS_OPT_DEBUG_TELNET)
								lprintf(LOG_DEBUG,"Node %d requesting telnet terminal type"
									,sbbs->cfg.node_num);

							char	buf[64];
							sprintf(buf,"%c%c%c%c%c%c"
								,TELNET_IAC,TELNET_SB
								,TELNET_TERM_TYPE,TELNET_TERM_SEND
								,TELNET_IAC,TELNET_SE);
							sbbs->putcom(buf,6);
						}
						else if(command==TELNET_WILL && option==TELNET_TERM_SPEED) {
							if(startup->options&BBS_OPT_DEBUG_TELNET)
								lprintf(LOG_DEBUG,"Node %d requesting telnet terminal speed"
									,sbbs->cfg.node_num);

							char	buf[64];
							sprintf(buf,"%c%c%c%c%c%c"
								,TELNET_IAC,TELNET_SB
								,TELNET_TERM_SPEED,TELNET_TERM_SEND
								,TELNET_IAC,TELNET_SE);
							sbbs->putcom(buf,6);
						}
					}
				}

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
	
	if(telnet_mode&TELNET_MODE_OFF)	
		return;

	if(cmd<TELNET_WILL) {
		if(startup->options&BBS_OPT_DEBUG_TELNET)
            lprintf(LOG_DEBUG,"Node %d sending telnet cmd: %s"
	            ,cfg.node_num
                ,telnet_cmd_desc(cmd));
		sprintf(buf,"%c%c",TELNET_IAC,cmd);
		putcom(buf,2);
	} else {
		if(startup->options&BBS_OPT_DEBUG_TELNET)
			lprintf(LOG_DEBUG,"Node %d sending telnet cmd: %s %s"
				,cfg.node_num
				,telnet_cmd_desc(cmd)
				,telnet_opt_desc(opt));
		sprintf(buf,"%c%c%c",TELNET_IAC,cmd,opt);
		putcom(buf,3);
	}
}

void sbbs_t::request_telnet_opt(uchar cmd, uchar opt)
{
	if(cmd==TELNET_DO || cmd==TELNET_DONT) {	/* remote option */
		if(telnet_remote_option[opt]==telnet_opt_ack(cmd))
			return;	/* already set in this mode, do nothing */
		telnet_remote_option[opt]=telnet_opt_ack(cmd);
	} else {	/* local option */
		if(telnet_local_option[opt]==telnet_opt_ack(cmd))
			return;	/* already set in this mode, do nothing */
		telnet_local_option[opt]=telnet_opt_ack(cmd);
	}
	send_telnet_cmd(cmd,opt);
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
	SOCKET		high_socket;
	SOCKET		sock;

	thread_up(TRUE /* setuid */);

#ifdef _DEBUG
	lprintf(LOG_DEBUG,"Node %d input thread started",sbbs->cfg.node_num);
#endif

	pthread_mutex_init(&sbbs->input_thread_mutex,NULL);
    sbbs->input_thread_running = true;
	sbbs->console|=CON_R_INPUT;

	while(sbbs->online && sbbs->client_socket!=INVALID_SOCKET
		&& node_socket[sbbs->cfg.node_num-1]!=INVALID_SOCKET) {

		if(pthread_mutex_lock(&sbbs->input_thread_mutex)!=0)
			sbbs->errormsg(WHERE,ERR_LOCK,"input_thread_mutex",0);

		FD_ZERO(&socket_set);
		FD_SET(sbbs->client_socket,&socket_set);
		high_socket=sbbs->client_socket;
#ifdef __unix__
		if(uspy_socket[sbbs->cfg.node_num-1]!=INVALID_SOCKET)  {
			FD_SET(uspy_socket[sbbs->cfg.node_num-1],&socket_set);
			if(uspy_socket[sbbs->cfg.node_num-1] > high_socket)
				high_socket=uspy_socket[sbbs->cfg.node_num-1];
		}
#endif

		tv.tv_sec=1;
		tv.tv_usec=0;

		if((i=select(high_socket+1,&socket_set,NULL,NULL,&tv))<1) {
			if(pthread_mutex_unlock(&sbbs->input_thread_mutex)!=0)
				sbbs->errormsg(WHERE,ERR_UNLOCK,"input_thread_mutex",0);
			if(i==0) {
				YIELD();	/* This kludge is necessary on some Linux distros */
				continue;	/* to allow other threads to lock the input_thread_mutex */
			}

			if(sbbs->client_socket==INVALID_SOCKET)
				break;
#ifdef __unix__
			if(uspy_socket[sbbs->cfg.node_num-1]!=INVALID_SOCKET) {
				if(!socket_check(uspy_socket[sbbs->cfg.node_num-1],NULL,NULL,0)) {
					close_socket(uspy_socket[sbbs->cfg.node_num-1]);
					lprintf(LOG_NOTICE,"Closing local spy socket: %d",uspy_socket[sbbs->cfg.node_num-1]);
					uspy_socket[sbbs->cfg.node_num-1]=INVALID_SOCKET;
					continue;
				}
			}
#endif
	       	if(ERROR_VALUE == ENOTSOCK)
    	        lprintf(LOG_NOTICE,"Node %d socket closed by peer on input->select", sbbs->cfg.node_num);
			else if(ERROR_VALUE==ESHUTDOWN)
				lprintf(LOG_NOTICE,"Node %d socket shutdown on input->select", sbbs->cfg.node_num);
			else if(ERROR_VALUE==EINTR)
				lprintf(LOG_DEBUG,"Node %d input thread interrupted",sbbs->cfg.node_num);
            else if(ERROR_VALUE==ECONNRESET) 
				lprintf(LOG_NOTICE,"Node %d connection reset by peer on input->select", sbbs->cfg.node_num);
	        else if(ERROR_VALUE==ECONNABORTED) 
				lprintf(LOG_NOTICE,"Node %d connection aborted by peer on input->select", sbbs->cfg.node_num);
			else
				lprintf(LOG_WARNING,"Node %d !ERROR %d input->select socket %d"
               		,sbbs->cfg.node_num, ERROR_VALUE, sbbs->client_socket);
			break;
		}

		if(sbbs->client_socket==INVALID_SOCKET) {
			if(pthread_mutex_unlock(&sbbs->input_thread_mutex)!=0)
				sbbs->errormsg(WHERE,ERR_UNLOCK,"input_thread_mutex",0);
			break;
		}

/*         ^          ^
 *		\______    ______/
 *       \  * \   / *   /
 *        -----   ------           /----\
 *              ||               -< Boo! |
 *             /__\                \----/
 *       \______________/
 *        \/\/\/\/\/\/\/
 *         ------------
 */

		if(FD_ISSET(sbbs->client_socket,&socket_set))
			sock=sbbs->client_socket;
#ifdef __unix__
		else if(uspy_socket[sbbs->cfg.node_num-1]!=INVALID_SOCKET
				&& FD_ISSET(uspy_socket[sbbs->cfg.node_num-1],&socket_set))  {
			if(!socket_check(uspy_socket[sbbs->cfg.node_num-1],NULL,NULL,0)) {
				close_socket(uspy_socket[sbbs->cfg.node_num-1]);
				lprintf(LOG_NOTICE,"Closing local spy socket: %d",uspy_socket[sbbs->cfg.node_num-1]);
				uspy_socket[sbbs->cfg.node_num-1]=INVALID_SOCKET;
				if(pthread_mutex_unlock(&sbbs->input_thread_mutex)!=0)
					sbbs->errormsg(WHERE,ERR_UNLOCK,"input_thread_mutex",0);
				continue;
			}
			sock=uspy_socket[sbbs->cfg.node_num-1];
		}
#endif
		else {
			if(pthread_mutex_unlock(&sbbs->input_thread_mutex)!=0)
				sbbs->errormsg(WHERE,ERR_UNLOCK,"input_thread_mutex",0);
			continue;
		}

    	rd=RingBufFree(&sbbs->inbuf);

		if(!rd) { // input buffer full
			lprintf(LOG_WARNING,"Node %d !WARNING input buffer full", sbbs->cfg.node_num);
        	// wait up to 5 seconds to empty (1 byte min)
			time_t start=time(NULL);
            while((rd=RingBufFree(&sbbs->inbuf))==0) {
            	if(time(NULL)-start>=5) {
                	rd=1;
					if(pthread_mutex_unlock(&sbbs->input_thread_mutex)!=0)
						sbbs->errormsg(WHERE,ERR_UNLOCK,"input_thread_mutex",0);
                	break;
                }
                YIELD();
            }
		}
		
	    if(rd > (int)sizeof(inbuf))
        	rd=sizeof(inbuf);

#ifdef USE_CRYPTLIB
		if(sbbs->ssh_mode && sock==sbbs->client_socket) {
			int err;
			if(!cryptStatusOK((err=cryptPopData(sbbs->ssh_session, (char*)inbuf, rd, &i)))) {
				if(pthread_mutex_unlock(&sbbs->input_thread_mutex)!=0)
					sbbs->errormsg(WHERE,ERR_UNLOCK,"input_thread_mutex",0);
				if(err==CRYPT_ERROR_TIMEOUT)
					continue;
				/* Handle the SSH error here... */
				lprintf(LOG_ERR,"Node %d !ERROR %d receiving on Cryptlib session", sbbs->cfg.node_num, err);
				break;
			}
			else {
				if(!i) {
					if(pthread_mutex_unlock(&sbbs->input_thread_mutex)!=0)
						sbbs->errormsg(WHERE,ERR_UNLOCK,"input_thread_mutex",0);
					continue;
				}
				rd=i;
			}
		}
		else
#endif
    	rd = recv(sock, (char*)inbuf, rd, 0);

		if(pthread_mutex_unlock(&sbbs->input_thread_mutex)!=0)
			sbbs->errormsg(WHERE,ERR_UNLOCK,"input_thread_mutex",0);

		if(rd == SOCKET_ERROR)
		{
#ifdef __unix__
			if(sock==sbbs->client_socket)  {
#endif
	        	if(ERROR_VALUE == ENOTSOCK)
    	            lprintf(LOG_NOTICE,"Node %d socket closed by peer on receive", sbbs->cfg.node_num);
        	    else if(ERROR_VALUE==ECONNRESET) 
					lprintf(LOG_NOTICE,"Node %d connection reset by peer on receive", sbbs->cfg.node_num);
				else if(ERROR_VALUE==ESHUTDOWN)
					lprintf(LOG_NOTICE,"Node %d socket shutdown on receive", sbbs->cfg.node_num);
        	    else if(ERROR_VALUE==ECONNABORTED) 
					lprintf(LOG_NOTICE,"Node %d connection aborted by peer on receive", sbbs->cfg.node_num);
				else
					lprintf(LOG_WARNING,"Node %d !ERROR %d receiving from socket %d"
        	        	,sbbs->cfg.node_num, ERROR_VALUE, sock);
				break;
#ifdef __unix__
			} else  {
				if(ERROR_VALUE != EAGAIN)  {
					lprintf(LOG_ERR,"Node %d !ERROR %d on local spy socket %d receive"
						, sbbs->cfg.node_num, errno, sock);
					close_socket(uspy_socket[sbbs->cfg.node_num-1]);
					uspy_socket[sbbs->cfg.node_num-1]=INVALID_SOCKET;
				}
				continue;
			}
#endif
		}

		if(rd == 0 && sock==sbbs->client_socket)
		{
			lprintf(LOG_NOTICE,"Node %d disconnected", sbbs->cfg.node_num);
			break;
		}

		total_recv+=rd;
		total_pkts++;

        // telbuf and wr are modified to reflect telnet escaped data
		wr=rd;
#ifdef __unix__
		if(sock!=sbbs->client_socket)
			wrbuf=inbuf;
		else
#endif
		if(sbbs->telnet_mode&TELNET_MODE_OFF)
			wrbuf=inbuf;
		else
			wrbuf=telnet_interpret(sbbs, inbuf, rd, telbuf, wr);
		if(wr > (int)sizeof(telbuf)) 
			lprintf(LOG_ERR,"!TELBUF OVERFLOW (%d>%d)",wr,sizeof(telbuf));

		/* First level Ctrl-C checking */
		if(!(sbbs->cfg.ctrlkey_passthru&(1<<CTRL_C))
			&& sbbs->rio_abortable 
			&& !(sbbs->telnet_mode&TELNET_MODE_GATE)
			&& sbbs->telnet_remote_option[TELNET_BINARY_TX]!=TELNET_WILL
			&& memchr(wrbuf, CTRL_C, wr)) {	
			if(RingBufFull(&sbbs->inbuf))
    			lprintf(LOG_DEBUG,"Node %d Ctrl-C hit with %lu bytes in input buffer"
					,sbbs->cfg.node_num,RingBufFull(&sbbs->inbuf));
			if(RingBufFull(&sbbs->outbuf))
    			lprintf(LOG_DEBUG,"Node %d Ctrl-C hit with %lu bytes in output buffer"
					,sbbs->cfg.node_num,RingBufFull(&sbbs->outbuf));
			sbbs->sys_status|=SS_ABORT;
			RingBufReInit(&sbbs->inbuf);	/* Purge input buffer */
    		RingBufReInit(&sbbs->outbuf);	/* Purge output buffer */
			sem_post(&sbbs->inbuf.sem);
			continue;	// Ignore the entire buffer
		}

		avail=RingBufFree(&sbbs->inbuf);

        if(avail<wr)
			lprintf(LOG_ERR,"!INPUT BUFFER FULL (%d free)", avail);
        else
			RingBufWrite(&sbbs->inbuf, wrbuf, wr);
//		if(wr>100)
//			mswait(500);	// Throttle sender
	}
	sbbs->online=FALSE;
	sbbs->sys_status|=SS_ABORT;	/* as though Ctrl-C were hit */

    sbbs->input_thread_running = false;
	if(node_socket[sbbs->cfg.node_num-1]==INVALID_SOCKET)	// Shutdown locally
		sbbs->terminated = true;	// Signal JS to stop execution

	while(pthread_mutex_destroy(&sbbs->input_thread_mutex)==EBUSY)
		mswait(1);

	thread_down();
	lprintf(LOG_DEBUG,"Node %d input thread terminated (received %lu bytes in %lu blocks)"
		,sbbs->cfg.node_num, total_recv, total_pkts);
}

#ifdef USE_CRYPTLIB
/*
 * This thread copies anything received from the client to the passthru_socket
 * It can only do that when the input thread is locked.
 * Luckily, the input thread is currently locked exactly when we want it to be.
 * Since the passthru socket is 8-bit clean and does NOT use a protocol,
 * we must handle telnet stuff HERE.
 * However, for JS stuff, direct operations on client_socket should generally
 * be done to the passthru_socket instead... THIS is the biggest problem here.
 */
void passthru_output_thread(void* arg)
{
	fd_set	socket_set;
	sbbs_t	*sbbs = (sbbs_t*) arg;
	struct	timeval	tv;
	int		i;
	BYTE	inbuf[4000];
   	BYTE	telbuf[sizeof(inbuf)];
	BYTE	*wrbuf;
	int		rd;
	int		wr;

	thread_up(FALSE /* setuid */);

    sbbs->passthru_output_thread_running = true;

	while(sbbs->client_socket!=INVALID_SOCKET && sbbs->passthru_socket!=INVALID_SOCKET && !terminate_server) {
		if(!sbbs->input_thread_mutex_locked) {
			SLEEP(1);
			continue;
		}
		
		FD_ZERO(&socket_set);
		FD_SET(sbbs->client_socket,&socket_set);

		tv.tv_sec=1;
		tv.tv_usec=0;

		if((i=select(sbbs->client_socket+1,&socket_set,NULL,NULL,&tv))<1) {
			if(i==0) {
				YIELD();	/* This kludge is necessary on some Linux distros */
				continue;	/* to allow other threads to lock the input_thread_mutex */
			}

			if(sbbs->client_socket==INVALID_SOCKET)
				break;
	       	if(ERROR_VALUE == ENOTSOCK)
    	        lprintf(LOG_NOTICE,"Node %d socket closed by peer on input->select", sbbs->cfg.node_num);
			else if(ERROR_VALUE==ESHUTDOWN)
				lprintf(LOG_NOTICE,"Node %d socket shutdown on input->select", sbbs->cfg.node_num);
			else if(ERROR_VALUE==EINTR)
				lprintf(LOG_DEBUG,"Node %d passthru output thread interrupted",sbbs->cfg.node_num);
            else if(ERROR_VALUE==ECONNRESET) 
				lprintf(LOG_NOTICE,"Node %d connection reset by peer on input->select", sbbs->cfg.node_num);
	        else if(ERROR_VALUE==ECONNABORTED) 
				lprintf(LOG_NOTICE,"Node %d connection aborted by peer on input->select", sbbs->cfg.node_num);
			else
				lprintf(LOG_WARNING,"Node %d !ERROR %d ->select socket %d"
               		,sbbs->cfg.node_num, ERROR_VALUE, sbbs->client_socket);
			break;
		}

		if(sbbs->client_socket==INVALID_SOCKET)
			break;

    	rd=sizeof(inbuf);

#ifdef USE_CRYPTLIB
		if(sbbs->ssh_mode) {
			if(!cryptStatusOK(cryptPopData(sbbs->ssh_session, (char*)inbuf, rd, &i)))
				rd=0;
			else {
				if(!i)
					continue;
				rd=i;
			}
		}
		else
#endif
    	rd = recv(sbbs->client_socket, (char*)inbuf, rd, 0);

		if(rd == SOCKET_ERROR)
		{
	       	if(ERROR_VALUE == ENOTSOCK)
    	        lprintf(LOG_NOTICE,"Node %d socket closed by peer on receive", sbbs->cfg.node_num);
            else if(ERROR_VALUE==ECONNRESET) 
				lprintf(LOG_NOTICE,"Node %d connection reset by peer on receive", sbbs->cfg.node_num);
			else if(ERROR_VALUE==ESHUTDOWN)
				lprintf(LOG_NOTICE,"Node %d socket shutdown on receive", sbbs->cfg.node_num);
            else if(ERROR_VALUE==ECONNABORTED) 
				lprintf(LOG_NOTICE,"Node %d connection aborted by peer on receive", sbbs->cfg.node_num);
			else
				lprintf(LOG_WARNING,"Node %d !ERROR %d receiving from socket %d"
                	,sbbs->cfg.node_num, ERROR_VALUE, sbbs->client_socket);
			break;
		}

		if(rd == 0)
		{
			lprintf(LOG_NOTICE,"Node %d passthru input socket disconnected", sbbs->cfg.node_num);
			break;
		}

        // telbuf and wr are modified to reflect telnet escaped data
		wr=rd;
		if(sbbs->telnet_mode&TELNET_MODE_OFF)
			wrbuf=inbuf;
		else
			wrbuf=telnet_interpret(sbbs, inbuf, rd, telbuf, wr);
		if(wr > (int)sizeof(telbuf)) 
			lprintf(LOG_ERR,"!TELBUF OVERFLOW (%d>%d)",wr,sizeof(telbuf));

		/*
		 * TODO: This should check for writability etc.
		 */
		sendsocket(sbbs->passthru_socket, (char*)wrbuf, wr);
	}

	sbbs->passthru_output_thread_running = false;
}

/*
 * This thread simply copies anything it manages to read from the
 * passthru_socket into the output ringbuffer.
 */
void passthru_input_thread(void* arg)
{
	fd_set	r_set;
	sbbs_t	*sbbs = (sbbs_t*) arg;
	struct	timeval	tv;
	BYTE	ch;
	int		i;

	thread_up(FALSE /* setuid */);

	sbbs->passthru_input_thread_running = true;

	while(sbbs->passthru_socket!=INVALID_SOCKET && !terminate_server) {
		tv.tv_sec=1;
		tv.tv_usec=0;

		FD_ZERO(&r_set);
		FD_SET(sbbs->passthru_socket,&r_set);
		if((i=select(sbbs->passthru_socket+1,&r_set,NULL,NULL,&tv))<1) {
			if(i==0) {
				YIELD();	/* This kludge is necessary on some Linux distros */
				continue;	/* to allow other threads to lock the input_thread_mutex */
			}

			if(sbbs->passthru_socket==INVALID_SOCKET)
				break;
	       	if(ERROR_VALUE == ENOTSOCK)
    	        lprintf(LOG_NOTICE,"Node %d socket closed by peer on passthru->select", sbbs->cfg.node_num);
			else if(ERROR_VALUE==ESHUTDOWN)
				lprintf(LOG_NOTICE,"Node %d socket shutdown on passthru->select", sbbs->cfg.node_num);
			else if(ERROR_VALUE==EINTR)
				lprintf(LOG_DEBUG,"Node %d passthru thread interrupted",sbbs->cfg.node_num);
            else if(ERROR_VALUE==ECONNRESET) 
				lprintf(LOG_NOTICE,"Node %d connection reset by peer on passthru->select", sbbs->cfg.node_num);
	        else if(ERROR_VALUE==ECONNABORTED) 
				lprintf(LOG_NOTICE,"Node %d connection aborted by peer on passthru->select", sbbs->cfg.node_num);
			else
				lprintf(LOG_WARNING,"Node %d !ERROR %d passthru->select socket %d"
               		,sbbs->cfg.node_num, ERROR_VALUE, sbbs->passthru_socket);
			break;
		}
		if(!RingBufFree(&sbbs->outbuf))
			continue;

    	i = recv(sbbs->passthru_socket, (char*)(&ch), 1, 0);

		if(i == SOCKET_ERROR)
		{
	        	if(ERROR_VALUE == ENOTSOCK)
    	            lprintf(LOG_NOTICE,"Node %d passthru socket closed by peer on receive", sbbs->cfg.node_num);
        	    else if(ERROR_VALUE==ECONNRESET) 
					lprintf(LOG_NOTICE,"Node %d passthru connection reset by peer on receive", sbbs->cfg.node_num);
				else if(ERROR_VALUE==ESHUTDOWN)
					lprintf(LOG_NOTICE,"Node %d passthru socket shutdown on receive", sbbs->cfg.node_num);
        	    else if(ERROR_VALUE==ECONNABORTED) 
					lprintf(LOG_NOTICE,"Node %d passthru connection aborted by peer on receive", sbbs->cfg.node_num);
				else
					lprintf(LOG_WARNING,"Node %d !ERROR %d receiving from passthru socket %d"
        	        	,sbbs->cfg.node_num, ERROR_VALUE, sbbs->passthru_socket);
				break;
		}

		if(i == 0)
		{
			lprintf(LOG_NOTICE,"Node %d passthru disconnected", sbbs->cfg.node_num);
			break;
		}

    	if(!RingBufWrite(&sbbs->outbuf, &ch, 1)) {
			lprintf(LOG_ERR,"Cannot pass from passthru socket to outbuf");
			break;
		}
	}
	if(sbbs->passthru_socket!=INVALID_SOCKET) {
		close_socket(sbbs->passthru_socket);
		sbbs->passthru_socket=INVALID_SOCKET;
	}
	thread_down();

	sbbs->passthru_input_thread_running = false;
}
#endif

void output_thread(void* arg)
{
	char		node[128];
	char		stats[128];
    BYTE		buf[IO_THREAD_BUF_SIZE];
	int			i;
    ulong		avail;
	ulong		total_sent=0;
	ulong		total_pkts=0;
	ulong		short_sends=0;
    ulong		bufbot=0;
    ulong		buftop=0;
	sbbs_t*		sbbs = (sbbs_t*) arg;
	fd_set		socket_set;
	struct timeval tv;
	ulong		mss=IO_THREAD_BUF_SIZE;

	thread_up(TRUE /* setuid */);

    if(sbbs->cfg.node_num)
    	SAFEPRINTF(node,"Node %d",sbbs->cfg.node_num);
    else
    	SAFECOPY(node,sbbs->client_name);
#ifdef _DEBUG
	lprintf(LOG_DEBUG,"%s output thread started",node);
#endif

    sbbs->output_thread_running = true;
	sbbs->console|=CON_R_ECHO;

#ifdef TCP_MAXSEG
	/*
	 * Auto-tune the highwater mark to be the negotiated MSS for the
     * socket (when possible)
	 */
	if(!sbbs->outbuf.highwater_mark) {
		socklen_t	sl;
		sl=sizeof(i);
		if(!getsockopt(sbbs->client_socket, IPPROTO_TCP, TCP_MAXSEG, &i, &sl)) {
			/* Check for sanity... */
			if(i>100) {
				sbbs->outbuf.highwater_mark=i;
				lprintf(LOG_DEBUG,"Autotuning outbuf highwater mark to %d based on MSS",i);
				mss=sbbs->outbuf.highwater_mark;
				if(mss>IO_THREAD_BUF_SIZE) {
					mss=IO_THREAD_BUF_SIZE;
					lprintf(LOG_DEBUG,"MSS (%d) is higher than IO_THREAD_BUF_SIZE (%d)",i,IO_THREAD_BUF_SIZE);
				}
			}
		}
	}
#endif

	while(sbbs->client_socket!=INVALID_SOCKET && !terminate_server) {
		/*
		 * I'd like to check the linear buffer against the highwater
		 * at this point, but it would get too clumsy imho - Deuce
		 *
		 * Actually, another option would just be to have the size
		 * of the linear buffer equal to the MSS... any larger and
		 * you could have small sends off the end.  this would
		 * probobly be even clumbsier
		 */
		if(bufbot == buftop) {
			/* Wait for something to output in the RingBuffer */
			if((avail=RingBufFull(&sbbs->outbuf))==0) {	/* empty */
				if(sem_trywait_block(&sbbs->outbuf.sem,1000))
					continue;
				/* Check for spurious sem post... */
				if((avail=RingBufFull(&sbbs->outbuf))==0)
					continue;
			}
			else
				sem_trywait(&sbbs->outbuf.sem);

			/* Wait for full buffer or drain timeout */
			if(sbbs->outbuf.highwater_mark) {
				if(avail<sbbs->outbuf.highwater_mark) {
					sem_trywait_block(&sbbs->outbuf.highwater_sem,startup->outbuf_drain_timeout);
					/* We (potentially) blocked, so get fill level again */
		    		avail=RingBufFull(&sbbs->outbuf);
				} else
					sem_trywait(&sbbs->outbuf.highwater_sem);	
			}

			/*
			 * At this point, there's something to send and,
			 * if the highwater mark is set, the timeout has
			 * passed or we've hit highwater.  Read ring buffer
			 * into linear buffer.
			 */
           	if(avail>sizeof(buf)) {
               	lprintf(LOG_WARNING,"!%s: Insufficient linear output buffer (%lu > %lu)"
					,node, avail, sizeof(buf));
               	avail=sizeof(buf);
           	}
			/* If we know the MSS, use it as the max send() size. */
			if(avail>mss)
				avail=mss;
           	buftop=RingBufRead(&sbbs->outbuf, buf, avail);
           	bufbot=0;
		}

		/* Check socket for writability (using select) */
		tv.tv_sec=0;
		tv.tv_usec=1000;

		FD_ZERO(&socket_set);
		FD_SET(sbbs->client_socket,&socket_set);

		i=select(sbbs->client_socket+1,NULL,&socket_set,NULL,&tv);
		if(i==SOCKET_ERROR) {
			if(sbbs->client_socket!=INVALID_SOCKET)
				lprintf(LOG_ERR,"!%s: ERROR %d selecting socket %u for send"
					,node,ERROR_VALUE,sbbs->client_socket);
			if(sbbs->cfg.node_num)	/* Only break if node output (not server) */
				break;
			RingBufReInit(&sbbs->outbuf);	/* Flush output buffer */
			continue;
		}
		if(i<1) {
			continue;
		}

#ifdef USE_CRYPTLIB
		if(sbbs->ssh_mode) {
			int err;
			if(!cryptStatusOK((err=cryptPushData(sbbs->ssh_session, (char*)buf+bufbot, buftop-bufbot, &i)))) {
				/* Handle the SSH error here... */
				lprintf(LOG_ERR,"!%s: ERROR %d sending on Cryptlib session", node, err);
				i=-1;
				sbbs->online=FALSE;
				i=buftop-bufbot;	// Pretend we sent it all
			}
			else
				cryptFlushData(sbbs->ssh_session);
		}
		else
#endif
		i=sendsocket(sbbs->client_socket, (char*)buf+bufbot, buftop-bufbot);
		if(i==SOCKET_ERROR) {
        	if(ERROR_VALUE == ENOTSOCK)
                lprintf(LOG_NOTICE,"%s client socket closed on send", node);
            else if(ERROR_VALUE==ECONNRESET) 
				lprintf(LOG_NOTICE,"%s connection reset by peer on send", node);
            else if(ERROR_VALUE==ECONNABORTED) 
				lprintf(LOG_NOTICE,"%s connection aborted by peer on send", node);
			else
				lprintf(LOG_WARNING,"!%s: ERROR %d sending on socket %d"
                	,node, ERROR_VALUE, sbbs->client_socket);
			sbbs->online=FALSE;
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
#ifdef __unix__
			if(uspy_socket[sbbs->cfg.node_num-1]!=INVALID_SOCKET)
				sendsocket(uspy_socket[sbbs->cfg.node_num-1],(char*)buf+bufbot,i);
#endif
		}

		if(i!=(int)(buftop-bufbot)) {
			lprintf(LOG_WARNING,"!%s: Short socket send (%u instead of %u)"
				,node, i ,buftop-bufbot);
			short_sends++;
		}
		bufbot+=i;
		total_sent+=i;
		total_pkts++;
    }

	sbbs->spymsg("Disconnected");

    sbbs->output_thread_running = false;

	if(total_sent)
		sprintf(stats,"(sent %lu bytes in %lu blocks, %lu average, %lu short)"
			,total_sent, total_pkts, total_sent/total_pkts, short_sends);
	else
		stats[0]=0;

	thread_down();
	lprintf(LOG_DEBUG,"%s output thread terminated %s", node, stats);
}

void event_thread(void* arg)
{
	ulong		stack_frame;
	char		str[MAX_PATH+1];
	char		bat_list[MAX_PATH+1];
	char		semfile[MAX_PATH+1];
	int			i,j,k;
	int			file;
	int			offset;
	bool		check_semaphores;
	bool		packed_rep;
	ulong	l;
	/* TODO: This is a silly hack... */
	uint32_t	l32;
	time_t		now;
	time_t		start;
	time_t		lastsemchk=0;
	time_t		lastnodechk=0;
	time32_t	lastprepack=0;
	time_t		tmptime;
	node_t		node;
	glob_t		g;
	sbbs_t*		sbbs = (sbbs_t*) arg;
	struct tm	now_tm;
	struct tm	tm;

	eprintf(LOG_DEBUG,"BBS Events thread started");

	sbbs->event_thread_running = true;

	sbbs_srand();	/* Seed random number generator */

	thread_up(TRUE /* setuid */);

#ifdef JAVASCRIPT
	if(!(startup->options&BBS_OPT_NO_JAVASCRIPT)) {
		if(!sbbs->js_init(&stack_frame)) /* This must be done in the context of the event thread */
			lprintf(LOG_ERR,"!JavaScript Initialization FAILURE");
	}
#endif

	// Read TIME.DAB
	SAFEPRINTF(str,"%stime.dab",sbbs->cfg.ctrl_dir);
	if((file=sbbs->nopen(str,O_RDWR|O_CREAT))==-1)
		sbbs->errormsg(WHERE,ERR_OPEN,str,0);
	else {
		for(i=0;i<sbbs->cfg.total_events;i++) {
			sbbs->cfg.event[i]->last=0;
			if(filelength(file)<(long)(sizeof(time32_t)*(i+1))) {
				eprintf(LOG_WARNING,"Initializing last run time for event: %s"
					,sbbs->cfg.event[i]->code);
				write(file,&sbbs->cfg.event[i]->last,sizeof(sbbs->cfg.event[i]->last));
			} else {
				if(read(file,&sbbs->cfg.event[i]->last,sizeof(sbbs->cfg.event[i]->last))!=sizeof(sbbs->cfg.event[i]->last))
					sbbs->errormsg(WHERE,ERR_READ,str,sizeof(time32_t));
			}
			/* Event always runs after initialization? */
			if(sbbs->cfg.event[i]->misc&EVENT_INIT)
				sbbs->cfg.event[i]->last=-1;
		}
		lastprepack=0;
		read(file,&lastprepack,sizeof(lastprepack));	/* expected to fail first time */
		close(file);
	}

	// Read QNET.DAB
	SAFEPRINTF(str,"%sqnet.dab",sbbs->cfg.ctrl_dir);
	if((file=sbbs->nopen(str,O_RDWR|O_CREAT))==-1)
		sbbs->errormsg(WHERE,ERR_OPEN,str,0);
	else {
		for(i=0;i<sbbs->cfg.total_qhubs;i++) {
			sbbs->cfg.qhub[i]->last=0;
			if(filelength(file)<(long)(sizeof(time32_t)*(i+1))) {
				eprintf(LOG_WARNING,"Initializing last call-out time for QWKnet hub: %s"
					,sbbs->cfg.qhub[i]->id);
				write(file,&sbbs->cfg.qhub[i]->last,sizeof(sbbs->cfg.qhub[i]->last));
			} else {
				if(read(file,&sbbs->cfg.qhub[i]->last,sizeof(sbbs->cfg.qhub[i]->last))!=sizeof(sbbs->cfg.qhub[i]->last))
					sbbs->errormsg(WHERE,ERR_READ,str,sizeof(sbbs->cfg.qhub[i]->last));
			}
		}
		close(file);
	}

	// Read PNET.DAB
	SAFEPRINTF(str,"%spnet.dab",sbbs->cfg.ctrl_dir);
	if((file=sbbs->nopen(str,O_RDWR|O_CREAT))==-1)
		sbbs->errormsg(WHERE,ERR_OPEN,str,0);
	else {
		for(i=0;i<sbbs->cfg.total_phubs;i++) {
			sbbs->cfg.phub[i]->last=0;
			if(filelength(file)<(long)(sizeof(time32_t)*(i+1)))
				write(file,&sbbs->cfg.phub[i]->last,sizeof(sbbs->cfg.phub[i]->last));
			else
				read(file,&sbbs->cfg.phub[i]->last,sizeof(sbbs->cfg.phub[i]->last)); 
		}
		close(file);
	}

	while(!sbbs->terminated && !terminate_server) {

		if(startup->options&BBS_OPT_NO_EVENTS) {
			SLEEP(1000);
			continue;
		}

		now=time(NULL);
		localtime_r(&now,&now_tm);

		if(now-lastsemchk>=sbbs->cfg.node_sem_check) {
			check_semaphores=true;
			lastsemchk=now;
		} else
			check_semaphores=false;

		sbbs->online=FALSE;	/* reset this from ON_LOCAL */

		/* QWK events */
		if(check_semaphores && !(startup->options&BBS_OPT_NO_QWK_EVENTS)) {
			/* Import any REP files that have magically appeared (via FTP perhaps) */
			SAFEPRINTF(str,"%sfile/",sbbs->cfg.data_dir);
			offset=strlen(str);
			strcat(str,"*.rep");
			glob(str,0,NULL,&g);
			for(i=0;i<(int)g.gl_pathc;i++) {
				sbbs->useron.number=atoi(g.gl_pathv[i]+offset);
				getuserdat(&sbbs->cfg,&sbbs->useron);
				if(sbbs->useron.number && flength(g.gl_pathv[i])>0) {
					SAFEPRINTF(semfile,"%s.lock",g.gl_pathv[i]);
					if(!fmutex(semfile,startup->host_name,24*60*60))
						continue;
					sbbs->online=ON_LOCAL;
					eprintf(LOG_INFO,"Un-packing QWK Reply packet from %s",sbbs->useron.alias);
					sbbs->getusrsubs();
					sbbs->unpack_rep(g.gl_pathv[i]);
					sbbs->batch_create_list();	/* FREQs? */
					sbbs->batdn_total=0;
					
					/* putuserdat? */
					remove(g.gl_pathv[i]);
					remove(semfile);
				}
			}
			globfree(&g);

			/* Create any QWK files that have magically appeared (via FTP perhaps) */
			SAFEPRINTF(str,"%spack*.now",sbbs->cfg.data_dir);
			offset=strlen(sbbs->cfg.data_dir)+4;
			glob(str,0,NULL,&g);
			for(i=0;i<(int)g.gl_pathc;i++) {
				eprintf(LOG_DEBUG,"QWK pack semaphore signaled: %s", g.gl_pathv[i]);
				sbbs->useron.number=atoi(g.gl_pathv[i]+offset);
				SAFEPRINTF2(semfile,"%spack%04u.lock",sbbs->cfg.data_dir,sbbs->useron.number);
				if(!fmutex(semfile,startup->host_name,24*60*60)) {
					eprintf(LOG_WARNING,"%s exists (already being packed?)", semfile);
					continue;
				}
				getuserdat(&sbbs->cfg,&sbbs->useron);
				if(sbbs->useron.number && !(sbbs->useron.misc&(DELETED|INACTIVE))) {
					eprintf(LOG_INFO,"Packing QWK Message Packet for %s",sbbs->useron.alias);
					sbbs->online=ON_LOCAL;
					delfiles(sbbs->cfg.temp_dir,ALLFILES);
					sbbs->getmsgptrs();
					sbbs->getusrsubs();
					sbbs->batdn_total=0;

					sbbs->last_ns_time=sbbs->ns_time=sbbs->useron.ns_time;
					SAFEPRINTF2(bat_list,"%sfile/%04u.dwn",sbbs->cfg.data_dir,sbbs->useron.number);
					sbbs->batch_add_list(bat_list);

					SAFEPRINTF3(str,"%sfile%c%04u.qwk"
						,sbbs->cfg.data_dir,PATH_DELIM,sbbs->useron.number);
					if(sbbs->pack_qwk(str,&l,true /* pre-pack/off-line */)) {
						eprintf(LOG_INFO,"Packing completed");
						sbbs->qwk_success(l,0,1);
						sbbs->putmsgptrs(); 
						remove(bat_list);
					} else
						eprintf(LOG_INFO,"No packet created (no new messages)");
					delfiles(sbbs->cfg.temp_dir,ALLFILES);
					sbbs->online=FALSE;
				}
				remove(g.gl_pathv[i]);
				remove(semfile);
			}
			globfree(&g);

			/* Create (pre-pack) QWK files for users configured as such */
			SAFEPRINTF(semfile,"%sprepack.now",sbbs->cfg.data_dir);
			if(sbbs->cfg.preqwk_ar[0] 
				&& (fexistcase(semfile) || (now-lastprepack)/60>(60*24))) {
				j=lastuser(&sbbs->cfg);
				eprintf(LOG_INFO,"Pre-packing QWK Message packets...");
				for(i=1;i<=j;i++) {

					SAFEPRINTF2(str,"%5u of %-5u",i,j);
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
						eprintf(LOG_INFO,"Pre-packing QWK for %s",sbbs->useron.alias);
						sbbs->online=ON_LOCAL;
						delfiles(sbbs->cfg.temp_dir,ALLFILES);
						sbbs->getmsgptrs();
						sbbs->getusrsubs();
						sbbs->batdn_total=0;
						SAFEPRINTF3(str,"%sfile%c%04u.qwk"
							,sbbs->cfg.data_dir,PATH_DELIM,sbbs->useron.number);
						if(sbbs->pack_qwk(str,&l,true /* pre-pack */)) {
							sbbs->qwk_success(l,0,1);
							sbbs->putmsgptrs(); 
						}
						delfiles(sbbs->cfg.temp_dir,ALLFILES);
						sbbs->online=FALSE;
					} 
				}
				lastprepack=now;
				SAFEPRINTF(str,"%stime.dab",sbbs->cfg.ctrl_dir);
				if((file=sbbs->nopen(str,O_WRONLY))==-1) {
					sbbs->errormsg(WHERE,ERR_OPEN,str,O_WRONLY);
					break; 
				}
				lseek(file,(long)sbbs->cfg.total_events*4L,SEEK_SET);
				write(file,&lastprepack,sizeof(lastprepack));
				close(file);

				remove(semfile);
				//status(STATUS_WFC);
			}
		}

		if(check_semaphores) {

			/* Run daily maintenance? */
			sbbs->cfg.node_num=0;
			sbbs->logonstats();
			if(sbbs->sys_status&SS_DAILY)
				sbbs->daily_maint();

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

						eprintf(LOG_INFO,"Running node %d daily event",i);
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
				SAFEPRINTF2(str,"%sqnet/%s.now",sbbs->cfg.data_dir,sbbs->cfg.qhub[i]->id);
				if(fexistcase(str)) {
					strcpy(str,sbbs->cfg.qhub[i]->id);
					eprintf(LOG_INFO,"Semaphore signaled for QWK Network Hub: %s",strupr(str));
					sbbs->cfg.qhub[i]->last=-1; 
				}
			}

			/* Timed Event sempahores */
			for(i=0;i<sbbs->cfg.total_events;i++) {
				if((sbbs->cfg.event[i]->node<first_node
					|| sbbs->cfg.event[i]->node>last_node)
					&& !(sbbs->cfg.event[i]->misc&EVENT_EXCL))
					continue;	// ignore non-exclusive events for other instances
				if(sbbs->cfg.event[i]->misc&EVENT_DISABLED)
					continue;
				if(sbbs->cfg.event[i]->last==-1) // already signaled
					continue;
				SAFEPRINTF2(str,"%s%s.now",sbbs->cfg.data_dir,sbbs->cfg.event[i]->code);
				if(fexistcase(str)) {
					strcpy(str,sbbs->cfg.event[i]->code);
					eprintf(LOG_INFO,"Semaphore signaled for Timed Event: %s",strupr(str));
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
				SAFEPRINTF2(str,"%s%s.q??",sbbs->cfg.data_dir,sbbs->cfg.qhub[i]->id);
				glob(str,GLOB_NOSORT,NULL,&g);
				for(j=0;j<(int)g.gl_pathc;j++) {
					SAFECOPY(str,g.gl_pathv[j]);
					if(flength(str)>0) {	/* silently ignore 0-byte QWK packets */
						eprintf(LOG_DEBUG,"Inbound QWK Packet detected: %s", str);
						delfiles(sbbs->cfg.temp_dir,ALLFILES);
						sbbs->online=ON_LOCAL;
						sbbs->console|=CON_L_ECHO;
						if(sbbs->unpack_qwk(str,i)==false) {
							char newname[MAX_PATH+1];
							SAFEPRINTF2(newname,"%s.%lx.bad",str,(long)now);
							remove(newname);
							if(rename(str,newname)==0) {
								char logmsg[MAX_PATH*3];
								SAFEPRINTF2(logmsg,"%s renamed to %s",str,newname);
								sbbs->logline("Q!",logmsg);
							}
						}
						sbbs->console&=~CON_L_ECHO;
						sbbs->online=FALSE;
						remove(str);
					} 
				}
				globfree(&g);
			}

			/* Qnet call out based on time */
			tmptime=sbbs->cfg.qhub[i]->last;
			if(localtime_r(&tmptime,&tm)==NULL)
				memset(&tm,0,sizeof(tm));
			if((sbbs->cfg.qhub[i]->last==-1L					/* or frequency */
				|| ((sbbs->cfg.qhub[i]->freq
					&& (now-sbbs->cfg.qhub[i]->last)/60>sbbs->cfg.qhub[i]->freq)
					|| (sbbs->cfg.qhub[i]->time
						&& (now_tm.tm_hour*60)+now_tm.tm_min>=sbbs->cfg.qhub[i]->time
						&& (now_tm.tm_mday!=tm.tm_mday || now_tm.tm_mon!=tm.tm_mon)))
						&& sbbs->cfg.qhub[i]->days&(1<<now_tm.tm_wday))) {
				SAFEPRINTF2(str,"%sqnet/%s.now"
					,sbbs->cfg.data_dir,sbbs->cfg.qhub[i]->id);
				if(fexistcase(str))
					remove(str);					/* Remove semaphore file */
				SAFEPRINTF2(str,"%sqnet/%s.ptr"
					,sbbs->cfg.data_dir,sbbs->cfg.qhub[i]->id);
				file=sbbs->nopen(str,O_RDONLY);
				for(j=0;j<sbbs->cfg.qhub[i]->subs;j++) {
					sbbs->subscan[sbbs->cfg.qhub[i]->sub[j]].ptr=0;
					if(file!=-1) {
						lseek(file,sbbs->cfg.sub[sbbs->cfg.qhub[i]->sub[j]]->ptridx*sizeof(int32_t),SEEK_SET);
						read(file,&sbbs->subscan[sbbs->cfg.qhub[i]->sub[j]].ptr,sizeof(sbbs->subscan[sbbs->cfg.qhub[i]->sub[j]].ptr)); 
					}
				}
				if(file!=-1)
					close(file);
				sbbs->console|=CON_L_ECHO;
				packed_rep=sbbs->pack_rep(i);
				sbbs->console&=~CON_L_ECHO;
				if(packed_rep) {
					if((file=sbbs->nopen(str,O_WRONLY|O_CREAT))==-1)
						sbbs->errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT);
					else {
						for(j=l=0;j<sbbs->cfg.qhub[i]->subs;j++) {
							while(filelength(file)<
								sbbs->cfg.sub[sbbs->cfg.qhub[i]->sub[j]]->ptridx*4L) {
								l32=l;
								write(file,&l32,4);		/* initialize ptrs to null */
							}
							lseek(file
								,sbbs->cfg.sub[sbbs->cfg.qhub[i]->sub[j]]->ptridx*sizeof(int32_t)
								,SEEK_SET);
							write(file,&sbbs->subscan[sbbs->cfg.qhub[i]->sub[j]].ptr,sizeof(sbbs->subscan[sbbs->cfg.qhub[i]->sub[j]].ptr)); 
						}
						close(file); 
					} 
				}
				delfiles(sbbs->cfg.temp_dir,ALLFILES);

				sbbs->cfg.qhub[i]->last=time(NULL);
				SAFEPRINTF(str,"%sqnet.dab",sbbs->cfg.ctrl_dir);
				if((file=sbbs->nopen(str,O_WRONLY))==-1) {
					sbbs->errormsg(WHERE,ERR_OPEN,str,O_WRONLY);
					break; 
				}
				lseek(file,sizeof(time32_t)*i,SEEK_SET);
				write(file,&sbbs->cfg.qhub[i]->last,sizeof(sbbs->cfg.qhub[i]->last));
				close(file);

				if(sbbs->cfg.qhub[i]->call[0]) {
					sbbs->cfg.node_num=sbbs->cfg.qhub[i]->node;
					if(sbbs->cfg.node_num<1) 
						sbbs->cfg.node_num=1;
					strcpy(sbbs->cfg.node_dir, sbbs->cfg.node_path[sbbs->cfg.node_num-1]);
					eprintf(LOG_INFO,"QWK Network call-out: %s",sbbs->cfg.qhub[i]->id); 
					sbbs->online=ON_LOCAL;
					sbbs->external(
						 sbbs->cmdstr(sbbs->cfg.qhub[i]->call
							,sbbs->cfg.qhub[i]->id,sbbs->cfg.qhub[i]->id,NULL)
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
			tmptime=sbbs->cfg.phub[i]->last;
			if(localtime_r(&tmptime,&tm)==NULL)
				memset(&tm,0,sizeof(tm));
			if(sbbs->cfg.phub[i]->last==-1
				|| (((sbbs->cfg.phub[i]->freq								/* or frequency */
					&& (now-sbbs->cfg.phub[i]->last)/60>sbbs->cfg.phub[i]->freq)
				|| (sbbs->cfg.phub[i]->time
					&& (now_tm.tm_hour*60)+now_tm.tm_min>=sbbs->cfg.phub[i]->time
				&& (now_tm.tm_mday!=tm.tm_mday || now_tm.tm_mon!=tm.tm_mon)))
				&& sbbs->cfg.phub[i]->days&(1<<now_tm.tm_wday))) {

				sbbs->cfg.phub[i]->last=time(NULL);
				SAFEPRINTF(str,"%spnet.dab",sbbs->cfg.ctrl_dir);
				if((file=sbbs->nopen(str,O_WRONLY))==-1) {
					sbbs->errormsg(WHERE,ERR_OPEN,str,O_WRONLY);
					break; 
				}
				lseek(file,sizeof(time32_t)*i,SEEK_SET);
				write(file,&sbbs->cfg.phub[i]->last,sizeof(sbbs->cfg.phub[i]->last));
				close(file);

				if(sbbs->cfg.phub[i]->call[0]) {
					sbbs->cfg.node_num=sbbs->cfg.phub[i]->node;
					if(sbbs->cfg.node_num<1) 
						sbbs->cfg.node_num=1;
					strcpy(sbbs->cfg.node_dir, sbbs->cfg.node_path[sbbs->cfg.node_num-1]);
					eprintf(LOG_INFO,"PostLink Network call-out: %s",sbbs->cfg.phub[i]->name); 
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

			if(sbbs->cfg.event[i]->misc&EVENT_DISABLED)
				continue;

			if((sbbs->cfg.event[i]->node<first_node
				|| sbbs->cfg.event[i]->node>last_node)
				&& !(sbbs->cfg.event[i]->misc&EVENT_EXCL))
				continue;	// ignore non-exclusive events for other instances

			tmptime=sbbs->cfg.event[i]->last;
			if(localtime_r(&tmptime,&tm)==NULL)
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
						eprintf(LOG_INFO,"Waiting for node %d to run timed event: %s"
							,sbbs->cfg.event[i]->node,sbbs->cfg.event[i]->code);
						eprintf(LOG_DEBUG,"%s event last run: %s (0x%08lx)"
							,sbbs->cfg.event[i]->code
							,timestr(&sbbs->cfg, sbbs->cfg.event[i]->last, str)
							,sbbs->cfg.event[i]->last);
						lastnodechk=0;	 /* really last event time check */
						start=time(NULL);
						while(!sbbs->terminated) {
							mswait(1000);
							now=time(NULL);
							if(now-start>10 && now-lastnodechk<10)
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
							SAFEPRINTF(str,"%stime.dab",sbbs->cfg.ctrl_dir);
							if((file=sbbs->nopen(str,O_RDONLY))==-1) {
								sbbs->errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
								sbbs->cfg.event[i]->last=now;
								continue; 
							}
							lseek(file,(long)i*4L,SEEK_SET);
							read(file,&sbbs->cfg.event[i]->last,sizeof(sbbs->cfg.event[i]->last));
							close(file);
							if(now-sbbs->cfg.event[i]->last<(60*60))	/* event is done */
								break; 
							if(now-start>(90*60)) {
								eprintf(LOG_WARNING,"!TIMEOUT waiting for event to complete");
								break;
							}
						}
						SAFEPRINTF2(str,"%s%s.now",sbbs->cfg.data_dir,sbbs->cfg.event[i]->code);
						if(fexistcase(str))
							remove(str);
						sbbs->cfg.event[i]->last=now;
					} else {	// Exclusive event to run on a node under our control
						eprintf(LOG_INFO,"Waiting for all nodes to become inactive before "
							"running timed event: %s",sbbs->cfg.event[i]->code);
						lastnodechk=0;
						start=time(NULL);
						while(!sbbs->terminated) {
							mswait(1000);
							now=time(NULL);
							if(now-start>10 && now-lastnodechk<10)
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
							eprintf(LOG_DEBUG,"Waiting for node %d (status=%d)",j,node.status);
							if(now-start>(90*60)) {
								eprintf(LOG_WARNING,"!TIMEOUT waiting for node %d to become inactive",j);
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
					eprintf(LOG_NOTICE,"Changing node status for nodes %d through %d to WFC"
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
				
					SAFEPRINTF2(str,"%s%s.now",sbbs->cfg.data_dir,sbbs->cfg.event[i]->code);
					if(fexistcase(str))
						remove(str);
					if(sbbs->cfg.event[i]->misc&EVENT_EXCL) {
						sbbs->getnodedat(sbbs->cfg.event[i]->node,&node,1);
						node.status=NODE_EVENT_RUNNING;
						sbbs->putnodedat(sbbs->cfg.event[i]->node,&node);
					}
					strcpy(str,sbbs->cfg.event[i]->code);
					eprintf(LOG_INFO,"Running timed event: %s",strupr(str));
					int ex_mode = EX_OFFLINE;
					if(!(sbbs->cfg.event[i]->misc&EVENT_EXCL)
						&& sbbs->cfg.event[i]->misc&EX_BG)
						ex_mode |= EX_BG;
					if(sbbs->cfg.event[i]->misc&XTRN_SH)
						ex_mode |= EX_SH;
					ex_mode|=(sbbs->cfg.event[i]->misc&EX_NATIVE);
					sbbs->online=ON_LOCAL;
					sbbs->external(
						 sbbs->cmdstr(sbbs->cfg.event[i]->cmd,nulstr,sbbs->cfg.event[i]->dir,NULL)
						,ex_mode
						,sbbs->cfg.event[i]->dir);
					sbbs->cfg.event[i]->last=time(NULL);
					SAFEPRINTF(str,"%stime.dab",sbbs->cfg.ctrl_dir);
					if((file=sbbs->nopen(str,O_WRONLY))==-1) {
						sbbs->errormsg(WHERE,ERR_OPEN,str,O_WRONLY);
						break; 
					}
					lseek(file,(long)i*4L,SEEK_SET);
					write(file,&sbbs->cfg.event[i]->last,sizeof(sbbs->cfg.event[i]->last));
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
		mswait(1000);
	}
	sbbs->cfg.node_num=0;
    sbbs->event_thread_running = false;

	thread_down();
	eprintf(LOG_DEBUG,"BBS Event thread terminated (%u threads remain)", thread_count);
}


//****************************************************************************
sbbs_t::sbbs_t(ushort node_num, DWORD addr, char* name, SOCKET sd,
			   scfg_t* global_cfg, char* global_text[], client_t* client_info)
{
	char	nodestr[32];
	char	path[MAX_PATH+1];
	uint	i;

    if(node_num)
    	SAFEPRINTF(nodestr,"Node %d",node_num);
    else
    	SAFECOPY(nodestr,name);

	lprintf(LOG_DEBUG,"%s constructor using socket %d (settings=%lx)"
		,nodestr, sd, global_cfg->node_misc);

	startup = ::startup;	// Convert from global to class member

	memcpy(&cfg, global_cfg, sizeof(cfg));

	cfg.node_num=node_num;
	if(node_num>0) {
		strcpy(cfg.node_dir, cfg.node_path[node_num-1]);
		prep_dir(cfg.node_dir, cfg.temp_dir, sizeof(cfg.temp_dir));
	} else {	/* event thread needs exclusive-use temp_dir */
		if(startup->temp_dir[0])
			SAFECOPY(cfg.temp_dir,startup->temp_dir);
		else
			SAFECOPY(cfg.temp_dir,"../temp");
    	prep_dir(cfg.ctrl_dir, cfg.temp_dir, sizeof(cfg.temp_dir));
		md(cfg.temp_dir);
		if(sd==INVALID_SOCKET) {	/* events thread */
			if(startup->first_node==1)
				SAFEPRINTF(path,"%sevent",cfg.temp_dir);
			else
				SAFEPRINTF2(path,"%sevent%u",cfg.temp_dir,startup->first_node);
			backslash(path);
			SAFECOPY(cfg.temp_dir,path);
		}
	}
	lprintf(LOG_DEBUG,"%s temporary file directory: %s", nodestr, cfg.temp_dir);

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

	telnet_location[0]=0;
	terminal[0]=0;
	rlogin_name[0]=0;
	rlogin_pass[0]=0;

	/* Init some important variables */

#ifdef USE_CRYPTLIB
	ssh_mode=false;
    passthru_input_thread_running = false;
    passthru_output_thread_running = false;
#endif

	rio_abortable=false;

	console = 0;
	online = 0;
	outchar_esc = 0;
	nodemsg_inside = 0;	/* allows single nest */
	hotkey_inside = 0;	/* allows single nest */
	event_time = 0;
	event_code = nulstr;
	nodesync_inside = false;
	errorlog_inside = false;
	errormsg_inside = false;
	gettimeleft_inside = false;
	timeleft = 60*10;	/* just incase this is being used for calling gettimeleft() */
	uselect_total = 0;
	lbuflen = 0;
	keybufbot=keybuftop=0;	/* initialize [unget]keybuf pointers */
	SAFECOPY(connection,"Telnet");
	node_connection=NODE_CONNECTION_TELNET;

	ZERO_VAR(telnet_local_option);
	ZERO_VAR(telnet_remote_option);

    telnet_cmdlen=0;
	telnet_mode=0;
	telnet_last_rxch=0;

	sys_status=lncntr=tos=criterrs=slcnt=0L;
	curatr=LIGHTGRAY;
	attr_sp=0;	/* attribute stack pointer */
	errorlevel=0;
	logcol=1;
	logfile_fp=NULL;
	nodefile=-1;
	node_ext=-1;
	nodefile_fp=NULL;
	node_ext_fp=NULL;
	current_msg=NULL;
	mnestr=NULL;

#ifdef JAVASCRIPT
	js_runtime=NULL;	/* runtime */
	js_cx=NULL;			/* context */
#endif

	for(i=0;i<TOTAL_TEXT;i++)
		text[i]=text_sav[i]=global_text[i];

	ZERO_VAR(main_csi);
	ZERO_VAR(thisnode);
	ZERO_VAR(useron);
	ZERO_VAR(inbuf);
	ZERO_VAR(outbuf);
	ZERO_VAR(smb);
	ZERO_VAR(nodesync_user);

	action=NODE_MAIN;
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
	outbuf.highwater_mark=startup->outbuf_highwater_mark;

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
#else
		client_socket_dup = client_socket;
#endif

		addr_len=sizeof(addr);
		if((result=getsockname(client_socket, (struct sockaddr *)&addr,&addr_len))!=0) {
			lprintf(LOG_ERR,"Node %d !ERROR %d (%d) getting address/port"
				,cfg.node_num, result, ERROR_VALUE);
			return(false);
		} 
		lprintf(LOG_INFO,"Node %d attached to local interface %s port %d"
			,cfg.node_num, inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

		local_addr=addr.sin_addr.s_addr;
	}

	if((comspec=os_cmdshell())==NULL) {
		errormsg(WHERE, ERR_CHK, OS_CMD_SHELL_ENV_VAR" environment variable", 0);
		return(false);
	}

	md(cfg.temp_dir);

	/* Shared NODE files */
	SAFEPRINTF2(str,"%s%s",cfg.ctrl_dir,"node.dab");
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
		SAFEPRINTF(str,"%snode.log",cfg.node_dir);
		if((logfile_fp=fopen(str,"a+b"))==NULL) {
			errormsg(WHERE, ERR_OPEN, str, 0);
			lprintf(LOG_ERR,"Perhaps this node is already running");
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

	main_csi.str=(char *)malloc(1024);
	if(main_csi.str==NULL) {
		errormsg(WHERE,ERR_ALLOC,"main_csi.str",1024);
		return(false); 
	}
	memset(main_csi.str,0,1024);
/***/

	if(cfg.total_grps) {

		usrgrp_total = cfg.total_grps;

		if((cursub=(uint *)malloc(sizeof(uint)*usrgrp_total))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "cursub", sizeof(uint)*usrgrp_total);
			return(false);
		}

		if((usrgrp=(uint *)malloc(sizeof(uint)*usrgrp_total))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "usrgrp", sizeof(uint)*usrgrp_total);
			return(false);
		}

		if((usrsubs=(uint *)malloc(sizeof(uint)*usrgrp_total))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "usrsubs", sizeof(uint)*usrgrp_total);
			return(false);
		}

		if((usrsub=(uint **)calloc(usrgrp_total,sizeof(uint *)))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "usrsub", sizeof(uint)*usrgrp_total);
			return(false);
		}
 
		if((subscan=(subscan_t *)malloc(sizeof(subscan_t)*cfg.total_subs))==NULL) {
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
			if((usrsub[i]=(uint *)malloc(sizeof(uint)*l))==NULL) {
				errormsg(WHERE, ERR_ALLOC, "usrsub[x]", sizeof(uint)*l);
				return(false);
			}

	if(cfg.total_libs) {

		usrlib_total = cfg.total_libs;

		if((curdir=(uint *)malloc(sizeof(uint)*usrlib_total))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "curdir", sizeof(uint)*usrlib_total);
			return(false);
		}

		if((usrlib=(uint *)malloc(sizeof(uint)*usrlib_total))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "usrlib", sizeof(uint)*usrlib_total);
			return(false);
		}

		if((usrdirs=(uint *)malloc(sizeof(uint)*usrlib_total))==NULL) {
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
			if((usrdir[i]=(uint *)malloc(sizeof(uint)*l))==NULL) {
				errormsg(WHERE, ERR_ALLOC, "usrdir[x]", sizeof(uint)*l);
				return(false);
			}
	}
 
	if(cfg.max_batup) {

		if((batup_desc=(char **)malloc(sizeof(char *)*cfg.max_batup))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "batup_desc", sizeof(char *)*cfg.max_batup);
			return(false);
		}
		if((batup_name=(char **)malloc(sizeof(char *)*cfg.max_batup))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "batup_name", sizeof(char *)*cfg.max_batup);
			return(false);
		}
		if((batup_misc=(long *)malloc(sizeof(long)*cfg.max_batup))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "batup_misc", sizeof(char *)*cfg.max_batup);
			return(false);
		}
		if((batup_dir=(uint *)malloc(sizeof(uint)*cfg.max_batup))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "batup_dir", sizeof(char *)*cfg.max_batup);
			return(false);
		}
		if((batup_alt=(ushort *)malloc(sizeof(ushort)*cfg.max_batup))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "batup_alt", sizeof(char *)*cfg.max_batup);
			return(false);
		}
		for(i=0;i<cfg.max_batup;i++) {
			if((batup_desc[i]=(char *)malloc(59))==NULL) {
				errormsg(WHERE, ERR_ALLOC, "batup_desc[x]", 59);
				return(false);
			}
			if((batup_name[i]=(char *)malloc(13))==NULL) {
				errormsg(WHERE, ERR_ALLOC, "batup_name[x]", 13);
				return(false);
			} 
		} 
	}

	if(cfg.max_batdn) {

		if((batdn_name=(char **)malloc(sizeof(char *)*cfg.max_batdn))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "batdn_name", sizeof(char *)*cfg.max_batdn);
			return(false);
		}
		if((batdn_dir=(uint *)malloc(sizeof(uint)*cfg.max_batdn))==NULL)  {
			errormsg(WHERE, ERR_ALLOC, "batdn_dir", sizeof(uint)*cfg.max_batdn);
			return(false);
		}
		if((batdn_offset=(long *)malloc(sizeof(long)*cfg.max_batdn))==NULL)  {
			errormsg(WHERE, ERR_ALLOC, "batdn_offset", sizeof(long)*cfg.max_batdn);
			return(false);
		}
		if((batdn_size=(ulong *)malloc(sizeof(ulong)*cfg.max_batdn))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "batdn_size", sizeof(ulong)*cfg.max_batdn);
			return(false);
		}
		if((batdn_cdt=(ulong *)malloc(sizeof(ulong)*cfg.max_batdn))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "batdn_cdt", sizeof(long)*cfg.max_batdn);
			return(false);
		}
		if((batdn_alt=(ushort *)malloc(sizeof(ushort)*cfg.max_batdn))==NULL) {
			errormsg(WHERE, ERR_ALLOC, "batdn_alt", sizeof(ushort)*cfg.max_batdn);
			return(false);
		}
		for(i=0;i<cfg.max_batdn;i++)
			if((batdn_name[i]=(char *)malloc(13))==NULL) {
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
    	SAFEPRINTF(node,"Node %d", cfg.node_num);
    else
    	SAFECOPY(node,client_name);
#ifdef _DEBUG
	lprintf(LOG_DEBUG,"%s destructor begin", node);
#endif

//	if(!cfg.node_num)
//		rmdir(cfg.temp_dir);

	if(client_socket_dup!=INVALID_SOCKET && client_socket_dup!=client_socket)
		closesocket(client_socket_dup);	/* close duplicate handle */

	if(cfg.node_num>0)
		node_inbuf[cfg.node_num-1]=NULL;
	if(cfg.node_num>0 && !input_thread_running)
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
		lprintf(LOG_DEBUG,"%s JavaScript: Destroying context",node);
		JS_DestroyContext(js_cx);
		js_cx=NULL;
	}

	if(js_runtime!=NULL) {
		lprintf(LOG_DEBUG,"%s JavaScript: Destroying runtime",node);
		JS_DestroyRuntime(js_runtime);
		js_runtime=NULL;
	}
#endif

	/* Reset text.dat */

	for(i=0;i<TOTAL_TEXT && text!=NULL;i++)
		if(text[i]!=text_sav[i]) {
			if(text[i]!=nulstr)
				free(text[i]); 
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
		lprintf(LOG_ERR,"!MEMORY ERRORS REPORTED IN DATA/DEBUG.LOG!");
#endif

#ifdef _DEBUG
	lprintf(LOG_DEBUG,"%s destructor end", node);
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
    while(((file=sopen(str,access,share,DEFFILEMODE))==-1)
        && (errno==EACCES || errno==EAGAIN) && count++<LOOP_NOPEN)
	    mswait(100);
    if(count>(LOOP_NOPEN/2) && count<=LOOP_NOPEN) {
        SAFEPRINTF2(logstr,"NOPEN COLLISION - File: \"%s\" Count: %d"
            ,str,count);
        logline("!!",logstr); 
	}
    if(file==-1 && (errno==EACCES || errno==EAGAIN)) {
        SAFEPRINTF2(logstr,"NOPEN ACCESS DENIED - File: \"%s\" errno: %d"
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
	SAFEPRINTF4(str,"\r\n\r\n*** Spy Message ***\r\nNode %d: %s [%s]\r\n*** %s ***\r\n\r\n"
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
#ifdef __unix__
	if(cfg.node_num && uspy_socket[cfg.node_num-1]!=INVALID_SOCKET) 
		sendsocket(uspy_socket[cfg.node_num-1],str,strlen(str));
#endif
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
	time_t	ftime;
	FILE *inp,*outp;

    if(!stricmp(src,dest))	 /* source and destination are the same! */
        return(0);
    if(!fexistcase(src)) {
        bprintf("\r\n\7MV ERROR: Source doesn't exist\r\n'%s'\r\n"
            ,src);
        return(-1); 
	}
    if(!copy && fexistcase(dest)) {
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
	ftime=filetime(ind);
    length=filelength(ind);
    if(length) {	/* Something to copy */
		if((buf=(char *)malloc(MV_BUFLEN))==NULL) {
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
				free(buf);
				fclose(inp);
				fclose(outp);
				errormsg(WHERE,ERR_READ,src,chunk);
				return(-1); 
			}
			if(fwrite(buf,1,chunk,outp)!=chunk) {
				free(buf);
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
		free(buf);
	}
    fclose(inp);
    fclose(outp);
	setfdate(dest,ftime);	/* Would be nice if we could use futime() instead */
    if(!copy && remove(src)) {
        errormsg(WHERE,ERR_REMOVE,src,0);
        return(-1); 
	}
    return(0);
}

void sbbs_t::hangup(void)
{
	if(client_socket_dup!=INVALID_SOCKET && client_socket_dup!=client_socket)
		closesocket(client_socket_dup);
	client_socket_dup=INVALID_SOCKET;

	if(client_socket!=INVALID_SOCKET) {
		mswait(1000);	/* Give socket output buffer time to flush */
		client_off(client_socket);
		close_socket(client_socket);
		client_socket=INVALID_SOCKET;
	}
	sem_post(&outbuf.sem);
	online=FALSE;
}

int sbbs_t::incom(unsigned long timeout)
{
	uchar	ch;

#if 0	/* looping version */
	while(!RingBufRead(&inbuf, &ch, 1))
		if(sem_trywait_block(&inbuf.sem,timeout)!=0 || sys_status&SS_ABORT)
			return(NOINP);
#else
	if(!RingBufRead(&inbuf, &ch, 1)) {
		if(sem_trywait_block(&inbuf.sem,timeout)!=0)
			return(NOINP);
		if(!RingBufRead(&inbuf, &ch, 1))
			return(NOINP);
	}
#endif
	return(ch);
}

int sbbs_t::outcom(uchar ch)
{
	if(!RingBufFree(&outbuf))
		return(TXBOF);
    if(!RingBufWrite(&outbuf, &ch, 1))
		return(TXBOF);
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
    rows=24;
	cols=80;
    lncntr=0;
    autoterm=0;
    lbuflen=0;
    slcnt=0;
    altul=0;
    timeleft_warn=0;
	keybufbot=keybuftop=0;
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
	char *buf;
	int  i,file;
	long length;
	struct tm tm;

	if(logfile_fp==NULL) {
		SAFEPRINTF(str,"%snode.log",cfg.node_dir);
		if((logfile_fp=fopen(str,"rb"))==NULL) {
			errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
			return; 
		}
	}
	length=ftell(logfile_fp);
	if(length) {
		if((buf=(char *)malloc(length))==NULL) {
			errormsg(WHERE,ERR_ALLOC,str,length);
			return; 
		}
		rewind(logfile_fp);
		if(fread(buf,1,length,logfile_fp)!=(size_t)length) {
			errormsg(WHERE,ERR_READ,"log file",length);
			free((char *)buf);
			return; 
		}
		now=time(NULL);
		localtime_r(&now,&tm);
		SAFEPRINTF4(str,"%slogs/%2.2d%2.2d%2.2d.log",cfg.logs_dir,tm.tm_mon+1,tm.tm_mday
			,TM_YEAR(tm.tm_year));
		if((file=nopen(str,O_WRONLY|O_APPEND|O_CREAT))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_APPEND|O_CREAT);
			free((char *)buf);
			return; 
		}
		if(lwrite(file,buf,length)!=length) {
			close(file);
			errormsg(WHERE,ERR_WRITE,str,length);
			free((char *)buf);
			return; 
		}
		close(file);
		if(crash) {
			for(i=0;i<2;i++) {
				SAFEPRINTF(str,"%scrash.log",i ? cfg.logs_dir : cfg.node_dir);
				if((file=nopen(str,O_WRONLY|O_APPEND|O_CREAT))==-1) {
					errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_APPEND|O_CREAT);
					free((char *)buf);
					return; 
				}
				if(lwrite(file,buf,length)!=length) {
					close(file);
					errormsg(WHERE,ERR_WRITE,str,length);
					free((char *)buf);
					return; 
				}
				close(file); 
			} 
		}
		free((char *)buf); 
	}

	fclose(logfile_fp);

	SAFEPRINTF(str,"%snode.log",cfg.node_dir);
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
		SAFEPRINTF(str,"%sdsts.dab",i ? cfg.ctrl_dir : cfg.node_dir);
		if((file=nopen(str,O_RDWR))==-1) {
			errormsg(WHERE,ERR_OPEN,str,O_RDWR);
			return; 
		}
		memset(&stats,0,sizeof(stats));
		lseek(file,4L,SEEK_SET);   /* Skip timestamp, logons and logons today */
		read(file,&stats,sizeof(stats));  

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

#if 0 // This is now handled in newuserdat()
		if(sys_status&SS_NEWUSER)
			stats.nusers++;
#endif

		lseek(file,4L,SEEK_SET);
		write(file,&stats,sizeof(stats));
		close(file); 
	}
}

void node_thread(void* arg)
{
	ulong			stack_frame;
	char			str[128];
	int				file;
	uint			curshell=0;
	node_t			node;
	sbbs_t*			sbbs = (sbbs_t*) arg;

	update_clients();
	thread_up(TRUE /* setuid */);

#ifdef _DEBUG
	lprintf(LOG_DEBUG,"Node %d thread started",sbbs->cfg.node_num);
#endif

	sbbs_srand();		/* Seed random number generator */

#ifdef JAVASCRIPT
	if(!(startup->options&BBS_OPT_NO_JAVASCRIPT)) {
		if(!sbbs->js_init(&stack_frame)) /* This must be done in the context of the node thread */
			lprintf(LOG_ERR,"Node %d !JavaScript Initialization FAILURE",sbbs->cfg.node_num);
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
				SAFEPRINTF2(str,"%s%s.bin",sbbs->cfg.mods_dir
					,sbbs->cfg.shell[sbbs->useron.shell]->code);
				if(sbbs->cfg.mods_dir[0]==0 || !fexistcase(str))
					SAFEPRINTF2(str,"%s%s.bin",sbbs->cfg.exec_dir
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
				if((sbbs->main_csi.cs=(uchar *)malloc(sbbs->main_csi.length))==NULL) {
					close(file);
					sbbs->errormsg(WHERE,ERR_ALLOC,str,sbbs->main_csi.length);
					sbbs->hangup();
					break; 
				}

				if(lread(file,sbbs->main_csi.cs,sbbs->main_csi.length)
					!=(int)sbbs->main_csi.length) {
					sbbs->errormsg(WHERE,ERR_READ,str,sbbs->main_csi.length);
					close(file);
					free(sbbs->main_csi.cs);
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
		sbbs->daily_maint();
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
	if(sbbs->input_thread_running || sbbs->output_thread_running
#ifdef USE_CRYPTLIB
		|| sbbs->passthru_input_thread_running || sbbs->passthru_output_thread_running
#endif
		) {
		lprintf(LOG_INFO,"Node %d Waiting for %s to terminate..."
			,sbbs->cfg.node_num
			,(sbbs->input_thread_running && sbbs->output_thread_running) ?
               	"I/O threads" : sbbs->input_thread_running
				? "input thread" : "output thread");
		time_t start=time(NULL);
		while(sbbs->input_thread_running
    		|| sbbs->output_thread_running
#ifdef USE_CRYPTLIB
			|| sbbs->passthru_input_thread_running || sbbs->passthru_output_thread_running
#endif
			) {
			if(time(NULL)-start>TIMEOUT_THREAD_WAIT) {
				lprintf(LOG_NOTICE,"Node %d !TIMEOUT waiting for %s to terminate"
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
	lprintf(LOG_DEBUG,"Node %d thread terminated (%u node threads remain, %lu clients served)"
		,sbbs->cfg.node_num, node_threads_running, served);
    if(!sbbs->input_thread_running && !sbbs->output_thread_running)
		delete sbbs;
    else
		lprintf(LOG_WARNING,"Node %d !ORPHANED I/O THREAD(s)",sbbs->cfg.node_num);

	update_clients();
	thread_down();
}

void sbbs_t::daily_maint(void)
{
	char			str[128];
	char			uname[LEN_ALIAS+1];
	uint			i;
	uint			usernum;
	uint			lastusernum;
	node_t			node;
	user_t			user;

	now=time(NULL);

	sbbs->getnodedat(sbbs->cfg.node_num,&node,1);
	node.status=NODE_EVENT_RUNNING;
	sbbs->putnodedat(sbbs->cfg.node_num,&node);

	sbbs->logentry("!:","Ran system daily maintenance");

	if(sbbs->cfg.user_backup_level) {
		lputs(LOG_INFO,"Backing-up user data...");
		SAFEPRINTF(str,"%suser/user.dat",sbbs->cfg.data_dir);
		backup(str,sbbs->cfg.user_backup_level,FALSE);
		SAFEPRINTF(str,"%suser/name.dat",sbbs->cfg.data_dir);
		backup(str,sbbs->cfg.user_backup_level,FALSE);
	}

	if(sbbs->cfg.mail_backup_level) {
		lputs(LOG_INFO,"Backing-up mail data...");
		SAFEPRINTF(str,"%smail.shd",sbbs->cfg.data_dir);
		backup(str,sbbs->cfg.mail_backup_level,FALSE);
		SAFEPRINTF(str,"%smail.sha",sbbs->cfg.data_dir);
		backup(str,sbbs->cfg.mail_backup_level,FALSE);
		SAFEPRINTF(str,"%smail.sdt",sbbs->cfg.data_dir);
		backup(str,sbbs->cfg.mail_backup_level,FALSE);
		SAFEPRINTF(str,"%smail.sda",sbbs->cfg.data_dir);
		backup(str,sbbs->cfg.mail_backup_level,FALSE);
		SAFEPRINTF(str,"%smail.sid",sbbs->cfg.data_dir);
		backup(str,sbbs->cfg.mail_backup_level,FALSE);
		SAFEPRINTF(str,"%smail.sch",sbbs->cfg.data_dir);
		backup(str,sbbs->cfg.mail_backup_level,FALSE);
	}

	lputs(LOG_INFO,"Checking for inactive/expired user records...");
	lastusernum=lastuser(&sbbs->cfg);
	for(usernum=1;usernum<=lastusernum;usernum++) {

		SAFEPRINTF2(str,"%5u of %-5u",usernum,lastusernum);
		status(str);
		user.number=usernum;
		if((i=getuserdat(&sbbs->cfg,&user))!=0) {
			SAFEPRINTF(str,"user record %u",usernum);
			sbbs->errormsg(WHERE,ERR_READ,str,i);
			continue;
		}

		/***********************************************/
		/* Fix name (name.dat and user.dat) mismatches */
		/***********************************************/
		username(&sbbs->cfg,user.number,uname);
		if(user.misc&DELETED) {
			if(strcmp(uname,"DELETED USER"))
				putusername(&sbbs->cfg,user.number,nulstr);
			continue; 
		}

		if(strcmp(user.alias,uname))
			putusername(&sbbs->cfg,user.number,user.alias);

		if(user.number==1)
			continue;	/* skip expiration/inactivity checks for user #1 */

		if(!(user.misc&(DELETED|INACTIVE))
			&& user.expire && (ulong)user.expire<=(ulong)now) {
			putsmsg(&sbbs->cfg,user.number,sbbs->text[AccountHasExpired]);
			SAFEPRINTF2(str,"%s #%u Expired",user.alias,user.number);
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
			putuserrec(&sbbs->cfg,user.number,U_LEVEL,2,ultoa(user.level,str,10));
			putuserrec(&sbbs->cfg,user.number,U_FLAGS1,8,ultoa(user.flags1,str,16));
			putuserrec(&sbbs->cfg,user.number,U_FLAGS2,8,ultoa(user.flags2,str,16));
			putuserrec(&sbbs->cfg,user.number,U_FLAGS3,8,ultoa(user.flags3,str,16));
			putuserrec(&sbbs->cfg,user.number,U_FLAGS4,8,ultoa(user.flags4,str,16));
			putuserrec(&sbbs->cfg,user.number,U_EXPIRE,8,ultoa(user.expire,str,16));
			putuserrec(&sbbs->cfg,user.number,U_EXEMPT,8,ultoa(user.exempt,str,16));
			putuserrec(&sbbs->cfg,user.number,U_REST,8,ultoa(user.rest,str,16));
			if(sbbs->cfg.expire_mod[0]) {
				sbbs->useron=user;
				sbbs->online=ON_LOCAL;
				sbbs->exec_bin(sbbs->cfg.expire_mod,&sbbs->main_csi);
				sbbs->online=FALSE; 
			}
		}

		/***********************************************************/
		/* Auto deletion based on expiration date or days inactive */
		/***********************************************************/
		if(!(user.exempt&FLAG('P'))     /* Not a permanent account */
			&& !(user.misc&(DELETED|INACTIVE))	 /* alive */
			&& (sbbs->cfg.sys_autodel && (now-user.laston)/(long)(24L*60L*60L)
			> sbbs->cfg.sys_autodel)) {			/* Inactive too long */
			SAFEPRINTF2(str,"Auto-Deleted %s #%u",user.alias,user.number);
			sbbs->logentry("!*",str);
			sbbs->delallmail(user.number);
			putusername(&sbbs->cfg,user.number,nulstr);
			putuserrec(&sbbs->cfg,user.number,U_MISC,8,ultoa(user.misc|DELETED,str,16)); 
		}
	}

	lputs(LOG_INFO,"Purging deleted/expired e-mail");
	SAFEPRINTF(sbbs->smb.file,"%smail",sbbs->cfg.data_dir);
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

	if(ver[0]==0) {	/* uninitialized */
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
	}
	return(ver);
}

/* Returns binary-coded version and revision (e.g. 0x31000 == 3.10a) */
long DLLCALL bbs_ver_num(void)
{
	return(VERSION_HEX);
}

void DLLCALL bbs_terminate(void)
{
   	lprintf(LOG_DEBUG,"BBS Server terminate");
	terminate_server=true;
}

static void cleanup(int code)
{
    lputs(LOG_INFO,"BBS System thread terminating");

	if(telnet_socket!=INVALID_SOCKET) {
		close_socket(telnet_socket);
		telnet_socket=INVALID_SOCKET;
	}
	if(rlogin_socket!=INVALID_SOCKET) {
		close_socket(rlogin_socket);
		rlogin_socket=INVALID_SOCKET;
	}
#ifdef USE_CRYPTLIB
	if(ssh_socket!=INVALID_SOCKET) {
		close_socket(ssh_socket);
		ssh_socket=INVALID_SOCKET;
	}
#endif


#ifdef _WINSOCKAPI_
	if(WSAInitialized && WSACleanup()!=0) 
		lprintf(LOG_ERR,"!WSACleanup ERROR %d",ERROR_VALUE);
#endif

	free_cfg(&scfg);
	free_text(text);

	semfile_list_free(&recycle_semfiles);
	semfile_list_free(&shutdown_semfiles);

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

	status("Down");
	thread_down();
	if(terminate_server || code)
		lprintf(LOG_INFO,"BBS System thread terminated (%u threads remain, %lu clients served)"
			,thread_count, served);
	if(startup->terminated!=NULL)
		startup->terminated(startup->cbdata,code);
}

void DLLCALL bbs_thread(void* arg)
{
	char*			host_name;
	char*			identity;
	char*			p;
    char			str[MAX_PATH+1];
	char			logstr[256];
	SOCKADDR_IN		server_addr={0};
	SOCKADDR_IN		client_addr;
	socklen_t		client_addr_len;
	SOCKET			client_socket=INVALID_SOCKET;
	fd_set			socket_set;
	SOCKET			high_socket_set;
	int				i;
    int				file;
	int				result;
	time_t			t;
	time_t			start;
	time_t			initialized=0;
	node_t			node;
	sbbs_t*			events=NULL;
	client_t		client;
	startup=(bbs_startup_t*)arg;
	BOOL			is_client=FALSE;
#ifdef __unix__
	SOCKET	uspy_listen_socket[MAX_NODES];
	struct sockaddr_un uspy_addr;
	socklen_t		uspy_addr_len;
#endif
#ifdef USE_CRYPTLIB
	CRYPT_CONTEXT	ssh_context;
#endif

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

#ifdef _THREAD_SUID_BROKEN
	if(thread_suid_broken)
		startup->seteuid(TRUE);
#endif

	/* Setup intelligent defaults */
	if(startup->telnet_port==0)				startup->telnet_port=IPPORT_TELNET;
	if(startup->rlogin_port==0)				startup->rlogin_port=513;
#ifdef USE_CRYPTLIB
	if(startup->ssh_port==0)				startup->ssh_port=22;
#endif
	if(startup->outbuf_drain_timeout==0)	startup->outbuf_drain_timeout=10;
	if(startup->sem_chk_freq==0)			startup->sem_chk_freq=2;
	if(startup->temp_dir[0])				backslash(startup->temp_dir);

	ZERO_VAR(js_server_props);
	SAFEPRINTF3(js_server_props.version,"%s %s%c",TELNET_SERVER,VERSION,REVISION);
	js_server_props.version_detail=bbs_ver();
	js_server_props.clients=&node_threads_running;
	js_server_props.options=&startup->options;
	js_server_props.interface_addr=&startup->telnet_interface;

	uptime=0;
	served=0;
	startup->recycle_now=FALSE;
	startup->shutdown_now=FALSE;
	terminate_server=false;

	do {

	thread_up(FALSE /* setuid */);

	status("Initializing");

	/* Defeat the lameo hex0rs - the name and copyright must remain intact */
	if(crc32(COPYRIGHT_NOTICE,0)!=COPYRIGHT_CRC 
		|| crc32(VERSION_NOTICE,10)!=SYNCHRONET_CRC) {
		lprintf(LOG_ERR,"!CORRUPTED LIBRARY FILE");
		cleanup(1);
		return;
	}

	memset(text, 0, sizeof(text));
    memset(&scfg, 0, sizeof(scfg));

	node_threads_running=0;
	lastuseron[0]=0;

	char compiler[32];
	DESCRIBE_COMPILER(compiler);

	lprintf(LOG_INFO,"%s Version %s Revision %c%s"
		,TELNET_SERVER
		,VERSION
		,toupper(REVISION)
#ifdef _DEBUG
		," Debug"
#else
		,""
#endif
		);
	lprintf(LOG_INFO,"Compiled %s %s with %s", __DATE__, __TIME__, compiler);
	lprintf(LOG_INFO,"SMBLIB %s (format %x.%02x)",smb_lib_ver(),smb_ver()>>8,smb_ver()&0xff);

    if(startup->first_node<1 || startup->first_node>startup->last_node) {
    	lprintf(LOG_ERR,"!ILLEGAL node configuration (first: %d, last: %d)"
        	,startup->first_node, startup->last_node);
		cleanup(1);
        return;
    }

#ifdef __BORLANDC__
	#pragma warn -8008	/* Disable "Condition always false" warning */
	#pragma warn -8066	/* Disable "Unreachable code" warning */
#endif
	if(sizeof(node_t)!=SIZEOF_NODE_T) {
		lprintf(LOG_ERR,"!COMPILER ERROR: sizeof(node_t)=%d instead of %d"
			,sizeof(node_t),SIZEOF_NODE_T);
		cleanup(1);
		return;
	}

#ifdef _WIN32
    if((exec_mutex=CreateMutex(NULL,false,NULL))==NULL) {
    	lprintf(LOG_ERR,"!ERROR %d creating exec_mutex", GetLastError());
		cleanup(1);
        return;
    }
	hK32 = LoadLibrary("KERNEL32");
#endif // _WIN32

	if(!winsock_startup()) {
		cleanup(1);
		return;
	}

	t=time(NULL);
	lprintf(LOG_INFO,"Initializing on %.24s with options: %lx"
		,ctime_r(&t,str),startup->options);

	if(chdir(startup->ctrl_dir)!=0)
		lprintf(LOG_ERR,"!ERROR %d changing directory to: %s", errno, startup->ctrl_dir);

	/* Initial configuration and load from CNF files */
    SAFECOPY(scfg.ctrl_dir,startup->ctrl_dir);
    lprintf(LOG_INFO,"Loading configuration files from %s", scfg.ctrl_dir);
	scfg.size=sizeof(scfg);
	scfg.node_num=startup->first_node;
	SAFECOPY(logstr,UNKNOWN_LOAD_ERROR);
	if(!load_cfg(&scfg, text, TRUE, logstr)) {
		lprintf(LOG_ERR,"!ERROR %s",logstr);
		lprintf(LOG_ERR,"!FAILED to load configuration files");
		cleanup(1);
		return;
	}
	
	if(startup->host_name[0]==0)
		SAFECOPY(startup->host_name,scfg.sys_inetaddr);

	if((t=checktime())!=0) {   /* Check binary time */
		lprintf(LOG_ERR,"!TIME PROBLEM (%ld)",t);
	}

	if(uptime==0)
		uptime=time(NULL);	/* this must be done *after* setting the timezone */

    if(startup->last_node>scfg.sys_nodes) {
    	lprintf(LOG_NOTICE,"Specified last_node (%d) > sys_nodes (%d), auto-corrected"
        	,startup->last_node, scfg.sys_nodes);
        startup->last_node=scfg.sys_nodes;
    }

	/* Create missing directories */
	lprintf(LOG_INFO,"Verifying/creating data directories");
	make_data_dirs(&scfg);

	/* Create missing node directories and dsts.dab files */
	lprintf(LOG_INFO,"Verifying/creating node directories");
	for(i=0;i<=scfg.sys_nodes;i++) {
		if(i)
			md(scfg.node_path[i-1]);
		SAFEPRINTF(str,"%sdsts.dab",i ? scfg.node_path[i-1] : scfg.ctrl_dir);
		if(flength(str)<DSTSDABLEN) {
			if((file=sopen(str,O_WRONLY|O_CREAT|O_APPEND, SH_DENYNO, DEFFILEMODE))==-1) {
				lprintf(LOG_ERR,"!ERROR %d creating %s",errno, str);
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
#ifdef __unix__
		uspy_socket[i]=INVALID_SOCKET;
		uspy_listen_socket[i]=INVALID_SOCKET;
#endif
	}

	startup->node_inbuf=node_inbuf;

    /* open a socket and wait for a client */

    telnet_socket = open_socket(SOCK_STREAM, "telnet");

	if(telnet_socket == INVALID_SOCKET) {
		lprintf(LOG_ERR,"!ERROR %d creating Telnet socket", ERROR_VALUE);
		cleanup(1);
		return;
	}

    lprintf(LOG_INFO,"Telnet socket %d opened",telnet_socket);

	/*****************************/
	/* Listen for incoming calls */
	/*****************************/
    memset(&server_addr, 0, sizeof(server_addr));

	server_addr.sin_addr.s_addr = htonl(startup->telnet_interface);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(startup->telnet_port);

	if(startup->telnet_port < IPPORT_RESERVED) {
		if(startup->seteuid!=NULL)
			startup->seteuid(FALSE);
	}
    result = retry_bind(telnet_socket,(struct sockaddr *)&server_addr,sizeof(server_addr)
		,startup->bind_retry_count,startup->bind_retry_delay,"Telnet Server",lprintf);
	if(startup->telnet_port < IPPORT_RESERVED) {
		if(startup->seteuid!=NULL)
			startup->seteuid(TRUE);
	}
	if(result != 0) {
		lprintf(LOG_NOTICE,"%s",BIND_FAILURE_HELP);
		cleanup(1);
		return;
	}

    result = listen(telnet_socket, 1);

	if(result != 0) {
		lprintf(LOG_ERR,"!ERROR %d (%d) listening on Telnet socket", result, ERROR_VALUE);
		cleanup(1);
		return;
	}
	lprintf(LOG_INFO,"Telnet server listening on port %d",startup->telnet_port);

	if(startup->options&BBS_OPT_ALLOW_RLOGIN) {

		/* open a socket and wait for a client */

		rlogin_socket = open_socket(SOCK_STREAM, "rlogin");

		if(rlogin_socket == INVALID_SOCKET) {
			lprintf(LOG_ERR,"!ERROR %d creating RLogin socket", ERROR_VALUE);
			cleanup(1);
			return;
		}

		lprintf(LOG_INFO,"RLogin socket %d opened",rlogin_socket);

		/*****************************/
		/* Listen for incoming calls */
		/*****************************/
		memset(&server_addr, 0, sizeof(server_addr));

		server_addr.sin_addr.s_addr = htonl(startup->rlogin_interface);
		server_addr.sin_family = AF_INET;
		server_addr.sin_port   = htons(startup->rlogin_port);

		if(startup->rlogin_port < IPPORT_RESERVED) {
			if(startup->seteuid!=NULL)
				startup->seteuid(FALSE);
		}
		result = retry_bind(rlogin_socket,(struct sockaddr *)&server_addr,sizeof(server_addr)
			,startup->bind_retry_count,startup->bind_retry_delay,"RLogin Server",lprintf);
		if(startup->rlogin_port < IPPORT_RESERVED) {
			if(startup->seteuid!=NULL)
				startup->seteuid(TRUE);
		}
		if(result != 0) {
			lprintf(LOG_NOTICE,"%s",BIND_FAILURE_HELP);
			cleanup(1);
			return;
		}

		result = listen(rlogin_socket, 1);

		if(result != 0) {
			lprintf(LOG_ERR,"!ERROR %d (%d) listening on RLogin socket", result, ERROR_VALUE);
			cleanup(1);
			return;
		}
		lprintf(LOG_INFO,"RLogin server listening on port %d",startup->rlogin_port);
	}

#ifdef USE_CRYPTLIB
#if CRYPTLIB_VERSION < 3300
	#warning This version of Cryptlib is known to crash Synchronet.  Upgrade to at least version 3.3 or do not build with Cryptlib support.
#endif
	if(startup->options&BBS_OPT_ALLOW_SSH) {
		bool			loaded_key=false;

		CRYPT_KEYSET	ssh_keyset;

		cryptInit();
		cryptAddRandom(NULL,CRYPT_RANDOM_SLOWPOLL);
		/* Get the private key... first try loading it from a file... */
		SAFEPRINTF2(str,"%s%s",scfg.ctrl_dir,"cryptlib.key");
		if(cryptStatusOK(cryptKeysetOpen(&ssh_keyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE, str, CRYPT_KEYOPT_NONE))) {
			if(cryptStatusOK(cryptGetPrivateKey(ssh_keyset, &ssh_context, CRYPT_KEYID_NAME, "ssh_server", scfg.sys_pass)))
				loaded_key=true;
			cryptKeysetClose(ssh_keyset);
			/* Failed to load the key... delete the keyfile and create a new one */
			if(!loaded_key)
				remove(str);
		}

		if(!loaded_key) {
			/* Couldn't do that... create a new context and use the key from there... */

			if(!cryptStatusOK(i=cryptCreateContext(&ssh_context, CRYPT_UNUSED, CRYPT_ALGO_RSA))) {
				lprintf(LOG_ERR,"Cryptlib error %d creating context",i);
				goto NO_SSH;
			}
			if(!cryptStatusOK(i=cryptSetAttributeString(ssh_context, CRYPT_CTXINFO_LABEL, "ssh_server", 10))) {
				lprintf(LOG_ERR,"Cryptlib error %d setting key label",i);
				goto NO_SSH;
			}
			if(!cryptStatusOK(i=cryptGenerateKey(ssh_context))) {
				lprintf(LOG_ERR,"Cryptlib error %d generating key",i);
				goto NO_SSH;
			}

			/* Ok, now try saving this one... use the syspass to enctrpy it. */
			if(cryptStatusOK(cryptKeysetOpen(&ssh_keyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE, str, CRYPT_KEYOPT_CREATE))) {
				cryptAddPrivateKey(ssh_keyset, ssh_context, scfg.sys_pass);
				cryptKeysetClose(ssh_keyset);
			}
		}

		/* open a socket and wait for a client */

		ssh_socket = open_socket(SOCK_STREAM, "ssh");

		if(ssh_socket == INVALID_SOCKET) {
			lprintf(LOG_ERR,"!ERROR %d creating SSH socket", ERROR_VALUE);
			cleanup(1);
			return;
		}

		lprintf(LOG_INFO,"SSH socket %d opened",ssh_socket);

		/*****************************/
		/* Listen for incoming calls */
		/*****************************/
		memset(&server_addr, 0, sizeof(server_addr));

		server_addr.sin_addr.s_addr = htonl(startup->ssh_interface);
		server_addr.sin_family = AF_INET;
		server_addr.sin_port   = htons(startup->ssh_port);

		if(startup->ssh_port < IPPORT_RESERVED) {
			if(startup->seteuid!=NULL)
				startup->seteuid(FALSE);
		}
		result = retry_bind(ssh_socket,(struct sockaddr *)&server_addr,sizeof(server_addr)
			,startup->bind_retry_count,startup->bind_retry_delay,"SSH Server",lprintf);
		if(startup->ssh_port < IPPORT_RESERVED) {
			if(startup->seteuid!=NULL)
				startup->seteuid(TRUE);
		}
		if(result != 0) {
			lprintf(LOG_NOTICE,"%s",BIND_FAILURE_HELP);
			cleanup(1);
			return;
		}

		result = listen(ssh_socket, 1);

		if(result != 0) {
			lprintf(LOG_ERR,"!ERROR %d (%d) listening on SSH socket", result, ERROR_VALUE);
			cleanup(1);
			return;
		}
		lprintf(LOG_INFO,"SSH server listening on port %d",startup->ssh_port);
	}
NO_SSH:
#endif

	sbbs = new sbbs_t(0, server_addr.sin_addr.s_addr
		,"BBS System", telnet_socket, &scfg, text, NULL);
    sbbs->online = 0;
	if(sbbs->init()==false) {
		lputs(LOG_ERR,"!BBS initialization failed");
		cleanup(1);
		return;
	}
	_beginthread(output_thread, 0, sbbs);

	if(!(startup->options&BBS_OPT_NO_EVENTS)) {
		events = new sbbs_t(0, server_addr.sin_addr.s_addr
			,"BBS Events", INVALID_SOCKET, &scfg, text, NULL);
		events->online = 0;
		if(events->init()==false) {
			lputs(LOG_ERR,"!Events initialization failed");
			cleanup(1);
			return;
		}
		_beginthread(event_thread, 0, events);
	}

	/* Save these values incase they're changed dynamically */
	first_node=startup->first_node;
	last_node=startup->last_node;

	for(i=first_node;i<=last_node;i++) {
		sbbs->getnodedat(i,&node,1);
		node.status=NODE_WFC;
		node.misc&=NODE_EVENT;	/* Note: Turns-off NODE_RRUN flag (and others) */
		node.action=0;
		sbbs->putnodedat(i,&node);
	}

	lprintf(LOG_INFO,"BBS System thread started for nodes %d through %d", first_node, last_node);
	status(STATUS_WFC);

#if defined(_WIN32) && defined(_DEBUG) && defined(_MSC_VER)
	
	SAFEPRINTF(str,"%sDEBUG.LOG",scfg.logs_dir);
	if((debug_log=CreateFile(
		str,				// pointer to name of the file
		GENERIC_READ|GENERIC_WRITE,
		FILE_SHARE_READ|FILE_SHARE_WRITE,
		NULL,               // pointer to security attributes
		OPEN_ALWAYS,		// how to create
		FILE_ATTRIBUTE_NORMAL, // file attributes
		NULL				// handle to file with attributes to 
		))==INVALID_HANDLE_VALUE) {
		lprintf(LOG_ERR,"!ERROR %ld creating %s",GetLastError(),str);
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

	/* Setup recycle/shutdown semaphore file lists */
	shutdown_semfiles=semfile_list_init(scfg.ctrl_dir,"shutdown","telnet");
	recycle_semfiles=semfile_list_init(scfg.ctrl_dir,"recycle","telnet");
	SAFEPRINTF(str,"%stelnet.rec",scfg.ctrl_dir);	/* legacy */
	semfile_list_add(&recycle_semfiles,str);
	if(!initialized)
		semfile_list_check(&initialized,shutdown_semfiles);
	semfile_list_check(&initialized,recycle_semfiles);

#ifdef __unix__	//	unix-domain spy sockets
	for(i=first_node;i<=last_node && !(startup->options&BBS_OPT_NO_SPY_SOCKETS);i++)  {
	    if((uspy_listen_socket[i-1]=socket(PF_UNIX,SOCK_STREAM,0))==INVALID_SOCKET)
	        lprintf(LOG_ERR,"Node %d !ERROR %d creating local spy socket"
	            , i, errno);
	    else {
	        lprintf(LOG_INFO,"Node %d local spy using socket %d", i, uspy_listen_socket[i-1]);
	        if(startup!=NULL && startup->socket_open!=NULL)
	            startup->socket_open(startup->cbdata,TRUE);
	    }
	
	    uspy_addr.sun_family=AF_UNIX;
	    if((unsigned int)snprintf(str,sizeof(uspy_addr.sun_path),
	            "%slocalspy%d.sock", startup->temp_dir, i)
	            >=sizeof(uspy_addr.sun_path))
	        uspy_listen_socket[i-1]=INVALID_SOCKET;
	    else  {
	        strcpy(uspy_addr.sun_path,str);
	        if(fexist(str))
	            unlink(str);
	    }
	    if(uspy_listen_socket[i-1]!=INVALID_SOCKET) {
	        uspy_addr_len=SUN_LEN(&uspy_addr);
	        if(bind(uspy_listen_socket[i-1], (struct sockaddr *) &uspy_addr, uspy_addr_len)) {
	            lprintf(LOG_ERR,"Node %d !ERROR %d binding local spy socket %d to %s"
	                , i, errno, uspy_listen_socket[i-1], uspy_addr.sun_path);
	            close_socket(uspy_listen_socket[i-1]);
				uspy_listen_socket[i-1]=INVALID_SOCKET;
	            continue;
	        }
            lprintf(LOG_INFO,"Node %d local spy socket %d bound to %s"
                , i, uspy_listen_socket[i-1], uspy_addr.sun_path);
	        if(listen(uspy_listen_socket[i-1],1))  {
	            lprintf(LOG_ERR,"Node %d !ERROR %d listening local spy socket %d"
					,i, errno, uspy_listen_socket[i-1]);
	            close_socket(uspy_listen_socket[i-1]);
				uspy_listen_socket[i-1]=INVALID_SOCKET;
	            continue;
			}
	        uspy_addr_len=sizeof(uspy_addr);
	    }
	}
#endif // __unix__ (unix-domain spy sockets)

	/* signal caller that we've started up successfully */
    if(startup->started!=NULL)
    	startup->started(startup->cbdata);


	while(!terminate_server) {

		if(node_threads_running==0) {	/* check for re-run flags and recycle/shutdown sem files */
			if(!(startup->options&BBS_OPT_NO_RECYCLE)) {

				bool rerun=false;
				for(i=first_node;i<=last_node;i++) {
					if(sbbs->getnodedat(i,&node,0)!=0)
						continue;
					if(node.misc&NODE_RRUN) {
						sbbs->getnodedat(i,&node,1);
						if(!rerun)
							lprintf(LOG_INFO,"Node %d flagged for re-run",i);
						rerun=true;
						node.misc&=~NODE_RRUN;
						sbbs->putnodedat(i,&node);
					}
				}
				if(rerun)
					break;

				if((p=semfile_list_check(&initialized,recycle_semfiles))!=NULL) {
					lprintf(LOG_INFO,"%04d Recycle semaphore file (%s) detected"
						,telnet_socket,p);
					break;
				}
				if(startup->recycle_now==TRUE) {
					lprintf(LOG_INFO,"%04d Recycle semaphore signaled",telnet_socket);
					startup->recycle_now=FALSE;
					break;
				}
			}
			if(((p=semfile_list_check(&initialized,shutdown_semfiles))!=NULL
					&& lprintf(LOG_INFO,"%04d Shutdown semaphore file (%s) detected"
						,telnet_socket,p))
				|| (startup->shutdown_now==TRUE
					&& lprintf(LOG_INFO,"%04d Shutdown semaphore signaled"
						,telnet_socket))) {
				startup->shutdown_now=FALSE;
				terminate_server=TRUE;
				break;
			}
		}

    	sbbs->online=FALSE;
#ifdef USE_CRYPTLIB
		sbbs->ssh_mode=false;
#endif

		/* now wait for connection */

		FD_ZERO(&socket_set);
		high_socket_set=0;
		if(telnet_socket!=INVALID_SOCKET) {
			FD_SET(telnet_socket,&socket_set);
			high_socket_set=telnet_socket+1;
		}
		if(startup->options&BBS_OPT_ALLOW_RLOGIN 
			&& rlogin_socket!=INVALID_SOCKET) {
			FD_SET(rlogin_socket,&socket_set);
			if(rlogin_socket+1>high_socket_set)
				high_socket_set=rlogin_socket+1;
		}
#ifdef USE_CRYPTLIB
		if(startup->options&BBS_OPT_ALLOW_SSH
			&& ssh_socket!=INVALID_SOCKET) {
			FD_SET(ssh_socket,&socket_set);
			if(ssh_socket+1>high_socket_set)
				high_socket_set=ssh_socket+1;
		}
#endif
#ifdef __unix__
		for(i=first_node;i<=last_node;i++)  {
			if(uspy_listen_socket[i-1]!=INVALID_SOCKET)  {
				FD_SET(uspy_listen_socket[i-1],&socket_set);
				if(uspy_listen_socket[i-1]+1>high_socket_set)
					high_socket_set=uspy_listen_socket[i-1]+1;
			}
			if(uspy_socket[i-1]!=INVALID_SOCKET)  {
				FD_SET(uspy_socket[i-1],&socket_set);
				if(uspy_socket[i-1]+1>high_socket_set)
					high_socket_set=uspy_listen_socket[i-1]+1;
			}
		}
#endif

		struct timeval tv;
		tv.tv_sec=startup->sem_chk_freq;
		tv.tv_usec=0;

		if((i=select(high_socket_set,&socket_set,NULL,NULL,&tv))<1) {
			if(i==0)
				continue;
			if(ERROR_VALUE==EINTR)
				lprintf(LOG_DEBUG,"Telnet Server listening interrupted");
			else if(ERROR_VALUE == ENOTSOCK)
            	lprintf(LOG_NOTICE,"Telnet Server sockets closed");
			else
				lprintf(LOG_WARNING,"!ERROR %d selecting sockets",ERROR_VALUE);
			continue;
		}

		if(terminate_server)	/* terminated */
			break;

		client_addr_len = sizeof(client_addr);

		bool rlogin = false;
#ifdef USE_CRYPTLIB
		bool ssh = false;
#endif

		is_client=FALSE;
		if(telnet_socket!=INVALID_SOCKET 
			&& FD_ISSET(telnet_socket,&socket_set)) {
			client_socket = accept_socket(telnet_socket, (struct sockaddr *)&client_addr
	        	,&client_addr_len);
	        is_client=TRUE;
		} else if(rlogin_socket!=INVALID_SOCKET 
			&& FD_ISSET(rlogin_socket,&socket_set)) {
			client_socket = accept_socket(rlogin_socket, (struct sockaddr *)&client_addr
	        	,&client_addr_len);
			rlogin = true;
			is_client=TRUE;
#ifdef USE_CRYPTLIB
		} else if(ssh_socket!=INVALID_SOCKET 
			&& FD_ISSET(ssh_socket,&socket_set)) {

			client_socket = accept_socket(ssh_socket, (struct sockaddr *)&client_addr
	        	,&client_addr_len);
			ssh = true;
			is_client=TRUE;
			sbbs->ssh_mode=true;
#endif
		} else {
#ifdef __unix__
			for(i=first_node;i<=last_node;i++)  {
				if(uspy_socket[i-1]!=INVALID_SOCKET
				&& FD_ISSET(uspy_socket[i-1],&socket_set)) {
					if(node_socket[i-1]==INVALID_SOCKET)
						read(uspy_socket[i-1],str,sizeof(str));
					if(!socket_check(uspy_socket[i-1],NULL,NULL,0)) {
						lprintf(LOG_NOTICE,"Spy socket for node %d disconnected",i);
						close_socket(uspy_socket[i-1]);
						uspy_socket[i-1]=INVALID_SOCKET;
					}
				}
				if(uspy_listen_socket[i-1]!=INVALID_SOCKET
				&& FD_ISSET(uspy_listen_socket[i-1],&socket_set)) {
					BOOL already_connected=(uspy_socket[i-1]!=INVALID_SOCKET);
					SOCKET new_socket=INVALID_SOCKET;
					new_socket = accept(uspy_listen_socket[i-1], (struct sockaddr *)&uspy_addr
						,&uspy_addr_len);
					if(new_socket < 0)  {
						lprintf(LOG_ERR,"!ERROR Spy socket for node %d unable to accept()",i);
						close_socket(uspy_listen_socket[i-1]);
						uspy_listen_socket[i-1]=INVALID_SOCKET;
					}
					fcntl(new_socket,F_SETFL,fcntl(new_socket,F_GETFL)|O_NONBLOCK);
					if(already_connected)  {
						lprintf(LOG_ERR,"!ERROR Spy socket %s already in use",uspy_addr.sun_path);
						send(new_socket,"Spy socket already in use.\r\n",27,0);
						close_socket(new_socket);
					}
					else  {
						lprintf(LOG_ERR,"!Spy socket %s (%d) connected",uspy_addr.sun_path,new_socket);
						uspy_socket[i-1]=new_socket;
						SAFEPRINTF(str,"Spy connection established to node %d\r\n",i);
						send(uspy_socket[i-1],str,strlen(str),0);
					}
				}
			}
#else
			lprintf(LOG_ERR,"!NO SOCKETS set by select");
#endif
		}

		if(!is_client) {
			/* Do not need to close_socket(client_socket) here */
			continue;
		}

		if(client_socket == INVALID_SOCKET)	{
#if 0	/* is this necessary still? */
			if(ERROR_VALUE == ENOTSOCK || ERROR_VALUE == EINTR || ERROR_VALUE == EINVAL) {
            	lputs(LOG_NOTICE,"BBS socket closed");
				break;
			}
#endif
			lprintf(LOG_ERR,"!ERROR %d accepting connection", ERROR_VALUE);
#ifdef _WIN32
			if(WSAGetLastError()==WSAENOBUFS)	/* recycle (re-init WinSock) on this error */
				break;
#endif
			SSH_END();
			continue;
		}
		char host_ip[32];

		strcpy(host_ip,inet_ntoa(client_addr.sin_addr));

		if(trashcan(&scfg,host_ip,"ip-silent")) {
			SSH_END();
			close_socket(client_socket);
			continue;
		}

		lprintf(LOG_INFO,"%04d %s connection accepted from: %s port %u"
			,client_socket
#ifdef USE_CRYPTLIB
			,rlogin ? "RLogin" : (ssh ? "SSH" : "Telnet")
#else
			,rlogin ? "RLogin" : "Telnet"
#endif
			, host_ip, ntohs(client_addr.sin_port));

#ifdef _WIN32
		if(startup->answer_sound[0] && !(startup->options&BBS_OPT_MUTE)) 
			PlaySound(startup->answer_sound, NULL, SND_ASYNC|SND_FILENAME);
#endif

		/* Do SSH stuff here */

		if(ssh) {
			if(!cryptStatusOK(i=cryptCreateSession(&sbbs->ssh_session, CRYPT_UNUSED, CRYPT_SESSION_SSH_SERVER))) {
				lprintf(LOG_ERR,"%04d Cryptlib error %d creating session", client_socket, i);
				close_socket(client_socket);
				continue;
			}
			if(!cryptStatusOK(i=cryptSetAttribute(sbbs->ssh_session, CRYPT_SESSINFO_PRIVATEKEY, ssh_context))) {
				lprintf(LOG_ERR,"%04d Cryptlib error %d setting private key",client_socket, i);
				cryptDestroySession(sbbs->ssh_session);
				close_socket(client_socket);
				continue;
			}
			/* Accept any credentials */
			if(!cryptStatusOK(i=cryptSetAttribute(sbbs->ssh_session, CRYPT_SESSINFO_AUTHRESPONSE, 1))) {
				lprintf(LOG_ERR,"%04d Cryptlib error %d setting AUTHRESPONSE",client_socket, i);
				cryptDestroySession(sbbs->ssh_session);
				close_socket(client_socket);
				continue;
			}
			if(!cryptStatusOK(i=cryptSetAttribute(sbbs->ssh_session, CRYPT_SESSINFO_NETWORKSOCKET, client_socket))) {
				lprintf(LOG_ERR,"%04d Cryptlib error %d setting socket",client_socket, i);
				cryptDestroySession(sbbs->ssh_session);
				close_socket(client_socket);
				continue;
			}
			if(!cryptStatusOK(i=cryptSetAttribute(sbbs->ssh_session, CRYPT_SESSINFO_ACTIVE, 1))) {
				lprintf(LOG_ERR,"%04d Cryptlib error %d setting session active",client_socket, i);
				cryptDestroySession(sbbs->ssh_session);
				close_socket(client_socket);
				continue;
			}
			cryptPopData(sbbs->ssh_session, str, sizeof(str), &i);
		}

   		sbbs->client_socket=client_socket;	// required for output to the user
        sbbs->online=ON_REMOTE;

		if(sbbs->trashcan(host_ip,"ip")) {
			SSH_END();
			close_socket(client_socket);
			lprintf(LOG_NOTICE,"%04d !CLIENT BLOCKED in ip.can"
				,client_socket);
			SAFEPRINTF(logstr, "Blocked IP: %s",host_ip);
			sbbs->syslog("@!",logstr);
			continue;
		}

		if(rlogin)
			sbbs->outcom(0); /* acknowledge RLogin per RFC 1282 */

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

		if(!(startup->options&BBS_OPT_NO_HOST_LOOKUP)) {
			lprintf(LOG_INFO,"%04d Hostname: %s", client_socket, host_name);
			for(i=0;h!=NULL && h->h_aliases!=NULL && h->h_aliases[i]!=NULL;i++)
				lprintf(LOG_INFO,"%04d HostAlias: %s", client_socket, h->h_aliases[i]);
		}

		if(sbbs->trashcan(host_name,"host")) {
			SSH_END();
			close_socket(client_socket);
			lprintf(LOG_NOTICE,"%04d !CLIENT BLOCKED in host.can",client_socket);
			SAFEPRINTF(logstr, "Blocked Hostname: %s",host_name);
			sbbs->syslog("@!",logstr);
			continue;
		}

		identity=NULL;
		if(startup->options&BBS_OPT_GET_IDENT) {
			sbbs->bprintf("Resolving identity...");
			/* ToDo: Make ident timeout configurable */
			if(identify(&client_addr, startup->telnet_port, str, sizeof(str)-1, /* timeout: */1)) {
				lprintf(LOG_DEBUG,"%04d Ident Response: %s",client_socket, str);
				identity=strrchr(str,':');
				if(identity!=NULL) {
					identity++;	/* skip colon */
					SKIP_WHITESPACE(identity);
					if(*identity)
						lprintf(LOG_INFO,"%04d Identity: %s",client_socket, identity);
				}
			}
			sbbs->putcom(crlf);
		}
		/* Initialize client display */
		client.size=sizeof(client);
		client.time=time(NULL);
		SAFECOPY(client.addr,host_ip);
		SAFECOPY(client.host,host_name);
		client.port=ntohs(client_addr.sin_port);
#ifdef USE_CRYPTLIB
		client.protocol=rlogin ? "RLogin":(ssh ? "SSH" : "Telnet");
#else
		client.protocol=rlogin ? "RLogin":"Telnet";
#endif
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
			lprintf(LOG_WARNING,"%04d !No nodes available for login.",client_socket);
			SAFEPRINTF(str,"%snonodes.txt",scfg.text_dir);
			if(fexist(str))
				sbbs->printfile(str,P_NOABORT);
			else {
				sbbs->putcom("\r\nSorry, all telnet nodes are in use or otherwise unavailable.\r\n");
				sbbs->putcom("Please try again later.\r\n");
			}
			mswait(3000);
			client_off(client_socket);
			SSH_END();
			close_socket(client_socket);
			continue;
		}

        node_socket[i-1]=client_socket;

		sbbs_t* new_node = new sbbs_t(i, client_addr.sin_addr.s_addr, host_name
        	,client_socket
			,&scfg, text, &client);

		new_node->client=client;
#ifdef USE_CRYPTLIB
		if(ssh) {
			new_node->ssh_session=sbbs->ssh_session;
			new_node->ssh_mode=true;
		}
#endif

		/* copy the IDENT response, if any */
		if(identity!=NULL)
			SAFECOPY(new_node->client_ident,identity);

		if(new_node->init()==false) {
			lprintf(LOG_INFO,"%04d !Node %d Initialization failure"
				,client_socket,new_node->cfg.node_num);
			SAFEPRINTF(str,"%snonodes.txt",scfg.text_dir);
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
			SSH_END();
			close_socket(client_socket);
			continue;
		}

		if(rlogin==true) {
			SAFECOPY(new_node->connection,"RLogin");
			new_node->node_connection=NODE_CONNECTION_RLOGIN;
			new_node->sys_status|=SS_RLOGIN;
			new_node->telnet_mode|=TELNET_MODE_OFF; // RLogin does not use Telnet commands
		}
#ifdef USE_CRYPTLIB
		if(ssh) {
			SOCKET	tmp_sock;
			SOCKADDR_IN		tmp_addr={0};
			socklen_t		tmp_addr_len;

    		/* open a socket and connect to yourself */

    		tmp_sock = open_socket(SOCK_STREAM, "passthru");

			if(tmp_sock == INVALID_SOCKET) {
				lprintf(LOG_ERR,"!ERROR %d creating passthru listen socket", ERROR_VALUE);
				goto NO_PASSTHRU;
			}

    		lprintf(LOG_INFO,"passthru listen socket %d opened",tmp_sock);

			/*****************************/
			/* Listen for incoming calls */
			/*****************************/
    		memset(&tmp_addr, 0, sizeof(tmp_addr));

			tmp_addr.sin_addr.s_addr = htonl(IPv4_LOCALHOST);
    		tmp_addr.sin_family = AF_INET;
    		tmp_addr.sin_port   = 0;

    		result = bind(tmp_sock,(struct sockaddr *)&tmp_addr,sizeof(tmp_addr));
			if(result != 0) {
				lprintf(LOG_NOTICE,"%s",BIND_FAILURE_HELP);
				close_socket(tmp_sock);
				goto NO_PASSTHRU;
			}

		    result = listen(tmp_sock, 1);

			if(result != 0) {
				lprintf(LOG_ERR,"!ERROR %d (%d) listening on passthru socket", result, ERROR_VALUE);
				close_socket(tmp_sock);
				goto NO_PASSTHRU;
			}
			lprintf(LOG_INFO,"Listening passthru socket listening on port %d",htons(tmp_addr.sin_port));

    		new_node->passthru_socket = open_socket(SOCK_STREAM, "passthru");

			if(new_node->passthru_socket == INVALID_SOCKET) {
				lprintf(LOG_ERR,"!ERROR %d creating passthru connecting socket", ERROR_VALUE);
				close_socket(tmp_sock);
				goto NO_PASSTHRU;
			}

    		lprintf(LOG_INFO,"passthru connect socket %d opened",new_node->passthru_socket);

			tmp_addr_len=sizeof(tmp_addr);
			if(getsockname(tmp_sock, (struct sockaddr *)&tmp_addr, &tmp_addr_len)) {
				lprintf(LOG_ERR,"!ERROR %d getting passthru listener address", ERROR_VALUE);
				close_socket(tmp_sock);
				close_socket(new_node->passthru_socket);
				new_node->passthru_socket=INVALID_SOCKET;
				goto NO_PASSTHRU;
			}

		    result = connect(new_node->passthru_socket, (struct sockaddr *)&tmp_addr, tmp_addr_len);

			if(result != 0) {
				lprintf(LOG_ERR,"!ERROR %d (%d) connecting to passthru socket", result, ERROR_VALUE);
				close_socket(new_node->passthru_socket);
				new_node->passthru_socket=INVALID_SOCKET;
				close_socket(tmp_sock);
				goto NO_PASSTHRU;
			}

			new_node->client_socket_dup=accept(tmp_sock, (struct sockaddr *)&tmp_addr, &tmp_addr_len);

			if(new_node->client_socket_dup == INVALID_SOCKET) {
				lprintf(LOG_ERR,"!ERROR (%d) connecting accept()ing on passthru socket", ERROR_VALUE);
				lprintf(LOG_WARNING,"!WARNING native doors which use sockets will not function");
				close_socket(new_node->passthru_socket);
				new_node->passthru_socket=INVALID_SOCKET;
				close_socket(tmp_sock);
				goto NO_PASSTHRU;
			}
			close_socket(tmp_sock);
			_beginthread(passthru_output_thread, 0, new_node);
			_beginthread(passthru_input_thread, 0, new_node);

NO_PASSTHRU:
			SAFECOPY(new_node->connection,"SSH");
			new_node->node_connection=NODE_CONNECTION_SSH;
			new_node->sys_status|=SS_SSH;
			new_node->telnet_mode|=TELNET_MODE_OFF; // SSH does not use Telnet commands
			new_node->ssh_session=sbbs->ssh_session;
			/* Wait for pending data to be sent then turn off ssh_mode for uber-output */
			while(RingBufFull(&sbbs->outbuf))
				SLEEP(1);
			cryptPopData(sbbs->ssh_session, str, sizeof(str), &i);
			sbbs->ssh_mode=false;
		}
#endif

	    node_threads_running++;
		new_node->input_thread=(HANDLE)_beginthread(input_thread,0, new_node);
		_beginthread(output_thread, 0, new_node);
		_beginthread(node_thread, 0, new_node);
		served++;
	}

    // Close all open sockets
    for(i=0;i<MAX_NODES;i++)  {
    	if(node_socket[i]!=INVALID_SOCKET) {
        	lprintf(LOG_INFO,"Closing node %d socket %d", i+1, node_socket[i]);
        	close_socket(node_socket[i]);
			node_socket[i]=INVALID_SOCKET;
        }
#ifdef __unix__
		if(uspy_listen_socket[i]!=INVALID_SOCKET) {
			close_socket(uspy_listen_socket[i]);
			uspy_listen_socket[i]=INVALID_SOCKET;
			snprintf(str,sizeof(uspy_addr.sun_path),"%slocalspy%d.sock", startup->temp_dir, i+1);
			if(fexist(str))
				unlink(str);
		}
		if(uspy_socket[i]!=INVALID_SOCKET) {
			close_socket(uspy_socket[i]);
			uspy_socket[i]=INVALID_SOCKET;
		}		
#endif
	}

	sbbs->client_socket=INVALID_SOCKET;
	if(events!=NULL)
		events->terminated=true;
    // Wake-up BBS output thread so it can terminate
    sem_post(&sbbs->outbuf.sem);

    // Wait for all node threads to terminate
	if(node_threads_running) {
		lprintf(LOG_INFO,"Waiting for %d node threads to terminate...", node_threads_running);
		start=time(NULL);
		while(node_threads_running) {
			if(time(NULL)-start>TIMEOUT_THREAD_WAIT) {
				lprintf(LOG_ERR,"!TIMEOUT waiting for %d node thread(s) to "
            		"terminate", node_threads_running);
				break;
			}
			mswait(100);
		}
	}

	// Wait for Events thread to terminate
	if(events!=NULL && events->event_thread_running) {
		lprintf(LOG_INFO,"Waiting for event thread to terminate...");
		start=time(NULL);
		while(events->event_thread_running) {
#if 0 /* the event thread can/will segfault if it continues to run and dereference sbbs->cfg */
			if(time(NULL)-start>TIMEOUT_THREAD_WAIT) {
				lprintf(LOG_ERR,"!TIMEOUT waiting for BBS event thread to "
            		"terminate");
				break;
			}
#endif
			mswait(100);
		}
	}

    // Wait for BBS output thread to terminate
	if(sbbs->output_thread_running) {
		lprintf(LOG_INFO,"Waiting for system output thread to terminate...");
		start=time(NULL);
		while(sbbs->output_thread_running) {
			if(time(NULL)-start>TIMEOUT_THREAD_WAIT) {
				lprintf(LOG_ERR,"!TIMEOUT waiting for BBS output thread to "
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

	if(events!=NULL && !events->event_thread_running)
		delete events;

    if(!sbbs->output_thread_running)
	    delete sbbs;

	cleanup(0);

	if(!terminate_server) {
		lprintf(LOG_INFO,"Recycling server...");
		mswait(2000);
		if(startup->recycle!=NULL)
			startup->recycle(startup->cbdata);
	}

	} while(!terminate_server);

}



