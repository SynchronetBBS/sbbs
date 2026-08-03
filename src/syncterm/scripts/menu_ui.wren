import "syncterm" for Key, KeyEvent, Mouse, MouseEvent, Screen
import "ui_app" for App
import "ui_draw" for Painter
import "ui_widget" for Rect, Widget
import "ui_pane" for Pane
import "ui_list" for ListView
import "ui_popup" for Alert, Confirm, Prompt, LinePrompt, Popup

class ChoiceMessage is Widget {
  construct new(message) {
    super()
    focusable = false
    _message = message == null ? "" : message
    _lines = []
    _layoutW = -1
    _layoutH = -1
    _scrollTop = 0
  }

  scrollTop { _scrollTop }

  bounds=(value) {
    super.bounds = value
    _layoutW = -1
  }

  ensureLayout_() {
    if (bounds == null) return
    if (_layoutW == bounds.w && _layoutH == bounds.h) return
    _layoutW = bounds.w
    _layoutH = bounds.h
    var width = bounds.w.max(1)
    var lines = Popup.wrap_(_message, width)
    if (lines.count > bounds.h && width > 2) {
      lines = Popup.wrap_(_message, width - 2)
    }
    _lines = lines
    setScrollTop_(_scrollTop)
  }

  setScrollTop_(value) {
    var maxTop = (_lines.count - (bounds == null ? 0 : bounds.h)).max(0)
    var next = value.max(0).min(maxTop)
    if (next == _scrollTop) return
    _scrollTop = next
    markDirty()
  }

  overflowing_ { _lines.count > bounds.h }

  onPaint_() {
    ensureLayout_()
    var sf = surface
    var st = style("default")
    Painter.fill(sf, Rect.new(0, 0, bounds.w, bounds.h), " ", st)
    var width = bounds.w - (overflowing_ ? 2 : 0)
    var row = 0
    while (row < bounds.h && _scrollTop + row < _lines.count) {
      Painter.text(sf, 0, row, _lines[_scrollTop + row], st, width)
      row = row + 1
    }
    if (!overflowing_) return
    var glyphs = effectiveTheme.glyphs
    Painter.scrollbar(sf, bounds.w - 1, 0, bounds.h, _scrollTop,
        _lines.count, bounds.h, glyphs, style("scrollbar.track"),
        style("scrollbar.thumb"))
    var sep = glyph("scrollbar.separator")
    for (y in 0...bounds.h) {
      Painter.fill(sf, Rect.new(bounds.w - 2, y, 1, 1), sep, st)
    }
  }

  handle(event) {
    ensureLayout_()
    if (event is KeyEvent) {
      var code = event.code
      if (code == Key.up) {
        setScrollTop_(_scrollTop - 1)
      } else if (code == Key.down) {
        setScrollTop_(_scrollTop + 1)
      } else if (code == Key.pageUp) {
        setScrollTop_(_scrollTop - bounds.h.max(1))
      } else if (code == Key.pageDown) {
        setScrollTop_(_scrollTop + bounds.h.max(1))
      } else if (code == Key.home) {
        setScrollTop_(0)
      } else if (code == Key.end) {
        setScrollTop_(_lines.count)
      } else {
        return false
      }
      return true
    }
    if (!(event is MouseEvent) || !bounds.contains(event.startX, event.startY)) {
      return false
    }
    if (event.event == Mouse.wheelUpPress ||
        event.event == Mouse.wheelUpClick) {
      setScrollTop_(_scrollTop - 1)
      return true
    }
    if (event.event == Mouse.wheelDownPress ||
        event.event == Mouse.wheelDownClick) {
      setScrollTop_(_scrollTop + 1)
      return true
    }
    if (event.event == Mouse.button1Click && overflowing_ &&
        event.startX == bounds.right) {
      setScrollTop_(Painter.scrollbarClick(event.endY - bounds.y,
          bounds.h, _lines.count, bounds.h, _scrollTop))
      return true
    }
    return false
  }
}

class StandaloneChoice is Popup {
  construct new(message, labels) {
    super(null)
    keyHints = [["PgUp/PgDn", "Scroll"], ["Enter", "Select"],
        ["Esc", "Close"]]
    _message = message == null ? "" : message
    _messageView = ChoiceMessage.new(_message)
    _messageView.visible = _message.count > 0
    add(_messageView)
    _list = ListView.new()
    _list.items = labels
    _list.onSelect = Fn.new {|index, item| dismissWith_(index) }
    add(_list)
    focusedIndex = 1
  }

