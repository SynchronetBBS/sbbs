/* js_queue.c */

/* Synchronet JavaScript "Queue" Object */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2004 Rob Swindell - http://www.synchro.net/copyright.html		*
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

typedef struct
{
	char	name[128];
	int		type;
	size_t	length;
	union {
		JSBool		b;
		int32		i;
		jsdouble	d;
		char*		s;
	} value;
} queued_value_t;

static link_list_t named_queues;

static const char* getprivate_failure = "line %d %s JS_GetPrivate failed";

/* Queue Destructor */

static void js_finalize_queue(JSContext *cx, JSObject *obj)
{
	msg_queue_t* q;
	list_node_t* n;

	if((q=(msg_queue_t*)JS_GetPrivate(cx,obj))==NULL)
		return;
	
	if(msgQueueDetach(q)==0 && (n=listFindNode(&named_queues,q,0))!=NULL)
		listRemoveNode(&named_queues,n);

	JS_SetPrivate(cx, obj, NULL);
}

static void parse_queued_value(JSContext *cx, queued_value_t* v, jsval* rval, BOOL peek)
{
	*rval = JSVAL_VOID;

	if(v==NULL)
		return;

	switch(v->type) {
		case JSVAL_BOOLEAN:
			*rval = BOOLEAN_TO_JSVAL(v->value.b);
			break;
		case JSVAL_INT:
			JS_NewNumberValue(cx,v->value.i,rval);
			break;
		case JSVAL_DOUBLE:
			JS_NewNumberValue(cx,v->value.d,rval);
			break;
		case JSVAL_STRING:
			if(v->value.s) {
				*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,v->value.s));
				if(!peek)
					free(v->value.s);
			}
			break;
	}
	free(v);
}

/* Queue Object Methods */

