import "classic_theme" for ClassicTheme
import "syncterm" for FilePickerOptions, Host, Key, KeyEvent, Mouse,
                       MouseEvent, REPL, Screen
import "ui_app" for App
import "ui_button" for Button
import "ui_draw" for Painter
import "ui_input" for SelectOnFocusInput
import "ui_list" for ListView
import "ui_pane" for Pane
import "ui_popup" for Alert, Confirm
import "ui_widget" for Rect, Widget
import "picker_bootstrap" for PickerBootstrap

class PickerIndicator is Widget {
  construct new() {
    super()
    focusable = false
    activitySensitive = false
    refresh()
  }

  refresh() {
    visible = Host.logUnread
    markDirty()
  }

  onPaint_() {
    var role = Host.logUnreadError ? "classic.console.error" :
        "classic.console"
    Painter.fill(surface, Rect.new(0, 0, 1, 1), "\u203c", style(role))
  }
}

class PickerInput is SelectOnFocusInput {
  construct new(owner) {
    super()
    _owner = owner
    _onLeave = null
  }

  onLeave=(fn) { _onLeave = fn }

  handle(ev) {
    if (ev is MouseEvent && bounds != null &&
        bounds.contains(ev.startX, ev.startY) &&
        ev.event == Mouse.button1Click) {
      _owner.focusWidget(this)
    }
    if (ev is KeyEvent && (ev.code == Key.tab ||
        ev.code == Key.backTab || ev.code == Key.up ||
        ev.code == Key.down)) {
      if (_onLeave != null) _onLeave.call(value)
    }
    return super.handle(ev)
  }
}

class PickerButton is Button {
  construct new(owner, label) {
    super(label)
    _owner = owner
  }

  handle(ev) {
    if (ev is MouseEvent && bounds != null &&
        bounds.contains(ev.startX, ev.startY) &&
        ev.event == Mouse.button1Click) {
      _owner.focusWidget(this)
    }
    return super.handle(ev)
  }
}

class PickerList is ListView {
  construct new(owner, directories) {
    super()
    _owner = owner
    _directories = directories
    highlightWhenUnfocused = false
  }

  formatItem(item, width) {
    var prefix = ""
    if (!_directories && _owner.multiple) {
      prefix = _owner.isSelected(item) ? glyph("tag.on") + " " : "  "
    }
    return prefix + item.name
  }

  searchTextFor_(item) { item.name }

  handleMouse_(event) {
    if (bounds != null && bounds.contains(event.startX, event.startY) &&
        event.event == Mouse.button1Click && !focused) {
      _owner.focusWidget(this)
      if (scrollbarVisible_ &&
          event.startX == bounds.x + scrollbarColumn_) {
        return super.handleMouse_(event)
      }
      if (scrollbarVisible_ && scrollbarSeparator &&
          event.startX == bounds.x + separatorColumn_) return true
      var row = scrollTop + event.startY - bounds.y
      if (row >= 0 && row < count) selected = row
      return true
    }
    return super.handleMouse_(event)
  }
}

class PickerReview is Pane {
  construct new(app, owner) {
    super()
    _app = app
    _owner = owner
    title = "Selected files"
    titleAsBar = true
    shadow = true
    helpText = "# Selected files\n\n" +
        "Return, Delete, or a left click removes the highlighted file. " +
        "Escape closes this window."
    _list = ListView.new()
    _list.items = owner.selectedPaths
    _list.onSelect = Fn.new {|index, item| remove_(index) }
    add(_list)
    onClose = Fn.new { _app.popModal() }
  }

  remove_(index) {
    _owner.removeSelectedAt(index)
    _list.items = _owner.selectedPaths
    if (_list.count == 0) _app.popModal()
  }

  handle(event) {
    if (event is KeyEvent) {
      if (event.code == Key.escape) {
        _app.popModal()
        return true
      }
      if (event.code == Key.delete || event.code == Key.delChar) {
        if (_list.selected != null) remove_(_list.selected)
        return true
      }
    }
    return super.handle(event)
  }
}

