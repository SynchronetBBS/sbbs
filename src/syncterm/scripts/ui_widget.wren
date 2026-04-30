// SyncTERM Wren UI library — Rect, Widget, Container.
//
// Rect: integer screen rectangle in 1-based coordinates.  Carries
// (x, y, w, h) but exposes inclusive right/bottom for the "left,
// top, right, bottom" shape that ScreenWindow.bounds and
// Screen.readRect/writeRect consume.
//
// Widget: base class for everything paintable on screen.  Holds
// bounds, an optional theme override, focus/visible/dirty flags, and
// a parent pointer.  Theme inheritance walks parent.effectiveTheme;
// a parent can be any object that responds to effectiveTheme +
// markDirty (Container, App).  Subclasses override draw() and
// handle(ev); base draw is a no-op and base handle returns false.
//
// Container: groups widgets, manages a focused-child index, dispatches
// events to the focused leaf first, falls back to its own handler if
// the leaf passes.  Tab / BackTab steps focus among focusable children
// when nothing else consumes it.

import "ui_style" for Theme
import "ui_draw"  for Painter
import "syncterm" for KeyEvent, Key, Surface

class Rect {
  construct new(x, y, w, h) {
    _x = x
    _y = y
    _w = w
    _h = h
  }

  x { _x }
  y { _y }
  w { _w }
  h { _h }

  // Inclusive right/bottom — matches ScreenWindow.bounds and the
  // (sx, sy, ex, ey) shape of Screen.readRect / writeRect.
  right  { _x + _w - 1 }
  bottom { _y + _h - 1 }

  contains(px, py) {
    return px >= _x && px <= right &&
           py >= _y && py <= bottom
  }

  ==(o) {
    if (!(o is Rect)) return false
    return _x == o.x && _y == o.y && _w == o.w && _h == o.h
  }

  !=(o) { !(this == o) }

  toString { "Rect(%(_x), %(_y), %(_w), %(_h))" }
}

class Widget {
  construct new() {
    _bounds    = null
    _parent    = null
    _theme     = null
    _focused   = false
    _visible   = true
    _dirty     = true
    _focusable = true
    _surface   = null     // lazy; matched to bounds in ensureSurface_
    _helpText  = null
    _shadow    = false    // drop-shadow on right + bottom edges
    _renderedInActive = null   // last `inActiveLayer` seen at paint
  }

  bounds { _bounds }
  bounds=(r) {
    _bounds = r
    markDirty()
  }

  // Help text for this widget.  Default null; widgets that want to
  // expose context-sensitive help (typically Pane-level dialogs)
  // assign a string.  App.showHelp walks from the focused leaf
  // upward through the parent chain to find the nearest non-null
  // value, so you can scope help to the most-specific widget that
  // has something to say.
  helpText { _helpText }
  helpText=(s) { _helpText = s }

  // Drop-shadow flag.  When true, the widget's parent (or App, for
  // modals) paints a darkened band on the cells immediately right
  // of and below the widget's bounds.  Pane defaults this to true
  // when used as a popup; bare Widgets default false.
  shadow     { _shadow }
  shadow=(b) {
    _shadow = b
    markDirty()
  }

  // True when this widget should render in its *active* theme variant.
  // Two reasons a widget can land in the inactive layer:
  //
  //   1. It isn't part of the App's modal-top subtree (a modal is on
  //      top, and this widget is in the layer behind it).
  //   2. An ancestor that gates focus visibility (e.g. a Pane in a
  //      multi-pane layout) has `focused == false` — the whole
  //      subtree of an unfocused gate goes inactive.
  //
  // Walks the parent chain twice: once to find the App and confirm
  // the widget is inside `modalTop`'s subtree, again to test for any
  // unfocused gate ancestor.  When the widget isn't anchored under
  // any App, default true (no inactive context).
  inActiveLayer {
    var app = null
    var w = this
    while (w != null) {
      var p = w.parent
      if (p == null) break
      if (!(p is Widget)) {
        app = p
        break
      }
      w = p
    }
    if (app != null) {
      var top  = app.modalTop
      var x    = this
      var inTop = false
      while (x is Widget) {
        if (x == top) {
          inTop = true
          break
        }
        x = x.parent
      }
      if (!inTop) return false
    }
    var a = this
    while (a is Widget) {
      if (a.gatesActiveLayer && !a.focused) return false
      a = a.parent
    }
    return true
  }

