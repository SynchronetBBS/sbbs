/**
 * The editor controller: canvas-first adaptive layout, GUI-style chrome
 * (buttons always show their key equivalents), Text/Draw tools with equal
 * keyboard and mouse support, and the save/abort lifecycle.
 *
 * Layout (0-based rows), recomputed every frame from the live terminal size
 * (resizes are polled and adapt the view):
 *   0            title bar
 *   1            button bar
 *   2            status bar (labeled segments; replaces the old top divider)
 *   3..canvasBot canvas viewport (+ side tool panel on terminals >= 100 cols)
 * Narrow terminals (bottom panel present):
 *   canvasBot+1  character-set bar (magenta, F1-F10 preview, ◄ ► cycling)
 *   canvasBot+2  divider ────────────
 *   canvasBot+3+ bottom-panel rows: the side panel's controls flowed
 *                inline — colors (+char/recent in Draw), then the tool
 *                list (Draw only), then the tool's options row
 * Wide terminals (side panel, no bottom panel):
 *   canvasBot+1  divider ────────────
 *   rows-1       character-set bar
 *
 * The document is capped at 79 columns (the message-safe width); terminals
 * wider than that show a boundary marker and, at >=100 columns, a side tool
 * panel with clickable color/glyph controls in the leftover space.
 */

import { Document } from '../core/doc';
import { DEFAULT_ATTR, makeAttr } from '../core/attr';
import { clamp } from '../core/std';
import { PlotCell, linePoints, boxCells, ellipsePoints, ellipseFillPoints, halfBlockLineCells, halfBlockBoxCells, floodFill, shapeToolAction } from '../core/shapes';
import { renderTdf, layoutTdfWpStyled, tdfWpCaretXY, tdfWpHitTest, COLOR_FONT, TdfFont } from '../core/tdf';
import { QuoteStyle, formatQuote, quotePrefix, authorInitials } from '../core/quote';
import { LANGS, LangDef, langById, highlightLines, highlightLine, initialHlState, fenceTag, resolveFenceLang, HL_COMMENT } from '../core/syntax';
import { FontProvider } from '../host/types';
import { fontPicker } from './fontpicker';
import { MessageSession, TerminalCaps, InputEvent } from '../host/types';
import { Screen } from './screen';
import { HitMap, drawButton, BOX } from './widgets';
import { CHARSETS, DEFAULT_CHARSET } from './charsets';
import { theme } from './theme';
import * as keys from './keys';
import {
  InputFn, AliveFn, messageBox, dropdownMenu, glyphPicker, colorPicker,
  quotePicker, promptLine, helpOverlay, MenuItem, ModalButton
} from './modals';

export type DrawTool = 'pencil' | 'type' | 'select' | 'line' | 'box' | 'circle' | 'fill' | 'recolor';
var DRAW_TOOLS: DrawTool[] = ['pencil', 'type', 'select', 'line', 'box', 'circle', 'fill', 'recolor'];
var DRAW_TOOL_LABEL: { [k: string]: string } = {
  pencil: 'Pencil', type: 'Type', select: 'Select', line: 'Line', box: 'Box', circle: 'Circle', fill: 'Fill', recolor: 'Recolor'
};
type ColorChannel = 'fg' | 'bg' | 'both';
var CHANNEL_LABEL: { [k: string]: string } = { fg: 'FG', bg: 'BG', both: 'Both' };

// Per-tool sub-options (set from the options row / nested menu).
type LineMode = 'char' | 'half';
type BoxStyle = 'single' | 'double' | 'char' | 'half';
type ShapeFill = 'none' | 'color' | 'char';
type FillMode = 'both' | 'color' | 'char';

/** One selectable sub-option of the current tool. */
interface ToolOpt {
  id: string;
  label: string;
  cur: boolean;
}
interface ToolOptGroup {
  title: string;
  opts: ToolOpt[];
}

var MESSAGE_SAFE_WIDTH = 79;
var TAB_STOP = 4;

export interface ControllerResult {
  /** 'save' -> exit 0 after writing files; 'abort' -> exit 1. */
  action: 'save' | 'abort';
  bodyCp437: string;
  subject: string;
}

export class Controller {
  private doc: Document;
  private scr: Screen;
  private hits = new HitMap();
  private session: MessageSession;
  private caps: TerminalCaps;
  private input: InputFn;
  private alive: AliveFn;
  private fonts: FontProvider | null;

  private mode: 'text' | 'draw' = 'text';
  private subject: string;
  private topRow = 0;
  private desiredCol = 0;
  private brush = { x: 0, y: 0 };
  private glyph = 0xdb;
  private recentGlyphs: number[] = [0xdb, 0xb0, 0xb1, 0xb2, 0xc4, 0xb3, 0xdc, 0xdf];
  // Active F-key character set (index into CHARSETS): F1-F10 type its glyphs.
  private charsetIdx = DEFAULT_CHARSET;

  // Draw tools: pencil paints single cells; line/box/circle are two-point
  // (anchor + end); fill flood-fills. anchor/previewEnd track a shape in
  // progress (mouse drag or keyboard anchor→commit) for the live preview.
  private drawTool: DrawTool = 'pencil';
  private anchor: { x: number; y: number } | null = null;
  private previewEnd: { x: number; y: number } | null = null;
  // The Type tool's carriage-return column: Enter returns the cursor here on
  // the next row, like a typewriter. Set when the cursor is (re)positioned.
  private textOrigin = 0;
  // Recolor tool: which channel(s) the ink brush repaints.
  private recolorChannel: ColorChannel = 'both';
  // Tool sub-options.
  private pencilSize = 1; // 1 | 2 | 3 (brush square)
  private lineMode: LineMode = 'char';
  private boxStyle: BoxStyle = 'single';
  private boxFill: ShapeFill = 'none';
  private circleFill: ShapeFill = 'none';
  private fillMode: FillMode = 'both';
  // A rendered TheDraw big-text block awaiting placement: cells are relative
  // to (0,0); it previews at the brush and commits on click/Enter.
  private pendingStamp: { x: number; y: number; ch: number; attr: number }[] | null = null;
  private pendingW = 0;
  private pendingH = 0;
  // Live TheDraw word-processor session: type/edit logical text that renders
  // through fonts, wrapping at the screen edge from an (originX, originY).
  // fonts[i] styles text[i]; curFont styles the NEXT insertion (^K switches
  // it mid-session, so one block can mix fonts like a real word processor).
  private wp: { fonts: TdfFont[]; curFont: TdfFont; text: string; caret: number; originX: number; originY: number; gap: number } | null = null;
  // Clipboards: art (a grid of cells, from a Draw box select) and text (flow
  // lines, from a Text-mode selection). Both paste via their own mode.
  private clipArt: { w: number; h: number; cells: { dx: number; dy: number; ch: number; attr: number }[] } | null = null;
  private clipText: string | null = null;
  // Draw-mode rectangular selection (committed); Text-mode selection anchor.
  private selRect: { x0: number; y0: number; x1: number; y1: number } | null = null;
  private textAnchor: { row: number; col: number } | null = null;
  // Output format at save: null = auto-detect (art-heavy -> ANSI, else CTRL-A).
  private saveMode: 'ctrla' | 'ansi' | null = null;

  // layout (recomputed every frame by applyLayout: resize-aware)
  private canvasTop = 3;
  private canvasBottom = 0;
  private canvasW = 0;
  private panelX = -1; // -1: no side panel
  private panelW = 0;
  // Narrow terminals (no side panel) show the Draw controls as a compact
  // two-row block above the character-set bar instead.
  private bottomPanel = false;
  private getSize: (() => { cols: number; rows: number }) | null;
  // True when the last input wait timed out; gates the resize probe.
  private idle = false;

  constructor(session: MessageSession, caps: TerminalCaps, scr: Screen, input: InputFn, alive: AliveFn, fonts?: FontProvider, getSize?: () => { cols: number; rows: number }) {
    this.session = session;
    this.caps = caps;
    this.scr = scr;
    this.input = input;
    this.alive = alive;
    this.fonts = fonts === undefined ? null : fonts;
    this.getSize = getSize === undefined ? null : getSize;
    this.subject = session.meta.subject;

    var docWidth = Math.min(MESSAGE_SAFE_WIDTH, caps.cols - 1);
    this.doc = new Document(docWidth);
    if (session.sourceText.length > 0) this.doc.loadText(session.sourceText);
    this.applyLayout();
  }

  // ------------------------------------------------------------------
  // Frame composition
  // ------------------------------------------------------------------

  private canvasRows(): number {
    return this.canvasBottom - this.canvasTop + 1;
  }

  /**
   * Recompute chrome geometry from the current terminal size and mode. Runs
   * every frame (cheap arithmetic), which is what makes the view adapt live
   * to terminal resizes and to the narrow-terminal bottom panel appearing
   * only in Draw mode. The document width is fixed at creation (reflowing a
   * message mid-edit is not resize behavior); only the chrome adapts.
   */
  private applyLayout(): void {
    var caps = this.caps;
    this.panelX = -1;
    this.panelW = 0;
    if (caps.cols >= 100) {
      // Anchor the panel right next to the canvas (divider column between),
      // not to the terminal's right edge — extra width beyond the panel
      // stays empty on the far right instead of opening a gap between the
      // canvas and its controls.
      this.panelX = this.doc.width + 1;
      this.panelW = Math.min(20, caps.cols - this.panelX);
    }
    this.canvasW = Math.min(this.doc.width, this.panelX === -1 ? caps.cols : this.panelX - 1);
    // No room for the side panel: its controls stack as a bottom block in
    // both modes (colors row always; Draw adds the tools row, plus an
    // options row when the current tool has sub-options).
    this.bottomPanel = this.panelX === -1;
    var panelRows = 0;
    if (this.bottomPanel) {
      panelRows = this.mode === 'draw'
        ? 2 + (this.toolOptionGroups().length > 0 ? 1 : 0)
        : 1;
    }
    // Below the canvas: charset bar + divider + panel rows (narrow), or
    // divider + charset bar (wide). The status bar lives at the top (row 2).
    this.canvasBottom = caps.rows - 3 - panelRows;
  }

  /** Adopt a changed terminal size (Synchronet tracks NAWS reports live). */
  private pollResize(): void {
    if (this.getSize === null) return;
    var sz = this.getSize();
    if (sz.cols === this.caps.cols && sz.rows === this.caps.rows) return;
    if (sz.cols < 40 || sz.rows < 10) return; // ignore bogus/unusable reports
    this.caps.cols = sz.cols;
    this.caps.rows = sz.rows;
    this.scr.resize(sz.cols, sz.rows);
  }

  private ensureVisible(row: number): void {
    var h = this.canvasRows();
    if (row < this.topRow) this.topRow = row;
    if (row >= this.topRow + h) this.topRow = row - h + 1;
    if (this.topRow < 0) this.topRow = 0;
  }

  private compose(): void {
    var scr = this.scr;
    var cols = this.caps.cols;
    var rows = this.caps.rows;
    this.hits.clear();

    // --- title bar ---
    scr.fill(0, 0, cols, 1, 0x20, theme.title);
    var title = ' HERMedIT ';
    scr.putStr(0, 0, title, theme.title);
    // Context segments: labels dim, values (the recipient) light cyan.
    var tx = title.length;
    var putCtx = function (s: string, attr: number): void {
      if (tx < cols) scr.putStr(tx, 0, s.substring(0, cols - tx), attr);
      tx += s.length;
    };
    if (this.session.meta.area.length > 0) putCtx(this.session.meta.area + '  ', theme.titleDim);
    if (this.session.meta.to.length > 0) {
      putCtx('To: ', theme.titleDim);
      putCtx(this.session.meta.to, theme.titleValue);
      putCtx('  ', theme.titleDim);
    }
    putCtx('Subj: ', theme.titleDim);
    putCtx(this.subject.length > 0 ? this.subject : '(none)', theme.titleValue);
    // Build stamp, right-aligned: always visible proof of which build runs.
    var stamp = typeof BUILD_STAMP === 'string' ? BUILD_STAMP : 'dev';
    if (tx + stamp.length + 2 < cols) {
      scr.putStr(cols - stamp.length - 1, 0, stamp, theme.titleDim);
    }

    // --- button bar ---
    scr.fill(0, 1, cols, 1, 0x20, theme.bar);
    // Buttons show Ctrl equivalents (more reliable than F-keys, which the
    // terminal often intercepts). F1-F10 belong to the character-set bar.
    // The mode button shows the key for the mode you'd switch TO.
    var bx = 1;
    bx += drawButton(scr, this.hits, bx, 1, 'menu', 'Esc', 'Menu') + 1;
    bx += drawButton(scr, this.hits, bx, 1, 'help', '^G', 'Help') + 1;
    bx += drawButton(scr, this.hits, bx, 1, 'save', '^O', 'Save') + 1;
    if (this.session.quoteLines.length > 0) {
      bx += drawButton(scr, this.hits, bx, 1, 'quote', '^R', 'Quote') + 1;
    }
    if (this.mode === 'text') {
      bx += drawButton(scr, this.hits, bx, 1, 'mode-draw', '^D', 'Draw', false) + 1;
    } else {
      bx += drawButton(scr, this.hits, bx, 1, 'mode-text', '^T', 'Text', true) + 1;
    }
    bx += drawButton(scr, this.hits, bx, 1, 'color', '^L', 'Color') + 1;
    if (this.mode === 'draw') {
      bx += drawButton(scr, this.hits, bx, 1, 'glyph', '^K', 'Char') + 1;
    }

    // --- divider. The status bar replaced the old top divider (row 2); the
    // lower divider sits under the canvas on wide terminals, and on narrow
    // terminals below the charset bar, above the bottom-panel controls. ---
    var lowerDiv = this.bottomPanel ? this.canvasBottom + 2 : this.canvasBottom + 1;
    scr.hline(0, lowerDiv, cols, BOX.h, theme.divider);
    if (this.panelX >= 0) {
      var divX = this.panelX - 1;
      for (var dy = this.canvasTop; dy <= this.canvasBottom; dy++) scr.put(divX, dy, BOX.v, theme.divider);
      scr.put(divX, lowerDiv, BOX.teeUp, theme.divider);
    }

    // --- canvas ---
    for (var y = this.canvasTop; y <= this.canvasBottom; y++) {
      var docY = this.topRow + (y - this.canvasTop);
      for (var x = 0; x < this.canvasW; x++) {
        var cell = this.doc.cellAt(x, docY);
        scr.put(x, y, cell.ch, cell.attr);
      }
      // message-safe boundary marker just right of the document edge
      if (this.doc.width < (this.panelX === -1 ? cols : this.panelX - 1)) {
        scr.put(this.doc.width, y, BOX.v, theme.boundary);
      }
      // clear leftover width between boundary and panel/edge
      var clearFrom = this.doc.width + 1;
      var clearTo = this.panelX === -1 ? cols : this.panelX - 1;
      for (var cx = clearFrom; cx < clearTo; cx++) scr.put(cx, y, 0x20, theme.canvas);
      // and everything right of the panel on very wide terminals
      if (this.panelX >= 0) {
        for (var cx2 = this.panelX + this.panelW; cx2 < cols; cx2++) scr.put(cx2, y, 0x20, theme.canvas);
      }
    }
    this.hits.add('canvas', 0, this.canvasTop, this.canvasW - 1, this.canvasBottom);

    // live shape preview (drawn over the canvas, not committed)
    this.drawPreview();
    // committed draw selection marquee
    this.drawSelection();
    // text-mode selection highlight
    this.drawTextSelection();
    // font block awaiting placement
    if (this.pendingStamp !== null) this.drawPendingStamp();
    // live word-processor render (drawn last so its caret wins)
    if (this.wp !== null) this.drawWp();

    // --- side panel (wide terms) / bottom panel (narrow terms, draw mode) ---
    if (this.panelX >= 0) this.composePanel();
    if (this.bottomPanel) this.composeBottomPanel();

    // --- character-set bar (above the status bar) ---
    this.composeCharsetBar();

    // --- status bar ---
    this.composeStatus();

    // --- hardware cursor (caret is region-relative; translate to canvas) ---
    // WP mode positions its own caret in drawWp(); don't clobber it.
    if (this.wp === null) {
      var cur = this.mode === 'text'
        ? { x: this.doc.caretDocX(), y: this.doc.caretDocY() }
        : this.brush;
      scr.cursorX = clamp(cur.x, 0, this.canvasW - 1) + 1;
      scr.cursorY = this.canvasTop + (cur.y - this.topRow) + 1;
      scr.cursorVisible = true;
    }
  }

