/**
 * Modal dialogs: message box, dropdown menu, glyph picker, color picker,
 * quote picker, subject prompt, help overlay.
 *
 * Every modal follows the same GUI rules as the main chrome: visible buttons
 * with their key equivalents printed on them, full mouse support via hit
 * regions, and complete keyboard-only operation. A modal draws over the
 * already-flushed frame and runs its own read loop; when it returns, the
 * controller's next compose naturally repaints the canvas beneath.
 */

import { Screen } from './screen';
import { HitMap, drawButton, drawFrame, fillBox } from './widgets';
import { theme } from './theme';
import { makeAttr, HIGH, BLINK } from '../core/attr';
import { InputEvent } from '../host/types';
import * as keys from './keys';

export type InputFn = (timeoutMs: number) => InputEvent;
export type AliveFn = () => boolean;

export interface ModalButton {
  id: string;
  key: string;
  label: string;
}

function center(w: number, total: number): number {
  var x = Math.floor((total - w) / 2);
  return x < 0 ? 0 : x;
}

/**
 * Message box with buttons. Returns the chosen button id, or null on ESC /
 * disconnect.
 */
export function messageBox(
  scr: Screen,
  input: InputFn,
  alive: AliveFn,
  title: string,
  lines: string[],
  buttons: ModalButton[],
  defaultIdx?: number
): string | null {
  var maxLine = 0;
  for (var i = 0; i < lines.length; i++) if ((lines[i] as string).length > maxLine) maxLine = (lines[i] as string).length;
  var btnW = 0;
  for (var b = 0; b < buttons.length; b++) {
    var bb = buttons[b] as ModalButton;
    btnW += bb.key.length + bb.label.length + 4;
  }
  var w = Math.max(maxLine + 6, btnW + 4, title.length + 6);
  if (w > scr.cols - 2) w = scr.cols - 2;
  var h = lines.length + 6;
  var x = center(w, scr.cols);
  var y = center(h, scr.rows);
  var sel = defaultIdx === undefined ? 0 : defaultIdx;
  var hits = new HitMap();

  while (alive()) {
    hits.clear();
    fillBox(scr, x, y, w, h, theme.modalBody);
    drawFrame(scr, x, y, w, h, theme.modalFrame, true, title, theme.modalTitle);
    for (var li = 0; li < lines.length; li++) {
      scr.putStr(x + 3, y + 2 + li, (lines[li] as string).substring(0, w - 6), theme.modalBody);
    }
    var bx = x + center(btnW, w);
    var by = y + h - 3;
    for (var bi = 0; bi < buttons.length; bi++) {
      var btn = buttons[bi] as ModalButton;
      bx += drawButton(scr, hits, bx, by, btn.id, btn.key, btn.label, bi === sel) + 1;
    }
    scr.cursorVisible = false;
    scr.flush();

    var ev = input(30000);
    if (ev.type === 'mouse') {
      if (ev.press && ev.button === 0) {
        var id = hits.test(ev.x - 1, ev.y - 1);
        if (id !== null) return id;
      }
      continue;
    }
    if (ev.type !== 'key') continue;
    var k = ev.key;
    if (k === keys.KEY_ESC) return null;
    if (k === keys.KEY_ENTER) return (buttons[sel] as ModalButton).id;
    if (k === keys.KEY_LEFT || (k === keys.KEY_TAB && false)) sel = (sel + buttons.length - 1) % buttons.length;
    else if (k === keys.KEY_RIGHT || k === keys.KEY_TAB) sel = (sel + 1) % buttons.length;
    else {
      for (var s = 0; s < buttons.length; s++) {
        var cand = buttons[s] as ModalButton;
        if (cand.key.length === 1 && k.toUpperCase() === cand.key.toUpperCase()) return cand.id;
        if (cand.key.length > 1 && k === cand.key) return cand.id; // F-keys
      }
    }
  }
  return null;
}

export interface MenuItem {
  id: string;
  label: string;
  /** Displayed right-aligned, e.g. 'F2' or '^Z'. */
  keyLabel: string;
  separator?: boolean;
}

