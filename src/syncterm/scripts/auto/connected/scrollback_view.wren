// scrollback_view.wren - in-terminal scrollback browser, reached via
// Alt-B, the wheel-up hook below, or Conn.scrollback() from any
// script.  Pushes the live cterm rows into the scrollback ring (only
// the cterm region, not the status bar - otherwise the status row
// duplicates as the most-recently-pushed row).  Then runs a tight
// Input.next() loop that blits Scrollback row windows to the screen
// and walks `top` in response to keys, mouse wheel, drag-select.
// popScreen on exit rewinds the ring counters; modalRun handles
// screen save/restore.
//
// No App for the main pan loop -- it's just a blocking-getch loop.
// F1 brings up a Pane help dialog via App.runSync() (sync variant
// since we're already nested inside a hook-driven blocking loop).
//
// Non-cterm areas (status bar, any frame around cterm) get blanked
// to plain yellow-on-blue once on entry; the status bar's update
// timer is paused for the duration because doterm() is parked
// inside our blocking Input.next() loop.

import "syncterm" for Scrollback, Screen, Input, Key, Mouse, KeyEvent, MouseEvent, CTerm, Hook
import "ui_widget" for Rect
import "ui_app"   for App
import "ui_help"  for Help

class ScrollbackView {
  static setupMouse_() {
    var saved = Input.mouseEvents
    Input.mouseEvents = 0
    Input.enableMouseEvent(Mouse.button1DragStart)
    Input.enableMouseEvent(Mouse.button1DragMove)
    Input.enableMouseEvent(Mouse.button1DragEnd)
    Input.enableMouseEvent(Mouse.wheelUpPress)
    Input.enableMouseEvent(Mouse.wheelDownPress)
    return saved
  }

  static run() {
    Fiber.new {
      // Replace the caller's mask for the duration of the viewer.
      // Retaining a BBS tracking mask would deliver raw motion here,
      // repainting the indicators for every movement, while a mask
      // without drag-move/end would strand the C selection loop.
      var savedMouse = setupMouse_()
      Screen.modalRun(Fn.new {
        Scrollback.pushScreen()
        blankSurroundings_()
        viewLoop_()
        // popScreen rewinds via the saved snapshot; the n argument is
        // informational so the value here doesn't matter.
        Scrollback.popScreen(0)
      })
      Input.mouseEvents = savedMouse
    }.call()
  }

  // Paint the entire screen with yellow-on-blue spaces, so the area
  // outside the cterm region (any centered/left-justified frame plus
  // the status bar row) shows a solid blue background.  The cterm
  // area gets overpainted with scrollback content each loop iter.
  static blankSurroundings_() {
    Screen.attr = 0x1e   // yellow on blue, no blink (blank cells)
    var sz = Screen.size
    Screen.window.bounds = [1, 1, sz[0], sz[1]]
    Screen.window.clear()
  }

