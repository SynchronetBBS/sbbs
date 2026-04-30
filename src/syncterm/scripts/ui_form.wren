// SyncTERM Wren UI library — Form.
//
// A `Container` that lays out (label, widget) pairs in a vertical
// stack with right-aligned labels in a column on the left, and the
// input widget filling the remaining width.  Optional OK / Cancel
// buttons sit on the row below the last field.
//
//   var f = Form.new()
//   f.bounds = pane.innerBounds
//   f.addField("Name:",   nameInput)
//   f.addField("Tags:",   tagsInput)
//   f.addField("Active:", activeCheckbox)
//   f.onSubmit = Fn.new { /* read field values */ }
//   f.onCancel = Fn.new { app.quit() }
//   pane.add(f)
//
// `addField(labelText, widget)` queues a row.  `bounds=` triggers
// layout so children land at the right place; reassign bounds after
// changing the field list to recompute.
//
// `onSubmit` and `onCancel` may be set to functions; when set, OK /
// Cancel buttons appear on the bottom row.  If neither is set, no
// button row is created.
//
// Tab order: every field's widget in declaration order, then OK,
// then Cancel.  Up / Down do their usual spatial nearest-focusable
// scan.  The Form itself doesn't intercept any keys; Container does
// the right thing because `addField` calls `add(widget)` for the
// field widget (and add() for the buttons), so Tab traversal hits
// them in the order they were added.
//
// Theme role consulted: "default" (for the label cells).  Labels
// render in the Pane's text style — they're not a separate themable
// element.

import "ui_widget" for Widget, Container, Rect
import "ui_button" for Button
import "ui_radio"  for RadioGroup
import "ui_draw"   for Painter
import "syncterm"  for KeyEvent, Key

class Form is Container {
  construct new() {
    super()
    _fields    = []        // list of [labelText, widget]
    _onSubmit  = null
    _onCancel  = null
    _ok        = null
    _cancel    = null
    _labelW    = 0         // computed at layout time
    _rowH      = 1         // height per field
    _rowGap    = 0         // blank rows between fields
  }

  // List of [labelText, widget] in insertion order.  Read-only from
  // the outside; mutate via addField / clearFields.
  fields { _fields }

  // Per-row height (some widgets — RadioGroup — need more than 1).
  // Default 1; set before adding fields if every field needs more.
  // For mixed heights, use addFieldH(label, widget, h) per row.
  rowH     { _rowH }
  rowH=(n) { _rowH = n }

  rowGap     { _rowGap }
  rowGap=(n) { _rowGap = n }

  onSubmit=(fn) {
    _onSubmit = fn
    ensureButtons_()
  }
  onCancel=(fn) {
    _onCancel = fn
    ensureButtons_()
  }

  // Per-field height: stored as the third element of the row.  When
  // omitted (addField), defaults to _rowH at layout time.
  addField(labelText, widget) { addFieldH(labelText, widget, null) }
  addFieldH(labelText, widget, h) {
    _fields.add([labelText, widget, h])
    add(widget)
    // RadioGroup wraps Up/Down internally by default — fine standalone
    // but not in a Form, where Up at the top of the list should move
    // focus to the previous field instead of cycling within the group.
    if (widget is RadioGroup) widget.wrap = false
    if (bounds != null) layout_()
    return widget
  }

  clearFields() {
    for (f in _fields) remove(f[1])
    _fields = []
    if (bounds != null) layout_()
  }

  // Re-layout when the bounds change.
  bounds=(r) {
    super.bounds = r
    layout_()
  }

  // Lazily create the OK / Cancel buttons when a callback is set.
  // Idempotent — calling twice doesn't double-add.
  ensureButtons_() {
    if (_onSubmit != null && _ok == null) {
      _ok = Button.new("OK")
      _ok.onPress = Fn.new {
        if (_onSubmit != null) _onSubmit.call()
      }
      add(_ok)
    } else if (_onSubmit == null && _ok != null) {
      remove(_ok)
      _ok = null
    }
    if (_onCancel != null && _cancel == null) {
      _cancel = Button.new("Cancel")
      _cancel.onPress = Fn.new {
        if (_onCancel != null) _onCancel.call()
      }
      add(_cancel)
    } else if (_onCancel == null && _cancel != null) {
      remove(_cancel)
      _cancel = null
    }
    if (bounds != null) layout_()
  }

  // Compute the maximum label width and lay each child out.
  layout_() {
    if (bounds == null) return
    _labelW = 0
    for (f in _fields) {
      var lw = f[0].count
      if (lw > _labelW) _labelW = lw
    }
    var labelGap   = _labelW > 0 ? 2 : 0    // " " between label and widget
    var widgetX    = bounds.x + _labelW + labelGap
    var widgetW    = (bounds.w - _labelW - labelGap).max(0)
    var y          = bounds.y
    for (f in _fields) {
      var w = f[1]
      var h = f[2] != null ? f[2] : _rowH
      w.bounds = Rect.new(widgetX, y, widgetW, h)
      y = y + h + _rowGap
    }
    if (_ok != null || _cancel != null) {
      var bw1 = _ok     != null ? _ok.intrinsicWidth     : 0
      var bw2 = _cancel != null ? _cancel.intrinsicWidth : 0
      var gap = (_ok != null && _cancel != null) ? 4 : 0
      var total = bw1 + bw2 + gap
      var bx = bounds.x + ((bounds.w - total) / 2).floor
      if (bx < bounds.x) bx = bounds.x
      var by = y + 1                        // blank row above the buttons
      if (_ok != null) {
        _ok.bounds = Rect.new(bx, by, bw1, 1)
        bx = bx + bw1 + gap
      }
      if (_cancel != null) {
        _cancel.bounds = Rect.new(bx, by, bw2, 1)
      }
    }
  }

  // Paint labels into our Surface, then composite children on top.
  // Labels are right-aligned in the label column.
  onPaint_() {
    var sf = surface
    var st = style("default")
    Painter.fill(sf, Rect.new(0, 0, bounds.w, bounds.h), " ", st)
    var y = 0
    for (f in _fields) {
      var lbl = f[0]
      var n   = lbl.count
      var pad = (_labelW - n).max(0)
      Painter.text(sf, pad, y, lbl, st, _labelW)
      var h = f[2] != null ? f[2] : _rowH
      y = y + h + _rowGap
    }
    compositeChildren_()
  }

  // Esc cancels (when wired); Container handles Tab/arrows naturally.
  handle(ev) {
    if (ev is KeyEvent && ev.code == Key.escape) {
      if (_onCancel != null) {
        _onCancel.call()
        return true
      }
    }
    return super.handle(ev)
  }
}
