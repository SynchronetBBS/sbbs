#!/usr/bin/env python3
# gen_splash.py -- generate the SyncDOOM waiting-room splash (sd_splash.h).
#
# Emits sd_splash.h: an 80x25 grid of {CP437 char, CGA attribute} cells (the
# "ENDOOM" format the door's sd_render_screen() draws), plus a PNG preview for
# eyeballing. The art is a bespoke DOOM wordmark -- crisp rasterized letters
# rendered with shaded-block dithering (the way real ANSI art grades color):
# `(0xB0 (0xB1 (0xB2 shade glyphs mix a bright foreground over a dark background
# to make intermediate tones, e.g. yellow over red = orange. A white-hot top
# melts down a cyan-chrome band into a yellow->orange->red fire, with a dark-red
# 3D bevel on the right/bottom letter edges, a left highlight, and flame drips.
# NOT derived from any commercial Doom asset, so it ships safely in the tree.
#
# Run from anywhere; writes sd_splash.h beside the door sources (the parent of
# this tools/ dir) and the preview to /tmp/syncdoom_splash.png.
#
#   python3 tools/gen_splash.py
#
# Needs Pillow + numpy + a DejaVu Sans Mono font (Debian: fonts-dejavu-core).

import os
import random
import numpy as np
from PIL import Image, ImageDraw, ImageFont

W, H = 80, 25
HERE = os.path.dirname(os.path.abspath(__file__))
OUT_H = os.path.join(HERE, os.pardir, "sd_splash.h")
OUT_PNG = "/tmp/syncdoom_splash.png"
FONT_BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf"
FONT_REG = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"

CGA = [(0, 0, 0), (0, 0, 170), (0, 170, 0), (0, 170, 170),
       (170, 0, 0), (170, 0, 170), (170, 85, 0), (170, 170, 170),
       (85, 85, 85), (85, 85, 255), (85, 255, 85), (85, 255, 255),
       (255, 85, 85), (255, 85, 255), (255, 255, 85), (255, 255, 255)]

SP, FULL = 0x20, 0xDB
SH25, SH50, SH75 = 0xB0, 0xB1, 0xB2   # the shade glyphs

# grid of (cp437_byte, attr); attr = fg | (bg<<4), fg 0-15, bg 0-7 (dark only --
# the door emits no bright backgrounds).
grid = [[(SP, 0x00) for _ in range(W)] for _ in range(H)]


def put(r, c, gb, fg, bg=0):
    if 0 <= r < H and 0 <= c < W:
        grid[r][c] = (gb, (fg & 0x0F) | ((bg & 7) << 4))


# Per-cell (glyph, fg, bg) for a letter cell at vertical fraction f in [0,1],
# dithered by a checkerboard phase p so band boundaries break up instead of
# banding. Bright fg over a dark bg via a shade glyph = the in-between tone.
def band(f, p):
    if f < 0.10:  return (FULL, 15, 0)                            # white-hot top
    if f < 0.20:  return (SH75, 15, 3) if p else (FULL, 15, 0)    # chrome (white/cyan)
    if f < 0.30:  return (FULL, 11, 0) if p else (SH50, 15, 3)    # bright cyan + speck
    if f < 0.38:  return (SH50, 15, 3)                            # chrome seam
    if f < 0.46:  return (FULL, 15, 0) if p else (FULL, 14, 0)    # white -> yellow
    if f < 0.56:  return (FULL, 14, 0)                            # yellow (hot fire)
    if f < 0.66:  return (SH75, 14, 4) if p else (FULL, 14, 0)    # yellow w/ orange
    if f < 0.74:  return (SH50, 14, 4)                            # orange
    if f < 0.82:  return (SH25, 14, 4) if p else (FULL, 12, 0)    # deep orange/red
    if f < 0.92:  return (FULL, 12, 0) if p else (SH50, 12, 4)    # bright red
    return (FULL, 4, 0) if p else (SH75, 4, 0)                    # dark-red base


