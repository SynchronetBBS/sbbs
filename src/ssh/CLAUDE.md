# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working on DeuceSSH.

## Project Overview

**DeuceSSH** is a from-scratch SSH library in standard C17, implementing
RFCs 4250-4254, 4419, 8332, and 8731.  No proprietary extensions (no `@`
algorithm names).  Uses OpenSSL 3.0+ libcrypto and C11 threads internally;
public headers expose neither.

## Build

CMake from this directory:
```sh
mkdir build && cd build
cmake ..
cmake --build . -j8
```

Produces `libdeucessh.a` (static) and `libdeucessh.so` (shared).

## Testing

372 tests across 9 executables:
```sh
cmake .. -DDEUCESSH_BUILD_TESTS=ON
cmake --build . -j8
ctest                        # run all
ctest -R dssh_unit           # unit tests only
ctest -R dssh_layer          # layer tests only
ctest -R dssh_integration    # integration only
./dssh_test_arch test_parse  # filter by name
```

Test infrastructure:
- `test/dssh_test.h` — test framework (assert macros, PASS/FAIL/SKIP, runner)
- `test/mock_io.h/.c` — bidirectional mock I/O with C11 condvar synchronization
- `test/dssh_test_internal.h` — declarations for `DSSH_TESTABLE` static functions
- `-DDSSH_TESTING` compile flag exposes static functions and `dssh_test_reset_global_config()`

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
- `ssh.c` — session lifecycle

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
- Error codes: 0 = success, negative `DSSH_ERROR_*` constants

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
