// SyncTERM Wren UI library — markdown parser + renderer for help
// text.  Subset of CommonMark sized for help blurbs:
//
//   Block:   # / ## / ###  headings, "- " or "* " bullets, blank-line-
//            separated paragraphs.
//   Inline:  **bold**, `code`.  Italics intentionally omitted —
//            terminal italic support varies and dim grey on dark
//            blue is hard to make readable.
//
// Two-stage so the same parsed form can re-layout on resize:
//
//   var doc   = Markdown.parse(source)        // List<MdBlock>
//   var lines = Markdown.layout(doc, width)   // List<MdLine>
//
// Each MdLine carries pre-resolved inline style-role runs ready for
// Painter.text — Help just paints the visible window.
//
// Style roles are looked up via Widget.style(roleName); the theme
// supplies cascading fallbacks so an unknown role resolves through
// "help.text" → "help" → "default".

import "ui_widget" for Rect

// Style-role names — cascade through "help" via the theme.
class MdRole {
  static text     { "help.text" }
  static bold     { "help.bold" }
  static code     { "help.code" }
  static heading1 { "help.heading.1" }
  static heading2 { "help.heading.2" }
  static heading3 { "help.heading.3" }
  static bullet   { "help.bullet" }
}

// Block kinds — used by layout to decide bullet glyph + indent and
// to render `blank` as a vertical gap.
class MdBlockKind {
  static heading1  { 1 }
  static heading2  { 2 }
  static heading3  { 3 }
  static paragraph { 4 }
  static bullet    { 5 }
  static blank     { 6 }
  // Pandoc-style definition list: term line followed by one or more
  // `: description` lines.  The whole list is one block; layout
  // computes a common term-column width across all items so the
  // descriptions line up.
  static deflist   { 7 }
}

// One inline span — a contiguous slice of text in a single style
// role.  A line is a list of these in left-to-right order.
class MdRun {
  construct new(text, role) {
    _text = text
    _role = role
  }
  text { _text }
  role { _role }
}

// One item in a deflist: the term (a list of style runs, so inline
// markup like `**Bold**` works in terms) plus the body runs.  The
// layout pads the term to a common column width across the list.
class MdDefItem {
  construct new(term, runs) {
    _term = term
    _runs = runs
  }
  term { _term }
  runs { _runs }
}

class MdBlock {
  construct new(kind, runs) {
    _kind = kind
    _runs = runs
  }
  kind { _kind }
  runs { _runs }
}

// One rendered terminal row.  Painter.text(sf, x, y, run.text,
// style(run.role), runWidth) per run, advancing x by run.text.count.
class MdLine {
  construct new(runs) {
    _runs = runs
  }
  runs { _runs }
}

// ----- Wrap state ------------------------------------------------
//
// Pulled out of layout into its own class so layout doesn't have to
// thread mutable line / word / counter state through closures —
// closure upvalue reassignment in Wren needs the single-element-list
// dance (wren.md §3) and at this scale a small stateful helper is
// clearer than five `var x = [...]` shims.

class MdWrapState {
  construct new(width, indent) {
    _width  = width
    _indent = indent
    _line   = []
    _lineW  = 0
    _word   = []
    _wordW  = 0
    _first  = true
    _out    = []
  }

  out { _out }

  // Seed the current line with `prefix` runs (e.g. the bullet
  // glyph for a list item).  Counts toward _lineW.
  appendPrefix(prefix) {
    for (r in prefix) {
      _line.add(r)
      _lineW = _lineW + r.text.count
    }
  }

  // Append one character to the in-progress word in `role`.  Same-
  // role consecutive chars are coalesced into a single MdRun.
  addToWord(c, role) {
    if (_word.count > 0 && _word[-1].role == role) {
      _word[-1] = MdRun.new(_word[-1].text + c, role)
    } else {
      _word.add(MdRun.new(c, role))
    }
    _wordW = _wordW + 1
  }

  // Commit the current word to the line.  If the line would overflow
  // `_width`, emit it first and start a fresh wrap-continuation line
  // with the indent prefix.  No-op when no word is buffered.
  flushWord() {
    if (_wordW == 0) return
    if (_lineW + _wordW > _width && _lineW > 0) {
      _out.add(MdLine.new(_line))
      _line  = []
      _lineW = 0
      // Continuation gets the wrap indent on every wrap — the
      // term column for def-lists, the hang for bullets.  The
      // first line uses `appendPrefix` (e.g. the bullet glyph or
      // term + padding); subsequent lines use the indent.
      if (_indent.count > 0) {
        _line.add(MdRun.new(_indent, MdRole.text))
        _lineW = _indent.count
      }
      _first = false
    }
    for (r in _word) {
      _line.add(r)
      _lineW = _lineW + r.text.count
    }
    _word  = []
    _wordW = 0
  }

