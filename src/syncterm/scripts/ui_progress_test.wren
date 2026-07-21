// Self-tests for ui_progress.

import "ui_widget"   for Rect
import "ui_progress" for ProgressBar, ProgressText

class UiProgressTest {
  static run() {
    __pass = 0
    __fail = 0
    System.print("=== ui_progress self-test starting ===")

    testDefaults_()
    testNotFocusable_()
    testSetUpdatesBoth_()
    testZeroPercentEmpty_()
    testFullPercentFilled_()
    testHalfPercentHalfFilled_()
    testZeroTotalEmpty_()
    testNullTotalEmpty_()
    testCurrentClampedHigh_()
    testCurrentClampedLow_()
    testPercentOverlayCentered_()
    testPercentOverlayHidden_()
    testFractionalRoundDown_()
    testTallBarFillsAllRows_()
    testTinyBoundsTruncatesOverlay_()
    testTextRowsShareLeftEdge_()

    var total = __pass + __fail
    System.print("=== ui_progress: %(total) tests, %(__pass) pass, %(__fail) fail ===")
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
    var p = ProgressBar.new()
    check_(p.current == 0 && p.total == 0 && p.showPercent == true,
           "defaults: current=0, total=0, showPercent=true")
  }

  static testNotFocusable_() {
    var p = ProgressBar.new()
    check_(p.focusable == false, "not focusable")
  }

  static testSetUpdatesBoth_() {
    var p = ProgressBar.new()
    p.set(7, 10)
    check_(p.current == 7 && p.total == 10,
           "set(cur, total) updates both")
  }

  // 0%: empty everywhere; cell 0 is space (no fill); first cell of the
  // overlay (centered "0\%") shows '0'.  Light-shade byte (0xB1) must
  // not appear anywhere.
  static testZeroPercentEmpty_() {
    var p = ProgressBar.new()
    p.bounds = Rect.new(1, 1, 20, 1)
    p.set(0, 100)
    var sf = p.draw()
    var anyFilled = false
    for (i in 0...20) {
      if (sf.cellAt(i, 0).chByte == 0xB1) anyFilled = true
    }
    check_(!anyFilled, "0\% does not paint any 0xB1 cells")
  }

  static testFullPercentFilled_() {
    var p = ProgressBar.new()
    p.bounds = Rect.new(1, 1, 20, 1)
    p.set(100, 100)
    var sf = p.draw()
    // Cells 0 and 19 must be 0xB1 — the overlay only occupies the
    // middle 4 cells (label "100\%" = "100%"), so the edges remain
    // light-shade.
    check_(sf.cellAt(0, 0).chByte == 0xB1 && sf.cellAt(19, 0).chByte == 0xB1,
           "100\% paints fill at the edges")
  }

  static testHalfPercentHalfFilled_() {
    var p = ProgressBar.new()
    p.bounds      = Rect.new(1, 1, 20, 1)
    p.showPercent = false       // overlay would clobber cells 8..10
    p.set(50, 100)
    var sf = p.draw()
    // floor(20 * 50 / 100) = 10 filled cells (indices 0..9).
    var c9  = sf.cellAt(9, 0).chByte
    var c10 = sf.cellAt(10, 0).chByte
    check_(c9 == 0xB1 && c10 == 0x20,
           "50\% fills cells 0..9, leaves cell 10 empty (chByte 0xB1 / 0x20)")
  }

  static testZeroTotalEmpty_() {
    var p = ProgressBar.new()
    p.bounds = Rect.new(1, 1, 20, 1)
    p.set(50, 0)
    var sf = p.draw()
    var anyFilled = false
    for (i in 0...20) {
      if (sf.cellAt(i, 0).chByte == 0xB1) anyFilled = true
    }
    check_(!anyFilled, "total=0 paints no fill (no div-by-zero, no overlay)")
  }

  static testNullTotalEmpty_() {
    var p = ProgressBar.new()
    p.bounds = Rect.new(1, 1, 20, 1)
    p.current = 5
    p.total   = null
    var sf = p.draw()
    var anyFilled = false
    for (i in 0...20) {
      if (sf.cellAt(i, 0).chByte == 0xB1) anyFilled = true
    }
    check_(!anyFilled, "total=null degenerates to empty")
  }

