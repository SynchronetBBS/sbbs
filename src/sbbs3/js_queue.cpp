/* Synchronet JavaScript "Queue" Object */

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
#include "msg_queue.h"
#include <js/StructuredClone.h>

typedef struct
{
	char name[128];
	size_t size;
	char *value;  /* SM128: flat byte buffer of JSStructuredCloneData */
} queued_value_t;

link_list_t named_queues;

/* Queue Destructor */

static void js_finalize_queue(JS::GCContext* gcx, JSObject *obj)
{
	msg_queue_t* q;
	list_node_t* n;

	if ((q = (msg_queue_t*)JS_GetPrivate(obj)) == NULL)
		return;

	if (msgQueueDetach(q) == 0 && (n = listFindNode(&named_queues, q, /* length=0 for ptr compare */ 0)) != NULL)
		listRemoveNode(&named_queues, n, false);

	JS_SetPrivate(obj, NULL);
}

static void js_decode_value(JSContext *cx, JSObject *parent
                            , queued_value_t* v, jsval* rval)
{
	*rval = JSVAL_VOID;

	if (v == NULL || v->value == NULL || v->size == 0)
		return;

	/* SM128: reconstruct JSStructuredCloneData from flat bytes, then read */
	JSStructuredCloneData cloneData(JS::StructuredCloneScope::SameProcess);
	size_t remaining = v->size;
	const char* src = v->value;
	while (remaining > 0) {
		size_t got;
		char* dest = cloneData.AllocateBytes(remaining, &got);
		if (!dest || got == 0)
			return;
		memcpy(dest, src, got);
		src += got;
		remaining -= got;
	}

	JS::RootedValue result(cx);
	JS::CloneDataPolicy policy;
	JS_ReadStructuredClone(cx, cloneData, 8 /* JS_STRUCTURED_CLONE_VERSION */,
	                       JS::StructuredCloneScope::SameProcess,
	                       &result, policy, nullptr, nullptr);
	*rval = result;
}

/* Queue Object Methods */

extern JSClass js_queue_class;

static JSBool
js_poll(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *      obj = JS_THIS_OBJECT(cx, arglist);
	jsval *         argv = JS_ARGV(cx, arglist);
	msg_queue_t*    q;
	queued_value_t* v;
	int32           timeout = 0;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((q = (msg_queue_t*)js_GetClassPrivate(cx, obj, &js_queue_class)) == NULL) {
		return JS_FALSE;
	}

	if (argc && JSVAL_IS_NUMBER(argv[0])) {  /* timeout specified */
		if (!JS_ValueToInt32(cx, argv[0], &timeout))
			return JS_FALSE;
	}

	v = static_cast<queued_value_t *>(msgQueuePeek(q, timeout));
	if (v == NULL)
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
	else if (v->name[0])
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, v->name)));
	else
		JS_SET_RVAL(cx, arglist, JSVAL_TRUE);

	return JS_TRUE;
}

static JSBool
js_read(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *      obj = JS_THIS_OBJECT(cx, arglist);
	jsval *         argv = JS_ARGV(cx, arglist);
	msg_queue_t*    q;
	queued_value_t  find_v;
	queued_value_t* v;
	int32           timeout = 0;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((q = (msg_queue_t*)js_GetClassPrivate(cx, obj, &js_queue_class)) == NULL) {
		return JS_FALSE;
	}

	if (argc && JSVAL_IS_STRING(argv[0])) {  /* value named specified */
		ZERO_VAR(find_v);
		JSVALUE_TO_STRBUF(cx, argv[0], find_v.name, sizeof(find_v.name), NULL);
		v = static_cast<queued_value_t *>(msgQueueFind(q, &find_v, sizeof(find_v.name)));
	} else {
		if (argc && JSVAL_IS_NUMBER(argv[0])) {
			if (!JS_ValueToInt32(cx, argv[0], &timeout))
				return JS_FALSE;
		}
		v = static_cast<queued_value_t *>(msgQueueRead(q, timeout));
	}

	if (v != NULL) {
		jsval rval;

		js_decode_value(cx, obj, v, &rval);
		free(v->value);
		free(v);
		JS_SET_RVAL(cx, arglist, rval);
	}

	return JS_TRUE;
}

