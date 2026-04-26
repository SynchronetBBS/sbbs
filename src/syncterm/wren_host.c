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
    "  foreign static getText(sx, sy, ex, ey)\n"
    "  foreign static putText(sx, sy, ex, ey, list)\n"
    "  foreign static mousedrag()\n"
    "  foreign static getMouse\n"
    "  foreign static getClipText\n"
    "}\n"
    /* Repl.compile_ returns the bare ObjClosure for the compiled body;
     * Wren-level Repl.eval invokes it via .call().  Going through Wren
     * for the call (rather than wrenInterpret-from-C) keeps apiStack
     * valid across the eval — wrenInterpret is not foreign-method-safe,
     * but Wren-level dispatch is. */
    /* Repl is the primitive: compile and call.  Return shape uses
     * an Optional-style wrapper so the caller can distinguish "this
     * was a statement" from "this was an expression whose value is
     * null":
     *   - statement form        -> null
     *   - expression form       -> [value]   (one-element list, value
     *                                         may itself be null)
     * Runtime errors propagate normally; wrap the call in
     * Fiber.new{}.try() at the call site if you want to catch them.
     * Display formatting belongs at the call site too (see
     * console.wren's handleLine_). */
    "class Repl {\n"
    "  static eval(src) { eval(\"syncterm\", src) }\n"
    "  static eval(module, src) {\n"
    /* Compile errors get diverted into a private capture buffer for
     * the duration of the eval.  If the expression-mode compile fails
     * with "Expected expression." (the parser's way of saying "this
     * isn't an expression — try as a statement"), we drop the capture
     * and try statement form.  Other compile failures, runtime
     * traces, etc. ride through the capture and get committed to the
     * real log at the end so the user sees them. */
    "    captureStart_()\n"
    "    var fn = compile_(module, src, true, true)\n"
    "    var wasExpr = fn != null\n"
    "    if (!wasExpr && captureContains_(\"Expected expression\")) {\n"
    "      captureClear_()\n"
    "      fn = compile_(module, src, false, true)\n"
    "    }\n"
    "    captureCommit_()\n"
    "    if (fn == null) return null\n"
    "    var v = fn.call()\n"
    "    return wasExpr ? [v] : null\n"
    "  }\n"
    "  foreign static compile_(module, src, isExpression, printErrors)\n"
    "  foreign static printTrace_(fiber)\n"
    "  foreign static hasModule(name)\n"
    "  foreign static captureStart_()\n"
    "  foreign static captureContains_(needle)\n"
    "  foreign static captureClear_()\n"
    "  foreign static captureCommit_()\n"
    "}\n"
    "foreign class Cell {\n"
    "  construct new() {}\n"
    "  foreign ch\n"
    "  foreign ch=(s)\n"
    "  foreign chByte\n"
    "  foreign chByte=(n)\n"
    "  foreign font\n"
    "  foreign font=(n)\n"
    "  foreign legacyAttr\n"
    "  foreign legacyAttr=(n)\n"
    "  foreign bright\n"
    "  foreign bright=(b)\n"
    "  foreign blink\n"
    "  foreign blink=(b)\n"
    "  foreign fgPalette\n"
    "  foreign fgPalette=(n)\n"
    "  foreign fgRgb\n"
    "  foreign fgRgb=(n)\n"
    "  foreign bgPalette\n"
    "  foreign bgPalette=(n)\n"
    "  foreign bgRgb\n"
    "  foreign bgRgb=(n)\n"
    "  foreign hyperlinkId\n"
    "  foreign hyperlinkId=(n)\n"
    "}\n"
    "foreign class Cells {\n"
    "  foreign count\n"
    "  foreign [i]\n"
    "  foreign iterate(it)\n"
    "  foreign iteratorValue(it)\n"
    "}\n"
    "class Font {\n"
    "  static cp437English      { 0 }\n"
    "  static commodore64Upper  { 32 }\n"
    "  static commodore64Lower  { 33 }\n"
    "  static commodore128Upper { 34 }\n"
    "  static commodore128Lower { 35 }\n"
    "  static atari             { 36 }\n"
    "  static potNoodle         { 37 }\n"
    "  static mosOul            { 38 }\n"
    "  static microKnightPlus   { 39 }\n"
    "  static topazPlus         { 40 }\n"
    "  static microKnight       { 41 }\n"
    "  static topaz             { 42 }\n"
    "  static prestel           { 43 }\n"
    "  static atariSt           { 44 }\n"
    "  static ripterm           { 45 }\n"
    "  foreign static name(i)\n"
    "  foreign static count\n"
    "  foreign static available(i)\n"
    "}\n"
    "class Hyperlinks {\n"
    "  foreign static [id]\n"
    "  foreign static containsKey(id)\n"
    "  foreign static add(uri, idParam)\n"
    "  foreign static params(id)\n"
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
    "  foreign static password\n"
    "  foreign static syspass\n"
    "  foreign static music\n"
    "  foreign static rip\n"
    "  foreign static comment\n"
    "}\n"
    "class ConnType {\n"
    "  static unknown        { 0 }\n"
    "  static rlogin         { 1 }\n"
    "  static rloginReversed { 2 }\n"
    "  static telnet         { 3 }\n"
    "  static raw            { 4 }\n"
    "  static ssh            { 5 }\n"
    "  static sshNoAuth      { 6 }\n"
    "  static modem          { 7 }\n"
    "  static serial         { 8 }\n"
    "  static serialNoRts    { 9 }\n"
    "  static shell          { 10 }\n"
    "  static mbbsGhost      { 11 }\n"
    "  static telnets        { 12 }\n"
    "}\n"
    "class Emulation {\n"
    "  static ansiBbs   { 0 }\n"
    "  static petascii  { 1 }\n"
    "  static atascii   { 2 }\n"
    "  static prestel   { 3 }\n"
    "  static beeb      { 4 }\n"
    "  static atariVt52 { 5 }\n"
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

