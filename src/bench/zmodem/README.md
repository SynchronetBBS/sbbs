# ZMODEM benchmark harness

Tooling used to produce `docs/zmodem_comparison.md` (repo root) and
to gate the (reverted) sexyz send-throughput work — see GitLab **#1195**
(throughput) and **#1196** (2 GB fix). Kept here so the eventual throughput
redesign starts from a working test bench, **especially the error-recovery
gate** that the first attempt skipped.

## What's here

| File | Purpose |
|---|---|
| `zbench_sock.py` | **Main harness.** Wires a sender + receiver (each speaking ZMODEM on stdin/stdout) through a userspace relay that can inject one-way latency, a bandwidth cap (`--rate-bps`, `--rate-back-bps` for asymmetry), and forward **bit-corruption** (`--corrupt-rate`). No root/`netem` needed. Each endpoint gets one bidirectional socket duped to both fds (works for sexyz, lrzsz, and Forsberg `sz`). |
| `zbench_pty.py` | Same idea over raw ptys — for serial-era tools that assume a tty. |
| `zbench.py` | Original two-pipe variant (separate stdin/stdout). Superseded by `zbench_sock.py`. |
| `ztx_buf.c` | A ~150-line ZMODEM sender that links the **real `zmodem.o`** behind a buffered `send_byte` (no ring, no thread). Isolates "how much is `zmodem.c`" from "how much is `sexyz.c`" — **not** a throughput model of SyncTERM (SyncTERM is BDP/socket-buffer tuned and faster). Build it at `-O3` to match the release flags; `-O2` under-reports. |
| `matrix.sh` | Driver for the block-size / CRC / latency / bandwidth / error sweeps. |
| `fixA.patch` | **Reverted.** Batch a subpacket span into the ring per flush (`sexyz.c`). Fast, but regressed error recovery. |
| `fixB.patch` | **Reverted.** Add a `send_buf` span callback to `zmodem.c` so the escape/CRC loop hands whole spans to the transport. Same error-recovery regression. |

## Building the comparison tools

- **sexyz:** `make RELEASE=1 sexyz` in `src/sbbs3` → `gcc.linux.x64.exe.release/sexyz`.
- **lrzsz** (`lsz`/`lrz`): old code; modern gcc needs permissive flags:
  `CFLAGS="-O2 -fcommon -w -Wno-implicit-int -Wno-implicit-function-declaration -Wno-int-conversion -Wno-return-mismatch" ./configure && make`
- **Forsberg rzsz** (reference): build the **`modern` branch** of the rzsz history
  repo (POSIX target) with the same permissive flags — the 2003 tarball segfaults
  on non-tty fds. Its `rz` (receiver) does not run headlessly; use its `sz` only.
- **ztx_buf:** `gcc -O2 -o ztx_buf ztx_buf.c <sbbs>/src/sbbs3/gcc.linux.x64.obj.release-mt/zmodem.o -I<sbbs>/src/sbbs3 -I<sbbs>/src/xpdev -I<sbbs>/src/hash <sbbs>/src/hash/gcc.linux.x64.lib.release/libhash.a <sbbs>/src/xpdev/gcc.linux.x64.lib.release/libxpdev_mt.a -lpthread -lm`

## Running

```bash
# Clean steady-state throughput (use a BIG file — 32 MB is startup-dominated!)
python3 zbench_sock.py --file bigfile.256M --outdir /tmp/o \
    --sender "/path/sexyz -8 sz bigfile.256M" --receiver "/path/lrz -y"

# The ERROR-RECOVERY GATE (see lesson below)
python3 zbench_sock.py --file test.8M --outdir /tmp/o \
    --sender "/path/sexyz -8 sz test.8M" --receiver "/path/lrz -y" \
    --corrupt-rate 0.000003 --timeout 200

# matrix.sh: DATA=<dir> ./matrix.sh {block|crc|lat|bw|err}  (edit binary paths at top)
```
Output line: `rc(s/r) elapsed size recv name INTEGRITY goodput wire overhead`.
`INTEGRITY` is a SHA-256 compare of source vs. received.

## Lessons for the redesign (read before touching the send path)

1. **Measure steady-state, not 32 MB.** Short transfers are dominated by
   per-transfer startup and *understate and distort* rates. Use ≥256 MB, and run
   every sender in one interleaved batch so the numbers are comparable.
   Reference points as of 2026-07-24 (256 MB → `lrz`, `-O3`): lrzsz `lsz -8`
   **203.9**; `ztx_buf` on `zmodem.c` rev 2.3 **115.8**; sexyz *receiving*
   **113.3**; Forsberg `sz` **96.9**; `ztx_buf` on rev 2.2 **91.9**; sexyz
   *sending* **11.5**.

2. **Gate on error recovery, with MULTIPLE runs.** A batching attempt was once
   committed on a *single* passing error run and later shown to fail. The
   injected-error model is chaotic (a retransmit shifts where later corruption
   lands), so one pass is not validation — run it ≥3× and require all pass.
   Reference: the shipped per-byte sender passes **3/3** (~50 s each).

