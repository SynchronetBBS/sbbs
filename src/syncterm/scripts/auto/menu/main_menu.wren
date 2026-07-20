import "syncterm" for Host, KeyEvent, MouseEvent, Key, Mouse, REPL, Screen
import "syncterm_menu" for Menu, MenuReadStatus
import "wren_console" for WrenConsole
import "menu_ui" for MenuUi
import "menu_bbs_editor" for BbsEditor
import "menu_settings_ui" for SettingsMenu
import "menu_scrollback" for OfflineScrollbackView
import "menu_sort_profiles" for SortProfiles
import "ui_app" for App
import "ui_widget" for Widget, Container, Rect
import "ui_input" for SelectOnFocusInput
import "ui_draw" for Painter
import "ui_pane" for Pane
import "ui_list" for ListView
import "ui_popup" for Alert, Confirm

class ClassicBackdrop is Widget {
  construct new() {
    super()
    focusable = false
    activitySensitive = false
  }

  onPaint_() {
    var sf = surface
    Painter.fill(sf, Rect.new(0, 0, bounds.w, bounds.h), "\u2591",
        style("classic.backdrop"))
    Painter.fill(sf, Rect.new(0, 0, bounds.w, 1), " ",
        style("classic.header"))
    Painter.text(sf, 2, 0, Menu.applicationTitle, style("classic.header"),
        (bounds.w - 4).max(0))
    if (bounds.w >= 80) {
      Painter.text(sf, bounds.w - 26, 0, Menu.timeText,
          style("classic.header"), 24)
    }
  }
}

class ConsoleIndicator is Widget {
  construct new() {
    super()
    focusable = false
    activitySensitive = false
    _error = false
    refresh()
  }

  refresh() {
    _error = Host.logUnreadError
    visible = Host.logUnread
    markDirty()
  }

  onPaint_() {
    Painter.text(surface, 0, 0, "\u203c",
        style(_error ? "classic.console.error" : "classic.console"), 1)
  }
}

class CommentInput is SelectOnFocusInput {
  construct new(onFocus) {
    super()
    _onFocus = onFocus
  }

  onPaint_() {
    if (parent.focused) {
      super.onPaint_()
      return
    }
    var sf = surface
    Painter.fill(sf, Rect.new(0, 0, bounds.w, bounds.h), " ",
        style("classic.comment"))
    var offset = 0
    if (value.count < bounds.w) {
      offset = ((bounds.w - value.count) / 2).floor
    }
    Painter.text(sf, offset, 0, value, style("classic.comment"),
        bounds.w - offset)
  }

  handle(event) {
    if (event is MouseEvent && event.event == Mouse.button1Click) {
      var active = parent != null && parent.focused
      if (!active) _onFocus.call()
      if (!active) return true
    }
    if (event is MouseEvent && parent != null && !parent.focused &&
        (event.event == Mouse.button2Click ||
         event.event == Mouse.button3Click)) return false
    return super.handle(event)
  }
}

class ClassicFooter is Container {
  construct new(onFocus, onCommit, onCancel) {
    super()
    activitySensitive = false
    _comment = ""
    _copied = false
    _directoryHints = true
    _onCommit = onCommit
    _onCancel = onCancel
    _input = CommentInput.new(onFocus)
    _input.maxLen = 1023
    _input.onSubmit = Fn.new {|value| _onCommit.call(value, 0) }
    add(_input)
  }

  gatesActiveLayer { true }

  comment=(value) {
    _comment = value == null ? "" : value
    _input.value = _comment
    markDirty()
  }

  value { _input.value }

  selectComment() { _input.selectAll() }

  copied=(value) {
    _copied = value
    markDirty()
  }

  directoryHints=(value) {
    if (_directoryHints == value) return
    _directoryHints = value
    markDirty()
  }

  paintHints_(sf) {
    var segments = [
      ["F1 ", true], ["Help  ", false]
    ]
    if (_directoryHints) {
      segments.add(["F2 ", true])
      segments.add(["Edit Item  ", false])
      segments.add(["F5 ", true])
      segments.add(["Copy  ", false])
      if (_copied) {
        segments.add(["F6 ", true])
        segments.add(["Paste  ", false])
      }
      segments.add(["INS", true])
      segments.add(["ert Item  ", false])
      segments.add(["DEL", true])
      segments.add(["ete Item  ", false])
    }
    segments.add(["ESC ", true])
    segments.add(["Exit", false])

    var x = 4
    for (segment in segments) {
      if (x >= bounds.w) return
      var text = segment[0]
      Painter.text(sf, x, 1, text,
          segment[1] ? style("classic.hotkey") : style("classic.hint"),
          bounds.w - x)
      x = x + text.count
    }
  }

