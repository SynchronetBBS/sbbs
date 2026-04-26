/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Per-connection Wren scripting host for SyncTERM.  The VM, hook
 * registry, and dispatchers all live here; the foreign-method bodies
 * (Conio/Conn/Cterm/BBS/Cache/Hook) live in wren_bind.c.
 *
 * Lifecycle: doterm() calls wren_host_init(bbs) before its main loop
 * and wren_host_shutdown() at every exit path.  Scripts in
 * SYNCTERM_PATH_SCRIPTS are loaded as individual modules; their
 * top-level code runs at module-load time and registers callbacks
 * via Hook.onKey/onInput/etc.
 *
 * Thread affinity: WrenVM is single-threaded.  The owner thread is
 * captured at init; dispatchers from other threads (e.g. background
 * SFTP/SSH calling conn_send) short-circuit to pass-through. */

#include "wren_host.h"
#include "wren_host_internal.h"

#include "wren.h"

#include "syncterm.h"
#include "bbslist.h"
#include "ciolib.h"
#include "dirwrap.h"
#include "genwrap.h"      /* xp_timer */
#include "threadwrap.h"   /* pthread_self */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------------
 * Embedded "syncterm" module — declares all foreign classes that user
 * scripts import.  The actual method implementations are bound at
 * runtime via wren_bind_lookup() (see wren_bind.c).
 * -------------------------------------------------------------------- */

static const char SYNCTERM_MODULE_SRC[] =
    "foreign class Conio {\n"
    "  foreign static putch(c)\n"
    "  foreign static cputs(s)\n"
    "  foreign static gotoxy(x, y)\n"
    "  foreign static wherex\n"
    "  foreign static wherey\n"
    "  foreign static textattr(a)\n"
    "  foreign static screenSize\n"
    "  foreign static ungetch(k)\n"
    "}\n"
    "foreign class Conn {\n"
    "  foreign static send(s)\n"
    "  foreign static sendRaw(s)\n"
    "  foreign static close()\n"
    "  foreign static connected\n"
    "  foreign static type\n"
    "  foreign static inbufBytes\n"
    "  foreign static outbufBytes\n"
    "  foreign static inbufPeek(n)\n"
    "  foreign static inbufGet(n)\n"
    "  foreign static outbufPut(s)\n"
    "}\n"
    "foreign class Cterm {\n"
    "  foreign static emulation\n"
    "  foreign static x\n"
    "  foreign static y\n"
    "  foreign static attr\n"
    "  foreign static doorwayMode\n"
    "  foreign static music\n"
    "  foreign static width\n"
    "  foreign static height\n"
    "  foreign static write(s)\n"
    "}\n"
    "foreign class BBS {\n"
    "  foreign static name\n"
    "  foreign static addr\n"
    "  foreign static port\n"
    "  foreign static connType\n"
    "  foreign static user\n"
    "  foreign static music\n"
    "  foreign static rip\n"
    "  foreign static comment\n"
    "}\n"
    "foreign class Cache {\n"
    "  foreign static basePath\n"
    "  foreign static subdirPath(s)\n"
    "  foreign static readFile(s)\n"
    "  foreign static writeFile(name, contents)\n"
    "}\n"
    "foreign class Hook {\n"
    "  foreign static onKey(fn)\n"
    "  foreign static onInput(fn)\n"
    "  foreign static onOutput(fn)\n"
    "  foreign static onMouse(fn)\n"
    "  foreign static onStatus(fn)\n"
    "  foreign static every(ms, fn)\n"
    "}\n";

/* --------------------------------------------------------------------
 * State
 * -------------------------------------------------------------------- */

static struct wren_host_state g_state;
static bool      g_active;
static pthread_t g_owner_thread;

static wren_capture_fn  g_capture_fn   = NULL;
static void            *g_capture_data = NULL;

void
wren_host_set_capture(wren_capture_fn fn, void *cbdata)
{
	g_capture_fn   = fn;
	g_capture_data = cbdata;
}

WrenInterpretResult
wren_host_interpret(const char *module, const char *source)
{
	if (!g_active || module == NULL || source == NULL)
		return WREN_RESULT_COMPILE_ERROR;
	return wrenInterpret(g_state.vm, module, source);
}

WrenVM *
wren_host_vm(void)
{
	return g_active ? g_state.vm : NULL;
}

struct wren_host_state *
wren_host_state(void)
{
	return g_active ? &g_state : NULL;
}

