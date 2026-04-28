/*
 * Tiny wait shim for SyncTERM's C-side SFTP callers.  The library is
 * async-only — sftpc_<op>() returns a pending pointer and fires the
 * caller's callback when the back half completes.  Anything that
 * still wants synchronous send-then-block semantics wires up this
 * helper: an event, a callback that signals it, and a WaitForEvent
 * loop.
 *
 * This file is throwaway scaffolding.  The Wren-driven SFTP path
 * doesn't need it (foreign methods capture Fiber.current and let
 * the result-queue framework resume the fiber); once the C-side
 * SFTP UI is gone, sftp_wait.[ch] go with it.
 */

#ifndef SYNCTERM_SFTP_WAIT_H
#define SYNCTERM_SFTP_WAIT_H

#include <stdbool.h>

#include <eventwrap.h>

#include "sftp.h"

struct sftp_wait {
	xpevent_t evt;
};

/* Initialise/teardown the wait state.  evt is auto-reset so the same
 * struct can be reused across multiple sftpc_<op>() calls without
 * re-creating the event each time. */
bool sftp_wait_init(struct sftp_wait *w);
void sftp_wait_destroy(struct sftp_wait *w);

/* Callback to register on sftpc_<op>().  Reads cbdata as a struct
 * sftp_wait * and signals its event so the caller can return from
 * WaitForEvent. */
void sftp_wait_cb(struct sftpc_pending *p);

/* Per-op sync wrappers.  Each builds a struct sftp_wait, calls the
 * matching async sftpc_<op>(), waits, and returns the pending.  The
 * caller reads pending->err / result / estr / typed-extension fields,
 * then frees with sftpc_pending_free.  Returns NULL on alloc failure
 * (extremely rare; treat as transient). */
struct sftpc_pending          *sftp_sync_init(sftpc_state_t state);
struct sftpc_realpath_pending *sftp_sync_realpath(sftpc_state_t state,
                                                   const char *path);
struct sftpc_open_pending     *sftp_sync_open(sftpc_state_t state,
                                               const char *path,
                                               uint32_t flags);
struct sftpc_open_pending     *sftp_sync_opendir(sftpc_state_t state,
                                                  const char *path);
struct sftpc_pending          *sftp_sync_close(sftpc_state_t state,
                                                sftp_str_t handle);
struct sftpc_read_pending     *sftp_sync_read(sftpc_state_t state,
                                               sftp_str_t handle,
                                               uint64_t offset, uint32_t len);
struct sftpc_pending          *sftp_sync_write(sftpc_state_t state,
                                                sftp_str_t handle,
                                                uint64_t offset,
                                                sftp_str_t data);
struct sftpc_stat_pending     *sftp_sync_stat(sftpc_state_t state,
                                               const char *path);
struct sftpc_pending          *sftp_sync_setstat(sftpc_state_t state,
                                                  const char *path,
                                                  sftp_file_attr_t attr);
struct sftpc_readdir_pending  *sftp_sync_readdir(sftpc_state_t state,
                                                  sftp_str_t handle);
struct sftpc_descs_pending    *sftp_sync_descs(sftpc_state_t state,
                                                const char *path);

#endif /* SYNCTERM_SFTP_WAIT_H */
