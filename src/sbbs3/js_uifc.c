/* js_uifc.c */

/* Synchronet "uifc" (user interface) object */

/* $Id: js_uifc.c,v 1.46 2020/04/12 20:30:47 rswindell Exp $ */

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

#ifndef JAVASCRIPT
#define JAVASCRIPT
#endif

#include "sbbs.h"
#include "uifc.h"
#include "ciolib.h"
#include "js_request.h"

struct list_ctx_private {
	int	cur;
	int bar;
	int left;
	int top;
	int width;
};

enum {
	 PROP_CUR
	,PROP_BAR
	,PROP_LEFT
	,PROP_TOP
	,PROP_WIDTH
};

static JSBool js_list_ctx_get(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	jsval idval;
    jsint		tiny;
	struct list_ctx_private*	p;

	if((p=(struct list_ctx_private*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	switch(tiny) {
		case PROP_CUR:
			*vp=INT_TO_JSVAL(p->cur);
			break;
		case PROP_BAR:
			*vp=INT_TO_JSVAL(p->bar);
			break;
		case PROP_LEFT:
			*vp=INT_TO_JSVAL(p->left);
			break;
		case PROP_TOP:
			*vp=INT_TO_JSVAL(p->top);
			break;
		case PROP_WIDTH:
			*vp=INT_TO_JSVAL(p->width);
			break;
	}
	return JS_TRUE;
}

static JSBool js_list_ctx_set(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
	jsval idval;
    jsint		tiny;
	int32		i=0;
	struct list_ctx_private*	p;

	if((p=(struct list_ctx_private*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	if(!JS_ValueToInt32(cx, *vp, &i))
		return JS_FALSE;

	switch(tiny) {
		case PROP_CUR:
			p->cur=i;
			break;
		case PROP_BAR:
			p->bar=i;
			break;
		case PROP_LEFT:
			p->left=i;
			break;
		case PROP_TOP:
			p->top=i;
			break;
		case PROP_WIDTH:
			p->width=i;
			break;
	}
	return JS_TRUE;
}

#ifdef BUILD_JSDOCS
static char* uifc_list_ctx_prop_desc[] = {
	 "Currently selected item"
	,"0-based Line number in the currently displayed set that is highlighted"
	,"left column"
	,"top line"
	,"forced width"
	,NULL
};
#endif

/* Destructor */

static void 
js_list_ctx_finalize(JSContext *cx, JSObject *obj)
{
	struct list_ctx_private*	p;

	if((p=(struct list_ctx_private*)JS_GetPrivate(cx,obj))==NULL)
		return;

	free(p);
	JS_SetPrivate(cx,obj,NULL);
}

static JSClass js_uifc_list_ctx_class = {
     "CTX"					/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_list_ctx_get		/* getProperty	*/
	,js_list_ctx_set		/* setProperty	*/
	,JS_EnumerateStub		/* enumerate	*/
	,JS_ResolveStub			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,js_list_ctx_finalize	/* finalize		*/
};

static jsSyncPropertySpec js_uifc_list_class_properties[] = {
/*       name               ,tinyid                 ,flags,             ver */

    {   "cur"             ,PROP_CUR 				,JSPROP_ENUMERATE,  317 },
    {   "bar"  		      ,PROP_BAR    				,JSPROP_ENUMERATE,  317 },
    {0}
};

/* Constructor */
static JSBool js_list_ctx_constructor(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj = JS_THIS_OBJECT(cx, arglist);
	struct list_ctx_private*	p;
	
	obj = JS_NewObject(cx, &js_uifc_list_ctx_class, NULL, NULL);
	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(obj));
	if ((p = (struct list_ctx_private *)calloc(1, sizeof(struct list_ctx_private)))==NULL) {
		JS_ReportError(cx, "calloc failed");
		return JS_FALSE;
	}
	if(!JS_SetPrivate(cx, obj, p)) {
		JS_ReportError(cx, "JS_SetPrivate failed");
		return JS_FALSE;
	}

	js_SyncResolve(cx, obj, NULL, js_uifc_list_class_properties, NULL, NULL, 0);

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx, obj, "Class used to retain UIFC list menu context", 317);
	js_DescribeSyncConstructor(cx, obj, "To create a new UIFCListContext object: <tt>var ctx = new UIFCListContext();</tt>");
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", uifc_list_ctx_prop_desc, JSPROP_READONLY);
#endif

	return JS_TRUE;
}

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

static JSBool js_get(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	jsval idval;
    jsint		tiny;
	uifcapi_t*	uifc;

	if((uifc=(uifcapi_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	switch(tiny) {
		case PROP_INITIALIZED:
			*vp=BOOLEAN_TO_JSVAL(uifc->initialized);
			break;
		case PROP_MODE:
			*vp=UINT_TO_JSVAL(uifc->mode);
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

static JSBool js_set(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
	jsval idval;
    jsint		tiny;
	int32		i=0;
	uifcapi_t*	uifc;

	if((uifc=(uifcapi_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	if(tiny==PROP_CHANGES)
		return JS_ValueToBoolean(cx,*vp,&uifc->changes);
	else if(tiny==PROP_HELPBUF) {
		if(uifc->helpbuf)
			free(uifc->helpbuf);
		JSVALUE_TO_MSTRING(cx, *vp, uifc->helpbuf, NULL);
		HANDLE_PENDING(cx, NULL);
		return JS_TRUE;
	}

	if(!JS_ValueToInt32(cx, *vp, &i))
		return JS_FALSE;

	switch(tiny) {
		case PROP_CHANGES:
			uifc->changes=i;
			break;
		case PROP_MODE:
			uifc->mode=i;
			break;
		case PROP_SAVNUM:
			uifc->savnum=i;
			break;
		case PROP_SCRN_LEN:
			uifc->scrn_len=i;
			break;
		case PROP_SCRN_WIDTH:
			uifc->scrn_width=i;
			break;
		case PROP_ESC_DELAY:
			uifc->esc_delay=i;
			break;
		case PROP_LIST_HEIGHT:
			uifc->list_height=i;
			break;
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

#ifdef BUILD_JSDOCS
static char* uifc_prop_desc[] = {
	 "uifc has been initialized"
	,"current mode bits (see uifcdefs.js)"
	,"a change has occured in an input call.  You are expected to set this to false before calling the input if you care about it."
	,"save buffer depth (advanced)"
	,"current screen length"
	,"current screen width"
	,"when WIN_FIXEDHEIGHT is set, specifies the hight used by a list method"
	,"delay before a single ESC char is parsed and assumed to not be an ANSI sequence (advanced)"
	,"text that will be displayed if F1 is pressed"
	,"background colour"
	,"frame colour"
	,"text colour"
	,"inverse colour"
	,"lightbar colour"
	,NULL
};
#endif

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
js_uifc_init(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	int		ciolib_mode=CIOLIB_MODE_AUTO;
	const char*	title_def="Synchronet";
	char*	title=(char *)title_def;
	char*	mode;
	uifcapi_t* uifc;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if((uifc=(uifcapi_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	if(argc) {
		JSVALUE_TO_MSTRING(cx, argv[0], title, NULL);
		HANDLE_PENDING(cx, title);
		if(title==NULL)
			return(JS_TRUE);
	}

	if(argc>1) {
		JSVALUE_TO_ASTRING(cx, argv[1], mode, 7, NULL);
		if(mode != NULL) {
			if(!stricmp(mode,"STDIO"))
				ciolib_mode=-1;
			else if(!stricmp(mode,"AUTO"))
				ciolib_mode=CIOLIB_MODE_AUTO;
			else if(!stricmp(mode,"X"))
				ciolib_mode=CIOLIB_MODE_X;
			else if(!stricmp(mode,"CURSES"))
				ciolib_mode=CIOLIB_MODE_CURSES;
			else if(!stricmp(mode,"CURSES_IBM"))
				ciolib_mode=CIOLIB_MODE_CURSES_IBM;
			else if(!stricmp(mode,"CURSES_ASCII"))
				ciolib_mode=CIOLIB_MODE_CURSES_ASCII;
			else if(!stricmp(mode,"ANSI"))
				ciolib_mode=CIOLIB_MODE_ANSI;
			else if(!stricmp(mode,"CONIO"))
				ciolib_mode=CIOLIB_MODE_CONIO;
			else if(!stricmp(mode,"SDL"))
				ciolib_mode=CIOLIB_MODE_SDL;
		}
	}

	rc=JS_SUSPENDREQUEST(cx);
	if(ciolib_mode==-1) {
		if(uifcinix(uifc)) {
			JS_RESUMEREQUEST(cx, rc);
			if(title != title_def)
				free(title);
			return(JS_TRUE);
		}
	} else {
		if(initciolib(ciolib_mode)) {
			JS_RESUMEREQUEST(cx, rc);
			if(title != title_def)
				free(title);
			return(JS_TRUE);
		}

		if(uifcini32(uifc)) {
			JS_RESUMEREQUEST(cx, rc);
			if(title != title_def)
				free(title);
			return(JS_TRUE);
		}
	}

	JS_SET_RVAL(cx, arglist, JSVAL_TRUE);
	uifc->scrn(title);
	if(title != title_def)
		free(title);
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_uifc_bail(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	uifcapi_t* uifc;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((uifc=get_uifc(cx,obj))==NULL)
		return(JS_FALSE);

	rc=JS_SUSPENDREQUEST(cx);
	uifc->bail();
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_uifc_showhelp(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	uifcapi_t* uifc;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((uifc=get_uifc(cx,obj))==NULL)
		return(JS_FALSE);

	rc=JS_SUSPENDREQUEST(cx);
	uifc->showhelp();
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_uifc_msg(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char*		str = NULL;
	uifcapi_t*	uifc;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((uifc=get_uifc(cx,obj))==NULL)
		return(JS_FALSE);

	JSVALUE_TO_MSTRING(cx, argv[0], str, NULL);
	HANDLE_PENDING(cx, str);
	if(str==NULL)
		return(JS_TRUE);

	rc=JS_SUSPENDREQUEST(cx);
	uifc->msg(str);
	free(str);
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_uifc_pop(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char*		str=NULL;
	uifcapi_t*	uifc;
	jsrefcount	rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((uifc=get_uifc(cx,obj))==NULL)
		return(JS_FALSE);

	if(argc) {
		JSVALUE_TO_MSTRING(cx, argv[0], str, NULL);
		HANDLE_PENDING(cx, str);
	}

	rc=JS_SUSPENDREQUEST(cx);
	uifc->pop(str);
	if(str)
		free(str);
	JS_RESUMEREQUEST(cx, rc);
	return(JS_TRUE);
}

static JSBool
js_uifc_input(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
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

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

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
	if(argn<argc && JSVAL_IS_STRING(argv[argn])) {
		JSVALUE_TO_MSTRING(cx, argv[argn], prompt, NULL);
		argn++;
		HANDLE_PENDING(cx, prompt);
		if(prompt==NULL)
			return(JS_TRUE);
	}
	if(argn<argc && JSVAL_IS_STRING(argv[argn])) {
		JSVALUE_TO_MSTRING(cx, argv[argn], org, NULL);
		argn++;
		if(JS_IsExceptionPending(cx)) {
			if(prompt)
				free(prompt);
			return JS_FALSE;
		}
		if(org==NULL) {
			if(prompt)
				free(prompt);
			return(JS_TRUE);
		}
	}
	if(argn<argc && JSVAL_IS_NUMBER(argv[argn]) 
		&& !JS_ValueToInt32(cx,argv[argn++],&maxlen)) {
		if(prompt)
			free(prompt);
		if(org)
			free(org);
		return(JS_FALSE);
	}
	if(argn<argc && JSVAL_IS_NUMBER(argv[argn]) 
		&& !JS_ValueToInt32(cx,argv[argn++],&kmode)) {
		if(prompt)
			free(prompt);
		if(org)
			free(org);
		return(JS_FALSE);
	}

	if(!maxlen)
		maxlen=40;

	if((str=(char*)malloc(maxlen+1))==NULL) {
		if(prompt)
			free(prompt);
		if(org)
			free(org);
		return(JS_FALSE);
	}

	memset(str,0,maxlen+1);

	if(org) {
		strncpy(str,org,maxlen);
		free(org);
	}

	rc=JS_SUSPENDREQUEST(cx);
	if(uifc->input(mode, left, top, prompt, str, maxlen, kmode)<0) {
		JS_RESUMEREQUEST(cx, rc);
		if(prompt)
			free(prompt);
		if(str)
			free(str);
		return(JS_TRUE);
	}
	if(prompt)
		free(prompt);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(JS_NewStringCopyZ(cx,str)));
	if(str)
		free(str);

	return(JS_TRUE);
}

static JSBool
js_uifc_list(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	char*		title=NULL;
	int32		left=0;
	int32		top=0;
	int32		width=0;
	int32		dflt=0;
	int32		*dptr = &dflt;
	int32		bar=0;
	int32		*bptr = &bar;
	int32		mode=0;
	JSObject*	objarg;
	uifcapi_t*	uifc;
	uintN		argn=0;
	jsval		val;
	jsuint      i;
	jsuint		numopts;
	str_list_t	opts=NULL;
	char		*opt=NULL;
	size_t		opt_sz=0;
	jsrefcount	rc;
	struct list_ctx_private *p;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((uifc=get_uifc(cx,obj))==NULL)
		return(JS_FALSE);

	if(argn<argc && JSVAL_IS_NUMBER(argv[argn]) 
		&& !JS_ValueToInt32(cx,argv[argn++],&mode))
		return(JS_FALSE);
	for(; argn<argc; argn++) {
		if(JSVAL_IS_STRING(argv[argn])) {
			JSVALUE_TO_MSTRING(cx, argv[argn], title, NULL);
			HANDLE_PENDING(cx, title);
			continue;
		}
		if(!JSVAL_IS_OBJECT(argv[argn]))
			continue;
		if((objarg = JSVAL_TO_OBJECT(argv[argn]))==NULL)
			return(JS_FALSE);
		if(JS_IsArrayObject(cx, objarg)) {
			if(!JS_GetArrayLength(cx, objarg, &numopts))
				return(JS_TRUE);
			if(opts == NULL)
				opts=strListInit();
			for(i=0;i<numopts;i++) {
				if(!JS_GetElement(cx, objarg, i, &val))
					break;
				JSVALUE_TO_RASTRING(cx, val, opt, &opt_sz, NULL);
				if(JS_IsExceptionPending(cx)) {
					if(title)
						free(title);
				}
				strListPush(&opts,opt);
			}
			FREE_AND_NULL(opt);
		}
		else if(JS_GetClass(cx, objarg) == &js_uifc_list_ctx_class) {
			p = JS_GetPrivate(cx, objarg);
			if (p != NULL) {
				dptr = &(p->cur);
				bptr = &(p->bar);
				left = p->left;
				top = p->top;
				width = p->width;
			}
		}
	}
	if(title == NULL || opts == NULL) {
		JS_SET_RVAL(cx, arglist, JS_FALSE);
	} else {
		rc=JS_SUSPENDREQUEST(cx);
		JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(uifc->list(mode,left,top,width,(int*)dptr,(int*)bptr,title,opts)));
		JS_RESUMEREQUEST(cx, rc);
	}
	strListFree(&opts);
	if(title != NULL)
		free(title);
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
	{"init",            js_uifc_init,       1,	JSTYPE_BOOLEAN,	JSDOCSTR("string title [, string mode]")
	,JSDOCSTR("initialize.  <tt>mode</tt> is a string representing the desired conio mode... one of STDIO, AUTO, "
	"X, CURSES, ANSI, CONIO, or SDL.")
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
	{"input",			js_uifc_input,		0,	JSTYPE_STRING,	JSDOCSTR("[number mode] [number left] [number top] [string default] [number maxlen] [number kmode]")
	,JSDOCSTR("prompt for a string input")
	,314
	},
	{"list",			js_uifc_list,		0,	JSTYPE_STRING,	JSDOCSTR("[number mode] [string title] [string array options] [uifc.list.CTX object]")
	,JSDOCSTR("select from a list of options.<br>"
		"The context object can be created using new uifc.list.CTX() and if the same object is passed, allows WIN_SAV to work correctly.<br>"
		"The context object has the following properties:<br>cur, bar, top, left, width"
	)
	,314
	},
	{"showhelp",		js_uifc_showhelp,	0,	JSTYPE_VOID,	JSDOCSTR("")
	,JSDOCSTR("Shows the current help text")
	,317
	},
	{0}
};

static JSBool js_uifc_resolve(JSContext *cx, JSObject *obj, jsid id)
{
	char*			name=NULL;
	JSBool			ret;
	jsval			objval;
	JSObject*		tobj;

	if(id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;

		JS_IdToValue(cx, id, &idval);
		if(JSVAL_IS_STRING(idval)) {
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
			HANDLE_PENDING(cx, name);
		}
	}

	ret=js_SyncResolve(cx, obj, name, js_properties, js_functions, NULL, 0);
	if (name == NULL || strcmp(name, "list") == 0) {
		if(JS_GetProperty(cx, obj, "list", &objval)) {
			tobj = JSVAL_TO_OBJECT(objval);
			if (tobj)
				JS_InitClass(cx, tobj, NULL, &js_uifc_list_ctx_class, js_list_ctx_constructor, 0, NULL, NULL, NULL, NULL);
		}
	}
	if(name)
		free(name);
	return ret;
}

static JSBool js_uifc_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_uifc_resolve(cx, obj, JSID_VOID));
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

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx,obj,"User InterFaCe object - used for jsexec menus" ,314);
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", uifc_prop_desc, JSPROP_READONLY);
#endif

	return(obj);
}

