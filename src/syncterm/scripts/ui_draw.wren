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

  // Frame around `rect` using glyphs under `prefix.*`.  Default
  // prefix "frame" gives a single-line box (`┌─┐│└┘`); pass
  // "frame.double" for a double-line one (`╔═╗║╚╝`).  `rect` is in
  // surface-local coords.  Interior cells are not touched.  Rects
  // narrower than 2×2 are no-ops.
  static frame(surface, rect, glyphs, style) {
    framePrefixed_(surface, rect, glyphs, style, "frame")
  }
  static frame(surface, rect, glyphs, style, prefix) {
    framePrefixed_(surface, rect, glyphs, style, prefix)
  }
  static framePrefixed_(surface, rect, glyphs, style, prefix) {
    if (rect.w < 2 || rect.h < 2) return
    var x  = rect.x
    var y  = rect.y
    var x2 = rect.right
    var y2 = rect.bottom

    putGlyph_(surface, x,  y,  glyphs[prefix + ".topLeft"],     style)
    putGlyph_(surface, x2, y,  glyphs[prefix + ".topRight"],    style)
    putGlyph_(surface, x,  y2, glyphs[prefix + ".bottomLeft"],  style)
    putGlyph_(surface, x2, y2, glyphs[prefix + ".bottomRight"], style)

    var topG = glyphs[prefix + ".top"]
    var botG = glyphs[prefix + ".bottom"]
    var i = x + 1
    while (i < x2) {
      putGlyph_(surface, i, y,  topG, style)
      putGlyph_(surface, i, y2, botG, style)
      i = i + 1
    }
    var leftG  = glyphs[prefix + ".left"]
    var rightG = glyphs[prefix + ".right"]
    var j = y + 1
    while (j < y2) {
      putGlyph_(surface, x,  j, leftG,  style)
      putGlyph_(surface, x2, j, rightG, style)
      j = j + 1
    }
  }

  // Frame with a centered title in the top border (`┤ Title ├`).
  // The `prefix` selects the glyph family ("frame" / "frame.double").
  // Title truncates at rect.w - 6 cells (corners + brackets + spaces).
  static frameTitle(surface, rect, glyphs, frameStyle, title, titleStyle) {
    frameTitlePrefixed_(surface, rect, glyphs, frameStyle, title, titleStyle, "frame")
  }
  static frameTitle(surface, rect, glyphs, frameStyle, title, titleStyle, prefix) {
    frameTitlePrefixed_(surface, rect, glyphs, frameStyle, title, titleStyle, prefix)
  }
  static frameTitlePrefixed_(surface, rect, glyphs, frameStyle, title, titleStyle, prefix) {
    framePrefixed_(surface, rect, glyphs, frameStyle, prefix)
    if (title == null || title.count == 0) return
    if (rect.w < 8) return
    var maxW    = rect.w - 6                 // corners(2) brackets(2) spaces(2)
    var visible = 0
    for (c in title) {
      if (visible >= maxW) break
      visible = visible + 1
    }
    if (visible == 0) return
    // Pre-truncate so the cap on text(...) doesn't bleed extra
    // characters past the trailing-space pad — visible+2 cells must
    // contain " <trimmed> ", not the first n chars of " <full> ".
    var trimmed = title.count > visible ? title[0...visible] : title
    var startX  = rect.x + ((rect.w - visible - 4) / 2).floor
    putGlyph_(surface, startX, rect.y,
              glyphs[prefix + ".title.left"], frameStyle)
    text(surface, startX + 1, rect.y, " " + trimmed + " ",
         titleStyle, visible + 2)
    putGlyph_(surface, startX + visible + 3, rect.y,
              glyphs[prefix + ".title.right"], frameStyle)
  }

  static putGlyph_(surface, x, y, ch, style) {
    var cell = surface.cellAt(x, y)
    if (cell == null) return
    applyStyle(cell, style)
    cell.ch = ch
  }

  // Vertical scrollbar at column `x`, starting at `y`, height `h`,
  // showing `viewport`-sized window onto a `total`-row content with
  // current top at `scrollTop`.  `glyphs` is a Glyphs map; styles
  // are pulled by role.  When h >= 3 the top + bottom rows host
  // up/down arrows that stay visible regardless of scroll position —
  // the thumb stays inside the track area between them.  Caller is
  // responsible for any separator column.
  static scrollbar(surface, x, y, h, scrollTop, total, viewport,
                   glyphs, trackStyle, thumbStyle) {
    if (h <= 0 || total <= 0) return
    var maxScroll = (total - viewport).max(0)
    var hasArrows = h >= 3
    var trackTop  = hasArrows ? 1 : 0
    var trackH    = hasArrows ? h - 2 : h
    if (trackH < 1) trackH = 1
    var thumbH    = (trackH * viewport / total).floor.max(1)
    if (thumbH > trackH) thumbH = trackH
    var thumbOffset = 0
    if (maxScroll > 0) {
      thumbOffset = ((trackH - thumbH) * scrollTop / maxScroll).floor
    }

    if (hasArrows) {
      putGlyph_(surface, x, y, glyphs["scrollbar.up"], thumbStyle)
      putGlyph_(surface, x, y + h - 1, glyphs["scrollbar.down"], thumbStyle)
    }

    var i = 0
    while (i < trackH) {
      var inThumb = i >= thumbOffset && i < thumbOffset + thumbH
      var g  = inThumb ? glyphs["scrollbar.thumb"] : glyphs["scrollbar.track"]
      var st = inThumb ? thumbStyle : trackStyle
      putGlyph_(surface, x, y + trackTop + i, g, st)
      i = i + 1
    }
  }

  // Resolve a click at offset `py` (0-based, relative to scrollbar
  // top) into a new scrollTop.  Top-row click on an arrow-armed
  // scrollbar steps -1; bottom-row click steps +1; clicks in the
  // track jump proportionally.  Result is clamped to [0, maxScroll].
  static scrollbarClick(py, h, total, viewport, current) {
    if (h <= 0 || total <= 0) return 0
    var maxScroll = (total - viewport).max(0)
    var hasArrows = h >= 3
    if (hasArrows) {
      if (py <= 0) return (current - 1).max(0).min(maxScroll)
      if (py >= h - 1) return (current + 1).max(0).min(maxScroll)
      var trackH = h - 2
      if (trackH <= 1) return 0
      var pos = py - 1
      return (pos * maxScroll / (trackH - 1)).floor.max(0).min(maxScroll)
    }
    if (h <= 1) return 0
    var clamped = py.max(0).min(h - 1)
    return (clamped * maxScroll / (h - 1)).floor.max(0).min(maxScroll)
  }

  // Paint a UIFC-style drop-shadow on the cells around the rect at
  // (x0, y0, w, h).  The right side is 2 cells wide (cols x0+w and
  // x0+w+1, rows y0+1..y0+h); the bottom is 1 cell tall (row y0+h,
  // cols x0+1..x0+w+1).  Each shadow cell keeps its existing char
  // (so any painted backdrop shows through as a faded silhouette)
  // and flips to a dim style: legacy 0x08 (bright-black on black),
  // RGB fg 0x202020 on bg 0x000000.
  static shadow(surface, x0, y0, w, h) {
    if (w <= 0 || h <= 0) return
    var i = 1
    while (i <= h) {
      shadowCell_(surface, x0 + w,     y0 + i)
      shadowCell_(surface, x0 + w + 1, y0 + i)
      i = i + 1
    }
    var j = 1
    while (j <= w + 1) {
      shadowCell_(surface, x0 + j, y0 + h)
      j = j + 1
    }
  }

  static shadowCell_(surface, x, y) {
    var cell = surface.cellAt(x, y)
    if (cell == null) return
    cell.legacyAttr = 0x08
    cell.fgRgb      = 0x202020
    cell.bgRgb      = 0x000000
  }

}
