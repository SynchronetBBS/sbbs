/* js_uifc.c */

/* Synchronet "uifc" (user interface) object */

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

#ifndef JAVASCRIPT
#define JAVASCRIPT
#endif

#include "sbbs.h"
#include "uifc.h"
#include "ciolib.h"

static JSBool
js_uifc_init(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int		ciolib_mode=CIOLIB_MODE_AUTO;
	char*	title="Synchronet";
	uifcapi_t* uifc;

	*rval = JSVAL_FALSE;

	if((uifc=(uifcapi_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	if(argc && (title=js_ValueToStringBytes(cx, argv[0], NULL))==NULL)
		return(JS_FALSE);

	if(initciolib(ciolib_mode))
		return(JS_TRUE);

    if(uifcini32(uifc))
		return(JS_TRUE);

	*rval = JSVAL_TRUE;
	uifc->scrn(title);
	return(JS_TRUE);
}

static JSBool
js_uifc_bail(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	uifcapi_t* uifc;

	if((uifc=(uifcapi_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	uifc->bail();
	clrscr();
	return(JS_TRUE);
}

static JSBool
js_uifc_msg(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		str;
	uifcapi_t*	uifc;

	if((uifc=(uifcapi_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	if((str=js_ValueToStringBytes(cx, argv[0], NULL))==NULL)
		return(JS_FALSE);

	uifc->msg(str);
	return(JS_TRUE);
}

static JSBool
js_uifc_pop(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		str=NULL;
	uifcapi_t*	uifc;

	if((uifc=(uifcapi_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	if(argc)
		str=js_ValueToStringBytes(cx, argv[0], NULL);

	uifc->pop(str);
	return(JS_TRUE);
}

static JSBool
js_uifc_input(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		str;
	char*		org=NULL;
	char*		prompt=NULL;
	int			maxlen=0;
	int			left=0;
	int			top=0;
	long		mode=0;
	long		kmode=0;
	uifcapi_t*	uifc;
	uintN		argn=0;

	if((uifc=(uifcapi_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	if(argn<argc && JSVAL_IS_NUMBER(argv[argn]) 
		&& !JS_ValueToInt32(cx,argv[argn++],&mode))
		return(JS_FALSE);
	if(argn<argc && JSVAL_IS_NUMBER(argv[argn]) 
		&& !JS_ValueToInt32(cx,argv[argn++],&left))
		return(JS_FALSE);
	if(argn<argc && JSVAL_IS_NUMBER(argv[argn]) 
		&& !JS_ValueToInt32(cx,argv[argn++],&top))
		return(JS_FALSE);
	if(argn<argc && JSVAL_IS_STRING(argv[argn]) 
		&& (prompt=js_ValueToStringBytes(cx,argv[argn++],NULL))==NULL)
		return(JS_FALSE);
	if(argn<argc && JSVAL_IS_STRING(argv[argn]) 
		&& (org=js_ValueToStringBytes(cx,argv[argn++],NULL))==NULL)
		return(JS_FALSE);
	if(argn<argc && JSVAL_IS_NUMBER(argv[argn]) 
		&& !JS_ValueToInt32(cx,argv[argn++],&maxlen))
		return(JS_FALSE);
	if(argn<argc && JSVAL_IS_NUMBER(argv[argn]) 
		&& !JS_ValueToInt32(cx,argv[argn++],&kmode))
		return(JS_FALSE);

	if(!maxlen)
		maxlen=40;

	if((str=(char*)alloca(maxlen+1))==NULL)
		return(JS_FALSE);

	memset(str,0,maxlen+1);

	if(org)
		strncpy(str,org,maxlen);

	if(uifc->input(mode, left, top, prompt, str, maxlen, kmode)<0)
		return(JS_TRUE);

	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,str));

	return(JS_TRUE);
}

static JSBool
js_uifc_list(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		title="Title";
	int			left=0;
	int			top=0;
	int			width=0;
	int			dflt=0;
	int			bar=0;
	long		mode=0;
	JSObject*	objarg;
	uifcapi_t*	uifc;
	uintN		argn=0;
	jsval		val;
	jsuint      i;
	jsuint		numopts;
	str_list_t	opts=NULL;

	if((uifc=(uifcapi_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	if(argn<argc && JSVAL_IS_NUMBER(argv[argn]) 
		&& !JS_ValueToInt32(cx,argv[argn++],&mode))
		return(JS_FALSE);
	if(argn<argc && JSVAL_IS_NUMBER(argv[argn]) 
		&& !JS_ValueToInt32(cx,argv[argn++],&left))
		return(JS_FALSE);
	if(argn<argc && JSVAL_IS_NUMBER(argv[argn]) 
		&& !JS_ValueToInt32(cx,argv[argn++],&top))
		return(JS_FALSE);
	if(argn<argc && JSVAL_IS_NUMBER(argv[argn]) 
		&& !JS_ValueToInt32(cx,argv[argn++],&width))
		return(JS_FALSE);
	if(argn<argc && JSVAL_IS_NUMBER(argv[argn]) 
		&& !JS_ValueToInt32(cx,argv[argn++],&dflt))
		return(JS_FALSE);
	if(argn<argc && JSVAL_IS_NUMBER(argv[argn]) 
		&& !JS_ValueToInt32(cx,argv[argn++],&bar))
		return(JS_FALSE);
	if(argn<argc && JSVAL_IS_STRING(argv[argn]) 
		&& (title=js_ValueToStringBytes(cx,argv[argn++],NULL))==NULL)
		return(JS_FALSE);
	if(argn<argc && JSVAL_IS_OBJECT(argv[argn])) {
		if((objarg = JSVAL_TO_OBJECT(argv[argn++]))==NULL)
			return(JS_FALSE);
		if(JS_IsArrayObject(cx, objarg)) {
			if(!JS_GetArrayLength(cx, objarg, &numopts))
				return(JS_TRUE);
			opts=strListInit();
			for(i=0;i<numopts;i++) {
				if(!JS_GetElement(cx, objarg, i, &val))
					break;
				strListPush(&opts,js_ValueToStringBytes(cx,val,NULL));
			}
		}
	}

    *rval = INT_TO_JSVAL(uifc->list(mode,left,top,width,&dflt,&bar,title,opts));
	strListFree(&opts);
	return(JS_TRUE);
}


static void 
js_uifc_finalize(JSContext *cx, JSObject *obj)
{
	uifcapi_t* p;

	if((p=(uifcapi_t*)JS_GetPrivate(cx,obj))==NULL)
		return;
	
	free(p);
	JS_SetPrivate(cx,obj,NULL);
}

static jsSyncMethodSpec js_functions[] = {
	{"init",            js_uifc_init,       1,	JSTYPE_BOOLEAN,	JSDOCSTR("string title")
	,JSDOCSTR("initialize")
	,313
	},		
	{"bail",			js_uifc_bail,		0,	JSTYPE_VOID,	JSDOCSTR("")
	,JSDOCSTR("uninitialize")
	,313
	},
	{"msg",				js_uifc_msg,		1,	JSTYPE_VOID,	JSDOCSTR("string text")
	,JSDOCSTR("print a message")
	,313
	},
	{"pop",				js_uifc_pop,		1,	JSTYPE_VOID,	JSDOCSTR("[string text]")
	,JSDOCSTR("popup (or down) a message")
	,313
	},
	{"input",			js_uifc_input,		0,	JSTYPE_STRING,	JSDOCSTR("[...]")
	,JSDOCSTR("prompt for a string input")
	,313
	},
	{"list",			js_uifc_list,		0,	JSTYPE_STRING,	JSDOCSTR("[...]")
	,JSDOCSTR("select from a list of options")
	,313
	},
	{0}
};


static JSClass js_uifc_class = {
     "UIFC"					/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,JS_PropertyStub		/* getProperty	*/
	,JS_PropertyStub		/* setProperty	*/
	,JS_EnumerateStub		/* enumerate	*/
	,JS_ResolveStub			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,js_uifc_finalize		/* finalize		*/
};

JSObject* js_CreateUifcObject(JSContext* cx, JSObject* parent)
{
	JSObject*	obj;
	uifcapi_t*	api;

	if((obj = JS_DefineObject(cx, parent, "uifc", &js_uifc_class, NULL
		,JSPROP_ENUMERATE|JSPROP_READONLY))==NULL)
		return(NULL);

	if((api=(uifcapi_t*)malloc(sizeof(uifcapi_t)))==NULL)
		return(NULL);

	memset(api,0,sizeof(uifcapi_t));
	api->size=sizeof(uifcapi_t);
	api->esc_delay=25;

	if(!JS_SetPrivate(cx, obj, api))	/* Store a pointer to uifcapi_t */
		return(NULL);
#if 0
	if(!js_DefineSyncProperties(cx, obj, js_properties))	/* expose them */
		return(NULL);
#endif
	if(!js_DefineSyncMethods(cx, obj, js_functions, /* append? */ FALSE)) 
		return(NULL);

	return(obj);
}

