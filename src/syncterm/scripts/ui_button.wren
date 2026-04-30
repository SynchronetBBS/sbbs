// SyncTERM Wren UI library — Button.
//
// A clickable / Enter-activatable label widget.  Renders as a single
// row "[ label ]"; the brackets are part of the visual.  Focus drives
// styling: focused buttons use the lightbar palette, unfocused are
// drawn in the default text style so the label still reads.
//
//   var b = Button.new("OK")
//   b.bounds  = Rect.new(x, y, b.intrinsicWidth, 1)
//   b.onPress = Fn.new { app.popModal() }
//
// Theme roles consulted:
//   button         — unfocused
//   button.focused — focused (lightbar)
//
// Activation:
//   Enter / Space   — fires onPress.
//   Mouse click     — fires onPress (only when the click lands on the
//                     button's bounds).
//
// Buttons are focusable so Container's Tab/BackTab traversal cycles
// through them.

import "ui_widget" for Widget, Rect
import "ui_draw"   for Painter
import "syncterm"  for KeyEvent, MouseEvent, Mouse, Key

class Button is Widget {
  construct new(label) {
    super()
    _label    = label
    _onPress  = null
    _hotkeyIdx = 0      // 0-based label index to highlight; -1 = none
  }

  label     { _label  }
  label=(s) {
    _label = s
    markDirty()
  }
  onPress=(fn) { _onPress = fn }

  // Index into label of the visually-highlighted hotkey letter.
  // Default 0 (first letter) matches the conventional menu look —
  // even when no actual key shortcut binds it, the colored letter
  // tells users where to look.  -1 disables highlighting.
  hotkeyIdx     { _hotkeyIdx }
  hotkeyIdx=(i) {
    _hotkeyIdx = i
    markDirty()
  }

  // Cell budget for "[ label ]".
  intrinsicWidth { _label.count + 4 }

  // Buttons don't host a cursor — let App hide it while focused.
  cursorVisible { false }

  onPaint_() {
    var sf = surface
    var st = focused ? style("button.focused") : style("button")
    Painter.fill(sf, Rect.new(0, 0, bounds.w, bounds.h), " ", st)
    var text = "[ " + _label + " ]"
    if (text.count > bounds.w) text = text[0...bounds.w]
    var col = ((bounds.w - text.count) / 2).floor
    if (col < 0) col = 0
    Painter.text(sf, col, 0, text, st, bounds.w)
    // Re-paint the hotkey cell on top with the hotkey style.  Cell
    // is at col + 2 + idx (label starts after "[ ").
    if (_hotkeyIdx >= 0 && _hotkeyIdx < _label.count) {
      var hkCol = col + 2 + _hotkeyIdx
      if (hkCol >= 0 && hkCol < bounds.w) {
        var hkRole  = focused ? "button.focused.hotkey" : "button.hotkey"
        var hkStyle = style(hkRole)
        var hkChar  = _label[_hotkeyIdx]
        Painter.text(sf, hkCol, 0, hkChar, hkStyle, 1)
      }
    }
  }

  activate_() {
    if (_onPress != null) _onPress.call()
  }

  // Parent-driven hotkey: if the typed codepoint matches our
  // highlighted label letter (case-insensitive ASCII), activate.
  // Container.handle calls this for each visible child after the
  // focused child fails to consume the key.
  tryHotkey(ev) {
    if (!(ev is KeyEvent)) return false
    if (_hotkeyIdx < 0 || _hotkeyIdx >= _label.count) return false
    var cp = ev.codepoint
    if (cp == null) return false
    var hkChar = _label[_hotkeyIdx]
    if (hkChar.bytes.count != 1) return false
    var hkB = hkChar.bytes[0]
    if (hkB >= 0x61 && hkB <= 0x7A) hkB = hkB - 0x20
    var cpU = cp
    if (cpU >= 0x61 && cpU <= 0x7A) cpU = cpU - 0x20
    if (cpU != hkB) return false
    activate_()
    return true
  }

  handle(ev) {
    if (ev is KeyEvent) {
      var c  = ev.code
      var cp = ev.codepoint
      // Enter or Space activates a focused button.
      if (c == Key.enter || cp == 0x20) {
        activate_()
        return true
      }
      return false
    }
    if (ev is MouseEvent) {
      if (bounds == null || !bounds.contains(ev.startX, ev.startY)) return false
      if (ev.event == Mouse.button1Click) {
        activate_()
        return true
      }
      return false
    }
    return false
  }
}
