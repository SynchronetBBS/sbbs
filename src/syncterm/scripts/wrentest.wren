// Self-test suite for SyncTERM's Wren bindings.  Triggered by Alt+T
// (registered in scripts/runtests.wren).  Assumes the remote is
// bash in a PTY for the sentinel-driven async tests.
//
// Inline tests cover everything that doesn't need to round-trip
// through the connection: constants, foreign-class roundtrips,
// REPL, etc.  The async phase issues `printf` commands with
// unique sentinels and matches them via Hook.onMatch.  Counters
// for the time-based hooks (every, filtered onKey via
// Input.unget) are ticked by hooks registered at module load and
// inspected in the final report.  T06 then drives the result-queue
// framework end-to-end: spawn a fiber that parks on
// Input.nextEvent, unget a sentinel key, and verify the fiber
// resumes with the right KeyEvent after the next main-loop drain.
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
    REPL, Input, KeyEvent, Cache, Platform, Timer, TimerElapsed
import "console" for WrenConsole
import "ui_style_test"  for UiStyleTest
import "ui_widget_test" for UiWidgetTest
import "ui_draw_test"   for UiDrawTest
import "ui_list_test"   for UiListTest
import "ui_input_test"     for UiInputTest
import "ui_button_test"    for UiButtonTest
import "ui_checkbox_test"  for UiCheckboxTest
import "ui_radio_test"     for UiRadioTest
import "ui_spinbox_test"   for UiSpinboxTest
import "ui_statusbar_test" for UiStatusbarTest
import "ui_menubar_test"   for UiMenubarTest
import "ui_form_test"      for UiFormTest
import "ui_popup_test"     for UiPopupTest
import "ui_help_test"      for UiHelpTest

