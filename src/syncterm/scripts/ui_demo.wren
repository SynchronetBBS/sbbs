// Interactive playground for the UI library.  Invoke from the
// Wren console (Ctrl+` or the Ctrl+S menu's "Wren Console" entry):
//
//   import "ui_demo" for UiDemo
//   UiDemo.run()
//
// Top level is a list of widget demos; pick one with Enter to launch
// it, Esc returns to the gallery.  Esc at the gallery quits.
//
// Each demo runs its own App via runSync (the Wren console blocks
// the main loop, so doterm can't dispatch through to a parked fiber);
// nesting is fine because every App save/restores Screen, mouse
// events, and CustomCursor on entry / exit.

import "ui_widget"    for Rect
import "ui_pane"      for Pane
import "ui_list"      for ListView
import "ui_input"     for TextInput
import "ui_checkbox"  for Checkbox
import "ui_radio"     for RadioGroup
import "ui_spinbox"   for SpinBox
import "ui_statusbar" for StatusBar
import "ui_menubar"   for MenuBar
import "ui_form"      for Form
import "ui_popup"     for Alert, Confirm, Prompt
import "ui_help"      for Help
import "ui_app"       for App
import "syncterm"     for Screen, Key, Input

class UiDemo {
  static run() {
    Screen.modalRun(Fn.new {
      var app  = App.new()
      var size = Screen.size
      var pane = Pane.new()
      pane.bounds   = Rect.new(2, 2, size[0] - 2, size[1] - 2)
      pane.title    = "ui_demo - Enter to launch, Esc to quit"
      pane.focused  = true
      pane.onClose  = Fn.new { app.quit() }
      pane.helpText = "UI library gallery.\n\n" +
                      "Up / Down       move the highlight\n" +
                      "Home / End      jump to ends\n" +
                      "PageUp/PageDown scroll a page\n" +
                      "Enter           launch the selected demo\n" +
                      "Esc / [X]       quit the gallery\n" +
                      "F1 / [?]        this help dialog\n\n" +
                      "Each demo runs its own App via runSync; nested " +
                      "Apps save and restore Screen, mouse events, and " +
                      "CustomCursor automatically."
      app.root.add(pane)

      var demos = [
        ["ListView - scrollable items + scrollbar",  Fn.new { runListDemo_() }],
        ["TextInput - single-line edit field",       Fn.new { runInputDemo_() }],
        ["Checkbox - boolean toggle",                Fn.new { runCheckboxDemo_() }],
        ["RadioGroup - mutually exclusive choices",  Fn.new { runRadioDemo_() }],
        ["SpinBox - numeric +/- with arrows",        Fn.new { runSpinboxDemo_() }],
        ["MenuBar - horizontal command strip",       Fn.new { runMenubarDemo_(app) }],
        ["StatusBar - bottom strip with segments",   Fn.new { runStatusbarDemo_() }],
        ["Form - mixed widgets in a layout",         Fn.new { runFormDemo_(app) }],
        ["Alert popup",                              Fn.new { runAlertDemo_(app) }],
        ["Confirm popup (Y/N)",                      Fn.new { runConfirmDemo_(app) }],
        ["Prompt popup (TextInput inside)",          Fn.new { runPromptDemo_(app) }],
        ["Help viewer (scrollable text)",            Fn.new { runHelpDemo_(app) }],
        ["F1 context help (per-widget helpText)",    Fn.new { runContextHelpDemo_() }],
        ["popStatus transient overlay",              Fn.new { runPopStatusDemo_(app) }],
      ]
      var labels = []
      for (d in demos) labels.add(d[0])

      var list = ListView.new()
      var ib   = pane.innerBounds
      list.bounds = Rect.new(ib.x, ib.y, ib.w, ib.h)
      list.items  = labels
      list.onSelect = Fn.new {|i, item|
        demos[i][1].call()
        // Sub-app's Screen.restore wiped the gallery, so repaint.
        app.root.markDirty()
      }
      pane.add(list)

      app.bind(Key.escape, Fn.new {|k| app.quit() })
      app.runSync()
    })
  }

  // ----- ListView demo --------------------------------------------

