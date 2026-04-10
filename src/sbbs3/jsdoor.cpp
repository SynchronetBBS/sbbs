/* Execute a BBS JavaScript module from the command-line */

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

#ifdef __unix__
#define _WITH_GETLINE
#include <signal.h>
#endif

#include "sbbs.h"
#include "ciolib.h"
#include "ini_file.h"
#include "js_rtpool.h"
#include "jsdebug.h"

extern scfg_t scfg;

void js_do_lock_input(JSContext *cx, JSBool lock)
{
	return;
}

size_t js_cx_input_pending(JSContext *cx)
{
	return 0;
}

void* DLLCALL js_GetClassPrivate(JSContext *cx, JSObject *obj, JSClass* cls)
{
	void *ret = JS_GetInstancePrivate(cx, obj, cls, NULL);

	if (ret == NULL)
		JS_ReportError(cx, "'%s' instance: No Private Data or Class Mismatch"
		               , cls == NULL ? "???" : cls->name);
	return ret;
}

void call_socket_open_callback(bool open)
{
}

SOCKET open_socket(int domain, int type, const char* protocol)
{
	SOCKET sock;
	char   error[256];

	sock = socket(domain, type, IPPROTO_IP);
	if (sock != INVALID_SOCKET && set_socket_options(&scfg, sock, protocol, error, sizeof(error)))
		lprintf(LOG_ERR, "%04d !ERROR %s", sock, error);

	return sock;
}

SOCKET accept_socket(SOCKET s, union xp_sockaddr* addr, socklen_t* addrlen)
{
	SOCKET sock;

	sock = accept(s, &addr->addr, addrlen);

	return sock;
}

int close_socket(SOCKET sock)
{
	int result;

	if (sock == INVALID_SOCKET || sock == 0)
		return 0;

	shutdown(sock, SHUT_RDWR);   /* required on Unix */
	result = closesocket(sock);
	if (result != 0 && ERROR_VALUE != ENOTSOCK)
		lprintf(LOG_WARNING, "!ERROR %d closing socket %d", ERROR_VALUE, sock);
	return result;
}

DLLEXPORT void DLLCALL sbbs_srand()
{
	DWORD seed;

	xp_randomize();
#if defined(HAS_DEV_RANDOM) && defined(RANDOM_DEV)
	int   rf, rd = 0;

	if ((rf = open(RANDOM_DEV, O_RDONLY | O_NONBLOCK)) != -1) {
		rd = read(rf, &seed, sizeof(seed));
		close(rf);
	}
	if (rd != sizeof(seed))
#endif
	seed = time32(NULL) ^ (uintmax_t)GetCurrentThreadId();

	srand(seed);
	sbbs_random(10);    /* Throw away first number */
}

int DLLCALL sbbs_random(int n)
{
	return xp_random(n);
}


#ifdef BUILD_JSDOCS

