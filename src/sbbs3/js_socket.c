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

typedef struct
{
	SOCKET	sock;
	BOOL	external;	/* externally created, don't close */
	BOOL	debug;
	BOOL	nonblocking;
	BOOL	is_connected;
	int		last_error;
	int		type;

} private_t;


static void dbprintf(BOOL error, private_t* p, char* fmt, ...)
{
	va_list argptr;
	char sbuf[1024];

	if(p==NULL || (!p->debug /*&& !error */))
		return;

    va_start(argptr,fmt);
    vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
    va_end(argptr);
	
	lprintf("%04d Socket %s%s",p->sock,error ? "ERROR: ":"",sbuf);
}

/* Socket Constructor (creates socket descriptor) */

static JSBool
js_socket_constructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int		type=SOCK_STREAM;	/* default = TCP */
	private_t* p;

	if(argc)
		type=JSVAL_TO_INT(argv[0]);

	*rval = JSVAL_VOID;

	if((p=(private_t*)malloc(sizeof(private_t)))==NULL) {
		dbprintf(TRUE, 0, "open_socket malloc failed");
		return(JS_FALSE);
	}
	memset(p,0,sizeof(private_t));

	if((p->sock=open_socket(type))==INVALID_SOCKET) {
		dbprintf(TRUE, 0, "open_socket failed with error %d",ERROR_VALUE);
		return(JS_FALSE);
	}
	p->type = type;

	if(!JS_SetPrivate(cx, obj, p)) {
		dbprintf(TRUE, p, "JS_SetPrivate failed");
		return(JS_FALSE);
	}

	dbprintf(FALSE, p, "object constructed");
	return(JS_TRUE);
}

/* Socket Destructor */

static void js_finalize_socket(JSContext *cx, JSObject *obj)
{
	private_t* p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return;

	if(p->external==FALSE && p->sock!=INVALID_SOCKET) {
		close_socket(p->sock);
		dbprintf(FALSE, p, "closed/deleted");
	}

	free(p);

	JS_SetPrivate(cx, obj, NULL);
}


/* Socket Object Methods */

static JSBool
js_close(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	private_t*	p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	*rval = JSVAL_VOID;

	if(p->sock==INVALID_SOCKET)
		return(JS_TRUE);

	close_socket(p->sock);

	p->last_error = ERROR_VALUE;

	dbprintf(FALSE, p, "closed");

	p->sock = INVALID_SOCKET; 
	p->is_connected = FALSE;

	return(JS_TRUE);
}

static ushort js_port(JSContext* cx, jsval val, int type)
{
	char*			cp;
	JSString*		str;
	struct servent*	serv;

	if(JSVAL_IS_INT(val))
		return((ushort)JSVAL_TO_INT(val));
	if(JSVAL_IS_STRING(val)) {
		str = JS_ValueToString(cx,val);
		cp = JS_GetStringBytes(str);
		if(isdigit(*cp))
			return((ushort)strtol(cp,NULL,0));
		serv = getservbyname(cp,type==SOCK_STREAM ? "tcp":"udp");
		if(serv!=NULL)
			return(htons(serv->s_port));
	}
	return(0);
}

static JSBool
js_bind(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	SOCKADDR_IN	addr;
	private_t*	p;
	ushort		port=0;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);
	
	memset(&addr,0,sizeof(addr));
	addr.sin_family = AF_INET;

	if(argc)
		port = js_port(cx,argv[0],p->type);
	addr.sin_port = htons(port);

	if(bind(p->sock, (struct sockaddr *) &addr, sizeof(addr))!=0) {
		p->last_error=ERROR_VALUE;
		dbprintf(TRUE, p, "bind failed with error %d",ERROR_VALUE);
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	dbprintf(FALSE, p, "bound to port %u",port);
	*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
	return(JS_TRUE);
}