  onPaint_() {
    var sf = surface
    Painter.fill(sf, Rect.new(0, 0, bounds.w, 1), " ",
        focused ? style("default") : style("classic.comment"))
    if (bounds.h < 2) {
      compositeChildren_()
      return
    }
    Painter.fill(sf, Rect.new(0, 1, bounds.w, bounds.h - 1), " ",
        style("classic.hint"))

    paintHints_(sf)
    compositeChildren_()
  }

  handle(event) {
    if (event is KeyEvent && event.code == Key.tab) {
      _onCommit.call(_input.value, 1)
      return true
    }
    if (event is KeyEvent && event.code == Key.backTab) {
      _onCommit.call(_input.value, -1)
      return true
    }
    if (event is KeyEvent && event.code == Key.escape) {
      _input.value = _comment
      _onCancel.call()
      return true
    }
    return super.handle(event)
  }

  bounds=(value) {
    super.bounds = value
    _input.bounds = Rect.new(value.x + 2, value.y,
        (value.w - 4).max(1), 1)
  }
}

class MainPane is Pane {
  construct new(onFocus) {
    super()
    _onFocus = onFocus
  }

  handle(event) {
    if (event is MouseEvent && bounds != null &&
        bounds.contains(event.startX, event.startY) &&
        event.event == Mouse.button1Click) {
      var active = focused
      var activate = _onFocus.call()
      if (!active && activate != true) return true
    }
    return super.handle(event)
  }
}

class MainList is ListView {
  construct new(onFocus, onCycle, onComment, commentKey, onSwitchPane,
      onInsert, onDelete) {
    super()
    _onFocus = onFocus
    _onCycle = onCycle
    _onComment = onComment
    _commentKey = commentKey
    _onSwitchPane = onSwitchPane
    _onInsert = onInsert
    _onDelete = onDelete
    _onInactiveWheel = null
  }

  onInactiveWheel=(fn) { _onInactiveWheel = fn }

  handle(event) {
    if (event is MouseEvent && bounds != null &&
        bounds.contains(event.startX, event.startY) &&
        event.event == Mouse.button1Click) {
      var active = parent != null && parent.focused
      var activate = _onFocus.call()
      if (!active && activate != true) return true
    }
    if (event is MouseEvent && parent != null && !parent.focused &&
        (event.event == Mouse.wheelUpPress ||
         event.event == Mouse.wheelUpClick ||
         event.event == Mouse.wheelDownPress ||
         event.event == Mouse.wheelDownClick)) {
      if (_onInactiveWheel != null) _onInactiveWheel.call(event)
      return true
    }
    if (_onComment != null && event is KeyEvent &&
        event.code == _commentKey) {
      _onComment.call()
      return true
    }
    if (_onSwitchPane != null && event is KeyEvent &&
        (event.code == Key.left || event.code == Key.right)) {
      _onSwitchPane.call()
      return true
    }
    if (_onCycle != null && event is KeyEvent &&
        event.codepoint == 0x3C) {
      _onCycle.call(-1)
      return true
    }
    if (_onCycle != null && event is KeyEvent &&
        event.codepoint == 0x3E) {
      _onCycle.call(1)
      return true
    }
    if (event is KeyEvent && event.codepoint == 0x2B) {
      if (_onInsert != null) _onInsert.call()
      return true
    }
    if (event is KeyEvent && event.codepoint == 0x2D) {
      if (_onDelete != null) _onDelete.call()
      return true
    }
    return super.handle(event)
  }
}

class MainMenuEventApp is App {
  construct new(afterDispatch) {
    super()
    _afterDispatch = afterDispatch
  }

  dispatchSync_(event) {
    var result = super.dispatchSync_(event)
    _afterDispatch.call()
    return result
  }
}

