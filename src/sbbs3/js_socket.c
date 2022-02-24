/* Synchronet JavaScript "Socket" Object */

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

#include <cryptlib.h>

#include "sbbs.h"
#include "js_socket.h"
#include "js_request.h"
#include "multisock.h"
#include "ssl.h"

#ifdef JAVASCRIPT

static void dbprintf(BOOL error, js_socket_private_t* p, char* fmt, ...);
static bool do_CryptFlush(js_socket_private_t *p);
static int do_cryptAttribute(const CRYPT_CONTEXT session, CRYPT_ATTRIBUTE_TYPE attr, int val);
static int do_cryptAttributeString(const CRYPT_CONTEXT session, CRYPT_ATTRIBUTE_TYPE attr, void *val, int len);
static void do_js_close(JSContext *cx, js_socket_private_t *p, bool finalize);
static BOOL js_DefineSocketOptionsArray(JSContext *cx, JSObject *obj, int type);
static JSBool js_accept(JSContext *cx, uintN argc, jsval *arglist);
static JSBool js_bind(JSContext *cx, uintN argc, jsval *arglist);
static JSBool js_close(JSContext *cx, uintN argc, jsval *arglist);
static JSBool js_connect(JSContext *cx, uintN argc, jsval *arglist);
static void js_finalize_socket(JSContext *cx, JSObject *obj);
static JSBool js_ioctlsocket(JSContext *cx, uintN argc, jsval *arglist);
static JSBool js_listen(JSContext *cx, uintN argc, jsval *arglist);
static js_callback_t * js_get_callback(JSContext *cx);
static JSBool js_getsockopt(JSContext *cx, uintN argc, jsval *arglist);
static JSBool js_peek(JSContext *cx, uintN argc, jsval *arglist);
static JSBool js_poll(JSContext *cx, uintN argc, jsval *arglist);
static ushort js_port(JSContext* cx, jsval val, int type);
static JSBool js_recv(JSContext *cx, uintN argc, jsval *arglist);
static JSBool js_recvbin(JSContext *cx, uintN argc, jsval *arglist);
static JSBool js_recvfrom(JSContext *cx, uintN argc, jsval *arglist);
static JSBool js_recvline(JSContext *cx, uintN argc, jsval *arglist);
static JSBool js_send(JSContext *cx, uintN argc, jsval *arglist);
static JSBool js_sendbin(JSContext *cx, uintN argc, jsval *arglist);
static JSBool js_sendfile(JSContext *cx, uintN argc, jsval *arglist);
static JSBool js_sendline(JSContext *cx, uintN argc, jsval *arglist);
static JSBool js_sendto(JSContext *cx, uintN argc, jsval *arglist);
static JSBool js_setsockopt(JSContext *cx, uintN argc, jsval *arglist);
static int js_sock_read_check(js_socket_private_t *p, time_t start, int32 timeout, int i);
static JSBool js_socket_constructor(JSContext *cx, uintN argc, jsval *arglist);
static JSBool js_socket_enumerate(JSContext *cx, JSObject *obj);
static BOOL js_socket_peek_byte(JSContext *cx, js_socket_private_t *p);
static JSBool js_socket_get(JSContext *cx, JSObject *obj, jsid id, jsval *vp);
static ptrdiff_t js_socket_recv(JSContext *cx, js_socket_private_t *p, void *buf, size_t len, int flags, int timeout);
static JSBool js_socket_resolve(JSContext *cx, JSObject *obj, jsid id);
static off_t js_socket_sendfilesocket(js_socket_private_t *p, int file, off_t *offset, off_t count);
static ptrdiff_t js_socket_sendsocket(js_socket_private_t *p, const void *msg, size_t len, int flush);
static JSBool js_socket_set(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp);
static JSBool js_install_event(JSContext *cx, uintN argc, jsval *arglist, BOOL once);
static JSBool js_on(JSContext *cx, uintN argc, jsval *arglist);
static JSBool js_once(JSContext *cx, uintN argc, jsval *arglist);

static int do_cryptAttribute(const CRYPT_CONTEXT session, CRYPT_ATTRIBUTE_TYPE attr, int val)
{
	int ret;
	int level;
	char *estr;
	char action[32];

	/* Force "sane" values (requirements) */
	switch(attr) {
		case CRYPT_OPTION_NET_READTIMEOUT:
			if (val < 0)
				val = 0;
			if (val > 300)
				val = 300;
			break;
		default:
			break;
	}

	ret=cryptSetAttribute(session, attr, val);
	if(ret != CRYPT_OK) {
		sprintf(action, "setting attribute %d", attr);
		get_crypt_error_string(ret, session, &estr, action, &level);
		if (estr) {
			lprintf(level, "TLS %s", estr);
			free_crypt_attrstr(estr);
		}
	}
	return ret;
}

static int do_cryptAttributeString(const CRYPT_CONTEXT session, CRYPT_ATTRIBUTE_TYPE attr, void *val, int len)
{
	int level;
	char *estr;
	char action[48];

	int ret=cryptSetAttributeString(session, attr, val, len);
	if(ret != CRYPT_OK) {
		sprintf(action, "setting attribute string %d", attr);
		get_crypt_error_string(ret, session, &estr, "setting attribute string", &level);
		if (estr) {
			lprintf(level, "TLS %s", estr);
			free_crypt_attrstr(estr);
		}
	}
	return ret;
}

#define GCES(status, pdata, estr, action) do {                                               \
	int GCES_level;                                                                      \
	get_crypt_error_string(status, pdata->session, &estr, action, &GCES_level); \
	if (estr) {                                                                          \
		lprintf(GCES_level, "%04d TLS %s", p->sock, estr);                               \
		free_crypt_attrstr(estr);                                                                  \
	}                                                                                    \
} while(0)

#define GCESH(status, socket, handle, estr, action) do {                                     \
	int GCESH_level;                                                                     \
	get_crypt_error_string(status, handle, &estr, action, &GCESH_level);        \
	if (estr) {                                                                          \
		lprintf(GCESH_level, "%04d TLS %s", socket, estr);                               \
		free_crypt_attrstr(estr);                                                                  \
	}                                                                                    \
} while(0)

static bool do_CryptFlush(js_socket_private_t *p)
{
	int ret;
	char	*estr;

	if (p->unflushed) {
		ret = cryptFlushData(p->session);

		if(ret==CRYPT_OK) {
			p->unflushed = 0;
			return true;
		}
		if (ret != CRYPT_ERROR_COMPLETE)
			GCES(ret, p, estr, "flushing data");
		return false;
	}
	return true;
}

static void
remove_js_socket_event(JSContext *cx, js_callback_t *cb, SOCKET sock)
{
	struct js_event_list *ev;
	struct js_event_list *nev;

	if (!cb->events_supported) {
		return;
	}

	for (ev = cb->events; ev; ev = nev) {
		nev = ev->next;
		if (ev->type == JS_EVENT_SOCKET_READABLE || ev->type == JS_EVENT_SOCKET_READABLE_ONCE
		    || ev->type == JS_EVENT_SOCKET_WRITABLE || ev->type == JS_EVENT_SOCKET_WRITABLE_ONCE) {
			if (ev->data.sock == sock) {
				if (ev->next)
					ev->next->prev = ev->prev;
				if (ev->prev)
					ev->prev->next = ev->next;
				else
					cb->events = ev->next;
				JS_RemoveObjectRoot(cx, &ev->cx);
				free(ev);
			}
		}
		else if(ev->type == JS_EVENT_SOCKET_CONNECT) {
			if (ev->data.connect.sock == sock) {
				if (ev->next)
					ev->next->prev = ev->prev;
				if (ev->prev)
					ev->prev->next = ev->next;
				else
					cb->events = ev->next;
				closesocket(ev->data.connect.sv[0]);
				JS_RemoveObjectRoot(cx, &ev->cx);
				free(ev);
			}
		}
	}
}

static void do_js_close(JSContext *cx, js_socket_private_t *p, bool finalize)
{
	size_t i;

	if(p->session != -1) {
		cryptDestroySession(p->session);
		p->session=-1;
	}

	// Delete any event handlers for the socket
	if (p->js_cb) {
		if (p->set) {
			for (i = 0; i < p->set->sock_count; i++) {
				if (p->set->socks[i].sock != INVALID_SOCKET)
					remove_js_socket_event(cx, p->js_cb, p->set->socks[i].sock);
			}
		}
		else {
			if (p->sock != INVALID_SOCKET)
				remove_js_socket_event(cx, p->js_cb, p->sock);
		}
	}

	if(p->sock==INVALID_SOCKET) {
		p->is_connected = FALSE;
		return;
	}
	if(p->external==FALSE) {
		close_socket(p->sock);
		p->last_error = ERROR_VALUE;
	}
	else {
		if (!finalize)
			shutdown(p->sock, SHUT_RDWR);
	}

	// This is a lie for external sockets... don't tell anyone.
	p->sock = INVALID_SOCKET;
	p->is_connected = FALSE;
}

static BOOL js_socket_peek_byte(JSContext *cx, js_socket_private_t *p)
{
	if (do_cryptAttribute(p->session, CRYPT_OPTION_NET_READTIMEOUT, 0) != CRYPT_OK)
		return FALSE;
	if (p->peeked)
		return TRUE;
	if (js_socket_recv(cx, p, &p->peeked_byte, 1, 0, 0) == 1) {
		p->peeked = TRUE;
		return TRUE;
	}
	return FALSE;
}

/* Returns > 0 upon successful data received (even if there was an error or disconnection) */
/* Returns -1 upon error (and no data received) */
/* Returns 0 upon timeout or disconnection (and no data received) */
static ptrdiff_t js_socket_recv(JSContext *cx, js_socket_private_t *p, void *buf, size_t len, int flags, int timeout)
{
	ptrdiff_t	total=0;
	int	copied,ret;
	char *estr;
	time_t now = time(NULL);
	int status;

	if (len == 0)
		return total;
	if (p->session != -1) {
		if (flags & MSG_PEEK)
			js_socket_peek_byte(cx, p);
		if (p->peeked) {
			*(uint8_t *)buf = p->peeked_byte;
			buf=((uint8_t *)buf) + 1;
			if (!(flags & MSG_PEEK))
				p->peeked = FALSE;
			total++;
			len--;
			if (len == 0)
				return total;
		}
		if (flags & MSG_PEEK)
			return total;
		if (do_cryptAttribute(p->session, CRYPT_OPTION_NET_READTIMEOUT, p->nonblocking?0:timeout) != CRYPT_OK)
			return -1;
	}
	do {
		if(p->session==-1) {
			if (p->sock == INVALID_SOCKET)
				ret = -1;
			else {
				ret = 0;
				if (socket_readable(p->sock, timeout * 1000))
					ret = recv(p->sock, buf, len, flags);
			}
		}
		else {
			status = cryptPopData(p->session, buf, len, &copied);
			if(cryptStatusOK(status))
				ret = copied;
			else {
				ret = -1;
				if (status == CRYPT_ERROR_TIMEOUT)
					ret = 0;
				else if (status != CRYPT_ERROR_COMPLETE) {
					GCES(ret, p, estr, "popping data");
					do_js_close(cx, p, false);
				}
			}
		}
		if (ret == -1) {
			if (total > 0)
				return total;
			return ret;
		}
		if ((!(flags & MSG_WAITALL)) || p->nonblocking)
			return ret;
		total += ret;
		if(total>=(ptrdiff_t)len)
			return total;
		len-=ret;
		buf=((uint8_t *)buf) + ret;

		if(!socket_check(p->sock,NULL,NULL,0)) {
			if (total > 0)
				return total;
			return -1;
		}
		if (now + timeout > time(NULL))
			return total;
	} while(len);
	return total;
}

