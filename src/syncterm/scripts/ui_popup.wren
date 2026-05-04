// SyncTERM Wren UI library — Popup, Alert, Confirm, Prompt.
//
// Modal dialogs that push themselves onto an App's modal stack, run
// the App's drain loop until dismissed, then return a result.  Built
// on Pane (frame + title + interior) with Button children (and, for
// Prompt, a TextInput).
//
// Usage:
//   Alert.show(app, "Disk full")
//   if (Confirm.show(app, "Delete file?")) { ... }
//   var name = Prompt.show(app, "Your name?", "default")
//   if (name != null) { ... }   // null = cancelled
//
// Each `show` is synchronous: the App's `modal()` pumps drain until
// the popup pops itself, then control returns.
//
// Keyboard:
//   Alert    — any key (or Enter / Esc) dismisses; mouse-click OK works too.
//   Confirm  — Y / Enter on Yes → true, N / Esc / Enter on No → false,
//              Tab cycles focus between Yes / No.
//   Prompt   — Enter in input or on OK button → submits the value,
//              Esc / Cancel button → null.  Tab cycles input ↔ OK ↔ Cancel.
//
// Sizing: each popup auto-fits its message + minimum-button-row
// against the screen, then centres itself.  Caller can override
// `bounds` after construction for fixed placement.

import "ui_widget" for Rect
import "ui_pane"   for Pane
import "ui_input"  for TextInput
import "ui_button" for Button
import "ui_draw"   for Painter
import "syncterm"  for KeyEvent, Key, Screen

// Transient status overlay used by App.popStatus(msg) — a frame
// with a centred message and no buttons.  Not pushed onto the modal
// stack: it's pure decoration, the App composites it on top of the
// foreground without intercepting input.
class PopStatus is Pane {
  construct new(message) {
    super()
    title       = "Status"
    _message    = message
    focusable   = false
    focused     = true       // visually active; doesn't take keyboard focus
    shadow      = true
    // Status overlay is dismissed programmatically (popStatus(null))
    // and has no per-pane help — opt out of the corner buttons.  Use
    // a single-line frame to keep it compact.
    framePreset = "single"
    titleAsBar  = false
    helpable    = false
    closeable   = false
  }

  message { _message }

  // Width: msgW + 2 (frame) + 2 (padding); height: 5 (frame top +
  // padding row + msg + padding row + frame bot).  Capped at the
  // screen with a 2-cell margin like every other popup.
  static centeredBounds_(message) {
    var sz   = Screen.size
    var msgW = (message == null ? 0 : message.count)
    var w    = (msgW + 4).max(20).min(sz[0] - 4)
    var h    = 5
    var x    = ((sz[0] - w) / 2).floor + 1
    var y    = ((sz[1] - h) / 2).floor + 1
    return Rect.new(x, y, w, h)
  }

  static show(message) {
    var p = PopStatus.new(message)
    p.bounds = PopStatus.centeredBounds_(message)
    return p
  }

  onPaint_() {
    super.onPaint_()
    if (_message == null) return
    var sf  = surface
    var st  = style("default")
    var cb  = contentBounds
    if (cb == null) return
    var iw  = cb.w
    var msg = _message
    var trim = msg
    if (msg.count > iw) trim = msg[0...iw]
    // Surface coords: cb.x - bounds.x is the leftmost content col,
    // cb.y - bounds.y is the top content row.  Both account for
    // the frame + UIFC padding cell.
    var col = (cb.x - bounds.x) + ((iw - trim.count) / 2).floor
    var row = cb.y - bounds.y
    Painter.text(sf, col, row, trim, st, iw)
  }
}

