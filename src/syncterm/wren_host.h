#ifndef WREN_HOST_H
#define WREN_HOST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct bbslist;
struct ciolib_key_event;
struct mouse_event;

/* Per-connection lifecycle. wren_host_init() loads every *.wren file
 * from SYNCTERM_PATH_SCRIPTS into a fresh VM and invokes <Module>.onConnect(bbs)
 * if defined. wren_host_shutdown() releases the VM. Repeated init without
 * intervening shutdown is a no-op. */
void wren_host_init(struct bbslist *bbs);
void wren_host_shutdown(void);

/* Register an extra Wren file to load at the end of every wren_host_init
 * (i.e. once per connect).  Backs the -W command-line flag.  The file is
 * interpreted as a filename-derived module after all embedded + user
 * auto-load scripts have finished, so its imports resolve against the
 * full surface.  Compile / load errors print "[wren] <path>: load
 * failed" to stderr.  Pass NULL to clear.  The current value is exposed
 * to scripts via Host.launchScript so they can detect whether they were
 * invoked from the command line. */
void wren_host_set_launch_script(const char *path);
const char *wren_host_launch_script(void);

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

/* Bind a doterm()-local ooii_mode int to the host so the CTerm.ooiiMode
 * Wren getter can report it.  Pass NULL on shutdown to detach. */
void wren_host_bind_ooii_mode(int *mode);

/* Bind a doterm()-local `speed` int to the host so the
 * CTerm.throttleSpeed getter and the throttleSpeedUp/Down actions can
 * see and mutate it.  `speed` is the network character-pacing rate
 * (BPS); 0 means unthrottled.  Pass NULL on shutdown to detach. */
void wren_host_bind_speed(int *speed);

/* Status-bar render: invoke the Wren callable installed via
 * Status.callable=(fn) with a width×1 Surface that has been pre-filled
 * to a blank default-attribute row.  The script mutates the surface in
 * place; on return, *out_buf is set to the surface's cell buffer
 * (caller must NOT free) and the function returns true.  Returns false
 * (and leaves *out_buf untouched) if no callable is registered, the VM
 * is not on the owner thread, the call fails, or width is zero. */
struct vmem_cell;
bool wren_status_render(int width, struct vmem_cell **out_buf);

/* Input-shaped dispatchers: return true to consume / drop the event,
 * false to pass through. The first hook returning true short-circuits;
 * subsequent hooks are not called for that event. */
bool wren_host_dispatch_key(int key);
bool wren_host_dispatch_physical_key(const struct ciolib_key_event *ev);
bool wren_host_dispatch_mouse(struct mouse_event *ev);
bool wren_host_wants_physical_keys(void);
void wren_host_enable_physical_key_events(void);

/* Establish a new input epoch and discard every conio event which could
 * have been queued by the previous owner of the terminal.  Input-shaped
 * Wren completions from an older epoch are discarded when drained. */
void wren_host_input_barrier(void);

/* Atomically read-and-clear a pending session-end request (set by a
 * Conn.endSession() Wren call).  Returns true when a request was
 * queued since the last call; on true, `*exit_app_out` reflects
 * whether the script asked for full app exit (Alt-X / window-close
 * semantics) versus just hangup (Alt-H / Ctrl-Q semantics).  No-op
 * (returns false) when the host isn't initialised. */
bool wren_host_take_pending_disconnect(bool *exit_app_out);

/* Per-byte onInput dispatch.  The hook chain runs in registration
 * order; the first hook that returns Bool true (drop) or String
 * (replace) wins, later hooks don't see this byte.
 *
 * Returns:
 *   WREN_INPUT_KEEP  (0)  — byte unchanged, pass through.
 *   WREN_INPUT_DROP  (-1) — byte consumed, drop it.
 *   N > 0                 — byte consumed, replace with the N bytes
 *                           written into `out`.  N never exceeds
 *                           `out_cap`; replacements that would exceed
 *                           it log a runtime error and fall through
 *                           as KEEP. */
#define WREN_INPUT_KEEP   0
#define WREN_INPUT_DROP   (-1)
int wren_host_dispatch_input(unsigned char byte, char *out, int out_cap);

/* True when log entries have arrived since the last
 * wren_host_mark_log_seen() (i.e., since the user last left the Wren
 * console).  Drives the bug indicator on the status bar.
 * `_error` is the subset that includes only compile errors, runtime
 * errors, and stack-frame entries; the status bar tints the indicator
 * red when set, yellow otherwise. */
bool wren_host_log_unread(void);
bool wren_host_log_unread_error(void);
void wren_host_mark_log_seen(void);

/* Show a modal Wren Alert from C-owned terminal paths.  Returns false
 * when no Wren VM is active, the call is off-thread, the bridge failed
 * to load, or the Wren call aborts; callers should fall back to their
 * historical UI in that case. */
bool wren_host_alert(const char *title, const char *message);

/* Fires any Hook.every() callbacks whose deadline has elapsed. Called
 * from doterm() just before the main-loop sleep. */
void wren_host_dispatch_timer(void);

/* Fired by term.c when the SSH shell channel has closed but the
 * session is still up (e.g., SFTP transfers may still be in flight).
 * Hook.onShellClose handlers run; they typically open a degraded
 * modal in a child fiber so the calling site can return.  No payload. */
void wren_host_dispatch_shell_close(void);

/* Fired by term.c at the end of the disconnect path, after the main
 * loop has fully exited but before the BBS / Wren VM tear down.
 * Hook.onDisconnect handlers run; intended for final local-state
 * flushes (no SFTP available at this point — sftpc_finish has run). */
void wren_host_dispatch_disconnect(void);

/* SFTP-active flag set by the Wren-side SftpQueue (CTerm.sftpActive=).
 * Read by ssh.c when deciding whether to set conn_api.terminate on
 * shell-channel close or wire idle — keep the SSH session up while
 * Wren has SFTP work in flight.  Reset on VM teardown. */
bool wren_sftp_active(void);
void wren_sftp_active_reset(void);

/* Drain the result queue: for every queued completion, resume the
 * target fiber with its result (or skip if the fiber is done), then
 * release the fiber handle and free the result data.  Owner-thread
 * only.  Called from the doterm() main loop alongside wren_host_compact. */
void wren_result_drain(void);

/* Walk the Timer.trigger pending list and push TimerElapsed onto the
 * result queue for any entry whose due time has passed.  Called from
 * doterm() just before wren_result_drain so newly-due deliveries fire
 * on this iteration. */
void wren_bind_sweep_pending_timers(void);

/* Reclaim hook entries that scripts have unregistered.  Walks the
 * cleanup queue, shifts each entry's pointer out of the dispatch
 * array, frees regex resources, and frees the entry struct itself if
 * its HookHandle has already been GC'd.  Must run from the owner
 * thread, OUTSIDE any dispatcher — the doterm() outer loop calls it
 * once per iteration. */
void wren_host_compact(void);

#endif /* WREN_HOST_H */
