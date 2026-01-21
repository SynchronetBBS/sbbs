/* Synchronet JavaScript "global" object properties/methods for all servers */

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
#include "md5.h"
#include "base64.h"
#include "htmlansi.h"
#include "ini_file.h"
#include "js_rtpool.h"
#include "js_request.h"
#include "wordwrap.h"
#include "utf8.h"

/* SpiderMonkey: */
#include <jsapi.h>
#if TODO_DEBUG
#include <jsdbgapi.h>
#endif

#define MAX_ANSI_SEQ    16
#define MAX_ANSI_PARAMS 8

#ifdef JAVASCRIPT

extern JSClass js_global_class;

/* Global Object Properties */
enum {
	GLOB_PROP_ERRNO
	, GLOB_PROP_ERRNO_STR
	, GLOB_PROP_SOCKET_ERRNO
	, GLOB_PROP_SOCKET_ERRNO_STR
};

bool js_argcIsInsufficient(JSContext *cx, uintN argc, uintN min)
{
	if (argc < min) {
		JS_ReportError(cx, "Insufficient Arguments (%u provided, a minimum of %u expected)", argc, min);
		return true;
	}
	return false;
}

bool js_argvIsNullOrVoid(JSContext* cx, jsval* args, uintN index)
{
	if (JSVAL_NULL_OR_VOID(args[index])) {
		JS_ReportError(cx, "Argument #%u is an unexpected 'null' or 'undefined' value", index + 1);
		return true;
	}
	return false;
}

static JSBool js_system_get(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
	jsval     idval;
	char      err[256];
	jsint     tiny;
	JSString* js_str;

	JS_IdToValue(cx, id, &idval);
	tiny = JSVAL_TO_INT(idval);

	switch (tiny) {
		case GLOB_PROP_SOCKET_ERRNO:
			*vp = DOUBLE_TO_JSVAL(SOCKET_ERRNO);
			break;
		case GLOB_PROP_SOCKET_ERRNO_STR:
			if ((js_str = JS_NewStringCopyZ(cx, SOCKET_STRERROR(err, sizeof(err)))) == NULL)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(js_str);
			break;
		case GLOB_PROP_ERRNO:
			*vp = INT_TO_JSVAL(errno);
			break;
		case GLOB_PROP_ERRNO_STR:
			if ((js_str = JS_NewStringCopyZ(cx, strerror(errno))) == NULL)
				return JS_FALSE;
			*vp = STRING_TO_JSVAL(js_str);
			break;
	}
	return JS_TRUE;
}

#define GLOBOBJ_FLAGS JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_SHARED

static jsSyncPropertySpec js_global_properties[] = {
/*		 name,				tinyid,						flags,			ver */

	{   "errno", GLOB_PROP_ERRNO, GLOBOBJ_FLAGS, 310 },
	{   "errno_str", GLOB_PROP_ERRNO_STR, GLOBOBJ_FLAGS, 310 },
	{   "socket_errno", GLOB_PROP_SOCKET_ERRNO, GLOBOBJ_FLAGS, 310 },
	{   "socket_errno_str", GLOB_PROP_SOCKET_ERRNO_STR, GLOBOBJ_FLAGS, 31800 },
	{0}
};

typedef struct {
	JSRuntime* runtime;
	JSContext* cx;
	JSContext* parent_cx;
	JSObject* obj;
	JSObject* script;
	msg_queue_t* msg_queue;
	js_callback_t cb;
	JSErrorReporter error_reporter;
	JSObject* logobj;
	sem_t *sem;
} background_data_t;

static void background_thread(void* arg)
{
	background_data_t* bg = (background_data_t*)arg;
	jsval              result = JSVAL_VOID;
	jsval              exit_code;

	SetThreadName("sbbs/jsBackgrnd");
	msgQueueAttach(bg->msg_queue);
	JS_SetContextThread(bg->cx);
	JS_BEGINREQUEST(bg->cx);
	if (!JS_ExecuteScript(bg->cx, bg->obj, bg->script, &result)
	    && JS_GetProperty(bg->cx, bg->obj, "exit_code", &exit_code))
		result = exit_code;
	js_EvalOnExit(bg->cx, bg->obj, &bg->cb);
	js_enqueue_value(bg->cx, bg->msg_queue, result, NULL);
	JS_RemoveObjectRoot(bg->cx, &bg->logobj);
	JS_RemoveObjectRoot(bg->cx, &bg->obj);
	JS_ENDREQUEST(bg->cx);
	JS_DestroyContext(bg->cx);  /* exception here (Feb-5-2012):
	mozjs185-1.0.dll!66f3cede()
	[Frames below may be incorrect and/or missing, no symbols loaded for mozjs185-1.0.dll]
	mozjs185-1.0.dll!66f3d49d()
	mozjs185-1.0.dll!66ee010f()
	mozjs185-1.0.dll!66ebb0c9()
	>	sbbs.dll!background_thread(void * arg=0x0167f050)  Line 146 + 0xf bytes	C
	sbbs.dll!_callthreadstart()  Line 259 + 0xf bytes	C
	sbbs.dll!_threadstart(void * ptd=0x016a0590)  Line 243	C
	*/

	jsrt_Release(bg->runtime);
	sem_post(bg->sem);
	free(bg);
}

static void
js_ErrorReporter(JSContext *cx, const char *message, JSErrorReport *report)
{
	background_data_t* bg;

	if ((bg = (background_data_t*)JS_GetContextPrivate(cx)) == NULL)
		return;

	/* Use parent's context private data */
	JS_SetContextPrivate(cx, JS_GetContextPrivate(bg->parent_cx));

	/* Call parent's error reporter */
	bg->error_reporter(cx, message, report);

	/* Restore our context private data */
	JS_SetContextPrivate(cx, bg);
}

static JSBool
js_OperationCallback(JSContext *cx)
{
	background_data_t* bg;
	JSBool             ret;

	JS_SetOperationCallback(cx, NULL);

	if ((bg = (background_data_t*)JS_GetContextPrivate(cx)) == NULL) {
		JS_SetOperationCallback(cx, js_OperationCallback);
		return JS_FALSE;
	}

	if (bg->parent_cx != NULL && !JS_IsRunning(bg->parent_cx)) {   /* die when parent dies */
		JS_SetOperationCallback(cx, js_OperationCallback);
		return JS_FALSE;
	}

	ret = js_CommonOperationCallback(cx, &bg->cb);
	JS_SetOperationCallback(cx, js_OperationCallback);
	return ret;
}

// Background log()
static JSBool
js_log(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *            argv = JS_ARGV(cx, arglist);
	JSBool             retval;
	background_data_t* bg;
	jsval              rval;

	if ((bg = (background_data_t*)JS_GetContextPrivate(cx)) == NULL)
		return JS_FALSE;

	/* Use parent's context private data */
	JS_SetContextPrivate(cx, JS_GetContextPrivate(bg->parent_cx));

	/* Call parent's log() function */
	retval = JS_CallFunctionName(cx, bg->logobj, "log", argc, argv, &rval);

	/* Restore our context private data */
	JS_SetContextPrivate(cx, bg);

	return retval;
}

/* Create a new value in the new context with a value from the original context */
/* BOTH CONTEXTX NEED TO BE SUSPENDED! */
static jsval* js_CopyValue(JSContext* cx, jsrefcount *cx_rc, jsval val, JSContext* new_cx, jsrefcount *new_rc, jsval* rval)
{
	size_t  size;
	uint64 *nval;

	*rval = JSVAL_VOID;
	JS_RESUMEREQUEST(cx, *cx_rc);
	if (JS_WriteStructuredClone(cx, val, &nval, &size, NULL, NULL)) {
		*cx_rc = JS_SUSPENDREQUEST(cx);
		JS_RESUMEREQUEST(new_cx, *new_rc);
		JS_ReadStructuredClone(new_cx, nval, size, JS_STRUCTURED_CLONE_VERSION, rval, NULL, NULL);
		*new_rc = JS_SUSPENDREQUEST(new_cx);
		JS_RESUMEREQUEST(cx, *cx_rc);
		JS_free(cx, nval);
	}
	*cx_rc = JS_SUSPENDREQUEST(cx);

	return rval;
}

static JSBool
js_load(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *         obj = JS_THIS_OBJECT(cx, arglist);
	jsval *            argv = JS_ARGV(cx, arglist);
	char               path[MAX_PATH + 1];
	uintN              i;
	uintN              argn = 0;
	char*              filename;
	JSObject*          script;
	global_private_t*  p;
	jsval              val;
	JSObject*          js_argv;
	jsval              old_js_argv = JSVAL_VOID;
	jsval              old_js_argc = JSVAL_VOID;
	BOOL               restore_args = FALSE;
	JSObject*          exec_obj;
	JSObject*          js_internal;
	JSContext*         exec_cx = cx;
	JSBool             success;
	JSBool             background = JS_FALSE;
	background_data_t* bg = NULL;
	jsrefcount         rc;
	jsrefcount         brc;
	size_t             len;
	JSObject*          scope = JS_GetScopeChain(cx);

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;

	if ((p = (global_private_t*)js_GetClassPrivate(cx, obj, &js_global_class)) == NULL)
		return JS_FALSE;

	exec_obj = scope;

	if (JSVAL_IS_BOOLEAN(argv[argn]))
		background = JSVAL_TO_BOOLEAN(argv[argn++]);

	if (background) {
		rc = JS_SUSPENDREQUEST(cx);

		if ((bg = (background_data_t*)malloc(sizeof(background_data_t))) == NULL) {
			JS_RESUMEREQUEST(cx, rc);
			return JS_FALSE;
		}
		memset(bg, 0, sizeof(background_data_t));

		bg->parent_cx = cx;

		/* Setup default values for callback settings */
		bg->cb.limit = p->startup->time_limit;
		bg->cb.gc_interval = p->startup->gc_interval;
		bg->cb.yield_interval = p->startup->yield_interval;
		bg->cb.terminated = NULL; /* could be bad pointer at any time */
		bg->cb.counter = 0;
		bg->cb.gc_attempts = 0;
		bg->cb.bg = TRUE;

		// Get the js.internal private data since it's the parents js_callback_t...
		JS_RESUMEREQUEST(cx, rc);
		if ((JS_GetProperty(cx, scope, "js", &val) && !JSVAL_NULL_OR_VOID(val))
		    || (JS_GetProperty(cx, obj, "js", &val) && !JSVAL_NULL_OR_VOID(val))) {
			js_internal = JSVAL_TO_OBJECT(val);
			if (js_internal != NULL) {
				bg->cb.parent_cb = (js_callback_t*)JS_GetPrivate(cx, js_internal);
				if (bg->cb.parent_cb == NULL) {
					lprintf(LOG_ERR, "!ERROR parent CB is NULL");
				}
			}
		}
		else {
			lprintf(LOG_ERR, "!ERROR unable to locate global js object");
		}
		rc = JS_SUSPENDREQUEST(cx);

		if ((bg->runtime = jsrt_GetNew(JAVASCRIPT_MAX_BYTES, 1000, __FILE__, __LINE__)) == NULL) {
			free(bg);
			JS_RESUMEREQUEST(cx, rc);
			return JS_FALSE;
		}

		if ((bg->cx = JS_NewContext(bg->runtime, JAVASCRIPT_CONTEXT_STACK)) == NULL) {
			jsrt_Release(bg->runtime);
			free(bg);
			JS_RESUMEREQUEST(cx, rc);
			return JS_FALSE;
		}
		JS_SetOptions(bg->cx, p->startup->options);
		JS_BEGINREQUEST(bg->cx);

		if (!js_CreateCommonObjects(bg->cx
		                            , p->cfg /* common config */
		                            , NULL /* node-specific config */
		                            , p->methods /* additional global methods */
		                            , 0 /* uptime */
		                            , "" /* hostname */
		                            , "" /* socklib_desc */
		                            , &bg->cb /* js */
		                            , p->startup /* js */
			                        , NULL /* user */
		                            , NULL /* client */
		                            , INVALID_SOCKET /* client_socket */
		                            , -1 /* client TLS session */
		                            , NULL /* server props */
		                            , &bg->obj
		                            , (struct mqtt*)NULL
		                            )) {
			JS_ENDREQUEST(bg->cx);
			JS_DestroyContext(bg->cx);
			jsrt_Release(bg->runtime);
			free(bg);
			JS_RESUMEREQUEST(cx, rc);
			return JS_FALSE;
		}

		if ((bg->logobj = JS_NewObjectWithGivenProto(bg->cx, NULL, NULL, bg->obj)) == NULL) {
			JS_RemoveObjectRoot(bg->cx, &bg->obj);
			JS_ENDREQUEST(bg->cx);
			JS_DestroyContext(bg->cx);
			jsrt_Release(bg->runtime);
			free(bg);
			JS_RESUMEREQUEST(cx, rc);
			return JS_FALSE;
		}
		JS_AddObjectRoot(bg->cx, &bg->logobj);

		bg->msg_queue = msgQueueInit(NULL, MSG_QUEUE_BIDIR);

		js_CreateQueueObject(bg->cx, bg->obj, "parent_queue", bg->msg_queue);

		/* Save parent's error reporter (for later use by our error reporter) */
		brc = JS_SUSPENDREQUEST(bg->cx);
		JS_RESUMEREQUEST(cx, rc);
		bg->error_reporter = JS_SetErrorReporter(cx, NULL);
		JS_SetErrorReporter(cx, bg->error_reporter);
		rc = JS_SUSPENDREQUEST(cx);
		JS_RESUMEREQUEST(bg->cx, brc);
		JS_SetErrorReporter(bg->cx, js_ErrorReporter);

		/* Set our Operation callback (which calls the generic branch callback) */
		JS_SetContextPrivate(bg->cx, bg);
		JS_SetOperationCallback(bg->cx, js_OperationCallback);

		/* Save parent's 'log' function (for later use by our log function) */
		brc = JS_SUSPENDREQUEST(bg->cx);
		JS_RESUMEREQUEST(cx, rc);
		if (JS_GetProperty(cx, scope, "log", &val) || JS_GetProperty(cx, obj, "log", &val)) {
			JSFunction* func;
			if ((func = JS_ValueToFunction(cx, val)) != NULL) {
				JSObject *obj;

				rc = JS_SUSPENDREQUEST(cx);
				JS_RESUMEREQUEST(bg->cx, brc);
				obj = JS_CloneFunctionObject(bg->cx, JS_GetFunctionObject(func), bg->logobj);
				JS_DefineProperty(bg->cx, bg->logobj, "log", OBJECT_TO_JSVAL(obj), NULL, NULL, JSPROP_ENUMERATE | JSPROP_PERMANENT);
				JS_DefineFunction(bg->cx, bg->obj, "log", js_log, JS_GetFunctionArity(func), JS_GetFunctionFlags(func));
				brc = JS_SUSPENDREQUEST(bg->cx);
				JS_RESUMEREQUEST(cx, rc);
			}
		}

		// These js_Create*Object() functions use GetContextPrivate() for the sbbs_t.
		rc = JS_SUSPENDREQUEST(cx);
		JS_RESUMEREQUEST(bg->cx, brc);
		JS_SetContextPrivate(bg->cx, JS_GetContextPrivate(bg->parent_cx));
		brc = JS_SUSPENDREQUEST(bg->cx);
		JS_RESUMEREQUEST(cx, rc);
		if (JS_HasProperty(cx, obj, "bbs", &success) && success) {
			rc = JS_SUSPENDREQUEST(cx);
			JS_RESUMEREQUEST(bg->cx, brc);
			js_CreateBbsObject(bg->cx, bg->obj);
			brc = JS_SUSPENDREQUEST(bg->cx);
			JS_RESUMEREQUEST(cx, rc);
			brc = JS_SUSPENDREQUEST(bg->cx);
			JS_RESUMEREQUEST(cx, rc);
		}
		if (JS_HasProperty(cx, obj, "console", &success) && success) {
			rc = JS_SUSPENDREQUEST(cx);
			JS_RESUMEREQUEST(bg->cx, brc);
			js_CreateConsoleObject(bg->cx, bg->obj);
			brc = JS_SUSPENDREQUEST(bg->cx);
			JS_RESUMEREQUEST(cx, rc);
		}
		if (JS_HasProperty(cx, obj, "stdin", &success) && success) {
			rc = JS_SUSPENDREQUEST(cx);
			JS_RESUMEREQUEST(bg->cx, brc);
			js_CreateFileObject(bg->cx, bg->obj, "stdin", STDIN_FILENO, "r");
			brc = JS_SUSPENDREQUEST(bg->cx);
			JS_RESUMEREQUEST(cx, rc);
		}
		if (JS_HasProperty(cx, obj, "stdout", &success) && success) {
			rc = JS_SUSPENDREQUEST(cx);
			JS_RESUMEREQUEST(bg->cx, brc);
			js_CreateFileObject(bg->cx, bg->obj, "stdout", STDOUT_FILENO, "w");
			brc = JS_SUSPENDREQUEST(bg->cx);
			JS_RESUMEREQUEST(cx, rc);
		}
		if (JS_HasProperty(cx, obj, "stderr", &success) && success) {
			rc = JS_SUSPENDREQUEST(cx);
			JS_RESUMEREQUEST(bg->cx, brc);
			js_CreateFileObject(bg->cx, bg->obj, "stderr", STDERR_FILENO, "w");
			brc = JS_SUSPENDREQUEST(bg->cx);
			JS_RESUMEREQUEST(cx, rc);
		}
		rc = JS_SUSPENDREQUEST(cx);
		JS_RESUMEREQUEST(bg->cx, brc);
		JS_SetContextPrivate(bg->cx, bg);
		brc = JS_SUSPENDREQUEST(bg->cx);
		JS_RESUMEREQUEST(cx, rc);

		exec_cx = bg->cx;
		exec_obj = bg->obj;

	} else if (JSVAL_IS_OBJECT(argv[argn])) {
		JSObject* tmp_obj = JSVAL_TO_OBJECT(argv[argn++]);
		if (tmp_obj != NULL && !JS_ObjectIsFunction(cx, tmp_obj)) /* Scope specified */
			exec_obj = tmp_obj;
	}

	if (argn == argc) {
		JS_ReportError(cx, "no filename specified");
		if (background) {
			rc = JS_SUSPENDREQUEST(cx);
			JS_RESUMEREQUEST(bg->cx, brc);
			JS_RemoveObjectRoot(bg->cx, &bg->obj);
			JS_ENDREQUEST(bg->cx);
			JS_DestroyContext(bg->cx);
			jsrt_Release(bg->runtime);
			JS_RESUMEREQUEST(cx, rc);
			free(bg);
		}
		return JS_FALSE;
	}
	JSVALUE_TO_MSTRING(cx, argv[argn], filename, NULL);
	argn++;
	if (filename == NULL) {    // This handles the pending error as well as a null JS string.
		if (background) {
			rc = JS_SUSPENDREQUEST(cx);
			JS_RESUMEREQUEST(bg->cx, brc);
			JS_RemoveObjectRoot(bg->cx, &bg->obj);
			JS_ENDREQUEST(bg->cx);
			JS_DestroyContext(bg->cx);
			jsrt_Release(bg->runtime);
			JS_RESUMEREQUEST(cx, rc);
			free(bg);
		}
		return JS_FALSE;
	}

	if (argc > argn || background) {

		if (background) {
			rc = JS_SUSPENDREQUEST(cx);
			JS_RESUMEREQUEST(bg->cx, brc);
		}
		else {
			if (JS_GetProperty(exec_cx, exec_obj, "argv", &old_js_argv))
				JS_AddValueRoot(exec_cx, &old_js_argv);
			if (JS_GetProperty(exec_cx, exec_obj, "argc", &old_js_argc))
				JS_AddValueRoot(exec_cx, &old_js_argc);
			restore_args = TRUE;
		}

		if ((js_argv = JS_NewArrayObject(exec_cx, 0, NULL)) == NULL) {
			if (background) {
				JS_RemoveObjectRoot(bg->cx, &bg->obj);
				JS_ENDREQUEST(bg->cx);
				JS_DestroyContext(bg->cx);
				jsrt_Release(bg->runtime);
				free(bg);
			}
			free(filename);
			return JS_FALSE;
		}

		JS_DefineProperty(exec_cx, exec_obj, "argv", OBJECT_TO_JSVAL(js_argv)
		                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);

		for (i = argn; i < argc; i++) {
			jsval *copy;
			if (background) {
				brc = JS_SUSPENDREQUEST(bg->cx);
				copy = js_CopyValue(cx, &rc, argv[i], exec_cx, &brc, &val);
				JS_RESUMEREQUEST(bg->cx, brc);
			}
			else {
				rc = JS_SUSPENDREQUEST(cx);
				copy = js_CopyValue(cx, &rc, argv[i], exec_cx, &rc, &val);
				JS_RESUMEREQUEST(cx, rc);
			}
			JS_SetElement(exec_cx, js_argv, i - argn, copy);
		}

		JS_DefineProperty(exec_cx, exec_obj, "argc", INT_TO_JSVAL(argc - argn)
		                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);

		if (background) {
			brc = JS_SUSPENDREQUEST(bg->cx);
			JS_RESUMEREQUEST(cx, rc);
		}
	}

	rc = JS_SUSPENDREQUEST(cx);
	errno = 0;
	if (isfullpath(filename))
		SAFECOPY(path, filename);
	else {
		JSObject* js_load_list = NULL;

		path[0] = 0;  /* Empty path, indicates load file not found (yet) */

		JS_RESUMEREQUEST(cx, rc);
		if ((JS_GetProperty(cx, scope, "js", &val) && val != JSVAL_VOID && JSVAL_IS_OBJECT(val))
		    || (JS_GetProperty(cx, obj, "js", &val) && val != JSVAL_VOID && JSVAL_IS_OBJECT(val))) {
			JSObject* js_obj = JSVAL_TO_OBJECT(val);

			if (js_obj != NULL) {
				/* if js.exec_dir is defined (location of executed script), search there first */
				if (JS_GetProperty(cx, js_obj, "exec_dir", &val) && val != JSVAL_VOID && JSVAL_IS_STRING(val)) {
					JSVALUE_TO_STRBUF(cx, val, path, sizeof(path), &len);
					strncat(path, filename, sizeof(path) - len);
					path[sizeof(path) - 1] = 0;
					rc = JS_SUSPENDREQUEST(cx);
					if (!fexistcase(path))
						path[0] = 0;
					JS_RESUMEREQUEST(cx, rc);
				}
				if (JS_GetProperty(cx, js_obj, JAVASCRIPT_LOAD_PATH_LIST, &val) && val != JSVAL_VOID && JSVAL_IS_OBJECT(val))
					js_load_list = JSVAL_TO_OBJECT(val);
			}

			/* if mods_dir is defined, search mods/js.load_path_list[n] next */
			if (path[0] == 0 && p->cfg->mods_dir[0] != 0 && js_load_list != NULL) {
				jsuint i;
				char   prefix[MAX_PATH + 1];

				for (i = 0; path[0] == 0; i++) {
					if (!JS_GetElement(cx, js_load_list, i, &val) || val == JSVAL_VOID)
						break;
					JSVALUE_TO_STRBUF(cx, val, prefix, sizeof(prefix), NULL);
					if (prefix[0] == 0)
						continue;
					backslash(prefix);
					rc = JS_SUSPENDREQUEST(cx);
					if (isfullpath(prefix)) {
						SAFEPRINTF2(path, "%s%s", prefix, filename);
						if (!fexistcase(path))
							path[0] = 0;
					} else {
						/* relative path */
						SAFEPRINTF3(path, "%s%s%s", p->cfg->mods_dir, prefix, filename);
						if (!fexistcase(path))
							path[0] = 0;
					}
					JS_RESUMEREQUEST(cx, rc);
				}
			}
		}
		rc = JS_SUSPENDREQUEST(cx);
		/* if mods_dir is defined, search there next */
		if (path[0] == 0 && p->cfg->mods_dir[0] != 0) {
			SAFEPRINTF2(path, "%s%s", p->cfg->mods_dir, filename);
			if (!fexistcase(path))
				path[0] = 0;
		}
		/* if js.load_path_list is defined, search exec/load_path_list[n] next */
		if (path[0] == 0 && js_load_list != NULL) {
			jsuint i;
			char   prefix[MAX_PATH + 1];

			for (i = 0; path[0] == 0; i++) {
				JS_RESUMEREQUEST(cx, rc);
				if (!JS_GetElement(cx, js_load_list, i, &val) || val == JSVAL_VOID) {
					rc = JS_SUSPENDREQUEST(cx);
					break;
				}
				JSVALUE_TO_STRBUF(cx, val, prefix, sizeof(prefix), NULL);
				rc = JS_SUSPENDREQUEST(cx);
				if (prefix[0] == 0)
					continue;
				backslash(prefix);
				if (isfullpath(prefix)) {
					SAFEPRINTF2(path, "%s%s", prefix, filename);
					if (!fexistcase(path))
						path[0] = 0;
				} else {
					/* relative path */
					SAFEPRINTF3(path, "%s%s%s", p->cfg->exec_dir, prefix, filename);
					if (!fexistcase(path))
						path[0] = 0;
				}
			}
		}
		/* lastly, search exec dir */
		if (path[0] == 0)
			SAFEPRINTF2(path, "%s%s", p->cfg->exec_dir, filename);

		if (!fexistcase(path)) {
			SAFECOPY(path, filename);
		}
	}
	free(filename);

	if (background)
		JS_RESUMEREQUEST(bg->cx, brc);
	else
		JS_RESUMEREQUEST(cx, rc);

	JS_ClearPendingException(exec_cx);

	if ((script = JS_CompileFile(exec_cx, exec_obj, path)) == NULL) {
		if (background) {
			JS_ENDREQUEST(bg->cx);
			JS_RemoveObjectRoot(bg->cx, &bg->obj);
			JS_DestroyContext(bg->cx);
			jsrt_Release(bg->runtime);
			free(bg);
			JS_RESUMEREQUEST(cx, rc);
		}
		// Restore args
		if (restore_args) {
			if (old_js_argv == JSVAL_VOID) {
				JS_DeleteProperty(exec_cx, exec_obj, "argv");
				JS_DeleteProperty(exec_cx, exec_obj, "argc");
			}
			else {
				JS_DefineProperty(exec_cx, exec_obj, "argv", old_js_argv
				                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
				JS_DefineProperty(exec_cx, exec_obj, "argc", old_js_argc
				                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
			}
		}
		JS_RemoveValueRoot(exec_cx, &old_js_argv);
		JS_RemoveValueRoot(exec_cx, &old_js_argc);
		return JS_FALSE;
	}

	if (background) {

		bg->script = script;
		brc = JS_SUSPENDREQUEST(bg->cx);
		JS_RESUMEREQUEST(cx, rc);
		JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(js_CreateQueueObject(cx, obj, NULL, bg->msg_queue)));
		rc = JS_SUSPENDREQUEST(cx);
		JS_RESUMEREQUEST(bg->cx, brc);
		js_PrepareToExecute(bg->cx, bg->obj, path, NULL, bg->obj);
		JS_ENDREQUEST(bg->cx);
		JS_ClearContextThread(bg->cx);
		bg->sem = &p->bg_sem;
//		lprintf(LOG_DEBUG, "JavaScript Background Load: %s", path); // non-contextual (always logs to terminal server)
		success = _beginthread(background_thread, 0, bg) != static_cast<uintptr_t>(-1);
		JS_RESUMEREQUEST(cx, rc);
		if (success) {
			p->bg_count++;
		}

	} else {
		jsval rval;

		success = JS_ExecuteScript(exec_cx, exec_obj, script, &rval);
		JS_SET_RVAL(cx, arglist, rval);
		if (restore_args) {
			if (old_js_argv == JSVAL_VOID) {
				JS_DeleteProperty(exec_cx, exec_obj, "argv");
				JS_DeleteProperty(exec_cx, exec_obj, "argc");
			}
			else {
				JS_DefineProperty(exec_cx, exec_obj, "argv", old_js_argv
				                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
				JS_DefineProperty(exec_cx, exec_obj, "argc", old_js_argc
				                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);
			}
			JS_RemoveValueRoot(exec_cx, &old_js_argv);
			JS_RemoveValueRoot(exec_cx, &old_js_argc);
		}
	}

	return success;
}

/*
 * This is hacky, but a but less hacky than using a magic '2'
 * It does assume the args are always last though (which seems reasonable
 * since it's variable length)
 */
#define JS_ARGS_OFFSET  ((unsigned long)(JS_ARGV(0, (jsval *)NULL)) / sizeof(jsval *))

static JSBool
js_require(JSContext *cx, uintN argc, jsval *arglist)
{
	uintN     argn = 0;
	uintN     fnarg;
	JSObject* exec_obj;
	JSObject* tmp_obj;
	char*     property;
	char*     filename = NULL;
	JSBool    found = JS_FALSE;
	JSBool    ret;
	jsval *   argv = JS_ARGV(cx, arglist);

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	exec_obj = JS_GetScopeChain(cx);
	if (JSVAL_IS_BOOLEAN(argv[argn])) {
		JS_ReportError(cx, "cannot require() background processes");
		return JS_FALSE;
	}

	if (JSVAL_IS_OBJECT(argv[argn])) {
		tmp_obj = JSVAL_TO_OBJECT(argv[argn++]);
		if (tmp_obj != NULL && !JS_ObjectIsFunction(cx, tmp_obj)) /* Scope specified */
			exec_obj = tmp_obj;
	}

	// Skip filename
	fnarg = argn++;

	if (argn == argc) {
		JS_ReportError(cx, "no symbol name specified");
		return JS_FALSE;
	}
	JSVALUE_TO_MSTRING(cx, argv[argn], property, NULL);

	// TODO: Does this support sub-objects?
	if (JS_HasProperty(cx, exec_obj, property, &found) && found) {
		JS_SET_RVAL(cx, arglist, JSVAL_VOID);
		free(property);
		return JS_TRUE;
	}

	// Remove symbol name from args
	if (argc > argn + 1)
		memmove(&arglist[argn + JS_ARGS_OFFSET], &arglist[argn + JS_ARGS_OFFSET + 1], sizeof(arglist[0]) * (argc - argn - 1));

	ret = js_load(cx, argc - 1, arglist);

	if (!JS_IsExceptionPending(cx)) {
		if (!JS_HasProperty(cx, exec_obj, property, &found) || !found) {
			if (js_IsTerminated(cx, exec_obj)) {
				JSVALUE_TO_MSTRING(cx, argv[fnarg], filename, NULL);
				JS_ReportError(cx, "symbol '%s' not defined by script '%s'", property, filename);
			}
			free(filename);
			free(property);
			return JS_FALSE;
		}
	}
	free(property);
	return ret;
}

static JSBool
js_format(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *   argv = JS_ARGV(cx, arglist);
	char*     p;
	JSString* str;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	if ((p = js_sprintf(cx, 0, argc, argv)) == NULL) {
		JS_ReportError(cx, "js_sprintf failed");
		return JS_FALSE;
	}

	str = JS_NewStringCopyZ(cx, p);
	js_sprintf_free(p);

	if (str == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(str));
	return JS_TRUE;
}

static JSBool
js_yield(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	BOOL       forced = TRUE;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if (argc) {
		if (js_argvIsNullOrVoid(cx, argv, 0))
			return JS_FALSE;
		JS_ValueToBoolean(cx, argv[0], &forced);
	}
	rc = JS_SUSPENDREQUEST(cx);
	if (forced) {
		YIELD();
	} else {
		MAYBE_YIELD();
	}
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}

static JSBool
js_mswait(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	int32      val = 1;
	double     start = xp_timer();
	jsrefcount rc;

	if (argc) {
		if (js_argvIsNullOrVoid(cx, argv, 0))
			return JS_FALSE;
		if (!JS_ValueToInt32(cx, argv[0], &val))
			return JS_FALSE;
	}
	rc = JS_SUSPENDREQUEST(cx);
	mswait(val);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, DOUBLE_TO_JSVAL((xp_timer() - start) * 1000));

	return JS_TRUE;
}

static JSBool
js_random(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv = JS_ARGV(cx, arglist);
	int32  val = 100;

	if (argc) {
		if (!JS_ValueToInt32(cx, argv[0], &val))
			return JS_FALSE;
	}

	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(sbbs_random(val)));
	return JS_TRUE;
}