static ptrdiff_t js_socket_sendsocket(js_socket_private_t *p, const void *msg, size_t len, int flush)
{
	ptrdiff_t total=0;
	int copied=0,ret;
	char *estr;

	if(p->session==-1)
		return sendsocket(p->sock, msg, len);
	do {
		// If we don't limit this, we occasionally get errors on large sends...
		if((ret=cryptPushData(p->session, msg, len > 0x2000 ? 0x2000 : len, &copied))==CRYPT_OK) {
			p->unflushed += copied;
			if(flush)
				do_CryptFlush(p);
			if(p->nonblocking)
				return copied;
			total += copied;
			if(total >= (ptrdiff_t)len)
				return total;
			do_CryptFlush(p);
			len -= copied;
			msg=((uint8_t *)msg) + copied;
		}
		else {
			if (ret != CRYPT_ERROR_COMPLETE)
				GCES(ret, p, estr, "pushing data");
			if(flush)
				do_CryptFlush(p);
			return total;
		}
		if(!socket_check(p->sock,NULL,NULL,0))
			break;
	} while(len);
	if(flush)
		do_CryptFlush(p);
	return total;
}

static off_t js_socket_sendfilesocket(js_socket_private_t *p, int file, off_t *offset, off_t count)
{
	char		buf[1024*16];
	off_t		len;
	int			rd;
	int			wr=0;
	off_t		total=0;
	int			i;

	if(p->session==-1)
		return (int)sendfilesocket(p->sock, file, offset, count);

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
			do_CryptFlush(p);
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
			do_CryptFlush(p);
			return(wr);
		}
		if(i!=rd) {
			do_CryptFlush(p);
			return(-1);
		}
		total+=rd;
	}

	if(offset!=NULL)
		(*offset)+=total;

	do_CryptFlush(p);
	return(total);
}

static void dbprintf(BOOL error, js_socket_private_t* p, char* fmt, ...)
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
	js_socket_private_t* p;

	if((p=(js_socket_private_t*)JS_GetPrivate(cx,obj))==NULL)
		return;

	do_js_close(cx, p, true);

	if(!p->external)
		free(p->set);
	free(p->hostname);
	free(p);

	JS_SetPrivate(cx, obj, NULL);
}


/* Socket Object Methods */

extern JSClass js_socket_class;
static JSBool
js_close(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	js_socket_private_t*	p;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(js_socket_private_t*)js_GetClassPrivate(cx, obj, &js_socket_class))==NULL) {
		return(JS_FALSE);
	}

	rc=JS_SUSPENDREQUEST(cx);
	do_js_close(cx, p, false);
	dbprintf(FALSE, p, "closed");
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
		if(IS_DIGIT(*cp))
			return((ushort)strtol(cp,NULL,0));
		rc=JS_SUSPENDREQUEST(cx);
		serv = getservbyname(cp,type==SOCK_STREAM ? "tcp":"udp");
		JS_RESUMEREQUEST(cx, rc);
		if(serv!=NULL)
			return(htons(serv->s_port));
	}
	return(0);
}

SOCKET js_socket(JSContext *cx, jsval val)
{
	void*		vp;
	JSClass*	cl;
	SOCKET		sock=INVALID_SOCKET;

	if(JSVAL_IS_OBJECT(val) && (cl=JS_GetClass(cx,JSVAL_TO_OBJECT(val)))!=NULL) {
		if(cl->flags&JSCLASS_HAS_PRIVATE)
			if((vp=JS_GetInstancePrivate(cx,JSVAL_TO_OBJECT(val), &js_socket_class,NULL))!=NULL)
				sock=*(SOCKET*)vp;
	} else if(val!=JSVAL_VOID) {
		int32	i;
		if(JS_ValueToInt32(cx,val,&i))
			sock = i;
	}

	return(sock);
}

#ifdef PREFER_POLL
size_t js_socket_numsocks(JSContext *cx, jsval val)
{
	js_socket_private_t	*p;
	JSClass*	cl;
	SOCKET		sock=INVALID_SOCKET;
	size_t		i;
	int32_t		intval;
	size_t ret = 0;

	if(JSVAL_IS_OBJECT(val) && (cl=JS_GetClass(cx,JSVAL_TO_OBJECT(val)))!=NULL) {
		if(cl->flags&JSCLASS_HAS_PRIVATE) {
			if((p=(js_socket_private_t *)JS_GetInstancePrivate(cx,JSVAL_TO_OBJECT(val),&js_socket_class,NULL))!=NULL) {
				if(p->set) {
					for(i=0; i<p->set->sock_count; i++) {
						if(p->set->socks[i].sock == INVALID_SOCKET)
							continue;
						ret++;
					}
				}
				else {
					sock = p->sock;
					if(sock != INVALID_SOCKET)
						ret = 1;
				}
			}
		}
	} else if(val!=JSVAL_VOID) {
		if(JS_ValueToInt32(cx,val,&intval)) {
			if (intval != INVALID_SOCKET)
				ret = 1;
		}
	}
	return ret;
}

size_t js_socket_add(JSContext *cx, jsval val, struct pollfd *fds, short events)
{
	js_socket_private_t	*p;
	JSClass*	cl;
	SOCKET		sock=INVALID_SOCKET;
	size_t		i;
	int32_t		intval;
	size_t ret = 0;

	if(JSVAL_IS_OBJECT(val) && (cl=JS_GetClass(cx,JSVAL_TO_OBJECT(val)))!=NULL) {
		if(cl->flags&JSCLASS_HAS_PRIVATE) {
			if((p=(js_socket_private_t *)JS_GetInstancePrivate(cx,JSVAL_TO_OBJECT(val),&js_socket_class,NULL))!=NULL) {
				if(p->set) {
					for(i=0; i<p->set->sock_count; i++) {
						if(p->set->socks[i].sock == INVALID_SOCKET)
							continue;
						fds[ret].events = events;
						fds[ret++].fd = p->set->socks[i].sock;
					}
				}
				else {
					sock = p->sock;
					if(sock != INVALID_SOCKET) {
						fds[ret].events = events;
						fds[ret++].fd = sock;
					}
				}
			}
		}
	} else if(val!=JSVAL_VOID) {
		if(JS_ValueToInt32(cx,val,&intval)) {
			sock = intval;
			if(sock != INVALID_SOCKET) {
				fds[ret].events = events;
				fds[ret++].fd = sock;
			}
		}
	}
	return ret;
}
#else
SOCKET js_socket_add(JSContext *cx, jsval val, fd_set *fds)
{
	js_socket_private_t	*p;
	JSClass*	cl;
	SOCKET		sock=INVALID_SOCKET;
	size_t		i;
	int32_t		intval;

	if(JSVAL_IS_OBJECT(val) && (cl=JS_GetClass(cx,JSVAL_TO_OBJECT(val)))!=NULL) {
		if(cl->flags&JSCLASS_HAS_PRIVATE) {
			if((p=(js_socket_private_t *)JS_GetInstancePrivate(cx,JSVAL_TO_OBJECT(val),&js_socket_class,NULL))!=NULL) {
				if(p->set) {
					for(i=0; i<p->set->sock_count; i++) {
						if(p->set->socks[i].sock == INVALID_SOCKET)
							continue;
						FD_SET(p->set->socks[i].sock, fds);
						if(p->set->socks[i].sock > sock)
							sock = p->set->socks[i].sock;
					}
				}
				else {
					sock = p->sock;
					if(sock != INVALID_SOCKET)
						FD_SET(p->sock, fds);
				}
			}
		}
	} else if(val!=JSVAL_VOID) {
		if(JS_ValueToInt32(cx,val,&intval)) {
			sock = intval;
			FD_SET(sock, fds);
		}
	}
	return sock;
}

BOOL  js_socket_isset(JSContext *cx, jsval val, fd_set *fds)
{
	js_socket_private_t	*p;
	JSClass*	cl;
	size_t		i;
	int			intval;

	if(JSVAL_IS_OBJECT(val) && (cl=JS_GetClass(cx,JSVAL_TO_OBJECT(val)))!=NULL) {
		if(cl->flags&JSCLASS_HAS_PRIVATE) {
			if((p=(js_socket_private_t *)JS_GetInstancePrivate(cx,JSVAL_TO_OBJECT(val),&js_socket_class,NULL))!=NULL) {
				if(p->set) {
					for(i=0; i<p->set->sock_count; i++) {
						if(p->set->socks[i].sock == INVALID_SOCKET)
							continue;
						if(FD_ISSET(p->set->socks[i].sock, fds))
							return TRUE;
					}
				}
				else {
					if (p->sock == INVALID_SOCKET)
						return TRUE;
					else {
						if(FD_ISSET(p->sock, fds))
							return TRUE;
					}
				}
			}
		}
	} else if(val!=JSVAL_VOID) {
		if(JS_ValueToInt32(cx,val,&intval)) {
			if(FD_ISSET(intval, fds))
				return TRUE;
		}
	}
	return FALSE;
}

void js_timeval(JSContext* cx, jsval val, struct timeval* tv)
{
	jsdouble jsd;

	if(JSVAL_IS_INT(val))
		tv->tv_sec = JSVAL_TO_INT(val);
	else if(JSVAL_IS_DOUBLE(val)) {
		if(JS_ValueToNumber(cx,val,&jsd)) {
			tv->tv_sec = (int)jsd;
			tv->tv_usec = (int)(jsd*1000000.0)%1000000;
		}
	}
}
#endif

int js_polltimeout(JSContext* cx, jsval val)
{
	jsdouble jsd;

	if(JSVAL_IS_INT(val))
		return JSVAL_TO_INT(val) * 1000;

	if(JSVAL_IS_DOUBLE(val)) {
		if(JS_ValueToNumber(cx,val,&jsd))
			return (int)(jsd * 1000);
	}

	return 0;
}