  private composePanel(): void {
    var scr = this.scr;
    var px = this.panelX;
    var pw = this.panelW;
    for (var y = this.canvasTop; y <= this.canvasBottom; y++) scr.fill(px, y, pw, 1, 0x20, theme.panel);

    var y0 = this.canvasTop;
    scr.putStr(px + 1, y0, 'Mode', theme.panelTitle);
    var mx = px + 1;
    mx += drawButton(scr, this.hits, mx, y0 + 1, 'mode-text', '^T', 'Text', this.mode === 'text') + 1;
    drawButton(scr, this.hits, mx, y0 + 1, 'mode-draw', '^D', 'Draw', this.mode === 'draw');

    scr.putStr(px + 1, y0 + 3, 'Foreground', theme.panelTitle);
    for (var f = 0; f < 16; f++) {
      var fx = px + 1 + (f % 8) * 2;
      var fy = y0 + 4 + Math.floor(f / 8);
      var sw = makeAttr(f & 7, 0, f >= 8);
      var isCur = (this.doc.curAttr & 0x0f) === f;
      scr.put(fx, fy, isCur ? 0xdb : 0xfe, sw);
      this.hits.add('fg' + f, fx, fy, fx + 1, fy);
    }
    scr.putStr(px + 1, y0 + 6, 'Background', theme.panelTitle);
    for (var b = 0; b < 8; b++) {
      var bxp = px + 1 + b * 2;
      var isCurB = ((this.doc.curAttr >> 4) & 0x07) === b;
      // The swatch IS the color (blank cell, colored background); the
      // selected one carries a contrast marker instead of hiding the color.
      scr.put(bxp, y0 + 7, isCurB ? 0xfe : 0x20, makeAttr(b === 7 ? 0 : 7, b, isCurB && b !== 7));
      this.hits.add('bg' + b, bxp, y0 + 7, bxp + 1, y0 + 7);
    }

    if (this.mode === 'draw') {
      // Sequential layout below the fixed color sections: the tool list's
      // length changes, so everything after it flows from `py` instead of
      // fixed offsets (which the growing list used to overwrite). Each block
      // is guarded so a short terminal truncates the panel rather than
      // bleeding into the chrome below the canvas.
      var py = y0 + 9;
      if (py <= this.canvasBottom) {
        scr.putStr(px + 1, py, 'Tools', theme.panelTitle);
        scr.putStr(px + 8, py, 'Tab', theme.keyHint);
      }
      py++;
      for (var tt = 0; tt < DRAW_TOOLS.length && py <= this.canvasBottom; tt++, py++) {
        var tool = DRAW_TOOLS[tt] as DrawTool;
        var active = tool === this.drawTool;
        var tAttr = active ? theme.panelSel : theme.panel;
        scr.fill(px + 1, py, pw - 2, 1, 0x20, tAttr);
        scr.putStr(px + 2, py, (active ? '\x10 ' : '  ') + DRAW_TOOL_LABEL[tool], tAttr);
        this.hits.add('tool-' + tool, px + 1, py, px + pw - 2, py);
      }
      // Nested options menu for the active tool's sub-options.
      if (this.toolOptionGroups().length > 0 && py <= this.canvasBottom) {
        scr.putStr(px + 2, py, '+ Options', theme.panelTitle);
        scr.putStr(px + 12, py, 'S-Tab', theme.keyHint);
        this.hits.add('tool-opts', px + 1, py, px + pw - 2, py);
        py++;
      }
      py++; // spacer
      if (py <= this.canvasBottom) {
        scr.putStr(px + 1, py, 'Char', theme.panelTitle);
        scr.put(px + 6, py, this.glyph, this.doc.curAttr);
        scr.putStr(px + 8, py, '^K', theme.keyHint);
        this.hits.add('glyph', px + 6, py, px + 6, py);
      }
      py++;
      if (py + 1 <= this.canvasBottom) {
        scr.putStr(px + 1, py, 'Recent', theme.panelTitle);
        py++;
        for (var r = 0; r < this.recentGlyphs.length && r < 8; r++) {
          var rx = px + 1 + r * 2;
          scr.put(rx, py, this.recentGlyphs[r] as number, theme.panel);
          this.hits.add('recent' + r, rx, py, rx + 1, py);
        }
      }
    } else {
      scr.putStr(px + 1, y0 + 9, 'Keys', theme.panelTitle);
      // key tokens yellow (the out-of-button hint color), descriptions plain
      scr.putStr(px + 1, y0 + 10, 'Enter', theme.keyHint);
      scr.putStr(px + 7, y0 + 10, 'paragraph', theme.panel);
      scr.putStr(px + 1, y0 + 11, 'Ins', theme.keyHint);
      scr.putStr(px + 6, y0 + 11, 'overwrite', theme.panel);
      scr.putStr(px + 1, y0 + 12, '^Z/^Y', theme.keyHint);
      scr.putStr(px + 7, y0 + 12, 'undo/redo', theme.panel);
      scr.putStr(px + 1, y0 + 14, 'Click places the', theme.panel);
      scr.putStr(px + 1, y0 + 15, 'cursor; wheel', theme.panel);
      scr.putStr(px + 1, y0 + 16, 'scrolls.', theme.panel);
    }
  }

  /**
   * Narrow-terminal replacement for the side panel, shown in BOTH modes
   * like the side panel is: the same controls flowed inline above the
   * character-set bar, using the horizontal space a narrow terminal does
   * have. Text mode gets the colors row; Draw mode adds char/recent to it
   * plus a second row with the tool list. All hit ids match the side
   * panel's, so clicks share one code path.
   */
  private composeBottomPanel(): void {
    var scr = this.scr;
    // Bottom of the screen, below the charset bar + divider.
    var y1 = this.canvasBottom + 3;
    var y2 = this.canvasBottom + 4;
    scr.fill(0, y1, this.caps.cols, this.mode === 'draw' ? 2 : 1, 0x20, theme.panel);

    var x = 1;
    scr.putStr(x, y1, 'FG', theme.panelTitle);
    x += 3;
    for (var f = 0; f < 16; f++) {
      var sw = makeAttr(f & 7, 0, f >= 8);
      var isCur = (this.doc.curAttr & 0x0f) === f;
      scr.put(x, y1, isCur ? 0xdb : 0xfe, sw);
      this.hits.add('fg' + f, x, y1, x, y1);
      x++;
    }
    x += 2;
    scr.putStr(x, y1, 'BG', theme.panelTitle);
    x += 3;
    for (var b = 0; b < 8; b++) {
      var isCurB = ((this.doc.curAttr >> 4) & 0x07) === b;
      // blank cell in the color; contrast marker on the selected one
      scr.put(x, y1, isCurB ? 0xfe : 0x20, makeAttr(b === 7 ? 0 : 7, b, isCurB && b !== 7));
      this.hits.add('bg' + b, x, y1, x, y1);
      x++;
    }
    if (this.mode !== 'draw') return;

    x += 2;
    scr.putStr(x, y1, 'Char', theme.panelTitle);
    x += 5;
    scr.put(x, y1, this.glyph, this.doc.curAttr);
    this.hits.add('glyph', x, y1, x, y1);
    x += 2;
    scr.putStr(x, y1, '^K', theme.keyHint);
    x += 3;
    for (var r = 0; r < this.recentGlyphs.length && r < 8; r++) {
      scr.put(x, y1, this.recentGlyphs[r] as number, theme.panel);
      this.hits.add('recent' + r, x, y1, x, y1);
      x++;
    }

    var tx = 1;
    scr.putStr(tx, y2, 'Tab', theme.keyHint);
    tx += 4;
    for (var t = 0; t < DRAW_TOOLS.length; t++) {
      var tool = DRAW_TOOLS[t] as DrawTool;
      var label = DRAW_TOOL_LABEL[tool] as string;
      var active = tool === this.drawTool;
      scr.putStr(tx, y2, label, active ? theme.panelSel : theme.panel);
      this.hits.add('tool-' + tool, tx, y2, tx + label.length - 1, y2);
      tx += label.length + 1;
    }

    // Options row: the current tool's sub-options flowed inline.
    var groups = this.toolOptionGroups();
    if (groups.length === 0) return;
    var y3 = y2 + 1;
    scr.fill(0, y3, this.caps.cols, 1, 0x20, theme.panel);
    var ox = 1;
    // key hint: Shift+Tab opens the options menu (clicking it works too)
    scr.putStr(ox, y3, 'S-Tab', theme.keyHint);
    this.hits.add('tool-opts', ox, y3, ox + 4, y3);
    ox += 6;
    for (var g = 0; g < groups.length; g++) {
      var grp = groups[g] as ToolOptGroup;
      scr.putStr(ox, y3, grp.title + ':', theme.panelTitle);
      ox += grp.title.length + 2;
      for (var o = 0; o < grp.opts.length; o++) {
        var opt = grp.opts[o] as ToolOpt;
        scr.putStr(ox, y3, opt.label, opt.cur ? theme.panelSel : theme.panel);
        this.hits.add(opt.id, ox, y3, ox + opt.label.length - 1, y3);
        ox += opt.label.length + 1;
      }
      ox += 2;
    }
  }

  /** Nested options menu for the current tool (wide-terminal sidebar path). */
  private openToolOptionsMenu(): void {
    var groups = this.toolOptionGroups();
    if (groups.length === 0) return;
    var items: MenuItem[] = [];
    for (var g = 0; g < groups.length; g++) {
      var grp = groups[g] as ToolOptGroup;
      if (g > 0) items.push({ id: 'sep-opt' + g, label: '', keyLabel: '', separator: true });
      for (var o = 0; o < grp.opts.length; o++) {
        var opt = grp.opts[o] as ToolOpt;
        items.push({ id: opt.id, label: (opt.cur ? '\x07 ' : '  ') + grp.title + ': ' + opt.label, keyLabel: '' });
      }
    }
    // Anchor near the side panel when present, else near the tool bar.
    var mx = this.panelX >= 0 ? Math.max(1, this.panelX - 24) : 1;
    var id = dropdownMenu(this.scr, this.input, this.alive, mx, 2, items);
    if (id !== null) this.action(id); // routes opt-* and chan-* alike
  }