/** Dropdown menu anchored at (x, y) 0-based. Returns item id or null. */
export function dropdownMenu(
  scr: Screen,
  input: InputFn,
  alive: AliveFn,
  x: number,
  y: number,
  items: MenuItem[]
): string | null {
  var w = 0;
  for (var i = 0; i < items.length; i++) {
    var it = items[i] as MenuItem;
    var lw = it.label.length + it.keyLabel.length + 6;
    if (lw > w) w = lw;
  }
  var h = items.length + 2;
  if (x + w > scr.cols) x = scr.cols - w;
  if (y + h > scr.rows) y = scr.rows - h;
  var sel = 0;
  while ((items[sel] as MenuItem).separator) sel++;
  var hits = new HitMap();

  while (alive()) {
    hits.clear();
    fillBox(scr, x, y, w, h, theme.modalBody);
    drawFrame(scr, x, y, w, h, theme.modalFrame, false);
    for (var li = 0; li < items.length; li++) {
      var item = items[li] as MenuItem;
      var iy = y + 1 + li;
      if (item.separator) {
        scr.hline(x + 1, iy, w - 2, 0xc4, theme.modalFrame);
        continue;
      }
      var attr = li === sel ? theme.modalSel : theme.modalBody;
      scr.fill(x + 1, iy, w - 2, 1, 0x20, attr);
      scr.putStr(x + 2, iy, item.label, attr);
      scr.putStr(x + w - 2 - item.keyLabel.length, iy, item.keyLabel, li === sel ? theme.modalSel : theme.modalTitle);
      hits.add(item.id, x + 1, iy, x + w - 2, iy);
    }
    scr.cursorVisible = false;
    scr.flush();

    var ev = input(30000);
    if (ev.type === 'mouse') {
      // Wheel is deliberately IGNORED here: trackpads emit incidental
      // scroll deltas while the pointer moves, which made the lightbar
      // jump around unrelated to the pointer (SyncTerm + trackpad).
      // Hover, arrows, and clicks are the menu's navigation.
      if (ev.wheel !== 0) continue;
      var overId = hits.test(ev.x - 1, ev.y - 1);
      // Hover: the lightbar follows the pointer (motion/release events).
      // SGR drag reports carry press===true, so a real click must be
      // press && !motion — otherwise the first drag tick of a click-hold
      // (common in SyncTerm) instantly activates whatever it grazes.
      if (ev.motion || ev.release) {
        if (overId !== null) {
          for (var hv = 0; hv < items.length; hv++) {
            if ((items[hv] as MenuItem).id === overId && !(items[hv] as MenuItem).separator) { sel = hv; break; }
          }
        }
        continue;
      }
      if (ev.press && ev.button === 0) {
        if (overId !== null) return overId;
        return null; // click outside closes the menu
      }
      continue;
    }
    if (ev.type !== 'key') continue;
    var k = ev.key;
    if (k === keys.KEY_ESC) return null;
    if (k === keys.KEY_ENTER) return (items[sel] as MenuItem).id;
    if (k === keys.KEY_UP) sel = moveSel(items, sel, -1);
    else if (k === keys.KEY_DOWN) sel = moveSel(items, sel, 1);
    else {
      // hotkey: first letter of the label
      for (var hk = 0; hk < items.length; hk++) {
        var cand = items[hk] as MenuItem;
        if (!cand.separator && cand.label.charAt(0).toUpperCase() === k.toUpperCase()) return cand.id;
      }
    }
  }
  return null;
}

function moveSel(items: MenuItem[], sel: number, dir: number): number {
  var n = items.length;
  var s = sel;
  for (var i = 0; i < n; i++) {
    s = (s + dir + n) % n;
    if (!(items[s] as MenuItem).separator) return s;
  }
  return sel;
}

