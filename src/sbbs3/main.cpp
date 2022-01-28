/* Synchronet terminal server thread and related functions */

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
#include "ident.h"
#include "telnet.h"
#include "netwrap.h"
#include "petdefs.h"
#include "js_rtpool.h"
#include "js_request.h"
#include "ssl.h"
#include "ver.h"
#include <multisock.h>
#include <limits.h>		// HOST_NAME_MAX

#ifdef __unix__
	#include <sys/un.h>
#endif

//#define SBBS_TELNET_ENVIRON_SUPPORT 1
//---------------------------------------------------------------------------

#define TELNET_SERVER "Synchronet Terminal Server"
#define STATUS_WFC	"Listening"

#define TIMEOUT_THREAD_WAIT		60			// Seconds (was 15)
#define IO_THREAD_BUF_SIZE	   	20000		// Bytes
#define TIMEOUT_MUTEX_FILE		12*60*60

#ifdef USE_CRYPTLIB

	static	protected_uint32_t	ssh_sessions;

	void ssh_session_destroy(SOCKET sock, CRYPT_SESSION session, int line)
	{
		int result = cryptDestroySession(session);

		if(result != 0)
			lprintf(LOG_ERR, "%04d SSH Error %d destroying Cryptlib Session %d from line %d"
				, sock, result, session, line);
		else {
			uint32_t remain = protected_uint32_adjust_fetch(&ssh_sessions, -1);
			lprintf(LOG_DEBUG, "%04d SSH Cryptlib Session: %d destroyed from line %d (%u remain)"
				, sock, session, line, remain);
		}
	}

	#define SSH_END(sock) do {										\
		if(ssh) {													\
			pthread_mutex_lock(&sbbs->ssh_mutex);					\
			ssh_session_destroy(sock, sbbs->ssh_session, __LINE__);	\
			sbbs->ssh_mode = false;									\
			pthread_mutex_unlock(&sbbs->ssh_mutex);					\
		}															\
	} while(0)
#else
	#define	SSH_END(x)
#endif

volatile time_t	uptime=0;
volatile ulong	served=0;

static	protected_uint32_t node_threads_running;

char 	lastuseron[LEN_ALIAS+1];  /* Name of user last online */
RingBuf* node_inbuf[MAX_NODES];
SOCKET	spy_socket[MAX_NODES];
#ifdef __unix__
SOCKET	uspy_socket[MAX_NODES];	  /* UNIX domain spy sockets */
#endif
SOCKET	node_socket[MAX_NODES];
struct xpms_set				*ts_set;
static	sbbs_t*	sbbs=NULL;
static	scfg_t	scfg;
static	char *	text[TOTAL_TEXT];
static	scfg_t	node_scfg[MAX_NODES];
static	char *	node_text[MAX_NODES][TOTAL_TEXT];
static	WORD	first_node;
static	WORD	last_node;
static	bool	terminate_server=false;
static	str_list_t recycle_semfiles;
static	str_list_t shutdown_semfiles;
static	str_list_t clear_attempts_semfiles;
static	link_list_t current_logins;
static	link_list_t current_connections;
#ifdef _THREAD_SUID_BROKEN
int	thread_suid_broken=TRUE;			/* NPTL is no longer broken */
#endif

/* convenient space-saving global variables */
extern "C" {
const char* crlf="\r\n";
const char* nulstr="";
};

#define GCES(status, node, sess, action) do {                          \
	char *GCES_estr;                                                    \
	int GCES_level;                                                      \
	get_crypt_error_string(status, sess, &GCES_estr, action, &GCES_level);\
	if (GCES_estr) {                                                       \
		lprintf(GCES_level, "Node %d SSH %s from %s", node, GCES_estr, __FUNCTION__);             \
		free_crypt_attrstr(GCES_estr);                                       \
	}                                                                         \
} while (0)

#define GCESNN(status, sess, action) do {                              \
	char *GCES_estr;                                                    \
	int GCES_level;                                                      \
	get_crypt_error_string(status, sess, &GCES_estr, action, &GCES_level);\
	if (GCES_estr) {                                                       \
		lprintf(GCES_level, "SSH %s from %s", GCES_estr, __FUNCTION__);     \
		free_crypt_attrstr(GCES_estr);                                       \
	}                                                                         \
} while (0)

#define GCESS(status, sock, sess, action) do {                         \
	char *GCES_estr;                                                    \
	int GCES_level;                                                      \
	get_crypt_error_string(status, sess, &GCES_estr, action, &GCES_level);\
	if (GCES_estr) {                                                       \
		lprintf(GCES_level, "%04d SSH %s from %s", sock, GCES_estr, __FUNCTION__);                \
		free_crypt_attrstr(GCES_estr);                                       \
	}                                                                         \
} while (0)

#define GCESSTR(status, str, log_level, sess, action) do {                         \
	char *GCES_estr;                                                    \
	int GCES_level;                                                      \
	get_crypt_error_string(status, sess, &GCES_estr, action, &GCES_level);\
	if (GCES_estr) {                                                       \
		lprintf(log_level, "%s SSH %s from %s (session %d)", str, GCES_estr, __FUNCTION__, sess);                \
		free_crypt_attrstr(GCES_estr);                                       \
	}                                                                         \
} while (0)


extern "C" {

static bbs_startup_t* startup=NULL;

static const char* status(const char* str)
{
	if(startup!=NULL && startup->status!=NULL)
		startup->status(startup->cbdata,str);
	return str;
}

static void update_clients()
{
	if(startup!=NULL && startup->clients!=NULL)
		startup->clients(startup->cbdata,protected_uint32_value(node_threads_running));
}

void client_on(SOCKET sock, client_t* client, BOOL update)
{
	if(!update)
		listAddNodeData(&current_connections, client->addr, strlen(client->addr)+1, sock, LAST_NODE);
	if(startup!=NULL && startup->client_on!=NULL)
		startup->client_on(startup->cbdata,TRUE,sock,client,update);
}

static void client_off(SOCKET sock)
{
	listRemoveTaggedNode(&current_connections, sock, /* free_data */TRUE);
	if(startup!=NULL && startup->client_on!=NULL)
		startup->client_on(startup->cbdata,FALSE,sock,NULL,FALSE);
}

static void thread_up(BOOL setuid)
{
	if(startup!=NULL && startup->thread_up!=NULL)
		startup->thread_up(startup->cbdata,TRUE,setuid);
}

static void thread_down()
{
	if(startup!=NULL && startup->thread_up!=NULL)
		startup->thread_up(startup->cbdata,FALSE,FALSE);
}

int lputs(int level, const char* str)
{
	if(level <= LOG_ERR) {
		char errmsg[1024];
		SAFEPRINTF(errmsg, "term %s", str);
		errorlog(&scfg, level, startup==NULL ? NULL:startup->host_name, errmsg);
		if(startup!=NULL && startup->errormsg!=NULL)
			startup->errormsg(startup->cbdata,level,errmsg);
	}

	if(startup==NULL || startup->lputs==NULL || str==NULL || level > startup->log_level)
    	return(0);

#if defined(_WIN32)
	if(IsBadCodePtr((FARPROC)startup->lputs))
		return(0);
#endif

    return(startup->lputs(startup->cbdata,level,str));
}

int eputs(int level, const char *str)
{
	if(*str == 0)
		return 0;

	if(level <= LOG_ERR) {
		char errmsg[1024];
		SAFEPRINTF(errmsg, "evnt %s", str);
		errorlog(&scfg, level, startup==NULL ? NULL:startup->host_name, errmsg);
		if(startup!=NULL && startup->errormsg!=NULL)
			startup->errormsg(startup->cbdata, level, errmsg);
	}

	if(startup==NULL || startup->event_lputs==NULL || level > startup->log_level)
		return(0);

	return(startup->event_lputs(startup->event_cbdata,level,str));
}

int lprintf(int level, const char *fmt, ...)
{
	va_list argptr;
	char sbuf[1024];

	va_start(argptr,fmt);
	vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
	va_end(argptr);
	return(lputs(level,sbuf));
}

/* Picks the right log callback function (event or term) based on the sbbs->cfg.node_num value */
/* Prepends the current node number and user alias (if applicable) */
int sbbs_t::lputs(int level, const char* str)
{
	char msg[2048];
	char prefix[32] = "";
	char user_str[64] = "";

	if(is_event_thread && event_code != NULL && *event_code)
		SAFEPRINTF(prefix, "%s ", event_code);
	else if(cfg.node_num && !is_event_thread)
		SAFEPRINTF(prefix, "Node %d ", cfg.node_num);
	else if(client_name[0])
		SAFEPRINTF(prefix, "%s ", client_name);
	if(useron.number)
		SAFEPRINTF(user_str, "<%s> ", useron.alias);
	SAFEPRINTF3(msg, "%s%s%s", prefix, user_str, str);
	strip_ctrl(msg, msg);
	if(is_event_thread)
		return ::eputs(level, msg);
	return ::lputs(level, msg);
}

int sbbs_t::lprintf(int level, const char *fmt, ...)
{
	va_list argptr;
	char sbuf[1024];

    va_start(argptr,fmt);
    vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
    va_end(argptr);
    return(lputs(level,sbuf));
}

struct main_sock_cb_data {
	bbs_startup_t	*startup;
	const char		*protocol;
};

void sock_cb(SOCKET sock, void *cb_data)
{
	char	error_str[256];
	struct main_sock_cb_data *cb=(struct main_sock_cb_data *)cb_data;

	if(cb->startup && cb->startup->socket_open)
		cb->startup->socket_open(cb->startup->cbdata, TRUE);
	if(set_socket_options(&scfg, sock, cb->protocol, error_str, sizeof(error_str)))
		lprintf(LOG_ERR,"%04d !ERROR %s",sock,error_str);
}

void sock_close_cb(SOCKET sock, void *cb_data)
{
	bbs_startup_t	*su=(bbs_startup_t *)cb_data;

	if(su && su->socket_open)
		su->socket_open(su->cbdata, FALSE);
}

void call_socket_open_callback(BOOL open)
{
	if(startup!=NULL && startup->socket_open!=NULL)
		startup->socket_open(startup->cbdata, open);
}

SOCKET open_socket(int domain, int type, const char* protocol)
{
	SOCKET	sock;
	char	error[256];

	sock=socket(domain, type, IPPROTO_IP);
	if(sock!=INVALID_SOCKET)
		call_socket_open_callback(TRUE);
	if(sock!=INVALID_SOCKET && set_socket_options(&scfg, sock, protocol, error, sizeof(error)))
		lprintf(LOG_ERR,"%04d !ERROR %s",sock,error);

	return(sock);
}

// Used by sbbs_t::ftp_put() and js_accept()
SOCKET accept_socket(SOCKET s, union xp_sockaddr* addr, socklen_t* addrlen)
{
	SOCKET	sock;

	sock=accept(s,&addr->addr,addrlen);
	if(sock!=INVALID_SOCKET)
		call_socket_open_callback(TRUE);

	return(sock);
}

int close_socket(SOCKET sock)
{
	int		result;

	if(sock==INVALID_SOCKET || sock==0)
		return(0);

	shutdown(sock,SHUT_RDWR);	/* required on Unix */
	result=closesocket(sock);
	call_socket_open_callback(FALSE);
	if(result!=0 && ERROR_VALUE!=ENOTSOCK)
		lprintf(LOG_WARNING,"!ERROR %d closing socket %d",ERROR_VALUE,sock);
	return(result);
}

/* TODO: IPv6 */
u_long resolve_ip(char *addr)
{
	HOSTENT*	host;
	char*		p;

	if(*addr==0)
		return((u_long)INADDR_NONE);

	for(p=addr;*p;p++)
		if(*p!='.' && !IS_DIGIT(*p))
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
		lprintf(LOG_DEBUG,"%s %s",WSAData.szDescription, WSAData.szSystemStatus);
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

DLLEXPORT void sbbs_srand()
{
	DWORD seed;

	xp_randomize();
#if defined(HAS_DEV_RANDOM) && defined(RANDOM_DEV)
	int     rf,rd=0;

	if((rf=open(RANDOM_DEV, O_RDONLY|O_NONBLOCK))!=-1) {
		rd=read(rf, &seed, sizeof(seed));
		close(rf);
	}
	if (rd != sizeof(seed))
#endif
		seed = time32(NULL) ^ (uintmax_t)GetCurrentThreadId();

 	srand(seed);
	sbbs_random(10);	/* Throw away first number */
}

int sbbs_random(int n)
{
	return(xp_random(n));
}

#ifdef JAVASCRIPT

static js_server_props_t js_server_props;

void* js_GetClassPrivate(JSContext *cx, JSObject *obj, JSClass* cls)
{
	void *ret = JS_GetInstancePrivate(cx, obj, cls, NULL);

	/*
	 * NOTE: Any changes here should also be added to the same function in jsdoor.c
	 *       (ie: anything not Synchronet specific).
	 */
	if(ret == NULL)
		JS_ReportError(cx, "'%s' instance: No Private Data or Class Mismatch"
			, cls == NULL ? "???" : cls->name);
	return ret;
}

JSBool
js_CreateArrayOfStrings(JSContext* cx, JSObject* parent, const char* name, const char* str[],uintN flags)
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
js_DescribeSyncObject(JSContext* cx, JSObject* obj, const char* str, int ver)
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
js_DescribeSyncConstructor(JSContext* cx, JSObject* obj, const char* str)
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
	"null",
	"xml",
	"array",
	"alias",
	"undefined"
};

JSBool
js_DefineSyncProperties(JSContext *cx, JSObject *obj, jsSyncPropertySpec* props)
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
		if (props[i].tinyid < 256 && props[i].tinyid > -129) {
			if(!JS_DefinePropertyWithTinyId(cx, obj, /* Never reserve any "slots" for properties */
			    props[i].name,props[i].tinyid, JSVAL_VOID, NULL, NULL, props[i].flags|JSPROP_SHARED))
				return(JS_FALSE);
		}
		else {
			if(!JS_DefineProperty(cx, obj, props[i].name, JSVAL_VOID, NULL, NULL, props[i].flags|JSPROP_SHARED))
				return(JS_FALSE);
		}
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
js_DefineSyncMethods(JSContext* cx, JSObject* obj, jsSyncMethodSpec *funcs)
{
	int			i;
	jsuint		len=0;
	long		ver;
	jsval		val;
	JSObject*	method;
	JSObject*	method_array;
	JSString*	js_str;
	size_t		str_len=0;
	char		*str=NULL;

	/* Return existing method_list array if it's already been created */
	if(JS_GetProperty(cx,obj,method_array_name,&val) && val!=JSVAL_VOID) {
		method_array=JSVAL_TO_OBJECT(val);
		// If the first item is already in the list, don't do anything.
		if(!JS_GetArrayLength(cx, method_array, &len))
			return(JS_FALSE);
		for(i=0; i<(int)len; i++) {
			if(JS_GetElement(cx, method_array, i, &val)!=JS_TRUE || val == JSVAL_VOID)
				continue;
			JS_GetProperty(cx, JSVAL_TO_OBJECT(val), "name", &val);
			JSVALUE_TO_RASTRING(cx, val, str, &str_len, NULL);
			if(str==NULL)
				continue;
			if(strcmp(str, funcs[0].name)==0)
				return(JS_TRUE);
		}
	}
	else {
		if((method_array=JS_NewArrayObject(cx, 0, NULL))==NULL)
			return(JS_FALSE);
		if(!JS_DefineProperty(cx, obj, method_array_name, OBJECT_TO_JSVAL(method_array)
				, NULL, NULL, 0))
			return(JS_FALSE);
	}

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
js_SyncResolve(JSContext* cx, JSObject* obj, char *name, jsSyncPropertySpec* props, jsSyncMethodSpec* funcs, jsConstIntSpec* consts, int flags)
{
	JSBool	ret=JS_TRUE;

	if(props) {
		if(!js_DefineSyncProperties(cx, obj, props))
			ret=JS_FALSE;
	}

	if(funcs) {
		if(!js_DefineSyncMethods(cx, obj, funcs))
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
js_DefineSyncProperties(JSContext *cx, JSObject *obj, jsSyncPropertySpec* props)
{
	uint i;

	/*
	 * NOTE: Any changes here should also be added to the same function in jsdoor.c
	 *       (ie: anything not Synchronet specific).
	 */
	for(i=0;props[i].name;i++) {
		if (props[i].tinyid < 256 && props[i].tinyid > -129) {
			if(!JS_DefinePropertyWithTinyId(cx, obj,
			    props[i].name,props[i].tinyid, JSVAL_VOID, NULL, NULL, props[i].flags|JSPROP_SHARED))
				return(JS_FALSE);
		}
		else {
			if(!JS_DefineProperty(cx, obj, props[i].name, JSVAL_VOID, NULL, NULL, props[i].flags|JSPROP_SHARED))
				return(JS_FALSE);
		}
	}

	return(JS_TRUE);
}


JSBool
js_DefineSyncMethods(JSContext* cx, JSObject* obj, jsSyncMethodSpec *funcs)
{
	uint i;

	/*
	 * NOTE: Any changes here should also be added to the same function in jsdoor.c
	 *       (ie: anything not Synchronet specific).
	 */
	for(i=0;funcs[i].name;i++)
		if(!JS_DefineFunction(cx, obj, funcs[i].name, funcs[i].call, funcs[i].nargs, 0))
			return(JS_FALSE);
	return(JS_TRUE);
}

JSBool
js_SyncResolve(JSContext* cx, JSObject* obj, char *name, jsSyncPropertySpec* props, jsSyncMethodSpec* funcs, jsConstIntSpec* consts, int flags)
{
	uint i;
	jsval	val;

	/*
	 * NOTE: Any changes here should also be added to the same function in jsdoor.c
	 *       (ie: anything not Synchronet specific).
	 */
	if(props) {
		for(i=0;props[i].name;i++) {
			if(name==NULL || strcmp(name, props[i].name)==0) {
				if (props[i].tinyid < 256 && props[i].tinyid > -129) {
					if(!JS_DefinePropertyWithTinyId(cx, obj,
					    props[i].name,props[i].tinyid, JSVAL_VOID, NULL, NULL, props[i].flags|JSPROP_SHARED))
						return(JS_FALSE);
				}
				else {
					if(!JS_DefineProperty(cx, obj, props[i].name, JSVAL_VOID, NULL, NULL, props[i].flags|JSPROP_SHARED))
						return(JS_FALSE);
				}
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
				val=INT_TO_JSVAL(consts[i].val);

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
js_DefineConstIntegers(JSContext* cx, JSObject* obj, jsConstIntSpec* ints, int flags)
{
	uint	i;
	jsval	val;

	for(i=0;ints[i].name;i++) {
		val=INT_TO_JSVAL(ints[i].val);

		if(!JS_DefineProperty(cx, obj, ints[i].name, val ,NULL, NULL, flags))
			return(JS_FALSE);
	}

	return(JS_TRUE);
}

static JSBool
js_log(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
    uintN		i=0;
	int32		level=LOG_INFO;
    JSString*	str=NULL;
	sbbs_t*		sbbs;
	jsrefcount	rc;
	char		*line=NULL;
	size_t		line_sz=0;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc > 1 && JSVAL_IS_NUMBER(argv[i])) {
		if(!JS_ValueToInt32(cx,argv[i++],&level))
			return JS_FALSE;
	}

    for(; i<argc; i++) {
		if((str=JS_ValueToString(cx, argv[i]))==NULL) {
			FREE_AND_NULL(line);
			return(JS_FALSE);
		}
		JSSTRING_TO_RASTRING(cx, str, line, &line_sz, NULL);
		if(line==NULL)
		    return(JS_FALSE);
		rc=JS_SUSPENDREQUEST(cx);
		sbbs->lputs(level, line);
		JS_RESUMEREQUEST(cx, rc);
	}
	if(line != NULL)
		free(line);

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
	uchar*		buf;
	int32		len=128;
	sbbs_t*		sbbs;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc) {
		if(!JS_ValueToInt32(cx,argv[0],&len))
			return JS_FALSE;
	}

	if((buf=(uchar*)malloc(len))==NULL)
		return(JS_TRUE);

	rc=JS_SUSPENDREQUEST(cx);
	len=RingBufRead(&sbbs->inbuf,buf,len);
	JS_RESUMEREQUEST(cx, rc);

	if(len>0)
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(JS_NewStringCopyN(cx,(char*)buf,len)));

	free(buf);
	return(JS_TRUE);
}

static JSBool
js_readln(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char*		buf;
	int32		len=128;
	sbbs_t*		sbbs;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(argc) {
		if(!JS_ValueToInt32(cx,argv[0],&len))
			return JS_FALSE;
	}

	if((buf=(char*)malloc(len))==NULL)
		return(JS_TRUE);

	rc=JS_SUSPENDREQUEST(cx);
	len=sbbs->getstr(buf,len,K_NONE);
	JS_RESUMEREQUEST(cx, rc);

	if(len>0)
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(JS_NewStringCopyZ(cx,buf)));

	free(buf);
	return(JS_TRUE);
}

static JSBool
js_write(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
    uintN		i;
    JSString*	str=NULL;
	sbbs_t*		sbbs;
	jsrefcount	rc;
	char		*cstr=NULL;
	size_t		cstr_sz=0;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

    for (i = 0; i < argc; i++) {
		if((str=JS_ValueToString(cx, argv[i]))==NULL) {
			FREE_AND_NULL(cstr);
			return(JS_FALSE);
		}
		JSSTRING_TO_RASTRING(cx, str, cstr, &cstr_sz, NULL);
		if(cstr==NULL)
		    return(JS_FALSE);
		rc=JS_SUSPENDREQUEST(cx);
		if(sbbs->online != ON_REMOTE)
			sbbs->lputs(LOG_INFO, cstr);
		else
			sbbs->bputs(cstr);
		JS_RESUMEREQUEST(cx, rc);
	}
	FREE_AND_NULL(cstr);

	if(str==NULL)
		JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	else
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(str));
    return(JS_TRUE);
}

static JSBool
js_write_raw(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
    uintN		i;
    char*		str=NULL;
    size_t		str_sz=0;
	size_t		len;
	sbbs_t*		sbbs;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

    for (i = 0; i < argc; i++) {
		JSVALUE_TO_RASTRING(cx, argv[i], str, &str_sz, &len);
		if(str==NULL)
		    return(JS_FALSE);
		if(len < 1)
			continue;
		rc=JS_SUSPENDREQUEST(cx);
		sbbs->putcom(str, len);
		JS_RESUMEREQUEST(cx, rc);
	}
	if (str != NULL)
		free(str);

    return(JS_TRUE);
}

static JSBool
js_writeln(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*		sbbs;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	js_write(cx,argc,arglist);
	rc=JS_SUSPENDREQUEST(cx);
	if(sbbs->online==ON_REMOTE)
		sbbs->bputs(crlf);
	JS_RESUMEREQUEST(cx, rc);

    return(JS_TRUE);
}

static JSBool
js_printf(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char*		p;
	sbbs_t*		sbbs;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if((p = js_sprintf(cx, 0, argc, argv))==NULL) {
		JS_ReportError(cx,"js_sprintf failed");
		return(JS_FALSE);
	}

	rc=JS_SUSPENDREQUEST(cx);
	if(sbbs->online != ON_REMOTE)
		sbbs->lputs(LOG_INFO, p);
	else
		sbbs->bputs(p);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, p)));

	js_sprintf_free(p);

    return(JS_TRUE);
}