  /**
   * The character-set bar (row rows-2, above the status bar): the
   * Moebius/PabloDraw F-key drawing convention. Ten labeled slots preview
   * what F1-F10 type (glyphs shown in the current color so the preview
   * matches what lands); clickable ◄ ► arrows — or F11/F12, or Ctrl+,/. on
   * terminals that can send them — cycle the sets. The whole cluster is
   * centered in the available width; the label form degrades gracefully
   * (drop the set indicator, then shorten "F1" labels to digits) when the
   * terminal is narrow.
   */
  private composeCharsetBar(): void {
    var scr = this.scr;
    var cols = this.caps.cols;
    // Wide terminals: the last row. Narrow terminals: directly under the
    // canvas (canvas → bar → divider → controls).
    var y = this.bottomPanel ? this.canvasBottom + 1 : this.caps.rows - 1;
    var set = CHARSETS[this.charsetIdx] as number[];
    scr.fill(0, y, cols, 1, 0x20, theme.charsetBar);

    var ind = (this.charsetIdx + 1) + '/' + CHARSETS.length;
    // Cluster width: "[F11 <-] " + ten "F<n><glyph> " slots + " [F12 ->]".
    // Full slot labels (F1..F10) cost 9*4+5 = 41 cells; digits (1..0) 30.
    var arrowW = 8; // drawButton "[F11 <-]"
    var fullW = arrowW + 1 + 41 + 1 + arrowW;
    var digitW = arrowW + 1 + 30 + 1 + arrowW;
    var fullLabels = fullW <= cols - 1;
    var clusterW = fullLabels ? fullW : digitW;
    var showInd = clusterW + 2 + ind.length <= cols - 1;
    var total = clusterW + (showInd ? 2 + ind.length : 0);
    var x = Math.max(0, Math.floor((cols - total) / 2));

    // prev/next are labeled buttons carrying their hotkeys, arrows on the
    // OUTSIDE edges ("[<- F11] ... [F12 ->]") and blue to stand apart from
    // the red key text. F11/F12 only: Ctrl+,/. also work where the terminal
    // can transmit them, but advertising keys most BBS terminals can't send
    // would just confuse.
    scr.put(x, y, 0x5b, theme.button);
    scr.putStr(x + 1, y, '<-', theme.buttonArrow);
    scr.put(x + 3, y, 0x20, theme.button);
    scr.putStr(x + 4, y, 'F11', theme.buttonKey);
    scr.put(x + 7, y, 0x5d, theme.button);
    this.hits.add('charset-prev', x, y, x + 7, y);
    x += arrowW + 1;
    // F1..F10 slots: key label + the glyph in the current drawing color
    for (var i = 0; i < 10; i++) {
      var label = fullLabels ? 'F' + (i + 1) : (i === 9 ? '0' : String(i + 1));
      scr.putStr(x, y, label, theme.charsetKey);
      scr.put(x + label.length, y, set[i] as number, this.doc.curAttr);
      this.hits.add('fkey' + i, x, y, x + label.length, y);
      x += label.length + 2;
    }
    x += 1;
    scr.put(x, y, 0x5b, theme.button);
    scr.putStr(x + 1, y, 'F12', theme.buttonKey);
    scr.put(x + 4, y, 0x20, theme.button);
    scr.putStr(x + 5, y, '->', theme.buttonArrow);
    scr.put(x + 7, y, 0x5d, theme.button);
    this.hits.add('charset-next', x, y, x + 7, y);
    if (showInd) scr.putStr(x + arrowW + 2, y, ind, theme.charsetKey);
  }

  /**
   * Segmented status bar: black background, DARKGRAY labels, white values,
   * e.g. `Mode:TEXT  Ln:5  Col:12 ... FG:█ BG:█`. Labels fall back to their
   * 2-letter abbreviations when the full form doesn't fit the row; key hints
   * (key white, word gray) sit right-aligned.
   */
  private composeStatus(): void {
    var scr = this.scr;
    // Row 2, directly under the button bar (it replaced the top divider).
    var y = 2;
    var cols = this.caps.cols;
    scr.fill(0, y, cols, 1, 0x20, theme.status);

    // Segments: label/abbr pair + a value (text, or a single glyph swatch).
    var segs: { l: string; a: string; v: string; glyph?: number; gattr?: number; hit?: string }[] = [];
    var hints: { key: string; word: string }[] = [];
    var fgSwatch = { l: 'FG:', a: 'FG:', v: '', glyph: 0xdb, gattr: this.doc.curAttr & 0x0f, hit: 'color' };
    var bgSwatch = { l: 'BG:', a: 'BG:', v: '', glyph: 0x20, gattr: this.doc.curAttr & 0x70, hit: 'color' };

    if (this.wp !== null) {
      segs.push({ l: 'Mode:', a: 'Md:', v: 'FONT WP' });
      segs.push({ l: 'Font:', a: 'Fn:', v: this.wp.curFont.name });
      segs.push(fgSwatch);
      segs.push(bgSwatch);
      hints = [{ key: '^K', word: 'font' }, { key: 'Esc', word: 'done' }];
    } else if (this.pendingStamp !== null) {
      segs.push({ l: 'Mode:', a: 'Md:', v: 'PLACE' });
      segs.push({ l: 'Size:', a: 'Sz:', v: this.pendingW + 'x' + this.pendingH });
      segs.push({ l: 'Pos:', a: 'P:', v: (this.brush.x + 1) + ',' + (this.brush.y + 1) });
      hints = [{ key: 'Enter', word: 'stamp' }, { key: 'Esc', word: 'cancel' }];
    } else if (this.mode === 'text') {
      segs.push({ l: 'Mode:', a: 'Md:', v: 'TEXT' });
      var reg = this.doc.region;
      if (reg !== null && reg.pre === true) {
        var rl = reg.lang;
        segs.push({ l: 'Code:', a: 'Cd:', v: (rl !== undefined && rl !== '' ? rl : 'plain') });
      } else if (reg !== null) {
        segs.push({ l: 'Box:', a: 'Bx:', v: reg.width + 'x' + reg.height });
      }
      segs.push({ l: 'Ln:', a: 'Ln:', v: String(this.doc.caret.row + 1) });
      segs.push({ l: 'Col:', a: 'Co:', v: String(this.doc.caret.col + 1) });
      segs.push({ l: '', a: '', v: this.doc.insertMode ? 'Ins' : 'Ovr' });
      if (reg === null) segs.push({ l: 'Width:', a: 'W:', v: String(this.doc.width) });
      segs.push(fgSwatch);
      segs.push(bgSwatch);
      // no hints: the Menu/Help buttons sit directly above
    } else {
      segs.push({ l: 'Mode:', a: 'Md:', v: 'DRAW' });
      var toolName = DRAW_TOOL_LABEL[this.drawTool] as string;
      var tn = this.drawTool === 'recolor' ? toolName + ':' + CHANNEL_LABEL[this.recolorChannel] : toolName;
      segs.push({ l: 'Tool:', a: 'Tl:', v: tn, hit: 'tool-opts' });
      segs.push({ l: 'Pos:', a: 'P:', v: (this.brush.x + 1) + ',' + (this.brush.y + 1) });
      segs.push({ l: 'Char:', a: 'Ch:', v: '', glyph: this.glyph, gattr: this.doc.curAttr, hit: 'glyph' });
      segs.push(fgSwatch);
      segs.push(bgSwatch);
      // Tab/S-Tab have no button equivalents; Menu/Help buttons sit above.
      hints = [{ key: 'Tab', word: 'tool' }, { key: 'S-Tab', word: 'opts' }];
    }

    // Right-aligned hints; measure them first.
    var hintW = 1;
    for (var h = 0; h < hints.length; h++) hintW += hints[h]!.key.length + 1 + hints[h]!.word.length + 2;
    var hx = cols - hintW;
    for (var h2 = 0; h2 < hints.length; h2++) {
      scr.putStr(hx, y, hints[h2]!.key, theme.keyHint);
      hx += hints[h2]!.key.length + 1;
      scr.putStr(hx, y, hints[h2]!.word, theme.statusLabel);
      hx += hints[h2]!.word.length + 2;
    }

    // Full labels if they fit, else the 2-letter abbreviations.
    var limit = cols - hintW - 2;
    var full = 1;
    for (var m = 0; m < segs.length; m++) {
      var sm = segs[m]!;
      full += sm.l.length + (sm.glyph !== undefined ? 1 : sm.v.length) + 2;
    }
    var abbr = full - 1 > limit;
    var x = 1;
    for (var i = 0; i < segs.length; i++) {
      var sg = segs[i]!;
      var label = abbr ? sg.a : sg.l;
      var vw = sg.glyph !== undefined ? 1 : sg.v.length;
      if (x + label.length + vw > limit) break; // never collide with the hints
      var x0 = x;
      scr.putStr(x, y, label, theme.statusLabel);
      x += label.length;
      if (sg.glyph !== undefined) {
        scr.put(x, y, sg.glyph, sg.gattr === undefined ? theme.statusValue : sg.gattr);
        x += 1;
      } else {
        scr.putStr(x, y, sg.v, theme.statusValue);
        x += sg.v.length;
      }
      if (sg.hit !== undefined) this.hits.add(sg.hit, x0, y, x - 1, y);
      x += 2;
    }
  }

  // ------------------------------------------------------------------
  // Main loop
  // ------------------------------------------------------------------

  run(): ControllerResult {
    while (this.alive()) {
      // Only poll for a resize when the input loop is idle (the previous
      // input wait timed out): the host's size probe is an active CPR query
      // whose blocking reply-read could swallow a keystroke mid-typing.
      if (this.idle) this.pollResize();
      this.applyLayout();
      var focusRow: number;
      if (this.wp !== null) {
        // keep the bottom of the caret's current display line in view
        var wpLay = this.wpLayout();
        var cpos = tdfWpCaretXY(wpLay, this.wp.caret);
        focusRow = this.wp.originY + cpos.y + wpLay.lines[cpos.line]!.render.height - 1;
      } else {
        focusRow = this.mode === 'text' ? this.doc.caretDocY() : this.brush.y;
      }
      this.ensureVisible(focusRow);
      this.compose();
      this.scr.flush();

      // Short idle timeout so terminal resizes are picked up promptly even
      // with no keyboard/mouse activity (an idle tick costs one no-op flush).
      var ev = this.input(1000);
      this.idle = ev.type === 'none';
      if (ev.type === 'none') continue;

      var result = ev.type === 'mouse' ? this.handleMouse(ev) : this.handleKey(ev.key);
      if (result !== null) return result;
    }
    // Disconnected: preserve the work like FSEditor does (write the body),
    // but report abort so the host never posts a half-finished message as OK.
    return { action: 'abort', bodyCp437: this.doc.toMessageBody(true), subject: this.subject };
  }

  // ------------------------------------------------------------------
  // Mouse
  // ------------------------------------------------------------------

  private screenToDoc(mx: number, my: number): { x: number; y: number } | null {
    var x = mx - 1;
    var y = my - 1;
    if (y < this.canvasTop || y > this.canvasBottom) return null;
    if (x < 0 || x >= this.canvasW) return null;
    return { x: x, y: this.topRow + (y - this.canvasTop) };
  }

  private handleMouse(ev: { x: number; y: number; button: number; press: boolean; release: boolean; motion: boolean; wheel: number }): ControllerResult | null {
    if (ev.wheel !== 0) {
      this.scrollBy(ev.wheel * 3);
      return null;
    }
    var pos = this.screenToDoc(ev.x, ev.y);
    // In WP mode the mouse is the text cursor: a click places the caret at the
    // nearest character in the rendered block (the origin is anchored once).
    if (this.wp !== null) {
      if (pos !== null) {
        if (ev.press && ev.button === 0) {
          this.wp.caret = tdfWpHitTest(this.wpLayout(), pos.x - this.wp.originX, pos.y - this.wp.originY);
          this.wpSyncFont();
        }
        return null;
      }
      // Off-canvas: chrome stays clickable during a WP session. Controls
      // that make sense mid-session (help, colors, charset cycling) act
      // directly; anything else (save, menu, tools...) first raises the
      // stamp/keep/discard prompt, exactly like pressing Esc.
      if (ev.press && ev.button === 0) {
        var wid = this.hits.test(ev.x - 1, ev.y - 1);
        if (wid === null) return null;
        if (wid === 'help' || wid === 'color' || wid === 'charset-prev' || wid === 'charset-next' ||
            wid.substring(0, 2) === 'fg' || wid.substring(0, 2) === 'bg') {
          return this.action(wid);
        }
        return this.finishWp();
      }
      return null;
    }
    // Font-block placement: move with the cursor, click to stamp, right-click
    // to cancel.
    if (this.pendingStamp !== null) {
      if (pos !== null) {
        this.brush = { x: pos.x, y: pos.y };
        // Only a REAL click commits: SGR drag reports carry press===true,
        // and a stray drag tick must not stamp the block mid-motion.
        if (ev.press && !ev.motion && ev.button === 0) this.commitStamp();
        else if (ev.press && !ev.motion && ev.button === 2) this.pendingStamp = null;
      }
      return null;
    }
    if (this.mode === 'draw') {
      if (pos !== null) {
        this.handleDrawCanvasMouse(ev, pos);
        return null;
      }
      // Off-canvas: release ends a shape drag; a press hits chrome buttons.
      if (ev.release && this.anchor !== null && this.previewEnd !== null) this.commitShape();
      if (ev.press && ev.button === 0) {
        var did = this.hits.test(ev.x - 1, ev.y - 1);
        if (did !== null) return this.action(did);
      }
      return null;
    }
    if (pos !== null && ev.button === 0 && (ev.press || ev.motion)) {
      if (ev.press && !ev.motion) {
        this.placeCaretAt(pos.x, pos.y);
        this.textAnchor = { row: this.doc.caret.row, col: this.doc.caret.col };
      } else if (ev.motion && this.textAnchor !== null) {
        this.extendCaretTo(pos.x, pos.y);
      }
      return null;
    }
    if (ev.press && ev.button === 0) {
      var id = this.hits.test(ev.x - 1, ev.y - 1);
      if (id !== null) return this.action(id);
    }
    return null;
  }