class Popup is Pane {
  construct new(message) {
    super()
    _message      = message
    _result       = null
    _preformatted = false
    _onDismiss    = null
    focused       = true
    shadow        = true              // dialogs always cast a drop-shadow
    // Popups use a single-line frame with title embedded in the top
    // border (UIFC convention for Yes/No / Alert / Prompt).
    framePreset = "single"
    titleAsBar  = false
    helpable    = false            // few popups have context help
    // [X] click dismisses with null — same as Esc in every Popup subclass.
    onClose = Fn.new { dismissWith_(null) }
  }

  message       { _message      }
  result        { _result       }
  // Preformatted mode left-aligns all wrapped lines at a single
  // column chosen so the *block* is centred — preserves indentation
  // and column-aligned text in things like long file descriptions.
  preformatted     { _preformatted }
  preformatted=(b) { _preformatted = b }

  // Number of rows the message occupies after wrapping.  Subclasses'
  // bounds= setters consult this to anchor buttons / input fields
  // immediately below the message — `ib.y + msgRows` is the first
  // free row.  Recomputed on each access; cheap for short messages,
  // and bounds= is the only place that calls it during layout.
  msgRows {
    if (_message == null) return 1
    var cb = contentBounds
    if (cb == null) return 1
    var lines
    if (_preformatted) {
      lines = Popup.splitHardLines_(_message)
    } else {
      lines = Popup.wrap_(_message, cb.w)
    }
    return lines.count.max(1)
  }

  // Pre-compute Pane bounds centred on the screen.  Layout reserves
  // 2 frame rows + 2 padding rows + however many rows the wrapped
  // message needs + `extraRows` for whatever the subclass paints
  // below the message.  `minW` lets callers bump the minimum width
  // past 20 cells when their button row is wider.  Width is sized
  // to the longest hard-line in the message + 4 (frame + padding),
  // capped to the screen width — short messages stay snug, long
  // ones wrap.  Pass preformatted=true to count rows by hard breaks
  // only (no word-wrapping) so a tall ASCII-art block gets the
  // height it needs.
  static centeredBounds_(message, extraRows, minW) {
    return centeredBounds_(message, extraRows, minW, false)
  }
  static centeredBounds_(message, extraRows, minW, preformatted) {
    var sz   = Screen.size                    // [w, h]
    var maxW = (sz[0] - 4).max(minW)
    var longest = longestHardLine_(message)
    var w  = (longest + 4).max(minW).min(maxW)   // +4 = frame(2)+padding(2)
    var iw = (w - 4).max(1)
    var rows = 1
    if (message != null) {
      var lines
      if (preformatted) {
        lines = splitHardLines_(message)
      } else {
        lines = wrap_(message, iw)
      }
      rows = lines.count.max(1)
    }
    var h    = 4 + rows + extraRows              // 2 frame + 2 padding
    var maxH = sz[1] - 2
    if (h > maxH) h = maxH
    var x = ((sz[0] - w) / 2).floor + 1
    var y = ((sz[1] - h) / 2).floor + 1
    return Rect.new(x, y, w, h)
  }

  // Length (codepoints ≅ display cells for the strings we draw) of
  // the longest line bounded by CR / LF / EOS.  Used by
  // centeredBounds_ to size the popup wide enough to avoid wrapping
  // when the screen has room.  Counts codepoints (`.count`) rather
  // than bytes so multi-byte UTF-8 (e.g. box-drawing) doesn't inflate
  // the measurement.
  static longestHardLine_(message) {
    if (message == null) return 0
    var lines = splitHardLines_(message)
    var longest = 0
    for (line in lines) {
      if (line.count > longest) longest = line.count
    }
    return longest
  }

  // Split `msg` on hard breaks (CR, LF, CRLF) without word-wrapping.
  // Used by preformatted popups: caller wants the lines as-is and
  // sized the popup wide enough to fit them.  Lines wider than `iw`
  // get truncated by Painter.text rather than re-wrapped.
  static splitHardLines_(msg) {
    if (msg == null) return []
    var n = msg.bytes.count
    var i = 0
    var lineStart = 0
    var out = []
    while (i < n) {
      var ch = msg[i]
      if (ch == "\n" || ch == "\r") {
        out.add(msg[lineStart...i])
        if (ch == "\r" && i + 1 < n && msg[i + 1] == "\n") {
          i = i + 1
        }
        lineStart = i + 1
      }
      i = i + 1
    }
    out.add(msg[lineStart...n])
    return out
  }

