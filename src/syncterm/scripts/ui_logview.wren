// SyncTERM Wren UI library — LogView.
//
// Append-only colored-line scroller with a fixed viewport and a ring-
// buffered backing store.  Designed for the transfer-window log
// channel (zmodem / xmodem / CET status messages) but reusable for
// any "splat lines as they happen, color by severity" surface.
//
//   var lv = LogView.new()
//   lv.bounds = Rect.new(2, 4, 60, 8)
//   lv.append(LogView.LEVEL_NOTICE,  "Receiving foo.bin (12345 bytes)")
//   lv.append(LogView.LEVEL_ERROR,   "Bad CRC on frame 42, retrying")
//
// Severity levels mirror syslog priorities — the same numeric scale
// used by the C-side `lputs(level, str)` callback, which is what the
// transfer mailbox will hand us per line.  Only four bins are
// distinguished visually:
//
//   level <= 3 (emerg / alert / crit / err)   — light red on blue
//   level == 4 (warning)                       — light magenta on blue
//   level == 5 (notice)                        — yellow on blue
//   level >= 6 (info / debug / etc.)           — white on blue
//
// Scroll-back is keyboard-driven (PgUp / PgDn / Up / Down / Home /
// End).  The live-tail mode is `scrollTop == null`; entering scroll
// mode anchors the view at the current bottom and `append` no longer
// auto-tails until the user returns to live with End.
//
// A vertical scrollbar (track + arrow caps + thumb) appears on the
// right edge whenever the ring's contents exceed the viewport height,
// matching ListView's default layout.  Mouse wheel scrolls the viewport
// by one page over the scrollbar and by the normal step over content;
// click + drag on the scrollbar column jumps the view.
// One column of frame separator sits between the scrollbar and the
// log text so they don't run together visually.
//
// Theme roles:
//   logview.error     — severity <= 3
//   logview.warning   — severity == 4
//   logview.notice    — severity == 5
//   logview.info      — severity >= 6 (default cascade)

import "ui_widget" for Widget, Rect
import "ui_draw"   for Painter
import "syncterm"  for Key, KeyEvent, Mouse, MouseEvent

class LogView is Widget {
  // Severity constants — mirror the C-side LOG_* macros so callers
  // that proxy lputs() into LogView.append() pass the level through
  // unchanged.
  static LEVEL_EMERG   { 0 }
  static LEVEL_ALERT   { 1 }
  static LEVEL_CRIT    { 2 }
  static LEVEL_ERR     { 3 }
  static LEVEL_WARNING { 4 }
  static LEVEL_NOTICE  { 5 }
  static LEVEL_INFO    { 6 }
  static LEVEL_DEBUG   { 7 }

  construct new() {
    super()
    focusable    = true
    _capacity    = 256
    _ring        = []     // entries are [level, text]
    _scrollTop   = -1     // -1 = live tail; >= 0 = anchored offset
  }

  // Maximum lines retained in the ring.  Setting smaller than the
  // current count drops the oldest lines.
  capacity     { _capacity }
  capacity=(n) {
    _capacity = n
    while (_ring.count > _capacity) _ring.removeAt(0)
    if (_scrollTop > _ring.count - 1) _scrollTop = -1
    markDirty()
  }

  // Read-only view of how many lines are currently buffered.
  count { _ring.count }

  // Public scroll position; null when live-tailing.
  scrollTop { _scrollTop < 0 ? null : _scrollTop }

  cursorVisible { false }

  // Append a new line.  When in live-tail mode the view auto-follows
  // the latest entry; in scroll mode the anchor stays put (so the
  // user can read older entries without the view jumping out from
  // under them).  Anchor adjustment on ring overflow uses the
  // WRAPPED row count of the dropped entry — a long line collapses
  // multiple visual rows when it falls off the back.
  append(level, text) {
    _ring.add([level, text])
    if (_ring.count > _capacity) {
      var droppedRows = rowsForEntry_(_ring[0], contentWidth_)
      _ring.removeAt(0)
      if (_scrollTop >= 0) {
        _scrollTop = (_scrollTop - droppedRows).max(0)
      }
    }
    markDirty()
  }

  // Drop everything and return to live mode.
  clear() {
    _ring.clear()
    _scrollTop = -1
    markDirty()
  }

  // Force live-tail mode (no anchor).
  goLive() {
    _scrollTop = -1
    markDirty()
  }

