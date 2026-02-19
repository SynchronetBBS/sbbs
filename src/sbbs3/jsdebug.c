/*
 * This doesn't belong here and is done wrong...
 * mcmlxxix should love it.
 */

#include "sbbs.h"
#include "js_request.h"
#include "jsdebug.h"
#include <link_list.h>
#include <dirwrap.h>
#include <jsdbgapi.h>
#include <xpprintf.h>

struct breakpoint {
	JSContext *cx;
	uintN line;
	char name[];
};

struct cur_script {
	JSScript *script;
	char *fname;
	uintN firstline;
	uintN lastline;
};

struct debugger {
	JSContext *cx;
	void (*puts)(const char *);
	char *(*getline)(void);
};

static link_list_t   breakpoints;
static link_list_t   scripts;
static link_list_t   debuggers;
static JSStackFrame *finish;

static JSTrapStatus trap_handler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval, jsval closure);
static JSTrapStatus throw_handler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval, void *closure);
static JSTrapStatus single_step_handler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval, void *closure);
static enum debug_action script_debug_prompt(struct debugger *cx, JSScript *script);
static JSTrapStatus finish_handler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval, void *closure);

static struct debugger *get_debugger(JSContext *cx)
{
	list_node_t *    node;
	struct debugger *dbg;

	for (node = listFirstNode(&debuggers); node; node = listNextNode(node)) {
		dbg = (struct debugger *)node->data;
		if (dbg->cx == cx)
			return dbg;
	}
	return NULL;
}

static void newscript_handler(JSContext  *cx,
                              const char *filename, /* URL of script */
                              uintN lineno, /* first line */
                              JSScript   *script,
                              JSFunction *fun,
                              void       *callerdata)
{
	const char *       fname = NULL;
	struct cur_script *cs;
	list_node_t *      node;
	struct breakpoint *bp;
	jsbytecode *       pc;
	struct debugger *  dbg = get_debugger(cx);
	char *             msg;

	if (dbg == NULL)
		return;
	JS_SetSingleStepMode(dbg->cx, script, JS_TRUE);
	cs = (struct cur_script *)malloc(sizeof(struct cur_script));
	if (!cs) {
		dbg->puts("Error allocating script struct\n");
		return;
	}
	fname = JS_GetScriptFilename(cx, script);
	if (fname == NULL)
		fname = "<unknown>";
	cs->fname = strdup(fname);
	if (cs->fname)
		fname = getfname(fname);
	cs->firstline = lineno;
	cs->lastline = lineno + JS_GetScriptLineExtent(cx, script);
	cs->script = script;

	// Loop through breakpoints.
	for (node = listFirstNode(&breakpoints); node; node = listNextNode(node)) {
		bp = (struct breakpoint *)node->data;
		if (bp->cx != cx)
			continue;
		if (strcmp(fname, bp->name) == 0 || (cs->fname && (strcmp(cs->fname, bp->name) == 0))) {
			if (bp->line >= cs->firstline && bp->line <= cs->lastline) {
				pc = JS_LineNumberToPC(cx, script, bp->line);
				if (pc == NULL) {
					msg = xp_asprintf("NEWSCRIPT: Unable to locate line %u\n", bp->line);
					if (msg) {
						dbg->puts(msg);
						xp_asprintf_free(msg);
					}
					break;
				}
				if (JS_PCToLineNumber(cx, script, pc) != bp->line) {
					continue;
				}
				if (!JS_SetTrap(cx, script, pc, trap_handler, JSVAL_VOID)) {
					msg = xp_asprintf("NEWSCRIPT: Unable to set breakpoint at line %u\n", bp->line);
					if (msg) {
						dbg->puts(msg);
						xp_asprintf_free(msg);
					}
				}
			}
		}
	}
	listPushNode(&scripts, cs);
}

static void killscript_handler(JSContext *cx, JSScript *script, void *callerdata)
{
	list_node_t *      node;
	list_node_t *      pnode;
	struct cur_script *cs;
	struct debugger *  dbg = get_debugger(cx);

	if (dbg == NULL)
		return;
	for (node = listFirstNode(&scripts); node; node == NULL?(node = listFirstNode(&scripts)):(node = listNextNode(node))) {
		cs = (struct cur_script *)node->data;

		if (cs->script == script) {
			pnode = listPrevNode(node);
			free(cs->fname);
			listRemoveNode(&scripts, node, TRUE);
			node = pnode;
		}
	}
}

void setup_debugger(void)
{
	listInit(&breakpoints, LINK_LIST_MUTEX);
	listInit(&scripts, LINK_LIST_MUTEX);
	listInit(&debuggers, LINK_LIST_MUTEX);
}

