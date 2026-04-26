/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef WREN_HOST_INTERNAL_H
#define WREN_HOST_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wren.h"

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

struct wren_host_state {
	WrenVM      *vm;
	/* Cached method handles: call() with 0 and 1 arg.  Wren's Fn class
	 * uses signature "call()" / "call(_)". */
	WrenHandle  *call0_handle;
	WrenHandle  *call1_handle;

	WrenHandle  *hooks[WREN_HOOK_COUNT][WREN_HOST_MAX_HOOKS_PER_EVENT];
	int          hook_count[WREN_HOOK_COUNT];

	struct wren_timer_entry timers[WREN_HOST_MAX_TIMERS];
	int                     timer_count;

	struct bbslist *bbs;
};

/* Singleton state pointer (NULL when shut down). */
extern struct wren_host_state *wren_host_state(void);

/* Hook registration helpers used by wren_bind.c.  Takes a slot-1
 * function value, captures a handle for it, appends to the registry
 * for the event.  Returns false if the per-event limit is reached. */
bool wren_host_register_hook(WrenVM *vm, enum wren_hook_event ev, int fn_slot);

/* Timer registration: ms is the recurrence interval, fn_slot holds the
 * callable. */
bool wren_host_register_timer(WrenVM *vm, int ms, int fn_slot);

/* Foreign-method dispatch table, defined in wren_bind.c.  Returns NULL
 * if no method matches, in which case Wren reports a runtime error. */
WrenForeignMethodFn wren_bind_lookup(const char *module,
                                     const char *className,
                                     bool isStatic,
                                     const char *signature);

/* Capture sink for System.print output and runtime errors.  When set
 * (non-NULL), the host writeFn/errorFn callbacks route to this
 * function instead of stderr.  Used by the Wren console (REPL) to
 * gather output into a scrollback buffer.  Only one capture may be
 * active at a time; pass NULL to clear. */
typedef void (*wren_capture_fn)(const char *text, bool is_error,
                                void *cbdata);
void wren_host_set_capture(wren_capture_fn fn, void *cbdata);

/* Direct interpret call for the console; returns the WrenInterpretResult
 * code so the caller can highlight compile vs runtime errors. */
WrenInterpretResult wren_host_interpret(const char *module,
                                        const char *source);

/* Public access to the VM, used by the console to call methods on
 * captured handles directly when needed.  NULL when no VM is active. */
WrenVM *wren_host_vm(void);

#endif /* WREN_HOST_INTERNAL_H */
