/* js_socket.c */

/* Synchronet JavaScript "Socket" Object */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2013 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#include <cryptlib.h>

#include "sbbs.h"
#include "js_request.h"

#ifdef JAVASCRIPT

int cryptInitialized=0;

typedef struct
{
	SOCKET	sock;
	BOOL	external;	/* externally created, don't close */
	BOOL	debug;
	BOOL	nonblocking;
	BOOL	is_connected;
	BOOL	network_byte_order;
	int		last_error;
	int		type;
	SOCKADDR_IN	remote_addr;
	CRYPT_SESSION	session;
	char	*hostname;
} private_t;

static const char* getprivate_failure = "line %d %s JS_GetPrivate failed";

static void do_cryptEnd(void)
{
	cryptEnd();
}

static int do_cryptAttribute(const CRYPT_CONTEXT session, CRYPT_ATTRIBUTE_TYPE attr, int val)
{
	int ret=cryptSetAttribute(session, attr, val);
	if(ret != CRYPT_OK)
		lprintf(LOG_ERR, "cryptSetAttribute(%d) returned %d", attr, ret);
	return ret;
}

int do_cryptInit(void)
{
	int ret;

	if(!cryptInitialized) {
		if((ret=cryptInit())==CRYPT_OK) {
			cryptAddRandom(NULL,CRYPT_RANDOM_SLOWPOLL);
			cryptInitialized=1;
			atexit(do_cryptEnd);
		}
		else
			lprintf(LOG_ERR,"cryptInit() returned %d", ret);
	}
	return cryptInitialized;
}

static int do_cryptAttributeString(const CRYPT_CONTEXT session, CRYPT_ATTRIBUTE_TYPE attr, void *val, int len)
{
	int ret=cryptSetAttributeString(session, attr, val, len);
	if(ret != CRYPT_OK)
		lprintf(LOG_ERR, "cryptSetAttributeString(%d) returned %d", attr, ret);
	return ret;
}

static void do_CryptFlush(const CRYPT_CONTEXT session)
{
	int ret=cryptFlushData(session);

	if(ret!=CRYPT_OK)
		lprintf(LOG_ERR, "cryptFlushData() returned %d", ret);
}

static ptrdiff_t js_socket_recv(private_t *p, void *buf, size_t len, int flags, int timeout)
{
	ptrdiff_t	total=0;
	int	copied,ret;
	
	if(p->session==-1)
		return(recv(p->sock, buf, len, flags));
	if(p->nonblocking) {
		do_cryptAttribute(p->session, CRYPT_OPTION_NET_READTIMEOUT, 0);
	}
	else {
		do_cryptAttribute(p->session, CRYPT_OPTION_NET_READTIMEOUT, timeout);
	}
	do {
		if((ret=cryptPopData(p->session, buf, len, &copied))==CRYPT_OK) {
			if(p->nonblocking)
				return copied;
			total += copied;
			if(total>=(ptrdiff_t)len)
				return total;
			len-=copied;
			buf=((uint8_t *)buf) + copied;
		}
		else {
			lprintf(LOG_ERR,"cryptPopData() returned %d", ret);
			return total;
		}
	} while(len);
	return total;	// Shouldn't happen...
}

static ptrdiff_t js_socket_sendsocket(private_t *p, const void *msg, size_t len, int flush)
{
	ptrdiff_t total=0;
	int copied=0,ret;
	
	if(p->session==-1)
		return sendsocket(p->sock, msg, len);
	do {
		if((ret=cryptPushData(p->session, msg, len, &copied))==CRYPT_OK) {
			if(p->nonblocking) {
				if(flush) do_CryptFlush(p->session);
				return copied;
			}
			total += copied;
			if(total >= (ptrdiff_t)len) {
				if(flush) do_CryptFlush(p->session);
				return total;
			}
			len -= copied;
			msg=((uint8_t *)msg) + copied;
		}
		else {
			lprintf(LOG_ERR,"cryptPushData() returned %d", ret);
			if(flush) do_CryptFlush(p->session);
			return total;
		}
	} while(len);
	if(flush) do_CryptFlush(p->session);
	return total; // shouldn't happen...
}

static int js_socket_sendfilesocket(private_t *p, int file, off_t *offset, off_t count)
{
	char		buf[1024*16];
	off_t		len;
	int			rd;
	int			wr=0;
	int			total=0;
	int			i;

	if(p->session==-1)
		return sendfilesocket(p->sock, file, offset, count);

	len=filelength(file);

	if(offset!=NULL)
		if(lseek(file,*offset,SEEK_SET)<0)
			return(-1);

	if(count<1 || count>len) {
		count=len;
		count-=tell(file);		/* don't try to read beyond EOF */
	}

	if(count<0) {
		errno=EINVAL;
		return(-1);
	}

	while(total<count) {
		rd=read(file,buf,sizeof(buf));
		if(rd==-1) {
			do_CryptFlush(p->session);
			return(-1);
		}
		if(rd==0)
			break;
		for(i=wr=0;i<rd;i+=wr) {
			wr=js_socket_sendsocket(p,buf+i,rd-i,FALSE);
			if(wr>0)
				continue;
			if(wr==SOCKET_ERROR && ERROR_VALUE==EWOULDBLOCK) {
				wr=0;
				SLEEP(1);
				continue;
			}
			do_CryptFlush(p->session);
			return(wr);
		}
		if(i!=rd) {
			do_CryptFlush(p->session);
			return(-1);
		}
		total+=rd;
	}

	if(offset!=NULL)
		(*offset)+=total;

	do_CryptFlush(p->session);
	return(total);
}

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
	
	lprintf(LOG_DEBUG,"%04d Socket %s%s",p->sock,error ? "ERROR: ":"",sbuf);
}

/* Socket Destructor */

static void js_finalize_socket(JSContext *cx, JSObject *obj)
{
	private_t* p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return;

	if(p->session != -1) {
		cryptDestroySession(p->session);
		p->session=-1;
	}
	if(p->external==FALSE && p->sock!=INVALID_SOCKET) {
		close_socket(p->sock);
		dbprintf(FALSE, p, "closed/deleted");
	}

	if(p->hostname)
		free(p->hostname);
	free(p);

	JS_SetPrivate(cx, obj, NULL);
}


/* Socket Object Methods */