static JSBool
js_peek(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *      obj = JS_THIS_OBJECT(cx, arglist);
	jsval *         argv = JS_ARGV(cx, arglist);
	msg_queue_t*    q;
	queued_value_t* v;
	int32           timeout = 0;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((q = (msg_queue_t*)js_GetClassPrivate(cx, obj, &js_queue_class)) == NULL) {
		return JS_FALSE;
	}

	if (argc && JSVAL_IS_NUMBER(argv[0])) {  /* timeout specified */
		if (!JS_ValueToInt32(cx, argv[0], &timeout))
			return JS_FALSE;
	}

	v = static_cast<queued_value_t *>(msgQueuePeek(q, timeout));
	if (v != NULL) {
		jsval rval;
		js_decode_value(cx, obj, v, &rval);
		JS_SET_RVAL(cx, arglist, rval);
	}

	return JS_TRUE;
}

static queued_value_t* js_encode_value(JSContext *cx, jsval val, char* name)
{
	queued_value_t *v;

	if ((v = static_cast<queued_value_t *>(malloc(sizeof(queued_value_t)))) == NULL)
		return NULL;
	memset(v, 0, sizeof(queued_value_t));

	if (name != NULL)
		SAFECOPY(v->name, name);

	/* SM128: write to JSStructuredCloneData, then flatten to a raw byte buffer */
	JSStructuredCloneData cloneData(JS::StructuredCloneScope::SameProcess);
	JS::RootedValue rv(cx, val);
	JS::CloneDataPolicy policy;
	if (!JS_WriteStructuredClone(cx, rv, &cloneData,
	                             JS::StructuredCloneScope::SameProcess,
	                             policy, nullptr, nullptr,
	                             JS::UndefinedHandleValue)) {
		free(v);
		return NULL;
	}

	/* Calculate total size */
	v->size = 0;
	cloneData.ForEachDataChunk([&](const char* d, size_t sz) {
		v->size += sz;
		return true;
	});

	if ((v->value = (char*)malloc(v->size)) == NULL) {
		free(v);
		return NULL;
	}

	/* Flatten into the buffer */
	size_t offset = 0;
	cloneData.ForEachDataChunk([&](const char* d, size_t sz) {
		memcpy(v->value + offset, d, sz);
		offset += sz;
		return true;
	});

	return v;
}

bool js_enqueue_value(JSContext *cx, msg_queue_t* q, jsval val, char* name)
{
	queued_value_t* v;
	bool            result;

	if ((v = js_encode_value(cx, val, name)) == NULL)
		return false;

	result = msgQueueWrite(q, v, sizeof(queued_value_t));
	free(v);
	return result;
}

static JSBool
js_write(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *   obj = JS_THIS_OBJECT(cx, arglist);
	jsval *      argv = JS_ARGV(cx, arglist);
	uintN        argn = 0;
	msg_queue_t* q;
	jsval        val;
	char*        name = NULL;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((q = (msg_queue_t*)js_GetClassPrivate(cx, obj, &js_queue_class)) == NULL) {
		return JS_FALSE;
	}

	val = argv[argn++];

	if (argn < argc) {
		JSVALUE_TO_MSTRING(cx, argv[argn], name, NULL);
		argn++;
		HANDLE_PENDING(cx, name);
	}

	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(js_enqueue_value(cx, q, val, name)));
	if (name)
		free(name);

	return JS_TRUE;
}

/* Queue Object Properites */
enum {
	QUEUE_PROP_NAME
	, QUEUE_PROP_DATA_WAITING
	, QUEUE_PROP_READ_LEVEL
	, QUEUE_PROP_WRITE_LEVEL
	, QUEUE_PROP_OWNER
	, QUEUE_PROP_ORPHAN
};

#ifdef BUILD_JSDOCS
static const char* queue_prop_desc[] = {
	"Name of the queue (if it has one)"
	, "<tt>true</tt> if data is waiting to be read from queue"
	, "Number of values in the read queue"
	, "Number of values in the write queue"
	, "<tt>true</tt> if current thread is the owner/creator of the queue"
	, "<tt>true</tt> if the owner of the queue has detached from the queue"
	, NULL
};
#endif

static JSBool js_queue_get_value(JSContext* cx, msg_queue_t* q, jsint tiny, jsval* vp)
{
	JS::RootedString  js_str(cx);

	switch (tiny) {
		case QUEUE_PROP_NAME:
			if (q->name[0]) {
				if ((js_str = JS_NewStringCopyZ(cx, q->name)) == nullptr)
					return JS_FALSE;
				*vp = STRING_TO_JSVAL(js_str.get());
			}
			break;
		case QUEUE_PROP_DATA_WAITING:
			*vp = BOOLEAN_TO_JSVAL(INT_TO_BOOL(msgQueueReadLevel(q)));
			break;
		case QUEUE_PROP_READ_LEVEL:
			*vp = INT_TO_JSVAL(msgQueueReadLevel(q));
			break;
		case QUEUE_PROP_WRITE_LEVEL:
			*vp = INT_TO_JSVAL(msgQueueWriteLevel(q));
			break;
		case QUEUE_PROP_OWNER:
			*vp = BOOLEAN_TO_JSVAL(INT_TO_BOOL(msgQueueOwner(q)));
			break;
		case QUEUE_PROP_ORPHAN:
			*vp = BOOLEAN_TO_JSVAL(INT_TO_BOOL(q->flags & MSG_QUEUE_ORPHAN));
			break;
		default:
			return JS_TRUE;
	}
	return JS_TRUE;
}

