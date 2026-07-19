// Reserved picker-VM recovery surface.  The host interprets the embedded
// copy before loading user picker scripts so an implementation error still
// leaves the picker-local console reachable.

import "syncterm" for Host, Input, Key, KeyEvent, REPL, Screen
import "wren_console" for WrenConsole

class PickerBootstrap {
  static console() { WrenConsole.run("file_picker") }

  static run(picker, request) {
    var fiber = Fiber.new { picker.run(request) }
    fiber.try()
    if (fiber.error == null) return
    System.print("File picker failed: %(fiber.error)")
    REPL.printTrace_(fiber)
    recover(request, fiber.error)
  }

  // Returns true only for the application-close key.  Escape cancels the
  // failed picker call while keeping SyncTERM alive.
  static recover(request, message) {
    var oldBounds = Screen.window.bounds
    var size = Screen.size
    Screen.window.bounds = [1, 1, size[0], size[1]]
    Screen.window.clear()
    Screen.window.position = [3, 2]
    Screen.window.print("File picker error")
    Screen.window.position = [3, 4]
    Screen.window.print(message)
    Screen.window.position = [3, 6]
    Screen.window.print("Ctrl+` opens the picker console.  Escape cancels.")

    while (true) {
      if (Host.logUnread) {
        Screen.window.position = [size[0], 1]
        Screen.window.print("\u203c")
      }
      var event = Input.next()
      if (!(event is KeyEvent)) continue
      if (event.code == Key.wrenConsole) {
        console()
        Screen.window.clear()
        Screen.window.position = [3, 2]
        Screen.window.print("File picker error")
        Screen.window.position = [3, 4]
        Screen.window.print(message)
        Screen.window.position = [3, 6]
        Screen.window.print("Ctrl+` opens the picker console.  Escape cancels.")
      } else if (event.code == Key.escape) {
        request.cancel()
        Screen.window.bounds = oldBounds
        return
      } else if (event.code == Key.quit) {
        request.quitApplication()
        Screen.window.bounds = oldBounds
        return
      }
    }
  }
}
