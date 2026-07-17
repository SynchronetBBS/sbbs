/* Per-connection Wren scripting host for SyncTERM.  The VM, hook
 * registry, and dispatchers all live here; the foreign-method bodies
 * (Screen/Input/Clipboard/Conn/CTerm/BBS/Cache/Hook) live in wren_bind.c.
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
#include "wren_bind_screen.h"
#include "wren_bind_xfer.h"

#include "wren.h"

#include "syncterm.h"
#include "bbslist.h"
#include "ciolib.h"
#include "dirwrap.h"
#include "genwrap.h"      /* xp_timer */
#include "term.h"         /* doterm_wake */
#include "threadwrap.h"   /* pthread_self */
#include "regexp.h"       /* RE1: Prog, PikeVM, pikevm_* */
#include "ansi_filter.h"  /* shared escape-sequence stripper */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------------
 * State
 * -------------------------------------------------------------------- */

static struct wren_host_state state;
static struct wren_host_state *selected_state;
static bool      active;
static pthread_t owner_thread;
static bool      physical_key_events_requested;

/* -W <path>: extra script loaded at the end of every wren_host_init.
 * NULL when not set (the common case).  Exposed to scripts via
 * Host.launchScript so they can detect command-line invocation. */
static char *launch_script = NULL;

static bool on_owner_thread(void);

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

struct wren_log {
	struct wren_log_entry entries[WREN_LOG_CAPACITY];
	int                   head;
	int                   count;
	uint64_t              total;         /* monotonic; survives clear */
	uint64_t              error_total;   /* same, counting only error
	                                       * entries (compile error,
	                                       * runtime error, stack frame) */
};

/* The user-visible scrollback. */
static struct wren_log main_log;

/* Side log used during a REPL.eval to capture compile errors that may
 * or may not survive to the user-visible log.  When a captured set
 * contains "Expected expression." (the parser's signal that input
 * isn't an expression), REPL.eval drops the side log and retries as a
 * statement.  Otherwise the captured entries are merged into the main
 * log via wren_log_capture_commit. */
static struct wren_log capture_log;

/* `log_append` writes here.  Default target is the main log; the
 * capture API points it at capture_log between start and
 * commit/clear, redirecting writes without touching the main log at
 * all.  Wren is single-threaded in our use, so no synchronization. */
static struct wren_log *log_target = &main_log;

static void
log_release_slot(struct wren_log_entry *e)
{
	if (e->cached_value != NULL && state.vm != NULL) {
		wrenReleaseHandle(state.vm, e->cached_value);
		e->cached_value = NULL;
	}
	if (e->text != NULL) {
		free(e->text);
		e->text = NULL;
	}
	e->len = 0;
}

static void
log_drop_all(struct wren_log *log)
{
	for (int i = 0; i < WREN_LOG_CAPACITY; i++)
		log_release_slot(&log->entries[i]);
	log->head  = 0;
	log->count = 0;
	/* total intentionally NOT reset — Wren scripts may track a
	 * high-water mark; resetting would make existing marks falsely
	 * reappear as "new writes."  Buffer is empty; next write gets
	 * seq = log->total (no skip). */
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

	struct wren_log       *log = log_target;
	struct wren_log_entry *e   = &log->entries[log->head];
	log_release_slot(e);
	e->seq    = log->total++;
	if (source != WREN_LOG_PRINT)
		log->error_total++;
	e->ts     = xp_timer();
	e->source = source;
	e->text   = buf;
	e->len    = alloc;

	log->head = (log->head + 1) % WREN_LOG_CAPACITY;
	if (log->count < WREN_LOG_CAPACITY)
		log->count++;
}

/* --------------------------------------------------------------------
 * Capture API: redirect log_append to a side log between start and
 * commit/clear.  REPL.eval uses this to preview compile errors from an
 * expression-mode attempt before deciding whether to surface them.
 * -------------------------------------------------------------------- */

void
wren_log_capture_start(void)
{
	log_drop_all(&capture_log);
	capture_log.total = 0;
	log_target = &capture_log;
}

bool
wren_log_capture_active(void)
{
	return log_target == &capture_log;
}

bool
wren_log_capture_contains(const char *needle)
{
	if (needle == NULL)
		return false;
	int n     = capture_log.count;
	int start = (capture_log.head - n + WREN_LOG_CAPACITY) % WREN_LOG_CAPACITY;
	for (int i = 0; i < n; i++) {
		struct wren_log_entry *e =
		    &capture_log.entries[(start + i) % WREN_LOG_CAPACITY];
		if (e->text != NULL && strstr(e->text, needle) != NULL)
			return true;
	}
	return false;
}

void
wren_log_capture_clear(void)
{
	log_drop_all(&capture_log);
	log_target = &main_log;
}

void
wren_log_capture_commit(void)
{
	/* Swap target back to main, then replay each captured entry
	 * through log_append so it lands in main with a fresh ring slot.
	 * Order is oldest-first; we walk from (head - count) forward. */
	log_target = &main_log;
	int n     = capture_log.count;
	int start = (capture_log.head - n + WREN_LOG_CAPACITY) % WREN_LOG_CAPACITY;
	for (int i = 0; i < n; i++) {
		struct wren_log_entry *e =
		    &capture_log.entries[(start + i) % WREN_LOG_CAPACITY];
		if (e->text != NULL)
			log_append(e->source, e->text);
	}
	log_drop_all(&capture_log);
}

int
wren_log_count(void)
{
	return main_log.count;
}

uint64_t
wren_log_total(void)
{
	return main_log.total;
}

/* High-water marks of main_log.total / .error_total at the time the
 * user last left the Wren console.  wren_host_log_unread() is true
 * while either has advanced; wren_host_log_unread_error() distinguishes
 * the error case so the status indicator can tint accordingly. */
static uint64_t s_log_seen_total;
static uint64_t s_log_seen_error_total;

bool
wren_host_log_unread(void)
{
	return main_log.total > s_log_seen_total;
}

bool
wren_host_log_unread_error(void)
{
	return main_log.error_total > s_log_seen_error_total;
}

void
wren_host_mark_log_seen(void)
{
	s_log_seen_total       = main_log.total;
	s_log_seen_error_total = main_log.error_total;
}

bool
wren_host_alert(const char *title, const char *message)
{
	if (!active || !on_owner_thread() || state.vm == NULL ||
	    state.host_popup_class == NULL || state.host_alert_handle == NULL)
		return false;

	wrenEnsureSlots(state.vm, 3);
	wrenSetSlotHandle(state.vm, 0, state.host_popup_class);
	wrenSetSlotString(state.vm, 1, title == NULL ? "Alert" : title);
	wrenSetSlotString(state.vm, 2, message == NULL ? "" : message);
	return wrenCall(state.vm, state.host_alert_handle) == WREN_RESULT_SUCCESS;
}

void
wren_log_clear(void)
{
	log_drop_all(&main_log);
}