  // Blocking pan loop.  `top` is the logical scrollback row that
  // displays at screen row 1 of the cterm area.  Range is
  // [0, totalRows - cterm.height]; capped each iteration.  Initial
  // top = visibleRowsFromTheBottom so the first paint matches what
  // the user was just looking at.
  static viewLoop_() {
    var w       = Scrollback.width
    var visH    = CTerm.height
    var totalH  = Scrollback.height
    var maxTop  = totalH - visH
    if (maxTop < 0) maxTop = 0
    var top = maxTop

    while (true) {
      paint_(top, visH, w)
      var ev = Input.next()
      if (ev is KeyEvent) {
        var c = ev.code
        if (c == Key.escape) return
        if (c == Key.up) {
          top = (top > 0) ? top - 1 : 0
        } else if (c == Key.down) {
          if (top < maxTop) {
            top = top + 1
          } else {
            return
          }
        } else if (c == Key.pageUp) {
          top = top - visH
          if (top < 0) top = 0
        } else if (c == Key.pageDown) {
          top = top + visH
          if (top > maxTop) top = maxTop
        } else if (c == Key.home) {
          top = 0
        } else if (c == Key.end) {
          top = maxTop
        } else if (c == Key.f1) {
          showHelp_()
        } else if (c == 0x4A || c == 0x6A) {     // J / j
          if (top < maxTop) {
            top = top + 1
          } else {
            return
          }
        } else if (c == 0x4B || c == 0x6B) {     // K / k
          top = (top > 0) ? top - 1 : 0
        } else if (c == 0x48 || c == 0x68) {     // H / h
          top = top - visH
          if (top < 0) top = 0
        } else if (c == 0x4C || c == 0x6C) {     // L / l
          top = top + visH
          if (top > maxTop) top = maxTop
        } else if (c == 0x71 || c == 0x51) {     // q / Q
          return
        }
      } else if (ev is MouseEvent) {
        var mev = ev.event
        if (mev == Mouse.wheelUpPress) {
          top = (top > 0) ? top - 1 : 0
        } else if (mev == Mouse.wheelDownPress) {
          if (top < maxTop) {
            top = top + 1
          } else {
            return
          }
        } else if (mev == Mouse.button1DragStart) {
          // Hand off to the C rect-select copier.  It reads the
          // currently-painted screen, lets the user drag, copies the
          // selection to the clipboard, then returns.  The next
          // iteration repaints over whatever it left behind.
          Input.mousedrag()
        }
      }
    }
  }

  // Blit `visH` rows from Scrollback starting at logical row `top`
  // onto the screen at (1, 1).  Then stamp "Scrollback" labels in
  // the top-left and top-right of the cterm area as visual
  // indicators that we're not in live terminal mode.  Window stays
  // at the full-screen bounds blankSurroundings_ set; if we shrunk
  // it to a 1-row strip the second print would scroll the strip
  // (cursor advances past right edge -> wrap -> 1-row scroll
  // clears the row) and the labels would vanish.
  static paint_(top, visH, w) {
    Screen.putRect(Scrollback, Rect.new(0, top, w, visH), 1, 1)
    Screen.attr = 0x9e   // blink + yellow on blue
    Screen.window.position = [1, 1]
    Screen.window.print("Scrollback")
    Screen.window.position = [w - 9, 1]
    Screen.window.print("Scrollback")
  }

  // F1 help.  Help is a Popup subclass that lays out and renders a
  // markdown body (pandoc-style def-list, matching the in-tree
  // convention) with optional scrollbar.  App.runSync() (not run())
  // because the parent viewLoop_ is already blocking the VM on
  // Input.next() -- the async run() would try to drain doterm
  // events but doterm itself is parked.
  static showHelp_() {
    Screen.modalRun(Fn.new {
      var app = App.new()
      var h   = Help.new("Scrollback Help",
        "Up / k\n" +
        ":  Scrolls up one line\n" +
        "Down / j\n" +
        ":  Scrolls down one line\n" +
        "PgUp / h\n" +
        ":  Scrolls up one screen\n" +
        "PgDn / l\n" +
        ":  Scrolls down one screen\n" +
        "Home\n" +
        ":  Jumps to the oldest row in the buffer\n" +
        "End\n" +
        ":  Jumps to the newest row\n" +
        "Mouse wheel\n" +
        ":  Scrolls up or down one line\n" +
        "Click + drag\n" +
        ":  Selects a region and copies it to the clipboard\n" +
        "Esc / q\n" +
        ":  Returns to the terminal\n")
      h.bounds     = Help.centeredBounds_()
      h.onDismiss  = Fn.new { |v| app.quit() }
      app.pushModal(h)
      app.runSync()
    })
  }
}

// Wheel-up at the cterm: enter the scrollback viewer.  Gated to mouse
// modes off / X10 / RIP -- the various XTerm tracking modes leave the
// wheel-press alone (returning false from a Hook lets the event fall
// through to the C dispatch, which forwards it to the remote).
Hook.onMouse(Mouse.wheelUpPress) { |me|
  var mm = CTerm.mouseMode
  if (mm == 0 || mm == 1 || mm == 9) {     // MM_OFF / MM_RIP / MM_X10
    ScrollbackView.run()
    return true
  }
  return false
}
