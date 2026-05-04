// SyncTERM Wren UI library — Help.
//
// A modal scrollable text viewer.  Body is a markdown string
// (subset — see ui_markdown.wren); parsed once at construction and
// laid out lazily on each viewport-width change.  Rendered lines
// scroll independently of source lines, so a long paragraph that
// wraps to ten cells on disk shows as ten rows here.
//
// Usage:
//   Help.show(app, "Help - Editor", helpText)
//
// Keyboard:
//   Up / Down       scroll one line
//   PageUp/PageDown scroll one viewport
//   Home / End      jump to top / bottom
//   Esc / Enter     dismiss
// Mouse:
//   Wheel up/down   scroll three lines
//   Scrollbar drag  scroll proportionally
//
// Sizing: by default the dialog claims most of the screen with a
// 4-cell margin; pass an explicit Rect via the bounds= setter
// after construction to override.

import "ui_widget"   for Rect
import "ui_popup"    for Popup
import "ui_draw"     for Painter
import "ui_markdown" for Markdown
import "syncterm"    for KeyEvent, MouseEvent, Mouse, Key, Screen

class Help is Popup {
  construct new(titleText, body) {
    super(null)                 // no auto-rendered message line
    title      = titleText
    focused    = true
    _doc       = Markdown.parse(body == null ? "" : body)
    _lines     = []             // populated by ensureLayout_()
    _layoutW   = -1             // width the cached _lines were laid out at
    _scrollTop = 0
    _sbSide    = "left"         // "left" (UIFC) or "right"
    _sbSep     = true           // separator between scrollbar and content
  }

  scrollbarSide      { _sbSide }
  scrollbarSide=(s) {
    _sbSide = s
    markDirty()
  }
  scrollbarSeparator      { _sbSep }
  scrollbarSeparator=(b) {
    _sbSep = b
    markDirty()
  }

  static wheelStep { 3 }

  // Re-layout the parsed doc when the viewport size changes (open,
  // resize, scrollbar appearing/disappearing).  Two-pass to break
  // the scrollbar-visibility / layout-width circular dependency:
  //   1. Lay out assuming no scrollbar.  If the result fits in
  //      viewportRows_, that's our final layout.
  //   2. Otherwise the scrollbar will be visible — re-lay out at
  //      the narrower content width so wrapping accounts for it.
  // After this returns, _lines reflects the final layout and
  // scrollbarVisible_ / contentLeft_ / viewportCols_ all read
  // consistent state.
  ensureLayout_() {
    if (bounds == null) return
    if (_layoutW != -1 && _layoutBoundsW == bounds.w && _layoutBoundsH == bounds.h) return
    _layoutBoundsW = bounds.w
    _layoutBoundsH = bounds.h
    // Width budget accounts for: 2 frame cells + 1-cell padding
    // on each non-scrollbar side.  When the scrollbar appears it
    // replaces the padding cell on its side (UIFC convention —
    // text can be flush against the scrollbar / separator).
    var rows   = (bounds.h - 4).max(0)             // -2 frame, -2 padding
    var noSb   = (bounds.w - 4).max(1)             // both sides padded
    var sbOH   = _sbSep ? 2 : 1
    var withSb = (bounds.w - 3 - sbOH).max(1)      // one side padded + scrollbar
    var lines = Markdown.layout(_doc, noSb)
    if (lines.count > rows) {
      lines = Markdown.layout(_doc, withSb)
      _layoutW = withSb
    } else {
      _layoutW = noSb
    }
    _lines = lines
    var maxTop = (_lines.count - rows).max(0)
    if (_scrollTop > maxTop) _scrollTop = maxTop
  }

  // Default help-dialog placement: matches UIFC.  The dialog is
  // 2-cell margin on each side (so the body sits 2 cells inside
  // the screen edge) and stretches full cterm height minus the
  // status bar — which is painted on the bottom row of the cterm
  // area (term.c update_status), so we drop one row off the
  // bottom and pin the top at row 1.
  static centeredBounds_() {
    var sz = Screen.size
    var w  = (sz[0] - 4).max(20)
    var h  = (sz[1] - 1).max(8)
    var x  = ((sz[0] - w) / 2).floor + 1
    var y  = 1
    return Rect.new(x, y, w, h)
  }

  scrollTop { _scrollTop }
  scrollTop_=(t) {
    ensureLayout_()
    var maxTop = (_lines.count - viewportRows_).max(0)
    var clamped = t.max(0).min(maxTop)
    if (clamped == _scrollTop) return
    _scrollTop = clamped
    markDirty()
  }

  // Content row count.  UIFC reserves a 1-row blank pad above and
  // below the content area so text never touches the frame.
  viewportRows_ {
    if (bounds == null) return 0
    return (bounds.h - 4).max(0)         // -2 frame, -2 padding
  }
  // First content row: frame (0) + top padding (1) → row 2.
  contentTop_ { 2 }
  // Inner-height rows used by the scrollbar / separator (the
  // padding rows next to the scrollbar are blank — the scrollbar
  // visually fills the inner column from frame to frame).
  innerRows_ {
    if (bounds == null) return 0
    return (bounds.h - 2).max(0)
  }
  scrollbarVisible_ { _lines.count > viewportRows_ }
  scrollbarOverhead_ {
    if (!scrollbarVisible_) return 0
    return _sbSep ? 2 : 1
  }
  scrollbarColumn_ {
    if (_sbSide == "right") return bounds.w - 2     // last interior col
    return 1                                          // just past left frame
  }
  separatorColumn_ {
    if (!_sbSep) return -1
    if (_sbSide == "right") return bounds.w - 3
    return 2
  }

