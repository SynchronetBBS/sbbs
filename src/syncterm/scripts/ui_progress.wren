// SyncTERM Wren UI library — progress display widgets.
//
// A horizontal progress bar that fills from the left with the medium-
// shade character (U+2592 = CP437 0xB1) up to the fraction
// current/total of its width.  Matches the existing transfer-window
// fill character.  An optional centered "% nn" overlay shows the
// percentage in plain text on top of the fill.
//
// Not focusable — purely a display widget.
//
//   var p = ProgressBar.new()
//   p.bounds = Rect.new(2, 5, 30, 1)
//   p.set(45, 100)         // 45% filled, "% 45" overlay
//
// When total is 0 or null the bar paints empty (no fill, no overlay)
// — callers with indeterminate-size transfers omit the widget rather
// than asking ProgressBar to render an animated bouncer.
//
// Theme roles:
//   progress.fill   — filled portion (default: light cyan on blue)
//   progress.empty  — unfilled portion (default cascade)
//   progress.text   — "% nn" overlay (default cascade)

import "ui_widget" for Widget, Rect
import "ui_draw"   for Painter
import "ui_popup"  for Popup

// A vertically centered block of wrapped progress-status rows.  Every row
// starts at the same padded left edge so callers can supply aligned columns.
class ProgressText is Widget {
  construct new() {
    super()
    focusable = false
    _lines = []
  }

  lines=(value) {
    _lines = value
    markDirty()
  }

  wrappedLines_(width) {
    var wrapped = []
    var textWidth = (width - 2).max(1)
    for (line in _lines) {
      for (part in Popup.wrap_(line, textWidth)) wrapped.add(part)
    }
    return wrapped
  }

  rowCount(width) { wrappedLines_(width).count }

  onPaint_() {
    var sf = surface
    var st = style("default")
    var textWidth = (bounds.w - 2).max(1)
    Painter.fill(sf, Rect.new(0, 0, bounds.w, bounds.h), " ", st)
    var lines = wrappedLines_(bounds.w)
    var count = lines.count.min(bounds.h)
    var row = ((bounds.h - count) / 2).floor
    for (i in 0...count) {
      Painter.text(sf, 1, row + i, lines[i], st, textWidth)
    }
  }
}

class ProgressBar is Widget {
  construct new() {
    super()
    focusable    = false
    _current     = 0
    _total       = 0
    _showPercent = true
  }

  current { _current }
  current=(v) {
    _current = v
    markDirty()
  }

  total { _total }
  total=(v) {
    _total = v
    markDirty()
  }

  showPercent     { _showPercent }
  showPercent=(b) {
    _showPercent = b
    markDirty()
  }

  // Convenience setter — single call updates both numerator + denom.
  // Recomputed pixel result depends on both, so re-marking dirty once
  // is cheaper than two property assignments that each mark dirty.
  set(cur, total) {
    _current = cur
    _total   = total
    markDirty()
  }

  cursorVisible { false }

  // Number of cells (out of `width`) that should be filled, given the
  // current/total ratio.  Both ends clamp; total <= 0 or null returns
  // 0 so onPaint_ degenerates cleanly to the empty state.
  filledCells_(width) {
    if (_total == null || _total <= 0) return 0
    var cur = _current
    if (cur < 0)      cur = 0
    if (cur > _total) cur = _total
    return (width * cur / _total).floor
  }

  // Integer percent (0..100); null when total is missing or 0.
  percent_ {
    if (_total == null || _total <= 0) return null
    var cur = _current
    if (cur < 0)      cur = 0
    if (cur > _total) cur = _total
    return (100 * cur / _total).floor
  }

  onPaint_() {
    var sf         = surface
    var w          = bounds.w
    var h          = bounds.h
    var fillStyle  = style("progress.fill")
    var emptyStyle = style("progress.empty")
    var filled     = filledCells_(w)

    Painter.fill(sf, Rect.new(0, 0, w, h), " ", emptyStyle)
    if (filled > 0) {
      Painter.fill(sf, Rect.new(0, 0, filled, h), "\u2592", fillStyle)
    }

    if (_showPercent) {
      var p = percent_
      if (p != null) {
        // "%nn\%" — three to four cells.  Centered on the middle row.
        // The literal "%" needs the \% escape (Wren lexer rule).
        var label = "%(p)\%"
        var cy    = (h / 2).floor
        var cx    = ((w - label.count) / 2).floor
        if (cx < 0) cx = 0
        var ts = style("progress.text")
        Painter.text(sf, cx, cy, label, ts, w - cx)
      }
    }
  }

  // Purely decorative — events fall straight through.
  handle(ev) { false }
}