static JSBool
js_close(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	private_t*	p;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(p->session != -1) {
		cryptDestroySession(p->session);
		p->session=-1;
	}
	if(p->sock==INVALID_SOCKET)
		return(JS_TRUE);

	rc=JS_SUSPENDREQUEST(cx);
	close_socket(p->sock);

	p->last_error = ERROR_VALUE;

	dbprintf(FALSE, p, "closed");

	p->sock = INVALID_SOCKET; 
	p->is_connected = FALSE;
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static ushort js_port(JSContext* cx, jsval val, int type)
{
	char*			cp;
	JSString*		str;
	int32			i=0;
	struct servent*	serv;
	jsrefcount		rc;

	if(JSVAL_IS_NUMBER(val)) {
		JS_ValueToInt32(cx,val,&i);
		return((ushort)i);
	}
	if(JSVAL_IS_STRING(val)) {
		str = JS_ValueToString(cx,val);
		JSSTRING_TO_ASTRING(cx, str, cp, 16, NULL);
		if(isdigit(*cp))
			return((ushort)strtol(cp,NULL,0));
		rc=JS_SUSPENDREQUEST(cx);
		serv = getservbyname(cp,type==SOCK_STREAM ? "tcp":"udp");
		JS_RESUMEREQUEST(cx, rc);
		if(serv!=NULL)
			return(htons(serv->s_port));
	}
	return(0);
}

SOCKET DLLCALL js_socket(JSContext *cx, jsval val)
{
	void*		vp;
	JSClass*	cl;
	SOCKET		sock=INVALID_SOCKET;

	if(JSVAL_IS_OBJECT(val) && (cl=JS_GetClass(cx,JSVAL_TO_OBJECT(val)))!=NULL) {
		if(cl->flags&JSCLASS_HAS_PRIVATE)
			if((vp=JS_GetPrivate(cx,JSVAL_TO_OBJECT(val)))!=NULL)
				sock=*(SOCKET*)vp;
	} else if(val!=JSVAL_VOID) {
		int32	i;
		if(JS_ValueToInt32(cx,val,&i))
			sock = i;
	}

	return(sock);
}

void DLLCALL js_timeval(JSContext* cx, jsval val, struct timeval* tv)
{
	jsdouble jsd;

	if(JSVAL_IS_INT(val))
		tv->tv_sec = JSVAL_TO_INT(val);
	else if(JSVAL_IS_DOUBLE(val)) {
		JS_ValueToNumber(cx,val,&jsd);
		tv->tv_sec = (int)jsd;
		tv->tv_usec = (int)(jsd*1000000.0)%1000000;
	}
}

static JSBool
js_bind(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	ulong		ip=0;
	private_t*	p;
	ushort		port=0;
	SOCKADDR_IN	addr;
	jsrefcount	rc;
	char		*cstr;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}
	
	memset(&addr,0,sizeof(addr));
	addr.sin_family = AF_INET;

	if(argc)
		port = js_port(cx,argv[0],p->type);
	addr.sin_port = htons(port);
	if(argc > 1)
		JSVALUE_TO_ASTRING(cx, argv[1], cstr, 16, NULL);
	if(argc>1 && cstr != NULL
		&& (ip=inet_addr(cstr))!=INADDR_NONE)
		addr.sin_addr.s_addr = ip;

	rc=JS_SUSPENDREQUEST(cx);
	if(bind(p->sock, (struct sockaddr *) &addr, sizeof(addr))!=0) {
		p->last_error=ERROR_VALUE;
		dbprintf(TRUE, p, "bind failed with error %d",ERROR_VALUE);
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		JS_RESUMEREQUEST(cx, rc);
		return(JS_TRUE);
	}

	dbprintf(FALSE, p, "bound to port %u",port);
	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_listen(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	private_t*	p;
	int32		backlog=1;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}
	
	if(argc && argv[0]!=JSVAL_VOID)
		backlog = JS_ValueToInt32(cx,argv[0],&backlog);

	rc=JS_SUSPENDREQUEST(cx);
	if(listen(p->sock, backlog)!=0) {
		p->last_error=ERROR_VALUE;
		dbprintf(TRUE, p, "listen failed with error %d",ERROR_VALUE);
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		JS_RESUMEREQUEST(cx, rc);
		return(JS_TRUE);
	}

	dbprintf(FALSE, p, "listening, backlog=%d",backlog);
	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_accept(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	private_t*	p;
	private_t*	new_p;
	JSObject*	sockobj;
	SOCKET		new_socket;
	socklen_t	addrlen;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	addrlen=sizeof(p->remote_addr);

	rc=JS_SUSPENDREQUEST(cx);
	if((new_socket=accept_socket(p->sock,(struct sockaddr *)&(p->remote_addr),&addrlen))==INVALID_SOCKET) {
		p->last_error=ERROR_VALUE;
		dbprintf(TRUE, p, "accept failed with error %d",ERROR_VALUE);
		JS_RESUMEREQUEST(cx, rc);
		return(JS_TRUE);
	}

	if((sockobj=js_CreateSocketObject(cx, obj, "new_socket", new_socket))==NULL) {
		closesocket(new_socket);
		JS_RESUMEREQUEST(cx, rc);
		JS_ReportError(cx,"Error creating new socket object");
		return(JS_TRUE);
	}
	if((new_p=(private_t*)JS_GetPrivate(cx,sockobj))==NULL) {
		JS_RESUMEREQUEST(cx, rc);
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	new_p->type=p->type;
	new_p->debug=p->debug;
	new_p->nonblocking=p->nonblocking;
	new_p->external=FALSE;		/* let destructor close socket */
	new_p->is_connected=TRUE;

	dbprintf(FALSE, p, "accepted connection");
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(sockobj));
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_connect(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	int			result;
	ulong		val;
	ulong		ip_addr;
	ushort		port;
	JSString*	str;
	private_t*	p;
	fd_set		socket_set;
	struct		timeval tv = {0, 0};
	jsrefcount	rc;
	
	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	str = JS_ValueToString(cx, argv[0]);
	if(p->hostname)
		free(p->hostname);
	JSSTRING_TO_MSTRING(cx, str, p->hostname, NULL);
	rc=JS_SUSPENDREQUEST(cx);
	dbprintf(FALSE, p, "resolving hostname: %s", p->hostname);
	if((ip_addr=resolve_ip(p->hostname))==INADDR_NONE) {
		p->last_error=ERROR_VALUE;
		dbprintf(TRUE, p, "resolve_ip failed with error %d",ERROR_VALUE);
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		JS_RESUMEREQUEST(cx, rc);
		return(JS_TRUE);
	}

	JS_RESUMEREQUEST(cx, rc);
	port = js_port(cx,argv[1],p->type);

	tv.tv_sec = 10;	/* default time-out */

	if(argc>2)	/* time-out value specified */
		js_timeval(cx,argv[2],&tv);

	rc=JS_SUSPENDREQUEST(cx);
	dbprintf(FALSE, p, "connecting to port %u at %s", port, p->hostname);

	memset(&(p->remote_addr),0,sizeof(p->remote_addr));
	p->remote_addr.sin_addr.s_addr = ip_addr;
	p->remote_addr.sin_family = AF_INET;
	p->remote_addr.sin_port   = htons(port);

	/* always set to nonblocking here */
	val=1;
	ioctlsocket(p->sock,FIONBIO,&val);	

	result=connect(p->sock, (struct sockaddr *)&(p->remote_addr), sizeof(p->remote_addr));

	if(result==SOCKET_ERROR
		&& (ERROR_VALUE==EWOULDBLOCK || ERROR_VALUE==EINPROGRESS)) {
		FD_ZERO(&socket_set);
		FD_SET(p->sock,&socket_set);
		if(select(p->sock+1,NULL,&socket_set,NULL,&tv)==1)
			result=0;	/* success */
	}

	/* Restore original setting here */
	ioctlsocket(p->sock,FIONBIO,(ulong*)&(p->nonblocking));

	if(result!=0) {
		p->last_error=ERROR_VALUE;
		dbprintf(TRUE, p, "connect failed with error %d",ERROR_VALUE);
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		JS_RESUMEREQUEST(cx, rc);
		return(JS_TRUE);
	}

	p->is_connected = TRUE;
	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	dbprintf(FALSE, p, "connected to port %u at %s", port, p->hostname);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_send(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char*		cp;
	size_t		len;
	JSString*	str;
	private_t*	p;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	str = JS_ValueToString(cx, argv[0]);
	JSSTRING_TO_MSTRING(cx, str, cp, &len);
	HANDLE_PENDING(cx);
	if(cp==NULL)
		return JS_TRUE;

	rc=JS_SUSPENDREQUEST(cx);
	if(js_socket_sendsocket(p,cp,len,TRUE)==len) {
		dbprintf(FALSE, p, "sent %u bytes",len);
		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	} else {
		p->last_error=ERROR_VALUE;
		dbprintf(TRUE, p, "send of %u bytes failed",len);
	}
	free(cp);
	JS_RESUMEREQUEST(cx, rc);
		
	return(JS_TRUE);
}

static JSBool
js_sendto(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char*		cp;
	size_t		len;
	ulong		ip_addr;
	ushort		port;
	JSString*	data_str;
	JSString*	ip_str;
	private_t*	p;
	SOCKADDR_IN	addr;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	/* data */
	data_str = JS_ValueToString(cx, argv[0]);
	JSSTRING_TO_MSTRING(cx, data_str, cp, &len);
	HANDLE_PENDING(cx);
	if(cp==NULL)
		return JS_TRUE;

	/* address */
	ip_str = JS_ValueToString(cx, argv[1]);
	if(p->hostname)
		free(p->hostname);
	JSSTRING_TO_MSTRING(cx, ip_str, p->hostname, NULL);
	if(JS_IsExceptionPending(cx)) {
		free(cp);
		return JS_FALSE;
	}
	if(p->hostname==NULL) {
		free(cp);
		return JS_TRUE;
	}
	rc=JS_SUSPENDREQUEST(cx);
	dbprintf(FALSE, p, "resolving hostname: %s", p->hostname);
	if((ip_addr=resolve_ip(p->hostname))==INADDR_NONE) {
		p->last_error=ERROR_VALUE;
		dbprintf(TRUE, p, "resolve_ip failed with error %d",ERROR_VALUE);
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		free(cp);
		JS_RESUMEREQUEST(cx, rc);
		return(JS_TRUE);
	}

	/* port */
	JS_RESUMEREQUEST(cx, rc);
	port = js_port(cx,argv[2],p->type);
	rc=JS_SUSPENDREQUEST(cx);

	dbprintf(FALSE, p, "sending %d bytes to port %u at %s"
		,len, port, p->hostname);

	memset(&addr,0,sizeof(addr));
	addr.sin_addr.s_addr = ip_addr;
	addr.sin_family = AF_INET;
	addr.sin_port   = htons(port);

	if(sendto(p->sock,cp,len,0 /* flags */,(SOCKADDR*)&addr,sizeof(addr))==len) {
		dbprintf(FALSE, p, "sent %u bytes",len);
		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	} else {
		p->last_error=ERROR_VALUE;
		dbprintf(TRUE, p, "send of %u bytes failed",len);
	}
	free(cp);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}


static JSBool
js_sendfile(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char*		fname;
	long		len;
	int			file;
	private_t*	p;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	JSVALUE_TO_MSTRING(cx, argv[0], fname, NULL);
	HANDLE_PENDING(cx);
	if(fname==NULL) {
		JS_ReportError(cx,"Failure reading filename");
		return(JS_FALSE);
	}

	rc=JS_SUSPENDREQUEST(cx);
	if((file=nopen(fname,O_RDONLY|O_BINARY))==-1) {
		free(fname);
		JS_RESUMEREQUEST(cx, rc);
		return(JS_TRUE);
	}
	free(fname);

	len = js_socket_sendfilesocket(p, file, NULL, 0);
	close(file);
	if(len > 0) {
		dbprintf(FALSE, p, "sent %u bytes",len);
		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	} else {
		p->last_error=ERROR_VALUE;
		dbprintf(TRUE, p, "send of %s failed",fname);
	}

	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_sendbin(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	BYTE		b;
	WORD		w;
	DWORD		l;
	int32		val=0;
	size_t		wr=0;
	int32		size=sizeof(DWORD);
	private_t*	p;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(argc && argv[0]!=JSVAL_VOID)
		JS_ValueToInt32(cx,argv[0],&val);
	if(argc>1 && argv[1]!=JSVAL_VOID) 
		JS_ValueToInt32(cx,argv[1],&size);

	rc=JS_SUSPENDREQUEST(cx);
	switch(size) {
		case sizeof(BYTE):
			b = (BYTE)val;
			wr=js_socket_sendsocket(p,&b,size,TRUE);
			break;
		case sizeof(WORD):
			w = (WORD)val;
			if(p->network_byte_order)
				w=htons(w);
			wr=js_socket_sendsocket(p,(BYTE*)&w,size,TRUE);
			break;
		case sizeof(DWORD):
			l = val;
			if(p->network_byte_order)
				l=htonl(l);
			wr=js_socket_sendsocket(p,(BYTE*)&l,size,TRUE);
			break;
		default:	
			/* unknown size */
			dbprintf(TRUE, p, "unsupported binary write size: %d",size);
			break;
	}
	if(wr==size) {
		dbprintf(FALSE, p, "sent %u bytes (binary)",size);
		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	} else {
		p->last_error=ERROR_VALUE;
		dbprintf(TRUE, p, "send of %u bytes (binary) failed",size);
	}
		
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}


static JSBool
js_recv(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char*		buf;
	int32		len=512;
	JSString*	str;
	jsrefcount	rc;
	private_t*	p;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(argc && argv[0]!=JSVAL_VOID)
		JS_ValueToInt32(cx,argv[0],&len);

	if((buf=(char*)malloc(len+1))==NULL) {
		JS_ReportError(cx,"Error allocating %u bytes",len+1);
		return(JS_FALSE);
	}

	rc=JS_SUSPENDREQUEST(cx);
	len = js_socket_recv(p,buf,len,0,120);
	JS_RESUMEREQUEST(cx, rc);
	if(len<0) {
		p->last_error=ERROR_VALUE;
		JS_SET_RVAL(cx, arglist, JSVAL_NULL);
		free(buf);
		return(JS_TRUE);
	}
	buf[len]=0;

	str = JS_NewStringCopyN(cx, buf, len);
	if(str==NULL) {
		free(buf);
		return(JS_FALSE);
	}

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(str));
	rc=JS_SUSPENDREQUEST(cx);
	dbprintf(FALSE, p, "received %u bytes",len);
	JS_RESUMEREQUEST(cx, rc);
	
	free(buf);
	return(JS_TRUE);
}

static JSBool
js_recvfrom(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char*		buf;
	char		ip_addr[64];
	char		port[32];
	int			rd=0;
	int32		len=512;
	uintN		n;
	BOOL		binary=FALSE;
	BYTE		b;
	WORD		w;
	DWORD		l;
	jsval		data_val=JSVAL_NULL;
	JSString*	str;
	JSObject*	retobj;
	SOCKADDR_IN	addr;
	socklen_t	addrlen;
	jsrefcount	rc;
	private_t*	p;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	for(n=0;n<argc;n++) {
		if(JSVAL_IS_BOOLEAN(argv[n])) {
			binary=JSVAL_TO_BOOLEAN(argv[n]);
			if(binary)
				len=sizeof(DWORD);
		} else if(argv[n]!=JSVAL_VOID)
			JS_ValueToInt32(cx,argv[n],&len);
	}

	addrlen=sizeof(addr);

	if(binary) {	/* Binary/Integer Data */

		rc=JS_SUSPENDREQUEST(cx);
		switch(len) {
			case sizeof(BYTE):
				if((rd=recvfrom(p->sock,&b,len,0,(SOCKADDR*)&addr,&addrlen))==len)
					data_val = INT_TO_JSVAL(b);
				break;
			case sizeof(WORD):
				if((rd=recvfrom(p->sock,(BYTE*)&w,len,0,(SOCKADDR*)&addr,&addrlen))==len) {
					if(p->network_byte_order)
						w=ntohs(w);
					data_val = INT_TO_JSVAL(w);
				}
				break;
			default:
			case sizeof(DWORD):
				if((rd=recvfrom(p->sock,(BYTE*)&l,len,0,(SOCKADDR*)&addr,&addrlen))==len) {
					if(p->network_byte_order)
						l=ntohl(l);
					data_val=UINT_TO_JSVAL(l);
				}
				break;
		}

		JS_RESUMEREQUEST(cx, rc);

		if(rd!=len) {
			p->last_error=ERROR_VALUE;
			return(JS_TRUE);
		}

	} else {		/* String Data */

		if((buf=(char*)malloc(len+1))==NULL) {
			JS_ReportError(cx,"Error allocating %u bytes",len+1);
			return(JS_FALSE);
		}

		rc=JS_SUSPENDREQUEST(cx);
		len = recvfrom(p->sock,buf,len,0,(SOCKADDR*)&addr,&addrlen);
		JS_RESUMEREQUEST(cx, rc);
		if(len<0) {
			p->last_error=ERROR_VALUE;
			free(buf);
			return(JS_TRUE);
		}
		buf[len]=0;

		str = JS_NewStringCopyN(cx, buf, len);
		free(buf);

		if(str==NULL)
			return(JS_FALSE);

		data_val = STRING_TO_JSVAL(str);
	}


	if((retobj=JS_NewObject(cx,NULL,NULL,obj))==NULL) {
		JS_ReportError(cx,"JS_NewObject failed");
		return(JS_FALSE);
	}

	JS_DefineProperty(cx, retobj, "data"
		,data_val
		,NULL,NULL,JSPROP_ENUMERATE);

	sprintf(port,"%u",ntohs(addr.sin_port));
	if((str=JS_NewStringCopyZ(cx,port))==NULL)
		return(JS_FALSE);
	JS_DefineProperty(cx, retobj, "port"
		,STRING_TO_JSVAL(str)
		,NULL,NULL,JSPROP_ENUMERATE);

	SAFECOPY(ip_addr,inet_ntoa(addr.sin_addr));
	if((str=JS_NewStringCopyZ(cx,ip_addr))==NULL)
		return(JS_FALSE);
	JS_DefineProperty(cx, retobj, "ip_address"
		,STRING_TO_JSVAL(str)
		,NULL,NULL,JSPROP_ENUMERATE);

	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(retobj));

	rc=JS_SUSPENDREQUEST(cx);
	dbprintf(FALSE, p, "received %u bytes from %s:%s",len,ip_addr,port);
	JS_RESUMEREQUEST(cx, rc);
		
	return(JS_TRUE);
}


static JSBool
js_peek(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char*		buf;
	int32		len=512;
	JSString*	str;
	jsrefcount	rc;
	private_t*	p;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(argc && argv[0]!=JSVAL_VOID)
		JS_ValueToInt32(cx,argv[0],&len);

	if((buf=(char*)malloc(len+1))==NULL) {
		JS_ReportError(cx,"Error allocating %u bytes",len+1);
		return(JS_FALSE);
	}
	rc=JS_SUSPENDREQUEST(cx);
	if(p->session==-1)
		len = js_socket_recv(p,buf,len,MSG_PEEK,120);
	else
		len=0;
	JS_RESUMEREQUEST(cx, rc);
	if(len<0) {
		p->last_error=ERROR_VALUE;	
		JS_SET_RVAL(cx, arglist, JSVAL_NULL);
		free(buf);
		return(JS_TRUE);
	}
	buf[len]=0;

	str = JS_NewStringCopyN(cx, buf, len);
	free(buf);
	if(str==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(str));
	rc=JS_SUSPENDREQUEST(cx);
	dbprintf(FALSE, p, "received %u bytes, lasterror=%d"
		,len,ERROR_VALUE);
	JS_RESUMEREQUEST(cx, rc);
		
	return(JS_TRUE);
}

static int
js_sock_read_check(private_t *p, time_t start, int32 timeout, int i)
{
	BOOL		rd;

	if(timeout > 0 && time(NULL)-start>timeout) {
		dbprintf(FALSE, p, "recvline timeout (received: %d)",i);
		return 1;
	}

	if(!socket_check(p->sock,&rd,NULL,1000)) {
		p->last_error=ERROR_VALUE;
		if(i==0) {
			return 1;
		}
		return 2;
	}

	if(!rd) {
		if(time(NULL)-start>timeout) {
			dbprintf(FALSE, p, "recvline timeout (received: %d)",i);
			return 1;
		}
		return 3;
	}
	return 0;
}

static JSBool
js_recvline(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char		ch;
	char*		buf;
	int			i;
	int32		len=512;
	time_t		start;
	int32		timeout=30;	/* seconds */
	JSString*	str;
	private_t*	p;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(argc && argv[0]!=JSVAL_VOID)
		JS_ValueToInt32(cx,argv[0],&len);

	if((buf=(char*)malloc(len+1))==NULL) {
		JS_ReportError(cx,"Error allocating %u bytes",len+1);
		return(JS_FALSE);
	}

	if(argc>1 && argv[1]!=JSVAL_VOID)
		JS_ValueToInt32(cx,argv[1],&timeout);

	start=time(NULL);
	rc=JS_SUSPENDREQUEST(cx);
	for(i=0;i<len;) {

		if(p->session==-1) {
			switch(js_sock_read_check(p,start,timeout,i)) {
				case 1:
					JS_SET_RVAL(cx, arglist, JSVAL_NULL);
					JS_RESUMEREQUEST(cx, rc);
					free(buf);
					return(JS_TRUE);	/* time-out */
				case 2:
					len=0;
					continue;
				case 3:
					continue;
			}
		}

		if(js_socket_recv(p, &ch, 1, 0, timeout)!=1) {
			if(p->session==-1) {
				p->last_error=ERROR_VALUE;
				break;
			}
			else {
				switch(js_sock_read_check(p,start,timeout,i)) {
					case 1:
						JS_SET_RVAL(cx, arglist, JSVAL_NULL);
						JS_RESUMEREQUEST(cx, rc);
						free(buf);
						return(JS_TRUE);	/* time-out */
					case 2:
						len=0;
						continue;
					case 3:
						continue;
				}
			}
		}

		if(ch=='\n' /* && i>=1 */) /* Mar-9-2003: terminate on sole LF */
			break;

		buf[i++]=ch;
	}
	if(i>0 && buf[i-1]=='\r')
		buf[i-1]=0;
	else
		buf[i]=0;

	JS_RESUMEREQUEST(cx, rc);
	str = JS_NewStringCopyZ(cx, buf);
	free(buf);
	if(str==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(str));
	rc=JS_SUSPENDREQUEST(cx);
	dbprintf(FALSE, p, "received %u bytes (recvline) lasterror=%d"
		,i,ERROR_VALUE);
	JS_RESUMEREQUEST(cx, rc);
		
	return(JS_TRUE);
}

static JSBool
js_recvbin(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	BYTE		b;
	WORD		w;
	DWORD		l;
	int32		size=sizeof(DWORD);
	int			rd=0;
	private_t*	p;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(-1));

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(argc && argv[0]!=JSVAL_VOID) 
		JS_ValueToInt32(cx,argv[0],&size);

	rc=JS_SUSPENDREQUEST(cx);
	switch(size) {
		case sizeof(BYTE):
			if((rd=js_socket_recv(p,&b,size,0,120))==size)
				JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(b));
			break;
		case sizeof(WORD):
			if((rd=js_socket_recv(p,(BYTE*)&w,size,0,120))==size) {
				if(p->network_byte_order)
					w=ntohs(w);
				JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(w));
			}
			break;
		case sizeof(DWORD):
			if((rd=js_socket_recv(p,(BYTE*)&l,size,0,120))==size) {
				if(p->network_byte_order)
					l=ntohl(l);
				JS_SET_RVAL(cx, arglist, UINT_TO_JSVAL(l));
			}
			break;
	}

	if(rd!=size)
		p->last_error=ERROR_VALUE;
		
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}