class MainMenuApp {
  construct new(current, connected) {
    _app = MainMenuEventApp.new(Fn.new { _consoleIndicator.refresh() })
    _connected = connected
    _current = current
    _result = null
    _entries = []
    _selected = null
    _clipboard = null
    _clipboardType = null
    _confirmingExit = false
    _commentReturn = null
    _settingRows = SettingsMenu.rows(_connected)

    _backdrop = ClassicBackdrop.new()
    _backdrop.visible = !_connected
    _app.root.add(_backdrop)

    _directory = MainPane.new(Fn.new { focusDirectory_() })
    _directory.helpText = directoryHelp_()
    _directory.onClose = Fn.new { exit_(false) }
    _app.root.add(_directory)

    _list = MainList.new(Fn.new { focusDirectory_() },
        Fn.new {|delta| cycleSort_(delta) },
        Fn.new { focusComment_(1) }, Key.tab,
        Fn.new { focusSettings_() }, Fn.new { add_() },
        Fn.new { delete_() })
    _list.onChange = Fn.new {|index, item|
      _selected = index != null && index >= 0 &&
          index < _entries.count ? _entries[index] : null
      updateWindowTitle_()
      _footer.comment = _selected == null ? "" : _selected.comment
    }
    _list.onSelect = Fn.new {|index, item|
      if (index == _entries.count) {
        add_()
      } else if (_connected) {
        edit_()
      } else {
        connect_()
      }
    }
    _directory.add(_list)

    _footer = ClassicFooter.new(Fn.new { focusComment_(0) },
        Fn.new {|value, destination| finishComment_(value, destination) },
        Fn.new { cancelComment_() })
    _footer.helpText = directoryHelp_()
    _app.root.add(_footer)

    _settings = MainPane.new(Fn.new { focusSettings_() })
    _settings.title = "SyncTERM Settings"
    _settings.helpText = SettingsMenu.helpText(_connected)
    _settings.onClose = Fn.new { exit_(false) }
    _app.root.add(_settings)

    _settingsList = MainList.new(Fn.new { focusSettings_() }, null,
        Fn.new { focusComment_(-1) }, Key.backTab,
        Fn.new { focusDirectory_() }, null, null)
    var settingLabels = []
    for (row in _settingRows) settingLabels.add(row[1])
    _settingsList.items = settingLabels
    _settingsList.onSelect = Fn.new {|index, item|
      settingsAction_(_settingRows[index][0])
    }
    _settings.add(_settingsList)

    _consoleIndicator = ConsoleIndicator.new()
    _app.root.add(_consoleIndicator)
    _app.onError = Fn.new {|fiber|
      System.print("menu UI error: " + fiber.error)
      REPL.printTrace_(fiber)
      _consoleIndicator.refresh()
    }

    var inactiveWheel = Fn.new {|event| routeWheel_(event) }
    _list.onInactiveWheel = inactiveWheel
    _settingsList.onInactiveWheel = inactiveWheel

    _app.onLayout = Fn.new {|width, height| layout_(width, height) }
    _app.onUnhandledMouse = Fn.new {|event, hit|
      backgroundMouse_(event, hit)
    }
    bind_(Key.escape, Fn.new { exit_(false) })
    bind_(Key.wrenConsole, Fn.new { console_() })
    _app.bind(Key.quit, Fn.new {|event| exit_(true) })
    bindDirectory_(Key.f2, Fn.new {
      if (_selected != null) edit_()
    })
    bindDirectory_(Key.ctrlE, Fn.new { edit_() })
    bindDirectory_(Key.insert, Fn.new { add_() })
    bindDirectory_(Key.delete, Fn.new { delete_() })
    bindDirectory_(Key.delChar, Fn.new { delete_() })
    bindDirectory_(Key.ctrlS, Fn.new { sort_() })
    bindDirectory_(Key.f5, Fn.new { copy_() })
    bindDirectory_(Key.ctrlIns, Fn.new { copy_() })
    bindDirectory_(Key.f6, Fn.new { paste_() })
    bindDirectory_(Key.shiftIns, Fn.new { paste_() })
    if (!_connected) bindDirectory_(Key.ctrlD, Fn.new { quick_() })
    if (!_connected) {
      bind_(Key.altB, Fn.new { OfflineScrollbackView.show(_app) })
    }
    _app.root.focusedIndex = 1
    refresh_(current)
  }

  bind_(key, fn) {
    _app.bind(key, Fn.new {|event|
      if (_app.modalStack.count == 0) fn.call()
    })
  }

  bindDirectory_(key, fn) {
    bind_(key, Fn.new {
      if (_app.root.focusedChild == _directory) fn.call()
    })
  }

  run() {
    _app.runSync()
    return _result
  }

  console_() {
    WrenConsole.run("main_menu")
    _consoleIndicator.refresh()
  }

