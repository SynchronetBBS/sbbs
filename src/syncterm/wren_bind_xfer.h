#ifndef WREN_BIND_XFER_H
#define WREN_BIND_XFER_H

/* Transfer-window bridge — file-static C state (log mailbox, tick
 * snapshot, abort flag, worker thread handle) and the foreigns Wren
 * uses to drive a transfer-progress UI.
 *
 * Two ways to drive a transfer:
 *
 *   1. From Wren — `Transfer.beginSession("fake")` spawns the built-in
 *      stub worker.  Used by ui_demo's TransferApp entry to exercise
 *      the UI without a real protocol attached.
 *
 *   2. From C — call `wren_run_transfer(label, worker_fn, arg)`.  The
 *      worker thread is spawned, then the Wren TransferApp is built
 *      and run; control returns when the worker calls `xfer_set_done`
 *      AND the App's run loop dismisses (timeout or keypress).  Used
 *      by term.c's protocol drivers (zmodem_download etc.).
 *
 * Worker threads use the public mailbox API below to push log lines
 * and update tick state.  All API is thread-safe — `xfer_log_push`
 * locks an internal mutex; `xfer_tick_lock` / `_get` / `_unlock`
 * lets the caller fill the borrowed struct without per-field
 * round-trips.  See memory/transfer_progress_wren_plan.md for the
 * full design. */

#include <stdbool.h>
#include <stdint.h>

#include "wren.h"

/* Tick state — shape matches the snapshot list TransferApp's
 * TickStateView consumes.  Worker fills under `xfer_tick_lock` /
 * `_unlock` then calls `xfer_tick_dirty` to wake the pump.
 *
 * Layout chosen so each protocol's progress callback formats its own
 * lines (the original C `cprintf`s) instead of pushing structured
 * data the Wren side has to reassemble per protocol.  TransferApp
 * paints:
 *
 *   row 0 — line1
 *   row 1 — line2
 *   row 2 — line3
 *   row 3 — when bytes_total > 0: centered "% nn"
 *           else: line4 (CET uses this for the "Remain" line)
 *   row 4 — when bytes_total > 0: bracketed [▒▒▒…] bar
 *           else: blank
 *
 * Empty line strings are skipped by the renderer, so protocols with
 * fewer than 3 status lines just leave the unused slots as "". */
struct xfer_tick_state {
	char    line1[256];
	char    line2[160];
	char    line3[160];
	char    line4[160];     /* only rendered when bytes_total == 0 */
	int64_t bytes_cur;
	int64_t bytes_total;    /* 0 = no bar / no auto percent */
};

/* ----- Public worker-side API ------------------------------------ */

/* Append a log line to the ring under mutex.  Severity uses syslog
 * priorities (LOG_NOTICE, LOG_INFO, LOG_ERR, ...); the Wren-side
 * LogView maps them to colors. */
void xfer_log_push(int level, const char *msg);

/* Borrow the tick struct under mutex.  Caller must `xfer_tick_lock`
 * before reading/writing through `xfer_tick_get` and `xfer_tick_unlock`
 * after.  Call `xfer_tick_dirty` (no lock needed) to wake the pump
 * once the new state is in. */
void                    xfer_tick_lock(void);
struct xfer_tick_state *xfer_tick_get(void);
void                    xfer_tick_unlock(void);
void                    xfer_tick_dirty(void);

/* Atomic abort flag — TransferApp sets via Transfer.requestAbort();
 * worker polls between protocol steps and unwinds when true. */
bool xfer_check_abort_atomic(void);

/* True between spawn_worker (begin) and join_and_clear (end).  Used
 * by term.c's lputs() to decide whether to route a log line through
 * the mailbox (in-session) or only to the log file (out-of-session). */
bool xfer_session_active(void);

/* Mark the worker complete.  `success == true` means the protocol
 * finished without aborting; the value is read back via
 * Transfer.success after Transfer.done flips. */
void xfer_set_done(bool success);

/* ----- Worker → main-thread dialog marshalling ------------------- */

/* Dialog kinds — surface the same enum to the Wren side via
 * Transfer.dialogPending.  0 means "no request outstanding". */
