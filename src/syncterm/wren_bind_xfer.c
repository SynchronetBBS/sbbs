/* wren_bind_xfer.c — transfer-window mailbox + foreigns.
 *
 * Single-transfer-at-a-time pattern.  Wren's TransferApp.run() is the
 * main loop while a transfer is active and replaces doterm() for the
 * duration.  A worker thread (spawned either via the foreign
 * Transfer.beginSession("fake") for the demo, or directly from C
 * through wren_run_transfer for real protocols) produces events into
 * a thread-safe mailbox; TransferApp's tick body drains it and
 * renders.
 *
 * Stage 3 wires zmodem-recv only; ymodem / xmodem / CET land later. */

#include "wren_bind_xfer.h"

/* MSVC defines __STDC_NO_ATOMICS__ defensively, which would make
 * <stdatomic.h> expand to nothing.  Undefine it first — same dance
 * conn.h already uses. */
#ifdef _MSC_VER
#undef __STDC_NO_ATOMICS__
#endif
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "eventwrap.h"      /* xpevent_t */
#include "genwrap.h"        /* SLEEP() macro */
#include "threadwrap.h"     /* pthread_mutex_t (Win32-portable), _beginthread */
#include "xp_syslog.h"      /* LOG_INFO etc. — portable syslog priorities */
#include "wren_bind_internal.h"
#include "wren_host.h"
#include "wren_host_internal.h"

/* ----- log mailbox ------------------------------------------------ */

#define LOG_RING_CAP 256
#define LOG_MSG_MAX  480

struct log_event {
	int  level;
	char msg[LOG_MSG_MAX];
};

static struct log_event  log_ring[LOG_RING_CAP];
static size_t            log_head;    /* oldest; advanced by drainLog */
static size_t            log_count;   /* entries currently buffered  */
static _Atomic unsigned  log_dropped; /* push attempts past full ring */
static pthread_mutex_t   log_mtx;

/* ----- tick state (single, borrowed) ------------------------------ */

static struct xfer_tick_state tick;
static pthread_mutex_t        tick_mtx;
static _Atomic bool           tick_dirty;

/* ----- session lifecycle ------------------------------------------ */

static _Atomic bool abort_requested;
static _Atomic bool worker_done;
static _Atomic bool worker_success;
static _Atomic bool session_active;
static xpevent_t    worker_done_evt;  /* set by worker wrapper at exit */

/* ----- dialog request (worker → main) ----------------------------- */

static struct {
	int             kind;         /* XFER_DLG_NONE / DUPLICATE / ... */
	char            filename[256];
	int             response;
	char            new_name[256];
	bool            ready;        /* true = main posted response */
	pthread_mutex_t mtx;
	xpevent_t       evt;          /* set when ready flips true */
} dlg = {
	.kind = XFER_DLG_NONE,
};

/* Storage for the worker callback so the _beginthread thunk can find
 * it.  Single-transfer-at-a-time, so a single slot is enough. */
static void (*worker_fn_g)(void *);
static void  *worker_arg_g;

/* Initialized once via pthread_once — covers all the runtime-init
 * mutexes + the two manual-reset events.  Static initializers don't
 * work on Win32 where pthread_mutex_t is a CRITICAL_SECTION. */
static pthread_once_t init_once_ctl = PTHREAD_ONCE_INIT;

static void
xfer_init_once(void)
{
	pthread_mutex_init_np(&log_mtx,  /* recursive */ false);
	pthread_mutex_init_np(&tick_mtx, /* recursive */ false);
	pthread_mutex_init_np(&dlg.mtx,  /* recursive */ false);
	dlg.evt         = CreateEvent(NULL, /* manual */ TRUE,
	                              /* initial */ FALSE, NULL);
	worker_done_evt = CreateEvent(NULL, /* manual */ TRUE,
	                              /* initial */ FALSE, NULL);
}

static void
ensure_init(void)
{
	pthread_once(&init_once_ctl, xfer_init_once);
}

/* ----- helpers ---------------------------------------------------- */

