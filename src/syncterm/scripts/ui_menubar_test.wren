// Self-tests for ui_menubar.

import "ui_widget"  for Rect
import "ui_menubar" for MenuBar
import "syncterm"   for KeyEvent, MouseEvent, Key, Mouse

class UiMenubarTest {
  static run() {
    __pass = 0
    __fail = 0
    System.print("=== ui_menubar self-test starting ===")

    testDefaults_()
    testItemsSeedsFocus_()
    testRightWraps_()
    testLeftWraps_()
    testHomeEnd_()
    testEnterActivatesFocused_()
    testSpaceActivatesFocused_()
    testHotkeyActivates_()
    testHotkeyCaseInsensitive_()
    testHotkeyNoMatchFallsThrough_()
    testMouseClickActivates_()
    testMouseClickGapIgnored_()
    testDrawShowsLabels_()
    testFocusedItemHasDifferentStyle_()

    var total = __pass + __fail
    System.print("=== ui_menubar: %(total) tests, %(__pass) pass, %(__fail) fail ===")
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

  static makeBar_(onFire) {
    var bar = MenuBar.new()
    bar.bounds = Rect.new(1, 1, 30, 1)
    bar.items = [
      ["File", Fn.new { onFire.call("File") }],
      ["Edit", Fn.new { onFire.call("Edit") }],
      ["Help", Fn.new { onFire.call("Help") }],
    ]
    return bar
  }

  static testDefaults_() {
    var bar = MenuBar.new()
    check_(bar.count == 0 && bar.focusedItem == null && bar.focusable == true,
           "MenuBar: defaults (no items, no focus, focusable)")
  }

  static testItemsSeedsFocus_() {
    var fired = null
    var bar = makeBar_(Fn.new {|label| fired = label })
    check_(bar.count == 3 && bar.focusedItem == 0,
           "MenuBar: items= seeds focusedItem=0")
  }

  static testRightWraps_() {
    var fired = null
    var bar = makeBar_(Fn.new {|label| fired = label })
    bar.handle(KeyEvent.new(Key.right))
    bar.handle(KeyEvent.new(Key.right))
    bar.handle(KeyEvent.new(Key.right))
    check_(bar.focusedItem == 0,
           "MenuBar: Right past last wraps to 0")
  }

  static testLeftWraps_() {
    var fired = null
    var bar = makeBar_(Fn.new {|label| fired = label })
    bar.handle(KeyEvent.new(Key.left))
    check_(bar.focusedItem == 2,
           "MenuBar: Left from 0 wraps to last")
  }

  static testHomeEnd_() {
    var fired = null
    var bar = makeBar_(Fn.new {|label| fired = label })
    bar.handle(KeyEvent.new(Key.end))
    check_(bar.focusedItem == 2, "MenuBar: End jumps to last")
    bar.handle(KeyEvent.new(Key.home))
    check_(bar.focusedItem == 0, "MenuBar: Home jumps to first")
  }

  static testEnterActivatesFocused_() {
    var fired = null
    var bar = makeBar_(Fn.new {|label| fired = label })
    bar.handle(KeyEvent.new(Key.right))    // focus Edit
    bar.handle(KeyEvent.new(Key.enter))
    check_(fired == "Edit",
           "MenuBar: Enter activates focused item")
  }

  static testSpaceActivatesFocused_() {
    var fired = null
    var bar = makeBar_(Fn.new {|label| fired = label })
    bar.handle(KeyEvent.new(0x20))         // space on default focus (File)
    check_(fired == "File",
           "MenuBar: Space activates focused item")
  }

  static testHotkeyActivates_() {
    var fired = null
    var bar = makeBar_(Fn.new {|label| fired = label })
    bar.handle(KeyEvent.new(0x45))         // 'E'
    check_(fired == "Edit" && bar.focusedItem == 1,
           "MenuBar: hotkey letter activates matching item")
  }

  static testHotkeyCaseInsensitive_() {
    var fired = null
    var bar = makeBar_(Fn.new {|label| fired = label })
    bar.handle(KeyEvent.new(0x68))         // 'h'
    check_(fired == "Help",
           "MenuBar: hotkey is case-insensitive (lowercase 'h')")
  }

  static testHotkeyNoMatchFallsThrough_() {
    var fired = null
    var bar = makeBar_(Fn.new {|label| fired = label })
    var consumed = bar.handle(KeyEvent.new(0x5A))   // 'Z' — no match
    check_(!consumed && fired == null,
           "MenuBar: unmatched letter falls through, nothing fires")
  }

  static testMouseClickActivates_() {
    var fired = null
    var bar = makeBar_(Fn.new {|label| fired = label })
    // Layout: File (0..6), space (6), Edit (7..13), space (13), Help (14..20)
    // Click on screen col bounds.x + 8 = 9 (inside Edit)
    var ev = MouseEvent.new(Mouse.button1Click, 9, 1, 9, 1)
    var consumed = bar.handle(ev)
    check_(consumed && fired == "Edit" && bar.focusedItem == 1,
           "MenuBar: click on item activates it")
  }

  static testMouseClickGapIgnored_() {
    var fired = null
    var bar = makeBar_(Fn.new {|label| fired = label })
    // Gap after File ends — surface col 6 (separator), screen col bounds.x + 6 = 7
    var ev = MouseEvent.new(Mouse.button1Click, 7, 1, 7, 1)
    var consumed = bar.handle(ev)
    check_(!consumed && fired == null,
           "MenuBar: click on inter-item gap is dropped")
  }

  static testDrawShowsLabels_() {
    var fired = null
    var bar = makeBar_(Fn.new {|label| fired = label })
    var sf = bar.draw()
    // File at col 1 (after the leading space)
    check_(sf.cellAt(1, 0).ch == "F" && sf.cellAt(2, 0).ch == "i",
           "MenuBar.draw: first item label visible")
  }

  static testFocusedItemHasDifferentStyle_() {
    var fired = null
    var onFire = Fn.new {|label| fired = label }
    var a = makeBar_(onFire)
    var b = makeBar_(onFire)
    b.handle(KeyEvent.new(Key.right))
    var sa = a.draw()
    var sb = b.draw()
    // Edit's first cell at surface col 7 — same item, but `a` has File
    // focused (so Edit is unfocused) and `b` has Edit focused.
    check_(sa.cellAt(8, 0).legacyAttr != sb.cellAt(8, 0).legacyAttr,
           "MenuBar.draw: focused item style differs from unfocused")
  }
}
