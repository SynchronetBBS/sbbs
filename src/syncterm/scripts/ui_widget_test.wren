// Self-tests for ui_widget + ui_app.  Covers Rect math, Widget
// theme/focus/dirty/hit semantics, Container focus traversal +
// hit-testing + event routing, and App modal stack + dispatch
// (drives dispatchKey_/dispatchMouse_ directly so no event-pump
// fiber is required).
//
// Pure in-process; no connection needed.  Run via Alt+T (auto) or:
//   import "ui_widget_test" for UiWidgetTest
//   UiWidgetTest.run()

import "ui_style"  for Style, Theme
import "ui_widget" for Rect, Widget, Container
import "ui_app"    for App
import "syncterm"  for KeyEvent, MouseEvent, Key

// A Widget subclass that records every event it sees, optionally
// consumes them, and counts draw() invocations.  Focus and bounds
// are configured via the public Widget setters.
class Probe is Widget {
  construct new() {
    super()
    _seen      = []
    _consume   = false
    _drawCount = 0
  }
  seen        { _seen }
  consume     { _consume }
  consume=(b) { _consume = b }
  drawCount   { _drawCount }

  draw() {
    _drawCount = _drawCount + 1
    clearDirty()
  }

  handle(ev) {
    _seen.add(ev)
    return _consume
  }
}

class UiWidgetTest {
  static run() {
    __pass = 0
    __fail = 0
    System.print("=== ui_widget self-test starting ===")

    // Rect
    testRectConstruct_()
    testRectInclusiveCorners_()
    testRectContainsInside_()
    testRectContainsEdges_()
    testRectContainsOutside_()
    testRectEquality_()

    // Widget
    testWidgetDefaults_()
    testWidgetBoundsSetter_()
    testWidgetThemeOverride_()
    testWidgetEffectiveThemeFallsBack_()
    testWidgetEffectiveThemeInheritsParent_()
    testWidgetStyleAndGlyphForward_()
    testWidgetMarkDirtyPropagates_()
    testWidgetHitWithoutBounds_()
    testWidgetHitInvisible_()
    testWidgetHitContains_()
    testWidgetHandleDefaultFalse_()

    // Container — composition
    testContainerAddSetsParent_()
    testContainerAddFocusesFirstFocusable_()
    testContainerAddSkipsNonFocusable_()
    testContainerRemoveClearsParent_()
    testContainerRemoveAdjustsFocusIndex_()

    // Container — focus traversal
    testContainerFocusNext_()
    testContainerFocusPrev_()
    testContainerFocusWraps_()
    testContainerFocusSkipsNonFocusable_()
    testContainerFocusEmptyNoOp_()
    testContainerFocusNoFocusableNoOp_()
    testContainerFocusSingleFocusableReturnsFalse_()

    // Container — event routing
    testContainerHandleRoutesToFocused_()
    testContainerHandleBubblesIfNotConsumed_()
    testContainerHandleTabSteps_()
    testContainerHandleBackTabSteps_()
    testContainerHandleConsumedNoTab_()

    // Container — hit testing
    testContainerHitTestDeepest_()
    testContainerHitTestReturnsContainerIfNoChild_()
    testContainerHitTestOutside_()
    testContainerHitTestSkipsInvisible_()

    // App
    testAppConstruction_()
    testAppRootParented_()
    testAppEffectiveTheme_()
    testAppThemeChange_()
    testAppPushPopModal_()
    testAppPopEmptyReturnsNull_()
    testAppModalTopFallsBackToRoot_()
    testAppModalWidgetInheritsTheme_()
    testAppKeymapBindUnbind_()
    testAppDispatchKeyToFocused_()
    testAppDispatchKeyHitsKeymap_()
    testAppDispatchKeyConsumedSkipsKeymap_()
    testAppDispatchKeyModalBlocksRoot_()
    testAppDispatchMouseHitsWidget_()
    testAppDispatchMouseModalBlocksRoot_()
    testAppDispatchMouseOutsideDrops_()

    var total = __pass + __fail
    System.print("=== ui_widget: %(total) tests, %(__pass) pass, %(__fail) fail ===")
    return [__pass, __fail]
  }

