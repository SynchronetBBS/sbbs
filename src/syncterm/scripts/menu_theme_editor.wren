// Full-screen editor for C-owned SyncTERM theme documents.

import "syncterm" for Color, Font, Key, KeyEvent, Mouse, MouseEvent, Screen
import "syncterm_menu" for Menu
import "ui_widget" for Container, Rect, Widget
import "ui_pane" for Pane
import "ui_button" for Button
import "ui_menubar" for MenuBar
import "ui_list" for ListView
import "ui_input" for TextInput
import "ui_checkbox" for Checkbox
import "ui_radio" for RadioGroup
import "ui_spinbox" for SpinBox
import "ui_progress" for ProgressBar
import "ui_statusbar" for StatusBar
import "ui_form" for Form
import "ui_draw" for Painter
import "ui_style" for Style, Theme
import "ui_color_picker" for ColorPicker
import "ui_popup" for Alert, Confirm, Popup
import "ui_help" for Help
import "menu_ui" for MenuUi

class ThemeGlyphGrid is Widget {
  construct new(mode, value) {
    super()
    if (mode != "cp437" && mode != "ascii") {
      Fiber.abort("ThemeGlyphGrid mode must be \"cp437\" or \"ascii\"")
    }
    _mode = mode
    _minimum = mode == "cp437" ? 0 : 0x20
    _maximum = mode == "cp437" ? 0xff : 0x7e
    _selected = value.max(_minimum).min(_maximum)
    _onChange = null
    _onSelect = null
  }

  mode { _mode }
  minimum { _minimum }
  maximum { _maximum }
  columns { 16 }
  rows { ((_maximum - _minimum + 1 + columns - 1) / columns).floor }
  preferredWidth { columns * 2 + 1 }
  preferredHeight { rows }
  selected { _selected }
  selected=(value) {
    var next = value.max(_minimum).min(_maximum)
    if (next == _selected) return
    _selected = next
    if (_onChange != null) _onChange.call(_selected)
    markDirty()
  }
  onChange=(fn) { _onChange = fn }
  onSelect=(fn) { _onSelect = fn }

  moveHorizontal_(delta) {
    var count = _maximum - _minimum + 1
    selected = _minimum + (((_selected - _minimum + delta) % count +
        count) % count)
  }

  moveVertical_(delta) {
    var offset = _selected - _minimum
    var column = offset % columns
    var row = (offset / columns).floor
    var direction = delta < 0 ? -1 : 1
    var remaining = delta.abs
    while (remaining > 0) {
      row = ((row + direction) % rows + rows) % rows
      while (row * columns + column > _maximum - _minimum) {
        row = ((row + direction) % rows + rows) % rows
      }
      remaining = remaining - 1
    }
    selected = _minimum + row * columns + column
  }

  cursorPos {
    if (bounds == null) return null
    var offset = _selected - _minimum
    return [bounds.x + (offset % columns) * 2 + 1,
        bounds.y + (offset / columns).floor]
  }

  onPaint_() {
    var normal = style("list.item")
    var selectedStyle = focused ? style("list.item.focused") :
        effectiveTheme.style("list.item.focused.inactive")
    Painter.fill(surface, Rect.new(0, 0, bounds.w, bounds.h), " ", normal)
    var value = _minimum
    while (value <= _maximum) {
      var offset = value - _minimum
      var x = (offset % columns) * 2
      var y = (offset / columns).floor
      if (y >= bounds.h) break
      var st = value == _selected ? selectedStyle : normal
      if (x >= bounds.w) break
      if (value == _selected) {
        Painter.fill(surface, Rect.new(x, y, 3.min(bounds.w - x), 1),
            " ", st)
      }
      var cell = surface.cellAt(x + 1, y)
      if (cell != null) {
        Painter.applyStyle(cell, st)
        cell.font = Font.cp437English
        cell.chByte = value
      }
      value = value + 1
    }
  }

  activate_() {
    if (_onSelect != null) _onSelect.call(_selected)
  }

  handle(event) {
    if (event is KeyEvent) return handleKey_(event)
    if (event is MouseEvent) return handleMouse_(event)
    return false
  }

  handleKey_(event) {
    var code = event.code
    if (code == Key.left) moveHorizontal_(-1)
    if (code == Key.right) moveHorizontal_(1)
    if (code == Key.up) moveVertical_(-1)
    if (code == Key.down) moveVertical_(1)
    if (code == Key.pageUp) moveVertical_(-rows.min(4))
    if (code == Key.pageDown) moveVertical_(rows.min(4))
    if (code == Key.home) selected = _minimum
    if (code == Key.end) selected = _maximum
    if (code == Key.enter || event.codepoint == 0x20) activate_()
    if (code == Key.left || code == Key.right || code == Key.up ||
        code == Key.down || code == Key.pageUp || code == Key.pageDown ||
        code == Key.home || code == Key.end || code == Key.enter ||
        event.codepoint == 0x20) return true
    return false
  }

  handleMouse_(event) {
    if (bounds == null || !bounds.contains(event.startX, event.startY)) {
      return false
    }
    if (event.event == Mouse.wheelUpPress ||
        event.event == Mouse.wheelUpClick) {
      moveVertical_(-1)
      return true
    }
    if (event.event == Mouse.wheelDownPress ||
        event.event == Mouse.wheelDownClick) {
      moveVertical_(1)
      return true
    }
    if (event.event != Mouse.button1Click &&
        event.event != Mouse.button1DblClick) return false
    var x = event.startX - bounds.x
    var y = event.startY - bounds.y
    var value = _minimum + y * columns +
        (((x - 1).max(0)) / 2).floor
    if (value > _maximum) return true
    selected = value
    if (event.event == Mouse.button1DblClick) activate_()
    return true
  }
}

class ThemeGlyphPickerPane is Pane {
  construct new(grid, onCancel) {
    super()
    shadow = true
    titleAsBar = false
    keyHints = [["Arrows", "Move"], ["Enter", "Select"],
        ["Esc", "Cancel"]]
    _onCancel = onCancel
    add(grid)
  }

  handle(event) {
    if (event is KeyEvent && event.code == Key.escape) {
      _onCancel.call()
      return true
    }
    return super.handle(event)
  }
}

