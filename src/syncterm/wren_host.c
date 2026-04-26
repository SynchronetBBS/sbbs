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
    "  foreign static getch\n"
    "  foreign static kbhit\n"
    "  foreign static clrscr()\n"
    "  foreign static clreol()\n"
    "  foreign static savescreen\n"
    "  foreign static restorescreen(handle)\n"
    "}\n"
    "foreign class Console {\n"
    "  foreign static count\n"
    "  foreign static total\n"
    "  foreign static [seq]\n"
    "  foreign static clear()\n"
    "  foreign static iterate(it)\n"
    "  foreign static iteratorValue(it)\n"
    "}\n"
    "class LogSource {\n"
    "  static print { 0 }\n"
    "  static compileError { 1 }\n"
    "  static runtimeError { 2 }\n"
    "  static stackFrame { 3 }\n"
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

/* --------------------------------------------------------------------
 * Log buffer (always-on print/error scrollback)
 *
 * Fixed-size ring; head = next write index, count = valid entries.
 * Oldest-relative reads come out as `(head - count + i) mod cap` so
 * scripts always see entry 0 as the oldest valid message regardless
 * of where the ring head currently sits.
 *
 * Each slot keeps either the C-malloc'd text bytes (until first read)
 * OR the cached Wren tuple handle (after first read), never both.
 * This keeps memory at one copy per slot worst-case.
 * -------------------------------------------------------------------- */

#define WREN_LOG_CAPACITY  1024
#define WREN_LOG_MAX_TEXT  (8 * 1024)

struct wren_log_entry {
	uint64_t             seq;           /* assigned at write time */
	long double          ts;
	enum wren_log_source source;
	char                *text;          /* freed at first read or eviction */
	size_t               len;
	WrenHandle          *cached_value;  /* NULL until first read */
};

static struct wren_log_entry g_log[WREN_LOG_CAPACITY];
static int      g_log_head;
static int      g_log_count;
static uint64_t g_log_total;            /* monotonic; survives wren_log_clear */

static void
log_release_slot(struct wren_log_entry *e)
{
	if (e->cached_value != NULL && g_state.vm != NULL) {
		wrenReleaseHandle(g_state.vm, e->cached_value);
		e->cached_value = NULL;
	}
	if (e->text != NULL) {
		free(e->text);
		e->text = NULL;
	}
	e->len = 0;
}

static void
log_append(enum wren_log_source source, const char *text)
{
	if (text == NULL)
		text = "";
	size_t len = strlen(text);
	bool truncated = false;
	if (len > WREN_LOG_MAX_TEXT) {
		len = WREN_LOG_MAX_TEXT;
		truncated = true;
	}
	static const char trunc_suffix[] = "…[truncated]";
	size_t alloc = len + (truncated ? sizeof(trunc_suffix) - 1 : 0);

	char *buf = malloc(alloc + 1);
	if (buf == NULL)
		return;
	memcpy(buf, text, len);
	if (truncated)
		memcpy(buf + len, trunc_suffix, sizeof(trunc_suffix) - 1);
	buf[alloc] = '\0';

	struct wren_log_entry *e = &g_log[g_log_head];
	log_release_slot(e);
	e->seq    = g_log_total++;
	e->ts     = xp_timer();
	e->source = source;
	e->text   = buf;
	e->len    = alloc;

	g_log_head = (g_log_head + 1) % WREN_LOG_CAPACITY;
	if (g_log_count < WREN_LOG_CAPACITY)
		g_log_count++;
}

int
wren_log_count(void)
{
	return g_log_count;
}

uint64_t
wren_log_total(void)
{
	return g_log_total;
}

void
wren_log_clear(void)
{
	for (int i = 0; i < WREN_LOG_CAPACITY; i++)
		log_release_slot(&g_log[i]);
	g_log_head  = 0;
	g_log_count = 0;
	/* g_log_total intentionally NOT reset — Wren scripts may have a
	 * high-water mark `lastTotal` they're tracking; resetting would
	 * make their existing mark falsely reappear as "new writes."
	 * Buffer is empty after this; next write gets seq = g_log_total
	 * (no skip). */
}

