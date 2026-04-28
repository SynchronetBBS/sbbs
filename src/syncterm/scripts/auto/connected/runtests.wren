// Hotkey to run the Wren-embedding self-test suite.  Press Alt+T
// (0x1400) against a bash-in-PTY remote and read the results in the
// Wren console (Ctrl+`).  See Wren.adoc § "Streaming regex hooks"
// for the suite's design and assumptions.
//
// Wren's `import` is a top-level statement, so the suite is loaded
// once at connect time (which also runs its top-level
// dispatcher/watchdog registration).  Subsequent hotkey presses
// just call WrenTest.run() to reset state.

import "syncterm" for Hook
import "wrentest" for WrenTest

Hook.onKey(0x1400) { |k|     // Alt+T
  WrenTest.run()
  return true
}
