// SyncTERM Wren UI library — MenuBar.
//
// A horizontal strip of activatable items, typically at the top of
// the screen.  Each item is a `[label, Fn]` pair; activating an item
// (mouse click, Enter on the focused item, or hotkey letter) calls
// the Fn with no arguments.  The Fn is responsible for whatever
// happens next — popping a Popup, opening another Pane, etc.  No
// nested submenus are part of v1; build them with Popup.
//
//   var bar = MenuBar.new()
//   bar.bounds = Rect.new(1, 1, sz[0], 1)
//   bar.items  = [
//     ["File", Fn.new { app.modal(fileMenu) }],
//     ["Edit", Fn.new { app.modal(editMenu) }],
//     ["Help", Fn.new { app.showHelp() }],
//   ]
//
// Layout: each item renders as " label "; items are separated by a
// single space.  The whole strip uses the `menubar` style; the
// focused item swaps to `menubar.item.focused`.
//
// Keys (when focused):
//   Left / Right   move focus between items, with wrap.
//   Home / End     jump to first / last.
//   Enter / Space  activate the focused item.
//   Letter         tries each item's first letter (case-insensitive
//                  ASCII) as a hotkey.  Containers also call our
//                  `tryHotkey` so a letter pressed anywhere in the
//                  same container fires the matching item.
//
// Mouse:
//   Click on item  activates the clicked item.  Click on the strip
//                  but outside any item is dropped.
//
// Theme roles consulted: menubar, menubar.item, menubar.item.focused.

import "ui_widget" for Widget, Rect
import "ui_draw"   for Painter
import "syncterm"  for KeyEvent, MouseEvent, Key, Mouse

class MenuBar is Widget {
  construct new() {
    super()
    _items   = []
    _focused = -1
    _layout  = null      // cached [[xStart, xEnd], ...] in surface coords
  }

  items { _items }
  items=(list) {
    _items   = list
    _focused = list.count > 0 ? 0 : -1
    _layout  = null
    markDirty()
  }

  count { _items.count }

  // The currently-highlighted item index (Tab/arrow navigation).  Not
  // the same as "active" — activation requires Enter / Space / click.
  focusedItem     { _focused }
  focusedItem=(i) {
    if (_items.count == 0) {
      _focused = -1
      markDirty()
      return
    }
    var clamped = i.max(0).min(_items.count - 1)
    if (clamped == _focused) return
    _focused = clamped
    markDirty()
  }

  cursorVisible { false }

  // (Re)compute item start/end columns; cached until items= or bounds=
  // invalidate.  Each item is " label " (label.count + 2); a single
  // space sits between consecutive items.
  layout_() {
    if (_layout != null) return _layout
    var out = []
    var x   = 0
    var i   = 0
    while (i < _items.count) {
      var lbl = _items[i][0]
      var w   = lbl.count + 2
      out.add([x, x + w])
      x = x + w + 1                  // +1 separator space
      i = i + 1
    }
    _layout = out
    return out
  }

  bounds=(r) {
    super.bounds = r
    _layout      = null
  }

  onPaint_() {
    var sf  = surface
    var bg  = style("menubar")
    Painter.fill(sf, Rect.new(0, 0, bounds.w, bounds.h), " ", bg)
    var rects = layout_()
    var i = 0
    while (i < _items.count) {
      var rng = rects[i]
      var x0  = rng[0]
      var x1  = rng[1]
      if (x0 >= bounds.w) break
      var role = i == _focused ? "menubar.item.focused" : "menubar.item"
      var st   = style(role)
      var w    = (x1 - x0).min(bounds.w - x0)
      Painter.fill(sf, Rect.new(x0, 0, w, 1), " ", st)
      var lbl = _items[i][0]
      var roomForLabel = (w - 2).max(0)
      if (lbl.count > roomForLabel) lbl = lbl[0...roomForLabel]
      Painter.text(sf, x0 + 1, 0, lbl, st, roomForLabel)
      i = i + 1
    }
  }

  activate_(idx) {
    if (idx < 0 || idx >= _items.count) return
    var fn = _items[idx][1]
    if (fn != null) fn.call()
  }

  // Try to match a typed printable codepoint against item labels'
  // first characters (case-insensitive ASCII).  Returns true and
  // activates if matched.  Used by Container.handle's hotkey scan
  // and by our own letter-key path.
  tryHotkey(ev) {
    if (!(ev is KeyEvent)) return false
    var cp = ev.codepoint
    if (cp == null) return false
    if (cp < 0x20 || cp >= 0x7F) return false
    var cpU = cp
    if (cpU >= 0x61 && cpU <= 0x7A) cpU = cpU - 0x20
    var i = 0
    while (i < _items.count) {
      var lbl = _items[i][0]
      if (lbl.count > 0) {
        var firstCh = lbl[0]
        if (firstCh.bytes.count == 1) {
          var b = firstCh.bytes[0]
          if (b >= 0x61 && b <= 0x7A) b = b - 0x20
          if (b == cpU) {
            focusedItem = i
            activate_(i)
            return true
          }
        }
      }
      i = i + 1
    }
    return false
  }

  handle(ev) {
    if (ev is KeyEvent)   return handleKey_(ev)
    if (ev is MouseEvent) return handleMouse_(ev)
    return false
  }

  handleKey_(ke) {
    if (_items.count == 0) return false
    var c = ke.code
    if (c == Key.left) {
      var i = _focused - 1
      if (i < 0) i = _items.count - 1
      focusedItem = i
      return true
    }
    if (c == Key.right) {
      var i = _focused + 1
      if (i >= _items.count) i = 0
      focusedItem = i
      return true
    }
    if (c == Key.home) {
      focusedItem = 0
      return true
    }
    if (c == Key.end) {
      focusedItem = _items.count - 1
      return true
    }
    if (c == Key.enter || ke.codepoint == 0x20) {
      activate_(_focused)
      return true
    }
    return tryHotkey(ke)
  }

  handleMouse_(me) {
    if (bounds == null) return false
    if (!bounds.contains(me.startX, me.startY)) return false
    if (me.event != Mouse.button1Click && me.event != Mouse.button1Press) {
      return false
    }
    var lx = me.startX - bounds.x
    var rects = layout_()
    var i = 0
    while (i < rects.count) {
      var r = rects[i]
      if (lx >= r[0] && lx < r[1]) {
        focusedItem = i
        activate_(i)
        return true
      }
      i = i + 1
    }
    return false
  }
}
