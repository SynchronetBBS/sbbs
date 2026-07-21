// Self-tests for ui_style.wren — Style merge semantics + Theme cascade.
//
// Pure in-process; no connection or hooks required.  Run from the
// Wren console (Ctrl+`) with:
//   import "ui_style_test" for UiStyleTest
//   UiStyleTest.run()
// or via the wrentest suite (Alt+T).

import "ui_style" for Style, Theme

class UiStyleTest {
  static run() {
    __pass = 0
    __fail = 0
    System.print("=== ui_style self-test starting ===")

    testStyleConstruct_()
    testStyleComplete_()
    testStyleEmpty_()
    testStyleEquality_()
    testStyleMergeAllFields_()
    testStyleMergeFillsNulls_()
    testStyleMergeLeftWins_()
    testStyleMergeWithNull_()
    testStyleMergeChain_()

    testThemeLookupExact_()
    testThemeLookupCascadeOneLevel_()
    testThemeCascadeMultiLevel_()
    testThemeCascadeToDefault_()
    testThemeUnknownRoleGetsDefault_()
    testThemeFieldWiseCascade_()
    testThemeGlyphsAccessor_()

    testParentRole_()

    testDefaultThemeComplete_()
    testDefaultThemeKnownRoles_()
    testDefaultThemeGlyphs_()

    var total = __pass + __fail
    System.print("=== ui_style: %(total) tests, %(__pass) pass, %(__fail) fail ===")
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

  // ----- Style ----------------------------------------------------

  static testStyleConstruct_() {
    var s = Style.new(0, 0x1F, 0xFFFFFF, 0x0000A8)
    check_(s.font == 0 && s.legacyAttr == 0x1F &&
           s.fgRgb == 0xFFFFFF && s.bgRgb == 0x0000A8,
           "Style.new + getters")
  }

  static testStyleComplete_() {
    var full    = Style.new(0, 0x1F, 0xFFFFFF, 0x0000A8)
    var noFont  = Style.new(null, 0x1F, 0xFFFFFF, 0x0000A8)
    var noAttr  = Style.new(0, null, 0xFFFFFF, 0x0000A8)
    var noFg    = Style.new(0, 0x1F, null, 0x0000A8)
    var noBg    = Style.new(0, 0x1F, 0xFFFFFF, null)
    var allNull = Style.new(null, null, null, null)
    check_(full.complete, "Style.complete: all populated")
    check_(!noFont.complete, "Style.complete: missing font")
    check_(!noAttr.complete, "Style.complete: missing legacyAttr")
    check_(!noFg.complete, "Style.complete: missing fgRgb")
    check_(!noBg.complete, "Style.complete: missing bgRgb")
    check_(!allNull.complete, "Style.complete: all null")
  }

  static testStyleEmpty_() {
    var e = Style.empty
    check_(e.font == null && e.legacyAttr == null &&
           e.fgRgb == null && e.bgRgb == null && !e.complete,
           "Style.empty has all-null fields")
  }

  static testStyleEquality_() {
    var a = Style.new(0, 0x1F, 0xFFFFFF, 0x0000A8)
    var b = Style.new(0, 0x1F, 0xFFFFFF, 0x0000A8)
    var c = Style.new(0, 0x1F, 0xFFFFFF, 0x000000)
    check_(a == b, "Style equality: identical fields")
    check_(a != c, "Style inequality: differing bgRgb")
    check_(!(a == "Style"), "Style equality: rejects non-Style")
  }

  static testStyleMergeAllFields_() {
    // Both fully populated — left wins, right is irrelevant.
    var lhs = Style.new(0, 0x1F, 0xFFFFFF, 0x0000A8)
    var rhs = Style.new(1, 0x70, 0x000000, 0xAAAAAA)
    var m   = lhs | rhs
    check_(m == lhs, "Style |: both populated, left wins")
  }

  static testStyleMergeFillsNulls_() {
    // Left has only legacyAttr; right has the rest.
    var lhs = Style.new(null, 0x1F, null, null)
    var rhs = Style.new(1, 0x70, 0xFFFFFF, 0x0000A8)
    var m   = lhs | rhs
    check_(m.font       == 1 &&
           m.legacyAttr == 0x1F &&     // left wins on this one
           m.fgRgb      == 0xFFFFFF &&
           m.bgRgb      == 0x0000A8,
           "Style |: nulls on left fall through to right")
  }

  static testStyleMergeLeftWins_() {
    // Left has fgRgb, right has different fgRgb — left wins.
    var lhs = Style.new(null, null, 0xFFFF00, null)
    var rhs = Style.new(1, 0x70, 0x000000, 0xAAAAAA)
    var m   = lhs | rhs
    check_(m.fgRgb == 0xFFFF00, "Style |: non-null left wins over right")
    check_(m.font == 1 && m.legacyAttr == 0x70 && m.bgRgb == 0xAAAAAA,
           "Style |: other fields fill from right")
  }

  static testStyleMergeWithNull_() {
    var s = Style.new(0, 0x1F, 0xFFFFFF, 0x0000A8)
    check_((s | null) == s, "Style | null returns self")
  }

  static testStyleMergeChain_() {
    // Three-way cascade: most-specific first, then mid, then base.
    var top  = Style.new(null, null, 0xFFFF00, null)
    var mid  = Style.new(null, 0x1E, null, null)
    var base = Style.new(0, 0x1F, 0xFFFFFF, 0x0000A8)
    var m    = top | mid | base
    check_(m.font       == 0       &&
           m.legacyAttr == 0x1E    &&     // from mid
           m.fgRgb      == 0xFFFF00 &&    // from top
           m.bgRgb      == 0x0000A8,      // from base
           "Style | chain: each level contributes its non-null fields")
  }

  // ----- Theme.style cascade --------------------------------------

  static makeTheme_() {
    return Theme.new({
      "default":
        Style.new(0, 0x1F, 0xFFFFFF, 0x0000A8),
      "menu":
        Style.new(null, 0x17, null, null),
      "menu.item":
        Style.new(null, null, 0xCCCCCC, null),
      "menu.item.focused":
        Style.new(null, 0x70, null, 0xAAAAAA)
    }, {
      "frame.display.topLeft": "+",
      "frame.display.top":     "-"
    })
  }

  static testThemeLookupExact_() {
    var t = Theme.new({
      "default": Style.new(0, 0x1F, 0xFFFFFF, 0x0000A8),
      "exact":   Style.new(1, 0x70, 0x000000, 0xAAAAAA)
    }, {})
    var s = t.style("exact")
    check_(s.font == 1 && s.legacyAttr == 0x70 &&
           s.fgRgb == 0x000000 && s.bgRgb == 0xAAAAAA,
           "Theme.style: complete role hits exact match")
  }

  static testThemeLookupCascadeOneLevel_() {
    // "menu.item" supplies fgRgb; "menu" supplies legacyAttr;
    // "default" supplies font + bgRgb.
    var t = makeTheme_()
    var s = t.style("menu.item")
    check_(s.font       == 0       &&
           s.legacyAttr == 0x17    &&
           s.fgRgb      == 0xCCCCCC &&
           s.bgRgb      == 0x0000A8,
           "Theme.style: one-level cascade")
  }

  static testThemeCascadeMultiLevel_() {
    var t = makeTheme_()
    var s = t.style("menu.item.focused")
    // focused: legacyAttr 0x70, bg 0xAAAAAA
    // menu.item: fg 0xCCCCCC
    // menu:      legacyAttr 0x17 (overridden by focused)
    // default:   font 0, fg 0xFFFFFF (overridden by item), bg 0x0000A8 (overridden)
    check_(s.font       == 0       &&
           s.legacyAttr == 0x70    &&     // most-specific wins
           s.fgRgb      == 0xCCCCCC &&    // from item, not default
           s.bgRgb      == 0xAAAAAA,      // from focused
           "Theme.style: multi-level cascade")
  }

  static testThemeCascadeToDefault_() {
    var t = makeTheme_()
    // "menu" only sets legacyAttr; everything else from default.
    var s = t.style("menu")
    check_(s.font       == 0       &&
           s.legacyAttr == 0x17    &&
           s.fgRgb      == 0xFFFFFF &&
           s.bgRgb      == 0x0000A8,
           "Theme.style: cascade to default")
  }

  static testThemeUnknownRoleGetsDefault_() {
    var t = makeTheme_()
    var s = t.style("nope.never.heard.of.it")
    var d = t.style("default")
    check_(s == d, "Theme.style: unknown role falls all the way to default")
  }

  static testThemeFieldWiseCascade_() {
    // Verify that a partial role doesn't shadow a more-specific
    // sibling's other fields.  Role "a.b" sets legacyAttr only; "a"
    // sets fgRgb only; default fills the rest.
    var t = Theme.new({
      "default": Style.new(0, 0x07, 0xCCCCCC, 0x000000),
      "a":       Style.new(null, null, 0xFFFF00, null),
      "a.b":     Style.new(null, 0x4F, null, null)
    }, {})
    var s = t.style("a.b")
    check_(s.font       == 0       &&
           s.legacyAttr == 0x4F    &&     // a.b
           s.fgRgb      == 0xFFFF00 &&    // a
           s.bgRgb      == 0x000000,      // default
           "Theme.style: field-wise cascade independent per field")
  }

  static testThemeGlyphsAccessor_() {
    var t = makeTheme_()
    check_(t.glyphs["frame.display.topLeft"] == "+" &&
           t.glyphs["frame.display.top"]     == "-" &&
           t.glyphs["nonexistent"]   == null,
           "Theme.glyphs returns the configured map")
  }

  // ----- parentRole_ edge cases (via Theme instance) --------------

  static testParentRole_() {
    var t = makeTheme_()
    check_(t.parentRole_("a.b.c") == "a.b",  "parentRole_: trims one suffix")
    check_(t.parentRole_("a.b")   == "a",    "parentRole_: two-segment")
    check_(t.parentRole_("a")     == "",     "parentRole_: single segment → empty")
    check_(t.parentRole_("")      == "",     "parentRole_: empty stays empty")
    check_(t.parentRole_("a.")    == "a",    "parentRole_: trailing dot")
    check_(t.parentRole_(".a")    == "",     "parentRole_: leading dot")
    check_(t.parentRole_("a.b.")  == "a.b",  "parentRole_: inner trailing")
  }

  // ----- Default theme sanity -------------------------------------

  static testDefaultThemeComplete_() {
    var d = Theme.default
    var s = d.style("default")
    check_(s.complete, "Theme.default: 'default' role is complete")
  }

  static testDefaultThemeKnownRoles_() {
    var d = Theme.default
    // Spot-check a handful of known roles produce complete Styles.
    var roles = [
      "frame", "frame.inactive",
      "title", "title.inactive",
      "menu.item", "menu.item.focused", "menu.item.disabled",
      "input", "input.focused",
      "statusbar", "scrollbar.track", "scrollbar.thumb",
      "help", "popup", "popup.frame", "popup.frame.inactive"
    ]
    var allComplete = true
    var failingRole = null
    for (r in roles) {
      var s = d.style(r)
      if (!s.complete) {
        allComplete = false
        failingRole = r
      }
    }
    check_(allComplete,
           "Theme.default: all known roles cascade to complete " +
           "(failing: %(failingRole))")
  }

  static testDefaultThemeGlyphs_() {
    var d = Theme.default
    var g = d.glyphs
    var keys = [
      "frame.display.topLeft", "frame.display.top",
      "frame.display.topRight", "frame.display.left",
      "frame.display.right", "frame.display.bottomLeft",
      "frame.display.bottom", "frame.display.bottomRight",
      "frame.display.tee.left", "frame.display.tee.right",
      "frame.display.title.left", "frame.display.title.right",
      "frame.display.tee.top", "frame.display.tee.bottom",
      "frame.display.cross", "frame.display.separator",
      "frame.control.topLeft", "frame.control.top",
      "frame.control.topRight", "frame.control.left",
      "frame.control.right", "frame.control.bottomLeft",
      "frame.control.bottom", "frame.control.bottomRight",
      "frame.control.tee.left", "frame.control.tee.right",
      "frame.control.title.left", "frame.control.title.right",
      "frame.control.tee.top", "frame.control.tee.bottom",
      "frame.control.cross", "frame.control.separator",
      "scrollbar.track", "scrollbar.thumb", "scrollbar.separator",
      "arrow.up", "arrow.down", "arrow.left", "arrow.right",
      "tag.on", "tag.off"
    ]
    var allPresent = true
    var missing = null
    for (k in keys) {
      if (g[k] == null) {
        allPresent = false
        missing = k
      }
    }
    check_(allPresent,
           "Theme.default: all expected glyphs present " +
           "(missing: %(missing))")
  }
}
