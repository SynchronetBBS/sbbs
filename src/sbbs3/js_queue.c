/* Synchronet JavaScript "Queue" Object */

/* $Id: js_queue.c,v 1.57 2019/08/22 01:41:23 rswindell Exp $ */

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
#include "msg_queue.h"
#include "js_request.h"

typedef struct
{
	char		name[128];
	size_t		size;
	uint64		*value;
} queued_value_t;

link_list_t named_queues;

/* Queue Destructor */

static void js_finalize_queue(JSContext *cx, JSObject *obj)
{
	msg_queue_t* q;
	list_node_t* n;

	if((q=(msg_queue_t*)JS_GetPrivate(cx,obj))==NULL)
		return;

	if(msgQueueDetach(q)==0 && (n=listFindNode(&named_queues,q,/* length=0 for ptr compare */0))!=NULL)
		listRemoveNode(&named_queues,n,FALSE);

	JS_SetPrivate(cx, obj, NULL);
}

static void js_decode_value(JSContext *cx, JSObject *parent
							   ,queued_value_t* v, jsval* rval)
{
	*rval = JSVAL_VOID;

	if(v==NULL)
		return;

	JS_ReadStructuredClone(cx, v->value, v->size, JS_STRUCTURED_CLONE_VERSION, rval, NULL, NULL);

	return;
}

/* Queue Object Methods */

extern JSClass js_queue_class;

static JSBool
js_poll(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	msg_queue_t*	q;
	queued_value_t*	v;
	int32 timeout=0;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((q=(msg_queue_t*)js_GetClassPrivate(cx, obj, &js_queue_class))==NULL) {
		return(JS_FALSE);
	}

	if(argc && JSVAL_IS_NUMBER(argv[0])) { 	/* timeout specified */
		if(!JS_ValueToInt32(cx,argv[0],&timeout))
			return JS_FALSE;
	}

	rc=JS_SUSPENDREQUEST(cx);
	v=msgQueuePeek(q,timeout);
	JS_RESUMEREQUEST(cx, rc);
	if(v==NULL)
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
	else if(v->name[0])
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(JS_NewStringCopyZ(cx,v->name)));
	else
		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);

	return(JS_TRUE);
}

static JSBool
js_read(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	msg_queue_t* q;
	queued_value_t	find_v;
	queued_value_t*	v;
	int32 timeout=0;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((q=(msg_queue_t*)js_GetClassPrivate(cx, obj, &js_queue_class))==NULL) {
		return(JS_FALSE);
	}

	if(argc && JSVAL_IS_STRING(argv[0])) {	/* value named specified */
		ZERO_VAR(find_v);
		JSVALUE_TO_STRBUF(cx, argv[0], find_v.name, sizeof(find_v.name), NULL);
		rc=JS_SUSPENDREQUEST(cx);
		v=msgQueueFind(q,&find_v,sizeof(find_v.name));
		JS_RESUMEREQUEST(cx, rc);
	} else {
		if(argc && JSVAL_IS_NUMBER(argv[0])) {
			if(!JS_ValueToInt32(cx,argv[0],&timeout))
				return JS_FALSE;
		}
		rc=JS_SUSPENDREQUEST(cx);
		v=msgQueueRead(q, timeout);
		JS_RESUMEREQUEST(cx, rc);
	}

	if(v!=NULL) {
		jsval	rval;

		js_decode_value(cx, obj, v, &rval);
		free(v->value);
		free(v);
		JS_SET_RVAL(cx, arglist, rval);
	}

	return(JS_TRUE);
}

static JSBool
js_peek(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	msg_queue_t*	q;
	queued_value_t*	v;
	int32 timeout=0;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((q=(msg_queue_t*)js_GetClassPrivate(cx, obj, &js_queue_class))==NULL) {
		return(JS_FALSE);
	}

	if(argc && JSVAL_IS_NUMBER(argv[0])) { 	/* timeout specified */
		if(!JS_ValueToInt32(cx,argv[0],&timeout))
			return JS_FALSE;
	}

	rc=JS_SUSPENDREQUEST(cx);
	v=msgQueuePeek(q, timeout);
	JS_RESUMEREQUEST(cx, rc);
	if(v!=NULL) {
		jsval	rval;
		js_decode_value(cx, obj, v, &rval);
		JS_SET_RVAL(cx, arglist, rval);
	}

	return(JS_TRUE);
}

static queued_value_t* js_encode_value(JSContext *cx, jsval val, char* name)
{
	queued_value_t	*v;
	uint64			*serialized;

	if((v=malloc(sizeof(queued_value_t)))==NULL)
		return(NULL);
	memset(v,0,sizeof(queued_value_t));

	if(name!=NULL)
		SAFECOPY(v->name,name);

	if(!JS_WriteStructuredClone(cx, val, &serialized, &v->size, NULL, NULL)) {
		free(v);
		return NULL;
	}
	if((v->value=(uint64 *)malloc(v->size))==NULL) {
		JS_free(cx, serialized);
		free(v);
		return NULL;
	}
	memcpy(v->value, serialized, v->size);
	JS_free(cx, serialized);

	return(v);
}