static void clear_dialog_locked(void);

static void
push_log_event(int level, const char *str)
{
	pthread_mutex_lock(&log_mtx);
	if (log_count >= LOG_RING_CAP) {
		atomic_fetch_add(&log_dropped, 1);
		pthread_mutex_unlock(&log_mtx);
		return;
	}
	size_t idx = (log_head + log_count) % LOG_RING_CAP;
	log_ring[idx].level = level;
	strncpy(log_ring[idx].msg, str, LOG_MSG_MAX - 1);
	log_ring[idx].msg[LOG_MSG_MAX - 1] = '\0';
	log_count++;
	pthread_mutex_unlock(&log_mtx);
}

static void
reset_state_locked(void)
{
	/* Caller holds neither mutex; called only when worker is
	 * guaranteed not to be running (before begin / after end). */
	pthread_mutex_lock(&log_mtx);
	log_head  = 0;
	log_count = 0;
	pthread_mutex_unlock(&log_mtx);
	atomic_store(&log_dropped, 0);

	pthread_mutex_lock(&tick_mtx);
	memset(&tick, 0, sizeof(tick));
	pthread_mutex_unlock(&tick_mtx);
	atomic_store(&tick_dirty, false);

	atomic_store(&abort_requested, false);
	atomic_store(&worker_done,     false);
	atomic_store(&worker_success,  false);

	clear_dialog_locked();
}

/* ----- public worker-side API ------------------------------------ */

void
xfer_log_push(int level, const char *str)
{
	if (str == NULL)
		return;
	/* Split on embedded newlines so each visible line becomes its
	 * own LogView entry — the legacy cputs path interpreted \n as
	 * a line break, and zmodem etc. include them in lprintf strings
	 * (e.g. "mode (0x23):\nFull-duplex, ...").  Trailing \r before
	 * each \n is stripped to keep CR-LF pairs from ending up as a
	 * stray glyph at the line tail. */
	const char *line  = str;
	while (*line != '\0') {
		const char *eol = line;
		while (*eol != '\0' && *eol != '\n')
			eol++;
		size_t len = (size_t)(eol - line);
		if (len > 0 && line[len - 1] == '\r')
			len--;
		if (len >= LOG_MSG_MAX)
			len = LOG_MSG_MAX - 1;
		char tmp[LOG_MSG_MAX];
		memcpy(tmp, line, len);
		tmp[len] = '\0';
		push_log_event(level, tmp);
		if (*eol == '\0')
			break;
		line = eol + 1;       /* skip the \n */
	}
}

void
xfer_tick_lock(void)
{
	pthread_mutex_lock(&tick_mtx);
}

struct xfer_tick_state *
xfer_tick_get(void)
{
	return &tick;
}

void
xfer_tick_unlock(void)
{
	pthread_mutex_unlock(&tick_mtx);
}

void
xfer_tick_dirty(void)
{
	atomic_store(&tick_dirty, true);
}

bool
xfer_check_abort_atomic(void)
{
	return atomic_load(&abort_requested);
}

bool
xfer_session_active(void)
{
	return atomic_load(&session_active);
}

void
xfer_set_done(bool success)
{
	atomic_store(&worker_success, success);
	atomic_store(&worker_done,    true);
}

/* Block worker on dlg.evt until main thread flips dlg.ready.  Wakes
 * every 100 ms to re-check the abort flag — the manual-reset event
 * remembers any SetEvent that fired after we cleared it, so racing
 * with dialogRespond is safe. */
static void
wait_for_dialog_response(int abort_response)
{
	while (!dlg.ready) {
		if (atomic_load(&abort_requested)) {
			dlg.response = abort_response;
			break;
		}
		pthread_mutex_unlock(&dlg.mtx);
		WaitForEvent(dlg.evt, 100);
		pthread_mutex_lock(&dlg.mtx);
	}
}

