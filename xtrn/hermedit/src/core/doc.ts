/**
 * Headless document model: reflowable prose lines plus a fixed CP437 art
 * overlay, exactly the split DESIGN.md calls for (v1 scope):
 *
 *   - `lines` hold prose. A paragraph is a run of soft lines ended by one
 *     hardcr line; edits reflow ONLY the touched paragraph (FSEditor's
 *     hardcr precedent, with per-char attributes).
 *   - `art` is a sparse overlay of fixed cells keyed by document row/col.
 *     Text editing never moves art; drawing never shifts text.
 *
 * Everything here is pure ES5 with no Synchronet globals, so vitest covers
 * it byte-exactly (see test/doc.test.ts, test/flatten.test.ts).
 *
 * Invariant for soft wraps: a soft-wrapped line keeps its trailing space, so
 * concatenating the lines of a paragraph is lossless. Wrapping breaks AFTER
 * the space.
 */

import { ctrlATransition, ansiFromAttr, applyColorChannel, DEFAULT_ATTR } from './attr';
import { clamp, objectKeys } from './std';

export interface ArtCell {
  ch: number;
  attr: number;
}

export interface Line {
  text: string;
  attr: number[];
  hardcr: boolean;
}

export interface Caret {
  row: number;
  col: number;
}

/**
 * A rectangular text region: the interior of a drawn art box that the prose
 * flow is confined to. `left`/`top` are the top-left interior cell; `width`
 * is the wrap width; `height` is the interior row count. When a region is
 * active, prose wraps at `width`, the caret cannot leave the interior, and
 * everything renders offset to (left, top) — so a box's border glyphs (fixed
 * art) act as the writing margins and never move.
 */
export interface Region {
  left: number;
  top: number;
  width: number;
  height: number;
  /** Preformatted (code block): no word-wrap, whitespace preserved. */
  pre?: boolean;
  /** Syntax-highlight language id (see core/syntax.ts); '' / undefined = none. */
  lang?: string;
}

/**
 * One independent text area. `region === null` is the full-width message
 * body; a non-null region is a box's interior. Every flow keeps its own
 * paragraph lines, so text in a box and text elsewhere coexist and persist —
 * clicking a box switches the ACTIVE flow rather than reflowing one shared
 * body into it.
 */
export interface TextFlow {
  region: Region | null;
  lines: Line[];
}

interface Snapshot {
  flows: TextFlow[];
  active: number;
  art: { [key: string]: ArtCell };
  caret: Caret;
}

var UNDO_LIMIT = 200;

function copyLine(l: Line): Line {
  return { text: l.text, attr: l.attr.slice(0), hardcr: l.hardcr };
}

function copyLines(lines: Line[]): Line[] {
  var out: Line[] = [];
  for (var i = 0; i < lines.length; i++) out.push(copyLine(lines[i] as Line));
  return out;
}

function copyFlows(flows: TextFlow[]): TextFlow[] {
  var out: TextFlow[] = [];
  for (var i = 0; i < flows.length; i++) {
    var f = flows[i] as TextFlow;
    out.push({ region: f.region === null ? null : { left: f.region.left, top: f.region.top, width: f.region.width, height: f.region.height, pre: f.region.pre, lang: f.region.lang }, lines: copyLines(f.lines) });
  }
  return out;
}

function regionsEqual(a: Region | null, b: Region | null): boolean {
  if (a === null || b === null) return a === b;
  return a.left === b.left && a.top === b.top && a.width === b.width && a.height === b.height;
}

function copyArt(art: { [key: string]: ArtCell }): { [key: string]: ArtCell } {
  var out: { [key: string]: ArtCell } = {};
  var keys = objectKeys(art);
  for (var i = 0; i < keys.length; i++) {
    var k = keys[i] as string;
    var c = art[k] as ArtCell;
    out[k] = { ch: c.ch, attr: c.attr };
  }
  return out;
}

function artKey(x: number, y: number): string {
  return y + ',' + x;
}

export class Document {
  width: number;
  art: { [key: string]: ArtCell } = {};
  caret: Caret = { row: 0, col: 0 };
  curAttr: number = DEFAULT_ATTR;
  insertMode: boolean = true;
  dirty: boolean = false;

