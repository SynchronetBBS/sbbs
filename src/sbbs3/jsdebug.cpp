/* jsdebug.cpp - Interactive JS debugger for SM128
 *
 * Uses the SM128 JS Debugger API (js/Debug.h) to provide breakpoints,
 * single-stepping, stack inspection, and eval-in-frame.
 *
 * Architecture: A separate "debugger realm" holds the Debugger object and
 * JS-level hooks.  Native functions (dbg_puts/dbg_getline) bridge the
 * hooks to the embedding's I/O callbacks.
 */

#include "sbbs.h"
#include "jsdebug.h"
#include <js/Debug.h>
#include <js/CompilationAndEvaluation.h>
#include <js/SourceText.h>
#include <js/Realm.h>
#include <js/Promise.h>
#include <js/ContextOptions.h>
#include <jsfriendapi.h>
#include <link_list.h>
#include <xpprintf.h>

/* ---- Minimal JobQueue for SM128 Debugger support ----
 *
 * SM128's Debugger internally creates AutoDebuggerJobQueueInterruption objects,
 * which call saveJobQueue() on the registered JobQueue.  Without a registered
 * queue, this dereferences a null pointer and crashes.
 *
 * We don't use promises or microtasks in jsexec, so this is a trivial
 * implementation that satisfies the interface contract.
 */

class DebuggerJobQueue : public JS::JobQueue {
public:
	JSObject* getIncumbentGlobal(JSContext* cx) override {
		return JS::CurrentGlobalOrNull(cx);
	}

	bool enqueuePromiseJob(JSContext* cx, JS::HandleObject promise,
	                       JS::HandleObject job,
	                       JS::HandleObject allocationSite,
	                       JS::HandleObject incumbentGlobal) override {
		/* We don't support promises in the debugger context */
		return true;
	}

	void runJobs(JSContext* cx) override {
		/* No jobs to run */
	}

	bool empty() const override {
		return true;
	}

	bool isDrainingStopped() const override {
		return false;
	}

protected:
	js::UniquePtr<SavedJobQueue> saveJobQueue(JSContext* cx) override {
		return js::MakeUnique<TrivialSavedJobQueue>();
	}

private:
	class TrivialSavedJobQueue : public SavedJobQueue {
	public:
		~TrivialSavedJobQueue() override = default;
	};
};

/* ---- Per-context debugger state ---- */

struct debugger_state {
	JSContext* cx;
	void (*puts_cb)(const char *);
	char* (*getline_cb)(void);
	JS::PersistentRootedObject dbg_global;   /* debugger realm global */
	JS::PersistentRootedObject debuggee_global; /* saved debuggee global */
	DebuggerJobQueue* job_queue;              /* registered with JS::SetJobQueue */
};

static link_list_t debuggers;

static struct debugger_state* get_debugger(JSContext* cx)
{
	list_node_t* node;
	struct debugger_state* ds;

	for (node = listFirstNode(&debuggers); node; node = listNextNode(node)) {
		ds = (struct debugger_state*)node->data;
		if (ds->cx == cx)
			return ds;
	}
	return NULL;
}

/* ---- Native functions exposed to the debugger realm ---- */

static JSBool
js_dbg_puts(JSContext* cx, unsigned argc, JS::Value* vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	struct debugger_state* ds = get_debugger(cx);
	if (ds == NULL || args.length() < 1) {
		args.rval().setUndefined();
		return true;
	}
	JS::RootedString str(cx, JS::ToString(cx, args[0]));
	if (!str)
		return false;
	JS::UniqueChars chars = JS_EncodeStringToLatin1(cx, str);
	if (chars)
		ds->puts_cb(chars.get());
	args.rval().setUndefined();
	return true;
}

static JSBool
js_dbg_getline(JSContext* cx, unsigned argc, JS::Value* vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	struct debugger_state* ds = get_debugger(cx);
	if (ds == NULL) {
		args.rval().setNull();
		return true;
	}
	char* line = ds->getline_cb();
	if (line == NULL) {
		args.rval().setNull();
		return true;
	}
	JSString* js_str = JS_NewStringCopyZ(cx, line);
	free(line);
	if (!js_str)
		return false;
	args.rval().setString(js_str);
	return true;
}

/* ---- Debugger realm class ---- */