static JSBool
js_bind(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	js_socket_private_t*	p;
	ushort		port=0;
	union xp_sockaddr	addr;
	jsrefcount	rc;
	char		*cstr=NULL;
	char		portstr[6];
	struct addrinfo	hints, *res, *tres;
	int			ret;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if((p=(js_socket_private_t*)js_GetClassPrivate(cx, obj, &js_socket_class))==NULL) {
		return(JS_FALSE);
	}

	memset(&addr,0,sizeof(addr));
	memset(&hints, 0, sizeof(hints));

	if(argc)
		port = js_port(cx,argv[0],p->type);
	if(argc > 1 && argv[1] != JSVAL_VOID) {
		JSVALUE_TO_ASTRING(cx, argv[1], cstr, INET6_ADDRSTRLEN, NULL);
	}

	hints.ai_flags = AI_ADDRCONFIG|AI_NUMERICHOST|AI_NUMERICSERV|AI_PASSIVE;
	hints.ai_socktype = p->type;

	/* We need servname to be non-NULL so we can use a NULL hostname */
	sprintf(portstr, "%hu", port);

	rc=JS_SUSPENDREQUEST(cx);
	if((ret=getaddrinfo(cstr, portstr, &hints, &res)) != 0) {
		JS_RESUMEREQUEST(cx,rc);
		dbprintf(TRUE, p, "getaddrinfo(%s, %s) failed with error %d", cstr, portstr, ret);
		p->last_error=ERROR_VALUE;
		return(JS_TRUE);
	}
	for(tres=res; tres; tres=tres->ai_next) {
		if(bind(p->sock, tres->ai_addr, tres->ai_addrlen)!=0) {
			if (tres->ai_next == NULL) {
				p->last_error=ERROR_VALUE;
				dbprintf(TRUE, p, "bind failed with error %d",ERROR_VALUE);
				freeaddrinfo(res);
				JS_RESUMEREQUEST(cx, rc);
				return(JS_TRUE);
			}
		}
		else
			break;
	}
	freeaddrinfo(res);

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
	js_socket_private_t*	p;
	int32		backlog=1;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(js_socket_private_t*)js_GetClassPrivate(cx, obj, &js_socket_class))==NULL) {
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
	js_socket_private_t*	p;
	js_socket_private_t*	new_p;
	JSObject*	sockobj;
	SOCKET		new_socket;
	socklen_t	addrlen;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(js_socket_private_t*)js_GetClassPrivate(cx, obj, &js_socket_class))==NULL) {
		return(JS_FALSE);
	}

	addrlen=sizeof(p->remote_addr);

	rc=JS_SUSPENDREQUEST(cx);
	if(p->set) {
		if((new_socket=xpms_accept(p->set,&(p->remote_addr),&addrlen,XPMS_FOREVER,XPMS_FLAGS_NONE,NULL))==INVALID_SOCKET) {
			p->last_error=ERROR_VALUE;
			dbprintf(TRUE, p, "accept failed with error %d",ERROR_VALUE);
			JS_RESUMEREQUEST(cx, rc);
			return(JS_TRUE);
		}
		call_socket_open_callback(TRUE);
	}
	else {
		if((new_socket=accept_socket(p->sock,&(p->remote_addr),&addrlen))==INVALID_SOCKET) {
			p->last_error=ERROR_VALUE;
			dbprintf(TRUE, p, "accept failed with error %d",ERROR_VALUE);
			JS_RESUMEREQUEST(cx, rc);
			return(JS_TRUE);
		}
	}

	if((sockobj=js_CreateSocketObject(cx, obj, "new_socket", new_socket, -1))==NULL) {
		closesocket(new_socket);
		JS_RESUMEREQUEST(cx, rc);
		JS_ReportError(cx,"Error creating new socket object");
		return(JS_TRUE);
	}
	if((new_p=(js_socket_private_t*)JS_GetPrivate(cx,sockobj))==NULL) {
		JS_RESUMEREQUEST(cx, rc);
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

struct js_connect_event_args {
	SOCKET sv[2];
	SOCKET sock;
	int socktype;
	char *host;
	BOOL nonblocking;
	ushort port;
};

static void
js_connect_event_thread(void *args)
{
	struct js_connect_event_args *a = args;
	struct addrinfo	hints,*res = NULL,*cur;
	int result;
	ulong val;
	char sresult;

	SetThreadName("sbbs/jsConnect");
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = a->socktype;
	hints.ai_flags = AI_ADDRCONFIG;
	result = getaddrinfo(a->host, NULL, &hints, &res);
	if(result != 0)
		goto done;
	/* always set to blocking here */
	val = 0;
	ioctlsocket(a->sock, FIONBIO, &val);
	result = SOCKET_ERROR;
	for(cur=res; cur != NULL; cur=cur->ai_next) {
		inet_setaddrport((void *)cur->ai_addr, a->port);

		result = connect(a->sock, cur->ai_addr, cur->ai_addrlen);
		if(result == 0)
			break;
	}
	sresult = result;
	/* Restore original setting here */
	ioctlsocket(a->sock,FIONBIO,(ulong*)&(a->nonblocking));
	send(a->sv[1], &sresult, 1, 0);

done:
	closesocket(a->sv[1]);
	freeaddrinfo(res);
	free(a);
}

static JSBool
js_connect_event(JSContext *cx, uintN argc, jsval *arglist, js_socket_private_t *p, ushort port, JSObject *obj)
{
	SOCKET sv[2];
	struct js_event_list *ev;
	JSFunction *ecb;
	js_callback_t *cb = js_get_callback(cx);
	struct js_connect_event_args *args;
	jsval *argv=JS_ARGV(cx, arglist);

	if (p->sock == INVALID_SOCKET) {
		JS_ReportError(cx, "invalid socket");
		return JS_FALSE;
	}

	if (cb == NULL) {
		return JS_FALSE;
	}

	if (!cb->events_supported) {
		JS_ReportError(cx, "events not supported");
		return JS_FALSE;
	}

	ecb = JS_ValueToFunction(cx, argv[2]);
	if (ecb == NULL) {
		return JS_FALSE;
	}

	// Create socket pair...
#ifdef _WIN32
	if (socketpair(AF_INET, SOCK_STREAM, 0, sv) == -1) {
#else
	if (socketpair(PF_UNIX, SOCK_STREAM, 0, sv) == -1) {
#endif
		JS_ReportError(cx, "Error %d creating socket pair", ERROR_VALUE);
		return JS_FALSE;
	}

	// Create event
	ev = malloc(sizeof(*ev));
	if (ev == NULL) {
		JS_ReportError(cx, "error allocating %lu bytes", sizeof(*ev));
		closesocket(sv[0]);
		closesocket(sv[1]);
		return JS_FALSE;
	}
	ev->prev = NULL;
	ev->next = cb->events;
	if (ev->next)
		ev->next->prev = ev;
	ev->type = JS_EVENT_SOCKET_CONNECT;
	ev->cx = obj;
	JS_AddObjectRoot(cx, &ev->cx);
	ev->cb = ecb;
	ev->data.connect.sv[0] = sv[0];
	ev->data.connect.sv[1] = sv[1];
	ev->data.connect.sock = p->sock;
	ev->id = cb->next_eid++;
	p->js_cb = cb;
	cb->events = ev;

	// Start thread
	args = malloc(sizeof(*args));
	if (args == NULL) {
		JS_ReportError(cx, "error allocating %lu bytes", sizeof(*args));
		closesocket(sv[0]);
		closesocket(sv[1]);
		return JS_FALSE;
	}
	args->sv[0] = sv[0];
	args->sv[1] = sv[1];
	args->sock = p->sock;
	args->socktype = p->type;
	args->host = strdup(p->hostname);
	args->port = port;
	args->nonblocking = p->nonblocking;
	if (args->host == NULL) {
		JS_ReportError(cx, "error duplicating hostname");
		closesocket(sv[0]);
		closesocket(sv[1]);
		free(args);
		return JS_FALSE;
	}
	_beginthread(js_connect_event_thread, 0 /* Can be smaller... */, args);

	// Success!
	p->is_connected = TRUE;
	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	return JS_TRUE;
}

static JSBool
js_connect(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	int			result;
	ulong		val;
	ushort		port;
	JSString*	str;
	js_socket_private_t*	p;
	jsrefcount	rc;
	char		ip_str[256];
	struct addrinfo	hints,*res,*cur;
	int timeout = 10000; /* Default time-out */

	if((p=(js_socket_private_t*)js_GetClassPrivate(cx, obj, &js_socket_class))==NULL) {
		return(JS_FALSE);
	}

	str = JS_ValueToString(cx, argv[0]);
	if(p->hostname)
		free(p->hostname);
	JSSTRING_TO_MSTRING(cx, str, p->hostname, NULL);
	port = js_port(cx,argv[1],p->type);
	rc=JS_SUSPENDREQUEST(cx);

	if (argc > 2 && JSVAL_IS_OBJECT(argv[2]) && JS_ObjectIsFunction(cx, JSVAL_TO_OBJECT(argv[2]))) {
		JSBool bgr = js_connect_event(cx, argc, arglist, p, port, obj);
		JS_RESUMEREQUEST(cx, rc);
		return bgr;
	}

	dbprintf(FALSE, p, "resolving hostname: %s", p->hostname);

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = p->type;
	hints.ai_flags = AI_ADDRCONFIG;
	result = getaddrinfo(p->hostname, NULL, &hints, &res);
	if(result != 0) {
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		p->last_error = ERROR_VALUE;
		dbprintf(TRUE, p, "getaddrinfo(%s) failed with error %d", p->hostname, result);
		JS_RESUMEREQUEST(cx, rc);
		return(JS_TRUE);
	}
	/* always set to nonblocking here */
	val=1;
	ioctlsocket(p->sock,FIONBIO,&val);
	result = SOCKET_ERROR;
	for(cur=res; cur != NULL; cur=cur->ai_next) {
		if(argc>2)	/* time-out value specified */
			timeout = js_polltimeout(cx, argv[2]);

		inet_addrtop((void *)cur->ai_addr, ip_str, sizeof(ip_str));
		dbprintf(FALSE, p, "connecting to %s on port %u at %s", ip_str, port, p->hostname);
		inet_setaddrport((void *)cur->ai_addr, port);

		result = connect(p->sock, cur->ai_addr, cur->ai_addrlen);

		if(result == SOCKET_ERROR) {
			result = ERROR_VALUE;
			if(result == EWOULDBLOCK || result == EINPROGRESS) {
				result = ETIMEDOUT;
				if (socket_writable(p->sock, timeout)) {
					int so_error = -1;
					socklen_t optlen = sizeof(so_error);
					if(getsockopt(p->sock, SOL_SOCKET, SO_ERROR, (void*)&so_error, &optlen) == 0 && so_error == 0)
						result = 0;	/* success */
					else
						result = so_error;
				}
			}
		}
		if(result == 0)
			break;
	}
	/* Restore original setting here */
	ioctlsocket(p->sock,FIONBIO,(ulong*)&(p->nonblocking));

	if(result!=0) {
		freeaddrinfo(res);
		p->last_error = result;
		dbprintf(TRUE, p, "connect failed with error %d", result);
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		JS_RESUMEREQUEST(cx, rc);
		return(JS_TRUE);
	}
	memcpy(&p->remote_addr, cur->ai_addr, cur->ai_addrlen);
	freeaddrinfo(res);

	p->is_connected = TRUE;
	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	dbprintf(FALSE, p, "connected to %s on port %u at %s", ip_str, port, p->hostname);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_send(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char*		cp = NULL;
	size_t		len;
	JSString*	str;
	js_socket_private_t*	p;
	jsrefcount	rc;
	int		ret;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(js_socket_private_t*)js_GetClassPrivate(cx, obj, &js_socket_class))==NULL) {
		return(JS_FALSE);
	}

	str = JS_ValueToString(cx, argv[0]);
	JSSTRING_TO_MSTRING(cx, str, cp, &len);
	HANDLE_PENDING(cx, cp);
	if(cp==NULL)
		return JS_TRUE;

	rc=JS_SUSPENDREQUEST(cx);
	ret = js_socket_sendsocket(p,cp,len,TRUE);
	if(ret >= 0) {
		dbprintf(FALSE, p, "sent %d of %lu bytes",ret,len);
		JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(ret));
	} else {
		p->last_error=ERROR_VALUE;
		dbprintf(TRUE, p, "send of %lu bytes failed",len);
	}
	free(cp);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_sendline(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char*		cp = NULL;
	size_t		len;
	JSString*	str;
	js_socket_private_t*	p;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(js_socket_private_t*)js_GetClassPrivate(cx, obj, &js_socket_class))==NULL) {
		return(JS_FALSE);
	}

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	str = JS_ValueToString(cx, argv[0]);
	JSSTRING_TO_MSTRING(cx, str, cp, &len);
	HANDLE_PENDING(cx, cp);
	if(cp==NULL)
		return JS_TRUE;

	rc=JS_SUSPENDREQUEST(cx);
	if(js_socket_sendsocket(p,cp,len,FALSE)==len && js_socket_sendsocket(p,"\r\n",2,TRUE)==2) {
		dbprintf(FALSE, p, "sent %lu bytes",len+2);
		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	} else {
		p->last_error=ERROR_VALUE;
		dbprintf(TRUE, p, "send of %lu bytes failed",len+2);
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
	char*		cp = NULL;
	size_t		len;
	ushort		port;
	JSString*	data_str;
	JSString*	ip_str;
	js_socket_private_t*	p;
	jsrefcount	rc;
	struct addrinfo hints,*res,*cur;
	int			result;
	char		ip_addr[INET6_ADDRSTRLEN];

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(js_socket_private_t*)js_GetClassPrivate(cx, obj, &js_socket_class))==NULL) {
		return(JS_FALSE);
	}

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	/* data */
	data_str = JS_ValueToString(cx, argv[0]);
	JSSTRING_TO_MSTRING(cx, data_str, cp, &len);
	HANDLE_PENDING(cx, cp);
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
	port = js_port(cx,argv[2],p->type);
	rc=JS_SUSPENDREQUEST(cx);

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = p->type;
	hints.ai_flags = AI_ADDRCONFIG;
	dbprintf(FALSE, p, "resolving hostname: %s", p->hostname);

	if((result=getaddrinfo(p->hostname, NULL, &hints, &res) != 0)) {
		p->last_error = ERROR_VALUE;
		dbprintf(TRUE, p, "getaddrinfo(%s) failed with error %d", p->hostname, result);
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		free(cp);
		JS_RESUMEREQUEST(cx, rc);
		return(JS_TRUE);
	}

	for(cur=res; cur; cur=cur->ai_next) {
		inet_addrtop((void *)cur->ai_addr, ip_addr, sizeof(ip_addr));
		dbprintf(FALSE, p, "sending %lu bytes to %s port %u at %s"
			,len, ip_addr, port, p->hostname);
		inet_setaddrport((void *)cur->ai_addr, port);
		if(sendto(p->sock,cp,len,0 /* flags */,cur->ai_addr,cur->ai_addrlen)==len) {
			dbprintf(FALSE, p, "sent %lu bytes",len);
			JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
		} else {
			p->last_error=ERROR_VALUE;
			dbprintf(TRUE, p, "send of %lu bytes failed to %s",len, ip_addr);
		}
	}
	free(cp);
	freeaddrinfo(res);
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}