  /** flows[0] is always the full-width message body; boxes are appended. */
  private flows: TextFlow[] = [{ region: null, lines: [{ text: '', attr: [], hardcr: true }] }];
  private active = 0;

  private undoStack: Snapshot[] = [];
  private redoStack: Snapshot[] = [];
  private lastOpTag: string = '';

  constructor(width: number) {
    this.width = width;
  }

  /** The active flow's paragraph lines (editing operates here). */
  get lines(): Line[] {
    return (this.flows[this.active] as TextFlow).lines;
  }
  set lines(v: Line[]) {
    (this.flows[this.active] as TextFlow).lines = v;
  }

  /** The active flow's region: a box interior, or null for the body. */
  get region(): Region | null {
    return (this.flows[this.active] as TextFlow).region;
  }

  /** Populate the body flow from CRLF/LF text (source message or draft). */
  loadText(text: string): void {
    this.active = 0;
    this.lines = [];
    var raw = text.replace(/\r\n/g, '\n').replace(/\r/g, '\n').split('\n');
    // A trailing newline produces one empty trailing element, not a line.
    if (raw.length > 1 && raw[raw.length - 1] === '') raw.pop();
    for (var i = 0; i < raw.length; i++) {
      var s = raw[i] as string;
      var attr: number[] = [];
      for (var j = 0; j < s.length; j++) attr.push(DEFAULT_ATTR);
      this.lines.push({ text: s, attr: attr, hardcr: true });
      if (s.length > this.ew()) this.rewrapParagraphAt(this.lines.length - 1);
    }
    if (this.lines.length === 0) this.lines.push({ text: '', attr: [], hardcr: true });
    this.caret = { row: 0, col: 0 };
    this.dirty = false;
    this.undoStack = [];
    this.redoStack = [];
  }

  // ------------------------------------------------------------------
  // Text region (box-constrained typing)
  // ------------------------------------------------------------------

  /** Effective wrap width: the active box's interior, else the canvas width.
   * Preformatted (code) regions never wrap — typing is width-capped instead. */
  private ew(): number {
    var r = this.region;
    if (r === null) return this.width;
    return r.pre ? 1000000000 : r.width;
  }

  /** Mark the active box flow as a code block (or change its language). */
  markRegionCode(lang: string): void {
    var r = this.region;
    if (r === null) return;
    this.pushUndo('');
    r.pre = true;
    r.lang = lang;
    this.dirty = true;
  }

  /** Caret column translated to an absolute canvas column. */
  caretDocX(): number {
    return (this.region ? this.region.left : 0) + this.caret.col;
  }

  /** Caret row translated to an absolute canvas row. */
  caretDocY(): number {
    return (this.region ? this.region.top : 0) + this.caret.row;
  }

  /**
   * Switch editing into the box `region`: activate that box's own text flow,
   * creating an empty one the first time. Other flows (the body, other boxes)
   * are untouched, so their text stays put. Re-entering the same box returns
   * to the text already there.
   */
  setRegion(region: Region): void {
    for (var i = 1; i < this.flows.length; i++) {
      if (regionsEqual((this.flows[i] as TextFlow).region, region)) {
        this.active = i;
        this.clampCaret();
        return;
      }
    }
    this.flows.push({ region: { left: region.left, top: region.top, width: region.width, height: region.height }, lines: [{ text: '', attr: [], hardcr: true }] });
    this.active = this.flows.length - 1;
    this.caret = { row: 0, col: 0 };
  }

  /** Switch editing back to the full-width message body flow. */
  clearRegion(): void {
    this.active = 0;
    this.clampCaret();
  }

  /** Snapshot of every flow's region + text (for callers that inspect layout). */
  flowList(): TextFlow[] {
    return this.flows;
  }

  private clampCaret(): void {
    if (this.caret.row >= this.lines.length) this.caret.row = this.lines.length - 1;
    if (this.caret.row < 0) this.caret.row = 0;
    var len = (this.lines[this.caret.row] as Line).text.length;
    if (this.caret.col > len) this.caret.col = len;
    if (this.caret.col < 0) this.caret.col = 0;
  }

