/**
 * TheDraw font (.tdf) parser and text renderer, ported from the working
 * webv4_custom browser implementation (root/js/tdf-browser.js) to pure ES5
 * TypeScript. No Synchronet globals: input is a binary string (one char per
 * byte, as File.read('rb') yields), output is a cell grid the editor stamps
 * onto the art layer.
 *
 * Differences from the browser port: cells store the CP437 BYTE (0-255), not a
 * Unicode char, because the editor's art cells are CP437. Only the first font
 * in a multi-font .tdf is parsed (matches the height map, which keys by
 * filename).
 */

/** Font type codes in the .tdf header. */
export var OUTLINE_FONT = 0;
export var BLOCK_FONT = 1;
export var COLOR_FONT = 2;

var NUM_CHARS = 94;
var CHARLIST = '!"#$%&\'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~';
var LIGHTGRAY = 7;
/** "\x13TheDraw FONTS file\x1a" */
var MAGIC = [0x13, 0x54, 0x68, 0x65, 0x44, 0x72, 0x61, 0x77, 0x20, 0x46, 0x4f, 0x4e, 0x54, 0x53, 0x20, 0x66, 0x69, 0x6c, 0x65, 0x1a];

/** Outline-font glyph-byte -> CP437 box-drawing substitution (TDF spec). */
var OUTLINE_SUB: { [code: number]: number } = {
  65: 205, 66: 196, 67: 179, 68: 186, 69: 213, 70: 187, 71: 214, 72: 191,
  73: 200, 74: 190, 75: 192, 76: 189, 77: 181, 78: 199, 79: 32, 64: 32, 38: 38
};

export interface TdfCell {
  /** CP437 byte 0-255. */
  ch: number;
  /** CGA attribute byte (color fonts); LIGHTGRAY for block/outline. */
  color: number;
  /**
   * True when this cell came from a COLOR_FONT glyph, so `color` is
   * authoritative. Only set by the styled (mixed-font) renderer, where the
   * producing font can differ per cell; single-font consumers keep using the
   * font's own type.
   */
  cf?: boolean;
}

export interface TdfGlyph {
  width: number;
  height: number;
  cell: TdfCell[];
}

export interface TdfFont {
  name: string;
  fonttype: number;
  spacing: number;
  height: number;
  charlist: number[];
  glyphs: (TdfGlyph | null)[];
}

export interface TdfCharBound {
  /** Rendered x of the character's first column. */
  start: number;
  /** Rendered column width of the character (glyph width, excl. spacing). */
  width: number;
}

export interface TdfRender {
  width: number;
  height: number;
  /** rows[y][x] cell. Empty areas are { ch: 0x20, color: 0 }. */
  rows: TdfCell[][];
  /** One entry per input character, in order (for caret positioning). */
  charBounds: TdfCharBound[];
}

function byte(s: string, i: number): number {
  return s.charCodeAt(i) & 0xff;
}

/** Parse the first font in a .tdf binary string. Returns null on bad data. */
export function parseTdf(data: string): TdfFont | null {
  if (data.length < 233) return null;
  for (var m = 0; m < MAGIC.length; m++) {
    if (byte(data, m) !== MAGIC[m]) return null;
  }
  var namelen = byte(data, 24);
  var name = '';
  for (var n = 0; n < namelen && n < 16; n++) name += String.fromCharCode(byte(data, 25 + n));

  var font: TdfFont = {
    name: name,
    fonttype: byte(data, 41),
    spacing: byte(data, 42),
    height: 0,
    charlist: [],
    glyphs: []
  };

  for (var c = 0; c < NUM_CHARS; c++) {
    var o = 45 + c * 2;
    font.charlist.push(byte(data, o) | (byte(data, o + 1) << 8));
  }

  var glyphData = data.substring(233);

  // Overall font height = tallest glyph.
  for (var h = 0; h < NUM_CHARS; h++) {
    var off = font.charlist[h] as number;
    if (off !== 0xffff && off + 1 < glyphData.length) {
      var gh = byte(glyphData, off + 1);
      if (gh > font.height) font.height = gh;
    }
  }

  for (var g = 0; g < NUM_CHARS; g++) {
    font.glyphs.push((font.charlist[g] as number) !== 0xffff ? parseGlyph(g, font, glyphData) : null);
  }
  return font;
}