  /**
   * Place the caret at a clicked canvas cell.
   *
   * The box region is STICKY: while you are inside a box, clicking anywhere
   * within it (interior or its own border) just repositions the caret and
   * keeps you in the box — a stray click no longer dumps the text back to the
   * full-width body. Only a click clearly outside the current box leaves it;
   * a click inside a different box enters that one.
   */
  private placeCaretAt(docX: number, docY: number): void {
    var active = this.doc.region;
    if (active !== null && this.withinBoxOuter(active, docX, docY)) {
      this.moveCaretInBox(active, docX, docY);
    } else {
      var box = this.doc.detectBox(docX, docY);
      if (box !== null) {
        this.doc.setRegion(box);
        this.moveCaretInBox(box, docX, docY);
      } else {
        this.doc.clearRegion();
        var row = clamp(docY, 0, this.doc.lines.length - 1);
        var l2 = this.doc.lines[row];
        this.doc.caret = { row: row, col: clamp(docX, 0, l2 === undefined ? 0 : l2.text.length) };
      }
    }
    this.desiredCol = this.doc.caret.col;
    this.doc.breakUndoGroup();
  }

  /** True when (docX, docY) is within a box's outer rectangle (walls included). */
  private withinBoxOuter(r: { left: number; top: number; width: number; height: number }, docX: number, docY: number): boolean {
    return docX >= r.left - 1 && docX <= r.left + r.width &&
      docY >= r.top - 1 && docY <= r.top + r.height;
  }

  private moveCaretInBox(r: { left: number; top: number }, docX: number, docY: number): void {
    var row = clamp(docY - r.top, 0, this.doc.lines.length - 1);
    var line = this.doc.lines[row];
    this.doc.caret = { row: row, col: clamp(docX - r.left, 0, line === undefined ? 0 : line.text.length) };
  }

  private scrollBy(delta: number): void {
    var maxTop = Math.max(0, this.doc.rowCount() - this.canvasRows());
    this.topRow = clamp(this.topRow + delta, 0, maxTop);
    // keep the focus point inside the viewport so ensureVisible doesn't snap
    // back. The caret-follow nicety only applies to the full-width body; a box
    // region is small and its caret is region-relative, so leave it be.
    if (this.mode === 'text' && this.doc.region === null) {
      var r = clamp(this.doc.caret.row, this.topRow, this.topRow + this.canvasRows() - 1);
      if (r !== this.doc.caret.row) {
        var line = this.doc.lines[clamp(r, 0, this.doc.lines.length - 1)];
        this.doc.caret = {
          row: clamp(r, 0, this.doc.lines.length - 1),
          col: clamp(this.desiredCol, 0, line === undefined ? 0 : line.text.length)
        };
      }
    } else if (this.mode === 'draw') {
      this.brush.y = clamp(this.brush.y, this.topRow, this.topRow + this.canvasRows() - 1);
    }
  }

  // ------------------------------------------------------------------
  // Actions (buttons, menu items, hotkeys funnel here)
  // ------------------------------------------------------------------

  private action(id: string): ControllerResult | null {
    if (id === 'canvas') return null;
    if (id === 'menu') return this.openMenu();
    if (id === 'help') {
      helpOverlay(this.scr, this.input, this.alive, this.mode);
      return null;
    }
    if (id === 'save') return this.trySave();
    if (id === 'abort') return this.tryAbort();
    if (id === 'quote') {
      this.openQuotes();
      return null;
    }
    if (id === 'mode' || id === 'mode-text' || id === 'mode-draw') {
      if (id === 'mode') this.mode = this.mode === 'text' ? 'draw' : 'text';
      else this.mode = id === 'mode-draw' ? 'draw' : 'text';
      if (this.mode === 'draw') {
        this.brush = { x: clamp(this.doc.caret.col, 0, this.doc.width - 1), y: this.doc.caret.row };
      }
      return null;
    }
    if (id === 'color') {
      var attr = colorPicker(this.scr, this.input, this.alive, this.doc.curAttr);
      if (attr !== null) this.doc.curAttr = attr;
      return null;
    }
    if (id === 'glyph') {
      var g = glyphPicker(this.scr, this.input, this.alive, this.glyph);
      if (g !== null) this.setGlyph(g);
      return null;
    }
    if (id === 'pick') {
      this.eyedrop();
      return null;
    }
    if (id === 'tool-opts') {
      this.openToolOptionsMenu();
      return null;
    }
    if (id.substring(0, 5) === 'tool-') {
      if (this.mode !== 'draw') this.mode = 'draw';
      this.setTool(id.substring(5) as DrawTool);
      return null;
    }
    if (id.substring(0, 5) === 'chan-') {
      this.recolorChannel = id.substring(5) as ColorChannel;
      return null;
    }
    if (id.substring(0, 4) === 'opt-') {
      this.applyToolOption(id);
      return null;
    }
    if (id === 'font') {
      this.openFontPicker();
      return null;
    }
    if (id === 'fontwp') {
      this.openFontWordProcessor();
      return null;
    }
    if (id === 'copy') { if (this.mode === 'draw') this.copyArtSelection(false); else this.copyTextSelection(false); return null; }
    if (id === 'cut') { if (this.mode === 'draw') this.copyArtSelection(true); else this.copyTextSelection(true); return null; }
    if (id === 'paste') {
      if (this.mode === 'draw') { if (this.clipArt !== null) this.pasteArt(); }
      else { if (this.clipText !== null) this.pasteText(); }
      return null;
    }
    if (id === 'code-insert') {
      this.insertCodeBlock();
      return null;
    }
    if (id === 'subject') {
      var ns = promptLine(this.scr, this.input, this.alive, 'Message subject', this.subject, 70);
      if (ns !== null) this.subject = ns;
      return null;
    }
    if (id === 'undo') {
      this.doc.undo();
      return null;
    }
    if (id === 'redo') {
      this.doc.redo();
      return null;
    }
    if (id === 'leave-box') {
      this.doc.clearRegion();
      this.desiredCol = this.doc.caret.col;
      return null;
    }
    if (id.substring(0, 2) === 'fg') {
      var f = parseInt(id.substring(2), 10);
      this.doc.curAttr = (this.doc.curAttr & ~0x0f) | (f & 7) | (f >= 8 ? 0x08 : 0);
      return null;
    }
    if (id.substring(0, 2) === 'bg') {
      var b = parseInt(id.substring(2), 10);
      this.doc.curAttr = (this.doc.curAttr & ~0x70) | ((b & 7) << 4);
      return null;
    }
    if (id.substring(0, 6) === 'recent') {
      var r = parseInt(id.substring(6), 10);
      var rg = this.recentGlyphs[r];
      if (rg !== undefined) this.setGlyph(rg);
      return null;
    }
    if (id === 'charset-prev') { this.cycleCharset(-1); return null; }
    if (id === 'charset-next') { this.cycleCharset(1); return null; }
    if (id.substring(0, 4) === 'fkey') {
      this.typeCharsetChar(parseInt(id.substring(4), 10));
      return null;
    }
    return null;
  }

  private cycleCharset(delta: number): void {
    var n = CHARSETS.length;
    this.charsetIdx = (this.charsetIdx + delta + n) % n;
  }

  /**
   * Type slot `i` of the active character set (F1..F10 or a click on the
   * bar): fixed art at the brush in Draw mode (any tool — it's an explicit
   * character action, like TheDraw), flowing text at the caret in Text mode.
   */
  private typeCharsetChar(slot: number): void {
    var set = CHARSETS[this.charsetIdx];
    if (set === undefined) return;
    var code = set[slot];
    if (code === undefined) return;
    if (this.mode === 'draw') {
      this.doc.setArt(this.brush.x, this.brush.y, { ch: code, attr: this.doc.curAttr });
      this.brush.x = clamp(this.brush.x + 1, 0, this.doc.width - 1);
    } else {
      this.textAnchor = null;
      this.doc.insertChar(code);
      this.desiredCol = this.doc.caret.col;
      this.rehighlightCode();
    }
  }

  private setGlyph(g: number): void {
    this.glyph = g;
    for (var i = 0; i < this.recentGlyphs.length; i++) {
      if (this.recentGlyphs[i] === g) this.recentGlyphs.splice(i, 1);
    }
    this.recentGlyphs.unshift(g);
    if (this.recentGlyphs.length > 8) this.recentGlyphs.pop();
  }

  /** Cell offsets the pencil covers at the current size (square brush). */
  private brushOffsets(): { dx: number; dy: number }[] {
    var s = this.pencilSize;
    var out: { dx: number; dy: number }[] = [];
    var from = s === 3 ? -1 : 0;
    var to = s === 1 ? 0 : 1;
    for (var dy = from; dy <= to; dy++) {
      for (var dx = from; dx <= to; dx++) out.push({ dx: dx, dy: dy });
    }
    return out;
  }

  private paint(): void {
    var offs = this.brushOffsets();
    for (var i = 0; i < offs.length; i++) {
      var x = this.brush.x + offs[i]!.dx;
      var y = this.brush.y + offs[i]!.dy;
      if (x < 0 || x >= this.doc.width || y < 0) continue;
      this.doc.setArt(x, y, { ch: this.glyph, attr: this.doc.curAttr });
    }
  }

  /** Erase at the brush, honoring the pencil size for the pencil tool. */
  private eraseBrush(x: number, y: number): void {
    var offs = this.drawTool === 'pencil' ? this.brushOffsets() : [{ dx: 0, dy: 0 }];
    for (var i = 0; i < offs.length; i++) {
      var ex = x + offs[i]!.dx;
      var ey = y + offs[i]!.dy;
      if (ex < 0 || ex >= this.doc.width || ey < 0) continue;
      this.doc.eraseArt(ex, ey);
    }
  }

  private eyedrop(): void {
    var cell = this.doc.cellAt(this.brush.x, this.brush.y);
    this.setGlyph(cell.ch);
    this.doc.curAttr = cell.attr;
  }

  /**
   * The current tool's sub-options, grouped. Drives the options row (narrow
   * bottom layout), the sidebar's nested menu (wide layout), and the Esc
   * menu. Option ids route through action().
   */
  private toolOptionGroups(): ToolOptGroup[] {
    var t = this.drawTool;
    if (t === 'pencil') {
      return [{ title: 'Size', opts: [
        { id: 'opt-psize-1', label: '1', cur: this.pencilSize === 1 },
        { id: 'opt-psize-2', label: '2', cur: this.pencilSize === 2 },
        { id: 'opt-psize-3', label: '3', cur: this.pencilSize === 3 }
      ] }];
    }
    if (t === 'type') {
      return [{ title: 'Text', opts: [
        { id: 'opt-type-ascii', label: 'ASCII', cur: true },
        { id: 'opt-type-tdf', label: 'TDF font...', cur: false }
      ] }];
    }
    if (t === 'line') {
      return [{ title: 'Style', opts: [
        { id: 'opt-line-char', label: 'Char', cur: this.lineMode === 'char' },
        { id: 'opt-line-half', label: 'Half-block', cur: this.lineMode === 'half' }
      ] }];
    }
    if (t === 'box') {
      return [
        { title: 'Style', opts: [
          { id: 'opt-boxstyle-single', label: 'Single', cur: this.boxStyle === 'single' },
          { id: 'opt-boxstyle-double', label: 'Double', cur: this.boxStyle === 'double' },
          { id: 'opt-boxstyle-char', label: 'Char', cur: this.boxStyle === 'char' },
          { id: 'opt-boxstyle-half', label: 'Half', cur: this.boxStyle === 'half' }
        ] },
        { title: 'Fill', opts: [
          { id: 'opt-boxfill-none', label: 'None', cur: this.boxFill === 'none' },
          { id: 'opt-boxfill-color', label: 'Color', cur: this.boxFill === 'color' },
          { id: 'opt-boxfill-char', label: 'Char', cur: this.boxFill === 'char' }
        ] }
      ];
    }
    if (t === 'circle') {
      return [{ title: 'Fill', opts: [
        { id: 'opt-circfill-none', label: 'None', cur: this.circleFill === 'none' },
        { id: 'opt-circfill-color', label: 'Color', cur: this.circleFill === 'color' },
        { id: 'opt-circfill-char', label: 'Char', cur: this.circleFill === 'char' }
      ] }];
    }
    if (t === 'fill') {
      return [{ title: 'Apply', opts: [
        { id: 'opt-fillmode-both', label: 'Char+Color', cur: this.fillMode === 'both' },
        { id: 'opt-fillmode-color', label: 'Color', cur: this.fillMode === 'color' },
        { id: 'opt-fillmode-char', label: 'Char', cur: this.fillMode === 'char' }
      ] }];
    }
    if (t === 'recolor') {
      return [{ title: 'Channel', opts: [
        { id: 'chan-fg', label: 'FG', cur: this.recolorChannel === 'fg' },
        { id: 'chan-bg', label: 'BG', cur: this.recolorChannel === 'bg' },
        { id: 'chan-both', label: 'Both', cur: this.recolorChannel === 'both' }
      ] }];
    }
    return []; // select: no options
  }

  /** Apply a tool sub-option id. Returns true when it was one. */
  private applyToolOption(id: string): boolean {
    if (id.substring(0, 10) === 'opt-psize-') { this.pencilSize = parseInt(id.substring(10), 10); return true; }
    if (id === 'opt-type-ascii') return true; // the normal Type tool
    if (id === 'opt-type-tdf') { this.openFontWordProcessor(); return true; }
    if (id.substring(0, 9) === 'opt-line-') { this.lineMode = id.substring(9) as LineMode; return true; }
    if (id.substring(0, 13) === 'opt-boxstyle-') { this.boxStyle = id.substring(13) as BoxStyle; return true; }
    if (id.substring(0, 12) === 'opt-boxfill-') { this.boxFill = id.substring(12) as ShapeFill; return true; }
    if (id.substring(0, 13) === 'opt-circfill-') { this.circleFill = id.substring(13) as ShapeFill; return true; }
    if (id.substring(0, 13) === 'opt-fillmode-') { this.fillMode = id.substring(13) as FillMode; return true; }
    return false;
  }

  private setTool(tool: DrawTool): void {
    this.drawTool = tool;
    this.anchor = null;
    this.previewEnd = null;
    // Leaving the Select tool dismisses the selection marquee entirely.
    if (tool !== 'select') this.selRect = null;
    if (tool === 'type') this.textOrigin = this.brush.x; // Enter returns here
  }