static JSBool
js_time(JSContext *cx, uintN argc, jsval *arglist)
{
	JS_SET_RVAL(cx, arglist, UINT_TO_JSVAL((uint32_t)time(NULL)));
	return JS_TRUE;
}


static JSBool
js_beep(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	int32      freq = 500;
	int32      dur = 500;
	jsrefcount rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if (argc) {
		if (js_argvIsNullOrVoid(cx, argv, 0))
			return JS_FALSE;
		if (!JS_ValueToInt32(cx, argv[0], &freq))
			return JS_FALSE;
	}
	if (argc > 1) {
		if (js_argvIsNullOrVoid(cx, argv, 1))
			return JS_FALSE;
		if (!JS_ValueToInt32(cx, argv[1], &dur))
			return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	sbbs_beep(freq, dur);
	JS_RESUMEREQUEST(cx, rc);
	return JS_TRUE;
}

static JSBool
js_exit(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *scope = JS_GetScopeChain(cx);
	JSObject *obj = JS_THIS_OBJECT(cx, arglist);
	jsval *   argv = JS_ARGV(cx, arglist);
	jsval     val;
	int       exit_code = 0;

	if (argc && JSVAL_IS_NUMBER(argv[0]))
		exit_code = JSVAL_TO_INT(argv[0]);
	if ((JS_GetProperty(cx, scope, "js", &val) && JSVAL_IS_OBJECT(val)) ||
	    (JS_GetProperty(cx, obj, "js", &val) && JSVAL_IS_OBJECT(val))) {
		obj = JSVAL_TO_OBJECT(val);
		if (obj != NULL && JS_GetProperty(cx, obj, "scope", &val) && JSVAL_IS_OBJECT(val) && !JSVAL_IS_NULL(val))
			obj = JSVAL_TO_OBJECT(val);
		else
			obj = JS_THIS_OBJECT(cx, arglist);
	}
	JS_DefineProperty(cx, obj, "exit_code", INT_TO_JSVAL(exit_code)
	                  , NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY);

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	return JS_FALSE;
}

static JSBool
js_crc16(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      p = NULL;
	size_t     len;
	jsrefcount rc;
	uint16_t   cs;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], p, &len);
	HANDLE_PENDING(cx, p);
	if (p == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	cs = crc16(p, len);
	free(p);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(cs));
	return JS_TRUE;
}

static JSBool
js_crc32(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      p = NULL;
	size_t     len;
	uint32_t   cs;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], p, &len);
	HANDLE_PENDING(cx, p);
	if (p == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	cs = crc32(p, len);
	free(p);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, UINT_TO_JSVAL(cs));
	return JS_TRUE;
}

static JSBool
js_chksum(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	ulong      sum = 0;
	char*      p = NULL;
	char*      sp;
	size_t     len;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], p, &len);
	HANDLE_PENDING(cx, p);
	if (p == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);   /* 3.8 seconds on Deuce's computer when len==UINT_MAX/8 */
	sp = p;
	while (len--) sum += *(sp++);
	free(p);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, DOUBLE_TO_JSVAL((double)sum));
	return JS_TRUE;
}

static JSBool
js_ascii(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *       argv = JS_ARGV(cx, arglist);
	char          str[2];
	int32         i = 0;
	JSString*     js_str;
	const jschar *p;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;

	if (JSVAL_NULL_OR_VOID(argv[0])) {
		JS_SET_RVAL(cx, arglist, argv[0]);
		return JS_TRUE;
	}

	if (JSVAL_IS_STRING(argv[0])) {  /* string to ascii-int */
		p = JS_GetStringCharsZ(cx, JSVAL_TO_STRING(argv[0]));
		if (p == NULL)
			JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(0));
		else
			JS_SET_RVAL(cx, arglist, INT_TO_JSVAL((uchar) * p));
		return JS_TRUE;
	}

	/* ascii-int to str */
	if (!JS_ValueToInt32(cx, argv[0], &i))
		return JS_FALSE;
	str[0] = (uchar)i;
	str[1] = 0;

	if ((js_str = JS_NewStringCopyN(cx, str, 1)) == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}

