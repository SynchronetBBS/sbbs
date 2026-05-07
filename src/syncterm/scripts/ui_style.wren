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

import "syncterm" for Codepage

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
// `asciiOnly` forces the fallback for every entry that has one.
// Otherwise, lookup auto-promotes to the fallback when the primary's
// first codepoint doesn't map to CP437 — Cell.ch= writes CP437 and
// substitutes `?` for unmappable codepoints, so a primary that isn't
// in the table would otherwise silently render as a question mark.
// Per-entry resolutions are cached so the encoding probe only runs
// once per glyph name.  Unknown names return null.  String-only
// entries don't have a fallback and ignore `asciiOnly`.
class Glyphs {
  construct new(map) {
    _map        = map
    _asciiOnly  = false
    _cache      = {}     // name -> resolved string (rich)
    _cacheAscii = {}     // name -> resolved string (asciiOnly)
  }

  asciiOnly     { _asciiOnly }
  asciiOnly=(b) { _asciiOnly = b }

  [name] {
    var cache = _asciiOnly ? _cacheAscii : _cache
    if (cache.containsKey(name)) return cache[name]
    var g = _map[name]
    if (g == null) {
      cache[name] = null
      return null
    }
    var out = g
    if (g is List) {
      if (_asciiOnly) {
        out = g[1]
      } else if (Codepage.encodes_(g[0])) {
        out = g[0]
      } else {
        out = g[1]
      }
    }
    cache[name] = out
    return out
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
    // ".inactive" is a stable suffix that swaps in the dim variant
    // at every level of the dotted cascade.  Detect it and walk a
    // separate inactive chain instead of treating ".inactive" as
    // just another component.
    var bn = role.bytes.count
    if (bn >= 9 && role.indexOf(".inactive") == bn - 9) {
      return inactiveStyle_(role[0...bn - 9])
    }
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

  // Inactive-cascade lookup for a base role (without ".inactive").
  // Walks the active-side parent chain, trying `<X>.inactive` at
  // each step; finally falls through "default.inactive" → "default"
  // so even roles with no explicit inactive variant pick up the
  // theme's blanket dim look.
  inactiveStyle_(baseRole) {
    var s   = Style.empty
    var cur = baseRole
    while (cur != "") {
      s = s | lookup_(cur + ".inactive")
      if (s.complete) return s
      cur = parentRole_(cur)
    }
    s = s | lookup_("default.inactive")
    if (s.complete) return s
    return s | style("default")
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
      // UIFC-style inactive palette: cyan background instead of
      // blue.  Frames go bright-white (0x3F), the bg shifts to cyan,
      // so any inactive pane reads as a separate "scheme" rather
      // than a dimmer version of the active one — works on legacy
      // backends that don't honor the bright bit.
      "default.inactive":
        Style.new(null, 0x3F, 0xFFFFFF, 0x00AAAA),

      // Frame: focused = yellow tint on blue (active palette);
      // inactive = bright white on cyan (UIFC inactive scheme).
      "frame":
        Style.new(null, 0x1E, 0xFFFF55, null),
      "frame.inactive":
        Style.new(null, 0x3F, 0xFFFFFF, 0x00AAAA),

      // Title bars: same scheme as frames.
      "title":
        Style.new(null, 0x1E, 0xFFFF55, null),
      "title.inactive":
        Style.new(null, 0x3F, 0xFFFFFF, 0x00AAAA),

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
      // Inactive list lightbar: bright yellow on cyan, matching UIFC.
      // Non-selected inactive items cascade through "default.inactive"
      // (bright white on cyan).
      "list.item.focused.inactive":
        Style.new(null, 0x3E, 0xFFFF55, 0x00AAAA),

      // Inputs: inverse bar (legacyAttr 0x70 = gray bg, black fg) so
      // the field is visually distinct from the surrounding pane
      // interior.  RGB-capable backends get a brighter background on
      // focus; legacy backends get the same lightbar both ways and
      // rely on the cursor to show focus.  No bit-7 / blink fallback.
      "input":
        Style.new(null, 0x70, 0x000000, 0xAAAAAA),
      "input.focused":
        Style.new(null, 0x70, 0x000000, 0xFFFFFF),

      // Buttons: same lightbar family as menu/list focus state.
      // Unfocused buttons inherit "default" so the bracketed label
      // reads against the surrounding pane background; focused
      // buttons get the lightbar to draw the eye.  The hotkey
      // variant tints a single label letter so users can see what
      // would activate the action — yellow on the default bg,
      // bright red on the lightbar.
      "button":
        Style.new(null, 0x1F, null, null),
      "button.hotkey":
        Style.new(null, 0x1E, 0xFFFF55, null),
      "button.focused":
        Style.new(null, 0x70, 0x000000, 0xAAAAAA),
      "button.focused.hotkey":
        Style.new(null, 0x78, 0x555555, null),

      // Status bar: black on cyan.
      "statusbar":
        Style.new(null, 0x30, 0x000000, 0x00AAAA),

      // Form controls — checkbox, radio, spinbox.  Unfocused state
      // inherits "default" so the brackets read against the pane
      // background; focused gets the lightbar.  All three share the
      // same family so users get a consistent feel.
      "checkbox":
        Style.new(null, 0x1F, null, null),
      "checkbox.focused":
        Style.new(null, 0x70, 0x000000, 0xAAAAAA),
      "radio.item":
        Style.new(null, 0x1F, null, null),
      "radio.item.focused":
        Style.new(null, 0x70, 0x000000, 0xAAAAAA),
      "spinbox":
        Style.new(null, 0x70, 0x000000, 0xAAAAAA),
      "spinbox.focused":
        Style.new(null, 0x70, 0x000000, 0xFFFFFF),

      // Menu bar — base strip + per-item.  Black on gray for the
      // resting strip; bright on dark for the focused / hovered item.
      "menubar":
        Style.new(null, 0x70, 0x000000, 0xAAAAAA),
      "menubar.item":
        Style.new(null, 0x70, 0x000000, 0xAAAAAA),
      "menubar.item.focused":
        Style.new(null, 0x07, 0xAAAAAA, 0x000000),

      // Scroll bar: dim track, bright thumb (handled by glyphs +
      // these Styles).
      "scrollbar.track":
        Style.new(null, 0x17, 0xAAAAAA, 0x0000A8),
      "scrollbar.thumb":
        Style.new(null, 0x1F, 0xFFFFFF, 0x0000A8),

      // Progress bar.  Filled portion is light cyan on blue (matching
      // the existing transfer-window palette); empty portion + text
      // overlay cascade through "default" so they pick up whatever
      // pane background is in effect.
      "progress.fill":
        Style.new(null, 0x1B, 0x55FFFF, 0x0000A8),

      // Log view.  Severity → tint mirrors the colors today's C-side
      // `lputs()` paints into the transfer log window: white info,
      // yellow notice, light magenta warning, light red error.  All
      // on the standard pane blue background.  `logview.info` covers
      // anything at or above LOG_INFO (severity >= 6).
      "logview.info":
        Style.new(null, 0x1F, 0xFFFFFF, 0x0000A8),
      "logview.notice":
        Style.new(null, 0x1E, 0xFFFF55, 0x0000A8),
      "logview.warning":
        Style.new(null, 0x1D, 0xFF55FF, 0x0000A8),
      "logview.error":
        Style.new(null, 0x1C, 0xFF5555, 0x0000A8),

      // Help text: bright white on the default blue background,
      // matching UIFC's helpbuf body colour (api->lclr = WHITE).
      // Used as the cascade root for the markdown sub-roles below;
      // each inherits the help bg and overrides only the legacyAttr
      // / fg it needs.
      "help":
        Style.new(null, 0x1F, 0xFFFFFF, 0x0000A8),
      // Plain paragraph / bullet body text.  Same as "help" but
      // explicit so layout code can target the role symbolically.
      "help.text":
        Style.new(null, null, null, null),
      // Inline **bold** and `` `code` ``: both yellow on the help
      // bg.  UIFC has only one highlight role (backtick toggles
      // api->hclr = YELLOW), so we collapse the two markdown
      // emphases onto the same colour for UIFC fidelity — the body
      // text is already bright white, so a separate "code" shade
      // would just blur into prose.
      "help.bold":
        Style.new(null, 0x1E, 0xFFFF55, null),
      "help.code":
        Style.new(null, 0x1E, 0xFFFF55, null),
      // Headings: blue on cyan, matching UIFC's reverse-bar title
      // render (lclr/bclr swapped — bg=cyan, fg=blue).  The layout
      // pads the heading text with a single space cell on each end
      // so the bar reads as " Title " instead of stopping flush
      // against the surrounding text — see Markdown.layout.  We
      // don't have font sizing in the terminal, so the visual
      // hierarchy comes from the bar plus blank-line spacing
      // rather than per-level colour shifts.
      "help.heading.1":
        Style.new(null, 0x31, 0x0000A8, 0x00AAAA),
      "help.heading.2":
        Style.new(null, 0x31, 0x0000A8, 0x00AAAA),
      "help.heading.3":
        Style.new(null, 0x31, 0x0000A8, 0x00AAAA),
      // Bullet glyph: bright white so the • visually anchors the
      // list item.
      "help.bullet":
        Style.new(null, 0x1F, 0xFFFFFF, null),

      // Modal popups: yellow on red — distinct from the main window.
      "popup":
        Style.new(null, 0x4E, 0xFFFF55, 0xAA0000),
      "popup.frame":
        Style.new(null, 0x4F, 0xFFFFFF, 0xAA0000),
      "popup.frame.inactive":
        Style.new(null, 0x47, 0xAAAAAA, null)
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
      // Title flanks rendered as `┤Title├` on the top frame.  Double-
      // line themes can swap to `╡Title╞` by overriding these.
      "frame.title.left":    ["┤", "-"],
      "frame.title.right":   ["├", "-"],
      "frame.tee.top":       ["┬", "+"],
      "frame.tee.bottom":    ["┴", "+"],
      "frame.cross":         ["┼", "+"],
      "frame.separator":     ["─", "-"],

      // Double-line frame variant — Pane.framePreset = "double" picks
      // these up via the "frame.double" glyph prefix.  UIFC list /
      // menu boxes are double-line.  The title brackets stay
      // `╡Title╞` (double horizontal meeting single vertical of the
      // title gap).
      "frame.double.topLeft":     ["╔", "+"],
      "frame.double.top":         ["═", "="],
      "frame.double.topRight":    ["╗", "+"],
      "frame.double.left":        ["║", "|"],
      "frame.double.right":       ["║", "|"],
      "frame.double.bottomLeft":  ["╚", "+"],
      "frame.double.bottom":      ["═", "="],
      "frame.double.bottomRight": ["╝", "+"],
      "frame.double.tee.left":    ["╠", "+"],
      "frame.double.tee.right":   ["╣", "+"],
      "frame.double.title.left":  ["╡", "="],
      "frame.double.title.right": ["╞", "="],
      "frame.double.separator":   ["═", "="],

      "scrollbar.track":     ["░", ":"],
      "scrollbar.thumb":     ["█", "#"],
      "scrollbar.up":        ["▲", "^"],
      "scrollbar.down":      ["▼", "v"],

      "arrow.up":            ["↑", "^"],
      "arrow.down":          ["↓", "v"],
      "arrow.right":         ["►", ">"],
      "arrow.left":          ["◄", "<"],

      "focus.left":          ["►", ">"],
      "focus.right":         ["◄", "<"],

      "check.on":            ["√", "X"],
      "check.off":           [" ", " "],
      // U+2022 (•) maps to CP437 byte 0x07; U+25CF (●) does NOT, so
      // it would silently render as `?` despite Glyphs' own fallback
      // probe deciding the primary "encodes."  Always pick CP437-
      // representable primaries — that's the cell-storage codepage.
      "radio.on":            ["•", "*"],
      "radio.off":           ["○", "o"],
      "tag.on":              ["»", ">"],
      "tag.off":             [" ", " "]
    }

    return Theme.new(roles, glyphs)
  }
}
