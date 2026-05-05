// online_menu.wren — Wren replacement for the C-side syncmenu()
// uifc dialog reached via Alt-Z (always) and Ctrl-S (text-mode only).
// Presents a ListView of session-control commands and dispatches the
// selection back into the Wren primitives that already drive the
// individual hot-keys (Conn.scrollback, Conn.upload, Conn.download,
// CaptureMenu.run, MusicMenu.run, ...).
//
// Most selections close the menu and return to the terminal — re-open
// with Alt-Z if you need to change another setting.  The Output Rate
// and Log Level entries chain into a sub-list before closing.

import "syncterm" for BBS, ConnType, Conn, CTerm, Hook, Host, Input, Key, Screen
import "ui_app"   for App
import "ui_pane"  for Pane
import "ui_list"  for ListView
import "ui_popup" for Alert
import "capture_menu"    for CaptureMenu
import "music_menu"      for MusicMenu
import "scrollback_view" for ScrollbackView
import "connected"       for Connected
import "console"         for WrenConsole

class OnlineMenu {
  // Build the (label, Fn) pairs for the current build / mode.  The
  // disconnect entry's hint differs between text-mode (Ctrl-Q hooked)
  // and graphical (Alt-H instead).  In text-mode the historical C
  // dialog stripped every "(Alt-...)" hint because Alt-keys aren't
  // reliably delivered on raw terminals — we mirror that here.
  static buildEntries_() {
    var hint = !Host.textTerminal
    var disconnectLabel = hint ? "Disconnect (Alt-H)" : "Disconnect (Ctrl-Q)"
    var entries = [
      [ hint ? "Scrollback (Alt-B)"         : "Scrollback",
        Fn.new { ScrollbackView.run() } ],
      // No confirm prompt for the menu's Disconnect / Exit entries —
      // selecting them from the list IS the confirmation step.  The
      // hot-key paths (Alt-X / Alt-H / Ctrl-Q / window-close) still go
      // through DisconnectFlow.
      [ disconnectLabel,
        Fn.new { Conn.endSession(false) } ],
      [ hint ? "Send Login (Alt-L)"         : "Send Login",
        Fn.new { Connected.sendLogin() } ],
      [ hint ? "Upload (Alt-U)"             : "Upload",
        Fn.new { Conn.upload() } ],
      [ hint ? "Download (Alt-D)"           : "Download",
        Fn.new { Conn.download() } ],
      [ hint ? "Change Output Rate (Alt-Up/Alt-Down)" : "Change Output Rate",
        Fn.new { outputRateFlow_() } ],
      [ "Change Log Level",
        Fn.new { logLevelFlow_() } ],
      [ hint ? "Capture Control (Alt-C)"    : "Capture Control",
        Fn.new { CaptureMenu.run() } ],
      [ hint ? "ANSI Music Control (Alt-M)" : "ANSI Music Control",
        Fn.new { MusicMenu.run() } ],
      [ hint ? "Font Setup (Alt-F)"         : "Font Setup",
        Fn.new { Host.fontControl() } ],
      [ "Toggle Doorway Mode",
        Fn.new { CTerm.doorwayMode = !CTerm.doorwayMode } ],
      [ hint ? "Toggle Remote Mouse (Alt-O)" : "Toggle Remote Mouse",
        Fn.new {
          CTerm.mouseDisabled = !CTerm.mouseDisabled
          Input.setupMouseEvents()
        } ],
    ]
    if (Host.haveOOII) {
      entries.add([
        "Toggle Operation Overkill ][ Mode",
        Fn.new {
          var next = CTerm.ooiiMode + 1
          if (next > Host.maxOOIIMode) next = 0
          CTerm.ooiiMode = next
        }])
    }
    entries.add([ hint ? "Exit (Alt-X)"     : "Exit",
                  Fn.new { Conn.endSession(true) } ])
    entries.add([ hint ? "Edit Dialing Directory (Alt-E)" : "Edit Dialing Directory",
                  Fn.new { Host.editBBSList() } ])
    // No key hint on Wren Console — Ctrl+` works on backends that pass
    // the byte through, but ANSI/Curses can't capture it, so the menu
    // entry is the universal access path.
    entries.add([ "Wren Console", Fn.new { WrenConsole.run() } ])
    return entries
  }

  static helpText_ {
    return "# Online Menu\n\n" +
      "Scrollback\n" +
      ":  Allows you to view the scrollback buffer\n" +
      "Disconnect\n" +
      ":  Disconnects the current connection\n" +
      "Send Login\n" +
      ":  Sends the username and password pair separated by CR\n" +
      "Upload / Download\n" +
      ":  Initiates a file upload or download\n" +
      "Output Rate\n" +
      ":  Changes the speed characters are output to the screen\n" +
      "Log Level\n" +
      ":  Minimum log level for transfer information\n" +
      "Capture\n" +
      ":  Enables/disables screen capture\n" +
      "ANSI Music\n" +
      ":  Enables/disables ANSI music\n" +
      "Font\n" +
      ":  Changes the current font (when supported)\n" +
      "Doorway Mode\n" +
      ":  Toggles the current DoorWay (keyboard input) setting\n" +
      "Remote Mouse\n" +
      ":  Toggles remote mouse events\n" +
      "Operation Overkill ][ Mode\n" +
      ":  Cycles the OOII tone-emulation mode\n" +
      "Exit\n" +
      ":  Disconnects and closes SyncTERM\n" +
      "Edit Dialing Directory\n" +
      ":  Opens the directory/setting menu\n" +
      "Wren Console\n" +
      ":  Opens the embedded Wren scripting console"
  }