  // Scroll by `delta` visual rows (negative = up = older content).
  // Going up from live mode anchors at the top of the currently-visible
  // window (so the next PgDn returns to live cleanly).  Going down
  // from the last anchor that would still fit in viewport snaps back
  // to live.  Counts are in WRAPPED visual rows, not logical entries.
  scrollBy(delta) {
    if (bounds == null) return
    var n  = visualRowCount_
    var vh = bounds.h
    if (n <= vh) return
    var maxTop = n - vh
    if (_scrollTop < 0) {
      _scrollTop = maxTop
    }
    _scrollTop = (_scrollTop + delta).max(0).min(maxTop)
    if (_scrollTop == maxTop && delta > 0) {
      _scrollTop = -1
    }
    markDirty()
  }

  // Effective top-of-viewport visual-row index given the current scroll
  // mode.  In live mode this is `visualRowCount - viewport` (or 0
  // when content fits); in scroll mode it's the explicit anchor.
  effectiveScrollTop_ {
    var n = visualRowCount_
    if (n == 0) return 0
    var vh = bounds == null ? 0 : bounds.h
    if (_scrollTop >= 0) return _scrollTop
    if (n > vh) return n - vh
    return 0
  }

  // True when wrapped content exceeds the viewport — the signal for
  // "draw a scrollbar."  Computed at the worst-case (scrollbar-shown)
  // width to avoid the showing-shrinks-content-shrinks-row-count
  // oscillation that would trigger if we asked at the wider width.
  scrollbarVisible_ {
    if (bounds == null || bounds.h <= 0) return false
    var narrowW = (bounds.w - 2).max(1)
    return visualRowCountAtWidth_(narrowW) > bounds.h
  }

  // Codepoint-aware row count for one entry at the given width.
  // String.count returns codepoints; multi-byte UTF-8 stays intact.
  rowsForEntry_(entry, width) {
    if (width <= 0) return 1
    var n = entry[1].count
    if (n == 0) return 1
    return ((n + width - 1) / width).floor
  }

  // Sum of wrapped rows across the ring at the given width.  Cheap
  // enough to recompute per frame; caching could come if 256 × few-
  // codepoints turns out to be hot.
  visualRowCountAtWidth_(width) {
    var total = 0
    for (e in _ring) total = total + rowsForEntry_(e, width)
    return total
  }

  visualRowCount_ { visualRowCountAtWidth_(contentWidth_) }

  // Split `text` into chunks of <= `width` codepoints, walking
  // codepoints (NOT bytes) so multi-byte UTF-8 doesn't get cleaved.
  // Empty input still returns one (empty) chunk so blank-line entries
  // occupy a row.
  wrapText_(text, width) {
    var chunks = []
    if (width <= 0) {
      chunks.add(text)
      return chunks
    }
    var chunk = ""
    var len   = 0
    for (cp in text) {
      chunk = chunk + cp
      len   = len + 1
      if (len >= width) {
        chunks.add(chunk)
        chunk = ""
        len   = 0
      }
    }
    if (chunk.bytes.count > 0 || chunks.count == 0) chunks.add(chunk)
    return chunks
  }

  // Wheel-step (rows) for mouse-wheel scrolling.  Matches the
  // ListView default of three rows per click.
  static wheelStep { 3 }

  // Layout columns when the scrollbar is visible.
  //   col 0 .. w-3    — log text content
  //   col w-2         — frame separator (single glyph column)
  //   col w-1         — scrollbar track + arrow caps + thumb
  // When the scrollbar is hidden, content spans cols 0 .. w-1.
  scrollbarColumn_ { bounds.w - 1 }
  separatorColumn_ { bounds.w - 2 }
  contentColumn_   { 0 }
  contentWidth_ {
    if (bounds == null) return 0
    return scrollbarVisible_ ? (bounds.w - 2).max(0) : bounds.w
  }

  styleForLevel_(level) {
    if (level <= LogView.LEVEL_ERR)     return style("logview.error")
    if (level == LogView.LEVEL_WARNING) return style("logview.warning")
    if (level == LogView.LEVEL_NOTICE)  return style("logview.notice")
    return style("logview.info")
  }

