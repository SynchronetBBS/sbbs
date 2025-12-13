/* Synchronet terminal server JavaScript related functions */

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
#include "ident.h"
#include "telnet.h"
#include "netwrap.h"
#include "petdefs.h"
#include "filedat.h"
#include "ssl.h"
#include "ver.h"
#include "eventwrap.h"
#include <multisock.h>
#include <limits.h>     // HOST_NAME_MAX

void* js_GetClassPrivate(JSContext *cx, JSObject *obj, JSClass* cls)
{
	void *ret = JS_GetInstancePrivate(cx, obj, cls, NULL);

	/*
	 * NOTE: Any changes here should also be added to the same function in jsdoor.c
	 *       (ie: anything not Synchronet specific).
	 */
	if (ret == NULL)
		JS_ReportError(cx, "'%s' instance: No Private Data or Class Mismatch"
		               , cls == NULL ? "???" : cls->name);
	return ret;
}

/* Convert from Synchronet-specific jsSyncMethodSpec to JSAPI's JSFunctionSpec */

JSBool
js_DescribeSyncObject(JSContext* cx, JSObject* obj, const char* str, int ver)
{
	JSString* js_str = JS_NewStringCopyZ(cx, str);

	if (js_str == NULL)
		return JS_FALSE;

	if (ver < 10000)     /* auto convert 313 to 31300 */
		ver *= 100;

	return JS_DefineProperty(cx, obj, "_description"
	                         , STRING_TO_JSVAL(js_str), NULL, NULL, JSPROP_READONLY) != JS_FALSE
	       && JS_DefineProperty(cx, obj, "_ver"
	                            , INT_TO_JSVAL(ver), NULL, NULL, JSPROP_READONLY) != JS_FALSE;
}

JSBool
js_DescribeSyncConstructor(JSContext* cx, JSObject* obj, const char* str)
{
	JSString* js_str = JS_NewStringCopyZ(cx, str);

	if (js_str == NULL)
		return JS_FALSE;

	return JS_DefineProperty(cx, obj, "_constructor"
	                         , STRING_TO_JSVAL(js_str), NULL, NULL, JSPROP_READONLY);
}

#ifdef BUILD_JSDOCS

static const char* method_array_name = "_method_list";
static const char* property_array_name = "_property_list";

/*
 * from jsatom.c:
 * Keep this in sync with jspubtd.h -- an assertion below will insist that
 * its length match the JSType enum's JSTYPE_LIMIT limit value.
 */
static const char *js_type_str[] = {
	"void",         // changed from "undefined"
	"object",
	"function",
	"string",
	"number",
	"boolean",
	"null",
	"xml",
	"array",
	"alias",
	"undefined"
};

JSBool
js_DefineSyncProperties(JSContext *cx, JSObject *obj, jsSyncPropertySpec* props)
{
	uint      i;
	int       ver;
	jsval     val;
	JSString* js_str;
	JSObject* array;
	JSObject* prop;

	if ((array = JS_NewObject(cx, NULL, NULL, obj)) == NULL)
		return JS_FALSE;

	for (i = 0; props[i].name != NULL; ++i) {
		if (props[i].tinyid < 256 && props[i].tinyid > -129) {
			if (!JS_DefinePropertyWithTinyId(cx, obj, /* Never reserve any "slots" for properties */
			                                 props[i].name, props[i].tinyid, JSVAL_VOID, NULL, NULL, props[i].flags | JSPROP_SHARED))
				return JS_FALSE;
		}
		else {
			if (!JS_DefineProperty(cx, obj, props[i].name, JSVAL_VOID, NULL, NULL, props[i].flags | JSPROP_SHARED))
				return JS_FALSE;
		}
		if (!(props[i].flags & JSPROP_ENUMERATE))   /* No need to document invisible props */
			continue;

		prop = JS_NewObject(cx, NULL, NULL, array);
		if (prop == NULL)
			return JS_FALSE;

		if ((ver = props[i].ver) < 10000)      /* auto convert 313 to 31300 */
			ver *= 100;
		val = INT_TO_JSVAL(ver);
		JS_SetProperty(cx, prop, "ver", &val);

		if (props[i].desc != NULL) {
			if ((js_str = JS_NewStringCopyZ(cx, props[i].desc)) == NULL)
				return JS_FALSE;
			val = STRING_TO_JSVAL(js_str);
			JS_SetProperty(cx, prop, "desc", &val);
		}
		if (!JS_DefineProperty(cx, array, props[i].name, OBJECT_TO_JSVAL(prop), NULL, NULL, JSPROP_READONLY | JSPROP_ENUMERATE))
			return JS_FALSE;
	}

	return JS_DefineProperty(cx, obj, property_array_name, OBJECT_TO_JSVAL(array), NULL, NULL, JSPROP_READONLY);
}

