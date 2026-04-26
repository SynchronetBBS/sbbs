// SPDX-License-Identifier: GPL-2.0-or-later
//
// SyncTERM Wren console (REPL).  Triggered by Ctrl+` (CIO_KEY_WREN_CONSOLE
// = 0x29E0).  Saves the screen, presents the always-on print/error
// scrollback, accepts immediate-mode Wren input via REPL.eval, and
// restores on exit.  While the console is open the doterm() main loop is
// blocked inside this script — connection bytes accumulate in the inbuf
// but aren't drained until exit.  Acceptable for a development tool.
//
// This script lives in module "syncterm" alongside the foreign-class
// declarations and other embedded scripts, so every binding (Screen,
// Cell, REPL, Hook, …) is directly visible without an import.  Lines
// submitted at the prompt are evaluated in a chosen module — default
// "syncterm" — via REPL.eval, so `var x = ...` and `class Foo {...}`
// declarations persist across submissions inside that module.
//
// `/in <module>` switches the eval target, letting the user inspect
// or mutate any other module that's been loaded.

class WrenConsole {
  // __history (static field) keeps submitted lines across run()
  // invocations so reopening the console doesn't lose context.
  // Up/Down arrows walk it; if there's anything typed at the time the
  // user hits Up, that becomes a *prefix anchor* — only entries that
  // start with it are visited.  Editing the line (printable, backspace,
  // Ctrl+W) cancels the search and the typed text becomes the new
  // free-form input.

  // Largest i < beforeIdx where __history[i].startsWith(prefix), or -1.
  static historyFindBackward_(prefix, beforeIdx) {
    var i = beforeIdx - 1
    while (i >= 0) {
      if (__history[i].startsWith(prefix)) return i
      i = i - 1
    }
    return -1
  }

  // Smallest i > afterIdx where __history[i].startsWith(prefix), or -1.
  static historyFindForward_(prefix, afterIdx) {
    var i = afterIdx + 1
    while (i < __history.count) {
      if (__history[i].startsWith(prefix)) return i
      i = i + 1
    }
    return -1
  }

  // Backward kill-word (Ctrl+W).  Strip trailing whitespace, then strip
  // the run of non-whitespace before it.  Matches readline's
  // unix-word-rubout: any non-space counts as part of the word.
  static killWord_(s) {
    var i = s.count
    while (i > 0 && s[i - 1] == " ") i = i - 1
    while (i > 0 && s[i - 1] != " ") i = i - 1
    return s[0...i]
  }

  // Append `line` to history if non-empty and not a dup of the last
  // entry, then dispatch it like Enter would.  Returns the (possibly
  // changed) current module.
  static submitLine_(line, current) {
    if (line.count > 0 &&
        (__history.count == 0 || __history[__history.count - 1] != line)) {
      __history.add(line)
    }
    return handleLine_(line, current)
  }

