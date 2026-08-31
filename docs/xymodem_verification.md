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

**Three of them fail hard rather than degrading**, and comparing each against
lrzsz and DSZ sorts out whose fault that is. On CRC fallback sexyz behaves
correctly and lrzsz does not; on **G-mode fallback sexyz is the one that fails
where DSZ degrades**, which is the actionable finding here; and the receive-side
block-size cap is a Synchronet-only setting with no counterpart to compare to,
whose only reachable effect is to break a working transfer.

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

The right question about all three is not "does sexyz fail" but **"does anyone
else do better in the same situation"**, since XMODEM has no capability
negotiation and someone has to degrade. Measured against both references, the
answer differs per case — and in one of them sexyz is the best of the three.

### 4.1 CRC fallback: sexyz is correct, lrzsz is the outlier

XMODEM-CRC specifies the degradation on the *receiver* side: it sends `C` to ask
for CRC-16 and, after enough unanswered attempts, falls back to `NAK` for
checksum. Put each receiver in front of a checksum-only sender
(`sexyz sx` with `SendCRC=false`):

| Receiver | Falls back? | |
|---|---|--:|
| **sexyz `rc`** | **yes** — transfers, 16,898 bytes | 12 s |
| **DSZ `rx`** (1997) | **yes** — transfers | — |
| `lrz --xmodem -c` | **no** — nothing transferred | 40 s timeout |

So the deadlock reported when `SendCRC=false` meets `lrz --xmodem -c` is lrzsz
declining to degrade, not sexyz misbehaving. sexyz in the same role does the
specified thing, and agrees with Forsberg's own implementation. Setting
`SendCRC=false` is still a footgun — it makes sexyz unable to satisfy a
CRC-insisting peer — but the missing fallback is at the other end.

### 4.2 G-mode fallback: sexyz is the one that fails

The mirror test, with a sender that refuses G (`sexyz sY` with `SendG=false`)
in front of a G-mode receiver:

| Receiver | Falls back? | |
|---|---|--:|
| **DSZ `rb -g`** (1997) | **yes** — transfers | — |
| **sexyz `rg`** | **no** — `!Error fetching YMODEM header block` | 27 s |

DSZ drops out of G mode and completes the transfer; sexyz gives up. This is a
real gap on our side, and the clearest actionable item in this document: a
receiver asked for G, offered a non-G sender, should transfer rather than fail.

Sending, the same configuration shows as `SendG=false` dying after **18 bytes**
against a G-mode receiver, against 16,731 bytes when `SendG=true`.

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

### 4.4 What the comparison shows

None of the three is a dead option — each demonstrably changes behaviour, which
is what the audit was looking for. Comparing against the other implementations
sorts them into three different verdicts rather than one:

| Situation | sexyz | lrzsz | DSZ |
|---|---|---|---|
| Receiver wants CRC, sender offers checksum | **falls back** | fails | **falls back** |
| Receiver wants G, sender refuses G | **fails** | — (no G receiver tested) | **falls back** |
| Receiver capped below the sender's block size | breaks mid-transfer | no equivalent setting | no equivalent setting |

Only the middle row is a defect of ours by comparison. The first row is lrzsz's
gap, not ours. The third has nothing to compare against — neither `lrz` nor DSZ
exposes a receive-side block-size cap at all, so `MaxBlockSize` is a
Synchronet-only setting whose sole reachable effect is to break a transfer that
would otherwise work.

The common thread worth fixing regardless: when a capability mismatch does end a
session, nothing in the log names the cause. A sysop who sets `SendCRC=false`
and then cannot send to half the terminals in the world has no way to connect
the two facts.

---

## 5. Not tested

Stated so the tables above are not read as more than they are:

- `MaxErrors`, `AckTimeout`, `ByteTimeout` and `G_Delay` — error- and
  timing-path settings that need injected faults or a rate-limited link.
- XMODEM-G (`rg` against an X-G sender, as distinct from the YMODEM-G path
  exercised in §4.2).
- The `-telnet` mode, which needs IAC injection.
- Transfers through a real serial line or modem, as opposed to a socket.
