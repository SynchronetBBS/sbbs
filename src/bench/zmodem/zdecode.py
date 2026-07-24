#!/usr/bin/env python3
"""Decode a ZMODEM wire capture (from zbench_sock.py --tap) into a frame trace.

Usage: zdecode.py wire.fwd [--from-offset N] [--limit N]

Prints one line per header/subpacket:
    <byte-offset>  <frame>  [pos=<file position>]  [len=<subpacket bytes>]

Intended for diffing a passing run against a failing one around a ZRPOS.  It
resynchronises on ZPAD ZDLE, so it recovers after corrupted stretches the way a
receiver does.
"""
import sys

ZPAD, ZDLE, ZBIN, ZHEX, ZBIN32 = 0x2a, 0x18, 0x41, 0x42, 0x43
FRAME = {0: 'ZRQINIT', 1: 'ZRINIT', 2: 'ZSINIT', 3: 'ZACK', 4: 'ZFILE',
         5: 'ZSKIP', 6: 'ZNAK', 7: 'ZABORT', 8: 'ZFIN', 9: 'ZRPOS',
         10: 'ZDATA', 11: 'ZEOF', 12: 'ZFERR', 13: 'ZCRC', 14: 'ZCHALLENGE',
         15: 'ZCOMPL', 16: 'ZCAN', 17: 'ZFREECNT', 18: 'ZCOMMAND',
         19: 'ZSTDERR'}
SUBPKT = {0x68: 'ZCRCE', 0x69: 'ZCRCG', 0x6a: 'ZCRCQ', 0x6b: 'ZCRCW'}


def unescape(buf, i, want):
    """Read `want` unescaped bytes starting at i; return (bytes, next_i)."""
    out = bytearray()
    while len(out) < want and i < len(buf):
        c = buf[i]
        i += 1
        if c == ZDLE:
            if i >= len(buf):
                break
            d = buf[i]
            i += 1
            if d in (0x6c, 0x6d):          # ZRUB0/ZRUB1
                out.append(0x7f if d == 0x6c else 0xff)
            elif d & 0x60 == 0x40:
                out.append(d ^ 0x40)
            else:
                return None, i             # frame end or garbage
        else:
            out.append(c)
    return bytes(out), i


def main():
    path = sys.argv[1]
    start = 0
    limit = 10 ** 9
    for k, a in enumerate(sys.argv):
        if a == '--from-offset':
            start = int(sys.argv[k + 1])
        if a == '--limit':
            limit = int(sys.argv[k + 1])
    buf = open(path, 'rb').read()

    i = 0
    printed = 0
    in_data = False
    data_start = 0
    while i < len(buf) - 2 and printed < limit:
        if buf[i] == ZPAD and buf[i + 1] == ZDLE or \
           (buf[i] == ZPAD and buf[i + 1] == ZPAD and buf[i + 2] == ZDLE):
            off = i
            i += 3 if buf[i + 1] == ZPAD else 2
            if i >= len(buf):
                break
            kind = buf[i]
            i += 1
            body = None
            if kind == ZHEX:
                hx = buf[i:i + 14]
                i += 14
                try:
                    body = bytes.fromhex(hx.decode('ascii'))
                except Exception:
                    continue
            elif kind in (ZBIN, ZBIN32):
                body, i = unescape(buf, i, 5)
                if body is None or len(body) < 5:
                    continue
                i += 2 if kind == ZBIN else 4   # skip CRC (already unescaped)
            else:
                continue
            if not body:
                continue
            t = body[0]
            pos = body[1] | (body[2] << 8) | (body[3] << 16) | (body[4] << 24)
            name = FRAME.get(t, 'Z?%d' % t)
            if off >= start:
                print('%9d  %-9s pos=%d' % (off, name, pos))
                printed += 1
            in_data = (name == 'ZDATA')
            data_start = i
            continue
        if in_data and buf[i] == ZDLE and i + 1 < len(buf) and buf[i + 1] in SUBPKT:
            if i >= start:
                print('%9d    %-7s len=%d' % (i, SUBPKT[buf[i + 1]], i - data_start))
                printed += 1
            i += 2
            data_start = i + 4
            continue
        i += 1


if __name__ == '__main__':
    main()
