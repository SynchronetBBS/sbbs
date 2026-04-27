/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Per-connection Wren scripting host for SyncTERM.  The VM, hook
 * registry, and dispatchers all live here; the foreign-method bodies
 * (Screen/Input/Clipboard/Conn/CTerm/BBS/Cache/Hook) live in wren_bind.c.
 *
 * Lifecycle: doterm() calls wren_host_init(bbs) before its main loop
 * and wren_host_shutdown() at every exit path.  Scripts in
 * SYNCTERM_PATH_SCRIPTS are loaded as individual modules; their
 * top-level code runs at module-load time and registers callbacks
 * via Hook.onKey/onInput/etc.
 *
 * Thread affinity: WrenVM is single-threaded.  The owner thread is
 * captured at init; dispatchers from other threads (e.g. background
 * SFTP/SSH calling conn_send) short-circuit to pass-through. */

#include "wren_host.h"
#include "wren_host_internal.h"

#include "wren.h"

#include "syncterm.h"
#include "bbslist.h"
#include "ciolib.h"
#include "dirwrap.h"
#include "genwrap.h"      /* xp_timer */
#include "threadwrap.h"   /* pthread_self */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------------
 * Embedded "syncterm" module — declares all foreign classes that user
 * scripts import.  The actual method implementations are bound at
 * runtime via wren_bind_lookup() (see wren_bind.c).
 * -------------------------------------------------------------------- */

