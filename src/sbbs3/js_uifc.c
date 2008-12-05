/* js_uifc.c */

/* Synchronet "uifc" (user interface) object */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2006 Rob Swindell - http://www.synchro.net/copyright.html		*
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

/* Properties */
enum {
	 PROP_INITIALIZED	/* read-only */
	,PROP_MODE
	,PROP_CHANGES
	,PROP_SAVNUM
	,PROP_SCRN_LEN
    ,PROP_SCRN_WIDTH
	,PROP_ESC_DELAY
	,PROP_HELPBUF
	,PROP_HCOLOR
	,PROP_LCOLOR
	,PROP_BCOLOR
	,PROP_CCOLOR
	,PROP_LBCOLOR
	,PROP_LIST_HEIGHT
};

static JSBool js_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint		tiny;
	uifcapi_t*	uifc;

	if((uifc=(uifcapi_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

    tiny = JSVAL_TO_INT(id);

	switch(tiny) {
		case PROP_INITIALIZED:
			*vp=BOOLEAN_TO_JSVAL(uifc->initialized);
			break;
		case PROP_MODE:
			JS_NewNumberValue(cx,uifc->mode,vp);
			break;
		case PROP_CHANGES:
			*vp=BOOLEAN_TO_JSVAL(uifc->changes);
			break;
		case PROP_SAVNUM:
			*vp=INT_TO_JSVAL(uifc->savnum);
			break;
		case PROP_SCRN_LEN:
			*vp=INT_TO_JSVAL(uifc->scrn_len);
			break;
		case PROP_SCRN_WIDTH:
			*vp=INT_TO_JSVAL(uifc->scrn_width);
			break;
		case PROP_ESC_DELAY:
			*vp=INT_TO_JSVAL(uifc->esc_delay);
			break;
		case PROP_HELPBUF:
			*vp=STRING_TO_JSVAL(JS_NewStringCopyZ(cx,uifc->helpbuf));
			break;
		case PROP_HCOLOR:
			*vp=INT_TO_JSVAL(uifc->hclr);
			break;
		case PROP_LCOLOR:
			*vp=INT_TO_JSVAL(uifc->lclr);
			break;
		case PROP_BCOLOR:
			*vp=INT_TO_JSVAL(uifc->bclr);
			break;
		case PROP_CCOLOR:
			*vp=INT_TO_JSVAL(uifc->cclr);
			break;
		case PROP_LBCOLOR:
			*vp=INT_TO_JSVAL(uifc->lbclr);
			break;
		case PROP_LIST_HEIGHT:
			*vp=INT_TO_JSVAL(uifc->list_height);
			break;
	}

	return(JS_TRUE);
}

static JSBool js_set(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint		tiny;
	int32		i=0;
	uifcapi_t*	uifc;

	if((uifc=(uifcapi_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

    tiny = JSVAL_TO_INT(id);

	switch(tiny) {
		case PROP_MODE:
			JS_ValueToInt32(cx, *vp, (int32*)&uifc->mode);
			break;
		case PROP_CHANGES:
			JS_ValueToBoolean(cx,*vp,&uifc->changes);
			break;
		case PROP_SAVNUM:
			JS_ValueToInt32(cx, *vp, (int32*)&uifc->savnum);
			break;
		case PROP_SCRN_LEN:
			JS_ValueToInt32(cx, *vp, (int32*)&uifc->scrn_len);
			break;
		case PROP_SCRN_WIDTH:
			JS_ValueToInt32(cx, *vp, (int32*)&uifc->scrn_width);
			break;
		case PROP_ESC_DELAY:
			JS_ValueToInt32(cx, *vp, (int32*)&uifc->esc_delay);
			break;
		case PROP_HELPBUF:
			uifc->helpbuf=js_ValueToStringBytes(cx, *vp, NULL);
			break;
		case PROP_LIST_HEIGHT:
			JS_ValueToInt32(cx, *vp, (int32*)&uifc->list_height);
			break;
		case PROP_HCOLOR:
		case PROP_LCOLOR:
		case PROP_BCOLOR:
		case PROP_CCOLOR:
		case PROP_LBCOLOR:
			JS_ValueToInt32(cx, *vp, &i);
			switch(tiny) {
				case PROP_HCOLOR:
					uifc->hclr=(char)i;
					break;
				case PROP_LCOLOR:
					uifc->lclr=(char)i;
					break;
				case PROP_BCOLOR:
					uifc->bclr=(char)i;
					break;
				case PROP_CCOLOR:
					uifc->cclr=(char)i;
					break;
				case PROP_LBCOLOR:
					uifc->lbclr=(char)i;
					break;
			}
			break;
	}	

	return(JS_TRUE);
}

static jsSyncPropertySpec js_properties[] = {
/*		 name,				tinyid,						flags,		ver	*/

	{	"initialized",		PROP_INITIALIZED,	JSPROP_ENUMERATE|JSPROP_READONLY, 314 },
	{	"mode",				PROP_MODE,			JSPROP_ENUMERATE,	314 },
	{	"changes",			PROP_CHANGES,		JSPROP_ENUMERATE,	314 },
	{	"save_num",			PROP_SAVNUM,		JSPROP_ENUMERATE,	314 },
	{	"screen_length",	PROP_SCRN_LEN,		JSPROP_ENUMERATE,	314 },
	{	"screen_width",		PROP_SCRN_WIDTH,	JSPROP_ENUMERATE,	314 },
	{	"list_height",		PROP_LIST_HEIGHT,	JSPROP_ENUMERATE,	314 },
	{	"esc_delay",		PROP_ESC_DELAY,		JSPROP_ENUMERATE,	314 },
	{	"help_text",		PROP_HELPBUF,		JSPROP_ENUMERATE,	314 },
	{	"background_color",	PROP_BCOLOR,		JSPROP_ENUMERATE,	314 },
	{	"frame_color",		PROP_HCOLOR,		JSPROP_ENUMERATE,	314 },
	{	"text_color",		PROP_LCOLOR,		JSPROP_ENUMERATE,	314 },
	{	"inverse_color",	PROP_CCOLOR,		JSPROP_ENUMERATE,	314 },
	{	"lightbar_color",	PROP_LBCOLOR,		JSPROP_ENUMERATE,	314 },
	{0}
};

/* Convenience functions */
static uifcapi_t* get_uifc(JSContext *cx, JSObject *obj)
{
	uifcapi_t* uifc;

	if((uifc=(uifcapi_t*)JS_GetPrivate(cx,obj))==NULL)
		return(NULL);

	if(!uifc->initialized) {
		JS_ReportError(cx,"UIFC not initialized");
		return(NULL);
	}

	return(uifc);
}

/* Methods */

static JSBool
js_uifc_init(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	int		ciolib_mode=CIOLIB_MODE_AUTO;
	char*	title="Synchronet";
	char*	mode;
	uifcapi_t* uifc;
	jsrefcount	rc;

	*rval = JSVAL_FALSE;

	if((uifc=(uifcapi_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	if(argc && (title=js_ValueToStringBytes(cx, argv[0], NULL))==NULL)
		return(JS_FALSE);

	if(argc>1 && (mode=js_ValueToStringBytes(cx, argv[1], NULL))!=NULL) {
		if(!stricmp(mode,"STDIO"))
			ciolib_mode=-1;
		else if(!stricmp(mode,"AUTO"))
			ciolib_mode=CIOLIB_MODE_AUTO;
		else if(!stricmp(mode,"X"))
			ciolib_mode=CIOLIB_MODE_X;
		else if(!stricmp(mode,"ANSI"))
			ciolib_mode=CIOLIB_MODE_ANSI;
		else if(!stricmp(mode,"CONIO"))
			ciolib_mode=CIOLIB_MODE_CONIO;
	}

	rc=JS_SuspendRequest(cx);
	if(ciolib_mode==-1) {
		if(uifcinix(uifc)) {
			JS_ResumeRequest(cx, rc);
			return(JS_TRUE);
		}
	} else {
		if(initciolib(ciolib_mode)) {
			JS_ResumeRequest(cx, rc);
			return(JS_TRUE);
		}

		if(uifcini32(uifc)) {
			JS_ResumeRequest(cx, rc);
			return(JS_TRUE);
		}
	}

	*rval = JSVAL_TRUE;
	uifc->scrn(title);
	JS_ResumeRequest(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_uifc_bail(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	uifcapi_t* uifc;
	jsrefcount	rc;

	if((uifc=get_uifc(cx,obj))==NULL)
		return(JS_FALSE);

	rc=JS_SuspendRequest(cx);
	uifc->bail();
	JS_ResumeRequest(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_uifc_msg(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		str;
	uifcapi_t*	uifc;
	jsrefcount	rc;

	if((uifc=get_uifc(cx,obj))==NULL)
		return(JS_FALSE);

	if((str=js_ValueToStringBytes(cx, argv[0], NULL))==NULL)
		return(JS_FALSE);

	rc=JS_SuspendRequest(cx);
	uifc->msg(str);
	JS_ResumeRequest(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_uifc_pop(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		str=NULL;
	uifcapi_t*	uifc;
	jsrefcount	rc;

	if((uifc=get_uifc(cx,obj))==NULL)
		return(JS_FALSE);

	if(argc)
		str=js_ValueToStringBytes(cx, argv[0], NULL);

	rc=JS_SuspendRequest(cx);
	uifc->pop(str);
	JS_ResumeRequest(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_uifc_input(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		str;
	char*		org=NULL;
	char*		prompt=NULL;
	int32		maxlen=0;
	int32		left=0;
	int32		top=0;
	int32		mode=0;
	int32		kmode=0;
	uifcapi_t*	uifc;
	uintN		argn=0;
	jsrefcount	rc;

	if((uifc=get_uifc(cx,obj))==NULL)
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

	rc=JS_SuspendRequest(cx);
	if(uifc->input(mode, left, top, prompt, str, maxlen, kmode)<0) {
		rc=JS_ResumeRequest(cx, rc);
		return(JS_TRUE);
	}
	rc=JS_ResumeRequest(cx, rc);

	*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,str));

	return(JS_TRUE);
}

static JSBool
js_uifc_list(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	char*		title=NULL;
	int32		left=0;
	int32		top=0;
	int32		width=0;
	int32		dflt=0;
	int32		bar=0;
	int32		mode=0;
	JSObject*	objarg;
	uifcapi_t*	uifc;
	uintN		argn=0;
	jsval		val;
	jsuint      i;
	jsuint		numopts;
	str_list_t	opts=NULL;
	jsrefcount	rc;

	if((uifc=get_uifc(cx,obj))==NULL)
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

	rc=JS_SuspendRequest(cx);
    *rval = INT_TO_JSVAL(uifc->list(mode,left,top,width,(int*)&dflt,(int*)&bar,title,opts));
	strListFree(&opts);
	JS_SuspendRequest(cx, rc);
	return(JS_TRUE);
}

/* Destructor */

static void 
js_finalize(JSContext *cx, JSObject *obj)
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
	,314
	},		
	{"bail",			js_uifc_bail,		0,	JSTYPE_VOID,	JSDOCSTR("")
	,JSDOCSTR("uninitialize")
	,314
	},
	{"msg",				js_uifc_msg,		1,	JSTYPE_VOID,	JSDOCSTR("string text")
	,JSDOCSTR("print a message")
	,314
	},
	{"pop",				js_uifc_pop,		1,	JSTYPE_VOID,	JSDOCSTR("[string text]")
	,JSDOCSTR("popup (or down) a message")
	,314
	},
	{"input",			js_uifc_input,		0,	JSTYPE_STRING,	JSDOCSTR("[...]")
	,JSDOCSTR("prompt for a string input")
	,314
	},
	{"list",			js_uifc_list,		0,	JSTYPE_STRING,	JSDOCSTR("[...]")
	,JSDOCSTR("select from a list of options")
	,314
	},
	{0}
};

static JSBool js_uifc_resolve(JSContext *cx, JSObject *obj, jsval id)
{
	char*			name=NULL;

	if(id != JSVAL_NULL)
		name=JS_GetStringBytes(JSVAL_TO_STRING(id));

	return(js_SyncResolve(cx, obj, name, js_properties, js_functions, NULL, 0));
}

static JSBool js_uifc_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_uifc_resolve(cx, obj, JSVAL_NULL));
}

static JSClass js_uifc_class = {
     "UIFC"					/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_get					/* getProperty	*/
	,js_set					/* setProperty	*/
	,js_uifc_enumerate		/* enumerate	*/
	,js_uifc_resolve		/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,js_finalize			/* finalize		*/
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

	return(obj);
}

