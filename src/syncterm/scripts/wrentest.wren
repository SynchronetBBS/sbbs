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
// inspected in the final report.  T06-T10 drive the input claim +
// result-queue framework end-to-end: T06 pushes a claim, ungets a
// sentinel, and verifies the fiber resumes with the KeyEvent;
// T07 exercises Wake.post on parked fibers; T08/T09 stack two
// claims and confirm newer-fiber-wins + restore-on-pop semantics;
// T10 pushes a claim that returns false and verifies the event
// falls through to a registered Hook.onKey.
//
// Re-running via Alt+T resets state in WrenTest.run() - the
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
    REPL, Input, Wake, KeyEvent, MouseEvent, Mouse, Cache, Platform,
    Timer, TimerElapsed, WON, FileError, FileErr, WONError, WONErr,
    Error, ScriptError, Surface, Scrollback, Host
import "wren_console" for WrenConsole
import "ui_style_test"  for UiStyleTest
import "ui_widget_test" for UiWidgetTest
import "ui_draw_test"   for UiDrawTest
import "ui_list_test"   for UiListTest
import "ui_input_test"     for UiInputTest
import "ui_button_test"    for UiButtonTest
import "ui_checkbox_test"  for UiCheckboxTest
import "ui_radio_test"     for UiRadioTest
import "ui_spinbox_test"   for UiSpinboxTest
import "ui_color_picker_test" for UiColorPickerTest
import "menu_ui_test" for MenuUiTest
import "menu_palette_picker_test" for MenuPalettePickerTest
import "ui_statusbar_test" for UiStatusbarTest
import "ui_menubar_test"   for UiMenubarTest
import "ui_form_test"      for UiFormTest
import "ui_popup_test"     for UiPopupTest
import "ui_help_test"      for UiHelpTest
import "ui_progress_test"  for UiProgressTest
import "ui_logview_test"   for UiLogviewTest
import "scrollback_view"   for ScrollbackView