  static runListDemo_() {
    Screen.modalRun(Fn.new {
      var app  = App.new()
      var size = Screen.size
      var pane = Pane.new()
      pane.bounds   = Rect.new(2, 2, size[0] - 2, size[1] - 2)
      pane.title    = "ListView"
      pane.focused  = true
      pane.helpText = "ListView demo:\n\n" +
                      "Up / Down       move the highlight\n" +
                      "Home / End      jump to ends\n" +
                      "PageUp/PageDown scroll a page\n" +
                      "Mouse wheel     scroll three rows\n" +
                      "Click scrollbar arrows to step one row\n" +
                      "Click [?] for this dialog, [■] to return\n" +
                      "Esc returns to the gallery."
      pane.onClose  = Fn.new { app.quit() }
      app.root.add(pane)

      var list = ListView.new()
      var ib   = pane.innerBounds
      list.bounds = Rect.new(ib.x, ib.y, ib.w, ib.h)
      list.items = [
        "alpha",   "beta",    "gamma",   "delta",
        "epsilon", "zeta",    "eta",     "theta",
        "iota",    "kappa",   "lambda",  "mu",
        "nu",      "xi",      "omicron", "pi",
        "rho",     "sigma",   "tau",     "upsilon",
        "phi",     "chi",     "psi",     "omega",
        "(end of list)"
      ]
      pane.add(list)

      app.bind(Key.escape, Fn.new {|k| app.quit() })
      app.runSync()
    })
  }

  // ----- Form-control demos ---------------------------------------

  static runCheckboxDemo_() {
    Screen.modalRun(Fn.new {
      var app  = App.new()
      var size = Screen.size
      var pane = Pane.new()
      pane.bounds   = Rect.new(2, 2, size[0] - 2, size[1] - 2)
      pane.title    = "Checkbox - Space/Enter toggles, Esc to return"
      pane.focused  = true
      pane.onClose  = Fn.new { app.quit() }
      pane.helpText = "Checkbox is a single-row [X] Label toggle.\n\n" +
                      "Tab / Down      move focus to the next box\n" +
                      "Space / Enter   toggle the focused box\n" +
                      "Mouse click     toggle the clicked box\n\n" +
                      "Each Checkbox owns one Bool; onChange fires only " +
                      "when the value actually changes."
      app.root.add(pane)

      var ib = pane.innerBounds
      var c1 = Checkbox.new("Show drop shadows")
      c1.bounds = Rect.new(ib.x + 2, ib.y + 1, 30, 1)
      c1.value  = true
      pane.add(c1)
      var c2 = Checkbox.new("Auto-reconnect on drop")
      c2.bounds = Rect.new(ib.x + 2, ib.y + 3, 30, 1)
      pane.add(c2)
      var c3 = Checkbox.new("Verbose logging")
      c3.bounds = Rect.new(ib.x + 2, ib.y + 5, 30, 1)
      pane.add(c3)

      app.bind(Key.escape, Fn.new {|k| app.quit() })
      app.runSync()
    })
  }

  static runRadioDemo_() {
    Screen.modalRun(Fn.new {
      var app  = App.new()
      var size = Screen.size
      var pane = Pane.new()
      pane.bounds   = Rect.new(2, 2, size[0] - 2, size[1] - 2)
      pane.title    = "RadioGroup - Up/Down moves, Space/Enter selects"
      pane.focused  = true
      pane.onClose  = Fn.new { app.quit() }
      pane.helpText = "RadioGroup is a vertical list of mutually-" +
                      "exclusive options.\n\n" +
                      "Up / Down       move the cursor (wraps standalone)\n" +
                      "Home / End      jump to ends\n" +
                      "Space / Enter   commit the cursor row as selected\n" +
                      "Mouse click     select the clicked row\n\n" +
                      "wrap defaults to true here (standalone); inside a " +
                      "Form, addField sets it to false so Up/Down at the " +
                      "edges traverse to other fields."
      app.root.add(pane)

      var ib = pane.innerBounds
      var g  = RadioGroup.new()
      g.bounds = Rect.new(ib.x + 2, ib.y + 1, 30, 5)
      g.items  = ["None", "Single-line", "Double-line", "Heavy", "Rounded"]
      pane.add(g)

      app.bind(Key.escape, Fn.new {|k| app.quit() })
      app.runSync()
    })
  }

