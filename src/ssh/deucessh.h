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
#define DSSH_ERROR_NONE 0
#define DSSH_ERROR_PARSE -1         /* Malformed packet or field */
#define DSSH_ERROR_INVALID -2       /* Valid parse but invalid value (MAC mismatch, bad signature) */
#define DSSH_ERROR_ALLOC -3         /* Memory allocation failure */
#define DSSH_ERROR_INIT -4          /* Initialization or crypto failure */
#define DSSH_ERROR_TERMINATED -5    /* Session terminated (peer disconnected) */
#define DSSH_ERROR_TOOLATE -6       /* Operation not allowed after session started */
#define DSSH_ERROR_TOOMANY -7       /* Too many registered algorithms */
#define DSSH_ERROR_TOOLONG -8       /* Data exceeds buffer or protocol limit */
#define DSSH_ERROR_MUST_BE_NULL -9  /* Linked list next pointer was not NULL */
#define DSSH_ERROR_NOMORE -10       /* No more items in name-list */
#define DSSH_ERROR_REKEY_NEEDED -11    /* Must rekey before sending more packets */
#define DSSH_ERROR_AUTH_REJECTED -12   /* Authentication rejected by peer */
#define DSSH_ERROR_REJECTED -13        /* Channel open or request rejected by peer */
#define DSSH_ERROR_TIMEOUT -14         /* Operation timed out waiting for peer */

/* Opaque session handle.  Created by session_init, freed by session_cleanup. */
typedef struct dssh_session_s *dssh_session;

/*
 * I/O callback: send or receive exactly bufsz bytes.
 * Must block until all bytes are transferred or
 * dssh_session_is_terminated(sess) returns true.
 * Returns 0 on success, negative error code on failure.
 *
 * When dssh_session_is_terminated() returns true, the callback
 * MUST return a negative error code within a reasonable period.
 * Failure to do so will cause dssh_session_cleanup() to hang
 * on thread join.  Use the terminate callback
 * (dssh_session_set_terminate_cb) to be notified when the
 * session terminates, e.g. to close the underlying socket.
 */
typedef int (*dssh_transport_io_cb)(uint8_t *buf, size_t bufsz,
    dssh_session sess, void *cbdata);

/*
 * Line-oriented receive callback for version exchange.
 * Reads until CR-LF is found.  Sets *bytes_received to the number
 * of bytes including the CR-LF.  Returns 0 on success.
 *
 * Optional: pass NULL to dssh_transport_set_callbacks() to use a
 * default implementation that reads one byte at a time via the rx
 * callback.
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
    void                                      *cbdata);

/*
 * Optional callback invoked exactly once when the session
 * terminates (fatal error, peer disconnect, or explicit
 * dssh_session_terminate() call).  Intended for closing the
 * underlying socket or signaling an event loop so that
 * blocked I/O callbacks return promptly.
 *
 * The callback may be invoked from any thread, including from
 * within an I/O callback on the same session.  It must not
 * call any dssh_* function on this session except
 * dssh_session_is_terminated().
 *
 * Must be set before dssh_session_start().
 */
typedef void (*dssh_terminate_cb)(dssh_session sess, void *cbdata);

/*
 * Parse a big-endian uint32 from buf[0..bufsz-1] into *val.
 * Returns 4 (bytes consumed) on success, or a negative
 * DSSH_ERROR_* code on failure (DSSH_ERROR_INVALID if buf or
 * val is NULL, DSSH_ERROR_PARSE if bufsz < 4).
 */
DSSH_PUBLIC int64_t dssh_parse_uint32(const uint8_t *buf, size_t bufsz, uint32_t *val);

/*
 * Serialize val as a big-endian uint32 into buf at offset *pos.
 * Advances *pos by 4.  Returns 0 on success, or a negative
 * DSSH_ERROR_* code on failure (DSSH_ERROR_INVALID if buf or
 * pos is NULL, DSSH_ERROR_TOOLONG if the buffer has fewer than
 * 4 bytes remaining at *pos).
 */
DSSH_PUBLIC int dssh_serialize_uint32(uint32_t val, uint8_t *buf, size_t bufsz, size_t *pos);

/*
 * Create a new session.  Allocates all internal state including
 * packet buffers sized to max_packet_size (pass 0 for the RFC
 * minimum of 33280 bytes).  Returns NULL on failure.
 */
