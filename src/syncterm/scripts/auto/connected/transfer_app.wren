// SyncTERM Wren — TransferApp.
//
// In-Wren reimplementation of SyncTERM's transfer-window UI: framed
// pane containing a colored log scroller (LogView), four status text
// rows, and a bracketed bar.  Driven by the C-side Transfer.* mailbox
// foreigns: a worker thread runs the protocol code and produces
// events into a thread-safe ring + snapshot pair; this App's tick
// body drains both each frame and renders.
//
// **Caller responsibility — TransferApp does NOT call
// Transfer.beginSession or endSession itself.**  The worker is
// expected to already be running (or about to be) when run() is
// invoked.  Wren-driven demos:
//
//   Transfer.beginSession("fake")
//   var t = TransferApp.new("File Transfer")
//   t.run()                      // blocks until done + dismissed
//   Transfer.endSession()
//
// C-driven protocols use wren_run_transfer(label, worker_fn, arg)
// in wren_bind_xfer.c which spawns the worker before constructing
// TransferApp and joins after run() returns.
//
// Esc / Ctrl+C / Ctrl+X all latch an abort.  The first press appends
// "Abort key pressed" to the LogView (WARNING tint) so the user sees
// their keystroke registered while the protocol unwinds at its own
// pace.  Once Transfer.done flips the App enters a post-transfer
// wait — "Hit any key or wait N seconds to continue..." — that
// dismisses on any key OR on the configurable timeout (default
// 5 seconds).

import "ui_app"      for App
import "ui_pane"     for Pane
import "ui_widget"   for Widget, Rect
import "ui_draw"     for Painter
import "ui_logview"  for LogView
import "ui_popup"    for Confirm, LinePrompt
import "syncterm"    for Screen, Key, KeyEvent, MouseEvent, Input, Timer, Transfer

// Named-field wrapper around the fixed-shape List that
// Transfer.snapshot() returns.  Each protocol's worker pre-formats
// its own line strings (matching what today's C cprintf calls
// produce), keeping the Wren-side renderer protocol-agnostic.
// bytesTotal == null suppresses the auto-rendered percent + bar
// (CET, xmodem-recv); otherwise the panel paints them at rows 3 + 4.
class TickStateView {
  construct new(snap) {
    _line1      = snap[0]
    _line2      = snap[1]
    _line3      = snap[2]
    _line4      = snap[3]
    _bytesCur   = snap[4]
    _bytesTotal = snap[5]
  }

  line1      { _line1 }
  line2      { _line2 }
  line3      { _line3 }
  line4      { _line4 }
  bytesCur   { _bytesCur }
  bytesTotal { _bytesTotal }
}

// Five-row status panel matching today's C transfer-window layout.
//
//   row 0   line1                 (File (N of M): name, etc.)
//   row 1   line2                 (Byte: ... / Block (sz): ...)
//   row 2   line3                 (Time: ... cps)
//   row 3   centered "% nn"  when bytesTotal != null
//           else line4            (CET "Remain")
//   row 4   bracketed [▒▒▒...]   when bytesTotal != null
//           else blank
//
// Caller pushes the latest snapshot in via `state=`; an empty state
// paints blank rows so the panel still occupies its bounds before
// the first tick arrives.
class TransferStatusPanel is Widget {
  construct new() {
    super()
    focusable = false
    _state    = null
  }

  state     { _state }
  state=(s) {
    _state = s
    markDirty()
  }

  cursorVisible { false }

  paintPercent_(sf, w, y, s, st) {
    if (s.bytesTotal == null || s.bytesTotal <= 0) return
    var pct = (s.bytesCur * 100 / s.bytesTotal).floor
    if (pct < 0)   pct = 0
    if (pct > 100) pct = 100
    var lbl = "%(pct)\%"
    var n   = lbl.bytes.count
    var cx  = ((w - n) / 2).floor
    if (cx < 0) cx = 0
    Painter.text(sf, cx, y, lbl, st, w - cx)
  }

  paintBar_(sf, w, y, s, st) {
    // [▒▒▒...   ]  — brackets at the row edges, medium-shade fill
    // (CP437 0xB1 = U+2592) between, sized by current/total.
    if (w < 3) return
    Painter.text(sf, 0,     y, "[", st, 1)
    Painter.text(sf, w - 1, y, "]", st, 1)
    if (s.bytesTotal == null || s.bytesTotal <= 0) return
    var inner  = w - 2
    var cur    = s.bytesCur
    if (cur < 0)             cur = 0
    if (cur > s.bytesTotal)  cur = s.bytesTotal
    var filled = (inner * cur / s.bytesTotal).floor
    if (filled > 0) {
      var fillStyle = style("progress.fill")
      Painter.fill(sf, Rect.new(1, y, filled, 1), "\u2592", fillStyle)
    }
  }

