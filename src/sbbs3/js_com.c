/* js_com.c */

/* Synchronet JavaScript "COM" Object */

/* $Id: js_com.c,v 1.33 2020/04/20 01:47:45 rswindell Exp $ */

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

#include "sbbs.h"
#include "comio.h"
#include "js_request.h"

#ifdef JAVASCRIPT

typedef struct
{
	COM_HANDLE	com;
	BOOL		external;	/* externally created, don't close */
	BOOL		is_open;
	BOOL		network_byte_order;
	BOOL		debug;
	BOOL		dtr;
	long		baud_rate;
	int			last_error;
	char		*dev;

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
	
	lprintf(LOG_DEBUG,"%s %s%s",p->dev,error ? "ERROR: ":"",sbuf);
}

/* COM Destructor */

static void js_finalize_com(JSContext *cx, JSObject *obj)
{
	private_t* p;

	if((p=(private_t*)JS_GetPrivate(cx,obj))==NULL)
		return;

	if(p->external==FALSE && p->com!=COM_HANDLE_INVALID) {
		comClose(p->com);
		dbprintf(FALSE, p, "closed");
	}

	if(p->dev)
		free(p->dev);
	free(p);

	JS_SetPrivate(cx, obj, NULL);
}


/* COM Object Methods */

extern JSClass js_com_class;

static JSBool
js_close(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	private_t*	p;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_com_class))==NULL) {
		return(JS_FALSE);
	}

	if(p->com==COM_HANDLE_INVALID)
		return(JS_TRUE);

	rc=JS_SUSPENDREQUEST(cx);
	comClose(p->com);

	p->last_error = COM_ERROR_VALUE;

	dbprintf(FALSE, p, "closed");

	p->com = COM_HANDLE_INVALID; 
	p->is_open = FALSE;
	JS_RESUMEREQUEST(cx, rc);

	return(JS_TRUE);
}

static JSBool
js_open(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	private_t*	p;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_com_class))==NULL) {
		return(JS_FALSE);
	}

	rc=JS_SUSPENDREQUEST(cx);
	dbprintf(FALSE, p, "opening port %s", p->dev);

	p->com=comOpen(p->dev);

	if(p->com==COM_HANDLE_INVALID) {
		p->last_error=COM_ERROR_VALUE;
		dbprintf(TRUE, p, "connect failed with error %d", p->last_error);
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
		JS_RESUMEREQUEST(cx, rc);
		return(JS_TRUE);
	}

	comSetBaudRate(p->com, p->baud_rate);

	p->is_open = TRUE;
	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	dbprintf(FALSE, p, "connected to port %s", p->dev);
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
	private_t*	p;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_com_class))==NULL) {
		return(JS_FALSE);
	}

	if(!js_argc(cx, argc, 1))
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	JSVALUE_TO_MSTRING(cx, argv[0], cp, &len);
	HANDLE_PENDING(cx, cp);

	rc=JS_SUSPENDREQUEST(cx);
	if(cp && comWriteBuf(p->com,(uint8_t *)cp,len)==len) {
		dbprintf(FALSE, p, "sent %u bytes",len);
		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	} else {
		p->last_error=COM_ERROR_VALUE;
		dbprintf(TRUE, p, "send of %u bytes failed",len);
	}
	if(cp)
		free(cp);
	JS_RESUMEREQUEST(cx, rc);
		
	return(JS_TRUE);
}

