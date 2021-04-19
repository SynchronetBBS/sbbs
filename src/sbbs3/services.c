/* Synchronet Services */

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

/* Platform-specific headers */
#ifdef __unix__
	#include <sys/param.h>	/* BSD? */
#endif

/* ANSI C Library headers */
#include <stdio.h>
#include <stdlib.h>			/* ltoa in GNU C lib */
#include <stdarg.h>			/* va_list */
#include <string.h>			/* strrchr */
#include <fcntl.h>			/* Open flags */
#include <errno.h>			/* errno */

/* Synchronet-specific headers */
#ifndef JAVASCRIPT
#define JAVASCRIPT	/* required to include JS API headers */
#endif
#define SERVICES_INI_BITDESC_TABLE	/* required to defined service_options */
#undef SBBS	/* this shouldn't be defined unless building sbbs.dll/libsbbs.so */
#include "sbbs.h"
#include "services.h"
#include "ident.h"	/* identify() */
#include "sbbs_ini.h"
#include "js_rtpool.h"
#include "js_request.h"
#include "js_socket.h"
#include "multisock.h"
#include "ssl.h"
#include "git_branch.h"
#include "git_hash.h"

/* Constants */

#define MAX_UDP_BUF_LEN			8192	/* 8K */
#define DEFAULT_LISTEN_BACKLOG	5

static services_startup_t* startup=NULL;
static scfg_t	scfg;
static char*	text[TOTAL_TEXT];
static volatile BOOL	terminated=FALSE;
static time_t	uptime=0;
static ulong	served=0;
static str_list_t recycle_semfiles;
static str_list_t shutdown_semfiles;
static protected_uint32_t threads_pending_start;

typedef struct {
	/* These are sysop-configurable */
	uint32_t		interface_addr;
	uint16_t		port;
	str_list_t		interfaces;
	struct in_addr		outgoing4;
	struct in6_addr	outgoing6;
	char			protocol[34];
	char			cmd[128];
	uint			max_clients;
	uint32_t		options;
	int				listen_backlog;
	int				log_level;
	uint32_t		stack_size;
	js_startup_t	js;
	js_server_props_t js_server_props;
	/* These are run-time state and stat vars */
	protected_uint32_t clients;
	ulong			served;
	struct xpms_set	*set;
	int				running;
	BOOL			terminated;
} service_t;

typedef struct {
	SOCKET				socket;
	struct xpms_set		*set;
	union xp_sockaddr	addr;
	time_t				logintime;
	user_t				user;
	client_t*			client;
	service_t*			service;
	js_callback_t		callback;
	/* Initial UDP datagram */
	BYTE*				udp_buf;
	int					udp_len;
	subscan_t			*subscan;
	CRYPT_SESSION		tls_sess;
} service_client_t;

static service_t	*service=NULL;
static unsigned int	services=0;

#if defined(__GNUC__)   // Catch printf-format errors with lprintf
static int lprintf(int level, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
#endif
static int lprintf(int level, const char *fmt, ...)
{
	va_list argptr;
	char sbuf[1024];

	va_start(argptr,fmt);
    vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
    va_end(argptr);

	if(level <= LOG_ERR) {
		char errmsg[sizeof(sbuf)+16];
		SAFEPRINTF(errmsg, "srvc %s", sbuf);
		errorlog(&scfg, level, startup==NULL ? NULL:startup->host_name, errmsg);
		if(startup!=NULL && startup->errormsg!=NULL)
			startup->errormsg(startup->cbdata,level,errmsg);
	}

    if(startup==NULL || startup->lputs==NULL || level > startup->log_level)
        return(0);

#if defined(_WIN32)
	if(IsBadCodePtr((FARPROC)startup->lputs))
		return(0);
#endif

    return(startup->lputs(startup->cbdata,level,sbuf));
}

#ifdef _WINSOCKAPI_

static WSADATA WSAData;
#define SOCKLIB_DESC WSAData.szDescription
static BOOL WSAInitialized=FALSE;

static BOOL winsock_startup(void)
{
	int		status;             /* Status Code */

    if((status = WSAStartup(MAKEWORD(1,1), &WSAData))==0) {
		lprintf(LOG_DEBUG,"%s %s",WSAData.szDescription, WSAData.szSystemStatus);
		WSAInitialized=TRUE;
		return (TRUE);
	}

    lprintf(LOG_CRIT,"!WinSock startup ERROR %d", status);
	return (FALSE);
}

#else /* No WINSOCK */

#define winsock_startup()	(TRUE)
#define SOCKLIB_DESC NULL

#endif

static char* server_host_name(void)
{
	return startup->host_name[0] ? startup->host_name : scfg.sys_inetaddr;
}

static ulong active_clients(void)
{
	ulong i;
	ulong total_clients=0;

	for(i=0;i<services;i++) 
		total_clients += protected_uint32_value(service[i].clients);

	return(total_clients);
}

static void update_clients(void)
{
	if(startup!=NULL && startup->clients!=NULL)
		startup->clients(startup->cbdata,active_clients());
}

static void client_on(SOCKET sock, client_t* client, BOOL update)
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
	if(startup!=NULL && startup->thread_up!=NULL)
		startup->thread_up(startup->cbdata,TRUE,setuid);
}

static void thread_down(void)
{
	if(startup!=NULL && startup->thread_up!=NULL)
		startup->thread_up(startup->cbdata,FALSE,FALSE);
}

void open_socket_cb(SOCKET sock, void *serv_ptr)
{
	char	error[256];
	char	section[128];
	service_t	*serv=(service_t *)serv_ptr;

	if(startup!=NULL && startup->socket_open!=NULL)
		startup->socket_open(startup->cbdata,TRUE);
	SAFEPRINTF(section,"services|%s", serv->protocol);
	if(set_socket_options(&scfg, sock, section, error, sizeof(error)))
		lprintf(LOG_ERR,"%04d !ERROR %s",sock, error);
}

void close_socket_cb(SOCKET sock, void *serv_ptr)
{
	if(startup!=NULL && startup->socket_open!=NULL)
		startup->socket_open(startup->cbdata,FALSE);
}

static SOCKET open_socket(int family, int type, service_t* serv)
{
	SOCKET	sock;

	sock=socket(family, type, IPPROTO_IP);
	if(sock!=INVALID_SOCKET)
		open_socket_cb(sock, serv);
	return(sock);
}

static int close_socket(SOCKET sock)
{
	char	err[128];
	int		result;

	if(sock==INVALID_SOCKET)
		return(-1);

	shutdown(sock,SHUT_RDWR);	/* required on Unix */
	result=closesocket(sock);
	if(startup!=NULL && startup->socket_open!=NULL) 
		startup->socket_open(startup->cbdata,FALSE);
	if(result!=0)
		lprintf(LOG_WARNING,"%04d !ERROR %d closing socket: %s",sock, ERROR_VALUE, socket_strerror(socket_errno, err, sizeof(err)));

	return(result);
}

static void status(char* str)
{
	if(startup!=NULL && startup->status!=NULL)
	    startup->status(startup->cbdata,str);
}

/* Global JavaScript Methods */

static JSBool
js_log(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char		str[512];
    uintN		i=0;
	int32		level=LOG_INFO;
	service_client_t* client;
	jsrefcount	rc;
	char		*line = NULL;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((client=(service_client_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(startup==NULL || startup->lputs==NULL)
		return(JS_FALSE);

	if(argc > 1 && JSVAL_IS_NUMBER(argv[i])) {
		if(!JS_ValueToInt32(cx,argv[i++],&level))
			return JS_FALSE;
	}

	str[0]=0;
    for(;i<argc && strlen(str)<(sizeof(str)/2);i++) {
		JSVALUE_TO_MSTRING(cx, argv[i], line, NULL);
		HANDLE_PENDING(cx, line);
		if(line==NULL)
		    return(JS_FALSE);
		strncat(str,line,sizeof(str)/2);
		free(line);
		strcat(str," ");
	}

	rc=JS_SUSPENDREQUEST(cx);
	if(service==NULL)
		lprintf(level,"%04d %s",client->socket,str);
	else if(level <= client->service->log_level)
		lprintf(level,"%04d %s %s",client->socket,client->service->protocol,str);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, str)));

    return(JS_TRUE);
}

static void badlogin(SOCKET sock, char* prot, char* user, char* passwd, char* host, union xp_sockaddr* addr)
{
	char reason[128];
	char addr_ip[INET6_ADDRSTRLEN];
	ulong count;

	SAFEPRINTF(reason,"%s LOGIN", prot);
	count=loginFailure(startup->login_attempt_list, addr, prot, user, passwd);
	if(startup->login_attempt.hack_threshold && count>=startup->login_attempt.hack_threshold) {
		hacklog(&scfg, reason, user, passwd, host, addr);
#ifdef _WIN32
		if(startup->sound.hack[0] && !sound_muted(&scfg))
			PlaySound(startup->sound.hack, NULL, SND_ASYNC|SND_FILENAME);
#endif
	}
	if(startup->login_attempt.filter_threshold && count>=startup->login_attempt.filter_threshold) {
		inet_addrtop(addr, addr_ip, sizeof(addr_ip));
		SAFEPRINTF(reason, "- TOO MANY CONSECUTIVE FAILED LOGIN ATTEMPTS (%lu)", count);
		filter_ip(&scfg, prot, reason, host, addr_ip, user, /* fname: */NULL);
	}

	mswait(startup->login_attempt.delay);
}

