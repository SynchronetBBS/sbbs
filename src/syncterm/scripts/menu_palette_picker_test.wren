// Self-tests for menu_palette_picker.

import "syncterm" for Key, KeyEvent, Mouse, MouseEvent
import "ui_widget" for Rect
import "ui_color_picker" for ColorPicker
import "menu_palette_picker" for PaletteContrastPreview,
    PalettePickerContent, PalettePickerPane

class MenuPalettePickerTest {
  static run() {
    __pass = 0
    __fail = 0
    System.print("=== menu_palette_picker self-test starting ===")

    testShortPaletteRepetition_()
    testContrastDirections_()
    testContrastLegacyAttributes_()
    testContrastClickSelectsSample_()
    testPaneCommands_()
    testCompactLayout_()
    testContentPadding_()
    testRampClickPromotesFocus_()
    testVerticalNavigationWrap_()
    testHomeEndNavigation_()
    testPickerChildFocusFollowsContainer_()

    var total = __pass + __fail
    System.print("=== menu_palette_picker: %(total) tests, %(__pass) pass, " +
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

  static testShortPaletteRepetition_() {
    var ok = true
    for (count in [2, 4, 8, 16]) {
      var colors = []
      for (i in 0...count) colors.add(i + 1)
      var preview = PaletteContrastPreview.new(colors, 1, 0xABCDEF)
      for (i in 0...16) {
        var expected = i % count == 1 ? 0xABCDEF : colors[i % count]
        if (preview.paletteColor_(i) != expected) ok = false
      }
    }
    check_(ok, "PaletteContrastPreview: 2/4/8/16-entry repetition")
  }

  static testContrastDirections_() {
    var preview = PaletteContrastPreview.new([0xFF0000, 0x00FF00],
        0, 0x0000FF)
    preview.bounds = Rect.new(1, 1, 22, 2)
    var sf = preview.draw()
    var asForeground = sf.cellAt(7, 0)
    var asBackground = sf.cellAt(7, 1)
    check_(asForeground.fgRgb == 0x0000FF &&
           asForeground.bgRgb == 0x00FF00 &&
           asBackground.fgRgb == 0x00FF00 &&
           asBackground.bgRgb == 0x0000FF,
           "PaletteContrastPreview: edited colour shown as FG and BG")
  }

  static testContrastLegacyAttributes_() {
    var preview = PaletteContrastPreview.new([0xFF0000, 0x00FF00],
        0, 0x0000FF)
    preview.bounds = Rect.new(1, 1, 22, 2)
    var sf = preview.draw()
    check_(sf.cellAt(7, 0).legacyAttr != 0 &&
           sf.cellAt(7, 1).legacyAttr != 0,
           "PaletteContrastPreview: explicit legacy attributes")
  }

  static testContrastClickSelectsSample_() {
    var preview = PaletteContrastPreview.new([0xFF0000, 0x00FF00],
        0, 0x0000FF)
    preview.bounds = Rect.new(1, 1, 22, 2)
    var sample = [0, 0]
    preview.onSample = Fn.new {|foreground, background|
      sample[0] = foreground
      sample[1] = background
    }
    preview.handle(MouseEvent.new(Mouse.button1Click, 8, 1, 8, 1))
    var foreground = sample[0] == 0x0000FF && sample[1] == 0x00FF00
    preview.handle(MouseEvent.new(Mouse.button1Click, 8, 2, 8, 2))
    check_(foreground && sample[0] == 0x00FF00 &&
           sample[1] == 0x0000FF,
           "PaletteContrastPreview: grid click selects sample pairing")
  }

  static pickerContent_(calls) {
    var picker = ColorPicker.new(0x123456)
    var preview = PaletteContrastPreview.new([0x123456, 0xFFFFFF],
        0, 0x123456)
    var content = PalettePickerContent.new(picker, preview,
        Fn.new { calls[0] = calls[0] + 1 },
        Fn.new { calls[1] = calls[1] + 1 },
        Fn.new { calls[2] = calls[2] + 1 })
    return [picker, content]
  }

  static testPaneCommands_() {
    var calls = [0, 0, 0]
    var pair = pickerContent_(calls)
    var pane = PalettePickerPane.new(pair[1],
        Fn.new { calls[0] = calls[0] + 1 },
        Fn.new { calls[1] = calls[1] + 1 },
        Fn.new { calls[2] = calls[2] + 1 })
    pane.handle(KeyEvent.new(0x25))
    pane.handle(KeyEvent.new(Key.enter))
    pane.handle(KeyEvent.new(Key.escape))
    check_(calls[0] == 1 && calls[1] == 1 && calls[2] == 1,
           "PalettePickerPane: Default, Enter, and Escape commands")
  }

  static testCompactLayout_() {
    var calls = [0, 0, 0]
    var pair = pickerContent_(calls)
    var content = pair[1]
    content.bounds = Rect.new(1, 1, 34, content.preferredHeight)
    var picker = content.children[1]
    var cancel = content.children[4]
    check_(picker.bounds.w == 34 && cancel.bounds.right <= 33 &&
           content.preferredHeight == 14,
           "PalettePickerContent: compact layout fits 40-column screen")
  }

  static testContentPadding_() {
    var calls = [0, 0, 0]
    var pair = pickerContent_(calls)
    var content = pair[1]
    content.bounds = Rect.new(5, 7, content.preferredWidth,
        content.preferredHeight)
    var contrast = content.children[0]
    var picker = content.children[1]
    var button = content.children[2]
    check_(contrast.bounds.x == 6 && contrast.bounds.y == 7 &&
           picker.bounds.x == 5 && picker.bounds.w == content.bounds.w &&
           button.bounds.bottom == content.bounds.bottom,
           "PalettePickerContent: only horizontal content is inset")
  }

  static testRampClickPromotesFocus_() {
    var calls = [0, 0, 0]
    var pair = pickerContent_(calls)
    var picker = pair[0]
    var content = pair[1]
    content.bounds = Rect.new(1, 1, 34, content.preferredHeight)
    content.focusedIndex = 2
    var red = picker.children[picker.firstFocusIndex]
    red.handle(MouseEvent.new(Mouse.button1Click,
        red.bounds.x + 3, red.bounds.y, red.bounds.x + 3, red.bounds.y))
    check_(content.focusedIndex == 1 &&
           picker.focusedIndex == picker.firstFocusIndex,
           "ColorRamp: mouse click promotes nested picker focus")
  }

  static testVerticalNavigationWrap_() {
    var calls = [0, 0, 0]
    var pair = pickerContent_(calls)
    var picker = pair[0]
    var content = pair[1]
    content.bounds = Rect.new(1, 1, content.preferredWidth,
        content.preferredHeight)
    content.focusedIndex = 1
    picker.focusedIndex = picker.firstFocusIndex
    var up = content.handle(KeyEvent.new(Key.up))
    var wrappedUp = content.focusedIndex == 4
    var down = content.handle(KeyEvent.new(Key.down))
    var wrappedDown = content.focusedIndex == 1 &&
        picker.focusedIndex == picker.firstFocusIndex
    picker.focusedIndex = picker.lastFocusIndex
    var intoButtons = content.handle(KeyEvent.new(Key.down))
    var button = content.focusedIndex == 2
    var backToHex = content.handle(KeyEvent.new(Key.up))
    check_(up && wrappedUp && down && wrappedDown && intoButtons && button &&
           backToHex && content.focusedIndex == 1 &&
           picker.focusedIndex == picker.lastFocusIndex,
           "PalettePickerContent: Up and Down wrap through all rows")
  }

  static testHomeEndNavigation_() {
    var calls = [0, 0, 0]
    var pair = pickerContent_(calls)
    var picker = pair[0]
    var content = pair[1]
    content.bounds = Rect.new(1, 1, content.preferredWidth,
        content.preferredHeight)
    content.focusedIndex = 1
    picker.focusedIndex = 3
    var end = content.handle(KeyEvent.new(Key.end))
    var atButtons = content.focusedIndex == 2
    content.focusedIndex = 1
    picker.focusedIndex = 3
    var home = content.handle(KeyEvent.new(Key.home))
    check_(end && atButtons && home && content.focusedIndex == 1 &&
           picker.focusedIndex == picker.firstFocusIndex,
           "PalettePickerContent: Home selects first field, End button row")
  }

  static testPickerChildFocusFollowsContainer_() {
    var calls = [0, 0, 0]
    var pair = pickerContent_(calls)
    var picker = pair[0]
    var content = pair[1]
    content.bounds = Rect.new(1, 1, content.preferredWidth,
        content.preferredHeight)
    content.focusedIndex = 1
    picker.focusedIndex = picker.firstFocusIndex
    var ramp = picker.focusedChild
    var initiallyFocused = ramp.focused
    content.focusedIndex = 2
    var released = !ramp.focused
    content.focusedIndex = 1
    check_(initiallyFocused && released && ramp.focused &&
           picker.focusedChild == ramp,
           "ColorPicker: remembered child is current only with container")
  }
}
