# DeuceSSH — TODO / Known Issues
<!-- Next item: 179 -->

## Open

84. **DH-GEX group size vs cipher strength mismatch.**
    The client's GEX_REQUEST parameters (`GEX_MIN 2048`, `GEX_N 4096`,
    `GEX_MAX 8192`) are compile-time constants that don't account for the
    negotiated cipher.  A 2048-bit group provides ~112 bits of security,
    which is a poor match for AES-256 (256-bit).  OpenSSH has the same
    mismatch, but we should investigate whether to:
    - Bump `GEX_MIN` to 3072 (NIST 128-bit floor)
    - Tie `GEX_N` (preferred) to the cipher's key size
    - Consider that cipher negotiation happens in the same KEXINIT as KEX
      negotiation, so the client doesn't know the cipher result before
      sending GEX_REQUEST — the server could enforce a floor, but the
      client can only guess
    - Decide whether the built-in `default_select_group()` should enforce
      a minimum based on cipher strength, and if so, how it learns the
      cipher (it currently only sees min/preferred/max from the client)
    This needs interactive discussion — there are compatibility and
    protocol-ordering trade-offs that don't have obvious right answers.
    **Not suitable for automatic planning.**

122. **Full RFC conformance re-audit.**
     The existing audit files (`docs/audit-4250.md` through
     `docs/audit-4254.md`) were written early in development and only
     cover RFCs 4250–4254.  Re-audit all code against every
     implemented RFC/draft (4256, 4335, 4419, 6668, 8160, 8308, 8332,
     8709, 8731, draft-ietf-sshm-ntruprime-ssh,
     draft-ietf-sshm-mlkem-hybrid-kex), create new audit files where
     missing, update existing audits to be backend-agnostic (current
     ones reference only `*-openssl.c` paths), and verify all
     MUST/SHOULD/MAY requirements against the current implementation.

## Closed

Items are listed newest-first.  Detailed write-ups are in git history.

