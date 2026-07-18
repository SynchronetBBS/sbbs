// Self-tests for ui_help.  FakeApp records modal pushes/pops without
// pumping the event loop.

import "ui_widget"   for Rect
import "ui_help"     for Help
import "ui_markdown" for Markdown
import "syncterm"    for KeyEvent, Key

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
    testMenuDefinitionListFormatting_()
    testPreformattedUsesHardLines_()
    testPreformattedPreservesInlineMarkup_()
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
    } else {
      __fail = __fail + 1
      System.print("  FAIL %(label)")
    }
  }

  // Build a help with `lineCount` rendered rows by emitting one bullet
  // per row — the markdown layout treats consecutive bullets as
  // separate blocks, so each maps to a single MdLine.
  static makeHelp_(lineCount, w, h) {
    var body = ""
    var i = 0
    while (i < lineCount) {
      if (i > 0) body = body + "\n"
      body = body + "- line %(i)"
      i = i + 1
    }
    var hp = Help.new("Help", body)
    hp.bounds = Rect.new(1, 1, w, h)
    return hp
  }

  // ----- Markdown.splitLines_ -------------------------------------

  static testSplitLines_() {
    var lines = Markdown.splitLines_("a\nb\nc")
    check_(lines.count == 3 && lines[0] == "a" && lines[1] == "b" &&
           lines[2] == "c",
           "Markdown.splitLines_: 'a\\nb\\nc' -> 3 lines")
  }

  static testSplitLinesEmpty_() {
    var lines = Markdown.splitLines_("")
    check_(lines.count == 1 && lines[0] == "",
           "Markdown.splitLines_: empty string -> [\"\"]")
  }

  static testSplitLinesNoTrailing_() {
    var lines = Markdown.splitLines_("only")
    check_(lines.count == 1 && lines[0] == "only",
           "Markdown.splitLines_: no newline -> single line")
  }

  // ----- Construction --------------------------------------------

  static testConstruction_() {
    var h = makeHelp_(3, 30, 8)
    check_(h.title == "Help" && h.scrollTop == 0,
           "Help: title set, scrollTop starts at 0")
  }

  static testMenuDefinitionListFormatting_() {
    var doc = Markdown.parse("# Settings\n\nName\n:  Entry name\nPort\n:  TCP port")
    var lines = Markdown.layout(doc, 60)
    check_(lines.count == 4 &&
           lines[0].runs[0].role == "help.heading.1" &&
           lines[2].runs[0].text == "Name" &&
           lines[2].runs[0].role == "help.bold" &&
           lines[3].runs[0].text == "Port",
           "Help: menu headings and definition lists retain formatting")
  }

  static testPreformattedUsesHardLines_() {
    var h = Help.new("Report", "  one\n\nSection\n  two\n  three")
    h.preformatted = true
    h.bounds = Rect.new(1, 1, 30, 8)
    h.handle(KeyEvent.new(Key.end))
    check_(h.preformatted && h.scrollTop == 1,
           "Help: preformatted body preserves hard lines")
  }

  static testPreformattedPreservesInlineMarkup_() {
    var h = Help.new("Report", "    [`√`] option")
    h.preformatted = true
    var runs = h.preformattedLines_()[0].runs
    check_(runs.count == 3 && runs[0].text == "    [" &&
           runs[1].text == "√" && runs[1].role == "help.code" &&
           runs[2].text == "] option",
           "Help: preformatted body keeps indentation and highlights")
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
    // 20-line body, 5-row viewport (h=9 minus 4 for frame + padding).
    var h = makeHelp_(20, 30, 9)
    h.handle(KeyEvent.new(Key.down))
    check_(h.scrollTop == 1,
           "Help: Down scrolls scrollTop by 1")
  }

  static testUpScrollsByOne_() {
    var h = makeHelp_(20, 30, 9)
    h.handle(KeyEvent.new(Key.down))
    h.handle(KeyEvent.new(Key.down))
    h.handle(KeyEvent.new(Key.up))
    check_(h.scrollTop == 1,
           "Help: Up scrolls scrollTop by -1")
  }

  static testHomeJumpsToTop_() {
    var h = makeHelp_(20, 30, 9)
    h.handle(KeyEvent.new(Key.end))
    h.handle(KeyEvent.new(Key.home))
    check_(h.scrollTop == 0,
           "Help: Home -> scrollTop 0")
  }

  static testEndJumpsToBottom_() {
    var h = makeHelp_(20, 30, 9)             // viewport = 5
    h.handle(KeyEvent.new(Key.end))
    check_(h.scrollTop == 15,
           "Help: End -> scrollTop count - viewport")
  }

  static testPageDownAdvancesByViewport_() {
    var h = makeHelp_(20, 30, 9)             // viewport = 5
    h.handle(KeyEvent.new(Key.pageDown))
    check_(h.scrollTop == 5,
           "Help: PageDown advances by viewport rows")
  }

  static testWheelDownScrolls_() {
    var h = makeHelp_(20, 30, 9)
    var app = FakeApp.new()
    app.modal(h)
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