static JSBool
js_listen(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	private_t*	p;
	int32		backlog=1;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);
	
	if(argc)
		backlog = JS_ValueToInt32(cx,argv[0],&backlog);

	if(listen(p->sock, backlog)!=0) {
		p->last_error=ERROR_VALUE;
		dbprintf(TRUE, p, "listen failed with error %d",ERROR_VALUE);
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	dbprintf(FALSE, p, "listening, backlog=%d",backlog);
	*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
	return(JS_TRUE);
}

static JSBool
js_accept(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	private_t*	p;
	private_t*	new_p;
	JSObject*	sockobj;
	SOCKET		new_socket;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	if((new_socket=accept_socket(p->sock,NULL,NULL))==INVALID_SOCKET) {
		p->last_error=ERROR_VALUE;
		dbprintf(TRUE, p, "accept failed with error %d",ERROR_VALUE);
		*rval = JSVAL_VOID;
		return(JS_TRUE);
	}

	if((sockobj=js_CreateSocketObject(cx, obj, "new_socket", new_socket))==NULL) {
		closesocket(new_socket);
		dbprintf(TRUE, p, "error creating new_socket object");
		*rval = JSVAL_VOID;
		return(JS_TRUE);
	}
	if((new_p=(private_t*)JS_GetPrivate(cx,sockobj))==NULL)
		return(JS_FALSE);

	new_p->type=p->type;
	new_p->debug=p->debug;
	new_p->nonblocking=p->nonblocking;
	new_p->external=FALSE;		/* let destructor close socket */
	new_p->is_connected=TRUE;

	dbprintf(FALSE, p, "accepted connection");
	*rval = OBJECT_TO_JSVAL(sockobj);
	return(JS_TRUE);
}

static JSBool
js_connect(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	ulong		ip_addr;
	ushort		port;
	JSString*	str;
	private_t*	p;
	SOCKADDR_IN	addr;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	if(argc<2)
		return(JS_FALSE);

	str = JS_ValueToString(cx, argv[0]);
	dbprintf(FALSE, p, "resolving hostname: %s", JS_GetStringBytes(str));
	if((ip_addr=resolve_ip(JS_GetStringBytes(str)))==0) {
		p->last_error=ERROR_VALUE;
		dbprintf(TRUE, p, "resolve_ip failed with error %d",ERROR_VALUE);
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	port = js_port(cx,argv[1],p->type);

	dbprintf(FALSE, p, "connecting to port %u at %s", port, JS_GetStringBytes(str));

	memset(&addr,0,sizeof(addr));
	addr.sin_addr.s_addr = ip_addr;
	addr.sin_family = AF_INET;
	addr.sin_port   = htons(port);

	if(connect(p->sock, (struct sockaddr *)&addr, sizeof(addr))!=0) {
		p->last_error=ERROR_VALUE;
		dbprintf(TRUE, p, "connect failed with error %d",ERROR_VALUE);
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	p->is_connected = TRUE;
	*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
	dbprintf(FALSE, p, "connected to port %u at %s", port, JS_GetStringBytes(str));

	return(JS_TRUE);
}

static JSBool
js_send(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		cp;
	int			len;
	JSString*	str;
	private_t*	p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);

	if(!argc)
		return(JS_FALSE);

	str = JS_ValueToString(cx, argv[0]);
	cp=JS_GetStringBytes(str);
	len=strlen(cp);

	if(sendsocket(p->sock,cp,len)==len) {
		dbprintf(FALSE, p, "sent %u bytes",len);
		*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
	} else {
		p->last_error=ERROR_VALUE;
		dbprintf(TRUE, p, "send of %u bytes failed",len);
	}
		
	return(JS_TRUE);
}

static JSBool
js_sendto(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		cp;
	int			len;
	ulong		ip_addr;
	ushort		port;
	JSString*	data_str;
	JSString*	ip_str;
	private_t*	p;
	SOCKADDR_IN	addr;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);

	if(argc<3)
		return(JS_FALSE);

	/* data */
	data_str = JS_ValueToString(cx, argv[0]);
	cp = JS_GetStringBytes(data_str);
	len = strlen(cp);

	/* address */
	ip_str = JS_ValueToString(cx, argv[1]);
	dbprintf(FALSE, p, "resolving hostname: %s", JS_GetStringBytes(ip_str));
	if((ip_addr=resolve_ip(JS_GetStringBytes(ip_str)))==0) {
		p->last_error=ERROR_VALUE;
		dbprintf(TRUE, p, "resolve_ip failed with error %d",ERROR_VALUE);
		*rval = BOOLEAN_TO_JSVAL(JS_FALSE);
		return(JS_TRUE);
	}

	/* port */
	port = (ushort)JSVAL_TO_INT(argv[2]);

	dbprintf(FALSE, p, "sending %d bytes to port %u at %s"
		,len, port, JS_GetStringBytes(ip_str));

	memset(&addr,0,sizeof(addr));
	addr.sin_addr.s_addr = ip_addr;
	addr.sin_family = AF_INET;
	addr.sin_port   = htons(port);

	if(sendto(p->sock,cp,len,0 /* flags */,(SOCKADDR*)&addr,sizeof(addr))==len) {
		dbprintf(FALSE, p, "sent %u bytes",len);
		*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
	} else {
		p->last_error=ERROR_VALUE;
		dbprintf(TRUE, p, "send of %u bytes failed",len);
	}

	return(JS_TRUE);
}