JSBool
js_DefineSyncMethods(JSContext* cx, JSObject* obj, jsSyncMethodSpec *funcs)
{
	int       i;
	jsuint    len = 0;
	int       ver;
	jsval     val;
	JSObject* method;
	JSObject* method_array;
	JSString* js_str;
	size_t    str_len = 0;
	char *    str = NULL;

	/* Return existing method_list array if it's already been created */
	if (JS_GetProperty(cx, obj, method_array_name, &val) && val != JSVAL_VOID) {
		method_array = JSVAL_TO_OBJECT(val);
		// If the first item is already in the list, don't do anything.
		if (method_array == NULL || !JS_GetArrayLength(cx, method_array, &len))
			return JS_FALSE;
		for (i = 0; i < (int)len; i++) {
			if (JS_GetElement(cx, method_array, i, &val) != JS_TRUE || val == JSVAL_VOID)
				continue;
			JS_GetProperty(cx, JSVAL_TO_OBJECT(val), "name", &val);
			JSVALUE_TO_RASTRING(cx, val, str, &str_len, NULL);
			if (str == NULL)
				continue;
			if (strcmp(str, funcs[0].name) == 0)
				return JS_TRUE;
		}
	}
	else {
		if ((method_array = JS_NewArrayObject(cx, 0, NULL)) == NULL)
			return JS_FALSE;
		if (!JS_DefineProperty(cx, obj, method_array_name, OBJECT_TO_JSVAL(method_array)
		                       , NULL, NULL, 0))
			return JS_FALSE;
	}

	for (i = 0; funcs[i].name != NULL; ++i) {

		if (!JS_DefineFunction(cx, obj, funcs[i].name, funcs[i].call, funcs[i].nargs, 0))
			return JS_FALSE;

		if (funcs[i].type == JSTYPE_ALIAS)
			continue;

		method = JS_NewObject(cx, NULL, NULL, method_array);
		if (method == NULL)
			return JS_FALSE;

		if (funcs[i].name != NULL) {
			if ((js_str = JS_NewStringCopyZ(cx, funcs[i].name)) == NULL)
				return JS_FALSE;
			val = STRING_TO_JSVAL(js_str);
			JS_SetProperty(cx, method, "name", &val);
		}

		val = INT_TO_JSVAL(funcs[i].nargs);
		if (!JS_SetProperty(cx, method, "nargs", &val))
			return JS_FALSE;

		if ((js_str = JS_NewStringCopyZ(cx, js_type_str[funcs[i].type])) == NULL)
			return JS_FALSE;
		val = STRING_TO_JSVAL(js_str);
		JS_SetProperty(cx, method, "type", &val);

		if (funcs[i].args != NULL) {
			if ((js_str = JS_NewStringCopyZ(cx, funcs[i].args)) == NULL)
				return JS_FALSE;
			val = STRING_TO_JSVAL(js_str);
			JS_SetProperty(cx, method, "args", &val);
		}

		if (funcs[i].desc != NULL) {
			if ((js_str = JS_NewStringCopyZ(cx, funcs[i].desc)) == NULL)
				return JS_FALSE;
			val = STRING_TO_JSVAL(js_str);
			JS_SetProperty(cx, method, "desc", &val);
		}

		if (funcs[i].ver) {
			if ((ver = funcs[i].ver) < 10000)      /* auto convert 313 to 31300 */
				ver *= 100;
			val = INT_TO_JSVAL(ver);
			JS_SetProperty(cx, method, "ver", &val);
		}

		val = OBJECT_TO_JSVAL(method);
		if (!JS_SetElement(cx, method_array, len + i, &val))
			return JS_FALSE;
	}

	return JS_TRUE;
}

