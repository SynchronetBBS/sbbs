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
- RFC 8308 — Base extension negotiation and `server-sig-algs` ([scope](docs/release-1.0-evidence/ext-info-implementation.md))
- RFC 8332 — RSA SHA-256 and SHA-512 Signature Algorithms
- RFC 8709 — Ed25519 Host Key Algorithm
- RFC 8731 — curve25519-sha256 Key Exchange
- RFC 8268 — Fixed-group SHA-2 Diffie-Hellman Key Exchange
- [RFC 9941](standards/rfc9941.txt) — sntrup761x25519-sha512 Key Exchange
- [RFC 10042](standards/rfc10042.txt) — mlkem768x25519-sha256 Hybrid Key Exchange

Built-in algorithms use standardized names; no proprietary extensions are implemented.

## Dependencies

- **Crypto backend**: Botan 3.13.0+ (preferred) or OpenSSL 3.0+
  (libcrypto only — no libssl)
- **C11 threads** (`<threads.h>`, `<stdatomic.h>`) — used internally only, not exposed in public headers

Botan-backed binaries require an ABI-compatible Botan library. Compatible
updates do not require an exact release-version match. Rebuild DeuceSSH when
Botan's ABI changes; the source/API minimum remains 3.13.0.

The version floors above specify source/API compatibility, not a security patch
level. Deploy a currently supported, security-patched provider (including a
vendor-maintained build where applicable). Upstream OpenSSL 3.0 no longer has
community support; see the [OpenSSL support policy](https://openssl-library.org/policies/releasestrat/),
[OpenSSL advisories](https://openssl-library.org/news/vulnerabilities/) and
[Botan advisories](https://botan.randombit.net/security.html).

Both backends use the bundled C17 ML-KEM-768 implementation. The selected
backend supplies randomness, X25519 and the SSH transcript hash.

## Building

CMake selects C17 when supported. On macOS x86 only, a compiler without
C17 mode may use C11 instead. Library targets, tests, and capability probes
use the same dialect. When `timespec_get()` / `TIME_UTC` are unavailable in the
build's feature namespace, a private POSIX shim uses
`clock_gettime(CLOCK_REALTIME)` instead (including on macOS 10.14). Atomics
remain required; selecting C11 does not supply missing SDK functions.

```sh
cmake -S . -B build
cmake --build build -j8
```

Produces `libdeucessh.a` (static) and `libdeucessh.so` (shared).
The v1.0 package version is 1.0.0 and the shared-library ABI major is 1.
All public functions have interface version 1.0: ELF uses `DEUCESSH_1.0`;
macOS and Windows bind to names ending in `_v1_0`. Ordinary C calls keep their
existing spelling through the public headers. Install `deucessh-abi.h` with
the other headers, and rebuild pre-1.0 consumers. Link DeuceSSH explicitly at
the final consumer link so its ABI requirements are recorded. See the
[ABI policy](docs/audit-dsohowto.md#symbol-versioning-plan).
On MSVC the static archive is `deucessh-static.lib`; `deucessh.lib` is the DLL
import library. The CMake static/shared target names remain unchanged.
`cmake --install build --prefix <prefix>` installs headers, libraries, CMake/
pkg-config metadata and the project/bundled-code notices in
`share/licenses/deucessh/LICENSE.md` (using the configured install data directory).

ELF shared builds apply `-z nodlopen` when the linker supports it. Link the
library as a program dependency; late loading with `dlopen` is unsupported in
that configuration. macOS and Windows do not use this ELF flag.


Use `-S . -B build` to prevent cmake from discovering the parent
`src/CMakeLists.txt`.

## Built-in Algorithms

| Category | Algorithms |
|----------|-----------|
| Key Exchange | `mlkem768x25519-sha256`, `sntrup761x25519-sha512`, `curve25519-sha256`, `diffie-hellman-group-exchange-sha256`, `diffie-hellman-group16-sha512`, `diffie-hellman-group14-sha256` |
| Host Key | `ssh-ed25519`, `rsa-sha2-256`, `rsa-sha2-512` |
| Encryption | `aes256-ctr`, `aes128-cbc`*, `none` |
| MAC | `hmac-sha2-256`, `hmac-sha2-512`, `none` |
| Compression | `none` |

All algorithms support both client and server side.

\* `aes128-cbc` is provided **only for client compatibility with servers
that offer no other cipher** (notably Mystic BBS).  CBC over SSH is
weakened by predictable-plaintext attacks (CVE-2008-5161); do not
register it on servers, and on clients pair it with the per-session
filter API below so it isn't offered on connections that don't need
it.

## Quick Start — Client

[examples/client.c](examples/client.c) is the complete POSIX client example.
It requires the trusted host fingerprint and a dedicated password descriptor;
stdin remains available for channel data. See the invocation under
[Host Key Verification](#host-key-verification-client-only).

For an application integration, configure algorithms and I/O before creating
sessions, check `dssh_session_init()` for NULL, and install the session's
termination and host-key verification callbacks before handshaking. The
[registration example](#algorithm-registration) includes both fixed SHA-2 DH
groups alongside the hybrid and Curve25519 methods. Select an encrypted cipher
and an integrity MAC for credential authentication.

```c
#include "deucessh.h"
#include "deucessh-auth.h"
#include "deucessh-conn.h"

/* sess has application I/O, termination and host-trust callbacks configured. */
int start_client(dssh_session sess, const char *username, const char *password)
{
    int ret = dssh_transport_handshake(sess);
    if (ret != 0)
        return ret;
    ret = dssh_auth_password(sess, username, password, NULL, NULL);
    if (ret != DSSH_AUTH_SUCCESS)
        return ret;
    return dssh_session_start(sess);
}
```

A PARTIAL or CONTINUE result leaves authentication incomplete; inspect the
current methods and choose another action before starting the connection layer.
After successful start, open channels with `dssh_chan_open()` or
`dssh_chan_zc_open()`. The application owns session cleanup on every outcome.

## Quick Start — Server

[examples/server.c](examples/server.c) and
[examples/kbi-game-server.c](examples/kbi-game-server.c) provide complete POSIX
I/O, key provisioning, authentication, channel setup and cleanup examples.
Build them with `cmake --build build --target server kbi-game-server`.

Register algorithms and load/generate host keys before the first session-init
attempt. Check every registration, key and callback setter result. Authentication
callbacks authorize credentials against the application's account policy;
channel callbacks separately authorize the requested shell/exec/subsystem.

```c
#include "deucessh.h"
#include "deucessh-auth.h"
#include "deucessh-conn.h"

/* sess has application I/O and termination callbacks configured.
 * cbs contains the application's credential-checking callbacks. */
int start_server(dssh_session sess, const struct dssh_auth_server_cbs *cbs)
{
    int ret = dssh_transport_handshake(sess);
    if (ret != 0)
        return ret;
    ret = dssh_auth_server(sess, cbs, NULL, NULL);
    if (ret != 0)
        return ret;
    return dssh_session_start(sess);
}
```

After successful start, `dssh_chan_accept()` performs callback-driven channel
setup. Dispose each owned channel with `dssh_chan_close()` and clean up the
session after application workers have stopped using it.

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

Client sessions also **require** a host key verification callback
(see [Host Key Verification](#host-key-verification-client-only)
below).  The handshake will not proceed without it.

Callbacks are registered globally once.  Per-session differentiation
is via the `cbdata` pointers.

An optional fourth callback (`extra_line_cb`) is called for non-SSH
lines received before the version string during version exchange.

**Important:** When `dssh_session_is_terminated(sess)` returns true,
I/O callbacks **must** return a negative error code within a reasonable
period.  Failure to do so will cause `dssh_session_cleanup()` to hang
on thread join.  Use the terminate callback to be notified:

Transport I/O, line-receive, extra-line, and gather callbacks run with no
DeuceSSH mutex held.  While one of these callbacks is active, it may call only
`dssh_session_is_terminated()` on the session passed to it.  All other
`dssh_*` calls are forbidden and are rejected where the API has an error return.

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

## Host Key Verification (Client Only)

Client sessions **must** set a host key verification callback before
`dssh_transport_handshake()`.  The callback fires during KEX with the
server's host key information:

```c
#include <string.h>
#include "deucessh.h"
#include "deucessh-crypto.h"

static dssh_hostkey_decision
my_hostkey_verify(const char *algo_name, unsigned int key_bits,
    const uint8_t *sha256_hash, const uint8_t *key_blob,
    size_t key_blob_len, void *cbdata)
{
    const char *expected = (const char *)cbdata; /* trusted SHA256: fingerprint */
    if (expected == NULL || strlen(expected) != 50 || algo_name == NULL
        || sha256_hash == NULL || key_blob == NULL || key_blob_len == 0)
        return DSSH_HOSTKEY_REJECT;

    /* Sizes are algorithm-specific: 256 for Ed25519, modulus bits for RSA. */
    bool rsa = strcmp(algo_name, "rsa-sha2-256") == 0
        || strcmp(algo_name, "rsa-sha2-512") == 0;
    if (rsa && key_bits < 2048)
        return DSSH_HOSTKEY_REJECT;

    char encoded[45];
    int written = dssh_base64_encode(sha256_hash, 32, encoded, sizeof(encoded));
    if (written != 44 || encoded[43] != '=')
        return DSSH_HOSTKEY_REJECT;
    char fingerprint[51] = "SHA256:";
    memcpy(&fingerprint[7], encoded, 43);
    fingerprint[50] = 0;
    return strcmp(expected, fingerprint) == 0
        ? DSSH_HOSTKEY_ACCEPT : DSSH_HOSTKEY_REJECT;
}

/* expected_hostkey is a validated pin from trusted configuration.
 * Keep its storage alive until session cleanup and check the setter result. */
int ret = dssh_session_set_hostkey_verify_cb(sess, my_hostkey_verify, expected_hostkey);
if (ret != 0) {
    /* Handle setup failure before starting the handshake. */
}
```

The callback checks the key again during rekey. The expected fingerprint is
`SHA256:` followed by 43 unpadded base64 characters. Obtain it through trusted
configuration before connecting. The POSIX client example takes it as its first
argument:

```sh
client 'SHA256:<trusted fingerprint>' username password-fd [host] [port] [command]
```

Supply a dedicated readable descriptor numbered at least 3, for example:

```sh
client 'SHA256:<trusted fingerprint>' alice 3 example.net 22 'uname -a' 3<password-file
```

The example reads one password line before resolving the host, then closes its
descriptor. LF ends the line and an immediately preceding CR is removed; EOF
also ends input. Empty passwords are allowed. Embedded NUL bytes and passwords
longer than 16 KiB are rejected without truncation. Other bytes are preserved.
The application clears and frees its password storage after authentication or
an error. Stdin remains available for channel input. The parent supplies and
owns its credential source; there is no password argument or fallback source.

If the callback is not set on a client session,
`dssh_transport_handshake()` returns `DSSH_ERROR_INIT`.  Server
sessions ignore this callback (the server's own host key is not
verified this way).

## Version String

Customize the SSH identification string (RFC 4253 s4.2):

```c
dssh_transport_set_version("MyApp_1.0", "FreeBSD");
/* Produces: SSH-2.0-MyApp_1.0 FreeBSD\r\n */
```

Must be called before `dssh_session_init()`.  Pass NULL for
`software_version` to keep the library default; pass NULL for
`comment` to omit it.  The resulting line must fit in 255 bytes.
Software-version tokens must contain printable non-space US-ASCII and exclude
minus (`-`); peer tokens follow the same rule. The built-in token uses
`DeuceSSH_<version>`. Comments may contain spaces and minus signs.

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
const char *enc  = dssh_transport_get_enc_name(sess, DSSH_CLIENT_TO_SERVER);
const char *mac  = dssh_transport_get_mac_name(sess, DSSH_SERVER_TO_CLIENT);
const char *comp = dssh_transport_get_comp_name(sess, DSSH_SERVER_TO_CLIENT);
```

Use `DSSH_CLIENT_TO_SERVER` or `DSSH_SERVER_TO_CLIENT` for each cipher,
MAC or compression query, regardless of the application's endpoint role.
The language field functions use the same `enum dssh_direction`.

Each call snapshots the latest negotiated selection. During rekey a selection
can be visible before its keys become active; separate calls are separate
snapshots. Algorithm names are borrowed from process-lifetime registered
definitions. The remote version string is session-owned. Coordinate all calls
using the handle with session cleanup.

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

`dssh_auth_begin(sess, username)` optionally starts authentication without
credentials. It may authenticate immediately or discover the methods needed
to continue. An application that already knows its desired method can call
password, public-key or keyboard-interactive directly; those calls never
implicitly perform discovery.

All four actions return the same outcomes:

| Outcome | Meaning |
| --- | --- |
| `DSSH_AUTH_SUCCESS` | Authentication is complete; the connection can start. |
| `DSSH_AUTH_PARTIAL` | This factor succeeded; another authentication is needed. |
| `DSSH_AUTH_CONTINUE` | Authentication is incomplete; this action accepted no factor. |
| Negative `DSSH_ERROR_*` | An operation error. |

After PARTIAL or CONTINUE, read the current list with
`dssh_auth_get_methods(sess, methods, methods_sz)` and choose the next method.
The getter performs no I/O and cannot authenticate. It reports the last complete
reply from any method, not just the initial discovery response. Banners and
interactive challenges leave that snapshot unchanged; SUCCESS publishes an empty
list. Failed operations preserve the prior completed snapshot.

Pass NULL output or zero capacity to query the required bytes including NUL.
A short buffer returns TOOLONG unchanged; an empty list needs one byte and does
not itself mean authentication succeeded. Before any completed reply the getter
returns INIT. Reuse a caller-owned buffer, growing it only when the required size
increases. The [client example](examples/client.c) implements this flow, refreshes
the list after each action and attempts each supported credential method once.


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

Pass a separate zero-initialized `password_change_state` as callback data for
each authentication operation, and keep it alive until `dssh_auth_password()`
returns. The same storage is reused for repeated password-change requests.
The library initializes `out` to zero and borrows its bytes until release,
including when the callback or a later operation fails. The producer owns and
clears its storage; the library never frees it.

```c
struct password_change_state {
    char storage[256];
    bool in_use;
};

static void
release_new_password(void *owner)
{
    struct password_change_state *state = owner;
    dssh_cleanse(state->storage, sizeof(state->storage));
    state->in_use = false;
}

static int
my_passwd_change_cb(const uint8_t *prompt, size_t prompt_len,
    const uint8_t *language, size_t language_len,
    struct dssh_bytes_result *out, void *cbdata)
{
    (void)language;
    (void)language_len;
    struct password_change_state *state = cbdata;
    if (state == NULL || state->in_use)
        return DSSH_ERROR_INVALID;
    if (prompt_len > 0 && fwrite(prompt, 1, prompt_len, stdout) != prompt_len)
        return DSSH_ERROR_INIT;
    if (fgets(state->storage, sizeof(state->storage), stdin) == NULL) {
        dssh_cleanse(state->storage, sizeof(state->storage));
        return DSSH_ERROR_INIT;
    }
    size_t len = strlen(state->storage);
    if (len > 0 && state->storage[len - 1] == '\n')
        len--;
    const uint8_t *bytes = (const uint8_t *)state->storage;
    state->in_use = true;
    out->bytes.data = bytes;
    out->bytes.len = len;
    out->lease.release = release_new_password;
    out->lease.owner = state;
    return 0;
}
```

### Public Key

```c
#include "deucessh-algorithms.h"

/* Register ssh-ed25519 during global setup, checking its return there.
 * After creating this client session and completing its handshake: */
int ret = dssh_session_load_client_key_file(sess, "ssh-ed25519",
    "/path/to/id_ed25519", NULL, NULL);
if (ret == 0)
    ret = dssh_auth_publickey(sess, "user", "ssh-ed25519");
/* Handle ret using the client authentication outcomes described above. */
```

The function signs the session-bound authentication data (session ID
+ request fields) per RFC 4252 s7 and sends the signed request.
Works with any registered key algorithm that supports signing. The application
chooses the order of methods and keys through explicit authentication calls.

Each client session can hold one local key per algorithm. When no local key is
installed, authentication uses that algorithm's global key if one is present.
A failed or refused local credential never triggers an automatic fallback.
Global private keys are optional for client-only applications; server handshakes
need an available host key. Algorithm registration remains global setup, and
global keys freeze at the first session-init attempt.

Load or replace a local key before authentication succeeds, including between
unsuccessful attempts. `dssh_session_clear_client_key(sess, algo_name)` removes
it and restores global fallback. Failed replacement preserves the old key.
`dssh_session_generate_client_key()` generates a local key, using 256 bits for
Ed25519 or the requested RSA modulus size. The session releases local keys on
replacement, clearing or cleanup. Serialize key operations with authentication,
handshake and lifecycle operations on that session; different sessions can run
independently. Callback reentry on the same session is rejected.

`dssh_session_get_client_key_pub_str()`, `dssh_session_save_client_pub_file()`
and `dssh_session_save_client_key_file()` export the same local/global key
selected for authentication. Exports also work after authentication. They use
the existing public-text size, PEM password and private-file save conventions.
Builtin loading, generation and private export support `ssh-ed25519`,
`rsa-sha2-256` and `rsa-sha2-512`.

Custom modules install a context with `dssh_session_set_client_key_ctx()`.
Its descriptor and callbacks remain registered for process lifetime; each local
context uses a producer-owned release lease. Successful installation moves and
clears the caller's lease; failure leaves it with the caller. Release runs once
on replacement, clearing or cleanup without library locks, with the usual
result-release restrictions. Private allocation ownership stays with the module.

### Keyboard-Interactive

`dssh_auth_keyboard_interactive()` passes borrowed name, instruction and
prompt spans to the application (RFC 4256). Return a `dssh_auth_responses`
bundle with one `dssh_bytes` entry per prompt and a release lease covering
all output storage. The library releases a present lease on every outcome,
including callback errors and invalid metadata. It never clears or frees
application storage.

This example accepts one prompt and retains its answer buffer between rounds.
Use a separate state for each authentication call; the application can support
larger prompt sets with retained arrays, as in [examples/client.c](examples/client.c).

```c
struct kbi_answer_state {
    uint8_t answer[256];
    struct dssh_bytes item;
};

/* Application UI: respect echo, return 0 and an explicit byte length,
 * or a negative error. Never write beyond capacity. */
int read_kbi_answer(uint8_t *buffer, size_t capacity, size_t *length, bool echo);

static void
release_kbi_answer(void *owner)
{
    struct kbi_answer_state *state = owner;
    dssh_cleanse(state->answer, sizeof(state->answer));
    state->item = (struct dssh_bytes){0};
}

static int
my_kbi_prompt(const uint8_t *name, size_t name_len,
    const uint8_t *instruction, size_t instruction_len,
    uint32_t num_prompts, const uint8_t **prompts,
    const size_t *prompt_lens, const bool *echo,
    struct dssh_auth_responses *out, void *cbdata)
{
    (void)name;
    (void)name_len;
    (void)instruction;
    (void)instruction_len;
    struct kbi_answer_state *state = cbdata;
    if (num_prompts > 1)
        return DSSH_ERROR_TOOLONG;
    if (num_prompts == 0)
        return 0; /* An empty response bundle is valid. */
    out->lease.release = release_kbi_answer;
    out->lease.owner = state;
    if (fwrite(prompts[0], 1, prompt_lens[0], stdout) != prompt_lens[0])
        return DSSH_ERROR_INIT;
    if (fflush(stdout) != 0)
        return DSSH_ERROR_INIT;
    size_t length = 0;
    int ret = read_kbi_answer(state->answer, sizeof(state->answer), &length, echo[0]);
    if (ret < 0)
        return ret;
    if (ret != 0 || length > sizeof(state->answer))
        return DSSH_ERROR_INVALID;
    state->item = (struct dssh_bytes){state->answer, length};
    out->items = &state->item;
    out->count = 1;
    return 0;
}

/* Usage: */
struct kbi_answer_state answers = {0};
dssh_auth_keyboard_interactive(sess, "user", my_kbi_prompt, &answers);
```

Returned arrays and bytes must remain valid until release, including on errors.
Clear actual owned capacity in the producer's release callback; do not use a
reported result length to decide how much storage to clear. Callback input spans
are borrowed only for that callback and cannot back its returned result.

## Authentication — Server Side

`dssh_auth_server()` drives the server-side authentication loop.
It handles SERVICE_REQUEST/ACCEPT, receives USERAUTH_REQUEST messages,
dispatches to your callbacks, and sends SUCCESS/FAILURE/CHANGEREQ/PK_OK
responses.

```c
struct dssh_auth_server_cbs cbs = {
    .methods_str = "publickey,password,keyboard-interactive",
    .request_cb = my_request_cb,             /* optional; see below */
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

Without `request_cb`, the first request binds the complete username for the
login exchange; a later change disconnects. A missing `none_cb` automatically
rejects the normal discovery probe. Simple password/public-key servers need
neither callback.

The optional request hook runs once per USERAUTH_REQUEST, including unknown
methods and replacement interactive attempts. It runs before method handling,
but never for an INFO_RESPONSE round. Reset accumulated factors when
`new_identity` is true, and reset attempt-local dialogue on every invocation.
The hook returns zero to continue; this grants no authentication. A negative
error or invalid positive result ends the session. Updated `methods_str` values
are validated before method handling. Inputs are borrowed only during the hook.
The successful username output is capped at 255 bytes and the caller's capacity;
callbacks and identity comparisons always use the complete username bytes.

Applications prepare textual usernames, passwords and messages in the wire
encoding required by their SSH fields (normally UTF-8), converting from local
encodings as needed. The library preserves callback byte spans without Unicode
normalization; account mapping, local password conversion and display validation
belong to the application. Channel stream bytes have no imposed text encoding.

### Callback Return Values

These values apply to credential callbacks, not the request hook.

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
    struct dssh_bytes_result *change_prompt, void *cbdata)
{
    /* Validate credentials.
     * To request a password change, set change_prompt->bytes.data/len
     * and its producer-owned release lease, then return
     * DSSH_AUTH_CHANGE_PASSWORD. Static prompt storage may use a no-op
     * release callback. The library never clears or frees the result. */
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
    struct dssh_bytes_result *change_prompt, void *cbdata)
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

Called in a loop. The first call has `num_responses=0` and `responses=NULL`.
Subsequent calls contain the client's answers. Return `DSSH_AUTH_KBI_PROMPT`
with a `dssh_auth_challenge`, or a final authentication result. The challenge
contains explicit name/instruction spans and an array of prompt text/echo pairs.
One release lease covers the entire bundle. Static storage uses a no-op release.

```c
struct kbi_server_state {
    bool prompted;
};

/* Install this as request_cb with the same cbdata as my_kbi_cb.
 * This single-factor example has no completed factors to reset. */
static int
my_request_cb(const uint8_t *username, size_t username_len,
    const uint8_t *method, size_t method_len, bool new_identity, void *cbdata)
{
    (void)username;
    (void)username_len;
    (void)method;
    (void)method_len;
    (void)new_identity;
    struct kbi_server_state *state = cbdata;
    state->prompted = false;
    return 0;
}

static void
release_static_challenge(void *owner)
{
    (void)owner;
}

static int
my_kbi_cb(const uint8_t *username, size_t username_len,
    uint32_t num_responses, const uint8_t **responses,
    const size_t *response_lens, struct dssh_auth_challenge *out,
    void *cbdata)
{
    (void)username;
    (void)username_len;
    struct kbi_server_state *state = cbdata;
    if (!state->prompted) {
        static const uint8_t name[] = "Auth";
        static const uint8_t instruction[] = "Enter your token";
        static const uint8_t text[] = "Token: ";
        static const struct dssh_auth_prompt prompts[] = {
            {{text, sizeof(text) - 1}, false}
        };
        state->prompted = true;
        out->name = (struct dssh_bytes){name, sizeof(name) - 1};
        out->instruction = (struct dssh_bytes){instruction, sizeof(instruction) - 1};
        out->prompts = prompts;
        out->count = 1;
        out->lease.release = release_static_challenge;
        return DSSH_AUTH_KBI_PROMPT;
    }
    if (num_responses == 1 && response_lens[0] == 6
        && memcmp(responses[0], "secret", 6) == 0)
        return DSSH_AUTH_SUCCESS;
    return DSSH_AUTH_FAILURE;
}
```

The library releases a present lease after its final read on every outcome.
It never clears or frees producer storage. Text spans do not require NUL
terminators. Name and instruction may be empty; prompt text must be nonempty.
A zero-prompt challenge still requires a response, so track rounds in application
state and reset it in `request_cb`, rather than treating every zero-response
callback as the first call.

## Algorithm Registration

Call `dssh_register_*()` functions before any session is created.
Registration order determines negotiation preference — first registered
is most preferred.

```c
/* Order = preference: post-quantum hybrid first, then classical.
 * Propagate any failure from the application's setup function. */
int ret = dssh_register_mlkem768x25519_sha256();
if (ret == 0)
    ret = dssh_register_sntrup761x25519_sha512();
if (ret == 0)
    ret = dssh_register_curve25519_sha256();
if (ret == 0)
    ret = dssh_register_dh_gex_sha256();
if (ret == 0)
    ret = dssh_register_dh_group16_sha512();
if (ret == 0)
    ret = dssh_register_dh_group14_sha256();
if (ret != 0)
    return ret;
```

The fixed methods use RFC 3526 group16 (4096 bits, SHA-512) and group14
(2048 bits, SHA-256). They use the standard fixed-group DH exchange defined by
RFC 8268, without a DH-GEX group provider. Host-key fingerprints remain SHA-256
for both methods. Registration stays application controlled; these functions
do not enable any algorithm automatically.

You can register your own custom algorithm modules by implementing
the callback signatures defined in the public module headers and
calling the corresponding `dssh_transport_register_*()` function.

### Registration Rules

- Call `dssh_register_*()` before any `dssh_session_init()`.
- After the first session init, registration is locked (`DSSH_ERROR_TOOLATE`).
- Names must be 1–64 printable ASCII characters (no spaces, no control chars).
- The `next` pointer must be NULL (the library manages the linked list).
- Registered descriptors, names, configuration and callback code remain borrowed
  for process lifetime; keep their storage and code alive. Do not modify registered
  records or names directly. Use configuration setters before the first session;
  the library manages links and freezes configuration pointers at session creation.
  Mutable producer state referenced through configuration follows its module's
  concurrency and lifetime contract. Successful registration neither transfers
  allocation ownership nor copies the descriptor.
- Registration order determines negotiation preference.

### Per-Session Algorithm Filters

The global registry is "register every algorithm you might ever need".
A per-session whitelist constrains a single session to a subset of
those algorithms (and reorders the preference list) — useful when one
connection wants different algorithms than another.

```c
DSSH_PUBLIC int dssh_session_set_kex_filter(dssh_session sess,
    const char * const *names, size_t count);
DSSH_PUBLIC int dssh_session_set_key_algo_filter(dssh_session sess,
    const char * const *names, size_t count);
DSSH_PUBLIC int dssh_session_set_enc_filter(dssh_session sess,
    const char * const *names, size_t count);
DSSH_PUBLIC int dssh_session_set_mac_filter(dssh_session sess,
    const char * const *names, size_t count);
DSSH_PUBLIC int dssh_session_set_comp_filter(dssh_session sess,
    const char * const *names, size_t count);
```

- Filter order = negotiation preference order.
- `count == 0` or `names == NULL` clears any prior filter (= use
  every registered algorithm in registration order).
- Names not registered are silently skipped — no error.
- A name containing `,` returns `DSSH_ERROR_INVALID`.
- Must be called before `dssh_session_start()`; returns
  `DSSH_ERROR_TOOLATE` afterwards.
- Caller-owned input: the library copies the strings; the caller's
  array and strings need not outlive the call.

Example — a client that registers both `aes256-ctr` and `aes128-cbc`
globally, but only offers `aes128-cbc` on a connection to a Mystic
BBS server:

```c
/* Once at startup */
dssh_register_aes256_ctr();
dssh_register_aes128_cbc();

/* Per connection */
dssh_session sess = dssh_session_init(true, 0);

if (connecting_to_mystic) {
    const char *enc[] = { "aes128-cbc" };
    dssh_session_set_enc_filter(sess, enc, 1);
} else {
    const char *enc[] = { "aes256-ctr" };
    dssh_session_set_enc_filter(sess, enc, 1);
}

/* ... set callbacks, start handshake ... */
```

## Language Preferences

Include `deucessh-lang.h` to set and read the two KEXINIT language fields.
Each session has independent client-to-server and server-to-client lists.
The setters copy arrays of language tags, preserve their order and case, and
clear a field when the array is NULL or its count is zero.

Getters copy comma-separated, NUL-terminated strings into application buffers.
Their return value includes the NUL. A NULL buffer or zero capacity queries
the required size. An undersized buffer returns `DSSH_ERROR_TOOLONG` and
retains its previous contents. Peer reads return `DSSH_ERROR_INIT` until a
complete peer KEXINIT has been validated.

An application with known language preferences can configure them before its
handshake and read the peer's corresponding field afterward.

```c
#include "deucessh.h"
#include "deucessh-lang.h"

static int64_t
handshake_with_language_preferences(dssh_session sess, char *peer, size_t capacity)
{
	const char *names[] = {"en-US", "fr"};
	int ret = dssh_session_set_languages(sess, DSSH_SERVER_TO_CLIENT, names, 2);
	if (ret == 0)
		ret = dssh_transport_handshake(sess);
	if (ret < 0)
		return ret;
	return dssh_session_get_peer_languages(sess, DSSH_SERVER_TO_CLIENT,
	    peer, capacity);
}
```

`dssh_session_get_languages()` reads a configured local field. Setters can
update that configuration during a session. Both local fields are captured
together when an outgoing KEXINIT is built, and a later update applies to a
subsequent offer. Peer getters report the latest validated peer KEXINIT,
including rekey updates. The application chooses how to use the preferences.

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
    enc->bytes_per_key = UINT64_C(1) << 36; /* 128-bit block limit */
    enc->blocksize = 16;  /* cipher block size in bytes */
    enc->key_size = 32;   /* key size in bytes */
    memcpy(enc->name, name, sizeof(name));
    int ret = dssh_transport_register_enc(enc);
    if (ret < 0)
        free(enc);
    return ret;
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
    int ret = dssh_transport_register_mac(mac);
    if (ret < 0)
        free(mac);
    return ret;
}
```

The `struct dssh_mac_s` (typedef `dssh_mac`) is defined in `deucessh-mac.h`.

### Compression Module

Compression modules provide five callbacks declared in `deucessh-comp.h`:

```c
int init(bool compress, dssh_comp_ctx **ctx);
int bound(size_t input_len, size_t *output_bound, dssh_comp_ctx *ctx);
int compress(const uint8_t *input, size_t input_len,
    uint8_t *output, size_t output_capacity, size_t *output_len,
    dssh_comp_ctx *ctx);
int uncompress(const uint8_t *input, size_t input_len,
    uint8_t *output, size_t output_capacity, size_t *output_len,
    dssh_comp_ctx *ctx);
void cleanup(dssh_comp_ctx *ctx);
```

`init` creates an independent context for each direction after key exchange;
`compress` is true for local transmit. On same-module rekey, `*ctx` contains
its existing context: reset stream history in place and reuse allocations.
If replacement is necessary, the provider releases its old context itself.
On failure, leave a valid cleanup target or NULL after releasing all resources.
The transport uses each old context through NEWKEYS before resetting it.

`bound` supplies a nondecreasing maximum compressed length, including flush
overhead, valid for every input and all future states of that context.
Compression may increase payload length. The transport computes a safe input
capacity from this bound before exposing writable packet buffers. Return
`DSSH_ERROR_TOOLONG` if the bound cannot be represented in `size_t`.

Transforms receive disjoint library buffers borrowed only for that call. They
return zero and set `*output_len <= output_capacity`, or a negative error;
positive results are invalid. The receive output capacity is fixed by the
session packet limit. A transform failure terminates the session because the
stream may already have advanced. Callbacks run without library mutexes and
must not reenter session/channel APIs. Clear private histories before releasing
them. Provider allocations remain owned and released by the provider; library
scratch buffers are retained and reused for the session.

The optional [zlib module example](examples/zlib-comp.c) demonstrates the full
contract, checked expansion bounds, packet flushes, private allocation cleanup,
and stream reset without reallocating dictionaries. Compile that file in your
application, add the public DeuceSSH include directory and link zlib; include
[its example header](examples/zlib-comp.h) and call
`dssh_example_register_zlib_comp()` before creating a session. It registers
standard `zlib`; registration order determines negotiation preference.
The example and zlib are not part of libdeucessh, its installed headers,
exported API or dependency list. Only `none` is built into the library.

The optional example can also be built as a separate CMake target:

```sh
cmake -S . -B build-example -DDEUCESSH_BUILD_ZLIB_EXAMPLE=ON
cmake --build build-example --target dssh_example_zlib
```


### Host Key Module

Host key modules implement sign, verify, public key export, and
optional key management (generate, load, save).  They use the `ctx`
field on the registered struct to store key material.

```c
#include <stdlib.h>
#include <string.h>
#include "deucessh.h"
#include "deucessh-algorithms.h"
#include "deucessh-key-algo.h"

static int my_sign(struct dssh_bytes_result *out,
    const uint8_t *data, size_t data_len, dssh_key_algo_ctx *ctx)
{
    /* Sign into producer-owned storage. Set out->bytes to the nonempty
     * SSH-encoded blob, and out->lease to its producer release callback/owner.
     * Retain capacity for reuse; each live result needs independent storage.
     * Return 0 on success or a negative error. Publish partial resources
     * through the lease on error, or release them before returning.
     * The consumer releases every present lease; it never frees the blob.
     * Implement the algorithm here; this outline reports unavailable. */
    (void)out;
    (void)data;
    (void)data_len;
    (void)ctx;
    return DSSH_ERROR_INIT;
}

static int my_verify(const uint8_t *key_blob, size_t key_blob_len,
    const uint8_t *sig_blob, size_t sig_blob_len,
    const uint8_t *data, size_t data_len)
{
    /* Verify that sig_blob is a valid signature of data under
     * the public key in key_blob. Return 0 only after verification. */
    (void)key_blob;
    (void)key_blob_len;
    (void)sig_blob;
    (void)sig_blob_len;
    (void)data;
    (void)data_len;
    return DSSH_ERROR_INIT;
}

static int my_pubkey(const uint8_t **out, size_t *outlen,
    dssh_key_algo_ctx *ctx)
{
    /* Set *out to the SSH-encoded public key blob (internal pointer,
     * NOT malloc'd -- caller must not free).
     * Set *outlen to the blob size. Return 0 on success. */
    (void)out;
    (void)outlen;
    (void)ctx;
    return DSSH_ERROR_INIT;
}

static int my_haskey(dssh_key_algo_ctx *ctx)
{
    /* Positive for a usable private key, zero for unavailable, negative error. */
    (void)ctx;
    return 0;
}

static void my_key_cleanup(dssh_key_algo_ctx *ctx)
{
    /* Release this producer's context; keep outstanding result leases valid. */
    (void)ctx;
}

static unsigned int my_key_bits(const uint8_t *key_blob,
    size_t key_blob_len)
{
    /* Return the key strength in bits from the wire-format public
     * key blob.  E.g. RSA: modulus bit count; Ed25519: 256.
     * Return 0 if the blob cannot be parsed. */
    (void)key_blob;
    (void)key_blob_len;
    return 0;
}

int register_my_key_algo(void)
{
    static const char name[] = "my-key-type";
    struct dssh_key_algo_s *ka = malloc(sizeof(*ka) + sizeof(name));
    if (ka == NULL)
        return DSSH_ERROR_ALLOC;
    ka->next = NULL;
    ka->sign = my_sign;
    ka->verify = my_verify;
    ka->pubkey = my_pubkey;
    ka->haskey = my_haskey;
    ka->cleanup = my_key_cleanup;
    ka->key_bits = my_key_bits; /* NULL ok: 0 passed to verify cb */
    ka->ctx = NULL;     /* set by generate/load */
    ka->flags = DSSH_KEY_ALGO_FLAG_SIGNATURE_CAPABLE;
    memcpy(ka->name, name, sizeof(name));
    int ret = dssh_transport_register_key_algo(ka);
    if (ret < 0)
        free(ka);
    return ret;
}
```

The `struct dssh_key_algo_s` (typedef `dssh_key_algo`) is defined in
`deucessh-key-algo.h`.  Key blobs and signature blobs use the SSH wire
format defined in the relevant RFC for the algorithm.  Use
`dssh_key_algo_set_ctx()` (from `deucessh-algorithms.h`) to store key
material on the registration entry.

Signature results start zeroed and remain immutable until their release callback.
The producer knows its actual capacities and clears owned storage before freeing
or reusing it; reported result lengths never determine cleanup size. Consumers
release on every outcome, including malformed metadata and downstream failures,
and after their last copy before packet I/O. A static result also requires an
explicit no-op release. The producer's release code must remain callable while
any result is outstanding. Built-in key modules retain independent output buffers
and preserve already returned signatures through key-context cleanup; this does
not add a registry shutdown or change registration lifetime.

### KEX Module

KEX modules drive the key exchange protocol after KEXINIT negotiation.
The handler receives borrowed inputs through a `const struct dssh_kex_context *`
and returns a `struct dssh_kex_result` containing immutable K/H spans under one
producer-owned lease. Modules need only the installed public headers.

Every registration supplies an initializer, handler and cleanup callback:

```c
#include <stdlib.h>
#include <string.h>
#include "deucessh-kex.h"

/* Application-defined functions implement the actual method and storage. */
int my_method_reset(bool client, void *config, void **state);
int my_method_exchange(const struct dssh_kex_context *ctx,
    struct dssh_kex_result *out);
void my_method_destroy(void *state);

int register_my_kex(void)
{
    static const char name[] = "my-kex-method";
    struct dssh_kex_s *kex = malloc(sizeof(*kex) + sizeof(name));
    if (kex == NULL)
        return DSSH_ERROR_ALLOC;
    kex->next = NULL;
    kex->init = my_method_reset;
    kex->handler = my_method_exchange;
    kex->cleanup = my_method_destroy;
    kex->flags = DSSH_KEX_FLAG_NEEDS_SIGNATURE_CAPABLE;
    kex->config = NULL;
    kex->hash_name = "SHA-256";
    memcpy(kex->name, name, sizeof(name));
    int ret = dssh_transport_register_kex(kex);
    if (ret < 0)
        free(kex);
    return ret;
}
```

`init(client, config, &state)` receives NULL state on the first exchange and the
same module's previous state on rekey. It can reset and reuse allocations; each
exchange must generate fresh ephemeral material. If it replaces state, it must
release the old state itself. Failure leaves NULL or a cleanup-valid state.
Stateless success with NULL is valid. Cleanup accepts NULL and releases session
state; it never owns global configuration. The library remembers the initialized
creator separately from the next negotiated method, so cleanup always returns to
the correct module, including partial initialization and method replacement.

The input context supplies role, versions, KEXINIT payloads, host-key operations,
`config`, `state`, host-key verification and `send`/`recv` callbacks. The handler
initially receives a zero result. It publishes unsigned big-endian K without SSH
length/sign encoding, the complete exchange hash matching `hash_name`, and one
real release callback covering both spans. On failure, it either releases partial
resources itself or publishes their lease. Return zero or a negative error;
positive status is invalid. The library validates output metadata, copies the
initial session identifier, derives directional keys and releases the result
before resetting or cleaning state. The producer clears its actual secret
capacity on release and may retain storage for the next exchange.

Init/cleanup must not enter session, lifecycle or configuration APIs. The common
result contract applies to release callbacks. No physical library mutex spans
these callbacks. Registrations, their names, configuration and callback code
remain borrowed for process lifetime. Set global configuration before the first
session with `dssh_kex_set_ctx()`; session state is separate. See
`deucessh-kex.h` and `deucessh-result.h` for the complete contracts.

## DH Group Exchange (Server)

DH-GEX works out of the box — the built-in default selects from
RFC 3526 groups 14–18 (2048–8192-bit) based on the client's requested
min/preferred/max range.
The built-in client requests 2048/4096/8192 bits. Fixed group14 uses 2048 bits
and group16 uses 4096 bits; DH strength depends on the modulus as well as the
hash. Registration order expresses application preference, rather than a
universal strength ranking.


To override with custom groups (e.g. from a moduli file), use the provider and
result types in the installed `deucessh-kex.h`. Configure a registered DH-GEX
algorithm before `dssh_session_init()`:

```c
#include "deucessh-algorithms.h"
#include "deucessh-kex.h"

/* Application implementation: select a group within min/preferred/max,
 * set out->p and out->g to unsigned big-endian spans, and set out->lease.
 * Return zero on success or a negative error. */
int my_group_selector(uint32_t min, uint32_t preferred, uint32_t max,
    struct dssh_dh_gex_group_result *out, void *cbdata);

int configure_my_dh_groups(void)
{
    static struct dssh_dh_gex_provider provider = {
        .select_group = my_group_selector,
        .cbdata = NULL,
    };
    return dssh_kex_set_ctx("diffie-hellman-group-exchange-sha256", &provider);
}
```

The callback receives the client's min/preferred/max bit sizes and a zeroed
`dssh_dh_gex_group_result`. Its nonempty `p` and `g` spans contain unsigned
big-endian integers, without SSH length prefixes or sign padding. One producer
lease covers both spans. The library validates/imports the group and releases
its lease before sending packets, on success and every failure. It never clears
or frees provider storage; invalid reported lengths do not determine producer
cleanup sizes. Publish partial resources through a lease on failure or release
them before returning. Positive status and missing release functions are invalid.

A static group needs an explicit no-op release. The built-in selector lends its
immutable RFC tables this way, with no per-selection allocations or copies.
Custom producers can retain their own capacities for reuse. Different sessions
can select concurrently, so each live mutable result needs independent storage.
Provider configuration and callback code have the borrowed process lifetime of
the registration. The common result contract governs release restrictions.

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

The key-file helpers own their file I/O. `pw_cb` is a
`dssh_pem_password_cb`: it returns an exact byte count in `[0, size]`, or a
negative value on failure. Empty passwords, embedded NUL bytes and a full
buffer are valid; no NUL terminator is required. A callback selects encrypted
private-key output using AES-256-CBC and supplies a password when encrypted
input needs one. NULL permits unencrypted input/output only, ignores cbdata
and never invokes a provider prompt. Failed imports do not publish a new key.

Private-key saves overwrite the selected file and retain its identity. The
opened target must be regular, singly linked and owned by the effective caller;
a final symbolic link or Windows reparse point is rejected before mutation.
The saved file uses POSIX mode 0600 or a protected caller-SID Windows ACL.
A save error may leave empty or partial output. The application coordinates
the destination directory/namespace and manages recovery and key retirement;
a failed save does not guarantee that the previous file was removed or changed.
Parent path resolution follows the native platform rules.

This example accepts a C-string password and rejects an oversized password:

```c
#include <limits.h>
#include <stdint.h>
#include <string.h>

int my_pw_cb(char *buf, int size, int rwflag, void *u)
{
	const char *pass = u;
	if (buf == NULL || pass == NULL || size < 1 || (rwflag != 0 && rwflag != 1))
		return -1;
#if INT_MAX > SIZE_MAX
	if (size > SIZE_MAX)
		return -1;
#endif
	size_t capacity = (size_t)size;
	size_t length = strlen(pass);
	if (length > capacity)
		return -1;
	memcpy(buf, pass, length);
	int result = (int)length;
	return result;
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
| `dssh_chan_params_set_accept_early_data(p, enable)` | Accept pre-setup data from broken servers (see below) |
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

Environment requests use `want_reply=false`. Channel opening does not wait for
environment acknowledgments or report whether the peer accepted a variable;
local allocation and send errors still fail the open.

`set_mode()` accumulates opcode/value pairs; replacing an existing opcode
reuses its storage. Opcode zero is TTY_OP_END, supplied by the encoder rather
than stored as a mode/value pair. Invalid mode metadata is rejected before
channel allocation or publication. Encoding emits opcodes 1–159 first, then
160–255, then TTY_OP_END (RFC 4254 s8). A receiver stops at END or a future
opcode (160–255); bytes after that stopping point are uninterpreted.

RFC 8160 assigns opcode 42 to IUTF8: value 0 disables it and value 1 enables
it. An application that knows its terminal's state should supply it before
opening a PTY. For example:

```c
int configure_terminal_utf8(struct dssh_chan_params *params, bool enabled)
{
    return dssh_chan_params_set_mode(params, 42, enabled ? 1 : 0);
}
```

Check that return value before opening the channel. The generic mode builder
preserves uint32 values; it does not enforce device-specific mode policy.
The library does not inspect or configure a local terminal, choose IUTF8 by
default, or decode channel bytes as text. A server may ignore unsupported modes;
its application interprets the accepted PTY parameters. The client example
leaves terminal modes unspecified because it does not query terminal state.

## Channel Lifecycle

### Open sequence

`dssh_chan_open()` performs the full setup sequence internally:

1. Allocate I/O buffers
2. Send CHANNEL_OPEN "session" with `initial_window=0`
3. Wait for CHANNEL_OPEN_CONFIRMATION
4. Send env requests (from params)
5. Send pty-req (if enabled in params)
6. Send terminal request (shell/exec/subsystem per type)
7. Send WINDOW_ADJUST to open the window
8. Mark setup complete and return live channel

### Inbound window enforcement

The library tracks the locally-advertised window on every channel and
**clips inbound CHANNEL_DATA / EXTENDED_DATA to the advertised window
before the data callback runs**.  A peer that ignores the window
cannot drive the library into unbounded buffering — bytes past the
window are silently dropped.  The data callback may therefore receive
`len == 0`, or a `len` smaller than the wire packet's payload; it
must handle that as a no-op.

`len == 0` is also legitimate when the peer sends an empty
CHANNEL_DATA message (RFC 4254 doesn't forbid it, and the library
itself sends one as part of the `DSSH_PARAM_ACCEPT_EARLY_DATA`
workaround).  In neither case does `len == 0` mean EOF: EOF is
delivered separately as a `DSSH_EVENT_EOF` event.

### `DSSH_PARAM_ACCEPT_EARLY_DATA` (broken-server workaround)

Cryptlib-based SSH servers that don't wait for
`CRYPT_SESSINFO_SSH_CHANNEL_TYPE` to be set before sending (such as
Mystic BBS) begin emitting CHANNEL_DATA (banners, greetings, etc.)
immediately after CHANNEL_OPEN_CONFIRMATION, before the client has
had a chance to send its terminal request and open the window with
WINDOW_ADJUST.  By default those bytes are dropped (the window is
still 0).

Setting `DSSH_PARAM_ACCEPT_EARLY_DATA` on the channel params asks the
library to deliver pre-setup data anyway, bypassing the rx-window
clip until local channel setup completes. The workaround is limited to
`DSSH_CHAN_SHELL` and `DSSH_CHAN_EXEC`, in either stream or zero-copy mode.
It is **rejected with `DSSH_ERROR_INVALID` (NULL channel) on
`DSSH_CHAN_SUBSYSTEM`**. Subsystem channels retain normal zero-window setup;
their receive storage follows the selected stream or zero-copy API.

```c
dssh_channel example_open_early_shell(dssh_session sess)
{
	struct dssh_chan_params cp;
	int ret = dssh_chan_params_init(&cp, DSSH_CHAN_SHELL);
	if (ret != 0)
		return NULL;
	ret = dssh_chan_params_set_accept_early_data(&cp, true);
	dssh_channel ch = NULL;
	if (ret == 0)
		ch = dssh_chan_open(sess, &cp);
	dssh_chan_params_free(&cp);
	return ch;
}
```

Bytes accepted via the bypass are NOT credited back to the peer (no
WINDOW_ADJUST is sent for them) — the over-budget bytes were free,
not advertised. Once local channel setup completes, normal
window enforcement resumes for the rest of the channel's life.

These same servers further gate *post-setup* output on receiving any
client CHANNEL_DATA (an empty banner-fragment first, then nothing
until the user types something).  When the flag is set, the library
also sends a 0-byte `SSH_MSG_CHANNEL_DATA` immediately after
`WINDOW_ADJUST` to satisfy that predicate without injecting a
spurious keystroke.  RFC 4254 doesn't forbid empty data messages
(well-behaved servers ignore them; Cryptlib treats them as the
"client has spoken" event it was waiting on).

Do not enable this flag globally.  It exists to allow the small set
of legacy servers that misbehave in a specific way to still be usable
with a stream-shaped channel; it is not a generic relaxation of the
SSH protocol.

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

Return values follow non-blocking POSIX semantics:
- `> 0`: bytes transferred
- `0`: read — EOF (peer sent `CHANNEL_EOF` / `CHANNEL_CLOSE`, or
  the session has terminated and the bytebuf is drained).
  Write — only when `bufsz == 0`.
- `DSSH_ERROR_NOMORE`: would block (analogous to POSIX
  `-1/EAGAIN`).  Read returns this when the bytebuf is empty but
  no EOF has arrived; write returns it when the remote send
  window is full. Readiness is a snapshot: another caller can consume data
  or send-window credit after poll returns, so the next operation must
  still handle `DSSH_ERROR_NOMORE`.
- `< 0` (other): `DSSH_ERROR_*`

The peek form (`buf == NULL, bufsz == 0`) always returns the
non-negative byte count; it never returns `NOMORE`.

Once a read delivers bytes, it returns their count. If returning receive credit
then fails, the channel retains the first error. The next non-NULL read with
nonzero capacity on either stream reports that error once, without copying or
consuming data. Peeks preserve it. A subsequent positive-capacity read retries
queued credit, even when its stream buffer is empty, so recovery needs no new
application data send. A same-thread zero-copy reservation for another session
must be finished or canceled before reading data; availability peeks remain
available. A reservation on the same session is preserved, with credit queued
until its owner finishes or cancels it.

### Poll events

| Flag | Meaning |
|------|---------|
| `DSSH_POLL_READ` | stdout data, EOF, or a retained read error available |
| `DSSH_POLL_READEXT` | stderr data, EOF, or a retained read error available |
| `DSSH_POLL_WRITE` | send window has space |
| `DSSH_POLL_EVENT` | event ready (signal, window-change, etc.) |

`dssh_chan_poll()` returns the bitwise-OR of fired flags; 0 means
"no events before timeout".  When the session is terminated
(`dssh_session_is_terminated()` is true), every requested data flag
is surfaced as ready — POSIX `POLLHUP` semantics — so the caller's
next `dssh_chan_read` returns 0 (EOF) once the bytebuf is drained
and any retained read error has been reported,
and `dssh_chan_write` returns a negative error.  A poll loop
therefore exits naturally through the subsequent read/write rather
than spinning on a 0 return.  `DSSH_POLL_EVENT` is unaffected by
termination; use the data flags or the terminate callback to
detect it.

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

/* Server close with exit status (exit-status + EOF + CLOSE) */
dssh_chan_close(ch, 0);

/* Close without exit status (negative or above UINT32_MAX omits it) */
dssh_chan_close(ch, -1);
```

An ordinary `dssh_chan_close()` consumes the channel handle on every return;
do not retry or reuse it after an error. Packet sends are best effort: zero
reports local disposal, including when sending fails or the session was already
terminated. A failed detach returns `DSSH_ERROR_TERMINATED`; the terminated
session owns any deferred storage until cleanup.

On a server, `exit_code` in `[0, UINT32_MAX]` selects the status packet. All other
`int64_t` values omit status and still close the channel. A client does not send
exit-status. No status argument produces an invalid-argument error.

Calls forbidden by callback or TX-reservation context return
`DSSH_ERROR_INVALID` without consuming the handle. Finish or cancel the caller's
same-session zero-copy reservation before closing a channel. The existing
server setup callback exception records cancellation and defers retirement
until that callback returns. Stop other application use before ordinary close.

### Control messages

```c
dssh_chan_send_signal(ch, "INT");
dssh_chan_send_window_change(ch, cols, rows, wpx, hpx);
dssh_chan_send_break(ch, 0);  /* RFC 4335 */
```

### Receiving BREAK requests

BREAK is an action request, separate from the event queue. Set a session default
before `dssh_session_start()`, or replace a channel's handler using
`dssh_chan_set_break_cb()`. Check every setter result. The session default is
inherited by new stream, zero-copy and accepted channels.

```c
/* Application function: return true only if a BREAK of some kind occurred. */
bool perform_break_on_device(uint32_t requested_ms, void *device);

static bool handle_break(dssh_channel ch,
    const struct dssh_chan_break_request *request, void *device)
{
    (void)ch;
    return perform_break_on_device(request->length, device);
}

int configure_break(dssh_session sess, void *device)
{
    return dssh_session_set_break_cb(sess, handle_break, device);
}
```

After channel setup, the handler runs on the demux thread without library
mutexes held. The channel and request are borrowed for the invocation. It gets
the exact uint32 duration, cumulative prior stdout/stderr positions (saturating
at `SIZE_MAX`) and normalized `want_reply`. Return true if any BREAK was
performed, even if a later part of the device operation failed; return false
only if none was performed. Queueing work alone does not establish execution.
A requested SUCCESS/FAILURE follows that return. With no handler, no action or
event occurs, and a requested reply is FAILURE. Requests during setup retain
the unperformed/rejection path. The sender uses `want_reply=false`; its success
reports local transmission, not remote device execution.

The library does no terminal/device I/O or duration clamping. The application
owns authorization, serial/nonserial attention behavior and cascaded forwarding.
RFC 4335 recommends the device default for zero, 500 ms if no default exists,
and ordinarily 500–3000 ms for controllable durations. The application chooses
its device policy.

Follow event callback restrictions: no channel I/O, poll, TX, close, open or
accept from this handler. Getters and callback/limit setters remain available.
Session lifecycle requests retain deferred cleanup. Waiting for a worker/device
here pauses receive progress for every channel in this session. Callback/data
replacement affects future snapshots; keep an already captured pair alive until
its invocation finishes. The examples have no device BREAK action and leave
the handler unset.

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
- CHANNEL_REQUEST "signal", "exit-status", "exit-signal", "window-change" — delivered as events
- CHANNEL_REQUEST "break" — invokes the synchronous action handler; requested replies reflect its result
- Forbidden channel types (x11, forwarded-tcpip, direct-tcpip, session-from-server) — auto-rejected
- Data after peer EOF/CLOSE — discarded
- Rekeying, global requests, IGNORE/DEBUG/UNIMPLEMENTED — transparent

`dssh_session_stop()` requests termination, joins the demux thread and
frees channel resources. `dssh_session_cleanup()` also stops the session
before freeing its transport state. First stop and join application threads
using the session or its channels, and serialize start, stop and cleanup calls.
The application's I/O callbacks must return promptly after termination.

Failure of a required internal termination wake calls `abort()` because the
library cannot safely return while hidden waiters may remain blocked. Ordinary
notification errors terminate the session through this required wake path.

A failed join of the valid joinable demux is an unrecoverable internal ownership
failure and calls `abort()`, without invoking diagnostic callbacks or attempting
uncertain cleanup. Ordinary initialization, peer and I/O errors keep their
defined error handling.
Lifecycle calls made during a callback on the session or while the caller
owns its zero-copy TX reservation defer destruction; the owner must call
stop or cleanup again after that context ends. The restrictions on I/O,
logging and termination callbacks still apply.

## Event Model

RFC 4254 notifications (`signal`, `window-change`, `exit-status`, `exit-signal`)
use `want_reply=false`. A reply-requested version is declined with CHANNEL_FAILURE
before event delivery. BREAK uses its dedicated synchronous action/reply contract.

Events (signal, window-change, EOF, close, exit-status,
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
        printf("Signal bytes:");
        for (size_t i = 0; i < event.signal.name.len; i++) {
            unsigned int byte = event.signal.name.data[i];
            printf(" %02x", byte);
        }
        printf("\n");
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

Borrowed `dssh_bytes` views (`signal.name`, `exit_signal.signal_name`,
`exit_signal.error_message`, `exit_signal.language`) preserve exact accepted
bytes and lengths, including embedded zero bytes, without a terminator promise.
Empty views have NULL data and zero length. Never free or release these views;
copy bytes you retain beyond their lifetime. They are valid until the next
`dssh_chan_read_event()` or `dssh_chan_poll()` on the same channel.

### Event callback (optional)

As an alternative to poll + read_event, set a callback that fires
from the demux thread when an event is queued:

```c
dssh_chan_set_event_cb(ch, my_event_cb, my_context);

/* Or set a default for all channels on the session: */
dssh_session_set_event_cb(sess, my_event_cb, my_context);
```

The callback consumes each newly published event, in wire order, without also
adding it to the poll queue. Its positions are cumulative bytes delivered on
each stream before the event, saturating at `SIZE_MAX`. The event and its strings
are valid only during that invocation. Callbacks run without library mutexes
and must be fast; queue work for another thread instead of calling channel I/O,
poll/read_event, open/accept, or TX functions.

The setter replaces the callback and context as a pair. An invocation that
already captured the old pair may finish after the setter returns; keep that
code and context alive until it finishes. Installing a callback leaves existing
queued or frozen poll events available. Removing it with a NULL callback sends
future events to the poll queue. A poll event's positions retain their separate
meaning: unread bytes when that event is frozen by poll.

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

/* Reserve the packet engine and get its data pointer (no mutex retained) */
dssh_chan_zc_getbuf(ch, 0, &buf, &max_len);

/* Build data directly in the buffer */
memcpy(buf, my_data, my_len);

/* Send — fills headers, MAC, encrypt, releases TX ownership */
dssh_chan_zc_send(ch, my_len);

/* Or cancel without sending */
dssh_chan_zc_cancel(ch);
```

**Contract:** `zc_getbuf` reserves exclusive packet-engine ownership without
holding a mutex across the application fill interval.  Call `zc_send` or
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

Setup callbacks run on the accept caller thread with no library mutex held.
The channel and accumulated parameters are borrowed for that invocation;
getters on the callback handle show those parameters. Event/BREAK callback and limit
setters are available. Same-session channel I/O, polling, TX reservations and
nested open/accept are rejected. Independent sessions remain available.
Do not hand the handle to another thread before accept returns successfully.

Calling `dssh_chan_close(ch, exit_code)` in its setup callback records whole-channel
cancellation and returns 0; the exit code is unused before startup. Do not use the
handle again. After the callback returns, the acceptor sends the required
rejection/CLOSE, retires the channel and continues accepting. Cancellation takes
precedence over a nonnegative callback return. A negative PTY/env return without
cancellation rejects only that request; a negative terminal return rejects the
channel. Session termination, stop and cleanup retain their deferred lifecycle
behavior. Successful setup publishes owned parameters before normal callbacks
can run, including callbacks that run before accept returns.

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

**Global setup:** serialize registration, global setters and builtin key
replacement in the application, and finish before the first session-init attempt.
Before freeze, serialize key utilities with replacement; same-thread password
callback replacement remains allowed. First-session initialization must not be
nested inside mutable setup. A permitted session-init attempt permanently freezes
setup before allocation, even if it returns NULL. After setup, independent
sessions may be constructed concurrently. After freeze, builtin key reads/exports
may run concurrently using immutable keys; coordinate destination files.

**Each session before `dssh_session_start()`**: serialize its handshake and auth
calls. Different sessions are independent.

**After `dssh_session_start()`**: the demux thread handles all receiving.
The application calls `poll()` / `read()` / `write()` from any
thread.  Per-channel `buf_mtx` protects buffer access; `poll_cnd`
wakes waiters.  Transport sends use a short state mutex to hand off exclusive
packet-engine ownership; packet construction, cryptographic callbacks, and
application I/O callbacks run after that mutex is released.

During rekeying, application-layer sends (message type >= 50) are
blocked until NEWKEYS completes.

Failure to release a valid owned internal mutex, or to join the valid joinable
owned demux under the lifecycle preconditions, calls `abort()`. These internal
ownership failures have no retry or partial-cleanup contract. Ordinary resource,
I/O, acquisition and timeout errors retain their documented error handling.

**ZC callback restrictions:** The data callback and event callback
run on the demux thread.  They MUST NOT call any TX function
(`dssh_chan_write`, `dssh_chan_zc_getbuf`, `dssh_chan_close`, etc.)
— enforced at runtime via a thread-local guard.

## Rekeying

The library automatically performs key re-exchange (RFC 4253 s9) when
any of these thresholds is exceeded:

| Threshold | Default | Constant / source |
|-----------|---------|-------------------|
| Packets per key | 2^28 (~268M) | `DSSH_REKEY_SOFT_LIMIT` (RFC 4344 s3.1) |
| Bytes per key (per direction) | per-cipher | `dssh_enc_s::bytes_per_key` (RFC 4344 s3.2) |
| Time per key | disabled (opt-in) | `DSSH_REKEY_SECONDS` (3600 s suggestion) |

Byte limits come from each `enc` module: 64 GiB (2^36) for 128-bit
block ciphers (`aes128-cbc` and `aes256-ctr`, using RFC 4344
s3.2's 2^(L/4) blocks for L=128),
and `UINT64_MAX` for the `none` cipher (no key wear).  Each
direction's byte counter is compared independently against the
selected cipher's limit, so c2s and s2c use their own keys' budgets.

The demux handles incoming `SSH_MSG_KEXINIT` and checks rekey
thresholds as it receives traffic. If a send reaches the outbound
packet or byte threshold while the demux is waiting for input, the
packet-engine owner sends KEXINIT and the demux finishes the
exchange. The optional time threshold is checked as the demux
processes traffic.

For send-only sessions without a receive thread, a hard packet limit
(2^31) returns `DSSH_ERROR_REKEY_NEEDED`.

### Time-based rekey (off by default)

The byte and packet thresholds have cryptographic justification
(key-material wear); the time-based threshold does not.  RFC 4253
s9 calls time rekey RECOMMENDED, not required, and some peers
(notably Cryptlib-based servers like Mystic BBS) refuse mid-stream
`SSH_MSG_KEXINIT` outright with `CRYPT_ERROR_BADDATA`, killing the
session at the 1-hour mark.  DeuceSSH therefore leaves time rekey
off by default.

```c
dssh_session_set_rekey_seconds(sess, DSSH_REKEY_SECONDS);  /* 3600 s */
dssh_session_set_rekey_seconds(sess, 28800);               /* 8 h */
dssh_session_set_rekey_seconds(sess, 0);                   /* off (default) */
```

Byte and packet thresholds remain in effect regardless.  May be
called at any point in the session.

## Optional Callbacks

Set these on the session after init, before start.  All return 0 on
success or `DSSH_ERROR_TOOLATE` if `dssh_session_start()` has already
been called:

```c
dssh_session_set_debug_cb(sess, my_debug_handler, my_context);
dssh_session_set_unimplemented_cb(sess, my_unimpl_handler, my_context);
dssh_session_set_banner_cb(sess, my_banner_handler, my_context);
dssh_session_set_global_request_cb(sess, my_global_req_handler, my_context);
dssh_session_set_terminate_cb(sess, my_terminate_handler, my_context);
```

### Structured diagnostics

An optional per-session diagnostic callback reports contextual errors,
warnings, and debugging information from session creation through
termination.  The default level is `DSSH_LOG_ERROR`; enable additional
records with `dssh_session_set_log_level()`:

```c
static dssh_log_action
my_log(const struct dssh_log_record *record, void *cbdata)
{
    /* message and language are length-delimited, callback-lifetime data. */
    save_record(cbdata, record);
    return DSSH_LOG_LOCAL_ONLY;
}

dssh_session_set_log_cb(sess, my_log, my_log_state);
dssh_session_set_log_level(sess, DSSH_LOG_WARNING);
```

The callback runs synchronously on the API caller or demux thread that
owns the event, with no library mutex held.  Calls for one session may
overlap, so applications retaining a shared list must synchronize their
own storage.  The callback must be quick, must copy retained strings, and
must not call a `dssh_*` function on the same session.

Returning `DSSH_LOG_SEND_DEBUG` or
`DSSH_LOG_SEND_DEBUG_ALWAYS_DISPLAY` requests best-effort forwarding of
a local record as `SSH_MSG_DEBUG`.  Forwarding is bounded and may be
silently skipped; peer-originated records are never echoed.  An actual
wire-send failure remains fatal to the transport.  Peer text is untrusted
and applications must filter control characters before displaying it.

When no callback is installed, diagnostic formatting, message copying,
queue allocation, and queue locking are skipped.  Suppressed warning and
debug records are gated before their formatting arguments are evaluated.

## Inactivity Timeout

Internal waits for peer responses (channel open confirmation, channel
request replies, setup messages, rekey completion) use a configurable
timeout.  Set it after init, before start (returns `DSSH_ERROR_TOOLATE`
after start):

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
`dssh_strerror()` maps these codes to generic static strings; structured
diagnostics provide operation-specific context when enabled.

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
| `DSSH_ERROR_TOOLATE` | -6 | Operation no longer allowed in this state |
| `DSSH_ERROR_TOOMANY` | -7 | Configured capacity or registry limit exceeded |
| `DSSH_ERROR_TOOLONG` | -8 | Data exceeds buffer or protocol limit |
| `DSSH_ERROR_MUST_BE_NULL` | -9 | Registration link was not NULL |
| `DSSH_ERROR_NOMORE` | -10 | No queued item is available |
| `DSSH_ERROR_REKEY_NEEDED` | -11 | Must rekey before sending more packets |
| `DSSH_ERROR_REJECTED` | -13 | Channel open or request rejected |
| `DSSH_ERROR_TIMEOUT` | -14 | Operation timed out |

## Packet Buffer Size

`dssh_session_init()` takes a `max_packet_size` parameter that
controls the per-session packet buffer allocation.  Pass 0 for the
RFC minimum (33280 bytes), which accommodates the RFC-required
32768-byte uncompressed payload with room for headers and padding.
Pass a larger value to support bigger payloads.

## Limits

| Limit | Value | Source | Description |
|-------|-------|--------|-------------|
| Identification line | 255 bytes | RFC 4253 s4.2 | Includes prefix and CRLF. |
| Algorithm name | 64 bytes | RFC 4251 s6 | Each individual name. |
| Complete negotiation lists | Session packet bound | Transport framing | No separate fixed 1024-byte list limit. |
| Locally sent disconnect description | 230 bytes | Implementation bound | Clamped to fit its control slot. |

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

CTest prepares the shared Ed25519/RSA key cache before parallel consumers
start, including filtered runs.

Optional independent OpenSSH tests require POSIX process/I/O support, Python
3.9+, `ssh`, `sshd`, and `ssh-keygen`. Enable them with
`-DDEUCESSH_TEST_OPENSSH=ON` together with `-DDEUCESSH_BUILD_TESTS=ON`, then build
and run `ctest --test-dir build -L openssh -j2 --output-on-failure`. They exercise
the production static/shared libraries in both roles over stdio, with temporary
fixture keys, pinned host keys, signed authentication, exact stdout/stderr/exit
status, forced plain algorithm names and peer-initiated rekey. No listening
socket, account change or global SSH configuration change is made. The daemon role requires that
the current local account is permitted to authenticate using its isolated test
configuration. Complete diagnostics and actual process exits remain in the
build tree's `test/openssh-evidence/` directory; private keys are removed.

Optional RFC 8308 delayed compression, flow-control and elevation extensions
are not advertised. Current release requirements, validation scopes and residual
limitations are in the [release checklist](docs/release-1.0-checklist.md).

## Debugging

Build with `-DDSSH_TRACE_WIRE` (e.g.
`-DCMAKE_C_FLAGS=-DDSSH_TRACE_WIRE`) to deliver packet metadata through the
session's diagnostic callback at `DSSH_LOG_DEBUG`. Configure that callback and
debug level before traffic. Records are local, prefixed `[dssh-wire]`, and report `tx msg_type=N len=M` or `rx msg_type=N len=M`, where
N is the SSH message type number (RFC 4253 / 4254, e.g. 20
KEXINIT, 21 NEWKEYS, 91 CHANNEL_OPEN_CONFIRMATION, 93
CHANNEL_WINDOW_ADJUST, 94 CHANNEL_DATA, 95 CHANNEL_EXTENDED_DATA,
99 CHANNEL_SUCCESS) and M is the SSH-payload length.  Default
builds compile the macro to a no-op with no overhead.

## Files

Public headers (consumers include these):
```
deucessh.h               Core: errors, session lifecycle, transport queries
deucessh-auth.h          Authentication (client + server)
deucessh-conn.h          Connection: channels, params, poll/read/write, accept
deucessh-algorithms.h    Algorithm registration + key management
deucessh-crypto.h        Backend-neutral crypto utilities
deucessh-lang.h          Directional language preferences and peer lists
deucessh-result.h        Byte spans and producer-owned release leases
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

crypto/             Backend-neutral primitive implementations
kex/                Key exchange modules
key_algo/           Host key algorithm modules
enc/                Encryption modules
mac/                MAC modules
comp/               Compression modules

examples/client.c          POSIX client
examples/server.c          POSIX server
examples/kbi-game-server.c POSIX keyboard-interactive server
posix/, win32/             Private C11 thread compatibility shims

test/               Test suite (built with -DDEUCESSH_BUILD_TESTS=ON)
  dssh_test.h       Test framework (assert macros, runner)
  mock_io.h/.c      Bidirectional mock I/O layer
  test_*.c          Test source files
```


Optional independent BREAK interoperability checks use a separately built SSH
client; it does not compile or link the C API. The audit helper
`test/build_break_ssh_peer.py` builds `test/break_ssh_peer.go` from existing
offline `golang.org/x/crypto` v0.58.0 and `golang.org/x/sys` v0.49.0 module
sources with Go 1.26. Pass the resulting executable as
`-DDEUCESSH_BREAK_SSH_PEER=/absolute/path/to/break-ssh-peer` to enable the two
`dssh_break_independent_*` CTests. Library builds and the C test suites require
no Go installation. These checks verify handler-derived acknowledgements,
pinned host authentication, transfer, exit status and rekey against the foreign
implementation; they do not operate a physical serial device. The optional
`dssh_terminal_mode_independent_*` checks additionally verify absent, disabled
and enabled IUTF8 through PTY callbacks and retained getters.

The optional `dssh_notification_flags_independent_*` checks verify notification
reply flags, callback fields and delivery against the same independent peer,
while retaining its transfer/status/rekey and BREAK checks.
