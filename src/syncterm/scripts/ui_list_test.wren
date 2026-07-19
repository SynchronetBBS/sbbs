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
    testOnChangeFiresForMovement_()
    testOnChangeFiresForReplacement_()

    // Navigation
    testUpDown_()
    testUpAtTopWraps_()
    testDownAtBottomWraps_()
    testNoWrapUpAtTopClamps_()
    testNoWrapDownAtBottomClamps_()
    testHomeEnd_()
    testPageUpDown_()
    testPageUpClampsAtTop_()
    testPageDownClampsAtBottom_()

    // Scroll bookkeeping
    testEnsureVisibleScrollsDown_()
    testEnsureVisibleScrollsUp_()
    testEnsureVisibleNoOpInsideViewport_()
    testInitialOffscreenSelectionCenters_()
    testInitialVisibleSelectionStaysAtTop_()
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
    testMousePressDoesNotSelectRow_()
    testMouseHoverIgnored_()
    testMouseClickScrollbarTrackBottom_()
    testMouseClickScrollbarSeparatorIgnored_()
    testMouseClickScrollbarDownArrow_()
    testMouseClickScrollbarUpArrow_()
    testMouseDragScrollbar_()
    testMouseWheelMovesSelection_()
    testMouseWheelWrapsSelection_()
    testMouseWheelScrollbarMovesPage_()
    testMouseClickOutsideDrops_()

    // Drawing — paint + readback
    testDrawRowsSelected_()
    testDrawSelectionOnlyWhenFocused_()
    testDrawScrollbarThumb_()
    testDrawLeftScrollbarOverride_()

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
           l.showScroll == true && l.wrap == true &&
           l.highlightWhenUnfocused == true &&
           l.scrollbarSide == "right" && l.scrollbarSeparator == true,
           "List defaults: empty, selected=null, scrollTop=0, " +
           "scrolling and wrapping enabled, scrollbar on right")
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

  static testOnChangeFiresForMovement_() {
    var l = makeList_(3)
    var seen = null
    l.onChange = Fn.new {|i, item| seen = [i, item] }
    l.selected = 2
    check_(seen[0] == 2 && seen[1] == "item-2",
           "ListView onChange: selection movement reports index + item")
  }

  static testOnChangeFiresForReplacement_() {
    var l = makeList_(2)
    var seen = null
    l.onChange = Fn.new {|i, item| seen = [i, item] }
    l.items = ["replacement"]
    var replaced = seen[0] == 0 && seen[1] == "replacement"
    l.items = []
    check_(replaced && seen[0] == null && seen[1] == null,
           "ListView onChange: item replacement and empty list report")
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

  static testUpAtTopWraps_() {
    var l = makeList_(3)
    l.bounds = Rect.new(1, 1, 10, 5)
    l.up()
    check_(l.selected == 2, "ListView.up at top wraps to last")
  }

  static testDownAtBottomWraps_() {
    var l = makeList_(3)
    l.bounds = Rect.new(1, 1, 10, 5)
    l.selected = 2
    l.down()
    check_(l.selected == 0, "ListView.down at bottom wraps to first")
  }

  static testNoWrapUpAtTopClamps_() {
    var l = makeList_(3)
    l.bounds = Rect.new(1, 1, 10, 5)
    l.wrap = false
    l.up()
    check_(l.selected == 0, "ListView wrap=false: up at top clamps")
  }

  static testNoWrapDownAtBottomClamps_() {
    var l = makeList_(3)
    l.bounds = Rect.new(1, 1, 10, 5)
    l.wrap = false
    l.selected = 2
    l.down()
    check_(l.selected == 2, "ListView wrap=false: down at bottom clamps")
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

  static testInitialOffscreenSelectionCenters_() {
    var l = makeList_(20)
    l.selected = 12
    l.bounds = Rect.new(1, 1, 10, 5)
    check_(l.scrollTop == 10,
           "ListView layout centers an initially offscreen selection")
  }

  static testInitialVisibleSelectionStaysAtTop_() {
    var l = makeList_(20)
    l.selected = 3
    l.bounds = Rect.new(1, 1, 10, 5)
    check_(l.scrollTop == 0,
           "ListView layout leaves an initially visible selection in place")
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
    check_(l.innerWidth == 18,
           "ListView.innerWidth: no overflow → bounds.w - 2 (1-cell pad each side)")
  }

  static testInnerWidthWithOverflow_() {
    var l = makeList_(20)
    l.bounds = Rect.new(1, 1, 20, 5)
    check_(l.innerWidth == 17,
           "ListView.innerWidth: overflow → bounds.w - 3 (left pad + right sep/scrollbar)")
  }

  static testInnerWidthScrollDisabled_() {
    var l = makeList_(20)
    l.bounds = Rect.new(1, 1, 20, 5)
    l.showScroll = false
    check_(l.innerWidth == 18,
           "ListView.innerWidth: showScroll=false → bounds.w - 2 (1-cell pad each side)")
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
    var firedIndex = null
    var firedItem  = null
    l.onSelect = Fn.new {|i, item|
      firedIndex = i
      firedItem  = item
    }
    l.selected = 2
    var consumed = l.handle(KeyEvent.new(Key.enter))
    check_(consumed && firedIndex == 2 && firedItem == "item-2",
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

  static testMousePressDoesNotSelectRow_() {
    var l = makeList_(3)
    l.bounds = Rect.new(1, 1, 10, 5)
    var ev = MouseEvent.new(Mouse.button1Press, 5, 3, 5, 3)
    var consumed = l.handle(ev)
    check_(!consumed && l.selected == 0,
           "ListView.handle Mouse: row press waits for click")
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
    // py=3 is the final track row, which maps to maxScroll=15.
    var ev = MouseEvent.new(Mouse.button1Click, 10, 4, 10, 4)
    var consumed = l.handle(ev)
    check_(consumed && l.scrollTop == 15 && l.selected == 15,
           "ListView.handle Mouse: scrollbar track click jumps viewport")
  }

  static testMouseClickScrollbarSeparatorIgnored_() {
    var l = makeList_(20)
    l.bounds = Rect.new(1, 1, 10, 5)
    // Right-side scrollbar is x=10; its separator is x=9.
    var ev = MouseEvent.new(Mouse.button1Click, 9, 3, 9, 3)
    var consumed = l.handle(ev)
    check_(consumed && l.scrollTop == 0 && l.selected == 0,
           "ListView.handle Mouse: scrollbar separator is inert")
  }

  static testMouseClickScrollbarDownArrow_() {
    var l = makeList_(20)
    l.bounds = Rect.new(1, 1, 10, 5)
    // py=4 is the down-arrow row; UIFC maps it to PageDown.
    var ev = MouseEvent.new(Mouse.button1Click, 10, 5, 10, 5)
    var consumed = l.handle(ev)
    check_(consumed && l.selected == 4 && l.scrollTop == 0,
           "ListView.handle Mouse: down arrow pages the selection")
  }

  static testMouseClickScrollbarUpArrow_() {
    var l = makeList_(20)
    l.bounds = Rect.new(1, 1, 10, 5)
    l.selected = 9
    // py=0 is the up-arrow row; UIFC maps it to PageUp.
    var ev = MouseEvent.new(Mouse.button1Click, 10, 1, 10, 1)
    var consumed = l.handle(ev)
    check_(consumed && l.selected == 5 && l.scrollTop == 5,
           "ListView.handle Mouse: up arrow pages the selection")
  }

  static testMouseDragScrollbar_() {
    var l = makeList_(20)
    l.bounds = Rect.new(1, 1, 10, 5)
    var prev = l.selected
    // UIFC hands list drags to screen selection, including drags that
    // start on the scrollbar.
    var ev = MouseEvent.new(Mouse.button1DragMove, 10, 1, 10, 4)
    var consumed = l.handle(ev)
    check_(!consumed && l.scrollTop == 0 && l.selected == prev,
           "ListView.handle Mouse: scrollbar drag falls through")
  }

  static testMouseWheelMovesSelection_() {
    var l = makeList_(20)
    l.bounds = Rect.new(1, 1, 10, 5)
    // UIFC maps wheel-down to one Down navigation event.
    var ev = MouseEvent.new(Mouse.wheelDownClick, 5, 3, 5, 3)
    var consumed = l.handle(ev)
    check_(consumed && l.selected == 1 && l.scrollTop == 0,
           "ListView.handle Mouse: wheel-down moves selection one row")
  }

  static testMouseWheelWrapsSelection_() {
    var l = makeList_(20)
    l.bounds = Rect.new(1, 1, 10, 5)
    // UIFC maps wheel-up to Up, including the normal list wrapping.
    var ev = MouseEvent.new(Mouse.wheelUpClick, 5, 3, 5, 3)
    var consumed = l.handle(ev)
    check_(consumed && l.selected == 19 && l.scrollTop == 15,
           "ListView.handle Mouse: wheel-up wraps selection")
  }

  static testMouseWheelScrollbarMovesPage_() {
    var l = makeList_(20)
    l.bounds = Rect.new(1, 1, 10, 5)
    var ev = MouseEvent.new(Mouse.wheelDownClick, 10, 3, 10, 3)
    var consumed = l.handle(ev)
    check_(consumed && l.scrollTop == 4 && l.selected == 4,
           "ListView.handle Mouse: wheel over scrollbar moves one page")
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
    // No overflow → no scrollbar.  leftInset_ = 1 always, so content
    // starts at col 1 even without a scrollbar (1-cell padding cell).
    // Row 0 has "aaa" (list.item — legacy 0x1F).
    // Row 1 has "bbb" (list.item.focused — legacy 0x70 lightbar).
    // The focused fill spans the full width, so col 0 of row 1 is a
    // space cell painted with the focused legacyAttr.
    check_(s.cellAt(1, 0).ch == "a" && s.cellAt(1, 1).ch == "b" &&
           s.cellAt(1, 1).legacyAttr == 0x70 &&
           s.cellAt(0, 1).legacyAttr == 0x70,
           "ListView.draw: rows rendered, selected gets focused style")
  }

  static testDrawSelectionOnlyWhenFocused_() {
    var l = ListView.new()
    l.items = ["aaa", "bbb"]
    l.bounds = Rect.new(1, 1, 10, 2)
    l.selected = 1
    l.highlightWhenUnfocused = false
    var s = l.draw()
    check_(s.cellAt(1, 1).legacyAttr == 0x1F,
           "ListView.draw: optional unfocused selection is not highlighted")

    l.focused = true
    s = l.draw()
    check_(s.cellAt(1, 1).legacyAttr == 0x70,
           "ListView.draw: focused selection remains highlighted")
  }

  static testDrawScrollbarThumb_() {
    var l = ListView.new()
    var items = []
    for (i in 0...20) items.add("x")
    l.items = items
    l.bounds = Rect.new(1, 1, 10, 5)            // overflows
    var s = l.draw()
    // Content starts after the left padding at column 1.  Column 8 is
    // the separator and column 9 is the right-side scrollbar.
    var top = s.cellAt(9, 0)
    var bot = s.cellAt(9, 4)
    check_(top.chByte != bot.chByte,
           "ListView.draw: scrollbar drawn with distinct thumb / track glyphs")
    check_(s.cellAt(8, 0).ch == "│" && s.cellAt(1, 0).ch == "x",
           "ListView.draw: content starts at col 1, separator at col 8")
  }

  static testDrawLeftScrollbarOverride_() {
    var l = ListView.new()
    var items = []
    for (i in 0...20) items.add("x")
    l.items = items
    l.bounds = Rect.new(1, 1, 10, 5)
    l.scrollbarSide = "left"
    var s = l.draw()
    check_(s.cellAt(0, 0).chByte == 0x1E &&
           s.cellAt(1, 0).ch == "│" && s.cellAt(2, 0).ch == "x",
           "ListView.draw: explicit left scrollbar override")
  }
}
