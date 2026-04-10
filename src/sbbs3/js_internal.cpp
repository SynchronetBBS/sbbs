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
#include "js_socket.h"

/* SpiderMonkey: */
#include <js/friend/JSMEnvironment.h>
#if TODO_DEBUG
#include <jsdbgapi.h>
#endif

enum {
	PROP_VERSION
	, PROP_TERMINATED
	, PROP_AUTO_TERMINATE
	, PROP_COUNTER
	, PROP_TIME_LIMIT
	, PROP_YIELD_INTERVAL
	, PROP_GC_INTERVAL
	, PROP_GC_ATTEMPTS
#ifdef jscntxt_h___
	, PROP_GC_COUNTER
	, PROP_GC_LASTBYTES
	, PROP_BYTES
	, PROP_MAXBYTES
#endif
	, PROP_GLOBAL
	, PROP_OPTIONS
	, PROP_KEEPGOING
};

JSBool js_IsTerminated(JSContext* cx, JSObject* obj)
{
	js_callback_t* cb;
	js_callback_t* top_cb;

	if ((cb = (js_callback_t*)JS_GetPrivate(cx, obj)) == NULL)
		return JS_FALSE;
	for (top_cb = cb; top_cb->bg && top_cb->parent_cb; top_cb = top_cb->parent_cb) {
		if (top_cb->terminated && *top_cb->terminated)
			return JS_TRUE;
	}
	return top_cb->terminated && *top_cb->terminated;
}

static JSBool js_internal_get_value(JSContext* cx, js_callback_t* cb, jsint tiny, jsval* vp)
{
	switch (tiny) {
		case PROP_VERSION:
			*vp = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, JS_GetImplementationVersion()));
			break;
		case PROP_TERMINATED:
			if (cb != NULL) {
				js_callback_t* top_cb = cb;
				while (top_cb->bg && top_cb->parent_cb) {
					if (top_cb->terminated && *top_cb->terminated) {
						*vp = BOOLEAN_TO_JSVAL(TRUE);
						return JS_TRUE;
					}
					top_cb = top_cb->parent_cb;
				}
				*vp = top_cb->terminated ? BOOLEAN_TO_JSVAL(*top_cb->terminated) : JSVAL_FALSE;
			}
			break;
		case PROP_AUTO_TERMINATE:
			if (cb != NULL)
				*vp = BOOLEAN_TO_JSVAL(cb->auto_terminate);
			break;
		case PROP_COUNTER:
			if (cb != NULL)
				*vp = DOUBLE_TO_JSVAL((double)cb->counter);
			break;
		case PROP_TIME_LIMIT:
			if (cb != NULL)
				*vp = DOUBLE_TO_JSVAL(cb->limit);
			break;
		case PROP_YIELD_INTERVAL:
			if (cb != NULL)
				*vp = DOUBLE_TO_JSVAL((double)cb->yield_interval);
			break;
		case PROP_GC_INTERVAL:
			if (cb != NULL)
				*vp = DOUBLE_TO_JSVAL((double)cb->gc_interval);
			break;
		case PROP_GC_ATTEMPTS:
			if (cb != NULL)
				*vp = DOUBLE_TO_JSVAL((double)cb->gc_attempts);
			break;
		case PROP_GLOBAL:
			*vp = OBJECT_TO_JSVAL(JS_GetGlobalObject(cx));
			break;
		case PROP_KEEPGOING:
			if (cb != NULL && cb->events_supported)
				*vp = BOOLEAN_TO_JSVAL(cb->keepGoing);
			else
				*vp = JSVAL_FALSE;
			break;
		default:
			return JS_TRUE;
	}
	return JS_TRUE;
}

static JSBool js_internal_set_value(JSContext* cx, js_callback_t* cb, jsint tiny, jsval* vp)
{
	int32 val = 0;

	if (cb == NULL)
		return JS_TRUE;

	if (JSVAL_IS_NUMBER(*vp) || JSVAL_IS_BOOLEAN(*vp)) {
		if (!JS_ValueToInt32(cx, *vp, &val))
			return JS_FALSE;
	}

	switch (tiny) {
		case PROP_TERMINATED:
			if (cb->terminated != NULL)
				*cb->terminated = val ? true : false;
			break;
		case PROP_AUTO_TERMINATE:
			cb->auto_terminate = val;
			break;
		case PROP_COUNTER:
			cb->counter = val;
			break;
		case PROP_TIME_LIMIT:
			cb->limit = val;
			break;
		case PROP_GC_INTERVAL:
			cb->gc_interval = val;
			break;
		case PROP_YIELD_INTERVAL:
			cb->yield_interval = val;
			break;
		case PROP_KEEPGOING:
			if (cb->events_supported)
				cb->keepGoing = val;
			break;
		default:
			return JS_TRUE;
	}

	return JS_TRUE;
}

#define PROP_FLAGS  JSPROP_ENUMERATE | JSPROP_READONLY

static jsSyncPropertySpec js_properties[] = {
/*		 name,				tinyid,						flags,		ver	*/

	{   "version",          PROP_VERSION,       PROP_FLAGS,         311
		, JSDOCSTR("JavaScript engine version information (AKA system.js_version) - <small>READ ONLY</small>")},
	{   "auto_terminate",   PROP_AUTO_TERMINATE, JSPROP_ENUMERATE,   311
		, JSDOCSTR("Set to <i>false</i> to disable the automatic termination of the script upon external request or user disconnection")},
	{   "terminated",       PROP_TERMINATED,    JSPROP_ENUMERATE,   311
		, JSDOCSTR("Termination has been requested (stop execution as soon as possible)")},
	{   "branch_counter",   PROP_COUNTER,       0,                  311
		, JSDOCSTR("alias")},
	{   "counter",          PROP_COUNTER,       JSPROP_ENUMERATE,   316
		, JSDOCSTR("Number of operation callbacks performed in this runtime")},
	{   "branch_limit",     PROP_TIME_LIMIT,    0,                  311
		, JSDOCSTR("alias")},
	{   "time_limit",       PROP_TIME_LIMIT,    JSPROP_ENUMERATE,   316
		, JSDOCSTR("Maximum number of operation callbacks, used for infinite-loop detection (0=disabled)")},
	{   "yield_interval",   PROP_YIELD_INTERVAL, JSPROP_ENUMERATE,   311
		, JSDOCSTR("Interval of periodic time-slice yields (lower number=higher frequency, 0=disabled)")},
	{   "gc_interval",      PROP_GC_INTERVAL,   JSPROP_ENUMERATE,   311
		, JSDOCSTR("Interval of periodic garbage collection attempts (lower number=higher frequency, 0=disabled)")},
	{   "gc_attempts",      PROP_GC_ATTEMPTS,   PROP_FLAGS,         311
		, JSDOCSTR("Number of garbage collections attempted in this runtime - <small>READ ONLY </small>")},
#ifdef jscntxt_h___
	{   "gc_counter",       PROP_GC_COUNTER,    PROP_FLAGS,         311
		, JSDOCSTR("Number of garbage collections performed in this runtime - <small>READ ONLY</small>")},
	{   "gc_last_bytes",    PROP_GC_LASTBYTES,  PROP_FLAGS,         311
		, JSDOCSTR("Number of heap bytes in use after last garbage collection - <small>READ ONLY</small>")},
	{   "bytes",            PROP_BYTES,         PROP_FLAGS,         311
		, JSDOCSTR("Number of heap bytes currently in use - <small>READ ONLY</small>")},
	{   "max_bytes",        PROP_MAXBYTES,      JSPROP_ENUMERATE,   311
		, JSDOCSTR("Maximum number of bytes available for heap")},
#endif
	{   "global",           PROP_GLOBAL,        PROP_FLAGS,         314
		, JSDOCSTR("Global (top level) object - <small>READ ONLY</small>")},
	{   "options",          PROP_OPTIONS,       PROP_FLAGS,         31802
		, JSDOCSTR("Option flags - <small>READ ONLY</small>")},
	{   "do_callbacks",     PROP_KEEPGOING,     JSPROP_ENUMERATE,   31900
		, JSDOCSTR("Do callbacks after script finishes running")},
	{0}
};

