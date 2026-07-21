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

import "syncterm" for Codepage, Host

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
  // white-on-blue, lightbar selection, yellow titles, and distinct
  // control/display frame families.  Both legacy attrs and 24-bit RGB are
  // populated so the renderer can pick whichever the terminal
  // supports.  Font slot 0 (cp437English) is the safe baseline.
  //
  // Memoized: every call returns the same Theme instance so that
  // `someTheme == Theme.default` works as identity equality.
  static default {
    if (__default == null) __default = fromData_(Host.defaultThemeData)
    return __default
  }

  static current {
    var generation = Host.themeGeneration
    if (__current == null || __currentGeneration != generation) {
      __current = fromData_(Host.themeData)
      __currentGeneration = generation
    }
    return __current
  }

  static fromData(data) { fromData_(data) }

  static fromData_(data) {
    var roles = {}
    for (row in data[0]) {
      roles[row[0]] = Style.new(row[1], row[2], row[3], row[4])
    }
    var glyphs = {}
    for (row in data[1]) glyphs[row[0]] = [row[1], row[2]]
    return Theme.new(roles, glyphs)
  }

}