static JSClassOps dbg_global_classops = {
	nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr,
	nullptr, JS_GlobalObjectTraceHook
};

static JSClass dbg_global_class = {
	"DebuggerGlobal",
	JSCLASS_GLOBAL_FLAGS,
	&dbg_global_classops
};

/* ---- JS bootstrap code for the debugger realm ----
 *
 * This script creates a Debugger instance and sets up hooks that call
 * the native dbg_puts / dbg_getline for interactive prompting.
 */
static const char debugger_bootstrap_js[] = R"JS(
"use strict";

var dbg = new Debugger();
var stepping = false;
var lastCmd = "";
var _pendingBreakpoints = [];
var inEval = false;

function getBasename(path) {
    if (!path) return "<unknown>";
    var i = Math.max(path.lastIndexOf('/'), path.lastIndexOf('\\'));
    return i >= 0 ? path.substr(i + 1) : path;
}

function formatFrame(frame) {
    var s = "";
    if (frame.script && frame.script.url) {
        s = getBasename(frame.script.url);
        try {
            var loc = frame.script.getOffsetLocation(frame.offset);
            if (loc) s += ":" + loc.lineNumber;
        } catch (e) {}
    }
    if (frame.callee) {
        var name = frame.callee.name;
        if (name) s += " " + name + "()";
    }
    return s;
}

function showHelp() {
    dbg_puts(
        "  break [file:]line  Set breakpoint\n" +
        "  run/r              Continue execution\n" +
        "  step/s             Single step to next line\n" +
        "  finish/f           Run until current frame returns\n" +
        "  eval/e <expr>      Evaluate in current frame\n" +
        "  bt/backtrace       Print stack trace\n" +
        "  up                 Move to caller frame\n" +
        "  down               Move to callee frame\n" +
        "  clear              Clear pending exception\n" +
        "  quit/q             Terminate script\n\n");
}

function promptLoop(frame) {
    var currentFrame = frame;
    var bottomFrame = frame;
    while (true) {
        dbg_puts(formatFrame(currentFrame) + "> ");
        var line = dbg_getline();
        if (line === null) return null;
        line = line.replace(/[\r\n]+$/, "");
        if (line === "") line = lastCmd;
        if (!line) continue;
        lastCmd = line;

        var spaceIdx = line.indexOf(' ');
        var cmd = spaceIdx > 0 ? line.substring(0, spaceIdx) : line;
        var arg = spaceIdx > 0 ? line.substring(spaceIdx + 1) : "";

        switch (cmd) {
        case "run": case "r":
            stepping = false;
            return undefined;
        case "step": case "s":
            stepping = true;
            var curLine = -1;
            try {
                var loc = currentFrame.script.getOffsetLocation(currentFrame.offset);
                if (loc) curLine = loc.lineNumber;
            } catch(e) {}
            currentFrame.onStep = makeStepHandler(curLine);
            return undefined;
        case "finish": case "f":
            stepping = false;
            currentFrame.onPop = function() {
                dbg_puts("Finished\n");
                if (this.older)
                    return promptLoop(this.older);
            };
            return undefined;
        case "quit": case "q":
            return null;
        case "eval": case "e":
            if (!arg) { dbg_puts("Usage: eval <expression>\n"); break; }
            try {
                inEval = true;
                var result = currentFrame.eval(arg);
                inEval = false;
                if (result) {
                    if ("return" in result) {
                        var val = result.return;
                        if (val && typeof val === "object" && val.unsafeDereference)
                            val = val.unsafeDereference();
                        dbg_puts(String(val) + "\n");
                    }
                    else if ("throw" in result) {
                        var ex = result.throw;
                        if (ex && typeof ex === "object" && ex.unsafeDereference)
                            ex = ex.unsafeDereference();
                        dbg_puts("Exception: " + String(ex) + "\n");
                    }
                }
            } catch (ex) {
                inEval = false;
                dbg_puts("Error: " + ex + "\n");
            }
            break;
        case "bt": case "backtrace": {
            var f = bottomFrame, n = 0;
            while (f) {
                var marker = (f === currentFrame) ? "*" : " ";
                dbg_puts(marker + "#" + n + " " + formatFrame(f) + "\n");
                f = f.older;
                n++;
            }
            break;
        }
        case "up":
            if (currentFrame.older) currentFrame = currentFrame.older;
            else dbg_puts("No frame above this one...\n");
            break;
        case "down": {
            var f = bottomFrame, prev = null;
            while (f && f !== currentFrame) { prev = f; f = f.older; }
            if (prev) currentFrame = prev;
            else dbg_puts("Already at deepest stack frame\n");
            break;
        }
        case "break": case "b":
            addBreakpoint(arg, currentFrame.script ? currentFrame.script.url : "");
            break;
        case "clear":
            dbg_puts("Cleared\n");
            break;
        default:
            showHelp();
        }
    }
}

