// SyncTERM Wren UI library — Pane.
//
// A Pane is a Container with a frame and an optional title.  It
// claims its full bounds (frame + interior) and lays children out
// within `innerBounds`.  The frame uses the theme's "frame" /
// "frame.inactive" Style and "frame.*" (or "frame.double.*") glyphs;
// the title uses "title" / "title.inactive".
//
// UIFC convention: the focused pane is the "default" (active) look;
// every other pane is dimmed via the *.inactive variants.
//
// Knobs:
//   framePreset   — "single" (default) or "double".  Switches the
//                   glyph family from `┌─┐│└┘` to `╔═╗║╚╝`.  UIFC
//                   list/menu boxes use "double".
//   titleAsBar    — when true, the title sits inside the frame in
//                   its own row with a horizontal separator
//                   underneath, instead of embedded in the top
//                   border.  Common for UIFC list views.
//   helpable      — show a `[?]` button in the top-left of the frame.
//                   Click triggers `onHelp`, or App.showHelp() if no
//                   handler is wired.
//   closeable     — show a `[X]` button in the top-right of the
//                   frame.  Click triggers `onClose`.
//
// Pane.focused is a *visual* flag — set it from the App or a parent
// container when the pane is the foreground.  It does NOT redirect
// keyboard focus; that still flows through Container's normal
// focused-child routing to a leaf widget inside.

import "ui_widget" for Widget, Container, Rect
import "ui_draw"   for Painter
import "syncterm"  for MouseEvent, Mouse

class Pane is Container {
  // UIFC-style defaults: double-line frame, title in its own bar
  // row, [?] and [■] corner buttons.  Sub-classes (Popup, PopStatus)
  // override what doesn't fit their look.
  construct new() {
    super()
    _title       = null
    _framePreset = "double"
    _titleAsBar  = true
    _helpable    = true
    _closeable   = true
    _onHelp      = null
    _onClose     = null
  }

  title { _title }
  title=(s) {
    _title = s
    markDirty()
  }

  framePreset      { _framePreset }
  framePreset=(s) {
    _framePreset = s
    markDirty()
  }

  titleAsBar      { _titleAsBar }
  titleAsBar=(b) {
    _titleAsBar = b
    markDirty()
  }

  helpable      { _helpable }
  helpable=(b) {
    _helpable = b
    markDirty()
  }
  closeable      { _closeable }
  closeable=(b) {
    _closeable = b
    markDirty()
  }
  onHelp=(fn)  { _onHelp = fn }
  onClose=(fn) { _onClose = fn }

  // Glyph prefix derived from the preset.
  glyphPrefix_ {
    if (_framePreset == "double") return "frame.double"
    return "frame"
  }

  // True when the title takes its own row inside the frame.
  titleBarActive_ { _titleAsBar && _title != null }

  // Surface-local rects of the corner buttons.  UIFC stacks them on
  // the left of the top border with `[■]` (close) first, then `[?]`
  // (help) immediately after.  Either may be hidden independently;
  // when only help is shown it occupies the close slot.
  closeButtonRect_ {
    if (!_closeable || bounds == null || bounds.w < 5) return null
    return Rect.new(1, 0, 3, 1)
  }
  helpButtonRect_ {
    if (!_helpable || bounds == null) return null
    // Only show [?] when help is actually wired — either a
    // pane-specific onHelp callback, or this widget has helpText
    // App.showHelp can find when it walks the focused chain.  A
    // visible button that does nothing is worse than no button.
    if (_onHelp == null && helpText == null) return null
    var x = _closeable ? 4 : 1
    if (bounds.w < x + 4) return null
    return Rect.new(x, 0, 3, 1)
  }

  // The drawable interior.  In titleAsBar mode the top is inset by
  // an extra 2 rows (title row + separator row).
  innerBounds {
    if (bounds == null) return null
    var topInset = titleBarActive_ ? 3 : 1
    return Rect.new(bounds.x + 1, bounds.y + topInset,
                    bounds.w - 2, bounds.h - topInset - 1)
  }

  // Pane gates the active-layer status of its subtree on its own
  // `focused` flag.  When the pane is unfocused, every cell it owns
  // (frame, title, bg fill) AND every descendant's surface picks the
  // inactive theme variant — so multi-pane layouts go dim
  // consistently instead of just along the chrome.
  gatesActiveLayer { true }

