// Self-tests for ui_draw (Painter) + ui_pane.  Each test allocates a
// Surface directly and inspects its cells — no Screen.save/restore
// or active UI, so the tests remain isolated.

import "ui_style"  for Style, Theme
import "ui_widget" for Rect
import "ui_draw"   for Painter
import "ui_pane"   for Pane
import "ui_list"   for ListView
import "syncterm"  for Surface, Cell, Screen

class UiDrawTest {
  static run() {
    __pass = 0
    __fail = 0
    System.print("=== ui_draw self-test starting ===")

    // Painter helpers (no Surface needed).
    testTemplate_()
    testApplyStyleAllFields_()
    testApplyStyleSkipsNulls_()

    // Painter primitives on a Surface.
    testFill_()
    testFillStyleApplied_()
    testText_()
    testTextTruncate_()
    testTextEmpty_()
    testHline_()
    testVline_()
    testFrame_()
    testFrameInteriorUntouched_()
    testFrameTooSmall_()
    testFrameTitleCentered_()
    testFrameTitleTruncated_()
    testFrameTitleNullPlain_()
    testScrollbarClickInvertsSingleCellThumb_()

    // Pane.
    testPaneTitleSetter_()
    testPaneInnerBoundsNoBounds_()
    testPaneInnerBoundsCorrect_()
    testPaneConstrainedFitSettlesScrollbar_()
    testPaneFitContentToScreen_()
    testPaneFitToScreen_()
    testPaneFrameKinds_()
    testPaneRejectsUnknownFrameKind_()
    testPaneDrawFramesWithTitle_()
    testPaneDrawClearsInterior_()

    var total = __pass + __fail
    System.print("=== ui_draw: %(total) tests, %(__pass) pass, %(__fail) fail ===")
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

  static testStyle_ {
    return Style.new(0, 0x1F, 0xFFFFFF, 0x0000A8)
  }

  static styleB_ {
    return Style.new(0, 0x70, 0x000000, 0xAAAAAA)
  }

  // ----- Painter.template + applyStyle -----------------------------

  static testTemplate_() {
    var c = Painter.template("X", testStyle_)
    check_(c.ch == "X" && c.font == 0 && c.legacyAttr == 0x1F &&
           c.fgRgb == 0xFFFFFF && c.bgRgb == 0x0000A8,
           "Painter.template: char + all Style fields applied")
  }

  static testApplyStyleAllFields_() {
    var c = Cell.new()
    Painter.applyStyle(c, testStyle_)
    check_(c.font == 0 && c.legacyAttr == 0x1F &&
           c.fgRgb == 0xFFFFFF && c.bgRgb == 0x0000A8,
           "Painter.applyStyle: all four fields applied")
  }

  static testApplyStyleSkipsNulls_() {
    var c = Cell.new()
    var orig = c.font
    var partial = Style.new(null, 0x70, null, null)
    Painter.applyStyle(c, partial)
    check_(c.legacyAttr == 0x70 && c.font == orig,
           "Painter.applyStyle: null Style fields leave Cell defaults")
  }

  // ----- Painter primitives on a Surface ---------------------------

  static testFill_() {
    var s = Surface.new(5, 2)
    Painter.fill(s, Rect.new(0, 0, 5, 2), "X", testStyle_)
    var ok = true
    var y = 0
    while (y < 2) {
      var x = 0
      while (x < 5) {
        if (s.cellAt(x, y).ch != "X") ok = false
        x = x + 1
      }
      y = y + 1
    }
    check_(ok, "Painter.fill: every cell has the char")
  }

  static testFillStyleApplied_() {
    var s = Surface.new(3, 1)
    Painter.fill(s, Rect.new(0, 0, 3, 1), "Z", testStyle_)
    check_(s.cellAt(0, 0).legacyAttr == 0x1F &&
           s.cellAt(2, 0).legacyAttr == 0x1F,
           "Painter.fill: Style applied to filled cells")
  }

  static testText_() {
    var s = Surface.new(10, 1)
    Painter.fill(s, Rect.new(0, 0, 10, 1), " ", testStyle_)
    Painter.text(s, 0, 0, "Hello", testStyle_)
    check_(s.cellAt(0, 0).ch == "H" && s.cellAt(1, 0).ch == "e" &&
           s.cellAt(4, 0).ch == "o" && s.cellAt(0, 0).legacyAttr == 0x1F,
           "Painter.text: writes string + applies style")
  }

  static testTextTruncate_() {
    var s = Surface.new(10, 1)
    Painter.fill(s, Rect.new(0, 0, 10, 1), " ", testStyle_)
    Painter.text(s, 0, 0, "ABCDE", testStyle_, 3)
    check_(s.cellAt(0, 0).ch == "A" && s.cellAt(2, 0).ch == "C" &&
           s.cellAt(3, 0).ch == " ",
           "Painter.text: maxW caps the cell count")
  }

  static testTextEmpty_() {
    var s = Surface.new(5, 1)
    Painter.fill(s, Rect.new(0, 0, 5, 1), "Z", testStyle_)
    Painter.text(s, 0, 0, "", testStyle_)
    check_(s.cellAt(0, 0).ch == "Z",
           "Painter.text: empty string is no-op")
  }

  static testHline_() {
    var s = Surface.new(8, 1)
    Painter.fill(s, Rect.new(0, 0, 8, 1), " ", testStyle_)
    Painter.hline(s, 1, 0, 5, "-", styleB_)
    check_(s.cellAt(0, 0).ch == " " && s.cellAt(1, 0).ch == "-" &&
           s.cellAt(5, 0).ch == "-" && s.cellAt(6, 0).ch == " " &&
           s.cellAt(1, 0).legacyAttr == 0x70,
           "Painter.hline: starts at x, len cells of `ch`, style applied")
  }

  static testVline_() {
    var s = Surface.new(1, 5)
    Painter.fill(s, Rect.new(0, 0, 1, 5), " ", testStyle_)
    Painter.vline(s, 0, 1, 3, "|", styleB_)
    check_(s.cellAt(0, 0).chByte == 0x20 &&
           s.cellAt(0, 1).chByte == 0x7C &&
           s.cellAt(0, 2).chByte == 0x7C &&
           s.cellAt(0, 3).chByte == 0x7C &&
           s.cellAt(0, 4).chByte == 0x20,
           "Painter.vline: starts at y, len cells, style applied")
  }

  static testFrame_() {
    var glyphs = Theme.default.glyphs
    var s = Surface.new(6, 4)
    Painter.fill(s, Rect.new(0, 0, 6, 4), " ", testStyle_)
    Painter.frame(s, Rect.new(0, 0, 6, 4), glyphs, testStyle_)
    check_(s.cellAt(0, 0).ch == "┌" && s.cellAt(1, 0).ch == "─" &&
           s.cellAt(5, 0).ch == "┐" &&
           s.cellAt(0, 3).ch == "└" && s.cellAt(5, 3).ch == "┘" &&
           s.cellAt(0, 1).ch == "│" && s.cellAt(5, 1).ch == "│",
           "Painter.frame: corners, edges, sides all use frame.* glyphs")
  }

  static testFrameInteriorUntouched_() {
    var glyphs = Theme.default.glyphs
    var s = Surface.new(6, 4)
    Painter.fill(s, Rect.new(1, 1, 4, 2), "Z", testStyle_)
    Painter.frame(s, Rect.new(0, 0, 6, 4), glyphs, testStyle_)
    var ok = true
    var y = 1
    while (y < 3) {
      var x = 1
      while (x < 5) {
        if (s.cellAt(x, y).ch != "Z") ok = false
        x = x + 1
      }
      y = y + 1
    }
    check_(ok, "Painter.frame: interior cells are not overwritten")
  }

  static testFrameTooSmall_() {
    var glyphs = Theme.default.glyphs
    var s = Surface.new(5, 1)
    Painter.fill(s, Rect.new(0, 0, 5, 1), "Z", testStyle_)
    Painter.frame(s, Rect.new(0, 0, 1, 5), glyphs, testStyle_)   // w < 2
    Painter.frame(s, Rect.new(0, 0, 5, 1), glyphs, testStyle_)   // h < 2
    check_(s.cellAt(0, 0).ch == "Z" && s.cellAt(4, 0).ch == "Z",
           "Painter.frame: too-small rect is a silent no-op")
  }

  static testFrameTitleCentered_() {
    var glyphs = Theme.default.glyphs
    var s = Surface.new(12, 3)
    Painter.fill(s, Rect.new(0, 0, 12, 3), " ", testStyle_)
    Painter.frameTitle(s, Rect.new(0, 0, 12, 3), glyphs, testStyle_,
                       "OK", styleB_)
    // Width 12, "┤ OK ├" is 6 cells; startX = (12-2-4)/2 = 3.
    // Layout: 0:┌ 1-2:─ 3:┤ 4:" " 5:O 6:K 7:" " 8:├ 9-10:─ 11:┐
    check_(s.cellAt(0, 0).ch == "┌" && s.cellAt(2, 0).ch == "─" &&
           s.cellAt(3, 0).ch == "┤" && s.cellAt(4, 0).ch == " " &&
           s.cellAt(5, 0).ch == "O" && s.cellAt(6, 0).ch == "K" &&
           s.cellAt(7, 0).ch == " " && s.cellAt(8, 0).ch == "├" &&
           s.cellAt(9, 0).ch == "─" && s.cellAt(11, 0).ch == "┐" &&
           s.cellAt(5, 0).legacyAttr == 0x70 &&
           s.cellAt(4, 0).legacyAttr == 0x70,   // padding inherits title style
           "Painter.frameTitle: title centered with bracketed spacing")
  }

  static testFrameTitleTruncated_() {
    var glyphs = Theme.default.glyphs
    var s = Surface.new(8, 3)
    Painter.fill(s, Rect.new(0, 0, 8, 3), " ", testStyle_)
    Painter.frameTitle(s, Rect.new(0, 0, 8, 3), glyphs, testStyle_,
                       "Hello", styleB_)
    // maxW = 8 - 6 = 2; visible = 2 ("He"); startX = (8-2-4)/2 = 1.
    // Layout: 0:┌ 1:┤ 2:" " 3:H 4:e 5:" " 6:├ 7:┐
    check_(s.cellAt(0, 0).ch == "┌" && s.cellAt(1, 0).ch == "┤" &&
           s.cellAt(2, 0).ch == " " && s.cellAt(3, 0).ch == "H" &&
           s.cellAt(4, 0).ch == "e" && s.cellAt(5, 0).ch == " " &&
           s.cellAt(6, 0).ch == "├" && s.cellAt(7, 0).ch == "┐",
           "Painter.frameTitle: title truncated to maxW with brackets + spaces")
  }

  static testFrameTitleNullPlain_() {
    var glyphs = Theme.default.glyphs
    var s = Surface.new(6, 3)
    Painter.fill(s, Rect.new(0, 0, 6, 3), "Z", testStyle_)
    Painter.frameTitle(s, Rect.new(0, 0, 6, 3), glyphs, testStyle_, null, styleB_)
    check_(s.cellAt(0, 0).ch == "┌" && s.cellAt(2, 0).ch == "─" &&
           s.cellAt(5, 0).ch == "┐",
           "Painter.frameTitle: null title draws plain frame")
  }

  static testScrollbarClickInvertsSingleCellThumb_() {
    var h = 7
    var total = 100
    var viewport = 7
    var glyphs = Theme.default.glyphs
    var ok = true
    var py = 1
    while (py < h - 1) {
      var top = Painter.scrollbarClick(py, h, total, viewport, 0)
      var s = Surface.new(1, h)
      Painter.scrollbar(s, 0, 0, h, top, total, viewport, glyphs,
                        testStyle_, styleB_)
      if (s.cellAt(0, py).ch != glyphs["scrollbar.thumb"]) ok = false
      py = py + 1
    }
    check_(ok,
           "Painter.scrollbarClick: one-cell thumb lands on clicked row")
  }

  // ----- Pane ------------------------------------------------------

  static testPaneTitleSetter_() {
    var p = Pane.new()
    p.title = "Files"
    check_(p.title == "Files" && p.dirty,
           "Pane.title= stores + marks dirty")
  }

  static testPaneInnerBoundsNoBounds_() {
    var p = Pane.new()
    check_(p.innerBounds == null,
           "Pane.innerBounds is null when bounds are unset")
  }

  static testPaneInnerBoundsCorrect_() {
    var p = Pane.new()
    p.bounds = Rect.new(5, 3, 20, 10)
    var ib = p.innerBounds
    check_(ib.x == 6 && ib.y == 4 && ib.w == 18 && ib.h == 8,
           "Pane.innerBounds: inset by 1 on each side")
  }

  static testPaneConstrainedFitSettlesScrollbar_() {
    var p = Pane.new()
    p.title = "X"
    var l = ListView.new()
    var items = []
    for (i in 0...Screen.size[1]) items.add("0123456789")
    l.items = items
    p.add(l)
    p.fitContent(100, Screen.size[1] - 2)
    check_(p.bounds.w == 15 && p.bounds.h == Screen.size[1] - 2 &&
           l.bounds.w == 13,
           "Pane.fitContent(max): one call includes a list scrollbar")
  }

  static testPaneFitContentToScreen_() {
    var p = Pane.new()
    p.shadow = true
    var l = ListView.new()
    var items = []
    var item = "0123456789012345678901234567890123456789012345678901234567890123456789" +
        "0123456789012345678901234567890123456789012345678901234567890123456789"
    for (i in 0...Screen.size[1]) {
      items.add(item)
    }
    l.items = items
    p.add(l)
    p.fitContentToScreen()
    var size = Screen.size
    check_(p.bounds.w == size[0] - 4 && p.bounds.h <= size[1] - 4 &&
           p.bounds.y >= 2 && p.bounds.right + 2 <= size[0] &&
           p.bounds.bottom + 1 <= size[1] - 2 &&
           l.bounds == p.innerBounds,
           "Pane.fitContentToScreen: preserves title, footer, and shadow room")
  }

  static testPaneFitToScreen_() {
    var p = Pane.new()
    p.fitToScreen(78, 20)
    var size = Screen.size
    var maxW = (size[0] - 4).max(1)
    var maxH = (size[1] - 4).max(1)
    check_(p.bounds.w == 78.min(maxW) && p.bounds.h == 20.min(maxH) &&
           p.bounds.y >= 2 && p.bounds.right + 2 <= size[0] &&
           p.bounds.bottom + 1 <= size[1] - 2,
           "Pane.fitToScreen: keeps a fixed modal clear of menu chrome")
  }

  static testPaneFrameKinds_() {
    var p = Pane.new()
    p.bounds = Rect.new(1, 1, 8, 3)
    p.helpable = false
    p.closeable = false
    var controlCorner = p.draw().cellAt(0, 0).ch
    p.frameKind = "display"
    var displayCorner = p.draw().cellAt(0, 0).ch
    check_(controlCorner == "╔" && displayCorner == "┌",
           "Pane.frameKind selects control and display glyph families")
  }

  static testPaneRejectsUnknownFrameKind_() {
    var p = Pane.new()
    var error = Fiber.new { p.frameKind = "double" }.try()
    check_(error is String && error.contains("Pane.frameKind"),
           "Pane.frameKind rejects appearance-based values")
  }

  static testPaneDrawFramesWithTitle_() {
    var p = Pane.new()
    p.bounds      = Rect.new(1, 1, 10, 4)
    // Test the informational embedded-title look.  Pane defaults to a
    // control frame with titleAsBar and corner buttons, so opt out of
    // those defaults to isolate Painter.frameTitle's display layout.
    p.frameKind   = "display"
    p.titleAsBar  = false
    p.helpable    = false
    p.closeable   = false
    p.title       = "Hi"
    var s = p.draw()
    // Width 10, title "Hi" (2 cells); startX = (10-2-4)/2 = 2.
    // Layout: 0:┌ 1:─ 2:┤ 3:" " 4:H 5:i 6:" " 7:├ 8:─ 9:┐
    check_(s.cellAt(0, 0).ch == "┌" && s.cellAt(2, 0).ch == "┤" &&
           s.cellAt(4, 0).ch == "H" && s.cellAt(5, 0).ch == "i" &&
           s.cellAt(7, 0).ch == "├" && s.cellAt(9, 0).ch == "┐",
           "Pane.draw: frames with centered title")
  }

  static testPaneDrawClearsInterior_() {
    var p = Pane.new()
    p.bounds = Rect.new(1, 1, 8, 3)
    var s = p.draw()
    // After draw, the interior (1,1)..(6,1) of the surface should be
    // background spaces.
    var ok = true
    var x = 1
    while (x < 7) {
      if (s.cellAt(x, 1).ch != " ") ok = false
      x = x + 1
    }
    check_(ok, "Pane.draw: clears interior to background")
  }
}