static JSBool
js_getsockopt(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	int			opt;
	int			level;
	int			val;
	private_t*	p;
	LINGER		linger;
	void*		vp=&val;
	socklen_t	len=sizeof(val);
	jsrefcount	rc;
	char		*cstr;

	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(-1));

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	rc=JS_SUSPENDREQUEST(cx);
	JSVALUE_TO_ASTRING(cx, argv[0], cstr, 64, NULL);
	if((opt = getSocketOptionByName(cstr, &level)) == -1) {
		JS_RESUMEREQUEST(cx, rc);
		return(JS_TRUE);
	}

	if(opt == SO_LINGER) {
		vp=&linger;
		len=sizeof(linger);
	}
	if(getsockopt(p->sock, level, opt, vp, &len)==0) {
		if(opt == SO_LINGER) {
			if(linger.l_onoff==TRUE)
				val = linger.l_linger;
			else
				val = 0;
		}
		JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(val));
	} else {
		p->last_error=ERROR_VALUE;
		dbprintf(TRUE, p, "error %d getting option %d"
			,ERROR_VALUE,opt);
	}

	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}


static JSBool
js_setsockopt(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	int			opt;
	int			level;
	int32		val=1;
	private_t*	p;
	LINGER		linger;
	void*		vp=&val;
	socklen_t	len=sizeof(val);
	jsrefcount	rc;
	char		*optname;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	JSVALUE_TO_ASTRING(cx, argv[0], optname, 64, NULL);
	rc=JS_SUSPENDREQUEST(cx);
	opt = getSocketOptionByName(optname,&level);
	if(argv[1]!=JSVAL_VOID) {
		JS_RESUMEREQUEST(cx, rc);
		JS_ValueToInt32(cx,argv[1],&val);
		rc=JS_SUSPENDREQUEST(cx);
	}

	if(opt == SO_LINGER) {
		if(val) {
			linger.l_onoff = TRUE;
			linger.l_linger = (ushort)val;
		} else {
			ZERO_VAR(linger);
		}
		vp=&linger;
		len=sizeof(linger);
	}

	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(
		setsockopt(p->sock, level, opt, vp, len)==0));
	p->last_error=ERROR_VALUE;

	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_ioctlsocket(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	int32		cmd=0;
	int32		arg=0;
	private_t*	p;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(argc && argv[0]!=JSVAL_VOID)
		JS_ValueToInt32(cx,argv[0],&cmd);
	if(argc>1 && argv[1]!=JSVAL_VOID)
		JS_ValueToInt32(cx,argv[1],&arg);

	rc=JS_SUSPENDREQUEST(cx);
	if(ioctlsocket(p->sock,cmd,(ulong*)&arg)==0) {
		JS_RESUMEREQUEST(cx, rc);
		JS_SET_RVAL(cx, arglist,INT_TO_JSVAL(arg));
	}
	else {
		JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(-1));
		JS_RESUMEREQUEST(cx, rc);
	}

	p->last_error=ERROR_VALUE;

	return(JS_TRUE);
}

