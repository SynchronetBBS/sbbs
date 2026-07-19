// SyncTERM Wren UI library — TextInput.
//
// Single-line text input.  Stores the value as a List of codepoint
// strings so the cursor can index by codepoint without paying for a
// String rebuild per character.  `value` joins on demand.
//
// Key handling follows UIFC's line editor: movement, insert/overwrite,
// deletion, whole-field copy/cut/paste, and truncate-to-end controls.
// Enter fires onSubmit (set by the caller); printable codepoints edit at
// the cursor.  Left clicks position the cursor and middle clicks paste.
// Tab/BackTab/Up/Down are NOT consumed so containers can traverse focus.
//
// Cursor exposure: cursorVisible returns true (text input wants the
// hardware cursor on screen), cursorPos reports the insertion cell,
// and cursorAttr supplies the focused field foreground.  Insert mode
// uses the normal cursor and overwrite uses a solid block, matching
// SyncTERM's reversed UIFC cursor convention.  App applies all three.
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
import "syncterm"  for Clipboard, KeyEvent, MouseEvent, Key, Mouse

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
    _allSelected = false
  }

  static insertMode_ {
    if (__insertMode == null) __insertMode = true
    return __insertMode
  }
  static insertMode_=(value) { __insertMode = value }

  // Layout may assign bounds before or after value.  Rebuild the
  // viewport from the cursor whenever its width changes so both
  // orders show the same text, including the insertion cell after a
  // value whose end lies beyond the field.
  bounds=(r) {
    var widthChanged = bounds == null || bounds.w != r.w
    super.bounds = r
    if (widthChanged) {
      _scrollOff = 0
      ensureVisible_()
    }
  }

  // String round-trip.  Setting value resets the cursor to the end
  // and triggers a repaint.
  value { _chars.join() }
  value=(s) {
    _chars = []
    for (c in s) _chars.add(c)
    _cursor    = _chars.count
    _scrollOff = 0
    _allSelected = false
    ensureVisible_()
    markDirty()
  }

  // Codepoint count (visible characters), independent of byte width.
  count { _chars.count }

  cursor { _cursor }
  cursor=(i) {
    var clamped = i.max(0).min(_chars.count)
    var selectionChanged = clearSelection_()
    if (clamped == _cursor) return
    _cursor = clamped
    ensureVisible_()
    if (!selectionChanged) markDirty()
  }

  allSelected { _allSelected }
  selectAll() {
    _allSelected = _chars.count > 0
    _cursor = _chars.count
    ensureVisible_()
    markDirty()
  }

  maxLen     { _maxLen }
  maxLen=(n) { _maxLen = n }

  onSubmit=(fn) { _onSubmit = fn }
  onChange=(fn) { _onChange = fn }

  insertMode { TextInput.insertMode_ }
  insertMode=(value) {
    TextInput.insertMode_ = value
    markDirty()
  }

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
  cursorShape { insertMode ? "normal" : "solid" }
  cursorAttr { fillStyle_.legacyAttr }
  cursorPos {
    if (bounds == null) return null
    var col = _cursor - _scrollOff
    if (col < 0) col = 0
    if (col >= bounds.w) col = bounds.w - 1
    return [bounds.x + col, bounds.y]
  }

  // ----- Painting --------------------------------------------------

  paintStyle_ { focused ? style("input.focused") : style("input") }
  fillStyle_ { paintStyle_ }

  onPaint_() {
    var sf  = surface
    var st  = paintStyle_
    Painter.fill(sf, Rect.new(0, 0, bounds.w, bounds.h), " ", fillStyle_)
    var w = bounds.w
    var i = 0
    while (i < w && _scrollOff + i < _chars.count) {
      var ch = _mask == null ? _chars[_scrollOff + i] : _mask
      Painter.text(sf, i, 0, ch, st, 1)
      i = i + 1
    }
  }

  // ----- Edit primitives ------------------------------------------

  clearSelection_() {
    if (!_allSelected) return false
    _allSelected = false
    markDirty()
    return true
  }

  clearSelectedValue_() {
    if (!_allSelected) return false
    _allSelected = false
    _chars = []
    _cursor = 0
    _scrollOff = 0
    markDirty()
    return true
  }

  insertOne_(s) {
    if (!insertMode && _cursor < _chars.count) {
      _chars[_cursor] = s
      _cursor = _cursor + 1
      ensureVisible_()
      markDirty()
      return true
    }
    if (_maxLen != null && _chars.count >= _maxLen) return false
    _chars.insert(_cursor, s)
    _cursor = _cursor + 1
    ensureVisible_()
    markDirty()
    return true
  }

  insert_(s) {
    var changed = clearSelectedValue_()
    if (insertOne_(s)) changed = true
    if (changed && _onChange != null) _onChange.call(value)
  }

  insertText_(text) {
    if (text == null) return
    var changed = clearSelectedValue_()
    for (ch in text) {
      var bytes = ch.bytes
      if (bytes.count > 1 || (bytes[0] >= 0x20 && bytes[0] != 0x7F)) {
        if (insertOne_(ch)) changed = true
      }
    }
    if (changed && _onChange != null) _onChange.call(value)
  }

  backspace_() {
    if (clearSelectedValue_()) {
      if (_onChange != null) _onChange.call(value)
      return
    }
    if (_cursor == 0) return
    _chars.removeAt(_cursor - 1)
    _cursor = _cursor - 1
    ensureVisible_()
    if (_onChange != null) _onChange.call(value)
    markDirty()
  }

  delete_() {
    if (clearSelectedValue_()) {
      if (_onChange != null) _onChange.call(value)
      return
    }
    if (_cursor >= _chars.count) return
    _chars.removeAt(_cursor)
    if (_onChange != null) _onChange.call(value)
    markDirty()
  }

  copy_() {
    if (_chars.count > 0) Clipboard.text = value
  }

  cut_() {
    if (_chars.count == 0) return
    copy_()
    _allSelected = false
    _chars = []
    _cursor = 0
    _scrollOff = 0
    if (_onChange != null) _onChange.call(value)
    markDirty()
  }

  truncate_() {
    if (clearSelectedValue_()) {
      if (_onChange != null) _onChange.call(value)
      return
    }
    if (_cursor >= _chars.count) return
    while (_chars.count > _cursor) _chars.removeAt(-1)
    if (_onChange != null) _onChange.call(value)
    markDirty()
  }

  paste_() { insertText_(Clipboard.text) }

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
    if (c == Key.ctrlB) {
      cursor = 0
      return true
    }
    if (c == Key.ctrlE) {
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
    if (c == Key.insert) {
      clearSelection_()
      insertMode = !insertMode
      return true
    }
    if (c == Key.ctrlC || c == Key.ctrlIns) {
      copy_()
      clearSelection_()
      return true
    }
    if (c == Key.ctrlX || c == Key.shiftDel) {
      cut_()
      return true
    }
    if (c == Key.ctrlV || c == Key.shiftIns) {
      paste_()
      return true
    }
    if (c == Key.ctrlY) {
      truncate_()
      return true
    }
    if (c == Key.ctrlZ) {
      clearSelection_()
      showHelp_()
      return true
    }
    if (c == Key.enter) {
      clearSelection_()
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
    clearSelection_()
    return false
  }

  showHelp_() {
    var w = parent
    while (w is Widget) w = w.parent
    if (w != null) w.showHelp()
  }

  handleMouse_(me) {
    if (bounds == null) return false
    if (!bounds.contains(me.startX, me.startY)) return false
    var e = me.event
    if (e == Mouse.button2Click || e == Mouse.button3Click) {
      var pasteCol = me.endX - bounds.x
      cursor = (_scrollOff + pasteCol).max(0).min(_chars.count)
      paste_()
      return true
    }
    if (e != Mouse.button1Click) return false
    var col = me.endX - bounds.x
    cursor = (_scrollOff + col).max(0).min(_chars.count)
    return true
  }
}

// Existing-value editor.  A non-empty value is selected whenever the
// widget gains focus, matching UIFC's K_EDIT entry state.  Only the
// selected text uses the lightbar; the unused row and ordinary editing
// retain the surrounding default style.
class SelectOnFocusInput is TextInput {
  construct new() { super() }

  focused=(value) {
    var gaining = value && !focused
    if (!value && focused) clearSelection_()
    super.focused = value
    if (gaining) selectAll()
  }

  paintStyle_ {
    return allSelected ? style("input.focused") : style("default")
  }

  fillStyle_ { style("default") }
}
