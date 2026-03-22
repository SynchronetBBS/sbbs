// RFC 4252: SSH Authentication Protocol

#ifndef DEUCE_SSH_AUTH_H
#define DEUCE_SSH_AUTH_H

#include "deucessh.h"

#define SSH_MSG_USERAUTH_REQUEST          UINT8_C(50)
#define SSH_MSG_USERAUTH_FAILURE          UINT8_C(51)
#define SSH_MSG_USERAUTH_SUCCESS          UINT8_C(52)
#define SSH_MSG_USERAUTH_BANNER           UINT8_C(53)

#define SSH_MSG_USERAUTH_PK_OK                UINT8_C(60)
#define SSH_MSG_USERAUTH_PASSWD_CHANGEREQ     UINT8_C(60)
#define SSH_MSG_USERAUTH_INFO_REQUEST         UINT8_C(60)
#define SSH_MSG_USERAUTH_INFO_RESPONSE        UINT8_C(61)

/*
 * Keyboard-interactive prompt callback (RFC 4256).
 * Called once per INFO_REQUEST with the name, instruction, and
 * individual prompts.  The callback must fill in responses[i] for
 * each prompt.  Each response must be a malloc'd string that the
 * library will free after sending the INFO_RESPONSE.
 *
 * name/name_len:    server-provided name string (may be empty)
 * instruction/instruction_len: server-provided instruction (may be empty)
 * num_prompts:      number of prompts
 * prompts[i]/prompt_lens[i]: the prompt text for each prompt
 * echo[i]:          true if the response should be echoed (not a password)
 * responses[i]:     OUT — caller sets each to a malloc'd string
 * response_lens[i]: OUT — caller sets each to the string length
 * cbdata:           opaque pointer from the caller
 *
 * Return 0 to continue authentication, negative to abort.
 */
typedef int (*deuce_ssh_auth_kbi_prompt_cb)(
    const uint8_t *name, size_t name_len,
    const uint8_t *instruction, size_t instruction_len,
    uint32_t num_prompts,
    const uint8_t **prompts, const size_t *prompt_lens,
    const bool *echo,
    uint8_t **responses, size_t *response_lens,
    void *cbdata);

/*
 * Optional callback for SSH_MSG_USERAUTH_BANNER (RFC 4252 s5.4).
 * The server may send banner messages during authentication.
 * message/message_len: the banner text (UTF-8, not NUL-terminated).
 * language/language_len: the language tag (may be empty).
 */
typedef void (*deuce_ssh_auth_banner_cb)(
    const uint8_t *message, size_t message_len,
    const uint8_t *language, size_t language_len,
    void *cbdata);

/*
 * Query available authentication methods by sending a "none"
 * auth request.  Sends SSH_MSG_SERVICE_REQUEST automatically
 * if not already sent.  On return:
 *   0 = no auth required (rare; methods buffer is empty)
 *   1 = auth required; methods is a comma-separated list
 *       (e.g., "publickey,password,keyboard-interactive")
 *   <0 = error
 */
DEUCE_SSH_PUBLIC int deuce_ssh_auth_get_methods(deuce_ssh_session sess,
    const char *username, char *methods, size_t methods_sz);

/*
 * Password change callback (RFC 4252 s8).
 * Called when the server requests a password change
 * (SSH_MSG_USERAUTH_PASSWD_CHANGEREQ).
 *
 * prompt/prompt_len: the server's prompt text (UTF-8, not NUL-terminated).
 * language/language_len: the language tag (may be empty).
 * new_password: OUT — caller sets to a malloc'd string containing the
 *               new password.  The library frees it after sending.
 * new_password_len: OUT — length of the new password.
 * cbdata: opaque pointer from the caller.
 *
 * Return 0 to send the new password, negative to abort authentication.
 */
typedef int (*deuce_ssh_auth_passwd_change_cb)(
    const uint8_t *prompt, size_t prompt_len,
    const uint8_t *language, size_t language_len,
    uint8_t **new_password, size_t *new_password_len,
    void *cbdata);

/*
 * Authenticate with a password (RFC 4252 s8).
 * If passwd_change_cb is not NULL and the server requests a password
 * change, the callback is invoked to get the new password.
 * If passwd_change_cb is NULL and the server requests a change,
 * authentication fails with DEUCE_SSH_ERROR_INIT.
 * Returns 0 on success, negative on failure or rejection.
 */
DEUCE_SSH_PUBLIC int deuce_ssh_auth_password(deuce_ssh_session sess,
    const char *username, const char *password,
    deuce_ssh_auth_passwd_change_cb passwd_change_cb, void *passwd_change_cbdata);

/*
 * Authenticate via keyboard-interactive (RFC 4256).
 * For each INFO_REQUEST, the prompt_cb is called with the server's
 * name, instruction, and prompts.  The callback provides responses.
 * Returns 0 on success, negative on failure or rejection.
 */
DEUCE_SSH_PUBLIC int deuce_ssh_auth_keyboard_interactive(deuce_ssh_session sess,
    const char *username, deuce_ssh_auth_kbi_prompt_cb prompt_cb,
    void *cbdata);

/* ================================================================
 * Server-side authentication
 * ================================================================ */