static JSBool
js_sendfile(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		fname;
	char*		buf;
	long		len;
	int			file;
	JSString*	str;
	private_t*	p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	*rval = BOOLEAN_TO_JSVAL(JS_FALSE);

	if(!argc)
		return(JS_FALSE);

	if((str = JS_ValueToString(cx, argv[0]))==NULL)
		return(JS_FALSE);
	if((fname=JS_GetStringBytes(str))==NULL)
		return(JS_FALSE);

	if((file=nopen(fname,O_RDONLY|O_BINARY))==-1)
		return(JS_TRUE);

	len=filelength(file);
	if((buf=malloc(len))==NULL) {
		close(file);
		return(JS_TRUE);
	}
	if(read(file,buf,len)!=len) {
		free(buf);
		close(file);
		return(JS_TRUE);
	}
	close(file);

	if(sendsocket(p->sock,buf,len)==len) {
		dbprintf(FALSE, p, "sent %u bytes",len);
		*rval = BOOLEAN_TO_JSVAL(JS_TRUE);
	} else {
		p->last_error=ERROR_VALUE;
		dbprintf(TRUE, p, "send of %u bytes failed",len);
	}
	free(buf);

	return(JS_TRUE);
}

static JSBool
js_recv(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		buf;
	int			len=512;
	JSString*	str;

	private_t*	p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	if(argc)
		len = JSVAL_TO_INT(argv[0]);

	if((buf=(char*)malloc(len+1))==NULL) {
		dbprintf(TRUE, p, "error allocating %u bytes",len+1);
		return(JS_FALSE);
	}

	len = recv(p->sock,buf,len,0);
	if(len<0) {
		free(buf);
		p->last_error=ERROR_VALUE;
		*rval = JSVAL_NULL;
		return(JS_TRUE);
	}
	buf[len]=0;

	str = JS_NewStringCopyZ(cx, buf);
	free(buf);
	*rval = STRING_TO_JSVAL(str);

	dbprintf(FALSE, p, "received %u bytes",len);
		
	return(JS_TRUE);
}

static JSClass js_recvfrom_class = {
     "Recvfrom"				/* name			*/
    ,0						/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,JS_PropertyStub		/* getProperty	*/
	,JS_PropertyStub		/* setProperty	*/
	,JS_EnumerateStub		/* enumerate	*/
	,JS_ResolveStub			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,JS_FinalizeStub		/* finalize		*/
};


