// SyncTERM Wren UI library — picker helpers.
//
// Small convenience wrappers for the common "modal Pane containing a
// ListView" shape.  Keep special-purpose flows local when they need
// custom key bindings or placement.

import "syncterm" for Key
import "ui_app"   for App
import "ui_pane"  for Pane
import "ui_list"  for ListView

class ListPicker {
  static pick(title, helpText, items) {
    return pickWithSelection(title, helpText, items, null)
  }

  static pickWithSelection(title, helpText, items, selected) {
    var picked = -1
    var app  = App.new()
    var pane = Pane.new()
    pane.title    = title
    pane.helpText = helpText
    pane.focused  = true
    pane.onClose  = Fn.new { app.quit() }
    app.root.add(pane)

    var list = ListView.new()
    list.items = items
    if (selected != null) list.selected = selected
    list.onSelect = Fn.new { |i, item|
      picked = i
      app.quit()
    }
    pane.add(list)
    pane.fitContent()
    pane.centerOnScreen()

    app.bind(Key.escape, Fn.new { |k| app.quit() })
    app.run()
    return picked
  }
}