bool
wren_host_active(void)
{
	if (!g_active)
		return false;
	/* Cheap fast-path: avoid a pthread_self() + compare on every call
	 * site by allowing a positive answer from any thread; only the
	 * dispatchers themselves enforce thread affinity. */
	return true;
}

/* --------------------------------------------------------------------
 * Wren callbacks
 * -------------------------------------------------------------------- */

static void
host_write_fn(WrenVM *vm, const char *text)
{
	(void)vm;
	if (text == NULL)
		return;
	if (g_capture_fn != NULL)
		g_capture_fn(text, false, g_capture_data);
	else
		fputs(text, stderr);
}

static void
host_error_fn(WrenVM *vm, WrenErrorType type, const char *module, int line,
    const char *message)
{
	(void)vm;
	char buf[1024];
	switch (type) {
		case WREN_ERROR_COMPILE:
			snprintf(buf, sizeof(buf), "%s:%d compile error: %s\n",
			    module ? module : "?", line, message ? message : "");
			break;
		case WREN_ERROR_RUNTIME:
			snprintf(buf, sizeof(buf), "runtime error: %s\n",
			    message ? message : "");
			break;
		case WREN_ERROR_STACK_TRACE:
			snprintf(buf, sizeof(buf), "  at %s:%d in %s\n",
			    module ? module : "?", line, message ? message : "");
			break;
		default:
			buf[0] = '\0';
			break;
	}
	if (g_capture_fn != NULL)
		g_capture_fn(buf, true, g_capture_data);
	else
		fprintf(stderr, "[wren] %s", buf);
}

static WrenForeignMethodFn
host_bind_foreign_method(WrenVM *vm, const char *module, const char *className,
    bool isStatic, const char *signature)
{
	(void)vm;
	return wren_bind_lookup(module, className, isStatic, signature);
}

/* --------------------------------------------------------------------
 * Hook + timer registration
 * -------------------------------------------------------------------- */

bool
wren_host_register_hook(WrenVM *vm, enum wren_hook_event ev, int fn_slot)
{
	if (!g_active || ev < 0 || ev >= WREN_HOOK_COUNT)
		return false;
	int n = g_state.hook_count[ev];
	if (n >= WREN_HOST_MAX_HOOKS_PER_EVENT) {
		fprintf(stderr, "[wren] hook limit reached for event %d\n", (int)ev);
		return false;
	}
	g_state.hooks[ev][n] = wrenGetSlotHandle(vm, fn_slot);
	g_state.hook_count[ev] = n + 1;
	return true;
}

bool
wren_host_register_timer(WrenVM *vm, int ms, int fn_slot)
{
	if (!g_active)
		return false;
	if (ms < 1)
		ms = 1;
	if (g_state.timer_count >= WREN_HOST_MAX_TIMERS) {
		fprintf(stderr, "[wren] timer limit reached\n");
		return false;
	}
	struct wren_timer_entry *t = &g_state.timers[g_state.timer_count++];
	t->callable    = wrenGetSlotHandle(vm, fn_slot);
	t->interval_ms = (uint32_t)ms;
	t->next_fire_s = xp_timer() + (long double)ms / 1000.0L;
	return true;
}

/* --------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------- */

static void
load_one_script(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (f == NULL) {
		fprintf(stderr, "[wren] cannot open %s\n", path);
		return;
	}
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz < 0 || sz > 4 * 1024 * 1024) {
		fprintf(stderr, "[wren] %s: bad size %ld\n", path, sz);
		fclose(f);
		return;
	}
	char *src = malloc((size_t)sz + 1);
	if (src == NULL) {
		fclose(f);
		return;
	}
	size_t got = fread(src, 1, (size_t)sz, f);
	fclose(f);
	src[got] = '\0';

	/* Module name = basename without ".wren" extension, so each
	 * script's import namespace is distinct. */
	const char *base = strrchr(path, '/');
#ifdef _WIN32
	const char *base2 = strrchr(path, '\\');
	if (base2 != NULL && (base == NULL || base2 > base))
		base = base2;
#endif
	base = (base == NULL) ? path : base + 1;
	char modname[256];
	strncpy(modname, base, sizeof(modname) - 1);
	modname[sizeof(modname) - 1] = '\0';
	char *dot = strrchr(modname, '.');
	if (dot != NULL)
		*dot = '\0';

	WrenInterpretResult r = wrenInterpret(g_state.vm, modname, src);
	free(src);
	if (r != WREN_RESULT_SUCCESS)
		fprintf(stderr, "[wren] %s: load failed\n", path);
}

