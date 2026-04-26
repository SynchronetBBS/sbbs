// SPDX-License-Identifier: GPL-2.0-or-later
//
// SyncTERM Wren console (REPL).  Triggered by Ctrl+` (CIO_KEY_WREN_CONSOLE
// = 0x29E0).  Saves the screen, presents the always-on print/error
// scrollback, accepts immediate-mode Wren input via Meta.compile, and
// restores on exit.  While the console is open the doterm() main loop is
// blocked inside this script — connection bytes accumulate in the inbuf
// but aren't drained until exit.  Acceptable for a development tool.
//
// Display rules (per agreed design):
//   - Console.LogSource.print entries: text printed verbatim, no inserted
//     newlines.  Track whether the cursor is at column 1 between writes.
//   - Other sources: if the previous text didn't end at column 1, emit a
//     newline; emit "[<source> <ts>] "; emit text; emit a trailing newline
//     if the text doesn't already end with one.

import "syncterm" for Hook, Conio, Console, LogSource
import "meta" for Meta

class WrenConsole {
  static run() {
    var saved = Conio.savescreen

    // One-time paint of whatever's currently in the log buffer.
    // Subsequent loop iterations only emit entries that arrived
    // *after* `lastTotal`; conio's built-in scroll-on-bottom-row
    // behavior moves history up naturally as new entries arrive.
    Conio.clrscr()
    Conio.gotoxy(1, 1)
    var atLineStart = true
    var lastTotal = paintNew_(0, true)

    var input = ""
    var promptRow = Conio.wherey
    drawPrompt_(promptRow, input)

    var done = false
    while (!done) {
      var key = Conio.getch

      // Handle the keystroke.
      var redrawPrompt = false
      if (key == 0x29E0 || key == 0x011B) {        // Ctrl+` or Esc
        done = true
      } else if (key == 0x000D || key == 0x000A) { // Enter
        System.print("> " + input)
        var fn = Meta.compile(input)
        if (fn != null) {
          var err = Fiber.new { fn.call() }.try()
          if (err != null) System.print("error: %(err)")
        }
        input = ""
        redrawPrompt = true
      } else if (key == 0x0008 || key == 0x007F) { // Backspace
        if (input.count > 0) {
          input = input[0...(input.count - 1)]
          redrawPrompt = true
        }
      } else if (key == 0x000C) {                  // Ctrl+L
        Console.clear()
        Conio.clrscr()
        Conio.gotoxy(1, 1)
        lastTotal = Console.total
        promptRow = Conio.wherey
        redrawPrompt = true
      } else if (key >= 0x20 && key < 0x7F) {
        input = input + String.fromCodePoint(key)
        redrawPrompt = true
      }
      if (done) break

      // Drain any new log entries that landed during the keystroke
      // (e.g. System.print echo + eval output from the Enter path).
      // The prompt row gets overwritten as we emit; conio scrolls
      // past the bottom for free.
      if (Console.total > lastTotal) {
        Conio.gotoxy(1, promptRow)
        Conio.clreol()
        lastTotal = paintNew_(lastTotal, false)
        promptRow = Conio.wherey
        redrawPrompt = true
      }

      if (redrawPrompt) drawPrompt_(promptRow, input)
    }

    Conio.restorescreen(saved)
  }

  // Emit log entries with seq >= `since` (clamped up to whatever's still
  // present in the ring after eviction).  `firstPaint` is true on the
  // initial paint; controls whether atLineStart starts assumed-true.
  // Returns the new `lastTotal` watermark.
  static paintNew_(since, firstPaint) {
    var totalNow = Console.total
    var oldest   = totalNow - Console.count
    var start    = since.max(oldest)
    var atStart  = firstPaint
    for (s in start...totalNow) {
      var e = Console[s]
      if (e != null) atStart = emitEntry_(e, atStart)
    }
    if (!atStart) put_("\n")
    return totalNow
  }

  static drawPrompt_(row, input) {
    Conio.gotoxy(1, row)
    Conio.clreol()
    Conio.cputs("wren> " + input)
  }

  // conio's cputs treats LF as line-feed-only (cursor down, same column).
  // The console expects \n to start a new line at column 1, so translate
  // every \n in `s` to \r\n on the way out.
  static put_(s) {
    var start = 0
    var i = 0
    while (i < s.count) {
      if (s[i] == "\n") {
        if (i > start) Conio.cputs(s[start...i])
        Conio.cputs("\r\n")
        start = i + 1
      }
      i = i + 1
    }
    if (start < s.count) Conio.cputs(s[start...s.count])
  }

  // Emit one log entry, returning whether the cursor is now at column 1.
  static emitEntry_(e, atLineStart) {
    var ts     = e[0]
    var src    = e[1]
    var text   = e[2]
    var newAtStart = atLineStart

    if (src == LogSource.print) {
      put_(text)
      if (text.count > 0) {
        var last = text[text.count - 1]
        newAtStart = (last == "\n")
      }
    } else {
      if (!atLineStart) put_("\n")
      var label = "?"
      if (src == LogSource.compileError) label = "compile"
      if (src == LogSource.runtimeError) label = "runtime"
      if (src == LogSource.stackFrame)   label = "trace"
      put_("[%(label) %(ts)] ")
      put_(text)
      newAtStart = true
      if (text.count > 0 && text[text.count - 1] != "\n") put_("\n")
    }
    return newAtStart
  }

}

Hook.onKey { |k|
  if (k == 0x29E0) {
    WrenConsole.run()
    return true
  }
  return false
}