class PickerPane is Pane {
  construct new(app, request) {
    super()
    _app = app
    _request = request
    _mode = request.mode
    _options = request.options
    _selectedPaths = []
    _path = null
    _listing = null
    _appliedMask = null
    _displayedPath = null
    _infoLine1 = ""
    _infoLine2 = ""
    _pathRow = -1
    _infoRow = -1
    _buttonRow = -1
    _separatorRow = -1

    title = request.title
    titleAsBar = true
    shadow = true
    helpText = "# File browser\n\n" +
        "The left pane contains directories and the right pane contains " +
        "files matching the mask. Return opens a directory or chooses a " +
        "file. In a multi-file browser, Return or Space toggles a file, " +
        "Ctrl+A selects the visible files, and F2 reviews the selection. " +
        "Ctrl+Return activates OK and Escape cancels."
    onClose = Fn.new { cancel() }

    _directories = PickerList.new(this, true)
    _files = PickerList.new(this, false)
    _directories.onSelect = Fn.new {|index, entry| enterDirectory_(entry) }
    _files.onSelect = Fn.new {|index, entry| enterFile_(entry) }
    _directories.onChange = Fn.new {|index, entry| markDirty() }
    _files.onChange = Fn.new {|index, entry| markDirty() }
    add(_directories)
    add(_files)

    _mask = PickerInput.new(this)
    _mask.value = request.mask.count == 0 ? "*" : request.mask
    _mask.focusable = !option_(FilePickerOptions.maskLocked)
    _mask.onSubmit = Fn.new {|value| applyMask_(value) }
    _mask.onLeave = Fn.new {|value| applyMask_(value) }
    add(_mask)

    _pathInput = PickerInput.new(this)
    _pathInput.visible = true
    _pathInput.focusable = option_(FilePickerOptions.allowEntry)
    _pathInput.onSubmit = Fn.new {|value| applyPath_(value, true) }
    _pathInput.onLeave = Fn.new {|value| applyPath_(value, false) }
    add(_pathInput)

    _review = PickerButton.new(this, "Review")
    _review.visible = multiple
    _review.focusable = multiple
    _review.onPress = Fn.new { review() }
    add(_review)

    _ok = PickerButton.new(this, directoryMode ? "Select" : "OK")
    _ok.onPress = Fn.new { finish() }
    add(_ok)
    _cancel = PickerButton.new(this, "Cancel")
    _cancel.onPress = Fn.new { cancel() }
    add(_cancel)

    var initial = request.initial
    if (initial == null || !refresh_(initial["path"], initial["selected"])) {
      request.cancel()
      Fiber.abort("Cannot read the initial directory")
    }
    focusedIndex = directoryMode ? children.indexOf(_directories) :
        children.indexOf(_files)
  }

  multiple { _mode == "multiple" }
  directoryMode { _mode == "directory" }
  selectedPaths { _selectedPaths.toList }

  option_(flag) { (_options & flag) != 0 }

  focusWidget(widget) {
    var index = children.indexOf(widget)
    if (index >= 0 && widget.focusable) focusedIndex = index
  }

  isSelected(entry) {
    var full = _request.join(_path, entry.name)
    return full != null && _selectedPaths.indexOf(full) >= 0
  }

  removeSelectedAt(index) {
    if (index < 0 || index >= _selectedPaths.count) return
    _selectedPaths.removeAt(index)
    _files.markDirty()
    markDirty()
  }

  refresh_(path, selectedName) {
    var listing = _request.list(path, _mask.value)
    if (listing == null) return false
    _listing = listing
    _path = listing.path
    _directories.items = listing.directories
    _files.items = listing.files
    if (selectedName != null && selectedName.count > 0) {
      var i = 0
      while (i < listing.files.count) {
        if (listing.files[i].name == selectedName) {
          _files.selected = i
          break
        }
        i = i + 1
      }
    }
    _pathInput.value = _path + _mask.value
    _appliedMask = _mask.value
    _displayedPath = _pathInput.value
    markDirty()
    return true
  }

  navigate_(path) {
    if (refresh_(path, null)) return
    Alert.show(_app, "File browser", "Cannot read directory.")
  }

  enterDirectory_(entry) {
    var next = _request.join(_path, entry.name)
    if (next != null) navigate_(next)
  }

  enterFile_(entry) {
    if (directoryMode) return
    var full = _request.join(_path, entry.name)
    if (full == null) return
    if (multiple) {
      var index = _selectedPaths.indexOf(full)
      if (index >= 0) {
        _selectedPaths.removeAt(index)
      } else {
        _selectedPaths.add(full)
      }
      _files.markDirty()
      markDirty()
      return
    }
    finishPath_(full, true)
  }

  applyMask_(value) {
    var next = value.count == 0 ? "*" : value
    if (next == _appliedMask && _listing != null) return
    var previous = _appliedMask
    _mask.value = next
    if (!refresh_(_path, null)) {
      _mask.value = previous
      Alert.show(_app, "File browser", "Cannot apply that file mask.")
    }
  }