static struct wren_host_state state;
static bool      active;
static pthread_t owner_thread;

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
	uint64_t              total;        /* monotonic; survives clear */
};

/* The user-visible scrollback. */
static struct wren_log main_log;

/* Side log used during a Repl.eval to capture compile errors that may
 * or may not survive to the user-visible log.  When a captured set
 * contains "Expected expression." (the parser's signal that input
 * isn't an expression), Repl.eval drops the side log and retries as a
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
 * commit/clear.  Repl.eval uses this to preview compile errors from an
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
	return active ? &state : NULL;
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
			break;
		case WREN_ERROR_RUNTIME:
			log_append(WREN_LOG_RUNTIME_ERROR,
			    message ? message : "");
			break;
		case WREN_ERROR_STACK_TRACE:
			snprintf(buf, sizeof(buf), "%s:%d in %s",
			    module ? module : "?", line, message ? message : "");
			log_append(WREN_LOG_STACK_FRAME, buf);
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

/* --------------------------------------------------------------------
 * Hook + timer registration
 * -------------------------------------------------------------------- */

bool
wren_host_register_hook(WrenVM *vm, enum wren_hook_event ev, int fn_slot)
{
	if (!active || ev < 0 || ev >= WREN_HOOK_COUNT)
		return false;
	int n = state.hook_count[ev];
	if (n >= WREN_HOST_MAX_HOOKS_PER_EVENT) {
		fprintf(stderr, "[wren] hook limit reached for event %d\n", (int)ev);
		return false;
	}
	state.hooks[ev][n] = wrenGetSlotHandle(vm, fn_slot);
	state.hook_count[ev] = n + 1;
	return true;
}

