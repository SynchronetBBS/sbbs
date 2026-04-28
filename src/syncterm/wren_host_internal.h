#ifndef WREN_HOST_INTERNAL_H
#define WREN_HOST_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wren.h"

struct mouse_event;
struct wren_file;
struct wren_directory;

/* Maximum simultaneous registrations per hook event and per timer slot.
 * These are intentionally small — there's no use case for hundreds of
 * scripts hooking the same event, and exceeding the cap simply drops
 * the registration with an error message. */
#define WREN_HOST_MAX_HOOKS_PER_EVENT 8
#define WREN_HOST_MAX_TIMERS          16

enum wren_hook_event {
	WREN_HOOK_KEY,
	WREN_HOOK_INPUT,
	WREN_HOOK_OUTPUT,
	WREN_HOOK_MOUSE,
	WREN_HOOK_STATUS,
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
	long double          next_fire_s;

	/* Streaming-regex fields (ev == WREN_HOOK_INPUT, regex hooks). */
	struct Prog         *regex_prog;
	struct PikeVM       *regex_vm;
	char                *regex_buf;
	size_t               regex_buf_len;
	size_t               regex_buf_cap;
	size_t               regex_pos;
	int                  regex_nsubp;

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

	/* Cached class handles for foreign classes that the C side
	 * allocates instances of (via wrenSetSlotNewForeign).  Filled
	 * lazily on first allocation; released at shutdown. */
	WrenHandle  *cell_class;
	WrenHandle  *cells_class;
	WrenHandle  *key_event_class;
	WrenHandle  *mouse_event_class;
	WrenHandle  *hook_handle_class;

	struct wren_hook_entry *hooks[WREN_HOOK_COUNT][WREN_HOST_MAX_HOOKS_PER_EVENT];
	int                     hook_count[WREN_HOOK_COUNT];

	struct wren_hook_entry *timers[WREN_HOST_MAX_TIMERS];
	int                     timer_count;

	/* Head of the cleanup queue.  remove() pushes here; the
	 * compactor (run at the top of each doterm() outer-loop
	 * iteration) drains it.  Empty == NULL — no counter needed. */
	struct wren_hook_entry *cleanup_head;

	/* When non-NULL, a Wren fiber is parked on Input.nextEvent() waiting
	 * for the next key/mouse event.  Set by Input._park, cleared and
	 * resumed by wren_host_dispatch_key/mouse before any hooks fire. */
	WrenHandle  *parked_fiber;

	/* Non-zero when wren_host_dispatch_output is on the call stack.
	 * conn_send fires dispatch_output, but conn_send is itself reachable
	 * from fn_Conn_send (a Wren foreign method) — calling wrenCall with
	 * a non-empty fiber corrupts its frame stack and has produced
	 * unbounded recursion in practice.  When this flag is set, nested
	 * dispatch_output returns false (no consume) so the bytes go through
	 * without firing onOutput hooks.  Wren-originated sends are already
	 * known to the script, so missing the hook callback is a feature,
	 * not a loss. */
	int          output_dispatching;

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
};

/* Singleton state pointer (NULL when shut down). */
extern struct wren_host_state *wren_host_state(void);

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

/* If a Wren fiber is parked on Input.nextEvent(), resume it with the
 * given event.  Returns true (consumed) if a fiber was waiting;
 * false (not consumed) if no fiber was parked, in which case the
 * caller should fall through to its hook dispatch path.  Defined
 * in wren_bind.c so the event-construction helpers live next to
 * push_key_event / push_mouse_event. */
bool wren_bind_resume_parked_key(int code);
bool wren_bind_resume_parked_mouse(struct mouse_event *ev);

/* Embedded scripts table.  Generated by wren_embed_gen at build time
 * from src/syncterm/scripts/ *.wren and linked in as a separate .c.
 * Terminated by a {NULL, NULL} sentinel. */
struct embedded_script {
	const char *name;
	const char *source;
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