class WrenTest {
  static run() {
    __step             = 0
    __pass             = 0
    __fail             = 0
    __pending          = null
    __nextEventResult  = null
    __timerResult      = null
    // Fiber that parks on Input.nextEvent and stores the resumption
    // value.  Started + unget'd at T06 so the earlier sentinel-driven
    // tests don't get their input claimed by the parked fiber.  The
    // fiber clears CTerm.suspended after capturing so T06's
    // sleep-and-printf sentinel can drain back through cterm.
    __nextEventFiber   = Fiber.new {
      Input.nextEvent(Fiber.current)
      __nextEventResult = Fiber.yield()
      CTerm.suspended = false
    }
    // Fiber that parks on Timer.trigger with ms=0; the next doterm()
    // iteration's sweep marks it past-due, the drain resumes the
    // fiber with a TimerElapsed.  Started at T06 so the wallclock
    // sentinel sleep gives the sweep time to fire.
    __timerFiber       = Fiber.new {
      Timer.trigger(Fiber.current, 0)
      __timerResult = Fiber.yield()
    }
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
    // Hook.onInput String-return path: replace each inbound LF (0x0A)
    // with CRLF.  A counter ticks each time the hook fires; report_()
    // checks it > 0 at end-of-suite.  The regex hook is registered
    // earlier in this same array, so it sees raw bytes BEFORE this
    // replacement runs (regex hooks never abort the dispatch chain).
    // Onlcr-mode PTYs already send CRLF over the wire, so the LFs we
    // see here are the bare ones (from raw printf output passing
    // through), and replacing them again just produces CRCRLF on the
    // terminal — visually harmless and keeps the regex sentinels
    // matching cleanly.
    __lfReplaceCount = 0
    __lfReplaceHook  = Hook.onInput(0x0A) { |b|
      __lfReplaceCount = __lfReplaceCount + 1
      return "\r\n"
    }
    __hooks.add(__lfReplaceHook)
    System.print("=== Wren self-test starting ===")

    // Aggregated [pass, fail] across all suites — every nested
    // test module returns its own pair, and WrenTest's __pass /
    // __fail counters are folded in at end-of-run().
    __sumPass = 0
    __sumFail = 0

    // ------ in-process UI library tests -----------------------------
    // Pure Wren, no connection needed.  Run before the
    // sentinel-driven async chain so a UI failure surfaces early.
    fold_(UiStyleTest.run())
    fold_(UiWidgetTest.run())
    fold_(UiDrawTest.run())
    fold_(UiListTest.run())
    fold_(UiInputTest.run())
    fold_(UiButtonTest.run())
    fold_(UiCheckboxTest.run())
    fold_(UiRadioTest.run())
    fold_(UiSpinboxTest.run())
    fold_(UiStatusbarTest.run())
    fold_(UiMenubarTest.run())
    fold_(UiFormTest.run())
    fold_(UiPopupTest.run())
    fold_(UiHelpTest.run())

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

    // ------ Cell / Surface / Screen rect roundtrip -----------------
    testCellRoundtrip_()
    testScreenRectRoundtrip_()

    // ------ Hyperlinks ---------------------------------------------
    testHyperlinks_()

    // ------ Hook.onMatch validation --------------------------------
    testAnchorReject_()
    testAnchorRejectPlus_()
    testAnchorRejectQuest_()

    // ------ Hook.dispatch_ contract --------------------------------
    testHookDispatchPassthrough_()
    testHookDispatchNoArg_()
    testHookDispatchRejectsYield_()
    testHookDispatchRejectsNoArgYield_()
    testHookDispatchAllowsChildFiberYield_()
    testHookDispatchCatchesAbort_()

    // ------ WrenConsole.register -----------------------------------
    testConsoleRegisterRoundtrip_()
    testConsoleRegisterOverwrite_()
    testConsoleRegisterRejectsSpace_()
    testConsoleUnregister_()

    // ------ File.readLine / File.writeLine -------------------------
    testFileLineRoundtrip_()

    // ------ File.sha1 / File.md5 -----------------------------------
    testFileHashEmpty_()
    testFileHashKnown_()

    // ------ Platform ----------------------------------------------
    testPlatformName_()

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

  // ===== Cell / Surface / Screen rect roundtrip ===================

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
    // readRect returns a Surface; mutate one cell through the view
    // and hand the same Surface back to writeRect — no copy.
    var saved = Screen.save()
    var surf = Screen.readRect(1, 1, 1, 1)
    surf[0].ch         = "Z"
    surf[0].legacyAttr = 0x07
    Screen.writeRect(1, 1, 1, 1, surf)
    var rb = Screen.readRect(1, 1, 1, 1)
    var ok = rb != null && rb.count == 1 && rb[0].ch == "Z" &&
             rb.width == 1 && rb.height == 1
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

  // ===== Hook.dispatch_ contract ==================================
  // Hook.dispatch_ wraps every hook fire in a child fiber so a
  // handler that yields directly is caught (and reported) instead
  // of stranding the C dispatcher with no return value.  These
  // tests invoke dispatch_ directly rather than going through a
  // registered hook + injected event, which keeps them synchronous
  // and lets us assert on the exact return value.

  static testHookDispatchPassthrough_() {
    check_(Hook.dispatch_(Fn.new { |x| x + 1 }, 41) == 42,
           "Hook.dispatch_(fn,arg) propagates return value")
    check_(Hook.dispatch_(Fn.new { |x| true }, 0) == true,
           "Hook.dispatch_ propagates true")
    check_(Hook.dispatch_(Fn.new { |x| false }, 0) == false,
           "Hook.dispatch_ propagates false")
  }

  static testHookDispatchNoArg_() {
    // No-arg form for timer hooks.
    check_(Hook.dispatch_(Fn.new { 7 }) == 7,
           "Hook.dispatch_(fn) propagates return value")
  }

  static testHookDispatchRejectsYield_() {
    // A handler that yields up to dispatch_'s wrapper fiber leaves
    // the wrapper not-done; dispatch_ logs an error and returns null
    // so the dispatcher's bool/string slot read falls through to the
    // default (passthrough / default status).
    var r = Hook.dispatch_(Fn.new { |x| Fiber.yield() }, 0)
    check_(r == null, "Hook.dispatch_(fn,arg) rejects direct yield")
  }

  static testHookDispatchRejectsNoArgYield_() {
    var r = Hook.dispatch_(Fn.new { Fiber.yield() })
    check_(r == null, "Hook.dispatch_(fn) rejects direct yield")
  }

  static testHookDispatchAllowsChildFiberYield_() {
    // A child fiber's yield returns to the hook body (its caller of
    // .call()), not to dispatch_'s wrapper — so the hook still
    // completes normally and dispatch_ returns its value.  This is
    // the supported pattern for parking work from inside a hook.
    // The inner fiber is built outside the Fn so the hook fn body
    // is a single expression; multi-statement Fn bodies don't
    // implicit-return the way method bodies do.
    var inner = Fiber.new { Fiber.yield(123) }
    var r = Hook.dispatch_(Fn.new { |x| inner.call() + 1 }, 0)
    check_(r == 124, "Hook.dispatch_ permits child-fiber yields")
  }

  static testHookDispatchCatchesAbort_() {
    var r = Hook.dispatch_(Fn.new { |x| Fiber.abort("test boom") }, 0)
    check_(r == null, "Hook.dispatch_ catches Fiber.abort")
  }

  // ===== WrenConsole.register =====================================
  // The dispatcher itself is in WrenConsole.runCommand_, which only
  // runs when a user is in the console.  These tests poke at the
  // registry directly via the public API; manual /? in the live
  // console is the way to verify dispatch and help-rendering.

  static testConsoleRegisterRoundtrip_() {
    WrenConsole.register("__wt_a", "test command a", Fn.new { |args| 0 })
    check_(WrenConsole.commands.contains("__wt_a"),
           "WrenConsole.register stores the command")
    WrenConsole.unregister("__wt_a")
  }

  static testConsoleRegisterOverwrite_() {
    WrenConsole.register("__wt_b", "first",  Fn.new { |args| 0 })
    WrenConsole.register("__wt_b", "second", Fn.new { |args| 1 })
    var n = 0
    for (c in WrenConsole.commands) {
      if (c == "__wt_b") n = n + 1
    }
    check_(n == 1, "WrenConsole.register overwrites same-name entry")
    WrenConsole.unregister("__wt_b")
  }

  static testConsoleRegisterRejectsSpace_() {
    var f = Fiber.new {
      WrenConsole.register("bad name", "x", Fn.new { |args| 0 })
    }
    var err = f.try()
    check_(err != null, "WrenConsole.register rejects names with spaces")
  }

  static testConsoleUnregister_() {
    WrenConsole.register("__wt_c", "to drop", Fn.new { |args| 0 })
    check_(WrenConsole.commands.contains("__wt_c"),
           "WrenConsole.register pre-condition for unregister test")
    WrenConsole.unregister("__wt_c")
    check_(!WrenConsole.commands.contains("__wt_c"),
           "WrenConsole.unregister removes the command")
    // Idempotent — double-unregister is a no-op, not an error.
    WrenConsole.unregister("__wt_c")
    check_(!WrenConsole.commands.contains("__wt_c"),
           "WrenConsole.unregister is idempotent")
  }

  // ===== File.readLine / File.writeLine ===========================
  // Writes a few lines (including blank + no-terminator-on-EOF cases),
  // closes + reopens for a clean offset, then reads them back.  Uses
  // Cache as a real on-disk directory; cleans up the test file after
  // (and once more on entry, in case a previous run aborted mid-test).
  static testFileLineRoundtrip_() {
    var name = "__wt_lines"
    if (Cache.contains(name)) Cache.delete(name)
    var f = Cache.create(name)
    if (f == null) {
      check_(false, "File.create returned null in line round-trip setup")
      return
    }
    f.open()
    f.writeLine("alpha")
    f.writeLine("")
    f.writeLine("gamma")
    // Tail without a trailing LF — readLine should still surface it.
    f.writeBytes("delta")
    f.close()

    f.open()
    f.offset = 0
    var l1 = f.readLine()
    var l2 = f.readLine()
    var l3 = f.readLine()
    var l4 = f.readLine()
    var l5 = f.readLine()
    f.close()

    check_(l1 == "alpha",
           "File.readLine: first line after writeLine")
    check_(l2 == "",
           "File.readLine: blank line is empty String, not null")
    check_(l3 == "gamma",
           "File.readLine: third line after writeLine")
    check_(l4 == "delta",
           "File.readLine: trailing line without LF is still surfaced")
    check_(l5 == null,
           "File.readLine: returns null at EOF")

    Cache.delete(name)
  }

  // Empty file exercises the special zero-length code path (xpmap
  // typically rejects 0-sized maps, so the helper synthesizes an
  // empty buffer for the hash).  Expected digests are well-known.
  static testFileHashEmpty_() {
    var name = "__wt_hash_empty"
    if (Cache.contains(name)) Cache.delete(name)
    var f = Cache.create(name)
    if (f == null) {
      check_(false, "File.create returned null in hash-empty setup")
      return
    }
    // Don't open / write — just hash an empty file.
    var sha = f.sha1
    var md5 = f.md5
    var expSha = bytesFromHex_("da39a3ee5e6b4b0d3255bfef95601890afd80709")
    var expMd5 = bytesFromHex_("d41d8cd98f00b204e9800998ecf8427e")
    check_(sha == expSha, "File.sha1 of empty file matches SHA1(\"\")")
    check_(md5 == expMd5, "File.md5 of empty file matches MD5(\"\")")
    Cache.delete(name)
  }

  // Non-empty exercises the xpmap path.  "hello" has well-known
  // SHA-1 and MD5 digests so this checks the full pipeline rather
  // than just round-tripping arbitrary bytes.
  static testFileHashKnown_() {
    var name = "__wt_hash_known"
    if (Cache.contains(name)) Cache.delete(name)
    var f = Cache.create(name)
    if (f == null) {
      check_(false, "File.create returned null in hash-known setup")
      return
    }
    f.open()
    f.write("hello")
    f.close()
    var sha = f.sha1
    var md5 = f.md5
    var expSha = bytesFromHex_("aaf4c61ddcc5e8a2dabede0f3b482cd9aea9434d")
    var expMd5 = bytesFromHex_("5d41402abc4b2a76b9719d911017c592")
    check_(sha == expSha, "File.sha1(\"hello\")")
    check_(md5 == expMd5, "File.md5(\"hello\")")
    Cache.delete(name)
  }

  // Convert a hex string to a byte-string for digest comparison.
  static bytesFromHex_(hex) {
    var out = ""
    var i = 0
    while (i < hex.count) {
      var hi = hexDigit_(hex[i])
      var lo = hexDigit_(hex[i + 1])
      out = out + String.fromByte(hi * 16 + lo)
      i = i + 2
    }
    return out
  }

  static hexDigit_(c) {
    var b = c.bytes[0]
    if (b >= 0x30 && b <= 0x39) return b - 0x30        // '0'..'9'
    if (b >= 0x61 && b <= 0x66) return b - 0x61 + 10   // 'a'..'f'
    if (b >= 0x41 && b <= 0x46) return b - 0x41 + 10   // 'A'..'F'
    Fiber.abort("bad hex digit '%(c)'")
  }

  static testPlatformName_() {
    var n = Platform.name
    check_(n is String && n.count > 0,
           "Platform.name returns non-empty String (got %(n))")
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
    } else if (__step == 6) {
      // Result-queue framework end-to-end.  Register the fiber to
      // receive the next event via Input.nextEvent, yield, then
      // unget a sentinel key.  The next main-loop iteration's
      // dispatch_key sees the registered fiber, pushes a KeyEvent
      // onto the result queue, and clears parked_fiber.  The
      // iteration after that drains the queue, which constructs the
      // foreign and resumes the fiber with it.  The 0.2 s pause via
      // sleep gives the drain a chance to fire — without it the
      // report_ check races against the resume.  0xFD00 matches the
      // same low-byte-zero constraint the existing 0xFE00 hook test
      // uses (see comment at the unget below).
      // CTerm.suspended is toggled around the unget+resume window
      // to exercise the wire-pump suspend flag — without it,
      // remote bytes from the T05 sleep tail could repaint while
      // the fiber is parked.  The flag is cleared in the fiber's
      // own body after it captures the event so the suspend
      // outlives the resume by exactly one drain.
      check_(CTerm.suspended == false,
             "CTerm.suspended defaults to false")
      CTerm.suspended = true
      check_(CTerm.suspended == true,
             "CTerm.suspended = true round-trips")
      __nextEventFiber.call()
      __timerFiber.call()
      Input.unget(KeyEvent.new(0xFD00))
      __pending = "T06"
      Conn.send("sleep 0.2 && printf '__\%s_X_done_X__\\n' T06\r")
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
    } else if (name == "06") {
      check_(value == "done", "T06 sleep+sentinel for result-queue test")
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
    // baselines.
    check_(__everyHook.callCount > 0,
           "Hook.every fires from main-loop timer")
    check_(__keyHook.callCount > 0,
           "Hook.onKey(0xFE00) caught Input.unget'd sentinel key")

    // Result-queue end-to-end: T06 unget'd 0xFD00 while the fiber
    // was parked, so dispatch_key should have routed it through the
    // queue and the drain should have resumed the fiber with a
    // KeyEvent before T06's sentinel sleep elapsed.
    check_(__nextEventResult is KeyEvent &&
           __nextEventResult.code == 0xFD00,
           "Input.nextEvent through result queue (T06 sentinel key)")
    // Fiber cleared CTerm.suspended after capturing.  If this still
    // reads true, either the fiber never resumed or the setter is
    // broken — and in the former case the T06 sentinel could only
    // have arrived because something else cleared it (the test
    // would still be wrong).
    check_(CTerm.suspended == false,
           "CTerm.suspended cleared by fiber after Input.nextEvent")

    // Timer.trigger(ms=0) parks the fiber; the next doterm() iteration
    // sweeps it past-due, the drain resumes with a TimerElapsed.  By
    // the time T06's wallclock sentinel arrives this has had plenty
    // of iterations to fire.
    check_(__timerResult is TimerElapsed,
           "Timer.trigger resumes fiber with TimerElapsed")

    // Hook.onInput String-replacement path: each LF off the wire
    // fires the LF->CRLF hook and runs through the C-side dispatcher's
    // WREN_TYPE_STRING branch.  Counter > 0 means the dispatcher
    // recognized the String return, copied the bytes into the filter
    // buffer, and aborted the rest of the hook chain (matching the
    // contract for replace).
    check_(__lfReplaceCount > 0,
           "Hook.onInput String return (LF->CRLF) fired off the wire")

    var total = __pass + __fail
    System.print("=== wrentest: %(total) tests, %(__pass) pass, %(__fail) fail ===")
    fold_([__pass, __fail])
    var grand = __sumPass + __sumFail
    System.print("=== TOTAL: %(grand) tests, %(__sumPass) pass, %(__sumFail) fail ===")
    cleanup_()
  }

  // Accumulate a [pass, fail] pair returned by a nested test suite
  // into the aggregate WrenTest reports at end-of-run.
  static fold_(r) {
    __sumPass = __sumPass + r[0]
    __sumFail = __sumFail + r[1]
  }

  // Tear down every hook this run() registered.  Safe to call from
  // inside the very dispatch that triggered it — the host
  // tombstones (just NULLs fn) so the in-flight dispatcher walks
  // off the entry on the next iteration.
  static cleanup_() {
    for (h in __hooks) h.remove()
    __hooks         = []
    __everyHook     = null
    __keyHook       = null
    __lfReplaceHook = null
  }
}

// All hooks are registered inside WrenTest.run() and torn down by
// cleanup_() at the end of the suite, so the regex VM, timers, and
// filtered key listener don't keep ticking once the tests are done.