#ifdef BUILD_JSDOCS
static const char* prop_desc[] = {
	/* New properties go here... */
	"<tt>load()</tt> search path array.<br>For relative load paths (e.g. not beginning with '/' or '\\'), "
		"the path is assumed to be a sub-directory of the (configurable) mods or exec directories "
		"and is searched accordingly.<br>"
		"So, by default, <tt>load(\"somefile.js\")</tt> will search in this order:<tt><ol>"
		"<li>mods/load/somefile.js"
		"<li>exec/load/somefile.js"
		"<li>mods/somefile.js"
		"<li>exec/somefile.js"
		"</ol></tt>"
	, "Full path and filename of JS file executed"
	, "JS filename executed (with no path)"
	, "Directory of executed JS file"
	, "Either the configured startup directory in SCFG (for externals) or the CWD when jsexec was started"
	, "Global scope for this script"
	, NULL
};
#endif

JSBool
js_CommonOperationCallback(JSContext *cx, js_callback_t* cb)
{
	js_callback_t *top_cb;

	cb->counter++;

	/* Terminated? */
	if (cb->auto_terminate) {
		for (top_cb = cb; top_cb; top_cb = top_cb->parent_cb) {
			if (top_cb->terminated != NULL && *top_cb->terminated) {
				top_cb->auto_terminated = true;
				/* Log as warning (matching old JS_ReportWarning behavior) */
				JS::WarnASCII(cx, "Terminated");
				/* SM128: returning false causes HandleInterrupt to report
				 * JSMSG_TERMINATED and return false, stopping execution.
				 * Do NOT set a pending exception here — it interferes with
				 * HandleInterrupt's own error reporting. */
				cb->counter = 0;
				return JS_FALSE;
			}
		}
	}

	/* Infinite loop? */
	if (cb->limit && cb->counter > cb->limit) {
		JS_ReportError(cx, "Infinite loop (%u operation callbacks) detected", cb->counter);
		cb->counter = 0;
		return JS_FALSE;
	}

	/* Give up timeslices every once in a while */
	if (cb->yield_interval && (cb->counter % cb->yield_interval) == 0) {

		YIELD();
	}

	/* Permit other contexts to run GC */

	/* Periodic Garbage Collection */
	if (cb->gc_interval && (cb->counter % cb->gc_interval) == 0)
		JS_MaybeGC(cx), cb->gc_attempts++;

	return JS_TRUE;
}