  private cycleTool(): void {
    var i = 0;
    for (var t = 0; t < DRAW_TOOLS.length; t++) if (DRAW_TOOLS[t] === this.drawTool) i = t;
    this.setTool(DRAW_TOOLS[(i + 1) % DRAW_TOOLS.length] as DrawTool);
  }


  // ------------------------------------------------------------------
  // Draw tools
  // ------------------------------------------------------------------

  private handleDrawCanvasMouse(ev: { button: number; press: boolean; release: boolean; motion: boolean }, pos: { x: number; y: number }): void {
    this.brush = { x: pos.x, y: pos.y };
    var tool = this.drawTool;
    if (tool === 'pencil') {
      if (ev.press || ev.motion) {
        if (ev.button === 0) this.paint();
        else if (ev.button === 2) this.eraseBrush(pos.x, pos.y);
        else if (ev.button === 1 && ev.press) this.eyedrop();
      }
      return;
    }
    // SGR motion reports (drag) also arrive with press === true — they end in
    // 'M' like a button-down. So a real button-down is press && !motion; a
    // drag is motion; a button-up is release. Order matters: handle release
    // and motion before the button-down, and never treat a drag tick as a
    // fresh press (that would reset the anchor to the cursor every tick).
    var down = ev.press && !ev.motion;
    if (tool === 'type') {
      if (down && ev.button === 0) { this.brush = { x: pos.x, y: pos.y }; this.textOrigin = pos.x; }
      else if (down && ev.button === 2) this.doc.eraseArt(pos.x, pos.y);
      else if (down && ev.button === 1) this.eyedrop();
      return;
    }
    if (tool === 'recolor') {
      // paint on press or drag, either button; middle picks up glyph+color
      if ((ev.press || ev.motion) && (ev.button === 0 || ev.button === 2)) this.recolorAt(pos.x, pos.y);
      else if (down && ev.button === 1) this.eyedrop();
      return;
    }
    if (tool === 'fill') {
      if (down && ev.button === 0) this.fillAt(pos.x, pos.y, false);
      else if (down && ev.button === 2) this.fillAt(pos.x, pos.y, true);
      else if (down && ev.button === 1) this.eyedrop();
      return;
    }
    // line / box / circle — the tested state machine (shapeToolAction) drives
    // it, so drag-release and click-move-click both work and drag ticks never
    // reset the anchor.
    var atAnchor = this.anchor !== null && this.anchor.x === pos.x && this.anchor.y === pos.y;
    var act = shapeToolAction(ev, this.anchor !== null, atAnchor);
    if (act === 'eyedrop') this.eyedrop();
    else if (act === 'cancel') { this.anchor = null; this.previewEnd = null; }
    else if (act === 'set-anchor') {
      // Starting a new selection dismisses the old marquee IMMEDIATELY,
      // not only once the new one commits.
      if (tool === 'select') this.selRect = null;
      this.anchor = { x: pos.x, y: pos.y };
      this.previewEnd = { x: pos.x, y: pos.y };
    }
    else if (act === 'preview') this.previewEnd = { x: pos.x, y: pos.y };
    else if (act === 'commit') { this.previewEnd = { x: pos.x, y: pos.y }; this.commitShape(); }
    else if (tool === 'select' && ev.release && atAnchor) {
      // A plain click (release on the press cell, no drag): deselect and
      // just place the cursor — Select doesn't do click-move-click.
      this.anchor = null;
      this.previewEnd = null;
      this.selRect = null;
    }
  }

  /** The glyph a shape fill paints: color fill = colored space, char fill = ink. */
  private fillGlyph(fill: ShapeFill): number {
    return fill === 'color' ? 0x20 : this.glyph;
  }

  /**
   * Cells a two-point tool would paint from `a` to `b` (glyph included; the
   * committing code applies the current color). Fill cells come first so the
   * border, painted after, wins on overlap.
   */
  private shapeCells(a: { x: number; y: number }, b: { x: number; y: number }): PlotCell[] {
    var out: PlotCell[] = [];
    var i = 0;
    if (this.drawTool === 'line') {
      if (this.lineMode === 'half') return halfBlockLineCells(a.x, a.y, b.x, b.y);
      var pts = linePoints(a.x, a.y, b.x, b.y);
      for (i = 0; i < pts.length; i++) out.push({ x: pts[i]!.x, y: pts[i]!.y, ch: this.glyph });
      return out;
    }
    if (this.drawTool === 'box') {
      if (this.boxFill !== 'none') {
        var fx0 = Math.min(a.x, b.x) + 1;
        var fx1 = Math.max(a.x, b.x) - 1;
        var fy0 = Math.min(a.y, b.y) + 1;
        var fy1 = Math.max(a.y, b.y) - 1;
        var fg = this.fillGlyph(this.boxFill);
        for (var by = fy0; by <= fy1; by++) {
          for (var bx = fx0; bx <= fx1; bx++) out.push({ x: bx, y: by, ch: fg });
        }
      }
      var border = this.boxStyle === 'half'
        ? halfBlockBoxCells(a.x, a.y, b.x, b.y)
        : boxCells(a.x, a.y, b.x, b.y, this.boxStyle === 'double');
      for (i = 0; i < border.length; i++) {
        var bc = border[i] as PlotCell;
        out.push({ x: bc.x, y: bc.y, ch: this.boxStyle === 'char' ? this.glyph : bc.ch });
      }
      return out;
    }
    if (this.drawTool === 'circle') {
      if (this.circleFill !== 'none') {
        var fp = ellipseFillPoints(a.x, a.y, b.x, b.y);
        var cg = this.fillGlyph(this.circleFill);
        for (i = 0; i < fp.length; i++) out.push({ x: fp[i]!.x, y: fp[i]!.y, ch: cg });
      }
      var ep = ellipsePoints(a.x, a.y, b.x, b.y);
      for (i = 0; i < ep.length; i++) out.push({ x: ep[i]!.x, y: ep[i]!.y, ch: this.glyph });
      return out;
    }
    return out;
  }

  private commitShape(): void {
    if (this.anchor === null || this.previewEnd === null) return;
    if (this.drawTool === 'select') {
      this.selRect = normRect(this.anchor, this.previewEnd);
      this.anchor = null;
      this.previewEnd = null;
      return;
    }
    var cells = this.shapeCells(this.anchor, this.previewEnd);
    var out: { x: number; y: number; ch: number; attr: number }[] = [];
    for (var i = 0; i < cells.length; i++) {
      out.push({ x: cells[i]!.x, y: cells[i]!.y, ch: cells[i]!.ch, attr: this.doc.curAttr });
    }
    this.doc.paintCells(out);
    this.anchor = null;
    this.previewEnd = null;
  }

  // ------------------------------------------------------------------
  // Clipboard: art (box select) and text (text-mode select)
  // ------------------------------------------------------------------

  /** Copy (or cut) the art cells in the current draw selection. */
  private copyArtSelection(cut: boolean): void {
    if (this.selRect === null) return;
    var r = this.selRect;
    var cells: { dx: number; dy: number; ch: number; attr: number }[] = [];
    var erase: { x: number; y: number; ch: number; attr: number }[] = [];
    for (var y = r.y0; y <= r.y1; y++) {
      for (var x = r.x0; x <= r.x1; x++) {
        var a = this.doc.artAt(x, y);
        if (a !== null) {
          cells.push({ dx: x - r.x0, dy: y - r.y0, ch: a.ch, attr: a.attr });
          if (cut) erase.push({ x: x, y: y, ch: -1, attr: 0 });
        }
      }
    }
    this.clipArt = { w: r.x1 - r.x0 + 1, h: r.y1 - r.y0 + 1, cells: cells };
    if (cut && erase.length > 0) this.doc.paintCells(erase);
  }

  /** Ordered text selection range in the active flow, or null if empty. */
  private textSelectionRange(): { r0: number; c0: number; r1: number; c1: number } | null {
    if (this.textAnchor === null) return null;
    var a = this.textAnchor;
    var b = this.doc.caret;
    if (a.row === b.row && a.col === b.col) return null;
    if (a.row < b.row || (a.row === b.row && a.col < b.col)) return { r0: a.row, c0: a.col, r1: b.row, c1: b.col };
    return { r0: b.row, c0: b.col, r1: a.row, c1: a.col };
  }

  private copyTextSelection(cut: boolean): void {
    var range = this.textSelectionRange();
    if (range === null) return;
    this.clipText = this.doc.getRangeText(range.r0, range.c0, range.r1, range.c1);
    if (cut) this.doc.deleteRange(range.r0, range.c0, range.r1, range.c1);
    this.textAnchor = null;
    this.desiredCol = this.doc.caret.col;
  }

  private pasteText(): void {
    if (this.clipText === null) return;
    if (this.mode !== 'text') this.mode = 'text';
    var range = this.textSelectionRange();
    if (range !== null) this.doc.deleteRange(range.r0, range.c0, range.r1, range.c1); // replace selection
    this.textAnchor = null;
    this.doc.insertString(this.clipText);
    this.desiredCol = this.doc.caret.col;
    this.rehighlightCode();
  }

  /** Move the caret within the active flow (drag-select; no flow switch). */
  private extendCaretTo(docX: number, docY: number): void {
    if (this.doc.region !== null) {
      this.moveCaretInBox(this.doc.region, docX, docY);
    } else {
      var row = clamp(docY, 0, this.doc.lines.length - 1);
      var l = this.doc.lines[row];
      this.doc.caret = { row: row, col: clamp(docX, 0, l === undefined ? 0 : l.text.length) };
    }
    this.desiredCol = this.doc.caret.col;
  }

  /** Highlight the current text selection over the canvas. */
  private drawTextSelection(): void {
    if (this.mode !== 'text') return;
    var range = this.textSelectionRange();
    if (range === null) return;
    var offX = this.doc.region ? this.doc.region.left : 0;
    var offY = this.doc.region ? this.doc.region.top : 0;
    var attr = makeAttr(7, 4, true); // bright white on red
    for (var row = range.r0; row <= range.r1; row++) {
      var line = this.doc.lines[row];
      if (line === undefined) continue;
      var cStart = row === range.r0 ? range.c0 : 0;
      var cEnd = row === range.r1 ? range.c1 : line.text.length;
      for (var col = cStart; col < cEnd; col++) {
        var dx = offX + col;
        var sy = this.canvasTop + (offY + row - this.topRow);
        if (sy < this.canvasTop || sy > this.canvasBottom) continue;
        if (dx < 0 || dx >= this.canvasW) continue;
        var ch = col < line.text.length ? line.text.charCodeAt(col) & 0xff : 0x20;
        this.scr.put(dx, sy, ch, attr);
      }
    }
  }

  /**
   * Paste the art clipboard AT THE CURSOR, immediately (one undo step).
   * No placement mode: pasted content is settled the moment you paste —
   * later clicks belong to whatever tool is active, they never re-place.
   * (The follow-the-cursor pendingStamp flow remains only for font stamps,
   * where choosing a landing spot is the whole point.)
   */
  private pasteArt(): void {
    if (this.clipArt === null) return;
    if (this.mode !== 'draw') this.mode = 'draw';
    var out: { x: number; y: number; ch: number; attr: number }[] = [];
    for (var i = 0; i < this.clipArt.cells.length; i++) {
      var c = this.clipArt.cells[i]!;
      out.push({ x: this.brush.x + c.dx, y: this.brush.y + c.dy, ch: c.ch, attr: c.attr });
    }
    this.doc.paintCells(out);
  }

  /**
   * Recolor the visible cell under the brush — art OR prose text: apply the
   * current color to the chosen channel(s), keeping the glyph. It repaints,
   * it never places a character.
   */
  private recolorAt(x: number, y: number): void {
    this.doc.recolorCell(x, y, this.doc.curAttr, this.recolorChannel);
  }

  private fillAt(x: number, y: number, erase: boolean): void {
    var self = this;
    var width = this.doc.width;
    var height = Math.max(this.doc.rowCount(), y + 1) + 1;
    var sample = function (sx: number, sy: number): string {
      var c = self.doc.cellAt(sx, sy);
      return c.ch + ':' + c.attr;
    };
    var pts = floodFill(x, y, width, height, sample);
    var cells: { x: number; y: number; ch: number; attr: number }[] = [];
    for (var i = 0; i < pts.length; i++) {
      var px = pts[i]!.x;
      var py = pts[i]!.y;
      if (erase) {
        cells.push({ x: px, y: py, ch: -1, attr: 0 });
        continue;
      }
      // Apply per the fill mode: 'both' stamps ink; 'char' keeps each cell's
      // color; 'color' keeps each cell's character (bare cells get a space).
      var existing = this.doc.artAt(px, py);
      var ch = this.fillMode === 'color' ? (existing !== null ? existing.ch : 0x20) : this.glyph;
      var attr = this.fillMode === 'char' ? (existing !== null ? existing.attr : this.doc.curAttr) : this.doc.curAttr;
      cells.push({ x: px, y: py, ch: ch, attr: attr });
    }
    this.doc.paintCells(cells);
  }

  /** Overlay the in-progress shape onto the screen buffer (view only). */
  private drawPreview(): void {
    if (this.mode !== 'draw' || this.anchor === null || this.previewEnd === null) return;
    if (this.drawTool === 'select') { this.highlightRect(normRect(this.anchor, this.previewEnd)); return; }
    var cells = this.shapeCells(this.anchor, this.previewEnd);
    for (var i = 0; i < cells.length; i++) {
      var c = cells[i] as PlotCell;
      var sy = this.canvasTop + (c.y - this.topRow);
      if (sy < this.canvasTop || sy > this.canvasBottom) continue;
      if (c.x < 0 || c.x >= this.canvasW) continue;
      this.scr.put(c.x, sy, c.ch, this.doc.curAttr);
    }
  }