static JSBool
js_poll(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	private_t*	p;
	BOOL	poll_for_write=FALSE;
	fd_set	socket_set;
	fd_set*	rd_set=NULL;
	fd_set*	wr_set=NULL;
	uintN	argn;
	int		result;
	struct	timeval tv = {0, 0};
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(p->sock==INVALID_SOCKET) {
		dbprintf(TRUE, p, "INVALID SOCKET in call to poll");
		JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(-1));
		return(JS_TRUE);
	}

	for(argn=0;argn<argc;argn++) {
		if(JSVAL_IS_BOOLEAN(argv[argn]))
			poll_for_write=JSVAL_TO_BOOLEAN(argv[argn]);
		else if(JSVAL_IS_NUMBER(argv[argn]))
			js_timeval(cx,argv[argn],&tv);
	}

	rc=JS_SUSPENDREQUEST(cx);
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

	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(result));
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}


/* Socket Object Properites */
enum {
	 SOCK_PROP_LAST_ERROR
	,SOCK_PROP_IS_CONNECTED
	,SOCK_PROP_IS_WRITEABLE
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
	,SOCK_PROP_NETWORK_ORDER
	,SOCK_PROP_SSL_SESSION

};

#ifdef BUILD_JSDOCS
static char* socket_prop_desc[] = {
	/* statically-defined properties: */
	 "array of socket option names supported by the current platform"
	/* Regular properties */
	,"error status for the last socket operation that failed - <small>READ ONLY</small>"
	,"<i>true</i> if socket is in a connected state - <small>READ ONLY</small>"
	,"<i>true</i> if socket can accept written data - Setting to false will shutdown the write end of the socket."
	,"alias for is_writeable"
	,"<i>true</i> if data is waiting to be read from socket - <small>READ ONLY</small>"
	,"number of bytes waiting to be read - <small>READ ONLY</small>"
	,"enable debug logging"
	,"socket descriptor (advanced uses only)"
	,"use non-blocking operation (default is <i>false</i>)"
	,"local IP address (string in dotted-decimal format)"
	,"local TCP or UDP port number"
	,"remote IP address (string in dotted-decimal format)"
	,"remote TCP or UDP port number"
	,"socket type, <tt>SOCK_STREAM</tt> (TCP) or <tt>SOCK_DGRAM</tt> (UDP)"
	,"<i>true</i> if binary data is to be sent in Network Byte Order (big end first), default is <i>true</i>"
	,"set to <i>true</i> to enable SSL as a client on the socket"
	,NULL
};
#endif