// This is kind of halfway between js_execfile() in exec.cpp and js_load
extern JSClass js_global_class; // Hurr derr
static JSBool
js_execfile(JSContext *cx, uintN argc, jsval *arglist)
{
	char*           cmd = NULL;
	size_t          cmdlen;
	size_t          pathlen;
	char*           startup_dir = NULL;
	uintN           arg = 0;
	char            path[MAX_PATH + 1] = "";
	JS::RootedObject scope(cx, JS_GetScopeChain(cx));
	JSObject*       js_scope = NULL;
	JS::RootedObject js_scope_root(cx, nullptr);
	JS::RootedObject pscope(cx);
	JSScript*       js_script = NULL;
	JS::RootedObject nargv(cx);
	jsval           rval = JSVAL_VOID;
	uintN           i;
	jsval           val;
	JS::RootedObject js_obj(cx);
	JS::RootedObject pjs_obj(cx);
	js_callback_t * js_callback;
	global_private_t *gptp;

	if ((gptp = (global_private_t*)js_GetClassPrivate(cx, JS_GetGlobalObject(cx), &js_global_class)) == NULL)
		return -1;

	if (argc < 1) {
		JS_ReportError(cx, "No filename passed");
		return JS_FALSE;
	}

	jsval *argv = JS_ARGV(cx, arglist);

	if (!JSVAL_IS_STRING(argv[arg])) {
		JS_ReportError(cx, "Invalid script name");
		return JS_FALSE;
	}
	JSVALUE_TO_MSTRING(cx, argv[arg++], cmd, NULL);
	HANDLE_PENDING(cx, cmd);
	if (cmd == NULL) {
		JS_ReportError(cx, "Invalid NULL string");
		return JS_FALSE;
	}

	if (argc > arg) {
		if (JSVAL_IS_STRING(argv[arg])) {
			JSVALUE_TO_MSTRING(cx, argv[arg++], startup_dir, &cmdlen);
			HANDLE_PENDING(cx, cmd);
			if (startup_dir == NULL) {
				free(cmd);
				JS_ReportError(cx, "Invalid NULL string");
				return JS_FALSE;
			}
		}
	}

	if (argc > arg && JSVAL_IS_OBJECT(argv[arg])) {
		js_scope = JSVAL_TO_OBJECT(argv[arg++]);
		if (js_scope == scope.get()) {
			free(cmd);
			free(startup_dir);
			JS_ReportError(cx, "Invalid Scope");
			return JS_FALSE;
		}
	}
	if (js_scope == NULL) {
		free(cmd);
		free(startup_dir);
		JS_ReportError(cx, "Invalid Scope");
		return JS_FALSE;
	}
	/* Root js_scope so SM128's moving GC keeps the pointer up-to-date */
	js_scope_root = js_scope;

	pscope = scope;
	while ((!JS_GetProperty(cx, pscope, "js", &val) || val == JSVAL_VOID || !JSVAL_IS_OBJECT(val)) && pscope) {
		pscope = JS_GetParent(cx, pscope);
		if (!pscope) {
			free(cmd);
			free(startup_dir);
			JS_ReportError(cx, "Walked to global, no js object!");
			return JS_FALSE;
		}
	}
	pjs_obj = JSVAL_TO_OBJECT(val);
	if (!pjs_obj) {
		free(cmd);
		free(startup_dir);
		JS_ReportError(cx, "Unable to get parent js object");
		return JS_FALSE;
	}
	js_callback = static_cast<js_callback_t *>(JS_GetPrivate(cx, pjs_obj));

	if (isfullpath(cmd))
		SAFECOPY(path, cmd);
	else {
		// If startup dir specified, check there first.
		if (startup_dir) {
			SAFECOPY(path, startup_dir);
			backslash(path);
			strncat(path, cmd, sizeof(path) - strlen(path) - 1);
			if (!fexist(path))
				*path = 0;
		}
		// Then check js.exec_dir
		/* if js.exec_dir is defined (location of executed script), search there first */
		if (*path == 0) {
			if (JS_GetProperty(cx, pjs_obj, "exec_dir", &val) && val != JSVAL_VOID && JSVAL_IS_STRING(val)) {
				JSVALUE_TO_STRBUF(cx, val, path, sizeof(path), &pathlen);
				strncat(path, cmd, sizeof(path) - pathlen - 1);
				if (!fexistcase(path))
					path[0] = 0;
			}
			if (*path == '\0') {
				if (gptp->cfg->mods_dir[0] != '\0') {
					snprintf(path, sizeof path, "%s%s", gptp->cfg->mods_dir, cmd);
					if (!fexistcase(path))
						*path = '\0';
				}
				if (*path == '\0') {
					snprintf(path, sizeof path, "%s%s", gptp->cfg->exec_dir, cmd);
					if (!fexistcase(path))
						SAFECOPY(path, cmd);
				}
			}
		}
	}
	free(cmd);

	if (!fexistcase(path)) {
		JS_ReportError(cx, "Script file (%s) does not exist", path);
		free(startup_dir);
		return JS_FALSE;
	}

	nargv = JS_NewArrayObject(cx, 0, NULL);

	JS_DefineProperty(cx, js_scope_root.get(), "argv", OBJECT_TO_JSVAL(nargv.get())
	                  , NULL, NULL, JSPROP_READONLY | JSPROP_ENUMERATE);

	uintN nargc = 0;
	for (i = arg; i < argc; i++) {
		JS_SetElement(cx, nargv, nargc, &argv[i]);
		nargc++;
	}

	JS_DefineProperty(cx, js_scope_root.get(), "argc", INT_TO_JSVAL(nargc)
	                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);

	js_obj = js_CreateInternalJsObject(cx, js_scope_root.get(), js_callback, NULL);
	js_PrepareToExecute(cx, js_scope_root.get(), path, startup_dir, js_scope_root.get());
	free(startup_dir);
	// Copy in the load_path_list...
	{
		JS::RootedObject pload_path_list(cx);
		JS::RootedObject load_path_list(cx);
		uint32_t  plen;
		uint32_t  pcnt;

		if (JS_GetProperty(cx, pjs_obj, JAVASCRIPT_LOAD_PATH_LIST, &val) && val != JSVAL_VOID && JSVAL_IS_OBJECT(val)) {
			pload_path_list = JSVAL_TO_OBJECT(val);
			if (!pload_path_list || !JS_IsArrayObject(cx, pload_path_list)) {
				JS_ReportError(cx, "Weird js." JAVASCRIPT_LOAD_PATH_LIST " value");
				return JS_FALSE;
			}
			if (!(load_path_list = JS_NewArrayObject(cx, 0, NULL))) {
				JS_ReportError(cx, "Unable to create js." JAVASCRIPT_LOAD_PATH_LIST);
				return JS_FALSE;
			}
			val = OBJECT_TO_JSVAL(load_path_list.get());
			JS_SetProperty(cx, js_obj, JAVASCRIPT_LOAD_PATH_LIST, &val);
			if (JS_GetArrayLength(cx, pload_path_list, &plen))
				for (pcnt = 0; pcnt < plen; pcnt++) {
					if (JS_GetElement(cx, pload_path_list, pcnt, &val))
						JS_SetElement(cx, load_path_list, pcnt, &val);
				}
		}
		else {
			JS_ReportError(cx, "Unable to get parent js." JAVASCRIPT_LOAD_PATH_LIST " array.");
			return JS_FALSE;
		}
	}

	/* SM128: when a scope object was provided, use ExecuteInJSMEnvironment
	 * for correct non-syntactic scoping.  See js_global.cpp js_load(). */
	bool nonsyntactic = (js_scope != scope.get());
	JS::RootedObject jsmEnv(cx);
	if (nonsyntactic) {
		JS::CompileOptions opts(cx);
		opts.setFileAndLine(path, 1);
		opts.setNonSyntacticScope(true);
		js_script = JS::CompileUtf8Path(cx, opts, path);
		if (js_script != NULL) {
			jsmEnv.set(JS::NewJSMEnvironment(cx));
			if (!jsmEnv)
				js_script = NULL;
		}
	} else {
		js_script = JS_CompileFile(cx, js_scope_root.get(), path);
	}

	if (js_script == NULL) {
		/* If the script fails to compile, it's not a fatal error
		 * for the caller. */
		if (JS_IsExceptionPending(cx)) {
			JS_GetPendingException(cx, &rval);
			JS_SET_RVAL(cx, arglist, rval);
		}
		JS_ClearPendingException(cx);
		return JS_TRUE;
	}

	/* SM128: JS_ExecuteScript ignores scope_obj, so scripts run in the main global.
	 * Temporarily update the parent js object's exec_dir/exec_path/exec_file/startup_dir
	 * so that load() calls within the sub-script resolve files correctly.
	 * Also update argv/argc on the global object so the subscript sees the right args.
	 * Rooted values are used to prevent GC from collecting the saved strings. */
	JSObject* global = JS_GetGlobalObject(cx);
	JS::RootedValue saved_exec_dir(cx, JS::UndefinedValue());
	JS::RootedValue saved_exec_path(cx, JS::UndefinedValue());
	JS::RootedValue saved_exec_file(cx, JS::UndefinedValue());
	JS::RootedValue saved_startup_dir(cx, JS::UndefinedValue());
	bool saved_exec_dir_ok = JS_GetProperty(cx, pjs_obj, "exec_dir", saved_exec_dir.address()) && saved_exec_dir.get() != JSVAL_VOID;
	bool saved_exec_path_ok = JS_GetProperty(cx, pjs_obj, "exec_path", saved_exec_path.address()) && saved_exec_path.get() != JSVAL_VOID;
	bool saved_exec_file_ok = JS_GetProperty(cx, pjs_obj, "exec_file", saved_exec_file.address()) && saved_exec_file.get() != JSVAL_VOID;
	bool saved_startup_dir_ok = JS_GetProperty(cx, pjs_obj, "startup_dir", saved_startup_dir.address()) && saved_startup_dir.get() != JSVAL_VOID;
	JS::RootedValue saved_scope(cx, JS::UndefinedValue());
	bool saved_scope_ok = JS_GetProperty(cx, pjs_obj, "scope", saved_scope.address()) && saved_scope.get() != JSVAL_VOID;
	JS::RootedValue saved_argv(cx, JS::UndefinedValue());
	JS::RootedValue saved_argc(cx, JS::UndefinedValue());
	JS_GetProperty(cx, global, "argv", saved_argv.address());
	JS_GetProperty(cx, global, "argc", saved_argc.address());
	{
		jsval tmp;
		if (JS_GetProperty(cx, js_obj, "exec_dir", &tmp) && tmp != JSVAL_VOID)
			JS_DefineProperty(cx, pjs_obj, "exec_dir", tmp, NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
		if (JS_GetProperty(cx, js_obj, "exec_path", &tmp) && tmp != JSVAL_VOID)
			JS_DefineProperty(cx, pjs_obj, "exec_path", tmp, NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
		if (JS_GetProperty(cx, js_obj, "exec_file", &tmp) && tmp != JSVAL_VOID)
			JS_DefineProperty(cx, pjs_obj, "exec_file", tmp, NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
		if (JS_GetProperty(cx, js_obj, "startup_dir", &tmp) && tmp != JSVAL_VOID)
			JS_DefineProperty(cx, pjs_obj, "startup_dir", tmp, NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
		if (JS_GetProperty(cx, js_obj, "scope", &tmp) && tmp != JSVAL_VOID)
			JS_DefineProperty(cx, pjs_obj, "scope", tmp, NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
		/* Install subscript's argv/argc on the global so the script sees them */
		if (JS_GetProperty(cx, js_scope_root.get(), "argv", &tmp) && tmp != JSVAL_VOID)
			JS_DefineProperty(cx, global, "argv", tmp, NULL, NULL, JSPROP_READONLY | JSPROP_ENUMERATE);
		if (JS_GetProperty(cx, js_scope_root.get(), "argc", &tmp) && tmp != JSVAL_VOID)
			JS_DefineProperty(cx, global, "argc", tmp, NULL, NULL, JSPROP_READONLY | JSPROP_ENUMERATE);
	}

	unsigned orig_bgcount = gptp->bg_count;
	if (nonsyntactic) {
		JS::RootedScript rscript(cx, js_script);
		bool ok = JS::ExecuteInJSMEnvironment(cx, rscript, jsmEnv);
		if (ok) {
			/* Copy const/let bindings from lexical env to jsmEnv */
			if (JS_HasExtensibleLexicalEnvironment(jsmEnv)) {
				JS::RootedObject lexEnv(cx, JS_ExtensibleLexicalEnvironment(jsmEnv));
				JS::RootedIdVector lexIds(cx);
				if (js::GetPropertyKeys(cx, lexEnv, JSITER_OWNONLY | JSITER_HIDDEN, &lexIds)) {
					for (size_t i = 0; i < lexIds.length(); i++) {
						JS::RootedValue val(cx);
						JS::RootedId id(cx, lexIds[i]);
						if (JS_GetPropertyById(cx, lexEnv, id, &val))
							JS_SetPropertyById(cx, jsmEnv, id, val);
					}
				}
			}
			rval = OBJECT_TO_JSVAL(jsmEnv);
		}
	} else {
		JS_ExecuteScript(cx, js_scope_root.get(), js_script, &rval);
	}
	if (JS_IsExceptionPending(cx)) {
		JS_GetPendingException(cx, &rval);
	}
	else {
		jsval exit_code = JSVAL_VOID;
		if (JS_GetProperty(cx, js_scope_root.get(), "exit_code", &exit_code) && exit_code != JSVAL_VOID)
			rval = exit_code;
	}
	JS_SET_RVAL(cx, arglist, rval);
	JS_ClearPendingException(cx);

	/* Restore the parent js object's exec context properties */
	if (saved_exec_dir_ok)
		JS_DefineProperty(cx, pjs_obj, "exec_dir", saved_exec_dir.get(), NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
	if (saved_exec_path_ok)
		JS_DefineProperty(cx, pjs_obj, "exec_path", saved_exec_path.get(), NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
	if (saved_exec_file_ok)
		JS_DefineProperty(cx, pjs_obj, "exec_file", saved_exec_file.get(), NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
	if (saved_startup_dir_ok)
		JS_DefineProperty(cx, pjs_obj, "startup_dir", saved_startup_dir.get(), NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
	if (saved_scope_ok)
		JS_DefineProperty(cx, pjs_obj, "scope", saved_scope.get(), NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
	/* Restore global argv/argc */
	if (saved_argv.get() != JSVAL_VOID)
		JS_DefineProperty(cx, global, "argv", saved_argv.get(), NULL, NULL, JSPROP_READONLY | JSPROP_ENUMERATE);
	if (saved_argc.get() != JSVAL_VOID)
		JS_DefineProperty(cx, global, "argc", saved_argc.get(), NULL, NULL, JSPROP_READONLY | JSPROP_ENUMERATE);

	js_EvalOnExit(cx, js_scope_root.get(), js_callback);
	while (gptp->bg_count > orig_bgcount) {
		if (sem_wait(&gptp->bg_sem) == 0)
			gptp->bg_count--;
	}

	JS_GC(cx);

	return JS_TRUE;
}

static const JSClassOps eval_classops = {
	nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr
};

static JSClass eval_class = {
	"Global",
	JSCLASS_GLOBAL_FLAGS,
	&eval_classops
};

/* Execute a string in its own context (away from Synchronet objects) */
static JSBool
js_eval(JSContext *parent_cx, uintN argc, jsval *arglist)
{
	jsval *         argv = JS_ARGV(parent_cx, arglist);
	char*           buf = NULL;
	size_t          buflen;
	JSString*       str;
	JSScript*       script;
	JSContext*      cx;
	JSObject*       obj;
	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if (argc < 1)
		return JS_TRUE;

	if ((str = JS_ValueToString(parent_cx, argv[0])) == NULL)
		return JS_FALSE;
	JSSTRING_TO_MSTRING(parent_cx, str, buf, &buflen);
	HANDLE_PENDING(parent_cx, buf);
	if (buf == NULL)
		return JS_TRUE;

	if ((cx = JS_NewContext(JAVASCRIPT_MAX_BYTES)) == NULL) {
		free(buf);
		return JS_FALSE;
	}

	JS_SetContextPrivate(cx, JS_GetContextPrivate(parent_cx));

	if ((obj = JS_NewCompartmentAndGlobalObject(cx, &eval_class, NULL)) == NULL
	    || !JS_InitStandardClasses(cx, obj)) {
		JS_DestroyContext(cx);
		free(buf);
		return JS_FALSE;
	}

	if ((script = JS_CompileScript(cx, obj, buf, buflen, NULL, 0)) != NULL) {
		jsval rval;

		JS_ExecuteScript(cx, obj, script, &rval);
		JS_SET_RVAL(cx, arglist, rval);
	}
	free(buf);

	JS_DestroyContext(cx);

	return JS_TRUE;
}

static JSBool
js_gc(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *     obj = JS_THIS_OBJECT(cx, arglist);
	jsval *        argv = JS_ARGV(cx, arglist);
	JSBool         forced = JS_TRUE;
	js_callback_t* cb;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((cb = (js_callback_t*)JS_GetPrivate(cx, obj)) == NULL)
		return JS_FALSE;

	if (argc)
		JS_ValueToBoolean(cx, argv[0], &forced);

	if (forced)
		JS_GC(cx);
	else
		JS_MaybeGC(cx);

	cb->gc_attempts++;

	return JS_TRUE;
}

static JSBool
js_report_error(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv = JS_ARGV(cx, arglist);
	char * p = NULL;

	JSVALUE_TO_MSTRING(cx, argv[0], p, NULL);
	HANDLE_PENDING(cx, p);
	if (p == NULL)
		JS_ReportError(cx, "NULL");
	else {
		JS_ReportError(cx, "%s", p);
		free(p);
	}

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if (argc > 1 && argv[1] == JSVAL_TRUE)
		return JS_FALSE;  /* fatal */

	return JS_TRUE;
}

static JSBool
js_on_exit(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *              scope = JS_GetScopeChain(cx);
	JSObject *              glob = JS_GetGlobalObject(cx);
	jsval *                 argv = JS_ARGV(cx, arglist);
	global_private_t*       pd;
	struct js_onexit_scope *oes = NULL;
	str_list_t              oldlist;
	str_list_t              list;
	char *                  p = NULL;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((pd = (global_private_t*)JS_GetPrivate(cx, glob)) == NULL)
		return JS_FALSE;
	if (glob == scope) {
		if (pd->exit_func == NULL)
			pd->exit_func = strListInit();
		list = pd->exit_func;
	}
	else {
		/* First, look for an existing onexit scope for this scope */
		for (oes = pd->onexit; oes; oes = oes->next) {
			if (oes->scope == scope)
				break;
		}

		/* If one isn't found, insert it */
		if (oes == NULL) {
			oes = static_cast<js_onexit_scope *>(malloc(sizeof(*oes)));
			if (oes == NULL) {
				JS_ReportError(cx, "Unable to allocate memory for onexit scope");
				return JS_FALSE;
			}
			oes->next = pd->onexit;
			pd->onexit = oes;
			oes->scope = scope;
			oes->onexit = strListInit();
		}
		list = oes->onexit;
	}

	JSVALUE_TO_MSTRING(cx, argv[0], p, NULL);
	HANDLE_PENDING(cx, p);
	if (!p)
		return JS_TRUE;
	oldlist = list;
	strListPush(&list, p);
	free(p);
	if (oldlist != list) {
		if (glob == scope)
			pd->exit_func = list;
		else
			oes->onexit = list;
	}
	return JS_TRUE;
}

static JSBool js_flatten(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv = JS_ARGV(cx, arglist);

	if (!JSVAL_IS_STRING(argv[0])) {
		JS_ReportError(cx, "Parameter is not a string.");
		return JS_FALSE;
	}
	JS_FlattenString(cx, JSVAL_TO_STRING(argv[0]));
	JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	return JS_TRUE;
}

static JSBool
js_setTimeout(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *               argv = JS_ARGV(cx, arglist);
	struct js_event_list *ev;
	js_callback_t*        cb;
	JSObject *            obj = JS_THIS_OBJECT(cx, arglist);
	JSFunction *          ecb;
	uint64_t              now = (uint64_t)(xp_timer() * 1000);
	jsdouble              timeout;

	if ((cb = (js_callback_t*)JS_GetPrivate(cx, obj)) == NULL)
		return JS_FALSE;

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
	ev = static_cast<js_event_list *>(malloc(sizeof(*ev)));
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
	int32                 id;
	jsval *               argv = JS_ARGV(cx, arglist);
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
			free(ev);
		}
	}

	return JS_TRUE;
}

static JSBool
js_internal_clear_event(JSContext *cx, uintN argc, jsval *arglist, enum js_event_type et)
{
	js_callback_t* cb;
	JSObject *     obj = JS_THIS_OBJECT(cx, arglist);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if (argc < 1) {
		JS_ReportError(cx, "js.clearTimeout() requires an id");
		return JS_FALSE;
	}
	if ((cb = (js_callback_t*)JS_GetPrivate(cx, obj)) == NULL)
		return JS_FALSE;

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
	jsval *               argv = JS_ARGV(cx, arglist);
	struct js_event_list *ev;
	js_callback_t*        cb;
	JSObject *            obj = JS_THIS_OBJECT(cx, arglist);
	JSFunction *          ecb;
	uint64_t              now = (uint64_t)(xp_timer() * 1000);
	jsdouble              period;

	if ((cb = (js_callback_t*)JS_GetPrivate(cx, obj)) == NULL)
		return JS_FALSE;

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
	ev = static_cast<js_event_list *>(malloc(sizeof(*ev)));
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
	ev->cb = ecb;
	ev->data.interval.last = now;
	ev->data.interval.period = (uint64_t)period;
	ev->id = cb->next_eid++;
	cb->events = ev;

	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(ev->id));
	return JS_TRUE;
}

static JSBool
js_setImmediate(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *               argv = JS_ARGV(cx, arglist);
	JSFunction *          cbf;
	JSObject *            obj = JS_THIS_OBJECT(cx, arglist);
	js_callback_t *       cb;
	struct js_runq_entry *rqe;

	if ((cb = (js_callback_t*)JS_GetPrivate(cx, obj)) == NULL)
		return JS_FALSE;

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

	rqe = static_cast<js_runq_entry *>(malloc(sizeof(*rqe)));
	if (rqe == NULL) {
		JS_ReportError(cx, "error allocating %zu bytes", sizeof(*rqe));
		return JS_FALSE;
	}
	rqe->func = cbf;
	rqe->cx = obj;
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
	jsval *                   argv = JS_ARGV(cx, arglist);
	char *                    name;
	JSFunction *              cbf;
	struct js_listener_entry *listener;
	JSObject *                obj = JS_THIS_OBJECT(cx, arglist);
	js_callback_t *           cb;

	if ((cb = (js_callback_t*)JS_GetPrivate(cx, obj)) == NULL)
		return JS_FALSE;

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

	listener = static_cast<js_listener_entry *>(malloc(sizeof(*listener)));
	if (listener == NULL) {
		free(name);
		JS_ReportError(cx, "error allocating %zu bytes", sizeof(*listener));
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
	jsval *                    argv = JS_ARGV(cx, arglist);
	int32                      id;
	struct js_listener_entry * listener;
	struct js_listener_entry **pnext;
	char *                     name;
	JSObject *                 obj = JS_THIS_OBJECT(cx, arglist);
	js_callback_t *            cb;

	if ((cb = (js_callback_t*)JS_GetPrivate(cx, obj)) == NULL)
		return JS_FALSE;

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
	jsval *                   argv = JS_ARGV(cx, arglist);
	struct js_listener_entry *listener;
	struct js_runq_entry *    rqe;
	JSObject *                obj = JS_THIS_OBJECT(cx, arglist);
	char *                    name;
	js_callback_t *           cb;

	if ((cb = (js_callback_t*)JS_GetPrivate(cx, obj)) == NULL)
		return JS_FALSE;

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
			rqe = static_cast<js_runq_entry *>(malloc(sizeof(*rqe)));
			if (rqe == NULL) {
				JS_ReportError(cx, "error allocating %zu bytes", sizeof(*rqe));
				free(name);
				return JS_FALSE;
			}
			rqe->func = listener->func;
			rqe->cx = obj;
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
	struct js_event_list *    ev;
	struct js_event_list *    nev;
	struct js_runq_entry *    rq;
	struct js_runq_entry *    nrq;
	struct js_listener_entry *le;
	struct js_listener_entry *nle;

	for (ev = cb->events, nev = NULL; ev; ev = nev) {
		nev = ev->next;
		free(ev);
	}
	cb->next_eid = 0;
	cb->events = NULL;

	for (rq = cb->rq_head; rq; rq = nrq) {
		nrq = rq->next;
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
js_handle_events(JSContext *cx, js_callback_t *cb, volatile bool *terminated)
{
	struct js_event_list **head = &cb->events;
	int                    timeout;
	int                    i;
	uint64_t               now;
	struct js_event_list * ev;
	struct js_event_list * tev;
	struct js_event_list * cev;
	jsval                  rval = JSVAL_NULL;
	JSBool                 ret = JS_TRUE;
	BOOL                   input_locked = FALSE;
	js_socket_private_t*   jssp;
	socklen_t              slen;
	JSFunction*            evf;
	JSObject*              evo;
#ifdef PREFER_POLL
	struct pollfd *        fds;
	nfds_t                 sc;
	nfds_t                 cfd;
#else
	fd_set                 rfds;
	fd_set                 wfds;
	struct timeval         tv;
	SOCKET                 hsock;
#endif

	if (!cb->events_supported)
		return JS_FALSE;

	while (cb->keepGoing && !JS_IsExceptionPending(cx) && (cb->events || cb->rq_head) && !*terminated) {
		timeout = -1;   // Infinity by default...
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
			fds = static_cast<pollfd *>(calloc(sc, sizeof(*fds)));
			if (fds == NULL) {
				JS_ReportError(cx, "error allocating %lu elements of %zu bytes", (unsigned long)sc, sizeof(*fds));
				return JS_FALSE;
			}
		}
#else
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
#endif

		if (cb->rq_head)
			timeout = 0;
		for (ev = *head; ev; ev = ev->next) {
			switch (ev->type) {
				case JS_EVENT_SOCKET_READABLE_ONCE:
				case JS_EVENT_SOCKET_READABLE:
#ifdef PREFER_POLL
					if (fds && cfd < sc) {
						fds[cfd].fd = ev->data.sock;
						fds[cfd].events = POLLIN;
						cfd++;
					}
					else
						JS_ReportError(cx, "socket count does not match event list");
#else
					FD_SET(ev->data.sock, &rfds);
					if (ev->data.sock > hsock)
						hsock = ev->data.sock;
#endif
					break;
				case JS_EVENT_SOCKET_WRITABLE_ONCE:
				case JS_EVENT_SOCKET_WRITABLE:
#ifdef PREFER_POLL
					if (fds && cfd < sc) {
						fds[cfd].fd = ev->data.sock;
						fds[cfd].events = POLLOUT;
						cfd++;
					}
					else
						JS_ReportError(cx, "socket count does not match event list");
#else
					FD_SET(ev->data.sock, &wfds);
					if (ev->data.sock > hsock)
						hsock = ev->data.sock;
#endif
					break;
				case JS_EVENT_SOCKET_CONNECT:
#ifdef PREFER_POLL
					if (fds && cfd < sc) {
						fds[cfd].fd = ev->data.connect.sv[0];
						fds[cfd].events = POLLIN;
						cfd++;
					}
					else
						JS_ReportError(cx, "socket count does not match event list");
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
						if (fds && cfd < sc) {
							fds[cfd].fd = ev->data.sock;
							fds[cfd].events = POLLIN;
							cfd++;
						}
						else
							JS_ReportError(cx, "socket count does not match event list");
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
			case 0:     // Timeout
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
			case -1:    // Error...
				if (SOCKET_ERRNO != EINTR) {
					JS_ReportError(cx, "poll() returned with error %d", SOCKET_ERRNO);
					ret = JS_FALSE;
					goto done;
				}
				break;
			default:    // Zero or more sockets ready
#ifdef PREFER_POLL
				cfd = 0;
#endif
				for (ev = *head; ev; ev = ev->next) {
					if (ev->type == JS_EVENT_SOCKET_READABLE || ev->type == JS_EVENT_SOCKET_READABLE_ONCE) {
#ifdef PREFER_POLL
						if (sc && cfd < sc && fds[cfd].revents & ~(POLLOUT | POLLWRNORM | POLLWRBAND)) {
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
						if (sc && cfd < sc && fds[cfd].revents & ~(POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI)) {
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
						if (sc && cfd < sc && fds[cfd].revents & ~(POLLOUT | POLLWRNORM | POLLWRBAND)) {
#else
						if (FD_ISSET(ev->data.connect.sv[0], &rfds)) {
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
						if (sc && cfd < sc && fds[cfd].revents & ~(POLLOUT | POLLWRNORM | POLLWRBAND)) {
#else
						if (FD_ISSET(ev->data.sock, &rfds)) {
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
				struct js_runq_entry *rqe = cb->rq_head;
				cb->rq_head = rqe->next;
				if (cb->rq_head == NULL)
					cb->rq_tail = NULL;
				ret = JS_CallFunction(cx, rqe->cx, rqe->func, 0, NULL, &rval);
				free(rqe);
			}
		}
		else {
			// Store a copy of object and function pointers
			evf = ev->cb;
			evo = ev->cx;

			// Deal with things before running the callback
			if (ev->type == JS_EVENT_CONSOLE_INPUT) {
				if (input_locked)
					js_do_lock_input(cx, FALSE);
			}
			if (ev->type == JS_EVENT_SOCKET_CONNECT) {
				if ((jssp = (js_socket_private_t*)JS_GetPrivate(cx, ev->cx)) != NULL) {
					slen = sizeof(jssp->remote_addr.addr);
					if (getpeername(ev->data.connect.sock, &jssp->remote_addr.addr, &slen) == -1) {
						memset(&jssp->remote_addr, 0, sizeof(jssp->remote_addr));
						// This should be zero, but be paranoid
						jssp->remote_addr.addr.sa_family = AF_UNSPEC;
						jssp->last_error = errno;
						socket_strerror(jssp->last_error, jssp->last_error_str, sizeof(jssp->last_error_str));
					}
				}
			}

			// Clean up one-shots before call
			if (ev->type == JS_EVENT_SOCKET_CONNECT) {
				// TODO: call recv() and pass result to callback?
				closesocket(ev->data.connect.sv[0]);
			}

			// Remove one-shot events before call
			if (ev->type == JS_EVENT_SOCKET_READABLE_ONCE || ev->type == JS_EVENT_SOCKET_WRITABLE_ONCE
			    || ev->type == JS_EVENT_SOCKET_CONNECT || ev->type == JS_EVENT_TIMEOUT
			    || ev->type == JS_EVENT_CONSOLE_INPUT_ONCE) {
				if (ev->next)
					ev->next->prev = ev->prev;
				if (ev->prev)
					ev->prev->next = ev->next;
				else
					*head = ev->next;
				free(ev);
				ev = NULL;
			}

			ret = JS_CallFunction(cx, evo, evf, 0, NULL, &rval);

			if (ev) {
				if (ev->type == JS_EVENT_INTERVAL)
					ev->data.interval.last += ev->data.interval.period;
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
	{"eval",            js_eval,            0,  JSTYPE_UNDEF,   JSDOCSTR("script")
	 , JSDOCSTR("Evaluate a JavaScript string in its own (secure) context, returning the result")
	 , 311},
	{"gc",              js_gc,              0,  JSTYPE_VOID,    JSDOCSTR("forced=true")
	 , JSDOCSTR("Perform a garbage collection operation (freeing memory for unused allocated objects), "
		        "if <i>forced</i> is <tt>true</tt> (the default) a garbage collection is always performed, "
		        "otherwise it is only performed if deemed appropriate by the JavaScript engine")
	 , 311},
	{"on_exit",         js_on_exit,         1,  JSTYPE_VOID,    JSDOCSTR("to_eval")
	 , JSDOCSTR("Add a string to evaluate/execute (LIFO stack) upon script's termination (e.g. call of <tt>exit()</tt>)")
	 , 313},
	{"report_error",    js_report_error,    1,  JSTYPE_VOID,    JSDOCSTR("error [,fatal=false]")
	 , JSDOCSTR("Report an error using the standard JavaScript error reporting mechanism "
		        "(including script filename and line number), "
		        "if <i>fatal</i> is <tt>true</tt>, immediately terminates script")
	 , 313},
	{"flatten_string",  js_flatten,         1,  JSTYPE_VOID,    JSDOCSTR("<i>string</i> value")
	 , JSDOCSTR("Flatten a string, optimizing allocated memory used for concatenated strings")
	 , 316},
	{"exec",    js_execfile,            1,  JSTYPE_NUMBER,  JSDOCSTR("<i>string</i> filename [,<i>string</i> startup_dir], <i>object</i> scope [,...]")
	 , JSDOCSTR("Execute a script within the specified <i>scope</i>.  The main difference between this method "
		        "and <tt>load()</tt> is that scripts invoked in this way can call <tt>exit()</tt> without terminating the caller.  If it does, any "
		        "<tt>on_exit()</tt> handlers will be evaluated in the script's scope when the script exits. <br>"
		        "NOTE: to get a child of the current scope, you need to create an object in the current scope. "
		        "An anonymous object can be created using '<tt>new function(){}</tt>'. <br>"
		        "TIP: Use <tt>js.exec.apply()</tt> if you need to pass a variable number of arguments to the executed script.")
	 , 31702},
	{"setTimeout",  js_setTimeout,          2,  JSTYPE_NUMBER,  JSDOCSTR("callback, time [,thisObj]")
	 , JSDOCSTR("Install a timeout timer.  <tt>callback()</tt> will be called <tt>time</tt> (milliseconds) after this function is called. "
		        "Returns an id which can be passed to <tt>clearTimeout()</tt>")
	 , 31900},
	{"setInterval", js_setInterval,         2,  JSTYPE_NUMBER,  JSDOCSTR("callback, period [,thisObj]")
	 , JSDOCSTR("Install an interval timer.  <tt>callback()</tt> will be called every <tt>period</tt> (milliseconds) after <tt>setInterval()</tt> is called. "
		        "Returns an id which can be passed to <tt>clearInterval()</tt>")
	 , 31900},
	{"clearTimeout",    js_clearTimeout,    1,  JSTYPE_VOID,    JSDOCSTR("id")
	 , JSDOCSTR("Remove a timeout timer")
	 , 31900},
	{"clearInterval",   js_clearInterval,   1,  JSTYPE_VOID,    JSDOCSTR("id")
	 , JSDOCSTR("Remove an interval timer")
	 , 31900},
	{"addEventListener",    js_addEventListener,    2,  JSTYPE_NUMBER,  JSDOCSTR("eventName, callback")
	 , JSDOCSTR("Add a listener that is ran after <tt>js.dispatchEvent(eventName)</tt> is called.  Returns an id to be passed to <tt>js.removeEventListener()</tt>")
	 , 31900},
	{"removeEventListener", js_removeEventListener, 1,  JSTYPE_VOID,    JSDOCSTR("id")
	 , JSDOCSTR("Remove listeners added with <tt>js.addEventListener()</tt>.  <tt>id</tt> can be a string or an id returned by <tt>addEventListener()</tt>.  This does not remove already triggered callbacks from the run queue.")
	 , 31900},
	{"dispatchEvent",   js_dispatchEvent,   1,  JSTYPE_VOID,    JSDOCSTR("eventName [,<i>object</i> thisObj]")
	 , JSDOCSTR("Add all listeners of eventName to the end of the run queue.  If <tt>thisObj</tt> is passed, specifies <tt>this</tt> in the callback (the <tt>js</tt> object is used otherwise).")
	 , 31900},
	{"setImmediate",    js_setImmediate,    1,  JSTYPE_VOID,    JSDOCSTR("callback [,thisObj]")
	 , JSDOCSTR("Add <tt>callback</tt> to the end of the run queue, where it will be called after all pending events are processed")
	 , 31900},
	{0}
};

static bool js_internal_prop_getter(JSContext* cx, unsigned argc, JS::Value* vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject thisObj(cx);
	if (!args.computeThis(cx, &thisObj))
		return false;
	js_callback_t* cb = (js_callback_t*)JS_GetPrivate(thisObj);
	JSObject* callee = &args.callee();
	jsint tiny = js::GetFunctionNativeReserved(callee, 0).toInt32();
	jsval val = JSVAL_VOID;
	if (!js_internal_get_value(cx, cb, tiny, &val))
		return false;
	args.rval().set(val);
	return true;
}

static bool js_internal_prop_setter(JSContext* cx, unsigned argc, JS::Value* vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::RootedObject thisObj(cx);
	if (!args.computeThis(cx, &thisObj))
		return false;
	js_callback_t* cb = (js_callback_t*)JS_GetPrivate(thisObj);
	if (cb == nullptr)
		return true;
	JSObject* callee = &args.callee();
	jsint tiny = js::GetFunctionNativeReserved(callee, 0).toInt32();
	jsval val = args.length() > 0 ? args[0] : JSVAL_VOID;
	if (!js_internal_set_value(cx, cb, tiny, &val))
		return false;
	args.rval().set(val);
	return true;
}

static JSBool js_internal_fill_properties(JSContext* cx, JSObject* obj, const char* name)
{
	return js_DefineSyncAccessors(cx, obj, js_properties, js_internal_prop_getter, name, js_internal_prop_setter);
}

static bool js_internal_resolve(JSContext *cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id, bool* resolvedp)
{
	char* name = NULL;

	if (id.get().isString()) {
		JSString* str = id.get().toString();
		JSSTRING_TO_MSTRING(cx, str, name, NULL);
		HANDLE_PENDING(cx, name);
		if (name == NULL) return false;
	}

	bool ret = js_SyncResolve(cx, obj, name, js_properties, js_functions, NULL, 0);
	if (ret)
		js_internal_fill_properties(cx, obj, name);
	if (name)
		free(name);
	if (resolvedp) *resolvedp = ret;
	return true;
}

static bool js_internal_enumerate(JSContext *cx, JS::Handle<JSObject*> obj)
{
	if (!js_SyncResolve(cx, obj, NULL, js_properties, js_functions, NULL, 0))
		return false;
	js_internal_fill_properties(cx, obj, NULL);
	return true;
}

static void js_trace_internal(JSTracer* trc, JSObject* obj)
{
	js_callback_t* cb = (js_callback_t*)JS_GetPrivate(obj);
	if (cb == NULL)
		return;

	for (struct js_event_list* ev = cb->events; ev != NULL; ev = ev->next) {
		if (ev->cb)
			JS::TraceRoot(trc, &ev->cb, "js_event cb");
		if (ev->cx)
			JS::TraceRoot(trc, &ev->cx, "js_event cx");
	}
	for (struct js_runq_entry* rq = cb->rq_head; rq != NULL; rq = rq->next) {
		if (rq->func)
			JS::TraceRoot(trc, &rq->func, "js_runq func");
		if (rq->cx)
			JS::TraceRoot(trc, &rq->cx, "js_runq cx");
	}
	for (struct js_listener_entry* le = cb->listeners; le != NULL; le = le->next) {
		if (le->func)
			JS::TraceRoot(trc, &le->func, "js_listener func");
	}
}

static const JSClassOps js_internal_classops = {
	nullptr,                    /* addProperty  */
	nullptr,                    /* delProperty  */
	js_internal_enumerate,      /* enumerate    */
	nullptr,                    /* newEnumerate */
	js_internal_resolve,        /* resolve      */
	nullptr,                    /* mayResolve   */
	nullptr,                    /* finalize     */
	nullptr,                    /* call         */
	nullptr,                    /* construct    */
	js_trace_internal           /* trace        */
};

static JSClass js_internal_class = {
	"JsInternal"
	, JSCLASS_HAS_PRIVATE
	, &js_internal_classops
};

void js_EvalOnExit(JSContext *cx, JSObject *obj, js_callback_t* cb)
{
	char*                    p;
	jsval                    rval;
	JSScript*                script;
	BOOL                     auto_terminate = cb->auto_terminate;
	JSObject *               glob = JS_GetGlobalObject(cx);
	global_private_t *       pt;
	str_list_t               list = NULL;
	struct js_onexit_scope **prev_oes_next = NULL;
	struct js_onexit_scope * oes = NULL;

	pt = (global_private_t *)JS_GetPrivate(cx, JS_GetGlobalObject(cx));
	if (glob == obj) {
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
		list = pt->exit_func;
	}
	else {
		/* Find this scope in onexit list */
		for (prev_oes_next = &pt->onexit, oes = pt->onexit; oes; prev_oes_next = &(oes->next), oes = oes->next) {
			if (oes->scope == obj) {
				(*prev_oes_next) = oes->next;
				list = oes->onexit;
				free(oes);
				break;
			}
		}
		if (oes == NULL)
			return;
	}

	cb->auto_terminate = FALSE;

	while ((p = strListPop(&list)) != NULL) {
		if ((script = JS_CompileScript(cx, obj, p, strlen(p), NULL, 0)) != NULL) {
			JS_ExecuteScript(cx, obj, script, &rval);
		}
		free(p);
	}

	strListFree(&list);
	if (glob == obj)
		pt->exit_func = NULL;

	if (auto_terminate)
		cb->auto_terminate = TRUE;
}

JSObject* js_CreateInternalJsObject(JSContext* cx, JSObject* parent, js_callback_t* cb, js_startup_t* startup)
{
	JS::RootedObject obj(cx);

	if (!(obj = JS_DefineObject(cx, parent, "js", &js_internal_class, NULL
	                           , JSPROP_ENUMERATE | JSPROP_READONLY)))
		return NULL;

	if (!JS_SetPrivate(cx, obj, cb)) /* Store a pointer to js_callback_t */
		return NULL;

	if (startup != NULL) {
		JS::RootedObject load_path_list(cx);
		jsval      val;
		str_list_t load_path;

		if (!(load_path_list = JS_NewArrayObject(cx, 0, NULL)))
			return NULL;
		val = OBJECT_TO_JSVAL(load_path_list.get());
		if (!JS_SetProperty(cx, obj, JAVASCRIPT_LOAD_PATH_LIST, &val))
			return NULL;

		if ((load_path = strListSplitCopy(NULL, startup->load_path, ",")) != NULL) {
			JS::RootedString js_str(cx);
			unsigned  i;

			for (i = 0; load_path[i] != NULL; i++) {
				if (!(js_str = JS_NewStringCopyZ(cx, load_path[i])))
					break;
				val = STRING_TO_JSVAL(js_str.get());
				if (!JS_SetElement(cx, load_path_list, i, &val))
					break;
			}
			strListFree(&load_path);
		}
	}

#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx, obj.get(), "JavaScript engine internal control object", 311);
	js_CreateArrayOfStrings(cx, obj, "_property_desc_list", prop_desc, JSPROP_READONLY);
#endif

	return obj;
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
	JS::RootedString str(cx);
	jsval     val;

	if (JS_GetProperty(cx, obj, "js", &val) && JSVAL_IS_OBJECT(val) && !JSVAL_IS_NULL(val)) {
		JS::RootedObject js(cx, JSVAL_TO_OBJECT(val));
		char      dir[MAX_PATH + 1];

		if (filename != NULL) {
			char* p;

			if ((str = JS_NewStringCopyZ(cx, filename)))
				JS_DefineProperty(cx, js, "exec_path", STRING_TO_JSVAL(str.get())
				                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
			if ((str = JS_NewStringCopyZ(cx, getfname(filename))))
				JS_DefineProperty(cx, js, "exec_file", STRING_TO_JSVAL(str.get())
				                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
			SAFECOPY(dir, filename);
			p = getfname(dir);
			*p = 0;
			if ((str = JS_NewStringCopyZ(cx, dir)))
				JS_DefineProperty(cx, js, "exec_dir", STRING_TO_JSVAL(str.get())
				                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
		}
		if (startup_dir == NULL)
			startup_dir = "";
		if ((str = JS_NewStringCopyZ(cx, startup_dir)))
			JS_DefineProperty(cx, js, "startup_dir", STRING_TO_JSVAL(str.get())
			                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
		JS_DefineProperty(cx, js, "scope", OBJECT_TO_JSVAL(scope)
		                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
	}
	JS_DefineProperty(cx, scope, "exit_code", JSVAL_VOID
	                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_PERMANENT);
#if defined(_MSC_VER)
	_set_invalid_parameter_handler(msvc_invalid_parameter_handler);
#endif
}

JSBool
js_CreateArrayOfStrings(JSContext* cx, JSObject* parent, const char* name, const char* str[], uintN flags)
{
	JS::RootedObject array(cx);
	JS::RootedString js_str(cx);
	jsval     val;
	size_t    i;
	jsuint    len = 0;

	if (JS_GetProperty(cx, parent, name, &val) && val != JSVAL_VOID)
		array = JSVAL_TO_OBJECT(val);
	else
	if (!(array = JS_NewArrayObject(cx, 0, NULL)))   /* Assertion here, in _heap_alloc_dbg, June-21-2004 */
		return JS_FALSE;                                  /* Caused by nntpservice.js? */

	if (!JS_DefineProperty(cx, parent, name, OBJECT_TO_JSVAL(array.get())
	                       , NULL, NULL, flags))
		return JS_FALSE;

	if (!array || !JS_GetArrayLength(cx, array, &len))
		return JS_FALSE;

	for (i = 0; str[i] != NULL; i++) {
		if (!(js_str = JS_NewStringCopyZ(cx, str[i])))
			break;
		val = STRING_TO_JSVAL(js_str.get());
		if (!JS_SetElement(cx, array, len + i, &val))
			break;
	}

	return JS_TRUE;
}

