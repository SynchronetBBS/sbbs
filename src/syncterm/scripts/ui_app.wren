// SyncTERM Wren UI library — App.
//
// App is the runtime root: owns a root Container, an optional modal
// stack, a keymap for global hotkeys, and the event pump that fans
// Input.nextEvent results out to the widget tree.  While App.run() is
// active, hooks are bypassed entirely (Input.nextEvent claims events
// before the hook chain in wren_host_dispatch_key).
//
// App is not itself a Widget but exposes the two methods that Widget
// tree walks call upward: `effectiveTheme` (returns App's theme) and
// `markDirty()` (no-op — App-level redraw is implicit in drainOnce_).
// Setting a Widget's parent to an App is therefore safe.
//
// Modal widgets:
//   var c = Confirm.new("Delete?")
//   var w = app.modal(c)            // blocks until c pops itself
//   if (w.result) { ... }
// Modal widgets pop themselves out of the stack via app.popModal()
// when the user dismisses them; app.modal() returns the same widget
// so callers can read whatever result it stored.
//
// Hook-callable: wrap the run() call in Fiber.new { app.run() }.call()
// per Hook.dispatch_'s contract — a hook handler must not yield
// directly, but may spawn a child fiber that runs an entire UI
// session and returns a value.

import "ui_style"  for Theme
import "ui_widget" for Widget, Container, Rect
import "ui_draw"   for Painter
import "syncterm"  for Input, KeyEvent, MouseEvent, Mouse, Timer,
                       TimerElapsed, Screen, Surface, CustomCursor

class App {
  construct new() {
    _root        = Container.new()
    _root.parent = this           // anchors tree-walks at the App
    _modalStack  = []
    _keymap      = {}
    _running     = false
    _theme       = Theme.default
    _tickMs      = null
    _surface     = null           // screen-sized backbuffer; lazy alloc
    _surfaceSize = null           // [w, h] last allocated for; reallocs on resize
    _savedCursor = null           // CustomCursor snapshot from entry; restored on exit
    _cursorShown = true            // mirrors the last applied state to skip redundant apply()s
  }

  root        { _root        }
  theme       { _theme       }
  theme=(t) {
    _theme = t
    markAllDirty_()
  }
  tickMs      { _tickMs      }
  tickMs=(ms) { _tickMs = ms }
  running     { _running     }
  modalStack  { _modalStack  }

  // The widget that owns the foreground — top of the modal stack if
  // any, else the root container.  All event dispatch starts here.
  modalTop {
    if (_modalStack.count > 0) return _modalStack[-1]
    return _root
  }

  // Pseudo-Widget surface for tree-walks.  Widget.effectiveTheme
  // walks up parent chains and stops at App; Widget.markDirty's
  // bubble terminates here too.
  effectiveTheme { _theme }
  markDirty() {}

  // Global hotkeys — consulted only when no widget consumes the key.
  // Keyed by the integer code from KeyEvent.code.
  bind(keyCode, fn) { _keymap[keyCode] = fn }
  unbind(keyCode)   { _keymap.remove(keyCode) }
  binding(keyCode)  { _keymap[keyCode] }

  pushModal(widget) {
    widget.parent = this
    _modalStack.add(widget)
    widget.markDirty()
    return widget
  }

  popModal() {
    if (_modalStack.count == 0) return null
    var w = _modalStack.removeAt(-1)
    w.parent = null
    markAllDirty_()
    return w
  }

  // Synchronous modal — push, drain events until the widget is no
  // longer on the stack.  The widget pops itself (typically from a
  // key handler that calls app.popModal()).  Returns the widget so
  // the caller can read whatever result it stored on itself.
  modal(widget) {
    pushModal(widget)
    while (_modalStack.indexOf(widget) >= 0) drainOnce_()
    return widget
  }

  quit() { _running = false }

  // Dispatch entries — public so tests (and external pumps) can
  // drive them directly without going through Input.nextEvent.

  // Key flow:
  //   1. modalTop.handle(ke) — focused-leaf-first via Container.
  //   2. App keymap lookup on the raw code.
  //   3. Otherwise drop.
  dispatchKey_(ke) {
    if (modalTop.handle(ke)) return true
    var fn = _keymap[ke.code]
    if (fn != null) {
      fn.call(ke)
      return true
    }
    return false
  }

  // Mouse flow:
  //   1. modalTop.hitTest(x, y) — deepest child covering the point.
  //      Containers descend; the last-added child wins ties.
  //   2. The hit widget gets handle(me).  No bubble — mouse events
  //      that miss the visible widget tree just drop.
  dispatchMouse_(me) {
    var top = modalTop
    if (top is Container) {
      var hit = top.hitTest(me.startX, me.startY)
      if (hit != null && hit.handle(me)) return true
    } else if (top.hit(me.startX, me.startY)) {
      return top.handle(me)
    }
    return false
  }