  // Split `msg` into display lines for a `width`-cell column.  Honours
  // explicit CR / LF / CRLF breaks and word-wraps long lines (hard-
  // break for words that exceed the column).  Byte-indexed throughout
  // — fine for ASCII / CP437 message text.
  static wrap_(msg, width) {
    if (msg == null || width <= 0) return []
    var bytes = msg.bytes
    var n = bytes.count
    var out = []
    var lineStart = 0
    var i = 0
    while (i < n) {
      var ch = msg[i]
      if (ch == "\n" || ch == "\r") {
        wrapInto_(msg, lineStart, i, width, out)
        if (ch == "\r" && i + 1 < n && msg[i + 1] == "\n") {
          i = i + 1
        }
        lineStart = i + 1
      }
      i = i + 1
    }
    wrapInto_(msg, lineStart, n, width, out)
    return out
  }

  // Push msg[start...end] into out, word-wrapped at width.  Empty
  // input emits a single blank entry so paragraph-separator newlines
  // render as visible blank rows.
  static wrapInto_(msg, start, end, width, out) {
    if (start >= end) {
      out.add("")
      return
    }
    while (end - start > width) {
      var brk = start + width
      while (brk > start && msg[brk] != " ") brk = brk - 1
      if (brk == start) brk = start + width      // hard break (no space)
      out.add(msg[start...brk])
      while (brk < end && msg[brk] == " ") brk = brk + 1
      start = brk
    }
    if (start < end) out.add(msg[start...end])
  }

  // Paint frame + title (Pane), then each message line in the
  // interior.  Default mode word-wraps and centres each line
  // individually.  Preformatted mode splits only on hard breaks (no
  // word-wrap) and left-aligns all lines at one block-column so the
  // block is centred while each line keeps its relative shape.
  onPaint_() {
    super.onPaint_()
    if (_message == null) return
    var sf = surface
    var st = style("default")
    var cb = contentBounds
    if (cb == null) return
    var iw   = cb.w
    var sx0  = cb.x - bounds.x          // surface col of leftmost content cell
    var sy0  = cb.y - bounds.y          // surface row of topmost content cell
    var lines
    if (_preformatted) {
      lines = Popup.splitHardLines_(_message)
    } else {
      lines = Popup.wrap_(_message, iw)
    }
    var blockCol = sx0
    if (_preformatted) {
      var longest = 0
      for (line in lines) {
        if (line.count > longest) longest = line.count
      }
      blockCol = sx0 + ((iw - longest) / 2).floor
    }
    var i = 0
    while (i < lines.count) {
      var line = lines[i]
      var col  = blockCol
      if (!_preformatted) col = sx0 + ((iw - line.count) / 2).floor
      Painter.text(sf, col, sy0 + i, line, st, iw)
      i = i + 1
    }
  }

  // Optional dismissal callback, fired by dismissWith_ AFTER the
  // popup has popped itself.  Use to wire dismissal to enclosing-app
  // bookkeeping — typically `fn = Fn.new { |v| app.quit() }` so a
  // standalone-popup driver (one that starts an App with the popup
  // as its only widget) can exit `app.run()` once the user has
  // chosen.  Receives the Bool result (or whatever value the
  // subclass dismissed with).
  onDismiss=(fn) { _onDismiss = fn }

  // Subclasses dismiss by calling dismissWith_(value) — writing
  // `_result` directly from a subclass method would create a new
  // subclass-scoped field instead of mutating Popup's, since Wren
  // instance fields are class-scoped (wren.md §3.1).
  dismissWith_(value) {
    _result = value
    if (parent != null) parent.popModal()
    if (_onDismiss != null) _onDismiss.call(value)
  }
}

