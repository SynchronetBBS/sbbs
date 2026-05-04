// music_menu.wren — Wren replacement for the C-side music_control()
// uifc dialog.  Driven by Hook.onKey(Key.altM) in keys_default.wren
// and from the Alt-Z online menu's "ANSI Music Control" entry.  A
// modal ListView seeded with Host.musicNames; choosing an entry
// updates CTerm.music and closes the modal.

import "syncterm" for CTerm, Host, Input, Key, Screen
import "ui_app"   for App
import "ui_pane"  for Pane
import "ui_list"  for ListView

class MusicMenu {
  // Entry point.  Wraps the modal flow in a child fiber so the
  // caller (a Hook.onKey body or the online menu's dispatch) can
  // return synchronously while the modal runs (Hook.dispatch_'s
  // no-direct-yield rule — see wren.md §8 / App's header comment).
  static run() {
    Fiber.new {
      Screen.modalRun(Fn.new {
        var app  = App.new()
        var pane = Pane.new()
        pane.title    = "ANSI Music Setup"
        pane.helpText = Host.musicHelp
        pane.focused  = true
        pane.onClose  = Fn.new { app.quit() }
        app.root.add(pane)

        var list = ListView.new()
        list.items    = Host.musicNames
        list.selected = CTerm.music
        list.onSelect = Fn.new { |i, item|
          CTerm.music = i
          app.quit()
        }
        pane.add(list)
        pane.fitContent()
        pane.centerOnScreen()

        app.bind(Key.escape, Fn.new { |k| app.quit() })
        app.run()
      })
      Input.setupMouseEvents()
    }.call()
  }
}
