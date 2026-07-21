// SyncTERM Wren UI library - RGB/HSV colour picker.
//
// ColorPicker is an embeddable control.  It owns a live swatch, three
// editable RGB ramps, three visual HSV ramps, and a hexadecimal input.
// The caller owns dialog policy such as Default / OK / Cancel buttons.

import "syncterm" for Color, Key, KeyEvent, Mouse, MouseEvent
import "ui_widget" for Container, Rect, Widget
import "ui_input" for SelectOnFocusInput
import "ui_draw" for Painter
import "ui_style" for Style

class ColorModel {
  static component(rgb, shift) { (rgb >> shift) & 0xFF }

  static pack(r, g, b) {
    return ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF)
  }

  static contrast(rgb) {
    var r = component(rgb, 16)
    var g = component(rgb, 8)
    var b = component(rgb, 0)
    return r * 299 + g * 587 + b * 114 >= 128000 ? 0x000000 : 0xFFFFFF
  }

  static hex(rgb) {
    var digits = "0123456789ABCDEF"
    var out = ""
    for (shift in [20, 16, 12, 8, 4, 0]) {
      out = out + digits[(rgb >> shift) & 0x0F]
    }
    return out
  }

  static hexNibble(codepoint) {
    if (codepoint >= 0x30 && codepoint <= 0x39) return codepoint - 0x30
    if (codepoint >= 0x41 && codepoint <= 0x46) return codepoint - 0x41 + 10
    if (codepoint >= 0x61 && codepoint <= 0x66) return codepoint - 0x61 + 10
    return null
  }

  static parseHex(text) {
    var bytes = text.bytes
    if (bytes.count != 6) return null
    var value = 0
    for (byte in bytes) {
      var nibble = hexNibble(byte)
      if (nibble == null) return null
      value = (value << 4) | nibble
    }
    return value
  }

  // Return integer [hue 0..359, saturation 0..100, value 0..100].
  // Hue is undefined for greys and hue/saturation are undefined for
  // black, so retain the supplied values in those cases.
  static toHsv(rgb, retainedHue, retainedSaturation) {
    var r = component(rgb, 16) / 255
    var g = component(rgb, 8) / 255
    var b = component(rgb, 0) / 255
    var high = r.max(g).max(b)
    var low = r.min(g).min(b)
    var delta = high - low
    var hue = retainedHue == null ? 0 : retainedHue
    var saturation = retainedSaturation == null ? 0 : retainedSaturation
    var value = (high * 100 + 0.5).floor

    if (high == 0) return [hue, saturation, 0]
    saturation = (delta * 100 / high + 0.5).floor
    if (delta == 0) return [hue, 0, value]

    if (high == r) {
      hue = 60 * ((g - b) / delta)
    } else if (high == g) {
      hue = 60 * (((b - r) / delta) + 2)
    } else {
      hue = 60 * (((r - g) / delta) + 4)
    }
    if (hue < 0) hue = hue + 360
    hue = (hue + 0.5).floor % 360
    return [hue, saturation, value]
  }

  static fromHsv(hue, saturation, value) {
    var h = ((hue % 360) + 360) % 360
    var s = saturation.max(0).min(100) / 100
    var v = value.max(0).min(100) / 100
    var chroma = v * s
    var sector = h / 60
    var x = chroma * (1 - ((sector % 2) - 1).abs)
    var r = 0
    var g = 0
    var b = 0
    if (sector < 1) {
      r = chroma
      g = x
    } else if (sector < 2) {
      r = x
      g = chroma
    } else if (sector < 3) {
      g = chroma
      b = x
    } else if (sector < 4) {
      g = x
      b = chroma
    } else if (sector < 5) {
      r = x
      b = chroma
    } else {
      r = chroma
      b = x
    }
    var m = v - chroma
    return pack(((r + m) * 255 + 0.5).floor,
        ((g + m) * 255 + 0.5).floor,
        ((b + m) * 255 + 0.5).floor)
  }
}

class ColorSwatch is Widget {
  construct new(rgb) {
    super()
    focusable = false
    _value = rgb & 0xFFFFFF
    _foreground = ColorModel.contrast(_value)
    _background = _value
  }

  value { _value }
  value=(rgb) {
    var next = rgb & 0xFFFFFF
    if (next == _value) return
    _value = next
    _foreground = ColorModel.contrast(_value)
    _background = _value
    markDirty()
  }