  directoryHelp_() {
    var action = "Connect to the selected entry"
    if (_connected) action = "Edit the selected entry"
    var text = "# SyncTERM Directory\n\n" +
        "## Commands\n\n" +
        "Enter\n:  %(action)\n" +
        "F2 or Ctrl-E\n:  Edit the selected entry\n" +
        "Insert or +\n:  Add a new personal entry\n" +
        "Blank final row\n:  Add a new personal entry\n" +
        "Delete or -\n:  Remove the selected personal entry\n" +
        "F5 / Ctrl-Insert\n:  Copy an entry\n" +
        "F6 / Shift-Insert\n:  Paste an entry\n" +
        "Ctrl-S\n:  Manage sort profiles\n" +
        "< / >\n:  Cycle through sort profiles\n" +
        "Tab\n:  Edit the selected comment, then move to settings"
    if (!_connected) {
      text = text + "\nCtrl-D\n:  Quick-connect to an address or URL\n" +
          "Alt-B\n:  View the scrollback from the last session"
    }
    return text + "\n\n" +
        "## Window Keys\n\n" +
        "Ctrl-`\n:  Open the Wren console\n" +
        "Alt-Left / Alt-Right\n:  Snap to the next smaller or larger width\n" +
        "Alt-Enter\n:  Toggle full-screen mode when available\n\n" +
        "## List Keys\n\n" +
        "Ctrl-F\n:  Find text in the current list\n" +
        "Ctrl-G\n:  Repeat the last search"
  }

  focusDirectory_() {
    if (_app.root.focusedChild == _footer) {
      var activate = _commentReturn == _directory
      finishComment_(_footer.value, -1)
      return activate
    }
    _footer.directoryHints = true
    _app.root.focusedIndex = 1
    updateWindowTitle_()
    return false
  }

  focusComment_(fallback) {
    if (_selected == null || Host.safeMode) {
      if (fallback == 0) return false
      if (fallback > 0) {
        focusSettings_()
      } else {
        focusDirectory_()
      }
      return false
    }
    _commentReturn = fallback == 0 ? _app.root.focusedChild : _directory
    _footer.comment = _selected.comment
    _footer.selectComment()
    _app.root.focusedIndex = 2
    return true
  }

  focusSettings_() {
    if (_app.root.focusedChild == _footer) {
      var activate = _commentReturn == _settings
      finishComment_(_footer.value, 1)
      return activate
    }
    _footer.directoryHints = false
    _app.root.focusedIndex = 3
    updateWindowTitle_()
    return false
  }

  cancelComment_() {
    if (_commentReturn == _settings) {
      _footer.directoryHints = false
      _app.root.focusedIndex = 3
    } else {
      _footer.directoryHints = true
      _app.root.focusedIndex = 1
    }
    _commentReturn = null
    updateWindowTitle_()
  }

  updateWindowTitle_() {
    var focused = _app.root.focusedChild
    if (focused == _footer) return
    if (focused == _settings) {
      Menu.showEntry(null)
    } else {
      Menu.showEntry(_selected)
    }
  }

  routeWheel_(event) {
    var up = event.event == Mouse.wheelUpPress ||
        event.event == Mouse.wheelUpClick
    var down = event.event == Mouse.wheelDownPress ||
        event.event == Mouse.wheelDownClick
    if (!up && !down) return false
    var focused = _app.root.focusedChild
    if (focused == _directory) {
      if (up) {
        _list.up()
      } else {
        _list.down()
      }
      return true
    }
    if (focused == _settings) {
      if (up) {
        _settingsList.up()
      } else {
        _settingsList.down()
      }
      return true
    }
    return true
  }

  backgroundMouse_(event, hit) {
    if (event.event == Mouse.wheelUpPress ||
        event.event == Mouse.wheelUpClick ||
        event.event == Mouse.wheelDownPress ||
        event.event == Mouse.wheelDownClick) return routeWheel_(event)
    if (event.event != Mouse.button1Click) return false
    if (hit != _app.root && hit != _backdrop) return false
    var focused = _app.root.focusedChild
    if (focused == _directory) {
      focusSettings_()
    } else if (focused == _settings) {
      focusDirectory_()
    } else if (_commentReturn == _directory) {
      finishComment_(_footer.value, 1)
    } else {
      finishComment_(_footer.value, -1)
    }
    return true
  }