  static check_(ok, label) {
    if (ok) {
      __pass = __pass + 1
      System.print("  PASS %(label)")
    } else {
      __fail = __fail + 1
      System.print("  FAIL %(label)")
    }
  }

  // ----- Rect -----------------------------------------------------

  static testRectConstruct_() {
    var r = Rect.new(3, 5, 10, 4)
    check_(r.x == 3 && r.y == 5 && r.w == 10 && r.h == 4,
           "Rect.new + getters")
  }

  static testRectInclusiveCorners_() {
    var r = Rect.new(3, 5, 10, 4)
    check_(r.right == 12 && r.bottom == 8,
           "Rect.right/bottom inclusive")
  }

  static testRectContainsInside_() {
    var r = Rect.new(3, 5, 10, 4)
    check_(r.contains(7, 6), "Rect.contains: interior point")
  }

  static testRectContainsEdges_() {
    var r = Rect.new(3, 5, 10, 4)
    check_(r.contains(3, 5)   && r.contains(12, 8) &&
           r.contains(3, 8)   && r.contains(12, 5),
           "Rect.contains: all four corners (inclusive)")
  }

  static testRectContainsOutside_() {
    var r = Rect.new(3, 5, 10, 4)
    check_(!r.contains(2, 6)  && !r.contains(13, 6) &&
           !r.contains(7, 4)  && !r.contains(7, 9),
           "Rect.contains: rejects points one past each edge")
  }

  static testRectEquality_() {
    check_(Rect.new(1, 2, 3, 4) == Rect.new(1, 2, 3, 4),
           "Rect equality")
    check_(Rect.new(1, 2, 3, 4) != Rect.new(1, 2, 3, 5),
           "Rect inequality")
    check_(!(Rect.new(1, 2, 3, 4) == "Rect"),
           "Rect equality rejects non-Rect")
  }

  // ----- Widget ---------------------------------------------------

  static testWidgetDefaults_() {
    var w = Probe.new()
    check_(w.bounds == null && w.parent == null && w.theme == null &&
           !w.focused && w.visible && w.dirty && w.focusable,
           "Widget defaults")
  }

  static testWidgetBoundsSetter_() {
    var w = Probe.new()
    w.draw()              // clear dirty
    check_(!w.dirty, "Widget.draw clears dirty (sanity)")
    w.bounds = Rect.new(1, 1, 10, 5)
    check_(w.bounds.w == 10 && w.dirty,
           "Widget.bounds= sets bounds and re-dirties")
  }

  static testWidgetThemeOverride_() {
    var w = Probe.new()
    var t = Theme.default
    w.theme = t
    check_(w.theme == t && w.effectiveTheme == t,
           "Widget.theme= and effectiveTheme returns own theme")
  }

  static testWidgetEffectiveThemeFallsBack_() {
    var w = Probe.new()
    check_(w.effectiveTheme == Theme.default,
           "Widget.effectiveTheme: no parent + no override → Theme.default")
  }

  static testWidgetEffectiveThemeInheritsParent_() {
    var custom = Theme.new({
      "default": Style.new(0, 0x07, 0xCCCCCC, 0x000000)
    }, {})
    var c = Container.new()
    c.theme = custom
    var leaf = Probe.new()
    c.add(leaf)
    check_(leaf.effectiveTheme == custom,
           "Widget.effectiveTheme: leaf walks up to parent's theme")
  }

  static testWidgetStyleAndGlyphForward_() {
    var w = Probe.new()
    var d = Theme.default
    var sExpected = d.style("default")
    var s = w.style("default")
    check_(s == sExpected && w.glyph("frame.topLeft") == d.glyphs["frame.topLeft"],
           "Widget.style/glyph forward to effectiveTheme")
  }

