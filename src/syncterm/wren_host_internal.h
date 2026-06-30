#ifndef WREN_HOST_INTERNAL_H
#define WREN_HOST_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "threadwrap.h"   /* pthread_mutex_t */
#include "wren.h"

struct mouse_event;
struct wren_file;
struct wren_directory;

/* --------------------------------------------------------------------
 * Result queue
 *
 * Generic "completion destined for a parked fiber" framework.  Any
 * async op that delivers to a fiber (Input.nextEvent, SFTP.realpath,
 * etc.) captures the fiber handle and arranges for a result to be
 * pushed here.  The owner-thread drainer pops in arrival order,
 * builds the Wren-side foreign for each result, calls the fiber,
 * releases the handle, frees the data.
 *
 * Workers can push from any thread (the queue is mutex-protected);
 * delivery happens entirely on the owner thread.  Results carrying a
 * dead fiber (Fiber.isDone) are skipped — the handle is released and
 * the data freed without invoking the fiber.
 * -------------------------------------------------------------------- */

/* Build the Wren-side result instance from `data` into `slot` (caller
 * has already ensured ≥2 slots and put the fiber receiver in slot 0).
 * `slot + 1` may be used as scratch for class loading; the deliver
 * function must leave the result foreign in `slot`. */
typedef void (*wren_result_deliver_fn)(WrenVM *vm, int slot, void *data);

/* Free `data` after delivery (or skip).  May be NULL if `data` itself
 * was statically allocated or owned elsewhere. */
typedef void (*wren_result_free_fn)(void *data);

struct wren_result {
	WrenHandle             *fiber;    /* owned; released after delivery */
	void                   *data;     /* owned; freed via free_fn */
	wren_result_deliver_fn  deliver;
	wren_result_free_fn     free_fn;
	struct wren_result     *next;
};

/* Push a completion onto the queue.  Takes ownership of `fiber` and
 * `data`.  Safe to call from any thread.  No-op if the host is shut
 * down (releases `fiber` and frees `data` immediately in that case so
 * callers don't have to special-case shutdown races). */
void wren_result_push(WrenHandle *fiber, void *data,
                      wren_result_deliver_fn deliver,
                      wren_result_free_fn free_fn);

/* Maximum simultaneous registrations per hook event and per timer slot.
 * 256 hooks per event is generous headroom for an entirely Wren-driven
 * UI (every modal, picker, status indicator, key handler is a hook);
 * the cost is one pointer per slot, so the whole `hooks` table is
 * WREN_HOOK_COUNT * 256 * sizeof(void *) ≈ 16 KiB.  Exceeding the cap
 * drops the registration with a logged error. */
#define WREN_HOST_MAX_HOOKS_PER_EVENT 256
#define WREN_HOST_MAX_TIMERS          16
#define WREN_HOST_MAX_PENDING_TIMERS  32

/* One-shot Timer.trigger registration: a fiber waiting to be resumed
 * at `due_ms` (xp_fast_timer64_ms() value).  Stored in the host's
 * pending_timers array and swept once per doterm() iteration. */
struct wren_pending_timer {
	WrenHandle *fiber;
	int64_t due_ms;
};

enum wren_hook_event {
	WREN_HOOK_KEY,
	WREN_HOOK_INPUT,
	WREN_HOOK_PHYSICAL_KEY,
	WREN_HOOK_MOUSE,
	WREN_HOOK_SHELL_CLOSE,      /* shell channel died, session still up */
	WREN_HOOK_DISCONNECT,       /* main loop exited, session tearing down */
	WREN_HOOK_COUNT,            /* count of dispatch-array events */
	WREN_HOOK_TIMER             /* lives in state.timers, not state.hooks */
};

/* Per-entry metrics — populated by every successful hook firing.
 * `min`/`max` are seeded on the first call, so a never-fired entry
 * reads back as all zeros. */
struct hook_metrics {
	uint64_t    call_count;
	long double total_runtime_s;
	long double min_runtime_s;
	long double max_runtime_s;
};

