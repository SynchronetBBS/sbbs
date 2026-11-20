// RFC 4252: SSH Authentication Protocol

#ifndef DSSH_AUTH_H
#define DSSH_AUTH_H

#include "deucessh.h"
#include "deucessh-result.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SSH_MSG_USERAUTH_REQUEST UINT8_C(50)
#define SSH_MSG_USERAUTH_FAILURE UINT8_C(51)
#define SSH_MSG_USERAUTH_SUCCESS UINT8_C(52)
#define SSH_MSG_USERAUTH_BANNER  UINT8_C(53)

/* RFC 4252 s7, RFC 4256 s5: message 60 is reused across auth methods.
 * The meaning depends on which method is in progress (publickey,
 * password, or keyboard-interactive). */
#define SSH_MSG_USERAUTH_PK_OK            UINT8_C(60)
#define SSH_MSG_USERAUTH_PASSWD_CHANGEREQ UINT8_C(60)
#define SSH_MSG_USERAUTH_INFO_REQUEST     UINT8_C(60)
#define SSH_MSG_USERAUTH_INFO_RESPONSE    UINT8_C(61)

struct dssh_auth_responses {
	const struct dssh_bytes *items;
	uint32_t count;
	struct dssh_result_lease lease;
};

struct dssh_auth_prompt {
	struct dssh_bytes text;
	bool echo;
};

struct dssh_auth_challenge {
	struct dssh_bytes name;
	struct dssh_bytes instruction;
	const struct dssh_auth_prompt *prompts;
	uint32_t count;
	struct dssh_result_lease lease;
};

/*
 * Keyboard-interactive prompt callback (RFC 4256).
 * Called once per INFO_REQUEST with the name, instruction, and
 * individual prompts. out starts zeroed. Set out->count to num_prompts,
 * out->items to a producer-owned array of byte spans, and its release lease.
 * Each item is one response; empty responses are permitted. A zero-prompt
 * request has count zero and still causes an INFO_RESPONSE to be sent.
 * Input strings and prompt arrays are borrowed for this callback only.
 * Returned storage must remain valid until release and must not alias inputs
 * whose lifetime ends on callback return. The library releases the single
 * bundle lease on every outcome, without clearing/freeing returned storage.
 * The producer clears actual owned secret capacity before release/reuse.
 * Lengths and aggregate wire size must fit the session packet limit. Validate
 * counts/metadata before reading; malformed results still receive release.
 *
 * name/name_len:    server-provided name string (may be empty)
 * instruction/instruction_len: server-provided instruction (may be empty)
 * num_prompts:      number of prompts
 * prompts[i]/prompt_lens[i]: the prompt text for each prompt
 * echo[i]:          true if the response should be echoed (not a password)
 * out:             OUT -- response spans, count and producer release lease
 * cbdata:           opaque pointer from the caller
 *
 * Return 0 to continue authentication, negative to abort.
 * Positive return values are invalid.
 */
typedef int (*dssh_auth_kbi_prompt_cb)(const uint8_t *name, size_t name_len, const uint8_t *instruction,
    size_t instruction_len, uint32_t num_prompts, const uint8_t **prompts, const size_t *prompt_lens,
    const bool *echo, struct dssh_auth_responses *out, void *cbdata);

/* dssh_auth_banner_cb is defined in deucessh.h */

/* Client authentication continuation; server callback outcomes are unchanged.
 * Values 2 and 3 are the server-only password-change/KBI-prompt outcomes. */
#define DSSH_AUTH_CONTINUE 4

/*
 * Common client authentication outcomes for begin/password/publickey/KBI:
 *   DSSH_AUTH_SUCCESS   authentication complete
 *   DSSH_AUTH_PARTIAL   factor accepted; more authentication is required
 *   DSSH_AUTH_CONTINUE  incomplete; no factor accepted by this action
 *   negative DSSH_ERROR_* operation error
 * Each complete FAILURE publishes its current continuation list; SUCCESS
 * publishes an empty list and completes authentication. Intermediate messages
 * and malformed/local/transport errors do not replace the previous snapshot.
 * Credential methods do not implicitly perform begin. Requesting the user
 * authentication service automatically remains separate from discovery.
 */

/* Start authentication without credentials. May complete authentication or
 * discover the methods needed to continue. Returns the common client outcomes
 * above, without attempting another method. Optional; applications may instead
 * start directly with password/publickey/KBI. */
DSSH_PUBLIC int dssh_auth_begin(dssh_session sess, const char *username) DSSH_ABI(dssh_auth_begin);