static const char SYNCTERM_MODULE_SRC[] =
    /* Capability flags for the current display backend.  Read-only;
     * reflect cio_api.options.  Underscore prefix marks the class as
     * an implementation detail of Screen — convention in Wren is to
     * not import names starting with `_` from another module. */
    "class ScreenSupports {\n"
    "  foreign static loadableFonts\n"
    "  foreign static altBlinkFont\n"
    "  foreign static altBoldFont\n"
    "  foreign static brightBackground\n"
    "  foreign static paletteChange\n"
    "  foreign static pixels\n"
    "  foreign static customCursor\n"
    "  foreign static fontSelection\n"
    "  foreign static windowTitle\n"
    "  foreign static windowName\n"
    "  foreign static windowIcon\n"
    "  foreign static extendedPalette\n"
    "  foreign static blockyScaling\n"
    "  foreign static externalScaling\n"
    "  foreign static closeLock\n"
    "}\n"
    /* Screen.font[i] — read/write the font index in slot i (0..4).
     * Same `_X` namespacing trick as Screen.supports. */
    "class ScreenFonts {\n"
    "  foreign static [i]\n"
    "  foreign static [i]=(n)\n"
    "}\n"
    /* Screen.window.* — operations scoped to the active text window.
     * Cursor position, putChar/print, clear, deleteLine/insertLine,
     * and the text attribute all act inside the window's rectangle.
     * .bounds is the [sx, sy, ex, ey] of the window (1-based, inclusive). */
    "class ScreenWindow {\n"
    "  foreign static bounds\n"                            /* -> [sx, sy, ex, ey] */
    "  foreign static bounds=(box)\n"
    "  foreign static position\n"                          /* -> [x, y] window-relative */
    "  foreign static position=(coord)\n"
    "  foreign static putChar(c)\n"
    "  foreign static print(s)\n"
    "  foreign static clear()\n"
    "  foreign static clearToLineEnd()\n"
    "  foreign static deleteLine()\n"
    "  foreign static insertLine()\n"
    "  foreign static scroll()\n"                          /* scroll window up one line */
    "}\n"
    /* Screen.palette[i] / Screen.palette.mode — colors are 24-bit
     * 0xRRGGBB.  Per-entry [_]/[_]= covers the full palette range
     * (extended palettes go beyond 16); .mode is the 16-color
     * legacy-attribute palette as a List, get or set in bulk.
     *
     * NOTE: term.c, cterm.c, and ripper.c all do their own palette
     * tricks; concurrent edits from a script and from the terminal
     * code may not compose cleanly until that's cleaned up. */
    "class Palette {\n"
    "  foreign static [i]\n"
    "  foreign static [i]=(c)\n"
    "  foreign static mode\n"
    "  foreign static mode=(list)\n"
    "}\n"
    "foreign class Screen {\n"
    /* Whole-screen / absolute operations.  Cursor, attribute,
     * clearing, line insert/delete, and direct character output
     * all live under Screen.window — they're window-scoped. */
    "  foreign static size\n"                              /* -> [w, h] whole screen */
    "  foreign static save()\n"                            /* returns handle */
    "  foreign static restore(handle)\n"
    "  foreign static readRect(sx, sy, ex, ey)\n"          /* absolute coords */
    "  foreign static writeRect(sx, sy, ex, ey, cells)\n"
    "  foreign static moveRect(sx, sy, ex, ey, dx, dy)\n"
    /* Singleton state shared by all window output. */
    "  foreign static attr=(a)\n"
    /* Hyperlink that gets attached to cells written via window
     * output (putChar, print).  0 = no hyperlink.  Use Hyperlinks.add
     * to allocate an ID; assign that ID here so subsequent writes
     * carry it. */
    "  foreign static hyperlinkId\n"
    "  foreign static hyperlinkId=(id)\n"
    /* Screen.supports.pixels / Screen.font[i] / Screen.cursor.blink /
     * Screen.videoFlags.bgBright / Screen.color.fromRgb(...) /
     * Screen.window.position — these static getters return the
     * helper class object so the script can access its static
     * members (live API) or call .new() to snapshot.  The classes
     * themselves carry a `_` prefix so user scripts don't import
     * them by accident. */
    "  static window     { ScreenWindow   }\n"
    "  static supports   { ScreenSupports }\n"
    "  static font       { ScreenFonts    }\n"
    "  static palette    { Palette        }\n"
    "  static cursor     { CustomCursor   }\n"
    "  static videoFlags { VideoFlags     }\n"
    "  static color      { Color          }\n"
    "}\n"
    /* CustomCursor — the cursor's startline/endline/range/blink/visible
     * geometry.  Static methods read/write the live cursor immediately;
     * an instance (`CustomCursor.new()` or `.current`) snapshots the
     * current values, lets you stage edits, and `.apply()` commits them
     * atomically.  Same shape for VideoFlags below. */
    "foreign class CustomCursor {\n"
    "  construct new() {}\n"
    "  static current { new() }\n"
    /* Pre-defined cursor snapshots that match the legacy
     * setcursortype() codes (0 = none, 1 = solid block, 2 = normal
     * mode-default).  Each returns a fresh CustomCursor instance
     * the script can apply, or modify before applying. */
    "  static normal  { presetLegacy_(2) }\n"
    "  static solid   { presetLegacy_(1) }\n"
    "  static none    { presetLegacy_(0) }\n"
    "  foreign static presetLegacy_(legacyType)\n"
    "  foreign static startLine\n"
    "  foreign static startLine=(n)\n"
    "  foreign static endLine\n"
    "  foreign static endLine=(n)\n"
    "  foreign static range\n"
    "  foreign static range=(n)\n"
    "  foreign static blink\n"
    "  foreign static blink=(b)\n"
    "  foreign static visible\n"
    "  foreign static visible=(b)\n"
    "  foreign startLine\n"
    "  foreign startLine=(n)\n"
    "  foreign endLine\n"
    "  foreign endLine=(n)\n"
    "  foreign range\n"
    "  foreign range=(n)\n"
    "  foreign blink\n"
    "  foreign blink=(b)\n"
    "  foreign visible\n"
    "  foreign visible=(b)\n"
    "  foreign apply()\n"
    "}\n"
    "foreign class VideoFlags {\n"
    "  construct new() {}\n"
    "  static current { new() }\n"
    "  foreign static altChars\n"
    "  foreign static altChars=(b)\n"
    "  foreign static noBright\n"
    "  foreign static noBright=(b)\n"
    "  foreign static bgBright\n"
    "  foreign static bgBright=(b)\n"
    "  foreign static blinkAltChars\n"
    "  foreign static blinkAltChars=(b)\n"
    "  foreign static noBlink\n"
    "  foreign static noBlink=(b)\n"
    /* expand is read-only: the bitmap buffer is sized at video-mode
     * init for 8 vs 9-pixel-wide cells; flipping it under a live
     * screen would mismatch the layout.  lineGraphicsExpand only
     * affects how chars 0xC0..0xDF render on the next draw, so it's
     * safe to toggle. */
    "  foreign static expand\n"
    "  foreign static lineGraphicsExpand\n"
    "  foreign static lineGraphicsExpand=(b)\n"
    "  foreign altChars\n"
    "  foreign altChars=(b)\n"
    "  foreign noBright\n"
    "  foreign noBright=(b)\n"
    "  foreign bgBright\n"
    "  foreign bgBright=(b)\n"
    "  foreign blinkAltChars\n"
    "  foreign blinkAltChars=(b)\n"
    "  foreign noBlink\n"
    "  foreign noBlink=(b)\n"
    "  foreign expand\n"                                 /* read-only */
    "  foreign lineGraphicsExpand\n"
    "  foreign lineGraphicsExpand=(b)\n"
    "  foreign apply()\n"
    "}\n"
    /* Color utilities: build/inspect 24-bit RGB values and convert
     * between attr bytes and palette colors.  Returned values are
     * raw 0xRRGGBB without the high RGB-mode bit — Cell.fgRgb=
     * adds the bit when writing into a cell. */
    "class Color {\n"
    "  foreign static fromRgb(r, g, b)\n"           /* -> uint32 0xRRGGBB */
    "  foreign static fromAttr(attr)\n"             /* -> [fg, bg] uint32 */
    "  foreign static toLegacyAttr(fg, bg)\n"       /* -> uint8 */
    "}\n"
    /* Input — unified event reader.  next() blocks for the next event
     * (KeyEvent or MouseEvent); next(ms) waits up to ms ms or returns
     * null on timeout; poll returns null when nothing is pending.
     * unget(ev) accepts either type — a KeyEvent goes through
     * ciolib_ungetch (which handles 16-bit and LITERAL_E0 reversal);
     * a MouseEvent goes through ciolib_ungetmouse, and the next
     * next/poll will see the mouse-marker key naturally. */
    "foreign class Input {\n"
    "  foreign static next\n"
    "  foreign static next(ms)\n"
    "  foreign static poll\n"
    "  foreign static mousedrag()\n"
    /* Setter only — there's no ciolib query for mouse visibility,
     * just show/hide.  Tracking it from Wren land would drift if
     * other code shows or hides the cursor. */
    "  foreign static mouseVisible=(b)\n"
    /* nextEvent — yield the calling fiber until the next key or mouse
     * event arrives, then return it as a KeyEvent or MouseEvent.  Must
     * be called from inside a fiber created with `Fiber.new {}` (the
     * root fiber can't be resumed from C).  When parked, the event
     * bypasses every onKey/onMouse hook AND the default doterm()
     * processing, exactly as if a hook had returned true. */
    "  static nextEvent {\n"
    "    _park(Fiber.current)\n"
    "    return Fiber.yield()\n"
    "  }\n"
    "  foreign static _park(fiber)\n"
    /* unget(ev): dispatch on event type.  Wren doesn't expose the
     * foreign class of a value to the C side, so we discriminate here
     * and call one of two typed primitives.  Silently does nothing
     * for anything that isn't one of our two event types. */
    "  static unget(ev) {\n"
    "    if (ev is KeyEvent) {\n"
    "      Input.ungetKey_(ev)\n"
    "    } else if (ev is MouseEvent) {\n"
    "      Input.ungetMouse_(ev)\n"
    "    }\n"
    "  }\n"
    "  foreign static ungetKey_(ev)\n"
    "  foreign static ungetMouse_(ev)\n"
    "}\n"
    /* KeyEvent — `code` is the raw 16-bit ciolib value.  For non-
     * extended keys (high byte == 0) `codepoint` is the Unicode value
     * of the byte under the current Font.codepage and `text` is its
     * UTF-8 form; for extended keys both are null/"".  Esc-by-scancode
     * (CIO_KEY_ABORTED, 0x01E0) is normalized to plain Esc (0x001B)
     * inside the constructor so scripts only ever see one value. */
    "foreign class KeyEvent {\n"
    "  construct new(code) {}\n"
    "  foreign code\n"
    "  foreign codepoint\n"
    "  foreign text\n"
    "}\n"
    /* MouseEvent — fields mirror struct mouse_event in ciolib.h. */
    "foreign class MouseEvent {\n"
    "  construct new(event, modifiers, startX, startY, endX, endY) {}\n"
    "  foreign event\n"
    "  foreign modifiers\n"
    "  foreign startX\n"
    "  foreign startY\n"
    "  foreign endX\n"
    "  foreign endY\n"
    "}\n"
    /* Key — named constants for the CIO_KEY_* set in ciolib.h.  These
     * are the codes you'll see in KeyEvent.code for non-printable keys
     * and key combinations.  Codes are backend-tagged but consistent
     * across platforms after ciolib normalization. */
    "class Key {\n"
    "  static escape      { 0x001B }\n"
    "  static enter       { 0x000D }\n"
    "  static backspace   { 0x0008 }\n"
    "  static tab         { 0x0009 }\n"
    "  static delChar     { 0x007F }\n"
    "  static home        { 0x4700 }\n"
    "  static up          { 0x4800 }\n"
    "  static pageUp      { 0x4900 }\n"
    "  static left        { 0x4B00 }\n"
    "  static right       { 0x4D00 }\n"
    "  static end         { 0x4F00 }\n"
    "  static down        { 0x5000 }\n"
    "  static pageDown    { 0x5100 }\n"
    "  static insert      { 0x5200 }\n"
    "  static delete      { 0x5300 }\n"
    "  static backTab     { 0x0F00 }\n"   /* Shift-Tab */
    "  static shiftIns    { 0x0500 }\n"
    "  static shiftDel    { 0x0700 }\n"
    "  static ctrlIns     { 0x0400 }\n"
    "  static ctrlDel     { 0x0600 }\n"
    "  static altIns      { 0xA200 }\n"
    "  static altDel      { 0xA300 }\n"
    "  static shiftUp     { 0x3800 }\n"
    "  static ctrlUp      { 0x8D00 }\n"
    "  static shiftLeft   { 0x3400 }\n"
    "  static ctrlLeft    { 0x7300 }\n"
    "  static shiftRight  { 0x3600 }\n"
    "  static ctrlRight   { 0x7400 }\n"
    "  static shiftDown   { 0x3200 }\n"
    "  static ctrlDown    { 0x9100 }\n"
    "  static shiftEnd    { 0x3100 }\n"
    "  static ctrlEnd     { 0x7500 }\n"
    "  static f1          { 0x3B00 }\n"
    "  static f2          { 0x3C00 }\n"
    "  static f3          { 0x3D00 }\n"
    "  static f4          { 0x3E00 }\n"
    "  static f5          { 0x3F00 }\n"
    "  static f6          { 0x4000 }\n"
    "  static f7          { 0x4100 }\n"
    "  static f8          { 0x4200 }\n"
    "  static f9          { 0x4300 }\n"
    "  static f10         { 0x4400 }\n"
    "  static f11         { 0x8500 }\n"
    "  static f12         { 0x8600 }\n"
    "  static shiftF1     { 0x5400 }\n"
    "  static shiftF2     { 0x5500 }\n"
    "  static shiftF3     { 0x5600 }\n"
    "  static shiftF4     { 0x5700 }\n"
    "  static shiftF5     { 0x5800 }\n"
    "  static shiftF6     { 0x5900 }\n"
    "  static shiftF7     { 0x5A00 }\n"
    "  static shiftF8     { 0x5B00 }\n"
    "  static shiftF9     { 0x5C00 }\n"
    "  static shiftF10    { 0x5D00 }\n"
    "  static shiftF11    { 0x8700 }\n"
    "  static shiftF12    { 0x8800 }\n"
    "  static ctrlF1      { 0x5E00 }\n"
    "  static ctrlF2      { 0x5F00 }\n"
    "  static ctrlF3      { 0x6000 }\n"
    "  static ctrlF4      { 0x6100 }\n"
    "  static ctrlF5      { 0x6200 }\n"
    "  static ctrlF6      { 0x6300 }\n"
    "  static ctrlF7      { 0x6400 }\n"
    "  static ctrlF8      { 0x6500 }\n"
    "  static ctrlF9      { 0x6600 }\n"
    "  static ctrlF10     { 0x6700 }\n"
    "  static ctrlF11     { 0x8900 }\n"
    "  static ctrlF12     { 0x8A00 }\n"
    "  static altF1       { 0x6800 }\n"
    "  static altF2       { 0x6900 }\n"
    "  static altF3       { 0x6A00 }\n"
    "  static altF4       { 0x6B00 }\n"
    "  static altF5       { 0x6C00 }\n"
    "  static altF6       { 0x6D00 }\n"
    "  static altF7       { 0x6E00 }\n"
    "  static altF8       { 0x6F00 }\n"
    "  static altF9       { 0x7000 }\n"
    "  static altF10      { 0x7100 }\n"
    "  static altF11      { 0x8B00 }\n"
    "  static altF12      { 0x8C00 }\n"
    "  static mouse       { 0x7DE0 }\n"
    "  static quit        { 0x7EE0 }\n"
    "  static wrenConsole { 0x29E0 }\n"
    "}\n"
    "foreign class Clipboard {\n"
    "  foreign static text\n"               /* read system clipboard */
    "  foreign static text=(s)\n"           /* write system clipboard */
    "}\n"
    /* Codepage values match `enum ciolib_codepage` from utf8_codepages.h.
     * Each Font slot's codepage is exposed via Font.codepageOf(i); the
     * bare Font.codepage is shorthand for slot 0 (the default font). */
    "class Codepage {\n"
    "  static cp437        { 0 }\n"
    "  static cp1251       { 1 }\n"
    "  static cp1251_b     { 2 }\n"
    "  static koi8r        { 3 }\n"
    "  static iso8859_2    { 4 }\n"
    "  static iso8859_4    { 5 }\n"
    "  static cp866m       { 6 }\n"
    "  static iso8859_9    { 7 }\n"
    "  static iso8859_8    { 8 }\n"
    "  static koi8u        { 9 }\n"
    "  static iso8859_15   { 10 }\n"
    "  static iso8859_5    { 11 }\n"
    "  static cp850        { 12 }\n"
    "  static cp850_b      { 13 }\n"
    "  static cp865        { 14 }\n"
    "  static cp865_b      { 15 }\n"
    "  static iso8859_7    { 16 }\n"
    "  static iso8859_1    { 17 }\n"
    "  static cp866m2      { 18 }\n"
    "  static cp866u       { 19 }\n"
    "  static cp1131       { 20 }\n"
    "  static armscii8     { 21 }\n"
    "  static haik8        { 22 }\n"
    "  static atascii      { 23 }\n"
    "  static petsciiUpper { 24 }\n"
    "  static petsciiLower { 25 }\n"
    "  static prestel      { 26 }\n"
    "  static prestelSep   { 27 }\n"
    "  static atariSt      { 28 }\n"
    "}\n"
    /* REPL.compile_ returns the bare ObjClosure for the compiled body;
     * Wren-level REPL.eval invokes it via .call().  Going through Wren
     * for the call (rather than wrenInterpret-from-C) keeps apiStack
     * valid across the eval — wrenInterpret is not foreign-method-safe,
     * but Wren-level dispatch is. */
    /* REPL is the primitive: compile and call.  Return shape uses
     * an Optional-style wrapper so the caller can distinguish "this
     * was a statement" from "this was an expression whose value is
     * null":
     *   - statement form        -> null
     *   - expression form       -> [value]   (one-element list, value
     *                                         may itself be null)
     * Runtime errors propagate normally; wrap the call in
     * Fiber.new{}.try() at the call site if you want to catch them.
     * Display formatting belongs at the call site too (see
     * console.wren's handleLine_). */
    "class REPL {\n"
    "  static eval(src) { eval(\"syncterm\", src) }\n"
    "  static eval(module, src) {\n"
    /* Compile errors get diverted into a private capture buffer for
     * the duration of the eval.  If the expression-mode compile fails
     * with "Expected expression." (the parser's way of saying "this
     * isn't an expression — try as a statement"), we drop the capture
     * and try statement form.  Other compile failures, runtime
     * traces, etc. ride through the capture and get committed to the
     * real log at the end so the user sees them. */
    "    captureStart_()\n"
    "    var fn = compile_(module, src, true, true)\n"
    "    var wasExpr = fn != null\n"
    "    if (!wasExpr && captureContains_(\"Expected expression\")) {\n"
    "      captureClear_()\n"
    "      fn = compile_(module, src, false, true)\n"
    "    }\n"
    "    captureCommit_()\n"
    "    if (fn == null) return null\n"
    "    var v = fn.call()\n"
    "    return wasExpr ? [v] : null\n"
    "  }\n"
    "  foreign static compile_(module, src, isExpression, printErrors)\n"
    "  foreign static printTrace_(fiber)\n"
    "  foreign static hasModule(name)\n"
    "  foreign static captureStart_()\n"
    "  foreign static captureContains_(needle)\n"
    "  foreign static captureClear_()\n"
    "  foreign static captureCommit_()\n"
    "}\n"
    "foreign class Cell {\n"
    "  construct new() {}\n"
    "  foreign ch\n"
    "  foreign ch=(s)\n"
    "  foreign chByte\n"
    "  foreign chByte=(n)\n"
    "  foreign font\n"
    "  foreign font=(n)\n"
    "  foreign legacyAttr\n"
    "  foreign legacyAttr=(n)\n"
    "  foreign bright\n"
    "  foreign bright=(b)\n"
    "  foreign blink\n"
    "  foreign blink=(b)\n"
    "  foreign fgPalette\n"
    "  foreign fgPalette=(n)\n"
    "  foreign fgRgb\n"
    "  foreign fgRgb=(n)\n"
    "  foreign bgPalette\n"
    "  foreign bgPalette=(n)\n"
    "  foreign bgRgb\n"
    "  foreign bgRgb=(n)\n"
    "  foreign hyperlinkId\n"
    "  foreign hyperlinkId=(n)\n"
    "}\n"
    "foreign class Cells {\n"
    "  foreign count\n"
    "  foreign [i]\n"
    "  foreign iterate(it)\n"
    "  foreign iteratorValue(it)\n"
    "}\n"
    "class Font {\n"
    "  static cp437English      { 0 }\n"
    "  static commodore64Upper  { 32 }\n"
    "  static commodore64Lower  { 33 }\n"
    "  static commodore128Upper { 34 }\n"
    "  static commodore128Lower { 35 }\n"
    "  static atari             { 36 }\n"
    "  static potNoodle         { 37 }\n"
    "  static mosOul            { 38 }\n"
    "  static microKnightPlus   { 39 }\n"
    "  static topazPlus         { 40 }\n"
    "  static microKnight       { 41 }\n"
    "  static topaz             { 42 }\n"
    "  static prestel           { 43 }\n"
    "  static atariSt           { 44 }\n"
    "  static ripterm           { 45 }\n"
    "  foreign static name(i)\n"
    "  foreign static count\n"
    "  foreign static available(i)\n"
    /* Codepage of the font in slot i (0..4); shorthand for slot 0. */
    "  foreign static codepage\n"
    "  foreign static codepageOf(i)\n"
    "}\n"
    "class Hyperlinks {\n"
    "  foreign static [id]\n"
    "  foreign static containsKey(id)\n"
    "  foreign static add(uri, idParam)\n"
    "  foreign static params(id)\n"
    "}\n"
    "foreign class Console {\n"
    "  foreign static count\n"
    "  foreign static total\n"
    "  foreign static [seq]\n"
    "  foreign static clear()\n"
    "  foreign static markSeen()\n"
    "  foreign static iterate(it)\n"
    "  foreign static iteratorValue(it)\n"
    "}\n"
    "class LogSource {\n"
    "  static print { 0 }\n"
    "  static compileError { 1 }\n"
    "  static runtimeError { 2 }\n"
    "  static stackFrame { 3 }\n"
    "}\n"
    "foreign class Conn {\n"
    "  foreign static send(s)\n"
    "  foreign static sendRaw(s)\n"
    "  foreign static close()\n"
    "  foreign static connected\n"
    "  foreign static type\n"
    "  foreign static pending\n"
    "  foreign static queued\n"
    "  foreign static peek(n)\n"
    "  foreign static recv(n)\n"
    "}\n"
    "foreign class CTerm {\n"
    /* Cursor position (1-based, on the terminal). */
    "  foreign static x\n"
    "  foreign static y\n"
    /* Terminal origin (1-based, on the host screen). */
    "  foreign static originX\n"
    "  foreign static originY\n"
    /* Dimensions and DEC margins. */
    "  foreign static width\n"
    "  foreign static height\n"
    "  foreign static topMargin\n"
    "  foreign static bottomMargin\n"
    "  foreign static leftMargin\n"
    "  foreign static rightMargin\n"
    /* Color state. */
    "  foreign static attr\n"
    "  foreign static fgColor\n"
    "  foreign static bgColor\n"
    "  foreign static hasPaletteOverride\n"
    "  foreign static paletteOverride\n"
    /* Fonts. */
    "  foreign static fontSlot\n"
    "  foreign static altFonts\n"
    /* Scrollback geometry. */
    "  foreign static scrollbackLines\n"
    "  foreign static scrollbackWidth\n"
    "  foreign static scrollbackPos\n"
    "  foreign static scrollbackStart\n"
    /* Mode flags. */
    "  foreign static emulation\n"
    "  foreign static doorwayMode\n"
    "  foreign static music\n"
    "  foreign static started\n"
    "  foreign static skypix\n"
    "  foreign static logMode\n"
    "  foreign static logPaused\n"
    "  foreign static statusDisplay\n"
    /* Bitfield snapshots. */
    "  foreign static extAttr\n"
    "  foreign static lastColumnFlag\n"
    /* Actions. */
    "  foreign static write(s)\n"
    "}\n"
    "foreign class ExtAttr {\n"
    "  foreign autoWrap\n"
    "  foreign originMode\n"
    "  foreign sxScroll\n"
    "  foreign decLrmm\n"
    "  foreign bracketPaste\n"
    "  foreign decBkm\n"
    "  foreign prestelMosaic\n"
    "  foreign prestelDoubleHeight\n"
    "  foreign prestelConceal\n"
    "  foreign prestelSeparated\n"
    "  foreign prestelHold\n"
    "  foreign alternateKeypad\n"
    "}\n"
    "foreign class LastColumnFlag {\n"
    "  foreign set\n"
    "  foreign enabled\n"
    "  foreign forced\n"
    "}\n"
    "class LogMode {\n"
    "  static none  { 0 }\n"
    "  static ascii { 1 }\n"
    "  static raw   { 2 }\n"
    "}\n"
    "class StatusDisplay {\n"
    "  static none      { 0 }\n"
    "  static indicator { 1 }\n"
    "  static host      { 2 }\n"
    "}\n"
    "foreign class BBS {\n"
    "  foreign static name\n"
    "  foreign static addr\n"
    "  foreign static port\n"
    "  foreign static connType\n"
    "  foreign static user\n"
    "  foreign static password\n"
    "  foreign static syspass\n"
    "  foreign static music\n"
    "  foreign static rip\n"
    "  foreign static comment\n"
    "  foreign static type\n"
    "  foreign static id\n"
    "  foreign static addressFamily\n"
    "  foreign static termName\n"
    "  foreign static screenMode\n"
    "  foreign static bpsRate\n"
    "  foreign static font\n"
    "  foreign static noStatus\n"
    "  foreign static hidePopups\n"
    "  foreign static yellowIsYellow\n"
    "  foreign static forceLcf\n"
    "  foreign static added\n"
    "  foreign static connected\n"
    "  foreign static fastConnected\n"
    "  foreign static calls\n"
    "  foreign static dlDir\n"
    "  foreign static ulDir\n"
    "  foreign static logFile\n"
    "  foreign static appendLogFile\n"
    "  foreign static xferLogLevel\n"
    "  foreign static telnetLogLevel\n"
    "  foreign static stopBits\n"
    "  foreign static dataBits\n"
    "  foreign static parity\n"
    "  foreign static flowControl\n"
    "  foreign static telnetNoBinary\n"
    "  foreign static deferTelnetNegotiation\n"
    "  foreign static ghostProgram\n"
    "  foreign static sftpPublicKey\n"
    "  foreign static sshFingerprint\n"
    "  foreign static sshFingerprintLen\n"
    "  foreign static palette\n"
    "  foreign static paletteSize\n"
    "  foreign static sortOrder\n"
    "}\n"
    "class ConnType {\n"
    "  static unknown        { 0 }\n"
    "  static rlogin         { 1 }\n"
    "  static rloginReversed { 2 }\n"
    "  static telnet         { 3 }\n"
    "  static raw            { 4 }\n"
    "  static ssh            { 5 }\n"
    "  static sshNoAuth      { 6 }\n"
    "  static modem          { 7 }\n"
    "  static serial         { 8 }\n"
    "  static serialNoRts    { 9 }\n"
    "  static shell          { 10 }\n"
    "  static mbbsGhost      { 11 }\n"
    "  static telnets        { 12 }\n"
    "}\n"
    "class Emulation {\n"
    "  static ansiBbs   { 0 }\n"
    "  static petascii  { 1 }\n"
    "  static atascii   { 2 }\n"
    "  static prestel   { 3 }\n"
    "  static beeb      { 4 }\n"
    "  static atariVt52 { 5 }\n"
    "}\n"
    "class BBSListType {\n"
    "  static user   { 0 }\n"
    "  static system { 1 }\n"
    "}\n"
    "class ScreenMode {\n"
    "  static current        { 0 }\n"
    "  static c80x25         { 1 }\n"
    "  static lcd80x25       { 2 }\n"
    "  static c80x28         { 3 }\n"
    "  static c80x30         { 4 }\n"
    "  static c80x43         { 5 }\n"
    "  static c80x50         { 6 }\n"
    "  static c80x60         { 7 }\n"
    "  static c132x37        { 8 }\n"
    "  static c132x52        { 9 }\n"
    "  static c132x25        { 10 }\n"
    "  static c132x28        { 11 }\n"
    "  static c132x30        { 12 }\n"
    "  static c132x34        { 13 }\n"
    "  static c132x43        { 14 }\n"
    "  static c132x50        { 15 }\n"
    "  static c132x60        { 16 }\n"
    "  static c64            { 17 }\n"
    "  static c128_40        { 18 }\n"
    "  static c128_80        { 19 }\n"
    "  static atari          { 20 }\n"
    "  static atariXep80     { 21 }\n"
    "  static custom         { 22 }\n"
    "  static ega80x25       { 23 }\n"
    "  static vga80x25       { 24 }\n"
    "  static prestel        { 25 }\n"
    "  static beeb           { 26 }\n"
    "  static atariSt40x25   { 27 }\n"
    "  static atariSt80x25   { 28 }\n"
    "  static atariSt80x25Mono { 29 }\n"
    "}\n"
    "class AddressFamily {\n"
    "  static unspec { 0 }\n"
    "  static inet   { 1 }\n"
    "  static inet6  { 2 }\n"
    "}\n"
    "class MusicMode {\n"
    "  static syncterm { 0 }\n"
    "  static bansi    { 1 }\n"
    "  static enabled  { 2 }\n"
    "}\n"
    "class RipVersion {\n"
    "  static none { 0 }\n"
    "  static v1   { 1 }\n"
    "  static v3   { 2 }\n"
    "}\n"
    "class Parity {\n"
    "  static none { 0 }\n"
    "  static even { 1 }\n"
    "  static odd  { 2 }\n"
    "}\n"
    "foreign class FlowControl {\n"
    "  foreign rtsCts\n"
    "  foreign xonOff\n"
    "}\n"
    "class LogLevel {\n"
    "  static emergency { 0 }\n"
    "  static alert     { 1 }\n"
    "  static critical  { 2 }\n"
    "  static error     { 3 }\n"
    "  static warning   { 4 }\n"
    "  static notice    { 5 }\n"
    "  static info      { 6 }\n"
    "  static debug     { 7 }\n"
    "}\n"
    "foreign class Directory {\n"
    "  foreign contains(name)\n"
    "  foreign list()\n"
    "  foreign create(name)\n"
    "}\n"
    "foreign class File {\n"
    "  foreign open()\n"
    "  foreign close()\n"
    "  foreign readBytes(count)\n"
    "  foreign readBytes(count, offset)\n"
    "  foreign read()\n"
    "  foreign writeBytes(s)\n"
    "  foreign writeBytes(s, offset)\n"
    "  foreign write(s)\n"
    "  foreign delete()\n"
    "  foreign offset\n"
    "  foreign offset=(o)\n"
    "  foreign size\n"
    "  foreign isOpen\n"
    "}\n"
    "foreign class Hook {\n"
    "  foreign static onKey(fn)\n"
    "  foreign static onKey(key, fn)\n"
    "  foreign static onInput(fn)\n"
    "  foreign static onInput(byte, fn)\n"
    "  foreign static onOutput(fn)\n"
    "  foreign static onMouse(fn)\n"
    "  foreign static onMouse(event, fn)\n"
    "  foreign static onStatus(fn)\n"
    "  foreign static every(ms, fn)\n"
    "}\n";

