/* js_socket.c */

/* Synchronet JavaScript "Socket" Object */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2001 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#ifdef JAVASCRIPT

BOOL debug=FALSE;

static void dbprintf(BOOL error, SOCKET sock, char* fmt, ...)
{
	va_list argptr;
	char sbuf[1024];

	if(!debug && !error)
		return;

    va_start(argptr,fmt);
    vsprintf(sbuf,fmt,argptr);
    va_end(argptr);
	
	lprintf("%04u Socket %s%s",sock,error ? "ERROR: ":"",sbuf);
}

/* Socket Constructor (creates socket descriptor) */

static JSBool
js_socket_constructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int		type=SOCK_STREAM;	/* default = TCP */
	SOCKET	sock;

	if(argc)
		type=JSVAL_TO_INT(argv[0]);

	if((sock=open_socket(type))==INVALID_SOCKET) {
		dbprintf(TRUE, 0, "open_socket failed with error %d",ERROR_VALUE);
		*rval = JSVAL_VOID;
		return JS_FALSE;
	}

	if(!JS_SetPrivate(cx, obj, (char*)(sock<<1))) {
		dbprintf(TRUE, sock, "JS_SetPrivate failed");
		*rval = JSVAL_VOID;
		return JS_FALSE;
	}

	dbprintf(FALSE, sock, "opened");
	return JS_TRUE;
}

/* Socket Destructor */

static void js_finalize_socket(JSContext *cx, JSObject *obj)
{
	SOCKET	sock;
	
	sock=(uint)JS_GetPrivate(cx,obj)>>1;

	if(sock==INVALID_SOCKET || sock==0)
		return;

	close_socket(sock);

	dbprintf(FALSE, sock, "closed");

	sock=INVALID_SOCKET;

	JS_SetPrivate(cx, obj, (char*)(sock<<1));
}


/* Socket Object Methods */

static JSBool
js_bind(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int			i;
	SOCKET		sock;
	SOCKADDR_IN	addr;
	
	sock=(uint)JS_GetPrivate(cx,obj)>>1;

	memset(&addr,0,sizeof(addr));
	addr.sin_family = AF_INET;

	if(argc)
		addr.sin_port = (ushort)JSVAL_TO_INT(argv[0]);

	if((i=bind(sock, (struct sockaddr *) &addr, sizeof (addr)))!=0) {
		dbprintf(TRUE, sock, "bind failed with error %d",ERROR_VALUE);
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return JS_TRUE;
	}

	dbprintf(FALSE, sock, "bound to port %u",addr.sin_port);
	*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
	return JS_TRUE;
}

static JSBool
js_connect(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int			i;
	ulong		ip_addr;
	ushort		port;
	JSString*	str;
	SOCKET		sock;
	SOCKADDR_IN	addr;

	sock=(uint)JS_GetPrivate(cx,obj)>>1;

	if(argc<2)
		return JS_FALSE;

	str = JS_ValueToString(cx, argv[0]);
	if((ip_addr=resolve_ip(JS_GetStringBytes(str)))==0) {
		dbprintf(TRUE, sock, "resolve_ip failed with error %d",ERROR_VALUE);
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return JS_TRUE;
	}

	port = (ushort)JSVAL_TO_INT(argv[1]);

	dbprintf(FALSE, sock, "connecting to port %u at %s", port, JS_GetStringBytes(str));

	memset(&addr,0,sizeof(addr));
	addr.sin_addr.s_addr = ip_addr;
	addr.sin_family = AF_INET;
	addr.sin_port   = htons(port);

	if((i=connect(sock, (struct sockaddr *)&addr, sizeof(addr)))!=0) {
		dbprintf(TRUE, sock, "connect failed with error %d",ERROR_VALUE);
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return JS_TRUE;
	}

	*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
	dbprintf(FALSE, sock, "connected to port %u at %s", port, JS_GetStringBytes(str));

	return(JS_TRUE);
}

static JSBool
js_send(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		p;
	int			len;
	SOCKET		sock;
	JSString*	str;

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);

	sock=(uint)JS_GetPrivate(cx,obj)>>1;

	if(!argc)
		return(JS_FALSE);

	str = JS_ValueToString(cx, argv[0]);
	p=JS_GetStringBytes(str);
	len=strlen(p);

	if(send(sock,p,len,0)==len) {
		dbprintf(FALSE, sock, "sent %u bytes",len);
		*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
	} else
		dbprintf(TRUE, sock, "send of %u bytes failed",len);
		
	return(JS_TRUE);
}

