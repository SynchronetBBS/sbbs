class ScreenSupports {
  foreign static loadableFonts
  foreign static altBlinkFont
  foreign static altBoldFont
  foreign static brightBackground
  foreign static paletteChange
  foreign static pixels
  foreign static customCursor
  foreign static fontSelection
  foreign static windowTitle
  foreign static windowName
  foreign static windowIcon
  foreign static extendedPalette
  foreign static blockyScaling
  foreign static externalScaling
  foreign static closeLock
}
class ScreenFonts {
  foreign static [i]
  foreign static [i]=(n)
}
class ScreenWindow {
  foreign static bounds
  foreign static bounds=(box)
  foreign static position
  foreign static position=(coord)
  foreign static putChar(c)
  foreign static print(s)
  foreign static clear()
  foreign static clearToLineEnd()
  foreign static deleteLine()
  foreign static insertLine()
  foreign static scroll()
}
class Palette {
  foreign static [i]
  foreign static [i]=(c)
  foreign static mode
  foreign static mode=(list)
}
foreign class Screen {
  foreign static size
  foreign static save()
  foreign static restore(handle)
  foreign static readRect(sx, sy, ex, ey)         // returns a Surface
  foreign static writeRect(sx, sy, ex, ey, surface)
  // Blit a Surface to the screen.  The 4-arg form takes a sub-rect of
  // the source; it unpacks the Rect into the 7-arg foreign primitive
  // `putRect_`.
  foreign static putRect(src, dstX, dstY)
  foreign static putRect_(src, srcX, srcY, srcW, srcH, dstX, dstY)
  static putRect(src, srcRect, dstX, dstY) {
    putRect_(src, srcRect.x, srcRect.y, srcRect.w, srcRect.h, dstX, dstY)
  }
  foreign static moveRect(sx, sy, ex, ey, dx, dy)
  foreign static attr=(a)
  foreign static hyperlinkId
  foreign static hyperlinkId=(id)
  static window     { ScreenWindow   }
  static supports   { ScreenSupports }
  static font       { ScreenFonts    }
  static palette    { Palette        }
  static cursor     { CustomCursor   }
  static videoFlags { VideoFlags     }
  static color      { Color          }
}
foreign class CustomCursor {
  construct new() {}
  static current { new() }
  static normal  { presetLegacy_(2) }
  static solid   { presetLegacy_(1) }
  static none    { presetLegacy_(0) }
  foreign static presetLegacy_(legacyType)
  foreign static startLine
  foreign static startLine=(n)
  foreign static endLine
  foreign static endLine=(n)
  foreign static range
  foreign static range=(n)
  foreign static blink
  foreign static blink=(b)
  foreign static visible
  foreign static visible=(b)
  foreign startLine
  foreign startLine=(n)
  foreign endLine
  foreign endLine=(n)
  foreign range
  foreign range=(n)
  foreign blink
  foreign blink=(b)
  foreign visible
  foreign visible=(b)
  foreign apply()
}
foreign class VideoFlags {
  construct new() {}
  static current { new() }
  foreign static altChars
  foreign static altChars=(b)
  foreign static noBright
  foreign static noBright=(b)
  foreign static bgBright
  foreign static bgBright=(b)
  foreign static blinkAltChars
  foreign static blinkAltChars=(b)
  foreign static noBlink
  foreign static noBlink=(b)
  foreign static expand
  foreign static lineGraphicsExpand
  foreign static lineGraphicsExpand=(b)
  foreign altChars
  foreign altChars=(b)
  foreign noBright
  foreign noBright=(b)
  foreign bgBright
  foreign bgBright=(b)
  foreign blinkAltChars
  foreign blinkAltChars=(b)
  foreign noBlink
  foreign noBlink=(b)
  foreign expand
  foreign lineGraphicsExpand
  foreign lineGraphicsExpand=(b)
  foreign apply()
}
class Color {
  foreign static fromRgb(r, g, b)
  foreign static fromAttr(attr)
  foreign static toLegacyAttr(fg, bg)
}
foreign class Input {
  // Synchronous reads — block the entire VM via getch.  Useful when
  // a script has fully claimed the screen (e.g. the Wren console)
  // and wants the main loop completely idle.
  foreign static next()
  foreign static next(ms)
  foreign static poll()
  foreign static mousedrag()
  foreign static mousedrag(forceRect)
  foreign static mouseVisible=(b)

  // Bitmask of `Mouse.*` events that get delivered.  The terminal
  // mode sets a default (typically scroll-up only); a UI App should
  // save the current mask, add what it needs, and restore on exit so
  // the surrounding terminal's mouse-mode survives.  Use the
  // {enable,disable}MouseEvent helpers for single events.
  foreign static mouseEvents
  foreign static mouseEvents=(mask)
  foreign static enableMouseEvent(ev)
  foreign static disableMouseEvent(ev)

  // Async event delivery — push a claim onto the input claim stack.
  // Each claim is a `Fn` taking a single event arg (KeyEvent or
  // MouseEvent foreign) and returning a Bool (consumed).  The C
  // dispatcher walks claims top-down (newest fiber first); the
  // first claim to return `true` consumes the event.  Below all
  // claims, registered `Hook.onKey` / `Hook.onMouse` handlers fire.
  //
  // Per-fiber slot: each fiber has at most one claim on the stack.
  // A same-fiber re-push replaces the existing entry in place; a
  // different-fiber push adds a new entry on top.  Auto-prune at
  // dispatch time drops claims whose owning fiber is `isDone`.
  //
  // Returns a `ClaimHandle`.  Call `.pop()` on it to remove the
  // claim explicitly; the handle's foreign finalizer also pops on
  // GC if the script forgets.  pop() is idempotent.
  //
  // Common pattern (App.run):
  //   var claim = Input.pushClaim {|ev|
  //     // dispatch ev into widget tree, return consumed bool
  //   }
  //   ...                              // run loop, parks on Fiber.yield
  //   claim.pop()
  //
  // The two-arg `pushClaim_` is the foreign; the public one-arg
  // `pushClaim` captures `Fiber.current` automatically.
  foreign static pushClaim_(fiber, fn)
  static pushClaim(fn) { pushClaim_(Fiber.current, fn) }

  static unget(ev) {
    if (ev is KeyEvent) {
      Input.ungetKey_(ev)
    } else if (ev is MouseEvent) {
      Input.ungetMouse_(ev)
    }
  }
  foreign static ungetKey_(ev)
  foreign static ungetMouse_(ev)
}
// Returned from Input.pushClaim.  Drop the claim by calling `.pop()`;
// idempotent (stale handles after a same-fiber re-push are no-ops).
// The C-side finalizer also pops on GC, so a forgotten handle still
// cleans up — but explicit pop is cheaper and more predictable.
foreign class ClaimHandle {
  foreign pop()
}