  static runSpinboxDemo_() {
    Screen.modalRun(Fn.new {
      var app  = App.new()
      var size = Screen.size
      var pane = Pane.new()
      pane.bounds   = Rect.new(2, 2, size[0] - 2, size[1] - 2)
      pane.title    = "SpinBox - Up/Down/+/- changes, Esc to return"
      pane.focused  = true
      pane.onClose  = Fn.new { app.quit() }
      pane.helpText = "SpinBox is a numeric input with up/down step " +
                      "buttons.\n\n" +
                      "Up / +              add step\n" +
                      "Down / -            subtract step\n" +
                      "PageUp / PageDown   step * 10\n" +
                      "Home / End          jump to min / max\n" +
                      "Mouse wheel         step ±1\n" +
                      "Click +▲+ / +▼+         step ±1\n\n" +
                      "value= clamps to [min, max].  This demo: " +
                      "min=0, max=200, step=5."
      app.root.add(pane)

      var ib = pane.innerBounds
      var s = SpinBox.new()
      s.bounds = Rect.new(ib.x + 2, ib.y + 1, 12, 1)
      s.min    = 0
      s.max    = 200
      s.step   = 5
      s.value  = 100
      pane.add(s)

      app.bind(Key.escape, Fn.new {|k| app.quit() })
      app.runSync()
    })
  }

  static runMenubarDemo_(parentApp) {
    Screen.modalRun(Fn.new {
      var app  = App.new()
      var size = Screen.size

      var bar = MenuBar.new()
      bar.bounds  = Rect.new(1, 1, size[0], 1)
      bar.focused = true
      bar.items   = [
        ["File", Fn.new { Alert.show(app, "File menu activated") }],
        ["Edit", Fn.new { Alert.show(app, "Edit menu activated") }],
        ["View", Fn.new { Alert.show(app, "View menu activated") }],
        ["Help", Fn.new { Alert.show(app, "Help menu activated") }],
      ]
      app.root.add(bar)

      app.bind(Key.escape, Fn.new {|k| app.quit() })
      app.popStatus("Left/Right move, Enter activates, " +
                    "letter = hotkey, Esc returns")
      app.runSync()
    })
  }

  static runStatusbarDemo_() {
    Screen.modalRun(Fn.new {
      var app  = App.new()
      var size = Screen.size

      var sb = StatusBar.new()
      sb.bounds   = Rect.new(1, size[1], size[0], 1)
      sb.segments = [
        ["F1 Help  Esc Quit", "left"],
        ["StatusBar demo",    "center"],
        ["09:42",             "right"],
      ]
      app.root.add(sb)

      app.bind(Key.escape, Fn.new {|k| app.quit() })
      app.popStatus("StatusBar at the bottom of the screen - " +
                    "Esc returns")
      app.runSync()
    })
  }

  static runFormDemo_(parentApp) {
    Screen.modalRun(Fn.new {
      var app  = App.new()
      var size = Screen.size

      var pane = Pane.new()
      pane.bounds   = Rect.new(4, 3, size[0] - 6, size[1] - 5)
      pane.title    = "Form - Tab to move, Enter to submit"
      pane.focused  = true
      pane.onClose  = Fn.new { app.quit() }
      pane.helpText = "Form is a Container that lays out (label, " +
                      "widget) pairs in a vertical stack.\n\n" +
                      "Tab / Down      next field\n" +
                      "BackTab / Up    previous field\n" +
                      "Enter           submit (fires onSubmit)\n" +
                      "Esc             cancel (fires onCancel)\n\n" +
                      "RadioGroup wrap is auto-disabled inside a Form, " +
                      "so Up/Down at its edges escape to the next field."
      app.root.add(pane)

      var f = Form.new()
      f.bounds = pane.innerBounds

      var name = TextInput.new()
      name.maxLen = 32
      f.addField("Name:", name)

      var port = SpinBox.new()
      port.min = 1
      port.max = 65535
      port.step = 1
      port.value = 23
      f.addField("Port:", port)

      var enc = RadioGroup.new()
      enc.items = ["None", "SSL", "SSH"]
      f.addFieldH("Encryption:", enc, 3)

      var auto = Checkbox.new("Connect on startup")
      auto.value = true
      f.addField("", auto)

      f.onSubmit = Fn.new {
        Alert.show(app,
                   "Submitted: name=%(name.value), port=%(port.value)")
      }
      f.onCancel = Fn.new { app.quit() }
      pane.add(f)

      app.bind(Key.escape, Fn.new {|k| app.quit() })
      app.runSync()
    })
  }

  // ----- Popup demos ----------------------------------------------
  // These reuse the gallery's App — modals push onto the same stack
  // and the existing runSync loop pumps them.

  static runAlertDemo_(app) {
    Alert.show(app, "Disk almost full.")
  }

  static runConfirmDemo_(app) {
    var ok = Confirm.show(app, "Reformat the universe?")
    Alert.show(app, ok ? "OK, reformatting..." : "Cancelled.")
  }

