/* js_queue.c */

/* Synchronet JavaScript "Queue" Object */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2006 Rob Swindell - http://www.synchro.net/copyright.html		*
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
	char	name[128];
	int		type;
	union {
		JSBool		b;
		jsdouble	n;
		char*		s;
	} value;
} queued_value_t;

link_list_t named_queues;

static const char* getprivate_failure = "line %d %s JS_GetPrivate failed";

/* Queue Destructor */

static void js_finalize_queue(JSContext *cx, JSObject *obj)
{
	msg_queue_t* q;
	list_node_t* n;
	jsrefcount	rc;

	if((q=(msg_queue_t*)JS_GetPrivate(cx,obj))==NULL)
		return;

	rc=JS_SUSPENDREQUEST(cx);
	if(msgQueueDetach(q)==0 && (n=listFindNode(&named_queues,q,/* length=0 for ptr compare */0))!=NULL)
		listRemoveNode(&named_queues,n,FALSE);
	JS_RESUMEREQUEST(cx, rc);

	JS_SetPrivate(cx, obj, NULL);
}

static size_t js_decode_value(JSContext *cx, JSObject *parent
							   ,queued_value_t* v, jsval* rval, BOOL peek)
{
	size_t			count=1;
	size_t			decoded;
	queued_value_t* pv;
	queued_value_t	term;
	jsval	prop_val;
	jsuint	index=0;
	JSObject *obj;

	ZERO_VAR(term);

	*rval = JSVAL_VOID;

	if(v==NULL || v->type==JSTYPE_VOID)
		return(count);

	switch(v->type) {
		case JSTYPE_NULL:
			*rval = JSVAL_NULL;
			break;
		case JSTYPE_BOOLEAN:
			*rval = BOOLEAN_TO_JSVAL(v->value.b);
			break;
		case JSTYPE_NUMBER:
			JS_NewNumberValue(cx,v->value.n,rval);
			break;
		case JSTYPE_STRING:
			if(v->value.s) {
				*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,v->value.s));
				if(!peek)
					free(v->value.s);
			}
			break;
		case JSTYPE_ARRAY:
		case JSTYPE_OBJECT:
			obj = JS_DefineObject(cx, parent, v->name, NULL, NULL
				,JSPROP_ENUMERATE);
			for(pv=v+1,count++;memcmp(pv,&term,sizeof(term));pv+=decoded,count+=decoded) {
				decoded=js_decode_value(cx,obj,pv,&prop_val,peek);
				if(v->type==JSTYPE_ARRAY)
					JS_SetElement(cx,obj,index++,&prop_val);
				else
					JS_DefineProperty(cx, obj, pv->name, prop_val,NULL,NULL,JSPROP_ENUMERATE);
			}
			*rval = OBJECT_TO_JSVAL(obj);
			break;
	}
	return(count);
}

/* Queue Object Methods */