/** 16x14 grid of CP437 glyphs 32..255. Returns the code or null. */
export function glyphPicker(scr: Screen, input: InputFn, alive: AliveFn, current: number): number | null {
  var COLS = 32;
  var ROWS = 7; // 224 glyphs
  var w = COLS + 4;
  var h = ROWS + 6;
  var x = center(w, scr.cols);
  var y = center(h, scr.rows);
  var sel = current >= 32 ? current - 32 : 0xdb - 32;
  var hits = new HitMap();

  while (alive()) {
    hits.clear();
    fillBox(scr, x, y, w, h, theme.modalBody);
    drawFrame(scr, x, y, w, h, theme.modalFrame, true, 'Choose a character', theme.modalTitle);
    for (var g = 0; g < COLS * ROWS; g++) {
      var gx = x + 2 + (g % COLS);
      var gy = y + 2 + Math.floor(g / COLS);
      var attr = g === sel ? theme.modalSel : theme.modalBody;
      scr.put(gx, gy, g + 32, attr);
    }
    hits.add('grid', x + 2, y + 2, x + 1 + COLS, y + 1 + ROWS);
    var info = 'Code ' + (sel + 32) + '   Arrows move · Enter picks · Esc cancels';
    scr.putStr(x + 2, y + h - 3, info.substring(0, w - 4), theme.modalBody);
    var bx = x + 2;
    var by = y + h - 2;
    bx += drawButton(scr, hits, bx, by, 'ok', 'Enter', 'Pick') + 1;
    drawButton(scr, hits, bx, by, 'cancel', 'Esc', 'Cancel');
    scr.cursorVisible = false;
    scr.flush();

    var ev = input(30000);
    if (ev.type === 'mouse') {
      if (ev.press && ev.button === 0) {
        var id = hits.test(ev.x - 1, ev.y - 1);
        if (id === 'grid') {
          var cx = ev.x - 1 - (x + 2);
          var cy = ev.y - 1 - (y + 2);
          sel = cy * COLS + cx;
          return sel + 32;
        }
        if (id === 'ok') return sel + 32;
        if (id === 'cancel') return null;
      }
      continue;
    }
    if (ev.type !== 'key') continue;
    var k = ev.key;
    if (k === keys.KEY_ESC) return null;
    if (k === keys.KEY_ENTER) return sel + 32;
    if (k === keys.KEY_LEFT) sel = (sel + COLS * ROWS - 1) % (COLS * ROWS);
    else if (k === keys.KEY_RIGHT) sel = (sel + 1) % (COLS * ROWS);
    else if (k === keys.KEY_UP) sel = (sel + COLS * ROWS - COLS) % (COLS * ROWS);
    else if (k === keys.KEY_DOWN) sel = (sel + COLS) % (COLS * ROWS);
    else if (keys.isPrintable(k)) return k.charCodeAt(0);
  }
  return null;
}

/**
 * Foreground/background/blink picker. Returns the new attribute or null.
 */