// Fiber-resume primitive.  `Wake.post(fiber, value)` queues a
// synthetic resume of `fiber`, delivering `value` as the return of
// its next `Fiber.yield()`.  The wake is enqueued on the same
// result queue Timer.trigger and SFTP completions use, so it's
// delivered on the next main-loop drain — never in the middle of
// the current foreign call.  Safe to call from hooks
// (Hook.onInput / Hook.onMouse / Hook.onStatus / claim handlers /
// worker fibers): remote bytes or any other state change a hook
// observes can wake a UI fiber that's parked on `Fiber.yield()`.
//
// `value` may be any Wren object — the deliverer pins it as a
// WrenHandle until delivery, then loads it into the fiber's
// resumed value slot.  Convention: pass a discriminated value
// your fiber's main-loop demuxer can recognise (a foreign
// KeyEvent / MouseEvent class, or your own sentinel).
//
// Multiple posts can queue for the same fiber; each becomes one
// resumption in arrival order.  `post` does NOT wait for delivery
// — the caller's foreign returns immediately and the surrounding
// hook chain completes synchronously, satisfying the hook contract.
//
// Do not call from inside another foreign method that itself
// re-enters wrenCall (same rule as wrenCall in §7 of wren.md);
// hooks are the canonical safe site.
class Wake {
  foreign static post(fiber, value)
}

foreign class KeyEvent {
  construct new(code) {}
  foreign code
  foreign codepoint
  foreign text
  foreign toString
}
foreign class MouseEvent {
  construct new(event, modifiers, startX, startY, endX, endY) {}
  foreign event
  foreign modifiers
  foreign startX
  foreign startY
  foreign endX
  foreign endY
  foreign toString
}
// Numeric values from `enum ciolib_mouse_event` in conio/ciolib.h.
// MouseEvent.event matches one of these.  Each button's slot is
// press/release/click/dbl/trpl/quad/dragStart/dragMove/dragEnd in
// that order, offset by 9 per button.  Buttons 4/5 are the scroll
// wheel (4 = up, 5 = down) and arrive as PRESS or CLICK depending
// on backend; coords are the cursor's current position.
class Mouse {
  static move           { 0  }
  static button1Press   { 1  }
  static button1Release { 2  }
  static button1Click   { 3  }
  static button1DblClick{ 4  }
  static button1TrplClick{ 5 }
  static button1QuadClick{ 6 }
  static button1DragStart{ 7 }
  static button1DragMove { 8 }
  static button1DragEnd  { 9 }
  static wheelUpPress   { 28 }   // BUTTON_4_PRESS
  static wheelUpClick   { 30 }   // BUTTON_4_CLICK
  static wheelDownPress { 37 }   // BUTTON_5_PRESS
  static wheelDownClick { 39 }   // BUTTON_5_CLICK
}
class Key {
  static escape      { 0x001B }
  static enter       { 0x000D }
  static backspace   { 0x0008 }
  static tab         { 0x0009 }
  static delChar     { 0x007F }
  static home        { 0x4700 }
  static up          { 0x4800 }
  static pageUp      { 0x4900 }
  static left        { 0x4B00 }
  static right       { 0x4D00 }
  static end         { 0x4F00 }
  static down        { 0x5000 }
  static pageDown    { 0x5100 }
  static insert      { 0x5200 }
  static delete      { 0x5300 }
  static backTab     { 0x0F00 }
  static shiftIns    { 0x0500 }
  static shiftDel    { 0x0700 }
  static ctrlIns     { 0x0400 }
  static ctrlDel     { 0x0600 }
  static altIns      { 0xA200 }
  static altDel      { 0xA300 }
  static shiftUp     { 0x3800 }
  static ctrlUp      { 0x8D00 }
  static shiftLeft   { 0x3400 }
  static ctrlLeft    { 0x7300 }
  static shiftRight  { 0x3600 }
  static ctrlRight   { 0x7400 }
  static shiftDown   { 0x3200 }
  static ctrlDown    { 0x9100 }
  static shiftEnd    { 0x3100 }
  static ctrlEnd     { 0x7500 }
  static f1          { 0x3B00 }
  static f2          { 0x3C00 }
  static f3          { 0x3D00 }
  static f4          { 0x3E00 }
  static f5          { 0x3F00 }
  static f6          { 0x4000 }
  static f7          { 0x4100 }
  static f8          { 0x4200 }
  static f9          { 0x4300 }
  static f10         { 0x4400 }
  static f11         { 0x8500 }
  static f12         { 0x8600 }
  static shiftF1     { 0x5400 }
  static shiftF2     { 0x5500 }
  static shiftF3     { 0x5600 }
  static shiftF4     { 0x5700 }
  static shiftF5     { 0x5800 }
  static shiftF6     { 0x5900 }
  static shiftF7     { 0x5A00 }
  static shiftF8     { 0x5B00 }
  static shiftF9     { 0x5C00 }
  static shiftF10    { 0x5D00 }
  static shiftF11    { 0x8700 }
  static shiftF12    { 0x8800 }
  static ctrlF1      { 0x5E00 }
  static ctrlF2      { 0x5F00 }
  static ctrlF3      { 0x6000 }
  static ctrlF4      { 0x6100 }
  static ctrlF5      { 0x6200 }
  static ctrlF6      { 0x6300 }
  static ctrlF7      { 0x6400 }
  static ctrlF8      { 0x6500 }
  static ctrlF9      { 0x6600 }
  static ctrlF10     { 0x6700 }
  static ctrlF11     { 0x8900 }
  static ctrlF12     { 0x8A00 }
  static altF1       { 0x6800 }
  static altF2       { 0x6900 }
  static altF3       { 0x6A00 }
  static altF4       { 0x6B00 }
  static altF5       { 0x6C00 }
  static altF6       { 0x6D00 }
  static altF7       { 0x6E00 }
  static altF8       { 0x6F00 }
  static altF9       { 0x7000 }
  static altF10      { 0x7100 }
  static altF11      { 0x8B00 }
  static altF12      { 0x8C00 }
  // Alt-letter scancodes (CIO).  High byte = PC scancode of the
  // letter key, low byte = 0.  Hex values come from rows of the
  // IBM PC keyboard, not alphabetical order.
  static altA        { 0x1E00 }
  static altB        { 0x3000 }
  static altC        { 0x2E00 }
  static altD        { 0x2000 }
  static altE        { 0x1200 }
  static altF        { 0x2100 }
  static altG        { 0x2200 }
  static altH        { 0x2300 }
  static altI        { 0x1700 }
  static altJ        { 0x2400 }
  static altK        { 0x2500 }
  static altL        { 0x2600 }
  static altM        { 0x3200 }
  static altN        { 0x3100 }
  static altO        { 0x1800 }
  static altP        { 0x1900 }
  static altQ        { 0x1000 }
  static altR        { 0x1300 }
  static altS        { 0x1F00 }
  static altT        { 0x1400 }
  static altU        { 0x1600 }
  static altV        { 0x2F00 }
  static altW        { 0x1100 }
  static altX        { 0x2D00 }
  static altY        { 0x1500 }
  static altZ        { 0x2C00 }
  // Alt-digit scancodes.  Alt-1 .. Alt-9 are 0x7800 .. 0x8000;
  // Alt-0 is 0x8100.
  static alt1        { 0x7800 }
  static alt2        { 0x7900 }
  static alt3        { 0x7A00 }
  static alt4        { 0x7B00 }
  static alt5        { 0x7C00 }
  static alt6        { 0x7D00 }
  static alt7        { 0x7E00 }
  static alt8        { 0x7F00 }
  static alt9        { 0x8000 }
  static alt0        { 0x8100 }
  static mouse       { 0x7DE0 }
  static quit        { 0x7EE0 }
  static wrenConsole { 0x29E0 }
}
foreign class Clipboard {
  foreign static text
  foreign static text=(s)
}
class Codepage {
  static cp437        { 0 }
  static cp1251       { 1 }
  static cp1251_b     { 2 }
  static koi8r        { 3 }
  static iso8859_2    { 4 }
  static iso8859_4    { 5 }
  static cp866m       { 6 }
  static iso8859_9    { 7 }
  static iso8859_8    { 8 }
  static koi8u        { 9 }
  static iso8859_15   { 10 }
  static iso8859_5    { 11 }
  static cp850        { 12 }
  static cp850_b      { 13 }
  static cp865        { 14 }
  static cp865_b      { 15 }
  static iso8859_7    { 16 }
  static iso8859_1    { 17 }
  static cp866m2      { 18 }
  static cp866u       { 19 }
  static cp1131       { 20 }
  static armscii8     { 21 }
  static haik8        { 22 }
  static atascii      { 23 }
  static petsciiUpper { 24 }
  static petsciiLower { 25 }
  static prestel      { 26 }
  static prestelSep   { 27 }
  static atariSt      { 28 }