  longest_(rows) {
    var length = 0
    for (row in rows) {
      var text = row is List ? row[1] : row
      if (text.bytes.count > length) length = text.bytes.count
    }
    return length
  }

  layout_(width, height) {
    _backdrop.bounds = Rect.new(1, 1, width, height)
    _consoleIndicator.bounds = Rect.new(width, 1, 1, 1)
    _footer.bounds = Rect.new(1, height - 1, width, 2)

    var settingW = (longest_(_settingRows) + 5).max(21)
    var settingH = (_settingRows.count + 4).min(height - 5)
    var titleW = _directory.title.bytes.count + 4
    var labels = []
    for (bbs in _entries) labels.add(bbs.name)
    var directoryW = (longest_(labels) + 5).max(titleW)
    var directoryH = (_entries.count + 5).min(height - 5).max(5)
    var leftMargin = width >= 80 ? 5 : 1
    var rightMargin = width >= 80 ? 4 : 0
    var stacked = width < settingW + 28 + leftMargin + rightMargin

    if (stacked) {
      var paneW = (width - 2).max(12)
      settingW = paneW
      directoryW = paneW
      settingH = settingH.min((height - 8).max(5))
      directoryH = directoryH.min((height - 3 - settingH).max(5))
      _directory.bounds = Rect.new(2, 2, directoryW, directoryH)
      _settings.bounds = Rect.new(2, 2 + directoryH, settingW, settingH)
      _directory.shadow = false
      _settings.shadow = false
    } else {
      settingW = settingW.min(width - leftMargin - rightMargin)
      var settingsX = width - rightMargin - settingW + 1
      directoryW = directoryW.min(settingsX - leftMargin)
      var directoryY = 3 + (((height - directoryH) / 2).floor - 4).max(0)
      var settingsY = (1 + ((height - settingH) / 2).floor).max(2)
      _directory.bounds = Rect.new(leftMargin, directoryY,
          directoryW, directoryH)
      _settings.bounds = Rect.new(settingsX, settingsY,
          settingW, settingH)
      _directory.shadow = width >= 80
      _settings.shadow = width >= 80
    }
    _list.bounds = _directory.innerBounds
    _settingsList.bounds = _settings.innerBounds
  }

  refresh_(preferred) { refresh_(preferred, 0) }

  refresh_(preferred, fallback) {
    _entries = Menu.entries
    var labels = []
    var selected = fallback.max(0)
    for (i in 0..._entries.count) {
      var bbs = _entries[i]
      labels.add(bbs.name)
      if (preferred != null && bbs.name == preferred) selected = i
    }
    if (Menu.canAppendEntry) labels.add("")
    _list.items = labels
    if (labels.count > 0) _list.selected = selected.min(labels.count - 1)
    if (_entries.count == 0) {
      _selected = null
      Menu.showEntry(null)
      _footer.comment = ""
    }
    var profiles = Menu.sortProfiles
    var active = Menu.activeSortProfile
    if (active >= 0 && active < profiles.count) {
      _directory.title = "Directory: %(profiles[active][0])"
    } else {
      _directory.title = "Directory (%(_entries.count) items)"
    }
    updateWindowTitle_()
    _app.root.markDirty()
  }

  connect_() {
    if (_selected == null) return
    _result = _selected
    _app.quit()
  }

  safeModeHelp_(operation) {
    if (operation == "quick") {
      return "# Cannot Quick-Connect in safe mode\n\n" +
          "SyncTERM is currently running in safe mode. This means you " +
          "cannot use the Quick-Connect feature."
    }
    var action = "edit the directory"
    if (operation == "add") action = "add to the directory"
    if (operation == "remove") action = "remove from the directory"
    return "# Cannot edit list in safe mode\n\n" +
        "SyncTERM is currently running in safe mode. This means you " +
        "cannot %(action)."
  }

  mutable_(operation) {
    if (!Host.safeMode) return true
    Alert.show(_app, "Safe Mode",
        "Cannot edit list in safe mode", safeModeHelp_(operation))
    return false
  }

  systemCopyHelp_() {
    return "# Copy from system directory\n\n" +
        "This entry was loaded from the system directory. In order to " +
        "edit it, it must be copied into your personal directory."
  }

  maxListHelp_() {
    return "# Max List size reached\n\n" +
        "The total combined size of loaded entries is currently the " +
        "highest supported size. You must delete entries before adding " +
        "more."
  }