static JSBool
js_sendfile(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char*		fname = NULL;
	off_t		len;
	int			file;
	js_socket_private_t*	p;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(js_socket_private_t*)js_GetClassPrivate(cx, obj, &js_socket_class))==NULL) {
		return(JS_FALSE);
	}

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	JSVALUE_TO_MSTRING(cx, argv[0], fname, NULL);
	HANDLE_PENDING(cx, fname);
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

	len = js_socket_sendfilesocket(p, file, NULL, 0);
	close(file);
	if(len > 0) {
		dbprintf(FALSE, p, "sent %"PRIdOFF" bytes",len);
		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	} else {
		p->last_error=ERROR_VALUE;
		dbprintf(TRUE, p, "send of %s failed",fname);
	}
	free(fname);

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
	js_socket_private_t*	p;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if((p=(js_socket_private_t*)js_GetClassPrivate(cx, obj, &js_socket_class))==NULL) {
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
	int32		timeout=120;
	JSString*	str;
	jsrefcount	rc;
	js_socket_private_t*	p;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(js_socket_private_t*)js_GetClassPrivate(cx, obj, &js_socket_class))==NULL) {
		return(JS_FALSE);
	}

	if(argc && argv[0]!=JSVAL_VOID) {
		JS_ValueToInt32(cx,argv[0],&len);

		if(argc > 1 && argv[1]!=JSVAL_VOID) {
			JS_ValueToInt32(cx,argv[1],&timeout);
		}
	}

	if((buf=(char*)malloc(len+1))==NULL) {
		JS_ReportError(cx,"Error allocating %u bytes",len+1);
		return(JS_FALSE);
	}

	rc=JS_SUSPENDREQUEST(cx);
	len = js_socket_recv(cx,p,buf,len,0,timeout);
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
	char		ip_addr[INET6_ADDRSTRLEN];
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
	union xp_sockaddr	addr;
	socklen_t	addrlen;
	jsrefcount	rc;
	js_socket_private_t*	p;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(js_socket_private_t*)js_GetClassPrivate(cx, obj, &js_socket_class))==NULL) {
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

	addrlen=sizeof(addr.addr);

	if(binary) {	/* Binary/Integer Data */

		rc=JS_SUSPENDREQUEST(cx);
		switch(len) {
			case sizeof(BYTE):
				if((rd=recvfrom(p->sock,&b,len,0,&addr.addr,&addrlen))==len)
					data_val = INT_TO_JSVAL(b);
				break;
			case sizeof(WORD):
				if((rd=recvfrom(p->sock,(BYTE*)&w,len,0,&addr.addr,&addrlen))==len) {
					if(p->network_byte_order)
						w=ntohs(w);
					data_val = INT_TO_JSVAL(w);
				}
				break;
			default:
			case sizeof(DWORD):
				if((rd=recvfrom(p->sock,(BYTE*)&l,len,0,&addr.addr,&addrlen))==len) {
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
		len = recvfrom(p->sock,buf,len,0,&addr.addr,&addrlen);
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

	sprintf(port,"%u",inet_addrport(&addr));
	if((str=JS_NewStringCopyZ(cx,port))==NULL)
		return(JS_FALSE);
	JS_DefineProperty(cx, retobj, "port"
		,STRING_TO_JSVAL(str)
		,NULL,NULL,JSPROP_ENUMERATE);

	inet_addrtop(&addr, ip_addr, sizeof(ip_addr));
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
	js_socket_private_t*	p;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(js_socket_private_t*)js_GetClassPrivate(cx, obj, &js_socket_class))==NULL) {
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
		len = js_socket_recv(cx, p,buf,len,MSG_PEEK,120);
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

/* Returns 0 if there is rx data waiting */
/* Returns 1 if the 'timeout' period has elapsed (with no data waiting) */
/* Returns 2 if the socket has been disconnected (regardless of any data waiting) */
/* Returns 3 if there was no rx data waiting and the 'timeout' period has not yet elapsed */
static int
js_sock_read_check(js_socket_private_t *p, time_t start, int32 timeout, int i)
{
	BOOL		rd;

	if(timeout > 0 && time(NULL)-start>timeout) {
		dbprintf(FALSE, p, "recvline timeout (received: %d)",i);
		return 1;
	}

	if(!socket_check(p->sock,&rd,NULL,1000)) {
		p->last_error=ERROR_VALUE;
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

/* This method is to return null on error/timeout, not void/undefined */
static JSBool
js_recvline(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char		ch;
	char*		buf;
	int			i,got;
	int32		len=512;
	time_t		start;
	int32		timeout=30;	/* seconds */
	JSString*	str;
	js_socket_private_t*	p;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	if((p=(js_socket_private_t*)js_GetClassPrivate(cx, obj, &js_socket_class))==NULL) {
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
				case 1:	// time-out */
				case 2: // disconnected
					if(i) {		// some data was received before the error/disconnection
						len=0;	// so break the loop
						continue;
					}
					// no data received, so just return null
					JS_RESUMEREQUEST(cx, rc);
					free(buf);
					return JS_TRUE;
				case 3:	// no data and no time-out... yet
					continue;
			}
		}

		if((got=js_socket_recv(cx, p, &ch, 1, 0, i?1:timeout))!=1) {
			if(p->session == -1)
				p->last_error = ERROR_VALUE;
			if (i == 0) {			// no data received
				JS_RESUMEREQUEST(cx, rc);
				free(buf);			// so return null (not an empty string)
				return(JS_TRUE);
			}
			break;
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
	js_socket_private_t*	p;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(-1));

	if((p=(js_socket_private_t*)js_GetClassPrivate(cx, obj, &js_socket_class))==NULL) {
		return(JS_FALSE);
	}

	if(argc && argv[0]!=JSVAL_VOID)
		JS_ValueToInt32(cx,argv[0],&size);

	rc=JS_SUSPENDREQUEST(cx);
	switch(size) {
		case sizeof(BYTE):
			if((rd=js_socket_recv(cx, p,&b,size,MSG_WAITALL,120))==size)
				JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(b));
			break;
		case sizeof(WORD):
			if((rd=js_socket_recv(cx, p,(BYTE*)&w,size,MSG_WAITALL,120))==size) {
				if(p->network_byte_order)
					w=ntohs(w);
				JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(w));
			}
			break;
		case sizeof(DWORD):
			if((rd=js_socket_recv(cx, p,(BYTE*)&l,size,MSG_WAITALL,120))==size) {
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
	int			level = 0;
	int			val = 0;
	js_socket_private_t*	p;
	LINGER		linger;
	void*		vp=&val;
	socklen_t	len=sizeof(val);
	jsrefcount	rc;
	char		*cstr;

	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(-1));

	if((p=(js_socket_private_t*)js_GetClassPrivate(cx, obj, &js_socket_class))==NULL) {
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
	js_socket_private_t*	p;
	LINGER		linger;
	void*		vp=&val;
	socklen_t	len=sizeof(val);
	jsrefcount	rc;
	char		*optname;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(js_socket_private_t*)js_GetClassPrivate(cx, obj, &js_socket_class))==NULL) {
		return(JS_FALSE);
	}
	if (p->sock == INVALID_SOCKET)
		return JS_TRUE;

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
	js_socket_private_t*	p;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(js_socket_private_t*)js_GetClassPrivate(cx, obj, &js_socket_class))==NULL) {
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
	js_socket_private_t*	p;
	BOOL	poll_for_write=FALSE;
	uintN	argn;
	int		result;
	jsrefcount	rc;
#ifdef PREFER_POLL
	int timeout = 0;
	struct pollfd *fds;
	nfds_t nfds;
	jsval objval = OBJECT_TO_JSVAL(obj);
#else
	size_t	i;
	SOCKET	high=0;
	fd_set  socket_set;
	fd_set* rd_set=NULL;
	fd_set* wr_set=NULL;
	struct  timeval tv = {0, 0};
#endif

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(js_socket_private_t*)js_GetClassPrivate(cx, obj, &js_socket_class))==NULL) {
		return(JS_FALSE);
	}

	if(p->sock==INVALID_SOCKET && p->set == NULL) {
		dbprintf(TRUE, p, "INVALID SOCKET in call to poll");
		JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(-1));
		return(JS_TRUE);
	}

	for(argn=0;argn<argc;argn++) {
		if(JSVAL_IS_BOOLEAN(argv[argn]))
			poll_for_write=JSVAL_TO_BOOLEAN(argv[argn]);
		else if(JSVAL_IS_NUMBER(argv[argn])) {
#ifdef PREFER_POLL
			timeout = js_polltimeout(cx, argv[argn]);
#else
			js_timeval(cx,argv[argn],&tv);
#endif
		}
	}

	rc=JS_SUSPENDREQUEST(cx);
#ifdef PREFER_POLL
	if (p->peeked && !poll_for_write) {
		result = 1;
	}
	else {
		nfds = js_socket_numsocks(cx, objval);
		fds = calloc(nfds, sizeof(*fds));
		if (fds == NULL) {
			JS_RESUMEREQUEST(cx, rc);
			JS_ReportError(cx, "Error allocating %d elements of %lu bytes at %s:%d"
				, nfds, sizeof(*fds), getfname(__FILE__), __LINE__);
			return JS_FALSE;
		}
		nfds = js_socket_add(cx, objval, fds, poll_for_write ? POLLOUT : POLLIN);
		result = poll(fds, nfds, timeout);
		free(fds);
	}
#else
	FD_ZERO(&socket_set);
	if(p->set) {
		for(i=0; i<p->set->sock_count; i++) {
			FD_SET(p->set->socks[i].sock,&socket_set);
			if(p->set->socks[i].sock > high)
				high = p->set->socks[i].sock;
		}
	}
	else {
		high=p->sock;
		FD_SET(p->sock,&socket_set);
	}
	if(poll_for_write)
		wr_set=&socket_set;
	else
		rd_set=&socket_set;

	if (p->peeked && !poll_for_write)
		result = 1;
	else
		result = select(high+1,rd_set,wr_set,NULL,&tv);
#endif

	p->last_error=ERROR_VALUE;

	dbprintf(FALSE, p, "poll: select/poll returned %d (errno %d)"
		,result,p->last_error);

	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(result));
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static js_callback_t *
js_get_callback(JSContext *cx)
{
	JSObject*	scope = JS_GetScopeChain(cx);
	JSObject*	pscope;
	jsval val = JSVAL_NULL;
	JSObject *pjs_obj;

	pscope = scope;
	while ((!JS_LookupProperty(cx, pscope, "js", &val) || val==JSVAL_VOID || !JSVAL_IS_OBJECT(val)) && pscope != NULL) {
		pscope = JS_GetParent(cx, pscope);
		if (pscope == NULL) {
			JS_ReportError(cx, "Walked to global, no js object!");
			return NULL;
		}
	}
	pjs_obj = JSVAL_TO_OBJECT(val);
	return JS_GetPrivate(cx, pjs_obj);
}

static void
js_install_one_socket_event(JSContext *cx, JSObject *obj, JSFunction *ecb, js_callback_t *cb, js_socket_private_t *p, SOCKET sock, enum js_event_type et)
{
	struct js_event_list *ev;

	ev = malloc(sizeof(*ev));
	if (ev == NULL) {
		JS_ReportError(cx, "error allocating %lu bytes", sizeof(*ev));
		return;
	}
	ev->prev = NULL;
	ev->next = cb->events;
	if (ev->next)
		ev->next->prev = ev;
	ev->type = et;
	ev->cx = obj;
	JS_AddObjectRoot(cx, &ev->cx);
	ev->cb = ecb;
	ev->data.sock = sock;
	ev->id = cb->next_eid;
	p->js_cb = cb;
	cb->events = ev;
}