  /** Draw the committed selection outline (marquee) over the canvas. */
  private drawSelection(): void {
    // Only while the Select tool is active: other tools must not show a
    // stale marquee (setTool also clears it, this is belt and braces).
    if (this.mode !== 'draw' || this.drawTool !== 'select' || this.selRect === null) return;
    this.highlightRect(this.selRect);
  }

  /**
   * Draw a dashed selection marquee: low-ASCII '-' and '|' on the edges (the
   * dashed look) with CP437 single-line box corners.
   */
  private highlightRect(r: { x0: number; y0: number; x1: number; y1: number }): void {
    var attr = makeAttr(7, 0, true); // bright white
    var self = this;
    var put = function (x: number, y: number, ch: number): void {
      if (x < 0 || x >= self.canvasW) return;
      var sy = self.canvasTop + (y - self.topRow);
      if (sy < self.canvasTop || sy > self.canvasBottom) return;
      self.scr.put(x, sy, ch, attr);
    };
    // top/bottom edges: '-' (0x2D); left/right edges: '|' (0x7C)
    for (var x = r.x0 + 1; x < r.x1; x++) { put(x, r.y0, 0x2d); put(x, r.y1, 0x2d); }
    for (var y = r.y0 + 1; y < r.y1; y++) { put(r.x0, y, 0x7c); put(r.x1, y, 0x7c); }
    // CP437 single-line corners
    put(r.x0, r.y0, 0xda); // top-left
    put(r.x1, r.y0, 0xbf); // top-right
    put(r.x0, r.y1, 0xc0); // bottom-left
    put(r.x1, r.y1, 0xd9); // bottom-right
  }

  private openMenu(): ControllerResult | null {
    var items: MenuItem[] = [
      { id: 'save', label: 'Save & post message', keyLabel: '^O' },
      { id: 'subject', label: 'Edit subject', keyLabel: '' }
    ];
    if (this.session.quoteLines.length > 0) {
      items.push({ id: 'quote', label: 'Quote original', keyLabel: '^R' });
    }
    items.push({ id: 'sep1', label: '', keyLabel: '', separator: true });
    if (this.mode === 'text') {
      items.push({ id: 'mode-draw', label: 'Switch to draw mode', keyLabel: '^D' });
    } else {
      items.push({ id: 'mode-text', label: 'Switch to text mode', keyLabel: '^T' });
    }
    items.push({ id: 'color', label: 'Colors...', keyLabel: '^L' });
    // Big-text entry point in BOTH modes (it switches to draw on open).
    if (this.fonts !== null) {
      items.push({ id: 'fontwp', label: 'Font word processor...', keyLabel: '' });
    }
    if (this.mode === 'draw') {
      items.push({ id: 'glyph', label: 'Character...', keyLabel: '^K' });
      if (this.fonts !== null) {
        items.push({ id: 'font', label: 'Font text (stamp)...', keyLabel: '' });
      }
      items.push({ id: 'sep-tools', label: '', keyLabel: '', separator: true });
      for (var ti = 0; ti < DRAW_TOOLS.length; ti++) {
        var tool = DRAW_TOOLS[ti] as DrawTool;
        var mark = tool === this.drawTool ? '\x07 ' : '  '; // bullet on the active tool
        items.push({ id: 'tool-' + tool, label: mark + DRAW_TOOL_LABEL[tool], keyLabel: ti === 0 ? 'Tab' : '' });
      }
      if (this.toolOptionGroups().length > 0) {
        items.push({ id: 'tool-opts', label: '  Tool options...', keyLabel: '' });
      }
    }
    if (this.mode === 'text' && this.doc.region === null) {
      items.push({ id: 'code-insert', label: 'Insert code block...', keyLabel: '' });
    }
    if (this.mode === 'text' && this.doc.region !== null) {
      items.push({ id: 'leave-box', label: 'Leave text box (full width)', keyLabel: '' });
    }
    items.push({ id: 'sep2', label: '', keyLabel: '', separator: true });
    items.push({ id: 'undo', label: 'Undo', keyLabel: '^Z' });
    items.push({ id: 'redo', label: 'Redo', keyLabel: '^Y' });
    // Clipboard: Draw = box region (Select tool), Text = highlighted string.
    var canCopy = this.mode === 'draw' ? this.selRect !== null : this.textAnchor !== null;
    var canPaste = this.mode === 'draw' ? this.clipArt !== null : this.clipText !== null;
    if (canCopy) {
      items.push({ id: 'copy', label: 'Copy', keyLabel: '^C' });
      items.push({ id: 'cut', label: 'Cut', keyLabel: '^X' });
    }
    if (canPaste) items.push({ id: 'paste', label: 'Paste', keyLabel: '^V' });
    items.push({ id: 'sep3', label: '', keyLabel: '', separator: true });
    items.push({ id: 'help', label: 'Help', keyLabel: '^G' });
    items.push({ id: 'abort', label: 'Abort message', keyLabel: '^A' });
    var id = dropdownMenu(this.scr, this.input, this.alive, 1, 2, items);
    if (id === null) return null;
    return this.action(id);
  }

  // ------------------------------------------------------------------
  // Code blocks (preformatted box regions with syntax highlighting)
  // ------------------------------------------------------------------

  /** Language picker for a new fence: '' = auto-detect, null = cancelled. */
  private pickFenceLang(): string | null {
    var items: MenuItem[] = [{ id: 'auto', label: '  Auto-detect', keyLabel: '' }];
    for (var i = 0; i < LANGS.length; i++) {
      var L = LANGS[i] as LangDef;
      items.push({ id: L.id, label: '  ' + L.name, keyLabel: '' });
    }
    var id = dropdownMenu(this.scr, this.input, this.alive, 4, 4, items);
    if (id === null) return null;
    return id === 'auto' ? '' : id;
  }

  /**
   * Insert a fenced code block at the caret: ```lang / empty line / ```.
   * Deliberately NOT a CP437 box: fences are plain text, so readers can
   * copy/paste the code cleanly; the dim fence lines act as the block's
   * horizontal dividers, and long lines wrap like any body text instead of
   * being width-capped.
   */
  private insertCodeBlock(langTag?: string): void {
    if (this.mode !== 'text') this.mode = 'text';
    if (this.doc.region !== null) this.doc.clearRegion();
    var tag = langTag === undefined ? this.pickFenceLang() : langTag;
    if (tag === null) return;
    var row = this.doc.caret.row;
    var mk = function (t: string): { text: string; attr: number[] } {
      var a: number[] = [];
      for (var i = 0; i < t.length; i++) a.push(HL_COMMENT);
      return { text: t, attr: a };
    };
    this.doc.insertLines([mk('```' + tag), mk(''), mk('```')]);
    this.doc.caret = { row: row + 1, col: 0 };
    this.desiredCol = 0;
    this.rehighlightCode();
  }

  /** Re-run syntax highlighting for the active flow: code box or body fences. */
  private rehighlightCode(): void {
    var r = this.doc.region;
    if (r === null) {
      this.rehighlightBody();
      return;
    }
    if (r.pre !== true) return;
    var def = langById(r.lang === undefined ? '' : r.lang);
    if (def === null) return;
    highlightLines(this.doc.lines, def);
  }

  // Body rows the fence pass colored last time, so stale highlighting is
  // reset when a fence is edited away.
  private fencedRows: number[] = [];

  /**
   * Markdown-style code fences in the message body: a line of ``` (with an
   * optional language tag, e.g. ```js) starts a block, the next ``` ends it.
   * Fence lines render dim; the block between highlights in the tagged
   * language, or auto-detects when the fence is bare. An unclosed fence
   * highlights to the end of the message (markdown convention).
   */
  private rehighlightBody(): void {
    if (this.doc.region !== null) return;
    var lines = this.doc.lines;
    var touched: number[] = [];
    var r = 0;
    while (r < lines.length) {
      var tag = fenceTag(lines[r]!.text);
      if (tag === null) { r++; continue; }
      // find the closing fence (or end of body)
      var end = lines.length;
      for (var e = r + 1; e < lines.length; e++) {
        if (fenceTag(lines[e]!.text) !== null) { end = e; break; }
      }
      var sample: string[] = [];
      for (var s = r + 1; s < end; s++) sample.push(lines[s]!.text);
      var def = resolveFenceLang(tag, sample);
      // fence line(s) dim; interior highlighted with carried state
      this.paintLineAttr(r, HL_COMMENT);
      touched.push(r);
      var st = initialHlState();
      for (var i = r + 1; i < end; i++) {
        lines[i]!.attr = highlightLine(lines[i]!.text, st, def);
        touched.push(i);
      }
      if (end < lines.length) {
        this.paintLineAttr(end, HL_COMMENT);
        touched.push(end);
      }
      r = end + 1;
    }
    // Reset rows that fell out of every fenced block (fence deleted/edited).
    for (var o = 0; o < this.fencedRows.length; o++) {
      var row = this.fencedRows[o] as number;
      var still = false;
      for (var t = 0; t < touched.length; t++) if (touched[t] === row) { still = true; break; }
      if (!still && row < lines.length) this.paintLineAttr(row, DEFAULT_ATTR);
    }
    this.fencedRows = touched;
  }

  private paintLineAttr(row: number, attr: number): void {
    var l = this.doc.lines[row];
    if (l === undefined) return;
    var attrs: number[] = [];
    for (var i = 0; i < l.text.length; i++) attrs.push(attr);
    l.attr = attrs;
  }

  /**
   * Leave the active box through its top or bottom edge (keyboard path —
   * boxes must never trap the caret). Places the caret in the body just
   * outside the box border.
   */
  private exitBox(below: boolean): void {
    var r = this.doc.region;
    if (r === null) return;
    this.doc.clearRegion();
    var row = below ? r.top + r.height + 1 : r.top - 2;
    row = clamp(row, 0, this.doc.lines.length - 1);
    var line = this.doc.lines[row];
    this.doc.caret = { row: row, col: clamp(this.desiredCol, 0, line === undefined ? 0 : line.text.length) };
    this.doc.breakUndoGroup();
  }

  private openQuotes(): void {
    // Quoting inserts prose, so it lands in the active TEXT flow at the caret.
    // Position the caret first (click into a box to quote inside it); if you're
    // in draw mode, switch to text so the quote has somewhere to go.
    if (this.mode !== 'text') this.mode = 'text';
    var picked = quotePicker(this.scr, this.input, this.alive, this.session.quoteLines);
    if (picked === null || picked.length === 0) return;

    // Choose a BBS quote convention for the pre-formatting. The quoted text
    // was written by the person we're REPLYING to — i.e. the recipient (To) of
    // this reply — not `from` (the composer, which is us).
    var author = this.session.meta.to;
    var styleChoice = messageBox(this.scr, this.input, this.alive, 'Quote style',
      ['How should the quoted text be formatted?'],
      [
        { id: 'gt', key: '>', label: 'Standard >' },
        { id: 'initials', key: 'I', label: author.length > 0 ? authorInitials(author) + '>' : 'Initials>' },
        { id: 'none', key: 'P', label: 'Plain' }
      ], 0);
    if (styleChoice === null) return;
    var style = styleChoice as QuoteStyle;
    var attribution = false;
    if (author.length > 0) {
      attribution = messageBox(this.scr, this.input, this.alive, 'Attribution',
        ['Add a "' + author + ' wrote:" header?'],
        [{ id: 'yes', key: 'Y', label: 'Yes' }, { id: 'no', key: 'N', label: 'No' }], 1) === 'yes';
    }

    var quoted = formatQuote(picked, author, style, attribution);
    var toInsert: { text: string; attr: number[] }[] = [];
    var prefix = quotePrefix(style, author);
    // Wrap to the active flow's width — the box interior when quoting in a box,
    // else the full message width.
    var wrapWidth = this.doc.region ? this.doc.region.width : this.doc.width;
    for (var i = 0; i < quoted.length; i++) {
      var q = quoted[i] as string;
      while (q.length > wrapWidth) {
        var brk = q.lastIndexOf(' ', wrapWidth - 1);
        if (brk <= prefix.length) brk = wrapWidth;
        toInsert.push(quoteLineObj(q.substring(0, brk)));
        q = prefix + q.substring(brk).replace(/^ +/, '');
      }
      toInsert.push(quoteLineObj(q));
    }
    this.doc.insertLines(toInsert);
  }

  /** Auto-detect the output format: art-heavy -> ANSI, else CTRL-A text. */
  private detectSaveMode(): 'ctrla' | 'ansi' {
    if (this.saveMode !== null) return this.saveMode;
    // Code blocks travel as text: the ANSI-art format would wreck readers'
    // copy/paste flow, so a fenced message never auto-selects it (the save
    // dialog's Format button can still force it).
    var body = (this.doc.flowList()[0] as { lines: { text: string }[] }).lines;
    for (var i = 0; i < body.length; i++) {
      if (fenceTag(body[i]!.text) !== null) return 'ctrla';
    }
    var art = this.doc.artCellCount();
    var prose = this.doc.proseCharCount();
    // Meaningful art that outweighs the prose reads as an ANSI-art post.
    return art >= 40 && art > prose ? 'ansi' : 'ctrla';
  }

  /** Add or remove a leading [ANSI] subject tag (idempotent). */
  private ansiTagSubject(subject: string, ansi: boolean): string {
    var m = /^\s*\[ANSI\]\s*/i.exec(subject);
    var base = m ? subject.substring((m[0] as string).length) : subject;
    return ansi ? '[ANSI] ' + base : base;
  }