  selected=(value) { _list.selected = value }
  messageScrollTop { _messageView.scrollTop }
  messageBounds { _messageView.bounds }
  listBounds { _list.bounds }
  closesOnOutsideClick(event) {
    return event == Mouse.button1Click || event == Mouse.button3Click
  }

  bounds=(value) {
    super.bounds = value
    var cb = contentBounds
    if (cb == null) return
    var listHeight = _list.count.min(cb.h).max(1)
    var gap = _message.count > 0 && cb.h > listHeight ? 1 : 0
    var messageHeight = (cb.h - listHeight - gap).max(0)
    _messageView.bounds = Rect.new(cb.x, cb.y, cb.w, messageHeight)
    _messageView.visible = messageHeight > 0 && _message.count > 0
    _list.bounds = Rect.new(cb.x, cb.y + messageHeight + gap, cb.w,
        listHeight)
  }

  handle(event) {
    if (event is KeyEvent && event.code == Key.escape) {
      dismissWith_(null)
      return true
    }
    if (event is KeyEvent &&
        (event.code == Key.pageUp || event.code == Key.pageDown) &&
        _messageView.visible) {
      return _messageView.handle(event)
    }
    return super.handle(event)
  }
}

class ModalPane is Pane {
  construct new(onDismiss) {
    super()
    shadow = true
    _onDismiss = onDismiss
  }

  closesOnOutsideClick(event) {
    return event == Mouse.button1Click || event == Mouse.button3Click
  }

  handle(event) {
    if (event is KeyEvent && event.code == Key.escape) {
      _onDismiss.call()
      return true
    }
    return super.handle(event)
  }
}

class CommandPane is ModalPane {
  construct new(onDismiss, commands, onCommand) {
    super(onDismiss)
    _commands = commands
    _onCommand = onCommand
  }

  handle(event) {
    if (event is KeyEvent) {
      var code = event.code
      var aliased = true
      if (code == 0x2B) {
        code = Key.insert
      } else if (code == 0x2D || code == Key.delChar) {
        code = Key.delete
      } else if (code == Key.ctrlIns) {
        code = Key.f5
      } else if (code == Key.shiftIns) {
        code = Key.f6
      } else {
        aliased = false
      }
      var spec = _commands[code]
      if (spec != null) {
        _onCommand.call(spec)
        return true
      }
      if (aliased) return true
    }
    return super.handle(event)
  }
}

// Retains the ListView used by a choice pane across repeated modal runs.
// Replacing its items then uses ListView's normal selection/viewport
// preservation instead of rebuilding from selection alone.
class ChoiceViewState {
  construct new() { _list = ListView.new() }
  list { _list }
}

class MenuUi {
  static commandLabel_(name) {
    if (name == "new") return "New"
    if (name == "edit") return "Edit"
    if (name == "insert") return "Add"
    if (name == "delete") return "Delete"
    if (name == "clear") return "Clear"
    if (name == "rename") return "Rename"
    if (name == "copy") return "Copy"
    if (name == "cut") return "Cut"
    if (name == "paste") return "Paste"
    if (name == "previous") return "Previous"
    if (name == "next") return "Next"
    return name
  }

  static addCommandHint_(hints, commands, code, key) {
    var spec = commands[code]
    if (spec != null) hints.add([key, commandLabel_(spec[0])])
  }

  static commandHints_(commands, hasHelp) {
    var hints = []
    if (hasHelp) hints.add(["F1", "Help"])
    hints.add(["Enter", "Select"])
    addCommandHint_(hints, commands, Key.f2, "F2")
    addCommandHint_(hints, commands, Key.f5, "F5")
    addCommandHint_(hints, commands, Key.f6, "F6")
    addCommandHint_(hints, commands, Key.insert, "INS")
    addCommandHint_(hints, commands, Key.delete, "DEL")
    addCommandHint_(hints, commands, Key.ctrlX, "Ctrl-X")
    addCommandHint_(hints, commands, 0x5B, "[")
    addCommandHint_(hints, commands, 0x5D, "]")
    hints.add(["Esc", "Close"])
    return hints
  }

  static choiceList_(viewState) {
    return viewState == null ? ListView.new() : viewState.list
  }

