// Self-test suite for SyncTERM's Wren bindings.  Triggered by Alt+T
// (registered in scripts/runtests.wren).  Assumes the remote is
// bash in a PTY for the sentinel-driven async tests.
//
// Inline tests cover everything that doesn't need to round-trip
// through the connection: constants, foreign-class roundtrips,
// REPL, etc.  The async phase issues `printf` commands with
// unique sentinels and matches them via Hook.onMatch.  Counters
// for the time-based hooks (every, onOutput, filtered onKey via
// Input.unget) are ticked by hooks registered at module load and
// inspected in the final report.
//
// Re-running via Alt+T resets state in WrenTest.run() — the
// dispatcher and the test-counter hooks are registered once when
// this module is first imported, so repeated runs don't leak hooks.
//
// Out of scope (documented):
//   - Hook.onMouse: no clean way to inject a mouse event without
//     fighting the live session.
//   - Hook.onStatus: would replace the status bar for the test's
//     duration and would never deregister.

import "syncterm" for Hook, Conn, Console, Screen, CTerm, BBS, Key,
    Codepage, ConnType, Emulation, Font, LogSource, LogLevel,
    Parity, BBSListType, ScreenMode, AddressFamily, MusicMode,
    RipVersion, LogMode, StatusDisplay, Color, Cell, Hyperlinks,
    REPL, Input, KeyEvent

class WrenTest {
  static run() {
    __step    = 0
    __pass    = 0
    __fail    = 0
    __pending = null
    // Register the test's hooks fresh each run; cleanup_() removes
    // them at end-of-suite so they don't keep firing once the
    // tests are done.  The match dispatcher and watchdog are
    // active for the whole sentinel chain; __everyHook and
    // __keyHook exist purely to let report_() check
    // HookHandle.callCount > 0 for "fires from main loop" and
    // "Input.unget'd key was processed".
    __hooks = []
    __hooks.add(Hook.onMatch("__T(.+)_X_(.+)_X__") { |m|
      handle_(m[1], m[2])
      return true
    })
    __hooks.add(Hook.every(3000) { timeout_() })
    __everyHook = Hook.every(50) { 0 }
    __keyHook   = Hook.onKey(0xFE00) { |k| true }
    __hooks.add(__everyHook)
    __hooks.add(__keyHook)
    System.print("=== Wren self-test starting ===")

    // ------ constant enums ------------------------------------------
    testKeyConstants_()
    testCodepageConstants_()
    testConnTypeConstants_()
    testEmulationConstants_()
    testFontConstants_()
    testLogSourceConstants_()
    testParityConstants_()
    testLogLevelConstants_()
    testBBSListTypeConstants_()
    testScreenModeConstants_()
    testAddressFamilyConstants_()
    testMusicModeConstants_()
    testRipVersionConstants_()
    testLogModeConstants_()
    testStatusDisplayConstants_()

    // ------ pure C math / util --------------------------------------
    testColor_()

    // ------ read-only live bindings ---------------------------------
    testConnConnected_()
    testConnType_()
    testConnPending_()
    testConnQueued_()
    testScreenSize_()
    testScreenSupports_()
    testScreenFont_()
    testScreenWindow_()
    testCTermCursor_()
    testCTermDimensions_()
    testCTermFlags_()
    testBBS_()

    // ------ Console + REPL ------------------------------------------
    testConsoleTotal_()
    testREPLEvalExpression_()
    testREPLEvalStatement_()
    testREPLHasModule_()

    // ------ Cell / Cells / Screen rect roundtrip --------------------
    testCellRoundtrip_()
    testScreenRectRoundtrip_()

    // ------ Hyperlinks ---------------------------------------------
    testHyperlinks_()

    // ------ Hook.onMatch validation --------------------------------
    testAnchorReject_()
    testAnchorRejectPlus_()
    testAnchorRejectQuest_()

    // ------ inject a sentinel key for async onKey filter test ------
    // Filter key must have low byte 0x00 (or 0xe0) — ciolib's
    // ungetch / getch byte-stuff a 16-bit value as low|high and only
    // re-compose at rip_getch() when the first byte read is 0 or
    // 0xe0.  0xFE00 fits that constraint and isn't a real CIO_KEY.
    Input.unget(KeyEvent.new(0xFE00))

    // ------ kick off the sentinel-driven async chain ----------------
    advance_()
  }

  static check_(ok, label) {
    if (ok) {
      __pass = __pass + 1
      System.print("  PASS %(label)")
    } else {
      __fail = __fail + 1
      System.print("  FAIL %(label)")
    }
  }

  // ===== constant enums ===========================================