static JSBool
js_login(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char*		user;
	char*		pass = NULL;
	JSBool		inc_logons=JS_FALSE;
	jsval		val;
	service_client_t* client;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(JS_FALSE));

	if((client=(service_client_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	/* User name or number */
	JSVALUE_TO_ASTRING(cx, argv[0], user, 128, NULL);
	if(user==NULL)
		return(JS_FALSE);

	/* Password */
	if(argc > 1) {
		JSVALUE_TO_ASTRING(cx, argv[1], pass, LEN_PASS+2, NULL);
		if(pass==NULL) 
			return(JS_FALSE);
	}
	rc=JS_SUSPENDREQUEST(cx);
	memset(&client->user,0,sizeof(user_t));

	/* ToDo Deuce: did you mean to do this *before* the above memset(0) ? */
	if(client->user.number) {
		if(client->subscan!=NULL)
			putmsgptrs(&scfg, &client->user, client->subscan);
	}

	if(IS_DIGIT(*user))
		client->user.number=atoi(user);
	else if(*user)
		client->user.number=matchuser(&scfg,user,FALSE);

	if(getuserdat(&scfg,&client->user)!=0) {
		lprintf(LOG_NOTICE,"%04d %s !USER NOT FOUND: '%s'"
			,client->socket,client->service->protocol,user);
		badlogin(client->socket, client->service->protocol, user, pass, client->client->host, &client->addr);
		JS_RESUMEREQUEST(cx, rc);
		return(JS_TRUE);
	}

	if(client->user.misc&(DELETED|INACTIVE)) {
		lprintf(LOG_WARNING,"%04d %s !DELETED OR INACTIVE USER #%d: %s"
			,client->socket,client->service->protocol,client->user.number,user);
		JS_RESUMEREQUEST(cx, rc);
		return(JS_TRUE);
	}

	/* Password */
	if(client->user.pass[0] && (pass == NULL || stricmp(client->user.pass,pass))) { /* Wrong password */
		lprintf(LOG_WARNING,"%04d %s !INVALID PASSWORD ATTEMPT FOR USER: %s"
			,client->socket,client->service->protocol,client->user.alias);
		badlogin(client->socket, client->service->protocol, user, pass, client->client->host, &client->addr);
		JS_RESUMEREQUEST(cx, rc);
		return(JS_TRUE);
	}
	JS_RESUMEREQUEST(cx, rc);

	if(argc>2)
		JS_ValueToBoolean(cx,argv[2],&inc_logons);

	rc=JS_SUSPENDREQUEST(cx);
	if(client->client!=NULL) {
		SAFECOPY(client->user.ipaddr,client->client->addr);
		SAFECOPY(client->user.comp,client->client->host);
		SAFECOPY(client->user.modem,client->service->protocol);
	}

	if(inc_logons) {
		client->user.logons++;
		client->user.ltoday++;
	}	

	putuserdat(&scfg,&client->user);
	if(client->subscan==NULL) {
		client->subscan=(subscan_t*)calloc(scfg.total_subs, sizeof(subscan_t));
		if(client->subscan==NULL)
			lprintf(LOG_CRIT,"!MALLOC FAILURE");
	}
	if(client->subscan!=NULL) {
		getmsgptrs(&scfg,&client->user,client->subscan,NULL,NULL);
	}

	JS_RESUMEREQUEST(cx, rc);

	if(!js_CreateUserObjects(cx, obj, &scfg, &client->user, client->client, NULL, client->subscan))
		lprintf(LOG_ERR,"%04d %s !JavaScript ERROR creating user objects"
			,client->socket,client->service->protocol);

	if(client->client!=NULL) {
		client->client->user=client->user.alias;
		client->client->usernum = client->user.number;
		client_on(client->socket,client->client,TRUE /* update */);
	}

	client->logintime=time(NULL);

	if(client->service->log_level >= LOG_INFO)
		lprintf(LOG_INFO,"%04d %s Logging in %s"
			,client->socket,client->service->protocol,client->user.alias);

	val = BOOLEAN_TO_JSVAL(JS_TRUE);
	if(!JS_SetProperty(cx, obj, "logged_in", &val))
		lprintf(LOG_ERR, "%04d %s Error setting logged in property for %s"
			,client->socket, client->service->protocol, client->user.alias);

	if(client->user.pass[0])
		loginSuccess(startup->login_attempt_list, &client->addr);

	JS_SET_RVAL(cx, arglist,BOOLEAN_TO_JSVAL(JS_TRUE));

#ifdef _WIN32
	if(startup->sound.login[0] && !sound_muted(&scfg)
		&& !(service->options&BBS_OPT_MUTE))
		PlaySound(startup->sound.login, NULL, SND_ASYNC|SND_FILENAME);
#endif

	return(JS_TRUE);
}

static JSBool
js_logout(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval val;
	service_client_t* client;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(JS_FALSE));

	if((client=(service_client_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	if(client->user.number<1)	/* Not logged in */
		return(JS_TRUE);

	rc=JS_SUSPENDREQUEST(cx);
	if(!logoutuserdat(&scfg,&client->user,time(NULL),client->logintime))
		lprintf(LOG_ERR,"%04d !ERROR in logoutuserdat",client->socket);

	if(client->service->log_level >= LOG_INFO)
		lprintf(LOG_INFO,"%04d %s Logging out %s"
			,client->socket,client->service->protocol,client->user.alias);

	memset(&client->user,0,sizeof(client->user));
	JS_RESUMEREQUEST(cx, rc);

	val = BOOLEAN_TO_JSVAL(JS_FALSE);
	JS_SetProperty(cx, obj, "logged_in", &val);

	JS_SET_RVAL(cx, arglist,BOOLEAN_TO_JSVAL(JS_TRUE));

	return(JS_TRUE);
}

/*
 * This macro is used to expose a function from the global
 * client.socket object in the global namespace.
 */
#define SOCKET_WRAPPER(funcname) \
static JSBool \
js_##funcname (JSContext *cx, uintN argc, jsval *arglist) \
{ \
	JSObject *obj=JS_THIS_OBJECT(cx, arglist); \
	JSObject *tmpobj; \
	jsval val; \
	jsval rval; \
	JSObject*	socket_obj; \
	jsval *argv=JS_ARGV(cx, arglist); \
	JSBool retval; \
\
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(JS_FALSE)); \
\
	if (!JS_GetProperty(cx, obj, "client", &val) || val == JSVAL_VOID) \
		return JS_FALSE; \
	tmpobj=JSVAL_TO_OBJECT(val); \
	if (!JS_GetProperty(cx, tmpobj, "socket", &val) || val == JSVAL_VOID) \
		return JS_FALSE; \
	socket_obj=JSVAL_TO_OBJECT(val); \
	retval = JS_CallFunctionName(cx, socket_obj, #funcname, argc, argv, &rval); \
	JS_SET_RVAL(cx, arglist, rval); \
	return retval; \
}

SOCKET_WRAPPER(read)
SOCKET_WRAPPER(readln)
SOCKET_WRAPPER(write)
SOCKET_WRAPPER(writeln)
SOCKET_WRAPPER(print)

static JSFunctionSpec js_global_functions[] = {
	{"read",			js_read,			0},		/* Read from socket */
	{"readln",			js_readln,			0},		/* Read line from socket */
	{"write",			js_write,			1},		/* Write to socket */
	{"writeln",			js_writeln,			0},		/* Write line to socket */
	{"print",			js_print,			0},		/* Write line to socket */
	{"log",				js_log,				0},		/* Log a string */
 	{"login",			js_login,			2},		/* Login specified username and password */
	{"logout",			js_logout,			0},		/* Logout user */
    {0}
};

static void
js_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
	char	line[64];
	char	user[LEN_ALIAS * 2] = "";
	char	file[MAX_PATH+1];
	char*	prot="???";
	SOCKET	sock=0;
	char*	warning;
	service_client_t* client;
	jsrefcount	rc;
	int		log_level;

	if((client=(service_client_t*)JS_GetContextPrivate(cx))!=NULL) {
		prot=client->service->protocol;
		sock=client->socket;
		if(client->user.number)
			SAFEPRINTF(user, " <%s>", client->user.alias);
	}

	if(report==NULL) {
		lprintf(LOG_ERR,"%04d %s !JavaScript: %s", sock,prot,message);
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

	rc=JS_SUSPENDREQUEST(cx);
	if(client == NULL || client->service == NULL || client->service->log_level >= log_level) {
		lprintf(log_level,"%04d %s%s !JavaScript %s%s%s: %s", sock, prot, user, warning, file, line, message);
	}
	JS_RESUMEREQUEST(cx, rc);
}

/* Server Methods */

static JSBool
js_client_add(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	client_t		client;
	SOCKET			sock=INVALID_SOCKET;
	socklen_t		addr_len;
	union xp_sockaddr	addr;
	service_client_t* service_client;
	jsrefcount		rc;
	char			*cstr=NULL;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((service_client=(service_client_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	protected_uint32_adjust(&service_client->service->clients, 1);
	update_clients();
	service_client->service->served++;
	served++;
	memset(&client,0,sizeof(client));
	client.size=sizeof(client);
	client.protocol=service_client->service->protocol;
	client.time=time32(NULL);
	client.user=STR_UNKNOWN_USER;
	client.usernum = 0;
	SAFECOPY(client.host,client.user);

	sock=js_socket(cx,argv[0]);
	if(sock < 0)
		return JS_TRUE;

	addr_len = sizeof(addr);
	if(getpeername(sock, &addr.addr, &addr_len)==0) {
		inet_addrtop(&addr, client.addr, sizeof(client.addr));
		client.port=inet_addrport(&addr);
	}

	if(argc>1) {
		JSVALUE_TO_MSTRING(cx, argv[1], cstr, NULL);
		HANDLE_PENDING(cx, cstr);
		client.user=cstr;
	}

	if(argc>2)
		JSVALUE_TO_STRBUF(cx, argv[2], client.host, sizeof(client.host), NULL);

	rc=JS_SUSPENDREQUEST(cx);
	client_on(sock, &client, /* update? */ FALSE);
#ifdef _DEBUG
	lprintf(LOG_DEBUG,"%s client_add(%04u,%s,%s)"
		,service_client->service->protocol
		,sock,client.user,client.host);
#endif
	if(cstr)
		free(cstr);
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_client_update(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	client_t	client;
	SOCKET		sock=INVALID_SOCKET;
	socklen_t	addr_len;
	union xp_sockaddr	addr;
	service_client_t*	service_client;
	jsrefcount	rc;
	char		*cstr=NULL;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((service_client=(service_client_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	memset(&client,0,sizeof(client));
	client.size=sizeof(client);
	client.protocol=service_client->service->protocol;
	client.user=STR_UNKNOWN_USER;
	SAFECOPY(client.host,client.user);

	sock=js_socket(cx,argv[0]);
	if(sock < 0)
		return JS_TRUE;

	addr_len = sizeof(addr);
	if(getpeername(sock, &addr.addr, &addr_len)==0) {
		inet_addrtop(&addr, client.addr, sizeof(client.addr));
		client.port=inet_addrport(&addr);
	}

	if(argc>1) {
		JSVALUE_TO_MSTRING(cx, argv[1], cstr, NULL);
		client.user=cstr;
	}

	if(argc>2)
		JSVALUE_TO_STRBUF(cx, argv[2], client.host, sizeof(client.host), NULL);

	rc=JS_SUSPENDREQUEST(cx);
	client_on(sock, &client, /* update? */ TRUE);
#ifdef _DEBUG
	lprintf(LOG_DEBUG,"%s client_update(%04u,%s,%s)"
		,service_client->service->protocol
		,sock,client.user,client.host);
#endif
	if(cstr)
		free(cstr);
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}


static JSBool
js_client_remove(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	SOCKET	sock=INVALID_SOCKET;
	service_client_t* service_client;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((service_client=(service_client_t*)JS_GetContextPrivate(cx))==NULL)
		return(JS_FALSE);

	sock=js_socket(cx,argv[0]);
	if(sock < 0)
		return JS_TRUE;

	if(sock!=INVALID_SOCKET) {

		rc=JS_SUSPENDREQUEST(cx);
		client_off(sock);

		if(protected_uint32_value(service_client->service->clients) == 0)
			lprintf(LOG_WARNING,"%s !client_remove() called with 0 service clients"
				,service_client->service->protocol);
		else {
			protected_uint32_adjust(&service_client->service->clients, -1);
			update_clients();
		}
		JS_RESUMEREQUEST(cx, rc);
	}

#ifdef _DEBUG
	lprintf(LOG_DEBUG,"%s client_remove(%04u)"
		,service_client->service->protocol, sock);
#endif
	return(JS_TRUE);
}

static JSContext* 
js_initcx(JSRuntime* js_runtime, SOCKET sock, service_client_t* service_client, JSObject** glob)
{
	JSContext*	js_cx;
	JSObject*	server;
	BOOL		success=FALSE;
	BOOL		rooted=FALSE;

    if((js_cx = JS_NewContext(js_runtime, JAVASCRIPT_CONTEXT_STACK))==NULL)
		return(NULL);
	JS_SetOptions(js_cx, startup->js.options);
	JS_BEGINREQUEST(js_cx);

    JS_SetErrorReporter(js_cx, js_ErrorReporter);

	/* ToDo: call js_CreateCommonObjects() instead */

	do {

		JS_SetContextPrivate(js_cx, service_client);

		if(!js_CreateGlobalObject(js_cx, &scfg, NULL, &service_client->service->js, glob))
			break;
		rooted=TRUE;

		if (!JS_DefineFunctions(js_cx, *glob, js_global_functions))
			break;

		/* Internal JS Object */
		if(js_CreateInternalJsObject(js_cx, *glob, &service_client->callback, &service_client->service->js)==NULL)
			break;

		/* Client Object */
		if(service_client->client!=NULL) {
			if(js_CreateClientObject(js_cx, *glob, "client", service_client->client, sock, service_client->tls_sess)==NULL)
				break;
		}

		/* User Class */
		if(js_CreateUserClass(js_cx, *glob, &scfg)==NULL) 
			break;

		/* Socket Class */
		if(js_CreateSocketClass(js_cx, *glob)==NULL)
			break;

		/* MsgBase Class */
		if(js_CreateMsgBaseClass(js_cx, *glob, &scfg)==NULL)
			break;

		/* FileBase Class */
		if(js_CreateFileBaseClass(js_cx, *glob, &scfg)==NULL)
			break;

		/* File Class */
		if(js_CreateFileClass(js_cx, *glob)==NULL)
			break;

		/* Queue Class */
		if(js_CreateQueueClass(js_cx, *glob)==NULL)
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

		/* user-specific objects */
		if(!js_CreateUserObjects(js_cx, *glob, &scfg, /*user: */NULL, service_client->client, NULL, service_client->subscan)) 
			break;

		if(js_CreateSystemObject(js_cx, *glob, &scfg, uptime, server_host_name(), SOCKLIB_DESC)==NULL) 
			break;

		if(service_client->service->js_server_props.version[0]==0) {
			SAFEPRINTF2(service_client->service->js_server_props.version
				,"Synchronet Services %s%c", VERSION, REVISION);
			service_client->service->js_server_props.version_detail=
				services_ver();
			service_client->service->js_server_props.clients = &service_client->service->clients;
			service_client->service->js_server_props.interfaces=
				&service_client->service->interfaces;
			service_client->service->js_server_props.options=
				&service_client->service->options;
		}

		if((server=js_CreateServerObject(js_cx,*glob
			,&service_client->service->js_server_props))==NULL)
			break;

		if(service_client->client==NULL) {	/* static service */
			if(js_CreateSocketObjectFromSet(js_cx, server, "socket", service_client->set)==NULL)
				break;
		}

		JS_DefineFunction(js_cx, server, "client_add"	, js_client_add,	1, 0);
		JS_DefineFunction(js_cx, server, "client_update", js_client_update,	1, 0);
		JS_DefineFunction(js_cx, server, "client_remove", js_client_remove, 1, 0);

		success=TRUE;

	} while(0);


	if(!success) {
		if(rooted)
			JS_RemoveObjectRoot(js_cx, glob);
		JS_ENDREQUEST(js_cx);
		JS_DestroyContext(js_cx);
		return(NULL);
	}

	return(js_cx);
}

static JSBool
js_OperationCallback(JSContext *cx)
{
	JSBool	ret;
	service_client_t* client;

	JS_SetOperationCallback(cx, NULL);
	if((client=(service_client_t*)JS_GetContextPrivate(cx))==NULL) {
		JS_SetOperationCallback(cx, js_OperationCallback);
		return(JS_FALSE);
	}

	/* Terminated? */ 
	if(client->callback.auto_terminate && terminated) {
		JS_ReportWarning(cx,"Terminated");
		client->callback.counter=0;
		JS_SetOperationCallback(cx, js_OperationCallback);
		return(JS_FALSE);
	}

	ret=js_CommonOperationCallback(cx,&client->callback);
	JS_SetOperationCallback(cx, js_OperationCallback);

	return ret;
}

static void js_init_args(JSContext* js_cx, JSObject* js_obj, const char* cmdline)
{
	char					argbuf[MAX_PATH+1];
	char*					p;
	char*					args;
	int						argc=0;
	JSString*				arg_str;
	JSObject*				argv;
	jsval					val;

	argv=JS_NewArrayObject(js_cx, 0, NULL);
	JS_DefineProperty(js_cx, js_obj, "argv", OBJECT_TO_JSVAL(argv)
		,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);

	p=(char*)cmdline;
	while(*p && *p>' ') p++;	/* find end of filename */
	while(*p && *p<=' ') p++;	/* find first arg */
	SAFECOPY(argbuf,p);

	args=argbuf;
	while(*args && argv!=NULL) {
		p=strchr(args,' ');
		if(p!=NULL)
			*p=0;
		while(*args && *args<=' ') args++; /* Skip spaces */
		arg_str = JS_NewStringCopyZ(js_cx, args);
		if(arg_str==NULL)
			break;
		val=STRING_TO_JSVAL(arg_str);
		if(!JS_SetElement(js_cx, argv, argc, &val))
			break;
		argc++;
		if(p==NULL)	/* last arg */
			break;
		args+=(strlen(args)+1);
	}
	JS_DefineProperty(js_cx, js_obj, "argc", INT_TO_JSVAL(argc)
		,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);
}

#define HANDLE_CRYPT_CALL(status, service_client, action)  handle_crypt_call(status, service_client, action, __FILE__, __LINE__)

static BOOL handle_crypt_call(int status, service_client_t *service_client, const char *action, const char *file, int line)
{
	char	*estr = NULL;
	int		sock = 0;
	CRYPT_HANDLE	sess;
	int level;

	if (status == CRYPT_OK)
		return TRUE;
	if (service_client != NULL) {
		sock = service_client->socket;
		if (service_client->service->options & SERVICE_OPT_TLS) {
			sess = service_client->tls_sess;
			if (sess == -1)
				sess = CRYPT_UNUSED;
			get_crypt_error_string(status, sess, &estr, action, &level);
			if (estr) {
				lprintf(level, "%04d %s TLS %s", sock, service_client->service->protocol, estr);
				free_crypt_attrstr(estr);
			}
		}
	}
	return FALSE;
}

static void js_service_failure_cleanup(service_t *service, SOCKET socket)
{
	close_socket(socket);
	protected_uint32_adjust(&service->clients, -1);
	thread_down();
	return;
}

static void js_service_thread(void* arg)
{
	char					host_name[256];
	SOCKET					socket;
	client_t				client;
	service_t*				service;
	service_client_t		service_client;
	ulong					login_attempts;
	/* JavaScript-specific */
	char					spath[MAX_PATH+1];
	char					fname[MAX_PATH+1];
	JSString*				datagram;
	JSObject*				js_glob;
	JSObject*				js_script;
	JSRuntime*				js_runtime;
	JSContext*				js_cx;
	jsval					val;
	jsval					rval;

	/* Copy service_client arg */
	service_client=*(service_client_t*)arg;
	/* Free original */
	free(arg);

	socket=service_client.socket;
	service=service_client.service;

	if(service->log_level >= LOG_DEBUG)
		lprintf(LOG_DEBUG,"%04d %s JavaScript service thread started", socket, service->protocol);

	SetThreadName("sbbs/jsService");
	thread_up(TRUE /* setuid */);
	sbbs_srand();	/* Seed random number generator */
	protected_uint32_adjust(&threads_pending_start, -1);

	inet_addrtop(&service_client.addr, client.addr, sizeof(client.addr));

	/* Host name lookup and filtering */
	SAFECOPY(host_name, STR_NO_HOSTNAME);
	if(!(service->options&BBS_OPT_NO_HOST_LOOKUP)
		&& !(startup->options&BBS_OPT_NO_HOST_LOOKUP)) {
		getnameinfo(&service_client.addr.addr, xp_sockaddr_len(&service_client), host_name, sizeof(host_name), NULL, 0, NI_NAMEREQD);
		if(service->log_level >= LOG_INFO)
			lprintf(LOG_INFO,"%04d %s Hostname: %s [%s]"
				,socket, service->protocol, host_name, client.addr);
	}

	if(trashcan(&scfg,host_name,"host")) {
		if(service->log_level >= LOG_NOTICE)
			lprintf(LOG_NOTICE,"%04d %s !CLIENT BLOCKED in host.can: %s"
				,socket, service->protocol, host_name);
		close_socket(socket);
		protected_uint32_adjust(&service->clients, -1);
		thread_down();
		return;
	}

	service_client.tls_sess = -1;
	if (service_client.service->options & SERVICE_OPT_TLS) {
		/* Create and initialize the TLS session */
		if (!HANDLE_CRYPT_CALL(cryptCreateSession(&service_client.tls_sess, CRYPT_UNUSED, CRYPT_SESSION_SSL_SERVER), &service_client, "creating session")) {
			js_service_failure_cleanup(service, socket);
			return;
		}
		/* Add all the user/password combinations */
#if 0 // TLS-PSK is currently broken in cryptlib
		last = lastuser(&scfg);
		for (i=1; i <= last; i++) {
			user.number = i;
			getuserdat(&scfg,&user);
			if(user.misc&(DELETED|INACTIVE))
				continue;
			if (user.alias[0] && user.pass[0]) {
				if(HANDLE_CRYPT_CALL(cryptSetAttributeString(service_client.tls_sess, CRYPT_SESSINFO_USERNAME, user.alias, strlen(user.alias)), &session, "adding PSK user"))
					HANDLE_CRYPT_CALL(cryptSetAttributeString(service_client.tls_sess, CRYPT_SESSINFO_PASSWORD, user.pass, strlen(user.pass)), &session, "adding PSK key");
			}
		}
#endif
		lock_ssl_cert();
		if (scfg.tls_certificate != -1) {
			HANDLE_CRYPT_CALL(cryptSetAttribute(service_client.tls_sess, CRYPT_SESSINFO_PRIVATEKEY, scfg.tls_certificate), &service_client, "setting private key");
		}
		BOOL nodelay=TRUE;
		setsockopt(socket,IPPROTO_TCP,TCP_NODELAY,(char*)&nodelay,sizeof(nodelay));

		HANDLE_CRYPT_CALL(cryptSetAttribute(service_client.tls_sess, CRYPT_SESSINFO_NETWORKSOCKET, socket), &service_client, "setting network socket");
		if (!HANDLE_CRYPT_CALL(cryptSetAttribute(service_client.tls_sess, CRYPT_SESSINFO_ACTIVE, 1), &service_client, "setting session active")) {
			unlock_ssl_cert();
			if (service_client.tls_sess != -1)
				cryptDestroySession(service_client.tls_sess);
			js_service_failure_cleanup(service, socket);
			return;
		}
		unlock_ssl_cert();
	}

#if 0	/* Need to export from SBBS.DLL */
	identity=NULL;
	if(service->options&BBS_OPT_GET_IDENT 
		&& startup->options&BBS_OPT_GET_IDENT) {
		identify(&service_client, service->port, str, sizeof(str)-1);
		identity=strrchr(str,':');
		if(identity!=NULL) {
			identity++;	/* skip colon */
			while(*identity && *identity<=SP) /* point to user name */
				identity++;
			lprintf(LOG_INFO,"%04d Identity: %s",socket, identity);
		}
	}
#endif

	client.size=sizeof(client);
	client.time=time32(NULL);
	SAFECOPY(client.host,host_name);
	client.port=inet_addrport(&service_client.addr);
	client.protocol=service->protocol;
	client.user=STR_UNKNOWN_USER;
	client.usernum = 0;
	service_client.client=&client;

	/* Initialize client display */
	client_on(socket,&client,FALSE /* update */);

	if((js_runtime=jsrt_GetNew(service->js.max_bytes, 5000, __FILE__, __LINE__))==NULL
		|| (js_cx=js_initcx(js_runtime,socket,&service_client,&js_glob))==NULL) {
		if(service->log_level >= LOG_ERR)
			lprintf(LOG_ERR,"%04d %s !ERROR initializing JavaScript context"
				,socket,service->protocol);
		if (service_client.tls_sess != -1)
			cryptDestroySession(service_client.tls_sess);
		client_off(socket);
		close_socket(socket);
		protected_uint32_adjust(&service->clients, -1);
		thread_down();
		return;
	}

	update_clients();

	if(startup->login_attempt.throttle
		&& (login_attempts=loginAttempts(startup->login_attempt_list, &service_client.addr)) > 1) {
		if(service->log_level >= LOG_DEBUG)
			lprintf(LOG_DEBUG,"%04d %s Throttling suspicious connection from: %s (%lu login attempts)"
				,socket, service->protocol, client.addr, login_attempts);
		mswait(login_attempts*startup->login_attempt.throttle);
	}

	/* RUN SCRIPT */
	SAFECOPY(fname,service->cmd);
	truncstr(fname," ");
	SAFEPRINTF2(spath,"%s%s",scfg.mods_dir,fname);
	if(scfg.mods_dir[0]==0 || !fexist(spath))
		SAFEPRINTF2(spath,"%s%s",scfg.exec_dir,fname);

	js_init_args(js_cx, js_glob, service->cmd);

	val = BOOLEAN_TO_JSVAL(JS_FALSE);
	JS_SetProperty(js_cx, js_glob, "logged_in", &val);

	if(service->options&SERVICE_OPT_UDP 
		&& service_client.udp_buf != NULL
		&& service_client.udp_len > 0) {
		datagram = JS_NewStringCopyN(js_cx, (char*)service_client.udp_buf, service_client.udp_len);
		if(datagram==NULL)
			val=JSVAL_VOID;
		else
			val = STRING_TO_JSVAL(datagram);
	} else
		val = JSVAL_VOID;
	JS_SetProperty(js_cx, js_glob, "datagram", &val);
	FREE_AND_NULL(service_client.udp_buf);

	JS_ClearPendingException(js_cx);

	js_script=JS_CompileFile(js_cx, js_glob, spath);

	if(js_script==NULL) {
		if(service->log_level >= LOG_ERR)
			lprintf(LOG_ERR,"%04d !JavaScript FAILED to compile script (%s)",socket,spath);
	} else {
		service_client.callback.events_supported = TRUE;
		js_PrepareToExecute(js_cx, js_glob, spath, /* startup_dir */NULL, js_glob);
		JS_SetOperationCallback(js_cx, js_OperationCallback);
		JS_ExecuteScript(js_cx, js_glob, js_script, &rval);
		js_handle_events(js_cx, &service_client.callback, &terminated);
		js_EvalOnExit(js_cx, js_glob, &service_client.callback);
	}
	JS_RemoveObjectRoot(js_cx, &js_glob);
	JS_ENDREQUEST(js_cx);
	JS_DestroyContext(js_cx);	/* Free Context */

	jsrt_Release(js_runtime);

	if(service_client.user.number) {
		if(service_client.subscan!=NULL)
			putmsgptrs(&scfg, &service_client.user, service_client.subscan);
		if(service->log_level >= LOG_INFO)
			lprintf(LOG_INFO,"%04d %s Logging out %s"
				,socket, service->protocol, service_client.user.alias);
		logoutuserdat(&scfg,&service_client.user,time(NULL),service_client.logintime);

#ifdef _WIN32
		if(startup->sound.logout[0] && !sound_muted(&scfg)
			&& !(service->options&BBS_OPT_MUTE))
			PlaySound(startup->sound.logout, NULL, SND_ASYNC|SND_FILENAME);
#endif
	}
	FREE_AND_NULL(service_client.subscan);

	ulong remain = protected_uint32_adjust(&service->clients, -1);
	update_clients();

#ifdef _WIN32
	if(startup->sound.hangup[0] && !sound_muted(&scfg)
		&& !(service->options&BBS_OPT_MUTE))
		PlaySound(startup->sound.hangup, NULL, SND_ASYNC|SND_FILENAME);
#endif

	thread_down();
	if(service->log_level >= LOG_INFO)
		lprintf(LOG_INFO,"%04d %s service thread terminated (%lu clients remain, %lu total, %lu served)"
			,socket, service->protocol, remain, active_clients(), service->served);

	client_off(socket);
	close_socket(socket);
}

static void js_static_service_thread(void* arg)
{
	char					spath[MAX_PATH+1];
	char					fname[MAX_PATH+1];
	service_t*				service;
	service_client_t		service_client;
	/* JavaScript-specific */
	JSObject*				js_glob;
	JSObject*				js_script;
	JSRuntime*				js_runtime;
	JSContext*				js_cx;
	jsval					val;
	jsval					rval;

	/* Copy service_client arg */
	service=(service_t*)arg;

	service->running=TRUE;

	if(service->log_level >= LOG_DEBUG)
		lprintf(LOG_DEBUG,"%s static JavaScript service thread started", service->protocol);

	SetThreadName("sbbs/jsStatic");
	thread_up(TRUE /* setuid */);
	sbbs_srand();	/* Seed random number generator */
	protected_uint32_adjust(&threads_pending_start, -1);

	memset(&service_client,0,sizeof(service_client));
	service_client.set = service->set;
	service_client.service = service;
	service_client.callback.limit = service->js.time_limit;
	service_client.callback.gc_interval = service->js.gc_interval;
	service_client.callback.yield_interval = service->js.yield_interval;
	service_client.callback.terminated = &service->terminated;
	service_client.callback.auto_terminate = TRUE;

	if((js_runtime=jsrt_GetNew(service->js.max_bytes, 5000, __FILE__, __LINE__))==NULL) {
		if(service->log_level >= LOG_ERR)
			lprintf(LOG_ERR,"%s !ERROR initializing JavaScript runtime"
				,service->protocol);
		xpms_destroy(service->set, close_socket_cb, service);
		service->set = NULL;
		thread_down();
		return;
	}

	SAFECOPY(fname,service->cmd);
	truncstr(fname," ");
	SAFEPRINTF2(spath,"%s%s",scfg.mods_dir,fname);
	if(scfg.mods_dir[0]==0 || !fexist(spath))
		SAFEPRINTF2(spath,"%s%s",scfg.exec_dir,fname);

	do {
		if((js_cx=js_initcx(js_runtime,INVALID_SOCKET,&service_client,&js_glob))==NULL) {
			if(service->log_level >= LOG_ERR)
				lprintf(LOG_ERR,"%s !ERROR initializing JavaScript context"
					,service->protocol);
			break;
		}

		js_init_args(js_cx, js_glob, service->cmd);

		val = BOOLEAN_TO_JSVAL(JS_FALSE);
		JS_SetProperty(js_cx, js_glob, "logged_in", &val);

		JS_SetOperationCallback(js_cx, js_OperationCallback);
	
		if((js_script=JS_CompileFile(js_cx, js_glob, spath))==NULL)  {
			if(service->log_level >= LOG_ERR)
				lprintf(LOG_ERR,"!JavaScript FAILED to compile script (%s)",spath);
			break;
		}

		service_client.callback.events_supported = TRUE;
		js_PrepareToExecute(js_cx, js_glob, spath, /* startup_dir */NULL, js_glob);
		JS_ExecuteScript(js_cx, js_glob, js_script, &rval);
		js_handle_events(js_cx, &service_client.callback, &terminated);
		js_EvalOnExit(js_cx, js_glob, &service_client.callback);
		JS_RemoveObjectRoot(js_cx, &js_glob);
		JS_ENDREQUEST(js_cx);
		JS_DestroyContext(js_cx);	/* Free Context */
		js_cx=NULL;
	} while(!service->terminated && service->options&SERVICE_OPT_STATIC_LOOP);

	if(js_cx!=NULL) {
		JS_RemoveObjectRoot(js_cx, &js_glob);
		JS_ENDREQUEST(js_cx);
		JS_DestroyContext(js_cx);	/* Free Context */
	}

	jsrt_Release(js_runtime);

	if(protected_uint32_value(service->clients)) {
		if(service->log_level >= LOG_WARNING)
			lprintf(LOG_WARNING,"%s !service terminating with %u active clients"
				, service->protocol, protected_uint32_value(service->clients));
		protected_uint32_set(&service->clients, 0);
	}

	thread_down();
	if(service->log_level >= LOG_INFO)
		lprintf(LOG_INFO,"%s service thread terminated (%lu clients served)"
			, service->protocol, service->served);

	xpms_destroy(service->set, close_socket_cb, service);
	service->set = NULL;

	service->running=FALSE;
}

struct native_service_instance {
	service_t	*service;
	SOCKET		socket;
};

static void native_static_service_thread(void* arg)
{
	char					cmd[MAX_PATH];
	char					fullcmd[MAX_PATH*2];
	SOCKET					socket_dup;
	struct native_service_instance inst;

	inst = *(struct native_service_instance *)arg;
	free(arg);

	inst.service->running++;

	if(inst.service->log_level >= LOG_DEBUG)
		lprintf(LOG_DEBUG,"%04d %s static service thread started", inst.socket, inst.service->protocol);

	SetThreadName("sbbs/static");
	thread_up(TRUE /* setuid */);
	protected_uint32_adjust(&threads_pending_start, -1);

#ifdef _WIN32
	if(!DuplicateHandle(GetCurrentProcess(),
			(HANDLE)inst.socket,
			GetCurrentProcess(),
			(HANDLE*)&socket_dup,
			0,
			TRUE, /* Inheritable */
			DUPLICATE_SAME_ACCESS)) {
		lprintf(LOG_ERR,"%04d %s !ERROR %d duplicating socket descriptor"
			,inst.socket,inst.service->protocol,GetLastError());
		close_socket(inst.socket);
		thread_down();
		inst.service->running--;
		return;
	}
#else
	socket_dup = dup(inst.socket);
	if(socket_dup == -1) {
		lprintf(LOG_ERR,"%04d %s !ERROR %d duplicating socket descriptor"
			,inst.socket, inst.service->protocol, errno);
		close_socket(inst.socket);
		thread_down();
		inst.service->running--;
		return;
	}
#endif

	/* RUN SCRIPT */
	if(strpbrk(inst.service->cmd,"/\\")==NULL)
		SAFEPRINTF2(cmd,"%s%s",scfg.exec_dir,inst.service->cmd);
	else
		SAFECOPY(cmd,inst.service->cmd);
	SAFEPRINTF(fullcmd,cmd,socket_dup);
	
	do {
		system(fullcmd);
	} while(!inst.service->terminated && inst.service->options&SERVICE_OPT_STATIC_LOOP);

	thread_down();
	if(inst.service->log_level >= LOG_INFO)
		lprintf(LOG_INFO,"%04d %s service thread terminated (%lu clients served)"
			,inst.socket, service->protocol, service->served);

	close_socket(inst.socket);
	closesocket(socket_dup);	/* close duplicate handle */

	service->running--;
}

static void native_service_thread(void* arg)
{
	char					cmd[MAX_PATH];
	char					fullcmd[MAX_PATH*2];
	char					host_name[256];
	SOCKET					socket;
	SOCKET					socket_dup;
	client_t				client;
	service_t*				service;
	service_client_t		service_client=*(service_client_t*)arg;
	ulong					login_attempts;

	free(arg);

	socket=service_client.socket;
	service=service_client.service;

	if(service->log_level >= LOG_DEBUG)
		lprintf(LOG_DEBUG,"%04d %s service thread started", socket, service->protocol);

	SetThreadName("sbbs/native");
	thread_up(TRUE /* setuid */);
	protected_uint32_adjust(&threads_pending_start, -1);

	inet_addrtop(&service_client.addr, client.addr, sizeof(client.addr));

	/* Host name lookup and filtering */
	SAFECOPY(host_name, STR_NO_HOSTNAME);
	if(!(service->options&BBS_OPT_NO_HOST_LOOKUP)
		&& !(startup->options&BBS_OPT_NO_HOST_LOOKUP)) {
		getnameinfo(&service_client.addr.addr, xp_sockaddr_len(&service_client), host_name, sizeof(host_name), NULL, 0, NI_NAMEREQD);
		lprintf(LOG_INFO,"%04d %s Hostname: %s [%s]"
			,socket, service->protocol, host_name, client.addr);
#if	0 /* gethostbyaddr() is apparently not (always) thread-safe
	     and getnameinfo() doesn't return alias information */
		for(i=0;host!=NULL && host->h_aliases!=NULL 
			&& host->h_aliases[i]!=NULL;i++)
			lprintf(LOG_INFO,"%04d %s HostAlias: %s"
				,socket, service->protocol, host->h_aliases[i]);
#endif
	}

	if(trashcan(&scfg,host_name,"host")) {
		lprintf(LOG_NOTICE,"%04d %s !CLIENT BLOCKED in host.can: %s"
			,socket, service->protocol, host_name);
		close_socket(socket);
		protected_uint32_adjust(&service->clients, -1);
		thread_down();
		return;
	}


#if 0	/* Need to export from SBBS.DLL */
	identity=NULL;
	if(service->options&BBS_OPT_GET_IDENT 
		&& startup->options&BBS_OPT_GET_IDENT) {
		identify(&service_client, service->port, str, sizeof(str)-1);
		identity=strrchr(str,':');
		if(identity!=NULL) {
			identity++;	/* skip colon */
			while(*identity && *identity<=SP) /* point to user name */
				identity++;
			lprintf(LOG_INFO,"%04d Identity: %s",socket, identity);
		}
	}
#endif

	client.size=sizeof(client);
	client.time=time32(NULL);
	SAFECOPY(client.host,host_name);
	client.port=inet_addrport(&service_client.addr);
	client.protocol=service->protocol;
	client.user=STR_UNKNOWN_USER;
	client.usernum = 0;

#ifdef _WIN32
	if(!DuplicateHandle(GetCurrentProcess(),
		(HANDLE)socket,
		GetCurrentProcess(),
		(HANDLE*)&socket_dup,
		0,
		TRUE, /* Inheritable */
		DUPLICATE_SAME_ACCESS)) {
		lprintf(LOG_ERR,"%04d %s !ERROR %d duplicating socket descriptor"
			,socket,service->protocol,GetLastError());
		close_socket(socket);
		thread_down();
		return;
	}
#else
	socket_dup = dup(socket);
	if(socket_dup == -1) {
		lprintf(LOG_ERR,"%04d %s !ERROR %d duplicating socket descriptor"
			,socket, service->protocol, errno);
		close_socket(socket);
		thread_down();
		return;
	}
#endif

	update_clients();

	/* Initialize client display */
	client_on(socket,&client,FALSE /* update */);

	if(startup->login_attempt.throttle
		&& (login_attempts=loginAttempts(startup->login_attempt_list, &service_client.addr)) > 1) {
		lprintf(LOG_DEBUG,"%04d %s Throttling suspicious connection from: %s (%lu login attempts)"
			,socket, service->protocol, client.addr, login_attempts);
		mswait(login_attempts*startup->login_attempt.throttle);
	}

	/* RUN SCRIPT */
	if(strpbrk(service->cmd,"/\\")==NULL)
		SAFEPRINTF2(cmd,"%s%s",scfg.exec_dir,service->cmd);
	else
		SAFECOPY(cmd,service->cmd);
	SAFEPRINTF(fullcmd,cmd,socket_dup);

	system(fullcmd);

	ulong remain = protected_uint32_adjust(&service->clients, -1);
	update_clients();

#ifdef _WIN32
	if(startup->sound.hangup[0] && !sound_muted(&scfg)
		&& !(service->options&BBS_OPT_MUTE))
		PlaySound(startup->sound.hangup, NULL, SND_ASYNC|SND_FILENAME);
#endif

	thread_down();
	if(service->log_level >= LOG_INFO)
		lprintf(LOG_INFO,"%04d %s service thread terminated (%lu clients remain, %lu total, %lu served)"
			,socket, service->protocol, remain, active_clients(), service->served);

	client_off(socket);
	close_socket(socket);
	closesocket(socket_dup);	/* close duplicate handle */
}


void services_terminate(void)
{
	uint32_t i;

   	lprintf(LOG_INFO,"0000 Services terminate");
	terminated=TRUE;
	for(i=0;i<services;i++)
		service[i].terminated=TRUE;
}

#define NEXT_FIELD(p)	FIND_WHITESPACE(p); SKIP_WHITESPACE(p)

static service_t* read_services_ini(const char* services_ini, service_t* service, uint32_t* services)
{
	uint		i,j;
	FILE*		fp;
	char*		p;
	char		cmd[INI_MAX_VALUE_LEN];
	char		host[INI_MAX_VALUE_LEN];
	char		prot[INI_MAX_VALUE_LEN];
	char		portstr[INI_MAX_VALUE_LEN];
	char**		sec_list;
	str_list_t	list;
	service_t*	np;
	service_t	serv;
	int			log_level;
	int			listen_backlog;
	uint		max_clients;
	uint32_t	options;
	uint32_t	stack_size;
	char		*default_interfaces;

	if((fp=fopen(services_ini,"r"))==NULL) {
		lprintf(LOG_CRIT,"!ERROR %d (%s) opening %s", errno, strerror(errno), services_ini);
		return(NULL);
	}

	lprintf(LOG_DEBUG,"Reading %s",services_ini);
	list=iniReadFile(fp);
	fclose(fp);

	/* Get default key values from "root" section */
	log_level		= iniGetLogLevel(list,ROOT_SECTION,"LogLevel",startup->log_level);
	stack_size		= (uint32_t)iniGetBytes(list,ROOT_SECTION,"StackSize",1,0);
	max_clients		= iniGetInteger(list,ROOT_SECTION,"MaxClients",0);
	listen_backlog	= iniGetInteger(list,ROOT_SECTION,"ListenBacklog",DEFAULT_LISTEN_BACKLOG);
	options			= iniGetBitField(list,ROOT_SECTION,"Options",service_options,0);

	/* Enumerate and parse each service configuration */
	sec_list = iniGetSectionList(list,"");
	default_interfaces = strListCombine(startup->interfaces, NULL, 16384, ",");
    for(i=0; sec_list!=NULL && sec_list[i]!=NULL; i++) {
		if(!iniGetBool(list,sec_list[i],"Enabled",TRUE)) {
			lprintf(LOG_WARNING,"Ignoring disabled service: %s",sec_list[i]);
			continue;
		}
		memset(&serv,0,sizeof(service_t));
		SAFECOPY(serv.protocol,iniGetString(list,sec_list[i],"Protocol",sec_list[i],prot));
		serv.set = NULL;
		serv.interfaces=iniGetStringList(list,sec_list[i],"Interface",",",default_interfaces);
		serv.max_clients=iniGetInteger(list,sec_list[i],"MaxClients",max_clients);
		serv.listen_backlog=iniGetInteger(list,sec_list[i],"ListenBacklog",listen_backlog);
		serv.stack_size=(uint32_t)iniGetBytes(list,sec_list[i],"StackSize",1,stack_size);
		serv.options=iniGetBitField(list,sec_list[i],"Options",service_options,options);
		serv.log_level=iniGetLogLevel(list,sec_list[i],"LogLevel",log_level);
		SAFECOPY(serv.cmd,iniGetString(list,sec_list[i],"Command","",cmd));

		p=iniGetString(list,sec_list[i],"Port",serv.protocol,portstr);
		if(IS_DIGIT(*p))
			serv.port=(ushort)strtol(p,NULL,0);
		else {
			struct servent* servent = getservbyname(p,serv.options&SERVICE_OPT_UDP ? "udp":"tcp");
			if(servent==NULL)
				servent = getservbyname(p,serv.options&SERVICE_OPT_UDP ? "tcp":"udp");
			if(servent!=NULL)
				serv.port = ntohs(servent->s_port);
		}

		if(serv.port==0) {
			lprintf(LOG_WARNING,"Ignoring service with invalid port (%s): %s",p, sec_list[i]);
			continue;
		}

		if(serv.cmd[0]==0) {
			lprintf(LOG_WARNING,"Ignoring service with no command: %s",sec_list[i]);
			continue;
		}

		/* JavaScript operating parameters */
		sbbs_get_js_settings(list, sec_list[i], &serv.js, &startup->js);

		/* TODO: Fix this up for IPv6 stuff etc... this is going to be ugly! */
		for(j=0;j<*services;j++)
			if(service[j].interface_addr==serv.interface_addr && service[j].port==serv.port
				&& (service[j].options&SERVICE_OPT_UDP)==(serv.options&SERVICE_OPT_UDP))
				break;
		if(j<*services)	{ /* ignore duplicate services */
			lprintf(LOG_NOTICE,"Ignoring duplicate service: %s",sec_list[i]);
			continue;
		}

		if(stricmp(iniGetString(list,sec_list[i],"Host",startup->host_name,host), startup->host_name)!=0) {
			lprintf(LOG_NOTICE,"Ignoring service (%s) for host: %s", sec_list[i], host);
			continue;
		}
		p=iniGetString(list,sec_list[i],"NotHost","",host);
		if(*p!=0 && stricmp(p, startup->host_name)==0) {
			lprintf(LOG_NOTICE,"Ignoring service (%s) not for host: %s", sec_list[i], host);
			continue;
		}

		if((np=(service_t*)realloc(service,sizeof(service_t)*((*services)+1)))==NULL) {
			lprintf(LOG_CRIT,"!MALLOC FAILURE");
			free(default_interfaces);
			iniFreeStringList(sec_list);
			return(service);
		}
		service=np;
		service[*services]=serv;
		(*services)++;
	}
	free(default_interfaces);
	iniFreeStringList(sec_list);
	strListFree(&list);

	return(service);
}

static void cleanup(int code)
{
	while(protected_uint32_value(threads_pending_start)) {
		lprintf(LOG_NOTICE,"0000 Services cleanup waiting on %d threads pending start",protected_uint32_value(threads_pending_start));
		SLEEP(1000);
	}
	protected_uint32_destroy(threads_pending_start);

	for(unsigned i = 0; i < services; i++)
		protected_uint32_destroy(service[i].clients);

	FREE_AND_NULL(service);
	services=0;

	free_cfg(&scfg);

	semfile_list_free(&recycle_semfiles);
	semfile_list_free(&shutdown_semfiles);

	update_clients();

#ifdef _WINSOCKAPI_	
	if(WSAInitialized) {
		char err[128];
		if(WSACleanup()!=0) 
			lprintf(LOG_ERR,"0000 !WSACleanup ERROR %d: %s",ERROR_VALUE, socket_strerror(socket_errno, err, sizeof(err)));
		WSAInitialized = FALSE;
	}
#endif

	thread_down();
	if(terminated || code)
		lprintf(LOG_INFO,"#### Services thread terminated (%lu clients served)",served);
	status("Down");
	if(startup!=NULL && startup->terminated!=NULL)
		startup->terminated(startup->cbdata,code);
}

const char* services_ver(void)
{
	static char ver[256];
	char compiler[32];

	DESCRIBE_COMPILER(compiler);

	sprintf(ver,"Synchronet Services %s%c%s  "
		"Compiled %s/%s %s %s with %s"
		,VERSION, REVISION
#ifdef _DEBUG
		," Debug"
#else
		,""
#endif
		,GIT_BRANCH, GIT_HASH
		,__DATE__, __TIME__, compiler
		);

	return(ver);
}

void service_udp_sock_cb(SOCKET sock, void *cbdata)
{
	service_t	*serv = (service_t *)cbdata;
	int			optval;
	char		err[128];

	open_socket_cb(sock, cbdata);

	/* We need to set the REUSE ADDRESS socket option */
	optval=TRUE;
	if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&optval,sizeof(optval))!=0) {
		lprintf(LOG_ERR,"%04d !ERROR %d setting %s socket option: %s"
			,sock, ERROR_VALUE, serv->protocol, socket_strerror(socket_errno, err, sizeof(err)));
		close_socket(sock);
		return;
	}
   #ifdef BSD
	if(setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (char*)&optval,sizeof(optval))!=0) {
		lprintf(LOG_ERR,"%04d !ERROR %d setting %s socket option: %s"
			,sock, ERROR_VALUE, serv->protocol, socket_strerror(socket_errno, err, sizeof(err)));
		close_socket(sock);
		return;
	}
   #endif
}

#ifdef PREFER_POLL
/*
 * Sets up the fds for poll, and returns the total sockets
 */
static nfds_t setup_poll(struct pollfd **fds)
{
	int i, j;
	nfds_t nfds = 0;

	free(*fds);
	for(i=0;i<(int)services;i++) {
		if(service[i].options&SERVICE_OPT_STATIC)
			continue;
		if(service[i].set==NULL)
			continue;
		nfds += service[i].set->sock_count;
	}

	*fds = calloc(nfds, sizeof(**fds));
	if (*fds == NULL)
		return 0;
	nfds = 0;
	for(i=0;i<(int)services;i++) {
		if(service[i].options&SERVICE_OPT_STATIC)
			continue;
		if(service[i].set==NULL)
			continue;
		for(j=0; j<service[i].set->sock_count; j++) {
			(*fds)[nfds].fd = service[i].set->socks[j].sock;
			(*fds)[nfds].events = POLLIN;
			nfds++;
		}
	}
	return nfds;
}
#endif

void services_thread(void* arg)
{
	char*			p;
	char			path[MAX_PATH+1];
	char			error[256];
	char			host_ip[64];
	char			compiler[32];
	char			str[128];
	char			services_ini[MAX_PATH+1];
	union xp_sockaddr	addr;
	socklen_t		addr_len;
	union xp_sockaddr	client_addr;
	socklen_t		client_addr_len;
	SOCKET			client_socket;
	BYTE*			udp_buf = NULL;
	int				udp_len;
	int				i;
	size_t			j;
	int				result;
	int				optval;
	ulong			total_running;
	time_t			t;
	time_t			initialized=0;
	ulong			total_sockets;
	service_client_t* client;
	char			*ssl_estr;
	int			level;
	BOOL			need_cert = FALSE;
#ifdef PREFER_POLL
	struct pollfd *fds = NULL;
	nfds_t nfds;
	nfds_t nfdsi;
#else
	fd_set			socket_set;
	SOCKET			high_socket;
	struct timeval	tv;
#endif

	startup=(services_startup_t*)arg;

    if(startup==NULL) {
    	sbbs_beep(100,500);
    	fprintf(stderr, "No startup structure passed!\n");
    	return;
    }

	if(startup->size!=sizeof(services_startup_t)) {	/* verify size */
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

	uptime=0;
	served=0;
	startup->recycle_now=FALSE;
	startup->shutdown_now=FALSE;

	SetThreadName("sbbs/services");

	do {
		/* Setup intelligent defaults */
		if(startup->sem_chk_freq==0)			startup->sem_chk_freq=DEFAULT_SEM_CHK_FREQ;
		if(startup->js.max_bytes==0)			startup->js.max_bytes=JAVASCRIPT_MAX_BYTES;

		thread_up(FALSE /* setuid */);

		status("Initializing");

		memset(&scfg, 0, sizeof(scfg));

		lprintf(LOG_INFO,"Synchronet Services Version %s%c%s"
			,VERSION, REVISION
#ifdef _DEBUG
			," Debug"
#else
			,""
#endif
			);

		DESCRIBE_COMPILER(compiler);

		lprintf(LOG_INFO,"Compiled %s/%s %s %s with %s", GIT_BRANCH, GIT_HASH, __DATE__, __TIME__, compiler);

		protected_uint32_init(&threads_pending_start,0);

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
		SAFECOPY(scfg.ctrl_dir, startup->ctrl_dir);
		lprintf(LOG_INFO,"Loading configuration files from %s", scfg.ctrl_dir);
		scfg.size=sizeof(scfg);
		SAFECOPY(error,UNKNOWN_LOAD_ERROR);
		if(!load_cfg(&scfg, text, /* prep: */TRUE, /* node: */FALSE, error, sizeof(error))) {
			lprintf(LOG_CRIT,"!ERROR %s",error);
			lprintf(LOG_CRIT,"!Failed to load configuration files");
			cleanup(1);
			return;
		}

		if(startup->temp_dir[0])
			SAFECOPY(scfg.temp_dir,startup->temp_dir);
		else
			SAFECOPY(scfg.temp_dir,"../temp");
	   	prep_dir(scfg.ctrl_dir, scfg.temp_dir, sizeof(scfg.temp_dir));
		if((i = md(scfg.temp_dir)) != 0) {
			lprintf(LOG_CRIT, "!ERROR %d (%s) creating directory: %s", i, strerror(i), scfg.temp_dir);
			cleanup(1);
			return;
		}
		lprintf(LOG_DEBUG,"Temporary file directory: %s", scfg.temp_dir);

		if((t=checktime())!=0) {   /* Check binary time */
			lprintf(LOG_ERR,"!TIME PROBLEM (%ld)",t);
		}

		if(uptime==0)
			uptime=time(NULL);	/* this must be done *after* setting the timezone */

		iniFileName(services_ini,sizeof(services_ini),scfg.ctrl_dir, startup->services_ini);

		if((service=read_services_ini(services_ini, service, &services))==NULL) {
			cleanup(1);
			return;
		}

		for(i=0; i < (int)services; i++)
			protected_uint32_init(&service[i].clients, 0);

		update_clients();

		/* Open and Bind Listening Sockets */
		total_sockets=0;

		for(i=0;i<(int)services && !startup->shutdown_now;i++) {
			if (service[i].options & SERVICE_OPT_TLS) {
				if (service[i].options & SERVICE_OPT_UDP) {
					lprintf(LOG_ERR, "Option error, TLS and UDP specified for %s", service[i].protocol);
					continue;
				}
				if (service[i].options & SERVICE_OPT_NATIVE) {
					lprintf(LOG_ERR, "Option error, TLS not yet supported for native services (%s)", service[i].protocol);
					continue;
				}
				if (service[i].options & SERVICE_OPT_STATIC) {
					lprintf(LOG_ERR, "Option error, TLS not yet supported for static services (%s)", service[i].protocol);
					continue;
				}
				if(scfg.tls_certificate == -1) {
					need_cert = TRUE;
				}
			}
			service[i].set=xpms_create(startup->bind_retry_count, startup->bind_retry_delay, lprintf);
			if(service[i].set == NULL) {
				lprintf(LOG_CRIT,"!ERROR creating %s socket set", service[i].protocol);
				cleanup(1);
				return;
			}
			xpms_add_list(service[i].set, PF_UNSPEC, (service[i].options&SERVICE_OPT_UDP) ? SOCK_DGRAM : SOCK_STREAM
					, IPPROTO_IP, service[i].interfaces, service[i].port, service[i].protocol
					, (service[i].options&SERVICE_OPT_UDP) ? service_udp_sock_cb : open_socket_cb, startup->seteuid, &service[i]);
			total_sockets += service[i].set->sock_count;
		}

		if(!total_sockets) {
			lprintf(LOG_WARNING,"0000 !No service sockets bound");
			cleanup(1);
			return;
		}

		/* Setup static service threads */
		for(i=0;i<(int)services;i++) {
			if(!(service[i].options&SERVICE_OPT_STATIC))
				continue;
			if(service[i].set==NULL)	/* bind failure? */
				continue;

			/* start thread here */
			if(service[i].options&SERVICE_OPT_NATIVE) {	/* Native */
				for(j=0; j<service[i].set->sock_count; j++) {
					struct native_service_instance	*inst=(struct native_service_instance *)malloc(sizeof(struct native_service_instance));
					if(inst) {
						inst->socket=service[i].set->socks[j].sock;
						inst->service=&service[i];
						protected_uint32_adjust(&threads_pending_start, 1);
						_beginthread(native_static_service_thread, service[i].stack_size, inst);
					}
				}
			}
			else {										/* JavaScript */
				protected_uint32_adjust(&threads_pending_start, 1);
				_beginthread(js_static_service_thread, service[i].stack_size, &service[i]);
			}
		}

		status("Listening");

		/* Setup recycle/shutdown semaphore file lists */
		shutdown_semfiles=semfile_list_init(scfg.ctrl_dir,"shutdown","services");
		recycle_semfiles=semfile_list_init(scfg.ctrl_dir,"recycle","services");
		semfile_list_add(&recycle_semfiles,startup->ini_fname);
		SAFEPRINTF(path,"%sservices.rec",scfg.ctrl_dir);	/* legacy */
		semfile_list_add(&recycle_semfiles,path);
		semfile_list_add(&recycle_semfiles,services_ini);
		if(!initialized) {
			semfile_list_check(&initialized,recycle_semfiles);
			semfile_list_check(&initialized,shutdown_semfiles);
		}

		terminated=FALSE;

		/* signal caller that we've started up successfully */
		if(startup->started!=NULL)
    		startup->started(startup->cbdata);

		if (need_cert) {
			if (get_ssl_cert(&scfg, &ssl_estr, &level) == -1) {
				if (ssl_estr) {
					lprintf(level, "No TLS certificiate %s", ssl_estr);
					free_crypt_attrstr(ssl_estr);
				}
			}
			need_cert = FALSE;
		}

		lprintf(LOG_INFO,"0000 Services thread started (%lu service sockets bound)", total_sockets);

#ifdef PREFER_POLL
		nfds = setup_poll(&fds);
		if (nfds == 0) {
			lprintf(LOG_CRIT, "!ERROR setting up poll() data");
			cleanup(1);
			return;
		}
#endif
		/* Main Server Loop */
		while(!terminated) {
			YIELD();
			if(active_clients()==0 && protected_uint32_value(threads_pending_start)==0) {
				if(!(startup->options&BBS_OPT_NO_RECYCLE)) {
					if((p=semfile_list_check(&initialized,recycle_semfiles))!=NULL) {
						lprintf(LOG_INFO,"0000 Recycle semaphore file (%s) detected",p);
						break;
					}
					if(startup->recycle_now==TRUE) {
						lprintf(LOG_NOTICE,"0000 Recycle semaphore signaled");
						startup->recycle_now=FALSE;
						break;
					}
				}
				if(((p=semfile_list_check(&initialized,shutdown_semfiles))!=NULL
						&& lprintf(LOG_INFO,"0000 Shutdown semaphore file (%s) detected",p))
					|| (startup->shutdown_now==TRUE
						&& lprintf(LOG_INFO,"0000 Shutdown semaphore signaled"))) {
					startup->shutdown_now=FALSE;
					terminated=TRUE;
					break;
				}
			}

#ifdef PREFER_POLL
			/* Clear poll FDs where necessary (ie: when !SERVICE_OPT_FULL_ACCEPT) */
			nfdsi = 0;
			for(i=0;i<(int)services;i++) {
				if(service[i].options&SERVICE_OPT_STATIC)
					continue;
				if(service[i].set==NULL)
					continue;
				if(!(service[i].options&SERVICE_OPT_FULL_ACCEPT)) {
					if (service[i].max_clients && protected_uint32_value(service[i].clients) >= service[i].max_clients) {
						for(j=0; j<service[i].set->sock_count; j++)
							fds[nfdsi + j].fd = -1;
					}
					else {
						for(j=0; j<service[i].set->sock_count; j++)
							fds[nfdsi + j].fd = service[i].set->socks[j].sock;
					}
				}
				nfdsi += service[i].set->sock_count;
			}

			if ((result = poll(fds, nfds, startup->sem_chk_freq * 1000)) < 1) {
				if(result==0)
					continue;

				if(ERROR_VALUE==EINTR)
					lprintf(LOG_DEBUG,"0000 Services listening interrupted");
				else
					lprintf(LOG_WARNING,"0000 !ERROR %d polling sockets: %s"
						, ERROR_VALUE, socket_strerror(socket_errno,error,sizeof(error)));
				continue;
			}
			nfdsi = 0;
			for(i=0;i<(int)services;i++) {
				if(service[i].options&SERVICE_OPT_STATIC)
					continue;
				if(service[i].set==NULL)
					continue;

				for(j=0; j<service[i].set->sock_count; j++) {
					if ((fds[nfdsi + j].revents) == 0)
						continue;
#else
			/* Setup select() parms */
			FD_ZERO(&socket_set);	
			high_socket=0;
			for(i=0;i<(int)services;i++) {
				if(service[i].options&SERVICE_OPT_STATIC)
					continue;
				if(service[i].set==NULL)
					continue;
				if(!(service[i].options&SERVICE_OPT_FULL_ACCEPT)
					&& service[i].max_clients && protected_uint32_value(service[i].clients) >= service[i].max_clients)
					continue;
				for(j=0; j<service[i].set->sock_count; j++) {
					FD_SET(service[i].set->socks[j].sock,&socket_set);
					if(service[i].set->socks[j].sock>high_socket)
						high_socket=service[i].set->socks[j].sock;
				}
			}
			if(high_socket==0) 	/* No dynamic services? */
				continue;
			tv.tv_sec=startup->sem_chk_freq;
			tv.tv_usec=0;
			if((result=select(high_socket+1,&socket_set,NULL,NULL,&tv))<1) {
				if(result==0)
					continue;

				if(ERROR_VALUE==EINTR)
					lprintf(LOG_DEBUG,"0000 Services listening interrupted");
				else if(ERROR_VALUE == ENOTSOCK)
            		lprintf(LOG_NOTICE,"0000 Services sockets closed");
				else
					lprintf(LOG_WARNING,"0000 !ERROR %d selecting sockets: %s"
						, ERROR_VALUE, socket_strerror(socket_errno,error,sizeof(error)));
				continue;
			}

			/* Determine who services this socket */
			for(i=0;i<(int)services;i++) {

				if(service[i].set==NULL)
					continue;

				for(j=0; j<service[i].set->sock_count; j++) {
					if(!FD_ISSET(service[i].set->socks[j].sock,&socket_set))
						continue;
#endif
					client_addr_len = sizeof(client_addr);

					udp_len=0;

					if(service[i].options&SERVICE_OPT_UDP) {
						/* UDP */
						if((udp_buf = (BYTE*)calloc(1, MAX_UDP_BUF_LEN)) == NULL) {
							lprintf(LOG_CRIT,"%04d %s !ERROR %d allocating UDP buffer"
								,service[i].set->socks[j].sock, service[i].protocol, errno);
							continue;
						}

						udp_len = recvfrom(service[i].set->socks[j].sock
							,udp_buf, MAX_UDP_BUF_LEN, 0 /* flags */
							,&client_addr.addr, &client_addr_len);
						if(udp_len<1) {
							FREE_AND_NULL(udp_buf);
							lprintf(LOG_WARNING,"%04d %s !ERROR %d recvfrom failed: %s"
								,service[i].set->socks[j].sock, service[i].protocol, ERROR_VALUE, socket_strerror(socket_errno,error,sizeof(error)));
							continue;
						}

						if((client_socket = open_socket(service[i].set->socks[j].domain, SOCK_DGRAM, &service[i]))
							==INVALID_SOCKET) {
							FREE_AND_NULL(udp_buf);
							lprintf(LOG_ERR,"%04d %s !ERROR %d opening socket: %s"
								,service[i].set->socks[j].sock, service[i].protocol, ERROR_VALUE, socket_strerror(socket_errno,error,sizeof(error)));
							continue;
						}

						lprintf(LOG_DEBUG,"%04d %s created client socket: %d"
							,service[i].set->socks[j].sock, service[i].protocol, client_socket);

						/* We need to set the REUSE ADDRESS socket option */
						optval=TRUE;
						if(setsockopt(client_socket,SOL_SOCKET,SO_REUSEADDR
							,(char*)&optval,sizeof(optval))!=0) {
							FREE_AND_NULL(udp_buf);
							lprintf(LOG_ERR,"%04d %s !ERROR %d setting socket option: %s"
								,client_socket, service[i].protocol, ERROR_VALUE, socket_strerror(socket_errno,error,sizeof(error)));
							close_socket(client_socket);
							continue;
						}
					   #ifdef BSD
						if(setsockopt(client_socket,SOL_SOCKET,SO_REUSEPORT
							,(char*)&optval,sizeof(optval))!=0) {
							FREE_AND_NULL(udp_buf);
							lprintf(LOG_ERR,"%04d %s !ERROR %d setting socket option: %s"
								,client_socket, service[i].protocol, ERROR_VALUE, socket_strerror(socket_errno,error,sizeof(error)));
							close_socket(client_socket);
							continue;
						}
					   #endif

						addr_len = sizeof(addr);
						getsockname(service[i].set->socks[j].sock, &addr.addr, &addr_len);
						result=bind(client_socket, &addr.addr, addr_len);
						if(result==SOCKET_ERROR) {
							/* Failed to re-bind to same port number, use user port */
							lprintf(LOG_NOTICE,"%04d %s ERROR %d re-binding socket to port %u failed, "
								"using user port: %s"
								,client_socket, service[i].protocol, ERROR_VALUE, service[i].port, socket_strerror(socket_errno,error,sizeof(error)));
							inet_setaddrport(&addr, 0);
							result=bind(client_socket, (struct sockaddr *) &addr, addr_len);
						}
						if(result!=0) {
							FREE_AND_NULL(udp_buf);
							lprintf(LOG_ERR,"%04d %s !ERROR %d re-binding socket to port %u: %s"
								,client_socket, service[i].protocol, ERROR_VALUE, service[i].port, socket_strerror(socket_errno,error,sizeof(error)));
							close_socket(client_socket);
							continue;
						}

						/* Set client address as default addres for send/recv */
						if(connect(client_socket
							,(struct sockaddr *)&client_addr, client_addr_len)!=0) {
							FREE_AND_NULL(udp_buf);
							lprintf(LOG_ERR,"%04d %s !ERROR %d connect failed: %s"
								,client_socket, service[i].protocol, ERROR_VALUE, socket_strerror(socket_errno,error,sizeof(error)));
							close_socket(client_socket);
							continue;
						}

					} else { 
						/* TCP */
						if((client_socket=accept(service[i].set->socks[j].sock
							,(struct sockaddr *)&client_addr, &client_addr_len))==INVALID_SOCKET) {
							if(ERROR_VALUE == ENOTSOCK || ERROR_VALUE == EINVAL)
								lprintf(LOG_NOTICE,"%04d %s socket closed while listening"
									,service[i].set->socks[j].sock, service[i].protocol);
							else
								lprintf(LOG_WARNING,"%04d %s !ERROR %d accepting connection: %s" 
									,service[i].set->socks[j].sock, service[i].protocol, ERROR_VALUE, socket_strerror(socket_errno,error,sizeof(error)));
	#ifdef _WIN32
							if(WSAGetLastError()==WSAENOBUFS)	/* recycle (re-init WinSock) on this error */
								break;
	#endif
							continue;
						}
						if(startup->socket_open!=NULL)	/* Callback, increments socket counter */
							startup->socket_open(startup->cbdata,TRUE);	
					}
					inet_addrtop(&client_addr, host_ip, sizeof(host_ip));

					if(trashcan(&scfg,host_ip,"ip-silent")) {
						FREE_AND_NULL(udp_buf);
						close_socket(client_socket);
						continue;
					}

					if(service[i].log_level >= LOG_INFO)
						lprintf(LOG_INFO,"%04d %s connection accepted from: %s port %u"
							,client_socket
							,service[i].protocol, host_ip, inet_addrport(&client_addr));

					if(service[i].max_clients && protected_uint32_value(service[i].clients) + 1 > service[i].max_clients) {
						FREE_AND_NULL(udp_buf);
						lprintf(LOG_WARNING,"%04d %s !MAXIMUM CLIENTS (%u) reached, access denied"
							,client_socket, service[i].protocol, service[i].max_clients);
						close_socket(client_socket);
						continue;
					}

					login_attempt_t attempted;
					ulong banned = loginBanned(&scfg, startup->login_attempt_list, client_socket, /* host_name: */NULL, startup->login_attempt, &attempted);
					if(banned || trashcan(&scfg,host_ip,"ip")) {
						if(banned) {
							char ban_duration[128];
							lprintf(LOG_NOTICE, "%04d !TEMPORARY BAN of %s (%lu login attempts, last: %s) - remaining: %s"
								,client_socket, host_ip, attempted.count-attempted.dupes, attempted.user, seconds_to_str(banned, ban_duration));
						} else
							lprintf(LOG_NOTICE,"%04d %s !CLIENT BLOCKED in ip.can: %s"
								,client_socket, service[i].protocol, host_ip);
						FREE_AND_NULL(udp_buf);
						close_socket(client_socket);
						continue;
					}

	#ifdef _WIN32
					if(startup->sound.answer[0] && !sound_muted(&scfg)
						&& !(service[i].options&BBS_OPT_MUTE))
						PlaySound(startup->sound.answer, NULL, SND_ASYNC|SND_FILENAME);
	#endif

					if((client=malloc(sizeof(service_client_t)))==NULL) {
						FREE_AND_NULL(udp_buf);
						lprintf(LOG_CRIT,"%04d %s !ERROR allocating %lu bytes of memory for service_client"
							,client_socket, service[i].protocol, (ulong)sizeof(service_client_t));
						close_socket(client_socket);
						continue;
					}

					memset(client,0,sizeof(service_client_t));
					client->socket=client_socket;
					client->addr=client_addr;
					client->service=&service[i];
					protected_uint32_adjust(&client->service->clients, 1);
					client->udp_buf=udp_buf;
					client->udp_len=udp_len;
					client->callback.limit			= service[i].js.time_limit;
					client->callback.gc_interval	= service[i].js.gc_interval;
					client->callback.yield_interval	= service[i].js.yield_interval;
					client->callback.terminated		= &client->service->terminated;
					client->callback.auto_terminate	= TRUE;

					udp_buf = NULL;

					protected_uint32_adjust(&threads_pending_start, 1);
					if(service[i].options&SERVICE_OPT_NATIVE)	/* Native */
						_beginthread(native_service_thread, service[i].stack_size, client);
					else										/* JavaScript */
						_beginthread(js_service_thread, service[i].stack_size, client);
					service[i].served++;
					served++;
				}
#ifdef PREFER_POLL
				nfdsi += service[i].set->sock_count;
#endif
			}
		}
#ifdef PREFER_POLL
		FREE_AND_NULL(fds);
#endif

		/* Close Service Sockets */
		lprintf(LOG_DEBUG,"0000 Closing service sockets");
		for(i=0;i<(int)services;i++) {
			service[i].terminated=TRUE;
			if(service[i].set==NULL)
				continue;
			if(service[i].options&SERVICE_OPT_STATIC)
				continue;
			xpms_destroy(service[i].set, close_socket_cb, &service[i]);
			service[i].set=NULL;
		}

		/* Wait for Dynamic Service Threads to terminate */
		if(active_clients()) {
			lprintf(LOG_INFO,"0000 Waiting for %lu clients to disconnect",active_clients());
			while(active_clients()) {
				mswait(500);
			}
			lprintf(LOG_INFO,"0000 Done waiting for clients to disconnect");
		}

		/* Wait for Static Service Threads to terminate */
		total_running=0;
		for(i=0;i<(int)services;i++) 
			total_running+=service[i].running;
		if(total_running) {
			lprintf(LOG_INFO,"0000 Waiting for %lu static services to terminate",total_running);
			while(1) {
				total_running=0;
				for(i=0;i<(int)services;i++) 
					total_running+=service[i].running;
				if(!total_running)
					break;
				mswait(500);
			}
			lprintf(LOG_INFO,"0000 Done waiting for static services to terminate");
		}

		cleanup(0);
		if(!terminated) {
			lprintf(LOG_INFO,"Recycling server...");
			mswait(2000);
			if(startup->recycle!=NULL)
				startup->recycle(startup->cbdata);
		}

	} while(!terminated);
}
