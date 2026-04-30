// SyncTERM Wren UI library — SpinBox.
//
// Single-row numeric input with up/down step buttons.  Layout:
//
//   [   42  ▲▼]
//
// The value sits left-aligned in the body cells; the last two cells
// host stacked up/down arrows.  Total widget width is at least 5
// (1 for value + 2 brackets + 2 arrows).
//
//   var s = SpinBox.new()
//   s.bounds   = Rect.new(x, y, w, 1)
//   s.min      = 0
//   s.max      = 100
//   s.step     = 1
//   s.value    = 50
//   s.onChange = Fn.new {|v| ... }
//
// Keys (when focused):
//   Up / +              value += step
//   Down / -            value -= step
//   PageUp / PageDown   value ± step * 10
//   Home / End          jump to min / max
// Mouse:
//   Click on ▲          value += step
//   Click on ▼          value -= step
//   Wheel up / down     value ± step
//
// Theme roles consulted:
//   spinbox          — unfocused
//   spinbox.focused  — focused
// Glyphs reused: scrollbar.up (`▲`), scrollbar.down (`▼`).

import "ui_widget" for Widget, Rect
import "ui_draw"   for Painter
import "syncterm"  for KeyEvent, MouseEvent, Key, Mouse

class SpinBox is Widget {
  construct new() {
    super()
    _value    = 0
    _min      = 0
    _max      = 100
    _step     = 1
    _onChange = null
  }

  value     { _value }
  value=(v) {
    var clamped = v.max(_min).min(_max)
    if (clamped == _value) return
    _value = clamped
    if (_onChange != null) _onChange.call(_value)
    markDirty()
  }

  min     { _min }
  min=(n) {
    _min = n
    if (_value < _min) value = _min
    markDirty()
  }
  max     { _max }
  max=(n) {
    _max = n
    if (_value > _max) value = _max
    markDirty()
  }

  step     { _step }
  step=(n) { _step = n }

  onChange=(fn) { _onChange = fn }

  cursorVisible { false }

  // Surface columns of the up / down arrows.  Right-aligned in the
  // bracketed body so the value gets all the room before them.
  upArrowX_   { bounds.w - 3 }
  downArrowX_ { bounds.w - 2 }

  onPaint_() {
    var sf = surface
    var st = focused ? style("spinbox.focused") : style("spinbox")
    Painter.fill(sf, Rect.new(0, 0, bounds.w, bounds.h), " ", st)
    if (bounds.w < 5) return
    Painter.text(sf, 0, 0, "[", st, 1)
    Painter.text(sf, bounds.w - 1, 0, "]", st, 1)
    var s = _value.toString
    var bodyW = bounds.w - 4   // brackets(2) + arrows(2)
    if (s.count > bodyW) s = s[0...bodyW]
    var col = 1 + ((bodyW - s.count) / 2).floor
    if (col < 1) col = 1
    Painter.text(sf, col, 0, s, st, bodyW)
    Painter.text(sf, upArrowX_,   0, glyph("scrollbar.up"),   st, 1)
    Painter.text(sf, downArrowX_, 0, glyph("scrollbar.down"), st, 1)
  }

  bump_(delta) { value = _value + delta }

  handle(ev) {
    if (ev is KeyEvent) return handleKey_(ev)
    if (ev is MouseEvent) return handleMouse_(ev)
    return false
  }

  handleKey_(ke) {
    var c  = ke.code
    var cp = ke.codepoint
    if (c == Key.up   || cp == 0x2B) {
      bump_(_step)
      return true
    }
    if (c == Key.down || cp == 0x2D) {
      bump_(-_step)
      return true
    }
    if (c == Key.pageUp) {
      bump_(_step * 10)
      return true
    }
    if (c == Key.pageDown) {
      bump_(-_step * 10)
      return true
    }
    if (c == Key.home) {
      value = _min
      return true
    }
    if (c == Key.end) {
      value = _max
      return true
    }
    return false
  }

  handleMouse_(me) {
    if (bounds == null) return false
    if (!bounds.contains(me.startX, me.startY)) return false
    var e = me.event
    if (e == Mouse.wheelUpPress || e == Mouse.wheelUpClick) {
      bump_(_step)
      return true
    }
    if (e == Mouse.wheelDownPress || e == Mouse.wheelDownClick) {
      bump_(-_step)
      return true
    }
    if (e != Mouse.button1Click && e != Mouse.button1Press) return false
    var lx = me.startX - bounds.x
    if (lx == upArrowX_) {
      bump_(_step)
      return true
    }
    if (lx == downArrowX_) {
      bump_(-_step)
      return true
    }
    return false
  }
}