  // Returns true iff the first codepoint of `text` maps to a CP437
  // byte (so a Cell.ch= assignment would land on a real glyph rather
  // than the `?` fallback the C side substitutes for unmapped chars).
  // Used by Glyphs to pick its ASCII-safe variant when the rich
  // Unicode primary isn't representable.  Other codepages aren't yet
  // exposed; CP437 is the cell-storage codepage so it's the only one
  // that matters for UI painting.
  foreign static encodes_(text)
}
class REPL {
  static eval(src) { eval("syncterm", src) }
  static eval(module, src) {
    // Pre-classify by the leading non-whitespace token so we don't
    // fight with expression-mode wrapping when the source is clearly
    // a statement (var/class/import/etc.).  An expression-form
    // compile failure on something that LOOKS like an expression
    // therefore preserves the original compile error in the log
    // instead of being shadowed by a statement-form retry.
    captureStart_()
    var asStatement = isStatementForm_(src)
    var fn          = compile_(module, src, !asStatement, true)
    var wasExpr     = !asStatement && fn != null
    captureCommit_()
    if (fn == null) return null
    var v = fn.call()
    return wasExpr ? [v] : null
  }

  // True if the first non-whitespace token in `src` is a Wren
  // statement keyword (one that can't begin an expression).  Used by
  // eval to route the source to compile_ in the right mode without
  // a fallback dance — preserving expression-mode compile errors
  // for actual expression-form sources.
  static isStatementForm_(src) {
    var i = 0
    while (i < src.count) {
      var c = src[i]
      if (c != " " && c != "\t" && c != "\n" && c != "\r") break
      i = i + 1
    }
    var start = i
    while (i < src.count) {
      var c = src[i]
      var ok = c == "_" ||
               (c.bytes[0] >= 0x41 && c.bytes[0] <= 0x5A) ||
               (c.bytes[0] >= 0x61 && c.bytes[0] <= 0x7A) ||
               (c.bytes[0] >= 0x30 && c.bytes[0] <= 0x39)
      if (!ok) break
      i = i + 1
    }
    if (i == start) return false
    var first = src[start...i]
    return [
      "var", "class", "foreign", "import",
      "return", "break", "continue",
      "if", "while", "for"
    ].contains(first)
  }

