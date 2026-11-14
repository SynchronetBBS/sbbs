# RIP Rendering Comparison Harness

Persistent multi-port server for real-time pixel-level comparison of RIPterm vs SyncTERM rendering.

## Starting the Harness

```sh
cd /synchronet/src/sbbs/src/syncterm/rip_test
python3 rip_harness.py
```

Default ports (all on 127.0.0.1):
- **1514** — SyncTERM connects here
- **1515** — RIPterm connects here
- **1516** — Control port (raw TCP, no telnet)

Options: `--syncterm-port`, `--ripterm-port`, `--control-port`, `--output <dir>`,
`--rip-dir <dir>`, `--control-bind <addr>` (for remote access to control port)

## Connecting Terminals

1. Get window IDs: `xwininfo` then click each terminal window
2. Connect SyncTERM to `127.0.0.1:1514`, enter window ID when prompted
3. Connect RIPterm to `127.0.0.1:1515`, enter window ID when prompted
4. Both must be **640x350** (no aspect-ratio scaling)

## Control Port

Raw TCP socket — use `nc`, `printf ... | nc`, or Python sockets. No telnet
negotiation. Line-based commands, responses end with `OK` or `ERROR:`.

When the harness runs on a different machine (e.g. a desktop with X11), use
`--control-bind <hostname>` to allow remote control connections. Terminal ports
always bind to localhost.

## Commands

All commands accept an optional target (`syncterm` or `ripterm`). Omit to target both.

| Command | Description |
|---------|-------------|
| `help` | List commands |
| `status` | Show connections, window IDs, last capture time |
| `send [target] <data>` | Send raw data (`\x` `\r` `\n` `\t` `\\` escapes) |
| `sendfile [target] <path>` | Send .rip file with SAUCE strip + flow control |
| `sendlines [target] <path> <n>` | Reset + send first N lines (for bisection) |
| `reset [target]` | Send RIP_RESET + SBAROFF + hide cursor |
| `capture [target]` | RIP_QUERY sync + XWD capture, auto-compare |
| `snap [target]` | XWD capture without sync (for non-RIP content) |
| `diff` | Re-compare last captures |
| `pixels x,y` | EGA color at point from both captures |
| `pixels x1,y1,x2,y2` | EGA colors in rectangle, inclusive (max 50x50) |
| `diffpixels [limit]` | List all differing pixels with EGA colors (default 500) |
| `ripdir [path]` | Show or set the .rip file base directory |
| `quit` | Close control session (terminals stay connected) |

## Typical Workflow

Always start with `reset` before sending content — this sends RIP_RESET, SBAROFF, and
hides the cursor. Without it, the status bar may be visible on one terminal but not the
other, causing spurious diffs. Note that `sendlines` has its own built-in RIP_RESET but
does NOT send SBAROFF, so always `reset` before a `sendlines` session too.

```
> status                        # verify both connected
> reset                         # clear prior state + disable status bar
> sendfile test01_ellipses.rip  # send test file to both
> capture                       # sync + capture + compare
> diffpixels                    # see exactly which pixels differ
> reset                         # clear for next test
> sendlines surfer1.rip 50      # bisect: send first 50 lines
> capture                       # compare partial render
> sendlines surfer1.rip 25      # narrow down
> capture
> pixels 320,175                # inspect specific pixel
```

## Cursor and Mouse Differences

RIPterm uses a solid block cursor; SyncTERM defaults to a flashing underline. This is
intentional and not a rendering bug. The `capture` command sends `RIP_HIDE_CURSOR` before
the sync query, so captures are normally cursor-free. The `snap` command does **not** hide
the cursor — if you need a clean snap, send `reset` first.

Launch RIPterm with `-M` to disable the mouse cursor. Without this, the DOSBox hardware
mouse cursor appears in XWD captures and causes spurious diffs (typically 2 pixels that
are NOT multiples of 8 — real cursor artifacts are always 8-pixel character cells).

## Bisecting Rendering Differences

Use binary search with `sendlines` to find which RIP command introduces a diff:

```
> reset
> sendlines file.rip 500     # test midpoint
> capture                     # check diffs
# if 0 diffs: problem is after line 500, try 750
# if diffs: problem is before line 500, try 250
# repeat until you find the exact line
```

Use `send [target]` to send individual commands from the identified line to isolate
which specific RIP command causes the diff. Send to one terminal only to see what changed.

For flood fill issues: send the file up to just before the fill, verify 0 diffs, then
send fills one at a time to find which one diverges.

## Response Format

Pixel colors reported as `name(index)` using EGA palette:
`black(0)` `blue(1)` `green(2)` `cyan(3)` `red(4)` `magenta(5)` `brown(6)` `ltgray(7)` `dkgray(8)` `ltblue(9)` `ltgreen(10)` `ltcyan(11)` `ltred(12)` `ltmagenta(13)` `yellow(14)` `white(15)`

## Files

