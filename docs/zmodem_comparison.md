# ZMODEM implementation comparison: sexyz vs. lrzsz vs. Forsberg rzsz

**Date:** 2026-07-23, re-measured 2026-07-24
**Author:** Claude (analysis commissioned by Rob Swindell)
**Scope:** Performance, scalability (>2 GB / >4 GB), and robustness of
Synchronet's ZMODEM implementation, benchmarked against `lrzsz` 0.12.21rc and
Chuck Forsberg's final `rzsz` (3.73, 2003-01-30).

---

## 0. TL;DR

> **All numbers are steady-state 256 MiB localhost sends, re-measured
> 2026-08-21** in one interleaved batch (three passes, spread ≤2%) so the tables
> are internally consistent. They supersede every earlier figure in this doc.
>
> **✅ The 115.8 MB/s "buffered floor" was one second of sleep per file.**
> Earlier revisions of this doc treated 115.8 first as `zmodem.c`'s CPU limit,
> then as an unexplained I/O stall in the measuring tool ("93 % of in-syscall
> wall time in polls; the precise stall mechanism has not been isolated"). It was
> neither. `zmodem_handle_zrpos()` purged the sender's receive buffer with a
> **1-second read timeout**, on the normal path: every file's first ZRPOS is just
> the routine answer to ZFILE, so with nothing to discard the purge could only
> establish that by letting the read time out. One dead second per file, at any
> line speed — and one of those polls was it. Fixed 2026-08-21; **every sender
> built on `zmodem.c` now runs level with lrzsz**:
>
> | Sender (256 MiB → `lrz`) | Goodput | Elapsed | CPU |
> |---|--:|--:|--:|
> | `lsz` (lrzsz baseline) | 203.8 MB/s | 1.32 s | 0.96 s |
> | **sexyz, fixed** | **203.9 MB/s** | 1.32 s | 1.02 s |
> | **`ztx_buf`, fixed** | **203.9 MB/s** | 1.32 s | 0.72 s |
> | sexyz, before the fix | 115.7 MB/s | 2.32 s | 1.03 s |
> | `ztx_buf`, before the fix | 115.8 MB/s | 2.32 s | 0.67 s |
>
> The arithmetic is exact: 1.32 s of work plus 1.00 s of sleep is 2.32 s, and
> 256 MiB over 2.32 s is 115.8 MB/s. Every buffered sender landed on the same
> "plateau" because they were all paying the same fixed second. The companion
> observation — "30 % less CPU than lrzsz yet idle 70 % of the wall clock" —
> resolves the same way: the CPU figures were right, and a full second of the
> idleness was this sleep.
>
> **Why the benchmark could not see it.** The harness sends **one file per run**
> (`--file` is singular), so a per-*file* cost appears once instead of scaling,
> in a metric built to divide fixed costs away. The rate-capped rows refunded it
> outright — the relay's token bucket banks credit up to `rate_bps`, exactly one
> second's worth, so the sender burst through the deficit and read as link-bound.
> The diagnostics in §3.1 are `rusage` CPU and context switches, which cannot see
> a sleeping `poll()` by construction. And when the sender was still the 11.5 MB/s
> per-byte one, a second inside a 23-second run sat behind an 18× gap.
>
> **Found by Uwe Ohse (lrzsz maintainer)**, testing sexyz against a new lrzsz
> release: 5.9 KB/s where lrzsz and zmtx/zmrx managed 10.2 KB/s over a simulated
> 115200 bps line, sending five small files — a workload where the per-file cost
> *is* the workload. He supplied the protocol dump and the strace that name the
> `poll(fd, 1000)`.
>
> **Caveats that still stand (per Deuce, SyncTERM's author):**
> 1. **`ztx_buf` is not a SyncTERM model** — and, per the above, not an engine
>    model either. SyncTERM's speed is a *deliberate BDP / socket-buffer choice*
>    (tuned to ~75 % of a 1 Gb LAN), not an emergent send-path fact. Never quote
>    a `ztx_buf` number as SyncTERM's, or as `zmodem.c`'s ceiling.
> 2. **These are localhost CPU microbenchmarks, not the real-world regime.**
>    Below ~8 ms RTT the socket buffer / bandwidth-delay product dominates, not
>    the send loop. On typical/WAN links the network dominates and none of this
>    matters; it bites on fast LANs.
> 3. **You cannot recall bytes once they hit socket/network buffers** — lrz
>    doesn't either, yet recovers. An early "abort-aware purge on ZRPOS" theory
>    in the bench README was a wrong garden path and has been retracted.

> **RESOLVED (zmodem.c 2.5):** a receiver's opening ZRINIT no longer
> retransmits the ZFILE. The receiver announces itself unprompted, the sender
> sends a ZRQINIT of its own anyway, and that second ZRINIT arrived after the
> ZFILE had gone out — the loop waiting for the ZFILE's answer accepted only
> ZACK, so it resent the whole frame. Both copies were answered, leaving a stale
> second ZRPOS on the back-channel that the sender read mid-stream and took for a
> retransmit request, restarting the file from zero. The 1-second purge above had
> been swallowing that ZRPOS, so removing the purge exposed it: with sender and
> receiver pinned to one CPU it restarted in 2 runs of 3. Now absorbed once per
> ZFILE attempt, as lrzsz's `zsendfile()` does in the same spot (0 of 12 after).
>
> **RESOLVED (sexyz.c 3.4):** the sexyz-specific gap below is fixed. The
> streaming send path now buffers the producer (accumulate, hand whole spans to
> the ring) instead of writing the ring one byte at a time, taking a 256 MB
> localhost send from 11.5 MB/s / 44 CPU-s to **~115 MB/s / 0.85 CPU-s**, error
> gate 5/5. It keeps the two-thread architecture — the threads were never the
> problem, per-byte *feeding* was — so it is a small, low-risk change, not the
> single-threaded rewrite this doc earlier recommended. Buffering is streaming
> only; `-w`/`-s` keep the byte-at-a-time path (they need acks flowing back as
> data is sent). The analysis below is the original investigation, kept for the
> record; §3.2/§3.3/§6 note where its "open" framing has since shipped.

- **Throughput:** the **`sexyz` sender was 11.5 MB/s vs `lsz`'s 203.9** (before
  the 3.4 fix above) — ~18×. Two stacked overheads, isolated by linking the real
  `zmodem.c` behind each send architecture (§3.2):
  - **`sexyz.c` ring-buffer + per-byte writes (115.8→11.5 MB/s as measured;
    ~17× once the fixed second of §0 is taken out of both figures): sexyz-only,
    and it is the whole sexyz-specific gap.** `send_byte` takes the
    ring mutex **twice per byte** (`RingBufFree` then `RingBufWrite`) while the
    drain `output_thread` hot-loops on the same mutex: **1 futex call per 17
    bytes** (37% of them contended) and **~94-byte** socket writes. It costs
    **44 CPU-seconds per 256 MB against `lsz`'s 0.95** — 46×. Buffering the
    producer removes *all* of it (a prototype reached 115.8 MB/s at 1.0
    CPU-second, i.e. the same I/O-stalled plateau every buffered sender hits)
    but breaks error recovery; see §3.3 and §6. **SyncTERM never had this
    layer** (buffered single-threaded send).
  - **`zmodem.c` per-byte send design: SHARED — affects SyncTERM, and it is no
    longer the limiting factor.** **Deuce reworked it on 2026-07-24** (class-table
    byte classifier, slicing-by-4 CRC-32, hoisted escape mask with `noinline`
    cold paths, buffered `fcrc32()`), moving the measured buffered path
    **91.9 → 115.8 MB/s**. Both of those figures carried the fixed second (§0);
    with it removed the buffered path measures **203.9**, level with lrzsz, on
    **less CPU per byte** (0.72 s vs 0.96 s per 256 MiB). Further engine
    micro-optimization has no goodput left to win in this regime.
- **sexyz *receiver* is fine** (121.0 MB/s, unchanged by these fixes — the
  receive path never calls the sender code), including past 2 GB. Every defect
  in this doc is on the sending side.
- **Forsberg** (modern branch) *sender* runs at **96.9 MB/s** (below lrzsz —
  1 KB block cap). Its *receiver* won't complete headlessly (serial-tty
  assumptions; segfaults under some transports).
