// keys_default.wren — default Wren bindings for the few in-terminal
// keystrokes that used to live as hard-coded `case` blocks in
// term.c's input loop.  Each handler returns `true` to consume the
// key; subsequent hooks and the C dispatch never see it.
//
// Override: drop a same-named file into your auto-load dir.  Replace
// individual handlers with your own `Hook.onKey(...)` registrations
// from any later script — the new handler runs first and consumes
// the key, leaving the default a no-op.

import "syncterm" for Hook, Key, CTerm, Input

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