  static run() {
    if (__history == null) __history = []
    var saved = Screen.save()
    var currentModule = "syncterm"

    // One-time paint of whatever's currently in the log buffer.
    // Subsequent loop iterations only emit entries that arrived
    // *after* `lastTotal`; conio's built-in scroll-on-bottom-row
    // behavior moves history up naturally as new entries arrive.
    Screen.window.clear()
    Screen.window.position = [1, 1]
    var lastTotal = paintNew_(0, true)

    var input = ""
    var promptRow = Screen.window.position[1]
    drawPrompt_(promptRow, currentModule, input)

    __quit = false
    // History search state: histIdx == -1 means "not searching"; the
    // buffer is whatever the user has typed.  Otherwise histIdx is the
    // current index into __history and histAnchor is the prefix the
    // user had typed at the moment they first pressed Up.
    var histIdx    = -1
    var histAnchor = ""
    var done = false
    while (!done && !__quit) {
      var ev = Input.next

      // Handle the event.
      var redrawPrompt = false
      if (ev is MouseEvent) {
        // 7  = CIOLIB_BUTTON_1_DRAG_START — hand off to the existing
        //      select-and-copy loop; it consumes the rest of the drag
        //      itself, then restores the screen on exit.
        // 12 = CIOLIB_BUTTON_2_CLICK     — paste clipboard into the
        //      input line.  LF submits the line in progress; CR is
        //      dropped (paired with LF or alone); other control bytes
        //      are silently skipped.
        if (ev.event == 7) {
          Input.mousedrag()
          redrawPrompt = true
        } else if (ev.event == 12) {
          var pasted = Clipboard.text
          if (pasted != null) {
            for (b in pasted.bytes) {
              if (b == 0x0A) {
                currentModule = submitLine_(input, currentModule)
                input = ""
                histIdx = -1
              } else if (b == 0x0D) {
                // CR ignored
              } else if (b >= 0x20 && b < 0x7F) {
                input = input + String.fromCodePoint(b)
                histIdx = -1
              }
            }
            redrawPrompt = true
          }
        }
      } else {
        var key = ev.code
        if (key == Key.wrenConsole || key == Key.escape) {
          done = true
        } else if (key == Key.enter || key == 0x000A) { // CR or LF
          currentModule = submitLine_(input, currentModule)
          input = ""
          histIdx = -1
          redrawPrompt = true
        } else if (key == Key.backspace || key == 0x007F) { // BS or DEL
          if (input.count > 0) {
            input = input[0...(input.count - 1)]
            histIdx = -1
            redrawPrompt = true
          }
        } else if (key == 0x0017) {                  // Ctrl+W
          input = killWord_(input)
          histIdx = -1
          redrawPrompt = true
        } else if (key == Key.up) {
          // First Up in a search anchors the prefix to whatever the
          // user had typed; subsequent Ups walk further back through
          // entries that share that prefix.
          var startIdx = histIdx
          if (histIdx == -1) {
            histAnchor = input
            startIdx = __history.count
          }
          var found = historyFindBackward_(histAnchor, startIdx)
          if (found >= 0) {
            histIdx = found
            input = __history[found]
            redrawPrompt = true
          }
        } else if (key == Key.down) {
          if (histIdx != -1) {
            var found = historyFindForward_(histAnchor, histIdx)
            if (found >= 0) {
              histIdx = found
              input = __history[found]
            } else {
              // Walked off the newest match — restore the anchor
              // (whatever the user had typed before Up).
              histIdx = -1
              input = histAnchor
            }
            redrawPrompt = true
          }
        } else if (key == 0x000C) {                  // Ctrl+L
          Console.clear()
          Screen.window.clear()
          Screen.window.position = [1, 1]
          lastTotal = Console.total
          promptRow = Screen.window.position[1]
          redrawPrompt = true
        } else if (ev.codepoint != null && ev.codepoint >= 0x20) {
          input = input + ev.text
          histIdx = -1
          redrawPrompt = true
        }
      }
      if (done) break

      // Drain any new log entries that landed during the keystroke
      // (e.g. System.print echo + eval output from the Enter path).
      // The prompt row gets overwritten as we emit; conio scrolls
      // past the bottom for free.
      if (Console.total > lastTotal) {
        Screen.window.position = [1, promptRow]
        Screen.window.clearToLineEnd()
        lastTotal = paintNew_(lastTotal, false)
        promptRow = Screen.window.position[1]
        redrawPrompt = true
      }

      if (redrawPrompt) drawPrompt_(promptRow, currentModule, input)
    }

    Screen.restore(saved)
  }

  // Dispatches a finished line: a leading "/" makes it a console
  // command; anything else is Wren source for REPL.eval.  Returns the
  // (possibly updated) current module.
  //
  // We wrap the eval in Fiber.new{}.try() so a runtime abort in the
  // user's snippet doesn't tear the console out from under the Hook
  // dispatcher.  Wren's runtime only emits stack frames for *uncaught*
  // aborts, but the failed fiber still has its frames intact — we
  // hand it to REPL.printTrace_ which walks them and pushes them
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
    var f = Fiber.new { v = REPL.eval(current, line) }
    f.try()
    // REPL.eval returns null for a statement, [value] for an
    // expression — so v != null means "we got a value, even if v[0]
    // happens to be null itself".
    if (f.error != null) {
      System.print("error: " + f.error)
      REPL.printTrace_(f)
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
  // /quit, /q     — leave the console (same as Ctrl+` or Esc).
  // /?            — list commands.
  //
  // Sets the static field __quit when the user asks to leave; the main
  // loop checks it alongside the local `done` flag.  Static fields in
  // Wren are double-underscore-prefixed and class-scoped, which is
  // exactly the right shape for "the run loop's exit signal."
  static runCommand_(line, current) {
    if (line == "/?" || line == "/help") {
      System.print("commands: /in [module], /quit, /?")
      System.print("keys:     Up/Down (history; type a prefix first to filter),")
      System.print("          Ctrl+W (kill word), Ctrl+L (clear), Esc/Ctrl+` (quit)")
      return current
    }
    if (line == "/quit" || line == "/q") {
      __quit = true
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
      if (!REPL.hasModule(name)) {
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
    Screen.window.position = [1, row]
    Screen.window.clearToLineEnd()
    Screen.window.print("(%(mod)) wren> " + input)
  }

  // conio's cputs treats LF as line-feed-only (cursor down, same column).
  // The console expects \n to start a new line at column 1, so translate
  // every \n in `s` to \r\n on the way out.
  static put_(s) {
    var start = 0
    var i = 0
    while (i < s.count) {
      if (s[i] == "\n") {
        if (i > start) Screen.window.print(s[start...i])
        Screen.window.print("\r\n")
        start = i + 1
      }
      i = i + 1
    }
    if (start < s.count) Screen.window.print(s[start...s.count])
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