- **>2 GB:** the shared `zmodem.c` narrowed several file positions to
  **signed `int32_t`**, corrupting the sender's transmit-window/ACK arithmetic
  above 2 GB (windowed sends hung at 2³¹). Only the windowed path triggers it,
  and **SyncTERM never sets a transmit window**, so in practice only sexyz could
  hit it. **Fixed** in shared `zmodem.c` (widened to `uint32_t`, GitLab #1196).
- **4 GB** is a hard ceiling for all three (the ZMODEM header position field is
  32-bit on the wire).
- **Adaptivity:** `lrzsz` continuously tunes its block length to the measured
  error rate (`calc_blklen`); `zmodem.c` (sexyz + SyncTERM) only does a blind
  ×2-up / ÷2-down ramp. Feature flags (`-8`, `-w`) are at parity.
- **Transmit window (`-w`):** two defects, both **fixed** in sexyz. (1) A
  window smaller than 4× the block size hit a divide-by-zero (SIGFPE) at the
  first data subpacket — the quarter-window ACK interval `window/block/4`
  reached 0 (GitLab #1197, `zmodem.c` rev 2.4). (2) A window *at or below* the
  block size then stalled to ~1 window/second: only one block can be unacked at
  a time, and the window-full wait misses the opening ACK. `lsz`/`sz` forbid
  this by construction — `-w` forces the block down to window/4 — so sexyz now
  does the same (block clamped to window/4 when `-w` is set; sexyz.c 3.4). Both
  are sexyz-reachable only; **SyncTERM never sets a transmit window**, so it hit
  neither. Windowed *throughput* remains far below lrzsz — see below.

Which component each finding lives in:

| Finding | `sexyz.c` | shared `zmodem.c` | SyncTERM `term.c` |
|---|:--:|:--:|:--:|
| 1-second ZRPOS purge before every file's data (115.8→203.9) — **FIXED** (zmodem.c 2.5) | — | ✗ (was here) | ✗ (inherited it; inherits the fix) |
| ZFILE retransmitted at the receiver's opening ZRINIT, leaving a stale ZRPOS — **FIXED** (zmodem.c 2.5) | — | ✗ (was here) | ✗ (inherited it; inherits the fix) |
| Ring-buffer/per-byte send (was 115.8→11.5, futex/tiny-writes) — **FIXED** (sexyz.c 3.4: buffered streaming producer; ~204 MB/s once the purge above is also fixed, gate 5/5) | ✗ (was here) | — | OK (immune) |
| Per-byte send cost (callback+escape+CRC) — **addressed** 2026-07-24; engine now cheaper per byte than lrzsz | — | was a weakness | inherits the fix |
| `-w` window ≤ block stall / window < 4×block SIGFPE — **fixed** (#1197) | ✗ (clamp, 3.4) | ✗ (divzero guard, 2.4) | OK (no `-w`) |
| `int32_t` >2 GB window/ACK corruption — **fixed** (#1196) | — | ✗ (was here) | OK (no `-w`) |
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
| **sexyz.c** | **3.3** — `send_byte` writes the ring one byte at a time; `-w` sets the window without touching the block size | **3.4** — buffered streaming send path (~11 → ~115 MB/s, #1195); `-w` clamps the block to window/4 like `lsz`/`sz` so `window ≤ block` no longer stalls (#1197). **3.5** — no change of its own: the startup banner prints only sexyz's own revision, so it moves with `zmodem.c` 2.5 to keep a build identifiable without `sexyz v` |
| **zmodem.c** | **rev 2.2** — window/ACK positions `int32_t`; switch-based byte classifier; byte-at-a-time CRC-32; quarter-window ACK interval divides by zero when window < 4×block | **rev 2.5** — 2 GB fix (`uint32_t`, #1196); Deuce's 2026-07-24 send-path work (class-table classifier, slicing-by-4 CRC-32, hoisted escape mask + `noinline` cold paths, buffered `fcrc32()`); window-interval divide-by-zero guarded (#1197); the 1-second ZRPOS purge off the normal send path and the ZFILE retransmit at the receiver's opening ZRINIT, both 2026-08-21 |

- **SyncTERM:** its `term.c` send path is **unchanged** by this work; a SyncTERM
  throughput figure here is *modeled* by `ztx_buf` (the real `zmodem.o` behind a
  SyncTERM-style buffered `send_byte`), not measured from the SyncTERM binary.
- **Third-party:** lrzsz **0.12.21rc**; Forsberg rzsz **3.73** from the history
  repo's **`modern` branch**.

The `revision`/`zmodem_ver` strings above are the stable anchors; the git log
maps them to commits. Throughput work is GitLab #1195, the 2 GB fix #1196.

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
- **One file per run.** `--file` is singular: the harness measures a single
  large steady-state transfer. **A per-file cost is therefore invisible here** —
  it lands once, in a goodput figure designed to divide fixed costs away, and the
  rate-capped rows refund it entirely from banked tokens. That blind spot hid a
  flat one-second-per-file stall for months (§0). For anything suspected of
  costing per *file* rather than per byte, send a batch of small files and time
  the batch.
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

Rows marked 2026-08-21 are from one interleaved batch that day, three passes
(spread ≤2%; median shown); the rest are the 2026-07-24 batch, kept where the
component no longer exists to re-measure. Every run verified byte-identical.

| Sender → Receiver | Goodput | Note |
|---|--:|---|
| **sexyz** → lrz — `zmodem.c` 2.5, sexyz.c 3.4 | **203.9 MB/s** | 2026-08-21; level with lrzsz |
| lsz → lrz (lrzsz baseline) | **203.8 MB/s** | 2026-08-21; 8 KB adaptive blocks, fully inlined escape/CRC |
| `ztx_buf` (`zmodem.c` **rev 2.5** + buffered send) → lrz | **203.9 MB/s** | 2026-08-21 |
| lsz → **sexyz** (sexyz *receives*) | **121.0 MB/s** | 2026-08-21; receive path untouched by these fixes |
| `ztx_buf` (`zmodem.c` **rev 2.4**) → lrz | **115.8 MB/s** | the old "buffered floor": 1.32 s of work + 1.00 s of sleep (§0) |
| **sexyz** → lrz — before the purge fix | **115.7 MB/s** | 2026-08-21; same second, same arithmetic |
| **Forsberg sz** → lrz | **96.9 MB/s** | 2026-07-24; 1 KB blocks (no ZedZap); single-threaded |
| `ztx_buf` (`zmodem.c` **rev 2.2**, pre-Deuce) → lrz | **91.9 MB/s** | 2026-07-24; what Deuce's work improved on |
| **sexyz** → lrz — sexyz.c **3.3** (ring per-byte) | **11.5 MB/s** | 2026-07-24; superseded by 3.4 |

Wire overhead is identical (~2.8 %, +2.55 % for Forsberg's 1 KB blocks) —
protocol efficiency is the same; the difference is purely implementation.

Three things stand out:

1. **Every sender driving `zmodem.c` now measures 203.9, the same as lrzsz's own
   inlined `zm.c`** — sexyz, and `ztx_buf`'s buffered single-threaded path alike.
   The three-way tie is the strongest evidence that nothing about the engine's
   framing, escaping, or CRC is costing throughput in this regime.
2. **The 115.8 that both buffered paths used to report was the fixed second**,
   not a plateau they shared for structural reasons (§0). Any older figure in
   this doc measured against a `zmodem.c`-based sender carries it.
3. **Forsberg reaches 96.9 with only 1 KB subpackets** — a clean single-threaded
   buffered send beats a 8×-larger block size behind a bad transport by ~8×.

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

### 3.1 Root cause of the sender collapse (measured 2026-07-24)

**CPU cost of one 256 MB send** (`wait4` rusage of the sender process):

| Sender | user | sys | CPU | voluntary ctx switches |
|---|--:|--:|--:|--:|
| **lsz** | 0.91 s | 0.04 s | 74 % | **1,402** |
| **sexyz** | 21.47 s | **22.55 s** | **185 %** | **1,464,130** |

**44 CPU-seconds against 0.95 — a 46× difference**, and sexyz pegs *both*
threads (185 %). Half of it is system time, and the context-switch count is
three orders of magnitude out.

**Syscall profile** (32 MB send, under `strace -c -f`):

| Sender | `write()` calls | avg write | `futex` calls |
|---|--:|--:|--:|
| **lsz** | 12,302 | ~2,803 B | 0 (single-threaded) |
| **sexyz** | 357,935 | **~94 B** | **1,959,794** (737,140 contended, 80 % of syscall time) |

That is **one futex call per 17 bytes transferred**.

**Mechanism.** `sexyz.c`'s `send_byte()` takes the ring mutex **twice for every
byte** — once in `RingBufFree()` to check for space, once in `RingBufWrite()` —
while `output_thread` hot-loops on `RingBufFull()`/`RingBufRead()` against the
same mutex. The consumer drains faster than the producer fills, so it empties
the ring, sleeps, and is woken again a few dozen bytes later. Every wake is a
futex round-trip and a ~94-byte `write()`.

**This is not a tuning problem.** Every relevant knob was swept via `sexyz.ini`
(no code changes) on the same 256 MB send:

| `OutbufSize` | `OutbufHighwaterMark` | `OutbufDrainTimeout` | Goodput |
|--:|--:|--:|--:|
| 16 K (default) | 1100 (default) | 10 ms (default) | 11.6 MB/s |
| 64 K | 1100 | 10 ms | 10.9 MB/s |
| 64 K | 0 (off) | 10 ms | 11.5 MB/s |
| 16 K | 0 (off) | 10 ms | 11.0 MB/s |
| 64 K | 0 (off) | 0 ms | 11.2 MB/s |
| 64 K | 8 K | 100 ms | 11.4 MB/s |
| 64 K | 32 K | 100 ms | 11.3 MB/s |
| 64 K | 60 K | 250 ms | 11.3 MB/s |

Flat inside noise, and CPU stayed at 187 % throughout. Ring capacity and
consumer hysteresis are irrelevant because the cost is per-byte on the producer
side, before the ring's fill level ever matters. (Flow-control stalls were also
ruled out: only 10–19 `FLOW` events occur in a whole 256 MB transfer.)

sexyz's three possible send architectures (the third is the fix — see §6):

| sexyz `send_byte` config | threads | batching | per-32 MB kernel cost |
|---|---|---|---|
| `SINGLE_THREADED FALSE` (built default) | 2 (ring + drain) | fragmented ~94 B writes | 2.0 M `futex` |
| `SINGLE_THREADED TRUE` (`sexyz.c:84`, source-edit only) | 1 | **none** — 1 byte per `write()` | ~34 M `write()` |
| buffered (SyncTERM `term.c` / `ztx_buf`) | 1 | **flush per subpacket** | ~8 K `write()`, 0 futex |

The single-threaded build is a portability/debug fallback, **not** a faster
mode: it drops the thread but keeps sending one byte per `write()` syscall.

**SyncTERM does not have *this* layer.** Its `send_byte` (`term.c:874`)
accumulates into an 8 KB `transfer_buffer` and `flush_send`→`conn_send` writes it
in bulk (one send per subpacket, driven by `zmodem_flush`) — single-threaded, no
ring buffer, no futex. So it escapes the ring-per-byte collapse entirely. But it
still rides on `zmodem.c`'s per-byte send pipeline, which has its own cost (next
section) — and therefore also inherits Deuce's 2026-07-24 improvement to it.

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
| `lsz` | lrzsz's own inlined `zm.c` | **203.9 MB/s** |
| `ztx_buf`, `zmodem.c` rev 2.5 | `zmodem.c` + **buffered** send | **203.9 MB/s** |
| `ztx_buf` = the old **buffered floor**, `zmodem.c` rev 2.3/2.4 | same, carrying the 1-second purge | **115.8 MB/s** |
| `ztx_buf` = buffered floor, `zmodem.c` rev 2.2 | pre-Deuce engine, same transport | **91.9 MB/s** |
| `sexyz` (**shipped**) | `zmodem.c` + **ring per-byte** + drain thread | **11.5 MB/s** |

The buffered `ztx_buf` is **CPU-bound inside `zmodem.c`** (negligible syscall
time), on the `send_byte`-callback-per-byte + ZDLE-escape + CRC-32 pipeline.
Block size barely moves it (the cost is per-byte, not per-block).

**Conclusion:** the sexyz-vs-lrzsz gap was two stacked costs, both now removed.
The `sexyz.c` ring-per-byte layer cost 10× and was sexyz-only (fixed in 3.4).
What was left after removing it was **not** an engine limit and not the "shared
I/O shape" this section previously blamed: every buffered sender landed on
~115.8 MB/s at 30–36 % CPU because each was sleeping a flat second before its
data (§0). With that gone, `ztx_buf` and sexyz both measure **203.9**, level
with `lsz`, on less CPU per byte (0.72 s vs 0.96 s per 256 MiB). The question
this section used to leave open — what a properly non-blocking sender could
reach — is no longer the interesting one at these rates: the engine is already
at lrzsz's goodput with CPU to spare.

### 3.3 Why the fix looked hard: eight prototypes, one gate

Batching the producer is trivially fast and repeatedly wrong. Note that every
"115.8" below carried the fixed second since diagnosed (§0), so every prototype
in the table was really running at ~204 MB/s for 1.32 s and then sleeping; their
clean-link column is not a measure of how fast each one was. The
gate is the error-injection test — 8 MB at `--corrupt-rate 0.000003`, **run three times,
all three must complete with a matching SHA-256** (a single pass proves nothing;
the model is chaotic, because a retransmit shifts where later corruption lands).
The shipped per-byte sender passes **3/3**.

| Prototype | Clean | Sender CPU / 256 MB | Error gate |
|---|--:|--:|:--:|
| **shipped** — per-byte into the ring | 11.5 MB/s | 44 s | **3/3** |
| batch 512 B, output thread kept | 115.8 MB/s | 1.0 s | 1/3 |
| batch 1 KB, output thread kept | — | — | 1/3 |
| batch 2 KB, output thread kept | — | — | 1/3 |
| batch 4 KB, output thread kept | 115.7 MB/s | 1.0 s | 0/3 |
| single-threaded + batch 4 KB, **blocking** writes | 115.8 MB/s | 0.83 s | **2/3** |
| batch 4 KB + a `flush()` that really drains the ring to the wire (§3.4) | 115.8 MB/s | 1.11 s | 1/3 |

Two results are worth keeping:

- **It is not a speed artifact.** The obvious objection — "the batched sender is
  10× faster, so it simply has more data in flight when corruption hits" — was
  tested directly by capping the batched sender to 11 MB/s, the shipped sender's
  own natural rate. At the identical rate, **the shipped sender passed 3/3 and
  the batched sender still failed**. Batching genuinely breaks recovery.
- **The output thread is most of the problem, but not all of it.** Dropping to a
  single thread with a buffered `send_byte` reached the same 115.8 MB/s, cut
  voluntary context switches from 1,464,130 to **1,585** (`lsz` does 1,402), and
  lifted the gate from 0–1/3 to **2/3** — the best result so far.
- **A real flush-to-wire is not the answer either.** `sexyz`'s `flush()`
  callback is a **no-op for the socket path** — it only `fflush(stdout)`s, and
  only in stdio mode — so `zmodem_flush()` returns with the bytes still sitting
  in the ring. `lsz` does a genuine `fflush(stdout)` immediately before draining
  the back-channel (§3.4), which looked like the missing invariant. Implementing
  it (wait for the ring to empty inside `flush()`) still failed at 1/3. Worth
  fixing on its own merits; not the cause.

### 3.3.1 The mechanism, found: queue depth, not batching

Capturing the wire (`zbench_sock.py --tap`, decoded with `zdecode.py`) shows the
difference immediately. Same 2 MB file, same `--corrupt-rate 0.00001`, decoding
the **receiver→sender** channel:

```
shipped (per-byte)            batched prototype
  ZRPOS  pos=154624             ZRPOS  pos=154624
  ZACK   pos=158720             ZRPOS  pos=154624     <-- same position
  ZRPOS  pos=166912             ZRPOS  pos=154624
  ZACK   pos=168960             ZRPOS  pos=154624
                                ZRPOS  pos=154624
                                ZRPOS  pos=154624
                                ZACK   pos=158720
```

The shipped sender needs **one** ZRPOS per error. The batched sender makes the
receiver repeat the *same* ZRPOS **2–8 times** before it can resynchronise,
because the sender keeps feeding it queued stale data from beyond the reposition
point. Wire overhead on that run: **+7.3 % shipped vs +216 % batched**.

So the variable is **how many bytes are queued ahead of the sender when the
ZRPOS is generated** — ring plus socket buffers — not batching itself. A
per-byte producer at 11 MB/s cannot outrun the drain, so its queue stays nearly
empty; a batched producer at 115 MB/s keeps it full.

**Proof, by bounding the queue.** With no code change beyond the batching
itself:

| Batched prototype | Error gate | Wire overhead |
|---|:--:|--:|
| default (16 K ring, default socket buffers) | 0/3 | −63 % … −83 % (aborted) |
| `OutbufSize=1024` (ini knob only) | **2/3** | — |
| `OutbufSize=1024` + 8 KB socket buffers (`--sockbuf`) | **3/3** | **+10.2 … +10.7 %** |
| *control:* shipped sender, same bounded config | 3/3 | — |

And it is **free**: shrinking the ring from 16 K to 1 K costs no throughput at
all (115.80 vs 115.79 MB/s) because the batching already removed the per-byte
cost.

This lands exactly on Deuce's socket-buffer/BDP point from a different
direction: on a real link the right bound on in-flight bytes *is* the
bandwidth-delay product, and error-recovery cost scales with how far past the
reposition point the sender has already run.

**Correction to §3.3:** the earlier claim that this "is not a speed artifact"
was too strong. That control used `--rate-bps`, which throttles the *relay*, not
the sender's backlog — the sender still filled ring and socket buffers at full
speed. It never tested the queue-depth hypothesis.

**One thing that does *not* substitute:** ZMODEM's own transmit window (`-w`).
Batched + `-w32768` timed out 3/3 and `-w8192` transferred essentially nothing
(−99.96 % overhead) — the batched prototype deadlocks under windowed mode, a
separate defect in it, not in `-w`.

**Still open:** the remaining prototypes' 2/3-vs-3/3 difference tracks socket
buffer size, which is a harness-side knob here. A shipped fix needs sexyz to
bound its own in-flight bytes (ring size plus `SO_SNDBUF`) rather than relying
on the environment.

Earlier failed variants are catalogued in `src/bench/zmodem/README.md`; two of
their failure modes are now understood and should not be repeated — a `void`
flush callback that silently swallowed a failed write, and a sticky error latch
that turned one transient full-buffer timeout into an infinite
`zmodem_send_raw ERROR: -1` spin. A correct implementation must treat a failed
flush as **transient**, keep the unwritten remainder buffered, and let
`zmodem.c`'s own retry/abort logic run.

### 3.4 What `lsz` actually does (verified, and it is not what this doc said)

An earlier revision claimed the missing ingredient was "non-blocking output with
`select()` on both directions, the way `lrz` does it". **That is wrong** —
`lrzsz` uses plain *blocking* stdio. Reading the source settles it:

- Output is `putchar`/`fwrite` into `stdout` (`zm.c:109-110`,
  `#define sendline(c) putchar((c) & 0377)`). There is **no** `O_NONBLOCK`,
  `fcntl` or `FIONBIO` anywhere in `lsz.c` or `zm.c`.
- `flushmo()` is literally `fflush(stdout)` (`zglobal.h:411`).
- The only `select()` in the sender (`lsz.c:754`) is in the pre-handshake purge
  that drains stale input before ZRQINIT — **not** in the data path.
- Escaping uses a **lookup table plus span writes** (`zm.c:285-320`): scan
  forward with `zsendline_tab[]` for a run needing no escape, `fwrite()` the
  whole run, and drop to per-byte only for the escape itself. That is both the
  class-table idea Deuce implemented in 2026-07-24 and the "hand `send_byte` a
  span" idea — `lrzsz` has had both since the 1990s.

Its per-subpacket loop (`lsz.c:2093-2120`) is:

```c
ZSDATA (DATAADR, n, e);            /* send the subpacket             */
fflush (stdout);                   /* 1. force it onto the wire      */
while (rdchk (io_mode_fd)) {       /* 2. drain back-channel to empty */
    switch (READLINE_PF (1)) {
    case CAN: case ZPAD:
        c = getinsync (zi, 1);     /*    handle ZRPOS / ZACK         */
```

So the property is "everything is on the wire before you look for a reply",
with blocking I/O throughout — not non-blocking sockets. `zmodem.c` already has
the matching drain loop (`while (is_data_waiting(...))`, `zmodem.c:1684`) and
already calls `zmodem_flush()` per subpacket (`zmodem.c:663`); what sexyz lacks
is a `flush()` that means anything. Supplying one did not fix the gate (§3.3),
so this is a real difference but not the decisive one.

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
a windowed (`-w`) send past 2 GB throttles/stalls. The narrowed field is in
shared `zmodem.c`, but the fault only fires on the windowed path; **SyncTERM
never sets a transmit window, so its uploads never reached it.** Fixed for
everyone in rev 2.3 regardless (#1196).

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
| **sexyz** → lrz, **`-w1M` (windowed)** | **HANG at 2³¹** *(before #1196; now MATCH to 2.36 GB)* | — |

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
- sexyz **sender *with* a window** (`-w`) hung at 2 GB. **Fixed** in `zmodem.c`
  rev 2.3 (positions widened to `uint32_t`, #1196); re-tested to 2.36 GB → MATCH.
  The bug lived in shared `zmodem.c`, but only the windowed code path triggers
  it, and **SyncTERM never sets a transmit window** (`max_window_size` stays 0 —
  there is no UI or code path to enable one), so SyncTERM could not reach this
  hang in practice. Fixing it in the shared engine was still correct.
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
> behavior under each condition, not steady-state throughput (see §3). They date
> from the original 2026-07-23 run and were not re-measured; the *relative*
> behavior under each condition is what they are for. Both the buffered sender
> (sexyz.c 3.4) and the purge fix (zmodem.c 2.5) have since shipped, so every
> clean-link row here understates the current sender by a wide margin — the
> 32 MB rows especially, which are mostly the fixed second (see the row note).

| Condition | lrzsz | sexyz | Forsberg | Takeaway |
|---|--:|--:|--:|---|
| Clean, `-2`/`-4`/`-8` (32 MB) | 156 / 156 / **204** | 8.4 / 8.2 / 8.7 | ~100 (1 KB only) | sexyz block-insensitive; lrzsz gains from `-8`. The sexyz column is dominated by the fixed second (§0): 32 MB at its then-real 11.5 MB/s is 2.78 s, plus 1.00 s reads as 8.5 MB/s, which is why the same sender measured 11.5 on 256 MB and ~8.4 here |
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

**Practical implication (as of the 3.3 sender):** sexyz's *throughput* deficit
was invisible on links slower than ~8 MB/s — everyone is link-bound there — and
bit only on LAN / localhost / fast-internet transfers. The fixed second was the
opposite: being per-file and not per-byte, it was **worst on slow links and small
files**, where it could exceed the transfer itself. Uwe Ohse's report was
5 files totalling 30 KB at 115200 bps, where 5 of the 5.2 seconds were sleep.
A single-large-file benchmark cannot distinguish those two shapes; §2 now says
so.

---

## 6. Recommendations & status

0. **✅ DONE (zmodem.c 2.5) — the one-second-per-file stall, and the ZFILE
   retransmit behind it.** `zmodem_handle_zrpos()` purged the receive buffer with
   a 1-second timeout on the normal send path, costing a flat second per file at
   any line speed (§0). The purge timeout is now passed in by the caller: 0 for
   the initial ZRPOS, 1 for the mid-transfer ZRPOS that follows an error, where
   waiting briefly for the rest of the receiver's retransmit request is the point
   of purging at all. Removing the wait exposed what it had been hiding: the
   sender retransmitted its ZFILE whenever the receiver's opening ZRINIT arrived
   after it, and the receiver answered both copies, leaving a stale ZRPOS that
   restarted the file mid-stream. One such ZRINIT is now absorbed per ZFILE
   attempt, as lrzsz's `zsendfile()` does. Together: 115.7 → 203.9 MB/s on a
   256 MiB send, level with lrzsz, and four small files that took 4.01 s take
   0.006 s. Reported by Uwe Ohse.

1. **✅ DONE (zmodem.c rev 2.3) — 2 GB signed-position bug.** Widened
   `ack_file_pos`, `crc_request`, and the `pos` params of
   `zmodem_send_pos_header` / `zmodem_send_ack` to `uint32_t`. Windowed >2 GB
   sends complete (validated to 2.36 GB); fixes sexyz and SyncTERM. GitLab #1196.
   This is the only shipped code fix; sexyz.c is otherwise unchanged (stays 3.3).

2. **✅ DONE (sexyz.c 3.4) — sender throughput.** Shipped as a buffered streaming producer that keeps the two-thread architecture (the threads were never the bottleneck — per-byte *feeding* of the ring was): ~11 → ~115 MB/s, 44 → 0.85 CPU-s, error gate 5/5, `-w`/`-s`/receive unchanged. The single-threaded rewrite described below was **not** needed. Original analysis retained:
   Six prototypes now (§3.3) confirm the shape of the answer:
   - The entire sexyz-specific penalty is the **per-byte ring traffic**, and
     buffering the producer removes **all** of it — a prototype reached
     115.8 MB/s at 1/44th the CPU, and no amount of ring/highwater/drain tuning
     touches it (§3.1). 115.8 was *not* the endpoint, for a reason not yet known
     when this was written: it carried the fixed second, and the same code now
     measures 203.9 (§0).
   - Batching **while keeping `output_thread`** fails the error gate at every
     buffer size tried (512 B – 4 KB, 0–1 of 3), and it is *not* a
     rate artifact — rate-capped to the shipped sender's own 11 MB/s it still
     failed while the shipped sender passed 3/3.
   - Going **single-threaded with a buffered `send_byte`** kept the full speed,
     brought context switches down to `lsz`'s level, and got the gate to 2/3.
     The remaining failure is the blocking `sendbuf()`.

   **The mechanism is now identified (§3.3.1): queue depth, not batching.** A
   batched producer keeps the ring and socket buffers full, so after a ZRPOS the
   receiver has to chew through everything already queued and repeats the same
   ZRPOS 2–8 times before it can resynchronise. Bounding the in-flight bytes
   (1 KB ring + 8 KB socket buffers) takes the batched prototype from 0/3 to
   **3/3** with near-shipped wire overhead, and costs **zero** throughput.

   So the fix is: **buffer the producer, and bound the sender's in-flight
   backlog** — ring size plus `SO_SNDBUF`, sized to the link's bandwidth-delay
   product rather than left to the environment. Prefer the **single-threaded**
   form of the buffered sender: same 115.8 MB/s, but 0.83 CPU-s and 1,585
   context switches versus 1.0 s and 63,047, and it was the best-behaved variant
   before the queue bound was found. Note that ZMODEM's own transmit window
   (`-w`) does *not* substitute — the batched prototype deadlocks under it.

   Implementation notes that cost time to learn: a failed flush is **transient**
   (keep the remainder buffered, never latch an error, never drop bytes), the
   flush must happen before any wait on the back-channel (`recv_buffer()` is the
   single choke point in `sexyz.c`), and `flush()` itself is currently a no-op on
   the socket path (§3.4) and should be made real regardless. GitLab #1195.

3. **✅ DONE (and no longer the lever) — per-byte `zmodem.c` send cost.** Deuce
   reworked the shared send path on 2026-07-24 (class-table byte classifier,
   slicing-by-4 CRC-32, hoisted escape mask with `noinline` cold paths, buffered
   `fcrc32()`), inherited by SyncTERM as well as sexyz. **The engine now costs
   less CPU per byte than lrzsz** (0.69 s vs 0.98 s per 256 MB), so handing
   `send_byte` a span instead of a byte would reduce CPU that is not being
   spent. The 115.8-vs-203.9 goodput gap this item used to warn about is gone:
   it was the fixed second (§0), and the engine now measures 203.9. Also note these per-byte
   CPU wins are **invisible to a single-stream localhost benchmark above
   ~100 MB/s**: Deuce measured his with six interleaved 1 GiB transfers, and two
   of his four commits measured exactly zero in this harness.

4. **○ OPEN — adaptive block-length cost model** (lrzsz-style `calc_blklen`) to
   replace the ×2/÷2 ramp. Largest lever on lossy/variable links; sexyz + SyncTERM.

5. **○ OPEN — the `.`-prefixed received filename** (sexyz receiver wrote
   `.test2g.bin`) — minor, separate.

None of these lift the 4 GB wire ceiling, which is inherent to ZMODEM.