/*
 * Always resolve all here since
 * 1) We'll always be enumerating anyways
 * 2) The speed penalty won't be seen in production code anyways
 */
JSBool
js_SyncResolve(JSContext* cx, JSObject* obj, char *name, jsSyncPropertySpec* props, jsSyncMethodSpec* funcs, jsConstIntSpec* consts, int flags)
{
	JSBool ret = JS_TRUE;

	if (props) {
		if (!js_DefineSyncProperties(cx, obj, props))
			ret = JS_FALSE;
	}

	if (funcs) {
		if (!js_DefineSyncMethods(cx, obj, funcs))
			ret = JS_FALSE;
	}

	if (consts) {
		if (!js_DefineConstIntegers(cx, obj, consts, flags))
			ret = JS_FALSE;
	}

	return ret;
}

#else // NON-JSDOCS

JSBool
js_DefineSyncProperties(JSContext *cx, JSObject *obj, jsSyncPropertySpec* props)
{
	uint i;

	/*
	 * NOTE: Any changes here should also be added to the same function in jsdoor.c
	 *       (ie: anything not Synchronet specific).
	 */
	for (i = 0; props[i].name; i++) {
		if (props[i].tinyid < 256 && props[i].tinyid > -129) {
			if (!JS_DefinePropertyWithTinyId(cx, obj,
			                                 props[i].name, props[i].tinyid, JSVAL_VOID, NULL, NULL, props[i].flags | JSPROP_SHARED))
				return JS_FALSE;
		}
		else {
			if (!JS_DefineProperty(cx, obj, props[i].name, JSVAL_VOID, NULL, NULL, props[i].flags | JSPROP_SHARED))
				return JS_FALSE;
		}
	}

	return JS_TRUE;
}


JSBool
js_DefineSyncMethods(JSContext* cx, JSObject* obj, jsSyncMethodSpec *funcs)
{
	uint i;

	/*
	 * NOTE: Any changes here should also be added to the same function in jsdoor.c
	 *       (ie: anything not Synchronet specific).
	 */
	for (i = 0; funcs[i].name; i++)
		if (!JS_DefineFunction(cx, obj, funcs[i].name, funcs[i].call, funcs[i].nargs, 0))
			return JS_FALSE;
	return JS_TRUE;
}

JSBool
js_SyncResolve(JSContext* cx, JSObject* obj, char *name, jsSyncPropertySpec* props, jsSyncMethodSpec* funcs, jsConstIntSpec* consts, int flags)
{
	uint  i;
	jsval val;

	/*
	 * NOTE: Any changes here should also be added to the same function in jsdoor.c
	 *       (ie: anything not Synchronet specific).
	 */
	if (props) {
		for (i = 0; props[i].name; i++) {
			if (name == NULL || strcmp(name, props[i].name) == 0) {
				if (props[i].tinyid < 256 && props[i].tinyid > -129) {
					if (!JS_DefinePropertyWithTinyId(cx, obj,
					                                 props[i].name, props[i].tinyid, JSVAL_VOID, NULL, NULL, props[i].flags | JSPROP_SHARED))
						return JS_FALSE;
				}
				else {
					if (!JS_DefineProperty(cx, obj, props[i].name, JSVAL_VOID, NULL, NULL, props[i].flags | JSPROP_SHARED))
						return JS_FALSE;
				}
				if (name)
					return JS_TRUE;
			}
		}
	}
	if (funcs) {
		for (i = 0; funcs[i].name; i++) {
			if (name == NULL || strcmp(name, funcs[i].name) == 0) {
				if (!JS_DefineFunction(cx, obj, funcs[i].name, funcs[i].call, funcs[i].nargs, 0))
					return JS_FALSE;
				if (name)
					return JS_TRUE;
			}
		}
	}
	if (consts) {
		for (i = 0; consts[i].name; i++) {
			if (name == NULL || strcmp(name, consts[i].name) == 0) {
				val = INT_TO_JSVAL(consts[i].val);

				if (!JS_DefineProperty(cx, obj, consts[i].name, val, NULL, NULL, flags))
					return JS_FALSE;

				if (name)
					return JS_TRUE;
			}
		}
	}

	return JS_TRUE;
}