  private trySave(): ControllerResult | null {
    var mode = this.detectSaveMode();
    var lines: string[] = [];
    if (this.session.meta.to.length > 0) lines.push('To:      ' + this.session.meta.to);
    lines.push('Subject: ' + (this.subject.length > 0 ? this.subject : '(none)'));
    lines.push('Format:  ' + (mode === 'ansi'
      ? 'ANSI art  (subject gets [ANSI]; needs an ANSI terminal)'
      : 'Colored text (Ctrl-A; degrades to monochrome cleanly)'));
    lines.push('');
    lines.push('Post this message?');
    var buttons: ModalButton[] = [
      { id: 'post', key: 'Enter', label: 'Post' },
      { id: 'format', key: 'F', label: mode === 'ansi' ? 'Use text' : 'Use ANSI' },
      { id: 'subject', key: 'S', label: 'Subject' },
      { id: 'back', key: 'Esc', label: 'Keep editing' }
    ];
    var choice = messageBox(this.scr, this.input, this.alive, 'Save message', lines, buttons, 0);
    if (choice === 'post') {
      var body = mode === 'ansi' ? this.doc.toAnsiBody() : this.doc.toMessageBody(true);
      return { action: 'save', bodyCp437: body, subject: this.ansiTagSubject(this.subject, mode === 'ansi') };
    }
    if (choice === 'format') {
      this.saveMode = mode === 'ansi' ? 'ctrla' : 'ansi';
      return this.trySave();
    }
    if (choice === 'subject') {
      this.action('subject');
      return this.trySave();
    }
    return null;
  }

  private tryAbort(): ControllerResult | null {
    if (!this.doc.dirty) {
      return { action: 'abort', bodyCp437: '', subject: this.subject };
    }
    var choice = messageBox(this.scr, this.input, this.alive, 'Abort message',
      ['Throw away this message?', 'Unsaved text and artwork will be lost.'],
      [
        { id: 'discard', key: 'D', label: 'Discard' },
        { id: 'back', key: 'Esc', label: 'Keep editing' }
      ], 1);
    if (choice === 'discard') return { action: 'abort', bodyCp437: '', subject: this.subject };
    return null;
  }

  // ------------------------------------------------------------------
  // Keyboard
  // ------------------------------------------------------------------

  private handleKey(k: string): ControllerResult | null {
    // The live word-processor session captures all keys until committed.
    if (this.wp !== null) return this.handleWpKey(k);
    // Placing a font block intercepts everything until committed or cancelled.
    if (this.pendingStamp !== null) return this.handleStampKey(k);
    // Global chrome keys first. Ctrl bindings follow common editor muscle
    // memory; F-keys remain as secondaries. ^C/^X/^V are deliberately left
    // unbound, reserved for the future copy/cut/paste.
    if (k === keys.KEY_ESC) return this.openMenu();
    // F1-F10 type from the active character set (the TheDraw/Moebius drawing
    // convention); F11/F12 — or Ctrl+,/. on terminals that can transmit them
    // (CSI-u/modifyOtherKeys) — cycle the sets, Ctrl+/ restores the default.
    var fk = fkeySlot(k);
    if (fk >= 0) { this.typeCharsetChar(fk); return null; }
    if (k === 'F11' || k === 'C-,') { this.cycleCharset(-1); return null; }
    if (k === 'F12' || k === 'C-.') { this.cycleCharset(1); return null; }
    if (k === 'C-/') { this.charsetIdx = DEFAULT_CHARSET; return null; }
    if (k === keys.CTRL_G) return this.action('help');
    // Save ^O (nano write-Out) and Abort ^A are the collision-free primaries;
    // ^S/^Q are muscle-memory secondaries that XON/XOFF may swallow. The
    // menu is the always-available fallback.
    if (k === keys.CTRL_O || k === keys.CTRL_S) return this.action('save');
    if (k === keys.CTRL_A || k === keys.CTRL_Q) return this.action('abort');
    if (k === keys.CTRL_R) return this.action('quote');
    // Separate keys for the two modes (not a toggle): ^T text, ^D draw.
    if (k === keys.CTRL_T) return this.action('mode-text');
    if (k === keys.CTRL_D) return this.action('mode-draw');
    if (k === keys.CTRL_L) return this.action('color');
    if (k === keys.CTRL_Z) return this.action('undo');
    if (k === keys.CTRL_Y) return this.action('redo');
    // Clipboard — mode-aware (Draw = box region, Text = string). ^V is also
    // the Insert-key byte, so paste supersedes the overwrite toggle.
    if (k === keys.CTRL_C) return this.action('copy');
    if (k === keys.CTRL_X) return this.action('cut');
    if (k === keys.KEY_INSERT) return this.action('paste'); // ^V and the Insert key share this byte
    if (this.mode === 'draw') {
      if (k === keys.CTRL_K) return this.action('glyph');
      if (k === keys.CTRL_W) return this.action('pick');
      return this.handleDrawKey(k);
    }
    return this.handleTextKey(k);
  }

  private handleTextKey(k: string): null {
    var doc = this.doc;
    // Any keyboard action clears a mouse selection (no shift-select in v1).
    this.textAnchor = null;
    if (k === keys.KEY_LEFT) {
      doc.moveLeft();
      this.desiredCol = doc.caret.col;
      doc.breakUndoGroup();
    } else if (k === keys.KEY_RIGHT) {
      doc.moveRight();
      this.desiredCol = doc.caret.col;
      doc.breakUndoGroup();
    } else if (k === keys.KEY_UP) {
      // Arrow off the top of a box exits it — boxes never trap the caret.
      if (doc.region !== null && doc.caret.row === 0) this.exitBox(false);
      else {
        doc.moveVert(-1, this.desiredCol);
        doc.breakUndoGroup();
      }
    } else if (k === keys.KEY_DOWN) {
      // Arrow off the bottom of a box's text exits it below the border.
      if (doc.region !== null && doc.caret.row >= doc.lines.length - 1) this.exitBox(true);
      else {
        doc.moveVert(1, this.desiredCol);
        doc.breakUndoGroup();
      }
    } else if (k === keys.KEY_PAGEUP) {
      doc.moveVert(-this.canvasRows(), this.desiredCol);
      doc.breakUndoGroup();
    } else if (k === keys.KEY_PAGEDN) {
      doc.moveVert(this.canvasRows(), this.desiredCol);
      doc.breakUndoGroup();
    } else if (k === keys.KEY_HOME) {
      doc.moveHome();
      this.desiredCol = 0;
    } else if (k === keys.KEY_END) {
      doc.moveEnd();
      this.desiredCol = doc.caret.col;
    } else if (k === keys.KEY_INSERT) {
      doc.insertMode = !doc.insertMode;
    } else if (k === keys.KEY_ENTER) {
      // ``` on its own line inside a code box closes the block: the fence
      // line is removed and the caret leaves the box (mirrors body fences).
      if (doc.region !== null && doc.region.pre === true && doc.curLineText() === '```') {
        doc.deleteRange(doc.caret.row, 0, doc.caret.row, 3);
        this.exitBox(true);
        return null;
      }
      doc.insertBreak();
      this.desiredCol = 0;
    } else if (k === keys.KEY_BACKSPACE || k === keys.KEY_DEL) {
      // Backspace. Terminals disagree on the byte the Backspace key sends
      // (many, incl. SyncTERM, send 0x7f == KEY_DEL), so both delete the
      // character before the caret. Forward-delete is intentionally not bound.
      doc.backspace();
      this.desiredCol = doc.caret.col;
    } else if (k === keys.KEY_TAB) {
      var n = TAB_STOP - (doc.caret.col % TAB_STOP);
      for (var i = 0; i < n; i++) doc.insertChar(0x20);
      this.desiredCol = doc.caret.col;
    } else if (keys.isPrintable(k)) {
      doc.insertChar(k.charCodeAt(0));
      this.desiredCol = doc.caret.col;
    }
    // Live re-highlight while editing inside a code block (no-op otherwise).
    this.rehighlightCode();
    return null;
  }

  private handleDrawKey(k: string): null {
    if (k === keys.KEY_TAB) { this.cycleTool(); return null; }
    // Keyboard access to tool sub-options: Shift+Tab opens the options menu
    // (Tab picks the tool, Shift+Tab configures it).
    if (k === 'STAB') { this.openToolOptionsMenu(); return null; }
    if (this.drawTool === 'type') return this.handleTypeKey(k);
    if (this.drawTool === 'recolor') return this.handleRecolorKey(k);
    var maxY = Math.max(this.doc.rowCount() + this.canvasRows(), this.brush.y + 1);
    var twoPoint = this.drawTool === 'line' || this.drawTool === 'box' || this.drawTool === 'circle' || this.drawTool === 'select';
    if (k === keys.KEY_LEFT) this.brush.x = clamp(this.brush.x - 1, 0, this.doc.width - 1);
    else if (k === keys.KEY_RIGHT) this.brush.x = clamp(this.brush.x + 1, 0, this.doc.width - 1);
    else if (k === keys.KEY_UP) this.brush.y = clamp(this.brush.y - 1, 0, maxY);
    else if (k === keys.KEY_DOWN) this.brush.y = clamp(this.brush.y + 1, 0, maxY);
    else if (k === keys.KEY_PAGEUP) this.brush.y = clamp(this.brush.y - this.canvasRows(), 0, maxY);
    else if (k === keys.KEY_PAGEDN) this.brush.y = clamp(this.brush.y + this.canvasRows(), 0, maxY);
    else if (k === keys.KEY_HOME) this.brush.x = 0;
    else if (k === keys.KEY_END) this.brush.x = this.doc.width - 1;
    else if (k === ' ' || k === keys.KEY_ENTER) {
      if (this.drawTool === 'pencil') {
        this.paint();
        this.brush.x = clamp(this.brush.x + 1, 0, this.doc.width - 1);
      } else if (this.drawTool === 'fill') {
        this.fillAt(this.brush.x, this.brush.y, false);
      } else if (this.anchor === null) {
        // first press sets the anchor; move the brush and press again to commit
        if (this.drawTool === 'select') this.selRect = null; // old marquee goes now
        this.anchor = { x: this.brush.x, y: this.brush.y };
        this.previewEnd = { x: this.brush.x, y: this.brush.y };
      } else {
        this.previewEnd = { x: this.brush.x, y: this.brush.y };
        this.commitShape();
      }
    } else if (k === keys.KEY_DEL || k === keys.KEY_BACKSPACE) {
      if (this.anchor !== null) { this.anchor = null; this.previewEnd = null; } // cancel pending shape
      else {
        if (k === keys.KEY_BACKSPACE) this.brush.x = clamp(this.brush.x - 1, 0, this.doc.width - 1);
        this.doc.eraseArt(this.brush.x, this.brush.y);
      }
    } else if (this.drawTool === 'pencil' && keys.isPrintable(k)) {
      // stamp the typed character as fixed art (pencil only)
      this.doc.setArt(this.brush.x, this.brush.y, { ch: k.charCodeAt(0), attr: this.doc.curAttr });
      this.brush.x = clamp(this.brush.x + 1, 0, this.doc.width - 1);
    }
    // keep the preview tracking the brush while a shape is being placed
    if (twoPoint && this.anchor !== null) this.previewEnd = { x: this.brush.x, y: this.brush.y };
    return null;
  }

  /**
   * Open the TheDraw font picker, render the chosen text, and enter placement:
   * the big-text block previews at the brush and commits on click/Enter.
   */
  private openFontPicker(): void {
    if (this.fonts === null) return;
    if (this.mode !== 'draw') this.mode = 'draw';
    var choice = fontPicker(this.scr, this.input, this.alive, this.fonts, '');
    this.scr.invalidate();
    if (choice === null) return;
    var font = this.fonts.load(choice.fontName);
    if (font === null) return;
    var render = renderTdf(font, choice.text);
    var isColor = font.fonttype === COLOR_FONT;
    var cells: { x: number; y: number; ch: number; attr: number }[] = [];
    for (var yy = 0; yy < render.rows.length; yy++) {
      var row = render.rows[yy] as { ch: number; color: number }[];
      for (var xx = 0; xx < row.length; xx++) {
        var c = row[xx] as { ch: number; color: number };
        // skip transparent gaps: blank cell with no background
        if (c.ch === 0x20 && !(isColor && (c.color & 0x70))) continue;
        cells.push({ x: xx, y: yy, ch: c.ch, attr: isColor ? (c.color & 0xff) : this.doc.curAttr });
      }
    }
    if (cells.length === 0) return;
    this.pendingStamp = cells;
    this.pendingW = render.width;
    this.pendingH = render.height;
  }

  // ------------------------------------------------------------------
  // TheDraw word-processor mode
  // ------------------------------------------------------------------

  /** Pick a font and start a live word-processor session at the brush. */
  private openFontWordProcessor(): void {
    if (this.fonts === null) return;
    if (this.mode !== 'draw') this.mode = 'draw';
    var choice = fontPicker(this.scr, this.input, this.alive, this.fonts, '');
    this.scr.invalidate();
    if (choice === null) return;
    var font = this.fonts.load(choice.fontName);
    if (font === null) return;
    // Start empty: the origin is anchored here once; you type and the block
    // grows from it. (The picker text is only for choosing/previewing a font.)
    this.wp = { fonts: [], curFont: font, text: '', caret: 0, originX: this.brush.x, originY: this.brush.y, gap: 0 };
  }

  private wpMaxWidth(): number {
    if (this.wp === null) return this.doc.width;
    var w = this.canvasW - this.wp.originX;
    return w < 8 ? 8 : w;
  }

  private wpLayout() {
    var wp = this.wp as { fonts: TdfFont[]; curFont: TdfFont; text: string; gap: number };
    return layoutTdfWpStyled(wp.text, wp.fonts, this.wpMaxWidth(), wp.gap, wp.curFont);
  }

