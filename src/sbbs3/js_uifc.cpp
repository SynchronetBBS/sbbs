/* Synchronet "uifc" (user interface) object */

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
#ifndef JAVASCRIPT
#define JAVASCRIPT
#endif
#include "sbbs.h"
#include "uifc.h"
#include "ciolib.h"
struct list_ctx_private {
	int cur;
	int bar;
	int left;
	int top;
	int width;
};

struct showbuf_ctx_private {
	int cur;
	int bar;
	int left;
	int top;
	int width;
	int height;
};
struct getstrxy_ctx_private {
	int lastkey;
};
enum {
	PROP_CUR
	, PROP_BAR
	, PROP_LEFT
	, PROP_TOP
	, PROP_WIDTH
	, PROP_HEIGHT
	, PROP_LASTKEY
};
static JSBool js_list_ctx_get_value(JSContext *cx, struct list_ctx_private* p, jsint tiny, jsval *vp)
{
	switch (tiny) {
		case PROP_CUR:
			*vp = INT_TO_JSVAL(p->cur);
			break;
		case PROP_BAR:
			*vp = INT_TO_JSVAL(p->bar);
			break;
		case PROP_LEFT:
			*vp = INT_TO_JSVAL(p->left);
			break;
		case PROP_TOP:
			*vp = INT_TO_JSVAL(p->top);
			break;
		case PROP_WIDTH:
			*vp = INT_TO_JSVAL(p->width);
			break;
	}
	return JS_TRUE;
}
static JSBool js_list_ctx_set_value(JSContext *cx, struct list_ctx_private* p, jsint tiny, jsval *vp)
{
	int32                    i = 0;
	if (!JS_ValueToInt32(cx, *vp, &i))
		return JS_FALSE;
	switch (tiny) {
		case PROP_CUR:
			p->cur = i;
			break;
		case PROP_BAR:
			p->bar = i;
			break;
		case PROP_LEFT:
			p->left = i;
			break;
		case PROP_TOP:
			p->top = i;
			break;
		case PROP_WIDTH:
			p->width = i;
			break;
	}
	return JS_TRUE;
}
static JSBool js_showbuf_ctx_get_value(JSContext *cx, struct showbuf_ctx_private* p, jsint tiny, jsval *vp)
{
	switch (tiny) {
		case PROP_CUR:
			*vp = INT_TO_JSVAL(p->cur);
			break;
		case PROP_BAR:
			*vp = INT_TO_JSVAL(p->bar);
			break;
		case PROP_LEFT:
			*vp = INT_TO_JSVAL(p->left);
			break;
		case PROP_TOP:
			*vp = INT_TO_JSVAL(p->top);
			break;
		case PROP_WIDTH:
			*vp = INT_TO_JSVAL(p->width);
			break;
		case PROP_HEIGHT:
			*vp = INT_TO_JSVAL(p->height);
			break;
	}
	return JS_TRUE;
}
static JSBool js_showbuf_ctx_set_value(JSContext *cx, struct showbuf_ctx_private* p, jsint tiny, jsval *vp)
{
	int32                       i = 0;
	if (!JS_ValueToInt32(cx, *vp, &i))
		return JS_FALSE;
	switch (tiny) {
		case PROP_CUR:
			p->cur = i;
			break;
		case PROP_BAR:
			p->bar = i;
			break;
		case PROP_LEFT:
			p->left = i;
			break;
		case PROP_TOP:
			p->top = i;
			break;
		case PROP_WIDTH:
			p->width = i;
			break;
		case PROP_HEIGHT:
			p->height = i;
			break;
	}
	return JS_TRUE;
}
static JSBool js_getstrxy_ctx_get_value(JSContext *cx, struct getstrxy_ctx_private* p, jsint tiny, jsval *vp)
{
	switch (tiny) {
		case PROP_LASTKEY:
			*vp = INT_TO_JSVAL(p->lastkey);
			break;
	}
	return JS_TRUE;
}
static JSBool js_getstrxy_ctx_set_value(JSContext *cx, struct getstrxy_ctx_private* p, jsint tiny, jsval *vp)
{
	int32                        i = 0;
	if (!JS_ValueToInt32(cx, *vp, &i))
		return JS_FALSE;
	switch (tiny) {
		case PROP_LASTKEY:
			p->lastkey = i;
			break;
	}
	return JS_TRUE;
}
#ifdef BUILD_JSDOCS
static const char* uifc_list_ctx_prop_desc[] = {
	"Currently selected item"
	, "0-based Line number in the currently displayed set that is highlighted"
	, "left column"
	, "top line"
	, "forced width"
	, NULL
};
static const char* uifc_showbuf_ctx_prop_desc[] = {
	"Currently selected item"
	, "0-based Line number in the currently displayed set that is highlighted"
	, "left column"
	, "top line"
	, "forced width"
	, "forced height"
	, NULL
};
static const char* uifc_getstrxy_ctx_prop_desc[] = {
	"Last pressed key"
	, NULL
};
#endif
/* Destructor */
static void
js_list_ctx_finalize(JS::GCContext *cx, JSObject *obj)
{
	struct list_ctx_private* p;
	if ((p = (struct list_ctx_private*)JS_GetPrivate(obj)) == NULL)
		return;
	free(p);
	JS_SetPrivate(obj, NULL);
}
static void
js_showbuf_ctx_finalize(JS::GCContext *cx, JSObject *obj)
{
	struct showbuf_ctx_private* p;
	if ((p = (struct showbuf_ctx_private*)JS_GetPrivate(obj)) == NULL)
		return;
	free(p);
	JS_SetPrivate(obj, NULL);
}
static void
js_getstrxy_ctx_finalize(JS::GCContext *cx, JSObject *obj)
{
	struct getstrxy_ctx_private* p;
	if ((p = (struct getstrxy_ctx_private*)JS_GetPrivate(obj)) == NULL)
		return;
	free(p);
	JS_SetPrivate(obj, NULL);
}
static bool js_list_ctx_prop_getter(JSContext* cx, unsigned argc, JS::Value* vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject thisObj(cx);
	if (!args.computeThis(cx, &thisObj))
		return false;
	struct list_ctx_private* p = (struct list_ctx_private*)JS_GetPrivate(thisObj);
	if (p == nullptr) { args.rval().setUndefined(); return true; }
	JSObject* callee = &args.callee();
	jsint tiny = js::GetFunctionNativeReserved(callee, 0).toInt32();
	jsval val = JSVAL_VOID;
	if (!js_list_ctx_get_value(cx, p, tiny, &val)) return false;
	args.rval().set(val);
	return true;
}
static bool js_list_ctx_prop_setter(JSContext* cx, unsigned argc, JS::Value* vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject thisObj(cx);
	if (!args.computeThis(cx, &thisObj))
		return false;
	struct list_ctx_private* p = (struct list_ctx_private*)JS_GetPrivate(thisObj);
	if (p == nullptr) return true;
	JSObject* callee = &args.callee();
	jsint tiny = js::GetFunctionNativeReserved(callee, 0).toInt32();
	jsval val = args.length() > 0 ? args[0] : JSVAL_VOID;
	if (!js_list_ctx_set_value(cx, p, tiny, &val)) return false;
	args.rval().set(val);
	return true;
}
static bool js_showbuf_ctx_prop_getter(JSContext* cx, unsigned argc, JS::Value* vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject thisObj(cx);
	if (!args.computeThis(cx, &thisObj))
		return false;
	struct showbuf_ctx_private* p = (struct showbuf_ctx_private*)JS_GetPrivate(thisObj);
	if (p == nullptr) { args.rval().setUndefined(); return true; }
	JSObject* callee = &args.callee();
	jsint tiny = js::GetFunctionNativeReserved(callee, 0).toInt32();
	jsval val = JSVAL_VOID;
	if (!js_showbuf_ctx_get_value(cx, p, tiny, &val)) return false;
	args.rval().set(val);
	return true;
}
static bool js_showbuf_ctx_prop_setter(JSContext* cx, unsigned argc, JS::Value* vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject thisObj(cx);
	if (!args.computeThis(cx, &thisObj))
		return false;
	struct showbuf_ctx_private* p = (struct showbuf_ctx_private*)JS_GetPrivate(thisObj);
	if (p == nullptr) return true;
	JSObject* callee = &args.callee();
	jsint tiny = js::GetFunctionNativeReserved(callee, 0).toInt32();
	jsval val = args.length() > 0 ? args[0] : JSVAL_VOID;
	if (!js_showbuf_ctx_set_value(cx, p, tiny, &val)) return false;
	args.rval().set(val);
	return true;
}
static bool js_getstrxy_ctx_prop_getter(JSContext* cx, unsigned argc, JS::Value* vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject thisObj(cx);
	if (!args.computeThis(cx, &thisObj))
		return false;
	struct getstrxy_ctx_private* p = (struct getstrxy_ctx_private*)JS_GetPrivate(thisObj);
	if (p == nullptr) { args.rval().setUndefined(); return true; }
	JSObject* callee = &args.callee();
	jsint tiny = js::GetFunctionNativeReserved(callee, 0).toInt32();
	jsval val = JSVAL_VOID;
	if (!js_getstrxy_ctx_get_value(cx, p, tiny, &val)) return false;
	args.rval().set(val);
	return true;
}
static bool js_getstrxy_ctx_prop_setter(JSContext* cx, unsigned argc, JS::Value* vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject thisObj(cx);
	if (!args.computeThis(cx, &thisObj))
		return false;
	struct getstrxy_ctx_private* p = (struct getstrxy_ctx_private*)JS_GetPrivate(thisObj);
	if (p == nullptr) return true;
	JSObject* callee = &args.callee();
	jsint tiny = js::GetFunctionNativeReserved(callee, 0).toInt32();
	jsval val = args.length() > 0 ? args[0] : JSVAL_VOID;
	if (!js_getstrxy_ctx_set_value(cx, p, tiny, &val)) return false;
	args.rval().set(val);
	return true;
}
static const JSClassOps js_uifc_list_ctx_classops = {
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	js_list_ctx_finalize,
	nullptr, nullptr, nullptr
};
static JSClass            js_uifc_list_ctx_class = {
	"CTX"
	, JSCLASS_HAS_PRIVATE | JSCLASS_FOREGROUND_FINALIZE
	, &js_uifc_list_ctx_classops
};
static const JSClassOps js_uifc_showbuf_ctx_classops = {
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	js_showbuf_ctx_finalize,
	nullptr, nullptr, nullptr
};
static JSClass            js_uifc_showbuf_ctx_class = {
	"CTX"
	, JSCLASS_HAS_PRIVATE | JSCLASS_FOREGROUND_FINALIZE
	, &js_uifc_showbuf_ctx_classops
};
static const JSClassOps js_uifc_getstrxy_ctx_classops = {
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	js_getstrxy_ctx_finalize,
	nullptr, nullptr, nullptr
};
static JSClass            js_uifc_getstrxy_ctx_class = {
	"CTX"
	, JSCLASS_HAS_PRIVATE | JSCLASS_FOREGROUND_FINALIZE
	, &js_uifc_getstrxy_ctx_classops
};
static jsSyncPropertySpec js_uifc_list_class_properties[] = {
/*       name			,tinyid                 ,flags,             ver */
	{   "cur", PROP_CUR, JSPROP_ENUMERATE,  317 },
	{   "bar", PROP_BAR, JSPROP_ENUMERATE,  317 },
	{   "left", PROP_LEFT, JSPROP_ENUMERATE,  31802 },
	{   "top", PROP_TOP, JSPROP_ENUMERATE,  31802 },
	{   "width", PROP_WIDTH, JSPROP_ENUMERATE,  31802 },
	{0}
};
static jsSyncPropertySpec js_uifc_showbuf_class_properties[] = {
/*       name           ,tinyid                 ,flags,             ver */
	{   "cur", PROP_CUR, JSPROP_ENUMERATE,  31802 },
	{   "bar", PROP_BAR, JSPROP_ENUMERATE,  31802 },
	{   "left", PROP_LEFT, JSPROP_ENUMERATE,  31802 },
	{   "top", PROP_TOP, JSPROP_ENUMERATE,  31802 },
	{   "width", PROP_WIDTH, JSPROP_ENUMERATE,  31802 },
	{   "height", PROP_HEIGHT, JSPROP_ENUMERATE,  31802 },
	{0}
};
static jsSyncPropertySpec js_uifc_getstrxy_class_properties[] = {
/*       name           ,tinyid                 ,flags,             ver */
	{   "lastkey", PROP_LASTKEY, JSPROP_ENUMERATE,  31802 },
	{0}
};
/* Constructor */
static JSBool js_list_ctx_constructor(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *                  argv = JS_ARGV(cx, arglist);
	JSObject *               obj = JS_THIS_OBJECT(cx, arglist);
	struct list_ctx_private* p;
	obj = JS_NewObject(cx, &js_uifc_list_ctx_class, NULL, NULL);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));
	if ((p = (struct list_ctx_private *)calloc(1, sizeof(struct list_ctx_private))) == NULL) {
		JS_ReportError(cx, "calloc failed");
		return JS_FALSE;
	}
	JS_SetPrivate(obj, p);
	p->bar = INT_MAX;
	if (argc > 0 && JSVAL_IS_NUMBER(argv[0]))
		p->cur = JSVAL_TO_INT(argv[0]);
	if (argc > 1 && JSVAL_IS_NUMBER(argv[1]))
		p->bar = JSVAL_TO_INT(argv[1]);
	if (argc > 2 && JSVAL_IS_NUMBER(argv[2]))
		p->left = JSVAL_TO_INT(argv[2]);
	if (argc > 3 && JSVAL_IS_NUMBER(argv[3]))
		p->top = JSVAL_TO_INT(argv[3]);
	if (argc > 4 && JSVAL_IS_NUMBER(argv[4]))
		p->width = JSVAL_TO_INT(argv[4]);
	js_SyncResolve(cx, obj, NULL, js_uifc_list_class_properties, NULL, NULL, 0);
	js_DefineSyncAccessors(cx, obj, js_uifc_list_class_properties, js_list_ctx_prop_getter, NULL, js_list_ctx_prop_setter);