function makeStepHandler(skipLine) {
    return function() {
        var loc = this.script.getOffsetLocation(this.offset);
        if (loc && loc.lineNumber === skipLine)
            return;
        var result = promptLoop(this);
        if (!stepping) this.onStep = undefined;
        return result;
    };
}

function addBreakpoint(text, currentUrl) {
    var file, linenum;
    if (!text) { dbg_puts("Usage: break [file:]line\n"); return; }
    var colonIdx = text.lastIndexOf(':');
    if (colonIdx > 0 && !isNaN(parseInt(text.substring(colonIdx + 1)))) {
        file = text.substring(0, colonIdx);
        linenum = parseInt(text.substring(colonIdx + 1));
    } else {
        file = currentUrl || "";
        linenum = parseInt(text);
    }
    if (isNaN(linenum)) {
        dbg_puts("Unable to parse breakpoint\n");
        return;
    }
    _pendingBreakpoints.push({file: file, line: linenum});
    applyBreakpointsToScripts();
}

function makeBpHandler() {
    return {
        hit: function(frame) {
            dbg_puts("Breakpoint reached\n");
            return promptLoop(frame);
        }
    };
}

function applyBreakpointsToScripts() {
    var scripts = dbg.findScripts();
    for (var i = 0; i < _pendingBreakpoints.length; i++) {
        var bp = _pendingBreakpoints[i];
        if (bp.applied) continue;
        var bname = getBasename(bp.file);
        for (var j = 0; j < scripts.length; j++) {
            var s = scripts[j];
            if (!s.url) continue;
            var sname = getBasename(s.url);
            if (sname === bname || s.url === bp.file || sname === bp.file) {
                try {
                    var offsets = s.getLineOffsets(bp.line);
                    if (offsets.length > 0) {
                        for (var k = 0; k < offsets.length; k++)
                            s.setBreakpoint(offsets[k], makeBpHandler());
                        bp.applied = true;
                    }
                } catch (e) {
                    dbg_puts("Breakpoint error: " + e + "\n");
                }
            }
        }
    }
}

dbg.onNewScript = function(script) {
    applyBreakpointsToScripts();
};

dbg.onEnterFrame = function(frame) {
    if (inEval) return;
    if (stepping) {
        frame.onStep = makeStepHandler(-1);
    }
};

dbg.onExceptionUnwind = function(frame, value) {
    if (inEval) return;
    dbg_puts("Exception thrown\n");
    return promptLoop(frame);
};

function startDebugging(debuggeeGlobal, doStep) {
    dbg.addDebuggee(debuggeeGlobal);
    stepping = doStep;
    applyBreakpointsToScripts();
}

function addPendingBreakpoint(file, line) {
    _pendingBreakpoints.push({file: file, line: line});
}
)JS";

/* ---- Public API implementation ---- */

void setup_debugger(void)
{
	listInit(&debuggers, LINK_LIST_MUTEX);
}