BOOL js_enqueue_value(JSContext *cx, msg_queue_t* q, jsval val, char* name)
{
	queued_value_t* v;
	BOOL			result;
	jsrefcount		rc;

	if((v=js_encode_value(cx,val,name))==NULL)
		return(FALSE);

	rc=JS_SUSPENDREQUEST(cx);
	result=msgQueueWrite(q,v,sizeof(queued_value_t));
	free(v);
	JS_RESUMEREQUEST(cx, rc);
	return(result);
}

static JSBool
js_write(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	uintN			argn=0;
	msg_queue_t*	q;
	jsval			val;
	char*			name=NULL;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((q=(msg_queue_t*)js_GetClassPrivate(cx, obj, &js_queue_class))==NULL) {
		return(JS_FALSE);
	}

	val = argv[argn++];

	if(argn < argc) {
		JSVALUE_TO_MSTRING(cx, argv[argn], name, NULL);
		argn++;
		HANDLE_PENDING(cx, name);
	}

	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(js_enqueue_value(cx, q, val, name)));
	if(name)
		free(name);

	return(JS_TRUE);
}

/* Queue Object Properites */
enum {
	 QUEUE_PROP_NAME
	,QUEUE_PROP_DATA_WAITING
	,QUEUE_PROP_READ_LEVEL
	,QUEUE_PROP_WRITE_LEVEL
	,QUEUE_PROP_OWNER
	,QUEUE_PROP_ORPHAN
};

#ifdef BUILD_JSDOCS
static char* queue_prop_desc[] = {
	 "name of the queue (if it has one)"
	,"<i>true</i> if data is waiting to be read from queue"
	,"number of values in the read queue"
	,"number of values in the write queue"
	,"<i>true</i> if current thread is the owner/creator of the queue"
	,"<i>true</i> if the owner of the queue has detached from the queue"
	,NULL
};
#endif

static JSBool js_queue_get(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	jsval idval;
    jsint			tiny;
	msg_queue_t*	q;
	jsrefcount		rc;

	if((q=(msg_queue_t*)JS_GetPrivate(cx,obj))==NULL)
		return JS_FALSE;

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	switch(tiny) {
		case QUEUE_PROP_NAME:
			if(q->name[0])
				*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,q->name));
			break;
		case QUEUE_PROP_DATA_WAITING:
			rc=JS_SUSPENDREQUEST(cx);
			*vp = BOOLEAN_TO_JSVAL(INT_TO_BOOL(msgQueueReadLevel(q)));
			JS_RESUMEREQUEST(cx, rc);
			break;
		case QUEUE_PROP_READ_LEVEL:
			rc=JS_SUSPENDREQUEST(cx);
			*vp = INT_TO_JSVAL(msgQueueReadLevel(q));
			JS_RESUMEREQUEST(cx, rc);
			break;
		case QUEUE_PROP_WRITE_LEVEL:
			rc=JS_SUSPENDREQUEST(cx);
			*vp = INT_TO_JSVAL(msgQueueWriteLevel(q));
			JS_RESUMEREQUEST(cx, rc);
			break;
		case QUEUE_PROP_OWNER:
			rc=JS_SUSPENDREQUEST(cx);
			*vp = BOOLEAN_TO_JSVAL(INT_TO_BOOL(msgQueueOwner(q)));
			JS_RESUMEREQUEST(cx, rc);
			break;
		case QUEUE_PROP_ORPHAN:
			rc=JS_SUSPENDREQUEST(cx);
			*vp = BOOLEAN_TO_JSVAL(INT_TO_BOOL(q->flags & MSG_QUEUE_ORPHAN));
			JS_RESUMEREQUEST(cx, rc);
			break;
	}
	return(JS_TRUE);
}

#define QUEUE_PROP_FLAGS JSPROP_ENUMERATE|JSPROP_READONLY

static jsSyncPropertySpec js_queue_properties[] = {
/*		 name				,tinyid					,flags,				ver	*/

	{	"name"				,QUEUE_PROP_NAME		,QUEUE_PROP_FLAGS,	312 },
	{	"data_waiting"		,QUEUE_PROP_DATA_WAITING,QUEUE_PROP_FLAGS,	312 },
	{	"read_level"		,QUEUE_PROP_READ_LEVEL	,QUEUE_PROP_FLAGS,	312 },
	{	"write_level"		,QUEUE_PROP_WRITE_LEVEL	,QUEUE_PROP_FLAGS,	312 },
	{	"owner"				,QUEUE_PROP_OWNER		,QUEUE_PROP_FLAGS,	316 },
	{	"orphan"			,QUEUE_PROP_ORPHAN		,QUEUE_PROP_FLAGS,	31702 },
	{0}
};