class Alert is Popup {
  construct new(message) {
    super(message)
    title = "Alert"
    _ok = Button.new("OK")
    _ok.onPress = Fn.new { dismissWith_(null) }
    add(_ok)
  }

  static show(app, message) {
    var p = Alert.new(message)
    p.bounds = Popup.centeredBounds_(message, 1, 20)
    app.modal(p)
    return null
  }

  // Preformatted variant — block-centre instead of per-line centre.
  // Suitable for file descriptions / log excerpts / anything whose
  // column alignment matters.  centeredBounds_ uses split-on-hard-
  // breaks for the row count so a tall ASCII-art block gets the
  // height it needs.
  static showPreformatted(app, message) {
    var p = Alert.new(message)
    p.preformatted = true
    p.bounds = Popup.centeredBounds_(message, 1, 20, true)
    app.modal(p)
    return null
  }

  // OK button sits hard against the bottom frame, centred
  // horizontally inside the padded content area.  The blank row
  // between the wrapped message and the button is the natural
  // visual separator (it falls out of the height math: 2 frame +
  // 1 top pad + msgRows + 1 separator + 1 button = 5 + msgRows).
  bounds=(r) {
    super.bounds = r
    var cb = contentBounds
    if (cb == null) return
    var bw = _ok.intrinsicWidth
    var bx = cb.x + ((cb.w - bw) / 2).floor
    _ok.bounds = Rect.new(bx, bounds.y + bounds.h - 2, bw, 1)
  }

  // Esc dismisses directly; everything else (Enter, Space, mouse
  // click) flows through to the OK button via Container's focus
  // dispatch.  Random letters fall through and don't dismiss, so
  // the OK button feels like an actual confirmation step.
  handle(ev) {
    if (ev is KeyEvent && ev.code == Key.escape) {
      dismissWith_(null)
      return true
    }
    return super.handle(ev)
  }
}

class Confirm is Popup {
  construct new(message) {
    super(message)
    title = "Confirm"
    _yes = Button.new("Yes")
    _no  = Button.new("No")
    _yes.onPress = Fn.new { dismissWith_(true) }
    _no.onPress  = Fn.new { dismissWith_(false) }
    add(_yes)
    add(_no)
  }

  static show(app, message) {
    var p = Confirm.new(message)
    p.bounds = Popup.centeredBounds_(message, 1, 24)
    app.modal(p)
    return p.result
  }

  // Side-by-side Yes / No hard against the bottom frame, centred
  // horizontally.  The blank row between the message and the
  // button row is the visual separator.
  bounds=(r) {
    super.bounds = r
    var cb = contentBounds
    if (cb == null) return
    var yw  = _yes.intrinsicWidth
    var nw  = _no.intrinsicWidth
    var gap = 4
    var total = yw + gap + nw
    var sx    = cb.x + ((cb.w - total) / 2).floor
    var row   = bounds.y + bounds.h - 2
    _yes.bounds = Rect.new(sx, row, yw, 1)
    _no.bounds  = Rect.new(sx + yw + gap, row, nw, 1)
  }

  // Y / N letter shortcuts and Esc dismiss directly; everything else
  // (Enter, Tab, mouse click on a button) flows through to Container's
  // focus-aware dispatch so the focused button activates.
  handle(ev) {
    if (ev is KeyEvent) {
      var c  = ev.code
      var cp = ev.codepoint
      if (cp == 0x59 || cp == 0x79) {
        dismissWith_(true)
        return true
      }
      if (cp == 0x4E || cp == 0x6E) {
        dismissWith_(false)
        return true
      }
      if (c == Key.escape) {
        dismissWith_(false)
        return true
      }
    }
    return super.handle(ev)
  }
}

