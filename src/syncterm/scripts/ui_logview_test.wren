// Self-tests for ui_logview.

import "ui_widget"  for Rect
import "ui_logview" for LogView
import "syncterm"   for Key, KeyEvent, Mouse, MouseEvent

class UiLogviewTest {
  static run() {
    __pass = 0
    __fail = 0
    System.print("=== ui_logview self-test starting ===")

    testDefaults_()
    testFocusable_()
    testAppendCount_()
    testCapacityDropsOldest_()
    testCapacityShrink_()
    testClear_()
    testLiveTailShowsLastLines_()
    testLiveTailFewerThanViewport_()
    testTextWrapsAtWidth_()
    testSeverityStyles_()
    testPgUpEntersScrollMode_()
    testPgDnReturnsToLive_()
    testHomeJumpsToOldest_()
    testEndReturnsToLive_()
    testAppendInScrollHoldsAnchor_()
    testCapacityShiftKeepsScrollAligned_()
    testNoScrollbarWhenContentFits_()
    testScrollbarAppearsWhenOverflowing_()
    testScrollbarWheelMovesPage_()

    var total = __pass + __fail
    System.print("=== ui_logview: %(total) tests, %(__pass) pass, %(__fail) fail ===")
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

  // Construct a key event for handle() — use synthetic events since
  // we don't go through the input layer.
  static keyEvent_(code) { KeyEvent.new(code) }

  static testDefaults_() {
    var lv = LogView.new()
    check_(lv.count == 0 && lv.scrollTop == null && lv.capacity == 256,
           "defaults: empty ring, live mode, capacity 256")
  }

  static testFocusable_() {
    var lv = LogView.new()
    check_(lv.focusable == true, "focusable (so PgUp/PgDn route here)")
  }

  static testAppendCount_() {
    var lv = LogView.new()
    lv.append(LogView.LEVEL_INFO,    "one")
    lv.append(LogView.LEVEL_NOTICE,  "two")
    lv.append(LogView.LEVEL_WARNING, "three")
    check_(lv.count == 3, "append increments count")
  }

  static testCapacityDropsOldest_() {
    var lv = LogView.new()
    lv.capacity = 3
    lv.append(LogView.LEVEL_INFO, "a")
    lv.append(LogView.LEVEL_INFO, "b")
    lv.append(LogView.LEVEL_INFO, "c")
    lv.append(LogView.LEVEL_INFO, "d")
    check_(lv.count == 3, "capacity caps the ring at 3 entries")
  }

  static testCapacityShrink_() {
    var lv = LogView.new()
    for (i in 1..10) {
      lv.append(LogView.LEVEL_INFO, "line %(i)")
    }
    lv.capacity = 4
    check_(lv.count == 4, "shrinking capacity drops oldest entries")
  }

  static testClear_() {
    var lv = LogView.new()
    lv.append(LogView.LEVEL_INFO, "x")
    lv.append(LogView.LEVEL_INFO, "y")
    lv.clear()
    check_(lv.count == 0 && lv.scrollTop == null,
           "clear empties ring, returns to live")
  }

  // Viewport 3 rows over a ring of 5 entries: live mode shows the
  // last 3 (entries 2..4).  Scrollbar is visible because 5 > 3, so
  // content ends before the separator at column 18 and scrollbar at 19.
  static testLiveTailShowsLastLines_() {
    var lv = LogView.new()
    lv.bounds = Rect.new(1, 1, 20, 3)
    lv.append(LogView.LEVEL_INFO, "alpha")
    lv.append(LogView.LEVEL_INFO, "beta")
    lv.append(LogView.LEVEL_INFO, "gamma")
    lv.append(LogView.LEVEL_INFO, "delta")
    lv.append(LogView.LEVEL_INFO, "epsilon")
    var sf = lv.draw()
    check_(sf.cellAt(0, 0).ch == "g" && sf.cellAt(0, 1).ch == "d" &&
           sf.cellAt(0, 2).ch == "e",
           "live tail: last 3 of 5 entries paint top-to-bottom (col 0)")
  }

  // Viewport bigger than data: paint at top, leave trailing rows blank.
  static testLiveTailFewerThanViewport_() {
    var lv = LogView.new()
    lv.bounds = Rect.new(1, 1, 20, 5)
    lv.append(LogView.LEVEL_INFO, "only")
    var sf = lv.draw()
    check_(sf.cellAt(0, 0).ch == "o" && sf.cellAt(0, 1).chByte == 0x20,
           "fewer entries than viewport: row 0 has data, row 1 blank")
  }

  static testTextWrapsAtWidth_() {
    var lv = LogView.new()
    lv.bounds = Rect.new(1, 1, 7, 2)
    lv.append(LogView.LEVEL_INFO, "abcdefghij")
    var sf = lv.draw()
    check_(sf.cellAt(0, 0).ch == "a" && sf.cellAt(6, 0).ch == "g" &&
           sf.cellAt(0, 1).ch == "h" && sf.cellAt(2, 1).ch == "j",
           "text wider than bounds wraps to the next row")
  }

  // Each severity bin paints with its own legacyAttr; spot-check the
  // first cell of each.
  static testSeverityStyles_() {
    var lv = LogView.new()
    lv.bounds = Rect.new(1, 1, 10, 4)
    lv.append(LogView.LEVEL_INFO,    "i")
    lv.append(LogView.LEVEL_NOTICE,  "n")
    lv.append(LogView.LEVEL_WARNING, "w")
    lv.append(LogView.LEVEL_ERR,     "e")
    var sf = lv.draw()
    check_(sf.cellAt(0, 0).legacyAttr == 0x1F &&  // info white
           sf.cellAt(0, 1).legacyAttr == 0x1E &&  // notice yellow
           sf.cellAt(0, 2).legacyAttr == 0x1D &&  // warning lt-magenta
           sf.cellAt(0, 3).legacyAttr == 0x1C,    // error lt-red
           "severities map to white / yellow / lt-magenta / lt-red")
  }

  // PgUp on a 3-row viewport with 6 entries: scrollTop becomes
  // (current_bottom_anchor) - viewport = 3 - 3 = 0.
  static testPgUpEntersScrollMode_() {
    var lv = LogView.new()
    lv.bounds = Rect.new(1, 1, 20, 3)
    for (i in 1..6) {
      lv.append(LogView.LEVEL_INFO, "line %(i)")
    }
    lv.handle(keyEvent_(Key.pageUp))
    check_(lv.scrollTop == 0,
           "PgUp from live anchors at top of buffer (6 entries, viewport 3)")
  }

  // PgDn from anchored scroll past the end snaps back to live.
  static testPgDnReturnsToLive_() {
    var lv = LogView.new()
    lv.bounds = Rect.new(1, 1, 20, 3)
    for (i in 1..6) {
      lv.append(LogView.LEVEL_INFO, "line %(i)")
    }
    lv.handle(keyEvent_(Key.pageUp))
    lv.handle(keyEvent_(Key.pageDown))
    check_(lv.scrollTop == null,
           "PgDn past end returns to live mode")
  }

  static testHomeJumpsToOldest_() {
    var lv = LogView.new()
    lv.bounds = Rect.new(1, 1, 20, 3)
    for (i in 1..10) {
      lv.append(LogView.LEVEL_INFO, "line %(i)")
    }
    lv.handle(keyEvent_(Key.home))
    check_(lv.scrollTop == 0,
           "Home pins viewport at the start of the ring")
  }

  static testEndReturnsToLive_() {
    var lv = LogView.new()
    lv.bounds = Rect.new(1, 1, 20, 3)
    for (i in 1..10) {
      lv.append(LogView.LEVEL_INFO, "line %(i)")
    }
    lv.handle(keyEvent_(Key.home))
    lv.handle(keyEvent_(Key.end))
    check_(lv.scrollTop == null,
           "End returns to live mode")
  }

  // Appending while anchored in scroll mode does not move the anchor.
  static testAppendInScrollHoldsAnchor_() {
    var lv = LogView.new()
    lv.bounds = Rect.new(1, 1, 20, 3)
    for (i in 1..6) {
      lv.append(LogView.LEVEL_INFO, "line %(i)")
    }
    lv.handle(keyEvent_(Key.home))
    var before = lv.scrollTop
    lv.append(LogView.LEVEL_INFO, "later")
    check_(lv.scrollTop == before,
           "append in scroll mode keeps anchor (no auto-tail)")
  }

  // No scrollbar painted when count fits in the viewport.  The last
  // column should hold content or default-fill space, not an arrow cap.
  static testNoScrollbarWhenContentFits_() {
    var lv = LogView.new()
    lv.bounds = Rect.new(1, 1, 10, 4)
    lv.append(LogView.LEVEL_INFO, "abc")
    lv.append(LogView.LEVEL_INFO, "def")
    var sf = lv.draw()
    var rightCh = sf.cellAt(9, 0).chByte
    // Whatever it is, it should not be one of the four scrollbar
    // glyphs (▲▼█░ → CP437 0x1E 0x1F 0xDB 0xB0).
    var isSb = rightCh == 0x1E || rightCh == 0x1F ||
               rightCh == 0xDB || rightCh == 0xB0
    check_(!isSb,
           "no scrollbar drawn when count <= viewport")
  }

  // Scrollbar appears (top-row up-arrow glyph) on the right column
  // when the ring exceeds the viewport height.
  static testScrollbarAppearsWhenOverflowing_() {
    var lv = LogView.new()
    lv.bounds = Rect.new(1, 1, 10, 3)
    for (i in 1..6) {
      lv.append(LogView.LEVEL_INFO, "line %(i)")
    }
    var sf = lv.draw()
    var topCh = sf.cellAt(9, 0).chByte
    check_(topCh == 0x1E && sf.cellAt(8, 0).ch == "│" &&
           sf.cellAt(0, 0).ch == "l",
           "right scrollbar has separator and leaves content at column 0")
  }

  static testScrollbarWheelMovesPage_() {
    var lv = LogView.new()
    lv.bounds = Rect.new(1, 1, 10, 5)
    for (i in 1..12) {
      lv.append(LogView.LEVEL_INFO, "line %(i)")
    }
    var ev = MouseEvent.new(Mouse.wheelUpClick, 10, 3, 10, 3)
    var consumed = lv.handle(ev)
    check_(consumed && lv.scrollTop == 2,
           "wheel over scrollbar moves one viewport page")
  }

  // When the ring drops the oldest line on overflow, an active scroll
  // anchor must shift down by 1 so the user keeps reading the same
  // entries (otherwise they'd silently glide forward in the buffer).
  static testCapacityShiftKeepsScrollAligned_() {
    var lv = LogView.new()
    lv.capacity = 4
    lv.bounds   = Rect.new(1, 1, 20, 2)
    lv.append(LogView.LEVEL_INFO, "1")
    lv.append(LogView.LEVEL_INFO, "2")
    lv.append(LogView.LEVEL_INFO, "3")
    lv.append(LogView.LEVEL_INFO, "4")
    lv.handle(keyEvent_(Key.home))      // anchor at index 0
    lv.append(LogView.LEVEL_INFO, "5")  // drops "1"; anchor was 0, can't go negative
    check_(lv.scrollTop == 0 && lv.count == 4,
           "ring overflow shifts anchor (clamped at 0) and trims oldest")
  }
}