/* Read the methods from the last complete authentication response. Pure
 * getter: no I/O and no change to authentication state. Returns required bytes
 * including NUL; NULL methods or zero methods_sz queries size. A short buffer
 * returns DSSH_ERROR_TOOLONG without modifying it. Before any completed reply,
 * returns DSSH_ERROR_INIT without starting discovery. An empty list needs one
 * byte and does not itself imply completed authentication; use the action's
 * outcome. The snapshot belongs to the last completed exchange, not a failed
 * subsequent action. Normal callback/pre-start serialization rules apply. */
DSSH_PUBLIC int64_t dssh_auth_get_methods(dssh_session sess, char *methods, size_t methods_sz) DSSH_ABI(dssh_auth_get_methods);

/*
 * Password change callback (RFC 4252 s8).
 * Called when the server requests a password change
 * (SSH_MSG_USERAUTH_PASSWD_CHANGEREQ).
 *
 * prompt/prompt_len: the server's prompt text (UTF-8, not NUL-terminated).
 * language/language_len: the language tag (may be empty).
 * out: zero-initialized producer-owned new password and its release lease.
 *      The library borrows the bytes until it has copied the request, then
 *      releases the lease on every outcome, including callback/length errors.
 *      The producer clears its actual owned capacity before releasing/reusing
 *      storage. Length must fit the session packet limit. Empty passwords are
 *      permitted. Input strings are borrowed only during this callback.
 * cbdata: opaque pointer from the caller.
 *
 * Return 0 to send the new password, negative to abort authentication.
 * Positive return values are invalid.
 */
typedef int (*dssh_auth_passwd_change_cb)(const uint8_t *prompt, size_t prompt_len, const uint8_t *language,
    size_t language_len, struct dssh_bytes_result *out, void *cbdata);

/*
 * Authenticate with a password (RFC 4252 s8).
 * If passwd_change_cb is not NULL and the server requests a password
 * change, the callback is invoked to get the new password.
 * If passwd_change_cb is NULL and the server requests a change,
 * authentication fails with DSSH_ERROR_INIT (missing prerequisite).
 * Uses the common client authentication outcomes above. A refused credential
 * attempt returns DSSH_AUTH_CONTINUE; operation errors are negative.
 */
DSSH_PUBLIC int dssh_auth_password(dssh_session sess, const char *username, const char *password,
    dssh_auth_passwd_change_cb passwd_change_cb, void *passwd_change_cbdata) DSSH_ABI(dssh_auth_password);

/*
 * Authenticate via keyboard-interactive (RFC 4256).
 * For each INFO_REQUEST, the prompt_cb is called with the server's
 * name, instruction, and prompts.  The callback provides responses.
 * Uses the common client authentication outcomes above. A refused credential
 * attempt returns DSSH_AUTH_CONTINUE; operation errors are negative.
 */
DSSH_PUBLIC int dssh_auth_keyboard_interactive(dssh_session sess, const char *username,
    dssh_auth_kbi_prompt_cb prompt_cb, void *cbdata) DSSH_ABI(dssh_auth_keyboard_interactive);

/* ================================================================
 * Server-side authentication
 * ================================================================ */

/*
 * Server auth callback results.
 * Return from the verify callbacks to indicate the outcome.
 * Username, credential and response inputs are borrowed for the callback
 * only. Retain a copy if the application needs them after it returns.
 */
#define DSSH_AUTH_SUCCESS         0  /* User authenticated */
#define DSSH_AUTH_FAILURE         -1 /* Wrong credentials */
#define DSSH_AUTH_DISCONNECT      -2 /* Reject and disconnect (e.g. too many attempts) */
#define DSSH_AUTH_PARTIAL         1  /* Succeeded, but more auth needed */
#define DSSH_AUTH_CHANGE_PASSWORD 2  /* Password expired (password method only) */
#define DSSH_AUTH_KBI_PROMPT      3  /* Send INFO_REQUEST with prompts (KBI only) */

/*
 * Server-side password verification callback.
 * Return DSSH_AUTH_SUCCESS, DSSH_AUTH_FAILURE,
 * DSSH_AUTH_PARTIAL, or DSSH_AUTH_CHANGE_PASSWORD.
 *
 * When DSSH_AUTH_CHANGE_PASSWORD is returned, set change_prompt->bytes
 * to the prompt and provide its producer-owned release lease. The prompt
 * pointer must be nonnull; a zero-length prompt is valid. The result starts
 * zeroed and its lease is released on every outcome, including other auth
 * results and invalid output. The library never clears or frees its storage.
 * The library will send PASSWD_CHANGEREQ and call passwd_change_cb
 * (below) when the client responds.
 * Input credentials are borrowed only for this call and cannot back the prompt.
 */