static JSBool
js_sendfile(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	long		len;
	int			file;
	char*		fname = NULL;
	private_t*	p;
	jsrefcount	rc;
	char		*buf;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_com_class))==NULL) {
		return(JS_FALSE);
	}

	if(!js_argc(cx, argc, 1))
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	JSVALUE_TO_MSTRING(cx, argv[0], fname, NULL);
	HANDLE_PENDING(cx, fname);
	if(fname==NULL) {
		JS_ReportError(cx,"Failure reading filename");
		return(JS_FALSE);
	}

	rc=JS_SUSPENDREQUEST(cx);
	if((file=nopen(fname,O_RDONLY|O_BINARY))==-1) {
		JS_RESUMEREQUEST(cx, rc);
		free(fname);
		return(JS_TRUE);
	}

	free(fname);
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

	if(comWriteBuf(p->com,(uint8_t *)buf,len)==len) {
		dbprintf(FALSE, p, "sent %u bytes",len);
		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	} else {
		p->last_error=COM_ERROR_VALUE;
		dbprintf(TRUE, p, "send of %u bytes failed",len);
	}
	free(buf);

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

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_com_class))==NULL) {
		return(JS_FALSE);
	}

	if(!js_argc(cx, argc, 1))
		return JS_FALSE;

	if(!JS_ValueToInt32(cx,argv[0],&val))
		return JS_FALSE;

	if(!JS_ValueToInt32(cx,argv[1],&size))
		return JS_FALSE;

	rc=JS_SUSPENDREQUEST(cx);
	switch(size) {
		case sizeof(BYTE):
			b = (BYTE)val;
			wr=comWriteBuf(p->com,&b,size);
			break;
		case sizeof(WORD):
			w = (WORD)val;
			if(p->network_byte_order)
				w=htons(w);
			wr=comWriteBuf(p->com,(BYTE*)&w,size);
			break;
		case sizeof(DWORD):
			l = val;
			if(p->network_byte_order)
				l=htonl(l);
			wr=comWriteBuf(p->com,(BYTE*)&l,size);
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
		p->last_error=COM_ERROR_VALUE;
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
	int32		timeout=30;	/* seconds */
	private_t*	p;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_com_class))==NULL) {
		return(JS_FALSE);
	}

	if(argc) {
		if(!JS_ValueToInt32(cx,argv[0],&len))
			return JS_FALSE;
	}

	if(argc>1) {
		if(!JS_ValueToInt32(cx,argv[1],&timeout))
			return JS_FALSE;
	}

	if((buf=(char*)malloc(len+1))==NULL) {
		JS_ReportError(cx,"Error allocating %u bytes",len+1);
		return(JS_FALSE);
	}

	rc=JS_SUSPENDREQUEST(cx);
	len = comReadBuf(p->com,buf,len,NULL,timeout);
	JS_RESUMEREQUEST(cx, rc);
	if(len<0) {
		p->last_error=COM_ERROR_VALUE;
		free(buf);
		JS_SET_RVAL(cx, arglist, JSVAL_NULL);
		return(JS_TRUE);
	}
	buf[len]=0;

	str = JS_NewStringCopyN(cx, buf, len);
	free(buf);
	if(str==NULL)
		return(JS_FALSE);

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(str));
	rc=JS_SUSPENDREQUEST(cx);
	dbprintf(FALSE, p, "received %u bytes",len);
	JS_RESUMEREQUEST(cx, rc);
	
	return(JS_TRUE);
}

static JSBool
js_recvline(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char*		buf;
	int			i;
	int32		len=512;
	int32		timeout=30;	/* seconds */
	JSString*	str;
	private_t*	p;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_com_class))==NULL) {
		return(JS_FALSE);
	}

	if(argc) {
		if(!JS_ValueToInt32(cx,argv[0],&len))
			return JS_FALSE;
	}

	if((buf=(char*)malloc(len+1))==NULL) {
		JS_ReportError(cx,"Error allocating %u bytes",len+1);
		return(JS_FALSE);
	}

	if(argc>1) {
		if(!JS_ValueToInt32(cx,argv[1],&timeout)) {
			free(buf);
			return JS_FALSE;
		}
	}

	rc=JS_SUSPENDREQUEST(cx);

	i=comReadLine(p->com, buf, len+1, timeout);
	
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
		,i,COM_ERROR_VALUE);
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
	int32		timeout=30;	/* seconds */

	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(-1));

	if((p=(private_t*)js_GetClassPrivate(cx, obj, &js_com_class))==NULL) {
		return(JS_FALSE);
	}

	if(argc) {
		if(!JS_ValueToInt32(cx,argv[0],&size))
			return JS_FALSE;
	}

	if(argc>1) {
		if(!JS_ValueToInt32(cx,argv[1],&timeout))
			return JS_FALSE;
	}

	rc=JS_SUSPENDREQUEST(cx);
	switch(size) {
		case sizeof(BYTE):
			if((rd=comReadBuf(p->com,(char*)&b,size,NULL,timeout))==size)
				JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(b));
			break;
		case sizeof(WORD):
			if((rd=comReadBuf(p->com,(char*)&w,size,NULL,timeout))==size) {
				if(p->network_byte_order)
					w=ntohs(w);
				JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(w));
			}
			break;
		case sizeof(DWORD):
			if((rd=comReadBuf(p->com,(char*)&l,size,NULL,timeout))==size) {
				if(p->network_byte_order)
					l=ntohl(l);
				JS_SET_RVAL(cx, arglist,UINT_TO_JSVAL(l));
			}
			break;
	}

	if(rd!=size)
		p->last_error=COM_ERROR_VALUE;
		
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

