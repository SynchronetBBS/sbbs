# SEXYZ Changes

Changes since **v3.1**, the last released binary archive (September 2025).

Component versions in this release: `sexyz.c` **3.5**, `zmodem.c` **2.6**,
`xmodem.c` **2.0**. `sexyz v` prints all three.

## Speed

- Much faster ZMODEM streaming sends. The send path now buffers its output
  instead of feeding the transmit ring one byte at a time, roughly a 10x
  throughput increase on fast/local links and far less CPU, with no change
  on slower links where the connection speed dominates anyway. Windowed
  (`-w`) and segmented (`-s`) sends are unchanged (issue #1195)
- Every file sent used to stall for one second before its data started. The
  sender purged its receive buffer after accepting the ZRPOS that answers
  each ZFILE, using a one-second read timeout, so with nothing to discard
  it waited out the timeout once per file. The cost was per file rather
  than per byte, so it hurt small files and slow links worst: five short
  files over a 115200 bps line spent more time asleep than transferring
- ZMODEM send-path speedups shared with SyncTERM: table-driven byte
  classification, slicing-by-4 CRC-32, an inlined common transmit path, and
  a buffered whole-file CRC
- The transmit ring no longer signals its empty/data events on every byte
  written, only when the ring actually changes state. Those are kernel
  calls on Windows and condition-variable operations elsewhere, and the
  send path was paying two or three of them per byte

## Transfers

- Fixed a hang on windowed (`-w`) sends of files larger than 2 GB, where the
  transmit-window and ACK positions were signed 32-bit and went negative
  past 2 GB (issue #1196). Validated to 2.36 GB; 4 GB remains a hard ZMODEM
  protocol limit
- Fixed a crash (divide-by-zero) at the start of any send where the transmit
  window is smaller than four times the block size, e.g. `-8 -w8192` or a
  comparable `MaxWindowSize` in `sexyz.ini` (issue #1197)
- `-w` now reduces the block size to a quarter of the window when needed,
  the way `lsz`/`sz` do, so a window at or below the block size no longer
  stalls the transfer (issue #1197)
- Fixed file sizes being truncated to 32 bits in three comparisons on the
  receive side, which misjudged files larger than 2 GB
- The sender no longer retransmits its ZFILE when the receiver's opening
  ZRINIT arrives after it. Both copies were answered, and the spare ZRPOS
  left on the back-channel could be read mid-stream and restart the file
- Hex headers terminated with parity-set CR/LF (`\x8D\x8A`) are accepted
- The `OO` that follows a ZFIN is now flushed, rather than waiting behind
  the buffer for a session that has already ended
- `zmodem_recv_bin32_header()` reported success after a receive error; the
  CRC check caught it in practice, but the result was still wrong
- X/YMODEM transfers now notice a local cancellation promptly instead of
  waiting out the current protocol read

## Messages

- The file offset shown in the receiver's progress and error messages was
  always 0. Only the sending side maintained it; the receiver now reports
  its real position
- "Finishing Session (Sending ZFIN)" is no longer prefixed with a file
  offset. It ends a session rather than a file, so no offset applied in
  either direction, and on the sending side it printed the last file's
  final position

## Behavior

- `poll()` is used instead of `select()` on Unix-like systems
- Wildcards in a specified transmit path or filename are no longer expanded
  (issue #1044)
- `-s` (segmented) is documented as affecting both directions. It is
  consulted when receiving too, where it asks the remote sender to go
  block-at-a-time; only `-w` is genuinely send-side

## Source and license

- sexyz now ships as a stand-alone source release that builds on Unix-like
  systems and on Windows with MSVC, needing nothing else from Synchronet.
  See `COMPILING.md`
- `zmodem.c` and `zmodem.h` are now under a 2-clause BSD license. The rest
  of the source set is GPL or LGPL; see the License section of
  `COMPILING.md`
