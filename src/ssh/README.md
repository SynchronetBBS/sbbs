# DeuceSSH

A from-scratch SSH library in standard C17 implementing the core SSH protocol:

- RFC 4251 — SSH Protocol Architecture (data types)
- RFC 4252 — SSH Authentication Protocol
- RFC 4253 — SSH Transport Layer Protocol
- RFC 4254 — SSH Connection Protocol
- RFC 4256 — Generic Message Exchange Authentication (keyboard-interactive)
- RFC 4335 — SSH Session Channel Break Extension
- RFC 4419 — Diffie-Hellman Group Exchange
- RFC 6668 — SHA-2 Data Integrity Verification
- RFC 8160 — IUTF8 Terminal Mode
- RFC 8308 — Extension Negotiation
- RFC 8332 — rsa-sha2-256 Host Key Algorithm
- RFC 8709 — Ed25519 Host Key Algorithm
- RFC 8731 — curve25519-sha256 Key Exchange
- draft-ietf-sshm-ntruprime-ssh — sntrup761x25519-sha512 Key Exchange
- draft-ietf-sshm-mlkem-hybrid-kex — mlkem768x25519-sha256 Key Exchange

No proprietary extensions (no `@` algorithm names).

## Dependencies

- **OpenSSL 3.0+** (libcrypto only — no libssl)
- **C11 threads** (`<threads.h>`, `<stdatomic.h>`) — used internally only, not exposed in public headers

## Building

```sh
cmake -S . -B build
cmake --build build -j8
```

Produces `libdeucessh.a` (static) and `libdeucessh.so` (shared).

Use `-S . -B build` to prevent cmake from discovering the parent
`src/CMakeLists.txt`.

## Built-in Algorithms

| Category | Algorithms |
|----------|-----------|
| Key Exchange | `mlkem768x25519-sha256`, `sntrup761x25519-sha512`, `curve25519-sha256`, `diffie-hellman-group-exchange-sha256` |
| Host Key | `ssh-ed25519`, `rsa-sha2-256` |
| Encryption | `aes256-ctr`, `none` |
| MAC | `hmac-sha2-256`, `none` |
| Compression | `none` |

All algorithms support both client and server side.

## Quick Start — Client

```c
#include "deucessh.h"
#include "deucessh-auth.h"
#include "deucessh-conn.h"
#include "deucessh-algorithms.h"

/* 1. Implement I/O callbacks */
static int my_tx(uint8_t *buf, size_t bufsz,
    dssh_session sess, void *cbdata)
{
    /* Send exactly bufsz bytes over your transport (socket, etc.).
     * Block until complete or dssh_session_is_terminated(sess).
     * Return 0 on success, negative on error. */
}

static int my_rx(uint8_t *buf, size_t bufsz,
    dssh_session sess, void *cbdata)
{
    /* Receive exactly bufsz bytes.  Same contract as tx. */
}

int main(void)
{
    /* 2. Register algorithms (order = preference) */
    dssh_transport_set_callbacks(my_tx, my_rx, NULL, NULL);
    dssh_register_curve25519_sha256();
    dssh_register_ssh_ed25519();
    dssh_register_aes256_ctr();
    dssh_register_none_enc();
    dssh_register_hmac_sha2_256();
    dssh_register_none_mac();
    dssh_register_none_comp();

    /* 3. Create session (opaque handle) */
    dssh_session sess = dssh_session_init(true, 0);
    dssh_session_set_terminate_cb(sess, on_terminate, NULL);

    /* 4. Handshake */
    dssh_transport_handshake(sess);

    /* 5. Authenticate */
    char methods[256];
    int r = dssh_auth_get_methods(sess, "user", methods, sizeof(methods));
    if (r > 0) {
        if (strstr(methods, "password"))
            dssh_auth_password(sess, "user", "pass", NULL, NULL);
        else if (strstr(methods, "keyboard-interactive"))
            dssh_auth_keyboard_interactive(sess, "user",
                my_kbi_prompt, my_context);
    }

    /* 6. Start the demux thread */
    dssh_session_start(sess);

    /* 7. Open channel via params builder */
    struct dssh_chan_params cp;
    dssh_chan_params_init(&cp, DSSH_CHAN_EXEC);
    dssh_chan_params_set_command(&cp, "echo hello");
    dssh_chan_params_set_pty(&cp, false);

    dssh_channel ch = dssh_chan_open(sess, &cp);
    dssh_chan_params_free(&cp);

    /* 8. Read output using poll/read */
    uint8_t buf[4096];
    int ev;
    while ((ev = dssh_chan_poll(ch,
            DSSH_POLL_READ | DSSH_POLL_EVENT, -1)) > 0) {
        if (ev & DSSH_POLL_EVENT) {
            struct dssh_chan_event event;
            dssh_chan_read_event(ch, &event);
            continue;
        }
        if (ev & DSSH_POLL_READ) {
            int64_t n = dssh_chan_read(ch, 0, buf, sizeof(buf));
            if (n <= 0)
                break;
            write(STDOUT_FILENO, buf, n);
        }
    }

    /* 9. Clean up */
    dssh_chan_close(ch, 0);
    dssh_session_cleanup(sess);
}
```

For an interactive shell instead of exec:

```c
struct dssh_chan_params cp;
dssh_chan_params_init(&cp, DSSH_CHAN_SHELL);
dssh_chan_params_set_term(&cp, "xterm-256color");
dssh_chan_params_set_size(&cp, 80, 24, 0, 0);
/* PTY is enabled by default for DSSH_CHAN_SHELL */

dssh_channel ch = dssh_chan_open(sess, &cp);
dssh_chan_params_free(&cp);
```

## Quick Start — Server

