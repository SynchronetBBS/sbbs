/*
 * Shared accessors for the per-SSH-session SFTP channel + client state.
 * Owned by ssh.c; consumed by the SFTP browser, transfer queue, and
 * status-bar indicator code.
 */

#ifndef SYNCTERM_SFTP_SESSION_H
#define SYNCTERM_SFTP_SESSION_H

#include <stdatomic.h>
#include <stdbool.h>

#include "sftp.h"

/*
 * sftp_state is valid when sftp_available is true.  Lifetime spans from
 * ssh_connect() success to the start of ssh_close(): once ssh_close sets
 * sftp_available = false, callers must not issue new sftpc_* calls,
 * though in-flight calls are still woken by sftpc_finish().
 */
extern sftpc_state_t    sftp_state;
extern _Atomic bool     sftp_available;

/*
 * True while the interactive shell channel (ssh_chan) is open.  May flip
 * to false while sftp_available remains true (server closed the shell;
 * SFTP subsystem may still be usable for already-enqueued transfers).
 */
extern _Atomic bool     sftp_shell_alive;

#endif /* SYNCTERM_SFTP_SESSION_H */