  static testKeyConstants_() {
    check_(Key.escape == 0x001B && Key.enter == 0x000D &&
           Key.up != Key.down && Key.f1 != Key.f12 &&
           Key.wrenConsole == 0x29E0,
           "Key constants: well-known values + uniqueness")
  }

  static testCodepageConstants_() {
    check_(Codepage.cp437 == 0 && Codepage.cp1251 == 1 &&
           Codepage.atascii == 23 && Codepage.atariSt == 28,
           "Codepage constants")
  }

  static testConnTypeConstants_() {
    check_(ConnType.unknown == 0 && ConnType.telnet == 3 &&
           ConnType.ssh == 5 && ConnType.telnets == 12,
           "ConnType constants")
  }

  static testEmulationConstants_() {
    check_(Emulation.ansiBbs == 0 && Emulation.atascii == 2 &&
           Emulation.atariVt52 == 5,
           "Emulation constants")
  }

  static testFontConstants_() {
    check_(Font.cp437English == 0 &&
           Font.commodore64Upper == 32 && Font.ripterm == 45 &&
           Font.count is Num && Font.count > 0,
           "Font constants + Font.count")
  }

  static testLogSourceConstants_() {
    check_(LogSource.print == 0 && LogSource.compileError == 1 &&
           LogSource.runtimeError == 2 && LogSource.stackFrame == 3,
           "LogSource constants")
  }

  static testParityConstants_() {
    check_(Parity.none == 0 && Parity.even == 1 && Parity.odd == 2,
           "Parity constants")
  }

  static testLogLevelConstants_() {
    check_(LogLevel.emergency == 0 && LogLevel.error == 3 &&
           LogLevel.debug == 7,
           "LogLevel constants")
  }

  static testBBSListTypeConstants_() {
    check_(BBSListType.user == 0 && BBSListType.system == 1,
           "BBSListType constants")
  }

  static testScreenModeConstants_() {
    check_(ScreenMode.current == 0 && ScreenMode.c80x25 == 1 &&
           ScreenMode.atari == 20,
           "ScreenMode constants")
  }

  static testAddressFamilyConstants_() {
    check_(AddressFamily.unspec == 0 && AddressFamily.inet == 1 &&
           AddressFamily.inet6 == 2,
           "AddressFamily constants")
  }

  static testMusicModeConstants_() {
    check_(MusicMode.syncterm == 0 && MusicMode.bansi == 1 &&
           MusicMode.enabled == 2,
           "MusicMode constants")
  }

  static testRipVersionConstants_() {
    check_(RipVersion.none == 0 && RipVersion.v1 == 1 &&
           RipVersion.v3 == 2,
           "RipVersion constants")
  }

  static testLogModeConstants_() {
    check_(LogMode.none == 0 && LogMode.ascii == 1 && LogMode.raw == 2,
           "LogMode constants")
  }

  static testStatusDisplayConstants_() {
    check_(StatusDisplay.none == 0 && StatusDisplay.indicator == 1 &&
           StatusDisplay.host == 2,
           "StatusDisplay constants")
  }

  // ===== Color ====================================================

  static testColor_() {
    // Color.fromRgb takes 16-bit channels (0..0xFFFF), not 0..255.
    var red = Color.fromRgb(0xff00, 0, 0)
    check_(red == 0xff0000, "Color.fromRgb(0xff00, 0, 0) == 0xff0000")
    var pair = Color.fromAttr(0x1f)   // 0x1f = white-on-blue
    check_(pair is List && pair.count == 2 &&
           pair[0] is Num && pair[1] is Num,
           "Color.fromAttr returns [fg, bg]")
    // toLegacyAttr is a best-match against the LEGACY palette; with
    // a BBS-customized palette the result may shift.  Only verify
    // the call succeeds and produces a byte attr.
    var back = Color.toLegacyAttr(pair[0], pair[1])
    check_(back is Num && back >= 0 && back <= 0xff,
           "Color.toLegacyAttr produces a byte attr")
  }

  // ===== read-only live bindings ==================================

  static testConnConnected_() {
    check_(Conn.connected, "Conn.connected is true")
  }

  static testConnType_() {
    var t = Conn.type
    check_(t is Num && t >= 0, "Conn.type is a Num")
  }

  static testConnPending_() {
    var p = Conn.pending
    check_(p is Num && p >= 0, "Conn.pending is non-negative")
  }

  static testConnQueued_() {
    var q = Conn.queued
    check_(q is Num && q >= 0, "Conn.queued is non-negative")
  }

  static testScreenSize_() {
    var sz = Screen.size
    check_(sz is List && sz.count == 2 &&
           sz[0] is Num && sz[0] > 0 &&
           sz[1] is Num && sz[1] > 0,
           "Screen.size returns [w, h]")
  }