static JSBool
js_install_event(JSContext *cx, uintN argc, jsval *arglist, BOOL once)
{
	jsval	*argv=JS_ARGV(cx, arglist);
	js_callback_t*	cb;
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	JSFunction *ecb;
	js_socket_private_t*	p;
	size_t i;
	char operation[16];
	enum js_event_type et;
	size_t slen;

	if((p=(js_socket_private_t*)js_GetClassPrivate(cx, obj, &js_socket_class))==NULL) {
		return(JS_FALSE);
	}

	/*
	 * NOTE: If you allow a thisObj here, you'll need to deal with js_GetClassPrivate
	 *       in js_internal.c where the object is assumed to be a socket.
	 */
	if (argc != 2) {
		JS_ReportError(cx, "js.on() and js.once() require exactly two parameters");
		return JS_FALSE;
	}
	ecb = JS_ValueToFunction(cx, argv[1]);
	if (ecb == NULL) {
		return JS_FALSE;
	}
	JSVALUE_TO_STRBUF(cx, argv[0], operation, sizeof(operation), &slen);
	HANDLE_PENDING(cx, NULL);
	if (strcmp(operation, "read") == 0) {
		if (once)
			et = JS_EVENT_SOCKET_READABLE_ONCE;
		else
			et = JS_EVENT_SOCKET_READABLE;
	}
	else if (strcmp(operation, "write") == 0) {
		if (once)
			et = JS_EVENT_SOCKET_WRITABLE_ONCE;
		else
			et = JS_EVENT_SOCKET_WRITABLE;
	}
	else {
		JS_ReportError(cx, "event parameter must be 'read' or 'write'");
		return JS_FALSE;
	}

	cb = js_get_callback(cx);
	if (cb == NULL) {
		return JS_FALSE;
	}
	if (!cb->events_supported) {
		JS_ReportError(cx, "events not supported");
		return JS_FALSE;
	}


	if (p->set) {
		for (i = 0; i < p->set->sock_count; i++) {
			if (p->set->socks[i].sock != INVALID_SOCKET)
				js_install_one_socket_event(cx, obj, ecb, cb, p, p->set->socks[i].sock, et);
		}
	}
	else {
		if (p->sock != INVALID_SOCKET)
			js_install_one_socket_event(cx, obj, ecb, cb, p, p->sock, et);
	}
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(cb->next_eid));
	cb->next_eid++;

	if (JS_IsExceptionPending(cx))
		return JS_FALSE;
	return JS_TRUE;
}

static JSBool
js_clear_socket_event(JSContext *cx, uintN argc, jsval *arglist, BOOL once)
{
	jsval	*argv=JS_ARGV(cx, arglist);
	enum js_event_type et;
	char operation[16];
	size_t slen;
	js_callback_t*	cb;

	if (argc != 2) {
		JS_ReportError(cx, "js.clearOn() and js.clearOnce() require exactly two parameters");
		return JS_FALSE;
	}
	JSVALUE_TO_STRBUF(cx, argv[0], operation, sizeof(operation), &slen);
	HANDLE_PENDING(cx, NULL);
	if (strcmp(operation, "read") == 0) {
		if (once)
			et = JS_EVENT_SOCKET_READABLE_ONCE;
		else
			et = JS_EVENT_SOCKET_READABLE;
	}
	else if (strcmp(operation, "write") == 0) {
		if (once)
			et = JS_EVENT_SOCKET_WRITABLE_ONCE;
		else
			et = JS_EVENT_SOCKET_WRITABLE;
	}
	else {
		JS_SET_RVAL(cx, arglist, JSVAL_VOID);
		return JS_TRUE;
	}

	cb = js_get_callback(cx);
	if (cb == NULL) {
		return JS_FALSE;
	}
	return js_clear_event(cx, arglist, cb, et, 1);
}

static JSBool
js_once(JSContext *cx, uintN argc, jsval *arglist)
{
	return js_install_event(cx, argc, arglist, TRUE);
}

static JSBool
js_clearOnce(JSContext *cx, uintN argc, jsval *arglist)
{
	return js_clear_socket_event(cx, argc, arglist, TRUE);
}

static JSBool
js_on(JSContext *cx, uintN argc, jsval *arglist)
{
	return js_install_event(cx, argc, arglist, FALSE);
}

static JSBool
js_clearOn(JSContext *cx, uintN argc, jsval *arglist)
{
	return js_clear_socket_event(cx, argc, arglist, FALSE);
}

/* Socket Object Properties */
enum {
	 SOCK_PROP_LAST_ERROR
	,SOCK_PROP_ERROR_STR
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
	,SOCK_PROP_FAMILY
	,SOCK_PROP_NETWORK_ORDER
	,SOCK_PROP_SSL_SESSION
	,SOCK_PROP_SSL_SERVER

};

#ifdef BUILD_JSDOCS
static char* socket_prop_desc[] = {
	/* Regular properties */
	 "error status for the last socket operation that failed - <small>READ ONLY</small>"
	,"error description for the last socket operation that failed - <small>READ ONLY</small>"
	,"<i>true</i> if socket is in a connected state - <small>READ ONLY</small>"
	,"<i>true</i> if socket can accept written data - Setting to false will shutdown the write end of the socket."
	,"alias for is_writeable"
	,"<i>true</i> if data is waiting to be read from socket - <small>READ ONLY</small>"
	,"number of bytes waiting to be read - TLS sockets will never return more than 1 - <small>READ ONLY</small>"
	,"enable debug logging"
	,"socket descriptor (advanced uses only)"
	,"use non-blocking operation (default is <i>false</i>)"
	,"local IP address (string in dotted-decimal format)"
	,"local TCP or UDP port number"
	,"remote IP address (string in dotted-decimal format)"
	,"remote TCP or UDP port number"
	,"socket type, <tt>SOCK_STREAM</tt> (TCP) or <tt>SOCK_DGRAM</tt> (UDP)"
	,"socket protocol family, <tt>PF_INET</tt> (IPv4) or <tt>PF_INET6</tt> (IPv6)"
	,"<i>true</i> if binary data is to be sent in Network Byte Order (big end first), default is <i>true</i>"
	,"set to <i>true</i> to enable SSL as a client on the socket"
	,"set to <i>true</i> to enable SSL as a server on the socket"

	/* statically-defined properties: */
	,"array of socket option names supported by the current platform"
	,NULL
};
#endif