void
wren_log_emit(WrenVM *vm, uint64_t seq, int slot)
{
	struct wren_log *log = &main_log;
	/* Valid range: oldest seq = log->total - log->count;
	 * newest seq = log->total - 1. */
	if (log->count == 0 ||
	    seq < log->total - (uint64_t)log->count ||
	    seq >= log->total) {
		wrenSetSlotNull(vm, slot);
		return;
	}
	/* Newest entry sits at (head - 1) mod cap.  Walking backward by
	 * (log->total - 1 - seq) gives the slot for `seq`. */
	int back = (int)(log->total - 1 - seq);
	int idx  = ((log->head - 1 - back) % WREN_LOG_CAPACITY +
	            WREN_LOG_CAPACITY) % WREN_LOG_CAPACITY;
	struct wren_log_entry *e = &log->entries[idx];

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
	if (selected_state != NULL)
		return selected_state;
	return active ? &state : NULL;
}

struct wren_host_state *
wren_host_select_state(struct wren_host_state *new_state)
{
	struct wren_host_state *old = selected_state;
	selected_state = new_state;
	return old;
}

bool
wren_host_take_pending_disconnect(bool *exit_app_out)
{
	if (!active || !state.pending_disconnect)
		return false;
	state.pending_disconnect = false;
	if (exit_app_out != NULL)
		*exit_app_out = state.pending_disconnect_exit;
	state.pending_disconnect_exit = false;
	return true;
}

bool
wren_host_active(void)
{
	if (!active)
		return false;
	/* Cheap fast-path: avoid a pthread_self() + compare on every call
	 * site by allowing a positive answer from any thread; only the
	 * dispatchers themselves enforce thread affinity. */
	return true;
}

void
wren_host_set_launch_script(const char *path)
{
	free(launch_script);
	launch_script = (path != NULL) ? strdup(path) : NULL;
}

const char *
wren_host_launch_script(void)
{
	return launch_script;
}

void
wren_host_bind_cterm_suspended(bool *flag)
{
	state.cterm_suspended = flag;
}

void
wren_host_bind_ooii_mode(int *mode)
{
	state.ooii_mode = mode;
}

void
wren_host_bind_speed(int *speed)
{
	state.speed = speed;
}

/* --------------------------------------------------------------------
 * Result queue
 * -------------------------------------------------------------------- */

static void
wren_result_push_(WrenHandle *fiber, void *data,
    wren_result_deliver_fn deliver, wren_result_free_fn free_fn,
    bool input_shaped)
{
	if (fiber == NULL)
		return;
	if (!active) {
		/* Shutdown race: drop on the floor and clean up. */
		if (free_fn != NULL)
			free_fn(data);
		return;
	}
	struct wren_result *r = calloc(1, sizeof(*r));
	if (r == NULL) {
		/* OOM: leak the fiber handle (no VM call available from
		 * an arbitrary thread anyway).  free the data. */
		if (free_fn != NULL)
			free_fn(data);
		return;
	}
	r->fiber   = fiber;
	r->data    = data;
	r->deliver = deliver;
	r->free_fn = free_fn;
	r->input_shaped = input_shaped;
	r->input_epoch = state.input_epoch;

	pthread_mutex_lock(&state.result_mutex);
	if (state.result_tail == NULL)
		state.result_head = r;
	else
		state.result_tail->next = r;
	state.result_tail = r;
	pthread_mutex_unlock(&state.result_mutex);
	/* Wake the doterm() main loop so the result drains on the next
	 * iteration without waiting for the WaitForEvent timeout. */
	doterm_wake();
}

void
wren_result_push(WrenHandle *fiber, void *data,
    wren_result_deliver_fn deliver, wren_result_free_fn free_fn)
{
	wren_result_push_(fiber, data, deliver, free_fn, false);
}

void
wren_result_push_input(WrenHandle *fiber, void *data,
    wren_result_deliver_fn deliver, wren_result_free_fn free_fn)
{
	wren_result_push_(fiber, data, deliver, free_fn, true);
}

/* Returns true if the fiber has finished or aborted.  Any error from
 * the isDone primitive itself is treated as "done" so a broken fiber
 * doesn't keep results queued indefinitely.  Non-static — also used
 * by wren_bind_screen.c's claim-stack auto-prune. */
bool
fiber_is_done(WrenHandle *fiber)
{
	if (state.fiber_isdone_handle == NULL)
		return false;
	wrenEnsureSlots(state.vm, 1);
	wrenSetSlotHandle(state.vm, 0, fiber);
	if (wrenCall(state.vm, state.fiber_isdone_handle) !=
	    WREN_RESULT_SUCCESS)
		return true;
	if (wrenGetSlotType(state.vm, 0) != WREN_TYPE_BOOL)
		return false;
	return wrenGetSlotBool(state.vm, 0);
}

void
wren_result_drain(void)
{
	if (!active || !on_owner_thread())
		return;

	/* Detach the entire batch under the lock; deliveries happen
	 * lock-free below.  Workers can keep pushing during delivery —
	 * those entries land on a fresh head and get drained next pass. */
	pthread_mutex_lock(&state.result_mutex);
	struct wren_result *r = state.result_head;
	state.result_head = state.result_tail = NULL;
	pthread_mutex_unlock(&state.result_mutex);

	while (r != NULL) {
		struct wren_result *next = r->next;

		if ((!r->input_shaped || r->input_epoch == state.input_epoch) &&
		    !fiber_is_done(r->fiber)) {
			wrenEnsureSlots(state.vm, 3);
			wrenSetSlotHandle(state.vm, 0, r->fiber);
			r->deliver(state.vm, 1, r->data);
			wrenCall(state.vm, state.call1_handle);
		}

		wrenReleaseHandle(state.vm, r->fiber);
		if (r->free_fn != NULL)
			r->free_fn(r->data);
		free(r);

		r = next;
	}
}

void
wren_host_input_barrier(void)
{
	if (active)
		state.input_epoch++;
	ciolib_clear_input();
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
	switch (type) {
		case WREN_ERROR_COMPILE:
			snprintf(buf, sizeof(buf), "%s:%d: %s",
			    module ? module : "?", line, message ? message : "");
			log_append(WREN_LOG_COMPILE_ERROR, buf);
			fprintf(stderr, "[wren] compile %s\n", buf);
			break;
		case WREN_ERROR_RUNTIME:
			log_append(WREN_LOG_RUNTIME_ERROR,
			    message ? message : "");
			fprintf(stderr, "[wren] runtime %s\n",
			    message ? message : "");
			break;
		case WREN_ERROR_STACK_TRACE:
			snprintf(buf, sizeof(buf), "%s:%d in %s",
			    module ? module : "?", line, message ? message : "");
			log_append(WREN_LOG_STACK_FRAME, buf);
			fprintf(stderr, "[wren]   at %s\n", buf);
			break;
	}
}

static WrenForeignMethodFn
host_bind_foreign_method(WrenVM *vm, const char *module, const char *className,
    bool isStatic, const char *signature)
{
	(void)vm;
	return wren_bind_lookup(module, className, isStatic, signature);
}

static WrenForeignClassMethods
host_bind_foreign_class(WrenVM *vm, const char *module, const char *className)
{
	(void)vm;
	return wren_bind_lookup_class(module, className);
}

