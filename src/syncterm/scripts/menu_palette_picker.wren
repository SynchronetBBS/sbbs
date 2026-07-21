// Palette-entry colour picker used by the dialing-directory editor.
// The sixteen-position contrast preview belongs here rather than in the
// generic ui_color_picker module.

import "syncterm" for Color, Key, KeyEvent, Mouse, MouseEvent
import "ui_widget" for Container, Rect, Widget
import "ui_pane" for Pane
import "ui_button" for Button
import "ui_draw" for Painter
import "ui_style" for Style
import "ui_color_picker" for ColorModel, ColorPicker

class PaletteContrastPreview is Widget {
  construct new(colors, entry, color) {
    super()
    focusable = false
    _colors = colors
    _entry = entry
    _color = color & 0xFFFFFF
    _sampleRow = 1
    _samplePosition = entry == 7 ? 0 : 7
    _onSample = null
  }

  preferredWidth { 22 }
  preferredHeight { 2 }

  color { _color }
  color=(value) {
    var next = value & 0xFFFFFF
    if (next == _color) return
    _color = next
    markDirty()
    notifySample_()
  }

  onSample=(fn) {
    _onSample = fn
    notifySample_()
  }

  paletteColor_(position) {
    if (_colors.count == 0) return 0
    var entry = position % _colors.count
    return entry == _entry ? _color : _colors[entry]
  }

  sampleStyle_(foreground, background) {
    var base = style("default")
    return Style.new(base.font,
        Color.toLegacyAttr(foreground, background),
        foreground, background)
  }

  sampleColors_() {
    var other = paletteColor_(_samplePosition)
    return _sampleRow == 0 ? [_color, other] : [other, _color]
  }

  notifySample_() {
    if (_onSample == null) return
    var colors = sampleColors_()
    _onSample.call(colors[0], colors[1])
  }

  handle(event) {
    if (!(event is MouseEvent) || event.event != Mouse.button1Click ||
        bounds == null || !bounds.contains(event.startX, event.startY)) {
      return false
    }
    var sampleWidth = 22
    var x = ((bounds.w - sampleWidth) / 2).floor.max(0)
    var row = event.endY - bounds.y
    var position = event.endX - bounds.x - x - 6
    if (row < 0 || row > 1 || position < 0 || position >= 16) return true
    _sampleRow = row
    _samplePosition = position
    notifySample_()
    return true
  }

  onPaint_() {
    var normal = style("default")
    Painter.fill(surface, Rect.new(0, 0, bounds.w, bounds.h), " ", normal)
    var sampleWidth = 22
    var x = ((bounds.w - sampleWidth) / 2).floor.max(0)
    Painter.text(surface, x, 0, "as FG ", normal, bounds.w)
    Painter.text(surface, x, 1, "as BG ", normal, bounds.w)
    var digits = "0123456789ABCDEF"
    for (i in 0...16) {
      var other = paletteColor_(i)
      Painter.text(surface, x + 6 + i, 0, digits[i],
          sampleStyle_(_color, other), 1)
      Painter.text(surface, x + 6 + i, 1, digits[i],
          sampleStyle_(other, _color), 1)
    }
  }
}

class PalettePickerContent is Container {
  construct new(picker, contrast, onDefault, onAccept, onCancel) {
    super()
    _picker = picker
    _contrast = contrast
    _default = Button.new("Default")
    _accept = Button.new("OK")
    _cancel = Button.new("Cancel")
    _default.onPress = onDefault
    _accept.onPress = onAccept
    _cancel.onPress = onCancel

    add(_contrast)
    add(_picker)
    add(_default)
    add(_accept)
    add(_cancel)
  }

  preferredWidth { _picker.preferredWidth + 2 }
  preferredHeight { 14 }