/* COM Object Properites */
enum {
	 COM_PROP_LAST_ERROR
	,COM_PROP_IS_OPEN
	,COM_PROP_DEBUG
	,COM_PROP_DESCRIPTOR
	,COM_PROP_NETWORK_ORDER
	,COM_PROP_BAUD_RATE
	,COM_PROP_DEVICE
	,COM_PROP_DTR
	,COM_PROP_CTS
	,COM_PROP_DSR
	,COM_PROP_RING
	,COM_PROP_DCD

};

#ifdef BUILD_JSDOCS
static char* com_prop_desc[] = {
	 "error status for the last COM operation that failed - <small>READ ONLY</small>"
	,"<i>true</i> if port is in a connected state - <small>READ ONLY</small>"
	,"enable debug logging"
	,"COM handle (advanced uses only)"
	,"<i>true</i> if binary data is to be sent in Network Byte Order (big end first), default is <i>true</i>"
	,"COM port Baud rate"
	,"Device name"
	,"Data Terminal Ready"
	,"Clear To Send"
	,"Data Set Ready"
	,"Ring Indicator"
	,"Data Carrier Detect"
	,NULL
};
#endif

static JSBool js_com_set(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
	jsval idval;
    jsint       tiny;
	private_t*	p;
	jsrefcount	rc;
	double		d;
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
		case COM_PROP_DEBUG:
			JS_ValueToBoolean(cx,*vp,&(p->debug));
			break;
		case COM_PROP_DESCRIPTOR:
			if(!JS_ValueToInt32(cx,*vp,&i))
				return JS_FALSE;
			p->com=(COM_HANDLE)i;
			p->is_open=TRUE;
			break;
		case COM_PROP_LAST_ERROR:
			if(!JS_ValueToInt32(cx,*vp,&i))
				return JS_FALSE;
			p->last_error=i;
			break;
		case COM_PROP_BAUD_RATE:
			JS_ValueToNumber(cx,*vp,&d);
			p->baud_rate=(long)d;
			rc=JS_SUSPENDREQUEST(cx);
			if(p->is_open)
				comSetBaudRate(p->com, p->baud_rate);
			JS_RESUMEREQUEST(cx, rc);
			break;
		case COM_PROP_NETWORK_ORDER:
			JS_ValueToBoolean(cx,*vp,&(p->network_byte_order));
			break;
		case COM_PROP_DTR:
			JS_ValueToBoolean(cx,*vp,&(p->dtr));
			if(p->is_open) {
				if(p->dtr)
					comRaiseDTR(p->com);
				else
					comLowerDTR(p->com);
			}
			else
				p->dtr=FALSE;
			break;

	}

	return(JS_TRUE);
}

static JSBool js_com_get(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	jsval idval;
    jsint       tiny;
	private_t*	p;
	JSString*	js_str;
	jsrefcount	rc;
	long		baud_rate;
	int			ms;

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
		case COM_PROP_LAST_ERROR:
			*vp = INT_TO_JSVAL(p->last_error);
			break;
		case COM_PROP_IS_OPEN:
			if(p->is_open)
				*vp = JSVAL_TRUE;
			else
				*vp = JSVAL_FALSE;
			break;
		case COM_PROP_DEBUG:
			*vp = BOOLEAN_TO_JSVAL(p->debug);
			break;
		case COM_PROP_DESCRIPTOR:
			*vp = INT_TO_JSVAL((int)p->com);
			break;
		case COM_PROP_NETWORK_ORDER:
			*vp = BOOLEAN_TO_JSVAL(p->network_byte_order);
			break;
		case COM_PROP_BAUD_RATE:
			baud_rate=comGetBaudRate(p->com);
			*vp=UINT_TO_JSVAL(baud_rate);
			break;
		case COM_PROP_DEVICE:
			JS_RESUMEREQUEST(cx, rc);
			if((js_str=JS_NewStringCopyZ(cx,p->dev))==NULL)
				return(JS_FALSE);
			*vp = STRING_TO_JSVAL(js_str);
			rc=JS_SUSPENDREQUEST(cx);
			break;
		case COM_PROP_DTR:
			*vp = BOOLEAN_TO_JSVAL(p->dtr);
			break;
		case COM_PROP_CTS:
			ms=comGetModemStatus(p->com);
			*vp = BOOLEAN_TO_JSVAL(ms & COM_CTS);
			break;
		case COM_PROP_DSR:
			ms=comGetModemStatus(p->com);
			*vp = BOOLEAN_TO_JSVAL(ms & COM_DSR);
			break;
		case COM_PROP_RING:
			ms=comGetModemStatus(p->com);
			*vp = BOOLEAN_TO_JSVAL(ms & COM_RING);
			break;
		case COM_PROP_DCD:
			ms=comGetModemStatus(p->com);
			*vp = BOOLEAN_TO_JSVAL(ms & COM_DCD);
			break;

	}

	JS_RESUMEREQUEST(cx, rc);
	return(TRUE);
}