/* Module loader: resolves `import "name"` against
 * <SYNCTERM_PATH_SCRIPTS>/load/<name>.wren.  Module names are
 * restricted to [A-Za-z0-9_]; an invalid name is reported as a
 * runtime error by synthesizing a one-line module body that calls
 * Fiber.abort, so the import fails with a clear message instead of
 * silently looking like "module not found." */
static void
host_load_module_complete(WrenVM *vm, const char *name,
    WrenLoadModuleResult result)
{
	(void)vm;
	(void)name;
	if (result.source != NULL)
		free((void *)result.source);
}

static bool
valid_module_name(const char *name)
{
	if (name == NULL || *name == '\0')
		return false;
	for (const char *p = name; *p != '\0'; p++) {
		unsigned char c = (unsigned char)*p;
		if (!((c >= 'A' && c <= 'Z') ||
		      (c >= 'a' && c <= 'z') ||
		      (c >= '0' && c <= '9') ||
		      c == '_'))
			return false;
	}
	return true;
}

/* Slurp a Wren source file into a malloc'd, NUL-terminated buffer.
 * Caps oversized files at 4 MB so a runaway script can't exhaust
 * memory.  Returns NULL on any failure. */
static char *
read_script_file(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (f == NULL)
		return NULL;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz < 0 || sz > 4 * 1024 * 1024) {
		fclose(f);
		return NULL;
	}
	char *src = malloc((size_t)sz + 1);
	if (src == NULL) {
		fclose(f);
		return NULL;
	}
	size_t got = fread(src, 1, (size_t)sz, f);
	fclose(f);
	src[got] = '\0';
	return src;
}

/* Resolve a module name to its source.  Search order:
 *   1. scripts/<name>.wren                   (user library module)
 *   2. scripts/auto/connected/<name>.wren    (catches imports of
 *                                             auto-load entry scripts
 *                                             before they self-load)
 *   3. EMBEDDED_SCRIPTS                      (built-in fallback)
 *
 * Disk hits are malloc'd and freed by host_load_module_complete; the
 * embedded path returns a static pointer with onComplete == NULL so
 * Wren skips the free. */
static WrenLoadModuleResult
host_load_module(WrenVM *vm, const char *name)
{
	(void)vm;
	WrenLoadModuleResult res = { NULL, NULL, NULL };
	static const char denied_menu_module[] =
	    "Fiber.abort(\"syncterm_menu is unavailable in a connected "
	    "session\")\n";

	if (!valid_module_name(name)) {
		res.source =
		    "Fiber.abort(\"invalid Wren module name: only "
		    "[A-Za-z0-9_] allowed\")\n";
		return res;
	}
	if (strcmp(name, "syncterm_menu") == 0) {
		res.source = denied_menu_module;
		return res;
	}

	char dir[MAX_PATH + 1];
	if (get_syncterm_filename(dir, sizeof(dir), SYNCTERM_PATH_SCRIPTS,
	    false) != NULL) {
		char path[MAX_PATH + 1];
		int  n;

		n = snprintf(path, sizeof(path), "%s%s.wren", dir, name);
		if (n > 0 && (size_t)n < sizeof(path)) {
			char *src = read_script_file(path);
			if (src != NULL) {
				res.source     = src;
				res.onComplete = host_load_module_complete;
				return res;
			}
		}

		n = snprintf(path, sizeof(path), "%sauto/connected/%s.wren",
		    dir, name);
		if (n > 0 && (size_t)n < sizeof(path)) {
			char *src = read_script_file(path);
			if (src != NULL) {
				res.source     = src;
				res.onComplete = host_load_module_complete;
				return res;
			}
		}
	}

	for (const struct embedded_script *es = EMBEDDED_SCRIPTS;
	    es->name != NULL; es++) {
		if (strcmp(es->name, name) == 0 &&
		    (es->event == NULL ||
		    strcmp(es->event, "connected") == 0)) {
			/* Static source; leave onComplete NULL. */
			res.source = es->source;
			return res;
		}
	}

	return res;
}

/* --------------------------------------------------------------------
 * Hook + timer registration
 * -------------------------------------------------------------------- */

/* Time the wrenCall, fold the elapsed wall-clock into a metrics
 * block.  Caller has set up slot 0 (and any args) already.  Returns
 * the wrenCall result so the caller can branch on success/failure. */
static WrenInterpretResult
metrics_invoke(struct hook_metrics *m, WrenHandle *call_handle)
{
	long double t0 = xp_timer();
	WrenInterpretResult r = wrenCall(state.vm, call_handle);
	long double dt = xp_timer() - t0;
	if (dt < 0)
		dt = 0;
	m->call_count++;
	m->total_runtime_s += dt;
	if (m->call_count == 1)
		m->min_runtime_s = dt;
	else if (dt < m->min_runtime_s)
		m->min_runtime_s = dt;
	if (dt > m->max_runtime_s)
		m->max_runtime_s = dt;
	return r;
}

static struct wren_hook_entry *
register_hook_internal(WrenVM *vm, enum wren_hook_event ev, int fn_slot,
    bool filtered, int filter)
{
	if (!active || ev < 0 || ev >= WREN_HOOK_COUNT)
		return NULL;
	int n = state.hook_count[ev];
	if (n >= WREN_HOST_MAX_HOOKS_PER_EVENT) {
		fprintf(stderr, "[wren] hook limit reached for event %d\n", (int)ev);
		return NULL;
	}
	struct wren_hook_entry *h = calloc(1, sizeof(*h));
	if (h == NULL)
		return NULL;
	h->ev       = ev;
	h->fn       = wrenGetSlotHandle(vm, fn_slot);
	h->filter   = filter;
	h->filtered = filtered;
	state.hooks[ev][n] = h;
	state.hook_count[ev] = n + 1;
	return h;
}

struct wren_hook_entry *
wren_host_register_hook(WrenVM *vm, enum wren_hook_event ev, int fn_slot)
{
	return register_hook_internal(vm, ev, fn_slot, false, 0);
}

struct wren_hook_entry *
wren_host_register_hook_filtered(WrenVM *vm, enum wren_hook_event ev,
    int fn_slot, int filter)
{
	return register_hook_internal(vm, ev, fn_slot, true, filter);
}

struct wren_hook_entry *
wren_host_register_hook_match_ex(WrenVM *vm, int fn_slot,
    struct Prog *prog, struct PikeVM *vm_state,
    char *buf, size_t buf_cap, int nsubp, bool clean)
{
	if (!active)
		return NULL;
	int n = state.hook_count[WREN_HOOK_INPUT];
	if (n >= WREN_HOST_MAX_HOOKS_PER_EVENT) {
		fprintf(stderr, "[wren] hook limit reached for input\n");
		return NULL;
	}
	struct wren_hook_entry *h = calloc(1, sizeof(*h));
	if (h == NULL)
		return NULL;
	h->ev            = WREN_HOOK_INPUT;
	h->fn            = wrenGetSlotHandle(vm, fn_slot);
	h->regex_prog    = prog;
	h->regex_vm      = vm_state;
	h->regex_buf     = buf;
	h->regex_buf_cap = buf_cap;
	h->regex_nsubp   = nsubp;
	h->regex_clean   = clean;
	h->regex_ansi_state = ANSI_STATE_NONE;
	pikevm_start(vm_state, buf);
	state.hooks[WREN_HOOK_INPUT][n] = h;
	state.hook_count[WREN_HOOK_INPUT] = n + 1;
	return h;
}