  /**
   * Detect the interior of an art box surrounding canvas cell (x, y): the
   * nearest fixed art cell in each of the four directions must exist, and
   * enclose at least a 1x1 interior. Returns null when (x, y) is on/over an
   * art cell or not fully boxed in — so clicking loose art never traps the
   * caret.
   */
  detectBox(x: number, y: number): Region | null {
    if (this.artAt(x, y) !== null) return null;
    var left = -1;
    var right = -1;
    var top = -1;
    var bottom = -1;
    for (var lx = x - 1; lx >= 0; lx--) {
      if (this.artAt(lx, y) !== null) { left = lx; break; }
    }
    for (var rx = x + 1; rx < this.width; rx++) {
      if (this.artAt(rx, y) !== null) { right = rx; break; }
    }
    var maxY = this.maxArtRow();
    for (var ty = y - 1; ty >= 0; ty--) {
      if (this.artAt(x, ty) !== null) { top = ty; break; }
    }
    for (var by = y + 1; by <= maxY; by++) {
      if (this.artAt(x, by) !== null) { bottom = by; break; }
    }
    if (left < 0 || right < 0 || top < 0 || bottom < 0) return null;
    var iw = right - left - 1;
    var ih = bottom - top - 1;
    if (iw < 1 || ih < 1) return null;
    return { left: left + 1, top: top + 1, width: iw, height: ih };
  }

  // ------------------------------------------------------------------
  // Undo/redo
  // ------------------------------------------------------------------

  private snapshot(): Snapshot {
    return { flows: copyFlows(this.flows), active: this.active, art: copyArt(this.art), caret: { row: this.caret.row, col: this.caret.col } };
  }

  private restore(s: Snapshot): void {
    this.flows = copyFlows(s.flows);
    this.active = s.active < this.flows.length ? s.active : 0;
    this.art = copyArt(s.art);
    this.caret = { row: s.caret.row, col: s.caret.col };
  }

  /**
   * Record undo state before a mutation. Consecutive ops with the same
   * non-empty tag coalesce into one undo step (typing a word is one undo).
   */
  pushUndo(tag: string): void {
    if (tag !== '' && tag === this.lastOpTag) return;
    this.lastOpTag = tag;
    this.undoStack.push(this.snapshot());
    if (this.undoStack.length > UNDO_LIMIT) this.undoStack.shift();
    this.redoStack = [];
  }

  undo(): boolean {
    var s = this.undoStack.pop();
    if (!s) return false;
    this.redoStack.push(this.snapshot());
    this.restore(s);
    this.lastOpTag = '';
    this.dirty = true;
    return true;
  }

  redo(): boolean {
    var s = this.redoStack.pop();
    if (!s) return false;
    this.undoStack.push(this.snapshot());
    this.restore(s);
    this.lastOpTag = '';
    this.dirty = true;
    return true;
  }

  /** Call when the user action changes (arrow key etc.) to end coalescing. */
  breakUndoGroup(): void {
    this.lastOpTag = '';
  }

  // ------------------------------------------------------------------
  // Paragraph plumbing
  // ------------------------------------------------------------------

  private paragraphStart(row: number): number {
    var p = row;
    while (p > 0 && !(this.lines[p - 1] as Line).hardcr) p--;
    return p;
  }

  private paragraphEnd(row: number): number {
    var e = row;
    while (e < this.lines.length - 1 && !(this.lines[e] as Line).hardcr) e++;
    return e;
  }

  private flattenParagraph(start: number, end: number): { text: string; attr: number[] } {
    var text = '';
    var attr: number[] = [];
    for (var i = start; i <= end; i++) {
      var l = this.lines[i] as Line;
      text += l.text;
      for (var j = 0; j < l.attr.length; j++) attr.push(l.attr[j] as number);
    }
    return { text: text, attr: attr };
  }