- 178: `libcrux_mlkem768_sha3.h` `#pragma once` replaced with standard `#ifndef LIBCRUX_MLKEM768_SHA3_H` include guard
- 177: `sntrup761.c` — stripped 91 unused cryptoint functions (~1600 lines); 9 kept (5 directly called + 4 transitive deps); `CRYPTOINT_MAYBE_UNUSED` macro replaces `__attribute__((unused))` on 2 generic-path-only helpers
- 174: `kex/sntrup761_optblocker.c` added — defines `crypto_int{16,32,64}_optblocker` volatile globals for non-x86\_64/aarch64 architectures (matches SUPERCOP `cryptoint/optblocker.c`)
- 175: `libcrux_mlkem768_sha3.h` `core_num__u8_6__count_ones` — added `#elif !defined(__GNUC__)` branch with portable bit-twiddling popcount for strict C17 compilers
- 176: `mlkem768.c` byte-order detection extended — `__BIG_ENDIAN__`, `_BIG_ENDIAN`, `__BYTE_ORDER`/`__BIG_ENDIAN` added alongside GCC/Clang's `__BYTE_ORDER__`
- 170: `curve25519-sha256.c` server ECDH_REPLY `reply_sz` sum overflow-checked with sequential SIZE_MAX guards (follows `dh-gex-sha256.c` pattern)
- 171: `hybrid-pq-kex.c` server REPLY `reply_sz` sum overflow-checked with sequential SIZE_MAX guards; UINT32_MAX pre-flight on `k_s_len`, `q_s_len`, `sig_len`
- 172: `curve25519-sha256.c` `k_s_len`/`sig_len` range-checked against UINT32_MAX before narrowing; inline casts replaced with initializer-style
- 173: `hybrid-pq-kex.c` `k_s_len`/`q_s_len`/`sig_len` range-checked against UINT32_MAX before narrowing; REPLY_SER macro now takes pre-checked uint32_t variables
- 163: `send_info_request` UINT32_MAX checks on `name_len`, `inst_len`, `plen` — `#if SIZE_MAX > UINT32_MAX` guards added (follows `flush_pending_banner` pattern)
- 164: Server KBI unbounded `calloc` from `num_responses` — rejected when `num_responses > last_nprompts`; new test `kbi_excess_responses`
- 165: Client KBI unbounded `calloc` from `num_prompts` — hard cap of 256 with `DSSH_ERROR_PARSE`; new test `kbi_too_many_prompts`
- 166: `event_queue_push` capacity overflow — `SIZE_MAX / 2` guard before `capacity * 2`
- 167: `send_to_slot` unchecked narrowing — `UINT8_MAX` range check on `payload_len` at function entry
- 168: `stream_zc_cb` inline cast → initializer (`uint32_t ret = (uint32_t)written; return ret;`)
- 169: `handle_channel_extended_data` inline cast → initializer (`int stream = (int)data_type;`)
- 160: Split `dhgex_handler_impl()` and `hybrid_pq_handler_impl()` into separate client/server static helpers with single `goto cleanup` labels — eliminates ~40 repetitive free-block error sites
- 161: `mlkem768.c` byte-swap `#undef htole64`/`le64toh`/`le32toh` replaced with libcrux-local `lcx_htole64`/`lcx_le64toh`/`lcx_le32toh` — no longer overrides system macros
- 159: `derive_key` `tmp` buffer changed from `uint8_t tmp[64]` stack array to `malloc(md_len)` — forward-compatible with any hash digest size, no hardcoded limit
- 162: `examples/kex_test.c` moved to `test/kex_test.c` — depends on `dssh_test_reset_global_config()` and `deucessh_test_lib`, not a standalone example
- 158: `project()` changed from `LANGUAGES C CXX` to `LANGUAGES C`; `enable_language(CXX)` added conditionally after Botan backend selection — OpenSSL-only builds no longer require a C++ compiler
- 151: Removed DH-GEX helper unit tests — covered by 16 DH-GEX integration test variants (client+server, all KEX/key/MAC combos)
- 156: Botan RNG atexit race — eliminated entirely; `crypto/botan-rng.c` singleton deleted, all Botan modules converted from FFI to native C++ API using `Botan::system_rng()` directly
- 153: `x25519_exchange` `peer_pub_len` validation added to all 4 hybrid PQ backend implementations (sntrup761+mlkem768, OpenSSL+Botan); rejects `peer_pub_len != HYBRID_PQ_X25519_KEY_LEN`
- 145: `dssh_hash_oneshot` in Botan backend — `h->update()` and `h->final()` wrapped in try/catch to prevent C++ exceptions escaping `extern "C"` boundary
- 144: OpenSSL `dssh_hash_update`/`dssh_hash_final` — added `ctx->mdctx == NULL` check to both functions; prevents NULL dereference after `dssh_hash_final` re-init failure
- 143: `dssh_base64_encode` 32-bit overflow — added `len > (SIZE_MAX - 3) / 4` guard to OpenSSL backend; added `needed > SIZE_MAX - 1` and `count > SIZE_MAX - 1` guards to Botan backend
- 157: `dssh_hash_final` `out == NULL` check added to both OpenSSL and Botan backends (matches `dssh_hash_oneshot`); new `test_hash_final_null_out` test confirms crash-before/pass-after
- 152: Memory leak in `test/test_alloc.c` — `p_bytes`/`g_bytes` freed on `select_group` error path before `goto done`
- 150+149: 11 DH-GEX server handler tests unblocked for Botan — removed `#ifdef DSSH_CRYPTO_OPENSSL` guard from infrastructure, test functions, and test table; `dummy_sign`/`dummy_pubkey` no longer cause `-Wunused-function`
- 148: Function pointer cast UB in PEM view callback — `pem_view_cb` signature changed from `const uint8_t *` to `const char *` in all 3 Botan key_algo files; `(botan_view_str_fn)` casts removed from 6 call sites
- 146: Unchecked `dssh_botan_get_rng()` return value — 9 call sites in 3 Botan key_algo files now extract RNG into local, check for 0, return `DSSH_ERROR_INIT` with proper cleanup
- 155: Server GEX_REPLY `reply_sz` sum overflow-checked with sequential SIZE_MAX guards; `k_s_len`/`sig_len` range-checked against UINT32_MAX before narrowing; inline casts replaced with initializer-style
- 154: `g_mpint_len` underflow guard added — `p_mpint_data_len > group_consumed - 4` check before subtraction
- 147: `verify()` in all 4 RSA key_algo files now sets `result = DSSH_ERROR_PARSE` (or `DSSH_ERROR_INVALID` for `slen > INT_MAX`) before `goto done` at `n` modulus parse failure
- 142: `parse_bn_mpint`/`parse_mp_mpint` bounds check changed from `4 + len > bufsz` (uint32_t overflow) to `len > bufsz - 4` (subtraction form, safe since bufsz >= 4 pre-checked)
- 141: Password callback overflow fixed — added `pw_len < (int)sizeof(pw_buf)` upper bound check at all 6 sites in 3 Botan key_algo files; rejects password when no room for null terminator
- 139: `examples/kex_test.c` `sock_fd` changed to `_Atomic int`; `on_terminate()` loads into local before shutdown to eliminate TOCTOU
- 136: `compute_shared_secret()` hard-coded `combined[64]` replaced with `malloc(combined_len)`; buffer size now derived from `p->pq_ss_len + HYBRID_PQ_X25519_KEY_LEN` with overflow guard
- 135: `kex/dh-gex-groups.h` static arrays/function/provider moved to new `kex/dh-gex-groups.c`; header now declarations only; unused include removed from `kex/dh-gex-sha256.c`
- 134: `botan_cipher_update()` in `enc/aes256-ctr-botan.c` now checks `outwritten == bufsz` and `inconsumed == bufsz` after successful return
- 137: `dssh_hash_update()` NULL data check added to both backends (matches `dssh_hash_oneshot()`)
- 140: `dssh_crypto_memcmp()` NULL input check added to both backends
- 133: Use-after-free in Botan key_algo `cleanup()` — comparison moved before `free()` in all 3 files (ed25519, rsa-sha2-256, rsa-sha2-512)
- 130: Unchecked RNG in Botan sign operations — `dssh_botan_get_rng()` return checked before `botan_pk_op_sign_finish()` in all 3 key_algo files
- 132: Buffer overread on curve25519-botan derive failure — saved original alloc size for `dssh_cleanse()` on error path
- 131: Key material not cleansed on DH-GEX Botan error paths — added `botan_mp_set_from_int(k, 0)` before `botan_mp_destroy(k)` in both client and server derive; OpenSSL backend already uses `BN_clear_free()`
- 125: Algo/alloc unit tests unblocked for Botan backend — `#ifdef DSSH_CRYPTO_OPENSSL/BOTAN` conditionals in test_algo_enc.c (Botan FFI cipher helper), test_algo_mac.c (Botan FFI HMAC helpers), test_algo_key.c (struct key_cbdata, e_pad/n_pad/ossl-injection tests), test_alloc.c (ossl iterate tests); CMakeLists.txt gates removed; 19 new CTest entries; key_algo coverage +31-40pp
- 126: `crypto/botan-rng.h` static globals moved to single definition in `crypto/botan-rng.c`; 7 per-TU RNG handles reduced to 1 process-wide singleton; `atexit(botan_rng_destroy)` added for cleanup
- 129: `ka` null checks made unconditional in all 3 shared KEX files (removed `#ifdef DSSH_TESTING` guard from `curve25519-sha256.c` client-side check to match `dh-gex-sha256.c` and `hybrid-pq-kex.c`)
- 128: Removed dead `if (p_sz > SIZE_MAX - 0)` check in `dh-gex-sha256-openssl.c`
- 127: Removed wrong `if (p_consumed > SIZE_MAX - p_consumed)` overflow check in `dh-gex-sha256-botan.c` (real check at line 221 is correct)
- 124: Added `combined_len > sizeof(combined)` stack overflow guard in `hybrid-pq-kex.c:compute_shared_secret()`
- 115: All 4 KEX methods deduplicated — shared protocol handlers with crypto ops vtables; curve25519 (`dssh_c25519_ops`), sntrup761+mlkem768 (`hybrid_pq_handler_impl`), dh-gex (`dhgex_ops` with pre-serialized mpints)
- 119: OpenSSL hash name mapping replaced with explicit lookup table (`map_hash_name`) instead of fragile hyphen-stripping
- 118: RFC 3526 DH group tables extracted to `kex/dh-gex-groups.h`; `default_select_group()` and `default_provider` shared between OpenSSL and Botan DH-GEX modules
- 117: Per-call `botan_rng_init`/`botan_rng_destroy` in 4 Botan KEX modules replaced with shared `dssh_botan_get_rng()` singleton
- 116: Mixed tabs/spaces in `curve25519-sha256-botan.c` fixed (16 comment lines used spaces instead of tabs)
- 108: Duplicated RNG boilerplate in 3 Botan key_algo modules replaced with shared `crypto/botan-rng.h` header
- 123: `DSSH_MPINT_SIGN_BIT` moved from `ssh-internal.h` to `deucessh-crypto.h`; removed 8 redundant `#ifndef` definitions across OpenSSL and Botan modules
- 121: Added `dssh_hash_oneshot()` one-shot hash API to both backends; sntrup761.c `crypto_hash_sha512` now uses it (eliminates per-call context alloc/free)
- 114: `dssh_hash_final()` OpenSSL backend now NULLs out `mdctx` if re-init after finalize fails, preventing use of undefined EVP state
- 107: Botan `rng_init_once()` now stores init result in `_Atomic int rng_init_ok`; `get_rng()` returns 0 on failure (crypto/botan.c + 3 key_algo files)
- 110: `dssh_random()` OpenSSL backend now rejects `len > INT_MAX` before `RAND_bytes` cast
- 111: `dssh_base64_encode()` OpenSSL backend now rejects `len > INT_MAX` before `EVP_EncodeBlock` cast
- 112: `dssh_base64_encode()` Botan backend now checks `count > INT_MAX` before return cast
- 113: `dssh_hash_final()` returns `DSSH_ERROR_INVALID` (not `DSSH_ERROR_TOOLONG`) when `outlen < digest_len`

