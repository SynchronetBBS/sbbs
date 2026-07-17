import "syncterm" for Key, Screen
import "syncterm_menu" for Menu, MenuReadStatus
import "menu_ui" for MenuUi
import "menu_bbs_editor" for BbsEditor
import "menu_settings_ui" for SettingsMenu
import "ui_app" for App
import "ui_widget" for Widget, Rect
import "ui_draw" for Painter
import "ui_pane" for Pane
import "ui_list" for ListView
import "ui_menubar" for MenuBar
import "ui_statusbar" for StatusBar
import "ui_popup" for Alert, Confirm

class EntrySummary is Widget {
  construct new() {
    super()
    focusable = false
    _entry = null
  }

  entry=(bbs) {
    _entry = bbs
    markDirty()
  }

  cleanLine_(text) {
    var out = ""
    for (ch in text) {
      if (ch == "\r" || ch == "\n") return out
      out = out + ch
    }
    return out
  }

  onPaint_() {
    var sf = surface
    var style = style("default")
    Painter.fill(sf, Rect.new(0, 0, bounds.w, bounds.h), " ", style)
    if (_entry == null) {
      Painter.text(sf, 0, 0, "No directory entries", style, bounds.w)
      return
    }
    var lines = [
      _entry.name,
      _entry.addr,
      "%(_entry.connTypeName)  Port %(_entry.port)",
      "Calls: %(_entry.calls)",
      _entry.type == 0 ? "Personal directory" : "Read-only directory",
      "",
      cleanLine_(_entry.comment)
    ]
    for (i in 0...lines.count.min(bounds.h)) {
      Painter.text(sf, 0, i, lines[i], style, bounds.w)
    }
  }
}

class MainMenuApp {
  construct new(current, connected) {
    _app = App.new()
    _connected = connected
    _current = current
    _result = null
    _entries = []
    _selected = null
    _clipboard = null

    _bar = MenuBar.new()
    _bar.items = [
      ["Directory", Fn.new { _app.root.focusedIndex = 1 }],
      ["Edit", Fn.new { edit_() }],
      ["New", Fn.new { add_() }],
      ["Delete", Fn.new { delete_() }],
      ["Settings", Fn.new { settings_() }],
      [_connected ? "Close" : "Quick", Fn.new {
        if (_connected) exit_() else quick_()
      }]
    ]
    _app.root.add(_bar)

    _directory = Pane.new()
    _directory.title = "SyncTERM Directory"
    _directory.focused = true
    _directory.helpable = false
    _app.root.add(_directory)

    _list = ListView.new()
    _list.onChange = Fn.new {|index, item|
      _selected = index >= 0 && index < _entries.count ? _entries[index] : null
      _summary.entry = _selected
      updateStatus_()
    }
    _list.onSelect = Fn.new {|index, item|
      if (_connected) edit_() else connect_()
    }
    _directory.add(_list)

    _details = Pane.new()
    _details.title = "Entry"
    _details.focusable = false
    _app.root.add(_details)
    _summary = EntrySummary.new()
    _details.add(_summary)

    _status = StatusBar.new()
    _app.root.add(_status)

    _app.onLayout = Fn.new {|width, height| layout_(width, height) }
    bind_(Key.escape, Fn.new { exit_() })
    bind_(Key.ctrlE, Fn.new { edit_() })
    bind_(Key.insert, Fn.new { add_() })
    bind_(Key.delete, Fn.new { delete_() })
    bind_(Key.f5, Fn.new { copy_() })
    bind_(Key.f6, Fn.new { paste_() })
    if (!_connected) bind_(Key.ctrlD, Fn.new { quick_() })
    _app.root.focusedIndex = 1
    refresh_(current)
  }

  bind_(key, fn) {
    _app.bind(key, Fn.new {|event|
      if (_app.modalStack.count == 0) fn.call()
    })
  }

  run() {
    _app.runSync()
    return _result
  }

  layout_(width, height) {
    _bar.bounds = Rect.new(1, 1, width, 1)
    _status.bounds = Rect.new(1, height, width, 1)
    var bodyH = (height - 2).max(1)
    if (width < 68) {
      _directory.bounds = Rect.new(1, 2, width, bodyH)
      _details.visible = false
    } else {
      var leftW = ((width * 2) / 3).floor.max(36)
      _directory.bounds = Rect.new(1, 2, leftW, bodyH)
      _details.visible = true
      _details.bounds = Rect.new(leftW + 1, 2, width - leftW, bodyH)
      _summary.bounds = _details.innerBounds
    }
    _list.bounds = _directory.innerBounds
  }

  refresh_(preferred) {
    _entries = Menu.entries
    var labels = []
    var selected = 0
    for (i in 0..._entries.count) {
      var bbs = _entries[i]
      labels.add(bbs.type == 0 ? bbs.name : "%(bbs.name)  [read-only]")
      if (preferred != null && bbs.name == preferred) selected = i
    }
    _list.items = labels
    if (labels.count > 0) _list.selected = selected.min(labels.count - 1)
    if (labels.count == 0) {
      _selected = null
      _summary.entry = null
      updateStatus_()
    }
  }

