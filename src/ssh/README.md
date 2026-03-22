# DeuceSSH

A from-scratch SSH library in standard C17 implementing the core SSH protocol:

- RFC 4251 — SSH Protocol Architecture (data types)
- RFC 4252 — SSH Authentication Protocol
- RFC 4253 — SSH Transport Layer Protocol
- RFC 4254 — SSH Connection Protocol
- RFC 4419 — Diffie-Hellman Group Exchange
- RFC 8332 — rsa-sha2-256 Host Key Algorithm
- RFC 8731 — curve25519-sha256 Key Exchange

No proprietary extensions (no `@` algorithm names).

## Dependencies

- **OpenSSL 3.0+** (libcrypto only — no libssl)
- **C11 threads** (`<threads.h>`, `<stdatomic.h>`)

## Building

```sh
mkdir build && cd build
cmake ..
cmake --build . -j8                    # library only
cmake --build . --target client -j8    # + test client
cmake --build . --target server -j8    # + test server
```

Produces `libdeuce-ssh.a` (static) and `libdeuce-ssh.so` (shared).

## Built-in Algorithms

| Category | Algorithms |
|----------|-----------|
| Key Exchange | `curve25519-sha256`, `diffie-hellman-group-exchange-sha256` |
| Host Key | `ssh-ed25519`, `rsa-sha2-256` |
| Encryption | `aes256-ctr`, `none` |
| MAC | `hmac-sha2-256`, `none` |
| Compression | `none` |

All algorithms support both client and server side.

## Quick Start — Client

```c
#include "deucessh.h"
#include "ssh-auth.h"
#include "ssh-conn.h"

/* Include headers for the algorithms you want */
#include "kex/curve25519-sha256.h"
#include "key_algo/ssh-ed25519.h"
#include "enc/aes256-ctr.h"
#include "enc/none.h"
#include "mac/hmac-sha2-256.h"
#include "mac/none.h"
#include "comp/none.h"

/* 1. Implement I/O callbacks */
static int my_tx(uint8_t *buf, size_t bufsz,
    atomic_bool *terminate, void *cbdata)
{
    /* Send exactly bufsz bytes over your transport (socket, etc.).
     * Block until complete or *terminate becomes true.
     * Return 0 on success, negative on error. */
}

static int my_rx(uint8_t *buf, size_t bufsz,
    atomic_bool *terminate, void *cbdata)
{
    /* Receive exactly bufsz bytes.  Same contract as tx. */
}

static int my_rxline(uint8_t *buf, size_t bufsz,
    size_t *bytes_received, atomic_bool *terminate, void *cbdata)
{
    /* Receive bytes one at a time until CR-LF.
     * Set *bytes_received to total including CR-LF.
     * Only used during version exchange. */
}

int main(void)
{
    /* 2. Register algorithms (order = preference) */
    deuce_ssh_transport_set_callbacks(my_tx, my_rx, my_rxline, NULL);
    register_curve25519_sha256();
    register_ssh_ed25519();
    register_aes256_ctr();
    register_none_enc();
    register_hmac_sha2_256();
    register_none_mac();
    register_none_comp();

    /* 3. Create and initialize session */
    struct deuce_ssh_session_s sess;
    memset(&sess, 0, sizeof(sess));
    sess.trans.client = true;
    /* sess.tx_cbdata = your_socket_ptr;  -- passed to I/O callbacks */
    /* sess.rx_cbdata = your_socket_ptr; */
    /* sess.rx_line_cbdata = your_socket_ptr; */
    deuce_ssh_session_init(&sess, 0);  /* 0 = RFC minimum (33280 bytes) */

    /* 4. Handshake */
    deuce_ssh_transport_handshake(&sess);

    /* 5. Authenticate */
    char methods[256];
    int r = deuce_ssh_auth_get_methods(&sess, "user", methods, sizeof(methods));
    if (r > 0) {
        if (strstr(methods, "password"))
            deuce_ssh_auth_password(&sess, "user", "pass", NULL, NULL);
        else if (strstr(methods, "keyboard-interactive"))
            deuce_ssh_auth_keyboard_interactive(&sess, "user",
                my_kbi_prompt, my_context);  /* see KBI section below */
    }

    /* 6. Start the demux thread */
    deuce_ssh_session_start(&sess);

    /* 7. Open channel and execute command (high-level API) */
    struct deuce_ssh_channel_s ch;
    deuce_ssh_session_open_exec(&sess, &ch, "echo hello");

    /* 8. Read output using poll/read */
    uint8_t buf[4096];
    int ev;
    while ((ev = deuce_ssh_session_poll(&sess, &ch,
            DEUCE_SSH_POLL_READ, -1)) & DEUCE_SSH_POLL_READ) {
        ssize_t n = deuce_ssh_session_read(&sess, &ch, buf, sizeof(buf));
        if (n <= 0)
            break;  /* EOF or closed */
        write(STDOUT_FILENO, buf, n);
    }

    /* 9. Clean up */
    deuce_ssh_session_close(&sess, &ch, 0);
    deuce_ssh_session_cleanup(&sess);
}
```