static JSBool
js_poll(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	msg_queue_t*	q;
	queued_value_t*	v;
	int32 timeout=0;
	jsrefcount	rc;

	if((q=(msg_queue_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(argc && JSVAL_IS_NUMBER(argv[0])) 	/* timeout specified */
		JS_ValueToInt32(cx,argv[0],&timeout);

	rc=JS_SUSPENDREQUEST(cx);
	v=msgQueuePeek(q,timeout);
	JS_RESUMEREQUEST(cx, rc);
	if(v==NULL)
		*rval = JSVAL_FALSE;
	else if(v->name!=NULL && v->name[0])
		*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,v->name));
	else
		*rval = JSVAL_TRUE;

	return(JS_TRUE);
}

static JSBool
js_read(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	msg_queue_t* q;
	queued_value_t	find_v;
	queued_value_t*	v;
	int32 timeout=0;
	jsrefcount	rc;

	if((q=(msg_queue_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(JSVAL_IS_STRING(argv[0])) {	/* value named specified */
		ZERO_VAR(find_v);
		SAFECOPY(find_v.name,JS_GetStringBytes(JS_ValueToString(cx,argv[0])));
		rc=JS_SUSPENDREQUEST(cx);
		v=msgQueueFind(q,&find_v,sizeof(find_v.name));
		JS_RESUMEREQUEST(cx, rc);
	} else {
		if(JSVAL_IS_NUMBER(argv[0]))
			JS_ValueToInt32(cx,argv[0],&timeout);
		rc=JS_SUSPENDREQUEST(cx);
		v=msgQueueRead(q, timeout);
		JS_RESUMEREQUEST(cx, rc);
	}

	if(v!=NULL) {
		js_decode_value(cx, obj, v, rval, /* peek */FALSE);
		free(v);
	}

	return(JS_TRUE);
}

static JSBool
js_peek(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	msg_queue_t*	q;
	queued_value_t*	v;
	int32 timeout=0;
	jsrefcount	rc;

	if((q=(msg_queue_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(argc && JSVAL_IS_NUMBER(argv[0])) 	/* timeout specified */
		JS_ValueToInt32(cx,argv[0],&timeout);

	rc=JS_SUSPENDREQUEST(cx);
	v=msgQueuePeek(q, timeout);
	JS_RESUMEREQUEST(cx, rc);
	if(v!=NULL) {
		js_decode_value(cx, obj, v, rval, /* peek */TRUE);
	}

	return(JS_TRUE);
}

static queued_value_t* js_encode_value(JSContext *cx, jsval val, char* name
									   ,queued_value_t* v, size_t* count)
{
	jsint       i;
	jsval		prop_name;
	jsval		prop_val;
    JSObject*	obj;
	JSIdArray*	id_array;
	queued_value_t* nv;

	if((nv=realloc(v,((*count)+1)*sizeof(queued_value_t)))==NULL) {
		if(v) free(v);
		return(NULL);
	}
	v=nv;
	nv=v+(*count);
	memset(nv,0,sizeof(queued_value_t));
	(*count)++;

	if(name!=NULL)
		SAFECOPY(nv->name,name);

	switch(JSVAL_TAG(val)) {
		case JSVAL_BOOLEAN:
			nv->type=JSTYPE_BOOLEAN;
			nv->value.b=JSVAL_TO_BOOLEAN(val);
			break;
		case JSVAL_OBJECT:
			if(JSVAL_IS_NULL(val)) {
				nv->type=JSTYPE_NULL;
				break;
			}
			nv->type=JSTYPE_OBJECT;
			obj = JSVAL_TO_OBJECT(val);

			if(JS_IsArrayObject(cx, obj))
				nv->type=JSTYPE_ARRAY;

			if((id_array=JS_Enumerate(cx,obj))==NULL) {
				free(v);
				return(NULL);
			}
			for(i=0; i<id_array->length; i++)  {
				/* property name */
				JS_IdToValue(cx,id_array->vector[i],&prop_name);
				if(JSVAL_IS_STRING(prop_name)) {
					name=JS_GetStringBytes(JSVAL_TO_STRING(prop_name));
					/* value */
					JS_GetProperty(cx,obj,name,&prop_val);
				} else {
					name=NULL;
					JS_GetElement(cx,obj,i,&prop_val);
				}
				if((v=js_encode_value(cx,prop_val,name,v,count))==NULL)
					break;
			}
			v=js_encode_value(cx,JSVAL_VOID,NULL,v,count);	/* terminate object */
			JS_DestroyIdArray(cx,id_array);
			break;
		default:
			if(JSVAL_IS_NUMBER(val)) {
				nv->type = JSTYPE_NUMBER;
				JS_ValueToNumber(cx,val,&nv->value.n);
			} else if(JSVAL_IS_VOID(val)) {
				nv->type = JSTYPE_VOID;
			} else {
				nv->type= JSTYPE_STRING;
				nv->value.s = strdup(JS_GetStringBytes(JS_ValueToString(cx,val)));
			}
			break;
	}

	return(v);
}

BOOL js_enqueue_value(JSContext *cx, msg_queue_t* q, jsval val, char* name)
{
	queued_value_t* v;
	size_t			count=0;
	BOOL			result;
	jsrefcount		rc;

	if((v=js_encode_value(cx,val,name,NULL,&count))==NULL || count<1)
		return(FALSE);

	rc=JS_SUSPENDREQUEST(cx);
	result=msgQueueWrite(q,v,count*sizeof(queued_value_t));
	free(v);
	JS_RESUMEREQUEST(cx, rc);
	return(result);
}

static JSBool
js_write(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	uintN			argn=0;
	msg_queue_t*	q;
	jsval			val;
	char*			name=NULL;

	if((q=(msg_queue_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	val = argv[argn++];

	if(argn < argc)
		name=JS_GetStringBytes(JS_ValueToString(cx,argv[argn++]));

	*rval = BOOLEAN_TO_JSVAL(js_enqueue_value(cx, q, val, name));

	return(JS_TRUE);
}

/* Queue Object Properites */
enum {
	 QUEUE_PROP_NAME
	,QUEUE_PROP_DATA_WAITING
	,QUEUE_PROP_READ_LEVEL
	,QUEUE_PROP_WRITE_LEVEL
};

#ifdef BUILD_JSDOCS
static char* queue_prop_desc[] = {
	 "name of the queue (if it has one)"
	,"<i>true</i> if data is waiting to be read from queue"
	,"number of values in the read queue"
	,"number of values in the write qeueue"
	,NULL
};
#endif

static JSBool js_queue_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint			tiny;
	msg_queue_t*	q;
	jsrefcount		rc;

	if((q=(msg_queue_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

    tiny = JSVAL_TO_INT(id);

	switch(tiny) {
		case QUEUE_PROP_NAME:
			if(q->name!=NULL && q->name[0])
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

static JSBool js_queue_resolve(JSContext *cx, JSObject *obj, jsval id)
{
	char*			name=NULL;

	if(id != JSVAL_NULL)
		name=JS_GetStringBytes(JSVAL_TO_STRING(id));

	return(js_SyncResolve(cx, obj, name, js_queue_properties, js_queue_functions, NULL, 0));
}

static JSBool js_queue_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_queue_resolve(cx, obj, JSVAL_NULL));
}

static JSClass js_queue_class = {
     "Queue"				/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_queue_get			/* getProperty	*/
	,JS_PropertyStub		/* setProperty	*/
	,js_queue_enumerate		/* enumerate	*/
	,js_queue_resolve		/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,js_finalize_queue		/* finalize		*/
};

/* Queue Constructor (creates queue) */

static JSBool
js_queue_constructor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	uintN			argn=0;
	char*			name=NULL;
	int32			flags=MSG_QUEUE_BIDIR;
	msg_queue_t*	q=NULL;
	list_node_t*	n;
	jsrefcount		rc;

	*rval = JSVAL_VOID;

#if 0	/* This doesn't appear to be doing anything but leaking memory */
	if((q=(msg_queue_t*)malloc(sizeof(msg_queue_t)))==NULL) {
		JS_ReportError(cx,"malloc failed");
		return(JS_FALSE);
	}
	memset(q,0,sizeof(msg_queue_t));
#endif

	if(argn<argc && JSVAL_IS_STRING(argv[argn]))
		name=JS_GetStringBytes(JS_ValueToString(cx,argv[argn++]));

	if(argn<argc && JSVAL_IS_NUMBER(argv[argn]))
		JS_ValueToInt32(cx,argv[argn++],&flags);

	rc=JS_SUSPENDREQUEST(cx);
	if(name!=NULL) {
		for(n=listFirstNode(&named_queues);n!=NULL;n=listNextNode(n))
			if((q=n->data)!=NULL && !stricmp(q->name,name))
				break;
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