bool
wren_host_register_timer(WrenVM *vm, int ms, int fn_slot)
{
	if (!active)
		return false;
	if (ms < 1)
		ms = 1;
	if (state.timer_count >= WREN_HOST_MAX_TIMERS) {
		fprintf(stderr, "[wren] timer limit reached\n");
		return false;
	}
	struct wren_timer_entry *t = &state.timers[state.timer_count++];
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

/* Returns true if `name` matches any embedded script's name.  When a
 * user script's basename matches an embedded one, the user version is
 * an override and gets loaded into the shared "syncterm" module so it
 * sees the same scope (foreign-class declarations + sibling embeds)
 * that the embedded version would have. */
static bool
is_embedded_script_name(const char *name)
{
	for (const struct embedded_script *es = EMBEDDED_SCRIPTS;
	    es->name != NULL; es++) {
		if (strcmp(es->name, name) == 0)
			return true;
	}
	return false;
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

	/* Override of an embedded script -> load into "syncterm".  Pure
	 * user script -> load into its own module (filename-derived). */
	const char *target =
	    is_embedded_script_name(modname) ? "syncterm" : modname;

	WrenInterpretResult r = wrenInterpret(state.vm, target, src);
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

	WrenConfiguration cfg;
	wrenInitConfiguration(&cfg);
	cfg.writeFn             = host_write_fn;
	cfg.errorFn             = host_error_fn;
	cfg.bindForeignMethodFn = host_bind_foreign_method;
	cfg.bindForeignClassFn  = host_bind_foreign_class;
	state.vm = wrenNewVM(&cfg);
	if (state.vm == NULL)
		return;

	state.call0_handle = wrenMakeCallHandle(state.vm, "call()");
	state.call1_handle = wrenMakeCallHandle(state.vm, "call(_)");

	owner_thread = pthread_self();
	active = true;

	/* Register the foreign-class declarations so user scripts can
	 * `import "syncterm" for ...`. */
	if (wrenInterpret(state.vm, "syncterm", SYNCTERM_MODULE_SRC) !=
	    WREN_RESULT_SUCCESS) {
		fprintf(stderr, "[wren] failed to load builtin syncterm module\n");
		wren_host_shutdown();
		return;
	}

	/* Capture handles for the foreign classes the C side allocates
	 * instances of (Cell from inside Conio.getText, Cells as the
	 * getText return value).  wrenSetSlotNewForeign needs a class
	 * handle in a slot, so we keep these alive for the VM's lifetime. */
	wrenEnsureSlots(state.vm, 1);
	wrenGetVariable(state.vm, "syncterm", "Cell", 0);
	state.cell_class = wrenGetSlotHandle(state.vm, 0);
	wrenGetVariable(state.vm, "syncterm", "Cells", 0);
	state.cells_class = wrenGetSlotHandle(state.vm, 0);

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
		/* Embedded scripts share the "syncterm" module with the
		 * foreign-class declarations.  Inside them, every binding
		 * (Conio, Cell, Repl, etc.) and every sibling embed's
		 * top-level definitions are directly visible — no imports
		 * needed.  Override scripts are loaded into the same module
		 * by load_one_script. */
		if (wrenInterpret(state.vm, "syncterm", es->source) !=
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
	if (!active)
		return;

	for (int e = 0; e < WREN_HOOK_COUNT; e++) {
		for (int i = 0; i < state.hook_count[e]; i++)
			if (state.hooks[e][i] != NULL)
				wrenReleaseHandle(state.vm, state.hooks[e][i]);
		state.hook_count[e] = 0;
	}
	for (int i = 0; i < state.timer_count; i++)
		if (state.timers[i].callable != NULL)
			wrenReleaseHandle(state.vm, state.timers[i].callable);
	state.timer_count = 0;

	if (state.call0_handle != NULL)
		wrenReleaseHandle(state.vm, state.call0_handle);
	if (state.call1_handle != NULL)
		wrenReleaseHandle(state.vm, state.call1_handle);
	if (state.cell_class != NULL)
		wrenReleaseHandle(state.vm, state.cell_class);
	if (state.cells_class != NULL)
		wrenReleaseHandle(state.vm, state.cells_class);

	/* Release log-buffer cached handles before freeing the VM (they
	 * reference Wren-heap values). */
	wren_log_clear();

	wrenFreeVM(state.vm);
	state.vm = NULL;
	active = false;
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
	int n = state.hook_count[WREN_HOOK_KEY];
	for (int i = 0; i < n; i++) {
		wrenEnsureSlots(state.vm, 2);
		wrenSetSlotHandle(state.vm, 0, state.hooks[WREN_HOOK_KEY][i]);
		wrenSetSlotDouble(state.vm, 1, (double)key);
		if (wrenCall(state.vm, state.call1_handle) !=
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
	if (!active || !on_owner_thread())
		return false;
	int n = state.hook_count[WREN_HOOK_INPUT];
	for (int i = 0; i < n; i++) {
		wrenEnsureSlots(state.vm, 2);
		wrenSetSlotHandle(state.vm, 0, state.hooks[WREN_HOOK_INPUT][i]);
		wrenSetSlotDouble(state.vm, 1, (double)byte);
		if (wrenCall(state.vm, state.call1_handle) !=
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
	if (!active || !on_owner_thread() || buf == NULL)
		return false;
	int n = state.hook_count[WREN_HOOK_OUTPUT];
	for (int i = 0; i < n; i++) {
		wrenEnsureSlots(state.vm, 2);
		wrenSetSlotHandle(state.vm, 0, state.hooks[WREN_HOOK_OUTPUT][i]);
		wrenSetSlotBytes(state.vm, 1, (const char *)buf, len);
		if (wrenCall(state.vm, state.call1_handle) !=
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
	if (!active || !on_owner_thread() || ev == NULL)
		return false;
	int n = state.hook_count[WREN_HOOK_MOUSE];
	if (n == 0)
		return false;
	/* Build a 7-element list: [event, bstate, kbmodifiers, startx,
	 * starty, endx, endy].  Scripts index by position. */
	for (int i = 0; i < n; i++) {
		wrenEnsureSlots(state.vm, 3);
		wrenSetSlotHandle(state.vm, 0, state.hooks[WREN_HOOK_MOUSE][i]);
		wrenSetSlotNewList(state.vm, 1);
		const double fields[] = {
			(double)ev->event, (double)ev->bstate,
			(double)ev->kbmodifiers, (double)ev->startx,
			(double)ev->starty, (double)ev->endx,
			(double)ev->endy
		};
		for (size_t f = 0; f < sizeof(fields) / sizeof(fields[0]); f++) {
			wrenSetSlotDouble(state.vm, 2, fields[f]);
			wrenInsertInList(state.vm, 1, -1, 2);
		}
		if (wrenCall(state.vm, state.call1_handle) !=
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
	if (!active || !on_owner_thread() || out == NULL || outsz == 0)
		return false;
	int n = state.hook_count[WREN_HOOK_STATUS];
	for (int i = 0; i < n; i++) {
		wrenEnsureSlots(state.vm, 2);
		wrenSetSlotHandle(state.vm, 0, state.hooks[WREN_HOOK_STATUS][i]);
		wrenSetSlotString(state.vm, 1, def != NULL ? def : "");
		if (wrenCall(state.vm, state.call1_handle) !=
		    WREN_RESULT_SUCCESS)
			continue;
		if (wrenGetSlotType(state.vm, 0) != WREN_TYPE_STRING)
			continue;
		const char *s = wrenGetSlotString(state.vm, 0);
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
	if (!active || !on_owner_thread())
		return;
	long double now = xp_timer();
	for (int i = 0; i < state.timer_count; i++) {
		struct wren_timer_entry *t = &state.timers[i];
		if (t->callable == NULL || now < t->next_fire_s)
			continue;
		wrenEnsureSlots(state.vm, 1);
		wrenSetSlotHandle(state.vm, 0, t->callable);
		(void)wrenCall(state.vm, state.call0_handle);
		/* Advance by interval; if the loop stalled long enough that
		 * we're more than one interval behind, jump forward to "now"
		 * rather than firing repeatedly trying to catch up. */
		t->next_fire_s += (long double)t->interval_ms / 1000.0L;
		if (t->next_fire_s < now)
			t->next_fire_s = now + (long double)t->interval_ms / 1000.0L;
	}
}
