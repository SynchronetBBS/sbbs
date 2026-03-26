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

## 2. Reduce internal API exposure for algorithm modules — DONE

All algorithm modules (kex, key_algo, enc, mac, comp) now use only
public headers.  No module includes `ssh-trans.h` or `ssh-internal.h`
in production builds.

- **KEX**: `struct dssh_kex_context` provides inputs, outputs, and
  I/O function pointers.  Handlers take `dssh_kex_context *` not
  `dssh_session`.  Defined in `deucessh-kex.h`.
- **Key algo**: `dssh_key_algo_set_ctx()` replaces direct struct
  access.  Modules keep a file-scope static for their cbdata.
  Struct/typedefs in `deucessh-key-algo.h`.
- **Enc/Mac/Comp**: Struct/typedefs and registration in
  `deucessh-enc.h`, `deucessh-mac.h`, `deucessh-comp.h`.
- **Per-session keys**: Considered and rejected — SSH host keys
  identify the server, not the listen address.
