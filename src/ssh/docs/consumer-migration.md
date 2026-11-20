# Consumer migration from the previous push

This note lists consumer-facing changes since commit `e7487c6`, the last pushed
revision before the v1.0 audit changes. Rebuild consumers against the new headers
and library: public structures and callback signatures have changed, and the
shared-library ABI major is now 1. Install `deucessh-result.h` and
`deucessh-abi.h` along with the other public headers.

## Application source changes

| Interface | Update required when used |
| --- | --- |
| Client signing keys | Load each connection's key with `dssh_session_load_client_key_file(sess, algo_name, path, pw_cb, cbdata)`; generate one with `dssh_session_generate_client_key()`. `dssh_auth_publickey()` keeps its signature and selects the local key first, then the global key when no local key exists. Global private keys are optional for clients. Replacement is allowed between unsuccessful attempts; clearing restores global fallback. No implicit retry changes credentials. |
| Client authentication discovery | Use optional `dssh_auth_begin(sess, username)` to attempt authentication without credentials and discover continuation methods. It may complete authentication. Credential methods can instead be called directly, without an implicit discovery attempt. |
| Authentication outcomes | Handle `DSSH_AUTH_SUCCESS`, `DSSH_AUTH_PARTIAL` and `DSSH_AUTH_CONTINUE`. Only SUCCESS completes authentication. The old `DSSH_AUTH_NONE_ACCEPTED`, `DSSH_AUTH_METHODS_AVAILABLE` and `DSSH_ERROR_AUTH_REJECTED` constants are removed. Negative results report operation errors. |
| Authentication methods getter | The signature is now `int64_t dssh_auth_get_methods(dssh_session sess, char *methods, size_t methods_sz)`. It performs no I/O and returns the required size including NUL. Each completed authentication response updates the snapshot; a local/malformed/transport error preserves it. Before any completed response, the getter returns `DSSH_ERROR_INIT`. |
| Authentication callback outputs | Update client keyboard-interactive, client password-change, server password/password-change and server keyboard-interactive callbacks to return `dssh_auth_responses`, `dssh_bytes_result` or `dssh_auth_challenge`, as appropriate. These use the producer-owned result contract below. |
| Server authentication configuration | Account for the new `request_cb` member of `dssh_auth_server_cbs`; designated initializers avoid positional field shifts. NULL binds authentication to the first complete username. Supporting identity changes requires a callback that clears accumulated factors when `new_identity` is true and resets dialogue state for each new attempt. The hook runs before method-specific handling; returning zero permits processing and does not authenticate. |
| Server method advertisement and interactive rounds | Do not list `none` in `methods_str`. A missing `none_cb` automatically rejects that probe. Use `request_cb` to distinguish a new keyboard-interactive attempt from a later round with zero responses; `num_responses == 0` alone is insufficient. |
| Channel event text | Signal names and exit-signal names/messages are now `dssh_bytes` views with `.data` and `.len`. Exit-signal events also expose language. Do not use C-string operations on these views. Events are consumed by the event callback or queued for polling, never both. Event constants were renumbered. |
| BREAK | Replace `DSSH_EVENT_BREAK` handling with `dssh_session_set_break_cb()` or `dssh_chan_set_break_cb()`. The synchronous handler returns true only when it performed a BREAK action. Waiting in it pauses receive progress for the session. |
| Negotiated algorithms | Add a `dssh_direction` argument to `dssh_transport_get_enc_name()` and `dssh_transport_get_mac_name()`. The values are `DSSH_CLIENT_TO_SERVER` and `DSSH_SERVER_TO_CLIENT`, independent of the local role. `dssh_transport_get_comp_name()` provides the corresponding compression query. |
| Language preferences | Replace global `dssh_transport_register_lang()` registrations with per-session `dssh_session_set_languages()`. Read local and peer fields with `dssh_session_get_languages()` and `dssh_session_get_peer_languages()`, separately for each wire direction. These getters return required sizes including NUL. Applications interpret the preferences; there is no selected-language result. |

Exact declarations and callback restrictions are in
[deucessh-auth.h](../deucessh-auth.h),
[deucessh-conn.h](../deucessh-conn.h),
[deucessh.h](../deucessh.h) and
[deucessh-lang.h](../deucessh-lang.h).

## Result ownership

Callback output storage remains owned by its producer. Return byte views and a
`dssh_result_lease` instead of allocating memory for the library to free.
Whenever returning a nonnull data/array pointer, supply a release callback,
including a no-op release for static storage. One lease covers a returned bundle.

Keep results valid and immutable until release. The library releases a present
lease after its last read on success, error or rejected output. The producer
clears its actual secret capacity before releasing or reusing storage. Reuse
allocations where possible, while keeping independently live results valid.
Callback inputs cannot back outputs when their borrowed lifetime ends on return.
Release callbacks perform no I/O and cannot enter session/channel, lifecycle or
configuration APIs. See [deucessh-result.h](../deucessh-result.h).

Event byte views are borrowed, without a release lease. Poll/read-event views
last until the next poll/read-event call on that channel; callback views last
only for that invocation. Copy bytes needed beyond those intervals.