  // First surface column where text content lives.  Content is
  // flush against the scrollbar (or its separator) on the
  // scrollbar side and gets a 1-cell padding on the opposite
  // side.  When no scrollbar is visible, both sides are padded.
  contentLeft_ {
    if (scrollbarVisible_ && _sbSide != "right") {
      return _sbSep ? 3 : 2     // just past sep / scrollbar
    }
    return 2                     // 1-cell padding from left frame
  }
  // One past the last surface column where text lives — exclusive,
  // so `contentRight_ - contentLeft_` is the wrappable width.
  contentRight_ {
    if (scrollbarVisible_ && _sbSide == "right") {
      return _sbSep ? bounds.w - 3 : bounds.w - 2
    }
    return bounds.w - 2          // 1-cell padding from right frame
  }

  viewportCols_ {
    if (bounds == null) return 0
    return (contentRight_ - contentLeft_).max(0)
  }

  // ----- Painting --------------------------------------------------

  onPaint_() {
    super.onPaint_()                      // frame + (null) message
    var sf = surface
    var ib = innerBounds
    if (ib == null) return
    ensureLayout_()

    var rows = viewportRows_
    var cols = viewportCols_
    var cx   = contentLeft_
    var cy   = contentTop_
    var i = 0
    while (i < rows && _scrollTop + i < _lines.count) {
      var ln = _lines[_scrollTop + i]
      var x  = cx
      var remaining = cols
      for (run in ln.runs) {
        if (remaining <= 0) break
        var st = style(run.role)
        var n  = run.text.count
        if (n > remaining) n = remaining
        Painter.text(sf, x, cy + i, run.text, st, n)
        x = x + n
        remaining = remaining - n
      }
      i = i + 1
    }

    if (_lines.count > rows) drawScrollbar_(rows)
  }

  // Scrollbar / separator span the full inner height (rows 1 ..
  // bounds.h-2) — the top/bottom padding only inset *text*, not
  // the scroll surface.  scrollbarClick takes the scrollbar height
  // and the viewport row count separately so the maxScroll math
  // (total - viewport) and the thumb sizing both stay consistent.
  drawScrollbar_(rows) {
    var sf         = surface
    var trackStyle = style("scrollbar.track")
    var thumbStyle = style("scrollbar.thumb")
    var glyphs     = effectiveTheme.glyphs
    var sbH        = innerRows_
    Painter.scrollbar(sf, scrollbarColumn_, 1, sbH, _scrollTop,
                      _lines.count, rows, glyphs, trackStyle, thumbStyle)
    if (_sbSep) {
      var sepStyle = style("default")
      var sepGlyph = glyph("frame.left")
      var sx = separatorColumn_
      var i = 0
      while (i < sbH) {
        Painter.fill(sf, Rect.new(sx, 1 + i, 1, 1), sepGlyph, sepStyle)
        i = i + 1
      }
    }
  }

  // ----- Event handling -------------------------------------------

  handle(ev) {
    // Lay out before any handler so they can read _lines.count
    // and viewportRows_ without ordering surprises (the End handler,
    // for example, passes _lines.count as the target scroll offset).
    ensureLayout_()
    if (ev is KeyEvent && handleKey_(ev)) return true
    if (ev is MouseEvent && handleMouse_(ev)) return true
    // Fall through to Pane/Popup so the frame's [?] / [■] buttons
    // and any other inherited handling still work.
    return super.handle(ev)
  }

  handleKey_(ke) {
    var c = ke.code
    if (c == Key.escape || c == Key.enter) {
      dismissWith_(null)
      return true
    }
    if (c == Key.up) {
      scrollTop_ = _scrollTop - 1
      return true
    }
    if (c == Key.down) {
      scrollTop_ = _scrollTop + 1
      return true
    }
    if (c == Key.pageUp) {
      scrollTop_ = _scrollTop - viewportRows_
      return true
    }
    if (c == Key.pageDown) {
      scrollTop_ = _scrollTop + viewportRows_
      return true
    }
    if (c == Key.home) {
      scrollTop_ = 0
      return true
    }
    if (c == Key.end) {
      scrollTop_ = _lines.count
      return true
    }
    return false
  }

  static show(app, titleText, body) {
    var h = Help.new(titleText, body)
    h.bounds = Help.centeredBounds_()
    app.modal(h)
    return null
  }

  handleMouse_(me) {
    if (bounds == null) return false
    if (!bounds.contains(me.startX, me.startY)) return false
    var e = me.event

    // Wheel: shift the viewport.
    if (e == Mouse.wheelUpPress || e == Mouse.wheelUpClick) {
      scrollTop_ = _scrollTop - Help.wheelStep
      return true
    }
    if (e == Mouse.wheelDownPress || e == Mouse.wheelDownClick) {
      scrollTop_ = _scrollTop + Help.wheelStep
      return true
    }

    // Scrollbar drag/click.  Surface column = scrollbarColumn_; in
    // screen coords that's bounds.x + scrollbarColumn_.  Convert
    // endY to a scrollbar-local row (0-based, accounting for the
    // top frame) and let Painter.scrollbarClick resolve the action.
    // The scrollbar spans innerRows_ (full inner height); the
    // viewport is shorter (viewportRows_) because of the top/bottom
    // text padding.  scrollbarClick wants both separately —
    // maxScroll = total - viewport, but the click position maps
    // through the scrollbar height.
    if (e == Mouse.button1Press || e == Mouse.button1Click ||
        e == Mouse.button1DragStart || e == Mouse.button1DragMove) {
      if (scrollbarVisible_ &&
          me.startX == bounds.x + scrollbarColumn_) {
        var py = me.endY - bounds.y - 1
        scrollTop_ = Painter.scrollbarClick(py, innerRows_, _lines.count,
                                            viewportRows_, _scrollTop)
        return true
      }
    }
    return false
  }
}
