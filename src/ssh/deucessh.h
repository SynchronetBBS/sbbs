#ifndef DSSH_H
#define DSSH_H

#include <inttypes.h>
#include <stdbool.h>

#include "deucessh-portable.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Error codes returned by library functions.  Zero is success;
 * negative values are errors. */
#define DSSH_ERROR_NONE           0
#define DSSH_ERROR_PARSE         -1   /* Malformed packet or field */
#define DSSH_ERROR_INVALID       -2   /* Valid parse but invalid value (MAC mismatch, bad signature) */
#define DSSH_ERROR_ALLOC         -3   /* Memory allocation failure */
#define DSSH_ERROR_INIT          -4   /* Initialization or crypto failure */
#define DSSH_ERROR_TERMINATED    -5   /* Session terminated (peer disconnected) */
#define DSSH_ERROR_TOOLATE       -6   /* Operation not allowed after session started */
#define DSSH_ERROR_TOOMANY       -7   /* Too many registered algorithms */
#define DSSH_ERROR_TOOLONG       -8   /* Data exceeds buffer or protocol limit */
#define DSSH_ERROR_MUST_BE_NULL  -9   /* Linked list next pointer was not NULL */
#define DSSH_ERROR_NOMORE       -10   /* No more items in name-list */
#define DSSH_ERROR_REKEY_NEEDED -11   /* Must rekey before sending more packets */

/* Opaque session handle.  Created by session_init, freed by session_cleanup. */
typedef struct dssh_session_s *dssh_session;

/*
 * I/O callback: send or receive exactly bufsz bytes.
 * Must block until all bytes are transferred or
 * dssh_session_is_terminated(sess) returns true.
 * Returns 0 on success, negative error code on failure.
 */
typedef int (*dssh_transport_io_cb)(uint8_t *buf, size_t bufsz,
    dssh_session sess, void *cbdata);

/*
 * Line-oriented receive callback for version exchange.
 * Reads until CR-LF is found.  Sets *bytes_received to the number
 * of bytes including the CR-LF.  Returns 0 on success.
 */
typedef int (*dssh_transport_rxline_cb)(uint8_t *buf, size_t bufsz,
    size_t *bytes_received, dssh_session sess, void *cbdata);

/*
 * Called for non-SSH lines received before the version string.
 * buf is NUL-terminated, bufsz is the length without NUL.
 * Return 0 to continue, negative to abort.
 */
typedef int (*dssh_transport_extra_line_cb)(uint8_t *buf, size_t bufsz,
    void *cbdata);

/*
 * Optional callback for SSH_MSG_DEBUG (RFC 4253 s11.3).
 * always_display: the sender's hint about whether to show the message.
 * message/message_len: the debug text (UTF-8, not NUL-terminated).
 */
typedef void (*dssh_debug_cb)(bool always_display,
    const uint8_t *message, size_t message_len, void *cbdata);

/*
 * Optional callback for SSH_MSG_UNIMPLEMENTED (RFC 4253 s11.4).
 * rejected_seq: the sequence number of the packet the peer did not understand.
 */
typedef void (*dssh_unimplemented_cb)(uint32_t rejected_seq,
    void *cbdata);

#include "deucessh-arch.h"

/*
 * Create a new session.  Allocates all internal state including
 * packet buffers sized to max_packet_size (pass 0 for the RFC
 * minimum of 33280 bytes).  Returns NULL on failure.
 */
DSSH_PUBLIC dssh_session dssh_session_init(bool client,
    size_t max_packet_size);

/*
 * Signal a session to terminate.  Sets terminate flag and returns true
 * if the session was initialized, false if already terminated.
 * Does not block — the session's I/O callbacks will see the flag.
 */
DSSH_PUBLIC bool dssh_session_terminate(dssh_session sess);

/*
 * Check whether a session has been terminated.
 */
DSSH_PUBLIC bool dssh_session_is_terminated(dssh_session sess);

/*
 * Clean up and free all session resources.  Calls terminate if needed.
 */
DSSH_PUBLIC void dssh_session_cleanup(dssh_session sess);

/*
 * Per-session I/O callback data (passed to the global I/O callbacks).
 */
DSSH_PUBLIC void dssh_session_set_cbdata(dssh_session sess,
    void *tx_cbdata, void *rx_cbdata, void *rx_line_cbdata,
    void *extra_line_cbdata);

/*
 * Optional notification callbacks.  Set before handshake.
 */
DSSH_PUBLIC void dssh_session_set_debug_cb(dssh_session sess,
    dssh_debug_cb cb, void *cbdata);
DSSH_PUBLIC void dssh_session_set_unimplemented_cb(
    dssh_session sess, dssh_unimplemented_cb cb, void *cbdata);
DSSH_PUBLIC void dssh_session_set_banner_cb(dssh_session sess,
    void *cb, void *cbdata);
DSSH_PUBLIC void dssh_session_set_global_request_cb(
    dssh_session sess, void *cb, void *cbdata);

/*
 * Set the SSH version identification strings (RFC 4253 s4.2).
 * Must be called before any session is initialized.
 *
 * software_version: e.g. "MySSH-1.0".  Must be non-empty printable
 *                   US-ASCII (0x21–0x7E, no spaces).  Pass NULL to
 *                   keep the library's built-in default.
 * comment:          optional (pass NULL to omit).  Printable US-ASCII
 *                   (0x20–0x7E); spaces are allowed in comments.
 *
 * The resulting version line "SSH-2.0-<version> <comment>\r\n"
 * must fit in 255 bytes per RFC 4253.
 *
 * If not called, defaults to the library's built-in version string.
 * Returns 0 on success, DSSH_ERROR_TOOLONG if the combined string
 * exceeds 255 bytes, DSSH_ERROR_INVALID if characters are out of
 * range, DSSH_ERROR_PARSE if software_version is empty (or comment
 * is empty when non-NULL), or DSSH_ERROR_TOOLATE if a session has
 * already been initialized.
 *
 * The strings are not copied — caller must ensure they remain
 * valid for the lifetime of the library's global configuration.
 */
DSSH_PUBLIC int dssh_transport_set_version(
    const char *software_version, const char *comment);

/*
 * Set the global I/O callbacks and optional extra-line callback.
 * Must be called before any session is initialized.
 * All sessions share the same callback functions; per-session
 * differentiation is via the cbdata pointers set above.
 */
DSSH_PUBLIC int dssh_transport_set_callbacks(
    dssh_transport_io_cb tx, dssh_transport_io_cb rx,
    dssh_transport_rxline_cb rx_line,
    dssh_transport_extra_line_cb extra_line_cb);

/*
 * Perform the SSH transport handshake (version exchange, KEXINIT,
 * key exchange, NEWKEYS).  Single-threaded, call before session_start.
 */
DSSH_PUBLIC int dssh_transport_handshake(dssh_session sess);

/*
 * Send SSH_MSG_DISCONNECT and set the terminate flag.
 */
DSSH_PUBLIC int dssh_transport_disconnect(dssh_session sess,
    uint32_t reason_code, const char *description);

/* Query negotiated algorithm names (NULL if not yet negotiated). */
DSSH_PUBLIC const char *dssh_transport_get_remote_version(dssh_session sess);
DSSH_PUBLIC const char *dssh_transport_get_kex_name(dssh_session sess);
DSSH_PUBLIC const char *dssh_transport_get_hostkey_name(dssh_session sess);
DSSH_PUBLIC const char *dssh_transport_get_enc_name(dssh_session sess);
DSSH_PUBLIC const char *dssh_transport_get_mac_name(dssh_session sess);

#ifdef __cplusplus
}
#endif

#endif