static JSBool js_socket_set(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
	jsval idval;
    jsint       tiny;
	private_t*	p;
	jsrefcount	rc;
	BOOL		b;
	int32		i;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		// Prototype access
		return(JS_TRUE);
	}

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	rc=JS_SUSPENDREQUEST(cx);
	dbprintf(FALSE, p, "setting property %d",tiny);
	JS_RESUMEREQUEST(cx, rc);

	switch(tiny) {
		case SOCK_PROP_DEBUG:
			JS_ValueToBoolean(cx,*vp,&(p->debug));
			break;
		case SOCK_PROP_DESCRIPTOR:
			if(p->session != -1) {
				cryptDestroySession(p->session);
				p->session=-1;
			}
			if(JS_ValueToInt32(cx,*vp,&i))
				p->sock = i;
			p->is_connected=TRUE;
			break;
		case SOCK_PROP_LAST_ERROR:
			if(JS_ValueToInt32(cx,*vp,&i))
				p->last_error = i;
			break;
		case SOCK_PROP_NONBLOCKING:
			JS_ValueToBoolean(cx,*vp,&(p->nonblocking));
			rc=JS_SUSPENDREQUEST(cx);
			ioctlsocket(p->sock,FIONBIO,(ulong*)&(p->nonblocking));
			JS_RESUMEREQUEST(cx, rc);
			break;
		case SOCK_PROP_NETWORK_ORDER:
			JS_ValueToBoolean(cx,*vp,&(p->network_byte_order));
			break;
		case SOCK_PROP_IS_WRITEABLE:
			JS_ValueToBoolean(cx,*vp,&b);
			if(!b)
				shutdown(p->sock,SHUT_WR);
			break;
		case SOCK_PROP_SSL_SESSION:
			JS_ValueToBoolean(cx,*vp,&b);
			rc=JS_SUSPENDREQUEST(cx);
			if(b) {
				if(p->session==-1) {
					int ret;

					if(do_cryptInit()) {
						if((ret=cryptCreateSession(&p->session, CRYPT_UNUSED, CRYPT_SESSION_SSL))==CRYPT_OK) {
							ulong nb=0;
							ioctlsocket(p->sock,FIONBIO,&nb);
							nb=1;
							setsockopt(p->sock,IPPROTO_TCP,TCP_NODELAY,(char*)&nb,sizeof(nb));
							if((ret=do_cryptAttribute(p->session, CRYPT_SESSINFO_NETWORKSOCKET, p->sock))==CRYPT_OK) {
//								if((ret=do_cryptAttribute(p->session, CRYPT_SESSINFO_VERSION, 0))==CRYPT_OK) {
									if((ret=do_cryptAttributeString(p->session, CRYPT_SESSINFO_SERVER_NAME, p->hostname, strlen(p->hostname)))==CRYPT_OK) {
										if((ret=do_cryptAttribute(p->session, CRYPT_SESSINFO_ACTIVE, 1))!=CRYPT_OK) {
											cryptDestroySession(p->session);
											p->session=-1;
											ioctlsocket(p->sock,FIONBIO,(ulong*)&(p->nonblocking));
										}
									}
//								}
							}
							else {
								cryptDestroySession(p->session);
								p->session=-1;
								ioctlsocket(p->sock,FIONBIO,(ulong*)&(p->nonblocking));
							}
						}
						else lprintf(LOG_ERR,"cryptCreateSession() Error %d",ret);
					}
				}
			}
			else {
				if(p->session != -1) {
					cryptDestroySession(p->session);
					p->session=-1;
					ioctlsocket(p->sock,FIONBIO,(ulong*)&(p->nonblocking));
				}
			}
			JS_RESUMEREQUEST(cx, rc);
			break;
	}

	return(JS_TRUE);
}

