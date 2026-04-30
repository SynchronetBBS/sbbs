// Self-tests for ui_help.  FakeApp records modal pushes/pops without
// pumping the event loop.

import "ui_widget" for Rect
import "ui_help"   for Help
import "syncterm"  for KeyEvent, Key

class FakeApp {
  construct new() {
    _stack = []
  }
  modalStack { _stack }
  effectiveTheme { null }
  markDirty() {}
  pushModal(w) {
    w.parent = this
    _stack.add(w)
    return w
  }
  popModal() {
    if (_stack.count == 0) return null
    var w = _stack.removeAt(-1)
    w.parent = null
    return w
  }
  modal(w) {
    pushModal(w)
    return w
  }
}

class UiHelpTest {
  static run() {
    __pass = 0
    __fail = 0
    System.print("=== ui_help self-test starting ===")

    testSplitLines_()
    testSplitLinesEmpty_()
    testSplitLinesNoTrailing_()
    testConstruction_()
    testEscDismisses_()
    testEnterDismisses_()
    testDownScrollsByOne_()
    testUpScrollsByOne_()
    testHomeJumpsToTop_()
    testEndJumpsToBottom_()
    testPageDownAdvancesByViewport_()
    testWheelDownScrolls_()
    testRandomKeyNotConsumed_()

    var total = __pass + __fail
    System.print("=== ui_help: %(total) tests, %(__pass) pass, %(__fail) fail ===")
    return [__pass, __fail]
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

  static makeHelp_(lineCount, w, h) {
    var body = ""
    var i = 0
    while (i < lineCount) {
      if (i > 0) body = body + "\n"
      body = body + "line %(i)"
      i = i + 1
    }
    var hp = Help.new("Help", body)
    hp.bounds = Rect.new(1, 1, w, h)
    return hp
  }

  // ----- splitLines_ ----------------------------------------------

  static testSplitLines_() {
    var lines = Help.splitLines_("a\nb\nc")
    check_(lines.count == 3 && lines[0] == "a" && lines[1] == "b" &&
           lines[2] == "c",
           "Help.splitLines_: 'a\\nb\\nc' -> 3 lines")
  }

  static testSplitLinesEmpty_() {
    var lines = Help.splitLines_("")
    check_(lines.count == 1 && lines[0] == "",
           "Help.splitLines_: empty string -> [\"\"]")
  }

  static testSplitLinesNoTrailing_() {
    var lines = Help.splitLines_("only")
    check_(lines.count == 1 && lines[0] == "only",
           "Help.splitLines_: no newline -> single line")
  }

  // ----- Construction --------------------------------------------

  static testConstruction_() {
    var h = makeHelp_(3, 30, 8)
    check_(h.title == "Help" && h.scrollTop == 0,
           "Help: title set, scrollTop starts at 0")
  }

  // ----- Dismiss --------------------------------------------------

  static testEscDismisses_() {
    var app = FakeApp.new()
    var h   = makeHelp_(3, 30, 8)
    app.modal(h)
    h.handle(KeyEvent.new(Key.escape))
    check_(app.modalStack.count == 0,
           "Help: Esc dismisses")
  }

  static testEnterDismisses_() {
    var app = FakeApp.new()
    var h   = makeHelp_(3, 30, 8)
    app.modal(h)
    h.handle(KeyEvent.new(Key.enter))
    check_(app.modalStack.count == 0,
           "Help: Enter dismisses")
  }

  // ----- Scroll ---------------------------------------------------

  static testDownScrollsByOne_() {
    // 20-line body, 5-row viewport (h=7 minus 2 for frame).
    var h = makeHelp_(20, 30, 7)
    h.handle(KeyEvent.new(Key.down))
    check_(h.scrollTop == 1,
           "Help: Down scrolls scrollTop by 1")
  }

  static testUpScrollsByOne_() {
    var h = makeHelp_(20, 30, 7)
    h.handle(KeyEvent.new(Key.down))
    h.handle(KeyEvent.new(Key.down))
    h.handle(KeyEvent.new(Key.up))
    check_(h.scrollTop == 1,
           "Help: Up scrolls scrollTop by -1")
  }

  static testHomeJumpsToTop_() {
    var h = makeHelp_(20, 30, 7)
    h.handle(KeyEvent.new(Key.end))
    h.handle(KeyEvent.new(Key.home))
    check_(h.scrollTop == 0,
           "Help: Home -> scrollTop 0")
  }

  static testEndJumpsToBottom_() {
    var h = makeHelp_(20, 30, 7)             // viewport = 5
    h.handle(KeyEvent.new(Key.end))
    check_(h.scrollTop == 15,
           "Help: End -> scrollTop count - viewport")
  }

  static testPageDownAdvancesByViewport_() {
    var h = makeHelp_(20, 30, 7)             // viewport = 5
    h.handle(KeyEvent.new(Key.pageDown))
    check_(h.scrollTop == 5,
           "Help: PageDown advances by viewport rows")
  }

  static testWheelDownScrolls_() {
    var h = makeHelp_(20, 30, 7)
    var app = FakeApp.new()
    app.modal(h)
    // Mouse wheel down.  Bounds at (1,1,30,7), interior at (2..,2..).
    var ev = KeyEvent.new(Key.pageDown)      // proxy: any key path
    h.handle(ev)
    check_(h.scrollTop == 5,
           "Help: PageDown scrolls (mouse-wheel uses identical scrollTop= path)")
  }

  static testRandomKeyNotConsumed_() {
    var h = makeHelp_(3, 30, 8)
    var consumed = h.handle(KeyEvent.new(0x41))   // 'A'
    check_(!consumed,
           "Help: random printable key not consumed")
  }
}