static JSBool
js_ctrl(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *       argv = JS_ARGV(cx, arglist);
	char          ch;
	char          str[2];
	int32         i = 0;
	JSString*     js_str;
	const jschar *p;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	if (JSVAL_IS_STRING(argv[0])) {
		p = JS_GetStringCharsZ(cx, JSVAL_TO_STRING(argv[0]));
		if (p == NULL) {
			JS_ReportError(cx, "Invalid NULL string");
			return JS_FALSE;
		}
		ch = (uchar) * p;
	} else {
		if (!JS_ValueToInt32(cx, argv[0], &i))
			return JS_FALSE;
		ch = (char)i;
	}

	str[0] = toupper(ch) & ~0x40;
	str[1] = 0;

	if ((js_str = JS_NewStringCopyZ(cx, str)) == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}

static JSBool
js_ascii_str(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      buf = NULL;
	JSString*  str;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (JSVAL_NULL_OR_VOID(argv[0])) {
		JS_SET_RVAL(cx, arglist, argv[0]);
		return JS_TRUE;
	}
	JSVALUE_TO_MSTRING(cx, argv[0], buf, NULL)
	HANDLE_PENDING(cx, buf);
	if (buf == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	ascii_str((uchar*)buf);
	JS_RESUMEREQUEST(cx, rc);

	str = JS_NewStringCopyZ(cx, buf);
	free(buf);
	if (str == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(str));
	return JS_TRUE;
}


static JSBool
js_strip_ctrl(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      buf = NULL;
	JSString*  js_str;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (JSVAL_NULL_OR_VOID(argv[0])) {
		JS_SET_RVAL(cx, arglist, argv[0]);
		return JS_TRUE;
	}
	JSVALUE_TO_MSTRING(cx, argv[0], buf, NULL);
	HANDLE_PENDING(cx, buf);
	if (buf == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	strip_ctrl(buf, buf);
	JS_RESUMEREQUEST(cx, rc);

	js_str = JS_NewStringCopyZ(cx, buf);
	free(buf);
	if (js_str == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}

static JSBool
js_strip_ctrl_a(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      buf = NULL;
	JSString*  js_str;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (JSVAL_NULL_OR_VOID(argv[0])) {
		JS_SET_RVAL(cx, arglist, argv[0]);
		return JS_TRUE;
	}
	JSVALUE_TO_MSTRING(cx, argv[0], buf, NULL);
	HANDLE_PENDING(cx, buf);
	if (buf == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	remove_ctrl_a(buf, buf);
	JS_RESUMEREQUEST(cx, rc);

	js_str = JS_NewStringCopyZ(cx, buf);
	free(buf);
	if (js_str == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}

static JSBool
js_strip_ansi(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      buf = NULL;
	JSString*  js_str;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (JSVAL_NULL_OR_VOID(argv[0])) {
		JS_SET_RVAL(cx, arglist, argv[0]);
		return JS_TRUE;
	}
	JSVALUE_TO_MSTRING(cx, argv[0], buf, NULL)
	HANDLE_PENDING(cx, buf);
	if (buf == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	strip_ansi(buf);
	JS_RESUMEREQUEST(cx, rc);

	js_str = JS_NewStringCopyZ(cx, buf);
	free(buf);
	if (js_str == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}

static JSBool
js_strip_exascii(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      buf = NULL;
	JSString*  js_str;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (JSVAL_NULL_OR_VOID(argv[0])) {
		JS_SET_RVAL(cx, arglist, argv[0]);
		return JS_TRUE;
	}
	JSVALUE_TO_MSTRING(cx, argv[0], buf, NULL)
	HANDLE_PENDING(cx, buf);
	if (buf == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	strip_exascii(buf, buf);
	JS_RESUMEREQUEST(cx, rc);

	js_str = JS_NewStringCopyZ(cx, buf);
	free(buf);
	if (js_str == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}

static JSBool
js_lfexpand(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	ulong      i, j;
	char*      inbuf = NULL;
	char*      outbuf;
	JSString*  str;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (JSVAL_NULL_OR_VOID(argv[0])) {
		JS_SET_RVAL(cx, arglist, argv[0]);
		return JS_TRUE;
	}
	JSVALUE_TO_MSTRING(cx, argv[0], inbuf, NULL);
	HANDLE_PENDING(cx, inbuf);
	if (inbuf == NULL)
		return JS_TRUE;

	if ((outbuf = (char*)malloc((strlen(inbuf) * 2) + 1)) == NULL) {
		JS_ReportError(cx, "Error allocating %lu bytes at %s:%d", (strlen(inbuf) * 2) + 1, getfname(__FILE__), __LINE__);
		free(inbuf);
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	for (i = j = 0; inbuf[i]; i++) {
		if (inbuf[i] == '\n' && (!i || inbuf[i - 1] != '\r'))
			outbuf[j++] = '\r';
		outbuf[j++] = inbuf[i];
	}
	free(inbuf);
	outbuf[j] = 0;
	JS_RESUMEREQUEST(cx, rc);

	str = JS_NewStringCopyZ(cx, outbuf);
	free(outbuf);
	if (str == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(str));
	return JS_TRUE;
}

static JSBool
js_word_wrap(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	int32      len = 79;
	int32      oldlen = 79;
	JSBool     handle_quotes = JS_TRUE;
	int32_t    mode = 0;
	char*      inbuf = NULL;
	char*      outbuf;
	JSString*  js_str;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (JSVAL_NULL_OR_VOID(argv[0])) {
		JS_SET_RVAL(cx, arglist, argv[0]);
		return JS_TRUE;
	}
	JSVALUE_TO_MSTRING(cx, argv[0], inbuf, NULL);
	HANDLE_PENDING(cx, inbuf);
	if (inbuf == NULL)
		return JS_TRUE;

	if (argc > 1) {
		if (!JS_ValueToInt32(cx, argv[1], &len)) {
			free(inbuf);
			return JS_FALSE;
		}
	}

	if (argc > 2) {
		if (!JS_ValueToInt32(cx, argv[2], &oldlen)) {
			free(inbuf);
			return JS_FALSE;
		}
	}

	if (argc > 3 && JSVAL_IS_BOOLEAN(argv[3]))
		handle_quotes = JSVAL_TO_BOOLEAN(argv[3]);
	if (argc > 4 && JSVAL_IS_NUMBER(argv[4])) {
		if (!JS_ValueToInt32(cx, argv[4], &mode)) {
			free(inbuf);
			return JS_FALSE;
		}
	} else {
		if (argc > 4 && JSVAL_IS_BOOLEAN(argv[4])) {
			if (JSVAL_TO_BOOLEAN(argv[4]))
				mode |= P_UTF8;
		}
		if (argc > 5 && JSVAL_IS_BOOLEAN(argv[5])) {
			if (JSVAL_TO_BOOLEAN(argv[5]))
				mode |= P_RENEGADE;
		}
	}

	rc = JS_SUSPENDREQUEST(cx);

	outbuf = wordwrap(inbuf, len, oldlen, handle_quotes, mode);
	free(inbuf);

	JS_RESUMEREQUEST(cx, rc);

	if (outbuf == NULL) {
		JS_ReportError(cx, "Allocation error in wordwrap()");
		return JS_FALSE;
	}
	js_str = JS_NewStringCopyZ(cx, outbuf);
	free(outbuf);
	if (js_str == NULL)
		return JS_FALSE;
	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}

static JSBool
js_quote_msg(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *     argv = JS_ARGV(cx, arglist);
	int32       len = 79;
	int         i, l, clen;
	char*       inbuf = NULL;
	char*       outbuf;
	char*       linebuf;
	char*       tmpptr;
	const char *prefix_def = " > ";
	char*       prefix = (char *)prefix_def;
	JSString*   js_str;
	jsrefcount  rc;
	size_t      linebuf_sz = 0;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], inbuf, NULL);
	HANDLE_PENDING(cx, inbuf);
	if (inbuf == NULL)
		return JS_TRUE;

	if (argc > 1) {
		if (!JS_ValueToInt32(cx, argv[1], &len)) {
			free(inbuf);
			return JS_FALSE;
		}
	}

	if (argc > 2) {
		JSVALUE_TO_MSTRING(cx, argv[2], prefix, NULL);
		if (JS_IsExceptionPending(cx)) {
			free(inbuf);
			return JS_FALSE;
		}
		if (prefix == NULL)
			prefix = strdup("");
	}

	if ((outbuf = (char*)malloc((strlen(inbuf) * (strlen(prefix) + 1)) + 1)) == NULL) {
		JS_ReportError(cx, "Error allocating %lu bytes at %s:%d", (strlen(inbuf) * (strlen(prefix) + 1)) + 1, getfname(__FILE__), __LINE__);
		free(inbuf);
		if (prefix != prefix_def)
			free(prefix);
		return JS_FALSE;
	}

	len -= strlen(prefix);
	if (len <= 0) {
		free(outbuf);
		free(inbuf);
		if (prefix != prefix_def)
			free(prefix);
		return JS_TRUE;
	}

	linebuf_sz = (len * 3) + 2;
	if ((linebuf = (char*)malloc(linebuf_sz)) == NULL) {
		free(inbuf);
		free(outbuf);
		if (prefix != prefix_def)
			free(prefix);
		JS_ReportError(cx, "Error allocating %lu bytes at %s:%d", linebuf_sz, getfname(__FILE__), __LINE__);
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	outbuf[0] = 0;
	clen = 0;
	for (i = l = 0; inbuf[i]; i++) {
		if (l == 0)    /* Do not use clen here since if the line starts with ^A, could stay at zero */
			strcat(outbuf, prefix);
		if (clen < len || inbuf[i] == '\n' || inbuf[i] == '\r')
			linebuf[l++] = inbuf[i];
		if (inbuf[i] == '\x01') {      /* Handle CTRL-A */
			linebuf[l++] = inbuf[++i];
			if (inbuf[i] == '\x01')
				clen++;
		}
		else
			clen++;
		if ((unsigned)l >= linebuf_sz - 1) {
			linebuf_sz *= 2;
			tmpptr = static_cast<char *>(realloc(linebuf, linebuf_sz));
			if (tmpptr == NULL) {
				free(linebuf);
				free(inbuf);
				free(outbuf);
				if (prefix != prefix_def)
					free(prefix);
				JS_RESUMEREQUEST(cx, rc);
				JS_ReportError(cx, "Error reallocating %lu bytes at %s:%d", linebuf_sz, getfname(__FILE__), __LINE__);
				return JS_FALSE;
			}
			linebuf = tmpptr;
		}
		/* ToDo: Handle TABs etc. */
		if (inbuf[i] == '\n') {
			strncat(outbuf, linebuf, l);
			l = 0;
			clen = 0;
		}
	}
	if (l)   /* remainder */
		strncat(outbuf, linebuf, l);
	free(linebuf);
	free(inbuf);
	if (prefix != prefix_def)
		free(prefix);
	JS_RESUMEREQUEST(cx, rc);

	js_str = JS_NewStringCopyZ(cx, outbuf);
	free(outbuf);
	if (js_str == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}

static JSBool
js_netaddr_type(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv = JS_ARGV(cx, arglist);
	char*  str = NULL;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(NET_NONE));
	if (JSVAL_NULL_OR_VOID(argv[0]))
		return JS_TRUE;

	JSVALUE_TO_MSTRING(cx, argv[0], str, NULL);
	HANDLE_PENDING(cx, str);
	if (str == NULL)
		return JS_TRUE;

	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(smb_netaddr_type(str)));
	free(str);

	return JS_TRUE;
}

/* This table is used to convert between IBM ex-ASCII and HTML character entities */
/* Much of this table supplied by Deuce (thanks!) */
static struct {
	int value;
	const char* name;
} exasctbl[128] = {
/*  HTML val,name             ASCII  description */
	{ 199, "Ccedil"   },   /* 128 C, cedilla */
	{ 252, "uuml"     },   /* 129 u, umlaut */
	{ 233, "eacute"   },   /* 130 e, acute accent */
	{ 226, "acirc"    },   /* 131 a, circumflex accent */
	{ 228, "auml"     },   /* 132 a, umlaut */
	{ 224, "agrave"   },   /* 133 a, grave accent */
	{ 229, "aring"    },   /* 134 a, ring */
	{ 231, "ccedil"   },   /* 135 c, cedilla */
	{ 234, "ecirc"    },   /* 136 e, circumflex accent */
	{ 235, "euml"     },   /* 137 e, umlaut */
	{ 232, "egrave"   },   /* 138 e, grave accent */
	{ 239, "iuml"     },   /* 139 i, umlaut */
	{ 238, "icirc"    },   /* 140 i, circumflex accent */
	{ 236, "igrave"   },   /* 141 i, grave accent */
	{ 196, "Auml"     },   /* 142 A, umlaut */
	{ 197, "Aring"    },   /* 143 A, ring */
	{ 201, "Eacute"   },   /* 144 E, acute accent */
	{ 230, "aelig"    },   /* 145 ae ligature */
	{ 198, "AElig"    },   /* 146 AE ligature */
	{ 244, "ocirc"    },   /* 147 o, circumflex accent */
	{ 246, "ouml"     },   /* 148 o, umlaut */
	{ 242, "ograve"   },   /* 149 o, grave accent */
	{ 251, "ucirc"    },   /* 150 u, circumflex accent */
	{ 249, "ugrave"   },   /* 151 u, grave accent */
	{ 255, "yuml"     },   /* 152 y, umlaut */
	{ 214, "Ouml"     },   /* 153 O, umlaut */
	{ 220, "Uuml"     },   /* 154 U, umlaut */
	{ 162, "cent"     },   /* 155 Cent sign */
	{ 163, "pound"    },   /* 156 Pound sign */
	{ 165, "yen"      },   /* 157 Yen sign */
	{ 8359, NULL       },  /* 158 Pt (unicode) */
	{ 402, NULL       },   /* 402 Florin (non-standard alsi 159?) */
	{ 225, "aacute"   },   /* 160 a, acute accent */
	{ 237, "iacute"   },   /* 161 i, acute accent */
	{ 243, "oacute"   },   /* 162 o, acute accent */
	{ 250, "uacute"   },   /* 163 u, acute accent */
	{ 241, "ntilde"   },   /* 164 n, tilde */
	{ 209, "Ntilde"   },   /* 165 N, tilde */
	{ 170, "ordf"     },   /* 166 Feminine ordinal */
	{ 186, "ordm"     },   /* 167 Masculine ordinal */
	{ 191, "iquest"   },   /* 168 Inverted question mark */
	{ 8976, NULL       },  /* 169 Inverse "Not sign" (unicode) */
	{ 172, "not"      },   /* 170 Not sign */
	{ 189, "frac12"   },   /* 171 Fraction one-half */
	{ 188, "frac14"   },   /* 172 Fraction one-fourth */
	{ 161, "iexcl"    },   /* 173 Inverted exclamation point */
	{ 171, "laquo"    },   /* 174 Left angle quote */
	{ 187, "raquo"    },   /* 175 Right angle quote */
	{ 9617, NULL       },  /* 176 drawing symbol (unicode) */
	{ 9618, NULL       },  /* 177 drawing symbol (unicode) */
	{ 9619, NULL       },  /* 178 drawing symbol (unicode) */
	{ 9474, NULL       },  /* 179 drawing symbol (unicode) */
	{ 9508, NULL       },  /* 180 drawing symbol (unicode) */
	{ 9569, NULL       },  /* 181 drawing symbol (unicode) */
	{ 9570, NULL       },  /* 182 drawing symbol (unicode) */
	{ 9558, NULL       },  /* 183 drawing symbol (unicode) */
	{ 9557, NULL       },  /* 184 drawing symbol (unicode) */
	{ 9571, NULL       },  /* 185 drawing symbol (unicode) */
	{ 9553, NULL       },  /* 186 drawing symbol (unicode) */
	{ 9559, NULL       },  /* 187 drawing symbol (unicode) */
	{ 9565, NULL       },  /* 188 drawing symbol (unicode) */
	{ 9564, NULL       },  /* 189 drawing symbol (unicode) */
	{ 9563, NULL       },  /* 190 drawing symbol (unicode) */
	{ 9488, NULL       },  /* 191 drawing symbol (unicode) */
	{ 9492, NULL       },  /* 192 drawing symbol (unicode) */
	{ 9524, NULL       },  /* 193 drawing symbol (unicode) */
	{ 9516, NULL       },  /* 194 drawing symbol (unicode) */
	{ 9500, NULL       },  /* 195 drawing symbol (unicode) */
	{ 9472, NULL       },  /* 196 drawing symbol (unicode) */
	{ 9532, NULL       },  /* 197 drawing symbol (unicode) */
	{ 9566, NULL       },  /* 198 drawing symbol (unicode) */
	{ 9567, NULL       },  /* 199 drawing symbol (unicode) */
	{ 9562, NULL       },  /* 200 drawing symbol (unicode) */
	{ 9556, NULL       },  /* 201 drawing symbol (unicode) */
	{ 9577, NULL       },  /* 202 drawing symbol (unicode) */
	{ 9574, NULL       },  /* 203 drawing symbol (unicode) */
	{ 9568, NULL       },  /* 204 drawing symbol (unicode) */
	{ 9552, NULL       },  /* 205 drawing symbol (unicode) */
	{ 9580, NULL       },  /* 206 drawing symbol (unicode) */
	{ 9575, NULL       },  /* 207 drawing symbol (unicode) */
	{ 9576, NULL       },  /* 208 drawing symbol (unicode) */
	{ 9572, NULL       },  /* 209 drawing symbol (unicode) */
	{ 9573, NULL       },  /* 210 drawing symbol (unicode) */
	{ 9561, NULL       },  /* 211 drawing symbol (unicode) */
	{ 9560, NULL       },  /* 212 drawing symbol (unicode) */
	{ 9554, NULL       },  /* 213 drawing symbol (unicode) */
	{ 9555, NULL       },  /* 214 drawing symbol (unicode) */
	{ 9579, NULL       },  /* 215 drawing symbol (unicode) */
	{ 9578, NULL       },  /* 216 drawing symbol (unicode) */
	{ 9496, NULL       },  /* 217 drawing symbol (unicode) */
	{ 9484, NULL       },  /* 218 drawing symbol (unicode) */
	{ 9608, NULL       },  /* 219 drawing symbol (unicode) */
	{ 9604, NULL       },  /* 220 drawing symbol (unicode) */
	{ 9612, NULL       },  /* 221 drawing symbol (unicode) */
	{ 9616, NULL       },  /* 222 drawing symbol (unicode) */
	{ 9600, NULL       },  /* 223 drawing symbol (unicode) */
	{ 945, NULL       },   /* 224 alpha symbol */
	{ 223, "szlig"    },   /* 225 sz ligature (beta symbol) */
	{ 915, NULL       },   /* 226 omega symbol */
	{ 960, NULL       },   /* 227 pi symbol*/
	{ 931, NULL       },   /* 228 epsilon symbol */
	{ 963, NULL       },   /* 229 o with stick */
	{ 181, "micro"    },   /* 230 Micro sign (Greek mu) */
	{ 964, NULL       },   /* 231 greek char? */
	{ 934, NULL       },   /* 232 greek char? */
	{ 920, NULL       },   /* 233 greek char? */
	{ 937, NULL       },   /* 234 greek char? */
	{ 948, NULL       },   /* 235 greek char? */
	{ 8734, NULL       },  /* 236 infinity symbol (unicode) */
	{ 966, "oslash"   },   /* 237 Greek Phi */
	{ 949, NULL       },   /* 238 rounded E */
	{ 8745, NULL       },  /* 239 unside down U (unicode) */
	{ 8801, NULL       },  /* 240 drawing symbol (unicode) */
	{ 177, "plusmn"   },   /* 241 Plus or minus */
	{ 8805, NULL       },  /* 242 drawing symbol (unicode) */
	{ 8804, NULL       },  /* 243 drawing symbol (unicode) */
	{ 8992, NULL       },  /* 244 drawing symbol (unicode) */
	{ 8993, NULL       },  /* 245 drawing symbol (unicode) */
	{ 247, "divide"   },   /* 246 Division sign */
	{ 8776, NULL       },  /* 247 two squiggles (unicode) */
	{ 176, "deg"      },   /* 248 Degree sign */
	{ 8729, NULL       },  /* 249 drawing symbol (unicode) */
	{ 183, "middot"   },   /* 250 Middle dot */
	{ 8730, NULL       },  /* 251 check mark (unicode) */
	{ 8319, NULL       },  /* 252 superscript n (unicode) */
	{ 178, "sup2"     },   /* 253 superscript 2 */
	{ 9632, NULL       },  /* 254 drawing symbol (unicode) */
	{ 160, "nbsp"     }    /* 255 non-breaking space */
};

static struct {
	int value;
	const char* name;
} lowasctbl[32] = {
	{ 160, "nbsp"     },   /* NULL non-breaking space */
	{ 9786, NULL       },  /* 0x263a white smiling face (^A, 1) */
	{ 9787, NULL       },  /* 0x263b black smiling face (^B, 2) */
	{ 9829, "hearts"   },  /* black heart suit */
	{ 9830, "diams"    },  /* black diamond suit */
	{ 9827, "clubs"    },  /* black club suit */
	{ 9824, "spades"   },  /* black spade suit */
	{ 8226, "bull"     },  /* bullet (beep, 7) */
	{ 9688, NULL       },  /* inverse bullet (backspace, 8) */
	{ 9702, NULL       },  /* white bullet (tab, 9) */
	{ 9689, NULL       },  /* inverse white circle */
	{ 9794, NULL       },  /* male sign */
	{ 9792, NULL       },  /* female sign */
	{ 9834, NULL       },  /* eighth note */
	{ 9835, NULL       },  /* beamed eighth notes */
	{ 9788, NULL       },  /* white sun with rays */
	{ 9658, NULL       },  /* 0x25BA black right-pointing pointer */
	{ 9668, NULL       },  /* 0x25C4 black left-pointing pointer */
	{ 8597, NULL       },  /* up down arrow */
	{ 8252, NULL       },  /* double exclamation mark */
	{ 182, "para"     },   /* pilcrow sign */
	{ 167, "sect"     },   /* section sign */
	{ 9644, NULL       },  /* 0x25AC black rectangle */
	{ 8616, NULL       },  /* up down arrow with base */
	{ 8593, "uarr"     },  /* upwards arrow */
	{ 8595, "darr"     },  /* downwards arrow */
	{ 8594, "rarr"     },  /* rightwards arrow */
	{ 8592, "larr"     },  /* leftwards arrow */
	{ 8985, NULL       },  /* turned not sign */
	{ 8596, "harr"     },  /* left right arrow */
	{ 9650, NULL       },  /* 0x25B2 black up-pointing triangle */
	{ 9660, NULL       }   /* 0x25BC black down-pointing triangle */
};

static JSBool
js_html_encode(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *        obj = JS_THIS_OBJECT(cx, arglist);
	jsval *           argv = JS_ARGV(cx, arglist);
	int               ch;
	ulong             i, j;
	char*             inbuf = NULL;
	char*             tmpbuf;
	size_t            tmpbuf_sz;
	char*             outbuf;
	char*             param;
	char*             lastparam;
	char*             tmpptr;
	JSBool            exascii = JS_TRUE;
	JSBool            wsp = JS_TRUE;
	JSBool            ansi = JS_TRUE;
	JSBool            ctrl_a = JS_TRUE;
	JSString*         js_str;
	int32             fg = 7;
	int32             bg = 0;
	JSBool            blink = FALSE;
	JSBool            bold = FALSE;
	int               esccount = 0;
	char              ansi_seq[MAX_ANSI_SEQ + 1];
	int               ansi_param[MAX_ANSI_PARAMS];
	int               k, l;
	ulong             savepos = 0;
	int32             hpos = 0;
	int32             currrow = 0;
	int               savehpos = 0;
	int               savevpos = 0;
	int32             wraphpos = -2;
	int32             wrapvpos = -2;
	int32             wrappos = 0;
	BOOL              extchar = FALSE;
	ulong             obsize;
	int32             lastcolor = 7;
	char              tmp1[128];
	struct      tm    tm;
	time_t            now;
	BOOL              nodisplay = FALSE;
	global_private_t* p;
	uchar             attr_stack[64]; /* Saved attributes (stack) */
	int               attr_sp = 0;        /* Attribute stack pointer */
	ulong             clear_screen = 0;
	JSObject*         stateobj = NULL;
	jsval             val;
	jsrefcount        rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	if ((p = (global_private_t*)js_GetClassPrivate(cx, obj, &js_global_class)) == NULL)
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], inbuf, NULL);
	HANDLE_PENDING(cx, inbuf);
	if (inbuf == NULL)
		return JS_TRUE;

	if (argc > 1 && JSVAL_IS_BOOLEAN(argv[1]))
		exascii = JSVAL_TO_BOOLEAN(argv[1]);

	if (argc > 2 && JSVAL_IS_BOOLEAN(argv[2]))
		wsp = JSVAL_TO_BOOLEAN(argv[2]);

	if (argc > 3 && JSVAL_IS_BOOLEAN(argv[3]))
		ansi = JSVAL_TO_BOOLEAN(argv[3]);

	if (argc > 4 && JSVAL_IS_BOOLEAN(argv[4]))
	{
		ctrl_a = JSVAL_TO_BOOLEAN(argv[4]);
		if (ctrl_a)
			ansi = ctrl_a;
	}

	if (argc > 5 && JSVAL_IS_OBJECT(argv[5])) {
		stateobj = JSVAL_TO_OBJECT(argv[5]);
		if (stateobj != NULL && JS_GetProperty(cx, stateobj, "fg", &val) && JSVAL_IS_NUMBER(val)) {
			if (!JS_ValueToInt32(cx, val, &fg)) {
				free(inbuf);
				return JS_FALSE;
			}
		}
		if (JS_GetProperty(cx, stateobj, "bg", &val) && JSVAL_IS_NUMBER(val)) {
			if (!JS_ValueToInt32(cx, val, &bg)) {
				free(inbuf);
				return JS_FALSE;
			}
		}
		if (JS_GetProperty(cx, stateobj, "lastcolor", &val) && JSVAL_IS_NUMBER(val)) {
			if (!JS_ValueToInt32(cx, val, &lastcolor)) {
				free(inbuf);
				return JS_FALSE;
			}
		}
		if (JS_GetProperty(cx, stateobj, "blink", &val) && JSVAL_IS_BOOLEAN(val)) {
			if (!JS_ValueToBoolean(cx, val, &blink)) {
				free(inbuf);
				return JS_FALSE;
			}
		}
		if (JS_GetProperty(cx, stateobj, "bold", &val) && JSVAL_IS_BOOLEAN(val)) {
			if (!JS_ValueToBoolean(cx, val, &bold)) {
				free(inbuf);
				return JS_FALSE;
			}
		}
		if (JS_GetProperty(cx, stateobj, "hpos", &val) && JSVAL_IS_NUMBER(val)) {
			if (!JS_ValueToInt32(cx, val, &hpos)) {
				free(inbuf);
				return JS_FALSE;
			}
		}
		if (JS_GetProperty(cx, stateobj, "currrow", &val) && JSVAL_IS_NUMBER(val)) {
			if (!JS_ValueToInt32(cx, val, &currrow)) {
				free(inbuf);
				return JS_FALSE;
			}
		}
		if (JS_GetProperty(cx, stateobj, "wraphpos", &val) && JSVAL_IS_NUMBER(val)) {
			if (!JS_ValueToInt32(cx, val, &wraphpos)) {
				free(inbuf);
				return JS_FALSE;
			}
		}
		if (JS_GetProperty(cx, stateobj, "wrapvpos", &val) && JSVAL_IS_NUMBER(val)) {
			if (!JS_ValueToInt32(cx, val, &wrapvpos)) {
				free(inbuf);
				return JS_FALSE;
			}
		}
		if (JS_GetProperty(cx, stateobj, "wrappos", &val) && JSVAL_IS_NUMBER(val)) {
			if (!JS_ValueToInt32(cx, val, &wrappos)) {
				free(inbuf);
				return JS_FALSE;
			}
		}
	}

	tmpbuf_sz = (strlen(inbuf) * 10) + 1;
	if ((tmpbuf = (char*)malloc(tmpbuf_sz)) == NULL) {
		free(inbuf);
		JS_ReportError(cx, "Error allocating %lu bytes at %s:%d", tmpbuf_sz, getfname(__FILE__), __LINE__);
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	for (i = j = 0; inbuf[i]; i++) {
		switch (inbuf[i]) {
			case TAB:
			case LF:
			case CR:
				if (wsp)
					j += sprintf(tmpbuf + j, "&#%u;", inbuf[i]);
				else
					tmpbuf[j++] = inbuf[i];
				break;
			case '"':
				j += sprintf(tmpbuf + j, "&quot;");
				break;
			case '&':
				j += sprintf(tmpbuf + j, "&amp;");
				break;
			case '<':
				j += sprintf(tmpbuf + j, "&lt;");
				break;
			case '>':
				j += sprintf(tmpbuf + j, "&gt;");
				break;
			case '\b':
				if (j)
					j--;
				break;
			default:
				if (inbuf[i] & 0x80) {
					if (exascii) {
						ch = inbuf[i] ^ '\x80';
						if (exasctbl[ch].name != NULL)
							j += sprintf(tmpbuf + j, "&%s;", exasctbl[ch].name);
						else
							j += sprintf(tmpbuf + j, "&#%u;", exasctbl[ch].value);
					} else
						tmpbuf[j++] = inbuf[i];
				}
				else if (inbuf[i] >= ' ' && inbuf[i] < DEL)
					tmpbuf[j++] = inbuf[i];
#if 0       /* ASCII 127 - Not displayed? */
				else if (inbuf[i] == DEL && exascii)
					j += sprintf(tmpbuf + j, "&#8962;", exasctbl[ch].value);
#endif
				else if (inbuf[i] < ' ' && inbuf[i] > 0) /* unknown control chars */
				{
					if (ansi && inbuf[i] == ESC)
					{
						esccount++;
						tmpbuf[j++] = inbuf[i];
					}
					else if (ctrl_a && inbuf[i] == CTRL_A)
					{
						esccount++;
						tmpbuf[j++] = inbuf[i];
						tmpbuf[j++] = inbuf[++i];
					}
					else if (exascii) {
						ch = inbuf[i];
						if (lowasctbl[ch].name != NULL)
							j += sprintf(tmpbuf + j, "&%s;", lowasctbl[ch].name);
						else
							j += sprintf(tmpbuf + j, "&#%u;", lowasctbl[ch].value);
					} else
						j += sprintf(tmpbuf + j, "&#%u;", (uchar)inbuf[i]);
				}
				break;
		}
		if (j >= tmpbuf_sz - 1) {
			tmpbuf_sz += (tmpbuf_sz / 2);
			tmpptr = static_cast<char *>(realloc(tmpbuf, tmpbuf_sz));
			if (tmpptr == NULL) {
				free(tmpbuf);
				free(inbuf);
				JS_RESUMEREQUEST(cx, rc);
				JS_ReportError(cx, "Error reallocating %lu bytes at %s:%d", tmpbuf_sz, getfname(__FILE__), __LINE__);
				return JS_FALSE;
			}
			tmpbuf = tmpptr;
		}
	}
	tmpbuf[j] = 0;
	free(inbuf);

	if (ansi)
	{
		obsize = (strlen(tmpbuf) + (esccount + 1) * MAX_COLOR_STRING) + 1;
		if (obsize < 2048)
			obsize = 2048;
		if ((outbuf = (char*)malloc(obsize)) == NULL)
		{
			free(tmpbuf);
			JS_RESUMEREQUEST(cx, rc);
			JS_ReportError(cx, "Error allocating %lu bytes at %s:%d", obsize, getfname(__FILE__), __LINE__);
			return JS_FALSE;
		}
		j = sprintf(outbuf, "<span style=\"%s\">", htmlansi[7]);
		clear_screen = j;
		for (i = 0; tmpbuf[i]; i++) {
			if (j > (obsize / 2))        /* Completely arbitrary here... must be carefull with this eventually ToDo */
			{
				obsize += (obsize / 2);
				if ((param = static_cast<char *>(realloc(outbuf, obsize))) == NULL)
				{
					free(tmpbuf);
					free(outbuf);
					JS_RESUMEREQUEST(cx, rc);
					JS_ReportError(cx, "Error allocating %lu bytes at %s:%d", obsize, getfname(__FILE__), __LINE__);
					return JS_FALSE;
				}
				outbuf = param;
			}
			if (tmpbuf[i] == ESC && tmpbuf[i + 1] == '[')
			{
				if (nodisplay)
					continue;
				k = 0;
				memset(&ansi_param, 0xff, sizeof(int) * MAX_ANSI_PARAMS);
				strncpy(ansi_seq, tmpbuf + i + 2, MAX_ANSI_SEQ);
				ansi_seq[MAX_ANSI_SEQ] = 0;
				for (lastparam = ansi_seq; *lastparam; lastparam++)
				{
					if (IS_ALPHA(*lastparam))
					{
						*(++lastparam) = 0;
						break;
					}
				}
				k = 0;
				param = ansi_seq;
				if (*param == '?')     /* This is to fix ESC[?7 whatever that is */
					param++;
				if (IS_DIGIT(*param))
					ansi_param[k++] = atoi(ansi_seq);
				while (IS_WHITESPACE(*param) || IS_DIGIT(*param))
					param++;
				lastparam = param;
				while ((param = strchr(param, ';')) != NULL)
				{
					param++;
					ansi_param[k++] = atoi(param);
					while (IS_WHITESPACE(*param) || IS_DIGIT(*param))
						param++;
					lastparam = param;
				}
				switch (*lastparam)
				{
					case 'm':   /* Colour */
						for (k = 0; ansi_param[k] >= 0; k++)
						{
							switch (ansi_param[k])
							{
								case 0:
									fg = 7;
									bg = 0;
									blink = FALSE;
									bold = FALSE;
									break;
								case 1:
									bold = TRUE;
									break;
								case 2:
									bold = FALSE;
									break;
								case 5:
									blink = TRUE;
									break;
								case 6:
									blink = TRUE;
									break;
								case 7:
									l = fg;
									fg = bg;
									bg = l;
									break;
								case 8:
									fg = bg;
									blink = FALSE;
									bold = FALSE;
									break;
								case 30:
								case 31:
								case 32:
								case 33:
								case 34:
								case 35:
								case 36:
								case 37:
									fg = ansi_param[k] - 30;
									break;
								case 40:
								case 41:
								case 42:
								case 43:
								case 44:
								case 45:
								case 46:
								case 47:
									bg = ansi_param[k] - 40;
									break;
							}
						}
						if (k == 0) {
							fg = 7;
							bg = 0;
							blink = FALSE;
							bold = FALSE;
							break;
						}
						break;
					case 'C': /* Move right */
						j += sprintf(outbuf + j, "%s%s%s", HTML_COLOR_PREFIX, htmlansi[0], HTML_COLOR_SUFFIX);
						lastcolor = 0;
						l = ansi_param[0] > 0?ansi_param[0]:1;
						if (wrappos == 0 && wrapvpos == currrow)  {
							/* j+=sprintf(outbuf+j,"<!-- \r\nC after A l=%d hpos=%d -->",l,hpos); */
							l = l - hpos;
							wrapvpos = -2;    /* Prevent additional move right */
						}
						if (l > 81 - hpos)
							l = 81 - hpos;
						for (k = 0; k < l; k++)
						{
							j += sprintf(outbuf + j, "%s", "&nbsp;");
							hpos++;
						}
						break;
					case 's': /* Save position */
						savepos = j;
						savehpos = hpos;
						savevpos = currrow;
						break;
					case 'u': /* Restore saved position */
						j = savepos;
						hpos = savehpos;
						currrow = savevpos;
						break;
					case 'H': /* Move */
						k = ansi_param[0];
						if (k <= 0)
							k = 1;
						k--;
						l = ansi_param[1];
						if (l <= 0)
							l = 1;
						l--;
						while (k > currrow)
						{
							hpos = 0;
							currrow++;
							outbuf[j++] = '\r';
							outbuf[j++] = '\n';
						}
						if (l > hpos)
						{
							j += sprintf(outbuf + j, "%s%s%s", HTML_COLOR_PREFIX, htmlansi[0], HTML_COLOR_SUFFIX);
							lastcolor = 0;
							while (l > hpos)
							{
								j += sprintf(outbuf + j, "%s", "&nbsp;");
								hpos++;
							}
						}
						break;
					case 'B': /* Move down */
						l = ansi_param[0];
						if (l <= 0)
							l = 1;
						for (k = 0; k < l; k++)
						{
							currrow++;
							outbuf[j++] = '\r';
							outbuf[j++] = '\n';
						}
						if (hpos != 0 && tmpbuf[i + 1] != CR)
						{
							j += sprintf(outbuf + j, "%s%s%s", HTML_COLOR_PREFIX, htmlansi[0], HTML_COLOR_SUFFIX);
							lastcolor = 0;
							for (k = 0; k < hpos ; k++)
							{
								j += sprintf(outbuf + j, "%s", "&nbsp;");
							}
							break;
						}
						break;
					case 'A': /* Move up */
						l = wrappos;
						if (j > (ulong)wrappos && hpos == 0 && currrow == wrapvpos + 1 && ansi_param[0] <= 1)  {
							hpos = wraphpos;
							currrow = wrapvpos;
							j = wrappos;
							wrappos = 0; /* Prevent additional move up */
						}
						break;
					case 'J': /* Clear */
						if (ansi_param[0] == 2)  {
							j = clear_screen;
							hpos = 0;
							currrow = 0;
							wraphpos = -2;
							wrapvpos = -2;
							wrappos = 0;
						}
						break;
				}
				i += (int)(lastparam - ansi_seq) + 2;
			}
			else if (ctrl_a && tmpbuf[i] == CTRL_A)        /* CTRL-A codes */
			{
/*				j+=sprintf(outbuf+j,"<!-- CTRL-A-%c (%u) -->",tmpbuf[i+1],tmpbuf[i+1]); */
				if (nodisplay && tmpbuf[i + 1] != ')')
					continue;
				if (tmpbuf[i + 1] & 0x80)
				{
					j += sprintf(outbuf + j, "%s%s%s", HTML_COLOR_PREFIX, htmlansi[0], HTML_COLOR_SUFFIX);
					lastcolor = 0;
					l = (tmpbuf[i + 1] ^ '\x80') + 1;
					if (l > 81 - hpos)
						l = 81 - hpos;
					for (k = 0; k < l; k++)
					{
						j += sprintf(outbuf + j, "%s", "&nbsp;");
						hpos++;
					}
				}
				else switch (toupper(tmpbuf[i + 1]))
					{
						case 'K':
							fg = 0;
							break;
						case 'R':
							fg = 1;
							break;
						case 'G':
							fg = 2;
							break;
						case 'Y':
							fg = 3;
							break;
						case 'B':
							fg = 4;
							break;
						case 'M':
							fg = 5;
							break;
						case 'C':
							fg = 6;
							break;
						case 'W':
							fg = 7;
							break;
						case '0':
							bg = 0;
							break;
						case '1':
							bg = 1;
							break;
						case '2':
							bg = 2;
							break;
						case '3':
							bg = 3;
							break;
						case '4':
							bg = 4;
							break;
						case '5':
							bg = 5;
							break;
						case '6':
							bg = 6;
							break;
						case '7':
							bg = 7;
							break;
						case 'H':
							bold = TRUE;
							break;
						case 'I':
							blink = TRUE;
							break;
						case '+':
							if (attr_sp < (int)sizeof(attr_stack))
								attr_stack[attr_sp++] = (blink?(1 << 7):0) | (bg << 4) | (bold?(1 << 3):0) | (int)fg;
							break;
						case '-':
							if (attr_sp > 0)
							{
								blink = (attr_stack[--attr_sp] & (1 << 7))?TRUE:FALSE;
								bg = (attr_stack[attr_sp] >> 4) & 7;
								blink = (attr_stack[attr_sp] & (1 << 3))?TRUE:FALSE;
								fg = attr_stack[attr_sp] & 7;
							}
							else if (bold || blink || bg)
							{
								bold = FALSE;
								blink = FALSE;
								fg = 7;
								bg = 0;
							}
							break;
						case '_':
							if (blink || bg)
							{
								bold = FALSE;
								blink = FALSE;
								fg = 7;
								bg = 0;
							}
							break;
						case 'N':
							bold = FALSE;
							blink = FALSE;
							fg = 7;
							bg = 0;
							break;
						case 'P':
						case 'Q':
						case ',':
						case ';':
						case '.':
						case 'S':
						case '>':
							break;
						case '<':   /* convert non-destructive backspace into destructive backspace */
							if (j)
								j--;
							break;

						case '!':   /* This needs to be fixed! (Somehow) */
						case '@':
						case '#':
						case '$':
						case '%':
						case '^':
						case '&':
						case '*':
						case '(':
							nodisplay = TRUE;
							break;
						case ')':
							nodisplay = FALSE;
							break;

						case 'D':
							now = time(NULL);
							j += sprintf(outbuf + j, "%s", datestr(p->cfg, now, tmp1));
							break;
						case 'T':
							now = time(NULL);
							localtime_r(&now, &tm);
							if (p->cfg->sys_misc & SM_MILITARY)
								j += sprintf(outbuf + j, "%02d:%02d:%02d"
								             , tm.tm_hour, tm.tm_min, tm.tm_sec);
							else
								j += sprintf(outbuf + j, "%02d:%02d %s"
								             , tm.tm_hour == 0 ? 12
								: tm.tm_hour > 12 ? tm.tm_hour - 12
								: tm.tm_hour, tm.tm_min, tm.tm_hour > 11 ? "pm":"am");
							break;
						case 'L':
							currrow = 0;
							hpos = 0;
							outbuf[j++] = '\r';
							outbuf[j++] = '\n';
							break;
						case '/': // conditional new-line
							if (hpos > 0) {
								hpos = 0;
								outbuf[j++] = '\r';
								outbuf[j++] = '\n';
							}
							break;
						case ']':
							currrow++;
							if (hpos != 0 && tmpbuf[i + 2] != CR && !(tmpbuf[i + 2] == CTRL_A && tmpbuf[i + 3] == '['))
							{
								outbuf[j++] = '\r';
								outbuf[j++] = '\n';
								j += sprintf(outbuf + j, "%s%s%s", HTML_COLOR_PREFIX, htmlansi[0], HTML_COLOR_SUFFIX);
								lastcolor = 0;
								for (k = 0; k < hpos ; k++)
								{
									j += sprintf(outbuf + j, "%s", "&nbsp;");
								}
								break;
							}
							outbuf[j++] = '\n';
							break;
						case '[':
							outbuf[j++] = '\r';
							hpos = 0;
							break;
						case 'Z': /* EOF */
							outbuf[j++] = 0;
							break;
						case 'A':
						default:
							if (exascii) {
								ch = tmpbuf[i];
								if (lowasctbl[ch].name != NULL)
									j += sprintf(outbuf + j, "&%s;", lowasctbl[ch].name);
								else
									j += sprintf(outbuf + j, "&#%u;", lowasctbl[ch].value);
							} else
								j += sprintf(outbuf + j, "&#%u;", (uchar)tmpbuf[i]);
							i--;
					}
				i++;
			}
			else
			{
				if (nodisplay)
					continue;
				switch (tmpbuf[i])
				{
					case TAB:           /* This assumes that tabs do NOT use the current background. */
						l = hpos % 8;
						if (l == 0)
							l = 8;
						j += sprintf(outbuf + j, "%s%s%s", HTML_COLOR_PREFIX, htmlansi[0], HTML_COLOR_SUFFIX);
						lastcolor = 0;
						for (k = 0; k < l ; k++)
						{
							j += sprintf(outbuf + j, "%s", "&nbsp;");
							hpos++;
						}
						break;
					case LF:
						wrapvpos = currrow;
						if ((ulong)wrappos < j - 3)
							wrappos = j;
						currrow++;
						if (hpos != 0 && tmpbuf[i + 1] != CR)
						{
							outbuf[j++] = '\r';
							outbuf[j++] = '\n';
							j += sprintf(outbuf + j, "%s%s%s", HTML_COLOR_PREFIX, htmlansi[0], HTML_COLOR_SUFFIX);
							lastcolor = 0;
							for (k = 0; k < hpos ; k++)
							{
								j += sprintf(outbuf + j, "%s", "&nbsp;");
							}
							break;
						}
					case CR:
						if (wraphpos == -2 || hpos != 0)
							wraphpos = hpos;
						if ((ulong)wrappos < j - 3)
							wrappos = j;
						outbuf[j++] = tmpbuf[i];
						hpos = 0;
						break;
					default:
						if (lastcolor != ((blink?(1 << 7):0) | (bg << 4) | (bold?(1 << 3):0) | fg))
						{
							lastcolor = (blink?(1 << 7):0) | (bg << 4) | (bold?(1 << 3):0) | fg;
							j += sprintf(outbuf + j, "%s%s%s", HTML_COLOR_PREFIX, htmlansi[lastcolor], HTML_COLOR_SUFFIX);
						}
						outbuf[j++] = tmpbuf[i];
						if (tmpbuf[i] == '&')
							extchar = TRUE;
						if (tmpbuf[i] == ';')
							extchar = FALSE;
						if (!extchar)
							hpos++;
						/* ToDo: Fix hard-coded terminal window width (80) */
						if (hpos >= 80 && tmpbuf[i + 1] != '\r' && tmpbuf[i + 1] != '\n' && tmpbuf[i + 1] != ESC)
						{
							wrapvpos = -2;
							wraphpos = -2;
							wrappos = 0;
							hpos = 0;
							currrow++;
							outbuf[j++] = '\r';
							outbuf[j++] = '\n';
						}
				}
			}
		}
		strcpy(outbuf + j, "</span>");
		JS_RESUMEREQUEST(cx, rc);

		js_str = JS_NewStringCopyZ(cx, outbuf);
		free(outbuf);
	}
	else {
		JS_RESUMEREQUEST(cx, rc);
		js_str = JS_NewStringCopyZ(cx, tmpbuf);
	}

	free(tmpbuf);   /* assertion here, Feb-20-2006 */
	if (js_str == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));

	if (stateobj != NULL) {
		JS_DefineProperty(cx, stateobj, "fg", INT_TO_JSVAL(fg)
		                  , NULL, NULL, JSPROP_ENUMERATE);
		JS_DefineProperty(cx, stateobj, "bg", INT_TO_JSVAL(bg)
		                  , NULL, NULL, JSPROP_ENUMERATE);
		JS_DefineProperty(cx, stateobj, "lastcolor", INT_TO_JSVAL(lastcolor)
		                  , NULL, NULL, JSPROP_ENUMERATE);
		JS_DefineProperty(cx, stateobj, "blink", BOOLEAN_TO_JSVAL(blink)
		                  , NULL, NULL, JSPROP_ENUMERATE);
		JS_DefineProperty(cx, stateobj, "bold", BOOLEAN_TO_JSVAL(bold)
		                  , NULL, NULL, JSPROP_ENUMERATE);
		JS_DefineProperty(cx, stateobj, "hpos", INT_TO_JSVAL(hpos)
		                  , NULL, NULL, JSPROP_ENUMERATE);
		JS_DefineProperty(cx, stateobj, "currrow", INT_TO_JSVAL(currrow)
		                  , NULL, NULL, JSPROP_ENUMERATE);
		JS_DefineProperty(cx, stateobj, "wraphpos", INT_TO_JSVAL(wraphpos)
		                  , NULL, NULL, JSPROP_ENUMERATE);
		JS_DefineProperty(cx, stateobj, "wrapvpos", INT_TO_JSVAL(wrapvpos)
		                  , NULL, NULL, JSPROP_ENUMERATE);
		JS_DefineProperty(cx, stateobj, "wrappos", INT_TO_JSVAL(wrappos)
		                  , NULL, NULL, JSPROP_ENUMERATE);
	}

	return JS_TRUE;
}

static JSBool
js_html_decode(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	int        ch;
	int        val;
	ulong      i, j;
	char*      inbuf = NULL;
	char*      outbuf;
	char       token[16];
	size_t     t;
	JSString*  js_str;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], inbuf, NULL);
	HANDLE_PENDING(cx, inbuf);
	if (inbuf == NULL)
		return JS_TRUE;

	if ((outbuf = (char*)malloc(strlen(inbuf) + 1)) == NULL) {
		free(inbuf);
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	for (i = j = 0; inbuf[i]; i++) {
		if (inbuf[i] != '&') {
			outbuf[j++] = inbuf[i];
			continue;
		}
		for (i++, t = 0; inbuf[i] != 0 && inbuf[i] != ';' && t < sizeof(token) - 1; i++, t++)
			token[t] = inbuf[i];
		if (inbuf[i] == 0)
			break;
		token[t] = 0;

		/* First search the ex-ascii table for a name match */
		for (ch = 0; ch < 128; ch++)
			if (exasctbl[ch].name != NULL && strcmp(token, exasctbl[ch].name) == 0)
				break;
		if (ch < 128) {
			outbuf[j++] = ch | 0x80;
			continue;
		}
		if (token[0] == '#') {     /* numeric constant */
			val = atoi(token + 1);

			/* search ex-ascii table for a value match */
			for (ch = 0; ch < 128; ch++)
				if (exasctbl[ch].value == val)
					break;
			if (ch < 128) {
				outbuf[j++] = ch | 0x80;
				continue;
			}

			if ((val >= ' ' && val <= 0xff) || val == '\r' || val == '\n' || val == '\t') {
				outbuf[j++] = val;
				continue;
			}
		}
		if (strcmp(token, "quot") == 0) {
			outbuf[j++] = '"';
			continue;
		}
		if (strcmp(token, "amp") == 0) {
			outbuf[j++] = '&';
			continue;
		}
		if (strcmp(token, "lt") == 0) {
			outbuf[j++] = '<';
			continue;
		}
		if (strcmp(token, "gt") == 0) {
			outbuf[j++] = '>';
			continue;
		}
		if (strcmp(token, "curren") == 0) {
			outbuf[j++] = CTRL_O;
			continue;
		}
		if (strcmp(token, "para") == 0) {
			outbuf[j++] = CTRL_T;
			continue;
		}
		if (strcmp(token, "sect") == 0) {
			outbuf[j++] = CTRL_U;
			continue;
		}
		if (strcmp(token, "lrm") == 0       /* left-to-right mark, not printable */
		    || strcmp(token, "rlm") == 0)  /* right-to-left mark, not printable */
			continue;

		if (strcmp(token, "hellip") == 0) { /* horizontal ellipsis  */
			j += sprintf(outbuf + j, "...");
			continue;
		}

		if (strcmp(token, "bull") == 0) {   /* bullet  */
			outbuf[j++] = (char)249;
			continue;
		}

		if (strcmp(token, "lsquo") == 0 || strcmp(token, "rsquo") == 0
		    || strcmp(token, "lsaquo") == 0 || strcmp(token, "rsaquo") == 0) {
			outbuf[j++] = '\'';   /* single quotation mark: should lsaquo be converted to backtick (`)? */
			continue;
		}

		if (strcmp(token, "ldquo") == 0 || strcmp(token, "rdquo") == 0) {
			outbuf[j++] = '"';    /* double quotation mark */
			continue;
		}

		if (strcmp(token, "ndash") == 0 || strcmp(token, "mdash") == 0) {
			outbuf[j++] = '-';    /* dash */
			continue;
		}

		if (strcmp(token, "zwj") == 0 || strcmp(token, "zwnj") == 0) /* zero-width joiner / non-joiner */
			continue;

		/* Unknown character entity, leave intact */
		j += sprintf(outbuf + j, "&%s;", token);

	}
	outbuf[j] = 0;
	free(inbuf);
	JS_RESUMEREQUEST(cx, rc);

	js_str = JS_NewStringCopyZ(cx, outbuf);
	free(outbuf);
	if (js_str == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}

static JSBool
js_b64_encode(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	int        res;
	size_t     len;
	size_t     inbuf_len;
	char*      inbuf = NULL;
	char*      outbuf;
	JSString*  js_str;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);
	JSVALUE_TO_MSTRING(cx, argv[0], inbuf, &inbuf_len);
	HANDLE_PENDING(cx, inbuf);
	if (inbuf == NULL)
		return JS_TRUE;

	len = (inbuf_len * 4 / 3) + 4;

	if ((outbuf = (char*)malloc(len)) == NULL) {
		free(inbuf);
		JS_ReportError(cx, "Error allocating %lu bytes at %s:%d", len, getfname(__FILE__), __LINE__);
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	res = b64_encode(outbuf, len, inbuf, inbuf_len);
	free(inbuf);
	JS_RESUMEREQUEST(cx, rc);

	if (res < 1) {
		free(outbuf);
		return JS_TRUE;
	}

	js_str = JS_NewStringCopyZ(cx, outbuf);
	free(outbuf);
	if (js_str == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}

static JSBool
js_b64_decode(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	int        res;
	size_t     len;
	char*      inbuf = NULL;
	char*      outbuf;
	JSString*  js_str;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);
	JSVALUE_TO_MSTRING(cx, argv[0], inbuf, NULL);
	HANDLE_PENDING(cx, inbuf);
	if (inbuf == NULL)
		return JS_TRUE;

	len = strlen(inbuf) + 1;

	if ((outbuf = (char*)malloc(len)) == NULL) {
		JS_ReportError(cx, "Error allocating %lu bytes at %s:%d", len, getfname(__FILE__), __LINE__);
		free(inbuf);
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	res = b64_decode(outbuf, len, inbuf, strlen(inbuf));
	free(inbuf);
	JS_RESUMEREQUEST(cx, rc);

	if (res < 1) {
		free(outbuf);
		return JS_TRUE;
	}

	js_str = JS_NewStringCopyN(cx, outbuf, res);
	free(outbuf);
	if (js_str == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}

static JSBool
js_md5_calc(JSContext* cx, uintN argc, jsval* arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	BYTE       digest[MD5_DIGEST_SIZE];
	JSBool     hex = JS_FALSE;
	size_t     inbuf_len;
	char*      inbuf = NULL;
	char       outbuf[64];
	JSString*  js_str;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], inbuf, &inbuf_len);
	HANDLE_PENDING(cx, inbuf);
	if (inbuf == NULL)
		return JS_TRUE;

	if (argc > 1 && JSVAL_IS_BOOLEAN(argv[1]))
		hex = JSVAL_TO_BOOLEAN(argv[1]);

	rc = JS_SUSPENDREQUEST(cx);
	MD5_calc(digest, inbuf, inbuf_len);
	free(inbuf);

	if (hex)
		MD5_hex(outbuf, digest);
	else
		b64_encode(outbuf, sizeof(outbuf), (char*)digest, sizeof(digest));
	JS_RESUMEREQUEST(cx, rc);

	js_str = JS_NewStringCopyZ(cx, outbuf);
	if (js_str == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}

static JSBool
js_sha1_calc(JSContext* cx, uintN argc, jsval* arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	BYTE       digest[SHA1_DIGEST_SIZE];
	JSBool     hex = JS_FALSE;
	size_t     inbuf_len;
	char*      inbuf = NULL;
	char       outbuf[64];
	JSString*  js_str;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], inbuf, &inbuf_len);
	HANDLE_PENDING(cx, inbuf);
	if (inbuf == NULL)
		return JS_TRUE;

	if (argc > 1 && JSVAL_IS_BOOLEAN(argv[1]))
		hex = JSVAL_TO_BOOLEAN(argv[1]);

	rc = JS_SUSPENDREQUEST(cx);
	SHA1_calc(digest, inbuf, inbuf_len);
	free(inbuf);

	if (hex)
		SHA1_hex(outbuf, digest);
	else
		b64_encode(outbuf, sizeof(outbuf), (char*)digest, sizeof(digest));
	JS_RESUMEREQUEST(cx, rc);

	js_str = JS_NewStringCopyZ(cx, outbuf);
	if (js_str == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}

static JSBool
js_internal_charfunc(JSContext *cx, uintN argc, jsval *arglist, char *(*func)(char *), unsigned extra_bytes)
{
	jsval *   argv = JS_ARGV(cx, arglist);
	char *    str = NULL, *rastr, *funcret;
	JSString* js_str;
	size_t    strlen;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (JSVAL_NULL_OR_VOID(argv[0])) {
		JS_SET_RVAL(cx, arglist, argv[0]);
		return JS_TRUE;
	}
	JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	JSVALUE_TO_MSTRING(cx, argv[0], str, &strlen);
	HANDLE_PENDING(cx, str);
	if (str == NULL)
		return JS_TRUE;
	if (extra_bytes) {
		rastr = static_cast<char *>(realloc(str, strlen + extra_bytes + 1 /* for terminator */));
		if (rastr == NULL)
			goto error;
		str = rastr;
	}

	funcret = func(str);
	if (funcret) {
		js_str = JS_NewStringCopyZ(cx, funcret);
		if (js_str == NULL)
			goto error;
		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	}
	free(str);

	return JS_TRUE;

error:
	free(str);
	return JS_FALSE;
}

static JSBool
js_rot13(JSContext *cx, uintN argc, jsval *arglist)
{
	return js_internal_charfunc(cx, argc, arglist, rot13, 0);
}

static JSBool
js_skipsp(JSContext *cx, uintN argc, jsval *arglist)
{
	return js_internal_charfunc(cx, argc, arglist, skipsp, 0);
}

static JSBool
js_truncsp(JSContext *cx, uintN argc, jsval *arglist)
{
	return js_internal_charfunc(cx, argc, arglist, truncsp, 0);
}

static JSBool
js_backslash(JSContext *cx, uintN argc, jsval *arglist)
{
	return js_internal_charfunc(cx, argc, arglist, backslash, 1);
}

static char *nonconst_getfname(char *c)
{
	return getfname(c);
}

static JSBool
js_getfname(JSContext *cx, uintN argc, jsval *arglist)
{
	return js_internal_charfunc(cx, argc, arglist, nonconst_getfname, 0);
}

static char *nonconst_getfext(char *c)
{
	return getfext(c);
}

static JSBool
js_getfext(JSContext *cx, uintN argc, jsval *arglist)
{
	return js_internal_charfunc(cx, argc, arglist, nonconst_getfext, 0);
}

static JSBool
js_truncstr(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *   argv = JS_ARGV(cx, arglist);
	char*     str = NULL;
	char*     set;
	JSString* js_str;

	if (js_argcIsInsufficient(cx, argc, 2))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 1))
		return JS_FALSE;
	if (JSVAL_NULL_OR_VOID(argv[0])) {
		JS_SET_RVAL(cx, arglist, argv[0]);
		return JS_TRUE;
	}
	JSVALUE_TO_MSTRING(cx, argv[0], str, NULL);
	HANDLE_PENDING(cx, str);
	if (str == NULL)
		return JS_TRUE;

	JSVALUE_TO_MSTRING(cx, argv[1], set, NULL);
	if (JS_IsExceptionPending(cx)) {
		free(str);
		FREE_AND_NULL(set);
		return JS_FALSE;
	}
	if (set == NULL) {
		free(str);
		return JS_TRUE;
	}

	truncstr(str, set);
	free(set);

	js_str = JS_NewStringCopyZ(cx, str);
	free(str);
	if (js_str == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}

static JSBool
js_fullpath(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char       path[MAX_PATH + 1];
	char*      str = NULL;
	JSString*  js_str;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], str, NULL);
	HANDLE_PENDING(cx, str);
	if (str == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	_fullpath(path, str, sizeof(path));
	free(str);
	JS_RESUMEREQUEST(cx, rc);

	if ((js_str = JS_NewStringCopyZ(cx, path)) == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}


static JSBool
js_getfcase(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char       path[MAX_PATH + 1];
	JSString*  js_str;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);
	JSVALUE_TO_STRBUF(cx, argv[0], path, sizeof(path), NULL);

	rc = JS_SUSPENDREQUEST(cx);
	if (fexistcase(path)) {
		JS_RESUMEREQUEST(cx, rc);
		js_str = JS_NewStringCopyZ(cx, path);
		if (js_str == NULL)
			return JS_FALSE;

		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	}
	else
		JS_RESUMEREQUEST(cx, rc);
	return JS_TRUE;
}

static JSBool
js_dosfname(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
#if defined(_WIN32)
	char*      str = NULL;
	char       path[MAX_PATH + 1];
	JSString*  js_str;
	jsrefcount rc;
#endif

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

#if defined(_WIN32)

	JSVALUE_TO_MSTRING(cx, argv[0], str, NULL);
	HANDLE_PENDING(cx, str);
	if (str == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	if (GetShortPathName(str, path, sizeof(path))) {
		free(str);
		JS_RESUMEREQUEST(cx, rc);
		js_str = JS_NewStringCopyZ(cx, path);
		if (js_str == NULL)
			return JS_FALSE;

		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	}
	else {
		free(str);
		JS_RESUMEREQUEST(cx, rc);
	}

#else   /* No non-Windows equivalent */

	JS_SET_RVAL(cx, arglist, argv[0]);

#endif

	return JS_TRUE;
}

static JSBool
js_cfgfname(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      path = NULL;
	char*      fname;
	char       result[MAX_PATH + 1];
	char*      cstr;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 2))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0) || js_argvIsNullOrVoid(cx, argv, 1))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], path, NULL);
	HANDLE_PENDING(cx, path);
	if (path == NULL)
		return JS_TRUE;

	JSVALUE_TO_MSTRING(cx, argv[1], fname, NULL);
	if (fname == NULL) {
		free(path);
		return JS_TRUE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	cstr = iniFileName(result, sizeof(result), path, fname);
	free(fname);
	free(path);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, cstr)));

	return JS_TRUE;
}

