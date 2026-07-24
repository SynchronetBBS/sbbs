# ZMODEM implementation comparison: sexyz vs. lrzsz vs. Forsberg rzsz

**Date:** 2026-07-23, re-measured 2026-07-24
**Author:** Claude (analysis commissioned by Rob Swindell)
**Scope:** Performance, scalability (>2 GB / >4 GB), and robustness of
Synchronet's ZMODEM implementation, benchmarked against `lrzsz` 0.12.21rc and
Chuck Forsberg's final `rzsz` (3.73, 2003-01-30).

---

## 0. TL;DR

> **All numbers are steady-state 256 MB localhost sends, re-measured
> 2026-07-24** in one interleaved batch (three passes, spread ≤2%) so the tables
> are internally consistent. They supersede every earlier figure in this doc.
>
> **⚠ The 115.8 MB/s "buffered floor" is NOT an engine ceiling.** An earlier
> revision of this doc (and of §3.2/§3.3/§6) treated it as `zmodem.c`'s CPU
> limit. It is not. Measuring the senders' actual CPU consumption settles it:
>
> | Sender | Goodput | CPU / 256 MB | CPU utilization |
> |---|--:|--:|--:|
> | `ztx_buf` — `zmodem.c` rev 2.3 + buffered send | 115.8 MB/s | **0.69 s** | **30 %** |
> | `lsz` | 203.9 MB/s | **0.98 s** | **77 %** |
>
> `zmodem.c` rev 2.3 does the same 256 MB in **30 % less CPU than lrzsz** and is
> **idle 70 % of the wall clock**. Extrapolated to their CPU limits: `zmodem.c`
> ≈ 371 MB/s, lrzsz ≈ 261 MB/s. The receiver is not the limit either — the same
> `lrz` absorbs 203.9 from `lsz`. **After Deuce's 2026-07-24 work the engine is
> cheaper per byte than lrzsz's inlined loop**, and 115.8 is where `ztx_buf`'s
> own I/O pattern stalls (a blocking `write()` per `zmodem_flush`, i.e. per
> subpacket, plus a `poll()` per subpacket in `data_waiting`). Under `strace -w`
> 93 % of `ztx_buf`'s in-syscall wall time is attributed to those polls, though
> the precise stall mechanism has not been isolated. Treat 115.8 as a property
> of the measuring tool.
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

- **Throughput:** the **`sexyz` sender runs at 11.5 MB/s vs `lsz`'s 203.9** —
  ~18×. Two stacked overheads, isolated by linking the real `zmodem.c` behind
  each send architecture (§3.2):
  - **`sexyz.c` ring-buffer + per-byte writes (10×, 115.8→11.5 MB/s):
    sexyz-only, and it is the whole sexyz-specific gap.** `send_byte` takes the
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
    **91.9 → 115.8 MB/s**. But per the warning above, that 115.8 is I/O-stalled,
    not CPU-bound: on **CPU per byte the engine now beats lrzsz** (0.69 s vs
    0.98 s per 256 MB). Further engine micro-optimization has no goodput to win
    here — the remaining gap to lrzsz's 203.9 is a transport/I/O-pattern
    difference, not an escape/CRC difference.
- **sexyz *receiver* is fine** (113.3 MB/s — the same plateau every buffered
  path reaches here), including past 2 GB. The defect is the sender only.
