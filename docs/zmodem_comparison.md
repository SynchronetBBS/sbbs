# ZMODEM implementation comparison: sexyz vs. lrzsz vs. Forsberg rzsz

**Date:** 2026-07-23
**Author:** Claude (analysis commissioned by Rob Swindell)
**Scope:** Performance, scalability (>2 GB / >4 GB), and robustness of
Synchronet's ZMODEM implementation, benchmarked against `lrzsz` 0.12.21rc and
Chuck Forsberg's final `rzsz` (3.73, 2003-01-30).

---

## 0. TL;DR

> **Numbers are steady-state (256 MB localhost send).** An earlier revision of
> this doc quoted 32 MB runs whose short duration was dominated by per-transfer
> startup, which deflated and distorted the rates; the values below supersede
> them. The *mechanism* findings (futex storm, ~84-byte writes, ring-per-byte)
> were unaffected — only the magnitudes changed.
>
> **Correction (per Deuce, SyncTERM's author):** three framings below are wrong
> and are retained only with this caveat.
> 1. **`ztx_buf` is NOT a faithful SyncTERM model, and "SyncTERM ≈85 MB/s" is
>    wrong.** Real SyncTERM is ~3× that; `ztx_buf` under-measures (Python-relay
>    harness overhead + poll-per-subpacket). SyncTERM's speed is a *deliberate
>    BDP / socket-buffer choice* (tuned to ~75% of a 1 Gb LAN; bump the buffer
>    for 10 Gb), not an emergent send-path fact. Read the ~85 figure as "a naive
>    buffered sender on `zmodem.c`, through this harness" — a floor, not SyncTERM.
> 2. **These are localhost CPU microbenchmarks, not the real-world regime.**
>    Below ~8 ms RTT the socket buffer / bandwidth-delay product dominates, not
>    the send loop. Real transfers live over the network; optimizing the send
>    loop for localhost is largely beside the point.
> 3. **The "recall stale bytes / abort-aware purge on ZRPOS" theory (bench
>    README) was a wrong garden path.** You can't recall bytes once they hit
>    socket/network buffers — lrz doesn't either, yet recovers. The batched-sender
>    error-recovery failures were *implementation bugs*, not something inherent to
>    batching.
>
> Still valid: `sexyz.c`'s ring-per-byte send *is* a real cap (consistent with
> SyncTERM, same `zmodem.c`, being multiples faster) — but it bites only on fast
> LANs; on typical/WAN links the network dominates.

- **Throughput:** the Synchronet **`sexyz` *sender* was ~18× slower than `lsz`**
  (≈11 vs ≈204 MB/s). This is **two stacked overheads**, isolated by linking the
  real `zmodem.c` behind each send architecture (§3.2):
  - **`sexyz.c` ring-buffer + per-byte writes (~7.5×, 85→11 MB/s): sexyz-only.**
    `send_byte` does one `RingBufWrite` per byte, so it and the drain
    `output_thread` ping-pong over tiny amounts — 2.3 M futex calls and ~84-byte
    fragmented `write()`s per 32 MB. **A batching prototype reached ~66 MB/s but
    was REVERTED — it regressed error recovery** (starves the back-channel during
    retransmit storms; §6). Still open. **SyncTERM never had this layer**
    (buffered single-threaded send).
  - **`zmodem.c` per-byte send design: SHARED — affects SyncTERM.** Its
    `send_byte`-callback-per-byte + ZDLE-escape + CRC pipeline is a real per-byte
    cost. A naive buffered sender on it measured ~85 MB/s *through this harness*
    (vs 204 for lrzsz's inlined path) — but treat that as a localhost floor, **not
    SyncTERM's throughput** (correction banner above; SyncTERM is BDP-tuned and
    faster). Worth reducing only if the target link outruns it (§6).
- **sexyz *receiver* is fine** (~130 MB/s), including past 2 GB.
- **Forsberg** (modern branch) *sender* runs at **~107 MB/s** (below lrzsz —
  1 KB block cap). Its *receiver* won't complete headlessly (serial-tty
  assumptions; segfaults under some transports).