/* Per-entry record.  Hooks (KEY/INPUT/OUTPUT/MOUSE/STATUS) and timers
 * share one struct type; `ev` discriminates.  Allocated on the heap
 * at registration; freed when both `unregistered` (drained from the
 * dispatch array by wren_host_compact) and `gced` (HookHandle's
 * finalizer ran) are true.  Pointers from HookHandle are stable for
 * the entry's lifetime — the dispatch arrays hold pointers, not
 * structs, so compaction shifts pointers without moving the data
 * the handles reference. */
struct PikeVM;	/* opaque; full type in re1/regexp.h */

struct wren_hook_entry {
	enum wren_hook_event ev;
	WrenHandle          *fn;          /* NULL = tombstone (remove'd) */
	int                  filter;
	bool                 filtered;
	struct hook_metrics  metrics;

	/* Timer-only fields (ev == WREN_HOOK_TIMER). */
	uint32_t             interval_ms;
	int64_t              next_fire_ms;

	/* Streaming-regex fields (ev == WREN_HOOK_INPUT, regex hooks). */
	struct Prog         *regex_prog;
	struct PikeVM       *regex_vm;
	char                *regex_buf;
	size_t               regex_buf_len;
	size_t               regex_buf_cap;
	size_t               regex_pos;
	int                  regex_nsubp;
	/* When true, inbound bytes are pre-filtered through ansi_filter
	 * (KEEP_TEXT) so colour codes / cursor moves never reach the
	 * regex VM.  `regex_ansi_state` is the per-hook filter state.
	 * Both onMatch and onMatchClean are passthrough-only; the flag
	 * only controls the pre-filter, not the dispatch path. */
	bool                 regex_clean;
	int                  regex_ansi_state;   /* enum ansi_state, kept as int to avoid header cycle */

	/* Lifetime bookkeeping.  Free when both flags are true. */
	bool                 unregistered;  /* compact dropped from arrays */
	bool                 gced;          /* HookHandle finalizer ran */

	/* Single linked list used as the cleanup queue.  Pushed by
	 * remove(); drained at the top of every doterm() outer-loop
	 * iteration via wren_host_compact(). */
	struct wren_hook_entry *next_cleanup;
};

struct wren_host_state {
	WrenVM      *vm;
	/* Cached method handles: call() with 0 and 1 arg.  Wren's Fn class
	 * uses signature "call()" / "call(_)". */
	WrenHandle  *call0_handle;
	WrenHandle  *call1_handle;
	/* Hook.dispatch_(fn) / Hook.dispatch_(fn, arg) — every hook fire
	 * goes through these so a handler that yields directly raises a
	 * clear error instead of stranding the dispatcher.  See
	 * Hook.dispatch_ in scripts/syncterm.wren for the contract.
	 * Populated lazily after the syncterm module is loaded; the
	 * receiver `hook_class` is the Hook class object itself. */
	WrenHandle  *hook_class;
	WrenHandle  *dispatch0_handle;
	WrenHandle  *dispatch1_handle;
	WrenHandle  *host_popup_class;
	WrenHandle  *host_alert_handle;

	/* Cached class handles for foreign classes that the C side
	 * allocates instances of (via wrenSetSlotNewForeign).  Filled
	 * lazily on first allocation; released at shutdown. */
	WrenHandle  *cell_class;
	WrenHandle  *surface_class;
	WrenHandle  *key_event_class;
	WrenHandle  *physical_key_event_class;
	WrenHandle  *mouse_event_class;
	WrenHandle  *hook_handle_class;
	WrenHandle  *claim_handle_class;
	WrenHandle  *sftp_entry_class;
	WrenHandle  *sftp_stat_class;
	WrenHandle  *sftp_handle_class;
	WrenHandle  *sftp_error_class;
	WrenHandle  *file_error_class;
	WrenHandle  *won_error_class;
	WrenHandle  *conn_error_class;
	WrenHandle  *timer_elapsed_class;
	WrenHandle  *scrollback_class;

	/* Saved cterm scrollback ring counters for Scrollback.pushScreen()
	 * / popScreen() bookkeeping.  pushScreen captures the live values
	 * before mutating the ring; popScreen restores them.  Single-shot
	 * (no nesting); the bool tracks whether a save is live. */
	bool        scrollback_save_valid;
	int         scrollback_save_start;
	int         scrollback_save_pos;
	int         scrollback_save_filled;