  /** Greedy wrap: break after the last space at or before `width`. */
  private splitFlat(text: string, attr: number[], hardcr: boolean): Line[] {
    var out: Line[] = [];
    var pos = 0;
    var w = this.ew();
    while (text.length - pos > w) {
      var brk = -1;
      // last space within [pos, pos+width-1]; break AFTER it
      for (var i = pos + w - 1; i >= pos; i--) {
        if (text.charAt(i) === ' ') {
          brk = i + 1;
          break;
        }
      }
      if (brk <= pos) brk = pos + w; // kludge: word longer than width
      out.push({ text: text.substring(pos, brk), attr: attr.slice(pos, brk), hardcr: false });
      pos = brk;
    }
    out.push({ text: text.substring(pos), attr: attr.slice(pos), hardcr: hardcr });
    return out;
  }

  private caretToOffset(start: number): number {
    var off = 0;
    for (var i = start; i < this.caret.row; i++) off += (this.lines[i] as Line).text.length;
    return off + this.caret.col;
  }

  private offsetToCaret(start: number, end: number, off: number): Caret {
    for (var i = start; i <= end; i++) {
      var len = (this.lines[i] as Line).text.length;
      if (off < len || i === end) return { row: i, col: clamp(off, 0, len) };
      off -= len;
      // Landing exactly on a soft boundary means col 0 of the next line.
    }
    return { row: end, col: (this.lines[end] as Line).text.length };
  }

  private rewrapParagraphAt(row: number): void {
    var start = this.paragraphStart(row);
    var end = this.paragraphEnd(row);
    var caretInside = this.caret.row >= start && this.caret.row <= end;
    var off = caretInside ? this.caretToOffset(start) : 0;
    var flat = this.flattenParagraph(start, end);
    var hardcr = (this.lines[end] as Line).hardcr;
    var repl = this.splitFlat(flat.text, flat.attr, hardcr);
    var args: unknown[] = [start, end - start + 1];
    for (var i = 0; i < repl.length; i++) args.push(repl[i]);
    Array.prototype.splice.apply(this.lines, args as [number, number, ...Line[]]);
    if (caretInside) this.caret = this.offsetToCaret(start, start + repl.length - 1, off);
  }

  // ------------------------------------------------------------------
  // Text editing (all operate at the caret)
  // ------------------------------------------------------------------

  private curLine(): Line {
    return this.lines[this.caret.row] as Line;
  }

  /** Text of the caret's line (for callers that inspect it, e.g. fences). */
  curLineText(): string {
    return this.curLine().text;
  }

  insertChar(chCode: number): void {
    var r = this.region;
    var lGuard = this.curLine();
    // Preformatted region: refuse to grow a line past the box width — code
    // must never wrap, and silently clipping it at export would be worse.
    if (r !== null && r.pre === true && lGuard.text.length >= r.width &&
        (this.insertMode || this.caret.col >= lGuard.text.length)) {
      return;
    }
    this.pushUndo('type');
    var ch = String.fromCharCode(chCode & 0xff);
    var l = this.curLine();
    var col = this.caret.col;
    if (this.insertMode || col >= l.text.length) {
      l.text = l.text.substring(0, col) + ch + l.text.substring(col);
      l.attr.splice(col, 0, this.curAttr);
    } else {
      l.text = l.text.substring(0, col) + ch + l.text.substring(col + 1);
      l.attr[col] = this.curAttr;
    }
    this.caret.col++;
    if (l.text.length > this.ew()) this.rewrapParagraphAt(this.caret.row);
    this.dirty = true;
  }

  insertBreak(): void {
    this.pushUndo('');
    var l = this.curLine();
    var col = this.caret.col;
    var right: Line = {
      text: l.text.substring(col),
      attr: l.attr.slice(col),
      hardcr: l.hardcr
    };
    l.text = l.text.substring(0, col);
    l.attr = l.attr.slice(0, col);
    l.hardcr = true;
    this.lines.splice(this.caret.row + 1, 0, right);
    this.caret = { row: this.caret.row + 1, col: 0 };
    this.rewrapParagraphAt(this.caret.row);
    this.dirty = true;
  }

