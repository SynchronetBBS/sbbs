// SyncTERM Wren UI library — Style + Theme.
//
// Style is a four-field tuple (font, legacyAttr, fgRgb, bgRgb); any
// field may be null meaning "inherit".  Two Styles compose with `|`:
// the left-hand side wins on every non-null field, the right fills
// the gaps.  Reads as "self over other".
//
// Theme is a sparse role → Style map plus a Glyphs map.
// `theme.style(role)` walks dotted role parents and merges until all
// four fields are populated:
//   "menu.item.focused" → "menu.item" → "menu" → "default"
// "default" is required to be fully populated and acts as the
// terminator of the cascade.

class Style {
  construct new(font, legacyAttr, fgRgb, bgRgb) {
    _font       = font
    _legacyAttr = legacyAttr
    _fgRgb      = fgRgb
    _bgRgb      = bgRgb
  }

  font       { _font       }
  legacyAttr { _legacyAttr }
  fgRgb      { _fgRgb      }
  bgRgb      { _bgRgb      }

  // True when every field is non-null (no further cascade needed).
  complete {
    return _font != null && _legacyAttr != null &&
           _fgRgb != null && _bgRgb != null
  }

  // Merge: pick non-null fields from this, fall back to other for
  // nulls.  Reads as "self over other" — left side has precedence.
  // `null` on the right is treated as Style.empty so the operator is
  // always safe to chain.
  |(other) {
    if (other == null) return this
    return Style.new(
      _font       == null ? other.font       : _font,
      _legacyAttr == null ? other.legacyAttr : _legacyAttr,
      _fgRgb      == null ? other.fgRgb      : _fgRgb,
      _bgRgb      == null ? other.bgRgb      : _bgRgb)
  }

  ==(other) {
    if (!(other is Style)) return false
    return _font       == other.font       &&
           _legacyAttr == other.legacyAttr &&
           _fgRgb      == other.fgRgb      &&
           _bgRgb      == other.bgRgb
  }

  !=(other) { !(this == other) }

  toString {
    return "Style(font=%(_font), legacy=%(_legacyAttr), " +
           "fg=%(_fgRgb), bg=%(_bgRgb))"
  }

  // The all-null sentinel — useful as the starting point of a cascade.
  static empty { Style.new(null, null, null, null) }
}

// Glyphs wraps a name → glyph map and supports per-entry ASCII
// fallbacks.  An entry value may be either a single String (always
// used) or a 2-element List [primary, fallback]: primary is the rich
// Unicode glyph, fallback is an ASCII-safe substitute used when the
// active codepage / font can't represent the primary.
//
// `asciiOnly` flips the lookup to always return fallback when one is
// defined.  Unknown names return null.  String-only entries don't
// have a fallback and ignore `asciiOnly`.
class Glyphs {
  construct new(map) {
    _map = map
    _asciiOnly = false
  }

  asciiOnly     { _asciiOnly }
  asciiOnly=(b) { _asciiOnly = b }

  [name] {
    var g = _map[name]
    if (g == null) return null
    if (g is List) return _asciiOnly ? g[1] : g[0]
    return g
  }
}

// A Theme bundles role → Style and a Glyphs (name → glyph).
// `glyphsInput` may be a Glyphs instance (used as-is) or a plain Map
// (wrapped in a fresh Glyphs).  Themes are immutable after
// construction; build a new one to mutate.
class Theme {
  construct new(roles, glyphsInput) {
    _roles  = roles
    _glyphs = glyphsInput is Glyphs ? glyphsInput : Glyphs.new(glyphsInput)
  }

  glyphs { _glyphs }
  roles  { _roles  }

  // Resolve `role` to a fully-populated Style.  Walks the dotted role
  // hierarchy, merging any partial Styles at each level until every
  // field is populated.  The "default" role is the cascade terminator
  // and MUST itself be a complete Style (no nulls) — themes that omit
  // it will end up with nulls in the result.
  style(role) {
    var s = lookup_(role)
    if (s.complete) return s
    while (true) {
      var parent = parentRole_(role)
      if (parent == "") parent = "default"
      role = parent
      s = s | lookup_(role)
      if (s.complete || role == "default") break
    }
    return s
  }

  // Internal: look up `role` in the Style map; return Style.empty for
  // unknown roles so the cascade can compose unconditionally.
  lookup_(role) {
    var s = _roles[role]
    return s == null ? Style.empty : s
  }

  // Internal: strip the last dotted suffix from `role`.
  //   "menu.item.focused" → "menu.item"
  //   "menu"              → ""
  //   ""                  → ""
  parentRole_(role) {
    if (role.count == 0) return ""
    var b = role.bytes
    var i = role.count - 1
    while (i >= 0) {
      if (b[i] == 0x2E) return role[0...i]
      i = i - 1
    }
    return ""
  }

