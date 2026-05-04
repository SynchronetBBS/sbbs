// keys_default.wren — default Wren bindings for the few in-terminal
// keystrokes that used to live as hard-coded `case` blocks in
// term.c's input loop.  Each handler returns `true` to consume the
// key; subsequent hooks and the C dispatch never see it.
//
// Override: drop a same-named file into your auto-load dir.  Replace
// individual handlers with your own `Hook.onKey(...)` registrations
// from any later script — the new handler runs first and consumes
// the key, leaving the default a no-op.

import "syncterm" for Hook, Key, CTerm, Conn, Host, Input, Screen
import "ui_app"   for App
import "ui_pane"  for Pane
import "ui_list"  for ListView
import "ui_popup" for Confirm, Popup
import "capture_menu" for CaptureMenu

// Disconnect / exit cluster.  Each hook spawns a child fiber that
// raises a "Disconnect... Are you sure?" Confirm popup; on yes, it
// calls Conn.endSession to flag the actual cleanup.  doterm() picks
// the flag up at the top of its next iteration, runs the
// (UI-free) C cleanup, and either returns to the bbslist
// (Alt-H / Ctrl-Q semantics) or exits syncterm entirely
// (Alt-X / window-close semantics).
//
// Ctrl-Q is gated to text-mode terminals (curses / ANSI) — graphical
// backends (X / SDL / Wayland / Quartz / GDI) leave Ctrl-Q to cterm
// as a normal control byte.  Host.textTerminal is stable for the
// session, so we check at module load and skip installing the hook
// in graphical modes rather than process-and-pass-through every key.
class DisconnectFlow {
  static run(exitApp) {
    Fiber.new {
      Screen.modalRun(Fn.new {
        var app = App.new()
        var msg = "Disconnect... Are you sure?\n\n" +
                  "Selecting Yes closes the connection."
        var c   = Confirm.new(msg)
        c.bounds     = Popup.centeredBounds_(msg, 1, 24)
        // The Confirm dismissal pops itself off the modal stack;
        // also quit the App so `app.run()` returns.  Without this
        // wire-up run() would loop forever on an empty modal stack.
        c.onDismiss  = Fn.new { |v| app.quit() }
        app.pushModal(c)
        app.run()
        if (c.result == true) Conn.endSession(exitApp)
      })
      Input.setupMouseEvents()
    }.call()
  }
}

Hook.onKey(Key.altX) { |k|
  DisconnectFlow.run(true)
  return true
}
Hook.onKey(Key.quit) { |k|
  DisconnectFlow.run(true)
  return true
}
Hook.onKey(Key.altH) { |k|
  DisconnectFlow.run(false)
  return true
}
if (Host.textTerminal) {
  Hook.onKey(Key.ctrlQ) { |k|
    DisconnectFlow.run(false)
    return true
  }
}

// Alt-O — toggle mouse-event reporting.  Mirrors the historical C
// handler: flip the MS_FLAGS_DISABLED bit on the live mouse_state,
// then reconfigure ciolib so it stops (or starts) emitting motion /
// click events for the active mode.
Hook.onKey(Key.altO) { |k|
  CTerm.mouseDisabled = !CTerm.mouseDisabled
  Input.setupMouseEvents()
  return true
}

// Alt-Up / Alt-Down — walk the network character-pacing rate up and
// down the BBS-list rates ladder.  No-op on serial connections (the
// underlying C action checks conn_type, mirroring the old handler).
// Returns true unconditionally so the key never falls through to the
// terminal even on a serial session.
Hook.onKey(Key.altUp) { |k|
  CTerm.throttleSpeedUp()
  return true
}
Hook.onKey(Key.altDown) { |k|
  CTerm.throttleSpeedDown()
  return true
}

// Shift-Insert — paste from system clipboard onto the wire.  The
// foreign primitive does the codepage / bracketed-paste handling
// the C path used to do directly.
Hook.onKey(Key.shiftIns) { |k|
  Conn.paste()
  return true
}

// Alt-B — open the scrollback viewer modal.  The foreign primitive
// handles mouse-event disable/restore around the uifc viewer.
Hook.onKey(Key.altB) { |k|
  Conn.scrollback()
  return true
}

// Alt-C — open the capture menu (Wren-driven; replaces the
// historical uifc capture_control dialog).  CaptureMenu spawns a
// child fiber that drives the modal flow.
Hook.onKey(Key.altC) { |k|
  CaptureMenu.run()
  return true
}

// Alt-M — ANSI music mode picker.  Replaces the C music_control()
// dialog.  The hook body itself returns immediately; the modal UI
// runs inside a child fiber so its event loop can yield freely (per
// Hook.dispatch_'s "no direct yield" rule -- see wren.md §8 and the
// header comment on App).  Refresh mouse events on the way out so
// the in-dialog reconfiguration doesn't leak.
Hook.onKey(Key.altM) { |k|
  Fiber.new {
    Screen.modalRun(Fn.new {
      var app  = App.new()
      var pane = Pane.new()
      pane.title    = "ANSI Music Setup"
      pane.helpText = Host.musicHelp
      pane.focused  = true
      pane.onClose  = Fn.new { app.quit() }
      app.root.add(pane)

      var list = ListView.new()
      list.items    = Host.musicNames
      list.selected = CTerm.music
      list.onSelect = Fn.new { |i, item|
        CTerm.music = i
        app.quit()
      }
      pane.add(list)
      pane.fitContent()
      pane.centerOnScreen()

      app.bind(Key.escape, Fn.new { |k| app.quit() })
      app.run()
    })
    Input.setupMouseEvents()
  }.call()
  return true
}