## Quick Start — Server

```c
#include "deucessh.h"
#include "ssh-trans.h"
#include "ssh-auth.h"
#include "ssh-conn.h"
#include "kex/curve25519-sha256.h"
#include "kex/dh-gex-sha256.h"
#include "key_algo/ssh-ed25519.h"
#include "enc/aes256-ctr.h"
#include "enc/none.h"
#include "mac/hmac-sha2-256.h"
#include "mac/none.h"
#include "comp/none.h"

/* Server-side auth: accept any password */
static int my_password_cb(const uint8_t *username, size_t username_len,
    const uint8_t *password, size_t password_len,
    uint8_t **change_prompt, size_t *change_prompt_len, void *cbdata)
{
    /* Validate credentials against your user database.
     * Return DEUCE_SSH_AUTH_SUCCESS, DEUCE_SSH_AUTH_FAILURE,
     * DEUCE_SSH_AUTH_PARTIAL, or DEUCE_SSH_AUTH_CHANGE_PASSWORD. */
    return DEUCE_SSH_AUTH_SUCCESS;
}

int main(void)
{
    /* 1. Register algorithms and I/O callbacks (same as client) */
    deuce_ssh_transport_set_callbacks(my_tx, my_rx, my_rxline, NULL);
    register_curve25519_sha256();
    register_dh_gex_sha256();
    register_ssh_ed25519();
    register_aes256_ctr();
    register_none_enc();
    register_hmac_sha2_256();
    register_none_mac();
    register_none_comp();

    /* 2. Accept a connection, then create session */
    struct deuce_ssh_session_s sess;
    memset(&sess, 0, sizeof(sess));
    sess.trans.client = false;  /* server mode */
    deuce_ssh_session_init(&sess, 0);  /* 0 = RFC minimum (33280 bytes) */

    /* 3. Load host keys (stored on algorithm entry, not session).
     * Pass NULL for unencrypted PEM, or a pem_password_cb for encrypted. */
    ssh_ed25519_load_key_file("/path/to/host_ed25519.pem", NULL, NULL);
    rsa_sha2_256_load_key_file("/path/to/host_rsa.pem", NULL, NULL);
    /* Or generate + save:
     *   ssh_ed25519_generate_key();
     *   ssh_ed25519_save_key_file("/path/to/host_ed25519.pem", NULL, NULL);
     *   ssh_ed25519_save_pub_file("/path/to/host_ed25519.pub");
     *
     * To encrypt the saved key, pass a pem_password_cb:
     *   ssh_ed25519_save_key_file("/path/to/key.pem", my_pw_cb, my_ctx);
     *
     * To get the public key string at runtime (pass NULL/0 for size query):
     *   ssize_t len = ssh_ed25519_get_pub_str(NULL, 0);
     *   char *pub = malloc(len);
     *   ssh_ed25519_get_pub_str(pub, len);  // "ssh-ed25519 AAAA..."
     */

    /* 4. If offering DH-GEX, provide a group selection callback */
    struct deuce_ssh_dh_gex_provider provider = {
        .select_group = my_group_selector,
        .cbdata = NULL,
    };
    deuce_ssh_dh_gex_set_provider(&sess, &provider);

    /* 5. Handshake */
    deuce_ssh_transport_handshake(&sess);

    /* 6. Authenticate */
    struct deuce_ssh_auth_server_cbs auth_cbs = {
        .methods_str = "publickey,password",
        .password_cb = my_password_cb,
        /* .publickey_cb = my_pubkey_cb, */
        /* .none_cb = my_none_cb, */
    };
    uint8_t username[256];
    size_t username_len;
    deuce_ssh_auth_server(&sess, &auth_cbs, username, &username_len);

    /* 7. Start demux thread */
    deuce_ssh_session_start(&sess);

    /* 8. Accept incoming channels */
    struct deuce_ssh_incoming_open *inc;
    while (deuce_ssh_session_accept(&sess, &inc, -1) == 0) {
        /* Accept as raw channel (for subsystems) */
        struct deuce_ssh_channel_s ch;
        deuce_ssh_channel_accept_raw(&sess, inc, &ch);

        /* Read/write using poll */
        int ev;
        while ((ev = deuce_ssh_channel_poll(&sess, &ch,
                DEUCE_SSH_POLL_READ, -1)) & DEUCE_SSH_POLL_READ) {
            ssize_t len = deuce_ssh_channel_read(&sess, &ch, NULL, 0);
            if (len <= 0)
                break;
            uint8_t *msg = malloc(len);
            deuce_ssh_channel_read(&sess, &ch, msg, len);
            /* process and respond... */
            deuce_ssh_channel_write(&sess, &ch, response, resp_len);
            free(msg);
        }

        deuce_ssh_channel_close(&sess, &ch);
    }

    /* 9. Clean up */
    deuce_ssh_session_cleanup(&sess);
}
```