/* --------------------------------------------------------------------
 * State
 * -------------------------------------------------------------------- */

static struct wren_host_state state;
static bool      active;
static pthread_t owner_thread;

/* --------------------------------------------------------------------
 * Log buffer (always-on print/error scrollback)
 *
 * Fixed-size ring; head = next write index, count = valid entries.
 * Oldest-relative reads come out as `(head - count + i) mod cap` so
 * scripts always see entry 0 as the oldest valid message regardless
 * of where the ring head currently sits.
 *
 * Each slot keeps either the C-malloc'd text bytes (until first read)
 * OR the cached Wren tuple handle (after first read), never both.
 * This keeps memory at one copy per slot worst-case.
 * -------------------------------------------------------------------- */

#define WREN_LOG_CAPACITY  1024
#define WREN_LOG_MAX_TEXT  (8 * 1024)

struct wren_log_entry {
	uint64_t             seq;           /* assigned at write time */
	long double          ts;
	enum wren_log_source source;
	char                *text;          /* freed at first read or eviction */
	size_t               len;
	WrenHandle          *cached_value;  /* NULL until first read */
};

struct wren_log {
	struct wren_log_entry entries[WREN_LOG_CAPACITY];
	int                   head;
	int                   count;
	uint64_t              total;        /* monotonic; survives clear */
};