static JSBool
js_fexist(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      p = NULL;
	jsrefcount rc;
	BOOL       fe;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
	if (JSVAL_NULL_OR_VOID(argv[0]))
		return JS_TRUE;

	JSVALUE_TO_MSTRING(cx, argv[0], p, NULL)
	HANDLE_PENDING(cx, p);
	if (p == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	fe = fexist(p);
	free(p);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(fe));
	return JS_TRUE;
}

static JSBool
js_removecase(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      p = NULL;
	jsrefcount rc;
	BOOL       ret;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], p, NULL)
	HANDLE_PENDING(cx, p);
	if (p == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	ret = removecase(p) == 0;
	free(p);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));
	return JS_TRUE;
}

static JSBool
js_remove(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      p = NULL;
	jsrefcount rc;
	BOOL       ret;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], p, NULL)
	HANDLE_PENDING(cx, p);
	if (p == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	ret = remove(p) == 0;
	free(p);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));
	return JS_TRUE;
}

static JSBool
js_rename(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      oldname = NULL;
	char*      newname = NULL;
	jsrefcount rc;
	BOOL       ret;

	if (js_argcIsInsufficient(cx, argc, 2))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0) || js_argvIsNullOrVoid(cx, argv, 1))
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(JS_FALSE));
	JSVALUE_TO_MSTRING(cx, argv[0], oldname, NULL);
	HANDLE_PENDING(cx, oldname);
	if (oldname == NULL)
		return JS_TRUE;
	JSVALUE_TO_MSTRING(cx, argv[1], newname, NULL);
	if (JS_IsExceptionPending(cx)) {
		free(oldname);
		if (newname != NULL)
			free(newname);
		return JS_FALSE;
	}
	if (newname == NULL) {
		free(oldname);
		return JS_TRUE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	ret = rename(oldname, newname) == 0;
	free(oldname);
	free(newname);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));
	return JS_TRUE;
}

