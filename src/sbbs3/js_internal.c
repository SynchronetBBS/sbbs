/* js_internal.c */

/* Synchronet "js" object, for internal JavaScript branch and GC control */

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

#include "sbbs.h"

#include <jscntxt.h>	/* Needed for Context-private data structure */

enum {
	 PROP_VERSION
	,PROP_TERMINATED
	,PROP_AUTO_TERMINATE
	,PROP_BRANCH_COUNTER
	,PROP_BRANCH_LIMIT
	,PROP_YIELD_INTERVAL
	,PROP_GC_INTERVAL
	,PROP_GC_ATTEMPTS
#ifdef jscntxt_h___
	,PROP_GC_COUNTER
	,PROP_GC_LASTBYTES
	,PROP_BYTES
	,PROP_MAXBYTES
#endif
	,PROP_GLOBAL
};

static JSBool js_get(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint			tiny;
	js_branch_t*	branch;

	if((branch=(js_branch_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

    tiny = JSVAL_TO_INT(id);

	switch(tiny) {
		case PROP_VERSION:
			*vp=STRING_TO_JSVAL(JS_NewStringCopyZ(cx,(char *)JS_GetImplementationVersion()));
			break;
		case PROP_TERMINATED:
			if(branch->terminated==NULL)
				*vp=JSVAL_FALSE;
			else
				*vp=BOOLEAN_TO_JSVAL(*branch->terminated);
			break;
		case PROP_AUTO_TERMINATE:
			*vp=BOOLEAN_TO_JSVAL(branch->auto_terminate);
			break;
		case PROP_BRANCH_COUNTER:
			JS_NewNumberValue(cx,branch->counter,vp);
			break;
		case PROP_BRANCH_LIMIT:
			JS_NewNumberValue(cx,branch->limit,vp);
			break;
		case PROP_YIELD_INTERVAL:
			JS_NewNumberValue(cx,branch->yield_interval,vp);
			break;
		case PROP_GC_INTERVAL:
			JS_NewNumberValue(cx,branch->gc_interval,vp);
			break;
		case PROP_GC_ATTEMPTS:
			JS_NewNumberValue(cx,branch->gc_attempts,vp);
			break;
#ifdef jscntxt_h___
		case PROP_GC_COUNTER:
			JS_NewNumberValue(cx,cx->runtime->gcNumber,vp);
			break;
		case PROP_GC_LASTBYTES:
			JS_NewNumberValue(cx,cx->runtime->gcLastBytes,vp);
			break;
		case PROP_BYTES:
			JS_NewNumberValue(cx,cx->runtime->gcBytes,vp);
			break;
		case PROP_MAXBYTES:
			JS_NewNumberValue(cx,cx->runtime->gcMaxBytes,vp);
			break;
		case PROP_GLOBAL:
			*vp = OBJECT_TO_JSVAL(cx->globalObject);
			break;
#else
		case PROP_GLOBAL:
			*vp = OBJECT_TO_JSVAL(JS_GetParent(cx,obj));
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
		case PROP_TERMINATED:
			if(branch->terminated!=NULL)
				JS_ValueToBoolean(cx, *vp, branch->terminated);
			break;
		case PROP_AUTO_TERMINATE:
			JS_ValueToBoolean(cx,*vp,&branch->auto_terminate);
			break;
		case PROP_BRANCH_COUNTER:
			JS_ValueToInt32(cx, *vp, (int32*)&branch->counter);
			break;
		case PROP_BRANCH_LIMIT:
			JS_ValueToInt32(cx, *vp, (int32*)&branch->limit);
			break;
		case PROP_GC_INTERVAL:
			JS_ValueToInt32(cx, *vp, (int32*)&branch->gc_interval);
			break;
		case PROP_YIELD_INTERVAL:
			JS_ValueToInt32(cx, *vp, (int32*)&branch->yield_interval);
			break;
#ifdef jscntxt_h___
		case PROP_MAXBYTES:
			JS_ValueToInt32(cx, *vp, (int32*)&cx->runtime->gcMaxBytes);
			break;
#endif
	}

	return(JS_TRUE);
}

#define PROP_FLAGS	JSPROP_ENUMERATE|JSPROP_READONLY

static jsSyncPropertySpec js_properties[] = {
/*		 name,				tinyid,						flags,		ver	*/

	{	"version",			PROP_VERSION,		PROP_FLAGS,			311 },
	{	"auto_terminate",	PROP_AUTO_TERMINATE,JSPROP_ENUMERATE,	311 },
	{	"terminated",		PROP_TERMINATED,	JSPROP_ENUMERATE,	311 },
	{	"branch_counter",	PROP_BRANCH_COUNTER,JSPROP_ENUMERATE,	311 },
	{	"branch_limit",		PROP_BRANCH_LIMIT,	JSPROP_ENUMERATE,	311 },
	{	"yield_interval",	PROP_YIELD_INTERVAL,JSPROP_ENUMERATE,	311 },
	{	"gc_interval",		PROP_GC_INTERVAL,	JSPROP_ENUMERATE,	311 },
	{	"gc_attempts",		PROP_GC_ATTEMPTS,	PROP_FLAGS,			311 },
#ifdef jscntxt_h___
	{	"gc_counter",		PROP_GC_COUNTER,	PROP_FLAGS,			311 },
	{	"gc_last_bytes",	PROP_GC_LASTBYTES,	PROP_FLAGS,			311 },
	{	"bytes",			PROP_BYTES,			PROP_FLAGS,			311 },
	{	"max_bytes",		PROP_MAXBYTES,		JSPROP_ENUMERATE,	311 },
#endif
	{	"global",			PROP_GLOBAL,		PROP_FLAGS,			314 },
	{0}
};

#ifdef BUILD_JSDOCS
static char* prop_desc[] = {
	 "JavaScript engine version information (AKA system.js_version)"
	,"set to <i>false</i> to disable the automatic termination of the script upon external request"
	,"termination has been requested (stop execution as soon as possible)"
	,"number of branch operations performed in this runtime"
	,"maximum number of branches, used for infinite-loop detection (0=disabled)"
	,"interval of periodic time-slice yields (lower number=higher frequency, 0=disabled)"
	,"interval of periodic garbage collection attempts (lower number=higher frequency, 0=disabled)"
	,"number of garbage collections attempted in this runtime - <small>READ ONLY</small>"
#ifdef jscntxt_h___
	,"number of garbage collections performed in this runtime - <small>READ ONLY</small>"
	,"number of heap bytes in use after last garbage collection - <small>READ ONLY</small>"
	,"number of heap bytes currently in use - <small>READ ONLY</small>"
	,"maximum number of bytes available for heap"
#endif
	,"global (top level) object - <small>READ ONLY</small>"
	,NULL
};
#endif

JSBool DLLCALL
js_CommonBranchCallback(JSContext *cx, js_branch_t* branch)
{
	branch->counter++;

	/* Terminated? */
	if(branch->auto_terminate &&
		(branch->terminated!=NULL && *branch->terminated)) {
		JS_ReportError(cx,"Terminated");
		branch->counter=0;
		return(JS_FALSE);
	}

	/* Infinite loop? */
	if(branch->limit && branch->counter > branch->limit) {
		JS_ReportError(cx,"Infinite loop (%lu branches) detected",branch->counter);
		branch->counter=0;
		return(JS_FALSE);
	}

	/* Give up timeslices every once in a while */
	if(branch->yield_interval && (branch->counter%branch->yield_interval)==0)
		YIELD();

	/* Periodic Garbage Collection */
	if(branch->gc_interval && (branch->counter%branch->gc_interval)==0)
		JS_MaybeGC(cx), branch->gc_attempts++;

    return(JS_TRUE);
}

/* Execute a string in its own context (away from Synchronet objects) */
static JSBool
js_eval(JSContext *parent_cx, JSObject *parent_obj, uintN argc, jsval *argv, jsval *rval)
{
	char*			buf;
	size_t			buflen;
	JSString*		str;
    JSScript*		script;
	JSContext*		cx;
	JSObject*		obj;
	JSErrorReporter	reporter;
#ifndef EVAL_BRANCH_CALLBACK
	JSBranchCallback callback;
#endif

	if(argc<1)
		return(JS_TRUE);

	if((str=JS_ValueToString(parent_cx, argv[0]))==NULL)
		return(JS_FALSE);
	if((buf=JS_GetStringBytes(str))==NULL)
		return(JS_FALSE);
	buflen=JS_GetStringLength(str);

	if((cx=JS_NewContext(JS_GetRuntime(parent_cx),JAVASCRIPT_CONTEXT_STACK))==NULL)
		return(JS_FALSE);

	/* Use the error reporter from the parent context */
	reporter=JS_SetErrorReporter(parent_cx,NULL);
	JS_SetErrorReporter(parent_cx,reporter);
	JS_SetErrorReporter(cx,reporter);

#ifdef EVAL_BRANCH_CALLBACK
	JS_SetContextPrivate(cx, JS_GetPrivate(parent_cx, parent_obj));
	JS_SetBranchCallback(cx, js_BranchCallback);
#else	/* Use the branch callback from the parent context */
	JS_SetContextPrivate(cx, JS_GetContextPrivate(parent_cx));
	callback=JS_SetBranchCallback(parent_cx,NULL);
	JS_SetBranchCallback(parent_cx, callback);
	JS_SetBranchCallback(cx, callback);
#endif

	if((obj=JS_NewObject(cx, NULL, NULL, NULL))==NULL
		|| !JS_InitStandardClasses(cx,obj)) {
		JS_DestroyContext(cx);
		return(JS_FALSE);
	}

	if((script=JS_CompileScript(cx, obj, buf, buflen, NULL, 0))!=NULL) {
		JS_ExecuteScript(cx, obj, script, rval);
		JS_DestroyScript(cx, script);
	}

	JS_DestroyContext(cx);

    return(JS_TRUE);
}

static JSBool
js_gc(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSBool			forced=JS_TRUE;
	js_branch_t*	branch;

	if((branch=(js_branch_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	if(argc)
		JS_ValueToBoolean(cx,argv[0],&forced);

	if(forced)
		JS_GC(cx);
	else
		JS_MaybeGC(cx);

	branch->gc_attempts++;

	return(JS_TRUE);
}

static JSBool
js_report_error(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JS_ReportError(cx,"%s",JS_GetStringBytes(JS_ValueToString(cx, argv[0])));

	if(argc>1 && argv[1]==JSVAL_TRUE)
		return(JS_FALSE);	/* fatal */

	return(JS_TRUE);
}

static JSBool
js_on_exit(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	js_branch_t*	branch;

	if((branch=(js_branch_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	if(branch->exit_func==NULL)
		branch->exit_func=strListInit();

	strListPush(&branch->exit_func,JS_GetStringBytes(JS_ValueToString(cx, argv[0])));

	return(JS_TRUE);
}

static JSBool
js_get_parent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
	JSObject* child=NULL;
	JSObject* parent;

	if(JS_ValueToObject(cx, argv[0], &child)
		&& child!=NULL
		&& (parent=JS_GetParent(cx,child))!=NULL)
		*rval = OBJECT_TO_JSVAL(parent);

	return(JS_TRUE);
}

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

static jsSyncMethodSpec js_functions[] = {
	{"eval",            js_eval,            0,	JSTYPE_UNDEF,	JSDOCSTR("script")
	,JSDOCSTR("evaluate a JavaScript string in its own (secure) context, returning the result")
	,311
	},		
	{"gc",				js_gc,				0,	JSTYPE_VOID,	JSDOCSTR("forced=<tt>true</tt>")
	,JSDOCSTR("perform a garbage collection operation (freeing memory for unused allocated objects), "
		"if <i>forced</i> is <i>true</i> (the default) a garbage collection is always performed, "
		"otherwise it is only performed if deemed appropriate by the JavaScript engine")
	,311
	},
	{"on_exit",			js_on_exit,			1,	JSTYPE_VOID,	JSDOCSTR("to_eval")
	,JSDOCSTR("add a string to evaluate/execute (LIFO stack) upon script's termination")
	,313
	},
	{"report_error",	js_report_error,	1,	JSTYPE_VOID,	JSDOCSTR("error [,fatal=<tt>false</tt>]")
	,JSDOCSTR("report an error using the standard JavaScript error reporting mechanism "
	"(including script filename and line number), "
	"if <i>fatal</i> is <i>true</i>, immediately terminates script")
	,313
	},
	{"get_parent",		js_get_parent,		1,	JSTYPE_OBJECT,	JSDOCSTR("object")
	,JSDOCSTR("return the parent of the specified object")
	,314
	},
	{0}
};

void DLLCALL js_EvalOnExit(JSContext *cx, JSObject *obj, js_branch_t* branch)
{
	char*	p;
	jsval	rval;
	JSScript* script;

	while((p=strListPop(&branch->exit_func))!=NULL) {
		if((script=JS_CompileScript(cx, obj, p, strlen(p), NULL, 0))!=NULL) {
			JS_ExecuteScript(cx, obj, script, &rval);
			JS_DestroyScript(cx, script);
		}
	}

	strListFree(&branch->exit_func);
}

JSObject* DLLCALL js_CreateInternalJsObject(JSContext* cx, JSObject* parent, js_branch_t* branch)
{
	JSObject*	obj;

	if((obj = JS_DefineObject(cx, parent, "js", &js_internal_class, NULL
		,JSPROP_ENUMERATE|JSPROP_READONLY))==NULL)
		return(NULL);

	if(!JS_SetPrivate(cx, obj, branch))	/* Store a pointer to js_branch_t */
		return(NULL);

	if(!js_DefineSyncProperties(cx, obj, js_properties))	/* expose them */
		return(NULL);

	if(!js_DefineSyncMethods(cx, obj, js_functions, /* append? */ FALSE)) 
		return(NULL);

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx,obj,"JavaScript execution and garbage collection control object",311);
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", prop_desc, JSPROP_READONLY);
#endif

	return(obj);
}