  static namesEqual(left, right) {
    if (left.bytes.count != right.bytes.count) return false
    for (i in 0...left.bytes.count) {
      var l = left.bytes[i]
      var r = right.bytes[i]
      if (l >= 0x41 && l <= 0x5A) l = l + 0x20
      if (r >= 0x41 && r <= 0x5A) r = r + 0x20
      if (l != r) return false
    }
    return true
  }

  static promptStandalone(title, message, initial, maxLen, masked) {
    return promptStandalone(title, message, initial, maxLen, masked, null)
  }

  static promptStandalone(title, message, initial, maxLen, masked,
      helpText) {
    return promptStandalone_(title, message, initial, maxLen, masked,
        helpText, false)
  }

  static promptStandaloneAtExit(title, message, initial, maxLen, masked,
      helpText) {
    return promptStandalone_(title, message, initial, maxLen, masked,
        helpText, true)
  }

  static promptStandalone_(title, message, initial, maxLen, masked,
      helpText, atExit) {
    var app = App.new()
    var p = Prompt.new(message, initial)
    p.title = title
    p.helpText = helpText
    p.atExit = atExit
    p.input.maxLen = maxLen
    if (masked) p.input.mask = "*"
    p.sizeForInput(maxLen, 34)
    p.onDismiss = Fn.new {|value| app.quit() }
    app.pushModal(p)
    app.runSync()
    return p.result
  }

  static alertStandalone(title, message) {
    alertStandalone(title, message, null)
  }

  static alertStandalone(title, message, helpText) {
    alertStandalone_(title, message, helpText, false)
  }

  static alertStandaloneAtExit(title, message) {
    alertStandalone_(title, message, null, true)
  }

  static alertStandaloneAtExit(title, message, helpText) {
    alertStandalone_(title, message, helpText, true)
  }

  static alertStandalone_(title, message, helpText, atExit) {
    var app = App.new()
    var p = Alert.new(title, message)
    p.helpText = helpText
    p.atExit = atExit
    p.bounds = Popup.centeredTitledBounds_(title, message, 1, 24)
    p.onDismiss = Fn.new {|value| app.quit() }
    app.pushModal(p)
    app.runSync()
  }

  static confirmStandalone(title, message) {
    return confirmStandalone_(title, message, false)
  }

  static confirmStandaloneAtExit(title, message) {
    return confirmStandalone_(title, message, true)
  }

  static confirmStandalone_(title, message, atExit) {
    var app = App.new()
    var p = Confirm.new(message)
    p.title = title
    p.atExit = atExit
    p.bounds = Popup.centeredTitledBounds_(title, message, 1, 24)
    p.onDismiss = Fn.new {|value| app.quit() }
    app.pushModal(p)
    app.runSync()
    return p.result
  }

  static choiceStandalone(title, rows, current) {
    return choiceStandalone(title, "", rows, current)
  }

  static choiceStandalone(title, message, rows, current) {
    if (rows.count == 0) return null
    var app = App.new()
    var longest = title.count + 12
    var labels = []
    for (row in rows) {
      var label = row is List ? row[1] : row
      labels.add(label)
      if (label.count + 4 > longest) longest = label.count + 4
    }
    var popup = StandaloneChoice.new(message, labels)
    popup.title = title
    popup.bounds = Popup.centeredBounds_(message, labels.count + 1,
        longest.max(24))
    if (current == null) {
      popup.selected = 0
    } else if (rows[0] is List) {
      popup.selected = rowIndex(rows, current)
    } else {
      popup.selected = current
    }
    popup.onDismiss = Fn.new {|value| app.quit() }
    app.pushModal(popup)
    app.runSync()
    if (popup.result == null) return null
    var row = rows[popup.result]
    return row is List ? row[0] : popup.result
  }

  static rowName(rows, value) {
    for (row in rows) {
      if (row[0] == value) return row[1]
    }
    return "Unknown (%(value))"
  }

  static rowIndex(rows, value) {
    for (i in 0...rows.count) {
      if (rows[i][0] == value) return i
    }
    return 0
  }

  static choice(app, title, rows, current) {
    return choice(app, title, rows, current, null)
  }

  static choice(app, title, rows, current, helpText) {
    return choice(app, title, rows, current, helpText, null)
  }

  // When onSelect is supplied, run it while the choice pane remains
  // immediately below any modal UI opened by the callback.
  static choice(app, title, rows, current, helpText, onSelect) {
    return choice(app, title, rows, current, helpText, onSelect, null)
  }