static JSBool
js_wait(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	msg_queue_t* q;
	int32 timeout=0;

	if((q=(msg_queue_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(argc) 	/* timeout specified */
		JS_ValueToInt32(cx,argv[0],&timeout);

	*rval = BOOLEAN_TO_JSVAL(msgQueueWait(q, timeout));

	return(JS_TRUE);
}

static JSBool
js_read(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	msg_queue_t* q;
	queued_value_t	find_v;
	queued_value_t*	v;

	if((q=(msg_queue_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	if(argc) {	/* value named specified */
		ZERO_VAR(find_v);
		SAFECOPY(find_v.name,JS_GetStringBytes(JS_ValueToString(cx,argv[0])));
		v=msgQueueFind(q,&find_v,sizeof(find_v.name));
	} else
		v=msgQueueRead(q, /* timeout */0);

	parse_queued_value(cx, v, rval, /* peek */FALSE);

	return(JS_TRUE);
}

static JSBool
js_peek(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	msg_queue_t*	q;
	queued_value_t*	v;

	if((q=(msg_queue_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

	v=msgQueuePeek(q, /* timeout */0);

	parse_queued_value(cx, v, rval, /* peek */TRUE);

	return(JS_TRUE);
}

BOOL js_enqueue_value(JSContext *cx, msg_queue_t* q, jsval val, char* name)
{
	queued_value_t v;

	ZERO_VAR(v);

	if(name!=NULL)
		SAFECOPY(v.name,name);

	switch(v.type=JSVAL_TAG(val)) {
		case JSVAL_BOOLEAN:
			v.length = sizeof(JSBool);
			v.value.b=JSVAL_TO_BOOLEAN(val);
			break;
		case JSVAL_DOUBLE:
			v.length =sizeof(jsdouble);
			v.value.d = *JSVAL_TO_DOUBLE(val);
			break;
		default:
			if(JSVAL_IS_NUMBER(val)) {
				v.type = JSVAL_INT;
				JS_ValueToInt32(cx,val,&v.value.i);
			} else {
				v.type= JSVAL_STRING;
				v.value.s = strdup(JS_GetStringBytes(JS_ValueToString(cx,val)));
			}
			break;
	}
	return(msgQueueWrite(q,&v,sizeof(v)));
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
	 QUEUE_PROP_READ_LEVEL
	,QUEUE_PROP_WRITE_LEVEL
};

#ifdef _DEBUG
static char* queue_prop_desc[] = {
	 "number of values in the read queue - <small>READ ONLY</small>"
	,"number of values in the write qeueue - <small>READ ONLY</small>"
	,NULL
};
#endif

static JSBool js_queue_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint			tiny;
	msg_queue_t*	q;

	if((q=(msg_queue_t*)JS_GetPrivate(cx,obj))==NULL) {
		JS_ReportError(cx,getprivate_failure,WHERE);
		return(JS_FALSE);
	}

    tiny = JSVAL_TO_INT(id);

	switch(tiny) {
		case QUEUE_PROP_READ_LEVEL:
			*vp = INT_TO_JSVAL(msgQueueReadLevel(q));
			break;
		case QUEUE_PROP_WRITE_LEVEL:
			*vp = INT_TO_JSVAL(msgQueueWriteLevel(q));
			break;
	}
	return(JS_TRUE);
}

#define QUEUE_PROP_FLAGS JSPROP_ENUMERATE|JSPROP_READONLY

static jsSyncPropertySpec js_queue_properties[] = {
/*		 name				,tinyid					,flags,				ver	*/

	{	"read_level"		,QUEUE_PROP_READ_LEVEL	,QUEUE_PROP_FLAGS,	312 },
	{	"write_level"		,QUEUE_PROP_WRITE_LEVEL	,QUEUE_PROP_FLAGS,	312 },
	{0}
};

static JSClass js_queue_class = {
     "Queue"				/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_queue_get			/* getProperty	*/
	,JS_PropertyStub		/* setProperty	*/
	,JS_EnumerateStub		/* enumerate	*/
	,JS_ResolveStub			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,js_finalize_queue		/* finalize		*/
};

static jsSyncMethodSpec js_queue_functions[] = {
	{"wait",		js_wait,		0,	JSTYPE_BOOLEAN,	"[timeout]"
	,JSDOCSTR("wait for a value on the queue up-to <i>timeout</i> milliseconds "
		"(default: <i>infinite</i>)")
	,312
	},
	{"read",		js_read,		0,	JSTYPE_VOID,	"[name]"
	,JSDOCSTR("read a value from the queue, if <i>name</i> not specified, reads next value "
		"from the bottom of the queue")
	,312
	},
	{"peek",		js_peek,		0,	JSTYPE_VOID,	""
	,JSDOCSTR("peek at the value at the bottom of the queue")
	,312
	},
	{"write",		js_write,		0,	JSTYPE_BOOLEAN,	"value [,name]"
	,JSDOCSTR("write a value (optionally named) to the queue")
	,312
	},
	{0}
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

	*rval = JSVAL_VOID;

	if((q=(msg_queue_t*)malloc(sizeof(msg_queue_t)))==NULL) {
		JS_ReportError(cx,"malloc failed");
		return(JS_FALSE);
	}
	memset(q,0,sizeof(msg_queue_t));

	if(argn<argc && JSVAL_IS_STRING(argv[argn]))
		name=JS_GetStringBytes(JS_ValueToString(cx,argv[argn++]));

	if(argn<argc && JSVAL_IS_NUMBER(argv[argn]))
		JS_ValueToInt32(cx,argv[argn++],&flags);

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

	if(!JS_SetPrivate(cx, obj, q)) {
		JS_ReportError(cx,"JS_SetPrivate failed");
		return(JS_FALSE);
	}

	if(!js_DefineSyncProperties(cx, obj, js_queue_properties)) {
		JS_ReportError(cx,"js_DefineSyncProperties failed");
		return(JS_FALSE);
	}

	if(!js_DefineSyncMethods(cx, obj, js_queue_functions, FALSE)) {
		JS_ReportError(cx,"js_DefineSyncMethods failed");
		return(JS_FALSE);
	}

#ifdef _DEBUG
	js_DescribeSyncObject(cx,obj,"Class used for uni-directional and bi-directional message queues. "
		"Used for inter-thread/module communications.",312);
	js_DescribeSyncConstructor(cx,obj,"To create a new Queue object: "
		"<tt>var q = new Queue(<i>name</i>,<i>flags</i>)</tt><br>"
		"where <i>flags</i> = <tt>SOCK_STREAM</tt> for TCP (default) or <tt>SOCK_DGRAM</tt> for UDP");
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

	if(!js_DefineSyncProperties(cx, obj, js_queue_properties))
		return(NULL);

	if(!JS_SetPrivate(cx, obj, q))
		return(NULL);

	if (!js_DefineSyncMethods(cx, obj, js_queue_functions, FALSE)) 
		return(NULL);

	return(obj);
}