  static testWidgetMarkDirtyPropagates_() {
    var c = Container.new()
    var leaf = Probe.new()
    c.add(leaf)
    c.draw()  // clears container's dirty AND leaf's via traversal
    leaf.markDirty()
    check_(leaf.dirty && c.dirty,
           "Widget.markDirty propagates up the parent chain")
  }

  static testWidgetHitWithoutBounds_() {
    var w = Probe.new()
    check_(!w.hit(5, 5), "Widget.hit: no bounds → no hit")
  }

  static testWidgetHitInvisible_() {
    var w = Probe.new()
    w.bounds  = Rect.new(1, 1, 10, 5)
    w.visible = false
    check_(!w.hit(2, 2), "Widget.hit: invisible → no hit")
  }

  static testWidgetHitContains_() {
    var w = Probe.new()
    w.bounds = Rect.new(1, 1, 10, 5)
    check_(w.hit(2, 2) && !w.hit(11, 2),
           "Widget.hit uses bounds.contains")
  }

  static testWidgetHandleDefaultFalse_() {
    var w = Widget.new()
    check_(!w.handle("anything"), "Widget.handle defaults to false")
  }

  // ----- Container — composition ----------------------------------

  static testContainerAddSetsParent_() {
    var c = Container.new()
    var w = Probe.new()
    c.add(w)
    check_(w.parent == c && c.children.count == 1 && c.children[0] == w,
           "Container.add appends child + sets parent")
  }

  static testContainerAddFocusesFirstFocusable_() {
    var c = Container.new()
    var a = Probe.new()
    var b = Probe.new()
    c.add(a)
    c.add(b)
    check_(c.focusedChild == a && a.focused && !b.focused,
           "Container.add: first focusable child gets focus")
  }

  static testContainerAddSkipsNonFocusable_() {
    var c = Container.new()
    var a = Probe.new()
    a.focusable = false
    var b = Probe.new()
    c.add(a)
    c.add(b)
    check_(c.focusedChild == b && !a.focused && b.focused,
           "Container.add: skips non-focusable for initial focus")
  }

  static testContainerRemoveClearsParent_() {
    var c = Container.new()
    var w = Probe.new()
    c.add(w)
    c.remove(w)
    check_(w.parent == null && c.children.count == 0,
           "Container.remove clears parent + drops from list")
  }

  static testContainerRemoveAdjustsFocusIndex_() {
    var c = Container.new()
    var a = Probe.new()
    var b = Probe.new()
    var d = Probe.new()
    c.add(a)
    c.add(b)
    c.add(d)
    c.focusNext()                     // a → b
    check_(c.focusedChild == b, "sanity: focus on b after focusNext")
    c.remove(a)                       // remove pre-focused → index shifts
    check_(c.focusedChild == b,
           "Container.remove: pre-focused removal shifts index, focus stable")
  }

  // ----- Container — focus traversal ------------------------------

  static testContainerFocusNext_() {
    var c = Container.new()
    var a = Probe.new()
    var b = Probe.new()
    c.add(a)
    c.add(b)
    c.focusNext()
    check_(c.focusedChild == b && !a.focused && b.focused,
           "Container.focusNext: a → b")
  }

  static testContainerFocusPrev_() {
    var c = Container.new()
    var a = Probe.new()
    var b = Probe.new()
    c.add(a)
    c.add(b)
    c.focusPrev()
    check_(c.focusedChild == b,
           "Container.focusPrev from a wraps backward to b")
  }

  static testContainerFocusWraps_() {
    var c = Container.new()
    var a = Probe.new()
    var b = Probe.new()
    c.add(a)
    c.add(b)
    c.focusNext()           // a → b
    c.focusNext()           // b → a (wrap)
    check_(c.focusedChild == a, "Container.focusNext wraps")
  }

