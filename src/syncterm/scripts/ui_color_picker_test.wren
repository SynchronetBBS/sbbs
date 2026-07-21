// Self-tests for ui_color_picker.

import "syncterm" for Key, KeyEvent, Mouse, MouseEvent
import "ui_widget" for Rect
import "ui_color_picker" for ColorModel, ColorPicker, ColorRamp

class UiColorPickerTest {
  static run() {
    __pass = 0
    __fail = 0
    System.print("=== ui_color_picker self-test starting ===")

    testRgbHsvPrimaries_()
    testHsvRgbPrimaries_()
    testUndefinedHsvComponentsAreRetained_()
    testHexRoundTrip_()
    testRampClickEndpoints_()
    testRampWheelMovesOne_()
    testRampDragFallsThrough_()
    testRampKeyboardSteps_()
    testRampStyleRoles_()
    testHsvRampHasNoNumber_()
    testEditableRampDigits_()
    testEditableRampRestarts_()
    testPickerRgbSynchronization_()
    testPickerHexSynchronization_()
    testPickerHexStyle_()
    testPickerHexCursorCell_()
    testPickerRejectsIncompleteHex_()
    testPickerLegacyAttributes_()
    testPickerCompactSample_()

    var total = __pass + __fail
    System.print("=== ui_color_picker: %(total) tests, %(__pass) pass, " +
        "%(__fail) fail ===")
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

  static testRgbHsvPrimaries_() {
    var red = ColorModel.toHsv(0xFF0000, 0, 0)
    var green = ColorModel.toHsv(0x00FF00, 0, 0)
    var blue = ColorModel.toHsv(0x0000FF, 0, 0)
    check_(red[0] == 0 && red[1] == 100 && red[2] == 100 &&
           green[0] == 120 && green[1] == 100 && green[2] == 100 &&
           blue[0] == 240 && blue[1] == 100 && blue[2] == 100,
           "ColorModel.toHsv: RGB primaries")
  }

  static testHsvRgbPrimaries_() {
    check_(ColorModel.fromHsv(0, 100, 100) == 0xFF0000 &&
           ColorModel.fromHsv(120, 100, 100) == 0x00FF00 &&
           ColorModel.fromHsv(240, 100, 100) == 0x0000FF,
           "ColorModel.fromHsv: RGB primaries")
  }

  static testUndefinedHsvComponentsAreRetained_() {
    var grey = ColorModel.toHsv(0x808080, 210, 70)
    var black = ColorModel.toHsv(0x000000, 210, 70)
    check_(grey[0] == 210 && grey[1] == 0 && grey[2] == 50 &&
           black[0] == 210 && black[1] == 70 && black[2] == 0,
           "ColorModel.toHsv: retain undefined grey/black components")
  }

  static testHexRoundTrip_() {
    check_(ColorModel.hex(0x12ABEF) == "12ABEF" &&
           ColorModel.parseHex("12abEF") == 0x12ABEF &&
           ColorModel.parseHex("12345") == null &&
           ColorModel.parseHex("GG0000") == null,
           "ColorModel: hexadecimal parse and format")
  }

  static ramp_() {
    var ramp = ColorRamp.new("R", 255, 16, true,
        Fn.new {|value| value << 16 })
    ramp.bounds = Rect.new(2, 5, 20, 1)
    ramp.value = 100
    return ramp
  }

  static testRampClickEndpoints_() {
    var ramp = ramp_()
    // Local ramp columns are 4..13, hence screen columns 6..15.
    ramp.handle(MouseEvent.new(Mouse.button1Click, 6, 5, 6, 5))
    var low = ramp.value == 0
    ramp.handle(MouseEvent.new(Mouse.button1Click, 15, 5, 15, 5))
    check_(low && ramp.value == 255,
           "ColorRamp: clicks map to both endpoints")
  }

  static testRampWheelMovesOne_() {
    var ramp = ramp_()
    ramp.handle(MouseEvent.new(Mouse.wheelUpPress, 8, 5, 8, 5))
    var up = ramp.value == 101
    ramp.handle(MouseEvent.new(Mouse.wheelDownPress, 8, 5, 8, 5))
    check_(up && ramp.value == 100,
           "ColorRamp: wheel changes exactly one unit")
  }

  static testRampDragFallsThrough_() {
    var ramp = ramp_()
    var start = ramp.handle(
        MouseEvent.new(Mouse.button1DragStart, 5, 5, 12, 5))
    var move = ramp.handle(
        MouseEvent.new(Mouse.button1DragMove, 5, 5, 14, 5))
    check_(!start && !move && ramp.value == 100,
           "ColorRamp: drag events remain available for text selection")
  }

  static testRampKeyboardSteps_() {
    var ramp = ramp_()
    ramp.handle(KeyEvent.new(Key.right))
    var right = ramp.value == 101
    ramp.handle(KeyEvent.new(Key.left))
    ramp.handle(KeyEvent.new(Key.pageUp))
    var page = ramp.value == 116
    check_(right && page,
           "ColorRamp: arrows and page step adjust the value")
  }

  static testRampStyleRoles_() {
    var ramp = ramp_()
    var normal = ramp.draw()
    var normalRow = normal.cellAt(0, 0).legacyAttr ==
        ramp.style("default").legacyAttr
    var normalInput = normal.cellAt(16, 0).legacyAttr ==
        ramp.style("input").legacyAttr
    ramp.focused = true
    var focused = ramp.draw()
    check_(normalRow && normalInput &&
           focused.cellAt(0, 0).legacyAttr ==
               ramp.style("list.item.focused").legacyAttr &&
           focused.cellAt(16, 0).legacyAttr ==
               ramp.style("input.focused").legacyAttr,
           "ColorRamp: row and numeric input use separate style roles")
  }

  static testHsvRampHasNoNumber_() {
    var ramp = ColorRamp.new("H", 359, 15, false,
        Fn.new {|value| ColorModel.fromHsv(value, 100, 100) })
    ramp.bounds = Rect.new(2, 5, 20, 1)
    ramp.value = 100
    var sf = ramp.draw()
    var right = ramp.handle(KeyEvent.new(Key.right))
    var wheel = ramp.handle(
        MouseEvent.new(Mouse.wheelUpPress, 8, 5, 8, 5))
    check_(sf.cellAt(18, 0).ch == "]" && sf.cellAt(19, 0).ch == " " &&
           right && wheel &&
           ramp.value == 102,
           "ColorRamp: HSV has no number and remains interactive")
  }

  static testEditableRampDigits_() {
    var ramp = ramp_()
    ramp.focused = true
    ramp.handle(KeyEvent.new(0x32))
    ramp.handle(KeyEvent.new(0x35))
    ramp.handle(KeyEvent.new(0x35))
    check_(ramp.value == 255,
           "ColorRamp: typed decimal replaces the focused value")
  }

  static testEditableRampRestarts_() {
    var ramp = ramp_()
    ramp.focused = true
    for (cp in [0x32, 0x35, 0x35, 0x34, 0x32]) {
      ramp.handle(KeyEvent.new(cp))
    }
    var automatic = ramp.value == 42
    ramp.handle(KeyEvent.new(Key.delete))
    ramp.handle(KeyEvent.new(0x37))
    check_(automatic && ramp.value == 7,
           "ColorRamp: complete entry and Delete restart RGB typing")
  }

  static testPickerRgbSynchronization_() {
    var picker = ColorPicker.new(0x102030)
    picker.bounds = Rect.new(1, 1, 44, picker.preferredHeight)
    picker.focusedIndex = picker.firstFocusIndex
    picker.children[picker.firstFocusIndex].handle(KeyEvent.new(Key.right))
    check_(picker.value == 0x112030,
           "ColorPicker: RGB ramp updates packed value")
  }

  static testPickerHexSynchronization_() {
    var picker = ColorPicker.new(0x102030)
    picker.bounds = Rect.new(1, 1, 44, picker.preferredHeight)
    picker.focusedIndex = picker.lastFocusIndex
    for (cp in [0x30, 0x30, 0x46, 0x46, 0x30, 0x30]) {
      picker.children[picker.lastFocusIndex].handle(KeyEvent.new(cp))
    }
    check_(picker.value == 0x00FF00 && picker.valid,
           "ColorPicker: complete hexadecimal input updates value")
  }

  static testPickerHexStyle_() {
    var picker = ColorPicker.new(0x102030)
    picker.bounds = Rect.new(1, 1, 44, picker.preferredHeight)
    var hex = picker.children[picker.lastFocusIndex]
    var normal = hex.draw()
    var normalInput = normal.cellAt(0, 0).legacyAttr ==
        hex.style("input").legacyAttr
    picker.focusedIndex = picker.lastFocusIndex
    var focused = hex.draw()
    check_(normalInput && focused.cellAt(0, 0).legacyAttr ==
           hex.style("input.focused").legacyAttr,
           "ColorHexInput: all cells use text-input style roles")
  }

  static testPickerHexCursorCell_() {
    var picker = ColorPicker.new(0x102030)
    picker.bounds = Rect.new(1, 1, 44, picker.preferredHeight)
    var hex = picker.children[picker.lastFocusIndex]
    var cursor = hex.cursorPos
    check_(hex.bounds.w == 7 && hex.maxLen == 6 && hex.scrollOff == 0 &&
           cursor[0] == hex.bounds.x + 6,
           "ColorHexInput: full value retains a trailing cursor cell")
  }

  static testPickerRejectsIncompleteHex_() {
    var picker = ColorPicker.new(0x102030)
    picker.children[picker.lastFocusIndex].value = "123"
    check_(!picker.valid && !picker.commitEdits() && picker.value == 0x102030,
           "ColorPicker: incomplete hexadecimal input cannot commit")
  }

  static testPickerLegacyAttributes_() {
    var picker = ColorPicker.new(0x800000)
    picker.bounds = Rect.new(1, 1, 44, picker.preferredHeight)
    picker.draw()
    var swatch = picker.children[0].draw()
    var ramp = picker.children[1].draw()
    check_(swatch.cellAt(12, 0).legacyAttr != 0 &&
           ramp.cellAt(4, 0).legacyAttr != 0,
           "ColorPicker: generated colour cells have legacy attributes")
  }

  static testPickerCompactSample_() {
    var picker = ColorPicker.new(0x123456)
    picker.bounds = Rect.new(1, 1, 44, picker.preferredHeight)
    picker.sampleColors(0xABCDEF, 0x654321)
    var sf = picker.children[0].draw()
    check_(sf.cellAt(11, 0).bgRgb != 0x654321 &&
           sf.cellAt(12, 0).bgRgb == 0x654321 &&
           sf.cellAt(30, 0).bgRgb == 0x654321 &&
           sf.cellAt(12, 2).bgRgb == 0x654321 &&
           sf.cellAt(30, 2).bgRgb == 0x654321 &&
           sf.cellAt(30, 1).bgRgb == 0x654321 &&
           sf.cellAt(31, 1).bgRgb != 0x654321 &&
           sf.cellAt(15, 1).ch == "A" &&
           sf.cellAt(15, 1).fgRgb == 0xABCDEF,
           "ColorPicker: sample is a compact three-row colour field")
  }

}
