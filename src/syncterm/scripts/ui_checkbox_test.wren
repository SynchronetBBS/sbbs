// Self-tests for ui_checkbox.

import "ui_widget"   for Rect
import "ui_checkbox" for Checkbox
import "syncterm"    for KeyEvent, MouseEvent, Key, Mouse

class UiCheckboxTest {
  static run() {
    __pass = 0
    __fail = 0
    System.print("=== ui_checkbox self-test starting ===")

    testDefaults_()
    testIntrinsicWidth_()
    testSpaceToggles_()
    testEnterToggles_()
    testRandomKeyDoesNothing_()
    testMouseClickToggles_()
    testMouseClickOutsideIgnored_()
    testOnChangeFires_()
    testOnChangeNotFiredWhenSame_()
    testValueSetterDirties_()
    testDrawShowsLabel_()
    testDrawCheckedGlyphDiffersFromUnchecked_()

    var total = __pass + __fail
    System.print("=== ui_checkbox: %(total) tests, %(__pass) pass, %(__fail) fail ===")
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
    var c = Checkbox.new("Show shadows")
    check_(c.label == "Show shadows" && c.value == false &&
           c.focusable == true,
           "Checkbox: label set, value false, focusable")
  }

  static testIntrinsicWidth_() {
    var c = Checkbox.new("ABC")
    check_(c.intrinsicWidth == 7,
           "Checkbox.intrinsicWidth: '[X] ABC' -> 7 cells")
  }

  static testSpaceToggles_() {
    var c = Checkbox.new("X")
    c.bounds = Rect.new(1, 1, 5, 1)
    var consumed = c.handle(KeyEvent.new(0x20))
    check_(consumed && c.value == true,
           "Checkbox: Space toggles off->on")
    c.handle(KeyEvent.new(0x20))
    check_(c.value == false,
           "Checkbox: Space toggles on->off")
  }

  static testEnterToggles_() {
    var c = Checkbox.new("X")
    c.bounds = Rect.new(1, 1, 5, 1)
    var consumed = c.handle(KeyEvent.new(Key.enter))
    check_(consumed && c.value == true,
           "Checkbox: Enter toggles")
  }

  static testRandomKeyDoesNothing_() {
    var c = Checkbox.new("X")
    c.bounds = Rect.new(1, 1, 5, 1)
    var consumed = c.handle(KeyEvent.new(0x41))
    check_(!consumed && c.value == false,
           "Checkbox: random printable key does not toggle")
  }

  static testMouseClickToggles_() {
    var c = Checkbox.new("X")
    c.bounds = Rect.new(5, 5, 5, 1)
    var ev = MouseEvent.new(Mouse.button1Click, 6, 5, 6, 5)
    var consumed = c.handle(ev)
    check_(consumed && c.value == true,
           "Checkbox: click inside bounds toggles")
  }

  static testMouseClickOutsideIgnored_() {
    var c = Checkbox.new("X")
    c.bounds = Rect.new(5, 5, 5, 1)
    var ev = MouseEvent.new(Mouse.button1Click, 50, 50, 50, 50)
    var consumed = c.handle(ev)
    check_(!consumed && c.value == false,
           "Checkbox: click outside bounds dropped")
  }

  static testOnChangeFires_() {
    var c = Checkbox.new("X")
    c.bounds = Rect.new(1, 1, 5, 1)
    var got = [null]
    c.onChange = Fn.new {|v| got[0] = v }
    c.handle(KeyEvent.new(0x20))
    check_(got[0] == true,
           "Checkbox: onChange fires with new value")
  }

  static testOnChangeNotFiredWhenSame_() {
    var c = Checkbox.new("X")
    var fires = [0]
    c.onChange = Fn.new {|v| fires[0] = fires[0] + 1 }
    c.value = false   // already false
    check_(fires[0] == 0,
           "Checkbox: assigning the same value does not fire onChange")
  }

  static testValueSetterDirties_() {
    var c = Checkbox.new("X")
    c.bounds = Rect.new(1, 1, 5, 1)
    c.draw()
    check_(c.dirty == false, "Checkbox: clean after draw")
    c.value = true
    check_(c.dirty == true, "Checkbox: value= marks dirty")
  }

  static testDrawShowsLabel_() {
    var c = Checkbox.new("Yes")
    c.bounds = Rect.new(1, 1, 7, 1)
    var s = c.draw()
    check_(s.cellAt(0, 0).ch == "[" && s.cellAt(2, 0).ch == "]" &&
           s.cellAt(4, 0).ch == "Y" && s.cellAt(5, 0).ch == "e" &&
           s.cellAt(6, 0).ch == "s",
           "Checkbox.draw: '[ ] Yes' painted (label visible)")
  }

  static testDrawCheckedGlyphDiffersFromUnchecked_() {
    var off = Checkbox.new("X")
    off.bounds = Rect.new(1, 1, 5, 1)
    var on = Checkbox.new("X")
    on.bounds = Rect.new(1, 1, 5, 1)
    on.value  = true
    var so = off.draw()
    var sn = on.draw()
    check_(so.cellAt(1, 0).ch != sn.cellAt(1, 0).ch,
           "Checkbox.draw: checked glyph differs from unchecked")
  }
}
