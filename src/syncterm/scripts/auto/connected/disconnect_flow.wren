// disconnect_flow.wren — modal "Disconnect... Are you sure?" Confirm
// flow shared by the Alt-X / Alt-H / Ctrl-Q hooks in
// keys_default.wren and the SM_DISCONNECT / SM_EXIT entries in
// online_menu.wren.  On a Yes, calls Conn.endSession to flag cleanup;
// doterm() picks the flag up at the top of its next iteration and
// either returns to the main menu (`exitApp == false`) or exits
// SyncTERM entirely (`exitApp == true`).

import "syncterm" for Conn, Input, Screen
import "ui_app"   for App
import "ui_popup" for Confirm

class DisconnectFlow {
  static run(exitApp) {
    Fiber.new {
      Screen.modalRun(Fn.new {
        var app = App.new()
        var msg = "Disconnect... Are you sure?\n\n" +
                  "Selecting Yes closes the connection."
        if (Confirm.runStandalone(app, msg)) Conn.endSession(exitApp)
      })
      Input.setupMouseEvents()
    }.call()
  }
}
