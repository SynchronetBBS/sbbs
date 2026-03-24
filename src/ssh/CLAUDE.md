# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working on DeuceSSH.

## Project Overview

**DeuceSSH** is a from-scratch SSH library in standard C17, implementing
RFCs 4250-4254, 4419, 8332, and 8731.  No proprietary extensions (no `@`
algorithm names).  Uses OpenSSL 3.0+ libcrypto and C11 threads internally;
public headers expose neither.

## Build

CMake from this directory.  Use `-S . -B build` to prevent cmake from
discovering the parent `src/CMakeLists.txt` (which has a broken
`project()` that causes install-export errors):
```sh
cmake -S . -B build
cmake --build build -j8
```

Produces `libdeucessh.a` (static) and `libdeucessh.so` (shared).

## Testing

~700 tests across 11 executables, 26 CTest runs (~120s sequential,
~40s with `-j8`):
```sh
cmake -S . -B build -DDEUCESSH_BUILD_TESTS=ON
cmake --build build -j8
cd build
ctest -j8                    # run all in parallel
ctest -R dssh_unit           # unit tests only
ctest -R dssh_layer          # layer tests only
ctest -R dssh_integration    # integration only
./dssh_test_arch test_parse  # filter by name
```

Test infrastructure:
- `test/dssh_test.h` — test framework (assert macros, PASS/FAIL/SKIP, runner)
- `test/mock_io.h/.c` — bidirectional mock I/O via socketpair()
- `test/mock_alloc.h/.c` — process-wide alloc injection (--wrap=malloc)
- `test/dssh_test_alloc.h/.c` — library-only alloc injection (macro-based,
  doesn't affect OpenSSL)
- `test/test_enc.h/.c` — XOR cipher with failure injection
- `test/test_mac.h/.c` — XOR-fold MAC with failure injection
- `test/dssh_test_internal.h` — declarations for `DSSH_TESTABLE` functions
- `-DDSSH_TESTING` compile flag exposes static functions, redirects
  library malloc/calloc/realloc to test allocator, and compiles out
  unreachable defense-in-depth guards for accurate coverage

Coverage measurement:
```sh
cmake -S . -B build-cov -DDEUCESSH_BUILD_TESTS=ON \
  -DCMAKE_C_FLAGS="-fprofile-instr-generate -fcoverage-mapping" \
  -DCMAKE_EXE_LINKER_FLAGS="-fprofile-instr-generate"
cmake --build build-cov -j8
cd build-cov
LLVM_PROFILE_FILE="dssh-%p.profraw" ctest
llvm-profdata merge -sparse dssh-*.profraw -o dssh.profdata
llvm-cov report ./dssh_test_selftest -instr-profile=dssh.profdata \
  -object=./dssh_test_transport -object=./dssh_test_auth \
  -object=./dssh_test_conn -object=./dssh_test_alloc \
  -object=./dssh_test_transport_errors -object=./dssh_test_arch \
  -object=./dssh_test_chan -object=./dssh_test_algo_enc \
  -object=./dssh_test_algo_mac -object=./dssh_test_algo_key \
  ../ssh-trans.c ../ssh-auth.c ../ssh-conn.c ../ssh.c ../ssh-arch.c \
  ../ssh-chan.c ../kex/dh-gex-sha256.c ../kex/curve25519-sha256.c \
  ../key_algo/ssh-ed25519.c ../key_algo/rsa-sha2-256.c \
  ../enc/aes256-ctr.c ../mac/hmac-sha2-256.c
```

## Architecture

- **Public API**: ~65 `dssh_*` functions across 4 consumer headers
  (`deucessh.h`, `deucessh-auth.h`, `deucessh-conn.h`, `deucessh-algorithms.h`)
- **Opaque handles**: `dssh_session` and `dssh_channel` hide internal structs
- **I/O callbacks**: library does no I/O — application provides tx/rx/rxline
- **Demux thread**: single thread per session reads packets, dispatches to channels
- **Two channel types**: session (stream-based bytebufs for shell/exec) and
  raw (message-based msgqueue for subsystems)
- **Channel type deferred**: determined by the terminal request (shell/exec vs
  subsystem), not at channel open time

Key source files:
- `ssh-trans.c` — transport layer (packets, KEX, keys, rekey)
- `ssh-auth.c` — authentication (client + server)
- `ssh-conn.c` — connection protocol (channels, demux, I/O)
- `ssh-chan.c` — buffer primitives (bytebuf, msgqueue, signal queue)
- `ssh.c` — session lifecycle, `dssh_session_set_terminate()`

See `README.md` for the full API reference with code examples.

## RFC Conformance

Audit files (`audit-4250.md` through `audit-4254.md`) document every
MUST/SHOULD/MAY requirement and the library's conformance status.
Tests cover all CONFORMS items; APPLICATION items are the app's responsibility
but the API enables conformance.

## Code Style

- Tabs for indentation
- Each statement on its own line (no single-line `if (x) break;`)
- When fixing bugs, discuss each individual bug with the user before making changes
- `DSSH_PUBLIC` for exported symbols, `DSSH_PRIVATE` for library-internal
  (visibility annotations, no-op in static builds)
- `DSSH_TESTABLE` for static functions exposed under `-DDSSH_TESTING`
- Error codes: 0 = success, negative `DSSH_ERROR_*` constants
- `DSSH_ERROR_TERMINATED` returned when session is dead (fatal transport
  error); distinguishes from recoverable auth rejection

## Key Design Decisions

- **No deprecated algorithms**: 3des-cbc, hmac-sha1, dh-group1/14-sha1,
  ssh-dss intentionally not implemented (SHA-1 deprecated)
- **Global I/O callbacks**: set once via `dssh_transport_set_callbacks()`,
  per-session differentiation via cbdata pointers
- **Global algorithm registry**: locked after first `dssh_session_init()` call;
  registration order determines negotiation preference
- **Channel request callback**: single `dssh_channel_request_cb` fires for
  every `CHANNEL_REQUEST` during setup; app returns accept/reject
- **Deferred auto-rekey**: `rekey_pending` flag set in recv_packet, fires
  at top of next recv_packet call (avoids overwriting live payload pointer)
- **Terminate signaling**: `dssh_session_set_terminate()` sets flag AND
  broadcasts all library-owned condvars (rekey_cnd, accept_cnd, poll_cnd).
  Fatal errors in send_packet/recv_packet auto-terminate.  Auth functions
  promote errors to `DSSH_ERROR_TERMINATED` when the session is dead.
- **Test key caching**: `DSSH_TEST_ED25519_KEY` / `DSSH_TEST_RSA_KEY` env
  vars cache host keys to files, avoiding repeated RSA keygen (~200ms each).
  CMakeLists.txt sets these for all CTest runs.
