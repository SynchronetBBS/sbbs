# DeuceSSH Design Rules

## Input Validation

Every parameter and return value received via public APIs (including the rx callback) MUST be fully
validated before use. Invalid values must result in an error propagating back to the application.

Library code must not make calls with potentially invalid values. If a value is not known to be valid,
it must be checked BEFORE passing it to another function.

When the library needs functionality provided by a public function, split the public function into:
- A **public wrapper** that validates parameters which can never be invalid in internal calls
  (e.g., NULL session pointers, enum range checks).
- An **internal function** that validates parameters which could still be invalid from internal
  call sites (e.g., values derived from remote input or computation).

Internal code calls the inner function directly, skipping the outer validation.

## Portability

Code must use only C17 standard functions and must not rely on any platform-specific APIs.

Code must be correct on any conforming platform, from 16-bit to 64-bit. `size_t`, `int`, and enum
widths vary across platforms; code must handle all valid widths.

Code must be correct regardless of byte order (big-endian, little-endian, PDP-endian, or any other
object representation). Do not assume any particular byte ordering or object layout.

## Arithmetic Safety

All arithmetic must be guarded against underflow and overflow. Only IMPOSSIBLE over/underflow may be
unchecked; UNLIKELY over/underflow MUST be checked.

## Error Handling

If a function's API defines any possible error indication, that error MUST be handled correctly, even
if it appears impossible to trigger on the current platform. Different platforms produce different errors.

## Pointer Ownership

Allocation ownership must not cross the library boundary. The library must not free anything the
application provides, and the application must not free anything the library provides.

## Memory Allocation

Allocations triggered by remote packets must enforce explicit size limits to prevent unbounded memory
usage from a malicious peer.

Allocations should be avoided where possible, but not at the expense of using fixed-size buffers when
the size could grow in the future. If the relevant specification guarantees a maximum (e.g., RFC 4251
limits algorithm names to 64 bytes), a fixed-size buffer is acceptable; otherwise, allocate dynamically.

## Memory Copies

Prefer in-place validation and processing over copying into temporary buffers. Minimize the number of
memory copies; do more work up front to avoid needing intermediate buffers.

## No Library I/O

The library must never perform I/O directly (no read/write/send/recv or equivalent). All I/O goes
through application-provided callbacks. This ensures the library is transport-agnostic and the
application retains full control over its I/O model.

## Opaque Public API

Public headers must expose only opaque handles and standard C types. Internal dependencies (OpenSSL,
C11 threads, internal structs) must never appear in public headers. Consumers of the library need only
a C17 compiler and the public headers.

## Type Safety

Narrowing casts must be placed in initializers (`type x = (type)val;`), never inline in function
arguments or return statements. Both bounds must be range-checked before every narrowing cast. The
build enforces `-Wconversion -Werror`; all implicit narrowing is a compile error.

## Thread Safety

All mutable state shared between the demux thread and API callers must be protected by locks or
atomics. No unprotected shared mutable state.

## No Deprecated Cryptography

SHA-1-based algorithms (hmac-sha1, dh-group1-sha1, dh-group14-sha1, ssh-dss, 3des-cbc) are
intentionally excluded. This is a security policy, not a gap — these algorithms are deprecated and
will not be implemented.