```c
#include "deucessh.h"
#include "deucessh-auth.h"
#include "deucessh-conn.h"
#include "deucessh-algorithms.h"

/* Server-side auth: accept any password */
static int my_password_cb(const uint8_t *username, size_t username_len,
    const uint8_t *password, size_t password_len,
    uint8_t **change_prompt, size_t *change_prompt_len, void *cbdata)
{
    return DSSH_AUTH_SUCCESS;
}

/* Channel accept callbacks */
static int my_shell_cb(dssh_channel ch,
    const struct dssh_chan_params *params,
    struct dssh_chan_accept_result *result, void *cbdata)
{
    return 0;  /* accept */
}

static int my_exec_cb(dssh_channel ch,
    const struct dssh_chan_params *params,
    struct dssh_chan_accept_result *result, void *cbdata)
{
    return 0;  /* accept */
}

int main(void)
{
    /* 1. Register algorithms and I/O callbacks (same as client) */
    dssh_transport_set_callbacks(my_tx, my_rx, NULL, NULL);
    dssh_register_curve25519_sha256();
    dssh_register_ssh_ed25519();
    dssh_register_rsa_sha2_256();
    dssh_register_aes256_ctr();
    dssh_register_hmac_sha2_256();
    dssh_register_none_comp();

    /* 2. Load host keys (stored on algorithm entry, not session) */
    dssh_ed25519_load_key_file("/path/to/host_ed25519.pem", NULL, NULL);
    dssh_rsa_sha2_256_load_key_file("/path/to/host_rsa.pem", NULL, NULL);

    /* 3. Create session */
    dssh_session sess = dssh_session_init(false, 0);
    dssh_session_set_terminate_cb(sess, on_terminate, NULL);

    /* 4. Handshake */
    dssh_transport_handshake(sess);

    /* 5. Authenticate */
    struct dssh_auth_server_cbs auth_cbs = {
        .methods_str = "publickey,password",
        .password_cb = my_password_cb,
    };
    uint8_t username[256];
    size_t username_len = sizeof(username);
    dssh_auth_set_banner(sess, "Welcome!\r\n", NULL);
    dssh_auth_server(sess, &auth_cbs, username, &username_len);

    /* 6. Start demux thread */
    dssh_session_start(sess);

    /* 7. Accept incoming channels */
    struct dssh_chan_accept_cbs accept_cbs = {
        .shell = my_shell_cb,
        .exec = my_exec_cb,
    };

    dssh_channel ch = dssh_chan_accept(sess, &accept_cbs, -1);
    if (ch == NULL)
        return 1;

    enum dssh_chan_type ctype = dssh_chan_get_type(ch);

    if (ctype == DSSH_CHAN_EXEC) {
        const char *cmd = dssh_chan_get_command(ch);
        /* ... handle exec ... */
        dssh_chan_write(ch, 0,
            (const uint8_t *)"done\n", 5);
    } else {
        /* Shell — echo loop */
        uint8_t buf[4096];
        for (;;) {
            int ev = dssh_chan_poll(ch, DSSH_POLL_READ, -1);
            if (ev <= 0)
                break;
            int64_t n = dssh_chan_read(ch, 0, buf, sizeof(buf));
            if (n <= 0)
                break;
            dssh_chan_write(ch, 0, buf, n);
        }
    }

    /* 8. Clean up */
    dssh_chan_close(ch, 0);
    dssh_session_cleanup(sess);
}
```

## I/O Callbacks

The library does no I/O itself.  You provide two required callbacks
and one optional:

| Callback | Purpose |
|----------|---------|
| `tx` | Send exactly N bytes.  Block until done or `dssh_session_is_terminated(sess)`. |
| `rx` | Receive exactly N bytes.  Same contract as tx. |
| `rx_line` | Receive until CR-LF (version exchange only).  **Optional** — pass NULL to use a built-in default that calls `rx` one byte at a time. |

All callbacks receive the session handle and a `cbdata` pointer.
Return 0 on success, negative on error.  The `cbdata` is set via
`dssh_session_set_cbdata()`.

Callbacks are registered globally once.  Per-session differentiation
is via the `cbdata` pointers.

An optional fourth callback (`extra_line_cb`) is called for non-SSH
lines received before the version string during version exchange.

**Important:** When `dssh_session_is_terminated(sess)` returns true,
I/O callbacks **must** return a negative error code within a reasonable
period.  Failure to do so will cause `dssh_session_cleanup()` to hang
on thread join.  Use the terminate callback to be notified:

```c
static int my_sock;

static void on_terminate(dssh_session sess, void *cbdata)
{
    shutdown(my_sock, SHUT_RDWR);  /* unblock pending recv/send */
}

/* Register before dssh_session_start(): */
dssh_session_set_terminate_cb(sess, on_terminate, NULL);
```

The terminate callback fires exactly once (fatal error, peer disconnect,
or explicit `dssh_session_terminate()` call), from any thread, before
library condvar broadcasts.  It must not call any `dssh_*` function on
this session except `dssh_session_is_terminated()`.

## Version String

Customize the SSH identification string (RFC 4253 s4.2):

```c
dssh_transport_set_version("MyApp-1.0", "FreeBSD");
/* Produces: SSH-2.0-MyApp-1.0 FreeBSD\r\n */
```

Must be called before `dssh_session_init()`.  Pass NULL for
`software_version` to keep the library default; pass NULL for
`comment` to omit it.  The resulting line must fit in 255 bytes.

## Session Termination

```c
/* Graceful disconnect (sends SSH_MSG_DISCONNECT to peer): */
dssh_transport_disconnect(sess,
    SSH_DISCONNECT_BY_APPLICATION, "goodbye");

/* Non-graceful (just sets terminate flag, no wire message): */
dssh_session_terminate(sess);

/* Query: */
if (dssh_session_is_terminated(sess)) { /* ... */ }
```

`dssh_transport_disconnect()` sends a disconnect message (best-effort)
and sets the terminate flag.  `dssh_session_terminate()` sets the flag
without sending anything.

## Negotiated Algorithm Queries

After a successful handshake, query the negotiated algorithms and
peer version string.  Returns NULL if not yet negotiated:

```c
const char *peer = dssh_transport_get_remote_version(sess);
const char *kex  = dssh_transport_get_kex_name(sess);
const char *hk   = dssh_transport_get_hostkey_name(sess);
const char *enc  = dssh_transport_get_enc_name(sess);
const char *mac  = dssh_transport_get_mac_name(sess);
```

The returned strings point into internal state and remain valid
until the session is cleaned up (or until the next rekey, which
updates them atomically).

## Secure Memory

`dssh_cleanse()` scrubs a buffer to remove sensitive data (passwords,
keys).  It resists compiler dead-store optimization and does not
require the application to link against OpenSSL.

```c
char password[64];
/* ... fill password ... */
dssh_auth_password(sess, "user", password, NULL, NULL);
dssh_cleanse(password, strlen(password));
```

Do not `realloc()` password buffers — the old allocation won't be
scrubbed.

## Authentication — Client Side

### Password

```c
/* Simple password auth (no password change support): */
dssh_auth_password(sess, "user", "pass", NULL, NULL);

/* With password change support: */
dssh_auth_password(sess, "user", "pass",
    my_passwd_change_cb, my_context);
```

If the server sends `SSH_MSG_USERAUTH_PASSWD_CHANGEREQ`, the callback
receives the prompt and returns the new password:

