/*
 * Shared accessors for the per-SSH-session SFTP channel + client state.
 * Owned by ssh.c; consumed by the SFTP browser, transfer queue, and
 * status-bar indicator code.
 */

#ifndef SYNCTERM_SFTP_SESSION_H
#define SYNCTERM_SFTP_SESSION_H

#ifdef _MSC_VER
/* MSVC keeps __STDC_NO_ATOMICS__ defined even with /experimental:c11atomics
 * because its atomic support is incomplete (no generic _Atomic with locks),
 * and the stdatomic.h header otherwise refuses to expand.  Same trick used
 * in conn.h. */
#undef __STDC_NO_ATOMICS__
#endif
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