static JSBool
js_recvfrom(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		buf;
	char		ip_addr[64];
	char		port[32];
	int			len=512;
	JSString*	str;
	JSObject*	retobj;
	SOCKADDR_IN	addr;
	socklen_t	addrlen;

	private_t*	p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	if(argc)
		len = JSVAL_TO_INT(argv[0]);

	if((buf=(char*)malloc(len+1))==NULL) {
		dbprintf(TRUE, p, "error allocating %u bytes",len+1);
		return(JS_FALSE);
	}

	addrlen=sizeof(addr);
	len = recvfrom(p->sock,buf,len,0,(SOCKADDR*)&addr,&addrlen);
	if(len<0) {
		free(buf);
		p->last_error=ERROR_VALUE;
		*rval = JSVAL_NULL;
		return(JS_TRUE);
	}
	buf[len]=0;

	str = JS_NewStringCopyZ(cx, buf);
	free(buf);

	if((retobj=JS_NewObject(cx,&js_recvfrom_class,NULL,obj))==NULL)
		return(JS_TRUE);

	sprintf(port,"%u",ntohs(addr.sin_port));
	JS_DefineProperty(cx, retobj, "port"
		,STRING_TO_JSVAL(JS_NewStringCopyZ(cx,port))
		,NULL,NULL,JSPROP_ENUMERATE);

	SAFECOPY(ip_addr,inet_ntoa(addr.sin_addr));
	JS_DefineProperty(cx, retobj, "ip_address"
		,STRING_TO_JSVAL(JS_NewStringCopyZ(cx,ip_addr))
		,NULL,NULL,JSPROP_ENUMERATE);

	JS_DefineProperty(cx, retobj, "data"
		,STRING_TO_JSVAL(str)
		,NULL,NULL,JSPROP_ENUMERATE);

	*rval = OBJECT_TO_JSVAL(retobj);

	dbprintf(FALSE, p, "received %u bytes from %s:%s",len,ip_addr,port);
		
	return(JS_TRUE);
}


static JSBool
js_peek(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		buf;
	int			len;
	JSString*	str;

	private_t*	p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	if(argc)
		len = JSVAL_TO_INT(argv[0]);

	if((buf=(char*)malloc(len+1))==NULL) {
		dbprintf(TRUE, p, "error allocating %u bytes",len+1);
		return(JS_FALSE);
	}
	len = recv(p->sock,buf,len,MSG_PEEK);
	if(len<0) {
		free(buf);
		p->last_error=ERROR_VALUE;	
		*rval = JSVAL_NULL;
		return(JS_TRUE);
	}
	buf[len]=0;

	str = JS_NewStringCopyZ(cx, buf);
	free(buf);
	*rval = STRING_TO_JSVAL(str);

	dbprintf(FALSE, p, "received %u bytes, lasterror=%d"
		,len,ERROR_VALUE);
		
	return(JS_TRUE);
}

static JSBool
js_recvline(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char		ch;
	char*		buf;
	int			i;
	int			len=512;
	BOOL		rd;
	time_t		start;
	time_t		timeout=30;	/* seconds */
	JSString*	str;
	private_t*	p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	if(argc)
		len = JSVAL_TO_INT(argv[0]);

	if((buf=(char*)malloc(len+1))==NULL) {
		dbprintf(TRUE, p, "error allocating %u bytes",len+1);
		return(JS_FALSE);
	}

	if(argc>1)
		timeout = JSVAL_TO_INT(argv[1]);

	start=time(NULL);
	for(i=0;i<len;) {

		if(!socket_check(p->sock,&rd)) {
			p->last_error=ERROR_VALUE;
			break;		/* disconnected */
		}

		if(!rd) {
			if(time(NULL)-start>timeout) {
				free(buf);
				dbprintf(FALSE, p, "recvline timeout (received: %d)",i);
				*rval = JSVAL_NULL;
				return(JS_TRUE);	/* time-out */
			}
			mswait(1);
			continue;	/* no data */
		}

		if(recv(p->sock, &ch, 1, 0)!=1) {
			p->last_error=ERROR_VALUE;
			break;
		}

		if(ch=='\n' && i>=1) 
			break;

		buf[i++]=ch;
	}
	if(i>0 && buf[i-1]=='\r')
		buf[i-1]=0;
	else
		buf[i]=0;

	str = JS_NewStringCopyZ(cx, buf);
	free(buf);
	*rval = STRING_TO_JSVAL(str);

	dbprintf(FALSE, p, "received %u bytes (recvline) lasterror=%d"
		,strlen(buf),ERROR_VALUE);
		
	return(JS_TRUE);
}