  /**
   * Adopt the font of the character left of the caret. Called after caret
   * movement (arrows/click): like a regular word processor, moving the caret
   * resets the pending typing font to the surrounding text's; an explicit
   * ^K switch afterwards styles only what is typed next.
   */
  private wpSyncFont(): void {
    var wp = this.wp;
    if (wp === null) return;
    var f = wp.caret > 0 ? wp.fonts[wp.caret - 1] : wp.fonts[0];
    if (f !== undefined) wp.curFont = f;
  }

  /** Build the art cells for the current WP text (transparency-filtered). */
  private wpCells(): { x: number; y: number; ch: number; attr: number }[] {
    var wp = this.wp as { originX: number; originY: number };
    var lay = this.wpLayout();
    var out: { x: number; y: number; ch: number; attr: number }[] = [];
    for (var li = 0; li < lay.lines.length; li++) {
      var line = lay.lines[li]!;
      for (var ry = 0; ry < line.render.rows.length; ry++) {
        var row = line.render.rows[ry]!;
        for (var rx = 0; rx < row.length; rx++) {
          var c = row[rx]!;
          // cf marks cells from color fonts (per cell, since fonts can mix)
          var isColor = c.cf === true;
          if (c.ch === 0x20 && !(isColor && (c.color & 0x70))) continue;
          out.push({ x: wp.originX + rx, y: wp.originY + line.yTop + ry, ch: c.ch, attr: isColor ? (c.color & 0xff) : this.doc.curAttr });
        }
      }
    }
    return out;
  }

  private drawWp(): void {
    if (this.wp === null) return;
    var cells = this.wpCells();
    for (var i = 0; i < cells.length; i++) {
      var c = cells[i] as { x: number; y: number; ch: number; attr: number };
      var sy = this.canvasTop + (c.y - this.topRow);
      if (sy < this.canvasTop || sy > this.canvasBottom) continue;
      if (c.x < 0 || c.x >= this.canvasW) continue;
      this.scr.put(c.x, sy, c.ch, c.attr);
    }
    // Pseudo-cursor: a blinking bar the full height of the caret's display
    // line — a single-cell hardware caret would misrepresent a tall font.
    var wp = this.wp;
    var lay = this.wpLayout();
    var pos = tdfWpCaretXY(lay, wp.caret);
    var lineH = lay.lines[pos.line]!.render.height;
    var cx = wp.originX + pos.x;
    var caretAttr = makeAttr(7, 0, true, true); // bright white, blinking
    for (var cr = 0; cr < lineH; cr++) {
      var csy = this.canvasTop + (wp.originY + pos.y + cr - this.topRow);
      if (csy < this.canvasTop || csy > this.canvasBottom) continue;
      if (cx < 0 || cx >= this.canvasW) continue;
      this.scr.put(cx, csy, 0xdd, caretAttr); // left half-block as an I-bar
    }
    this.scr.cursorVisible = false; // the pseudo-cursor stands in for it
  }

  private commitWp(): void {
    if (this.wp === null) return;
    var cells = this.wpCells();
    if (cells.length > 0) this.doc.paintCells(cells);
    this.wp = null;
  }

  /** Logical index of the start of the paragraph containing `caret`. */
  private wpLineStart(): number {
    var wp = this.wp as { text: string; caret: number };
    var i = wp.text.lastIndexOf('\n', wp.caret - 1);
    return i + 1;
  }

  private wpLineEnd(): number {
    var wp = this.wp as { text: string; caret: number };
    var i = wp.text.indexOf('\n', wp.caret);
    return i === -1 ? wp.text.length : i;
  }

  /** Insert `s` at the WP caret, styled with the current typing font. */
  private wpInsert(s: string): void {
    var wp = this.wp;
    if (wp === null) return;
    wp.text = wp.text.substring(0, wp.caret) + s + wp.text.substring(wp.caret);
    for (var i = 0; i < s.length; i++) wp.fonts.splice(wp.caret + i, 0, wp.curFont);
    wp.caret += s.length;
  }

  /** Pick a new font for subsequent WP typing (existing text keeps its own). */
  private wpPickFont(): void {
    if (this.fonts === null || this.wp === null) return;
    var choice = fontPicker(this.scr, this.input, this.alive, this.fonts, '');
    this.scr.invalidate();
    if (choice === null) return;
    var font = this.fonts.load(choice.fontName);
    if (font !== null) this.wp.curFont = font;
  }

  private handleWpKey(k: string): ControllerResult | null {
    var wp = this.wp as { fonts: TdfFont[]; curFont: TdfFont; text: string; caret: number; originX: number; originY: number; gap: number };
    if (k === keys.KEY_ESC) return this.finishWp();
    if (k === keys.CTRL_K) { this.wpPickFont(); }
    else if (k === keys.KEY_ENTER) { this.wpInsert('\n'); }
    else if (k === keys.KEY_TAB) { this.wpInsert('  '); }
    else if (k === keys.KEY_BACKSPACE || k === keys.KEY_DEL) {
      // Backspace. Terminals disagree on the byte (many send 0x7f == KEY_DEL),
      // so both mean "delete the character before the caret".
      if (wp.caret > 0) {
        wp.text = wp.text.substring(0, wp.caret - 1) + wp.text.substring(wp.caret);
        wp.fonts.splice(wp.caret - 1, 1);
        wp.caret--;
        this.wpSyncFont();
      }
    } else if (k === keys.KEY_LEFT) { if (wp.caret > 0) wp.caret--; this.wpSyncFont(); }
    else if (k === keys.KEY_RIGHT) { if (wp.caret < wp.text.length) wp.caret++; this.wpSyncFont(); }
    else if (k === keys.KEY_HOME) { wp.caret = this.wpLineStart(); this.wpSyncFont(); }
    else if (k === keys.KEY_END) { wp.caret = this.wpLineEnd(); this.wpSyncFont(); }
    else if (k === keys.KEY_UP) { this.wpMoveVert(-1); this.wpSyncFont(); }
    else if (k === keys.KEY_DOWN) { this.wpMoveVert(1); this.wpSyncFont(); }
    else if (keys.isPrintable(k)) {
      this.wpInsert(k);
    }
    return null;
  }

  /** Move the caret to a neighbouring display line, keeping the column offset. */
  private wpMoveVert(delta: number): void {
    var wp = this.wp as { caret: number };
    var lay = this.wpLayout();
    var here = tdfWpCaretXY(lay, wp.caret);
    var target = here.line + delta;
    if (target < 0 || target >= lay.lines.length) return;
    var curLine = lay.lines[here.line]!;
    var offset = wp.caret - curLine.startIdx;
    var tgt = lay.lines[target]!;
    wp.caret = tgt.startIdx + Math.min(offset, tgt.text.length);
  }

  private finishWp(): ControllerResult | null {
    var choice = messageBox(this.scr, this.input, this.alive, 'Font text',
      ['Stamp this text onto the canvas?'],
      [
        { id: 'stamp', key: 'Enter', label: 'Stamp it' },
        { id: 'keep', key: 'K', label: 'Keep editing' },
        { id: 'discard', key: 'D', label: 'Discard' }
      ], 0);
    if (choice === 'stamp') this.commitWp();
    else if (choice === 'discard') this.wp = null;
    return null;
  }

  private drawPendingStamp(): void {
    if (this.pendingStamp === null) return;
    for (var i = 0; i < this.pendingStamp.length; i++) {
      var c = this.pendingStamp[i] as { x: number; y: number; ch: number; attr: number };
      var dx = this.brush.x + c.x;
      var dy = this.brush.y + c.y;
      var sy = this.canvasTop + (dy - this.topRow);
      if (sy < this.canvasTop || sy > this.canvasBottom) continue;
      if (dx < 0 || dx >= this.canvasW) continue;
      this.scr.put(dx, sy, c.ch, c.attr);
    }
  }

  private commitStamp(): void {
    if (this.pendingStamp === null) return;
    var out: { x: number; y: number; ch: number; attr: number }[] = [];
    for (var i = 0; i < this.pendingStamp.length; i++) {
      var c = this.pendingStamp[i] as { x: number; y: number; ch: number; attr: number };
      out.push({ x: this.brush.x + c.x, y: this.brush.y + c.y, ch: c.ch, attr: c.attr });
    }
    this.doc.paintCells(out);
    this.pendingStamp = null;
  }

  /** Placement mode input (a font block is following the cursor). */
  private handleStampKey(k: string): null {
    var maxY = Math.max(this.doc.rowCount() + this.canvasRows(), this.brush.y + 1);
    if (k === keys.KEY_ESC) this.pendingStamp = null;
    else if (k === keys.KEY_ENTER || k === ' ') this.commitStamp();
    else if (k === keys.KEY_LEFT) this.brush.x = clamp(this.brush.x - 1, 0, this.doc.width - 1);
    else if (k === keys.KEY_RIGHT) this.brush.x = clamp(this.brush.x + 1, 0, this.doc.width - 1);
    else if (k === keys.KEY_UP) this.brush.y = clamp(this.brush.y - 1, 0, maxY);
    else if (k === keys.KEY_DOWN) this.brush.y = clamp(this.brush.y + 1, 0, maxY);
    else if (k === keys.KEY_HOME) this.brush.x = 0;
    return null;
  }

  /** Recolor tool keys: arrows move, 1/2/3 pick the channel, Space repaints. */
  private handleRecolorKey(k: string): null {
    var maxY = Math.max(this.doc.rowCount() + this.canvasRows(), this.brush.y + 1);
    if (k === keys.KEY_LEFT) this.brush.x = clamp(this.brush.x - 1, 0, this.doc.width - 1);
    else if (k === keys.KEY_RIGHT) this.brush.x = clamp(this.brush.x + 1, 0, this.doc.width - 1);
    else if (k === keys.KEY_UP) this.brush.y = clamp(this.brush.y - 1, 0, maxY);
    else if (k === keys.KEY_DOWN) this.brush.y = clamp(this.brush.y + 1, 0, maxY);
    else if (k === keys.KEY_PAGEUP) this.brush.y = clamp(this.brush.y - this.canvasRows(), 0, maxY);
    else if (k === keys.KEY_PAGEDN) this.brush.y = clamp(this.brush.y + this.canvasRows(), 0, maxY);
    else if (k === keys.KEY_HOME) this.brush.x = 0;
    else if (k === keys.KEY_END) this.brush.x = this.doc.width - 1;
    else if (k === '1') this.recolorChannel = 'fg';
    else if (k === '2') this.recolorChannel = 'bg';
    else if (k === '3') this.recolorChannel = 'both';
    else if (k === ' ') this.recolorAt(this.brush.x, this.brush.y);
    else if (k === keys.KEY_DEL) this.doc.eraseArt(this.brush.x, this.brush.y);
    return null;
  }

  /**
   * The Type tool: free-form positional text placed as fixed art cells with
   * typewriter behavior. No wrap, no reflow — every character is pinned where
   * you put it. Enter carriage-returns to the column typing started at;
   * Backspace retreats and erases. This is the text-artist path, distinct from
   * the reflowing word-processor flows of Text mode.
   */
  private handleTypeKey(k: string): null {
    var maxY = Math.max(this.doc.rowCount() + this.canvasRows(), this.brush.y + 1);
    if (k === keys.KEY_LEFT) { this.brush.x = clamp(this.brush.x - 1, 0, this.doc.width - 1); this.textOrigin = this.brush.x; }
    else if (k === keys.KEY_RIGHT) { this.brush.x = clamp(this.brush.x + 1, 0, this.doc.width - 1); this.textOrigin = this.brush.x; }
    else if (k === keys.KEY_UP) this.brush.y = clamp(this.brush.y - 1, 0, maxY);
    else if (k === keys.KEY_DOWN) this.brush.y = clamp(this.brush.y + 1, 0, maxY);
    else if (k === keys.KEY_PAGEUP) this.brush.y = clamp(this.brush.y - this.canvasRows(), 0, maxY);
    else if (k === keys.KEY_PAGEDN) this.brush.y = clamp(this.brush.y + this.canvasRows(), 0, maxY);
    else if (k === keys.KEY_HOME) { this.brush.x = 0; this.textOrigin = 0; }
    else if (k === keys.KEY_END) { this.brush.x = this.doc.width - 1; this.textOrigin = this.brush.x; }
    else if (k === keys.KEY_ENTER) {
      // carriage return + line feed to the column we started at
      this.brush.x = clamp(this.textOrigin, 0, this.doc.width - 1);
      this.brush.y = clamp(this.brush.y + 1, 0, maxY);
    } else if (k === keys.KEY_BACKSPACE || k === keys.KEY_DEL) {
      // Backspace (either byte): step back a cell and erase it.
      this.brush.x = clamp(this.brush.x - 1, 0, this.doc.width - 1);
      this.doc.eraseArt(this.brush.x, this.brush.y);
    } else if (keys.isPrintable(k)) {
      // space is a real (masking) cell here, so colored backgrounds work
      this.doc.setArt(this.brush.x, this.brush.y, { ch: k.charCodeAt(0), attr: this.doc.curAttr });
      this.brush.x = clamp(this.brush.x + 1, 0, this.doc.width - 1);
    }
    return null;
  }
}

/** 'F1'..'F10' -> slot 0..9; anything else -> -1. */
function fkeySlot(k: string): number {
  if (k.length < 2 || k.length > 3 || k.charAt(0) !== 'F') return -1;
  var n = parseInt(k.substring(1), 10);
  return n >= 1 && n <= 10 ? n - 1 : -1;
}

function normRect(a: { x: number; y: number }, b: { x: number; y: number }): { x0: number; y0: number; x1: number; y1: number } {
  return { x0: Math.min(a.x, b.x), y0: Math.min(a.y, b.y), x1: Math.max(a.x, b.x), y1: Math.max(a.y, b.y) };
}

function quoteLineObj(text: string): { text: string; attr: number[] } {
  var attr: number[] = [];
  for (var i = 0; i < text.length; i++) attr.push(makeAttr(2, 0, false));
  return { text: text, attr: attr };
}