#define QUEUE_PROP_FLAGS JSPROP_ENUMERATE | JSPROP_READONLY

static jsSyncPropertySpec js_queue_properties[] = {
/*		 name				,tinyid					,flags,				ver	*/

	{   "name", QUEUE_PROP_NAME, QUEUE_PROP_FLAGS,  312 },
	{   "data_waiting", QUEUE_PROP_DATA_WAITING, QUEUE_PROP_FLAGS,  312 },
	{   "read_level", QUEUE_PROP_READ_LEVEL, QUEUE_PROP_FLAGS,  312 },
	{   "write_level", QUEUE_PROP_WRITE_LEVEL, QUEUE_PROP_FLAGS,  312 },
	{   "owner", QUEUE_PROP_OWNER, QUEUE_PROP_FLAGS,  316 },
	{   "orphan", QUEUE_PROP_ORPHAN, QUEUE_PROP_FLAGS,  31702 },
	{0}
};

static jsSyncMethodSpec js_queue_functions[] = {
	{"poll",        js_poll,        1,  JSTYPE_UNDEF,   "[<i>number</i> timeout=0]"
	 , JSDOCSTR("Wait for any value to be written to the queue for up to <i>timeout</i> milliseconds "
		        "(default: <i>0</i>), returns <tt>true</tt> or the <i>name</i> (string) of "
		        "the value waiting (if it has one), or <tt>false</tt> if no values are waiting")
	 , 312},
	{"read",        js_read,        1,  JSTYPE_UNDEF,   "[<i>string</i> name] or [<i>number</i>timeout=0]"
	 , JSDOCSTR("Read a value from the queue, if <i>name</i> not specified, reads next value "
		        "from the bottom of the queue (waiting up to <i>timeout</i> milliseconds)")
	 , 313},
	{"peek",        js_peek,        1,  JSTYPE_UNDEF,   "[<i>number</i> timeout=0]"
	 , JSDOCSTR("Peek at the value at the bottom of the queue, "
		        "wait up to <i>timeout</i> milliseconds for any value to be written "
		        "(default: <i>0</i>)")
	 , 313},
	{"write",       js_write,       1,  JSTYPE_BOOLEAN, "value [,name=<i>none</i>]"
	 , JSDOCSTR("Write a value (optionally named) to the queue")
	 , 312},
	{0}
};

static bool js_queue_prop_getter(JSContext* cx, unsigned argc, JS::Value* vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject thisObj(cx);
	if (!args.computeThis(cx, &thisObj))
		return false;
	msg_queue_t* q = (msg_queue_t*)js_GetClassPrivate(cx, thisObj, &js_queue_class);
	if (q == nullptr) {
		args.rval().setUndefined();
		return true;
	}
	JSObject* callee = &args.callee();
	jsint tiny = js::GetFunctionNativeReserved(callee, 0).toInt32();
	jsval val = JSVAL_VOID;
	if (!js_queue_get_value(cx, q, tiny, &val))
		return false;
	args.rval().set(val);
	return true;
}

static JSBool js_queue_fill_properties(JSContext* cx, JSObject* obj, msg_queue_t* q, const char* name)
{
	(void)q;
	return js_DefineSyncAccessors(cx, obj, js_queue_properties, js_queue_prop_getter, name);
}

static bool js_queue_resolve(JSContext *cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id, bool* resolvedp)
{
	char*        name = NULL;
	msg_queue_t* q;

	if (id.get().isString()) {
		JSString* str = id.get().toString();
		JSSTRING_TO_MSTRING(cx, str, name, NULL);
		HANDLE_PENDING(cx, name);
		if (name == NULL) return false;
	}

	bool ret = js_SyncResolve(cx, obj, name, js_queue_properties, js_queue_functions, NULL, 0);
	if (ret && (q = (msg_queue_t*)JS_GetPrivate(cx, obj)) != NULL)
		js_queue_fill_properties(cx, obj, q, name);
	if (name)
		free(name);
	if (resolvedp) *resolvedp = ret;
	if (JS_IsExceptionPending(cx))
		JS_ClearPendingException(cx);
	return true;
}

