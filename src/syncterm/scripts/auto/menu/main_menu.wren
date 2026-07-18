import "syncterm" for KeyEvent, MouseEvent, Key, Mouse
import "syncterm_menu" for Menu, MenuReadStatus
import "menu_ui" for MenuUi
import "menu_bbs_editor" for BbsEditor
import "menu_settings_ui" for SettingsMenu
import "menu_scrollback" for OfflineScrollbackView
import "menu_sort_profiles" for SortProfiles
import "ui_app" for App
import "ui_widget" for Widget, Rect
import "ui_draw" for Painter
import "ui_style" for Style, Theme
import "ui_pane" for Pane
import "ui_list" for ListView
import "ui_popup" for Alert, Confirm

class ClassicTheme {
  static rgb_(index) {
    return [
      0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
      0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
      0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
      0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
    ][index]
  }

  static color_(value, sentinel, fallback) {
    return value == sentinel ? fallback : value
  }

  static style_(foreground, background) {
    return Style.new(0, (background << 4) | foreground,
        rgb_(foreground), rgb_(background))
  }

  static from(settings) {
    var frame = color_(settings.frameColor, 16, 14)
    var text = color_(settings.textColor, 16, 15)
    var background = color_(settings.backgroundColor, 8, 1)
    var inverse = color_(settings.inverseColor, 8, 3)
    var lightbar = color_(settings.lightbarColor, 16, 1)
    var lightbarBackground = color_(settings.lightbarBackgroundColor, 8, 7)

    var normal = style_(text, background)
    var inactive = style_(text, inverse)
    var selected = style_(lightbar, lightbarBackground)
    var inactiveSelected = style_(frame, inverse)
    var frameStyle = style_(frame, background)
    var roles = {}
    var base = Theme.default
    for (name in base.roles.keys) roles[name] = base.roles[name]

    roles["default"] = normal
    roles["default.inactive"] = inactive
    roles["frame"] = frameStyle
    roles["frame.inactive"] = inactive
    roles["title"] = frameStyle
    roles["title.inactive"] = inactive
    roles["menu.item"] = normal
    roles["menu.item.focused"] = selected
    roles["list.item"] = normal
    roles["list.item.focused"] = selected
    roles["list.item.focused.inactive"] = inactiveSelected
    roles["input"] = selected
    roles["input.focused"] = selected
    roles["button"] = normal
    roles["button.hotkey"] = frameStyle
    roles["button.focused"] = selected
    roles["button.focused.hotkey"] = selected
    roles["checkbox"] = normal
    roles["checkbox.focused"] = selected
    roles["radio.item"] = normal
    roles["radio.item.focused"] = selected
    roles["spinbox"] = selected
    roles["spinbox.focused"] = selected
    roles["statusbar"] = style_(0, inverse)
    roles["classic.backdrop"] = style_(inverse, background)
    roles["classic.header"] = style_(background, inverse)
    roles["classic.comment"] = inactive
    roles["classic.hint"] = style_(0, inverse)
    roles["classic.hotkey"] = style_(background, inverse)
    roles["help"] = normal
    roles["help.bold"] = frameStyle
    roles["help.code"] = frameStyle
    roles["help.heading.1"] = style_(background, inverse)
    roles["help.heading.2"] = style_(background, inverse)
    roles["help.heading.3"] = style_(background, inverse)
    roles["help.bullet"] = normal
    return Theme.new(roles, base.glyphs)
  }
}

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

class ClassicFooter is Widget {
  construct new(onComment) {
    super()
    focusable = false
    activitySensitive = false
    _comment = ""
    _copied = false
    _onComment = onComment
  }

  comment=(value) {
    _comment = value == null ? "" : value
    markDirty()
  }

  copied=(value) {
    _copied = value
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

  paintHints_(sf) {
    var segments = [
      ["F1 ", true], ["Help  ", false],
      ["F2 ", true], ["Edit Item  ", false],
      ["F5 ", true], ["Copy  ", false]
    ]
    if (_copied) {
      segments.add(["F6 ", true])
      segments.add(["Paste  ", false])
    }
    segments.add(["INS", true])
    segments.add(["ert Item  ", false])
    segments.add(["DEL", true])
    segments.add(["ete Item  ", false])
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
    Painter.fill(sf, Rect.new(0, 0, bounds.w, bounds.h), " ",
        style("classic.hint"))
    if (bounds.h < 2) return

    var text = cleanLine_(_comment)
    var width = (bounds.w - 4).max(0)
    var offset = text.count < width ? ((width - text.count) / 2).floor : 0
    Painter.text(sf, 2 + offset, 0, text, style("classic.comment"),
        width - offset)
    paintHints_(sf)
  }

  handle(event) {
    if (event is MouseEvent && event.event == Mouse.button1Click &&
        event.startY == bounds.y && _onComment != null) {
      _onComment.call()
      return true
    }
    return false
  }
}

class MainPane is Pane {
  construct new(onFocus) {
    super()
    _onFocus = onFocus
  }

