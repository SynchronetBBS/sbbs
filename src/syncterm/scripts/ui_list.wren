// SyncTERM Wren UI library — List.
//
// A scrollable list of items with a selection cursor and an optional
// scrollbar.  Items can be anything — Strings, file entries, etc. —
// and rendering goes through `formatItem(item, width)` which
// subclasses override for rich per-row formatting (file size + date,
// columns, ANSI codes after a renderer pass, …).  Default formatting
// is `item.toString`.
//
// Navigation:  Up/Down/PageUp/PageDown/Home/End move the cursor.
// Enter fires the `onSelect` callback (if set) with (index, item).
// Mouse clicks select the row under the pointer; clicks on the
// scrollbar column jump the view proportionally.
//
// Theme roles consulted:
//   list.item            — unselected rows
//   list.item.focused    — the selected row (always shown highlighted,
//                          regardless of widget focus state — let App
//                          drive multi-pane styling itself if needed)
//   scrollbar.track      — scrollbar background
//   scrollbar.thumb      — scrollbar thumb
// Theme glyphs:
//   scrollbar.track, scrollbar.thumb

import "ui_style"  for Style
import "ui_widget" for Widget, Rect
import "ui_draw"   for Painter
import "syncterm"  for KeyEvent, MouseEvent, Key, Mouse

class ListView is Widget {
  construct new() {
    super()
    _items      = []
    _selected   = -1
    _scrollTop  = 0
    _showScroll = true
    _onSelect   = null
    _sbSide     = "left"        // "left" (UIFC) or "right"
    _sbSep      = true          // separator between scrollbar and content
  }

  items { _items }
  items=(list) {
    _items     = list
    _selected  = list.count > 0 ? 0 : -1
    _scrollTop = 0
    markDirty()
  }

  count { _items.count }

  // null when no row is selected (empty list, or explicitly cleared);
  // otherwise an integer in 0..count - 1.
  selected { _selected < 0 ? null : _selected }
  selected=(i) {
    if (i == null || _items.count == 0) {
      if (_selected == -1) return
      _selected = -1
      markDirty()
      return
    }
    var clamped = i.max(0).min(_items.count - 1)
    if (clamped == _selected) return
    _selected = clamped
    ensureVisible_()
    markDirty()
  }

  selectedItem {
    if (_selected < 0 || _selected >= _items.count) return null
    return _items[_selected]
  }

  scrollTop { _scrollTop }
  scrollTop=(t) {
    var maxTop = 0
    if (bounds != null) maxTop = (_items.count - bounds.h).max(0)
    var clamped = t.max(0).min(maxTop)
    if (clamped == _scrollTop) return
    _scrollTop = clamped
    markDirty()
  }

  showScroll { _showScroll }
  showScroll=(b) {
    _showScroll = b
    markDirty()
  }

  // Scrollbar layout knobs.  Default is UIFC: scrollbar on the left
  // with a `│` separator between it and the content.  Set side to
  // "right" for the older / web-style layout, or separator to false
  // to drop the divider.
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

  onSelect=(fn) { _onSelect = fn }

  scrollbarVisible_ {
    if (bounds == null) return false
    return _showScroll && _items.count > bounds.h
  }
  scrollbarOverhead_ {
    if (!scrollbarVisible_) return 0
    return _sbSep ? 2 : 1
  }
  scrollbarColumn_ {
    if (_sbSide == "right") return bounds.w - 1
    return 0
  }
  separatorColumn_ {
    if (!_sbSep) return -1
    if (_sbSide == "right") return bounds.w - 2
    return 1
  }

  // Per-side inset between the widget edge and the row content.
  // The scrollbar (when present) provides its own visual separation;
  // the bare side gets a single padding cell so items never touch
  // the frame the parent Pane draws around us.
  leftInset_ {
    if (scrollbarVisible_ && _sbSide != "right") return scrollbarOverhead_
    return 1
  }
  rightInset_ {
    if (scrollbarVisible_ && _sbSide == "right") return scrollbarOverhead_
    return 1
  }

  innerWidth {
    if (bounds == null) return 0
    var w = bounds.w - leftInset_ - rightInset_
    return w.max(0)
  }

  contentX_ { leftInset_ }

  // Auto-layout: smallest cell budget that displays every item
  // without truncation.  Width sums the longest item's display
  // length and both insets (1 cell of breathing space on the
  // frameless side(s) plus whatever the scrollbar would take if
  // visible — assumed not visible at the natural height, see
  // preferredHeight).  Height = items.count, clamped to at least 1
  // so an empty list still occupies a row.
  preferredWidth {
    var maxLen = 0
    for (item in _items) {
      var s = formatItem(item, 1024)
      var n = s.bytes.count
      if (n > maxLen) maxLen = n
    }
    return maxLen + leftInset_ + rightInset_
  }
  preferredHeight { _items.count.max(1) }

  // Subclass hook: format `item` into a String.  `width` is the cell
  // budget for the row; implementations may pad / truncate / column-
  // align as they see fit.  Default returns `item.toString`.
  formatItem(item, width) { item.toString }

  // Park the hardware cursor on the first cell of the selected row.
  // Some terminals visibly track the cursor, and screen readers / BBS
  // doors that read the cursor position depend on it.  Returns null
  // when there's no selection or the bounds aren't laid out yet.
  cursorPos {
    if (bounds == null || _selected < 0) return null
    var rowOffset = _selected - _scrollTop
    if (rowOffset < 0 || rowOffset >= bounds.h) return null
    return [bounds.x + contentX_, bounds.y + rowOffset]
  }