static jsSyncMethodSpec js_queue_functions[] = {
	{"poll",		js_poll,		1,	JSTYPE_UNDEF,	"[timeout=<tt>0</tt>]"
	,JSDOCSTR("wait for any value to be written to the queue for up to <i>timeout</i> milliseconds "
		"(default: <i>0</i>), returns <i>true</i> or the <i>name</i> (string) of "
		"the value waiting (if it has one), or <i>false</i> if no values are waiting")
	,312
	},
	{"read",		js_read,		1,	JSTYPE_UNDEF,	"[string name] or [timeout=<tt>0</tt>]"
	,JSDOCSTR("read a value from the queue, if <i>name</i> not specified, reads next value "
		"from the bottom of the queue (waiting up to <i>timeout</i> milliseconds)")
	,313
	},
	{"peek",		js_peek,		1,	JSTYPE_UNDEF,	"[timeout=<tt>0</tt>]"
	,JSDOCSTR("peek at the value at the bottom of the queue, "
		"wait up to <i>timeout</i> milliseconds for any value to be written "
		"(default: <i>0</i>)")
	,313
	},
	{"write",		js_write,		1,	JSTYPE_BOOLEAN,	"value [,name=<i>none</i>]"
	,JSDOCSTR("write a value (optionally named) to the queue")
	,312
	},
	{0}
};

static JSBool js_queue_resolve(JSContext *cx, JSObject *obj, jsid id)
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

	ret=js_SyncResolve(cx, obj, name, js_queue_properties, js_queue_functions, NULL, 0);
	if(name)
		free(name);
	return ret;
}

static JSBool js_queue_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_queue_resolve(cx, obj, JSID_VOID));
}

JSClass js_queue_class = {
     "Queue"				/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_queue_get			/* getProperty	*/
	,JS_StrictPropertyStub	/* setProperty	*/
	,js_queue_enumerate		/* enumerate	*/
	,js_queue_resolve		/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,js_finalize_queue		/* finalize		*/
};

/* Queue Constructor (creates queue) */

static JSBool
js_queue_constructor(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj;
	jsval *argv=JS_ARGV(cx, arglist);
	uintN			argn=0;
	char*			name=NULL;
	int32			flags=MSG_QUEUE_BIDIR;
	msg_queue_t*	q=NULL;
	list_node_t*	n;
	jsrefcount		rc;

	obj=JS_NewObject(cx, &js_queue_class, NULL, NULL);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));

#if 0	/* This doesn't appear to be doing anything but leaking memory */
	if((q=(msg_queue_t*)malloc(sizeof(msg_queue_t)))==NULL) {
		JS_ReportError(cx,"malloc failed");
		return(JS_FALSE);
	}
	memset(q,0,sizeof(msg_queue_t));
#endif

	if(argn<argc && JSVAL_IS_STRING(argv[argn])) {
		JSVALUE_TO_ASTRING(cx, argv[argn], name, sizeof(q->name), NULL);
		argn++;
	}

	if(argn<argc && JSVAL_IS_NUMBER(argv[argn])) {
		if(!JS_ValueToInt32(cx,argv[argn++],&flags))
			return JS_FALSE;
	}

	rc=JS_SUSPENDREQUEST(cx);
	if(name!=NULL) {
		listLock(&named_queues);
		for(n=named_queues.first;n!=NULL;n=n->next)
			if((q=n->data)!=NULL && !stricmp(q->name,name))
				break;
		listUnlock(&named_queues);
		if(n==NULL)
			q=NULL;
	}

	if(q==NULL) {
		q=msgQueueInit(NULL,flags);
		if(name!=NULL)
			SAFECOPY(q->name,name);
		listPushNode(&named_queues,q);
	} else
		msgQueueAttach(q);
	JS_RESUMEREQUEST(cx, rc);

	if(!JS_SetPrivate(cx, obj, q)) {
		JS_ReportError(cx,"JS_SetPrivate failed");
		return(JS_FALSE);
	}

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx,obj,"Class for bi-directional message queues. "
		"Used for inter-thread/module communications.", 312);
	js_DescribeSyncConstructor(cx,obj,"To create a new (named) Queue object: "
		"<tt>var q = new Queue(<i>name</i>)</tt>");
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", queue_prop_desc, JSPROP_READONLY);
#endif

	return(JS_TRUE);
}

JSObject* DLLCALL js_CreateQueueClass(JSContext* cx, JSObject* parent)
{
	JSObject*	obj;

	obj = JS_InitClass(cx, parent, NULL
		,&js_queue_class
		,js_queue_constructor
		,0	/* number of constructor args */
		,NULL /* props, specified in constructor */
		,NULL /* funcs, specified in constructor */
		,NULL
		,NULL);

	return(obj);
}

JSObject* DLLCALL js_CreateQueueObject(JSContext* cx, JSObject* parent, char *name, msg_queue_t* q)
{
	JSObject*		obj;

	if(name==NULL)
	    obj = JS_NewObject(cx, &js_queue_class, NULL, parent);
	else
		obj = JS_DefineObject(cx, parent, name, &js_queue_class, NULL
			,JSPROP_ENUMERATE|JSPROP_READONLY);

	if(obj==NULL)
		return(NULL);

	if(!JS_SetPrivate(cx, obj, q))
		return(NULL);

	return(obj);
}