static JSBool js_socket_set(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
	jsval idval;
	jsint       tiny;
	js_socket_private_t*	p;
	jsrefcount	rc;
	BOOL		b;
	int32		i;
	scfg_t *scfg;
	char* estr;
	int level;

	if((p=(js_socket_private_t*)JS_GetPrivate(cx,obj))==NULL) {
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
		case SOCK_PROP_SSL_SERVER:
		case SOCK_PROP_SSL_SESSION:
			JS_ValueToBoolean(cx,*vp,&b);
			rc=JS_SUSPENDREQUEST(cx);
			if(b) {
				if(p->session==-1) {
					int ret = CRYPT_ERROR_NOTINITED;

					if(do_cryptInit()) {
						if((ret=cryptCreateSession(&p->session, CRYPT_UNUSED, tiny == SOCK_PROP_SSL_SESSION ? CRYPT_SESSION_SSL: CRYPT_SESSION_SSL_SERVER))==CRYPT_OK) {
							ulong nb=0;
							ioctlsocket(p->sock,FIONBIO,&nb);
							nb=1;
							setsockopt(p->sock,IPPROTO_TCP,TCP_NODELAY,(char*)&nb,sizeof(nb));
							if((ret=do_cryptAttribute(p->session, CRYPT_SESSINFO_NETWORKSOCKET, p->sock))==CRYPT_OK) {
								// Reduced compliance checking... required for acme-staging-v02.api.letsencrypt.org
								do_cryptAttribute(p->session, CRYPT_OPTION_CERT_COMPLIANCELEVEL, CRYPT_COMPLIANCELEVEL_REDUCED);
								if (tiny == SOCK_PROP_SSL_SESSION) {
									// TODO: Make this configurable
									do_cryptAttribute(p->session, CRYPT_SESSINFO_SSL_OPTIONS, CRYPT_SSLOPTION_DISABLE_NAMEVERIFY);
									ret=do_cryptAttributeString(p->session, CRYPT_SESSINFO_SERVER_NAME, p->hostname, strlen(p->hostname));
									p->tls_server = FALSE;
								}
								else {
									scfg = JS_GetRuntimePrivate(JS_GetRuntime(cx));

									if (scfg == NULL) {
										ret = CRYPT_ERROR_NOTAVAIL;
									}
									else {
										if (get_ssl_cert(scfg, &estr, &level) == -1) {
											if (estr) {
												lprintf(level, "%04d %s", p->sock, estr);
												free_crypt_attrstr(estr);
											}
										}
										if (scfg->tls_certificate == -1)
											ret = CRYPT_ERROR_NOTAVAIL;
										else {
											lock_ssl_cert();
											ret = cryptSetAttribute(p->session, CRYPT_SESSINFO_PRIVATEKEY, scfg->tls_certificate);
											if (ret != CRYPT_OK) {
												unlock_ssl_cert();
												GCES(ret, p, estr, "setting private key");
											}
										}
									}
								}
								if(ret==CRYPT_OK) {
									if((ret=do_cryptAttribute(p->session, CRYPT_SESSINFO_ACTIVE, 1))!=CRYPT_OK) {
										GCES(ret, p, estr, "setting session active");
									}
									if (tiny == SOCK_PROP_SSL_SERVER)
										unlock_ssl_cert();
								}
							}
						}
						else
							GCESH(ret, p->sock, CRYPT_UNUSED, estr, "creating session");
					}
					if (ret != CRYPT_OK) {
						if (p->session != -1)
							cryptDestroySession(p->session);
						p->session=-1;
						ioctlsocket(p->sock,FIONBIO,(ulong*)&(p->nonblocking));
						do_js_close(cx, p, false);
					}
				}
			}
			else {
				if(p->session != -1) {
					cryptDestroySession(p->session);
					p->session=-1;
					ioctlsocket(p->sock,FIONBIO,(ulong*)&(p->nonblocking));
					do_js_close(cx, p, false);
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
	js_socket_private_t*	p;
	JSString*	js_str;
	union xp_sockaddr	addr;
	socklen_t	len=sizeof(addr);
	jsrefcount	rc;
	char		str[256];

	if((p=(js_socket_private_t*)JS_GetPrivate(cx,obj))==NULL) {
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
		case SOCK_PROP_ERROR_STR:
			if((js_str=JS_NewStringCopyZ(cx, socket_strerror(p->last_error, str, sizeof(str))))==NULL)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(js_str);
			break;
		case SOCK_PROP_IS_CONNECTED:
			if(!p->is_connected)
				*vp = JSVAL_FALSE;
			else
				*vp = BOOLEAN_TO_JSVAL(socket_check(p->sock,NULL,NULL,0));
			break;
		case SOCK_PROP_IS_WRITEABLE:
			if(p->sock==INVALID_SOCKET && p->set)
				wr = FALSE;
			else
				socket_check(p->sock,NULL,&wr,0);
			*vp = BOOLEAN_TO_JSVAL(wr);
			break;
		case SOCK_PROP_DATA_WAITING:
			if(p->sock==INVALID_SOCKET && p->set)
				rd = FALSE;
			else {
				if (p->peeked)
					rd = TRUE;
				else if (p->session != -1)
					rd = js_socket_peek_byte(cx, p);
				else
					socket_check(p->sock,&rd,NULL,0);
			}
			*vp = BOOLEAN_TO_JSVAL(rd);
			break;
		case SOCK_PROP_NREAD:
			if(p->sock==INVALID_SOCKET && p->set) {
				*vp = JSVAL_ZERO;
				break;
			}
			cnt=0;
			if (p->session != -1) {
				if (js_socket_peek_byte(cx, p))
					*vp=DOUBLE_TO_JSVAL((double)1);
				else
					*vp = JSVAL_ZERO;
			}
			else if(ioctlsocket(p->sock, FIONREAD, &cnt)==0) {
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
				inet_addrtop(&addr, str, sizeof(str));
				if((js_str=JS_NewStringCopyZ(cx,str))==NULL)
					return(JS_FALSE);
				*vp = STRING_TO_JSVAL(js_str);
				rc=JS_SUSPENDREQUEST(cx);
			}
			else
				*vp=JSVAL_VOID;
			break;
		case SOCK_PROP_LOCAL_PORT:
			if(p->local_port != 0) {
				*vp = INT_TO_JSVAL(p->local_port);
			} else if(p->sock != INVALID_SOCKET) {
				if(getsockname(p->sock, &addr.addr,&len)!=0)
					return(JS_FALSE);
				JS_RESUMEREQUEST(cx, rc);
				*vp = INT_TO_JSVAL(inet_addrport(&addr));
				rc=JS_SUSPENDREQUEST(cx);
			}
			else
				*vp=JSVAL_VOID;
			break;
		case SOCK_PROP_REMOTE_IP:
			if(p->is_connected) {
				JS_RESUMEREQUEST(cx, rc);
				inet_addrtop(&p->remote_addr, str, sizeof(str));
				if((js_str=JS_NewStringCopyZ(cx,str))==NULL)
					return(JS_FALSE);
				*vp = STRING_TO_JSVAL(js_str);
				rc=JS_SUSPENDREQUEST(cx);
			}
			else
				*vp=JSVAL_VOID;
			break;
		case SOCK_PROP_REMOTE_PORT:
			if(p->is_connected)
				*vp = INT_TO_JSVAL(inet_addrport(&p->remote_addr));
			else
				*vp=JSVAL_VOID;
			break;
		case SOCK_PROP_TYPE:
			*vp = INT_TO_JSVAL(p->type);
			break;
		case SOCK_PROP_FAMILY:
			if(p->sock != INVALID_SOCKET) {
				if(getsockname(p->sock, &addr.addr, &len)!=0)
					return(JS_FALSE);
				JS_RESUMEREQUEST(cx, rc);
				*vp = INT_TO_JSVAL(addr.addr.sa_family);
				rc=JS_SUSPENDREQUEST(cx);
			}
			else
				*vp=JSVAL_VOID;
			break;
		case SOCK_PROP_NETWORK_ORDER:
			*vp = BOOLEAN_TO_JSVAL(p->network_byte_order);
			break;
		case SOCK_PROP_SSL_SESSION:
			*vp = BOOLEAN_TO_JSVAL(p->session != -1);
			break;
		case SOCK_PROP_SSL_SERVER:
			*vp = BOOLEAN_TO_JSVAL(p->session != -1 && p->tls_server);
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
	{	"error_str"			,SOCK_PROP_ERROR_STR	,SOCK_PROP_FLAGS,	318 },
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
	{	"family"			,SOCK_PROP_FAMILY		,SOCK_PROP_FLAGS,	318 },
	{	"network_byte_order",SOCK_PROP_NETWORK_ORDER,JSPROP_ENUMERATE,	311 },
	{	"ssl_session"		,SOCK_PROP_SSL_SESSION	,JSPROP_ENUMERATE,	316	},
	{	"ssl_server"		,SOCK_PROP_SSL_SERVER	,JSPROP_ENUMERATE,	316	},
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
	{"send",		js_send,		1,	JSTYPE_NUMBER,	JSDOCSTR("data")
	,JSDOCSTR("send a string (AKA write).  Returns the number of bytes sent or undefined if an error occured.  "
	"Versions before 3.17 returned a bool true if all bytes were sent and false otherwise.")
	,310
	},
	{"writeln",		js_sendline,		1,	JSTYPE_ALIAS },
	{"sendline",	js_sendline,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("data")
	,JSDOCSTR("send a string (AKA write) with a carriage return line feed appended")
	,317
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
	{"recv",		js_recv,		1,	JSTYPE_STRING,	JSDOCSTR("[maxlen=<tt>512</tt>, [timeout_sec=<tt>120</tt>]]")
	,JSDOCSTR("receive a string, default maxlen is 512 characters (AKA read)")
	,310
	},
	{"peek",		js_peek,		0,	JSTYPE_STRING,	JSDOCSTR("[maxlen=<tt>512</tt>]")
	,JSDOCSTR("receive a string, default maxlen is 512 characters, leaves string in receive buffer (TLS sockets will never return more than one byte)")
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
	{"on",		js_on,		2,	JSTYPE_NUMBER,	JSDOCSTR("('read' | 'write'), callback")
	,JSDOCSTR("execute callback whenever socket is readable/writable.  Returns an id to be passed to js.clearOn()")
	,31900
	},
	{"once",	js_once,	2,	JSTYPE_NUMBER,	JSDOCSTR("('read' | 'write'), callback")
	,JSDOCSTR("execute callback next time socket is readable/writable  Returns and id to be passed to js.clearOnce()")
	,31900
	},
	{"clearOn",	js_clearOn,	2,	JSTYPE_NUMBER,	JSDOCSTR("('read' | 'write'), id")
	,JSDOCSTR("remove callback installed by Socket.on()")
	,31900
	},
	{"clearOnce",	js_clearOnce,	2,	JSTYPE_NUMBER,	JSDOCSTR("('read' | 'write'), id")
	,JSDOCSTR("remove callback installed by Socket.once()")
	,31900
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
			HANDLE_PENDING(cx, name);
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

JSClass js_socket_class = {
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

JSClass js_connected_socket_class = {
     "ConnectedSocket"				/* name			*/
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

JSClass js_listening_socket_class = {
     "ListeningSocket"				/* name			*/
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

JSObject* js_CreateSocketObjectWithoutParent(JSContext* cx, SOCKET sock, CRYPT_CONTEXT session)
{
	JSObject*	obj;
	js_socket_private_t*	p;
	int			type=SOCK_STREAM;
	socklen_t	len;

	obj=JS_NewObject(cx, &js_socket_class, NULL, NULL);

	if(obj==NULL)
		return(NULL);

	len = sizeof(type);
	getsockopt(sock,SOL_SOCKET,SO_TYPE,(void*)&type,&len);

	if(!js_DefineSocketOptionsArray(cx, obj, type))
		return(NULL);

	if((p=(js_socket_private_t*)malloc(sizeof(js_socket_private_t)))==NULL)
		return(NULL);
	memset(p,0,sizeof(js_socket_private_t));

	p->sock = sock;
	p->external = TRUE;
	p->network_byte_order = TRUE;
	p->session=session;
	p->unflushed = 0;

	if (p->sock != INVALID_SOCKET) {
		len=sizeof(p->remote_addr);
		if(getpeername(p->sock, &p->remote_addr.addr,&len)==0)
			p->is_connected=TRUE;
	}

	if(!JS_SetPrivate(cx, obj, p)) {
		dbprintf(TRUE, p, "JS_SetPrivate failed");
		return(NULL);
	}

	dbprintf(FALSE, p, "object created");

	return(obj);
}

static BOOL
handle_addrs(char *host, struct sockaddr_in *addr4, socklen_t *addr4len, struct sockaddr_in6 *addr6, socklen_t *addr6len)
{
	in_addr_t ia;
	union xp_sockaddr ia6;
	char *p, *p2;
	struct addrinfo	hints;
	struct addrinfo	*res=NULL;
	struct addrinfo	*cur;
	int i;

	// First, clean up for host:port style...
	p = strrchr(host, ':');
	/*
	 * If there isn't a [, and the first and last colons aren't the same
	 * it's assumed to be an IPv6 address
	 */
	if(host[0] != '[' && p != NULL && strchr(host, ':') != p)
		p=NULL;
	if(host[0]=='[') {
		host++;
		p2=strrchr(host,']');
		if(p2)
			*p2=0;
		if(p2 > p)
			p=NULL;
	}
	if(p!=NULL)
		*p=0;

	ia = inet_addr(host);
	if (ia != INADDR_NONE) {
		if (*addr4len == 0) {
			addr4->sin_addr.s_addr = ia;
			*addr4len = sizeof(struct sockaddr_in);
		}
		return TRUE;
	}

	if (inet_ptoaddr(host, &ia6, sizeof(ia6)) != NULL) {
		if (*addr6len == 0) {
			addr6->sin6_addr = ia6.in6.sin6_addr;
			*addr6len = sizeof(struct sockaddr_in6);
		}
		return TRUE;
	}

	// So it's a hostname... resolve it into addr4 and addr6 if possible.
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_flags = AI_ADDRCONFIG | AI_PASSIVE;
	if((i = getaddrinfo(host, NULL, &hints, &res)) != 0)
		return FALSE;
	for(cur=res; cur && (*addr4len == 0 || *addr6len == 0); cur=cur->ai_next) {
		switch (cur->ai_family) {
			case AF_INET:
				if (*addr4len == 0) {
					addr4->sin_addr = ((struct sockaddr_in *)cur->ai_addr)->sin_addr;
					*addr4len = sizeof(struct sockaddr_in);
				}
				break;
			case AF_INET6:
				if (*addr6len == 0) {
					addr6->sin6_addr = ((struct sockaddr_in6 *)cur->ai_addr)->sin6_addr;
					*addr6len = sizeof(struct sockaddr_in6);
				}
				break;
		}
	}
	freeaddrinfo(res);

	return TRUE;
}

static JSBool
js_connected_socket_constructor(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj;
	JSObject *ao;
	jsval *argv=JS_ARGV(cx, arglist);
	int32	type=SOCK_STREAM;	/* default = TCP */
	int32		domain = PF_UNSPEC; /* default = IPvAny */
	int32		proto = IPPROTO_IP;
	char*	protocol=NULL;
	uintN	i;
	js_socket_private_t* p = NULL;
	jsval v;
	struct addrinfo	hints;
	struct addrinfo	*res=NULL;
	struct addrinfo	*cur;
	char *host = NULL;
	uint16_t port;
	char	pstr[6];
	jsrefcount	rc;
	int nonblock;
	scfg_t *scfg;
	char error[256];
	uint16_t bindport = 0;
	struct sockaddr_in addr4;
	struct sockaddr_in6 addr6;
	socklen_t addr4len;
	socklen_t addr6len;
	struct sockaddr *addr;
	socklen_t *addrlen;
	BOOL sockbind = FALSE;
	jsuint count;
	int32 timeout = 10;

	scfg = JS_GetRuntimePrivate(JS_GetRuntime(cx));
	if (scfg == NULL) {
		JS_ReportError(cx, "Unable to get private runtime");
		return JS_FALSE;
	}

	if (argc < 2) {
			JS_ReportError(cx, "At least two arguments required (hostname and port)");
			return JS_FALSE;
	}
	// Optional arguments in an object...
	if (argc > 2) {
		if (!JS_ValueToObject(cx, argv[2], &obj)) {
			JS_ReportError(cx, "Invalid third argument");
			return JS_FALSE;
		}
		if (JS_GetProperty(cx, obj, "domain", &v) && !JSVAL_IS_VOID(v)) {
			if (!JS_ValueToInt32(cx, v, &domain)) {
				JS_ReportError(cx, "Invalid domain property");
				return JS_FALSE;
			}
		}
		if (JS_GetProperty(cx, obj, "type", &v) && !JSVAL_IS_VOID(v)) {
			if (!JS_ValueToInt32(cx, v, &type)) {
				JS_ReportError(cx, "Invalid type property");
				return JS_FALSE;
			}
		}
		if (JS_GetProperty(cx, obj, "proto", &v) && !JSVAL_IS_VOID(v)) {
			if (!JS_ValueToInt32(cx, v, &proto)) {
				JS_ReportError(cx, "Invalid proto property");
				return JS_FALSE;
			}
		}
		if (JS_GetProperty(cx, obj, "timeout", &v) && !JSVAL_IS_VOID(v)) {
			if (!JS_ValueToInt32(cx, v, &timeout)) {
				JS_ReportError(cx, "Invalid timeout property");
				return JS_FALSE;
			}
		}
		if (JS_GetProperty(cx, obj, "protocol", &v) && !JSVAL_IS_VOID(v)) {
			JSVALUE_TO_MSTRING(cx, v, protocol, NULL);
			HANDLE_PENDING(cx, protocol);
		}
		if (JS_GetProperty(cx, obj, "bindport", &v) && !JSVAL_IS_VOID(v)) {
			bindport = js_port(cx, v, type);
			memset(&addr4, 0, sizeof(addr4));
			addr4.sin_family = AF_INET;
			addr4.sin_addr.s_addr = INADDR_ANY;
			addr4len = sizeof(addr4);
			addr4.sin_port = htons(bindport);
			memset(&addr6, 0, sizeof(addr6));
			addr6.sin6_family = AF_INET6;
			addr6.sin6_addr = in6addr_any;
			addr6len = sizeof(addr6);
			addr6.sin6_port = htons(bindport);
			sockbind = TRUE;
		}
		if (JS_GetProperty(cx, obj, "bindaddrs", &v) && !JSVAL_IS_VOID(v)) {
			if (!sockbind) {
				memset(&addr4, 0, sizeof(addr4));
				addr4.sin_family = AF_INET;
				addr4.sin_addr.s_addr = INADDR_ANY;
				addr4.sin_port = htons(bindport);
				memset(&addr6, 0, sizeof(addr6));
				addr6.sin6_family = AF_INET6;
				addr6.sin6_addr = in6addr_any;
				addr6.sin6_port = htons(bindport);
				sockbind = TRUE;
			}
			addr4len = 0;
			addr6len = 0;
			if (JSVAL_IS_OBJECT(v)) {
				ao = JSVAL_TO_OBJECT(v);
				if (ao == NULL || !JS_IsArrayObject(cx, ao)) {
					JS_ReportError(cx, "Invalid bindaddrs list");
					goto fail;
				}
				if (!JS_GetArrayLength(cx, ao, &count)) {
					JS_ReportError(cx, "Unable to get bindaddrs length");
					goto fail;
				}
				for (i = 0; i < count; i++) {
					if (!JS_GetElement(cx, ao, i, &v)) {
						JS_ReportError(cx, "Invalid bindaddrs entry");
						goto fail;
					}
					JSVALUE_TO_MSTRING(cx, v, host, NULL);
					HANDLE_PENDING(cx, host);
					rc = JS_SUSPENDREQUEST(cx);
					if (!handle_addrs(host, &addr4, &addr4len, &addr6, &addr6len)) {
						JS_RESUMEREQUEST(cx, rc);
						JS_ReportError(cx, "Unparsable bindaddrs entry");
						goto fail;
					}
					FREE_AND_NULL(host);
					JS_RESUMEREQUEST(cx, rc);
				}
			}
			else {
				JSVALUE_TO_MSTRING(cx, v, host, NULL);
				HANDLE_PENDING(cx, host);
				rc = JS_SUSPENDREQUEST(cx);
				if (!handle_addrs(host, &addr4, &addr4len, &addr6, &addr6len)) {
					JS_RESUMEREQUEST(cx, rc);
					JS_ReportError(cx, "Unparsable bindaddrs entry");
					goto fail;
				}
				FREE_AND_NULL(host);
				JS_RESUMEREQUEST(cx, rc);
			}
		}
	}
	JSVALUE_TO_MSTRING(cx, argv[0], host, NULL);
	HANDLE_PENDING(cx, host);
	port = js_port(cx, argv[1], type);
	if (port == 0) {
			JS_ReportError(cx, "Invalid port");
			goto fail;
	}

	obj=JS_NewObject(cx, &js_socket_class, NULL, NULL);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));

	if((p=(js_socket_private_t*)malloc(sizeof(js_socket_private_t)))==NULL) {
		JS_ReportError(cx,"malloc failed");
		goto fail;
	}
	memset(p,0,sizeof(js_socket_private_t));

	rc = JS_SUSPENDREQUEST(cx);
	sprintf(pstr, "%hu", port);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = domain;
	hints.ai_socktype = type;
	hints.ai_protocol = proto;
	hints.ai_flags = AI_ADDRCONFIG | AI_NUMERICSERV;
	if((i = getaddrinfo(host, pstr, &hints, &res)) != 0) {
		JS_RESUMEREQUEST(cx, rc);
		JS_ReportError(cx, gai_strerror(i));
		goto fail;
	}
	p->sock = INVALID_SOCKET;
	for(cur=res; cur && p->sock == INVALID_SOCKET; cur=cur->ai_next) {
		if(p->sock==INVALID_SOCKET) {
			p->sock=socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
			if(p->sock==INVALID_SOCKET)
				continue;
			if (set_socket_options(scfg, p->sock, protocol, error, sizeof(error))) {
				freeaddrinfo(res);
				JS_RESUMEREQUEST(cx, rc);
				JS_ReportError(cx, error);
				goto fail;
			}
			if (sockbind) {
				addr = NULL;
				switch(cur->ai_family) {
					case PF_INET:
						addr = (struct sockaddr *)&addr4;
						addrlen = &addr4len;
						break;
					case PF_INET6:
						addr = (struct sockaddr *)&addr6;
						addrlen = &addr6len;
						break;
				}
				if (addr == NULL)
					continue;
				if (*addrlen == 0)
					continue;
				if (bind(p->sock, addr, *addrlen) != 0) {
					lprintf(LOG_WARNING, "Unable to bind to local address");
					closesocket(p->sock);
					p->sock = INVALID_SOCKET;
					continue;
				}
			}
			/* Set to non-blocking for the connect */
			nonblock=-1;
			ioctlsocket(p->sock, FIONBIO, &nonblock);
		}

		if(connect(p->sock, cur->ai_addr, cur->ai_addrlen)) {
			switch(ERROR_VALUE) {
				case EINPROGRESS:
				case EINTR:
				case EAGAIN:
#if (EAGAIN!=EWOULDBLOCK)
				case EWOULDBLOCK:
#endif
					for(;p->sock!=INVALID_SOCKET;) {
						// TODO: Do clever timeout stuff here.
						if (socket_writable(p->sock, timeout * 1000)) {
							if(!socket_recvdone(p->sock, 0))
								goto connected;
							closesocket(p->sock);
							p->sock=INVALID_SOCKET;
							continue;
						}
						else {
							closesocket(p->sock);
							p->sock=INVALID_SOCKET;
							continue;
						}
					}

connected:
					memcpy(&p->remote_addr, cur->ai_addr, cur->ai_addrlen);
					break;
				default:
					closesocket(p->sock);
					p->sock=INVALID_SOCKET;
					continue;
			}
		}
	}
	freeaddrinfo(res);
	JS_RESUMEREQUEST(cx, rc);

	if (p->sock == INVALID_SOCKET) {
		JS_ReportError(cx, "Unable to connect");
		goto fail;
	}
	ioctlsocket(p->sock,FIONBIO,(ulong*)&(p->nonblocking));
	call_socket_open_callback(TRUE);
	if(protocol)
		free(protocol);
	p->hostname = host;
	p->type = type;
	p->network_byte_order = TRUE;
	p->session=-1;
	p->unflushed = 0;
	p->is_connected = TRUE;

	if(!JS_SetPrivate(cx, obj, p)) {
		JS_ReportError(cx,"JS_SetPrivate failed");
		free(p);
		return(JS_FALSE);
	}

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx,obj,"Class used for outgoing TCP/IP socket communications",317);
	js_DescribeSyncConstructor(cx,obj,"To create a new ConnectedSocket object: "
		"<tt>load('sockdefs.js'); var s = new ConnectedSocket(<i>hostname</i>, <i>port</i>, {domain:<i>domain</i>, proto:<i>proto</i> ,type:<i>type</i>, protocol:<i>protocol</i>, timeout:<i>timeout</i>, bindport:<i>port</i>, bindaddrs:<i>bindaddrs</i>})</tt><br>"
		"where <i>domain</i> (optional) = <tt>PF_UNSPEC</tt> (default) for IPv4 or IPv6, <tt>PF_INET</tt> for IPv4, or <tt>PF_INET6</tt> for IPv6<br>"
		"<i>proto</i> (optional) = <tt>IPPROTO_IP</tt> (default)<br>"
		"<i>type</i> (optional) = <tt>SOCK_STREAM</tt> for TCP (default) or <tt>SOCK_DGRAM</tt> for UDP<br>"
		"<i>protocol</i> (optional) = the name of the protocol or service the socket is to be used for<br>"
		"<i>timeout</i> (optional) = 10 (default) the number of seconds to wait for each connect() call to complete.<br>"
		"<i>bindport</i> (optional) = the name or number of the source port to bind to<br>"
		"<i>bindaddrs</i> (optional) = the name or number of the source addresses to bind to.  The first of each IPv4 or IPv6 type is used for that family.<br>"
		);
	JS_DefineProperty(cx,obj,"_dont_document",JSVAL_TRUE,NULL,NULL,JSPROP_READONLY);
#endif

	if(!js_DefineSocketOptionsArray(cx, obj, type))
		return(JS_FALSE);

	dbprintf(FALSE, p, "object constructed");
	return(JS_TRUE);

fail:
	if (p)
		free(p);
	if (protocol)
		free(protocol);
	if (host)
		free(host);
	return JS_FALSE;
}

struct ls_cb_data {
	char *protocol;
	scfg_t *scfg;
};

static void
ls_cb(SOCKET sock, void *cbptr)
{
	struct ls_cb_data *cb = cbptr;
	char error[256];

	call_socket_open_callback(TRUE);
	if(set_socket_options(cb->scfg, sock, cb->protocol, error, sizeof(error)))
		lprintf(LOG_ERR,"%04d !ERROR %s", sock, error);
}

static JSBool
js_listening_socket_constructor(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj;
	jsval *argv=JS_ARGV(cx, arglist);
	int32 type=SOCK_STREAM;	/* default = TCP */
	int32 domain = PF_UNSPEC; /* default = IPvAny */
	int32 proto = IPPROTO_IP;
	int retry_count = 0;
	int retry_delay = 15;
	char* protocol=NULL;
	char *interface=NULL;
	js_socket_private_t* p = NULL;
	jsval v;
	uint16_t port;
	jsrefcount rc;
	scfg_t *scfg;
	struct xpms_set *set = NULL;
	struct ls_cb_data cb;
	jsuint count;
	int i;

	scfg = JS_GetRuntimePrivate(JS_GetRuntime(cx));
	if (scfg == NULL) {
		JS_ReportError(cx, "Unable to get private runtime");
		goto fail;
	}
	cb.scfg = scfg;
	if (argc < 3) {
		JS_ReportError(cx, "At least three arguments required (interfaces, port, and protocol)");
		goto fail;
	}
	if (argc > 3) {
		if (!JS_ValueToObject(cx, argv[3], &obj)) {
			JS_ReportError(cx, "Invalid fourth argument");
			goto fail;
		}
		if (JS_GetProperty(cx, obj, "domain", &v) && !JSVAL_IS_VOID(v)) {
			if (!JS_ValueToInt32(cx, v, &domain)) {
				JS_ReportError(cx, "Invalid domain property");
				goto fail;
			}
		}
		if (JS_GetProperty(cx, obj, "type", &v) && !JSVAL_IS_VOID(v)) {
			if (!JS_ValueToInt32(cx, v, &type)) {
				JS_ReportError(cx, "Invalid type property");
				goto fail;
			}
		}
		if (JS_GetProperty(cx, obj, "proto", &v) && !JSVAL_IS_VOID(v)) {
			if (!JS_ValueToInt32(cx, v, &proto)) {
				JS_ReportError(cx, "Invalid proto property");
				goto fail;
			}
		}
		if (JS_GetProperty(cx, obj, "retry_count", &v) && !JSVAL_IS_VOID(v)) {
			if (!JS_ValueToInt32(cx, v, &retry_count)) {
				JS_ReportError(cx, "Invalid retry_count property");
				goto fail;
			}
		}
		if (JS_GetProperty(cx, obj, "retry_delay", &v) && !JSVAL_IS_VOID(v)) {
			if (!JS_ValueToInt32(cx, v, &retry_delay)) {
				JS_ReportError(cx, "Invalid retry_delay property");
				goto fail;
			}
		}
	}
	obj = NULL;
	port = js_port(cx, argv[1], type);
	JSVALUE_TO_MSTRING(cx, argv[2], protocol, NULL);
	HANDLE_PENDING(cx, protocol);
	cb.protocol = protocol;
	if (JSVAL_IS_OBJECT(argv[0])) {
		obj = JSVAL_TO_OBJECT(argv[0]);
		if (obj == NULL || !JS_IsArrayObject(cx, obj)) {
			JS_ReportError(cx, "Invalid interface list");
			goto fail;
		}
	}
	set = xpms_create(retry_count, retry_delay, lprintf);
	if (set == NULL) {
		JS_ReportError(cx, "Unable to create socket set");
		goto fail;
	}
	if (obj == NULL) {
		JSVALUE_TO_MSTRING(cx, argv[0], interface, NULL);
		HANDLE_PENDING(cx, interface);
		rc = JS_SUSPENDREQUEST(cx);
		if (!xpms_add(set, domain, type, proto, interface, port, protocol, ls_cb, NULL, &cb)) {
			JS_RESUMEREQUEST(cx, rc);
			JS_ReportError(cx, "Unable to add host to socket set");
			goto fail;
		}
		JS_RESUMEREQUEST(cx, rc);
	}
	else {
		if (!JS_GetArrayLength(cx, obj, &count)) {
			JS_ReportError(cx, "zero-length array");
			goto fail;
		}
		for (i = 0; (jsuint)i < count; i++) {
			if (!JS_GetElement(cx, obj, i, &v)) {
				lprintf(LOG_WARNING, "Unable to get element %d from interface array", i);
				continue;
			}
			JSVALUE_TO_MSTRING(cx, v, interface, NULL);
			HANDLE_PENDING(cx, interface);
			rc = JS_SUSPENDREQUEST(cx);
			if (!xpms_add(set, domain, type, proto, interface, port, protocol, ls_cb, NULL, &cb)) {
				free(interface);
				JS_RESUMEREQUEST(cx, rc);
				JS_ReportError(cx, "Unable to add host to socket set");
				goto fail;
			}
			free(interface);
			JS_RESUMEREQUEST(cx, rc);
		}
	}

	obj=JS_NewObject(cx, &js_socket_class, NULL, NULL);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));

	if((p=(js_socket_private_t*)malloc(sizeof(js_socket_private_t)))==NULL) {
		JS_ReportError(cx,"malloc failed");
		free(protocol);
		free(set);
		return(JS_FALSE);
	}
	memset(p,0,sizeof(js_socket_private_t));
	p->type = type;
	p->set = set;
	p->sock = INVALID_SOCKET;
	p->external = TRUE;
	p->network_byte_order = TRUE;
	p->session=-1;
	p->unflushed = 0;
	p->local_port = port;

	if(!JS_SetPrivate(cx, obj, p)) {
		JS_ReportError(cx,"JS_SetPrivate failed");
		free(p);
		free(set);
		return(JS_FALSE);
	}

	if(!js_DefineSocketOptionsArray(cx, obj, type)) {
		free(p);
		free(set);
		return(JS_FALSE);
	}

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx,obj,"Class used for incoming TCP/IP socket communications",317);
	js_DescribeSyncConstructor(cx,obj,"To create a new ListeningSocket object: "
		"<tt>load('sockdefs.js'); var s = new ListeningSocket(<i>interface</i>, <i>port</i> ,<i>protocol</i>, {domain:<i>domain</i>, type:<i>type</i>, proto:<i>proto</i>, retry_count:<i>retry_count</i>, retry_delay:<i>retry_delay</i>})</tt><br>"
		"where <i>interface</i> = A array or strings or a single string of hostnames or address optionally including a :port suffix<br>"
		"<i>port</i> = a port to use when the interface doesn't specify one<br>"
		"<i>protocol</i> = protocol name, used for socket options and logging.<br>"
		"<i>domain</i> (optional) = <tt>PF_UNSPEC</tt> (default) for IPv4 or IPv6, <tt>AF_INET</tt> for IPv4, or <tt>AF_INET6</tt> for IPv6<br>"
		"<i>proto</i> (optional) = <tt>IPPROTO_IP</tt> (default)<br>"
		"<i>type</i> (optional) = <tt>SOCK_STREAM</tt> for TCP (default) or <tt>SOCK_DGRAM</tt> for UDP<br>"
		"<i>retry_count</i> (optional) = <tt>0</tt> (default) number of times to retry binding<br>"
		"and <i>retry_delay</i> (optional) = <tt>15</tt> (default) seconds to wait before a retry<br>"
		);
	JS_DefineProperty(cx,obj,"_dont_document",JSVAL_TRUE,NULL,NULL,JSPROP_READONLY);
#endif

	dbprintf(FALSE, p, "object constructed");

	return(JS_TRUE);

fail:
	free(protocol);
	free(set);
	return JS_FALSE;
}

static JSBool
js_socket_constructor(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj;
	jsval *argv=JS_ARGV(cx, arglist);
	int32	type=SOCK_STREAM;	/* default = TCP */
	int		domain = AF_INET; /* default = IPv4 */
	uintN	i;
	js_socket_private_t* p;
	char*	protocol=NULL;
	BOOL	from_descriptor=FALSE;

	i = 0;
	if(JSVAL_IS_BOOLEAN(argv[i]) && argc > 1) {
		from_descriptor = JSVAL_TO_BOOLEAN(argv[i]);
		i++;
		if (from_descriptor) {
			uint32 sock;
			if(!JS_ValueToECMAUint32(cx,argv[i],&sock)) {
				JS_ReportError(cx, "Failed to convert socket descriptor to uint32");
				return JS_FALSE;
			}
			obj = js_CreateSocketObjectWithoutParent(cx, sock, -1);
			if (obj == NULL) {
				JS_ReportError(cx, "Failed to create external socket object");
				return JS_FALSE;
			}
			JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));
			FREE_AND_NULL(protocol);
			return JS_TRUE;
		}
	}

	for(;i<argc;i++) {
		if(JSVAL_IS_NUMBER(argv[i])) {
			JS_ValueToInt32(cx,argv[i],&type);
		}
		else if(JSVAL_IS_BOOLEAN(argv[i])) {
			if(argv[i] == JSVAL_TRUE)
				domain = AF_INET6;
		}
		else if(JSVAL_IS_STRING(argv[i])) {
			if(protocol == NULL) {
				JSVALUE_TO_MSTRING(cx, argv[i], protocol, NULL);
				HANDLE_PENDING(cx, protocol);
			}
		}
	}

	obj=JS_NewObject(cx, &js_socket_class, NULL, NULL);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));

	if((p=(js_socket_private_t*)malloc(sizeof(js_socket_private_t)))==NULL) {
		JS_ReportError(cx,"malloc failed");
		if(protocol)
			free(protocol);
		return(JS_FALSE);
	}
	memset(p,0,sizeof(js_socket_private_t));

	if((p->sock=open_socket(domain,type,protocol))==INVALID_SOCKET) {
		JS_ReportError(cx,"open_socket failed with error %d",ERROR_VALUE);
		if(protocol)
			free(protocol);
		free(p);
		return(JS_FALSE);
	}
	if(protocol)
		free(protocol);
	p->type = type;
	p->network_byte_order = TRUE;
	p->session=-1;
	p->unflushed = 0;

	if(!JS_SetPrivate(cx, obj, p)) {
		JS_ReportError(cx,"JS_SetPrivate failed");
		free(p);
		return(JS_FALSE);
	}

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx,obj,"Class used for TCP/IP socket communications",310);
	js_DescribeSyncConstructor(cx,obj,"To create a new Socket object: "
		"<tt>load('sockdefs.js'); var s = new Socket(<i>type</i>, <i>protocol</i> ,<i>ipv6</i>=false)</tt><br>"
		"where <i>type</i> = <tt>SOCK_STREAM</tt> for TCP (default) or <tt>SOCK_DGRAM</tt> for UDP<br>"
		"and <i>protocol</i> (optional) = the name of the protocol or service the socket is to be used for<br>"
		"To create a socket from an existing socket descriptor: "
		"<tt>var s = new Socket(true, <i>descriptor</i>)</tt><br>"
		"where <i>descriptor</i> is the numerical value of an existing valid socket descriptor"
		);
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", socket_prop_desc, JSPROP_READONLY);
#endif

	if(!js_DefineSocketOptionsArray(cx, obj, type))
		return(JS_FALSE);

	dbprintf(FALSE, p, "object constructed");
	return(JS_TRUE);
}

