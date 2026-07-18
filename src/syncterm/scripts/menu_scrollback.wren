import "syncterm" for Clipboard, Hyperlinks, Input, KeyEvent, MouseEvent,
    Key, Mouse, Screen
import "syncterm_menu" for Menu
import "ui_widget" for Widget, Rect
import "ui_draw" for Painter
import "ui_popup" for Alert
import "ui_style" for Style

class OfflineScrollbackCanvas is Widget {
  construct new(app, source, background, hasStatus, dismiss) {
    super()
    _app = app
    _source = source
    _background = background
    _hasStatus = hasStatus
    _dismiss = dismiss
    _top = 0
    _hoverId = 0
  }

  terminalHeight_ {
    if (bounds == null) return 0
    return (bounds.h - (_hasStatus ? 1 : 0)).max(0)
  }

  maxTop_ { (_source.height - bounds.h).max(0) }

  bounds=(value) {
    super.bounds = value
    _top = maxTop_
  }

  sourceX_ { ((bounds.w - _source.width) / 2).floor }

  titleStyle_ {
    var base = style("title")
    return Style.new(base.font, base.legacyAttr + 0x80,
        base.fgRgb, base.bgRgb)
  }

  move_(delta) {
    _top = (_top + delta).max(0).min(maxTop_)
    markDirty()
  }

  title_(x) {
    if (bounds.w <= 0 || x >= bounds.w) return
    Painter.text(surface, x.max(0), 0, "Scrollback", titleStyle_,
        (bounds.w - x.max(0)).min(10))
  }

  onPaint_() {
    if (_background != null) {
      surface.putRect(_background, 0, 0)
    } else {
      Painter.fill(surface, Rect.new(0, 0, bounds.w, bounds.h), " ",
          style("default"))
    }
    var sourceX = (-sourceX_).max(0)
    var width = (_source.width - sourceX).min(bounds.w)
    var rows = (_source.height - _top).min(bounds.h)
    if (width > 0 && rows > 0) {
      surface.putRect(_source, Rect.new(sourceX, _top, width, rows),
          sourceX_.max(0), 0)
    }
    title_(0)
    title_(_source.width - 10)
  }

  position_(event) {
    var x = event.startX - bounds.x - sourceX_
    var y = event.startY - bounds.y
    if (x < 0 || x >= _source.width || y < 0 ||
        y >= bounds.h) return null
    var row = _top + y
    if (row < 0 || row >= _source.height) return null
    return [x, row]
  }

  copied_(url) {
    Clipboard.text = url
    Alert.show(_app, "URL copied to clipboard", url)
  }

  activate_(event) {
    var position = position_(event)
    if (position == null) return
    var cell = _source.cellAt(position[0], position[1])
    if (cell.hyperlinkId != 0) {
      if (!Menu.openHyperlink(cell.hyperlinkId)) {
        var url = Hyperlinks[cell.hyperlinkId]
        if (url != null) copied_(url)
      }
      return
    }
    var url = _source.urlAt(position[0], position[1])
    if (url != null && !Menu.openUrl(url)) copied_(url)
  }

  hover_(event) {
    var id = 0
    var position = position_(event)
    if (position != null) {
      id = _source.cellAt(position[0], position[1]).hyperlinkId
    }
    if (id == _hoverId) return
    _hoverId = id
    Menu.setHyperlinkHover(id)
  }

  handle(event) {
    if (event is KeyEvent) {
      var key = event.code
      var cp = event.codepoint
      if (key == Key.escape) {
        _dismiss.call()
      } else if (key == Key.up || cp == 0x4A || cp == 0x6A) {
        move_(-1)
      } else if (key == Key.down || cp == 0x4B || cp == 0x6B) {
        move_(1)
      } else if (key == Key.pageUp) {
        move_(-bounds.h)
      } else if (key == Key.pageDown) {
        move_(bounds.h)
      } else if (cp == 0x48 || cp == 0x68) {
        move_(-terminalHeight_)
      } else if (cp == 0x4C || cp == 0x6C) {
        move_(terminalHeight_)
      } else if (key == Key.home) {
        move_(-_source.height)
      } else if (key == Key.end) {
        move_(_source.height)
      } else {
        return false
      }
      return true
    }
    if (event is MouseEvent) {
      if (event.event == Mouse.wheelUpPress ||
          event.event == Mouse.wheelUpClick) {
        move_(-1)
        return true
      }
      if (event.event == Mouse.wheelDownPress ||
          event.event == Mouse.wheelDownClick) {
        move_(1)
        return true
      }
      if (event.event == Mouse.button1Click) {
        activate_(event)
        return true
      }
      if (event.event == Mouse.move) {
        hover_(event)
        return true
      }
      if (event.event == Mouse.button1DragStart) {
        Input.mousedrag()
        return true
      }
    }
    return false
  }
}

class OfflineScrollbackView {
  static help_() {
    return "# Scrollback Buffer\n\n" +
        "J / Up Arrow\n:  Scroll up one line\n" +
        "K / Down Arrow\n:  Scroll down one line\n" +
        "H / Page Up\n:  Scroll up one screen\n" +
        "L / Page Down\n:  Scroll down one screen\n" +
        "Mouse Wheel\n:  Scroll up or down one line\n" +
        "Click\n:  Open a hyperlink or detected URL\n" +
        "Drag\n:  Copy selected text\n" +
        "Escape\n:  Return to the directory"
  }

  static show(app) {
    var source = Menu.offlineScrollback
    if (source == null) return
    var savedScreen = Screen.save()
    if (savedScreen == 0) return
    if (!Menu.prepareOfflineScrollback()) {
      Screen.restore(savedScreen)
      return
    }
    var savedMouse = Input.mouseEvents
    Input.enableMouseEvent(Mouse.move)
    Input.enableMouseEvent(Mouse.button1Click)
    Input.enableMouseEvent(Mouse.button1DragStart)
    Input.enableMouseEvent(Mouse.button1DragMove)
    Input.enableMouseEvent(Mouse.button1DragEnd)
    Input.enableMouseEvent(Mouse.wheelUpPress)
    Input.enableMouseEvent(Mouse.wheelDownPress)
    Input.mouseVisible = true
    var background = Screen.readRect(1, 1, Screen.size[0], Screen.size[1])
    var dismiss = Fn.new { app.popModal() }
    var viewer = OfflineScrollbackCanvas.new(app, source, background,
        Menu.offlineScrollbackHasStatus, dismiss)
    viewer.helpText = help_()
    viewer.focused = true
    viewer.bounds = Rect.new(1, 1, Screen.size[0], Screen.size[1])
    app.modal(viewer)
    Menu.setHyperlinkHover(0)
    Input.mouseEvents = savedMouse
    Screen.restore(savedScreen)
  }
}
