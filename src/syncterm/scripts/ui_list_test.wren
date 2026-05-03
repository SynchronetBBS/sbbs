// Self-tests for ui_list.  Logic-heavy (selection, navigation,
// scroll bookkeeping, event handling) plus a couple of paint tests
// that read cells out of the ListView's own Surface — no
// Screen.save/restore needed.

import "ui_widget" for Rect
import "ui_list"   for ListView
import "syncterm"  for KeyEvent, MouseEvent, Key, Mouse

class UiListTest {
  static run() {
    __pass = 0
    __fail = 0
    System.print("=== ui_list self-test starting ===")

    // Construction + items
    testListDefaults_()
    testListItemsSetterResets_()
    testListItemsSetterEmpty_()

    // Selection clamping
    testSelectedClampsHigh_()
    testSelectedClampsLow_()
    testSelectedItemAccessor_()
    testSelectedItemNullWhenEmpty_()

    // Navigation
    testUpDown_()
    testUpAtTop_()
    testDownAtBottom_()
    testHomeEnd_()
    testPageUpDown_()
    testPageUpClampsAtTop_()
    testPageDownClampsAtBottom_()

    // Scroll bookkeeping
    testEnsureVisibleScrollsDown_()
    testEnsureVisibleScrollsUp_()
    testEnsureVisibleNoOpInsideViewport_()
    testHomeResetsScroll_()

    // formatItem hook
    testFormatItemDefault_()

    // innerWidth (with / without scrollbar)
    testInnerWidthNoOverflow_()
    testInnerWidthWithOverflow_()
    testInnerWidthScrollDisabled_()

    // Event handling
    testKeyDownConsumed_()
    testKeyEnterFiresOnSelect_()
    testKeyEnterNoCallback_()
    testMouseClickSelectsRow_()
    testMouseHoverIgnored_()
    testMouseClickScrollbarTrackBottom_()
    testMouseClickScrollbarDownArrow_()
    testMouseClickScrollbarUpArrow_()
    testMouseDragScrollbar_()
    testMouseWheelScrolls_()
    testMouseWheelClampsAtTop_()
    testMouseClickOutsideDrops_()

    // Drawing — paint + readback
    testDrawRowsSelected_()
    testDrawScrollbarThumb_()

    var total = __pass + __fail
    System.print("=== ui_list: %(total) tests, %(__pass) pass, %(__fail) fail ===")
    return [__pass, __fail]
  }

  static check_(ok, label) {
    if (ok) {
      __pass = __pass + 1
    } else {
      __fail = __fail + 1
      System.print("  FAIL %(label)")
    }
  }

  static makeList_(n) {
    var l = ListView.new()
    var items = []
    for (i in 0...n) items.add("item-%(i)")
    l.items = items
    return l
  }

  // ----- Construction + items setter ------------------------------

  static testListDefaults_() {
    var l = ListView.new()
    check_(l.count == 0 && l.selected == null && l.scrollTop == 0 &&
           l.showScroll == true,
           "List defaults: empty, selected=null, scrollTop=0, showScroll=true")
  }

  static testListItemsSetterResets_() {
    var l = ListView.new()
    l.items = ["a", "b", "c"]
    check_(l.count == 3 && l.selected == 0 && l.scrollTop == 0,
           "List items=: sets count, selects 0, resets scrollTop")
  }

  static testListItemsSetterEmpty_() {
    var l = ListView.new()
    l.items = ["x"]
    l.items = []
    check_(l.count == 0 && l.selected == null,
           "List items= []: selected becomes null")
  }

  // ----- Selection clamping ---------------------------------------

  static testSelectedClampsHigh_() {
    var l = makeList_(3)
    l.selected = 99
    check_(l.selected == 2, "List selected=: clamps above count-1")
  }

  static testSelectedClampsLow_() {
    var l = makeList_(3)
    l.selected = -5
    check_(l.selected == 0, "List selected=: clamps to 0")
  }