static JSBool js_socket_get(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	jsval idval;
    jsint       tiny;
	ulong		cnt;
	BOOL		rd;
	BOOL		wr;
	private_t*	p;
	JSString*	js_str;
	SOCKADDR_IN	addr;
	socklen_t	len=sizeof(addr);
	jsrefcount	rc;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL) {
		// Protoype access
		return(JS_TRUE);
	}

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	rc=JS_SUSPENDREQUEST(cx);
#if 0 /* just too much */
	dbprintf(FALSE, p, "getting property %d",tiny);
#endif

	switch(tiny) {
		case SOCK_PROP_LAST_ERROR:
			*vp = INT_TO_JSVAL(p->last_error);
			break;
		case SOCK_PROP_IS_CONNECTED:
			if(!p->is_connected)
				*vp = JSVAL_FALSE;
			else
				*vp = BOOLEAN_TO_JSVAL(socket_check(p->sock,NULL,NULL,0));
			break;
		case SOCK_PROP_IS_WRITEABLE:
			socket_check(p->sock,NULL,&wr,0);
			*vp = BOOLEAN_TO_JSVAL(wr);
			break;
		case SOCK_PROP_DATA_WAITING:
			socket_check(p->sock,&rd,NULL,0);
			*vp = BOOLEAN_TO_JSVAL(rd);
			break;
		case SOCK_PROP_NREAD:
			cnt=0;
			if(ioctlsocket(p->sock, FIONREAD, &cnt)==0) {
				*vp=DOUBLE_TO_JSVAL((double)cnt);
			}
			else
				*vp = JSVAL_ZERO;
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
			if(p->sock != INVALID_SOCKET) {
				if(getsockname(p->sock, (struct sockaddr *)&addr,&len)!=0)
					return(JS_FALSE);
				JS_RESUMEREQUEST(cx, rc);
				if((js_str=JS_NewStringCopyZ(cx,inet_ntoa(addr.sin_addr)))==NULL)
					return(JS_FALSE);
				*vp = STRING_TO_JSVAL(js_str);
				rc=JS_SUSPENDREQUEST(cx);
			}
			else
				*vp=JSVAL_VOID;
			break;
		case SOCK_PROP_LOCAL_PORT:
			if(p->sock != INVALID_SOCKET) {
				if(getsockname(p->sock, (struct sockaddr *)&addr,&len)!=0)
					return(JS_FALSE);
				JS_RESUMEREQUEST(cx, rc);
				if((js_str=JS_NewStringCopyZ(cx,inet_ntoa(addr.sin_addr)))==NULL)
					return(JS_FALSE);

				*vp = INT_TO_JSVAL(ntohs(addr.sin_port));
				rc=JS_SUSPENDREQUEST(cx);
			}
			else
				*vp=JSVAL_VOID;
			break;
		case SOCK_PROP_REMOTE_IP:
			if(p->is_connected) {
				JS_RESUMEREQUEST(cx, rc);
				if((js_str=JS_NewStringCopyZ(cx,inet_ntoa(p->remote_addr.sin_addr)))==NULL)
					return(JS_FALSE);
				*vp = STRING_TO_JSVAL(js_str);
				rc=JS_SUSPENDREQUEST(cx);
			}
			else
				*vp=JSVAL_VOID;
			break;
		case SOCK_PROP_REMOTE_PORT:
			if(p->is_connected)
				*vp = INT_TO_JSVAL(ntohs(p->remote_addr.sin_port));
			else
				*vp=JSVAL_VOID;
			break;
		case SOCK_PROP_TYPE:
			*vp = INT_TO_JSVAL(p->type);
			break;
		case SOCK_PROP_NETWORK_ORDER:
			*vp = BOOLEAN_TO_JSVAL(p->network_byte_order);
			break;
		case SOCK_PROP_SSL_SESSION:
			*vp = BOOLEAN_TO_JSVAL(p->session != -1);
			break;
	}

	JS_RESUMEREQUEST(cx, rc);
	return(TRUE);
}