  // Override and return true on widgets that act as a focus gate
  // for their subtree — when such a widget has `focused == false`,
  // its descendants render with the inactive theme variant even
  // when no modal is on top.  Pane is the canonical user; bare
  // Widgets default to "doesn't gate" so there's no behavior change
  // for the simple cases.
  gatesActiveLayer { false }

  parent     { _parent }
  // Accept anything that quacks like a parent (responds to
  // effectiveTheme + markDirty).  Container and App both satisfy
  // this; passing a non-conforming object will surface as a method
  // missing error on the first walk.
  parent=(p) { _parent = p }

  theme { _theme }
  theme=(t) {
    _theme = t
    markDirty()
  }

  focused { _focused }
  focused=(b) {
    _focused = b
    markDirty()
  }

  visible { _visible }
  visible=(b) {
    _visible = b
    markDirty()
  }

  focusable     { _focusable }
  focusable=(b) { _focusable = b }

  dirty { _dirty }

  // Resolve the active theme by walking up the parent chain to the
  // nearest ancestor with a theme set (Widget, Container, or App).
  // Falls back to Theme.default when the tree has no override at all.
  effectiveTheme {
    if (_theme != null) return _theme
    if (_parent != null) return _parent.effectiveTheme
    return Theme.default
  }

  // Resolve a theme role for *this* widget.  When the widget isn't
  // part of the App's active layer (modalTop subtree), append
  // ".inactive" so the theme's inactive cascade swaps in the dim
  // variant.  Widgets call this for every paint — frame, list rows,
  // input field, button label, scrollbar — so a single layer-state
  // check cascades through everything visible.
  style(role) {
    var r = role
    if (!inActiveLayer) r = role + ".inactive"
    return effectiveTheme.style(r)
  }
  glyph(name) { effectiveTheme.glyphs[name] }

  markDirty() {
    _dirty = true
    if (_parent != null) _parent.markDirty()
  }

  clearDirty() { _dirty = false }

  // True when (px, py) falls inside the widget's bounds and the
  // widget is visible.  No bounds set ⇒ no hit (widget hasn't been
  // laid out yet).
  hit(px, py) {
    if (!_visible || _bounds == null) return false
    return _bounds.contains(px, py)
  }

  // Surface ownership.  `surface` returns the widget's private
  // back-buffer (allocated lazily, matched in size to bounds).  The
  // Surface persists across draw cycles so a clean widget can be
  // re-blitted by its parent without re-rendering its cells.
  surface { _surface }

  // Reallocate _surface only when bounds change (or on first paint).
  // Setting bounds= invalidates dirty so next draw will repaint.
  ensureSurface_() {
    if (bounds == null) return
    if (_surface == null || _surface.width != bounds.w ||
        _surface.height != bounds.h) {
      _surface = Surface.new(bounds.w, bounds.h)
      _dirty = true
    }
  }

  // Update the widget's surface (calling onPaint_ when dirty OR
  // when the App's active-layer state changed since last paint),
  // then return it.  The layer-state self-check means modal
  // push/pop doesn't need a tree-wide dirty pass — each widget
  // notices on its next draw that its cache no longer matches and
  // repaints itself.
  draw() {
    ensureSurface_()
    if (_surface == null) return null
    var nowActive = inActiveLayer
    if (_dirty || _renderedInActive != nowActive) {
      onPaint_()
      clearDirty()
      _renderedInActive = nowActive
    }
    return _surface
  }

  // Subclass hook: paint into _surface.  Default is a no-op (a bare
  // Widget renders nothing).
  onPaint_() {}

  // Optional hardware-cursor position for this widget, in 1-based
  // screen coords as `[x, y]`.  Return `null` if the widget has no
  // meaningful cursor.  App reads this off the focused leaf each
  // draw and gotoxy's the screen cursor accordingly so terminals
  // that highlight or follow the cursor look right.
  cursorPos { null }

  // Should the hardware cursor be visible while this widget is
  // focused?  Text-input widgets override to true; everything else
  // hides the cursor (to avoid a stray block on a list row, etc.).
  // Some backends can't hide the cursor — that's why widgets must
  // also park it sensibly via `cursorPos` instead of just relying on
  // visibility.
  cursorVisible { false }

  // Hook for parent-driven hotkey activation.  Container scans its
  // children with this when a printable key fell through the focused
  // child; widgets that map their own letter to an action (Button
  // and friends) override to return true after acting.
  tryHotkey(ev) { false }

