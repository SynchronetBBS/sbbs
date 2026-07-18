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
import "ui_popup"  for Find
import "syncterm"  for KeyEvent, MouseEvent, Key, Mouse, Screen

class ListView is Widget {
  construct new() {
    super()
    _items         = []
    _selected      = -1
    _scrollTop     = 0
    _showScroll    = true
    _onSelect      = null
    _onChange      = null
    _wrap          = true
    _sbSide        = "left"        // "left" (UIFC) or "right"
    _sbSep         = true          // separator between scrollbar and content
    _searchBuf     = ""             // type-to-search rolling buffer
    _lastSearch    = ""             // most recent Ctrl-F query (for Ctrl-G)
    _selectionMode = "single"       // "single" or "tag"
    _tagged        = null           // List<Bool> in tag mode, else null
  }

  items { _items }
  items=(list) {
    _items     = list
    _selected  = list.count > 0 ? 0 : -1
    _scrollTop = 0
    _searchBuf = ""
    if (_selectionMode == "tag") rebuildTagged_()
    notifyChange_()
    markDirty()
  }

  // "single" (default) or "tag".  In tag mode, Space toggles a
  // per-item flag and `tagged` returns the indices that are flagged
  // on.  Tagged rows are visually marked with a `•` glyph in the
  // leftmost content column.  Switching modes resets the tag state.
  selectionMode { _selectionMode }
  selectionMode=(m) {
    if (m != "single" && m != "tag") return
    if (_selectionMode == m) return
    _selectionMode = m
    if (m == "tag") {
      rebuildTagged_()
    } else {
      _tagged = null
    }
    markDirty()
  }

  // List<Int> of currently-tagged item indices, in ascending order.
  // Returns an empty list when not in tag mode.
  tagged {
    var out = []
    if (_tagged == null) return out
    var i = 0
    while (i < _tagged.count) {
      if (_tagged[i]) out.add(i)
      i = i + 1
    }
    return out
  }

  // Toggle the tag at index `i`.  No-op outside tag mode or when
  // `i` is out of range.
  toggleTagged(i) {
    if (_tagged == null) return
    if (i < 0 || i >= _tagged.count) return
    _tagged[i] = !_tagged[i]
    markDirty()
  }

  rebuildTagged_() {
    _tagged = []
    for (i in 0..._items.count) _tagged.add(false)
  }

  count { _items.count }

  // When true (default), Up at the top wraps to the bottom and Down
  // at the bottom wraps to the top.  Set false for lists whose
  // single-row navigation should clamp at the ends.
  wrap     { _wrap }
  wrap=(b) { _wrap = b }

  // null when no row is selected (empty list, or explicitly cleared);
  // otherwise an integer in 0..count - 1.
  selected { _selected < 0 ? null : _selected }
  selected=(i) {
    if (i == null || _items.count == 0) {
      if (_selected == -1) return
      _selected = -1
      notifyChange_()
      markDirty()
      return
    }
    var clamped = i.max(0).min(_items.count - 1)
    if (clamped == _selected) return
    _selected = clamped
    ensureVisible_()
    notifyChange_()
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
  // Fires whenever the current row changes, without activating it.
  // Arguments are (index, item), or (null, null) for an empty list.
  onChange=(fn) { _onChange = fn }

  notifyChange_() {
    if (_onChange == null) return
    _onChange.call(selected, selectedItem)
  }

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
    var w = bounds.w - leftInset_ - rightInset_ - tagMarkerWidth_
    return w.max(0)
  }

  contentX_ { leftInset_ + tagMarkerWidth_ }

  // 1 cell reserved for the tag marker (`•` / blank) when in tag
  // mode, 0 otherwise.  Reduces the text-content width and shifts
  // contentX_ right by the same amount.
  tagMarkerWidth_ { _selectionMode == "tag" ? 1 : 0 }

  // Auto-layout: smallest cell budget that displays every item
  // without truncation.  Width sums the longest item's display
  // length and both insets (1 cell of breathing space on the
  // frameless side(s) plus whatever the scrollbar would take if
  // visible).  Height is the item count clamped to "fits on the
  // screen with chrome budget" — ListView already scrolls, so a
  // 200-item list reports a viewport-shaped height rather than
  // forcing the parent pane to overflow vertically.  Callers that
  // want a specific viewport height can set `bounds` directly.
  preferredWidth {
    var maxLen = 0
    for (item in _items) {
      var s = formatItem(item, 1024)
      var n = s.bytes.count
      if (n > maxLen) maxLen = n
    }
    return maxLen + leftInset_ + rightInset_
  }
  preferredHeight {
    // Cap at screen height minus chrome budget (title bar + help
    // text + borders).  6 rows is the typical Pane overhead.
    var screenH = Screen.size[1]
    var cap     = (screenH - 6).max(1)
    return _items.count.max(1).min(cap)
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
    return [bounds.x + contentX_, bounds.y + rowOffset]
  }

