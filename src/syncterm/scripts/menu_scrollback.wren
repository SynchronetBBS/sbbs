import "syncterm" for KeyEvent, MouseEvent, Key, Mouse
import "syncterm_menu" for Menu
import "ui_widget" for Widget, Rect
import "ui_draw" for Painter
import "menu_ui" for ModalPane

class OfflineScrollbackCanvas is Widget {
  construct new(source) {
    super()
    _source = source
    _top = (source.height - 1).max(0)
  }

  maxTop_ {
    if (bounds == null) return 0
    return (_source.height - bounds.h).max(0)
  }

  bounds=(value) {
    super.bounds = value
    if (_top > maxTop_) _top = maxTop_
  }

  move_(delta) {
    _top = (_top + delta).max(0).min(maxTop_)
    markDirty()
  }

  onPaint_() {
    Painter.fill(surface, Rect.new(0, 0, bounds.w, bounds.h), " ",
        style("default"))
    var width = _source.width.min(bounds.w)
    var rows = (_source.height - _top).min(bounds.h)
    if (width > 0 && rows > 0) {
      var x = ((bounds.w - width) / 2).floor
      surface.putRect(_source, Rect.new(0, _top, width, rows), x, 0)
    }
  }

  handle(event) {
    if (event is KeyEvent) {
      var key = event.code
      if (key == Key.up) {
        move_(-1)
      } else if (key == Key.down) {
        move_(1)
      } else if (key == Key.pageUp) {
        move_(-bounds.h)
      } else if (key == Key.pageDown) {
        move_(bounds.h)
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
      if (event.event == Mouse.wheelUpPress) {
        move_(-1)
        return true
      }
      if (event.event == Mouse.wheelDownPress) {
        move_(1)
        return true
      }
    }
    return false
  }
}

class OfflineScrollbackView {
  static show(app) {
    var source = Menu.offlineScrollback
    if (source == null) return
    var dismiss = Fn.new { app.popModal() }
    var pane = ModalPane.new(dismiss)
    pane.title = "Scrollback"
    pane.helpable = false
    pane.focused = true
    pane.bounds = app.root.bounds
    pane.onClose = dismiss
    var canvas = OfflineScrollbackCanvas.new(source)
    canvas.bounds = pane.innerBounds
    pane.add(canvas)
    app.modal(pane)
  }
}
