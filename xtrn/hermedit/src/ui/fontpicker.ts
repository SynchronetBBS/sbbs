/**
 * TheDraw font picker: type the text, filter the font list by size (height)
 * and name, and see a LIVE preview of the highlighted font rendering that
 * text — the preview surfaces the rendered W x H, so a font that blows past
 * the safe width is obvious before it is placed. Returns the chosen font name
 * and the text, or null on cancel.
 */

import { Screen } from './screen';
import { HitMap, drawButton, drawFrame, fillBox } from './widgets';
import { theme } from './theme';
import { makeAttr } from '../core/attr';
import { InputFn, AliveFn } from './modals';
import { FontMeta, FontProvider } from '../host/types';
import { renderTdf, COLOR_FONT, TdfFont } from '../core/tdf';
import * as keys from './keys';

export interface FontChoice {
  fontName: string;
  text: string;
}

/** Height buckets for the size filter. */
var SIZE_FILTERS: { label: string; test: (h: number) => boolean }[] = [
  { label: 'All', test: function () { return true; } },
  { label: 'Small', test: function (h) { return h <= 5; } },
  { label: 'Medium', test: function (h) { return h >= 6 && h <= 8; } },
  { label: 'Large', test: function (h) { return h >= 9; } }
];

function applyFilters(all: FontMeta[], sizeIdx: number, nameFilter: string): FontMeta[] {
  var sf = SIZE_FILTERS[sizeIdx] as { label: string; test: (h: number) => boolean };
  var needle = nameFilter.toLowerCase();
  var out: FontMeta[] = [];
  for (var i = 0; i < all.length; i++) {
    var f = all[i] as FontMeta;
    if (!sf.test(f.height)) continue;
    if (needle.length > 0 && f.name.toLowerCase().indexOf(needle) === -1) continue;
    out.push(f);
  }
  return out;
}