# Rasterize "DOOM" heavy, then map filled pixels to cells.
LW, LH = 74, 18
R0, C0 = 1, (W - LW) // 2
fnt = ImageFont.truetype(FONT_BOLD, 34)
msk = Image.new("L", (360, 52), 0)
ImageDraw.Draw(msk).text((2, 1), "DOOM", fill=255, font=fnt, stroke_width=1, stroke_fill=255)
msk = msk.crop(msk.getbbox()).resize((LW, LH))
m = np.asarray(msk) > 110

for cr in range(LH):
    for cc in range(LW):
        if not m[cr, cc]:
            continue
        f = cr / (LH - 1)
        p = (cr + cc) & 1
        right = cc + 1 >= LW or not m[cr, cc + 1]
        bottom = cr + 1 >= LH or not m[cr + 1, cc]
        left = cc - 1 < 0 or not m[cr, cc - 1]
        if (right or bottom) and f > 0.2:
            put(R0 + cr, C0 + cc, FULL, 4, 0)            # dark-red bevel edge
        elif left and f < 0.5:
            put(R0 + cr, C0 + cc, FULL, 15, 0)           # left chrome highlight
        else:
            gb, fg, bg = band(f, p)
            put(R0 + cr, C0 + cc, gb, fg, bg)

# molten drips off the letter bottoms
random.seed(7)
for cc in range(LW):
    if m[LH - 1, cc] and random.random() < 0.5:
        put(R0 + LH, C0 + cc, random.choice([SH50, SH25, FULL]), random.choice([12, 4]), 0)
        if random.random() < 0.3:
            put(R0 + LH + 1, C0 + cc, SH25, 4, 0)

# PNG preview
CW, CH = 8, 16
cp = {i: bytes([i]).decode("cp437") for i in range(256)}
img = Image.new("RGB", (W * CW, H * CH), (0, 0, 0))
dr = ImageDraw.Draw(img)
gf = ImageFont.truetype(FONT_REG, 14)
for y in range(H):
    for x in range(W):
        b, attr = grid[y][x]
        fg, bg = attr & 0x0F, (attr >> 4) & 7
        if bg:
            dr.rectangle([x * CW, y * CH, x * CW + CW, y * CH + CH], fill=CGA[bg])
        ch = cp[b]
        if ch != " ":
            dr.text((x * CW, y * CH - 1), ch, fill=CGA[fg], font=gf)
img.save(OUT_PNG)
print("wrote", OUT_PNG, img.size)

# C array (ENDOOM format: char,attr per cell, row-major)
with open(OUT_H, "w") as f:
    f.write("// SyncDOOM waiting-room splash (bespoke DOOM wordmark, 80x25 char+CGA-attr).\n")
    f.write("// Generated by tools/gen_splash.py (crisp letters + shaded-block dithering).\n")
    f.write("// Edit the generator, not this header.\n")
    f.write("static const unsigned char sd_splash[80 * 25 * 2] = {\n")
    out = []
    for y in range(H):
        for x in range(W):
            b, a = grid[y][x]
            out.append("0x%02x,0x%02x" % (b, a))
    for i in range(0, len(out), 8):
        f.write("\t" + ",".join(out[i:i + 8]) + ",\n")
    f.write("};\n")
print("wrote", OUT_H)

# Editable runtime copy: raw 80x25 "binary text" (.bin) -- char,attr per cell,
# row-major -- in the door's xtrn dir. A sysop edits this in PabloDraw/Moebius to
# re-skin the waiting room with no rebuild; the door loads it over the baked-in
# default (sd_render_screen / [game] splash).
OUT_BIN = os.path.abspath(os.path.join(HERE, os.pardir, os.pardir, os.pardir,
                                       os.pardir, "xtrn", "syncdoom", "waiting.bin"))
with open(OUT_BIN, "wb") as f:
    for y in range(H):
        for x in range(W):
            b, a = grid[y][x]
            f.write(bytes((b, a)))
print("wrote", OUT_BIN)
