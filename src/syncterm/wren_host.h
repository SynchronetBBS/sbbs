#ifndef WREN_HOST_H
#define WREN_HOST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct bbslist;
struct mouse_event;

/* Per-connection lifecycle. wren_host_init() loads every *.wren file
 * from SYNCTERM_PATH_SCRIPTS into a fresh VM and invokes <Module>.onConnect(bbs)
 * if defined. wren_host_shutdown() releases the VM. Repeated init without
 * intervening shutdown is a no-op. */
void wren_host_init(struct bbslist *bbs);
void wren_host_shutdown(void);

/* Returns true while a VM is loaded with at least one registered hook —
 * used by hot-path call sites to skip dispatcher overhead when nothing
 * is hooked. */
bool wren_host_active(void);

/* Bind a doterm()-local cterm_suspended flag to the host so the
 * CTerm.suspended Wren getter/setter can read and write it.  While
 * the flag is true, doterm() halts the wire-byte pump; bytes pile up
 * in the conn buffer and the remote eventually sees backpressure.
 * Pass NULL on shutdown to detach. */
void wren_host_bind_cterm_suspended(bool *flag);

/* Input-shaped dispatchers: return true to consume / drop the event,
 * false to pass through. The first hook returning true short-circuits;
 * subsequent hooks are not called for that event. */
bool wren_host_dispatch_key(int key);
bool wren_host_dispatch_input(unsigned char byte);
bool wren_host_dispatch_mouse(struct mouse_event *ev);

/* Status composition: copies a replacement status line into out (size
 * outsz including NUL) and returns true if a script provided one;
 * returns false to use the default. */
bool wren_host_compose_status(const char *def, char *out, size_t outsz);

/* True when log entries have arrived since the last
 * wren_host_mark_log_seen() (i.e., since the user last left the Wren
 * console).  Drives the bug indicator on the status bar.
 * `_error` is the subset that includes only compile errors, runtime
 * errors, and stack-frame entries; the status bar tints the indicator
 * red when set, yellow otherwise. */
bool wren_host_log_unread(void);
bool wren_host_log_unread_error(void);
void wren_host_mark_log_seen(void);

/* Fires any Hook.every() callbacks whose deadline has elapsed. Called
 * from doterm() just before the main-loop sleep. */
void wren_host_dispatch_timer(void);

/* Drain the result queue: for every queued completion, resume the
 * target fiber with its result (or skip if the fiber is done), then
 * release the fiber handle and free the result data.  Owner-thread
 * only.  Called from the doterm() main loop alongside wren_host_compact. */
void wren_result_drain(void);

/* Reclaim hook entries that scripts have unregistered.  Walks the
 * cleanup queue, shifts each entry's pointer out of the dispatch
 * array, frees regex resources, and frees the entry struct itself if
 * its HookHandle has already been GC'd.  Must run from the owner
 * thread, OUTSIDE any dispatcher — the doterm() outer loop calls it
 * once per iteration. */
void wren_host_compact(void);

#endif /* WREN_HOST_H */