#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx, obj, "Class used to retain UIFC list menu context", 317);
	js_DescribeSyncConstructor(cx, obj, "To create a new UIFCListContext object: <tt>var ctx = new UIFCListContext();</tt>");
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", uifc_list_ctx_prop_desc, JSPROP_READONLY);
#endif
	return JS_TRUE;
}
static JSBool js_showbuf_ctx_constructor(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *                     argv = JS_ARGV(cx, arglist);
	JSObject *                  obj = JS_THIS_OBJECT(cx, arglist);
	struct showbuf_ctx_private* p;
	obj = JS_NewObject(cx, &js_uifc_showbuf_ctx_class, NULL, NULL);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));
	if ((p = (struct showbuf_ctx_private *)calloc(1, sizeof(struct showbuf_ctx_private))) == NULL) {
		JS_ReportError(cx, "calloc failed");
		return JS_FALSE;
	}
	p->height = INT_MAX;
	p->width = INT_MAX;
	if (argc > 0 && JSVAL_IS_NUMBER(argv[0]))
		p->cur = JSVAL_TO_INT(argv[0]);
	if (argc > 1 && JSVAL_IS_NUMBER(argv[1]))
		p->bar = JSVAL_TO_INT(argv[1]);
	if (argc > 2 && JSVAL_IS_NUMBER(argv[2]))
		p->left = JSVAL_TO_INT(argv[2]);
	if (argc > 3 && JSVAL_IS_NUMBER(argv[3]))
		p->top = JSVAL_TO_INT(argv[3]);
	if (argc > 4 && JSVAL_IS_NUMBER(argv[4]))
		p->width = JSVAL_TO_INT(argv[4]);
	if (argc > 5 && JSVAL_IS_NUMBER(argv[5]))
		p->height = JSVAL_TO_INT(argv[5]);
	JS_SetPrivate(obj, p);
	js_SyncResolve(cx, obj, NULL, js_uifc_showbuf_class_properties, NULL, NULL, 0);
	js_DefineSyncAccessors(cx, obj, js_uifc_showbuf_class_properties, js_showbuf_ctx_prop_getter, NULL, js_showbuf_ctx_prop_setter);