  static testSelectedItemAccessor_() {
    var l = makeList_(3)
    l.selected = 1
    check_(l.selectedItem == "item-1", "ListView.selectedItem returns the right one")
  }

  static testSelectedItemNullWhenEmpty_() {
    var l = ListView.new()
    check_(l.selectedItem == null,
           "ListView.selectedItem is null on empty list")
  }

  // ----- Navigation -----------------------------------------------

  static testUpDown_() {
    var l = makeList_(3)
    l.bounds = Rect.new(1, 1, 10, 5)
    l.down()
    check_(l.selected == 1, "ListView.down moves selection +1")
    l.up()
    check_(l.selected == 0, "ListView.up moves selection -1")
  }

  static testUpAtTop_() {
    var l = makeList_(3)
    l.bounds = Rect.new(1, 1, 10, 5)
    l.up()
    check_(l.selected == 0, "ListView.up at top is no-op")
  }

  static testDownAtBottom_() {
    var l = makeList_(3)
    l.bounds = Rect.new(1, 1, 10, 5)
    l.selected = 2
    l.down()
    check_(l.selected == 2, "ListView.down at bottom is no-op")
  }

  static testHomeEnd_() {
    var l = makeList_(5)
    l.bounds = Rect.new(1, 1, 10, 3)
    l.end()
    check_(l.selected == 4, "ListView.end selects last")
    l.home()
    check_(l.selected == 0, "ListView.home selects first")
  }

  static testPageUpDown_() {
    var l = makeList_(20)
    l.bounds = Rect.new(1, 1, 10, 5)            // 5-row viewport
    l.pageDown()
    check_(l.selected == 4, "ListView.pageDown advances by viewport - 1")
    l.pageDown()
    check_(l.selected == 8, "ListView.pageDown advances again")
    l.pageUp()
    check_(l.selected == 4, "ListView.pageUp retreats by viewport - 1")
  }

  static testPageUpClampsAtTop_() {
    var l = makeList_(20)
    l.bounds = Rect.new(1, 1, 10, 5)
    l.pageUp()
    check_(l.selected == 0, "ListView.pageUp at top clamps to 0")
  }

  static testPageDownClampsAtBottom_() {
    var l = makeList_(7)
    l.bounds = Rect.new(1, 1, 10, 5)
    l.end()
    l.pageDown()
    check_(l.selected == 6, "ListView.pageDown at bottom clamps to count-1")
  }

  // ----- Scroll bookkeeping ---------------------------------------

  static testEnsureVisibleScrollsDown_() {
    var l = makeList_(20)
    l.bounds = Rect.new(1, 1, 10, 5)
    l.selected = 10
    check_(l.scrollTop == 6,
           "ListView.scrollTop tracks selection past viewport bottom")
  }

  static testEnsureVisibleScrollsUp_() {
    var l = makeList_(20)
    l.bounds = Rect.new(1, 1, 10, 5)
    l.selected = 15
    l.selected = 3
    check_(l.scrollTop == 3,
           "ListView.scrollTop tracks selection above viewport top")
  }

  static testEnsureVisibleNoOpInsideViewport_() {
    var l = makeList_(20)
    l.bounds = Rect.new(1, 1, 10, 5)
    // Selecting 0..4 should keep scrollTop at 0.
    l.selected = 3
    check_(l.scrollTop == 0,
           "ListView.scrollTop stays put when selection is in viewport")
  }

  static testHomeResetsScroll_() {
    var l = makeList_(20)
    l.bounds = Rect.new(1, 1, 10, 5)
    l.end()
    l.home()
    check_(l.scrollTop == 0, "ListView.home resets scrollTop to 0")
  }

  // ----- formatItem hook ------------------------------------------

  static testFormatItemDefault_() {
    var l = ListView.new()
    check_(l.formatItem(42, 10) == "42",
           "ListView.formatItem default: item.toString")
  }

  // ----- innerWidth -----------------------------------------------

  static testInnerWidthNoOverflow_() {
    var l = makeList_(3)
    l.bounds = Rect.new(1, 1, 20, 5)
    check_(l.innerWidth == 20,
           "ListView.innerWidth: no overflow → full bounds.w")
  }