  foreign static compile_(module, src, isExpression, printErrors)
  foreign static printTrace_(fiber)
  foreign static hasModule(name)
  foreign static modules
  foreign static captureStart_()
  foreign static captureContains_(needle)
  foreign static captureClear_()
  foreign static captureCommit_()
}
foreign class Cell {
  construct new() {}
  foreign ch
  foreign ch=(s)
  foreign chByte
  foreign chByte=(n)
  foreign font
  foreign font=(n)
  foreign legacyAttr
  foreign legacyAttr=(n)
  foreign bright
  foreign bright=(b)
  foreign blink
  foreign blink=(b)
  foreign fgPalette
  foreign fgPalette=(n)
  foreign fgRgb
  foreign fgRgb=(n)
  foreign bgPalette
  foreign bgPalette=(n)
  foreign bgRgb
  foreign bgRgb=(n)
  foreign hyperlinkId
  foreign hyperlinkId=(n)
  foreign toString
}
// Surface owns a w×h grid of vmem_cells.  Inherits Sequence so the
// linear `[i]` subscript, `count`, and the standard iteration helpers
// (each, where, map, all, any, reduce, join, take, skip, toList, …)
// all work off the iterate/iteratorValue protocol below — handy for
// "tell me where any 'X' lives" without having to switch to 2D
// indexing.
//
// Surface adds rectangular operations:
//   surface.cellAt(x, y)              — 2D-indexed Cell view (null if oob).
//   surface.putRect(src, dstX, dstY)  — paste full src.
//   surface.putRect(src, srcRect, dstX, dstY) — paste sub-rect of src.
//   surface.fill(rect, cell)          — bulk fill with a Cell template.
//
// `Screen.readRect` returns a Surface (sized to the requested rect);
// `Screen.putRect` blits a Surface to the screen as a single atomic
// puttext.
foreign class Surface is Sequence {
  construct new(width, height) {}
  foreign count
  foreign [i]
  foreign iterate(it)
  foreign iteratorValue(it)
  foreign width
  foreign height
  foreign cellAt(x, y)
  foreign putRect(src, dstX, dstY)
  foreign putRect_(src, srcX, srcY, srcW, srcH, dstX, dstY)
  foreign fill_(x, y, w, h, cell)
  foreign toString
  // Wren-side wrappers that unpack a Rect into the primitive form.
  putRect(src, srcRect, dstX, dstY) {
    putRect_(src, srcRect.x, srcRect.y, srcRect.w, srcRect.h, dstX, dstY)
  }
  fill(rect, cell) {
    fill_(rect.x, rect.y, rect.w, rect.h, cell)
  }
}
class Font {
  static cp437English      { 0 }
  static commodore64Upper  { 32 }
  static commodore64Lower  { 33 }
  static commodore128Upper { 34 }
  static commodore128Lower { 35 }
  static atari             { 36 }
  static potNoodle         { 37 }
  static mosOul            { 38 }
  static microKnightPlus   { 39 }
  static topazPlus         { 40 }
  static microKnight       { 41 }
  static topaz             { 42 }
  static prestel           { 43 }
  static atariSt           { 44 }
  static ripterm           { 45 }
  foreign static name(i)
  foreign static count
  foreign static available(i)
  foreign static codepage
  foreign static codepageOf(i)
}
class Hyperlinks {
  foreign static [id]
  foreign static containsKey(id)
  foreign static add(uri, idParam)
  foreign static params(id)
}
foreign class Console {
  foreign static count
  foreign static total
  foreign static [seq]
  foreign static clear()
  foreign static markSeen()
  foreign static iterate(it)
  foreign static iteratorValue(it)
}
class LogSource {
  static print { 0 }
  static compileError { 1 }
  static runtimeError { 2 }
  static stackFrame { 3 }
}
foreign class Conn {
  foreign static send(s)
  foreign static sendRaw(s)
  foreign static close()
  foreign static connected
  foreign static type
  foreign static pending
  foreign static queued
  foreign static peek(n)
  foreign static recv(n)
}
foreign class CTerm {
  foreign static x
  foreign static y
  foreign static originX
  foreign static originY
  foreign static width
  foreign static height
  foreign static topMargin
  foreign static bottomMargin
  foreign static leftMargin
  foreign static rightMargin
  foreign static attr
  foreign static fgColor
  foreign static bgColor
  foreign static hasPaletteOverride
  foreign static paletteOverride
  foreign static fontSlot
  foreign static altFonts
  foreign static scrollbackLines
  foreign static scrollbackWidth
  foreign static scrollbackPos
  foreign static scrollbackStart
  foreign static emulation
  foreign static doorwayMode
  foreign static music
  foreign static started
  foreign static skypix
  foreign static logMode
  foreign static logPaused
  foreign static statusDisplay
  foreign static refreshStatus()
  // Wren-controlled flag the SSH driver reads to keep the session
  // alive when the shell channel closes (or wire goes idle) while
  // SFTP transfers are in flight.  SftpQueue raises it on enqueue
  // and clears it when the queue drains; ssh.c OR's it with its own
  // worker counts in the conn_api.terminate decision.  Generic — any
  // SFTP-using script can hold the session up.
  foreign static sftpActive
  foreign static sftpActive=(b)
  foreign static extAttr
  foreign static lastColumnFlag
  foreign static write(s)
  foreign static suspended
  foreign static suspended=(b)
}
foreign class ExtAttr {
  foreign autoWrap
  foreign originMode
  foreign sxScroll
  foreign decLrmm
  foreign bracketPaste
  foreign decBkm
  foreign prestelMosaic
  foreign prestelDoubleHeight
  foreign prestelConceal
  foreign prestelSeparated
  foreign prestelHold
  foreign alternateKeypad
}
foreign class LastColumnFlag {
  foreign set
  foreign enabled
  foreign forced
}
class LogMode {
  static none  { 0 }
  static ascii { 1 }
  static raw   { 2 }
}
class StatusDisplay {
  static none      { 0 }
  static indicator { 1 }
  static host      { 2 }
}
foreign class BBS {
  foreign static name
  foreign static addr
  foreign static port
  foreign static connType
  foreign static user
  foreign static password
  foreign static syspass
  foreign static music
  foreign static rip
  foreign static comment
  foreign static type
  foreign static id
  foreign static addressFamily
  foreign static termName
  foreign static screenMode
  foreign static bpsRate
  foreign static font
  foreign static noStatus
  foreign static hidePopups
  foreign static yellowIsYellow
  foreign static forceLcf
  foreign static added
  foreign static connected
  foreign static fastConnected
  foreign static calls
  foreign static dlDir
  foreign static ulDir
  foreign static logFile
  foreign static appendLogFile
  foreign static xferLogLevel
  foreign static telnetLogLevel
  foreign static stopBits
  foreign static dataBits
  foreign static parity
  foreign static flowControl
  foreign static telnetNoBinary
  foreign static deferTelnetNegotiation
  foreign static ghostProgram
  foreign static sftpPublicKey
  foreign static sshFingerprint
  foreign static sshFingerprintLen
  foreign static palette
  foreign static paletteSize
  foreign static sortOrder
}
class ConnType {
  static unknown        { 0 }
  static rlogin         { 1 }
  static rloginReversed { 2 }
  static telnet         { 3 }
  static raw            { 4 }
  static ssh            { 5 }
  static sshNoAuth      { 6 }
  static modem          { 7 }
  static serial         { 8 }
  static serialNoRts    { 9 }
  static shell          { 10 }
  static mbbsGhost      { 11 }
  static telnets        { 12 }
}
class Emulation {
  static ansiBbs   { 0 }
  static petascii  { 1 }
  static atascii   { 2 }
  static prestel   { 3 }
  static beeb      { 4 }
  static atariVt52 { 5 }
}
class BBSListType {
  static user   { 0 }
  static system { 1 }
}
class ScreenMode {
  static current        { 0 }
  static c80x25         { 1 }
  static lcd80x25       { 2 }
  static c80x28         { 3 }
  static c80x30         { 4 }
  static c80x43         { 5 }
  static c80x50         { 6 }
  static c80x60         { 7 }
  static c132x37        { 8 }
  static c132x52        { 9 }
  static c132x25        { 10 }
  static c132x28        { 11 }
  static c132x30        { 12 }
  static c132x34        { 13 }
  static c132x43        { 14 }
  static c132x50        { 15 }
  static c132x60        { 16 }
  static c64            { 17 }
  static c128_40        { 18 }
  static c128_80        { 19 }
  static atari          { 20 }
  static atariXep80     { 21 }
  static custom         { 22 }
  static ega80x25       { 23 }
  static vga80x25       { 24 }
  static prestel        { 25 }
  static beeb           { 26 }
  static atariSt40x25   { 27 }
  static atariSt80x25   { 28 }
  static atariSt80x25Mono { 29 }
}
class AddressFamily {
  static unspec { 0 }
  static inet   { 1 }
  static inet6  { 2 }
}
class MusicMode {
  static syncterm { 0 }
  static bansi    { 1 }
  static enabled  { 2 }
}
class RipVersion {
  static none { 0 }
  static v1   { 1 }
  static v3   { 2 }
}
class Parity {
  static none { 0 }
  static even { 1 }
  static odd  { 2 }
}
foreign class FlowControl {
  foreign rtsCts
  foreign xonOff
}
class LogLevel {
  static emergency { 0 }
  static alert     { 1 }
  static critical  { 2 }
  static error     { 3 }
  static warning   { 4 }
  static notice    { 5 }
  static info      { 6 }
  static debug     { 7 }
}
foreign class Directory {
  foreign contains(name)
  foreign list
  foreign create(name)
  foreign createDir(name)
  foreign delete(name)
  foreign toString
}
foreign class File {
  foreign open()
  foreign close()
  foreign readBytes(count)
  foreign readBytes(count, offset)
  foreign read()
  foreign writeBytes(s)
  foreign writeBytes(s, offset)
  foreign write(s)
  foreign readLine()
  foreign writeLine(s)
  foreign offset
  foreign offset=(o)
  foreign size
  foreign mtime
  foreign mtime=(t)
  foreign isOpen
  // Opaque consent token for picker-sourced Files; null for Files
  // obtained via Cache / Upload / Download Directory listings.
  // Persist this in WON next to the path string and pass back to
  // Host.openLocalFile on resume to re-construct a File foreign at
  // the same absolute path.  See the picker-tokens section in
  // Wren.adoc for the threat model.
  foreign token
  // Hashes of the file's full content (mmap'd; zero-length files are
  // hashed as the empty buffer).  Returned as raw digest bytes —
  // Wren strings are byte-safe — to compare directly against
  // SFTPEntry.hash from the sha1s/md5s extensions.  Format hex
  // yourself if you need it for display.
  foreign sha1
  foreign md5
  foreign toString
}
class Hook {
  foreign static onKey(fn)
  foreign static onKey(key, fn)
  foreign static onInput(fn)
  foreign static onInput(byte, fn)
  foreign static onMatch(pattern, fn)