  static testContainerFocusSkipsNonFocusable_() {
    var c = Container.new()
    var a = Probe.new()
    var b = Probe.new()
    b.focusable = false
    var d = Probe.new()
    c.add(a)
    c.add(b)
    c.add(d)
    c.focusNext()
    check_(c.focusedChild == d, "Container.focusNext skips non-focusable")
  }

  static testContainerFocusEmptyNoOp_() {
    var c = Container.new()
    check_(!c.focusNext() && !c.focusPrev(),
           "Container.focusNext/Prev no-op on empty container")
  }

  static testContainerFocusNoFocusableNoOp_() {
    var c = Container.new()
    var a = Probe.new()
    a.focusable = false
    var b = Probe.new()
    b.focusable = false
    c.add(a)
    c.add(b)
    check_(!c.focusNext() && c.focusedChild == null,
           "Container.focusNext: no focusable child, no-op")
  }

  // Single focusable child → focusNext/focusPrev must return false so
  // the parent Container can route Tab upward.  Without this, a
  // nested Pane with one child traps Tab inside itself.
  static testContainerFocusSingleFocusableReturnsFalse_() {
    var c = Container.new()
    var a = Probe.new()
    a.focusable = false
    var b = Probe.new()       // the one focusable child
    var d = Probe.new()
    d.focusable = false
    c.add(a)
    c.add(b)
    c.add(d)
    // First focusNext finds b (was unfocused) — ok, returns true.
    // Now we're focused on the only focusable child; the next call
    // must return false so the Tab bubbles to the parent.
    var moved = c.focusNext()
    check_(moved == false && c.focusedChild == b,
           "Container.focusNext: single focusable returns false, focus stays")
    var movedBack = c.focusPrev()
    check_(movedBack == false && c.focusedChild == b,
           "Container.focusPrev: single focusable returns false, focus stays")
  }

  // ----- Container — event routing --------------------------------

  static testContainerHandleRoutesToFocused_() {
    var c = Container.new()
    var a = Probe.new()
    a.consume = true
    var b = Probe.new()
    b.consume = true
    c.add(a)
    c.add(b)
    var ev = KeyEvent.new(Key.enter)
    var consumed = c.handle(ev)
    check_(consumed && a.seen.count == 1 && b.seen.count == 0,
           "Container.handle routes to focused child first")
  }

  static testContainerHandleBubblesIfNotConsumed_() {
    var c = Container.new()
    var a = Probe.new()           // consume = false
    c.add(a)
    var ev = KeyEvent.new(Key.enter)
    var consumed = c.handle(ev)
    check_(!consumed && a.seen.count == 1,
           "Container.handle: non-Tab key not consumed → returns false")
  }

  static testContainerHandleTabSteps_() {
    var c = Container.new()
    var a = Probe.new()
    var b = Probe.new()
    c.add(a)
    c.add(b)
    var consumed = c.handle(KeyEvent.new(Key.tab))
    check_(consumed && c.focusedChild == b,
           "Container.handle Tab steps focus when leaf doesn't consume")
  }

  static testContainerHandleBackTabSteps_() {
    var c = Container.new()
    var a = Probe.new()
    var b = Probe.new()
    c.add(a)
    c.add(b)
    var consumed = c.handle(KeyEvent.new(Key.backTab))
    check_(consumed && c.focusedChild == b,    // wraps backward from a
           "Container.handle BackTab steps focus")
  }

  static testContainerHandleConsumedNoTab_() {
    var c = Container.new()
    var a = Probe.new()
    a.consume = true
    var b = Probe.new()
    c.add(a)
    c.add(b)
    c.handle(KeyEvent.new(Key.tab))
    check_(c.focusedChild == a,
           "Container.handle: leaf consuming Tab prevents focus step")
  }

  // ----- Container — hit testing ----------------------------------

