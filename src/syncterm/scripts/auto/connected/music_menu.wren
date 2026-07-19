// music_menu.wren — ANSI music controls driven by Hook.onKey(Key.altM)
// in keys_default.wren
// and from the Alt-Z online menu's "ANSI Music Control" entry.  A
// modal ListView seeded with Host.musicNames; choosing an entry
// updates CTerm.music and closes the modal.

import "syncterm"  for CTerm, Host, Input, Screen
import "ui_picker" for ListPicker

class MusicMenu {
  // Entry point.  Wraps the modal flow in a child fiber so the
  // caller (a Hook.onKey body or the online menu's dispatch) can
  // return synchronously while the modal runs (Hook.dispatch_'s
  // no-direct-yield rule — see wren.md §8 / App's header comment).
  static run() {
    Fiber.new {
      Screen.modalRun(Fn.new {
        var picked = ListPicker.pickWithSelection("ANSI Music Setup",
            Host.musicHelp, Host.musicNames, CTerm.music)
        if (picked >= 0) CTerm.music = picked
      })
      Input.setupMouseEvents()
    }.call()
  }
}