JSObject* js_CreateSocketClass(JSContext* cx, JSObject* parent)
{
	JSObject*	sockobj;
	JSObject*	sockproto;
	JSObject*	csockobj;
	JSObject*	lsockobj;
	jsval		val;
	JSObject*	constructor;

	sockobj = JS_InitClass(cx, parent, NULL
		,&js_socket_class
		,js_socket_constructor
		,0	/* number of constructor args */
		,NULL /* props, specified in constructor */
		,NULL /* funcs, specified in constructor */
		,NULL,NULL);
	if (sockobj == NULL)
		return sockobj;
	if(JS_GetProperty(cx, parent, js_socket_class.name, &val) && !JSVAL_NULL_OR_VOID(val)) {
		JS_ValueToObject(cx,val,&constructor);
		JS_DefineProperty(cx, constructor, "PF_INET", INT_TO_JSVAL(PF_INET), NULL, NULL
			, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
		JS_DefineProperty(cx, constructor, "PF_INET6", INT_TO_JSVAL(PF_INET6), NULL, NULL
			, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
		JS_DefineProperty(cx, constructor, "AF_INET", INT_TO_JSVAL(AF_INET), NULL, NULL
			, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
		JS_DefineProperty(cx, constructor, "AF_INET6", INT_TO_JSVAL(AF_INET6), NULL, NULL
			, JSPROP_PERMANENT|JSPROP_ENUMERATE|JSPROP_READONLY);
	}
	sockproto = JS_GetPrototype(cx, sockobj);
	csockobj = JS_InitClass(cx, parent, sockproto
		,&js_connected_socket_class
		,js_connected_socket_constructor
		,2	/* number of constructor args */
		,NULL /* props, specified in constructor */
		,NULL /* funcs, specified in constructor */
		,NULL,NULL);
	(void)csockobj;
	lsockobj = JS_InitClass(cx, parent, sockproto
		,&js_listening_socket_class
		,js_listening_socket_constructor
		,2	/* number of constructor args */
		,NULL /* props, specified in constructor */
		,NULL /* funcs, specified in constructor */
		,NULL,NULL);
	(void)lsockobj;

	return(sockobj);
}

JSObject* js_CreateSocketObject(JSContext* cx, JSObject* parent, char *name, SOCKET sock, CRYPT_CONTEXT session)
{
	JSObject*	obj;

	obj = js_CreateSocketObjectWithoutParent(cx, sock, session);
	if(obj==NULL)
		return(NULL);
	JS_DefineProperty(cx, parent, name, OBJECT_TO_JSVAL(obj), NULL, NULL, JSPROP_ENUMERATE|JSPROP_READONLY);

	return(obj);
}

JSObject* js_CreateSocketObjectFromSet(JSContext* cx, JSObject* parent, char *name, struct xpms_set *set)
{
	JSObject*	obj;
	js_socket_private_t*	p;
	int			type=SOCK_STREAM;
	socklen_t	len;

	obj = JS_DefineObject(cx, parent, name, &js_socket_class, NULL
		,JSPROP_ENUMERATE|JSPROP_READONLY);

	if(obj==NULL)
		return(NULL);

	if(set->sock_count < 1)
		return NULL;

	len = sizeof(type);
	getsockopt(set->socks[0].sock,SOL_SOCKET,SO_TYPE,(void*)&type,&len);

	if(!js_DefineSocketOptionsArray(cx, obj, type))
		return(NULL);

	if((p=(js_socket_private_t*)malloc(sizeof(js_socket_private_t)))==NULL)
		return(NULL);
	memset(p,0,sizeof(js_socket_private_t));

	p->set = set;
	p->sock = INVALID_SOCKET;
	p->external = TRUE;
	p->network_byte_order = TRUE;
	p->session=-1;
	p->unflushed = 0;

	if(!JS_SetPrivate(cx, obj, p)) {
		dbprintf(TRUE, p, "JS_SetPrivate failed");
		return(NULL);
	}

	dbprintf(FALSE, p, "object created");

	return(obj);
}

#endif	/* JAVSCRIPT */
