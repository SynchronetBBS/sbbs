// SyncTERM Wren UI library — Help.
//
// A modal scrollable text viewer.  Splits the supplied text on
// newlines and renders them inside a Popup with its own scroll
// state (no selected-line concept — every arrow press scrolls one
// row, no highlight follows the user around).
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

import "ui_widget" for Rect
import "ui_popup"  for Popup
import "ui_draw"   for Painter
import "syncterm"  for KeyEvent, MouseEvent, Mouse, Key, Screen

class Help is Popup {
  construct new(titleText, body) {
    super(null)                 // no auto-rendered message line
    title      = titleText
    focused    = true
    _lines     = Help.splitLines_(body)
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

  static splitLines_(s) {
    if (s == null || s == "") return [""]
    var out  = []
    var line = ""
    for (c in s) {
      if (c == "\n") {
        out.add(line)
        line = ""
      } else {
        line = line + c
      }
    }
    out.add(line)
    return out
  }

  static centeredBounds_() {
    var sz = Screen.size
    var w  = (sz[0] - 4).max(20)
    var h  = (sz[1] - 4).max(8)
    var x  = ((sz[0] - w) / 2).floor + 1
    var y  = ((sz[1] - h) / 2).floor + 1
    return Rect.new(x, y, w, h)
  }

  scrollTop { _scrollTop }
  scrollTop_=(t) {
    var maxTop = (_lines.count - viewportRows_).max(0)
    var clamped = t.max(0).min(maxTop)
    if (clamped == _scrollTop) return
    _scrollTop = clamped
    markDirty()
  }

  // Inner area row count, defensive against missing bounds.  The
  // scrollbar steals the rightmost column when content overflows;
  // viewport height isn't affected.
  viewportRows_ {
    if (bounds == null) return 0
    return (bounds.h - 2).max(0)         // -2 for top + bottom frame
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

  viewportCols_ {
    if (bounds == null) return 0
    var w = bounds.w - 2                  // frame on each side
    w = w - scrollbarOverhead_
    return w.max(0)
  }

  // First surface column where text content lives.
  contentLeft_ {
    if (!scrollbarVisible_ || _sbSide == "right") return 1
    return _sbSep ? 3 : 2
  }

  // ----- Painting --------------------------------------------------

  onPaint_() {
    super.onPaint_()                      // frame + (null) message
    var sf = surface
    var st = style("default")
    var ib = innerBounds
    if (ib == null) return

    var rows = viewportRows_
    var cols = viewportCols_
    var cx   = contentLeft_
    var i = 0
    while (i < rows && _scrollTop + i < _lines.count) {
      var ln = _lines[_scrollTop + i]
      Painter.text(sf, cx, 1 + i, ln, st, cols)
      i = i + 1
    }

    if (_lines.count > rows) drawScrollbar_(rows)
  }

  drawScrollbar_(rows) {
    var sf         = surface
    var trackStyle = style("scrollbar.track")
    var thumbStyle = style("scrollbar.thumb")
    var glyphs     = effectiveTheme.glyphs
    Painter.scrollbar(sf, scrollbarColumn_, 1, rows, _scrollTop,
                      _lines.count, rows, glyphs, trackStyle, thumbStyle)
    if (_sbSep) {
      var sepStyle = style("default")
      var sepGlyph = glyph("frame.left")
      var sx = separatorColumn_
      var i = 0
      while (i < rows) {
        Painter.fill(sf, Rect.new(sx, 1 + i, 1, 1), sepGlyph, sepStyle)
        i = i + 1
      }
    }
  }

  // ----- Event handling -------------------------------------------

  handle(ev) {
    if (ev is KeyEvent)   return handleKey_(ev)
    if (ev is MouseEvent) return handleMouse_(ev)
    return false
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
    if (e == Mouse.button1Press || e == Mouse.button1Click ||
        e == Mouse.button1DragStart || e == Mouse.button1DragMove) {
      var rows = viewportRows_
      if (scrollbarVisible_ &&
          me.startX == bounds.x + scrollbarColumn_) {
        var py = me.endY - bounds.y - 1
        scrollTop_ = Painter.scrollbarClick(py, rows, _lines.count,
                                            rows, _scrollTop)
        return true
      }
    }
    return false
  }
}