- **>2 GB:** the shared `zmodem.c` narrowed several file positions to
  **signed `int32_t`**, corrupting the sender's transmit-window/ACK arithmetic
  above 2 GB (windowed sends hung at 2³¹). Because SyncTERM shares `zmodem.c`,
  it was affected too. **Fixed** (widened to `uint32_t`, GitLab #1196).
- **4 GB** is a hard ceiling for all three (the ZMODEM header position field is
  32-bit on the wire).
- **Adaptivity:** `lrzsz` continuously tunes its block length to the measured
  error rate (`calc_blklen`); `zmodem.c` (sexyz + SyncTERM) only does a blind
  ×2-up / ÷2-down ramp. Feature flags (`-8`, `-w`) are at parity.

Which component each finding lives in:

| Finding | `sexyz.c` | shared `zmodem.c` | SyncTERM `term.c` |
|---|:--:|:--:|:--:|
| Ring-buffer/per-byte send (85→11, futex/tiny-writes) — prototype **reverted**, open | ✗ (here) | — | OK (immune) |
| Per-byte send cost (204→85 harness floor, callback+escape+CRC) | — | ✗ (weakness) | ✗ (inherits the cost) |
| `int32_t` >2 GB window/ACK corruption — **fixed** | — | ✗ (was here) | ✗ (inherited) |
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

### 1.1 Versions under test — this is a living doc

Synchronet's ZMODEM is being actively improved, so every number here is tagged to
a component version. sexyz reports its version via `const char* revision`
(`sexyz.c:113`); zmodem.c via `zmodem_ver()`. The fixes in this doc ship as:

| Component | Baseline | Shipped |
|---|---|---|
| **sexyz.c** | **3.3** — `send_byte` writes the ring one byte at a time | **3.3** (unchanged) — the batching prototype was reverted (regressed error recovery, §6); throughput still open |
| **zmodem.c** | **rev 2.2** — window/ACK positions `int32_t` | **rev 2.3** — 2 GB fix (`uint32_t`, #1196). The `send_buf` prototype was reverted. |

- **SyncTERM:** its `term.c` send path is **unchanged** by this work; a SyncTERM
  throughput figure here is *modeled* by `ztx_buf` (the real `zmodem.o` behind a
  SyncTERM-style buffered `send_byte`), not measured from the SyncTERM binary.
- **Third-party:** lrzsz **0.12.21rc**; Forsberg rzsz **3.73** from the history
  repo's **`modern` branch**.

Git SHAs are omitted on purpose (the Synchronet commits are unpushed as of
writing); the `revision`/`zmodem_ver` strings above are the stable anchors, and
the git log maps them to commits once pushed. Throughput work is GitLab #1195,
the 2 GB fix #1196.

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

## 3. Throughput (256 MB steady-state, clean localhost, `-8` where supported)

| Sender → Receiver | Goodput | Note |
|---|--:|---|
| lsz → lrz (lrzsz baseline) | **203.9 MB/s** | 8 KB adaptive blocks |
| lsz → **sexyz** (sexyz *receives*) | **133.0 MB/s** | sexyz receiver OK |
| **Forsberg sz** → lrz | **106.6 MB/s** | 1 KB blocks (no ZedZap); single-threaded |
| buffered floor (`zmodem.c` + naive buffered send, **through this harness**) → lrz | **84.7 MB/s** | a localhost floor — **NOT SyncTERM** (correction banner) |
| **sexyz** → lrz — **shipped** (ring per-byte) | **11.4 MB/s** | the current sender |
| **sexyz** → lrz — batched prototype (**reverted**, §6) | **65.9 MB/s** | reverted: regressed error recovery |

Wire overhead is identical (~2.8 %, +2.55 % for Forsberg's 1 KB blocks) —
protocol efficiency is the same; the difference is purely implementation.

**Sender ranking (with the reverted prototype shown for reference):** lrzsz (204)
> Forsberg (107) > buffered-floor (85, harness) > sexyz-batched-prototype (66) >
sexyz-shipped (11). Two things stand out: (1) batching sexyz's send recovers most
of the deficit (11→66) but the prototype regressed error recovery and was
reverted (§6); (2) everything on `zmodem.c` (buffered-floor ~85, batched ~66)
sits below lrzsz because of the shared per-byte send cost — but ~85 is a localhost
harness figure, **not SyncTERM's real throughput** (which is BDP/socket-buffer
tuned; correction banner). Forsberg's ~107 with only 1 KB blocks shows how much a
clean single-threaded buffered send matters.

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
ring buffer, no futex. So it escapes the ring-per-byte collapse (the same layer
Fix A removed from sexyz). But it still rides on `zmodem.c`'s per-byte send
pipeline, which has its own cost (next section).

### 3.2 Splitting the two layers (real `zmodem.c`, two send paths)

To separate "how much is `zmodem.c`" from "how much is `sexyz.c`", a ~150-line
test sender (`ztx_buf.c`) was linked against the **real, unmodified
`zmodem.o`** — the same object sexyz uses — but with a **SyncTERM-style buffered
`send_byte`** (accumulate into an array, bulk `write()` per subpacket; no ring
buffer, no thread). This models the *shape* of SyncTERM's send path (buffered, no
ring/thread) but **not its throughput** — through this harness it under-measures
real SyncTERM by ~3× (harness overhead + poll-per-subpacket; correction banner).
Sending 256 MB to `lrz` (steady-state), identical wire bytes in every case:

| Sender | Send path (all drive the same `zmodem.c` except `lsz`) | Goodput |
|---|---|--:|
| `lsz` | lrzsz's own inlined `zm.c` | **204 MB/s** |
| `ztx_buf` = **buffered floor** (harness) | `zmodem.c` + **buffered** send | **~85 MB/s** |
| `sexyz` (batched prototype, **reverted**) | `zmodem.c` + **batched ring** + drain thread | **~66 MB/s** |
| `sexyz` (**shipped**) | `zmodem.c` + **ring per-byte** + drain thread | **~11 MB/s** |

`strace` of the shipped sexyz: **2.3 M futex** calls, 409 K writes of ~84 B.
`strace` of the batched prototype: **12 K futex**, 4 K writes of ~8 KB. The
buffered `ztx_buf` is **CPU-bound inside `zmodem.c`** (negligible syscall time),
on the `send_byte`-callback-per-byte + ZDLE-escape + CRC-32 pipeline — that
per-byte cost sets a ~85 MB/s floor *in this harness* (a floor, not SyncTERM's
real number), and block size barely moves it (per-byte, not per-block). The
batched prototype (~66) sat just under that floor but
**regressed error recovery** and was reverted (§6): under heavy injected errors
it starved the back-channel and stalled, where the shipped per-byte sender
recovers. A correct batched sender must keep servicing the back-channel while
sending.

**Conclusion:** the ~18× sexyz-vs-lrzsz gap is **~7.5× `sexyz.c`** (ring
per-byte, sexyz-only — batching prototype reverted, still open) **× ~2.4×
`zmodem.c`** (per-byte send design, shared with SyncTERM, ~85 MB/s *harness
floor*, open). That ~85 is a localhost harness figure, **not SyncTERM's real
throughput** (which is BDP/socket-buffer tuned and faster — correction banner).
Closing the residual gap to lrzsz needs the `zmodem.c` change: batch the
escape/CRC inner loop and hand `send_byte`
a span instead of a byte (§6, "Fix B").

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
- **Threading:** sexyz's `output_thread` drains the socket asynchronously while
  the protocol thread services the back-channel — this is what preserves error
  recovery on lossy links. It is also why the reverted batching prototype broke:
  batching a whole subpacket before one blocking flush left the sender stuck in
  the flush, unable to service the back-channel during retransmit storms. Both a
  single-threaded buffered sexyz *and* the threaded batching prototype hung/failed
  under injected errors; only the shipped per-byte-to-ring sender recovers (§6).

### 5.1 Empirical conditions matrix (shipped sexyz, sender-isolated → lrz)

> Rows below are the **shipped** (per-byte) sexyz at 32 MB / 8 MB — indicative of
> behavior under each condition, not steady-state throughput (see §3). This is
> the current sender; the batching prototype that would have raised the clean
> rate to ~66 MB/s was reverted for regressing error recovery (§6).

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

## 6. Recommendations & status

1. **✅ DONE (zmodem.c rev 2.3) — 2 GB signed-position bug.** Widened
   `ack_file_pos`, `crc_request`, and the `pos` params of
   `zmodem_send_pos_header` / `zmodem_send_ack` to `uint32_t`. Windowed >2 GB
   sends complete (validated to 2.36 GB); fixes sexyz and SyncTERM. GitLab #1196.
   This is the only shipped code fix; sexyz.c is otherwise unchanged (stays 3.3).

2. **⚠ OPEN — sender throughput (batching attempted, REVERTED).** Two batching
   prototypes were built and measured — Fix A (batch spans into the ring in
   `sexyz.c`, ~11→66 MB/s) and Fix B (a `send_buf` span callback in `zmodem.c`,
   ~66→88 MB/s). **Both regressed error recovery** under a heavy injected-error
   test and were reverted: original per-byte send passed 3/3 (~50 s), Fix A 0/3
   (timeout), Fix B 1/3 hard-fail. Root cause: batching a whole subpacket before
   a single blocking flush **starves the back-channel** during retransmit storms
   (the sender is stuck in the flush and can't service ZRPOS) → a bidirectional
   stall. The original's per-byte-to-ring keeps the pipeline draining, so it
   stays responsive. **A correct fix must service the back-channel while
   sending** (interleave, or flush in back-channel-yielding chunks) — lrzsz
   batches *and* recovers, so it's achievable, but it's a redesign, not the
   prototypes here. The measured ~66/~88 figures below are from those reverted
   prototypes, kept for reference. Patches saved out-of-tree. GitLab #1195.

3. **○ OPEN — per-byte `zmodem.c` send cost.** A buffered sender on `zmodem.c`
   measured ~85 MB/s *in this harness* (§3.2) — a real per-byte cost shared with
   SyncTERM, though ~85 is a localhost floor, not SyncTERM's real (BDP-tuned)
   throughput. Whether it's worth reducing depends on the target link; folds into
   the redesign above if so.

4. **○ OPEN — adaptive block-length cost model** (lrzsz-style `calc_blklen`) to
   replace the ×2/÷2 ramp. Largest lever on lossy/variable links; sexyz + SyncTERM.

5. **○ OPEN — the `.`-prefixed received filename** (sexyz receiver wrote
   `.test2g.bin`) — minor, separate.

None of these lift the 4 GB wire ceiling, which is inherent to ZMODEM.
