#!/usr/bin/env python3
"""Simple SyncTERM physical-key protocol tester.

Run this under SyncTERM/CTerm. It enables physical key reports, suppresses
normal translated key output, and shows the currently pressed evdev key codes.
Press physical Esc or Q to exit.
"""

import os
import re
import select
import sys
import termios
import time
import tty


CSI = "\x1b["
ENABLE = CSI + "=1h" + CSI + "=2h"
DISABLE = CSI + "=2l" + CSI + "=1l"
CLEAR = CSI + "2J" + CSI + "H"
HIDE_CURSOR = CSI + "?25l"
SHOW_CURSOR = CSI + "?25h"

REPORT_RE = re.compile(rb"\x1b\[=([0-9;]*)?([Kk])")

KEY_NAMES = {
    1: "ESC",
    2: "1",
    3: "2",
    4: "3",
    5: "4",
    6: "5",
    7: "6",
    8: "7",
    9: "8",
    10: "9",
    11: "0",
    12: "MINUS",
    13: "EQUAL",
    14: "BACKSPACE",
    15: "TAB",
    16: "Q",
    17: "W",
    18: "E",
    19: "R",
    20: "T",
    21: "Y",
    22: "U",
    23: "I",
    24: "O",
    25: "P",
    26: "LEFTBRACE",
    27: "RIGHTBRACE",
    28: "ENTER",
    29: "LEFTCTRL",
    30: "A",
    31: "S",
    32: "D",
    33: "F",
    34: "G",
    35: "H",
    36: "J",
    37: "K",
    38: "L",
    39: "SEMICOLON",
    40: "APOSTROPHE",
    41: "GRAVE",
    42: "LEFTSHIFT",
    43: "BACKSLASH",
    44: "Z",
    45: "X",
    46: "C",
    47: "V",
    48: "B",
    49: "N",
    50: "M",
    51: "COMMA",
    52: "DOT",
    53: "SLASH",
    54: "RIGHTSHIFT",
    55: "KPASTERISK",
    56: "LEFTALT",
    57: "SPACE",
    58: "CAPSLOCK",
    59: "F1",
    60: "F2",
    61: "F3",
    62: "F4",
    63: "F5",
    64: "F6",
    65: "F7",
    66: "F8",
    67: "F9",
    68: "F10",
    69: "NUMLOCK",
    70: "SCROLLLOCK",
    71: "KP7",
    72: "KP8",
    73: "KP9",
    74: "KPMINUS",
    75: "KP4",
    76: "KP5",
    77: "KP6",
    78: "KPPLUS",
    79: "KP1",
    80: "KP2",
    81: "KP3",
    82: "KP0",
    83: "KPDOT",
    86: "102ND",
    87: "F11",
    88: "F12",
    96: "KPENTER",
    97: "RIGHTCTRL",
    98: "KPSLASH",
    99: "SYSRQ",
    100: "RIGHTALT",
    102: "HOME",
    103: "UP",
    104: "PAGEUP",
    105: "LEFT",
    106: "RIGHT",
    107: "END",
    108: "DOWN",
    109: "PAGEDOWN",
    110: "INSERT",
    111: "DELETE",
    119: "PAUSE",
    125: "LEFTMETA",
    126: "RIGHTMETA",
    139: "MENU",
    183: "F13",
    184: "F14",
    185: "F15",
    186: "F16",
    187: "F17",
    188: "F18",
    189: "F19",
    190: "F20",
    191: "F21",
    192: "F22",
    193: "F23",
    194: "F24",
}


def key_name(code):
    return KEY_NAMES.get(code, f"KEY_{code}")


def parse_reports(buf, pressed, events):
    pos = 0
    while True:
        match = REPORT_RE.search(buf, pos)
        if match is None:
            break
        is_press = match.group(2) == b"K"
        params = match.group(1) or b""
        for part in params.split(b";"):
            if not part:
                continue
            try:
                code = int(part)
            except ValueError:
                continue
            if is_press:
                pressed.add(code)
            else:
                pressed.discard(code)
            events.append((time.monotonic(), is_press, code))
        pos = match.end()
    return buf[pos:]


def render(pressed, events, raw_tail):
    rows = [
        CLEAR,
        "SyncTERM physical key tester\r\n",
        "Press physical Esc or Q to exit.\r\n\r\n",
        "Pressed keys:\r\n",
    ]
    if pressed:
        for code in sorted(pressed):
            rows.append(f"  {code:3d}  {key_name(code)}\r\n")
    else:
        rows.append("  (none)\r\n")

    rows.append("\r\nRecent events:\r\n")
    for _, is_press, code in events[-12:]:
        edge = "down" if is_press else "up  "
        rows.append(f"  {edge} {code:3d}  {key_name(code)}\r\n")

    if raw_tail:
        rows.append("\r\nBuffered partial input:\r\n")
        rows.append(f"  {raw_tail!r}\r\n")

    sys.stdout.write("".join(rows))
    sys.stdout.flush()


def main():
    if not sys.stdin.isatty() or not sys.stdout.isatty():
        print("stdin and stdout must be a terminal", file=sys.stderr)
        return 1

    old = termios.tcgetattr(sys.stdin)
    pressed = set()
    events = []
    buf = b""

    exiting = False
    exit_keys = {1, 16}

    try:
        tty.setcbreak(sys.stdin.fileno())
        sys.stdout.write(ENABLE + HIDE_CURSOR)
        sys.stdout.flush()
        render(pressed, events, buf)

        while True:
            readable, _, _ = select.select([sys.stdin], [], [], 0.25)
            if readable:
                chunk = os.read(sys.stdin.fileno(), 4096)
                if not chunk:
                    break
                buf += chunk
                buf = parse_reports(buf, pressed, events)
                render(pressed, events, buf)
                if not exiting and pressed & exit_keys:
                    exiting = True
                if exiting and not pressed & exit_keys:
                    break
    finally:
        sys.stdout.write(DISABLE + SHOW_CURSOR + CLEAR)
        sys.stdout.flush()
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