  // onMatchClean is the escape-aware variant: inbound bytes are
  // pre-filtered through SyncTERM's shared ANSI parser (the same
  // state machine ripper.c uses) before reaching the regex VM, so
  // colour codes, cursor moves, and DCS / OSC strings never split
  // a literal sequence.  A pattern like "Welcome" matches even
  // when the BBS sends "We" + ESC[1;33m + "lcome" inline.
  //
  // The callback's return value is IGNORED — onMatchClean is
  // passthrough-only.  Mapping a cleaned-byte match span back to
  // the raw byte stream (escapes interleaved through it) has no
  // unambiguous answer for "consume", so v1 doesn't try; the
  // matched text always reaches the terminal.  Use onMatch when
  // you need to drop bytes off the wire.
  foreign static onMatchClean(pattern, fn)

  foreign static onMouse(fn)
  foreign static onMouse(event, fn)
  foreign static onStatus(fn)
  // Lifecycle hooks: onShellClose fires when the SSH shell channel
  // closes but the session is still up (SFTP transfers may still be
  // in flight); onDisconnect fires after the main loop has exited,
  // during teardown but before the VM is torn down (no SFTP at this
  // point — sftpc_finish has already run).  Handlers take no args,
  // return value is ignored.
  foreign static onShellClose(fn)
  foreign static onDisconnect(fn)
  foreign static every(ms, fn)