class ThemeGlyphPicker {
  static hex_(value) {
    var chars = "0123456789ABCDEF"
    return "0x" + chars[(value >> 4) & 0x0f] + chars[value & 0x0f]
  }

  static title_(mode, value) {
    if (mode == "cp437") return "CP437 Character - %(hex_(value))"
    return "ASCII Character - %(value) '%(String.fromCodePoint(value))'"
  }

  static choose(app, mode, value) {
    var result = null
    var grid = ThemeGlyphGrid.new(mode, value)
    var pane = null
    var close = Fn.new {|accepted|
      if (accepted) result = grid.selected
      app.popModal()
    }
    pane = ThemeGlyphPickerPane.new(grid, Fn.new { close.call(false) })
    pane.title = title_(mode, grid.selected)
    pane.helpText = mode == "cp437" ?
        "# CP437 Character\n\nSelect one of the 256 raw CP437 character " +
        "codes. The grid is rendered with the CP437 font regardless of the " +
        "font selected by the draft theme." :
        "# ASCII Character\n\nSelect one of the 95 printable ASCII " +
        "characters, from space through tilde."
    pane.focused = true
    pane.onClose = Fn.new { close.call(false) }
    grid.onChange = Fn.new {|next| pane.title = title_(mode, next) }
    grid.onSelect = Fn.new {|next| close.call(true) }
    pane.fitContentToScreen()
    app.modal(pane)
    return result
  }
}

class ThemeEditorModel {
  construct new(document) {
    _document = document
    _mode = "styles"
    _colorMode = "rgb"
    _glyphMode = "cp437"
    _selectedStyle = null
    _selectedGlyph = null
    _onChange = null
    refresh()
  }

  document { _document }
  mode { _mode }
  mode=(value) {
    if (value != "styles" && value != "glyphs") return
    if (_mode == value) return
    _mode = value
    changed_()
  }
  colorMode { _colorMode }
  colorMode=(value) {
    if (value != "rgb" && value != "legacy") return
    if (_colorMode == value) return
    _colorMode = value
    changed_()
  }
  glyphMode { _glyphMode }
  glyphMode=(value) {
    if (value != "cp437" && value != "ascii") return
    if (_glyphMode == value) return
    _glyphMode = value
    _theme.glyphs.asciiOnly = value == "ascii"
    changed_()
  }
  onChange=(fn) { _onChange = fn }

  styles { _styles }
  glyphs { _glyphs }
  theme { _theme }
  rows { _mode == "styles" ? _styles : _glyphs }
  selectedName { _mode == "styles" ? _selectedStyle : _selectedGlyph }
  selectedRow {
    var name = selectedName
    if (name == null) return null
    for (row in rows) {
      if (row[0] == name) return row
    }
    return null
  }
  selectedIndex {
    var name = selectedName
    if (name == null) return null
    var list = rows
    for (i in 0...list.count) {
      if (list[i][0] == name) return i
    }
    return null
  }

  select(index) {
    var list = rows
    if (list.count == 0) return
    var next = index.max(0).min(list.count - 1)
    if (_mode == "styles") {
      _selectedStyle = list[next][0]
    } else {
      _selectedGlyph = list[next][0]
    }
    changed_()
  }

  selectStyle(role) {
    for (row in _styles) {
      if (row[0] == role) {
        _mode = "styles"
        _selectedStyle = role
        changed_()
        return true
      }
    }
    return false
  }

  move(delta) {
    var i = selectedIndex
    if (i == null) i = 0
    var count = rows.count
    if (count == 0) return
    select(((i + delta) % count + count) % count)
  }

  refresh() {
    _styles = _document.styles
    _glyphs = _document.glyphs
    _theme = Theme.fromData(_document.themeData)
    _theme.glyphs.asciiOnly = _glyphMode == "ascii"
    _selectedStyle = preserveSelection_(_styles, _selectedStyle)
    _selectedGlyph = preserveSelection_(_glyphs, _selectedGlyph)
    changed_()
  }

  preserveSelection_(rows, selected) {
    if (rows.count == 0) return null
    for (row in rows) {
      if (row[0] == selected) return selected
    }
    return rows[0][0]
  }

  directRow_(name) {
    for (row in _styles) {
      if (row[0] == name) return row
    }
    return null
  }

  parentRole_(role) {
    if (role.count == 0) return ""
    var i = role.bytes.count - 1
    while (i >= 0) {
      if (role.bytes[i] == 0x2e) return role[0...i]
      i = i - 1
    }
    return ""
  }

  cascade_(role) {
    var out = []
    var inactive = role.count >= 9 &&
        role.indexOf(".inactive") == role.bytes.count - 9
    if (inactive) {
      var current = role[0...role.bytes.count - 9]
      while (current != "") {
        out.add(current + ".inactive")
        current = parentRole_(current)
      }
      if (out.count == 0 || out[-1] != "default.inactive") {
        out.add("default.inactive")
      }
      out.add("default")
      return out
    }
    var current = role
    while (current != "") {
      out.add(current)
      current = parentRole_(current)
    }
    if (out.count == 0 || out[-1] != "default") out.add("default")
    return out
  }

  styleCascade(role, field) {
    var out = []
    var found = false
    for (candidate in cascade_(role)) {
      var row = directRow_(candidate)
      if (row == null) {
        out.add([candidate, "absent", null, false])
        continue
      }
      var value = row[3 + field]
      var present = (row[2] & (1 << field)) != 0
      var kind
      if (present) {
        kind = value == -1 ? "explicit inherit" : "explicit"
      } else if (row[1]) {
        kind = value == -1 ? "compiled inherit" : "compiled default"
      } else {
        kind = "not set"
      }
      var winner = !found && value != -1 &&
          (present || row[1])
      if (winner) found = true
      out.add([candidate, kind, value == -1 ? null : value, winner])
    }
    if (!found) {
      var resolved = _theme.style(role)
      var values = [resolved.font, resolved.legacyAttr,
          resolved.fgRgb, resolved.bgRgb]
      out.add(["resolved", "fallback", values[field], true])
    }
    return out
  }

  styleField(role, field) {
    for (entry in styleCascade(role, field)) {
      if (entry[3]) return [entry[2], entry[0], entry[1]]
    }
    Fiber.abort("Theme style cascade did not resolve %(role)")
  }