  static choice(app, title, rows, current, helpText, onSelect, viewState) {
    var onResult = null
    if (onSelect != null) {
      onResult = Fn.new {|picked| onSelect.call(picked[1]) }
    }
    var picked = commandChoice(app, title, rows, current, helpText, {},
        onResult, viewState)
    if (picked == null) return null
    return picked[1]
  }

  // Run a choice list with direct editing commands. `commands` maps
  // key codes to [commandName, allowOnBlankRow]. Enter returns
  // ["select", rowValue]; Escape returns null.
  static commandChoice(app, title, rows, current, helpText, commands) {
    return commandChoice(app, title, rows, current, helpText, commands, null)
  }

  // onResult receives [commandName, rowValue] before the choice pane
  // is dismissed, allowing nested UI to retain the owning menu as its
  // visible parent.
  static commandChoice(app, title, rows, current, helpText, commands,
      onResult) {
    return commandChoice(app, title, rows, current, helpText, commands,
        onResult, null)
  }

  static commandChoice(app, title, rows, current, helpText, commands,
      onResult, viewState) {
    if (rows.count == 0) return null
    var state = {"result": null}
    var cancel = Fn.new { app.popModal() }
    var list = null
    var pane = null
    var finish = Fn.new {|result|
      state["result"] = result
      if (onResult != null) onResult.call(result)
      if (app.modalStack.indexOf(pane) >= 0) app.popModal()
    }
    var onCommand = Fn.new {|spec|
      var index = list.selected
      if (index == null) return
      if (list.items[index].count == 0 && !spec[1]) return
      finish.call([spec[0], rowValue_(rows, index)])
    }
    pane = CommandPane.new(cancel, commands, onCommand)
    pane.title = title
    pane.helpText = helpText
    pane.keyHints = commandHints_(commands, helpText != null)
    pane.focused = true
    var size = Screen.size
    var longest = title.count + 6
    var labels = []
    for (row in rows) {
      var label = row is List ? row[1] : row
      labels.add(label)
      if (label.count + 4 > longest) longest = label.count + 4
    }
    var w = longest.max(24).min(size[0] - 4)
    var h = (labels.count + 4).min(size[1] - 4)
    pane.bounds = Pane.modalBounds(w, h)
    pane.onClose = cancel

    list = choiceList_(viewState)
    pane.add(list)
    list.bounds = pane.innerBounds
    list.items = labels
    if (current == null) {
      list.selected = 0
    } else if (rows[0] is List) {
      list.selected = rowIndex(rows, current)
    } else {
      list.selected = current
    }
    list.onSelect = Fn.new {|i, item|
      finish.call(["select", rowValue_(rows, i)])
    }
    app.modal(pane)
    return state["result"]
  }

  // Choice variant for catalogs which preview on row movement and may
  // contain informational rows that cannot be activated. helpFor and
  // onChange receive the row value; onAccept returns true to close.
  static browserChoice(app, title, rows, current, helpFor, onChange,
      onAccept, onCancel) {
    if (rows.count == 0) return null
    var state = {"result": null, "closed": false}
    var list = null
    var pane = null
    var cancel = Fn.new {
      if (state["closed"]) return
      state["closed"] = true
      onCancel.call()
      app.popModal()
    }
    pane = CommandPane.new(cancel, {}, Fn.new {|spec| null })
    pane.title = title
    pane.keyHints = commandHints_({}, true)
    pane.focused = true
    pane.onClose = cancel

    var size = Screen.size
    var longest = title.count + 6
    var labels = []
    for (row in rows) {
      labels.add(row[1])
      if (row[1].count + 4 > longest) longest = row[1].count + 4
    }
    var w = longest.max(24).min(size[0] - 4)
    var h = (labels.count + 4).min(size[1] - 4)
    pane.bounds = Pane.modalBounds(w, h)

    list = ListView.new()
    list.bounds = pane.innerBounds
    list.items = labels
    if (rows[0] is List) {
      list.selected = rowIndex(rows, current)
    } else {
      list.selected = current == null ? 0 : current
    }
    list.onChange = Fn.new {|index, item|
      if (index == null) return
      var value = rowValue_(rows, index)
      pane.helpText = helpFor.call(value)
      onChange.call(value)
    }
    list.onSelect = Fn.new {|index, item|
      var value = rowValue_(rows, index)
      if (onAccept.call(value) == true) {
        state["result"] = value
        state["closed"] = true
        app.popModal()
      }
    }
    pane.add(list)
    var selected = list.selected
    if (selected != null) {
      var value = rowValue_(rows, selected)
      pane.helpText = helpFor.call(value)
      onChange.call(value)
    }
    app.modal(pane)
    return state["result"]
  }