  // Built-in default theme.  Mirrors the classic SyncTERM look:
  // white-on-blue, lightbar selection, yellow titles, single-line
  // box-drawing borders.  Both legacy attrs and 24-bit RGB are
  // populated so the renderer can pick whichever the terminal
  // supports.  Font slot 0 (cp437English) is the safe baseline.
  //
  // Memoized: every call returns the same Theme instance so that
  // `someTheme == Theme.default` works as identity equality.
  static default {
    if (__default == null) __default = buildDefault_()
    return __default
  }

  static buildDefault_() {
    var roles = {
      // Cascade terminator — every field set.
      "default":
        Style.new(0, 0x1F, 0xFFFFFF, 0x0000A8),

      // Frame defaults inherit "default"; focused gets a yellow tint.
      "frame":
        Style.new(null, 0x1F, 0xFFFFFF, 0x0000A8),
      "frame.focused":
        Style.new(null, 0x1E, 0xFFFF55, null),

      // Title bars: bright white on blue.
      "title":
        Style.new(null, 0x1F, 0xFFFFFF, 0x0000A8),
      "title.focused":
        Style.new(null, 0x1E, 0xFFFF55, null),

      // Menu items: normal inherits default; focused = lightbar.
      "menu.item":
        Style.new(null, 0x1F, null, null),
      "menu.item.focused":
        Style.new(null, 0x70, 0x000000, 0xAAAAAA),
      "menu.item.disabled":
        Style.new(null, 0x18, 0x808080, null),

      // List items: same family as menu but separately themable so a
      // long-running list (file picker, scrollback) can diverge.
      "list.item":
        Style.new(null, 0x1F, null, null),
      "list.item.focused":
        Style.new(null, 0x70, 0x000000, 0xAAAAAA),
      "list.item.disabled":
        Style.new(null, 0x18, 0x808080, null),

      // Inputs: yellow text by default, white when focused.
      "input":
        Style.new(null, 0x1E, 0xFFFF55, 0x0000A8),
      "input.focused":
        Style.new(null, 0x1F, 0xFFFFFF, 0x0000A8),

      // Status bar: black on cyan.
      "statusbar":
        Style.new(null, 0x30, 0x000000, 0x00AAAA),

      // Scroll bar: dim track, bright thumb (handled by glyphs +
      // these Styles).
      "scrollbar.track":
        Style.new(null, 0x17, 0xAAAAAA, 0x0000A8),
      "scrollbar.thumb":
        Style.new(null, 0x1F, 0xFFFFFF, 0x0000A8),

      // Help text: dim grey.
      "help":
        Style.new(null, 0x17, 0xAAAAAA, 0x0000A8),

      // Modal popups: yellow on red — distinct from the main window.
      "popup":
        Style.new(null, 0x4E, 0xFFFF55, 0xAA0000),
      "popup.frame":
        Style.new(null, 0x4F, 0xFFFFFF, 0xAA0000),
      "popup.frame.focused":
        Style.new(null, 0x4F, 0xFFFFFF, 0xAA0000)
    }

    // Glyphs are stored as [primary, fallback] pairs: primary is a
    // rich Unicode codepoint (UTF-8 in source); fallback is an
    // ASCII-safe substitute used when Theme.glyphs.asciiOnly is set
    // (e.g. on emulations / fonts where the rich codepoint can't be
    // represented).  Cell.ch= decodes the codepoint and maps it to
    // CP437 (cell storage); the renderer then maps CP437 to the
    // active emulation.  When a theme is locked to ASCII, the rich
    // glyph is bypassed entirely so no mapping ever fails.
    var glyphs = {
      "background":          [" ",  " "],

      "frame.topLeft":       ["┌", "+"],
      "frame.top":           ["─", "-"],
      "frame.topRight":      ["┐", "+"],
      "frame.left":          ["│", "|"],
      "frame.right":         ["│", "|"],
      "frame.bottomLeft":    ["└", "+"],
      "frame.bottom":        ["─", "-"],
      "frame.bottomRight":   ["┘", "+"],
      "frame.tee.left":      ["├", "+"],
      "frame.tee.right":     ["┤", "+"],
      "frame.tee.top":       ["┬", "+"],
      "frame.tee.bottom":    ["┴", "+"],
      "frame.cross":         ["┼", "+"],
      "frame.separator":     ["─", "-"],

      "scrollbar.track":     ["░", ":"],
      "scrollbar.thumb":     ["█", "#"],

      "arrow.up":            ["↑", "^"],
      "arrow.down":          ["↓", "v"],
      "arrow.right":         ["►", ">"],
      "arrow.left":          ["◄", "<"],

      "focus.left":          ["►", ">"],
      "focus.right":         ["◄", "<"],

      "check.on":            ["■", "X"],
      "check.off":           [" ", " "],
      "radio.on":            ["●", "*"],
      "radio.off":           ["○", "o"],
      "tag.on":              ["►", ">"],
      "tag.off":             [" ", " "]
    }

    return Theme.new(roles, glyphs)
  }
}