  // Append a single space in `role`.  Skipped when the line is at
  // column 0 (so wrapped continuation lines don't start with a
  // stray space) or already at width (overflow).
  addSpace(role) {
    if (_lineW == 0 || _lineW >= _width) return
    if (_line.count > 0 && _line[-1].role == role) {
      _line[-1] = MdRun.new(_line[-1].text + " ", role)
    } else {
      _line.add(MdRun.new(" ", role))
    }
    _lineW = _lineW + 1
  }

  // Hard line break: flush the in-progress word, emit the current
  // line as-is, and start a fresh continuation line with the same
  // wrap indent that a soft wrap would use.  Triggered by a
  // markdown trailing-double-space line break inside a paragraph
  // or bullet (the parser converts those to a literal "\n" in the
  // run text; layout dispatches to here when it sees one).
  forceBreak() {
    flushWord()
    _out.add(MdLine.new(_line))
    _line  = []
    _lineW = 0
    if (_indent.count > 0) {
      _line.add(MdRun.new(_indent, MdRole.text))
      _lineW = _indent.count
    }
    _first = false
  }

  // Finalise: flush any pending word, then emit the last line if
  // it has content.
  finish() {
    flushWord()
    if (_line.count > 0) {
      _out.add(MdLine.new(_line))
    }
  }
}

class Markdown {
  // ----- Parsing -------------------------------------------------

  // Parse `source` into a List<MdBlock>.  Inline markup inside
  // each block is also resolved here — layout doesn't re-parse.
  static parse(source) {
    var lines  = splitLines_(source)
    var blocks = []
    var i      = 0
    while (i < lines.count) {
      var ln = lines[i]
      if (ln == "") {
        blocks.add(MdBlock.new(MdBlockKind.blank, []))
        i = i + 1
      } else if (ln.startsWith("### ")) {
        blocks.add(MdBlock.new(MdBlockKind.heading3,
            parseInline_(ln[4..-1], MdRole.heading3)))
        i = i + 1
      } else if (ln.startsWith("## ")) {
        blocks.add(MdBlock.new(MdBlockKind.heading2,
            parseInline_(ln[3..-1], MdRole.heading2)))
        i = i + 1
      } else if (ln.startsWith("# ")) {
        blocks.add(MdBlock.new(MdBlockKind.heading1,
            parseInline_(ln[2..-1], MdRole.heading1)))
        i = i + 1
      } else if (ln.startsWith("- ") || ln.startsWith("* ")) {
        blocks.add(MdBlock.new(MdBlockKind.bullet,
            parseInline_(ln[2..-1], MdRole.text)))
        i = i + 1
      } else if (i + 1 < lines.count && isDefMarker_(lines[i + 1])) {
        // Pandoc-style definition list.  Greedy: consume successive
        // (term, ":  desc") pairs, allowing a single blank line
        // between items and treating consecutive ":" lines as
        // continuation of the same description.
        var items = []
        while (i < lines.count) {
          var termLn = lines[i]
          if (termLn == "" || isBlockMarker_(termLn)) break
          if (i + 1 >= lines.count) break
          if (!isDefMarker_(lines[i + 1])) break
          var defText = stripDefMarker_(lines[i + 1])
          i = i + 2
          while (i < lines.count && isDefMarker_(lines[i])) {
            defText = defText + " " + stripDefMarker_(lines[i])
            i = i + 1
          }
          items.add(MdDefItem.new(
              parseInline_(termLn, MdRole.bold),
              parseInline_(defText, MdRole.text)))
          // Optional blank between items — only consume if another
          // term/`: ` pair follows.
          if (i + 2 < lines.count && lines[i] == "" &&
              isDefMarker_(lines[i + 2])) {
            i = i + 1
          }
        }
        blocks.add(MdBlock.new(MdBlockKind.deflist, items))
      } else {
        // Paragraph: gather consecutive non-special lines, joined
        // with a single space (markdown's soft-break collapse).
        // A line ending with two trailing spaces forces a hard
        // line break — strip the spaces and join with "\n" so
        // layout can emit a forced break at that point.
        var paraText = ln
        var prev     = ln
        i = i + 1
        while (i < lines.count) {
          var nl = lines[i]
          if (nl == "" || isBlockMarker_(nl)) break
          if (i + 1 < lines.count && isDefMarker_(lines[i + 1])) break
          if (endsWithHardBreak_(prev)) {
            paraText = stripTrailingSpaces_(paraText) + "\n" + nl
          } else {
            paraText = paraText + " " + nl
          }
          prev = nl
          i = i + 1
        }
        blocks.add(MdBlock.new(MdBlockKind.paragraph,
            parseInline_(paraText, MdRole.text)))
      }
    }
    return blocks
  }

