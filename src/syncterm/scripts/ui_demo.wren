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

import "ui_widget" for Rect
import "ui_pane"   for Pane
import "ui_list"   for ListView
import "ui_input"  for TextInput
import "ui_popup"  for Alert, Confirm, Prompt
import "ui_help"   for Help
import "ui_app"    for App
import "syncterm"  for Screen, Key, Input

class UiDemo {
  static run() {
    var snap = Screen.save()

    var app  = App.new()
    var size = Screen.size
    var pane = Pane.new()
    pane.bounds  = Rect.new(2, 2, size[0] - 2, size[1] - 2)
    pane.title   = "ui_demo - Enter to launch, Esc to quit"
    pane.focused = true
    pane.onClose = Fn.new { app.quit() }
    app.root.add(pane)

    var demos = [
      ["ListView - scrollable items + scrollbar",  Fn.new { runListDemo_() }],
      ["TextInput - single-line edit field",       Fn.new { runInputDemo_() }],
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
    Screen.restore(snap)
  }

  // ----- ListView demo --------------------------------------------

  static runListDemo_() {
    var snap = Screen.save()

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
    Screen.restore(snap)
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
    var snap = Screen.save()
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
    Screen.restore(snap)
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
    var snap = Screen.save()

    var app  = App.new()
    var size = Screen.size
    var pane = Pane.new()
    pane.bounds  = Rect.new(2, 2, size[0] - 2, size[1] - 2)
    pane.title   = "TextInput - type, Enter to submit, Esc to return"
    pane.focused = true
    pane.onClose = Fn.new { app.quit() }
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
    Screen.restore(snap)
  }
}