  directMode(row, field) {
    if ((row[2] & (1 << field)) == 0) return "default"
    return row[3 + field] == -1 ? "inherit" : "explicit"
  }

  changed_() {
    if (_onChange != null) _onChange.call()
  }
}

class ThemeAtlas is Widget {
  construct new(model, onEdit, onFocus) {
    super()
    _model = model
    _onEdit = onEdit
    _onFocus = onFocus
    _scrollTop = 0
  }

  refresh() {
    ensureVisible_()
    markDirty()
  }

  count_ { _model.rows.count }
  viewport_ { bounds == null ? 1 : bounds.h.max(1) }
  maxTop_ { (count_ - viewport_).max(0) }

  ensureVisible_() {
    var selected = _model.selectedIndex
    if (selected == null) {
      _scrollTop = 0
      return
    }
    if (selected < _scrollTop) _scrollTop = selected
    if (selected >= _scrollTop + viewport_) {
      _scrollTop = selected - viewport_ + 1
    }
    _scrollTop = _scrollTop.max(0).min(maxTop_)
  }

  legacyStyle_(style) {
    if (_model.colorMode != "legacy") return style
    var palette = [
      0x000000, 0x0000aa, 0x00aa00, 0x00aaaa,
      0xaa0000, 0xaa00aa, 0xaa5500, 0xaaaaaa,
      0x555555, 0x5555ff, 0x55ff55, 0x55ffff,
      0xff5555, 0xff55ff, 0xffff55, 0xffffff
    ]
    var attr = style.legacyAttr
    if (attr == null) return style
    return Style.new(style.font, attr, palette[attr & 0x0f],
        palette[(attr >> 4) & 0x07])
  }

  hexByte_(value) {
    var chars = "0123456789ABCDEF"
    return "0x" + chars[(value >> 4) & 0x0f] + chars[value & 0x0f]
  }

  onPaint_() {
    var base = style("default")
    Painter.fill(surface, Rect.new(0, 0, bounds.w, bounds.h), " ", base)
    var rows = _model.rows
    var selected = _model.selectedIndex
    var contentW = bounds.w - (rows.count > bounds.h ? 1 : 0)
    var y = 0
    while (y < bounds.h) {
      var index = _scrollTop + y
      if (index >= rows.count) break
      var row = rows[index]
      if (_model.mode == "styles") {
        paintStyleRow_(row, index == selected, y, contentW)
      } else {
        paintGlyphRow_(row, index == selected, y, contentW)
      }
      y = y + 1
    }
    if (rows.count > bounds.h) {
      Painter.scrollbar(surface, bounds.w - 1, 0, bounds.h, _scrollTop,
          rows.count, bounds.h, _model.theme.glyphs,
          _model.theme.style("scrollbar.track"),
          _model.theme.style("scrollbar.thumb"))
    }
  }

  paintStyleRow_(row, selected, y, width) {
    var role = row[0]
    var sample = legacyStyle_(_model.theme.style(role))
    Painter.fill(surface, Rect.new(0, y, width, 1), " ", sample)
    var marker = " "
    if (selected) marker = glyph("focus.left")
    var text = "%(marker) %(role)"
    var suffix = "  Aa Bb Cc 0123"
    var room = (width - suffix.count).max(1)
    Painter.text(surface, 0, y, text, sample, room)
    if (selected) Painter.text(surface, 0, y, marker,
        focused ? style("list.item.focused") :
            effectiveTheme.style("list.item.focused.inactive"), 1)
    if (width > suffix.count) {
      Painter.text(surface, width - suffix.count, y, suffix, sample,
          suffix.count)
    }
  }

  paintGlyphRow_(row, selected, y, width) {
    var sample = legacyStyle_(_model.theme.style("default"))
    Painter.fill(surface, Rect.new(0, y, width, 1), " ", sample)
    var shownGlyph = _model.theme.glyphs[row[0]]
    var marker = " "
    if (selected) marker = glyph("focus.left")
    var modeValue = _model.glyphMode == "ascii" ? row[4].toString :
        hexByte_(row[3])
    var text = "%(marker) %(shownGlyph)  %(row[0])  [%(modeValue)]"
    Painter.text(surface, 0, y, text, sample, width)
    if (selected) Painter.text(surface, 0, y, marker,
        focused ? style("list.item.focused") :
            effectiveTheme.style("list.item.focused.inactive"), 1)
  }

  handle(event) {
    if (event is KeyEvent) return handleKey_(event)
    if (event is MouseEvent) return handleMouse_(event)
    return false
  }

  handleKey_(event) {
    var code = event.code
    if (code == Key.up) _model.move(-1)
    if (code == Key.down) _model.move(1)
    if (code == Key.pageUp) _model.move(-viewport_ + 1)
    if (code == Key.pageDown) _model.move(viewport_ - 1)
    if (code == Key.home) _model.select(0)
    if (code == Key.end) _model.select(count_ - 1)
    if (code == Key.enter) _onEdit.call()
    if (code == Key.up || code == Key.down || code == Key.pageUp ||
        code == Key.pageDown || code == Key.home || code == Key.end ||
        code == Key.enter) {
      ensureVisible_()
      markDirty()
      return true
    }
    return false
  }

  handleMouse_(event) {
    if (bounds == null || !bounds.contains(event.startX, event.startY)) {
      return false
    }
    var overScroll = count_ > bounds.h && event.startX == bounds.right
    if (event.event == Mouse.wheelUpPress ||
        event.event == Mouse.wheelUpClick) {
      _model.move(overScroll ? -viewport_ + 1 : -1)
      ensureVisible_()
      return true
    }
    if (event.event == Mouse.wheelDownPress ||
        event.event == Mouse.wheelDownClick) {
      _model.move(overScroll ? viewport_ - 1 : 1)
      ensureVisible_()
      return true
    }
    if (event.event != Mouse.button1Click &&
        event.event != Mouse.button1DblClick) return false
    _onFocus.call()
    if (overScroll) {
      _scrollTop = Painter.scrollbarClick(event.startY - bounds.y,
          bounds.h, count_, bounds.h, _scrollTop)
      _model.select((_scrollTop + viewport_ / 2).floor.min(count_ - 1))
      markDirty()
      return true
    }
    var index = _scrollTop + event.startY - bounds.y
    if (index < 0 || index >= count_) return true
    _model.select(index)
    ensureVisible_()
    if (event.event == Mouse.button1DblClick) _onEdit.call()
    return true
  }
}