  static testInnerWidthWithOverflow_() {
    var l = makeList_(20)
    l.bounds = Rect.new(1, 1, 20, 5)
    check_(l.innerWidth == 18,
           "ListView.innerWidth: overflow → bounds.w - 2 (scrollbar + separator)")
  }

  static testInnerWidthScrollDisabled_() {
    var l = makeList_(20)
    l.bounds = Rect.new(1, 1, 20, 5)
    l.showScroll = false
    check_(l.innerWidth == 20,
           "ListView.innerWidth: showScroll=false → full bounds.w")
  }

  // ----- Event handling -------------------------------------------

  static testKeyDownConsumed_() {
    var l = makeList_(3)
    l.bounds = Rect.new(1, 1, 10, 5)
    var consumed = l.handle(KeyEvent.new(Key.down))
    check_(consumed && l.selected == 1,
           "ListView.handle KeyEvent down: consumed + selection moved")
  }

  static testKeyEnterFiresOnSelect_() {
    var l = makeList_(3)
    l.bounds = Rect.new(1, 1, 10, 5)
    var fired = [null, null]
    l.onSelect = Fn.new {|i, item|
      fired[0] = i
      fired[1] = item
    }
    l.selected = 2
    var consumed = l.handle(KeyEvent.new(Key.enter))
    check_(consumed && fired[0] == 2 && fired[1] == "item-2",
           "ListView.handle Enter: fires onSelect with index + item")
  }

  static testKeyEnterNoCallback_() {
    var l = makeList_(3)
    l.bounds = Rect.new(1, 1, 10, 5)
    var consumed = l.handle(KeyEvent.new(Key.enter))
    check_(consumed,
           "ListView.handle Enter: consumed even with no onSelect set")
  }

  static testMouseClickSelectsRow_() {
    var l = makeList_(3)
    l.bounds = Rect.new(1, 1, 10, 5)
    // bounds y=1; row index 2 = y=3.  scrollTop=0 so idx=2.
    var ev = MouseEvent.new(Mouse.button1Click, 5, 3, 5, 3)
    var consumed = l.handle(ev)
    check_(consumed && l.selected == 2,
           "ListView.handle Mouse: row click selects that row")
  }

  static testMouseHoverIgnored_() {
    var l = makeList_(3)
    l.bounds = Rect.new(1, 1, 10, 5)
    // Mouse.move should NOT change selection.
    var ev = MouseEvent.new(Mouse.move, 5, 3, 5, 3)
    var consumed = l.handle(ev)
    check_(!consumed && l.selected == 0,
           "ListView.handle Mouse: hover (Mouse.move) ignored")
  }

  static testMouseClickScrollbarTrackBottom_() {
    var l = makeList_(20)
    l.bounds = Rect.new(1, 1, 10, 5)            // h=5 → arrows on
    // Track rows are 1..3 (between the up + down arrows).  Click at
    // py=3 (screen y=4) is the last track row; maps to maxScroll=15.
    var prev = l.selected
    var ev = MouseEvent.new(Mouse.button1Click, 1, 4, 1, 4)
    var consumed = l.handle(ev)
    check_(consumed && l.scrollTop == 15 && l.selected == prev,
           "ListView.handle Mouse: scrollbar track-bottom click scrolls to end, selection unchanged")
  }

  static testMouseClickScrollbarDownArrow_() {
    var l = makeList_(20)
    l.bounds = Rect.new(1, 1, 10, 5)
    // py=4 (screen y=5) is the down-arrow row; clicking it steps +1.
    var ev = MouseEvent.new(Mouse.button1Click, 1, 5, 1, 5)
    var consumed = l.handle(ev)
    check_(consumed && l.scrollTop == 1,
           "ListView.handle Mouse: scrollbar down-arrow click steps +1")
  }