  static testScreenSupports_() {
    var lf = Screen.supports.loadableFonts
    check_(lf is Bool, "Screen.supports.loadableFonts is Bool")
  }

  static testScreenFont_() {
    var f0 = Screen.font[0]
    check_(f0 is Num && f0 >= 0, "Screen.font[0] is a Num")
  }

  static testScreenWindow_() {
    var b = Screen.window.bounds
    check_(b is List && b.count == 4 &&
           b[2] >= b[0] && b[3] >= b[1],
           "Screen.window.bounds is [sx, sy, ex, ey]")
    var p = Screen.window.position
    check_(p is List && p.count == 2 &&
           p[0] is Num && p[1] is Num,
           "Screen.window.position is [x, y]")
  }

  static testCTermCursor_() {
    check_(CTerm.x is Num && CTerm.x >= 1 &&
           CTerm.y is Num && CTerm.y >= 1,
           "CTerm.x/y are 1-based positives")
  }

  static testCTermDimensions_() {
    check_(CTerm.width is Num && CTerm.width > 0 &&
           CTerm.height is Num && CTerm.height > 0,
           "CTerm.width/height are positive Nums")
  }

  static testCTermFlags_() {
    check_(CTerm.doorwayMode is Bool && CTerm.started is Bool &&
           CTerm.emulation is Num,
           "CTerm.doorwayMode/.started Bool, .emulation Num")
  }

  static testBBS_() {
    check_(BBS.name is String && BBS.name.count > 0 &&
           BBS.connType is Num,
           "BBS.name non-empty String, BBS.connType Num")
  }

  // ===== Console + REPL ===========================================

  static testConsoleTotal_() {
    var before = Console.total
    System.print("[probe]")
    var after = Console.total
    check_(after > before, "Console.total grows on print")
  }

  static testREPLEvalExpression_() {
    var r = REPL.eval("1 + 2")
    check_(r is List && r.count == 1 && r[0] == 3,
           "REPL.eval(\"1 + 2\") returns [3]")
  }

  static testREPLEvalStatement_() {
    // Variable name must not start with an underscore — Wren's lexer
    // tokenizes those as field/static-field references, never NAME.
    var r = REPL.eval("var wrenTestProbe = 99")
    check_(r == null, "REPL.eval statement form returns null")
  }

  static testREPLHasModule_() {
    check_(REPL.hasModule("syncterm") == true &&
           REPL.hasModule("does_not_exist_zzz") == false,
           "REPL.hasModule discriminates known modules")
  }

  // ===== Cell / Cells / Screen rect roundtrip =====================

  static testCellRoundtrip_() {
    // Each property is set + read independently — bright/blink share
    // legacy_attr's bit field, so setting them after legacyAttr would
    // mutate legacyAttr's stored value.
    var c = Cell.new()
    c.ch = "A"
    check_(c.ch == "A" && c.chByte == 65, "Cell.ch + chByte")
    c.legacyAttr = 0x1e
    check_(c.legacyAttr == 0x1e, "Cell.legacyAttr roundtrip")
    c.bright = true
    check_(c.bright == true, "Cell.bright = true round-trips")
    c.blink = false
    check_(c.blink == false, "Cell.blink = false round-trips")
    c.fgPalette = 7
    check_(c.fgPalette == 7, "Cell.fgPalette palette write/read")
    c.bgPalette = 0
    check_(c.bgPalette == 0, "Cell.bgPalette palette write/read")
    c.hyperlinkId = 0
    check_(c.hyperlinkId == 0, "Cell.hyperlinkId default 0")
  }

  static testScreenRectRoundtrip_() {
    // Save the whole screen so any test mutation is invisible.
    // readRect returns a Cells; mutate one cell through the view and
    // hand the same Cells object back to writeRect — no copy.
    var saved = Screen.save()
    var cells = Screen.readRect(1, 1, 1, 1)
    cells[0].ch         = "Z"
    cells[0].legacyAttr = 0x07
    Screen.writeRect(1, 1, 1, 1, cells)
    var rb = Screen.readRect(1, 1, 1, 1)
    var ok = rb != null && rb.count == 1 && rb[0].ch == "Z"
    Screen.restore(saved)
    check_(ok, "Screen.read/writeRect roundtrip")
  }

  // ===== Hyperlinks ================================================

  static testHyperlinks_() {
    var id = Hyperlinks.add("https://example.com/test", "")
    check_(id is Num && id > 0, "Hyperlinks.add returns positive id")
    check_(Hyperlinks.containsKey(id) == true,
           "Hyperlinks.containsKey(id) true")
    var uri = Hyperlinks[id]
    check_(uri is String && uri.contains("example.com"),
           "Hyperlinks[id] returns the URI")
  }