  onPaint_() {
    var sf = surface
    var w  = bounds.w
    var h  = bounds.h
    var st = style("default")
    Painter.fill(sf, Rect.new(0, 0, w, h), " ", st)
    if (_state == null) return
    var s = _state
    Painter.text(sf, 0, 0, s.line1, st, w)
    if (h >= 2) Painter.text(sf, 0, 1, s.line2, st, w)
    if (h >= 3) Painter.text(sf, 0, 2, s.line3, st, w)
    if (h >= 4) {
      if (s.bytesTotal != null && s.bytesTotal > 0) {
        paintPercent_(sf, w, 3, s, st)
      } else {
        Painter.text(sf, 0, 3, s.line4, st, w)
      }
    }
    if (h >= 5 && s.bytesTotal != null && s.bytesTotal > 0) {
      paintBar_(sf, w, 4, s, st)
    }
  }

  // Purely decorative — events fall straight through.
  handle(ev) { false }
}

// One-row connector that overpaints the parent Pane's side frames at
// this row with `╟───╢`, matching today's C transfer-window divider.
// Bounds should span the FULL pane width (including frame columns) so
// the side-tee glyphs land on the existing ║ chars.
class TransferDivider is Widget {
  construct new() {
    super()
    focusable = false
  }
  cursorVisible { false }
  onPaint_() {
    var sf = surface
    var w  = bounds.w
    var st = style("frame")
    Painter.text(sf, 0, 0, "\u255F", st, 1)            // ╟
    Painter.text(sf, w - 1, 0, "\u2562", st, 1)        // ╢
    var i = 1
    while (i < w - 1) {
      Painter.text(sf, i, 0, "\u2500", st, 1)          // ─
      i = i + 1
    }
  }
  handle(ev) { false }
}

class TransferApp is App {
  construct new(label) {
    super()
    _label          = label
    _abortLatched   = false
    _doneLogged     = false
    _aborted        = false
    _success        = false
    _quit           = false
    _waitDeadlineMs = null      // set when Transfer.done flips
    _waitTimeoutMs  = 5000      // default; caller can override before run()
  }

  // Public state available after run() returns.
  aborted { _aborted }
  success { _success }

  // Post-transfer wait timeout (ms) — how long the "Hit any key or
  // wait N seconds" prompt holds before dismissing on its own.  Set
  // to 0 to dismiss immediately on Transfer.done.
  waitTimeoutMs     { _waitTimeoutMs }
  waitTimeoutMs=(n) { _waitTimeoutMs = n }

  // Override App.quit so our own loop sees the request — App's quit
  // only flips _running, which our loop never reads.  We also call
  // super so any code reading the public `running` getter gets the
  // expected value.
  quit() {
    super.quit()
    _quit = true
  }

  // Override App.modal so popup dispatch uses our tick-aware loop.
  // App's stock modal() picks between drainOnce_ (fiber yield) and
  // drainOnceSync_ (Input.next blocking) based on _runMode, neither
  // of which we use; running our custom Input.next(100) loop here
  // keeps key/mouse routing alive while a Confirm/LinePrompt is up.  We
  // intentionally do NOT pump onTick_ inside the modal — the worker
  // is parked waiting on us anyway, so log/tick events won't accrue.
  modal(widget) {
    pushModal(widget)
    while (modalStack.indexOf(widget) >= 0) {
      drawAll_()
      var ev = Input.next(100)
      if (ev is KeyEvent)   dispatchKey_(ev)
      if (ev is MouseEvent) dispatchMouse_(ev)
    }
    return widget
  }

  // Custom loop: runSync() doesn't fire Timer.trigger / Hook.every,
  // and run() needs a doterm-dispatching context.  Polling
  // Input.next(100) gives us a guaranteed tick every 100 ms whether
  // we're nested inside another runSync or not, while still
  // dispatching key / mouse events through the standard App path.
  run() {
    Screen.modalRun(Fn.new {
      buildUi_()
      var savedMouse = setupMouse_()
      setupCursor_()
      while (!_quit) {
        drawAll_()
        var ev = Input.next(100)
        if (ev is KeyEvent) {
          if (_waitDeadlineMs != null) {
            // Post-transfer wait: any key dismisses.  No abort
            // semantics — the transfer is already done.
            quit()
          } else {
            dispatchKey_(ev)
          }
        }
        if (ev is MouseEvent) dispatchMouse_(ev)
        onTick_()
      }
      teardownCursor_()
      Input.mouseEvents = savedMouse
      _aborted = Transfer.aborted
      _success = Transfer.success
    })
  }