```c
static int
my_passwd_change(const uint8_t *prompt, size_t prompt_len,
    const uint8_t *language, size_t language_len,
    uint8_t **new_password, size_t *new_password_len,
    void *cbdata)
{
    printf("%.*s", (int)prompt_len, prompt);
    char buf[256];
    fgets(buf, sizeof(buf), stdin);
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
        len--;

    *new_password = malloc(len);
    if (*new_password == NULL)
        return DSSH_ERROR_ALLOC;
    memcpy(*new_password, buf, len);
    *new_password_len = len;
    return 0;
}
```

### Public Key

```c
#include "deucessh-algorithms.h"

/* Load the private key (stored on the algorithm entry) */
dssh_ed25519_load_key_file("/path/to/id_ed25519", NULL, NULL);

/* Authenticate */
dssh_auth_publickey(sess, "user", "ssh-ed25519");
```

The function signs the session-bound authentication data (session ID
+ request fields) per RFC 4252 s7 and sends the signed request.
Works with any registered key algorithm that supports signing.

### Keyboard-Interactive

`dssh_auth_keyboard_interactive()` uses a callback to handle
server prompts (RFC 4256).  The callback receives the server's name
and instruction strings, plus an array of prompts with echo flags,
and must fill in the responses:

```c
static int
my_kbi_prompt(const uint8_t *name, size_t name_len,
    const uint8_t *instruction, size_t instruction_len,
    uint32_t num_prompts,
    const uint8_t **prompts, const size_t *prompt_lens,
    const bool *echo,
    uint8_t **responses, size_t *response_lens,
    void *cbdata)
{
    for (uint32_t i = 0; i < num_prompts; i++) {
        /* Display the prompt (prompt text is not NUL-terminated) */
        printf("%.*s", (int)prompt_lens[i], prompts[i]);

        /* Read the response (echo[i] indicates whether to echo) */
        char buf[256];
        fgets(buf, sizeof(buf), stdin);
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n')
            len--;

        /* Each response must be malloc'd — the library frees it */
        responses[i] = malloc(len);
        if (responses[i] == NULL)
            return DSSH_ERROR_ALLOC;
        memcpy(responses[i], buf, len);
        response_lens[i] = len;
    }
    return 0;
}

/* Usage: */
dssh_auth_keyboard_interactive(sess, "user",
    my_kbi_prompt, my_context);
```

## Authentication — Server Side

`dssh_auth_server()` drives the server-side authentication loop.
It handles SERVICE_REQUEST/ACCEPT, receives USERAUTH_REQUEST messages,
dispatches to your callbacks, and sends SUCCESS/FAILURE/CHANGEREQ/PK_OK
responses.

```c
struct dssh_auth_server_cbs cbs = {
    .methods_str = "publickey,password,keyboard-interactive",
    .password_cb = my_password_cb,
    .passwd_change_cb = my_passwd_change_cb,  /* optional */
    .publickey_cb = my_pubkey_cb,             /* optional */
    .keyboard_interactive_cb = my_kbi_cb,     /* optional */
    .none_cb = my_none_cb,                    /* optional */
    .cbdata = my_server_context,
};
uint8_t username[256];
size_t username_len = sizeof(username);

/* Optional: set a banner shown before the first auth exchange */
dssh_auth_set_banner(sess, "Welcome to my server!\r\n", NULL);

int r = dssh_auth_server(sess, &cbs, username, &username_len);
```

### Callback Return Values

| Value | Meaning |
|-------|---------|
| `DSSH_AUTH_SUCCESS` | User authenticated — library sends SUCCESS |
| `DSSH_AUTH_FAILURE` | Rejected — library sends FAILURE |
| `DSSH_AUTH_PARTIAL` | Succeeded, but more auth needed — library sends FAILURE with partial_success=TRUE |
| `DSSH_AUTH_DISCONNECT` | Reject and disconnect (too many attempts, etc.) |
| `DSSH_AUTH_CHANGE_PASSWORD` | Password expired — library sends PASSWD_CHANGEREQ (password method only) |
| `DSSH_AUTH_KBI_PROMPT` | Send INFO_REQUEST with prompts (keyboard-interactive only) |

### Banner API

Set a pending banner at any time during auth.  It will be sent
before the next SUCCESS, FAILURE, or INFO_REQUEST.  Callbacks can
set new banners dynamically.

```c
dssh_auth_set_banner(sess, "Message of the day\r\n", NULL);
/* language tag (2nd arg) may be NULL (defaults to empty) */
dssh_auth_set_banner(sess, NULL, NULL);  /* cancel pending banner */
```

### Password Callback

```c
static int
my_password_cb(const uint8_t *username, size_t username_len,
    const uint8_t *password, size_t password_len,
    uint8_t **change_prompt, size_t *change_prompt_len,
    void *cbdata)
{
    /* Validate credentials.
     * To request a password change, set *change_prompt to a malloc'd
     * prompt string and return DSSH_AUTH_CHANGE_PASSWORD. */
    if (check_password(username, username_len, password, password_len))
        return DSSH_AUTH_SUCCESS;
    return DSSH_AUTH_FAILURE;
}
```

### Password Change Callback

```c
static int
my_passwd_change_cb(const uint8_t *username, size_t username_len,
    const uint8_t *old_password, size_t old_password_len,
    const uint8_t *new_password, size_t new_password_len,
    uint8_t **change_prompt, size_t *change_prompt_len,
    void *cbdata)
{
    /* Validate old password and set new one.
     * Return DSSH_AUTH_CHANGE_PASSWORD to re-prompt. */
    if (!check_password(username, username_len,
            old_password, old_password_len))
        return DSSH_AUTH_FAILURE;
    set_password(username, username_len, new_password, new_password_len);
    return DSSH_AUTH_SUCCESS;
}
```

### Public Key Callback

```c
static int
my_pubkey_cb(const uint8_t *username, size_t username_len,
    const char *algo_name,
    const uint8_t *pubkey_blob, size_t pubkey_blob_len,
    bool has_signature, void *cbdata)
{
    /* When has_signature is false, this is a key probe —
     * return SUCCESS if the key is in the user's authorized_keys.
     * When has_signature is true, the library has already verified
     * the signature — just check authorization. */
    if (is_key_authorized(username, username_len,
            pubkey_blob, pubkey_blob_len))
        return DSSH_AUTH_SUCCESS;
    return DSSH_AUTH_FAILURE;
}
```

### Keyboard-Interactive Callback (Server)

Called in a loop.  First call has `num_responses=0` and
`responses=NULL` — provide initial prompts.  Subsequent calls
have the client's answers.  Return `DSSH_AUTH_KBI_PROMPT` to
send more prompts, or a final result to end the exchange.