  colors(foreground, background) {
    _foreground = foreground & 0xFFFFFF
    _background = background & 0xFFFFFF
    markDirty()
  }

  onPaint_() {
    var base = style("default")
    Painter.fill(surface, Rect.new(0, 0, bounds.w, bounds.h), " ", base)
    if (bounds.h < 1) return
    var sample = Style.new(base.font,
        Color.toLegacyAttr(_foreground, _background),
        _foreground, _background)
    var text = "Aa Bb Cc 0123"
    var width = (text.count + 6).min(bounds.w)
    var x = ((bounds.w - width) / 2).floor.max(0)
    var height = bounds.h.min(3)
    Painter.fill(surface, Rect.new(x, 0, width, height), " ", sample)
    var y = (height / 2).floor
    Painter.text(surface, x + ((width - text.count) / 2).floor, y,
        text, sample, width)
  }
}

class ColorRamp is Widget {
  construct new(label, maximum, pageStep, editable, sampler) {
    super()
    _label = label
    _maximum = maximum
    _pageStep = pageStep
    _editable = editable
    _sampler = sampler
    _value = 0
    _onChange = null
    _typedDigits = 0
  }

  value { _value }
  value=(number) { setValue_(number, false) }
  maximum { _maximum }
  editable { _editable }
  onChange=(fn) { _onChange = fn }
  preferredHeight { 1 }

  focused=(value) {
    if (value && !focused) _typedDigits = 0
    super.focused = value
  }

  setValue_(number, notify) {
    var next = number.max(0).min(_maximum).floor
    if (next == _value) return
    _value = next
    markDirty()
    if (notify && _onChange != null) _onChange.call(_value)
  }

  refreshRamp() { markDirty() }
  resetTyping() { _typedDigits = 0 }

  barStart_ { 4 }
  barWidth_ { (bounds.w - (_editable ? 10 : 6)).max(1) }
  barEnd_ { barStart_ + barWidth_ - 1 }

  marker_() {
    if (_maximum <= 0 || barWidth_ <= 1) return 0
    return ((_value * (barWidth_ - 1) / _maximum) + 0.5).floor
  }

  valueAt_(column) {
    if (_maximum <= 0 || barWidth_ <= 1) return 0
    var offset = (column - barStart_).max(0).min(barWidth_ - 1)
    return ((offset * _maximum / (barWidth_ - 1)) + 0.5).floor
  }

  focusSelf_() {
    var child = this
    var owner = parent
    while (owner is Container) {
      var index = owner.children.indexOf(child)
      if (index >= 0) owner.focusedIndex = index
      child = owner
      owner = owner.parent
    }
  }

  change_(number) {
    _typedDigits = 0
    setValue_(number, true)
  }

  onPaint_() {
    var row = focused ? style("list.item.focused") : style("default")
    Painter.fill(surface, Rect.new(0, 0, bounds.w, bounds.h), " ", row)
    Painter.text(surface, 1, 0, _label, row, 1)
    Painter.text(surface, 3, 0, "[", row, 1)
    Painter.text(surface, barEnd_ + 1, 0, "]", row, 1)

    var marker = marker_()
    var base = style("default")
    for (i in 0...barWidth_) {
      var sampleValue = barWidth_ <= 1 ? _value :
          ((i * _maximum / (barWidth_ - 1)) + 0.5).floor
      var bg = _sampler.call(sampleValue)
      var fg = ColorModel.contrast(bg)
      var rampStyle = Style.new(base.font, Color.toLegacyAttr(fg, bg), fg, bg)
      Painter.text(surface, barStart_ + i, 0, i == marker ? "|" : " ",
          rampStyle, 1)
    }

    if (_editable) {
      var input = focused ? style("input.focused") : style("input")
      var text = _value.toString
      var valueX = bounds.w - 4
      Painter.fill(surface, Rect.new(valueX, 0, 3, 1), " ", input)
      Painter.text(surface, valueX + 3 - text.count, 0, text, input, 3)
    }
  }