  // Wraps every hook fire so a handler that yields directly raises
  // a clear error instead of corrupting the dispatch chain — yielded
  // fibers can't return the bool/string the dispatcher needs to
  // proceed.  Hooks that need to wait on async ops should fire them
  // against a Fiber.new {|r| ... } callback (so the calling fiber
  // returns normally) or wrap the work in `Fiber.new { ... }.call()`
  // (so the child fiber's yield returns to the hook, not the
  // dispatcher's wrenCall).
  //
  // C dispatchers swap their cached call(_) / call() handles for
  // these and pass `Hook` itself as the receiver; the return value
  // lands in slot 0 exactly as if the user's fn had been called
  // directly.  On yield or abort, the return is null — non-bool /
  // non-string slot types fall through the dispatchers' type
  // checks the same way a hook returning the wrong type already
  // does, so the input passes through untouched.
  static dispatch_(fn) {
    var r = null
    var f = Fiber.new { r = fn.call() }
    f.try()
    return finishDispatch_(f, r)
  }
  static dispatch_(fn, arg) {
    var r = null
    var f = Fiber.new { r = fn.call(arg) }
    f.try()
    return finishDispatch_(f, r)
  }
  static finishDispatch_(f, r) {
    if (!f.isDone) {
      System.print("hook handler must not yield directly; " +
                   "wrap parking work in Fiber.new { ... }.call()")
      return null
    }
    if (f.error != null) {
      System.print("hook error: " + f.error)
      REPL.printTrace_(f)
      return null
    }
    return r
  }
}

// Returned by every Hook.on*/Hook.every registration.  No Wren-side
// constructor — the only path to a HookHandle is via a successful
// registration, so scripts can't fabricate one to remove arbitrary
// hooks.  `remove()` tombstones the underlying entry (a second call
// is a no-op; metric getters read back zeros once removed).
foreign class HookHandle {
  foreign remove()
  foreign callCount       // total invocations
  foreign totalRuntime    // seconds (sum of every wrenCall in this hook)
  foreign minRuntime      // seconds (smallest single invocation)
  foreign maxRuntime      // seconds (largest single invocation)
  foreign toString
}

// Module-private bridge for state the host owns but Wren shouldn't
// be able to construct directly.  `cacheDirectory` returns a fresh
// Directory whose path resolves lazily from the active BBS context;
// no Wren-visible constructor for an `is_cache` Directory exists, so
// this is the only path to one.
//
// `downloadDir` / `uploadDir` return a fresh Directory rooted at the
// BBS's configured DownloadPath / UploadPath, with the relaxed
// filename predicate enabled (1..255 bytes, no path separators / NUL
// / control bytes / `.` / `..` / Windows reserved devices, but
// spaces / leading dots / parens etc. allowed).  Returns null when
// the corresponding path is empty OR equal to the user's $HOME —
// the "unfortunate default" guard for old configs.  The Cache
// strict policy is unchanged; only Directories rooted at
// `downloadDir` / `uploadDir` (and their subdirectories) take the
// relaxed path.
//
// `pickFile(initialDir, mask, opts)` is the user-consent escape
// hatch for "upload from anywhere": invokes uifc's filepick UI and
// returns a File foreign whose path is the absolute path the user
// picked, or null on cancel.  initialDir / mask may be null
// (defaults to BBS.ulDir / `*`).  opts is a bitmask of the C-side
// UIFC_FP_* flags (see uifc/filepick.h).  The returned File bypasses
// the relaxed-name policy — the picker UI is the sandbox boundary,
// since the user explicitly chose the path.
class Host {
  foreign static cacheDirectory
  foreign static downloadDir
  foreign static uploadDir
  // initialDir may be a String path, a Directory foreign (uses its
  // path; handles Cache lazy-resolve), or null (defaults to
  // BBS.uldir).  Returns a File on OK with a non-null .token
  // (when the picker-token signing key is loaded), or null on
  // cancel.  See pickFiles for the multi-select counterpart.
  foreign static pickFile(initialDir, mask, opts)
  // Multi-select counterpart of pickFile.  Returns a non-empty
  // List<File> on OK (each File has a .token), or null on cancel /
  // empty selection.  opts: same UIFC_FP_* bitmask, except
  // ALLOWENTRY / OVERPROMPT / CREATPROMPT cannot be combined with
  // multi-select (filepick rejects those flags).
  foreign static pickFiles(initialDir, mask, opts)
  // Re-open a File from an opaque token previously returned by
  // pickFile / pickFiles via the .token getter.  Returns null when
  // the token's HMAC doesn't verify (signing key rotated / corrupt
  // token), or when the file no longer exists or its content has
  // changed since the user consented (SHA-1 mismatch).  This is
  // the only path by which Wren can construct a File foreign for a
  // path outside the sandboxed Cache / Upload / Download roots.
  foreign static openLocalFile(token)
  // Status-bar transfer-indicator arrows.  Generic — any Wren
  // script that knows it's transferring something can light them
  // (the SFTP queue is just the first user; future Zmodem / HTTP
  // fetcher / ftp client / etc. can too).  Both default to false
  // and are reset on session teardown by the host.
  foreign static uploadArrow=(b)
  foreign static downloadArrow=(b)
  // First locally-configured SSH public key, as a Map
  // {"algo": String, "blob": String} (e.g. algo: "ssh-ed25519", blob:
  // base64-encoded), or null when none is available.  Used by
  // sftp_pubkey.wren to append the user's authorized_keys entry on
  // connect.  Returns the same key on every call within a session.
  foreign static sshPublicKey
}