  static testContainerHitTestDeepest_() {
    var outer = Container.new()
    outer.bounds = Rect.new(1, 1, 40, 20)
    var leaf = Probe.new()
    leaf.bounds = Rect.new(5, 5, 10, 5)
    outer.add(leaf)
    check_(outer.hitTest(7, 7) == leaf,
           "Container.hitTest: descends to deepest covering leaf")
  }

  static testContainerHitTestReturnsContainerIfNoChild_() {
    var outer = Container.new()
    outer.bounds = Rect.new(1, 1, 40, 20)
    var leaf = Probe.new()
    leaf.bounds = Rect.new(5, 5, 10, 5)
    outer.add(leaf)
    // Inside outer, outside the only leaf.
    check_(outer.hitTest(20, 15) == outer,
           "Container.hitTest: point inside container, outside any child")
  }

  static testContainerHitTestOutside_() {
    var outer = Container.new()
    outer.bounds = Rect.new(1, 1, 40, 20)
    check_(outer.hitTest(50, 50) == null,
           "Container.hitTest: point outside container → null")
  }

  static testContainerHitTestSkipsInvisible_() {
    var outer = Container.new()
    outer.bounds = Rect.new(1, 1, 40, 20)
    var leaf = Probe.new()
    leaf.bounds  = Rect.new(5, 5, 10, 5)
    leaf.visible = false
    outer.add(leaf)
    check_(outer.hitTest(7, 7) == outer,
           "Container.hitTest: invisible leaf skipped, falls back to container")
  }

  // ----- App ------------------------------------------------------

  static testAppConstruction_() {
    var app = App.new()
    check_(app.root is Container && app.modalStack.count == 0 &&
           !app.running && app.tickMs == null,
           "App.new: defaults")
  }

  static testAppRootParented_() {
    var app = App.new()
    check_(app.root.parent == app,
           "App.new: root container is parented to the App")
  }

  static testAppEffectiveTheme_() {
    var app = App.new()
    check_(app.effectiveTheme == Theme.default,
           "App.effectiveTheme defaults to Theme.default")
  }

  static testAppThemeChange_() {
    var app = App.new()
    var t = Theme.new({
      "default": Style.new(0, 0x07, 0xCCCCCC, 0x000000)
    }, {})
    app.theme = t
    var leaf = Probe.new()
    app.root.add(leaf)
    check_(app.effectiveTheme == t && leaf.effectiveTheme == t,
           "App.theme= propagates to widgets via tree walk")
  }

  static testAppPushPopModal_() {
    var app = App.new()
    var w = Probe.new()
    app.pushModal(w)
    check_(app.modalTop == w && w.parent == app, "App.pushModal sets parent + top")
    var popped = app.popModal()
    check_(popped == w && w.parent == null && app.modalTop == app.root,
           "App.popModal clears parent + restores root as top")
  }

  static testAppPopEmptyReturnsNull_() {
    var app = App.new()
    check_(app.popModal() == null, "App.popModal on empty stack → null")
  }

  static testAppModalTopFallsBackToRoot_() {
    var app = App.new()
    check_(app.modalTop == app.root,
           "App.modalTop falls back to root when stack is empty")
  }

  static testAppModalWidgetInheritsTheme_() {
    var app = App.new()
    var t = Theme.new({
      "default": Style.new(0, 0x07, 0xCCCCCC, 0x000000)
    }, {})
    app.theme = t
    var w = Probe.new()
    app.pushModal(w)
    check_(w.effectiveTheme == t,
           "App: pushed modal inherits app theme via parent walk")
  }

  static testAppKeymapBindUnbind_() {
    var app = App.new()
    var fn = Fn.new {|k| true }
    app.bind(Key.f1, fn)
    check_(app.binding(Key.f1) == fn, "App.bind stores handler")
    app.unbind(Key.f1)
    check_(app.binding(Key.f1) == null, "App.unbind removes handler")
  }

