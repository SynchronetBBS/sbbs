# cterm.adoc Ambiguities & Clarifications Needed

## 1. DSR 6n response in Origin Mode (DECSET ?6)
When origin mode is enabled, DSR 6n cursor position reports are relative
to the scroll region (matches VT100 behavior). Testing confirms this,
but cterm.adoc doesn't specify it. Document this explicitly.

## 2. LCF mode interaction with DSR 6n
When the Last Column Flag is set (cursor "stuck" at last column), what
position does DSR 6n report? The last column, or last column + 1?
cterm.adoc describes LCF behavior for printable characters but not for
position reporting.

## 3. CTSV (APC SyncTERM:VER) response format
The doc says SyncTERM responds with "an APC string with the full version
string of SyncTERM, either SyncTERM 1.7rc1 for release builds or
SyncTERM 1.7b Debug..." — but the actual response is
`APC SyncTERM:VER;SyncTERM 1.8b Debug... ST`. The doc should mention
the `SyncTERM:VER;` prefix in the response.

## 4. SGR 100-107 (bright background) requires DECSET ?33
The doc says "For 100-107, there is no effect unless bright background
colours are enabled." Testing confirms DECSET ?33 is the mechanism.
The doc should say explicitly: "requires DECSET mode 33 to be set".

## 5. DECRQCRA does not reflect palette changes
DECRQCRA checksums the character/attribute data, not the rendered pixel
colors. Palette redefinitions via OSC 4 don't change the checksum since
the attribute index is unchanged. This means OSC 4/104 can't be tested
via DECRQCRA. Not a doc bug, but worth noting for test authors.

## 6. DCS ASCII-mode macro content cannot contain ESC
cterm.adoc defines DCS string content as `0x08-0x0d and 0x20-0x7e`.
Since ESC (0x1B) is not in this range, ASCII-mode macros (p3=0) cannot
contain escape sequences. Only hex-mode (p3=1) can encode ESC. The doc
should clarify this more prominently since it's a common pitfall.

## 7. SGR 2 (dim) behavior should be documented
SGR 2 is listed in cterm.adoc as "Dim intensity" but in PC text mode
there is no separate dim attribute. The implementation treats SGR 2 as
clearing the bold/bright bit (same effect as SGR 22). The doc should
clarify that SGR 2 clears the bright attribute rather than setting a
distinct "dim" state, since PC text mode only has normal/bright, not
normal/dim/bright.

## 8. Copy/Paste (CTCSIB/CTPBTS) operates on pixel framebuffer
Copy/Paste via APC commands operates on the pixel framebuffer (uses
getpixels()/freepixels()), not the character cell buffer. DECRQCRA
checksums the cell buffer, so it cannot verify Copy/Paste results.
The doc should clarify that these are pixel-level operations.
X/Y/W/H coordinates are in pixels (defaults derived from
vparams[vmode].xres/.yres).