- `rip_harness.py` — the harness script
- `rip_harness.md` — this file
- Captures saved to `/tmp/rip_harness/` (or `--output` dir) as `cap_NNNN_syncterm.xwd`, `cap_NNNN_ripterm.xwd`, `cap_NNNN_diff.png`

## Bugs Found With This Harness (2026-04-04)

1. **Degenerate polygon fill** — `scanline_poly_fill` rejected 2-point polygons (09-awe.rip)
2. **NO_MORE state machine** — bytes after `|#|#|#` leaked to terminal, causing text window scroll (blitz.rip)
3. **Thick line viewport clipping** — offset lines need Cohen-Sutherland endpoint clipping (blitz.rip)
4. **Flood fill: fillfg as border** — BGI uses EGA Color Compare with fill color, so pre-existing pixels matching the fill color block seed propagation (blitz.rip)
5. **Bezier start point** — zero-length draw at start needed in all modes, not just XOR (7up-svr5.rip)
6. **8x8 font data** — 764 byte corrections across 141 glyphs extracted from RIPterm runtime (ag-awe.rip, ag-tr6.rip, many others)


## Native numeric parser regression tests

`ripper_test.c` includes the production parser and bitmap renderer. Build and
run it with the SkyPix regression tests from the SyncTERM directory:

```sh
cmake --build build --target ripper_test skypix_test syncterm -j4
ctest --test-dir build --output-on-failure -R '^(ripper_test|skypix_test)$'
```

The `words`, `word-consumers`, and `word-buttons` filters run the numeric
cases separately. The fixture uses SDL offscreen; standalone invocations need
`SDL_VIDEODRIVER=offscreen SDL_RENDER_DRIVER=software SDL_VIDEO_EGL_DRIVER=none`.
The reference for the RIPv1 expectations is `RIP154_PARSER.adoc` in the
research tree (`/bbsdev/syncterm/docs/`). These tests check literal reference
words, consumer state, selected pixels, replies and packet boundaries. They
are not fresh DOS pixel comparisons.

RIPv1 numeric conversion retains the low 16 bits before the consumer assigns
a signed coordinate, byte, unsigned index or mask role. In particular,
`-001` and `1EKF` both produce `ffff`, `1EKG` produces zero, and `A-` produces
ten. Numeric storage stops at 511 words while validation continues; text
storage stops at 1023 bytes. Invalid mandatory/reserved fields prevent the
body from running. Polygon counts must fit the retained pairs, including
space for the closing vertex. Reference reads of unavailable vertices and
out-of-range palette indexes are deliberately bounded in C.

Requested line, fill, font, viewport, palette, button and text-window words
are retained separately from effective renderer state. Invalid line/fill
requests leave the effective drawing state unchanged. Palette values retain
the low port byte and map the six EGA colour bits through a bounded table.
Invalid fonts fall back to font 0, size 1. The verified RIPterm metric table has
entries for sizes 1..10 only; its unchecked size lookup can read neighbouring
font entries or unrelated data. SyncTERM keeps metric indexes bounded.
Similarly, RIPterm stores out-of-range TTY cursor bytes which its erase
command can subsequently use in rectangle arithmetic. SyncTERM retains its
terminal cursor/window bounds. Those unchecked reference behaviours do not
establish supported rendering cases or unfinished numeric-parser requirements.
BGI glyph scaling is separate from RIPterm's metric lookup; no general claim
of size parity beyond the verified entries is made here. Later SyncTERM
command extensions are separate from signed numeric decoding. RIPv3 retains
its existing numeric conversion and
descriptor policy; shared bounds and wide intermediates also protect its
renderer.


## Copy-region regression tests

The `copy-region` filter in `ripper_test` exercises `!|1G` through the
production parser. It seeds distinct images in the two blink buffers and
compares every pixel with an independently specified expected rectangle.
It covers both overlap directions, reversed endpoints, disabled and offset
viewports, unchanged clipboard/drawing state, negative coordinates, screen
clamps, malformed fields, packet splits, reduced display mapping and copies
larger than the BGI clipboard-size limit.

RIPv1 follows the reference helper at `0c16:0623`, as recorded in
`RIP154_PARSER.adoc`: signed endpoint sorting, signed remainders in X
alignment, use of the nominally reserved fifth word, whole-byte widths,
the initial zero-height exit and the destination check using the original
Y difference. The downward helper at `0c16:0293` sorts the adjusted
endpoints again. Both paths copy inclusive source rows. For example,
`!|1G080A0O0D000B` copies X=8..23, Y=10..13 to Y=11..14.

The reference's loose bottom check can allow writes beyond the displayed
rows. The native implementation copies the visible destination rows and
never accesses pixels outside its bitmap. Nonpositive byte widths are
ignored safely. RIPv3 uses the specified outward X alignment, ignores the
reserved fifth word, permits a single source row, and requires the entire
destination rectangle to fit. Both versions copy absolute screen pixels
independently of the graphics viewport, pattern and write mode. A temporary
snapshot preserves both blink buffers during overlapping copies without
replacing the RIP clipboard. These are source/disassembly-directed native
tests; no fresh DOS captures are claimed.