	/* One-shot timer registrations from Timer.trigger.  Bounded ring
	 * of (fiber, due_ms) entries; swept once per doterm() iteration. */
	struct wren_pending_timer pending_timers[WREN_HOST_MAX_PENDING_TIMERS];
	int                       pending_timer_count;

	struct wren_hook_entry *hooks[WREN_HOOK_COUNT][WREN_HOST_MAX_HOOKS_PER_EVENT];
	int                     hook_count[WREN_HOOK_COUNT];

	struct wren_hook_entry *timers[WREN_HOST_MAX_TIMERS];
	int                     timer_count;

	/* Head of the cleanup queue.  remove() pushes here; the
	 * compactor (run at the top of each doterm() outer-loop
	 * iteration) drains it.  Empty == NULL — no counter needed. */
	struct wren_hook_entry *cleanup_head;

	/* Stack of input claims.  Each claim is owned by a fiber and
	 * delivers the next key/mouse event to a Wren callable.  Newest
	 * push is at `claim_top`; dispatch walks head-first.  Per-fiber
	 * uniqueness is enforced — a same-fiber push replaces the
	 * existing entry in place.  Registering does NOT imply any
	 * screen claim; scripts that want modal behavior must set
	 * CTerm.suspended = true explicitly to halt the wire pump.
	 * `claim_next_id` is monotonic so ClaimHandle.pop() can disarm
	 * a stale handle without confusing it with a fresh entry that
	 * happened to be allocated at the same address. */
	struct wren_input_claim {
		WrenHandle  *fiber;     /* pinned; identifies owner       */
		WrenHandle  *fn;        /* pinned for claim's lifetime    */
		uint64_t     id;
		struct wren_input_claim *next;
	}           *claim_top;
	uint64_t     claim_next_id;

	/* Pointer to doterm()'s local cterm_suspended flag.  Set by
	 * wren_host_bind_cterm_suspended() after init; backs the
	 * CTerm.suspended getter/setter.  When the flag is true, doterm()
	 * stops draining bytes from the wire so the underlying conn buffer
	 * fills, the TCP window shrinks, and the remote sees backpressure. */
	bool        *cterm_suspended;

	/* Pointer to doterm()'s local ooii_mode (0..3).  Set by
	 * wren_host_bind_ooii_mode() so the CTerm.ooiiMode Wren getter can
	 * report the live value.  NULL when no doterm() session is active. */
	int         *ooii_mode;

	/* Pointer to doterm()'s local `speed` (the throttle rate Alt-Up /
	 * Alt-Down walk over).  Set by wren_host_bind_speed(); backs the
	 * CTerm.throttleSpeed getter and the throttleSpeedUp/Down actions
	 * the default keys script binds. */
	int         *speed;

	/* Status-bar callable (Fn{|surface|...}).  Installed by Wren via
	 * Status.callable=(fn).  C invokes it from wren_status_render().
	 * NULL when no callable is set; in that case C leaves the row
	 * blank (prefilled default attribute, no content). */
	WrenHandle  *status_callable;

	/* Long-lived width×1 Surface, recycled across status renders to
	 * avoid per-frame allocation churn.  Reallocated when the terminal
	 * width changes.  Holds a class-instance handle pinning the
	 * foreign value so the GC doesn't reclaim it between renders. */
	WrenHandle  *status_surface;
	int          status_surface_width;

	/* Cached Fiber.isDone getter, used by the result drainer to skip
	 * deliveries to fibers that have terminated since the result was
	 * queued.  Built at init from wrenMakeCallHandle("isDone"). */
	WrenHandle  *fiber_isdone_handle;

	/* Result queue head/tail.  Workers may push from any thread;
	 * drained on the owner thread once per main-loop iteration. */
	pthread_mutex_t      result_mutex;
	struct wren_result  *result_head;
	struct wren_result  *result_tail;

	/* Doubly-linked lists of every live wren_file / wren_directory
	 * foreign.  Each foreign self-registers in its allocator (or in the
	 * C-side push helpers used by Directory.list / Directory.create /
	 * Directory.createDir / Host.cacheDirectory) and unregisters in its
	 * finalizer.  A successful Directory.delete walks both lists and
	 * marks any handle whose path is at-or-below the deleted entry as
	 * `dead`; subsequent operations on a dead handle abort the calling
	 * fiber with a "handle is dead" error rather than touching state. */
	struct wren_file       *fs_file_head;
	struct wren_directory  *fs_dir_head;

