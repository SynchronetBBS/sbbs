// Connected-session entry point for the shared Wren console.

import "syncterm" for Hook, Key
import "wren_console" for WrenConsole

Hook.onKey(Key.wrenConsole) {|key|
  WrenConsole.run()
  return true
}