  // Default event handler — returns false (event not consumed).
  handle(ev) { false }
}

class Container is Widget {
  construct new() {
    super()
    _children     = []
    _focusedIndex = -1
    // Containers are focusable so they participate in the parent's
    // focus chain — the parent's focused-child lookup picks the
    // Container, and the Container's own handle() then routes the
    // event to its focused leaf.  Without this, intermediate panes
    // / panels short-circuit dispatch and the leaf never sees keys.
    // Subclasses can override (`focusable = false` in their
    // constructor) for purely decorative containers.
  }

  children     { _children }
  focusedIndex { _focusedIndex }
  focusedChild {
    if (_focusedIndex < 0 || _focusedIndex >= _children.count) return null
    return _children[_focusedIndex]
  }

  // Bubble cursor queries down to the focused leaf.  No focused
  // child ⇒ no cursor preference (null pos, default-hidden).
  cursorPos {
    var c = focusedChild
    if (c == null) return null
    return c.cursorPos
  }
  cursorVisible {
    var c = focusedChild
    if (c == null) return false
    return c.cursorVisible
  }

  add(child) {
    child.parent = this
    _children.add(child)
    if (_focusedIndex < 0 && child.focusable) {
      _focusedIndex = _children.count - 1
      child.focused = true
    }
    markDirty()
    return child
  }

  remove(child) {
    var i = _children.indexOf(child)
    if (i < 0) return false
    if (i == _focusedIndex) {
      child.focused = false
      _focusedIndex = -1
    } else if (i < _focusedIndex) {
      _focusedIndex = _focusedIndex - 1
    }
    _children.removeAt(i)
    child.parent = null
    if (_focusedIndex < 0) focusFirst_()
    markDirty()
    return true
  }

  // Move focus to the first focusable child; returns true if found.
  focusFirst_() {
    for (i in 0..._children.count) {
      if (_children[i].focusable) {
        _focusedIndex = i
        _children[i].focused = true
        return true
      }
    }
    _focusedIndex = -1
    return false
  }

  focusNext() { focusStep_(1) }
  focusPrev() { focusStep_(-1) }

  // Step focus by `dir` (+1 / -1), skipping non-focusable children.
  // Returns true when focus actually moved to a *different* child.
  // Returns false when the only focusable child is already focused
  // (so the parent Container can route the Tab upward) OR when no
  // focusable child exists at all.
  //
  // Uses a counter-based termination — visit at most `n` children,
  // bailing when none are focusable.  An earlier "wrap-back to
  // start" sentinel hung when _focusedIndex == -1 because the
  // sentinel was -1 but `i` is always coerced into [0, n).
  focusStep_(dir) {
    var n = _children.count
    if (n == 0) return false
    // First index to check.  When nothing is focused yet, start at
    // 0 (forward) or n-1 (backward) and check it directly.
    // Otherwise, advance from the current focused index.
    var i = -1
    if (_focusedIndex < 0) {
      i = dir > 0 ? 0 : n - 1
    } else {
      // Wren modulo follows the dividend's sign — coerce positive.
      i = ((_focusedIndex + dir) % n + n) % n
    }
    var visited = 0
    while (visited < n) {
      if (_children[i].focusable) {
        if (i == _focusedIndex) {
          // Wrapped back to the same child — there's no *other*
          // focusable child to receive focus.  Returning false lets
          // the parent Container's handle() see the Tab and try
          // focusNext at the next layer up.
          return false
        }
        if (_focusedIndex >= 0 && _focusedIndex < n) {
          _children[_focusedIndex].focused = false
        }
        _focusedIndex = i
        _children[i].focused = true
        markDirty()
        return true
      }
      i = ((i + dir) % n + n) % n
      visited = visited + 1
    }
    return false
  }

  // Container's onPaint_: just composite children.  The surface
  // around the children is whatever was already there — typically
  // the Surface.new default (current text-mode normattr) at
  // allocation, or the App's captured screen backdrop if it copied
  // one in.  No theme style is applied; the bare canvas isn't part
  // of any widget and shouldn't pick up a focus / inactive variant.
  // Subclasses with a themed interior (Pane, popups, …) override
  // wholesale.
  onPaint_() {
    compositeChildren_()
  }