#define XFER_DLG_NONE             0
#define XFER_DLG_DUPLICATE        1
#define XFER_DLG_FILENAME_PROMPT  2

/* Response codes for XFER_DLG_DUPLICATE. */
#define XFER_DLG_OVERWRITE 0
#define XFER_DLG_SKIP      1
#define XFER_DLG_RENAME    2

/* Response codes for XFER_DLG_FILENAME_PROMPT. */
#define XFER_DLG_PROMPT_OK     0
#define XFER_DLG_PROMPT_CANCEL 1

/* Worker-side: ask the main-thread Wren TransferApp how to resolve a
 * filename collision.  Blocks on a condvar until the App posts a
 * response via Transfer.dialogRespond.  Returns one of the
 * XFER_DLG_* response constants.  When the response is RENAME, the
 * caller-supplied buffer is filled with the user's new filename
 * (truncated to new_name_sz - 1).  Safe to call from any thread but
 * MUST come from the worker (the main thread is the consumer). */
int xfer_request_duplicate(const char *filename,
                           char *new_name, size_t new_name_sz);

/* Worker-side: prompt the user for a filename (used by xmodem
 * recv when YMODEM detection fails mid-handshake and the protocol
 * falls back to plain XMODEM, which has no filename in the wire
 * stream).  `prompt` is shown as the dialog's title.  Returns true
 * with `out` filled when the user confirms, false on cancel.  Same
 * blocking semantics as xfer_request_duplicate. */
bool xfer_request_filename(const char *prompt,
                           char *out, size_t out_sz);

/* ----- C-side top-level entry ------------------------------------ */

/* Spawn the worker, build a TransferApp Wren instance, and call its
 * run() method.  Blocks until the worker finishes AND the App
 * dismisses (post-transfer wait, key, or timeout).  `label` is
 * passed to TransferApp.new(label) for display; protocol drivers
 * pass strings like "ZMODEM Download".  Worker signature matches
 * `_beginthread` (returns void) for portability across pthreads
 * and the Win32 thread API. */
void wren_run_transfer(const char *label,
                       void (*worker_fn)(void *), void *arg);

/* Invoke the Wren-side UploadApp protocol-picker → file-picker →
 * dispatch flow.  When `autoZ` is true the picker is skipped and
 * the flow goes straight to file-pick + ZMODEM upload (this is
 * how doterm()'s auto-ZRQINIT trigger reaches the Wren UI; the
 * keyboard Alt-U path goes directly through the Hook.onKey
 * handler in keys_default.wren without touching this function).
 * `lastCh` is the trailing wire byte that bias-feeds the xmodem
 * recv state — meaningless outside auto-Z. */
void wren_run_upload_app(bool autoZ, int lastCh);

/* Run any deferred Transfer.upload / uploadBatch / download dispatch
 * recorded by the Wren foreigns.  Must be called at C top-level
 * relative to the VM (NOT from inside a foreign method) because the
 * dispatch itself runs wrenCall to drive TransferApp.  doterm()
 * calls this once per iteration after hook handlers have run. */
void xfer_drain_pending(void);

/* ----- foreigns (Wren ABI surface) ------------------------------- */

void fn_Transfer_beginSession(WrenVM *vm);
void fn_Transfer_endSession(WrenVM *vm);
void fn_Transfer_drainLog(WrenVM *vm);
void fn_Transfer_tickDirty(WrenVM *vm);
void fn_Transfer_done(WrenVM *vm);
void fn_Transfer_success(WrenVM *vm);
void fn_Transfer_requestAbort(WrenVM *vm);
void fn_Transfer_aborted(WrenVM *vm);
void fn_Transfer_snapshot(WrenVM *vm);
void fn_Transfer_dialogPending(WrenVM *vm);
void fn_Transfer_dialogFilename(WrenVM *vm);
void fn_Transfer_dialogRespond(WrenVM *vm);
void fn_Transfer_upload(WrenVM *vm);
void fn_Transfer_uploadBatch(WrenVM *vm);
void fn_Transfer_download(WrenVM *vm);

/* Called by wren_host_shutdown so worker threads / dynamic state get
 * torn down cleanly even when the host exits mid-transfer. */
void wren_xfer_shutdown(void);

#endif /* WREN_BIND_XFER_H */
