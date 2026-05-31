// Self-tests for ui_input.  Pure logic plus a paint test that reads
// cells out of the TextInput's own Surface.

import "ui_widget" for Rect
import "ui_input"  for TextInput
import "syncterm"  for KeyEvent, MouseEvent, Key, Mouse

class UiInputTest {
  static run() {
    __pass = 0
    __fail = 0
    System.print("=== ui_input self-test starting ===")

    testDefaults_()
    testValueRoundtrip_()
    testValueResetsCursor_()
    testCursorClamps_()
    testInsertCodepoint_()
    testInsertAtCursor_()
    testBackspace_()
    testBackspaceAtStart_()
    testDelete_()
    testDeleteAtEnd_()
    testHomeEnd_()
    testLeftRight_()
    testMaxLenCaps_()
    testEnterFiresOnSubmit_()
    testOnChangeFires_()
    testTabNotConsumed_()
    testScrollOffTracksCursor_()
    testCursorPosVisible_()
    testCursorVisibleTrue_()
    testMouseClickPositionsCursor_()
    testMouseHoverIgnored_()
    testDrawShowsValue_()
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
    check_(t.cursorVisible == true,
           "TextInput cursorVisible defaults to true")
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

  static testMouseHoverIgnored_() {
    var t = TextInput.new()
    t.bounds = Rect.new(1, 1, 10, 1)
    t.value  = "hello"
    var ev = MouseEvent.new(Mouse.move, 4, 1, 4, 1)
    var consumed = t.handle(ev)
    check_(!consumed && t.cursor == 5,
           "TextInput mouse hover: ignored, cursor unchanged")
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