static JSBool
js_fcopy(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      src = NULL;
	char*      dest;
	jsrefcount rc;
	BOOL       ret;

	if (js_argcIsInsufficient(cx, argc, 2))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0) || js_argvIsNullOrVoid(cx, argv, 1))
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(JS_FALSE));
	JSVALUE_TO_MSTRING(cx, argv[0], src, NULL);
	HANDLE_PENDING(cx, src);
	if (src == NULL)
		return JS_TRUE;
	JSVALUE_TO_MSTRING(cx, argv[1], dest, NULL);
	if (JS_IsExceptionPending(cx)) {
		free(src);
		FREE_AND_NULL(dest);
		return JS_FALSE;
	}
	if (dest == NULL) {
		free(src);
		return JS_TRUE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	ret = CopyFile(src, dest, /* failIfExists: */ FALSE);
	free(src);
	free(dest);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));
	return JS_TRUE;
}

static JSBool
js_fcompare(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      fn1 = NULL;
	char*      fn2 = NULL;
	jsrefcount rc;
	BOOL       ret;

	if (js_argcIsInsufficient(cx, argc, 2))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0) || js_argvIsNullOrVoid(cx, argv, 1))
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(JS_FALSE));
	JSVALUE_TO_MSTRING(cx, argv[0], fn1, NULL);
	HANDLE_PENDING(cx, fn1);
	if (fn1 == NULL)
		return JS_TRUE;
	JSVALUE_TO_MSTRING(cx, argv[1], fn2, NULL);
	if (JS_IsExceptionPending(cx)) {
		FREE_AND_NULL(fn1);
		FREE_AND_NULL(fn2);
		return JS_FALSE;
	}
	if (fn2 == NULL) {
		FREE_AND_NULL(fn1);
		FREE_AND_NULL(fn2);
		return JS_TRUE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	ret = fcompare(fn1, fn2);
	free(fn1);
	free(fn2);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));
	return JS_TRUE;
}

static JSBool
js_backup(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      fname = NULL;
	int32      level = 5;
	BOOL       ren = FALSE;
	jsrefcount rc;
	BOOL       ret;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(JS_FALSE));
	JSVALUE_TO_MSTRING(cx, argv[0], fname, NULL);
	HANDLE_PENDING(cx, fname);
	if (fname == NULL)
		return JS_TRUE;

	if (argc > 1) {
		if (!JS_ValueToInt32(cx, argv[1], &level)) {
			free(fname);
			return JS_FALSE;
		}
	}
	if (argc > 2) {
		if (!JS_ValueToBoolean(cx, argv[2], &ren)) {
			free(fname);
			return JS_FALSE;
		}
	}

	rc = JS_SUSPENDREQUEST(cx);
	ret = backup(fname, level, ren);
	free(fname);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));
	return JS_TRUE;
}

static JSBool
js_isdir(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      p = NULL;
	jsrefcount rc;
	BOOL       ret;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
	if (JSVAL_NULL_OR_VOID(argv[0]))
		return JS_TRUE;

	JSVALUE_TO_MSTRING(cx, argv[0], p, NULL)
	HANDLE_PENDING(cx, p);
	if (p == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	ret = isdir(p);
	free(p);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));
	return JS_TRUE;
}

static JSBool
js_fattr(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      p = NULL;
	jsrefcount rc;
	int        attr;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], p, NULL)
	HANDLE_PENDING(cx, p);
	if (p == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	attr = getfattr(p);
	free(p);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(attr));
	return JS_TRUE;
}

static JSBool
js_fmode(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      fname = NULL;
	jsrefcount rc;
	int        mode = -1;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], fname, NULL)
	HANDLE_PENDING(cx, fname);
	if (fname == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	struct stat st {};
	if (stat(fname, &st) == 0)
		mode = st.st_mode;
	free(fname);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(mode));
	return JS_TRUE;
}

static JSBool
js_chmod(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      fname = NULL;
	jsrefcount rc;
	int32      mode;

	if (js_argcIsInsufficient(cx, argc, 2))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], fname, NULL)
	HANDLE_PENDING(cx, fname);
	if (fname == NULL)
		return JS_TRUE;

	if (!JS_ValueToInt32(cx, argv[1], &mode)) {
		free(fname);
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	int result = CHMOD(fname, mode);
	free(fname);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(result == 0));
	return JS_TRUE;
}

static JSBool
js_fdate(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      p = NULL;
	time_t     fd;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], p, NULL)
	HANDLE_PENDING(cx, p);
	if (p == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	fd = fdate(p);
	free(p);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, DOUBLE_TO_JSVAL((double)fd));
	return JS_TRUE;
}

static JSBool
js_fcdate(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      p = NULL;
	time_t     fd;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], p, NULL)
	HANDLE_PENDING(cx, p);
	if (p == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	fd = fcdate(p);
	free(p);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, DOUBLE_TO_JSVAL((double)fd));
	return JS_TRUE;
}

static JSBool
js_utime(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *         argv = JS_ARGV(cx, arglist);
	char*           fname = NULL;
	int32           actime;
	int32           modtime;
	struct utimbuf  ut;
	struct utimbuf* tp = NULL;
	jsrefcount      rc;
	BOOL            ret;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], fname, NULL);
	HANDLE_PENDING(cx, fname);
	if (fname == NULL)
		return JS_TRUE;

	if (argc > 1) {
		actime = modtime = time32(NULL);
		if (!JS_ValueToInt32(cx, argv[1], &actime)) {
			free(fname);
			return JS_FALSE;
		}
		if (argc > 2) {
			if (!JS_ValueToInt32(cx, argv[2], &modtime)) {
				free(fname);
				return JS_FALSE;
			}
		}
		ut.actime = actime;
		ut.modtime = modtime;
		tp = &ut;
	}

	rc = JS_SUSPENDREQUEST(cx);
	ret = utime(fname, tp) == 0;
	free(fname);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));

	return JS_TRUE;
}


static JSBool
js_flength(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      p = NULL;
	off_t      fl;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(-1));
	if (JSVAL_NULL_OR_VOID(argv[0]))
		return JS_TRUE;

	JSVALUE_TO_MSTRING(cx, argv[0], p, NULL)
	HANDLE_PENDING(cx, p);
	if (p == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	fl = flength(p);
	free(p);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, DOUBLE_TO_JSVAL((double)fl));
	return JS_TRUE;
}


static JSBool
js_ftouch(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      fname = NULL;
	jsrefcount rc;
	BOOL       ret;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], fname, NULL);
	HANDLE_PENDING(cx, fname);
	if (fname == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	ret = ftouch(fname);
	free(fname);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));
	return JS_TRUE;
}

static JSBool
js_fmutex(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      fname = NULL;
	char*      text = NULL;
	int32      max_age = 0;
	uintN      argn = 0;
	jsrefcount rc;
	BOOL       ret;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[argn], fname, NULL);
	argn++;
	HANDLE_PENDING(cx, fname);
	if (fname == NULL)
		return JS_TRUE;
	if (argc > argn && JSVAL_IS_STRING(argv[argn])) {
		JSVALUE_TO_MSTRING(cx, argv[argn], text, NULL);
		argn++;
		if (JS_IsExceptionPending(cx)) {
			FREE_AND_NULL(text);
			free(fname);
			return JS_FALSE;
		}
	}
	if (argc > argn && JSVAL_IS_NUMBER(argv[argn])) {
		if (!JS_ValueToInt32(cx, argv[argn++], &max_age)) {
			FREE_AND_NULL(text);
			free(fname);
			return JS_FALSE;
		}
	}

	rc = JS_SUSPENDREQUEST(cx);
	ret = fmutex(fname, text, max_age, /* time: */ NULL);
	free(fname);
	if (text)
		free(text);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));
	return JS_TRUE;
}

static JSBool
js_sound(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      p = NULL;
	jsrefcount rc;
	BOOL       ret = FALSE;

	if (!argc) { /* Stop playing sound */
#ifdef _WIN32
		PlaySound(NULL, NULL, 0);
#endif
		JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(JS_TRUE));
		return JS_TRUE;
	}

	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], p, NULL)
	HANDLE_PENDING(cx, p);
	if (p == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
#ifdef _WIN32
	ret = PlaySound(p, NULL, SND_ASYNC | SND_FILENAME);
#endif
	free(p);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));

	return JS_TRUE;
}

static JSBool
js_directory(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	int        i;
	int32      flags = GLOB_MARK;
	char*      p = NULL;
	glob_t     g;
	JSObject*  array;
	JSString*  js_str;
	jsint      len = 0;
	jsval      val;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], p, NULL)
	HANDLE_PENDING(cx, p);
	if (p == NULL)
		return JS_TRUE;

	if (argc > 1) {
		if (!JS_ValueToInt32(cx, argv[1], &flags)) {
			free(p);
			return JS_FALSE;
		}
	}

	if ((array = JS_NewArrayObject(cx, 0, NULL)) == NULL) {
		free(p);
		return JS_FALSE;
	}

	rc = JS_SUSPENDREQUEST(cx);
	glob(p, flags, NULL, &g);
	free(p);
	JS_RESUMEREQUEST(cx, rc);
	for (i = 0; i < (int)g.gl_pathc; i++) {
		if ((js_str = JS_NewStringCopyZ(cx, g.gl_pathv[i])) == NULL)
			break;
		val = STRING_TO_JSVAL(js_str);
		if (!JS_SetElement(cx, array, len++, &val))
			break;
	}
	rc = JS_SUSPENDREQUEST(cx);
	globfree(&g);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(array));

	return JS_TRUE;
}

static JSBool
js_wildmatch(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *     argv = JS_ARGV(cx, arglist);
	BOOL        case_sensitive = FALSE;
	BOOL        path = FALSE;
	char*       fname = NULL;
	const char *spec_def = "*";
	char*       spec = (char *)spec_def;
	uintN       argn = 0;
	jsrefcount  rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);

	if (JSVAL_IS_BOOLEAN(argv[argn]))
		JS_ValueToBoolean(cx, argv[argn++], &case_sensitive);

	JSVALUE_TO_MSTRING(cx, argv[argn], fname, NULL);
	argn++;
	HANDLE_PENDING(cx, fname);
	if (fname == NULL)
		return JS_TRUE;

	if (argn < argc && argv[argn] != JSVAL_VOID) {
		JSVALUE_TO_MSTRING(cx, argv[argn], spec, NULL);
		argn++;
		if (JS_IsExceptionPending(cx)) {
			free(fname);
			if (spec != NULL && spec != spec_def)
				free(spec);
			return JS_FALSE;
		}
		if (spec == NULL) {
			free(fname);
			return JS_TRUE;
		}
	}

	if (argn < argc && argv[argn] != JSVAL_VOID)
		JS_ValueToBoolean(cx, argv[argn++], &path);

	rc = JS_SUSPENDREQUEST(cx);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(wildmatch(fname, spec, path, case_sensitive)));
	free(fname);
	if (spec != spec_def)
		free(spec);
	JS_RESUMEREQUEST(cx, rc);

	return JS_TRUE;
}