static JSBool
js_getsockopt(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int			opt;
	socklen_t	len;
	int			val;
	private_t*	p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	opt = sockopt(JS_GetStringBytes(JS_ValueToString(cx,argv[0])));
	len = sizeof(val);

	if(getsockopt(p->sock,SOL_SOCKET,opt,(void*)&val,&len)==0) {
		dbprintf(FALSE, p, "option %d = %d",opt,val);
		*rval = INT_TO_JSVAL(val);
	} else {
		p->last_error=ERROR_VALUE;
		dbprintf(TRUE, p, "error %d getting option %d"
			,ERROR_VALUE,opt);
		*rval = INT_TO_JSVAL(-1);
	}

	return(JS_TRUE);
}


static JSBool
js_setsockopt(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int			opt;
	int32		val=1;
	private_t*	p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	opt = sockopt(JS_GetStringBytes(JS_ValueToString(cx,argv[0])));
	JS_ValueToInt32(cx,argv[1],&val);

	*rval = BOOLEAN_TO_JSVAL(setsockopt(p->sock,SOL_SOCKET,opt,(char*)&val,sizeof(val))==0);
	p->last_error=ERROR_VALUE;

	return(JS_TRUE);
}

static JSBool
js_ioctlsocket(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	long		cmd;
	ulong		arg=0;
	private_t*	p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	cmd = JSVAL_TO_INT(argv[0]);
	if(argc>1)
		arg = JSVAL_TO_INT(argv[1]);

	if(ioctlsocket(p->sock,cmd,&arg)==0)
		*rval = INT_TO_JSVAL(arg);
	else
		*rval = INT_TO_JSVAL(-1);

	p->last_error=ERROR_VALUE;

	return(JS_TRUE);
}

static JSBool
js_poll(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	private_t*	p;
	BOOL	poll_for_write=FALSE;
	fd_set	socket_set;
	fd_set*	rd_set=NULL;
	fd_set*	wr_set=NULL;
	uintN	argn;
	int		result;
	struct	timeval tv = {0, 0};

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	if(p->sock==INVALID_SOCKET) {
		dbprintf(TRUE, p, "INVALID SOCKET in call to poll");
		*rval = INT_TO_JSVAL(-1);
		return(JS_TRUE);
	}

	for(argn=0;argn<argc;argn++) {
		if(JSVAL_IS_BOOLEAN(argv[argn]))
			poll_for_write=JSVAL_TO_BOOLEAN(argv[argn]);
		else if(JSVAL_IS_INT(argv[argn]))
			tv.tv_sec = JSVAL_TO_INT(argv[0]);
	}

	FD_ZERO(&socket_set);
	FD_SET(p->sock,&socket_set);
	if(poll_for_write)
		wr_set=&socket_set;
	else
		rd_set=&socket_set;

	result = select(p->sock+1,rd_set,wr_set,NULL,&tv);

	p->last_error=ERROR_VALUE;

	dbprintf(FALSE, p, "poll: select returned %d (errno %d)"
		,result,p->last_error);

	*rval = INT_TO_JSVAL(result);

	return(JS_TRUE);
}


/* Socket Object Properites */
enum {
	 SOCK_PROP_LAST_ERROR
	,SOCK_PROP_IS_CONNECTED
	,SOCK_PROP_DATA_WAITING
	,SOCK_PROP_NREAD
	,SOCK_PROP_DEBUG
	,SOCK_PROP_DESCRIPTOR
	,SOCK_PROP_NONBLOCKING
	,SOCK_PROP_LOCAL_IP
	,SOCK_PROP_LOCAL_PORT
	,SOCK_PROP_REMOTE_IP
	,SOCK_PROP_REMOTE_PORT
	,SOCK_PROP_TYPE

