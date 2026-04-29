// SyncTERM Wren UI library — Painter.
//
// Widget-level helpers that compose drawing into a Surface.  Every
// primitive takes a Surface as its first argument and mutates the
// cells in place (via Surface.cellAt or Surface.fill); no calls
// reach Screen.writeRect from here.  The App's drawAll_ owns the
// only path from a Surface to the screen, via Screen.putRect.
//
// Coordinates inside a Surface are 0-based (0..width-1, 0..height-1).
// Painter helpers expect those local coords; widgets that work in
// absolute screen coords convert at composition time
// (`dst = abs - parent.bounds.x`).
//
// Style application is field-wise: every Style field that is non-null
// is written to the Cell, leaving null fields at their default.

import "syncterm" for Cell, Surface

class Painter {
  // Apply a Style to an existing Cell, leaving null fields untouched.
  // Used by every primitive that touches cells, exposed for callers
  // that mutate a cell view directly.
  static applyStyle(cell, style) {
    if (style.font       != null) cell.font       = style.font
    if (style.legacyAttr != null) cell.legacyAttr = style.legacyAttr
    if (style.fgRgb      != null) cell.fgRgb      = style.fgRgb
    if (style.bgRgb      != null) cell.bgRgb      = style.bgRgb
  }

  // Build a Cell template (standalone) preloaded with style + ch.
  // Suitable for `surface.fill(rect, template)`.
  static template(ch, style) {
    var c = Cell.new()
    applyStyle(c, style)
    c.ch = ch
    return c
  }

  // Bulk-fill a Surface rect with a single character + style.  Uses
  // the C-side Surface.fill primitive — single foreign call regardless
  // of rect size.
  static fill(surface, rect, ch, style) {
    surface.fill(rect, template(ch, style))
  }

  // Draw `str` on a single row starting at (x, y) in surface.
  // Iterates codepoints (one cell per codepoint).  No truncation in
  // the 5-arg form; the 6-arg form caps at maxW cells.  Cells past
  // the surface bounds are silently skipped (cellAt returns null).
  static text(surface, x, y, str, style) {
    for (c in str) {
      var cell = surface.cellAt(x, y)
      if (cell == null) break
      applyStyle(cell, style)
      cell.ch = c
      x = x + 1
    }
  }

  static text(surface, x, y, str, style, maxW) {
    if (maxW <= 0) return
    var written = 0
    for (c in str) {
      if (written >= maxW) break
      var cell = surface.cellAt(x, y)
      if (cell == null) break
      applyStyle(cell, style)
      cell.ch = c
      x = x + 1
      written = written + 1
    }
  }

  // Single-row horizontal line.
  static hline(surface, x, y, len, ch, style) {
    var i = 0
    while (i < len) {
      var cell = surface.cellAt(x + i, y)
      if (cell == null) break
      applyStyle(cell, style)
      cell.ch = ch
      i = i + 1
    }
  }

  // Single-column vertical line.
  static vline(surface, x, y, len, ch, style) {
    var i = 0
    while (i < len) {
      var cell = surface.cellAt(x, y + i)
      if (cell == null) break
      applyStyle(cell, style)
      cell.ch = ch
      i = i + 1
    }
  }

  // Single-line frame around `rect` using the theme's `frame.*`
  // glyphs.  `rect` is in surface-local coords.  Interior cells are
  // not touched.  Rects narrower than 2x2 are no-ops.
  static frame(surface, rect, glyphs, style) {
    if (rect.w < 2 || rect.h < 2) return
    var x  = rect.x
    var y  = rect.y
    var x2 = rect.right
    var y2 = rect.bottom

    // Corners.
    putGlyph_(surface, x,  y,  glyphs["frame.topLeft"],     style)
    putGlyph_(surface, x2, y,  glyphs["frame.topRight"],    style)
    putGlyph_(surface, x,  y2, glyphs["frame.bottomLeft"],  style)
    putGlyph_(surface, x2, y2, glyphs["frame.bottomRight"], style)

    // Top + bottom edges.
    var topG = glyphs["frame.top"]
    var botG = glyphs["frame.bottom"]
    var i = x + 1
    while (i < x2) {
      putGlyph_(surface, i, y,  topG, style)
      putGlyph_(surface, i, y2, botG, style)
      i = i + 1
    }
    // Left + right edges.
    var leftG  = glyphs["frame.left"]
    var rightG = glyphs["frame.right"]
    var j = y + 1
    while (j < y2) {
      putGlyph_(surface, x,  j, leftG,  style)
      putGlyph_(surface, x2, j, rightG, style)
      j = j + 1
    }
  }

  // Frame with a centered title in the top border.  `frameStyle` is
  // used for the border; `titleStyle` for the title text.  Title is
  // truncated when it would overflow rect.w - 4 cells (corners + one
  // padding cell each side).
  static frameTitle(surface, rect, glyphs, frameStyle, title, titleStyle) {
    frame(surface, rect, glyphs, frameStyle)
    if (title == null || title.count == 0) return
    if (rect.w < 6) return
    var maxW    = rect.w - 4
    // Count codepoints (capped at maxW) without copying — we just
    // need the visible width to compute the centering offset.
    var visible = 0
    for (c in title) {
      if (visible >= maxW) break
      visible = visible + 1
    }
    if (visible == 0) return
    var titleX = rect.x + ((rect.w - visible) / 2).floor
    text(surface, titleX, rect.y, title, titleStyle, maxW)
  }

  static putGlyph_(surface, x, y, ch, style) {
    var cell = surface.cellAt(x, y)
    if (cell == null) return
    applyStyle(cell, style)
    cell.ch = ch
  }
}
