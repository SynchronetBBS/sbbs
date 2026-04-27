// SPDX-License-Identifier: GPL-2.0-or-later
//
// Self-test suite for SyncTERM's Wren bindings.  Triggered by Alt+T
// (registered in scripts/runtests.wren).
//
// Assumes the remote is bash in a PTY: each async test sends a
// shell command with a unique sentinel and matches the response via
// Hook.onMatch.  PTY echo means each `printf` triggers two sentinel
// matches (the kernel/shell echo of the command line + the actual
// printf output) — handle_() guards with a __pending check so only
// the first match advances state; later duplicates are dropped.
//
// The dispatcher and watchdog are registered ONCE when this module
// is first imported.  Re-running via Alt+T just resets state, so
// repeated runs don't leak hooks.

import "syncterm" for Hook, Conn, Console, Screen, CTerm

class WrenTest {
  static run() {
    __step    = 0
    __pass    = 0
    __fail    = 0
    __pending = null
    System.print("=== Wren self-test starting ===")
    // Inline (synchronous) tests first — they don't depend on
    // remote responses, so they always run regardless of network.
    testConnConnected_()
    testConnType_()
    testScreenSize_()
    testCTermCursor_()
    testConsoleTotal_()
    testAnchorReject_()
    // Then the sentinel-driven async tests.
    advance_()
  }

  static check_(ok, label) {
    if (ok) {
      __pass = __pass + 1
      System.print("  PASS %(label)")
    } else {
      __fail = __fail + 1
      System.print("  FAIL %(label)")
    }
  }

  // ----- inline tests ----------------------------------------------

  static testConnConnected_() {
    check_(Conn.connected, "Conn.connected is true")
  }

  static testConnType_() {
    var t = Conn.type
    check_(t is Num && t >= 0, "Conn.type is a number")
  }

  static testScreenSize_() {
    var sz = Screen.size
    check_(sz is List && sz.count == 2 &&
           sz[0] is Num && sz[0] > 0 &&
           sz[1] is Num && sz[1] > 0,
           "Screen.size returns [w, h]")
  }

  static testCTermCursor_() {
    var x = CTerm.x
    var y = CTerm.y
    check_(x is Num && x >= 1 && y is Num && y >= 1,
           "CTerm.x/y are 1-based positives")
  }

  static testConsoleTotal_() {
    var before = Console.total
    System.print("[probe]")
    var after = Console.total
    check_(after > before, "Console.total grows on print")
  }

  static testAnchorReject_() {
    // Regex starting with .* must Fiber.abort at registration.
    var f = Fiber.new { Hook.onMatch(".*x") { |m| } }
    var err = f.try()
    check_(err != null, "Hook.onMatch rejects leading .*")
  }

  // ----- sentinel-driven async tests -------------------------------

  static advance_() {
    __step = __step + 1
    if (__step == 1) {
      __pending = "T01"
      Conn.send("printf '__T01_X_HELLO_X__\\n'\r")
    } else if (__step == 2) {
      __pending = "T02"
      Conn.send("printf '__T02_X_joe_X__\\n'\r")
    } else {
      report_()
    }
  }

  // name is the test number from the dispatcher's first capture
  // ("01", "02", …); value is the second capture (the payload).
  static handle_(name, value) {
    var expected = "T" + name
    if (__pending != expected) return     // duplicate (PTY echo) — drop
    __pending = null
    if (name == "01") {
      check_(value == "HELLO", "T01 echo + Hook.onMatch literal")
    } else if (name == "02") {
      check_(value == "joe", "T02 capture group")
    } else {
      check_(false, "T%(name) unexpected sentinel")
    }
    advance_()
  }

  static timeout_() {
    if (__pending == null) return
    __fail = __fail + 1
    System.print("  FAIL %(__pending) (timeout)")
    __pending = null
    advance_()
  }

  static report_() {
    var total = __pass + __fail
    System.print("=== %(total) tests, %(__pass) pass, %(__fail) fail ===")
  }
}

// One-time dispatcher registration.  Sentinel format:
//   __T<NN>_X_<value>_X__
// The leading literal `__T` makes the pattern legal under
// Hook.onMatch's leading-anchor rule.  `_X_` and `_X__` mark the
// capture group boundaries so a value containing underscores
// doesn't confuse the parser.
Hook.onMatch("__T(.+)_X_(.+)_X__") { |m|
  WrenTest.handle_(m[1], m[2])
  return true   // consume — keep the sentinel out of the screen
}

// Watchdog — fires every 3s; ticks the timeout path only if a test
// is currently pending a sentinel response.
Hook.every(3000) { WrenTest.timeout_() }
