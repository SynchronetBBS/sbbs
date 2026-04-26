// SPDX-License-Identifier: GPL-2.0-or-later
//
// SyncTERM Wren console (REPL).  Triggered by Ctrl+` (CIO_KEY_WREN_CONSOLE
// = 0x29E0).  Saves the screen, presents the always-on print/error
// scrollback, accepts immediate-mode Wren input via Repl.eval, and
// restores on exit.  While the console is open the doterm() main loop is
// blocked inside this script — connection bytes accumulate in the inbuf
// but aren't drained until exit.  Acceptable for a development tool.
//
// This script lives in module "syncterm" alongside the foreign-class
// declarations and other embedded scripts, so every binding (Conio,
// Cell, Repl, Hook, …) is directly visible without an import.  Lines
// submitted at the prompt are evaluated in a chosen module — default
// "syncterm" — via Repl.eval, so `var x = ...` and `class Foo {...}`
// declarations persist across submissions inside that module.
//
// `/in <module>` switches the eval target, letting the user inspect
// or mutate any other module that's been loaded.

class WrenConsole {
  static run() {
    var saved = Conio.savescreen
    var currentModule = "syncterm"

    // One-time paint of whatever's currently in the log buffer.
    // Subsequent loop iterations only emit entries that arrived
    // *after* `lastTotal`; conio's built-in scroll-on-bottom-row
    // behavior moves history up naturally as new entries arrive.
    Conio.clrscr()
    Conio.gotoxy(1, 1)
    var lastTotal = paintNew_(0, true)

    var input = ""
    var promptRow = Conio.wherey
    drawPrompt_(promptRow, currentModule, input)

    var done = false
    while (!done) {
      var key = Conio.getch

      // Handle the keystroke.
      var redrawPrompt = false
      if (key == 0x29E0 || key == 0x011B) {        // Ctrl+` or Esc
        done = true
      } else if (key == 0x000D || key == 0x000A) { // Enter
        currentModule = handleLine_(input, currentModule)
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
      } else if (key == 0x7DE0) {                  // CIO_KEY_MOUSE
        var ev = Conio.getMouse
        // 7  = CIOLIB_BUTTON_1_DRAG_START — hand off to the existing
        //      select-and-copy loop; it consumes the rest of the drag
        //      itself, then restores the screen on exit.
        // 12 = CIOLIB_BUTTON_2_CLICK     — paste clipboard into the
        //      input line.  LF submits the line in progress; CR is
        //      dropped (paired with LF or alone); other control bytes
        //      are silently skipped.
        if (ev != null && ev[0] == 7) {
          Conio.mousedrag()
          redrawPrompt = true
        } else if (ev != null && ev[0] == 12) {
          var pasted = Conio.getClipText
          if (pasted != null) {
            for (b in pasted.bytes) {
              if (b == 0x0A) {
                currentModule = handleLine_(input, currentModule)
                input = ""
              } else if (b == 0x0D) {
                // CR ignored
              } else if (b >= 0x20 && b < 0x7F) {
                input = input + String.fromCodePoint(b)
              }
            }
            redrawPrompt = true
          }
        }
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

      if (redrawPrompt) drawPrompt_(promptRow, currentModule, input)
    }

    Conio.restorescreen(saved)
  }

  // Dispatches a finished line: a leading "/" makes it a console
  // command; anything else is Wren source for Repl.eval.  Returns the
  // (possibly updated) current module.
  //
  // We wrap the eval in Fiber.new{}.try() so a runtime abort in the
  // user's snippet doesn't tear the console out from under the Hook
  // dispatcher.  Wren's runtime only emits stack frames for *uncaught*
  // aborts, but the failed fiber still has its frames intact — we
  // hand it to Repl.printTrace_ which walks them and pushes them
  // through the host's error callback into the log buffer.
  //
  // .try() can't be used to detect the abort because it returns the
  // block's last-expression value on normal completion (here the
  // result of the `v = ...` assignment, NOT null).  Use fiber.error
  // instead — it stays null unless the fiber actually aborted.
  static handleLine_(line, current) {
    if (line.startsWith("/")) return runCommand_(line, current)
    System.print("(%(current))> " + line)
    var v = null
    var f = Fiber.new { v = Repl.eval(current, line) }
    f.try()
    // Repl.eval returns null for a statement, [value] for an
    // expression — so v != null means "we got a value, even if v[0]
    // happens to be null itself".
    if (f.error != null) {
      System.print("error: " + f.error)
      Repl.printTrace_(f)
    } else if (v != null) {
      System.print(formatValue_(v[0]))
    }
    return current
  }

  // Render a Wren value for REPL output.  Strings get C-style quoting
  // so control bytes don't dump literal newlines/escapes into the
  // scrollback and so a String result is visibly distinct from a Num
  // (e.g. "7" the string vs 7 the number).  Everything else goes
  // through Wren's default Object.toString.
  static formatValue_(v) {
    if (v == null) return "null"
    if (v is String) return quoteString_(v)
    return "%(v)"
  }

  static quoteString_(s) {
    var hex = "0123456789abcdef"
    var out = "\""
    for (b in s.bytes) {
      if (b == 0x22) {                       // "
        out = out + "\\\""
      } else if (b == 0x5C) {                // \
        out = out + "\\\\"
      } else if (b == 0x0A) {
        out = out + "\\n"
      } else if (b == 0x0D) {
        out = out + "\\r"
      } else if (b == 0x09) {
        out = out + "\\t"
      } else if (b == 0x00) {
        out = out + "\\0"
      } else if (b < 0x20 || b == 0x7F) {
        out = out + "\\x" + hex[(b >> 4) & 0xf] + hex[b & 0xf]
      } else {
        out = out + String.fromCodePoint(b)
      }
    }
    return out + "\""
  }

  // /in <module>  — switch the current eval target.
  // /in           — show the current target.
  // /?            — list commands.
  static runCommand_(line, current) {
    if (line == "/?" || line == "/help") {
      System.print("commands: /in [module], /?")
      return current
    }
    if (line == "/in") {
      System.print("current module: " + current)
      return current
    }
    if (line.startsWith("/in ")) {
      var name = line[4...line.count].trim()
      if (name == "") {
        System.print("usage: /in <module>")
        return current
      }
      if (!Repl.hasModule(name)) {
        System.print("warning: module '%(name)' not loaded; an empty one will be created")
      }
      System.print("now in module: " + name)
      return name
    }
    System.print("unknown command: " + line)
    return current
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

  static drawPrompt_(row, mod, input) {
    Conio.gotoxy(1, row)
    Conio.clreol()
    Conio.cputs("(%(mod)) wren> " + input)
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