  buildUi_() {
    var size = Screen.size
    // Match today's C draw_transfer_window sizing: prefer 66 wide /
    // 18 tall, but cap to screen so 40-column hosts still get a
    // workable layout (twh also caps to tww, matching the C check).
    var tww = 66
    var twh = 18
    if (tww > size[0]) tww = size[0]
    if (twh > size[1]) twh = size[1]
    if (twh > tww)     twh = tww
    var px = ((size[0] - tww) / 2).floor + 1
    var py = ((size[1] - twh) / 2).floor + 1

    _pane         = Pane.new()
    _pane.bounds  = Rect.new(px, py, tww, twh)
    _pane.title   = _label
    _pane.focused = true
    _pane.shadow  = true
    root.add(_pane)

    var ib = _pane.innerBounds

    // 5-row status block at the top (filename / bytes / time / pct /
    // bar) — matches the original window's progress region.
    _statusPanel = TransferStatusPanel.new()
    var sh = ib.h.min(5)
    _statusPanel.bounds = Rect.new(ib.x, ib.y, ib.w, sh)
    _pane.add(_statusPanel)

    // Divider row at the bottom of the status block — only when we
    // have enough vertical room for a log area below.  Bounds span
    // the FULL pane width so the connector glyphs (╟ ╢) overpaint the
    // pane's side frame at that row.
    var dividerY = ib.y + sh
    var logY     = dividerY + 1
    var logH     = (ib.y + ib.h - logY).max(0)
    if (logH > 0) {
      _divider = TransferDivider.new()
      _divider.bounds = Rect.new(_pane.bounds.x, dividerY,
                                 _pane.bounds.w, 1)
      _pane.add(_divider)

      _log = LogView.new()
      _log.bounds = Rect.new(ib.x, logY, ib.w, logH)
      _pane.add(_log)
    } else {
      // Vertical squeeze (≤ 5 rows of inner area): drop the divider
      // and log; the user gets just the progress display.  Avoids
      // crashing the layout on absurdly tall fonts.
      _divider = null
      _log     = null
    }

    bind(Key.escape, Fn.new {|k| latchAbort_() })
    bind(Key.ctrlC,  Fn.new {|k| latchAbort_() })
    bind(Key.ctrlX,  Fn.new {|k| latchAbort_() })
  }

  latchAbort_() {
    if (Transfer.requestAbort() && _log != null) {
      _log.append(LogView.LEVEL_WARNING, "Abort key pressed")
    }
  }

  onTick_() {
    // Drain log channel.  drainLog() returns a list of [level, text]
    // entries plus an optional synthesized "[N suppressed]" tail.
    if (_log != null) {
      for (entry in Transfer.drainLog()) {
        _log.append(entry[0], entry[1])
      }
    } else {
      // No log widget at this size — drain to keep the ring from
      // filling up, but discard.
      Transfer.drainLog()
    }

    // Snapshot tick channel only when dirty — tickDirty atomically
    // reads + clears.  Saves a mutex acquisition per idle frame.
    if (Transfer.tickDirty) {
      _statusPanel.state = TickStateView.new(Transfer.snapshot())
    }

    // Worker → main dialog requests.  Surface as a popup; the foreign
    // call posts the response and unblocks the worker condvar.  Run
    // before the done check so a pending dialog gets serviced even
    // if the worker has finished pushing other events.
    var dlg = Transfer.dialogPending
    if (dlg != 0) handleDialog_(dlg)

    // Done flips → enter post-transfer wait once.  The keypress
    // handler in run() shortcuts the wait on any key.
    if (Transfer.done && !_doneLogged) {
      _doneLogged = true
      if (_waitTimeoutMs <= 0) {
        quit()
        return
      }
      var secs = (_waitTimeoutMs / 1000).floor
      if (_log != null) {
        _log.append(LogView.LEVEL_NOTICE,
                    "Hit any key or wait %(secs) seconds to continue...")
      }
      _waitDeadlineMs = nowMs_ + _waitTimeoutMs
    }

    if (_waitDeadlineMs != null && nowMs_ >= _waitDeadlineMs) {
      quit()
    }
  }

  // Monotonic millisecond clock.  Wraps Timer.now (seconds, Num) into
  // milliseconds for the post-transfer deadline arithmetic.
  nowMs_ { (Timer.now * 1000).floor }

  // Surface a worker dialog as a popup, post the response back, and
  // unblock the parked condvar.  Two kinds today:
  //   1 = XFER_DLG_DUPLICATE — Confirm yes/no → OVERWRITE / SKIP.
  //       (RENAME wired through but no input affordance yet.)
  //   2 = XFER_DLG_FILENAME_PROMPT — LinePrompt → text or cancel.
  // Response codes must match the XFER_DLG_* enums in
  // wren_bind_xfer.h: 0 / 1 / 2 mean different things per kind.
  handleDialog_(kind) {
    if (kind == 1) {
      var fname = Transfer.dialogFilename
      var msg   = "Duplicate file:\n%(fname)\n\nOverwrite?"
      var ok    = Confirm.show(this, msg)
      Transfer.dialogRespond(ok ? 0 : 1, "")
    } else if (kind == 2) {
      // dialogFilename is overloaded as the prompt label here.
      var label = Transfer.dialogFilename
      var name  = LinePrompt.show(this, label)
      if (name == null) {
        Transfer.dialogRespond(1, "")            // cancel
      } else {
        Transfer.dialogRespond(0, name)          // OK with text
      }
    } else {
      Transfer.dialogRespond(1, "")
    }
  }
}
