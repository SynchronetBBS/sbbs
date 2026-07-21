import "syncterm" for CustomCursor, Screen
import "menu_ui" for MenuUi
import "ui_app" for App
import "ui_widget" for Rect
import "ui_pane" for Pane
import "ui_progress" for ProgressText

class MenuHostUI {
  static statusWidth_(screenWidth) {
    return 74.min((screenWidth - 4).max(1))
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
    // Match UIFC's 74-column progress window.  Keeping the width stable
    // preserves the C formatter's 20-cell name column between rows and
    // across updates; narrow screens still retain the standard margins.
    var w = statusWidth_(size[0])
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