/* The user-visible scrollback. */
static struct wren_log main_log;

/* Side log used during a REPL.eval to capture compile errors that may
 * or may not survive to the user-visible log.  When a captured set
 * contains "Expected expression." (the parser's signal that input
 * isn't an expression), REPL.eval drops the side log and retries as a
 * statement.  Otherwise the captured entries are merged into the main
 * log via wren_log_capture_commit. */
static struct wren_log capture_log;

/* `log_append` writes here.  Default target is the main log; the
 * capture API points it at capture_log between start and
 * commit/clear, redirecting writes without touching the main log at
 * all.  Wren is single-threaded in our use, so no synchronization. */
static struct wren_log *log_target = &main_log;

static void
log_release_slot(struct wren_log_entry *e)
{
	if (e->cached_value != NULL && state.vm != NULL) {
		wrenReleaseHandle(state.vm, e->cached_value);
		e->cached_value = NULL;
	}
	if (e->text != NULL) {
		free(e->text);
		e->text = NULL;
	}
	e->len = 0;
}

static void
log_drop_all(struct wren_log *log)
{
	for (int i = 0; i < WREN_LOG_CAPACITY; i++)
		log_release_slot(&log->entries[i]);
	log->head  = 0;
	log->count = 0;
	/* total intentionally NOT reset — Wren scripts may track a
	 * high-water mark; resetting would make existing marks falsely
	 * reappear as "new writes."  Buffer is empty; next write gets
	 * seq = log->total (no skip). */
}