	/* MUST be last */
	,SOCK_PROPERTIES
};

#ifdef _DEBUG
static char* socket_prop_desc[SOCK_PROPERTIES+1] = {
	 "last occurred error value"
	,"true if socket is in a connected state"
	,"true if data is waiting to be read"
	,"number of bytes waiting to be read"
	,"enable debug logging"
	,"socket descriptor"
	,"non-blocking operation"
	,"local IP address"
	,"local TCP/UDP port"
	,"remote IP address"
	,"remote TCP/UDP port"
	,"socket type (TCP or UDP)"
	,NULL
};
#endif

static JSBool js_socket_set(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint       tiny;
	private_t*	p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

    tiny = JSVAL_TO_INT(id);

	dbprintf(FALSE, p, "setting property %d",tiny);

	switch(tiny) {
		case SOCK_PROP_DEBUG:
			p->debug = JSVAL_TO_BOOLEAN(*vp);
			break;
		case SOCK_PROP_DESCRIPTOR:
			p->sock = JSVAL_TO_INT(*vp);
			break;
		case SOCK_PROP_LAST_ERROR:
			p->last_error = JSVAL_TO_INT(*vp);
			break;
		case SOCK_PROP_NONBLOCKING:
			p->nonblocking = JSVAL_TO_BOOLEAN(*vp);
			ioctlsocket(p->sock,FIONBIO,(ulong*)&(p->nonblocking));
			break;
	}

	return(JS_TRUE);
}

static JSBool js_socket_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint       tiny;
	ulong		cnt;
	BOOL		rd;
	private_t*	p;
	socklen_t	addr_len;
	SOCKADDR_IN	addr;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

    tiny = JSVAL_TO_INT(id);

#if 0 /* just too much */
	dbprintf(FALSE, p, "getting property %d",tiny);
#endif

	switch(tiny) {
		case SOCK_PROP_LAST_ERROR:
			*vp = INT_TO_JSVAL(p->last_error);
			break;
		case SOCK_PROP_IS_CONNECTED:
			if(!p->is_connected && !p->external)
				*vp = JSVAL_FALSE;
			else
				*vp = BOOLEAN_TO_JSVAL(socket_check(p->sock,NULL));
			break;
		case SOCK_PROP_DATA_WAITING:
			socket_check(p->sock,&rd);
			*vp = BOOLEAN_TO_JSVAL(rd);
			break;
		case SOCK_PROP_NREAD:
			cnt=0;
			if(ioctlsocket(p->sock, FIONREAD, &cnt)==0) 
				*vp = INT_TO_JSVAL(cnt);
			else
				*vp = INT_TO_JSVAL(0);
			break;
		case SOCK_PROP_DEBUG:
			*vp = INT_TO_JSVAL(p->debug);
			break;
		case SOCK_PROP_DESCRIPTOR:
			*vp = INT_TO_JSVAL(p->sock);
			break;
		case SOCK_PROP_NONBLOCKING:
			*vp = BOOLEAN_TO_JSVAL(p->nonblocking);
			break;
		case SOCK_PROP_LOCAL_IP:
			addr_len = sizeof(addr);
			if(getsockname(p->sock, (struct sockaddr *)&addr,&addr_len)!=0) {
				p->last_error=ERROR_VALUE;
				*vp = JSVAL_VOID;
			} else
				*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,inet_ntoa(addr.sin_addr)));
			break;
		case SOCK_PROP_LOCAL_PORT:
			addr_len = sizeof(addr);
			if(getsockname(p->sock, (struct sockaddr *)&addr,&addr_len)!=0) {
				p->last_error=ERROR_VALUE;
				*vp = JSVAL_VOID;
			} else
				*vp = INT_TO_JSVAL(ntohs(addr.sin_port));
			break;
		case SOCK_PROP_REMOTE_IP:
			addr_len = sizeof(addr);
			if(getpeername(p->sock, (struct sockaddr *)&addr,&addr_len)!=0) {
				p->last_error=ERROR_VALUE;
				*vp = JSVAL_VOID;
			} else
				*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,inet_ntoa(addr.sin_addr)));
			break;
		case SOCK_PROP_REMOTE_PORT:
			addr_len = sizeof(addr);
			if(getpeername(p->sock, (struct sockaddr *)&addr,&addr_len)!=0) {
				p->last_error=ERROR_VALUE;
				*vp = JSVAL_VOID;
			} else
				*vp = INT_TO_JSVAL(ntohs(addr.sin_port));
			break;
		case SOCK_PROP_TYPE:
			*vp = INT_TO_JSVAL(p->type);
			break;
	}

	return(TRUE);
}

