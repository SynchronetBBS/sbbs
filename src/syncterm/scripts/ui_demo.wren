// Tiny interactive playground for the UI library.  Invoke from the
// Wren console (Ctrl+`):
//
//   import "ui_demo" for UiDemo
//   UiDemo.run()
//
// Builds an App with a Pane containing a ListView, runs the
// synchronous event loop, and quits on Esc.  Saves + restores the
// screen so you land back on the live console viewport when the
// demo exits.
//
// Sync runner is used because the console's main loop is itself
// inside Hook.onKey(Ctrl+`); doterm can't dispatch events through
// to a parked App fiber while that hook is on the stack.

import "ui_style"  for Theme
import "ui_widget" for Rect, Container
import "ui_pane"   for Pane
import "ui_list"   for ListView
import "ui_app"    for App
import "syncterm"  for Screen, Key

class UiDemo {
  static run() {
    var snap = Screen.save()
    var size = Screen.size                     // [w, h]

    var app = App.new()

    // Pane fills the viewport with a 1-cell margin.
    var pane = Pane.new()
    pane.bounds = Rect.new(2, 2, size[0] - 2, size[1] - 2)
    pane.title  = "ui_demo — Esc to quit"
    pane.focused = true
    app.root.add(pane)

    // List inside the pane's interior.
    var list = ListView.new()
    var ib   = pane.innerBounds
    list.bounds = Rect.new(ib.x, ib.y, ib.w, ib.h)
    list.items = [
      "alpha",      "beta",       "gamma",      "delta",
      "epsilon",    "zeta",       "eta",        "theta",
      "iota",       "kappa",      "lambda",     "mu",
      "nu",         "xi",         "omicron",    "pi",
      "rho",        "sigma",      "tau",        "upsilon",
      "phi",        "chi",        "psi",        "omega",
      "(end of list)"
    ]
    pane.add(list)

    // Quit on Esc.  Bound at App level so it works even if the list
    // doesn't consume the key (which it doesn't — Esc isn't a list
    // navigation key).
    app.bind(Key.escape, Fn.new {|k| app.quit() })

    app.runSync()
    Screen.restore(snap)
  }
}
