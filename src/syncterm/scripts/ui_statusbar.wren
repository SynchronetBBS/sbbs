// SyncTERM Wren UI library — StatusBar.
//
// A single-row strip used at the bottom (or top) of the screen to show
// status text or key hints.  Holds either:
//
//   * `text=` — a single string, left-aligned with the rest of the
//      row blank-filled, or
//   * `segments=` — a list of `[text, align]` pairs where align is
//      "left", "center", or "right".  Multiple segments share the row
//      and are placed left-to-right within their allocated portion.
//
// Not focusable — StatusBar exists only to display state.
//
//   var s = StatusBar.new()
//   s.bounds = Rect.new(1, sz[1], sz[0], 1)
//   s.text   = "F1 Help  Esc Quit"
//
//   // or:
//   s.segments = [
//     ["F1 Help",         "left"],
//     ["Connected",       "center"],
//     ["09:42",           "right"],
//   ]
//
// Theme role: `statusbar` (black on cyan by default).

import "ui_widget" for Widget, Rect
import "ui_draw"   for Painter

class StatusBar is Widget {
  construct new() {
    super()
    focusable = false
    _text     = ""
    _segments = null      // when non-null, takes precedence over _text
  }

  text     { _text }
  text=(s) {
    _text     = s == null ? "" : s
    _segments = null
    markDirty()
  }

  segments     { _segments }
  segments=(list) {
    _segments = list
    markDirty()
  }

  cursorVisible { false }

  onPaint_() {
    var sf = surface
    var st = style("statusbar")
    Painter.fill(sf, Rect.new(0, 0, bounds.w, bounds.h), " ", st)
    if (_segments != null) {
      paintSegments_(sf, st)
      return
    }
    Painter.text(sf, 0, 0, _text, st, bounds.w)
  }

  paintSegments_(sf, st) {
    // Place left-aligned segments first, then right-aligned (from the
    // right edge inward), then centered segments in whatever is left.
    var w     = bounds.w
    var lefts  = []
    var rights = []
    var centers = []
    for (seg in _segments) {
      var a = seg[1]
      if (a == "right") {
        rights.add(seg[0])
      } else if (a == "center") {
        centers.add(seg[0])
      } else {
        lefts.add(seg[0])
      }
    }

    var lx = 0
    for (s in lefts) {
      if (lx >= w) break
      var room = w - lx
      Painter.text(sf, lx, 0, s, st, room)
      lx = lx + s.count + 2          // 2-cell gap between segments
    }

    var rx = w
    var i  = rights.count - 1
    while (i >= 0) {
      var s    = rights[i]
      var room = (rx - lx).max(0)
      var n    = s.count.min(room)
      if (n <= 0) break
      Painter.text(sf, rx - n, 0, s, st, n)
      rx = rx - n - 2
      i  = i - 1
    }

    if (centers.count > 0) {
      var s    = centers[0]
      var room = (rx - lx).max(0)
      if (room > 0) {
        var n  = s.count.min(room)
        var cx = lx + ((room - n) / 2).floor
        Painter.text(sf, cx, 0, s, st, n)
      }
    }
  }

  // StatusBar is purely decorative — events fall straight through.
  handle(ev) { false }
}
