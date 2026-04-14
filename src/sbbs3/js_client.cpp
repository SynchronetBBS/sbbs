/* Synchronet JavaScript "Client" Object */

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

#ifdef JAVASCRIPT

/* Client Object Properites */
enum {
	CLIENT_PROP_ADDR        /* IP address */
	, CLIENT_PROP_HOST       /* host name */
	, CLIENT_PROP_PORT       /* TCP port number */
	, CLIENT_PROP_TIME       /* connect time */
	, CLIENT_PROP_PROTOCOL   /* protocol description */
	, CLIENT_PROP_USER       /* user name */
	, CLIENT_PROP_USERNUM    /* user number */
};

#ifdef BUILD_JSDOCS
static const char* client_prop_desc[] = {
	"Client's IPv4 or IPv6 address"
	, "Client's host name (up to 64 characters)"
	, "Client's TCP or UDP port number"
	, "Date/time of initial connection (in time_t format)"
	, "Protocol/service name (e.g. 'Telnet', 'FTP', etc.)"
	, "User's name/alias"
	, "User's number (non-zero if logged in)"

	/* this next one must be last */
	, "Instance of <a href=#Socket_class>Socket class</a> representing client's TCP/IP connection"
	, NULL
};
#endif

#define CLIENT_PROP_FLAGS JSPROP_ENUMERATE | JSPROP_READONLY

static jsSyncPropertySpec js_client_properties[] = {
/*		 name				,tinyid					,flags,					ver	*/

	{   "ip_address", CLIENT_PROP_ADDR, CLIENT_PROP_FLAGS,     310},
	{   "host_name", CLIENT_PROP_HOST, CLIENT_PROP_FLAGS,     310},
	{   "port", CLIENT_PROP_PORT, CLIENT_PROP_FLAGS,     310},
	{   "connect_time", CLIENT_PROP_TIME, CLIENT_PROP_FLAGS,     310},
	{   "protocol", CLIENT_PROP_PROTOCOL, CLIENT_PROP_FLAGS,     310},
	{   "user_name", CLIENT_PROP_USER, CLIENT_PROP_FLAGS,     310},
	{   "user_number", CLIENT_PROP_USERNUM, CLIENT_PROP_FLAGS,     31702},
	{0}
};

static JSBool js_client_get_value(JSContext* cx, client_t* client, jsint tiny, jsval* vp)
{
	const char*        p = NULL;
	int32              val = 0;
	JS::RootedString   js_str(cx);

	switch (tiny) {
		case CLIENT_PROP_ADDR:
			p = client->addr;
			break;
		case CLIENT_PROP_HOST:
			p = client->host;
			break;
		case CLIENT_PROP_PORT:
			val = client->port;
			break;
		case CLIENT_PROP_TIME:
			val = (int32)client->time;
			break;
		case CLIENT_PROP_PROTOCOL:
			p = (char*)client->protocol;
			break;
		case CLIENT_PROP_USER:
			p = client->user;
			break;
		case CLIENT_PROP_USERNUM:
			val = client->usernum;
			break;
		default:
			return JS_TRUE;
	}
	if (p != NULL) {
		if ((js_str = JS_NewStringCopyZ(cx, p)) == nullptr)
			return JS_FALSE;
		*vp = STRING_TO_JSVAL(js_str.get());
	} else {
		*vp = INT_TO_JSVAL(val);
	}
	return JS_TRUE;
}

static bool js_client_prop_getter(JSContext* cx, unsigned argc, JS::Value* vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject thisObj(cx);
	if (!args.computeThis(cx, &thisObj))
		return false;
	client_t* client = (client_t*)JS_GetPrivate(thisObj);
	if (client == nullptr) {
		args.rval().setUndefined();
		return true;
	}
	JSObject* callee = &args.callee();
	jsint tiny = js::GetFunctionNativeReserved(callee, 0).toInt32();
	jsval val = JSVAL_VOID;
	if (!js_client_get_value(cx, client, tiny, &val))
		return false;
	args.rval().set(val);
	return true;
}

static JSBool js_client_fill_properties(JSContext* cx, JSObject* obj, client_t* client, const char* name)
{
	(void)client;
	return js_DefineSyncAccessors(cx, obj, js_client_properties, js_client_prop_getter, name);
}

static bool js_client_resolve(JSContext *cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id, bool* resolvedp)
{
	char*     name = NULL;
	client_t* client;

	if (id.get().isString()) {
		JSString* str = id.get().toString();
		JSSTRING_TO_MSTRING(cx, str, name, NULL);
		HANDLE_PENDING(cx, name);
		if (name == NULL) return false;
	}

	bool ret = js_SyncResolve(cx, obj, name, js_client_properties, NULL, NULL, 0);
	if (ret && (client = (client_t*)JS_GetPrivate(cx, obj)) != NULL)
		js_client_fill_properties(cx, obj, client, name);
	if (name)
		free(name);
	if (resolvedp) *resolvedp = ret;
	if (JS_IsExceptionPending(cx))
		JS_ClearPendingException(cx);
	return true;
}

static bool js_client_enumerate(JSContext *cx, JS::Handle<JSObject*> obj)
{
	client_t* client;

	if (!js_SyncResolve(cx, obj, NULL, js_client_properties, NULL, NULL, 0))
		return false;
	if ((client = (client_t*)JS_GetPrivate(cx, obj)) != NULL)
		js_client_fill_properties(cx, obj, client, NULL);
	return true;
}

static const JSClassOps js_client_classops = {
	nullptr,                /* addProperty  */
	nullptr,                /* delProperty  */
	js_client_enumerate,    /* enumerate    */
	nullptr,                /* newEnumerate */
	js_client_resolve,      /* resolve      */
	nullptr,                /* mayResolve   */
	nullptr,                /* finalize     */
	nullptr, nullptr, nullptr /* call, construct, trace */
};

static JSClass js_client_class = {
	"Client"
	, JSCLASS_HAS_PRIVATE
	, &js_client_classops
};

JSObject* js_CreateClientObject(JSContext* cx, JSObject* parent
                                , const char* name, client_t* client, SOCKET sock, CRYPT_CONTEXT session)
{
	JS::RootedObject obj(cx);

	obj = JS_DefineObject(cx, parent, name, &js_client_class, NULL
	                      , JSPROP_ENUMERATE | JSPROP_READONLY);

	if (!obj)
		return NULL;

	JS_SetPrivate(cx, obj, client); /* Store a pointer to client_t */

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx, obj.get(), "Represents a TCP/IP client session", 310);
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", client_prop_desc, JSPROP_READONLY);
#endif

	js_CreateSocketObject(cx, obj, "socket", sock, session);

	return obj;
}

#endif  /* JAVSCRIPT */