#endif

/* This is a stream-lined version of JS_DefineConstDoubles */
JSBool
js_DefineConstIntegers(JSContext* cx, JSObject* obj, jsConstIntSpec* ints, int flags)
{
	uint  i;
	jsval val;

	for (i = 0; ints[i].name; i++) {
		val = INT_TO_JSVAL(ints[i].val);

		if (!JS_DefineProperty(cx, obj, ints[i].name, val, NULL, NULL, flags))
			return JS_FALSE;
	}

	return JS_TRUE;
}

static JSBool
js_log(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	uintN      i = 0;
	int32      level = LOG_INFO;
	JSString*  str = NULL;
	sbbs_t*    sbbs;
	jsrefcount rc;
	char *     line = NULL;
	size_t     line_sz = 0;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;

	if ((sbbs = (sbbs_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	if (argc > 1 && JSVAL_IS_NUMBER(argv[i])) {
		if (!JS_ValueToInt32(cx, argv[i++], &level))
			return JS_FALSE;
	}

	for (; i < argc; i++) {
		if ((str = JS_ValueToString(cx, argv[i])) == NULL) {
			FREE_AND_NULL(line);
			return JS_FALSE;
		}
		JSSTRING_TO_RASTRING(cx, str, line, &line_sz, NULL);
		if (line == NULL)
			return JS_FALSE;
		rc = JS_SUSPENDREQUEST(cx);
		sbbs->lputs(level, line);
		JS_RESUMEREQUEST(cx, rc);
	}
	if (line != NULL)
		free(line);

	if (str == NULL)
		JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	else
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(str));
	return JS_TRUE;
}

static JSBool
js_read(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	uchar*     buf;
	int32      len = 128;
	sbbs_t*    sbbs;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((sbbs = (sbbs_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	if (argc) {
		if (js_argvIsNullOrVoid(cx, argv, 0))
			return JS_FALSE;
		if (!JS_ValueToInt32(cx, argv[0], &len))
			return JS_FALSE;
	}

	if ((buf = (uchar*)malloc(len)) == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	len = RingBufRead(&sbbs->inbuf, buf, len);
	JS_RESUMEREQUEST(cx, rc);

	if (len > 0)
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(JS_NewStringCopyN(cx, (char*)buf, len)));

	free(buf);
	return JS_TRUE;
}

static JSBool
js_readln(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      buf;
	int32      len = 128;
	sbbs_t*    sbbs;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((sbbs = (sbbs_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	if (argc) {
		if (js_argvIsNullOrVoid(cx, argv, 0))
			return JS_FALSE;
		if (!JS_ValueToInt32(cx, argv[0], &len))
			return JS_FALSE;
	}

	if (len > 0) {
		if ((buf = (char*)malloc(len)) == NULL)
			return JS_TRUE;

		rc = JS_SUSPENDREQUEST(cx);
		len = sbbs->getstr(buf, len, K_NONE);
		JS_RESUMEREQUEST(cx, rc);

		if (len > 0)
			JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, buf)));

		free(buf);
	}
	return JS_TRUE;
}

static JSBool
js_write(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	uintN      i;
	JSString*  str = NULL;
	sbbs_t*    sbbs;
	jsrefcount rc;
	char *     cstr = NULL;
	size_t     cstr_sz = 0;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((sbbs = (sbbs_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	for (i = 0; i < argc; i++) {
		if ((str = JS_ValueToString(cx, argv[i])) == NULL) {
			FREE_AND_NULL(cstr);
			return JS_FALSE;
		}
		JSSTRING_TO_RASTRING(cx, str, cstr, &cstr_sz, NULL);
		if (cstr == NULL)
			return JS_FALSE;
		rc = JS_SUSPENDREQUEST(cx);
		if (sbbs->online != ON_REMOTE)
			sbbs->lputs(LOG_INFO, cstr);
		else
			sbbs->bputs(cstr);
		JS_RESUMEREQUEST(cx, rc);
	}
	FREE_AND_NULL(cstr);

	return JS_TRUE;
}

static JSBool
js_write_raw(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	uintN      i;
	char*      str = NULL;
	size_t     str_sz = 0;
	size_t     len;
	sbbs_t*    sbbs;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((sbbs = (sbbs_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	for (i = 0; i < argc; i++) {
		JSVALUE_TO_RASTRING(cx, argv[i], str, &str_sz, &len);
		if (str == NULL)
			return JS_FALSE;
		if (len < 1)
			continue;
		rc = JS_SUSPENDREQUEST(cx);
		sbbs->term_out(str, len);
		JS_RESUMEREQUEST(cx, rc);
	}
	if (str != NULL)
		free(str);

	return JS_TRUE;
}

static JSBool
js_writeln(JSContext *cx, uintN argc, jsval *arglist)
{
	sbbs_t*    sbbs;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((sbbs = (sbbs_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	js_write(cx, argc, arglist);
	rc = JS_SUSPENDREQUEST(cx);
	if (sbbs->online == ON_REMOTE)
		sbbs->bputs(crlf);
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_printf(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      p;
	sbbs_t*    sbbs;
	jsrefcount rc;

	if (argc < 1) {
		JS_SET_RVAL(cx, arglist, JS_GetEmptyStringValue(cx));
		return JS_TRUE;
	}

	if ((sbbs = (sbbs_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	if ((p = js_sprintf(cx, 0, argc, argv)) == NULL) {
		JS_ReportError(cx, "js_sprintf failed");
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	if (sbbs->online != ON_REMOTE)
		sbbs->lputs(LOG_INFO, p);
	else
		sbbs->bputs(p);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, p)));

	js_sprintf_free(p);

	return JS_TRUE;
}

static JSBool
js_alert(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	sbbs_t*    sbbs;
	jsrefcount rc;
	char *     cstr;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((sbbs = (sbbs_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], cstr, NULL);
	if (cstr == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	if (sbbs->online != ON_REMOTE)
		sbbs->lputs(LOG_WARNING, cstr);
	else {
		sbbs->attr(sbbs->cfg.color[clr_err]);
		sbbs->bputs(cstr);
		sbbs->attr(LIGHTGRAY);
		sbbs->bputs(crlf);
	}
	free(cstr);
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_confirm(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	sbbs_t*    sbbs;
	jsrefcount rc;
	char *     cstr;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	if ((sbbs = (sbbs_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], cstr, NULL);
	if (cstr == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->yesno(cstr)));
	free(cstr);
	JS_RESUMEREQUEST(cx, rc);
	return JS_TRUE;
}

static JSBool
js_deny(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	sbbs_t*    sbbs;
	jsrefcount rc;
	char *     cstr;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	if ((sbbs = (sbbs_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], cstr, NULL);
	if (cstr == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(sbbs->noyes(cstr)));
	free(cstr);
	JS_RESUMEREQUEST(cx, rc);
	return JS_TRUE;
}


static JSBool
js_prompt(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char       instr[128] = "";
	JSString * str;
	sbbs_t*    sbbs;
	jsrefcount rc;
	char*      prompt = NULL;
	int32      mode = K_EDIT;
	size_t     result;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	if ((sbbs = (sbbs_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	uintN argn = 0;
	if (argc > argn && JSVAL_IS_STRING(argv[argn])) {
		JSVALUE_TO_MSTRING(cx, argv[argn], prompt, NULL);
		if (prompt == NULL)
			return JS_FALSE;
		argn++;
	}
	if (argc > argn && JSVAL_IS_STRING(argv[argn])) {
		JSVALUE_TO_STRBUF(cx, argv[argn], instr, sizeof(instr), NULL);
		argn++;
	}
	if (argc > argn && JSVAL_IS_NUMBER(argv[argn])) {
		if (!JS_ValueToInt32(cx, argv[argn], &mode)) {
			free(prompt);
			return JS_FALSE;
		}
		argn++;
	}

	rc = JS_SUSPENDREQUEST(cx);
	if (prompt != NULL) {
		sbbs->bprintf("\1n\1y\1h%s\1w: ", prompt);
		free(prompt);
	}

	result = sbbs->getstr(instr, sizeof(instr) - 1, mode);
	sbbs->attr(LIGHTGRAY);
	if (!result) {
		JS_RESUMEREQUEST(cx, rc);
		return JS_TRUE;
	}
	JS_RESUMEREQUEST(cx, rc);

	if ((str = JS_NewStringCopyZ(cx, instr)) == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(str));
	return JS_TRUE;
}

static jsSyncMethodSpec js_global_functions[] = {
	{"log",             js_log,             1,  JSTYPE_STRING,  JSDOCSTR("[<i>number</i> level=LOG_INFO,] value [,value]")
	 , JSDOCSTR("Add a line of text to the server and/or system log.<br>"
		        "<i>values</i> are typically string constants or variables (each logged as a separate log message),<br>"
		        "<i>level</i> is the severity of the message to be logged, one of the globally-defined values, in decreasing severity:<br>"
		        "<tt>LOG_EMERG, LOG_ALERT, LOG_CRIT, LOG_ERR, LOG_WARNING, LOG_NOTICE, LOG_INFO, and LOG_DEBUG</tt> (default: <tt>LOG_INFO</tt>)")
	 , 311
	},
	{"read",            js_read,            0,  JSTYPE_STRING,  JSDOCSTR("[count=128]")
	 , JSDOCSTR("Read up to <tt>count</tt> characters from input stream and return as a string or <tt>undefined</tt> upon error")
	 , 311
	},
	{"readln",          js_readln,          0,  JSTYPE_STRING,  JSDOCSTR("[count=128]")
	 , JSDOCSTR("Read a single line, up to <tt>count</tt> characters, from input stream and return as a string or <tt>undefined</tt> upon error")
	 , 311
	},
	{"write",           js_write,           0,  JSTYPE_VOID,    JSDOCSTR("value [,value]")
	 , JSDOCSTR("Send one or more values (typically strings) to the output stream")
	 , 311
	},
	{"write_raw",       js_write_raw,           0,  JSTYPE_VOID,    JSDOCSTR("value [,value]")
	 , JSDOCSTR("Send a stream of bytes (possibly containing NUL or special control code sequences) to the output stream")
	 , 314
	},
	{"print",           js_writeln,         0,  JSTYPE_ALIAS },
	{"writeln",         js_writeln,         0,  JSTYPE_VOID,    JSDOCSTR("value [,value]")
	 , JSDOCSTR("Send a line of text to the output stream with automatic line termination (CRLF), "
		        "<i>values</i> are typically string constants or variables (AKA print)")
	 , 311
	},
	{"printf",          js_printf,          1,  JSTYPE_STRING,  JSDOCSTR("<i>string</i> format [,value][,value]")
	 , JSDOCSTR("Send a C-style formatted string of text to the output stream.  See also the <tt>format()</tt> function.")
	 , 310
	},
	{"alert",           js_alert,           1,  JSTYPE_VOID,    JSDOCSTR("value")
	 , JSDOCSTR("Send an alert message (ala client-side JS) to the output stream")
	 , 310
	},
	{"prompt",          js_prompt,          1,  JSTYPE_STRING,  JSDOCSTR("[<i>string</i> text] [,<i>string</i> value] [,<i>number</i> k_mode=K_EDIT]")
	 , JSDOCSTR("Display a prompt (<tt>text</tt>) and return a string of user input (ala client-side JS) or <tt>null</tt> upon no-input<br>"
		        "<tt>value</tt> is an optional default string to be edited (used with the <tt>k_mode K_EDIT</tt> flag)<br>"
		        "See <tt>sbbsdefs.js</tt> for all valid <tt>K_</tt> (keyboard-input) mode flags.")
	 , 310
	},
	{"confirm",         js_confirm,         1,  JSTYPE_BOOLEAN, JSDOCSTR("value")
	 , JSDOCSTR("Display a Yes/No prompt and return <tt>true</tt> or <tt>false</tt> "
		        "based on user's confirmation (ala client-side JS, <tt>true</tt> = yes)<br>"
		        "see also <tt>console.yesno()<tt>")
	 , 310
	},
	{"deny",            js_deny,            1,  JSTYPE_BOOLEAN, JSDOCSTR("value")
	 , JSDOCSTR("Display a No/Yes prompt and returns <tt>true</tt> or <tt>false</tt> "
		        "based on user's denial (<tt>true</tt> = no)<br>"
		        "see also <tt>console.noyes()<tt>")
	 , 31501
	},
	{0}
};

static void
js_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
	char        line[64];
	char        file[MAX_PATH + 1];
	sbbs_t*     sbbs;
	const char* warning;
	jsrefcount  rc;
	int         log_level;

	if ((sbbs = (sbbs_t*)JS_GetContextPrivate(cx)) == NULL)
		return;

	if (report == NULL) {
		sbbs->lprintf(LOG_ERR, "!JavaScript: %s", message);
		return;
	}

	if (report->filename)
		SAFEPRINTF(file, " %s", report->filename);
	else
		file[0] = 0;

	if (report->lineno)
		SAFEPRINTF(line, " line %d", report->lineno);
	else
		line[0] = 0;

	if (JSREPORT_IS_WARNING(report->flags)) {
		if (JSREPORT_IS_STRICT(report->flags))
			warning = "strict warning ";
		else
			warning = "warning ";
		log_level = LOG_WARNING;
	} else {
		warning = nulstr;
		log_level = LOG_ERR;
	}

	rc = JS_SUSPENDREQUEST(cx);
	sbbs->lprintf(log_level, "!JavaScript %s%s%s: %s", warning, file, line, message);
	if (sbbs->online == ON_REMOTE)
		sbbs->bprintf("!JavaScript %s%s%s: %s\r\n", warning, getfname(file), line, message);
	JS_RESUMEREQUEST(cx, rc);
}

void sbbs_t::js_cleanup(void)
{
	/* Free Context */
	if (js_cx != NULL) {
		lprintf(LOG_DEBUG, "JavaScript: Destroying context");
		JS_BEGINREQUEST(js_cx);
		JS_RemoveObjectRoot(js_cx, &js_glob);
		JS_ENDREQUEST(js_cx);
		JS_DestroyContext(js_cx);
		js_cx = NULL;
	}

	if (js_runtime != NULL) {
		lprintf(LOG_DEBUG, "JavaScript: Destroying runtime");
		jsrt_Release(js_runtime);
		js_runtime = NULL;
	}

	if (js_hotkey_cx != NULL) {
		lprintf(LOG_DEBUG, "JavaScript: Destroying HotKey context");
		JS_BEGINREQUEST(js_hotkey_cx);
		JS_RemoveObjectRoot(js_hotkey_cx, &js_hotkey_glob);
		JS_ENDREQUEST(js_hotkey_cx);
		JS_DestroyContext(js_hotkey_cx);
		js_hotkey_cx = NULL;
	}

	if (js_hotkey_runtime != NULL) {
		lprintf(LOG_DEBUG, "JavaScript: Destroying HotKey runtime");
		jsrt_Release(js_hotkey_runtime);
		js_hotkey_runtime = NULL;
	}

}

bool sbbs_t::js_create_user_objects(JSContext* cx, JSObject* glob)
{
	bool result = false;
	if (cx != NULL) {
		JS_BEGINREQUEST(cx);
		if (!js_CreateUserObjects(cx, glob, &cfg, &useron, &client, startup == NULL ? NULL :startup->web_file_vpath_prefix, subscan, mqtt))
			errprintf(LOG_ERR, WHERE, "!JavaScript ERROR creating user objects");
		else
			result = true;
		JS_ENDREQUEST(cx);
	}
	return result;
}

extern "C" bool js_CreateCommonObjects(JSContext* js_cx
                                       , scfg_t* cfg                /* common */
                                       , scfg_t* node_cfg           /* node-specific */
                                       , jsSyncMethodSpec* methods  /* global */
                                       , time_t uptime              /* system */
                                       , char* host_name            /* system */
                                       , char* socklib_desc         /* system */
                                       , js_callback_t* cb          /* js */
                                       , js_startup_t* js_startup   /* js */
                                       , client_t* client           /* client */
                                       , SOCKET client_socket       /* client */
                                       , CRYPT_CONTEXT session      /* client */
                                       , js_server_props_t* props   /* server */
                                       , JSObject** glob
                                       , struct mqtt* mqtt
                                       , const char* web_file_vpath_prefix
                                       )
{
	bool success = false;

	if (node_cfg == NULL)
		node_cfg = cfg;

	/* Global Object */
	if (!js_CreateGlobalObject(js_cx, node_cfg, methods, js_startup, glob))
		return false;

	do {
		/*
		 * NOTE: Where applicable, anything added here should also be added to
		 *       the same function in jsdoor.c (ie: anything not Synchronet
		 *       specific).
		 */

		/* System Object */
		if (js_CreateSystemObject(js_cx, *glob, node_cfg, uptime, host_name, socklib_desc, mqtt) == NULL)
			break;

		/* Internal JS Object */
		if (cb != NULL
		    && js_CreateInternalJsObject(js_cx, *glob, cb, js_startup) == NULL)
			break;

		/* Client Object */
		if (client != NULL
		    && js_CreateClientObject(js_cx, *glob, "client", client, client_socket, session) == NULL)
			break;

		/* Server */
		if (props != NULL
		    && js_CreateServerObject(js_cx, *glob, props) == NULL)
			break;

		/* Socket Class */
		if (js_CreateSocketClass(js_cx, *glob) == NULL)
			break;

		/* Queue Class */
		if (js_CreateQueueClass(js_cx, *glob) == NULL)
			break;

		/* MsgBase Class */
		if (js_CreateMsgBaseClass(js_cx, *glob) == NULL)
			break;

		/* FileBase Class */
		if (js_CreateFileBaseClass(js_cx, *glob) == NULL)
			break;

		/* File Class */
		if (js_CreateFileClass(js_cx, *glob) == NULL)
			break;

		/* Archive Class */
		if (js_CreateArchiveClass(js_cx, *glob, cfg->supported_archive_formats) == NULL)
			break;

		/* User class */
		if (js_CreateUserClass(js_cx, *glob) == NULL)
			break;

		/* COM Class */
		if (js_CreateCOMClass(js_cx, *glob) == NULL)
			break;

		/* CryptContext Class */
		if (js_CreateCryptContextClass(js_cx, *glob) == NULL)
			break;

		/* CryptKeyset Class */
		if (js_CreateCryptKeysetClass(js_cx, *glob) == NULL)
			break;

		/* CryptCert Class */
		if (js_CreateCryptCertClass(js_cx, *glob) == NULL)
			break;

#if defined USE_MOSQUITTO
		if (js_CreateMQTTClass(js_cx, *glob) == NULL)
			break;
#endif
		/* Area Objects */
		if (!js_CreateUserObjects(js_cx, *glob, cfg, /* user: */ NULL, client, web_file_vpath_prefix, /* subscan: */ NULL, mqtt))
			break;

		success = true;
	} while (0);

	if (!success)
		JS_RemoveObjectRoot(js_cx, glob);

	return success;
}