```c
static int
my_kbi_cb(const uint8_t *username, size_t username_len,
    uint32_t num_responses,
    const uint8_t **responses, const size_t *response_lens,
    char **name_out, char **instruction_out,
    uint32_t *num_prompts_out,
    char ***prompts_out, bool **echo_out,
    void *cbdata)
{
    if (num_responses == 0) {
        /* First call — send a prompt */
        *name_out = strdup("Auth");
        *instruction_out = strdup("Enter your token");
        *num_prompts_out = 1;
        *prompts_out = malloc(sizeof(char *));
        *echo_out = malloc(sizeof(bool));
        (*prompts_out)[0] = strdup("Token: ");
        (*echo_out)[0] = false;
        return DSSH_AUTH_KBI_PROMPT;
    }
    /* Check the response */
    if (num_responses == 1
        && response_lens[0] == 6
        && memcmp(responses[0], "secret", 6) == 0)
        return DSSH_AUTH_SUCCESS;
    return DSSH_AUTH_FAILURE;
}
```

The library frees all out params (`name_out`, `instruction_out`,
`prompts_out[i]`, `prompts_out`, `echo_out`) after sending
INFO_REQUEST.

## Algorithm Registration

Call `dssh_register_*()` functions before any session is created.
Registration order determines negotiation preference — first registered
is most preferred.

```c
/* Order = preference: post-quantum hybrid first, then classical */
dssh_register_mlkem768x25519_sha256();
dssh_register_sntrup761x25519_sha512();
dssh_register_curve25519_sha256();
dssh_register_dh_gex_sha256();
```

You can register your own custom algorithm modules by implementing
the callback signatures defined in the public module headers and
calling the corresponding `dssh_transport_register_*()` function.

### Registration Rules

- Call `dssh_register_*()` before any `dssh_session_init()`.
- After the first session init, registration is locked (`DSSH_ERROR_TOOLATE`).
- Names must be 1–64 printable ASCII characters (no spaces, no control chars).
- The `next` pointer must be NULL (the library manages the linked list).
- Registration order determines negotiation preference.

## Custom Algorithm Modules

You can implement custom encryption, MAC, compression, host key, or
key exchange algorithms by filling in the appropriate struct and
calling the registration function.  Each module type has its own
public header with the struct definitions and registration API.

### Encryption Module

Implement four callbacks and register:

```c
#include "deucessh-enc.h"

static int my_init(const uint8_t *key, const uint8_t *iv,
    bool encrypt, dssh_enc_ctx **ctx)
{
    /* Allocate and initialize context.  encrypt indicates direction.
     * Return 0 on success, negative on error. */
}

static int my_encrypt(uint8_t *buf, size_t bufsz, dssh_enc_ctx *ctx)
{
    /* Encrypt buf in place.  Return 0 on success. */
}

static int my_decrypt(uint8_t *buf, size_t bufsz, dssh_enc_ctx *ctx)
{
    /* Decrypt buf in place.  Return 0 on success. */
}

static void my_cleanup(dssh_enc_ctx *ctx)
{
    /* Free context resources. */
}

int register_my_enc(void)
{
    static const char name[] = "my-cipher";
    struct dssh_enc_s *enc = malloc(sizeof(*enc) + sizeof(name));
    if (enc == NULL) return DSSH_ERROR_ALLOC;
    enc->next = NULL;
    enc->init = my_init;
    enc->encrypt = my_encrypt;
    enc->decrypt = my_decrypt;
    enc->cleanup = my_cleanup;
    enc->flags = 0;
    enc->blocksize = 16;  /* cipher block size in bytes */
    enc->key_size = 32;   /* key size in bytes */
    memcpy(enc->name, name, sizeof(name));
    return dssh_transport_register_enc(enc);
}
```

The `struct dssh_enc_s` (typedef `dssh_enc`) is defined in `deucessh-enc.h`.

### MAC Module

Implement three callbacks:

```c
#include "deucessh-mac.h"

static int my_mac_init(const uint8_t *key, dssh_mac_ctx **ctx)
{
    /* Allocate and initialize MAC context with key.
     * Return 0 on success, negative on error. */
}

static int my_mac_generate(const uint8_t *buf, size_t bufsz,
    uint8_t *outbuf, dssh_mac_ctx *ctx)
{
    /* Compute MAC of buf, write to outbuf.
     * outbuf is always digest_size bytes.
     * Return 0 on success. */
}

static void my_mac_cleanup(dssh_mac_ctx *ctx) { /* ... */ }

int register_my_mac(void)
{
    static const char name[] = "my-mac";
    struct dssh_mac_s *mac = malloc(sizeof(*mac) + sizeof(name));
    if (mac == NULL) return DSSH_ERROR_ALLOC;
    mac->next = NULL;
    mac->init = my_mac_init;
    mac->generate = my_mac_generate;
    mac->cleanup = my_mac_cleanup;
    mac->digest_size = 32;  /* output tag size in bytes */
    mac->key_size = 32;     /* key size in bytes */
    memcpy(mac->name, name, sizeof(name));
    return dssh_transport_register_mac(mac);
}
```

The `struct dssh_mac_s` (typedef `dssh_mac`) is defined in `deucessh-mac.h`.

### Compression Module

```c
#include "deucessh-comp.h"

static int my_compress(uint8_t *buf, size_t *bufsz, dssh_comp_ctx *ctx)
{
    /* Compress buf in place, update *bufsz.  Return 0 on success. */
}

static int my_uncompress(uint8_t *buf, size_t *bufsz, dssh_comp_ctx *ctx)
{
    /* Decompress buf in place, update *bufsz.  Return 0 on success. */
}

static void my_comp_cleanup(dssh_comp_ctx *ctx) { /* ... */ }

int register_my_comp(void)
{
    static const char name[] = "my-comp";
    struct dssh_comp_s *comp = malloc(sizeof(*comp) + sizeof(name));
    if (comp == NULL) return DSSH_ERROR_ALLOC;
    comp->next = NULL;
    comp->compress = my_compress;
    comp->uncompress = my_uncompress;
    comp->cleanup = my_comp_cleanup;
    memcpy(comp->name, name, sizeof(name));
    return dssh_transport_register_comp(comp);
}
```

The `struct dssh_comp_s` (typedef `dssh_comp`) is defined in `deucessh-comp.h`.

### Host Key Module

Host key modules implement sign, verify, public key export, and
optional key management (generate, load, save).  They use the `ctx`
field on the registered struct to store key material.