3. **The bottleneck is the *producer*, confirmed with hard numbers.**
   `sexyz.c`'s `send_byte()` takes the ring mutex **twice per byte**
   (`RingBufFree` then `RingBufWrite`) while `output_thread` hot-loops on the
   same mutex. Measured on a 256 MB send: **44 CPU-seconds** (21.5 user +
   22.6 sys, 185 % — both threads pegged) and **1,464,130 voluntary context
   switches**, against `lsz`'s **0.95 s** and **1,402**. Under `strace -c -f`
   (32 MB): **1,959,794 futex** calls, 37 % contended, 80 % of syscall time —
   *one futex per 17 bytes* — plus **357,935 writes averaging ~94 bytes**.

   **Do not chase this with tuning.** Every `sexyz.ini` knob was swept on the
   same 256 MB send and the result is flat inside noise: `OutbufSize` 16 K/64 K
   × `OutbufHighwaterMark` 0/1100/8 K/32 K/60 K × `OutbufDrainTimeout`
   0/10/100/250 ms all land between **10.9 and 11.6 MB/s** with CPU stuck at
   187 %. Ring capacity and consumer hysteresis cannot help, because the cost is
   per-byte on the producer side before fill level matters. Flow-control stalls
   are also a red herring: only 10–19 `FLOW` events occur in a whole 256 MB run.

4. **Producer batching is trivially fast and repeatedly wrong. Six attempts.**
   Buffering the producer removes the *entire* sexyz-specific penalty — a
   prototype hit **115.8 MB/s at 1.0 CPU-second**, exactly the `ztx_buf` ceiling,
   i.e. no residual transport cost at all. Every one of them then failed the
   error gate, where the shipped per-byte sender passes 3/3:

   | Attempt | Clean | Gate |
   |---|--:|:--:|
   | *Fix A* (`fixA.patch`) — accumulate a subpacket, flush via the **void** `flush()` callback | fast | 0/3 |
   | *Fix B* (`fixB.patch`) — `send_buf` span callback in `zmodem.c` | fast | 1/3 |
   | *v3* — 8 KB threshold flush inside `send_byte`, non-dropping boundary flush | fast | 0/3 |
   | *v4/v5* — abort-aware `purge_output` on ZRPOS + bounded 2 KB read | fast | 0/3 |
   | *v6* — flush at `recv_buffer()` (the single back-channel choke point), transient errors, no byte drops; batch 512 B–4 KB | 115.8 MB/s | 0–1/3 |
   | *v7* — `SINGLE_THREADED TRUE` + buffered `send_byte`, **blocking** writes | 115.8 MB/s | **2/3** |

   Failure modes actually identified, so nobody repeats them: a **`void` flush
   callback silently swallowing a failed write**; a **mid-subpacket blocking
   flush starving the back-channel**; **position accounting desyncing** from the
   wire (ZFIN sent at 4.9 MB of an 8 MB file); and a **sticky error latch** that
   turned one transient full-buffer timeout into an infinite
   `zmodem_send_raw ERROR: -1` spin. A failed flush must be treated as
   **transient** — keep the unwritten remainder buffered, never latch, never
   drop — and the flush must happen before any wait on the back-channel
   (`recv_buffer()` is the one choke point in `sexyz.c`).

   **It is not a speed artifact — this was tested.** The obvious objection is
   that a 10×-faster sender simply has more data in flight when corruption hits.
   Rate-capping the batched sender to **11 MB/s**, the shipped sender's own
   natural rate, settles it: at the identical rate the shipped sender passed
   **3/3** and the batched sender still failed. Batching genuinely breaks
   recovery under the current architecture.

   **Correction (Deuce, SyncTERM's author): the "recall stale bytes" framing was
   wrong.** You cannot recall bytes once they hit the socket/network buffers —
   lrz can't either, yet it recovers. The abort-aware `purge_output` detour was
   solving a non-problem.

   **Recommended path — and v7 says what's missing.** Dropping to a single
   thread with a buffered `send_byte` kept the full 115.8 MB/s, cut voluntary
   context switches from 1,464,130 to **1,585** (`lsz` does 1,402), and lifted
   the gate from 0–1/3 to **2/3**. What still fails is the **blocking**
   `sendbuf()`: when the socket fills while the receiver is trying to send a
   ZRPOS, the sender stops reading the back-channel and the session stalls. So
   the target is precisely what `lrz` is — single-threaded, buffered,
   **non-blocking output with `select()`/`poll()` on both directions**. Remove
   the two-thread ring rather than optimize it. A real rewrite, not a patch.

   **But first ask whether it's worth it (Deuce):** this whole exercise measured
   **localhost**, which is largely beside the point — below ~8 ms RTT the socket
   buffer / bandwidth-delay product dominates the send loop, not the CPU. On real
   (network) transfers the link usually dominates; the send-loop rewrite only pays
   off on fast LANs. SyncTERM's throughput is itself a deliberate socket-buffer
   (BDP) choice, tuned to ~75% of a 1 Gb LAN — not an emergent send-path fact.

5. **`zmodem.c`'s own share is now ~1.8× (203.9 → 115.8), down from ~2.2×.**
   Deuce reworked the shared send path on 2026-07-24 — class-table byte
   classifier, slicing-by-4 CRC-32, hoisted escape mask with `noinline` cold
   paths, buffered `fcrc32()` — moving the buffered floor **91.9 → 115.8 MB/s**.
   SyncTERM inherits it. The rest of the gap to lrzsz is its fully inlined
   escape/CRC loop; closing it means handing `send_byte` a span instead of a
   byte, and is only worth it if the target link outruns 115 MB/s.

6. **A single-stream localhost benchmark cannot see per-byte CPU wins above
   ~100 MB/s.** Two of Deuce's optimizations measured **exactly zero** here
   (his own numbers: +3.15 % for CRC slicing, +14.46 % for the inlining work)
   because at that rate this harness is bound by the write/relay path, not the
   classify loop. He measured with **six interleaved 1 GiB transfers** — a
   CPU-saturated setup. If you are validating a per-byte optimization, reproduce
   that shape; if you are validating a *transport* change, a single stream is
   fine (it moved 11.5 → 115.8 here without ambiguity).