typedef int (*dssh_auth_server_password_cb)(const uint8_t *username, size_t username_len, const uint8_t *password,
    size_t password_len, struct dssh_bytes_result *change_prompt, void *cbdata);

/*
 * Server-side password change callback.
 * Called when the client sends a password change request (boolean TRUE).
 * Validate the old password and set the new one.
 * May be configured independently of the normal password callback.
 * Return DSSH_AUTH_SUCCESS, DSSH_AUTH_FAILURE, or
 * DSSH_AUTH_CHANGE_PASSWORD (to re-prompt). change_prompt uses the same
 * producer-owned result/release contract as the normal password callback.
 */
typedef int (*dssh_auth_server_passwd_change_cb)(const uint8_t *username, size_t username_len,
    const uint8_t *old_password, size_t old_password_len, const uint8_t *new_password, size_t new_password_len,
    struct dssh_bytes_result *change_prompt, void *cbdata);

/*
 * Server-side public key verification callback.
 * Verify that the given public key is authorized for the user.
 * Return DSSH_AUTH_SUCCESS if the key is acceptable,
 * DSSH_AUTH_FAILURE if not, DSSH_AUTH_PARTIAL if more
 * auth is needed.
 *
 * When has_signature is false, this is a key probe (PK_OK query) --
 * return SUCCESS to indicate the key is acceptable (the library
 * sends PK_OK), FAILURE to reject.
 *
 * When has_signature is true, the library has already verified the
 * signature.  This callback only needs to check authorization.
 */
typedef int (*dssh_auth_server_publickey_cb)(const uint8_t *username, size_t username_len, const char *algo_name,
    const uint8_t *pubkey_blob, size_t pubkey_blob_len, bool has_signature, void *cbdata);

/*
 * Server-side "none" auth callback.
 * Return DSSH_AUTH_SUCCESS to grant access without credentials.
 * Return DSSH_AUTH_FAILURE to require authentication.
 */
typedef int (*dssh_auth_server_none_cb)(const uint8_t *username, size_t username_len, void *cbdata);

/*
 * Called once for each USERAUTH_REQUEST after the username/service/method
 * prefix is validated, before method-specific handling. This includes none,
 * unsupported methods and a request replacing an interactive attempt.
 * All byte spans are borrowed only for this callback.
 *
 * new_identity is true for the first request and whenever the complete
 * username differs from the preceding request. On true, clear accumulated
 * authentication factors for the old identity. On every invocation, retire
 * the previous attempt's dialogue state and prepare the current attempt.
 * Persistent account/password changes are not rolled back.
 *
 * Return 0 to continue method processing; this does not authenticate the
 * user. A negative DSSH_ERROR_* ends the SSH session, and dssh_auth_server
 * returns DSSH_ERROR_TERMINATED. A positive result is invalid and also ends
 * the session. Session termination requested by the callback prevents method
 * dispatch. The normal authentication-callback restrictions apply.
 */
typedef int (*dssh_auth_server_request_cb)(const uint8_t *username, size_t username_len,
    const uint8_t *method, size_t method_len, bool new_identity, void *cbdata);

/*
 * Server-side keyboard-interactive callback (RFC 4256).
 *
 * Called in a loop. Use request_cb to reset attempt-local state on each new
 * request when initial and zero-response rounds must be distinguished.
 * On the first call, num_responses is 0 and
 * responses is NULL -- the callback should provide initial prompts.
 * On subsequent calls, responses contains the client's answers to
 * the previous prompts.
 *
 * Return values:
 *   DSSH_AUTH_KBI_PROMPT -- send INFO_REQUEST with the provided prompts
 *   DSSH_AUTH_SUCCESS    -- authentication succeeded
 *   DSSH_AUTH_FAILURE    -- authentication failed
 *   DSSH_AUTH_PARTIAL    -- partial success, more auth needed
 *
 * out starts zeroed. When returning DSSH_AUTH_KBI_PROMPT, provide name and
 * instruction spans (either may be empty), the count and a producer-owned
 * prompt array. Each prompt has an explicit text span and echo flag. Provide
 * one release lease for the entire bundle, including static storage (a no-op
 * release is valid). Returned spans do not require NUL terminators.
 *
 * All output remains producer-owned until release after the final read.
 * The library releases a present lease on every callback/auth outcome and
 * downstream failure, without clearing/freeing the array or bytes. Partial
 * resources on error must either be released by the producer or published
 * through the lease. Prompt texts must be nonempty. A zero prompt count is
 * valid and still requires a client response; track rounds in cbdata, since
 * a zero-response round can also have responses == NULL. Input credentials
 * and arrays remain borrowed only for the callback and cannot back outputs.
 */
