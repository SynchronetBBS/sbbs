#!/usr/bin/env python3
"""Decode a SYNCMOO1_WIREDUMP capture into a readable, interleaved trace.

    SYNCMOO1_WIREDUMP=/tmp/moo.wire  <run the door>
    tools/wiredump.py /tmp/moo.wire            # summary + annotated timeline
    tools/wiredump.py /tmp/moo.wire --frames   # also dump per-sixel-frame geometry

Record format (see syncmoo1_io.c): an ASCII header line

    <ms> <I|O> <len>\\n

followed by exactly <len> raw payload bytes. Payload may contain anything,
newlines included -- always trust the length, never scan for a delimiter.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sixdecode

ESC = 0x1B


def records(path):
    data = open(path, "rb").read()
    i = 0
    while i < len(data):
        nl = data.find(b"\n", i)
        if nl < 0:
            break
        try:
            ms, direction, n = data[i:nl].split()
        except ValueError:
            break
        n = int(n)
        payload = data[nl + 1:nl + 1 + n]
        yield int(ms), direction.decode(), payload
        i = nl + 1 + n


def describe_out(buf, slot_cache=None):
    """Name the interesting outbound sequences in one record. `slot_cache`
    is the running {slot: cache_name} map threaded across the WHOLE capture
    (see sixdecode.audio_events/format_audio_event) so a Queue seen in a
    later record still names the cache file a Load put in that slot in an
    earlier one; pass the same dict across every call in a capture."""
    out = []
    for m in re.finditer(rb"\x1bP([0-9;]*)q\"(\d+);(\d+);(\d+);(\d+)", buf):
        pan, pad, w, h = (int(x) for x in m.groups()[1:])
        out.append("SIXEL enc=%dx%d pan=%d pad=%d -> shown %dx%d"
                   % (w, h, pan, pad, w * pad, h * pan))
    for payload in sixdecode.iter_apcs(buf):
        ev = sixdecode.parse_audio_apc(payload)
        if ev is not None:
            out.append("AUDIO " + sixdecode.format_audio_event(ev, slot_cache))
    for pat, name in [
        (rb"\x1b\[2J", "ED2 (clear screen)"),
        (rb"\x1b\[\?25l", "cursor hide"),
        (rb"\x1b\[\?25h", "cursor show"),
        (rb"\x1b\[\?7l", "autowrap off"),
        (rb"\x1b\[\?80l", "DECSDM ?80l"),
        (rb"\x1b\[\?80h", "DECSDM ?80h"),
        (rb"\x1b\[14t", "probe ESC[14t (canvas px)"),
        (rb"\x1b\[16t", "probe ESC[16t (cell px)"),
        (rb"\x1b\[6n", "DSR (pace probe/ack request)"),
        (rb"\x1b\[c", "DA1"),
        (rb"\x1b\[<c", "CTDA"),
        (rb"\x1b\[\?1003h", "mouse 1003 on"),
        (rb"\x1b\[\?1006h", "mouse 1006 on"),
        (rb"\x1b\[\?1016h", "mouse 1016 on"),
        (rb"\x1b\[\?1016\$p", "DECRQM ?1016"),
    ]:
        c = len(re.findall(pat, buf))
        if c:
            out.append("%s%s" % (name, "" if c == 1 else " x%d" % c))
    for m in re.finditer(rb"\x1b7\x1b\[(\d+);(\d+)H", buf):
        out.append("cursor -> row %s col %s" % (m.group(1).decode(), m.group(2).decode()))
    return out


def describe_in(buf):
    out = []
    for m in re.finditer(rb"\x1b\[<(\d+);(\d+);(\d+)([Mm])", buf):
        b, x, y, fin = int(m.group(1)), m.group(2).decode(), m.group(3).decode(), m.group(4).decode()
        base = b & ~28
        if base & 32:
            kind = "MOVE" + (" (SyncTERM no-button 96)" if base == 96 else
                             " (xterm no-button 35)" if base == 35 else "")
        elif base & 64:
            kind = "WHEEL %s" % ("down" if base & 1 else "up")
        else:
            kind = "BUTTON %d %s" % (base & 3, "release" if fin == "m" else "press")
        out.append("mouse b=%d %s @%s,%s" % (b, kind, x, y))
    for m in re.finditer(rb"\x1b\[(\d+);(\d+)R", buf):
        out.append("CPR %s;%s (grid reply or pace-ack)" % (m.group(1).decode(), m.group(2).decode()))
    for m in re.finditer(rb"\x1b\[4;(\d+);(\d+)t", buf):
        out.append("canvas reply %sx%s px" % (m.group(2).decode(), m.group(1).decode()))
    for m in re.finditer(rb"\x1b\[6;(\d+);(\d+)t", buf):
        out.append("cell-size reply %sx%s px" % (m.group(2).decode(), m.group(1).decode()))
    for m in re.finditer(rb"\x1b\[\?(\d+);(\d+)\$y", buf):
        out.append("DECRPM mode %s = %s" % (m.group(1).decode(), m.group(2).decode()))
    for m in re.finditer(rb"\x1b\[\?([0-9;]*)c", buf):
        out.append("DA reply ?%s" % m.group(1).decode())
    if not out and buf:
        out.append("%d bytes: %r%s" % (len(buf), buf[:32], "..." if len(buf) > 32 else ""))
    return out


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    path = sys.argv[1]
    show_frames = "--frames" in sys.argv

    n_in = n_out = b_in = b_out = frames = 0
    for ms, d, buf in records(path):
        if d == "O":
            n_out += 1
            b_out += len(buf)
            frames += len(re.findall(rb"\x1bP[0-9;]*q", buf))
        else:
            n_in += 1
            b_in += len(buf)

    print("== %s: %d out records (%d bytes, %d sixel frames), %d in records (%d bytes)"
          % (path, n_out, b_out, frames, n_in, b_in))
    print()

    slot_cache = {}   # audio Load->Queue cache-name map, threaded across records
    for ms, d, buf in records(path):
        lines = describe_out(buf, slot_cache) if d == "O" else describe_in(buf)
        if not show_frames:
            lines = [l for l in lines if not (d == "O" and l.startswith("SIXEL") and frames > 8)]
        for l in lines:
            print("%7d ms  %s  %s" % (ms, "-->" if d == "O" else "<--", l))


if __name__ == "__main__":
    main()
