/* Synchronet "js" object, for internal JavaScript callback and GC control */

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
#include "sockwrap.h"
#include "js_request.h"
#include "js_socket.h"

/* SpiderMonkey: */
#include <jsdbgapi.h>

enum {
	 PROP_VERSION
	,PROP_TERMINATED
	,PROP_AUTO_TERMINATE
	,PROP_COUNTER
	,PROP_TIME_LIMIT
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
	,PROP_OPTIONS
	,PROP_KEEPGOING
};

static JSBool js_get(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	jsval idval;
    jsint			tiny;
	js_callback_t*	cb;
	js_callback_t*	top_cb;

	if((cb=(js_callback_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	switch(tiny) {
		case PROP_VERSION:
			*vp=STRING_TO_JSVAL(JS_NewStringCopyZ(cx,(char *)JS_GetImplementationVersion()));
			break;
		case PROP_TERMINATED:
			for(top_cb=cb; top_cb->bg && top_cb->parent_cb; top_cb=top_cb->parent_cb) {
				if(top_cb->terminated && *top_cb->terminated) {
					*vp=BOOLEAN_TO_JSVAL(TRUE);
					return JS_TRUE;
				}
			}
			if(top_cb->terminated==NULL)
				*vp=JSVAL_FALSE;
			else
				*vp=BOOLEAN_TO_JSVAL(*top_cb->terminated);
			break;
		case PROP_AUTO_TERMINATE:
			*vp=BOOLEAN_TO_JSVAL(cb->auto_terminate);
			break;
		case PROP_COUNTER:
			*vp=DOUBLE_TO_JSVAL((double)cb->counter);
			break;
		case PROP_TIME_LIMIT:
			*vp=DOUBLE_TO_JSVAL(cb->limit);
			break;
		case PROP_YIELD_INTERVAL:
			*vp=DOUBLE_TO_JSVAL((double)cb->yield_interval);
			break;
		case PROP_GC_INTERVAL:
			*vp=DOUBLE_TO_JSVAL((double)cb->gc_interval);
			break;
		case PROP_GC_ATTEMPTS:
			*vp=DOUBLE_TO_JSVAL((double)cb->gc_attempts);
			break;
#ifdef jscntxt_h___
		case PROP_GC_COUNTER:
			*vp=UINT_TO_JSVAL(cx->runtime->gcNumber);
			break;
		case PROP_GC_LASTBYTES:
			*vp=DOUBLE_TO_JSVAL((double)cx->runtime->gcLastBytes);
			break;
		case PROP_BYTES:
			*vp=DOUBLE_TO_JSVAL((double)cx->runtime->gcBytes);
			break;
		case PROP_MAXBYTES:
			*vp=DOUBLE_TO_JSVAL((double)cx->runtime->gcMaxBytes);
			break;
#endif
		case PROP_GLOBAL:
			*vp = OBJECT_TO_JSVAL(JS_GetGlobalObject(cx));	
			break;
		case PROP_OPTIONS:
			*vp = UINT_TO_JSVAL(JS_GetOptions(cx));
			break;
		case PROP_KEEPGOING:
			if (cb->events_supported)
				*vp = BOOLEAN_TO_JSVAL(cb->keepGoing);
			else
				*vp = JSVAL_FALSE;
			break;
	}

	return(JS_TRUE);
}

static JSBool js_set(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
	jsval idval;
    jsint			tiny;
	js_callback_t*	cb;

	if((cb=(js_callback_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

    JS_IdToValue(cx, id, &idval);
    tiny = JSVAL_TO_INT(idval);

	switch(tiny) {
		case PROP_TERMINATED:
			if(cb->terminated!=NULL)
				JS_ValueToBoolean(cx, *vp, (int *)cb->terminated);
			break;
		case PROP_AUTO_TERMINATE:
			JS_ValueToBoolean(cx,*vp,&cb->auto_terminate);
			break;
		case PROP_COUNTER:
			if(!JS_ValueToInt32(cx, *vp, (int32*)&cb->counter))
				return JS_FALSE;
			break;
		case PROP_TIME_LIMIT:
			if(!JS_ValueToInt32(cx, *vp, (int32*)&cb->limit))
				return JS_FALSE;
			break;
		case PROP_GC_INTERVAL:
			if(!JS_ValueToInt32(cx, *vp, (int32*)&cb->gc_interval))
				return JS_FALSE;
			break;
		case PROP_YIELD_INTERVAL:
			if(!JS_ValueToInt32(cx, *vp, (int32*)&cb->yield_interval))
				return JS_FALSE;
			break;
#ifdef jscntxt_h___
		case PROP_MAXBYTES:
			if(!JS_ValueToInt32(cx, *vp, (int32*)&cx->runtime->gcMaxBytes))
				return JS_FALSE;
			break;
#endif
		case PROP_KEEPGOING:
			if (cb->events_supported)
				JS_ValueToBoolean(cx, *vp, &cb->keepGoing);
			break;
	}

	return(JS_TRUE);
}

#define PROP_FLAGS	JSPROP_ENUMERATE|JSPROP_READONLY

static jsSyncPropertySpec js_properties[] = {
/*		 name,				tinyid,						flags,		ver	*/

	{	"version",			PROP_VERSION,		PROP_FLAGS,			311 },
	{	"auto_terminate",	PROP_AUTO_TERMINATE,JSPROP_ENUMERATE,	311 },
	{	"terminated",		PROP_TERMINATED,	JSPROP_ENUMERATE,	311 },
	{	"branch_counter",	PROP_COUNTER,		0,					311 },
	{	"counter",			PROP_COUNTER,		JSPROP_ENUMERATE,	316 },
	{	"branch_limit",		PROP_TIME_LIMIT,	0,					311 },
	{	"time_limit",		PROP_TIME_LIMIT,	JSPROP_ENUMERATE,	316 },
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
	{	"options",			PROP_OPTIONS,		PROP_FLAGS,			31802 },
	{	"do_callbacks",		PROP_KEEPGOING,		JSPROP_ENUMERATE,			31900 },
	{0}
};

#ifdef BUILD_JSDOCS
static char* prop_desc[] = {
	 "JavaScript engine version information (AKA system.js_version)"
	,"set to <i>false</i> to disable the automatic termination of the script upon external request"
	,"termination has been requested (stop execution as soon as possible)"
	,"number of operation callbacks performed in this runtime"
	,"maximum number of operation callbacks, used for infinite-loop detection (0=disabled)"
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
	,"option flags - <small>READ ONLY</small>"
	,"do callbacks after script finishes running"
	/* New properties go here... */
	,"full path and filename of JS file executed"
	,"directory of executed JS file"
	,"JS filename executed (with no path)"
	,"Either the configured startup directory in SCFG (for externals) or the cwd when jsexec is started"
	,"global scope for this script"
	,"load() search path array.<br>For relative load paths (e.g. not beginning with '/' or '\\'), "
		"the path is assumed to be a sub-directory of the (configurable) mods or exec directories "
		"and is searched accordingly. "
		"So, by default, load(\"somefile.js\") will search in this order:<br>"
		"mods/load/somefile.js<br>"
		"exec/load/somefile.js<br>"
		"mods/somefile.js<br>"
		"exec/somefile.js<br>"
	,NULL
};
#endif

JSBool
js_CommonOperationCallback(JSContext *cx, js_callback_t* cb)
{
	js_callback_t *top_cb;

	cb->counter++;

	/* Terminated? */
	if(cb->auto_terminate) {
		for(top_cb=cb; top_cb; top_cb=top_cb->parent_cb) {
			if (top_cb->terminated!=NULL && *top_cb->terminated) {
				JS_ReportWarning(cx,"Terminated");
				cb->counter=0;
				return(JS_FALSE);
			}
		}
	}

	/* Infinite loop? */
	if(cb->limit && cb->counter > cb->limit) {
		JS_ReportError(cx,"Infinite loop (%lu operation callbacks) detected",cb->counter);
		cb->counter=0;
		return(JS_FALSE);
	}

	/* Give up timeslices every once in a while */
	if(cb->yield_interval && (cb->counter%cb->yield_interval)==0) {
		jsrefcount	rc;

		rc=JS_SUSPENDREQUEST(cx);
		YIELD();
		JS_RESUMEREQUEST(cx, rc);
	}

	/* Permit other contexts to run GC */
	JS_YieldRequest(cx);

	/* Periodic Garbage Collection */
	if(cb->gc_interval && (cb->counter%cb->gc_interval)==0)
		JS_MaybeGC(cx), cb->gc_attempts++;

    return(JS_TRUE);
}

// This is kind of halfway between js_execfile() in exec.cpp and js_load
static int
js_execfile(JSContext *cx, uintN argc, jsval *arglist)
{
	char*		cmd = NULL;
	size_t		cmdlen;
	size_t		pathlen;
	char*		startup_dir = NULL;
	uintN		arg=0;
	char		path[MAX_PATH+1] = "";
	JSObject*	scope = JS_GetScopeChain(cx);
	JSObject*	js_scope = NULL;
	JSObject*	pscope;
	JSObject*	js_script=NULL;
	JSObject*	nargv;
	jsval		rval = JSVAL_VOID;
	jsrefcount	rc;
	uintN		i;
	jsval		val;
	JSObject *js_obj;
	JSObject *pjs_obj;
	js_callback_t *	js_callback;

	if(argc<1) {
		JS_ReportError(cx, "No filename passed");
		return(JS_FALSE);
	}

	jsval *argv=JS_ARGV(cx, arglist);

	if (!JSVAL_IS_STRING(argv[arg])) {
		JS_ReportError(cx, "Invalid script name");
		return(JS_FALSE);
	}
	JSVALUE_TO_MSTRING(cx, argv[arg++], cmd, NULL);
	HANDLE_PENDING(cx, cmd);
	if (cmd == NULL) {
		JS_ReportError(cx, "Invalid NULL string");
		return(JS_FALSE);
	}

	if (argc > arg) {
		if (JSVAL_IS_STRING(argv[arg])) {
			JSVALUE_TO_MSTRING(cx, argv[arg++], startup_dir, &cmdlen);
			HANDLE_PENDING(cx, cmd);
			if(startup_dir == NULL) {
				free(cmd);
				JS_ReportError(cx, "Invalid NULL string");
				return(JS_FALSE);
			}
		}
	}

	if (argc > arg && JSVAL_IS_OBJECT(argv[arg])) {
		js_scope = JSVAL_TO_OBJECT(argv[arg++]);
		if (js_scope == scope) {
			free(cmd);
			free(startup_dir);
			JS_ReportError(cx, "Invalid Scope");
			return(JS_FALSE);
		}
	}
	else {
		free(cmd);
		free(startup_dir);
		JS_ReportError(cx, "Invalid Scope");
		return(JS_FALSE);
	}

	pscope = scope;
	while ((!JS_GetProperty(cx, pscope, "js", &val) || val==JSVAL_VOID || !JSVAL_IS_OBJECT(val)) && pscope != NULL) {
		pscope = JS_GetParent(cx, pscope);
		if (pscope == NULL) {
			free(cmd);
			free(startup_dir);
			JS_ReportError(cx, "Walked to global, no js object!");
			return JS_FALSE;
		}
	}
	pjs_obj = JSVAL_TO_OBJECT(val);
	js_callback = JS_GetPrivate(cx, pjs_obj);

	if(isfullpath(cmd))
		SAFECOPY(path,cmd);
	else {
		// If startup dir specified, check there first.
		if (startup_dir) {
			SAFECOPY(path, startup_dir);
			backslash(path);
			strncat(path, cmd, sizeof(path) - strlen(path) - 1);
			rc=JS_SUSPENDREQUEST(cx);
			if (!fexist(path))
				*path = 0;
			JS_RESUMEREQUEST(cx, rc);
		}
		// Then check js.exec_dir
		/* if js.exec_dir is defined (location of executed script), search there first */
		if (*path == 0) {
			if(JS_GetProperty(cx, pjs_obj, "exec_dir", &val) && val!=JSVAL_VOID && JSVAL_IS_STRING(val)) {
				JSVALUE_TO_STRBUF(cx, val, path, sizeof(path), &pathlen);
				strncat(path, cmd, sizeof(path)-pathlen-1);
				rc=JS_SUSPENDREQUEST(cx);
				if(!fexistcase(path))
					path[0]=0;
				JS_RESUMEREQUEST(cx, rc);
			}
		}
	}
	free(cmd);

	if(!fexistcase(path)) {
		JS_ReportError(cx, "Script file (%s) does not exist", path);
		free(startup_dir);
		return JS_FALSE;
	}

	nargv=JS_NewArrayObject(cx, 0, NULL);

	JS_DefineProperty(cx, js_scope, "argv", OBJECT_TO_JSVAL(nargv)
		,NULL,NULL,JSPROP_READONLY|JSPROP_ENUMERATE);

	uintN nargc = 0;
	for(i=arg; i<argc; i++) {
		JS_SetElement(cx, nargv, nargc, &argv[i]);
		nargc++;
	}

	JS_DefineProperty(cx, js_scope, "argc", INT_TO_JSVAL(nargc)
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);

	js_obj = js_CreateInternalJsObject(cx, js_scope, js_callback, NULL);
	js_PrepareToExecute(cx, js_scope, path, startup_dir, js_scope);
	free(startup_dir);
	// Copy in the load_path_list...
	if(pjs_obj != NULL) {
		JSObject*	pload_path_list;
		JSObject*	load_path_list;
		uint32_t	plen;
		uint32_t	pcnt;

		if (JS_GetProperty(cx, pjs_obj, JAVASCRIPT_LOAD_PATH_LIST, &val) && val!=JSVAL_VOID && JSVAL_IS_OBJECT(val)) {
			pload_path_list = JSVAL_TO_OBJECT(val);
			if (!JS_IsArrayObject(cx, pload_path_list)) {
				JS_ReportError(cx, "Weird js."JAVASCRIPT_LOAD_PATH_LIST" value");
				return JS_FALSE;
			}
			if((load_path_list=JS_NewArrayObject(cx, 0, NULL))==NULL) {
				JS_ReportError(cx, "Unable to create js."JAVASCRIPT_LOAD_PATH_LIST);
				return JS_FALSE;
			}
			val = OBJECT_TO_JSVAL(load_path_list);
			JS_SetProperty(cx, js_obj, JAVASCRIPT_LOAD_PATH_LIST, &val);
			if(JS_GetArrayLength(cx, pload_path_list, &plen))
				for (pcnt = 0; pcnt < plen; pcnt++) {
					if(JS_GetElement(cx, pload_path_list, pcnt, &val))
						JS_SetElement(cx, load_path_list, pcnt, &val);
				}
		}
		else {
			JS_ReportError(cx, "Unable to get parent js."JAVASCRIPT_LOAD_PATH_LIST" array.");
			return JS_FALSE;
		}
	}
	else {
		JS_ReportError(cx, "Unable to get parent js object");
		return JS_FALSE;
	}

	js_script=JS_CompileFile(cx, js_scope, path);

	if(js_script == NULL) {
		/* If the script fails to compile, it's not a fatal error
		 * for the caller. */
		if (JS_IsExceptionPending(cx)) {
			JS_GetPendingException(cx, &rval);
			JS_SET_RVAL(cx, arglist, rval);
		}
		JS_ClearPendingException(cx);
		return(JS_TRUE);
	}

	JS_ExecuteScript(cx, js_scope, js_script, &rval);
	if (JS_IsExceptionPending(cx)) {
		JS_GetPendingException(cx, &rval);
	}
	else {
		jsval exit_code = JSVAL_VOID;
		if(JS_GetProperty(cx, js_scope, "exit_code", &exit_code) && exit_code != JSVAL_VOID)
			rval = exit_code;
	}
	JS_SET_RVAL(cx, arglist, rval);
	JS_ClearPendingException(cx);

	js_EvalOnExit(cx, js_scope, js_callback);
	JS_ReportPendingException(cx);
	JS_DestroyScript(cx, js_script);
	JS_GC(cx);

	return JS_TRUE;
}

static JSClass eval_class = {
    "Global",  /* name */
    JSCLASS_GLOBAL_FLAGS,  /* flags */
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

/* Execute a string in its own context (away from Synchronet objects) */
static JSBool
js_eval(JSContext *parent_cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(parent_cx, arglist);
	char*			buf = NULL;
	size_t			buflen;
	JSString*		str;
    JSObject*		script;
	JSContext*		cx;
	JSObject*		obj;
	JSErrorReporter	reporter;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if(argc<1)
		return(JS_TRUE);

	if((str=JS_ValueToString(parent_cx, argv[0]))==NULL)
		return(JS_FALSE);
	JSSTRING_TO_MSTRING(parent_cx, str, buf, &buflen);
	HANDLE_PENDING(parent_cx, buf);
	if(buf==NULL)
		return(JS_TRUE);

	if((cx=JS_NewContext(JS_GetRuntime(parent_cx),JAVASCRIPT_CONTEXT_STACK))==NULL) {
		free(buf);
		return(JS_FALSE);
	}

	/* Use the error reporter from the parent context */
	reporter=JS_SetErrorReporter(parent_cx,NULL);
	JS_SetErrorReporter(parent_cx,reporter);
	JS_SetErrorReporter(cx,reporter);

	/* Use the operation callback from the parent context */
	JS_SetContextPrivate(cx, JS_GetContextPrivate(parent_cx));
	JS_SetOperationCallback(cx, JS_GetOperationCallback(parent_cx));

	if((obj=JS_NewCompartmentAndGlobalObject(cx, &eval_class, NULL))==NULL
		|| !JS_InitStandardClasses(cx,obj)) {
		JS_DestroyContext(cx);
		free(buf);
		return(JS_FALSE);
	}

	if((script=JS_CompileScript(cx, obj, buf, buflen, NULL, 0))!=NULL) {
		jsval	rval;

		JS_ExecuteScript(cx, obj, script, &rval);
		JS_SET_RVAL(cx, arglist, rval);
	}
	free(buf);

	JS_DestroyContext(cx);

    return(JS_TRUE);
}

static JSBool
js_gc(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	jsval *argv=JS_ARGV(cx, arglist);
	JSBool			forced=JS_TRUE;
	js_callback_t*	cb;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((cb=(js_callback_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	if(argc)
		JS_ValueToBoolean(cx,argv[0],&forced);

	if(forced)
		JS_GC(cx);
	else
		JS_MaybeGC(cx);

	cb->gc_attempts++;

	return(JS_TRUE);
}

static JSBool
js_report_error(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	char	*p = NULL;

	JSVALUE_TO_MSTRING(cx, argv[0], p, NULL);
	HANDLE_PENDING(cx, p);
	if(p==NULL)
		JS_ReportError(cx,"NULL");
	else {
		JS_ReportError(cx,"%s",p);
		free(p);
	}

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if(argc>1 && argv[1]==JSVAL_TRUE)
		return(JS_FALSE);	/* fatal */

	return(JS_TRUE);
}

static JSBool
js_on_exit(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *scope=JS_GetScopeChain(cx);
	JSObject *glob=JS_GetGlobalObject(cx);
	jsval *argv=JS_ARGV(cx, arglist);
	global_private_t*	pd;
	struct js_onexit_scope *oes = NULL;
	str_list_t	oldlist;
	str_list_t	list;
	char		*p = NULL;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if((pd=(global_private_t*)JS_GetPrivate(cx,glob))==NULL)
		return(JS_FALSE);
	if(glob==scope) {
		if(pd->exit_func==NULL)
			pd->exit_func=strListInit();
		list=pd->exit_func;
	}
	else {
		/* First, look for an existing onexit scope for this scope */
		for (oes = pd->onexit; oes; oes = oes->next) {
			if (oes->scope == scope)
				break;
		}

		/* If one isn't found, insert it */
		if (oes == NULL) {
			oes = malloc(sizeof(*oes));
			if (oes == NULL) {
				JS_ReportError(cx, "Unable to allocate memory for onexit scope");
				return JS_FALSE;
			}
			oes->next = pd->onexit;
			pd->onexit = oes;
			oes->scope = scope;
			oes->onexit = strListInit();
			JS_AddObjectRoot(cx, &oes->scope);
		}
		list = oes->onexit;
	}

	JSVALUE_TO_MSTRING(cx, argv[0], p, NULL);
	HANDLE_PENDING(cx, p);
	if(!p)
		return JS_TRUE;
	oldlist=list;
	strListPush(&list,p);
	free(p);
	if(oldlist != list) {
		if(glob==scope)
			pd->exit_func=list;
		else
			oes->onexit = list;
	}
	return(JS_TRUE);
}

static JSBool
js_get_parent(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	JSObject* child=NULL;
	JSObject* parent;

	if(JS_ValueToObject(cx, argv[0], &child)
		&& child!=NULL
		&& (parent=JS_GetParent(cx,child))!=NULL)
		JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(parent));

	return(JS_TRUE);
}

static JSBool js_getsize(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval	*argv=JS_ARGV(cx, arglist);
	JSObject* tmp_obj;

	if(!JSVAL_IS_OBJECT(argv[0])) {
		JS_ReportError(cx, "get_size() error!  Parameter is not an object.");
		return(JS_FALSE);
	}
	tmp_obj=JSVAL_TO_OBJECT(argv[0]);
	if(!tmp_obj)
		return(JS_FALSE);
	JS_SET_RVAL(cx, arglist, DOUBLE_TO_JSVAL(JS_GetObjectTotalSize(cx, tmp_obj)));
	return(JS_TRUE);
}

static JSBool js_flatten(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval	*argv=JS_ARGV(cx, arglist);

	if(!JSVAL_IS_STRING(argv[0])) {
		JS_ReportError(cx, "get_size() error!  Parameter is not a string.");
		return(JS_FALSE);
	}
	JS_FlattenString(cx, JSVAL_TO_STRING(argv[0]));
	JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	return(JS_TRUE);
}

static JSBool
js_setTimeout(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval	*argv=JS_ARGV(cx, arglist);
	struct js_event_list *ev;
	js_callback_t*	cb;
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	JSFunction *ecb;
	uint64_t now = (uint64_t)(xp_timer() * 1000);
	jsdouble timeout;

	if((cb=(js_callback_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	if (!cb->events_supported) {
		JS_ReportError(cx, "events not supported");
		return JS_FALSE;
	}

	if (argc < 2) {
		JS_ReportError(cx, "js.setTimeout() requires two parameters");
		return JS_FALSE;
	}
	ecb = JS_ValueToFunction(cx, argv[0]);
	if (ecb == NULL) {
		return JS_FALSE;
	}
	if (argc > 2) {
		if (!JS_ValueToObject(cx, argv[2], &obj))
			return JS_FALSE;
	}
	if (!JS_ValueToNumber(cx, argv[1], &timeout)) {
		return JS_FALSE;
	}
	ev = malloc(sizeof(*ev));
	if (ev == NULL) {
		JS_ReportError(cx, "error allocating %lu bytes", sizeof(*ev));
		return JS_FALSE;
	}
	ev->prev = NULL;
	ev->next = cb->events;
	if (ev->next)
		ev->next->prev = ev;
	ev->type = JS_EVENT_TIMEOUT;
	ev->cx = obj;
	JS_AddObjectRoot(cx, &ev->cx);
	ev->cb = ecb;
	ev->data.timeout.end = (uint64_t)(now + timeout);
	ev->id = cb->next_eid++;
	cb->events = ev;

	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(ev->id));
	return JS_TRUE;
}

JSBool
js_clear_event(JSContext *cx, jsval *arglist, js_callback_t *cb, enum js_event_type et, int ididx)
{
	int32 id;
	jsval	*argv=JS_ARGV(cx, arglist);
	struct js_event_list *ev;
	struct js_event_list *nev;

	if (!JS_ValueToInt32(cx, argv[ididx], &id)) {
		return JS_FALSE;
	}
	if (!cb->events_supported) {
		JS_ReportError(cx, "events not supported");
		return JS_FALSE;
	}

	for (ev = cb->events; ev; ev = nev) {
		nev = ev->next;
		if (ev->type == et && ev->id == id) {
			if (ev->next)
				ev->next->prev = ev->prev;
			if (ev->prev)
				ev->prev->next = ev->next;
			else
				cb->events = ev->next;
			JS_RemoveObjectRoot(cx, &ev->cx);
			free(ev);
		}
	}

	return JS_TRUE;
}

static JSBool
js_internal_clear_event(JSContext *cx, uintN argc, jsval *arglist, enum js_event_type et)
{
	js_callback_t*	cb;
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if (argc < 1) {
		JS_ReportError(cx, "js.clearTimeout() requires an id");
		return JS_FALSE;
	}
	if((cb=(js_callback_t*)JS_GetPrivate(cx, obj))==NULL)
		return(JS_FALSE);

	return js_clear_event(cx, arglist, cb, et, 0);
}

static JSBool
js_clearTimeout(JSContext *cx, uintN argc, jsval *arglist)
{
	return js_internal_clear_event(cx, argc, arglist, JS_EVENT_TIMEOUT);
}

static JSBool
js_clearInterval(JSContext *cx, uintN argc, jsval *arglist)
{
	return js_internal_clear_event(cx, argc, arglist, JS_EVENT_INTERVAL);
}

static JSBool
js_setInterval(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval	*argv=JS_ARGV(cx, arglist);
	struct js_event_list *ev;
	js_callback_t*	cb;
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	JSFunction *ecb;
	uint64_t now = (uint64_t)(xp_timer() * 1000);
	jsdouble period;

	if((cb=(js_callback_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	if (!cb->events_supported) {
		JS_ReportError(cx, "events not supported");
		return JS_FALSE;
	}

	if (argc < 2) {
		JS_ReportError(cx, "js.setInterval() requires two parameters");
		return JS_FALSE;
	}
	ecb = JS_ValueToFunction(cx, argv[0]);
	if (ecb == NULL) {
		return JS_FALSE;
	}
	if (!JS_ValueToNumber(cx, argv[1], &period)) {
		return JS_FALSE;
	}
	if (argc > 2) {
		if (!JS_ValueToObject(cx, argv[2], &obj))
			return JS_FALSE;
	}
	ev = malloc(sizeof(*ev));
	if (ev == NULL) {
		JS_ReportError(cx, "error allocating %lu bytes", sizeof(*ev));
		return JS_FALSE;
	}
	ev->prev = NULL;
	ev->next = cb->events;
	if (ev->next)
		ev->next->prev = ev;
	ev->type = JS_EVENT_INTERVAL;
	ev->cx = obj;
	JS_AddObjectRoot(cx, &ev->cx);
	ev->cb = ecb;
	ev->data.interval.last = now;
	ev->data.interval.period =(uint64_t)period;
	ev->id = cb->next_eid++;
	cb->events = ev;

	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(ev->id));
	return JS_TRUE;
}

static JSBool
js_setImmediate(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval	*argv=JS_ARGV(cx, arglist);
	JSFunction *cbf;
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	js_callback_t *cb;
	struct js_runq_entry *rqe;

	if((cb=(js_callback_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	if (argc < 1) {
		JS_ReportError(cx, "js.setImmediate() requires a callback");
		return JS_FALSE;
	}

	cbf = JS_ValueToFunction(cx, argv[0]);
	if (cbf == NULL) {
		return JS_FALSE;
	}

	if (argc > 1) {
		if (!JS_ValueToObject(cx, argv[1], &obj))
			return JS_FALSE;
	}

	rqe = malloc(sizeof(*rqe));
	if (rqe == NULL) {
		JS_ReportError(cx, "error allocating %ul bytes", sizeof(*rqe));
		return JS_FALSE;
	}
	rqe->func = cbf;
	rqe->cx = obj;
	JS_AddObjectRoot(cx, &rqe->cx);
	rqe->next = NULL;
	if (cb->rq_tail != NULL)
		cb->rq_tail->next = rqe;
	cb->rq_tail = rqe;
	if (cb->rq_head == NULL)
		cb->rq_head = rqe;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	return JS_TRUE;
}

static JSBool
js_addEventListener(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval	*argv=JS_ARGV(cx, arglist);
	char *name;
	JSFunction *cbf;
	struct js_listener_entry *listener;
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	js_callback_t *cb;

	if((cb=(js_callback_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	if (argc < 2) {
		JS_ReportError(cx, "js.addEventListener() requires exactly two parameters");
		return JS_FALSE;
	}

	cbf = JS_ValueToFunction(cx, argv[1]);
	if (cbf == NULL) {
		return JS_FALSE;
	}

	JSVALUE_TO_MSTRING(cx, argv[0], name, NULL);
	HANDLE_PENDING(cx, name);

	listener = malloc(sizeof(*listener));
	if (listener == NULL) {
		free(name);
		JS_ReportError(cx, "error allocating %ul bytes", sizeof(*listener));
		return JS_FALSE;
	}

	listener->func = cbf;
	listener->id = cb->next_eid++;
	listener->next = cb->listeners;
	listener->name = name;
	cb->listeners = listener;

	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(listener->id));
	return JS_TRUE;
}

static JSBool
js_removeEventListener(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	int32 id;
	struct js_listener_entry *listener;
	struct js_listener_entry **pnext;
	char *name;
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	js_callback_t *cb;

	if((cb=(js_callback_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	if (argc < 1) {
		JS_ReportError(cx, "js.removeEventListener() requires exactly one parameter");
		return JS_FALSE;
	}

	if (JSVAL_IS_INT(argv[0])) {
		// Remove by ID
		if (!JS_ValueToInt32(cx, argv[0], &id))
			return JS_FALSE;
		for (listener = cb->listeners, pnext = &cb->listeners; listener; listener = (*pnext)) {
			if (listener->id == id) {
				(*pnext) = listener->next;
				free(listener);
			}
			else
				pnext = &listener->next;
		}
	}
	else {
		// Remove by name
		JSVALUE_TO_MSTRING(cx, argv[0], name, NULL);
		HANDLE_PENDING(cx, name);
		for (listener = cb->listeners, pnext = &cb->listeners; listener; listener = (*pnext)) {
			if (strcmp(listener->name, name) == 0) {
				(*pnext) = listener->next;
				free(listener);
			}
			else
				pnext = &listener->next;
		}
		free(name);
	}

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	return JS_TRUE;
}

static JSBool
js_dispatchEvent(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv=JS_ARGV(cx, arglist);
	struct js_listener_entry *listener;
	struct js_runq_entry *rqe;
	JSObject *obj=JS_THIS_OBJECT(cx, arglist);
	char *name;
	js_callback_t *cb;

	if((cb=(js_callback_t*)JS_GetPrivate(cx,obj))==NULL)
		return(JS_FALSE);

	if (argc < 1) {
		JS_ReportError(cx, "js.dispatchEvent() requires a event name");
		return JS_FALSE;
	}

	if (argc > 1) {
		if (!JSVAL_IS_OBJECT(argv[1])) {
			JS_ReportError(cx, "second argument must be an object");
			return JS_FALSE;
		}
		obj = JSVAL_TO_OBJECT(argv[1]);
	}

	JSVALUE_TO_MSTRING(cx, argv[0], name, NULL);
	HANDLE_PENDING(cx, name);

	for (listener = cb->listeners; listener; listener = listener->next) {
		if (strcmp(name, listener->name) == 0) {
			rqe = malloc(sizeof(*rqe));
			if (rqe == NULL) {
				JS_ReportError(cx, "error allocating %ul bytes", sizeof(*rqe));
				free(name);
				return JS_FALSE;
			}
			rqe->func = listener->func;
			rqe->cx = obj;
			JS_AddObjectRoot(cx, &rqe->cx);
			rqe->next = NULL;
			if (cb->rq_tail != NULL)
				cb->rq_tail->next = rqe;
			cb->rq_tail = rqe;
			if (cb->rq_head == NULL)
				cb->rq_head = rqe;
		}
	}
	free(name);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	return JS_TRUE;
}

static void
js_clear_events(JSContext *cx, js_callback_t *cb)
{
	struct js_event_list *ev;
	struct js_event_list *nev;
	struct js_runq_entry *rq;
	struct js_runq_entry *nrq;
	struct js_listener_entry *le;
	struct js_listener_entry *nle;

	for (ev = cb->events, nev = NULL; ev; ev = nev) {
		nev = ev->next;
		JS_RemoveObjectRoot(cx, &ev->cx);
		free(ev);
	}
	cb->next_eid = 0;
	cb->events = NULL;

	for (rq = cb->rq_head; rq; rq = nrq) {
		nrq = rq->next;
		JS_RemoveObjectRoot(cx, &rq->cx);
		free(rq);
	}
	cb->rq_head = NULL;
	cb->rq_tail = NULL;

	for (le = cb->listeners; le; le = nle) {
		nle = le->next;
		free(le);
	}
	cb->listeners = NULL;
}

JSBool
js_handle_events(JSContext *cx, js_callback_t *cb, volatile int *terminated)
{
	struct js_event_list **head = &cb->events;
	int timeout;
	int i;
	uint64_t now;
	struct js_event_list *ev;
	struct js_event_list *tev;
	struct js_event_list *cev;
	jsval rval = JSVAL_NULL;
	jsrefcount rc;
	JSBool ret = JS_TRUE;
	BOOL input_locked = FALSE;
	js_socket_private_t*	jssp;
	socklen_t slen;
#ifdef PREFER_POLL
	struct pollfd *fds;
	nfds_t sc;
	nfds_t cfd;
#else
	fd_set rfds;
	fd_set wfds;
	struct timeval tv;
	SOCKET hsock;
#endif

	if (!cb->events_supported)
		return JS_FALSE;

	while (cb->keepGoing && !JS_IsExceptionPending(cx) && (cb->events || cb->rq_head) && !*terminated) {
		timeout = -1;	// Infinity by default...
		now = (uint64_t)(xp_timer() * 1000);
		ev = NULL;
		tev = NULL;
		cev = NULL;
#ifdef PREFER_POLL
		fds = NULL;
		sc = 0;
		cfd = 0;
#else
		hsock = 0;
#endif

#ifdef PREFER_POLL
		for (ev = *head; ev; ev = ev->next) {
			if (ev->type == JS_EVENT_SOCKET_READABLE || ev->type == JS_EVENT_SOCKET_READABLE_ONCE
			    || ev->type == JS_EVENT_SOCKET_WRITABLE || ev->type == JS_EVENT_SOCKET_WRITABLE_ONCE
			    || ev->type == JS_EVENT_SOCKET_CONNECT || ev->type == JS_EVENT_CONSOLE_INPUT
			    || ev->type == JS_EVENT_CONSOLE_INPUT_ONCE)
				sc++;
		}

		if (sc) {
			fds = calloc(sc, sizeof(*fds));
			if (fds == NULL) {
				JS_ReportError(cx, "error allocating %d elements of %ul bytes", sc, sizeof(*fds));
				return JS_FALSE;
			}
		}
#else
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
#endif

		rc = JS_SUSPENDREQUEST(cx);
		if (cb->rq_head)
			timeout = 0;
		for (ev = *head; ev; ev = ev->next) {
			switch (ev->type) {
				case JS_EVENT_SOCKET_READABLE_ONCE:
				case JS_EVENT_SOCKET_READABLE:
#ifdef PREFER_POLL
					fds[cfd].fd = ev->data.sock;
					fds[cfd].events = POLLIN;
					cfd++;
#else
					FD_SET(ev->data.sock, &rfds);
					if (ev->data.sock > hsock)
						hsock = ev->data.sock;
#endif
					break;
				case JS_EVENT_SOCKET_WRITABLE_ONCE:
				case JS_EVENT_SOCKET_WRITABLE:
#ifdef PREFER_POLL
					fds[cfd].fd = ev->data.sock;
					fds[cfd].events = POLLOUT;
					cfd++;
#else
					FD_SET(ev->data.sock, &wfds);
					if (ev->data.sock > hsock)
						hsock = ev->data.sock;
#endif
					break;
				case JS_EVENT_SOCKET_CONNECT:
#ifdef PREFER_POLL
					fds[cfd].fd = ev->data.connect.sv[0];
					fds[cfd].events = POLLIN;
					cfd++;
#else
					FD_SET(ev->data.connect.sv[0], &rfds);
					if (ev->data.connect.sv[0] > hsock)
						hsock = ev->data.connect.sv[0];
#endif
					break;
				case JS_EVENT_INTERVAL:
					// TODO: Handle clock wrapping
					if (ev->data.interval.last + ev->data.interval.period <= now) {
						timeout = 0;
						tev = ev;
					}
					else {
						i = (int)(ev->data.interval.last + ev->data.interval.period - now);
						if (timeout == -1 || i < timeout) {
							timeout = i;
							tev = ev;
						}
					}
					break;
				case JS_EVENT_TIMEOUT:
					// TODO: Handle clock wrapping
					if (now >= ev->data.timeout.end) {
						timeout = 0;
						tev = ev;
					}
					else {
						i = (int)(ev->data.timeout.end - now);
						if (timeout == -1 || i < timeout) {
							timeout = i;
							tev = ev;
						}
					}
					break;
				case JS_EVENT_CONSOLE_INPUT_ONCE:
				case JS_EVENT_CONSOLE_INPUT:
					if (!input_locked)
						js_do_lock_input(cx, TRUE);
					if (js_cx_input_pending(cx) != 0) {
						js_do_lock_input(cx, FALSE);
						timeout = 0;
						cev = ev;
					}
					else {
						input_locked = TRUE;
#ifdef PREFER_POLL
						fds[cfd].fd = ev->data.sock;
						fds[cfd].events = POLLIN;
						cfd++;
#else
						FD_SET(ev->data.sock, &rfds);
						if (ev->data.sock > hsock)
							hsock = ev->data.sock;
#endif
					}
					break;
			}
		}

		// TODO: suspend/resume request
#ifdef PREFER_POLL
		switch (poll(fds, cfd, timeout)) {
#else
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
		switch (select(hsock + 1, &rfds, &wfds, NULL, &tv)) {
#endif
			case 0:		// Timeout
				JS_RESUMEREQUEST(cx, rc);
				if (tev && tev->cx && tev->cb)
					ev = tev;
				else if (cev && cev->cx && cev->cb)
					ev = cev;
				else {
					if (cb->rq_head == NULL) {
						JS_ReportError(cx, "Timeout with no event!");
						ret = JS_FALSE;
						goto done;
					}
				}
				break;
			case -1:	// Error...
				JS_RESUMEREQUEST(cx, rc);
				if (ERROR_VALUE != EINTR) {
					JS_ReportError(cx, "poll() returned with error %d", ERROR_VALUE);
					ret = JS_FALSE;
					goto done;
				}
				break;
			default:	// Zero or more sockets ready
				JS_RESUMEREQUEST(cx, rc);
#ifdef PREFER_POLL
				cfd = 0;
#endif
				for (ev = *head; ev; ev = ev->next) {
					if (ev->type == JS_EVENT_SOCKET_READABLE || ev->type == JS_EVENT_SOCKET_READABLE_ONCE) {
#ifdef PREFER_POLL
						if (fds[cfd].revents & ~(POLLOUT | POLLWRNORM | POLLWRBAND)) {
#else
						if (FD_ISSET(ev->data.sock, &rfds)) {
#endif
							break;
						}
#ifdef PREFER_POLL
						cfd++;
#endif
					}
					else if (ev->type == JS_EVENT_SOCKET_WRITABLE || ev->type == JS_EVENT_SOCKET_WRITABLE_ONCE) {
#ifdef PREFER_POLL
						if (fds[cfd].revents & ~(POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI)) {
#else
						if (FD_ISSET(ev->data.sock, &wfds)) {
#endif
							break;
						}
#ifdef PREFER_POLL
						cfd++;
#endif
					}
					else if (ev->type == JS_EVENT_SOCKET_CONNECT) {
#ifdef PREFER_POLL
						if (fds[cfd].revents & ~(POLLOUT | POLLWRNORM | POLLWRBAND)) {
#else
						if (FD_ISSET(ev->data.connect.sv[0], &wfds)) {
#endif
							closesocket(ev->data.connect.sv[0]);
							break;
						}
#ifdef PREFER_POLL
						cfd++;
#endif
					}
					else if (ev->type == JS_EVENT_CONSOLE_INPUT) {
#ifdef PREFER_POLL
						if (fds[cfd].revents & ~(POLLOUT | POLLWRNORM | POLLWRBAND)) {
#else
						if (FD_ISSET(ev->data.sock, &wfds)) {
#endif
							break;
						}
#ifdef PREFER_POLL
						cfd++;
#endif
					}
				}
				break;
		}
		if (ev == NULL) {
			if (cb->rq_head == NULL) {
				JS_ReportError(cx, "poll() returned ready, but didn't find anything");
				ret = JS_FALSE;
				goto done;
			}
			else {
				struct js_runq_entry    *rqe = cb->rq_head;
				cb->rq_head = rqe->next;
				if (cb->rq_head == NULL)
					cb->rq_tail = NULL;
				ret = JS_CallFunction(cx, rqe->cx, rqe->func, 0, NULL, &rval);
				JS_RemoveObjectRoot(cx, &rqe->cx);
				free(rqe);
			}
		}
		else {
			// Deal with things before running the callback
			if (ev->type == JS_EVENT_CONSOLE_INPUT) {
				if (input_locked)
					js_do_lock_input(cx, FALSE);
			}
			if (ev->type == JS_EVENT_SOCKET_CONNECT) {
				if ((jssp = (js_socket_private_t*)JS_GetPrivate(cx, ev->cx)) != NULL) {
					slen = sizeof(jssp->remote_addr.addr);
					getpeername(ev->data.connect.sock, &jssp->remote_addr.addr, &slen);
				}
			}

			ret = JS_CallFunction(cx, ev->cx, ev->cb, 0, NULL, &rval);

			// Clean up/update after call
			if (ev->type == JS_EVENT_SOCKET_CONNECT) {
				// TODO: call recv() and pass result to callback?
				closesocket(ev->data.connect.sv[0]);
			}
			else if (ev->type == JS_EVENT_INTERVAL)
				ev->data.interval.last += ev->data.interval.period;

			// Remove one-shot events
			if (ev->type == JS_EVENT_SOCKET_READABLE_ONCE || ev->type == JS_EVENT_SOCKET_WRITABLE_ONCE
			    || ev->type == JS_EVENT_SOCKET_CONNECT || ev->type == JS_EVENT_TIMEOUT
			    || ev->type == JS_EVENT_CONSOLE_INPUT_ONCE) {
				if (ev->next)
					ev->next->prev = ev->prev;
				if (ev->prev)
					ev->prev->next = ev->next;
				else
					*head = ev->next;
				JS_RemoveObjectRoot(cx, &ev->cx);
				free(ev);
			}
		}

done:
#ifdef PREFER_POLL
		free(fds);
#endif
		if (input_locked)
			js_do_lock_input(cx, FALSE);

		if (!ret)
			break;
	}
	js_clear_events(cx, cb);
	return ret;
}

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
	{"get_size",		js_getsize,			1,	JSTYPE_NUMBER,	JSDOCSTR("[object]")
	,JSDOCSTR("return the size in bytes the object uses in memory (forces GC) ")
	,316
	},
	{"flatten_string",	js_flatten,			1,	JSTYPE_VOID,	JSDOCSTR("[string]")
	,JSDOCSTR("flatten a string, optimizing allocated memory used for concatenated strings")
	,316
	},
	{"exec",	js_execfile,			1,	JSTYPE_NUMBER,	JSDOCSTR("filename [, startup_dir], <i>object</i> scope [,...]")
	,JSDOCSTR("execute a script within the specified scope.  The main difference between this method "
	"and <tt>load()</tt> is that scripts called this way can call <tt>exit()</tt> without terminating the caller.  If it does, any "
	"<tt>on_exit()</tt> handlers will be evaluated in scripts scope when the script exists. <br>"
	"NOTE: to get a child of the current scope, you need to create an object in the current scope. "
	"An anonymous object can be created using '<tt>new function(){}</tt>'. <br>"
	"NOTE: Use <tt>js.exec.apply()</tt> if you need to pass a variable number of arguments to the executed script.")
	,31702
	},
	{"setTimeout",	js_setTimeout,			2,	JSTYPE_NUMBER,	JSDOCSTR("callback, time[, thisObj]")
	,JSDOCSTR("install a timeout.  callback() will be called time ms after the function is called. "
	 "returns an id which can be passed to clearTimeout()")
	,31900
	},
	{"setInterval",	js_setInterval,			2,	JSTYPE_OBJECT,	JSDOCSTR("callback, period[, thisObj]")
	,JSDOCSTR("install a timeout.  callback() will be called every period ms after setInterval() is called."
	 "returns and id which can be passed to clearIntervat()")
	,31900
	},
	{"clearTimeout",	js_clearTimeout,	1,	JSTYPE_VOID,	JSDOCSTR("id")
	,JSDOCSTR("remove a timeout")
	,31900
	},
	{"clearInterval",	js_clearInterval,	1,	JSTYPE_VOID,	JSDOCSTR("id")
	,JSDOCSTR("remove an interval")
	,31900
	},
	{"addEventListener",	js_addEventListener,	2,	JSTYPE_NUMBER,	JSDOCSTR("eventName, callback")
	,JSDOCSTR("Add a listener that is ran after js.dispatchEvent(eventName) is called.  Returns an id to be passed to js.removeEventListener")
	,31900
	},
	{"removeEventListener",	js_removeEventListener,	1,	JSTYPE_VOID,	JSDOCSTR("id")
	,JSDOCSTR("Remove listeners added with js.addEventListener().  id can be a string or an id returned by addEventListener.  This does not remove already triggered callbacks from the runqueue.")
	,31900
	},
	{"dispatchEvent",	js_dispatchEvent,	1,	JSTYPE_VOID,	JSDOCSTR("eventName [, thisObj]")
	,JSDOCSTR("Add all listeners of eventName to the end of the runqueue.  If obj is passed, specifies this in the callback (the js object is used otherwise).")
	,31900
	},
	{"setImmediate",	js_setImmediate,	1,	JSTYPE_VOID,	JSDOCSTR("callback[, thisObj]")
	,JSDOCSTR("adds the callback to the end of the run queue, where it will be called after all pending events are processed")
	,31900
	},
	{0}
};

static JSBool js_internal_resolve(JSContext *cx, JSObject *obj, jsid id)
{
	char*			name=NULL;
	JSBool			ret;

	if(id != JSID_VOID && id != JSID_EMPTY) {
		jsval idval;
		
		JS_IdToValue(cx, id, &idval);
		if(JSVAL_IS_STRING(idval)) {
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
			HANDLE_PENDING(cx, name);
		}
	}

	ret=js_SyncResolve(cx, obj, name, js_properties, js_functions, NULL, 0);
	if(name)
		free(name);
	return(ret);
}

static JSBool js_internal_enumerate(JSContext *cx, JSObject *obj)
{
	return(js_internal_resolve(cx, obj, JSID_VOID));
}

static JSClass js_internal_class = {
     "JsInternal"				/* name			*/
    ,JSCLASS_HAS_PRIVATE	/* flags		*/
	,JS_PropertyStub		/* addProperty	*/
	,JS_PropertyStub		/* delProperty	*/
	,js_get					/* getProperty	*/
	,js_set					/* setProperty	*/
	,js_internal_enumerate	/* enumerate	*/
	,js_internal_resolve	/* resolve		*/
	,JS_ConvertStub			/* convert		*/
	,JS_FinalizeStub		/* finalize		*/
};

void js_EvalOnExit(JSContext *cx, JSObject *obj, js_callback_t* cb)
{
	char*	p;
	jsval	rval;
	JSObject* script;
	BOOL	auto_terminate=cb->auto_terminate;
	JSObject	*glob=JS_GetGlobalObject(cx);
	global_private_t *pt;
	str_list_t	list = NULL;
	struct js_onexit_scope **prev_oes_next = NULL;
	struct js_onexit_scope *oes = NULL;

	pt=(global_private_t *)JS_GetPrivate(cx,JS_GetGlobalObject(cx));
	if(glob==obj) {
		/* Yes, this is recursive to one level */
		while (pt->onexit) {
			if (pt->onexit->scope == glob) {
				// This shouldn't happen, but let's not go inifinite eh?
				JS_ReportError(cx, "js_EvalOnExit() extra scope is global");
				return;
			}
			else {
				oes = pt->onexit;
				js_EvalOnExit(cx, pt->onexit->scope, cb);
				if (oes == pt->onexit) {
					// This *really* shouldn't happen...
					JS_ReportError(cx, "js_EvalOnExit() did not pop on_exit stack");
					return;
				}
			}
		}
		list=pt->exit_func;
	}
	else {
		/* Find this scope in onexit list */
		for (prev_oes_next = &pt->onexit, oes = pt->onexit; oes; prev_oes_next = &(oes->next), oes = oes->next) {
			if (oes->scope == obj) {
				(*prev_oes_next) = oes->next;
				list = oes->onexit;
				JS_RemoveObjectRoot(cx, &oes->scope);
				free(oes);
				break;
			}
		}
		if (oes == NULL)
			return;
	}

	cb->auto_terminate=FALSE;

	while((p=strListPop(&list))!=NULL) {
		if((script=JS_CompileScript(cx, obj, p, strlen(p), NULL, 0))!=NULL) {
			JS_ExecuteScript(cx, obj, script, &rval);
		}
		free(p);
	}

	strListFree(&list);
	if(glob == obj)
		pt->exit_func=NULL;

	if(auto_terminate)
		cb->auto_terminate = TRUE;
}

JSObject* js_CreateInternalJsObject(JSContext* cx, JSObject* parent, js_callback_t* cb, js_startup_t* startup)
{
	JSObject*	obj;

	if((obj = JS_DefineObject(cx, parent, "js", &js_internal_class, NULL
		,JSPROP_ENUMERATE|JSPROP_READONLY))==NULL)
		return(NULL);

	if(!JS_SetPrivate(cx, obj, cb))	/* Store a pointer to js_callback_t */
		return(NULL);

	if(startup!=NULL) {
		JSObject*	load_path_list;
		jsval		val;
		str_list_t	load_path;

		if((load_path_list=JS_NewArrayObject(cx, 0, NULL))==NULL) 
			return(NULL);
		val=OBJECT_TO_JSVAL(load_path_list);
		if(!JS_SetProperty(cx, obj, JAVASCRIPT_LOAD_PATH_LIST, &val)) 
			return(NULL);

		if((load_path=strListSplitCopy(NULL, startup->load_path, ",")) != NULL) {
			JSString*	js_str;
			unsigned	i;

			for(i=0; load_path[i]!=NULL; i++) {
				if((js_str=JS_NewStringCopyZ(cx, load_path[i]))==NULL)
					break;
				val=STRING_TO_JSVAL(js_str);
				if(!JS_SetElement(cx, load_path_list, i, &val))
					break;
			}
			strListFree(&load_path);
		}
	}

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx,obj,"JavaScript engine internal control object",311);
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", prop_desc, JSPROP_READONLY);
#endif

	return(obj);
}

#if defined(_MSC_VER)
void msvc_invalid_parameter_handler(const wchar_t* expression,
   const wchar_t* function, 
   const wchar_t* file, 
   unsigned int line, 
   uintptr_t pReserved)
{
}
#endif

void js_PrepareToExecute(JSContext *cx, JSObject *obj, const char *filename, const char* startup_dir, JSObject *scope)
{
	JSString*	str;
	jsval		val;

	if(JS_GetProperty(cx, obj, "js", &val) && JSVAL_IS_OBJECT(val)) {
		JSObject* js = JSVAL_TO_OBJECT(val);
		char	dir[MAX_PATH+1];

		if(filename!=NULL) {
			char* p;

			if((str=JS_NewStringCopyZ(cx, filename)) != NULL)
				JS_DefineProperty(cx, js, "exec_path", STRING_TO_JSVAL(str)
					,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);
			if((str=JS_NewStringCopyZ(cx, getfname(filename))) != NULL)
				JS_DefineProperty(cx, js, "exec_file", STRING_TO_JSVAL(str)
					,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);
			SAFECOPY(dir,filename);
			p=getfname(dir);
			*p=0;
			if((str=JS_NewStringCopyZ(cx, dir)) != NULL)
				JS_DefineProperty(cx, js, "exec_dir", STRING_TO_JSVAL(str)
					,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);
		}
		if(startup_dir==NULL)
			startup_dir="";
		if((str=JS_NewStringCopyZ(cx, startup_dir)) != NULL)
			JS_DefineProperty(cx, js, "startup_dir", STRING_TO_JSVAL(str)
				,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);
		JS_DefineProperty(cx, js, "scope", OBJECT_TO_JSVAL(scope)
			,NULL,NULL,JSPROP_ENUMERATE|JSPROP_READONLY);
	}
	JS_DefineProperty(cx, scope, "exit_code", JSVAL_VOID
		,NULL,NULL,JSPROP_ENUMERATE|JSPROP_PERMANENT);
#if defined(_MSC_VER)
	_set_invalid_parameter_handler(msvc_invalid_parameter_handler);
#endif
}
