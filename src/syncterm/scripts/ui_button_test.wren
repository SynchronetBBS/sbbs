// Self-tests for ui_button.

import "ui_widget" for Rect
import "ui_button" for Button
import "syncterm"  for KeyEvent, MouseEvent, Key, Mouse

class UiButtonTest {
  static run() {
    __pass = 0
    __fail = 0
    System.print("=== ui_button self-test starting ===")

    testDefaults_()
    testIntrinsicWidth_()
    testEnterFiresOnPress_()
    testSpaceFiresOnPress_()
    testRandomKeyDoesNotFire_()
    testMouseClickFiresOnPress_()
    testMouseClickOutsideIgnored_()
    testMouseHoverIgnored_()
    testCursorHidden_()
    testDrawShowsLabel_()
    testDrawFocusedDifferentStyle_()

    var total = __pass + __fail
    System.print("=== ui_button: %(total) tests, %(__pass) pass, %(__fail) fail ===")
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
    var b = Button.new("OK")
    check_(b.label == "OK" && b.focusable == true,
           "Button: label set, focusable by default")
  }

  static testIntrinsicWidth_() {
    var b = Button.new("Yes")
    check_(b.intrinsicWidth == 7,
           "Button.intrinsicWidth: '[ Yes ]' -> 7 cells")
  }

  static testEnterFiresOnPress_() {
    var b = Button.new("OK")
    b.bounds = Rect.new(1, 1, 6, 1)
    var fired = false
    b.onPress = Fn.new { fired = true }
    var consumed = b.handle(KeyEvent.new(Key.enter))
    check_(consumed && fired,
           "Button: Enter activates")
  }

  static testSpaceFiresOnPress_() {
    var b = Button.new("OK")
    b.bounds = Rect.new(1, 1, 6, 1)
    var fired = false
    b.onPress = Fn.new { fired = true }
    var consumed = b.handle(KeyEvent.new(0x20))
    check_(consumed && fired,
           "Button: Space activates")
  }

  static testRandomKeyDoesNotFire_() {
    var b = Button.new("OK")
    b.bounds = Rect.new(1, 1, 6, 1)
    var fired = false
    b.onPress = Fn.new { fired = true }
    var consumed = b.handle(KeyEvent.new(0x41))   // 'A'
    check_(!consumed && !fired,
           "Button: random printable key does not activate")
  }

  static testMouseClickFiresOnPress_() {
    var b = Button.new("OK")
    b.bounds = Rect.new(5, 5, 6, 1)
    var fired = false
    b.onPress = Fn.new { fired = true }
    var ev = MouseEvent.new(Mouse.button1Click, 6, 5, 6, 5)
    var consumed = b.handle(ev)
    check_(consumed && fired,
           "Button: mouse click inside bounds activates")
  }

  static testMouseClickOutsideIgnored_() {
    var b = Button.new("OK")
    b.bounds = Rect.new(5, 5, 6, 1)
    var fired = false
    b.onPress = Fn.new { fired = true }
    var ev = MouseEvent.new(Mouse.button1Click, 50, 50, 50, 50)
    var consumed = b.handle(ev)
    check_(!consumed && !fired,
           "Button: click outside bounds dropped")
  }

  static testMouseHoverIgnored_() {
    var b = Button.new("OK")
    b.bounds = Rect.new(5, 5, 6, 1)
    var fired = false
    b.onPress = Fn.new { fired = true }
    var ev = MouseEvent.new(Mouse.move, 6, 5, 6, 5)
    var consumed = b.handle(ev)
    check_(!consumed && !fired,
           "Button: hover (Mouse.move) ignored")
  }

  static testCursorHidden_() {
    var b = Button.new("OK")
    check_(b.cursorVisible == false,
           "Button: cursorVisible false (no insertion cursor)")
  }

  static testDrawShowsLabel_() {
    var b = Button.new("Yes")
    b.bounds = Rect.new(1, 1, 7, 1)
    var s = b.draw()
    check_(s.cellAt(0, 0).ch == "[" && s.cellAt(2, 0).ch == "Y" &&
           s.cellAt(3, 0).ch == "e" && s.cellAt(4, 0).ch == "s" &&
           s.cellAt(6, 0).ch == "]",
           "Button.draw: '[ Yes ]' painted")
  }

  static testDrawFocusedDifferentStyle_() {
    var a = Button.new("OK")
    a.bounds = Rect.new(1, 1, 6, 1)
    var b = Button.new("OK")
    b.bounds  = Rect.new(1, 1, 6, 1)
    b.focused = true
    var sa = a.draw()
    var sb = b.draw()
    check_(sa.cellAt(0, 0).legacyAttr != sb.cellAt(0, 0).legacyAttr,
           "Button.draw: focused style differs from unfocused")
  }
}