  // Parse inline markup into runs.  Bold (`**...**`) and code
  // (`` `...` ``) toggle the active role on / off; backslash
  // escapes a literal `*`, `` ` ``, or `\`.  Unmatched markers
  // pass through as literal characters by reverting the role at
  // end-of-input — this matches CommonMark's permissive lexer.
  //
  // We materialise the string into a list of codepoints up front
  // because the inline scanner needs lookahead (`**`, escape pairs)
  // and Wren's `s[i]` is byte-indexed — see `wren.md` §4.1.
  static parseInline_(text, defaultRole) {
    var cps = []
    for (c in text) cps.add(c)
    var runs = []
    var buf  = ""
    var role = defaultRole
    var i    = 0
    var n    = cps.count
    while (i < n) {
      var c = cps[i]
      if (c == "\\" && i + 1 < n) {
        var next = cps[i + 1]
        if (next == "*" || next == "`" || next == "\\") {
          buf = buf + next
          i = i + 2
          continue
        }
      }
      if (c == "*" && i + 1 < n && cps[i + 1] == "*") {
        if (buf != "") {
          runs.add(MdRun.new(buf, role))
          buf = ""
        }
        role = (role == MdRole.bold) ? defaultRole : MdRole.bold
        i = i + 2
        continue
      }
      if (c == "`") {
        if (buf != "") {
          runs.add(MdRun.new(buf, role))
          buf = ""
        }
        role = (role == MdRole.code) ? defaultRole : MdRole.code
        i = i + 1
        continue
      }
      buf = buf + c
      i = i + 1
    }
    if (buf != "") runs.add(MdRun.new(buf, role))
    return runs
  }

  // ----- Layout (word-wrap to width) -----------------------------

  // Lay out parsed blocks into rendered lines at `width` cells.
  // `width <= 0` returns the empty list (defensive — no bounds yet).
  static layout(blocks, width) {
    var lines = []
    if (width <= 0) return lines
    for (block in blocks) {
      if (block.kind == MdBlockKind.blank) {
        lines.add(MdLine.new([]))
        continue
      }
      if (block.kind == MdBlockKind.deflist) {
        layoutDefList_(block.runs, width, lines)
        continue
      }
      var firstPrefix = []
      var indent      = ""
      var headingRole = headingRoleFor_(block.kind)
      if (block.kind == MdBlockKind.bullet) {
        // Bullet glyph + a space; wrap continuation lines indent
        // by two cells so they hang under the text, not the bullet.
        firstPrefix = [MdRun.new("• ", MdRole.bullet)]
        indent      = "  "
      } else if (headingRole != null) {
        // Pad the heading text with a single styled space cell on each
        // end so the bar reads as " Title " instead of stopping flush
        // against the surrounding margin.  Mirrors UIFC's reverse-bar
        // title render.
        firstPrefix = [MdRun.new(" ", headingRole)]
      }
      var s = MdWrapState.new(width, indent)
      s.appendPrefix(firstPrefix)
      for (r in block.runs) {
        // Codepoint iteration via `for (c in r.text)`; raw `r.text[i]`
        // would be byte-indexed and truncate on multi-byte UTF-8.
        for (c in r.text) {
          if (c == "\n") {
            s.forceBreak()
          } else if (c == " ") {
            s.flushWord()
            s.addSpace(r.role)
          } else {
            s.addToWord(c, r.role)
          }
        }
      }
      if (headingRole != null) {
        s.flushWord()
        s.addSpace(headingRole)
      }
      s.finish()
      for (ln in s.out) lines.add(ln)
    }
    return lines
  }

