# X/YMODEM: sexyz interoperability and option verification

**Measured:** 2026-08-30, one host.
**Author:** Claude (analysis commissioned by Rob Swindell)
**Scope:** Does sexyz's X/YMODEM support interoperate, and does every X/YMODEM
option actually do something? A companion to `zmodem_comparison.md`, which
covers ZMODEM only.

This exists because an audit of sexyz's options — prompted by `Escape8thBit`
having been advertised on the wire but implemented in neither direction for 21
years (GitLab #1229) — left the X/YMODEM knobs untested. They are tested here.

---

## 0. Summary

**Interoperability is clean.** All twelve sender/receiver combinations against
two independent implementations transfer a 16 KiB random file byte-for-byte,
including against a 1997 DOS binary.

**Every knob does something**, so there is no second `Escape8thBit` here.

**But three of them fail hard rather than degrading**, which is the finding
worth acting on. XMODEM has no capability negotiation — the receiver drives and
the sender complies — so switching a capability *off* in `sexyz.ini` does not
make sexyz negotiate downward. It makes sexyz unable to talk to a peer that
requires that capability, and the failure is a timeout or an immediate abort
with no diagnostic naming the cause.

---

## 1. Method

The harness is `zbench_sock.py` (`src/bench/zmodem/`), the same one used for the
ZMODEM work: two programs speaking on stdin/stdout through a userspace relay,
with a SHA-256 comparison of source against received file.

- **Payload:** 16 KiB of `/dev/urandom`. XMODEM has no length field and pads the
  final block, so comparisons truncate to the original length before hashing.
- **References:** lrzsz 0.12.21rc (`lsz -X`, `lsz --ymodem`, `lrz --xmodem`,
  `lrz --ymodem`) and Forsberg's **DSZ.EXE 1997-05-25** under DOSBox, its COM1
  bridged to TCP. DSZ is the period-correct implementation and its X/YMODEM
  commands are `sx`/`rx` and `sb`/`rb`.
- **Wire byte counts** are quoted where they distinguish one encoding from
  another; they are the cheapest evidence that a knob changed anything.

---

## 2. Interoperability

### 2.1 Against lrzsz

| Pair | Result |
|---|---|
| sexyz `sx` → `lrz --xmodem` | identical |
| sexyz `sX` (1K) → `lrz --xmodem` | identical |
| sexyz `sy` → `lrz --ymodem` | identical |
| sexyz `sY` (1K) → `lrz --ymodem` | identical |
| `lsz -X` → sexyz `rx` | identical |
| `lsz -X -k` → sexyz `rc` (CRC) | identical |
| `lsz --ymodem` → sexyz `ry` | identical |

### 2.2 Against Forsberg's DSZ (1997)

| Pair | Result |
|---|---|
| sexyz `sx` → `DSZ rx` | identical |
| sexyz `sX` (1K) → `DSZ rx` | identical |
| sexyz `sy` → `DSZ rb` | identical |
| `DSZ sx` → sexyz `rx` | identical |
| `DSZ sb` → sexyz `ry` | identical |

A note for anyone repeating this: YMODEM carries the filename, and DSZ writes it
through DOS's 8.3 rules — `xy.16k` arrives as `XY.16K`. A test that looks for
the name it sent will report a failure that did not happen. That cost one
diagnostic cycle here.

---

## 3. Option verification

Each row was set in `sexyz.ini`, exercised in a real transfer, and judged by
whether the wire or the outcome changed.

| Option | Verified effect |
|---|---|
| `[XMODEM] SendCRC` | **yes** — `sx` to a CRC-requesting receiver puts 17,025 bytes on the wire against 16,897 with checksum, the extra byte per block being the second CRC octet. See §4.1 |
| `[XMODEM] SendG` | **yes** — G-mode streaming drops the per-block ACKs, 16,731 bytes against 17,291. See §4.2 |
| `[XMODEM] MaxBlockSize` | **partly** — no effect on sends; breaks receives. See §4.3 |
| `[XMODEM] BlockSize` | reached; the send block size is set by the command (`sx` vs `sX`) and by `-k` |
| `[YMODEM] FallbackToXmodem` | **yes** — with `=1`, a `sy` send completes against `lrz --xmodem`, i.e. the sender fell back |
| `[XMODEM] MaxErrors`, `AckTimeout`, `ByteTimeout`, `G_Delay` | reach the engine; error- and timing-path only, not exercised here |

---

## 4. Findings

### 4.1 `SendCRC=false` hangs against a CRC-only receiver

| Receiver | Result |
|---|---|
| `lrz --xmodem` (accepts checksum) | identical, 16,897 bytes |
| `lrz --xmodem -c` (requires CRC) | **TIMEOUT** — nothing transferred |

In XMODEM the receiver drives: it sends `C` to request CRC-16 or `NAK` for
checksum. With `SendCRC=false` sexyz will not answer `C`, and a receiver that
only ever sends `C` waits forever. Both ends are behaving defensibly and the
session still deadlocks, burning the full timeout with no message naming the
cause.

### 4.2 `SendG=false` aborts against a G-mode receiver

Sending YMODEM-1K to a receiver in G mode: with `SendG=true` the transfer
completes (16,731 bytes on the wire); with `SendG=false` it dies after **18
bytes**. Same shape as §4.1 — the capability is refused and there is no path
back to a mode both ends can use.

### 4.3 `[XMODEM] MaxBlockSize` is asymmetric, and its receive side breaks transfers

Sending, it does nothing:

| | Wire |
|---|--:|
| `sx`, default | 16,897 |
| `sx`, `MaxBlockSize=128` | 16,897 |
| `sX` (1K), default | 16,449 |
| `sX`, `MaxBlockSize=128` | 16,449 |

An explicit `sX` still sends 1024-byte blocks with the maximum pinned to 128 —
the command wins over the setting.

Receiving, it does something worse than nothing: with `MaxBlockSize=128`, a
transfer from `lsz -X -k` (1024-byte blocks) **fails part-way**, 10,280 bytes in,
with `!File Transfer Failure`. The same pair succeeds at the default.

So the one direction where the setting has an effect is the direction where its
effect is to break an otherwise working transfer.

### 4.4 What these three have in common

None is a dead option — every one demonstrably changes behaviour, which is what
the audit was looking for. They share a different defect: **turning a capability
off produces a hard failure rather than a negotiated fallback.** That is partly
inherent to XMODEM, which has no capability negotiation, but the diagnostics
could name the cause instead of leaving a timeout. A sysop who sets
`SendCRC=false` and then cannot receive from half the terminals in the world has
no way to connect the two facts from the log.

---

## 5. Not tested

Stated so the tables above are not read as more than they are:

- `MaxErrors`, `AckTimeout`, `ByteTimeout` and `G_Delay` — error- and
  timing-path settings that need injected faults or a rate-limited link.
- XMODEM-G (`rg` against an X-G sender, as distinct from the YMODEM-G path
  exercised in §4.2).
- The `-telnet` mode, which needs IAC injection.
- Transfers through a real serial line or modem, as opposed to a socket.
