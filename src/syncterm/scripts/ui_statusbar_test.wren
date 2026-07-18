// Self-tests for ui_statusbar.

import "ui_widget"    for Rect
import "ui_statusbar" for StatusBar

class UiStatusbarTest {
  static run() {
    __pass = 0
    __fail = 0
    System.print("=== ui_statusbar self-test starting ===")

    testDefaults_()
    testNotFocusable_()
    testTextPainted_()
    testTextOverflowTrimmed_()
    testEmptyTextBlankFilled_()
    testSegmentsLeftAlign_()
    testSegmentsRightAlign_()
    testSegmentsCenterAlign_()
    testSegmentsTakePrecedenceOverText_()

    var total = __pass + __fail
    System.print("=== ui_statusbar: %(total) tests, %(__pass) pass, %(__fail) fail ===")
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

  static testDefaults_() {
    var s = StatusBar.new()
    check_(s.text == "" && s.segments == null,
           "StatusBar: defaults (empty text, no segments)")
  }

  static testNotFocusable_() {
    var s = StatusBar.new()
    check_(!s.focusable && !s.activitySensitive,
           "StatusBar: not focusable or activity-sensitive")
  }

  static testTextPainted_() {
    var s = StatusBar.new()
    s.bounds = Rect.new(1, 24, 20, 1)
    s.text   = "OK"
    var sf = s.draw()
    check_(sf.cellAt(0, 0).ch == "O" && sf.cellAt(1, 0).ch == "K",
           "StatusBar.draw: text painted at left")
  }

  static testTextOverflowTrimmed_() {
    var s = StatusBar.new()
    s.bounds = Rect.new(1, 24, 5, 1)
    s.text   = "abcdefghij"
    var sf = s.draw()
    // First 5 cells get the truncated text, no panic.
    check_(sf.cellAt(0, 0).ch == "a" && sf.cellAt(4, 0).ch == "e",
           "StatusBar.draw: text wider than bounds is truncated")
  }

  static testEmptyTextBlankFilled_() {
    var s = StatusBar.new()
    s.bounds = Rect.new(1, 24, 10, 1)
    var sf = s.draw()
    check_(sf.cellAt(0, 0).ch == " " && sf.cellAt(9, 0).ch == " ",
           "StatusBar.draw: empty text blank-fills the row")
  }

  static testSegmentsLeftAlign_() {
    var s = StatusBar.new()
    s.bounds   = Rect.new(1, 24, 30, 1)
    s.segments = [["F1", "left"], ["F2", "left"]]
    var sf = s.draw()
    // "F1" at 0, then 2-cell gap (cols 2,3), then "F2" at 4
    check_(sf.cellAt(0, 0).ch == "F" && sf.cellAt(1, 0).ch == "1" &&
           sf.cellAt(4, 0).ch == "F" && sf.cellAt(5, 0).ch == "2",
           "StatusBar.draw: left segments stacked with 2-cell gap")
  }

  static testSegmentsRightAlign_() {
    var s = StatusBar.new()
    s.bounds   = Rect.new(1, 24, 10, 1)
    s.segments = [["09:42", "right"]]
    var sf = s.draw()
    check_(sf.cellAt(5, 0).ch == "0" && sf.cellAt(9, 0).ch == "2",
           "StatusBar.draw: right segment flushed to right edge")
  }

  static testSegmentsCenterAlign_() {
    var s = StatusBar.new()
    s.bounds   = Rect.new(1, 24, 10, 1)
    s.segments = [["AB", "center"]]
    var sf = s.draw()
    // Center: room=10, n=2, cx = 0 + (10-2)/2 = 4
    check_(sf.cellAt(4, 0).ch == "A" && sf.cellAt(5, 0).ch == "B",
           "StatusBar.draw: center segment placed centred")
  }

  static testSegmentsTakePrecedenceOverText_() {
    var s = StatusBar.new()
    s.bounds   = Rect.new(1, 24, 10, 1)
    s.text     = "TEXT"
    s.segments = [["SEG", "left"]]
    var sf = s.draw()
    check_(sf.cellAt(0, 0).ch == "S" && sf.cellAt(2, 0).ch == "G",
           "StatusBar.draw: segments override text")
  }
}