/*
 * Server auth callback results.
 * Return from the verify callbacks to indicate the outcome.
 */
#define DEUCE_SSH_AUTH_SUCCESS          0   /* User authenticated */
#define DEUCE_SSH_AUTH_FAILURE         -1   /* Wrong credentials */
#define DEUCE_SSH_AUTH_PARTIAL          1   /* Succeeded, but more auth needed */
#define DEUCE_SSH_AUTH_CHANGE_PASSWORD  2   /* Password expired (password method only) */

/*
 * Server-side password verification callback.
 * Return DEUCE_SSH_AUTH_SUCCESS, DEUCE_SSH_AUTH_FAILURE,
 * DEUCE_SSH_AUTH_PARTIAL, or DEUCE_SSH_AUTH_CHANGE_PASSWORD.
 *
 * When DEUCE_SSH_AUTH_CHANGE_PASSWORD is returned, set
 * *change_prompt to a malloc'd prompt string (the library frees it).
 * The library will send PASSWD_CHANGEREQ and call passwd_change_cb
 * (below) when the client responds.
 */
typedef int (*deuce_ssh_auth_server_password_cb)(
    const uint8_t *username, size_t username_len,
    const uint8_t *password, size_t password_len,
    uint8_t **change_prompt, size_t *change_prompt_len,
    void *cbdata);

/*
 * Server-side password change callback.
 * Called when the client sends a password change request (boolean TRUE).
 * Validate the old password and set the new one.
 * Return DEUCE_SSH_AUTH_SUCCESS, DEUCE_SSH_AUTH_FAILURE, or
 * DEUCE_SSH_AUTH_CHANGE_PASSWORD (to re-prompt).
 */
typedef int (*deuce_ssh_auth_server_passwd_change_cb)(
    const uint8_t *username, size_t username_len,
    const uint8_t *old_password, size_t old_password_len,
    const uint8_t *new_password, size_t new_password_len,
    uint8_t **change_prompt, size_t *change_prompt_len,
    void *cbdata);

/*
 * Server-side public key verification callback.
 * Verify that the given public key is authorized for the user.
 * Return DEUCE_SSH_AUTH_SUCCESS if the key is acceptable,
 * DEUCE_SSH_AUTH_FAILURE if not, DEUCE_SSH_AUTH_PARTIAL if more
 * auth is needed.
 *
 * When has_signature is false, this is a key probe (PK_OK query) —
 * return SUCCESS to indicate the key is acceptable (the library
 * sends PK_OK), FAILURE to reject.
 *
 * When has_signature is true, the library has already verified the
 * signature.  This callback only needs to check authorization.
 */
typedef int (*deuce_ssh_auth_server_publickey_cb)(
    const uint8_t *username, size_t username_len,
    const char *algo_name,
    const uint8_t *pubkey_blob, size_t pubkey_blob_len,
    bool has_signature,
    void *cbdata);

/*
 * Server-side "none" auth callback.
 * Return DEUCE_SSH_AUTH_SUCCESS to grant access without credentials.
 * Return DEUCE_SSH_AUTH_FAILURE to require authentication.
 */
typedef int (*deuce_ssh_auth_server_none_cb)(
    const uint8_t *username, size_t username_len,
    void *cbdata);

/*
 * Server-side authentication callbacks and configuration.
 * Set the callbacks you support; leave others NULL.
 * methods_str is a comma-separated list of supported method names
 * (e.g., "publickey,password") sent in FAILURE responses.
 */
struct deuce_ssh_auth_server_cbs {
	const char *methods_str;
	deuce_ssh_auth_server_none_cb none_cb;
	deuce_ssh_auth_server_password_cb password_cb;
	deuce_ssh_auth_server_passwd_change_cb passwd_change_cb;
	deuce_ssh_auth_server_publickey_cb publickey_cb;
	void *cbdata;
};

/*
 * Run the server-side authentication loop (RFC 4252).
 * Receives SSH_MSG_SERVICE_REQUEST, sends SERVICE_ACCEPT,
 * then loops receiving USERAUTH_REQUEST and dispatching to the
 * appropriate callback.  Sends SUCCESS, FAILURE, PK_OK, or
 * PASSWD_CHANGEREQ as appropriate.
 *
 * On return, *username and *username_len identify the authenticated
 * user (points into session buffer — valid until next recv_packet).
 * Copy if needed.
 *
 * Returns 0 on successful authentication.
 */
DEUCE_SSH_PUBLIC int deuce_ssh_auth_server(deuce_ssh_session sess,
    const struct deuce_ssh_auth_server_cbs *cbs,
    uint8_t *username_out, size_t *username_out_len);

/* ================================================================
 * Client-side authentication
 * ================================================================ */

/*
 * Authenticate with a public key (RFC 4252 s7).
 * Uses the key loaded via ssh_ed25519_load_key_file(),
 * rsa_sha2_256_load_key_file(), etc., on the registered algorithm entry.
 * The algo_name must match a registered key algorithm that has a key
 * loaded (e.g., "ssh-ed25519", "rsa-sha2-256").
 * Returns 0 on success, negative on failure or rejection.
 */
DEUCE_SSH_PUBLIC int deuce_ssh_auth_publickey(deuce_ssh_session sess,
    const char *username, const char *algo_name);

#endif