  static runPromptDemo_(app) {
    var name = Prompt.show(app, "Your name?", "anonymous")
    if (name == null) {
      Alert.show(app, "Cancelled.")
    } else {
      Alert.show(app, "Hello, %(name)!")
    }
  }

  // ----- Help demo ------------------------------------------------

  static runHelpDemo_(app) {
    var body = "Welcome to the help viewer.\n" +
               "\n" +
               "Up / Down       move the highlight\n" +
               "PageUp/PageDown scroll a page\n" +
               "Home / End      jump to top / bottom\n" +
               "Mouse wheel     scroll three rows at a time\n" +
               "Scrollbar drag  scroll proportionally\n" +
               "Esc             close this dialog\n" +
               "\n" +
               "This is just a ListView in a Pane fed one row per " +
               "line; scrolling, selection, and the scrollbar all " +
               "come for free.\n" +
               "\n" +
               "Long enough body to overflow:\n" +
               "1.  one\n" +
               "2.  two\n" +
               "3.  three\n" +
               "4.  four\n" +
               "5.  five\n" +
               "6.  six\n" +
               "7.  seven\n" +
               "8.  eight\n" +
               "9.  nine\n" +
               "10. ten\n" +
               "11. eleven\n" +
               "12. twelve\n" +
               "(end)"
    Help.show(app, "Help viewer demo", body)
  }

  // ----- Context-help demo ---------------------------------------

  static runContextHelpDemo_() {
    Screen.modalRun(Fn.new {
      var app  = App.new()
      var size = Screen.size

      var pane = Pane.new()
      pane.bounds  = Rect.new(2, 2, size[0] - 2, size[1] - 2)
      pane.title   = "Context help - press F1, Esc to return"
      pane.focused = true
      pane.onClose = Fn.new { app.quit() }
      pane.helpText = "This is the Pane's help text.\n" +
                      "\n" +
                      "F1 walks from the focused leaf upward through\n" +
                      "the parent chain until it finds the first widget\n" +
                      "with helpText set.  Try pressing F1 here, then\n" +
                      "press Esc to dismiss the help dialog and return."
      app.root.add(pane)

      var input = TextInput.new()
      var ib    = pane.innerBounds
      input.bounds = Rect.new(ib.x + 1, ib.y + 1, (ib.w - 2).max(10), 1)
      input.value  = "(focused; F1 finds the Pane's help)"
      pane.add(input)

      app.bind(Key.escape, Fn.new {|k| app.quit() })
      app.runSync()
    })
  }

  // ----- popStatus demo ------------------------------------------

  static runPopStatusDemo_(app) {
    app.popStatus("Working... press any key")
    // Block on raw input so the overlay shows uninterrupted.  Using
    // Alert here would centre on top of the (also centred) status
    // popup — UIFC's `pop` has the same placement, so by convention
    // a status overlay is meant to coexist with a backdrop, not
    // another modal.
    Input.next()
    app.popStatus(null)
  }

  // ----- TextInput demo -------------------------------------------

  static runInputDemo_() {
    Screen.modalRun(Fn.new {
      var app  = App.new()
      var size = Screen.size
      var pane = Pane.new()
      pane.bounds   = Rect.new(2, 2, size[0] - 2, size[1] - 2)
      pane.title    = "TextInput - type, Enter to submit, Esc to return"
      pane.focused  = true
      pane.onClose  = Fn.new { app.quit() }
      pane.helpText = "TextInput is a single-line edit field.  Stores " +
                      "the value as a list of codepoints, so the " +
                      "cursor steps one cell per codepoint.\n\n" +
                      "Left / Right      move cursor\n" +
                      "Home / End        ends of the field\n" +
                      "Backspace / Del   edit\n" +
                      "Enter             submit (fires onSubmit)\n" +
                      "Mouse click       position cursor at clicked cell\n\n" +
                      "maxLen caps the codepoint count.  Tab and arrow " +
                      "keys not consumed here would let a Container " +
                      "traverse focus."
      app.root.add(pane)

      var input = TextInput.new()
      var ib    = pane.innerBounds
      // 1-row input centred horizontally a couple rows below the top.
      var w = (ib.w - 4).max(10).min(40)
      var x = ib.x + ((ib.w - w) / 2).floor
      input.bounds = Rect.new(x, ib.y + 1, w, 1)
      input.maxLen = 80
      input.onSubmit = Fn.new {|s| app.quit() }
      pane.add(input)

      app.bind(Key.escape, Fn.new {|k| app.quit() })
      app.runSync()
    })
  }
}