## Behavioral contracts to check

- Complete global registration, callback/version/context setup and builtin key
  replacement before the first session-init attempt. Mutable setup is
  application-serialized. An accepted init attempt freezes it before allocation,
  including when initialization later fails.
- An ordinary `dssh_chan_close()` consumes the handle on every return; do not
  retry it. Zero reports local retirement with best-effort sends. On a server,
  an exit code outside `[0, UINT32_MAX]` omits the status and still closes;
  clients do not send exit status. Finish/cancel a TX reservation before closing.
  Calling close in a server setup callback records cancellation, which the
  acceptor completes after that callback returns.
- Readiness is a snapshot: handle `DSSH_ERROR_NOMORE` even after poll. A read
  returns bytes already delivered before reporting a later receive-credit error
  on a subsequent positive-capacity read. Peeks do not consume retained errors;
  requested read directions poll ready while an error awaits reporting.
- Stop/join application users before session cleanup and serialize lifecycle
  calls. Cleanup inside a callback on that session or while holding a TX
  reservation requests termination and defers destruction; the owner must call
  cleanup again after that context ends. I/O callbacks must unblock promptly
  after termination.
- Custom identification software tokens must exclude `-`; use a token such as
  `MySSH_1.0`. Peer software tokens containing `-` are also rejected.
- Supply a PEM password callback for encrypted key files. A NULL callback never
  invokes a provider prompt. Password callback lengths describe bytes, allow
  embedded NULs and need no terminating NUL.
- Private-key saves overwrite the validated file in place. Targets must be
  regular, singly linked and owned by the effective caller; final symbolic links
  and reparse points are rejected. Existing identity is retained and permissions
  are made private. A failed save may leave empty or partial contents; the
  application coordinates the namespace and recovery copies.
- Environment requests use `want_reply=false`. Channel setup therefore does not
  report whether the peer accepted each requested variable.

## Custom algorithm modules

- **KEX:** implement `init`, separate borrowed process-lifetime `config` from
  per-session `state`, and return a leased `dssh_kex_result` through the handler's
  output argument. Init/reset should reuse state on rekey while generating fresh
  ephemeral material. Cleanup releases session state, not global configuration.
  All three callbacks are required. See [deucessh-kex.h](../deucessh-kex.h).
- **Signing:** change the signer output to `dssh_bytes_result` with a producer
  release lease. See [deucessh-key-algo.h](../deucessh-key-algo.h).
- **Custom DH-GEX groups:** use `dssh_dh_gex_provider` and return unsigned
  big-endian group values in a leased `dssh_dh_gex_group_result`.
- **Compression:** implement directional init/reset, an expansion-bound callback
  and transforms with separate input/output spans and explicit output capacity.
  All five callbacks are required. See [deucessh-comp.h](../deucessh-comp.h).

Registration definitions, names and callback code remain borrowed for process
lifetime. Cipher and MAC callback signatures have not changed. Custom
signing modules can install a session context with
`dssh_session_set_client_key_ctx(sess, algo_name, ctx, &lease)`. Success moves and
zeros the lease; failure preserves it. The producer releases its own storage
when the session replaces, clears or cleans up the key. Global descriptor/key
contexts remain the fallback and server host keys.

## Build, optional algorithms and examples

Builds that list library sources manually must add `key_algo/client-key.c`.

All 138 public functions now use interface version 1.0. Rebuild and explicitly
link consumers with the matching headers and library. Ordinary C function names
stay the same. ELF references record `DEUCESSH_1.0`; macOS/Windows external names
end in `_v1_0`, including static definitions on those platforms. Code using
`dlsym`/`GetProcAddress` must request that qualified name on macOS/Windows
(without the Mach-O object-file leading underscore). ELF current-default
`dlsym` uses the ordinary name; version-aware lookup selects the required node.
See the [ABI policy](audit-dsohowto.md#symbol-versioning-plan).

On MSVC, manually linked static consumers now select `deucessh-static.lib`;
`deucessh.lib` selects DLL imports. Installed CMake target names are unchanged,
and the shared target supplies `DSSH_SHARED_IMPORT` for Windows consumers.

Botan-backed binaries require an ABI-compatible Botan library, with no exact
release-version pin in the installed CMake or pkg-config metadata. Compatible
updates do not require a rebuild just because the release version differs.
Rebuild DeuceSSH when Botan's ABI changes; the source/API minimum remains
Botan 3.13.0. Existing dynamically linked binaries retain their normal shared
library dependencies. Botan's [ABI policy](https://botan.randombit.net/handbook/abi.html)
describes SONAME revisions for ABI changes. Static-library consumers must also
link against an ABI-compatible Botan or rebuild DeuceSSH for the new ABI.

The additional optional KEX registrations are
`dssh_register_dh_group14_sha256()` and
`dssh_register_dh_group16_sha512()`. The examples register both. The zlib module
is optional example code, not an official compression backend.

The [client example](../examples/client.c) now takes:

```text
client hostkey-sha256 username password-fd [host] [port] [command]
```

Its password comes from the dedicated descriptor, leaving stdin for channel
input, and it requires the supplied host fingerprint to match on every exchange.