	struct bbslist *bbs;

	/* Pending session-end request from a Conn.endSession() Wren call.
	 * `pending_disconnect` is the trigger; `pending_disconnect_exit`
	 * tells doterm() whether to also tear down the app (Alt-X /
	 * window-close semantics) versus just hang up the BBS and return
	 * to the bbslist (Alt-H / Ctrl-Q semantics).  Drained by
	 * `wren_host_take_pending_disconnect` at the top of the doterm
	 * key-dispatch block. */
	bool pending_disconnect;
	bool pending_disconnect_exit;
};

/* Singleton state pointer (NULL when shut down). */
extern struct wren_host_state *wren_host_state(void);
/* `wren_host_take_pending_disconnect` lives in the public header
 * `wren_host.h`, so callers (e.g. term.c) only need that include. */

/* Hook registration helpers used by wren_bind.c.  Each calloc's a
 * fresh entry, captures the fn handle, appends a pointer into the
 * per-event array, and returns the entry.  The entry pointer is
 * stable for the entry's lifetime — compaction shifts the dispatch
 * array's pointer slots without moving the structs themselves, so
 * HookHandle keeps a single pointer the entire time.  Returns NULL
 * if the per-event limit is hit. */
struct wren_hook_entry *
wren_host_register_hook(WrenVM *vm, enum wren_hook_event ev, int fn_slot);

/* Filtered registration: only fires the hook when the event's
 * discriminator equals `filter`.  Filtered and unfiltered hooks share
 * one per-event array, preserving registration order across both. */
struct wren_hook_entry *
wren_host_register_hook_filtered(WrenVM *vm, enum wren_hook_event ev,
                                 int fn_slot, int filter);

/* Streaming-regex registration on the input event.  Takes ownership of
 * `prog`, `vm_state` (PikeVM*), and `buf`.  Lives in the same
 * WREN_HOOK_INPUT array as filtered/unfiltered hooks; registration
 * order is preserved across all three forms.  Resources are freed
 * during compaction once the hook is unregistered. */
struct wren_hook_entry *
wren_host_register_hook_match_ex(WrenVM *vm, int fn_slot, struct Prog *prog,
    struct PikeVM *vm_state, char *buf, size_t buf_cap, int nsubp,
    bool clean);

struct wren_hook_entry *
wren_host_register_hook_match(WrenVM *vm, int fn_slot, struct Prog *prog,
                              struct PikeVM *vm_state, char *buf,
                              size_t buf_cap, int nsubp);

/* Timer registration: ms is the recurrence interval, fn_slot holds the
 * callable.  Returns a pointer to the new entry, or NULL on failure. */
struct wren_hook_entry *
wren_host_register_timer(WrenVM *vm, int ms, int fn_slot);

/* Mark the entry for cleanup: release its callable handle (so the
 * dispatcher skips it immediately) and link it into the cleanup
 * queue.  Returns true if the entry was live, false if already
 * tombstoned.  The actual removal from the dispatch array, freeing
 * of regex resources, etc. happens in wren_host_compact(). */
bool wren_host_remove_entry(struct wren_hook_entry *h);

/* Drain the cleanup queue: for each tombstoned entry, free its
 * regex resources, find its slot in the dispatch array (state.hooks
 * for hook events, state.timers for timer events), shift later
 * pointers down to fill the gap, and mark the entry `unregistered`.
 * If the entry's HookHandle has already been GC'd (`gced` set), the
 * struct is freed here too; otherwise it stays alive so post-remove
 * metric reads keep working until the script drops the handle.
 * Must be called from the owner thread, OUTSIDE any dispatch
 * (typically at the top of doterm()'s outer loop). */
void wren_host_compact(void);

/* Foreign-method dispatch table, defined in wren_bind.c.  Returns NULL
 * if no method matches, in which case Wren reports a runtime error. */
WrenForeignMethodFn wren_bind_lookup(const char *module,
                                     const char *className,
                                     bool isStatic,
                                     const char *signature);