  handle(event) {
    if (event is MouseEvent) return handleMouse_(event)
    if (!(event is KeyEvent)) return false
    var code = event.code
    if (code == Key.left) {
      change_(_value - 1)
      return true
    }
    if (code == Key.right) {
      change_(_value + 1)
      return true
    }
    if (code == Key.pageUp) {
      change_(_value + _pageStep)
      return true
    }
    if (code == Key.pageDown) {
      change_(_value - _pageStep)
      return true
    }
    if (_editable && (code == Key.delete || code == Key.delChar)) {
      _typedDigits = 0
      return true
    }
    if (_editable && code == Key.backspace) {
      setValue_((_value / 10).floor, true)
      if (_typedDigits > 0) _typedDigits = _typedDigits - 1
      return true
    }
    var cp = event.codepoint
    if (_editable && cp != null && cp >= 0x30 && cp <= 0x39) {
      var digit = cp - 0x30
      var next = _typedDigits == 0 ? digit : _value * 10 + digit
      var maximumDigits = _maximum.toString.count
      if (_typedDigits >= maximumDigits || next > _maximum) {
        next = digit
        _typedDigits = 1
      } else {
        _typedDigits = _typedDigits + 1
      }
      setValue_(next, true)
      return true
    }
    return false
  }

  handleMouse_(event) {
    if (bounds == null || !bounds.contains(event.startX, event.startY)) {
      return false
    }
    if (event.event == Mouse.wheelUpPress ||
        event.event == Mouse.wheelUpClick) {
      focusSelf_()
      change_(_value + 1)
      return true
    }
    if (event.event == Mouse.wheelDownPress ||
        event.event == Mouse.wheelDownClick) {
      focusSelf_()
      change_(_value - 1)
      return true
    }
    // Press and drag events deliberately fall through to App's text
    // selection path.  Only a completed click changes the ramp.
    if (event.event != Mouse.button1Click) return false
    focusSelf_()
    var x = event.endX - bounds.x
    if (x >= barStart_ && x <= barEnd_) change_(valueAt_(x))
    return true
  }
}

class ColorHexInput is SelectOnFocusInput {
  construct new(onLeave) {
    super()
    maxLen = 6
    _onLeave = onLeave
  }

  valid { ColorModel.parseHex(value) != null }
  parsed { ColorModel.parseHex(value) }
  paintStyle_ { focused ? style("input.focused") : style("input") }
  fillStyle_ { paintStyle_ }

  commitEdit() {
    if (!valid) return false
    value = ColorModel.hex(parsed)
    return true
  }

  focused=(value) {
    if (!value && focused && _onLeave != null) _onLeave.call()
    super.focused = value
  }

  focusSelf_() {
    var child = this
    var owner = parent
    while (owner is Container) {
      var index = owner.children.indexOf(child)
      if (index >= 0) owner.focusedIndex = index
      child = owner
      owner = owner.parent
    }
  }

  handle(event) {
    if (event is MouseEvent && event.event == Mouse.button1Click &&
        bounds != null && bounds.contains(event.startX, event.startY)) {
      focusSelf_()
    }
    if (event is KeyEvent && event.code == Key.enter) {
      if (!commitEdit()) return true
      return false
    }
    if (event is KeyEvent && event.codepoint != null &&
        event.codepoint >= 0x20 &&
        ColorModel.hexNibble(event.codepoint) == null) return true
    return super.handle(event)
  }
}

class ColorPicker is Container {
  construct new(rgb) {
    super()
    _value = rgb & 0xFFFFFF
    _onChange = null
    var hsv = ColorModel.toHsv(_value, 0, 0)
    _hue = hsv[0]
    _saturation = hsv[1]
    _brightness = hsv[2]

    _swatch = ColorSwatch.new(_value)
    _red = ColorRamp.new("R", 255, 16, true,
        Fn.new {|value| rgbRampColor_(0, value) })
    _green = ColorRamp.new("G", 255, 16, true,
        Fn.new {|value| rgbRampColor_(1, value) })
    _blue = ColorRamp.new("B", 255, 16, true,
        Fn.new {|value| rgbRampColor_(2, value) })
    _hueRamp = ColorRamp.new("H", 359, 15, false,
        Fn.new {|value| ColorModel.fromHsv(value, 100, 100) })
    _saturationRamp = ColorRamp.new("S", 100, 10, false,
        Fn.new {|value| ColorModel.fromHsv(_hue, value, 100) })
    _valueRamp = ColorRamp.new("V", 100, 10, false,
        Fn.new {|value| ColorModel.fromHsv(_hue, _saturation, value) })
    _hex = ColorHexInput.new(Fn.new { restoreHex_() })

    add(_swatch)
    add(_red)
    add(_green)
    add(_blue)
    add(_hueRamp)
    add(_saturationRamp)
    add(_valueRamp)
    add(_hex)

    _red.onChange = Fn.new {|value| rgbChanged_(0, value) }
    _green.onChange = Fn.new {|value| rgbChanged_(1, value) }
    _blue.onChange = Fn.new {|value| rgbChanged_(2, value) }
    _hueRamp.onChange = Fn.new {|value| hsvChanged_(0, value) }
    _saturationRamp.onChange = Fn.new {|value| hsvChanged_(1, value) }
    _valueRamp.onChange = Fn.new {|value| hsvChanged_(2, value) }
    _hex.onChange = Fn.new {|text| hexChanged_(text) }
    syncChildren_()
  }