#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx, obj, "Class used to retain UIFC showbuf context", 317);
	js_DescribeSyncConstructor(cx, obj, "To create a new UIFCShowbufContext object: <tt>var ctx = new UIFCShowbufContext();</tt>");
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", uifc_showbuf_ctx_prop_desc, JSPROP_READONLY);
#endif
	return JS_TRUE;
}
static JSBool js_getstrxy_ctx_constructor(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *                   obj = JS_THIS_OBJECT(cx, arglist);
	struct getstrxy_ctx_private* p;
	obj = JS_NewObject(cx, &js_uifc_getstrxy_ctx_class, NULL, NULL);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));
	if ((p = (struct getstrxy_ctx_private *)calloc(1, sizeof(struct getstrxy_ctx_private))) == NULL) {
		JS_ReportError(cx, "calloc failed");
		return JS_FALSE;
	}
	JS_SetPrivate(obj, p);
	js_SyncResolve(cx, obj, NULL, js_uifc_getstrxy_class_properties, NULL, NULL, 0);
	js_DefineSyncAccessors(cx, obj, js_uifc_getstrxy_class_properties, js_getstrxy_ctx_prop_getter, NULL, js_getstrxy_ctx_prop_setter);
#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx, obj, "Class used to retain UIFC getstrxy context", 317);
	js_DescribeSyncConstructor(cx, obj, "To create a new UIFCGetStrXYContext object: <tt>var ctx = new UIFCGetStrXYContext();</tt>");
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", uifc_getstrxy_ctx_prop_desc, JSPROP_READONLY);
#endif
	return JS_TRUE;
}
/* Properties */
enum {
	PROP_INITIALIZED    /* read-only */
	, PROP_MODE
	, PROP_CHANGES
	, PROP_SAVNUM
	, PROP_SCRN_LEN
	, PROP_SCRN_WIDTH
	, PROP_ESC_DELAY
	, PROP_HELPBUF
	, PROP_HCOLOR
	, PROP_LCOLOR
	, PROP_BCOLOR
	, PROP_CCOLOR
	, PROP_LBCOLOR
	, PROP_LIST_HEIGHT
};
static JSBool js_uifc_get_value(JSContext *cx, uifcapi_t* uifc, jsint tiny, jsval *vp)
{
	JS::RootedString js_str(cx);

	switch (tiny) {
		case PROP_INITIALIZED:
			*vp = BOOLEAN_TO_JSVAL(uifc->initialized);
			break;
		case PROP_MODE:
			*vp = UINT_TO_JSVAL(uifc->mode);
			break;
		case PROP_CHANGES:
			*vp = BOOLEAN_TO_JSVAL(uifc->changes);
			break;
		case PROP_SAVNUM:
			*vp = INT_TO_JSVAL(uifc->savnum);
			break;
		case PROP_SCRN_LEN:
			*vp = INT_TO_JSVAL(uifc->scrn_len);
			break;
		case PROP_SCRN_WIDTH:
			*vp = INT_TO_JSVAL(uifc->scrn_width);
			break;
		case PROP_ESC_DELAY:
			*vp = INT_TO_JSVAL(uifc->esc_delay);
			break;
		case PROP_HELPBUF:
			if ((js_str = JS_NewStringCopyZ(cx, uifc->helpbuf)) == nullptr)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(js_str.get());
			break;
		case PROP_HCOLOR:
			*vp = INT_TO_JSVAL(uifc->hclr);
			break;
		case PROP_LCOLOR:
			*vp = INT_TO_JSVAL(uifc->lclr);
			break;
		case PROP_BCOLOR:
			*vp = INT_TO_JSVAL(uifc->bclr);
			break;
		case PROP_CCOLOR:
			*vp = INT_TO_JSVAL(uifc->cclr);
			break;
		case PROP_LBCOLOR:
			*vp = INT_TO_JSVAL(uifc->lbclr);
			break;
		case PROP_LIST_HEIGHT:
			*vp = INT_TO_JSVAL(uifc->list_height);
			break;
	}
	return JS_TRUE;
}
static JSBool js_uifc_set_value(JSContext *cx, uifcapi_t* uifc, jsint tiny, jsval *vp)
{
	int32      i = 0;

	if (tiny == PROP_CHANGES)
		return JS_ValueToBoolean(cx, *vp, &uifc->changes);
	else if (tiny == PROP_HELPBUF) {
		if (uifc->helpbuf)
			free(uifc->helpbuf);
		JSVALUE_TO_MSTRING(cx, *vp, uifc->helpbuf, NULL);
		HANDLE_PENDING(cx, NULL);
		return JS_TRUE;
	}
	if (!JS_ValueToInt32(cx, *vp, &i))
		return JS_FALSE;
	switch (tiny) {
		case PROP_MODE:
			uifc->mode = i;
			break;
		case PROP_SAVNUM:
			if (i == static_cast<int32>(uifc->savnum) - 1 && uifc->restore != NULL)
				uifc->restore();
			else
				uifc->savnum = i;
			break;
		case PROP_SCRN_LEN:
			uifc->scrn_len = i;
			break;
		case PROP_SCRN_WIDTH:
			uifc->scrn_width = i;
			break;
		case PROP_ESC_DELAY:
			uifc->esc_delay = i;
			break;
		case PROP_LIST_HEIGHT:
			uifc->list_height = i;
			break;
		case PROP_HCOLOR:
			uifc->hclr = (char)i;
			break;
		case PROP_LCOLOR:
			uifc->lclr = (char)i;
			break;
		case PROP_BCOLOR:
			uifc->bclr = (char)i;
			break;
		case PROP_CCOLOR:
			uifc->cclr = (char)i;
			break;
		case PROP_LBCOLOR:
			uifc->lbclr = (char)i;
			break;
	}
	return JS_TRUE;
}
static bool js_uifc_prop_getter(JSContext* cx, unsigned argc, JS::Value* vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject thisObj(cx);
	if (!args.computeThis(cx, &thisObj))
		return false;
	uifcapi_t* uifc = (uifcapi_t*)JS_GetPrivate(thisObj);
	if (uifc == nullptr || !uifc->initialized) { args.rval().setUndefined(); return true; }
	JSObject* callee = &args.callee();
	jsint tiny = js::GetFunctionNativeReserved(callee, 0).toInt32();
	jsval val = JSVAL_VOID;
	if (!js_uifc_get_value(cx, uifc, tiny, &val)) return false;
	args.rval().set(val);
	return true;
}
static bool js_uifc_prop_setter(JSContext* cx, unsigned argc, JS::Value* vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject thisObj(cx);
	if (!args.computeThis(cx, &thisObj))
		return false;
	uifcapi_t* uifc = (uifcapi_t*)JS_GetPrivate(thisObj);
	if (uifc == nullptr)
		return true;
	JSObject* callee = &args.callee();
	jsint tiny = js::GetFunctionNativeReserved(callee, 0).toInt32();
	jsval val = args.length() > 0 ? args[0] : JSVAL_VOID;
	if (!js_uifc_set_value(cx, uifc, tiny, &val))
		return false;
	args.rval().set(val);
	return true;
}
static jsSyncPropertySpec js_properties[] = {
/*		 name,				tinyid,						flags,		ver	*/
	{   "initialized",      PROP_INITIALIZED,   JSPROP_ENUMERATE | JSPROP_READONLY, 314 },
	{   "mode",             PROP_MODE,          JSPROP_ENUMERATE,   314 },
	{   "changes",          PROP_CHANGES,       JSPROP_ENUMERATE,   314 },
	{   "save_num",         PROP_SAVNUM,        JSPROP_ENUMERATE,   314 },
	{   "screen_length",    PROP_SCRN_LEN,      JSPROP_ENUMERATE,   314 },
	{   "screen_width",     PROP_SCRN_WIDTH,    JSPROP_ENUMERATE,   314 },
	{   "list_height",      PROP_LIST_HEIGHT,   JSPROP_ENUMERATE,   314 },
	{   "esc_delay",        PROP_ESC_DELAY,     JSPROP_ENUMERATE,   314 },
	{   "help_text",        PROP_HELPBUF,       JSPROP_ENUMERATE,   314 },
	{   "background_color", PROP_BCOLOR,        JSPROP_ENUMERATE,   314 },
	{   "frame_color",      PROP_HCOLOR,        JSPROP_ENUMERATE,   314 },
	{   "text_color",       PROP_LCOLOR,        JSPROP_ENUMERATE,   314 },
	{   "inverse_color",    PROP_CCOLOR,        JSPROP_ENUMERATE,   314 },
	{   "lightbar_color",   PROP_LBCOLOR,       JSPROP_ENUMERATE,   314 },
	{0}
};
#ifdef BUILD_JSDOCS
static const char*        uifc_prop_desc[] = {
	"UIFC library has been successfully initialized"
	, "Current mode flags (see <tt>uifcdefs.js</tt>)"
	, "A change has occurred in an input call.  You are expected to set this to false before calling the input if you care about it."
	, "Save buffer depth (advanced)"
	, "Current screen length"
	, "Current screen width"
	, "When <tt>WIN_FIXEDHEIGHT</tt> mode flag is set, specifies the height used by a list method"
	, "Delay before a single ESC char is parsed and assumed to not be an ANSI sequence (advanced)"
	, "Text that will be displayed when F1 or '?' keys are pressed"
	, "Background colour"
	, "Frame colour"
	, "Text colour"
	, "Inverse colour"
	, "Lightbar colour"
	, NULL
};
#endif
static JSBool js_uifc_fill_properties(JSContext* cx, JSObject* obj, const char* name)
{
	return js_DefineSyncAccessors(cx, obj, js_properties, js_uifc_prop_getter, name, js_uifc_prop_setter);
}
/* Convenience functions */
static uifcapi_t* get_uifc(JSContext *cx, JSObject *obj)
{
	uifcapi_t* uifc;
	if ((uifc = (uifcapi_t*)JS_GetPrivate(obj)) == NULL)
		return NULL;
	if (!uifc->initialized) {
		JS_ReportError(cx, "UIFC not initialized");
		return NULL;
	}
	return uifc;
}
/* Methods */
static JSBool
js_uifc_init(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *  obj = JS_THIS_OBJECT(cx, arglist);
	jsval *     argv = JS_ARGV(cx, arglist);
	int         ciolib_mode = CIOLIB_MODE_AUTO;
	const char* title_def = "Synchronet";
	char*       title = (char *)title_def;
	char*       mode;
	uifcapi_t*  uifc;
	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
	if ((uifc = (uifcapi_t*)JS_GetPrivate(obj)) == NULL)
		return JS_FALSE;
	if (argc) {
		JSVALUE_TO_MSTRING(cx, argv[0], title, NULL);
		HANDLE_PENDING(cx, title);
		if (title == NULL)
			return JS_TRUE;
	}
	if (argc > 1) {
		JSVALUE_TO_ASTRING(cx, argv[1], mode, 16, NULL);
		if (mode != NULL) {
			if (!stricmp(mode, "STDIO"))
				ciolib_mode = -1;
			else if (!stricmp(mode, "AUTO"))
				ciolib_mode = CIOLIB_MODE_AUTO;
			else if (!stricmp(mode, "X"))
				ciolib_mode = CIOLIB_MODE_X;
			else if (!stricmp(mode, "CURSES"))
				ciolib_mode = CIOLIB_MODE_CURSES;
			else if (!stricmp(mode, "CURSES_IBM"))
				ciolib_mode = CIOLIB_MODE_CURSES_IBM;
			else if (!stricmp(mode, "CURSES_ASCII"))
				ciolib_mode = CIOLIB_MODE_CURSES_ASCII;
			else if (!stricmp(mode, "ANSI"))
				ciolib_mode = CIOLIB_MODE_ANSI;
			else if (!stricmp(mode, "CONIO"))
				ciolib_mode = CIOLIB_MODE_CONIO;
			else if (!stricmp(mode, "SDL"))
				ciolib_mode = CIOLIB_MODE_SDL;
		}
	}
	if (ciolib_mode == -1) {
		if (uifcinix(uifc)) {
			if (title != title_def)
				free(title);
			return JS_TRUE;
		}
	} else {
		if (initciolib(ciolib_mode)) {
			if (title != title_def)
				free(title);
			return JS_TRUE;
		}
		if (uifcini32(uifc)) {
			if (title != title_def)
				free(title);
			return JS_TRUE;
		}
	}
	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	uifc->scrn(title);
	if (title != title_def)
		free(title);
	return JS_TRUE;
}
static JSBool
js_uifc_bail(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	uifcapi_t* uifc;
	JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	if ((uifc = get_uifc(cx, obj)) == NULL)
		return JS_FALSE;
	uifc->bail();
	return JS_TRUE;
}
static JSBool
js_uifc_showhelp(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	uifcapi_t* uifc;
	JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	if ((uifc = get_uifc(cx, obj)) == NULL)
		return JS_FALSE;
	uifc->showhelp();
	return JS_TRUE;
}
static JSBool
js_uifc_msg(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      str = NULL;
	uifcapi_t* uifc;
	JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	if ((uifc = get_uifc(cx, obj)) == NULL)
		return JS_FALSE;
	JSVALUE_TO_MSTRING(cx, argv[0], str, NULL);
	HANDLE_PENDING(cx, str);
	if (str == NULL)
		return JS_TRUE;
	uifc->msg(str);
	free(str);
	return JS_TRUE;
}
static JSBool
js_uifc_pop(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      str = NULL;
	uifcapi_t* uifc;
	JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	if ((uifc = get_uifc(cx, obj)) == NULL)
		return JS_FALSE;
	if (argc) {
		JSVALUE_TO_MSTRING(cx, argv[0], str, NULL);
		HANDLE_PENDING(cx, str);
	}
	uifc->pop(str);
	if (str)
		free(str);
	return JS_TRUE;
}
static JSBool
js_uifc_input(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      str;
	char*      org = NULL;
	char*      prompt = NULL;
	int32      maxlen = 0;
	int32      left = 0;
	int32      top = 0;
	int32      mode = 0;
	int32      kmode = 0;
	uifcapi_t* uifc;
	uintN      argn = 0;
	JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	if ((uifc = get_uifc(cx, obj)) == NULL)
		return JS_FALSE;
	if (argn < argc && JSVAL_IS_NUMBER(argv[argn])
	    && !JS_ValueToInt32(cx, argv[argn++], &mode))
		return JS_FALSE;
	if (argn < argc && JSVAL_IS_NUMBER(argv[argn])
	    && !JS_ValueToInt32(cx, argv[argn++], &left))
		return JS_FALSE;
	if (argn < argc && JSVAL_IS_NUMBER(argv[argn])
	    && !JS_ValueToInt32(cx, argv[argn++], &top))
		return JS_FALSE;
	if (argn < argc && JSVAL_IS_STRING(argv[argn])) {
		JSVALUE_TO_MSTRING(cx, argv[argn], prompt, NULL);
		argn++;
		HANDLE_PENDING(cx, prompt);
		if (prompt == NULL)
			return JS_TRUE;
	}
	if (argn < argc && JSVAL_IS_STRING(argv[argn])) {
		JSVALUE_TO_MSTRING(cx, argv[argn], org, NULL);
		argn++;
		if (JS_IsExceptionPending(cx)) {
			if (prompt)
				free(prompt);
			return JS_FALSE;
		}
		if (org == NULL) {
			if (prompt)
				free(prompt);
			return JS_TRUE;
		}
	}
	if (argn < argc && JSVAL_IS_NUMBER(argv[argn])
	    && !JS_ValueToInt32(cx, argv[argn++], &maxlen)) {
		if (prompt)
			free(prompt);
		if (org)
			free(org);
		return JS_FALSE;
	}
	if (argn < argc && JSVAL_IS_NUMBER(argv[argn])
	    && !JS_ValueToInt32(cx, argv[argn++], &kmode)) {
		if (prompt)
			free(prompt);
		if (org)
			free(org);
		return JS_FALSE;
	}
	if (!maxlen)
		maxlen = 40;
	if ((str = (char*)malloc(maxlen + 1)) == NULL) {
		if (prompt)
			free(prompt);
		if (org)
			free(org);
		return JS_FALSE;
	}
	memset(str, 0, maxlen + 1);
	if (org) {
		strncpy(str, org, maxlen);
		free(org);
	}
	if (uifc->input(mode, left, top, prompt, str, maxlen, kmode) < 0) {
		if (prompt)
			free(prompt);
		if (str)
			free(str);
		return JS_TRUE;
	}
	if (prompt)
		free(prompt);
	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, str)));
	if (str)
		free(str);
	return JS_TRUE;
}
static JSBool
js_uifc_list(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *               obj = JS_THIS_OBJECT(cx, arglist);
	jsval *                  argv = JS_ARGV(cx, arglist);
	char*                    title = NULL;
	int32                    left = 0;
	int32                    top = 0;
	int32                    width = 0;
	int32                    dflt = 0;
	int32 *                  dptr = &dflt;
	int32                    bar = 0;
	int32 *                  bptr = &bar;
	int32                    mode = 0;
	JSObject*                objarg;
	uifcapi_t*               uifc;
	uintN                    argn = 0;
	jsval                    val;
	jsuint                   i;
	jsuint                   numopts;
	str_list_t               opts = NULL;
	char *                   opt = NULL;
	size_t                   opt_sz = 0;
	struct list_ctx_private *p;
	JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	if ((uifc = get_uifc(cx, obj)) == NULL)
		return JS_FALSE;
	if (argn < argc && JSVAL_IS_NUMBER(argv[argn])
	    && !JS_ValueToInt32(cx, argv[argn++], &mode))
		return JS_FALSE;
	for (; argn < argc; argn++) {
		if (JSVAL_IS_STRING(argv[argn])) {
			free(title);
			JSVALUE_TO_MSTRING(cx, argv[argn], title, NULL);
			HANDLE_PENDING(cx, title);
			continue;
		}
		if (!JSVAL_IS_OBJECT(argv[argn]))
			continue;
		if ((objarg = JSVAL_TO_OBJECT(argv[argn])) == NULL) {
			free(title);
			return JS_FALSE;
		}
		if (JS_IsArrayObject(cx, objarg)) {
			if (!JS_GetArrayLength(cx, objarg, &numopts)) {
				free(title);
				return JS_TRUE;
			}
			if (opts == NULL)
				opts = strListInit();
			for (i = 0; i < numopts; i++) {
				if (!JS_GetElement(cx, objarg, i, &val))
					break;
				JSVALUE_TO_RASTRING(cx, val, opt, &opt_sz, NULL);
				if (JS_IsExceptionPending(cx)) {
					if (title)
						free(title);
				}
				strListPush(&opts, opt);
			}
			FREE_AND_NULL(opt);
		}
		else if (JS_GetClass(cx, objarg) == &js_uifc_list_ctx_class) {
			p = static_cast<list_ctx_private *>(JS_GetPrivate(objarg));
			if (p != NULL) {
				dptr = &(p->cur);
				bptr = &(p->bar);
				left = p->left;
				top = p->top;
				width = p->width;
			}
		}
	}
	if (title == NULL || opts == NULL) {
		JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
	} else {
		JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(uifc->list(mode | WIN_BLANKOPTS, left, top, width, (int*)dptr, (int*)bptr, title, opts)));
	}
	strListFree(&opts);
	if (title != NULL)
		free(title);
	return JS_TRUE;
}
static JSBool
js_uifc_scrn(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      str = NULL;
	uifcapi_t* uifc;
	JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	if ((uifc = get_uifc(cx, obj)) == NULL)
		return JS_FALSE;
	JSVALUE_TO_MSTRING(cx, argv[0], str, NULL);
	HANDLE_PENDING(cx, str);
	if (str == NULL)
		return JS_TRUE;
	uifc->scrn(str);
	free(str);
	return JS_TRUE;
}
static JSBool
js_uifc_timedisplay(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	JSBool     force = JS_FALSE;
	uifcapi_t* uifc;
	JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	if ((uifc = get_uifc(cx, obj)) == NULL)
		return JS_FALSE;
	if (argc > 0)
		force = JSVAL_TO_BOOLEAN(argv[0]);
	uifc->timedisplay(force);
	return JS_TRUE;
}
static JSBool
js_uifc_bottomline(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject * obj = JS_THIS_OBJECT(cx, arglist);
	jsval *    argv = JS_ARGV(cx, arglist);
	int        mode;
	uifcapi_t* uifc;
	JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	if ((uifc = get_uifc(cx, obj)) == NULL)
		return JS_FALSE;
	if (argc == 0) {
		JS_ReportError(cx, "No mode specified");
		return JS_FALSE;
	}
	mode = JSVAL_TO_INT(argv[0]);
	uifc->bottomline(mode);
	return JS_TRUE;
}
static JSBool
js_uifc_getstrxy(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *                   obj = JS_THIS_OBJECT(cx, arglist);
	jsval *                      argv = JS_ARGV(cx, arglist);
	char*                        str;
	char*                        org = NULL;
	int32                        left = 0;
	int32                        top = 0;
	int32                        width = 0;
	int32                        maxlen = 0;
	int32                        mode = 0;
	uifcapi_t*                   uifc;
	uintN                        argn = 0;
	JSObject*                    objarg;
	int *                        lastkey = NULL;
	struct getstrxy_ctx_private *p;
	JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	if ((uifc = get_uifc(cx, obj)) == NULL)
		return JS_FALSE;
	if (argc < 5) {
		JS_ReportError(cx, "getstrxy requires at least five arguments");
		return JS_FALSE;
	}
	if (!JS_ValueToInt32(cx, argv[argn++], &left))
		return JS_FALSE;
	if (!JS_ValueToInt32(cx, argv[argn++], &top))
		return JS_FALSE;
	if (!JS_ValueToInt32(cx, argv[argn++], &width))
		return JS_FALSE;
	if (!JS_ValueToInt32(cx, argv[argn++], &maxlen))
		return JS_FALSE;
	if (!JS_ValueToInt32(cx, argv[argn++], &mode))
		return JS_FALSE;
	if (argn < argc && JSVAL_IS_STRING(argv[argn])) {
		JSVALUE_TO_MSTRING(cx, argv[argn], org, NULL);
		argn++;
		if (JS_IsExceptionPending(cx)) {
			free(org);
			return JS_FALSE;
		}
		if (org == NULL)
			return JS_TRUE;
	}
	if (argn < argc && JSVAL_IS_OBJECT(argv[argn])) {
		if ((objarg = JSVAL_TO_OBJECT(argv[argn])) == NULL) {
			free(org);
			return JS_FALSE;
		}
		if (JS_GetClass(cx, objarg) == &js_uifc_getstrxy_ctx_class) {
			p = static_cast<getstrxy_ctx_private *>(JS_GetPrivate(objarg));
			if (p != NULL) {
				lastkey = &(p->lastkey);
			}
		}
	}
	if (maxlen < 1) {
		JS_ReportError(cx, "max length less than one");
		free(org);
		return JS_FALSE;
	}
	if ((str = (char*)malloc(maxlen + 1)) == NULL) {
		free(org);
		return JS_FALSE;
	}
	memset(str, 0, maxlen + 1);
	if (org) {
		strncpy(str, org, maxlen);
		free(org);
	}
	if (uifc->getstrxy(left, top, width, str, maxlen, mode, lastkey) < 0) {
		free(str);
		JS_SET_RVAL(cx, arglist, JSVAL_NULL);
		return JS_TRUE;
	}
	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, str)));
	if (str)
		free(str);
	return JS_TRUE;
}
static JSBool
js_uifc_showbuf(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *                  obj = JS_THIS_OBJECT(cx, arglist);
	jsval *                     argv = JS_ARGV(cx, arglist);
	char*                       str;
	char*                       title = NULL;
	int32                       left = 0;
	int32                       top = 0;
	int32                       width = INT_MAX;
	int32                       height = INT_MAX;
	int32                       mode = 0;
	int *                       cur = NULL;
	int *                       bar = NULL;
	uifcapi_t*                  uifc;
	uintN                       argn = 0;
	JSObject*                   objarg;
	struct showbuf_ctx_private *p;
	JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	if ((uifc = get_uifc(cx, obj)) == NULL)
		return JS_FALSE;
	if (argc < 3) {
		JS_ReportError(cx, "showbuf requires at least three arguments");
		return JS_FALSE;
	}
	if (!JS_ValueToInt32(cx, argv[argn++], &mode))
		return JS_FALSE;
	JSVALUE_TO_MSTRING(cx, argv[argn++], title, NULL);
	if (JS_IsExceptionPending(cx)) {
		free(title);
		return JS_FALSE;
	}
	if (title == NULL)
		return JS_TRUE;
	JSVALUE_TO_MSTRING(cx, argv[argn++], str, NULL);
	if (JS_IsExceptionPending(cx)) {
		free(title);
		return JS_FALSE;
	}
	if (str == NULL) {
		free(title);
		return JS_TRUE;
	}
	if (argn < argc && JSVAL_IS_OBJECT(argv[argn])) {
		if ((objarg = JSVAL_TO_OBJECT(argv[argn])) == NULL) {
			free(title);
			free(str);
			return JS_FALSE;
		}
		if (JS_GetClass(cx, objarg) == &js_uifc_showbuf_ctx_class) {
			p = static_cast<showbuf_ctx_private *>(JS_GetPrivate(objarg));
			if (p != NULL) {
				cur = &(p->cur);
				bar = &(p->bar);
				left = p->left;
				top = p->top;
				width = p->width;
				height = p->height;
			}
		}
	}
	uifc->showbuf(mode, left, top, width, height, title, str, cur, bar);
	free(title);
	free(str);
	return JS_TRUE;
}
/* Destructor */
static void
js_finalize(JS::GCContext *cx, JSObject *obj)
{
	uifcapi_t* p;
	if ((p = (uifcapi_t*)JS_GetPrivate(obj)) == NULL)
		return;
	free(p);
	JS_SetPrivate(obj, NULL);
}
static jsSyncMethodSpec js_functions[] = {
	{"init",            js_uifc_init,       1,  JSTYPE_BOOLEAN, JSDOCSTR("<i>string</i> title [,<i>string</i> interface_mode]")
	 , JSDOCSTR("Initialize the UIFC library with the specified application/script title (e.g. name and maybe version).<br>"
		        "<tt>interface_mode</tt> is a string representing the desired console mode, one of 'STDIO', 'AUTO', "
		        "'X', 'CURSES', 'ANSI', 'CONIO', or 'SDL' (see <tt>conio.init()</tt> for details).<br>"
		        "Return <tt>true</tt> upon successful UIFC library initialization, <tt>false</tt> upon error."
		        )
	 , 314},
	{"bail",            js_uifc_bail,       0,  JSTYPE_VOID,    JSDOCSTR("")
	 , JSDOCSTR("Uninitialize the UIFC library")
	 , 314},
	{"msg",             js_uifc_msg,        1,  JSTYPE_VOID,    JSDOCSTR("<i>string</i> text")
	 , JSDOCSTR("Print a short text message and wait for user acknowledgment")
	 , 314},
	{"pop",             js_uifc_pop,        1,  JSTYPE_VOID,    JSDOCSTR("[<i>string</i> text]")
	 , JSDOCSTR("Pop-up (or down) a short text message. Pop-down by passing no <i>text</i> argument.")
	 , 314},
	{"input",           js_uifc_input,      0,  JSTYPE_STRING,  JSDOCSTR("[<i>number</i> mode] [,<i>number</i> left] [,<i>number</i> top] [,<i>string</i> prompt] [,<i>string</i> default] [,<i>number</i> maxlen [,<i>number</i> k_mode]]")
	 , JSDOCSTR("Prompt for a string input.<br>"
		        "<tt>mode</tt> is an optional combination of <tt>WIN_</tt> mode flags from <tt>uifcdefs.js</tt>.<br>"
		        "<tt>left</tt> and <tt>top</tt> are optional window offsets to display the input dialog box.<br>"
		        "<tt>prompt</tt> is an optional text string to display as part of the string input dialog box.<br>"
		        "<tt>default</tt> is an optional original text string that the user can edit (requires the <tt>K_EDIT k_mode</tt> flag).<br>"
		        "<tt>maxlen</tt> is an optional maximum input string length (default is 40 characters).<br>"
		        "<tt>k_mode</tt> is an optional combination of <tt>K_</tt> mode flags from either <tt>sbbsdefs.js</tt> or <tt>uifcdefs.js</tt>."
		        "<p>"
		        "Return the new/edited string or <tt>undefined</tt> if editing was aborted (e.g. via ESC key press)."
		        )
	 , 314},
	{"list",            js_uifc_list,       0,  JSTYPE_NUMBER,  JSDOCSTR("[<i>number</i> mode,] <i>string</i> title, <i>array</i> options [,<i>uifc.list.CTX</i> ctx]")
	 , JSDOCSTR("Select from a list of displayed options.<br>"
		        "<tt>title</tt> is the brief description of the list (menu) to be displayed in the list heading.<br>"
		        "<tt>options</tt> is an array of items (typically strings) that comprise the displayed list.<br>"
		        "The <tt>CTX</tt> (context) object can be created using <tt>new uifc.list.CTX</tt> and if the same object is passed in successive calls, allows <tt>WIN_SAV</tt> to work correctly.<br>"
		        "The context object has the following properties (<i>numbers</i>):<br><tt>cur, bar, left, top, width</tt>"
		        "<p>"
		        "Return <tt>-1</tt> if list is aborted (e.g. via ESC key press), <tt>false</tt> upon error (e.g. no option array provided), "
		        "or the 0-based numeric index of the option selected by the user.<br>"
		        "Other negative values may be returned in advanced modes/use-cases (e.g. copy/paste), see <tt>MSK_</tt> and related <tt>WIN_</tt> constants/comments in <tt>uifcdefs.js</tt> for details."
		        )
	 , 314},
	{"showhelp",            js_uifc_showhelp,   0,  JSTYPE_VOID,    JSDOCSTR("")
	 , JSDOCSTR("Show the current help text")
	 , 317},
	{"scrn",            js_uifc_scrn,       1,  JSTYPE_BOOLEAN, JSDOCSTR("<i>string</i> text")
	 , JSDOCSTR("Fill the screen with the appropriate background attribute.  string is the title for the application banner.")
	 , 31802},
	{"showbuf",         js_uifc_showbuf,    7,  JSTYPE_VOID,    JSDOCSTR("<i>number</i> mode, <i>string</i> title, <i>string</i> helpbuf [,<i>uifc.showbuf.CTX</i> ctx]")
	 , JSDOCSTR("Show a scrollable text buffer - optionally parsing \"help markup codes\"<br>"
		        "The context object can be created using <tt>new uifc.showbuf.CTX</tt> and if the same object is passed, allows <tt>WIN_SAV</tt> to work correctly.<br>"
		        "The context object has the following properties (<i>numbers</i>):<br><tt>cur, bar, left, top, width, height</tt>")
	 , 31802},
	{"timedisplay",         js_uifc_timedisplay,    0,  JSTYPE_VOID,    JSDOCSTR("[<i>bool<i/> force = false]")
	 , JSDOCSTR("Update time in upper right corner of screen with current time in ASCII/Unix format")
	 , 31802},
	{"bottomline",          js_uifc_bottomline, 1,  JSTYPE_VOID,    JSDOCSTR("<i>number</i> mode")
	 , JSDOCSTR("Display the bottom line using the <tt>WIN_*</tt> mode flags")
	 , 31802},
	{"getstrxy",            js_uifc_getstrxy,   7,  JSTYPE_STRING,  JSDOCSTR("<i>number</i> left, <i>number</i> top, <i>number</i> width, <i>number</i> max, <i>number</i> mode [,<i>string</i> original][, <i>uifc.getstrxy.CTX</i> ctx]")
	 , JSDOCSTR("String input/exit box at a specified position"
		        "The context object can be created using <tt>new uifc.showbuf.CTX</tt> and if the same object is passed, allows <tt>WIN_SAV</tt> to work correctly.<br>"
		        "The context object has the following properties: <i>number</i> <tt>lastkey</tt>")
	 , 31802},
	{0}
};
static bool js_uifc_resolve_impl(JSContext *cx, JSObject *obj, char* name);