class ThemeInspector is Widget {
  construct new(model) {
    super()
    focusable = false
    _model = model
  }

  refresh() { markDirty() }

  hex_(value, digits) {
    if (value == null) return "null"
    var chars = "0123456789ABCDEF"
    var text = ""
    var remaining = value
    if (remaining == 0) text = "0"
    while (remaining > 0) {
      text = chars[remaining % 16] + text
      remaining = (remaining / 16).floor
    }
    while (text.count < digits) text = "0" + text
    return "0x" + text
  }

  fieldLine_(label, field, digits) {
    var found = _model.styleField(_model.selectedName, field)
    var value = found[0]
    var shown = digits == 0 ? value.toString : hex_(value, digits)
    return "%(label): %(shown)"
  }

  originLine_(field) {
    var found = _model.styleField(_model.selectedName, field)
    return "  <- %(found[1]) (%(found[2]))"
  }

  lines_() {
    var row = _model.selectedRow
    if (row == null) return ["No selection"]
    if (_model.mode == "glyphs") {
      var source = row[1] ? "built-in" : "custom"
      var cpMode = (row[2] & 1) == 0 ? "default" : "explicit"
      var asciiMode = (row[2] & 2) == 0 ? "default" : "explicit"
      var cpGlyph = _model.glyphMode == "cp437" ?
          " '%(_model.theme.glyphs[row[0]])'" : ""
      return [row[0], source, "",
          "CP437: %(hex_(row[3], 2))%(cpGlyph)",
          "  %(cpMode)", "ASCII: %(row[4]) '%(String.fromCodePoint(row[4]))'",
          "  %(asciiMode)"]
    }
    var source = row[1] ? "built-in role" : "custom role"
    return [row[0], source, "",
        fieldLine_("Font", 0, 0), originLine_(0),
        fieldLine_("Legacy", 1, 2), originLine_(1),
        fieldLine_("Foreground", 2, 6), originLine_(2),
        fieldLine_("Background", 3, 6), originLine_(3)]
  }

  onPaint_() {
    var normal = style("default")
    Painter.fill(surface, Rect.new(0, 0, bounds.w, bounds.h), " ", normal)
    var lines = lines_()
    for (i in 0...lines.count.min(bounds.h)) {
      var lineStyle = i == 0 ? style("title") : normal
      Painter.text(surface, 1, i, lines[i], lineStyle,
          (bounds.w - 2).max(0))
    }
    if (_model.mode == "styles" && bounds.h >= 3) {
      var sample = _model.theme.style(_model.selectedName)
      var y = bounds.h - 2
      Painter.fill(surface, Rect.new(1, y, (bounds.w - 2).max(0), 1),
          " ", sample)
      Painter.text(surface, 2, y, "Aa Bb Cc 0123", sample,
          (bounds.w - 4).max(0))
    }
  }
}

class ThemeColorContent is Container {
  construct new(picker, onAccept, onCancel) {
    super()
    _picker = picker
    _accept = Button.new("OK")
    _cancel = Button.new("Cancel")
    _accept.onPress = onAccept
    _cancel.onPress = onCancel
    add(_picker)
    add(_accept)
    add(_cancel)
  }

  preferredWidth { _picker.preferredWidth + 2 }
  preferredHeight { _picker.preferredHeight + 2 }

  bounds=(rect) {
    super.bounds = rect
    _picker.bounds = Rect.new(rect.x + 1, rect.y,
        (rect.w - 2).max(1), _picker.preferredHeight)
    var gap = 2
    var total = _accept.intrinsicWidth + gap + _cancel.intrinsicWidth
    var x = rect.x + ((rect.w - total) / 2).floor
    var y = rect.bottom
    _accept.bounds = Rect.new(x, y, _accept.intrinsicWidth, 1)
    x = x + _accept.intrinsicWidth + gap
    _cancel.bounds = Rect.new(x, y, _cancel.intrinsicWidth, 1)
  }

  onPaint_() {
    Painter.fill(surface, Rect.new(0, 0, bounds.w, bounds.h), " ",
        style("default"))
    compositeChildren_()
  }
}

class ThemeColorPane is Pane {
  construct new(content, onCancel) {
    super()
    shadow = true
    keyHints = [["Enter", "Accept"], ["Esc", "Cancel"]]
    _onCancel = onCancel
    add(content)
  }

  handle(event) {
    if (event is KeyEvent && event.code == Key.escape) {
      _onCancel.call()
      return true
    }
    return super.handle(event)
  }
}

class ThemeColorDialog {
  static choose(app, title, value) {
    var picker = ColorPicker.new(value)
    var result = null
    var close = Fn.new {|accepted|
      if (accepted && !picker.commitEdits()) return
      if (accepted) result = picker.value
      app.popModal()
    }
    var content = ThemeColorContent.new(picker,
        Fn.new { close.call(true) }, Fn.new { close.call(false) })
    var pane = ThemeColorPane.new(content, Fn.new { close.call(false) })
    pane.title = title
    pane.focused = true
    pane.onClose = Fn.new { close.call(false) }
    pane.fitContentToScreen()
    app.modal(pane)
    return result
  }
}

class ThemeClosePrompt is Popup {
  construct new(onSave) {
    super("Save changes before closing?")
    keyHints = [["Enter", "Choose"], ["Esc", "Cancel"]]
    title = "Theme Editor"
    atExit = true
    _save = Button.new("Save")
    _discard = Button.new("Discard")
    _cancel = Button.new("Cancel")
    _save.onPress = Fn.new {
      if (onSave.call()) dismissWith_("save")
    }
    _discard.onPress = Fn.new { dismissWith_("discard") }
    _cancel.onPress = Fn.new { dismissWith_(null) }
    add(_save)
    add(_discard)
    add(_cancel)
  }

  bounds=(rect) {
    super.bounds = rect
    var gap = 2
    var total = _save.intrinsicWidth + _discard.intrinsicWidth +
        _cancel.intrinsicWidth + gap * 2
    var x = rect.x + ((rect.w - total) / 2).floor
    var y = rect.y + rect.h - 2
    _save.bounds = Rect.new(x, y, _save.intrinsicWidth, 1)
    x = x + _save.intrinsicWidth + gap
    _discard.bounds = Rect.new(x, y, _discard.intrinsicWidth, 1)
    x = x + _discard.intrinsicWidth + gap
    _cancel.bounds = Rect.new(x, y, _cancel.intrinsicWidth, 1)
  }