void
wren_log_emit(WrenVM *vm, uint64_t seq, int slot)
{
	/* Valid range: oldest seq = g_log_total - g_log_count;
	 * newest seq = g_log_total - 1. */
	if (g_log_count == 0 ||
	    seq < g_log_total - (uint64_t)g_log_count ||
	    seq >= g_log_total) {
		wrenSetSlotNull(vm, slot);
		return;
	}
	/* Newest entry sits at (head - 1) mod cap.  Walking backward by
	 * (g_log_total - 1 - seq) gives the slot for `seq`. */
	int back = (int)(g_log_total - 1 - seq);
	int idx  = ((g_log_head - 1 - back) % WREN_LOG_CAPACITY +
	            WREN_LOG_CAPACITY) % WREN_LOG_CAPACITY;
	struct wren_log_entry *e = &g_log[idx];

	if (e->cached_value != NULL) {
		wrenSetSlotHandle(vm, slot, e->cached_value);
		return;
	}

	/* First read: build [ts, source, text] in slot, capture handle,
	 * free the C buffer (text now lives once, in the Wren String the
	 * tuple references). */
	wrenEnsureSlots(vm, slot + 4);
	wrenSetSlotNewList(vm, slot);
	wrenSetSlotDouble(vm, slot + 1, (double)e->ts);
	wrenInsertInList(vm, slot, -1, slot + 1);
	wrenSetSlotDouble(vm, slot + 1, (double)(int)e->source);
	wrenInsertInList(vm, slot, -1, slot + 1);
	wrenSetSlotBytes(vm, slot + 1,
	    e->text != NULL ? e->text : "", e->len);
	wrenInsertInList(vm, slot, -1, slot + 1);

	e->cached_value = wrenGetSlotHandle(vm, slot);
	free(e->text);
	e->text = NULL;
	/* Re-emit through the cached handle so the slot caller sees the
	 * same value as subsequent reads will. */
	wrenSetSlotHandle(vm, slot, e->cached_value);
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
	log_append(WREN_LOG_PRINT, text);
}

static void
host_error_fn(WrenVM *vm, WrenErrorType type, const char *module, int line,
    const char *message)
{
	(void)vm;
	char buf[1024];
	const char *tag = "?";
	switch (type) {
		case WREN_ERROR_COMPILE:
			snprintf(buf, sizeof(buf), "%s:%d: %s",
			    module ? module : "?", line, message ? message : "");
			log_append(WREN_LOG_COMPILE_ERROR, buf);
			tag = "compile";
			break;
		case WREN_ERROR_RUNTIME:
			log_append(WREN_LOG_RUNTIME_ERROR,
			    message ? message : "");
			snprintf(buf, sizeof(buf), "%s",
			    message ? message : "");
			tag = "runtime";
			break;
		case WREN_ERROR_STACK_TRACE:
			snprintf(buf, sizeof(buf), "%s:%d in %s",
			    module ? module : "?", line, message ? message : "");
			log_append(WREN_LOG_STACK_FRAME, buf);
			tag = "trace";
			break;
	}
	/* TODO: drop this stderr fallback once the console is solid.
	 * Useful during bring-up when load failures happen before the
	 * user can open the REPL to see them. */
	fprintf(stderr, "[wren %s] %s\n", tag, buf);
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

/* Compute a module name from a script path: basename, sans ".wren". */
static void
modname_from_path(const char *path, char *out, size_t outsz)
{
	const char *base = strrchr(path, '/');
#ifdef _WIN32
	const char *base2 = strrchr(path, '\\');
	if (base2 != NULL && (base == NULL || base2 > base))
		base = base2;
#endif
	base = (base == NULL) ? path : base + 1;
	strncpy(out, base, outsz - 1);
	out[outsz - 1] = '\0';
	char *dot = strrchr(out, '.');
	if (dot != NULL)
		*dot = '\0';
}

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

	char modname[256];
	modname_from_path(path, modname, sizeof(modname));

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

	/* Two-phase load order:
	 *   1. Glob the user-script dir to learn which module names the
	 *      user has provided.
	 *   2. Walk EMBEDDED_SCRIPTS[], interpreting each whose name is
	 *      NOT in the user set.  User overrides any embedded with
	 *      the same module name.
	 *   3. Interpret the user scripts. */
	glob_t gl;
	memset(&gl, 0, sizeof(gl));
	bool have_user_dir = false;
	if (get_syncterm_filename(dir, sizeof(dir), SYNCTERM_PATH_SCRIPTS, false)
	    != NULL) {
		char pat[MAX_PATH + 16];
		snprintf(pat, sizeof(pat), "%s*.wren", dir);
		if (glob(pat, 0, NULL, &gl) == 0)
			have_user_dir = true;
	}

	for (const struct embedded_script *es = EMBEDDED_SCRIPTS;
	    es->name != NULL; es++) {
		bool overridden = false;
		if (have_user_dir) {
			for (size_t i = 0; i < gl.gl_pathc && !overridden; i++) {
				char umod[256];
				modname_from_path(gl.gl_pathv[i], umod, sizeof(umod));
				if (strcmp(umod, es->name) == 0)
					overridden = true;
			}
		}
		if (overridden)
			continue;
		if (wrenInterpret(g_state.vm, es->name, es->source) !=
		    WREN_RESULT_SUCCESS)
			fprintf(stderr, "[wren] embedded %s: load failed\n",
			    es->name);
	}

	if (have_user_dir) {
		for (size_t i = 0; i < gl.gl_pathc; i++)
			load_one_script(gl.gl_pathv[i]);
		globfree(&gl);
	}
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

	/* Release log-buffer cached handles before freeing the VM (they
	 * reference Wren-heap values). */
	wren_log_clear();

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