BOOL init_debugger(JSContext* cx, void (*puts_cb)(const char*), char* (*getline_cb)(void))
{
	if (get_debugger(cx))
		return FALSE;

	struct debugger_state* ds = new struct debugger_state;
	if (!ds)
		return FALSE;
	ds->cx = cx;
	ds->puts_cb = puts_cb;
	ds->getline_cb = getline_cb;
	new (&ds->dbg_global) JS::PersistentRootedObject(cx);
	new (&ds->debuggee_global) JS::PersistentRootedObject(cx);

	/* Save the debuggee global */
	ds->debuggee_global = JS::CurrentGlobalOrNull(cx);
	if (!ds->debuggee_global) {
		delete ds;
		return FALSE;
	}

	/* Register a job queue so SM128's Debugger can use
	 * AutoDebuggerJobQueueInterruption without crashing */
	ds->job_queue = new DebuggerJobQueue();
	JS::SetJobQueue(cx, ds->job_queue);

	/* Allow eval in debugger hooks without DebuggeeWouldRun errors */
	JS::ContextOptionsRef(cx).setThrowOnDebuggeeWouldRun(false);

	/* Create a new global for the debugger realm */
	JS::RealmOptions options;
	JS::RootedObject dbg_glob(cx,
		JS_NewGlobalObject(cx, &dbg_global_class, nullptr,
		                   JS::FireOnNewGlobalHook, options));
	if (!dbg_glob) {
		delete ds;
		return FALSE;
	}
	ds->dbg_global = dbg_glob;

	/* Enter the debugger realm */
	JS::Realm* old_realm = JS::EnterRealm(cx, dbg_glob);

	/* Initialize standard classes and the Debugger constructor */
	if (!JS::InitRealmStandardClasses(cx)) {
		JS::LeaveRealm(cx, old_realm);
		delete ds;
		return FALSE;
	}
	{
		JS::RootedObject hobj(cx, dbg_glob);
		if (!JS_DefineDebuggerObject(cx, hobj)) {
			JS::LeaveRealm(cx, old_realm);
			delete ds;
			return FALSE;
		}
	}

	/* Define native I/O functions in the debugger realm */
	{
		JS::HandleObject hobj = dbg_glob;
		if (!JS_DefineFunction(cx, hobj, "dbg_puts", js_dbg_puts, 1, 0) ||
		    !JS_DefineFunction(cx, hobj, "dbg_getline", js_dbg_getline, 0, 0)) {
			JS::LeaveRealm(cx, old_realm);
			delete ds;
			return FALSE;
		}
	}

	/* Compile and execute the bootstrap JS */
	{
		JS::CompileOptions opts(cx);
		opts.setFileAndLine("jsdebug-bootstrap", 1);
		JS::SourceText<mozilla::Utf8Unit> source;
		if (!source.init(cx, debugger_bootstrap_js, strlen(debugger_bootstrap_js),
		                 JS::SourceOwnership::Borrowed)) {
			JS::LeaveRealm(cx, old_realm);
			delete ds;
			return FALSE;
		}
		JS::RootedValue rval(cx);
		if (!JS::Evaluate(cx, opts, source, &rval)) {
			JS::LeaveRealm(cx, old_realm);
			delete ds;
			return FALSE;
		}
	}

	/* Return to the debuggee realm */
	JS::LeaveRealm(cx, old_realm);

	listPushNode(&debuggers, ds);
	return TRUE;
}

enum debug_action debug_prompt(JSContext* cx, JSScript* script)
{
	struct debugger_state* ds = get_debugger(cx);
	if (ds == NULL)
		return DEBUG_CONTINUE;

	const char* fname = JS_GetScriptFilename(script);
	const char* basename = fname;
	if (basename) {
		const char* p = strrchr(basename, '/');
		if (p) basename = p + 1;
#ifdef _WIN32
		p = strrchr(basename, '\\');
		if (p) basename = p + 1;
#endif
	}
	if (!basename)
		basename = "<unknown>";

	char lastline[1024] = "";

