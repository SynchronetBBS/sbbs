import "syncterm" for CustomCursor, Screen
import "menu_ui" for MenuUi
import "ui_app" for App
import "ui_widget" for Widget, Rect
import "ui_draw" for Painter
import "ui_pane" for Pane
import "ui_popup" for Popup

class HostProgressBody is Widget {
  construct new() {
    super()
    focusable = false
    _lines = []
  }

  lines=(value) {
    _lines = value
    markDirty()
  }

  wrappedLines_(width) {
    var wrapped = []
    var textWidth = (width - 2).max(1)
    for (line in _lines) {
      for (part in Popup.wrap_(line, textWidth)) wrapped.add(part)
    }
    return wrapped
  }

  rowCount(width) { wrappedLines_(width).count }

  onPaint_() {
    var sf = surface
    var st = style("default")
    Painter.fill(sf, Rect.new(0, 0, bounds.w, bounds.h), " ", st)
    var lines = wrappedLines_(bounds.w)
    var count = lines.count.min(bounds.h)
    var row = ((bounds.h - count) / 2).floor
    for (i in 0...count) {
      var line = lines[i]
      var col = ((bounds.w - line.count) / 2).floor
      Painter.text(sf, col, row + i, line, st)
    }
  }
}

class MenuHostUI {
  static alert(title, message) {
    MenuUi.alertStandalone(title, message)
  }

  static confirm(title, message) {
    return MenuUi.confirmStandalone(title, message)
  }

  static prompt(title, message, initial, maxLen, masked) {
    return MenuUi.promptStandalone(title, message, initial, maxLen, masked)
  }

  static choice(title, message, options, current) {
    return MenuUi.choiceStandalone(title, message, options, current)
  }

  static status(title, lines) {
    if (__statusApp == null) {
      __statusCursor = CustomCursor.current
      __statusApp = App.new()
      __statusPane = Pane.new()
      __statusPane.focusable = false
      __statusPane.focused = true
      __statusPane.helpable = false
      __statusPane.closeable = false
      __statusPane.shadow = true
      __statusBody = HostProgressBody.new()
      __statusPane.add(__statusBody)
      __statusApp.root.add(__statusPane)
    }
    __statusPane.title = title
    __statusBody.lines = lines
    var size = Screen.size
    var longest = title.count + 6
    for (line in lines) {
      if (line.count + 4 > longest) longest = line.count + 4
    }
    var w = longest.max(24).min(size[0] - 4)
    var rows = __statusBody.rowCount((w - 2).max(1))
    var h = (rows + 4).max(5).min(size[1] - 4)
    __statusPane.bounds = Rect.new(((size[0] - w) / 2).floor + 1,
        ((size[1] - h) / 2).floor + 1, w, h)
    __statusBody.bounds = __statusPane.innerBounds
    __statusApp.drawAll_()
  }

  static statusClear() {
    if (__statusCursor != null) __statusCursor.apply()
    __statusCursor = null
    __statusBody = null
    __statusPane = null
    __statusApp = null
  }
}
