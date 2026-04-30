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

  static centeredBounds_(message) {
    var sz   = Screen.size
    var msgW = (message == null ? 0 : message.count)
    var w    = (msgW + 4).max(20).min(sz[0] - 4)
    var h    = 3                              // top frame + 1 msg + bot frame
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
    var ib  = innerBounds
    var iw  = ib.w
    var msg = _message
    var trim = msg
    if (msg.count > iw) trim = msg[0...iw]
    var col = ((iw - trim.count) / 2).floor + 1
    Painter.text(sf, col, 1, trim, st, iw)
  }
}

class Popup is Pane {
  construct new(message) {
    super()
    _message = message
    _result  = null
    focused  = true
    shadow   = true                // dialogs always cast a drop-shadow
    // Popups use a single-line frame with title embedded in the top
    // border (UIFC convention for Yes/No / Alert / Prompt).
    framePreset = "single"
    titleAsBar  = false
    helpable    = false            // few popups have context help
    // [X] click dismisses with null — same as Esc in every Popup subclass.
    onClose = Fn.new { dismissWith_(null) }
  }

  message { _message }
  result  { _result  }

  // Pre-compute Pane bounds centred on the screen.  Layout reserves
  // 2 frame rows + 1 message row, then `extraRows` for whatever the
  // subclass paints below.  `minW` lets the caller bump the minimum
  // width past the default 20 cells when their button row is wider.
  static centeredBounds_(message, extraRows, minW) {
    var sz   = Screen.size                    // [w, h]
    var msgW = (message == null ? 0 : message.count)
    var w    = (msgW + 4).max(minW).min(sz[0] - 4)
    var h    = 3 + extraRows                  // top+bot frame + 1 msg + extra
    var x    = ((sz[0] - w) / 2).floor + 1
    var y    = ((sz[1] - h) / 2).floor + 1
    return Rect.new(x, y, w, h)
  }

  // Paint frame + title (Pane), then the message in row 1 of the
  // interior centred horizontally.  Subclasses extend by overriding
  // and calling super.onPaint_.  Children draw on top via the
  // Container compositeChildren_ pass that Pane.onPaint_ runs.
  onPaint_() {
    super.onPaint_()
    if (_message == null) return
    var sf  = surface
    var st  = style("default")
    var ib  = innerBounds
    var iw  = ib.w
    var msg = _message
    var trim = msg
    if (msg.count > iw) trim = msg[0...iw]
    var col = ((iw - trim.count) / 2).floor + 1   // +1 for left frame
    Painter.text(sf, col, 1, trim, st, iw)
  }

  // Subclasses dismiss by calling dismissWith_(value) — writing
  // `_result` directly from a subclass method would create a new
  // subclass-scoped field instead of mutating Popup's, since Wren
  // instance fields are class-scoped (wren.md §3.1).
  dismissWith_(value) {
    _result = value
    if (parent != null) parent.popModal()
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

  // Position the OK button on row 2 (in surface coords), centred.
  bounds=(r) {
    super.bounds = r
    var ib = innerBounds
    if (ib == null) return
    var bw = _ok.intrinsicWidth
    var bx = ib.x + ((ib.w - bw) / 2).floor
    _ok.bounds = Rect.new(bx, ib.y + 1, bw, 1)
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

  // Side-by-side Yes / No, centred on row 2.
  bounds=(r) {
    super.bounds = r
    var ib = innerBounds
    if (ib == null) return
    var yw  = _yes.intrinsicWidth
    var nw  = _no.intrinsicWidth
    var gap = 4
    var total = yw + gap + nw
    var sx    = ib.x + ((ib.w - total) / 2).floor
    var row   = ib.y + 1
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

  // Input fills row 2; OK / Cancel sit side-by-side on row 3.
  bounds=(r) {
    super.bounds = r
    var ib = innerBounds
    if (ib == null) return
    var iw = (ib.w - 2).max(4)
    _input.bounds = Rect.new(ib.x + 1, ib.y + 1, iw, 1)
    var ow = _ok.intrinsicWidth
    var cw = _cancel.intrinsicWidth
    var gap   = 4
    var total = ow + gap + cw
    var sx    = ib.x + ((ib.w - total) / 2).floor
    var row   = ib.y + 2
    _ok.bounds     = Rect.new(sx, row, ow, 1)
    _cancel.bounds = Rect.new(sx + ow + gap, row, cw, 1)
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