static void
log_append(enum wren_log_source source, const char *text)
{
	if (text == NULL)
		text = "";
	size_t len = strlen(text);
	bool truncated = false;
	if (len > WREN_LOG_MAX_TEXT) {
		len = WREN_LOG_MAX_TEXT;
		truncated = true;
	}
	static const char trunc_suffix[] = "…[truncated]";
	size_t alloc = len + (truncated ? sizeof(trunc_suffix) - 1 : 0);

	char *buf = malloc(alloc + 1);
	if (buf == NULL)
		return;
	memcpy(buf, text, len);
	if (truncated)
		memcpy(buf + len, trunc_suffix, sizeof(trunc_suffix) - 1);
	buf[alloc] = '\0';

	struct wren_log       *log = log_target;
	struct wren_log_entry *e   = &log->entries[log->head];
	log_release_slot(e);
	e->seq    = log->total++;
	e->ts     = xp_timer();
	e->source = source;
	e->text   = buf;
	e->len    = alloc;

	log->head = (log->head + 1) % WREN_LOG_CAPACITY;
	if (log->count < WREN_LOG_CAPACITY)
		log->count++;
}

/* --------------------------------------------------------------------
 * Capture API: redirect log_append to a side log between start and
 * commit/clear.  REPL.eval uses this to preview compile errors from an
 * expression-mode attempt before deciding whether to surface them.
 * -------------------------------------------------------------------- */

void
wren_log_capture_start(void)
{
	log_drop_all(&capture_log);
	capture_log.total = 0;
	log_target = &capture_log;
}

bool
wren_log_capture_active(void)
{
	return log_target == &capture_log;
}

bool
wren_log_capture_contains(const char *needle)
{
	if (needle == NULL)
		return false;
	int n     = capture_log.count;
	int start = (capture_log.head - n + WREN_LOG_CAPACITY) % WREN_LOG_CAPACITY;
	for (int i = 0; i < n; i++) {
		struct wren_log_entry *e =
		    &capture_log.entries[(start + i) % WREN_LOG_CAPACITY];
		if (e->text != NULL && strstr(e->text, needle) != NULL)
			return true;
	}
	return false;
}

void
wren_log_capture_clear(void)
{
	log_drop_all(&capture_log);
	log_target = &main_log;
}

void
wren_log_capture_commit(void)
{
	/* Swap target back to main, then replay each captured entry
	 * through log_append so it lands in main with a fresh ring slot.
	 * Order is oldest-first; we walk from (head - count) forward. */
	log_target = &main_log;
	int n     = capture_log.count;
	int start = (capture_log.head - n + WREN_LOG_CAPACITY) % WREN_LOG_CAPACITY;
	for (int i = 0; i < n; i++) {
		struct wren_log_entry *e =
		    &capture_log.entries[(start + i) % WREN_LOG_CAPACITY];
		if (e->text != NULL)
			log_append(e->source, e->text);
	}
	log_drop_all(&capture_log);
}