function parseGlyph(idx: number, font: TdfFont, glyphData: string): TdfGlyph | null {
  var off = font.charlist[idx] as number;
  if (off + 1 >= glyphData.length) return null;
  var width = byte(glyphData, off);
  var glyph: TdfGlyph = { width: width, height: byte(glyphData, off + 1), cell: [] };
  var size = width * font.height;
  for (var s = 0; s < size; s++) glyph.cell.push({ ch: 0x20, color: 0 });

  var p = off + 2;
  var row = 0;
  var col = 0;
  while (p < glyphData.length && byte(glyphData, p) !== 0x00) {
    var ch = byte(glyphData, p++);
    if (ch === 0x0d) { row++; col = 0; continue; }
    if (p >= glyphData.length) break;
    var color = font.fonttype === COLOR_FONT ? byte(glyphData, p++) : LIGHTGRAY;
    if (ch < 0x20) ch = 0x20;
    var ci = row * width + col;
    if (ci < size) {
      var sub = OUTLINE_SUB[ch];
      var fc = font.fonttype === OUTLINE_FONT && sub !== undefined ? sub : ch;
      (glyph.cell[ci] as TdfCell).ch = fc & 0xff;
      (glyph.cell[ci] as TdfCell).color = color;
      col++;
    }
  }
  return glyph;
}

/** Glyph index for a character (with uppercase fallback), or -1. */
function lookup(ch: string, font: TdfFont): number {
  var code = ch.charCodeAt(0);
  for (var i = 0; i < NUM_CHARS; i++) {
    if (CHARLIST.charCodeAt(i) === code) return (font.charlist[i] as number) !== 0xffff ? i : -1;
  }
  var up = ch.toUpperCase().charCodeAt(0);
  if (up !== code) {
    for (var j = 0; j < NUM_CHARS; j++) {
      if (CHARLIST.charCodeAt(j) === up) return (font.charlist[j] as number) !== 0xffff ? j : -1;
    }
  }
  return -1;
}

/** Render `text` in `font` to a cell grid. Unknown chars become spaces. */
export function renderTdf(font: TdfFont, text: string): TdfRender {
  var h = font.height;
  var rows: TdfCell[][] = [];
  for (var r = 0; r < h; r++) {
    var line: TdfCell[] = [];
    for (var ci = 0; ci < text.length; ci++) {
      var gi = lookup(text.charAt(ci), font);
      if (gi === -1) {
        line.push({ ch: 0x20, color: 0 });
        if (ci < text.length - 1) for (var sp = 0; sp < font.spacing; sp++) line.push({ ch: 0x20, color: 0 });
        continue;
      }
      var glyph = font.glyphs[gi] as TdfGlyph;
      for (var col = 0; col < glyph.width; col++) {
        var cell = glyph.cell[r * glyph.width + col];
        if (cell) line.push({ ch: cell.ch, color: cell.color });
        else line.push({ ch: 0x20, color: 0 });
      }
      if (ci < text.length - 1) for (var s2 = 0; s2 < font.spacing; s2++) line.push({ ch: 0x20, color: 0 });
    }
    rows.push(line);
  }

  // Per-char bounds (single pass over the row-0 layout logic).
  var bounds: TdfCharBound[] = [];
  var cx = 0;
  for (var b = 0; b < text.length; b++) {
    var bgi = lookup(text.charAt(b), font);
    var gw = bgi === -1 ? 1 : (font.glyphs[bgi] as TdfGlyph).width;
    bounds.push({ start: cx, width: gw });
    cx += gw;
    if (b < text.length - 1) cx += font.spacing;
  }

  return { width: rows.length > 0 ? (rows[0] as TdfCell[]).length : 0, height: h, rows: rows, charBounds: bounds };
}

// ---------------------------------------------------------------------------
// Styled (per-character font) rendering, for the mixed-font word processor
// ---------------------------------------------------------------------------

/** Rendered column width of one character in a font (missing glyph -> 1). */
function glyphWidthIn(font: TdfFont, ch: string): number {
  var gi = lookup(ch, font);
  return gi === -1 ? 1 : (font.glyphs[gi] as TdfGlyph).width;
}

/** Rendered width of logical span [from, to) where fonts[i] styles text[i]. */
function measureStyledSpan(text: string, fonts: TdfFont[], from: number, to: number): number {
  var w = 0;
  for (var i = from; i < to; i++) {
    var f = fonts[i] as TdfFont;
    w += glyphWidthIn(f, text.charAt(i));
    if (i < to - 1) w += f.spacing;
  }
  return w;
}