#define COM_PROP_FLAGS JSPROP_ENUMERATE|JSPROP_READONLY

static jsSyncPropertySpec js_com_properties[] = {
/*		 name				,tinyid					,flags,				ver	*/

	{	"error"				,COM_PROP_LAST_ERROR	,COM_PROP_FLAGS,	315 },
	{	"last_error"		,COM_PROP_LAST_ERROR	,JSPROP_READONLY,	315 },	/* alias */
	{	"is_open"			,COM_PROP_IS_OPEN		,COM_PROP_FLAGS,	315 },
	{	"debug"				,COM_PROP_DEBUG			,JSPROP_ENUMERATE,	315 },
	{	"descriptor"		,COM_PROP_DESCRIPTOR	,JSPROP_ENUMERATE,	315 },
	{	"network_byte_order",COM_PROP_NETWORK_ORDER	,JSPROP_ENUMERATE,	315 },
	{	"baud_rate"			,COM_PROP_BAUD_RATE		,JSPROP_ENUMERATE,	315 },
	{	"device"			,COM_PROP_DEVICE		,COM_PROP_FLAGS,	315 },
	{	"dtr"				,COM_PROP_DTR			,JSPROP_ENUMERATE,	315 },
	{	"cts"				,COM_PROP_CTS			,COM_PROP_FLAGS,	315 },
	{	"dsr"				,COM_PROP_DSR			,COM_PROP_FLAGS,	315 },
	{	"ring"				,COM_PROP_RING			,COM_PROP_FLAGS,	315 },
	{	"dcd"				,COM_PROP_DCD			,COM_PROP_FLAGS,	315 },
	{0}
};

static jsSyncMethodSpec js_com_functions[] = {
	{"close",		js_close,		0,	JSTYPE_VOID,	""
	,JSDOCSTR("close the port immediately")
	,315
	},
	{"open",     js_open,     2,	JSTYPE_BOOLEAN,	JSDOCSTR("")
	,JSDOCSTR("connect to a COM port")
	,315
	},
	{"write",		js_send,		1,	JSTYPE_ALIAS },
	{"send",		js_send,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("data")
	,JSDOCSTR("send a string (AKA write)")
	,315
	},
	{"sendfile",	js_sendfile,	1,	JSTYPE_BOOLEAN,	JSDOCSTR("path/filename")
	,JSDOCSTR("send an entire file over the port")
	,315
	},
	{"writeBin",	js_sendbin,		1,	JSTYPE_ALIAS },
	{"sendBin",		js_sendbin,		1,	JSTYPE_BOOLEAN,	JSDOCSTR("value [,bytes=<tt>4</tt>]")
	,JSDOCSTR("send a binary integer over the port, default number of bytes is 4 (32-bits)")
	,315
	},
	{"read",		js_recv,		1,	JSTYPE_ALIAS },
	{"recv",		js_recv,		1,	JSTYPE_STRING,	JSDOCSTR("[maxlen=<tt>512</tt> [,timeout=<tt>30</tt>]]")
	,JSDOCSTR("receive a string, default maxlen is 512 characters, default timeout is 30 seconds (AKA read)")
	,315
	},
	{"readline",	js_recvline,	0,	JSTYPE_ALIAS },
	{"readln",		js_recvline,	0,	JSTYPE_ALIAS },
	{"recvline",	js_recvline,	0,	JSTYPE_STRING,	JSDOCSTR("[maxlen=<tt>512</tt>] [,timeout=<tt>30.0</tt>]")
	,JSDOCSTR("receive a line-feed terminated string, default maxlen is 512 characters, default timeout is 30 seconds (AKA readline and readln)")
	,315
	},
	{"readBin",		js_recvbin,		0,	JSTYPE_ALIAS },
	{"recvBin",		js_recvbin,		0,	JSTYPE_NUMBER,	JSDOCSTR("[bytes=<tt>4</tt> [,timeout=<tt>30</tt>]")
	,JSDOCSTR("receive a binary integer from the port, default number of bytes is 4 (32-bits), default timeout is 30 seconds")
	,315
	},
	{0}
};

