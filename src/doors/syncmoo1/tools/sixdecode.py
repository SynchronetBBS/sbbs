#!/usr/bin/env python3
"""sixdecode.py -- stdlib-only decoder for syncmoo1's outbound wire stream:
sixel frames and SyncTERM audio APCs.

Shared by fakterm.py (drives the real door live) and wiredump.py (replays a
SYNCMOO1_WIREDUMP capture offline), so the two tools parse the exact same
wire format instead of carrying two slightly-different regexes that drift
apart. No third-party dependencies (no numpy, no Pillow) -- this has to run
anywhere the door builds.

Sixel format reminder (DEC sixel, as termgfx emits it):
    ESC P <params> q " Pan ; Pad ; Pw ; Ph  <sixel body>  ESC \\
Body alternates "#<idx>[;u;r;g;b]" (select/define color), runs of sixel data
bytes 0x3f-0x7e (6 vertical pixels each, one bit per row), "!<n><ch>"
(repeat), "$" (carriage return -- back to x=0, same y) and "-" (newline --
x=0, y+=6).

SyncTERM audio APC reminder (see ../../termgfx/audio.c/audio_mgr.c):
    ESC _ SyncTERM:Q;libsndfile ESC \\              capability query (outbound)
    ESC _ SyncTERM:C;S;<file>;<base64>  ESC \\       Store (cache upload)
    ESC _ SyncTERM:C;L;<glob>  ESC \\                client-cache list query
    ESC _ SyncTERM:C;L\\n<path>\\t<md5> ...  ESC \\    client-cache list reply (inbound)
    ESC _ SyncTERM:A;Load;S=<slot>;<file>  ESC \\     Load cache file into slot
    ESC _ SyncTERM:A;Queue;C=<ch>;S=<slot>;VL=<n>;VR=<n>[;L]  ESC \\   Queue (";L" = loop)
    ESC _ SyncTERM:A;Volume;C=<ch>;V=<n>  ESC \\               mono volume
    ESC _ SyncTERM:A;Volume;C=<ch>;VL=<n>;VR=<n>  ESC \\       stereo volume
    ESC _ SyncTERM:A;Synth;S=<slot>;W=<wave>;F=<hz>;T=<ms>  ESC \\
    ESC _ SyncTERM:A;Flush;C=<ch>[;O=<fade_ms>]  ESC \\        stop (fade or immediate)
"""
import re

ESC = 0x1B

_APC_RE = re.compile(rb"\x1b_(.*?)\x1b\\", re.DOTALL)
_SIXEL_RE = re.compile(rb"\x1bP[0-9;]*q(.*?)\x1b\\", re.DOTALL)
_RASTER_RE = re.compile(rb'"(\d+);(\d+);(\d+);(\d+)')


# ---- APC extraction --------------------------------------------------------

def iter_apcs(buf):
    """Yield each APC payload (the bytes between ESC_ and the closing ST),
    in the order they appear in buf."""
    for m in _APC_RE.finditer(buf):
        yield m.group(1)


def iter_sixel_frames(buf):
    """Yield each raw sixel body (the bytes between the DCS intro's 'q' and
    the closing ST), in the order they appear in buf."""
    for m in _SIXEL_RE.finditer(buf):
        yield m.group(1)


# ---- sixel -> indexed image -------------------------------------------------