  applyPath_(value, wantsFinish) {
    if (!wantsFinish && value == _displayedPath) return
    var resolved = _request.resolve(_path, value)
    if (resolved == null) {
      Alert.show(_app, "File browser", "The path is invalid.")
      return
    }
    if ((option_(FilePickerOptions.pathMustExist) ||
        option_(FilePickerOptions.fileMustExist)) && !resolved.exists) {
      Alert.show(_app, "File browser", "No such path or file.")
      return
    }
    if (resolved.isDirectory) {
      if (directoryMode && wantsFinish) {
        finishPath_(resolved.full, true)
      } else {
        navigate_(resolved.full)
      }
      return
    }
    if (resolved.wildcards || !wantsFinish) {
      if (option_(FilePickerOptions.maskLocked)) {
        Alert.show(_app, "File browser", "The file mask cannot be changed.")
        return
      }
      _mask.value = resolved.name.count == 0 ? "*" : resolved.name
      if (!refresh_(resolved.directory, null)) {
        Alert.show(_app, "File browser", "Cannot read directory.")
      }
      return
    }
    if (!directoryMode) finishPath_(resolved.full, resolved.exists)
  }

  finishPath_(path, exists) {
    if (_mode == "save") {
      if (exists) {
        if (option_(FilePickerOptions.confirmOverwrite) &&
            !Confirm.show(_app, "File exists, overwrite?")) return
        _request.acceptOverwrite(path)
      } else {
        if (option_(FilePickerOptions.confirmCreate) &&
            !Confirm.show(_app, "File does not exist, create?")) return
        _request.acceptCreate(path)
      }
    } else {
      if (exists && option_(FilePickerOptions.confirmOverwrite) &&
          !Confirm.show(_app, "File exists, overwrite?")) return
      if (!exists && option_(FilePickerOptions.confirmCreate) &&
          !Confirm.show(_app, "File does not exist, create?")) return
      _request.accept(path)
    }
    _app.quit()
  }

  finish() {
    if (multiple) {
      if (_selectedPaths.count == 0) {
        Alert.show(_app, "File browser", "No files have been selected.")
        return
      }
      _request.acceptAll(_selectedPaths)
      _app.quit()
      return
    }
    if (directoryMode) {
      finishPath_(_path, true)
      return
    }
    if (_files.selectedItem == null) {
      Alert.show(_app, "File browser", "No file is highlighted.")
      return
    }
    var full = _request.join(_path, _files.selectedItem.name)
    if (full != null) finishPath_(full, true)
  }

  cancel() {
    _request.cancel()
    _app.quit()
  }

  selectAll() {
    if (!multiple) return
    for (entry in _files.items) {
      var full = _request.join(_path, entry.name)
      if (full != null && _selectedPaths.indexOf(full) < 0) {
        _selectedPaths.add(full)
      }
    }
    _files.markDirty()
    markDirty()
  }

  review() {
    if (!multiple) return
    if (_selectedPaths.count == 0) {
      Alert.show(_app, "File browser", "No files have been selected yet.")
      return
    }
    var pane = PickerReview.new(_app, this)
    pane.fitContentToScreen()
    _app.modal(pane)
    _files.markDirty()
    markDirty()
  }

  infoEntry_ {
    if (_directories.focused) return _directories.selectedItem
    return _files.selectedItem
  }

  sizeString_(size) {
    if (size < 1024) return "%(size.floor)B"
    var unit = 1024
    var suffix = "KiB"
    if (size >= 1024 * 1024) {
      unit = 1024 * 1024
      suffix = "MiB"
    }
    if (size >= 1024 * 1024 * 1024) {
      unit = 1024 * 1024 * 1024
      suffix = "GiB"
    }
    var tenths = (size * 10 / unit).floor
    return "%((tenths / 10).floor).%(tenths % 10)%(suffix)"
  }

  updateInfo_() {
    var entry = infoEntry_
    if (entry == null) {
      _infoLine1 = ""
      _infoLine2 = ""
      return
    }
    _infoLine1 = (entry.directory ? "Dir:  " : "File: ") + entry.name
    var detail = entry.directory ? "directory" : sizeString_(entry.size)
    var stamp = entry.timestamp.count == 0 ? "" : "  " + entry.timestamp
    var selected = !entry.directory && multiple && isSelected(entry) ?
        "  [selected]" : ""
    _infoLine2 = "      %(detail)%(stamp)%(selected)"
  }