```c
#include "deucessh-key-algo.h"

static int my_sign(uint8_t **out, size_t *outlen,
    const uint8_t *data, size_t data_len, dssh_key_algo_ctx *ctx)
{
    /* Sign data, malloc the SSH-encoded signature blob into *out.
     * Set *outlen to the blob size.  Caller frees *out.
     * Return 0 on success. */
}

static int my_verify(const uint8_t *key_blob, size_t key_blob_len,
    const uint8_t *sig_blob, size_t sig_blob_len,
    const uint8_t *data, size_t data_len)
{
    /* Verify that sig_blob is a valid signature of data under
     * the public key in key_blob.  Return 0 if valid. */
}

static int my_pubkey(const uint8_t **out, size_t *outlen,
    dssh_key_algo_ctx *ctx)
{
    /* Set *out to the SSH-encoded public key blob (internal pointer,
     * NOT malloc'd -- caller must not free).
     * Set *outlen to the blob size.  Return 0 on success. */
}

static int my_haskey(dssh_key_algo_ctx *ctx)
{
    /* Return true if a private key is loaded. */
}

static void my_key_cleanup(dssh_key_algo_ctx *ctx) { /* ... */ }

int register_my_key_algo(void)
{
    static const char name[] = "my-key-type";
    struct dssh_key_algo_s *ka = malloc(sizeof(*ka) + sizeof(name));
    if (ka == NULL) return DSSH_ERROR_ALLOC;
    ka->next = NULL;
    ka->sign = my_sign;
    ka->verify = my_verify;
    ka->pubkey = my_pubkey;
    ka->haskey = my_haskey;
    ka->cleanup = my_key_cleanup;
    ka->ctx = NULL;     /* set by generate/load */
    ka->flags = DSSH_KEY_ALGO_FLAG_SIGNATURE_CAPABLE;
    memcpy(ka->name, name, sizeof(name));
    return dssh_transport_register_key_algo(ka);
}
```

The `struct dssh_key_algo_s` (typedef `dssh_key_algo`) is defined in
`deucessh-key-algo.h`.  Key blobs and signature blobs use the SSH wire
format defined in the relevant RFC for the algorithm.  Use
`dssh_key_algo_set_ctx()` (from `deucessh-algorithms.h`) to store key
material on the registration entry.

### KEX Module

KEX modules drive the key exchange protocol after KEXINIT negotiation.
A KEX handler receives a `struct dssh_kex_context *` (defined in
`deucessh-kex.h`) containing all inputs, I/O function pointers, and
output fields — no access to session internals.

```c
#include "deucessh-kex.h"

static int my_kex_handler(struct dssh_kex_context *kctx)
{
    /* Inputs available on kctx:
     *   client, v_c/v_c_len, v_s/v_s_len (version strings)
     *   i_c/i_c_len, i_s/i_s_len (KEXINIT payloads)
     *   key_algo (host key sign/verify/pubkey)
     *   kex_data (from kex->ctx, set via dssh_kex_set_ctx())
     *
     * I/O:
     *   kctx->send(payload, len, kctx->io_ctx)
     *   kctx->recv(&msg_type, &payload, &len, kctx->io_ctx)
     *
     * Outputs (malloc'd, caller takes ownership):
     *   kctx->shared_secret / shared_secret_sz
     *   kctx->exchange_hash / exchange_hash_sz
     */
    return 0;
}

static void my_kex_cleanup(void *kex_data)
{
    /* Free any module-specific state. */
}

int register_my_kex(void)
{
    static const char name[] = "my-kex-method";
    struct dssh_kex_s *kex = malloc(sizeof(*kex) + sizeof(name));
    if (kex == NULL) return DSSH_ERROR_ALLOC;
    kex->next = NULL;
    kex->handler = my_kex_handler;
    kex->cleanup = my_kex_cleanup;
    kex->flags = DSSH_KEX_FLAG_NEEDS_SIGNATURE_CAPABLE;
    kex->hash_name = "SHA256";  /* OpenSSL digest name for key derivation */
    memcpy(kex->name, name, sizeof(name));
    return dssh_transport_register_kex(kex);
}
```

The `struct dssh_kex_s` (typedef `dssh_kex`) and `struct dssh_kex_context`
are defined in `deucessh-kex.h`.  KEX modules need no private headers.

## DH Group Exchange (Server)

DH-GEX works out of the box — the built-in default selects from
RFC 3526 groups 14–18 (2048–8192-bit) based on the client's requested
min/preferred/max range.

To override with custom groups (e.g. from a moduli file), include
`kex/dh-gex-sha256.h` and call `dssh_kex_set_ctx()` before
`dssh_session_init()`:

```c
#include "kex/dh-gex-sha256.h"

struct dssh_dh_gex_provider prov = {
    .select_group = my_group_selector,
    .cbdata = NULL,
};
dssh_kex_set_ctx("diffie-hellman-group-exchange-sha256", &prov);
```

The `select_group` callback receives the client's min/preferred/max
bit sizes and returns p and g as big-endian byte arrays (caller frees).

## Key Management

Keys are stored on the global algorithm registration entry, not on
individual sessions.  Load once, use across all sessions.  Multiple
algorithms can have keys loaded simultaneously — the server's KEXINIT
only advertises algorithms that have a key loaded.

Each key algorithm provides these functions:

| Function | Purpose |
|----------|---------|
| `_load_key_file(path, pw_cb, cbdata)` | Load private key from PEM file |
| `_save_key_file(path, pw_cb, cbdata)` | Save private key to PEM file |
| `_save_pub_file(path)` | Save public key in OpenSSH format |
| `_get_pub_str(buf, bufsz)` | Get OpenSSH public key string (0/NULL to query size) |
| `_generate_key(...)` | Generate a new key in memory |

The `pw_cb` parameter is `dssh_pem_password_cb` — pass NULL for
unencrypted PEM, or a callback to encrypt (save) / decrypt (load)
with AES-256-CBC:

```c
/* Passphrase callback: fill buf, return length */
int my_pw_cb(char *buf, int size, int rwflag, void *u)
{
    const char *pass = u;
    int len = strlen(pass);
    if (len > size)
        len = size;
    memcpy(buf, pass, len);
    return len;
}

/* Generate, save encrypted, export public key */
dssh_ed25519_generate_key();
dssh_ed25519_save_key_file("/etc/ssh/host_ed25519",
    my_pw_cb, "passphrase");
dssh_ed25519_save_pub_file("/etc/ssh/host_ed25519.pub");

/* Load encrypted key */
dssh_ed25519_load_key_file("/etc/ssh/host_ed25519",
    my_pw_cb, "passphrase");

/* Get public key string at runtime */
int64_t len = dssh_ed25519_get_pub_str(NULL, 0);
char *pub = malloc(len);
dssh_ed25519_get_pub_str(pub, len);  /* "ssh-ed25519 AAAA..." */
```