  static testAppDispatchKeyToFocused_() {
    var app  = App.new()
    var leaf = Probe.new()
    leaf.consume = true
    app.root.add(leaf)
    var consumed = app.dispatchKey_(KeyEvent.new(Key.enter))
    check_(consumed && leaf.seen.count == 1,
           "App.dispatchKey_ routes to focused child")
  }

  static testAppDispatchKeyHitsKeymap_() {
    var app   = App.new()
    var fired = [false]
    app.bind(Key.f1, Fn.new {|k| fired[0] = true })
    var consumed = app.dispatchKey_(KeyEvent.new(Key.f1))
    check_(consumed && fired[0],
           "App.dispatchKey_ hits keymap when no widget consumes")
  }

  static testAppDispatchKeyConsumedSkipsKeymap_() {
    var app   = App.new()
    var leaf  = Probe.new()
    leaf.consume = true
    app.root.add(leaf)
    var fired = [false]
    app.bind(Key.enter, Fn.new {|k| fired[0] = true })
    app.dispatchKey_(KeyEvent.new(Key.enter))
    check_(!fired[0],
           "App.dispatchKey_: widget consumption short-circuits keymap")
  }

  static testAppDispatchKeyModalBlocksRoot_() {
    var app  = App.new()
    var rootLeaf = Probe.new()
    rootLeaf.consume = true
    app.root.add(rootLeaf)
    var modal = Probe.new()
    modal.consume = true
    app.pushModal(modal)
    app.dispatchKey_(KeyEvent.new(Key.enter))
    check_(modal.seen.count == 1 && rootLeaf.seen.count == 0,
           "App.dispatchKey_: modal blocks root")
  }

  // Mouse event constructor: (event, modifiers, sx, sy, ex, ey).
  static mouse_(x, y) { MouseEvent.new(0, 0, x, y, x, y) }

  static testAppDispatchMouseHitsWidget_() {
    var app  = App.new()
    app.root.bounds = Rect.new(1, 1, 80, 25)
    var leaf = Probe.new()
    leaf.bounds = Rect.new(10, 5, 20, 3)
    leaf.consume = true
    app.root.add(leaf)
    var consumed = app.dispatchMouse_(mouse_(15, 6))
    check_(consumed && leaf.seen.count == 1,
           "App.dispatchMouse_: hits widget at coords")
  }

  static testAppDispatchMouseModalBlocksRoot_() {
    var app  = App.new()
    app.root.bounds = Rect.new(1, 1, 80, 25)
    var rootLeaf = Probe.new()
    rootLeaf.bounds = Rect.new(10, 5, 20, 3)
    rootLeaf.consume = true
    app.root.add(rootLeaf)

    var modal = Container.new()
    modal.bounds = Rect.new(40, 10, 20, 10)
    var modalLeaf = Probe.new()
    modalLeaf.bounds = Rect.new(45, 12, 10, 3)
    modalLeaf.consume = true
    modal.add(modalLeaf)
    app.pushModal(modal)

    // Click at root-leaf coords: modal hit-tests (point outside its
    // bounds) → returns null.  rootLeaf must NOT receive the event.
    var consumed = app.dispatchMouse_(mouse_(15, 6))
    check_(!consumed && rootLeaf.seen.count == 0 && modalLeaf.seen.count == 0,
           "App.dispatchMouse_: modal blocks root, click outside modal drops")
  }

  static testAppDispatchMouseOutsideDrops_() {
    var app  = App.new()
    app.root.bounds = Rect.new(1, 1, 80, 25)
    var leaf = Probe.new()
    leaf.bounds = Rect.new(10, 5, 20, 3)
    leaf.consume = true
    app.root.add(leaf)
    var consumed = app.dispatchMouse_(mouse_(70, 20))
    check_(!consumed && leaf.seen.count == 0,
           "App.dispatchMouse_: outside any widget drops")
  }
}
