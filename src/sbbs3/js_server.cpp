/* Synchronet JavaScript "server" Object */

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

/* System Object Properites */
enum {
	SERVER_PROP_VER
	, SERVER_PROP_VER_DETAIL
	, SERVER_PROP_INTERFACE
	, SERVER_PROP_OPTIONS
	, SERVER_PROP_CLIENTS
};

static JSBool js_server_get_value(JSContext* cx, js_server_props_t* p, jsint tiny, jsval* vp)
{
	JS::RootedString  js_str(cx);
	char**            iface;
	char*             ipv4;
	char*             colon;

	switch (tiny) {
		case SERVER_PROP_VER:
			if ((js_str = JS_NewStringCopyZ(cx, p->version)) == nullptr)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(js_str.get());
			break;
		case SERVER_PROP_VER_DETAIL:
			if (p->version_detail != NULL) {
				if ((js_str = JS_NewStringCopyZ(cx, p->version_detail)) == nullptr)
					return JS_FALSE;
				*vp = STRING_TO_JSVAL(js_str.get());
			}
			break;
		case SERVER_PROP_INTERFACE:
			for (iface = *p->interfaces; *iface; iface++) {
				if (strchr(*iface, '.')) {
					ipv4 = strdup(*iface);
					if ((colon = strchr(ipv4, ':')))
						*colon = 0;
					js_str = JS_NewStringCopyZ(cx, ipv4);
					free(ipv4);
					if (js_str == nullptr)
						return JS_FALSE;
					*vp = STRING_TO_JSVAL(js_str.get());
					return JS_TRUE;
				}
			}
			if ((js_str = JS_NewStringCopyZ(cx, "255.255.255.255")) == nullptr)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(js_str.get());
			break;
		case SERVER_PROP_OPTIONS:
			if (p->options != NULL)
				*vp = UINT_TO_JSVAL(*p->options);
			break;
		case SERVER_PROP_CLIENTS:
			if (p->clients != NULL)
				*vp = UINT_TO_JSVAL(protected_uint32_value(*p->clients));
			break;
		default:
			return JS_TRUE;
	}
	return JS_TRUE;
}

static JSBool js_server_set_value(JSContext* cx, js_server_props_t* p, jsint tiny, jsval* vp)
{
	switch (tiny) {
		case SERVER_PROP_OPTIONS:
			if (p->options != NULL) {
				if (!JS_ValueToInt32(cx, *vp, (int32*)p->options))
					return JS_FALSE;
			}
			break;
	}

	return JS_TRUE;
}

#define PROP_FLAGS JSPROP_ENUMERATE | JSPROP_READONLY

static jsSyncPropertySpec js_server_properties[] = {
/*		 name,						tinyid,					flags,			ver	*/

	{   "version",                  SERVER_PROP_VER,        PROP_FLAGS,         310 },
	{   "version_detail",           SERVER_PROP_VER_DETAIL, PROP_FLAGS,         310 },
	{   "interface_ip_address",     SERVER_PROP_INTERFACE,  PROP_FLAGS,         311 },
	{   "options",                  SERVER_PROP_OPTIONS,    JSPROP_ENUMERATE,   311 },
	{   "clients",                  SERVER_PROP_CLIENTS,    PROP_FLAGS,         311 },
	{0}
};

#ifdef BUILD_JSDOCS
static const char* server_prop_desc[] = {

	"Server name and version number"
	, "Detailed version/build information"
	, "First bound IPv4 address (<tt>0.0.0.0</tt> = <i>ANY</i>) (obsolete since 3.17, see interface_ip_addr_list)"
	, "Bit-field of server-specific startup options"
	, "Number of active clients (if available)"
	, "Array of IP addresses of bound network interface (<tt>0.0.0.0</tt> = <i>ANY</i>)"
	, NULL
};
#endif

static void remove_port_part(char *host)
{
	char *p = strchr(host, 0) - 1;

	if (IS_DIGIT(*p)) {
		/*
		 * If the first and last : are not the same, and it doesn't
		 * start with '[', there's no port part.
		 */
		if (host[0] != '[') {
			if (strchr(host, ':') != strrchr(host, ':'))
				return;
		}
		for (; p >= host; p--) {
			if (*p == ':') {
				*p = 0;
				break;
			}
			if (!IS_DIGIT(*p))
				break;
		}
	}
	// Now, remove []s...
	if (host[0] == '[') {
		memmove(host, host + 1, strlen(host));
		p = strchr(host, ']');
		if (p)
			*p = 0;
	}
}

