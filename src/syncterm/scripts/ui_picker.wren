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
    return pick_(title, helpText, items, null, false)
  }

  static pickWithSelection(title, helpText, items, selected) {
    return pick_(title, helpText, items, selected, false)
  }

  static pickSync(title, helpText, items) {
    return pick_(title, helpText, items, null, true)
  }

  static pickWithSelectionSync(title, helpText, items, selected) {
    return pick_(title, helpText, items, selected, true)
  }

  static pick_(title, helpText, items, selected, synchronous) {
    var picked = -1
    var app  = App.new()
    var pane = Pane.new()
    pane.title    = title
    pane.helpText = helpText
    pane.focused  = true
    pane.shadow   = true
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
    pane.fitContentToScreen()

    app.bind(Key.escape, Fn.new { |k| app.quit() })
    if (synchronous) {
      app.runSync()
    } else {
      app.run()
    }
    return picked
  }
}