## I/O Callbacks

The library does no I/O itself.  You provide three callbacks:

| Callback | Purpose |
|----------|---------|
| `tx` | Send exactly N bytes.  Block until done or `*terminate` is true. |
| `rx` | Receive exactly N bytes.  Block until done or `*terminate` is true. |
| `rx_line` | Receive until CR-LF (version exchange only). |

All callbacks return 0 on success, negative on error.  The `cbdata`
parameter comes from the session struct (`sess->tx_cbdata`, etc.),
allowing you to pass a socket handle or connection context.

Callbacks are registered globally once.  Per-session differentiation
is via the `cbdata` pointers.

## Authentication — Client Side

### Password

```c
/* Simple password auth (no password change support): */
deuce_ssh_auth_password(&sess, "user", "pass", NULL, NULL);

/* With password change support: */
deuce_ssh_auth_password(&sess, "user", "pass",
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
        return DEUCE_SSH_ERROR_ALLOC;
    memcpy(*new_password, buf, len);
    *new_password_len = len;
    return 0;
}
```

### Public Key

```c
#include "key_algo/ssh-ed25519.h"

/* Load the private key (stored on the algorithm entry) */
ssh_ed25519_load_key_file("/path/to/id_ed25519", NULL, NULL);

/* Authenticate */
deuce_ssh_auth_publickey(&sess, "user", "ssh-ed25519");
```

The function signs the session-bound authentication data (session ID
+ request fields) per RFC 4252 s7 and sends the signed request.
Works with any registered key algorithm that supports signing.

### Keyboard-Interactive

`deuce_ssh_auth_keyboard_interactive()` uses a callback to handle
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
            return DEUCE_SSH_ERROR_ALLOC;
        memcpy(responses[i], buf, len);
        response_lens[i] = len;
    }
    return 0;
}

/* Usage: */
deuce_ssh_auth_keyboard_interactive(&sess, "user",
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
            return DEUCE_SSH_ERROR_ALLOC;
        memcpy(responses[i], password, plen);
        response_lens[i] = plen;
    }
    return 0;
}

/* Usage: */
deuce_ssh_auth_keyboard_interactive(&sess, "user",
    simple_kbi, (void *)"mypassword");
