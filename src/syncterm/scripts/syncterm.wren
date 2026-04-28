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
  foreign static readRect(sx, sy, ex, ey)
  foreign static writeRect(sx, sy, ex, ey, cells)
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
  foreign static mouseVisible=(b)

  // Async event delivery — register `fiber` to receive the next key
  // or mouse event.  Fiber yields after, receives KeyEvent or
  // MouseEvent on resume.  Throws if another fiber is already
  // registered (single-subscriber).  Re-arm by calling again after
  // each event.  Common idiom:
  //   Input.nextEvent(Fiber.current)
  //   var ev = Fiber.yield()
  // Or as part of a multi-fire event loop:
  //   Input.nextEvent(Fiber.current)
  //   SFTP.stat(Fiber.current, "/foo")
  //   while (true) {
  //     var x = Fiber.yield()
  //     if (x is KeyEvent) { Input.nextEvent(Fiber.current); ... }
  //     if (x is SFTPStat) { ... }
  //   }
  foreign static nextEvent(fiber)

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
foreign class KeyEvent {
  construct new(code) {}
  foreign code
  foreign codepoint
  foreign text
}
foreign class MouseEvent {
  construct new(event, modifiers, startX, startY, endX, endY) {}
  foreign event
  foreign modifiers
  foreign startX
  foreign startY
  foreign endX
  foreign endY
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
}
foreign class Cells {
  foreign count
  foreign [i]
  foreign iterate(it)
  foreign iteratorValue(it)
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
  foreign isOpen
}
class Hook {
  foreign static onKey(fn)
  foreign static onKey(key, fn)
  foreign static onInput(fn)
  foreign static onInput(byte, fn)
  foreign static onMatch(pattern, fn)
  foreign static onMouse(fn)
  foreign static onMouse(event, fn)
  foreign static onStatus(fn)
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
}

// Module-private bridge for state the host owns but Wren shouldn't
// be able to construct directly.  `cacheDirectory` returns a fresh
// Directory whose path resolves lazily from the active BBS context;
// no Wren-visible constructor for an `is_cache` Directory exists, so
// this is the only path to one.
class Host {
  foreign static cacheDirectory
}

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
  foreign hash
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
}

// Cache singleton.  Bound at module-load time so any script that
// `import "syncterm" for Cache`s sees a fully-formed Directory.
var Cache = Host.cacheDirectory