- 106: Selftest write-before-window races across 12 tests (poll for DSSH_POLL_WRITE before first write)
- 104: Selftest echo tests failing under `-j16` (same root cause as 106)
- 105: `send_channel_request_wait` race — CHANNEL_CLOSE clobbers successful response
- 103: Selftest race — cleanup while server echo thread still sending
- 102, 102a-c: Malformed message parse failures now send required replies before disconnect
- 99: Callback setters after `dssh_session_start()` now rejected (DSSH_ERROR_TOOLATE)
- 96: Signal pointer lifetime documented (superseded by channel I/O redesign)
- 95, 101: Channel I/O redesign — `dssh_chan_*` API, stream + ZC, event model, accept
- 93: `dssh_transport_register_lang()` moved to public `deucessh-lang.h`
- 97: `dssh_parse_uint32()` / `dssh_serialize_uint32()` return values documented
- 100: Opaque-handle typedefs replaced with struct pointers in internal code
- 92: `dssh_channel_read()` peek mode fixed (NULL buf with bufsz 0)
- 94: Wrong channel type now detected at runtime (DSSH_ERROR_INVALID)
- 91: Redundant `dssh_*` typedefs removed (7 unused type aliases)
- 28: Near-duplicate read/write pairs merged (`session_readable`, `session_read_impl`)
- 90: Symbol prefix cleanup — `dssh_` for public, `dssh_test_` for testing, none for internal
- 16: Type-unsafe linked list traversal — `_Static_assert` on `next` offset + typed `FREE_LIST`
- 18: Six near-identical `dssh_transport_register_*()` replaced with `DEFINE_REGISTER` macro
- 76: Demux head-of-line blocking under `tx_mtx` — `send_or_queue()` with trylock
- 83: `dssh_session_cleanup()` hang on `thrd_join` — terminate callback + single-fire guard
- 26, 30: `demux_dispatch()` and `dssh_session_accept_channel()` decomposed
- 17: Cascading allocation/cleanup duplication — unified `goto` cleanup
- 69: Self-initiated rekey data loss — buffer non-KEXINIT messages, replay after rekey
- 65, 66: Session-wide inactivity timeout — `dssh_session_set_timeout()`, `DSSH_ERROR_TIMEOUT`
- 64: Poll/accept deadline reset on spurious wakeup — compute deadline once
- 12: KBI client-side allocation cleanup (`goto kbi_cleanup`)
- 13: ssh-auth.c magic numbers replaced with named macros
- 27: Timeout computation helper extracted (`deadline_from_ms`)
- 29: ssh-conn.c magic numbers replaced with computed expressions
- 88: Symbol visibility audit — `DSSH_PRIVATE` added to 9 unannotated functions
- 85, 86: C11 threading return values now checked (`dssh_thrd_check` wrapper)
- 87: Shutdown path tolerates prior lock/condvar failures
- 62, 79: Channel close use-after-free with demux thread — `atomic_bool closing`
- 89: Dead code removal — 14 unused `DSSH_PRIVATE` functions removed
- 53: Data race: rekey counters split into tx/rx, tx made atomic
- 57: Data race: algorithm query `*_selected` pointers made `_Atomic`
- 60: Data race: `dssh_key_algo_set_ctx()` — `DSSH_ERROR_TOOLATE` after first session
- 61: Data race: `dssh_dh_gex_set_provider()` — removed, replaced by built-in provider
- 32: Data race: callback setters documented as must-call-before-start
- 52: Data race: setup-to-normal transition protected by `buf_mtx`
- 8: `dssh_auth_server()` username buffer — in/out `username_out_len` parameter
- 58, 59: Data race: unlocked flag pre-checks moved under `buf_mtx`
- 73: `cnd_signal` on `poll_cnd` changed to `cnd_broadcast`
- 68: `dssh_session_start()` double-start guard fixed
- 70: Server auth `DSSH_AUTH_DISCONNECT` callback return value
- 6, 45, 46, 49, 50: NULL parameter validation for public API functions
- 51, 63, 84: Data race and stale-window bugs in window accounting
- 43: Non-ASCII characters in comments replaced with ASCII equivalents
- 9, 44: Error code accuracy audit — `DSSH_ERROR_AUTH_REJECTED`, `DSSH_ERROR_REJECTED`
- 23: Data race in `dssh_session_write()` / `write_ext()` — reads under `buf_mtx`
- 24: `dssh_session_accept_channel()` fail path leaked `setup_payload`
- 33: `dssh_session_set_terminate()` skipped channels with `chan_type == 0`
- 7: `send_auth_failure()` and `flush_pending_banner()` stack/truncation fixes
- 8: `auth_server_impl()` SERVICE_ACCEPT stack buffer overflow
- 16: Peer KEXINIT parsing OOB read
- 26: Use-after-free race in channel close — hand-over-hand locking
- 29: `demux_dispatch()` CHANNEL_DATA/EXTENDED_DATA silent truncation rejected
- 2: Serialize overflow checks — subtraction form to prevent `size_t` wrap on 32-bit
- 21: `serialize_namelist_from_str()` overflow and silent truncation
- 1: Dead `dssh_bytearray` type removed
- 3: `dssh_parse_namelist_next` removed from public API
- 38: Unused `dssh_transport_packet_s` struct removed
- 25: Stale comment in `open_session_channel()` fixed
- 40: `dssh_auth_server()` doc fixed (username copied, not pointed into session buffer)
- 42: Message type 60 aliasing comment added (PK_OK/PASSWD_CHANGEREQ/INFO_REQUEST)
- 4: ssh-arch.c visibility annotations verified correct
- 5: deucessh-arch.h verified clean (no `<openssl/bn.h>`)
- 34: `deucessh-algorithms.h` — `pem_password_cb` replaced with `dssh_pem_password_cb`
- 37: Duplicate transport function declarations removed from ssh-trans.h
- 39: Unnecessary `<threads.h>` include removed from ssh-chan.h
- 41: `extra_line_cb` typedef applied
- 71: `dssh_session_accept_channel()` / `dssh_channel_accept_raw()` leaked `inc` on error
- 72: `dssh_transport_init()` leaked `tx_mtx` when `rx_mtx` init failed
- 77: DH-GEX `dhgex_handler()` leaked BIGNUM `p` on size-check failure
- 78: Missing `ka`/`ka->verify`/`ka->pubkey`/`ka->sign` NULL checks in PQ KEX handlers
- 80: Setup mailbox `malloc` failure silently dropped message — `setup_error` flag added
- 82: Auth banner handling drained only one banner — `if` changed to `while`
- 14: Magic numbers 80/81/82 replaced with `SSH_MSG_GLOBAL_REQUEST` etc.
- 35: Duplicate `DSSH_CHAN_SESSION` / `DSSH_CHAN_RAW` definitions removed
- 36: Duplicate `SSH_OPEN_ADMINISTRATIVELY_PROHIBITED` definition removed
- 44: `SSH_MSG_UNIMPLEMENTED` callback already invoked (stale TODO)
- 54: Data race: `rekey_in_progress` changed to `atomic_bool`
- 55: Lost wakeup: `set_terminate()` uses `mtx_trylock` on `tx_mtx` for `rekey_cnd`
- 56: Data race: `conn_initialized` changed to `atomic_bool`
- 15: `recv_packet_raw()` asymmetric termination on REKEY_NEEDED fixed
- 74: Bytebuf write truncation caused window accounting drift
- 81: Accept queue size — not a library concern (application's responsibility)
- 10, 11: `auth_server_impl()` decomposed into per-method handlers
- 19, 20, 22: `kexinit()` and `newkeys()` decomposed into helpers
- 31: ssh-chan.c/h folded into ssh-conn.c
- ssh-arch.c/h and deucessh-arch.h eliminated (inlined into ssh.c / deucessh.h)