export function colorPicker(scr: Screen, input: InputFn, alive: AliveFn, currentAttr: number): number | null {
  var w = 44;
  var h = 12;
  var x = center(w, scr.cols);
  var y = center(h, scr.rows);
  var fg = (currentAttr & 0x0f);
  var bg = (currentAttr >> 4) & 0x07;
  var blink = (currentAttr & BLINK) !== 0;
  var hits = new HitMap();
  var NAMES = ['Blk', 'Blu', 'Grn', 'Cyn', 'Red', 'Mag', 'Brn', 'Gry'];

  while (alive()) {
    hits.clear();
    fillBox(scr, x, y, w, h, theme.modalBody);
    drawFrame(scr, x, y, w, h, theme.modalFrame, true, 'Colors', theme.modalTitle);

    scr.putStr(x + 2, y + 2, 'Text color (\x1b\x1a keys):', theme.modalBody);
    for (var f = 0; f < 16; f++) {
      var fx = x + 2 + (f % 8) * 2;
      var fy = y + 3 + Math.floor(f / 8);
      var swatch = makeAttr(f & 7, 0, f >= 8);
      scr.put(fx, fy, f === fg ? 0xdb : 0xfe, swatch);
      scr.put(fx + 1, fy, 0x20, theme.modalBody);
      hits.add('fg' + f, fx, fy, fx, fy);
    }
    scr.putStr(x + 22, y + 2, 'Background (\x18\x19 keys):', theme.modalBody);
    for (var g2 = 0; g2 < 8; g2++) {
      var gx = x + 22 + g2 * 2;
      var isB = g2 === bg;
      // the swatch IS the color; the selected one gets a contrast marker
      scr.put(gx, y + 3, isB ? 0xfe : 0x20, makeAttr(g2 === 7 ? 0 : 7, g2, isB && g2 !== 7));
      hits.add('bg' + g2, gx, y + 3, gx, y + 3);
    }

    scr.putStr(x + 2, y + 6, '[' + (blink ? 'X' : ' ') + '] Blink  (K toggles)', theme.modalBody);
    hits.add('blink', x + 2, y + 6, x + 22, y + 6);

    var preview = makeAttr(fg & 7, bg, fg >= 8, blink);
    scr.putStr(x + 2, y + 8, 'Sample: ', theme.modalBody);
    scr.putStr(x + 10, y + 8, ' Sample text \xb0\xb1\xb2\xdb ', preview);

    var bx = x + 2;
    var by = y + h - 2;
    bx += drawButton(scr, hits, bx, by, 'ok', 'Enter', 'Use this') + 1;
    drawButton(scr, hits, bx, by, 'cancel', 'Esc', 'Cancel');
    scr.cursorVisible = false;
    scr.flush();

    var ev = input(30000);
    if (ev.type === 'mouse') {
      if (ev.press && ev.button === 0) {
        var id = hits.test(ev.x - 1, ev.y - 1);
        if (id === null) continue;
        if (id === 'ok') return makeAttr(fg & 7, bg, fg >= 8, blink);
        if (id === 'cancel') return null;
        if (id === 'blink') blink = !blink;
        else if (id.substring(0, 2) === 'fg') fg = parseInt(id.substring(2), 10);
        else if (id.substring(0, 2) === 'bg') bg = parseInt(id.substring(2), 10);
      }
      continue;
    }
    if (ev.type !== 'key') continue;
    var k = ev.key;
    if (k === keys.KEY_ESC) return null;
    if (k === keys.KEY_ENTER) return makeAttr(fg & 7, bg, fg >= 8, blink);
    if (k === keys.KEY_LEFT) fg = (fg + 15) % 16;
    else if (k === keys.KEY_RIGHT) fg = (fg + 1) % 16;
    else if (k === keys.KEY_UP) bg = (bg + 7) % 8;
    else if (k === keys.KEY_DOWN) bg = (bg + 1) % 8;
    else if (k.toUpperCase() === 'K') blink = !blink;
  }
  return null;
}

/**
 * Quote picker: checkbox list of the original message's lines.
 * Returns the selected line texts (unprefixed) or null.
 */
export function quotePicker(scr: Screen, input: InputFn, alive: AliveFn, quoteLines: string[]): string[] | null {
  var w = scr.cols - 4;
  var h = scr.rows - 4;
  if (h < 8) h = scr.rows;
  var x = center(w, scr.cols);
  var y = center(h, scr.rows);
  var listH = h - 4;
  var top = 0;
  var cur = 0;
  var selected: boolean[] = [];
  for (var i = 0; i < quoteLines.length; i++) selected.push(false);
  var hits = new HitMap();

  while (alive()) {
    hits.clear();
    if (cur < top) top = cur;
    if (cur >= top + listH) top = cur - listH + 1;
    fillBox(scr, x, y, w, h, theme.modalBody);
    drawFrame(scr, x, y, w, h, theme.modalFrame, true, 'Quote original message', theme.modalTitle);
    for (var li = 0; li < listH; li++) {
      var idx = top + li;
      if (idx >= quoteLines.length) break;
      var ly = y + 1 + li;
      var attr = idx === cur ? theme.quoteSel : theme.quote;
      var mark = selected[idx] ? '[x] ' : '[ ] ';
      var text = (mark + (quoteLines[idx] as string));
      if (text.length > w - 2) text = text.substring(0, w - 2);
      scr.fill(x + 1, ly, w - 2, 1, 0x20, attr);
      scr.putStr(x + 1, ly, text, attr);
      hits.add('line' + idx, x + 1, ly, x + w - 2, ly);
    }
    var bx = x + 2;
    var by = y + h - 2;
    bx += drawButton(scr, hits, bx, by, 'toggle', 'Space', 'Mark') + 1;
    bx += drawButton(scr, hits, bx, by, 'all', 'A', 'All') + 1;
    bx += drawButton(scr, hits, bx, by, 'insert', 'Enter', 'Insert') + 1;
    drawButton(scr, hits, bx, by, 'cancel', 'Esc', 'Cancel');
    scr.cursorVisible = false;
    scr.flush();

    var ev = input(30000);
    if (ev.type === 'mouse') {
      if (ev.wheel !== 0) {
        cur = clampIdx(cur + ev.wheel * 3, quoteLines.length);
        continue;
      }
      if (ev.press && ev.button === 0) {
        var id = hits.test(ev.x - 1, ev.y - 1);
        if (id === null) continue;
        if (id === 'cancel') return null;
        if (id === 'all') {
          for (var a = 0; a < selected.length; a++) selected[a] = true;
        } else if (id === 'insert') return collectSelection(quoteLines, selected, cur);
        else if (id === 'toggle') selected[cur] = !selected[cur];
        else if (id.substring(0, 4) === 'line') {
          cur = parseInt(id.substring(4), 10);
          selected[cur] = !selected[cur];
        }
      }
      continue;
    }
    if (ev.type !== 'key') continue;
    var k = ev.key;
    if (k === keys.KEY_ESC) return null;
    if (k === keys.KEY_ENTER) return collectSelection(quoteLines, selected, cur);
    if (k === ' ') {
      selected[cur] = !selected[cur];
      cur = clampIdx(cur + 1, quoteLines.length);
    } else if (k === keys.KEY_UP) cur = clampIdx(cur - 1, quoteLines.length);
    else if (k === keys.KEY_DOWN) cur = clampIdx(cur + 1, quoteLines.length);
    else if (k === keys.KEY_PAGEUP) cur = clampIdx(cur - listH, quoteLines.length);
    else if (k === keys.KEY_PAGEDN) cur = clampIdx(cur + listH, quoteLines.length);
    else if (k === keys.KEY_HOME) cur = 0;
    else if (k === keys.KEY_END) cur = quoteLines.length - 1;
    else if (k.toUpperCase() === 'A') {
      for (var a2 = 0; a2 < selected.length; a2++) selected[a2] = true;
    } else if (k.toUpperCase() === 'N') {
      for (var n2 = 0; n2 < selected.length; n2++) selected[n2] = false;
    }
  }
  return null;
}

