# Cryptlib removal from sbbs3 — design

Date: 2026-08-03
Last updated: 2026-08-06
Status: draft; implementation prerequisite: Botan 3.13.0 release

## Problem

`sbbs3` does not use Cryptlib for one isolated feature.  It uses it as four
different libraries at once:

| Role | Current consumers |
|---|---|
| TLS client and server transport | FTP, POP3/POP3S/STLS, SMTP/SMTPS/STARTTLS, outbound SMTP STARTTLS, HTTPS, JavaScript services, JavaScript `Socket`, and the MQTT client and broker |
| Shared TLS identity | `ssl.c`, `ssl.cert`, self-signed certificate creation, certificate reload, and system-password re-encryption |
| SSH transport and authentication | the terminal server's SSH listener, shell channels, public-key/password authentication, and channel multiplexing |
| General JavaScript crypto and persistence | `CryptContext`, `CryptCert`, `CryptKeyset`, `certtool.js`, `letsyncrypt.js`, and `acmev2.js` |

The dependency consequently reaches nearly every server library.  `ssl.h`
publishes Cryptlib handle types, `sbbs.h` stores a `CRYPT_SESSION`, and
`js_socket.h` embeds another one.  Build dependency lists then mark many
otherwise-unrelated translation units as Cryptlib consumers merely because
they include those headers.

The replacement architecture uses two typed libraries:

- `src/xptls` supplies provider-neutral TLS client/server sessions, shared server
  credentials, encrypted PEM key handling, X.509/CSR/CRL primitives, digests,
  signatures, KDFs, streaming symmetric encryption, and opaque storage-backed
  asymmetric keys.
- `src/ssh` (DeuceSSH) supplies server transport, password/public-key
  authentication callbacks, shell/subsystem channels, stream I/O, terminal
  metadata, and window-change events.

The sbbs3 integration owns TLS identity management, endpoint policy, terminal
server ownership, authentication, channel and SFTP lifecycles, JavaScript
compatibility objects, and migration from Cryptlib's proprietary keysets.

Simply changing function names would preserve Cryptlib's attribute-driven
architecture and its ambiguous ownership rules.  Simply deleting the three
JavaScript classes would make the tree compile while breaking certificate
renewal and documented public APIs.  This design replaces both the dependency
and those architectural assumptions.

## Goals

- No `sbbs3` executable or shared library includes `cryptlib.h`, links
  Cryptlib, calls a `crypt*` function, or requires a Cryptlib runtime file.
- xptls supplies all TLS and general cryptographic mechanism through either
  Botan 3 or OpenSSL 3, selected once for the whole build.
- DeuceSSH supplies SSH transport, authentication, shell channels, and SFTP
  subsystem channels.
- Existing TLS listeners, explicit TLS upgrades, outbound SMTP policy, SSH
  authentication, interactive shells, SFTP, and certificate hot reload retain
  their externally-visible behavior unless this document calls out a change.
- The documented `Socket` TLS properties and the in-tree behavior of
  `CryptContext`, `CryptCert`, and `CryptKeyset` remain available.  Provider
  differences fail explicitly; they must not silently select a different
  algorithm or validation policy.
- Private keys are opaque storage-backed handles.  New installations prefer
  a TPM, PKCS#11 token, or supported platform hardware keystore; encrypted
  PKCS#8 files remain a configurable and fully-supported backend.
- Key-storage selection is explicit and observable.  Loss of a bound hardware
  store fails closed and never causes an unnoticed fallback to a new file key
  or a changed public identity.
- An operator can move through a transition release without an unexpected TLS
  or SSH host-key change.
- The port is divided into independently testable commits.  Cryptlib is
  removed only after no live consumer remains.

## Non-goals

- Binary compatibility with Cryptlib handles, error numbers, keyset bytes, or
  the Cryptlib C API.
- A generic Cryptlib emulation layer.  New native code uses typed xptls and
  DeuceSSH APIs, never locally-defined `CRYPT_*` aliases.
- Adding SSH client support to `sbbs3`.  DeuceSSH's client support remains a
  SyncTERM concern.
- Adding SSH forwarding, arbitrary `exec` channels, an SCP server, or more
  than one shell and one SFTP channel per terminal-server connection.
- Changing BBS account policy.  Password comparison calls the common
  account-authentication helper, including its hashed-password policy; this
  work does not create a second verifier.
- Requiring TLS authentication for opportunistic outbound SMTP.  That is a
  separate mail-policy change.
- Preserving TLS 1.0 or 1.1 as a default.  The JavaScript compatibility
  property continues to express them, but provider or system policy may reject
  them and every built-in server remains TLS-1.2-or-newer.
- Deleting Cryptlib from `3rdp` while another repository target still uses it.
  The `sbbs3` dependency is removed here; repository-wide deletion happens
  only after a repository-wide reference check.
- Requiring hardware that the selected provider, operating system, or host
  does not expose.  File-backed storage remains supported for hosts without
  usable hardware and where local policy deliberately selects it.

## Architectural decisions

### One crypto provider per build

Integrated builds select one `CRYPTO_BACKEND` for xptls and DeuceSSH:

1. explicitly configured Botan or OpenSSL;
2. system Botan 3.13.0 or newer;
3. system OpenSSL 3.0 or newer; or
4. vendored Botan where the existing build permits it.

The shared CMake configuration pins `XP_CRYPTO_BACKEND` and
`DEUCESSH_CRYPTO_BACKEND` to that selection.  GNU make pins
`XP_CRYPTO_BACKEND` and `DEUCESSH_BACKEND` in the same way.  Component-specific
selectors exist only for standalone library builds; an integrated build
rejects conflicts.  Loading both providers into one server gains no useful
fallback: provider objects cannot cross either API, behavior becomes
build-order dependent, and the security surface doubles.

`WITHOUT_CRYPTO` leaves the xptls disabled stubs available so non-TLS code can
compile.  `WITHOUT_DEUCESSH` independently removes SSH.  DeuceSSH still needs
a real crypto backend when SSH is enabled, even if xptls is stubbed.  A server
configured for a disabled feature logs that fact and does not open the
corresponding listener; a nominal TLS port must never fall back to plaintext.

### Botan version boundary

The release of Botan 3.13.0 is a prerequisite for implementing this design.
The shared build logic requires Botan 3.13.0 or newer, and the provider uses
the 3.13 interfaces directly.  An unreleased master snapshot does not satisfy
this prerequisite and is not an ordinary build dependency.

On 2026-08-05, Botan master revision
`a8f6c6a59e850016f77dd2a1d3d10991d2610eba`, identifying itself as unreleased
3.13.0, was built on FreeBSD and tested with xptls.  Focused probes verified
all of the capabilities currently hidden behind compatibility code:

- `BER_Decoder::Limits::DER()` rejects non-canonical BER input;
- the typed `AlternativeName` IPv4 and IPv6 accessors preserve the raw
  address bytes;
- the native `CRL_Issuing_Distribution_Point` encoder and decoder round trip;
  and
