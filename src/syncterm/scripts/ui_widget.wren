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
  }

  bounds { _bounds }
  bounds=(r) {
    _bounds = r
    markDirty()
  }

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

  style(role) { effectiveTheme.style(role) }
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

  // Update the widget's surface (calling onPaint_ when dirty), then
  // return it.  Caller (typically a parent Container) composites the
  // returned Surface into its own surface at the right offset.
  draw() {
    ensureSurface_()
    if (_surface == null) return null
    if (_dirty) {
      onPaint_()
      clearDirty()
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

  // Step focus by `dir` (+1 / -1), wrapping, skipping non-focusable
  // children.  Returns true if focus moved (or stayed, if there's
  // exactly one focusable child).  False when no focusable child
  // exists at all.
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

  // Container's onPaint_: fill background with the default style, then
  // composite visible children.  Subclasses (Pane, etc.) override
  // wholesale and use compositeChildren_() if they want their own
  // background / decoration treatment.
  //
  // Both methods reach Widget's `_surface` field through the `surface`
  // getter — referencing `_surface` directly here would create a new
  // Container-scoped field (Wren fields are per-class, not inherited).
  onPaint_() {
    Painter.fill(surface,
                 Rect.new(0, 0, bounds.w, bounds.h),
                 " ", style("default"))
    compositeChildren_()
  }

  // Walk visible children and putRect each one's surface into ours
  // at the child-relative offset.  Triggers child's draw() which may
  // re-paint that child's own surface.
  compositeChildren_() {
    var s = surface
    for (c in _children) {
      if (!c.visible) continue
      var cs = c.draw()
      if (cs == null) continue
      s.putRect(cs, c.bounds.x - bounds.x, c.bounds.y - bounds.y)
    }
  }

  // Route key events to the focused child first; if unconsumed,
  // handle Tab / BackTab focus traversal at this level.  Mouse and
  // other events fall through to the base handler (returns false) —
  // mouse routing is the App's job via hitTest, not focus-based.
  // Subclasses override for click-on-background behaviors.
  handle(ev) {
    if (ev is KeyEvent) {
      var fc = focusedChild
      if (fc != null && fc.visible && fc.handle(ev)) return true
      if (ev.code == Key.tab) return focusNext()
      if (ev.code == Key.backTab) return focusPrev()
      return false
    }
    return false
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
