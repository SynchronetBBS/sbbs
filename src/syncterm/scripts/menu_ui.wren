import "syncterm" for Screen
import "ui_app" for App
import "ui_widget" for Rect
import "ui_pane" for Pane
import "ui_list" for ListView
import "ui_popup" for Alert, Confirm, Prompt, Popup

class MenuUi {
  static promptStandalone(title, message, initial, maxLen, masked) {
    var app = App.new()
    var p = Prompt.new(message, initial)
    p.title = title
    p.input.maxLen = maxLen
    if (masked) p.input.mask = "*"
    p.bounds = Popup.centeredBounds_(message, 2, 34)
    p.onDismiss = Fn.new {|value| app.quit() }
    app.pushModal(p)
    app.runSync()
    return p.result
  }

  static alertStandalone(title, message) {
    var app = App.new()
    var p = Alert.new(title, message)
    p.bounds = Popup.centeredBounds_(message, 1, 24)
    p.onDismiss = Fn.new {|value| app.quit() }
    app.pushModal(p)
    app.runSync()
  }

  static rowName(rows, value) {
    for (row in rows) {
      if (row[0] == value) return row[1]
    }
    return "Unknown (%(value))"
  }

  static rowIndex(rows, value) {
    for (i in 0...rows.count) {
      if (rows[i][0] == value) return i
    }
    return 0
  }

  static choice(app, title, rows, current) {
    return choice(app, title, rows, current, null)
  }

  static choice(app, title, rows, current, helpText) {
    if (rows.count == 0) return null
    var result = null
    var pane = Pane.new()
    pane.title = title
    pane.helpText = helpText
    pane.focused = true
    var size = Screen.size
    var longest = title.count + 6
    var labels = []
    for (row in rows) {
      var label = row is List ? row[1] : row
      labels.add(label)
      if (label.count + 4 > longest) longest = label.count + 4
    }
    var w = longest.max(24).min(size[0] - 4)
    var h = (labels.count + 4).max(7).min(size[1] - 4)
    pane.bounds = Rect.new(((size[0] - w) / 2).floor + 1,
        ((size[1] - h) / 2).floor + 1, w, h)
    pane.onClose = Fn.new { app.popModal() }

    var list = ListView.new()
    list.bounds = pane.innerBounds
    list.items = labels
    if (current == null) {
      list.selected = 0
    } else if (rows[0] is List) {
      list.selected = rowIndex(rows, current)
    } else {
      list.selected = current
    }
    list.onSelect = Fn.new {|i, item|
      result = rows[i] is List ? rows[i][0] : i
      app.popModal()
    }
    pane.add(list)
    app.modal(pane)
    return result
  }

  static prompt(app, title, message, initial, maxLen, masked) {
    var p = Prompt.new(message, initial)
    p.title = title
    p.input.maxLen = maxLen
    if (masked) p.input.mask = "*"
    p.bounds = Popup.centeredBounds_(message, 2, 34)
    app.modal(p)
    return p.result
  }

  static integer(app, title, message, initial, minimum, maximum) {
    while (true) {
      var value = prompt(app, title, message, initial.toString,
          12, false)
      if (value == null) return null
      var number = Num.fromString(value)
      if (number != null && number.isInteger &&
          number >= minimum && number <= maximum) return number
      Alert.show(app, title,
          "Enter an integer from %(minimum) through %(maximum).")
    }
  }

  static text(app, title, message) {
    var p = Alert.new(title, message)
    p.preformatted = true
    p.bounds = Popup.centeredBounds_(message, 1, 34, true)
    app.modal(p)
  }
}