static JSBool
js_alert(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	sbbs_t*		sbbs;
	jsrefcount	rc;
	char		*cstr;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	JSVALUE_TO_MSTRING(cx, argv[0], cstr, NULL);
	if(cstr==NULL)
	    return(JS_FALSE);

	rc=JS_SUSPENDREQUEST(cx);
	if(sbbs->online != ON_REMOTE)
		sbbs->lputs(LOG_WARNING, cstr);
	else {
		sbbs->attr(sbbs->cfg.color[clr_err]);
		sbbs->bputs(cstr);
		sbbs->attr(LIGHTGRAY);
		sbbs->bputs(crlf);
	}
	free(cstr);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, argv[0]);

    return(JS_TRUE);
}

static JSBool
js_confirm(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	sbbs_t*		sbbs;
	jsrefcount	rc;
	char		*cstr;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	JSVALUE_TO_MSTRING(cx, argv[0], cstr, NULL);
	if(cstr==NULL)
	    return(JS_FALSE);

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->yesno(cstr)));
	free(cstr);
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_deny(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	sbbs_t*		sbbs;
	jsrefcount	rc;
	char		*cstr;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	JSVALUE_TO_MSTRING(cx, argv[0], cstr, NULL);
	if(cstr==NULL)
	    return(JS_FALSE);

	rc=JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->noyes(cstr)));
	free(cstr);
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}


static JSBool
js_prompt(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char		instr[128] = "";
    JSString *	str;
	sbbs_t*		sbbs;
	jsrefcount	rc;
    char*		prompt=NULL;
	int32		mode = K_EDIT;
	size_t		result;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	uintN argn = 0;
	if(argc > argn && JSVAL_IS_STRING(argv[argn])) {
		JSVALUE_TO_MSTRING(cx, argv[argn], prompt, NULL);
		if(prompt==NULL)
			return(JS_FALSE);
		argn++;
	}
	if(argc > argn && JSVAL_IS_STRING(argv[argn])) {
		JSVALUE_TO_STRBUF(cx, argv[argn], instr, sizeof(instr), NULL);
		argn++;
	}
	if(argc > argn && JSVAL_IS_NUMBER(argv[argn])) {
		if(!JS_ValueToInt32(cx,argv[argn], &mode)) {
			free(prompt);
			return JS_FALSE;
		}
		argn++;
	}

	rc=JS_SUSPENDREQUEST(cx);
	if(prompt != NULL) {
		sbbs->bprintf("\1n\1y\1h%s\1w: ",prompt);
		free(prompt);
	}

	result = sbbs->getstr(instr, sizeof(instr)-1, mode);
	sbbs->attr(LIGHTGRAY);
	if(!result) {
		JS_SET_RVAL(cx, arglist, JSVAL_NULL);
		JS_RESUMEREQUEST(cx, rc);
		return(JS_TRUE);
	}
	JS_RESUMEREQUEST(cx, rc);

	if((str=JS_NewStringCopyZ(cx, instr))==NULL)
	    return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(str));
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
	{"prompt",			js_prompt,			1,	JSTYPE_STRING,	JSDOCSTR("[text] [,value] [,mode=K_EDIT]")
	,JSDOCSTR("displays a prompt (<i>text</i>) and returns a string of user input (ala client-side JS)")
	,310
	},
	{"confirm",			js_confirm,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("value")
	,JSDOCSTR("displays a Yes/No prompt and returns <i>true</i> or <i>false</i> "
		"based on user's confirmation (ala client-side JS, <i>true</i> = yes)")
	,310
	},
	{"deny",			js_deny,			1,	JSTYPE_BOOLEAN,	JSDOCSTR("value")
	,JSDOCSTR("displays a No/Yes prompt and returns <i>true</i> or <i>false</i> "
		"based on user's denial (<i>true</i> = no)")
	,31501
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
	jsrefcount	rc;
	int		log_level;

	if((sbbs=(sbbs_t*)JS_GetContextPrivate(cx))==NULL)
		return;

	if(report==NULL) {
		sbbs->lprintf(LOG_ERR,"!JavaScript: %s", message);
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
			warning="strict warning ";
		else
			warning="warning ";
		log_level = LOG_WARNING;
	} else {
		warning=nulstr;
		log_level = LOG_ERR;
	}

	rc=JS_SUSPENDREQUEST(cx);
	sbbs->lprintf(log_level, "!JavaScript %s%s%s: %s",warning,file,line,message);
	if(sbbs->online==ON_REMOTE)
		sbbs->bprintf("!JavaScript %s%s%s: %s\r\n",warning,getfname(file),line,message);
	JS_RESUMEREQUEST(cx, rc);
}

JSContext* sbbs_t::js_init(JSRuntime** runtime, JSObject** glob, const char* desc)
{
	JSContext* js_cx;

	if(startup->js.max_bytes==0)			startup->js.max_bytes=JAVASCRIPT_MAX_BYTES;

	lprintf(LOG_DEBUG,"JavaScript: Creating %s runtime: %lu bytes"
		,desc, startup->js.max_bytes);
	if((*runtime = jsrt_GetNew(startup->js.max_bytes, 1000, __FILE__, __LINE__)) == NULL)
		return NULL;

    if((js_cx = JS_NewContext(*runtime, JAVASCRIPT_CONTEXT_STACK))==NULL)
		return NULL;
	JS_SetOptions(js_cx, startup->js.options);
	JS_BEGINREQUEST(js_cx);

	memset(&js_callback,0,sizeof(js_callback));
	js_callback.limit = startup->js.time_limit;
	js_callback.gc_interval = startup->js.gc_interval;
	js_callback.yield_interval = startup->js.yield_interval;
	js_callback.terminated = &terminated;
	js_callback.auto_terminate = TRUE;
	js_callback.events_supported = TRUE;

	bool success=false;
	bool rooted=false;

	do {

		JS_SetErrorReporter(js_cx, js_ErrorReporter);

		JS_SetContextPrivate(js_cx, this);	/* Store a pointer to sbbs_t instance */

		/* Global Objects (including system, js, client, Socket, MsgBase, File, User, etc. */
		if(!js_CreateCommonObjects(js_cx, &scfg, &cfg, js_global_functions
					,uptime, server_host_name(), SOCKLIB_DESC	/* system */
					,&js_callback								/* js */
					,&startup->js
					,client_socket == INVALID_SOCKET ? NULL : &client, client_socket, -1 /* client */
					,&js_server_props							/* server */
					,glob
			))
			break;
		rooted=true;

#ifdef BUILD_JSDOCS
		js_CreateUifcObject(js_cx, *glob);
		js_CreateConioObject(js_cx, *glob);
#endif

		/* BBS Object */
		if(js_CreateBbsObject(js_cx, *glob)==NULL)
			break;

		/* Console Object */
		if(js_CreateConsoleObject(js_cx, *glob)==NULL)
			break;

		success=true;

	} while(0);

	if(!success) {
		if(rooted)
			JS_RemoveObjectRoot(js_cx, glob);
		JS_ENDREQUEST(js_cx);
		JS_DestroyContext(js_cx);
		js_cx=NULL;
		return NULL;
	}
	else
		JS_ENDREQUEST(js_cx);

	return js_cx;
}

void sbbs_t::js_cleanup(void)
{
	/* Free Context */
	if(js_cx!=NULL) {
		lprintf(LOG_DEBUG,"JavaScript: Destroying context");
		JS_BEGINREQUEST(js_cx);
		JS_RemoveObjectRoot(js_cx, &js_glob);
		JS_ENDREQUEST(js_cx);
		JS_DestroyContext(js_cx);
		js_cx=NULL;
	}

	if(js_runtime!=NULL) {
		lprintf(LOG_DEBUG,"JavaScript: Destroying runtime");
		jsrt_Release(js_runtime);
		js_runtime=NULL;
	}

	if(js_hotkey_cx!=NULL) {
		lprintf(LOG_DEBUG,"JavaScript: Destroying HotKey context");
		JS_BEGINREQUEST(js_hotkey_cx);
		JS_RemoveObjectRoot(js_hotkey_cx, &js_hotkey_glob);
		JS_ENDREQUEST(js_hotkey_cx);
		JS_DestroyContext(js_hotkey_cx);
		js_hotkey_cx=NULL;
	}

	if(js_hotkey_runtime!=NULL) {
		lprintf(LOG_DEBUG,"JavaScript: Destroying HotKey runtime");
		jsrt_Release(js_hotkey_runtime);
		js_hotkey_runtime=NULL;
	}

}

bool sbbs_t::js_create_user_objects(JSContext* cx, JSObject* glob)
{
	bool result = false;
	if(cx != NULL) {
		JS_BEGINREQUEST(cx);
		if(!js_CreateUserObjects(cx, glob, &cfg, &useron, &client, startup->web_file_vpath_prefix, subscan))
			lprintf(LOG_ERR,"!JavaScript ERROR creating user objects");
		else
			result = true;
		JS_ENDREQUEST(cx);
	}
	return result;
}

extern "C" BOOL js_CreateCommonObjects(JSContext* js_cx
										,scfg_t* cfg				/* common */
										,scfg_t* node_cfg			/* node-specific */
										,jsSyncMethodSpec* methods	/* global */
										,time_t uptime				/* system */
										,char* host_name			/* system */
										,char* socklib_desc			/* system */
										,js_callback_t* cb			/* js */
										,js_startup_t* js_startup	/* js */
										,client_t* client			/* client */
										,SOCKET client_socket		/* client */
										,CRYPT_CONTEXT session		/* client */
										,js_server_props_t* props	/* server */
										,JSObject** glob
										)
{
	BOOL	success=FALSE;

	if(node_cfg==NULL)
		node_cfg=cfg;

	/* Global Object */
	if(!js_CreateGlobalObject(js_cx, node_cfg, methods, js_startup, glob))
		return(FALSE);

	do {
		/*
		 * NOTE: Where applicable, anything added here should also be added to
		 *       the same function in jsdoor.c (ie: anything not Synchronet
		 *       specific).
		 */

		/* System Object */
		if(js_CreateSystemObject(js_cx, *glob, node_cfg, uptime, host_name, socklib_desc)==NULL)
			break;

		/* Internal JS Object */
		if(cb!=NULL
			&& js_CreateInternalJsObject(js_cx, *glob, cb, js_startup)==NULL)
			break;

		/* Client Object */
		if(client!=NULL
			&& js_CreateClientObject(js_cx, *glob, "client", client, client_socket, session)==NULL)
			break;

		/* Server */
		if(props!=NULL
			&& js_CreateServerObject(js_cx, *glob, props)==NULL)
			break;

		/* Socket Class */
		if(js_CreateSocketClass(js_cx, *glob)==NULL)
			break;

		/* Queue Class */
		if(js_CreateQueueClass(js_cx, *glob)==NULL)
			break;

		/* MsgBase Class */
		if(js_CreateMsgBaseClass(js_cx, *glob, cfg)==NULL)
			break;

		/* FileBase Class */
		if(js_CreateFileBaseClass(js_cx, *glob, node_cfg)==NULL)
			break;

		/* File Class */
		if(js_CreateFileClass(js_cx, *glob)==NULL)
			break;

		/* Archive Class */
		if(js_CreateArchiveClass(js_cx, *glob)==NULL)
			break;

		/* User class */
		if(js_CreateUserClass(js_cx, *glob, cfg)==NULL)
			break;

		/* COM Class */
		if(js_CreateCOMClass(js_cx, *glob)==NULL)
			break;

		/* CryptContext Class */
		if(js_CreateCryptContextClass(js_cx, *glob)==NULL)
			break;

		/* CryptKeyset Class */
		if(js_CreateCryptKeysetClass(js_cx, *glob)==NULL)
			break;

		/* CryptCert Class */
		if(js_CreateCryptCertClass(js_cx, *glob)==NULL)
			break;

		/* Area Objects */
		if(!js_CreateUserObjects(js_cx, *glob, cfg, /* user: */NULL, client, startup->web_file_vpath_prefix, /* subscan: */NULL))
			break;

		success=TRUE;
	} while(0);

	if(!success)
		JS_RemoveObjectRoot(js_cx, glob);

	return(success);
}

#endif	/* JAVASCRIPT */

