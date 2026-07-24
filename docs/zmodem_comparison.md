# ZMODEM implementation comparison: sexyz vs. lrzsz vs. Forsberg rzsz

**Date:** 2026-07-23
**Author:** Claude (analysis commissioned by Rob Swindell)
**Scope:** Performance, scalability (>2 GB / >4 GB), and robustness of
Synchronet's ZMODEM implementation, benchmarked against `lrzsz` 0.12.21rc and
Chuck Forsberg's final `rzsz` (3.73, 2003-01-30).

---

## 0. TL;DR

- **Throughput:** the Synchronet **`sexyz` *sender* is ~25× slower than `lsz`**
  on a clean link (≈8 MB/s vs ≈204 MB/s). This is **two stacked overheads**,
  isolated by linking the real `zmodem.c` behind each send architecture (§3.2):
  - **`zmodem.c` per-byte design (~8×, 204→26 MB/s): SHARED — affects SyncTERM.**
    Its `send_byte`-callback-per-byte + ZDLE-escape + CRC pipeline costs ~38
    ns/byte vs `lsz`'s ~5 ns/byte inlined. A buffered sender on `zmodem.c` tops
    out at ~26 MB/s regardless of block size.
  - **`sexyz.c` ring-buffer + output-thread (~3×, 26→8 MB/s): sexyz-only.** Per-
    byte `RingBufWrite` + event signaling ping-pongs with the drain thread — 2.3 M
    futex calls and ~84-byte fragmented `write()`s per 32 MB. **SyncTERM avoids
    this layer** (buffered single-threaded send), so its ZMODEM upload runs
    ~26 MB/s — 3× faster than sexyz, still ~8× slower than lrzsz.
- **sexyz *receiver* is fine** (~100 MB/s), including past 2 GB.
- **Forsberg** (modern branch) *sender* runs at **~92 MB/s** (between lrzsz and
  sexyz; limited by its 1 KB block cap). Its *receiver* won't complete a transfer
  headlessly at all (serial-tty assumptions; segfaults under some transports).
- **>2 GB:** the shared `zmodem.c` narrows several file positions to
  **signed `int32_t`**, corrupting the sender's transmit-window/ACK arithmetic
  above 2 GB. Because SyncTERM shares `zmodem.c`, **SyncTERM is affected too**.
- **4 GB** is a hard ceiling for all three (the ZMODEM header position field is
  32-bit on the wire).
- **Adaptivity:** `lrzsz` continuously tunes its block length to the measured
  error rate (`calc_blklen`); `zmodem.c` (sexyz + SyncTERM) only does a blind
  ×2-up / ÷2-down ramp. Feature flags (`-8`, `-w`) are at parity.

Which component each finding lives in:

| Finding | `sexyz.c` | shared `zmodem.c` | SyncTERM `term.c` |
|---|:--:|:--:|:--:|
| Ring-buffer/thread send collapse (26→8, futex/tiny-writes) | ✗ (bug here) | — | OK (immune) |
| Per-byte send cost (204→26, callback+escape+CRC) | — | ✗ (weakness) | ✗ (inherits, ~26 MB/s) |
| `int32_t` >2 GB window/ACK corruption | — | ✗ (bug here) | ✗ (inherits) |
| Non-adaptive block-size ramp | — | ✗ (weakness) | ✗ (inherits) |
| 4 GB wire-field ceiling | — | inherent to protocol | inherent |
| Receiver >2 GB correctness | OK | OK | OK |

---

## 1. Codebases

| Impl | Source | Lines | Model |
|---|---|--:|---|
| **sexyz** | `src/sbbs3/{zmodem.c,sexyz.c,zmodem.h}` | 2440+1996 | multi-threaded (in/out threads + ring buffers) |
| **SyncTERM** | shared `src/sbbs3/zmodem.c` + `src/syncterm/term.c` wrapper | — | single-threaded, buffered send |
| **lrzsz** 0.12.21rc | `src/{zm.c,lsz.c,lrz.c}` | 982+2493+2314 | single-threaded, stdio-buffered |
| **Forsberg rzsz** 3.73 | `zm.c,sz.c,rz.c` | 876+1746+1383 | single-threaded, stdio-buffered |

`sexyz` and `SyncTERM` share `zmodem.c` (the protocol engine: framing, block
sizing, streaming, `zmodem_send_file`→`zmodem_send_from`, `zmodem_recv_files`).
They differ only in the transport wrapper (the `send_byte`/`recv_byte`/`flush`
callbacks passed to `zmodem_init`).