- a complete, same-issuer CRL without `issuingDistributionPoint` matches a
  certificate containing `CRLDistributionPoints`, as required by RFC 5280
  section 6.3.3 and tracked by
  [Botan issue 5784](https://github.com/randombit/botan/issues/5784).

A temporary xptls provider converted to the 3.13 interfaces built successfully
and passed all four xptls test programs.  Adopting 3.13 is nevertheless a
source migration, not only a version-check change:

- replace the raw subjectAltName IP decoder with `ipv4_addresses()` and
  `ipv6_addresses()`;
- replace the local issuing-distribution-point encoder and raw decoder with
  `CRL_Issuing_Distribution_Point` and `DistributionPointName`;
- restore strict DER limits when decoding externally supplied PKCS#7 bundles;
- use `dns_names()`, `email_addresses()`, and `uri_names()` and their typed
  values instead of retaining references to the deprecated string accessors;
- implement `Certificate_Extension::is_appropriate_context()` for the custom
  requested-certificate extension used to carry arbitrary CSR extensions;
  and
- move to the non-deprecated distribution-point, subject-key-ID, and CRL-number
  accessors.

The first three changes remove xptls workarounds.  The remaining changes adapt
to Botan 3.13's public API.  `AlternativeName` and `Certificate_Extension` were
already declared public before their incompatible 3.13 changes, so this is
also a source and potential ABI boundary for external subclasses and callers.
All xptls and downstream Botan consumer objects must be rebuilt together; a
build must not mix objects compiled against 3.12 headers with a 3.13 shared
library.

Implement these provider changes together with the 3.13.0 minimum and retain
the focused interoperability cases as permanent tests.  A supported platform
without a suitable system package must use the vendored 3.13.0 provider,
select another supported provider, or explicitly disable the dependent
feature.  Do not carry a parallel pre-3.13 implementation.

### No application provider objects

OpenSSL `SSL_CTX`, `SSL`, `EVP_PKEY`, and `X509` pointers and Botan key,
certificate, credential, and channel objects remain private to their
libraries.  `sbbs3` sees opaque xptls and DeuceSSH handles only.  This applies
to the JavaScript bindings too: their private data stores xptls handles rather
than provider handles.

### Asymmetric keys are opaque storage-backed handles

`xp_key_t` is the provider-neutral asymmetric-key handle shared by `xp_key`,
`xp_sign`, `xp_ca`, and TLS credentials.  A handle may contain a public or
private Ed25519, RSA, or ECDSA key in memory, in an encrypted file, or in a
supported hardware store.  Callers retain and release the handle, query its
algorithm, storage kind, private/exportable status, export its public SPKI and
SHA-256 fingerprint, and request typed signing operations.  Provider objects
never cross the interface.  Private export of a non-exportable key returns
`XP_CRYPTO_ERR_NOT_EXPORTABLE`; no caller treats that result as permission to
generate a replacement.

The storage contract is:

```c
typedef struct xp_key *xp_key_t;

enum xp_key_storage_policy {
    XP_KEY_STORAGE_AUTO,
    XP_KEY_STORAGE_HARDWARE,
    XP_KEY_STORAGE_FILE,
    XP_KEY_STORAGE_NAMED,
};

struct xp_key_store_config {
    enum xp_key_storage_policy policy;
    const char *store;       /* file, pkcs11, tpm2, or platform */
    const char *store_uri;   /* non-secret store locator */
    const char *file_path;   /* file destination or auto fallback */
    xp_crypto_secret_callback_t authorize;
    void *authorize_context;
};

int xp_key_store_query(
    const struct xp_key_store_config *store,
    const struct xp_key_spec *spec,
    struct xp_key_store_capabilities *capabilities);
int xp_key_generate_stored(
    xp_key_t *out, const struct xp_key_store_config *store,
    const struct xp_key_spec *spec);
int xp_key_import_stored(
    xp_key_t *out, const struct xp_key_store_config *store, xp_key_t source);
int xp_key_reference(xp_key_t key, void *out, size_t *len);
int xp_key_open_stored(
    xp_key_t *out, const struct xp_key_store_config *store,
    const void *reference, size_t reference_len);
int xp_key_destroy_stored(
    const struct xp_key_store_config *store,
    const void *reference, size_t reference_len,
    const void *expected_fingerprint, size_t fingerprint_len);
```

The counted, versioned reference identifies the selected store and object but
contains no authorization secret.  Import, generation, open, signing, export,
and destruction capabilities are queried independently.  Destruction requires
both the exact reference and expected SHA-256 public-key fingerprint.

The file store supports transactional generation, import, open, export, and
fingerprint-guarded destruction using encrypted private-key PEM.  Both crypto
providers support PKCS#11 generation, open, signing, and destruction for RSA
and ECDSA when their optional PKCS#11 dependencies are available; unsupported
import or algorithm combinations are reported through the capability mask.
TPM 2.0 and platform-store names are reserved by the common contract and must
report unavailable until their adapters implement the requested operation.

The storage API has four operator-facing policies:

| Policy | Behavior |
|---|---|
| `auto` | For a newly-created key, use the configured supported hardware store when available, then use the configured encrypted file destination. |
| `hardware` | Select the configured TPM, PKCS#11, or platform store and fail if it cannot be used.  There is no file fallback. |
| `file` | Generate or load encrypted PKCS#8 using the Synchronet system password. |
| named store | Require `tpm2`, `pkcs11`, or a supported platform store explicitly and fail if unavailable. |

`auto` is the installation default.  A configured PKCS#11 module/token takes
precedence over opportunistic local-device discovery; otherwise the platform
preference is documented and stable.  A Raspberry Pi or other host with no
usable hardware therefore provisions file keys without needing a special
build.  An operator may also select `file` as local policy even when hardware
is present.

SCFG exposes a global storage policy plus per-purpose overrides for TLS,
SSH, ACME account keys, and script-created keysets.  Hardware configuration
includes the store kind, PKCS#11 module/token or platform locator, and a
protected authorization source; it does not include an inline PIN.  SCFG's
effective-configuration display reports the selected backend and whether each
required algorithm can be generated, imported, opened, and used for signing.
Changing policy affects creation and explicit migration only.  It does not
silently move or regenerate existing keys.

The selected store and its non-secret locator are recorded for each logical
key in `ctrl/keyrefs.ini`, together with algorithm and public fingerprint.
That binding, not the current result of hardware discovery, is authoritative
after creation.  A missing token, changed TPM state, PIN failure, or provider
failure makes the bound key unavailable and disables the dependent listener;
it never invokes `auto` again.  Hardware authorization comes from the existing
protected-secret callback or the platform facility and is never written into
the reference file or a log.

File-backed handles contain encrypted PKCS#8 and preserve the atomic-write,
permission, and system-password rules in this document.  Hardware-backed
asymmetric keys are generated non-exportable when the store supports it.
TLS PSKs and other symmetric values may be stored as hardware-sealed blobs
where supported, but a TLS backend that requires raw PSK bytes may unseal them
for the handshake; it must lock/cleanse the temporary buffer.  The spec does
not falsely describe such PSKs as non-exportable hardware operations.

Both provider implementations expose the same capability query and storage
errors.  OpenSSL or Botan may use different native mechanisms internally,
but the application sees only an `xp_key_t` and a stable key reference.
Hardware integration is compiled and tested only where its SDK or provider
module is supported; lack of it is a reported capability, not a reason to
bypass an explicitly selected hardware policy.

### The socket owner remains the application

TLS and SSH sessions refer to a pre-connected `SOCKET`; they do not own it.
Every close path follows this order:

1. tell the protocol session to terminate;
2. `shutdown()` the socket when a blocked protocol I/O callback must wake;
3. join/clean up the protocol session; and
4. let the existing server owner close the socket exactly once.

This rule removes the current mix of `cryptDestroySession()`,
`destroy_session()`, session-owner clearing, and direct socket close calls.

## Cryptlib-shape audit

This is a semantic replacement, not a mechanical translation of Cryptlib
calls.  The implementation must identify behavior Synchronet needs and delete
the API scaffolding Cryptlib forced around it.  In particular, none of the
following is a compatibility requirement:

| Current artifact | Why it exists | Replacement disposition |
|---|---|---|
| `do_cryptInit()`, `is_crypt_initialized()`, `cryptEnd()`, slow-poll seeding, and runtime version/patch-hash checks | Cryptlib is a process-global runtime with a separately-staged binary | Delete from sbbs3.  Provider initialization and version validation are library/build concerns; feature availability is queried through xptls or DeuceSSH. |
| Integer `CRYPT_HANDLE`/`CRYPT_SESSION`/`CRYPT_CONTEXT` values, `CRYPT_UNUSED`, and `-1` sentinels in public structs | Cryptlib uses one untyped handle namespace | Replace with typed opaque pointers and `NULL`.  Do not create integer compatibility typedefs. |
| `cryptSetAttribute*()` / `cryptGetAttribute*()` setup and the two-call allocate/read helpers | Cryptlib exposes unrelated operations through an attribute bag | Use typed constructor config, getters, auth callbacks, and channel handles.  Delete `get_crypt_attribute()`, `get_binary_crypt_attribute()`, and `free_crypt_attrstr()`. |
| `cert_list`, `sess_list`, certificate epochs, four associated locks, `add_private_key()`, and the special `destroy_session()` | A Cryptlib private-key context is checked out, attached to one session, then recovered or destroyed with that session | Delete the entire pool.  An immutable reference-counted xptls credential can be retained by every session; reload swaps one manager reference. |
| `CRYPT_ENVELOPE_RESOURCE`, reading username/password/public-key attributes, setting `CRYPT_SESSINFO_AUTHRESPONSE`, and setting `ACTIVE` again | Cryptlib turns server authentication into a paused activation state machine | Delete.  `dssh_auth_server()` calls Synchronet verification callbacks directly and returns once authentication succeeds or terminates. |
| `CRYPT_SESSINFO_SSH_CHANNEL` as mutable current-session state, integer channel IDs, channel-type/argument attributes, and rescanning after `CRYPT_ERROR_NOTFOUND` | Cryptlib multiplexes all channels through one session I/O API and reports channel lifecycle indirectly | Delete.  Keep distinct `dssh_channel` handles; accept callbacks provide type and subsystem, while EOF/CLOSE are explicit events. |
| `ssh_mutex` around every SSH read, write, attribute query, channel switch, and SFTP send | Changing Cryptlib's session-wide current channel races with every other operation | Delete this serialization.  The adapter locks only its own handle publication/lifetime state; DeuceSSH owns transport/channel synchronization. |
| The `main.cpp` and `sftp.cpp` write-timeout sequence `5` → push/flush → `0` | Cryptlib's RX and TX timeout attributes interfere, so bounding a flush can alter subsequent receive behavior | Delete the entire dance.  `dssh_chan_poll()` and `dssh_chan_write()` handle channel backpressure without changing session timeout state or requiring a flush. |
| FTP's save/set/restore read timeout, MQTT/Web's zero-timeout drain, and JavaScript Socket changing timeout for peek/nonblocking reads | Cryptlib offers a mutable session attribute where callers need a deadline on one operation | Delete.  xptls receives a timeout on each I/O call and `xp_tls_has_pending()` reports buffered plaintext; one call cannot change a later call's timeout. |
| The `0x2000` SSH/SFTP write cap | Cryptlib `PushData`/`FlushData` behavior required defensive chunking copied from another transport | Delete.  `dssh_chan_write()` returns actual partial progress and enforces the negotiated channel window. |
| `drain_ssh` and the special “one SSH packet per pop” input loop | `cryptPopData()` can leave decrypted channel data hidden behind an unreadable socket | Delete.  `dssh_chan_poll()` reports DeuceSSH's per-channel buffered data independently of kernel socket readability. |
| `CRYPT_PROPERTY_OWNER`, `ssh_session_destroy()`, `SSH_END`, and session-count changes coupled to destruction success | Cryptlib attaches thread ownership and cleanup to a generic handle | Delete.  `sbbs_ssh_session_t` has one idempotent owner-driven teardown path; connection metrics, if retained, are changed independently of library cleanup. |

The current `check_pubkey()` policy is retained but not its Cryptlib buffer
shape.  Cryptlib's public-key attribute includes a leading length field, which
causes the current code to compare `pksz - 4` bytes starting at `pkey + 4`.
DeuceSSH's authorization callback supplies the complete RFC 4253 public-key
blob.  The replacement parser therefore decodes the OpenSSH authorized-key
blob and compares the complete byte strings; it must not preserve the
four-byte offset.

The following are Synchronet semantics and remain even though their current
implementation is interleaved with Cryptlib:

- password, public-key, `SSH_ANYAUTH`, unknown-user/new-user, sysop secondary
  password, three-failure, shell, and SFTP policy;
- the authorized-key location and parsing of options/comments;
- one shell plus one SFTP channel, terminal dimensions/window changes, and
  independent channel closure;
- socket ownership, input/output ring backpressure, inactivity bounds, and
  clean shutdown of blocked worker threads;
- certificate hot reload with old-session survival; and
- configured minimum logging levels and protocol/socket/node/peer context.

`src/syncterm/ssh.c` is useful interoperability evidence, not an sbbs3 adapter
template.  Its historical `init_crypt()`/`exit_crypt()` names and
`crypt_initialized` flag are not copied.  Likewise, sbbs3 never enables
`DSSH_PARAM_ACCEPT_EARLY_DATA`; that is an opt-in DeuceSSH client workaround
for Cryptlib-based servers, not server behavior.  DeuceSSH's own termination,
rekey, transport, and channel synchronization remains library-owned and is
not deleted merely because it lives in `src/ssh/ssh.c`.

## xptls capability boundary

The server port uses the provider-neutral contracts in `src/xptls`; it never
reaches into the OpenSSL or Botan backend.  Application migration may begin
from the TLS server, key, signing, digest, KDF, cipher, and CA APIs defined
here.  The remaining xptls extensions are listed explicitly below.

### Shared server credentials

The TLS server identity is an opaque, immutable, reference-counted credential
object:

```c
typedef struct xp_tls_server_credentials *xp_tls_server_credentials_t;

struct xp_tls_server_credentials_config {
    const char *certificate_chain_file; /* PEM, leaf first */
    xp_key_t private_key;                /* retained on success */
};

int xp_tls_server_credentials_load(
    xp_tls_server_credentials_t *out,
    const struct xp_tls_server_credentials_config *config);
void xp_tls_server_credentials_retain(xp_tls_server_credentials_t credentials);
void xp_tls_server_credentials_release(xp_tls_server_credentials_t credentials);
```

Loading verifies all of the following before returning success:

- the complete PEM chain parses;
- the private-key handle is usable for the certificate's signature scheme;
- the leaf certificate matches the private key;
- the leaf is within its validity interval and usable as a TLS server
  certificate; and
- no trailing non-certificate PEM object is silently ignored.

A TLS session retains its credentials.  Reloading and releasing the manager's
old reference therefore cannot invalidate established sessions.

### Server sessions

`xp_tls_server_open()` performs a synchronous server handshake over an
existing socket:

```c
enum xp_tls_client_auth {
    XP_TLS_CLIENT_AUTH_NONE,
    XP_TLS_CLIENT_AUTH_REQUEST_UNVERIFIED,
    XP_TLS_CLIENT_AUTH_REQUIRE_VALID,
};

typedef int (*xp_tls_psk_lookup_cb)(
    void *arg, const void *identity, size_t identity_len,
    void *key, size_t *key_len);

struct xp_tls_server_config {
    enum xp_tls_version min_version;
    enum xp_tls_version max_version; /* UNKNOWN means provider maximum */
    int handshake_timeout_ms;
    enum xp_tls_client_auth client_auth;
    const char *client_ca_file;      /* required only for REQUIRE_VALID */
    xp_tls_psk_lookup_cb psk_lookup;
    void *psk_lookup_arg;
    enum xp_tls_psk_policy psk_policy;
};

xp_tls_t xp_tls_server_open(
    SOCKET socket, xp_tls_server_credentials_t credentials,
    const struct xp_tls_server_config *config);
```

`psk_lookup` is called only during `xp_tls_server_open()`.  It receives the
identity as counted bytes, never as an assumed C string.  On input `*key_len`
is capacity; on success it is the raw key length.  The callback returns zero
for a match and non-zero for an unknown identity or insufficient buffer.  The
backend cleanses its temporary key copy after the handshake.

Certificate and PSK authentication may be enabled together.  Certificate
credentials are still required in that mode so ordinary certificate clients
can connect.  The selected authentication method is queryable after the
handshake.

`REQUEST_UNVERIFIED` deliberately means "ask for a certificate, retain the
presented chain, and let the application inspect it without making it an
authentication assertion."  It exists to preserve JavaScript `Socket`'s
current manual-client-certificate behavior.  Built-in listeners do not use
it.  `REQUIRE_VALID` validates to the explicit CA file and fails the handshake
when the peer has no acceptable certificate.

### Common session operations

The common session API provides:

```c
int xp_tls_pop_timeout(
    xp_tls_t session, void *buf, size_t len, size_t *copied,
    int timeout_ms);
int xp_tls_push_timeout(
    xp_tls_t session, const void *buf, size_t len, size_t *copied,
    int timeout_ms);
int xp_tls_flush_timeout(xp_tls_t session, int timeout_ms);
const char *xp_tls_cipher_name(xp_tls_t session);
int xp_tls_psk_identity(
    xp_tls_t session, void *identity, size_t *identity_len);
size_t xp_tls_peer_certificate_count(xp_tls_t session);
int xp_tls_peer_certificate_der(
    xp_tls_t session, size_t index, void *out, size_t *len);
enum xp_tls_version xp_tls_protocol_version(xp_tls_t session);
enum xp_tls_auth_method xp_tls_authentication_method(xp_tls_t session);
int xp_tls_terminate(xp_tls_t session);
void xp_tls_close(xp_tls_t session, bool close_socket);
```

`timeout_ms` belongs to that high-level operation: zero means do not wait,
positive values bound the whole call, and negative means no deadline.  If a
TLS read must write protocol traffic, or a write must read protocol traffic,
the initiating operation's one deadline still applies.  RX work never reads
or changes a TX deadline and vice versa.  Backends use monotonic elapsed time
so retrying provider `WANT_READ`/`WANT_WRITE` states cannot restart the bound.

All callers use the timeout variants; sessions contain no mutable default I/O
timeout and expose no constructor-configured `xp_tls_pop()`, `xp_tls_push()`,
or `xp_tls_flush()` calls.  xptls must not set or temporarily modify
`SO_RCVTIMEO` or `SO_SNDTIMEO` on the caller-owned socket.  Backend readiness
waits or I/O callbacks implement the deadline without exposing
direction-coupled socket state to another thread.

A session supports one application reader and one application writer at the
same time.  Calls on the same direction remain caller-serialized, including
`push` plus `flush`.  Internally, a backend does not hold its provider-state
lock while waiting for socket readiness: a blocked read must not prevent an
independent write from reaching the peer, and a blocked write must not consume
or replace the read call's deadline.  TLS protocol work in the opposite
direction (for example post-handshake messages) runs under the initiating
call's deadline and wakes the other side as necessary.  Close/termination
wakes both directions according to the socket-ownership sequence above.

`xp_tls_client_config` includes `min_version`, `max_version`, an explicit PSK
policy, and a private-key password callback.  The configured interface accepts
`XP_TLS_SERVER_AUTH_NONE`; that is required for deliberately opportunistic
SMTP and for compatibility with a JavaScript socket whose verification flag
is false.  All sbbs3 clients use `xp_tls_client_open_config()`.

TLS PSK configuration also carries an explicit policy.  The default modern
policy permits ephemeral TLS 1.2 PSK exchange with AEAD ciphers.  The TLS 1.2
compatibility policy additionally permits plain PSK and AES-CBC/SHA for older
brokers.  TLS 1.3 remains forward-secret under both policies.

### Core crypto, KDF, and streaming cipher primitives

`xp_crypto` defines the shared status domain, counted secret-callback contract,
provider name and version, cryptographic random generation, stable status text,
and thread-local diagnostics for constructors which cannot return a handle.
`xp_digest` supplies one-shot and streaming SHA-256/384/512.  `xp_sign` and
`xp_verify` operate on complete messages using RSA PKCS#1 SHA-2, ECDSA SHA-2
with DER or P1363 encoding, and Ed25519.

`xp_kdf` exposes PBKDF2 with an explicit digest and scrypt with explicit
`N`, `r`, and `p`.  Both providers enforce a fixed 1025 MiB per-derivation
scrypt limit.  It is not a process-wide pool and concurrent derivations each
have their own limit.

`xp_cipher` exposes typed algorithm, mode, direction, padding, key, IV, and
tag configuration.  AES-CBC/CFB/GCM and IETF ChaCha20 are available for normal
use.  3DES-CBC, CAST128-CBC, and RC4 are capability-queried decrypt-only
migration algorithms.  ChaCha20 takes a 12-byte nonce and a separate 32-bit
initial block counter; provider-specific packed IV layouts are never public.
Unauthenticated modes produce incremental output, while authenticated
decryption withholds plaintext until tag verification.  A too-small output
buffer does not consume input.

`enum xp_tls_version` includes TLS 1.0 and 1.1 so the JavaScript property has an
exact representation.  xptls does not weaken provider policy to make those
versions work.  Failure because a backend disables an obsolete version is a
normal, explicit handshake error.

The read contract is made unambiguous and both backends must match it:

- `XP_TLS_OK` means plaintext was produced; `*copied > 0` and may be smaller
  than requested.  A zero-length caller request is the sole successful
  `*copied == 0` exception.
- `XP_TLS_TIMEOUT` means no application data arrived before the configured
  deadline and always returns `*copied == 0`.  If a backend obtains any
  plaintext before an underlying wait expires, that call returns
  `XP_TLS_OK`; the timeout can be reported by a later call only.
- `XP_TLS_ERR_CLOSED` means clean close, reset, or a write to a closed peer.
- `XP_TLS_ERR` means a protocol/provider failure and has a non-empty
  `xp_tls_errstr()`.

`xp_tls_has_pending()` remains mandatory.  Server event loops must check it in
addition to socket readability because decrypted plaintext can remain inside
the provider after the kernel receive queue is empty.

### xptls tests

The OpenSSL and Botan implementations use the same provider-independent test
programs.  The TLS loopback/socket-pair suite must cover:

- certificate server to permissive and Web-PKI/explicit-anchor clients;
- wrong key/certificate pairs and malformed or extra PEM objects;
- TLS 1.2 and TLS 1.3 negotiation and version-floor rejection;
- successful and unknown server PSKs, certificate/PSK coexistence, selected
  identity reporting, and temporary-key cleansing where instrumentable;
- optional unverified and required validated client certificates;
- peer-chain DER access on both sides;
- zero/finite/infinite per-call deadlines with the invariant
  `XP_TLS_TIMEOUT => copied == 0`, partial I/O returning `XP_TLS_OK`, pending
  plaintext, clean close, reset, and close during handshake;
- a read that requires a protocol write and a write that requires a protocol
  read, proving each uses only its initiating deadline and leaves both socket
  timeout options unchanged; and
- concurrent reader/writer tests in which one direction is deliberately
  blocked past the other direction's shorter deadline, proving neither
  operation overwrites, extends, or inherits the other's deadline; and
- releasing/replacing the caller's credential reference while an established
  session continues to transfer data.

The disabled backend receives matching linkable stubs and a test that every
constructor fails with `XP_CA_ERR_DISABLED` or the xptls equivalent and a
useful diagnostic.

The common crypto suite covers SHA-2, PBKDF2, bounded scrypt, AES-CBC/CFB/GCM,
IETF ChaCha20, decrypt-only migration ciphers, RSA/ECDSA/Ed25519 signatures,
standard key encodings and public components, encrypted file storage, stable
references, and capability-gated PKCS#11 fixtures.  The CA suite covers CSR
proof of possession, certificate issuance and validation, exact identity and
profile matching, certificate-chain PEM, CRLs, and revocation.  Its Botan
coverage additionally includes IPv4 and IPv6 subjectAltName extraction,
native issuing-distribution-point encode/decode, and the RFC 5280
implicit-distribution-point case in which a complete same-issuer CRL has no
`issuingDistributionPoint` but the certificate has
`CRLDistributionPoints`.  Negative controls retain mismatched scoped and
indirect-CRL cases.

### Remaining xptls extensions

The Cryptlib replacement requires these additions beyond the available xptls
contracts:

- a TLS client-identity configuration which accepts an `xp_key_t` and
  certificate chain, so a client certificate can use a non-exportable stored
  key without an unencrypted private-key filename;
- the additional CSR and certificate import, export, extension, and metadata
  operations specified for `CryptCert` compatibility;
- the `xp_keyset` persistence API specified below; and
- TPM 2.0 and platform-keystore adapters for every store the build claims to
  support, plus any hardware import operation advertised by a store's
  capability mask.

Each extension remains provider-neutral, has matching Botan, OpenSSL, and
disabled-stub behavior, and is added to the shared xptls tests before an sbbs3
consumer depends on it.

## TLS identity and persistence

### Standard public files and key references

The shared server identity moves from the proprietary `ctrl/ssl.cert` keyset
to a standard certificate chain plus a storage-backed key reference:

| File | Contents | Unix mode |
|---|---|---|
| `ctrl/ssl.crt` | PEM certificate chain, leaf first | `0644` |
| `ctrl/keyrefs.ini` | storage kind, non-secret locator, algorithm, and public fingerprint for logical service keys | `0600` |
| `ctrl/ssl.key` | encrypted PKCS#8 PEM private key, present only for the file backend | `0600`, or `0640` only when the existing group-readable setting explicitly requests it |

`ssl.cert` is not reused for a different format.  Keeping a distinct filename
makes downgrade and recovery unambiguous and lets a transition release keep
the old and new identities side by side.

The automatic fallback identity is RSA-3072 for broad TLS compatibility.  It
has a ten-year validity, the system Internet address as CN and DNS SAN, and no
fake country or email attribute.  The key is generated using the configured
storage policy.  A file-backed key is encrypted with the non-empty system
password.  The certificate and key reference, plus the key file when
applicable, are staged, parsed back, checked for a match, and committed only
after all are complete.  A failed transaction deletes any newly-created
hardware object when possible; otherwise it records and reports the orphan
for explicit cleanup.

ACME may install an RSA or ECDSA leaf.  xptls must accept either regardless of
which provider produced the files.

### `ssl.c` becomes the identity manager

`ssl.c` remains the shared server-security module, with a public surface based
only on xptls types.  Cryptlib error translation, the context pool, session
list, and certificate epoch lists are deleted.

The manager owns one reference to the current
`xp_tls_server_credentials_t`.  It records file identity for `ssl.crt`, the
TLS entry in `keyrefs.ini`, and `ssl.key` when that entry selects the file
backend (device/inode where available, size, and high-resolution modification
time).  On sync:

1. stat the certificate and applicable reference/key files;
2. if any differs, open the bound key and fully validate a new credential object without
   holding the manager write lock;
3. swap it under the lock only after successful validation; and
4. release the old manager reference after the swap.

A failed reload leaves the last valid identity active and emits a
rate-limited error containing the path and xptls diagnostic.  Initial load
failure disables TLS listeners.  Existing sessions are unaffected in either
case.

The replacement public surface is intentionally small:

```c
bool sbbs_tls_init(scfg_t *, sbbs_lprintf_t);
bool sbbs_tls_sync(scfg_t *, sbbs_lprintf_t);
xp_tls_server_credentials_t sbbs_tls_credentials_acquire(void);
void sbbs_tls_credentials_release(xp_tls_server_credentials_t);
```

Callers acquire credentials, call `xp_tls_server_open()`, and release their
caller reference.  The session retains its own reference.

### TLS endpoint conversion

All application structs and function parameters change from `CRYPT_SESSION`
and sentinel `-1` to `xp_tls_t` and `NULL`.  Common send/read helpers use
`xp_tls_push_timeout()`, `xp_tls_flush_timeout()`,
`xp_tls_pop_timeout()`, and `xp_tls_has_pending()`.  Return-code translation
belongs in one sbbs3 TLS helper, not duplicated Cryptlib-shaped macros in
every server.  FTP, MQTT, Web, and JavaScript peek/drain paths pass zero to a
single operation; they do not save, set, or restore session timeout state.

| Consumer | Required policy |
|---|---|
| FTP control and data TLS (`ftpsrvr.cpp`) | server credentials, TLS 1.2 minimum, existing inactivity timeout; preserve the raw `234` response before an explicit handshake |
| POP3S and STLS (`mailsrvr.cpp`) | server credentials, TLS 1.2 minimum, existing inactivity timeout |
| SMTPS and inbound STARTTLS (`mailsrvr.cpp`) | server credentials, TLS 1.2 minimum, existing inactivity timeout; advertise STARTTLS only while valid credentials are available |
| Outbound SMTP STARTTLS (`mailsrvr.cpp`) | client, `SERVER_AUTH_NONE`, TLS 1.2 minimum, no bounce solely for TLS failure; do not present the BBS server certificate |
| HTTPS (`websrvr.cpp`) | server credentials, TLS 1.2 minimum, blocking read after handshake |
| JavaScript services (`services.cpp`) | server credentials, TLS 1.2 minimum |
| MQTT broker (`mqtt_broker.cpp`) | server credentials plus per-broker PSK lookup, TLS 1.2 minimum; preserve selected PSK identity |
| MQTT client (`mqtt_client.cpp`) | configured PSK for PSK modes; Web PKI plus hostname and optional client identity for certificate mode |
| JavaScript `Socket` (`js_socket.cpp`) | configuration described below |

The deleted user-password PSK loops in `websrvr.cpp` and `services.cpp` are not
restored.  User passwords are not bulk-loaded as TLS PSKs.  MQTT's explicit
PSK table remains supported.

No endpoint tests `do_cryptInit()` to decide whether to advertise an upgrade.
It tests whether `sbbs_tls_credentials_acquire()` succeeds.  STARTTLS/STLS
state changes occur only after the plaintext readiness response has been sent
and the xptls handshake succeeds.

### JavaScript `Socket` TLS compatibility

`js_socket_private_t::session` becomes `xp_tls_t`.  The existing properties
retain these meanings:

- `ssl_session = true`: open a TLS client over the connected socket;
- `ssl_server = true`: open a TLS server with shared credentials;
- `tls_minver`: map `100`, `101`, and `102` to TLS 1.0, 1.1, and 1.2;
- `tls_nameverify` and `tls_certverify`: independently select hostname and
  chain validation for client sessions;
- `tls_psk`: copy the JavaScript identity/key pairs into a native temporary
  lookup table before suspending the JS request and entering the handshake;
- `tls_psk_id`: return `xp_tls_psk_identity()` only when PSK was selected;
- `tls_client_auth`: on a server use `REQUEST_UNVERIFIED`; on a client present
  the shared certificate and key; and
- `tls_remote_cert`: construct a `CryptCert` compatibility object from peer
  certificate DER after a successful handshake.

The provider never calls into SpiderMonkey during a handshake.  This is why
the PSK object must be copied first.  Copies of PSK values are cleansed on all
success and error exits.

`tls_enhanced_certcheck` remains present for script compatibility but is
documented as a deprecated no-op: xptls performs its backend's normal strict
validation and has no Cryptlib "reduced compliance" mode.  It must not weaken
validation while reporting that enhanced checking is enabled.

The `is_readable`, `data_waiting`, `MSG_PEEK`, event, and nonblocking paths use
the existing one-byte application peek buffer plus `xp_tls_has_pending()`.
They pass a zero timeout to `xp_tls_pop_timeout()` when a plaintext probe is
needed and do not alter the timeout used by the next blocking read.  They do
not peek at encrypted socket bytes.  Changing `descriptor` or closing the JS
object closes the xptls session before replacing/closing the socket.

## DeuceSSH integration

### Small API additions

The integration uses DeuceSSH's typed handshake, authentication, channel,
event, and I/O operations.  Required DeuceSSH additions are limited to explicit
status reporting for pointer-returning constructors/acceptors and the hardware
signer described below:

```c
const char *dssh_strerror(int status); /* static text for DSSH_ERROR_* */
const char *dssh_session_errstr(dssh_session session); /* borrowed detail */

dssh_channel dssh_chan_accept_ex(
    dssh_session session, const struct dssh_chan_accept_cbs *callbacks,
    int timeout_ms, int *status_out);
```

`dssh_chan_accept_ex()` sets `status_out` to success, timeout, termination,
rejection, or the underlying error whenever it returns `NULL`; the existing
convenience function may remain for source compatibility.  Constructors that
can return only `NULL` use the library's existing constructor-error mechanism
or gain an equivalent status-returning form.  sbbs3 must not infer failure
causes from mutable global state or add a Cryptlib-style generic attribute
interface.

### Process-wide initialization and host keys

DeuceSSH's algorithm registry and host-key contexts are process-wide and must
be finalized before the first session is created.  Terminal-server startup
does this once, before opening the SSH listener.

Register server algorithms in preference order:

- KEX: `mlkem768x25519-sha256`, `sntrup761x25519-sha512`,
  `curve25519-sha256`, `diffie-hellman-group-exchange-sha256`;
- host key: `ssh-ed25519`, `rsa-sha2-512`, `rsa-sha2-256`;
- encryption: `aes256-ctr`;
- MAC: `hmac-sha2-512`, `hmac-sha2-256`; and
- compression: `none`.

Do not register `aes128-cbc`, `none` encryption, or `none` MAC on the server.
They exist for tests and narrowly-scoped client compatibility, not for an
Internet-facing listener.  Per-session filters repeat this allow-list so a
future globally-registered client-only algorithm cannot accidentally become a
server offer.

Host keys use these logical references and public files:

| Logical key/file | Contents |
|---|---|
| `ssh-host-ed25519` in `ctrl/keyrefs.ini` | hardware locator or file-key reference |
| `ctrl/ssh_host_ed25519_key.pub` | OpenSSH public-key line |
| `ctrl/ssh_host_ed25519_key` | encrypted PKCS#8 PEM only for the file backend |
| `ssh-host-rsa` in `ctrl/keyrefs.ini` | hardware locator or file-key reference |
| `ctrl/ssh_host_rsa_key.pub` | OpenSSH public-key line |
| `ctrl/ssh_host_rsa_key` | encrypted PKCS#8 PEM only for the file backend |

Missing keys are generated through the configured `xp_key` store.
Extend DeuceSSH with an opaque host-signer registration containing public-key
bytes, a typed signing callback, and retain/release callbacks.  The sbbs
adapter implements it with `xp_key_t`, so DeuceSSH never asks to export a
hardware key.  The two RSA signature algorithms use the same key
handle.  Startup logs storage kind and SHA-256 fingerprints in OpenSSH
notation.  Host-key reload requires terminal-server restart because DeuceSSH
intentionally freezes global signer contexts after its first session.

`dssh_transport_set_version()` reports the Synchronet version without spaces
in the software token.  Global socket callbacks are registered once and use
per-session callback data; no callback accesses global "current connection"
state.

### The sbbs SSH owner

Add `ssh_session.{h,cpp}` as the only sbbs3 adapter over DeuceSSH.  Its opaque
`sbbs_ssh_session_t` owns:

- the `dssh_session`;
- the referenced socket, its address, and termination flag;
- copied authenticated username and the minimum authentication state needed
  by `answer()`;
- at most one shell `dssh_channel` and one SFTP `dssh_channel`;
- a mutex and condition variable protecting channel publication and closure;
- the channel-accept worker; and
- the I/O callback context and error string.

`sbbs.h` stores only `sbbs_ssh_session_t *`; the integer
`session_channel`/`sftp_channel` fields disappear.  The adapter has one
idempotent destroy function.  A caller can request termination from any
thread, but exactly one owner joins the accept worker and calls
`dssh_session_cleanup()`.

The DeuceSSH terminate callback calls `shutdown(socket, SHUT_RDWR)` to wake
blocked exact-length I/O.  It does not call another DeuceSSH function.  TX and
RX callbacks loop on partial `send`/`recv`, retry `EINTR`, respect session
termination, and turn reset/EOF into a negative `DSSH_ERROR_*`.  The optional
gather callback uses `writev`/`WSASend` only where the platform wrapper can
guarantee complete ordered transmission; otherwise it is omitted.

### Handshake and authentication

For an accepted SSH socket:

1. create the adapter and DeuceSSH server session;
2. set callback data, termination callback, algorithm filters, and
   `startup->ssh_connect_timeout * 1000` as the pre-start operation timeout;
3. run `dssh_transport_handshake()`;
4. run `dssh_auth_server()` with the callbacks below;
5. set the DeuceSSH operation timeout to disabled before
   `dssh_session_start()`; normal BBS and SFTP inactivity policy remains the
   authority after login;
6. start the demultiplexer and channel-accept worker; and
7. wait no longer than the SSH connect timeout for the first accepted shell
   or SFTP channel before handing the connection to `answer()`.

The auth method list is `publickey,password`.  Keyboard-interactive is not
advertised in the first port because the current server does not expose it.
When `BBS_OPT_SSH_ANYAUTH` is set, add `none` and make all presented methods
succeed; this preserves the option's deliberately insecure meaning.

Authentication callbacks use the same login-ID parser and account password
verifier as other BBS logins:

- password auth copies the counted username into a bounded, NUL-terminated
  buffer, finds the account, and verifies the counted password;
- public-key auth compares the complete decoded authorized-key blob with
  DeuceSSH's complete `pubkey_blob`; it does not skip an assumed four-byte
  prefix;
- the no-signature public-key probe returns success only when that key would
  be authorized, without recording a successful login;
- comments and options after an authorized-key blob are not part of the key
  comparison;
- malformed, overlong, or unsupported authorized-key lines are skipped with a
  rate-limited diagnostic, not treated as a partial match; and
- temporary password and decoded-key buffers are cleansed before release.

An unknown username with a password is allowed through SSH authentication so
the existing interactive new-user flow can run; the attempted username and
password are still recorded through the normal bad-login policy.  An unknown
user cannot authenticate by public key and cannot enter SFTP, because there is
no account to authorize the key or own transferred files.

Three rejected credential attempts end the SSH session with
`DSSH_AUTH_DISCONNECT`.  The adapter records successful username/method for
`answer()`; it does not reproduce the current `CRYPT_ENVELOPE_RESOURCE` /
`AUTHRESPONSE` retry state machine.  The normal sysop system-password check
still runs after a shell has been accepted.

The connection setup is a linear typed sequence:
`dssh_transport_handshake()`, `dssh_auth_server()`,
`dssh_session_start()`, then `dssh_chan_accept_ex()`.  There is no
`ssh_active` event, repeated `SESSINFO_ACTIVE` call, auth-response setter,
owner-clearing operation, or polling for a channel's type attribute to change
from `session` to `shell`/`subsystem`.

### Channel acceptance

The accept worker repeatedly calls `dssh_chan_accept_ex()` until session
termination.  Its callbacks implement this policy:

- accept `pty-req` and retain terminal name, columns, rows, pixel size, and
  modes;
- accept environment entries into DeuceSSH's bounded parameter object but do
  not copy them into the process environment;
- accept the first `shell` channel and reject subsequent shell channels;
- accept the first `subsystem` named exactly `sftp` only when
  `BBS_OPT_ALLOW_SFTP` is set, `BBS_OPT_SSH_ANYAUTH` is not being used as the
  sole authority, and a real user account authenticated;
- reject other subsystems and all `exec` requests; and
- reject rather than queue channels when the application slot is occupied.

Publication of either accepted channel wakes the thread waiting for the first
usable channel.  Initial shell terminal metadata populates `terminal`,
`term->cols`, and `term->rows`.  `DSSH_EVENT_WINDOW_CHANGE` updates dimensions
during the session.  EOF and CLOSE wake BBS input/output waiters.  Unsupported
signal and break events are logged at debug level and consumed; they are not
injected as terminal bytes.

The existing limit of one active shell plus one active SFTP channel is
preserved.  Closing one does not terminate the other.  The connection ends
when both have closed and no replacement is accepted before the normal wait
deadline, or when the BBS explicitly hangs up.

### Shell and SFTP I/O

The input thread replaces Cryptlib channel selection and `cryptPopData()` with
`dssh_chan_poll(shell, DSSH_POLL_READ | DSSH_POLL_EVENT, timeout)` followed by
`dssh_chan_read()`.  It drains events in order and writes only shell data to
the terminal input ring.

The output thread waits for `DSSH_POLL_WRITE` and loops over partial
`dssh_chan_write()` results.  `DSSH_ERROR_NOMORE` is a would-block result, not
a disconnect.  No explicit flush exists or is needed.  Existing output-ring
backpressure and the dead-peer hangup bound remain in force.

The SFTP library is unchanged above its transport callbacks.  `sftp.cpp`
stores/acquires the adapter's SFTP channel and replaces channel-selection plus
`cryptPushData()` with the same poll/write loop.  Inbound SFTP bytes come from
`dssh_chan_read(sftp, 0, ...)`, never through the shell input path.  This keeps
SSH framing and channel windows inside DeuceSSH and SFTP packet parsing inside
the existing SFTP server.

Adapter tests must exercise a shell and SFTP channel concurrently, either
channel closing first, window changes, a client that stops reading, and
termination while DeuceSSH's RX callback is blocked.  A blocked shell read
while shell and SFTP writes make progress proves that no adapter path changes
a session-wide read/write timeout or performs the old `5` → flush → `0`
sequence.

## JavaScript crypto compatibility

Removing Cryptlib is not complete while `js_cryptcon.cpp`,
`js_cryptcert.cpp`, or `js_cryptkeyset.cpp` still include its header.  These
classes stay public but become compatibility facades over typed xptls APIs.

### Constants and availability

Copy the published numeric algorithm, mode, certificate type, format, cursor,
keyset option, and attribute values into a Synchronet-owned
`js_crypto_constants.h`.  Existing property names and numbers do not change
merely because the provider changed.

Keeping a constant does not promise that every provider implements it.  A
constructor or operation for an unavailable algorithm throws
`"<algorithm> is not supported by the <provider> build"`.  It must not fall
back to another algorithm.  This matches Cryptlib's existing reality for
disabled legacy algorithms while keeping feature detection stable.

The required cross-provider baseline is:

| JavaScript need | xptls mechanism |
|---|---|
| SHA-256/384/512 streaming digest and `hashvalue` | `xp_digest` |
| RSA-2048/3072/4096 and P-256/P-384/P-521 key generation/import/export | `xp_key` |
| RSA PKCS#1 SHA-2 and ECDSA SHA-2 signatures | `xp_sign` |
| RSA and EC JWK public components | `xp_key_get_rsa_public()` and `xp_key_get_ec_public()` |
| AES and ChaCha20 contexts used by supported scripts | `xp_cipher` streaming/context API |
| CSR and X.509 import/export/signing | `xp_ca` additions below |
| PKCS#12 and labelled key/certificate persistence | new `xp_keyset` API |

DES, 3DES, IDEA, CAST, RC2, RC4, DSA, ElGamal, MD5, SHA-1, and their old HMAC
variants are compatibility constants but not part of the mandatory baseline.
They may be enabled only when both providers implement the operation safely
or when a separately-audited legacy module is explicitly compiled.  New
repository code must not use them.

The `hotline.js` Blowfish/OFB path uses constants outside the published
`CryptContext` surface and is not part of this compatibility baseline.  A
separate Hotline compatibility fix must either carry an audited Blowfish/OFB
implementation or negotiate a supported mode.

### `CryptContext`

The private object is a tagged union of digest, symmetric-cipher, and
public-key handles.  Method/property behavior is retained for the mandatory
baseline:

- `encrypt()` updates a digest context and returns the input bytes, as the
  current scripts expect; for a cipher it transforms the bytes;
- reading `hashvalue` finalizes a digest exactly once;
- `generate_key`, `set_key`, `derive_key`, key size, IV, mode, label, and
  algorithm properties validate by context kind;
- `create_signature(key)` signs the finalized digest with the supplied key;
- RSA/ECDSA `public_key` exposes the existing JWK-shaped `n`/`e` or
  `x`/`y` object; and
- raw RSA private "decrypt a hand-built PKCS#1 signature block" is removed
  from `acmev2.js` and replaced with `xp_sign` through
  `create_signature()`.  The compatibility `decrypt()` method is not a new
  general raw-RSA API.

The ACME change is important: raw private RSA operations are an accidental
Cryptlib implementation detail, not a behavior to reproduce in xptls.

### `CryptCert`

`xp_ca` supplies RSA/ECDSA CSR signing, common-name and DNS-SAN identities,
DER CSR and certificate import/export, PEM certificate-chain import/export,
validity access, public-key extraction, certificate issuance, and certificate
path validation.  Extend it with organization and the remaining typed CSR
subject fields, arbitrary extension DER for CSR construction, PEM/text and
PKCS#7 forms, and the certificate getters needed by the published JavaScript
properties.  Draft attributes are stored until `sign()`; parsed certificate
attributes are read-only provider data.

The mandatory compatibility set is the behavior exercised by
`certtool.js`, `letsyncrypt.js`, `acmev2.js`, the JavaScript docs generator,
and `exec/tests/crypt/cert.js`:

- create and sign RSA or ECDSA CSRs;
- subject public key, common name, organization, and DNS SAN extension;
- import DER certificate, PEM certificate/chain, and PKCS#7 certificate
  bundles;
- export DER, PEM/text certificate, CSR, and chain forms;
- signature/structural check;
- label, type, validity, subject/issuer, serial, fingerprints, public key,
  basic constraints, key usage, EKU, and SAN getters; and
- construction from `xp_tls_peer_certificate_der()`.

Keep all documented attribute constants so scripts can feature-test them.
An attribute outside the implemented typed map throws a named unsupported
attribute error rather than returning a plausible zero/empty value.  Adding a
typed xptls getter is preferred to exposing provider-specific generic OID
lookups.

### `CryptKeyset` and the new on-disk format

Add an xptls `xp_keyset` abstraction backed by PKCS#12 for portable import and
export and by an atomic Synchronet manifest for multi-label working keysets.
The manifest is a documented UTF-8 file containing a version, labels, object
kind, DER certificate bytes, and either encrypted PKCS#8 or a versioned
`xp_key_t` storage reference.  File private-key records remain individually
encrypted by the supplied non-empty password; the manifest is not treated as
encryption.  Hardware locators contain no PIN or other
authorization secret.

The API supports the existing operations: create/read-write/read-only open,
close, add/get/delete private key by label, and add/get public certificate or
chain by label.  A `.p12`/`.pfx` path uses PKCS#12.  Other new keysets use the
manifest.  Writes take an exclusive lock, validate the complete replacement,
fsync where supported, and atomically rename.  Concurrent readers see the old
or new complete file, never a partial edit.

Exporting a non-exportable hardware key to PKCS#12 or raw private-key bytes
fails with the explicit not-exportable error.  Signing, public-key access,
certificate matching, and CSR creation continue to work through the handle.
JavaScript never receives a provider pointer or a hardware authorization
value.

Cryptlib keyset bytes are not parsed by xptls.  The transition process below
converts them while Cryptlib is still present.

### In-tree script changes

`letsyncrypt.js` stops using a single Cryptlib keyset as the live TLS file.
It stores:

- ACME account keys as logical storage-backed keys below the Synchronet
  manifest namespace, keyed by a filesystem-safe hash of the ACME directory
  URL and account hostname;
- the pending service key as a temporary logical key in the configured store;
  file policy uses a private temporary PEM;
- the issued leaf/chain in a temporary PEM chain.

After verifying that the chain matches the key and contains all requested
DNS names, it atomically promotes the pending key reference, replaces
`ssl.crt`, and touches the existing recycle semaphore.  Under file policy the
transaction also replaces `ssl.key`.  At no point is a new certificate paired
with an old key.  Failed orders remove their temporary file key or destroy the
temporary hardware object when supported; an undeletable orphan is reported
without becoming the active identity.

`certtool.js` reads and writes PEM/PKCS#12 explicitly and reports which format
it selected.  `acmev2.js` uses the signature API rather than manual RSA block
construction.  `sha256.js` continues to work through the compatibility
`CryptContext`, with an added test that binary file position is preserved.

## Migration from Cryptlib keysets

### Why a transition release is required

The files `ssl.cert`, `cryptlib.key`, and `letsyncrypt.key` are proprietary
Cryptlib keysets.  OpenSSL and Botan cannot be expected to read them.  Waiting
until the final no-Cryptlib binary to discover that is too late to preserve
private keys.

Ship one transition release that carries Cryptlib solely for a native
migration executable and contains:

- the standard xptls key/certificate/keyset APIs;
- a native `cryptmigrate` utility able to open known Cryptlib labels with the
  system password and export standard PKCS#12;
- xptls import of that PKCS#12 into the configured key store plus PEM chains;
  `auto` imports into hardware when lossless private-key import is supported,
  otherwise it preserves the key in encrypted PKCS#8;
- validation and dry-run reporting.

Known live labels are sufficient; arbitrary Cryptlib keyset enumeration is
not required.  They are:

- `ssl_cert` and its public certificate chain in `ssl.cert`;
- `ssh_server` in `cryptlib.key`;
- the current ACME hostname account key in `letsyncrypt.key`; and
- `ssl_cert` / `ssl_certchain` in the current ACME service keyset.

### Native migration isolation

In the transition release the JavaScript names `CryptContext`, `CryptCert`,
and `CryptKeyset` refer only to their xptls
implementations.  They do not select an implementation by file contents,
expose a legacy mode, or load Cryptlib and xptls objects in the same
SpiderMonkey runtime.  Attempting to open a legacy Cryptlib keyset through the
xptls-backed `CryptKeyset` produces a specific error directing the operator to
the native migration utility.

`cryptmigrate` is a separately-linked, temporary native executable with no
SpiderMonkey or JSAPI dependency.  It is the only published transition target
allowed to link both Cryptlib and xptls.  Its Cryptlib-facing translation unit
opens a known legacy label and returns only a standard PKCS#12/DER buffer plus
public metadata to its xptls-facing coordinator; no Cryptlib handle, constant,
or error number crosses that boundary.  The coordinator performs storage
import, fingerprint comparison, transaction handling, and reporting through
the ordinary xptls APIs.

The utility is never loaded into `sbbs`, `jsexec`, a server DLL/shared library,
or an existing JavaScript process.  It obtains the system password through an
interactive protected input or an SCFG-launched private IPC channel, never a
command-line argument, environment variable, or JavaScript string.  The final
release removes this executable and its Cryptlib-only translation unit.

For the SSH key, the exporter may wrap the RSA key in a temporary self-signed
certificate solely to carry it through PKCS#12.  The imported object is
validated by comparing its SSH public blob before and after conversion.  If
the installed Cryptlib cannot export that private key, migration reports a
host-key rotation as required; it must not claim success and generate a new
key silently.

### Migration transaction

The native transition utility performs a dry run by default.  With explicit
apply:

1. acquire an exclusive migration lock in `ctrl`;
2. read the system password through the normal protected configuration path;
3. export each present known key to a private temporary PKCS#12 file;
4. import into temporary storage objects and standard certificate files;
5. validate key/certificate matches, certificate chain parsing, SSH public
   blob equality, and ACME key label/account mapping;
6. print old and new TLS and SSH public fingerprints;
7. atomically install only the validated new key references and files without
   overwriting an existing destination; and
8. write `ctrl/cryptmigrate.ini` recording source file identity, installed
   paths, fingerprints, time, and per-item result.

Source keysets are never deleted or renamed.  Temporary PKCS#12 files are
cleansed where practical and unlinked on every exit.  A second run is
idempotent: matching installed fingerprints are success, conflicting existing
standard files are an error requiring operator choice.

Migration never sacrifices identity continuity merely to satisfy an `auto`
preference.  If a selected hardware store cannot import an existing key,
`auto` installs that key using the encrypted file backend and reports the
decision.  An explicit `hardware`, `tpm2`, `pkcs11`, or platform policy instead
fails before commit and offers two operator-visible choices: select file
storage to preserve the fingerprint, or explicitly authorize generation of a
new non-exportable hardware key and the resulting identity rotation.

The final no-Cryptlib release follows this startup policy:

- if a standard TLS certificate and bound key reference exist and validate,
  use them;
- if only `ssl.cert` exists, generate a new self-signed standard identity and
  log a prominent message that the transition migration was skipped;
- if SSH standard keys do not exist, generate new Ed25519 and RSA keys and log
  old-key migration/rotation guidance;
- never overwrite a standard file merely because a legacy file also exists;
  and
- leave legacy files for downgrade/recovery and tell the operator when they
  are no longer read.

### System-password changes

`scfgsys.c::reencrypt_keys()` is rewritten over xptls.  Under its existing
password-change transaction it loads and re-saves, atomically, every present
file-backed encrypted standard key:

- `ssl.key`;
- both SSH host private keys;
- ACME account and pending keys below `ctrl/letsyncrypt`; and
- private-key records in Synchronet keyset manifests.

All keys are first proven readable with the old password.  If any required
key cannot be loaded or any replacement cannot be validated, no configuration
password change is committed.  Successful replacement cleanses both password
copies and temporary encoded keys.  Hardware-backed entries are not exported,
opened, or rewritten by a system-password change; their independent PIN,
token, or platform authorization lifecycle is unchanged.  Certificate files
need no rewrite.

## Errors, logging, and observability

Delete the Cryptlib status-to-log-level tables and every wrapper that expands
a call through them: `DO`, `GCES`, `GCESH`, `GCESHL`, `GCESNN`, `GCESS`,
`GCESSTR`, `HANDLE_CRYPT_CALL`, `get_crypt_error_string()`,
`HANDLE_CRYPT_CALL_EXCEPT*`, `handle_crypt_call*()`,
`do_cryptAttribute*()`, `get_crypt_error()`, `log_cryptlib_error()`, and
`log_crypt_error_status_sock()`.  These are not renamed to `XP_*` or
`DSSH_*` macros.

Each call site evaluates a typed library operation once, branches on its
documented result, and logs only an unexpected failure through one ordinary
TLS or SSH adapter function.  The adapter takes a structured context
containing operation, protocol, socket, node, peer, and configured severity
floor.  It combines `xp_tls_errstr()` or `dssh_session_errstr()` with the
stable text for the status.  It does not allocate an expanded message that a
caller must free and does not accept an unevaluated function call.

Non-session crypto code uses `xp_crypto_status_string()` for stable status
text, the applicable object diagnostic such as `xp_key_errstr()` or
`xp_cipher_errstr()`, and `xp_crypto_last_error()` or `xp_ca_last_error()` only
when a failed constructor could not return an object.  Diagnostics are copied
or logged before another operation on the same object or thread can replace
them.

Timeout, would-block, clean EOF, authentication rejection, and channel close
are control-flow results at the relevant I/O/auth boundary, not generic
errors.  Callers decide whether and at what level to log them.  Provider or
protocol failures carry a non-empty diagnostic; the adapter applies the
existing operator-configured severity floor without reproducing
`crypt_ll()`'s mapping of unrelated Cryptlib numbers.

The following are logged at notice or higher:

- TLS credential initial-load or reload failure;
- generated or changed TLS/SSH public fingerprints;
- negotiated obsolete TLS versions;
- SSH transport/auth/channel rejection reason without credential contents;
- migration conflict or required host-key rotation; and
- selected key-storage kind, hardware unavailability, file fallback during
  initial `auto` provisioning, and undeletable hardware orphans; and
- disabling a configured listener because its backend is unavailable.

Successful TLS handshakes log protocol version, cipher, and whether PSK was
used at debug level.  Successful SSH handshakes log remote version and
negotiated KEX, host-key, cipher, and MAC at debug level.  Passwords, PSKs,
hardware PINs, private-key material, sensitive provider diagnostics, and raw
public-key blobs are never logged.  Storage type, logical key name, non-secret
token label, and public fingerprint may be logged; PKCS#11 URIs are redacted
to exclude any provider-specific authentication query fields.

Borrowed object error text remains valid until that object's next operation
and is logged or copied immediately.  Static status text needs no release.
Thread-local constructor diagnostics are used only when no object was
returned.  No replacement has an equivalent of `free_crypt_attrstr()`.

## Build-system changes

### CMake

- Call `synchronet_configure_crypto()` from
  `src/build/SynchronetCrypto.cmake` before adding xptls or DeuceSSH.  Add the
  independent `src/xptls` static library and link `xptls::static` to every
  sbbs3 target that owns TLS or JavaScript crypto.
- In the transition release only, build `cryptmigrate` as a standalone target
  from isolated Cryptlib-export and xptls-import translation units.  Do not
  propagate its Cryptlib include directories, definitions, or libraries to
  any server, JavaScript, or common-library target.
- Add `src/ssh` with only the static target for integrated builds and link it,
  plus the existing SFTP library, to the terminal server library/executable.
- Select the provider with `CRYPTO_BACKEND`; the shared configuration pins
  `XP_CRYPTO_BACKEND` and `DEUCESSH_CRYPTO_BACKEND` to the same value for an
  integrated sbbs3 build.
- Reuse the vendored Botan target when selected; do not build a second Botan.
- Detect TPM 2.0, PKCS#11, and supported platform-keystore adapters
  independently and publish their capabilities through xptls.  A missing
  optional SDK/module must not disable the file backend.
- Remove `3RDP_CRYPTLIB_LIB`, Cryptlib include paths, version defines, and
  patch-hash checks from sbbs3 targets.

### GNU make

Use the shared `CRYPTO_BACKEND` selection in `src/build/Common.gmake` and the
CMake-subproject rules in `src/build/CMakeSubproject.gmake` to build the
independent xptls and DeuceSSH static libraries.  Add explicit library
prerequisites and provider libraries to final link order.  Replace the broad
`$(CRYPT_LIB)` dependencies in `extdeps.mk` with the actual xptls dependency
only on objects whose headers or code use xptls.

`sbbsdefs.mk` drops `-DUSE_CRYPTLIB`.  Optional code uses positive
`WITH_XPTLS` and `WITH_DEUCESSH` definitions generated by the build; it does
not infer SSH availability from TLS availability.

The transition makefile gives `cryptmigrate` a target-local Cryptlib
dependency.  No shared object list containing its exporter is reused by
`sbbs`, `jsexec`, SCFG, or a server library.

### Windows projects and UI

Update the Visual Studio projects for the selected xptls and DeuceSSH static
libraries and provider runtime.  Remove `cl32` DLL staging.  `AboutBox` and
`ver.cpp` report:

- `xptls (<provider> <provider-version>)`; and
- `DeuceSSH <version>` when enabled.

The configuration wizard stops creating or referring to `cryptlib.key` and
uses the new SSH host-key paths.  SCFG no longer calls `cryptInit`/`cryptEnd`.

## Implementation sequence

Each milestone builds and has its own tests.  Development builds may retain
old server consumers while their milestone is in progress, but converted code
must not call back through a compatibility shim.  In the published transition
release, only the standalone migration utility still links Cryptlib.

1. **Complete the remaining xptls surface.** Implement storage-backed TLS
   client identities, the certificate operations required by `CryptCert`,
   `xp_keyset`, claimed TPM/platform store adapters, and any advertised
   hardware import operations.  Add Botan, OpenSSL, and disabled-backend tests
   for each contract before its sbbs3 consumer depends on it.
2. **Replace TLS identity persistence.** Implement standard files, the new
   key-reference registry, the new `ssl.c` manager, storage-policy-aware
   self-signed creation, reload tests, and the isolated native transition
   utility while existing endpoints still use Cryptlib in development builds.
3. **Port TLS endpoints.** Convert common I/O types, then FTP, mail, web,
   services, MQTT, and JavaScript `Socket`.  Run protocol interop after each
   server rather than converting every endpoint before the first handshake
   test.  Delete that endpoint's `GCES*`/`HANDLE_CRYPT_CALL` macros as it is
   converted; do not leave a renamed compatibility wrapper.
4. **Integrate DeuceSSH.** Add host-key startup, the adapter, auth callbacks,
   opaque host signers, channel worker, shell I/O, SFTP I/O, and lifecycle
   tests.  Keep the Cryptlib SSH path behind a temporary build-only comparison
   option during this milestone; never enable two SSH listeners on the same
   port.
5. **Port JavaScript crypto and tools.** Implement provider-backed classes,
   new keysets, update ACME/certificate scripts, and expand JS tests.  Ship
   these xptls-only objects in the transition release alongside, but entirely
   separate from, the native migration utility.
6. **Remove Cryptlib from sbbs3 in the release after the transition.** Delete
   the native migration utility and old SSH/TLS/JS code paths and error helpers,
   remove includes/types/defines/link inputs and UI version text, and make the
   no-reference scan an acceptance gate.  The migration report and standard
   file import remain because neither needs Cryptlib.

The native migration utility must be released before milestone 6 reaches
ordinary operators.  The milestones may be adjacent development commits, but
the transition artifact must be published and supported; the final binary is
never asked to parse a library format it no longer supports.

## Testing

### Unit and library tests

- Run the full xptls suite with Botan, OpenSSL, and disabled backends.
- Run the full DeuceSSH suite with Botan and OpenSSL, including allocation,
  transport-error, rekey, auth, concurrent-channel, and cleanup tests.
- Add sbbs TLS identity-manager tests for first creation, atomicity, bad
  password, mismatched key, partial ACME replacement, successful hot reload,
  failed reload retaining the old identity, and old-session survival.
- Add storage-policy tests for explicit file, `auto` with and without usable
  hardware, explicit hardware failure, per-purpose override, algorithm
  capability mismatch, bad PIN, missing token after binding, and restart with
  a stable locator.  Assert that no failure path falls back or changes a
  fingerprint after a reference has been committed.
- Run hardware-backed signing/CSR/TLS/SSH tests with a software TPM and
  SoftHSM-compatible PKCS#11 fixture in CI where supported.  At least one
  periodic platform job uses real hardware when project infrastructure makes
  that possible; its absence does not weaken the emulator-backed acceptance
  gate.
- Add SSH adapter tests with mocked user lookup and socket I/O for password,
  public-key probe/signature, unknown-user new-user path, three failures,
  ANYAUTH, channel policy, dimensions, simultaneous shell/SFTP, and every
  cleanup interleaving.
- Authorized-key fixtures compare the complete DeuceSSH callback blob,
  including its algorithm length field, and prove the old Cryptlib `+4`
  offset is gone.
- Add keyset fixtures and round trips across providers: Botan writes/OpenSSL
  reads and the reverse for encrypted PEM, PKCS#12, certificate chain, CSR,
  signatures, and Synchronet manifests.
- Verify non-exportable key behavior: public export and signing succeed,
  private export/PKCS#12 fail explicitly, certificate matching works, system
  password changes leave the object untouched, and logs contain no PIN or
  private material.
- Test the native migration utility against fixtures for every known legacy
  label, wrong passwords, unsupported private-key export, dry run, idempotent
  apply, conflicts, and interrupted transactions.  Inspect its target link
  graph to prove Cryptlib does not propagate to another executable or shared
  library.
- Add adapter error tests in which a counted stub operation proves each call
  is evaluated once.  Expected timeout/would-block/EOF paths produce no
  generic error, while unexpected xptls/DeuceSSH failures include stable
  status text plus operation and connection context without allocating an
  attribute string.

### JavaScript tests

Expand `exec/tests/crypt` to assert property names and numeric constants as
well as operations.  Required tests cover:

- SHA-256 streaming and binary data;
- RSA and ECDSA generation, JWK components, sign/verify, and PEM round trip;
- CSR SAN generation and DER import/export;
- certificate-chain and peer-certificate construction;
- keyset labels, wrong password, delete, read-only enforcement, atomic
  replacement, and PKCS#12 import/export;
- a legacy Cryptlib keyset passed to the xptls `CryptKeyset`, which must return
  the migration-directed error without loading or probing Cryptlib;
- `Socket` certificate client/server, PSK client/server, client certificate,
  verification flags, timeout, pending plaintext, and close; and
- complete `letsyncrypt.js --staging` flow against an ACME test service or a
  recorded local protocol fixture, ending with a matching active TLS key
  reference and `ssl.crt`; file-policy coverage also verifies `ssl.key`.

### Protocol interoperability

On both providers, test:

- HTTPS with curl and OpenSSL's client;
- POP3S and STLS;
- SMTPS and STARTTLS, plus an outbound opportunistic STARTTLS success and
  failure that does not incorrectly bounce mail;
- FTP explicit TLS on control and passive data connections;
- MQTT certificate and PSK modes with selected identity propagation;
- a TLS JavaScript service and both JavaScript `Socket` roles;
- OpenSSH password and public-key shell login, wrong credentials, terminal
  resize, disconnect, and reconnect;
- OpenSSH SFTP file/list/stat operations and concurrent shell plus SFTP; and
- current SyncTERM SSH/SFTP against the new server.

At least one interop client must negotiate the RSA host key and one Ed25519.
At least one TLS test must exercise TLS 1.2 and one TLS 1.3.

### Build matrix

The gate covers GNU make and CMake on the supported Linux, FreeBSD, macOS,
MinGW, and Visual Studio configurations, with:

- Botan;
- OpenSSL where supported;
- xptls disabled and SSH disabled; and
- xptls disabled with DeuceSSH enabled.

Sanitizer builds run the handshake-failure, wrong-password, concurrent close,
and migration tests because those paths carry the highest ownership risk.
Hardware-store jobs are capability-gated rather than OS-name-gated and cover
each provider/store combination that the build claims to support.

At least one Botan job uses 3.13.0, the oldest version the design supports,
and another uses the newest released version.  A supported platform whose
packages lag uses the vendored 3.13.0 provider rather than retaining a pre-3.13
compatibility job.

## Expected files touched

| Area | Principal files |
|---|---|
| xptls API/backends/tests | `xp_tls.h` and both TLS backends for storage-backed client identity; `xp_ca.*` compatibility additions; new `xp_keyset.*` and claimed TPM/platform adapters; common tests and CMake wiring |
| TLS identity/common errors | `src/sbbs3/ssl.{h,c}` and a small common TLS I/O helper |
| TLS consumers | `ftpsrvr.cpp`, `mailsrvr.cpp`, `websrvr.cpp`, `services.cpp`, `mqtt_{client,broker}.{h,cpp}`, `js_socket.{h,cpp}`, and structs that carry TLS handles |
| SSH/SFTP | new `ssh_session.{h,cpp}`, `main.cpp`, `answer.cpp`, `sbbs.h`, `sftp.cpp` |
| JavaScript crypto | `js_cryptcon.*`, `js_cryptcert.*`, `js_cryptkeyset.cpp`, new constants/header and tests |
| Scripts and persistence | `exec/letsyncrypt.js`, `exec/certtool.js`, `exec/load/acmev2.js`, `ctrl/keyrefs.ini` handling, native transition-only `cryptmigrate`, `scfgsys.c` |
| Build/UI | GNU make dependency/link files, CMake, Visual Studio projects, SCFG projects, controller UI, `ver.cpp`, test-build staging |

This is a guide, not permission to make unrelated cleanup changes in those
large server files.

## Acceptance criteria

The replacement is complete only when all of the following are true:

- `git grep` over tracked `src/sbbs3` code and build files finds no
  `cryptlib.h`, `CRYPT_`, `cryptInit`, `cryptEnd`, `cryptCreate*`,
  `cryptSet*`, `cryptGet*`, `cryptPushData`, `cryptPopData`,
  `cryptFlushData`, or `cryptDestroy*` reference.
- `git grep` also finds no `GCES`, `GCESH`, `GCESHL`, `GCESNN`, `GCESS`,
  `GCESSTR`, `HANDLE_CRYPT_CALL`, `get_crypt_error_string`,
  `handle_crypt_call`, `do_cryptAttribute`, `free_crypt_attrstr`,
  `do_cryptInit`, `add_private_key`, `destroy_session`, `set_authresponse`,
  `CRYPT_ENVELOPE_RESOURCE`, or Cryptlib-shaped SSH channel selection in
  tracked sbbs3 code.
- `sbbs.h` contains no generic crypto integer handle, SSH channel ID,
  `ssh_mutex`, or `ssh_active` event.  The adapter exposes typed session and
  channel operations instead.
- No sbbs3 link line or staged runtime artifact contains `cl`, `cl32`, or a
  Cryptlib DLL/library dependency.
- In the transition release, `cryptmigrate` is the sole target with a
  Cryptlib dependency; no JavaScript object can instantiate or select a
  Cryptlib-backed implementation.  In the final release the migration target
  and Cryptlib dependency are absent.
- Every configured TLS endpoint either starts with a validated shared
  identity or remains closed with a clear error; none serves plaintext on a
  TLS port.
- Converted TLS and SSH/SFTP paths contain no timeout save/set/restore dance.
  Per-operation RX and TX deadlines remain independent under concurrent I/O,
  and xptls leaves the caller-owned socket's `SO_RCVTIMEO` and `SO_SNDTIMEO`
  unchanged.
- Certificate reload is atomic, a failed reload retains the previous valid
  identity, and established sessions survive a successful reload.
- SSH password, authorized-key, ANYAUTH, interactive new-user shell, sysop
  secondary check, SFTP, terminal dimensions, and simultaneous shell/SFTP
  behavior pass adapter and interop tests.
- New private keys use supported hardware by default under `auto`; explicit
  hardware policy fails closed, and explicit file policy works on systems
  without hardware.  Once bound, no storage failure silently changes backend
  or public fingerprint.
- File-backed private keys are encrypted standard files with restrictive
  permissions, and changing the system password re-encrypts all file-backed
  keys transactionally without exporting or rewriting hardware keys.
- TLS, SSH, CSR, ACME, and JavaScript signing operate with a non-exportable
  hardware key; requests to export that private key fail explicitly while its
  public key remains available.
- A transition install preserves TLS and SSH public fingerprints when its
  Cryptlib version permits export, and reports rather than hides a required
  rotation when it does not.
- `CryptContext`, `CryptCert`, `CryptKeyset`, `letsyncrypt.js`,
  `certtool.js`, ACME signing, and JavaScript `Socket` TLS pass on both Botan
  and OpenSSL.
- Provider-unavailable and unsupported-algorithm cases fail explicitly and do
  not downgrade algorithms, certificate checks, SSH channel policy, or wire
  security.
