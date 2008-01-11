/* js_server.c */

/* Synchronet JavaScript "server" Object */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2005 Rob Swindell - http://www.synchro.net/copyright.html		*
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

static JSBool js_server_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
	char*		ip;
    jsint       tiny;
	struct in_addr in_addr;
	js_server_props_t*	p;

	if((p=(js_server_props_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

    tiny = JSVAL_TO_INT(id);

	switch(tiny) {
		case SERVER_PROP_VER:
			if(p->version!=NULL)
				*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,p->version));
			break;
		case SERVER_PROP_VER_DETAIL:
			if(p->version_detail!=NULL)
				*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,p->version_detail));
			break;
		case SERVER_PROP_INTERFACE:
			if(p->interface_addr!=NULL) {
				in_addr.s_addr=*(p->interface_addr);
				in_addr.s_addr=htonl(in_addr.s_addr);
				if((ip=inet_ntoa(in_addr))!=NULL)
					*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,ip));
			}
			break;
		case SERVER_PROP_OPTIONS:
			if(p->options!=NULL)
				JS_NewNumberValue(cx,*p->options,vp);
			break;
		case SERVER_PROP_CLIENTS:
			if(p->clients!=NULL)
				JS_NewNumberValue(cx,*p->clients,vp);
			break;
	}

	return(JS_TRUE);
}

static JSBool js_server_set(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint				tiny;
	js_server_props_t*	p;

	if((p=(js_server_props_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

    tiny = JSVAL_TO_INT(id);

	switch(tiny) {
		case SERVER_PROP_OPTIONS:
			if(p->options!=NULL)
				JS_ValueToInt32(cx, *vp, (int32*)p->options);
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
	,"IP address of bound network interface (<tt>0.0.0.0</tt> = <i>ANY</i>)"
	,"bit-field of server-specific startup options"
	,"number of active clients (if available)"
	,NULL
};
#endif

static JSBool js_server_resolve(JSContext *cx, JSObject *obj, jsval id)
{
	char*			name=NULL;

	if(id != JSVAL_NULL)
		name=JS_GetStringBytes(JSVAL_TO_STRING(id));

	return(js_SyncResolve(cx, obj, name, js_server_properties, NULL, NULL, 0));
}

static JSBool js_server_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_server_resolve(cx, obj, JSVAL_NULL));
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