static bool js_queue_enumerate(JSContext *cx, JS::Handle<JSObject*> obj)
{
	msg_queue_t* q;

	if (!js_SyncResolve(cx, obj, NULL, js_queue_properties, js_queue_functions, NULL, 0))
		return false;
	if ((q = (msg_queue_t*)JS_GetPrivate(cx, obj)) != NULL)
		js_queue_fill_properties(cx, obj, q, NULL);
	return true;
}

static const JSClassOps js_queue_classops = {
	nullptr,                /* addProperty  */
	nullptr,                /* delProperty  */
	js_queue_enumerate,     /* enumerate    */
	nullptr,                /* newEnumerate */
	js_queue_resolve,       /* resolve      */
	nullptr,                /* mayResolve   */
	js_finalize_queue,      /* finalize     */
	nullptr, nullptr, nullptr /* call, construct, trace */
};

JSClass js_queue_class = {
	"Queue"
	, JSCLASS_HAS_PRIVATE | JSCLASS_FOREGROUND_FINALIZE
	, &js_queue_classops
};

/* Queue Constructor (creates queue) */

static JSBool
js_queue_constructor(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *   obj;
	jsval *      argv = JS_ARGV(cx, arglist);
	uintN        argn = 0;
	char*        name = NULL;
	int32        flags = MSG_QUEUE_BIDIR;
	msg_queue_t* q = NULL;
	list_node_t* n;

	obj = JS_NewObject(cx, &js_queue_class, NULL, NULL);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));

#if 0   /* This doesn't appear to be doing anything but leaking memory */
	if ((q = (msg_queue_t*)malloc(sizeof(msg_queue_t))) == NULL) {
		JS_ReportError(cx, "malloc failed");
		return JS_FALSE;
	}
	memset(q, 0, sizeof(msg_queue_t));
#endif

	if (argn < argc && JSVAL_IS_STRING(argv[argn])) {
		JSVALUE_TO_ASTRING(cx, argv[argn], name, sizeof(q->name), NULL);
		argn++;
	}

	if (argn < argc && JSVAL_IS_NUMBER(argv[argn])) {
		if (!JS_ValueToInt32(cx, argv[argn++], &flags))
			return JS_FALSE;
	}

	if (name != NULL) {
		listLock(&named_queues);
		for (n = named_queues.first; n != NULL; n = n->next)
			if ((q = static_cast<msg_queue_t *>(n->data)) != NULL && !stricmp(q->name, name))
				break;
		listUnlock(&named_queues);
		if (n == NULL)
			q = NULL;
	}

	if (q == NULL) {
		q = msgQueueInit(NULL, flags);
		if (name != NULL)
			SAFECOPY(q->name, name);
		listPushNode(&named_queues, q);
	} else
		msgQueueAttach(q);

	if (!JS_SetPrivate(cx, obj, q)) {
		JS_ReportError(cx, "JS_SetPrivate failed");
		return JS_FALSE;
	}

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx, obj, "Class for bi-directional message queues. "
	                      "Used for inter-thread/module communications.", 312);
	js_DescribeSyncConstructor(cx, obj, "To create a new (named) Queue object: "
	                           "<tt>var q = new Queue(<i>name</i>)</tt>");
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", queue_prop_desc, JSPROP_READONLY);
#endif

	return JS_TRUE;
}

JSObject* js_CreateQueueClass(JSContext* cx, JSObject* parent)
{
	JSObject* obj;

	obj = JS_InitClass(cx, parent, NULL
	                   , &js_queue_class
	                   , js_queue_constructor
	                   , 0 /* number of constructor args */
	                   , NULL /* props, specified in constructor */
	                   , NULL /* funcs, specified in constructor */
	                   , NULL
	                   , NULL);

	return obj;
}

JSObject* js_CreateQueueObject(JSContext* cx, JSObject* parent, const char *name, msg_queue_t* q)
{
	JSObject* obj;

	if (name == NULL)
		obj = JS_NewObject(cx, &js_queue_class, NULL, parent);
	else
		obj = JS_DefineObject(cx, parent, name, &js_queue_class, NULL
		                      , JSPROP_ENUMERATE | JSPROP_READONLY);

	if (obj == NULL)
		return NULL;

	if (!JS_SetPrivate(cx, obj, q))
		return NULL;

	return obj;
}