  bounds=(rect) {
    super.bounds = rect
    var x = rect.x + 1
    var width = (rect.w - 2).max(1)
    _contrast.bounds = Rect.new(x, rect.y, width, 2)
    _picker.bounds = Rect.new(rect.x, rect.y + 2, rect.w,
        _picker.preferredHeight)

    var gap = 2
    var total = _default.intrinsicWidth + _accept.intrinsicWidth +
        _cancel.intrinsicWidth + gap * 2
    var buttonX = x + ((width - total) / 2).floor.max(0)
    var y = rect.y + preferredHeight - 1
    _default.bounds = Rect.new(buttonX, y, _default.intrinsicWidth, 1)
    buttonX = buttonX + _default.intrinsicWidth + gap
    _accept.bounds = Rect.new(buttonX, y, _accept.intrinsicWidth, 1)
    buttonX = buttonX + _accept.intrinsicWidth + gap
    _cancel.bounds = Rect.new(buttonX, y, _cancel.intrinsicWidth, 1)
  }

  onPaint_() {
    Painter.fill(surface, Rect.new(0, 0, bounds.w, bounds.h), " ",
        style("default"))
    compositeChildren_()
  }

  handle(event) {
    var previous = focusedIndex
    var handled = super.handle(event)
    if (handled && event is KeyEvent && focusedChild == _picker &&
        previous != 1) {
      if (event.code == Key.tab) {
        _picker.focusedIndex = _picker.firstFocusIndex
      } else if (event.code == Key.backTab || event.code == Key.up) {
        _picker.focusedIndex = _picker.lastFocusIndex
      }
    }
    if (!handled && event is KeyEvent) {
      if (event.code == Key.home && focusedChild == _picker) {
        _picker.focusedIndex = _picker.firstFocusIndex
        return true
      }
      if (event.code == Key.end && focusedChild == _picker) {
        focusedIndex = 2
        return true
      }
      if (event.code == Key.up && focusedChild == _picker) {
        focusedIndex = children.count - 1
        return true
      }
      if (event.code == Key.down && focusedChild != _picker) {
        focusedIndex = 1
        _picker.focusedIndex = _picker.firstFocusIndex
        return true
      }
    }
    return handled
  }
}

class PalettePickerPane is Pane {
  construct new(content, onDefault, onAccept, onCancel) {
    super()
    shadow = true
    keyHints = [["F1", "Help"], ["Enter", "Accept"],
        ["\%", "Default"], ["Esc", "Cancel"]]
    _default = onDefault
    _accept = onAccept
    _cancel = onCancel
    add(content)
  }

  handle(event) {
    if (event is KeyEvent && event.code == Key.escape) {
      _cancel.call()
      return true
    }
    if (event is KeyEvent && event.codepoint == 0x25) {
      _default.call()
      return true
    }
    if (event is KeyEvent && event.code == Key.enter) {
      if (super.handle(event)) return true
      _accept.call()
      return true
    }
    return super.handle(event)
  }
}

class PaletteColorPicker {
  static choose(app, color, defaultColor, colors, entry, helpText) {
    var picker = ColorPicker.new(color)
    var contrast = PaletteContrastPreview.new(colors, entry, color)
    picker.onChange = Fn.new {|value| contrast.color = value }
    contrast.onSample = Fn.new {|foreground, background|
      picker.sampleColors(foreground, background)
    }

    var result = null
    var close = Fn.new {|accepted|
      if (accepted && !picker.commitEdits()) return
      if (accepted) result = picker.value
      app.popModal()
    }
    var reset = Fn.new { picker.value = defaultColor }
    var content = PalettePickerContent.new(picker, contrast, reset,
        Fn.new { close.call(true) }, Fn.new { close.call(false) })
    var pane = PalettePickerPane.new(content, reset,
        Fn.new { close.call(true) }, Fn.new { close.call(false) })
    pane.title = "Edit Palette Entry"
    pane.helpText = helpText
    pane.focused = true
    pane.onClose = Fn.new { close.call(false) }
    pane.fitContentToScreen()
    app.modal(pane)
    return result
  }
}
