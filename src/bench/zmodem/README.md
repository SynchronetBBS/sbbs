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

2. **Gate on error recovery, with MULTIPLE runs.** The first batching attempt was
   committed on a *single* passing error run and later shown to fail **0/3**
   (Fix A) and **1/3** (Fix B), while the shipped per-byte sender passes **3/3**.
   The injected-error model is chaotic (a retransmit shifts where later
   corruption lands), so one pass is not validation — run it ≥3×.

3. **Why the prototypes failed — the design constraint.** Batching a whole
   subpacket before one *blocking* flush leaves the sender stuck inside the flush
   (ring full while the receiver is back-pressured mid-retransmit-storm), so it
   never services the incoming ZRPOS → bidirectional stall. The shipped sender
   writes each byte to the ring *as produced*, so the output thread drains
   continuously and the protocol thread reaches the back-channel check promptly.
   **A correct batched sender must keep servicing the back-channel while
   sending** — interleave, or flush in back-channel-yielding chunks — not block
   on a single whole-subpacket flush. lrzsz batches *and* recovers, so it's
   achievable.

4. **Two stacked costs.** ~7.5× is `sexyz.c` (ring per-byte → futex storm /
   84-byte writes; sexyz-only) and ~2.4× is `zmodem.c` (per-byte
   `send_byte`-callback + escape + CRC; shared with SyncTERM, ~85 MB/s ceiling).
   A full fix needs both, and `zmodem.c`'s share is where SyncTERM benefits too.