- **Forsberg** (modern branch) *sender* runs at **96.9 MB/s** (below lrzsz —
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
| Ring-buffer/per-byte send (115.8→11.5, futex/tiny-writes) — **open**, 6 prototypes failed the error gate | ✗ (here) | — | OK (immune) |
| Per-byte send cost (callback+escape+CRC) — **addressed** 2026-07-24; engine now cheaper per byte than lrzsz | — | was a weakness | inherits the fix |
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
| **sexyz.c** | **3.3** — `send_byte` writes the ring one byte at a time | **3.3** (unchanged) — every batching prototype regressed error recovery (§3.3, §6); throughput still open |
| **zmodem.c** | **rev 2.2** — window/ACK positions `int32_t`; switch-based byte classifier; byte-at-a-time CRC-32 | **rev 2.3** — 2 GB fix (`uint32_t`, #1196) *plus* Deuce's 2026-07-24 send-path work: class-table classifier, slicing-by-4 CRC-32, hoisted escape mask + `noinline` cold paths, buffered `fcrc32()` |

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

All rows from one interleaved batch, 2026-07-24, three passes (spread ≤2%;
median shown). Every run verified byte-identical by SHA-256.

| Sender → Receiver | Goodput | Note |
|---|--:|---|
| lsz → lrz (lrzsz baseline) | **203.9 MB/s** | 8 KB adaptive blocks, fully inlined escape/CRC |
| buffered floor (`zmodem.c` **rev 2.3** + buffered send) → lrz | **115.8 MB/s** | **I/O-stalled plateau at 30 % CPU** — not an engine limit (§0) |
| lsz → **sexyz** (sexyz *receives*) | **113.3 MB/s** | sexyz receiver is at the engine's ceiling |
| **Forsberg sz** → lrz | **96.9 MB/s** | 1 KB blocks (no ZedZap); single-threaded |
| buffered floor (`zmodem.c` **rev 2.2**, pre-Deuce) → lrz | **91.9 MB/s** | what Deuce's 2026-07-24 work improved on |
| **sexyz** → lrz — **shipped** (ring per-byte) | **11.5 MB/s** | the current sender |

Wire overhead is identical (~2.8 %, +2.55 % for Forsberg's 1 KB blocks) —
protocol efficiency is the same; the difference is purely implementation.

Three things stand out:

1. **sexyz's *receiver* (113.3) reaches the same plateau as every buffered path
   (115.8).** The receive path has no equivalent transport penalty, which is the
   cleanest proof that the sender's 11.5 is a `sexyz.c` send-path defect and not
   anything about `zmodem.c`. (The plateau itself is I/O-stalled, not an engine
   limit — see §0.)
2. **Deuce's `zmodem.c` work moved the shared floor 91.9 → 115.8 (+26%)**, and
   that improvement is inherited by SyncTERM as well as sexyz.
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
| `ztx_buf` = **buffered floor**, `zmodem.c` rev 2.3 | `zmodem.c` + **buffered** send | **115.8 MB/s** |
| `ztx_buf` = buffered floor, `zmodem.c` rev 2.2 | pre-Deuce engine, same transport | **91.9 MB/s** |
| `sexyz` (**shipped**) | `zmodem.c` + **ring per-byte** + drain thread | **11.5 MB/s** |

The buffered `ztx_buf` is **CPU-bound inside `zmodem.c`** (negligible syscall
time), on the `send_byte`-callback-per-byte + ZDLE-escape + CRC-32 pipeline.
Block size barely moves it (the cost is per-byte, not per-block).

**Conclusion:** the sexyz-vs-lrzsz gap is **entirely a transport problem**.
The `sexyz.c` ring-per-byte layer costs 10× and is sexyz-only. What is left
after removing it is **not** an engine limit: every buffered sender measured
here — `ztx_buf` and both sexyz prototypes — lands on ~115.8 MB/s while running
only 30–36 % CPU, i.e. all of them stall in the same *shared I/O shape*
(blocking flush per subpacket + poll per subpacket), not in `zmodem.c`. On CPU
per byte the engine beats lrzsz (0.69 s vs 0.98 s per 256 MB). So the ceiling
for a properly non-blocking sexyz sender is **unknown and plausibly above
lrzsz's 203.9** — it has not been measured, because no prototype has yet used
non-blocking output. The obstacle is not the engine and not raw performance —
it is that every batched prototype so far breaks error recovery (§3.3).

### 3.3 Why the fix is hard: six prototypes, one gate

Batching the producer is trivially fast and repeatedly wrong. Note that every
"115.8" below is the I/O-stalled plateau described in §0, not a ceiling: these
prototypes ran at 30–36 % CPU, so their real headroom was never measured. The
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
| single-threaded + batch 4 KB, **blocking** writes | 115.8 MB/s | 0.83 s | 2/3 |

Two results are worth keeping:

- **It is not a speed artifact.** The obvious objection — "the batched sender is
  10× faster, so it simply has more data in flight when corruption hits" — was
  tested directly by capping the batched sender to 11 MB/s, the shipped sender's
  own natural rate. At the identical rate, **the shipped sender passed 3/3 and
  the batched sender still failed**. Batching genuinely breaks recovery.
- **The output thread is most of the problem, but not all of it.** Dropping to a
  single thread with a buffered `send_byte` reached the same 115.8 MB/s, cut
  voluntary context switches from 1,464,130 to **1,585** (`lsz` does 1,402), and
  lifted the gate from 0–1/3 to **2/3**. What remains is that the prototype's
  `sendbuf()` still *blocks*: when the socket fills while the receiver is trying
  to send a ZRPOS, the sender stops reading the back-channel and the session
  stalls.

Earlier failed variants are catalogued in `src/bench/zmodem/README.md`; two of
their failure modes are now understood and should not be repeated — a `void`
flush callback that silently swallowed a failed write, and a sticky error latch
that turned one transient full-buffer timeout into an infinite
`zmodem_send_raw ERROR: -1` spin. A correct implementation must treat a failed
flush as **transient**, keep the unwritten remainder buffered, and let
`zmodem.c`'s own retry/abort logic run.

That leaves exactly one missing ingredient, and it is the one `lrz` has and none
of the prototypes did: **non-blocking output with `select()` on both directions**.

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
> behavior under each condition, not steady-state throughput (see §3). They date
> from the original 2026-07-23 run and were not re-measured; the *relative*
> behavior under each condition is what they are for. A buffered sender would
> raise every clean-link row to at least the 115.8 plateau, but none has passed
> the error gate (§3.3).

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

2. **⚠ OPEN — sender throughput. Rewrite the sender the way `lrz` does it.**
   Six prototypes now (§3.3) confirm the shape of the answer:
   - The entire sexyz-specific penalty is the **per-byte ring traffic**, and
     buffering the producer removes **all** of it — a prototype reached
     115.8 MB/s at 1/44th the CPU, and no amount of ring/highwater/drain tuning
     touches it (§3.1). 115.8 is *not* the endpoint: at that rate the prototype
     ran only 44 % CPU, so a non-blocking sender's real ceiling is unmeasured
     and plausibly above lrzsz's 203.9 (§0).
   - Batching **while keeping `output_thread`** fails the error gate at every
     buffer size tried (512 B – 4 KB, 0–1 of 3), and it is *not* a
     rate artifact — rate-capped to the shipped sender's own 11 MB/s it still
     failed while the shipped sender passed 3/3.
   - Going **single-threaded with a buffered `send_byte`** kept the full speed,
     brought context switches down to `lsz`'s level, and got the gate to 2/3.
     The remaining failure is the blocking `sendbuf()`.

   So the target is what `lrz` actually is: **single-threaded, buffered,
   non-blocking output, `select()`/`poll()` on both directions**, with the
   protocol thread owning its own output buffer and flushing it in step with
   protocol state. The two-thread ring should go away rather than be optimized.
   Implementation notes that cost time to learn: a failed flush is **transient**
   (keep the remainder buffered, never latch an error, never drop bytes), and
   the flush must happen before any wait on the back-channel — `recv_buffer()`
   is the single choke point for that in `sexyz.c`. GitLab #1195.

3. **✅ DONE (and no longer the lever) — per-byte `zmodem.c` send cost.** Deuce
   reworked the shared send path on 2026-07-24 (class-table byte classifier,
   slicing-by-4 CRC-32, hoisted escape mask with `noinline` cold paths, buffered
   `fcrc32()`), inherited by SyncTERM as well as sexyz. **The engine now costs
   less CPU per byte than lrzsz** (0.69 s vs 0.98 s per 256 MB), so handing
   `send_byte` a span instead of a byte would reduce CPU that is not being
   spent. Do not treat the 115.8-vs-203.9 goodput gap as evidence for more
   engine work — that gap is transport/I/O shape (§0). Also note these per-byte
   CPU wins are **invisible to a single-stream localhost benchmark above
   ~100 MB/s**: Deuce measured his with six interleaved 1 GiB transfers, and two
   of his four commits measured exactly zero in this harness.

4. **○ OPEN — adaptive block-length cost model** (lrzsz-style `calc_blklen`) to
   replace the ×2/÷2 ramp. Largest lever on lossy/variable links; sexyz + SyncTERM.

5. **○ OPEN — the `.`-prefixed received filename** (sexyz receiver wrote
   `.test2g.bin`) — minor, separate.

None of these lift the 4 GB wire ceiling, which is inherent to ZMODEM.
