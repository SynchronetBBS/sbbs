/* js_server.c */

/* Synchronet JavaScript "server" Object */

/* $Id: js_server.c,v 1.19 2018/02/20 11:56:27 rswindell Exp $ */

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

/* System Object Properites */
enum {
	 SERVER_PROP_VER
	,SERVER_PROP_VER_DETAIL
	,SERVER_PROP_INTERFACE
	,SERVER_PROP_OPTIONS
	,SERVER_PROP_CLIENTS
};

static JSBool js_server_get(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	jsval				idval;
    jsint				tiny;
	js_server_props_t*	p;
	char * *			interface;
	char *				ipv4;
	char *				colon;

	if((p=(js_server_props_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	switch(tiny) {
		case SERVER_PROP_VER:
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,p->version));
			break;
		case SERVER_PROP_VER_DETAIL:
			if(p->version_detail!=NULL)
				*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,p->version_detail));
			break;
		case SERVER_PROP_INTERFACE:
			for (interface = *p->interfaces; *interface; interface++) {
				if (strchr(*interface, '.')) {
					ipv4 = strdup(*interface);
					if ((colon = strchr(ipv4, ':')))
						*colon = 0;
					*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,ipv4));
					free(ipv4);
					return JS_TRUE;
				}
			}
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,"255.255.255.255"));
			break;
		case SERVER_PROP_OPTIONS:
			if(p->options!=NULL)
				*vp=UINT_TO_JSVAL(*p->options);
			break;
		case SERVER_PROP_CLIENTS:
			if(p->clients!=NULL)
				*vp=UINT_TO_JSVAL(*p->clients);
			break;
	}

	return(JS_TRUE);
}

static JSBool js_server_set(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
	jsval idval;
    jsint				tiny;
	js_server_props_t*	p;

	if((p=(js_server_props_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	switch(tiny) {
		case SERVER_PROP_OPTIONS:
			if(p->options!=NULL) {
				if(!JS_ValueToInt32(cx, *vp, (int32*)p->options))
					return JS_FALSE;
			}
			break;
	}

	return(TRUE);
}


#define PROP_FLAGS JSPROP_ENUMERATE|JSPROP_READONLY

static jsSyncPropertySpec js_server_properties[] = {
/*		 name,						tinyid,					flags,			ver	*/

	{	"version",					SERVER_PROP_VER,		PROP_FLAGS,			310 },
	{	"version_detail",			SERVER_PROP_VER_DETAIL,	PROP_FLAGS,			310 },
	{	"interface_ip_address",		SERVER_PROP_INTERFACE,	PROP_FLAGS,			311 },
	{	"options",					SERVER_PROP_OPTIONS,	JSPROP_ENUMERATE,	311 },
	{	"clients",					SERVER_PROP_CLIENTS,	PROP_FLAGS,			311 },
	{0}
};

#ifdef BUILD_JSDOCS
static char* server_prop_desc[] = {

	 "server name and version number"
	,"detailed version/build information"
	,"First bound IPv4 address (<tt>0.0.0.0</tt> = <i>ANY</i>) (obsolete since 3.17, see interface_ip_addr_list)"
	,"bit-field of server-specific startup options"
	,"number of active clients (if available)"
	,"Array of IP addresses of bound network interface (<tt>0.0.0.0</tt> = <i>ANY</i>)"
	,NULL
};
#endif

static void remove_port_part(char *host)
{
	char *p=strchr(host, 0)-1;

	if (isdigit(*p)) {
		/*
		 * If the first and last : are not the same, and it doesn't
		 * start with '[', there's no port part.
		 */
		if (host[0] != '[') {
			if (strchr(host, ':') != strrchr(host, ':'))
				return;
		}
		for(; p >= host; p--) {
			if (*p == ':') {
				*p = 0;
				break;
			}
			if (!isdigit(*p))
				break;
		}
	}
	// Now, remove []s...
	if (host[0] == '[') {
		memmove(host, host+1, strlen(host));
		p=strchr(host, ']');
		if (p)
			*p = 0;
	}
}

static JSBool js_server_resolve(JSContext *cx, JSObject *obj, jsid id)
{
	char*			name=NULL;
	JSBool			ret;
	jsval			val;
	char			*str;
	JSObject*		newobj;
	uint			i;
	js_server_props_t*	props;

	if(id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;
		
		JS_IdToValue(cx, id, &idval);
		if(JSVAL_IS_STRING(idval)) {
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
			HANDLE_PENDING(cx, name);
		}
	}

	/* interface_ip_address property */
	if(name==NULL || strcmp(name, "interface_ip_addr_list")==0) {
		if(name) free(name);

		if((props=(js_server_props_t*)JS_GetPrivate(cx,obj))==NULL)
			return(JS_FALSE);

		if((newobj=JS_NewArrayObject(cx, 0, NULL))==NULL)
			return(JS_FALSE);

		if(!JS_SetParent(cx, newobj, obj))
			return(JS_FALSE);

		if(!JS_DefineProperty(cx, obj, "interface_ip_addr_list", OBJECT_TO_JSVAL(newobj)
				, NULL, NULL, JSPROP_ENUMERATE))
			return(JS_FALSE);

		for (i=0; (*props->interfaces)[i]; i++) {
			str = strdup((*props->interfaces)[i]);
			if (str == NULL)
				return JS_FALSE;
			remove_port_part(str);
			val=STRING_TO_JSVAL(JS_NewStringCopyZ(cx,str));
			free(str);
			JS_SetElement(cx, newobj, i, &val);
		}
		JS_DeepFreezeObject(cx, newobj);
		if(name) return(JS_TRUE);
	}

	ret = js_SyncResolve(cx, obj, name, js_server_properties, NULL, NULL, 0);
	if(name)
		free(name);
	return ret;
}

static JSBool js_server_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_server_resolve(cx, obj, JSID_VOID));
}

static JSClass js_server_class = {
     "Server"				/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_server_get			/* getProperty	*/
	,js_server_set			/* setProperty	*/
	,js_server_enumerate	/* enumerate	*/
	,js_server_resolve		/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,JS_FinalizeStub		/* finalize		*/
};

JSObject* DLLCALL js_CreateServerObject(JSContext* cx, JSObject* parent
										,js_server_props_t* props)
{
	JSObject*	obj;

	if((obj = JS_DefineObject(cx, parent, "server", &js_server_class, NULL
		,JSPROP_ENUMERATE|JSPROP_READONLY))==NULL)
		return(NULL);

	if(!JS_SetPrivate(cx, obj, props))
		return(NULL);

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx,obj,"Server-specifc properties",310);
	js_CreateArrayOfStrings(cx,obj,"_property_desc_list", server_prop_desc, JSPROP_READONLY);
#endif

	return(obj);
}