typedef int (*dssh_auth_server_kbi_cb)(const uint8_t *username, size_t username_len, uint32_t num_responses,
    const uint8_t **responses, const size_t *response_lens,
    struct dssh_auth_challenge *out, void *cbdata);

/*
 * Server-side authentication callbacks and configuration.
 * Set the callbacks you support; leave others NULL.
 * methods_str is a comma-separated list of supported method names
 * (e.g., "publickey,password,keyboard-interactive") sent in FAILURE
 * responses. Each name must be nonempty printable ASCII without commas
 * or spaces, at most 64 bytes. The reserved "none" query is not listed.
 * An empty string advertises no methods.
 */
struct dssh_auth_server_cbs {
	const char                       *methods_str;
	dssh_auth_server_request_cb       request_cb;
	dssh_auth_server_none_cb          none_cb;
	dssh_auth_server_password_cb      password_cb;
	dssh_auth_server_passwd_change_cb passwd_change_cb;
	dssh_auth_server_publickey_cb     publickey_cb;
	dssh_auth_server_kbi_cb           keyboard_interactive_cb;
	void                             *cbdata;
};

/*
 * Run the server-side authentication loop (RFC 4252).
 * Receives SSH_MSG_SERVICE_REQUEST, sends SERVICE_ACCEPT once per session,
 * then loops receiving USERAUTH_REQUEST and dispatching to the
 * appropriate callback.  Sends SUCCESS, FAILURE, PK_OK, or
 * PASSWD_CHANGEREQ as appropriate.
 *
 * A NULL request_cb binds authentication to the first request's complete
 * username. A later change disconnects and returns DSSH_ERROR_TERMINATED.
 * An installed request_cb permits a change only after it reports a complete
 * reset. A missing none_cb still produces the ordinary automatic failure;
 * neither a request callback nor a none callback is required for that probe.
 * methods_str updates made by request_cb are validated before method handling.
 * Only ssh-connection is supported; requesting another service disconnects.
 *
 * username_out_len is in/out: on input *username_out_len is the
 * buffer capacity; on output it is the number of bytes written.
 * The output copy is limited to 255 bytes and the supplied capacity.
 * Method callbacks receive the full request username; identity comparisons
 * likewise use its full length, independently of this output copy.
 * Pass NULL for both username_out and username_out_len to skip
 * username output.
 *
 * Returns 0 on successful authentication.
 */
DSSH_PUBLIC int dssh_auth_server(dssh_session sess, const struct dssh_auth_server_cbs *cbs, uint8_t *username_out,
    size_t *username_out_len) DSSH_ABI(dssh_auth_server);

/*
 * Set a pending banner to be sent before the next auth response.
 * message and language are copied. A failed replacement preserves the
 * existing pending banner. Pass NULL for message to cancel a pending banner.
 * language may be NULL (defaults to empty string per RFC 4252 s5.4).
 * Can be called before dssh_auth_server() or from within auth callbacks.
 */
DSSH_PUBLIC int dssh_auth_set_banner(dssh_session sess, const char *message, const char *language) DSSH_ABI(dssh_auth_set_banner);

/* ================================================================
 * Client-side authentication
 * ================================================================ */
/*
 * Authenticate with a public key (RFC 4252 s7).
 * Uses this session's key for algo_name, falling back to the registered global
 * key only when no local key is installed. A rejected/failed local key does
 * not cause an implicit global-key attempt. Load local credentials with
 * dssh_session_load_client_key_file() or install a module context with
 * dssh_session_set_client_key_ctx(). algo_name must match a registered signing
 * algorithm (e.g., "ssh-ed25519", "rsa-sha2-256"). The application selects
 * method/key order through explicit authentication calls; there is no implicit
 * none probe. It can replace/clear a local key between unsuccessful attempts.
 * Uses the common client authentication outcomes above. A refused credential
 * attempt returns DSSH_AUTH_CONTINUE; operation errors are negative.
 */
DSSH_PUBLIC int dssh_auth_publickey(dssh_session sess, const char *username, const char *algo_name) DSSH_ABI(dssh_auth_publickey);

#ifdef __cplusplus
}
#endif

#endif   // ifndef DSSH_AUTH_H