  systemDeleteHelp_() {
    return "# Cannot delete from system list\n\n" +
        "This entry was loaded from the system-wide list and cannot be " +
        "deleted."
  }

  emptyDeleteHelp_() {
    return "# Calming down\n\n" +
        "## Some handy tips on calming down\n\n" +
        "Close your eyes, imagine yourself alone on a brilliant white " +
        "beach. Picture the palm trees up towards the small town. " +
        "Glory in the deep blue of the perfectly clean ocean. Feel the " +
        "plush comfort of your beach towel. Enjoy the shade of your " +
        "satellite internet feed which envelops your head, keeping you " +
        "cool. Set your TEMPEST rated laptop aside on the beach, knowing " +
        "it is completely impervious to anything on the beach. Reach " +
        "over to your fridge, grab a cold one. Watch the seagulls in " +
        "their dance."
  }

  editEntry_(first, isNew) {
    var current = first
    var currentIsNew = isNew
    var finalName = first.name
    var selected = 0
    while (current != null) {
      var result = BbsEditor.editNavigable(_app, current, false,
          currentIsNew, selected)
      if (!result[0]) return finalName
      selected = result[2]
      finalName = current.name
      Menu.sort()
      if (result[1] == 0) return finalName
      var entries = Menu.entries
      if (entries.count == 0) return finalName
      var index = 0
      for (i in 0...entries.count) {
        if (entries[i].name == finalName) index = i
      }
      index = ((index + result[1]) % entries.count + entries.count) % entries.count
      current = entries[index]
      currentIsNew = false
    }
    return finalName
  }

  edit_() {
    if (_selected == null) {
      if (_list.selected == _entries.count) add_()
      return
    }
    if (!mutable_("edit")) return
    var bbs = _selected
    if (bbs.type != 0) {
      if (!Confirm.show(_app, "Copy from system directory?",
          systemCopyHelp_())) return
      var name = bbs.name
      bbs = Menu.copy(bbs, name)
      if (bbs == null) {
        Alert.show(_app, "Copy Failed",
            "The personal entry could not be created.")
        return
      }
      name = editEntry_(bbs, true)
      refresh_(name)
      return
    }
    var name = editEntry_(bbs, false)
    refresh_(name)
  }

  finishComment_(value, destination) {
    var bbs = _selected
    if (bbs != null && value != bbs.comment) {
      if (bbs.type != 0) {
        if (Confirm.show(_app, "Copy from system directory?",
            systemCopyHelp_())) {
          bbs = Menu.copy(bbs, bbs.name)
        } else {
          bbs = null
        }
      }
      if (bbs == null) {
        _footer.comment = _selected.comment
      } else {
        var name = bbs.name
        bbs.comment = value
        if (!bbs.save()) {
          Alert.show(_app, "Entry Comment",
              "The comment could not be saved.")
        }
        refresh_(name)
      }
    }
    if (destination > 0 ||
        (destination == 0 && _commentReturn == _settings)) {
      _footer.directoryHints = false
      _app.root.focusedIndex = 3
    } else {
      _footer.directoryHints = true
      _app.root.focusedIndex = 1
    }
    _commentReturn = null
    updateWindowTitle_()
  }

  add_() {
    if (!Menu.canAppendEntry) {
      Alert.show(_app, "New Entry", "Max List size reached!",
          maxListHelp_())
      return
    }
    if (!mutable_("add")) return
    var preferred = _selected == null ? null : _selected.name
    var name = ""
    while (true) {
      name = MenuUi.prompt(_app, "New Entry", "Entry name", name, 30,
          false, entryNameHelp_())
      if (name == null || name.count == 0) return
      if (Menu.nameAvailable(name)) break
      BbsEditor.showNameError(_app, "New Entry", name)
    }
    var bbs = Menu.create(name)
    if (bbs == null) {
      Alert.show(_app, "New Entry",
          "The personal entry could not be created.")
      return
    }

    var type = BbsEditor.chooseConnectionType(_app, bbs.connType)
    if (type == null) {
      bbs.delete()
      refresh_(preferred)
      return
    }
    bbs.connType = type
    var port = Menu.defaultPort(type)
    if (port >= 1 && port <= 65535) bbs.port = port

    var label = addressLabel_(type)
    var address = MenuUi.prompt(_app, "New Entry", label, bbs.addr, 64,
        false, addAddressHelp_(type))
    if (address == null || address.count == 0) {
      bbs.delete()
      refresh_(preferred)
      return
    }
    if (addressCanContainPort_(type)) {
      var parsed = splitAddressPort_(address)
      address = parsed[0]
      if (parsed[1] != null) bbs.port = parsed[1]
    }
    bbs.addr = address
    if (!bbs.save()) {
      Alert.show(_app, "New Entry",
          "The directory entry could not be written.")
      bbs.delete()
      refresh_(preferred)
      return
    }
    Menu.sort()
    refresh_(name)
  }