#define SOCK_PROP_FLAGS JSPROP_ENUMERATE|JSPROP_READONLY

static jsSyncPropertySpec js_socket_properties[] = {
/*		 name				,tinyid					,flags,				ver	*/

	{	"error"				,SOCK_PROP_LAST_ERROR	,SOCK_PROP_FLAGS,	311 },
	{	"last_error"		,SOCK_PROP_LAST_ERROR	,JSPROP_READONLY,	310 },	/* alias */
	{	"is_connected"		,SOCK_PROP_IS_CONNECTED	,SOCK_PROP_FLAGS,	310 },
	{	"is_writeable"		,SOCK_PROP_IS_WRITEABLE	,JSPROP_ENUMERATE,	311 },
	{	"is_writable"		,SOCK_PROP_IS_WRITEABLE	,JSPROP_ENUMERATE,	312 },	/* alias */
	{	"data_waiting"		,SOCK_PROP_DATA_WAITING	,SOCK_PROP_FLAGS,	310 },
	{	"nread"				,SOCK_PROP_NREAD		,SOCK_PROP_FLAGS,	310 },
	{	"debug"				,SOCK_PROP_DEBUG		,JSPROP_ENUMERATE,	310 },
	{	"descriptor"		,SOCK_PROP_DESCRIPTOR	,JSPROP_ENUMERATE,	310 },
	{	"nonblocking"		,SOCK_PROP_NONBLOCKING	,JSPROP_ENUMERATE,	310 },
	{	"local_ip_address"	,SOCK_PROP_LOCAL_IP		,SOCK_PROP_FLAGS,	310 },
	{	"local_port"		,SOCK_PROP_LOCAL_PORT	,SOCK_PROP_FLAGS,	310 },
	{	"remote_ip_address"	,SOCK_PROP_REMOTE_IP	,SOCK_PROP_FLAGS,	310 },
	{	"remote_port"		,SOCK_PROP_REMOTE_PORT	,SOCK_PROP_FLAGS,	310 },
	{	"type"				,SOCK_PROP_TYPE			,SOCK_PROP_FLAGS,	310 },
	{	"network_byte_order",SOCK_PROP_NETWORK_ORDER,JSPROP_ENUMERATE,	311 },
	{	"ssl_session"		,SOCK_PROP_SSL_SESSION	,JSPROP_ENUMERATE,	316	},
	{0}
};

static jsSyncMethodSpec js_socket_functions[] = {
	{"close",		js_close,		0,	JSTYPE_VOID,	""
	,JSDOCSTR("close (shutdown) the socket immediately")
	,310
	},
	{"bind",		js_bind,		0,	JSTYPE_BOOLEAN,	JSDOCSTR("[port] [,ip_address]")
	,JSDOCSTR("bind socket to a TCP or UDP <i>port</i> (number or service name), "
		"optionally specifying a network interface (via <i>ip_address</i>)")
	,311
	},
	{"connect",     js_connect,     2,	JSTYPE_BOOLEAN,	JSDOCSTR("host, port [,timeout=<tt>10.0</tt>]")
	,JSDOCSTR("connect to a remote port (number or service name) on the specified host (IP address or host name)"
	", default <i>timeout</i> value is <i>10.0</i> (seconds)")
	,311
	},
	{"listen",		js_listen,		0,	JSTYPE_BOOLEAN,	JSDOCSTR("")
	,JSDOCSTR("place socket in a state to listen for incoming connections (use before an accept)")
	,310
	},
	{"accept",		js_accept,		0,	JSTYPE_OBJECT,	JSDOCSTR("")					
	,JSDOCSTR("accept an incoming connection, returns a new <i>Socket</i> object representing the new connection")
	,310
	},
	{"write",		js_send,		1,	JSTYPE_ALIAS },
	{"send",		js_send,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("data")
	,JSDOCSTR("send a string (AKA write)")
	,310
	},
	{"sendto",		js_sendto,		3,	JSTYPE_BOOLEAN,	JSDOCSTR("data, address, port")
	,JSDOCSTR("send data to a specific host (IP address or host name) and port (number or service name), for UDP sockets")
	,310
	},
	{"sendfile",	js_sendfile,	1,	JSTYPE_BOOLEAN,	JSDOCSTR("path/filename")
	,JSDOCSTR("send an entire file over the socket")
	,310
	},
	{"writeBin",	js_sendbin,		1,	JSTYPE_ALIAS },
	{"sendBin",		js_sendbin,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("value [,bytes=<tt>4</tt>]")
	,JSDOCSTR("send a binary integer over the socket, default number of bytes is 4 (32-bits)")
	,311
	},
	{"read",		js_recv,		1,	JSTYPE_ALIAS },
	{"recv",		js_recv,		1,	JSTYPE_STRING,	JSDOCSTR("[maxlen=<tt>512</tt>]")
	,JSDOCSTR("receive a string, default maxlen is 512 characters (AKA read)")
	,310
	},
	{"peek",		js_peek,		0,	JSTYPE_STRING,	JSDOCSTR("[maxlen=<tt>512</tt>]")
	,JSDOCSTR("receive a string, default maxlen is 512 characters, leaves string in receive buffer")
	,310
	},
	{"readline",	js_recvline,	0,	JSTYPE_ALIAS },
	{"readln",		js_recvline,	0,	JSTYPE_ALIAS },
	{"recvline",	js_recvline,	0,	JSTYPE_STRING,	JSDOCSTR("[maxlen=<tt>512</tt>] [,timeout=<tt>30.0</tt>]")
	,JSDOCSTR("receive a line-feed terminated string, default maxlen is 512 characters, default timeout is 30 seconds (AKA readline and readln)")
	,310
	},
	{"recvfrom",	js_recvfrom,	0,	JSTYPE_OBJECT,	JSDOCSTR("[binary=<tt>false</tt>] [,maxlen=<tt>512</tt> or int_size=<tt>4</tt>]")
	,JSDOCSTR("receive data (string or integer) from a socket (typically UDP)"
	"<p>returns object with <i>ip_address</i> and <i>port</i> of sender along with <i>data</i> properties"
	"<p><i>binary</i> defaults to <i>false</i>, <i>maxlen</i> defaults to 512 chars, <i>int_size</i> defaults to 4 bytes (32-bits)")
	,311
	},
	{"readBin",		js_recvbin,		0,	JSTYPE_ALIAS },
	{"recvBin",		js_recvbin,		0,	JSTYPE_NUMBER,	JSDOCSTR("[bytes=<tt>4</tt>]")
	,JSDOCSTR("receive a binary integer from the socket, default number of bytes is 4 (32-bits)")
	,311
	},
	{"getoption",	js_getsockopt,	1,	JSTYPE_NUMBER,	JSDOCSTR("option")
	,JSDOCSTR("get socket option value, option may be socket option name "
	"(see <tt>sockopts</tt> in <tt>sockdefs.js</tt>) or number")
	,310
	},
	{"setoption",	js_setsockopt,	2,	JSTYPE_BOOLEAN,	JSDOCSTR("option, value")
	,JSDOCSTR("set socket option value, option may be socket option name "
	"(see <tt>sockopts</tt> in <tt>sockdefs.js</tt>) or number")
	,310
	},
	{"ioctl",		js_ioctlsocket,	1,	JSTYPE_NUMBER,	JSDOCSTR("command [,argument=<tt>0</tt>]")
	,JSDOCSTR("send socket IOCTL (advanced)")					
	,310
	},
	{"poll",		js_poll,		1,	JSTYPE_NUMBER,	JSDOCSTR("[timeout=<tt>0</tt>] [,write=<tt>false</tt>]")
	,JSDOCSTR("poll socket for read or write ability (default is <i>read</i>), "
	"default timeout value is 0.0 seconds (immediate timeout)")
	,310
	},
	{0}
};

