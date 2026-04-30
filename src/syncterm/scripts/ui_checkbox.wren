// SyncTERM Wren UI library — Checkbox.
//
// Single-row "[X] Label" toggle.  Space or Enter (when focused)
// flips the value; mouse click anywhere on the widget toggles too.
// `value` is a Bool, `label` is a String, `onChange` is a function
// called with the new Bool whenever the value changes.
//
//   var c = Checkbox.new("Show shadows")
//   c.bounds   = Rect.new(x, y, c.intrinsicWidth, 1)
//   c.value    = true
//   c.onChange = Fn.new {|v| ... }
//
// Theme roles consulted:
//   checkbox          — unfocused
//   checkbox.focused  — focused
// Glyphs reused: check.on (`■`), check.off (` `).

import "ui_widget" for Widget, Rect
import "ui_draw"   for Painter
import "syncterm"  for KeyEvent, MouseEvent, Key, Mouse

class Checkbox is Widget {
  construct new(label) {
    super()
    _label    = label
    _value    = false
    _onChange = null
  }

  label     { _label }
  label=(s) {
    _label = s
    markDirty()
  }

  value     { _value }
  value=(b) {
    if (b == _value) return
    _value = b
    if (_onChange != null) _onChange.call(_value)
    markDirty()
  }

  onChange=(fn) { _onChange = fn }

  // "[X] " + label
  intrinsicWidth { _label.count + 4 }

  cursorVisible { false }

  toggle_() { value = !_value }

  onPaint_() {
    var sf = surface
    var st = focused ? style("checkbox.focused") : style("checkbox")
    Painter.fill(sf, Rect.new(0, 0, bounds.w, bounds.h), " ", st)
    var g  = _value ? glyph("check.on") : glyph("check.off")
    Painter.text(sf, 0, 0, "[", st, 1)
    Painter.text(sf, 1, 0, g,   st, 1)
    Painter.text(sf, 2, 0, "]", st, 1)
    if (bounds.w > 4) {
      Painter.text(sf, 4, 0, _label, st, bounds.w - 4)
    }
  }

  handle(ev) {
    if (ev is KeyEvent) {
      var c  = ev.code
      var cp = ev.codepoint
      if (c == Key.enter || cp == 0x20) {
        toggle_()
        return true
      }
      return false
    }
    if (ev is MouseEvent) {
      if (bounds == null || !bounds.contains(ev.startX, ev.startY)) return false
      if (ev.event == Mouse.button1Click) {
        toggle_()
        return true
      }
    }
    return false
  }
}