  // Navigation primitives.  Each clamps via selected= and is a no-op
  // when there's nothing to select.
  up()       { selected = _selected - 1 }
  down()     { selected = _selected + 1 }
  pageUp()   { selected = _selected - viewportRows_ + 1 }
  pageDown() { selected = _selected + viewportRows_ - 1 }
  home()     { selected = 0 }
  end()      { selected = _items.count - 1 }

  viewportRows_ {
    if (bounds == null) return 1
    return bounds.h.max(1)
  }

  // Adjust _scrollTop so the current selection is visible.  Called
  // automatically from selected=.
  ensureVisible_() {
    if (bounds == null || _selected < 0) return
    var h = bounds.h
    if (h <= 0) return
    if (_selected < _scrollTop) {
      _scrollTop = _selected
    } else if (_selected >= _scrollTop + h) {
      _scrollTop = _selected - h + 1
    }
  }

  // Paint into our own Surface (0-based coords).  Read it through
  // Widget's `surface` getter — `_surface` here would refer to a new
  // ListView-scoped field, not Widget's.
  onPaint_() {
    var sf = surface
    var bg = style("default")
    Painter.fill(sf, Rect.new(0, 0, bounds.w, bounds.h), " ", bg)

    var iw = innerWidth
    var h  = bounds.h
    var cx = contentX_

    // Row highlight spans the full widget width, excluding the
    // scrollbar column when one is shown — both the content area
    // and the padding cells get the row's background so the
    // selected-row lightbar reads as a single edge-to-edge bar.
    var fillX = (scrollbarVisible_ && _sbSide != "right") ? scrollbarOverhead_ : 0
    var fillW = bounds.w - (scrollbarVisible_ ? scrollbarOverhead_ : 0)
    var i = 0
    while (i < h) {
      var idx = _scrollTop + i
      if (idx >= _items.count) break
      var s    = formatItem(_items[idx], iw)
      var role = idx == _selected ? "list.item.focused" : "list.item"
      var st   = style(role)
      Painter.fill(sf, Rect.new(fillX, i, fillW, 1), " ", st)
      Painter.text(sf, cx, i, s, st, iw)
      i = i + 1
    }

    if (_showScroll && _items.count > h) drawScrollbar_()
  }

  drawScrollbar_() {
    var sf         = surface
    var h          = bounds.h
    var trackStyle = style("scrollbar.track")
    var thumbStyle = style("scrollbar.thumb")
    var glyphs     = effectiveTheme.glyphs
    Painter.scrollbar(sf, scrollbarColumn_, 0, h, _scrollTop,
                      _items.count, h, glyphs, trackStyle, thumbStyle)
    if (_sbSep) {
      var sepStyle = style("default")
      var sepGlyph = glyph("frame.left")
      var sx = separatorColumn_
      var i = 0
      while (i < h) {
        Painter.fill(sf, Rect.new(sx, i, 1, 1), sepGlyph, sepStyle)
        i = i + 1
      }
    }
  }

  handle(ev) {
    if (ev is KeyEvent) return handleKey_(ev)
    if (ev is MouseEvent) return handleMouse_(ev)
    return false
  }

  handleKey_(ke) {
    var c = ke.code
    if (c == Key.up) {
      up()
      return true
    }
    if (c == Key.down) {
      down()
      return true
    }
    if (c == Key.pageUp) {
      pageUp()
      return true
    }
    if (c == Key.pageDown) {
      pageDown()
      return true
    }
    if (c == Key.home) {
      home()
      return true
    }
    if (c == Key.end) {
      end()
      return true
    }
    if (c == Key.enter) {
      if (_onSelect != null && selectedItem != null) {
        _onSelect.call(_selected, selectedItem)
      }
      return true
    }
    return false
  }

  // How many rows a single wheel notch moves the viewport.
  static wheelStep { 3 }

  handleMouse_(me) {
    if (bounds == null) return false
    if (!bounds.contains(me.startX, me.startY)) return false
    var e = me.event

    // Scroll wheel: shift the viewport by wheelStep rows.  Backends
    // emit either a PRESS or a CLICK depending on platform; accept
    // both.  Selection is not touched.
    if (e == Mouse.wheelUpPress || e == Mouse.wheelUpClick) {
      scrollTop = _scrollTop - ListView.wheelStep
      return true
    }
    if (e == Mouse.wheelDownPress || e == Mouse.wheelDownClick) {
      scrollTop = _scrollTop + ListView.wheelStep
      return true
    }

    // Only react to button-1 press / click / drag.  Mouse-move events
    // (event == 0) reach handle() too — without this gate, just hovering
    // over the scrollbar would scroll.
    if (e != Mouse.button1Press && e != Mouse.button1Click &&
        e != Mouse.button1DragStart && e != Mouse.button1DragMove) {
      return false
    }

    // For drags, endX/endY follow the mouse while startX/startY stay
    // pinned at the original press point.  Hit-test the *grab* with
    // start (so dragging off the scrollbar still scrolls), but resolve
    // the new scrollTop from the *current* position via end.
    if (scrollbarVisible_ &&
        me.startX == bounds.x + scrollbarColumn_) {
      var py = me.endY - bounds.y
      scrollTop = Painter.scrollbarClick(py, bounds.h, _items.count,
                                         bounds.h, _scrollTop)
      return true
    }

    // Outside the scrollbar: only press/click selects a row.  Drag
    // events fall through so dispatchMouse_ can hand them to the
    // C-side selector for text selection on the visible cells.
    if (e != Mouse.button1Press && e != Mouse.button1Click) {
      return false
    }
    var idx = _scrollTop + (me.endY - bounds.y)
    if (idx >= 0 && idx < _items.count) {
      selected = idx
      return true
    }
    return false
  }
}