  preferredWidth { 44 }
  preferredHeight { 10 }
  firstFocusIndex { 1 }
  lastFocusIndex { 7 }

  focused=(value) {
    super.focused = value
    var child = focusedChild
    if (child != null) child.focused = value
  }

  value { _value }
  value=(rgb) {
    setRgb_(rgb, true)
    for (ramp in [_red, _green, _blue]) ramp.resetTyping()
  }
  onChange=(fn) { _onChange = fn }
  valid { _hex.valid }

  sampleColors(foreground, background) {
    _swatch.colors(foreground, background)
  }

  commitEdits() {
    if (!_hex.commitEdit()) return false
    return true
  }

  bounds=(rect) {
    super.bounds = rect
    var y = rect.y
    _swatch.bounds = Rect.new(rect.x, y, rect.w, 3)
    y = y + 3
    for (ramp in [_red, _green, _blue, _hueRamp,
        _saturationRamp, _valueRamp]) {
      ramp.bounds = Rect.new(rect.x, y, rect.w, 1)
      y = y + 1
    }
    _hex.bounds = Rect.new(rect.x + 7, y,
        7.min((rect.w - 8).max(1)), 1)
  }

  handle(event) {
    if (event is KeyEvent && event.code == Key.tab &&
        focusedIndex == lastFocusIndex) return false
    if (event is KeyEvent && event.code == Key.backTab &&
        focusedIndex == firstFocusIndex) return false
    return super.handle(event)
  }

  onPaint_() {
    Painter.fill(surface, Rect.new(0, 0, bounds.w, bounds.h), " ",
        style("default"))
    Painter.text(surface, 1, 9, "Hex  #", style("default"),
        (bounds.w - 2).max(0).min(6))
    compositeChildren_()
  }

  rgbRampColor_(component, value) {
    var r = ColorModel.component(_value, 16)
    var g = ColorModel.component(_value, 8)
    var b = ColorModel.component(_value, 0)
    if (component == 0) r = value
    if (component == 1) g = value
    if (component == 2) b = value
    return ColorModel.pack(r, g, b)
  }

  rgbChanged_(component, number) {
    setRgb_(rgbRampColor_(component, number), true)
  }

  hsvChanged_(component, number) {
    if (component == 0) _hue = number
    if (component == 1) _saturation = number
    if (component == 2) _brightness = number
    var next = ColorModel.fromHsv(_hue, _saturation, _brightness)
    if (next == _value) {
      syncChildren_()
      return
    }
    _value = next
    syncChildren_()
    notifyChange_()
  }

  hexChanged_(text) {
    var parsed = ColorModel.parseHex(text)
    if (parsed != null) setRgb_(parsed, true)
  }

  restoreHex_() {
    if (!_hex.valid) _hex.value = ColorModel.hex(_value)
  }

  setRgb_(rgb, notify) {
    var next = rgb.floor & 0xFFFFFF
    if (next == _value) {
      syncChildren_()
      return
    }
    _value = next
    var hsv = ColorModel.toHsv(_value, _hue, _saturation)
    _hue = hsv[0]
    _saturation = hsv[1]
    _brightness = hsv[2]
    syncChildren_()
    if (notify) notifyChange_()
  }

  syncChildren_() {
    _swatch.value = _value
    _red.value = ColorModel.component(_value, 16)
    _green.value = ColorModel.component(_value, 8)
    _blue.value = ColorModel.component(_value, 0)
    _hueRamp.value = _hue
    _saturationRamp.value = _saturation
    _valueRamp.value = _brightness
    _hex.value = ColorModel.hex(_value)
    for (ramp in [_red, _green, _blue, _hueRamp,
        _saturationRamp, _valueRamp]) ramp.refreshRamp()
    markDirty()
  }

  notifyChange_() {
    if (_onChange != null) _onChange.call(_value)
  }
}