---

## 2. Benchmark method

- **Harness:** `zbench.py` — wires a sender and receiver (each speaking ZMODEM
  on stdin/stdout) through a userspace relay that can inject one-way latency and
  a token-bucket bandwidth cap in each direction. No root/`netem` required.
- **Relay ceiling** measured at **5069 MB/s** — 25×+ above any tool, so it is
  not the bottleneck for clean-link runs.
- **Transport:** localhost pipes (CPU/architecture-bound regime), on tmpfs
  (RAM-backed) so disk never confounds protocol behavior.
- **Flags:** `-8` (8 KB max block, ZedZap) on both sides; `-w` (transmit window)
  where noted. `sexyz` binary: `gcc.linux.x64.exe.release/sexyz`
  (`v3.3 master/074785210`). lrzsz built from `~/lrzsz-0.12.21rc`.
- **Integrity:** SHA-256 of source vs. received file every run.
- **Content:** random data (`/dev/urandom`), so ZDLE-escaping cost is realistic
  (~2.8% wire expansion — identical across all impls, confirming equal protocol
  efficiency).
- **Transports:** three harness variants — separate stdin/stdout pipes (sexyz,
  lrzsz), a single bidirectional socket duped to both fds (needed for Forsberg's
  `sz`, which reads+writes one fd), and raw ptys (for tty-assuming tools). All
  three give matching numbers for sexyz/lrzsz, so transport is not a confound.
- **Forsberg build:** from the rzsz history repo's **`modern` branch** (POSIX
  target), which was required — the 2003 tarball segfaults on non-tty fds.
  Forsberg caps at 1 KB subpackets, so it has no `-8` equivalent.

---

## 3. Throughput (32 MB, clean localhost, `-8` where supported, unlim. window)

| Sender → Receiver | Goodput | Note |
|---|--:|---|
| lsz → lrz (lrzsz baseline) | **203.8 MB/s** | 8 KB adaptive blocks |
| **Forsberg sz** → lrz | **~92 MB/s** | 1 KB blocks (no ZedZap); single-threaded |
| lsz → **sexyz** (sexyz *receives*) | **106.5 MB/s** | sexyz receiver OK |
| **sexyz** → lrz (sexyz *sends*) | **5.7–8.9 MB/s** | sexyz sender is the bottleneck |
| sexyz → sexyz | **6.2 MB/s** | |