```

## Authentication — Server Side

`deuce_ssh_auth_server()` drives the server-side authentication loop.
It handles SERVICE_REQUEST/ACCEPT, receives USERAUTH_REQUEST messages,
dispatches to your callbacks, and sends SUCCESS/FAILURE/CHANGEREQ/PK_OK
responses.

```c
struct deuce_ssh_auth_server_cbs cbs = {
    .methods_str = "publickey,password",
    .password_cb = my_password_cb,
    .passwd_change_cb = my_passwd_change_cb,  /* optional */
    .publickey_cb = my_pubkey_cb,             /* optional */
    .none_cb = my_none_cb,                    /* optional */
    .cbdata = my_server_context,
};
uint8_t username[256];
size_t username_len;
int r = deuce_ssh_auth_server(&sess, &cbs, username, &username_len);
```

### Callback Return Values

| Value | Meaning |
|-------|---------|
| `DEUCE_SSH_AUTH_SUCCESS` | User authenticated — library sends SUCCESS |
| `DEUCE_SSH_AUTH_FAILURE` | Rejected — library sends FAILURE |
| `DEUCE_SSH_AUTH_PARTIAL` | Succeeded, but more auth needed — library sends FAILURE with partial_success=TRUE |
| `DEUCE_SSH_AUTH_CHANGE_PASSWORD` | Password expired — library sends PASSWD_CHANGEREQ (password method only) |

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
     * prompt string and return DEUCE_SSH_AUTH_CHANGE_PASSWORD. */
    if (check_password(username, username_len, password, password_len))
        return DEUCE_SSH_AUTH_SUCCESS;
    return DEUCE_SSH_AUTH_FAILURE;
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
     * Return DEUCE_SSH_AUTH_CHANGE_PASSWORD to re-prompt. */
    if (!check_password(username, username_len,
            old_password, old_password_len))
        return DEUCE_SSH_AUTH_FAILURE;
    set_password(username, username_len, new_password, new_password_len);
    return DEUCE_SSH_AUTH_SUCCESS;
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
        return DEUCE_SSH_AUTH_SUCCESS;
    return DEUCE_SSH_AUTH_FAILURE;
}
```

## Algorithm Registration

Call `register_*()` functions before any session is created.
Registration order determines negotiation preference — first registered
is most preferred.

```c
/* Prefer curve25519, fall back to DH-GEX */
register_curve25519_sha256();
register_dh_gex_sha256();
```

You can register your own custom algorithm modules by implementing
the callback signatures defined in `ssh-trans.h` and calling the
corresponding `deuce_ssh_transport_register_*()` function.

## DH Group Provider (Server)

If the server offers `diffie-hellman-group-exchange-sha256`, it must
provide a callback that selects a safe prime for the requested size:

```c
int my_group_selector(uint32_t min, uint32_t preferred, uint32_t max,
    uint8_t **p, size_t *p_len,
    uint8_t **g, size_t *g_len, void *cbdata)
{
    /* Return p and g as big-endian byte arrays (caller frees).
     * Select based on min/preferred/max.
     * No OpenSSL dependency needed here — just return bytes. */
}
```

Set it on the session before key exchange:

```c
struct deuce_ssh_dh_gex_provider prov = {
    .select_group = my_group_selector,
    .cbdata = NULL,
};
deuce_ssh_dh_gex_set_provider(&sess, &prov);
```

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

The `pw_cb` parameter is OpenSSL's `pem_password_cb` — pass NULL for
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
ssh_ed25519_generate_key();
ssh_ed25519_save_key_file("/etc/ssh/host_ed25519",
    my_pw_cb, "passphrase");
ssh_ed25519_save_pub_file("/etc/ssh/host_ed25519.pub");

/* Load encrypted key */
ssh_ed25519_load_key_file("/etc/ssh/host_ed25519",
    my_pw_cb, "passphrase");

/* Get public key string at runtime */
ssize_t len = ssh_ed25519_get_pub_str(NULL, 0);
char *pub = malloc(len);
ssh_ed25519_get_pub_str(pub, len);  /* "ssh-ed25519 AAAA..." */
```

## Channel Lifecycle

### Two channel types

**Session channels** (shell/exec) — stream-based, three logical
streams (stdin/stdout/stderr), signal synchronization, exit status:

```c
/* Open */
deuce_ssh_session_open_shell(&sess, &ch, &pty);  /* or open_exec */

/* I/O */
deuce_ssh_session_poll(&sess, &ch, events, timeout_ms);
deuce_ssh_session_read(&sess, &ch, buf, bufsz);       /* stdout */
deuce_ssh_session_read_ext(&sess, &ch, buf, bufsz);   /* stderr */
deuce_ssh_session_write(&sess, &ch, buf, bufsz);      /* stdin */
deuce_ssh_session_write_ext(&sess, &ch, buf, bufsz);  /* stderr out */
deuce_ssh_session_read_signal(&sess, &ch, &name);     /* signals */
deuce_ssh_session_send_signal(&sess, &ch, "INT");     /* send signal */

/* Close (sends exit-status + EOF + CLOSE) */
deuce_ssh_session_close(&sess, &ch, exit_code);
```

**Raw channels** (subsystems) — message-based, bidirectional, no
partial reads or writes:

```c
/* Open */
deuce_ssh_channel_open_subsystem(&sess, &ch, "sftp");

