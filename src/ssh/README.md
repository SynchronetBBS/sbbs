# DeuceSSH

A from-scratch SSH library in standard C17 implementing the core SSH protocol:

- RFC 4251 — SSH Protocol Architecture (data types)
- RFC 4252 — SSH Authentication Protocol
- RFC 4253 — SSH Transport Layer Protocol
- RFC 4254 — SSH Connection Protocol
- RFC 4256 — Generic Message Exchange Authentication (keyboard-interactive)
- RFC 4419 — Diffie-Hellman Group Exchange
- RFC 8332 — rsa-sha2-256 Host Key Algorithm
- RFC 8731 — curve25519-sha256 Key Exchange
- draft-ietf-sshm-ntruprime-ssh — sntrup761x25519-sha512 Key Exchange
- draft-ietf-sshm-mlkem-hybrid-kex — mlkem768x25519-sha256 Key Exchange

No proprietary extensions (no `@` algorithm names).

## Dependencies

- **OpenSSL 3.0+** (libcrypto only — no libssl)
- **C11 threads** (`<threads.h>`, `<stdatomic.h>`) — used internally only, not exposed in public headers

## Building

```sh
mkdir build && cd build
cmake ..
cmake --build . -j8                    # library only
cmake --build . --target client -j8    # + test client
cmake --build . --target server -j8    # + test server
```

Produces `libdeucessh.a` (static) and `libdeucessh.so` (shared).

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

static int my_rxline(uint8_t *buf, size_t bufsz,
    size_t *bytes_received, dssh_session sess, void *cbdata)
{
    /* Receive bytes one at a time until CR-LF.
     * Set *bytes_received to total including CR-LF.
     * Only used during version exchange. */
}

int main(void)
{
    /* 2. Register algorithms (order = preference) */
    dssh_transport_set_callbacks(my_tx, my_rx, my_rxline, NULL);
    dssh_register_curve25519_sha256();
    dssh_register_ssh_ed25519();
    dssh_register_aes256_ctr();
    dssh_register_none_enc();
    dssh_register_hmac_sha2_256();
    dssh_register_none_mac();
    dssh_register_none_comp();

    /* 3. Create session (opaque handle) */
    dssh_session sess = dssh_session_init(true, 0);
    /* dssh_session_set_cbdata(sess, sock_ptr, sock_ptr, sock_ptr, NULL); */

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
                my_kbi_prompt, my_context);  /* see KBI section below */
    }

    /* 6. Start the demux thread */
    dssh_session_start(sess);

    /* 7. Open channel and execute command */
    dssh_channel ch = dssh_session_open_exec(sess, "echo hello");

    /* 8. Read output using poll/read */
    uint8_t buf[4096];
    int ev;
    while ((ev = dssh_session_poll(sess, ch,
            DSSH_POLL_READ, -1)) & DSSH_POLL_READ) {
        int64_t n = dssh_session_read(sess, ch, buf, sizeof(buf));
        if (n <= 0)
            break;  /* EOF or closed */
        write(STDOUT_FILENO, buf, n);
    }

    /* 9. Clean up (close frees the channel, cleanup frees the session) */
    dssh_session_close(sess, ch, 0);
    dssh_session_cleanup(sess);
}
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