static JSBool
js_freediskspace(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	int32      unit = 0;
	char*      p = NULL;
	uint64_t   fd;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], p, NULL)
	HANDLE_PENDING(cx, p);
	if (p == NULL)
		return JS_TRUE;

	if (argc > 1) {
		if (!JS_ValueToInt32(cx, argv[1], &unit)) {
			free(p);
			return JS_FALSE;
		}
	}

	rc = JS_SUSPENDREQUEST(cx);
	fd = getfreediskspace(p, unit);
	free(p);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, DOUBLE_TO_JSVAL((double)fd));

	return JS_TRUE;
}

static JSBool
js_disksize(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	int32      unit = 0;
	char*      p = NULL;
	uint64_t   ds;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], p, NULL)
	HANDLE_PENDING(cx, p);
	if (p == NULL)
		return JS_TRUE;

	if (argc > 1) {
		if (!JS_ValueToInt32(cx, argv[1], &unit)) {
			free(p);
			return JS_FALSE;
		}
	}

	rc = JS_SUSPENDREQUEST(cx);
	ds = getdisksize(p, unit);
	free(p);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, DOUBLE_TO_JSVAL((double)ds));

	return JS_TRUE;
}

static JSBool
js_socket_select(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *             argv = JS_ARGV(cx, arglist);
	JSObject*           inarray[3] = {NULL, NULL, NULL};
	jsuint              inarray_cnt = 0;
	JSObject*           robj;
	JSObject*           rarray;
	BOOL                poll_for_write = FALSE;
#ifdef PREFER_POLL
	struct pollfd *     fds;
	int                 poll_timeout = 0;
	nfds_t              nfds;
	short               events = 0;
	int                 scount;
	int                 k;
#else
	fd_set              socket_set[3];
	fd_set*             sets[3] = {NULL, NULL, NULL};
	SOCKET              maxsock = 0;
	struct      timeval tv = {0, 0};
	SOCKET              sock;
#endif
	uintN               argn;
	jsuint              i;
	jsuint              j;
	jsuint              limit[3];
	jsval               val;
	int                 len = 0;
	jsrefcount          rc;
	BOOL                all_zero = TRUE;
	const char *        props[3] = {"read", "write", "except"};

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);

	for (argn = 0; argn < argc; argn++) {
		if (JSVAL_IS_BOOLEAN(argv[argn]))
			poll_for_write = JSVAL_TO_BOOLEAN(argv[argn]);
		else if (JSVAL_IS_OBJECT(argv[argn]) && !JSVAL_IS_NULL(argv[argn]))
			inarray[inarray_cnt++] = JSVAL_TO_OBJECT(argv[argn]);
#ifdef PREFER_POLL
		else if (JSVAL_IS_NUMBER(argv[argn]))
			poll_timeout = js_polltimeout(cx, argv[argn]);
#else
		else if (JSVAL_IS_NUMBER(argv[argn]))
			js_timeval(cx, argv[argn], &tv);
#endif
	}

	if (inarray_cnt == 0)
		return JS_TRUE;   /* This not a fatal error */
	for (i = 0; i < inarray_cnt; i++) {
		if (!JS_IsArrayObject(cx, inarray[i]))
			return JS_TRUE;   /* This not a fatal error */
		if (JS_GetArrayLength(cx, inarray[i], &limit[i]) != 0)
			all_zero = FALSE;
	}
	if (inarray_cnt > 3)
		inarray_cnt = 3;

	if (all_zero)
		return JS_TRUE;

	if (inarray_cnt == 1) {
		/* Return array */
		if ((robj = JS_NewArrayObject(cx, 0, NULL)) == NULL)
			return JS_FALSE;
#ifdef PREFER_POLL
		// First, count the number of sockets...
		nfds = 0;
		for (i = 0; i < limit[0]; i++) {
			if (!JS_GetElement(cx, inarray[0], i, &val))
				break;
			nfds += js_socket_numsocks(cx, val);
		}
		if (nfds == 0)
			return JS_TRUE;

		fds = static_cast<pollfd *>(calloc(nfds, sizeof(*fds)));
		if (fds == NULL) {
			JS_ReportError(cx, "Error allocating %d elements of %lu bytes at %s:%d"
			               , nfds, sizeof(*fds), getfname(__FILE__), __LINE__);
			return JS_FALSE;
		}
		nfds = 0;
		for (i = 0; i < limit[0]; i++) {
			if (!JS_GetElement(cx, inarray[0], i, &val))
				break;
			nfds += js_socket_add(cx, val, &fds[nfds], poll_for_write ? POLLOUT : POLLIN);
		}

		rc = JS_SUSPENDREQUEST(cx);
		if (poll(fds, nfds, poll_timeout) >= 0) {
			nfds = 0;
			for (i = 0; i < limit[0]; i++) {
				if (!JS_GetElement(cx, inarray[0], i, &val))
					break;
				scount = js_socket_numsocks(cx, val);
				for (k = 0; k < scount; k++) {
					if (fds[nfds + k].revents & (poll_for_write ? POLLOUT : POLLIN | POLLHUP)) {
						val = INT_TO_JSVAL(i);
						JS_RESUMEREQUEST(cx, rc);
						if (!JS_SetElement(cx, robj, len++, &val)) {
							rc = JS_SUSPENDREQUEST(cx);
							break;
						}
						rc = JS_SUSPENDREQUEST(cx);
						break;
					}
				}
				nfds += scount;
			}

			JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(robj));
		}
		free(fds);
#else
		FD_ZERO(&socket_set[0]);
		if (poll_for_write)
			sets[1] = &socket_set[0];
		else
			sets[0] = &socket_set[0];

		for (i = 0; i < limit[0]; i++) {
			if (!JS_GetElement(cx, inarray[0], i, &val))
				break;
			sock = js_socket_add(cx, val, &socket_set[0]);
			if (sock != INVALID_SOCKET) {
				if (sock > maxsock)
					maxsock = sock;
			}
		}

		rc = JS_SUSPENDREQUEST(cx);
		if (select(maxsock + 1, sets[0], sets[1], sets[2], &tv) >= 0) {
			for (i = 0; i < limit[0]; i++) {
				if (!JS_GetElement(cx, inarray[0], i, &val))
					break;
				if (js_socket_isset(cx, val, &socket_set[0])) {
					val = INT_TO_JSVAL(i);
					JS_RESUMEREQUEST(cx, rc);
					if (!JS_SetElement(cx, robj, len++, &val)) {
						rc = JS_SUSPENDREQUEST(cx);
						break;
					}
					rc = JS_SUSPENDREQUEST(cx);
				}
			}

			JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(robj));
		}
#endif
		JS_RESUMEREQUEST(cx, rc);

		return JS_TRUE;
	}
	else {
		/* Return object */
		if ((robj = JS_NewObject(cx, NULL, NULL, NULL)) == NULL)
			return JS_FALSE;
#ifdef PREFER_POLL
		/*
		 * So, we need to collapse all the FDs into a list with flags set...
		 * readfd corresponds to POLLIN
		 * writefd corresponds to POLLOUT
		 * and exceptfd corresponds to POLLPRI
		 *
		 * We don't want duplicates, and we don't want to set things that
		 * weren't requested.
		 *
		 * Brute-force method is to add them all to an array, sort the array,
		 * then remove duplicates after ORing the events together.
		 *
		 * NOTE that some of the socket objects may actually be duplicates
		 * themselves, and all of them should set with the appropriate status.
		 *
		 * Alternatively, we can just add them all in-order and pass a larger
		 * array to poll().  This will certainly be more efficient when
		 * generating the returned arrays.
		 */
		nfds = 0;
		for (j = 0; j < inarray_cnt; j++) {
			if (limit[j] > 0) {
				for (i = 0; i < limit[j]; i++) {
					if (!JS_GetElement(cx, inarray[0], i, &val))
						break;
					nfds += js_socket_numsocks(cx, val);
				}
			}
		}
		if (nfds == 0)
			return JS_TRUE;

		fds = static_cast<pollfd *>(calloc(nfds, sizeof(*fds)));
		if (fds == NULL) {
			JS_ReportError(cx, "Error allocating %d elements of %lu bytes at %s:%d"
			               , nfds, sizeof(*fds), getfname(__FILE__), __LINE__);
			return JS_FALSE;
		}
		nfds = 0;
		for (j = 0; j < inarray_cnt; j++) {
			if (limit[j] > 0) {
				if (inarray_cnt == 1)
					events = poll_for_write ? POLLOUT : POLLIN;
				else {
					switch (j) {
						case 0:
							events = POLLIN;
							break;
						case 1:
							events = POLLOUT;
							break;
						case 2:
							events = POLLPRI;
							break;
					}
				}
				for (i = 0; i < limit[j]; i++) {
					if (!JS_GetElement(cx, inarray[j], i, &val))
						break;
					nfds += js_socket_add(cx, val, &fds[nfds], events);
				}
			}
		}

		rc = JS_SUSPENDREQUEST(cx);
		if (poll(fds, nfds, poll_timeout) >= 0) {
			nfds = 0;
			for (j = 0; j < inarray_cnt; j++) {
				if (limit[j] > 0) {
					switch (j) {
						case 0:
							events = POLLIN | POLLHUP;
							break;
						case 1:
							events = POLLOUT;
							break;
						case 2:
							events = POLLPRI;
							break;
					}
					len = 0;
					JS_RESUMEREQUEST(cx, rc);
					if ((rarray = JS_NewArrayObject(cx, 0, NULL)) == NULL) {
						free(fds);
						return JS_FALSE;
					}
					val = OBJECT_TO_JSVAL(rarray);
					if (!JS_SetProperty(cx, robj, props[j], &val)) {
						free(fds);
						return JS_FALSE;
					}
					rc = JS_SUSPENDREQUEST(cx);
					for (i = 0; i < limit[j]; i++) {
						JS_RESUMEREQUEST(cx, rc);
						if (!JS_GetElement(cx, inarray[j], i, &val)) {
							rc = JS_SUSPENDREQUEST(cx);
							break;
						}
						rc = JS_SUSPENDREQUEST(cx);
						scount = js_socket_numsocks(cx, val);
						for (k = 0; k < scount; k++) {
							if (fds[nfds + k].revents & (events | POLLERR | POLLNVAL)) {
								val = INT_TO_JSVAL(i);
								JS_RESUMEREQUEST(cx, rc);
								if (!JS_SetElement(cx, rarray, len++, &val)) {
									rc = JS_SUSPENDREQUEST(cx);
									break;
								}
								rc = JS_SUSPENDREQUEST(cx);
							}
						}
						nfds += scount;
					}
				}
			}
			JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(robj));
		}
		free(fds);
#else
		for (j = 0; j < inarray_cnt; j++) {
			if (limit[j] > 0) {
				FD_ZERO(&socket_set[j]);
				sets[j] = &socket_set[j];
				for (i = 0; i < limit[j]; i++) {
					if (!JS_GetElement(cx, inarray[j], i, &val))
						break;
					sock = js_socket_add(cx, val, &socket_set[j]);
					if (sock != INVALID_SOCKET) {
						if (sock > maxsock)
							maxsock = sock;
					}
				}
			}
		}

		rc = JS_SUSPENDREQUEST(cx);
		if (select(maxsock + 1, sets[0], sets[1], sets[2], &tv) >= 0) {
			for (j = 0; j < inarray_cnt; j++) {
				if (limit[j] > 0) {
					len = 0;
					JS_RESUMEREQUEST(cx, rc);
					if ((rarray = JS_NewArrayObject(cx, 0, NULL)) == NULL)
						return JS_FALSE;
					val = OBJECT_TO_JSVAL(rarray);
					if (!JS_SetProperty(cx, robj, props[j], &val))
						return JS_FALSE;
					rc = JS_SUSPENDREQUEST(cx);
					for (i = 0; i < limit[j]; i++) {
						JS_RESUMEREQUEST(cx, rc);
						if (!JS_GetElement(cx, inarray[j], i, &val)) {
							rc = JS_SUSPENDREQUEST(cx);
							break;
						}
						rc = JS_SUSPENDREQUEST(cx);
						if (js_socket_isset(cx, val, &socket_set[j])) {
							val = INT_TO_JSVAL(i);
							JS_RESUMEREQUEST(cx, rc);
							if (!JS_SetElement(cx, rarray, len++, &val)) {
								rc = JS_SUSPENDREQUEST(cx);
								break;
							}
							rc = JS_SUSPENDREQUEST(cx);
						}
					}
				}
			}
			JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(robj));
		}
#endif
		JS_RESUMEREQUEST(cx, rc);

		return JS_TRUE;
	}
}

static JSBool
js_socket_strerror(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *   argv = JS_ARGV(cx, arglist);
	int32     err = 0;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;
	if (!JS_ValueToInt32(cx, argv[0], &err))
		return JS_FALSE;

	char      str[256];
	JSString* js_str;
	if ((js_str = JS_NewStringCopyZ(cx, socket_strerror(err, str, sizeof(str)))) == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}

static JSBool
js_strerror(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *   argv = JS_ARGV(cx, arglist);
	int32     err = 0;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;
	if (!JS_ValueToInt32(cx, argv[0], &err))
		return JS_FALSE;

	char      str[256];
	JSString* js_str;
	if ((js_str = JS_NewStringCopyZ(cx, safe_strerror(err, str, sizeof(str)))) == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}

static JSBool
js_mkdir(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      p = NULL;
	jsrefcount rc;
	BOOL       ret;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], p, NULL)
	HANDLE_PENDING(cx, p);
	if (p == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	ret = MKDIR(p) == 0;
	free(p);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));
	return JS_TRUE;
}

static JSBool
js_mkpath(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      p = NULL;
	jsrefcount rc;
	BOOL       ret;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], p, NULL)
	HANDLE_PENDING(cx, p);
	if (p == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	ret = mkpath(p) == 0;
	free(p);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));
	return JS_TRUE;
}

static JSBool
js_rmdir(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      p = NULL;
	jsrefcount rc;
	BOOL       ret;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], p, NULL)
	HANDLE_PENDING(cx, p);
	if (p == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	ret = rmdir(p) == 0;
	free(p);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(ret));
	return JS_TRUE;
}

static JSBool
js_rmfiles(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      dir = NULL;
	char*      spec = NULL;
	int32      keep = 0;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(-1));

	JSVALUE_TO_MSTRING(cx, argv[0], dir, NULL)
	HANDLE_PENDING(cx, dir);
	if (dir == NULL)
		return JS_TRUE;

	uintN argn = 1;
	if (argn < argc && JSVAL_IS_STRING(argv[argn])) {
		JSVALUE_TO_MSTRING(cx, argv[argn], spec, NULL)
		HANDLE_PENDING(cx, spec);
		if (spec == NULL) {
			free(dir);
			return JS_TRUE;
		}
		argn++;
	}
	if (argn < argc && JSVAL_IS_NUMBER(argv[argn])) {
		if (!JS_ValueToInt32(cx, argv[argn], &keep)) {
			free(dir);
			free(spec);
			return JS_FALSE;
		}
		argn++;
	}

	rc = JS_SUSPENDREQUEST(cx);
	int ret = delfiles(dir, spec, keep);
	free(dir);
	free(spec);
	JS_RESUMEREQUEST(cx, rc);
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(ret));
	return JS_TRUE;
}

static JSBool
js_strftime(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char       str[128];
	char*      fmt = NULL;
	jsdouble   jst = (jsdouble)time(NULL);
	time_t     t;
	struct tm  tm;
	JSString*  js_str;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], fmt, NULL);
	HANDLE_PENDING(cx, fmt);
	if (fmt == NULL)
		return JS_TRUE;

	if (argc > 1) {
		if (!JS_ValueToNumber(cx, argv[1], &jst)) {
			free(fmt);
			return JS_FALSE;
		}
	}

	rc = JS_SUSPENDREQUEST(cx);
	strcpy(str, "-Invalid time-");
	t = (time_t)jst;
	if (localtime_r(&t, &tm) != NULL)
		strftime(str, sizeof(str), fmt, &tm);
	free(fmt);
	JS_RESUMEREQUEST(cx, rc);

	if ((js_str = JS_NewStringCopyZ(cx, str)) == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}

/* TODO: IPv6 */
static JSBool
js_resolve_ip(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *         argv = JS_ARGV(cx, arglist);
	JSString*       str;
	char*           p = NULL;
	jsrefcount      rc;
	struct addrinfo hints, *res, *cur;
	char            ip_str[INET6_ADDRSTRLEN];
	BOOL            want_array = FALSE;
	JSObject *      rarray;
	unsigned        alen = 0;
	uintN           argn;
	jsval           val;
	int             result;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);
	for (argn = 0; argn < argc; argn++) {
		if (JSVAL_IS_BOOLEAN(argv[argn]))
			want_array = JSVAL_TO_BOOLEAN(argv[argn]);
		else if (JSVAL_IS_STRING(argv[argn])) {
			if (p)
				free(p);
			JSVALUE_TO_MSTRING(cx, argv[argn], p, NULL)
			HANDLE_PENDING(cx, p);
		}
	}
	if (p == NULL)
		return JS_TRUE;
	truncsp(p);
	if (*p == '\0') {
		// Defeat Windows feature:
		// If the pNodeName parameter contains an empty string, all registered addresses on the local computer are returned.
		free(p);
		return JS_TRUE;
	}
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_ADDRCONFIG;
	hints.ai_socktype = SOCK_STREAM;
	rc = JS_SUSPENDREQUEST(cx);
	if ((result = getaddrinfo(p, NULL, &hints, &res)) != 0) {
		JS_RESUMEREQUEST(cx, rc);
		free(p);
		return JS_TRUE;
	}
	free(p);

	if (want_array) {
		JS_RESUMEREQUEST(cx, rc);
		if ((rarray = JS_NewArrayObject(cx, 0, NULL)) == NULL) {
			freeaddrinfo(res);
			return JS_FALSE;
		}
		JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(rarray));
		for (cur = res; cur; cur = cur->ai_next) {
			inet_addrtop((xp_sockaddr *)cur->ai_addr, ip_str, sizeof(ip_str));
			if ((str = JS_NewStringCopyZ(cx, ip_str)) == NULL) {
				freeaddrinfo(res);
				return JS_FALSE;
			}
			val = STRING_TO_JSVAL(str);
			if (!JS_SetElement(cx, rarray, alen++, &val))
				break;
		}
		freeaddrinfo(res);
	}
	else {
		inet_addrtop((xp_sockaddr *)res->ai_addr, ip_str, sizeof(ip_str));
		freeaddrinfo(res);
		JS_RESUMEREQUEST(cx, rc);

		if ((str = JS_NewStringCopyZ(cx, ip_str)) == NULL)
			return JS_FALSE;

		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(str));
	}
	return JS_TRUE;
}


static JSBool
js_resolve_host(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *         argv = JS_ARGV(cx, arglist);
	char*           p = NULL;
	jsrefcount      rc;
	struct addrinfo hints, *res;
	char            host_name[256];

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, JSVAL_NULL);
	JSVALUE_TO_MSTRING(cx, argv[0], p, NULL)
	HANDLE_PENDING(cx, p);
	if (p == NULL)
		return JS_TRUE;
	truncsp(p);
	if (*p == '\0') {
		// Defeat Windows feature:
		// If the pNodeName parameter contains an empty string, all registered addresses on the local computer are returned.
		free(p);
		return JS_TRUE;
	}
	rc = JS_SUSPENDREQUEST(cx);
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_NUMERICHOST;
	if (getaddrinfo(p, NULL, NULL, &res) != 0) {
		free(p);
		JS_RESUMEREQUEST(cx, rc);
		return JS_TRUE;
	}
	free(p);

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = NI_NAMEREQD;
	if (getnameinfo(res->ai_addr, res->ai_addrlen, host_name, sizeof(host_name), NULL, 0, NI_NAMEREQD) != 0) {
		JS_RESUMEREQUEST(cx, rc);
		freeaddrinfo(res);
		return JS_TRUE;
	}
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, host_name)));
	freeaddrinfo(res);

	return JS_TRUE;

}

extern link_list_t named_queues;    /* js_queue.c */

static JSBool
js_list_named_queues(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject*    array;
	jsint        len = 0;
	jsval        val;
	list_node_t* node;
	msg_queue_t* q;
	jsrefcount   rc;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if ((array = JS_NewArrayObject(cx, 0, NULL)) == NULL)
		return JS_FALSE;

	rc = JS_SUSPENDREQUEST(cx);
	listLock(&named_queues);
	for (node = named_queues.first; node != NULL; node = node->next) {
		if ((q = static_cast<msg_queue_t *>(listNodeData(node))) == NULL)
			continue;
		JS_RESUMEREQUEST(cx, rc);
		val = STRING_TO_JSVAL(JS_NewStringCopyZ(cx, q->name));
		if (!JS_SetElement(cx, array, len++, &val)) {
			rc = JS_SUSPENDREQUEST(cx);
			break;
		}
		rc = JS_SUSPENDREQUEST(cx);
	}
	listUnlock(&named_queues);
	JS_RESUMEREQUEST(cx, rc);

	JS_SET_RVAL(cx, arglist, OBJECT_TO_JSVAL(array));

	return JS_TRUE;
}

static JSBool
js_flags_str(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *   argv = JS_ARGV(cx, arglist);
	char*     p = NULL;
	char      str[64];
	jsdouble  d;
	JSString* js_str;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	if (JSVAL_IS_STRING(argv[0])) {  /* string to long */
		JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(argv[0]), p, NULL);
		HANDLE_PENDING(cx, p);
		if (p == NULL)
			return JS_TRUE;

		JS_SET_RVAL(cx, arglist, DOUBLE_TO_JSVAL((double)aftou32(p)));
		free(p);
		return JS_TRUE;
	}

	/* number to string */
	if (JS_ValueToNumber(cx, argv[0], &d)) {

		if ((js_str = JS_NewStringCopyZ(cx, u32toaf((uint32_t)d, str))) == NULL)
			return JS_FALSE;

		JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	}
	return JS_TRUE;
}