  backspace(): void {
    var l = this.curLine();
    if (this.caret.col > 0) {
      this.pushUndo('erase');
      var col = this.caret.col;
      l.text = l.text.substring(0, col - 1) + l.text.substring(col);
      l.attr.splice(col - 1, 1);
      this.caret.col--;
      this.rewrapParagraphAt(this.caret.row);
    } else if (this.caret.row > 0) {
      this.pushUndo('');
      var prev = this.lines[this.caret.row - 1] as Line;
      this.caret = { row: this.caret.row - 1, col: prev.text.length };
      if (prev.hardcr) {
        // Join this paragraph with the previous one.
        prev.hardcr = false;
        this.rewrapParagraphAt(this.caret.row);
      } else {
        // Soft boundary: erase the previous line's last char (usually the
        // wrap space).
        if (prev.text.length > 0) {
          prev.text = prev.text.substring(0, prev.text.length - 1);
          prev.attr.pop();
          this.caret.col--;
        }
        this.rewrapParagraphAt(this.caret.row);
      }
    }
    this.dirty = true;
  }

  deleteForward(): void {
    var l = this.curLine();
    var col = this.caret.col;
    if (col < l.text.length) {
      this.pushUndo('erase');
      l.text = l.text.substring(0, col) + l.text.substring(col + 1);
      l.attr.splice(col, 1);
      this.rewrapParagraphAt(this.caret.row);
    } else if (l.hardcr && this.caret.row < this.lines.length - 1) {
      this.pushUndo('');
      l.hardcr = false;
      this.rewrapParagraphAt(this.caret.row);
    } else if (!l.hardcr && this.caret.row < this.lines.length - 1) {
      this.pushUndo('erase');
      var next = this.lines[this.caret.row + 1] as Line;
      if (next.text.length > 0) {
        next.text = next.text.substring(1);
        next.attr.shift();
      }
      this.rewrapParagraphAt(this.caret.row);
    }
    this.dirty = true;
  }

  /**
   * Extract the text of a selection range [r0,c0)-(r1,c1) in the active flow.
   * Soft-wrapped lines join without a break (they are one paragraph); a hard
   * line contributes a '\n'.
   */
  getRangeText(r0: number, c0: number, r1: number, c1: number): string {
    if (r0 === r1) return (this.lines[r0] as Line).text.substring(c0, c1);
    var out = (this.lines[r0] as Line).text.substring(c0);
    for (var r = r0; r < r1; r++) {
      if ((this.lines[r] as Line).hardcr) out += '\n';
      if (r + 1 < r1) out += (this.lines[r + 1] as Line).text;
    }
    out += (this.lines[r1] as Line).text.substring(0, c1);
    return out;
  }

  /** Delete a selection range; the caret ends at (r0, c0). One undo step. */
  deleteRange(r0: number, c0: number, r1: number, c1: number): void {
    this.pushUndo('');
    this.caret = { row: r1, col: c1 };
    var guard = 0;
    while ((this.caret.row > r0 || this.caret.col > c0) && guard++ < 100000) this.backspace();
    this.dirty = true;
  }

  /** Insert a string at the caret, treating '\n' as a hard paragraph break. */
  insertString(s: string): void {
    this.pushUndo('');
    for (var i = 0; i < s.length; i++) {
      var ch = s.charAt(i);
      if (ch === '\n') this.insertBreak();
      else if (ch === '\r') { /* skip */ }
      else this.insertChar(ch.charCodeAt(0));
    }
    this.dirty = true;
  }

  /** Insert prepared lines (quotes) above the caret row as hard lines. */
  insertLines(lines: { text: string; attr: number[] }[]): void {
    if (lines.length === 0) return;
    this.pushUndo('');
    for (var i = 0; i < lines.length; i++) {
      var src = lines[i] as { text: string; attr: number[] };
      this.lines.splice(this.caret.row + i, 0, {
        text: src.text,
        attr: src.attr.slice(0),
        hardcr: true
      });
    }
    this.caret = { row: this.caret.row + lines.length, col: 0 };
    this.dirty = true;
  }

  // ------------------------------------------------------------------
  // Caret movement
  // ------------------------------------------------------------------

  moveLeft(): void {
    if (this.caret.col > 0) this.caret.col--;
    else if (this.caret.row > 0) {
      this.caret.row--;
      this.caret.col = this.curLine().text.length;
    }
  }