	/* Initial prompt loop (before execution, no frame yet) */
	for (;;) {
		char* msg = xp_asprintf("%s:1> ", basename);
		if (msg) {
			ds->puts_cb(msg);
			xp_asprintf_free(msg);
		}
		char* line = ds->getline_cb();
		if (line == NULL)
			return DEBUG_EXIT;
		/* Strip trailing newline */
		size_t len = strlen(line);
		while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
			line[--len] = '\0';

		/* Enter repeats last command */
		if (line[0] == '\0') {
			free(line);
			line = strdup(lastline);
			if (!line) continue;
		} else {
			snprintf(lastline, sizeof(lastline), "%s", line);
		}

		if (strncmp(line, "break ", 6) == 0 || strncmp(line, "b ", 2) == 0) {
			const char* text = line + (line[1] == ' ' ? 2 : 6);
			char file[MAX_PATH + 1];
			unsigned linenum = 0;
			const char* colon = strrchr(text, ':');

			if (colon && colon > text && IS_DIGIT(*(colon + 1))) {
				snprintf(file, sizeof(file), "%.*s", (int)(colon - text), text);
				linenum = atoi(colon + 1);
			} else if (IS_DIGIT(*text)) {
				SAFECOPY(file, fname ? fname : "");
				linenum = atoi(text);
			} else {
				ds->puts_cb("Unable to parse breakpoint\n");
				free(line);
				continue;
			}

			/* Store the breakpoint in the JS debugger */
			JS::Realm* old_realm = JS::EnterRealm(cx, ds->dbg_global);
			{
				JS::RootedObject glob(cx, ds->dbg_global.get());
				JS::RootedValue fileVal(cx);
				JSString* js_str = JS_NewStringCopyZ(cx, file);
				if (js_str)
					fileVal.setString(js_str);
				JS::RootedValue lineVal(cx, JS::Int32Value((int32_t)linenum));
				JS::RootedValueVector args(cx);
				if (!args.append(fileVal) || !args.append(lineVal))
					break;
				JS::RootedValue rval(cx);
				JS_CallFunctionName(cx, glob, "addPendingBreakpoint", args, &rval);
			}
			JS::LeaveRealm(cx, old_realm);

			msg = xp_asprintf("Breakpoint set at %s:%u\n", file, linenum);
			if (msg) {
				ds->puts_cb(msg);
				xp_asprintf_free(msg);
			}
			free(line);
			continue;
		}

		if (strcmp(line, "run") == 0 || strcmp(line, "r") == 0) {
			free(line);
			/* Activate the debugger with stepping=false */
			JS::Realm* old_realm = JS::EnterRealm(cx, ds->dbg_global);
			{
				JS::RootedObject glob(cx, ds->dbg_global.get());
				JS::RootedObject debuggee(cx, ds->debuggee_global.get());
				if (!JS_WrapObject(cx, &debuggee)) {
					JS::LeaveRealm(cx, old_realm);
					return DEBUG_EXIT;
				}
				JS::RootedValueVector args(cx);
				if (!args.append(JS::ObjectValue(*debuggee)) ||
				    !args.append(JS::BooleanValue(false))) {
					JS::LeaveRealm(cx, old_realm);
					return DEBUG_EXIT;
				}
				JS::RootedValue rval(cx);
				JS_CallFunctionName(cx, glob, "startDebugging", args, &rval);
			}
			JS::LeaveRealm(cx, old_realm);
			return DEBUG_CONTINUE;
		}

		if (strcmp(line, "step") == 0 || strcmp(line, "s") == 0) {
			free(line);
			/* Activate the debugger with stepping=true */
			JS::Realm* old_realm = JS::EnterRealm(cx, ds->dbg_global);
			{
				JS::RootedObject glob(cx, ds->dbg_global.get());
				JS::RootedObject debuggee(cx, ds->debuggee_global.get());
				if (!JS_WrapObject(cx, &debuggee)) {
					JS::LeaveRealm(cx, old_realm);
					return DEBUG_EXIT;
				}
				JS::RootedValueVector args(cx);
				if (!args.append(JS::ObjectValue(*debuggee)) ||
				    !args.append(JS::BooleanValue(true))) {
					JS::LeaveRealm(cx, old_realm);
					return DEBUG_EXIT;
				}
				JS::RootedValue rval(cx);
				JS_CallFunctionName(cx, glob, "startDebugging", args, &rval);
			}
			JS::LeaveRealm(cx, old_realm);
			return DEBUG_CONTINUE;
		}

		if (strcmp(line, "quit") == 0 || strcmp(line, "q") == 0) {
			free(line);
			return DEBUG_EXIT;
		}

		/* Help */
		ds->puts_cb(
			"  break [file:]line  Set breakpoint\n"
			"  run/r              Run the script\n"
			"  step/s             Single step from start\n"
			"  quit/q             Terminate\n\n");
		free(line);
	}
	/* NOTREACHED */
	return DEBUG_EXIT;
}

void end_debugger(JSContext* cx)
{
	list_node_t* node;
	struct debugger_state* ds;

	for (node = listFirstNode(&debuggers); node; node = listNextNode(node)) {
		ds = (struct debugger_state*)node->data;
		if (ds->cx == cx) {
			listRemoveNode(&debuggers, node, FALSE);
			ds->dbg_global.reset();
			ds->debuggee_global.reset();
			JS::SetJobQueue(cx, nullptr);
			delete ds->job_queue;
			delete ds;
			return;
		}
	}
}
