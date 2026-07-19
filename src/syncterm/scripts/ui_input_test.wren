// Self-tests for ui_input.  Pure logic plus a paint test that reads
// cells out of the TextInput's own Surface.

import "ui_widget" for Rect
import "ui_input"  for TextInput
import "syncterm"  for KeyEvent, MouseEvent, Key, Mouse

class PasteProbe is TextInput {
  construct new() {
    super()
    _pasted = false
  }
  pasted { _pasted }
  paste_() { _pasted = true }
}

class UiInputTest {
  static run() {
    __pass = 0
    __fail = 0
    System.print("=== ui_input self-test starting ===")

    var reset = TextInput.new()
    reset.insertMode = true

    testDefaults_()
    testValueRoundtrip_()
    testValueResetsCursor_()
    testCursorClamps_()
    testInsertCodepoint_()
    testInsertAtCursor_()
    testInsertTogglesOverwrite_()
    testInsertModeShared_()
    testInsertTextFiltersControls_()
    testSelectAllTypingReplaces_()
    testSelectAllMovementPreserves_()
    testSelectAllDeleteClears_()
    testSelectAllMousePreserves_()
    testValueClearsSelection_()
    testBackspace_()
    testBackspaceAtStart_()
    testDelete_()
    testDeleteAtEnd_()
    testHomeEnd_()
    testLeftRight_()
    testCtrlMovementAndTruncate_()
    testMaxLenCaps_()
    testEnterFiresOnSubmit_()
    testCtrlZConsumed_()
    testOnChangeFires_()
    testTabNotConsumed_()
    testScrollOffTracksCursor_()
    testCursorPosVisible_()
    testCursorVisibleTrue_()
    testCursorAttr_()
    testMouseClickPositionsCursor_()
    testMousePressDoesNotPositionCursor_()
    testRightClickPastes_()
    testMouseHoverIgnored_()
    testMouseDragFallsThrough_()
    testDrawShowsValue_()
    testDrawMasksValue_()
    testDrawFocusedStyle_()

    var total = __pass + __fail
    System.print("=== ui_input: %(total) tests, %(__pass) pass, %(__fail) fail ===")
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

  // ----- Construction --------------------------------------------

  static testDefaults_() {
    var t = TextInput.new()
    check_(t.value == "" && t.cursor == 0 && t.scrollOff == 0 &&
           t.count == 0,
           "TextInput defaults: empty, cursor=0, scrollOff=0")
  }

  static testValueRoundtrip_() {
    var t = TextInput.new()
    t.value = "hello"
    check_(t.value == "hello" && t.count == 5,
           "TextInput value=/value: round-trip + count")
  }

  static testValueResetsCursor_() {
    var t = TextInput.new()
    t.bounds = Rect.new(1, 1, 10, 1)
    t.value = "abc"
    check_(t.cursor == 3,
           "TextInput value=: cursor lands at end")
  }

  static testCursorClamps_() {
    var t = TextInput.new()
    t.value  = "abc"
    t.cursor = 99
    check_(t.cursor == 3, "TextInput cursor=: clamps above count")
    t.cursor = -10
    check_(t.cursor == 0, "TextInput cursor=: clamps below 0")
  }

  // ----- Insert / delete -----------------------------------------

  static testInsertCodepoint_() {
    var t = TextInput.new()
    t.bounds = Rect.new(1, 1, 10, 1)
    t.handle(KeyEvent.new(0x41))   // 'A'
    check_(t.value == "A" && t.cursor == 1,
           "TextInput type 'A': appended, cursor advanced")
  }

  static testInsertAtCursor_() {
    var t = TextInput.new()
    t.bounds = Rect.new(1, 1, 10, 1)
    t.value  = "ac"
    t.cursor = 1
    t.handle(KeyEvent.new(0x42))   // 'B'
    check_(t.value == "aBc" && t.cursor == 2,
           "TextInput type at mid: inserts and advances")
  }

  static testInsertTogglesOverwrite_() {
    var t = TextInput.new()
    t.bounds = Rect.new(1, 1, 10, 1)
    t.value = "abc"
    t.cursor = 1
    t.handle(KeyEvent.new(Key.insert))
    t.handle(KeyEvent.new(0x58))
    var ok = t.value == "aXc" && t.cursor == 2 && !t.insertMode &&
        t.cursorShape == "solid"
    t.insertMode = true
    check_(ok, "TextInput Insert: toggles overwrite mode")
  }

  static testInsertModeShared_() {
    var first = TextInput.new()
    first.insertMode = false
    var second = TextInput.new()
    var ok = !second.insertMode && second.cursorShape == "solid"
    second.insertMode = true
    check_(ok, "TextInput insert mode is shared between fields")
  }

  static testInsertTextFiltersControls_() {
    var t = TextInput.new()
    t.bounds = Rect.new(1, 1, 10, 1)
    t.insertText_("a\nb\t\u007fc")
    check_(t.value == "abc",
           "TextInput paste path: ignores control characters")
  }

  static testSelectAllTypingReplaces_() {
    var t = TextInput.new()
    t.bounds = Rect.new(1, 1, 10, 1)
    t.value = "hello"
    var changes = 0
    var seen = null
    t.onChange = Fn.new {|s|
      changes = changes + 1
      seen = s
    }
    t.selectAll()
    t.handle(KeyEvent.new(0x58))
    check_(t.value == "X" && t.cursor == 1 && !t.allSelected &&
           changes == 1 && seen == "X",
           "TextInput selectAll: typing replaces once")
  }

  static testSelectAllMovementPreserves_() {
    var t = TextInput.new()
    t.bounds = Rect.new(1, 1, 10, 1)
    t.value = "hello"
    t.selectAll()
    t.handle(KeyEvent.new(Key.left))
    var moved = t.value == "hello" && t.cursor == 4 && !t.allSelected
    t.handle(KeyEvent.new(0x58))
    check_(moved && t.value == "hellXo",
           "TextInput selectAll: movement preserves before editing")
  }

  static testSelectAllDeleteClears_() {
    var backspace = TextInput.new()
    backspace.value = "hello"
    backspace.selectAll()
    backspace.handle(KeyEvent.new(Key.backspace))
    var deleted = TextInput.new()
    deleted.value = "hello"
    deleted.selectAll()
    deleted.handle(KeyEvent.new(Key.delete))
    check_(backspace.value == "" && deleted.value == "" &&
           !backspace.allSelected && !deleted.allSelected,
           "TextInput selectAll: Backspace and Delete clear value")
  }

  static testSelectAllMousePreserves_() {
    var t = TextInput.new()
    t.bounds = Rect.new(1, 1, 10, 1)
    t.value = "hello"
    t.selectAll()
    var ev = MouseEvent.new(Mouse.button1Click, 3, 1, 3, 1)
    t.handle(ev)
    check_(t.value == "hello" && t.cursor == 2 && !t.allSelected,
           "TextInput selectAll: mouse positioning preserves value")
  }

  static testValueClearsSelection_() {
    var t = TextInput.new()
    t.value = "old"
    t.selectAll()
    t.value = "new"
    check_(t.value == "new" && t.cursor == 3 && !t.allSelected,
           "TextInput value=: clears select-all state")
  }

  static testCtrlZConsumed_() {
    var t = TextInput.new()
    check_(t.handle(KeyEvent.new(Key.ctrlZ)),
           "TextInput Ctrl-Z: opens contextual help when available")
  }

  static testBackspace_() {
    var t = TextInput.new()
    t.bounds = Rect.new(1, 1, 10, 1)
    t.value = "abc"
    t.handle(KeyEvent.new(Key.backspace))
    check_(t.value == "ab" && t.cursor == 2,
           "TextInput backspace: removes char before cursor")
  }

  static testBackspaceAtStart_() {
    var t = TextInput.new()
    t.bounds = Rect.new(1, 1, 10, 1)
    t.value  = "abc"
    t.cursor = 0
    t.handle(KeyEvent.new(Key.backspace))
    check_(t.value == "abc" && t.cursor == 0,
           "TextInput backspace at start: no-op")
  }

  static testDelete_() {
    var t = TextInput.new()
    t.bounds = Rect.new(1, 1, 10, 1)
    t.value  = "abc"
    t.cursor = 1
    t.handle(KeyEvent.new(Key.delete))
    check_(t.value == "ac" && t.cursor == 1,
           "TextInput delete: removes char at cursor")
  }

  static testDeleteAtEnd_() {
    var t = TextInput.new()
    t.bounds = Rect.new(1, 1, 10, 1)
    t.value  = "abc"
    t.handle(KeyEvent.new(Key.delete))
    check_(t.value == "abc",
           "TextInput delete at end: no-op")
  }

  // ----- Cursor movement -----------------------------------------

  static testHomeEnd_() {
    var t = TextInput.new()
    t.bounds = Rect.new(1, 1, 10, 1)
    t.value = "hello"
    t.handle(KeyEvent.new(Key.home))
    check_(t.cursor == 0, "TextInput Home: cursor to 0")
    t.handle(KeyEvent.new(Key.end))
    check_(t.cursor == 5, "TextInput End: cursor to count")
  }

  static testLeftRight_() {
    var t = TextInput.new()
    t.bounds = Rect.new(1, 1, 10, 1)
    t.value = "ab"
    t.handle(KeyEvent.new(Key.left))
    check_(t.cursor == 1, "TextInput Left: cursor -1")
    t.handle(KeyEvent.new(Key.right))
    check_(t.cursor == 2, "TextInput Right: cursor +1")
  }

  static testCtrlMovementAndTruncate_() {
    var t = TextInput.new()
    t.bounds = Rect.new(1, 1, 10, 1)
    t.value = "abcd"
    t.handle(KeyEvent.new(Key.ctrlB))
    var atStart = t.cursor == 0
    t.cursor = 2
    t.handle(KeyEvent.new(Key.ctrlY))
    var truncated = t.value == "ab"
    t.handle(KeyEvent.new(Key.ctrlE))
    check_(atStart && truncated && t.cursor == 2,
           "TextInput Ctrl-B/Ctrl-E/Ctrl-Y: move and truncate")
  }

  // ----- maxLen + callbacks --------------------------------------

  static testMaxLenCaps_() {
    var t = TextInput.new()
    t.bounds = Rect.new(1, 1, 10, 1)
    t.maxLen = 3
    t.handle(KeyEvent.new(0x41))
    t.handle(KeyEvent.new(0x42))
    t.handle(KeyEvent.new(0x43))
    t.handle(KeyEvent.new(0x44))   // dropped
    check_(t.value == "ABC",
           "TextInput maxLen: caps insertion")
  }

  static testEnterFiresOnSubmit_() {
    var t = TextInput.new()
    t.bounds = Rect.new(1, 1, 10, 1)
    t.value  = "ok"
    var fired = null
    t.onSubmit = Fn.new {|s| fired = s }
    var consumed = t.handle(KeyEvent.new(Key.enter))
    check_(consumed && fired == "ok",
           "TextInput Enter: fires onSubmit with value")
  }

  static testOnChangeFires_() {
    var t = TextInput.new()
    t.bounds = Rect.new(1, 1, 10, 1)
    var seen = null
    t.onChange = Fn.new {|s| seen = s }
    t.handle(KeyEvent.new(0x58))   // 'X'
    check_(seen == "X",
           "TextInput onChange: fires after edit with new value")
  }

  static testTabNotConsumed_() {
    var t = TextInput.new()
    t.bounds = Rect.new(1, 1, 10, 1)
    var consumed = t.handle(KeyEvent.new(Key.tab))
    check_(!consumed,
           "TextInput Tab: not consumed (left for focus traversal)")
  }

  // ----- Scrolling + cursor exposure ------------------------------

  static testScrollOffTracksCursor_() {
    var t = TextInput.new()
    t.bounds = Rect.new(1, 1, 4, 1)         // 4-cell viewport
    // Type 6 chars; cursor lands at 6, scrollOff should be 3 so
    // cursor sits on column bounds.w-1 = 3.
    var s = "abcdef"
    for (c in s) t.handle(KeyEvent.new(c.bytes[0]))
    check_(t.cursor == 6 && t.scrollOff == 3,
           "TextInput overflow: scrollOff tracks cursor at right edge")
  }

  static testCursorPosVisible_() {
    var t = TextInput.new()
    t.bounds = Rect.new(5, 7, 10, 1)
    t.value  = "hi"
    var p = t.cursorPos
    check_(p[0] == 7 && p[1] == 7,
           "TextInput cursorPos: bounds.x + (cursor - scrollOff)")
  }

  static testCursorVisibleTrue_() {
    var t = TextInput.new()
    check_(t.cursorVisible == true && t.cursorShape == "normal",
           "TextInput cursor defaults to visible normal insert shape")
  }

  static testCursorAttr_() {
    var t = TextInput.new()
    check_(t.cursorAttr == t.style("input.focused").legacyAttr,
           "TextInput cursor uses the focused input foreground")
  }

  // ----- Mouse ----------------------------------------------------

  static testMouseClickPositionsCursor_() {
    var t = TextInput.new()
    t.bounds = Rect.new(1, 1, 10, 1)
    t.value  = "hello"
    // Click at column 3 (bounds.x=1, so endX=4).
    var ev = MouseEvent.new(Mouse.button1Click, 4, 1, 4, 1)
    var consumed = t.handle(ev)
    check_(consumed && t.cursor == 3,
           "TextInput mouse click: positions cursor at column")
  }

  static testMousePressDoesNotPositionCursor_() {
    var t = TextInput.new()
    t.bounds = Rect.new(1, 1, 10, 1)
    t.value = "hello"
    var ev = MouseEvent.new(Mouse.button1Press, 2, 1, 2, 1)
    var consumed = t.handle(ev)
    check_(!consumed && t.cursor == 5,
           "TextInput mouse press: ignored until click")
  }

  static testRightClickPastes_() {
    var t = PasteProbe.new()
    t.bounds = Rect.new(1, 1, 10, 1)
    var ev = MouseEvent.new(Mouse.button3Click, 2, 1, 2, 1)
    var consumed = t.handle(ev)
    check_(consumed && t.pasted,
           "TextInput right click: uses the paste path")
  }

  static testMouseHoverIgnored_() {
    var t = TextInput.new()
    t.bounds = Rect.new(1, 1, 10, 1)
    t.value  = "hello"
    var ev = MouseEvent.new(Mouse.move, 4, 1, 4, 1)
    var consumed = t.handle(ev)
    check_(!consumed && t.cursor == 5,
           "TextInput mouse hover: ignored, cursor unchanged")
  }

  static testMouseDragFallsThrough_() {
    var t = TextInput.new()
    t.bounds = Rect.new(1, 1, 10, 1)
    t.value = "hello"
    var ev = MouseEvent.new(Mouse.button1DragStart, 4, 1, 8, 1)
    var consumed = t.handle(ev)
    check_(!consumed && t.cursor == 5,
           "TextInput mouse drag: falls through to screen selection")
  }

  // ----- Drawing --------------------------------------------------

  static testDrawShowsValue_() {
    var t = TextInput.new()
    t.bounds = Rect.new(1, 1, 10, 1)
    t.value  = "abc"
    var s = t.draw()
    check_(s.cellAt(0, 0).ch == "a" && s.cellAt(1, 0).ch == "b" &&
           s.cellAt(2, 0).ch == "c" && s.cellAt(3, 0).ch == " ",
           "TextInput draw: characters appear, padding is space")
  }

  static testDrawMasksValue_() {
    var t = TextInput.new()
    t.bounds = Rect.new(1, 1, 10, 1)
    t.value = "secret"
    t.mask = "*"
    var s = t.draw()
    check_(t.value == "secret" && s.cellAt(0, 0).ch == "*" &&
           s.cellAt(5, 0).ch == "*" && s.cellAt(6, 0).ch == " ",
           "TextInput mask: hides paint without changing value")
  }

  static testDrawFocusedStyle_() {
    var t = TextInput.new()
    t.bounds  = Rect.new(1, 1, 5, 1)
    t.value   = "x"
    t.focused = true
    var s = t.draw()
    // input.focused legacy attr is 0x70 (gray bg lightbar) per ui_style;
    // the focus differentiation happens via bgRgb (white vs gray).
    check_(s.cellAt(0, 0).legacyAttr == 0x70 &&
           s.cellAt(0, 0).bgRgb == 0xFFFFFF,
           "TextInput draw: focused uses input.focused style")
  }
}
