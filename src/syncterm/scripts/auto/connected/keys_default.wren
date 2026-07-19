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
import "disconnect_flow"  for DisconnectFlow
import "capture_menu"     for CaptureMenu
import "music_menu"       for MusicMenu
import "scrollback_view"  for ScrollbackView
import "transfer_pick"    for UploadApp, DownloadApp
import "font_pick"        for FontApp

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

// Alt-B - open the Wren scrollback viewer modal.  ScrollbackView
// pushes the live screen into the ring, runs a tight Input.next()
// pan loop, then pops the ring on exit.
Hook.onKey(Key.altB) { |k|
  ScrollbackView.run()
  return true
}

// Alt-C — open the capture menu. CaptureMenu spawns a child fiber
// that drives the modal flow.
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

// Alt-D / Alt-U — download / upload protocol pickers. Auto-Z
// (in-stream ZRQINIT detection from doterm()) bypasses the picker
// and reaches UploadApp directly via wren_run_upload_app() in C.
Hook.onKey(Key.altD) { |k|
  DownloadApp.run()
  return true
}
Hook.onKey(Key.altU) { |k|
  UploadApp.run(false, 0)
  return true
}

// Alt-F — font picker. FontApp pops a ListView of conio_fontdata names; Insert
// keys into the load-from-file flow, gated by ScreenSupports.loadableFonts.
Hook.onKey(Key.altF) { |k|
  FontApp.run()
  return true
}
