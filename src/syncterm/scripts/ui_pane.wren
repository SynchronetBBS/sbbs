// SyncTERM Wren UI library — Pane.
//
// A Pane is a Container with a frame and an optional title.  It
// claims its full bounds (frame + interior) and lays children out
// within `innerBounds`.  The frame uses the theme's "frame" /
// "frame.inactive" Style and either the "frame.control.*" or
// "frame.display.*" glyphs;
// the title uses "title" / "title.inactive".
//
// UIFC convention: the focused pane is the "default" (active) look;
// every other pane is dimmed via the *.inactive variants.
//
// Knobs:
//   frameKind     — "control" (default) or "display".  Control-bearing
//                   panes use `╔═╗║╚╝`; informational panes use
//                   `┌─┐│└┘`.
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
import "syncterm"  for MouseEvent, Mouse, Screen

class Pane is Container {
  // UIFC-style defaults: control frame, title in its own bar
  // row, [?] and [■] corner buttons.  Sub-classes (Popup, PopStatus)
  // override what doesn't fit their look.
  construct new() {
    super()
    _title       = null
    _frameKind   = "control"
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

  frameKind      { _frameKind }
  frameKind=(s) {
    if (s != "control" && s != "display") {
      Fiber.abort("Pane.frameKind must be \"control\" or \"display\"")
    }
    _frameKind = s
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

  // Glyph prefix derived from the frame's semantic purpose.
  glyphPrefix_ {
    return "frame." + _frameKind
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

  // UIFC-style padded interior: innerBounds inset by 1 cell on
  // every side so text never touches the frame.  Popup-style
  // widgets (Alert / Confirm / Prompt / PopStatus / Help) lay
  // themselves out inside this; Panes that want full-frame access
  // (e.g. a ListView in the SFTP browser) keep using innerBounds
  // directly.  Width / height are clamped to >= 0 so a too-small
  // pane returns a degenerate (rather than negative) rect.
  contentBounds {
    var ib = innerBounds
    if (ib == null) return null
    return Rect.new(ib.x + 1, ib.y + 1,
                    (ib.w - 2).max(0), (ib.h - 2).max(0))
  }

  // Per-axis frame overhead (= bounds size minus innerBounds size).
  // Horizontal is always 2 (left + right border); vertical is the
  // top inset (1 plain, 3 with title bar) plus 1 for the bottom
  // border.  Used by fitContent_ to size around a child's
  // preferredWidth/Height.
  frameOverheadW_ { 2 }
  frameOverheadH_ { (titleBarActive_ ? 3 : 1) + 1 }

  // Outer width needed to display the title without truncation.
  // The two title modes have different geometry:
  //   * titleAsBar  — title is centered on its own row inside the
  //     frame; reserve a 1-cell padding on each side so the title
  //     never butts up against the side frame.  Width = title + 4
  //     (frame(2) + padding(2)).
  //   * frameTitle  — title sits in the top frame border between
  //     `─ ` and ` ─` brackets; rendered cap is `bounds.w - 6`
  //     (corners(2) + brackets(2) + spaces(2)), so width = title + 6.
  // Returns 0 when there's no title to fit.
  titleOuterWidth_ {
    if (_title == null) return 0
    var t = _title.bytes.count
    return t + (titleBarActive_ ? 4 : 6)
  }

  // Outer width needed to display the corner buttons without
  // overrunning either side corner.  Close (3 cells) and help
  // (3 cells) sit on the top frame border starting at col 1, so
  // the bar needs at least button_total + 2 (corners).  In
  // titleAsBar mode the buttons share row 0 with no title text,
  // so this is independent of titleOuterWidth_.  In frameTitle
  // mode the buttons share row 0 *with* the centered title; the
  // pane is only guaranteed to be wide enough for both when the
  // larger of the two outer-width requests wins.
  cornerButtonsOuterWidth_ {
    var bw = 0
    if (_closeable) bw = bw + 3
    if (_helpable && (_onHelp != null || helpText != null)) bw = bw + 3
    return bw == 0 ? 0 : bw + 2
  }

  // Auto-size the pane around a single child's preferredWidth /
  // preferredHeight, plus whatever the title bar and corner buttons
  // demand.  Re-measure after assigning the resulting child bounds
  // so size-dependent preferences (notably ListView scrollbars) settle
  // in one call.  Falls back to existing bounds (or sensible defaults)
  // for any axis the child doesn't declare.  If the pane already
  // has bounds, its (x, y) is preserved; otherwise the pane is
  // parked at (1, 1) until centerOnScreen / a parent layout moves
  // it.  No-op when there are no children to measure.
  //
  // Multi-child panes need to lay themselves out manually — this
  // helper is the single-content-widget convenience.
  fitContent() { fitContent_(null, null) }

  // Constrained form of fitContent.  Limits apply to the complete
  // pane, including its frame and title bar.
  fitContent(maximumWidth, maximumHeight) {
    fitContent_(maximumWidth, maximumHeight)
  }

  fitContent_(maximumWidth, maximumHeight) {
    if (children.count == 0) return
    var c = children[0]
    var x = bounds == null ? 1 : bounds.x
    var y = bounds == null ? 1 : bounds.y
    var pass = 0
    // The first pass establishes the child's viewport.  The second
    // incorporates preferences which depend on that viewport.
    while (pass < 2) {
      var pw = c.preferredWidth
      var ph = c.preferredHeight
      if (pw == null && ph == null) return
      var innerW = pw == null ? (bounds == null ? 20 : bounds.w - frameOverheadW_) : pw
      var innerH = ph == null ? (bounds == null ? 5 : bounds.h - frameOverheadH_) : ph
      if (innerW < 1) innerW = 1
      if (innerH < 1) innerH = 1
      var outerW = innerW + frameOverheadW_
      // Title and corner-button widths are independent outer-width
      // demands; pick whichever is largest so neither gets clipped.
      var titleW = titleOuterWidth_
      var buttonsW = cornerButtonsOuterWidth_
      if (titleW > outerW) outerW = titleW
      if (buttonsW > outerW) outerW = buttonsW
      var outerH = innerH + frameOverheadH_
      if (maximumWidth != null) outerW = outerW.min(maximumWidth)
      if (maximumHeight != null) outerH = outerH.min(maximumHeight)
      bounds = Rect.new(x, y, outerW.max(1), outerH.max(1))
      c.bounds = innerBounds
      pass = pass + 1
    }
  }

  // Size content within the standard modal margins and center the
  // result.  The horizontal and vertical margins reserve the full
  // two-column right shadow and one-row bottom shadow.
  fitContentToScreen() {
    var limits = screenFitLimits_
    fitContent_(limits[0], limits[1])
    centerOnScreen()
  }

  // Apply a requested fixed size within the same modal limits.  This
  // is the fixed-size counterpart to fitContentToScreen, used by panes
  // such as preformatted Help viewers.
  fitToScreen(width, height) {
    var limits = screenFitLimits_
    var w = width.min(limits[0]).max(1)
    var h = height.min(limits[1]).max(1)
    bounds = Rect.new(1, 1, w, h)
    centerOnScreen()
  }

  screenFitLimits_ {
    var size = Screen.size
    return [(size[0] - 4).max(1), (size[1] - 2).max(1)]
  }

  // Reposition the pane so it's centered on the current Screen.
  // Requires bounds to be set; pairs naturally with fitContent
  // (`pane.fitContent(); pane.centerOnScreen()`).  Off-screen sizes
  // are honoured -- the caller is expected to keep the pane within
  // the screen rect themselves.
  centerOnScreen() {
    if (bounds == null) return
    var sz = Screen.size
    var x = ((sz[0] - bounds.w) / 2).floor + 1
    var y = ((sz[1] - bounds.h) / 2).floor + 1
    bounds = Rect.new(x, y, bounds.w, bounds.h)
    if (children.count > 0) children[0].bounds = innerBounds
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
    if (ev is MouseEvent && ev.event == Mouse.button1Click) {
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