  onPaint_() {
    var theme   = effectiveTheme
    // No `focused ? ... : ...` shortcut here — Widget.style auto-
    // suffixes `.inactive` when this Pane (or an unfocused ancestor)
    // gates the layer, so all three styles agree on active-vs-
    // inactive without the per-element ternary having to know.
    var fStyle  = style("frame")
    var tStyle  = style("title")
    var bgStyle = style("default")
    var rect    = Rect.new(0, 0, bounds.w, bounds.h)
    var s       = surface
    var pre     = glyphPrefix_

    Painter.fill(s, rect, " ", bgStyle)
    if (titleBarActive_) {
      Painter.frame(s, rect, theme.glyphs, fStyle, pre)
      paintTitleBar_(s, fStyle, tStyle, pre)
    } else if (_title != null) {
      Painter.frameTitle(s, rect, theme.glyphs, fStyle, _title, tStyle, pre)
    } else {
      Painter.frame(s, rect, theme.glyphs, fStyle, pre)
    }
    paintCornerButtons_(s, fStyle)
    compositeChildren_()
  }

  // Title centred on row 1, separator across row 2 with side tees.
  paintTitleBar_(s, fStyle, tStyle, pre) {
    var glyphs = effectiveTheme.glyphs
    var iw     = bounds.w - 2
    if (iw <= 0) return
    var msg = _title
    var trim = msg
    if (msg.count > iw) trim = msg[0...iw]
    var col = ((iw - trim.count) / 2).floor + 1
    Painter.text(s, col, 1, trim, tStyle, iw)
    Painter.fill(s, Rect.new(0, 2, 1, 1),
                 glyphs[pre + ".tee.left"],  fStyle)
    Painter.fill(s, Rect.new(bounds.w - 1, 2, 1, 1),
                 glyphs[pre + ".tee.right"], fStyle)
    Painter.hline(s, 1, 2, bounds.w - 2,
                  glyphs[pre + ".separator"], fStyle)
  }

  // Paint the [?] / [■] buttons with the brackets in frame style and
  // the inner glyph in "button" text style — bright white so it
  // reads against the frame chrome regardless of pane.focused.
  paintCornerButtons_(s, fStyle) {
    var hr = helpButtonRect_
    if (hr != null) paintCornerLabel_(s, hr.x, hr.y, "?", fStyle)
    var cr = closeButtonRect_
    if (cr != null) paintCornerLabel_(s, cr.x, cr.y, "■", fStyle)
  }
  paintCornerLabel_(s, x, y, ch, fStyle) {
    var btn = style("button")
    Painter.text(s, x,     y, "[", fStyle, 1)
    Painter.text(s, x + 1, y, ch, btn,    1)
    Painter.text(s, x + 2, y, "]", fStyle, 1)
  }

  // Mouse: intercept clicks on the corner buttons before they fall
  // through to whatever's under them.  Container's hitTest returns
  // `this` for clicks inside our bounds that don't land on a child,
  // so the App routes those clicks here.
  handle(ev) {
    if (ev is MouseEvent &&
        (ev.event == Mouse.button1Click || ev.event == Mouse.button1Press)) {
      if (bounds != null) {
        var sx = ev.startX - bounds.x
        var sy = ev.startY - bounds.y
        var hr = helpButtonRect_
        if (hr != null && hr.contains(sx, sy)) {
          triggerHelp_()
          return true
        }
        var cr = closeButtonRect_
        if (cr != null && cr.contains(sx, sy)) {
          triggerClose_()
          return true
        }
      }
    }
    return super.handle(ev)
  }

  triggerHelp_() {
    if (_onHelp != null) {
      _onHelp.call()
      return
    }
    // Walk up the parent chain past the Widget tree to the App and
    // invoke its showHelp.  App responds to `showHelp()` (defined in
    // ui_app.wren); if there's no App parent, do nothing.
    var w = parent
    while (w is Widget) w = w.parent
    if (w != null) w.showHelp()
  }

  triggerClose_() {
    if (_onClose != null) _onClose.call()
  }
}