  // Internal: ensure a screen-sized backbuffer Surface, draw widgets
  // into their own Surfaces, composite each into the App's Surface,
  // then blit the App's Surface to the screen with a single putRect.
  // Order: root first, then modals top-down so the foreground wins
  // the final paint.
  drawAll_() {
    var sz = Screen.size
    if (_surface == null || _surfaceSize == null ||
        _surfaceSize[0] != sz[0] || _surfaceSize[1] != sz[1]) {
      _surface     = Surface.new(sz[0], sz[1])
      _surfaceSize = sz
    }
    if (_root.bounds == null) {
      _root.bounds = Rect.new(1, 1, sz[0], sz[1])
    }

    var rs = _root.draw()
    if (rs != null) {
      _surface.putRect(rs, _root.bounds.x - 1, _root.bounds.y - 1)
    }
    for (m in _modalStack) {
      var ms = m.draw()
      if (ms != null && m.bounds != null) {
        _surface.putRect(ms, m.bounds.x - 1, m.bounds.y - 1)
      }
    }
    Screen.putRect(_surface, 1, 1)
    // Park the hardware cursor wherever the foreground widget tree
    // wants it.  Putting this *after* the blit means the cursor lands
    // on top of the just-painted frame instead of where putRect last
    // happened to leave it.  Widgets that aren't text-input also flip
    // CustomCursor to "none" so backends that *can* hide it do — and
    // the position is still set so backends that can't show a sensible
    // cell instead of a stale one.
    var top = modalTop
    var cp  = top.cursorPos
    if (cp != null) Screen.window.position = cp
    var want = top.cursorVisible
    if (want != _cursorShown) {
      if (want) _savedCursor.apply() else CustomCursor.none.apply()
      _cursorShown = want
    }
  }

  markAllDirty_() {
    _root.markDirty()
    for (m in _modalStack) m.markDirty()
  }

  // One iteration of the event loop: paint, park, dispatch.  Public
  // (well, trailing-underscore-private) for the modal-loop reuse;
  // call run() for the full event loop.
  drainOnce_() {
    drawAll_()
    Input.nextEvent(Fiber.current)
    if (_tickMs != null) Timer.trigger(Fiber.current, _tickMs)
    var ev = Fiber.yield()
    if (ev is KeyEvent)     dispatchKey_(ev)
    if (ev is MouseEvent)   dispatchMouse_(ev)
    if (ev is TimerElapsed) onTick_()
  }

  // Save the terminal's current mouse-events bitmask, REPLACE it with
  // exactly the events a UI cares about (button-1 click + drag, wheel
  // up/down), and return the saved value so the caller restores on
  // exit.  Replacing rather than ORing matters: any events the
  // terminal had enabled for its own tracking would otherwise also
  // get queued during the App's run, which both adds dispatch noise
  // and changes how the terminal-side mouse-protocol parser sees
  // sequences (some events drive escape-sequence assembly).
  setupMouse_() {
    var saved = Input.mouseEvents
    Input.mouseEvents = 0
    Input.enableMouseEvent(Mouse.button1Press)
    Input.enableMouseEvent(Mouse.button1Click)
    Input.enableMouseEvent(Mouse.button1DragStart)
    Input.enableMouseEvent(Mouse.button1DragMove)
    Input.enableMouseEvent(Mouse.button1DragEnd)
    Input.enableMouseEvent(Mouse.wheelUpPress)
    Input.enableMouseEvent(Mouse.wheelDownPress)
    return saved
  }

  // Snapshot the cursor on entry so we can restore it on exit.
  // _cursorShown starts true so the first drawAll_ that wants the
  // cursor hidden actually applies CustomCursor.none.
  setupCursor_() {
    _savedCursor = CustomCursor.current
    _cursorShown = true
  }
  teardownCursor_() {
    if (_savedCursor != null) {
      _savedCursor.apply()
      _savedCursor = null
    }
  }

  run() {
    var savedMouse = setupMouse_()
    setupCursor_()
    _running = true
    while (_running) drainOnce_()
    teardownCursor_()
    Input.mouseEvents = savedMouse
  }

  // Synchronous variant: blocks the VM on Input.next() instead of
  // parking a fiber.  Use when calling from a context where doterm
  // can't dispatch back to the App (e.g. the Wren console's REPL,
  // which itself runs inside a Hook.onKey handler that blocks the
  // main loop until it returns).  Trade-off: Timer.trigger and
  // Hook.every don't fire while runSync is in its inner loop, so
  // dynamic widgets that depend on ticks won't refresh.  Use the
  // async run() for long-lived UI sessions where doterm needs to
  // keep flowing.
  runSync() {
    var savedMouse = setupMouse_()
    setupCursor_()
    _running = true
    while (_running) {
      drawAll_()
      var ev = Input.next()
      if (ev is KeyEvent)   dispatchKey_(ev)
      if (ev is MouseEvent) dispatchMouse_(ev)
    }
    teardownCursor_()
    Input.mouseEvents = savedMouse
  }

  // Default tick is a no-op.  Subclasses or App users override to
  // refresh dynamic content (transfer progress, blinking cursor, …).
  onTick_() {}
}