  // Lay out one definition-list block.  All items share a term
  // column wide enough for the longest term (capped at half the
  // viewport so a giant term can't squeeze the description out
  // entirely).  Continuation lines indent into the description
  // column so wrapped descriptions stay aligned.
  static layoutDefList_(items, width, lines) {
    var termMax = 0
    for (item in items) {
      var n = runsWidth_(item.term)
      if (n > termMax) termMax = n
    }
    var cap     = (width / 2).floor.max(8)
    var termCol = termMax.min(cap)
    var gap     = 2
    var stop    = (termCol + gap).min(width.max(1))
    // Continuation indent: blank-pad to the description column.
    var indent  = ""
    var ii      = 0
    while (ii < stop) {
      indent = indent + " "
      ii = ii + 1
    }
    for (item in items) {
      // Build the per-row prefix: term runs followed by a padding
      // run to land the description at column `stop`.  When the term
      // overflows termCol we still leave a single-cell gap so the
      // two columns don't collide.
      var prefix = []
      for (r in item.term) prefix.add(r)
      var tw  = runsWidth_(item.term)
      var pad = (stop - tw).max(1)
      var padStr = ""
      ii = 0
      while (ii < pad) {
        padStr = padStr + " "
        ii = ii + 1
      }
      prefix.add(MdRun.new(padStr, MdRole.text))

      var s = MdWrapState.new(width, indent)
      s.appendPrefix(prefix)
      for (r in item.runs) {
        for (c in r.text) {
          if (c == "\n") {
            s.forceBreak()
          } else if (c == " ") {
            s.flushWord()
            s.addSpace(r.role)
          } else {
            s.addToWord(c, r.role)
          }
        }
      }
      s.finish()
      for (ln in s.out) lines.add(ln)
    }
  }

  // ----- Helpers -------------------------------------------------

  // Heading-role lookup keyed by block kind.  Returns the role
  // string for #/##/### headings, null otherwise — null is the
  // sentinel for "this block is not a heading" (callers test
  // `headingRole != null`).
  static headingRoleFor_(kind) {
    if (kind == MdBlockKind.heading1) return MdRole.heading1
    if (kind == MdBlockKind.heading2) return MdRole.heading2
    if (kind == MdBlockKind.heading3) return MdRole.heading3
    return null
  }

  // Does this line start a heading or bullet?  Used to terminate
  // paragraph and def-list scans.  Blank lines are checked
  // separately by the caller.
  static isBlockMarker_(line) {
    return line.startsWith("# ")  || line.startsWith("## ") ||
           line.startsWith("### ") ||
           line.startsWith("- ")  || line.startsWith("* ")
  }

  // Pandoc def-list marker: a colon followed by at least one space
  // or tab.  We don't accept `:` alone — that's just a colon at
  // start of a paragraph.
  static isDefMarker_(line) {
    if (!line.startsWith(":")) return false
    var n = line.bytes.count
    if (n < 2) return false
    var c = line[1]
    return c == " " || c == "\t"
  }

  // Strip the leading `:` plus run of horizontal whitespace.  All
  // ASCII so byte indexing is safe (see `wren.md` §4.1).
  static stripDefMarker_(line) {
    var i = 1
    var n = line.bytes.count
    while (i < n && (line[i] == " " || line[i] == "\t")) {
      i = i + 1
    }
    if (i >= n) return ""
    return line[i..-1]
  }

  // Sum of run text lengths in display cells.  Used for laying out
  // the term column in def lists.
  static runsWidth_(runs) {
    var n = 0
    for (r in runs) n = n + r.text.count
    return n
  }

  // CommonMark hard-break: a line ending in two or more trailing
  // spaces.  Trailing spaces are all ASCII so byte indexing is
  // safe (see `wren.md` §4.1).
  static endsWithHardBreak_(line) {
    var n = line.bytes.count
    if (n < 2) return false
    return line[n - 1] == " " && line[n - 2] == " "
  }

  // Strip any run of trailing space characters.  Companion to
  // `endsWithHardBreak_` — once we've detected a hard break we
  // also drop the trailing run that triggered it so layout
  // doesn't see the dangling spaces.
  static stripTrailingSpaces_(text) {
    var n = text.bytes.count
    while (n > 0 && text[n - 1] == " ") {
      n = n - 1
    }
    if (n <= 0) return ""
    return text[0...n]
  }

  // Split on LF.  CR is not stripped — callers shouldn't be passing
  // CRLF help text in the first place, and silent stripping would
  // mask source-side bugs.
  static splitLines_(source) {
    if (source == null || source == "") return [""]
    var out  = []
    var line = ""
    for (c in source) {
      if (c == "\n") {
        out.add(line)
        line = ""
      } else {
        line = line + c
      }
    }
    out.add(line)
    return out
  }
}
