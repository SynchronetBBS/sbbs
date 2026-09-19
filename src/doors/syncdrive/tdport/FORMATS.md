# Test Drive (1987, DSI/Accolade) — file formats & internals

Everything under "Verified" was confirmed against the disassembly and/or by exact size + CRC
round-trips on every shipped file. Unverified items are marked as such.

## Tools (run from this folder, Python 3.12 + Pillow + numpy + capstone)

| Script | Purpose |
|---|---|
| `tools/unexepack.py IN.EXE OUT.EXE` | Unpacks Microsoft EXEPACK (all three EXEs are packed) |
| `tools/x86dis.py EXE find HEX` / `dis OFF LEN` | Byte search / 16-bit disassembly on image offsets |
| `tools/tdres.py info|export FILE [OUTDIR]` | Decodes `.PES`/`.CMP` archives, exports each sprite as `.bin` + `.png` |
| `tools/sheet.py Game work/sheets` | One labelled contact sheet per archive |

Outputs: `work/ega/<ARCHIVE>/*.png` (541 sprites), `work/cga/<ARCHIVE>/*.png` (552 sprites),
`work/sheets/*.png`.

## Executables (Verified)

* `TD.EXE` – launcher: menu "1 CGA/EGA 4 colours, 2 EGA 16 colours, 3 Hercules", joystick calibration,
  then execs `tdcga.exe` or `tdega.exe`.
* `TDEGA.EXE` / `TDCGA.EXE` – the game. Microsoft C (1986 runtime) + hand-written assembly.
  EXEPACK-packed. TDEGA: 65 584 → 81 424 bytes unpacked, entry 0000:A50C, DGROUP segment 0x0C9A
  (DS:x = image offset 0xC9A0 + x), ~52 KB code, ~170 C function prologues.
* Hardware touched by TDEGA: `int 10h` (mode 0Dh 320×200/16 col; mode 4 CGA and mode 7 Hercules code
  also present). Palette: the mode-set routine 0x4D96 loads the stock table DS:637C, then `main`
  (0x5D) loads the game palette DS:00CC via `int 10h AX=1002h` and keeps it for the whole game:
  register values `00 01 02 03 04 05 07 16 00 10 06 12 13 14 11 17`, i.e. index 6 = light grey,
  7 = yellow, 8 = black, 9 = dark grey, 10 = brown, 11 = light green, 12 = light cyan,
  13 = light red, 14 = light blue, 15 = white, `int 16h` keyboard,
  `int 21h` files, timer `int 8` and `int 0` vectors hooked, PIT/speaker ports 40h–43h/61h,
  PIC 20h/21h, ~190 `out dx,al` (EGA sequencer/graphics-controller plane writes in blitters).
* Copy protection (image 0x8E80–0x91CF): `int 11h` floppy count, then `int 13h` reads of sectors with
  impossible IDs (0xF1/0xDE on track 39) expecting a CRC-error status and a 4-word signature
  (table at 0x8DB0); variant for hard-disk installs (`dl=80h`).
* `ROADDATA.SHP` string exists in the data segment but has no code references (leftover).

## Resource archive (Verified)

Both `.PES` (EGA) and `.CMP` (CGA) decompress to the same layout:

```
u32  total_size
u16  count
count × char[4]  names (zero padded; a few CGA names start with 0x00)
count × u32      offsets, relative to end of this table
...  resource data
```