  onPaint_() {
    var sf = surface
    var w  = bounds.w
    var h  = bounds.h
    var cx = contentColumn_
    var cw = contentWidth_

    // Default background: blank cells with the info style (cascades to
    // default if no role).  Severity-tinted lines paint over the
    // content area; the scrollbar column draws its own track / thumb.
    var bg = style("logview.info")
    Painter.fill(sf, Rect.new(0, 0, w, h), " ", bg)

    // Walk entries from the start, skipping wrapped rows until we
    // reach `effectiveScrollTop_`, then paint up to `h` rows.  Each
    // entry expands into 1+ wrapped chunks (codepoint-aware) sharing
    // the entry's severity tint.
    var skip    = effectiveScrollTop_
    var painted = 0
    var ei      = 0
    while (ei < _ring.count && painted < h) {
      var entry  = _ring[ei]
      var lvl    = entry[0]
      var txt    = entry[1]
      var st     = styleForLevel_(lvl)
      var chunks = wrapText_(txt, cw)
      var ci     = 0
      while (ci < chunks.count && painted < h) {
        if (skip > 0) {
          skip = skip - 1
        } else {
          Painter.fill(sf, Rect.new(cx, painted, cw, 1), " ", st)
          Painter.text(sf, cx, painted, chunks[ci], st, cw)
          painted = painted + 1
        }
        ci = ci + 1
      }
      ei = ei + 1
    }

    if (scrollbarVisible_) {
      var glyphs     = effectiveTheme.glyphs
      var trackStyle = style("scrollbar.track")
      var thumbStyle = style("scrollbar.thumb")
      Painter.scrollbar(sf, scrollbarColumn_, 0, h,
                        effectiveScrollTop_, visualRowCount_, h,
                        glyphs, trackStyle, thumbStyle)
      // Single-column separator between scrollbar and content.
      var sepStyle = style("default")
      var sepGlyph = glyph("scrollbar.separator")
      var sx       = separatorColumn_
      var j = 0
      while (j < h) {
        Painter.fill(sf, Rect.new(sx, j, 1, 1), sepGlyph, sepStyle)
        j = j + 1
      }
    }
  }

  // Move the absolute scroll anchor to `target`, clamping into the
  // valid range and snapping back to live mode when the target lands
  // at (or past) the natural tail.  Internal helper used by the
  // mouse-driven scroll path.
  setScrollTo_(target) {
    var n  = _ring.count
    var vh = bounds.h
    if (n <= vh) return
    var maxTop = n - vh
    var t      = target.max(0).min(maxTop)
    if (t == maxTop) {
      _scrollTop = -1
    } else {
      _scrollTop = t
    }
    markDirty()
  }

  handleMouse_(me) {
    if (bounds == null) return false
    var e = me.event

    var overScrollbar = scrollbarVisible_ &&
        me.startX == bounds.x + scrollbarColumn_

    // Wheel: use the normal row step over content and a full viewport
    // over the scrollbar.
    if (e == Mouse.wheelUpPress || e == Mouse.wheelUpClick) {
      var step = overScrollbar ? bounds.h.max(1) : LogView.wheelStep
      scrollBy(-step)
      return true
    }
    if (e == Mouse.wheelDownPress || e == Mouse.wheelDownClick) {
      var step = overScrollbar ? bounds.h.max(1) : LogView.wheelStep
      scrollBy(step)
      return true
    }

    // Beyond the wheel, only react to button-1 press / click / drag.
    if (e != Mouse.button1Press && e != Mouse.button1Click &&
        e != Mouse.button1DragStart && e != Mouse.button1DragMove) {
      return false
    }

    // Hit-test the GRAB by `start` so dragging off the scrollbar still
    // scrolls; resolve the new scrollTop from the CURRENT pointer Y
    // (`end`).  The scrollbar is at column scrollbarColumn_ in widget-
    // local coords; convert against bounds.x to get screen coords.
    if (overScrollbar) {
      var py = me.endY - bounds.y
      var t  = Painter.scrollbarClick(py, bounds.h, _ring.count,
                                      bounds.h, effectiveScrollTop_)
      setScrollTo_(t)
      return true
    }

    return false
  }

  handle(ev) {
    if (ev is MouseEvent) return handleMouse_(ev)
    if (!(ev is KeyEvent)) return false
    var c = ev.code
    if (c == Key.pageUp) {
      scrollBy(-bounds.h)
      return true
    }
    if (c == Key.pageDown) {
      scrollBy(bounds.h)
      return true
    }
    if (c == Key.up) {
      scrollBy(-1)
      return true
    }
    if (c == Key.down) {
      scrollBy(1)
      return true
    }
    if (c == Key.home) {
      if (_ring.count > 0) {
        _scrollTop = 0
        markDirty()
      }
      return true
    }
    if (c == Key.end) {
      goLive()
      return true
    }
    return false
  }
}