## Channel Params Builder

Channel parameters are configured via `struct dssh_chan_params`,
populated through a builder API.  The params are consumed at
`dssh_chan_open()` time — call `dssh_chan_params_free()` after open.

```c
struct dssh_chan_params cp;

/* Initialize with type — sets defaults */
dssh_chan_params_init(&cp, DSSH_CHAN_SHELL);

/* Configure */
dssh_chan_params_set_term(&cp, "xterm-256color");
dssh_chan_params_set_size(&cp, 80, 24, 0, 0);
dssh_chan_params_set_mode(&cp, 53, 1);    /* ECHO = 1 */
dssh_chan_params_set_max_window(&cp, 4 * 1024 * 1024);  /* 4 MiB */
dssh_chan_params_add_env(&cp, "LANG", "en_US.UTF-8");

/* Open and free */
dssh_channel ch = dssh_chan_open(sess, &cp);
dssh_chan_params_free(&cp);
```

### Builder Functions

| Function | Purpose |
|----------|---------|
| `dssh_chan_params_init(p, type)` | Set type and defaults |
| `dssh_chan_params_free(p)` | Release heap storage |
| `dssh_chan_params_set_max_window(p, max)` | Max window size (0 = 2 MiB default) |
| `dssh_chan_params_set_pty(p, enable)` | Enable/disable PTY |
| `dssh_chan_params_set_term(p, term)` | Terminal type string |
| `dssh_chan_params_set_size(p, cols, rows, wpx, hpx)` | Terminal dimensions |
| `dssh_chan_params_set_mode(p, opcode, value)` | Terminal mode (RFC 4254 s8) |
| `dssh_chan_params_set_command(p, cmd)` | Exec command (DSSH_CHAN_EXEC) |
| `dssh_chan_params_set_subsystem(p, name)` | Subsystem name (DSSH_CHAN_SUBSYSTEM) |
| `dssh_chan_params_add_env(p, name, value)` | Environment variable |

`init` sets defaults: `term="dumb"`, dimensions `0x0`, PTY enabled
for shell (disabled for exec/subsystem), no modes, no env.  Zero
terminal mode opcodes by default — the library cannot know the app's
terminal state.

`set_mode()` accumulates opcode/value pairs.  Encoding at open time
emits opcodes 1–159 first, then 160+, then TTY_OP_END (RFC 4254 s8).

## Channel Lifecycle

### Open sequence

`dssh_chan_open()` performs the full setup sequence internally:

1. Send CHANNEL_OPEN "session" with `initial_window=0`
2. Wait for CHANNEL_OPEN_CONFIRMATION
3. Send env requests (from params)
4. Send pty-req (if enabled in params)
5. Send terminal request (shell/exec/subsystem per type)
6. Allocate I/O buffers
7. Send WINDOW_ADJUST to open the window
8. Return live channel

### Stream I/O

All channel I/O uses the unified `dssh_chan_*` prefix.  The `stream`
parameter selects stdout/stdin (0) or stderr (1):

```c
/* Read stdout */
int64_t n = dssh_chan_read(ch, 0, buf, sizeof(buf));

/* Read stderr */
int64_t n = dssh_chan_read(ch, 1, buf, sizeof(buf));

/* Peek: returns bytes available without consuming */
int64_t avail = dssh_chan_read(ch, 0, NULL, 0);

/* Write stdin */
int64_t w = dssh_chan_write(ch, 0, buf, len);

/* Write stderr (server side) */
int64_t w = dssh_chan_write(ch, 1, buf, len);
```

Return values follow POSIX semantics:
- `> 0`: bytes transferred
- `0`: EOF (read only — peer sent CHANNEL_EOF)
- `< 0`: `DSSH_ERROR_*`

### Poll events

| Flag | Meaning |
|------|---------|
| `DSSH_POLL_READ` | stdout data or EOF available |
| `DSSH_POLL_READEXT` | stderr data or EOF available |
| `DSSH_POLL_WRITE` | send window has space |
| `DSSH_POLL_EVENT` | event ready (signal, window-change, etc.) |

### Channel getters

After open or accept, query channel properties:

```c
enum dssh_chan_type dssh_chan_get_type(ch);       /* shell/exec/subsystem */
const char *dssh_chan_get_command(ch);            /* exec command (or NULL) */
const char *dssh_chan_get_subsystem(ch);          /* subsystem name (or NULL) */
bool dssh_chan_has_pty(ch);                       /* PTY enabled? */
const struct dssh_chan_params *dssh_chan_get_pty(ch);  /* PTY params (or NULL) */
```

### Half-close and close

```c
/* Half-close: send EOF, keep reading */
dssh_chan_shutwr(ch);

/* Close with exit status (sends exit-status + EOF + CLOSE) */
dssh_chan_close(ch, 0);

/* Close without exit status (exit_code < 0: no exit-status sent) */
dssh_chan_close(ch, -1);
```

`dssh_chan_close()` frees the channel handle — do not use it
after close.

### Control messages

```c
dssh_chan_send_signal(ch, "INT");
dssh_chan_send_window_change(ch, cols, rows, wpx, hpx);
dssh_chan_send_break(ch, 0);  /* RFC 4335 */
```

### Windowing

The library manages flow control automatically.  The max window size
is set per-channel via `dssh_chan_params_set_max_window()` (default
2 MiB).  WINDOW_ADJUST messages are sent automatically when the
application drains read buffers.  Window arithmetic is clamped at
2^32-1.

### Demux thread

`dssh_session_start()` launches a single demux thread that reads
all incoming packets and dispatches them to per-channel buffers.  The
demux thread also handles:

- Incoming CHANNEL_OPEN — queued for `dssh_chan_accept()`
- CHANNEL_EOF / CHANNEL_CLOSE — sets flags, wakes poll
- CHANNEL_REQUEST "signal", "exit-status", "exit-signal", "window-change", "break" — queued as events
- Forbidden channel types (x11, forwarded-tcpip, direct-tcpip, session-from-server) — auto-rejected
- Data after peer EOF/CLOSE — discarded
- Rekeying, global requests, IGNORE/DEBUG/UNIMPLEMENTED — transparent

`dssh_session_stop()` terminates the demux thread and frees all
channel resources.  Called automatically by `dssh_session_cleanup()`.

## Event Model

Events (signal, window-change, break, EOF, close, exit-status,
exit-signal) are separate from data — the `signalfd()` model.
Data always flows freely via `dssh_chan_read()`.  Events are
queued and pulled independently.

