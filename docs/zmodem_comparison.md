# ZMODEM implementation comparison: sexyz vs. lrzsz vs. zmtx/zmrx vs. Forsberg rzsz

**Measured:** 2026-08-29, one host. Every implementation at the current
release, including zmtx/zmrx **2.04**, which landed four days earlier and
changes several conclusions a 2.02 checkout would have produced.
**Author:** Claude (analysis commissioned by Rob Swindell)
**Scope:** Feature support, interoperability, error recovery, throughput and CPU
cost of four ZMODEM implementations, **in both directions** — plus Forsberg's
commercial DSZ as a fifth, for the negotiated option bits only (§7.4). ZMODEM
only; sexyz's X/YMODEM support is verified separately in
`xymodem_verification.md`.

This is a full re-measurement and supersedes every figure in earlier revisions
of this document. Those accumulated across five sessions between 2026-07-23 and
2026-08-25 and compared components at versions no longer shipped; nothing below
is carried forward from them.

---

## 0. Summary

**Every pair interoperates.** All twenty sender/receiver combinations transfer
256 MiB byte-identically on a clean link. Wire overhead is identical to two
decimal places at the same block size. There is no compatibility problem here at
the level a user would meet.

**The differences are cost and robustness, and the two do not agree.** The
fastest sender has the weakest error recovery and a broken escaping mode.

| | Best | Worst | Ratio |
|---|---|---|--:|
| Send cost (CPU-s / 256 MiB) | `zmtx` 0.38 | Forsberg `sz` 2.76 | 7.3× |
| Receive cost (CPU-s / 256 MiB) | `zmrx` 0.62 | `lrz` 1.28 | 2.1× |
| Peak pair goodput | `zmtx` → `zmrx` 514.2 MB/s | Forsberg → any ~97 MB/s | 5.3× |
| Error recovery, 8 MiB @ 3e-6 | **sexyz 10/10 both ways** | `zmtx` 0/5 | — |

Seven specifics:

1. **zmtx/zmrx 2.04 is the cheapest implementation in both directions, by
   roughly a factor of two** — 10.0 instructions per data byte sending and 12.1
   receiving, against the next best of 23.5 either way. Its 2.03 release
   optimised both data paths; at 2.02 the same code cost 15.2 and 34.8. Any
   comparison against zmtx/zmrx older than 2026-08-25 is obsolete.
2. **sexyz's receiver is second, and much improved** — 0.68 CPU-seconds per
   256 MiB and 24.6 instructions per byte, against `lrz`'s 1.28 and 24.1 and
   `zmrx`'s 0.62 and 12.1. Eight days earlier the same code cost 2.35
   CPU-seconds and 126.8 instructions per byte and was the slowest of the four.
   That work is in the shared engine, so SyncTERM has it.
3. **sexyz has the most robust error recovery measured** — 10 of 10 sending and
   10 of 10 receiving at an error rate where the all-lrzsz pair manages 7 of 10
   and `zmtx` 0 of 5 (§6.1).
4. **`zmtx` still cannot finish a transfer over a lossy link**, at the fastest
   code in this comparison. 2.04 improved this — its retry budget now resets on
   forward progress where 2.02's never did — but only on the ZRPOS path; NAK-
   and timeout-driven recovery still drains it monotonically, and the transfer
   fails at any rate at or above 5e-7 (§6.2).
5. **`zmtx`'s ESCCTL support is broken**, in 2.04 as in 2.02 (§7.2), in exactly
   the way Synchronet's was until 2026-08-24. A receiver that requests
   control-character escaping cannot download from `zmtx` at all.