  static testMouseClickScrollbarUpArrow_() {
    var l = makeList_(20)
    l.bounds = Rect.new(1, 1, 10, 5)
    l.scrollTop = 5
    // py=0 (screen y=1) is the up-arrow row; clicking it steps -1.
    var ev = MouseEvent.new(Mouse.button1Click, 1, 1, 1, 1)
    var consumed = l.handle(ev)
    check_(consumed && l.scrollTop == 4,
           "ListView.handle Mouse: scrollbar up-arrow click steps -1")
  }

  static testMouseDragScrollbar_() {
    var l = makeList_(20)
    l.bounds = Rect.new(1, 1, 10, 5)
    var prev = l.selected
    // DragMove pressed at scrollbar top (startY=1), pointer now at the
    // bottom of the *track* (endY=4 → py=3).  scrollTop tracks the end
    // coord; selection stays put.
    var ev = MouseEvent.new(Mouse.button1DragMove, 1, 1, 1, 4)
    var consumed = l.handle(ev)
    check_(consumed && l.scrollTop == 15 && l.selected == prev,
           "ListView.handle Mouse: scrollbar drag follows endY, selection unchanged")
  }

  static testMouseWheelScrolls_() {
    var l = makeList_(20)
    l.bounds = Rect.new(1, 1, 10, 5)
    var prev = l.selected
    // Wheel-down inside the widget moves scrollTop by wheelStep.
    var ev = MouseEvent.new(Mouse.wheelDownClick, 5, 3, 5, 3)
    var consumed = l.handle(ev)
    check_(consumed && l.scrollTop == ListView.wheelStep && l.selected == prev,
           "ListView.handle Mouse: wheel-down scrolls viewport, selection unchanged")
  }

  static testMouseWheelClampsAtTop_() {
    var l = makeList_(20)
    l.bounds = Rect.new(1, 1, 10, 5)
    // scrollTop is already 0; wheel-up clamps, doesn't go negative.
    var ev = MouseEvent.new(Mouse.wheelUpClick, 5, 3, 5, 3)
    var consumed = l.handle(ev)
    check_(consumed && l.scrollTop == 0,
           "ListView.handle Mouse: wheel-up at top clamps to 0")
  }

  static testMouseClickOutsideDrops_() {
    var l = makeList_(3)
    l.bounds = Rect.new(1, 1, 10, 5)
    var ev = MouseEvent.new(Mouse.button1Click, 50, 50, 50, 50)
    var consumed = l.handle(ev)
    check_(!consumed, "ListView.handle Mouse: click outside bounds drops")
  }

  // ----- Drawing --------------------------------------------------

  static testDrawRowsSelected_() {
    var l = ListView.new()
    l.items  = ["aaa", "bbb", "ccc"]
    l.bounds = Rect.new(1, 1, 10, 4)
    l.selected = 1
    var s = l.draw()
    // No overflow → no scrollbar → content starts at column 0.
    // Row 0 has "aaa" (list.item — legacy 0x1F).
    // Row 1 has "bbb" (list.item.focused — legacy 0x70 lightbar).
    check_(s.cellAt(0, 0).ch == "a" && s.cellAt(0, 1).ch == "b" &&
           s.cellAt(0, 1).legacyAttr == 0x70,
           "ListView.draw: rows rendered, selected gets focused style")
  }

  static testDrawScrollbarThumb_() {
    var l = ListView.new()
    var items = []
    for (i in 0...20) items.add("x")
    l.items = items
    l.bounds = Rect.new(1, 1, 10, 5)            // overflows
    var s = l.draw()
    // Scrollbar at column 0 (UIFC-left convention); column 1 is the
    // separator (frame.left "│"); content from column 2 onward.
    var top = s.cellAt(0, 0)
    var bot = s.cellAt(0, 4)
    check_(top.chByte != bot.chByte,
           "ListView.draw: scrollbar drawn with distinct thumb / track glyphs")
    check_(s.cellAt(1, 0).ch == "│" && s.cellAt(2, 0).ch == "x",
           "ListView.draw: separator '│' at col 1, content starts at col 2")
  }
}