/* Foreign-class allocator/finalizer dispatch.  Returns {NULL, NULL} when
 * no foreign class matches.  Defined in wren_bind.c. */
WrenForeignClassMethods wren_bind_lookup_class(const char *module,
                                                const char *className);

/* Walk the input claim stack with the given event.  Returns true if
 * any claim returned consumed (or, on key, the claim handler consumed
 * it implicitly by virtue of always-consume semantics).  False means
 * no claim was on the stack, all claims passed through, or every
 * remaining claim's owning fiber was dead and got auto-pruned — in
 * which case the caller falls through to its hook dispatch path.
 * Defined in wren_bind_screen.c so the event-construction helpers
 * live next to push_key_event / push_mouse_event. */
bool wren_bind_dispatch_claim_key(int code);
bool wren_bind_dispatch_claim_mouse(struct mouse_event *ev);

/* True iff the Wren fiber has finished or aborted.  Implemented in
 * wren_host.c using the cached Fiber.isDone call handle.  Used both
 * by the result-queue drainer (skip dead fibers) and the input-claim
 * dispatcher (auto-prune dead-fiber claims). */
bool fiber_is_done(WrenHandle *fiber);

/* Walk the pending one-shot timer list and push TimerElapsed onto
 * the result queue for any entry past its due time.  Called once per
 * doterm() iteration, before wren_result_drain — the resulting
 * deliveries land on this iteration's drain. */
void wren_bind_sweep_pending_timers(void);

/* Embedded scripts table.  Generated by wren_embed_gen at build time
 * from src/syncterm/scripts/ *.wren and linked in as a separate .c.
 * Terminated by a {NULL, NULL, NULL} sentinel.
 *
 * `event` is NULL for pure library modules (live at scripts/ root,
 * loaded only on import) and "<eventname>" for auto-load entry scripts
 * (live under scripts/auto/<event>/, run when the host fires that
 * event).  Currently only "connected" exists; future events (startup,
 * ssh, ui, …) get their own auto/ subdirectory and matching string.
 * Inferred at build time by wren_embed_gen from the file's path. */
struct embedded_script {
	const char *name;
	const char *source;
	const char *event;
};
extern const struct embedded_script EMBEDDED_SCRIPTS[];

/* Log buffer (always-on print/error scrollback).  Stores up to 1024
 * messages; oldest evicted on overflow.  System.print and the three
 * WrenErrorType cases all flow into this buffer.  Wren scripts read
 * via the foreign Console class (count, [i], clear, iterate,
 * iteratorValue).  Per-message text is capped at 8 KB. */
enum wren_log_source {
	WREN_LOG_PRINT,
	WREN_LOG_COMPILE_ERROR,
	WREN_LOG_RUNTIME_ERROR,
	WREN_LOG_STACK_FRAME
};

/* Compile-error capture (used by Repl.eval to peek at errors from an
 * expression-mode compile attempt without committing them to the
 * user-visible log).  Between capture_start and capture_commit/clear,
 * any errors that would normally go to the log are diverted into a
 * private buffer instead.  Single-buffer, single-thread (Wren is); no
 * nesting.  Capture is also a no-op when wren_host_state() is NULL. */
void     wren_log_capture_start(void);
bool     wren_log_capture_active(void);
bool     wren_log_capture_contains(const char *needle);
void     wren_log_capture_clear(void);    /* drop captured entries */
void     wren_log_capture_commit(void);   /* flush captured entries to the log */

int      wren_log_count(void);
/* Total writes since wren_host_init.  Monotonic across the session,
 * not reset by wren_log_clear.  Wren scripts use it as a high-water
 * mark to drive incremental "show only new entries" rendering. */
uint64_t wren_log_total(void);
void     wren_log_clear(void);
/* Build (or reuse the cached) Wren tuple [ts, source, text] for the
 * entry whose sequence number is `seq` into Wren slot `slot`.  Sets
 * the slot to null if `seq` is outside the currently-buffered range
 * (already evicted, or not yet written). */
void     wren_log_emit(WrenVM *vm, uint64_t seq, int slot);

#endif /* WREN_HOST_INTERNAL_H */
