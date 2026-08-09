import "syncterm" for CustomCursor, Screen
import "menu_ui" for MenuUi
import "ui_app" for App
import "ui_pane" for Pane
import "ui_progress" for ProgressText

class MenuHostUI {
  static statusWidth_(screenWidth, title, lines) {
    var longest = title == null ? 0 : title.count
    for (line in lines) {
      if (line.count > longest) longest = line.count
    }
    return (longest + 4).min((screenWidth - 4).max(1))
  }

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
      __statusPane.frameKind = "display"
      __statusPane.focusable = false
      __statusPane.focused = true
      __statusPane.helpable = false
      __statusPane.closeable = false
      __statusPane.shadow = true
      __statusBody = ProgressText.new()
      __statusPane.add(__statusBody)
      __statusApp.root.add(__statusPane)
    }
    __statusPane.title = title
    __statusBody.lines = lines
    var size = Screen.size
    var w = statusWidth_(size[0], title, lines)
    var rows = __statusBody.rowCount((w - 2).max(1))
    var h = (rows + 4).max(5).min(size[1] - 4)
    __statusPane.bounds = Pane.modalBounds(w, h)
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