  // Navigation primitives.  Single-row movement wraps by default;
  // page movement and direct selection continue to clamp.
  up() {
    if (_items.count == 0) return
    if (_wrap && _selected <= 0) {
      selected = _items.count - 1
    } else {
      selected = _selected - 1
    }
  }
  down() {
    if (_items.count == 0) return
    if (_wrap && _selected >= _items.count - 1) {
      selected = 0
    } else {
      selected = _selected + 1
    }
  }
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
    var tagged = _selectionMode == "tag"

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
      // Tag-mode marker column: theme `tag.on` / `tag.off` glyph,
      // painted in the same row style so the lightbar reads through.
      if (tagged) {
        var on   = _tagged != null && _tagged[idx]
        var mark = glyph(on ? "tag.on" : "tag.off")
        Painter.text(sf, cx - 1, i, mark, st, 1)
      }
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
    var c  = ke.code
    var cp = ke.codepoint
    // Navigation / activation: consume + clear the type-search
    // buffer so the next typed letter starts a fresh prefix search.
    // Two-pass to keep the per-key dispatch on a single line:
    // first run the action for the matching key, then bundle the
    // shared "clear buffer + return true" tail.
    if (c == Key.up)       up()
    if (c == Key.down)     down()
    if (c == Key.pageUp)   pageUp()
    if (c == Key.pageDown) pageDown()
    if (c == Key.home)     home()
    if (c == Key.end)      end()
    if (c == Key.enter && _onSelect != null && selectedItem != null) {
      _onSelect.call(_selected, selectedItem)
    }
    if (c == Key.up   || c == Key.down  || c == Key.pageUp ||
        c == Key.pageDown || c == Key.home || c == Key.end ||
        c == Key.enter) {
      _searchBuf = ""
      return true
    }
    // Ctrl-F (0x06) — prompt for a substring; Ctrl-G (0x07) — repeat
    // the last query.  Both walk forward from the current selection,
    // wrapping; case-insensitive substring match.
    if (cp == 0x06) {
      runFindPrompt_()
      return true
    }
    if (cp == 0x07) {
      runFindAgain_()
      return true
    }
    // Space toggles the tag at the current row when in tag mode.
    if (cp == 0x20 && _selectionMode == "tag") {
      toggleTagged(_selected)
      return true
    }
    // Type-to-search: any other printable codepoint.  KeyEvent.text
    // is the UTF-8 of the codepoint (or "" for extended keys).
    // Extended keys (function keys, Insert, arrow keys, ...) have
    // codepoint == null; guard before the numeric compare.
    if (cp != null && cp >= 0x20 && cp != 0x7F && ke.text.count > 0) {
      typeahead_(ke.text)
      return true
    }
    return false
  }

  // Append `ch` to the rolling search buffer and jump to the first
  // item whose display text starts with the new buffer (case-
  // insensitive).  If nothing matches, fall back to just `ch` —
  // restart the search with the new keystroke as the prefix.  When
  // even that fails, leave the selection alone but keep the buffer
  // (the next keystroke will keep building on it, in case the user
  // is correcting a typo).
  typeahead_(ch) {
    var attempt = _searchBuf + ch
    var found = findPrefix_(attempt)
    if (found >= 0) {
      _searchBuf = attempt
      selected = found
      return
    }
    found = findPrefix_(ch)
    if (found >= 0) {
      _searchBuf = ch
      selected = found
      return
    }
    _searchBuf = attempt
  }

  // Open a compact Find dialog for a search string.  Walks the
  // parent chain to find the App; no-op when not anchored.  The
  // result becomes `_lastSearch` and drives the immediate jump
  // (and any subsequent Ctrl-G).
  runFindPrompt_() {
    var app = findApp_()
    if (app == null) return
    var q = Find.show(app, "Find", _lastSearch)
    if (q == null || q.count == 0) return
    _lastSearch = q
    var found = findSubstring_(q)
    if (found >= 0) selected = found
  }

  runFindAgain_() {
    if (_lastSearch.count == 0) return
    var found = findSubstring_(_lastSearch)
    if (found >= 0) selected = found
  }

  // Walk parent chain past Widget ancestors to find the App.  Mirrors
  // the pattern in Pane.triggerHelp_.  Returns null when the list
  // isn't anchored (no parent App).
  findApp_() {
    var w = parent
    while (w is Widget) w = w.parent
    return w
  }

  // ----- Search helpers ----------------------------------------

  // Subclass hook: returns the string to search against for `item`.
  // Defaults to the formatItem display string (capped wide so any
  // padding/truncation policy doesn't cut off the term).  Override
  // when display text and search text should differ (e.g., a list
  // of file rows where users type the filename, not the chip-prefixed
  // display string).
  searchTextFor_(item) { formatItem(item, 1024) }

  // Case-insensitive prefix walk: starts at current+1 (or 0 when no
  // selection), wraps, returns the first index whose `searchTextFor_`
  // starts with `prefix`.  -1 when no match.  Empty prefix returns
  // the start index unchanged.
  findPrefix_(prefix) {
    var n = _items.count
    if (n == 0 || prefix.count == 0) return -1
    var start = _selected < 0 ? 0 : (_selected + 1) % n
    var i = 0
    while (i < n) {
      var idx = (start + i) % n
      if (ciStartsWith_(searchTextFor_(_items[idx]), prefix)) return idx
      i = i + 1
    }
    return -1
  }

  // Case-insensitive substring walk: same shape as findPrefix_ but
  // matches anywhere in the search text.
  findSubstring_(needle) {
    var n = _items.count
    if (n == 0 || needle.count == 0) return -1
    var start = _selected < 0 ? 0 : (_selected + 1) % n
    var i = 0
    while (i < n) {
      var idx = (start + i) % n
      if (ciContains_(searchTextFor_(_items[idx]), needle)) return idx
      i = i + 1
    }
    return -1
  }

  // ASCII case-insensitive `startsWith` / `contains` — folds A..Z to
  // a..z byte-wise.  Multi-byte UTF-8 passes through unchanged
  // (matched as raw bytes), which is fine for the BBS-style names
  // we typically search through.  Instance methods (not static) so
  // bare-name calls from findPrefix_ / findSubstring_ resolve via
  // implicit `this`; subclasses inherit them automatically.
  ciStartsWith_(s, prefix) {
    var sb = s.bytes
    var pb = prefix.bytes
    if (pb.count > sb.count) return false
    var i = 0
    while (i < pb.count) {
      if (foldByte_(sb[i]) != foldByte_(pb[i])) return false
      i = i + 1
    }
    return true
  }
  ciContains_(s, needle) {
    var sb = s.bytes
    var nb = needle.bytes
    if (nb.count == 0) return true
    if (nb.count > sb.count) return false
    var last = sb.count - nb.count
    var i = 0
    while (i <= last) {
      var j = 0
      var ok = true
      while (j < nb.count) {
        if (foldByte_(sb[i + j]) != foldByte_(nb[j])) {
          ok = false
          break
        }
        j = j + 1
      }
      if (ok) return true
      i = i + 1
    }
    return false
  }
  foldByte_(b) {
    if (b >= 0x41 && b <= 0x5A) return b + 0x20
    return b
  }

  // UIFC maps each wheel notch to one Up/Down navigation event.
  static wheelStep { 1 }

  handleMouse_(me) {
    if (bounds == null) return false
    if (!bounds.contains(me.startX, me.startY)) return false
    var e = me.event

    // Scroll wheel: move the highlighted row by one item.  Backends
    // emit either a PRESS or a CLICK depending on platform; accept
    // both.
    if (e == Mouse.wheelUpPress || e == Mouse.wheelUpClick) {
      up()
      return true
    }
    if (e == Mouse.wheelDownPress || e == Mouse.wheelDownClick) {
      down()
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
      // Click-to-activate: matches UIFC's `ulist`, which returns
      // immediately on a row click rather than waiting for Enter.
      // Fires only on full clicks (not the initial press of a
      // potential drag) so a click-then-drag for text selection
      // doesn't fire onSelect prematurely.
      if (e == Mouse.button1Click && _onSelect != null &&
          selectedItem != null) {
        _onSelect.call(_selected, selectedItem)
      }
      return true
    }
    return false
  }
}
