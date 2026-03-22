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
    deuce_ssh_session_init(&sess);

    /* 4. Handshake (single-threaded) */
    deuce_ssh_transport_version_exchange(&sess);
    deuce_ssh_transport_kexinit(&sess);
    deuce_ssh_transport_kex(&sess);
    deuce_ssh_transport_newkeys(&sess);
    /* Encrypted transport is now active */

    /* 5. Authenticate */
    deuce_ssh_auth_request_service(&sess, "ssh-userauth");

    char methods[256];
    int r = deuce_ssh_auth_get_methods(&sess, "user", methods, sizeof(methods));
    if (r > 0) {
        if (strstr(methods, "password"))
            deuce_ssh_auth_password(&sess, "user", "pass");
        else if (strstr(methods, "keyboard-interactive"))
            deuce_ssh_auth_keyboard_interactive(&sess, "user", "pass");
    }

    /* 6. Open channel and execute command */
    struct deuce_ssh_channel_s ch;
    deuce_ssh_conn_open_session(&sess, &ch, 0);
    deuce_ssh_conn_request_exec(&sess, &ch, "echo hello");

    /* 7. Read output */
    uint8_t msg_type;
    uint8_t *data;
    size_t data_len;
    while (deuce_ssh_conn_recv(&sess, &ch, &msg_type, &data, &data_len) == 0) {
        if (msg_type == SSH_MSG_CHANNEL_DATA)
            write(STDOUT_FILENO, data, data_len);
        else if (msg_type == SSH_MSG_CHANNEL_EOF ||
                 msg_type == SSH_MSG_CHANNEL_CLOSE)
            break;
        /* Replenish window when low */
        if (ch.local_window < 0x100000)
            deuce_ssh_conn_send_window_adjust(&sess, &ch, 0x200000);
    }

    /* 8. Clean up */
    deuce_ssh_conn_close(&sess, &ch);
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
    deuce_ssh_session_init(&sess);

    /* 3. Load or generate a host key */
    ssh_ed25519_generate_key(&sess);
    /* Or: ssh_ed25519_load_key_file(&sess, "/path/to/host_key.pem"); */

    /* 4. If offering DH-GEX, provide a group selection callback */
    struct deuce_ssh_dh_gex_provider provider = {
        .select_group = my_group_selector,
        .cbdata = NULL,
    };
    deuce_ssh_dh_gex_set_provider(&sess, &provider);

    /* 5. Handshake */
    deuce_ssh_transport_version_exchange(&sess);
    deuce_ssh_transport_kexinit(&sess);
    deuce_ssh_transport_kex(&sess);
    deuce_ssh_transport_newkeys(&sess);

    /* 6. Handle authentication (server-side, using send/recv directly) */
    /* Receive SSH_MSG_SERVICE_REQUEST, send SERVICE_ACCEPT */
    /* Receive USERAUTH_REQUEST, validate, send SUCCESS or FAILURE */
    /* See server.c for a complete example */

    /* 7. Handle channels (server-side) */
    /* Receive CHANNEL_OPEN, send CHANNEL_OPEN_CONFIRMATION */
    /* Receive CHANNEL_REQUEST "exec", send CHANNEL_SUCCESS */
    /* Send CHANNEL_DATA with response */
    /* Send exit-status, CHANNEL_EOF, CHANNEL_CLOSE */

    /* 8. Clean up */
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

## Thread Safety

The library supports one transmit thread and one receive thread
operating concurrently on the same session.  The transport layer
uses separate `tx_mtx` and `rx_mtx` mutexes:

- `send_packet` holds `tx_mtx` — multiple senders are serialized
- `recv_packet` holds `rx_mtx` — multiple receivers are serialized
- Send and receive never block each other

The typical pattern:

1. Single-threaded handshake (version exchange → KEXINIT → KEX → NEWKEYS → auth → channel open)
2. Spawn a receive thread that calls `deuce_ssh_conn_recv()` in a loop
3. Main thread sends data via `deuce_ssh_conn_send_data()`

## Optional Callbacks

Set these on the session struct before or after init:

```c
/* SSH_MSG_DEBUG notification */
sess.debug_cb = my_debug_handler;
sess.debug_cbdata = my_context;

/* SSH_MSG_UNIMPLEMENTED notification */
sess.unimplemented_cb = my_unimpl_handler;
sess.unimplemented_cbdata = my_context;
```

`send_packet` accepts an optional `uint32_t *seq_out` parameter
that returns the packet's sequence number, which can be correlated
with UNIMPLEMENTED notifications.

## Error Handling

All functions return 0 on success, negative `DEUCE_SSH_ERROR_*` codes
on failure.  The transport layer sends `SSH_MSG_DISCONNECT` on
protocol errors (negotiation failure, MAC mismatch) before returning.

Received `SSH_MSG_DISCONNECT` from the peer returns
`DEUCE_SSH_ERROR_TERMINATED`.  Transport messages (`IGNORE`, `DEBUG`,
`UNIMPLEMENTED`) are handled transparently by `recv_packet` and never
returned to the caller.

## Packet Buffer Size

Pass 0 to `deuce_ssh_session_init()` for the RFC minimum (33280 bytes).
For larger payloads, pass a bigger value — the library allocates
per-session tx, rx, and MAC scratch buffers at that size.

## Files

```
deucessh.h          Main header (session, errors, callbacks)
ssh-arch.h/.c       RFC 4251 data types (parse/serialize)
ssh-trans.h/.c      RFC 4253 transport (packets, KEX, keys)
ssh-auth.h/.c       RFC 4252 authentication (client-side)
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