/* I/O */
deuce_ssh_channel_poll(&sess, &ch, events, timeout_ms);
len = deuce_ssh_channel_read(&sess, &ch, NULL, 0);   /* peek size */
deuce_ssh_channel_read(&sess, &ch, buf, bufsz);      /* read msg */
deuce_ssh_channel_write(&sess, &ch, buf, len);        /* write msg */

/* Close (sends EOF + CLOSE) */
deuce_ssh_channel_close(&sess, &ch);
```

### Poll events

| Flag | Meaning |
|------|---------|
| `DEUCE_SSH_POLL_READ` | stdout data (session) or message (raw) available, or EOF |
| `DEUCE_SSH_POLL_READEXT` | stderr data available, or EOF (session only) |
| `DEUCE_SSH_POLL_WRITE` | send window has space |
| `DEUCE_SSH_POLL_SIGNAL` | signal ready — both streams drained to mark (session only) |

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

`deuce_ssh_session_start()` launches a single demux thread that reads
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

`deuce_ssh_session_stop()` terminates the demux thread and frees all
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
| Packets per key | 2^28 (~268M) | `DEUCE_SSH_REKEY_SOFT_LIMIT` |
| Bytes per key | 1 GiB | `DEUCE_SSH_REKEY_BYTES` |
| Time per key | 1 hour | `DEUCE_SSH_REKEY_SECONDS` |

Auto-rekey is triggered transparently by the demux thread.  Peer-
initiated rekey (incoming `SSH_MSG_KEXINIT`) is also handled
transparently.  The application does not need to do anything.

For send-only sessions without a receive thread, a hard packet limit
(2^31) returns `DEUCE_SSH_ERROR_REKEY_NEEDED`.

## Optional Callbacks

Set these on the session struct before or after init:

```c
/* SSH_MSG_DEBUG notification */
sess.debug_cb = my_debug_handler;
sess.debug_cbdata = my_context;

/* SSH_MSG_UNIMPLEMENTED notification */
sess.unimplemented_cb = my_unimpl_handler;
sess.unimplemented_cbdata = my_context;

/* SSH_MSG_USERAUTH_BANNER (client-side, during authentication) */
sess.banner_cb = (void *)my_banner_handler;
sess.banner_cbdata = my_context;

/* SSH_MSG_GLOBAL_REQUEST (RFC 4254 s4) — escape hatch for
 * application-specific global requests.  By default, all global
 * requests are consumed by the library and answered with
 * REQUEST_FAILURE.  If set, the callback can return >= 0 to
 * send REQUEST_SUCCESS instead. */
sess.global_request_cb = (void *)my_global_req_handler;
sess.global_request_cbdata = my_context;
```

## Error Handling

All functions return 0 on success, negative `DEUCE_SSH_ERROR_*` codes
on failure.  The transport layer sends `SSH_MSG_DISCONNECT` on
protocol errors (negotiation failure, MAC mismatch) before returning.

Received `SSH_MSG_DISCONNECT` from the peer returns
`DEUCE_SSH_ERROR_TERMINATED`.  Transport messages (`IGNORE`, `DEBUG`,
`UNIMPLEMENTED`), global requests (`GLOBAL_REQUEST`), and peer-
initiated rekey (`KEXINIT`) are handled transparently by the library
and never returned to the caller.

## Packet Buffer Size

`deuce_ssh_session_init()` takes a `max_packet_size` parameter that
controls the per-session packet buffer allocation.  Pass 0 for the
RFC minimum (33280 bytes), which accommodates the RFC-required
32768-byte uncompressed payload with room for headers and padding.
Pass a larger value to support bigger payloads.

## Files

```
deucessh.h          Main header (session, errors, callbacks)
ssh-arch.h/.c       RFC 4251 data types (parse/serialize)
ssh-trans.h/.c      RFC 4253 transport (packets, KEX, keys)
ssh-auth.h/.c       RFC 4252 authentication (client and server)
ssh-conn.h/.c       RFC 4254 connection (channels)
ssh.c               Session lifecycle

kex/                Key exchange modules
key_algo/           Host key algorithm modules
enc/                Encryption modules
mac/                MAC modules
comp/               Compression modules

client.c            Example client with two-threaded I/O
server.c            Example server with ping/pong exec
```