bool
xfer_request_filename(const char *prompt, char *out, size_t out_sz)
{
	if (prompt == NULL)
		prompt = "Filename";
	ensure_init();

	pthread_mutex_lock(&dlg.mtx);
	ResetEvent(dlg.evt);
	dlg.kind = XFER_DLG_FILENAME_PROMPT;
	/* The `filename` field is overloaded as the dialog's prompt
	 * text — same channel as XFER_DLG_DUPLICATE, the Wren side
	 * just renders it as the Prompt's title for kind == 2. */
	strncpy(dlg.filename, prompt, sizeof(dlg.filename) - 1);
	dlg.filename[sizeof(dlg.filename) - 1] = '\0';
	dlg.new_name[0] = '\0';
	dlg.response    = XFER_DLG_PROMPT_CANCEL;
	dlg.ready       = false;

	wait_for_dialog_response(XFER_DLG_PROMPT_CANCEL);

	bool ok = (dlg.response == XFER_DLG_PROMPT_OK);
	if (ok && out != NULL && out_sz > 0) {
		strncpy(out, dlg.new_name, out_sz - 1);
		out[out_sz - 1] = '\0';
	}
	dlg.kind = XFER_DLG_NONE;
	pthread_mutex_unlock(&dlg.mtx);
	return ok;
}

int
xfer_request_duplicate(const char *filename,
                       char *new_name, size_t new_name_sz)
{
	if (filename == NULL)
		filename = "";
	ensure_init();

	pthread_mutex_lock(&dlg.mtx);
	ResetEvent(dlg.evt);
	dlg.kind = XFER_DLG_DUPLICATE;
	strncpy(dlg.filename, filename, sizeof(dlg.filename) - 1);
	dlg.filename[sizeof(dlg.filename) - 1] = '\0';
	dlg.new_name[0] = '\0';
	dlg.response    = XFER_DLG_SKIP;     /* safe default if main bails */
	dlg.ready       = false;

	wait_for_dialog_response(XFER_DLG_SKIP);

	int response = dlg.response;
	if (response == XFER_DLG_RENAME && new_name != NULL && new_name_sz > 0) {
		strncpy(new_name, dlg.new_name, new_name_sz - 1);
		new_name[new_name_sz - 1] = '\0';
	}
	dlg.kind = XFER_DLG_NONE;
	pthread_mutex_unlock(&dlg.mtx);
	return response;
}

static void
clear_dialog_locked(void)
{
	/* Wakes any worker still parked on the dialog event so it
	 * unwinds with the SKIP default. */
	pthread_mutex_lock(&dlg.mtx);
	dlg.kind     = XFER_DLG_NONE;
	dlg.response = XFER_DLG_SKIP;
	dlg.ready    = true;
	SetEvent(dlg.evt);
	pthread_mutex_unlock(&dlg.mtx);
}

/* ----- stub worker (Stage 2 demo) -------------------------------- */

static void
fake_worker_main(void *arg)
{
	(void)arg;

	const int64_t TOTAL_BYTES   = 100L * 1024;       /* 100 KiB */
	const int64_t BYTES_PER_TICK = 4L * 1024;        /* 4 KiB / tick */
	int64_t       cur_bytes     = 0;
	time_t        start         = time(NULL);

	xfer_log_push(LOG_NOTICE, "Receiving fake.bin (102400 bytes)");

	while (cur_bytes < TOTAL_BYTES) {
		if (xfer_check_abort_atomic()) {
			xfer_log_push(LOG_WARNING, "Transfer aborted by user");
			break;
		}

		SLEEP(100);
		cur_bytes += BYTES_PER_TICK;
		if (cur_bytes > TOTAL_BYTES)
			cur_bytes = TOTAL_BYTES;
		time_t now     = time(NULL);
		time_t elapsed = now - start;

		uint32_t cps = (elapsed > 0)
		    ? (uint32_t)(cur_bytes / elapsed)
		    : (uint32_t)cur_bytes;

		xfer_tick_lock();
		struct xfer_tick_state *t = xfer_tick_get();
		snprintf(t->line1, sizeof(t->line1),
		    "File (1 of 1): fake.bin");
		snprintf(t->line2, sizeof(t->line2),
		    "Byte: %lld of %lld (%lld KB)",
		    (long long)cur_bytes,
		    (long long)TOTAL_BYTES,
		    (long long)(TOTAL_BYTES / 1024));
		snprintf(t->line3, sizeof(t->line3),
		    "Time: %ld:%02ld  %u cps",
		    (long)(elapsed / 60), (long)(elapsed % 60), cps);
		t->line4[0]    = '\0';
		t->bytes_cur   = cur_bytes;
		t->bytes_total = TOTAL_BYTES;
		xfer_tick_unlock();
		xfer_tick_dirty();

		/* Log a checkpoint every 20 KiB. */
		if ((cur_bytes % (20 * 1024)) == 0) {
			char msg[96];
			snprintf(msg, sizeof(msg),
			    "Received %lld of %lld bytes",
			    (long long)cur_bytes, (long long)TOTAL_BYTES);
			xfer_log_push(LOG_INFO, msg);
		}
	}

	bool ok = (cur_bytes >= TOTAL_BYTES);
	if (ok)
		xfer_log_push(LOG_NOTICE, "Transfer complete");
	xfer_set_done(ok);
}