### Poll + read_event

```c
int ev = dssh_chan_poll(ch,
    DSSH_POLL_READ | DSSH_POLL_READEXT | DSSH_POLL_EVENT, -1);

if (ev & DSSH_POLL_EVENT) {
    struct dssh_chan_event event;
    dssh_chan_read_event(ch, &event);

    /* Advisory stream positions: bytes of stdout/stderr
     * received before this event */
    drain_stdout(ch, event.stdout_pos);
    drain_stderr(ch, event.stderr_pos);

    switch (event.type) {
    case DSSH_EVENT_SIGNAL:
        printf("Signal: SIG%s\n", event.signal.name);
        break;
    case DSSH_EVENT_WINDOW_CHANGE:
        resize(event.window_change.cols, event.window_change.rows);
        break;
    case DSSH_EVENT_EXIT_STATUS:
        exit_code = event.exit_status.exit_code;
        break;
    /* ... */
    }
    continue;  /* poll again — might be more events */
}
```

One event per poll/read_event cycle.  If multiple events are pending,
the next poll returns `DSSH_POLL_EVENT` immediately.

Stream positions are advisory — the app can drain to the position for
correlation, or ignore positions entirely.

String pointers in events (`signal.name`, `exit_signal.signal_name`,
`exit_signal.error_message`) are valid until the next
`dssh_chan_read_event()` or `dssh_chan_poll()` on the same channel.

### Event callback (optional)

As an alternative to poll + read_event, set a callback that fires
from the demux thread when an event is queued:

```c
dssh_chan_set_event_cb(ch, my_event_cb, my_context);

/* Or set a default for all channels on the session: */
dssh_session_set_event_cb(sess, my_event_cb, my_context);
```

The callback receives the full event struct with positions computed
at queue time.  Same restrictions as ZC callbacks: cannot call TX
functions, must be fast.

## Zero-Copy API

For high-throughput protocols (e.g. SFTP), the zero-copy API
eliminates all library-side copies.

### Open

```c
dssh_channel ch = dssh_chan_zc_open(sess, &cp, my_zc_cb, my_ctx);
```

### Inbound: data callback

The ZC callback fires on the demux thread with a pointer directly
into the decrypted `rx_packet`:

```c
static uint32_t
my_zc_cb(dssh_channel ch, int stream,
    const uint8_t *data, size_t len, void *cbdata)
{
    /* data is valid only for the duration of this callback.
     * Process or copy the data.
     * Return bytes consumed for WINDOW_ADJUST (usually len). */
    return len;
}
```

**Restrictions:** The callback MUST NOT call any TX function
(`dssh_chan_zc_getbuf`, `dssh_chan_write`, `dssh_chan_close`, etc.)
— the demux thread would deadlock during rekey.  Queue work for a
separate thread instead.

### Outbound: direct buffer access

```c
uint8_t *buf;
size_t max_len;

/* Get pointer into tx_packet — acquires tx_mtx */
dssh_chan_zc_getbuf(ch, 0, &buf, &max_len);

/* Build data directly in the buffer */
memcpy(buf, my_data, my_len);

/* Send — fills headers, MAC, encrypt, releases tx_mtx */
dssh_chan_zc_send(ch, my_len);

/* Or cancel without sending */
dssh_chan_zc_cancel(ch);
```

**Contract:** `zc_getbuf` holds `tx_mtx`.  Call `zc_send` or
`zc_cancel` promptly — do not block between them.

## Server-Side Channel Accept

`dssh_chan_accept()` receives incoming channels via callback-driven
setup.  The library drives the setup state machine and populates
a `struct dssh_chan_params` as requests arrive.

### Accept callbacks

```c
struct dssh_chan_accept_cbs cbs = {
    .pty_req = my_pty_cb,       /* optional — auto-accepts if NULL */
    .env = my_env_cb,           /* optional — auto-rejects if NULL */
    .shell = my_shell_cb,       /* optional — auto-rejects if NULL */
    .exec = my_exec_cb,         /* optional — auto-rejects if NULL */
    .subsystem = my_sub_cb,     /* optional — auto-rejects if NULL */
    .cbdata = my_context,
};

dssh_channel ch = dssh_chan_accept(sess, &cbs, -1);
```

Setup callbacks (`pty_req`, `env`) receive the accumulated params
and return 0 to accept, negative to reject.  Terminal request
callbacks (`shell`, `exec`, `subsystem`) additionally receive a
`struct dssh_chan_accept_result` to set the I/O model:

```c
static int
my_shell_cb(dssh_channel ch,
    const struct dssh_chan_params *params,
    struct dssh_chan_accept_result *result, void *cbdata)
{
    /* result is pre-filled with defaults:
     *   max_window = 0 (2 MiB default)
     *   zc_cb = NULL (stream mode)
     * Set zc_cb for zero-copy mode. */
    return 0;  /* accept */
}
```

### NULL callback behavior

| Callback | If NULL |
|----------|---------|
| `pty_req` | Auto-accept (benign) |
| `env` | Auto-reject (security: uncontrolled env vars) |
| `shell` | Auto-reject |
| `exec` | Auto-reject |
| `subsystem` | Auto-reject |

The minimal useful server sets one terminal callback:

```c
struct dssh_chan_accept_cbs cbs = { .shell = my_shell_cb };
dssh_channel ch = dssh_chan_accept(sess, &cbs, -1);
```

### Lifecycle enforcement

The library enforces during setup:
- Env requests only before the terminal request
- Exactly one terminal request per channel
- Second pty-req: reject and disconnect
- Post-setup: only window-change, break, signal accepted

### ZC mode on accept

For zero-copy accept, set `zc_cb` in the result struct:

```c
static int
my_subsystem_cb(dssh_channel ch,
    const struct dssh_chan_params *params,
    struct dssh_chan_accept_result *result, void *cbdata)
{
    result->zc_cb = my_zc_data_cb;
    result->zc_cbdata = my_context;
    return 0;
}
```

## Thread Safety

**Before `dssh_session_start()`**: single-threaded.  Handshake and auth
are sequential — no concurrent calls.

**After `dssh_session_start()`**: the demux thread handles all receiving.
The application calls `poll()` / `read()` / `write()` from any
thread.  Per-channel `buf_mtx` protects buffer access; `poll_cnd`
wakes waiters.  Transport-layer `tx_mtx` serializes sends.

During rekeying, application-layer sends (message type >= 50) are
blocked until NEWKEYS completes.

