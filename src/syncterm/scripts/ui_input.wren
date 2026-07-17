// SyncTERM Wren UI library — TextInput.
//
// Single-line text input.  Stores the value as a List of codepoint
// strings so the cursor can index by codepoint without paying for a
// String rebuild per character.  `value` joins on demand.
//
// Key handling: Left/Right/Home/End move the cursor; Backspace/Delete
// edit; Enter fires onSubmit (set by the caller); printable codepoints
// insert at the cursor.  Mouse clicks position the cursor at the
// clicked column.  Tab/BackTab/Up/Down are NOT consumed so containers
// can use them for focus traversal.
//
// Cursor exposure: cursorVisible returns true (text input wants the
// hardware cursor on screen) and cursorPos reports the cell that
// corresponds to the current insertion point in screen coords.  App
// reads these each frame and applies them.
//
// Theme roles consulted:
//   input          — unfocused border / background
//   input.focused  — focused
// Glyph: scrollbar.thumb is reused for the off-screen overflow tick.
//
// Construction:
//   var t = TextInput.new()
//   t.bounds   = Rect.new(x, y, w, 1)
//   t.value    = "hello"
//   t.maxLen   = 64                // optional cap, default null = no cap
//   t.onSubmit = Fn.new {|s| ... } // optional Enter callback
//   t.onChange = Fn.new {|s| ... } // optional change callback
//
// `value` and `value=` both round-trip a String; the cursor lands at
// the end on assignment.

import "ui_widget" for Widget, Rect
import "ui_draw"   for Painter
import "syncterm"  for KeyEvent, MouseEvent, Key, Mouse

class TextInput is Widget {
  construct new() {
    super()
    _chars     = []        // List<String>; one codepoint per element
    _cursor    = 0         // codepoint index in [0.._chars.count]
    _scrollOff = 0         // first visible codepoint index
    _maxLen    = null
    _onSubmit  = null
    _onChange  = null
    _mask      = null
  }

  // String round-trip.  Setting value resets the cursor to the end
  // and triggers a repaint.
  value { _chars.join() }
  value=(s) {
    _chars = []
    for (c in s) _chars.add(c)
    _cursor    = _chars.count
    _scrollOff = 0
    ensureVisible_()
    markDirty()
  }

  // Codepoint count (visible characters), independent of byte width.
  count { _chars.count }

  cursor { _cursor }
  cursor=(i) {
    var clamped = i.max(0).min(_chars.count)
    if (clamped == _cursor) return
    _cursor = clamped
    ensureVisible_()
    markDirty()
  }

  maxLen     { _maxLen }
  maxLen=(n) { _maxLen = n }

  onSubmit=(fn) { _onSubmit = fn }
  onChange=(fn) { _onChange = fn }

  // Optional one-codepoint display mask.  The underlying value and all
  // edit callbacks continue to use the original text.
  mask { _mask }
  mask=(s) {
    if (s == null) {
      _mask = null
      markDirty()
      return
    }
    var first = null
    for (c in s) {
      first = c
      break
    }
    if (first == null) return
    _mask = first
    markDirty()
  }

  scrollOff { _scrollOff }

  // Adjust _scrollOff so the cursor is on screen.  `bounds.w` cells
  // are visible; cursor at column 0 is the first visible character,
  // cursor at column bounds.w-1 is the last.  Inserting past the
  // right edge bumps the scroll one cell at a time.
  ensureVisible_() {
    if (bounds == null) return
    var w = bounds.w
    if (w <= 0) return
    if (_cursor < _scrollOff) {
      _scrollOff = _cursor
    } else if (_cursor >= _scrollOff + w) {
      _scrollOff = _cursor - w + 1
    }
    if (_scrollOff < 0) _scrollOff = 0
  }

  // ----- Cursor exposure for App ----------------------------------

  cursorVisible { true }
  cursorPos {
    if (bounds == null) return null
    var col = _cursor - _scrollOff
    if (col < 0) col = 0
    if (col >= bounds.w) col = bounds.w - 1
    return [bounds.x + col, bounds.y]
  }

  // ----- Painting --------------------------------------------------

  onPaint_() {
    var sf  = surface
    var st  = focused ? style("input.focused") : style("input")
    Painter.fill(sf, Rect.new(0, 0, bounds.w, bounds.h), " ", st)
    var w = bounds.w
    var i = 0
    while (i < w && _scrollOff + i < _chars.count) {
      var ch = _mask == null ? _chars[_scrollOff + i] : _mask
      Painter.text(sf, i, 0, ch, st, 1)
      i = i + 1
    }
  }

  // ----- Edit primitives ------------------------------------------

  insert_(s) {
    if (_maxLen != null && _chars.count >= _maxLen) return
    _chars.insert(_cursor, s)
    _cursor = _cursor + 1
    ensureVisible_()
    if (_onChange != null) _onChange.call(value)
    markDirty()
  }

  backspace_() {
    if (_cursor == 0) return
    _chars.removeAt(_cursor - 1)
    _cursor = _cursor - 1
    ensureVisible_()
    if (_onChange != null) _onChange.call(value)
    markDirty()
  }

  delete_() {
    if (_cursor >= _chars.count) return
    _chars.removeAt(_cursor)
    if (_onChange != null) _onChange.call(value)
    markDirty()
  }

  // ----- Event handling -------------------------------------------

  handle(ev) {
    if (ev is KeyEvent) return handleKey_(ev)
    if (ev is MouseEvent) return handleMouse_(ev)
    return false
  }

  handleKey_(ke) {
    var c = ke.code
    if (c == Key.left) {
      cursor = _cursor - 1
      return true
    }
    if (c == Key.right) {
      cursor = _cursor + 1
      return true
    }
    if (c == Key.home) {
      cursor = 0
      return true
    }
    if (c == Key.end) {
      cursor = _chars.count
      return true
    }
    if (c == Key.backspace) {
      backspace_()
      return true
    }
    // Forward-delete arrives as either Key.delete (0x5300, dedicated
    // keypad scancode) or Key.delChar (0x7F, ASCII DEL).  Which one
    // a given terminal emits depends on its keymap; accept both.
    if (c == Key.delete || c == Key.delChar) {
      delete_()
      return true
    }
    if (c == Key.enter) {
      if (_onSubmit != null) _onSubmit.call(value)
      return true
    }
    // Printable codepoint?  ke.codepoint is null for extended keys
    // (arrows, F-keys, etc.) and a Num for everything else.  Filter
    // null first, then drop control codes (< 0x20 or 0x7F) so
    // containers can use those for focus traversal / cancel.
    var cp = ke.codepoint
    if (cp != null && cp >= 0x20 && cp != 0x7F) {
      insert_(ke.text)
      return true
    }
    return false
  }

  handleMouse_(me) {
    if (bounds == null) return false
    if (!bounds.contains(me.startX, me.startY)) return false
    var e = me.event
    if (e != Mouse.button1Press && e != Mouse.button1Click &&
        e != Mouse.button1DragStart && e != Mouse.button1DragMove) {
      return false
    }
    var col = me.endX - bounds.x
    cursor = (_scrollOff + col).max(0).min(_chars.count)
    return true
  }
}