  // ===== Hook.onMatch validation ===================================

  static testAnchorReject_() {
    var f = Fiber.new { Hook.onMatch(".*x") { |m| } }
    var err = f.try()
    check_(err != null, "Hook.onMatch rejects leading .*")
  }

  static testAnchorRejectPlus_() {
    var f = Fiber.new { Hook.onMatch(".+x") { |m| } }
    var err = f.try()
    check_(err != null, "Hook.onMatch rejects leading .+")
  }

  static testAnchorRejectQuest_() {
    var f = Fiber.new { Hook.onMatch("a?b") { |m| } }
    var err = f.try()
    check_(err != null, "Hook.onMatch rejects leading a?")
  }

  // ===== sentinel-driven async tests ==============================

  static advance_() {
    __step = __step + 1
    if (__step == 1) {
      __pending = "T01"
      Conn.send("printf '__T01_X_HELLO_X__\\n'\r")
    } else if (__step == 2) {
      __pending = "T02"
      Conn.send("printf '__T02_X_joe_X__\\n'\r")
    } else if (__step == 3) {
      __pending = "T03"
      Conn.send("printf '__T03_X_a=1_X__\\n'\r")
    } else if (__step == 4) {
      __pending = "T04"
      Conn.send("printf '__T04_X_done_X__\\n'\r")
    } else if (__step == 5) {
      // Deliberate ≥1 s wall-clock pause so the main loop's
      // dispatch_timer sees Hook.every(50)'s next_fire_s pass
      // many times before report_() runs.  The sentinel string
      // is composed by printf at run time (the `\%s` is a
      // *literal* percent — escape sequence — preserved into
      // the bash command), so the *command echo* contains
      // `%s` plus `T05` separately and never matches the
      // regex; only the post-sleep printf output does.
      // Without this trick the regex matches the echo
      // immediately, sleep never gates the test, and the timer
      // assertion races.
      __pending = "T05"
      Conn.send("sleep 1 && printf '__\%s_X_done_X__\\n' T05\r")
    } else {
      report_()
    }
  }

  static handle_(name, value) {
    var expected = "T" + name
    if (__pending != expected) return
    __pending = null
    if (name == "01") {
      check_(value == "HELLO", "T01 Hook.onMatch literal echo")
    } else if (name == "02") {
      check_(value == "joe", "T02 Hook.onMatch single capture")
    } else if (name == "03") {
      check_(value == "a=1", "T03 Hook.onMatch capture preserves =")
    } else if (name == "04") {
      check_(value == "done", "T04 Hook.onMatch trailing sentinel")
    } else if (name == "05") {
      check_(value == "done", "T05 sleep+sentinel for timer/key tests")
    } else {
      check_(false, "T%(name) unexpected sentinel")
    }
    advance_()
  }

  static timeout_() {
    if (__pending == null) return
    __fail = __fail + 1
    System.print("  FAIL %(__pending) (timeout)")
    __pending = null
    advance_()
  }

  // ===== final report =============================================

  static report_() {
    // Time-based hook checks: by the time we get here, T05 has
    // forced a ≥1 s wall-clock pause via `sleep 1`, so Hook.every's
    // 50 ms timer must have fired several times and the
    // Input.unget'd 0xFE00 key has had plenty of main-loop
    // iterations to be picked up.  HookHandle.callCount > 0 is the
    // direct test — much cleaner than the old static-counter
    // baselines.  Hook.onOutput isn't checked here: the host
    // suppresses output dispatch for Wren-originated Conn.send
    // (otherwise wrenCall re-enters its own fiber and corrupts the
    // stack), so it wouldn't fire from any of this suite's sends.
    check_(__everyHook.callCount > 0,
           "Hook.every fires from main-loop timer")
    check_(__keyHook.callCount > 0,
           "Hook.onKey(0xFE00) caught Input.unget'd sentinel key")

    var total = __pass + __fail
    System.print("=== %(total) tests, %(__pass) pass, %(__fail) fail ===")
    cleanup_()
  }

  // Tear down every hook this run() registered.  Safe to call from
  // inside the very dispatch that triggered it — the host
  // tombstones (just NULLs fn) so the in-flight dispatcher walks
  // off the entry on the next iteration.
  static cleanup_() {
    for (h in __hooks) h.remove()
    __hooks     = []
    __everyHook = null
    __keyHook   = null
  }
}

// All hooks are registered inside WrenTest.run() and torn down by
// cleanup_() at the end of the suite, so the regex VM, timers, and
// filtered key listener don't keep ticking once the tests are done.