6. **The ESC8 option has no agreed encoding, and sexyz no longer asks for it.**
   Running Forsberg's own 1997 DSZ under DOSBox settles what the bit means —
   `rz -E` sets it — and that the only two implementations of it encode it
   differently: DSZ prefixes with `0x0E` and clears bit 7, `zmrx` 2.04 uses
   `ZDLE, c ^ 0x40`. They cannot talk to each other, because the spec never
   said which (§7.4). sexyz had advertised the mode while implementing neither
   half of it; the option was removed on 2026-08-30 (GitLab #1229). With or
   without it, that same DOS binary transfers to sexyz byte-identically.
7. **Instruction counts do not order CPU time, and neither orders goodput.**
   `lrz` executes fewer instructions per byte than `zmrx` (24.1 vs 34.8) and
   costs 36 % more CPU. sexyz's sender executes 2.4× `lsz`'s instructions and
   still beats it on wall clock, by using a second thread. Quote all three
   numbers or none.

---

## 1. What is under test

### 1.1 Versions

| Implementation | Version | Provenance |
|---|---|---|
| **sexyz** | v3.6 — `sexyz.c` 3.6, `zmodem.c` 2.7, `xmodem.c` 2.0 | Synchronet `master/~d3aba361a6`, GCC 14.2.0, Debian 13 |
| **lrzsz** (`lsz`/`lrz`) | 0.12.21rc | Uwe Ohse's maintained fork of Forsberg's rzsz |
| **zmtx/zmrx** | **2.04** (`3a5d2ed`, tag `v2.04`, 2026-08-25) | Jacques Mattheij, 1994; currently maintained by Stephen Hurd |
| **Forsberg rzsz** | `sz` 3.73, 2003-01-30 | Omen Technology; built from the history repo's `modern` branch |
| **Forsberg DSZ** (§7.4) | DSZ.EXE 1997-05-25 | Omen Technology's *commercial* DOS product; run under DOSBox, not throughput-benchmarked |

`sexyz v` prints all three component versions, which is how a build is
identified here; the other three print one.

Synchronet's `zmodem.c` is shared with **SyncTERM**, so every engine-level
result applies to SyncTERM's built-in transfers too. SyncTERM itself is not
benchmarked — it cannot be driven headlessly — and its real-world speed is
additionally a deliberate socket-buffer choice rather than an emergent property
of the engine.

### 1.2 Two instrumented tools that are not products

`ztx_buf` and `zrx_buf` (`src/bench/zmodem/`) are ~150-line senders and
receivers linking the **real `zmodem.o`** behind a minimal buffered transport:
no ring buffer, no threads, no telnet-IAC handling. They separate "what the
shared engine costs" from "what sexyz's transport wrapper costs", which is the
only way to get a SyncTERM-relevant number.

They are **not** throughput models of anything shipped, and their error handling
is minimal by design (§6.3). Rows using them are marked.

---

## 2. Method

### 2.1 Harness

`zbench_sock.py` (`src/bench/zmodem/`) wires a sender and a receiver — each
speaking ZMODEM on stdin/stdout — through a userspace relay that can inject
one-way latency, a per-direction bandwidth cap, and forward bit corruption. No
root or `netem` needed. Both endpoints are reaped with `wait4()`, so every run
reports each end's user/system CPU and context switches beside goodput, plus a
SHA-256 comparison of source against received file.

- **Transport:** localhost sockets, files on tmpfs, so neither disk nor network
  confounds a CPU-bound measurement. The relay's own ceiling is ~5 GB/s, far
  above any implementation here.
- **Payload:** 256 MiB of `/dev/urandom` for throughput, 8 MiB for error gates,
  4 MiB under the profiler. Random data makes the ZDLE escape rate realistic
  (~2.8 % wire expansion) and exercises all 256 byte values.
- **Flags:** `-8` (8 KiB ZedZap blocks) wherever supported; Forsberg has no
  equivalent and caps at 1 KiB.
- **Passes:** three per pair, interleaved so drift affects every pair equally;
  medians reported.

### 2.2 The fixed end must be the cheap end

When one end is fixed and the other varied, **the fixed end must cost less CPU
than everything under test**, or the table measures the fixed end. This is not a
formality — it invalidated this document's sender table for a month. Every
sender row was scored against `lrz`, which receives for 1.29 CPU-seconds, more
than any sender in the table spends to send; four senders spanning 1.74× in real
cost all reported ~204–209 MB/s and were written up as a tie.

This revision avoids the trap by measuring **the full 20-pair matrix** (§4.1)
instead of fixing either end, and by printing both endpoints' CPU in every row
so the bottleneck is visible. The check is mechanical: if an end's CPU equals
the wall clock, that end is the bottleneck.

### 2.3 The error model, and what a fixed seed means

`--corrupt-rate` applies an independent per-byte bit-flip probability to the
**forward direction only**, from a PRNG seeded with a **fixed constant**. Two
consequences shape how gate results must be read.

The **first** corruption always lands at the same byte offset for a given rate,
so repeated runs at one rate are not independent samples. At 1e-7 on an 8 MiB
transfer the first flip falls past the end of the stream, so every
implementation "passes" because **no error is injected at all**. Verified by
wire size: a 1e-7 run puts exactly as many bytes on the wire as a clean one
(8,624,412), while 2e-7 grows to 8,765,099 as retransmissions occur.

After the first error the runs *do* diverge, because a retransmission changes
the stream length and shifts every later flip. A **rate sweep** is therefore the
meaningful instrument (§6.2), with repetition at a rate used only to catch that
divergence.

### 2.4 Host caveats, and a warning about gates

This host also runs GitLab (with CI runners), the Synchronet BBS, and the Samba
server every other BBS host's I/O flows through. Two effects, both of which bit
during this session:

**Throughput outliers.** In 60 matrix runs exactly one produced a 5× outlier
(sexyz → sexyz at 61.5 MB/s against a 313–330 MB/s median); ten consecutive
repeats of that pair were clean at ~320 MB/s. Medians are used throughout.
Treat single-digit percentage differences as noise; the conclusions here rest on
differences of 1.5× and more.

**Error gates are far more sensitive to load than throughput runs**, because a
recovering transfer depends on timeouts and round trips rather than on
bandwidth. A first pass of §6.1's gates, run while a Callgrind job was still
finishing, scored `lsz` → `lrz` at 0/3 and 2/3 in two identically-configured
invocations of the same pair — while direct runs of that pair complete in 0.48 s
and pass every time. Those results were discarded and the gates re-run on an
idle host with a 150-second protocol timeout. **A gate result from a loaded host
is not evidence.**

Callgrind instruction counts are exact and repeatable; its *timings* are not, so
every profiled figure is paired with a wall-clock/CPU run of the same pair.

---

## 3. Feature and option support

### 3.1 Protocol features

| Feature | sexyz 3.6 | lrzsz 0.12.21rc | zmtx/zmrx 2.04 | Forsberg 3.73 |
|---|---|---|---|---|
| 8 KiB subpackets (ZedZap) | `-8` | `-8` | `-8` | **no** (1 KiB cap) |
| 4 KiB subpackets | `-4` | `-4` | `-4` | no |
| CRC-32 | yes (default) | yes (default) | yes | yes |
| CRC-16 fallback | `-o` | `-o` | — | — |
| Segmented (ack per subpacket) | `-s`, both directions | — | `zmtx -s`, `zmrx -s` | — |
| Window management | `-w#` (send) | `-w N` | `zmtx -w` | `-w N` |
| Control-char escaping (ESCCTL) | `-e`, ini, **both directions** | `-e`, both directions | `zmrx -e` requests; sender honours it but **escaping is broken** (§7.2) | `-e` (sender) |
| 8th-bit escaping (ESC8) | **no** — never requested, never honoured; the `Escape8thBit` key was removed (§7.2) | — | implements it (`zmrx -b`), incompatibly with DSZ | `rzsz` no; **DSZ `-E` yes** (§7.4) |
| File-management option (ZFILE ZF1) | `-y`/`-p`/`-n`, ini `SendManagement` | `-y`/`-p`/`-n`/`-E`/`-Y` | `-o`/`-p`/`-n`, both ends | `-y`/`-p`/`-n` |
| Batch / file lists | multiple, `@list`, `+list` | multiple | multiple | multiple |
| Crash recovery / resume | — | `-r` | — | — |
| ZSINIT (Attn sequence, sender escape declaration) | **never sent; a received one is ACKed and its flags and Attn string discarded** | handled both ways | `zmrx` handles a received one; **`zmtx` never sends** | sends when it has something to declare (observed with `sz -e`) |
| Honours a *sender's* ZFILE management request | **no** — local policy (`-y`) decides | yes (`lrz.c`) | yes (`zmrx.c`) | yes (verified: honoured `-p`) |
| Telnet IAC escaping | `-telnet` | — | — | — |
| Socket-descriptor argument | **yes** | `--tcp-server`/`--tcp-client` | `-l line` | — |
| Minimum-BPS abort | — | `-m` / `-M` | — | — |
| Stop at wall-clock time | — | `-s HH:MM` | — | — |
| Remote command execution | **no** (deliberate) | `lsz -c`/`-i`, `lrz -C` | — | `zcommand` |
| Restricted mode | filename always sanitised | `-R` / `-U` | — | `-R` |

Two entries need expansion.

**Remote command execution.** lrzsz and Forsberg can carry a command in the
ZMODEM stream for the far end to run (`lsz -c`, gated at the receiver by
`lrz -C`). sexyz implements no such thing, which for a BBS is the right answer.

**Filename safety.** sexyz strips the directory component from every
sender-supplied filename, on both `/` and `\` separators, at two independent
points (`zmodem_parse_zfile_subpacket()` and again in `sexyz.c`), and rejects
files not on the receive list. It needs no `-R` equivalent because it does not
have the behaviour `-R` exists to disable.

### 3.2 Where the option sets diverge

lrzsz has the most featureful command line by a distance: it alone offers resume
(`-r`), a minimum-throughput abort (`-m`/`-M`) and a stop-at-time (`-s`). The
first two have no equivalent in any other implementation measured.

The minimum-BPS abort is the gap most relevant to a BBS. sexyz's timeouts are
per-read and reset on every byte received, so a transfer trickling one byte per
second never times out and holds a node indefinitely. lrzsz's `-m`/`-M` exists
for exactly that case.

---

## 4. Throughput

### 4.1 The full matrix

256 MiB, clean localhost, `-8` where supported, median of three interleaved
passes. **Every cell verified byte-identical.** Rows are senders, columns
receivers; `ztx_buf`/`zrx_buf` are the engine-isolation tools of §1.2.

| Sender ↓ / Receiver → | sexyz | `zrx_buf` | `zmrx` | `lrz` |
|---|--:|--:|--:|--:|
| **`zmtx`** | 484.6 | 476.2 | **514.2** | 208.9 |
| **`ztx_buf`** (engine) | 390.9 | 391.6 | 391.6 | 208.7 |
| **sexyz** | 313.3 | 319.0 | 325.9 | 205.1 |
| **`lsz`** | 278.4 | 279.8 | 281.0 | 208.4 |
| **Forsberg `sz`** | 97.1 | 97.1 | 97.3 | 97.0 |

(MB/s. All twenty pairs MATCH.)

### 4.2 Who is the bottleneck

Reading goodput alone from that table is a mistake: each cell is limited by
whichever end is more expensive. The per-endpoint CPU makes the structure plain:

| End | Cost per 256 MiB | Reading |
|---|--:|---|
| `zmtx` sending | 0.38 CPU-s | never the bottleneck — its whole row measures the receiver |
| `ztx_buf` sending | 0.69 | bottleneck only against sexyz / `zrx_buf` / `zmrx` |
| `lsz` sending | 0.96 | bottleneck except against `lrz` |
| sexyz sending | 1.08 | bottleneck except against `lrz` |
| Forsberg sending | 2.76 | bottleneck against everything — hence a flat row |
| `zmrx` receiving | 0.62 | bottleneck only under `zmtx` |
| sexyz receiving | 0.68 | bottleneck only under `zmtx` |
| `zrx_buf` receiving | 0.68 | same |
| `lrz` receiving | 1.28 | bottleneck under everything but Forsberg |

The `zmtx` row is therefore the cleanest measurement of the four receivers, and
the `lrz` column the least informative of the five senders: four of them land
within 1.8 % of each other there, which is `lrz`'s ceiling, not a property of
any sender. `zmtx` → `zmrx` at 514.2 MB/s is the only pair in the matrix where
both ends are from the same, cheapest implementation, and it is the fastest.

Forsberg's row is flat at ~97 MB/s because at 2.76 CPU-seconds it is slower than
every receiver. Its 1 KiB subpacket cap, not its code quality, sets that number.

### 4.3 Wire overhead

Identical to two decimal places across implementations at the same block size:
**+2.81 % to +2.82 %** for 8 KiB subpackets, **+2.55 %** for Forsberg's 1 KiB.
That is the ZDLE escape rate on random data plus framing. Protocol efficiency is
not a differentiator; every difference in this document is implementation cost.

### 4.4 There is no per-file cost anywhere

Five 6 KiB files in one batch over a simulated 115200 bps line — the workload in
which a fixed per-file cost dominates — took **2.23 s and 32,131 wire bytes on
all three receivers**, identically. 32,131 ÷ 14,400 B/s = 2.23 s: the link is
the entire story.

This check is repeated here because a per-*file* cost is invisible in a
single-file goodput figure by construction, the metric being built to divide
fixed costs away. That blind spot hid a one-second-per-file stall in sexyz's
sender for months, found by Uwe Ohse in August 2026 and since fixed.

---

## 5. CPU

### 5.1 Cost per endpoint

CPU-seconds to move 256 MiB, median across every partner:

| Sender | CPU-s | MiB/CPU-s | | Receiver | CPU-s | MiB/CPU-s |
|---|--:|--:|---|---|--:|--:|
| **`zmtx`** | **0.38** | 674 | | **`zmrx`** | **0.62** | 413 |
| `ztx_buf` (engine) | 0.69 | 371 | | sexyz | 0.68 | 376 |
| `lsz` | 0.96 | 267 | | `zrx_buf` (engine) | 0.68 | 376 |
| sexyz | 1.08 | 238 | | `lrz` | 1.28 | 200 |
| Forsberg `sz` | 2.76 | 93 | | Forsberg `rz` | — (§7.3) | — |

sexyz and `zrx_buf` tie exactly on the receive side. That is the expected result
and a useful check: sexyz's receive transport adds nothing measurable over a
minimal one, so essentially all of the receive cost is the shared engine.

`zmrx` 2.04 is cheaper than sexyz in every column of §5.3, not just on the
median — 0.52 against 0.56 behind `zmtx`, 0.62 against 0.68 behind `lsz`, 1.18
against 1.30 behind Forsberg. At 2.02 it cost 0.94 to 0.98 and was the more
expensive of the two by a third.

The send side is the opposite. sexyz costs 0.39 CPU-s more than `ztx_buf`
driving the same engine; that difference is the ring buffer and drain thread,
and it is why sexyz is the most expensive sender here in CPU terms while still
beating `lsz` on wall clock (§5.2).

### 5.2 Instructions per byte

Callgrind, 4 MiB transfers, instructions per **data** byte:

| Receiver | instr/byte | | Sender | instr/byte |
|---|--:|---|---|--:|
| **`zmrx`** | **12.1** | | **`zmtx`** | **10.0** |
| `zrx_buf` (engine) | 23.5 | | `lsz` | 23.5 |
| `lrz` | 24.1 | | `ztx_buf` (engine) | 39.7 |
| sexyz | 24.6 | | sexyz | 56.1 |

Three things this shows that the CPU table does not.

**zmtx/zmrx 2.04 is in a different class**, at 10.0 and 12.1 instructions per
byte where nothing else is below 23.5. Its 2.03 release rewrote both data paths;
the same binaries at 2.02 measured 15.2 and 34.8, so the receive path in
particular improved by 2.9× in four days.

**The shared engine's receive path now costs 23.5 instructions per byte, below
`lrz`'s 24.1** — where a week earlier the same code cost 126.8. sexyz's 24.6 is
that plus about 1.1 for its transport. That is second place, not first.

**Instruction count does not order CPU time.** `lrz` executes 24.1 instructions
per byte and costs 1.28 CPU-s; sexyz executes 24.6 — slightly more — and costs
0.68, nearly half. `lrz`'s inline state machine is branch-heavy and mispredicts
where the table-driven paths do not. Optimising by instruction count alone would
make a receiver slower in order to make it look better.

**sexyz's sender wins on threads, not efficiency.** It executes 56.1
instructions per byte to `lsz`'s 23.5 and spends more total CPU (1.08 vs 0.96),
yet finishes sooner in wall clock by spreading that across a producer and a
drain thread (~65,000 context switches per transfer). The engine's own
single-threaded cost is `ztx_buf`'s 39.7 instructions at 0.69 CPU-s — cheaper
than `lsz` in CPU-seconds despite 1.7× the instructions, again because
instructions retire at very different rates.

### 5.3 A receiver's cost is not a property of the receiver alone

The same receiver, driven by different senders, costs materially different
amounts of CPU per 256 MiB:

| Receiver ↓ / driven by → | `zmtx` (8K) | `ztx_buf` (8K) | `lsz` (8K) | sexyz (8K) | Forsberg (1K) |
|---|--:|--:|--:|--:|--:|
| `zmrx` | **0.52** | 0.61 | 0.62 | 0.68 | 1.18 |
| sexyz | 0.55 | 0.64 | 0.68 | 0.74 | **1.30** |
| `zrx_buf` | 0.56 | 0.65 | 0.68 | 0.74 | 1.41 |
| `lrz` | 1.28 | 1.28 | 1.28 | 1.31 | 1.92 |

Every receiver costs more behind Forsberg's 1 KiB subpackets, but the spread
differs: sexyz ranges 2.4× (0.55 → 1.30) and `zmrx` 2.3× (0.52 → 1.18), where
`lrz` ranges 1.5× (1.28 → 1.92). sexyz's receive path takes whole runs of unescaped bytes per
call, so larger arrivals mean longer runs and fewer wakeups; it gains most from
a well-behaved sender and loses most to a chatty one. The receiver is identical
in every column — only its input granularity changes.

So sexyz receives for 0.55 CPU-seconds or 1.30 depending on who is sending, and
any single figure for a receiver needs its sender named.

---

## 6. Reliability under injected errors

### 6.1 Gate results

8 MiB, forward-direction bit corruption at 3e-6 per byte, three runs each, on an
idle host (§2.4). One end is held at a known-good partner so a failure is
attributable: senders are tested against `lrz`, receivers against `lsz`.

| Pair | Passes | |
|---|--:|---|
| **sexyz sending** → `lrz` | **10 / 10** | |
| `lsz` → **sexyz receiving** | **10 / 10** | |
| `lsz` → `lrz` (both lrzsz) | **7 / 10** | the reference pair, at the same rate |
| `lsz` → `zmrx` | 5 / 5 | |
| `lsz` → `zrx_buf` (engine) | 3 / 3 | |
| Forsberg `sz` → `lrz` | 3 / 5 | marginal; failures abort after ~10 s |
| **`zmtx`** → `lrz` | **0 / 5** | structural — §6.2 |
| `ztx_buf` (engine) → `lrz` | 0 / 3 | instrument limitation — §6.3 |

**sexyz recovers more reliably than lrzsz does.** With ten passes each, sexyz
passes every run in both directions while the all-lrzsz pair loses three. Since
each sexyz row has lrzsz at the other end, putting sexyz on either end of the
pair improved its outcome.

Read the smaller samples with care. The corruption model is chaotic after the
first error (§2.3), so among implementations that *do* recover, a 2/3 or 3/5 is
not meaningfully different from a 3/3 — Forsberg scored 3/3 on a loaded host,
0/3 on an idle one and 3/5 in direct repeats of the same command. The only
result here that is a category rather than a sample is `zmtx`'s, and §6.2 shows
the code that produces it.

### 6.2 `zmtx`: a retry budget that resets on only one of three paths

`zmtx` fails at any rate that reliably injects errors, in 2.04 as in 2.02, and
scores 0 of 5 at the gate rate. A rate sweep to `lrz`, both releases, same file:

| Rate | `zmtx` **2.02** → `lrz` | `zmtx` **2.04** → `lrz` |
|---|---|---|
| 2e-7 | MISSING — 8,985,617 | **MATCH** — 8,979,003 |
| 5e-7 | MISSING — 1,455,347 | MISSING — 1,987,502 |
| 1e-6 | MISSING — 1,555,499 | MISSING — 2,136,796 |
| 3e-6 | MISSING — 1,168,053 | MISSING — 1,615,751 |

(`lsz` and sexyz complete every one of those rates; see the gate table above.)

2.04 is measurably better — it completes 2e-7 where this run of 2.02 did not,
and at every failing rate it moves 30–40 % more data before giving up — but the
outcome above 2e-7 is unchanged. It also now says why, which 2.02 did not:

```
zmtx: file transfer retries exhausted: unexpected protocol response
```

2.02 exited 4 (`EXIT_TRANSFER_FAILED`) silently; reporting failures by default
arrived in the same release series.

The remaining cause is visible in `zmtx.c`'s send loop. 2.04 added a watermark
and the comment *"Only recovery requests without net progress consume the retry
budget"*:

```c
attempts = 0U;
while (attempts < MAX_RETRIES) {          /* MAX_RETRIES 10 */
        ...
        if (type == ZRPOS) {
                if (requested > furthest_recovery_position) {
                        furthest_recovery_position = requested;
                        attempts = 0U;    /* progress: budget restored */
                } else  attempts += 1U;
                ...
        }
        if (type == ZNAK)    { attempts += 1U; ... continue; }   /* no reset */
        if (type == TIMEOUT) { attempts += 1U; ... continue; }   /* no reset */
```

The reset covers the **ZRPOS** path only. Recovery driven by a NAK or a timeout
— which is what a corrupted *header* produces, as opposed to a corrupted data
subpacket — still consumes the budget permanently, however much progress the
transfer has made since. Ten such events anywhere in a file still end it, so a
file size beyond which the transfer cannot complete still exists; it is merely
larger than it was.

Synchronet's *receiver* carried the same defect until 2026-08-24, where
`zmodem_recv_file_data()` counted errors for the lifetime of a file and aborted
at the tenth. Its fix resets the count whenever the file position advances,
independent of which protocol event caused the error — which is the difference
between the two fixes.

### 6.3 What the gate does not cover

A 3 of 3 is evidence, not proof. The corruption model is chaotic after the first
error (§2.3), and gate results are load-sensitive (§2.4). What the gate reliably
separates is *categories*: implementations that recover and occasionally lose a
race, versus implementations that structurally cannot finish (§6.2).

One pairing outside those categories: **`ztx_buf` → `zmrx` hangs** under
corruption, with `ztx_buf` streaming on to 5.7 MB while `zmrx` goes silent, until
the harness kills it. `ztx_buf` is the minimal test sender of §1.2 — it has no
window handling and no receiver-silence detection — so this is a limitation of
the instrument, not a `zmrx` defect. `zmrx` driven by `lsz` recovers cleanly.
Recorded so it is not rediscovered as a finding.

---

## 7. Compatibility

### 7.1 Every pair interoperates on a clean link

All twenty sender × receiver combinations of §4.1 completed and verified
byte-identical, including every cross-implementation pair and both
engine-isolation tools. No negotiation failures, no fallbacks, no
implementation-specific workarounds.

Forsberg's DSZ interoperates with sexyz too, across sexyz's whole ZMODEM option
set and in both directions — §7.4 has the option-by-option table. The single
exception anywhere in this document is ESC8 (§7.2).

### 7.2 ESCCTL: partial support, and one broken implementation

ZMODEM's ESCCTL mode has the receiver ask, in its ZRINIT, that the sender escape
all control characters — for links that eat them. Support is uneven:

| Implementation | As receiver (can request) | As sender (honours a request) |
|---|---|---|
| sexyz 3.6 | **yes** (`-e`, ini `EscapeCtrlChars`) | **yes** |
| lrzsz | yes (`lrz -e`) | yes (`lsz -e`, and on request) |
| Forsberg | receiver untestable (§7.3) | yes |
| zmtx/zmrx 2.04 | yes (`zmrx -e`) | reads the flag, **escaping is broken** |

Verified transfers, 4 MiB, against a ~4.31 MB unescaped baseline:

| Pair | Result |
|---|---|
| sexyz `-e` → `lrz -e` | MATCH, wire 5,245,034 |
| `lsz -e` → sexyz `-e` | MATCH, wire 5,245,032 |
| sexyz `-e` → sexyz `-e` | MATCH, wire 5,245,034 |
| `lsz -e` → `lrz -e` | MATCH, wire 5,245,032 |
| Forsberg `sz` → sexyz `-e` | MATCH, wire 5,286,333 |
| **`zmtx` 2.04 → sexyz `-e`** | **MISSING**, aborted at 766,470 |
| **`zmtx` 2.04 → `lrz -e`** | **MISSING**, aborted at 62,399 |
| sexyz → `zmrx -e` | MATCH, wire 5,245,034 |
| `lsz` → `zmrx -e` | MATCH, wire 5,245,032 |
| `zmtx` 2.04 → `zmrx -e` | MATCH, wire **5,212,630** — see below |

sexyz's escaped wire is within 2 bytes of lrzsz's for the same file, so the two
escape the same byte set; random data exercises all 256 values, so nothing is
escaped by luck.

`zmtx` fails against both *external* ESCCTL receivers, so this is not a
Synchronet interop quirk — but it passes against its own `zmrx -e`, and the wire
size says why. Where sexyz and `lsz` put 5,245,03x bytes on the wire for that
file, `zmtx` puts 5,212,630 — about **32,400 bytes short**, which is the count
of unescaped CR and 0x8D bytes in 4 MiB of random data. `zmtx` under-escapes and
`zmrx` under-enforces, in matching measure: the two agree with each other and
with no one else. A receiver that drops unescaped control characters, as the
ESCCTL contract requires and as both sexyz and `lrz` do, sees corruption.

**ESC8, the 8th-bit equivalent, is implemented by exactly one of the four.**
zmtx/zmrx 2.04 does it at both ends: `zmtx → zmrx -b` transfers correctly with
genuine escaping, the wire growing from 4,311,816 to **6,360,011** bytes for the
same 4 MiB file. Neither lrzsz nor sexyz escapes when a peer requests it —
`lsz → zmrx -b` succeeds only by putting the unescaped 4,311,816 bytes on the
wire.

**sexyz used to ask for the mode without being able to decode it, and no longer
does.** Until 2026-08-30 an `Escape8thBit=true` in `sexyz.ini` advertised ESC8
in the ZRINIT while the receiver had no decode path for it: an escaped high-bit
byte (`0xA5` sent as `ZDLE 0xE5`) failed the `(c & 0x60) == 0x40` test and was
logged as `Illegal sequence: ZDLE 229` until the session timed out, so
`zmtx → sexyz` could not start a transfer at all. The key and the advertisement
are both gone (GitLab #1229); sexyz never sets the bit now, and ignores a remote
receiver's request rather than refusing it, there being no way to decline a
ZRINIT capability — which is what lrzsz does too. With that removed,
`zmtx → sexyz` completes byte-identically.

Chuck Forsberg defined `ESC8`/`TESC8` in his `zmodem.h` and never referenced
either in any `.c` file; lrzsz inherited the same two dead defines. The option
therefore had no working peer anywhere for 21 years, which is why sexyz
advertising it was harmless until 2026-08-25. The cause is in `zmdm.c`:

```c
return TX_ESCAPE_ALWAYS |
    (zmodem->escape_all_control_characters ?
     TX_ESCAPE_CONTROL | TX_ESCAPE_CR : 0U);
...
if (action == TX_ESCAPE_CR) { return previous == '@'; }
```

`TX_ESCAPE_CR` is enabled *only* when ESCCTL is negotiated, and carriage return
is then escaped only when the previous byte was `'@'`. So under ESCCTL — and
only under ESCCTL — `zmtx` puts raw CRs on the wire, which an ESCCTL receiver
discards as unescaped control characters by definition, failing the CRC of every
subpacket containing one. On random data that is nearly every subpacket.

lrzsz shows what the rule should be (`zsendline_init()`, `zm.c`): for `015`,
`if (Zctlesc) tab = 1` — always escape — while the conditional `'@'` rule is the
*non*-ESCCTL case. The two are swapped.

**This is still present in 2.04.** The 2.03 commit `7d2604c`, "Fix negotiated
ZMODEM escaping", touches this exact function — but it adds *8th-bit* escaping
(`TX_ESCAPE_8TH`) and leaves the CR rule untouched. The title refers to ESC8,
not ESCCTL.

**Synchronet's `zmodem.c` had this identical bug**, fixed 2026-08-24 by folding
CR into the control class. The shared history is direct: Synchronet's send path
received this table-driven classifier in July 2026, and `zmtx` received the same
optimisation in commit `71f2f7b`, "Optimize ZMODEM data transmission", dated
**2026-08-24**. The Synchronet fix transfers to `zmdm.c` unchanged.

### 7.3 Forsberg's receiver cannot be benchmarked headlessly

Forsberg's `rz` does not complete a transfer in any headless transport tried —
socketpair, pty, or `socat`-bridged ptys, from either `lsz` or Forsberg's own
`sz`, with or without a rate cap. It handshakes and then stalls, and segfaults
under `socat`'s pty. The 2003 tarball is worse, segfaulting immediately on a
socketpair; the `modern` branch was required to get even the sender running.
Only Forsberg's **sender** appears in this document. That fragility outside a
real serial tty is itself a finding — it is the kind of thing that motivated the
maintained lrzsz fork.

---

### 7.4 A fifth implementation, for the negotiated options: Forsberg's DSZ

Forsberg's **commercial** line (DSZ, GSZ, ZCOMM, Pro-YAM) is not in the
throughput tables — it is a 16-bit DOS product with no source, so it cannot be
built or profiled here. It is invaluable for one thing the open implementations
cannot settle on their own: **what the negotiated option bits were actually
meant to do**, from the author of the protocol.

DSZ.EXE 1997-05-25 — the last release, and the EXE rather than the COM, because
the 7-bit options and `-Q<string>` quoting are EXE-only — runs under DOSBox with
its COM1 bridged to TCP (`serial1=nullmodem`) into a relay that pipes it to a
local ZMODEM program. (DSZ needs its `d` command or it exits silently: DOSBox
never asserts carrier.)

**The base protocol interoperates perfectly.** A 4096-byte random file sent by
this 1997 DOS binary arrives byte-identical at both sexyz and `zmrx`.

**On the option bits, its ZRINIT is the reference:**

| DSZ command | ZF0 | Flags |
|---|--:|---|
| `rz` | 0x2F | CANFDX, CANOVIO, CANBRK, **CANRLE**, CANFC32 |
| `rz -e` | 0x6F | the same **+ ESCCTL** |
| **`rz -E`** | **0xAF** | the same **+ ESC8** |
| `rz -P` | 0x2F | (Pack-7 negotiated some other way) |

So **ESC8 is the negotiation bit for "8th bit quoting"**, which DSZ.DOC
describes as half of ZMODEM-90's default 7-bit mode — "RLE compression and 8th
bit quoting" — and `-E` is what turns it on. CANRLE being set unconditionally
matches the other half: RLE *is* implemented in Forsberg's free source
(`zmr.c`, `ZRESC`, frame types `ZBINR32 'D'` / `ZVBINR32 'd'`), while the
8th-bit-quoting half never was, in any release from 1987 to 2003.

**And the two ESC8 implementations in existence do not interoperate.** Asked for
ESC8 by `zmrx -b`, DSZ acts on it — bytes with bit 7 set fall from 47.7 % of the
wire to 3.4 %, and the residue is one repeated 3-byte sequence rather than data
— but it also switches to frame type **`0x31` (`'1'`)**, which is not among the
eight types any published `zmodem.h` defines (`ZBIN 'A'`, `ZHEX 'B'`,
`ZBIN32 'C'`, `ZBINR32 'D'` and the lowercase `ZVBIN` variants). `zmrx` cannot
parse it: *"can't establish contact with sender"*.

**DSZ's encoding, recovered by known-plaintext capture:** a `0x0E` (SO) prefix
followed by the byte with bit 7 cleared, with ordinary ZDLE escaping of control
characters continuing alongside it. In a capture of DSZ sending a 4096-byte
random file, `0x0E` occurs 4,991 times and is followed by a byte below 0x80 in
100 % of them, never doubled, successors spanning 0x01–0x7E; decoding on that
rule recovers 92 contiguous bytes of the known plaintext and restores the
stream from 0.0 % high-bit to 45.6 % (random data is ~50 %). That is exactly
DSZ.DOC's "8th bit quoting similar to Kermit", Kermit being where prefixing
comes from.

So the two implementations use unrelated mechanisms for the same negotiated
mode:

| | Encoding of a high-bit byte `c` |
|---|---|
| **DSZ** (Omen, 1997) | `0x0E`, then `c & 0x7F` — a Kermit-style prefix |
| **zmtx/zmrx 2.04** (2026) | `ZDLE`, then `c ^ 0x40` — ZMODEM's own escape |

Neither is wrong, because the specification never said. ZMODEM.DOC names ESC8
exactly once operationally and never states how a high-bit byte is escaped,
where ESCCTL gets a full paragraph. Two implementers 29 years apart read that
one line and built different things, and one of them also switched to an
unpublished frame type.

(Method, for anyone revisiting: disassembly is the obvious route and the wrong
first one. DSZ.EXE is a plain unpacked MZ image, so it disassembles fine, but
the escape table is not in the file — DSZ builds it at runtime into BSS,
exactly as Forsberg's free `zsendline_init()` does, so scanning for a 256-byte
class table finds nothing. Running the binary against a deliberately permissive
receiver with a known-plaintext file settled it in three runs. Disassembly
remains the only way to learn what frame type `0x31` means.)

sexyz sat in a third position, worse than either — *advertising* ESC8 while
implementing neither half (§7.2, GitLab #1229) — until the option was removed on
2026-08-30. Precisely: sexyz
no longer sets the ESC8 bit in its ZRINIT, so nothing will ever send it an
8th-bit-escaped stream; and when a remote receiver asks *sexyz* for the mode,
sexyz records the request for its log and then ignores it, sending unescaped.
It does not refuse — ZMODEM has no way to decline a ZRINIT capability — and
ignoring is what lrzsz does with the same request.

#### Option-by-option compatibility with DSZ

Everything else interoperates. Each row is a real transfer of an 8 KiB random
file between sexyz and DSZ.EXE 1997 under DOSBox, verified byte-for-byte.

**sexyz sending, DSZ receiving** — the BBS download case:

| sexyz option | Result |
|---|---|
| default | identical |
| `-8`, `-4`, `-2` (block size) | identical |
| `-o` (CRC-16) | identical |
| `-s` (segmented) | identical |
| `-w4096` (transmit window) | identical |
| `-e` (ESCCTL) | identical |
| `-l` (lowercase names) | identical |
| `-y` (clobber) | overwrote the existing file |
| `-p` (protect) | **DSZ kept its existing file** — the request is honoured |
| `-n` (newer) | source dated 2020 → **skipped**; dated 2030 → transferred |

**DSZ sending, sexyz receiving:**

| DSZ option | Result | Wire |
|---|---|--:|
| default | identical | 8,626 |
| `-m` (MobyTurbo) | identical | 8,626 |
| `-Q@` (per-character escaping) | identical | 8,698 |
| `-e` (ESCCTL) | identical | 10,557 |

Two of those are worth pulling out. The **file-management options added to sexyz
in August 2026 work against a third-party implementation** — `-p` and `-n` both
produce the documented behaviour at the far end, and `-n` only demonstrates
itself once the timestamps are forced apart, since FAT's two-second granularity
makes a same-minute comparison ambiguous. And **ESCCTL works in both
directions**, which is the path that carried the carriage-return defect until
2026-08-24.

DSZ's 7-bit options are receiver-driven, which is worth stating because the
names suggest otherwise. `DSZ sz -E` and `DSZ sz -P` sending to sexyz both
transfer identically and leave 46.9 % of the wire carrying bytes with bit 7
set — the same as with no option at all. They request the mode when DSZ
receives; they do not impose it when DSZ sends. So a sender's `-E` is inert
against a receiver that does not ask, and sexyz never asks.

**Not tested, and not claimed:** `-m` (a local size limit with no wire
content), the X/YMODEM modes (`-k`, `-c`, `-g`), and `-telnet` (needs IAC
injection).

**One capability sexyz leaves on the table.** DSZ advertises `CANRLE` in every
ZRINIT it sends (§7.4's flag table), and implements RLE in both directions;
sexyz never advertises it, so the compression is never used. On a BBS serving
DSZ-based terminals that is throughput given away on compressible files, for a
feature the far end already has.

## 8. Findings

**For Synchronet.** The receive path went from slowest to second in eight days
and SyncTERM inherits all of it, but zmtx/zmrx 2.04 took the lead four days
before this measurement: 12.1 instructions per byte against sexyz's 24.6, and
0.62 CPU-seconds against 0.68. There is roughly a 2× headroom still visible on
the receive side and more on the send side. Error recovery is the strongest
measured, 10 of 10 in both directions where the lrzsz pair manages 7 of 10 and
`zmtx` 0 of 5 — which is the axis on which sexyz currently wins outright. The
remaining gap is the sender: 56.1 instructions per byte against `lsz`'s 23.5 and
`zmtx`'s 15.2, offset by threading rather than fixed. `ztx_buf` puts the engine
alone at 39.7, so roughly a third of sexyz's send cost is the ring-buffer
transport rather than protocol code. `zmtx`'s 15.2 in that sentence is its 2.02
figure, kept because it is what the ratio was measured against; at 2.04 it is
10.0.

One defect of our own, since fixed. **ESC8 was advertised and implemented in
neither direction** (§7.2) — `Escape8thBit=true` made sexyz ask a peer to escape
high-bit bytes it could not then decode, harmless for 21 years because no peer
implemented it and not harmless once one did. The option and the advertisement
were removed on 2026-08-30 (**#1229**). The remaining feature gap worth closing
is a minimum-throughput abort
(§3.2): sexyz's timeouts reset on every byte, so a trickling transfer holds a
node indefinitely.

**For zmtx/zmrx 2.04** — the fastest and cheapest implementation measured in
both directions, by roughly a factor of two, with two defects left on the
sending side:

- The retry budget resets only on the ZRPOS path (§6.2). NAK- and
  timeout-driven recovery still consumes it permanently, so `zmtx` still cannot
  complete a transfer at or above a 5e-7 error rate, scoring 0 of 5 at the gate
  rate. 2.04 improved this materially over 2.02 and now reports the reason
  rather than exiting silently; extending the reset to the other two paths would
  finish the job.
- ESCCTL escaping still omits carriage return (§7.2), so no ESCCTL receiver can
  download from `zmtx`. `7d2604c` ("Fix negotiated ZMODEM escaping", 2.03)
  addresses 8th-bit escaping and leaves this untouched. The fix Synchronet
  applied on 2026-08-24 transfers directly.

`zmrx` has neither problem: it recovers 5 of 5 at the gate rate, and it is the
only ESC8 implementation in the comparison that works.

**For anyone quoting these numbers.** Goodput without CPU is not interpretable
(§4.2); instruction counts do not order CPU time (§5.2); a receiver's cost
depends on its sender's block size (§5.3); a comparison whose fixed end is more
expensive than the varied end measures the fixed end (§2.2); and an error gate
run on a loaded host is not evidence (§2.4). All five are errors this document
has made and corrected.