int
wren_log_count(void)
{
	return main_log.count;
}

uint64_t
wren_log_total(void)
{
	return main_log.total;
}

/* High-water mark of the log total at the time the user last left the
 * Wren console.  wren_host_log_unread() is true while main_log.total
 * has advanced past this. */
static uint64_t s_log_seen_total;

bool
wren_host_log_unread(void)
{
	return main_log.total > s_log_seen_total;
}

void
wren_host_mark_log_seen(void)
{
	s_log_seen_total = main_log.total;
}

void
wren_log_clear(void)
{
	log_drop_all(&main_log);
}

void
wren_log_emit(WrenVM *vm, uint64_t seq, int slot)
{
	struct wren_log *log = &main_log;
	/* Valid range: oldest seq = log->total - log->count;
	 * newest seq = log->total - 1. */
	if (log->count == 0 ||
	    seq < log->total - (uint64_t)log->count ||
	    seq >= log->total) {
		wrenSetSlotNull(vm, slot);
		return;
	}
	/* Newest entry sits at (head - 1) mod cap.  Walking backward by
	 * (log->total - 1 - seq) gives the slot for `seq`. */
	int back = (int)(log->total - 1 - seq);
	int idx  = ((log->head - 1 - back) % WREN_LOG_CAPACITY +
	            WREN_LOG_CAPACITY) % WREN_LOG_CAPACITY;
	struct wren_log_entry *e = &log->entries[idx];

	if (e->cached_value != NULL) {
		wrenSetSlotHandle(vm, slot, e->cached_value);
		return;
	}

	/* First read: build [ts, source, text] in slot, capture handle,
	 * free the C buffer (text now lives once, in the Wren String the
	 * tuple references). */
	wrenEnsureSlots(vm, slot + 4);
	wrenSetSlotNewList(vm, slot);
	wrenSetSlotDouble(vm, slot + 1, (double)e->ts);
	wrenInsertInList(vm, slot, -1, slot + 1);
	wrenSetSlotDouble(vm, slot + 1, (double)(int)e->source);
	wrenInsertInList(vm, slot, -1, slot + 1);
	wrenSetSlotBytes(vm, slot + 1,
	    e->text != NULL ? e->text : "", e->len);
	wrenInsertInList(vm, slot, -1, slot + 1);

	e->cached_value = wrenGetSlotHandle(vm, slot);
	free(e->text);
	e->text = NULL;
	/* Re-emit through the cached handle so the slot caller sees the
	 * same value as subsequent reads will. */
	wrenSetSlotHandle(vm, slot, e->cached_value);
}

struct wren_host_state *
wren_host_state(void)
{
	return active ? &state : NULL;
}

bool
wren_host_active(void)
{
	if (!active)
		return false;
	/* Cheap fast-path: avoid a pthread_self() + compare on every call
	 * site by allowing a positive answer from any thread; only the
	 * dispatchers themselves enforce thread affinity. */
	return true;
}

bool
wren_host_input_held(void)
{
	return active && state.parked_fiber != NULL;
}

/* --------------------------------------------------------------------
 * Wren callbacks
 * -------------------------------------------------------------------- */

static void
host_write_fn(WrenVM *vm, const char *text)
{
	(void)vm;
	log_append(WREN_LOG_PRINT, text);
}

static void
host_error_fn(WrenVM *vm, WrenErrorType type, const char *module, int line,
    const char *message)
{
	(void)vm;
	char buf[1024];
	switch (type) {
		case WREN_ERROR_COMPILE:
			snprintf(buf, sizeof(buf), "%s:%d: %s",
			    module ? module : "?", line, message ? message : "");
			log_append(WREN_LOG_COMPILE_ERROR, buf);
			break;
		case WREN_ERROR_RUNTIME:
			log_append(WREN_LOG_RUNTIME_ERROR,
			    message ? message : "");
			break;
		case WREN_ERROR_STACK_TRACE:
			snprintf(buf, sizeof(buf), "%s:%d in %s",
			    module ? module : "?", line, message ? message : "");
			log_append(WREN_LOG_STACK_FRAME, buf);
			break;
	}
}

static WrenForeignMethodFn
host_bind_foreign_method(WrenVM *vm, const char *module, const char *className,
    bool isStatic, const char *signature)
{
	(void)vm;
	return wren_bind_lookup(module, className, isStatic, signature);
}

static WrenForeignClassMethods
host_bind_foreign_class(WrenVM *vm, const char *module, const char *className)
{
	(void)vm;
	return wren_bind_lookup_class(module, className);
}

/* Module loader: resolves `import "name"` against
 * <SYNCTERM_PATH_SCRIPTS>/load/<name>.wren.  Module names are
 * restricted to [A-Za-z0-9_]; an invalid name is reported as a
 * runtime error by synthesizing a one-line module body that calls
 * Fiber.abort, so the import fails with a clear message instead of
 * silently looking like "module not found." */
static void
host_load_module_complete(WrenVM *vm, const char *name,
    WrenLoadModuleResult result)
{
	(void)vm;
	(void)name;
	if (result.source != NULL)
		free((void *)result.source);
}

static bool
valid_module_name(const char *name)
{
	if (name == NULL || *name == '\0')
		return false;
	for (const char *p = name; *p != '\0'; p++) {
		unsigned char c = (unsigned char)*p;
		if (!((c >= 'A' && c <= 'Z') ||
		      (c >= 'a' && c <= 'z') ||
		      (c >= '0' && c <= '9') ||
		      c == '_'))
			return false;
	}
	return true;
}

static WrenLoadModuleResult
host_load_module(WrenVM *vm, const char *name)
{
	(void)vm;
	WrenLoadModuleResult res = { NULL, NULL, NULL };

	if (!valid_module_name(name)) {
		res.source =
		    "Fiber.abort(\"invalid Wren module name: only "
		    "[A-Za-z0-9_] allowed\")\n";
		return res;
	}

	char dir[MAX_PATH + 1];
	if (get_syncterm_filename(dir, sizeof(dir), SYNCTERM_PATH_SCRIPTS,
	    false) == NULL)
		return res;

	char path[MAX_PATH + 1];
	int n = snprintf(path, sizeof(path), "%sload/%s.wren", dir, name);
	if (n < 0 || (size_t)n >= sizeof(path))
		return res;

	FILE *f = fopen(path, "rb");
	if (f == NULL)
		return res;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz < 0 || sz > 4 * 1024 * 1024) {
		fclose(f);
		return res;
	}
	char *src = malloc((size_t)sz + 1);
	if (src == NULL) {
		fclose(f);
		return res;
	}
	size_t got = fread(src, 1, (size_t)sz, f);
	fclose(f);
	src[got] = '\0';

	res.source     = src;
	res.onComplete = host_load_module_complete;
	return res;
}

/* --------------------------------------------------------------------
 * Hook + timer registration
 * -------------------------------------------------------------------- */

static bool
register_hook_internal(WrenVM *vm, enum wren_hook_event ev, int fn_slot,
    bool filtered, int filter)
{
	if (!active || ev < 0 || ev >= WREN_HOOK_COUNT)
		return false;
	int n = state.hook_count[ev];
	if (n >= WREN_HOST_MAX_HOOKS_PER_EVENT) {
		fprintf(stderr, "[wren] hook limit reached for event %d\n", (int)ev);
		return false;
	}
	state.hooks[ev][n].fn       = wrenGetSlotHandle(vm, fn_slot);
	state.hooks[ev][n].filter   = filter;
	state.hooks[ev][n].filtered = filtered;
	state.hook_count[ev] = n + 1;
	return true;
}

bool
wren_host_register_hook(WrenVM *vm, enum wren_hook_event ev, int fn_slot)
{
	return register_hook_internal(vm, ev, fn_slot, false, 0);
}

