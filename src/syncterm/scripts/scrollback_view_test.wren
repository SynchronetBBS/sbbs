import "syncterm" for CTerm, Screen, Surface, Scrollback, Input, KeyEvent, Key, Host
import "scrollback_view" for ScrollbackView

class ScrollbackViewTest {
  static check_(ok, label) {
    if (ok) {
      __pass = __pass + 1
    } else {
      __fail = __fail + 1
      Host.print("  FAIL %(label)")
    }
  }

  // Run the real pan loop without modal restoration so its rendered
  // rows can be inspected after the queued Escape returns control.
  static paint_(live, height) {
    ScrollbackView.blankSurroundings_(true)
    Input.unget(KeyEvent.new(Key.escape))
    ScrollbackView.viewLoop_(Surface.new(live.width, height), live,
        Scrollback.height)
    return Screen.readRect(1, 1, live.width, CTerm.height)
  }

  static run() {
    __pass = 0
    __fail = 0
    Screen.modalRun(Fn.new {
      var w = CTerm.width
      var h = CTerm.height
      // Ensure the full-height disconnected view can pull an older row
      // back out of history, as prepare_disconnect_notice_row does.
      for (i in 0...h) CTerm.write("history\r\n")
      var historyH = Scrollback.height
      var source = Surface.new(w, h)
      for (row in source.rows) for (cell in row) cell.ch = "A"
      for (cell in source.rows[h - 2]) cell.ch = "Z"
      for (cell in source.rows[h - 1]) cell.ch = "!"
      var live = Surface.new(w, h - 1)
      live.putRect(source, 0, 0)
      var latestHistory = Surface.new(w, 1)
      Scrollback.copyRowsTo(latestHistory, historyH - 1, 1, 0)
      var savedMouse = Input.mouseEvents
      var savedSuspension = CTerm.suspended
      var savedPos = CTerm.scrollbackPos
      var savedStart = CTerm.scrollbackStart

      var bar = paint_(live, h - 1)
      check_(bar.cellAt(15, 0).ch == "A" &&
          bar.cellAt(0, h - 2).ch == "Z",
          "enabled status bar shows the newest live rows")
      check_(bar.cellAt(0, h - 1).ch == " " &&
          (bar.cellAt(0, h - 1).legacyAttr & 0x70) == 0x40,
          "disconnected status row is blank red")

      var full = paint_(live, h)
      check_(full.cellAt(15, 0).eqContent(latestHistory.cellAt(15, 0)),
          "hidden status bar starts one history row earlier")
      check_(full.cellAt(0, h - 1).ch == "Z",
          "hidden status bar allows content on the bottom row")

      var connected = paint_(source, h)
      check_(connected.cellAt(0, h - 1).ch == "!",
          "connected scrollback retains the complete live screen")

      // Exercise both disconnected entry points and their modal cleanup.
      Screen.putRect(source, CTerm.originX, CTerm.originY)
      for (hasStatus in [true, false]) {
        Input.unget(KeyEvent.new(Key.escape))
        ScrollbackView.runDisconnected(hasStatus)
      }
      var restored = Screen.readRect(CTerm.originX, CTerm.originY,
          CTerm.originX + w - 1, CTerm.originY + h - 1)
      check_(restored.cellAt(0, h - 1).ch == "!" &&
          Input.mouseEvents == savedMouse && CTerm.suspended == savedSuspension,
          "scrollback restores the notice, mouse mask and suspension")
      check_(CTerm.scrollbackPos == savedPos &&
          CTerm.scrollbackStart == savedStart && Scrollback.height == historyH,
          "browsing disconnected scrollback leaves history unchanged")
    })
    return [__pass, __fail]
  }
}