int main(void)
{
    /* 1. Register algorithms and I/O callbacks (same as client) */
    dssh_transport_set_callbacks(my_tx, my_rx, my_rxline, NULL);
    dssh_register_mlkem768x25519_sha256();
    dssh_register_sntrup761x25519_sha512();
    dssh_register_curve25519_sha256();
    dssh_register_dh_gex_sha256();
    dssh_register_ssh_ed25519();
    dssh_register_rsa_sha2_256();
    dssh_register_aes256_ctr();
    dssh_register_hmac_sha2_256();
    dssh_register_none_comp();

    /* 2. Load host keys (stored on algorithm entry, not session) */
    dssh_ed25519_load_key_file("/path/to/host_ed25519.pem", NULL, NULL);
    dssh_rsa_sha2_256_load_key_file("/path/to/host_rsa.pem", NULL, NULL);

    /* 3. Accept a connection, then create session */
    dssh_session sess = dssh_session_init(false, 0);

    /* 4. Handshake (DH-GEX uses built-in RFC 3526 groups by default) */
    dssh_transport_handshake(sess);

    /* 5. Authenticate */
    struct dssh_auth_server_cbs auth_cbs = {
        .methods_str = "publickey,password,keyboard-interactive",
        .password_cb = my_password_cb,
        .publickey_cb = my_publickey_cb,
        .keyboard_interactive_cb = my_kbi_cb,
    };
    uint8_t username[256];
    size_t username_len = sizeof(username);
    dssh_auth_set_banner(sess, "Welcome!\r\n", NULL);
    dssh_auth_server(sess, &auth_cbs, username, &username_len);

    /* 7. Start demux thread */
    dssh_session_start(sess);

    /* 8. Accept incoming channels */
    struct dssh_incoming_open *inc;
    while (dssh_session_accept(sess, &inc, -1) == 0) {
        struct dssh_server_session_cbs scbs = {
            .request_cb = my_channel_request_cb,
        };
        const char *req_type, *req_data;
        dssh_channel ch = dssh_session_accept_channel(sess, inc,
            &scbs, &req_type, &req_data);
        if (ch == NULL) continue;

        if (strcmp(req_type, "subsystem") == 0) {
            /* Subsystem channels are raw (message-based) */
            int ev;
            while ((ev = dssh_channel_poll(sess, ch,
                    DSSH_POLL_READ, -1)) & DSSH_POLL_READ) {
                uint8_t msg[4096];
                int64_t len = dssh_channel_read(sess, ch,
                    msg, sizeof(msg));
                if (len <= 0) break;
                /* process and respond... */
                dssh_channel_write(sess, ch, response, resp_len);
            }
            dssh_channel_close(sess, ch);
        } else {
            /* Shell/exec channels are session (stream-based) */
            uint8_t buf[4096];
            int ev;
            while ((ev = dssh_session_poll(sess, ch,
                    DSSH_POLL_READ, -1)) & DSSH_POLL_READ) {
                int64_t n = dssh_session_read(sess, ch,
                    buf, sizeof(buf));
                if (n <= 0) break;
                dssh_session_write(sess, ch, buf, n);
            }
            dssh_session_close(sess, ch, 0);
        }
    }

    /* 9. Clean up */
    dssh_session_cleanup(sess);
}
```

## I/O Callbacks

The library does no I/O itself.  You provide three callbacks:

| Callback | Purpose |
|----------|---------|
| `tx` | Send exactly N bytes.  Block until done or `dssh_session_is_terminated(sess)`. |
| `rx` | Receive exactly N bytes.  Same contract as tx. |
| `rx_line` | Receive until CR-LF (version exchange only). |

All callbacks receive the session handle and a `cbdata` pointer.
Return 0 on success, negative on error.  The `cbdata` is set via
`dssh_session_set_cbdata()`.

Callbacks are registered globally once.  Per-session differentiation
is via the `cbdata` pointers.

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

For simple password-only authentication, the callback can ignore the
prompts and return the password for every response:

```c
static int
simple_kbi(const uint8_t *name, size_t name_len,
    const uint8_t *instruction, size_t instruction_len,
    uint32_t num_prompts,
    const uint8_t **prompts, const size_t *prompt_lens,
    const bool *echo,
    uint8_t **responses, size_t *response_lens,
    void *cbdata)
{
    const char *password = cbdata;
    size_t plen = strlen(password);
    for (uint32_t i = 0; i < num_prompts; i++) {
        responses[i] = malloc(plen);
        if (responses[i] == NULL)
            return DSSH_ERROR_ALLOC;
        memcpy(responses[i], password, plen);
        response_lens[i] = plen;
    }
    return 0;
}