BOOL init_debugger(JSRuntime *rt, JSContext *cx, void (*puts)(const char *), char *(*getline)(void))
{
	struct debugger *dbg;

	if (get_debugger(cx))
		return FALSE;
	dbg = malloc(sizeof(struct debugger));
	if (!dbg)
		return FALSE;
	dbg->cx = cx;
	dbg->puts = puts;
	dbg->getline = getline;
	listPushNode(&debuggers, dbg);
	JS_SetDebugMode(cx, JS_TRUE);
	JS_SetThrowHook(rt, throw_handler, NULL);
	JS_SetNewScriptHook(rt, newscript_handler, NULL);
	JS_SetDestroyScriptHook(rt, killscript_handler, NULL);
	return TRUE;
}

void end_debugger(JSRuntime *rt, JSContext *cx)
{
	list_node_t *    node;
	struct debugger *dbg;

	JS_SetDebugMode(cx, JS_FALSE);
	JS_SetThrowHook(rt, NULL, NULL);
	JS_SetNewScriptHook(rt, NULL, NULL);
	JS_SetDestroyScriptHook(rt, NULL, NULL);

	for (node = listFirstNode(&debuggers); node; node = listNextNode(node)) {
		dbg = (struct debugger *)node->data;
		if (dbg->cx == cx) {
			listRemoveNode(&debuggers, node, TRUE);
			return;
		}
	}
}

static enum debug_action script_debug_prompt(struct debugger *dbg, JSScript *script)
{
	char            *line = NULL;
	const char      *fpath;
	const char      *fname;
	JSStackFrame    *fp;
	JSFunction      *fn;
	jsbytecode      *pc;
	char            *cp;
	jsrefcount rc;
	char            *msg;
	static char lastline[1024];