  delete_() {
    if (_selected == null) {
      if (_list.selected == _entries.count && Menu.canAppendEntry) {
        Alert.show(_app, "Delete Entry", "It's gone, calm down man!",
            emptyDeleteHelp_())
      }
      return
    }
    if (!mutable_("remove")) return
    if (_selected.type != 0) {
      Alert.show(_app, "Delete Entry",
          "Cannot delete system list entries", systemDeleteHelp_())
      return
    }
    var name = _selected.name
    var selected = _list.selected
    if (!Confirm.show(_app, "Delete %(name)?")) return
    if (!_selected.delete()) {
      Alert.show(_app, "Delete Entry",
          "The directory entry could not be deleted.")
      return
    }
    refresh_(null, selected)
  }

  copy_() {
    if (_selected == null) return
    if (!mutable_("edit")) return
    _clipboard = BbsEditor.draft_(_selected)
    _clipboardType = _selected.type
    _footer.copied = true
  }

  paste_() {
    if (_clipboard == null) return
    if (!mutable_("edit")) return
    var preferred = _selected == null ? null : _selected.name
    var name = _clipboard["name"]
    if (_clipboardType == 0) {
      while (true) {
        name = MenuUi.prompt(_app, "Paste Entry",
            "New personal entry name", name, 30, false, entryNameHelp_())
        if (name == null || name.count == 0) return
        if (pasteNameAvailable_(name)) break
        BbsEditor.showNameError(_app, "Paste Entry", name)
      }
    }
    var bbs = Menu.create(name)
    if (bbs == null) {
      Alert.show(_app, "Paste Entry",
          "The personal entry could not be created.")
      return
    }
    var draft = {}
    for (key in _clipboard.keys) draft[key] = _clipboard[key]
    draft["palette"] = _clipboard["palette"].toList
    draft["name"] = name
    if (!BbsEditor.apply_(_app, bbs, draft, false)) {
      bbs.delete()
      refresh_(preferred)
      return
    } else {
      name = editEntry_(bbs, false)
      _clipboard = null
      _clipboardType = null
      _footer.copied = false
    }
    refresh_(name)
  }

  pasteNameAvailable_(name) {
    if (!Menu.nameAvailable(name)) return false
    for (bbs in Menu.entries) {
      if (MenuUi.namesEqual(bbs.name, name)) return false
    }
    return true
  }

  quick_() {
    if (!mutable_("quick")) return
    var url = MenuUi.prompt(_app, "Quick Connect", "Address or URL", "",
        64, false,
        "# SyncTERM Quick Connect\n\n" +
        "Enter a URL in this form:\n\n" +
        "`[(rlogin|telnet|ssh)://][user[:password]@]host[:port]`")
    if (url == null || url.count == 0) return
    var bbs = Menu.quickConnect(url)
    if (bbs == null) {
      Alert.show(_app, "Quick Connect", "The address could not be parsed.")
      return
    }
    _result = bbs
    _app.quit()
  }

  entryNameHelp_() {
    return "# Directory Entry Name\n\n" +
        "Enter the name of the entry as it is to appear in the directory."
  }

  addressLabel_(type) {
    if (type == 7) return "Phone Number"
    if (type == 8 || type == 9) return "Device Name"
    if (type == 10) return "Command"
    return "Address"
  }

  addressCanContainPort_(type) {
    return type != 7 && type != 8 && type != 9 && type != 10
  }

  splitAddressPort_(address) {
    var colon = address.indexOf(":")
    if (colon <= 0 || address.indexOf(":", colon + 1) >= 0 ||
        colon + 1 >= address.bytes.count) return [address, null]
    var port = Num.fromString(address[(colon + 1)...address.bytes.count])
    if (port == null || !port.isInteger || port < 1 || port > 65535) {
      return [address, null]
    }
    return [address[0...colon], port]
  }