/* ----- session start / stop --------------------------------------- */

static void
worker_thunk(void *arg)
{
	(void)arg;
	void (*fn)(void *) = worker_fn_g;
	void  *farg        = worker_arg_g;
	if (fn != NULL)
		fn(farg);
	/* Always set worker_done — the protocol body should have done so
	 * via xfer_set_done, but this is the last-line guarantee for the
	 * main-thread join. */
	atomic_store(&worker_done, true);
	SetEvent(worker_done_evt);
}

static int
spawn_worker(void (*fn)(void *), void *arg)
{
	ensure_init();
	if (atomic_load(&session_active))
		return -1;
	reset_state_locked();
	ResetEvent(worker_done_evt);
	worker_fn_g  = fn;
	worker_arg_g = arg;
	atomic_store(&session_active, true);
	if (_beginthread(worker_thunk, 0, NULL) == (ulong)-1L) {
		atomic_store(&session_active, false);
		worker_fn_g  = NULL;
		worker_arg_g = NULL;
		return -1;
	}
	return 0;
}

static void
join_and_clear(void)
{
	if (!atomic_load(&session_active))
		return;
	atomic_store(&abort_requested, true);
	/* Wait for the worker thunk's SetEvent.  No bounded timeout —
	 * the worker is responsible for honoring the abort flag and
	 * exiting; if it doesn't, the caller (TransferApp.run loop) has
	 * already returned, so we can afford to wait. */
	WaitForEvent(worker_done_evt, INFINITE);
	worker_fn_g  = NULL;
	worker_arg_g = NULL;
	atomic_store(&session_active, false);
}

/* ----- foreigns: lifecycle ---------------------------------------- */

void
fn_Transfer_beginSession(WrenVM *vm)
{
	ensure_init();
	if (atomic_load(&session_active)) {
		wren_throw(vm, "Transfer.beginSession: already active");
		return;
	}
	if (wrenGetSlotType(vm, 1) != WREN_TYPE_STRING) {
		wren_throw(vm, "Transfer.beginSession: kind must be a string");
		return;
	}
	const char *kind = wrenGetSlotString(vm, 1);
	if (strcmp(kind, "fake") != 0) {
		wren_throw(vm, "Transfer.beginSession: unknown kind "
		    "(only \"fake\" is wired in Stage 2)");
		return;
	}
	if (spawn_worker(fake_worker_main, NULL) != 0) {
		wren_throw(vm, "Transfer.beginSession: spawn failed");
		return;
	}
	wrenSetSlotBool(vm, 0, true);
}

void
fn_Transfer_endSession(WrenVM *vm)
{
	(void)vm;
	join_and_clear();
	wrenSetSlotNull(vm, 0);
}

/* ----- foreigns: log channel -------------------------------------- */