  moveRight(): void {
    if (this.caret.col < this.curLine().text.length) this.caret.col++;
    else if (this.caret.row < this.lines.length - 1) {
      this.caret.row++;
      this.caret.col = 0;
    }
  }

  moveVert(delta: number, desiredCol: number): void {
    var row = clamp(this.caret.row + delta, 0, this.lines.length - 1);
    this.caret.row = row;
    this.caret.col = clamp(desiredCol, 0, this.curLine().text.length);
  }

  moveHome(): void {
    this.caret.col = 0;
  }

  moveEnd(): void {
    this.caret.col = this.curLine().text.length;
  }

  moveWordLeft(): void {
    this.moveLeft();
    var l = this.curLine();
    while (this.caret.col > 0 && l.text.charAt(this.caret.col - 1) !== ' ') this.caret.col--;
    while (this.caret.col > 0 && l.text.charAt(this.caret.col - 1) === ' ') {
      if (this.caret.col === 1) break;
      this.caret.col--;
    }
  }

  moveWordRight(): void {
    var l = this.curLine();
    var len = l.text.length;
    var col = this.caret.col;
    while (col < len && l.text.charAt(col) !== ' ') col++;
    while (col < len && l.text.charAt(col) === ' ') col++;
    if (col === this.caret.col) this.moveRight();
    else this.caret.col = col;
  }

  // ------------------------------------------------------------------
  // Art overlay
  // ------------------------------------------------------------------

  setArt(x: number, y: number, cell: ArtCell): void {
    if (x < 0 || y < 0 || x >= this.width) return;
    this.pushUndo('draw');
    this.art[artKey(x, y)] = { ch: cell.ch, attr: cell.attr };
    this.dirty = true;
  }

  eraseArt(x: number, y: number): void {
    var k = artKey(x, y);
    if (this.art[k] === undefined) return;
    this.pushUndo('draw');
    delete this.art[k];
    this.dirty = true;
  }

  /**
   * Apply a batch of art cells (a committed shape or flood fill) as ONE undo
   * step. `ch < 0` erases the cell instead of setting it. Out-of-width cells
   * are skipped.
   */
  paintCells(cells: { x: number; y: number; ch: number; attr: number }[]): void {
    if (cells.length === 0) return;
    this.pushUndo(''); // '' never coalesces: each shape is its own undo step
    for (var i = 0; i < cells.length; i++) {
      var c = cells[i] as { x: number; y: number; ch: number; attr: number };
      if (c.x < 0 || c.y < 0 || c.x >= this.width) continue;
      if (c.ch < 0) delete this.art[artKey(c.x, c.y)];
      else this.art[artKey(c.x, c.y)] = { ch: c.ch & 0xff, attr: c.attr };
    }
    this.dirty = true;
  }

  /**
   * Recolor the VISIBLE cell at (x, y): the art overlay if it owns the cell,
   * else the owning text flow's character (box flows mask the body, same
   * ownership rules as cellAt). Applies `ink` to the chosen channel(s) while
   * keeping the glyph. Consecutive calls coalesce into one undo step (drag).
   * Returns true when something changed.
   */
  recolorCell(x: number, y: number, ink: number, channel: 'fg' | 'bg' | 'both'): boolean {
    var a = this.art[artKey(x, y)];
    if (a !== undefined) {
      var next = applyColorChannel(a.attr, ink, channel);
      if (next === a.attr) return false;
      this.pushUndo('recolor');
      (this.art[artKey(x, y)] as ArtCell).attr = next;
      this.dirty = true;
      return true;
    }
    for (var i = this.flows.length - 1; i >= 0; i--) {
      var f = this.flows[i] as TextFlow;
      var r = f.region;
      if (i > 0) {
        var rr = r as Region;
        if (x < rr.left || x >= rr.left + rr.width || y < rr.top || y >= rr.top + rr.height) continue;
      }
      var px = r ? x - r.left : x;
      var py = r ? y - r.top : y;
      var miss = px < 0 || py < 0 || py >= f.lines.length ||
        px >= (f.lines[py] === undefined ? 0 : (f.lines[py] as Line).text.length);
      if (miss) {
        if (i > 0) return false; // the box owns (and masks) this cell
        continue;
      }
      var l = f.lines[py] as Line;
      var cur = l.attr[px] as number;
      var nx = applyColorChannel(cur, ink, channel);
      if (nx === cur) return false;
      this.pushUndo('recolor');
      l.attr[px] = nx;
      this.dirty = true;
      return true;
    }
    return false;
  }