  // Browser variant with direct commands. Returns [command, rowValue],
  // where command is "select" for Enter. A direct command closes the
  // browser after running onCancel so callers can rebuild a changed catalog.
  static browserCommandChoice(app, title, rows, current, helpFor, onChange,
      onAccept, onCancel, commands) {
    return browserCommandChoice(app, title, rows, current, helpFor, onChange,
        onAccept, onCancel, commands, 0)
  }

  static browserMaximumHeight_(screenHeight, bottomClearance) {
    return (screenHeight - 4 - bottomClearance.max(0)).max(4)
  }

  static browserCommandChoice(app, title, rows, current, helpFor, onChange,
      onAccept, onCancel, commands, bottomClearance) {
    if (rows.count == 0) return null
    var state = {"result": null, "closed": false}
    var list = null
    var pane = null
    var closeWith = Fn.new {|result|
      if (state["closed"]) return
      state["closed"] = true
      state["result"] = result
      onCancel.call()
      app.popModal()
    }
    var cancel = Fn.new { closeWith.call(null) }
    var onCommand = Fn.new {|spec|
      var value = list.selected == null ? null :
          rowValue_(rows, list.selected)
      if (value == null && !spec[1]) return
      closeWith.call([spec[0], value])
    }
    pane = CommandPane.new(cancel, commands, onCommand)
    pane.title = title
    pane.keyHints = commandHints_(commands, true)
    pane.focused = true
    pane.onClose = cancel

    var size = Screen.size
    var longest = title.count + 6
    var labels = []
    for (row in rows) {
      labels.add(row[1])
      if (row[1].count + 4 > longest) longest = row[1].count + 4
    }
    var w = longest.max(24).min(size[0] - 4)
    var h = (labels.count + 4).min(
        browserMaximumHeight_(size[1], bottomClearance))
    pane.bounds = Pane.modalBounds(w, h)

    list = ListView.new()
    list.bounds = pane.innerBounds
    list.items = labels
    if (rows[0] is List) {
      list.selected = rowIndex(rows, current)
    } else {
      list.selected = current == null ? 0 : current
    }
    list.onChange = Fn.new {|index, item|
      if (index == null) return
      var value = rowValue_(rows, index)
      pane.helpText = helpFor.call(value)
      onChange.call(value)
    }
    list.onSelect = Fn.new {|index, item|
      var value = rowValue_(rows, index)
      if (onAccept.call(value) == true) {
        state["result"] = ["select", value]
        state["closed"] = true
        app.popModal()
      }
    }
    pane.add(list)
    var selected = list.selected
    if (selected != null) {
      var value = rowValue_(rows, selected)
      pane.helpText = helpFor.call(value)
      onChange.call(value)
    }
    app.modal(pane)
    return state["result"]
  }

  static rowValue_(rows, index) {
    return rows[index] is List ? rows[index][0] : index
  }

  static prompt(app, title, message, initial, maxLen, masked) {
    return prompt(app, title, message, initial, maxLen, masked, null)
  }

  static prompt(app, title, message, initial, maxLen, masked, helpText) {
    var p = LinePrompt.new(message, initial)
    if (title != message) p.title = title
    p.helpText = helpText
    p.input.maxLen = maxLen
    if (masked) p.input.mask = "*"
    p.sizeForInput(maxLen, 34)
    app.modal(p)
    return p.result
  }

  static integer(app, title, message, initial, minimum, maximum) {
    return integer(app, title, message, initial, minimum, maximum, null)
  }

  static integer(app, title, message, initial, minimum, maximum, helpText) {
    while (true) {
      var value = prompt(app, title, message, initial.toString,
          12, false, helpText)
      if (value == null) return null
      var number = Num.fromString(value)
      if (number != null && number.isInteger &&
          number >= minimum && number <= maximum) return number
      Alert.show(app, title,
          "Enter an integer from %(minimum) through %(maximum).")
    }
  }

  static text(app, title, message) {
    var p = Alert.new(title, message)
    p.preformatted = true
    p.bounds = Popup.centeredTitledBounds_(title, message, 1, 34, true)
    app.modal(p)
  }
}