  handle(event) {
    if (event is KeyEvent && event.code == Key.escape) {
      dismissWith_(null)
      return true
    }
    return super.handle(event)
  }
}

class ThemeWidgetPreviewContent is Container {
  construct new(draftTheme, onClose, onSelectRole) {
    super()
    theme = draftTheme
    _onSelectRole = onSelectRole
    _shownRole = null

    _menu = MenuBar.new()
    var idle = Fn.new {}
    _menu.items = [["File", idle], ["Edit", idle], ["View", idle],
        ["Help", idle]]

    _listPane = Pane.new()
    _listPane.title = "List"
    _listPane.helpable = false
    _listPane.closeable = false
    _list = ListView.new()
    _list.items = ["Selected item", "Ordinary item", "Another item",
        "A longer list entry", "Fifth item", "Sixth item", "Seventh item",
        "Eighth item", "Ninth item", "Tenth item", "Eleventh item"]
    _list.selected = 0
    _list.onSelect = Fn.new {|index, item| null }
    _listPane.add(_list)

    _controlsPane = Pane.new()
    _controlsPane.title = "Controls"
    _controlsPane.helpable = false
    _controlsPane.closeable = false
    _form = Form.new()
    _input = TextInput.new()
    _input.value = "Sample text"
    _input.maxLen = 32
    _check = Checkbox.new("Enabled")
    _check.value = true
    _radio = RadioGroup.new()
    _radio.items = ["First choice", "Second choice"]
    _radio.selected = 0
    _spin = SpinBox.new()
    _spin.min = 0
    _spin.max = 100
    _spin.value = 42
    _progress = ProgressBar.new()
    _progress.set(65, 100)
    _form.addField("Text", _input)
    _form.addField("Option", _check)
    _form.addFieldH("Choice", _radio, 2)
    _form.addField("Value", _spin)
    _form.addField("Progress", _progress)
    _form.onSubmit = idle
    _form.onCancel = onClose
    _controlsPane.add(_form)

    _status = StatusBar.new()
    _status.segments = [["Ready", "left"], ["Draft theme", "right"]]

    add(_menu)
    add(_listPane)
    add(_controlsPane)
    add(_status)
    focusedIndex = 1
    updateStatus_()
  }

  currentRole {
    var current = focusedChild
    if (current == _menu) return "menubar.item.focused"
    if (current == _listPane) return "list.item.focused"
    if (current != _controlsPane) return null
    var field = _form.focusedChild
    if (field == _input) return "input.focused"
    if (field == _check) return "checkbox.focused"
    if (field == _radio) return "radio.item.focused"
    if (field == _spin) return "spinbox.focused"
    return "button.focused"
  }

  selectCurrentRole() {
    var role = currentRole
    if (role == null) return false
    _onSelectRole.call(role)
    return true
  }

  updateStatus_() {
    var role = currentRole
    if (role == _shownRole) return
    _shownRole = role
    var shown = role == null ? "No selectable role" : role
    _status.segments = [["F2 Select Role", "left"], [shown, "right"]]
  }

  bounds=(rect) {
    super.bounds = rect
    _menu.bounds = Rect.new(rect.x, rect.y, rect.w, 1)
    _status.bounds = Rect.new(rect.x, rect.bottom, rect.w, 1)
    var bodyY = rect.y + 1
    var bodyH = (rect.h - 2).max(1)
    var leftW = ((rect.w - 1) / 2).floor.max(1)
    var rightW = (rect.w - leftW - 1).max(1)
    _listPane.bounds = Rect.new(rect.x, bodyY, leftW, bodyH)
    _controlsPane.bounds = Rect.new(rect.x + leftW + 1, bodyY,
        rightW, bodyH)
    _list.bounds = _listPane.innerBounds
    _form.bounds = _controlsPane.innerBounds
  }

  onPaint_() {
    updateStatus_()
    Painter.fill(surface, Rect.new(0, 0, bounds.w, bounds.h), " ",
        style("default"))
    compositeChildren_()
  }

  handle(event) {
    var handled = super.handle(event)
    updateStatus_()
    return handled
  }
}

class ThemeWidgetPreviewPane is Pane {
  construct new(content, onClose) {
    super()
    shadow = true
    _content = content
    _onCancel = onClose
    add(content)
  }

  handle(event) {
    if (event is KeyEvent) {
      if (event.code == Key.escape) {
        _onCancel.call()
        return true
      }
      if (event.code == Key.f2 && _content.selectCurrentRole()) {
        return true
      }
    }
    return super.handle(event)
  }
}

class ThemeEditorContent is Container {
  construct new(model, actions) {
    super()
    _model = model
    _actions = actions
    _primary = MenuBar.new()
    _secondary = MenuBar.new()
    _atlas = ThemeAtlas.new(model, actions["edit"], Fn.new {
      focusedIndex = 2
    })
    _inspector = ThemeInspector.new(model)
    _modeAction = null
    _displayAction = null

    _primary.onFocus = Fn.new { focusedIndex = 0 }
    _secondary.onFocus = Fn.new { focusedIndex = 1 }

    updatePrimary_()
    _secondary.items = [
      ["Metadata", Fn.new { actions["metadata"].call() }],
      ["Import", Fn.new { actions["import"].call() }],
      ["Undo", Fn.new { actions["undo"].call() }],
      ["Redo", Fn.new { actions["redo"].call() }],
      ["Save", Fn.new { actions["save"].call() }],
      ["Save As", Fn.new { actions["saveAs"].call() }],
      ["Close", Fn.new { actions["close"].call() }]
    ]
    add(_primary)
    add(_secondary)
    add(_atlas)
    add(_inspector)
    focusedIndex = 2
  }

  atlas { _atlas }

  refresh() {
    updatePrimary_()
    _atlas.refresh()
    _inspector.refresh()
    markDirty()
  }

