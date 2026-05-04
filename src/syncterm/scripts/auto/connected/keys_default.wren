// keys_default.wren — default Wren bindings for the few in-terminal
// keystrokes that used to live as hard-coded `case` blocks in
// term.c's input loop.  Each handler returns `true` to consume the
// key; subsequent hooks and the C dispatch never see it.
//
// Override: drop a same-named file into your auto-load dir.  Replace
// individual handlers with your own `Hook.onKey(...)` registrations
// from any later script — the new handler runs first and consumes
// the key, leaving the default a no-op.

import "syncterm" for Hook, Key, CTerm, Host, Input, Screen
import "ui_app"   for App
import "ui_pane"  for Pane
import "ui_list"  for ListView

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
