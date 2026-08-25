# ZMODEM implementation comparison: sexyz vs. lrzsz vs. zmtx/zmrx vs. Forsberg rzsz

**Date:** 2026-07-23; re-measured 2026-07-24, 2026-08-21, and 2026-08-24
(receive path, §3.5)
**Author:** Claude (analysis commissioned by Rob Swindell)
**Scope:** Performance, scalability (>2 GB / >4 GB), and robustness of
Synchronet's ZMODEM implementation, **in both directions**, benchmarked against
`lrzsz` 0.12.21rc, `zmtx`/`zmrx` 2.02, and Chuck Forsberg's final `rzsz`
(3.73, 2003-01-30).

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
> built on `zmodem.c` now runs level with lrzsz** — a claim the 2026-08-24
> re-baseline retracts, see §3:
>
> | Sender (256 MiB → `lrz`) | Goodput | Elapsed | CPU |
> |---|--:|--:|--:|
> | `lsz` (lrzsz baseline) | 203.8 MB/s | 1.32 s | 0.96 s |
> | **sexyz, fixed** | **203.9 MB/s** | 1.32 s | 1.02 s |
> | **`ztx_buf`, fixed** | **203.9 MB/s** | 1.32 s | 0.72 s |
> | sexyz, before the fix | 115.7 MB/s | 2.32 s | 1.03 s |
> | `ztx_buf`, before the fix | 115.8 MB/s | 2.32 s | 0.67 s |
>
> **Those rows are `lrz`'s ceiling, not a tie.** `lrz` receives at 208.4 MB/s
> for 1.28 CPU-seconds and was the slowest end of every run above. Re-measured
> against a receiver cheaper than the senders, sexyz sends at **335.3 MB/s**
> and `lsz` at **279.9** — sexyz is 20 % ahead, not level (§3).
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
> 4. **The fixed end of a comparison must be cheaper than the varied end**
>    (2026-08-24, also Deuce: "switch the baseline receiver, `lrz` can't handle
>    the load"). Every sender table here was scored against `lrz`, which costs
>    more CPU to receive than any of those senders spend to send — so the table
>    reported `lrz`'s ceiling four times over and called it a tie. §3 is
>    re-baselined; §3.0.2 states the rule.

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
- **✅ RESOLVED (zmodem.c 2.7, sexyz.c 3.6) — the receive path.** A `recv_span`
  callback lets a consumer hand the engine whole runs of unescaped bytes;
  `zmodem_rx()`'s per-byte predicate became a table the two paths share.
  **sexyz receives at 486.5 MB/s / 0.55 CPU-s, up from 117.2 / 2.29** — 1.70×
  `zmrx` 2.02 and 2.3× `lrz`, making it the fastest receiver measured. The
  callback is optional (`NULL` = the old path), so **SyncTERM needs no source
  change**, and the table alone lifts the un-adopted per-byte path to
  145.7 MB/s. The analysis that led here follows.
- **The RECEIVE path was slow, and the cost was in `zmodem.c` — so it was
  SyncTERM's too (§3.5).** Driven by the same `lsz` sender: `zmrx` 281.6 MB/s,
  `lrz` 208.4, **`zmodem.c` + a minimal transport (`zrx_buf`) 128.2**, sexyz
  114.0. CPU is additive, so the split is direct: of sexyz's **+1.07 CPU-seconds
  over `lrz`, 0.81 s (76 %) is `zmodem.c`** and 0.26 s is `sexyz.c`'s
  `recv_byte`. **No receiver built on `zmodem.c` can exceed ~128 MB/s however
  good its wrapper** — 0.61× `lrz`, 0.45× `zmrx`. `zmodem.c`'s three receive
  functions cost **82.6 instructions per received byte**; `lrz`'s *entire*
  receiver costs **24.1**.
  The cause is a four-deep **per-byte** call chain that makes batching
  inexpressible: no span copy, no hoisted invariants, and a byte-at-a-time CRC
  where the send path got slicing-by-4 in 2026-07-24. **The indirect calls
  themselves are not the cost** — a direct A/B adding one more per byte moves
  throughput ~0.5 % (§3.5) — the 4–5× instruction count and 8× load traffic are.
  Correct past 2 GB, no per-file cost, and invisible below ~1 Gbps. Reported by
  Deuce; profiled 2026-08-24.
  > Earlier revisions of this doc said "sexyz *receiver* is fine (121.0 MB/s)"
  > and "every defect in this doc is on the sending side." Both were wrong. 121
  > was read against the sender's then-broken 11.5 MB/s rather than against the
  > receivers sexyz competes with, and the receive path had only ever been
  > timed, never profiled.
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
| **zmtx/zmrx** 2.02 | `{zmdm.c,zmtx.c,zmrx.c,zmdm_posix.c}` | 1169+1124+983+300 | single-threaded, buffered; inline fast path + `rx_slow` for escapes |
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
| **sexyz.c** | **3.3** — `send_byte` writes the ring one byte at a time; `-w` sets the window without touching the block size | **3.4** — buffered streaming send path (~11 → ~115 MB/s, #1195); `-w` clamps the block to window/4 like `lsz`/`sz` so `window ≤ block` no longer stalls (#1197). **3.5** — no change of its own: the startup banner prints only sexyz's own revision, so it moves with `zmodem.c` 2.5 to keep a build identifiable without `sexyz v`. **3.6** — supplies `recv_span`, taking a receive from 117.2 to 486.5 MB/s and making sexyz the fastest receiver measured (§3.5) |
| **zmodem.c** | **rev 2.2** — window/ACK positions `int32_t`; switch-based byte classifier; byte-at-a-time CRC-32; quarter-window ACK interval divides by zero when window < 4×block | **rev 2.5** — 2 GB fix (`uint32_t`, #1196); Deuce's 2026-07-24 send-path work (class-table classifier, slicing-by-4 CRC-32, hoisted escape mask + `noinline` cold paths, buffered `fcrc32()`); window-interval divide-by-zero guarded (#1197); the 1-second ZRPOS purge off the normal send path and the ZFILE retransmit at the receiver's opening ZRINIT, both 2026-08-21. **rev 2.6** — the receiver reports its own file position (`09ea8a6901`, gold-13-arrest, 2026-08-23). **rev 2.7** — the receive path: a shared plain-byte table and an optional `recv_span` bulk callback (§3.5), and a receive no longer aborts at the tenth error of the whole file (§5.2) |

- **SyncTERM:** its `term.c` send path is **unchanged** by this work; a SyncTERM
  throughput figure here is *modeled* by `ztx_buf` (the real `zmodem.o` behind a
  SyncTERM-style buffered `send_byte`), not measured from the SyncTERM binary.
- **Third-party:** lrzsz **0.12.21rc**; Forsberg rzsz **3.73** from the history
  repo's **`modern` branch**; **zmtx/zmrx 2.02** — Jacques Mattheij's original
  1994 ZMODEM, modernised by Deuce at `RealDeuce/zmtx-zmrx` on GitHub,
  `c05df7b` (fake-1-beds, 2026-08-24), built `-O3`. It is the **fastest
  receiver measured** (§3.5), so it belongs in the comparison rather than a
  footnote.

The `revision`/`zmodem_ver` strings above are the stable anchors; the git log
maps them to commits. Throughput work is GitLab #1195, the 2 GB fix #1196.

---

## 2. Benchmark method

- **Harness:** `zbench_sock.py` (`src/bench/zmodem/`) — wires a sender and
  receiver (each speaking ZMODEM on stdin/stdout) through a userspace relay that
  can inject one-way latency, a token-bucket bandwidth cap in each direction,
  and forward bit-corruption. No root/`netem` required.
- **Profilers (§3.5):** `strace -c -f` for the syscall mix; `valgrind
  --tool=callgrind` (with `--branch-sim --cache-sim`) for per-byte instruction,
  load/store, branch and call counts. `perf` is unavailable on this host
  (`perf_event_paranoid=3`, package not installed), and callgrind needs no
  privileges, so it is the profiler of record here. Its instruction counts are
  exact; its *timings* are not — always pair it with the wall-clock/CPU run.
- **Relay ceiling** measured at **5069 MB/s** — 25×+ above any tool, so it is
  not the bottleneck for clean-link runs.
- **Transport:** localhost pipes (CPU/architecture-bound regime), on tmpfs
  (RAM-backed) so disk never confounds protocol behavior.
- **Flags:** `-8` (8 KB max block, ZedZap) on both sides; `-w` (transmit window)
  where noted. `sexyz` binary: `gcc.linux.x64.exe.release/sexyz`
  (`v3.3 master/074785210`). lrzsz built from `~/lrzsz-0.12.21rc`.
- **Multi-file batches (added 2026-08-24).** `--file` now repeats, so a run can
  send several files in one session. It used to be singular, and **a per-file
  cost was therefore invisible** — it landed once, in a goodput figure designed
  to divide fixed costs away, and the rate-capped rows refunded it entirely from
  banked tokens. That blind spot hid a flat one-second-per-file stall for months
  (§0). Anything suspected of costing per *file* rather than per byte gets a
  batch of small files; §3.5 uses five 6 KB files to show the receive path has
  no such cost.
- **CPU per endpoint (added 2026-08-24).** Both processes are reaped with
  `wait4()`, so every run prints the sender's and receiver's user/system CPU and
  context switches beside the goodput. **A number with no CPU figure next to it
  cannot distinguish "fast code" from "idle code"** — that ambiguity cost this
  investigation a day in July, and it is what makes §3.5's receiver-bound
  finding legible at a glance (receiver CPU ≈ wall clock in every row).
- **The fixed end must be the cheap end (§3.0.2).** When the sender is varied,
  the receiver has to cost less CPU than every sender under test, and vice
  versa — otherwise the fixed end is the bottleneck and the table measures it
  instead. This is not a formality: §3's sender rows read as a three-way tie for
  three days because `lrz` (1.28 CPU-s) was more expensive than any sender in
  the table. The per-endpoint CPU columns are what make the violation visible.
  Current cheapest of each kind, and therefore the current baselines: **sexyz
  receiving** (0.55 CPU-s / 256 MB, `zmodem.c` 2.7) and **`lsz` sending**
  (0.96 s) — though `zmtx` sends for 0.47 s, cheaper than any receiver
  measured, so no receiver-varied table can yet be driven at its full rate.
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

**The baseline receiver changed on 2026-08-24, and it moved every number in
this section.** Every sender row this doc ever published was scored against
`lrz`. §3.5 then measured `lrz` itself: 208.4 MB/s at **1.28 receiver
CPU-seconds, equal to the wall clock**. `lrz` was never a neutral sink — it was
the slowest component in each of those runs, so the senders were being scored on
*its* ceiling. Deuce called this ("switch the baseline receiver, `lrz` can't
handle the load"), and he is right.

`zmrx` is the obvious replacement at 0.94 CPU-s, but as of `zmodem.c` 2.7 it is
not the cheapest receiver available: the span work of §3.5 put **sexyz itself at
0.55–0.67 CPU-s**, 30–40 % below `zmrx`. So the baseline receiver is now sexyz,
and the old rows are kept below only for continuity. (The range is real and
sender-dependent: a sender that delivers larger chunks yields longer plain runs
and fewer wakeups, so sexyz costs 0.55 s behind `zmtx` and 0.67 s behind `lsz`.
Either is below `zmrx`'s flat 0.94.)

**Sender varied, receiver = sexyz (`zmodem.c` 2.7, `recv_span`).** 2026-08-24,
one interleaved batch, five passes, median shown, every run byte-identical:

| Sender | Goodput | Sender CPU | Threads | Bound by |
|---|--:|--:|--:|---|
| **`zmtx`** 2.02 | **487.2 MB/s** | **0.47 s** | 1 | *still the receiver* (0.55 s = wall) |
| **`ztx_buf`** — `zmodem.c` 2.7 + buffered send | **385.4 MB/s** | **0.70 s** | 1 | sender |
| **sexyz** 3.6 | **335.3 MB/s** | **1.00 s** | 2 | sender |
| **`lsz`** (lrzsz baseline) | **279.9 MB/s** | **0.96 s** | 1 | sender |
| **Forsberg `sz`** 3.73 | 96.7 MB/s | 2.77 s | 1 | sender (always was) |

**The same senders, across all three receivers** — this is the whole point:

| Sender | → `lrz` (1.28 CPU-s) | → `zmrx` (0.94) | → sexyz 2.7 (0.55–1.34) | Sender CPU |
|---|--:|--:|--:|--:|
| `zmtx` | 208.8 | 286.1 | **487.2** | 0.47 s |
| `ztx_buf` | 208.9 | 286.0 | **385.4** | 0.70 s |
| **sexyz** | 205.9 | 275.2 | **335.3** | 1.00 s |
| `lsz` | 207.9 | 281.5 | **279.9** | 0.96 s |
| Forsberg `sz` | 97.3 | 97.2 | 96.7 | 2.77 s |

Against `lrz` the top four senders sit inside **1.6 %** of each other; against
`zmrx`, inside 4 %; against a receiver cheaper than they are, they spread over
**1.74×**. The first two columns were not measuring senders at all. Only
Forsberg is unmoved, because at 2.77 CPU-seconds it is the one sender that was
always slower than every receiver.

The sexyz column's receiver cost is a range because **the span receiver's
efficiency depends on how the sender writes.** Behind `zmtx`'s 8 KB subpackets
it receives 256 MB for 0.55 CPU-s; behind Forsberg's 1 KB blocks the same
receiver spends **1.34 s and 256 k context switches** for the same bytes. Bigger
arrivals mean longer plain runs per `recv_span()` call and fewer wakeups. The
receiver is unchanged; only its input granularity is.

Wire overhead is identical (~2.8 %, +2.55 % for Forsberg's 1 KB blocks) —
protocol efficiency is the same; the difference is purely implementation.

Four things stand out:

1. **sexyz now sends faster than `lsz` — 335.3 vs 279.9, +20 %.** That was
   invisible before: against `lrz` the two read 205.9 and 207.9, a difference
   this doc reported as "level with lrzsz". They were level in the sense that
   both were waiting on the same receiver.
2. **But sexyz's advantage is concurrency, not efficiency.** It spends *more*
   CPU than `lsz` (1.00 s vs 0.96 s) and wins on wall clock by spreading it over
   two threads — 65 k context switches per transfer, the ring producer and the
   drain thread. The honest efficiency claim belongs to `ztx_buf`: the same
   `zmodem.c`, single-threaded and buffered, at **385.4 MB/s for 0.70 CPU-s** —
   faster *and* cheaper than `lsz` on both axes. That, not sexyz's number, is
   the measure of the engine SyncTERM shares.
3. **`zmtx` is the fastest sender by a wide margin, at the lowest cost:**
   487.2 MB/s for 0.47 CPU-seconds, roughly half of `lsz`'s CPU for 1.74× the
   throughput. Its row is still receiver-bound (receiver CPU 0.55 s = wall
   clock), so 487.2 is a *lower bound* on `zmtx`. Nothing here is fast enough to
   find its ceiling.
4. **Forsberg reaches 96.7 with only 1 KB subpackets** — a clean single-threaded
   buffered send beats an 8×-larger block size behind a bad transport by ~8×.

### 3.0.1 The 2026-08-21 rows, kept for continuity

These are the same senders against `lrz`, and they are what this doc reported
before the baseline moved. Read every figure as "`lrz`'s ceiling", not as the
sender's:

| Sender → Receiver | Goodput | Note |
|---|--:|---|
| **sexyz** → lrz — `zmodem.c` 2.5, sexyz.c 3.4 | 203.9 MB/s | 2026-08-21; receiver-bound |
| lsz → lrz (lrzsz baseline) | 203.8 MB/s | 2026-08-21; receiver-bound |
| `ztx_buf` (`zmodem.c` **rev 2.5** + buffered send) → lrz | 203.9 MB/s | 2026-08-21; receiver-bound |
| lsz → **`zrx_buf`** (`zmodem.c` *receives*, pre-2.7) | 128.2 MB/s | 2026-08-24; the engine's old receive ceiling — **§3.5** |
| lsz → **sexyz** (sexyz *receives*, v3.5) | 114.0 MB/s | 2026-08-24; 0.55× lrz — fixed in 3.6, see **§3.5** |
| `ztx_buf` (`zmodem.c` **rev 2.4**) → lrz | 115.8 MB/s | the old "buffered floor": 1.32 s of work + 1.00 s of sleep (§0) |
| **sexyz** → lrz — before the purge fix | 115.7 MB/s | 2026-08-21; same second, same arithmetic |
| **Forsberg sz** → lrz | 96.9 MB/s | 2026-07-24; 1 KB blocks (no ZedZap); single-threaded |
| `ztx_buf` (`zmodem.c` **rev 2.2**, pre-Deuce) → lrz | 91.9 MB/s | 2026-07-24; what Deuce's work improved on |
| **sexyz** → lrz — sexyz.c **3.3** (ring per-byte) | 11.5 MB/s | 2026-07-24; superseded by 3.4 |

The 115.8-vs-203.9 step in that table is real and is §0's fixed second; it is
below `lrz`'s ceiling, so the receiver could not mask it. Everything at 203.9
was against the ceiling and says nothing about the sender.

### 3.0.2 The general lesson: a benchmark measures its slowest end

Both halves of this document have now been wrong in the same way, in opposite
directions. §3.5's receive table holds the sender fixed, which is right — but
only because that sender (`lsz`, 0.96 CPU-s) was cheaper than the receivers
under test, so the receiver was the bottleneck in every row. §3's sender table
held the *receiver* fixed at something more expensive than the senders, and
therefore measured nothing.

The rule this yields, and the one the harness README now carries: **the fixed
end must be cheaper in CPU than everything being compared at the varied end,
and that must be shown, not assumed.** The `wait4()` CPU columns added on
2026-08-24 are what makes it checkable at a glance — when the varied end's CPU
is below the wall clock and the fixed end's equals it, the run is measuring the
fixed end. Every table in this doc now reports both endpoints' CPU for exactly
that reason.

### 3.0.3 Forsberg's receiver could not be benchmarked headlessly

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

### 3.5 The receive path (measured 2026-08-24) — the cost was in `zmodem.c`, so it was SyncTERM's too

Deuce reported that sexyz receives slower than `lrz` and `zmrx`. That is
correct, and §0's "sexyz *receiver* is fine (121.0 MB/s)" was wrong — that
verdict was set against the sender's then-broken 11.5 MB/s rather than against
the receivers sexyz competes with. Every earlier receive figure in this doc is a
**throughput** number; the receive path had never been profiled.

**The important result is not about sexyz.** Splitting the two layers the way
§3.2 did for the send side — `zrx_buf`, a minimal receiver linking the real
`zmodem.o` behind a plain buffered `recv_byte` — puts **~76 % of the deficit
inside `zmodem.c`**, which SyncTERM shares in full. sexyz's own transport
wrapper is the minority of it.

Same sender (`lsz -8`) in every row, so the receiver is the only variable.
256 MiB, localhost, tmpfs, one interleaved batch, three passes (spread <1 %,
median shown), every run byte-identical:

| Receiver | Goodput | Receiver CPU | Excess CPU vs `lrz` |
|---|--:|--:|--:|
| **`zmrx`** (zmtx-zmrx 2.02) | **281.6 MB/s** | **0.95 s** | −0.33 s |
| **`lrz`** (lrzsz 0.12.21rc) | **208.4 MB/s** | **1.28 s** | — |
| **`zrx_buf`** — real `zmodem.c`, minimal transport | **128.2 MB/s** | **2.09 s** | **+0.81 s** |
| **`sexyz`** (v3.5, zmodem.c 2.6) | **114.0 MB/s** | **2.35 s** | +1.07 s |

**One caveat on that table, found on 2026-08-24 (§3.0.2):** the fixed `lsz`
sender costs 0.96 CPU-s, so the `zmrx` row — 0.95 s — was at the sender's
ceiling, and 281.6 understates it; driven properly `zmrx` reads 286.1. The other
three rows cost 1.28 s and up, well above the sender, so they are true receiver
measurements and the diagnosis below is unaffected. The CPU column is what
matters here anyway, and CPU does not depend on who was waiting for whom.

CPU is additive, so the split is direct: of sexyz's **+1.07 CPU-seconds** over
`lrz`, **0.81 s (76 %) is `zmodem.c`** and 0.26 s (24 %) is `sexyz.c`'s
`recv_byte`. **A receiver built on `zmodem.c` cannot exceed ~128 MB/s no matter
how good its transport wrapper is** — that is 0.61× `lrz` and 0.45× `zmrx`
before sexyz contributes anything.

Instruction counts say the same thing more sharply. Summing `zmodem.c`'s three
receive functions gives **82.6 instructions per received byte** measured through
`zrx_buf` and **81.5** measured through sexyz — the engine costs the same either
way, as it should. `lrz`'s **entire** receiver, transport included, is
**24.1**. So `zmodem.c`'s engine alone is **3.4× the whole of lrzsz's receiver.**

`zrx_buf` is built in the **SyncTERM shape** (see below), so its 128.2 is the
number to reason about for SyncTERM — with the caveats in "What this does and
does not say about SyncTERM".

#### It is not syscalls — the opposite of the send-path bug

The 2026-07-24 sender collapse was a syscall pathology (§3.1: 1.96 M futex
calls, ~94-byte writes). Receive is the reverse. Under `strace -c -f` on a
32 MB receive, total syscall time is **1.8 ms (`lrz`), 25 ms (sexyz), 3.8 ms
(`zmrx`)** against ~0.3 CPU-seconds of work — and sexyz issues the **fewest**
reads of the three:

| Receiver | `read()` | avg read | other hot syscalls |
|---|--:|--:|---|
| `lrz` | 4,225 | ~8 KB | 8,434 `alarm`, 4,223 `rt_sigaction` |
| **sexyz** | **561** | **~60 KB** | 8,235 `write`, 17 `futex` |
| `zmrx` | 2,135 | ~16 KB | 8,196 `write`, 4,104 `lseek`, 2,135 `pselect6` |

`recv_buffer()` fills a 64 KB `inbuf` (`sexyz.c:88,454`), so the transport read
side is already efficient; sexyz's 25 ms is 17 `futex` calls from an idle output
thread. **The cost is entirely userspace compute.**

#### Where it goes: a four-deep per-byte call chain

`callgrind`, 4 MiB receive. Share of all instructions:

| `lrz` | | `zmrx` | | **`zrx_buf`** (engine + minimal transport) | | **sexyz** | |
|---|--:|---|--:|---|--:|---|--:|
| `zrdata` | 80.9 % | `rx_data` | 84.1 % | `zmodem_recv_raw` | 30.0 % | `recv_byte` (sexyz.c) | 30.0 % |
| `zdlread2` | 17.9 % | `crc32_update` | 11.5 % | `zmodem_rx` | 29.7 % | `zmodem_recv_raw` | 25.2 % |
| | | | | `zmodem_recv_data32` | 17.8 % | `zmodem_rx` | 24.1 % |
| | | | | `my_recv_byte` | 15.4 % | `zmodem_recv_data32` | 15.0 % |
| | | | | `my_is_connected` | 4.7 % | | |

`lrz` and `zmrx` each do their inner loop in essentially one function.
`zmodem.c` spreads it across three, none inlined into the others, plus a
transport callback: `zmodem_recv_data32` → `zmodem_rx` → `zmodem_recv_raw` →
`zm->recv_byte`. Invocation counts per 4 MiB (4,194,304 data bytes,
4,312,067 wire bytes) confirm every level is per byte:

| Callee | Calls | Per byte |
|---|--:|--:|
| `recv_byte` (via `zm->recv_byte`) | 4,312,066 | 1.03 (one per *wire* byte) |
| `zmodem_recv_raw` | 4,312,041 | 1.03 |
| `is_connected` (via `zm->is_connected`) | 4,197,025 | 1.00 (one per *data* byte) |
| `zmodem_rx` | 4,196,993 | 1.00 |
| — `lrz`: `zdlread2` (rest inlined) | 1,048,003 | 0.25 |
| — `zmrx`: `rx_slow` (escape path only) | 115,038 | 0.027 |

`zmrx`'s 0.027 is the giveaway: it matches the **2.8 % ZDLE escape rate**, so
`zmrx` handles unescaped bytes inline (`rx()` is `static inline`) and calls out
only for a real escape. `lrz` calls its slow half for a quarter of bytes.
`zmodem.c` calls out for **every** byte.

#### The indirect calls are cheap; the structure they impose is not

Per received byte:

| | instr/B | loads/B | stores/B | cond-br/B | mispred/B | mispred % | indirect/B |
|---|--:|--:|--:|--:|--:|--:|--:|
| `lrz` | 24.1 | 3.89 | 3.62 | 4.38 | 0.443 | 10.12 % | 0.00 |
| `zmrx` | 34.8 | 7.54 | 5.26 | 6.61 | 0.155 | 2.35 % | 0.00 |
| **`zrx_buf`** | **106.5** | **29.70** | 19.48 | 17.40 | 0.120 | 0.69 % | **3.06** |
| **sexyz** | **126.8** | **33.75** | 18.42 | 21.49 | 0.118 | 0.55 % | **2.03** |

The 2–3 indirect branches per byte are eye-catching, and it would be easy to
call them the cause. **They are not, and the difference between those two rows
proves it.** `zrx_buf` makes *more* indirect calls per byte than sexyz (3.06 vs
2.03) because `zmodem_rx`'s loop condition is
`while (is_connected(zm) && !is_cancelled(zm))` and both helpers dispatch
through a function pointer when one is supplied — sexyz passes **`NULL`** for
`is_cancelled` (`sexyz.c:1644`), SyncTERM and `zrx_buf` pass a real one. Running
`zrx_buf` both ways is a direct A/B on exactly one indirect call per byte:

| `zrx_buf` | Goodput |
|---|--:|
| `is_cancelled` supplied (SyncTERM shape) | 127.5 MB/s |
| `is_cancelled` `NULL` (sexyz shape) | 128.1 MB/s |

**~0.5 %, inside the layout noise below.** A perfectly-predicted call to a
function that always returns the same value costs almost nothing; note the
mispredict *rate* is 0.55–0.69 % here against `lrz`'s 10.12 %. `zmodem.c` is not
stalling on branches — it is executing 4–5× the instructions and issuing 8×
the loads (29.7–33.8 per byte against `lrz`'s 3.89).

That load traffic is the real signature. Nothing stays in registers across four
non-inlined frames, so each level reloads `zm` and respills. The calls are the
*mechanism* by which the work cannot be batched — no span `memcpy`, no hoisted
loop invariants, and a CRC that must run a byte at a time. Deuce's 2026-07-24
work gave the **send** path slicing-by-4 (`ucrc32_4`, `zmodem.c:576`); the
**receive** path still runs byte-at-a-time `ucrc32` (`zmodem.c:943`) because the
bytes arrive one call at a time. **Inlining is not the fix — batching is, and
inlining is what makes batching expressible.**

#### Two things this is *not*

**There is no per-file receive cost.** Five 6 KB files (30,720 B) in one batch
over a simulated 115200 bps line — the shape of Uwe Ohse's report — took
**2.23 s on all three receivers**, twice, identically. 32,131 wire bytes ÷
14,400 B/s = 2.23 s: the link is the whole story. The one-second-per-file stall
of §0 was sender-side; nothing analogous exists on receive.

**It is invisible below gigabit.** Rate-capped at 50, 100, 200 and 400 Mbps all
receivers return identical link-bound numbers (6.08 / 12.15 / 24.28 /
48.49 MB/s). `zmodem.c`'s ceiling is ~128 MB/s ≈ 1.0 Gbps, so a receive only
falls behind `lrz` on a link that can carry more than that — LAN, loopback, or a
local socket. On dial-up or WAN the network dominates and every implementation
is indistinguishable. That bounds the priority; it does not make the gap less
real, and it is exactly the regime SyncTERM users on a LAN are in.

#### Caveat: at this scale codegen layout moves the number ±5 %

A same-tree bisect of the commits since the 2026-08-21 batch shows one
reproducible step, at `09ea8a6901` (gold-13-arrest, 2026-08-23) — the commit
that assigns `zm->current_file_pos` from `ftello()` per received subpacket:

| Build | Goodput |
|---|--:|
| `9e7bbc3092` (endorsed-14-habits, 2026-08-21) — zmodem.c 2.5 | 123.6 MB/s |
| `09ea8a6901` (gold-13-arrest, 2026-08-23) — zmodem.c 2.6 | **117.2 MB/s** |
| `ea25028dae` (menus-17-verse, 2026-08-23) | 117.0 MB/s |
| HEAD `6fba4d0e06` | 116.2 MB/s |

**That −5.2 % is not added work.** Total instructions are identical
(531,907,105 → 531,966,714, +0.011 %), syscall counts are identical (8,236 →
8,235 writes), and the extra `ftello` costs 518 calls per 4 MiB — one per 8 KB
subpacket, flushing nothing, because the pre-existing `zm->progress(...,
ftello(fp))` on the next line already flushed. Same instructions, same syscalls,
5 % less throughput: this is code layout. The same source built in two different
trees measured 116.2 and 113.8, and `zrx_buf` moved 124.6 → 128.2 across an
edit that did not touch its hot loop. **Treat any single-digit percentage in
this section as layout noise.** The gap being reported here is 63 % and 147 %,
far outside it.

#### What this does and does not say about SyncTERM

`zrx_buf` is deliberately SyncTERM-shaped: single-threaded, a buffered
`recv_byte` that is a buffer-index fetch (SyncTERM's own, `term.c:1024`, is the
same shape), and a real `is_cancelled` callback, which SyncTERM supplies as
`xfer_zmodem_check_abort` (`term.c:2256`) and sexyz does not.

It is **not a SyncTERM throughput model**, for the same reasons §0 gives for
`ztx_buf` on the send side: SyncTERM's real-world speed is a deliberate
BDP/socket-buffer choice, and its input path additionally runs the wren and RIP
filters (`recv_bytes()`, `term.c:964`) that `zrx_buf` omits. Those can only make
SyncTERM slower than `zrx_buf`, not faster, so **128.2 MB/s is an upper bound on
what SyncTERM's receive can do on this hardware**, not a measurement of it.
Measuring the real thing needs the GUI driven end-to-end, which this harness
cannot do — and is why the engine is isolated instead.

One sexyz-specific note that does **not** transfer: sexyz's `recv_byte` contains
a telnet-IAC state machine, but it is conditional (`if (telnet)`, `sexyz.c:583`)
and `sexyz.c:1980` clears `telnet` in stdio mode unless `-telnet` was passed —
so it is **not** in the measured path here, and `-rlogin`/`-ssh`/`-raw` disable
it in production too. Its cost is unmeasured: a plain `lsz` does not IAC-escape,
so `sexyz -telnet` cannot complete a transfer against one.

#### What closed it (zmodem.c 2.7, sexyz.c 3.6)

Both changes are in `zmodem.c`, so they reach sexyz and SyncTERM alike.

**One table, two paths.** `zmodem_rx()` decided per byte, through a switch,
whether the byte needed handling. That predicate is now precomputed into
`rx_plain_tab[256]`, tested before the switch, and **shared with the span
path** — so the two cannot develop separate ideas of which bytes are special.
Built in `zmodem_init()` and rebuilt where `escape_ctrl_chars` is assigned from
the peer's ZRINIT, the only place it changes. This alone, with no callback
supplied, is worth **126.9 → 153.0 MB/s** and 2.11 → 1.74 CPU-s. (That pair is
`lsz`-driven — a matched A/B, so the ratio holds; both builds are far slower
than the sender, so neither is pinned by it. The same NULL path driven by `zmtx`
reads 145.7 MB/s / 1.84 CPU-s, in the table below.)

**An optional `recv_span` callback.** The transport copies while the table says
the byte is plain and stops before the first that is not, leaving it unconsumed;
`zmodem_recv_data32()` takes the run whole and CRCs it as a block with
`ucrc32_span()` — the slicing-by-4 the send path has used since 2026-07-24.
Passing the table *to* the transport keeps protocol knowledge out of it and
avoids any need to hand bytes back when a run ends early. It is a `zmodem_t`
struct field, not a `zmodem_init()` parameter, so `zmodem_init()`'s memset
leaves it `NULL` for every existing consumer with no source change, and `NULL`
is exactly the old path.

Driven by `zmtx -8` (0.47 sender CPU-seconds, the cheapest sender measured —
§3.0.2 on why the fixed end must be the cheap one). Receiver CPU equals the wall
clock in every row, so every row is genuinely receiver-bound:

| Receiver | Goodput | Receiver CPU |
|---|--:|--:|
| **sexyz 3.6, span** | **486.5 MB/s** | **0.55 s** |
| `zrx_buf`, span | 479.3 MB/s | 0.56 s |
| `zmrx` 2.02 | 286.1 MB/s | 0.94 s |
| `lrz` | 209.2 MB/s | 1.28 s |
| `zrx_buf`, `recv_span` NULL — **what an un-adopted consumer gets** | **145.7 MB/s** | 1.84 s |
| sexyz 3.5 (per-byte, before) | 117.2 MB/s | 2.29 s |

On random data the runs average **36 bytes and cover 97.3 %** of the stream.
sexyz is now **4.2×** its former throughput at **24 %** of the CPU, and is the
**fastest receiver measured** — 1.70× `zmrx`, which held that place until today.

**This table was itself re-measured on 2026-08-24 for the reason §3.0.2 gives.**
Driven by `lsz` (0.96 CPU-s) the top three rows all read ~280 MB/s and the fix
looked like it had merely drawn level with `zmrx`. It had not: 280 was `lsz`'s
own ceiling, and all three receivers were idling below it. The pre-fix rows were
unaffected — at 1.84 and 2.29 CPU-seconds they were the slowest end either way,
which is why the deficit this section diagnoses was measured correctly even
while the fix's size was being understated.

**The span path stays off in telnet mode**, where sexyz's IAC state machine
must see every byte. That mode is already off by default for a stdio session,
which is how the BBS invokes sexyz for rlogin, SSH and raw connections.

**A trap this design invites, hit during development.** The first version
guarded the table build with a sentinel field compared against
`escape_ctrl_chars`. `zmodem_init()` memsets the struct, so both were 0, the
guard never fired, the table stayed all-zero, every byte looked special, and the
span path returned nothing — pure overhead, measuring **24 % slower** while
still verifying byte-identical. The memset default that makes the callback
optional also makes 0 mean both "unset" and a legitimate value. Building the
table eagerly removes the sentinel and the trap.

**Error recovery: gated at the §3.3 rate, in both configurations.** Injecting
bit errors at 3e-6 into an 8 MB transfer, **5 of 5** runs pass for sexyz's span
path, for `zrx_buf`'s span path, and for `zrx_buf` with `recv_span` left
`NULL` — the last being what a consumer that has not adopted the callback
runs, which is the configuration the optional-callback decision obliges us to
keep testing. The canonical sender-side gate passes **3 of 3** as well:
`zmodem_rx()` is shared, so the plain-byte table changes how a *sender* reads
its back-channel too, even though nothing in the send path was touched.

That gate only became usable once §5.2 was fixed. Before that the
**unmodified** receiver failed it outright, for a reason unrelated to this
work, so there was no baseline to compare a change against.


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

### 5.2 A receive could not survive more errors than max_errors, ever (FIXED, zmodem.c 2.7)

Found while trying to gate the §3.5 receive work: at a bit-error rate of 3e-6
into an 8 MB transfer, **sexyz's receiver failed 0 of 2 runs** where `zmrx`
2.02 succeeded 2 of 2 (taking ~30 s to do it) and `lrz` managed 1 of 2. sexyz
gave up in about a second. The §3.3 gate was unusable as a result — the
unmodified baseline could not pass it, so there was nothing to compare a
change against.

**It was not the resynchronisation.** The receiver detects the bad subpacket,
logs `CRC ERROR`, sends ZRPOS, chews through the in-flight remainder emitting
`UNRECOGNIZED header` for each false start, and comes back in sync. That
sequence works, and the log noise it produces is expected. Two hypotheses that
looked obvious were tested and are **wrong**: bounding the sender's in-flight
backlog (`--sockbuf 8192`) does not help, and neither does dropping the link to
200 KB/s, so it is not the queue-depth effect of §3.3.1.

**`zmodem_recv_file_data()` counted errors for the lifetime of the file.**
`unsigned errors = 0` once per file, `errors++` per failure, break at
`errors > max_errors` — with **no reset on success**. At 3e-6 an 8 MB file
takes about 25 independently-corrupted subpackets, so the transfer died at the
tenth even though every one of them had been recovered and the file was
progressing normally between them.

The sending side never had this bug: `zmodem_send_from()` counts
`zm->consecutive_errors` and clears it on success. The receive path simply
never got the same treatment — the pattern was already in the file.

**Consequence, and why it matters more than the throughput work.** Any link
with a non-zero error rate had a file size past which a receive could not
succeed, no matter how healthy the link was between errors. The bigger the
file, the likelier the abort. This is a real-world dial-up and noisy-line
failure, i.e. exactly the regime where the §3.5 speed work is irrelevant.

**Fixed** by clearing the count once the file position has advanced beyond
where the last error was recorded. A run of errors with no progress between
them still stops at `max_errors`, so a genuinely wedged transfer ends as
before. The same 3e-6 test now completes **3 of 3**, verified, with the counter
returning to `ERROR #1` after each of the ~25 recoveries.


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

6. **✅ DONE (zmodem.c 2.7, sexyz.c 3.6) — `zmodem.c`'s per-byte receive cost
   (§3.5).** Shipped as an optional `recv_span` callback plus a shared
   plain-byte table: sexyz receives at **486.5 MB/s / 0.55 CPU-s**, up from
   117.2 / 2.29, 1.70× `zmrx` and 2.3× `lrz`. **SyncTERM needs no source
   change** — the callback defaults to `NULL` — and the table alone lifts the
   un-adopted path to 145.7 MB/s. Original analysis retained:

   **○ OPEN — `zmodem.c`'s per-byte receive cost (§3.5).** This is an **engine**
   item, not a sexyz one: `zmodem.c` behind a minimal transport (`zrx_buf`)
   receives at **128.2 MB/s** against `lrz`'s 208.4 and `zmrx`'s 281.6, and
   **76 % of sexyz's excess receive CPU is inside it**. **SyncTERM shares all of
   it**, and 128.2 is an upper bound on SyncTERM's receive, not a measurement of
   it (§3.5, "What this does and does not say about SyncTERM"). The fix is
   **batching**, in this order:
   (a) give `zmodem_recv_data32` a **span reader** so unescaped runs come out of
   the transport buffer in one call and only a real ZDLE drops to per-byte —
   `zmrx`'s `rx()`/`rx_slow()` split, `lrz`'s `zdlread`/`zdlread2` split, and
   where the 4× instruction gap and 8× load traffic live. The span fetch is an
   **optional callback added as a `zmodem_t` struct field, not a
   `zmodem_init()` parameter**: `zmodem_init()` memsets the struct, so it is
   `NULL` for every existing consumer with no source change and `NULL` keeps
   today's per-byte path. Consumers opt in independently — **SyncTERM is not
   required to adopt it** — at the cost that both paths stay live, so the §3.3
   gate must pass in both;
   (b) **slicing-by-4 CRC on receive** (`ucrc32_4`), which the send path has had
   since 2026-07-24 and which (a) makes possible — `zmrx` did exactly this in
   `75a598b` (diff-27-each, 2026-08-24), ten lines, and `zmodem_recv_data32`
   already accumulates into `p`.
   Run both against the §3.3 error-recovery gate — a span optimisation that
   looks free can regress recovery, as it did on the send side.
   **Do not start with the obvious-looking micro-fix:** hoisting
   `is_connected`/`is_cancelled` out of the per-byte loop condition
   (`zmodem.c:803`) is worth ~0.5 %, measured, not the 2–3 indirect branches per
   byte it appears to remove. Invisible below ~1 Gbps, so this is a LAN/loopback
   concern, not a dial-up one.

None of these lift the 4 GB wire ceiling, which is inherent to ZMODEM.