static JSBool js_socket_resolve(JSContext *cx, JSObject *obj, jsid id)
{
	char*			name=NULL;
	JSBool			ret;

	if(id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;
		
		JS_IdToValue(cx, id, &idval);
		if(JSVAL_IS_STRING(idval)) {
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
			HANDLE_PENDING(cx);
		}
	}

	ret=js_SyncResolve(cx, obj, name, js_socket_properties, js_socket_functions, NULL, 0);
	if(name)
		free(name);
	return ret;
}

static JSBool js_socket_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_socket_resolve(cx, obj, JSID_VOID));
}

static JSClass js_socket_class = {
     "Socket"				/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_socket_get			/* getProperty	*/
	,js_socket_set			/* setProperty	*/
	,js_socket_enumerate	/* enumerate	*/
	,js_socket_resolve		/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,js_finalize_socket		/* finalize		*/
};

static BOOL js_DefineSocketOptionsArray(JSContext *cx, JSObject *obj, int type)
{
	size_t		i;
	size_t		count=0;
	jsval		val;
	JSObject*	array;
	socket_option_t* options;

	if((options=getSocketOptionList())==NULL)
		return(FALSE);

	if((array=JS_NewArrayObject(cx, 0, NULL))==NULL) 
		return(FALSE);

	if(!JS_DefineProperty(cx, obj, "option_list", OBJECT_TO_JSVAL(array)
		, NULL, NULL, JSPROP_ENUMERATE))
		return(FALSE);

	for(i=0; options[i].name!=NULL; i++) {
		if(options[i].type && options[i].type!=type)
			continue;
		val=STRING_TO_JSVAL(JS_NewStringCopyZ(cx,options[i].name));
		JS_SetElement(cx, array, count++, &val);
	}
	return(TRUE);
}

/* Socket Constructor (creates socket descriptor) */

static JSBool
js_socket_constructor(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj;
	jsval *argv=JS_ARGV(cx, arglist);
	int32	type=SOCK_STREAM;	/* default = TCP */
	uintN	i;
	private_t* p;
	char*	protocol=NULL;

	obj=JS_NewObject(cx, &js_socket_class, NULL, NULL);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));

	for(i=0;i<argc;i++) {
		if(JSVAL_IS_NUMBER(argv[i]))
			JS_ValueToInt32(cx,argv[i],&type);
		else if(protocol==NULL) {
			JSVALUE_TO_MSTRING(cx, argv[i], protocol, NULL);
			HANDLE_PENDING(cx);
		}
	}
		
	if((p=(private_t*)malloc(sizeof(private_t)))==NULL) {
		JS_ReportError(cx,"malloc failed");
		if(protocol)
			free(protocol);
		return(JS_FALSE);
	}
	memset(p,0,sizeof(private_t));

	if((p->sock=open_socket(type,protocol))==INVALID_SOCKET) {
		JS_ReportError(cx,"open_socket failed with error %d",ERROR_VALUE);
		if(protocol)
			free(protocol);
		return(JS_FALSE);
	}
	if(protocol)
		free(protocol);
	p->type = type;
	p->network_byte_order = TRUE;
	p->session=-1;

	if(!JS_SetPrivate(cx, obj, p)) {
		JS_ReportError(cx,"JS_SetPrivate failed");
		return(JS_FALSE);
	}

	if(!js_DefineSocketOptionsArray(cx, obj, type))
		return(JS_FALSE);

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx,obj,"Class used for TCP/IP socket communications",310);
	js_DescribeSyncConstructor(cx,obj,"To create a new Socket object: "
		"<tt>load('sockdefs.js'); var s = new Socket(<i>type</i>, <i>protocol</i>)</tt><br>"
		"where <i>type</i> = <tt>SOCK_STREAM</tt> for TCP (default) or <tt>SOCK_DGRAM</tt> for UDP<br>"
		"and <i>protocol</i> (optional) = the name of the protocol or service the socket is to be used for"
		);
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", socket_prop_desc, JSPROP_READONLY);
#endif

	dbprintf(FALSE, p, "object constructed");
	return(JS_TRUE);
}

JSObject* DLLCALL js_CreateSocketClass(JSContext* cx, JSObject* parent)
{
	JSObject*	sockobj;

	sockobj = JS_InitClass(cx, parent, NULL
		,&js_socket_class
		,js_socket_constructor
		,0	/* number of constructor args */
		,NULL /* props, specified in constructor */
		,NULL /* funcs, specified in constructor */
		,NULL,NULL);

	return(sockobj);
}

JSObject* DLLCALL js_CreateSocketObject(JSContext* cx, JSObject* parent, char *name, SOCKET sock)
{
	JSObject*	obj;
	private_t*	p;
	int			type=SOCK_STREAM;
	socklen_t	len;

	obj = JS_DefineObject(cx, parent, name, &js_socket_class, NULL
		,JSPROP_ENUMERATE|JSPROP_READONLY);

	if(obj==NULL)
		return(NULL);

	len = sizeof(type);
	getsockopt(sock,SOL_SOCKET,SO_TYPE,(void*)&type,&len);

	if(!js_DefineSocketOptionsArray(cx, obj, type))
		return(NULL);

	if((p=(private_t*)malloc(sizeof(private_t)))==NULL)
		return(NULL);
	memset(p,0,sizeof(private_t));

	p->sock = sock;
	p->external = TRUE;
	p->network_byte_order = TRUE;
	p->session=-1;

	len=sizeof(p->remote_addr);
	if(getpeername(p->sock, (struct sockaddr *)&p->remote_addr,&len)==0)
		p->is_connected=TRUE;

	if(!JS_SetPrivate(cx, obj, p)) {
		dbprintf(TRUE, p, "JS_SetPrivate failed");
		return(NULL);
	}

	dbprintf(FALSE, p, "object created");

	return(obj);
}

#endif	/* JAVSCRIPT */