void
fn_Transfer_drainLog(WrenVM *vm)
{
	ensure_init();
	wrenEnsureSlots(vm, 4);
	wrenSetSlotNewList(vm, 0);

	pthread_mutex_lock(&log_mtx);
	for (size_t i = 0; i < log_count; i++) {
		size_t            ri = (log_head + i) % LOG_RING_CAP;
		struct log_event *e  = &log_ring[ri];
		/* Each entry is a 2-element list: [level, message] — same
		 * shape as LogView.append(level, text) consumes. */
		wrenSetSlotNewList(vm, 1);
		wrenSetSlotDouble(vm, 2, (double)e->level);
		wrenInsertInList(vm, 1, -1, 2);
		wrenSetSlotString(vm, 2, e->msg);
		wrenInsertInList(vm, 1, -1, 2);
		wrenInsertInList(vm, 0, -1, 1);
	}
	log_head  = 0;
	log_count = 0;
	pthread_mutex_unlock(&log_mtx);

	unsigned dropped = atomic_exchange(&log_dropped, 0);
	if (dropped > 0) {
		char buf[64];
		snprintf(buf, sizeof(buf), "[%u log lines suppressed]", dropped);
		wrenSetSlotNewList(vm, 1);
		/* LOG_NOTICE so the suppression marker pops yellow without
		 * being mistaken for a real error. */
		wrenSetSlotDouble(vm, 2, (double)LOG_NOTICE);
		wrenInsertInList(vm, 1, -1, 2);
		wrenSetSlotString(vm, 2, buf);
		wrenInsertInList(vm, 1, -1, 2);
		wrenInsertInList(vm, 0, -1, 1);
	}
}

/* ----- foreigns: tick channel ------------------------------------- */

void
fn_Transfer_tickDirty(WrenVM *vm)
{
	bool d = atomic_exchange(&tick_dirty, false);
	wrenSetSlotBool(vm, 0, d);
}

/* Snapshot returns a fixed-shape list — a Wren-side TickStateView
 * wraps it for ergonomic field access.  Layout:
 *   0 line1      — String  (filename or "File (N of M): ...")
 *   1 line2      — String  (bytes / block descriptor)
 *   2 line3      — String  (time / cps line)
 *   3 line4      — String  (CET "Remain"; ignored when bytesTotal > 0)
 *   4 bytesCur   — Num
 *   5 bytesTotal — Num | null   (null suppresses bar + auto percent) */
void
fn_Transfer_snapshot(WrenVM *vm)
{
	ensure_init();
	pthread_mutex_lock(&tick_mtx);
	struct xfer_tick_state s = tick;
	pthread_mutex_unlock(&tick_mtx);

	wrenEnsureSlots(vm, 2);
	wrenSetSlotNewList(vm, 0);

	wrenSetSlotString(vm, 1, s.line1);
	wrenInsertInList(vm, 0, -1, 1);
	wrenSetSlotString(vm, 1, s.line2);
	wrenInsertInList(vm, 0, -1, 1);
	wrenSetSlotString(vm, 1, s.line3);
	wrenInsertInList(vm, 0, -1, 1);
	wrenSetSlotString(vm, 1, s.line4);
	wrenInsertInList(vm, 0, -1, 1);
	wrenSetSlotDouble(vm, 1, (double)s.bytes_cur);
	wrenInsertInList(vm, 0, -1, 1);
	if (s.bytes_total > 0) {
		wrenSetSlotDouble(vm, 1, (double)s.bytes_total);
	} else {
		wrenSetSlotNull(vm, 1);
	}
	wrenInsertInList(vm, 0, -1, 1);
}

/* ----- foreigns: completion + abort ------------------------------- */

void
fn_Transfer_done(WrenVM *vm)
{
	wrenSetSlotBool(vm, 0, atomic_load(&worker_done));
}

void
fn_Transfer_success(WrenVM *vm)
{
	wrenSetSlotBool(vm, 0, atomic_load(&worker_success));
}