  updatePrimary_() {
    var modeLabel = _model.mode == "styles" ? "Show Glyphs" : "Show Styles"
    var label
    if (_model.mode == "styles") {
      label = _model.colorMode == "rgb" ? "Show Legacy" : "Show RGB"
    } else {
      label = _model.glyphMode == "cp437" ? "Show ASCII" : "Show CP437"
    }
    if (modeLabel == _modeAction && label == _displayAction) return
    var selected = _primary.focusedItem
    _modeAction = modeLabel
    _displayAction = label
    var items = [
      [modeLabel, Fn.new { _actions["mode"].call() }],
      [label, Fn.new { _actions["display"].call() }],
      ["Preview", Fn.new { _actions["preview"].call() }]
    ]
    if (_model.mode == "styles") {
      items.add(["Cascade", Fn.new { _actions["cascade"].call() }])
    }
    items.add(["Edit", Fn.new { _actions["edit"].call() }])
    items.add(["New", Fn.new { _actions["new"].call() }])
    items.add(["Remove", Fn.new { _actions["remove"].call() }])
    _primary.items = items
    if (selected != null) _primary.focusedItem = selected
  }

  bounds=(rect) {
    super.bounds = rect
    _primary.bounds = Rect.new(rect.x, rect.y, rect.w, 1)
    _secondary.bounds = Rect.new(rect.x, rect.y + 1, rect.w, 1)
    var bodyY = rect.y + 2
    var bodyH = (rect.h - 2).max(1)
    var inspectorW = (rect.w / 3).floor.max(24).min(rect.w - 20)
    var atlasW = (rect.w - inspectorW - 1).max(1)
    _atlas.bounds = Rect.new(rect.x, bodyY, atlasW, bodyH)
    _inspector.bounds = Rect.new(rect.x + atlasW + 1, bodyY,
        inspectorW, bodyH)
  }

  onPaint_() {
    Painter.fill(surface, Rect.new(0, 0, bounds.w, bounds.h), " ",
        style("default"))
    if (bounds.h > 2) {
      var separatorX = _atlas.bounds.right - bounds.x + 1
      Painter.vline(surface, separatorX, 2, bounds.h - 2,
          glyph("frame.display.left"), style("frame"))
    }
    compositeChildren_()
  }
}

class ThemeEditorPane is Pane {
  construct new(content, actions) {
    super()
    shadow = true
    _actions = actions
    add(content)
  }

  handle(event) {
    if (event is KeyEvent) {
      if (event.code == Key.escape) {
        _actions["close"].call()
        return true
      }
      if (event.code == Key.ctrlS) {
        _actions["save"].call()
        return true
      }
      if (event.code == Key.f2) {
        _actions["saveAs"].call()
        return true
      }
      if (event.code == Key.ctrlZ) {
        _actions["undo"].call()
        return true
      }
      if (event.code == Key.ctrlY) {
        _actions["redo"].call()
        return true
      }
      if (event.code == Key.insert) {
        _actions["new"].call()
        return true
      }
      if (event.code == Key.delete || event.code == Key.delChar) {
        _actions["remove"].call()
        return true
      }
    }
    return super.handle(event)
  }
}

class ThemeEditor {
  static run(app, document) {
    var editor = ThemeEditor.new_(app, document)
    editor[0].call()
    return editor[1]
  }

  static new_(app, document) {
    var model = ThemeEditorModel.new(document)
    var state = {"saved": false, "closed": false}
    var pane = null
    var content = null

    var refresh = Fn.new {
      model.refresh()
      content.refresh()
      pane.title = title_(document, model)
    }
    model.onChange = Fn.new {
      if (content != null) {
        content.refresh()
        pane.title = title_(document, model)
      }
    }

    var operation = Fn.new {|error|
      if (error != null) Alert.show(app, "Theme Editor", error)
      if (error == null) refresh.call()
      return error == null
    }

    var actions = {}
    actions["mode"] = Fn.new {
      model.mode = model.mode == "styles" ? "glyphs" : "styles"
    }
    actions["display"] = Fn.new {
      if (model.mode == "styles") {
        model.colorMode = model.colorMode == "rgb" ? "legacy" : "rgb"
      } else {
        model.glyphMode = model.glyphMode == "cp437" ? "ascii" : "cp437"
      }
    }
    actions["preview"] = Fn.new { showWidgetPreview_(app, model) }
    actions["cascade"] = Fn.new { showCascade_(app, model) }
    actions["edit"] = Fn.new {
      if (model.mode == "styles") {
        editStyle_(app, model, operation)
      } else {
        editGlyph_(app, model, operation)
      }
    }
    actions["new"] = Fn.new {
      if (model.mode == "styles") {
        addStyle_(app, document, operation)
      } else {
        addGlyph_(app, document, operation)
      }
    }
    actions["remove"] = Fn.new {
      removeSelected_(app, model, operation)
    }
    actions["metadata"] = Fn.new { editMetadata_(app, document, operation) }
    actions["import"] = Fn.new { import_(app, document, operation) }
    actions["undo"] = Fn.new {
      if (document.undo()) refresh.call()
    }
    actions["redo"] = Fn.new {
      if (document.redo()) refresh.call()
    }
    actions["save"] = Fn.new {
      if (save_(app, document, false)) {
        state["saved"] = true
        refresh.call()
      }
    }
    actions["saveAs"] = Fn.new {
      if (save_(app, document, true)) {
        state["saved"] = true
        refresh.call()
      }
    }
    actions["close"] = Fn.new {
      if (state["closed"]) return
      if (document.dirty && !confirmClose_(app, document)) return
      state["closed"] = true
      app.popModal()
    }

    content = ThemeEditorContent.new(model, actions)
    pane = ThemeEditorPane.new(content, actions)
    pane.title = title_(document, model)
    pane.helpText = helpText
    pane.keyHints = [["F1", "Help"], ["Enter", "Edit"],
        ["Ctrl-S", "Save"], ["F2", "Save As"], ["Esc", "Close"]]
    pane.focused = true
    pane.onClose = actions["close"]
    var size = Screen.size
    var x = size[0] >= 46 ? 3 : 1
    var y = size[1] >= 19 ? 2 : 1
    pane.bounds = Rect.new(x, y, (size[0] - x * 2).max(1),
        (size[1] - y - 1).max(1))
    content.bounds = pane.innerBounds
    content.refresh()

    var open = Fn.new { app.modal(pane) }
    return [open, state]
  }