export function fontPicker(
  scr: Screen,
  input: InputFn,
  alive: AliveFn,
  provider: FontProvider,
  initialText: string
): FontChoice | null {
  var all = provider.list();
  if (all.length === 0) return null;

  var w = Math.min(scr.cols - 2, 76);
  var h = Math.min(scr.rows - 2, 22);
  var x = Math.floor((scr.cols - w) / 2);
  var y = Math.floor((scr.rows - h) / 2);

  var text = initialText.length > 0 ? initialText : 'Hello';
  var sizeIdx = 0;
  var nameFilter = '';
  var filtered = applyFilters(all, sizeIdx, nameFilter);
  var sel = 0;
  var top = 0;
  var focus = 0; // 0=text field, 1=list (Tab switches)

  var listX = x + 2;
  var listY = y + 5;
  var listW = 22;
  var listH = h - 8;
  var previewX = listX + listW + 2;
  var previewW = x + w - 2 - previewX;

  // preview render cache, keyed by fontName + '|' + text
  var cacheKey = '';
  var cached: { font: TdfFont | null; render: ReturnType<typeof renderTdf> | null } = { font: null, render: null };

  function refilter(): void {
    filtered = applyFilters(all, sizeIdx, nameFilter);
    if (sel >= filtered.length) sel = filtered.length - 1;
    if (sel < 0) sel = 0;
    top = 0;
  }

  function currentFont(): FontMeta | null {
    return sel >= 0 && sel < filtered.length ? (filtered[sel] as FontMeta) : null;
  }

  function ensurePreview(): void {
    var fm = currentFont();
    if (fm === null) { cached = { font: null, render: null }; cacheKey = ''; return; }
    var key = fm.name + '|' + text;
    if (key === cacheKey) return;
    cacheKey = key;
    var font = provider.load(fm.name);
    cached = { font: font, render: font === null ? null : renderTdf(font, text.length > 0 ? text : ' ') };
  }

  while (alive()) {
    if (sel < top) top = sel;
    if (sel >= top + listH) top = sel - listH + 1;
    ensurePreview();
    var hits = new HitMap();

    fillBox(scr, x, y, w, h, theme.modalBody);
    drawFrame(scr, x, y, w, h, theme.modalFrame, true, 'TheDraw font', theme.modalTitle);

    // text field
    scr.putStr(x + 2, y + 2, 'Text:', theme.modalBody);
    var fieldAttr = makeAttr(7, 0, true);
    var fieldW = w - 10;
    scr.fill(x + 8, y + 2, fieldW, 1, 0x20, focus === 0 ? fieldAttr : theme.modalBody);
    scr.putStr(x + 8, y + 2, text.substring(text.length > fieldW ? text.length - fieldW : 0), focus === 0 ? fieldAttr : theme.modalBody);
    hits.add('field', x + 8, y + 2, x + 8 + fieldW - 1, y + 2);

    // size + name filters
    var fx = x + 2;
    scr.putStr(fx, y + 3, 'Size:', theme.modalBody);
    fx += 6;
    for (var si = 0; si < SIZE_FILTERS.length; si++) {
      var sfa = si === sizeIdx ? theme.modalSel : theme.modalBody;
      var lbl = ' ' + (SIZE_FILTERS[si] as { label: string }).label + ' ';
      scr.putStr(fx, y + 3, lbl, sfa);
      hits.add('size' + si, fx, y + 3, fx + lbl.length - 1, y + 3);
      fx += lbl.length + 1;
    }
    scr.putStr(fx + 1, y + 3, 'Name: ' + (nameFilter.length ? nameFilter : '(any)'), theme.modalBody);
    hits.add('namefilter', fx + 1, y + 3, x + w - 3, y + 3);

    scr.putStr(x + 2, y + 4, filtered.length + ' fonts  (Tab field/list \x1b\x1a size)', theme.modalTitle);

    // list
    fillBox(scr, listX, listY, listW, listH, theme.modalBody);
    for (var li = 0; li < listH; li++) {
      var idx = top + li;
      if (idx >= filtered.length) break;
      var fm2 = filtered[idx] as FontMeta;
      var rowAttr = idx === sel ? theme.modalSel : theme.modalBody;
      var rowTxt = fm2.name;
      if (rowTxt.length > listW - 5) rowTxt = rowTxt.substring(0, listW - 5);
      scr.fill(listX, listY + li, listW, 1, 0x20, rowAttr);
      scr.putStr(listX, listY + li, rowTxt, rowAttr);
      scr.putStr(listX + listW - 3, listY + li, ('' + fm2.height), idx === sel ? theme.modalSel : theme.modalTitle);
      hits.add('font' + idx, listX, listY + li, listX + listW - 1, listY + li);
    }

    // preview pane
    drawFrame(scr, previewX - 1, listY - 1, previewW + 2, listH + 2, theme.divider, false);
    var pv = cached.render;
    var fmc = currentFont();
    if (fmc !== null) {
      var dims = pv ? (pv.width + 'x' + pv.height) : '?';
      scr.putStr(previewX, listY - 1, ' ' + fmc.name + ' (' + fmc.height + ') ' + dims + ' ', theme.modalTitle);
    }
    if (pv !== null) {
      var isColor = cached.font !== null && cached.font.fonttype === COLOR_FONT;
      for (var ry = 0; ry < listH && ry < pv.rows.length; ry++) {
        var prow = pv.rows[ry] as { ch: number; color: number }[];
        for (var rx = 0; rx < previewW && rx < prow.length; rx++) {
          var pc = prow[rx] as { ch: number; color: number };
          var pAttr = isColor ? (pc.color & 0xff) : makeAttr(7, 0, true);
          scr.put(previewX + rx, listY + ry, pc.ch, pc.ch === 0x20 && !(isColor && (pc.color & 0x70)) ? theme.modalBody : pAttr);
        }
      }
      if (pv.width > previewW) scr.putStr(previewX, listY + listH - 1, '\x1a wider than pane \x1a', theme.modalTitle);
    } else if (fmc !== null) {
      scr.putStr(previewX, listY + 1, '(font failed to load)', theme.modalBody);
    }

    // buttons
    var by = y + h - 2;
    var bx = x + 2;
    bx += drawButton(scr, hits, bx, by, 'ok', 'Enter', 'Use font') + 1;
    drawButton(scr, hits, bx, by, 'cancel', 'Esc', 'Cancel');

    scr.cursorVisible = focus === 0;
    scr.cursorX = x + 8 + Math.min(text.length, fieldW);
    scr.cursorY = y + 2;
    scr.flush();

    var ev = input(30000);
    if (ev.type === 'mouse') {
      if (ev.wheel !== 0) { sel = clampSel(sel + ev.wheel * 3, filtered.length); continue; }
      if (ev.press && ev.button === 0) {
        var id = hits.test(ev.x - 1, ev.y - 1);
        if (id === null) continue;
        if (id === 'ok') return commit(currentFont(), text);
        if (id === 'cancel') return null;
        if (id === 'field') { focus = 0; continue; }
        if (id === 'namefilter') { focus = 2; continue; }
        if (id.substring(0, 4) === 'size') { sizeIdx = parseInt(id.substring(4), 10); refilter(); continue; }
        if (id.substring(0, 4) === 'font') { sel = parseInt(id.substring(4), 10); focus = 1; continue; }
      }
      continue;
    }
    if (ev.type !== 'key') continue;
    var k = ev.key;
    if (k === keys.KEY_ESC) return null;
    if (k === keys.KEY_ENTER) return commit(currentFont(), text);
    if (k === keys.KEY_TAB) { focus = focus === 1 ? 0 : 1; continue; }
    if (k === keys.KEY_UP) { sel = clampSel(sel - 1, filtered.length); focus = 1; continue; }
    if (k === keys.KEY_DOWN) { sel = clampSel(sel + 1, filtered.length); focus = 1; continue; }
    if (k === keys.KEY_PAGEUP) { sel = clampSel(sel - listH, filtered.length); continue; }
    if (k === keys.KEY_PAGEDN) { sel = clampSel(sel + listH, filtered.length); continue; }
    // Left/Right cycle the size filter (arrows aren't used for the fields).
    if (k === keys.KEY_LEFT) { sizeIdx = (sizeIdx + SIZE_FILTERS.length - 1) % SIZE_FILTERS.length; refilter(); continue; }
    if (k === keys.KEY_RIGHT) { sizeIdx = (sizeIdx + 1) % SIZE_FILTERS.length; refilter(); continue; }
    // Backspace and Delete both erase the last char (terminals disagree on
    // which byte the Backspace key sends — often 0x7f == KEY_DEL).
    var erase = k === keys.KEY_BACKSPACE || k === keys.KEY_DEL;
    if (focus === 2) {
      if (erase) nameFilter = nameFilter.substring(0, nameFilter.length - 1);
      else if (keys.isPrintable(k)) nameFilter += k;
      refilter();
    } else {
      if (erase) text = text.substring(0, text.length - 1);
      else if (keys.isPrintable(k)) text += k;
      focus = 0;
    }
  }
  return null;
}

function commit(fm: FontMeta | null, text: string): FontChoice | null {
  if (fm === null || text.length === 0) return null;
  return { fontName: fm.name, text: text };
}

function clampSel(v: number, n: number): number {
  if (n <= 0) return 0;
  if (v < 0) return 0;
  if (v >= n) return n - 1;
  return v;
}