**ZC callback restrictions:** The data callback and event callback
run on the demux thread.  They MUST NOT call any TX function
(`dssh_chan_write`, `dssh_chan_zc_getbuf`, `dssh_chan_close`, etc.)
— enforced at runtime via a thread-local guard.

## Rekeying

The library automatically performs key re-exchange (RFC 4253 s9) when
any of these thresholds is exceeded:

| Threshold | Default | Constant |
|-----------|---------|----------|
| Packets per key | 2^28 (~268M) | `DSSH_REKEY_SOFT_LIMIT` |
| Bytes per key | 1 GiB | `DSSH_REKEY_BYTES` |
| Time per key | 1 hour | `DSSH_REKEY_SECONDS` |

Auto-rekey is triggered transparently by the demux thread.  Peer-
initiated rekey (incoming `SSH_MSG_KEXINIT`) is also handled
transparently.  The application does not need to do anything.

For send-only sessions without a receive thread, a hard packet limit
(2^31) returns `DSSH_ERROR_REKEY_NEEDED`.

## Optional Callbacks

Set these on the session after init, before start:

```c
dssh_session_set_debug_cb(sess, my_debug_handler, my_context);
dssh_session_set_unimplemented_cb(sess, my_unimpl_handler, my_context);
dssh_session_set_banner_cb(sess, my_banner_handler, my_context);
dssh_session_set_global_request_cb(sess, my_global_req_handler, my_context);
dssh_session_set_terminate_cb(sess, my_terminate_handler, my_context);
```

## Inactivity Timeout

Internal waits for peer responses (channel open confirmation, channel
request replies, setup messages, rekey completion) use a configurable
timeout.  Set it after init, before start:

```c
dssh_session_set_timeout(sess, 10000);   /* 10 seconds */
```

The default is **75000 ms** (75 seconds), matching the standard BSD
TCP connect timeout.  Values <= 0 disable the timeout (wait
indefinitely).  On timeout, channel-open and setup functions return
`DSSH_ERROR_TIMEOUT` (non-fatal, the session remains usable).
A rekey timeout is fatal and terminates the session.

## Error Handling

All functions return 0 on success, negative `DSSH_ERROR_*` codes
on failure.  The transport layer sends `SSH_MSG_DISCONNECT` on
protocol errors (negotiation failure, MAC mismatch) before returning.

Received `SSH_MSG_DISCONNECT` from the peer returns
`DSSH_ERROR_TERMINATED`.  Transport messages (`IGNORE`, `DEBUG`,
`UNIMPLEMENTED`), global requests (`GLOBAL_REQUEST`), and peer-
initiated rekey (`KEXINIT`) are handled transparently by the library
and never returned to the caller.

| Error Code | Value | Meaning |
|------------|-------|---------|
| `DSSH_ERROR_PARSE` | -1 | Malformed packet or field |
| `DSSH_ERROR_INVALID` | -2 | Valid parse but invalid value |
| `DSSH_ERROR_ALLOC` | -3 | Memory allocation failure |
| `DSSH_ERROR_INIT` | -4 | Initialization or crypto failure |
| `DSSH_ERROR_TERMINATED` | -5 | Session terminated |
| `DSSH_ERROR_TOOLATE` | -6 | Operation not allowed after session started |
| `DSSH_ERROR_TOOMANY` | -7 | Too many registered algorithms |
| `DSSH_ERROR_TOOLONG` | -8 | Data exceeds buffer or protocol limit |
| `DSSH_ERROR_NOMORE` | -10 | No more items in name-list |
| `DSSH_ERROR_REKEY_NEEDED` | -11 | Must rekey before sending more packets |
| `DSSH_ERROR_AUTH_REJECTED` | -12 | Authentication rejected by peer |
| `DSSH_ERROR_REJECTED` | -13 | Channel open or request rejected |
| `DSSH_ERROR_TIMEOUT` | -14 | Operation timed out |

## Packet Buffer Size

`dssh_session_init()` takes a `max_packet_size` parameter that
controls the per-session packet buffer allocation.  Pass 0 for the
RFC minimum (33280 bytes), which accommodates the RFC-required
32768-byte uncompressed payload with room for headers and padding.
Pass a larger value to support bigger payloads.

## Limits

| Constant | Value | Source | Description |
|----------|-------|--------|-------------|
| `DSSH_VERSION_STRING_MAX` | 255 | RFC 4253 s4.2 | Maximum SSH identification string length. |
| `DSSH_ALGO_NAME_MAX` | 64 | RFC 4251 s6 | Maximum algorithm name length. |
| `DSSH_NAMELIST_BUF_SIZE` | 1024 | Practical | Maximum total algorithm name-list length. |
| `DSSH_DISCONNECT_DESC_MAX` | 230 | Practical | Maximum disconnect description (clamped, not rejected). |

## Testing

```sh
cmake -S . -B build -DDEUCESSH_BUILD_TESTS=ON
cmake --build build -j8
cd build
ctest -j16                   # run all in parallel
ctest -R dssh_unit           # unit tests only
ctest -R dssh_transport      # transport layer tests
ctest -R dssh_self           # integration selftests
```

## Files

Public headers (consumers include these):
```
deucessh.h               Core: errors, session lifecycle, transport queries
deucessh-auth.h          Authentication (client + server)
deucessh-conn.h          Connection: channels, params, poll/read/write, accept
deucessh-algorithms.h    Algorithm registration + key management
deucessh-portable.h      Visibility macros (included automatically)
```

Public module headers (for third-party algorithm implementations):
```
deucessh-kex.h           KEX: context struct, handler, kex_s, registration
deucessh-key-algo.h      Host key: key_algo_s, sign/verify/pubkey, registration
deucessh-enc.h           Encryption: enc_s, init/crypt/cleanup, registration
deucessh-mac.h           MAC: mac_s, init/generate/cleanup, registration
deucessh-comp.h          Compression: comp_s, compress/uncompress, registration
```

Internal headers + sources:
```
ssh-internal.h      Session + channel struct definitions
ssh-trans.h/.c      RFC 4253 transport (packets, KEX, keys)
ssh-auth.c          RFC 4252 authentication
ssh-conn.c          RFC 4254 connection + channel I/O
ssh.c               Session lifecycle + wire format

kex/                Key exchange modules
key_algo/           Host key algorithm modules
enc/                Encryption modules
mac/                MAC modules
comp/               Compression modules

client.c            Example client
server.c            Example server

test/               Test suite (built with -DDEUCESSH_BUILD_TESTS=ON)
  dssh_test.h       Test framework (assert macros, runner)
  mock_io.h/.c      Bidirectional mock I/O layer
  test_*.c          Test source files
```
