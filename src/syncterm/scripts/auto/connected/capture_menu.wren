// capture_menu.wren — Wren replacement for the C-side capture_control()
// uifc dialog.  Driven by Hook.onKey(Key.altC) in keys_default.wren.
//
// Three states based on the current Capture.* status:
//
//   not capturing  →  ask "Capture Type" (ASCII / Raw / Binary [+SAUCE])
//                  →  ask save path (Host.pickSavePath)
//                  →  Capture.start(file, raw) for ASCII/Raw, or
//                     CTerm.saveScreenshot(file, withSauce) for binary
//   paused         →  ask "Unpause" or "Close"
//   active         →  ask "Pause" or "Close"
//
// Help text and option labels are kept close to the historical uifc
// dialog so muscle memory carries over.

import "syncterm" for Capture, CTerm, Download, Host, Input, Key, Screen
import "ui_app"   for App
import "ui_pane"  for Pane
import "ui_list"  for ListView

class CaptureMenu {
  // Entry point.  Hook handlers wrap this in a child fiber so the
  // modal loop can yield freely (Hook.dispatch_'s no-direct-yield
  // rule -- see wren.md §8 / App's header comment).
  static run() {
    Fiber.new {
      Screen.modalRun(Fn.new {
        if (!Capture.active) {
          startFlow_()
        } else if (Capture.paused) {
          pickAndAct_("Capture Control",
            "# Capture Control\n\n" +
            "- **Unpause** - Continues logging\n" +
            "- **Close** - Closes the log",
            ["Unpause", "Close"],
            Fn.new { |i|
              if (i == 0) {
                Capture.resume()
              } else if (i == 1) {
                Capture.stop()
              }
            })
        } else {
          pickAndAct_("Capture Control",
            "# Capture Control\n\n" +
            "- **Pause** - Suspends logging\n" +
            "- **Close** - Closes the log",
            ["Pause", "Close"],
            Fn.new { |i|
              if (i == 0) {
                Capture.pause()
              } else if (i == 1) {
                Capture.stop()
              }
            })
        }
      })
      Input.setupMouseEvents()
    }.call()
  }

  // Type-selection + save-picker + dispatch.  Indices in the type
  // list:
  //   0 = ASCII            (streaming log, escapes stripped)
  //   1 = Raw              (streaming log, escapes preserved)
  //   2 = Binary           (one-shot screen save)
  //   3 = Binary with SAUCE
  static startFlow_() {
    var picked = [-1]
    var app  = App.new()
    var pane = Pane.new()
    pane.title    = "Capture Type"
    pane.helpText =
      "# Capture Type\n" +
      "\n" +
      "ASCII\n" +
      ":  Plain text, no escape sequences\n" +
      "Raw\n" +
      ":  Preserves ANSI escape sequences\n" +
      "Binary\n" +
      ":  Saves the current screen as IBM-CGA / BinaryText\n" +
      "Binary with SAUCE\n" +
      ":  Same as Binary plus a SAUCE block\n" +
      "\n" +
      "**Raw** is useful for stealing ANSI screens from other systems.  \n" +
      "Don't do that though.  :-)"
    pane.focused = true
    pane.onClose = Fn.new { app.quit() }
    app.root.add(pane)

    var list = ListView.new()
    list.items    = ["ASCII", "Raw", "Binary", "Binary with SAUCE"]
    list.onSelect = Fn.new { |i, item|
      picked[0] = i
      app.quit()
    }
    pane.add(list)
    pane.fitContent()
    pane.centerOnScreen()
    app.bind(Key.escape, Fn.new { |k| app.quit() })
    app.run()

    var sel = picked[0]
    if (sel < 0) {
      return
    }
    var mask = (sel >= 2) ? "*.bin" : null
    var file = Host.pickSavePath(Download, mask)
    if (file == null) {
      return
    }
    if (sel >= 2) {
      CTerm.saveScreenshot(file, sel > 2)
    } else {
      Capture.start(file, sel == 1)
    }
  }

  // Generic "show a list, dispatch on selection" helper used by the
  // paused / active flows.  `action` is called with the chosen
  // index (>= 0) -- nothing happens on cancel.
  static pickAndAct_(title, helpText, items, action) {
    var picked = [-1]
    var app  = App.new()
    var pane = Pane.new()
    pane.title    = title
    pane.helpText = helpText
    pane.focused  = true
    pane.onClose  = Fn.new { app.quit() }
    app.root.add(pane)

    var list = ListView.new()
    list.items    = items
    list.onSelect = Fn.new { |i, item|
      picked[0] = i
      app.quit()
    }
    pane.add(list)
    pane.fitContent()
    pane.centerOnScreen()
    app.bind(Key.escape, Fn.new { |k| app.quit() })
    app.run()

    if (picked[0] >= 0) {
      action.call(picked[0])
    }
  }
}