  static title_(document, model) {
    var name = document.metadata(0)
    var dirty = document.dirty ? " *" : ""
    var mode
    if (model.mode == "styles") {
      mode = model.colorMode == "rgb" ? "Styles / RGB" : "Styles / Legacy"
    } else {
      mode = model.glyphMode == "cp437" ? "Glyphs / CP437" : "Glyphs / ASCII"
    }
    return "Theme Editor - %(name) [%(mode)]%(dirty)"
  }

  static showWidgetPreview_(app, model) {
    var close = Fn.new { app.popModal() }
    var selectRole = Fn.new {|role|
      if (model.selectStyle(role)) app.popModal()
    }
    var content = ThemeWidgetPreviewContent.new(model.theme, close,
        selectRole)
    var pane = ThemeWidgetPreviewPane.new(content, close)
    pane.title = "Theme Preview"
    pane.helpText = "# Theme Preview\n\nThis interactive preview uses " +
        "the unsaved draft theme on representative SyncTERM widgets. " +
        "Move focus with Tab or the arrow keys and operate the controls " +
        "normally. Press F2 to close the preview and select the style " +
        "role used by the focused widget. The outer frame retains the " +
        "installed theme so the preview can always be closed."
    pane.keyHints = [["F1", "Help"], ["Tab", "Next Control"],
        ["F2", "Select Role"], ["Esc", "Close"]]
    pane.focused = true
    pane.onClose = close
    var size = Screen.size
    var x = size[0] >= 50 ? 4 : 1
    var y = size[1] >= 18 ? 2 : 1
    pane.bounds = Rect.new(x, y, (size[0] - x * 2).max(1),
        (size[1] - y - 1).max(1))
    content.bounds = pane.innerBounds
    app.modal(pane)
  }

  static helpText {
    return "# Theme Editor\n\nThe preview atlas renders every style or " +
        "glyph in the draft theme. Select a row to inspect its direct " +
        "value and cascade source. The editor controls retain the installed " +
        "theme while the atlas previews unsaved changes.\n\n" +
        "Show Glyphs / Show Styles\n:  Change the kind of definition shown\n" +
        "Show Legacy / Show RGB\n:  Change the style colour preview\n" +
        "Show ASCII / Show CP437\n:  Change the glyph preview\n" +
        "Preview\n:  Apply the draft to representative interactive widgets; " +
        "F2 selects the focused widget's style role\n" +
        "Cascade\n:  Show every source considered for each style field\n" +
        "Edit\n:  Edit the selected definition\n" +
        "New / Remove\n:  Add or remove custom definitions\n" +
        "Metadata\n:  Edit theme name, author, description, and version\n" +
        "Import\n:  Replace all style and glyph definitions from an " +
        "installed theme\nUndo / Redo\n:  Traverse this editing session\n" +
        "Save / Save As\n:  Write the document without closing the editor"
  }

  static styleFieldLabel_(field) {
    if (field == 0) return "Font"
    if (field == 1) return "Legacy Attribute"
    if (field == 2) return "Foreground"
    return "Background"
  }

  static cascadeValue_(field, value) {
    if (value == null) return "inherit"
    if (field == 1) return hex_(value, 2)
    if (field == 2 || field == 3) return hex_(value, 6)
    return value.toString
  }

  static cascadeText_(model) {
    if (model.mode != "styles" || model.selectedName == null) {
      return "# Style Cascade\n\nSelect a style role first."
    }
    var text = "# %(model.selectedName)\n\n"
    for (field in 0..3) {
      text = text + "## %(styleFieldLabel_(field))\n\n"
      for (entry in model.styleCascade(model.selectedName, field)) {
        var marker = entry[3] ? "**winner**" : "considered"
        var value = entry[2] == null ? "" :
            ": `%(cascadeValue_(field, entry[2]))`"
        text = text + "* `%(entry[0])` - %(entry[1])%(value) " +
            "(%(marker))\n"
      }
      text = text + "\n"
    }
    return text
  }

  static showCascade_(app, model) {
    if (model.mode != "styles") {
      Alert.show(app, "Style Cascade",
          "Glyph definitions do not use the style cascade.")
      return
    }
    Help.show(app, "Style Cascade", cascadeText_(model))
  }

  static styleValue_(model, row, field) {
    var mode = model.directMode(row, field)
    if (mode == "inherit") return "inherit"
    var value = row[3 + field]
    if (mode == "default") return "default"
    if (field == 2 || field == 3) {
      return hex_(value, 6)
    }
    if (field == 1) return hex_(value, 2)
    return value.toString
  }

  static hex_(value, digits) {
    var chars = "0123456789ABCDEF"
    var text = ""
    var remaining = value
    if (remaining == 0) text = "0"
    while (remaining > 0) {
      text = chars[remaining % 16] + text
      remaining = (remaining / 16).floor
    }
    while (text.count < digits) text = "0" + text
    return "0x" + text
  }

  static editStyle_(app, model, operation) {
    while (true) {
      var row = model.selectedRow
      if (row == null) return
      var fields = model.colorMode == "rgb" ? [0, 2, 3] : [0, 1]
      var rows = []
      for (field in fields) {
        rows.add([field, "%(styleFieldLabel_(field)): " +
            styleValue_(model, row, field)])
      }
      var field = MenuUi.choice(app, row[0], rows, fields[0],
          "# Style\n\nChoose a direct field to edit. **Default** removes " +
          "the file override, while **Inherit** explicitly continues the " +
          "role cascade.")
      if (field == null) return
      var currentMode = model.directMode(row, field)
      var modes = [[0, "Default"]]
      if (row[0] != "default") modes.add([1, "Inherit"])
      modes.add([2, "Explicit"])
      var selectedMode = currentMode == "default" ? 0 :
          (currentMode == "inherit" ? 1 : 2)
      var pickedMode = MenuUi.choice(app, styleFieldLabel_(field), modes,
          selectedMode)
      if (pickedMode == null) continue
      var value = row[3 + field]
      if (pickedMode == 2) {
        var resolved = model.styleField(row[0], field)[0]
        if (field == 0) {
          value = MenuUi.integer(app, "Font", "Font Slot", resolved,
              0, 255)
        } else if (field == 1) {
          value = editLegacy_(app, resolved)
        } else {
          value = ThemeColorDialog.choose(app, styleFieldLabel_(field),
              resolved)
        }
        if (value == null) continue
      } else {
        value = 0
      }
      var error = model.document.setStyle(row[0], field, pickedMode, value)
      operation.call(error)
    }
  }