  // Entry point.  Hook handlers wrap this in a child fiber so the
  // modal loop can yield freely (Hook.dispatch_'s no-direct-yield
  // rule -- see wren.md §8 / App's header comment).
  //
  // The chosen action runs OUTSIDE the menu's Screen.modalRun so the
  // screen is fully restored to terminal state before it fires.  Sub-
  // flows (CaptureMenu, MusicMenu, DisconnectFlow, ScrollbackView,
  // outputRateFlow_, logLevelFlow_) wrap themselves in their own
  // modalRun, so they save/restore against the now-clean terminal.
  static run() {
    Fiber.new {
      var entries = buildEntries_()
      var picked  = [-1]

      Screen.modalRun(Fn.new {
        var app  = App.new()
        var pane = Pane.new()
        pane.title    = "SyncTERM Online Menu"
        pane.helpText = helpText_
        pane.focused  = true
        pane.onClose  = Fn.new { app.quit() }
        app.root.add(pane)

        var labels = []
        for (e in entries) labels.add(e[0])

        var list = ListView.new()
        list.items    = labels
        list.onSelect = Fn.new { |i, item|
          picked[0] = i
          app.quit()
        }
        pane.add(list)
        pane.fitContent()
        pane.centerOnScreen()
        app.bind(Key.escape, Fn.new { |k| app.quit() })
        app.run()
      })
      Input.setupMouseEvents()

      var sel = picked[0]
      if (sel >= 0) entries[sel][1].call()
    }.call()
  }

  // Output Rate sub-list.  Mirrors the historical SM_OUTPUT_RATE
  // branch — refuses to operate on serial / modem connections (the
  // configured port speed is the rate, not a script-tweakable cap).
  // Wraps in its own Screen.modalRun since it's invoked from outside
  // the parent menu's modal context.
  static outputRateFlow_() {
    var ct = BBS.connType
    if (ct == ConnType.modem || ct == ConnType.serial || ct == ConnType.serialNoRts) {
      Screen.modalRun(Fn.new {
        var app = App.new()
        app.bind(Key.escape, Fn.new { |k| app.quit() })
        Alert.show(app,
          "Not supported for this connection type.\n\n" +
          "Cannot change the display rate for Modem/Serial connections.")
        app.run()
      })
      Input.setupMouseEvents()
      return
    }

    var rates = Host.outputRates
    var names = Host.outputRateNames
    var cur   = CTerm.throttleSpeed
    var idx   = 0
    for (i in 0...rates.count) {
      if (rates[i] == cur) {
        idx = i
        break
      }
    }

    var picked = [-1]
    Screen.modalRun(Fn.new {
      var app  = App.new()
      var pane = Pane.new()
      pane.title    = "Output Rate"
      pane.helpText =
        "# Output Rate\n\n" +
        "The output rate is the rate in emulated \"bits per second\" to draw\n" +
        "incoming data on the screen.  This rate is a maximum, not guaranteed\n" +
        "to be attained.  In general, you will only use this option for ANSI\n" +
        "animations."
      pane.focused = true
      pane.onClose = Fn.new { app.quit() }
      app.root.add(pane)

      var list = ListView.new()
      list.items    = names
      list.selected = idx
      list.onSelect = Fn.new { |i, item|
        picked[0] = i
        app.quit()
      }
      pane.add(list)
      pane.fitContent()
      pane.centerOnScreen()
      app.bind(Key.escape, Fn.new { |k| app.quit() })
      app.run()
    })
    Input.setupMouseEvents()

    if (picked[0] >= 0) CTerm.throttleSpeed = rates[picked[0]]
  }

  // Log Level sub-list.  Indexes line up with Host.logLevelNames.
  // Wraps in its own Screen.modalRun (see outputRateFlow_).
  static logLevelFlow_() {
    var names  = Host.logLevelNames
    var picked = [-1]

    Screen.modalRun(Fn.new {
      var app  = App.new()
      var pane = Pane.new()
      pane.title    = "Log Level"
      pane.helpText =
        "# Log Level\n\n" +
        "The log level changes the verbosity of messages shown in the transfer\n" +
        "window.  For the selected log level, messages of that level and those\n" +
        "above it will be displayed."
      pane.focused = true
      pane.onClose = Fn.new { app.quit() }
      app.root.add(pane)

      var list = ListView.new()
      list.items    = names
      list.selected = Host.logLevel
      list.onSelect = Fn.new { |i, item|
        picked[0] = i
        app.quit()
      }
      pane.add(list)
      pane.fitContent()
      pane.centerOnScreen()
      app.bind(Key.escape, Fn.new { |k| app.quit() })
      app.run()
    })
    Input.setupMouseEvents()

    if (picked[0] >= 0) Host.logLevel = picked[0]
  }
}

// Alt-Z is always installed; Ctrl-S only on text-mode terminals (the
// curses/ANSI backends had the historical Ctrl-S dispatch path).
Hook.onKey(Key.altZ) { |k|
  OnlineMenu.run()
  return true
}
if (Host.textTerminal) {
  Hook.onKey(Key.ctrlS) { |k|
    OnlineMenu.run()
    return true
  }
}
