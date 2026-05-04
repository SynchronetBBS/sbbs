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
import "disconnect_flow" for DisconnectFlow
import "capture_menu"    for CaptureMenu
import "music_menu"      for MusicMenu

// Disconnect / exit cluster.  DisconnectFlow lives in its own module
// (disconnect_flow.wren) so the online menu can reuse it.  Each hook
// here just routes to the right exitApp argument:
//   exitApp = true   → Alt-X / window-close: exit SyncTERM after hangup
//   exitApp = false  → Alt-H / Ctrl-Q: return to bbslist after hangup
//
// Ctrl-Q is gated to text-mode terminals (curses / ANSI) — graphical
// backends (X / SDL / Wayland / Quartz / GDI) leave Ctrl-Q to cterm
// as a normal control byte.  Host.textTerminal is stable for the
// session, so we check at module load and skip installing the hook
// in graphical modes rather than process-and-pass-through every key.
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

// Alt-M — ANSI music mode picker.  Delegates to MusicMenu.run() so
// the Alt-Z online menu's "ANSI Music Control" entry can reach the
// same modal flow.
Hook.onKey(Key.altM) { |k|
  MusicMenu.run()
  return true
}