class Prompt is Popup {
  construct new(message, initial) {
    super(message)
    title  = "Prompt"
    _input = TextInput.new()
    _input.value = (initial == null ? "" : initial)
    _input.onSubmit = Fn.new {|s| dismissWith_(s) }
    add(_input)
    _ok     = Button.new("OK")
    _cancel = Button.new("Cancel")
    _ok.onPress     = Fn.new { dismissWith_(_input.value) }
    _cancel.onPress = Fn.new { dismissWith_(null) }
    add(_ok)
    add(_cancel)
  }

  static show(app, message) { show(app, message, null) }
  static show(app, message, initial) {
    var p = Prompt.new(message, initial)
    p.bounds = Popup.centeredBounds_(message, 2, 30)
    app.modal(p)
    return p.result
  }

  // OK / Cancel hard against the bottom frame; input sits one
  // row above them.  The blank row between the message and the
  // input is the visual separator.
  bounds=(r) {
    super.bounds = r
    var cb = contentBounds
    if (cb == null) return
    var iw = cb.w.max(4)
    var btnRow   = bounds.y + bounds.h - 2
    var inputRow = btnRow - 1
    _input.bounds = Rect.new(cb.x, inputRow, iw, 1)
    var ow = _ok.intrinsicWidth
    var cw = _cancel.intrinsicWidth
    var gap   = 4
    var total = ow + gap + cw
    var sx    = cb.x + ((cb.w - total) / 2).floor
    _ok.bounds     = Rect.new(sx, btnRow, ow, 1)
    _cancel.bounds = Rect.new(sx + ow + gap, btnRow, cw, 1)
  }

  // Esc cancels; Enter / Tab / mouse fall through to the focused
  // child (TextInput or one of the buttons).  TextInput's onSubmit
  // wires Enter-in-the-input to dismissWith_(value).
  handle(ev) {
    if (ev is KeyEvent && ev.code == Key.escape) {
      dismissWith_(null)
      return true
    }
    return super.handle(ev)
  }
}

// Compact single-row prompt used for inline find dialogs (ListView's
// Ctrl-F).  Title sits in the top frame border (UIFC convention);
// the input row uses the full innerBounds (no top/bottom padding,
// no buttons).  Enter submits the value; Esc returns null.  The [X]
// corner button is hidden so the title bar reads as a clean
// `┤ Find ├`.
class Find is Popup {
  construct new(label, initial) {
    super(null)
    title       = label
    closeable   = false
    _input      = TextInput.new()
    _input.value = (initial == null ? "" : initial)
    _input.onSubmit = Fn.new {|s| dismissWith_(s) }
    _input.focused  = true
    add(_input)
  }

  static show(app, label) { show(app, label, null) }
  static show(app, label, initial) {
    var p = Find.new(label, initial)
    p.bounds = Find.centeredBounds_(label)
    app.modal(p)
    return p.result
  }

  // Width: title (with `┤  ├` brackets) + a minimum input area.
  // Height: 3 — top frame, single input row, bottom frame.  No
  // padding rows: this is meant to be a quick, low-overhead prompt.
  static centeredBounds_(label) {
    var sz = Screen.size
    var lb = (label == null) ? 0 : label.bytes.count
    var w  = (lb + 6 + 20).min(sz[0] - 4).max(20)
    var h  = 3
    var x  = ((sz[0] - w) / 2).floor + 1
    var y  = ((sz[1] - h) / 2).floor + 1
    return Rect.new(x, y, w, h)
  }

  // Single-row layout — input fills the entire innerBounds row,
  // bypassing the contentBounds 1-cell padding (Find is tighter
  // than the message popups).
  bounds=(r) {
    super.bounds = r
    var ib = innerBounds
    if (ib == null) return
    _input.bounds = Rect.new(ib.x, ib.y, ib.w, 1)
  }

  handle(ev) {
    if (ev is KeyEvent && ev.code == Key.escape) {
      dismissWith_(null)
      return true
    }
    return super.handle(ev)
  }
}