  updateStatus_() {
    if (_selected == null) {
      _status.segments = [["Directory is empty", "left"]]
    } else {
      var origin = _selected.type == 0 ? "Personal" : "Read-only"
      _status.segments = [
        [origin, "left"],
        [_selected.connTypeName, "center"],
        ["%(_selected.calls) calls", "right"]
      ]
    }
  }

  connect_() {
    if (_selected == null) return
    _result = _selected
    _app.quit()
  }

  edit_() {
    if (_selected == null) return
    var bbs = _selected
    if (bbs.type != 0) {
      if (!Confirm.show(_app, "Copy this read-only entry to the personal directory?")) return
      var name = MenuUi.prompt(_app, "Copy Entry", "Personal entry name",
          bbs.name, 30, false)
      if (name == null) return
      bbs = Menu.copy(bbs, name)
      if (bbs == null) {
        Alert.show(_app, "Copy Failed", "The personal entry name is invalid or already in use.")
        return
      }
      BbsEditor.edit(_app, bbs, false, true)
      refresh_(name)
      return
    }
    var name = bbs.name
    BbsEditor.edit(_app, bbs, false, false)
    refresh_(bbs.name)
  }

  add_() {
    var name = MenuUi.prompt(_app, "New Entry", "Entry name", "", 30, false)
    if (name == null) return
    var bbs = Menu.create(name)
    if (bbs == null) {
      Alert.show(_app, "New Entry", "The entry name is invalid or already in use.")
      return
    }
    BbsEditor.edit(_app, bbs, false, true)
    refresh_(name)
  }

  delete_() {
    if (_selected == null) return
    if (_selected.type != 0) {
      Alert.show(_app, "Delete Entry", "Read-only directory entries cannot be deleted.")
      return
    }
    var name = _selected.name
    if (!Confirm.show(_app, "Delete %(name)?")) return
    if (!_selected.delete()) {
      Alert.show(_app, "Delete Entry", "The directory entry could not be deleted.")
      return
    }
    _clipboard = null
    refresh_(null)
  }

  copy_() {
    if (_selected == null) return
    _clipboard = BbsEditor.draft_(_selected)
    _status.segments = [["Copied %(_selected.name)", "left"]]
  }

  paste_() {
    if (_clipboard == null) return
    var name = MenuUi.prompt(_app, "Paste Entry", "New personal entry name",
        _clipboard["name"], 30, false)
    if (name == null) return
    var bbs = Menu.create(name)
    if (bbs == null) {
      Alert.show(_app, "Paste Entry", "The entry name is invalid or already in use.")
      return
    }
    var draft = {}
    for (key in _clipboard) draft[key] = _clipboard[key]
    draft["palette"] = _clipboard["palette"].toList
    draft["name"] = name
    if (!BbsEditor.apply_(_app, bbs, draft, false)) bbs.delete()
    refresh_(name)
  }

  quick_() {
    var url = MenuUi.prompt(_app, "Quick Connect", "Address or URL", "", 1024, false)
    if (url == null) return
    var bbs = Menu.quickConnect(url)
    if (bbs == null) {
      Alert.show(_app, "Quick Connect", "The address could not be parsed.")
      return
    }
    _result = bbs
    _app.quit()
  }

  settings_() {
    var preferred = _selected == null ? null : _selected.name
    var changed = SettingsMenu.run(_app, _connected)
    if (changed) {
      if (SettingsMenu.passwordChanged) MainMenu.password = SettingsMenu.directoryPassword
      var status = Menu.load(MainMenu.password)
      if (status == MenuReadStatus.ok) refresh_(preferred)
    }
  }

  exit_() {
    if (_connected) {
      _app.quit()
      return
    }
    var settings = Menu.settings
    if (!settings.confirmClose || Confirm.show(_app, "Exit SyncTERM?")) _app.quit()
  }
}

class MainMenu {
  static password { __password }
  static password=(value) { __password = value }

  static run(current, connected) {
    var status = Menu.load(__password)
    while (status == MenuReadStatus.passwordRequired ||
        status == MenuReadStatus.decryptFailed) {
      var message = "Incorrect password.  Try again"
      if (status == MenuReadStatus.passwordRequired) {
        message = "Password for the personal directory"
      }
      var password = MenuUi.promptStandalone("Directory Password",
          message, "", 1023, true)
      if (password == null) return null
      status = Menu.load(password)
      if (status == MenuReadStatus.ok) __password = password
    }
    if (status != MenuReadStatus.ok) {
      MenuUi.alertStandalone("Directory Error", Menu.statusMessage(status))
      return null
    }
    return MainMenuApp.new(current, connected).run()
  }
}