  addAddressHelp_(type) {
    var text = "# Address, Phone Number, Serial Port, or Command\n\n" +
        "Enter the hostname, IP address, phone number, or serial port " +
        "device of the system to connect to. For example: " +
        "`nix.synchro.net`.\n\nFor a Shell connection, enter the command " +
        "to run. Shell connections are available on Unix-like systems."
    if (addressCanContainPort_(type)) {
      text = text + "\n\nA TCP port may follow a single colon, such as " +
          "`bbs.example.net:2323`."
    }
    return text
  }

  settingsAction_(picked) {
    var preferred = _selected == null ? null : _selected.name
    SettingsMenu.begin()
    var changed = SettingsMenu.runAction(_app, picked, _connected)
    if (!changed) return
    if (SettingsMenu.passwordChanged) {
      MainMenu.password = SettingsMenu.directoryPassword
    }
    if (MainMenu.prepare()) {
      refresh_(preferred)
    }
  }

  sort_() {
    var preferred = _selected == null ? null : _selected.name
    SortProfiles.show(_app)
    refresh_(preferred)
  }

  cycleSort_(delta) {
    var profiles = Menu.sortProfiles
    if (profiles.count < 2) return
    var next = (Menu.activeSortProfile + delta) % profiles.count
    if (next < 0) next = next + profiles.count
    var preferred = _selected == null ? null : _selected.name
    if (Menu.setActiveSortProfile(next)) {
      if (!Menu.saveSortProfiles()) {
        Alert.show(_app, "Sort Profiles",
            "The active sort profile could not be saved.")
      }
      refresh_(preferred)
    }
  }

  exit_(quitApplication) {
    if (_app.root.focusedChild == _footer) {
      finishComment_(_footer.value, 0)
    }
    if (_connected && !quitApplication) {
      if (!_app.closeModals()) return
      _app.quit()
      return
    }
    if (_confirmingExit) return
    var settings = Menu.settings
    var confirmed = true
    if (settings.confirmClose) {
      _confirmingExit = true
      confirmed = Confirm.show(_app, "Exit SyncTERM?")
      _confirmingExit = false
    }
    if (confirmed) {
      if (!_app.closeModals()) return
      if (quitApplication) Menu.quitApplication()
      _app.quit()
    }
  }
}

class MainMenu {
  static password { __password }
  static password=(value) { __password = value }

  static drawBackdrop_() {
    var app = App.new()
    var backdrop = ClassicBackdrop.new()
    var size = Screen.size
    backdrop.bounds = Rect.new(1, 1, size[0], size[1])
    app.root.add(backdrop)
    app.drawAll_()
  }

  static prepare() {
    var status = Menu.load(__password)
    while (status == MenuReadStatus.passwordRequired ||
        status == MenuReadStatus.decryptFailed) {
      var message = "Incorrect password.  Try again"
      if (status == MenuReadStatus.passwordRequired) {
        message = "Password for the personal directory"
      }
      var password = MenuUi.promptStandalone("Directory Password",
          message, "", 1023, true,
          "# Encrypted List\n\nEnter the password used to encrypt the " +
          "personal directory.")
      if (password == null) return false
      status = Menu.load(password)
      if (status == MenuReadStatus.ok) __password = password
    }
    if (status != MenuReadStatus.ok) {
      MenuUi.alertStandalone("Directory Error", Menu.statusMessage(status))
      return false
    }
    return true
  }

  static offerSave(source) {
    if (!MenuUi.confirmStandalone("Save Directory Entry",
        "Save this directory entry?")) return false
    var name = source.name
    while (!Menu.nameAvailable(name)) {
      name = MenuUi.promptStandalone("Save Directory Entry",
          "Personal entry name", name, 30, false,
          "# Directory Entry Name\n\nEnter a unique name for the saved entry.")
      if (name == null || name.count == 0) return false
      if (!Menu.nameAvailable(name)) {
        BbsEditor.showNameErrorStandalone("Save Directory Entry", name)
      }
    }
    var bbs = Menu.copy(source, name)
    if (bbs == null) {
      MenuUi.alertStandalone("Save Directory Entry",
          "The personal entry could not be created.")
      return false
    }
    return BbsEditor.editStandalone(bbs, false, true)
  }

  static run(current, connected) {
    if (!prepare()) return null
    return MainMenuApp.new(current, connected).run()
  }
}

// Startup progress and alerts are displayed after the menu VM loads but
// before MainMenu.run() creates the interactive controller.
MainMenu.drawBackdrop_()