#define SOCK_PROP_FLAGS JSPROP_ENUMERATE|JSPROP_READONLY

static struct JSPropertySpec js_socket_properties[] = {
/*		 name				,tinyid					,flags,				getter,	setter	*/

	{	"last_error"		,SOCK_PROP_LAST_ERROR	,SOCK_PROP_FLAGS,	NULL,NULL},
	{	"is_connected"		,SOCK_PROP_IS_CONNECTED	,SOCK_PROP_FLAGS,	NULL,NULL},
	{	"data_waiting"		,SOCK_PROP_DATA_WAITING	,SOCK_PROP_FLAGS,	NULL,NULL},
	{	"nread"				,SOCK_PROP_NREAD		,SOCK_PROP_FLAGS,	NULL,NULL},
	{	"debug"				,SOCK_PROP_DEBUG		,JSPROP_ENUMERATE,	NULL,NULL},
	{	"descriptor"		,SOCK_PROP_DESCRIPTOR	,JSPROP_ENUMERATE,	NULL,NULL},
	{	"nonblocking"		,SOCK_PROP_NONBLOCKING	,JSPROP_ENUMERATE,	NULL,NULL},
	{	"local_ip_address"	,SOCK_PROP_LOCAL_IP		,SOCK_PROP_FLAGS,	NULL,NULL},
	{	"local_port"		,SOCK_PROP_LOCAL_PORT	,SOCK_PROP_FLAGS,	NULL,NULL},
	{	"remote_ip_address"	,SOCK_PROP_REMOTE_IP	,SOCK_PROP_FLAGS,	NULL,NULL},
	{	"remote_port"		,SOCK_PROP_REMOTE_PORT	,SOCK_PROP_FLAGS,	NULL,NULL},
	{	"type"				,SOCK_PROP_TYPE			,SOCK_PROP_FLAGS,	NULL,NULL},
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

static jsMethodSpec js_socket_functions[] = {
	{"close",		js_close,		0,	JSTYPE_VOID,	""
	,JSDOCSTR("close socket")
	},
	{"bind",		js_bind,		0,	JSTYPE_BOOLEAN,	JSDOCSTR("[port]")
	,JSDOCSTR("bind socket to a port")
	},
	{"connect",     js_connect,     2,	JSTYPE_BOOLEAN,	JSDOCSTR("string host, port")
	,JSDOCSTR("connect to a specific port at the specified IP address or hostname")
	},
	{"listen",		js_listen,		0,	JSTYPE_BOOLEAN,	""					
	,JSDOCSTR("put socket in listening state (use before an accept)")
	},
	{"accept",		js_accept,		0,	JSTYPE_OBJECT,	""					
	,JSDOCSTR("accept an incoming connection, returns a new Socket object")
	},
	{"write",		js_send,		1,	JSTYPE_ALIAS },
	{"send",		js_send,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("string data")
	,JSDOCSTR("send a string (AKA write)")
	},
	{"sendto",		js_sendto,		3,	JSTYPE_BOOLEAN,	JSDOCSTR("string data, address, port")
	,JSDOCSTR("send a string to a specific address and port (typically used for UDP sockets)")
	},
	{"sendfile",	js_sendfile,	1,	JSTYPE_BOOLEAN,	JSDOCSTR("string filename")
	,JSDOCSTR("send a file")
	},
	{"read",		js_recv,		1,	JSTYPE_ALIAS },
	{"recv",		js_recv,		0,	JSTYPE_STRING,	JSDOCSTR("[maxlen]")
	,JSDOCSTR("receive a string, default maxlen is 512 characters (AKA read)")
	},
	{"peek",		js_peek,		0,	JSTYPE_STRING,	JSDOCSTR("[maxlen]")
	,JSDOCSTR("receive a string, default maxlen is 512 characters, leave string in receive buffer")
	},
	{"readline",	js_recvline,	0,	JSTYPE_ALIAS },
	{"readln",		js_recvline,	0,	JSTYPE_ALIAS },
	{"recvline",	js_recvline,	0,	JSTYPE_STRING,	JSDOCSTR("[maxlen] [,timeout]")
	,JSDOCSTR("receive a line-feed terminated string, default maxlen is 512 characters, default timeout is 30 seconds (AKA readline and readln)")
	},
	{"recvfrom",	js_recvfrom,	0,	JSTYPE_OBJECT,	JSDOCSTR("[maxlen]")
	,JSDOCSTR("receive a string from (typically UDP) socket, return address and port of sender")
	},
	{"getoption",	js_getsockopt,	1,	JSTYPE_NUMBER,	JSDOCSTR("number option")
	,JSDOCSTR("get socket option value")
	},
	{"setoption",	js_setsockopt,	2,	JSTYPE_BOOLEAN,	JSDOCSTR("number option, value")
	,JSDOCSTR("set socket option value")
	},
	{"ioctl",		js_ioctlsocket,	1,	JSTYPE_NUMBER,	JSDOCSTR("number cmd [,arg]")
	,JSDOCSTR("send socket IOCTL")					
	},
	{"poll",		js_poll,		1,	JSTYPE_NUMBER,	JSDOCSTR("[number timeout] [,bool write]")
	,JSDOCSTR("poll socket for read or write ability (defaults to read), default timeout value is 0 seconds (immediate timeout)")
	},
	{0}
};

JSObject* DLLCALL js_CreateSocketClass(JSContext* cx, JSObject* parent)
{
	JSObject*	sockobj;
	JSFunctionSpec	funcs[sizeof(js_socket_functions)/sizeof(jsMethodSpec)];

	memset(funcs,0,sizeof(funcs));
	js_MethodsToFunctions(js_socket_functions,funcs);

	sockobj = JS_InitClass(cx, parent, NULL
		,&js_socket_class
		,js_socket_constructor
		,0	/* number of constructor args */
		,js_socket_properties
		,funcs
		,NULL,NULL);

	return(sockobj);
}

JSObject* DLLCALL js_CreateSocketObject(JSContext* cx, JSObject* parent, char *name, SOCKET sock)
{
	JSObject* obj;
	private_t*	p;

	obj = JS_DefineObject(cx, parent, name, &js_socket_class, NULL, JSPROP_ENUMERATE);

	if(obj==NULL)
		return(NULL);

	if(!JS_DefineProperties(cx, obj, js_socket_properties))
		return(NULL);

	if (!js_DefineMethods(cx, obj, js_socket_functions)) 
		return(NULL);

	if((p=(private_t*)malloc(sizeof(private_t)))==NULL)
		return(NULL);
	memset(p,0,sizeof(private_t));

	p->sock = sock;
	p->external = TRUE;

	if(!JS_SetPrivate(cx, obj, p)) {
		dbprintf(TRUE, p, "JS_SetPrivate failed");
		return(NULL);
	}

#ifdef _DEBUG
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", socket_prop_desc, JSPROP_READONLY);
#endif

	dbprintf(FALSE, p, "object created");

	return(obj);
}

#endif	/* JAVSCRIPT */
