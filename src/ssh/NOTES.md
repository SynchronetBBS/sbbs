# DeuceSSH — API Design Notes

## 1. Return SSH DEBUG messages for library diagnostic information

Use SSH_MSG_DEBUG (RFC 4253 s11.3) as a channel for library-internal
diagnostic information back to the application.  When the library
encounters a non-fatal internal condition (e.g. allocation failure in
a non-critical path, unexpected but recoverable protocol state), it
could synthesize a DEBUG message and deliver it through the existing
`dssh_debug_cb` callback.

This would let applications log library-internal events without a
separate logging API, reusing the SSH debug mechanism that already
exists in the protocol and the library's callback infrastructure.

Open questions:
- Use `always_display = false` for library diagnostics vs `true` for
  peer-originated debug messages?
- Define a naming convention for library-originated messages (e.g.
  prefix with "dssh: ") to distinguish from peer debug messages?
- Or use a separate callback / flag to avoid conflation?

## 2. Reduce internal API exposure for KEX and key_algo modules

KEX handlers currently reach directly into `sess->trans.*` fields to
read version strings, KEXINIT payloads, and the negotiated host key
algorithm, and to write the shared secret and exchange hash.  This
couples module code tightly to the session struct layout.

Key algorithm modules access `dssh_transport_find_key_algo()` (an
internal function) and store key material on the global registration
entry's `ctx` field.

Ideas to explore:

- **KEX context struct**: Instead of raw `sess->trans.*` access, pass
  a `struct dssh_kex_params` to the handler containing the fields it
  needs (version strings, KEXINIT payloads, host key algo) and output
  fields (shared_secret, exchange_hash).  The handler fills the outputs
  and the library copies them into the session.  This decouples KEX
  modules from `ssh-internal.h` entirely.

- **Helper functions**: Replace direct field access with accessor
  functions like `dssh_kex_get_peer_kexinit(sess, &buf, &len)` and
  `dssh_kex_set_shared_secret(sess, data, len)`.  Less disruptive
  than a context struct but still adds indirection.

- **Key algo lookup**: `dssh_transport_find_key_algo()` is only used
  by key_algo modules to find their own registration entry (for
  storing key material on `ctx`).  A dedicated
  `dssh_key_algo_get_ctx(name)` / `dssh_key_algo_set_ctx(name, ctx)`
  pair would avoid exposing the full `dssh_key_algo` pointer.

- **Separate key storage**: Move key material out of the global
  algorithm registry entirely.  Store keys on the session or in a
  separate key store.  This would allow per-session keys (e.g.,
  different host keys for different listen addresses) without
  changing the algorithm registry.

The current approach works and is efficient (no copies, no
indirection), but it means KEX modules must include `ssh-internal.h`
and are fragile against struct layout changes.  Worth revisiting if
third-party algorithm modules become a goal.