  handle(event) {
    if (event is MouseEvent && bounds != null &&
        bounds.contains(event.startX, event.startY)) {
      _onFocus.call()
    }
    return super.handle(event)
  }
}

class MainList is ListView {
  construct new(onFocus, onCycle) {
    super()
    _onFocus = onFocus
    _onCycle = onCycle
  }

  handle(event) {
    if (event is MouseEvent && bounds != null &&
        bounds.contains(event.startX, event.startY)) {
      _onFocus.call()
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
    return super.handle(event)
  }
}

class MainMenuApp {
  construct new(current, connected) {
    _app = App.new()
    _app.theme = ClassicTheme.from(Menu.settings)
    _connected = connected
    _current = current
    _result = null
    _entries = []
    _selected = null
    _clipboard = null
    _settingRows = SettingsMenu.rows(_connected)

    _backdrop = ClassicBackdrop.new()
    _backdrop.visible = !_connected
    _app.root.add(_backdrop)

    _directory = MainPane.new(Fn.new { focusDirectory_() })
    _directory.helpText = directoryHelp_()
    _directory.onClose = Fn.new { exit_() }
    _app.root.add(_directory)

    _list = MainList.new(Fn.new { focusDirectory_() },
        Fn.new {|delta| cycleSort_(delta) })
    _list.onChange = Fn.new {|index, item|
      _selected = index != null && index >= 0 &&
          index < _entries.count ? _entries[index] : null
      Menu.showEntry(_selected)
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

    _settings = MainPane.new(Fn.new { focusSettings_() })
    _settings.title = "SyncTERM Settings"
    _settings.helpText = SettingsMenu.helpText(_connected)
    _settings.onClose = Fn.new { exit_() }
    _app.root.add(_settings)

    _settingsList = MainList.new(Fn.new { focusSettings_() }, null)
    var settingLabels = []
    for (row in _settingRows) settingLabels.add(row[1])
    _settingsList.items = settingLabels
    _settingsList.onSelect = Fn.new {|index, item|
      settingsAction_(_settingRows[index][0])
    }
    _settings.add(_settingsList)

    _footer = ClassicFooter.new(Fn.new { editComment_() })
    _app.root.add(_footer)

    _app.onLayout = Fn.new {|width, height| layout_(width, height) }
    bind_(Key.escape, Fn.new { exit_() })
    bind_(Key.f2, Fn.new { edit_() })
    bind_(Key.ctrlE, Fn.new { edit_() })
    bind_(Key.insert, Fn.new { add_() })
    bind_(Key.delete, Fn.new { delete_() })
    bind_(Key.ctrlS, Fn.new { sort_() })
    bind_(Key.f5, Fn.new { copy_() })
    bind_(Key.f6, Fn.new { paste_() })
    if (!_connected) bind_(Key.ctrlD, Fn.new { quick_() })
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

  run() {
    _app.runSync()
    return _result
  }

  directoryHelp_() {
    var action = "Connect to the selected entry"
    if (_connected) action = "Edit the selected entry"
    var text = "# SyncTERM Directory\n\n" +
        "## Commands\n\n" +
        "Enter\n:  %(action)\n" +
        "F2 or Ctrl-E\n:  Edit the selected entry\n" +
        "Insert\n:  Add a new personal entry\n" +
        "Blank final row\n:  Add a new personal entry\n" +
        "Delete\n:  Remove the selected personal entry\n" +
        "F5 / F6\n:  Copy and paste an entry\n" +
        "Ctrl-S\n:  Manage sort profiles\n" +
        "< / >\n:  Cycle through sort profiles\n" +
        "Tab\n:  Move between the directory and settings panes"
    if (_connected) {
      return text
    }
    return text + "\nCtrl-D\n:  Quick-connect to an address or URL\n" +
        "Alt-B\n:  View the scrollback from the last session\n\n" +
        "## Window Keys\n\n" +
        "Alt-Left / Alt-Right\n:  Snap to the next smaller or larger width\n" +
        "Alt-Enter\n:  Toggle full-screen mode when available\n\n" +
        "## List Keys\n\n" +
        "Ctrl-F\n:  Find text in the current list\n" +
        "Ctrl-G\n:  Repeat the last search"
  }

  focusDirectory_() {
    _app.root.focusedIndex = 1
  }

  focusSettings_() {
    _app.root.focusedIndex = 2
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

  refresh_(preferred) {
    _entries = Menu.entries
    var labels = []
    var selected = 0
    for (i in 0..._entries.count) {
      var bbs = _entries[i]
      labels.add(bbs.name)
      if (preferred != null && bbs.name == preferred) selected = i
    }
    labels.add("")
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
    _app.root.markDirty()
  }

  connect_() {
    if (_selected == null) return
    _result = _selected
    _app.quit()
  }

  edit_() {
    if (_selected == null) {
      if (_list.selected == _entries.count) add_()
      return
    }
    var bbs = _selected
    if (bbs.type != 0) {
      if (!Confirm.show(_app,
          "Copy this read-only entry to the personal directory?")) return
      var name = MenuUi.prompt(_app, "Copy Entry", "Personal entry name",
          bbs.name, 30, false,
          "# Copy From System Directory\n\n" +
          "This entry came from a read-only system directory. Give the " +
          "new personal copy a unique name before editing it.")
      if (name == null) return
      bbs = Menu.copy(bbs, name)
      if (bbs == null) {
        Alert.show(_app, "Copy Failed",
            "The personal entry name is invalid or already in use.")
        return
      }
      BbsEditor.edit(_app, bbs, false, true)
      refresh_(name)
      return
    }
    BbsEditor.edit(_app, bbs, false, false)
    refresh_(bbs.name)
  }

  editComment_() {
    if (_selected == null) return
    if (_selected.type != 0) {
      edit_()
      return
    }
    var value = MenuUi.prompt(_app, "Entry Comment", "Comment",
        _selected.comment, 1023, false,
        "# Comment\n\nEnter the text shown below this directory entry.")
    if (value == null || value == _selected.comment) return
    var name = _selected.name
    _selected.comment = value
    if (!_selected.save()) {
      Alert.show(_app, "Entry Comment", "The comment could not be saved.")
    }
    refresh_(name)
  }

  add_() {
    var name = MenuUi.prompt(_app, "New Entry", "Entry name", "", 30,
        false, entryNameHelp_())
    if (name == null) return
    var bbs = Menu.create(name)
    if (bbs == null) {
      Alert.show(_app, "New Entry",
          "The entry name is invalid or already in use.")
      return
    }
    BbsEditor.edit(_app, bbs, false, true)
    refresh_(name)
  }

  delete_() {
    if (_selected == null) return
    if (_selected.type != 0) {
      Alert.show(_app, "Delete Entry",
          "Read-only directory entries cannot be deleted.")
      return
    }
    var name = _selected.name
    if (!Confirm.show(_app, "Delete %(name)?")) return
    if (!_selected.delete()) {
      Alert.show(_app, "Delete Entry",
          "The directory entry could not be deleted.")
      return
    }
    _clipboard = null
    _footer.copied = false
    refresh_(null)
  }

  copy_() {
    if (_selected == null) return
    _clipboard = BbsEditor.draft_(_selected)
    _footer.copied = true
  }

  paste_() {
    if (_clipboard == null) return
    var name = MenuUi.prompt(_app, "Paste Entry", "New personal entry name",
        _clipboard["name"], 30, false, entryNameHelp_())
    if (name == null) return
    var bbs = Menu.create(name)
    if (bbs == null) {
      Alert.show(_app, "Paste Entry",
          "The entry name is invalid or already in use.")
      return
    }
    var draft = {}
    for (key in _clipboard.keys) draft[key] = _clipboard[key]
    draft["palette"] = _clipboard["palette"].toList
    draft["name"] = name
    if (!BbsEditor.apply_(_app, bbs, draft, false)) bbs.delete()
    refresh_(name)
  }

  quick_() {
    var url = MenuUi.prompt(_app, "Quick Connect", "Address or URL", "",
        1024, false,
        "# SyncTERM Quick Connect\n\n" +
        "Enter a URL in this form:\n\n" +
        "`[(rlogin|telnet|ssh)://][user[:password]@]host[:port]`")
    if (url == null) return
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
        "Enter the unique name that will appear in the directory."
  }

  settingsAction_(picked) {
    var preferred = _selected == null ? null : _selected.name
    SettingsMenu.begin()
    var changed = SettingsMenu.runAction(_app, picked)
    if (!changed) return
    if (SettingsMenu.passwordChanged) {
      MainMenu.password = SettingsMenu.directoryPassword
    }
    var status = Menu.load(MainMenu.password)
    if (status == MenuReadStatus.ok) {
      _app.theme = ClassicTheme.from(Menu.settings)
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
    if (profiles.count == 0) return
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

  exit_() {
    if (_connected) {
      _app.quit()
      return
    }
    var settings = Menu.settings
    if (!settings.confirmClose || Confirm.show(_app, "Exit SyncTERM?")) {
      _app.quit()
    }
  }
}

class MainMenu {
  static password { __password }
  static password=(value) { __password = value }

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
    if (!Menu.nameAvailable(name)) {
      name = MenuUi.promptStandalone("Save Directory Entry",
          "Personal entry name", name, 30, false,
          "# Directory Entry Name\n\nEnter a unique name for the saved entry.")
      if (name == null) return false
    }
    var bbs = Menu.copy(source, name)
    if (bbs == null) {
      MenuUi.alertStandalone("Save Directory Entry",
          "The entry name is invalid or already in use.")
      return false
    }
    return BbsEditor.editStandalone(bbs, false, true)
  }

  static run(current, connected) {
    if (!prepare()) return null
    return MainMenuApp.new(current, connected).run()
  }
}
