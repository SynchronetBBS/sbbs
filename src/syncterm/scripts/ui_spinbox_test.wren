// Self-tests for ui_spinbox.

import "ui_widget"  for Rect
import "ui_spinbox" for SpinBox
import "syncterm"   for KeyEvent, MouseEvent, Key, Mouse

class UiSpinboxTest {
  static run() {
    __pass = 0
    __fail = 0
    System.print("=== ui_spinbox self-test starting ===")

    testDefaults_()
    testValueClampsToBounds_()
    testUpAddsStep_()
    testDownSubtractsStep_()
    testPlusMinusKeysWork_()
    testPageStepX10_()
    testHomeEndJumpToBounds_()
    testValueClampedAtMax_()
    testValueClampedAtMin_()
    testOnChangeFires_()
    testOnChangeNotFiredWhenSame_()
    testMouseClickUpArrow_()
    testMouseClickDownArrow_()
    testMouseClickBetweenArrowsIgnored_()
    testWheelChangesValue_()
    testDrawShowsValueAndArrows_()

    var total = __pass + __fail
    System.print("=== ui_spinbox: %(total) tests, %(__pass) pass, %(__fail) fail ===")
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
    var s = SpinBox.new()
    check_(s.value == 0 && s.min == 0 && s.max == 100 && s.step == 1 &&
           s.focusable == true,
           "SpinBox: defaults (value=0, min=0, max=100, step=1)")
  }

  static testValueClampsToBounds_() {
    var s = SpinBox.new()
    s.min = 10
    s.max = 20
    s.value = 5
    check_(s.value == 10, "SpinBox.value=: clamped to min")
    s.value = 30
    check_(s.value == 20, "SpinBox.value=: clamped to max")
  }

  static testUpAddsStep_() {
    var s = SpinBox.new()
    s.bounds = Rect.new(1, 1, 10, 1)
    s.value  = 5
    s.handle(KeyEvent.new(Key.up))
    check_(s.value == 6, "SpinBox: Up adds step")
  }

  static testDownSubtractsStep_() {
    var s = SpinBox.new()
    s.bounds = Rect.new(1, 1, 10, 1)
    s.value  = 5
    s.handle(KeyEvent.new(Key.down))
    check_(s.value == 4, "SpinBox: Down subtracts step")
  }

  static testPlusMinusKeysWork_() {
    var s = SpinBox.new()
    s.bounds = Rect.new(1, 1, 10, 1)
    s.value  = 5
    s.handle(KeyEvent.new(0x2B))      // '+'
    check_(s.value == 6, "SpinBox: '+' key adds step")
    s.handle(KeyEvent.new(0x2D))      // '-'
    check_(s.value == 5, "SpinBox: '-' key subtracts step")
  }

  static testPageStepX10_() {
    var s = SpinBox.new()
    s.bounds = Rect.new(1, 1, 10, 1)
    s.step   = 2
    s.value  = 50
    s.handle(KeyEvent.new(Key.pageUp))
    check_(s.value == 70, "SpinBox: PageUp = step*10")
    s.handle(KeyEvent.new(Key.pageDown))
    check_(s.value == 50, "SpinBox: PageDown = -step*10")
  }

  static testHomeEndJumpToBounds_() {
    var s = SpinBox.new()
    s.bounds = Rect.new(1, 1, 10, 1)
    s.min    = 5
    s.max    = 95
    s.value  = 50
    s.handle(KeyEvent.new(Key.home))
    check_(s.value == 5, "SpinBox: Home jumps to min")
    s.handle(KeyEvent.new(Key.end))
    check_(s.value == 95, "SpinBox: End jumps to max")
  }

  static testValueClampedAtMax_() {
    var s = SpinBox.new()
    s.bounds = Rect.new(1, 1, 10, 1)
    s.max    = 10
    s.value  = 10
    s.handle(KeyEvent.new(Key.up))
    check_(s.value == 10, "SpinBox: Up at max stays at max")
  }

  static testValueClampedAtMin_() {
    var s = SpinBox.new()
    s.bounds = Rect.new(1, 1, 10, 1)
    s.value  = 0
    s.handle(KeyEvent.new(Key.down))
    check_(s.value == 0, "SpinBox: Down at min stays at min")
  }

  static testOnChangeFires_() {
    var s = SpinBox.new()
    s.bounds = Rect.new(1, 1, 10, 1)
    var got = [null]
    s.onChange = Fn.new {|v| got[0] = v }
    s.handle(KeyEvent.new(Key.up))
    check_(got[0] == 1, "SpinBox: onChange fires with new value")
  }

  static testOnChangeNotFiredWhenSame_() {
    var s = SpinBox.new()
    s.bounds = Rect.new(1, 1, 10, 1)
    var fires = [0]
    s.onChange = Fn.new {|v| fires[0] = fires[0] + 1 }
    s.value = 0    // already 0
    check_(fires[0] == 0, "SpinBox: same-value assign doesn't fire")
  }

  static testMouseClickUpArrow_() {
    var s = SpinBox.new()
    s.bounds = Rect.new(2, 5, 10, 1)
    s.value  = 5
    // Up arrow at surface col w-3 = 7, screen col bounds.x + 7 = 9
    var ev = MouseEvent.new(Mouse.button1Click, 9, 5, 9, 5)
    s.handle(ev)
    check_(s.value == 6, "SpinBox: click on up arrow adds step")
  }

  static testMouseClickDownArrow_() {
    var s = SpinBox.new()
    s.bounds = Rect.new(2, 5, 10, 1)
    s.value  = 5
    // Down arrow at surface col w-2 = 8, screen col bounds.x + 8 = 10
    var ev = MouseEvent.new(Mouse.button1Click, 10, 5, 10, 5)
    s.handle(ev)
    check_(s.value == 4, "SpinBox: click on down arrow subtracts step")
  }

  static testMouseClickBetweenArrowsIgnored_() {
    var s = SpinBox.new()
    s.bounds = Rect.new(2, 5, 10, 1)
    s.value  = 5
    // Click in the body (surface col 4), should not change value
    var ev = MouseEvent.new(Mouse.button1Click, 6, 5, 6, 5)
    var consumed = s.handle(ev)
    check_(!consumed && s.value == 5,
           "SpinBox: click on body (not arrow) doesn't change value")
  }

  static testWheelChangesValue_() {
    var s = SpinBox.new()
    s.bounds = Rect.new(2, 5, 10, 1)
    s.value  = 5
    var up = MouseEvent.new(Mouse.wheelUpPress, 6, 5, 6, 5)
    s.handle(up)
    check_(s.value == 6, "SpinBox: wheel up adds step")
    var dn = MouseEvent.new(Mouse.wheelDownPress, 6, 5, 6, 5)
    s.handle(dn)
    check_(s.value == 5, "SpinBox: wheel down subtracts step")
  }

  static testDrawShowsValueAndArrows_() {
    var s = SpinBox.new()
    s.bounds = Rect.new(1, 1, 10, 1)
    s.value  = 42
    var sf = s.draw()
    var has4 = false
    var has2 = false
    var i = 0
    while (i < 10) {
      if (sf.cellAt(i, 0).ch == "4") has4 = true
      if (sf.cellAt(i, 0).ch == "2") has2 = true
      i = i + 1
    }
    var up = sf.cellAt(7, 0).ch
    var dn = sf.cellAt(8, 0).ch
    check_(has4 && has2 && up != " " && dn != " ",
           "SpinBox.draw: value digits + up/down arrows present")
  }
}