function clampIdx(v: number, n: number): number {
  if (v < 0) return 0;
  if (v >= n) return n - 1;
  return v;
}

function collectSelection(lines: string[], selected: boolean[], cur: number): string[] {
  var out: string[] = [];
  for (var i = 0; i < lines.length; i++) {
    if (selected[i]) out.push(lines[i] as string);
  }
  if (out.length === 0 && lines.length > 0) out.push(lines[cur] as string);
  return out;
}

/** Single-line text prompt (subject editing). Returns text or null. */
export function promptLine(
  scr: Screen,
  input: InputFn,
  alive: AliveFn,
  title: string,
  initial: string,
  maxLen: number
): string | null {
  var w = Math.min(scr.cols - 4, Math.max(40, maxLen + 6));
  var h = 7;
  var x = center(w, scr.cols);
  var y = center(h, scr.rows);
  var text = initial;
  var col = text.length;
  var fieldW = w - 6;
  var hits = new HitMap();

  while (alive()) {
    hits.clear();
    fillBox(scr, x, y, w, h, theme.modalBody);
    drawFrame(scr, x, y, w, h, theme.modalFrame, true, title, theme.modalTitle);
    var fieldAttr = makeAttr(7, 0, true);
    scr.fill(x + 3, y + 2, fieldW, 1, 0x20, fieldAttr);
    var scroll = col > fieldW - 1 ? col - (fieldW - 1) : 0;
    scr.putStr(x + 3, y + 2, text.substring(scroll, scroll + fieldW), fieldAttr);
    var bx = x + 3;
    var by = y + h - 2;
    bx += drawButton(scr, hits, bx, by, 'ok', 'Enter', 'OK') + 1;
    drawButton(scr, hits, bx, by, 'cancel', 'Esc', 'Cancel');
    scr.cursorX = x + 4 + (col - scroll);
    scr.cursorY = y + 3;
    scr.cursorVisible = true;
    scr.flush();

    var ev = input(30000);
    if (ev.type === 'mouse') {
      if (ev.press && ev.button === 0) {
        var id = hits.test(ev.x - 1, ev.y - 1);
        if (id === 'ok') return text;
        if (id === 'cancel') return null;
      }
      continue;
    }
    if (ev.type !== 'key') continue;
    var k = ev.key;
    if (k === keys.KEY_ESC) return null;
    if (k === keys.KEY_ENTER) return text;
    if (k === keys.KEY_LEFT && col > 0) col--;
    else if (k === keys.KEY_RIGHT && col < text.length) col++;
    else if (k === keys.KEY_HOME) col = 0;
    else if (k === keys.KEY_END) col = text.length;
    else if (k === keys.KEY_BACKSPACE && col > 0) {
      text = text.substring(0, col - 1) + text.substring(col);
      col--;
    } else if (k === keys.KEY_DEL && col < text.length) {
      text = text.substring(0, col) + text.substring(col + 1);
    } else if (keys.isPrintable(k) && text.length < maxLen) {
      text = text.substring(0, col) + k + text.substring(col);
      col++;
    }
  }
  return null;
}