/* Usage: */
dssh_auth_keyboard_interactive(sess, "user",
    simple_kbi, (void *)"mypassword");
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
static int my_sign(uint8_t *buf, size_t bufsz, size_t *outlen,
    const uint8_t *data, size_t data_len, dssh_key_algo_ctx *ctx)
{
    /* Sign data, write SSH-encoded signature blob to buf.
     * Set *outlen to actual size written.  Return 0 on success. */
}

static int my_verify(const uint8_t *key_blob, size_t key_blob_len,
    const uint8_t *sig_blob, size_t sig_blob_len,
    const uint8_t *data, size_t data_len)
{
    /* Verify that sig_blob is a valid signature of data under
     * the public key in key_blob.  Return 0 if valid. */
}

static int my_pubkey(uint8_t *buf, size_t bufsz, size_t *outlen,
    dssh_key_algo_ctx *ctx)
{
    /* Write SSH-encoded public key blob to buf.
     * Set *outlen to actual size.  Return 0 on success. */
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

### Registration Rules

- Call `dssh_register_*()` before any `dssh_session_init()`.
- After the first session init, registration is locked (`DSSH_ERROR_TOOLATE`).
- Names must be 1–64 printable ASCII characters (no spaces, no control chars).
- The `next` pointer must be NULL (the library manages the linked list).
- Registration order determines negotiation preference.

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

## Server-Side Channel Setup

`dssh_session_accept_channel()` processes incoming `CHANNEL_REQUEST`
messages until a terminal request (shell/exec/subsystem) arrives.
Every request is passed to a single callback:

```c
static int
my_channel_request_cb(const char *type, size_t type_len,
    bool want_reply, const uint8_t *data, size_t data_len,
    void *cbdata)
{
    if (type_len == 7 && memcmp(type, "pty-req", 7) == 0) {
        struct dssh_pty_req pty;
        dssh_parse_pty_req_data(data, data_len, &pty);
        printf("PTY: %.*s %ux%u\n",
            /* pty.term points into data, NOT NUL-terminated */
            (int)(data_len >= 4 ? *(uint32_t*)data : 0), pty.term,
            pty.cols, pty.rows);
        return 0;  /* accept */
    }
    if (type_len == 3 && memcmp(type, "env", 3) == 0)
        return 0;  /* accept env */
    if (type_len == 5 && memcmp(type, "shell", 5) == 0)
        return 0;  /* accept → DSSH_CHAN_SESSION */
    if (type_len == 4 && memcmp(type, "exec", 4) == 0)
        return 0;  /* accept → DSSH_CHAN_SESSION */
    if (type_len == 9 && memcmp(type, "subsystem", 9) == 0)
        return 0;  /* accept → DSSH_CHAN_RAW */
    return -1;     /* reject unknown */
}
```

The channel type is determined by the terminal request:
- **shell** or **exec** → `DSSH_CHAN_SESSION` (stream-based, use `dssh_session_*` API)
- **subsystem** → `DSSH_CHAN_RAW` (message-based, use `dssh_channel_*` API)

Parse helpers for the `data` payload:
- `dssh_parse_pty_req_data(data, len, &pty)` — fills `struct dssh_pty_req`
- `dssh_parse_env_data(data, len, &name, &nlen, &value, &vlen)`
- `dssh_parse_exec_data(data, len, &command, &clen)`
- `dssh_parse_subsystem_data(data, len, &name, &nlen)`

You can also use the RFC 4251 primitives directly: `dssh_parse_uint32()`,
`dssh_parse_string()`, etc. (from `deucessh-arch.h`).

## Channel Lifecycle

### Two channel types

**Session channels** (shell/exec) — stream-based, three logical
streams (stdin/stdout/stderr), signal synchronization, exit status:

```c
/* Open (returns opaque channel handle, NULL on failure) */
dssh_channel ch = dssh_session_open_shell(sess, &pty);

/* I/O */
dssh_session_poll(sess, ch, events, timeout_ms);
dssh_session_read(sess, ch, buf, bufsz);       /* stdout */
dssh_session_read_ext(sess, ch, buf, bufsz);   /* stderr */
dssh_session_write(sess, ch, buf, bufsz);      /* stdin */
dssh_session_write_ext(sess, ch, buf, bufsz);  /* stderr out */
dssh_session_read_signal(sess, ch, &name);     /* signals */
dssh_session_send_signal(sess, ch, "INT");     /* send signal */

/* Close (sends exit-status + EOF + CLOSE) */
dssh_session_close(sess, ch, exit_code);
```

**Raw channels** (subsystems) — message-based, bidirectional, no
partial reads or writes:

```c
/* Open (returns opaque channel handle, NULL on failure) */
dssh_channel ch = dssh_channel_open_subsystem(sess, "sftp");

/* I/O */
dssh_channel_poll(sess, ch, events, timeout_ms);
len = dssh_channel_read(sess, ch, NULL, 0);   /* peek size */
dssh_channel_read(sess, ch, buf, bufsz);      /* read msg */
dssh_channel_write(sess, ch, buf, len);        /* write msg */

/* Close (sends EOF + CLOSE) */
dssh_channel_close(sess, ch);
```

### Poll events

| Flag | Meaning |
|------|---------|
| `DSSH_POLL_READ` | stdout data (session) or message (raw) available, or EOF |
| `DSSH_POLL_READEXT` | stderr data available, or EOF (session only) |
| `DSSH_POLL_WRITE` | send window has space |
| `DSSH_POLL_SIGNAL` | signal ready — both streams drained to mark (session only) |

A read that returns 0 means EOF or closed.

### Windowing

The library manages flow control automatically.  The application sets
a max window size at channel creation (or uses the default 2 MiB).
WINDOW_ADJUST messages are sent automatically when the application
drains the read buffers.  The application never sees window messages.
Window arithmetic is clamped at 2^32-1.

### Channel close

Channels are bidirectional — each direction closes independently.
EOF indicates "I'm done sending"; CLOSE tears down the channel.

When the peer sends CLOSE, the library sets `close_received` and
wakes poll (reads return 0).  The library does NOT auto-send the
reciprocal CLOSE — the application gets a chance to drain remaining
data, send exit-status, and clean up before calling `session_close()`
or `channel_close()` to send the reciprocal CLOSE.

Data received after the peer's EOF or CLOSE is silently discarded.
Writes are blocked after `close_received`.

### Demux thread

`dssh_session_start()` launches a single demux thread that reads
all incoming packets and dispatches them to per-channel buffers.  The
demux thread also handles:

- Incoming CHANNEL_OPEN — queued for `session_accept()`
- CHANNEL_EOF / CHANNEL_CLOSE — sets flags, wakes poll
- CHANNEL_REQUEST "signal" — queued with stream position marks
- CHANNEL_REQUEST "exit-status" — stored on channel
- CHANNEL_REQUEST "window-change" — server callback
- Forbidden channel types (x11, forwarded-tcpip, direct-tcpip,
  session-from-server) — auto-rejected
- Data after peer EOF/CLOSE — discarded
- Rekeying, global requests, IGNORE/DEBUG/UNIMPLEMENTED — transparent

`dssh_session_stop()` terminates the demux thread and frees all
channel resources.  Called automatically by `session_cleanup()`.

## Thread Safety

The library uses two concurrency models:

**Before `session_start()`**: single-threaded.  Handshake and auth
are sequential — no concurrent calls.

**After `session_start()`**: the demux thread handles all receiving.
The application calls `poll()` / `read()` / `write()` from any
thread.  Per-channel `buf_mtx` protects buffer access; `poll_cnd`
wakes waiters.  Transport-layer `tx_mtx` serializes sends.  The
demux thread and application threads never contend on the data path
beyond brief mutex acquisitions.

During rekeying, application-layer sends (message type >= 50) are
blocked until NEWKEYS completes.  Transport messages pass through.

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

Set these on the session after init, before handshake:

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

## Packet Buffer Size

`dssh_session_init()` takes a `max_packet_size` parameter that
controls the per-session packet buffer allocation.  Pass 0 for the
RFC minimum (33280 bytes), which accommodates the RFC-required
32768-byte uncompressed payload with room for headers and padding.
Pass a larger value to support bigger payloads.

## Limits

The library enforces several size limits, some from the RFCs and some
practical choices:

| Constant | Value | Source | Description |
|----------|-------|--------|-------------|
| `DSSH_VERSION_STRING_MAX` | 255 | RFC 4253 s4.2 | Maximum SSH identification string length (including `SSH-2.0-` prefix and CR LF). |
| `DSSH_ALGO_NAME_MAX` | 64 | RFC 4251 s6 | Maximum length of a single algorithm name. Names exceeding this are rejected during KEXINIT parsing. |
| `DSSH_NAMELIST_BUF_SIZE` | 1024 | Practical | Maximum total length of a comma-separated algorithm name-list. With 64-char names this fits ~15 algorithms per category. Algorithms that would cause the list to exceed this size are silently omitted from negotiation. |
| `DSSH_DISCONNECT_DESC_MAX` | 230 | Practical | Maximum disconnect description length. Longer descriptions are silently clamped (not rejected). |

## Testing

~1000 tests across 11 executables, ~2150 CTest runs (~23s with `-j8`).
Layer and integration tests run as individual processes to eliminate
shared-state issues:

```sh
cmake -S . -B build -DDEUCESSH_BUILD_TESTS=ON
cmake --build build -j8
cd build
ctest -j8                    # run all
ctest -R dssh_unit           # unit tests only
ctest -R dssh_transport      # transport layer tests
ctest -R dssh_self           # integration selftests
```

| Suite | Tests | Scope |
|-------|-------|-------|
| test_arch | ~130 | RFC 4251 wire format: parse/serialize/roundtrip for all types |
| test_chan | ~80 | Buffer primitives: bytebuf, msgqueue, signal queue, accept queue |
| test_algo_enc | 23 | AES-256-CTR (NIST vectors), none cipher |
| test_algo_mac | 19 | HMAC-SHA-256 (RFC 4231 vectors), none MAC |
| test_algo_key | ~130 | Ed25519 + RSA: generate/sign/verify/save/load/parse/coverage |
| test_transport | ~170 | Version exchange, packets, KEXINIT, handshake, rekey, registration |
| test_auth | ~160 | Password, KBI, publickey, server-side auth, banners, parse errors |
| test_conn | ~120 | Channels, demux, session/raw I/O, signals, close, threading |
| test_alloc | ~80 | Iterative malloc/OpenSSL/C11 failure injection (×4 KEX/key combos) |
| test_transport_errors | 11 | Transport error paths with test enc/mac modules |
| test_selftest | 15 | Full client↔server via socketpair, including rekey under load |

Tests use a custom framework (`test/dssh_test.h`) and mock I/O via
`socketpair(AF_UNIX)`.  Failure injection uses library-only malloc
redirect (`dssh_test_alloc.h`) and OpenSSL/C11 call wrappers
(`dssh_test_ossl.h`) — both use atomic countdown with plateau
detection for automatic termination.

## Files

Public headers (consumers include these):
```
deucessh.h               Core: errors, session lifecycle, transport queries
deucessh-auth.h          Authentication (client + server)
deucessh-conn.h          Connection: channels, poll/read/write
deucessh-algorithms.h    Algorithm registration + key management
deucessh-portable.h      Visibility macros (included automatically)
deucessh-arch.h          Wire data types (included automatically)
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
ssh-conn.c          RFC 4254 connection
ssh-chan.h/.c       Buffer primitives (bytebuf, msgqueue, signals)
ssh.c               Session lifecycle

kex/                Key exchange modules
key_algo/           Host key algorithm modules
enc/                Encryption modules
mac/                MAC modules
comp/               Compression modules

client.c            Example client
server.c            Example server

test/               Test suite (built with -DDEUCESSH_BUILD_TESTS=ON)
  dssh_test.h       Test framework (assert macros, runner)
  dssh_test_internal.h  Declarations for exposed static functions
  mock_io.h/.c      Bidirectional mock I/O layer
  test_*.c          Test source files (9 executables)
```