bool
wren_host_register_hook_filtered(WrenVM *vm, enum wren_hook_event ev,
    int fn_slot, int filter)
{
	return register_hook_internal(vm, ev, fn_slot, true, filter);
}

bool
wren_host_register_timer(WrenVM *vm, int ms, int fn_slot)
{
	if (!active)
		return false;
	if (ms < 1)
		ms = 1;
	if (state.timer_count >= WREN_HOST_MAX_TIMERS) {
		fprintf(stderr, "[wren] timer limit reached\n");
		return false;
	}
	struct wren_timer_entry *t = &state.timers[state.timer_count++];
	t->callable    = wrenGetSlotHandle(vm, fn_slot);
	t->interval_ms = (uint32_t)ms;
	t->next_fire_s = xp_timer() + (long double)ms / 1000.0L;
	return true;
}

/* --------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------- */

/* Compute a module name from a script path: basename, sans ".wren". */
static void
modname_from_path(const char *path, char *out, size_t outsz)
{
	const char *base = strrchr(path, '/');
#ifdef _WIN32
	const char *base2 = strrchr(path, '\\');
	if (base2 != NULL && (base == NULL || base2 > base))
		base = base2;
#endif
	base = (base == NULL) ? path : base + 1;
	strncpy(out, base, outsz - 1);
	out[outsz - 1] = '\0';
	char *dot = strrchr(out, '.');
	if (dot != NULL)
		*dot = '\0';
}

/* Returns true if `name` matches any embedded script's name.  When a
 * user script's basename matches an embedded one, the user version is
 * an override and gets loaded into the shared "syncterm" module so it
 * sees the same scope (foreign-class declarations + sibling embeds)
 * that the embedded version would have. */
static bool
is_embedded_script_name(const char *name)
{
	for (const struct embedded_script *es = EMBEDDED_SCRIPTS;
	    es->name != NULL; es++) {
		if (strcmp(es->name, name) == 0)
			return true;
	}
	return false;
}

static void
load_one_script(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (f == NULL) {
		fprintf(stderr, "[wren] cannot open %s\n", path);
		return;
	}
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz < 0 || sz > 4 * 1024 * 1024) {
		fprintf(stderr, "[wren] %s: bad size %ld\n", path, sz);
		fclose(f);
		return;
	}
	char *src = malloc((size_t)sz + 1);
	if (src == NULL) {
		fclose(f);
		return;
	}
	size_t got = fread(src, 1, (size_t)sz, f);
	fclose(f);
	src[got] = '\0';

	char modname[256];
	modname_from_path(path, modname, sizeof(modname));

	/* Override of an embedded script -> load into "syncterm".  Pure
	 * user script -> load into its own module (filename-derived). */
	const char *target =
	    is_embedded_script_name(modname) ? "syncterm" : modname;

	WrenInterpretResult r = wrenInterpret(state.vm, target, src);
	free(src);
	if (r != WREN_RESULT_SUCCESS)
		fprintf(stderr, "[wren] %s: load failed\n", path);
}

void
wren_host_init(struct bbslist *bbs)
{
	char dir[MAX_PATH + 1];

	if (active)
		return;

	memset(&state, 0, sizeof(state));
	state.bbs = bbs;

	WrenConfiguration cfg;
	wrenInitConfiguration(&cfg);
	cfg.writeFn             = host_write_fn;
	cfg.errorFn             = host_error_fn;
	cfg.bindForeignMethodFn = host_bind_foreign_method;
	cfg.bindForeignClassFn  = host_bind_foreign_class;
	cfg.loadModuleFn        = host_load_module;
	state.vm = wrenNewVM(&cfg);
	if (state.vm == NULL)
		return;

	state.call0_handle = wrenMakeCallHandle(state.vm, "call()");
	state.call1_handle = wrenMakeCallHandle(state.vm, "call(_)");

	owner_thread = pthread_self();
	active = true;

	/* Register the foreign-class declarations so user scripts can
	 * `import "syncterm" for ...`. */
	if (wrenInterpret(state.vm, "syncterm", SYNCTERM_MODULE_SRC) !=
	    WREN_RESULT_SUCCESS) {
		fprintf(stderr, "[wren] failed to load builtin syncterm module\n");
		/* Dump captured compile/runtime errors to stderr so the user
		 * has something to act on — the in-memory log isn't reachable
		 * once we shut down. */
		int n     = main_log.count;
		int start = (main_log.head - n + WREN_LOG_CAPACITY)
		    % WREN_LOG_CAPACITY;
		for (int i = 0; i < n; i++) {
			struct wren_log_entry *e =
			    &main_log.entries[(start + i) % WREN_LOG_CAPACITY];
			if (e->text != NULL)
				fprintf(stderr, "[wren] %s\n", e->text);
		}
		wren_host_shutdown();
		return;
	}

	/* Capture handles for the foreign classes the C side allocates
	 * instances of (Cell from inside Screen.getText, Cells as the
	 * getText return value).  wrenSetSlotNewForeign needs a class
	 * handle in a slot, so we keep these alive for the VM's lifetime. */
	wrenEnsureSlots(state.vm, 1);
	wrenGetVariable(state.vm, "syncterm", "Cell", 0);
	state.cell_class = wrenGetSlotHandle(state.vm, 0);
	wrenGetVariable(state.vm, "syncterm", "Cells", 0);
	state.cells_class = wrenGetSlotHandle(state.vm, 0);
	wrenGetVariable(state.vm, "syncterm", "KeyEvent", 0);
	state.key_event_class = wrenGetSlotHandle(state.vm, 0);
	wrenGetVariable(state.vm, "syncterm", "MouseEvent", 0);
	state.mouse_event_class = wrenGetSlotHandle(state.vm, 0);

	/* Inject the module-level `Cache` Directory.  Done from C so no
	 * Wren-visible factory exists for constructing a cache-pointing
	 * Directory. */
	wren_bind_define_cache(state.vm);

	/* Two-phase load order:
	 *   1. Glob the user-script dir to learn which module names the
	 *      user has provided.
	 *   2. Walk EMBEDDED_SCRIPTS[], interpreting each whose name is
	 *      NOT in the user set.  User overrides any embedded with
	 *      the same module name.
	 *   3. Interpret the user scripts. */
	glob_t gl;
	memset(&gl, 0, sizeof(gl));
	bool have_user_dir = false;
	if (get_syncterm_filename(dir, sizeof(dir), SYNCTERM_PATH_SCRIPTS, false)
	    != NULL) {
		char pat[MAX_PATH + 16];
		snprintf(pat, sizeof(pat), "%s*.wren", dir);
		if (glob(pat, 0, NULL, &gl) == 0)
			have_user_dir = true;
	}

	for (const struct embedded_script *es = EMBEDDED_SCRIPTS;
	    es->name != NULL; es++) {
		bool overridden = false;
		if (have_user_dir) {
			for (size_t i = 0; i < gl.gl_pathc && !overridden; i++) {
				char umod[256];
				modname_from_path(gl.gl_pathv[i], umod, sizeof(umod));
				if (strcmp(umod, es->name) == 0)
					overridden = true;
			}
		}
		if (overridden)
			continue;
		/* Embedded scripts share the "syncterm" module with the
		 * foreign-class declarations.  Inside them, every binding
		 * (Screen, Input, Cell, REPL, etc.) and every sibling embed's
		 * top-level definitions are directly visible — no imports
		 * needed.  Override scripts are loaded into the same module
		 * by load_one_script. */
		if (wrenInterpret(state.vm, "syncterm", es->source) !=
		    WREN_RESULT_SUCCESS)
			fprintf(stderr, "[wren] embedded %s: load failed\n",
			    es->name);
	}

	if (have_user_dir) {
		for (size_t i = 0; i < gl.gl_pathc; i++)
			load_one_script(gl.gl_pathv[i]);
		globfree(&gl);
	}
}