  bounds=(rect) {
    super.bounds = rect
    var inner = innerBounds
    if (inner == null) return
    var footerRows = _pathInput.visible ? 7 : 6
    var listHeight = (inner.h - footerRows).max(1)
    var divider = inner.x + ((inner.w - 1) / 2).floor
    var leftWidth = (divider - inner.x).max(1)
    var rightX = divider + 1
    var rightWidth = (inner.right - rightX + 1).max(1)
    _directories.bounds = Rect.new(inner.x, inner.y, leftWidth, listHeight)
    _files.bounds = Rect.new(rightX, inner.y, rightWidth, listHeight)

    _separatorRow = inner.y + listHeight
    _infoRow = _separatorRow + 1
    var maskRow = _infoRow + 2
    var fieldX = inner.x + 7
    var fieldWidth = (inner.right - fieldX + 1).max(1)
    _mask.bounds = Rect.new(fieldX, maskRow, fieldWidth, 1)
    _pathRow = _pathInput.visible ? maskRow + 1 : -1
    if (_pathInput.visible) {
      _pathInput.bounds = Rect.new(fieldX, _pathRow, fieldWidth, 1)
    }
    _buttonRow = inner.bottom
    var gap = 4
    var total = _ok.intrinsicWidth + gap + _cancel.intrinsicWidth
    if (_review.visible) total = total + gap + _review.intrinsicWidth
    var x = inner.x + ((inner.w - total) / 2).floor
    if (_review.visible) {
      _review.bounds = Rect.new(x, _buttonRow, _review.intrinsicWidth, 1)
      x = x + _review.intrinsicWidth + gap
    }
    _ok.bounds = Rect.new(x, _buttonRow, _ok.intrinsicWidth, 1)
    x = x + _ok.intrinsicWidth + gap
    _cancel.bounds = Rect.new(x, _buttonRow, _cancel.intrinsicWidth, 1)
  }

  onPaint_() {
    super.onPaint_()
    updateInfo_()
    var x = 1
    var width = bounds.w - 2
    var normalStyle = style("default")
    var frame = style("frame")
    var divider = _directories.bounds.right - bounds.x + 1
    var listTop = _directories.bounds.y - bounds.y
    Painter.vline(surface, divider, listTop, _directories.bounds.h,
        glyph("frame.double.left"), frame)
    var sep = _separatorRow - bounds.y
    Painter.hline(surface, x, sep, width,
        glyph("frame.double.separator"), frame)
    Painter.text(surface, 2, _infoRow - bounds.y, _infoLine1, normalStyle,
        bounds.w - 4)
    Painter.text(surface, 2, _infoRow - bounds.y + 1, _infoLine2,
        normalStyle,
        bounds.w - 4)
    var maskY = _mask.bounds.y - bounds.y
    Painter.text(surface, 2, maskY, "Mask: ", style("title"), 6)
    if (_pathInput.visible) {
      Painter.text(surface, 2, _pathRow - bounds.y, "Path: ",
          style("title"), 6)
    }
    Painter.hline(surface, x, _buttonRow - bounds.y - 1, width,
        glyph("frame.double.separator"), frame)
  }

  handle(event) {
    if (event is KeyEvent) {
      if (event.code == Key.escape) {
        cancel()
        return true
      }
      if (event.code == Key.ctrlJ) {
        finish()
        return true
      }
      if (event.code == Key.f2 && multiple) {
        review()
        return true
      }
      if (event.code == Key.ctrlA && multiple) {
        selectAll()
        return true
      }
      if (event.codepoint == 0x20 && multiple && _files.focused &&
          _files.selectedItem != null) {
        enterFile_(_files.selectedItem)
        return true
      }
    }
    return super.handle(event)
  }
}

class FilePicker {
  static run(request) {
    var app = App.new()
    app.theme = ClassicTheme.fromColors(request.frameColor,
        request.textColor, request.backgroundColor, request.inverseColor,
        request.lightbarColor, request.lightbarBackgroundColor)
    var pane = PickerPane.new(app, request)
    var indicator = PickerIndicator.new()
    app.root.add(pane)
    app.root.add(indicator)
    app.onLayout = Fn.new {|width, height|
      pane.bounds = Rect.new(3, 2, (width - 4).max(1),
          (height - 2).max(1))
      indicator.bounds = Rect.new(width, 1, 1, 1)
    }
    app.onError = Fn.new {|fiber|
      System.print("File picker handler failed: %(fiber.error)")
      REPL.printTrace_(fiber)
      indicator.refresh()
    }
    app.bind(Key.wrenConsole, Fn.new {|key|
      PickerBootstrap.console()
      indicator.refresh()
    })
    app.bind(Key.quit, Fn.new {|key|
      request.quitApplication()
      app.quit()
    })
    app.runSync()
  }
}