  static testCurrentClampedHigh_() {
    var p = ProgressBar.new()
    p.bounds = Rect.new(1, 1, 10, 1)
    p.set(200, 100)
    var sf = p.draw()
    // current > total clamps; expect all 10 cells filled (excluding
    // the overlay).  Cells 0 and 9 should be 0xB1.
    check_(sf.cellAt(0, 0).chByte == 0xB1 && sf.cellAt(9, 0).chByte == 0xB1,
           "current > total clamped (full bar)")
  }

  static testCurrentClampedLow_() {
    var p = ProgressBar.new()
    p.bounds = Rect.new(1, 1, 10, 1)
    p.set(-5, 100)
    var sf = p.draw()
    var anyFilled = false
    for (i in 0...10) {
      if (sf.cellAt(i, 0).chByte == 0xB1) anyFilled = true
    }
    check_(!anyFilled, "current < 0 clamped (empty bar)")
  }

  // 50/100 over a width of 20 has filled=10 and label "50\%" (3 chars).
  // Centered: cx = (20 - 3)/2 = 8 -> cells 8, 9, 10 = '5', '0', '%'.
  static testPercentOverlayCentered_() {
    var p = ProgressBar.new()
    p.bounds = Rect.new(1, 1, 20, 1)
    p.set(50, 100)
    var sf = p.draw()
    check_(sf.cellAt(8, 0).ch == "5" &&
           sf.cellAt(9, 0).ch == "0" &&
           sf.cellAt(10, 0).ch == "\%",
           "overlay '50\%' centered at cells 8..10")
  }

  static testPercentOverlayHidden_() {
    var p = ProgressBar.new()
    p.bounds = Rect.new(1, 1, 20, 1)
    p.showPercent = false
    p.set(50, 100)
    var sf = p.draw()
    // Without overlay, cells 8..10 stay light-shade (within filled
    // range) — verify cell 8 is still 0xB1.
    check_(sf.cellAt(8, 0).chByte == 0xB1,
           "showPercent=false suppresses the overlay")
  }

  // Floor rounding: 1/3 of 10 = floor(10/3) = 3 cells, not 4.
  static testFractionalRoundDown_() {
    var p = ProgressBar.new()
    p.bounds = Rect.new(1, 1, 10, 1)
    p.showPercent = false
    p.set(1, 3)
    var sf = p.draw()
    check_(sf.cellAt(2, 0).chByte == 0xB1 &&
           sf.cellAt(3, 0).chByte == 0x20,
           "1/3 over width 10 fills 3 cells (floor)")
  }

  static testTallBarFillsAllRows_() {
    var p = ProgressBar.new()
    p.bounds = Rect.new(1, 1, 10, 3)
    p.showPercent = false
    p.set(100, 100)
    var sf = p.draw()
    check_(sf.cellAt(0, 0).chByte == 0xB1 &&
           sf.cellAt(0, 2).chByte == 0xB1,
           "3-row bar: top and bottom rows both filled")
  }

  // Bounds narrower than the overlay label: it gets capped at the
  // available width without crashing.  No specific assertion about the
  // visible chars — the contract is just "doesn't blow up."
  static testTinyBoundsTruncatesOverlay_() {
    var p = ProgressBar.new()
    p.bounds = Rect.new(1, 1, 2, 1)
    p.set(50, 100)
    var sf = p.draw()
    check_(sf != null,
           "2-cell bounds with overlay does not crash")
  }

  static testTextRowsShareLeftEdge_() {
    var body = ProgressText.new()
    body.bounds = Rect.new(1, 1, 40, 2)
    body.lines = [
      "Alpha               : ok",
      "Longer Name         : waiting"
    ]
    var sf = body.draw()
    check_(sf.cellAt(0, 0).ch == " " &&
           sf.cellAt(1, 0).ch == "A" &&
           sf.cellAt(1, 1).ch == "L" &&
           sf.cellAt(21, 0).ch == ":" &&
           sf.cellAt(21, 1).ch == ":",
           "text rows preserve one left edge and name column")
  }
}