void
fn_Transfer_requestAbort(WrenVM *vm)
{
	bool expected = false;
	bool latched  = atomic_compare_exchange_strong(&abort_requested,
	    &expected, true);
	wrenSetSlotBool(vm, 0, latched);
}

void
fn_Transfer_aborted(WrenVM *vm)
{
	wrenSetSlotBool(vm, 0, atomic_load(&abort_requested));
}

/* ----- foreigns: dialog channel ----------------------------------- */

void
fn_Transfer_dialogPending(WrenVM *vm)
{
	ensure_init();
	pthread_mutex_lock(&dlg.mtx);
	int k = dlg.ready ? XFER_DLG_NONE : dlg.kind;
	pthread_mutex_unlock(&dlg.mtx);
	wrenSetSlotDouble(vm, 0, (double)k);
}

void
fn_Transfer_dialogFilename(WrenVM *vm)
{
	ensure_init();
	pthread_mutex_lock(&dlg.mtx);
	char buf[256];
	strncpy(buf, dlg.filename, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';
	pthread_mutex_unlock(&dlg.mtx);
	wrenSetSlotString(vm, 0, buf);
}

void
fn_Transfer_dialogRespond(WrenVM *vm)
{
	ensure_init();
	int         resp     = (int)wrenGetSlotDouble(vm, 1);
	const char *new_name = (wrenGetSlotType(vm, 2) == WREN_TYPE_STRING)
	    ? wrenGetSlotString(vm, 2)
	    : "";

	pthread_mutex_lock(&dlg.mtx);
	dlg.response = resp;
	strncpy(dlg.new_name, new_name, sizeof(dlg.new_name) - 1);
	dlg.new_name[sizeof(dlg.new_name) - 1] = '\0';
	dlg.ready = true;
	SetEvent(dlg.evt);
	pthread_mutex_unlock(&dlg.mtx);
	wrenSetSlotNull(vm, 0);
}

/* ----- C-side top-level entry ------------------------------------ */

/* Build a TransferApp Wren instance and call its run() method.  The
 * worker thread has already been spawned by spawn_worker(); this just
 * stands up the UI loop and blocks until run() returns.  Mirrors the
 * Conn.scrollback() wiring pattern for invoking a Wren App from C. */
static void
invoke_transfer_app(const char *label)
{
	struct wren_host_state *st = wren_host_state();
	if (st == NULL || st->vm == NULL)
		return;
	WrenVM *vm = st->vm;

	/* TransferApp lives in scripts/auto/connected/transfer_app.wren —
	 * auto-loaded at host_init so wrenGetVariable can fetch it
	 * without a prior wrenInterpret (which would reset apiStack and
	 * crash the next slot access — wren_vm.c:1537). */
	wrenEnsureSlots(vm, 2);
	wrenGetVariable(vm, "transfer_app", "TransferApp", 0);
	WrenHandle *cls = wrenGetSlotHandle(vm, 0);

	/* Construct: TransferApp.new(label). */
	wrenSetSlotHandle(vm, 0, cls);
	wrenSetSlotString(vm, 1, label == NULL ? "Transfer" : label);
	WrenHandle *ctor = wrenMakeCallHandle(vm, "new(_)");
	wrenCall(vm, ctor);
	WrenHandle *inst = wrenGetSlotHandle(vm, 0);

	/* Invoke .run(). */
	wrenSetSlotHandle(vm, 0, inst);
	WrenHandle *run = wrenMakeCallHandle(vm, "run()");
	wrenCall(vm, run);

	wrenReleaseHandle(vm, run);
	wrenReleaseHandle(vm, ctor);
	wrenReleaseHandle(vm, inst);
	wrenReleaseHandle(vm, cls);
}

void
wren_run_transfer(const char *label,
                  void (*worker_fn)(void *), void *arg)
{
	if (worker_fn == NULL)
		return;
	if (spawn_worker(worker_fn, arg) != 0)
		return;
	invoke_transfer_app(label);
	join_and_clear();
}

/* ----- shutdown --------------------------------------------------- */

void
wren_xfer_shutdown(void)
{
	join_and_clear();
}