  artAt(x: number, y: number): ArtCell | null {
    var c = this.art[artKey(x, y)];
    return c === undefined ? null : c;
  }

  hasArt(): boolean {
    return objectKeys(this.art).length > 0;
  }

  maxArtRow(): number {
    var max = -1;
    var keys = objectKeys(this.art);
    for (var i = 0; i < keys.length; i++) {
      var y = parseInt((keys[i] as string).split(',')[0] as string, 10);
      if (y > max) max = y;
    }
    return max;
  }

  /** Total document rows across every flow plus any art rows. */
  rowCount(): number {
    var rows = 0;
    for (var i = 0; i < this.flows.length; i++) {
      var f = this.flows[i] as TextFlow;
      var bottom = f.region
        ? f.region.top + Math.min(f.lines.length, f.region.height)
        : f.lines.length;
      if (bottom > rows) rows = bottom;
    }
    var artMax = this.maxArtRow();
    if (artMax + 1 > rows) rows = artMax + 1;
    return rows;
  }

  private flowCell(f: TextFlow, x: number, y: number): { ch: number; attr: number } | null {
    var r = f.region;
    var px = r ? x - r.left : x;
    var py = r ? y - r.top : y;
    if (px < 0 || py < 0) return null;
    if (r && (px >= r.width || py >= r.height)) return null;
    if (py >= f.lines.length) return null;
    var l = f.lines[py] as Line;
    if (px >= l.text.length) return null;
    return { ch: l.text.charCodeAt(px) & 0xff, attr: l.attr[px] as number };
  }

  /**
   * The visible content of one canvas cell. Art wins over prose. A box flow
   * OWNS (and masks) its interior rectangle — the body never bleeds through a
   * box — so cells are resolved box-flows-first (newest on top), then the body.
   */
  cellAt(x: number, y: number): { ch: number; attr: number; isArt: boolean } {
    var a = this.art[artKey(x, y)];
    if (a !== undefined) return { ch: a.ch, attr: a.attr, isArt: true };
    // Box flows, newest first: if (x,y) is inside a box interior, that box
    // owns the cell (its glyph, or a blank mask), and we stop.
    for (var i = this.flows.length - 1; i >= 1; i--) {
      var f = this.flows[i] as TextFlow;
      var r = f.region as Region;
      if (x >= r.left && x < r.left + r.width && y >= r.top && y < r.top + r.height) {
        var bc = this.flowCell(f, x, y);
        if (bc !== null) return { ch: bc.ch, attr: bc.attr, isArt: false };
        return { ch: 0x20, attr: DEFAULT_ATTR, isArt: false };
      }
    }
    var mc = this.flowCell(this.flows[0] as TextFlow, x, y);
    if (mc !== null) return { ch: mc.ch, attr: mc.attr, isArt: false };
    return { ch: 0x20, attr: DEFAULT_ATTR, isArt: false };
  }

  // ------------------------------------------------------------------
  // Message export
  // ------------------------------------------------------------------

  /**
   * Flatten prose + art into a Synchronet message body (CP437 binary string;
   * transcode to UTF-8 at the host boundary when the session requires it).
   *
   * - hard lines end with CRLF; soft-wrapped prose lines are emitted without
   *   CRLF (they keep their wrap space, so readers reflow them);
   * - if the document contains ANY art, every row is emitted as a hard line:
   *   fixed art depends on fixed rows, so host-side reflow must not happen;
   * - Ctrl-A attribute runs are emitted minimally (FSEditor-compatible) when
   *   embedColors is on;
   * - trailing blanks are trimmed per hard row, but never past an art cell.
   */
  /** Non-space prose characters across all flows (message "text-ness" gauge). */
  proseCharCount(): number {
    var n = 0;
    for (var f = 0; f < this.flows.length; f++) {
      var lines = (this.flows[f] as TextFlow).lines;
      for (var i = 0; i < lines.length; i++) {
        var t = (lines[i] as Line).text;
        for (var j = 0; j < t.length; j++) if (t.charAt(j) !== ' ') n++;
      }
    }
    return n;
  }

