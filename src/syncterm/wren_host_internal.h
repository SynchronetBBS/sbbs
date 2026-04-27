/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef WREN_HOST_INTERNAL_H
#define WREN_HOST_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wren.h"

struct mouse_event;

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
	WREN_HOOK_COUNT
};

struct wren_timer_entry {
	WrenHandle  *callable;
	uint32_t     interval_ms;
	long double  next_fire_s;   /* xp_timer() epoch in seconds */
};

/* Per-hook entry: a Wren callable plus an optional integer match
 * filter, plus an optional streaming-regex match filter (mutually
 * exclusive with the integer filter).  The dispatcher branches on
 * shape:
 *   - filtered && !regex: skip unless event discriminator == filter
 *   - regex != NULL: feed each input byte to pikevm_step, fire on
 *     MATCH, drop a byte on IMPOSSIBLE, wait on PENDING (input only)
 *   - neither: fire unconditionally
 * `filter` is unused for non-discriminator events (output, status). */
struct PikeVM;	/* opaque; full type in re1/regexp.h */

struct wren_hook_entry {
	WrenHandle    *fn;
	int            filter;
	bool           filtered;
	struct Prog   *regex_prog;	/* compiled program; freed at shutdown */
	struct PikeVM *regex_vm;	/* streaming match state */
	char          *regex_buf;	/* persistent so Sub captures stay valid */
	size_t         regex_buf_len;
	size_t         regex_buf_cap;
	size_t         regex_pos;	/* next offset the VM expects to consume */
	int            regex_nsubp;
};

struct wren_host_state {
	WrenVM      *vm;
	/* Cached method handles: call() with 0 and 1 arg.  Wren's Fn class
	 * uses signature "call()" / "call(_)". */
	WrenHandle  *call0_handle;
	WrenHandle  *call1_handle;

	/* Cached class handles for foreign classes that the C side
	 * allocates instances of (via wrenSetSlotNewForeign).  Captured
	 * after the syncterm module compiles; released at shutdown. */
	WrenHandle  *cell_class;
	WrenHandle  *cells_class;
	WrenHandle  *key_event_class;
	WrenHandle  *mouse_event_class;

	struct wren_hook_entry hooks[WREN_HOOK_COUNT][WREN_HOST_MAX_HOOKS_PER_EVENT];
	int                    hook_count[WREN_HOOK_COUNT];

	struct wren_timer_entry timers[WREN_HOST_MAX_TIMERS];
	int                     timer_count;

	/* When non-NULL, a Wren fiber is parked on Input.nextEvent waiting
	 * for the next key/mouse event.  Set by Input._park, cleared and
	 * resumed by wren_host_dispatch_key/mouse before any hooks fire. */
	WrenHandle  *parked_fiber;

	struct bbslist *bbs;
};

/* Singleton state pointer (NULL when shut down). */
extern struct wren_host_state *wren_host_state(void);

/* Hook registration helpers used by wren_bind.c.  Takes a slot-1
 * function value, captures a handle for it, appends to the registry
 * for the event.  Returns false if the per-event limit is reached. */
bool wren_host_register_hook(WrenVM *vm, enum wren_hook_event ev, int fn_slot);

/* Filtered registration: only fires the hook when the event's
 * discriminator equals `filter`.  Filtered and unfiltered hooks share
 * one per-event array, preserving registration order across both. */
bool wren_host_register_hook_filtered(WrenVM *vm, enum wren_hook_event ev,
                                      int fn_slot, int filter);

/* Streaming-regex registration on the input event.  Takes ownership of
 * `prog` (frees at shutdown), `vm_state` (PikeVM*; pikevm_free at
 * shutdown), and `buf` (free at shutdown).  Lives in the same
 * WREN_HOOK_INPUT array as filtered/unfiltered hooks; registration
 * order is preserved across all three forms. */
bool wren_host_register_hook_match(WrenVM *vm, int fn_slot,
                                   struct Prog *prog, struct PikeVM *vm_state,
                                   char *buf, size_t buf_cap, int nsubp);

/* Timer registration: ms is the recurrence interval, fn_slot holds the
 * callable. */
bool wren_host_register_timer(WrenVM *vm, int ms, int fn_slot);

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

/* Inject the module-level `Cache` variable into the syncterm module.
 * Must be called once after the builtin module source has been
 * interpreted.  Defined in wren_bind.c. */
void wren_bind_define_cache(WrenVM *vm);

/* If a Wren fiber is parked on Input.nextEvent, resume it with the
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