bool js_uifc_resolve(JSContext *cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id, bool* resolvedp)
{
	char*     name = NULL;

	if (id.get().isString()) {
		JSString* jstr = id.get().toString();
		JSSTRING_TO_MSTRING(cx, jstr, name, NULL);
		HANDLE_PENDING(cx, name);
		if (name == NULL) return false;
	}

	bool ret = js_uifc_resolve_impl(cx, obj, name);
	if (name)
		free(name);
	if (resolvedp) *resolvedp = ret;
	if (JS_IsExceptionPending(cx))
		JS_ClearPendingException(cx);
	return true;
}

static bool js_uifc_resolve_impl(JSContext *cx, JSObject *obj, char* name)
{
	bool      ret;
	jsval     objval;
	JSObject* tobj;

	ret = js_SyncResolve(cx, obj, name, js_properties, js_functions, NULL, 0);
	if (ret)
		js_uifc_fill_properties(cx, obj, name);
	if (name == NULL || strcmp(name, "list") == 0) {
		if (JS_GetProperty(cx, obj, "list", &objval)) {
			tobj = JSVAL_TO_OBJECT(objval);
			if (tobj)
				JS_InitClass(cx, tobj, NULL, &js_uifc_list_ctx_class, js_list_ctx_constructor, 0, NULL, NULL, NULL, NULL);
		}
	}
	if (name == NULL || strcmp(name, "showbuf") == 0) {
		if (JS_GetProperty(cx, obj, "showbuf", &objval)) {
			tobj = JSVAL_TO_OBJECT(objval);
			if (tobj)
				JS_InitClass(cx, tobj, NULL, &js_uifc_showbuf_ctx_class, js_showbuf_ctx_constructor, 0, NULL, NULL, NULL, NULL);
		}
	}
	if (name == NULL || strcmp(name, "getstrxy") == 0) {
		if (JS_GetProperty(cx, obj, "getstrxy", &objval)) {
			tobj = JSVAL_TO_OBJECT(objval);
			if (tobj)
				JS_InitClass(cx, tobj, NULL, &js_uifc_getstrxy_ctx_class, js_getstrxy_ctx_constructor, 0, NULL, NULL, NULL, NULL);
		}
	}
	return ret;
}
static bool js_uifc_enumerate(JSContext *cx, JS::Handle<JSObject*> obj)
{
	return js_uifc_resolve_impl(cx, obj, NULL);
}
static const JSClassOps js_uifc_classops = {
	nullptr,                /* addProperty  */
	nullptr,                /* delProperty  */
	js_uifc_enumerate,      /* enumerate    */
	nullptr,                /* newEnumerate */
	js_uifc_resolve,        /* resolve      */
	nullptr,                /* mayResolve   */
	js_finalize,            /* finalize     */
	nullptr, nullptr, nullptr /* call, construct, trace */
};
static JSClass js_uifc_class = {
	"UIFC"
	, JSCLASS_HAS_PRIVATE | JSCLASS_FOREGROUND_FINALIZE
	, &js_uifc_classops
};
JSObject* js_CreateUifcObject(JSContext* cx, JSObject* parent)
{
	JSObject*  obj;
	uifcapi_t* api;
	JS::RootedObject rparent(cx, parent);

	if ((obj = JS_DefineObject(cx, rparent, "uifc", &js_uifc_class
	                           , JSPROP_ENUMERATE | JSPROP_READONLY)) == NULL)
		return NULL;
	if ((api = (uifcapi_t*)malloc(sizeof(uifcapi_t))) == NULL)
		return NULL;
	memset(api, 0, sizeof(uifcapi_t));
	api->size = sizeof(uifcapi_t);
	api->esc_delay = 25;
	JS_SetPrivate(obj, api);    /* Store a pointer to uifcapi_t */
#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx, obj, "User InterFaCe object - Text User Interface (TUI) menu system for JSexec", 314);
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", uifc_prop_desc, JSPROP_READONLY);
#endif
	return obj;
}