  /** Number of fixed art cells (message "art-ness" gauge). */
  artCellCount(): number {
    return objectKeys(this.art).length;
  }

  /** ANSI-SGR flatten of the whole grid, with a leading/trailing reset — for
   * [ANSI]-tagged art posts. Colors as ANSI escape codes rather than Ctrl-A. */
  toAnsiBody(): string {
    return this.compositeBody('ansi');
  }

  toMessageBody(embedColors: boolean): string {
    // Any box present makes the document positional (prose sits inside fixed
    // borders at offsets), so it flattens through the all-rows-hard composite
    // sourced from cellAt. The plain body-only path keeps soft-wrap semantics.
    if (this.flows.length > 1) return this.compositeBody(embedColors ? 'ctrla' : 'none');

    var out = '';
    var lastattr = DEFAULT_ATTR;
    var anyArt = this.hasArt();
    var rows = this.rowCount();
    for (var y = 0; y < rows; y++) {
      var line: Line | null = y < this.lines.length ? (this.lines[y] as Line) : null;
      var hard = anyArt || line === null || line.hardcr || y === rows - 1;
      // Row width: text length extended to the rightmost art cell in the row.
      var textLen = line === null ? 0 : line.text.length;
      var rowLen = textLen;
      var lastArtX = -1;
      for (var x = 0; x < this.width; x++) {
        if (this.art[artKey(x, y)] !== undefined && x > lastArtX) lastArtX = x;
      }
      if (lastArtX + 1 > rowLen) rowLen = lastArtX + 1;
      // Trim trailing blank prose on hard rows (but keep art columns).
      if (hard && line !== null) {
        var lastInk = -1;
        for (var i = 0; i < textLen; i++) {
          var c = line.text.charAt(i);
          if (c !== ' ' && c !== '\t') lastInk = i;
        }
        var keep = (lastInk + 1) > (lastArtX + 1) ? lastInk + 1 : lastArtX + 1;
        if (keep < rowLen) rowLen = keep;
        if (textLen > keep) textLen = keep;
      }
      for (var x2 = 0; x2 < rowLen; x2++) {
        var cell = this.cellAt(x2, y);
        if (embedColors) {
          out += ctrlATransition(lastattr, cell.attr);
          lastattr = cell.attr;
        }
        out += String.fromCharCode(cell.ch);
      }
      if (hard) out += '\r\n';
    }
    return out;
  }

  /**
   * Positional flatten used when a text region is active: every row is a hard
   * CRLF line, cells come from cellAt (art over prose at its box offset), and
   * each row is trimmed to its rightmost inked cell.
   */
  private compositeBody(mode: 'none' | 'ctrla' | 'ansi'): string {
    var out = mode === 'ansi' ? '\x1b[0m' : '';
    var lastattr = DEFAULT_ATTR;
    var rows = this.rowCount();
    for (var y = 0; y < rows; y++) {
      var rowLen = 0;
      for (var x = 0; x < this.width; x++) {
        var probe = this.cellAt(x, y);
        if (probe.isArt || probe.ch !== 0x20) rowLen = x + 1;
      }
      for (var x2 = 0; x2 < rowLen; x2++) {
        var cell = this.cellAt(x2, y);
        if (mode === 'ctrla') {
          out += ctrlATransition(lastattr, cell.attr);
          lastattr = cell.attr;
        } else if (mode === 'ansi' && cell.attr !== lastattr) {
          out += ansiFromAttr(cell.attr);
          lastattr = cell.attr;
        }
        out += String.fromCharCode(cell.ch);
      }
      out += '\r\n';
    }
    if (mode === 'ansi') out += '\x1b[0m';
    return out;
  }
}
