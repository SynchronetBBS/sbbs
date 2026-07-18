// SyncTERM Wren UI library — App.
//
// App is the runtime root: owns a root Container, an optional modal
// stack, a keymap for global hotkeys, and a single run-fiber that
// pumps the event loop.  App.run pushes an Input claim that decides
// synchronously, from App state alone, whether each event is "for
// the App" — modal up consumes everything; with a focused widget,
// keys are claimed and mouse events are claimed when they hit the
// visible widget tree (or start a drag, which the App owns for
// rectangular select); otherwise the event falls through to lower
// claims and then to registered Hook.onKey / Hook.onMouse.  Claimed
// events are delivered to the run-fiber via Wake.post; the next
// drainOnce_ iteration picks them up and dispatches into the widget
// tree.  All widget handlers run in the run-fiber and may yield
// freely (modal pumps, SFTP awaits, etc.); the claim handler does
// not.
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
import "ui_popup"  for PopStatus
import "ui_help"   for Help
import "syncterm"  for Input, KeyEvent, MouseEvent, Mouse, Key, Timer,
                       TimerElapsed, Wake, Screen, Surface, CustomCursor, CTerm

// Sentinel passed into post() when the caller doesn't supply a value.
// Identity-comparable so the App can recognise "just wake up, nothing
// to dispatch" without confusing it for a user-supplied null payload.
class WakeOnly {
  static instance {
    if (__inst == null) __inst = WakeOnly.new()
    return __inst
  }
  construct new() {}
}

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
    _layoutSize  = null           // [w, h] last passed through layout_
    _backdrop    = null           // Screen.readRect snapshot from first paint
    _savedCursor = null           // CustomCursor snapshot from entry; restored on exit
    _cursorShape = null           // mirrors last applied preset; null forces first apply
    _runMode     = null           // "async" / "sync"; modal() uses to pick pump
    _status      = null           // PopStatus overlay or null
    _runFiber    = null           // set in run(); target for post() and the claim handler's wake
    _onPost      = null           // user-supplied posted-value handler
    _onLayout    = null           // called with (width, height) on resize
    _dragHandedOff = false        // see dispatchMouse_ for the belt this catches
    _claim         = null         // ClaimHandle; set by run(), popped on exit
    _tickPending   = false        // true while a Timer.trigger is queued for _runFiber
    _released      = false        // true while releaseFocus is in effect
    _releasedFocus = null         // saved focusedIndex (may be null if nothing was focused)
    // F1 → context help; user can rebind / unbind freely.
    bind(Key.f1, Fn.new {|k| showHelp() })
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
  onLayout=(fn) { _onLayout = fn }

  // Apply the effective screen bounds and notify layout clients only
  // when they change.  Public-with-underscore so pure Wren tests can
  // exercise resize behavior without painting the real screen.
  layout_(w, h) {
    var changed = _layoutSize == null || _layoutSize[0] != w ||
        _layoutSize[1] != h
    _layoutSize = [w, h]
    if (_root.bounds == null || _root.bounds.w != w ||
        _root.bounds.h != h || _root.bounds.x != 1 ||
        _root.bounds.y != 1) {
      _root.bounds = Rect.new(1, 1, w, h)
    }
    if (changed && _onLayout != null) _onLayout.call(w, h)
    return changed
  }

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
    return w
  }

  // Synchronous modal — push, drain events until the widget is no
  // longer on the stack.  The widget pops itself (typically from a
  // key handler that calls app.popModal()).  Returns the widget so
  // the caller can read whatever result it stored on itself.
  //
  // Picks the pump that matches the surrounding loop's mode: if the
  // App is in runSync (mode == "sync"), use drainOnceSync_ so we
  // keep blocking on Input.next() rather than yielding the fiber.
  // Yielding from inside a runSync would surface control back to the
  // hook caller (typically doterm), which causes the visible flash
  // back to the terminal.
  modal(widget) {
    pushModal(widget)
    while (_modalStack.indexOf(widget) >= 0) {
      if (_runMode == "sync") {
        drainOnceSync_()
      } else {
        drainOnce_()
      }
    }
    return widget
  }

  quit() { _running = false }

  // ----- External wake-up ----------------------------------------
  //
  // post() / post(value) — wake the App's parked fiber from outside
  // the Input.nextEvent / Timer.trigger paths.  The canonical use
  // case is a Hook.onInput callback that mutates a widget tree
  // (chat scrollback, ticker, etc.) and needs the App to repaint
  // even though the user hasn't pressed anything.  Hooks must run
  // synchronously, but `post` only queues — the resume is delivered
  // by the next main-loop drain, so the hook contract holds.
  //
  // post() with no args sends a sentinel that the App recognises as
  // "just wake up, redraw, no further dispatch."  post(value) sends
  // the supplied value; if `onPost` is wired the App calls it with
  // that value; otherwise the value is ignored after the redraw.
  //
  // No-op when the App isn't running (no parked fiber to deliver
  // to).  Safe to call from runSync()? — no.  runSync blocks on
  // Input.next() at the C level, not on Fiber.yield, so a wake
  // through the result queue won't reach it.  Use the async run()
  // for any app that needs external wake-ups.
  post()      { post(WakeOnly.instance) }
  post(value) {
    if (_runFiber == null) return
    Wake.post(_runFiber, value)
  }

  // Optional handler for posted values.  Called from drainOnce_
  // when the resumed value isn't a KeyEvent / MouseEvent /
  // TimerElapsed and isn't the WakeOnly sentinel.  The App always
  // redraws on a wake-up (drawAll_ runs at the top of each
  // drainOnce_), so onPost is only needed when the caller wants to
  // do extra work with the value.
  onPost     { _onPost }
  onPost=(fn) { _onPost = fn }

  // ----- Run a child fiber to completion --------------------------
  //
  // Spawn `fn` in a fresh fiber, pump the App's event loop from the
  // run-fiber until the child completes, return whatever `fn`
  // returned.  The child is the only thing parked on its SFTP /
  // Timer.trigger / Wake.post completions, so the wakes-meant-for-
  // someone-else hazard the run-fiber has (key/mouse/tickMs all
  // resume the run-fiber) doesn't apply here — only the child's own
  // results land on the child.  Meanwhile the run-fiber's own
  // events keep dispatching normally during the wait, so keystrokes,
  // ticks, and repaints are unaffected.
  //
  // Caller MUST be the run-fiber (i.e., this is sync-shaped code in
  // the App's run loop).  Modal calls (Alert.show, Confirm.show)
  // belong on the run-fiber side after this returns; the child fiber
  // shouldn't try to push modals (its drainOnce_ would yield the
  // child, not the App).
  runChild(fn) {
    var result = null
    var f = Fiber.new { result = fn.call() }
    f.try()
    while (!f.isDone) drainOnce_()
    return result
  }

  // ----- Status overlay ------------------------------------------
  //
  // popStatus(message) — show a transient frame with the message
  // centred on screen.  Doesn't block input; the caller does
  // whatever work it wants and dismisses with popStatus(null).
  // Forces an immediate paint so the overlay shows up before the
  // caller's blocking work proceeds — runSync/run wouldn't otherwise
  // redraw until the next event arrives.
  popStatus(message) {
    if (message == null) {
      _status = null
      markAllDirty_()
    } else {
      _status = PopStatus.show(message)
    }
    drawAll_()
  }

  // ----- Focus hand-off ------------------------------------------
  //
  // Wrap blocking host UIs that own the screen themselves (the
  // filepicker, modal C dialogs) so the App's foreground tree
  // doesn't *also* draw as focused.  Without this, the host UI's
  // pre-call save captures the App's panes in their focused colour
  // scheme, and the post-call restore re-paints them that way until
  // the next App-driven repaint — visually, two widgets are focused
  // at once.
  //
  //   app.releaseFocus()
  //   var picked = Host.pickFiles(...)
  //   app.restoreFocus()
  //
  // Idempotent on both sides — a second releaseFocus before
  // restoreFocus is a no-op, and restoreFocus without a prior
  // release is a no-op.  Operates on `modalTop` so a release while
  // a modal is up correctly defocuses the modal's leaf, not the
  // root's.  Forces an immediate paint so the unfocused state
  // lands on screen *before* the host UI's screen save.
  releaseFocus() {
    if (_released) return
    _released = true
    var top = modalTop
    if (!(top is Container)) return
    _releasedFocus = top.focusedIndex
    top.focusedIndex = null
    markAllDirty_()
    drawAll_()
  }
  restoreFocus() {
    if (!_released) return
    var top = modalTop
    if (top is Container) top.focusedIndex = _releasedFocus
    _released      = false
    _releasedFocus = null
    markAllDirty_()
    drawAll_()
  }

  // ----- Context help (F1) ---------------------------------------
  //
  // Walk from the foreground widget tree's focused leaf up through
  // its parent chain until we hit a Widget with helpText set, then
  // pop a Help dialog with that text.  No-op if nothing in the
  // chain has help.
  showHelp() {
    var w = modalTop
    // Descend to the focused leaf.
    while (w is Container && w.focusedChild != null) w = w.focusedChild
    var found = null
    while (w is Widget) {
      if (w.helpText != null) {
        found = w.helpText
        break
      }
      w = w.parent
    }
    if (found != null) Help.show(this, "Help", found)
  }

  // Synchronous claim decision — runs in the C dispatch fiber via
  // the claim handler installed by run().  No widget handlers, no
  // yields; queries App state only.  Modal up consumes everything;
  // otherwise the App claims input only when a widget is focused.
  // Mouse additionally requires a hit on the visible tree, with
  // drag-start always consumed because the App owns the rectangular-
  // select hand-off.  Falls through (returns false) for the
  // no-focused-widget case so an xeyes-style App can sit on screen
  // without intercepting events.
  shouldConsume_(ev) {
    if (_modalStack.count > 0) return true
    if (_root.focusedChild == null) return false
    if (ev is KeyEvent) return true
    if (ev is MouseEvent) {
      if (ev.event == Mouse.button1DragStart) return true
      return _root.hitTest(ev.startX, ev.startY) != null
    }
    return false
  }

  // Dispatch entries — public so tests (and external pumps) can
  // drive them directly.

  // Key flow:
  //   1. modalTop.handle(ke) — focused-leaf-first via Container.
  //   2. App keymap lookup on the raw code.
  //   3. Otherwise drop.
  dispatchKey_(ke) {
    if (modalTop.handle(ke)) return true
    if (ke.code == Key.backspace) {
      var escape = KeyEvent.new(Key.escape)
      if (modalTop.handle(escape)) return true
      var escapeFn = _keymap[Key.escape]
      if (escapeFn != null) {
        escapeFn.call(escape)
        return true
      }
      return false
    }
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
  //      that miss the visible widget tree just drop, EXCEPT that
  //      a button-1 drag-start with no taker hands off to the C-side
  //      `mousedrag` selector so the user gets the standard SyncTERM
  //      rectangle / line-copy UI even while a Wren App is up.
  //
  //   mousedrag is synchronous — it spins on getch/getmouse until the
  //   user releases (the release event fires its `default` arm which
  //   copies the selection and returns).  conio preserves `startx/y`
  //   across drag events so we don't have to ungetmouse the start —
  //   doing so would actually feed BUTTON_1_DRAG_START to mousedrag's
  //   `default` arm and exit on iteration 1 with an empty selection.
  //
  //   `_dragHandedOff` is a defensive belt: if a stray DRAG_END or
  //   release ever leaks into Wren's queue after mousedrag returned
  //   (e.g. backend race), we silently absorb it instead of dropping
  //   it on a widget that isn't expecting it.
  dispatchMouse_(me) {
    if (me.event == Mouse.button3Click) {
      dispatchKey_(KeyEvent.new(Key.escape))
      return true
    }
    if (_dragHandedOff &&
        (me.event == Mouse.button1DragEnd ||
         me.event == Mouse.button1Release)) {
      _dragHandedOff = false
      return true
    }
    var top = modalTop
    var outside = false
    if (top is Container) {
      var hit = top.hitTest(me.startX, me.startY)
      outside = hit == null
      if (hit != null && hit.handle(me)) return true
    } else if (top.hit(me.startX, me.startY)) {
      if (top.handle(me)) return true
    } else {
      outside = true
    }
    if (outside && _modalStack.count > 0 &&
        top.closesOnOutsideClick(me.event)) {
      dispatchKey_(KeyEvent.new(Key.escape))
      return true
    }
    if (me.event == Mouse.button1DragStart) {
      // Force rectangular select while a Wren App owns the screen.
      // Line-mode select makes no sense across widget chrome / cell-
      // aligned UI; users almost always want a rectangle.
      Input.mousedrag(true)
      _dragHandedOff = true
      return true
    }
    return false
  }

  // Internal: ensure a screen-sized backbuffer Surface, draw widgets
  // into their own Surfaces, composite each into the App's Surface,
  // then blit the App's Surface to the screen with a single putRect.
  // Order: root first, then modals top-down so the foreground wins
  // the final paint.
  drawAll_() {
    // Effective height excludes the SyncTERM status row when one is
    // present (statusDisplay > 0 = either the indicator bar or a
    // host-owned status_sub).  Without this, the App's surface and
    // its periodic blit would paint over the status row's contents
    // on every repaint, fighting with the C-side update_status path
    // and producing visible flicker on the transfer arrows / log
    // indicators.
    var sw      = Screen.size[0]
    var sh      = Screen.size[1]
    var hideRow = CTerm.statusDisplay > 0
    if (hideRow) sh = sh - 1
    var resized = _surface == null || _surfaceSize == null ||
        _surfaceSize[0] != sw || _surfaceSize[1] != sh
    if (resized) {
      _surface     = Surface.new(sw, sh)
      _surfaceSize = [sw, sh]
      _backdrop    = null
    }
    // Snapshot whatever was on screen before our first paint and use
    // it as the canvas behind every subsequent frame.  Areas not
    // covered by widgets show that snapshot rather than a styled
    // fill — UIFC convention: dialogs sit "on top of" the screen
    // they came from.
    if (_backdrop == null) {
      _backdrop = Screen.readRect(1, 1, sw, sh)
    }
    if (_backdrop != null) _surface.putRect(_backdrop, 0, 0)

    layout_(sw, sh)
    // Composite root's children straight onto the App surface.  Root
    // has no chrome of its own — it's a passthrough container — so
    // skipping its draw() lets the backdrop show through the gaps
    // between children.
    for (c in _root.children) {
      if (!c.visible) continue
      var cs = c.draw()
      if (cs == null || c.bounds == null) continue
      var dx = c.bounds.x - 1
      var dy = c.bounds.y - 1
      _surface.putRect(cs, dx, dy)
      if (c.shadow) {
        Painter.shadow(_surface, dx, dy, c.bounds.w, c.bounds.h)
      }
    }
    // Status overlay sits above the foreground widget tree but
    // *below* the modal stack — a "Working..." indicator is fine to
    // hide behind a dialog the user is actively working with.
    // popStatus(null) clears it.
    if (_status != null) {
      var ss = _status.draw()
      if (ss != null && _status.bounds != null) {
        var dx = _status.bounds.x - 1
        var dy = _status.bounds.y - 1
        _surface.putRect(ss, dx, dy)
        if (_status.shadow) {
          Painter.shadow(_surface, dx, dy, _status.bounds.w, _status.bounds.h)
        }
      }
    }
    for (m in _modalStack) {
      var ms = m.draw()
      if (ms != null && m.bounds != null) {
        var dx = m.bounds.x - 1
        var dy = m.bounds.y - 1
        _surface.putRect(ms, dx, dy)
        if (m.shadow) Painter.shadow(_surface, dx, dy, m.bounds.w, m.bounds.h)
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
    var want = top.cursorVisible ? top.cursorShape : "none"
    if (want == null) want = "normal"
    if (want != _cursorShape) {
      // The saved state is for restoring on exit; using it here would
      // inherit whatever the parent app left the cursor at, which could
      // itself be hidden or have unrelated geometry.
      if (want == "solid") {
        CustomCursor.solid.apply()
      } else if (want == "normal") {
        CustomCursor.normal.apply()
      } else {
        CustomCursor.none.apply()
      }
      _cursorShape = want
    }
  }

  markAllDirty_() {
    _root.markDirty()
    for (m in _modalStack) m.markDirty()
  }

  // One iteration of the event loop: paint, park, dispatch.  All
  // event handling lives in this fiber — the claim handler only
  // decides consumption and posts a wake; KeyEvent / MouseEvent
  // dispatch into the widget tree happens here, where handlers may
  // yield freely.  Used by run()'s outer loop and by modal()'s
  // nested pump.
  drainOnce_() {
    drawAll_()
    // Only schedule a tick when one isn't already pending.  Without
    // this gate, every non-tick wake (key, mouse, app.post()) would
    // queue a fresh timer while the previous one is still in
    // wren_host's pending-timer array, blowing past WREN_HOST_MAX_TIMERS
    // within a few keystrokes.
    if (_tickMs != null && !_tickPending) {
      Timer.trigger(Fiber.current, _tickMs)
      _tickPending = true
    }
    var ev = Fiber.yield()
    if (ev is TimerElapsed) {
      _tickPending = false
      onTick_()
      return
    }
    if (ev is KeyEvent) {
      dispatchKey_(ev)
      return
    }
    if (ev is MouseEvent) {
      dispatchMouse_(ev)
      return
    }
    // Posted via app.post() with a non-event value.  WakeOnly is
    // the no-payload "just repaint" signal (redraw is automatic at
    // the top of the next iteration); other values flow through
    // the optional onPost handler.
    if (ev != WakeOnly.instance && _onPost != null) _onPost.call(ev)
  }

  // Synchronous-mode drain: paint, block on Input.next(), dispatch.
  // No Fiber.yield — used by runSync and by modal() when invoked
  // inside a runSync context.
  drainOnceSync_() {
    drawAll_()
    var ev = Input.next()
    if (ev is KeyEvent)   dispatchKey_(ev)
    if (ev is MouseEvent) dispatchMouse_(ev)
  }

  // Save the terminal's current mouse-events bitmask, REPLACE it with
  // exactly the events a UI cares about (button-1 click + drag,
  // middle-click paste, right-click cancel, and wheel up/down), and
  // return the saved value so the caller restores on
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
    Input.enableMouseEvent(Mouse.button2Click)
    Input.enableMouseEvent(Mouse.button3Click)
    Input.enableMouseEvent(Mouse.wheelUpPress)
    Input.enableMouseEvent(Mouse.wheelDownPress)
    return saved
  }

  // Snapshot the cursor on entry so we can restore it on exit.
  // _cursorShape is null so the first drawAll_ comparison always trips,
  // regardless of what the parent app or terminal
  // had the cursor doing.
  setupCursor_() {
    _savedCursor = CustomCursor.current
    _cursorShape = null
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
    _runMode     = "async"
    _running     = true
    _backdrop    = null            // recapture screen on first paint
    _runFiber    = Fiber.current   // claim handler posts wakes here
    _tickPending = false           // any prior run's timers fire to a dead fiber
    // Push an input claim.  The handler runs synchronously in the
    // C dispatch frame (via Hook.dispatch_'s wrap, same as any
    // hook); it queries App state via shouldConsume_ to decide
    // whether to claim the event, posts the event to _runFiber via
    // Wake.post when claimed, and returns the consumed bool the
    // C dispatcher needs.  The handler does no widget work and
    // never yields — actual dispatch happens in this fiber from
    // drainOnce_, where handlers are free to yield.
    //
    // When run() exits, the claim is popped explicitly; the
    // ClaimHandle's foreign finalizer pops on GC as a safety net
    // if cleanup is ever skipped (aborted run, etc.).
    _claim = Input.pushClaim(Fn.new {|ev|
      if (!shouldConsume_(ev)) return false
      Wake.post(_runFiber, ev)
      return true
    })
    while (_running) drainOnce_()
    _claim.pop()
    _claim    = null
    _runMode  = null
    _backdrop = null
    _runFiber = null
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
    _runMode  = "sync"
    _running  = true
    _backdrop = null            // recapture screen on first paint
    while (_running) drainOnceSync_()
    _runMode  = null
    _backdrop = null
    teardownCursor_()
    Input.mouseEvents = savedMouse
  }

  // Default tick is a no-op.  Subclasses or App users override to
  // refresh dynamic content (transfer progress, blinking cursor, …).
  onTick_() {}
}