void
wren_host_init(struct bbslist *bbs)
{
	char dir[MAX_PATH + 1];

	if (g_active)
		return;

	memset(&g_state, 0, sizeof(g_state));
	g_state.bbs = bbs;

	WrenConfiguration cfg;
	wrenInitConfiguration(&cfg);
	cfg.writeFn             = host_write_fn;
	cfg.errorFn             = host_error_fn;
	cfg.bindForeignMethodFn = host_bind_foreign_method;
	g_state.vm = wrenNewVM(&cfg);
	if (g_state.vm == NULL)
		return;

	g_state.call0_handle = wrenMakeCallHandle(g_state.vm, "call()");
	g_state.call1_handle = wrenMakeCallHandle(g_state.vm, "call(_)");

	g_owner_thread = pthread_self();
	g_active = true;

	/* Register the foreign-class declarations so user scripts can
	 * `import "syncterm" for ...`. */
	if (wrenInterpret(g_state.vm, "syncterm", SYNCTERM_MODULE_SRC) !=
	    WREN_RESULT_SUCCESS) {
		fprintf(stderr, "[wren] failed to load builtin syncterm module\n");
		wren_host_shutdown();
		return;
	}

	if (get_syncterm_filename(dir, sizeof(dir), SYNCTERM_PATH_SCRIPTS, false)
	    == NULL)
		return;

	char pat[MAX_PATH + 16];
	snprintf(pat, sizeof(pat), "%s*.wren", dir);
	glob_t gl;
	memset(&gl, 0, sizeof(gl));
	if (glob(pat, 0, NULL, &gl) == 0) {
		for (size_t i = 0; i < gl.gl_pathc; i++)
			load_one_script(gl.gl_pathv[i]);
	}
	globfree(&gl);
}

void
wren_host_shutdown(void)
{
	if (!g_active)
		return;

	for (int e = 0; e < WREN_HOOK_COUNT; e++) {
		for (int i = 0; i < g_state.hook_count[e]; i++)
			if (g_state.hooks[e][i] != NULL)
				wrenReleaseHandle(g_state.vm, g_state.hooks[e][i]);
		g_state.hook_count[e] = 0;
	}
	for (int i = 0; i < g_state.timer_count; i++)
		if (g_state.timers[i].callable != NULL)
			wrenReleaseHandle(g_state.vm, g_state.timers[i].callable);
	g_state.timer_count = 0;

	if (g_state.call0_handle != NULL)
		wrenReleaseHandle(g_state.vm, g_state.call0_handle);
	if (g_state.call1_handle != NULL)
		wrenReleaseHandle(g_state.vm, g_state.call1_handle);

	wrenFreeVM(g_state.vm);
	g_state.vm = NULL;
	g_active = false;
}

/* --------------------------------------------------------------------
 * Dispatchers
 *
 * All input-shaped events (key/input/output/mouse) walk their hook
 * list in registration order; the first hook returning Bool true
 * short-circuits with consume==true.  Errors during a callback log
 * once and pass through.
 * -------------------------------------------------------------------- */

static bool
on_owner_thread(void)
{
	return pthread_self() == g_owner_thread;
}

static bool
read_consume_result(void)
{
	if (wrenGetSlotType(g_state.vm, 0) == WREN_TYPE_BOOL)
		return wrenGetSlotBool(g_state.vm, 0);
	return false;
}

bool
wren_host_dispatch_key(int key)
{
	if (!g_active || !on_owner_thread())
		return false;
	int n = g_state.hook_count[WREN_HOOK_KEY];
	for (int i = 0; i < n; i++) {
		wrenEnsureSlots(g_state.vm, 2);
		wrenSetSlotHandle(g_state.vm, 0, g_state.hooks[WREN_HOOK_KEY][i]);
		wrenSetSlotDouble(g_state.vm, 1, (double)key);
		if (wrenCall(g_state.vm, g_state.call1_handle) !=
		    WREN_RESULT_SUCCESS)
			continue;
		if (read_consume_result())
			return true;
	}
	return false;
}