// Platform identification.  `name` returns the uname(2) sysname on
// POSIX (e.g. "FreeBSD", "Linux", "Darwin"), "Windows" on Windows,
// and "Unknown" elsewhere.  Useful for branching on host OS in
// scripts; no further OS surface (no shell exec, no stdio, no
// process model) is exposed.
class Platform {
  foreign static name
}

// One-shot fiber resumption after a delay.  Register a fiber to
// receive a TimerElapsed event after `ms` milliseconds:
//
//   Timer.trigger(Fiber.current, 250)
//   var x = Fiber.yield()  // x is TimerElapsed
//
// Multiple pending timers per fiber are fine — each fires
// independently and yields one TimerElapsed.  For recurring callbacks
// that don't park a fiber, use Hook.every(ms, fn) instead.
class Timer {
  foreign static trigger(fiber, ms)
}

// Marker class returned via the result queue when a Timer.trigger
// delay elapses.  Carries no fields — the caller already knows what
// it scheduled; just dispatch on type.
foreign class TimerElapsed {}

// SFTP — SSH-channel side-band file transfer.  All methods are static
// on the SFTP class itself.
//
// Every async op takes the fiber-to-resume as its first argument.
// The op returns null when the request was queued (the caller will
// receive the result via that fiber later) or an SFTPError directly
// when the request couldn't be queued (session is gone, OOM).
//
// Common idiom — fire and immediately await:
//   var r = SFTP.realpath(Fiber.current, ".") || Fiber.yield()
//   // r is String or SFTPError
//
// Multi-fire / event-loop dispatch — fire several heterogeneous ops
// against the same fiber, then yield in a loop and demux by type:
//   SFTP.stat(Fiber.current, "/a")
//   SFTP.stat(Fiber.current, "/b")
//   Timer.trigger(Fiber.current, 100)
//   while (true) {
//     var x = Fiber.yield()
//     if (x is SFTPStat) { ... }
//     if (x is SFTPError) { break }
//     if (x is TimerElapsed) { ... }
//   }
//
// Hook-friendly callback pattern — pass a Fiber.new whose body runs
// when the result arrives; the calling fiber doesn't yield at all:
//   SFTP.realpath(Fiber.new {|r| ... }, ".")
//
// readdir returns null at EOF; call SFTP.close yourself when done
// with a handle.
class SFTP {
  foreign static available
  foreign static pubdir
  foreign static lname

  foreign static realpath(fiber, path)
  foreign static stat(fiber, path)
  foreign static opendir(fiber, path)
  foreign static readdir(fiber, handle)
  foreign static close(fiber, handle)
  foreign static open(fiber, path, flags)
  foreign static read(fiber, handle, offset, count)
  foreign static write(fiber, handle, offset, bytes)
  foreign static mkdir(fiber, path)
  foreign static rmdir(fiber, path)
  foreign static remove(fiber, path)
  foreign static rename(fiber, oldpath, newpath)
  foreign static setMtime(fiber, path, mtime)
  foreign static descs(fiber, path)
}

// Open-flag bitmask for SFTP.open.  Values match the SSH_FXF_* wire
// constants and may be OR'd together, e.g.
//   SFTP.open(path, FileFlag.write | FileFlag.creat | FileFlag.trunc)
class FileFlag {
  static read   { 0x00000001 }
  static write  { 0x00000002 }
  static append { 0x00000004 }
  static creat  { 0x00000008 }
  static trunc  { 0x00000010 }
  static excl   { 0x00000020 }
}

// One entry in a SFTP.readdir result.  `longname` is null when the
// server didn't negotiate the lname@syncterm.net extension; `hash` is
// null unless sha1s/md5s was advertised.
foreign class SFTPEntry {
  foreign name
  foreign longname
  foreign size
  foreign mtime
  foreign isDir
  foreign hasLongDesc
  foreign hash
  foreign toString
}

// Result of SFTP.stat.  Time fields are POSIX seconds; `mode` is the
// SFTP-wire permissions bits (Unix-style when the server is POSIX).
foreign class SFTPStat {
  foreign size
  foreign mtime
  foreign atime
  foreign mode
  foreign uid
  foreign gid
  foreign toString
}

// Opaque handle returned by SFTP.open / SFTP.opendir; consumed by
// SFTP.read / SFTP.write / SFTP.close (and SFTP.readdir for opendir
// handles).  No fields — the server-side bytes are not script-visible.
foreign class SFTPHandle {}

// Error result, surfaced in place of the typed result foreign when an
// SFTP op fails.  Two error layers — distinguish via `code`:
//   code != 0   — library/transport-level failure (sftp_err_code_t).
//                 serverStatus is meaningless.
//   code == 0   — server returned a STATUS reply with an error code;
//                 read serverStatus for the SSH_FX_* value (e.g.
//                 SSH_FX_NO_SUCH_FILE, SSH_FX_PERMISSION_DENIED).
// `message` is human-readable diagnostic text accumulated by the
// library (may be null).  `isTransient` is true for failures that
// may succeed on retry (transport drops, aborts, OOM).
foreign class SFTPError {
  foreign code
  foreign serverStatus
  foreign message
  foreign isTransient
  foreign toString
}

// Cache singleton.  Bound at module-load time so any script that
// `import "syncterm" for Cache`s sees a fully-formed Directory.
var Cache = Host.cacheDirectory
// Download / Upload roots — null when the BBS's DownloadPath /
// UploadPath is empty or set to $HOME (see Host.downloadDir
// docstring).  Bound at module-load time, like Cache; the Wren VM
// is per-session, so a reconnect to a different BBS reloads the
// module and re-binds these.  Scripts MUST null-check before use.
var Download = Host.downloadDir
var Upload   = Host.uploadDir

