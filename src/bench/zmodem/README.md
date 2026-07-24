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
| `ztx_buf.c` | A ~150-line ZMODEM sender that links the **real `zmodem.o`** behind a SyncTERM-style **buffered** `send_byte` (no ring, no thread). A faithful model of SyncTERM's sender, used to isolate "how much is `zmodem.c`" from "how much is `sexyz.c`". |
| `matrix.sh` | Driver for the block-size / CRC / latency / bandwidth / error sweeps. |
| `fixA.patch` | **Reverted.** Batch a subpacket span into the ring per flush (`sexyz.c`). ~11→66 MB/s but regressed error recovery. |
| `fixB.patch` | **Reverted.** Add a `send_buf` span callback to `zmodem.c` so the escape/CRC loop hands whole spans to the transport. ~66→88 MB/s; would benefit SyncTERM too. Same error-recovery regression. |

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
   per-transfer startup and *understate and distort* rates. Use ≥256 MB.
   (Original sexyz ≈ 11 MB/s; the reverted batched prototype ≈ 66; the
   `zmodem.c` per-byte ceiling ≈ 85; lrzsz ≈ 204.)

2. **Gate on error recovery, with MULTIPLE runs.** A batching attempt was once
   committed on a *single* passing error run and later shown to fail. The
   injected-error model is chaotic (a retransmit shifts where later corruption
   lands), so one pass is not validation — run it ≥3× and require all pass.
   Reference: the shipped per-byte sender passes **3/3** (~50 s each).

3. **The bottleneck is the *producer*, confirmed.** The protocol thread's
   per-byte `RingBufWrite` (mutex-locked) caps the send at ~11 MB/s. `ztx_buf`
   (same `zmodem.o`, buffered `send_byte`, no ring) does the identical per-byte
   escape/CRC work at ~85 MB/s — so it is the ring write, not zmodem's escaping,
   that costs the ~7.5×. Consumer-side coalescing of the output thread was tried
   and did **nothing** (11.4→11.5 MB/s): the data already trickles in per byte,
   so only *producer* batching can help.

4. **Producer batching is the crux — and it keeps breaking error recovery.**
   Three distinct batching prototypes each reached ~66–88 MB/s and each FAILED
   the 3× error gate (0/3, 1/3, 0/3) where the per-byte sender passes 3/3:
   - *Fix A* (`fixA.patch`): accumulate a subpacket, flush via the void `flush()`
     callback. Bug: a failed flush was silently dropped (void return), so the
     sender kept going and desynced the receiver.
   - *Fix B* (`fixB.patch`): a `send_buf` span callback in `zmodem.c`. Same class
     of failure, plus a mid-subpacket blocking flush that starves the back-channel.
   - *v3* (not saved; see git history around this README): flush on an 8 KB
     threshold *inside* `send_byte` so the error propagates, and a boundary
     `flush()` that never drops. Still failed 0/3 — it sent ZFIN at 4.9 MB of an
     8 MB file, i.e. the sender's position accounting desynced from the wire.

   **Root cause (the real design constraint):** batching decouples *what zmodem
   believes it has sent* (`send_byte` returned success) from *what is actually on
   the wire* (still in the transport buffer). ZMODEM error recovery (ZRPOS →
   seek → resend) assumes those track. On a reposition, the buffered-but-unsent
   bytes are stale and must be reconciled — dropped, kept, and re-based against
   the new position — which the decoupled `send_byte` sink cannot do on its own.
   lrzsz batches *and* recovers because its buffering is integrated with the
   protocol and it purges/repositions its output buffer on ZRPOS.

   **Abort-aware purge was tried — it helps but is not sufficient.** A
   `purge_output` callback was added to `zmodem.c` (called from
   `zmodem_handle_zrpos`) and implemented in `sexyz.c` to clear `txbuf`, the ring
   (`RingBufReInit`), and the output thread's linear buffer on every reposition;
   the output-thread read was also bounded (2 KB) so the in-flight chunk a purge
   cannot recall stays small. The purge fires correctly (confirmed) and the
   transfer gets noticeably further under errors (~1.3 MB → ~2.7 MB before
   failing) — but it **still failed the error gate**: under heavy corruption the
   block size collapses to 128 B, retransmit storms fill the ring, the receiver
   eventually gives up, and the sender stalls (`waiting for output buffer to
   flush, 16384 bytes`). Five distinct batched-transport variants now fail while
   the per-byte sender passes 3/3.

   **Recommended path (untried here):** stop fighting the two-thread ring for the
   *sender*. The provably-fast-AND-robust reference is lrzsz: single-threaded,
   the protocol thread owns its output buffer, and it flushes/repositions that
   buffer *synchronously* with protocol state (so there is never a second thread
   holding stale bytes it can't recall, and the back-channel is serviced between
   subpackets). Re-architecting sexyz's sender that way — a buffered,
   single-threaded send path integrated with the protocol, rather than a
   ring + `output_thread` — is the change most likely to be both fast and
   error-robust. It is a real rewrite, not a patch.

5. **`zmodem.c`'s own share (~2.4×, 204→85).** Even a correct batched sender is
   capped ~85 MB/s by the per-byte `send_byte`-callback + escape + CRC pipeline,
   shared with SyncTERM. Reducing that (batch the escape/CRC inner loop, hand the
   transport spans) is the second, shared lever — but only worth doing once the
   send architecture above makes batching safe.