def decode_sixel(payload):
    """Decode one raw sixel body (as yielded by iter_sixel_frames) into
    (width, height, img, pal): img is a list of `height` rows, each a list
    of `width` palette indices; pal maps index -> (r, g, b) in 0..255 (only
    entries actually defined with a color are present)."""
    m = _RASTER_RE.match(payload)
    pos = 0
    if m:
        w, h = int(m.group(3)), int(m.group(4))
        pos = m.end()
    else:
        w, h = 320, 200
    pal = {}
    img = [[0] * w for _ in range(h)]
    x = 0
    y = 0
    color = 0
    n = len(payload)
    while pos < n:
        c = payload[pos]
        if c == 0x23:  # '#' -- select/define color
            pos += 1
            m = re.match(rb"(\d+)(?:;(\d+);(\d+);(\d+);(\d+))?", payload[pos:])
            idx = int(m.group(1))
            if m.group(2):
                pr, pg, pb = int(m.group(3)), int(m.group(4)), int(m.group(5))
                pal[idx] = (pr * 255 // 100, pg * 255 // 100, pb * 255 // 100)
            color = idx
            pos += m.end()
        elif c == 0x24:  # '$' -- graphics carriage return
            x = 0
            pos += 1
        elif c == 0x2d:  # '-' -- graphics newline
            x = 0
            y += 6
            pos += 1
        elif c == 0x21:  # '!' -- repeat
            pos += 1
            m = re.match(rb"(\d+)", payload[pos:])
            rep = int(m.group(1))
            pos += m.end()
            ch = payload[pos]
            pos += 1
            _put(img, w, h, x, y, color, ch, rep)
            x += rep
        elif 0x3f <= c <= 0x7e:
            _put(img, w, h, x, y, color, c, 1)
            x += 1
            pos += 1
        else:
            pos += 1
    return w, h, img, pal


def _put(img, w, h, x, y, color, ch, rep):
    bits = ch - 0x3f
    for b in range(6):
        if bits & (1 << b):
            yy = y + b
            if yy >= h:
                continue
            for r in range(rep):
                xx = x + r
                if xx < w:
                    img[yy][xx] = color


def decode_stream(buf):
    """Yield (w, h, img, pal) for every sixel frame in buf, IN ORDER, with
    the palette carried forward frame to frame: real sixel terminals (and
    SyncTERM is no exception) keep color registers persistent across
    images in the same session, so a later frame that reuses colors an
    earlier frame already defined does not redefine them. Decoding each
    frame in total isolation (as decode_sixel() alone does) under-reports
    its palette for exactly that reason -- use this when you want the
    frame as a viewer would actually have rendered it, e.g. picking the
    LAST frame of a run to inspect."""
    pal_state = {}
    for payload in iter_sixel_frames(buf):
        w, h, img, pal = decode_sixel(payload)
        pal_state.update(pal)
        yield w, h, img, dict(pal_state)


def write_pnm(path, w, h, img, pal):
    """Write an indexed frame out as a P6 (binary PPM); undefined palette
    entries render as magenta (255, 0, 255) so a missing #define stands out."""
    with open(path, "wb") as out:
        out.write(b"P6\n%d %d\n255\n" % (w, h))
        for row in img:
            out.write(bytes(v for p in row for v in pal.get(p, (255, 0, 255))))


# ---- audio APC parsing ------------------------------------------------------

_LOAD_RE = re.compile(rb"A;Load;S=(\d+);(.*)", re.DOTALL)
_QUEUE_RE = re.compile(rb"A;Queue;C=(\d+);S=(\d+);VL=(\d+);VR=(\d+)(;L)?")
_VOL_MONO_RE = re.compile(rb"A;Volume;C=(\d+);V=(\d+)")
_VOL_STEREO_RE = re.compile(rb"A;Volume;C=(\d+);VL=(\d+);VR=(\d+)")
_SYNTH_RE = re.compile(rb"A;Synth;S=(\d+);W=([^;]*);F=(\d+);T=(\d+)")
_FLUSH_FADE_RE = re.compile(rb"A;Flush;C=(\d+);O=(\d+)")
_FLUSH_RE = re.compile(rb"A;Flush;C=(\d+)")


def parse_audio_apc(payload):
    """Parse one APC payload (as yielded by iter_apcs, WITHOUT the ESC_/ST
    wrapper) as a SyncTERM audio-transport command. Returns a dict with at
    least a "cmd" key, or None if this APC isn't one this tool understands
    (a non-audio SyncTERM extension, e.g. the Q;JXL tier probe, or the
    inbound C;L list reply, which has no fixed shape to key/value parse)."""
    if not payload.startswith(b"SyncTERM:"):
        return None
    body = payload[len(b"SyncTERM:"):]

    if body.startswith(b"C;S;"):
        rest = body[4:]
        name, sep, b64 = rest.partition(b";")
        if not sep:
            return None
        return {"cmd": "Store", "cache": name.decode("latin1"), "b64len": len(b64)}

    if body.startswith(b"C;L"):
        rest = body[3:]
        if rest[:1] == b";":
            return {"cmd": "ListQuery", "glob": rest[1:].decode("latin1")}
        return {"cmd": "ListReply", "raw": rest.decode("latin1", "replace")}

    if body.startswith(b"Q;libsndfile"):
        return {"cmd": "AudioProbe"}

    if body.startswith(b"A;Load;"):
        m = _LOAD_RE.match(body)
        if m:
            return {"cmd": "Load", "slot": int(m.group(1)),
                     "cache": m.group(2).decode("latin1")}

    if body.startswith(b"A;Queue;"):
        m = _QUEUE_RE.match(body)
        if m:
            return {"cmd": "Queue", "channel": int(m.group(1)), "slot": int(m.group(2)),
                     "vl": int(m.group(3)), "vr": int(m.group(4)),
                     "loop": m.group(5) is not None}

    if body.startswith(b"A;Volume;"):
        m = _VOL_MONO_RE.match(body)
        if m:
            return {"cmd": "Volume", "channel": int(m.group(1)), "v": int(m.group(2))}
        m = _VOL_STEREO_RE.match(body)
        if m:
            return {"cmd": "Volume", "channel": int(m.group(1)),
                     "vl": int(m.group(2)), "vr": int(m.group(3))}

    if body.startswith(b"A;Synth;"):
        m = _SYNTH_RE.match(body)
        if m:
            return {"cmd": "Synth", "slot": int(m.group(1)),
                     "wave": m.group(2).decode("latin1"),
                     "freq_hz": int(m.group(3)), "dur_ms": int(m.group(4))}

    if body.startswith(b"A;Flush;"):
        m = _FLUSH_FADE_RE.match(body)
        if m:
            return {"cmd": "Flush", "channel": int(m.group(1)), "fade_ms": int(m.group(2))}
        m = _FLUSH_RE.match(body)
        if m:
            return {"cmd": "Flush", "channel": int(m.group(1)), "fade_ms": None}

    return None


def format_audio_event(ev, slot_cache=None):
    """One human-readable line for an event dict from parse_audio_apc().
    `slot_cache`, if given, is a {slot: cache_name} dict this call may read
    (to annotate a Queue with the name last Loaded into its slot) and update
    (after a Load) -- callers fold events in wire order so this stays in
    sync with what SyncTERM would actually have in that slot."""
    cmd = ev["cmd"]
    if cmd == "Store":
        return "Store    cache=%s (%d b64 bytes)" % (ev["cache"], ev["b64len"])
    if cmd == "Load":
        if slot_cache is not None:
            slot_cache[ev["slot"]] = ev["cache"]
        return "Load     slot=%d cache=%s" % (ev["slot"], ev["cache"])
    if cmd == "Queue":
        cache = (slot_cache or {}).get(ev["slot"], "?")
        return ("Queue    channel=%d slot=%d cache=%s vol(L/R)=%d/%d loop=%s"
                % (ev["channel"], ev["slot"], cache, ev["vl"], ev["vr"],
                   "YES (;L)" if ev["loop"] else "no"))
    if cmd == "Volume":
        if "v" in ev:
            return "Volume   channel=%d vol=%d" % (ev["channel"], ev["v"])
        return "Volume   channel=%d vol(L/R)=%d/%d" % (ev["channel"], ev["vl"], ev["vr"])
    if cmd == "Synth":
        return ("Synth    slot=%d wave=%s freq=%dHz dur=%dms"
                % (ev["slot"], ev["wave"], ev["freq_hz"], ev["dur_ms"]))
    if cmd == "Flush":
        if ev["fade_ms"] is None:
            return "Flush    channel=%d (immediate stop)" % ev["channel"]
        return "Flush    channel=%d (fade %dms then stop)" % (ev["channel"], ev["fade_ms"])
    if cmd == "AudioProbe":
        return "AudioProbe (Q;libsndfile capability query)"
    if cmd == "ListQuery":
        return "ListQuery glob=%s" % ev["glob"]
    if cmd == "ListReply":
        return "ListReply %r" % ev["raw"][:80]
    return repr(ev)


def audio_events(buf):
    """Yield (event_dict, formatted_line) for every audio APC in buf, in
    wire order, with cache names threaded through Load->Queue via a local
    slot map."""
    slot_cache = {}
    for payload in iter_apcs(buf):
        ev = parse_audio_apc(payload)
        if ev is None:
            continue
        yield ev, format_audio_event(ev, slot_cache)
