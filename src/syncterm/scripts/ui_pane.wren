// SyncTERM Wren UI library — Pane.
//
// A Pane is a Container with a single-line frame and an optional
// title rendered in the top border.  It claims its full bounds (frame
// + interior) and lays children out within `innerBounds` (the area
// inside the border).  The frame uses the theme's "frame" / "frame.
// focused" Style and "frame.*" glyphs; the title uses "title" /
// "title.focused".
//
// Pane.focused is a *visual* flag — set it from the App or a parent
// container when the pane is the foreground.  It does NOT redirect
// keyboard focus; that still flows through Container's normal
// focused-child routing to a leaf widget inside.

import "ui_widget" for Container, Rect
import "ui_draw"   for Painter

class Pane is Container {
  construct new() {
    super()
    _title = null
  }

  title { _title }
  title=(s) {
    _title = s
    markDirty()
  }

  // The drawable interior — one cell in from each edge of the frame.
  // Returns null if bounds haven't been set yet.
  innerBounds {
    if (bounds == null) return null
    return Rect.new(bounds.x + 1, bounds.y + 1, bounds.w - 2, bounds.h - 2)
  }

  onPaint_() {
    var theme   = effectiveTheme
    var fStyle  = focused ? style("frame.focused") : style("frame")
    var tStyle  = focused ? style("title.focused") : style("title")
    var bgStyle = style("default")
    var rect    = Rect.new(0, 0, bounds.w, bounds.h)
    // `surface` getter, NOT `_surface` — Wren instance fields are
    // class-scoped, so `_surface` here would shadow Widget's.
    var s = surface

    // Clear, then draw frame (with optional title) onto our Surface.
    Painter.fill(s, rect, " ", bgStyle)
    if (_title != null) {
      Painter.frameTitle(s, rect, theme.glyphs, fStyle, _title, tStyle)
    } else {
      Painter.frame(s, rect, theme.glyphs, fStyle)
    }
    // Children paint on top of the cleared interior.
    compositeChildren_()
  }
}
