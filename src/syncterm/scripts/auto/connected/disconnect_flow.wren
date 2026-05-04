// disconnect_flow.wren — modal "Disconnect... Are you sure?" Confirm
// flow shared by the Alt-X / Alt-H / Ctrl-Q / window-close hooks in
// keys_default.wren and the SM_DISCONNECT / SM_EXIT entries in
// online_menu.wren.  On a Yes, calls Conn.endSession to flag cleanup;
// doterm() picks the flag up at the top of its next iteration and
// either returns to the bbslist (`exitApp == false`) or exits
// SyncTERM entirely (`exitApp == true`).

import "syncterm" for Conn, Input, Screen
import "ui_app"   for App
import "ui_popup" for Confirm, Popup

class DisconnectFlow {
  static run(exitApp) {
    Fiber.new {
      Screen.modalRun(Fn.new {
        var app = App.new()
        var msg = "Disconnect... Are you sure?\n\n" +
                  "Selecting Yes closes the connection."
        var c   = Confirm.new(msg)
        c.bounds     = Popup.centeredBounds_(msg, 1, 24)
        // The Confirm dismissal pops itself off the modal stack;
        // also quit the App so `app.run()` returns.  Without this
        // wire-up run() would loop forever on an empty modal stack.
        c.onDismiss  = Fn.new { |v| app.quit() }
        app.pushModal(c)
        app.run()
        if (c.result == true) Conn.endSession(exitApp)
      })
      Input.setupMouseEvents()
    }.call()
  }
}