### `.CMP` container (CGA)
`u32 unpacked_size` stored raw (it is also the archive's first field), followed by an RLE stream:
`83 vv nn` = byte `vv` repeated `nn` times, any other byte is literal.

### `.PES` container (EGA) — it is ARC's compression methods verbatim
```
char[4] "Pckd"
u32     packed_len (file size − 16)
u32     unpacked_len
u16     method      8 = "crunched" (LZW), 4 = "squeezed" (Huffman)   (only TESTDRV.PES uses 4)
u16     CRC-16/ARC  (poly 0xA001, init 0) of the final unpacked data
...     payload
```
* Method 8: `u8 maxbits (=12)` + Unix `compress` 4.0 LZW (9→12 bit codes, CLEAR=256, first free 257,
  codes read `n_bits` bytes at a time so leftover bits are discarded on width change/clear).
  Decoder at TDEGA image 0x9E40, getcode 0x9FA2.
* Method 4: `u16 node_count`, nodes of two `s16` children (negative = leaf `-(byte+1)`, leaf 256 = EOF),
  bitstream LSB-first.
* Both then pass through RLE90 (routine at 0x9CF9): `90 nn` repeats the previous byte `nn−1` more times,
  `90 00` is a literal 0x90.
* All 18 `.PES` files decode to exactly `unpacked_len` with matching CRC.

### Sprite resource
```
u16 width_in_bytes, u16 height, u16 hot_x, u16 hot_y, s16 x, s16 y, u8 planemap[4]
pixel data
```
* CGA: `height` rows of `width` bytes, 2 bits/pixel MSB first (320 px = 80 bytes). planemap unused.
* EGA: one block of `height × width` bytes per stored bit plane (1 bit/pixel, 320 px = 40 bytes).
  For planemap byte k, the low nibble lists the colour planes the k-th stored plane is written to.
  The high nibble of byte 0 lists planes **cleared** over the sprite rectangle, the high nibble of
  byte 1 lists planes **set** (or XORed, depending on the blit operation); byte 3 is padding
  (verified in the blitters, see `port/spec/platform.md`).
  Examples: `01 02 04 08` = normal 4 planes; `09 06 00 00` = 2 planes (planes 0+3, planes 1+2);
  `87 00 00 00` = 1 plane into planes 0–2 with plane 3 cleared (colour 0 or 7);
  `07 80 00 00` = 1 plane into planes 0–2 with plane 3 set (colour 8 or 15).
* There is no transparent colour: masked drawing is an AND blit of a mask sprite (`rg0m`) followed
  by an OR blit of the image (`rig0`).

### Archive contents
| Archive | Contents |
|---|---|
| `TESTDRV` | title logo, key fob, side view car + wheel frames, passenger window animation (`wnd0…R`) |
| `ACCOLADE`, `SLOGO`, `LLOGO` | publisher logo; car-select / high-score logos |
| `<CAR>` | cockpit: `dash`, `inst`, `roof`, steering wheel `whl1/3`, gear box `gbox`/knob, mirror, radar detector lights `rad0-5` |
| `<CAR>SB` | showroom: spec sheet `stat` (with acceleration graph), car, window animation, name plate |
| `GAS` | gas station screen + four station signs (`don`, `john`, `kevn`, `tony`) |
| `XROADA` | road-side objects: cliffs, poles, rocks, lane lines, speed/turn/two-way/gas signs at 5 scales, oil, potholes, gravel, `deal` (THE END MOTORS), `note` ("Nice job. Keep the car. Go home."), `tick` (speeding ticket), `govr` (GAME OVER) |
| `XROADB` / `XROADC` | traffic at 5 scales: truck, rig, RX-7, van front/rear, sedan, cop car front/rear with light frames |

## Road data (embedded in TDEGA.EXE; decoded by `tools/roadmap.py`)

Verified from the disassembly unless noted. DS = image 0xC9A0.

| What | DS | Image |
|---|---|---|
| Record table, 0x6E × `[flag, curve, pitch, object]` | 0x2B70 | 0xF510 |
| Road byte stream, 5 stages, each ends with `0xFF` | 0x2D28–0x6360 | 0xF6C8–0x12D00 |
| Stage start pointers, 5 × u16 | 0x6361 | 0x12D01 |

(TDCGA: same stream at image 0xD598; its pointers are offset by −0x30.)

* One stream byte = one road unit = an index into the record table.
  * Codes 01–08: right curves (+1 +2 +4 +6 +9 +12 +16 +20). Codes 09–10: the matching left curves.
  * Codes 11–18: hills (pitch ±8 … ±64).
  * Object codes, low 6 bits: 2–8 signs (right turn, left turn, two-way, three speed limits, gas station; bit 0x80 = other side of the road), 1 police, 0x10–0x14 / 0x18–0x1C traffic (top 2 bits = spawn-chance threshold), 0x20–0x23 hazards (top 2 bits = lane).
* Stage lengths: 2097, 2913, 2845, 2674 and 3347 units.
* Moving forward (0x3FE8): a sub-unit counter drops by speed/128 each tick and gets +90 on each unit advance. The curve value ×64 pushes the car sideways; the road edge is at ±0x264.
* Look-ahead (0x4241) reads 40 units ahead: `0xFF` there triggers the gas-station sequence, and object codes spawn traffic, police or hazards (0x467D).
* Road drawing (0x2054): 40 rows. heading += curve×64 (clamped to ±75°), x += sin(heading); slope += pitch×4, y += sin(slope).
* **Schematic only:** the game only uses heading within the 40-row window, so integrating it over a whole stage gives loops (stage 3 nets +1022°). The pitch values don't add up to a consistent altitude either.

## Car files (`tools/cardata.py` → `work/cars/NAME.json`)

`NAME.BIN` is read raw, 0x4D6 bytes, into DS:268F (code at image 0x10E2). Fields below are verified in TDEGA unless marked.

| Offset | Field |
|---|---|
| +000 | number of gears |
| +004 | rev limit; going over it damages the engine |
| +008 | grip limit, above which the car skids; +00A is the skid drift |
| +00C/+00E | steering-wheel sprite x,y (inferred) |
| +012 | 7 gear ratios (gear 0 = neutral); rpm = ratio × speed ÷ 65536 |
| +020 | gear-knob x,y for each gear |
| +03C | 16 shift-gate nodes; +07C is a 9×16 table of joystick direction × node → next node; +10C maps node → gear |
| +11C | 80-byte torque curve, one entry per 128 rpm |
| +16C | 1 = needle gauges, otherwise digital (Corvette); +16E is the needle pivots; +17A speedo tips by speed; +328 tach tips by rpm ÷ 64 |

* Acceleration = (torque × ratio, ×1.5 in 1st, − drag) ÷ 64. The drag table, braking and idle rpm (800) are in the EXE.
* Unused by TDEGA: +002, +006, +010 and +172..+179.
* `NAME.SS` is text: two numbers, then the lists of wheel and window animation frame names.

## TDSND.SND (`tools/sndplay.py` → `work/sound/*.wav`)

* A standard resource archive holding 4 songs (`sng1`–`sng4`) of bytecode.
* The player runs on a 100 Hz timer and drives the PC speaker. Play function at image 0x8A3E, queue function at 0x8A0E.
* Events: a note number (note 55 ≈ A440, 0 = rest; divisor table at DS:6452) followed by a u16 duration in ticks.
* Control codes:
  * `FE` sets the note cut-off.
  * `FD/FC/FB n` start loops 1–3; `FA/F9/F8` end them.
  * `F7/F6/F5` skip the rest of a loop on its last pass.
  * `FF` ends the song or chains to the next.

## SCORES

* 8 entries, each written with `%-20.20s%-20.20s%ld` plus a CRLF.
* The line after each entry is a `%x` CRC-8 checksum: reflected polynomial 0xB8, seeded with the row number 0–7, computed over the formatted line plus `\n`.
* Entries with a bad checksum are skipped when the file is loaded.