// WON — Wren Object Notation serialization.
//
// Serializes / deserializes values built from Null, Bool, Num, String, List,
// Map, Range, and Sequence (the latter two coerce to List).  Output is valid
// Wren-literal syntax.  Cycles abort the fiber.
//
// Two serialize variants:
//   serialize(v)         — strict; aborts on unsupported types or NaN/Inf.
//   serializeLossy(v)    — silently omits unsupported items from Lists/Maps;
//                          a top-level unsupported value becomes "null".
// Each takes an optional indent String for pretty-printing:
//   WON.serialize(v)            // compact: {"a":1,"b":[1,2,3]}
//   WON.serialize(v, "  ")      // pretty, 2-space indent
//   WON.serialize(v, "\t")      // pretty, tab indent
//
// deserialize runs a hardened C parser — no eval — and accepts arbitrary
// whitespace and trailing commas between tokens.  Map keys must be a value
// Wren can hash (Bool, Num, String, Range, null); List/Map keys are rejected.
class WON {
  static serialize(value)              { serialize_(value, null,   true)  }
  static serialize(value, indent)      { serialize_(value, indent, true)  }
  static serializeLossy(value)         { serialize_(value, null,   false) }
  static serializeLossy(value, indent) { serialize_(value, indent, false) }

  foreign static deserialize(text)

  // ---- internal ----

  static serialize_(value, indent, strict) {
    if (indent != null && !(indent is String)) {
      Fiber.abort("WON.serialize: indent must be a String or null")
    }
    if (!strict && !isWritable_(value)) return "null"
    return write_(value, indent, 0, [], strict)
  }

  static write_(value, indent, depth, stack, strict) {
    if (value == null)     return "null"
    if (value is Bool)     return value ? "true" : "false"
    if (value is Num)      return writeNum_(value, strict)
    if (value is String)   return writeString_(value)
    if (value is List)     return writeList_(value,        indent, depth, stack, strict)
    if (value is Map)      return writeMap_( value,        indent, depth, stack, strict)
    if (value is Range)    return writeList_(value.toList, indent, depth, stack, strict)
    if (value is Sequence) return writeList_(value.toList, indent, depth, stack, strict)
    if (strict) Fiber.abort("WON.serialize: unsupported type %(value.type)")
    return "null"
  }

  static writeNum_(n, strict) {
    if (n != n) {
      if (strict) Fiber.abort("WON.serialize: NaN has no Wren literal")
      return "null"
    }
    if (n == 1/0 || n == -1/0) {
      if (strict) Fiber.abort("WON.serialize: Infinity has no Wren literal")
      return "null"
    }
    return n.toString
  }

  // Escape every byte that could break a Wren string literal.  Notably
  // includes `%` (would otherwise start an interpolation) and all control
  // chars; UTF-8 high bytes pass through verbatim.
  static writeString_(s) {
    var parts = ["\""]
    for (b in s.bytes) parts.add(escapeByte_(b))
    parts.add("\"")
    return parts.join("")
  }

  static escapeByte_(b) {
    if (b == 0x22) return "\\\""           // "
    if (b == 0x5C) return "\\\\"           // \
    if (b == 0x25) return "\\\%"           // % (must escape both: \\ for the
                                            // backslash, \% for the %, since
                                            // an unescaped % starts interp)
    if (b == 0x0A) return "\\n"
    if (b == 0x0D) return "\\r"
    if (b == 0x09) return "\\t"
    if (b == 0x08) return "\\b"
    if (b == 0x0C) return "\\f"
    if (b == 0x0B) return "\\v"
    if (b == 0x07) return "\\a"
    if (b == 0x1B) return "\\e"
    if (b == 0x00) return "\\0"
    if (b < 0x20 || b == 0x7F) return "\\x" + hex2_(b)
    return String.fromByte(b)
  }

  static hex2_(b) { hexNibble_((b/16).floor) + hexNibble_(b % 16) }
  static hexNibble_(n) {
    // 0x30='0', 0x37+10=0x41='A', 0x37+15=0x46='F'
    return n < 10 ? String.fromByte(0x30 + n) : String.fromByte(0x37 + n)
  }

  static writeList_(list, indent, depth, stack, strict) {
    if (stack.contains(list)) Fiber.abort("WON.serialize: cycle in List")
    if (list.count == 0) return "[]"
    stack.add(list)
    var parts = []
    for (item in list) {
      if (!strict && !isWritable_(item)) continue
      parts.add(write_(item, indent, depth + 1, stack, strict))
    }
    stack.removeAt(-1)
    return container_("[", "]", parts, indent, depth)
  }

  static writeMap_(map, indent, depth, stack, strict) {
    if (stack.contains(map)) Fiber.abort("WON.serialize: cycle in Map")
    if (map.count == 0) return "{}"
    stack.add(map)
    var parts = []
    var colon = indent == null ? ":" : ": "
    for (key in map.keys) {
      var val = map[key]
      if (!strict && (!isWritable_(key) || !isWritable_(val))) continue
      var ks = write_(key, indent, depth + 1, stack, strict)
      var vs = write_(val, indent, depth + 1, stack, strict)
      parts.add(ks + colon + vs)
    }
    stack.removeAt(-1)
    return container_("{", "}", parts, indent, depth)
  }

  static container_(open, close, parts, indent, depth) {
    if (parts.count == 0) return open + close
    if (indent == null)   return open + parts.join(",") + close
    var inner = "\n" + repeat_(indent, depth + 1)
    var outer = "\n" + repeat_(indent, depth)
    return open + inner + parts.join("," + inner) + outer + close
  }

  static repeat_(s, n) {
    var r = ""
    var i = 0
    while (i < n) {
      r = r + s
      i = i + 1
    }
    return r
  }

  // True if write_ would handle [value] without aborting.  Used by lossy
  // mode to decide whether to skip an entry.
  static isWritable_(value) {
    if (value == null)     return true
    if (value is Bool)     return true
    if (value is Num)      return value == value && value != 1/0 && value != -1/0
    if (value is String)   return true
    if (value is List)     return true
    if (value is Map)      return true
    if (value is Range)    return true
    if (value is Sequence) return true
    return false
  }
}