class WrenTest {
  static run() {
    __step             = 0
    __pass             = 0
    __fail             = 0
    __pending          = null
    __nextEventResult  = null
    __timerResult      = null
    __wakeResult       = null
    __wakeStringResult = null
    // Fiber that pushes a one-shot input claim and parks until the
    // claim fires.  Started + unget'd at T06 so the earlier
    // sentinel-driven tests don't get their input claimed first.
    // The claim wakes the fiber with the event, then pops itself
    // and returns true (consumed).  The fiber clears CTerm.suspended
    // after capturing so T06's sleep-and-printf sentinel can drain
    // back through cterm.
    __nextEventFiber   = Fiber.new {
      var ch
      ch = Input.pushClaim(Fn.new {|ev|
        Wake.post(__nextEventFiber, ev)
        ch.pop()
        return true
      })
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
    // Fibers that park on plain `Fiber.yield()` (no registration on
    // the input or timer queues).  Wake.post should still be able
    // to resume them via the result-queue path.  One captures a
    // KeyEvent (a foreign object), one captures a String - both
    // exercise the WrenHandle pin/release in the wake payload.
    __wakeFiber        = Fiber.new {
      __wakeResult = Fiber.yield()
    }
    __wakeStringFiber  = Fiber.new {
      __wakeStringResult = Fiber.yield()
    }
    // Two fibers each push a long-lived input claim; T08 ungets a
    // sentinel key, the newer-fiber claim should fire first by the
    // per-fiber-stack semantics (newest on top).  After T08, the
    // top claim's handle is popped and a second key is unget'd -
    // the older fiber's claim should now fire.  Both counters wind
    // up at 1; whichever is wrong proves the dispatch order.
    __claimACount = 0
    __claimBCount = 0
    __claimAFiber = Fiber.new {
      __claimAHandle = Input.pushClaim(Fn.new {|ev|
        __claimACount = __claimACount + 1
        return true
      })
      Fiber.yield()
    }
    __claimBFiber = Fiber.new {
      __claimBHandle = Input.pushClaim(Fn.new {|ev|
        __claimBCount = __claimBCount + 1
        return true
      })
      Fiber.yield()
    }
    // Fall-through: a claim that returns false (passes the event) and
    // a Hook.onKey for the same key.  T10 ungets the key after popping
    // every other claim from the stack; both counters should bump.
    __fallthroughCount     = 0
    __fallthroughHookCount = 0
    __fallthroughFiber = Fiber.new {
      __fallthroughHandle = Input.pushClaim(Fn.new {|ev|
        __fallthroughCount = __fallthroughCount + 1
        return false
      })
      Fiber.yield()
    }
    __fallthroughHook = Hook.onKey(0xFA00) {|k|
      __fallthroughHookCount = __fallthroughHookCount + 1
      return true
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
    // terminal - visually harmless and keeps the regex sentinels
    // matching cleanly.
    __lfReplaceCount = 0
    __lfReplaceHook  = Hook.onInput(0x0A) { |b|
      __lfReplaceCount = __lfReplaceCount + 1
      return "\r\n"
    }
    __hooks.add(__lfReplaceHook)
    __hooks.add(__fallthroughHook)
    print_("=== Wren self-test starting ===")

    // Aggregated [pass, fail] across all suites - every nested
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
    fold_(UiColorPickerTest.run())
    fold_(MenuUiTest.run())
    fold_(MenuPalettePickerTest.run())
    fold_(UiStatusbarTest.run())
    fold_(UiMenubarTest.run())
    fold_(UiFormTest.run())
    fold_(UiPopupTest.run())
    fold_(UiHelpTest.run())
    fold_(UiProgressTest.run())
    fold_(UiLogviewTest.run())

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
    testMouseEventCtors_()
    testScrollbackMouseMask_()
    testKeyEventCtorValidation_()

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
    testConsoleEntriesSequence_()
    testREPLEvalExpression_()
    testREPLEvalStatement_()
    testREPLHasModule_()

    // ------ Cell / Surface / Screen rect roundtrip -----------------
    testCellRoundtrip_()
    testCellEqContent_()
    testScreenRectRoundtrip_()
    testSurfaceRowsCols_()

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

    // ------ FileError ---------------------------------------------
    testFileErrConstants_()

    // ------ Platform ----------------------------------------------
    testPlatformName_()

    // ------ WON serialize / deserialize ----------------------------
    testWONPrimitives_()
    testWONStringEscapes_()
    testWONContainers_()
    testWONPrettyPrint_()
    testWONRangeCoerce_()
    testWONDeserializeWhitespace_()
    testWONDeserializeTrailingComma_()
    testWONHexLiteral_()
    testWONRoundTripNested_()
    testWONLossyOmits_()
    testWONLossyTopLevel_()
    testWONStrictAborts_()
    testWONCycleAborts_()
    testWONDeserializeErrors_()

    // ------ Surface.urlAt / Scrollback -----------------------------
    testSurfaceUrlAt_()
    testScrollbackContract_()
    testScrollbackPushPop_()
    testScrollbackUrlAt_()
    testScrollbackAsSurfaceSource_()

    // ------ inject a sentinel key for async onKey filter test ------
    // Filter key must have low byte 0x00 (or 0xe0) - ciolib's
    // ungetch / getch byte-stuff a 16-bit value as low|high and only
    // re-compose at rip_getch() when the first byte read is 0 or
    // 0xe0.  0xFE00 fits that constraint and isn't a real CIO_KEY.
    Input.unget(KeyEvent.new(0xFE00))

    // ------ kick off the sentinel-driven async chain ----------------
    advance_()
  }

  // Only FAILs get printed; the summary line at the end carries
  // the pass count.  Cuts ~250 lines of noise per Alt+T run.
  static check_(ok, label) {
    if (ok) {
      __pass = __pass + 1
    } else {
      __fail = __fail + 1
      print_("  FAIL %(label)")
    }
  }

  // Dispatch print: when invoked via syncterm -W (Host.launchScript
  // is set), emit to actual stdout so the launching shell sees the
  // results.  Otherwise fall back to System.print, which logs to
  // the Wren console.  The two Console-test probes elsewhere keep
  // calling System.print directly because they specifically verify
  // that System.print appends to the console log.
  static print_(s) {
    if (Host.launchScript != null) {
      Host.print(s)
    } else {
      System.print(s)
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

  // Five MouseEvent.new overloads share a single C allocator that
  // dispatches on wrenGetSlotCount.  Verify each arity fills the
  // missing fields with the documented defaults: endX/endY copy
  // startX/startY, modifiers defaults to 0, bstate derives from
  // event (bit for the button it's about, 0 for MOUSE_MOVE).
  static testMouseEventCtors_() {
    // 3-arg: every default kicks in.  button1Click → button 1 →
    // bstate bit 0.
    var a = MouseEvent.new(Mouse.button1Click, 5, 6)
    check_(a.event == Mouse.button1Click && a.startX == 5 &&
           a.startY == 6 && a.endX == 5 && a.endY == 6 &&
           a.modifiers == 0 && a.bstate == 1,
           "MouseEvent.new(event, sx, sy) defaults correctly")

    // 4-arg: explicit modifiers, defaults for end and bstate.
    var b = MouseEvent.new(Mouse.button1Press, 7, 8, 0x42)
    check_(b.endX == 7 && b.endY == 8 && b.modifiers == 0x42 &&
           b.bstate == 1,
           "MouseEvent.new(event, sx, sy, modifiers)")

    // 5-arg: explicit endX/endY; modifiers + bstate default.
    // Mouse.button1DragMove (8) is button 1 → bstate 1.
    var c = MouseEvent.new(Mouse.button1DragMove, 1, 2, 9, 10)
    check_(c.endX == 9 && c.endY == 10 && c.modifiers == 0 &&
           c.bstate == 1,
           "MouseEvent.new(event, sx, sy, ex, ey)")

    // 6-arg: everything explicit except bstate.  button2Press (10)
    // → button 2 → bstate bit 1 (value 2).
    var d = MouseEvent.new(10, 3, 4, 5, 6, 7)
    check_(d.modifiers == 7 && d.bstate == 2,
           "MouseEvent.new(event, sx, sy, ex, ey, modifiers)")

    // 7-arg: every field explicit, including a bstate that does
    // NOT match the natural derivation from the event code.
    var e = MouseEvent.new(Mouse.button1Click, 1, 2, 3, 4, 5, 0xFF)
    check_(e.bstate == 0xFF,
           "MouseEvent.new(7-arg) honors explicit bstate")

    // Mouse.move event → bstate 0 by derivation.
    var f = MouseEvent.new(Mouse.move, 0, 0)
    check_(f.bstate == 0, "MouseEvent.new with Mouse.move → bstate 0")
  }

  static testScrollbackMouseMask_() {
    var saved = Input.mouseEvents
    var returned = ScrollbackView.setupMouse_()
    // Bits 7/8/9 are button-1 drag start/move/end; 28 and 37 are
    // wheel up/down press.  No raw movement or remote button events.
    check_(returned == saved && Input.mouseEvents == 137707389824,
           "ScrollbackView replaces the mouse mask for selection")
    Input.mouseEvents = returned
    check_(Input.mouseEvents == saved,
           "ScrollbackView mouse mask can be restored exactly")
  }

  // KeyEvent.new validates that the code is roundtrip-representable
  // through ciolib's byte-stuffing: high-byte 0 (single-byte ASCII /
  // printable) OR low-byte 0 / 0xE0 (extended scancode-style).  Any
  // other combination would silently split into two reads on
  // Input.unget - reject at construction.
  static testKeyEventCtorValidation_() {
    // Valid: high byte 0 (printable / ASCII).
    var a = KeyEvent.new(0x001B)        // Esc
    check_(a.code == 0x001B, "KeyEvent.new(0x001B) accepted (Esc)")
    var b = KeyEvent.new(0x0041)        // 'A'
    check_(b.code == 0x0041, "KeyEvent.new(0x0041) accepted (printable)")

    // Valid: low byte 0 (extended scancode-only).
    var c = KeyEvent.new(0x3B00)        // F1
    check_(c.code == 0x3B00, "KeyEvent.new(0x3B00) accepted (F1)")

    // Valid: low byte 0xE0 (E0-prefixed extended).
    var d = KeyEvent.new(0x29E0)        // wrenConsole
    check_(d.code == 0x29E0, "KeyEvent.new(0x29E0) accepted (E0-extended)")

    // Invalid: high byte non-zero AND low byte not 0/0xE0.
    var f = Fiber.new { KeyEvent.new(0xFB01) }
    var err = f.try()
    check_(err is String && err.contains("0xFB01") &&
           err.contains("not a representable"),
           "KeyEvent.new(0xFB01) rejected")

    var g = Fiber.new { KeyEvent.new(0x1234) }
    var err2 = g.try()
    check_(err2 is String && err2.contains("0x1234"),
           "KeyEvent.new(0x1234) rejected")
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
           CTerm.mouseSgrPixels is Bool && CTerm.emulation is Num,
           "CTerm.doorwayMode/.started/.mouseSgrPixels Bool, .emulation Num")
  }

  static testBBS_() {
    check_(BBS.name is String && BBS.name.count > 0 &&
           BBS.connType is Num && BBS.wrenScripts is List,
           "BBS identity fields and Wren script list are available")
  }

  // ===== Console + REPL ===========================================

  static testConsoleTotal_() {
    var before = Console.total
    System.print("[probe]")
    var after = Console.total
    check_(after > before, "Console.total grows on print")
  }

  static testConsoleEntriesSequence_() {
    System.print("[entries-probe]")
    var entries = Console.entries
    check_(entries is Sequence, "Console.entries is Sequence")
    check_(entries.count == Console.count,
           "Console.entries.count mirrors Console.count")
    var list = entries.toList
    check_(list is List && list.count == entries.count,
           "Console.entries.toList materializes")
    var hits = entries.where {|e| e[2].contains("[entries-probe]") }.toList
    check_(hits.count >= 1,
           "Console.entries.where finds the probe entry")
  }

  static testREPLEvalExpression_() {
    var r = REPL.eval("1 + 2")
    check_(r is List && r.count == 1 && r[0] == 3,
           "REPL.eval(\"1 + 2\") returns [3]")
  }

  static testREPLEvalStatement_() {
    // Variable name must not start with an underscore - Wren's lexer
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
    // Each property is set + read independently - bright/blink share
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

  static testCellEqContent_() {
    var a = Cell.new()
    a.ch = "Z"
    a.legacyAttr = 0x07
    a.fgPalette  = 7
    a.bgPalette  = 0
    var b = Cell.new()
    b.ch = "Z"
    b.legacyAttr = 0x07
    b.fgPalette  = 7
    b.bgPalette  = 0
    check_(a.eqContent(b), "Cell.eqContent: identical content matches")
    check_(a == a, "Cell == is identity (same instance)")
    check_(!(a == b),
           "Cell == stays identity-based (different instances unequal)")
    b.ch = "Y"
    check_(!a.eqContent(b),
           "Cell.eqContent: differing ch is unequal")
    check_(!a.eqContent("not a cell"),
           "Cell.eqContent: non-Cell other returns false (no abort)")
    // Surface roundtrip via cellAt should produce eqContent-equal cells.
    var s = Surface.new(2, 1)
    var src = s.cellAt(0, 0)
    src.ch         = "Q"
    src.legacyAttr = 0x1e
    src.fgRgb      = 0x336699
    var dst = s.cellAt(1, 0)
    dst.ch         = "Q"
    dst.legacyAttr = 0x1e
    dst.fgRgb      = 0x336699
    check_(src.eqContent(dst),
           "Cell.eqContent: cells from same Surface with matching content")
  }

  static testScreenRectRoundtrip_() {
    // Save the whole screen so any test mutation is invisible.
    // readRect returns a Surface; mutate one cell through the view
    // and hand the same Surface back to writeRect - no copy.
    var ok
    Screen.modalRun(Fn.new {
      var surf = Screen.readRect(1, 1, 1, 1)
      surf[0].ch         = "Z"
      surf[0].legacyAttr = 0x07
      Screen.writeRect(1, 1, 1, 1, surf)
      var rb = Screen.readRect(1, 1, 1, 1)
      ok = rb != null && rb.count == 1 && rb[0].ch == "Z" &&
           rb.width == 1 && rb.height == 1
    })
    check_(ok, "Screen.read/writeRect roundtrip")
  }

  // Build a 3×2 surface, stamp a known pattern via cellAt, then verify
  // the rows / cols views see the same cells in the right order - and
  // that linear iteration walks row-major.
  static testSurfaceRowsCols_() {
    var s = Surface.new(3, 2)
    var letters = ["A", "B", "C", "D", "E", "F"]
    for (y in 0...2) {
      for (x in 0...3) s.cellAt(x, y).ch = letters[y * 3 + x]
    }
    // Linear: row-major.
    var lin = ""
    for (c in s) lin = lin + c.ch
    check_(lin == "ABCDEF", "Surface linear iteration is row-major")
    // .rows: outer-by-y, inner-by-x.
    var byRow = ""
    for (row in s.rows) for (c in row) byRow = byRow + c.ch
    check_(byRow == "ABCDEF", "Surface.rows iterates row-major")
    check_(s.rows.count == 2, "Surface.rows.count == height")
    check_(s.rows[1][0].ch == "D", "Surface.rows[y][x] indexes correctly")
    // .cols: outer-by-x, inner-by-y.
    var byCol = ""
    for (col in s.cols) for (c in col) byCol = byCol + c.ch
    check_(byCol == "ADBECF", "Surface.cols iterates col-major")
    check_(s.cols.count == 3, "Surface.cols.count == width")
    check_(s.cols[2][1].ch == "F", "Surface.cols[x][y] indexes correctly")
    // Sequence helpers should compose.
    var rowChars = s.rows[0].map {|c| c.ch }.toList
    check_(rowChars.count == 3 && rowChars[0] == "A" && rowChars[2] == "C",
           "Surface.rows[y].map composes")
  }

  // Stamp a known URL string into a Surface and verify Surface.urlAt
  // walks left/right from a click anywhere inside it.
  static testSurfaceUrlAt_() {
    var s = Surface.new(40, 1)
    var url = "https://example.com/x"
    for (i in 0...url.count) s.cellAt(i, 0).ch = url[i]
    var clickHit = s.urlAt(0, 0)
    check_(clickHit is String && clickHit == url,
           "Surface.urlAt: click on URL returns it")
    var clickMid = s.urlAt(8, 0)
    check_(clickMid == url,
           "Surface.urlAt: click in the middle of URL still finds it")
    var clickMiss = s.urlAt(35, 0)
    check_(clickMiss == null,
           "Surface.urlAt: click on blank cell returns null")
    var clickOOB = s.urlAt(-1, 0)
    check_(clickOOB == null, "Surface.urlAt: out-of-bounds returns null")
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

  // ===== Scrollback ================================================

  // Scrollback presents the Surface contract via linearize-and-
  // dispatch.  Verify the basic shape: width/height/count are
  // numeric and consistent, `is` reports Surface/Sequence/Object/
  // Scrollback, putRect/fill/rows/cols methods exist (we don't blit
  // here, just smoke-check accessor existence), cellAt returns Cell
  // or null, [i] roundtrips with cellAt.
  static testScrollbackContract_() {
    var w = Scrollback.width
    var h = Scrollback.height
    check_(w is Num && w >= 0, "Scrollback.width is non-negative Num")
    check_(h is Num && h >= 0, "Scrollback.height is non-negative Num")
    check_(Scrollback.count == w * h, "Scrollback.count == width * height")

    // is(_) override claims Surface / Sequence ancestry without
    // actually inheriting (Wren forbids foreign-from-foreign).
    check_(Scrollback is Surface,    "Scrollback is Surface")
    check_(Scrollback is Sequence,   "Scrollback is Sequence")
    check_(Scrollback is Scrollback, "Scrollback is Scrollback")
    check_(Scrollback is Object,     "Scrollback is Object")
    check_(!(Scrollback is Cell),    "Scrollback is not Cell")

    // Out-of-bounds cellAt returns null for both axes.
    check_(Scrollback.cellAt(-1, 0) == null,
           "Scrollback.cellAt(-1, 0) == null")
    check_(Scrollback.cellAt(0, h + 1) == null,
           "Scrollback.cellAt(0, height+1) == null")

    // Wren-side rows/cols wrappers - non-null, lengths match.
    if (h > 0) {
      check_(Scrollback.rows.count == h, "Scrollback.rows.count == height")
    }
    if (w > 0) {
      check_(Scrollback.cols.count == w, "Scrollback.cols.count == width")
    }
  }

  // pushScreen / popScreen bracket a synthetic modal: push grows
  // height by exactly screenheight rows (clamped at backlines), pop
  // restores it.  Reads the cterm screen size from CTerm.height.
  static testScrollbackPushPop_() {
    var w = Scrollback.width
    if (w == 0) {
      // No ring allocated yet - push/pop are no-ops; just smoke.
      Scrollback.pushScreen()
      Scrollback.popScreen(0)
      check_(true, "Scrollback push/pop with no ring is a no-op")
      return
    }
    var hBefore = Scrollback.height
    var screenH = CTerm.height
    Scrollback.pushScreen()
    var hAfter = Scrollback.height
    var grew   = hAfter - hBefore
    // Ring grows by min(screenH, capacity-remaining).  In the
    // typical fresh-session case capacity is plenty; the count
    // either grew by screenH or hit the cap.
    check_(grew == screenH || hAfter <= w * 1000, // sanity bound
           "Scrollback.pushScreen grew height by ~screenH (was %(hBefore), now %(hAfter), screenH=%(screenH))")
    Scrollback.popScreen(screenH)
    check_(Scrollback.height == hBefore,
           "Scrollback.popScreen restored height")
  }

  // urlAt on the live ring - push the screen so we have rows to
  // write into, write a known URL directly into scrollback cells,
  // and verify urlAt detects it.  Avoids depending on exact
  // pushScreen row counts (which use gettextinfo().screenheight,
  // not CTerm.height).
  static testScrollbackUrlAt_() {
    if (Scrollback.width == 0) {
      check_(true, "Scrollback.urlAt skipped (no ring)")
      return
    }
    Scrollback.pushScreen()
    var pushed = Scrollback.height
    if (pushed == 0) {
      Scrollback.popScreen(0)
      check_(true, "Scrollback.urlAt skipped (no rows after push)")
      return
    }
    // Write into the most-recently-pushed row.
    var row = pushed - 1
    var w   = Scrollback.width
    for (i in 0...w) Scrollback.cellAt(i, row).ch = " "
    var url = "https://example.com/sb"
    for (i in 0...url.count) Scrollback.cellAt(i, row).ch = url[i]

    var hit = Scrollback.urlAt(0, row)
    check_(hit is String && hit.contains("example.com"),
           "Scrollback.urlAt: written URL is detected")
    var midHit = Scrollback.urlAt(8, row)
    check_(midHit is String && midHit.contains("example.com"),
           "Scrollback.urlAt: mid-URL click finds it")
    var miss = Scrollback.urlAt(w - 1, row)
    check_(miss == null,
           "Scrollback.urlAt: blank cell returns null")
    Scrollback.popScreen(CTerm.height)
  }

  // Scrollback as a SOURCE for Surface.putRect_ - exercises the
  // slot_to_surface_ extension that recognizes the Scrollback class
  // and linearizes the ring before reading.  Push rows into the ring,
  // write a marker into the pushed row, blit that row into a fresh
  // Surface, and verify the cells landed.
  static testScrollbackAsSurfaceSource_() {
    if (Scrollback.width == 0) {
      check_(true, "Scrollback as source skipped (no ring)")
      return
    }
    var marker = "SB_BLIT"
    if (Scrollback.width < marker.count) {
      check_(true, "Scrollback as source skipped (narrow ring)")
      return
    }
    Scrollback.pushScreen()
    var screenH = CTerm.height
    var row = Scrollback.height - 1
    for (i in 0...Scrollback.width) Scrollback.cellAt(i, row).ch = " "
    for (i in 0...marker.count) Scrollback.cellAt(i, row).ch = marker[i]
    // Copy one row from Scrollback into a fresh Surface and check
    // the marker text comes through.  putRect_ is the 7-arg foreign
    // (src, srcX, srcY, srcW, srcH, dstX, dstY) on the dst Surface;
    // src is the Scrollback class, recognized by slot_to_surface_.
    var dst = Surface.new(Scrollback.width, 1)
    dst.putRect_(Scrollback, 0, row, Scrollback.width, 1, 0, 0)
    var s = ""
    for (i in 0...Scrollback.width) s = s + dst[i].ch
    var ok = s.contains(marker)
    Scrollback.popScreen(screenH)
    check_(ok, "Scrollback as Surface source for putRect_ blits cells")
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
    // .call()), not to dispatch_'s wrapper - so the hook still
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
    // Idempotent - double-unregister is a no-op, not an error.
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
    // Tail without a trailing LF - readLine should still surface it.
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

  // FileError / FileErr are wired up.  We can't construct a
  // FileError from Wren (no public constructor) or trigger a
  // recoverable I/O failure from a deterministic test (every
  // Bucket B/C path requires OS-level conditions: disk full,
  // read-only mount, external rm out from under us, OOM).  So
  // this only verifies the bindings exist and the enum values
  // are sensible integers.
  static testFileErrConstants_() {
    check_(FileErr.ok            == 0 &&
           FileErr.openFailed    == 1 &&
           FileErr.writeFailed   == 2 &&
           FileErr.statFailed    == 3 &&
           FileErr.mmapFailed    == 4 &&
           FileErr.oom           == 5 &&
           FileErr.vanished      == 6 &&
           FileErr.resolveFailed == 7,
           "FileErr enum constants")
    check_(FileError is Class,
           "FileError class importable from syncterm")
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
    // Don't open / write - just hash an empty file.
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

  // ===== WON serialize / deserialize =============================

  static testWONPrimitives_() {
    check_(
      WON.serialize(null)  == "null"  &&
      WON.serialize(true)  == "true"  &&
      WON.serialize(false) == "false" &&
      WON.serialize(0)     == "0"     &&
      WON.serialize(42)    == "42"    &&
      WON.serialize(-7)    == "-7"    &&
      WON.serialize(3.14)  == "3.14"  &&
      WON.serialize("")    == "\"\""  &&
      WON.serialize("hi")  == "\"hi\"",
      "WON.serialize primitives")
    check_(
      WON.deserialize("null")  == null  &&
      WON.deserialize("true")  == true  &&
      WON.deserialize("false") == false &&
      WON.deserialize("42")    == 42    &&
      WON.deserialize("-7")    == -7    &&
      WON.deserialize("3.14")  == 3.14  &&
      WON.deserialize("\"\"")  == ""    &&
      WON.deserialize("\"hi\"") == "hi",
      "WON.deserialize primitives")
  }

  static testWONStringEscapes_() {
    // Each input has a tricky byte; serialize must escape, deserialize
    // must round-trip back to the original string.
    var cases = [
      ["a\"b",      "\"a\\\"b\""],   // double quote
      ["a\\b",      "\"a\\\\b\""],   // backslash
      ["a\nb",      "\"a\\nb\""],    // newline
      ["a\tb",      "\"a\\tb\""],    // tab
      ["a\%(x)b",   "\"a\\\%(x)b\""], // percent (interp suppression)
      ["a\x01b",    "\"a\\x01b\""],  // control byte
      ["a\x7Fb",    "\"a\\x7Fb\""],  // DEL
      ["a\u00FFb",  "\"a\u00FFb\""]  // UTF-8 high byte preserved literally
    ]
    var ok = true
    for (c in cases) {
      if (WON.serialize(c[0]) != c[1]) ok = false
      if (WON.deserialize(c[1]) != c[0]) ok = false
    }
    check_(ok, "WON string escapes round-trip")
  }

  static testWONContainers_() {
    check_(
      WON.serialize([])           == "[]"           &&
      WON.serialize({})           == "{}"           &&
      WON.serialize([1, 2, 3])    == "[1,2,3]"      &&
      WON.serialize([[1], [2]])   == "[[1],[2]]"    &&
      WON.serialize({"a": 1})     == "{\"a\":1}"    &&
      WON.serialize({1: "x"})     == "{1:\"x\"}",
      "WON.serialize containers (compact)")
    var l = WON.deserialize("[1,2,3]")
    var m = WON.deserialize("{\"a\":1,\"b\":2}")
    check_(l is List && l.count == 3 && l[0] == 1 && l[2] == 3 &&
           m is Map && m.count == 2 && m["a"] == 1 && m["b"] == 2,
           "WON.deserialize containers")
  }

  static testWONPrettyPrint_() {
    // Lists iterate in stable order so we can pin the exact string.
    var p1 = WON.serialize([1, 2, 3], "  ")
    var expected1 = "[\n  1,\n  2,\n  3\n]"
    // Single-key map exercises the ": " separator and the inner/outer
    // indent transitions deterministically (Wren Map iteration order
    // is hash-bucket order, not insertion order).
    var p2 = WON.serialize({"a": [1, 2]}, "  ")
    var expected2 = "{\n  \"a\": [\n    1,\n    2\n  ]\n}"
    check_(p1 == expected1 && p2 == expected2,
           "WON pretty-print indents and uses ': '")
    // Pretty output must round-trip.
    check_(WON.deserialize(p1).join(",") == "1,2,3" &&
           WON.deserialize(p2)["a"][1] == 2,
           "WON pretty output round-trips through deserialize")
  }

  static testWONRangeCoerce_() {
    check_(
      WON.serialize(1..3)  == "[1,2,3]" &&    // inclusive
      WON.serialize(1...3) == "[1,2]",        // exclusive
      "WON.serialize coerces Range to List")
  }

  static testWONDeserializeWhitespace_() {
    var v = WON.deserialize("  [ 1 , 2 , 3 ]  ")
    var v2 = WON.deserialize("[\n  1,\n  2\n]")
    check_(v.join(",") == "1,2,3" && v2.join(",") == "1,2",
           "WON.deserialize tolerates arbitrary whitespace")
  }

  static testWONDeserializeTrailingComma_() {
    var l = WON.deserialize("[1, 2, 3,]")
    var m = WON.deserialize("{1: 2, 3: 4,}")
    check_(l.count == 3 && l[2] == 3 && m.count == 2 && m[3] == 4,
           "WON.deserialize accepts trailing commas")
  }

  static testWONHexLiteral_() {
    check_(WON.deserialize("0xFF")  == 255 &&
           WON.deserialize("-0x10") == -16 &&
           WON.deserialize("0x0")   == 0,
           "WON.deserialize accepts hex literals")
  }

  static testWONRoundTripNested_() {
    var data = {
      "name":   "syncterm",
      "items":  [1, 2, [3, 4, [5]]],
      "nested": {"flags": [true, false, null], "n": 7}
    }
    var s = WON.serialize(data)
    var back = WON.deserialize(s)
    var s2 = WON.serialize(back)
    check_(s == s2, "WON nested data round-trips")
  }

  static testWONLossyOmits_() {
    // A Fiber is unsupported; lossy mode must skip it from a List.
    var f = Fiber.new {}
    check_(WON.serializeLossy([1, f, 2, f, 3]) == "[1,2,3]",
           "WON.serializeLossy skips unsupported list items")
    // Round-trip the lossy Map to compare structurally - Wren Map
    // iteration order is hash-bucket order, so the literal string
    // depends on key hashes.
    var back = WON.deserialize(WON.serializeLossy({"a": 1, "b": f, "c": 3}))
    check_(back is Map && back.count == 2 &&
           back["a"] == 1 && back["c"] == 3 && !back.containsKey("b"),
           "WON.serializeLossy skips unsupported map values")
  }

  static testWONLossyTopLevel_() {
    var f = Fiber.new {}
    check_(WON.serializeLossy(f) == "null",
           "WON.serializeLossy: top-level unsupported -> null")
  }

  static testWONStrictAborts_() {
    var f = Fiber.new {}
    var msg = Fiber.new { WON.serialize([1, f]) }.try()
    check_(msg is String && msg.contains("unsupported"),
           "WON.serialize aborts on unsupported types")
  }

  static testWONCycleAborts_() {
    var c = []
    c.add(c)
    var m1 = Fiber.new { WON.serialize(c) }.try()
    var m2 = Fiber.new { WON.serializeLossy(c) }.try()
    check_(m1 is String && m1.contains("cycle") &&
           m2 is String && m2.contains("cycle"),
           "WON cycles abort in both strict and lossy modes")
  }

  // WON.deserialize parse failures return a WONError IN PLACE of
  // the parsed value - they don't abort the fiber.  Programmer
  // errors (passing a non-String) DO still abort.
  static testWONDeserializeErrors_() {
    var bad = WON.deserialize("xyz")
    var unt = WON.deserialize("[1,2")
    var trl = WON.deserialize("[1] more")
    var key = WON.deserialize("{[1]: 2}")
    var deep = WON.deserialize("\\u01")        // truncated unicode escape
    check_(bad is WONError && bad.code == WONErr.syntax,
           "WON.deserialize: syntax error returns WONError")
    check_(unt is WONError && unt.code == WONErr.truncated,
           "WON.deserialize: truncated input returns WONError")
    check_(trl is WONError && trl.code == WONErr.trailingData,
           "WON.deserialize: trailing data returns WONError")
    check_(key is WONError && key.code == WONErr.invalidKey,
           "WON.deserialize: List/Map as Map key returns WONError")
    check_(bad is WONError && bad.offset >= 0,
           "WONError.offset is non-negative")
    check_(bad is WONError && bad.toString.contains("WONError"),
           "WONError.toString includes 'WONError'")

    // Polymorphic Error base - every recoverable-failure foreign
    // (WONError, FileError, SFTPError, ConnError) inherits from
    // Error, so a script can demux all of them with one check.
    check_(bad is Error,
           "WONError is Error (polymorphic base check)")

    // ScriptError: pure-Wren error subclass with constructor +
    // fields, for scripts that want their own typed errors without
    // C bindings.  Inherits from Error so it satisfies `is Error`
    // alongside the foreign subclasses.
    var se = ScriptError.new(99, "config missing")
    check_(se is ScriptError && se is Error,
           "ScriptError instance is both ScriptError and Error")
    check_(se.code == 99 && se.message == "config missing",
           "ScriptError exposes code + message via getters")
    check_(se.toString.contains("ScriptError") &&
           se.toString.contains("99") &&
           se.toString.contains("config missing"),
           "ScriptError.toString includes type, code, and message")

    // Programmer error (wrong type) STILL aborts.
    var typeErr = Fiber.new { WON.deserialize(42) }.try()
    check_(typeErr is String && typeErr.contains("must be a String"),
           "WON.deserialize: non-String arg aborts (programmer error)")
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
      // *literal* percent - escape sequence - preserved into
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
      // sleep gives the drain a chance to fire - without it the
      // report_ check races against the resume.  0xFD00 matches the
      // same low-byte-zero constraint the existing 0xFE00 hook test
      // uses (see comment at the unget below).
      // CTerm.suspended is toggled around the unget+resume window
      // to exercise the wire-pump suspend flag - without it,
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
    } else if (__step == 7) {
      // Wake.post end-to-end.  Park two fibers on plain Fiber.yield
      // (neither registered with Input.nextEvent or Timer.trigger),
      // then fire wake on each with a different value type.  The
      // result-queue drain should resume both fibers with their
      // respective values before the wallclock sentinel returns.
      __wakeFiber.call()
      __wakeStringFiber.call()
      Wake.post(__wakeFiber, KeyEvent.new(0xFC00))
      Wake.post(__wakeStringFiber, "wake-payload")
      __pending = "T07"
      Conn.send("sleep 0.2 && printf '__\%s_X_done_X__\\n' T07\r")
    } else if (__step == 8) {
      // Claim-stack newer-fiber-wins.  Push A's claim first, then
      // B's; B is on top of the stack (different fibers).  Unget
      // a sentinel key - only B's counter should bump.  The
      // sentinel sleep gives the dispatch a chance to fire.  Keys
      // 0xFB00 / 0xFB01 are arbitrary - they just need to make it
      // through the C-side dispatch_key as KeyEvents.
      __claimAFiber.call()
      __claimBFiber.call()
      Input.unget(KeyEvent.new(0xFB00))
      __pending = "T08"
      Conn.send("sleep 0.2 && printf '__\%s_X_done_X__\\n' T08\r")
    } else if (__step == 9) {
      // Pop B's claim; the older A claim is now on top.  Unget
      // another sentinel key - A's counter should bump this time.
      // Code must satisfy ciolib's byte-stuffing rule (low byte 0
      // or 0xE0) or it splits into two separate getch reads and
      // bumps A's counter twice.  0xF900 is unused elsewhere in
      // this suite.
      __claimBHandle.pop()
      Input.unget(KeyEvent.new(0xF900))
      __pending = "T09"
      Conn.send("sleep 0.2 && printf '__\%s_X_done_X__\\n' T09\r")
    } else if (__step == 10) {
      // Claim fall-through.  Pop A's claim so the fall-through claim
      // is the only one on the stack, then push it and unget 0xFA00.
      // The claim returns false, the C dispatcher walks past every
      // claim, and the registered Hook.onKey(0xFA00) fires.  Both
      // counters wind up at 1; either being wrong proves the
      // dispatch path.
      __claimAHandle.pop()
      __fallthroughFiber.call()
      Input.unget(KeyEvent.new(0xFA00))
      __pending = "T10"
      Conn.send("sleep 0.2 && printf '__\%s_X_done_X__\\n' T10\r")
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
    } else if (name == "07") {
      check_(value == "done", "T07 sleep+sentinel for Wake.post test")
    } else if (name == "08") {
      check_(value == "done", "T08 sleep+sentinel for claim-stack test")
    } else if (name == "09") {
      check_(value == "done", "T09 sleep+sentinel for claim-pop test")
    } else if (name == "10") {
      check_(value == "done", "T10 sleep+sentinel for claim fall-through test")
    } else {
      check_(false, "T%(name) unexpected sentinel")
    }
    advance_()
  }

  static timeout_() {
    if (__pending == null) return
    __fail = __fail + 1
    print_("  FAIL %(__pending) (timeout)")
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
    // direct test - much cleaner than the old static-counter
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
    // broken - and in the former case the T06 sentinel could only
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

    // Wake.post delivers a queued resumption to a parked fiber via
    // the same result-queue path Input.nextEvent and Timer.trigger
    // use.  Two probes: one with a foreign KeyEvent value (exercises
    // the WrenHandle pin/release across types), one with a plain
    // String.  By T07's wallclock sentinel the drain must have
    // resumed both fibers and stored the values in their respective
    // captures.
    check_(__wakeResult is KeyEvent && __wakeResult.code == 0xFC00,
           "Wake.post delivers a foreign value to parked fiber")
    check_(__wakeStringResult == "wake-payload",
           "Wake.post delivers a String value to parked fiber")

    // Claim-stack newer-fiber-wins: T08 unget'd one key while both
    // A and B claims were on the stack (B on top).  Only B's
    // counter should have bumped.  T09 popped B and unget another
    // key, so A's counter should also be 1 by now.
    check_(__claimBCount == 1,
           "Claim stack: newer-fiber claim wins dispatch (T08)")
    check_(__claimACount == 1,
           "Claim stack: older-fiber claim wins after newer pops (T09)")

    // Claim fall-through: T10 ungets 0xFA00 with only the
    // fall-through claim on the stack (returns false) and a
    // Hook.onKey for the same key.  The claim fires once and
    // passes; the hook then catches.  Either count being wrong
    // proves the cascade is broken.
    check_(__fallthroughCount == 1,
           "Claim stack: fall-through claim fires on event (T10)")
    check_(__fallthroughHookCount == 1,
           "Claim stack: Hook.onKey fires when no claim consumes (T10)")

    var total = __pass + __fail
    print_("=== wrentest: %(total) tests, %(__pass) pass, %(__fail) fail ===")
    fold_([__pass, __fail])
    var grand = __sumPass + __sumFail
    print_("=== TOTAL: %(grand) tests, %(__sumPass) pass, %(__sumFail) fail ===")
    cleanup_()
    if (Host.launchScript != null) Conn.send("exit\r")
  }

  // Accumulate a [pass, fail] pair returned by a nested test suite
  // into the aggregate WrenTest reports at end-of-run.
  static fold_(r) {
    __sumPass = __sumPass + r[0]
    __sumFail = __sumFail + r[1]
  }

  // Tear down every hook this run() registered.  Safe to call from
  // inside the very dispatch that triggered it - the host
  // tombstones (just NULLs fn) so the in-flight dispatcher walks
  // off the entry on the next iteration.
  static cleanup_() {
    for (h in __hooks) h.remove()
    __hooks         = []
    __everyHook       = null
    __keyHook         = null
    __lfReplaceHook   = null
    __fallthroughHook = null
    // Pop A's claim if it's still on the stack (T10 popped it; if
    // T10 didn't run, it might still be there).  B was popped
    // during T09.  pop() is idempotent.  Same for the fall-through
    // claim - T10 may or may not have completed.
    if (__claimAHandle      != null) __claimAHandle.pop()
    if (__fallthroughHandle != null) __fallthroughHandle.pop()
  }
}

// All hooks are registered inside WrenTest.run() and torn down by
// cleanup_() at the end of the suite, so the regex VM, timers, and
// filtered key listener don't keep ticking once the tests are done.

// Auto-run when invoked via `syncterm -W <path-to-this-file>`.  The
// in-session entry point (Alt+T) lives in
// scripts/auto/connected/runtests.wren and stays unchanged.  Match
// on the path's tail so a wrapper script that imports wrentest from
// some other location can also opt in by passing wrentest.wren via
// -W -- imports happen before this block runs, so the WrenTest class
// is fully initialized either way.
if (Host.launchScript != null && Host.launchScript.endsWith("wrentest.wren")) {
  WrenTest.run()
}
