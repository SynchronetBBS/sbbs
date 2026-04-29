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
  }

  items { _items }
  items=(list) {
    _items     = list
    _selected  = list.count > 0 ? 0 : -1
    _scrollTop = 0
    markDirty()
  }

  count { _items.count }

  selected { _selected }
  selected=(i) {
    if (_items.count == 0) {
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

  onSelect=(fn) { _onSelect = fn }

  // Available cell width for item content — bounds.w minus the
  // scrollbar column when one will be drawn (showScroll on AND
  // items overflow the viewport height).
  innerWidth {
    if (bounds == null) return 0
    if (_showScroll && _items.count > bounds.h) return bounds.w - 1
    return bounds.w
  }

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
    return [bounds.x, bounds.y + rowOffset]
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

    var i = 0
    while (i < h) {
      var idx = _scrollTop + i
      if (idx >= _items.count) break
      var s    = formatItem(_items[idx], iw)
      var role = idx == _selected ? "list.item.focused" : "list.item"
      var st   = style(role)
      // Pad the row to iw so the highlight extends across the full
      // item line, even when the formatted text is shorter.
      Painter.fill(sf, Rect.new(0, i, iw, 1), " ", st)
      Painter.text(sf, 0, i, s, st, iw)
      i = i + 1
    }

    if (_showScroll && _items.count > h) drawScrollbar_()
  }

  drawScrollbar_() {
    var sf = surface
    var x = bounds.w - 1
    var h = bounds.h
    var trackStyle = style("scrollbar.track")
    var thumbStyle = style("scrollbar.thumb")
    var trackGlyph = glyph("scrollbar.track")
    var thumbGlyph = glyph("scrollbar.thumb")

    // Thumb height is proportional to viewport / total.  Thumb Y maps
    // [0..maxScroll] linearly onto [0..h-thumbH], so the thumb hits the
    // bottom exactly when scrollTop is at its max.  Naive `h*scrollTop /
    // count` undershoots after .floor and leaves a track-coloured gap
    // at the bottom when fully scrolled.
    var thumbH    = (h * h / _items.count).floor.max(1)
    var maxScroll = _items.count - h
    var thumbY    = 0
    if (maxScroll > 0) {
      thumbY = ((h - thumbH) * _scrollTop / maxScroll).floor
    }

    var i = 0
    while (i < h) {
      var inThumb = i >= thumbY && i < thumbY + thumbH
      var st = inThumb ? thumbStyle : trackStyle
      var g  = inThumb ? thumbGlyph : trackGlyph
      Painter.fill(sf, Rect.new(x, i, 1, 1), g, st)
      i = i + 1
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
    // start (so dragging off the scrollbar still scrolls), but compute
    // the selected index from the *current* position with end.
    // Scrollbar column?  Only when actually drawn (overflow + showScroll).
    // Scrollbar moves the *viewport* (scrollTop), not the selection —
    // standard scrollbar behavior.  The selection stays put; clicking
    // on a row is what changes selection.
    if (_showScroll && _items.count > bounds.h &&
        me.startX == bounds.right) {
      // Map py ∈ [0..h-1] linearly onto scrollTop ∈ [0..maxTop].
      var py = (me.endY - bounds.y).max(0).min(bounds.h - 1)
      var maxTop = _items.count - bounds.h
      var t = bounds.h > 1 ? (py * maxTop / (bounds.h - 1)).floor : 0
      scrollTop = t
      return true
    }

    // Otherwise: click (or drag) on a row picks it.
    var idx = _scrollTop + (me.endY - bounds.y)
    if (idx >= 0 && idx < _items.count) {
      selected = idx
      return true
    }
    return false
  }
}