static bool
str_is_utf16(JSContext *cx, jsval val)
{
	if (JSVAL_NULL_OR_VOID(val))
		return false;

	JSString*      js_str = JS_ValueToString(cx, val);
	if (js_str == NULL)
		return false;

	size_t         len;
	const jschar * str = JS_GetStringCharsAndLength(cx, js_str, &len);
	if (str == NULL)
		return false;

	bool           result = false;
	for (size_t i = 0; i < len; i++) {
		if (str[i] > 0xff)
			result = true;
	}
	return result;
}

static JSBool
js_utf8_encode(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	size_t     len;
	char*      outbuf;
	JSString*  js_str;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (JSVAL_NULL_OR_VOID(argv[0])) {
		JS_SET_RVAL(cx, arglist, argv[0]);
		return JS_TRUE;
	}
	if (JSVAL_IS_STRING(argv[0])) {
		if (str_is_utf16(cx, argv[0])) {
			js_str = JS_ValueToString(cx, argv[0]);
			if (js_str == NULL)
				return JS_TRUE;

			size_t         inbuf_len;
			const jschar * inbuf = JS_GetStringCharsAndLength(cx, js_str, &inbuf_len);
			if (inbuf == NULL)
				return JS_TRUE;

			len = (inbuf_len * UTF8_MAX_LEN) + 1;

			if ((outbuf = static_cast<char *>(malloc(len))) == NULL) {
				JS_ReportError(cx, "Error allocating %lu bytes at %s:%d"
				               , len, getfname(__FILE__), __LINE__);
				return JS_FALSE;
			}

			rc = JS_SUSPENDREQUEST(cx);
			size_t outlen = 0;
			for (size_t i = 0; i < inbuf_len; i++) {
				int retval = utf8_putc(outbuf + outlen, len - outlen, static_cast<unicode_codepoint>(inbuf[i]));
				if (retval < 1)
					break;
				outlen += retval;
			}
			outbuf[outlen] = 0;
			JS_RESUMEREQUEST(cx, rc);
		} else {
			size_t inbuf_len;
			char*  inbuf = NULL;

			JSVALUE_TO_MSTRING(cx, argv[0], inbuf, &inbuf_len);
			HANDLE_PENDING(cx, inbuf);
			if (inbuf == NULL)
				return JS_TRUE;

			len = (inbuf_len * UTF8_MAX_LEN) + 1;

			if ((outbuf = static_cast<char *>(malloc(len))) == NULL) {
				free(inbuf);
				JS_ReportError(cx, "Error allocating %lu bytes at %s:%d"
				               , len, getfname(__FILE__), __LINE__);
				return JS_FALSE;
			}

			rc = JS_SUSPENDREQUEST(cx);
			cp437_to_utf8_str(inbuf, outbuf, len, /* minval: */ 0x80);
			free(inbuf);
			JS_RESUMEREQUEST(cx, rc);
		}
	}
	else if (JSVAL_IS_NUMBER(argv[0])) {
		len = UTF8_MAX_LEN + 1;
		if ((outbuf = static_cast<char *>(malloc(len))) == NULL) {
			JS_ReportError(cx, "Error allocating %lu bytes at %s:%d"
			               , len, getfname(__FILE__), __LINE__);
			return JS_FALSE;
		}
		int32 codepoint = 0;
		if (!JS_ValueToInt32(cx, argv[0], &codepoint)) {
			free(outbuf);
			return JS_FALSE;
		}
		int result = utf8_putc(outbuf, len - 1, static_cast<unicode_codepoint>(codepoint));
		if (result < 1) {
			free(outbuf);
			JS_ReportError(cx, "utf8_encode: error: %d for codepoint %X", result, codepoint);
			return JS_FALSE;
		}
		outbuf[result] = 0;
	}
	else {
		JS_ReportError(cx, "utf8_encode: Invalid argument type");
		return JS_FALSE;
	}

	js_str = JS_NewStringCopyZ(cx, outbuf);
	free(outbuf);
	if (js_str == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}

static JSBool
js_utf8_decode(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      buf = NULL;
	JSString*  js_str;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (JSVAL_NULL_OR_VOID(argv[0])) {
		JS_SET_RVAL(cx, arglist, argv[0]);
		return JS_TRUE;
	}
	JSVALUE_TO_MSTRING(cx, argv[0], buf, NULL);
	HANDLE_PENDING(cx, buf);
	if (buf == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	utf8_to_cp437_inplace(buf);
	JS_RESUMEREQUEST(cx, rc);

	js_str = JS_NewStringCopyZ(cx, buf);
	free(buf);
	if (js_str == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}

static JSBool
js_utf8_get_width(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      str = NULL;
	jsrefcount rc;
	int        zerowidth = 1;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	if (js_argvIsNullOrVoid(cx, argv, 0))
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], str, NULL);
	HANDLE_PENDING(cx, str);
	if (str == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	size_t width = utf8_str_total_width(str, zerowidth);
	JS_RESUMEREQUEST(cx, rc);

	free(str);
	JS_SET_RVAL(cx, arglist, INT_TO_JSVAL(width));
	return JS_TRUE;
}

static JSBool
js_str_is_utf8(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      str = NULL;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
	if (JSVAL_NULL_OR_VOID(argv[0]))
		return JS_TRUE;

	JSVALUE_TO_MSTRING(cx, argv[0], str, NULL);
	HANDLE_PENDING(cx, str);
	if (str == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	bool result = utf8_str_is_valid(str);
	JS_RESUMEREQUEST(cx, rc);

	free(str);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(result));
	return JS_TRUE;
}

static JSBool
js_str_is_utf16(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *argv = JS_ARGV(cx, arglist);

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
	if (JSVAL_NULL_OR_VOID(argv[0]))
		return JS_TRUE;

	bool result = str_is_utf16(cx, argv[0]);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(result));
	return JS_TRUE;
}

static JSBool
js_str_is_ascii(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      str = NULL;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
	if (JSVAL_NULL_OR_VOID(argv[0]))
		return JS_TRUE;

	JSVALUE_TO_MSTRING(cx, argv[0], str, NULL);
	HANDLE_PENDING(cx, str);
	if (str == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	bool result = str_is_ascii(str);
	JS_RESUMEREQUEST(cx, rc);

	free(str);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(result));
	return JS_TRUE;
}

static JSBool
js_str_has_ctrl(JSContext *cx, uintN argc, jsval *arglist)
{
	jsval *    argv = JS_ARGV(cx, arglist);
	char*      str = NULL;
	jsrefcount rc;

	if (js_argcIsInsufficient(cx, argc, 1))
		return JS_FALSE;
	JS_SET_RVAL(cx, arglist, JSVAL_FALSE);
	if (JSVAL_NULL_OR_VOID(argv[0]))
		return JS_TRUE;

	JSVALUE_TO_MSTRING(cx, argv[0], str, NULL);
	HANDLE_PENDING(cx, str);
	if (str == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	bool result = str_has_ctrl(str);
	JS_RESUMEREQUEST(cx, rc);

	free(str);
	JS_SET_RVAL(cx, arglist, BOOLEAN_TO_JSVAL(result));
	return JS_TRUE;
}


#if 0
static JSBool
js_qwknet_route(JSContext *cx, uintN argc, jsval *arglist)
{
	JSObject *        obj = JS_THIS_OBJECT(cx, arglist);
	jsval *           argv = JS_ARGV(cx, arglist);
	char              path[MAX_PATH + 1];
	char*             str;
	JSString*         js_str;
	jsrefcount        rc;
	global_private_t* p;

	JS_SET_RVAL(cx, arglist, JSVAL_VOID);

	if (argc == 0 || JSVAL_IS_VOID(argv[0]))
		return JS_TRUE;

	if ((p = (global_private_t*)JS_GetPrivate(cx, obj)) == NULL)      /* Will this work?  Ask DM */
		return JS_FALSE;

	JSVALUE_TO_MSTRING(cx, argv[0], str, NULL);
	HANDLE_PENDING(cx);
	if (str == NULL)
		return JS_TRUE;

	rc = JS_SUSPENDREQUEST(cx);
	qwk_route(&p->cfg, str, path, sizeof(path));
	free(str);
	JS_RESUMEREQUEST(cx, rc);

	if ((js_str = JS_NewStringCopyZ(cx, path)) == NULL)
		return JS_FALSE;

	JS_SET_RVAL(cx, arglist, STRING_TO_JSVAL(js_str));
	return JS_TRUE;
}
#endif

static jsSyncMethodSpec js_global_functions[] = {
	{"exit",            js_exit,            0,  JSTYPE_VOID,    "[<i>number</i> exit_code=0]"
	 , JSDOCSTR("Stop script execution, "
		        "optionally setting the global property <tt>exit_code</tt> to the specified numeric value")
	 , 311},
	{"load",            js_load,            1,  JSTYPE_UNDEF
	 , JSDOCSTR("[<i>bool</i> background or <i>object</i> scope,] <i>string</i> filename [,args]")
	 , JSDOCSTR("Load and execute a JavaScript module (<i>filename</i>), "
		        "optionally specifying a target <i>scope</i> object (default: <i>this</i>) "
		        "and a list of arguments to pass to the module (as <i>argv</i>).<br>"
		        "Returns the result (last executed statement) of the executed script "
		        "or a newly created <i>Queue</i> object if <i>background</i> is <tt>true</tt>).<br><br>"
		        "<b>Background</b>:<br>"
		        "When the <i>background</i> parameter is <tt>true</tt>, the loaded script runs in the background "
		        "(in a child thread) but may communicate with the parent "
		        "script/thread by reading from and/or writing to the <i>parent_queue</i> "
		        "(an automatically created <i>Queue</i> object). "
		        "The result (last executed statement) of the executed script "
		        "(or the optional <i>exit_code</i> passed to the <i>exit()</i> function) "
		        "will be automatically written to the <i>parent_queue</i> "
		        "which may be read later by the parent script (using <i>load_result.read()</i>, for example).")
	 , 312},
	{"require",         js_require,         1,  JSTYPE_UNDEF
	 , JSDOCSTR("[<i>object</i> scope,] <i>string</i> filename, propname [,args]")
	 , JSDOCSTR("Load and execute a JavaScript module (<i>filename</i>), "
		        "optionally specifying a target <i>scope</i> object (default: <i>this</i>) "
		        "and a list of arguments to pass to the module (as <i>argv</i>) "
		        "IF AND ONLY IF the property named <i>propname</i> is not defined in "
		        "the target scope (a defined symbol with a value of undefined will not "
		        "cause the script to be loaded).<br>"
		        "Returns the result (last executed statement) of the executed script "
		        "or null if the script is not executed. ")
	 , 317},
	{"sleep",           js_mswait,          0,  JSTYPE_ALIAS },
	{"mswait",          js_mswait,          0,  JSTYPE_NUMBER,  JSDOCSTR("[milliseconds=1]")
	 , JSDOCSTR("Pause execution for the specified number of milliseconds (AKA sleep), returns elapsed duration, in seconds")
	 , 313},
	{"yield",           js_yield,           0,  JSTYPE_VOID,    JSDOCSTR("[forced=true]")
	 , JSDOCSTR("Release current thread time-slice, "
		        "a <i>forced</i> yield will yield to all other pending tasks (lowering CPU utilization), "
		        "a non-<i>forced</i> yield will yield only to pending tasks of equal or higher priority. "
		        "<i>forced</i> defaults to <tt>true</tt>")
	 , 311},
	{"random",          js_random,          1,  JSTYPE_NUMBER,  JSDOCSTR("[max_number=100]")
	 , JSDOCSTR("Return random integer between <tt>0</tt> and <i>max_number</i>-1")
	 , 310},
	{"time",            js_time,            0,  JSTYPE_NUMBER,  ""
	 , JSDOCSTR("Return current time and date in Unix (time_t) format "
		        "(number of seconds since January 1st, 1970 UTC)")
	 , 310},
	{"beep",            js_beep,            0,  JSTYPE_VOID,    JSDOCSTR("[frequency=500] [,duration=500]")
	 , JSDOCSTR("Produce a tone on the local speaker at specified frequency for specified duration (in milliseconds)")
	 , 310},
	{"sound",           js_sound,           0,  JSTYPE_BOOLEAN, JSDOCSTR("[filename]")
	 , JSDOCSTR("Play a waveform (.wav) sound file (currently, on Windows platforms only)")
	 , 310},
	{"ctrl",            js_ctrl,            1,  JSTYPE_STRING,  JSDOCSTR("<i>number</i> or <i>string</i> value")
	 , JSDOCSTR("Return ASCII control character representing character value passed - Example: <tt>ctrl('C')</tt> returns string containing the single character string: <tt>'\\3'</tt>")
	 , 311},
	{"ascii",           js_ascii,           1,  JSTYPE_UNDEF,   JSDOCSTR("[<i>string</i> text] or [<i>number</i> value]")
	 , JSDOCSTR("Convert single character to numeric ASCII value or vice-versa, returns number OR string, or <tt>null</tt> or <tt>undefined</tt> if passed those values")
	 , 310},
	{"ascii_str",       js_ascii_str,       1,  JSTYPE_STRING,  JSDOCSTR("text")
	 , JSDOCSTR("Convert extended-ASCII (CP437) characters in text string to plain US-ASCII equivalent, returns modified string or <tt>null</tt> or <tt>undefined</tt> if passed those values")
	 , 310},
	{"strip_ctrl",      js_strip_ctrl,      1,  JSTYPE_STRING,  JSDOCSTR("text")
	 , JSDOCSTR("Strip all control characters and Ctrl-A (attribute) sequences from string, returns modified string or <tt>null</tt> or <tt>undefined</tt> if passed those values")
	 , 310},
	{"strip_ctrl_a",    js_strip_ctrl_a,    1,  JSTYPE_STRING,  JSDOCSTR("text")
	 , JSDOCSTR("Strip all Ctrl-A (attribute) sequences from string, returns modified string or <tt>null</tt> or <tt>undefined</tt> if passed those values")
	 , 320},
	{"strip_ansi",      js_strip_ansi,      1,  JSTYPE_STRING,  JSDOCSTR("text")
	 , JSDOCSTR("Strip all ANSI terminal control sequences from string, returns modified string or <tt>null</tt> or <tt>undefined</tt> if passed those values")
	 , 31802},
	{"strip_exascii",   js_strip_exascii,   1,  JSTYPE_STRING,  JSDOCSTR("text")
	 , JSDOCSTR("Strip all extended-ASCII characters from string, returns modified string or <tt>null</tt> or <tt>undefined</tt> if passed those values")
	 , 310},
	{"skipsp",          js_skipsp,          1,  JSTYPE_STRING,  JSDOCSTR("text")
	 , JSDOCSTR("Skip (trim) white-space characters off <b>beginning</b> of string, returns modified string or <tt>null</tt> or <tt>undefined</tt> if passed those values")
	 , 315},
	{"truncsp",         js_truncsp,         1,  JSTYPE_STRING,  JSDOCSTR("text")
	 , JSDOCSTR("Truncate (trim) white-space characters off <b>end</b> of string, returns modified string or <tt>null</tt> or <tt>undefined</tt> if passed those values")
	 , 310},
	{"truncstr",        js_truncstr,        2,  JSTYPE_STRING,  JSDOCSTR("text, charset")
	 , JSDOCSTR("Truncate (trim) string at first char in <i>charset</i>, returns modified string or <tt>null</tt> or <tt>undefined</tt> if passed those values")
	 , 310},
	{"lfexpand",        js_lfexpand,        1,  JSTYPE_STRING,  JSDOCSTR("text")
	 , JSDOCSTR("Expand sole line-feeds (LF) to carriage-return/line-feed sequences (CRLF), returns modified string or <tt>null</tt> or <tt>undefined</tt> if passed those values")
	 , 310},
	{"wildmatch",       js_wildmatch,       2,  JSTYPE_BOOLEAN, JSDOCSTR("[<i>bool</i> case_sensitive=false,] filename [,pattern='*'] [,path=false]")
	 , JSDOCSTR("Return <tt>true</tt> if the <i>filename</i> matches the wildcard <i>pattern</i> (wildcard characters supported are '*' and '?'), "
		        "if <i>path</i> is <tt>true</tt>, '*' will not match path delimiter characters (e.g. '/')")
	 , 314},
	{"backslash",       js_backslash,       1,  JSTYPE_STRING,  JSDOCSTR("path")
	 , JSDOCSTR("Return directory path with trailing (platform-specific) path delimiter "
		        "(i.e. \"slash\" or \"backslash\")")
	 , 312},
	{"fullpath",        js_fullpath,        1,  JSTYPE_STRING,  JSDOCSTR("path")
	 , JSDOCSTR("Create and return an absolute or full path name for the specified relative path name.")
	 , 315},
	{"file_getname",    js_getfname,        1,  JSTYPE_STRING,  JSDOCSTR("path/filename")
	 , JSDOCSTR("Return filename portion of passed path string")
	 , 311},
	{"file_getext",     js_getfext,         1,  JSTYPE_STRING,  JSDOCSTR("path/filename")
	 , JSDOCSTR("Return file extension portion of passed path/filename string (including '.') "
		        "or <tt>undefined</tt> if no extension is found")
	 , 311},
	{"file_getcase",    js_getfcase,        1,  JSTYPE_STRING,  JSDOCSTR("path/filename")
	 , JSDOCSTR("Return correct case of filename (long version of filename on Windows) "
		        "or <tt>undefined</tt> if the file doesn't exist")
	 , 311},
	{"file_cfgname",    js_cfgfname,        2,  JSTYPE_STRING,  JSDOCSTR("path, filename")
	 , JSDOCSTR("Return completed configuration filename from supplied <i>path</i> and <i>filename</i>, "
		        "optionally including the local hostname (e.g. <tt>path/file.<i>host</i>.<i>domain</i>.ext</tt> "
		        "or <tt>path/file.<i>host</i>.ext</tt>) if such a variation of the filename exists")
	 , 312},
	{"file_getdosname", js_dosfname,        1,  JSTYPE_STRING,  JSDOCSTR("path/filename")
	 , JSDOCSTR("Return DOS-compatible (Micros~1 shortened) version of specified <i>path/filename</i>"
		        "(on Windows only)<br>"
		        "return unmodified <i>path/filename</i> on other platforms")
	 , 315},
	{"file_exists",     js_fexist,          1,  JSTYPE_BOOLEAN, JSDOCSTR("path/filename")
	 , JSDOCSTR("Verify a file's existence")
	 , 310},
	{"file_remove",     js_remove,          1,  JSTYPE_BOOLEAN, JSDOCSTR("path/filename")
	 , JSDOCSTR("Delete a file")
	 , 310},
	{"file_removecase", js_removecase,          1,  JSTYPE_BOOLEAN, JSDOCSTR("path/filename")
	 , JSDOCSTR("Delete files case insensitively")
	 , 314},
	{"file_rename",     js_rename,          2,  JSTYPE_BOOLEAN, JSDOCSTR("path/oldname, path/newname")
	 , JSDOCSTR("Rename a file, possibly moving it to another directory in the process")
	 , 311},
	{"file_copy",       js_fcopy,           2,  JSTYPE_BOOLEAN, JSDOCSTR("path/source, path/destination")
	 , JSDOCSTR("Copy a file from one directory or filename to another")
	 , 311},
	{"file_backup",     js_backup,          1,  JSTYPE_BOOLEAN, JSDOCSTR("path/filename [,level=5] [,rename=false]")
	 , JSDOCSTR("Back up the specified <i>filename</i> as <tt>filename.<i>number</i>.extension</tt> "
		        "where <i>number</i> is the backup number 0 through <i>level</i>-1 "
		        "(default backup <i>level</i> is 5), "
		        "if <i>rename</i> is <tt>true</tt>, the original file is renamed instead of copied "
		        "(default is <tt>false</tt>)")
	 , 311},
	{"file_isdir",      js_isdir,           1,  JSTYPE_BOOLEAN, JSDOCSTR("path/filename")
	 , JSDOCSTR("Check if specified <i>filename</i> is a directory")
	 , 310},
	{"file_attrib",     js_fattr,           1,  JSTYPE_NUMBER,  JSDOCSTR("path/filename")
	 , JSDOCSTR("Get a file's attributes (same as </i>file_mode()</i> on *nix). "
		        "On Windows, the return value corresponds with <tt>_finddata_t.attrib</tt> "
		        "(includes DOS/Windows file system-specific attributes, like <i>hidden</i>, and <i>archive</i>). "
		        "Returns <tt>-1</tt> if the <i>path/filename</i> does not exist.")
	 , 310},
	{"file_mode",       js_fmode,           1,  JSTYPE_NUMBER,  JSDOCSTR("path/filename")
	 , JSDOCSTR("Get a file's type and mode flags (e.g. read/write/execute permissions). "
		        "The return value corresponds with <tt>struct stat.st_mode</tt>. "
		        "Returns <tt>-1</tt> if the <i>path/filename</i> does not exist.")
	 , 31702},
	{"file_chmod",      js_chmod,           1,  JSTYPE_BOOLEAN, JSDOCSTR("path/filename, number mode")
	 , JSDOCSTR("Set a file's permissions flags. "
		        "The supported <i>mode</i> bit values are system-dependent "
		        "(e.g. Windows only supports setting or clearing the user-write/0x80 mode flag). "
		        "Returns <tt>true</tt> if the requested change was successful.")
	 , 31702},
	{"file_date",       js_fdate,           1,  JSTYPE_NUMBER,  JSDOCSTR("path/filename")
	 , JSDOCSTR("Get a file's last modified date/time (in time_t format). Returns <tt>-1</tt> if the <i>path/filename</i> does not exist.")
	 , 310},
	{"file_cdate",      js_fcdate,          1,  JSTYPE_NUMBER,  JSDOCSTR("path/filename")
	 , JSDOCSTR("Get a file's creation date/time (in time_t format). Returns <tt>-1</tt> if the <i>path/filename</i> does not exist.")
	 , 317},
	{"file_size",       js_flength,         1,  JSTYPE_NUMBER,  JSDOCSTR("path/filename")
	 , JSDOCSTR("Get a file's length (in bytes). Returns <tt>-1</tt> if the <i>path/filename</i> does not exist.")
	 , 310},
	{"file_utime",      js_utime,           3,  JSTYPE_BOOLEAN, JSDOCSTR("path/filename [,access_time=<i>current</i>] [,mod_time=<i>current</i>]")
	 , JSDOCSTR("Change a file's last accessed and modification date/time (in time_t format), "
		        "or change to current time")
	 , 311},
	{"file_touch",      js_ftouch,          1,  JSTYPE_BOOLEAN, JSDOCSTR("path/filename")
	 , JSDOCSTR("Update a file's last modification date/time to current time, "
		        "creating an empty file if it doesn't already exist")
	 , 311},
	{"file_mutex",      js_fmutex,          1,  JSTYPE_BOOLEAN, JSDOCSTR("path/filename [,<i>string</i> text=<i>local_hostname</i>] [,<i>number</i> max_age=0]")
	 , JSDOCSTR("Attempt to create an mutual-exclusion (e.g. lock) file, "
		        "optionally with the contents of <i>text</i>. "
		        "If a non-zero <i>max_age</i> is specified "
		        "and the lock file exists, but is older than this value (in seconds), "
		        "it is presumed stale and removed/over-written")
	 , 312},
	{"file_compare",    js_fcompare,        2,  JSTYPE_BOOLEAN, JSDOCSTR("path/file1, path/file2")
	 , JSDOCSTR("Compare 2 files, returning <tt>true</tt> if they are identical, <tt>false</tt> otherwise")
	 , 314},
	{"directory",       js_directory,       1,  JSTYPE_ARRAY,   JSDOCSTR("path/pattern [,flags=GLOB_MARK]")
	 , JSDOCSTR("Return an array of directory entries, "
		        "<i>pattern</i> is the path and filename or wildcards to search for (e.g. '/subdir/*.txt'), "
		        "<i>flags</i> is a set of optional <tt>glob</tt> bit-flags (default is <tt>GLOB_MARK</tt>)")
	 , 310},
	{"dir_freespace",   js_freediskspace,   2,  JSTYPE_NUMBER,  JSDOCSTR("directory [,unit_size=1]")
	 , JSDOCSTR("Return the amount of available disk space in the specified <i>directory</i> "
		        "using the specified <i>unit_size</i> in bytes (default: 1), "
		        "specify a <i>unit_size</i> of <tt>1000</tt> to return the available space in <i>kilobytes</i> or <tt>1024</t> for <i>kibibytes</i>.")
	 , 311},
	{"disk_size",       js_disksize,        2,  JSTYPE_NUMBER,  JSDOCSTR("directory [,unit_size=1]")
	 , JSDOCSTR("Return the total disk size of the specified <i>directory</i> "
		        "using the specified <i>unit_size</i> in bytes (default: 1), "
		        "specify a <i>unit_size</i> of <tt>1000</tt> to return the total disk size in <i>kilobytes</i> or <tt>1024</t> for <i>kibibytes</i>.")
	 , 314},
	{"socket_select",   js_socket_select,   0,  JSTYPE_ARRAY,   JSDOCSTR("[array of socket objects or descriptors] [,<i>number</i> timeout=0] [,<i>bool</i> write=false]")
	 , JSDOCSTR("Check an array of socket objects or descriptors for read or write ability (default is <i>read</i>), "
		        "default timeout value is 0.0 seconds (immediate timeout), "
		        "returns an array of 0-based index values into the socket array, representing the sockets that were ready for reading or writing, or <tt>null</tt> on error. "
		        "If multiple arrays of sockets are passed, they are presumed to be in the order of read, write, and except.  In this case, the <i>write</i> parameter is ignored "
		        "and an object is returned instead with up to three properties \"read\", \"write\", and \"except\", corresponding to the passed arrays.  Empty passed "
		        "arrays will not have a corresponding property in the returned object.")
	 , 311},
	{"socket_strerror", js_socket_strerror, 1,  JSTYPE_STRING,  JSDOCSTR("error")
	 , JSDOCSTR("Get the description(string representation) of a numeric socket error value (e.g. <tt>socket_errno</tt>)")
	 , 31802},
	{"strerror",        js_strerror,        1,  JSTYPE_STRING,  JSDOCSTR("error")
	 , JSDOCSTR("Get the description(string representation) of a numeric system error value (e.g. <tt>errno</tt>)")
	 , 31802},
	{"mkdir",           js_mkdir,           1,  JSTYPE_BOOLEAN, JSDOCSTR("path/directory")
	 , JSDOCSTR("Make a directory on a local file system")
	 , 310},
	{"mkpath",          js_mkpath,          1,  JSTYPE_BOOLEAN, JSDOCSTR("path/directory")
	 , JSDOCSTR("Make a path to a directory (creating all necessary sub-directories). Returns <tt>true</tt> if the directory already exists.")
	 , 315},
	{"rmdir",           js_rmdir,           1,  JSTYPE_BOOLEAN, JSDOCSTR("path/directory")
	 , JSDOCSTR("Remove a directory")
	 , 310},
	{"rmfiles",         js_rmfiles,         1,  JSTYPE_BOOLEAN, JSDOCSTR("path/directory [,file-spec='*'] [,files-to-keep=0]")
	 , JSDOCSTR("Remove all files and sub-directories in the specified directory, recursively - <b>use with caution!</b>")
	 , 320},
	{"strftime",        js_strftime,        1,  JSTYPE_STRING,  JSDOCSTR("format [,time=<i>current</i>]")
	 , JSDOCSTR("Return a formatted time string (ala the standard C <tt>strftime</tt> function)")
	 , 310},
	{"format",          js_format,          1,  JSTYPE_STRING,  JSDOCSTR("format [,args]")
	 , JSDOCSTR("Return a C-style formatted string (ala the standard C <tt>sprintf</tt> function)")
	 , 310},
	{"html_encode",     js_html_encode,     1,  JSTYPE_STRING,  JSDOCSTR("text [,<i>bool</i> ex_ascii=true] [,<i>bool</i> white_space=true] [,<i>bool</i> ansi=true] [,<i>bool</i> ctrl_a=true] [, state (object)]")
	 , JSDOCSTR("Return an HTML-encoded text string (using standard HTML character entities), "
		        "escaping IBM extended-ASCII (CP437), white-space characters, ANSI codes, and CTRL-A codes by default."
		        "Optionally storing the current ANSI state in <i>state</i> object")
	 , 311},
	{"html_decode",     js_html_decode,     1,  JSTYPE_STRING,  JSDOCSTR("html")
	 , JSDOCSTR("Return a decoded HTML-encoded text string, translating HTML character entities into CP437 character equivalents")
	 , 311},
	{"word_wrap",       js_word_wrap,       1,  JSTYPE_STRING,  JSDOCSTR("text [,line_length=79 [,orig_line_length=79 [,<i>bool</i> handle_quotes=true [,<i>number</i> pmode=0]]]]")
	 , JSDOCSTR("Return a word-wrapped version of the <i>text</i> string argument optionally handing quotes magically, "
		        "<i>line_length</i> defaults to <i>79</i>, <i>orig_line_length</i> defaults to <tt>79</tt>, "
		        "<i>handle_quotes</i> defaults to <tt>true</tt>, <i>pmode</i> defaults to <tt>0</tt>, "
				"<tt>P_UTF8</tt> and <tt>P_RENEGADE</tt> <i>pmode</i> bit-flags are supported and may be optionally specified as a set of <i>bool</i> values."
		        "<p>Note: if the original text does not contain any carriage-return (CR) characters, lines are wrapped with sole line-feed (LF) characters.")
	 , 311},
	{"quote_msg",       js_quote_msg,       1,  JSTYPE_STRING,  JSDOCSTR("text [,line_length=79] [,prefix=\" > \"]")
	 , JSDOCSTR("Return a quoted version of the message <i>text</i> string argument, <i>line_length</i> defaults to <tt>79</tt>, "
		        "<i>prefix</i> defaults to <tt>\" > \"</tt>")
	 , 311},
	{"rot13_translate", js_rot13,           1,  JSTYPE_STRING,  JSDOCSTR("text")
	 , JSDOCSTR("Return ROT13-translated version of text string (will encode or decode text)")
	 , 311},
	{"base64_encode",   js_b64_encode,      1,  JSTYPE_STRING,  JSDOCSTR("text")
	 , JSDOCSTR("Return base64-encoded version of text string or <tt>null</tt> on error")
	 , 311},
	{"base64_decode",   js_b64_decode,      1,  JSTYPE_STRING,  JSDOCSTR("text")
	 , JSDOCSTR("Return base64-decoded text string or <tt>null</tt> on error")
	 , 311},
	{"crc16_calc",      js_crc16,           1,  JSTYPE_NUMBER,  JSDOCSTR("text")
	 , JSDOCSTR("Calculate and return 16-bit CRC of text string")
	 , 311},
	{"crc32_calc",      js_crc32,           1,  JSTYPE_NUMBER,  JSDOCSTR("text")
	 , JSDOCSTR("Calculate and return 32-bit CRC of text string")
	 , 311},
	{"chksum_calc",     js_chksum,          1,  JSTYPE_NUMBER,  JSDOCSTR("text")
	 , JSDOCSTR("Calculate and return 32-bit checksum of text string")
	 , 311},
	{"md5_calc",        js_md5_calc,        1,  JSTYPE_STRING,  JSDOCSTR("text [,<i>bool</i> hex=false]")
	 , JSDOCSTR("Calculate and return 128-bit MD5 digest of text string, result encoded in base64 (default) or hexadecimal (32 hex digits)")
	 , 311},
	{"sha1_calc",       js_sha1_calc,       1,  JSTYPE_STRING,  JSDOCSTR("text [,<i>bool</i> hex=false]")
	 , JSDOCSTR("Calculate and return 160-bit SHA-1 digest of text string, result encoded in base64 (default) or hexadecimal (40 hex digits)")
	 , 31900},
	{"gethostbyname",   js_resolve_ip,      1,  JSTYPE_ALIAS },
	{"resolve_ip",      js_resolve_ip,      1,  JSTYPE_STRING,  JSDOCSTR("<i>string</i> hostname [,<i>bool</i> array=false]")
	 , JSDOCSTR("Resolve IP address of specified hostname (AKA gethostbyname).  If <i>array</i> is <tt>true</tt>, will return "
		        "an array of all addresses rather than just the first one (upon success).  Returns <tt>null</tt> if unable to resolve address.")
	 , 311},
	{"gethostbyaddr",   js_resolve_host,    1,  JSTYPE_ALIAS },
	{"resolve_host",    js_resolve_host,    1,  JSTYPE_STRING,  JSDOCSTR("ip_address")
	 , JSDOCSTR("Resolve hostname of specified IP address (AKA gethostbyaddr).  Returns <tt>null</tt> if unable to resolve address.")
	 , 311},
	{"netaddr_type",    js_netaddr_type,    1,  JSTYPE_NUMBER,  JSDOCSTR("email_address")
	 , JSDOCSTR("Return the proper message <i>net_type</i> for the specified <i>email_address</i>, "
		        "(e.g. <tt>NET_INTERNET</tt> for Internet e-mail or <tt>NET_NONE</tt> for local e-mail)")
	 , 312},
	{"list_named_queues", js_list_named_queues, 0, JSTYPE_ARRAY,   JSDOCSTR("")
	 , JSDOCSTR("Return an array of <i>named queues</i> (created with the <i>Queue</i> constructor)")
	 , 312},
	{"flags_str",       js_flags_str,       1,  JSTYPE_UNDEF,   JSDOCSTR("<i>string</i> or <i>number</i>")
	 , JSDOCSTR("Convert a string of security flags (letters) into their numeric value or vice-versa, "
		        "returns number OR string")
	 , 313},
	{"utf8_encode",     js_utf8_encode,     1,  JSTYPE_STRING,  JSDOCSTR("[<i>string</i> CP437] or [<i>string</i> UTF16] or [<i>number</i> codepoint]")
	 , JSDOCSTR("Return UTF-8 encoded version of the specified CP437 text string, UTF-16 encoded text string, or a single Unicode <i>codepoint</i>")
	 , 31702},
	{"utf8_decode",     js_utf8_decode,     1,  JSTYPE_STRING,  JSDOCSTR("text")
	 , JSDOCSTR("Return CP437 representation of UTF-8 encoded text string or <tt>null</tt> on error (invalid UTF-8)")
	 , 31702},
	{"utf8_get_width",      js_utf8_get_width,  1,  JSTYPE_NUMBER,  JSDOCSTR("text")
	 , JSDOCSTR("Return the fixed printed-width of the specified string of UTF-8 encoded characters")
	 , 31702},
	{"str_is_utf8",         js_str_is_utf8,     1,  JSTYPE_BOOLEAN, JSDOCSTR("text")
	 , JSDOCSTR("Return <tt>true</tt> if the specified string contains only valid UTF-8 encoded and US-ASCII characters")
	 , 31702},
	{"str_is_utf16",        js_str_is_utf16,        1,  JSTYPE_BOOLEAN, JSDOCSTR("text")
	 , JSDOCSTR("Return <tt>true</tt> if the specified string contains one or more UTF-16 encoded characters")
	 , 31702},
	{"str_is_ascii",        js_str_is_ascii,    1,  JSTYPE_BOOLEAN, JSDOCSTR("text")
	 , JSDOCSTR("Return <tt>true</tt> if the specified string contains only US-ASCII (no CP437 or UTF-8) characters")
	 , 31702},
	{"str_has_ctrl",        js_str_has_ctrl,    1,  JSTYPE_BOOLEAN, JSDOCSTR("text")
	 , JSDOCSTR("Return <tt>true</tt> if the specified string contains any control characters (ASCII 0x01 - 0x1F)")
	 , 31702},
	{0}
};

static jsConstIntSpec   js_global_const_ints[] = {
	/* Numeric error constants from errno.h (platform-dependant) */
	{"EPERM", EPERM          },
	{"ENOENT", ENOENT         },
	{"ESRCH", ESRCH          },
	{"EIO", EIO            },
	{"ENXIO", ENXIO          },
	{"E2BIG", E2BIG          },
	{"ENOEXEC", ENOEXEC        },
	{"EBADF", EBADF          },
	{"ECHILD", ECHILD         },
	{"EAGAIN", EAGAIN         },
	{"ENOMEM", ENOMEM         },
	{"EACCES", EACCES         },
	{"EFAULT", EFAULT         },
	{"EBUSY", EBUSY          },
	{"EEXIST", EEXIST         },
	{"EXDEV", EXDEV          },
	{"ENODEV", ENODEV         },
	{"ENOTDIR", ENOTDIR        },
	{"EISDIR", EISDIR         },
	{"EINVAL", EINVAL         },
	{"ENFILE", ENFILE         },
	{"EMFILE", EMFILE         },
	{"ENOTTY", ENOTTY         },
	{"EFBIG", EFBIG          },
	{"ENOSPC", ENOSPC         },
	{"ESPIPE", ESPIPE         },
	{"EROFS", EROFS          },
	{"EMLINK", EMLINK         },
	{"EPIPE", EPIPE          },
	{"EDOM", EDOM           },
	{"ERANGE", ERANGE         },
	{"EDEADLOCK", EDEADLOCK      },
	{"ENAMETOOLONG", ENAMETOOLONG   },
	{"ENOTEMPTY", ENOTEMPTY      },

	/* Socket errors */
	{"EINTR", EINTR          },
	{"ENOTSOCK", ENOTSOCK       },
	{"EMSGSIZE", EMSGSIZE       },
	{"EWOULDBLOCK", EWOULDBLOCK    },
	{"EPROTOTYPE", EPROTOTYPE     },
	{"ENOPROTOOPT", ENOPROTOOPT    },
	{"EPROTONOSUPPORT", EPROTONOSUPPORT},
	{"ESOCKTNOSUPPORT", ESOCKTNOSUPPORT},
	{"EOPNOTSUPP", EOPNOTSUPP     },
	{"EPFNOSUPPORT", EPFNOSUPPORT   },
	{"EAFNOSUPPORT", EAFNOSUPPORT   },
	{"EADDRINUSE", EADDRINUSE     },
	{"EADDRNOTAVAIL", EADDRNOTAVAIL  },
	{"ECONNABORTED", ECONNABORTED   },
	{"ECONNRESET", ECONNRESET     },
	{"ENOBUFS", ENOBUFS        },
	{"EISCONN", EISCONN        },
	{"ENOTCONN", ENOTCONN       },
	{"ESHUTDOWN", ESHUTDOWN      },
	{"ETIMEDOUT", ETIMEDOUT      },
	{"ECONNREFUSED", ECONNREFUSED   },
	{"EINPROGRESS", EINPROGRESS    },

	/* Log priority values from syslog.h/sbbsdefs.h (possibly platform-dependant) */
	{"LOG_EMERG", LOG_EMERG      },
	{"LOG_ALERT", LOG_ALERT      },
	{"LOG_CRIT", LOG_CRIT       },
	{"LOG_ERR", LOG_ERR        },
	{"LOG_ERROR", LOG_ERR        },
	{"LOG_WARNING", LOG_WARNING    },
	{"LOG_NOTICE", LOG_NOTICE     },
	{"LOG_INFO", LOG_INFO       },
	{"LOG_DEBUG", LOG_DEBUG      },

	/* Other useful constants */
	{"INVALID_SOCKET", (int)INVALID_SOCKET },

	/* Terminator (Governor Arnold) */
	{0}
};

static void js_global_finalize(JSContext *cx, JSObject *obj)
{
	global_private_t* p;

	p = (global_private_t*)JS_GetPrivate(cx, obj);

	if (p != NULL) {
		while (p->bg_count) {
			if (sem_wait(&p->bg_sem) == 0)
				p->bg_count--;
		}
		sem_destroy(&p->bg_sem);
		free(p);
	}

	p = NULL;
	JS_SetPrivate(cx, obj, p);
}

static JSBool js_global_resolve(JSContext *cx, JSObject *obj, jsid id)
{
	char*             name = NULL;
	global_private_t* p;
	JSBool            ret = JS_TRUE;

	if ((p = (global_private_t*)JS_GetPrivate(cx, obj)) == NULL)
		return JS_FALSE;

	if (id != JSID_VOID && id != JSID_EMPTY && id != JS_DEFAULT_XML_NAMESPACE_ID) {
		jsval idval;

		JS_IdToValue(cx, id, &idval);
		if (JSVAL_IS_STRING(idval)) {
			JSSTRING_TO_MSTRING(cx, JSVAL_TO_STRING(idval), name, NULL);
			if (JS_IsExceptionPending(cx)) {
				JS_ClearPendingException(cx);
				free(name);
				return JS_FALSE;
			}
		}
	}

	if (p->methods) {
		if (js_SyncResolve(cx, obj, name, NULL, p->methods, NULL, 0) == JS_FALSE)
			ret = JS_FALSE;
	}
	if (js_SyncResolve(cx, obj, name, js_global_properties, js_global_functions, js_global_const_ints, 0) == JS_FALSE)
		ret = JS_FALSE;
	if (name)
		free(name);
	return ret;
}

static JSBool js_global_enumerate(JSContext *cx, JSObject *obj)
{
	return js_global_resolve(cx, obj, JSID_VOID);
}

JSClass js_global_class = {
	"Global"                /* name			*/
	, JSCLASS_HAS_PRIVATE | JSCLASS_GLOBAL_FLAGS   /* flags		*/
	, JS_PropertyStub        /* addProperty	*/
	, JS_PropertyStub        /* delProperty	*/
	, js_system_get          /* getProperty	*/
	, JS_StrictPropertyStub  /* setProperty	*/
	, js_global_enumerate    /* enumerate	*/
	, js_global_resolve      /* resolve		*/
	, JS_ConvertStub         /* convert		*/
	, js_global_finalize     /* finalize		*/
};

bool js_CreateGlobalObject(JSContext* cx, scfg_t* cfg, jsSyncMethodSpec* methods, js_startup_t* startup, JSObject**glob)
{
	global_private_t* p;
	JSRuntime*        rt;

	if ((rt = JS_GetRuntime(cx)) != NULL)
		JS_SetRuntimePrivate(rt, cfg);

	if ((p = (global_private_t*)malloc(sizeof(global_private_t))) == NULL)
		return FALSE;

	p->cfg = cfg;
	p->methods = methods;
	p->startup = startup;
	p->exit_func = NULL;
	p->onexit = NULL;

	if ((*glob = JS_NewCompartmentAndGlobalObject(cx, &js_global_class, NULL)) == NULL) {
		free(p);
		return FALSE;
	}
	if (!JS_AddObjectRoot(cx, glob)) {
		free(p);
		return FALSE;
	}

	if (!JS_SetPrivate(cx, *glob, p)) {  /* Store a pointer to scfg_t and the new methods */
		JS_RemoveObjectRoot(cx, glob);
		free(p);
		return FALSE;
	}

	if (!JS_InitStandardClasses(cx, *glob)) {
		JS_RemoveObjectRoot(cx, glob);
		free(p);
		return FALSE;
	}

	p->bg_count = 0;
	if (sem_init(&p->bg_sem, 0, 0) == -1) {
		JS_RemoveObjectRoot(cx, glob);
		free(p);
		return FALSE;
	}
	// TODO: Deuce, what purpose does this ReservedSlot serve?
	// Removing this had no observable negative impact <shrug>
	if (!JS_SetReservedSlot(cx, *glob, 0, INT_TO_JSVAL(0))) {
		JS_RemoveObjectRoot(cx, glob);
		free(p);
		return FALSE;
	}
#ifdef BUILD_JSDOCS
	js_DescribeSyncObject(cx, *glob
	                      , "Top-level functions and properties (common to all servers, services, and <i>JSexec</i>)", 310);
#endif

	return TRUE;
}
#endif  /* JAVSCRIPT */