	fp = JS_GetScriptedCaller(dbg->cx, NULL);
	while (1) {
		FREE_AND_NULL(line);
		if (JS_IsExceptionPending(dbg->cx))
			dbg->puts("!");
		fpath = JS_GetScriptFilename(dbg->cx, script);
		if (fpath) {
			fname = getfname(fpath);
			dbg->puts(fname);
			if (fp) {
				pc = JS_GetFramePC(dbg->cx, fp);
				if (pc) {
					msg = xp_asprintf(":%u", JS_PCToLineNumber(dbg->cx, script, pc));
					if (msg) {
						dbg->puts(msg);
						xp_asprintf_free(msg);
					}
				}
				fn = JS_GetFrameFunction(dbg->cx, fp);
				if (fn) {
					JSString    *name;

					name = JS_GetFunctionId(fn);
					if (name) {
						JSSTRING_TO_ASTRING(dbg->cx, name, cp, 128, NULL);
						msg = xp_asprintf(" %s()", cp);
						if (msg) {
							dbg->puts(msg);
							xp_asprintf_free(msg);
						}
					}
				}
			}
		}
		dbg->puts("> ");

		rc = JS_SUSPENDREQUEST(dbg->cx);
		if ((line = dbg->getline()) == NULL) {
			dbg->puts("Error readin input\n");
			JS_RESUMEREQUEST(dbg->cx, rc);
			continue;
		}
		JS_RESUMEREQUEST(dbg->cx, rc);
		if (strcmp(line, "\n") == 0) { // Enter key repeats last command
			free(line);
			line = strdup(lastline);
		} else
			snprintf(lastline, sizeof lastline, "%s", line);
		if (strncmp(line, "break ", 6) == 0) {
			ulong linenum = 0;
			jsbytecode          *pc;
			const char          *sname;
			struct cur_script *cs;
			list_node_t         *node;
			struct breakpoint *bp;
			const char          *text;
			char                *num;

			text = line + 6;
			if (isdigit(*text) && strchr(text, '.') == NULL) {
				linenum = strtoul(text, NULL, 10);
				text = fpath;
			}
			else {
				num = (char*)strchr(text, ':');
				if (num) {
					*num = 0;
					num++;
					if (!(isdigit(*num))) {
						dbg->puts("Unable to parse breakpoint\n");
						continue;
					}
					linenum = strtoul(num, NULL, 10);
				}
			}
			if (linenum == ULONG_MAX) {
				dbg->puts("Unable to parse line number\n");
				continue;
			}
			for (node = listFirstNode(&scripts); node; node = listNextNode(node)) {
				cs = (struct cur_script *)node->data;
				sname = getfname(cs->fname);
				if (strcmp(sname, text) == 0 || strcmp(cs->fname, text) == 0) {
					pc = JS_LineNumberToPC(dbg->cx, cs->script, linenum);
					if (JS_PCToLineNumber(dbg->cx, cs->script, pc) != linenum)
						continue;
					if (pc == NULL) {
						msg = xp_asprintf("Unable to locate line %lu\n", linenum);
						if (msg) {
							dbg->puts(msg);
							xp_asprintf_free(msg);
						}
						continue;
					}
					if (!JS_SetTrap(dbg->cx, cs->script, pc, trap_handler, JSVAL_VOID)) {
						msg = xp_asprintf("Unable to set breakpoint at line %lu\n", linenum);
						if (msg) {
							dbg->puts(msg);
							xp_asprintf_free(msg);
						}
					}
				}
			}
			bp = (struct breakpoint *)malloc(offsetof(struct breakpoint, name) + strlen(text) + 1);
			if (!bp) {
				dbg->puts("Unable to allocate breakpoint\n");
				continue;
			}
			bp->cx = dbg->cx;
			bp->line = linenum;
			strcpy(bp->name, text);
			listPushNode(&breakpoints, bp);
			continue;
		}
		if (strncmp(line, "run\n", 4) == 0 || strncmp(line, "r\n", 2) == 0) {
			free(line);
			JS_ClearInterrupt(JS_GetRuntime(dbg->cx), NULL, NULL);
			return DEBUG_CONTINUE;
		}
		if (strncmp(line, "step\n", 5) == 0 || strncmp(line, "s\n", 2) == 0) {
			free(line);
			JS_SetInterrupt(JS_GetRuntime(dbg->cx), single_step_handler, NULL);
			return DEBUG_CONTINUE;
		}
		if (strncmp(line, "finish\n", 7) == 0 || strncmp(line, "f\n", 2) == 0) {
			free(line);
			finish = JS_GetScriptedCaller(dbg->cx, NULL);
			JS_SetInterrupt(JS_GetRuntime(dbg->cx), finish_handler, NULL);
			return DEBUG_CONTINUE;
		}
		if (strncmp(line, "quit\n", 5) == 0 ||
		    strncmp(line, "q\n", 2) == 0
		    ) {
			free(line);
			return DEBUG_EXIT;
		}
		if (strncmp(line, "eval ", 5) == 0 ||
		    strncmp(line, "e ", 2) == 0
		    ) {
			jsval ret;
			jsval oldexcept;
			BOOL has_old = FALSE;
			int cmdlen = 5;

			if (line[1] == ' ')
				cmdlen = 2;
			if (JS_IsExceptionPending(dbg->cx)) {
				if (JS_GetPendingException(dbg->cx, &oldexcept))
					has_old = TRUE;
				JS_ClearPendingException(dbg->cx);
			}

			if (!fp) {
				if (has_old)
					JS_SetPendingException(dbg->cx, oldexcept);
				dbg->puts("Unable to get frame pointer\n");
				continue;
			}
			if (!JS_EvaluateInStackFrame(dbg->cx, fp, line + cmdlen, strlen(line) - cmdlen, "eval-d statement", 1, &ret)) {
				if (JS_IsExceptionPending(dbg->cx)) {
					JS_ReportPendingException(dbg->cx);
					JS_ClearPendingException(dbg->cx);
				}
			}
			else {
				// TODO: Check ret...
			}
			if (has_old)
				JS_SetPendingException(dbg->cx, oldexcept);
			continue;
		}
		if (strncmp(line, "clear", 5) == 0) {
			JS_ClearPendingException(dbg->cx);
			continue;
		}
		if (strncmp(line, "bt", 2) == 0
		    || strncmp(line, "backtrace", 9) == 0) {
			JSScript        *fs;
			JSStackFrame    *fpi = NULL;
			int fnum = 0;

			while (JS_FrameIterator(dbg->cx, &fpi)) {
				fs = JS_GetFrameScript(dbg->cx, fpi);
				fname = JS_GetScriptFilename(dbg->cx, fs);
				if (fname == NULL)
					fname = "No name";
				fname = getfname(fname);
				pc = JS_GetFramePC(dbg->cx, fpi);
				msg = xp_asprintf("%c#%-2u %p ", fpi == fp?'*':' ', fnum, pc);
				if (msg) {
					dbg->puts(msg);
					xp_asprintf_free(msg);
				}
				fn = JS_GetFrameFunction(dbg->cx, fpi);
				if (fn) {
					JSString    *name;

					name = JS_GetFunctionId(fn);
					if (name) {
						JSSTRING_TO_ASTRING(dbg->cx, name, cp, 128, NULL);
						msg = xp_asprintf("in %s() ", cp);
						if (msg) {
							dbg->puts(msg);
							xp_asprintf_free(msg);
						}
					}
				}
				dbg->puts("at ");
				dbg->puts(fname);
				if (pc) {
					msg = xp_asprintf(":%u", JS_PCToLineNumber(dbg->cx, fs, pc));
					if (msg) {
						dbg->puts(msg);
						xp_asprintf_free(msg);
					}
				}
				dbg->puts("\n");
				fnum++;
			}
			continue;
		}
		if (strncmp(line, "up", 2) == 0) {
			JSStackFrame    *fpn = fp;

			JS_FrameIterator(dbg->cx, &fpn);
			if (fpn == NULL) {
				dbg->puts("No frame above this one...\n");
				continue;
			}
			fp = fpn;
			script = JS_GetFrameScript(dbg->cx, fp);
			continue;
		}
		if (strncmp(line, "down", 4) == 0) {
			JSStackFrame    *fpi = NULL;
			JSStackFrame    *fpp = NULL;

			while (JS_FrameIterator(dbg->cx, &fpi)) {
				if (fpi == fp) {
					if (fpp == NULL) {
						dbg->puts("Already at deepest stack frame\n");
						break;
					}
					fp = fpp;
					script = JS_GetFrameScript(dbg->cx, fp);
					break;
				}
				fpp = fpi;
			}
			if (fpi == NULL)
				dbg->puts("A strange and mysterious error occurred!\n");
			continue;
		}
		dbg->puts("Unrecognized command:\n"
		          "break [file:]#### - Sets a breakpoint at line #### in file\n"
		          "                    If no file is specified, uses the current one\n"
		          "run               - Runs the script\n"
		          "r                 - Runs the script\n"
		          "step              - Runs to the next line\n"
		          "s                 - Runs to the next line\n"
		          "finish            - Runs until current stack frame is gone\n"
		          "f                 - Runs until current stack frame is gone\n"
		          "eval <statement>  - eval() <statement> in the current frame\n"
		          "e <statement>     - eval() <statement> in the current frame\n"
		          "clear             - Clears pending exceptions (doesn't seem to help)\n"
		          "bt                - Prints a backtrace\n"
		          "backtrace         - Alias for bt\n"
		          "up                - Move to the previous stack frame\n"
		          "down              - Move to the next stack frame\n"
		          "quit              - Terminate script and exit\n"
		          "q                 - Terminate script and exit\n"
		          "\n");
	}
	FREE_AND_NULL(line);
	return DEBUG_CONTINUE;
}