void
wren_host_shutdown(void)
{
	if (!active)
		return;

	for (int e = 0; e < WREN_HOOK_COUNT; e++) {
		for (int i = 0; i < state.hook_count[e]; i++)
			if (state.hooks[e][i].fn != NULL)
				wrenReleaseHandle(state.vm, state.hooks[e][i].fn);
		state.hook_count[e] = 0;
	}
	for (int i = 0; i < state.timer_count; i++)
		if (state.timers[i].callable != NULL)
			wrenReleaseHandle(state.vm, state.timers[i].callable);
	state.timer_count = 0;

	if (state.call0_handle != NULL)
		wrenReleaseHandle(state.vm, state.call0_handle);
	if (state.call1_handle != NULL)
		wrenReleaseHandle(state.vm, state.call1_handle);
	if (state.cell_class != NULL)
		wrenReleaseHandle(state.vm, state.cell_class);
	if (state.cells_class != NULL)
		wrenReleaseHandle(state.vm, state.cells_class);
	if (state.key_event_class != NULL)
		wrenReleaseHandle(state.vm, state.key_event_class);
	if (state.mouse_event_class != NULL)
		wrenReleaseHandle(state.vm, state.mouse_event_class);
	if (state.parked_fiber != NULL) {
		wrenReleaseHandle(state.vm, state.parked_fiber);
		state.parked_fiber = NULL;
	}

	/* Release log-buffer cached handles before freeing the VM (they
	 * reference Wren-heap values). */
	wren_log_clear();

	wrenFreeVM(state.vm);
	state.vm = NULL;
	active = false;
}

/* --------------------------------------------------------------------
 * Dispatchers
 *
 * All input-shaped events (key/input/output/mouse) walk their hook
 * list in registration order; the first hook returning Bool true
 * short-circuits with consume==true.  Errors during a callback log
 * once and pass through.
 * -------------------------------------------------------------------- */

static bool
on_owner_thread(void)
{
	return pthread_self() == owner_thread;
}

static bool
read_consume_result(void)
{
	if (wrenGetSlotType(state.vm, 0) == WREN_TYPE_BOOL)
		return wrenGetSlotBool(state.vm, 0);
	return false;
}

bool
wren_host_dispatch_key(int key)
{
	if (!active || !on_owner_thread())
		return false;
	/* A fiber parked on Input.nextEvent claims the event before any
	 * hook sees it.  Only non-mouse keys go straight to the fiber;
	 * the CIO_KEY_MOUSE marker falls through so dispatch_mouse can
	 * deliver the actual MouseEvent next. */
	if (wren_bind_resume_parked_key(key))
		return true;
	int n = state.hook_count[WREN_HOOK_KEY];
	for (int i = 0; i < n; i++) {
		struct wren_hook_entry *h = &state.hooks[WREN_HOOK_KEY][i];
		if (h->filtered && h->filter != key)
			continue;
		wrenEnsureSlots(state.vm, 2);
		wrenSetSlotHandle(state.vm, 0, h->fn);
		wrenSetSlotDouble(state.vm, 1, (double)key);
		if (wrenCall(state.vm, state.call1_handle) !=
		    WREN_RESULT_SUCCESS)
			continue;
		if (read_consume_result())
			return true;
	}
	return false;
}

bool
wren_host_dispatch_input(unsigned char byte)
{
	if (!active || !on_owner_thread())
		return false;
	int n = state.hook_count[WREN_HOOK_INPUT];
	for (int i = 0; i < n; i++) {
		struct wren_hook_entry *h = &state.hooks[WREN_HOOK_INPUT][i];
		if (h->filtered && h->filter != (int)byte)
			continue;
		wrenEnsureSlots(state.vm, 2);
		wrenSetSlotHandle(state.vm, 0, h->fn);
		wrenSetSlotDouble(state.vm, 1, (double)byte);
		if (wrenCall(state.vm, state.call1_handle) !=
		    WREN_RESULT_SUCCESS)
			continue;
		if (read_consume_result())
			return true;
	}
	return false;
}

bool
wren_host_dispatch_output(const void *buf, size_t len)
{
	if (!active || !on_owner_thread() || buf == NULL)
		return false;
	int n = state.hook_count[WREN_HOOK_OUTPUT];
	for (int i = 0; i < n; i++) {
		wrenEnsureSlots(state.vm, 2);
		wrenSetSlotHandle(state.vm, 0, state.hooks[WREN_HOOK_OUTPUT][i].fn);
		wrenSetSlotBytes(state.vm, 1, (const char *)buf, len);
		if (wrenCall(state.vm, state.call1_handle) !=
		    WREN_RESULT_SUCCESS)
			continue;
		if (read_consume_result())
			return true;
	}
	return false;
}

bool
wren_host_dispatch_mouse(struct mouse_event *ev)
{
	if (!active || !on_owner_thread() || ev == NULL)
		return false;
	if (wren_bind_resume_parked_mouse(ev))
		return true;
	int n = state.hook_count[WREN_HOOK_MOUSE];
	if (n == 0)
		return false;
	/* Build a 7-element list: [event, bstate, kbmodifiers, startx,
	 * starty, endx, endy].  Scripts index by position. */
	for (int i = 0; i < n; i++) {
		struct wren_hook_entry *h = &state.hooks[WREN_HOOK_MOUSE][i];
		if (h->filtered && h->filter != ev->event)
			continue;
		wrenEnsureSlots(state.vm, 3);
		wrenSetSlotHandle(state.vm, 0, h->fn);
		wrenSetSlotNewList(state.vm, 1);
		const double fields[] = {
			(double)ev->event, (double)ev->bstate,
			(double)ev->kbmodifiers, (double)ev->startx,
			(double)ev->starty, (double)ev->endx,
			(double)ev->endy
		};
		for (size_t f = 0; f < sizeof(fields) / sizeof(fields[0]); f++) {
			wrenSetSlotDouble(state.vm, 2, fields[f]);
			wrenInsertInList(state.vm, 1, -1, 2);
		}
		if (wrenCall(state.vm, state.call1_handle) !=
		    WREN_RESULT_SUCCESS)
			continue;
		if (read_consume_result())
			return true;
	}
	return false;
}

bool
wren_host_compose_status(const char *def, char *out, size_t outsz)
{
	if (!active || !on_owner_thread() || out == NULL || outsz == 0)
		return false;
	int n = state.hook_count[WREN_HOOK_STATUS];
	for (int i = 0; i < n; i++) {
		wrenEnsureSlots(state.vm, 2);
		wrenSetSlotHandle(state.vm, 0, state.hooks[WREN_HOOK_STATUS][i].fn);
		wrenSetSlotString(state.vm, 1, def != NULL ? def : "");
		if (wrenCall(state.vm, state.call1_handle) !=
		    WREN_RESULT_SUCCESS)
			continue;
		if (wrenGetSlotType(state.vm, 0) != WREN_TYPE_STRING)
			continue;
		const char *s = wrenGetSlotString(state.vm, 0);
		if (s == NULL)
			continue;
		strlcpy(out, s, outsz);
		return true;
	}
	return false;
}

void
wren_host_dispatch_timer(void)
{
	if (!active || !on_owner_thread())
		return;
	long double now = xp_timer();
	for (int i = 0; i < state.timer_count; i++) {
		struct wren_timer_entry *t = &state.timers[i];
		if (t->callable == NULL || now < t->next_fire_s)
			continue;
		wrenEnsureSlots(state.vm, 1);
		wrenSetSlotHandle(state.vm, 0, t->callable);
		(void)wrenCall(state.vm, state.call0_handle);
		/* Advance by interval; if the loop stalled long enough that
		 * we're more than one interval behind, jump forward to "now"
		 * rather than firing repeatedly trying to catch up. */
		t->next_fire_s += (long double)t->interval_ms / 1000.0L;
		if (t->next_fire_s < now)
			t->next_fire_s = now + (long double)t->interval_ms / 1000.0L;
	}
}
