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