Wire overhead is identical (+2.81 %, +2.55 % for Forsberg's 1 KB blocks) —
protocol efficiency is the same; the difference is purely implementation.

**Sender ranking:** lrzsz (204) > Forsberg (92) > sexyz (6–9). Forsberg trails
lrzsz only because it caps subpackets at 1024 bytes (no 4K/8K ZedZap), paying
~8× more per-block CRC/header overhead; it is otherwise a clean single-threaded
buffered sender like `lsz`. sexyz is an order of magnitude below both.

### 3.0 Forsberg's receiver could not be benchmarked headlessly

Forsberg's **`rz` (receiver) does not complete a transfer** in any headless
transport tested (socketpair, pty, and `socat`-bridged ptys; receiving from
either `lsz` or Forsberg `sz`; with or without a serial-rate cap). It handshakes
(ZRINIT → ZRPOS) then stalls after ~9 KB, and **segfaults** under `socat`'s pty.
The 2003 original is worse — it segfaults immediately on a socketpair; the
**"modern" branch** (the GitLab history repo, with readability/portability fixes)
was required to get even the sender running. This fragility outside a real serial
tty is itself a finding — the kind of thing that motivated the maintained `lrzsz`
fork. Only Forsberg's **sender** number above is trustworthy.

### 3.1 Root cause of the sender collapse (syscall profile, 32 MB)

| Sender | `write()` calls | avg write | `futex` calls | rate under strace |
|---|--:|--:|--:|--:|
| **lsz** | 12,302 | ~2,803 B | 0 (single-threaded) | 126 MB/s |
| **sexyz** | 409,188 | **~84 B** | **2,268,600** (872 K err) | 2.3 MB/s |

`sexyz.c` feeds the protocol output one byte at a time into a `RingBuf`
(`send_byte`→`RingBufWrite`, with event signaling), drained by a separate
`output_thread`. The producer and consumer ping-pong: the consumer wakes on
each trickle, drains ~84 bytes, `write()`s, and waits again — a futex storm and
massively fragmented output. Disabling the `OutbufHighwaterMark` /
`OutbufDrainTimeout` batching (via `sexyz.ini`) changed nothing (6.13→6.18
MB/s): the batching only engages when the ring hits *empty*, which steady
production avoids.

sexyz's three possible send architectures (the third is the fix — see §6):

| sexyz `send_byte` config | threads | batching | per-32 MB kernel cost |
|---|---|---|---|
| `SINGLE_THREADED FALSE` (built default) | 2 (ring + drain) | fragmented ~84 B writes | 2.3 M `futex` |
| `SINGLE_THREADED TRUE` (`sexyz.c:84`, source-edit only) | 1 | **none** — 1 byte per `write()` | ~34 M `write()` |
| buffered (SyncTERM `term.c` / `ztx_buf`) | 1 | **flush per subpacket** | ~8 K `write()`, 0 futex |

The single-threaded build is a portability/debug fallback, **not** a faster
mode: it drops the thread but keeps sending one byte per `write()` syscall.

**SyncTERM does not have *this* layer.** Its `send_byte` (`term.c:874`)
accumulates into an 8 KB `transfer_buffer` and `flush_send`→`conn_send` writes it
in bulk (one send per subpacket, driven by `zmodem_flush`) — single-threaded, no
ring buffer, no futex. So it escapes the 26→8 collapse. But it still rides on
`zmodem.c`'s per-byte send pipeline, which has its own cost (next section).

### 3.2 Splitting the two layers (real `zmodem.c`, two send paths)

To separate "how much is `zmodem.c`" from "how much is `sexyz.c`", a ~150-line
test sender (`ztx_buf.c`) was linked against the **real, unmodified
`zmodem.o`** — the same object sexyz uses — but with a **SyncTERM-style buffered
`send_byte`** (accumulate into an array, bulk `write()` per subpacket; no ring
buffer, no thread). This is a faithful model of SyncTERM's own sender. Sending
32 MB to `lrz`, identical wire bytes in every case:

| Sender | Send path (all drive the same `zmodem.c` except `lsz`) | Goodput |
|---|---|--:|
| `lsz` | lrzsz's own inlined `zm.c` | **204 MB/s** |
| `ztx_buf` = **SyncTERM model** | `zmodem.c` + **buffered** send | **~26 MB/s** |
| `sexyz` | `zmodem.c` + **ring-buffer + output thread** | **8 MB/s** |

`strace` of `ztx_buf`: **no futex**, 8 260 `write()`s, ~5 ms total in syscalls
for a 1.3 s run — i.e. it is **CPU-bound inside `zmodem.c`**, at ~38 ns/byte, on
the `send_byte`-callback-per-byte + ZDLE-escape + CRC-32 pipeline. Block size
barely moves it (1 KB → 24.5 MB/s, 8 KB → 26 MB/s), confirming the cost is
per-byte, not per-block.

**Conclusion:** the ~25× sexyz-vs-lrzsz gap is **~8× `zmodem.c`** (per-byte
design, shared with SyncTERM) **× ~3× `sexyz.c`** (ring/thread, sexyz-only).
SyncTERM's ZMODEM upload lands at ~26 MB/s — meaningfully faster than sexyz, but
still far short of lrzsz, because both Synchronet tools inherit `zmodem.c`'s
per-byte send cost. Closing the full gap needs both: fix `sexyz.c`'s TX path
(the easy 3×) *and* reduce `zmodem.c`'s per-byte overhead (batch the escape/CRC
inner loop instead of a function-pointer call per byte).

---

## 4. Scalability: >2 GB and >4 GB

### 4.1 The shared 4 GB ceiling

The ZMODEM header carries file position in a **32-bit field (ZP0–ZP3)**. No
implementation can express a position ≥ 4 GB in a header; all three are capped
at 4 GB for in-band resync. Confirmed in `zmodem.c:1314`, lrzsz `rclhdr()`,
Forsberg `zm.c:531`.

### 4.2 sexyz/SyncTERM break earlier — at 2 GB (signed `int32_t`)

`zmodem.c` reconstructs the wire position cleanly into `uint32_t
rxd_header_pos` (good to 4 GB), but then narrows it to **signed `int32_t`** on
the accounting paths:

- `zmodem.h:263` `int32_t ack_file_pos;`
- `zmodem.h:279` `int32_t crc_request;`
- `zmodem.c:596` `zmodem_send_pos_header(..., int32_t pos, ...)`
- `zmodem.c:614` `zmodem_send_ack(..., int32_t pos)`
- `zmodem.c:1322` `zm->ack_file_pos = zm->rxd_header_pos;` (uint32→int32)
- `zmodem.c:1614` `zm->current_window_size = zm->current_file_pos - zm->ack_file_pos;`

Above 2 GB, `ack_file_pos` goes negative, so `current_window_size` (int64 −
negative int32) balloons to ~2 GB, permanently exceeding any configured
`max_window_size` and forcing the sender into transmit-window throttling. The
*wire encoding* survives (byte masking), so the data path itself is intact — but
a windowed (`-w`) send past 2 GB throttles/stalls. **Shared → SyncTERM
inherits this on uploads.**

Note: sexyz's *file I/O* uses `fseeko`/`ftello` + `int64_t` counters
(`zmodem.h:250-251`), so the data path is 64-bit clean; only the protocol-
position arithmetic is broken.

### 4.3 lrzsz / Forsberg

Both reconstruct into 64-bit `long` with `& 0377` masking → clean, unsigned,
through 4 GB. On a **32-bit (ILP32) build**, however, `long` is 32-bit and
`fseek()` takes a `long` offset (no `_FILE_OFFSET_BITS=64` in lrzsz's build), so
lrzsz/Forsberg cap at **2 GB** on 32-bit hosts — same wall as sexyz, different
cause (native `long` width vs. deliberate `int32_t` narrowing).

### 4.4 Empirical (2.2 GB file, crosses 2³¹ = 2 147 483 648, `-w1M`)

| Run | Result | Goodput |
|---|---|--:|
| lrzsz → lrzsz, `-w1M` | **MATCH** (full 2.36 GB) | 186.9 MB/s |
| lsz → **sexyz** (receiver), `-w1M` | **MATCH** | 100.9 MB/s |
| **sexyz** → lrz, no window (control) | **MATCH** (full 2.36 GB) | 6.98 MB/s |
| **sexyz** → lrz, **`-w1M` (windowed)** | **HANG at 2³¹** | — |

The windowed sexyz sender **stalls permanently the instant the offset crosses
2³¹**, at 2,147,713,024 bytes (~229 KB past 2 GB). Its log shows the exact
signed-overflow signature:

```
!2147744768 Transmit-Window management: 4294975488 >= 1048576
!Receive timeout (1 seconds)          ← repeats until timeout
```

The computed transmit window is **4,294,975,488 ≈ 2³² + 8192** — i.e.
`current_file_pos (2,147,744,768) − ack_file_pos`, where `ack_file_pos` has gone
negative (`int32_t` overflow at 2 GB). It permanently exceeds the 1 MB window,
so the sender throttles, waits for a ZACK that can never satisfy the (corrupt)
window, times out, and hangs.

Confirmed conclusions:
- sexyz **receiver** is correct past 2 GB (int64/`fseeko` data path).
- sexyz **sender without a window** is correct past 2 GB (window check is
  short-circuited when `max_window_size == 0`) — only slow.
- sexyz/SyncTERM **sender *with* a window** (`-w`) hangs at 2 GB. Since the bug
  is in shared `zmodem.c`, **a SyncTERM upload with a transmit window would hang
  identically.**
- lrzsz handles a windowed >2 GB transfer cleanly (64-bit LP64 build).

---

## 5. Adaptivity & robustness on variable links

- **lrzsz `calc_blklen()`** models expected bytes-on-wire as a function of block
  length given the measured `bytes_per_error` (constants `OVERHEAD=18`,
  `OVER_ERR=20`) and picks the throughput-optimal block size continuously —
  growing when clean, shrinking hard on errors.
- **`zmodem.c` (sexyz + SyncTERM)** only does a blind ×2 grow on success
  (`zmodem.c:1727`) and ÷2 shrink on error (`zmodem.c:1982`), with default
  `max_block_size` 1024 (raise with `-8`). No cost model.
- **Windowing:** all three expose `-w`; sexyz defaults it off
  (`max_window_size=0` = unlimited). `-w0` is **not** comparable across tools
  (sexyz: unlimited; lsz: clamps to 256-byte window) — use a nonzero byte count
  on both.
- **Threading:** sexyz's dedicated input thread keeps the back-channel drained
  (fast reaction to ZRPOS/ZACK under latency) — the one upside of its
  architecture — but at the cost of the sender throughput collapse in §3.

### 5.1 Empirical conditions matrix (sender-isolated → lrz unless noted)

| Condition | lrzsz | sexyz | Forsberg | Takeaway |
|---|--:|--:|--:|---|
| Clean, `-2`/`-4`/`-8` (32 MB) | 156 / 156 / **204** | 8.4 / 8.2 / 8.7 | ~100 (1 KB only) | sexyz block-insensitive (arch-capped); lrzsz gains from `-8` |
| CRC-32 vs CRC-16 (32 MB) | 204 / 156 | 8.8 / 8.3 | — | negligible for sexyz |
| Bandwidth cap 4 MB/s, back 16 KB/s (8 MB) | **3.87** | **3.87** | **3.87** | link-bound: **all equal** when link < ~8 MB/s; asymmetry fine |
| Bandwidth cap 1 MB/s sym (8 MB) | 0.97 | 0.97 | — | link-bound, equal |
| Injected bit-errors (8 MB) | recovers, adaptive | **very slow / TIMEOUT** | hit-or-miss | sexyz recovers correctly but pathologically slowly |

Two caveats on this matrix:
- **Latency** was also swept (5/25/100 ms one-way), but the harness's per-chunk
  `sleep` serializes rather than pipelines, so absolute numbers overstate latency
  impact; *relative* to each other, windowing (`-w`) hurts badly under latency
  (a round-trip per window) and **sexyz windowed timed out at every latency**.
- **Error injection** flips random bits, hitting headers and data
  indiscriminately, and the same seed lands on different logical offsets per
  protocol — so results are non-monotonic and **directional only**, not clean
  science. The robust signal is qualitative: sexyz stalls under errors; lrzsz's
  `calc_blklen` shrink-on-error recovers fastest.

**Practical implication:** sexyz's sender deficit is invisible on links slower
than ~8 MB/s (dial-up, most serial, throttled internet) — everyone is link-bound
there. It only bites on LAN / localhost / fast-internet transfers, which is
exactly where a modern BBS file transfer runs.

---

## 6. Recommendations (no code changed yet)

Ordered by impact:

1. **[sexyz.c] Fix the ring-buffer/thread send path.** Batch escaped output into
   the ring in spans (or bypass the ring for bulk data), so `write()`s are
   subpacket-sized, not ~84 B, and the producer/consumer stop ping-ponging on a
   futex. ~3× win (8→26 MB/s); sexyz-only. Brings sexyz up to SyncTERM's level.
   **Caveat — don't just flip `SINGLE_THREADED`.** `sexyz.c:84` still has a
   `#define SINGLE_THREADED FALSE` (source-level only; no makefile toggle), but
   its single-threaded `send_byte` (`sexyz.c:682`) calls `sendbuf()` with
   `len==1` — an **unbuffered `write()` per byte** (~34 M syscalls/32 MB). That
   trades the futex storm for a write-syscall storm and is generally *worse*. The
   fix is **buffering**, not thread count: accumulate + flush per subpacket
   (single-threaded *and* buffered, exactly as SyncTERM's `term.c` does).
2. **[zmodem.c] Reduce the per-byte send cost.** The `send_byte`-callback-per-
   byte + escape + CRC pipeline caps *any* sender on `zmodem.c` at ~26 MB/s
   (§3.2). Batch the ZDLE-escape/CRC inner loop over a span and hand `send_byte`
   a buffer instead of a byte. This is the larger lever (~8×) and benefits both
   sexyz **and SyncTERM**. Bigger change than #1.
3. **[zmodem.c] Fix the 2 GB signed-position bug.** Widen `ack_file_pos`,
   `crc_request`, and the `pos` params of `zmodem_send_pos_header` /
   `zmodem_send_ack` to `uint32_t` (or `int64_t`), and audit
   `current_window_size` for unsigned correctness. Fixes both sexyz and
   SyncTERM windowed transfers >2 GB.
4. **[zmodem.c] Add an lrzsz-style adaptive block-length cost model** to replace
   the ×2/÷2 ramp. Largest throughput lever on lossy/variable links; benefits
   sexyz and SyncTERM.
5. **[sexyz.c] Investigate the `.`-prefixed received filename** (sexyz receiver
   wrote `.test2g.bin`) — minor, separate.

None of these lift the 4 GB wire ceiling, which is inherent to ZMODEM.
