// SyncTERM Wren UI library — RadioGroup.
//
// A vertical stack of mutually-exclusive options.  Owns its own item
// list and selected index — no per-row child widgets, so layout stays
// simple and the whole group is a single focusable unit.
//
//   var g = RadioGroup.new()
//   g.bounds   = Rect.new(x, y, w, h)
//   g.items    = ["Single", "Double", "Off"]
//   g.selected = 0
//   g.onChange = Fn.new {|i, item| ... }
//
// Navigation:
//   Up / Down       move the row cursor
//   Home / End      jump to ends
//   Space / Enter   commit the cursor row as the selection (and fire
//                   onChange if it differs from the current selection)
//   Mouse click     selects the row under the pointer
//
// Theme roles consulted:
//   radio.item            — non-cursor row
//   radio.item.focused    — row under the cursor
// Glyphs reused: radio.on (`●`), radio.off (`○`).
//
// "selected" is the *committed* value; "cursor" is the visually-
// highlighted row.  Cursor moves freely as the user navigates;
// selection only updates on Space / Enter / mouse click.

import "ui_widget" for Widget, Rect
import "ui_draw"   for Painter
import "syncterm"  for KeyEvent, MouseEvent, Key, Mouse

class RadioGroup is Widget {
  construct new() {
    super()
    _items    = []
    _selected = -1
    _cursor   = 0
    _onChange = null
    _wrap     = true            // standalone behavior; Form turns this off
  }

  // When true (default), Up at the top wraps to the bottom and Down
  // at the bottom wraps to the top — every Up/Down press is
  // consumed.  When false, Up at the top and Down at the bottom
  // return false instead, letting the parent Container move focus to
  // a sibling.  Form.addField sets this to false on RadioGroup
  // children so vertical traversal escapes the group at its edges.
  wrap     { _wrap }
  wrap=(b) { _wrap = b }

  items { _items }
  items=(list) {
    _items    = list
    _selected = list.count > 0 ? 0 : -1
    _cursor   = list.count > 0 ? 0 : -1
    markDirty()
  }

  count { _items.count }

  // null when no row is selected (empty group or explicitly cleared);
  // otherwise an integer in 0..count - 1.  `selected` is the
  // committed value; `cursor` is the currently-highlighted row.
  selected { _selected < 0 ? null : _selected }
  selected=(i) {
    if (i == null || _items.count == 0) {
      if (_selected == -1) return
      _selected = -1
      markDirty()
      return
    }
    var clamped = i.max(0).min(_items.count - 1)
    if (clamped == _selected) return
    _selected = clamped
    if (_onChange != null) _onChange.call(_selected, _items[_selected])
    markDirty()
  }

  selectedItem {
    if (_selected < 0 || _selected >= _items.count) return null
    return _items[_selected]
  }

  cursor { _cursor < 0 ? null : _cursor }
  cursor=(i) {
    if (i == null || _items.count == 0) {
      if (_cursor == -1) return
      _cursor = -1
      markDirty()
      return
    }
    var clamped = i.max(0).min(_items.count - 1)
    if (clamped == _cursor) return
    _cursor = clamped
    markDirty()
  }

  onChange=(fn) { _onChange = fn }

  // Park the hardware cursor on the first cell of the cursor row so
  // backends that follow the cursor track the highlight.
  cursorPos {
    if (bounds == null || _cursor < 0) return null
    return [bounds.x, bounds.y + _cursor]
  }
  cursorVisible { false }

  onPaint_() {
    var sf = surface
    var bg = style("default")
    Painter.fill(sf, Rect.new(0, 0, bounds.w, bounds.h), " ", bg)

    var i = 0
    while (i < _items.count && i < bounds.h) {
      // Cursor highlight only appears when the widget itself is
      // focused; otherwise every row uses the plain item style so
      // tab-traversal through a Form doesn't leave a stale lightbar
      // hinting at focus that's actually elsewhere.  The committed
      // selection is still visible via the filled `(•)` glyph.
      var isCursor = focused && i == _cursor
      var role = isCursor ? "radio.item.focused" : "radio.item"
      var st   = style(role)
      Painter.fill(sf, Rect.new(0, i, bounds.w, 1), " ", st)
      var g = i == _selected ? glyph("radio.on") : glyph("radio.off")
      Painter.text(sf, 0, i, "(", st, 1)
      Painter.text(sf, 1, i, g,   st, 1)
      Painter.text(sf, 2, i, ")", st, 1)
      if (bounds.w > 4) {
        Painter.text(sf, 4, i, _items[i].toString, st, bounds.w - 4)
      }
      i = i + 1
    }
  }

  // Commit the cursor row as the selection.  Idempotent — calling
  // it on the already-selected row doesn't fire onChange (selected=
  // checks for that itself).
  commit_() {
    if (_cursor < 0) return
    selected = _cursor
  }

  handle(ev) {
    if (ev is KeyEvent) return handleKey_(ev)
    if (ev is MouseEvent) return handleMouse_(ev)
    return false
  }

  handleKey_(ke) {
    var c  = ke.code
    var cp = ke.codepoint
    if (c == Key.up) {
      if (_items.count == 0) return false
      if (_cursor == 0) {
        if (!_wrap) return false
        cursor = _items.count - 1
        return true
      }
      cursor = _cursor - 1
      return true
    }
    if (c == Key.down) {
      if (_items.count == 0) return false
      if (_cursor == _items.count - 1) {
        if (!_wrap) return false
        cursor = 0
        return true
      }
      cursor = _cursor + 1
      return true
    }
    if (c == Key.home) {
      cursor = 0
      return true
    }
    if (c == Key.end) {
      cursor = _items.count - 1
      return true
    }
    if (c == Key.enter || cp == 0x20) {
      commit_()
      return true
    }
    return false
  }

  handleMouse_(me) {
    if (bounds == null) return false
    if (!bounds.contains(me.startX, me.startY)) return false
    var e = me.event
    if (e != Mouse.button1Click && e != Mouse.button1Press) return false
    var idx = me.startY - bounds.y
    if (idx < 0 || idx >= _items.count) return false
    cursor   = idx
    commit_()
    return true
  }
}