static bool js_server_prop_setter(JSContext* cx, unsigned argc, JS::Value* vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject thisObj(cx);
	if (!args.computeThis(cx, &thisObj))
		return false;
	js_server_props_t* p = (js_server_props_t*)JS_GetPrivate(thisObj);
	if (p == nullptr)
		return true;
	JSObject* callee = &args.callee();
	jsint tiny = js::GetFunctionNativeReserved(callee, 0).toInt32();
	jsval val = args.length() > 0 ? args[0] : JSVAL_VOID;
	if (!js_server_set_value(cx, p, tiny, &val))
		return false;
	args.rval().set(val);
	return true;
}

static bool js_server_prop_getter(JSContext* cx, unsigned argc, JS::Value* vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject thisObj(cx);
	if (!args.computeThis(cx, &thisObj))
		return false;
	js_server_props_t* p = (js_server_props_t*)JS_GetPrivate(thisObj);
	if (p == nullptr) {
		args.rval().setUndefined();
		return true;
	}
	JSObject* callee = &args.callee();
	jsint tiny = js::GetFunctionNativeReserved(callee, 0).toInt32();
	jsval val = JSVAL_VOID;
	if (!js_server_get_value(cx, p, tiny, &val))
		return false;
	args.rval().set(val);
	return true;
}

static JSBool js_server_fill_properties(JSContext* cx, JSObject* obj, js_server_props_t* p, const char* name)
{
	(void)p;
	return js_DefineSyncAccessors(cx, obj, js_server_properties, js_server_prop_getter, name, js_server_prop_setter);
}

static bool js_server_resolve_impl(JSContext* cx, JSObject* areaobj, char* name);

bool js_server_resolve(JSContext* cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id, bool* resolvedp)
{
	char* name = NULL;

	if (id.get().isString()) {
		JSString* jstr = id.get().toString();
		JSSTRING_TO_MSTRING(cx, jstr, name, NULL);
		HANDLE_PENDING(cx, name);
		if (name == NULL) return false;
	}

	bool ret = js_server_resolve_impl(cx, obj, name);
	if (name)
		free(name);
	if (resolvedp) *resolvedp = ret;
	return true;
}

static bool js_server_resolve_impl(JSContext* cx, JSObject* rawobj, char* name)
{
	JS::RootedObject   obj(cx, rawobj);
	bool               ret;
	jsval              val;
	char *             str;
	JS::RootedObject   newobj(cx);
	uint               i;
	js_server_props_t* props;

	/* interface_ip_address property */
	if (name == NULL || strcmp(name, "interface_ip_addr_list") == 0) {
		if ((props = (js_server_props_t*)JS_GetPrivate(cx, obj)) == NULL)
			return false;

		newobj = JS_NewArrayObject(cx, 0, NULL);
		if (!newobj)
			return false;

		if (!JS_DefineProperty(cx, obj, "interface_ip_addr_list", OBJECT_TO_JSVAL(newobj)
		                       , NULL, NULL, JSPROP_ENUMERATE))
			return false;

		for (i = 0; (*props->interfaces)[i]; i++) {
			str = strdup((*props->interfaces)[i]);
			if (str == NULL)
				return false;
			remove_port_part(str);
			val = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, str));
			free(str);
			JS_SetElement(cx, newobj, i, &val);
		}
		JS_DeepFreezeObject(cx, newobj.get());
		if (name)
			return true;
	}

	ret = js_SyncResolve(cx, obj, name, js_server_properties, NULL, NULL, 0);
	if (ret) {
		js_server_props_t* p = (js_server_props_t*)JS_GetPrivate(cx, obj);
		if (p != NULL)
			js_server_fill_properties(cx, obj, p, name);
	}
	return ret;
}

static bool js_server_enumerate(JSContext *cx, JS::Handle<JSObject*> obj)
{
	return js_server_resolve_impl(cx, obj, NULL);
}

static const JSClassOps js_server_classops = {
	nullptr,                /* addProperty  */
	nullptr,                /* delProperty  */
	js_server_enumerate,    /* enumerate    */
	nullptr,                /* newEnumerate */
	js_server_resolve,      /* resolve      */
	nullptr,                /* mayResolve   */
	nullptr,                /* finalize     */
	nullptr, nullptr, nullptr /* call, construct, trace */
};

static JSClass js_server_class = {
	"Server"
	, JSCLASS_HAS_PRIVATE
	, &js_server_classops
};

JSObject* js_CreateServerObject(JSContext* cx, JSObject* parent
                                , js_server_props_t* props)
{
	JSObject* obj;

	if ((obj = JS_DefineObject(cx, parent, "server", &js_server_class, NULL
	                           , JSPROP_ENUMERATE | JSPROP_READONLY)) == NULL)
		return NULL;

	if (!JS_SetPrivate(cx, obj, props))
		return NULL;

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx, obj, "Server-specific properties", 310);
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", server_prop_desc, JSPROP_READONLY);
#endif

	return obj;
}