/* This is a stream-lined version of JS_DefineConstDoubles */
JSBool
js_DefineConstIntegers(JSContext* cx, JSObject* obj, jsConstIntSpec* ints, int flags)
{
	uint  i;
	jsval val;
	JS::RootedObject robj(cx, obj);

	for (i = 0; ints[i].name; i++) {
		val = INT_TO_JSVAL(ints[i].val);

		if (!JS_DefineProperty(cx, robj.get(), ints[i].name, val, NULL, NULL, flags))
			return JS_FALSE;
	}

	return JS_TRUE;
}

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
			                                 props[i].name, props[i].tinyid, JSVAL_VOID, NULL, NULL, props[i].flags ))
				return JS_FALSE;
		}
		else {
			if (!JS_DefineProperty(cx, obj, props[i].name, JSVAL_VOID, NULL, NULL, props[i].flags ))
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
	JS::RootedObject robj(cx, obj);

	/*
	 * NOTE: Any changes here should also be added to the same function in jsdoor.c
	 *       (ie: anything not Synchronet specific).
	 */
	for (i = 0; props[i].name; i++) {
		/* SM128: strip JSPROP_READONLY so fill_properties can set the actual
		 * value via JS_SetProperty.  JSPROP_RESOLVING prevents resolve-hook
		 * recursion during this define. */
		unsigned flags = (props[i].flags & ~(JSPROP_READONLY | JSPROP_PERMANENT)) | JSPROP_RESOLVING;
		if (props[i].tinyid < 256 && props[i].tinyid > -129) {
			if (!JS_DefinePropertyWithTinyId(cx, robj.get(),
			                                 props[i].name, props[i].tinyid, JSVAL_VOID, NULL, NULL, flags))
				return JS_FALSE;
		}
		else {
			if (!JS_DefineProperty(cx, robj.get(), props[i].name, JSVAL_VOID, NULL, NULL, flags))
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
js_DefineSyncAccessors(JSContext* cx, JSObject* obj,
                       jsSyncPropertySpec* props, JSNative getter,
                       const char* name, JSNative setter)
{
	JS::RootedObject robj(cx, obj);

	for (uint i = 0; props[i].name; i++) {
		if (name != NULL && strcmp(name, props[i].name) != 0)
			continue;
		JSFunction* getterFunc = js::NewFunctionWithReserved(cx, getter,
			0, 0, props[i].name);
		if (!getterFunc)
			return JS_FALSE;
		JSObject* getterObj = JS_GetFunctionObject(getterFunc);
		js::SetFunctionNativeReserved(getterObj, 0, JS::Int32Value(props[i].tinyid));
		JS::RootedObject getterRoot(cx, getterObj);
		JS::RootedObject setterRoot(cx, nullptr);
		if (setter != nullptr && !(props[i].flags & JSPROP_READONLY)) {
			JSFunction* setterFunc = js::NewFunctionWithReserved(cx, setter,
				1, 0, props[i].name);
			if (!setterFunc)
				return JS_FALSE;
			JSObject* setterObj = JS_GetFunctionObject(setterFunc);
			js::SetFunctionNativeReserved(setterObj, 0, JS::Int32Value(props[i].tinyid));
			setterRoot.set(setterObj);
		}
		if (!JS_DefineProperty(cx, robj, props[i].name,
				getterRoot, setterRoot, JSPROP_ENUMERATE))
			return JS_FALSE;
		if (name != NULL)
			return JS_TRUE;
	}
	return JS_TRUE;
}

JSBool
js_SyncResolve(JSContext* cx, JSObject* obj, char *name, jsSyncPropertySpec* props, jsSyncMethodSpec* funcs, jsConstIntSpec* consts, int flags)
{
	uint  i;
	jsval val;
	JSBool found;

	/*
	 * NOTE: Any changes here should also be added to the same function in jsdoor.c
	 *       (ie: anything not Synchronet specific).
	 * JSPROP_RESOLVING must be passed to JS_Define* calls made from a resolve/enumerate
	 * hook — it tells the engine to skip the resolve hook during the lookup that precedes
	 * property definition, preventing accidental infinite recursion (SM78+ requirement).
	 * When enumerating all properties (name==NULL), skip any already defined to avoid
	 * "can't redefine non-configurable property" errors (e.g. after a native accessor
	 * override was applied for a specific property on first access).
	 */
	if (props) {
		for (i = 0; props[i].name; i++) {
			if (name == NULL || strcmp(name, props[i].name) == 0) {
				if (name == NULL && JS_HasProperty(cx, obj, props[i].name, &found) && found) {
					continue;
				}
				if (props[i].tinyid < 256 && props[i].tinyid > -129) {
					if (!JS_DefinePropertyWithTinyId(cx, obj,
					                                 props[i].name, props[i].tinyid, JSVAL_VOID, NULL, NULL, props[i].flags  | JSPROP_RESOLVING))
						return JS_FALSE;
				}
				else {
					if (!JS_DefineProperty(cx, obj, props[i].name, JSVAL_VOID, NULL, NULL, props[i].flags  | JSPROP_RESOLVING))
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
				if (name == NULL && JS_HasProperty(cx, obj, funcs[i].name, &found) && found) {
					continue;
				}
				if (!JS_DefineFunction(cx, obj, funcs[i].name, funcs[i].call, funcs[i].nargs, JSPROP_RESOLVING))
					return JS_FALSE;
				if (name)
					return JS_TRUE;
			}
		}
	}
	if (consts) {
		for (i = 0; consts[i].name; i++) {
			if (name == NULL || strcmp(name, consts[i].name) == 0) {
				if (name == NULL && JS_HasProperty(cx, obj, consts[i].name, &found) && found) {
					continue;
				}
				val = INT_TO_JSVAL(consts[i].val);

				if (!JS_DefineProperty(cx, obj, consts[i].name, val, NULL, NULL, flags | JSPROP_RESOLVING))
					return JS_FALSE;

				if (name)
					return JS_TRUE;
			}
		}
	}

	return JS_TRUE;
}

#endif

// Needed for load()
JSObject* js_CreateBbsObject(JSContext* cx, JSObject* parent)
{
	return NULL;
}
JSObject* js_CreateConsoleObject(JSContext* cx, JSObject* parent)
{
	return NULL;
}

bool DLLCALL js_CreateCommonObjects(JSContext* js_cx
                                    , scfg_t *unused1
                                    , scfg_t *unused2
                                    , jsSyncMethodSpec* methods     /* global */
                                    , time_t uptime                 /* system */
                                    , const char* host_name         /* system */
                                    , const char* socklib_desc      /* system */
                                    , js_callback_t* cb             /* js */
                                    , js_startup_t* js_startup      /* js */
									, user_t* user                  /* user */
                                    , client_t* client              /* client */
                                    , SOCKET client_socket          /* client */
                                    , CRYPT_CONTEXT session         /* client */
                                    , js_server_props_t* props      /* server */
                                    , JSObject** glob
                                    , struct mqtt* mqtt
                                    )
{
	bool success = FALSE;

	/* Global Object */
	if (!js_CreateGlobalObject(js_cx, &scfg, methods, js_startup, glob))
		return FALSE;

	/* SM128: root the global so compacting GC during object creation
	 * can update the pointer; write back at end for the caller. */
	JS::RootedObject glob_root(js_cx, *glob);

#ifdef JS_HAS_CTYPES
	JS_InitCTypesClass(js_cx, glob_root);
#endif

	do {
		/* System Object */
		if (js_CreateSystemObject(js_cx, glob_root, &scfg, uptime, host_name, socklib_desc, mqtt) == NULL)
			break;

		/* Internal JS Object */
		if (cb != NULL
		    && js_CreateInternalJsObject(js_cx, glob_root, cb, js_startup) == NULL)
			break;

		/* Client Object */
		if (client != NULL
		    && js_CreateClientObject(js_cx, glob_root, "client", client, client_socket, session) == NULL)
			break;

		/* Server */
		if (props != NULL
		    && js_CreateServerObject(js_cx, glob_root, props) == NULL)
			break;

		/* Socket Class */
		if (js_CreateSocketClass(js_cx, glob_root) == NULL)
			break;

		/* Queue Class */
		if (js_CreateQueueClass(js_cx, glob_root) == NULL)
			break;

		/* File Class */
		if (js_CreateFileClass(js_cx, glob_root) == NULL)
			break;

		/* Archive Class */
		if (js_CreateArchiveClass(js_cx, glob_root, NULL) == NULL)
			break;

		/* COM Class */
		if (js_CreateCOMClass(js_cx, glob_root) == NULL)
			break;

		/* CryptContext Class */
		if (js_CreateCryptContextClass(js_cx, glob_root) == NULL)
			break;

		/* CryptKeyset Class */
		if (js_CreateCryptKeysetClass(js_cx, glob_root) == NULL)
			break;

		/* CryptCert Class */
		if (js_CreateCryptCertClass(js_cx, glob_root) == NULL)
			break;

		success = TRUE;
	} while (0);

	*glob = glob_root;
	return success;
}

#define PROG_NAME   "JSDoor"
#define PROG_NAME_LC    "jsdoor"
#define JSDOOR

#include "jsexec.cpp"
#include "js_system.cpp"
#include "ver.cpp"