struct wren_hook_entry *
wren_host_register_hook_match(WrenVM *vm, int fn_slot,
    struct Prog *prog, struct PikeVM *vm_state,
    char *buf, size_t buf_cap, int nsubp)
{
	return wren_host_register_hook_match_ex(vm, fn_slot, prog, vm_state,
	    buf, buf_cap, nsubp, false);
}

struct wren_hook_entry *
wren_host_register_timer(WrenVM *vm, int ms, int fn_slot)
{
	if (!active)
		return NULL;
	if (ms < 1)
		ms = 1;
	if (state.timer_count >= WREN_HOST_MAX_TIMERS) {
		fprintf(stderr, "[wren] timer limit reached\n");
		return NULL;
	}
	struct wren_hook_entry *t = calloc(1, sizeof(*t));
	if (t == NULL)
		return NULL;
	t->ev          = WREN_HOOK_TIMER;
	t->fn          = wrenGetSlotHandle(vm, fn_slot);
	t->interval_ms = (uint32_t)ms;
	t->next_fire_ms = xp_fast_timer64_ms() + ms;
	state.timers[state.timer_count++] = t;
	return t;
}

/* Free regex resources owned by an entry, if any.  Used at compact
 * time and shutdown. */
static void
hook_entry_free_regex(struct wren_hook_entry *h)
{
	if (h->regex_prog != NULL) {
		free(h->regex_prog);
		h->regex_prog = NULL;
	}
	if (h->regex_vm != NULL) {
		pikevm_free(h->regex_vm);
		h->regex_vm = NULL;
	}
	if (h->regex_buf != NULL) {
		free(h->regex_buf);
		h->regex_buf = NULL;
	}
}

bool
wren_host_remove_entry(struct wren_hook_entry *h)
{
	if (!active || h == NULL || h->fn == NULL)
		return false;
	wrenReleaseHandle(state.vm, h->fn);
	h->fn = NULL;
	/* Push onto the cleanup queue; compact pops at the top of the
	 * next main-loop iteration.  The dispatcher already skips entries
	 * with fn == NULL, so it's safe to leave the pointer in the
	 * dispatch array until then. */
	h->next_cleanup    = state.cleanup_head;
	state.cleanup_head = h;
	return true;
}

/* Remove `h`'s pointer from a dispatch array of length *count, then
 * decrement the count.  Returns true on found-and-removed. */
static bool
dispatch_array_drop(struct wren_hook_entry **arr, int *count,
    struct wren_hook_entry *h)
{
	int n = *count;
	for (int i = 0; i < n; i++) {
		if (arr[i] != h)
			continue;
		for (int j = i; j < n - 1; j++)
			arr[j] = arr[j + 1];
		arr[n - 1] = NULL;
		*count = n - 1;
		return true;
	}
	return false;
}