DSSH_PUBLIC dssh_session dssh_session_init(bool client,
    size_t                                      max_packet_size);

/*
 * Signal a session to terminate.  Sets terminate flag and returns true
 * if the session was initialized, false if already terminated.
 * Does not block -- the session's I/O callbacks will see the flag.
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
 * Securely scrub a memory buffer (e.g. a password).
 * Resists compiler optimization.  Does not require the application
 * to link against OpenSSL.  Do not realloc() password buffers --
 * the old allocation won't be scrubbed.
 */
DSSH_PUBLIC void dssh_cleanse(void *buf, size_t len);

/*
 * Per-session I/O callback data (passed to the global I/O callbacks).
 */
DSSH_PUBLIC void dssh_session_set_cbdata(dssh_session sess,
    void *tx_cbdata, void *rx_cbdata, void *rx_line_cbdata,
    void *extra_line_cbdata);

/*
 * Optional notification callbacks.  Must be set before
 * dssh_session_start(); calling after start is undefined behavior.
 */
DSSH_PUBLIC void dssh_session_set_debug_cb(dssh_session sess,
    dssh_debug_cb cb, void *cbdata);
DSSH_PUBLIC void dssh_session_set_unimplemented_cb(dssh_session sess, dssh_unimplemented_cb cb, void *cbdata);
/*
 * Optional callback for SSH_MSG_USERAUTH_BANNER (RFC 4252 s5.4).
 * message/message_len: the banner text (UTF-8, not NUL-terminated).
 * language/language_len: the language tag (may be empty).
 */
typedef void (*dssh_auth_banner_cb)(const uint8_t *message,
    size_t message_len, const uint8_t *language, size_t language_len,
    void *cbdata);

/*
 * Optional callback for SSH_MSG_GLOBAL_REQUEST (RFC 4254 s4).
 * name/name_len: the request name.
 * want_reply: whether the sender expects a response.
 * data/data_len: request-specific data following the name.
 * Return >= 0 to accept, < 0 to reject.
 */
typedef int (*dssh_global_request_cb)(const uint8_t *name,
    size_t name_len, bool want_reply, const uint8_t *data,
    size_t data_len, void *cbdata);

DSSH_PUBLIC void dssh_session_set_banner_cb(dssh_session sess,
    dssh_auth_banner_cb cb, void *cbdata);
DSSH_PUBLIC void dssh_session_set_global_request_cb(dssh_session sess,
    dssh_global_request_cb cb, void *cbdata);
DSSH_PUBLIC void dssh_session_set_terminate_cb(dssh_session sess,
    dssh_terminate_cb cb, void *cbdata);

/*
 * Set the session inactivity timeout in milliseconds.
 * Applies to internal waits for peer responses (channel open
 * confirmation, channel request replies, setup messages, and
 * rekey completion).  Default is 75000 (75 seconds, matching
 * the standard BSD TCP connect timeout).  Values <= 0 disable
 * the timeout (wait indefinitely).
 * Must be set before dssh_session_start().
 */
DSSH_PUBLIC void dssh_session_set_timeout(dssh_session sess,
    int timeout_ms);

/*
 * Set the SSH version identification strings (RFC 4253 s4.2).
 * Must be called before any session is initialized.
 *
 * software_version: e.g. "MySSH-1.0".  Must be non-empty printable
 *                   US-ASCII (0x21--0x7E, no spaces).  Pass NULL to
 *                   keep the library's built-in default.
 * comment:          optional (pass NULL to omit).  Printable US-ASCII
 *                   (0x20--0x7E); spaces are allowed in comments.
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
 * The strings are not copied -- caller must ensure they remain
 * valid for the lifetime of the library's global configuration.
 */
DSSH_PUBLIC int dssh_transport_set_version(const char *software_version, const char *comment);

/*
 * Set the global I/O callbacks and optional extra-line callback.
 * Must be called before any session is initialized.
 * All sessions share the same callback functions; per-session
 * differentiation is via the cbdata pointers set above.
 * rx_line may be NULL — a default that reads one byte at a time
 * via the rx callback is used.
 */
DSSH_PUBLIC int dssh_transport_set_callbacks(dssh_transport_io_cb tx, dssh_transport_io_cb rx,
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

#endif // ifndef DSSH_H