static JSBool
js_recv(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char		buf[513];
	int			len;
	SOCKET		sock;
	JSString*	str;

	sock=(uint)JS_GetPrivate(cx,obj)>>1;

	if(argc)
		len = JSVAL_TO_INT(argv[0]);
	else
		len = sizeof(buf)-1;

	len = len > sizeof(buf)-1 ? sizeof(buf)-1 : len;

	len = recv(sock,buf,len,0);
	if(len<0)
		len=0;

	buf[len]=0;

	str = JS_NewStringCopyZ(cx, buf);

	*rval = STRING_TO_JSVAL(str);

	dbprintf(FALSE, sock, "received %u bytes",len);
		
	return(JS_TRUE);
}

static JSBool
js_peek(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char		buf[513];
	int			len;
	SOCKET		sock;
	JSString*	str;

	sock=(uint)JS_GetPrivate(cx,obj)>>1;

	if(argc)
		len = JSVAL_TO_INT(argv[0]);
	else
		len = sizeof(buf)-1;

	len = len > sizeof(buf)-1 ? sizeof(buf)-1 : len;

	len = recv(sock,buf,len,MSG_PEEK);
	if(len<0)
		len=0;

	buf[len]=0;

	str = JS_NewStringCopyZ(cx, buf);

	*rval = STRING_TO_JSVAL(str);

	dbprintf(FALSE, sock, "received %u bytes",len);
		
	return(JS_TRUE);
}

#define TIMEOUT_SOCK_READLINE	30	/* seconds */

static JSBool
js_recvline(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char		ch;
	char		buf[513];
	int			i;
	int			len;
	BOOL		rd;
	SOCKET		sock;
	time_t		start;
	JSString*	str;

	sock=(uint)JS_GetPrivate(cx,obj)>>1;

	if(argc)
		len = JSVAL_TO_INT(argv[0]);
	else
		len = sizeof(buf)-1;

	len = len > sizeof(buf)-1 ? sizeof(buf)-1 : len;

	start=time(NULL);
	for(i=0;i<len;i++) {

		if(!socket_check(sock,&rd))
			break;		/* disconnected */

		if(time(NULL)-start>TIMEOUT_SOCK_READLINE) 
			break;		/* time-out */

		if(!rd)
			continue;	/* no data */

		if(recv(sock, &ch, 1, 0)!=1) 
			break;

		if(ch=='\n' && i>=1) 
			break;

		buf[i]=ch;
	}
	if(i>0)
		buf[i-1]=0;
	else
		buf[0]=0;

	str = JS_NewStringCopyZ(cx, buf);

	*rval = STRING_TO_JSVAL(str);

	dbprintf(FALSE, sock, "received %u bytes",strlen(buf));
		
	return(JS_TRUE);
}

static JSBool
js_getsockopt(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int			lvl;
	int			opt;
	int			len;
	int			val;
	SOCKET		sock;
	
	sock=(uint)JS_GetPrivate(cx,obj)>>1;

	lvl = JSVAL_TO_INT(argv[0]);
	opt = JSVAL_TO_INT(argv[1]);
	len = sizeof(val);

	if(getsockopt(sock,lvl,opt,(char*)&val,&len)==0) {
		dbprintf(FALSE, sock, "option %d (level: %d) = %d",opt,lvl,val);
		*rval = INT_TO_JSVAL(val);
	} else {
		dbprintf(TRUE, sock, "error %d getting option %d (level: %d)"
			,ERROR_VALUE,opt,lvl);
		*rval = INT_TO_JSVAL(-1);
	}

	return(JS_TRUE);
}


static JSBool
js_setsockopt(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int			lvl;
	int			opt;
	int			val;
	SOCKET		sock;
	
	sock=(uint)JS_GetPrivate(cx,obj)>>1;

	lvl = JSVAL_TO_INT(argv[0]);
	opt = JSVAL_TO_INT(argv[1]);
	val = JSVAL_TO_INT(argv[2]);

	*rval = BOOLEAN_TO_JSVAL(setsockopt(sock,lvl,opt,(char*)&val,sizeof(val))==0);

	return(JS_TRUE);
}