static BYTE* telnet_interpret(sbbs_t* sbbs, BYTE* inbuf, int inlen,
  									BYTE* outbuf, int& outlen)
{
	BYTE*   first_iac=NULL;
	BYTE*	first_cr=NULL;
	int 	i;

	outlen=0;

	if(inlen<1) {
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

    if(!sbbs->telnet_cmdlen) {
		if(first_iac==NULL && first_cr==NULL) {
			outlen=inlen;
			return(inbuf);	// no interpretation needed
		}

		if(first_iac!=NULL || first_cr!=NULL) {
			if(first_iac!=NULL && (first_cr==NULL || first_iac<first_cr))
   				outlen=first_iac-inbuf;
			else
				outlen=first_cr-inbuf;
			memcpy(outbuf, inbuf, outlen);
		}
	}

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
			else {
				lprintf(LOG_WARNING, "Node %d telnet command (%d, %d) buffer limit reached (%u bytes)"
					,sbbs->cfg.node_num, sbbs->telnet_cmd[1], sbbs->telnet_cmd[2], sbbs->telnet_cmdlen);
				sbbs->telnet_cmdlen = 0;
			}

			uchar command	= sbbs->telnet_cmd[1];
			uchar option	= sbbs->telnet_cmd[2];

			if(sbbs->telnet_cmdlen == 2 && command == TELNET_SE) {
				lprintf(LOG_WARNING, "Node %d unexpected telnet sub-negotiation END command"
					,sbbs->cfg.node_num);
				sbbs->telnet_cmdlen = 0;
			}
			else if(sbbs->telnet_cmdlen>=2 && command==TELNET_SB) {
				if(inbuf[i]==TELNET_SE
					&& sbbs->telnet_cmd[sbbs->telnet_cmdlen-2]==TELNET_IAC) {
					sbbs->telnet_cmds_received++;
					if(startup->options&BBS_OPT_DEBUG_TELNET)
						lprintf(LOG_DEBUG,"Node %d %s telnet sub-negotiation command: %s (%u bytes)"
	                		,sbbs->cfg.node_num
							,sbbs->telnet_mode&TELNET_MODE_GATE ? "passed-through" : "received"
							,telnet_opt_desc(option)
							,sbbs->telnet_cmdlen);

					/* sub-option terminated */
					if(option==TELNET_TERM_TYPE
						&& sbbs->telnet_cmd[3]==TELNET_TERM_IS) {
						safe_snprintf(sbbs->telnet_terminal,sizeof(sbbs->telnet_terminal),"%.*s"
							,(int)sbbs->telnet_cmdlen-6,sbbs->telnet_cmd+4);
						lprintf(LOG_DEBUG,"Node %d %s telnet terminal type: %s"
	                		,sbbs->cfg.node_num
							,sbbs->telnet_mode&TELNET_MODE_GATE ? "passed-through" : "received"
							,sbbs->telnet_terminal);

					} else if(option==TELNET_TERM_SPEED
						&& sbbs->telnet_cmd[3]==TELNET_TERM_IS) {
						char speed[128];
						safe_snprintf(speed,sizeof(speed),"%.*s",(int)sbbs->telnet_cmdlen-6,sbbs->telnet_cmd+4);
						lprintf(LOG_DEBUG,"Node %d %s telnet terminal speed: %s"
	                		,sbbs->cfg.node_num
							,sbbs->telnet_mode&TELNET_MODE_GATE ? "passed-through" : "received"
							,speed);
						sbbs->telnet_speed=atoi(speed);
#ifdef SBBS_TELNET_ENVIRON_SUPPORT
					} else if(option==TELNET_NEW_ENVIRON
						&& sbbs->telnet_cmd[3]==TELNET_ENVIRON_IS) {
						BYTE*	p;
						BYTE*   end=sbbs->telnet_cmd+(sbbs->telnet_cmdlen-2);
						for(p=sbbs->telnet_cmd+4; p < end; ) {
							if(*p==TELNET_ENVIRON_VAR || *p==TELNET_ENVIRON_USERVAR) {
								BYTE type=*p++;
								char* name=(char*)p;
								/* RFC 1572: The characters following a "type" up to the next "type" or VALUE specify the variable name. */
								while(p < end) {
									if(*p==TELNET_ENVIRON_VAR || *p==TELNET_ENVIRON_USERVAR || *p == TELNET_ENVIRON_VALUE)
										break;
									p++;
								}
								if(p < end) {
									char* value=(char*)p+1;
									*(p++)=0;
									while(p < end) {
										if(*p==TELNET_ENVIRON_VAR || *p==TELNET_ENVIRON_USERVAR || *p == TELNET_ENVIRON_VALUE)
											break;
										p++;
									}
									*p=0;
									lprintf(LOG_DEBUG,"Node %d telnet %s %s environment variable '%s' = '%s'"
	                					,sbbs->cfg.node_num
										,sbbs->telnet_mode&TELNET_MODE_GATE ? "passed-through" : "received"
										,type==TELNET_ENVIRON_VAR ? "well-known" : "user-defined"
										,name
										,value);
									if(strcmp(name,"USER") == 0) {
										SAFECOPY(sbbs->rlogin_name, value);
									}
								}
							} else
								p++;
						}
#endif
					} else if(option==TELNET_SEND_LOCATION) {
						safe_snprintf(sbbs->telnet_location
							,sizeof(sbbs->telnet_location)
							,"%.*s",(int)sbbs->telnet_cmdlen-5,sbbs->telnet_cmd+3);
						lprintf(LOG_DEBUG,"Node %d %s telnet location: %s"
	                		,sbbs->cfg.node_num
							,sbbs->telnet_mode&TELNET_MODE_GATE ? "passed-through" : "received"
							,sbbs->telnet_location);
					} else if(option==TELNET_TERM_LOCATION_NUMBER && sbbs->telnet_cmd[3] == 0) {
						SAFEPRINTF4(sbbs->telnet_location, "%u.%u.%u.%u"
							,sbbs->telnet_cmd[4]
							,sbbs->telnet_cmd[5]
							,sbbs->telnet_cmd[6]
							,sbbs->telnet_cmd[7]
						);
						lprintf(LOG_DEBUG,"Node %d %s telnet location number (IP address): %s"
	                		,sbbs->cfg.node_num
							,sbbs->telnet_mode&TELNET_MODE_GATE ? "passed-through" : "received"
							,sbbs->telnet_location);
					} else if(option==TELNET_NEGOTIATE_WINDOW_SIZE) {
						long cols = (sbbs->telnet_cmd[3]<<8) | sbbs->telnet_cmd[4];
						long rows = (sbbs->telnet_cmd[5]<<8) | sbbs->telnet_cmd[6];
						lprintf(LOG_DEBUG,"Node %d %s telnet window size: %ldx%ld"
	                		,sbbs->cfg.node_num
							,sbbs->telnet_mode&TELNET_MODE_GATE ? "passed-through" : "received"
							,cols
							,rows);
						sbbs->telnet_cols = cols;
						sbbs->telnet_rows = rows;
					} else if(startup->options&BBS_OPT_DEBUG_TELNET)
            			lprintf(LOG_DEBUG,"Node %d %s unsupported telnet sub-negotiation cmd: %s, 0x%02X"
	                		,sbbs->cfg.node_num
							,sbbs->telnet_mode&TELNET_MODE_GATE ? "passed-through" : "received"
                			,telnet_opt_desc(option)
							,sbbs->telnet_cmd[3]);
					sbbs->telnet_cmdlen=0;
				}
			} // Sub-negotiation command
            else if(sbbs->telnet_cmdlen==2 && inbuf[i]<TELNET_WILL) {
				sbbs->telnet_cmds_received++;
				if(startup->options&BBS_OPT_DEBUG_TELNET)
            		lprintf(LOG_DEBUG,"Node %d %s telnet cmd: %s"
	                	,sbbs->cfg.node_num
						,sbbs->telnet_mode&TELNET_MODE_GATE ? "passed-through" : "received"
                		,telnet_cmd_desc(option));
                sbbs->telnet_cmdlen=0;
            }
            else if(sbbs->telnet_cmdlen>=3) {	/* telnet option negotiation */
				sbbs->telnet_cmds_received++;
				if(startup->options&BBS_OPT_DEBUG_TELNET)
					lprintf(LOG_DEBUG,"Node %d %s telnet cmd: %s %s"
						,sbbs->cfg.node_num
						,sbbs->telnet_mode&TELNET_MODE_GATE ? "passed-through" : "received"
						,telnet_cmd_desc(command)
						,telnet_opt_desc(option));

				if(!(sbbs->telnet_mode&TELNET_MODE_GATE)) {
					if(command==TELNET_DO || command==TELNET_DONT) {	/* local options */
						if(sbbs->telnet_local_option[option]==command)
							SetEvent(sbbs->telnet_ack_event);
						else {
							sbbs->telnet_local_option[option]=command;
							sbbs->send_telnet_cmd(telnet_opt_ack(command),option);
						}
					} else { /* WILL/WONT (remote options) */
						if(sbbs->telnet_remote_option[option]==command)
							SetEvent(sbbs->telnet_ack_event);
						else {
							switch(option) {
								case TELNET_BINARY_TX:
								case TELNET_ECHO:
								case TELNET_TERM_TYPE:
								case TELNET_TERM_SPEED:
								case TELNET_SUP_GA:
								case TELNET_NEGOTIATE_WINDOW_SIZE:
								case TELNET_SEND_LOCATION:
								case TELNET_TERM_LOCATION_NUMBER:
#ifdef SBBS_TELNET_ENVIRON_SUPPORT
								case TELNET_NEW_ENVIRON:
#endif
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
#ifdef SBBS_TELNET_ENVIRON_SUPPORT
						else if(command==TELNET_WILL && option==TELNET_NEW_ENVIRON) {
							if(startup->options&BBS_OPT_DEBUG_TELNET)
								lprintf(LOG_DEBUG,"Node %d requesting USER environment variable value"
									,sbbs->cfg.node_num);

							char	buf[64];
							int len=sprintf(buf,"%c%c%c%c%c%c"
								,TELNET_IAC,TELNET_SB
								,TELNET_NEW_ENVIRON,TELNET_ENVIRON_SEND //,TELNET_ENVIRON_VAR
								,TELNET_IAC,TELNET_SE);
							sbbs->putcom(buf,len);
						}
#endif
					}
				}
                sbbs->telnet_cmdlen=0;
            }
			if(sbbs->telnet_mode&TELNET_MODE_GATE)	// Pass-through commands
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
            lprintf(LOG_DEBUG,"sending telnet cmd: %s"
                ,telnet_cmd_desc(cmd));
		sprintf(buf,"%c%c",TELNET_IAC,cmd);
		(void)sendsocket(client_socket, buf, 2);
	} else {
		if(startup->options&BBS_OPT_DEBUG_TELNET)
			lprintf(LOG_DEBUG,"sending telnet cmd: %s %s"
				,telnet_cmd_desc(cmd)
				,telnet_opt_desc(opt));
		sprintf(buf,"%c%c%c",TELNET_IAC,cmd,opt);
		(void)sendsocket(client_socket, buf, 3);
	}
}

bool sbbs_t::request_telnet_opt(uchar cmd, uchar opt, unsigned waitforack)
{
	if(telnet_mode&TELNET_MODE_OFF)
		return false;

	if(cmd==TELNET_DO || cmd==TELNET_DONT) {	/* remote option */
		if(telnet_remote_option[opt]==telnet_opt_ack(cmd))
			return true;	/* already set in this mode, do nothing */
		telnet_remote_option[opt]=telnet_opt_ack(cmd);
	} else {	/* local option */
		if(telnet_local_option[opt]==telnet_opt_ack(cmd))
			return true;	/* already set in this mode, do nothing */
		telnet_local_option[opt]=telnet_opt_ack(cmd);
	}
	if(waitforack)
		ResetEvent(telnet_ack_event);
	send_telnet_cmd(cmd,opt);
	if(waitforack)
		return WaitForEvent(telnet_ack_event, waitforack)==WAIT_OBJECT_0;
	return true;
}

static int crypt_pop_channel_data(sbbs_t *sbbs, char *inbuf, int want, int *got)
{
	int status;
	int cid;
	char *cname;
	int ret;
	int closing_channel = -1;

	*got=0;
	while(sbbs->online && sbbs->client_socket!=INVALID_SOCKET
	    && node_socket[sbbs->cfg.node_num-1]!=INVALID_SOCKET) {
		ret = cryptPopData(sbbs->ssh_session, inbuf, want, got);
		if (ret == CRYPT_OK) {
			status = cryptGetAttribute(sbbs->ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, &cid);
			if (status == CRYPT_OK) {
				if (cid == closing_channel)
					continue;
				if (cid != sbbs->session_channel) {
					if (cryptStatusError(status = cryptSetAttribute(sbbs->ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, cid))) {
						GCESS(status, sbbs->client_socket, sbbs->ssh_session, "setting channel");
						return status;
					}
					cname = get_crypt_attribute(sbbs->ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_TYPE);
					lprintf(LOG_WARNING, "Node %d SSH WARNING: attempt to use channel '%s' (%d != %d)"
						, sbbs->cfg.node_num, cname ? cname : "<unknown>", cid, sbbs->session_channel);
					if (cname)
						free_crypt_attrstr(cname);
					closing_channel = cid;
					if (cryptStatusError(status = cryptSetAttribute(sbbs->ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_ACTIVE, 0))) {
						GCESS(status, sbbs->client_socket, sbbs->ssh_session, "closing channel");
						return status;
					}
					continue;
				}
			}
			else {
				GCESS(status, sbbs->client_socket, sbbs->ssh_session, "getting channel id");
			}
		}
		if (ret == CRYPT_ENVELOPE_RESOURCE)
			return CRYPT_ERROR_TIMEOUT;
		return ret;
	}
	return CRYPT_ERROR_TIMEOUT;
}

void input_thread(void *arg)
{
	BYTE		inbuf[4000];
   	BYTE		telbuf[sizeof(inbuf)];
    BYTE		*wrbuf;
    int			i,rd,wr,avail;
	ulong		total_recv=0;
	ulong		total_pkts=0;
	sbbs_t*		sbbs = (sbbs_t*) arg;
	SOCKET sock;
#ifdef PREFER_POLL
	struct pollfd fds[2];
	int nfds;
#endif

	SetThreadName("sbbs/termInput");
	thread_up(TRUE /* setuid */);

#ifdef _DEBUG
	lprintf(LOG_DEBUG,"Node %d input thread started",sbbs->cfg.node_num);
#endif

	sbbs->console|=CON_R_INPUT;

	while(sbbs->online && sbbs->client_socket!=INVALID_SOCKET
		&& node_socket[sbbs->cfg.node_num-1]!=INVALID_SOCKET) {

#ifdef _WIN32	// No spy sockets
		if (!socket_readable(sbbs->client_socket, 1000))
			continue;
#else
#ifdef PREFER_POLL
		fds[0].fd = sbbs->client_socket;
		fds[0].events = POLLIN;
		nfds = 1;
		if (uspy_socket[sbbs->cfg.node_num-1] != INVALID_SOCKET) {
			fds[1].fd = uspy_socket[sbbs->cfg.node_num-1];
			fds[1].events = POLLIN;
			nfds++;
		}

		if (poll(fds, nfds, 1000) < 1)
			continue;
#else
#error Spy sockets without poll() was removed in commit 3971ef4dcc3db19f400a648b6110718e56a64cf3
#endif
#endif

		if(sbbs->client_socket==INVALID_SOCKET)
			break;

/*         ^          ^
 *      \______    ______/
 *       \  * \   / *   /
 *        -----   ------           /----\
 *              ||               -< Boo! |
 *             /__\                \----/
 *       \______________/
 *        \/\/\/\/\/\/\/
 *         ------------
 */

#ifdef _WIN32	// No spy sockets
		sock=sbbs->client_socket;
#else
#ifdef PREFER_POLL
		if (fds[0].revents & POLLIN)
			sock = sbbs->client_socket;
		else if(uspy_socket[sbbs->cfg.node_num - 1] != INVALID_SOCKET && fds[1].revents & POLLIN) {
			if(socket_recvdone(uspy_socket[sbbs->cfg.node_num-1], 0)) {
				close_socket(uspy_socket[sbbs->cfg.node_num-1]);
				lprintf(LOG_NOTICE,"Closing local spy socket: %d",uspy_socket[sbbs->cfg.node_num-1]);
				uspy_socket[sbbs->cfg.node_num-1]=INVALID_SOCKET;
				continue;
			}
			sock=uspy_socket[sbbs->cfg.node_num-1];
		}
		else {
			continue;
		}
#else
#error Spy sockets without poll() was removed in commit 3971ef4dcc3db19f400a648b6110718e56a64cf3
#endif
#endif
    	rd=RingBufFree(&sbbs->inbuf);

		if(rd==0) { // input buffer full
			lprintf(LOG_WARNING,"Node %d !WARNING input buffer full", sbbs->cfg.node_num);
        	// wait up to 5 seconds to empty (1 byte min)
			time_t start=time(NULL);
            while((rd=RingBufFree(&sbbs->inbuf))==0 && time(NULL)-start<5) {
                YIELD();
            }
			if(rd==0)	/* input buffer still full */
				continue;
		}

	    if(rd > (int)sizeof(inbuf))
        	rd=sizeof(inbuf);

		if(pthread_mutex_lock(&sbbs->input_thread_mutex)!=0)
			sbbs->errormsg(WHERE,ERR_LOCK,"input_thread_mutex",0);

#ifdef USE_CRYPTLIB
		if(sbbs->ssh_mode && sock==sbbs->client_socket) {
			int err;
			pthread_mutex_lock(&sbbs->ssh_mutex);
			if(cryptStatusError((err=crypt_pop_channel_data(sbbs, (char*)inbuf, rd, &i)))) {
				pthread_mutex_unlock(&sbbs->ssh_mutex);
				if(pthread_mutex_unlock(&sbbs->input_thread_mutex)!=0)
					sbbs->errormsg(WHERE,ERR_UNLOCK,"input_thread_mutex",0);
				if(err==CRYPT_ERROR_TIMEOUT)
					continue;
				/* Handle the SSH error here... */
				GCES(err, sbbs->cfg.node_num, sbbs->ssh_session, "popping data");
				break;
			}
			else {
				pthread_mutex_unlock(&sbbs->ssh_mutex);
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

		if (rd == 0 && !socket_recvdone(sock, 0))
			continue;

		if(rd == SOCKET_ERROR)
		{
#ifdef __unix__
			if(sock==sbbs->client_socket)  {
#endif
				if(!sbbs->online)	// sbbs_t::hangup() called?
					break;
				if(ERROR_VALUE == EAGAIN)
					continue;
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
					lprintf(LOG_ERR,"Node %d !ERROR %d (%s) on local spy socket %d receive"
						, sbbs->cfg.node_num, errno, strerror(errno), sock);
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
			lprintf(LOG_ERR,"!TELBUF OVERFLOW (%d>%d)",wr,(int)sizeof(telbuf));

		if(sbbs->passthru_socket_active == true) {
			BOOL writable = FALSE;
			if(socket_check(sbbs->passthru_socket, NULL, &writable, 1000) && writable)
				(void)sendsocket(sbbs->passthru_socket, (char*)wrbuf, wr);
			else
				lprintf(LOG_WARNING, "Node %d could not write to passthru socket (writable=%d)"
					, sbbs->cfg.node_num, (int)writable);
			continue;
		}

		/* First level Ctrl-C checking */
		if(!(sbbs->cfg.ctrlkey_passthru&(1<<CTRL_C))
			&& sbbs->rio_abortable
			&& !(sbbs->telnet_mode&TELNET_MODE_GATE)
			&& sbbs->telnet_remote_option[TELNET_BINARY_TX]!=TELNET_WILL
			&& memchr(wrbuf, CTRL_C, wr)) {
			if(RingBufFull(&sbbs->inbuf))
    			lprintf(LOG_DEBUG,"Node %d Ctrl-C hit with %u bytes in input buffer"
					,sbbs->cfg.node_num,RingBufFull(&sbbs->inbuf));
			if(RingBufFull(&sbbs->outbuf))
    			lprintf(LOG_DEBUG,"Node %d Ctrl-C hit with %u bytes in output buffer"
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
	sbbs->terminate_output_thread = true;
	if(node_socket[sbbs->cfg.node_num-1]==INVALID_SOCKET)	// Shutdown locally
		sbbs->terminated = true;	// Signal JS to stop execution

	thread_down();
	lprintf(LOG_DEBUG,"Node %d input thread terminated (received %lu bytes in %lu blocks)"
		,sbbs->cfg.node_num, total_recv, total_pkts);
}

// Flush the duplicate client_socket when activating the passthru socket
// to eliminate any stale data from the previous passthru session
void sbbs_t::passthru_socket_activate(bool activate)
{
	if(activate) {
		BOOL rd = FALSE;
		while(socket_check(client_socket_dup, &rd, /* wr_p */NULL, /* timeout */0) && rd) {
			char ch;
			if(recv(client_socket_dup, &ch, sizeof(ch), /* flags: */0) != sizeof(ch))
				break;
		}
	} else {
		/* Re-enable blocking (in case disabled by external program) */	 
		ulong l=0;	 
		ioctlsocket(client_socket_dup, FIONBIO, &l);	 
 	 
		/* Re-set socket options */
		char err[512];
		if(set_socket_options(&cfg, client_socket_dup, "passthru", err, sizeof(err)))	 
			lprintf(LOG_ERR,"%04d !ERROR %s setting passthru socket options", client_socket, err);

		do { // Allow time for the passthru_thread to move any pending socket data to the outbuf
			SLEEP(100); // Before the node_thread starts sending its own data to the outbuf
		} while(RingBufFull(&sbbs->outbuf));
	}
	passthru_socket_active = activate;
}

/*
 * This thread simply copies anything it manages to read from the
 * passthru_socket into the output ringbuffer.
 */
void passthru_thread(void* arg)
{
	sbbs_t	*sbbs = (sbbs_t*) arg;
	int		rd;
	char	inbuf[IO_THREAD_BUF_SIZE / 2];

	SetThreadName("sbbs/passthru");
	thread_up(FALSE /* setuid */);

	while(sbbs->online && sbbs->passthru_socket!=INVALID_SOCKET && !terminate_server) {
		if (!socket_readable(sbbs->passthru_socket, 1000))
			continue;

		if(sbbs->passthru_socket==INVALID_SOCKET)
			break;

		rd = RingBufFree(&sbbs->outbuf) / 2;
		if(rd > (int)sizeof(inbuf))
			rd = sizeof(inbuf);

    	rd = recv(sbbs->passthru_socket, inbuf, rd, 0);

		if(rd == SOCKET_ERROR)
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

		if(rd == 0)
		{
			char ch;
			if(recv(sbbs->passthru_socket, &ch, sizeof(ch), MSG_PEEK) == 0) {
				lprintf(sbbs->online ? LOG_WARNING : LOG_DEBUG
					,"Node %d passthru socket disconnected", sbbs->cfg.node_num);
				break;
			}
			YIELD();
			continue;
		}

		if(sbbs->xtrn_mode & EX_BIN) {
			BYTE	telnet_buf[sizeof(inbuf) * 2];
			BYTE*	bp = (BYTE*)inbuf;

			if(!(sbbs->telnet_mode&TELNET_MODE_OFF))
				rd = telnet_expand((BYTE*)inbuf, rd, telnet_buf, sizeof(telnet_buf), /* expand_cr */false, &bp);

			int wr = RingBufWrite(&sbbs->outbuf, bp, rd);
    		if(wr != rd) {
				lprintf(LOG_ERR,"Short-write (%ld of %ld bytes) from passthru socket to outbuf"
					,(long)wr, (long)rd);
				break;
			}
		} else {
			sbbs->rputs(inbuf, rd);
		}
	}
	if(sbbs->passthru_socket!=INVALID_SOCKET) {
		close_socket(sbbs->passthru_socket);
		sbbs->passthru_socket=INVALID_SOCKET;
	}
	thread_down();

	sbbs->passthru_thread_running = false;
	sbbs->passthru_socket_active = false;
}

void output_thread(void* arg)
{
	char		node[128];
	char		stats[128];
	BYTE		buf[IO_THREAD_BUF_SIZE];
	int			i=0;	// Assignment to silence Valgrind
	ulong		avail;
	ulong		total_sent=0;
	ulong		total_pkts=0;
	ulong		short_sends=0;
	ulong		bufbot=0;
	ulong		buftop=0;
	sbbs_t*		sbbs = (sbbs_t*) arg;
	ulong		mss=IO_THREAD_BUF_SIZE;
	ulong		ssh_errors = 0;

	SetThreadName("sbbs/termOutput");
	thread_up(TRUE /* setuid */);

	if(sbbs->cfg.node_num)
		SAFEPRINTF(node,"Node %d",sbbs->cfg.node_num);
	else
		SAFECOPY(node,sbbs->client_name);
#ifdef _DEBUG
	lprintf(LOG_DEBUG,"%s output thread started",node);
#endif

	sbbs->console|=CON_R_ECHO;

#ifdef TCP_MAXSEG
	/*
	 * Auto-tune the highwater mark to be the negotiated MSS for the
	 * socket (when possible)
	 */
	if(!sbbs->outbuf.highwater_mark) {
		socklen_t	sl;
		sl=sizeof(i);
		if(!getsockopt(sbbs->client_socket, IPPROTO_TCP, TCP_MAXSEG,
#ifdef _WIN32
			(char *)
#endif
			&i, &sl)) {
			/* Check for sanity... */
			if(i>100) {
#ifdef _WIN32
#ifdef TCP_TIMESTAMPS
				DWORD ts;
				sl = sizeof(ts);
				if (!getsockopt(sbbs->client_socket, IPPROTO_TCP, TCP_TIMESTAMPS, (char *)&ts, &sl)) {
					if (ts)
						i -= 12;
				}
#endif
#else
#if (defined(TCP_INFO) && defined(TCPI_OPT_TIMESTAMPS))
				struct tcp_info tcpi;

				sl = sizeof(tcpi);
				if (!getsockopt(sbbs->client_socket, IPPROTO_TCP, TCP_INFO,&tcpi, &sl)) {
					if (tcpi.tcpi_options & TCPI_OPT_TIMESTAMPS)
						i -= 12;
				}
#endif
#endif
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
	sbbs->terminate_output_thread = false;

	/* Note: do not terminate when online==FALSE, that is expected for the terminal server output_thread */
	while(sbbs->client_socket!=INVALID_SOCKET && !terminate_server && !sbbs->terminate_output_thread) {
		/*
		 * I'd like to check the linear buffer against the highwater
		 * at this point, but it would get too clumsy imho - Deuce
		 *
		 * Actually, another option would just be to have the size
		 * of the linear buffer equal to the MSS... any larger and
		 * you could have small sends off the end.  this would
		 * probably be even clumsier
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
				lprintf(LOG_WARNING,"%s !Insufficient linear output buffer (%lu > %d)"
					,node, avail, (int)sizeof(buf));
				avail=sizeof(buf);
			}
			/* If we know the MSS, use it as the max send() size. */
			if(avail>mss)
				avail=mss;
			buftop=RingBufRead(&sbbs->outbuf, buf, avail);
			bufbot=0;
			if (buftop == 0)
				continue;
		}

		/* Check socket for writability */
		if (!socket_writable(sbbs->client_socket, 1)) {
			continue;
		}

#ifdef USE_CRYPTLIB
		if(sbbs->ssh_mode) {
			int err;
			pthread_mutex_lock(&sbbs->ssh_mutex);
			if(sbbs->terminate_output_thread) {
				pthread_mutex_unlock(&sbbs->ssh_mutex);
				break;
			}
			if(!sbbs->ssh_mode) {
				pthread_mutex_unlock(&sbbs->ssh_mutex);
				continue;
			}
			if (cryptStatusError((err=cryptSetAttribute(sbbs->ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, sbbs->session_channel)))) {
				GCESSTR(err, node, LOG_WARNING, sbbs->ssh_session, "setting channel");
				ssh_errors++;
				sbbs->online=FALSE;
				i=buftop-bufbot;	// Pretend we sent it all
			}
			else {
				/*
				 * Limit as per js_socket.c.
				 * Sure, this is TLS, not SSH, but we see weird stuff here in sz file transfers.
				 */
				size_t sendbytes = buftop-bufbot;
				if (sendbytes > 0x2000)
					sendbytes = 0x2000;
				if(cryptStatusError((err=cryptPushData(sbbs->ssh_session, (char*)buf+bufbot, buftop-bufbot, &i)))) {
					/* Handle the SSH error here... */
					GCESSTR(err, node, LOG_WARNING, sbbs->ssh_session, "pushing data");
					ssh_errors++;
					sbbs->online=FALSE;
					i=buftop-bufbot;	// Pretend we sent it all
				}
				else {
					// READ = WRITE TIMEOUT HACK... REMOVE WHEN FIXED
					/* This sets the write timeout for the flush, then sets it to zero
					 * afterward... presumably because the read timeout gets set to
					 * what the current write timeout is.
					 */
					if(cryptStatusError(err=cryptSetAttribute(sbbs->ssh_session, CRYPT_OPTION_NET_WRITETIMEOUT, 5)))
						GCESSTR(err, node, LOG_WARNING, sbbs->ssh_session, "setting write timeout");
					if(cryptStatusError((err=cryptFlushData(sbbs->ssh_session)))) {
						GCESSTR(err, node, LOG_WARNING, sbbs->ssh_session, "flushing data");
						ssh_errors++;
						if (err != CRYPT_ERROR_TIMEOUT) {
							sbbs->online=FALSE;
							i=buftop-bufbot;	// Pretend we sent it all
						}
					}
					// READ = WRITE TIMEOUT HACK... REMOVE WHEN FIXED
					if(cryptStatusError(err=cryptSetAttribute(sbbs->ssh_session, CRYPT_OPTION_NET_WRITETIMEOUT, 0)))
						GCESSTR(err, node, LOG_WARNING, sbbs->ssh_session, "setting write timeout");
				}
			}
			pthread_mutex_unlock(&sbbs->ssh_mutex);
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
				lprintf(LOG_WARNING,"%s !ERROR %d sending on socket %d"
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
				(void)sendsocket(spy_socket[sbbs->cfg.node_num-1],(char*)buf+bufbot,i);
#ifdef __unix__
			if(uspy_socket[sbbs->cfg.node_num-1]!=INVALID_SOCKET)
				(void)sendsocket(uspy_socket[sbbs->cfg.node_num-1],(char*)buf+bufbot,i);
#endif
		}

		if(i!=(int)(buftop-bufbot)) {
			lprintf(LOG_WARNING,"%s !Short socket send (%u instead of %lu)"
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
		safe_snprintf(stats,sizeof(stats),"(sent %lu bytes in %lu blocks, %lu average, %lu short)"
			,total_sent, total_pkts, total_sent/total_pkts, short_sends);
	else
		stats[0]=0;

	thread_down();
	lprintf(LOG_DEBUG,"%s output thread terminated %s", node, stats);
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
	char		event_code[LEN_CODE+1];

	sbbs->lprintf(LOG_INFO,"BBS Events thread started");

	sbbs_srand();	/* Seed random number generator */

	SetThreadName("sbbs/events");
	thread_up(TRUE /* setuid */);

#ifdef JAVASCRIPT
	if(!(startup->options&BBS_OPT_NO_JAVASCRIPT)) {
		if((sbbs->js_cx = sbbs->js_init(&sbbs->js_runtime, &sbbs->js_glob, "event")) == NULL) /* This must be done in the context of the events thread */
			sbbs->lprintf(LOG_ERR,"!JavaScript Initialization FAILURE");
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
				sbbs->lprintf(LOG_WARNING,"Initializing last run time for event: %s"
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
				sbbs->lprintf(LOG_WARNING,"Initializing last call-out time for QWKnet hub: %s"
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
			sbbs->event_code = "unpackREP";
			/* Import any REP files that have magically appeared (via FTP perhaps) */
			SAFEPRINTF(str,"%sfile/",sbbs->cfg.data_dir);
			offset=strlen(str);
			strcat(str,"*.rep");
			glob(str,0,NULL,&g);
			for(i=0;i<(int)g.gl_pathc && !sbbs->terminated;i++) {
				if(flength(g.gl_pathv[i]) < 1)
					continue;
				sbbs->useron.number = 0;
				sbbs->lprintf(LOG_DEBUG, "Inbound QWK Reply Packet detected: %s", g.gl_pathv[i]);
				sbbs->useron.number = atoi(g.gl_pathv[i]+offset);
				getuserdat(&sbbs->cfg,&sbbs->useron);
				if(sbbs->useron.number != 0 && !(sbbs->useron.misc&(DELETED|INACTIVE))) {
					SAFEPRINTF(semfile,"%s.lock",g.gl_pathv[i]);
					if(!fmutex(semfile,startup->host_name,TIMEOUT_MUTEX_FILE)) {
						sbbs->lprintf(LOG_INFO," %s exists (unpack in progress?)", semfile);
						continue;
					}
					sbbs->online=ON_LOCAL;
					sbbs->console|=CON_L_ECHO;
					sbbs->getusrsubs();
					bool success = sbbs->unpack_rep(g.gl_pathv[i]);
					sbbs->delfiles(sbbs->cfg.temp_dir,ALLFILES);		/* clean-up temp_dir after unpacking */
					sbbs->online=FALSE;
					sbbs->console&=~CON_L_ECHO;

					/* putuserdat? */
					if(success) {
						if(remove(g.gl_pathv[i]))
							sbbs->errormsg(WHERE, ERR_REMOVE, g.gl_pathv[i], 0);
					} else {
						char badpkt[MAX_PATH+1];
						SAFEPRINTF2(badpkt, "%s.%lx.bad", g.gl_pathv[i], time(NULL));
						(void)remove(badpkt);
						if(rename(g.gl_pathv[i], badpkt) == 0)
							sbbs->lprintf(LOG_NOTICE, "%s renamed to %s", g.gl_pathv[i], badpkt);
						else
							sbbs->lprintf(LOG_ERR, "!ERROR %d (%s) renaming %s to %s"
								,errno, strerror(errno), g.gl_pathv[i], badpkt);
						SAFEPRINTF(badpkt, "%u.rep.*.bad", sbbs->useron.number);
						SAFEPRINTF(str,"%sfile/", sbbs->cfg.data_dir);
						sbbs->delfiles(str, badpkt, /* keep: */10);
					}
					if(remove(semfile))
						sbbs->errormsg(WHERE, ERR_REMOVE, semfile, 0);
				}
				else {
					sbbs->lprintf(LOG_INFO, "Removing: %s", g.gl_pathv[i]);
					if(remove(g.gl_pathv[i]))
						sbbs->errormsg(WHERE, ERR_REMOVE, g.gl_pathv[i], 0);
				}
			}
			globfree(&g);
			sbbs->useron.number = 0;

			/* Create any QWK files that have magically appeared (via FTP perhaps) */
			sbbs->event_code = "packQWK";
			SAFEPRINTF(str,"%spack*.now",sbbs->cfg.data_dir);
			offset=strlen(sbbs->cfg.data_dir)+4;
			glob(str,0,NULL,&g);
			for(i=0;i<(int)g.gl_pathc && !sbbs->terminated;i++) {
				sbbs->useron.number = 0;
				sbbs->lprintf(LOG_INFO, "QWK pack semaphore signaled: %s", g.gl_pathv[i]);
				sbbs->useron.number = atoi(g.gl_pathv[i]+offset);
				SAFEPRINTF2(semfile,"%spack%04u.lock",sbbs->cfg.data_dir,sbbs->useron.number);
				if(!fmutex(semfile,startup->host_name,TIMEOUT_MUTEX_FILE)) {
					sbbs->lprintf(LOG_INFO,"%s exists (pack in progress?)", semfile);
					continue;
				}
				getuserdat(&sbbs->cfg,&sbbs->useron);
				if(sbbs->useron.number != 0 && !(sbbs->useron.misc&(DELETED|INACTIVE))) {
					sbbs->lprintf(LOG_INFO, "Packing QWK Message Packet");
					sbbs->online=ON_LOCAL;
					sbbs->console|=CON_L_ECHO;
					sbbs->getmsgptrs();
					sbbs->getusrsubs();

					sbbs->last_ns_time=sbbs->ns_time=sbbs->useron.ns_time;

					SAFEPRINTF3(str,"%sfile%c%04u.qwk"
						,sbbs->cfg.data_dir,PATH_DELIM,sbbs->useron.number);
					if(sbbs->pack_qwk(str,&l,true /* pre-pack/off-line */)) {
						sbbs->lprintf(LOG_INFO, "Packing completed: %s", str);
						sbbs->qwk_success(l,0,1);
						sbbs->putmsgptrs();
						remove(bat_list);
					} else
						sbbs->lputs(LOG_INFO, "No packet created (no new messages)");
					sbbs->delfiles(sbbs->cfg.temp_dir,ALLFILES);
					sbbs->console&=~CON_L_ECHO;
					sbbs->online=FALSE;
				}
				if(remove(g.gl_pathv[i]))
					sbbs->errormsg(WHERE, ERR_REMOVE, g.gl_pathv[i], 0);
				if(remove(semfile))
					sbbs->errormsg(WHERE, ERR_REMOVE, semfile, 0);
			}
			globfree(&g);
			sbbs->useron.number = 0;

			/* Create (pre-pack) QWK files for users configured as such */
			sbbs->event_code = "prepackQWK";
			SAFEPRINTF(semfile,"%sprepack.now",sbbs->cfg.data_dir);
			if(sbbs->cfg.preqwk_ar[0]
				&& (fexistcase(semfile) || (now-lastprepack)/60>(60*24))) {
				j=lastuser(&sbbs->cfg);
				sbbs->lputs(LOG_INFO,"Pre-packing QWK Message packets...");
				int userfile = openuserdat(&sbbs->cfg, /* for_modify: */FALSE);
				for(i=1;i<=j;i++) {

					SAFEPRINTF2(str,"%5u of %-5u",i,j);
					//status(str);
					sbbs->useron.number=i;
					fgetuserdat(&sbbs->cfg,&sbbs->useron, userfile);

					if(sbbs->useron.number
						&& !(sbbs->useron.misc&(DELETED|INACTIVE))	 /* Pre-QWK */
						&& sbbs->chk_ar(sbbs->cfg.preqwk_ar,&sbbs->useron,/* client: */NULL)) {
						for(k=1;k<=sbbs->cfg.sys_nodes;k++) {
							if(sbbs->getnodedat(k,&node,0)!=0)
								continue;
							if((node.status==NODE_INUSE || node.status==NODE_QUIET
								|| node.status==NODE_LOGON) && node.useron==i)
								break;
						}
						if(k<=sbbs->cfg.sys_nodes)	/* Don't pre-pack with user online */
							continue;
						sbbs->lprintf(LOG_INFO, "Pre-packing QWK");
						sbbs->online=ON_LOCAL;
						sbbs->console|=CON_L_ECHO;
						sbbs->getmsgptrs();
						sbbs->getusrsubs();
						SAFEPRINTF3(str,"%sfile%c%04u.qwk"
							,sbbs->cfg.data_dir,PATH_DELIM,sbbs->useron.number);
						if(sbbs->pack_qwk(str,&l,true /* pre-pack */)) {
							sbbs->qwk_success(l,0,1);
							sbbs->putmsgptrs();
						}
						sbbs->delfiles(sbbs->cfg.temp_dir,ALLFILES);
						sbbs->console&=~CON_L_ECHO;
						sbbs->online=FALSE;
					}
				}
				closeuserdat(userfile);
				lastprepack=(time32_t)now;
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
			sbbs->useron.number = 0;
		}

		if(check_semaphores) {

			/* Run daily maintenance? */
			sbbs->cfg.node_num=0;
			if(!(startup->options&BBS_OPT_NO_NEWDAY_EVENTS)) {
				sbbs->event_code = "";
				sbbs->logonstats();
				if(sbbs->sys_status&SS_DAILY)
					sbbs->daily_maint();
			}
			/* Node Daily Events */
			sbbs->event_code = "DAILY";
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
						SAFECOPY(sbbs->cfg.node_dir, sbbs->cfg.node_path[i-1]);

						sbbs->lprintf(LOG_INFO,"Running node %d daily event",i);
						sbbs->online=ON_LOCAL;
						sbbs->console|=CON_L_ECHO;
						sbbs->logentry("!:","Run node daily event");
						const char* cmd = sbbs->cmdstr(sbbs->cfg.node_daily,nulstr,nulstr,NULL);
						int result = sbbs->external(cmd, EX_OFFLINE);
						sbbs->lprintf(result ? LOG_ERR : LOG_INFO, "Node daily event: '%s' returned %d", cmd, result);
						sbbs->console&=~CON_L_ECHO;
						sbbs->online=FALSE;
					}
					sbbs->getnodedat(i,&node,1);
					node.misc&=~NODE_EVENT;
					node.status=NODE_WFC;
					node.useron=0;
					sbbs->putnodedat(i,&node);
				}
			}
			sbbs->event_code = nulstr;

			/* QWK Networking Call-out semaphores */
			for(i=0;i<sbbs->cfg.total_qhubs;i++) {
				if(sbbs->cfg.qhub[i]->node != NODE_ANY
					&& (sbbs->cfg.qhub[i]->node<first_node || sbbs->cfg.qhub[i]->node>last_node))
					continue;
				if(sbbs->cfg.qhub[i]->last==-1) // already signaled
					continue;
				SAFEPRINTF2(str,"%sqnet/%s.now",sbbs->cfg.data_dir,sbbs->cfg.qhub[i]->id);
				if(fexistcase(str)) {
					SAFECOPY(str,sbbs->cfg.qhub[i]->id);
					sbbs->lprintf(LOG_INFO,"Semaphore signaled for QWK Network Hub: %s",strupr(str));
					sbbs->cfg.qhub[i]->last=-1;
				}
			}

			/* Timed Event semaphores */
			for(i=0;i<sbbs->cfg.total_events;i++) {
				if(sbbs->cfg.event[i]->node != NODE_ANY
					&& (sbbs->cfg.event[i]->node<first_node || sbbs->cfg.event[i]->node>last_node)
					&& !(sbbs->cfg.event[i]->misc&EVENT_EXCL))
					continue;	// ignore non-exclusive events for other instances
				if(sbbs->cfg.event[i]->misc&EVENT_DISABLED)
					continue;
				if(sbbs->cfg.event[i]->last==-1) // already signaled
					continue;
				SAFEPRINTF2(str,"%s%s.now",sbbs->cfg.data_dir,sbbs->cfg.event[i]->code);
				if(fexistcase(str)) {
					SAFECOPY(str,sbbs->cfg.event[i]->code);
					sbbs->lprintf(LOG_INFO,"Semaphore signaled for Timed Event: %s",strupr(str));
					sbbs->cfg.event[i]->last=-1;
				}
			}
		}

		/* QWK Networking Call-out Events */
		sbbs->event_code = "QNET";
		for(i=0;i<sbbs->cfg.total_qhubs && !sbbs->terminated;i++) {
			if(sbbs->cfg.qhub[i]->node != NODE_ANY
				&& (sbbs->cfg.qhub[i]->node<first_node || sbbs->cfg.qhub[i]->node>last_node))
				continue;

			if(check_semaphores) {
				// See if any packets have come in
				SAFEPRINTF2(str,"%s%s.q??",sbbs->cfg.data_dir,sbbs->cfg.qhub[i]->id);
				glob(str,GLOB_NOSORT,NULL,&g);
				for(j=0;j<(int)g.gl_pathc;j++) {
					SAFECOPY(str,g.gl_pathv[j]);
					if(flength(str)>0) {	/* silently ignore 0-byte QWK packets */
						sbbs->lprintf(LOG_DEBUG,"Inbound QWK Packet detected: %s", str);
						sbbs->online=ON_LOCAL;
						sbbs->console|=CON_L_ECHO;
						if(sbbs->unpack_qwk(str,i)==false) {
							char newname[MAX_PATH+1];
							SAFEPRINTF2(newname,"%s.%lx.bad",str,(long)now);
							remove(newname);
							if(rename(str,newname)==0)
								sbbs->lprintf(LOG_NOTICE, "%s renamed to %s", str, newname);
							else
								sbbs->lprintf(LOG_ERR, "!ERROR %d (%s) renaming %s to %s"
									,errno, strerror(errno), str, newname);
							SAFEPRINTF(newname, "%s.q??.*.bad", sbbs->cfg.qhub[i]->id);
							sbbs->delfiles(sbbs->cfg.data_dir, newname, /* keep: */10);
						}
						sbbs->delfiles(sbbs->cfg.temp_dir,ALLFILES);
						sbbs->console&=~CON_L_ECHO;
						sbbs->online=FALSE;
						if(fexist(str) && remove(str))
							sbbs->errormsg(WHERE, ERR_REMOVE, str, 0);
					}
				}
				globfree(&g);
			}

			/* Qnet call out based on time */
			tmptime=sbbs->cfg.qhub[i]->last;
			if(localtime_r(&tmptime,&tm)==NULL)
				memset(&tm,0,sizeof(tm));
			if(sbbs->cfg.qhub[i]->last==-1L					/* or frequency */
				|| (((sbbs->cfg.qhub[i]->freq && (now-sbbs->cfg.qhub[i]->last)/60>=sbbs->cfg.qhub[i]->freq)
					|| (sbbs->cfg.qhub[i]->time
						&& (now_tm.tm_hour*60)+now_tm.tm_min>=sbbs->cfg.qhub[i]->time
						&& (now_tm.tm_mday!=tm.tm_mday || now_tm.tm_mon!=tm.tm_mon)))
							&& sbbs->cfg.qhub[i]->days&(1<<now_tm.tm_wday))) {
				SAFEPRINTF2(str,"%sqnet/%s.now"
					,sbbs->cfg.data_dir,sbbs->cfg.qhub[i]->id);
				if(fexistcase(str)) {
					if(remove(str))					/* Remove semaphore file */
						sbbs->errormsg(WHERE, ERR_REMOVE, str, 0);
				}
				SAFEPRINTF2(str,"%sqnet/%s.ptr"
					,sbbs->cfg.data_dir,sbbs->cfg.qhub[i]->id);
				file=sbbs->nopen(str,O_RDONLY);
				for(j=0;j<sbbs->cfg.qhub[i]->subs;j++) {
					sbbs->subscan[sbbs->cfg.qhub[i]->sub[j]->subnum].ptr=0;
					if(file!=-1) {
						lseek(file,sbbs->cfg.sub[sbbs->cfg.qhub[i]->sub[j]->subnum]->ptridx*sizeof(int32_t),SEEK_SET);
						read(file,&sbbs->subscan[sbbs->cfg.qhub[i]->sub[j]->subnum].ptr,sizeof(sbbs->subscan[sbbs->cfg.qhub[i]->sub[j]->subnum].ptr));
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
								sbbs->cfg.sub[sbbs->cfg.qhub[i]->sub[j]->subnum]->ptridx*4L) {
								l32=l;
								write(file,&l32,4);		/* initialize ptrs to null */
							}
							lseek(file
								,sbbs->cfg.sub[sbbs->cfg.qhub[i]->sub[j]->subnum]->ptridx*sizeof(int32_t)
								,SEEK_SET);
							write(file,&sbbs->subscan[sbbs->cfg.qhub[i]->sub[j]->subnum].ptr,sizeof(sbbs->subscan[sbbs->cfg.qhub[i]->sub[j]->subnum].ptr));
						}
						close(file);
					}
				}
				sbbs->delfiles(sbbs->cfg.temp_dir,ALLFILES);

				sbbs->cfg.qhub[i]->last=time32(NULL);
				SAFEPRINTF(str,"%sqnet.dab",sbbs->cfg.ctrl_dir);
				if((file=sbbs->nopen(str,O_WRONLY))==-1) {
					sbbs->errormsg(WHERE,ERR_OPEN,str,O_WRONLY);
					break;
				}
				lseek(file,sizeof(time32_t)*i,SEEK_SET);
				write(file,&sbbs->cfg.qhub[i]->last,sizeof(sbbs->cfg.qhub[i]->last));
				close(file);

				if(sbbs->cfg.qhub[i]->call[0]) {
					if(sbbs->cfg.qhub[i]->node == NODE_ANY)
						sbbs->cfg.node_num = startup->first_node;
					else
						sbbs->cfg.node_num=sbbs->cfg.qhub[i]->node;
					if(sbbs->cfg.node_num<1)
						sbbs->cfg.node_num=1;
					SAFECOPY(sbbs->cfg.node_dir, sbbs->cfg.node_path[sbbs->cfg.node_num-1]);
					sbbs->lprintf(LOG_INFO,"Call-out: %s",sbbs->cfg.qhub[i]->id);
					sbbs->online=ON_LOCAL;
					sbbs->console|=CON_L_ECHO;
					int result = sbbs->external(
						 sbbs->cmdstr(sbbs->cfg.qhub[i]->call
							,sbbs->cfg.qhub[i]->id,sbbs->cfg.qhub[i]->id,NULL)
						,EX_OFFLINE|EX_SH);	/* sh for Unix perl scripts */
					sbbs->lprintf(result ? LOG_ERR : LOG_INFO, "Call-out to: %s returned %d", sbbs->cfg.qhub[i]->id, result);
					sbbs->console&=~CON_L_ECHO;
					sbbs->online=FALSE;
				}
			}
		}

		/* PostLink Networking Call-out Events */
		sbbs->event_code = "PostLink";
		for(i=0;i<sbbs->cfg.total_phubs && !sbbs->terminated;i++) {
			if(sbbs->cfg.phub[i]->node != NODE_ANY
				&& (sbbs->cfg.phub[i]->node<first_node || sbbs->cfg.phub[i]->node>last_node))
				continue;
			/* PostLink call out based on time */
			tmptime=sbbs->cfg.phub[i]->last;
			if(localtime_r(&tmptime,&tm)==NULL)
				memset(&tm,0,sizeof(tm));
			if(sbbs->cfg.phub[i]->last==-1
				|| (((sbbs->cfg.phub[i]->freq								/* or frequency */
					&& (now-sbbs->cfg.phub[i]->last)/60>=sbbs->cfg.phub[i]->freq)
				|| (sbbs->cfg.phub[i]->time
					&& (now_tm.tm_hour*60)+now_tm.tm_min>=sbbs->cfg.phub[i]->time
				&& (now_tm.tm_mday!=tm.tm_mday || now_tm.tm_mon!=tm.tm_mon)))
				&& sbbs->cfg.phub[i]->days&(1<<now_tm.tm_wday))) {

				sbbs->cfg.phub[i]->last=time32(NULL);
				SAFEPRINTF(str,"%spnet.dab",sbbs->cfg.ctrl_dir);
				if((file=sbbs->nopen(str,O_WRONLY))==-1) {
					sbbs->errormsg(WHERE,ERR_OPEN,str,O_WRONLY);
					break;
				}
				lseek(file,sizeof(time32_t)*i,SEEK_SET);
				write(file,&sbbs->cfg.phub[i]->last,sizeof(sbbs->cfg.phub[i]->last));
				close(file);

				if(sbbs->cfg.phub[i]->call[0]) {
					if(sbbs->cfg.phub[i]->node == NODE_ANY)
						sbbs->cfg.node_num = startup->first_node;
					else
						sbbs->cfg.node_num=sbbs->cfg.phub[i]->node;
					if(sbbs->cfg.node_num<1)
						sbbs->cfg.node_num=1;
					SAFECOPY(sbbs->cfg.node_dir, sbbs->cfg.node_path[sbbs->cfg.node_num-1]);
					sbbs->lprintf(LOG_INFO,"PostLink Network call-out: %s",sbbs->cfg.phub[i]->name);
					sbbs->online=ON_LOCAL;
					sbbs->console|=CON_L_ECHO;
					sbbs->external(
						 sbbs->cmdstr(sbbs->cfg.phub[i]->call,nulstr,nulstr,NULL)
						,EX_OFFLINE|EX_SH);	/* sh for Unix perl scripts */
					sbbs->online=FALSE;
					sbbs->console&=~CON_L_ECHO;
				}
			}
		}

		/* Timed Events */
		for(i=0;i<sbbs->cfg.total_events && !sbbs->terminated;i++) {
			if(sbbs->cfg.event[i]->node != NODE_ANY
				&& (!sbbs->cfg.event[i]->node || sbbs->cfg.event[i]->node>sbbs->cfg.sys_nodes))
				continue;	// ignore events for invalid nodes

			if(sbbs->cfg.event[i]->misc&EVENT_DISABLED)
				continue;

			if(sbbs->cfg.event[i]->node != NODE_ANY
				&& (sbbs->cfg.event[i]->node<first_node || sbbs->cfg.event[i]->node>last_node)
				&& !(sbbs->cfg.event[i]->misc&EVENT_EXCL))
				continue;	// ignore non-exclusive events for other instances

			SAFECOPY(event_code, sbbs->cfg.event[i]->code);
			strupr(event_code);
			sbbs->event_code = event_code;

			tmptime=sbbs->cfg.event[i]->last;
			if(localtime_r(&tmptime,&tm)==NULL)
				memset(&tm,0,sizeof(tm));
			if(sbbs->cfg.event[i]->last==-1 ||
				(((sbbs->cfg.event[i]->freq
					&& (now-sbbs->cfg.event[i]->last)/60>=sbbs->cfg.event[i]->freq)
				|| 	(!sbbs->cfg.event[i]->freq
					&& (now_tm.tm_hour*60)+now_tm.tm_min>=sbbs->cfg.event[i]->time
				&& (now_tm.tm_mday!=tm.tm_mday || now_tm.tm_mon!=tm.tm_mon)))
				&& sbbs->cfg.event[i]->days&(1<<now_tm.tm_wday)
				&& (sbbs->cfg.event[i]->mdays < 2
					|| sbbs->cfg.event[i]->mdays&(1<<now_tm.tm_mday))
				&& (sbbs->cfg.event[i]->months==0
					|| sbbs->cfg.event[i]->months&(1<<now_tm.tm_mon))))
			{
				if(sbbs->cfg.event[i]->node != NODE_ANY && (sbbs->cfg.event[i]->misc&EVENT_EXCL)) { /* exclusive event */

					if(sbbs->cfg.event[i]->node<first_node
						|| sbbs->cfg.event[i]->node>last_node) {
						sbbs->lprintf(LOG_INFO,"Waiting for node %d to run timed event: %s"
							,sbbs->cfg.event[i]->node, event_code);
						sbbs->lprintf(LOG_DEBUG,"event last run: %s (0x%08x)"
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
								sbbs->cfg.event[i]->last=(time32_t)now;
								continue;
							}
							lseek(file,(long)i*4L,SEEK_SET);
							read(file,&sbbs->cfg.event[i]->last,sizeof(sbbs->cfg.event[i]->last));
							close(file);
							if(now-sbbs->cfg.event[i]->last<(60*60))	/* event is done */
								break;
							if(now-start>(90*60)) {
								sbbs->lprintf(LOG_WARNING,"!TIMEOUT waiting for event to complete");
								break;
							}
						}
						SAFEPRINTF2(str,"%s%s.now",sbbs->cfg.data_dir,sbbs->cfg.event[i]->code);
						if(fexistcase(str)) {
							if(remove(str))
								sbbs->errormsg(WHERE, ERR_REMOVE, str, 0);
						}
						sbbs->cfg.event[i]->last=(time32_t)now;
					} else {	// Exclusive event to run on a node under our control
						sbbs->lprintf(LOG_INFO,"Waiting for all nodes to become inactive before running timed event");
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
							sbbs->lprintf(LOG_DEBUG,"Waiting for node %d (status=%d)"
								,j, node.status);
							if(now-start>(90*60)) {
								sbbs->lprintf(LOG_WARNING,"!TIMEOUT waiting for node %d to become inactive (status=%d)"
									,j, node.status);
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
				if(sbbs->cfg.event[i]->node != NODE_ANY
					&& (sbbs->cfg.event[i]->node<first_node || sbbs->cfg.event[i]->node>last_node)) {
					sbbs->lprintf(LOG_NOTICE,"Changing node status for nodes %d through %d to WFC"
						,first_node,last_node);
					sbbs->cfg.event[i]->last=(time32_t)now;
					for(j=first_node;j<=last_node;j++) {
						node.status=NODE_INVALID_STATUS;
						if(sbbs->getnodedat(j,&node,1)!=0)
							continue;
						node.status=NODE_WFC;
						sbbs->putnodedat(j,&node);
					}
				}
				else {
					if(sbbs->cfg.event[i]->node == NODE_ANY)
						sbbs->cfg.node_num = startup->first_node;
					else
						sbbs->cfg.node_num=sbbs->cfg.event[i]->node;
					if(sbbs->cfg.node_num<1)
						sbbs->cfg.node_num=1;
					SAFECOPY(sbbs->cfg.node_dir, sbbs->cfg.node_path[sbbs->cfg.node_num-1]);

					SAFEPRINTF2(str,"%s%s.now",sbbs->cfg.data_dir,sbbs->cfg.event[i]->code);
					if(fexistcase(str)) {
						if(remove(str))
							sbbs->errormsg(WHERE, ERR_REMOVE, str, 0);
					}
					if(sbbs->cfg.event[i]->node != NODE_ANY && (sbbs->cfg.event[i]->misc&EVENT_EXCL)) {
						sbbs->getnodedat(sbbs->cfg.event[i]->node,&node,1);
						node.status=NODE_EVENT_RUNNING;
						sbbs->putnodedat(sbbs->cfg.event[i]->node,&node);
					}
					int ex_mode = EX_OFFLINE;
					if(!(sbbs->cfg.event[i]->misc&EVENT_EXCL)
						&& sbbs->cfg.event[i]->misc&EX_BG)
						ex_mode |= EX_BG;
					if(sbbs->cfg.event[i]->misc&XTRN_SH)
						ex_mode |= EX_SH;
					ex_mode|=(sbbs->cfg.event[i]->misc&EX_NATIVE);
					sbbs->online=ON_LOCAL;
					sbbs->console|=CON_L_ECHO;
					sbbs->lprintf(LOG_INFO,"Running %s%stimed event: %s"
						,(ex_mode&EX_NATIVE)	? "native ":""
						,(ex_mode&EX_BG)		? "background ":""
						,event_code);
					{
						int result=
							sbbs->external(
								 sbbs->cmdstr(sbbs->cfg.event[i]->cmd,nulstr,sbbs->cfg.event[i]->dir,NULL)
								,ex_mode
								,sbbs->cfg.event[i]->dir);
						if(!(ex_mode&EX_BG))
							sbbs->lprintf(result ? sbbs->cfg.event[i]->errlevel : LOG_INFO, "Timed event: %s returned %d", event_code, result);
					}
					sbbs->console&=~CON_L_ECHO;
					sbbs->online=FALSE;
					sbbs->cfg.event[i]->last=time32(NULL);
					SAFEPRINTF(str,"%stime.dab",sbbs->cfg.ctrl_dir);
					if((file=sbbs->nopen(str,O_WRONLY))==-1) {
						sbbs->errormsg(WHERE,ERR_OPEN,str,O_WRONLY);
						break;
					}
					lseek(file,(long)i*4L,SEEK_SET);
					write(file,&sbbs->cfg.event[i]->last,sizeof(sbbs->cfg.event[i]->last));
					close(file);

					if(sbbs->cfg.event[i]->node != NODE_ANY
						&& sbbs->cfg.event[i]->misc&EVENT_EXCL) { /* exclusive event */
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
		sbbs->event_code = nulstr;
		mswait(1000);
	}
	sbbs->cfg.node_num=0;
	sbbs->useron.number = 0;
	sbbs->js_cleanup();

	sbbs->event_thread_running = false;

	thread_down();
	sbbs->lprintf(LOG_INFO,"BBS Events thread terminated");
}


//****************************************************************************
sbbs_t::sbbs_t(ushort node_num, union xp_sockaddr *addr, size_t addr_len, const char* name, SOCKET sd,
			   scfg_t* global_cfg, char* global_text[], client_t* client_info, bool is_event_thread)
{
	char	path[MAX_PATH+1];
	uint	i;

	// Initialize some member variables used by lputs:
	this->is_event_thread = is_event_thread;
	cfg.node_num = node_num;
	event_code = nulstr;
	ZERO_VAR(useron);
	SAFECOPY(client_name, name);

    lprintf(LOG_DEBUG,"constructor using socket %d (settings=%x)"
		,sd, global_cfg->node_misc);

	startup = ::startup;	// Convert from global to class member

	memcpy(&cfg, global_cfg, sizeof(cfg));
	cfg.node_num = node_num;	// Restore the node_num passed to the constructor

	if(node_num>0) {
		SAFECOPY(cfg.node_dir, cfg.node_path[node_num-1]);
		prep_dir(cfg.node_dir, cfg.temp_dir, sizeof(cfg.temp_dir));
		SAFEPRINTF2(syspage_semfile, "%ssyspage.%u", cfg.ctrl_dir, node_num);
		(void)remove(syspage_semfile);
	} else {	/* event thread needs exclusive-use temp_dir */
		if(startup->temp_dir[0])
			SAFECOPY(cfg.temp_dir,startup->temp_dir);
		else
			SAFECOPY(cfg.temp_dir,"../temp");
    	prep_dir(cfg.ctrl_dir, cfg.temp_dir, sizeof(cfg.temp_dir));
		if((i = md(cfg.temp_dir)) != 0)
			lprintf(LOG_CRIT,"!ERROR %d (%s) creating directory: %s", i, strerror(i), cfg.temp_dir);
		if(sd==INVALID_SOCKET) {	/* events thread */
			if(startup->first_node==1)
				SAFEPRINTF(path,"%sevent",cfg.temp_dir);
			else
				SAFEPRINTF2(path,"%sevent%u",cfg.temp_dir,startup->first_node);
			backslash(path);
			SAFECOPY(cfg.temp_dir,path);
		}
		syspage_semfile[0] = 0;
	}
	lprintf(LOG_DEBUG, "temporary file directory: %s", cfg.temp_dir);

	terminated = false;
	event_thread_running = false;
    input_thread_running = false;
    output_thread_running = false;
	passthru_socket_active = false;
	passthru_thread_running = false;

	if(client_info==NULL)
		memset(&client,0,sizeof(client));
	else
		memcpy(&client,client_info,sizeof(client));
	client_addr.store = addr->store;
	client_socket = sd;
	client_socket_dup=INVALID_SOCKET;
	client_ident[0]=0;
	client_ipaddr[0]=0;

	terminal[0]=0;
	telnet_location[0]=0;
	telnet_terminal[0]=0;
	telnet_cols=0;
	telnet_rows=0;
	telnet_speed=0;
	telnet_cmds_received = 0;
	rlogin_name[0]=0;
	rlogin_pass[0]=0;
	rlogin_term[0]=0;

	/* Init some important variables */

	input_thread_mutex_created = false;
#ifdef USE_CRYPTLIB
	ssh_mode=false;
	ssh_mutex_created=false;
#endif

	rio_abortable=false;

	mouse_mode = MOUSE_MODE_OFF;
	hungry_hotspots = true;
	pause_hotspot = NULL;
	console = 0;
	online = 0;
	outchar_esc = ansiState_none;
	nodemsg_inside = 0;	/* allows single nest */
	hotkey_inside = 0;	/* allows single nest */
	event_time = 0;
	nodesync_inside = false;
	errormsg_inside = false;
	gettimeleft_inside = false;
	readmail_inside = false;
	scanposts_inside = false;
	scansubs_inside = false;
	pause_inside = false;
	timeleft = 60*10;	/* just incase this is being used for calling gettimeleft() */
	last_sysop_auth = 0;
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
	telnet_ack_event=CreateEvent(NULL, /* Manual Reset: */FALSE,/* InitialState */FALSE,NULL);

	listInit(&savedlines, /* flags: */0);
	sys_status=lncntr=criterrs=0L;
	msghdr_tos = false;
	listInit(&smb_list, /* flags: */0);
	listInit(&mouse_hotspots, /* flags: */0);
	column=0;
	tabstop=8;
	lastlinelen=0;
	curatr=LIGHTGRAY;
	hot_attr = 0;
	attr_sp=0;	/* attribute stack pointer */
	errorlevel=0;
	logcol=1;
	logfile_fp=NULL;
	nodefile=-1;
	pthread_mutex_init(&nodefile_mutex, NULL);
	node_ext=-1;
	nodefile_fp=NULL;
	node_ext_fp=NULL;
	current_msg=NULL;
	current_msg_subj=NULL;
	current_msg_from=NULL;
	current_msg_to=NULL;
	current_file=NULL;
	mnestr=NULL;

#ifdef JAVASCRIPT
	js_runtime=NULL;	/* runtime */
	js_cx=NULL;			/* context */
	js_hotkey_runtime = NULL;
	js_hotkey_cx = NULL;
#endif

	for(i=0;i<TOTAL_TEXT;i++)
		text[i]=text_sav[i]=global_text[i];

	ZERO_VAR(main_csi);
	ZERO_VAR(thisnode);
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
	memset(sysvar_p, 0, sizeof(sysvar_p));
	memset(sysvar_l, 0, sizeof(sysvar_l));

	cursub=NULL;
	cursubnum=INVALID_SUB;
	usrgrp=NULL;
	usrsubs=NULL;
	usrsub=NULL;
	usrgrp_total=0;

	subscan=NULL;

	curdir=NULL;
	curdirnum=INVALID_SUB;
	usrlib=NULL;
	usrdirs=NULL;
	usrdir=NULL;
	usrlib_total=0;

	/* used by update_qwkroute(): */
	qwknode=NULL;
	total_qwknodes=0;

	qwkmail_last = 0;
	logon_ulb = 0;
    logon_dlb = 0;
    logon_uls = 0;
    logon_dls = 0;
    logon_posts = 0;
    logon_emails = 0;
    logon_fbacks = 0;
    logon_ml = 0;
    main_cmds = 0;
    xfer_cmds = 0;
    posts_read = 0;
    autohang = 0;
    curgrp = 0;
    curlib = 0;
    usrgrps = 0;
    usrlibs = 0;
    comspec = 0;
    noaccess_str = 0;
    noaccess_val = 0;
    cur_output_rate = output_rate_unlimited;
    getstr_offset = 0;
    lastnodemsg = 0;
    xtrn_mode = 0;
	last_ns_time = 0;
}

//****************************************************************************
bool sbbs_t::init()
{
	char		str[MAX_PATH+1];
	char		tmp[128];
	char		tmp2[128];
	int			result;
	uint		i,j,k,l;
	node_t		node;
	socklen_t	addr_len;
	union xp_sockaddr	addr;

	RingBufInit(&inbuf, IO_THREAD_BUF_SIZE);
	if(cfg.node_num>0)
		node_inbuf[cfg.node_num-1]=&inbuf;

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
		lprintf(LOG_DEBUG,"socket %u duplicated as %u", client_socket, client_socket_dup);
#else
		client_socket_dup = client_socket;
#endif

		addr_len=sizeof(addr);
		if((result=getsockname(client_socket, &addr.addr, &addr_len))!=0) {
			lprintf(LOG_ERR,"!ERROR %d (%d) getting address/port"
				,result, ERROR_VALUE);
			return(false);
		}
		inet_addrtop(&addr, local_addr, sizeof(local_addr));
		inet_addrtop(&client_addr, client_ipaddr, sizeof(client_ipaddr));
		SAFEPRINTF(str, "%sclient.ini", cfg.node_dir);
		FILE* fp = fopen(str, "wt");
		if(fp != NULL) {
			fprintf(fp, "sock=%d\n", client_socket);
			fprintf(fp, "addr=%s\n", client.addr);
			fprintf(fp, "host=%s\n", client.host);
			fprintf(fp, "port=%u\n", (uint)client.port);
			fprintf(fp, "time=%lu\n", (ulong)client.time);
			fprintf(fp, "prot=%s\n", client.protocol);
			fprintf(fp, "local_addr=%s\n", local_addr);
			fprintf(fp, "local_port=%u\n", (uint)inet_addrport(&addr));
			fclose(fp);
		}
		lprintf(LOG_INFO,"socket %u attached to local interface %s port %u"
			,client_socket, local_addr, inet_addrport(&addr));
		spymsg("Connected");
	}

	if((comspec=os_cmdshell())==NULL) {
		errormsg(WHERE, ERR_CHK, OS_CMD_SHELL_ENV_VAR" environment variable", 0);
		return(false);
	}

	if((i = md(cfg.temp_dir)) != 0) {
		lprintf(LOG_CRIT,"!ERROR %d (%s) creating directory: %s", i, strerror(i), cfg.temp_dir);
		return false;
	}

	/* Shared NODE files */
	SAFEPRINTF2(str,"%s%s",cfg.ctrl_dir,"node.dab");
	pthread_mutex_lock(&nodefile_mutex);
	if((nodefile=nopen(str,O_DENYNONE|O_RDWR|O_CREAT))==-1) {
		errormsg(WHERE, ERR_OPEN, str, cfg.node_num);
		pthread_mutex_unlock(&nodefile_mutex);
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
	pthread_mutex_unlock(&nodefile_mutex);

	if(i>=LOOP_NODEDAB) {
		errormsg(WHERE, ERR_LOCK, str, cfg.node_num);
		return(false);
	}

	if(cfg.node_num) {
		SAFEPRINTF(str,"%snode.log",cfg.node_dir);
		if((logfile_fp=fopen(str,"a+b"))==NULL) {
			errormsg(WHERE, ERR_OPEN, str, 0);
			lprintf(LOG_NOTICE, "Perhaps this node is already running");
			return(false);
		}

		if(filelength(fileno(logfile_fp))) {
			log(crlf);
			now=time(NULL);
			struct tm tm;
			localtime_r(&now,&tm);
			time_t ftime = fdate(str);
			safe_snprintf(str,sizeof(str),"%s  %s %s %02d %u  "
				"End of preexisting log entry (possible crash on %.24s)"
				,hhmmtostr(&cfg,&tm,tmp)
				,wday[tm.tm_wday]
				,mon[tm.tm_mon],tm.tm_mday,tm.tm_year+1900
				,ctime_r(&ftime, tmp2));
			logline(LOG_NOTICE,"L!",str);
			log(crlf);
			catsyslog(TRUE);
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
	}
	if(cfg.total_subs) {
		if((subscan=(subscan_t *)calloc(cfg.total_subs, sizeof(subscan_t)))==NULL) {
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

#ifdef USE_CRYPTLIB
	pthread_mutex_init(&ssh_mutex,NULL);
	ssh_mutex_created = true;
#endif
	pthread_mutex_init(&input_thread_mutex,NULL);
	input_thread_mutex_created = true;

	reset_logon_vars();

	online=ON_REMOTE;

	return(true);
}

//****************************************************************************
sbbs_t::~sbbs_t()
{
	uint i;

	useron.number = 0;

#ifdef _DEBUG
	lprintf(LOG_DEBUG,"destructor begin");
#endif

//	if(!cfg.node_num)
//		rmdir(cfg.temp_dir);

	if(client_socket_dup!=INVALID_SOCKET && client_socket_dup!=client_socket)
		closesocket(client_socket_dup);	/* close duplicate handle */

	if(cfg.node_num>0)
		node_inbuf[cfg.node_num-1]=NULL;
	if(!input_thread_running)
		RingBufDispose(&inbuf);
	if(!output_thread_running && !passthru_thread_running)
		RingBufDispose(&outbuf);

	if(telnet_ack_event!=NULL)
		CloseEvent(telnet_ack_event);

	/* Close all open files */
	pthread_mutex_lock(&nodefile_mutex);
	if(nodefile!=-1) {
		close(nodefile);
		nodefile=-1;
		pthread_mutex_unlock(&nodefile_mutex);
	}
	if(node_ext!=-1) {
		close(node_ext);
		node_ext=-1;
	}
	if(logfile_fp!=NULL) {
		fclose(logfile_fp);
		logfile_fp=NULL;
	}

	if(syspage_semfile[0])
		remove(syspage_semfile);

	/********************************/
	/* Free allocated class members */
	/********************************/

	js_cleanup();

	/* Reset text.dat */

	for(i=0;i<TOTAL_TEXT;i++)
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

	listFree(&savedlines);
	listFree(&smb_list);
	listFree(&mouse_hotspots);

#ifdef USE_CRYPTLIB
	while(ssh_mutex_created && pthread_mutex_destroy(&ssh_mutex)==EBUSY)
		mswait(1);
#endif
	while(input_thread_mutex_created && pthread_mutex_destroy(&input_thread_mutex)==EBUSY)
		mswait(1);

#if 0 && defined(_WIN32) && defined(_DEBUG) && defined(_MSC_VER)
	if(!_CrtCheckMemory())
		lprintf(LOG_ERR,"!MEMORY ERRORS REPORTED IN DATA/DEBUG.LOG!");
#endif

#ifdef _DEBUG
	lprintf(LOG_DEBUG, "destructor end");
#endif
}

int sbbs_t::smb_stack(smb_t* smb, bool push)
{
	if(push) {
		if(smb == NULL || !SMB_IS_OPEN(smb))
			return SMB_SUCCESS;	  /* Msg base not open, do nothing */
		if(listPushNodeData(&smb_list, smb, sizeof(*smb)) == NULL)
			return SMB_FAILURE;
		return SMB_SUCCESS;
	}
	/* pop */
	smb_t* data = (smb_t*)listPopNode(&smb_list);
	if(data == NULL)	/* Nothing on the stack, so do nothing */
		return SMB_SUCCESS;

	*smb = *data;
	free(data);
	return SMB_SUCCESS;
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
        logline(LOG_WARNING,"!!",logstr);
	}
    if(file==-1 && (errno==EACCES || errno==EAGAIN)) {
        SAFEPRINTF2(logstr,"NOPEN ACCESS DENIED - File: \"%s\" errno: %d"
			,str,errno);
		logline(LOG_WARNING,"!!",logstr);
		bputs("\7\r\nNOPEN: ACCESS DENIED\r\n\7");
	}
    return(file);
}

void sbbs_t::spymsg(const char* msg)
{
	char str[512];

	if(cfg.node_num<1)
		return;

	SAFEPRINTF4(str,"\r\n\r\n*** Spy Message ***\r\nNode %d: %s [%s]\r\n*** %s ***\r\n\r\n"
		,cfg.node_num,client_name,client_ipaddr,msg);
	if(startup->node_spybuf!=NULL
		&& startup->node_spybuf[cfg.node_num-1]!=NULL) {
		RingBufWrite(startup->node_spybuf[cfg.node_num-1],(uchar*)str,strlen(str));
		/* Signal spy output semaphore? */
		if(startup->node_spysem!=NULL
			&& startup->node_spysem[sbbs->cfg.node_num-1]!=NULL)
			sem_post(startup->node_spysem[sbbs->cfg.node_num-1]);
	}

	if(cfg.node_num && spy_socket[cfg.node_num-1]!=INVALID_SOCKET)
		(void)sendsocket(spy_socket[cfg.node_num-1],str,strlen(str));
#ifdef __unix__
	if(cfg.node_num && uspy_socket[cfg.node_num-1]!=INVALID_SOCKET)
		(void)sendsocket(uspy_socket[cfg.node_num-1],str,strlen(str));
#endif
}

/****************************************************************************/
/* Moves or copies a file from one dir to another                           */
/* both 'src' and 'dest' must contain full path and filename                */
/* returns 0 if successful, -1 if error                                     */
/****************************************************************************/
int sbbs_t::mv(const char* path, const char* dest, bool copy)
{
	char src[MAX_PATH + 1];

    if(!stricmp(path, dest))	 /* source and destination are the same! */
        return(0);

	SAFECOPY(src, path);
    if(!fexistcase(src)) {
        bprintf("\r\n\7MV ERROR: Source doesn't exist\r\n'%s'\r\n"
            ,src);
        return(-1);
	}
    if(!copy && fexist(dest)) {
        bprintf("\r\n\7MV ERROR: Destination already exists\r\n'%s'\r\n"
            ,dest);
        return(-1);
	}
    if(!copy && rename(src, dest) == 0)
        return 0;
	if(!CopyFile(src, dest, /* fail if exists: */!copy)) {
		errormsg(WHERE, "CopyFile", src, 0, dest);
		return -1;
	}
    if(!copy && remove(src)) {
        errormsg(WHERE,ERR_REMOVE,src,0);
        return(-1);
	}
    return(0);
}

void sbbs_t::hangup(void)
{
	if(online) {
		lprintf(LOG_DEBUG,"disconnecting client");
		online=FALSE;	// moved from the bottom of this function on Jan-25-2009
	}
	if(client_socket_dup!=INVALID_SOCKET && client_socket_dup!=client_socket)
		closesocket(client_socket_dup);
	client_socket_dup=INVALID_SOCKET;

	if(client_socket!=INVALID_SOCKET) {
		mswait(1000);	/* Give socket output buffer time to flush */
		client_off(client_socket);
		if(ssh_mode) {
			pthread_mutex_lock(&sbbs->ssh_mutex);
			ssh_session_destroy(client_socket, ssh_session, __LINE__);
			sbbs->ssh_mode = false;
			pthread_mutex_unlock(&sbbs->ssh_mutex);
		}
		close_socket(client_socket);
		client_socket=INVALID_SOCKET;
	}
	sem_post(&outbuf.sem);
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

// Steve's original implementation (in RCIOL) did not incorporate a retry
// ... so this function does not either. :-P
int sbbs_t::_outcom(uchar ch)
{
	if(!RingBufFree(&outbuf))
		return(TXBOF);
    if(!RingBufWrite(&outbuf, &ch, 1))
		return(TXBOF);
	return(0);
}

// This outcom version retries - copied loop from sbbs_t::outchar()
int sbbs_t::outcom(uchar ch, int max_attempts)
{
	int i = 0;
	while(_outcom(ch) != 0) {
		if(!online)
			break;
		i++;
		if(i >= max_attempts) {			/* timeout - beep flush outbuf */
			lprintf(LOG_NOTICE, "timeout(outcom) %04X %04X", rioctl(TXBC), rioctl(IOFO));
			_outcom(BEL);
			rioctl(IOCS|PAUSE);
			return TXBOF;
		}
		if(sys_status&SS_SYSPAGE)
			sbbs_beep(i, OUTCOM_RETRY_DELAY);
		else
			mswait(OUTCOM_RETRY_DELAY);
	}
	return 0;	// Success
}

int sbbs_t::putcom(const char *str, size_t len)
{
	size_t i;

	if(!len)
		len=strlen(str);
    for(i=0;i<len && online;i++)
		if(outcom(str[i])!=0)
			break;
	return i;
}

/* Legacy Remote I/O Control Interface:
 * This function mimics the RCIOL MS-DOS library written in 8086 assembler by Steven B. Deppe (1958-2014).
 * This function prototype shall remain the same in tribute to Steve (Ille Homine Albe).
 */
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

    sys_status&=~(SS_USERON|SS_TMPSYSOP|SS_LCHAT|SS_ABORT
        |SS_PAUSEON|SS_PAUSEOFF|SS_EVENT|SS_NEWUSER|SS_NEWDAY|SS_QWKLOGON|SS_FASTLOGON);
    cid[0]=0;
    wordwrap[0]=0;
    question[0]=0;
    menu_dir[0]=0;
    menu_file[0]=0;
	row = 0;
    rows = TERM_ROWS_DEFAULT;
	cols = TERM_COLS_DEFAULT;
    lncntr=0;
    autoterm=0;
	cterm_version = 0;
    lbuflen=0;
    timeleft_warn=0;
	keybufbot=keybuftop=0;
    logon_uls=logon_ulb=logon_dls=logon_dlb=0;
    logon_posts=logon_emails=logon_fbacks=0;
    usrgrps=usrlibs=0;
    curgrp=curlib=0;
	for(i=0;i<cfg.total_libs;i++)
		curdir[i]=0;
	for(i=0;i<cfg.total_grps;i++)
		cursub[i]=0;
	cur_cps=3000;
    cur_rate=30000;
    dte_rate=38400;
	cur_output_rate = output_rate_unlimited;
	main_cmds=xfer_cmds=posts_read=0;
	lastnodemsg=0;
	lastnodemsguser[0]=0;
}

/****************************************************************************/
/* Writes NODE.LOG at end of SYSTEM.LOG										*/
/****************************************************************************/
void sbbs_t::catsyslog(int crash)
{
	char str[MAX_PATH+1] = "node.log";
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
	length = ftell(logfile_fp);
	if(length > 0) {
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
			stats.timeon+=(uint32_t)(now-logontime)/60;
			stats.ttoday+=(uint32_t)(now-logontime)/60;
			stats.ptoday+=logon_posts;
		}
		stats.uls+=(uint32_t)logon_uls;
		stats.ulb+=(uint32_t)logon_ulb;
		// logon_dls and logons_dlb are now handled in user_downloaded_file()
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
	char			str[MAX_PATH + 1];
	int				file;
	uint			curshell=0;
	node_t			node;
	ulong			login_attempts;
	sbbs_t*			sbbs = (sbbs_t*) arg;

	update_clients();
	SetThreadName("sbbs/termNode");
	thread_up(TRUE /* setuid */);

#ifdef _DEBUG
	lprintf(LOG_DEBUG,"Node %d thread started",sbbs->cfg.node_num);
#endif

	sbbs_srand();		/* Seed random number generator */

#ifdef JAVASCRIPT
	if(!(startup->options&BBS_OPT_NO_JAVASCRIPT)) {
		if((sbbs->js_cx = sbbs->js_init(&sbbs->js_runtime, &sbbs->js_glob, "node")) == NULL) /* This must be done in the context of the node thread */
			lprintf(LOG_ERR,"Node %d !JavaScript Initialization FAILURE",sbbs->cfg.node_num);
	}
#endif

	if(startup->login_attempt.throttle
		&& (login_attempts=loginAttempts(startup->login_attempt_list, &sbbs->client_addr)) > 1) {
		lprintf(LOG_DEBUG,"Node %d Throttling suspicious connection from: %s (%lu login attempts)"
			,sbbs->cfg.node_num, sbbs->client_ipaddr, login_attempts);
		mswait(login_attempts*startup->login_attempt.throttle);
	}

	bool login_success = false;
	if(sbbs->answer()) {

		login_success = true;
		if(sbbs->useron.pass[0])
			listAddNodeData(&current_logins, sbbs->client.addr, strlen(sbbs->client.addr)+1, sbbs->cfg.node_num, LAST_NODE);
		if(sbbs->sys_status&SS_QWKLOGON) {
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

				sbbs->main_csi.length=(long)filelength(file);
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
		listRemoveTaggedNode(&current_logins, sbbs->cfg.node_num, /* free_data */TRUE);
	}

#ifdef _WIN32
	if(startup->sound.hangup[0] && !sound_muted(&scfg))
		PlaySound(startup->sound.hangup, NULL, SND_ASYNC|SND_FILENAME);
#endif

	sbbs->hangup();	/* closes sockets, calls client_off, and shuts down the output_thread */
    node_socket[sbbs->cfg.node_num-1]=INVALID_SOCKET;

	sbbs->logout();
	sbbs->logoffstats();	/* Updates both system and node dsts.dab files */

	SAFEPRINTF(str, "%sclient.ini", sbbs->cfg.node_dir);
	FILE* fp = fopen(str, "at");
	if(fp != NULL) {
		fprintf(fp, "user=%u\n", sbbs->useron.number);
		fprintf(fp, "name=%s\n", sbbs->useron.alias);
		fprintf(fp, "done=%lu\n", (ulong)time(NULL));
		fclose(fp);
	}

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
		|| sbbs->passthru_thread_running
		) {
		lprintf(LOG_INFO,"Node %d Waiting for %s to terminate..."
			,sbbs->cfg.node_num
			,(sbbs->input_thread_running && sbbs->output_thread_running) ?
               	"I/O threads" : sbbs->input_thread_running
				? "input thread" : "output thread");
		time_t start=time(NULL);
		while(sbbs->input_thread_running
    		|| sbbs->output_thread_running
			|| sbbs->passthru_thread_running
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

	if(login_success)
		sbbs->catsyslog(/* Crash: */FALSE);
	else {
		rewind(sbbs->logfile_fp);
		chsize(fileno(sbbs->logfile_fp), 0);
	}

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

	{
		/* crash here on Aug-4-2015:
		node_thread_running already destroyed
		bbs_thread() timed out waiting for 1 node thread(s) to terminate */
		uint32_t remain = protected_uint32_adjust_fetch(&node_threads_running, -1);
		lprintf(LOG_INFO,"Node %d thread terminated (%u node threads remain, %lu clients served)"
			,sbbs->cfg.node_num, remain, served);
	}
    if(!sbbs->input_thread_running && !sbbs->output_thread_running)
		delete sbbs;
    else
		lprintf(LOG_WARNING,"Node %d !ORPHANED I/O THREAD(s)",sbbs->cfg.node_num);

	/* crash here July-27-2018:
 	ntdll.dll!77282e19()	Unknown
 	[Frames below may be incorrect and/or missing, no symbols loaded for ntdll.dll]
 	[External Code]
 	sbbs.dll!pthread_mutex_lock(_RTL_CRITICAL_SECTION * mutex) Line 171	C
 	sbbs.dll!protected_uint32_adjust(protected_uint32_t * i, int adjustment) Line 244	C
 	sbbs.dll!update_clients() Line 185	C++
>	sbbs.dll!node_thread(void * arg) Line 4568	C++
 	[External Code]

	node_threads_running	{value=0 mutex={DebugInfo=0x00000000 <NULL> LockCount=-6 RecursionCount=0 ...} }	protected_uint32_t

	and again on July-10-2019:

	ntdll.dll!RtlpWaitOnCriticalSection()	Unknown
 	ntdll.dll!RtlpEnterCriticalSectionContended()	Unknown
 	ntdll.dll!_RtlEnterCriticalSection@4()	Unknown
 	sbbs.dll!pthread_mutex_lock(_RTL_CRITICAL_SECTION * mutex) Line 171	C
 	sbbs.dll!protected_uint32_adjust(protected_uint32_t * i, int adjustment) Line 244	C
 	sbbs.dll!update_clients() Line 187	C++
>	sbbs.dll!node_thread(void * arg) Line 4668	C++
	*/
	update_clients();
	thread_down();
}

bool sbbs_t::backup(const char* fname, int backup_level, bool rename)
{
	if(!fexist(fname))
		return false;

	lprintf(LOG_DEBUG, "Backing-up %s (%lu bytes)", fname, (long)flength(fname));
	return ::backup(fname, backup_level, rename) ? true : false;
}

void sbbs_t::daily_maint(void)
{
	char			str[128];
	char			uname[LEN_ALIAS+1];
	uint			i;
	uint			usernum;
	uint			lastusernum;
	user_t			user;

	lputs(LOG_INFO, "DAILY: System maintenance begun");
	now=time(NULL);

	if(cfg.node_num) {
		if((i=getnodedat(cfg.node_num,&thisnode,true)) != 0)
			errormsg(WHERE,ERR_LOCK,"node file",i);
		else {
			thisnode.status=NODE_EVENT_RUNNING;
			putnodedat(cfg.node_num,&thisnode);
		}
	}

	if(cfg.user_backup_level) {
		lputs(LOG_INFO,"DAILY: Backing-up user data...");
		SAFEPRINTF(str,"%suser/user.dat",cfg.data_dir);
		backup(str,cfg.user_backup_level,FALSE);
		SAFEPRINTF(str,"%suser/name.dat",cfg.data_dir);
		backup(str,cfg.user_backup_level,FALSE);
	}

	if(cfg.mail_backup_level) {
		lputs(LOG_INFO,"DAILY: Backing-up mail data...");
		smb_t mail;
		int result = smb_open_sub(&cfg, &mail, INVALID_SUB);
		if(result != SMB_SUCCESS)
			lprintf(LOG_ERR, "ERROR %d (%s) opening mail base", result, mail.last_error);
		else {
			result = smb_lock(&mail);
			if(result != SMB_SUCCESS)
				lprintf(LOG_ERR, "ERROR %d (%s) locking mail base", result, mail.last_error);
			else {
				SAFEPRINTF(str,"%smail.shd",cfg.data_dir);
				backup(str,cfg.mail_backup_level,FALSE);
				SAFEPRINTF(str,"%smail.sha",cfg.data_dir);
				backup(str,cfg.mail_backup_level,FALSE);
				SAFEPRINTF(str,"%smail.sdt",cfg.data_dir);
				backup(str,cfg.mail_backup_level,FALSE);
				SAFEPRINTF(str,"%smail.sda",cfg.data_dir);
				backup(str,cfg.mail_backup_level,FALSE);
				SAFEPRINTF(str,"%smail.sid",cfg.data_dir);
				backup(str,cfg.mail_backup_level,FALSE);
				SAFEPRINTF(str,"%smail.sch",cfg.data_dir);
				backup(str,cfg.mail_backup_level,FALSE);
				SAFEPRINTF(str,"%smail.hash",cfg.data_dir);
				backup(str,cfg.mail_backup_level,FALSE);
				SAFEPRINTF(str,"%smail.ini",cfg.data_dir);
				backup(str,cfg.mail_backup_level,FALSE);
				result = smb_unlock(&mail);
				if(result != SMB_SUCCESS)
					lprintf(LOG_ERR, "ERROR %d (%s) unlocking mail base", result, mail.last_error);
			}
			smb_close(&mail);
		}
	}

	lputs(LOG_INFO, "DAILY: Checking for inactive/expired user records...");
	lastusernum=lastuser(&cfg);
	int userfile=openuserdat(&cfg, /* for_modify: */FALSE);
	for(usernum=1;usernum<=lastusernum;usernum++) {
		SAFEPRINTF2(str,"%5u of %-5u",usernum,lastusernum);
		status(str);
		user.number = usernum;
		if((i=fgetuserdat(&cfg, &user, userfile)) != 0) {
			SAFEPRINTF(str,"user record %u",usernum);
			errormsg(WHERE, ERR_READ, str, i);
			continue;
		}

		/***********************************************/
		/* Fix name (name.dat and user.dat) mismatches */
		/***********************************************/
		username(&cfg,user.number,uname);
		if(user.misc&DELETED) {
			if(strcmp(uname,"DELETED USER"))
				putusername(&cfg,user.number,nulstr);
			continue;
		}

		if(strcmp(user.alias,uname))
			putusername(&cfg,user.number,user.alias);

		if(user.number==1)
			continue;	/* skip expiration/inactivity checks for user #1 */

		if(!(user.misc&(DELETED|INACTIVE))
			&& user.expire && (ulong)user.expire<=(ulong)now) {
			putsmsg(&cfg,user.number,text[AccountHasExpired]);
			SAFEPRINTF2(str,"DAILY: %s #%u Expired",user.alias,user.number);
			lputs(LOG_NOTICE, str);
			if(cfg.level_misc[user.level]&LEVEL_EXPTOVAL
				&& cfg.level_expireto[user.level]<10) {
				user.flags1=cfg.val_flags1[cfg.level_expireto[user.level]];
				user.flags2=cfg.val_flags2[cfg.level_expireto[user.level]];
				user.flags3=cfg.val_flags3[cfg.level_expireto[user.level]];
				user.flags4=cfg.val_flags4[cfg.level_expireto[user.level]];
				user.exempt=cfg.val_exempt[cfg.level_expireto[user.level]];
				user.rest=cfg.val_rest[cfg.level_expireto[user.level]];
				if(cfg.val_expire[cfg.level_expireto[user.level]])
					user.expire=(time32_t)now
						+(cfg.val_expire[cfg.level_expireto[user.level]]*24*60*60);
				else
					user.expire=0;
				user.level=cfg.val_level[cfg.level_expireto[user.level]];
			}
			else {
				if(cfg.level_misc[user.level]&LEVEL_EXPTOLVL)
					user.level=cfg.level_expireto[user.level];
				else
					user.level=cfg.expired_level;
				user.flags1&=~cfg.expired_flags1; /* expired status */
				user.flags2&=~cfg.expired_flags2; /* expired status */
				user.flags3&=~cfg.expired_flags3; /* expired status */
				user.flags4&=~cfg.expired_flags4; /* expired status */
				user.exempt&=~cfg.expired_exempt;
				user.rest|=cfg.expired_rest;
				user.expire=0;
			}
			putuserrec(&cfg,user.number,U_LEVEL,2,ultoa(user.level,str,10));
			putuserrec(&cfg,user.number,U_FLAGS1,8,ultoa(user.flags1,str,16));
			putuserrec(&cfg,user.number,U_FLAGS2,8,ultoa(user.flags2,str,16));
			putuserrec(&cfg,user.number,U_FLAGS3,8,ultoa(user.flags3,str,16));
			putuserrec(&cfg,user.number,U_FLAGS4,8,ultoa(user.flags4,str,16));
			putuserrec(&cfg,user.number,U_EXPIRE,8,ultoa((ulong)user.expire,str,16));
			putuserrec(&cfg,user.number,U_EXEMPT,8,ultoa(user.exempt,str,16));
			putuserrec(&cfg,user.number,U_REST,8,ultoa(user.rest,str,16));
			if(cfg.expire_mod[0]) {
				useron=user;
				online=ON_LOCAL;
				exec_bin(cfg.expire_mod,&main_csi);
				online=FALSE;
			}
		}

		/***********************************************************/
		/* Auto deletion based on expiration date or days inactive */
		/***********************************************************/
		if(!(user.exempt&FLAG('P'))     /* Not a permanent account */
			&& !(user.misc&(DELETED|INACTIVE))	 /* alive */
			&& (cfg.sys_autodel && (now-user.laston)/(long)(24L*60L*60L)
			> cfg.sys_autodel)) {			/* Inactive too long */
			SAFEPRINTF3(str,"DAILY: Auto-Deleted %s #%u due to inactivity > %u days"
				,user.alias, user.number, cfg.sys_autodel);
			lputs(LOG_NOTICE, str);
			delallmail(user.number, MAIL_ANY);
			putusername(&cfg,user.number,nulstr);
			putuserrec(&cfg,user.number,U_MISC,8,ultoa(user.misc|DELETED,str,16));
		}
	}
	closeuserdat(userfile);

	lputs(LOG_INFO,"DAILY: Purging deleted/expired e-mail");
	SAFEPRINTF(smb.file,"%smail",cfg.data_dir);
	smb.retry_time=cfg.smb_retry_time;
	smb.subnum=INVALID_SUB;
	if((i=smb_open(&smb))!=0)
		errormsg(WHERE,ERR_OPEN,smb.file,i,smb.last_error);
	else {
		if(filelength(fileno(smb.shd_fp))>0) {
			if((i=smb_locksmbhdr(&smb))!=0)
				errormsg(WHERE,ERR_LOCK,smb.file,i,smb.last_error);
			else
				lprintf(LOG_INFO, "DAILY: Removed %d messages", delmail(0, MAIL_ALL));
		}
		smb_close(&smb);
	}

	if(cfg.sys_daily[0]) {
		lputs(LOG_INFO, "DAILY: Running system event");
		const char* cmd = cmdstr(cfg.sys_daily,nulstr,nulstr,NULL);
		online = ON_LOCAL;
		int result = external(cmd, EX_OFFLINE);
		online = FALSE;
		lprintf(result ? LOG_ERR : LOG_INFO, "Daily event: '%s' returned %d", cmd, result);
	}
	status(STATUS_WFC);
	lputs(LOG_INFO, "DAILY: System maintenance ended");
	sys_status&=~SS_DAILY;
}

const char* js_ver(void)
{
#ifdef JAVASCRIPT
	return(JS_GetImplementationVersion());
#else
	return("");
#endif
}

/* Returns char string of version and revision */
const char* bbs_ver(void)
{
	static char ver[256];
	char compiler[32];

	if(ver[0]==0) {	/* uninitialized */
		DESCRIBE_COMPILER(compiler);

		safe_snprintf(ver,sizeof(ver),"%s %s%c%s  Compiled %s/%s %s %s with %s"
			,TELNET_SERVER
			,VERSION, REVISION
#ifdef _DEBUG
			," Debug"
#else
			,""
#endif
			,git_branch, git_hash
			,__DATE__, __TIME__, compiler
			);
	}
	return(ver);
}

/* Returns binary-coded version and revision (e.g. 0x31000 == 3.10a) */
long bbs_ver_num(void)
{
	return(VERSION_HEX);
}

void bbs_terminate(void)
{
   	lprintf(LOG_INFO,"BBS Server terminate");
	terminate_server=true;
}

extern bbs_startup_t bbs_startup;
static void cleanup(int code)
{
    lputs(LOG_INFO,"Terminal Server thread terminating");

	xpms_destroy(ts_set, sock_close_cb, startup);

#ifdef _WINSOCKAPI_
	if(WSAInitialized && WSACleanup()!=0)
		lprintf(LOG_ERR,"!WSACleanup ERROR %d",ERROR_VALUE);
#endif

	free_cfg(&scfg);
	free_text(text);

	for(int i = 0; i < MAX_NODES; i++) {
		free_text(node_text[i]);
		free_cfg(&node_scfg[i]);
		memset(&node_scfg[i], 0, sizeof(node_scfg[i]));
	}

	semfile_list_free(&recycle_semfiles);
	semfile_list_free(&shutdown_semfiles);
	semfile_list_free(&clear_attempts_semfiles);

	listFree(&current_logins);
	listFree(&current_connections);

	protected_uint32_destroy(node_threads_running);
	protected_uint32_destroy(ssh_sessions);

	status("Down");
	thread_down();
	if(terminate_server || code)
		lprintf(LOG_INFO,"Terminal Server thread terminated (%lu clients served)", served);
	if(startup->terminated!=NULL)
		startup->terminated(startup->cbdata,code);
}

void bbs_thread(void* arg)
{
	char			host_name[256];
	char*			identity;
	char*			p;
    char			str[MAX_PATH+1];
	char			logstr[256];
	union xp_sockaddr	server_addr={{0}};
	union xp_sockaddr	client_addr;
	socklen_t		client_addr_len;
	SOCKET			client_socket=INVALID_SOCKET;
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
	struct main_sock_cb_data	uspy_cb[MAX_NODES]={};
	union xp_sockaddr uspy_addr;
#endif
#ifdef USE_CRYPTLIB
	CRYPT_CONTEXT	ssh_context;
#endif
	struct main_sock_cb_data	telnet_cb;
	struct main_sock_cb_data	ssh_cb;
	struct main_sock_cb_data	rlogin_cb;
	void						*ts_cb;

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

	ZERO_VAR(js_server_props);
	SAFEPRINTF3(js_server_props.version,"%s %s%c",TELNET_SERVER,VERSION,REVISION);
	js_server_props.version_detail=bbs_ver();
	js_server_props.clients=&node_threads_running;
	js_server_props.options=&startup->options;
	js_server_props.interfaces=&startup->telnet_interfaces;

	uptime=0;
	served=0;

	startup->recycle_now=FALSE;
	startup->shutdown_now=FALSE;
	terminate_server=false;

	SetThreadName("sbbs/termServer");

	do {
	/* Setup intelligent defaults */
	if(startup->telnet_port==0)				startup->telnet_port=IPPORT_TELNET;
	if(startup->rlogin_port==0)				startup->rlogin_port=513;
#ifdef USE_CRYPTLIB
	if(startup->ssh_port==0)				startup->ssh_port=22;
#endif
	if(startup->outbuf_drain_timeout==0)	startup->outbuf_drain_timeout=10;
	if(startup->sem_chk_freq==0)			startup->sem_chk_freq=DEFAULT_SEM_CHK_FREQ;
	if(startup->temp_dir[0])				backslash(startup->temp_dir);

	protected_uint32_init(&node_threads_running,0);
	protected_uint32_init(&ssh_sessions,0);

	thread_up(FALSE /* setuid */);
	if(startup->seteuid!=NULL)
		startup->seteuid(TRUE);

	status("Initializing");

	memset(text, 0, sizeof(text));
    memset(&scfg, 0, sizeof(scfg));

	lastuseron[0]=0;

	char compiler[32];
	DESCRIBE_COMPILER(compiler);

	lprintf(LOG_INFO,"%s Version %s%c%s"
		,TELNET_SERVER
		,VERSION
		,REVISION
#ifdef _DEBUG
		," Debug"
#else
		,""
#endif
		);
	lprintf(LOG_INFO,"Compiled %s/%s %s %s with %s", git_branch, git_hash, __DATE__, __TIME__, compiler);

#ifdef _DEBUG
	lprintf(LOG_DEBUG, "sizeof: int=%d, long=%d, off_t=%d, time_t=%d"
		,(int)sizeof(int), (int)sizeof(long), (int)sizeof(off_t), (int)sizeof(time_t));

	memset(str, 0xff, sizeof(str));
	snprintf(str, 2, "\x01\x02");
	if(str[1] != '\0') {
		lprintf(LOG_CRIT, "Non-terminating (unsafe) snprintf function detected (0x%02X instead of 0x00)", (uchar)str[1]);
		cleanup(1);
		return;
	}
#endif

    if(startup->first_node<1 || startup->first_node>startup->last_node) {
    	lprintf(LOG_CRIT,"!ILLEGAL node configuration (first: %d, last: %d)"
        	,startup->first_node, startup->last_node);
		cleanup(1);
        return;
    }

#ifdef __BORLANDC__
	#pragma warn -8008	/* Disable "Condition always false" warning */
	#pragma warn -8066	/* Disable "Unreachable code" warning */
#endif
	if(sizeof(node_t)!=SIZEOF_NODE_T) {
		lprintf(LOG_CRIT,"!COMPILER ERROR: sizeof(node_t)=%d instead of %d"
			,(int)sizeof(node_t),SIZEOF_NODE_T);
		cleanup(1);
		return;
	}

	if(!winsock_startup()) {
		cleanup(1);
		return;
	}

	t=time(NULL);
	lprintf(LOG_INFO,"Initializing on %.24s with options: %x"
		,ctime_r(&t,str),startup->options);

	if(chdir(startup->ctrl_dir)!=0)
		lprintf(LOG_ERR,"!ERROR %d (%s) changing directory to: %s", errno, strerror(errno), startup->ctrl_dir);

	/* Initial configuration and load from CNF files */
    SAFECOPY(scfg.ctrl_dir,startup->ctrl_dir);
    lprintf(LOG_INFO,"Loading configuration files from %s", scfg.ctrl_dir);
	scfg.size=sizeof(scfg);
	scfg.node_num=startup->first_node;
	SAFECOPY(logstr,UNKNOWN_LOAD_ERROR);
	if(!load_cfg(&scfg, text, /* prep: */TRUE, /* node_req: */TRUE, logstr, sizeof(logstr))) {
		lprintf(LOG_CRIT,"!ERROR %s",logstr);
		lprintf(LOG_CRIT,"!FAILED to load configuration files");
		cleanup(1);
		return;
	}

	if((t=checktime())!=0) {   /* Check binary time */
		lprintf(LOG_ERR,"!TIME PROBLEM (%ld)",t);
	}

	if(smb_tzutc(sys_timezone(&scfg)) != xpTimeZone_local()) {
		lprintf(LOG_WARNING,"Configured time zone (%s, 0x%04hX, offset: %d) does not match system-local time zone offset: %d"
			,smb_zonestr(scfg.sys_timezone,str), scfg.sys_timezone, smb_tzutc(scfg.sys_timezone), xpTimeZone_local());
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
		if(i) {
			int err;
			if((err = md(scfg.node_path[i-1])) != 0) {
				lprintf(LOG_CRIT,"!ERROR %d (%s) creating directory: %s", err, strerror(err), scfg.node_path[i-1]);
				cleanup(1);
				return;
			}
		}
		SAFEPRINTF(str,"%sdsts.dab",i ? scfg.node_path[i-1] : scfg.ctrl_dir);
		if(flength(str)<DSTSDABLEN) {
			if((file=sopen(str,O_WRONLY|O_CREAT|O_APPEND, SH_DENYNO, DEFFILEMODE))==-1) {
				lprintf(LOG_CRIT,"!ERROR %d (%s) creating %s",errno, strerror(errno), str);
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
#endif
	}

	startup->node_inbuf=node_inbuf;

	/* open a socket and wait for a client */
	ts_set = xpms_create(startup->bind_retry_count, startup->bind_retry_delay, lprintf);
	if(ts_set==NULL) {
		lprintf(LOG_CRIT,"!ERROR %d creating Terminal Server socket set", ERROR_VALUE);
		cleanup(1);
		return;
	}
	if (!(startup->options & BBS_OPT_NO_TELNET)) {
		telnet_cb.protocol="telnet";
		telnet_cb.startup=startup;

		/*
		 * Add interfaces
		 */
		xpms_add_list(ts_set, PF_UNSPEC, SOCK_STREAM, 0, startup->telnet_interfaces, startup->telnet_port, "Telnet Server", sock_cb, startup->seteuid, &telnet_cb);
	}

	if(startup->options&BBS_OPT_ALLOW_RLOGIN) {
		/* open a socket and wait for a client */
		rlogin_cb.protocol="rlogin";
		rlogin_cb.startup=startup;
		xpms_add_list(ts_set, PF_UNSPEC, SOCK_STREAM, 0, startup->rlogin_interfaces, startup->rlogin_port, "RLogin Server", sock_cb, startup->seteuid, &rlogin_cb);
	}

#ifdef USE_CRYPTLIB
#if CRYPTLIB_VERSION < 3300 && CRYPTLIB_VERSION > 999
	#warning This version of Cryptlib is known to crash Synchronet.  Upgrade to at least version 3.3 or do not build with Cryptlib support.
#endif
	if(startup->options&BBS_OPT_ALLOW_SSH) {
		CRYPT_KEYSET	ssh_keyset;

		if(!do_cryptInit()) {
			lprintf(LOG_ERR, "SSH cryptInit failure");
			goto NO_SSH;
		}
		/* Get the private key... first try loading it from a file... */
		SAFEPRINTF2(str,"%s%s",scfg.ctrl_dir,"cryptlib.key");
		if(cryptStatusOK(cryptKeysetOpen(&ssh_keyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE, str, CRYPT_KEYOPT_READONLY))) {
			if(cryptStatusError(i=cryptGetPrivateKey(ssh_keyset, &ssh_context, CRYPT_KEYID_NAME, "ssh_server", scfg.sys_pass))) {
				GCESNN(i, ssh_keyset, "getting private key");
				goto NO_SSH;
			}
		}
		else {
			/* Couldn't do that... create a new context and use the key from there... */

			if(cryptStatusError(i=cryptCreateContext(&ssh_context, CRYPT_UNUSED, CRYPT_ALGO_RSA))) {
				GCESNN(i, CRYPT_UNUSED, "creating context");
				goto NO_SSH;
			}
			if(cryptStatusError(i=cryptSetAttributeString(ssh_context, CRYPT_CTXINFO_LABEL, "ssh_server", 10))) {
				GCESNN(i, ssh_context, "setting label");
				goto NO_SSH;
			}
			if(cryptStatusError(i=cryptGenerateKey(ssh_context))) {
				GCESNN(i, ssh_context, "generating key");
				goto NO_SSH;
			}

			/* Ok, now try saving this one... use the syspass to encrypt it. */
			if(cryptStatusOK(i=cryptKeysetOpen(&ssh_keyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE, str, CRYPT_KEYOPT_CREATE))) {
				if(cryptStatusError(i=cryptAddPrivateKey(ssh_keyset, ssh_context, scfg.sys_pass)))
					GCESNN(i, ssh_keyset, "adding private key");
				if(cryptStatusError(i=cryptKeysetClose(ssh_keyset)))
					GCESNN(i, ssh_keyset, "closing keyset");
			}
			else
				GCESNN(i, CRYPT_UNUSED, "creating keyset");
		}

		/* open a socket and wait for a client */
		ssh_cb.protocol="ssh";
		ssh_cb.startup=startup;
		xpms_add_list(ts_set, PF_UNSPEC, SOCK_STREAM, 0, startup->ssh_interfaces, startup->ssh_port, "SSH Server", sock_cb, startup->seteuid, &ssh_cb);
	}
NO_SSH:
#endif

	sbbs = new sbbs_t(0, &server_addr, sizeof(server_addr)
		,"Terminal Server", ts_set->socks[0].sock, &scfg, text, NULL);
	if(sbbs->init()==false) {
		lputs(LOG_CRIT,"!BBS initialization failed");
		cleanup(1);
		return;
	}
    sbbs->output_thread_running = true;
	_beginthread(output_thread, 0, sbbs);

	if(!(startup->options&BBS_OPT_NO_EVENTS)) {
		events = new sbbs_t(0, &server_addr, sizeof(server_addr)
			,"BBS Events", INVALID_SOCKET, &scfg, text, NULL, true);
		if(events->init()==false) {
			lputs(LOG_CRIT,"!Events initialization failed");
			cleanup(1);
			return;
		}
		events->event_thread_running = true;
		_beginthread(event_thread, 0, events);
	}

	/* Save these values in case they're changed dynamically */
	first_node=startup->first_node;
	last_node=startup->last_node;

	for(i=first_node;i<=last_node;i++) {
		sbbs->getnodedat(i,&node,1);
		node.status=NODE_WFC;
		node.misc&=NODE_EVENT;	/* Note: Turns-off NODE_RRUN flag (and others) */
		node.action=0;
		sbbs->putnodedat(i,&node);
	}

	status(STATUS_WFC);

	/* Setup recycle/shutdown semaphore file lists */
	shutdown_semfiles = semfile_list_init(scfg.ctrl_dir,"shutdown", "term");
	recycle_semfiles = semfile_list_init(scfg.ctrl_dir,"recycle", "term");
	clear_attempts_semfiles = semfile_list_init(scfg.ctrl_dir,"clear", "term");
	semfile_list_add(&recycle_semfiles,startup->ini_fname);
	SAFEPRINTF(str,"%stext.dat",scfg.ctrl_dir);
	semfile_list_add(&recycle_semfiles,str);
	SAFEPRINTF(str,"%sattr.cfg",scfg.ctrl_dir);
	semfile_list_add(&recycle_semfiles,str);
	if(!initialized)
		semfile_list_check(&initialized,shutdown_semfiles);
	semfile_list_check(&initialized,recycle_semfiles);
	semfile_list_check(&initialized,clear_attempts_semfiles);

	listInit(&current_logins, LINK_LIST_MUTEX);
	listInit(&current_connections, LINK_LIST_MUTEX);

#ifdef __unix__	//	unix-domain spy sockets
	for(i=first_node;i<=last_node && !(startup->options&BBS_OPT_NO_SPY_SOCKETS);i++)  {
	    if((unsigned int)snprintf(str,sizeof(uspy_addr.un.sun_path),
	            "%slocalspy%d.sock", startup->temp_dir, i)
	            >=sizeof(uspy_addr.un.sun_path)) {
			lprintf(LOG_ERR,"Node %d !ERROR local spy socket path \"%slocalspy%d.sock\" too long."
				, i, startup->temp_dir, i);
			continue;
		}
	    else  {
			if(xpms_add(ts_set, PF_UNIX, SOCK_STREAM, 0, str, 0, "Spy Socket", sock_cb, NULL, &uspy_cb[i-1]))
				lprintf(LOG_INFO,"Node %d local spy using socket %s", i, str);
			else
				lprintf(LOG_ERR,"Node %d !ERROR %d (%s) creating local spy socket %s"
					, i, errno, strerror(errno), str);
	    }
	}
#endif // __unix__ (unix-domain spy sockets)

	/* signal caller that we've started up successfully */
	if(startup->started!=NULL)
		startup->started(startup->cbdata);

	lprintf(LOG_INFO,"Terminal Server thread started for nodes %d through %d", first_node, last_node);

	while(!terminate_server) {
		YIELD();
		if(protected_uint32_value(node_threads_running)==0) {	/* check for re-run flags and recycle/shutdown sem files */
			if(!(startup->options&BBS_OPT_NO_RECYCLE)) {
				if((p=semfile_list_check(&initialized,recycle_semfiles))!=NULL) {
					lprintf(LOG_INFO,"Recycle semaphore file (%s) detected"
						,p);
					break;
				}
				if(startup->recycle_now==TRUE) {
					lprintf(LOG_INFO,"Recycle semaphore signaled");
					startup->recycle_now=FALSE;
					break;
				}
			}
			if(((p=semfile_list_check(&initialized,shutdown_semfiles))!=NULL
					&& lprintf(LOG_INFO,"Shutdown semaphore file (%s) detected"
						,p))
				|| (startup->shutdown_now==TRUE
					&& lprintf(LOG_INFO,"Shutdown semaphore signaled"))) {
				startup->shutdown_now=FALSE;
				terminate_server=TRUE;
				break;
			}
		}

    	sbbs->online=FALSE;
//		sbbs->client_socket=INVALID_SOCKET;
#ifdef USE_CRYPTLIB
		sbbs->ssh_mode=false;
#endif

		/* now wait for connection */
		client_addr_len = sizeof(client_addr);
		client_socket=xpms_accept(ts_set, &client_addr
	        	,&client_addr_len, startup->sem_chk_freq*1000, (startup->options & BBS_OPT_HAPROXY_PROTO) ? XPMS_ACCEPT_FLAG_HAPROXY : XPMS_FLAGS_NONE, &ts_cb);

		if(terminate_server)	/* terminated */
			break;

		if((p=semfile_list_check(&initialized,clear_attempts_semfiles))!=NULL) {
			lprintf(LOG_INFO,"Clear Failed Login Attempts semaphore file (%s) detected", p);
			loginAttemptListClear(startup->login_attempt_list);
		}

		if(client_socket == INVALID_SOCKET)
			continue;

		bool rlogin = false;
#ifdef USE_CRYPTLIB
		bool ssh = false;
#endif

		is_client=FALSE;
		if(ts_cb == &telnet_cb) {
	        is_client=TRUE;
		} else if(ts_cb == &rlogin_cb) {
			rlogin = true;
			is_client=TRUE;
#ifdef USE_CRYPTLIB
		} else if(ts_cb == &ssh_cb) {
			ssh = true;
			is_client=TRUE;
#endif
		} else {
#ifdef __unix__
			for(i=first_node;i<=last_node;i++)  {
				if(&uspy_cb[i-1] == ts_cb) {
					if(node_socket[i-1]==INVALID_SOCKET)
						read(uspy_socket[i-1],str,sizeof(str));
					if(!socket_check(uspy_socket[i-1],NULL,NULL,0)) {
						lprintf(LOG_NOTICE,"Spy socket for node %d disconnected",i);
						close_socket(uspy_socket[i-1]);
						uspy_socket[i-1]=INVALID_SOCKET;
					}
				}
				if(&uspy_cb[i-1] == ts_cb) {
					BOOL already_connected=(uspy_socket[i-1]!=INVALID_SOCKET);
					SOCKET new_socket=client_socket;
					fcntl(new_socket,F_SETFL,fcntl(new_socket,F_GETFL)|O_NONBLOCK);
					if(already_connected)  {
						lprintf(LOG_ERR,"!ERROR Spy socket %s already in use",uspy_addr.un.sun_path);
						send(new_socket,"Spy socket already in use.\r\n",27,0);
						close_socket(new_socket);
					}
					else  {
						lprintf(LOG_ERR,"!Spy socket %s (%d) connected",uspy_addr.un.sun_path,new_socket);
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

		// Count the socket:
		call_socket_open_callback(TRUE);

		if(client_socket == INVALID_SOCKET)	{
#if 0	/* is this necessary still? */
			if(ERROR_VALUE == ENOTSOCK || ERROR_VALUE == EINTR || ERROR_VALUE == EINVAL) {
            	lputs(LOG_NOTICE,"BBS socket closed");
				break;
			}
#endif
			lprintf(LOG_ERR,"!ERROR %d accepting connection", ERROR_VALUE);
#ifdef _WIN32
			if(WSAGetLastError()==WSAENOBUFS) {	/* recycle (re-init WinSock) on this error */
				break;
			}
#endif
			continue;
		}
		char host_ip[INET6_ADDRSTRLEN];

		inet_addrtop(&client_addr, host_ip, sizeof(host_ip));

		if(trashcan(&scfg,host_ip,"ip-silent")) {
			close_socket(client_socket);
			continue;
		}

#ifdef USE_CRYPTLIB
		client.protocol=rlogin ? "RLogin":(ssh ? "SSH" : "Telnet");
#else
		client.protocol=rlogin ? "RLogin":"Telnet";
#endif
		lprintf(LOG_INFO,"%04d %s connection accepted from: %s port %u"
			,client_socket, client.protocol
			, host_ip, inet_addrport(&client_addr));

		if(startup->max_concurrent_connections > 0) {
			int ip_len = strlen(host_ip)+1;
			int connections = listCountMatches(&current_connections, host_ip, ip_len);
			int logins = listCountMatches(&current_logins, host_ip, ip_len);

			if(connections - logins >= (int)startup->max_concurrent_connections
				&& !is_host_exempt(&scfg, host_ip, /* host_name */NULL)) {
				lprintf(LOG_NOTICE, "%04d %s !Maximum concurrent connections without login (%u) reached from host: %s"
 					,client_socket, client.protocol, startup->max_concurrent_connections, host_ip);
				close_socket(client_socket);
				continue;
			}
		}

		sbbs->client_socket=client_socket;	// required for output to the user
		if(!ssh)
			sbbs->online=ON_REMOTE;

		login_attempt_t attempted;
		ulong banned = loginBanned(&scfg, startup->login_attempt_list, client_socket, /* host_name: */NULL, startup->login_attempt, &attempted);
		if(banned || sbbs->trashcan(host_ip,"ip")) {
			if(banned) {
				char ban_duration[128];
				lprintf(LOG_NOTICE, "%04d %s !TEMPORARY BAN of %s (%lu login attempts%s%s) - remaining: %s"
					,client_socket, client.protocol, host_ip, attempted.count-attempted.dupes
					,attempted.user[0] ? ", last: " : "", attempted.user, seconds_to_str(banned, ban_duration));
			} else
				lprintf(LOG_NOTICE,"%04d %s !CLIENT BLOCKED in ip.can: %s", client_socket, client.protocol, host_ip);
			close_socket(client_socket);
			continue;
		}

#ifdef _WIN32
		if(startup->sound.answer[0] && !sound_muted(&scfg))
			PlaySound(startup->sound.answer, NULL, SND_ASYNC|SND_FILENAME);
#endif

		/* Purge (flush) any pending input or output data */
		sbbs->rioctl(IOFB);

		/* Do SSH stuff here */
#ifdef USE_CRYPTLIB
		if(ssh) {
			int	ssh_failed=0;
			BOOL nodelay = TRUE;
			ulong nb = 0;

			if(cryptStatusError(i=cryptCreateSession(&sbbs->ssh_session, CRYPT_UNUSED, CRYPT_SESSION_SSH_SERVER))) {
				GCESS(i, client_socket, CRYPT_UNUSED, "creating SSH session");
				close_socket(client_socket);
				continue;
			}
			lprintf(LOG_DEBUG, "%04d SSH Cryptlib Session: %d created", client_socket, sbbs->ssh_session);
			protected_uint32_adjust(&ssh_sessions, 1);
			sbbs->ssh_mode = true;

			if(cryptStatusError(i=cryptSetAttribute(sbbs->ssh_session, CRYPT_OPTION_NET_CONNECTTIMEOUT, startup->ssh_connect_timeout)))
				GCESS(i, client_socket, sbbs->ssh_session, "setting connect timeout");

			if(cryptStatusError(i=cryptSetAttribute(sbbs->ssh_session, CRYPT_SESSINFO_PRIVATEKEY, ssh_context))) {
				GCESS(i, client_socket, sbbs->ssh_session, "setting private key");
				SSH_END(client_socket);
				close_socket(client_socket);
				continue;
			}
			setsockopt(client_socket,IPPROTO_TCP,TCP_NODELAY,(char*)&nodelay,sizeof(nodelay));
			ioctlsocket(client_socket,FIONBIO,&nb);
			if(cryptStatusError(i=cryptSetAttribute(sbbs->ssh_session, CRYPT_SESSINFO_NETWORKSOCKET, client_socket))) {
				GCESS(i, client_socket, sbbs->ssh_session, "setting network socket");
				SSH_END(client_socket);
				close_socket(client_socket);
				continue;
			}
			for(ssh_failed=0; ssh_failed < 2; ssh_failed++) {
				/* Accept any credentials */
				lprintf(LOG_DEBUG, "%04d SSH Setting attribute: SESSINFO_AUTHRESPONSE", client_socket);
				if(cryptStatusError(i=cryptSetAttribute(sbbs->ssh_session, CRYPT_SESSINFO_AUTHRESPONSE, 1))) {
					GCESS(i, client_socket, sbbs->ssh_session, "setting auth response");
					ssh_failed=1;
					break;
				}
				lprintf(LOG_DEBUG, "%04d SSH Setting attribute: SESSINFO_ACTIVE", client_socket);
				if(cryptStatusError(i=cryptSetAttribute(sbbs->ssh_session, CRYPT_SESSINFO_ACTIVE, 1))) {
					GCESS(i, client_socket, sbbs->ssh_session, "setting session active");
					if(i != CRYPT_ENVELOPE_RESOURCE) {
						ssh_failed=2;
						break;
					}
				}
				else {
					int cid;
					char tname[1024];
					int tnamelen;

					ssh_failed=0;
					// Check the channel ID and name...
					if (cryptStatusOK(i=cryptGetAttribute(sbbs->ssh_session, CRYPT_SESSINFO_SSH_CHANNEL, &cid))) {
						if (i == CRYPT_OK) {
							tnamelen = 0;
							i=cryptGetAttributeString(sbbs->ssh_session, CRYPT_SESSINFO_SSH_CHANNEL_TYPE, tname, &tnamelen);
							GCESS(i, client_socket, sbbs->ssh_session, "getting channel type");
							if (tnamelen != 7 || strnicmp(tname, "session", 7)) {
								lprintf(LOG_NOTICE, "%04d SSH active channel '%.*s' is not 'session', disconnecting.", client_socket, tnamelen, tname);
								sbbs->badlogin(/* user: */NULL, /* passwd: */NULL, "SSH", &client_addr, /* delay: */false);
								// Fail because there's no session.
								ssh_failed = 3;
							}
							else
								sbbs->session_channel = cid;
						}
					}
					else {
						GCESS(i, client_socket, sbbs->ssh_session, "getting channel id");
						if (i == CRYPT_ERROR_PERMISSION)
							lprintf(LOG_ERR, "!ERROR Your cryptlib build is obsolete, please update");
					}
					break;
				}
			}
			if (!ssh_failed) {
				if(cryptStatusError(i=cryptSetAttribute(sbbs->ssh_session, CRYPT_PROPERTY_OWNER, CRYPT_UNUSED))) {
					GCESS(i, client_socket, sbbs->ssh_session, "clearing owner");
					ssh_failed = 2;
				}
			}
			if(ssh_failed) {
				lprintf(LOG_NOTICE, "%04d SSH session establishment failed", client_socket);
				SSH_END(client_socket);
				close_socket(client_socket);
				continue;
			}
			if(cryptStatusError(i=cryptSetAttribute(sbbs->ssh_session, CRYPT_OPTION_NET_READTIMEOUT, 0)))
				GCESS(i, sbbs->client_socket, sbbs->ssh_session, "setting read timeout");
			// READ = WRITE TIMEOUT HACK... REMOVE WHEN FIXED
			if(cryptStatusError(i=cryptSetAttribute(sbbs->ssh_session, CRYPT_OPTION_NET_WRITETIMEOUT, 0)))
				GCESS(i, sbbs->client_socket, sbbs->ssh_session, "setting write timeout");
#if 0
			if(cryptStatusError(err=crypt_pop_channel_data(sbbs, str, sizeof(str), &i))) {
				GCES(i, sbbs->cfg.node_num, sbbs->ssh_session, "popping data");
				i=0;
			}
#endif
			sbbs->online=ON_REMOTE;
		}
#endif

		if(rlogin)
			sbbs->outcom(0); /* acknowledge RLogin per RFC 1282 */

		sbbs->autoterm=0;
		sbbs->cols = TERM_COLS_DEFAULT;
		SOCKADDR_IN local_addr;
		memset(&local_addr, 0, sizeof(local_addr));
		socklen_t addr_len=sizeof(local_addr);
		if(getsockname(client_socket, (struct sockaddr *)&local_addr, &addr_len) == 0
			&& (ntohs(local_addr.sin_port) == startup->pet40_port
				|| ntohs(local_addr.sin_port) == startup->pet80_port)) {
			sbbs->autoterm = PETSCII;
			sbbs->cols = ntohs(local_addr.sin_port) == startup->pet40_port ? 40 : 80;
			sbbs->outcom(PETSCII_UPPERLOWER);
		}

		sbbs->bprintf("\r\n%s\r\n", VERSION_NOTICE);
		sbbs->bprintf("%s connection from: %s\r\n", client.protocol, host_ip);

		SAFECOPY(host_name, STR_NO_HOSTNAME);
		if(!(startup->options&BBS_OPT_NO_HOST_LOOKUP)) {
			sbbs->bprintf("Resolving hostname...");
			getnameinfo(&client_addr.addr, client_addr_len, host_name, sizeof(host_name), NULL, 0, NI_NAMEREQD);
			sbbs->putcom(crlf);
			lprintf(LOG_INFO,"%04d %s Hostname: %s [%s]", client_socket, client.protocol, host_name, host_ip);
		}

		if(sbbs->trashcan(host_name,"host")) {
			SSH_END(client_socket);
			close_socket(client_socket);
			lprintf(LOG_NOTICE,"%04d %s !CLIENT BLOCKED in host.can: %s"
				,client_socket, client.protocol, host_name);
			continue;
		}

		identity=NULL;
		if(startup->options&BBS_OPT_GET_IDENT) {
			sbbs->bprintf("Resolving identity...");
			/* ToDo: Make ident timeout configurable */
			if(identify(&client_addr, inet_addrport(&client_addr), str, sizeof(str)-1, /* timeout: */1)) {
				lprintf(LOG_DEBUG,"%04d %s Ident Response: %s",client_socket, client.protocol, str);
				identity=strrchr(str,':');
				if(identity!=NULL) {
					identity++;	/* skip colon */
					SKIP_WHITESPACE(identity);
					if(*identity)
						lprintf(LOG_INFO,"%04d %s Identity: %s",client_socket, client.protocol, identity);
				}
			}
			sbbs->putcom(crlf);
		}
		/* Initialize client display */
		client.size=sizeof(client);
		client.time=time32(NULL);
		SAFECOPY(client.addr,host_ip);
		SAFECOPY(client.host,host_name);
		client.port=inet_addrport(&client_addr);
		client.user=STR_UNKNOWN_USER;
		client.usernum = 0;
		client_on(client_socket,&client,FALSE /* update */);

		for(i=first_node;i<=last_node;i++) {
			/* paranoia: make sure node.status!=NODE_WFC by default */
			node.status=NODE_INVALID_STATUS;
			if(sbbs->getnodedat(i,&node,1)!=0)
				continue;
			if(node.status==NODE_WFC) {
				node.status=NODE_LOGON;
#ifdef USE_CRYPTLIB
				if(ssh)
					node.connection=NODE_CONNECTION_SSH;
				else
#endif
				if(rlogin)
					node.connection=NODE_CONNECTION_RLOGIN;
				else
					node.connection=NODE_CONNECTION_TELNET;

				sbbs->putnodedat(i,&node);
				break;
			}
			sbbs->putnodedat(i,&node);
		}

		if(i>last_node) {
			lprintf(LOG_WARNING,"%04d %s !No nodes available for login.", client_socket, client.protocol);
			SAFEPRINTF(str,"%snonodes.txt",scfg.text_dir);
			if(fexist(str))
				sbbs->printfile(str,P_NOABORT);
			else {
				sbbs->putcom("\r\nSorry, all terminal nodes are in use or otherwise unavailable.\r\n");
				sbbs->putcom("Please try again later.\r\n");
			}
			mswait(3000);
			client_off(client_socket);
			SSH_END(client_socket);
			close_socket(client_socket);
			continue;
		}

		// Load the configuration files for this node, only if/when needed/updated
		scfg_t* cfg = &node_scfg[i - 1];
		if(cfg->size != sizeof(*cfg) || (node.misc & NODE_RRUN)) {
			sbbs->bprintf("Loading configuration...");
			cfg->size = sizeof(*cfg);
			cfg->node_num = i;
		    SAFECOPY(cfg->ctrl_dir, startup->ctrl_dir);
			lprintf(LOG_INFO,"Node %d Loading configuration files from %s", cfg->node_num, cfg->ctrl_dir);
			SAFECOPY(logstr,UNKNOWN_LOAD_ERROR);
			if(!load_cfg(cfg, node_text[i - 1], /* prep: */TRUE, /* node_req: */TRUE, logstr, sizeof(logstr))) {
				lprintf(LOG_WARNING, "Node %d LOAD ERROR: %s, falling back to Node %d", cfg->node_num, logstr, first_node);
				cfg->node_num = first_node;
				if(!load_cfg(cfg, node_text[i - 1], /* prep: */TRUE, /* node: */TRUE, logstr, sizeof(logstr))) {
					lprintf(LOG_CRIT,"!ERROR %s",logstr);
					lprintf(LOG_CRIT,"!FAILED to load configuration files");
					sbbs->bprintf("\r\nFAILED: %s", logstr);
					client_off(client_socket);
					SSH_END(client_socket);
					close_socket(client_socket);
					sbbs->getnodedat(cfg->node_num,&node,true);
					node.status = NODE_WFC;
					sbbs->putnodedat(cfg->node_num,&node);
					continue;
				}
				cfg->node_num = i; // correct the node number
			}
			if(node.misc & NODE_RRUN) {
				sbbs->getnodedat(cfg->node_num,&node,true);
				node.misc &= ~NODE_RRUN;
				sbbs->putnodedat(cfg->node_num,&node);
			}
			sbbs->bputs(crlf);
		}
		// Copy event last-run info from global config
		for(int e=0; e < cfg->total_events && e < scfg.total_events; e++)
			cfg->event[e]->last = scfg.event[e]->last;

        node_socket[i-1]=client_socket;

		sbbs_t* new_node = new sbbs_t(/* node_num: */i, &client_addr, client_addr_len, host_name
        	,client_socket
			,cfg, node_text[i-1], &client);

		new_node->client=client;
#ifdef USE_CRYPTLIB
		if(ssh) {
			new_node->ssh_session=sbbs->ssh_session;	// This is done again later, after NO_PASSTHRU: Why?
			new_node->ssh_mode=true;
			new_node->session_channel = sbbs->session_channel;
		}
#endif

		/* copy the IDENT response, if any */
		if(identity!=NULL)
			SAFECOPY(new_node->client_ident,identity);

		if(new_node->init()==false) {
			lprintf(LOG_INFO,"%04d %s Node %d !Initialization failure"
				,client_socket, client.protocol, new_node->cfg.node_num);
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
			SSH_END(client_socket);
			close_socket(client_socket);
			continue;
		}

		if(rlogin==true) {
			SAFECOPY(new_node->connection,"RLogin");
			new_node->node_connection=NODE_CONNECTION_RLOGIN;
			new_node->sys_status|=SS_RLOGIN;
			new_node->telnet_mode|=TELNET_MODE_OFF; // RLogin does not use Telnet commands
		}

		// Passthru socket creation/connection
		if(true) {
			/* TODO: IPv6? */
			SOCKET	tmp_sock;
			SOCKADDR_IN		tmp_addr={0};
			socklen_t		tmp_addr_len;
			char addr_str[INET6_ADDRSTRLEN] = "";

    		/* open a socket and connect to yourself */

    		tmp_sock = open_socket(PF_INET, SOCK_STREAM, "passthru");

			if(tmp_sock == INVALID_SOCKET) {
				lprintf(LOG_ERR,"Node %d !ERROR %d creating passthru listen socket"
					,new_node->cfg.node_num, ERROR_VALUE);
				goto NO_PASSTHRU;
			}

    		lprintf(LOG_DEBUG,"Node %d passthru listen socket %d opened"
				,new_node->cfg.node_num, tmp_sock);

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
				lprintf(LOG_ERR,"Node %d !ERROR %d (%d) listening on passthru socket"
					,new_node->cfg.node_num, result, ERROR_VALUE);
				close_socket(tmp_sock);
				goto NO_PASSTHRU;
			}

			tmp_addr_len=sizeof(tmp_addr);
			if(getsockname(tmp_sock, (struct sockaddr *)&tmp_addr, &tmp_addr_len)) {
				lprintf(LOG_ERR,"Node %d !ERROR %d getting passthru listener address"
					,new_node->cfg.node_num, ERROR_VALUE);
				close_socket(tmp_sock);
				goto NO_PASSTHRU;
			}
			lprintf(LOG_DEBUG,"Node %d passthru socket listening on port %u"
				,new_node->cfg.node_num, htons(tmp_addr.sin_port));

    		new_node->passthru_socket = open_socket(PF_INET, SOCK_STREAM, "passthru");

			if(new_node->passthru_socket == INVALID_SOCKET) {
				lprintf(LOG_ERR,"Node %d !ERROR %d creating passthru connecting socket"
					,new_node->cfg.node_num, ERROR_VALUE);
				close_socket(tmp_sock);
				goto NO_PASSTHRU;
			}

			lprintf(LOG_DEBUG,"Node %d passthru connect socket %d opened"
				,new_node->cfg.node_num, new_node->passthru_socket);

			result = connect(new_node->passthru_socket, (struct sockaddr *)&tmp_addr, tmp_addr_len);

			if(result != 0) {
				inet_ntop(tmp_addr.sin_family, &tmp_addr.sin_addr, addr_str, sizeof(addr_str));
				lprintf(LOG_ERR,"Node %d !ERROR %d (%d) connecting to passthru socket: %s port %u"
					,new_node->cfg.node_num, result, ERROR_VALUE, addr_str, htons(tmp_addr.sin_port));
				close_socket(new_node->passthru_socket);
				new_node->passthru_socket=INVALID_SOCKET;
				close_socket(tmp_sock);
				goto NO_PASSTHRU;
			}

			new_node->client_socket_dup=accept(tmp_sock, (struct sockaddr *)&tmp_addr, &tmp_addr_len);

			if(new_node->client_socket_dup == INVALID_SOCKET) {
				lprintf(LOG_ERR,"Node %d !ERROR (%d) accepting on passthru socket"
					,new_node->cfg.node_num, ERROR_VALUE);
				lprintf(LOG_WARNING,"Node %d !WARNING native doors which use sockets will not function"
					,new_node->cfg.node_num);
				close_socket(new_node->passthru_socket);
				new_node->passthru_socket=INVALID_SOCKET;
				close_socket(tmp_sock);
				goto NO_PASSTHRU;
			}
			close_socket(tmp_sock);
			new_node->passthru_thread_running = true;
			_beginthread(passthru_thread, 0, new_node);

	NO_PASSTHRU:
			if(ssh) {
				SAFECOPY(new_node->connection,"SSH");
				new_node->node_connection=NODE_CONNECTION_SSH;
				new_node->sys_status|=SS_SSH;
				new_node->telnet_mode|=TELNET_MODE_OFF; // SSH does not use Telnet commands
				new_node->ssh_session=sbbs->ssh_session;
			}
			/* Wait for pending data to be sent then turn off ssh_mode for uber-output */
			while(sbbs->output_thread_running && RingBufFull(&sbbs->outbuf))
				SLEEP(1);
			pthread_mutex_lock(&sbbs->ssh_mutex);
			sbbs->ssh_mode=false;
			sbbs->ssh_session=0; // Don't allow subsequent SSH connections to affect this one (!)
			pthread_mutex_unlock(&sbbs->ssh_mutex);
		}

	    protected_uint32_adjust(&node_threads_running, 1);
	    new_node->input_thread_running = true;
		new_node->input_thread=(HANDLE)_beginthread(input_thread,0, new_node);
	    new_node->output_thread_running = true;
		new_node->autoterm = sbbs->autoterm;
		new_node->cols = sbbs->cols;
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
		if(uspy_socket[i]!=INVALID_SOCKET) {
			close_socket(uspy_socket[i]);
			uspy_socket[i]=INVALID_SOCKET;
		}
		snprintf(str,sizeof(uspy_addr.un.sun_path),"%slocalspy%d.sock", startup->temp_dir, i+1);
		if(fexist(str))
			unlink(str);
#endif
	}

	sbbs->client_socket=INVALID_SOCKET;
	if(events!=NULL)
		events->terminated=true;
    // Wake-up BBS output thread so it can terminate
    sem_post(&sbbs->outbuf.sem);

    // Wait for all node threads to terminate
	if(protected_uint32_value(node_threads_running)) {
		lprintf(LOG_INFO,"Waiting for %d node threads to terminate...", protected_uint32_value(node_threads_running));
		start=time(NULL);
		while(protected_uint32_value(node_threads_running)) {
			if(time(NULL)-start>TIMEOUT_THREAD_WAIT) {
				lprintf(LOG_ERR,"!TIMEOUT waiting for %d node thread(s) to "
            		"terminate", protected_uint32_value(node_threads_running));
				break;
			}
			mswait(100);
		}
		lprintf(LOG_INFO, "Done waiting for node threads to terminate");
	}

	// Wait for Events thread to terminate
	if(events!=NULL && events->event_thread_running) {
		lprintf(LOG_INFO,"Waiting for events thread to terminate...");
		start=time(NULL);
		while(events->event_thread_running) {
#if 0 /* the events thread can/will segfault if it continues to run and dereference sbbs->cfg */
			if(time(NULL)-start>TIMEOUT_THREAD_WAIT) {
				lprintf(LOG_ERR,"!TIMEOUT waiting for BBS events thread to "
            		"terminate");
				break;
			}
#endif
			mswait(100);
		}
		lprintf(LOG_INFO, "Done waiting for events thread to terminate");
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
		lprintf(LOG_INFO, "Done waiting for system output thread to terminate");
	}

    // Wait for BBS passthru thread to terminate
	if(sbbs->passthru_thread_running) {
		lprintf(LOG_INFO,"Waiting for passthru thread to terminate...");
		start=time(NULL);
		while(sbbs->passthru_thread_running) {
			if(time(NULL)-start>TIMEOUT_THREAD_WAIT) {
				lprintf(LOG_ERR,"!TIMEOUT waiting for passthru thread to terminate");
				break;
			}
			mswait(100);
		}
		lprintf(LOG_INFO, "Done waiting for passthru threads to terminate");
	}

    // Set all nodes' status to OFFLINE
    for(i=first_node;i<=last_node;i++) {
        sbbs->getnodedat(i,&node,1);
        node.status=NODE_OFFLINE;
        sbbs->putnodedat(i,&node);
    }

    if(events!=NULL) {
		if(events->event_thread_running)
			lprintf(LOG_ERR,"!Events thread still running, can't delete");
		else
		    delete events;
	}

    if(sbbs->passthru_thread_running || sbbs->output_thread_running)
		lprintf(LOG_ERR,"!System I/O thread still running, can't delete");
	else
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