bool
wren_host_dispatch_input(unsigned char byte)
{
	if (!g_active || !on_owner_thread())
		return false;
	int n = g_state.hook_count[WREN_HOOK_INPUT];
	for (int i = 0; i < n; i++) {
		wrenEnsureSlots(g_state.vm, 2);
		wrenSetSlotHandle(g_state.vm, 0, g_state.hooks[WREN_HOOK_INPUT][i]);
		wrenSetSlotDouble(g_state.vm, 1, (double)byte);
		if (wrenCall(g_state.vm, g_state.call1_handle) !=
		    WREN_RESULT_SUCCESS)
			continue;
		if (read_consume_result())
			return true;
	}
	return false;
}

bool
wren_host_dispatch_output(const void *buf, size_t len)
{
	if (!g_active || !on_owner_thread() || buf == NULL)
		return false;
	int n = g_state.hook_count[WREN_HOOK_OUTPUT];
	for (int i = 0; i < n; i++) {
		wrenEnsureSlots(g_state.vm, 2);
		wrenSetSlotHandle(g_state.vm, 0, g_state.hooks[WREN_HOOK_OUTPUT][i]);
		wrenSetSlotBytes(g_state.vm, 1, (const char *)buf, len);
		if (wrenCall(g_state.vm, g_state.call1_handle) !=
		    WREN_RESULT_SUCCESS)
			continue;
		if (read_consume_result())
			return true;
	}
	return false;
}

bool
wren_host_dispatch_mouse(struct mouse_event *ev)
{
	if (!g_active || !on_owner_thread() || ev == NULL)
		return false;
	int n = g_state.hook_count[WREN_HOOK_MOUSE];
	if (n == 0)
		return false;
	/* Build a 7-element list: [event, bstate, kbmodifiers, startx,
	 * starty, endx, endy].  Scripts index by position. */
	for (int i = 0; i < n; i++) {
		wrenEnsureSlots(g_state.vm, 3);
		wrenSetSlotHandle(g_state.vm, 0, g_state.hooks[WREN_HOOK_MOUSE][i]);
		wrenSetSlotNewList(g_state.vm, 1);
		const double fields[] = {
			(double)ev->event, (double)ev->bstate,
			(double)ev->kbmodifiers, (double)ev->startx,
			(double)ev->starty, (double)ev->endx,
			(double)ev->endy
		};
		for (size_t f = 0; f < sizeof(fields) / sizeof(fields[0]); f++) {
			wrenSetSlotDouble(g_state.vm, 2, fields[f]);
			wrenInsertInList(g_state.vm, 1, -1, 2);
		}
		if (wrenCall(g_state.vm, g_state.call1_handle) !=
		    WREN_RESULT_SUCCESS)
			continue;
		if (read_consume_result())
			return true;
	}
	return false;
}

bool
wren_host_compose_status(const char *def, char *out, size_t outsz)
{
	if (!g_active || !on_owner_thread() || out == NULL || outsz == 0)
		return false;
	int n = g_state.hook_count[WREN_HOOK_STATUS];
	for (int i = 0; i < n; i++) {
		wrenEnsureSlots(g_state.vm, 2);
		wrenSetSlotHandle(g_state.vm, 0, g_state.hooks[WREN_HOOK_STATUS][i]);
		wrenSetSlotString(g_state.vm, 1, def != NULL ? def : "");
		if (wrenCall(g_state.vm, g_state.call1_handle) !=
		    WREN_RESULT_SUCCESS)
			continue;
		if (wrenGetSlotType(g_state.vm, 0) != WREN_TYPE_STRING)
			continue;
		const char *s = wrenGetSlotString(g_state.vm, 0);
		if (s == NULL)
			continue;
		strlcpy(out, s, outsz);
		return true;
	}
	return false;
}

void
wren_host_dispatch_timer(void)
{
	if (!g_active || !on_owner_thread())
		return;
	long double now = xp_timer();
	for (int i = 0; i < g_state.timer_count; i++) {
		struct wren_timer_entry *t = &g_state.timers[i];
		if (t->callable == NULL || now < t->next_fire_s)
			continue;
		wrenEnsureSlots(g_state.vm, 1);
		wrenSetSlotHandle(g_state.vm, 0, t->callable);
		(void)wrenCall(g_state.vm, g_state.call0_handle);
		/* Advance by interval; if the loop stalled long enough that
		 * we're more than one interval behind, jump forward to "now"
		 * rather than firing repeatedly trying to catch up. */
		t->next_fire_s += (long double)t->interval_ms / 1000.0L;
		if (t->next_fire_s < now)
			t->next_fire_s = now + (long double)t->interval_ms / 1000.0L;
	}
}