enum debug_action debug_prompt(JSContext *cx, JSObject *script)
{
	struct debugger *dbg = get_debugger(cx);

	if (dbg == NULL)
		return DEBUG_CONTINUE;
	return script_debug_prompt(dbg, JS_GetScriptFromObject(script));
}

static JSTrapStatus trap_handler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval, jsval closure)
{
	struct debugger *dbg = get_debugger(cx);

	if (dbg == NULL)
		return JSTRAP_CONTINUE;
	JS_GC(cx);

	dbg->puts("Breakpoint reached\n");

	switch (script_debug_prompt(dbg, script)) {
		case DEBUG_CONTINUE:
			return JSTRAP_CONTINUE;
		case DEBUG_EXIT:
			return JSTRAP_ERROR;
	}
	return JSTRAP_CONTINUE;
}

static JSTrapStatus throw_handler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval, void *closure)
{
	struct debugger *dbg = get_debugger(cx);

	if (dbg == NULL)
		return JSTRAP_CONTINUE;
	JS_GC(cx);

	dbg->puts("Exception thrown\n");

	switch (script_debug_prompt(dbg, script)) {
		case DEBUG_CONTINUE:
			return JSTRAP_CONTINUE;
		case DEBUG_EXIT:
			return JSTRAP_ERROR;
	}
	return JSTRAP_CONTINUE;
}

static JSTrapStatus
single_step_handler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval, void *closure)
{
	struct debugger *dbg = get_debugger(cx);

	if (dbg == NULL)
		return JSTRAP_CONTINUE;
	// Return if not beginning of line...
	if (JS_LineNumberToPC(cx, script, JS_PCToLineNumber(cx, script, pc)) != pc)
		return JSTRAP_CONTINUE;
	JS_GC(cx);

	dbg->puts("Single Stepping\n");

	switch (script_debug_prompt(dbg, script)) {
		case DEBUG_CONTINUE:
			return JSTRAP_CONTINUE;
		case DEBUG_EXIT:
			return JSTRAP_ERROR;
	}
	return JSTRAP_CONTINUE;
}

static JSTrapStatus
finish_handler(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval, void *closure)
{
	JSStackFrame *   fpi = NULL;
	struct debugger *dbg = get_debugger(cx);

	if (dbg == NULL)
		return JSTRAP_CONTINUE;

	JS_GC(cx);

	while (JS_FrameIterator(dbg->cx, &fpi)) {
		if (fpi == finish)
			return JSTRAP_CONTINUE;
	}

	dbg->puts("Finished\n");

	switch (script_debug_prompt(dbg, script)) {
		case DEBUG_CONTINUE:
			return JSTRAP_CONTINUE;
		case DEBUG_EXIT:
			return JSTRAP_ERROR;
	}
	return JSTRAP_CONTINUE;
}
