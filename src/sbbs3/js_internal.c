/* js_internal.c */

/* Synchronet "js" object, for internal JavaScript branch and GC control */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2003 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#include <jscntxt.h>	/* Needed for Context-private data structure */

enum {
	 PROP_BRANCH_COUNTER
	,PROP_BRANCH_LIMIT
	,PROP_YIELD_INTERVAL
	,PROP_GC_INTERVAL
#ifdef jscntxt_h___
	,PROP_GC_BYTES
	,PROP_GC_LASTBYTES
	,PROP_GC_MAXBYTES
	,PROP_GC_COUNTER
#endif
};

static JSBool js_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint			tiny;
	js_branch_t*	branch;

	if((branch=(js_branch_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

    tiny = JSVAL_TO_INT(id);

	switch(tiny) {
		case PROP_BRANCH_COUNTER:
			JS_NewNumberValue(cx,branch->counter,vp);
			break;
		case PROP_BRANCH_LIMIT:
			JS_NewNumberValue(cx,branch->limit,vp);
			break;
		case PROP_YIELD_INTERVAL:
			JS_NewNumberValue(cx,branch->yield_freq,vp);
			break;
		case PROP_GC_INTERVAL:
			JS_NewNumberValue(cx,branch->gc_freq,vp);
			break;
#ifdef jscntxt_h___
		case PROP_GC_BYTES:
			JS_NewNumberValue(cx,cx->runtime->gcBytes,vp);
			break;
		case PROP_GC_LASTBYTES:
			JS_NewNumberValue(cx,cx->runtime->gcLastBytes,vp);
			break;
		case PROP_GC_MAXBYTES:
			JS_NewNumberValue(cx,cx->runtime->gcMaxBytes,vp);
			break;
		case PROP_GC_COUNTER:
			JS_NewNumberValue(cx,cx->runtime->gcNumber,vp);
			break;
#endif
	}

	return(JS_TRUE);
}

static JSBool js_set(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint			tiny;
	js_branch_t*	branch;

	if((branch=(js_branch_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

    tiny = JSVAL_TO_INT(id);

	switch(tiny) {
		case PROP_BRANCH_COUNTER:
			JS_ValueToInt32(cx, *vp, (int32*)&branch->counter);
			break;
		case PROP_BRANCH_LIMIT:
			JS_ValueToInt32(cx, *vp, (int32*)&branch->limit);
			break;
		case PROP_GC_INTERVAL:
			JS_ValueToInt32(cx, *vp, (int32*)&branch->gc_freq);
			break;
		case PROP_YIELD_INTERVAL:
			JS_ValueToInt32(cx, *vp, (int32*)&branch->yield_freq);
			break;
	}

	return(TRUE);
}

#define GC_PROP_FLAGS	JSPROP_ENUMERATE|JSPROP_READONLY

static struct JSPropertySpec js_properties[] = {
/*		 name,				tinyid,						flags,				getter,	setter	*/

	{	"branch_counter",	PROP_BRANCH_COUNTER,JSPROP_ENUMERATE,	NULL,	NULL },
	{	"branch_limit",		PROP_BRANCH_LIMIT,	JSPROP_ENUMERATE,	NULL,	NULL },
	{	"yield_interval",	PROP_YIELD_INTERVAL,JSPROP_ENUMERATE,	NULL,	NULL },
	{	"gc_interval",		PROP_GC_INTERVAL,	JSPROP_ENUMERATE,	NULL,	NULL },
#ifdef jscntxt_h___
	{	"gc_bytes",			PROP_GC_BYTES,		GC_PROP_FLAGS,		NULL,	NULL },
	{	"gc_last_bytes",	PROP_GC_LASTBYTES,	GC_PROP_FLAGS,		NULL,	NULL },
	{	"gc_max_bytes",		PROP_GC_MAXBYTES,	GC_PROP_FLAGS,		NULL,	NULL },
	{	"gc_counter",		PROP_GC_COUNTER,	GC_PROP_FLAGS,		NULL,	NULL },
#endif
	{0}
};

#ifdef _DEBUG
static char* prop_desc[] = {
	 "counter incremented for each branch"
	,"maximum number of branches, used for infinite-loop detection (0=disabled)"
	,"interval of periodic garbage collection (lower number=higher frequency, 0=disabled)"
	,"interval of periodic time-slice yields (lower number=higher frequency, 0=disabled)"
#ifdef jscntxt_h___
	,"number of bytes currently in heap"
	,"number of bytes in heap after last garbage collection"
	,"maximum number of bytes in heap"
	,"number of garbage collections"
#endif
	,NULL
};
#endif


static JSClass js_internal_class = {
     "JsInternal"				/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_get					/* getProperty	*/
	,js_set					/* setProperty	*/
	,JS_EnumerateStub		/* enumerate	*/
	,JS_ResolveStub			/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,JS_FinalizeStub		/* finalize		*/
};

JSObject* DLLCALL js_CreateInternalJsObject(JSContext* cx, JSObject* parent, js_branch_t* branch)
{
	JSObject*	obj;

	if((obj = JS_DefineObject(cx, parent, "js", &js_internal_class, NULL
		,JSPROP_ENUMERATE|JSPROP_READONLY))==NULL)
		return(NULL);

	if(!JS_SetPrivate(cx, obj, branch))	/* Store a pointer to js_branch_t */
		return(NULL);

	if(!JS_DefineProperties(cx, obj, js_properties))	/* expose them */
		return(NULL);

#ifdef _DEBUG
	js_DescribeObject(cx,obj,"JavaScript internal control object");
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", prop_desc, JSPROP_READONLY);
#endif

	return(obj);
}