/** Mode-aware help overlay; any key or click closes. */
export function helpOverlay(scr: Screen, input: InputFn, alive: AliveFn, mode: string): void {
  var lines: string[];
  if (mode === 'draw') {
    lines = [
      'DRAW MODE - paint CP437 art that stays fixed in place',
      '',
      'Tab cycles tools: Pencil, Type, Select, Line, Box,',
      '        Circle, Fill, Recolor.  Shift+Tab opens the',
      '        current tool\'s options (size, style, fill...).',
      'Pencil: left paints, right erases, middle picks up a cell.',
      'Type:   click anywhere, then type text freely — it lands as',
      '        fixed art (no wrap). Enter returns to your start',
      '        column; Backspace erases. Text art placed anywhere.',
      'Line/Box/Circle: drag start-to-end, or Space to set start,',
      '        move, Space to commit. Fill: left/Space fills.',
      'Recolor: drag over glyphs to repaint their color, keeping',
      '        the character. 1=FG only, 2=BG only, 3=both.',
      'F1-F10: type the characters previewed on the bar above',
      '        the status line; F11/F12 or \x11 \x10 cycle its sets.',
      '^L  colors    ^K  character    ^W  eyedrop   ^T  text mode',
      '',
      'Art cells never move when message text is edited or',
      'rewrapped. Draw a box, switch to text mode, and click',
      'inside it to type constrained within its walls.'
    ];
  } else {
    lines = [
      'TEXT MODE - write your message like a normal editor',
      '',
      'Type to insert text; Enter starts a new paragraph.',
      'Arrows, Home/End, PgUp/PgDn move. Ins toggles overwrite.',
      'Click anywhere to place the cursor; wheel scrolls.',
      'Click inside a drawn box to type constrained within it;',
      'the menu\'s "Leave text box" returns to full width.',
      '',
      '^O save & post    ^R quote reply    ^D draw mode',
      '^L text colors    ^Z undo   ^Y redo   ^G this help',
      '^A abort          Esc opens the full menu.',
      'F1-F10 insert the art characters previewed on the bar',
      'above; F11/F12 or the \x11 \x10 arrows cycle its sets.',
      '',
      '``` on its own line opens a code block (```js tags the',
      'language, bare ``` auto-detects); another ``` closes it.',
      'Arrows walk out of boxes through their top/bottom edge.',
      '',
      'The right-edge marker shows the 79-column safe width',
      'for posting; long paragraphs wrap automatically.'
    ];
  }
  var w = 0;
  for (var i = 0; i < lines.length; i++) if ((lines[i] as string).length > w) w = (lines[i] as string).length;
  w += 6;
  var h = lines.length + 4;
  var x = center(w, scr.cols);
  var y = center(h, scr.rows);
  fillBox(scr, x, y, w, h, theme.modalBody);
  drawFrame(scr, x, y, w, h, theme.modalFrame, true, 'Help  (^G)', theme.modalTitle);
  for (var li = 0; li < lines.length; li++) scr.putStr(x + 3, y + 2 + li, lines[li] as string, theme.modalBody);
  scr.putStr(x + 3, y + h - 2, 'Press any key or click to continue...', theme.modalTitle);
  scr.cursorVisible = false;
  scr.flush();
  while (alive()) {
    var ev = input(30000);
    if (ev.type === 'key') return;
    if (ev.type === 'mouse' && ev.press) return;
  }
}
