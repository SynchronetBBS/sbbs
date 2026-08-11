// SyncTERM host-popup bridge.
//
// C-owned terminal paths use this auto-loaded module to show Wren UI
// popups without knowing the widget-library details.

import "syncterm" for Screen
import "ui" for App, Alert, Popup

class HostPopup {
  static alert(title, message) {
    var saved = Screen.save()
    var app = App.new()
    var p = Alert.new(title, message)
    p.bounds = Popup.centeredBounds_(message, 1, 20)
    p.onDismiss = Fn.new { |v| app.quit() }
    app.pushModal(p)
    app.runSync()
    Screen.restore(saved)
  }
}