static JSBool js_com_resolve(JSContext *cx, JSObject *obj, jsid id)
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

	ret=js_SyncResolve(cx, obj, name, js_com_properties, js_com_functions, NULL, 0);
	if(name)
		free(name);
	return ret;
}

static JSBool js_com_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_com_resolve(cx, obj, JSID_VOID));
}

JSClass js_com_class = {
     "COM"				/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_com_get				/* getProperty	*/
	,js_com_set				/* setProperty	*/
	,js_com_enumerate		/* enumerate	*/
	,js_com_resolve			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,js_finalize_com		/* finalize		*/
};

/* COM Constructor (creates COM descriptor) */

static JSBool
js_com_constructor(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj;
	jsval *argv=JS_ARGV(cx, arglist);
	private_t* p;
	char*		fname = NULL;

	obj=JS_NewObject(cx, &js_com_class, NULL, NULL);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));
	if(argc > 0) {
		JSVALUE_TO_MSTRING(cx, argv[0], fname, NULL);
		HANDLE_PENDING(cx, fname);
	}
	if(argc==0 || fname==NULL) {
		JS_ReportError(cx,"Failure reading port name");
		return(JS_FALSE);
	}

	if((p=(private_t*)malloc(sizeof(private_t)))==NULL) {
		JS_ReportError(cx,"malloc failed");
		free(fname);
		return(JS_FALSE);
	}
	memset(p,0,sizeof(private_t));

	p->dev=fname;
	p->network_byte_order = TRUE;
	p->baud_rate = 9600;
	p->com = COM_HANDLE_INVALID;

	if(!JS_SetPrivate(cx, obj, p)) {
		JS_ReportError(cx,"JS_SetPrivate failed");
		return(JS_FALSE);
	}

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx,obj,"Class used for serial port communications",31501);
	js_DescribeSyncConstructor(cx,obj,"To create a new COM object: "
		"var c = new COM('<i>device</i>')</tt><br>"
		"where <i>device</i> = <tt>COMx</tt> (e.g. COM1) for Win32 or <tt>/dev/ttyXY</tt> for *nix (e.g. /dev/ttyu0)"
		);
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", com_prop_desc, JSPROP_READONLY);
#endif

	dbprintf(FALSE, p, "object constructed");
	return(JS_TRUE);
}

JSObject* DLLCALL js_CreateCOMClass(JSContext* cx, JSObject* parent)
{
	JSObject*	comobj;

	comobj = JS_InitClass(cx, parent, NULL
		,&js_com_class
		,js_com_constructor
		,0	/* number of constructor args */
		,NULL /* props, specified in constructor */
		,NULL /* funcs, specified in constructor */
		,NULL,NULL);

	return(comobj);
}

JSObject* DLLCALL js_CreateCOMObject(JSContext* cx, JSObject* parent, const char *name, COM_HANDLE com)
{
	JSObject*	obj;
	private_t*	p;

	obj = JS_DefineObject(cx, parent, name, &js_com_class, NULL
		,JSPROP_ENUMERATE|JSPROP_READONLY);

	if(obj==NULL)
		return(NULL);

	if((p=(private_t*)malloc(sizeof(private_t)))==NULL)
		return(NULL);
	memset(p,0,sizeof(private_t));

	p->com = com;
	p->external = TRUE;
	p->network_byte_order = TRUE;

	if(!JS_SetPrivate(cx, obj, p)) {
		dbprintf(TRUE, p, "JS_SetPrivate failed");
		return(NULL);
	}

	dbprintf(FALSE, p, "object created");

	return(obj);
}

#endif	/* JAVSCRIPT */