  // Walk visible children and putRect each one's surface into ours
  // at the child-relative offset.  Triggers child's draw() which may
  // re-paint that child's own surface.  Children with `shadow` set
  // get a dim band painted on the parent surface in the cells just
  // right of and below their bounds.
  compositeChildren_() {
    var s = surface
    for (c in _children) {
      if (!c.visible) continue
      var cs = c.draw()
      if (cs == null) continue
      var dx = c.bounds.x - bounds.x
      var dy = c.bounds.y - bounds.y
      s.putRect(cs, dx, dy)
      if (c.shadow) Painter.shadow(s, dx, dy, c.bounds.w, c.bounds.h)
    }
  }

  // Route key events to the focused child first; if unconsumed,
  // handle focus traversal at this level.
  //   Tab / BackTab               — ordered ring (always available)
  //   Left / Right                — ordered ring (horizontal-row dialogs)
  //   Up / Down                   — *spatial* nearest-focusable scan
  // Spatial Up/Down picks the focusable child whose centre is above /
  // below the current focus's centre and minimises Manhattan distance.
  // Falls back to no-op if there's no candidate in that direction —
  // arrows shouldn't wrap around in mixed layouts.  Widgets that need
  // arrows for their own semantics (TextInput consumes Left/Right,
  // ListView consumes Up/Down etc.) catch the keys before they bubble.
  handle(ev) {
    if (ev is KeyEvent) {
      var fc = focusedChild
      if (fc != null && fc.visible && fc.handle(ev)) return true
      var c = ev.code
      if (c == Key.tab || c == Key.right) return focusNext()
      if (c == Key.backTab || c == Key.left) return focusPrev()
      if (c == Key.up)   return focusByDirection_(0, -1)
      if (c == Key.down) return focusByDirection_(0,  1)
      // Hotkey scan: any focusable visible child whose tryHotkey
      // matches consumes the key.  Lets a typed letter activate the
      // button advertising that letter even when focus is elsewhere.
      for (ch in _children) {
        if (ch.visible && ch.tryHotkey(ev)) return true
      }
      return false
    }
    return false
  }

  // Spatial focus pick: find the focusable visible child whose
  // centre is on the requested side of the currently-focused child's
  // centre (`dy < 0` = above, `dy > 0` = below; `dx` analogous), and
  // minimises Manhattan distance.  Returns true if focus moved.
  focusByDirection_(dx, dy) {
    var fc = focusedChild
    if (fc == null || fc.bounds == null) return false
    var cx = fc.bounds.x + (fc.bounds.w / 2).floor
    var cy = fc.bounds.y + (fc.bounds.h / 2).floor
    var bestI = -1
    var bestD = -1
    var i = 0
    while (i < _children.count) {
      var ch = _children[i]
      if (i != _focusedIndex && ch.focusable && ch.visible &&
          ch.bounds != null) {
        var ccx = ch.bounds.x + (ch.bounds.w / 2).floor
        var ccy = ch.bounds.y + (ch.bounds.h / 2).floor
        var ok = true
        if (dy < 0 && ccy >= cy) ok = false
        if (dy > 0 && ccy <= cy) ok = false
        if (dx < 0 && ccx >= cx) ok = false
        if (dx > 0 && ccx <= cx) ok = false
        if (ok) {
          // Compare on the primary axis only.  Tie (children on the
          // same row for Up/Down or same column for Left/Right) goes
          // to the lower-indexed sibling — i.e. tab order, which is
          // typically the visually-leftmost button.
          var d = dy != 0 ? (ccy - cy).abs : (ccx - cx).abs
          if (bestI < 0 || d < bestD) {
            bestI = i
            bestD = d
          }
        }
      }
      i = i + 1
    }
    if (bestI < 0) return false
    if (_focusedIndex >= 0 && _focusedIndex < _children.count) {
      _children[_focusedIndex].focused = false
    }
    _focusedIndex = bestI
    _children[bestI].focused = true
    markDirty()
    return true
  }

  // Recursive hit test: walk children top-to-bottom (last-added is
  // visually on top), descend into Containers, return the deepest
  // visible widget covering (px, py).  Returns this Container when
  // the point is inside our bounds but no child covers it; null
  // when the point is outside us entirely.
  hitTest(px, py) {
    if (!hit(px, py)) return null
    var i = _children.count - 1
    while (i >= 0) {
      var c = _children[i]
      if (c.visible) {
        if (c is Container) {
          var deep = c.hitTest(px, py)
          if (deep != null) return deep
        } else if (c.hit(px, py)) {
          return c
        }
      }
      i = i - 1
    }
    return this
  }
}