  static editLegacy_(app, value) {
    var foreground = value & 0x0f
    var background = (value >> 4) & 0x07
    var blink = (value & 0x80) != 0
    var colors = []
    for (i in 0...16) colors.add([i, Menu.colors[i]])
    foreground = MenuUi.choice(app, "Legacy Attribute", colors,
        foreground)
    if (foreground == null) return null
    var backgrounds = []
    for (i in 0...8) backgrounds.add([i, Menu.backgroundColors[i]])
    background = MenuUi.choice(app, "Legacy Background", backgrounds,
        background)
    if (background == null) return null
    var pickedBlink = MenuUi.choice(app, "Legacy Blink",
        [[0, "No Blink"], [1, "Blink"]], blink ? 1 : 0)
    if (pickedBlink == null) return null
    return foreground | (background << 4) | (pickedBlink == 1 ? 0x80 : 0)
  }

  static editGlyph_(app, model, operation) {
    while (true) {
      var row = model.selectedRow
      if (row == null) return
      var cpGlyph = model.glyphMode == "cp437" ?
          " '%(model.theme.glyphs[row[0]])'" : ""
      var rows = [
        [0, "CP437: %(hex_(row[3], 2))%(cpGlyph)"],
        [1, "ASCII: %(row[4]) '%(String.fromCodePoint(row[4]))'"]
      ]
      var field = MenuUi.choice(app, row[0], rows, 0,
          "# Glyph\n\nEdit either the CP437 code or its printable ASCII " +
          "fallback. Built-in values can be restored to their compiled " +
          "default.")
      if (field == null) return
      var present = (row[2] & (1 << field)) != 0
      var mode = 2
      if (row[1]) {
        mode = MenuUi.choice(app, field == 0 ? "CP437" : "ASCII",
            [[0, "Default"], [2, "Explicit"]], present ? 2 : 0)
        if (mode == null) continue
      }
      var value = row[3 + field]
      if (mode == 2) {
        value = ThemeGlyphPicker.choose(app,
            field == 0 ? "cp437" : "ascii", value)
        if (value == null) continue
      }
      operation.call(model.document.setGlyph(row[0], field, mode, value))
    }
  }

  static addStyle_(app, document, operation) {
    var name = MenuUi.prompt(app, "New Style", "Role", "", 80, false)
    if (name == null) return
    operation.call(document.addStyle(name))
  }

  static addGlyph_(app, document, operation) {
    var name = MenuUi.prompt(app, "New Glyph", "Name", "", 80, false)
    if (name == null) return
    var cp437 = ThemeGlyphPicker.choose(app, "cp437", 32)
    if (cp437 == null) return
    var ascii = ThemeGlyphPicker.choose(app, "ascii", 32)
    if (ascii == null) return
    operation.call(document.addGlyph(name, cp437, ascii))
  }

  static removeSelected_(app, model, operation) {
    var row = model.selectedRow
    if (row == null) return
    if (row[1]) {
      Alert.show(app, "Theme Editor",
          "Built-in definitions are restored field by field and cannot " +
          "be removed.")
      return
    }
    if (!Confirm.show(app, "Remove '%(row[0])' from this theme?")) return
    var error = model.mode == "styles" ?
        model.document.removeStyle(row[0]) :
        model.document.removeGlyph(row[0])
    operation.call(error)
  }

  static editMetadata_(app, document, operation) {
    var names = ["Name", "Author", "Description", "Version"]
    while (true) {
      var rows = []
      for (i in 0...names.count) {
        var value = document.metadata(i)
        var shown = value == null ? "" : value
        rows.add([i, "%(names[i]): %(shown)"])
      }
      var field = MenuUi.choice(app, "Theme Metadata", rows, 0,
          "# Theme Metadata\n\nName is required. Empty optional values are " +
          "removed from the theme file.")
      if (field == null) return
      var current = document.metadata(field)
      if (current == null) current = ""
      var value = MenuUi.prompt(app, "Theme Metadata", names[field],
          current, field == 2 ? 240 : 80, false)
      if (value == null) continue
      operation.call(document.setMetadata(field, value))
    }
  }

  static import_(app, document, operation) {
    var rows = []
    for (entry in Menu.themes) {
      if (entry[5] != null) continue
      var label = entry[1]
      if (entry[0].count > 0) label = "%(label) (%(entry[0]))"
      rows.add([entry[0], label])
    }
    var filename = MenuUi.choice(app, "Import Theme Definitions", rows,
        null, "# Import Theme Definitions\n\nImport replaces every style " +
        "and glyph definition in the draft as one undoable operation. " +
        "The target filename and metadata are retained.")
    if (filename == null) return
    operation.call(document.importTheme(filename))
  }

  static filename_(app, document) {
    var initial = document.filename
    var value = MenuUi.prompt(app, "Save Theme As", "Filename", initial,
        Menu.maxPathLength, false)
    if (value == null) return null
    var suffix = value.count < 4 ? "" : value[value.count - 4...value.count]
    if (!MenuUi.namesEqual(suffix, ".ini")) {
      value = value + ".ini"
    }
    return value
  }

  static save_(app, document, saveAs) {
    var filename = saveAs || document.filename.count == 0 ?
        filename_(app, document) : null
    if ((saveAs || document.filename.count == 0) && filename == null) {
      return false
    }
    var result = document.save(filename, false)
    if (result[0] == 2 || result[0] == 3) {
      var reason = result[0] == 2 ?
          "The file changed after it was opened." :
          "The destination file already exists."
      if (!Confirm.show(app, "%(reason) Replace it?")) return false
      result = document.save(filename, true)
    }
    if (result[0] != 0) {
      Alert.show(app, "Save Theme", result[1])
      return false
    }
    return true
  }

  static confirmClose_(app, document) {
    var prompt = ThemeClosePrompt.new(Fn.new { save_(app, document, false) })
    prompt.bounds = Popup.centeredBounds_(prompt.message, 1, 39)
    app.modal(prompt)
    if (prompt.result == null) return false
    return prompt.result == "save" || prompt.result == "discard"
  }
}