/* Socket Object Properites */
enum {
	 SOCK_PROP_LAST_ERROR
	,SOCK_PROP_IS_CONNECTED
	,SOCK_PROP_DATA_WAITING
	,SOCK_PROP_NREAD
	,SOCK_PROP_DEBUG
};

static JSBool js_socket_set(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint       tiny;
	SOCKET		sock;

	sock=(uint)JS_GetPrivate(cx,obj)>>1;

    tiny = JSVAL_TO_INT(id);

	dbprintf(FALSE, sock, "setting property %d",tiny);

	switch(tiny) {
		case SOCK_PROP_DEBUG:
			debug = JSVAL_TO_BOOLEAN(*vp);
			break;
	}

	return(JS_TRUE);
}

static JSBool js_socket_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint       tiny;
	ulong		cnt;
	BOOL		rd;
	SOCKET		sock;

	sock=(uint)JS_GetPrivate(cx,obj)>>1;

    tiny = JSVAL_TO_INT(id);

#if 0 /* just too much */
	dbprintf(FALSE, sock, "getting property %d",tiny);
#endif

	switch(tiny) {
		case SOCK_PROP_LAST_ERROR:
			*vp = INT_TO_JSVAL(ERROR_VALUE);
			break;
		case SOCK_PROP_IS_CONNECTED:
			*vp = BOOLEAN_TO_JSVAL(socket_check(sock,NULL));
			break;
		case SOCK_PROP_DATA_WAITING:
			socket_check(sock,&rd);
			*vp = BOOLEAN_TO_JSVAL(rd);
			break;
		case SOCK_PROP_NREAD:
			cnt=0;
			if(ioctlsocket(sock, FIONREAD, &cnt)==0) 
				*vp = INT_TO_JSVAL(cnt);
			else
				*vp = INT_TO_JSVAL(0);
			break;
		case SOCK_PROP_DEBUG:
			*vp = INT_TO_JSVAL(debug);
			break;
	}

	return(TRUE);
}

#define OBJ_FLAGS JSPROP_ENUMERATE|JSPROP_READONLY

static struct JSPropertySpec js_socket_properties[] = {
/*		 name				,tinyid					,flags,				getter,	setter	*/

	{	"last_error"		,SOCK_PROP_LAST_ERROR	,OBJ_FLAGS,			NULL,NULL},
	{	"is_connected"		,SOCK_PROP_IS_CONNECTED	,OBJ_FLAGS,			NULL,NULL},
	{	"data_waiting"		,SOCK_PROP_DATA_WAITING	,OBJ_FLAGS,			NULL,NULL},
	{	"nread"				,SOCK_PROP_NREAD		,OBJ_FLAGS,			NULL,NULL},
	{	"debug"				,SOCK_PROP_DEBUG		,JSPROP_ENUMERATE,	NULL,NULL},
	{0}
};

static JSClass js_socket_class = {
     "Socket"				/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_socket_get			/* getProperty	*/
	,js_socket_set			/* setProperty	*/
	,JS_EnumerateStub		/* enumerate	*/
	,JS_ResolveStub			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,js_finalize_socket		/* finalize		*/
};

static JSFunctionSpec js_socket_functions[] = {
	{"bind",			js_bind,			0},		/* bind to a port */
	{"connect",         js_connect,         2},		/* connect to an IP address and port */
	{"send",			js_send,			1},		/* send a string */
	{"write",			js_send,			1},		/* send a string */
	{"recv",			js_recv,			0},		/* receive a string */
	{"read",			js_recv,			0},		/* receive a string */
	{"peek",			js_peek,			0},		/* receive a string, leave in buffer */
	{"recvline",		js_recvline,		0},		/* receive a \n terminated string	*/
	{"readline",		js_recvline,		0},		/* receive a \n terminated string	*/
	{"getoption",		js_getsockopt,		2},		/* getsockopt(level,opt)			*/
	{"setoption",		js_setsockopt,		3},		/* setsockopt(level,opt,val)		*/
	{0}
};


JSObject* DLLCALL js_CreateSocketClass(JSContext* cx, JSObject* parent)
{
	JSObject*	sockobj;

	sockobj = JS_InitClass(cx, parent, NULL
		,&js_socket_class
		,js_socket_constructor
		,0	/* number of constructor args */
		,js_socket_properties
		,js_socket_functions
		,NULL,NULL);

	return(sockobj);
}

#endif	/* JAVSCRIPT */