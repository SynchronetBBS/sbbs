/**
 * GUI-style widgets on the cell buffer: labeled buttons that always show
 * their key equivalent, CP437 boxes for modals/panels, and a coordinate
 * hit-region registry (arithmetic hit-testing — never per-cell hotspots).
 */

import { Screen } from './screen';
import { theme } from './theme';

export interface HitRegion {
  id: string;
  /** 0-based inclusive cell rectangle. */
  x1: number;
  y1: number;
  x2: number;
  y2: number;
}

export class HitMap {
  private regions: HitRegion[] = [];

  clear(): void {
    this.regions = [];
  }

  add(id: string, x1: number, y1: number, x2: number, y2: number): void {
    this.regions.push({ id: id, x1: x1, y1: y1, x2: x2, y2: y2 });
  }

  /** 0-based coordinates; latest matching region wins (modals overlay). */
  test(x: number, y: number): string | null {
    for (var i = this.regions.length - 1; i >= 0; i--) {
      var r = this.regions[i] as HitRegion;
      if (x >= r.x1 && x <= r.x2 && y >= r.y1 && y <= r.y2) return r.id;
    }
    return null;
  }
}

/**
 * Draw a clickable button like `[F2 Save]` — key hint highlighted, label
 * plain — and register its hit region. Returns the width consumed.
 */
export function drawButton(
  scr: Screen,
  hits: HitMap,
  x: number,
  y: number,
  id: string,
  keyLabel: string,
  label: string,
  active?: boolean
): number {
  var body = active ? theme.buttonActive : theme.button;
  var key = active ? theme.buttonActiveKey : theme.buttonKey;
  var text = '[' + keyLabel + ' ' + label + ']';
  scr.put(x, y, '['.charCodeAt(0), body);
  scr.putStr(x + 1, y, keyLabel, key);
  scr.putStr(x + 1 + keyLabel.length, y, ' ' + label, body);
  scr.put(x + text.length - 1, y, ']'.charCodeAt(0), body);
  hits.add(id, x, y, x + text.length - 1, y);
  return text.length;
}

/** CP437 single-line box glyphs. */
export var BOX = {
  tl: 0xda, tr: 0xbf, bl: 0xc0, br: 0xd9,
  h: 0xc4, v: 0xb3,
  teeDown: 0xc2, teeUp: 0xc1, teeRight: 0xc3, teeLeft: 0xb4
};

/** Double-line variants for modal frames. */
export var DBOX = {
  tl: 0xc9, tr: 0xbb, bl: 0xc8, br: 0xbc,
  h: 0xcd, v: 0xba
};

export function drawFrame(
  scr: Screen,
  x: number,
  y: number,
  w: number,
  h: number,
  attr: number,
  dbl?: boolean,
  title?: string,
  titleAttr?: number
): void {
  var g = dbl ? DBOX : BOX;
  scr.put(x, y, g.tl, attr);
  scr.put(x + w - 1, y, g.tr, attr);
  scr.put(x, y + h - 1, g.bl, attr);
  scr.put(x + w - 1, y + h - 1, g.br, attr);
  scr.hline(x + 1, y, w - 2, g.h, attr);
  scr.hline(x + 1, y + h - 1, w - 2, g.h, attr);
  for (var yy = y + 1; yy < y + h - 1; yy++) {
    scr.put(x, yy, g.v, attr);
    scr.put(x + w - 1, yy, g.v, attr);
  }
  if (title !== undefined && title.length > 0) {
    var t = ' ' + title + ' ';
    if (t.length > w - 4) t = t.substring(0, w - 4);
    var tx = x + Math.floor((w - t.length) / 2);
    scr.putStr(tx, y, t, titleAttr === undefined ? attr : titleAttr);
  }
}

/** Fill the interior of a frame. */
export function fillBox(scr: Screen, x: number, y: number, w: number, h: number, attr: number): void {
  scr.fill(x, y, w, h, 0x20, attr);
}