/**
 * Render logical span [from, to) with a font per character. Glyphs are
 * bottom-aligned in a line box as tall as the span's tallest font, so mixed
 * sizes share a common baseline. Inter-character spacing follows the font of
 * the character on the left. Cells from color fonts carry `cf: true`.
 */
function renderStyledSpan(text: string, fonts: TdfFont[], from: number, to: number, minHeight: number): TdfRender {
  var h = minHeight;
  for (var i = from; i < to; i++) {
    var fh = (fonts[i] as TdfFont).height;
    if (fh > h) h = fh;
  }
  var rows: TdfCell[][] = [];
  for (var r = 0; r < h; r++) rows.push([]);
  var bounds: TdfCharBound[] = [];
  var cx = 0;

  for (var ci = from; ci < to; ci++) {
    var font = fonts[ci] as TdfFont;
    var isColor = font.fonttype === COLOR_FONT;
    var yOff = h - font.height;
    var gi = lookup(text.charAt(ci), font);
    var gw = gi === -1 ? 1 : (font.glyphs[gi] as TdfGlyph).width;
    var spacing = ci < to - 1 ? font.spacing : 0;
    for (var y = 0; y < h; y++) {
      var row = rows[y] as TdfCell[];
      for (var x = 0; x < gw + spacing; x++) {
        var cell: TdfCell = { ch: 0x20, color: 0 };
        if (gi !== -1 && x < gw && y >= yOff) {
          var glyph = font.glyphs[gi] as TdfGlyph;
          var src = glyph.cell[(y - yOff) * gw + x];
          if (src) {
            cell.ch = src.ch;
            cell.color = src.color;
            if (isColor) cell.cf = true;
          }
        }
        row.push(cell);
      }
    }
    bounds.push({ start: cx, width: gw });
    cx += gw + spacing;
  }
  return { width: cx, height: h, rows: rows, charBounds: bounds };
}

// ---------------------------------------------------------------------------
// Word-processor layout: wrap logical text to a width and map caret <-> screen
// ---------------------------------------------------------------------------

export interface TdfWpLine {
  /** Logical index (into the source text) of this display line's first char. */
  startIdx: number;
  /** The characters shown on this line (a wrap-consumed space is dropped). */
  text: string;
  render: TdfRender;
  /** Top row of this line, relative to the block origin. */
  yTop: number;
}

export interface TdfWpLayout {
  lines: TdfWpLine[];
  /** Widest rendered line. */
  width: number;
  /** Total rows including inter-line gap. */
  height: number;
  /** First line's row height + gap. Lines can differ when fonts are mixed;
   * use each line's render.height for exact per-line math. */
  lineHeight: number;
}

/**
 * Lay out `text` as a word-wrapped TheDraw block no wider than `maxWidth`
 * rendered columns, breaking on spaces and on '\n' (hard paragraph breaks).
 * `lineGap` blank rows separate display lines. Words wider than maxWidth are
 * placed anyway (never dropped). Single-font wrapper over the styled layout.
 */
export function layoutTdfWp(font: TdfFont, text: string, maxWidth: number, lineGap: number): TdfWpLayout {
  var fonts: TdfFont[] = [];
  for (var i = 0; i < text.length; i++) fonts.push(font);
  return layoutTdfWpStyled(text, fonts, maxWidth, lineGap, font);
}

/**
 * Mixed-font word-processor layout: fonts[i] styles text[i], so a font
 * switch mid-string reflows as one cohesive block. Lines are as tall as
 * their tallest font (glyphs bottom-aligned); `defaultFont` sizes empty
 * text/lines with no better hint.
 */
export function layoutTdfWpStyled(text: string, fonts: TdfFont[], maxWidth: number, lineGap: number, defaultFont: TdfFont): TdfWpLayout {
  var lines: TdfWpLine[] = [];
  var width = 0;

  var paraStart = 0;
  // Walk paragraphs (split on '\n'), tracking logical indices.
  while (paraStart <= text.length) {
    var nl = text.indexOf('\n', paraStart);
    var paraEnd = nl === -1 ? text.length : nl;
    layoutStyledParagraph(text, fonts, paraStart, paraEnd, maxWidth, defaultFont, lines);
    if (nl === -1) break;
    paraStart = nl + 1;
    // An empty trailing paragraph (text ends with '\n') still gets a blank line.
    if (paraStart > text.length) break;
  }
  if (lines.length === 0) {
    lines.push({ startIdx: 0, text: '', render: renderStyledSpan(text, fonts, 0, 0, defaultFont.height), yTop: 0 });
  }

  var yCur = 0;
  for (var y = 0; y < lines.length; y++) {
    var ln = lines[y] as TdfWpLine;
    ln.yTop = yCur;
    yCur += ln.render.height + lineGap;
    if (ln.render.width > width) width = ln.render.width;
  }
  var height = yCur - lineGap > 0 ? yCur - lineGap : (lines[0] as TdfWpLine).render.height;
  return { lines: lines, width: width, height: height, lineHeight: (lines[0] as TdfWpLine).render.height + lineGap };
}

