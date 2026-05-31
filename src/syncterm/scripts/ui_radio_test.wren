// Self-tests for ui_radio.

import "ui_widget" for Rect
import "ui_radio"  for RadioGroup
import "syncterm"  for KeyEvent, MouseEvent, Key, Mouse

class UiRadioTest {
  static run() {
    __pass = 0
    __fail = 0
    System.print("=== ui_radio self-test starting ===")

    testDefaults_()
    testItemsSetsSelectionAndCursor_()
    testDownMovesCursorOnly_()
    testSpaceCommitsCursor_()
    testEnterCommitsCursor_()
    testHomeEndJump_()
    testWrapDefaultUpFromTopWrapsToBottom_()
    testWrapDefaultDownFromBottomWrapsToTop_()
    testNoWrapUpAtTopFallsThrough_()
    testNoWrapDownAtBottomFallsThrough_()
    testMouseClickSelects_()
    testMouseClickOutsideIgnored_()
    testOnChangeFires_()
    testOnChangeNotFiredWhenSame_()
    testDrawSelectionGlyphDiffers_()
    testCursorPosTracksCursorRow_()

    var total = __pass + __fail
    System.print("=== ui_radio: %(total) tests, %(__pass) pass, %(__fail) fail ===")
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
    var g = RadioGroup.new()
    check_(g.count == 0 && g.selected == null && g.focusable == true,
           "RadioGroup: empty, no selection, focusable")
  }

  static testItemsSetsSelectionAndCursor_() {
    var g = RadioGroup.new()
    g.items = ["a", "b", "c"]
    check_(g.count == 3 && g.selected == 0 && g.cursor == 0,
           "RadioGroup: items= seeds selected=0 cursor=0")
  }

  static testDownMovesCursorOnly_() {
    var g = RadioGroup.new()
    g.items  = ["a", "b", "c"]
    g.bounds = Rect.new(1, 1, 10, 3)
    g.handle(KeyEvent.new(Key.down))
    check_(g.cursor == 1 && g.selected == 0,
           "RadioGroup: Down moves cursor without committing")
  }

  static testSpaceCommitsCursor_() {
    var g = RadioGroup.new()
    g.items  = ["a", "b", "c"]
    g.bounds = Rect.new(1, 1, 10, 3)
    g.handle(KeyEvent.new(Key.down))
    g.handle(KeyEvent.new(0x20))
    check_(g.selected == 1,
           "RadioGroup: Space commits cursor as selection")
  }

  static testEnterCommitsCursor_() {
    var g = RadioGroup.new()
    g.items  = ["a", "b", "c"]
    g.bounds = Rect.new(1, 1, 10, 3)
    g.handle(KeyEvent.new(Key.down))
    g.handle(KeyEvent.new(Key.enter))
    check_(g.selected == 1,
           "RadioGroup: Enter commits cursor as selection")
  }

  static testHomeEndJump_() {
    var g = RadioGroup.new()
    g.items  = ["a", "b", "c"]
    g.bounds = Rect.new(1, 1, 10, 3)
    g.handle(KeyEvent.new(Key.end))
    check_(g.cursor == 2, "RadioGroup: End jumps cursor to last")
    g.handle(KeyEvent.new(Key.home))
    check_(g.cursor == 0, "RadioGroup: Home jumps cursor to first")
  }

  static testWrapDefaultUpFromTopWrapsToBottom_() {
    var g = RadioGroup.new()
    g.items  = ["a", "b", "c"]
    g.bounds = Rect.new(1, 1, 10, 3)
    var consumed = g.handle(KeyEvent.new(Key.up))
    check_(consumed && g.cursor == 2,
           "RadioGroup: wrap default — Up at top wraps to last")
  }

  static testWrapDefaultDownFromBottomWrapsToTop_() {
    var g = RadioGroup.new()
    g.items  = ["a", "b"]
    g.bounds = Rect.new(1, 1, 10, 2)
    g.handle(KeyEvent.new(Key.down))
    var consumed = g.handle(KeyEvent.new(Key.down))
    check_(consumed && g.cursor == 0,
           "RadioGroup: wrap default — Down at bottom wraps to first")
  }

  static testNoWrapUpAtTopFallsThrough_() {
    var g = RadioGroup.new()
    g.items  = ["a", "b"]
    g.bounds = Rect.new(1, 1, 10, 2)
    g.wrap   = false
    var consumed = g.handle(KeyEvent.new(Key.up))
    check_(!consumed && g.cursor == 0,
           "RadioGroup: wrap=false — Up at top falls through (cursor unchanged)")
  }

  static testNoWrapDownAtBottomFallsThrough_() {
    var g = RadioGroup.new()
    g.items  = ["a", "b"]
    g.bounds = Rect.new(1, 1, 10, 2)
    g.wrap   = false
    g.handle(KeyEvent.new(Key.down))
    var consumed = g.handle(KeyEvent.new(Key.down))
    check_(!consumed && g.cursor == 1,
           "RadioGroup: wrap=false — Down at bottom falls through")
  }

  static testMouseClickSelects_() {
    var g = RadioGroup.new()
    g.items  = ["a", "b", "c"]
    g.bounds = Rect.new(2, 5, 10, 3)
    var ev = MouseEvent.new(Mouse.button1Click, 4, 7, 4, 7)
    var consumed = g.handle(ev)
    check_(consumed && g.selected == 2 && g.cursor == 2,
           "RadioGroup: click selects the clicked row")
  }

  static testMouseClickOutsideIgnored_() {
    var g = RadioGroup.new()
    g.items  = ["a", "b"]
    g.bounds = Rect.new(2, 5, 10, 2)
    var ev = MouseEvent.new(Mouse.button1Click, 50, 50, 50, 50)
    var consumed = g.handle(ev)
    check_(!consumed && g.selected == 0,
           "RadioGroup: click outside bounds dropped")
  }

  static testOnChangeFires_() {
    var g = RadioGroup.new()
    g.items  = ["a", "b"]
    g.bounds = Rect.new(1, 1, 10, 2)
    var got = null
    g.onChange = Fn.new {|i, it| got = [i, it] }
    g.handle(KeyEvent.new(Key.down))
    g.handle(KeyEvent.new(0x20))
    check_(got != null && got[0] == 1 && got[1] == "b",
           "RadioGroup: onChange fires with index + item")
  }

  static testOnChangeNotFiredWhenSame_() {
    var g = RadioGroup.new()
    g.items  = ["a", "b"]
    g.bounds = Rect.new(1, 1, 10, 2)
    var fires = 0
    g.onChange = Fn.new {|i, it| fires = fires + 1 }
    g.handle(KeyEvent.new(0x20))      // commits cursor==selected==0
    check_(fires == 0,
           "RadioGroup: committing same selection doesn't fire onChange")
  }

  static testDrawSelectionGlyphDiffers_() {
    var g = RadioGroup.new()
    g.items  = ["a", "b"]
    g.bounds = Rect.new(1, 1, 10, 2)
    var s = g.draw()
    check_(s.cellAt(1, 0).ch != s.cellAt(1, 1).ch,
           "RadioGroup.draw: selected and unselected glyphs differ")
  }

  static testCursorPosTracksCursorRow_() {
    var g = RadioGroup.new()
    g.items  = ["a", "b", "c"]
    g.bounds = Rect.new(3, 5, 10, 3)
    g.handle(KeyEvent.new(Key.down))
    var p = g.cursorPos
    check_(p[0] == 3 && p[1] == 6,
           "RadioGroup.cursorPos: tracks cursor row in screen coords")
  }
}
