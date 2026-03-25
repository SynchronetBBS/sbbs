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

**DONE (KEX)**: `struct dssh_kex_context` (defined in `deucessh-kex.h`)
provides all inputs, outputs, and I/O function pointers.  KEX handlers
take `struct dssh_kex_context *kctx` instead of `dssh_session`.
`dssh_transport_kex()` builds the context, calls the handler, and
copies outputs back.  KEX modules no longer include `ssh-internal.h`
in production builds.

Key algorithm modules still access `dssh_transport_find_key_algo()`
(an internal function) and store key material on the global
registration entry's `ctx` field.  Remaining ideas:

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