function layoutStyledParagraph(text: string, fonts: TdfFont[], start: number, end: number, maxWidth: number, defaultFont: TdfFont, out: TdfWpLine[]): void {
  var mkLine = function (from: number, to: number): TdfWpLine {
    // An empty line is as tall as the font in effect where it sits (the
    // char at/before it), so blank paragraphs occupy a sensible height.
    var hintFont = fonts[from] !== undefined ? fonts[from] as TdfFont
      : (fonts[from - 1] !== undefined ? fonts[from - 1] as TdfFont : defaultFont);
    return { startIdx: from, text: text.substring(from, to), render: renderStyledSpan(text, fonts, from, to, hintFont.height), yTop: 0 };
  };
  if (start === end) {
    out.push(mkLine(start, end));
    return;
  }
  // Word-wrap on spaces, all indices logical/absolute. The current line is
  // the span [lineStart, lineEnd); a candidate extends it to the next word's
  // end and wraps (dropping the delimiter space) when it measures too wide.
  var lineStart = start;
  var lineEnd = start;
  var pos = start;
  while (pos < end) {
    var sp = text.indexOf(' ', pos);
    if (sp >= end) sp = -1;
    var wordEnd = sp === -1 ? end : sp;
    if (lineEnd > lineStart && measureStyledSpan(text, fonts, lineStart, wordEnd) > maxWidth) {
      out.push(mkLine(lineStart, lineEnd));
      lineStart = pos;
    }
    lineEnd = wordEnd;
    pos = sp === -1 ? end : sp + 1;
  }
  out.push(mkLine(lineStart, lineEnd));
}

/**
 * Logical caret index for a click at (localX, localY) relative to the block
 * origin: pick the display line by row, then the nearest character boundary.
 */
export function tdfWpHitTest(layout: TdfWpLayout, localX: number, localY: number): number {
  if (layout.lines.length === 0) return 0;
  // Lines can differ in height (mixed fonts): pick the last line whose top
  // is at or above the click (clicks in an inter-line gap go to the line
  // above; clicks past the block go to the last line).
  var li = 0;
  for (var l = 0; l < layout.lines.length; l++) {
    if ((layout.lines[l] as TdfWpLine).yTop <= localY) li = l;
  }
  var ln = layout.lines[li] as TdfWpLine;
  var bounds = ln.render.charBounds;
  for (var i = 0; i < bounds.length; i++) {
    var b = bounds[i] as TdfCharBound;
    if (localX < b.start + b.width / 2) return ln.startIdx + i;
  }
  return ln.startIdx + ln.text.length;
}

/**
 * Screen position (relative to the block origin) of the caret at logical index
 * `caretIdx`, plus which display line it is on.
 */
export function tdfWpCaretXY(layout: TdfWpLayout, caretIdx: number): { x: number; y: number; line: number } {
  for (var i = layout.lines.length - 1; i >= 0; i--) {
    var ln = layout.lines[i] as TdfWpLine;
    if (caretIdx >= ln.startIdx) {
      var within = caretIdx - ln.startIdx;
      if (within > ln.text.length) within = ln.text.length;
      var x = within >= ln.render.charBounds.length
        ? ln.render.width
        : (ln.render.charBounds[within] as TdfCharBound).start;
      return { x: x, y: ln.yTop, line: i };
    }
  }
  return { x: 0, y: 0, line: 0 };
}


/** Human-readable type name. */
export function tdfTypeName(fonttype: number): string {
  if (fonttype === OUTLINE_FONT) return 'Outline';
  if (fonttype === BLOCK_FONT) return 'Block';
  if (fonttype === COLOR_FONT) return 'Color';
  return 'Unknown';
}