void
wren_host_compact(void)
{
	if (!active || !on_owner_thread())
		return;
	while (state.cleanup_head != NULL) {
		struct wren_hook_entry *h = state.cleanup_head;
		state.cleanup_head = h->next_cleanup;
		h->next_cleanup    = NULL;

		hook_entry_free_regex(h);

		if (h->ev == WREN_HOOK_TIMER) {
			dispatch_array_drop(state.timers,
			    &state.timer_count, h);
		} else if (h->ev >= 0 && h->ev < WREN_HOOK_COUNT) {
			dispatch_array_drop(state.hooks[h->ev],
			    &state.hook_count[h->ev], h);
		}
		h->unregistered = true;

		/* If the script's already dropped its HookHandle, the
		 * struct has no remaining owner — free it.  Otherwise leave
		 * it alive so the script can still read .callCount etc.;
		 * the finalizer will free it once Wren GCs the handle. */
		if (h->gced)
			free(h);
	}
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

/* Run a user script as its own filename-derived module.  No special
 * routing for names that match an embedded script — overrides just
 * mean the embedded version of the same module is skipped during the
 * embedded iteration in wren_host_init.  Imports of other modules
 * (including "syncterm") resolve through host_load_module. */
static void
load_one_script(const char *path)
{
	char *src = read_script_file(path);
	if (src == NULL) {
		fprintf(stderr, "[wren] cannot read %s\n", path);
		return;
	}

	char modname[256];
	modname_from_path(path, modname, sizeof(modname));
	if (strcmp(modname, "syncterm_menu") == 0) {
		fprintf(stderr, "[wren] %s: reserved connected-session "
		    "module name\n", path);
		free(src);
		return;
	}

	WrenInterpretResult r = wrenInterpret(state.vm, modname, src);
	free(src);
	if (r != WREN_RESULT_SUCCESS)
		fprintf(stderr, "[wren] %s: load failed\n", path);
}

void
wren_host_init(struct bbslist *bbs)
{
	char dir[MAX_PATH + 1];

	if (active)
		return;

	memset(&state, 0, sizeof(state));
	state.bbs = bbs;
	state.input_epoch = 1;

	WrenConfiguration cfg;
	wrenInitConfiguration(&cfg);
	cfg.writeFn             = host_write_fn;
	cfg.errorFn             = host_error_fn;
	cfg.bindForeignMethodFn = host_bind_foreign_method;
	cfg.bindForeignClassFn  = host_bind_foreign_class;
	cfg.loadModuleFn        = host_load_module;
	state.vm = wrenNewVM(&cfg);
	if (state.vm == NULL)
		return;

	state.call0_handle = wrenMakeCallHandle(state.vm, "call()");
	state.call1_handle = wrenMakeCallHandle(state.vm, "call(_)");
	state.fiber_isdone_handle = wrenMakeCallHandle(state.vm, "isDone");

	pthread_mutex_init(&state.result_mutex, NULL);
	state.result_head = state.result_tail = NULL;

	owner_thread = pthread_self();
	active = true;
	selected_state = &state;

	/* Glob the user's auto-load dir for the "connected" event to detect
	 * overrides of embedded entry scripts and to discover any user-only
	 * auto-load files.  Pure library modules at scripts/<name>.wren are
	 * NOT auto-loaded — they live there to be imported on demand and
	 * are reached through host_load_module's search path. */
	const char *event = "connected";
	glob_t gl;
	memset(&gl, 0, sizeof(gl));
	bool have_user_dir = false;
	char auto_dir[MAX_PATH + 32];
	if (get_syncterm_filename(dir, sizeof(dir), SYNCTERM_PATH_SCRIPTS, false)
	    != NULL) {
		snprintf(auto_dir, sizeof(auto_dir), "%sauto/%s/", dir, event);
		char pat[MAX_PATH + 32];
		snprintf(pat, sizeof(pat), "%s*.wren", auto_dir);
		if (glob(pat, 0, NULL, &gl) == 0)
			have_user_dir = true;
	}

	/* Run each embedded entry script for this event as its own
	 * filename-named module.  Skip any embed the user has overridden
	 * with a same-named file in their auto-load dir (the user-script
	 * loop runs that version instead), and any module already loaded —
	 * that catches modules an earlier entry script's `import` statement
	 * has already pulled in (notably `syncterm`, which has no top-level
	 * side effects so lazy loading is fine), and avoids re-compile
	 * errors against the existing module.  Library embeds (event ==
	 * NULL) are skipped — they load on first import via
	 * host_load_module's EMBEDDED_SCRIPTS fallback. */
	for (const struct embedded_script *es = EMBEDDED_SCRIPTS;
	    es->name != NULL; es++) {
		if (es->event == NULL || strcmp(es->event, event) != 0)
			continue;
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
		if (wrenHasModule(state.vm, es->name))
			continue;
		if (wrenInterpret(state.vm, es->name, es->source) !=
		    WREN_RESULT_SUCCESS)
			fprintf(stderr, "[wren] embedded %s: load failed\n",
			    es->name);
	}

	/* Run user auto-load files as their own filename-named modules.
	 * Skip any already loaded for the same reason as above. */
	if (have_user_dir) {
		for (size_t i = 0; i < gl.gl_pathc; i++) {
			char umod[256];
			modname_from_path(gl.gl_pathv[i], umod, sizeof(umod));
			if (wrenHasModule(state.vm, umod))
				continue;
			load_one_script(gl.gl_pathv[i]);
		}
		globfree(&gl);
	}

	/* -W: extra script supplied on the command line, loaded last so
	 * its imports resolve against the full embedded + user surface.
	 * Skip if a module of the same filename-derived name is already
	 * loaded — happens when the file is symlinked into the user-dir
	 * scripts/ tree, or has been pulled in by an `import` from another
	 * already-loaded module (e.g. auto/connected/runtests.wren imports
	 * "wrentest" which finds it via host_load_module).  In either
	 * case the module's top-level code (including any Host.launchScript
	 * auto-run guard) has already executed once, so re-interpreting
	 * would just trip "Module variable is already defined" errors. */
	if (launch_script != NULL) {
		char modname[256];
		modname_from_path(launch_script, modname, sizeof(modname));
		if (!wrenHasModule(state.vm, modname))
			load_one_script(launch_script);
	}

	/* Cache Hook + its dispatch_ method handles.  Every hook fire
	 * goes through Hook.dispatch_(fn[, arg]) instead of fn.call()
	 * so a handler that yields directly is caught and reported
	 * rather than silently stranding the dispatcher with no return
	 * value to act on.  Both forms are required: dispatch0 for
	 * timer fires (no arg), dispatch1 for everything else. */
	wrenEnsureSlots(state.vm, 1);
	wrenGetVariable(state.vm, "syncterm", "Hook", 0);
	state.hook_class = wrenGetSlotHandle(state.vm, 0);
	state.dispatch0_handle = wrenMakeCallHandle(state.vm, "dispatch_(_)");
	state.dispatch1_handle = wrenMakeCallHandle(state.vm, "dispatch_(_,_)");

	if (wrenHasVariable(state.vm, "host_popup", "HostPopup")) {
		wrenGetVariable(state.vm, "host_popup", "HostPopup", 0);
		state.host_popup_class = wrenGetSlotHandle(state.vm, 0);
		state.host_alert_handle = wrenMakeCallHandle(state.vm,
		    "alert(_,_)");
	}
}

void
wren_host_shutdown(void)
{
	if (!active)
		return;

	/* Stop any active transfer-window worker thread first so its
	 * mutexes / atomics aren't accessed after we tear down. */
	wren_xfer_shutdown();

	/* Drain the live dispatch arrays.  Each entry struct may still be
	 * referenced by a HookHandle on the Wren side; the VM teardown
	 * below will fire the foreign-class finalizer for those handles
	 * and free anything we leave behind here.  But the fn handle and
	 * any regex resources are owned by us — release them now while the
	 * VM is still around. */
	for (int e = 0; e < WREN_HOOK_COUNT; e++) {
		for (int i = 0; i < state.hook_count[e]; i++) {
			struct wren_hook_entry *h = state.hooks[e][i];
			if (h == NULL)
				continue;
			if (h->fn != NULL) {
				wrenReleaseHandle(state.vm, h->fn);
				h->fn = NULL;
			}
			hook_entry_free_regex(h);
			h->unregistered = true;
			if (h->gced)
				free(h);
			state.hooks[e][i] = NULL;
		}
		state.hook_count[e] = 0;
	}
	for (int i = 0; i < state.timer_count; i++) {
		struct wren_hook_entry *t = state.timers[i];
		if (t == NULL)
			continue;
		if (t->fn != NULL) {
			wrenReleaseHandle(state.vm, t->fn);
			t->fn = NULL;
		}
		t->unregistered = true;
		if (t->gced)
			free(t);
		state.timers[i] = NULL;
	}
	state.timer_count = 0;

	/* Drain the cleanup queue too — these are entries that were
	 * removed mid-session but whose compaction never happened (e.g.
	 * shutdown fires from a code path that bypassed the main loop). */
	while (state.cleanup_head != NULL) {
		struct wren_hook_entry *h = state.cleanup_head;
		state.cleanup_head = h->next_cleanup;
		hook_entry_free_regex(h);
		h->unregistered = true;
		if (h->gced)
			free(h);
	}

	if (state.call0_handle != NULL)
		wrenReleaseHandle(state.vm, state.call0_handle);
	if (state.call1_handle != NULL)
		wrenReleaseHandle(state.vm, state.call1_handle);
	if (state.dispatch0_handle != NULL)
		wrenReleaseHandle(state.vm, state.dispatch0_handle);
	if (state.dispatch1_handle != NULL)
		wrenReleaseHandle(state.vm, state.dispatch1_handle);
	if (state.host_alert_handle != NULL)
		wrenReleaseHandle(state.vm, state.host_alert_handle);
	if (state.hook_class != NULL)
		wrenReleaseHandle(state.vm, state.hook_class);
	if (state.host_popup_class != NULL)
		wrenReleaseHandle(state.vm, state.host_popup_class);
	if (state.cell_class != NULL)
		wrenReleaseHandle(state.vm, state.cell_class);
	if (state.surface_class != NULL)
		wrenReleaseHandle(state.vm, state.surface_class);
	if (state.scrollback_class != NULL)
		wrenReleaseHandle(state.vm, state.scrollback_class);
	if (state.key_event_class != NULL)
		wrenReleaseHandle(state.vm, state.key_event_class);
	if (state.physical_key_event_class != NULL)
		wrenReleaseHandle(state.vm, state.physical_key_event_class);
	if (state.mouse_event_class != NULL)
		wrenReleaseHandle(state.vm, state.mouse_event_class);
	if (state.hook_handle_class != NULL)
		wrenReleaseHandle(state.vm, state.hook_handle_class);
	if (state.sftp_entry_class != NULL)
		wrenReleaseHandle(state.vm, state.sftp_entry_class);
	if (state.sftp_stat_class != NULL)
		wrenReleaseHandle(state.vm, state.sftp_stat_class);
	if (state.sftp_handle_class != NULL)
		wrenReleaseHandle(state.vm, state.sftp_handle_class);
	if (state.sftp_error_class != NULL)
		wrenReleaseHandle(state.vm, state.sftp_error_class);
	if (state.file_error_class != NULL)
		wrenReleaseHandle(state.vm, state.file_error_class);
	if (state.won_error_class != NULL)
		wrenReleaseHandle(state.vm, state.won_error_class);
	if (state.conn_error_class != NULL)
		wrenReleaseHandle(state.vm, state.conn_error_class);
	if (state.timer_elapsed_class != NULL)
		wrenReleaseHandle(state.vm, state.timer_elapsed_class);
	if (state.status_callable != NULL)
		wrenReleaseHandle(state.vm, state.status_callable);
	if (state.status_surface != NULL)
		wrenReleaseHandle(state.vm, state.status_surface);
	state.status_surface_width = 0;
	for (int i = 0; i < state.pending_timer_count; i++) {
		if (state.pending_timers[i].fiber != NULL)
			wrenReleaseHandle(state.vm,
			    state.pending_timers[i].fiber);
	}
	state.pending_timer_count = 0;
	wren_bind_claim_stack_clear();
	if (state.claim_handle_class != NULL) {
		wrenReleaseHandle(state.vm, state.claim_handle_class);
		state.claim_handle_class = NULL;
	}

	/* Drain any results still in the queue without delivering.  Workers
	 * may still be pushing, but `active` is going false right after
	 * this and subsequent pushes will release-and-free on their own.
	 * Loop until the queue stays empty under the lock. */
	for (;;) {
		pthread_mutex_lock(&state.result_mutex);
		struct wren_result *r = state.result_head;
		state.result_head = state.result_tail = NULL;
		pthread_mutex_unlock(&state.result_mutex);
		if (r == NULL)
			break;
		while (r != NULL) {
			struct wren_result *next = r->next;
			wrenReleaseHandle(state.vm, r->fiber);
			if (r->free_fn != NULL)
				r->free_fn(r->data);
			free(r);
			r = next;
		}
	}
	state.cterm_suspended = NULL;

	if (state.fiber_isdone_handle != NULL)
		wrenReleaseHandle(state.vm, state.fiber_isdone_handle);

	/* Release log-buffer cached handles before freeing the VM (they
	 * reference Wren-heap values). */
	wren_log_clear();

	wrenFreeVM(state.vm);
	state.vm = NULL;
	active = false;
	if (selected_state == &state)
		selected_state = NULL;
	physical_key_events_requested = false;
	pthread_mutex_destroy(&state.result_mutex);

	/* The transfer-arrow flags now live in Wren static fields
	 * (Host.uploadArrow / Host.downloadArrow) so they vanish with the
	 * VM on teardown — no C-side reset needed.
	 *
	 * Clear the SFTP-active flag so the next session's ssh.c teardown
	 * decision starts from a clean slate. */
	wren_sftp_active_reset();
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
	return pthread_self() == owner_thread;
}

bool
wren_host_wants_physical_keys(void)
{
#ifdef CIOLIB_KEY_EVENTS
	return active && physical_key_events_requested;
#else
	return false;
#endif
}

void
wren_host_enable_physical_key_events(void)
{
#ifdef CIOLIB_KEY_EVENTS
	physical_key_events_requested = true;
	ciokey_setenabled(true);
#else
	physical_key_events_requested = false;
#endif
}

/* C2 (observe-only) hooks ignore returns by design.  Some scripts try
 * to "consume" by returning Bool / String — silently ignored at the
 * dispatch level, but easy to misdiagnose as "my hook isn't firing."
 * Warn every time so the noise itself is the diagnostic; a misused
 * `every(50)` hook will log 20 lines/sec until the script is fixed,
 * which is the right pressure.  `who` is the hook-name token. */
static void
warn_if_c2_misused(const char *who)
{
	WrenType t = wrenGetSlotType(state.vm, 0);
	if (t != WREN_TYPE_BOOL && t != WREN_TYPE_STRING)
		return;
	char msg[160];
	snprintf(msg, sizeof(msg),
	    "Hook.%s callback returned %s; the value is ignored. "
	    "%s is observe-only; use Hook.onInput/onKey/"
	    "onPhysicalKey/onMouse to consume.",
	    who,
	    t == WREN_TYPE_BOOL ? "Bool" : "String",
	    who);
	log_append(WREN_LOG_RUNTIME_ERROR, msg);
}

static bool
read_consume_result(void)
{
	if (wrenGetSlotType(state.vm, 0) == WREN_TYPE_BOOL)
		return wrenGetSlotBool(state.vm, 0);
	return false;
}

bool
wren_host_dispatch_key(int key)
{
	if (!active || !on_owner_thread())
		return false;
	/* The input claim stack gets first crack — topmost (newest)
	 * claim wins.  Only non-mouse keys go to the claim dispatcher;
	 * the CIO_KEY_MOUSE marker falls through so dispatch_mouse can
	 * deliver the actual MouseEvent next.  If no claim consumed,
	 * fall through to the hook chain. */
	if (wren_bind_dispatch_claim_key(key))
		return true;
	int n = state.hook_count[WREN_HOOK_KEY];
	for (int i = 0; i < n; i++) {
		struct wren_hook_entry *h = state.hooks[WREN_HOOK_KEY][i];
		if (h == NULL || h->fn == NULL)
			continue;
		if (h->filtered && h->filter != key)
			continue;
		wrenEnsureSlots(state.vm, 3);
		wrenSetSlotHandle(state.vm, 0, state.hook_class);
		wrenSetSlotHandle(state.vm, 1, h->fn);
		wrenSetSlotDouble(state.vm, 2, (double)key);
		if (metrics_invoke(&h->metrics, state.dispatch1_handle) !=
		    WREN_RESULT_SUCCESS)
			continue;
		if (read_consume_result())
			return true;
	}
	return false;
}

bool
wren_host_dispatch_physical_key(const struct ciolib_key_event *ev)
{
	if (!active || !on_owner_thread() || ev == NULL)
		return false;
	if (wren_bind_dispatch_claim_physical_key(ev))
		return true;
	int n = state.hook_count[WREN_HOOK_PHYSICAL_KEY];
	if (n == 0)
		return false;
	for (int i = 0; i < n; i++) {
		struct wren_hook_entry *h = state.hooks[WREN_HOOK_PHYSICAL_KEY][i];
		if (h == NULL || h->fn == NULL)
			continue;
		if (h->filtered && h->filter != ev->evdev)
			continue;
		wrenEnsureSlots(state.vm, 4);
		wrenSetSlotHandle(state.vm, 0, state.hook_class);
		wrenSetSlotHandle(state.vm, 1, h->fn);
		push_physical_key_event(state.vm, ev, 2);
		if (metrics_invoke(&h->metrics, state.dispatch1_handle) !=
		    WREN_RESULT_SUCCESS)
			continue;
		if (read_consume_result())
			return true;
	}
	return false;
}

/* Build a Wren List for a regex match — slot `list_slot` receives
 * [match, group1, group2, ...] from the subp[] capture array.  Pairs
 * are walked until the first NULL/NULL pair.  Slot `list_slot + 1`
 * is used as scratch.  Caller must have ≥list_slot+2 Wren slots
 * ensured before calling. */
static void
build_match_list(char **subp, int nsubp, int list_slot)
{
	int scratch = list_slot + 1;
	wrenSetSlotNewList(state.vm, list_slot);
	for (int k = 0; k + 1 < nsubp; k += 2) {
		if (subp[k] == NULL || subp[k+1] == NULL)
			break;
		wrenSetSlotBytes(state.vm, scratch, subp[k],
		    (size_t)(subp[k+1] - subp[k]));
		wrenInsertInList(state.vm, list_slot, -1, scratch);
	}
}

/* Drive a single regex hook entry through any unfed bytes in its
 * buffer.  Handles MATCH (fire fn, trim, restart) and IMPOSSIBLE
 * (drop oldest byte, restart) inline; bounds itself by buf_len.
 * Regex hooks (both onMatch and onMatchClean) are passthrough-only —
 * the callback's return value is ignored.  An earlier "true drops
 * the trigger byte" contract was misleading: it dropped only the
 * single byte that completed the match (every prior byte already
 * went through cterm), so users never actually got "drop the
 * matched span."  Anyone needing wire-level drop should use
 * Hook.onInput, which is byte-granular by design. */
static void
dispatch_match_drain(struct wren_hook_entry *h)
{
	char *subp[MAXSUB];

	while (h->regex_pos < h->regex_buf_len) {
		int r = pikevm_step(h->regex_vm,
		    h->regex_buf + h->regex_pos);
		if (r == -1) {
			h->regex_pos++;
			continue;
		}
		if (r == 1) {
			pikevm_match(h->regex_vm, subp);
			wrenEnsureSlots(state.vm, 4);
			wrenSetSlotHandle(state.vm, 0, state.hook_class);
			wrenSetSlotHandle(state.vm, 1, h->fn);
			build_match_list(subp, h->regex_nsubp, 2);
			(void)metrics_invoke(&h->metrics,
			    state.dispatch1_handle);
			warn_if_c2_misused(
			    h->regex_clean ? "onMatchClean" : "onMatch");

			size_t end = (subp[1] != NULL)
			    ? (size_t)(subp[1] - h->regex_buf)
			    : h->regex_buf_len;
			if (end > h->regex_buf_len)
				end = h->regex_buf_len;
			memmove(h->regex_buf, h->regex_buf + end,
			    h->regex_buf_len - end);
			h->regex_buf_len -= end;
			h->regex_buf[h->regex_buf_len] = '\0';
			h->regex_pos = 0;
			pikevm_start(h->regex_vm, h->regex_buf);
			continue;
		}
		/* IMPOSSIBLE — drop one byte and re-feed survivors. */
		if (h->regex_buf_len <= 1) {
			h->regex_buf_len = 0;
			h->regex_buf[0] = '\0';
			h->regex_pos = 0;
			pikevm_start(h->regex_vm, h->regex_buf);
			break;
		}
		memmove(h->regex_buf, h->regex_buf + 1,
		    h->regex_buf_len - 1);
		h->regex_buf_len--;
		h->regex_buf[h->regex_buf_len] = '\0';
		h->regex_pos = 0;
		pikevm_start(h->regex_vm, h->regex_buf);
	}
}

int
wren_host_dispatch_input(unsigned char byte, char *out, int out_cap)
{
	if (!active || !on_owner_thread())
		return WREN_INPUT_KEEP;
	int n = state.hook_count[WREN_HOOK_INPUT];
	for (int i = 0; i < n; i++) {
		struct wren_hook_entry *h = state.hooks[WREN_HOOK_INPUT][i];
		if (h == NULL || h->fn == NULL)
			continue;

		if (h->regex_prog != NULL) {
			/* For onMatchClean: filter the byte through the
			 * shared ANSI parser; bytes inside an escape sequence
			 * never reach the regex VM, so the pattern matches
			 * the visible text only.  The ansi_filter call
			 * advances the per-hook state in place. */
			uint8_t        b = byte;
			uint8_t        kept;
			size_t         klen = 1;
			if (h->regex_clean) {
				enum ansi_state st =
				    (enum ansi_state)h->regex_ansi_state;
				klen = ansi_filter(&b, 1, &kept, &st,
				    ANSI_FILTER_KEEP_TEXT);
				h->regex_ansi_state = (int)st;
				if (klen == 0)
					continue;
				b = kept;
			}
			/* On overflow, drop the oldest half and restart so the
			 * survivors get re-fed by dispatch_match_drain. */
			if (h->regex_buf_len + 1 >= h->regex_buf_cap) {
				size_t drop = h->regex_buf_cap / 2 + 1;
				if (drop > h->regex_buf_len)
					drop = h->regex_buf_len;
				memmove(h->regex_buf, h->regex_buf + drop,
				    h->regex_buf_len - drop);
				h->regex_buf_len -= drop;
				h->regex_pos = 0;
				pikevm_start(h->regex_vm, h->regex_buf);
			}
			h->regex_buf[h->regex_buf_len++] = (char)b;
			h->regex_buf[h->regex_buf_len] = '\0';
			dispatch_match_drain(h);
			continue;
		}

		if (h->filtered && h->filter != (int)byte)
			continue;
		wrenEnsureSlots(state.vm, 3);
		wrenSetSlotHandle(state.vm, 0, state.hook_class);
		wrenSetSlotHandle(state.vm, 1, h->fn);
		wrenSetSlotDouble(state.vm, 2, (double)byte);
		if (metrics_invoke(&h->metrics, state.dispatch1_handle) !=
		    WREN_RESULT_SUCCESS)
			continue;

		WrenType t = wrenGetSlotType(state.vm, 0);
		if (t == WREN_TYPE_BOOL && wrenGetSlotBool(state.vm, 0))
			return WREN_INPUT_DROP;
		if (t == WREN_TYPE_STRING) {
			int len = 0;
			const char *s = wrenGetSlotBytes(state.vm, 0, &len);
			if (len < 0)
				len = 0;
			if (out == NULL || len > out_cap) {
				char msg[160];
				snprintf(msg, sizeof(msg),
				    "Hook.onInput: replacement of %d bytes "
				    "exceeds the %d-byte cap; keeping the "
				    "original byte 0x%02x",
				    len, out_cap, byte);
				log_append(WREN_LOG_RUNTIME_ERROR, msg);
				continue;
			}
			if (len > 0)
				memcpy(out, s, (size_t)len);
			return len;
		}
		/* Anything else (Num, null, instance, etc.) is a no-op
		 * for this hook; fall through to the next. */
	}
	return WREN_INPUT_KEEP;
}

bool
wren_host_dispatch_mouse(struct mouse_event *ev)
{
	if (!active || !on_owner_thread() || ev == NULL)
		return false;
	if (wren_bind_dispatch_claim_mouse(ev))
		return true;
	int n = state.hook_count[WREN_HOOK_MOUSE];
	if (n == 0)
		return false;
	/* Push a MouseEvent foreign as the callback's single argument.
	 * dispatch_(fn, arg) takes its arg in slot 2; push_mouse_event
	 * uses slot 3 as scratch for the class lookup, hence
	 * wrenEnsureSlots(4). */
	for (int i = 0; i < n; i++) {
		struct wren_hook_entry *h = state.hooks[WREN_HOOK_MOUSE][i];
		if (h == NULL || h->fn == NULL)
			continue;
		if (h->filtered && h->filter != ev->event)
			continue;
		wrenEnsureSlots(state.vm, 4);
		wrenSetSlotHandle(state.vm, 0, state.hook_class);
		wrenSetSlotHandle(state.vm, 1, h->fn);
		push_mouse_event(state.vm, ev, 2);
		if (metrics_invoke(&h->metrics, state.dispatch1_handle) !=
		    WREN_RESULT_SUCCESS)
			continue;
		if (read_consume_result())
			return true;
	}
	return false;
}

/* --------------------------------------------------------------------
 * Status bar render
 * --------------------------------------------------------------------
 *
 * The Wren-side default lives in scripts/auto/connected/status_default.wren
 * and installs itself via Status.callable=(fn).  Drop a same-named file
 * in the user's auto-load dir to override; or set Status.callable from
 * any script after the default has loaded.
 *
 * Per render: ensure a width×1 Surface exists (lazily allocated, recycled
 * across frames; reallocated on width change), fill its cells to the
 * default attribute (yellow on blue, spaces), then call the script's Fn
 * with the surface as its single argument.  The script mutates cells in
 * place; on return *out_buf points at the surface's underlying vmem_cell
 * buffer for the caller to vmem_puttext.
 *
 * Returns false (and leaves *out_buf untouched) if no callable is set,
 * the VM isn't on the owner thread, the surface allocation failed, or
 * the call failed -- term.c will fall back to the prefilled blank row in
 * that case. */
static void
status_surface_prefill(struct wren_surface *sf)
{
	if (sf == NULL || sf->buf == NULL)
		return;
	for (int i = 0; i < sf->count; i++) {
		sf->buf[i].ch           = 0x20;
		sf->buf[i].font         = 0;
		sf->buf[i].legacy_attr  = 0x1e;
		sf->buf[i].fg           = 0x80ffff54;
		sf->buf[i].bg           = 0x800000a8;
		sf->buf[i].hyperlink_id = 0;
	}
}

bool
wren_status_render(int width, struct vmem_cell **out_buf)
{
	if (!active || !on_owner_thread() || width <= 0 || out_buf == NULL)
		return false;
	if (state.status_callable == NULL)
		return false;
	if (state.surface_class == NULL) {
		wrenEnsureSlots(state.vm, 1);
		wrenGetVariable(state.vm, "syncterm", "Surface", 0);
		state.surface_class = wrenGetSlotHandle(state.vm, 0);
		if (state.surface_class == NULL)
			return false;
	}

	/* (Re)allocate the long-lived Surface on first call or width change.
	 * The previous surface's foreign data is held only via the released
	 * handle, so dropping the handle lets Wren's GC reclaim it. */
	if (state.status_surface == NULL ||
	    state.status_surface_width != width) {
		if (state.status_surface != NULL) {
			wrenReleaseHandle(state.vm, state.status_surface);
			state.status_surface = NULL;
		}
		wrenEnsureSlots(state.vm, 3);
		wrenSetSlotHandle(state.vm, 0, state.surface_class);
		wrenSetSlotDouble(state.vm, 1, (double)width);
		wrenSetSlotDouble(state.vm, 2, 1.0);
		wren_surface_allocate(state.vm);
		struct wren_surface *sf = wrenGetSlotForeign(state.vm, 0);
		if (sf == NULL || sf->buf == NULL || sf->count != width)
			return false;
		state.status_surface = wrenGetSlotHandle(state.vm, 0);
		state.status_surface_width = width;
	}

	wrenEnsureSlots(state.vm, 2);
	wrenSetSlotHandle(state.vm, 0, state.status_callable);
	wrenSetSlotHandle(state.vm, 1, state.status_surface);
	struct wren_surface *sf = wrenGetSlotForeign(state.vm, 1);
	if (sf == NULL || sf->buf == NULL)
		return false;
	status_surface_prefill(sf);

	if (wrenCall(state.vm, state.call1_handle) != WREN_RESULT_SUCCESS)
		return false;

	*out_buf = sf->buf;
	return true;
}

void
wren_host_dispatch_timer(void)
{
	if (!active || !on_owner_thread())
		return;
	int64_t now = xp_fast_timer64_ms();
	for (int i = 0; i < state.timer_count; i++) {
		struct wren_hook_entry *t = state.timers[i];
		if (t == NULL || t->fn == NULL || now < t->next_fire_ms)
			continue;
		wrenEnsureSlots(state.vm, 2);
		wrenSetSlotHandle(state.vm, 0, state.hook_class);
		wrenSetSlotHandle(state.vm, 1, t->fn);
		(void)metrics_invoke(&t->metrics, state.dispatch0_handle);
		warn_if_c2_misused("every");
		/* Advance by interval; if the loop stalled long enough that
		 * we're more than one interval behind, jump forward to "now"
		 * rather than firing repeatedly trying to catch up. */
		t->next_fire_ms += t->interval_ms;
		if (t->next_fire_ms < now)
			t->next_fire_ms = now + t->interval_ms;
	}
}

/* Fire every Hook.onShellClose / Hook.onDisconnect handler in
 * registration order.  No payload; handlers take no args.  Return
 * value is ignored (these aren't "consume the event" hooks).  Same
 * shape as the input/mouse dispatchers above, but using
 * dispatch0_handle since there's no argument to pass. */
static void
dispatch_zero_arg_event(enum wren_hook_event ev)
{
	if (!active || !on_owner_thread())
		return;
	int n = state.hook_count[ev];
	for (int i = 0; i < n; i++) {
		struct wren_hook_entry *h = state.hooks[ev][i];
		if (h == NULL || h->fn == NULL)
			continue;
		wrenEnsureSlots(state.vm, 2);
		wrenSetSlotHandle(state.vm, 0, state.hook_class);
		wrenSetSlotHandle(state.vm, 1, h->fn);
		(void)metrics_invoke(&h->metrics, state.dispatch0_handle);
		warn_if_c2_misused(
		    ev == WREN_HOOK_SHELL_CLOSE
		        ? "onShellClose" : "onDisconnect");
	}
}

void
wren_host_dispatch_shell_close(void)
{
	dispatch_zero_arg_event(WREN_HOOK_SHELL_CLOSE);
}

void
wren_host_dispatch_disconnect(void)
{
	dispatch_zero_arg_event(WREN_HOOK_DISCONNECT);
}
